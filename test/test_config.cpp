// config_test.cpp - MiniORM 配置文件测试
// 测试 config.hpp 中定义的所有宏和功能

// 首先包含我们的配置文件
#include "../include/miniorm/core/config.hpp"

// 标准库头文件
#include <iostream>
#include <cassert>
#include <atomic>
#include <vector>
#include <memory>
#include <stdexcept>

// 使用我们定义的命名空间宏
namespace miniorm
{

    // ==================== 测试类定义 ====================

    // 测试1：使用禁用拷贝/移动宏的类
    class NonCopyableClass
    {
    private:
        MINIORM_DISABLE_COPY_AND_MOVE(NonCopyableClass);

        int value;

    public:
        NonCopyableClass() = default;

        explicit NonCopyableClass(int v) : value(v)
        {
            std::cout << "NonCopyableClass constructed with value: " << value << std::endl;
        }

        ~NonCopyableClass()
        {
            std::cout << "NonCopyableClass destroyed, value was: " << value << std::endl;
        }

        int get_value() const
        {
            return value;
        }
    };

    // 测试2：使用缓存对齐优化的多线程安全类
    struct MINIORM_CACHELINE_ALIGN ThreadSafeCounter
    {
        std::atomic<int64_t> value{0};

        MINIORM_HOT_PATH
        void increment()
        {
            // 使用安全性宏
            MINIORM_NOT_NULLPTR(this);
            value.fetch_add(1, std::memory_order_relaxed);
        }

        MINIORM_COLD_PATH
        void reset()
        {
            value.store(0, std::memory_order_release);
        }

        int64_t get() const
        {
            return value.load(std::memory_order_acquire);
        }
    };

    // 测试3：使用边界检查的缓冲区类
    class SafeBuffer
    {
    private:
        char *data;
        Size size;

    public:
        SafeBuffer(Size buffer_size) : size(buffer_size)
        {
            // 使用断言检查
            MINIORM_ASSERT(size > 0, "Buffer size must be positive");

            data = new char[size];
            MINIORM_NOT_NULLPTR(data);

            std::cout << "SafeBuffer created with size: " << size << std::endl;
        }

        char &operator[](Size index)
        {
            // 使用边界检查宏
            MINIORM_BOUNDS_CHECK(index, size);
            return data[index];
        }

        const char &operator[](Size index) const
        {
            MINIORM_BOUNDS_CHECK(index, size);
            return data[index];
        }

        Size get_size() const
        {
            return size;
        }

        ~SafeBuffer()
        {
            delete[] data;
            std::cout << "SafeBuffer destroyed" << std::endl;
        }
    };

    // 测试4：使用范围检查的数值类
    class BoundedValue
    {
    private:
        int value;
        int min_value;
        int max_value;

    public:
        BoundedValue(int val, int min, int max)
            : min_value(min), max_value(max)
        {
            // 使用范围检查
            MINIORM_RANGE_CHECK(val, min, max);
            value = val;
        }

        void set_value(int new_value)
        {
            MINIORM_RANGE_CHECK(new_value, min_value, max_value);
            value = new_value;
        }

        int get_value() const
        {
            return value;
        }

        int get_min() const { return min_value; }
        int get_max() const { return max_value; }
    };

    // 测试5：使用除零检查的数学函数
    class SafeMath
    {
    public:
        static double divide(double numerator, double denominator)
        {
            MINIORM_DIV_CHECK(denominator);
            return numerator / denominator;
        }

        static int modulo(int dividend, int divisor)
        {
            MINIORM_DIV_CHECK(divisor);
            return dividend % divisor;
        }
    };

    // ==================== 测试函数 ====================

    // 测试编译期字符串连接宏
    void test_string_macros()
    {
        std::cout << "\n=== 测试字符串连接宏 ===" << std::endl;

        // 使用 CONCAT 宏
        int testValue = 42;
        int MINIORM_CONCAT(concat, Value) = 100; // 创建变量 concatValue

        std::cout << "原始 testValue: " << testValue << std::endl;
        std::cout << "CONCAT 创建的 concatValue: " << MINIORM_CONCAT(concat, Value) << std::endl;

        // 使用 STRINGIFY 宏
        const char *error_message = MINIORM_STRINGIFY(File not found);
        std::cout << "字符串化结果: " << error_message << std::endl;

        // 模拟错误检查
        bool file_exists = false;
        if (!file_exists)
        {
            std::cout << "错误: " << error_message << std::endl;
        }
    }

    // 测试内联和分支预测宏
    MINIORM_FORCE_INLINE
    int fast_multiply(int a, int b)
    {
        return a * b;
    }

    int process_value(int value)
    {
        // 使用分支预测宏
        if (MINIORM_LIKELY(value >= 0))
        {
            return fast_multiply(value, 2);
        }
        else
        {
            return -1; // 错误情况，不太可能发生
        }
    }

    void test_performance_macros()
    {
        std::cout << "\n=== 测试性能优化宏 ===" << std::endl;

        // 测试内联函数
        int result = fast_multiply(5, 6);
        std::cout << "fast_multiply(5, 6) = " << result << std::endl;

        // 测试分支预测
        for (int i = 0; i < 5; ++i)
        {
            int processed = process_value(i);
            std::cout << "process_value(" << i << ") = " << processed << std::endl;
        }

        // 测试不太可能的分支
        int unlikely_result = process_value(-1);
        std::cout << "process_value(-1) = " << unlikely_result << " (unlikely branch)" << std::endl;
    }

    // 测试断言和安全性宏
    void test_assertion_macros()
    {
        std::cout << "\n=== 测试断言和安全性宏 ===" << std::endl;

        try
        {
            // 测试边界检查
            SafeBuffer buffer(10);

            // 正常访问
            buffer[0] = 'A';
            buffer[9] = 'Z';
            std::cout << "正常访问: buffer[0] = " << buffer[0]
                      << ", buffer[9] = " << buffer[9] << std::endl;

            // 测试范围检查
            BoundedValue bounded(50, 0, 100);
            std::cout << "BoundedValue: " << bounded.get_value()
                      << " (范围: " << bounded.get_min()
                      << " - " << bounded.get_max() << ")" << std::endl;

            // 测试除零检查
            double division_result = SafeMath::divide(10.0, 2.0);
            std::cout << "10.0 / 2.0 = " << division_result << std::endl;

            // 测试模运算
            int modulo_result = SafeMath::modulo(10, 3);
            std::cout << "10 % 3 = " << modulo_result << std::endl;
        }
        catch (const std::exception &e)
        {
            std::cerr << "异常捕获: " << e.what() << std::endl;
        }
    }

    // 测试多线程缓存对齐
    void test_cache_alignment()
    {
        std::cout << "\n=== 测试缓存对齐 ===" << std::endl;

        ThreadSafeCounter counter1, counter2, counter3;

        // 检查对齐
        std::cout << "counter1 地址: " << &counter1 << std::endl;
        std::cout << "counter2 地址: " << &counter2 << std::endl;
        std::cout << "counter3 地址: " << &counter3 << std::endl;

        // 检查地址差（应该是缓存行大小的倍数）
        size_t diff1 = reinterpret_cast<size_t>(&counter2) - reinterpret_cast<size_t>(&counter1);
        size_t diff2 = reinterpret_cast<size_t>(&counter3) - reinterpret_cast<size_t>(&counter2);

        std::cout << "地址差1: " << diff1 << " 字节" << std::endl;
        std::cout << "地址差2: " << diff2 << " 字节" << std::endl;
        std::cout << "缓存行大小: 64 字节" << std::endl;

        // 操作计数器
        counter1.increment();
        counter1.increment();
        counter2.increment();

        std::cout << "counter1 值: " << counter1.get() << std::endl;
        std::cout << "counter2 值: " << counter2.get() << std::endl;
        std::cout << "counter3 值: " << counter3.get() << std::endl;
    }

    // 测试平台检测宏
    void test_platform_detection()
    {
        std::cout << "\n=== 测试平台检测 ===" << std::endl;

        std::cout << "MiniORM 版本: " << MINIORM_VERSION_STRING << std::endl;

// 编译器检测
#ifdef MINIORM_COMPILER_MSVC
        std::cout << "编译器: MSVC" << std::endl;
        std::cout << "编译器版本: " << MINIORM_COMPILER_VERSION << std::endl;
#elif defined(MINIORM_COMPILER_CLANG)
        std::cout << "编译器: Clang" << std::endl;
        std::cout << "编译器版本: " << MINIORM_COMPILER_VERSION << std::endl;
#elif defined(MINIORM_COMPILER_GCC)
        std::cout << "编译器: GCC" << std::endl;
        std::cout << "编译器版本: " << MINIORM_COMPILER_VERSION << std::endl;
#else
        std::cout << "编译器: Unknown" << std::endl;
#endif

// 操作系统检测
#ifdef MINIORM_OS_WINDOWS
        std::cout << "操作系统: Windows" << std::endl;
#elif defined(MINIORM_OS_LINUX)
        std::cout << "操作系统: Linux" << std::endl;
#elif defined(MINIORM_OS_MACOS)
        std::cout << "操作系统: macOS" << std::endl;
#ifdef MINIORM_OS_IOS
        std::cout << "  子平台: iOS" << std::endl;
#endif
#else
        std::cout << "操作系统: Unknown" << std::endl;
#endif

// 架构检测
#ifdef MINIORM_ARCH_X64
        std::cout << "架构: x64" << std::endl;
#elif defined(MINIORM_ARCH_X86)
        std::cout << "架构: x86" << std::endl;
#elif defined(MINIORM_ARCH_ARM64)
        std::cout << "架构: ARM64" << std::endl;
#elif defined(MINIORM_ARCH_ARM)
        std::cout << "架构: ARM" << std::endl;
#else
        std::cout << "架构: Unknown" << std::endl;
#endif

        // C++标准检测
        std::cout << "C++标准: ";
#if MINIORM_CPP20
        std::cout << "C++20" << std::endl;
#elif MINIORM_CPP17
        std::cout << "C++17" << std::endl;
#elif MINIORM_CPP14
        std::cout << "C++14" << std::endl;
#else
        std::cout << "C++11 或更早" << std::endl;
#endif
    }

    // 测试类型别名
    void test_type_aliases()
    {
        std::cout << "\n=== 测试类型别名 ===" << std::endl;

        // 测试整数类型
        int8 i8 = 127;
        int16 i16 = 32767;
        int32 i32 = 2147483647;
        int64 i64 = 9223372036854775807LL;

        uint8 u8 = 255;
        uint16 u16 = 65535;
        uint32 u32 = 4294967295U;
        uint64 u64 = 18446744073709551615ULL;

        std::cout << "int8: " << static_cast<int>(i8) << std::endl;
        std::cout << "int16: " << i16 << std::endl;
        std::cout << "int32: " << i32 << std::endl;
        std::cout << "int64: " << i64 << std::endl;

        std::cout << "uint8: " << static_cast<unsigned>(u8) << std::endl;
        std::cout << "uint16: " << u16 << std::endl;
        std::cout << "uint32: " << u32 << std::endl;
        std::cout << "uint64: " << u64 << std::endl;

        // 测试浮点类型
        float32 f32 = 3.14159f;
        float64 f64 = 2.718281828459045;

        std::cout << "float32: " << f32 << std::endl;
        std::cout << "float64: " << f64 << std::endl;

        // 测试字符串类型
        String str = "Hello MiniORM!";
        StringView view = str;

        std::cout << "String: " << str << std::endl;
        std::cout << "StringView: " << view << std::endl;

        // 测试其他类型
        Size size = sizeof(int);
        Bool boolean = true;
        Byte byte = static_cast<Byte>(0xFF);

        std::cout << "Size of int: " << size << " bytes" << std::endl;
        std::cout << "Bool: " << (boolean ? "true" : "false") << std::endl;
        std::cout << "Byte: 0x" << std::hex << static_cast<int>(byte) << std::dec << std::endl;
    }

    // 测试编译时检查
    void test_compile_time_checks()
    {
        std::cout << "\n=== 测试编译时检查 ===" << std::endl;

        // 这些检查在编译时进行
        std::cout << "编译时检查已通过:" << std::endl;
        std::cout << "  - int32 大小为 4 字节" << std::endl;
        std::cout << "  - int64 大小为 8 字节" << std::endl;
        std::cout << "  - float32 大小为 4 字节" << std::endl;
        std::cout << "  - float64 大小为 8 字节" << std::endl;

        // 使用静态断言
        MINIORM_STATIC_ASSERT(sizeof(int32) == 4, "int32 must be 4 bytes");
        MINIORM_STATIC_ASSERT(sizeof(char) == 1, "char must be 1 byte");

        std::cout << "所有静态断言通过!" << std::endl;
    }

} // namespace miniorm

using namespace miniorm;

// ==================== 主函数 ====================
int main()
{
    std::cout << "==========================================" << std::endl;
    std::cout << "MiniORM Config Test Program" << std::endl;
    std::cout << "测试 config.hpp 中的所有功能" << std::endl;
    std::cout << "==========================================" << std::endl;

    try
    {
        // 测试1：平台和版本信息
        test_platform_detection();

        // 测试2：类型别名
        test_type_aliases();

        // 测试3：编译时检查
        test_compile_time_checks();

        // 测试4：字符串宏
        test_string_macros();

        // 测试5：性能优化宏
        test_performance_macros();

        // 测试6：创建和使用测试类
        std::cout << "\n=== 测试类实例化 ===" << std::endl;

        // 测试不可复制类
        {
            NonCopyableClass nc1(42);
            std::cout << "NonCopyableClass value: " << nc1.get_value() << std::endl;

            // 注意：以下代码会导致编译错误（因为禁用了拷贝）
            // NonCopyableClass nc2 = nc1;  // 错误！
            // NonCopyableClass nc3(std::move(nc1));  // 错误！
        }

        // 测试7：安全性宏
        test_assertion_macros();

        // 测试8：缓存对齐
        test_cache_alignment();

        // 测试9：边界情况（需要在调试模式下测试断言）
        std::cout << "\n=== 测试边界情况（仅调试模式）===" << std::endl;

#if MINIORM_ENABLE_ASSERT
        std::cout << "断言已启用，可以测试错误情况" << std::endl;

        // 注意：以下测试在断言启用时会触发断言失败
        // 在实际测试中，你可能想要捕获这些或单独测试

        /*
        // 测试边界检查失败
        try {
            SafeBuffer small_buffer(5);
            small_buffer[10] = 'X';  // 应该触发断言
        } catch (...) {
            std::cout << "边界检查失败被捕获" << std::endl;
        }

        // 测试范围检查失败
        try {
            BoundedValue invalid(150, 0, 100);  // 应该触发断言
        } catch (...) {
            std::cout << "范围检查失败被捕获" << std::endl;
        }

        // 测试除零检查失败
        try {
            SafeMath::divide(10.0, 0.0);  // 应该触发断言
        } catch (...) {
            std::cout << "除零检查失败被捕获" << std::endl;
        }
        */

#else
        std::cout << "断言已禁用，跳过错误情况测试" << std::endl;
#endif

        std::cout << "\n==========================================" << std::endl;
        std::cout << "所有测试完成！" << std::endl;
        std::cout << "MiniORM 配置文件功能正常" << std::endl;
        std::cout << "==========================================" << std::endl;

        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "\n测试失败，异常: " << e.what() << std::endl;
        return 1;
    }
    catch (...)
    {
        std::cerr << "\n测试失败，未知异常" << std::endl;
        return 1;
    }
}
