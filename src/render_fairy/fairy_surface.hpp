#pragma once

#include <memory>
#include <vulkan/vulkan.hpp>
#include "../fairy_viewer.hpp"
#include "vk_mem_alloc.h"

namespace fv
{

class GpuBuffer;
class GpuTexture;
class FairyPipeline;
class FairySurface
{
public:
    FairySurface(uint32_t width, uint32_t height, vk::Format format);
    FV_DELETE_COPY_MOVE(FairySurface)
    ~FairySurface();

    void Render(const FairyPipeline* fairy_pipeline, vk::Semaphore& out_signal_semaphore);
    void WaitGpuIfNeeded();
    FV_INLINE const GpuTexture* RenderTarget() const { return render_target_.get(); };
    FV_INLINE vk::RenderPass RenderPass() const { return render_pass_; }

private:
    void CreateRenderTarget();
    void CreateRenderPass();
    void CreateFramebuffer();
    void CreateSubmitResource();

    uint32_t width_;
    uint32_t height_;
    vk::Format format_;

    std::unique_ptr<GpuTexture> render_target_;
    vk::RenderPass render_pass_;
    vk::Framebuffer framebuffer_;

    vk::CommandBuffer render_command_buffer_;
    vk::Semaphore render_signal_semaphore_;
    bool need_wait_render_fence_;
    vk::Fence render_fence_;
};

} // namespace fv