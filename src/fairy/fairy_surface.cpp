#include "fairy_surface.hpp"
#include "fairy_pipeline.hpp"
#include "../gpu/gpu_context.hpp"
#include "../gpu/gpu_texture.hpp"

namespace fairy
{

FairySurface::FairySurface(uint32_t width, uint32_t height, vk::Format format, FairySurfaceUsage usage,
                           uint32_t buffer_count)
    : width_(width), height_(height), format_(format), usage_(usage), buffer_count_(buffer_count)
{
    CreateRenderPass();
    CreateRenderTarget();
    CreateFramebuffer();
    CreateSubmitResource();
}

FairySurface::~FairySurface()
{
    gpu::GpuContext& gpu_context = gpu::GpuContext::Get();
    gpu_context.device.freeCommandBuffers(render_command_pool_, render_command_buffers_);
    gpu_context.device.destroyCommandPool(render_command_pool_);
    for (int i = 0; i < buffer_count_; ++i)
    {
        gpu_context.device.destroyFence(render_fences_[i]);
        gpu_context.device.destroyFramebuffer(framebuffers_[i]);
    }
    gpu_context.device.destroyRenderPass(render_pass_);
}

void FairySurface::AddWaitSemaphore(vk::Semaphore semaphore, vk::PipelineStageFlags stage)
{
    if (semaphore)
    {
        need_wait_semaphores_.push_back(semaphore);
        need_wait_stages_.push_back(stage);
    }
}

void FairySurface::AddSignalSemaphore(vk::Semaphore semaphore)
{
    if (semaphore)
    {
        need_signal_semaphores_.push_back(semaphore);
    }
}

void FairySurface::Render(const FairyPipeline* fairy_pipeline, int index)
{
    vk::CommandBuffer render_command_buffer = render_command_buffers_[index];
    render_command_buffer.reset();

    vk::CommandBufferBeginInfo begin_info = {};
    begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    render_command_buffer.begin(begin_info);

    vk::ClearValue clear_value = {};
    vk::RenderPassBeginInfo render_pass_begin_info = {};
    render_pass_begin_info.framebuffer = framebuffers_[index];
    render_pass_begin_info.renderPass = render_pass_;
    render_pass_begin_info.renderArea.offset = vk::Offset2D(0, 0);
    render_pass_begin_info.renderArea.extent = vk::Extent2D(width_, height_);
    render_pass_begin_info.clearValueCount = 1;
    render_pass_begin_info.pClearValues = &clear_value;
    render_command_buffer.beginRenderPass(render_pass_begin_info, vk::SubpassContents::eInline);
    render_command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, fairy_pipeline->Pipeline());

    vk::Viewport viewport = { 0.f, 0.f, static_cast<float>(width_), static_cast<float>(height_), 0.f, 1.f };
    vk::Rect2D scissor = { vk::Offset2D(0.f, 0.f), vk::Extent2D(width_, height_) };
    render_command_buffer.setViewport(0, viewport);
    render_command_buffer.setScissor(0, scissor);
    render_command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, fairy_pipeline->PipelineLayout(), 0,
                                             fairy_pipeline->DescriptorSets(), {});
    render_command_buffer.bindIndexBuffer(fairy_pipeline->IndexBuffer(), vk::DeviceSize(0), vk::IndexType::eUint16);
    render_command_buffer.drawIndexed(fairy_pipeline->IndexCount(), 1, 0, 0, 0);
    render_command_buffer.endRenderPass();
    render_command_buffer.end();

    vk::SubmitInfo submit_info = {};
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &render_command_buffer;
    if (!need_wait_semaphores_.empty())
    {
        submit_info.waitSemaphoreCount = need_wait_semaphores_.size();
        submit_info.pWaitSemaphores = need_wait_semaphores_.data();
        submit_info.pWaitDstStageMask = need_wait_stages_.data();
    }
    if (!need_signal_semaphores_.empty())
    {
        submit_info.signalSemaphoreCount = need_signal_semaphores_.size();
        submit_info.pSignalSemaphores = need_signal_semaphores_.data();
    }
    gpu::GpuContext::Get().device.resetFences(render_fences_[index]);
    render_queue_.submit(submit_info, render_fences_[index]);
    need_wait_semaphores_.clear();
    need_wait_stages_.clear();
    need_signal_semaphores_.clear();
}

void FairySurface::WaitRenderComplete()
{
    auto _ = gpu::GpuContext::Get().device.waitForFences(render_fences_, true, std::numeric_limits<uint64_t>::max());
}

void FairySurface::WaitRenderComplete(int index)
{
    auto _ =
        gpu::GpuContext::Get().device.waitForFences(render_fences_[index], true, std::numeric_limits<uint64_t>::max());
}

void FairySurface::CreateRenderPass()
{
    vk::ImageLayout final_layout;
    switch (usage_)
    {
    case FairySurfaceUsage::eSample:
        final_layout = vk::ImageLayout::eShaderReadOnlyOptimal;
        break;
    case FairySurfaceUsage::eCopy:
        final_layout = vk::ImageLayout::eTransferSrcOptimal;
        break;
    }

    vk::AttachmentDescription color_attachment = {};
    color_attachment.format = format_;
    color_attachment.samples = vk::SampleCountFlagBits::e1;
    color_attachment.loadOp = vk::AttachmentLoadOp::eClear;
    color_attachment.storeOp = vk::AttachmentStoreOp::eStore;
    color_attachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    color_attachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    color_attachment.initialLayout = vk::ImageLayout::eUndefined;
    color_attachment.finalLayout = final_layout;

    vk::AttachmentReference color_attachment_ref = {};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = vk::ImageLayout::eColorAttachmentOptimal;

    vk::SubpassDescription subpass = {};
    subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;

    vk::RenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &color_attachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;

    render_pass_ = gpu::GpuContext::Get().device.createRenderPass(renderPassInfo);
}

void FairySurface::CreateRenderTarget()
{
    vk::ImageUsageFlags image_usage = vk::ImageUsageFlagBits::eColorAttachment;
    switch (usage_)
    {
    case FairySurfaceUsage::eSample:
        image_usage |= vk::ImageUsageFlagBits::eSampled;
        break;
    case FairySurfaceUsage::eCopy:
        image_usage |= vk::ImageUsageFlagBits::eTransferSrc;
        break;
    }
    render_targets_.reserve(buffer_count_);
    for (int i = 0; i < buffer_count_; ++i)
    {
        render_targets_.emplace_back(std::unique_ptr<gpu::GpuTexture>(
            new gpu::GpuTexture(width_, height_, format_, image_usage, vk::MemoryPropertyFlagBits::eDeviceLocal)));
    }
}

void FairySurface::CreateFramebuffer()
{
    framebuffers_.reserve(buffer_count_);
    for (int i = 0; i < buffer_count_; ++i)
    {
        vk::ImageView render_target_view = render_targets_[i]->ImageView();
        vk::FramebufferCreateInfo frame_buffer_create_info = {};
        frame_buffer_create_info.renderPass = render_pass_;
        frame_buffer_create_info.attachmentCount = 1;
        frame_buffer_create_info.pAttachments = &render_target_view;
        frame_buffer_create_info.width = width_;
        frame_buffer_create_info.height = height_;
        frame_buffer_create_info.layers = 1;
        framebuffers_.emplace_back(gpu::GpuContext::Get().device.createFramebuffer(frame_buffer_create_info));
    }
}

void FairySurface::CreateSubmitResource()
{
    gpu::GpuContext& gpu_context = gpu::GpuContext::Get();
    render_queue_ = gpu_context.device.getQueue(gpu_context.queue_family_index, 0);

    vk::CommandPoolCreateInfo command_pool_create_info = {};
    command_pool_create_info.queueFamilyIndex = gpu_context.queue_family_index;
    command_pool_create_info.flags |= vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    command_pool_create_info.flags |= vk::CommandPoolCreateFlagBits::eTransient;
    render_command_pool_ = gpu_context.device.createCommandPool(command_pool_create_info);

    vk::CommandBufferAllocateInfo command_buffer_allocate_info = {};
    command_buffer_allocate_info.commandPool = render_command_pool_;
    command_buffer_allocate_info.level = vk::CommandBufferLevel::ePrimary;
    command_buffer_allocate_info.commandBufferCount = buffer_count_;
    render_command_buffers_ = gpu_context.device.allocateCommandBuffers(command_buffer_allocate_info);

    render_fences_.reserve(buffer_count_);
    for (int i = 0; i < buffer_count_; ++i)
    {
        vk::FenceCreateInfo fence_create_info = {};
        fence_create_info.flags = vk::FenceCreateFlagBits::eSignaled;
        render_fences_.emplace_back(gpu_context.device.createFence(fence_create_info));
    }
}

} // namespace fairy