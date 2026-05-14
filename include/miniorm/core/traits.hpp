// traits.hpp 是 MiniORM 项目的类型特征系统核心
// 负责定义 C++ 类型到数据库类型的映射关系、类型可空性检查、默认值获取、字符串转换等关键功能

#ifndef MINIORM_CORE_TRAITS_HPP
#define MINIORM_CORE_TRAITS_HPP

#include "config.hpp"
#include "concepts.hpp"

#if !MINIORM_CPP20
#error "traits.hpp requires C++20 support. Please enable C++20 in your compiler settings."
#endif

#include <type_traits>
#include <string>
#include <string_view>
#include <limits>
#include <cstddef>
#include <charconv>
#include <optional>
#include <memory>
#include <stdexcept>

namespace miniorm
{

    template <typename T> // 辅助模板
    struct always_false : std::false_type
    {
    };

    template <typename T> // 主模板定义
    struct TypeIdentity
    {
        using type = T;
    };

    template <typename T> // 类型别名
    using TypeIdentity_t = typename TypeIdentity<T>::type;

    template <typename T> // 获取类型大小和对齐
    struct TypeSize : std::integral_constant<Size, sizeof(T)>
    {
    };

    template <typename T>
    struct TypeAlign : std::integral_constant<Size, alignof(T)>
    {
    };

    template <typename T>
    constexpr Size type_size_v = TypeSize<T>::value;

    template <typename T>
    constexpr Size type_align_v = TypeAlign<T>::value;

    template <typename T>
    struct TypeName
    {
        static constexpr StringView value = "Unknown";
    };

    template <>
    struct TypeName<int32>
    {
        static constexpr StringView value = "int32";
    };

    template <>
    struct TypeName<int64>
    {
        static constexpr StringView value = "int64";
    };

    template <>
    struct TypeName<float32>
    {
        static constexpr StringView value = "float32";
    };

    template <>
    struct TypeName<float64>
    {
        static constexpr StringView value = "float64";
    };

    template <>
    struct TypeName<String>
    {
        static constexpr StringView value = "String";
    };

    template <>
    struct TypeName<StringView>
    {
        static constexpr StringView value = "StringView";
    };

    template <>
    struct TypeName<Bool>
    {
        static constexpr StringView value = "Bool";
    };

    template <>
    struct TypeName<Byte>
    {
        static constexpr StringView value = "Byte";
    };

    template <typename T>
    constexpr StringView type_name_v = TypeName<T>::value;

    template <typename T> //  C++ 类型到 SQL 类型的映射
    struct SqlType
    {

        static_assert(DatabaseMappable<T>,
                      "Type must be DatabaseMappable to have SQL type mapping");

        static constexpr StringView value = "TEXT";
    };

    template <IntegerType T>
    struct SqlType<T>
    {
        static constexpr StringView value = "INTEGER";
    };

    template <>
    struct SqlType<int8>
    {
        static constexpr StringView value = "TINYINT";
    };

    template <>
    struct SqlType<int16>
    {
        static constexpr StringView value = "SMALLINT";
    };

    template <>
    struct SqlType<int32>
    {
        static constexpr StringView value = "INTEGER";
    };

    template <>
    struct SqlType<int64>
    {
        static constexpr StringView value = "BIGINT";
    };

    template <>
    struct SqlType<uint8>
    {
        static constexpr StringView value = "TINYINT UNSIGNED";
    };

    template <>
    struct SqlType<uint16>
    {
        static constexpr StringView value = "SMALLINT UNSIGNED";
    };

    template <>
    struct SqlType<uint32>
    {
        static constexpr StringView value = "INTEGER UNSIGNED";
    };

    template <>
    struct SqlType<uint64>
    {
        static constexpr StringView value = "BIGINT UNSIGNED";
    };

    template <FloatingType T>
    struct SqlType<T>
    {
        static constexpr StringView value = "REAL";
    };

    template <>
    struct SqlType<float32>
    {
        static constexpr StringView value = "FLOAT";
    };

    template <>
    struct SqlType<float64>
    {
        static constexpr StringView value = "DOUBLE";
    };

    template <StringType T>
    struct SqlType<T>
    {
        static constexpr StringView value = "VARCHAR(255)";
    };

    template <>
    struct SqlType<String>
    {
        static constexpr StringView value = "VARCHAR(255)";
    };

    template <>
    struct SqlType<StringView>
    {
        static constexpr StringView value = "VARCHAR(255)";
    };

    template <>
    struct SqlType<const char *>
    {
        static constexpr StringView value = "VARCHAR(255)";
    };

    template <BooleanType T>
    struct SqlType<T>
    {
        static constexpr StringView value = "BOOLEAN";
    };

    template <>
    struct SqlType<Byte>
    {
        static constexpr StringView value = "BLOB";
    };

    template <TimeType T>
    struct SqlType<T>
    {
        static constexpr StringView value = "DATETIME";
    };

    template <EnumType T>
    struct SqlType<T>
    {
        static constexpr StringView value = "INTEGER";
    };

    template <typename T>
    constexpr StringView sql_type_v = SqlType<T>::value;

    template <typename T>
    struct IsNullable : std::false_type
    {
    };

    template <typename T>
    struct IsNullable<std::optional<T>> : std::true_type
    {
    };

    template <typename T>
    struct IsNullable<T *> : std::true_type
    {
    };

    template <typename T>
    struct IsNullable<const T *> : std::true_type
    {
    };

    template <typename T>
    struct IsNullable<std::unique_ptr<T>> : std::true_type
    {
    };

    template <typename T>
    struct IsNullable<std::shared_ptr<T>> : std::true_type
    {
    };

    template <typename T>
    constexpr Bool is_nullable_v = IsNullable<T>::value;

    template <typename T>
    struct DefaultValue
    {
        static_assert(std::is_default_constructible_v<T>,
                      "Type must be default constructible to have a default value");

        static constexpr T value = T();
    };

    template <>
    struct DefaultValue<int32>
    {
        static constexpr int32 value = 0;
    };

    template <>
    struct DefaultValue<int64>
    {
        static constexpr int64 value = 0;
    };

    template <>
    struct DefaultValue<float32>
    {
        static constexpr float32 value = 0.0f;
    };

    template <>
    struct DefaultValue<float64>
    {
        static constexpr float64 value = 0.0;
    };

    template <>
    struct DefaultValue<Bool>
    {
        static constexpr Bool value = false;
    };

    template <>
    struct DefaultValue<Byte>
    {
        static constexpr Byte value = Byte{0};
    };

    template <>
    struct DefaultValue<String>
    {

        static String value()
        {
            return "";
        }
    };

    template <typename T>
    struct DefaultValue<std::optional<T>>
    {

        static constexpr std::optional<T> value = std::nullopt;
    };

    template <typename T>
    inline auto default_value() -> decltype(auto)
    {
        if constexpr (std::is_same_v<T, String>)
        {
            return DefaultValue<String>::value();
        }
        else
        {
            return DefaultValue<T>::value;
        }
    }

    template <typename T>
    constexpr auto default_value_v = default_value<T>();

    template <typename T>
    struct IsPrimaryKey : std::bool_constant<
                              IntegerType<T> ||
                              StringType<T>>
    {
    };

    template <typename T>
    constexpr Bool is_primary_key_v = IsPrimaryKey<T>::value;

    template <typename T>
    struct IsAutoIncrement : std::bool_constant<
                                 IntegerType<T> && !BooleanType<T>>
    {
    };

    template <typename T>
    constexpr Bool is_auto_increment_v = IsAutoIncrement<T>::value;

    // ToString 和 FromString 模板结构体，提供 C++ 类型与字符串之间的转换功能，支持数据库交互中的数据序列化和反序列化
    template <typename T>
    struct ToString
    {

        static_assert(DatabaseMappable<T>,
                      "Type must be DatabaseMappable to be convertible to string");

        static String convert(const T &value)
        {

            if constexpr (IntegerType<T>)
            {
                return std::to_string(value);
            }
            else if constexpr (FloatingType<T>)
            {
                return std::to_string(value);
            }
            else if constexpr (BooleanType<T>)
            {
                return value ? "TRUE" : "FALSE";
            }
            else if constexpr (StringType<T>)
            {

                String str_value;
                if constexpr (std::is_same_v<T, String>)
                {
                    str_value = value;
                }
                else if constexpr (std::is_same_v<T, StringView>)
                {
                    str_value = String(value);
                }
                else if constexpr (std::is_same_v<T, const char *> || std::is_same_v<T, char *>)
                {
                    str_value = String(value);
                }

                String res = "'";
                for (char c : str_value)
                {
                    if (c == '\'')
                    {
                        res += "''";
                    }
                    else
                    {
                        res += c;
                    }
                }
                res += "'";
                return res;
            }
            else if constexpr (same_as<T, Byte>)
            {

                return "X'" + std::to_string(static_cast<unsigned>(value)) + "'";
            }
            else
            {
                static_assert(always_false<T>::value,
                              "Unsupported type for ToString conversion");
            }
        }
    };

    template <typename T>
    struct ToString<std::optional<T>>
    {
        static String convert(const std::optional<T> &value)
        {
            if (value.has_value())
            {
                return ToString<T>::convert(value.value());
            }
            else
            {
                return "NULL";
            }
        }
    };

    template <typename T>
    struct FromString
    {

        static_assert(DatabaseMappable<T>,
                      "Type must be DatabaseMappable to parse from string");

        static T parse(const StringView &str)
        {

            if constexpr (IntegerType<T>)
            {
                T result{};
                auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), result);
                if (ec != std::errc())
                {
                    throw std::invalid_argument("Invalid integer format: " + String(str));
                }
                return result;
            }
            else if constexpr (FloatingType<T>)
            {
                try
                {
                    return static_cast<T>(std::stod(String(str)));
                }
                catch (const std::exception &e)
                {
                    throw std::invalid_argument("Invalid floating point format: " + String(str));
                }
            }
            else if constexpr (BooleanType<T>)
            {
                if (str == "TRUE" || str == "true" || str == "1")
                {
                    return true;
                }
                else if (str == "FALSE" || str == "false" || str == "0")
                {
                    return false;
                }
                else
                {
                    throw std::invalid_argument("Invalid boolean format: " + String(str));
                }
            }
            else if constexpr (StringType<T>)
            {

                if (str.size() >= 2 && str.front() == '\'' && str.back() == '\'')
                {
                    String result;
                    result.reserve(str.size() - 2);
                    for (Size i = 1; i < str.size() - 1; ++i)
                    {
                        if (str[i] == '\'' && i + 1 < str.size() - 1 && str[i + 1] == '\'')
                        {
                            result += '\'';
                            ++i;
                        }
                        else
                        {
                            result += str[i];
                        }
                    }
                    return result;
                }
                return String(str);
            }
            else if constexpr (same_as<T, Byte>)
            {

                if (str.size() >= 3 && str[0] == 'X' && str[1] == '\'')
                {
                    String hex_str(str.substr(2, str.size() - 3));
                    return Byte{static_cast<std::byte>(std::stoul(hex_str, nullptr, 16))};
                }
                throw std::invalid_argument("Invalid byte format: " + String(str));
            }
            else
            {
                static_assert(always_false<T>::value,
                              "Unsupported type for FromString conversion");
            }
        }
    };

    template <typename T>
    struct FromString<std::optional<T>>
    {
        static std::optional<T> parse(const StringView &str)
        {
            if (str == "NULL")
            {
                return std::nullopt;
            }
            else
            {
                return FromString<T>::parse(str);
            }
        }
    };

    template <typename T> // 完整数据库类型检查，确保类型满足映射、SQL 类型定义、字符串转换等所有必要条件
        struct IsCompleteDatabaseType : std::bool_constant <
                                        DatabaseMappable<T> &&requires
    {
        {sql_type_v<T>}->std::convertible_to<StringView>;
        requires requires(const T &value) {
            { ToString<T>::convert(value) } -> std::convertible_to<String>;
        };
        requires requires(const StringView &str) {
            { FromString<T>::parse(str) } -> std::convertible_to<T>;
        };
    }>{};

    template <typename T>
    constexpr Bool is_complete_database_type_v = IsCompleteDatabaseType<T>::value;

// 静态断言宏，确保类型满足特征要求和映射关系，提供编译期错误检查，增强类型安全性和库的健壮性
#define MINIORM_TRAIT_ASSERT(Type, Trait)     \
    MINIORM_STATIC_ASSERT(Trait<Type>::value, \
                          "Type " MINIORM_STRINGIFY(Type) " does not satisfy trait " MINIORM_STRINGIFY(Trait))

// 检查 SQL 类型映射是否存在，确保每个数据库可映射类型都有对应的 SQL 类型定义，防止遗漏和类型不匹配问题
#define MINIORM_CHECK_SQL_TYPE(Type)                                       \
    static_assert(!std::is_same_v<decltype(sql_type_v<Type>), StringView>, \
                  "Type " MINIORM_STRINGIFY(Type) " must have SQL type mapping")

    template <Bool Condition, typename TrueTrait, typename FalseTrait>
    struct TypeTraitSelector
    {
        using type = TrueTrait;
    };

    template <typename TrueTrait, typename FalseTrait>
    struct TypeTraitSelector<false, TrueTrait, FalseTrait>
    {
        using type = FalseTrait;
    };

    template <Bool Condition, typename TrueTrait, typename FalseTrait>
    using TypeTraitSelector_t = typename TypeTraitSelector<Condition, TrueTrait, FalseTrait>::type;

    template <template <typename> typename Trait, typename T>
    struct ApplyTrait
    {
        using type = typename Trait<T>::type;
    };

    template <template <typename> typename Trait, typename T>
    using ApplyTrait_t = typename ApplyTrait<Trait, T>::type;

// 编译期反射相关的静态断言，确保核心类型满足必要的特征和映射要求
#ifdef MINIORM_ENABLE_COMPILE_TIME_REFLECTION
    MINIORM_TRAIT_ASSERT(int32, IsPrimaryKey);
    MINIORM_TRAIT_ASSERT(String, IsPrimaryKey);

    MINIORM_CHECK_SQL_TYPE(int32);
    MINIORM_CHECK_SQL_TYPE(float64);
    MINIORM_CHECK_SQL_TYPE(String);

    static_assert(is_complete_database_type_v<int32>,
                  "int32 must be a complete database type");
    static_assert(is_complete_database_type_v<String>,
                  "String must be a complete database type");
#endif

}

#endif
