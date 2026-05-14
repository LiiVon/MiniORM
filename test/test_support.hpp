// test_support.hpp - MiniORM 测试辅助对象
// 提供可复用的 fake 结果集、语句和数据库连接
// ===============================================================

#ifndef MINIORM_TEST_SUPPORT_HPP
#define MINIORM_TEST_SUPPORT_HPP

#include "../include/miniorm/adapter/adapter.hpp"

#include <utility>
#include <vector>

namespace miniorm
{
    class TestResultRow : public IResultRow
    {
    public:
        TestResultRow(std::vector<String> values, std::vector<Bool> null_flags, std::vector<String> names)
            : values_(std::move(values)), null_flags_(std::move(null_flags)), names_(std::move(names))
        {
        }

        Size column_count() const override { return values_.size(); }
        String column_name(Size index) const override { return names_.at(index); }

        Size column_index(const StringView &name) const override
        {
            for (Size i = 0; i < names_.size(); ++i)
            {
                if (names_[i] == name)
                {
                    return i;
                }
            }
            return names_.size();
        }

        Bool is_null(Size index) const override { return null_flags_.at(index); }
        Bool is_null(const StringView &name) const override { return is_null(column_index(name)); }

    protected:
        String get_string(Size index) const override { return values_.at(index); }

    private:
        std::vector<String> values_;
        std::vector<Bool> null_flags_;
        std::vector<String> names_;
    };

    class TestResultSet : public IResultSet
    {
    public:
        TestResultSet(std::vector<std::vector<String>> rows,
                      std::vector<String> column_names,
                      Size cursor = 0)
            : rows_(std::move(rows)), column_names_(std::move(column_names)), cursor_(cursor)
        {
        }

        Size row_count() const override { return rows_.size(); }
        Size column_count() const override { return column_names_.size(); }
        String column_name(Size index) const override { return column_names_.at(index); }

        Bool next() override
        {
            if (cursor_ < rows_.size())
            {
                ++cursor_;
                return true;
            }
            return false;
        }

        const IResultRow &current_row() const override
        {
            current_row_cache_ = TestResultRow(rows_.at(cursor_ - 1),
                                               std::vector<Bool>(column_names_.size(), false),
                                               column_names_);
            return current_row_cache_;
        }

    private:
        std::unique_ptr<IResultSet> clone() const override
        {
            return std::make_unique<TestResultSet>(rows_, column_names_, cursor_);
        }

    private:
        std::vector<std::vector<String>> rows_;
        std::vector<String> column_names_;
        Size cursor_;
        mutable TestResultRow current_row_cache_{std::vector<String>{}, std::vector<Bool>{}, std::vector<String>{}};
    };

    class TestStatement : public IStatement
    {
    public:
        const String sql() const override { return sql_; }

        void bind_parameters(Size index, const StringView &params) override
        {
            last_bind_index_ = index;
            last_bind_value_ = String(params);
        }

        void bind_null(Size index) override
        {
            last_bind_index_ = index;
            last_bind_value_ = "NULL";
        }

        std::unique_ptr<IResultSet> execute_query() override
        {
            return std::make_unique<TestResultSet>(std::vector<std::vector<String>>{{"1", "Alice"}},
                                                   std::vector<String>{"id", "name"});
        }

        int32 execute_update() override { return 1; }
        void reset() override
        {
            last_bind_index_ = 0;
            last_bind_value_.clear();
        }
        void clear_bindings() override { reset(); }

        void set_sql(String sql) { sql_ = std::move(sql); }

        Size last_bind_index() const { return last_bind_index_; }
        String last_bind_value() const { return last_bind_value_; }

    private:
        String sql_;
        Size last_bind_index_ = 0;
        String last_bind_value_;
    };

    class TestConnection : public IDatabaseConnection
    {
    public:
        Bool connect(const DatabaseConfig &config) override
        {
            connected_ = true;
            last_config_ = config;
            return true;
        }

        void disconnect() override { connected_ = false; }
        Bool is_connected() const override { return connected_; }
        ConnectionState state() const override { return connected_ ? ConnectionState::Connected : ConnectionState::Disconnected; }

        Bool begin_transaction() override
        {
            in_transaction_ = true;
            begin_transaction_calls_++;
            return true;
        }

        Bool commit() override
        {
            in_transaction_ = false;
            commit_calls_++;
            return true;
        }

        Bool rollback() override
        {
            in_transaction_ = false;
            rollback_calls_++;
            return true;
        }

        Bool is_in_transaction() const override { return in_transaction_; }

        std::unique_ptr<IStatement> prepare_statement(const StringView &sql) override
        {
            auto stmt = std::make_unique<TestStatement>();
            stmt->set_sql(String(sql));
            return stmt;
        }

        std::unique_ptr<IResultSet> execute_query(const StringView &sql) override
        {
            last_query_ = String(sql);
            return std::make_unique<TestResultSet>(query_rows_, query_columns_);
        }

        int32 execute_update(const StringView &sql) override
        {
            last_update_ = String(sql);
            return update_result_;
        }

        int32 execute_batch(const std::vector<StringView> &sql_statements) override
        {
            last_batch_.clear();
            for (const auto &sql : sql_statements)
            {
                last_batch_.emplace_back(sql);
            }
            return static_cast<int32>(sql_statements.size());
        }

        DatabaseType database_type() const override { return DatabaseType::SQLite; }
        String database_version() const override { return "3.0-test"; }
        std::vector<String> get_table_names() override { return {"users", "orders"}; }
        std::vector<String> get_column_names(const StringView &table_name) override
        {
            if (table_name == "users")
            {
                return {"id", "name"};
            }
            return {};
        }

        int64 last_insert_id() const override { return last_insert_id_; }
        int32 affected_rows() const override { return update_result_; }
        String last_error() const override { return last_error_; }
        int32 last_error_code() const override { return last_error_code_; }

        void set_query_result(std::vector<std::vector<String>> rows, std::vector<String> columns)
        {
            query_rows_ = std::move(rows);
            query_columns_ = std::move(columns);
        }

        void set_update_result(int32 result)
        {
            update_result_ = result;
        }

        const String &last_query() const { return last_query_; }
        const String &last_update() const { return last_update_; }
        const std::vector<String> &last_batch() const { return last_batch_; }
        int32 begin_transaction_calls() const { return begin_transaction_calls_; }
        int32 commit_calls() const { return commit_calls_; }
        int32 rollback_calls() const { return rollback_calls_; }

    private:
        Bool connected_ = false;
        Bool in_transaction_ = false;
        int32 begin_transaction_calls_ = 0;
        int32 commit_calls_ = 0;
        int32 rollback_calls_ = 0;
        int32 update_result_ = 1;
        int64 last_insert_id_ = 1;
        int32 last_error_code_ = 0;
        String last_error_;
        String last_query_;
        String last_update_;
        std::vector<String> last_batch_;
        DatabaseConfig last_config_;
        std::vector<std::vector<String>> query_rows_{{"1", "Alice"}};
        std::vector<String> query_columns_{"id", "name"};
    };
}

#endif // MINIORM_TEST_SUPPORT_HPP
