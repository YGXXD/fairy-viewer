#pragma once

#include <memory>
#include <vulkan/vulkan.hpp>
#include "fairy_viewer.hpp"
#include "vk_mem_alloc.h"

namespace fv
{

class FairyTexture
{
public:
    FairyTexture(uint32_t width, uint32_t height, vk::Format image_format, vk::ImageUsageFlags image_usage,
                 vk::MemoryPropertyFlags memory_property);
    FV_DELETE_COPY_MOVE(FairyTexture)
    ~FairyTexture();

    std::unique_ptr<vk::ImageSubresourceLayers> MakeSubresourceLayers() const;
    std::unique_ptr<vk::ImageSubresourceRange> MakeSubresourceRange() const;
    FV_INLINE vk::Format Format() const { return format_; }
    FV_INLINE uint32_t Width() const { return width_; }
    FV_INLINE uint32_t Height() const { return height_; }
    FV_INLINE vk::Image Image() const { return image_; }
    FV_INLINE vk::ImageView ImageView() const { return image_view_; }
    FV_INLINE void* HostPointer() const { return host_pointer_; }

private:
    std::unique_ptr<vk::ImageViewCreateInfo> MakeImageViewCreateInfo() const;

    vk::Format format_;
    uint32_t width_;
    uint32_t height_;
    vk::Image image_;
    vk::ImageView image_view_;
    VmaAllocation allocation_;
    void* host_pointer_;
};

} // namespace fv