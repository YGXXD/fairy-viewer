#include "fairy_surface.hpp"
#include "fairy_pipeline.hpp"
#include "../render_core/gpu_context.hpp"
#include "../render_core/gpu_texture.hpp"
#include "../render_core/gpu_buffer.hpp"

namespace fv
{

FairySurface::FairySurface(uint32_t width, uint32_t height, vk::Format format)
    : width_(width), height_(height), format_(format)
{
    CreateRenderTarget();
    CreateRenderPass();
    CreateFramebuffer();
    CreateFence();
}

FairySurface::~FairySurface()
{
    GpuContext& gpu_context = GpuContext::Get();
    gpu_context.device.destroy(render_fence_);
    gpu_context.device.destroyFramebuffer(framebuffer_);
    gpu_context.device.destroyRenderPass(render_pass_);
    render_target_.reset();
    render_target_copy_.reset();
}

void FairySurface::Render(const FairyPipeline* fairy_pipeline)
{
    GpuContext& gpu_context = GpuContext::Get();
    vk::CommandBufferAllocateInfo command_buffer_allocate_info = {};
    command_buffer_allocate_info.commandPool = gpu_context.command_pool;
    command_buffer_allocate_info.level = vk::CommandBufferLevel::ePrimary;
    command_buffer_allocate_info.commandBufferCount = 1;

    vk::UniqueCommandBuffer command_buffer_up =
        std::move(gpu_context.device.allocateCommandBuffersUnique(command_buffer_allocate_info)[0]);
    vk::CommandBuffer command_buffer = command_buffer_up.get();
    command_buffer.reset();

    vk::CommandBufferBeginInfo begin_info = {};
    begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    command_buffer.begin(begin_info);
    vk::ClearValue clear_value = {};
    vk::RenderPassBeginInfo render_pass_begin_info = {};
    render_pass_begin_info.framebuffer = framebuffer_;
    render_pass_begin_info.renderPass = render_pass_;
    render_pass_begin_info.renderArea.offset = vk::Offset2D(0, 0);
    render_pass_begin_info.renderArea.extent = vk::Extent2D(width_, height_);
    render_pass_begin_info.clearValueCount = 1;
    render_pass_begin_info.pClearValues = &clear_value;
    command_buffer.beginRenderPass(render_pass_begin_info, vk::SubpassContents::eInline);
    command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, fairy_pipeline->Pipeline());
    vk::Viewport viewport = { 0.f, 0.f, static_cast<float>(width_), static_cast<float>(height_), 0.f, 1.f };
    vk::Rect2D scissor = { vk::Offset2D(0.f, 0.f), vk::Extent2D(width_, height_) };
    command_buffer.setViewport(0, viewport);
    command_buffer.setScissor(0, scissor);
    command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, fairy_pipeline->PipelineLayout(), 0,
                                      fairy_pipeline->DescriptorSet(), {});
    command_buffer.bindIndexBuffer(fairy_pipeline->IndexBuffer(), vk::DeviceSize(0), vk::IndexType::eUint16);
    command_buffer.drawIndexed(fairy_pipeline->IndexCount(), 1, 0, 0, 0);
    command_buffer.endRenderPass();
    // vk::ImageMemoryBarrier render_target_image_barrier = {};
    // render_target_image_barrier.srcAccessMask = vk::AccessFlagBits::eNone;
    // render_target_image_barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;
    // render_target_image_barrier.oldLayout = vk::ImageLayout::ePresentSrcKHR;
    // render_target_image_barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
    // render_target_image_barrier.image = render_target_texture->Image();
    // render_target_image_barrier.subresourceRange = *render_target_texture->MakeSubresourceRange();
    // command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput,
    //                                vk::PipelineStageFlagBits::eTransfer, static_cast<vk::DependencyFlags>(0), {}, {},
    //                                render_target_image_barrier);
    vk::BufferImageCopy copy_region = {};
    copy_region.imageSubresource = *render_target_->MakeSubresourceLayers();
    copy_region.imageOffset = vk::Offset3D(0, 0, 0);
    copy_region.imageExtent = vk::Extent3D(width_, height_, 1);
    command_buffer.copyImageToBuffer(render_target_->Image(), vk::ImageLayout::eTransferSrcOptimal,
                                     render_target_copy_->Buffer(), 1, &copy_region);
    command_buffer.end();

    vk::SubmitInfo submit_info = {};
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;
    gpu_context.queue.submit(submit_info, render_fence_);
    auto _ = gpu_context.device.waitForFences(render_fence_, true, std::numeric_limits<uint64_t>::max());
    gpu_context.device.resetFences(render_fence_);
}

const void* FairySurface::SurfaceData() const
{
    return render_target_copy_->HostPointer();
}

void FairySurface::CreateRenderTarget()
{
    render_target_ = std::unique_ptr<GpuTexture>(new GpuTexture(
        width_, height_, format_, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eDeviceLocal));
    render_target_copy_ = std::unique_ptr<GpuBuffer>(new GpuBuffer(
        width_ * height_ * 4, vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eHostVisible));
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
    color_attachment.finalLayout = vk::ImageLayout::eTransferSrcOptimal;
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

void FairySurface::CreateFence()
{
    render_fence_ = GpuContext::Get().device.createFence(vk::FenceCreateInfo());
}

} // namespace fv