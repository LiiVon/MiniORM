// condition.hpp - MiniORM 查询条件构建模块
// 提供灵活的查询条件构建工具，支持各种比较、逻辑运算符、参数绑定等功能，简化复杂查询的构建过程。

#ifndef MINIORM_QUERY_CONDITION_HPP
#define MINIORM_QUERY_CONDITION_HPP

#include "../core/config.hpp"
#include "../core/concepts.hpp"
#include "../core/traits.hpp"
#include "../core/utils.hpp"

#if !MINIORM_CPP20
#error "condition.hpp requires C++20 support. Please enable C++20 in your compiler settings."
#endif

#include <vector>
#include <utility>

namespace miniorm
{
    // 表示一个 SQL 查询条件，包含条件字符串和参数列表，支持 AND/OR/NOT 组合
    class Condition
    {
    public:
        Condition() = default;

        explicit Condition(String sql, std::vector<String> parameters = {})
            : sql_(std::move(sql)), parameters_(std::move(parameters))
        {
        }

        const String &to_sql() const noexcept
        {
            return sql_;
        }

        const std::vector<String> &parameters() const noexcept
        {
            return parameters_;
        }

        Bool empty() const noexcept
        {
            return sql_.empty();
        }

        // 将两个条件用 AND/OR/NOT 组合，并合并参数列表

        Condition operator&&(const Condition &other) const
        {
            if (empty())
            {
                return other;
            }
            if (other.empty())
            {
                return *this;
            }

            std::vector<String> merged = parameters_;
            merged.insert(merged.end(), other.parameters_.begin(), other.parameters_.end());
            return Condition(StringFormatter::format("({}) AND ({})", sql_, other.sql_), std::move(merged));
        }

        Condition operator||(const Condition &other) const
        {
            if (empty())
            {
                return other;
            }
            if (other.empty())
            {
                return *this;
            }

            std::vector<String> merged = parameters_;
            merged.insert(merged.end(), other.parameters_.begin(), other.parameters_.end());
            return Condition(StringFormatter::format("({}) OR ({})", sql_, other.sql_), std::move(merged));
        }

        Condition operator!() const
        {
            if (empty())
            {
                return *this;
            }
            return Condition(StringFormatter::format("NOT ({})", sql_), parameters_);
        }

    private:
        String sql_;
        std::vector<String> parameters_;
    };

    namespace detail
    {
        template <typename T>
        String literal_to_sql(T &&value)
        {
            using ValueType = std::decay_t<T>;

            // 将字面量值转为安全的 SQL 文字（含引号/转义），支持字符串、bool、以及可映射类型
            if constexpr (std::is_same_v<ValueType, String>)
            {
                return SqlStringEscaper::quoted_value(value);
            }
            else if constexpr (std::is_same_v<ValueType, StringView>)
            {
                return SqlStringEscaper::quoted_value(value);
            }
            else if constexpr (std::is_same_v<ValueType, const char *> || std::is_same_v<ValueType, char *>)
            {
                return SqlStringEscaper::quoted_value(value);
            }
            else if constexpr (BooleanType<ValueType>)
            {
                return value ? "TRUE" : "FALSE";
            }
            else if constexpr (DatabaseMappable<ValueType>)
            {
                return ToString<ValueType>::convert(std::forward<T>(value));
            }
            else
            {
                static_assert(always_false<ValueType>::value, "Unsupported value type for SQL literal generation");
            }
        }
    }

    class QueryFieldExpression
    {
    public:
        explicit QueryFieldExpression(StringView field_name)
            : field_name_(field_name)
        {
        }

        StringView field_name() const noexcept
        {
            return field_name_;
        }

        // 为字段构建常见比较操作（=, <>, >, >=, <, <=, LIKE），返回 `Condition` 对象
        template <typename T>
            requires DatabaseMappable<std::decay_t<T>>
        Condition eq(T &&value) const
        {
            String literal = detail::literal_to_sql(std::forward<T>(value));
            return Condition(StringFormatter::format("{} = {}", field_name_, literal), {literal});
        }

        template <typename T>
            requires DatabaseMappable<std::decay_t<T>>
        Condition ne(T &&value) const
        {
            String literal = detail::literal_to_sql(std::forward<T>(value));
            return Condition(StringFormatter::format("{} <> {}", field_name_, literal), {literal});
        }

        template <typename T>
            requires DatabaseMappable<std::decay_t<T>>
        Condition gt(T &&value) const
        {
            String literal = detail::literal_to_sql(std::forward<T>(value));
            return Condition(StringFormatter::format("{} > {}", field_name_, literal), {literal});
        }

        template <typename T>
            requires DatabaseMappable<std::decay_t<T>>
        Condition ge(T &&value) const
        {
            String literal = detail::literal_to_sql(std::forward<T>(value));
            return Condition(StringFormatter::format("{} >= {}", field_name_, literal), {literal});
        }

        template <typename T>
            requires DatabaseMappable<std::decay_t<T>>
        Condition lt(T &&value) const
        {
            String literal = detail::literal_to_sql(std::forward<T>(value));
            return Condition(StringFormatter::format("{} < {}", field_name_, literal), {literal});
        }

        template <typename T>
            requires DatabaseMappable<std::decay_t<T>>
        Condition le(T &&value) const
        {
            String literal = detail::literal_to_sql(std::forward<T>(value));
            return Condition(StringFormatter::format("{} <= {}", field_name_, literal), {literal});
        }

        template <typename T>
            requires DatabaseMappable<std::decay_t<T>>
        Condition like(T &&value) const
        {
            String literal = detail::literal_to_sql(std::forward<T>(value));
            return Condition(StringFormatter::format("{} LIKE {}", field_name_, literal), {literal});
        }

        // 空值判断构造器：IS NULL / IS NOT NULL
        Condition is_null() const
        {
            return Condition(StringFormatter::format("{} IS NULL", field_name_));
        }

        Condition is_not_null() const
        {
            return Condition(StringFormatter::format("{} IS NOT NULL", field_name_));
        }

    private:
        StringView field_name_;
    };

    inline QueryFieldExpression condition(StringView field_name)
    {
        return QueryFieldExpression(field_name);
    }

    inline QueryFieldExpression field(StringView field_name)
    {
        return QueryFieldExpression(field_name);
    }
}

#endif 
