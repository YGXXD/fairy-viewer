#include "fairy_resource.hpp"
#include "../gpu/gpu_context.hpp"
#include "../gpu/gpu_buffer.hpp"
#include "../gpu/gpu_texture.hpp"

namespace fairy
{

FairyResource::FairyResource()
{
    i_resolution_buffer = std::unique_ptr<gpu::GpuBuffer>(new gpu::GpuBuffer(
        sizeof(ktm::fvec3), vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible));
    i_time_buffer = std::unique_ptr<gpu::GpuBuffer>(new gpu::GpuBuffer(
        sizeof(float), vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible));
    i_time_delta_buffer = std::unique_ptr<gpu::GpuBuffer>(new gpu::GpuBuffer(
        sizeof(float), vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible));
    i_frame_rate_buffer = std::unique_ptr<gpu::GpuBuffer>(new gpu::GpuBuffer(
        sizeof(float), vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible));
    i_frame_buffer = std::unique_ptr<gpu::GpuBuffer>(new gpu::GpuBuffer(
        sizeof(int), vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible));
    // todo i_channel_time_4_buffer_
    // todo i_channel_resolution_4_buffer_
    i_mouse_buffer = std::unique_ptr<gpu::GpuBuffer>(new gpu::GpuBuffer(
        sizeof(ktm::fvec4), vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible));
    // todo i_channel_4_texture_
    i_date_buffer = std::unique_ptr<gpu::GpuBuffer>(new gpu::GpuBuffer(
        sizeof(ktm::fvec4), vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible));
}

FairyResource::~FairyResource() = default;

void FairyResource::Update_iResolution(const ktm::fvec3& i_resolution) const
{
    ktm::fvec3* i_resolution_ptr = static_cast<ktm::fvec3*>(i_resolution_buffer->HostPointer());
    *i_resolution_ptr = i_resolution;
}

void FairyResource::Update_iTime(float i_time) const
{
    float* i_time_ptr = static_cast<float*>(i_time_buffer->HostPointer());
    *i_time_ptr = i_time;
}

void FairyResource::Update_iTimeDelta(float i_time_delta) const
{
    float* i_time_delta_ptr = static_cast<float*>(i_time_delta_buffer->HostPointer());
    *i_time_delta_ptr = i_time_delta;
}

void FairyResource::Update_iFrameRate(float i_frame_rate) const
{
    float* i_frame_rate_ptr = static_cast<float*>(i_frame_rate_buffer->HostPointer());
    *i_frame_rate_ptr = i_frame_rate;
}

void FairyResource::Update_iFrame(int i_frame) const
{
    int* i_frame_ptr = static_cast<int*>(i_frame_buffer->HostPointer());
    *i_frame_ptr = i_frame;
}

void FairyResource::Update_iChannelTime(int index) const
{
    // todo
}

void FairyResource::Update_iChannelResolution(int index) const
{
    // todo
}

void FairyResource::Update_iMouse(const ktm::fvec4& i_mouse) const
{
    ktm::fvec4* i_mouse_ptr = static_cast<ktm::fvec4*>(i_mouse_buffer->HostPointer());
    *i_mouse_ptr = i_mouse;
}

void FairyResource::Update_iChannel(int index) const
{
    // todo
}

void FairyResource::Update_iDate(const ktm::fvec4& i_date) const
{
    ktm::fvec4* i_date_ptr = static_cast<ktm::fvec4*>(i_date_buffer->HostPointer());
    *i_date_ptr = i_date;
}

} // namespace fairy