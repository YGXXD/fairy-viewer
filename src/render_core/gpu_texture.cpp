#include "gpu_texture.hpp"
#include "gpu_context.hpp"

namespace fv
{

static constexpr vk::ImageAspectFlags SubresourceAspectMask(vk::Format format)
{
    switch (format)
    {
    case vk::Format::eD16Unorm:
    case vk::Format::eD32Sfloat:
    case vk::Format::eX8D24UnormPack32:
        return vk::ImageAspectFlagBits::eDepth;
    case vk::Format::eS8Uint:
        return vk::ImageAspectFlagBits::eStencil;
    case vk::Format::eD16UnormS8Uint:
    case vk::Format::eD24UnormS8Uint:
    case vk::Format::eD32SfloatS8Uint:
        return vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
    default:
        return vk::ImageAspectFlagBits::eColor;
    }
}

GpuTexture::GpuTexture(uint32_t width, uint32_t height, vk::Format image_format, vk::ImageUsageFlags image_usage,
                           vk::MemoryPropertyFlags memory_property)
    : format_(image_format), width_(width), height_(height)
{
    bool is_host = static_cast<bool>(memory_property & vk::MemoryPropertyFlagBits::eHostVisible);
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
    alloc_create_info.requiredFlags = static_cast<VkMemoryPropertyFlags>(memory_property);
    alloc_create_info.preferredFlags =
        is_host ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    alloc_create_info.flags = is_host ? VMA_ALLOCATION_CREATE_MAPPED_BIT : 0;

    VkImage image;
    VmaAllocation allocation;
    vmaCreateImage(GpuContext::Get().allocator, &static_cast<VkImageCreateInfo&>(image_create_info),
                   &alloc_create_info, &image, &allocation, nullptr);
    image_ = image;
    image_view_ = GpuContext::Get().device.createImageView(*MakeImageViewCreateInfo());
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
    GpuContext::Get().device.destroyImageView(image_view_);
    vmaDestroyImage(GpuContext::Get().allocator, image_, allocation_);
}

std::unique_ptr<vk::ImageSubresourceLayers> GpuTexture::MakeSubresourceLayers() const
{
    std::unique_ptr<vk::ImageSubresourceLayers> subresourceLayers = std::make_unique<vk::ImageSubresourceLayers>();
    subresourceLayers->aspectMask = SubresourceAspectMask(format_);
    subresourceLayers->mipLevel = 0;
    subresourceLayers->baseArrayLayer = 0;
    subresourceLayers->layerCount = 1;
    return std::move(subresourceLayers);
}

std::unique_ptr<vk::ImageSubresourceRange> GpuTexture::MakeSubresourceRange() const
{
    std::unique_ptr<vk::ImageSubresourceRange> subresourceRange = std::make_unique<vk::ImageSubresourceRange>();
    subresourceRange->aspectMask = SubresourceAspectMask(format_);
    subresourceRange->levelCount = 1;
    subresourceRange->baseMipLevel = 0;
    subresourceRange->layerCount = 1;
    subresourceRange->baseArrayLayer = 0;
    return std::move(subresourceRange);
}

std::unique_ptr<vk::ImageViewCreateInfo> GpuTexture::MakeImageViewCreateInfo() const
{

    std::unique_ptr<vk::ImageViewCreateInfo> create_info = std::make_unique<vk::ImageViewCreateInfo>();
    create_info->image = image_;
    create_info->viewType = vk::ImageViewType::e2D;
    create_info->format = format_;
    create_info->subresourceRange = *MakeSubresourceRange();
    create_info->components = { vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity,
                                vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity };
    return create_info;
}

} // namespace fv