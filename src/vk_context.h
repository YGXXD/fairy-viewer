#pragma once

#include <vulkan/vulkan.hpp>

#include "fv_define.h"
#include "vk_mem_alloc.h"

namespace fv
{

class VkContext
{
public:
    FV_SINGLETON_IMPL(VkContext)
    FV_DELETE_COPY_MOVE(VkContext)
    static void Init();
    static void Quit();

    vk::Instance instance;
    vk::PhysicalDevice physical_device;
    vk::Device device;
    vk::Queue queue;
    vk::CommandPool command_pool;
    VmaAllocator allocator;

private:
    VkContext();
    ~VkContext();
};

} // namespace fv