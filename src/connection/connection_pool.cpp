#include "../../include/miniorm/adapter/adapter.hpp"

namespace miniorm
{
    DatabaseConnectionPool::DatabaseConnectionPool(const DatabaseConfig &config, const PoolConfig &poo_config)
        : config_(config), pool_config_(poo_config), total_connections_(0), active_connections_(0)
    {
        initialize_pool();
    }

    DatabaseConnectionPool::~DatabaseConnectionPool()
    {
        shutdown();
    }

    std::shared_ptr<IDatabaseConnection> DatabaseConnectionPool::acquire()
    {
        std::unique_lock<std::mutex> lock(mutex_);

        struct WaitingThreadGuard
        {
            Size &counter;

            explicit WaitingThreadGuard(Size &c) : counter(c)
            {
                ++counter;
            }

            ~WaitingThreadGuard()
            {
                --counter;
            }
        };

        WaitingThreadGuard waiting_guard(waiting_threads_);
        cv_.wait(lock, [this]()
                 { return !idle_connections_.empty() || total_connections_ < pool_config_.max_connections; });

        if (!idle_connections_.empty())
        {
            auto conn = std::move(idle_connections_.back());
            idle_connections_.pop_back();

            if (pool_config_.test_on_borrow && !test_connection(*conn->connection))
            {
                conn = create_connection();
            }
            active_connections_.push_back(conn);
            return conn->connection;
        }

        if (total_connections_ < pool_config_.max_connections)
        {
            auto conn = create_connection();
            active_connections_.push_back(conn);
            return conn->connection;
        }

        throw DatabaseException("Failed to acquire database connection: pool is at maximum capacity");
    }

    void DatabaseConnectionPool::release(std::shared_ptr<IDatabaseConnection> conn)
    {
        std::unique_lock<std::mutex> lock(mutex_);

        auto it = std::find_if(active_connections_.begin(), active_connections_.end(),
                               [&conn](const std::shared_ptr<PooledConnection> &pooled)
                               { return pooled->connection == conn; });
        if (it != active_connections_.end())
        {
            auto pooled = std::move(*it);
            active_connections_.erase(it);

            if (pool_config_.test_on_return && !test_connection(*pooled->connection))
            {
                total_connections_--;
            }
            else
            {
                pooled->last_used_time = std::chrono::steady_clock::now();
                idle_connections_.push_back(std::move(pooled));
            }
            cv_.notify_one();
        }
    }

    DatabaseConnectionPool::PoolStats DatabaseConnectionPool::get_stats() const
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return PoolStats{
            total_connections_,
            idle_connections_.size(),
            active_connections_.size(),
            waiting_threads_};
    }

    void DatabaseConnectionPool::cleanup_idle_connections()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        auto now = std::chrono::steady_clock::now();

        auto it = idle_connections_.begin();
        while (it != idle_connections_.end())
        {
            auto idle_time = std::chrono::duration_cast<std::chrono::seconds>(now - (*it)->last_used_time).count();

            if (idle_time >= pool_config_.idle_timeout_seconds)
            {
                it = idle_connections_.erase(it);
                total_connections_--;
            }
            else
            {
                ++it;
            }
        }
    }

    void DatabaseConnectionPool::initialize_pool()
    {
        for (Size i = 0; i < pool_config_.min_connections; ++i)
        {
            auto conn = create_connection();
            idle_connections_.push_back(std::move(conn));
        }
    }

    std::shared_ptr<DatabaseConnectionPool::PooledConnection> DatabaseConnectionPool::create_connection()
    {
        try
        {
            auto conn = DatabaseAdapterFactory::create(config_);
            total_connections_++;
            return std::make_shared<PooledConnection>(std::move(conn));
        }
        catch (const std::exception &e)
        {
            Logger::error("Failed to create database connection: {}", e.what());
            throw;
        }
    }

    Bool DatabaseConnectionPool::test_connection(IDatabaseConnection &conn)
    {
        try
        {
            auto result = conn.execute_query("SELECT 1");
            return result != nullptr;
        }
        catch (...)
        {
            return false;
        }
    }

    void DatabaseConnectionPool::shutdown()
    {
        std::unique_lock<std::mutex> lock(mutex_);

        for (auto &conn : idle_connections_)
        {
            conn->connection->disconnect();
        }

        for (auto &conn : active_connections_)
        {
            conn->connection->disconnect();
        }

        idle_connections_.clear();
        active_connections_.clear();
        total_connections_ = 0;
    }
}
