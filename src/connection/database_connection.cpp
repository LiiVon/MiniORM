#include "../../include/miniorm/connection/database_connection.hpp"

namespace miniorm
{
    DatabaseConnectionException::DatabaseConnectionException(const String &msg, const String &connection_info, int32 error_code)
        : DatabaseException(msg, error_code, ""), connection_info_(connection_info)
    {
    }

    DatabaseConnectionException DatabaseConnectionException::connection_timeout(const String &connection_info, int32 timeout_seconds)
    {
        return DatabaseConnectionException(
            StringFormatter::format("Connection timed out after {} seconds", timeout_seconds),
            connection_info, -1);
    }

    DatabaseConnectionException DatabaseConnectionException::pool_exhausted(const String &connection_info, Size pool_size)
    {
        return DatabaseConnectionException(
            StringFormatter::format("Connection pool exhausted (pool size: {})", pool_size),
            connection_info, -2);
    }

    String DatabaseConnectionException::detailed_message() const
    {
        return StringFormatter::format("Database connection error: {} (code: {}) Connection Info: {}",
                                       what(), error_code(), connection_info_);
    }

    Bool DatabaseConnectionConfig::validate() const noexcept
    {
        if (!DatabaseConfig::validate())
        {
            return false;
        }

        if (pool_config.enable_pool)
        {
            if (pool_config.min_connections == 0 || pool_config.max_connections == 0)
            {
                return false;
            }
            if (pool_config.min_connections > pool_config.max_connections)
            {
                return false;
            }
        }

        if (behavior_config.max_retries == 0 && behavior_config.auto_reconnect)
        {
            return false;
        }
        return true;
    }

    String DatabaseConnectionConfig::summary() const
    {
        return StringFormatter::format(
            "DatabaseConnectionConfig: type={}, host={}, port={}, database={}, username={}, options={}, "
            "PoolConfig: enable_pool={}, min_connections={}, max_connections={}, idle_timeout_seconds={}, connection_timeout_seconds={}, test_on_borrow={}, test_on_return={}; "
            "BehaviorConfig: auto_reconnect={}, max_retries={}, reconnect_delay_ms={}, enable_query_cache={}, query_cache_size={}, enable_statement_cache={}, statement_cache_size={}; "
            "PerformanceConfig: enable_batch_operations={}, batch_size={}, enable_prepared_statements={}, enable_connection_pooling={}",
            database_type_name(type), host, port, database, username, options,
            pool_config.enable_pool, pool_config.min_connections, pool_config.max_connections,
            pool_config.idle_timeout_seconds, pool_config.connection_timeout_seconds, pool_config.test_on_borrow, pool_config.test_on_return,
            behavior_config.auto_reconnect, behavior_config.max_retries, behavior_config.reconnect_delay_ms,
            behavior_config.enable_query_cache, behavior_config.query_cache_size, behavior_config.enable_statement_cache, behavior_config.statement_cache_size,
            performance_config.enable_batch_operations, performance_config.batch_size, performance_config.enable_prepared_statements, performance_config.enable_connection_pooling);
    }

    String ConnectionMonitor::ConnectionStats::generate_report() const
    {
        return StringFormatter::format(
            "Connections: total={}, active={}, idle={}, failed={}\n"
            "Queries: total={}, success={}, failed={}, avg_time={}ms\n"
            "Updates: total={}, success={}, failed={}, avg_time={}ms",
            total_connections, active_connections, idle_connections, failed_connections,
            total_queries, successful_queries, failed_queries,
            total_queries > 0 ? total_query_time.count() / total_queries : 0,
            total_updates, successful_updates, failed_updates,
            total_updates > 0 ? total_update_time.count() / total_updates : 0);
    }

    ConnectionMonitor &ConnectionMonitor::instance()
    {
        static ConnectionMonitor monitor;
        return monitor;
    }

    void ConnectionMonitor::record_connection_created()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stats_.total_connections++;
    }

    void ConnectionMonitor::record_connection_closed()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stats_.total_connections > 0)
        {
            stats_.total_connections--;
        }
    }

    void ConnectionMonitor::record_connection_failed()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stats_.failed_connections++;
    }

    void ConnectionMonitor::record_query_started()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stats_.total_queries++;
        stats_.active_connections++;
    }

    void ConnectionMonitor::record_query_completed(std::chrono::milliseconds duration, Bool success)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (success)
        {
            stats_.successful_queries++;
        }
        else
        {
            stats_.failed_queries++;
        }

        stats_.total_query_time += duration;
        if (duration > stats_.max_query_time)
        {
            stats_.max_query_time = duration;
        }

        if (stats_.active_connections > 0)
        {
            stats_.active_connections--;
        }
    }

    void ConnectionMonitor::record_error(int32 error_code)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stats_.error_codes[error_code]++;
    }

    ConnectionMonitor::ConnectionStats ConnectionMonitor::get_stats() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return stats_;
    }

    void ConnectionMonitor::reset_stats()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stats_ = ConnectionStats{};
    }
}
