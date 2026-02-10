#pragma once

#include <vulkan/vulkan.hpp>

#include "fairy_viewer.h"
#include "vk_mem_alloc.h"

namespace fv
{

class GPUContext
{
public:
    GPUContext();
    FV_DELETE_COPY_MOVE(GPUContext);
    ~GPUContext();

private:
    vk::UniqueInstance instance;
    vk::PhysicalDevice physicalDevice;
    vk::UniqueDevice device;
    vk::UniqueCommandPool commandPool;
    VmaAllocator allocator;
};

} // namespace fv