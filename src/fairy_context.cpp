#include "fairy_context.hpp"

#if defined(FV_DEBUG_ENABLE)
#    include <iostream>
#endif

namespace fv
{

constexpr uint32_t vulkan_api_version_ = VK_API_VERSION_1_2;
vk::Instance instance_;
vk::PhysicalDevice physical_device_;
uint32_t queue_family_index_;
vk::Device device_;
vk::Queue queue_;
vk::CommandPool command_pool_;
VmaAllocator allocator_;

std::vector<const char*> instance_extension_names_ = {
    "VK_KHR_surface",
#if defined(FV_PLATFORM_APPLE)
    // Apple does not officially support Vulkan. To run Vulkan on Apple
    // platforms using MoltenVK, this extension allows
    // the physical device to enable the
    // "VK_KHR_portability_subset" extension.
    "VK_KHR_portability_enumeration", "VK_MVK_macos_surface", "VK_EXT_metal_surface",
#elif defined(FV_PLATFORM_WINDOWS)
    "VK_KHR_win32_surface",
#endif
    "VK_KHR_get_physical_device_properties2"
};
std::vector<const char*> device_extension_names_ = { "VK_KHR_create_renderpass2",
#if defined(FV_PLATFORM_APPLE)
                                                     // This extension allows Vulkan to be built on top of other
                                                     // graphics APIs, macOS requires it to be built on top of Metal.
                                                     "VK_KHR_portability_subset",
#endif
                                                     "VK_KHR_swapchain" };

void CreateInstance()
{
    vk::ApplicationInfo app_info = {};
    app_info.pApplicationName = "fairy";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "fairy";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = vulkan_api_version_;

    vk::InstanceCreateInfo instance_create_info = {};
    instance_create_info.pApplicationInfo = &app_info;
#if defined(FV_DEBUG_ENABLE)
    auto available_extensions = vk::enumerateInstanceExtensionProperties();
    for (const auto& extension_name : instance_extension_names_)
    {
        auto it = std::find_if(available_extensions.begin(), available_extensions.end(),
                               [extension_name](const auto& available_extension)
        {
            return std::string_view(available_extension.extensionName) == std::string_view(extension_name);
        });
        if (it == available_extensions.end())
        {
            std::cerr << "vulkan does not support enabling instance extensions:\n";
            std::cerr << "\t" << extension_name << std::endl;
        }
    }
#endif
    instance_create_info.enabledExtensionCount = instance_extension_names_.size();
    instance_create_info.ppEnabledExtensionNames = instance_extension_names_.data();
#if defined(FV_PLATFORM_APPLE)
    instance_create_info.flags |= vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;
#endif
#if defined(FV_DEBUG_ENABLE)
    const char* validation_layers = "VK_LAYER_KHRONOS_validation";
    auto available_layers = vk::enumerateInstanceLayerProperties();
    auto it =
        std::find_if(available_layers.begin(), available_layers.end(), [validation_layers](const auto& available_layer)
    {
        return std::string_view(available_layer.layerName) == std::string_view(validation_layers);
    });
    if (it == available_layers.end())
    {
        std::cerr << "Vulkan不支持开启InstanceExtension:\n";
        std::cerr << "\t" << validation_layers << std::endl;
    }
    instance_create_info.enabledLayerCount = 1;
    instance_create_info.ppEnabledLayerNames = &validation_layers;
#endif
    instance_ = vk::createInstance(instance_create_info);
}

void CreatePhysicalDeviceAndProperties()
{
    std::vector<vk::PhysicalDevice> physical_devices = instance_.enumeratePhysicalDevices();
    auto physical_device_it =
        std::find_if(physical_devices.begin(), physical_devices.end(), [](const vk::PhysicalDevice& pd) -> bool
    {
        return pd.getProperties().deviceType == vk::PhysicalDeviceType::eDiscreteGpu;
    });
    physical_device_ = physical_device_it != physical_devices.end() ? *physical_device_it : physical_devices.front();

    std::vector<vk::QueueFamilyProperties2> queue_family_properties = physical_device_.getQueueFamilyProperties2();
    auto queue_family_propertie_it = std::find_if(queue_family_properties.begin(), queue_family_properties.end(),
                                                  [](const vk::QueueFamilyProperties2& qfp) -> bool
    {
        return static_cast<bool>(
            qfp.queueFamilyProperties.queueFlags &
            (vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eTransfer | vk::QueueFlagBits::eCompute));
    });
    queue_family_index_ = queue_family_propertie_it - queue_family_properties.begin();
}

void CreateDeviceAndQueue()
{
    float queue_priority = 1.0f;
    vk::DeviceQueueCreateInfo queue_create_info = {};
    queue_create_info.queueFamilyIndex = queue_family_index_;
    queue_create_info.queueCount = 1;
    queue_create_info.pQueuePriorities = &queue_priority;
    vk::PhysicalDeviceFeatures2 physical_device_features = physical_device_.getFeatures2();
    vk::DeviceCreateInfo device_create_info = {};
    device_create_info.pNext = &physical_device_features;
    device_create_info.queueCreateInfoCount = 1;
    device_create_info.pQueueCreateInfos = &queue_create_info;
#if defined(FV_DEBUG_ENABLE)
    auto available_extensions = physical_device_.enumerateDeviceExtensionProperties();
    for (const auto& extension_name : device_extension_names_)
    {
        auto it = std::find_if(available_extensions.begin(), available_extensions.end(),
                               [extension_name](const auto& available_extension)
        {
            return std::string_view(available_extension.extensionName) == std::string_view(extension_name);
        });
        if (it == available_extensions.end())
        {
            std::cerr << "vulkan does not support enabling device extensions:\n";
            std::cerr << "\t" << extension_name << std::endl;
        }
    }
#endif
    device_create_info.enabledExtensionCount = device_extension_names_.size();
    device_create_info.ppEnabledExtensionNames = device_extension_names_.data();

    device_ = physical_device_.createDevice(device_create_info);
    queue_ = device_.getQueue(queue_family_index_, 0);
}

void CreateCommandPool()
{
    vk::CommandPoolCreateInfo command_pool_create_info = {};
    command_pool_create_info.queueFamilyIndex = queue_family_index_;
    command_pool_create_info.flags |= vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    command_pool_create_info.flags |= vk::CommandPoolCreateFlagBits::eTransient;
    command_pool_ = device_.createCommandPool(command_pool_create_info);
}

void CreateVmaMemoryAllocator()
{
    VmaAllocatorCreateInfo allocator_info = {};
    allocator_info.vulkanApiVersion = vulkan_api_version_;
    allocator_info.instance = instance_;
    allocator_info.physicalDevice = physical_device_;
    allocator_info.device = device_;
    vmaCreateAllocator(&allocator_info, &allocator_);
}

void FairyContext::Init()
{
    CreateInstance();
    CreatePhysicalDeviceAndProperties();
    CreateDeviceAndQueue();
    CreateCommandPool();
    CreateVmaMemoryAllocator();
    Get().instance = instance_;
    Get().allocator = allocator_;
    Get().physical_device = physical_device_;
    Get().device = device_;
    Get().queue = queue_;
    Get().command_pool = command_pool_;
}

void FairyContext::Quit()
{
    vmaDestroyAllocator(allocator_);
    device_.destroyCommandPool(command_pool_);
    device_.destroy();
    instance_.destroy();
    Get().instance = nullptr;
    Get().allocator = nullptr;
    Get().physical_device = nullptr;
    Get().device = nullptr;
    Get().command_pool = nullptr;
}

FairyContext::FairyContext() = default;

FairyContext::~FairyContext() = default;

} // namespace fv