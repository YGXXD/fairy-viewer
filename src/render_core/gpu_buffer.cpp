#include "gpu_buffer.hpp"
#include "gpu_context.hpp"

namespace fv
{

GpuBuffer::GpuBuffer(size_t size, vk::BufferUsageFlags buffer_usage, vk::MemoryPropertyFlags memory_property)
    : size_(size), host_pointer_(nullptr)
{
    bool is_host = static_cast<bool>(memory_property & vk::MemoryPropertyFlagBits::eHostVisible);
    vk::BufferCreateInfo buffer_create_info = {};
    buffer_create_info.size = size_;
    buffer_create_info.usage = buffer_usage;

    VmaAllocationCreateInfo alloc_create_info = {};
    alloc_create_info.requiredFlags = static_cast<VkMemoryPropertyFlags>(memory_property);
    alloc_create_info.preferredFlags =
        is_host ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    alloc_create_info.flags = is_host ? VMA_ALLOCATION_CREATE_MAPPED_BIT : 0;

    VkBuffer buffer;
    VmaAllocation allocation;
    vmaCreateBuffer(GpuContext::Get().allocator, &static_cast<VkBufferCreateInfo&>(buffer_create_info),
                    &alloc_create_info, &buffer, &allocation, nullptr);
    buffer_ = buffer;
    allocation_ = allocation;
    if (is_host)
    {
        VmaAllocationInfo allocation_info;
        vmaGetAllocationInfo(GpuContext::Get().allocator, allocation_, &allocation_info);
        host_pointer_ = allocation_info.pMappedData;
    }
}

GpuBuffer::~GpuBuffer()
{
    vmaDestroyBuffer(GpuContext::Get().allocator, buffer_, allocation_);
}

vk::UniqueBufferView GpuBuffer::CreateBufferView(vk::Format buffer_format, VkDeviceSize offset,
                                                   VkDeviceSize range) const
{
    auto create_info = MakeBufferViewCreateInfo(buffer_format, offset, range);
    return GpuContext::Get().device.createBufferViewUnique(*create_info);
}

std::unique_ptr<vk::BufferViewCreateInfo>
GpuBuffer::MakeBufferViewCreateInfo(vk::Format buffer_format, VkDeviceSize offset, VkDeviceSize range) const
{
    std::unique_ptr<vk::BufferViewCreateInfo> create_info = std::make_unique<vk::BufferViewCreateInfo>();
    create_info->buffer = buffer_;
    create_info->format = buffer_format;
    create_info->offset = offset;
    create_info->range = range;
    return create_info;
}

} // namespace fv