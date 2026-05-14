#include "../../include/miniorm/adapter/adapter.hpp"

#ifdef MINIORM_HAS_MYSQL_CLIENT
#include <mysql/mysql.h>
#endif

namespace miniorm
{
    Bool DatabaseConfig::validate() const noexcept
    {
        if (type == DatabaseType::Unknown)
        {
            return false;
        }
        if (database.empty())
        {
            return false;
        }

        switch (type)
        {
        case DatabaseType::SQLite:
            return !database.empty();
        case DatabaseType::MySQL:
        case DatabaseType::PostgreSQL:
        case DatabaseType::SQLServer:
            return !host.empty() && port > 0 && !username.empty() && !password.empty();
        default:
            return false;
        }
    }

    String DatabaseConfig::connection_string() const
    {
        switch (type)
        {
        case DatabaseType::SQLite:
            return StringFormatter::format("sqlite://{}", database);
        case DatabaseType::MySQL:
            return StringFormatter::format("mysql://{}:{}@{}:{}/{}?{}", username, password, host, port, database, options);
        case DatabaseType::PostgreSQL:
            return StringFormatter::format("postgresql://{}:{}@{}:{}/{}?{}", username, password, host, port, database, options);
        case DatabaseType::SQLServer:
            return StringFormatter::format("sqlserver://{}:{}@{}:{}/{}?{}", username, password, host, port, database, options);
        default:
            return "";
        }
    }

    DatabaseException::DatabaseException(const String &msg, int32 error_code, const String &sql)
        : std::runtime_error(msg), error_code_(error_code), sql_(sql)
    {
    }

    String DatabaseException::detailed_message() const
    {
        return StringFormatter::format("Database error: {} (code: {}) SQL: {}", what(), error_code_, sql_);
    }

    std::unique_ptr<IDatabaseConnection> DatabaseAdapterFactory::create(DatabaseType type)
    {
        switch (type)
        {
        case DatabaseType::SQLite:
            return create_sqlite_adapter();
        case DatabaseType::MySQL:
            return create_mysql_adapter();
        case DatabaseType::PostgreSQL:
            return create_postgresql_adapter();
        case DatabaseType::SQLServer:
            return create_sqlserver_adapter();
        default:
            throw std::invalid_argument(StringFormatter::format("Unsupported database type: {}", static_cast<int32>(type)));
        }
    }

    std::unique_ptr<IDatabaseConnection> DatabaseAdapterFactory::create(const DatabaseConfig &config)
    {
        auto adapter = create(config.type);
        if (!adapter->connect(config))
        {
            throw DatabaseException(StringFormatter::format("Failed to connect to database with config: {}", config.connection_string()));
        }
        return adapter;
    }

    std::unique_ptr<IDatabaseConnection> DatabaseAdapterFactory::create_from_connection_string(const StringView &connection_string)
    {
        auto config = parse_connection_string(connection_string);
        return create(config);
    }

    DatabaseConfig DatabaseAdapterFactory::parse_connection_string(const StringView &connection_string)
    {
        DatabaseConfig config;

        if (connection_string.starts_with("sqlite://"))
        {
            config.type = DatabaseType::SQLite;
            config.database = String(connection_string.substr(9));
        }
        else if (connection_string.starts_with("mysql://"))
        {
            config.type = DatabaseType::MySQL;
            parse_standard_connection_string(connection_string, config);
        }
        else if (connection_string.starts_with("postgresql://"))
        {
            config.type = DatabaseType::PostgreSQL;
            parse_standard_connection_string(connection_string, config);
        }
        else if (connection_string.starts_with("sqlserver://"))
        {
            config.type = DatabaseType::SQLServer;
            parse_standard_connection_string(connection_string, config);
        }
        else
        {
            throw std::invalid_argument(StringFormatter::format("Unsupported connection string format: {}", connection_string));
        }
        return config;
    }

    void DatabaseAdapterFactory::parse_standard_connection_string(const StringView &connection_string, DatabaseConfig &config)
    {
        auto protocol_end = connection_string.find("://");
        if (protocol_end == StringView::npos)
        {
            throw std::invalid_argument(StringFormatter::format("Invalid connection string format: {}", connection_string));
        }

        auto credentials_start = protocol_end + 3;
        auto at_pos = connection_string.find('@', credentials_start);
        if (at_pos == StringView::npos)
        {
            throw std::invalid_argument(StringFormatter::format("Invalid connection string format: {}", connection_string));
        }

        auto host_start = at_pos + 1;
        auto colon_pos = connection_string.find(':', host_start);
        if (colon_pos == StringView::npos)
        {
            throw std::invalid_argument(StringFormatter::format("Invalid connection string format: {}", connection_string));
        }

        auto slash_pos = connection_string.find('/', colon_pos);
        if (slash_pos == StringView::npos)
        {
            throw std::invalid_argument(StringFormatter::format("Invalid connection string format: {}", connection_string));
        }

        auto question_pos = connection_string.find('?', slash_pos);

        config.username = String(connection_string.substr(credentials_start, at_pos - credentials_start));
        config.password = String(connection_string.substr(at_pos + 1, host_start - (at_pos + 1)));
        config.host = String(connection_string.substr(host_start, colon_pos - host_start));
        config.port = std::stoi(String(connection_string.substr(colon_pos + 1, slash_pos - (colon_pos + 1))));
        config.database = String(connection_string.substr(slash_pos + 1, question_pos - (slash_pos + 1)));

        if (question_pos != StringView::npos)
        {
            config.options = String(connection_string.substr(question_pos + 1));
        }
    }

#ifdef MINIORM_HAS_MYSQL_CLIENT
    namespace
    {
        String mysql_escape_identifier(const StringView &identifier)
        {
            String result;
            result.reserve(identifier.size() + 2);
            result += '`';
            for (char c : identifier)
            {
                if (c == '`')
                {
                    result += "``";
                }
                else
                {
                    result += c;
                }
            }
            result += '`';
            return result;
        }

        String mysql_error_message(MYSQL *handle)
        {
            if (!handle)
            {
                return "Unknown MySQL error";
            }
            return StringFormatter::format("{} (code: {})", mysql_error(handle), mysql_errno(handle));
        }

        String substitute_sql_fragments(String sql, const std::vector<String> &bindings)
        {
            Size search_from = 0;
            for (const auto &binding : bindings)
            {
                Size placeholder = sql.find("{}", search_from);
                if (placeholder == String::npos)
                {
                    break;
                }
                sql.replace(placeholder, 2, binding);
                search_from = placeholder + binding.size();
            }
            return sql;
        }

        class MySQLResultRow : public IResultRow
        {
        public:
            MySQLResultRow(std::vector<String> values, std::vector<String> names, std::vector<Bool> null_flags)
                : values_(std::move(values)), names_(std::move(names)), null_flags_(std::move(null_flags))
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
            std::vector<String> names_;
            std::vector<Bool> null_flags_;
        };

        class MySQLResultSet : public IResultSet
        {
        public:
            MySQLResultSet(std::vector<std::vector<String>> rows, std::vector<String> names)
                : rows_(std::move(rows)), names_(std::move(names))
            {
            }

            Size row_count() const override { return rows_.size(); }
            Size column_count() const override { return names_.size(); }
            String column_name(Size index) const override { return names_.at(index); }

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
                current_row_cache_ = MySQLResultRow(rows_.at(cursor_ - 1), names_, std::vector<Bool>(rows_.at(cursor_ - 1).size(), false));
                return current_row_cache_;
            }

        private:
            std::unique_ptr<IResultSet> clone() const override
            {
                return std::make_unique<MySQLResultSet>(rows_, names_);
            }

        private:
            std::vector<std::vector<String>> rows_;
            std::vector<String> names_;
            Size cursor_ = 0;
            mutable MySQLResultRow current_row_cache_{std::vector<String>{}, std::vector<String>{}, std::vector<Bool>{}};
        };

        std::unique_ptr<IResultSet> mysql_execute_query(MYSQL *handle, const StringView &sql)
        {
            if (mysql_real_query(handle, sql.data(), static_cast<unsigned long>(sql.size())) != 0)
            {
                throw DatabaseException(mysql_error_message(handle), static_cast<int32>(mysql_errno(handle)), String(sql));
            }

            MYSQL_RES *result = mysql_store_result(handle);
            if (!result)
            {
                if (mysql_field_count(handle) == 0)
                {
                    return std::make_unique<MySQLResultSet>(std::vector<std::vector<String>>{}, std::vector<String>{});
                }
                throw DatabaseException(mysql_error_message(handle), static_cast<int32>(mysql_errno(handle)), String(sql));
            }

            const unsigned int field_count = mysql_num_fields(result);
            MYSQL_FIELD *fields = mysql_fetch_fields(result);

            std::vector<String> names;
            names.reserve(field_count);
            for (unsigned int i = 0; i < field_count; ++i)
            {
                names.emplace_back(fields[i].name ? fields[i].name : "");
            }

            std::vector<std::vector<String>> rows;
            MYSQL_ROW row = nullptr;
            while ((row = mysql_fetch_row(result)) != nullptr)
            {
                unsigned long *lengths = mysql_fetch_lengths(result);
                std::vector<String> values;
                values.reserve(field_count);
                for (unsigned int i = 0; i < field_count; ++i)
                {
                    if (row[i])
                    {
                        values.emplace_back(row[i], lengths[i]);
                    }
                    else
                    {
                        values.emplace_back();
                    }
                }
                rows.push_back(std::move(values));
            }

            mysql_free_result(result);
            return std::make_unique<MySQLResultSet>(std::move(rows), std::move(names));
        }

        int32 mysql_execute_update(MYSQL *handle, const StringView &sql)
        {
            if (mysql_real_query(handle, sql.data(), static_cast<unsigned long>(sql.size())) != 0)
            {
                throw DatabaseException(mysql_error_message(handle), static_cast<int32>(mysql_errno(handle)), String(sql));
            }

            if (MYSQL_RES *result = mysql_store_result(handle))
            {
                mysql_free_result(result);
            }

            my_ulonglong affected = mysql_affected_rows(handle);
            if (affected == static_cast<my_ulonglong>(-1))
            {
                return 0;
            }
            return static_cast<int32>(affected);
        }

        class MySQLStatement : public IStatement
        {
        public:
            using QueryExecutor = std::function<std::unique_ptr<IResultSet>(const String &)>;
            using UpdateExecutor = std::function<int32(const String &)>;

            MySQLStatement(String sql, QueryExecutor query_executor, UpdateExecutor update_executor)
                : sql_(std::move(sql)), query_executor_(std::move(query_executor)), update_executor_(std::move(update_executor))
            {
            }

            const String sql() const override { return sql_; }

            void bind_parameters(Size index, const StringView &params) override
            {
                if (index >= bindings_.size())
                {
                    bindings_.resize(index + 1);
                }
                bindings_[index] = String(params);
            }

            void bind_null(Size index) override
            {
                if (index >= bindings_.size())
                {
                    bindings_.resize(index + 1);
                }
                bindings_[index] = "NULL";
            }

            std::unique_ptr<IResultSet> execute_query() override
            {
                return query_executor_(substitute_sql_fragments(sql_, bindings_));
            }

            int32 execute_update() override
            {
                return update_executor_(substitute_sql_fragments(sql_, bindings_));
            }

            void reset() override
            {
                clear_bindings();
            }

            void clear_bindings() override
            {
                bindings_.clear();
            }

        private:
            String sql_;
            std::vector<String> bindings_;
            QueryExecutor query_executor_;
            UpdateExecutor update_executor_;
        };

        // MySQL 数据库适配器
        class MySQLDatabaseAdapter : public DatabaseAdapter
        {
        public:
            MySQLDatabaseAdapter()
                : DatabaseAdapter(DatabaseType::MySQL)
            {
            }

            ~MySQLDatabaseAdapter() override
            {
                do_disconnect();
            }

            String escape_identifier(const StringView &identifier) const override
            {
                return mysql_escape_identifier(identifier);
            }

        protected:
            Bool do_connect(const DatabaseConfig &config) override
            {
                config_ = config;
                connection_ = mysql_init(nullptr);
                if (!connection_) {
                    throw DatabaseException("Failed to initialize MySQL client");
                }

                unsigned int timeout = 5;
                mysql_options(connection_, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);

                String charset = "utf8mb4";
                if (!config.options.empty())
                {
                    auto charset_pos = config.options.find("charset=");
                    if (charset_pos != String::npos)
                    {
                        Size charset_start = charset_pos + 8;
                        Size charset_end = config.options.find_first_of("&; ", charset_start);
                        charset = config.options.substr(charset_start, charset_end == String::npos ? String::npos : charset_end - charset_start);
                    }
                }
                mysql_options(connection_, MYSQL_SET_CHARSET_NAME, charset.c_str());

                if (!mysql_real_connect(connection_, config.host.c_str(), config.username.c_str(), config.password.c_str(), nullptr, static_cast<unsigned int>(config.port), nullptr, 0))
                {
                    throw DatabaseException(mysql_error_message(connection_), static_cast<int32>(mysql_errno(connection_)));
                }

                if (!config.database.empty())
                {
                    auto create_database_sql = StringFormatter::format("CREATE DATABASE IF NOT EXISTS {}", mysql_escape_identifier(config.database));
                    mysql_execute_update(connection_, create_database_sql);

                    if (mysql_select_db(connection_, config.database.c_str()) != 0)
                    {
                        throw DatabaseException(mysql_error_message(connection_), static_cast<int32>(mysql_errno(connection_)), config.database);
                    }
                }

                return true;
            }

            void do_disconnect() override
            {
                if (connection_)
                {
                    mysql_close(connection_);
                    connection_ = nullptr;
                }
            }

            Bool do_begin_transaction() override
            {
                return mysql_execute_update(connection_, "START TRANSACTION") >= 0;
            }

            Bool do_commit() override
            {
                return mysql_commit(connection_) == 0;
            }

            Bool do_rollback() override
            {
                return mysql_rollback(connection_) == 0;
            }

            std::unique_ptr<IStatement> do_prepare_statement(const StringView &sql) override
            {
                return std::make_unique<MySQLStatement>(String(sql),
                                                        [this](const String &executed_sql) {
                                                            return execute_query_raw(executed_sql);
                                                        },
                                                        [this](const String &executed_sql) {
                                                            return execute_update_raw(executed_sql);
                                                        });
            }

            std::unique_ptr<IResultSet> do_execute_query(const StringView &sql) override
            {
                return execute_query_raw(String(sql));
            }

            int32 do_execute_update(const StringView &sql) override
            {
                return execute_update_raw(String(sql));
            }

            int32 do_execute_batch(const std::vector<StringView> &sql_statements) override
            {
                int32 total = 0;
                for (const auto &sql : sql_statements)
                {
                    total += mysql_execute_update(connection_, sql);
                }
                return total;
            }

            String do_database_version() const override
            {
                return mysql_get_server_info(connection_);
            }

            std::vector<String> do_get_table_names() override
            {
                auto result = mysql_execute_query(connection_, "SHOW TABLES");
                return result->template fetch_column<String>(0);
            }

            std::vector<String> do_get_column_names(const StringView &table_name) override
            {
                auto sql = StringFormatter::format("SHOW COLUMNS FROM {}", mysql_escape_identifier(table_name));
                auto result = mysql_execute_query(connection_, sql);
                return result->template fetch_column<String>(0);
            }

            int64 do_last_insert_id() const override
            {
                return static_cast<int64>(mysql_insert_id(connection_));
            }

            int32 do_affected_rows() const override
            {
                my_ulonglong affected = mysql_affected_rows(connection_);
                if (affected == static_cast<my_ulonglong>(-1))
                {
                    return 0;
                }
                return static_cast<int32>(affected);
            }

        private:
            std::unique_ptr<IResultSet> execute_query_raw(const String &sql)
            {
                return mysql_execute_query(connection_, sql);
            }

            int32 execute_update_raw(const String &sql)
            {
                return mysql_execute_update(connection_, sql);
            }

            MYSQL *connection_ = nullptr;
            DatabaseConfig config_;
        };
    }
#endif

    namespace
    {
         // 内存适配器 --模拟实现
        class MemoryResultRow : public IResultRow
        {
        public:
            MemoryResultRow(std::vector<String> values, std::vector<String> names)
                : values_(std::move(values)), names_(std::move(names)), null_flags_(values_.size(), false)
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
            std::vector<String> names_;
            std::vector<Bool> null_flags_;
        };

        class MemoryResultSet : public IResultSet
        {
        public:
            MemoryResultSet(std::vector<std::vector<String>> rows, std::vector<String> names)
                : rows_(std::move(rows)), names_(std::move(names))
            {
            }

            Size row_count() const override { return rows_.size(); }
            Size column_count() const override { return names_.size(); }
            String column_name(Size index) const override { return names_.at(index); }

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
                current_row_cache_ = MemoryResultRow(rows_.at(cursor_ - 1), names_);
                return current_row_cache_;
            }

        private:
            std::unique_ptr<IResultSet> clone() const override
            {
                return std::make_unique<MemoryResultSet>(rows_, names_);
            }

        private:
            std::vector<std::vector<String>> rows_;
            std::vector<String> names_;
            Size cursor_ = 0;
            mutable MemoryResultRow current_row_cache_{std::vector<String>{}, std::vector<String>{}};
        };

        class MemoryStatement : public IStatement
        {
        public:
            explicit MemoryStatement(String sql) : sql_(std::move(sql)) {}

            const String sql() const override { return sql_; }
            void bind_parameters(Size, const StringView &) override {}
            void bind_null(Size) override {}

            std::unique_ptr<IResultSet> execute_query() override
            {
                return std::make_unique<MemoryResultSet>(std::vector<std::vector<String>>{{"1"}}, std::vector<String>{"value"});
            }

            int32 execute_update() override { return 1; }
            void reset() override {}
            void clear_bindings() override {}

        private:
            String sql_;
        };

        class MemoryDatabaseAdapter : public DatabaseAdapter
        {
        public:
            explicit MemoryDatabaseAdapter(DatabaseType type)
                : DatabaseAdapter(type)
            {
            }

        protected:
            Bool do_connect(const DatabaseConfig &) override { return true; }
            void do_disconnect() override {}
            Bool do_begin_transaction() override { return true; }
            Bool do_commit() override { return true; }
            Bool do_rollback() override { return true; }

            std::unique_ptr<IStatement> do_prepare_statement(const StringView &sql) override
            {
                return std::make_unique<MemoryStatement>(String(sql));
            }

            std::unique_ptr<IResultSet> do_execute_query(const StringView &) override
            {
                return std::make_unique<MemoryResultSet>(std::vector<std::vector<String>>{{"1"}}, std::vector<String>{"value"});
            }

            int32 do_execute_update(const StringView &) override { return 1; }
            int32 do_execute_batch(const std::vector<StringView> &sql_statements) override { return static_cast<int32>(sql_statements.size()); }
            String do_database_version() const override { return "memory-1.0"; }
            std::vector<String> do_get_table_names() override { return {}; }
            std::vector<String> do_get_column_names(const StringView &) override { return {}; }
            int64 do_last_insert_id() const override { return 1; }
            int32 do_affected_rows() const override { return 1; }
        };
    }

    std::unique_ptr<IDatabaseConnection> DatabaseAdapterFactory::create_sqlite_adapter()
    {
        return std::make_unique<MemoryDatabaseAdapter>(DatabaseType::SQLite);
    }

    std::unique_ptr<IDatabaseConnection> DatabaseAdapterFactory::create_mysql_adapter()
    {
    #ifdef MINIORM_HAS_MYSQL_CLIENT
        return std::make_unique<MySQLDatabaseAdapter>();
    #else
        return std::make_unique<MemoryDatabaseAdapter>(DatabaseType::MySQL);
    #endif
    }

    std::unique_ptr<IDatabaseConnection> DatabaseAdapterFactory::create_postgresql_adapter()
    {
        return std::make_unique<MemoryDatabaseAdapter>(DatabaseType::PostgreSQL);
    }

    std::unique_ptr<IDatabaseConnection> DatabaseAdapterFactory::create_sqlserver_adapter()
    {
        return std::make_unique<MemoryDatabaseAdapter>(DatabaseType::SQLServer);
    }
}
