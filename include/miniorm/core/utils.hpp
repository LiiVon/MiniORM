// utils.hpp 是 MiniORM 项目的工具函数和实用程序库
// 提供了字符串处理、格式化、类型转换、容器操作、SQL 构建等通用功能

#ifndef MINIORM_CORE_UTILS_HPP
#define MINIORM_CORE_UTILS_HPP

#include "config.hpp"
#include "concepts.hpp"
#include "traits.hpp"

#if !MINIORM_CPP20
#error "utils.hpp requires C++20 support. Please enable C++20 in your compiler settings."
#endif

#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <optional>
#include <type_traits>
#include <utility>
#include <algorithm>
#include <cctype>
#include <charconv>
#include <ctime>
#include <stdexcept>
#include <memory>
#include <chrono>
#include <format>

#if MINIORM_ENABLE_LOGGING
#include <iostream>
#include <sstream>
#endif

namespace miniorm
{

    template <Size N>
    struct FixedString // 编译期字符串类型
    {
        std::array<char, N> value{};
        constexpr FixedString(const char (&str)[N])
        {
            // 编译期安全的拷贝
            std::copy_n(str, N, value.data());
        }

        constexpr Size size() const
        {
            return N - 1;
        }

        constexpr const char *data() const
        {
            return value.data();
        }
    };

    template <Size N> // CATD 自动推导参数N 使用简化
    FixedString(const char (&)[N]) -> FixedString<N>;

    template <FixedString... Strs> // 编译期字符串连接
    struct JoinString
    {
        static constexpr Size total_length = (Strs.size() + ... + 0);

        static constexpr std::array<char, total_length + 1> value = []()
        {
            std::array<char, total_length + 1> result{};
            Size pos = 0;

            ((std::copy_n(Strs.data(), Strs.size(), result.data() + pos),
              pos += Strs.size()),
             ...);

            result[total_length] = '\0';
            return result;
        }();
    };

    template <FixedString... Strs>
    constexpr auto join_string()
    {
        return JoinString<Strs...>::value;
    }

    template <typename T>
    constexpr StringView type_name()
    {
        return type_name_v<T>;
    }

    // SQL 字符串转义和标识符处理
    class SqlStringEscaper
    {
    public:
        // 转义 SQL 字符串中的特殊字符
        static String escape(const StringView &str)
        {
            String result;
            result.reserve(str.size() * 2);
            for (char c : str)
            {
                switch (c)
                {
                case '\'':
                    result += "''";
                    break;
                case '\\':
                    result += "\\\\";
                    break;
                case '\0':
                    result += "\\0";
                    break;
                case '\n':
                    result += "\\n";
                    break;
                case '\r':
                    result += "\\r";
                    break;
                case '\b':
                    result += "\\b";
                    break;
                case '\t':
                    result += "\\t";
                    break;
                default:
                    result += c;
                }
            }
            return result;
        }

        // 转义标识符
        static String escape_identifier(const StringView &identifier)
        {

            Bool needs_quotes = false;
            for (char c : identifier)
            {
                if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_')
                {
                    needs_quotes = true;
                    break;
                }
            }
            if (!needs_quotes)
            {
                return String(identifier);
            }

            String result{};
            result.reserve(identifier.size() + 2);
            result += '"';

            for (char c : identifier)
            {
                if (c == '"')
                {
                    result += "\"\"";
                }
                else
                {
                    result += c;
                }
            }

            result += '"';
            return result;
        }

        // 将字符串值转换为 SQL 字符串字面量，自动添加引号和转义
        static String quoted_value(const StringView &str)
        {
            return "'" + escape(str) + "'";
        }

        // 生成参数占位符，适用于预编译 SQL 语句
        static String parameter_placeholder(Size index)
        {
            return "?" + std::to_string(index + 1);
        }
    };

    // 通用的字符串格式化工具
    class StringFormatter
    {
    public:
        template <typename... Args>
        static String format(const StringView &fmt, Args &&...args)
        {
            String result;
            format_impl(result, fmt, std::forward<Args>(args)...);
            return result;
        }

        template <typename... Args>
        static String format_sql(const StringView &sql, Args &&...args)
        {
            String result(sql);
            format_sql_impl(result, std::forward<Args>(args)...);
            return result;
        }

        template <FixedString Fmt, typename... Args>
        static constexpr auto format_constexpr(Args &&...args)
        {
            return join_string<Fmt>();
        }

    private:
        template <typename T, typename... Args>
        static void format_impl(String &result, const StringView &fmt, T &&arg, Args &&...args)
        {
            Size pos = fmt.find("{}");
            if (pos == StringView::npos)
            {
                result += fmt;
                return;
            }

            result += fmt.substr(0, pos);
            // 格式化当前参数
            format_arg(result, std::forward<T>(arg));
            // 递归处理剩余
            format_impl(result, fmt.substr(pos + 2), std::forward<Args>(args)...);
        }

        static void format_impl(String &result, const StringView &fmt)
        {
            result += fmt;
        }

        template <typename T>
        static void format_arg(String &result, T &&arg)
        {
            // 字符串直接添加
            if constexpr (StringType<std::decay_t<T>>)
            {
                result += std::forward<T>(arg);
            }
            // 高性能数值转换
            else if constexpr (NumericType<std::decay_t<T>>)
            {

                char buffer[64];
                auto [ptr, ec] = std::to_chars(buffer, buffer + sizeof(buffer), arg);
                if (ec == std::errc())
                {
                    result.append(buffer, ptr);
                }
                else
                {
                    // 失败，回退普通版本
                    result += std::to_string(arg);
                }
            }
            else if constexpr (BooleanType<std::decay_t<T>>)
            {
                result += arg ? "true" : "false";
            }
            else if constexpr (DatabaseMappable<std::decay_t<T>>)
            {

                result += ToString<std::decay_t<T>>::convert(arg);
            }
            else
            {
                static_assert(always_false<std::decay_t<T>>::value,
                              "Unsupported type for string formatting");
            }
        }

        template <typename... Args>
        static void format_sql_impl(String &result, Args &&...args)
        {
            Size index = 0;
            (replace_placeholder(result, index++, std::forward<Args>(args)), ...);
        }

        template <typename T>
        static void replace_placeholder(String &sql, Size index, T &&arg)
        {
            Size pos = 0;
            Size cnt = 0;

            while ((pos = sql.find("{}", pos)) != String::npos)
            {
                if (cnt == index)
                {
                    String replacement;

                    if constexpr (StringType<std::decay_t<T>>)
                    {
                        replacement = SqlStringEscaper::quoted_value(std::forward<T>(arg));
                    }
                    else if constexpr (NumericType<std::decay_t<T>>)
                    {
                        char buffer[64];
                        auto [ptr, ec] = std::to_chars(buffer, buffer + sizeof(buffer), arg);
                        if (ec == std::errc())
                        {
                            replacement.append(buffer, ptr);
                        }
                        else
                        {
                            replacement = std::to_string(arg);
                        }
                    }
                    else if constexpr (BooleanType<std::decay_t<T>>)
                    {
                        replacement = arg ? "TRUE" : "FALSE";
                    }
                    else if constexpr (DatabaseMappable<std::decay_t<T>>)
                    {

                        replacement = ToString<std::decay_t<T>>::convert(arg);
                    }
                    else
                    {
                        static_assert(always_false<std::decay_t<T>>::value,
                                      "Unsupported type for SQL formatting");
                    }

                    sql.replace(pos, 2, replacement);
                    break;
                }

                pos += 2;
                ++cnt;
            }
        }
    };

    // 安全的类型转换，检查溢出和有效性
    template <typename To, typename From>
    constexpr To safe_cast(From &&value)
    {
        static_assert(std::is_convertible_v<From, To>,
                      "Types are not convertible");

        if constexpr (std::is_same_v<std::decay_t<From>, To>)
        {
            return std::forward<From>(value);
        }
        else
        {
            if constexpr (NumericType<std::decay_t<From>> && NumericType<To>)
            {
                if (static_cast<long double>(value) <
                        static_cast<long double>(std::numeric_limits<To>::lowest()) ||
                    static_cast<long double>(value) >
                        static_cast<long double>(std::numeric_limits<To>::max()))
                {
                    throw std::overflow_error("Numeric overflow in safe_cast");
                }
            }
            return static_cast<To>(std::forward<From>(value));
        }
    }

    template <NumericType To, NumericType From>
    constexpr To safe_numeric_cast(From value)
    {

        static_assert(std::is_convertible_v<From, To>,
                      "Numeric types are not convertible");

        if (static_cast<long double>(value) <
                static_cast<long double>(std::numeric_limits<To>::lowest()) ||
            static_cast<long double>(value) >
                static_cast<long double>(std::numeric_limits<To>::max()))
        {
            throw std::overflow_error("Numeric overflow in safe_numeric_cast");
        }

        return static_cast<To>(value);
    }

    template <typename T>
    class OptionalHelper
    {
    public:
        static T value_or(const std::optional<T> &opt, T &&default_value)
        {
            return opt.value_or(std::forward<T>(default_value));
        }

        template <typename U>
            requires DatabaseMappable<U>
        static std::optional<U> transform(const std::optional<T> &opt,
                                          std::function<U(const T &)> transformer_func)
        {
            if (opt.has_value())
            {
                return transformer_func(opt.value());
            }
            return std::nullopt;
        }

        template <typename U>
            requires DatabaseMappable<U>
        static std::optional<U> and_then(const std::optional<T> &opt,
                                         std::function<std::optional<U>(const T &)> func)
        {
            if (opt.has_value())
            {
                return func(opt.value());
            }
            return std::nullopt;
        }

        template <typename U>
            requires DatabaseMappable<U>
        static std::optional<U> safe_convert(const std::optional<String> &opt)
        {
            if (opt.has_value())
            {
                try
                {
                    return FromString<U>::parse(opt.value());
                }
                catch (...)
                {
                    return std::nullopt;
                }
            }
            return std::nullopt;
        }
    };

    // 编译期数组工具，提供查找、排序检查、类型转换等功能
    template <typename T, Size N, T value>
    constexpr Size find_index(const std::array<T, N> &arr)
    {
        for (Size i = 0; i < N; ++i)
        {
            if (arr[i] == value)
            {
                return i;
            }
        }
        return N;
    }

    template <typename T, Size N>
    constexpr Bool is_sorted(const std::array<T, N> &arr)
    {
        for (Size i = 1; i < N; ++i)
        {
            if (arr[i] < arr[i - 1])
            {
                return false;
            }
        }
        return true;
    }

    template <typename From, typename To, Size N>
        requires DatabaseMappable<From> && DatabaseMappable<To>
    constexpr std::array<To, N> transform_array(const std::array<From, N> &arr,
                                                std::function<To(const From &)> func)
    {
        std::array<To, N> result{};
        for (Size i = 0; i < N; ++i)
        {
            result[i] = func(arr[i]);
        }
        return result;
    }

    // 异常工厂，提供统一的异常创建接口，支持数据库错误、类型转换错误、SQL 语法错误等
    class ExceptionFactory
    {
    public:
        static std::runtime_error database_error(const StringView &operation,
                                                 const StringView &message,
                                                 int32 error_code = 0);
        template <typename From, typename To>

        // 类型转换错误，提供详细的错误信息，包括源值、源类型和目标类型
        static std::runtime_error type_conversion_error(const StringView &value)
        {
            return std::runtime_error(
                StringFormatter::format(
                    "Cannot convert value '{}' from {} to {}",
                    String(value), type_name_v<From>, type_name_v<To>));
        }

        // SQL 语法错误，提供错误的 SQL 语句和错误位置的详细信息
        static std::runtime_error sql_syntax_error(const StringView &sql,
                                                   const StringView &pos = "");

        // 参数错误，提供参数名称、参数值和预期类型的详细信息
        template <typename T>
        static std::runtime_error parameter_error(const StringView &param_name,
                                                  const T &param_value,
                                                  const StringView &expected_type)
        {
            return std::runtime_error(
                StringFormatter::format("Invalid parameter '{}' with value '{}', expected type: {}",
                                        param_name, ToString<T>::convert(param_value), expected_type));
        }
    };

// 日志记录器，提供不同级别的日志记录功能，支持格式化日志消息和 SQL 语句
#if MINIORM_ENABLE_LOGGING
    class Logger
    {
    public:
        enum class Level : int32
        {
            Debug = 0,
            Info = 1,
            Warning = 2,
            Error = 3,
            Critical = 4
        };

        static void set_level(Level level) noexcept
        {
            current_level = level;
        }

        static Level get_level() noexcept
        {
            return current_level;
        }

        template <typename... Args>
        static void log(Level level, const StringView &fmt, Args &&...args)
        {
            if (static_cast<int32>(level) >= static_cast<int32>(current_level))
            {
                String message = StringFormatter::format(fmt, std::forward<Args>(args)...);
                output_log(level, message);
            }
        }

        template <typename... Args>
        static void debug(const StringView &fmt, Args &&...args)
        {
            log(Level::Debug, fmt, std::forward<Args>(args)...);
        }

        template <typename... Args>
        static void info(const StringView &fmt, Args &&...args)
        {
            log(Level::Info, fmt, std::forward<Args>(args)...);
        }

        template <typename... Args>
        static void warning(const StringView &fmt, Args &&...args)
        {
            log(Level::Warning, fmt, std::forward<Args>(args)...);
        }

        template <typename... Args>
        static void error(const StringView &fmt, Args &&...args)
        {
            log(Level::Error, fmt, std::forward<Args>(args)...);
        }

        template <typename... Args>
        static void critical(const StringView &fmt, Args &&...args)
        {
            log(Level::Critical, fmt, std::forward<Args>(args)...);
        }

        template <typename... Args>
        static void sql_debug(const StringView &sql, Args &&...args)
        {
            if (static_cast<int32>(Level::Debug) >= static_cast<int32>(current_level))
            {
                String formatted_sql = StringFormatter::format_sql(sql, std::forward<Args>(args)...);
                log(Level::Debug, "SQL: {}", formatted_sql);
            }
        }

    private:
        static inline Level current_level = Level::Info;

        static void output_log(Level level, const String &msg);

        static StringView level_to_string(Level level);
    };
#else
// 无日志实现，所有日志函数都是空的
    class Logger
    {
    public:
        enum class Level : int32
        {
            Debug = 0,
            Info = 1,
            Warning = 2,
            Error = 3,
            Critical = 4
        };

        static void set_level(Level) noexcept {}
        static Level get_level() noexcept { return Level::Info; }

        template <typename... Args>
        static void log(Level, const StringView &, Args &&...) {}

        template <typename... Args>
        static void debug(const StringView &, Args &&...) {}

        template <typename... Args>
        static void info(const StringView &, Args &&...) {}

        template <typename... Args>
        static void warning(const StringView &, Args &&...) {}

        template <typename... Args>
        static void error(const StringView &, Args &&...) {}

        template <typename... Args>
        static void critical(const StringView &, Args &&...) {}

        template <typename... Args>
        static void sql_debug(const StringView &, Args &&...) {}
    };
#endif

    // 容器工具，提供常用的容器操作，如查找、连接、转换等，支持任何满足 SequenceContainer 概念的容器类型
    template <SequenceContainer Container>
    class ContainerUtils
    {
    public:
        using ValueType = typename Container::value_type;

        static auto find(Container &container, const ValueType &value)
        {
            return std::find(container.begin(), container.end(), value);
        }

        static Bool contains(const Container &container, const ValueType &value)
        {
            return std::find(container.begin(), container.end(), value) != container.end();
        }

        template <typename Delimiter>
        static String join(const Container &container, Delimiter &&delimiter)
        {
            if (container.empty())
            {
                return "";
            }

            String result;
            auto it = container.begin();

            result += ToString<ValueType>::convert(*it);
            ++it;

            for (; it != container.end(); ++it)
            {
                result += std::forward<Delimiter>(delimiter);
                result += ToString<ValueType>::convert(*it);
            }
            return result;
        }

        template <SequenceContainer TargetContainer, typename TransformFunc>
        static TargetContainer transform(const Container &container, TransformFunc &&func)
        {
            TargetContainer result;
            result.reserve(container.size());

            for (const auto &item : container)
            {
                result.push_back(func(item));
            }
            return result;
        }

        template <typename Predicate>
        static Container filter(const Container &container, Predicate &&pred)
        {
            Container result;
            result.reserve(container.size());

            for (const auto &item : container)
            {
                if (pred(item))
                {
                    result.push_back(item);
                }
            }
            return result;
        }

        static String to_sql_values(const Container &container)
        {
            if (container.empty())
            {
                return "()";
            }

            String result = "(";
            auto it = container.begin();

            result += ToString<ValueType>::convert(*it);
            ++it;

            for (; it != container.end(); ++it)
            {
                result += ", ";
                result += ToString<ValueType>::convert(*it);
            }

            result += ")";
            return result;
        }
    };

    // 安全检查
    template <typename T, template <typename> typename Concept>
    struct ConceptChecker
    {
        static_assert(Concept<T>::value,
                      "Type does not satisfy the required concept");
        using type = T;
    };

    template <typename T>
    struct TypeValidator
    {
        static_assert(is_complete_database_type_v<T>,
                      "Type must be a complete database type");

        static_assert(!std::is_same_v<decltype(sql_type_v<T>), StringView>,
                      "Type must have a valid SQL type mapping");

        static_assert(requires(const T &value) {
        { ToString<T>::convert(value) } -> std::convertible_to<String>; }, "Type must be convertible to String for SQL generation");

        using type = T;
    };

    template <DatabaseMappable T>
    struct DatabaseTypeValidator : TypeValidator<T>
    {

        static_assert(!is_nullable_v<T> || requires {
        { default_value<T>() } -> std::convertible_to<T>; }, "Nullable types must have a default value");
    };

// 编译期断言和条件表达式，提供更友好的错误信息和更灵活的条件处理
#define MINIORM_STATIC_ASSERT_MSG(expr, msg) \
    MINIORM_STATIC_ASSERT(expr, msg)

#define MINIORM_IF_CONSTEXPR(cond, true_expr, false_expr) \
    []() { \
        if constexpr (cond) { return true_expr; } \
        else { return false_expr; } }()

#define MINIORM_SAFE_CALL(ptr, func, ...) \
    ((ptr) != nullptr ? (ptr)->func(__VA_ARGS__) : decltype((ptr)->func(__VA_ARGS__)){})

#define MINIORM_CHECK_RANGE(value, min, max)                                       \
    do                                                                             \
    {                                                                              \
        if ((value) < (min) || (value) > (max))                                    \
        {                                                                          \
            throw std::out_of_range(                                               \
                StringFormatter::format("Value {} out of range [{}, {}]",          \
                                        ToString<decltype(value)>::convert(value), \
                                        ToString<decltype(min)>::convert(min),     \
                                        ToString<decltype(max)>::convert(max)));   \
        }                                                                          \
    } while (false)

#define MINIORM_CHECK_PARAM(condition, param_name, param_value)                                              \
    do                                                                                                       \
    {                                                                                                        \
        if (!(condition))                                                                                    \
        {                                                                                                    \
            throw std::invalid_argument(                                                                     \
                StringFormatter::format("Invalid parameter '{}' with value '{}'",                            \
                                        param_name, ToString<decltype(param_value)>::convert(param_value))); \
        }                                                                                                    \
    } while (false)

    // 编译期字符串处理函数，提供哈希计算、字符串比较、长度计算等功能
    constexpr Size constexpr_hash(const char *str, Size hash = 5381)
    {
        return (*str == '\0') ? hash : constexpr_hash(str + 1, hash * 33 ^ static_cast<Size>(*str));
    }

    constexpr Bool constexpr_strcmp(const char *a, const char *b)
    {
        return (*a == *b) ? (*a == '\0' || constexpr_strcmp(a + 1, b + 1)) : false;
    }

    constexpr Size constexpr_strlen(const char *str)
    {
        Size len = 0;
        while (str[len] != '\0')
            ++len;
        return len;
    }

    // 字符串缓存，提供编译期字符串常量的缓存和复用，减少运行时内存分配和复制
    class StringCache
    {
    public:
        static StringView get_cached(const StringView &str);

        static void clear_cache();
    };

    // 内存池，提供高效的内存分配和回收机制
    template <typename T, Size BlockSize = 1024>
    class MemoryPool
    {
    public:
        MemoryPool() : current_block(nullptr), current_pos(0) {}

        MINIORM_DISABLE_COPY(MemoryPool);

        MemoryPool(MemoryPool &&other) noexcept
            : blocks(std::move(other.blocks)), current_block(other.current_block), current_pos(other.current_pos)
        {
            other.current_block = nullptr;
            other.current_pos = 0;
        }

        ~MemoryPool()
        {
            for (auto block : blocks)
            {
                ::operator delete(block);
            }
        }

        T *allocate()
        {
            if (!current_block || current_pos >= BlockSize)
            {
                allocate_block();
            }

            T *ptr = reinterpret_cast<T *>(current_block + current_pos);
            current_pos += sizeof(T);
            return ptr;
        }

        void deallocate(T *, Size = 1) noexcept
        {
        }

        Size allocated_blocks() const { return blocks.size(); }
        Size current_position() const { return current_pos; }

    private:
        void allocate_block()
        {
            current_block = reinterpret_cast<char *>(::operator new(BlockSize * sizeof(T)));
            blocks.push_back(current_block);
            current_pos = 0;
        }

        std::vector<char *> blocks;
        char *current_block;
        Size current_pos;
    };

    // SQL 构建器，提供动态构建 SQL 语句的功能，支持 SELECT、INSERT、UPDATE、DELETE 等常用语句类型
    class SqlBuilder
    {
    public:
        template <SequenceContainer Container>
        static String build_select(const StringView &table_name,
                                   const Container &columns,
                                   const StringView &where_clause = "",
                                   const StringView &order_by = "",
                                   Size limit = 0,
                                   Size offset = 0)
        {
            String sql = "SELECT ";

            if (columns.empty())
            {
                sql += "*";
            }
            else
            {
                sql += ContainerUtils<Container>::join(columns, ", ");
            }

            sql += " FROM " + SqlStringEscaper::escape_identifier(table_name);

            if (!where_clause.empty())
            {
                sql += " WHERE " + String(where_clause);
            }

            if (!order_by.empty())
            {
                sql += " ORDER BY " + String(order_by);
            }

            if (limit > 0)
            {
                sql += " LIMIT " + std::to_string(limit);
                if (offset > 0)
                {
                    sql += " OFFSET " + std::to_string(offset);
                }
            }

            return sql;
        }

        template <SequenceContainer Container>
        static String build_insert(const StringView &table_name,
                                   const Container &columns,
                                   const Container &values)
        {
            MINIORM_ASSERT(columns.size() == values.size(),
                           "Number of columns must match number of values");

            String sql = "INSERT INTO " + SqlStringEscaper::escape_identifier(table_name);
            sql += " (" + ContainerUtils<Container>::join(columns, ", ") + ")";
            sql += " VALUES (" + ContainerUtils<Container>::join(values, ", ") + ")";

            return sql;
        }

        template <SequenceContainer Container>
        static String build_update(const StringView &table_name,
                                   const Container &set_clauses,
                                   const StringView &where_clause = "")
        {
            String sql = "UPDATE " + SqlStringEscaper::escape_identifier(table_name);
            sql += " SET " + ContainerUtils<Container>::join(set_clauses, ", ");

            if (!where_clause.empty())
            {
                sql += " WHERE " + String(where_clause);
            }

            return sql;
        }

        static String build_delete(const StringView &table_name,
                                   const StringView &where_clause = "")
        {
            String sql = "DELETE FROM " + SqlStringEscaper::escape_identifier(table_name);

            if (!where_clause.empty())
            {
                sql += " WHERE " + String(where_clause);
            }

            return sql;
        }
    };

}

#endif
