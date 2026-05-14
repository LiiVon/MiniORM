// config.hpp 是 MiniORM 项目的配置中心
// 负责整个项目的基础配置、平台检测、类型别名定义和编译选项管理

#ifndef MINIORM_CORE_CONFIG_HPP
#define MINIORM_CORE_CONFIG_HPP

// 版本
#define MINIORM_VERSION_MAJOR 0
#define MINIORM_VERSION_MINOR 1
#define MINIORM_VERSION_PATCH 0
#define MINIORM_VERSION_STRING "0.1.0"

// 编译器检测
#if defined(_MSC_VER)
#define MINIORM_COMPILER_MSVC
#define MINIORM_COMPILER_VERSION _MSC_VER
#elif defined(__clang__)
#define MINIORM_COMPILER_CLANG
#define MINIORM_COMPILER_VERSION (__clang_major__ * 10000 + __clang_minor__ * 100 + __clang_patchlevel__)
#elif defined(__GNUC__)
#define MINIORM_COMPILER_GCC
#define MINIORM_COMPILER_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#else
#define MINIORM_COMPILER_UNKNOWN
#endif

// 操作系统检测
#if defined(_WIN32) || defined(_WIN64)
#define MINIORM_OS_WINDOWS
#elif defined(__linux__)
#define MINIORM_OS_LINUX
#elif defined(__APPLE__) && defined(__MACH__)
#define MINIORM_OS_MACOS
#include <TargetConditionals.h>
#if TARGET_IPHONE_SIMULATOR || TARGET_OS_IPHONE
#define MINIORM_OS_IOS
#endif
#else
#define MINIORM_OS_UNKNOWN
#endif

// 架构检测 --简化
#if defined(__x86_64__) || defined(_M_X64)
#define MINIORM_ARCH_X64
#elif defined(__i386__) || defined(_M_IX86)
#define MINIORM_ARCH_X86
#elif defined(__aarch64__) || defined(_M_ARM64)
#define MINIORM_ARCH_ARM64
#elif defined(__arm__) || defined(_M_ARM)
#define MINIORM_ARCH_ARM
#else
#define MINIORM_ARCH_UNKNOWN
#endif

// C++ 版本检测
#if defined(_MSVC_LANG)
#define MINIORM_CPLUSPLUS _MSVC_LANG
#else
#define MINIORM_CPLUSPLUS __cplusplus
#endif

#if MINIORM_CPLUSPLUS >= 202002L
#define MINIORM_CPP20 1
#define MINIORM_CPP17 1
#define MINIORM_CPP14 1
#elif MINIORM_CPLUSPLUS >= 201703L
#define MINIORM_CPP20 0
#define MINIORM_CPP17 1
#define MINIORM_CPP14 1
#elif MINIORM_CPLUSPLUS >= 201402L
#define MINIORM_CPP20 0
#define MINIORM_CPP17 0
#define MINIORM_CPP14 1
#else
#define MINIORM_CPP20 0
#define MINIORM_CPP17 0
#define MINIORM_CPP14 0
#endif

// 功能开关
#ifndef MINIORM_ENABLE_LOGGING // 日志
#define MINIORM_ENABLE_LOGGING 1
#endif

#ifndef MINIORM_ENABLE_ASSERT // 断言
#define MINIORM_ENABLE_ASSERT 1
#endif

#ifndef MINIORM_ENABLE_THREAD_SAFE // 线程安全
#define MINIORM_ENABLE_THREAD_SAFE 1
#endif

#ifndef MINIORM_ENABLE_CONNECTION_POOL // 连接池
#define MINIORM_ENABLE_CONNECTION_POOL 1
#endif

#ifndef MINIORM_ENABLE_COMPILE_TIME_REFLECTION // 编译时反射
#define MINIORM_ENABLE_COMPILE_TIME_REFLECTION 1
#endif

// 导出宏 --动态库支持
#if defined(MINIORM_BUILD_SHARED_LIBS)
#if defined(MINIORM_OS_WINDOWS)
#ifdef MINIORM_EXPORTS
#define MINIORM_API __declspec(dllexport)
#else
#define MINIORM_API __declspec(dllimport)
#endif

#else
#ifdef MINIORM_EXPORTS
#define MINIORM_API __attribute__((visibility("default")))
#else
#define MINIORM_API
#endif
#endif

#else
#define MINIORM_API
#endif

// 标准库依赖
#include <cstdint>
#include <cstddef>
#include <string_view>
#include <string>
#include <chrono>
#include <functional>
namespace miniorm
{
    // 基础类型别名
    using int8 = std::int8_t;
    using int16 = std::int16_t;
    using int32 = std::int32_t;
    using int64 = std::int64_t;

    using uint8 = std::uint8_t;
    using uint16 = std::uint16_t;
    using uint32 = std::uint32_t;
    using uint64 = std::uint64_t;

    using float32 = float;
    using float64 = double;

    using char8 = char;
    using char16 = char16_t;
    using char32 = char32_t;
    using wchar = wchar_t;

    using String = std::string;
    using StringView = std::string_view;

    using Size = std::size_t;
    using PtrDiff = std::ptrdiff_t;

    using NullPtr = std::nullptr_t;

    using Byte = std::byte;

    using Bool = bool;

}

// C++20 要求检查
#if !MINIORM_CPP20
#error "MiniORM requires C++20 or later.Please enable C++20 support in your compiler."
#endif

// 头文件存在性检查
#if !defined(__has_include)
#define __has_include(x) 0
#endif

#if !__has_include(<version>)
#warning "Compiler may not fully support C++20. Consider upgrading."
#endif

// 类型大小检查
static_assert(sizeof(miniorm::int32) == 4, "int32 must be 4 bytes");
static_assert(sizeof(miniorm::int64) == 8, "int64 must be 8 bytes");
static_assert(sizeof(miniorm::float32) == 4, "float32 must be 4 bytes");
static_assert(sizeof(miniorm::float64) == 8, "float64 must be 8 bytes");

// 实用宏定义
#define MINIORM_DISABLE_COPY(Class) \
    Class(const Class &) = delete;  \
    Class &operator=(const Class &) = delete

#define MINIORM_DISABLE_MOVE(Class) \
    Class(Class &&) = delete;       \
    Class &operator=(Class &&) = delete

#define MINIORM_DISABLE_COPY_AND_MOVE(Class) \
    MINIORM_DISABLE_COPY(Class);             \
    MINIORM_DISABLE_MOVE(Class)

#define MINIORM_DEFAULT_CONSTRUCTOR(Class) \
    Class() = default

// 字符串化和连接宏
#define MINIORM_CONCAT_IMPL(x, y) x##y
#define MINIORM_CONCAT(x, y) MINIORM_CONCAT_IMPL(x, y)
#define MINIORM_STRINGIFY_IMPL(x) #x
#define MINIORM_STRINGIFY(x) MINIORM_STRINGIFY_IMPL(x)

// 内联提示宏 给编译器内联优化提示
#if defined(MINIORM_COMPILER_MSVC)
#define MINIORM_FORCE_INLINE __forceinline
#define MINIORM_NEVER_INLINE __declspec(noinline)
#elif defined(MINIORM_COMPILER_GCC)
#define MINIORM_FORCE_INLINE __attribute__((always_inline)) inline
#define MINIORM_NEVER_INLINE __attribute__((noinline))
#else
#define MINIORM_FORCE_INLINE inline
#define MINIORM_NEVER_INLINE
#endif

// 缓存行对齐宏 将数据对齐到缓存行边界
#if defined(MINIORM_COMPILER_MSVC)
#define MINIORM_CACHELINE_ALIGN __declspec(align(64))
#elif defined(MINIORM_COMPILER_GCC) || defined(MINIORM_COMPILER_CLANG)
#define MINIORM_CACHELINE_ALIGN __attribute__((aligned(64)))
#else
#define MINIORM_CACHELINE_ALIGN
#endif

// 热路径/冷路径提示 标记函数的冷热程度，指导编译器优化
#if defined(MINIORM_COMPILER_GCC) || defined(MINIORM_COMPILER_CLANG)
#define MINIORM_HOT_PATH __attribute__((hot))
#define MINIORM_COLD_PATH __attribute__((cold))
#else
#define MINIORM_HOT_PATH
#define MINIORM_COLD_PATH
#endif

#define MINIORM_UNUSED(x) (void)(x) //  未使用变量宏

// 分支预测提示
#if defined(MINIORM_COMPILER_GCC) || defined(MINIORM_COMPILER_CLANG)
#define MINIORM_LIKELY(x) __builtin_expect(!!(x), 1)
#define MINIORM_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define MINIORM_LIKELY(x) (x)
#define MINIORM_UNLIKELY(x) (x)
#endif

// 调试和断言宏
#if MINIORM_ENABLE_ASSERT
#include <cassert>
#define MINIORM_ASSERT(expr, msg) assert((expr) && (msg))
#define MINIORM_ASSERT_MSG(expr, msg) assert((expr) && (msg))

#define MINIORM_BOUNDS_CHECK(index, size) \
    MINIORM_ASSERT((index) < (size), "Index out of bounds")

#define MINIORM_NOT_NULLPTR(ptr) \
    MINIORM_ASSERT((ptr) != nullptr, "Pointer must not be null")

#define MINIORM_RANGE_CHECK(value, min, max) \
    MINIORM_ASSERT((value) >= (min) && (value) <= (max), "Value " #value " out of range [" #min ", " #max "]")

#define MINIORM_DIV_CHECK(divisor) \
    MINIORM_ASSERT((divisor) != 0, "Divisor must not be zero")
#else
#define MINIORM_ASSERT(expr, msg) ((void)0)
#define MINIORM_ASSERT_MSG(expr, msg) ((void)0)
#define MINIORM_BOUNDS_CHECK(index, size) ((void)0)
#define MINIORM_NOT_NULLPTR(ptr) ((void)0)
#define MINIORM_RANGE_CHECK(value, min, max) ((void)0)
#define MINIORM_DIV_CHECK(divisor) ((void)0)
#endif

#define MINIORM_STATIC_ASSERT(expr, msg) static_assert((expr), msg)

#endif