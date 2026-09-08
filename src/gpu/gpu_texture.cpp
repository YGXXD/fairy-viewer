#include "gpu_texture.hpp"
#include "gpu_context.hpp"

namespace gpu
{

GpuTexture::GpuTexture(uint32_t width, uint32_t height, vk::Format image_format, vk::ImageUsageFlags image_usage,
                       GpuMemory::Usage memory_usage)
    : format_(image_format), width_(width), height_(height)
{
    bool is_host = GpuMemory::IsHostRead(memory_usage);
    vk::ImageCreateInfo image_create_info = {};
    image_create_info.imageType = vk::ImageType::e2D;
    image_create_info.format = image_format;
    image_create_info.extent = vk::Extent3D(width_, height_, 1);
    image_create_info.mipLevels = 1;
    image_create_info.arrayLayers = 1;
    image_create_info.samples = vk::SampleCountFlagBits::e1;
    image_create_info.tiling = is_host ? vk::ImageTiling::eLinear : vk::ImageTiling::eOptimal;
    image_create_info.initialLayout = vk::ImageLayout::eUndefined;
    image_create_info.usage = image_usage;
    image_create_info.sharingMode = vk::SharingMode::eExclusive;

    VmaAllocationCreateInfo alloc_create_info = {};
    alloc_create_info.usage = GpuMemory::GetVmaMemoryUsage(memory_usage);
    alloc_create_info.flags = is_host ? VMA_ALLOCATION_CREATE_MAPPED_BIT : 0;

    VkImage image;
    VmaAllocation allocation;
    vmaCreateImage(GpuContext::Get().allocator, &static_cast<VkImageCreateInfo&>(image_create_info), &alloc_create_info,
                   &image, &allocation, nullptr);
    image_ = image;
    allocation_ = allocation;

    if (is_host)
    {
        VmaAllocationInfo allocation_info;
        vmaGetAllocationInfo(GpuContext::Get().allocator, allocation_, &allocation_info);
        host_pointer_ = allocation_info.pMappedData;
    }
}

GpuTexture::~GpuTexture()
{
    vmaDestroyImage(GpuContext::Get().allocator, image_, allocation_);
}

vk::UniqueImageView GpuTexture::CreateImageView(vk::ImageAspectFlags aspect_flags) const
{
    return GpuContext::Get().device.createImageViewUnique(*MakeImageViewCreateInfo(aspect_flags));
}

std::unique_ptr<vk::ImageSubresourceLayers> GpuTexture::MakeSubresourceLayers(vk::ImageAspectFlags aspect_flags) const
{
    std::unique_ptr<vk::ImageSubresourceLayers> subresource_layers = std::make_unique<vk::ImageSubresourceLayers>();
    subresource_layers->aspectMask = aspect_flags;
    subresource_layers->mipLevel = 0;
    subresource_layers->baseArrayLayer = 0;
    subresource_layers->layerCount = 1;
    return std::move(subresource_layers);
}

std::unique_ptr<vk::ImageSubresourceRange> GpuTexture::MakeSubresourceRange(vk::ImageAspectFlags aspect_flags) const
{
    std::unique_ptr<vk::ImageSubresourceRange> subresource_range = std::make_unique<vk::ImageSubresourceRange>();
    subresource_range->aspectMask = aspect_flags;
    subresource_range->levelCount = 1;
    subresource_range->baseMipLevel = 0;
    subresource_range->layerCount = 1;
    subresource_range->baseArrayLayer = 0;
    return std::move(subresource_range);
}

std::unique_ptr<vk::ImageViewCreateInfo> GpuTexture::MakeImageViewCreateInfo(vk::ImageAspectFlags aspect_flags) const
{

    std::unique_ptr<vk::ImageViewCreateInfo> create_info = std::make_unique<vk::ImageViewCreateInfo>();
    create_info->image = image_;
    create_info->viewType = vk::ImageViewType::e2D;
    create_info->format = format_;
    create_info->subresourceRange = *MakeSubresourceRange(aspect_flags);
    create_info->components = { vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity,
                                vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity };
    return create_info;
}

} // namespace gpu