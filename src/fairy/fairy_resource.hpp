#pragma once

#include <vector>
#include <memory>
#include <vulkan/vulkan.hpp>
#include "ktm/ktm.h"
#include "../fairy_viewer.hpp"

namespace gpu
{
class GpuBuffer;
class GpuTexture;
} // namespace gpu

namespace fairy
{

class FairyResource
{
public:
    FairyResource();
    FV_DELETE_COPY_MOVE(FairyResource)
    ~FairyResource();

    void Update_iResolution(const ktm::fvec3& i_resolution) const;
    void Update_iTime(float i_time) const;
    void Update_iTimeDelta(float i_time_delta) const;
    void Update_iFrameRate(float i_frame_rate) const;
    void Update_iFrame(int i_frame) const;
    void Update_iChannelTime(int index) const;
    void Update_iChannelResolution(int index) const;
    void Update_iMouse(const ktm::fvec4& i_mouse) const;
    void Update_iChannel(int index) const;
    void Update_iDate(const ktm::fvec4& i_date) const;

    std::unique_ptr<gpu::GpuBuffer> i_resolution_buffer;
    std::unique_ptr<gpu::GpuBuffer> i_time_buffer;
    std::unique_ptr<gpu::GpuBuffer> i_time_delta_buffer;
    std::unique_ptr<gpu::GpuBuffer> i_frame_rate_buffer;
    std::unique_ptr<gpu::GpuBuffer> i_frame_buffer;
    std::unique_ptr<gpu::GpuBuffer> i_channel_time_4_buffer;
    std::unique_ptr<gpu::GpuBuffer> i_channel_resolution_4_buffer;
    std::unique_ptr<gpu::GpuBuffer> i_mouse_buffer;
    std::unique_ptr<gpu::GpuTexture> i_channel_4_texture[4];
    std::unique_ptr<gpu::GpuBuffer> i_date_buffer;
};

} // namespace fairy