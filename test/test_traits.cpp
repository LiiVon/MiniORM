// traits_test.cpp - MiniORM 类型特征系统测试
// 测试 traits.hpp 中定义的类型特征、转换和编译期检查

#include "../include/miniorm/core/config.hpp"
#include "../include/miniorm/core/concepts.hpp"
#include "../include/miniorm/core/traits.hpp"

#include <iostream>
#include <vector>
#include <optional>
#include <memory>
#include <chrono>
#include <string>
#include <cassert>

namespace miniorm
{

    // ==================== 测试类型定义 ====================

    enum class Status
    {
        Active,
        Inactive,
        Pending
    };

    struct EventTime
    {
        int year_;
        int month_;
        int day_;
        int hour_;
        int minute_;
        int second_;

        int year() const { return year_; }
        int month() const { return month_; }
        int day() const { return day_; }
        int hour() const { return hour_; }
        int minute() const { return minute_; }
        int second() const { return second_; }

        bool operator==(const EventTime &other) const
        {
            return year_ == other.year_ && month_ == other.month_ && day_ == other.day_ &&
                   hour_ == other.hour_ && minute_ == other.minute_ && second_ == other.second_;
        }
    };

    struct Account
    {
        int32 id_;
        String name_;
        float64 balance_;

        static StringView table_name() { return "accounts"; }
        static StringView primary_key_name() { return "id"; }
        using PrimaryKeyType = int32;

        bool operator==(const Account &other) const
        {
            return id_ == other.id_ && name_ == other.name_ && balance_ == other.balance_;
        }
    };

    // ==================== 辅助函数 ====================

    template <typename T>
    void print_type_metadata(const StringView &label)
    {
        std::cout << label << std::endl;
        std::cout << "  type_name_v: " << type_name_v<T> << std::endl;
        std::cout << "  type_size_v: " << type_size_v<T> << std::endl;
        std::cout << "  type_align_v: " << type_align_v<T> << std::endl;
    }

    template <DatabaseMappable T>
    void print_database_roundtrip(const T &value)
    {
        String serialized = ToString<T>::convert(value);
        auto parsed = FromString<T>::parse(serialized);
        std::cout << "  value: " << serialized << std::endl;
        std::cout << "  roundtrip: " << std::boolalpha << (parsed == value) << std::endl;
    }

    // ==================== 测试函数 ====================

    void test_type_name_and_size()
    {
        std::cout << "\n=== 测试类型名称和尺寸特征 ===" << std::endl;

        print_type_metadata<int32>("1. int32 元信息");
        print_type_metadata<int64>("2. int64 元信息");
        print_type_metadata<float32>("3. float32 元信息");
        print_type_metadata<float64>("4. float64 元信息");
        print_type_metadata<String>("5. String 元信息");
        print_type_metadata<Bool>("6. Bool 元信息");
        print_type_metadata<Byte>("7. Byte 元信息");

        static_assert(type_size_v<int32> == sizeof(int32), "type_size_v mismatch for int32");
        static_assert(type_align_v<double> == alignof(double), "type_align_v mismatch for double");
        static_assert(type_name_v<Bool> == "Bool", "type_name_v mismatch for Bool");
    }

    void test_sql_type_mapping()
    {
        std::cout << "\n=== 测试 SQL 类型映射 ===" << std::endl;

        std::cout << "1. 整数类型映射:" << std::endl;
        std::cout << "   int8  -> " << sql_type_v<int8> << std::endl;
        std::cout << "   int16 -> " << sql_type_v<int16> << std::endl;
        std::cout << "   int32 -> " << sql_type_v<int32> << std::endl;
        std::cout << "   int64 -> " << sql_type_v<int64> << std::endl;
        std::cout << "   uint8 -> " << sql_type_v<uint8> << std::endl;
        std::cout << "   uint16 -> " << sql_type_v<uint16> << std::endl;
        std::cout << "   uint32 -> " << sql_type_v<uint32> << std::endl;
        std::cout << "   uint64 -> " << sql_type_v<uint64> << std::endl;

        std::cout << "2. 浮点类型映射:" << std::endl;
        std::cout << "   float32 -> " << sql_type_v<float32> << std::endl;
        std::cout << "   float64 -> " << sql_type_v<float64> << std::endl;

        std::cout << "3. 字符串类型映射:" << std::endl;
        std::cout << "   String -> " << sql_type_v<String> << std::endl;
        std::cout << "   StringView -> " << sql_type_v<StringView> << std::endl;
        std::cout << "   const char* -> " << sql_type_v<const char *> << std::endl;

        std::cout << "4. 其他类型映射:" << std::endl;
        std::cout << "   Bool -> " << sql_type_v<Bool> << std::endl;
        std::cout << "   Byte -> " << sql_type_v<Byte> << std::endl;
        std::cout << "   Status type_name -> " << type_name_v<Status> << std::endl;
        std::cout << "   EventTime type_name -> " << type_name_v<EventTime> << std::endl;

        static_assert(sql_type_v<int32> == "INTEGER", "int32 SQL type mismatch");
        static_assert(sql_type_v<float64> == "DOUBLE", "float64 SQL type mismatch");
        static_assert(sql_type_v<String> == "VARCHAR(255)", "String SQL type mismatch");
        static_assert(sql_type_v<Bool> == "BOOLEAN", "Bool SQL type mismatch");
    }

    void test_nullable_traits()
    {
        std::cout << "\n=== 测试可空性特征 ===" << std::endl;

        std::cout << "1. std::optional:" << std::endl;
        std::cout << "   std::optional<int32>: " << is_nullable_v<std::optional<int32>> << std::endl;
        std::cout << "   std::optional<String>: " << is_nullable_v<std::optional<String>> << std::endl;

        std::cout << "2. 指针类型:" << std::endl;
        std::cout << "   int*: " << is_nullable_v<int *> << std::endl;
        std::cout << "   const String*: " << is_nullable_v<const String *> << std::endl;

        std::cout << "3. 智能指针:" << std::endl;
        std::cout << "   std::unique_ptr<int>: " << is_nullable_v<std::unique_ptr<int>> << std::endl;
        std::cout << "   std::shared_ptr<String>: " << is_nullable_v<std::shared_ptr<String>> << std::endl;

        std::cout << "4. 非空类型:" << std::endl;
        std::cout << "   int32: " << is_nullable_v<int32> << std::endl;
        std::cout << "   String: " << is_nullable_v<String> << std::endl;

        static_assert(is_nullable_v<std::optional<int32>>, "optional should be nullable");
        static_assert(is_nullable_v<int *>, "pointer should be nullable");
        static_assert(!is_nullable_v<int32>, "int32 should not be nullable");
    }

    void test_default_value_traits()
    {
        std::cout << "\n=== 测试默认值特征 ===" << std::endl;

        std::cout << "1. 基础类型默认值:" << std::endl;
        std::cout << "   int32: " << DefaultValue<int32>::value << std::endl;
        std::cout << "   int64: " << DefaultValue<int64>::value << std::endl;
        std::cout << "   float32: " << DefaultValue<float32>::value << std::endl;
        std::cout << "   float64: " << DefaultValue<float64>::value << std::endl;
        std::cout << "   Bool: " << DefaultValue<Bool>::value << std::endl;

        std::cout << "2. 字节和字符串默认值:" << std::endl;
        std::cout << "   Byte: " << static_cast<unsigned>(DefaultValue<Byte>::value) << std::endl;
        std::cout << "   String: '" << default_value<String>() << "'" << std::endl;

        std::cout << "3. 可空类型默认值:" << std::endl;
        std::cout << "   std::optional<int32> has_value: " << DefaultValue<std::optional<int32>>::value.has_value() << std::endl;

        static_assert(DefaultValue<int32>::value == 0, "int32 default value mismatch");
        static_assert(DefaultValue<Bool>::value == false, "Bool default value mismatch");
        static_assert(DefaultValue<std::optional<int32>>::value == std::nullopt, "optional default mismatch");
    }

    void test_primary_key_traits()
    {
        std::cout << "\n=== 测试主键和自增特征 ===" << std::endl;

        std::cout << "1. 主键类型:" << std::endl;
        std::cout << "   int32: " << is_primary_key_v<int32> << std::endl;
        std::cout << "   int64: " << is_primary_key_v<int64> << std::endl;
        std::cout << "   String: " << is_primary_key_v<String> << std::endl;
        std::cout << "   Bool: " << is_primary_key_v<Bool> << std::endl;

        std::cout << "2. 自增特征:" << std::endl;
        std::cout << "   int32: " << is_auto_increment_v<int32> << std::endl;
        std::cout << "   int64: " << is_auto_increment_v<int64> << std::endl;
        std::cout << "   Bool: " << is_auto_increment_v<Bool> << std::endl;
        std::cout << "   String: " << is_auto_increment_v<String> << std::endl;

        std::cout << "3. 实体主键特征:" << std::endl;
        std::cout << "   Account::PrimaryKeyType is primary key: "
                  << IsPrimaryKey<typename Account::PrimaryKeyType>::value << std::endl;
        std::cout << "   Account::PrimaryKeyType is auto increment: "
                  << IsAutoIncrement<typename Account::PrimaryKeyType>::value << std::endl;

        static_assert(is_primary_key_v<int32>, "int32 should be a primary key type");
        static_assert(is_auto_increment_v<int32>, "int32 should support auto increment");
        static_assert(!is_auto_increment_v<Bool>, "Bool should not support auto increment");
    }

    void test_conversion_traits()
    {
        std::cout << "\n=== 测试字符串转换特征 ===" << std::endl;

        std::cout << "1. ToString 转换:" << std::endl;
        std::cout << "   int32 123 -> " << ToString<int32>::convert(123) << std::endl;
        std::cout << "   float64 3.14 -> " << ToString<float64>::convert(3.14) << std::endl;
        std::cout << "   Bool true -> " << ToString<Bool>::convert(true) << std::endl;
        std::cout << "   String -> " << ToString<String>::convert("MiniORM") << std::endl;
        std::cout << "   StringView -> " << ToString<StringView>::convert("ViewText") << std::endl;
        std::cout << "   Byte -> " << ToString<Byte>::convert(Byte{7}) << std::endl;
        std::cout << "   optional<int32> -> " << ToString<std::optional<int32>>::convert(std::optional<int32>{42}) << std::endl;
        std::cout << "   optional<int32>(null) -> " << ToString<std::optional<int32>>::convert(std::optional<int32>{}) << std::endl;

        std::cout << "2. FromString 解析:" << std::endl;
        std::cout << "   int32 '123' -> " << FromString<int32>::parse("123") << std::endl;
        std::cout << "   float64 '2.5' -> " << FromString<float64>::parse("2.5") << std::endl;
        std::cout << "   Bool 'TRUE' -> " << FromString<Bool>::parse("TRUE") << std::endl;
        std::cout << "   String '\''hello\'' -> " << FromString<String>::parse("'hello'") << std::endl;
        std::cout << "   Byte X'07' -> " << static_cast<unsigned>(FromString<Byte>::parse("X'07'")) << std::endl;
        std::cout << "   optional<int32> 'NULL' -> has_value: "
                  << FromString<std::optional<int32>>::parse("NULL").has_value() << std::endl;

        std::cout << "3. Roundtrip 验证:" << std::endl;
        print_database_roundtrip<int32>(123);
        print_database_roundtrip<float64>(3.14);
        print_database_roundtrip<Bool>(true);
        print_database_roundtrip<String>("MiniORM");
        print_database_roundtrip<Byte>(Byte{7});
    }

    void test_complete_database_type()
    {
        std::cout << "\n=== 测试完整数据库类型特征 ===" << std::endl;

        std::cout << "1. 基础类型:" << std::endl;
        std::cout << "   int32: " << is_complete_database_type_v<int32> << std::endl;
        std::cout << "   float64: " << is_complete_database_type_v<float64> << std::endl;
        std::cout << "   String: " << is_complete_database_type_v<String> << std::endl;
        std::cout << "   Bool: " << is_complete_database_type_v<Bool> << std::endl;

        std::cout << "2. 其他类型:" << std::endl;
        std::cout << "   Byte: " << is_complete_database_type_v<Byte> << std::endl;
        std::cout << "   Status type_name: " << type_name_v<Status> << std::endl;
        std::cout << "   EventTime type_name: " << type_name_v<EventTime> << std::endl;

        static_assert(is_complete_database_type_v<int32>, "int32 should be complete database type");
        static_assert(is_complete_database_type_v<String>, "String should be complete database type");
        static_assert(is_complete_database_type_v<float64>, "float64 should be complete database type");
    }

    void test_trait_selection()
    {
        std::cout << "\n=== 测试类型特征工具 ===" << std::endl;

        using Selected1 = TypeTraitSelector_t<true, TypeName<int32>, TypeName<String>>;
        using Selected2 = TypeTraitSelector_t<false, TypeName<int32>, TypeName<String>>;

        std::cout << "1. 类型特征选择器:" << std::endl;
        std::cout << "   Selected1::type::value = " << Selected1::value << std::endl;
        std::cout << "   Selected2::type::value = " << Selected2::value << std::endl;

        using Applied1 = ApplyTrait_t<TypeIdentity, int32>;
        using Applied2 = ApplyTrait_t<TypeIdentity, String>;

        std::cout << "2. 类型特征应用器:" << std::endl;
        std::cout << "   ApplyTrait_t<TypeIdentity, int32> type_name = " << type_name_v<Applied1> << std::endl;
        std::cout << "   ApplyTrait_t<TypeIdentity, String> type_name = " << type_name_v<Applied2> << std::endl;

        static_assert(std::is_same_v<Selected1, TypeName<int32>>, "trait selector true branch mismatch");
        static_assert(std::is_same_v<Selected2, TypeName<String>>, "trait selector false branch mismatch");
        static_assert(std::is_same_v<Applied1, int32>, "apply trait identity mismatch");
        static_assert(std::is_same_v<Applied2, String>, "apply trait identity mismatch");
    }

    void test_compile_time_checks()
    {
        std::cout << "\n=== 测试编译时断言和概念验证宏 ===" << std::endl;

        std::cout << "1. 类型特征断言:" << std::endl;
        MINIORM_TRAIT_ASSERT(int32, IsPrimaryKey);
        MINIORM_TRAIT_ASSERT(String, IsPrimaryKey);
        std::cout << "   ✓ MINIORM_TRAIT_ASSERT(int32, IsPrimaryKey) passed" << std::endl;
        std::cout << "   ✓ MINIORM_TRAIT_ASSERT(String, IsPrimaryKey) passed" << std::endl;

        std::cout << "2. SQL类型检查:" << std::endl;
        MINIORM_CHECK_SQL_TYPE(int32);
        MINIORM_CHECK_SQL_TYPE(float64);
        MINIORM_CHECK_SQL_TYPE(String);
        std::cout << "   ✓ MINIORM_CHECK_SQL_TYPE for int32/float64/String passed" << std::endl;

        std::cout << "3. 完整数据库类型静态断言:" << std::endl;
        static_assert(is_complete_database_type_v<int32>, "int32 must be complete");
        static_assert(is_complete_database_type_v<String>, "String must be complete");
        std::cout << "   ✓ is_complete_database_type_v<int32> and <String> passed" << std::endl;
    }

    void test_traits_with_entities()
    {
        std::cout << "\n=== 测试 traits 与实体的结合 ===" << std::endl;

        std::cout << "1. 实体元信息:" << std::endl;
        std::cout << "   Account::table_name(): " << Account::table_name() << std::endl;
        std::cout << "   Account::primary_key_name(): " << Account::primary_key_name() << std::endl;
        std::cout << "   Account::PrimaryKeyType type_name: " << type_name_v<typename Account::PrimaryKeyType> << std::endl;

        std::cout << "2. 实体主键检查:" << std::endl;
        std::cout << "   Account::PrimaryKeyType is_primary_key_v: "
                  << is_primary_key_v<typename Account::PrimaryKeyType> << std::endl;
        std::cout << "   Account::PrimaryKeyType is_auto_increment_v: "
                  << is_auto_increment_v<typename Account::PrimaryKeyType> << std::endl;

        static_assert(EntityType<Account>, "Account should be EntityType");
        static_assert(EntityWithPrimaryKey<Account>, "Account should be EntityWithPrimaryKey");
    }

    // ==================== 主函数 ====================
} // namespace miniorm

using namespace miniorm;

int main()
{
    std::cout << "==========================================" << std::endl;
    std::cout << "MiniORM Traits Test Program" << std::endl;
    std::cout << "测试 traits.hpp 中的所有类型特征和转换" << std::endl;
    std::cout << "==========================================" << std::endl;

    try
    {
        test_type_name_and_size();
        test_sql_type_mapping();
        test_nullable_traits();
        test_default_value_traits();
        test_primary_key_traits();
        test_conversion_traits();
        test_complete_database_type();
        test_trait_selection();
        test_compile_time_checks();
        test_traits_with_entities();

        std::cout << "\n==========================================" << std::endl;
        std::cout << "所有测试完成！" << std::endl;
        std::cout << "MiniORM traits 系统功能正常" << std::endl;
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
