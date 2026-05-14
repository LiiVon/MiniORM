//  MiniORM 框架的数据库连接管理层，
// 提供高级的数据库连接抽象、连接池管理、监控统计、事务管理等功能。

#ifndef MINIORM_CONNECTION_DATABASE_CONNECTION_HPP
#define MINIORM_CONNECTION_DATABASE_CONNECTION_HPP

#include "../core/config.hpp"
#include "../core/concepts.hpp"
#include "../core/traits.hpp"
#include "../core/utils.hpp"
#include "../adapter/adapter.hpp"

#if !MINIORM_CPP20
#error "database_connection.hpp requires C++20 support. Please enable C++20 in your compiler settings."
#endif

#include <algorithm>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace miniorm
{
    class DatabaseConnection;
    // 表示数据库连接相关的异常，包含连接信息和错误码
    class DatabaseConnectionException : public DatabaseException
    {
    public:
        DatabaseConnectionException(const String &msg, const String &connection_info = "", int32 error_code = 0);

        static DatabaseConnectionException connection_timeout(const String &connection_info, int32 timeout_seconds);

        static DatabaseConnectionException pool_exhausted(const String &connection_info, Size pool_size);

        const String &connection_info() const noexcept { return connection_info_; }

        String detailed_message() const;

    private:
        String connection_info_;
    };

    // 数据库连接配置，包含连接池、行为与性能相关子配置
    struct DatabaseConnectionConfig : public DatabaseConfig
    {
        // 连接池相关配置
        struct PoolConfig
        {
            Size min_connections = 1;
            Size max_connections = 10;
            Size idle_timeout_seconds = 300;
            Size connection_timeout_seconds = 30;
            Bool test_on_borrow = true;
            Bool test_on_return = false;
            Bool enable_pool = true;
        };

        // 行为相关配置（重连、缓存、重试等）
        struct BehaviorConfig
        {
            Bool auto_reconnect = true;
            Size max_retries = 3;
            Size reconnect_delay_ms = 1000;
            Bool enable_query_cache = false;
            Size query_cache_size = 1000;
            Bool enable_statement_cache = false;
            Size statement_cache_size = 100;
        };

        // 性能相关配置（批量操作、预编译等）
        struct PerformanceConfig
        {
            Bool enable_batch_operations = true;
            Size batch_size = 1000;
            Bool enable_prepared_statements = true;
            Bool enable_connection_pooling = true;
        };

        PoolConfig pool_config;
        BehaviorConfig behavior_config;
        PerformanceConfig performance_config;

        Bool validate() const noexcept;

        String summary() const;
    };

    // 连接监控单例：统计连接/查询/错误等运行时信息
    class ConnectionMonitor
    {
    public:
        struct ConnectionStats
        {
            Size total_connections = 0;
            Size active_connections = 0;
            Size idle_connections = 0;
            Size failed_connections = 0;

            Size total_queries = 0;
            Size successful_queries = 0;
            Size failed_queries = 0;
            Size total_updates = 0;
            Size successful_updates = 0;
            Size failed_updates = 0;

            std::chrono::milliseconds total_query_time{0};
            std::chrono::milliseconds total_update_time{0};
            std::chrono::milliseconds max_query_time{0};
            std::chrono::milliseconds max_update_time{0};

            std::map<int32, Size> error_codes;

            String generate_report() const;
        };

        // 获取监控单例实例
        static ConnectionMonitor &instance();

        void record_connection_created();

        void record_connection_closed();

        void record_connection_failed();

        void record_query_started();

        void record_query_completed(std::chrono::milliseconds duration, Bool success);

        void record_error(int32 error_code);

        ConnectionStats get_stats() const;

        void reset_stats();

    private:
        ConnectionStats stats_;
        mutable std::mutex mutex_;

        ConnectionMonitor() = default;
        ~ConnectionMonitor() = default;
    };

    // 高级数据库连接封装，支持直接连接、连接池、事务和查询工具
    class DatabaseConnection
    {
    public:
        explicit DatabaseConnection(const DatabaseConnectionConfig &config)
            : config_(config)
        {
            initialize_connection();
        }

        explicit DatabaseConnection(const StringView &connection_string)
            : config_()
        {
            parse_connection_string(connection_string);
            initialize_connection();
        }

        explicit DatabaseConnection(std::shared_ptr<IDatabaseConnection> existing_connection)
            : connection_(std::move(existing_connection)),
              operations_(std::make_unique<DatabaseOperations>(connection_)),
              context_(std::make_unique<DatabaseContext>(connection_))
        {
            ConnectionMonitor::instance().record_connection_created();
            Logger::info("DatabaseConnection created with existing connection");
        }

        MINIORM_DISABLE_COPY(DatabaseConnection);

        DatabaseConnection(DatabaseConnection &&other) noexcept
            : config_(std::move(other.config_)),
              connection_(std::move(other.connection_)),
              connection_pool_(std::move(other.connection_pool_)),
              operations_(std::move(other.operations_)),
              context_(std::move(other.context_))
        {
            other.connection_ = nullptr;
        }

        DatabaseConnection &operator=(DatabaseConnection &&other) noexcept
        {
            if (this != &other)
            {
                config_ = std::move(other.config_);
                connection_ = std::move(other.connection_);
                connection_pool_ = std::move(other.connection_pool_);
                operations_ = std::move(other.operations_);
                context_ = std::move(other.context_);
                other.connection_ = nullptr;
            }
            return *this;
        }

        // 析构时确保连接关闭且资源释放
        ~DatabaseConnection()
        {
            try
            {
                close();
            }
            catch (...)
            {
                Logger::error("Exception occurred while closing DatabaseConnection in destructor");
            }
        }

        // 打开连接：根据配置选择连接池或直接创建适配器
        Bool open()
        {
            if (is_open())
            {
                Logger::warning("DatabaseConnection is already open");
                return true;
            }

            try
            {
                if (config_.performance_config.enable_connection_pooling && config_.pool_config.enable_pool)
                {
                    if (!connection_pool_)
                    {
                        initialize_connection_pool();
                    }
                    connection_ = connection_pool_->acquire();
                    Logger::info("DatabaseConnection acquired from pool: {}", config_.summary());
                }
                else
                {
                    connection_ = DatabaseAdapterFactory::create(config_);
                    Logger::info("DatabaseConnection created directly: {}", config_.summary());
                }

                operations_ = std::make_unique<DatabaseOperations>(connection_);
                context_ = std::make_unique<DatabaseContext>(connection_);
                ConnectionMonitor::instance().record_connection_created();
                return true;
            }
            catch (const std::exception &ex)
            {
                ConnectionMonitor::instance().record_connection_failed();
                Logger::error("Failed to open DatabaseConnection: {}. Exception: {}", config_.summary(), ex.what());
                return false;
            }
        }

        // 关闭连接或将连接返回连接池
        void close()
        {
            if (!is_open())
            {
                return;
            }

            try
            {
                ConnectionMonitor::instance().record_connection_closed();
                if (connection_pool_ && connection_)
                {
                    connection_pool_->release(connection_);
                    Logger::debug("DatabaseConnection released back to pool: {}", config_.summary());
                }
                else if (connection_)
                {
                    connection_->disconnect();
                    Logger::debug("DatabaseConnection closed directly: {}", config_.summary());
                }

                connection_ = nullptr;
                operations_.reset();
                context_.reset();
            }
            catch (const std::exception &ex)
            {
                Logger::error("Failed to close DatabaseConnection: {}. Exception: {}", config_.summary(), ex.what());
            }
        }

        // 自动重连逻辑（受配置控制，包含重试机制）
        Bool reconnect()
        {
            if (!config_.behavior_config.auto_reconnect)
            {
                Logger::warning("Auto-reconnect is disabled in configuration");
                return false;
            }

            for (Size attempt = 1; attempt <= config_.behavior_config.max_retries; ++attempt)
            {
                try
                {
                    close();
                    if (attempt > 1)
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(config_.behavior_config.reconnect_delay_ms));
                    }

                    if (open())
                    {
                        Logger::info("Successfully reconnected DatabaseConnection on attempt {}: {}", attempt, config_.summary());
                        return true;
                    }
                }
                catch (const std::exception &ex)
                {
                    Logger::error("Reconnect attempt {} failed for DatabaseConnection: {}. Exception: {}", attempt, config_.summary(), ex.what());
                }
            }
            return false;
        }

        // 判断底层连接是否可用
        Bool is_open() const
        {
            return connection_ && connection_->is_connected();
        }

        // 简单健康检查：执行轻量查询验证连接
        Bool is_healthy() const
        {
            if (!is_open())
            {
                return false;
            }

            try
            {
                auto result = connection_->execute_query("SELECT 1");
                return result != nullptr;
            }
            catch (...)
            {
                Logger::warning("Health check query failed for DatabaseConnection: {}", config_.summary());
                return false;
            }
        }

        // 获取数据库类型（MySQL/SQLite等）
        DatabaseType database_type() const
        {
            check_connection();
            return connection_->database_type();
        }

        // 获取底层数据库版本字符串
        String database_version() const
        {
            check_connection();
            return connection_->database_version();
        }

        // 获取当前数据库中的表名列表
        std::vector<String> get_table_names()
        {
            check_connection();
            return connection_->get_table_names();
        }

        // 获取指定表的列名列表
        std::vector<String> get_column_names(const String &table_name)
        {
            check_connection();
            return connection_->get_column_names(table_name);
        }

        // 判断表是否存在（使用表名列表检索）
        Bool table_exists(const StringView &table_name)
        {
            check_connection();
            auto tables = connection_->get_table_names();
            return std::find(tables.begin(), tables.end(), String(table_name)) != tables.end();
        }

        // 执行查询并返回结果集，同时记录监控统计与日志
        std::unique_ptr<IResultSet> execute_query(const String &query)
        {
            auto start_time = std::chrono::steady_clock::now();
            ConnectionMonitor::instance().record_query_started();

            try
            {
                check_connection();
                Logger::sql_debug("Executing query: {}", query);
                auto result = connection_->execute_query(query);
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time);
                ConnectionMonitor::instance().record_query_completed(duration, true);
                Logger::debug("Query executed successfully in {} ms: {}", duration.count(), query);
                return result;
            }
            catch (const std::exception &ex)
            {
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time);
                ConnectionMonitor::instance().record_query_completed(duration, false);
                Logger::error("Query execution failed in {} ms: {}. Exception: {}", duration.count(), query, ex.what());
                throw;
            }
        }

        // 执行更新语句并返回受影响行数
        int32 execute_update(const StringView &sql)
        {
            auto start_time = std::chrono::steady_clock::now();
            ConnectionMonitor::instance().record_query_started();

            try
            {
                check_connection();
                Logger::sql_debug("Executing update: {}", sql);
                int32 affected_rows = connection_->execute_update(sql);
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time);
                ConnectionMonitor::instance().record_query_completed(duration, true);
                Logger::debug("Update executed successfully in {} ms: {}. Affected rows: {}", duration.count(), sql, affected_rows);
                return affected_rows;
            }
            catch (const std::exception &ex)
            {
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time);
                ConnectionMonitor::instance().record_query_completed(duration, false);
                Logger::error("Update execution failed in {} ms: {}. Exception: {}", duration.count(), sql, ex.what());
                throw;
            }
        }

        // 批量执行多条 SQL（在事务中），失败时回滚
        int32 execute_batch(const std::vector<String> &sql_commands)
        {
            auto start_time = std::chrono::steady_clock::now();
            ConnectionMonitor::instance().record_query_started();

            try
            {
                check_connection();
                Logger::sql_debug("Executing batch of {} commands", sql_commands.size());

                int32 total_affected_rows = 0;
                begin_transaction();

                try
                {
                    for (const auto &sql : sql_commands)
                    {
                        total_affected_rows += connection_->execute_update(sql);
                    }

                    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time);
                    ConnectionMonitor::instance().record_query_completed(duration, true);
                    Logger::debug("Batch executed successfully in {} ms. Total affected rows: {}", duration.count(), total_affected_rows);
                    return total_affected_rows;
                }
                catch (...)
                {
                    rollback();
                    throw;
                }
            }
            catch (const std::exception &ex)
            {
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time);
                ConnectionMonitor::instance().record_query_completed(duration, false);
                Logger::error("Batch execution failed in {} ms. Exception: {}", duration.count(), ex.what());
                throw;
            }
        }

        // 从查询中提取单个标量或映射类型的值
        template <DatabaseMappable T>
        std::optional<T> query_value(const StringView &sql)
        {
            auto result = execute_query(String(sql));
            if (result && result->row_count() > 0)
            {
                return result->fetch_first<T>();
            }
            return std::nullopt;
        }

        template <DatabaseMappable T, typename... Args>
        std::optional<T> query_value(const StringView &sql, Args &&...args)
        {
            String prepared_sql = prepare_sql(sql, std::forward<Args>(args)...);
            return query_value<T>(prepared_sql);
        }

        template <typename RowType, typename Mapper>
            requires requires(Mapper mapper, const IResultRow &row) {
                { mapper(row) } -> std::convertible_to<RowType>;
            }
        std::optional<RowType> query_row(const StringView &sql, Mapper &&mapper)
        {
            auto result = execute_query(String(sql));
            if (result && result->row_count() > 0)
            {
                auto rows = result->template map<RowType>(std::forward<Mapper>(mapper));
                if (!rows.empty())
                {
                    return rows.front();
                }
            }
            return std::nullopt;
        }

        template <typename RowType, typename Mapper>
            requires requires(Mapper mapper, const IResultRow &row) {
                { mapper(row) } -> std::convertible_to<RowType>;
            }
        std::vector<RowType> query_rows(const StringView &sql, Mapper &&mapper)
        {
            auto result = execute_query(String(sql));
            if (!result)
            {
                return {};
            }
            return result->template map<RowType>(std::forward<Mapper>(mapper));
        }

        template <SequenceContainer Container, typename Mapper>
            requires requires(Mapper mapper, const IResultRow &row) {
                { mapper(row) } -> std::convertible_to<typename Container::value_type>;
            }
        Container query_to_container(const StringView &sql, Mapper &&mapper)
        {
            auto rows = query_rows<typename Container::value_type>(sql, std::forward<Mapper>(mapper));
            Container result;
            if constexpr (requires { result.reserve(rows.size()); })
            {
                result.reserve(rows.size());
            }

            for (auto &row : rows)
            {
                if constexpr (requires { result.push_back(std::move(row)); })
                {
                    result.push_back(std::move(row));
                }
                else if constexpr (requires { result.insert(result.end(), std::move(row)); })
                {
                    result.insert(result.end(), std::move(row));
                }
            }
            return result;
        }

        // 准备预编译语句对象
        std::unique_ptr<IStatement> prepare(const StringView &sql)
        {
            check_connection();
            Logger::sql_debug("Preparing statement: {}", sql);
            return connection_->prepare_statement(sql);
        }

        template <typename... Args>
        std::unique_ptr<IResultSet> execute_prepared_query(const StringView &sql, Args &&...args)
        {
            auto statement = prepare(sql);
            bind_parameters(*statement, std::forward<Args>(args)...);
            return statement->execute_query();
        }

        template <typename... Args>
        int32 execute_prepared_update(const StringView &sql, Args &&...args)
        {
            auto statement = prepare(sql);
            bind_parameters(*statement, std::forward<Args>(args)...);
            return statement->execute_update();
        }

        // 事务控制：开始
        Bool begin_transaction()
        {
            check_connection();
            Logger::debug("Beginning transaction");
            return connection_->begin_transaction();
        }

        // 事务控制：提交
        Bool commit()
        {
            check_connection();
            Logger::debug("Committing transaction");
            return connection_->commit();
        }

        // 事务控制：回滚
        Bool rollback()
        {
            check_connection();
            Logger::debug("Rolling back transaction");
            return connection_->rollback();
        }

        Bool is_in_transaction() const
        {
            return connection_ && connection_->is_in_transaction();
        }

        // 在事务中执行传入函数，自动提交或回滚
        template <typename Func>
        auto transaction(Func &&func)
        {
            check_connection();
            if (!begin_transaction())
            {
                throw DatabaseException("Failed to begin transaction");
            }

            try
            {
                auto result = func();
                if (!commit())
                {
                    throw DatabaseException("Failed to commit transaction");
                }
                return result;
            }
            catch (...)
            {
                rollback();
                throw;
            }
        }

        // 支持嵌套事务（使用 SAVEPOINT 实现）
        template <typename Func>
        auto nested_transaction(Func &&func)
        {
            Bool outer_transaction = is_in_transaction();
            if (!outer_transaction)
            {
                return transaction(std::forward<Func>(func));
            }

            try
            {
                execute_update("SAVEPOINT nested_transaction");
                auto result = func();
                execute_update("RELEASE SAVEPOINT nested_transaction");
                return result;
            }
            catch (...)
            {
                execute_update("ROLLBACK TO SAVEPOINT nested_transaction");
                throw;
            }
        }

        template <SequenceContainer Container, typename ValueMapper>
            requires DatabaseMappable<typename Container::value_type>
        // 批量插入：根据配置拆分为多批并在事务中执行
        int32 batch_insert(const StringView &table_name, const Container &values, ValueMapper &&value_mapper)
        {
            if (!config_.performance_config.enable_batch_operations)
            {
                throw DatabaseConnectionException("Batch operations are disabled");
            }
            if (values.empty())
            {
                return 0;
            }

            Size batch_size = config_.performance_config.batch_size;
            int32 total_affected = 0;

            for (Size i = 0; i < values.size(); i += batch_size)
            {
                Size end = std::min(i + batch_size, values.size());
                auto batch = std::vector<typename Container::value_type>(values.begin() + i, values.begin() + end);
                total_affected += transaction([&]()
                                              {
                    int32 batch_affected = 0;
                    for (const auto &value : batch)
                    {
                        String sql = value_mapper(table_name, value);
                        batch_affected += execute_update(sql);
                    }
                    return batch_affected; });
                Logger::debug("Processed batch {}-{} of {}", i, end - 1, values.size());
            }
            return total_affected;
        }

        template <SequenceContainer Container, typename UpdateMapper>
            requires DatabaseMappable<typename Container::value_type>
        int32 batch_update(const StringView &table_name, const Container &values, UpdateMapper &&update_mapper)
        {
            return batch_insert(table_name, values, std::forward<UpdateMapper>(update_mapper));
        }

        // 获取最后插入行的自增 ID
        int64 last_insert_id()
        {
            check_connection();
            return connection_->last_insert_id();
        }

        int32 affected_rows()
        {
            check_connection();
            return connection_->affected_rows();
        }

        // 返回最近一次底层连接的错误信息
        String last_error() const
        {
            if (connection_)
            {
                return connection_->last_error();
            }
            return "No connection";
        }

        int32 last_error_code() const
        {
            if (connection_)
            {
                return connection_->last_error_code();
            }
            return 0;
        }

        // 转义标识符（表名、列名等），以防 SQL 注入或关键字冲突
        String escape_identifier(const StringView &identifier) const
        {
            check_connection();
            return connection_->escape_identifier(identifier);
        }

        // 使用底层适配器的参数绑定机制生成最终 SQL
        template <typename... Args>
        String prepare_sql(const StringView &sql, Args &&...args)
        {
            check_connection();
            return connection_->prepare_sql(sql, std::forward<Args>(args)...);
        }

        std::shared_ptr<IDatabaseConnection> get_raw_connection() const { return connection_; }
        DatabaseOperations *get_operations() { return operations_.get(); }
        DatabaseContext *get_context() { return context_.get(); }
        const DatabaseConnectionConfig &get_config() const { return config_; }
        ConnectionMonitor::ConnectionStats get_stats() const { return ConnectionMonitor::instance().get_stats(); }
        void reset_stats() { ConnectionMonitor::instance().reset_stats(); }

        // 清理连接池中的空闲连接
        void cleanup_connection_pool()
        {
            if (connection_pool_)
            {
                connection_pool_->cleanup_idle_connections();
                Logger::debug("Cleaned up idle connections in pool");
            }
        }

        // 获取连接池统计信息（如果启用并初始化）
        std::optional<DatabaseConnectionPool::PoolStats> get_pool_stats() const
        {
            if (connection_pool_)
            {
                return connection_pool_->get_stats();
            }
            return std::nullopt;
        }

    private:
        // 初始化连接：校验配置并打开连接（抛出异常表示失败）
        void initialize_connection()
        {
            if (!config_.validate())
            {
                throw DatabaseConnectionException("Invalid database configuration");
            }
            Logger::info("Initializing database connection: {}", config_.summary());
            if (!open())
            {
                throw DatabaseConnectionException(StringFormatter::format("Failed to open database connection: {}", config_.summary()));
            }
        }

        // 根据配置创建并初始化连接池
        void initialize_connection_pool()
        {
            try
            {
                DatabaseConnectionPool::PoolConfig pool_config;
                pool_config.min_connections = config_.pool_config.min_connections;
                pool_config.max_connections = config_.pool_config.max_connections;
                pool_config.idle_timeout_seconds = config_.pool_config.idle_timeout_seconds;
                pool_config.connection_timeout_seconds = config_.pool_config.connection_timeout_seconds;
                pool_config.test_on_borrow = config_.pool_config.test_on_borrow;
                pool_config.test_on_return = config_.pool_config.test_on_return;

                connection_pool_ = std::make_unique<DatabaseConnectionPool>(config_, pool_config);
                Logger::info("Initialized connection pool: min={}, max={}, timeout={}s", pool_config.min_connections, pool_config.max_connections, pool_config.idle_timeout_seconds);
            }
            catch (const std::exception &ex)
            {
                Logger::error("Failed to initialize connection pool: {}", ex.what());
                throw DatabaseConnectionException(StringFormatter::format("Connection pool initialization failed: {}", ex.what()));
            }
        }

        // 解析连接字符串（支持 sqlite/mysql/postgresql/sqlserver）
        void parse_connection_string(const StringView &connection_string)
        {
            try
            {
                if (connection_string.starts_with("sqlite://"))
                {
                    config_.type = DatabaseType::SQLite;
                    config_.database = String(connection_string.substr(9));
                    return;
                }

                if (connection_string.starts_with("mysql://"))
                {
                    config_.type = DatabaseType::MySQL;
                }
                else if (connection_string.starts_with("postgresql://"))
                {
                    config_.type = DatabaseType::PostgreSQL;
                }
                else if (connection_string.starts_with("sqlserver://"))
                {
                    config_.type = DatabaseType::SQLServer;
                }
                else
                {
                    throw std::invalid_argument(StringFormatter::format("Unsupported connection string format: {}", connection_string));
                }

                auto protocol_end = connection_string.find("://");
                auto credentials_start = protocol_end + 3;
                auto at_pos = connection_string.find('@', credentials_start);
                auto host_start = at_pos + 1;
                auto colon_pos = connection_string.find(':', host_start);
                auto slash_pos = connection_string.find('/', colon_pos);
                auto question_pos = connection_string.find('?', slash_pos);

                config_.username = String(connection_string.substr(credentials_start, at_pos - credentials_start));
                config_.password = String(connection_string.substr(at_pos + 1, host_start - (at_pos + 1)));
                config_.host = String(connection_string.substr(host_start, colon_pos - host_start));
                config_.port = std::stoi(String(connection_string.substr(colon_pos + 1, slash_pos - (colon_pos + 1))));
                config_.database = String(connection_string.substr(slash_pos + 1, question_pos - (slash_pos + 1)));
                if (question_pos != StringView::npos)
                {
                    config_.options = String(connection_string.substr(question_pos + 1));
                }

                Logger::debug("Parsed connection string: {}", connection_string);
            }
            catch (const std::exception &ex)
            {
                throw DatabaseConnectionException(StringFormatter::format("Failed to parse connection string '{}': {}", connection_string, ex.what()));
            }
        }

        // 校验连接是否已打开，否则抛出异常
        void check_connection() const
        {
            if (!is_open())
            {
                throw DatabaseConnectionException("Database connection is not open");
            }
        }

        // 将可变参数按索引绑定到预编译语句上
        template <typename... Args>
        void bind_parameters(IStatement &statement, Args &&...args)
        {
            Size index = 0;
            (bind_parameter(statement, index++, std::forward<Args>(args)), ...);
        }

        // 单个参数绑定实现，支持 nullable 与 nullopt
        template <typename T>
        void bind_parameter(IStatement &statement, Size index, T &&value)
        {
            if constexpr (std::is_same_v<std::decay_t<T>, std::nullopt_t>)
            {
                statement.bind_null(index);
            }
            else if constexpr (is_nullable_v<std::decay_t<T>>)
            {
                if (!value.has_value())
                {
                    statement.bind_null(index);
                }
                else
                {
                    statement.bind_parameter(index, *value);
                }
            }
            else
            {
                statement.bind_parameter(index, std::forward<T>(value));
            }
        }

    private:
        DatabaseConnectionConfig config_;
        std::shared_ptr<IDatabaseConnection> connection_;
        std::unique_ptr<DatabaseConnectionPool> connection_pool_;
        std::unique_ptr<DatabaseOperations> operations_;
        std::unique_ptr<DatabaseContext> context_;
    };

    // 管理多个命名数据库连接的单例管理器（创建/获取/关闭）
    class DatabaseConnectionManager
    {
    public:
        static DatabaseConnectionManager &instance();

        DatabaseConnectionManager(const DatabaseConnectionManager &) = delete;
        DatabaseConnectionManager &operator=(const DatabaseConnectionManager &) = delete;
        DatabaseConnectionManager(DatabaseConnectionManager &&) = delete;
        DatabaseConnectionManager &operator=(DatabaseConnectionManager &&) = delete;

        std::shared_ptr<DatabaseConnection> create_connection(const DatabaseConnectionConfig &config);

        std::shared_ptr<DatabaseConnection> get_connection(const String &connection_id);

        void close_connection(const String &connection_id);

        void close_all_connections();

        std::vector<String> get_all_connection_ids() const;

        struct ManagerStats
        {
            Size total_connections;
            Size active_connections;
            std::map<String, ConnectionMonitor::ConnectionStats> connection_stats;
        };

        ManagerStats get_stats() const;

    private:
        DatabaseConnectionManager() = default;
        ~DatabaseConnectionManager();

        String generate_connection_id(const DatabaseConnectionConfig &config) const;

        mutable std::mutex mutex_;
        std::map<String, std::shared_ptr<DatabaseConnection>> connections_;
    };

    // RAII 风格的短期连接持有者，析构时自动释放或关闭
    class ScopedConnection
    {
    public:
        explicit ScopedConnection(std::shared_ptr<DatabaseConnection> connection);

        explicit ScopedConnection(const DatabaseConnectionConfig &config);

        ~ScopedConnection();

        DatabaseConnection *operator->() { return connection_.get(); }
        const DatabaseConnection *operator->() const { return connection_.get(); }
        DatabaseConnection &operator*() { return *connection_; }
        const DatabaseConnection &operator*() const { return *connection_; }
        std::shared_ptr<DatabaseConnection> get() { return connection_; }

    private:
        std::shared_ptr<DatabaseConnection> connection_;
    };


    // 作用域事务：在构造时开始事务，析构时自动回滚（除非 commit）
    class ScopedTransaction
    {
    public:
        explicit ScopedTransaction(DatabaseConnection &connection);

        ~ScopedTransaction();

        void commit();

        ScopedTransaction(const ScopedTransaction &) = delete;
        ScopedTransaction &operator=(const ScopedTransaction &) = delete;
        ScopedTransaction(ScopedTransaction &&) = delete;
        ScopedTransaction &operator=(ScopedTransaction &&) = delete;

    private:
        DatabaseConnection &connection_;
        Bool committed_;
    };

#define MINIORM_SCOPED_CONNECTION(config_var, connection_var) \
    miniorm::ScopedConnection connection_var(config_var)

#define MINIORM_SCOPED_TRANSACTION(connection_var, transaction_var) \
    miniorm::ScopedTransaction transaction_var(connection_var)

#define MINIORM_CHECK_CONNECTION(connection_ptr)                                            \
    do                                                                                      \
    {                                                                                       \
        if (!(connection_ptr) || !(connection_ptr)->is_open())                              \
        {                                                                                   \
            throw miniorm::DatabaseConnectionException("Database connection is not valid"); \
        }                                                                                   \
    } while (false)

}

#endif