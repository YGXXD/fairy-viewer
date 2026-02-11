#pragma once

// os platform config, only support 64-bit os
#if defined(__APPLE__)
#    if defined(__LP64__)
#        define FV_PLATFORM_APPLE
#        include <TargetConditionals.h>
#        if TARGET_IPHONE_SIMULATOR == 1
#            error "fairy-viewer not support ios simulator!"
#        elif TARGET_OS_IPHONE == 1
#            error "fairy-viewer not support ios!"
#        elif TARGET_OS_MAC == 1
#        else
#            error "fairy-viewer not support unknown macos platform!"
#        endif
#    else
#        error "fairy-viewer not support 32-bit apple platform!"
#    endif
#elif defined(_WIN32)
#    if defined(_WIN64)
#        define FV_PLATFORM_WINDOWS
#    else
#        error "fairy-viewer not support 32-bit windows platform!"
#    endif
#elif defined(__linux__)
#    define FV_PLATFORM_LINUX
#    error "fairy-viewer not support linux platform!"
#elif defined(__ANDROID__)
#    define FV_PLATFORM_ANDROID
#    error "fairy-viewer not support android platform!"
#else
#    error "fairy-viewer not support unknown platform!"
#endif

// debug state
#ifndef NDEBUG
#    define FV_DEBUG_ENABLE
#endif

// c++ compiler config
#if defined(__clang__) && defined(__GNUC__)
#    define FV_COMPILER_CLANG
#elif defined(__GNUC__) || defined(__MINGW32__)
#    define FV_COMPILER_GCC
#elif defined(_MSC_VER)
#    define FV_COMPILER_MSVC
#else
#    error "fairy-viewer not support unknown compiler, it only support clang++, g++ and vc++"
#endif

// function Function config
#if defined(FV_COMPILER_CLANG)
#    define FV_INLINE __inline__ __attribute__((always_inline))
#    define FV_NOINLINE __attribute__((noinline))
#    define FV_FUNC __inline__ __attribute__((always_inline, nothrow, nodebug))
#elif defined(FV_COMPILER_GCC)
#    define FV_INLINE __inline__ __attribute__((__always_inline__))
#    define FV_NOINLINE __attribute__((__noinline__))
#    define FV_FUNC __inline__ __attribute__((__always_inline__, __nothrow__, __artificial__))
#elif defined(FV_COMPILER_MSVC)
#    define FV_INLINE __forceinline
#    define FV_NOINLINE __declspec(noinline)
#    define FV_NOTHROW __declspec(nothrow)
#    define FV_FUNC __forceinline __declspec(nothrow)
#endif

#define FV_SINGLETON_IMPL(ClassName)             \
    static FV_INLINE ClassName& Get()            \
    {                                            \
        static ClassName ClassName##Instance {}; \
        return ClassName##Instance;              \
    }

#define FV_DELETE_COPY_MOVE(ClassName)               \
    ClassName(const ClassName&) = delete;            \
    ClassName(ClassName&&) = delete;                 \
    ClassName& operator=(const ClassName&) = delete; \
    ClassName& operator=(ClassName&&) = delete;