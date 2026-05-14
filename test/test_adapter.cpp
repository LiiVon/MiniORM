// test_adapter.cpp - MiniORM 数据库适配器系统测试
// 测试 adapter.hpp 中定义的配置、结果集接口、参数接口和数据库操作辅助类

#include "../include/miniorm/core/config.hpp"
#include "../include/miniorm/core/concepts.hpp"
#include "../include/miniorm/core/traits.hpp"
#include "../include/miniorm/core/utils.hpp"
#include "../include/miniorm/adapter/adapter.hpp"

#include <cassert>
#include <chrono>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace miniorm
{

    // ==================== 测试用的轻量数据类型 ====================

    struct UserRecord
    {
        int32 id;
        String name;

        bool operator==(const UserRecord &other) const
        {
            return id == other.id && name == other.name;
        }
    };

    struct UserEntity
    {
        int32 id_;
        String name_;

        static StringView table_name() { return "users"; }
        static StringView primary_key_name() { return "id"; }
        using PrimaryKeyType = int32;

        bool operator==(const UserEntity &other) const
        {
            return id_ == other.id_ && name_ == other.name_;
        }
    };

    // ==================== 结果行 / 结果集 fake 对象 ====================

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

        Bool is_null(Size index) const override
        {
            return null_flags_.at(index);
        }

        Bool is_null(const StringView &name) const override
        {
            return is_null(column_index(name));
        }

    protected:
        String get_string(Size index) const override
        {
            return values_.at(index);
        }

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

    // ==================== SQL 语句 fake 对象 ====================

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

    // ==================== 数据库连接 fake 对象 ====================

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

        int64 last_insert_id() const override { return 42; }
        int32 affected_rows() const override { return update_result_; }
        String last_error() const override { return last_error_; }
        int32 last_error_code() const override { return last_error_code_; }

        void set_query_rows(std::vector<std::vector<String>> rows, std::vector<String> columns)
        {
            query_rows_ = std::move(rows);
            query_columns_ = std::move(columns);
        }

        void set_update_result(int32 result) { update_result_ = result; }

        Size begin_transaction_calls() const { return begin_transaction_calls_; }
        Size commit_calls() const { return commit_calls_; }
        Size rollback_calls() const { return rollback_calls_; }
        String last_query() const { return last_query_; }
        String last_update() const { return last_update_; }
        std::vector<String> last_batch() const { return last_batch_; }

    private:
        Bool connected_ = false;
        Bool in_transaction_ = false;
        DatabaseConfig last_config_{};

        std::vector<std::vector<String>> query_rows_{{"1", "Alice"}, {"2", "Bob"}};
        std::vector<String> query_columns_{"id", "name"};
        int32 update_result_ = 1;

        Size begin_transaction_calls_ = 0;
        Size commit_calls_ = 0;
        Size rollback_calls_ = 0;

        String last_query_;
        String last_update_;
        std::vector<String> last_batch_;

        String last_error_;
        int32 last_error_code_ = 0;
    };

    // ==================== 测试函数 ====================

    void test_database_type_and_config()
    {
        std::cout << "\n=== 测试数据库类型与配置 ===" << std::endl;

        std::cout << "1. DatabaseType 名称映射:" << std::endl;
        std::cout << "   SQLite -> " << database_type_name(DatabaseType::SQLite) << std::endl;
        std::cout << "   MySQL -> " << database_type_name(DatabaseType::MySQL) << std::endl;
        std::cout << "   Unknown -> " << database_type_name(DatabaseType::Unknown) << std::endl;

        static_assert(database_type_name(DatabaseType::SQLite) == "SQLite", "database type name mismatch");

        DatabaseConfig sqlite_config;
        sqlite_config.type = DatabaseType::SQLite;
        sqlite_config.database = "miniorm.db";

        DatabaseConfig mysql_config;
        mysql_config.type = DatabaseType::MySQL;
        mysql_config.host = "127.0.0.1";
        mysql_config.port = 3306;
        mysql_config.database = "miniorm";
        mysql_config.username = "root";
        mysql_config.password = "123456";
        mysql_config.options = "charset=utf8mb4";

        std::cout << "2. SQLite validate: " << sqlite_config.validate() << std::endl;
        std::cout << "3. SQLite connection_string: " << sqlite_config.connection_string() << std::endl;
        std::cout << "4. MySQL validate: " << mysql_config.validate() << std::endl;
        std::cout << "5. MySQL connection_string: " << mysql_config.connection_string() << std::endl;

        assert(sqlite_config.validate());
        assert(mysql_config.validate());
        assert(sqlite_config.connection_string() == "sqlite://miniorm.db");
        assert(mysql_config.connection_string().find("mysql://root:123456@127.0.0.1:3306/miniorm?charset=utf8mb4") != String::npos);
    }

    void test_database_exception()
    {
        std::cout << "\n=== 测试数据库异常 ===" << std::endl;

        DatabaseException ex("query failed", 1001, "SELECT * FROM users");
        std::cout << "1. what: " << ex.what() << std::endl;
        std::cout << "2. detailed_message: " << ex.detailed_message() << std::endl;
        std::cout << "3. error_code: " << ex.error_code() << std::endl;
        std::cout << "4. sql: " << ex.sql() << std::endl;

        assert(String(ex.what()) == "query failed");
        assert(ex.error_code() == 1001);
        assert(ex.sql() == "SELECT * FROM users");
        assert(ex.detailed_message().find("Database error:") != String::npos);
    }

    void test_result_row_and_set()
    {
        std::cout << "\n=== 测试结果行与结果集接口 ===" << std::endl;

        TestResultRow row({"1", "Alice"}, {false, false}, {"id", "name"});
        std::cout << "1. column_count: " << row.column_count() << std::endl;
        std::cout << "2. column_name(1): " << row.column_name(1) << std::endl;
        std::cout << "3. get<int32>(0): " << row.get<int32>(0) << std::endl;
        std::cout << "4. get_optional<String>(1): " << row.get_optional<String>(1).value() << std::endl;

        assert(row.column_count() == 2);
        assert(row.column_index("name") == 1);
        assert(row.get<int32>(0) == 1);
        assert(row.get_optional<String>(1).has_value());

        TestResultSet set({{"1", "Alice"}, {"2", "Bob"}}, {"id", "name"});

        auto all = set.fetch_all<>();
        std::cout << "5. fetch_all size: " << all.size() << std::endl;
        assert(all.size() == 2);
        assert(all[0][1] == "Alice");

        auto first = set.fetch_first<int32>();
        std::cout << "6. fetch_first<int32>: " << first.value() << std::endl;
        assert(first.has_value() && first.value() == 1);

        auto column = set.fetch_column<int32>(0);
        std::cout << "7. fetch_column<int32>(0) size: " << column.size() << std::endl;
        assert(column.size() == 2);
        assert(column[1] == 2);

        auto mapped = set.map<UserRecord>([](const IResultRow &r)
                                          { return UserRecord{r.get<int32>(0), r.get<String>(1)}; });
        std::cout << "8. map<UserRecord> size: " << mapped.size() << std::endl;
        assert(mapped.size() == 2);
        assert(mapped[0].id == 1 && mapped[0].name == "Alice");
    }

    void test_parameter_and_statement()
    {
        std::cout << "\n=== 测试参数与语句接口 ===" << std::endl;

        TypedDatabaseParameter<int32> p1(1, 42);
        TypedDatabaseParameter<String> p2(2);

        std::cout << "1. p1 type_name: " << p1.type_name() << std::endl;
        std::cout << "2. p1 value_string: " << p1.value_string() << std::endl;
        std::cout << "3. p2 is_null: " << p2.is_null() << std::endl;

        assert(p1.type_name() == "int32");
        assert(p1.value_string() == "42");
        assert(p2.is_null());

        p2.set_value("MiniORM");
        std::cout << "4. p2 value_string after set_value: " << p2.value_string() << std::endl;
        assert(!p2.is_null());

        TestStatement stmt;
        stmt.bind_parameter(0, 123);
        std::cout << "5. statement bind int32: " << stmt.last_bind_value() << std::endl;
        assert(stmt.last_bind_value() == "123");

        stmt.bind_parameter(1, String("hello"));
        std::cout << "6. statement bind string: " << stmt.last_bind_value() << std::endl;
        assert(stmt.last_bind_value().find("hello") != String::npos);

        stmt.bind_null(2);
        std::cout << "7. statement bind null: " << stmt.last_bind_value() << std::endl;
        assert(stmt.last_bind_value() == "NULL");
    }

    void test_connection_and_operations()
    {
        std::cout << "\n=== 测试连接与数据库操作辅助类 ===" << std::endl;

        TestConnection conn;
        DatabaseConfig config;
        config.type = DatabaseType::SQLite;
        config.database = "miniorm.db";

        assert(conn.connect(config));
        assert(conn.is_connected());

        std::cout << "1. map_type_to_sql(int32): " << conn.map_type_to_sql(1) << std::endl;
        std::cout << "2. prepare_sql: " << conn.prepare_sql("SELECT {} + {}", 1, 2) << std::endl;
        assert(conn.map_type_to_sql(1) == "INTEGER");
        assert(conn.prepare_sql("SELECT {} + {}", 1, 2).find("SELECT 1 + {}") != String::npos);

        conn.set_query_rows({{"1", "Alice"}, {"2", "Bob"}}, {"id", "name"});
        auto shared_conn = std::make_shared<TestConnection>(conn);
        DatabaseOperations ops(shared_conn);

        auto v = ops.query_value<int32>("SELECT id FROM users");
        std::cout << "3. query_value<int32>: " << v.value() << std::endl;
        assert(v.has_value() && v.value() == 1);

        auto v2 = ops.query_value<int32>("SELECT id FROM users WHERE name = {}", "Alice");
        std::cout << "4. query_value with args: " << v2.value() << std::endl;
        assert(v2.has_value() && v2.value() == 1);

        auto row = ops.query_row<UserRecord>("SELECT id, name FROM users", [](const IResultRow &r)
                                             { return UserRecord{r.get<int32>(0), r.get<String>(1)}; });
        std::cout << "5. query_row<UserRecord> has_value: " << row.has_value() << std::endl;
        assert(row.has_value());

        auto rows = ops.query_rows<UserRecord>("SELECT id, name FROM users", [](const IResultRow &r)
                                               { return UserRecord{r.get<int32>(0), r.get<String>(1)}; });
        std::cout << "6. query_rows size: " << rows.size() << std::endl;
        assert(rows.size() == 2);

        int32 affected = ops.execute_update("UPDATE users SET name = {} WHERE id = {}", "Alice", 1);
        std::cout << "7. execute_update affected: " << affected << std::endl;
        assert(affected == 1);

        std::vector<String> values{"1", "2", "3"};
        int32 inserted = ops.batch_insert("users", values, [](const StringView &table, const String &value)
                                          { return StringFormatter::format("INSERT INTO {} VALUES ({})", table, value); });
        std::cout << "8. batch_insert inserted: " << inserted << std::endl;
        assert(inserted == 3);
        assert(shared_conn->begin_transaction_calls() >= 1);
        assert(shared_conn->commit_calls() >= 1);

        Bool dropped = ops.drop_table("users");
        std::cout << "9. drop_table: " << dropped << std::endl;
        assert(dropped);
    }

    void test_context()
    {
        std::cout << "\n=== 测试数据库上下文 ===" << std::endl;

        auto conn = std::make_shared<TestConnection>();
        DatabaseConfig config;
        config.type = DatabaseType::SQLite;
        config.database = "miniorm.db";
        conn->connect(config);

        DatabaseContext context(conn);

        auto result = context.transaction([&]()
                                          { return String("tx-ok"); });
        std::cout << "1. transaction result: " << result << std::endl;
        assert(result == "tx-ok");

        Bool vacuum_ok = context.vacuum();
        Bool optimize_ok = context.optimize();
        std::cout << "2. vacuum: " << vacuum_ok << std::endl;
        std::cout << "3. optimize: " << optimize_ok << std::endl;
        assert(vacuum_ok);
        assert(optimize_ok);
    }

} // namespace miniorm

using namespace miniorm;

int main()
{
    std::cout << "==========================================" << std::endl;
    std::cout << "MiniORM Adapter Test Program" << std::endl;
    std::cout << "测试 adapter.hpp 中的配置、接口和辅助类" << std::endl;
    std::cout << "==========================================" << std::endl;

    try
    {
        test_database_type_and_config();
        test_database_exception();
        test_result_row_and_set();
        test_parameter_and_statement();
        test_connection_and_operations();
        test_context();

        std::cout << "\n==========================================" << std::endl;
        std::cout << "所有测试完成！" << std::endl;
        std::cout << "MiniORM adapter 系统功能正常" << std::endl;
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