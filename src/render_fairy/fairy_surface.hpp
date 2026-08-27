#pragma once

#include <memory>
#include <vector>
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
    FairySurface(uint32_t width, uint32_t height, vk::Format format, uint32_t buffer_count);
    FV_DELETE_COPY_MOVE(FairySurface)
    ~FairySurface();

    void AddWaitSemaphore(vk::Semaphore semaphore, vk::PipelineStageFlags stage);
    void AddSignalSemaphore(vk::Semaphore semaphore);
    void Render(const FairyPipeline* fairy_pipeline, int index);
    void WaitRenderComplete();
    void WaitRenderComplete(int index);
    FV_INLINE uint32_t BufferCount() const { return buffer_count_; }
    FV_INLINE const GpuTexture* RenderTarget(int index) const { return render_targets_[index].get(); };
    FV_INLINE vk::RenderPass RenderPass() const { return render_pass_; }

private:
    void CreateRenderPass();
    void CreateRenderTarget();
    void CreateFramebuffer();
    void CreateSubmitResource();

    uint32_t width_;
    uint32_t height_;
    vk::Format format_;
    uint32_t buffer_count_;

    vk::RenderPass render_pass_;
    std::vector<std::unique_ptr<GpuTexture>> render_targets_;
    std::vector<vk::Framebuffer> framebuffers_;

    vk::Queue render_queue_;
    vk::CommandPool render_command_pool_;
    std::vector<vk::CommandBuffer> render_command_buffers_;
    std::vector<vk::Fence> render_fences_;

    std::vector<vk::PipelineStageFlags> need_wait_stages_;
    std::vector<vk::Semaphore> need_wait_semaphores_;
    std::vector<vk::Semaphore> need_signal_semaphores_;
};

} // namespace fv