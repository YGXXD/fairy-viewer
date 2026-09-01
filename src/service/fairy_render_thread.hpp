#pragma once

#include <memory>
#include <atomic>
#include <thread>
#include <future>
#include <chrono>
#include <vulkan/vulkan.hpp>
#include "ktm/ktm.h"

namespace fv
{
class GpuBuffer;
class FairySurface;
class FairyPipeline;
} // namespace fv

namespace service
{

class FairyRenderThread final
{
public:
    FairyRenderThread(int fairy_surface_width, int fairy_surface_height, int fairy_buffer_count);
    ~FairyRenderThread();

    std::unique_ptr<uint8_t[]> RequestCopySurface();
    inline int SurfaceWidth() const { return surface_width_; }
    inline int SurfaceHeight() const { return surface_height_; }

private:
    void FairyRenderThreadMain();
    void InitAppSubmitContext();
    void DestoryAppSubmitContext();
    void InitFairy();
    void DestoryFairy();
    void RenderFairy(int index);
    void ResetPipeline(const std::string& codes);

    int surface_width_;
    int surface_height_;
    vk::Format surface_format_;
    int buffer_count_;

    std::atomic<bool> is_run_;
    std::thread run_thread_;

    vk::Queue gpu_queue_;
    vk::CommandPool gpu_command_pool_;
    std::vector<vk::CommandBuffer> gpu_command_buffers_;
    std::vector<vk::Fence> gpu_fences_;
    std::vector<bool> is_gpu_fence_reset_;

    std::unique_ptr<fv::FairySurface> fairy_surface_;
    std::vector<vk::Semaphore> fairy_complete_signals_;
    std::unique_ptr<fv::FairyPipeline> fairy_pipeline_;
    bool pipeline_reset_status_;

    int current_render_index_;
    std::chrono::steady_clock::time_point fairy_start_ticks_;
    float i_time_;
    float i_time_delta_;
    float i_frame_rate_;
    int i_frame_;
    ktm::fvec4 i_mouse_;
    ktm::fvec4 i_date_;

    std::mutex surface_copy_mutex_;
    std::vector<std::promise<std::pair<std::shared_ptr<fv::GpuBuffer>, vk::Fence>>> surface_copy_promises_;
};

}; // namespace service
