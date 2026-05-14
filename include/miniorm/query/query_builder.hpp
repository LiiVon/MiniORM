

#ifndef MINIORM_QUERY_QUERY_BUILDER_HPP
#define MINIORM_QUERY_QUERY_BUILDER_HPP

// query_builder.hpp - 查询构建器模块
// 提供链式 API 构建 SELECT/INSERT/UPDATE/DELETE 语句，支持条件、排序、分页与参数绑定

#include "condition.hpp"

#if !MINIORM_CPP20
#error "query_builder.hpp requires C++20 support. Please enable C++20 in your compiler settings."
#endif

#include <initializer_list>
#include <utility>
#include <vector>
#include <optional>

namespace miniorm
{
    // 链式查询构建器，维护查询状态并生成最终 SQL
    // 支持 select/insert/update/delete 模式的构建与参数收集
    class QueryBuilder
    {
    public:
        enum class QueryType : int32
        {
            Unknown = 0,
            Select,
            Insert,
            Update,
            Delete
        };

        QueryBuilder();

        QueryBuilder &reset();
        QueryBuilder &select(std::initializer_list<StringView> columns = {});
        QueryBuilder &insert_into(StringView table_name);
        QueryBuilder &update(StringView table_name);
        QueryBuilder &delete_from(StringView table_name);
        QueryBuilder &from(StringView table_name);
        QueryBuilder &where(const Condition &condition);
        QueryBuilder &order_by(StringView column_name, Bool ascending = true);
        QueryBuilder &limit(Size value);
        QueryBuilder &offset(Size value);

        template <typename T>
            requires DatabaseMappable<std::decay_t<T>>
        QueryBuilder &value(StringView column_name, T &&value)
        {
            insert_columns_.push_back(String(column_name));
            insert_values_.push_back(detail::literal_to_sql(std::forward<T>(value)));
            return *this;
        }

        QueryBuilder &value_raw(StringView column_name, StringView literal)
        {
            insert_columns_.push_back(String(column_name));
            insert_values_.push_back(String(literal));
            return *this;
        }

        template <typename T>
            requires DatabaseMappable<std::decay_t<T>>
        QueryBuilder &set(StringView column_name, T &&value)
        {
            update_assignments_.emplace_back(String(column_name), detail::literal_to_sql(std::forward<T>(value)));
            return *this;
        }

        QueryBuilder &set_raw(StringView column_name, StringView literal)
        {
            update_assignments_.emplace_back(String(column_name), String(literal));
            return *this;
        }

        String build() const;
        const std::vector<String> &parameters() const noexcept;
        QueryType type() const noexcept;

    private:
        String build_select() const;
        String build_insert() const;
        String build_update() const;
        String build_delete() const;
        void append_where_order_limit(String &sql) const;

    private:
        QueryType type_;
        String table_name_;
        std::vector<String> select_columns_;
        std::vector<String> insert_columns_;
        std::vector<String> insert_values_;
        std::vector<std::pair<String, String>> update_assignments_;
        std::optional<Condition> where_condition_;
        std::vector<std::pair<String, Bool>> order_by_;
        std::optional<Size> limit_;
        std::optional<Size> offset_;
    };
}

#endif 
