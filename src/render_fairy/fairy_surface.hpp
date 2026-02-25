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

    void Render(const FairyPipeline* fairy_pipeline);

    const void* SurfaceData() const;
    FV_INLINE vk::RenderPass RenderPass() const { return render_pass_; }

private:
    void CreateRenderTarget();
    void CreateRenderPass();
    void CreateFramebuffer();
    void CreateFence();

    uint32_t width_;
    uint32_t height_;
    vk::Format format_;

    std::unique_ptr<GpuTexture> render_target_;
    vk::RenderPass render_pass_;
    vk::Framebuffer framebuffer_;
    std::unique_ptr<GpuBuffer> render_target_copy_;

    vk::Fence render_fence_;
};

} // namespace fv