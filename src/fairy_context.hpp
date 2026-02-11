#pragma once

#include <vulkan/vulkan.hpp>
#include "fairy_viewer.hpp"
#include "vk_mem_alloc.h"

namespace fv
{

class FairyContext
{
public:
    FV_SINGLETON_IMPL(FairyContext)
    FV_DELETE_COPY_MOVE(FairyContext)
    static void Init();
    static void Quit();

    vk::Instance instance;
    vk::PhysicalDevice physical_device;
    vk::Device device;
    vk::Queue queue;
    vk::CommandPool command_pool;
    VmaAllocator allocator;

private:
    FairyContext();
    ~FairyContext();
};

} // namespace fv