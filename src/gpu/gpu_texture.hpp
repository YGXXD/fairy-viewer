#pragma once

#include <memory>
#include <vulkan/vulkan.hpp>
#include "../fairy_viewer.hpp"
#include "gpu_memory.hpp"
#include "vk_mem_alloc.h"

namespace gpu
{

class GpuTexture
{
public:
    GpuTexture(uint32_t width, uint32_t height, vk::Format image_format, vk::ImageUsageFlags image_usage,
               GpuMemory::Usage memory_usage);
    FV_DELETE_COPY_MOVE(GpuTexture)
    ~GpuTexture();

    vk::UniqueImageView CreateImageView(vk::ImageAspectFlags aspect_flags) const;
    std::unique_ptr<vk::ImageSubresourceLayers> MakeSubresourceLayers(vk::ImageAspectFlags aspect_flags) const;
    std::unique_ptr<vk::ImageSubresourceRange> MakeSubresourceRange(vk::ImageAspectFlags aspect_flags) const;
    FV_INLINE vk::Format Format() const { return format_; }
    FV_INLINE uint32_t Width() const { return width_; }
    FV_INLINE uint32_t Height() const { return height_; }
    FV_INLINE vk::Image Image() const { return image_; }
    FV_INLINE void* HostPointer() const { return host_pointer_; }

private:
    std::unique_ptr<vk::ImageViewCreateInfo> MakeImageViewCreateInfo(vk::ImageAspectFlags aspect_flags) const;

    vk::Format format_;
    uint32_t width_;
    uint32_t height_;
    vk::Image image_;
    VmaAllocation allocation_;
    void* host_pointer_;
};

} // namespace gpu