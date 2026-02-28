#include "fairy_surface.hpp"
#include "fairy_pipeline.hpp"
#include "../render_core/gpu_context.hpp"
#include "../render_core/gpu_texture.hpp"

namespace fv
{

FairySurface::FairySurface(uint32_t width, uint32_t height, vk::Format format)
    : width_(width), height_(height), format_(format)
{
    CreateRenderTarget();
    CreateRenderPass();
    CreateFramebuffer();
    CreateSubmitResource();
}

FairySurface::~FairySurface()
{
    GpuContext& gpu_context = GpuContext::Get();
    gpu_context.device.freeCommandBuffers(gpu_context.command_pool, render_command_buffer_);
    gpu_context.device.destroySemaphore(render_signal_semaphore_);
    gpu_context.device.destroyFence(render_fence_);
    gpu_context.device.destroyFramebuffer(framebuffer_);
    gpu_context.device.destroyRenderPass(render_pass_);
}

void FairySurface::Render(const FairyPipeline* fairy_pipeline, vk::Semaphore& out_signal_semaphore)
{
    GpuContext& gpu_context = GpuContext::Get();
    render_command_buffer_.reset();

    vk::CommandBufferBeginInfo begin_info = {};
    begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    render_command_buffer_.begin(begin_info);
    vk::ClearValue clear_value = {};
    vk::RenderPassBeginInfo render_pass_begin_info = {};
    render_pass_begin_info.framebuffer = framebuffer_;
    render_pass_begin_info.renderPass = render_pass_;
    render_pass_begin_info.renderArea.offset = vk::Offset2D(0, 0);
    render_pass_begin_info.renderArea.extent = vk::Extent2D(width_, height_);
    render_pass_begin_info.clearValueCount = 1;
    render_pass_begin_info.pClearValues = &clear_value;
    render_command_buffer_.beginRenderPass(render_pass_begin_info, vk::SubpassContents::eInline);
    render_command_buffer_.bindPipeline(vk::PipelineBindPoint::eGraphics, fairy_pipeline->Pipeline());
    vk::Viewport viewport = { 0.f, 0.f, static_cast<float>(width_), static_cast<float>(height_), 0.f, 1.f };
    vk::Rect2D scissor = { vk::Offset2D(0.f, 0.f), vk::Extent2D(width_, height_) };
    render_command_buffer_.setViewport(0, viewport);
    render_command_buffer_.setScissor(0, scissor);
    render_command_buffer_.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, fairy_pipeline->PipelineLayout(), 0,
                                              fairy_pipeline->DescriptorSets(), {});
    render_command_buffer_.bindIndexBuffer(fairy_pipeline->IndexBuffer(), vk::DeviceSize(0), vk::IndexType::eUint16);
    render_command_buffer_.drawIndexed(fairy_pipeline->IndexCount(), 1, 0, 0, 0);
    render_command_buffer_.endRenderPass();
    render_command_buffer_.end();

    vk::SubmitInfo submit_info = {};
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &render_command_buffer_;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &render_signal_semaphore_;
    gpu_context.queue.submit(submit_info, render_fence_);
    need_wait_render_fence_ = true;
    out_signal_semaphore = render_signal_semaphore_;
}

void FairySurface::WaitGpuIfNeeded()
{
    if (need_wait_render_fence_)
    {
        GpuContext& gpu_context = GpuContext::Get();
        auto _ = gpu_context.device.waitForFences(render_fence_, true, std::numeric_limits<uint64_t>::max());
        gpu_context.device.resetFences(render_fence_);
        need_wait_render_fence_ = false;
    }
}

void FairySurface::CreateRenderTarget()
{
    render_target_ = std::unique_ptr<GpuTexture>(new GpuTexture(width_, height_, format_,
                                                                vk::ImageUsageFlagBits::eColorAttachment |
                                                                    vk::ImageUsageFlagBits::eTransferSrc |
                                                                    vk::ImageUsageFlagBits::eSampled,
                                                                vk::MemoryPropertyFlagBits::eDeviceLocal));
}

void FairySurface::CreateRenderPass()
{
    vk::AttachmentDescription color_attachment = {};
    color_attachment.format = format_;
    color_attachment.samples = vk::SampleCountFlagBits::e1;
    color_attachment.loadOp = vk::AttachmentLoadOp::eClear;
    color_attachment.storeOp = vk::AttachmentStoreOp::eStore;
    color_attachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    color_attachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    color_attachment.initialLayout = vk::ImageLayout::eUndefined;
    color_attachment.finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
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
    render_pass_ = GpuContext::Get().device.createRenderPass(renderPassInfo);
}

void FairySurface::CreateFramebuffer()
{
    vk::ImageView render_target_view = render_target_->ImageView();
    vk::FramebufferCreateInfo frame_buffer_create_info = {};
    frame_buffer_create_info.renderPass = render_pass_;
    frame_buffer_create_info.attachmentCount = 1;
    frame_buffer_create_info.pAttachments = &render_target_view;
    frame_buffer_create_info.width = width_;
    frame_buffer_create_info.height = height_;
    frame_buffer_create_info.layers = 1;
    framebuffer_ = GpuContext::Get().device.createFramebuffer(frame_buffer_create_info);
}

void FairySurface::CreateSubmitResource()
{
    GpuContext& gpu_context = GpuContext::Get();
    vk::CommandBufferAllocateInfo command_buffer_allocate_info = {};
    command_buffer_allocate_info.commandPool = gpu_context.command_pool;
    command_buffer_allocate_info.level = vk::CommandBufferLevel::ePrimary;
    command_buffer_allocate_info.commandBufferCount = 1;
    render_command_buffer_ = gpu_context.device.allocateCommandBuffers(command_buffer_allocate_info)[0];
    render_signal_semaphore_ = gpu_context.device.createSemaphore(vk::SemaphoreCreateInfo());
    render_fence_ = gpu_context.device.createFence(vk::FenceCreateInfo());
}

} // namespace fv