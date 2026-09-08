#pragma once

#include "../fairy_viewer.hpp"
#include "vk_mem_alloc.h"

namespace gpu
{

class GpuMemory
{
public:
    enum class Usage
    {
        eCpuOnly,
        eCpuToGpu,
        eGpuToCpu,
        eGpuOnly,
    };

    static FV_INLINE VmaMemoryUsage GetVmaMemoryUsage(Usage usage)
    {
        switch (usage)
        {
        case Usage::eCpuOnly:
            return VMA_MEMORY_USAGE_CPU_ONLY;
        case Usage::eCpuToGpu:
            return VMA_MEMORY_USAGE_CPU_TO_GPU;
        case Usage::eGpuToCpu:
            return VMA_MEMORY_USAGE_GPU_TO_CPU;
        case Usage::eGpuOnly:
            return VMA_MEMORY_USAGE_GPU_ONLY;
        }
        return VMA_MEMORY_USAGE_UNKNOWN;
    }

    static FV_INLINE bool IsHostRead(Usage usage)
    {
        switch (usage)
        {
        case Usage::eCpuOnly:
        case Usage::eCpuToGpu:
        case Usage::eGpuToCpu:
            return true;
        case Usage::eGpuOnly:
            return false;
        }
        return false;
    }
};

} // namespace gpu
