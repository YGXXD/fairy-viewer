#pragma once

#include <vulkan/vulkan.hpp>
#include "../fairy_viewer.hpp"
#include "vk_mem_alloc.h"

namespace fv
{

class GpuContext
{
public:
    FV_SINGLETON_IMPL(GpuContext)
    FV_DELETE_COPY_MOVE(GpuContext)
    static void Init();
    static void Quit();

    vk::Instance instance;
    vk::PhysicalDevice physical_device;
    uint32_t queue_family_index;
    vk::Device device;
    VmaAllocator allocator;

private:
    GpuContext();
    ~GpuContext();
};

} // namespace fv