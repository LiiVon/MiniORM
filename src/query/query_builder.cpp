#include "../../include/miniorm/query/query_builder.hpp"

namespace miniorm
{
    QueryBuilder::QueryBuilder()
        : type_(QueryType::Unknown), limit_(std::nullopt), offset_(std::nullopt)
    {
    }

    QueryBuilder &QueryBuilder::reset()
    {
        type_ = QueryType::Unknown;
        table_name_.clear();
        select_columns_.clear();
        insert_columns_.clear();
        insert_values_.clear();
        update_assignments_.clear();
        where_condition_.reset();
        order_by_.clear();
        limit_.reset();
        offset_.reset();
        return *this;
    }

    QueryBuilder &QueryBuilder::select(std::initializer_list<StringView> columns)
    {
        reset();
        type_ = QueryType::Select;
        select_columns_.reserve(columns.size());
        for (auto column : columns)
        {
            select_columns_.emplace_back(column);
        }
        return *this;
    }

    QueryBuilder &QueryBuilder::insert_into(StringView table_name)
    {
        reset();
        type_ = QueryType::Insert;
        table_name_ = String(table_name);
        return *this;
    }

    QueryBuilder &QueryBuilder::update(StringView table_name)
    {
        reset();
        type_ = QueryType::Update;
        table_name_ = String(table_name);
        return *this;
    }

    QueryBuilder &QueryBuilder::delete_from(StringView table_name)
    {
        reset();
        type_ = QueryType::Delete;
        table_name_ = String(table_name);
        return *this;
    }

    QueryBuilder &QueryBuilder::from(StringView table_name)
    {
        table_name_ = String(table_name);
        return *this;
    }

    QueryBuilder &QueryBuilder::where(const Condition &condition)
    {
        where_condition_ = condition;
        return *this;
    }

    QueryBuilder &QueryBuilder::order_by(StringView column_name, Bool ascending)
    {
        order_by_.emplace_back(String(column_name), ascending);
        return *this;
    }

    QueryBuilder &QueryBuilder::limit(Size value)
    {
        limit_ = value;
        return *this;
    }

    QueryBuilder &QueryBuilder::offset(Size value)
    {
        offset_ = value;
        return *this;
    }

    String QueryBuilder::build() const
    {
        switch (type_)
        {
        case QueryType::Select:
            return build_select();
        case QueryType::Insert:
            return build_insert();
        case QueryType::Update:
            return build_update();
        case QueryType::Delete:
            return build_delete();
        default:
            return "";
        }
    }

    const std::vector<String> &QueryBuilder::parameters() const noexcept
    {
        if (where_condition_)
        {
            return where_condition_->parameters();
        }
        static const std::vector<String> empty_parameters;
        return empty_parameters;
    }

    QueryBuilder::QueryType QueryBuilder::type() const noexcept
    {
        return type_;
    }

    String QueryBuilder::build_select() const
    {
        String sql = "SELECT ";
        if (select_columns_.empty())
        {
            sql += "*";
        }
        else
        {
            for (Size i = 0; i < select_columns_.size(); ++i)
            {
                if (i > 0)
                {
                    sql += ", ";
                }
                sql += select_columns_[i];
            }
        }

        sql += " FROM ";
        sql += SqlStringEscaper::escape_identifier(table_name_);
        append_where_order_limit(sql);
        return sql;
    }

    String QueryBuilder::build_insert() const
    {
        MINIORM_ASSERT(!table_name_.empty(), "INSERT query requires a table name");
        MINIORM_ASSERT(insert_columns_.size() == insert_values_.size(), "INSERT columns and values count must match");

        String sql = "INSERT INTO ";
        sql += SqlStringEscaper::escape_identifier(table_name_);
        sql += " (";
        for (Size i = 0; i < insert_columns_.size(); ++i)
        {
            if (i > 0)
            {
                sql += ", ";
            }
            sql += insert_columns_[i];
        }
        sql += ") VALUES (";
        for (Size i = 0; i < insert_values_.size(); ++i)
        {
            if (i > 0)
            {
                sql += ", ";
            }
            sql += insert_values_[i];
        }
        sql += ")";
        return sql;
    }

    String QueryBuilder::build_update() const
    {
        MINIORM_ASSERT(!table_name_.empty(), "UPDATE query requires a table name");
        MINIORM_ASSERT(!update_assignments_.empty(), "UPDATE query requires at least one assignment");

        String sql = "UPDATE ";
        sql += SqlStringEscaper::escape_identifier(table_name_);
        sql += " SET ";

        for (Size i = 0; i < update_assignments_.size(); ++i)
        {
            if (i > 0)
            {
                sql += ", ";
            }
            sql += SqlStringEscaper::escape_identifier(update_assignments_[i].first);
            sql += " = ";
            sql += update_assignments_[i].second;
        }

        append_where_order_limit(sql);
        return sql;
    }

    String QueryBuilder::build_delete() const
    {
        MINIORM_ASSERT(!table_name_.empty(), "DELETE query requires a table name");

        String sql = "DELETE FROM ";
        sql += SqlStringEscaper::escape_identifier(table_name_);
        append_where_order_limit(sql);
        return sql;
    }

    void QueryBuilder::append_where_order_limit(String &sql) const
    {
        if (where_condition_ && !where_condition_->empty())
        {
            sql += " WHERE ";
            sql += where_condition_->to_sql();
        }

        if (!order_by_.empty())
        {
            sql += " ORDER BY ";
            for (Size i = 0; i < order_by_.size(); ++i)
            {
                if (i > 0)
                {
                    sql += ", ";
                }
                sql += SqlStringEscaper::escape_identifier(order_by_[i].first);
                sql += order_by_[i].second ? " ASC" : " DESC";
            }
        }

        if (limit_)
        {
            sql += StringFormatter::format(" LIMIT {}", *limit_);
            if (offset_)
            {
                sql += StringFormatter::format(" OFFSET {}", *offset_);
            }
        }
    }
}
