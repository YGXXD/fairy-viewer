#include "fairy_render_thread.hpp"
#include "../render_core/gpu_buffer.hpp"
#include "../render_core/gpu_texture.hpp"
#include "../render_core/gpu_context.hpp"
#include "../render_fairy/fairy_surface.hpp"
#include "../render_fairy/fairy_pipeline.hpp"

#include <iostream>

namespace service
{

FairyRenderThread::FairyRenderThread(int fairy_surface_width, int fairy_surface_height, int fairy_buffer_count)
    : surface_width_(fairy_surface_width), surface_height_(fairy_surface_height),
      surface_format_(vk::Format::eR8G8B8A8Srgb), buffer_count_(fairy_buffer_count)
{
    is_run_.store(true);
    run_thread_ = std::thread(&FairyRenderThread::FairyRenderThreadMain, this);
    std::cout << "[FairyRenderThread]:" << this << ": Start Run Render Fairy" << std::endl;
}

FairyRenderThread::~FairyRenderThread()
{
    is_run_.store(false);
    if (run_thread_.joinable())
    {
        run_thread_.join();
    }
    std::cout << "[FairyRenderThread]:" << this << ": Stop Run Render Fairy" << std::endl;
}

std::unique_ptr<uint8_t[]> FairyRenderThread::RequestCopySurface()
{
    std::promise<std::pair<std::shared_ptr<fv::GpuBuffer>, vk::Fence>> promise;
    std::future<std::pair<std::shared_ptr<fv::GpuBuffer>, vk::Fence>> future = promise.get_future();
    {
        std::lock_guard lock(surface_copy_mutex_);
        surface_copy_promises_.emplace_back(std::move(promise));
    }
    future.wait();
    auto [gpu_buffer, fence] = future.get();
    auto _ = fv::GpuContext::Get().device.waitForFences(fence, true, std::numeric_limits<uint64_t>::max());
    if (gpu_buffer->HostPointer())
    {
        auto result = std::unique_ptr<uint8_t[]>(new uint8_t[gpu_buffer->Size()]);
        memcpy(result.get(), gpu_buffer->HostPointer(), gpu_buffer->Size());
        return std::move(result);
    }
    return nullptr;
}

void FairyRenderThread::FairyRenderThreadMain()
{
    InitAppSubmitContext();
    InitFairy();
    while (is_run_.load())
    {
        std::vector<std::promise<std::pair<std::shared_ptr<fv::GpuBuffer>, vk::Fence>>> surface_copy_promises {};
        {
            std::lock_guard lock(surface_copy_mutex_);
            if (!surface_copy_promises_.empty())
                std::swap(surface_copy_promises, surface_copy_promises_);
        }
        if (!surface_copy_promises.empty())
            fairy_surface_->AddSignalSemaphore(fairy_complete_signals_[current_render_index_]);
        RenderFairy(current_render_index_);
        if (!surface_copy_promises.empty())
        {
            size_t buffer_size = surface_width_ * surface_height_ * 4;
            std::shared_ptr<fv::GpuBuffer> buffer = std::unique_ptr<fv::GpuBuffer>(new fv::GpuBuffer(
                buffer_size, vk::BufferUsageFlagBits::eTransferDst,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent));
            vk::CommandBuffer gpu_command_buffer = gpu_command_buffers_[current_render_index_];
            gpu_command_buffer.reset();
            vk::CommandBufferBeginInfo begin_info = {};
            begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
            gpu_command_buffer.begin(begin_info);
            const fv::GpuTexture* render_target = fairy_surface_->RenderTarget(current_render_index_);
            vk::BufferImageCopy region = {};
            region.bufferOffset = 0;
            region.bufferRowLength = 0;
            region.bufferImageHeight = 0;
            region.imageSubresource = *render_target->MakeSubresourceLayers();
            region.imageOffset = vk::Offset3D { 0, 0, 0 };
            region.imageExtent =
                vk::Extent3D { static_cast<uint32_t>(surface_width_), static_cast<uint32_t>(surface_height_), 1 };
            gpu_command_buffer.copyImageToBuffer(render_target->Image(), vk::ImageLayout::eTransferSrcOptimal,
                                                 buffer->Buffer(), region);
            gpu_command_buffer.end();
            vk::SubmitInfo submit_info = {};
            vk::PipelineStageFlags gpu_wait_stage = vk::PipelineStageFlagBits::eTransfer;
            submit_info.commandBufferCount = 1;
            submit_info.pCommandBuffers = &gpu_command_buffer;
            submit_info.waitSemaphoreCount = 1;
            submit_info.pWaitSemaphores = &fairy_complete_signals_[current_render_index_];
            submit_info.pWaitDstStageMask = &gpu_wait_stage;
            fv::GpuContext::Get().device.resetFences(gpu_fences_[current_render_index_]);
            is_gpu_fence_reset_[current_render_index_] = true;
            gpu_queue_.submit(submit_info, gpu_fences_[current_render_index_]);
            for (auto& promise : surface_copy_promises)
                promise.set_value({ buffer, gpu_fences_[current_render_index_] });
        }
        current_render_index_ = (current_render_index_ + 1) % buffer_count_;
        if (!is_gpu_fence_reset_[current_render_index_])
            fairy_surface_->WaitRenderComplete(current_render_index_);
        else
        {
            auto _ = fv::GpuContext::Get().device.waitForFences(gpu_fences_[current_render_index_], true,
                                                                std::numeric_limits<uint64_t>::max());
            is_gpu_fence_reset_[current_render_index_] = false;
        }
    }
    for (int i = 0; i < buffer_count_ - 1; ++i)
    {
        current_render_index_ = (current_render_index_ + 1) % buffer_count_;
        if (!is_gpu_fence_reset_[current_render_index_])
            fairy_surface_->WaitRenderComplete(current_render_index_);
        else
        {
            auto _ = fv::GpuContext::Get().device.waitForFences(gpu_fences_[current_render_index_], true,
                                                                std::numeric_limits<uint64_t>::max());
            is_gpu_fence_reset_[current_render_index_] = false;
        }
    }
    DestoryFairy();
    DestoryAppSubmitContext();
}

void FairyRenderThread::InitAppSubmitContext()
{
    fv::GpuContext& gpu_context = fv::GpuContext::Get();
    gpu_queue_ = gpu_context.device.getQueue(gpu_context.queue_family_index, 0);
    vk::CommandPoolCreateInfo command_pool_create_info = {};
    command_pool_create_info.queueFamilyIndex = gpu_context.queue_family_index;
    command_pool_create_info.flags |= vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    command_pool_create_info.flags |= vk::CommandPoolCreateFlagBits::eTransient;
    gpu_command_pool_ = gpu_context.device.createCommandPool(command_pool_create_info);
    vk::CommandBufferAllocateInfo command_buffer_allocate_info = {};
    command_buffer_allocate_info.commandPool = gpu_command_pool_;
    command_buffer_allocate_info.level = vk::CommandBufferLevel::ePrimary;
    command_buffer_allocate_info.commandBufferCount = buffer_count_;
    gpu_command_buffers_ = gpu_context.device.allocateCommandBuffers(command_buffer_allocate_info);
    gpu_fences_.reserve(buffer_count_);
    for (int i = 0; i < buffer_count_; ++i)
    {
        vk::FenceCreateInfo fence_create_info = {};
        fence_create_info.flags = vk::FenceCreateFlagBits::eSignaled;
        gpu_fences_.emplace_back(gpu_context.device.createFence(fence_create_info));
    }
    is_gpu_fence_reset_.resize(buffer_count_, false);
}

void FairyRenderThread::DestoryAppSubmitContext()
{
    fv::GpuContext& gpu_context = fv::GpuContext::Get();
    gpu_context.device.freeCommandBuffers(gpu_command_pool_, gpu_command_buffers_);
    gpu_context.device.destroyCommandPool(gpu_command_pool_);
    for (int i = 0; i < buffer_count_; ++i)
        gpu_context.device.destroyFence(gpu_fences_[i]);
}

void FairyRenderThread::InitFairy()
{
    fairy_surface_ = std::unique_ptr<fv::FairySurface>(new fv::FairySurface(
        surface_width_, surface_height_, surface_format_, fv::FairySurfaceUsage::eCopy, buffer_count_));
    fairy_pipeline_ = std::unique_ptr<fv::FairyPipeline>(new fv::FairyPipeline());
    const std::string default_shader = R"(/*
    shadertoy.com input variables
    uniform vec3      iResolution;           // viewport resolution (in pixels)
    uniform float     iTime;                 // shader playback time (in seconds)
    uniform float     iTimeDelta;            // render time (in seconds)
    uniform float     iFrameRate;            // shader frame rate
    uniform int       iFrame;                // shader playback frame
    uniform float     iChannelTime[4];       // channel playback time (in seconds)
    uniform vec3      iChannelResolution[4]; // channel resolution (in pixels)
    uniform vec4      iMouse;                // mouse pixel coords. xy: current (if MLB down), zw: click
    uniform samplerXX iChannel0..3;          // input channel. XX = 2D/Cube
    uniform vec4      iDate;                 // (year, month, day, time in seconds)
*/

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    // Normalized pixel coordinates (from 0 to 1)
    vec2 uv = fragCoord/iResolution.xy;

    // Time varying pixel color
    vec3 col = 0.5 + 0.5*cos(iTime+uv.xyx+vec3(0,2,4));

    // Output to screen
    fragColor = vec4(col,1.0);
})";
    ResetPipeline(default_shader);
    fairy_complete_signals_.reserve(buffer_count_);
    for (int i = 0; i < buffer_count_; ++i)
        fairy_complete_signals_.emplace_back(fv::GpuContext::Get().device.createSemaphore({}));
}

void FairyRenderThread::DestoryFairy()
{
    fairy_surface_.reset();
    fairy_pipeline_.reset();
    for (auto& semaphore : fairy_complete_signals_)
        fv::GpuContext::Get().device.destroySemaphore(semaphore);
}

void FairyRenderThread::RenderFairy(int index)
{
    auto fairy_current_ticks = std::chrono::steady_clock::now();
    float fairy_current_time =
        std::chrono::duration_cast<std::chrono::milliseconds>(fairy_current_ticks - fairy_start_ticks_).count() /
        1000.f;
    i_time_delta_ = fairy_current_time - i_time_;
    i_time_ = fairy_current_time;
    i_frame_rate_ = 1.f / i_time_delta_;
    fairy_pipeline_->Update_iResolution(
        ktm::fvec3 { static_cast<float>(surface_width_), static_cast<float>(surface_height_), 1.f });
    fairy_pipeline_->Update_iTime(i_time_);
    fairy_pipeline_->Update_iTimeDelta(i_time_delta_);
    fairy_pipeline_->Update_iFrameRate(i_frame_rate_);
    fairy_pipeline_->Update_iFrame(i_frame_++);
    fairy_pipeline_->Update_iMouse(i_mouse_);
    fairy_pipeline_->Update_iDate(i_date_);
    fairy_surface_->Render(fairy_pipeline_.get(), index);
}

void FairyRenderThread::ResetPipeline(const std::string& codes)
{
    fairy_surface_->WaitRenderComplete();
    pipeline_reset_status_ = fairy_pipeline_->Reset(fairy_surface_->RenderPass(), codes);
    if (pipeline_reset_status_)
    {
        std::cout << "[FairyRenderThread]:" << this << ": Fairy Pipeline Reset Success" << std::endl;
    }
    else
    {
        std::cout << "[FairyRenderThread]:" << this << ": Fairy Pipeline Reset Error\n"
                  << fairy_pipeline_->ResetErrorMessage() << std::endl;
    }
    fairy_start_ticks_ = std::chrono::steady_clock::now();
    i_time_ = 0;
    i_time_delta_ = 0;
    i_frame_rate_ = 0;
    i_frame_ = 0;
    i_mouse_ = ktm::fvec4 { 0, 0, 0, 0 };
    i_date_ = ktm::fvec4 { 0, 0, 0, 0 };
}

} // namespace service
