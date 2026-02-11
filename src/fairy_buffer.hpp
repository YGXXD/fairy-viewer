#pragma once

#include <memory>
#include <vulkan/vulkan.hpp>
#include "fairy_viewer.hpp"
#include "vk_mem_alloc.h"

namespace fv
{

class FairyBuffer
{
public:
    FairyBuffer(size_t size, vk::BufferUsageFlags buffer_usage, vk::MemoryPropertyFlags memory_property);
    FV_DELETE_COPY_MOVE(FairyBuffer)
    ~FairyBuffer();

    vk::UniqueBufferView CreateBufferView(vk::Format buffer_format, VkDeviceSize offset = 0,
                                          VkDeviceSize range = VK_WHOLE_SIZE) const;
    FV_INLINE size_t Size() const { return size_; }
    FV_INLINE vk::Buffer Buffer() const { return buffer_; }
    FV_INLINE void* HostPointer() const { return host_pointer_; }

private:
    std::unique_ptr<vk::BufferViewCreateInfo> MakeBufferViewCreateInfo(vk::Format buffer_format,
                                                                       VkDeviceSize offset = 0,
                                                                       VkDeviceSize range = VK_WHOLE_SIZE) const;

    size_t size_;
    vk::Buffer buffer_;
    VmaAllocation allocation_;
    void* host_pointer_;
};

} // namespace fv