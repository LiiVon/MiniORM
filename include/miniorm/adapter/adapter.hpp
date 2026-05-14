// adapter.hpp  是 MiniORM 项目的数据库适配器模块
// 负责数据库连接管理、SQL 执行、结果集处理等核心功能。
// 这个模块实现了数据库抽象层，支持多种数据库后端

#ifndef MINIORM_ADAPTER_ADAPTER_HPP
#define MINIORM_ADAPTER_ADAPTER_HPP

#include "../core/config.hpp"
#include "../core/concepts.hpp"
#include "../core/traits.hpp"
#include "../core/utils.hpp"

#if !MINIORM_CPP20
#error "adapter.hpp requires C++20 support. Please enable C++20 in your compiler settings."
#endif

#include <string>
#include <string_view>
#include <stdexcept>
#include <memory>
#include <functional>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <optional>
#include <unordered_map>
#include <condition_variable>

namespace miniorm
{
    //  数据库类型枚举
    enum class DatabaseType : int32
    {
        Unknown = 0,
        SQLite = 1,
        MySQL = 2,
        PostgreSQL = 3,
        SQLServer = 4,
    };

    constexpr StringView database_type_name(DatabaseType type) noexcept
    {
        switch (type)
        {
        case DatabaseType::SQLite:
            return "SQLite";
        case DatabaseType::MySQL:
            return "MySQL";
        case DatabaseType::PostgreSQL:
            return "PostgreSQL";
        case DatabaseType::SQLServer:
            return "SQL Server";
        default:
            return "Unknown";
        }
    }

    // 数据库配置结构
    struct DatabaseConfig
    {
        DatabaseType type = DatabaseType::Unknown;
        String host = "localhost";
        int32 port = 0;
        String database;
        String username;
        String password;
        String options;

        Bool validate() const noexcept; // 验证配置是否合法

        String connection_string() const; // 生成连接字符串
    };

    enum class ConnectionState : int32
    {
        Disconnected = 0,
        Connecting = 1,
        Connected = 2,
        Executing = 3,
        Error = 4
    };

    class DatabaseException : public std::runtime_error
    {
    public:
        DatabaseException(const String &msg, int32 error_code = 0, const String &sql = "");

        int32 error_code() const noexcept { return error_code_; }
        const String &sql() const noexcept { return sql_; }

        String detailed_message() const;

    private:
        int32 error_code_;
        String sql_;
    };

    // 结果行接口
    class IResultRow
    {
    public:
        virtual ~IResultRow() = default;

        virtual Size column_count() const = 0;
        virtual String column_name(Size index) const = 0;
        virtual Size column_index(const StringView &name) const = 0;

        virtual Bool is_null(Size index) const = 0;
        virtual Bool is_null(const StringView &name) const = 0;

        template <typename T> // 获取指定类型的值
        T get(Size index) const
        {
            if (is_null(index))
            {
                if constexpr (is_nullable_v<T>)
                {
                    return T{};
                }
                else
                {
                    throw DatabaseException(StringFormatter::format("Cannot get null value as non-nullable type {} at index {}", type_name_v<T>, index));
                }
            }
            String value_str = get_string(index);
            try
            {
                return FromString<T>::parse(value_str);
            }
            catch (const std::exception &e)
            {
                throw DatabaseException(StringFormatter::format("Failed to convert value '{}' to type {} at index {}: {}", value_str, type_name_v<T>, index, e.what()));
            }
        }

        template <typename T>
        T get(const StringView &name) const
        {
            Size index = column_index(name);
            return get<T>(index);
        }

        template <typename T>
        std::optional<T> get_optional(Size index) const
        {
            if (is_null(index))
            {
                return std::nullopt;
            }
            try
            {
                return get<T>(index);
            }
            catch (...)
            {
                return std::nullopt;
            }
        }

        template <typename T>
        std::optional<T> get_optional(const StringView &name) const
        {
            Size index = column_index(name);
            return get_optional<T>(index);
        }

    protected:
        virtual String get_string(Size index) const = 0;
    };

    class IResultSet
    {
    public:
        virtual ~IResultSet() = default;

        virtual Size row_count() const = 0;
        virtual Size column_count() const = 0;
        virtual String column_name(Size index) const = 0;

        virtual Bool next() = 0;
        virtual const IResultRow &current_row() const = 0;

        // 获取所有行
        template <SequenceContainer Container = std::vector<std::vector<String>>>
        Container fetch_all() const
        {
            Container res;
            auto temp_set = clone();
            while (temp_set->next())
            {
                const auto &row = temp_set->current_row();

                std::vector<String> row_data;
                row_data.reserve(column_count());

                for (Size i = 0; i < column_count(); ++i)
                {
                    if (row.is_null(i))
                    {
                        row_data.emplace_back("NULL");
                    }
                    else
                    {

                        row_data.emplace_back(row.get<String>(i));
                    }
                }

                if constexpr (requires { res.push_back(row_data); })
                {
                    res.push_back(std::move(row_data));
                }
            }
            return res;
        }

        // 获取第一行的第一个值
        template <DatabaseMappable T>
        std::optional<T> fetch_first() const
        {
            auto temp_set = clone();
            if (temp_set->next())
            {

                return temp_set->current_row().get_optional<T>(0);
            }
            return std::nullopt;
        }

        // 获取指定列的所有值
        template <DatabaseMappable T, SequenceContainer Container = std::vector<T>>
        Container fetch_column(Size column_index = 0) const
        {
            Container res;
            auto temp_set = clone();

            while (temp_set->next())
            {
                const auto &row = temp_set->current_row();

                if (auto value = row.get_optional<T>(column_index))
                {

                    if constexpr (requires { res.push_back(*value); })
                    {
                        res.push_back(*value);
                    }
                }
            }
            return res;
        }

        template <typename T, typename Mapper>
            requires requires(Mapper mapper, const IResultRow &row) {
                { mapper(row) } -> std::convertible_to<T>;
            }
        std::vector<T> map(Mapper mapper) const
        {
            std::vector<T> res;
            auto temp_set = clone();

            while (temp_set->next())
            {
                res.push_back(mapper(temp_set->current_row()));
            }
            return res;
        }

    private:
        virtual std::unique_ptr<IResultSet> clone() const = 0;
    };

    //  数据库参数接口
    class IDatabaseParameter
    {
    public:
        virtual ~IDatabaseParameter() = default;

        virtual Size index() const = 0;
        virtual String value_string() const = 0;
        virtual Bool is_null() const = 0;
        virtual String type_name() const = 0;
    };

    template <DatabaseMappable T>
    class TypedDatabaseParameter : public IDatabaseParameter
    {
    public:
        TypedDatabaseParameter(Size index, const T &value)
            : index_(index), value_(value), is_null_(false) {}

        TypedDatabaseParameter(Size index)
            : index_(index), is_null_(true) {}

        Size index() const override { return index_; }
        Bool is_null() const override { return is_null_; }
        String type_name() const override { return String(type_name_v<T>); }
        String value_string() const override
        {
            if (is_null_)
            {
                return "NULL";
            }
            else
            {
                return ToString<T>::convert(*value_);
            }
        }

        const std::optional<T> value() const
        {
            if (is_null_)
            {
                return std::nullopt;
            }
            else
            {
                return value_;
            }
        }

        void set_value(const T &value)
        {
            value_ = value;
            is_null_ = false;
        }

        void set_null()
        {
            value_.reset();
            is_null_ = true;
        }

    private:
        Size index_;
        std::optional<T> value_;
        Bool is_null_;
    };

    using ParameterList = std::vector<std::unique_ptr<IDatabaseParameter>>;

    //  语句接口
    class IStatement
    {
    public:
        virtual ~IStatement() = default;

        virtual const String sql() const = 0;
        virtual void bind_parameters(Size index, const StringView &params) = 0;

        template <DatabaseMappable T>
        void bind_parameter(Size index, const T &value)
        {
            bind_parameters(index, ToString<T>::convert(value));
        }

        virtual void bind_null(Size index) = 0;
        virtual std::unique_ptr<IResultSet> execute_query() = 0;
        virtual int32 execute_update() = 0;
        virtual void reset() = 0;
        virtual void clear_bindings() = 0;
    };

    // 数据库连接接口
    class IDatabaseConnection
    {
    public:
        virtual ~IDatabaseConnection() = default;

        virtual Bool connect(const DatabaseConfig &config) = 0;
        virtual void disconnect() = 0;
        virtual Bool is_connected() const = 0;
        virtual ConnectionState state() const = 0;

        virtual Bool begin_transaction() = 0;
        virtual Bool commit() = 0;
        virtual Bool rollback() = 0;
        virtual Bool is_in_transaction() const = 0;

        virtual std::unique_ptr<IStatement> prepare_statement(const StringView &sql) = 0;

        virtual std::unique_ptr<IResultSet> execute_query(const StringView &sql) = 0;
        virtual int32 execute_update(const StringView &sql) = 0;

        virtual int32 execute_batch(const std::vector<StringView> &sql_statements) = 0;

        virtual DatabaseType database_type() const = 0;
        virtual String database_version() const = 0;
        virtual std::vector<String> get_table_names() = 0;
        virtual std::vector<String> get_column_names(const StringView &table_name) = 0;

        virtual int64 last_insert_id() const = 0;
        virtual int32 affected_rows() const = 0;
        virtual String last_error() const = 0;
        virtual int32 last_error_code() const = 0;

        template <DatabaseMappable T>
        String map_type_to_sql(const T &) const
        {
            return String(sql_type_v<std::decay_t<T>>);
        }

        virtual String escape_identifier(const StringView &identifier) const
        {
            return SqlStringEscaper::escape_identifier(identifier);
        }

        template <typename... Args>
        String prepare_sql(const StringView &sql, Args &&...args)
        {
            return StringFormatter::format_sql(sql, std::forward<Args>(args)...);
        }
    };

    // 数据库适配器基类
    class DatabaseAdapter : public IDatabaseConnection
    {
    public:
        explicit DatabaseAdapter(DatabaseType type)
            : type_(type), state_(ConnectionState::Disconnected), in_transaction_(false) {}
        virtual ~DatabaseAdapter() = default;

        Bool connect(const DatabaseConfig &config) override
        {
            Logger::info("Connecting to {} database at {}:{}", database_type_name(type_), config.host, config.port);
            state_ = ConnectionState::Connecting;

            try
            {
                Bool success = do_connect(config);
                if (success)
                {
                    state_ = ConnectionState::Connected;
                    Logger::info("Successfully connected to {} database", database_type_name(type_));
                }
                else
                {
                    state_ = ConnectionState::Error;
                    Logger::error("Failed to connect to {} database", database_type_name(type_));
                }
            }
            catch (const std::exception &e)
            {
                state_ = ConnectionState::Error;
                Logger::error("Exception while connecting to {} database: {}", database_type_name(type_), e.what());
                return false;
            }

            return state_ == ConnectionState::Connected;
        }

        void disconnect() override
        {
            if (is_connected())
            {
                Logger::info("Disconnecting from {} database", database_type_name(type_));
                if (in_transaction_)
                {
                    Logger::warning("Disconnecting while in transaction, rolling back");
                    rollback();
                }

                do_disconnect();
                state_ = ConnectionState::Disconnected;
                Logger::info("Successfully disconnected from {} database", database_type_name(type_));
            }
        }

        Bool is_connected() const override
        {
            return state_ == ConnectionState::Connected;
        }

        ConnectionState state() const override
        {
            return state_;
        }

        Bool begin_transaction() override
        {
            if (!is_connected())
            {
                Logger::error("Cannot begin transaction: not connected to database");
                return false;
            }
            if (in_transaction_)
            {
                Logger::warning("Already in transaction, cannot begin a new one");
                return false;
            }

            Logger::debug("Beginning transaction on {} database", database_type_name(type_));
            Bool success = do_begin_transaction();
            if (success)
            {
                in_transaction_ = true;
                Logger::debug("Transaction started successfully");
            }
            else
            {
                Logger::error("Failed to start transaction");
            }
            return success;
        }

        Bool commit() override
        {
            if (!in_transaction_)
            {
                Logger::warning("No active transaction to commit");
                return false;
            }

            Logger::debug("Committing transaction on {} database", database_type_name(type_));
            Bool success = do_commit();
            if (success)
            {
                in_transaction_ = false;
                Logger::debug("Transaction committed successfully");
            }
            else
            {
                Logger::error("Failed to commit transaction");
            }
            return success;
        }

        Bool rollback() override
        {
            if (!in_transaction_)
            {
                Logger::warning("No active transaction to rollback");
                return false;
            }

            Logger::debug("Rolling back transaction on {} database", database_type_name(type_));
            Bool success = do_rollback();
            if (success)
            {
                in_transaction_ = false;
                Logger::debug("Transaction rolled back successfully");
            }
            else
            {
                Logger::error("Failed to rollback transaction");
            }
            return success;
        }

        Bool is_in_transaction() const override
        {
            return in_transaction_;
        }

        DatabaseType database_type() const override
        {
            return type_;
        }

        String last_error() const override
        {
            return last_error_;
        }

        int32 last_error_code() const override
        {
            return last_error_code_;
        }

    protected:
        virtual Bool do_connect(const DatabaseConfig &config) = 0;
        virtual void do_disconnect() = 0;
        virtual Bool do_begin_transaction() = 0;
        virtual Bool do_commit() = 0;
        virtual Bool do_rollback() = 0;
        virtual std::unique_ptr<IStatement> do_prepare_statement(const StringView &sql) = 0;
        virtual std::unique_ptr<IResultSet> do_execute_query(const StringView &sql) = 0;
        virtual int32 do_execute_update(const StringView &sql) = 0;
        virtual int32 do_execute_batch(const std::vector<StringView> &sql_statements) = 0;
        virtual String do_database_version() const = 0;
        virtual std::vector<String> do_get_table_names() = 0;
        virtual std::vector<String> do_get_column_names(const StringView &table_name) = 0;
        virtual int64 do_last_insert_id() const = 0;
        virtual int32 do_affected_rows() const = 0;

        void set_last_error(const String &error, int32 code = 0)
        {
            last_error_ = error;
            last_error_code_ = code;
            Logger::error("Database error: {} (code: {})", error, code);
        }

        void clear_last_error()
        {
            last_error_.clear();
            last_error_code_ = 0;
        }

        void check_connected() const
        {
            if (!is_connected())
            {
                throw DatabaseException("Not connected to database");
            }
        }

        void check_not_in_transaction() const
        {
            if (is_in_transaction())
            {
                throw DatabaseException("Cannot perform this operation while in a transaction");
            }
        }

    private:
        std::unique_ptr<IStatement> prepare_statement(const StringView &sql) override
        {
            check_connected();
            clear_last_error();

            try
            {
                Logger::sql_debug("Preparing statement: {}", sql);
                return do_prepare_statement(sql);
            }
            catch (const std::exception &e)
            {
                set_last_error(e.what());
                throw;
            }
        }

        std::unique_ptr<IResultSet> execute_query(const StringView &sql) override
        {
            check_connected();
            clear_last_error();

            try
            {
                Logger::sql_debug("Executing query: {}", sql);
                return do_execute_query(sql);
            }
            catch (const std::exception &e)
            {
                set_last_error(e.what());
                throw;
            }
        }

        int32 execute_update(const StringView &sql) override
        {
            check_connected();
            clear_last_error();

            try
            {
                Logger::sql_debug("Executing update: {}", sql);
                return do_execute_update(sql);
            }
            catch (const std::exception &e)
            {
                set_last_error(e.what());
                throw;
            }
        }

        int32 execute_batch(const std::vector<StringView> &sql_statements) override
        {
            check_connected();
            clear_last_error();

            try
            {
                Logger::sql_debug("Executing batch of {} statements", sql_statements.size());
                return do_execute_batch(sql_statements);
            }
            catch (const std::exception &e)
            {
                set_last_error(e.what());
                throw;
            }
        }

        String database_version() const override
        {
            check_connected();
            return do_database_version();
        }

        std::vector<String> get_table_names() override
        {
            check_connected();
            return do_get_table_names();
        }

        std::vector<String> get_column_names(const StringView &table_name) override
        {
            check_connected();
            return do_get_column_names(table_name);
        }

        int64 last_insert_id() const override
        {
            check_connected();
            return do_last_insert_id();
        }

        int32 affected_rows() const override
        {
            check_connected();
            return do_affected_rows();
        }

    private:
        DatabaseType type_;
        ConnectionState state_;
        Bool in_transaction_;
        String last_error_;
        int32 last_error_code_;
    };

    // 数据库适配器工厂
    class DatabaseAdapterFactory
    {
    public:
        DatabaseAdapterFactory() = delete;

        static std::unique_ptr<IDatabaseConnection> create(DatabaseType type);
        static std::unique_ptr<IDatabaseConnection> create(const DatabaseConfig &config);
        static std::unique_ptr<IDatabaseConnection> create_from_connection_string(const StringView &connection_string);

    private:
        static std::unique_ptr<IDatabaseConnection> create_sqlite_adapter();
        static std::unique_ptr<IDatabaseConnection> create_mysql_adapter();
        static std::unique_ptr<IDatabaseConnection> create_postgresql_adapter();
        static std::unique_ptr<IDatabaseConnection> create_sqlserver_adapter();

        static DatabaseConfig parse_connection_string(const StringView &connection_string);

        static void parse_standard_connection_string(const StringView &connection_string, DatabaseConfig &config);
    };

    //  数据库连接池
    class DatabaseConnectionPool
    {
    public:
        struct PoolConfig
        {
            Size min_connections = 1;
            Size max_connections = 10;
            Size idle_timeout_seconds = 300;
            Size connection_timeout_seconds = 30;
            Bool test_on_borrow = true;
            Bool test_on_return = false;
        };

        DatabaseConnectionPool(const DatabaseConfig &config, const PoolConfig &poo_config);
        ~DatabaseConnectionPool();

        std::shared_ptr<IDatabaseConnection> acquire();

        void release(std::shared_ptr<IDatabaseConnection> conn);

        struct PoolStats
        {
            Size total_connections;
            Size idle_connections;
            Size active_connections;
            Size waiting_threads;
        };

        PoolStats get_stats() const;

        void cleanup_idle_connections();

    private:
        struct PooledConnection
        {
            std::shared_ptr<IDatabaseConnection> connection;
            std::chrono::steady_clock::time_point last_used_time;

            explicit PooledConnection(std::shared_ptr<IDatabaseConnection> conn)
                : connection(std::move(conn)), last_used_time(std::chrono::steady_clock::now()) {}
        };

        void initialize_pool();

        std::shared_ptr<PooledConnection> create_connection();

        Bool test_connection(IDatabaseConnection &conn);

        void shutdown();

    private:
        DatabaseConfig config_;
        PoolConfig pool_config_;

        mutable std::mutex mutex_;
        std::condition_variable cv_;

        std::vector<std::shared_ptr<PooledConnection>> idle_connections_;
        std::vector<std::shared_ptr<PooledConnection>> active_connections_;

        Size total_connections_;
        Size waiting_threads_ = 0;
    };

    // 数据库操作类
    class DatabaseOperations
    {
    public:
        explicit DatabaseOperations(std::shared_ptr<IDatabaseConnection> connection)
            : connection_(std::move(connection))
        {
            if (!connection_)
            {
                throw std::invalid_argument("DatabaseOperations requires a valid database connection");
            }
        }

        template <DatabaseMappable T>
        std::optional<T> query_value(const StringView &sql)
        {
            auto result = connection_->execute_query(sql);
            return result->fetch_first<T>();
        }

        template <DatabaseMappable T, typename... Args>
        std::optional<T> query_value(const StringView &sql, Args &&...args)
        {

            String prepared_sql = connection_->prepare_sql(sql, std::forward<Args>(args)...);
            return query_value<T>(prepared_sql);
        }

        template <typename RowType, typename Mapper>
            requires requires(Mapper mapper, const IResultRow &row) {
                { mapper(row) } -> std::convertible_to<RowType>;
            }
        std::optional<RowType> query_row(const StringView &sql, Mapper &&mapper)
        {
            auto result = connection_->execute_query(sql);
            if (result->next())
            {
                return mapper(result->current_row());
            }
            return std::nullopt;
        }

        template <typename RowType, typename Mapper>
            requires requires(Mapper mapper, const IResultRow &row) {
                { mapper(row) } -> std::convertible_to<RowType>;
            }
        std::vector<RowType> query_rows(const StringView &sql, Mapper &&mapper)
        {
            auto result = connection_->execute_query(sql);
            return result->template map<RowType>(std::forward<Mapper>(mapper));
        }

        template <typename... Args>
        int32 execute_update(const StringView &sql, Args &&...args)
        {
            String prepared_sql = connection_->prepare_sql(sql, std::forward<Args>(args)...);
            return connection_->execute_update(prepared_sql);
        }

        template <SequenceContainer Container, typename ValueMapper>
            requires DatabaseMappable<typename Container::value_type>
        int32 batch_insert(const StringView &table_name, const Container &values, ValueMapper &&value_mapper)
        {
            if (values.empty())
            {
                return 0;
            }

            connection_->begin_transaction();

            try
            {
                int32 total_affected = 0;

                for (const auto &value : values)
                {
                    String sql = value_mapper(table_name, value);
                    total_affected += connection_->execute_update(sql);
                }

                connection_->commit();
                return total_affected;
            }
            catch (...)
            {
                connection_->rollback();
                throw;
            }
        };

        template <typename TableSchema>
        Bool create_table(const StringView &schema)
        {
            String sql = generate_create_table_sql<TableSchema>(schema);
            try
            {
                connection_->execute_update(sql);
                return true;
            }
            catch (const DatabaseException &e)
            {
                Logger::error("Failed to create table with schema {}: {}", schema, e.what());
                return false;
            }
        }

        Bool drop_table(const StringView &table_name)
        {
            String sql = StringFormatter::format("DROP TABLE IF EXISTS {}", connection_->escape_identifier(table_name));
            try
            {
                connection_->execute_update(sql);
                return true;
            }
            catch (const DatabaseException &e)
            {
                Logger::error("Failed to drop table {}: {}", table_name, e.what());
                return false;
            }
        }

    private:
        template <typename TableSchema>
        String generate_create_table_sql(const StringView &schema)
        {

            String sql = StringFormatter::format("CREATE TABLE IF NOT EXISTS {} (", connection_->escape_identifier(schema));

            sql += ")";
            return sql;
        }

        std::shared_ptr<IDatabaseConnection> connection_;
    };

    // 数据库上下文
    class DatabaseContext
    {
    public:
        explicit DatabaseContext(std::shared_ptr<IDatabaseConnection> connection)
            : connection_(std::move(connection)), operations_(connection_)
        {
        }

        std::shared_ptr<IDatabaseConnection> connection() { return connection_; }
        const std::shared_ptr<IDatabaseConnection> connection() const { return connection_; }
        DatabaseOperations &operations() { return operations_; }
        const DatabaseOperations &operations() const { return operations_; }

        template <typename Func>
        auto transaction(Func &&func)
        {
            connection_->begin_transaction();
            try
            {
                auto result = func();
                connection_->commit();
                return result;
            }
            catch (...)
            {
                connection_->rollback();
                throw;
            }
        }

        template <typename Func>
        auto batch(Func &&func)
        {
            return func(operations_);
        }

        Bool vacuum()
        {
            try
            {
                connection_->execute_update("VACUUM");
                return true;
            }
            catch (...)
            {
                return false;
            }
        }

        Bool optimize()
        {
            try
            {
                switch (connection_->database_type())
                {
                case DatabaseType::SQLite:
                    connection_->execute_update("ANALYZE");
                    break;
                case DatabaseType::MySQL:
                    connection_->execute_update("OPTIMIZE TABLE");
                    break;
                case DatabaseType::PostgreSQL:
                    connection_->execute_update("VACUUM ANALYZE");
                    break;
                case DatabaseType::SQLServer:
                    connection_->execute_update("UPDATE STATISTICS");
                    break;
                default:

                    break;
                }
                return true;
            }
            catch (...)
            {
                return false;
            }
        }

    private:
        std::shared_ptr<IDatabaseConnection> connection_;
        DatabaseOperations operations_;
    };

}

#endif