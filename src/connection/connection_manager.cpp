#include "../../include/miniorm/connection/database_connection.hpp"

namespace miniorm
{
    DatabaseConnectionManager &DatabaseConnectionManager::instance()
    {
        static DatabaseConnectionManager manager;
        return manager;
    }

    DatabaseConnectionManager::~DatabaseConnectionManager()
    {
        close_all_connections();
    }

    std::shared_ptr<DatabaseConnection> DatabaseConnectionManager::create_connection(const DatabaseConnectionConfig &config)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        String connection_id = generate_connection_id(config);

        auto it = connections_.find(connection_id);
        if (it != connections_.end())
        {
            Logger::debug("Reusing existing connection: {}", connection_id);
            return it->second;
        }

        auto connection = std::make_shared<DatabaseConnection>(config);
        connections_[connection_id] = connection;
        Logger::info("Created new database connection: {}", connection_id);
        return connection;
    }

    std::shared_ptr<DatabaseConnection> DatabaseConnectionManager::get_connection(const String &connection_id)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = connections_.find(connection_id);
        if (it != connections_.end())
        {
            return it->second;
        }
        throw DatabaseConnectionException(StringFormatter::format("Connection not found: {}", connection_id));
    }

    void DatabaseConnectionManager::close_connection(const String &connection_id)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = connections_.find(connection_id);
        if (it != connections_.end())
        {
            it->second->close();
            connections_.erase(it);
            Logger::info("Closed database connection: {}", connection_id);
        }
    }

    void DatabaseConnectionManager::close_all_connections()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        Logger::info("Closing all database connections (total: {})", connections_.size());
        for (auto &[id, connection] : connections_)
        {
            try
            {
                connection->close();
            }
            catch (const std::exception &ex)
            {
                Logger::error("Error closing connection {}: {}", id, ex.what());
            }
        }
        connections_.clear();
    }

    std::vector<String> DatabaseConnectionManager::get_all_connection_ids() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<String> ids;
        ids.reserve(connections_.size());
        for (const auto &[id, _] : connections_)
        {
            ids.push_back(id);
        }
        return ids;
    }

    DatabaseConnectionManager::ManagerStats DatabaseConnectionManager::get_stats() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ManagerStats stats{};
        stats.total_connections = connections_.size();
        stats.active_connections = 0;
        for (const auto &[id, connection] : connections_)
        {
            if (connection->is_open())
            {
                stats.active_connections++;
            }
            stats.connection_stats[id] = connection->get_stats();
        }
        return stats;
    }

    String DatabaseConnectionManager::generate_connection_id(const DatabaseConnectionConfig &config) const
    {
        return StringFormatter::format("{}-{}-{}-{}", database_type_name(config.type), config.host, config.port, config.database);
    }

    ScopedConnection::ScopedConnection(std::shared_ptr<DatabaseConnection> connection)
        : connection_(std::move(connection))
    {
        if (!connection_)
        {
            throw DatabaseConnectionException("ScopedConnection requires a valid connection");
        }
        if (!connection_->is_open())
        {
            connection_->open();
        }
    }

    ScopedConnection::ScopedConnection(const DatabaseConnectionConfig &config)
        : connection_(DatabaseConnectionManager::instance().create_connection(config))
    {
    }

    ScopedConnection::~ScopedConnection()
    {
        if (connection_)
        {
            connection_->close();
        }
    }

    ScopedTransaction::ScopedTransaction(DatabaseConnection &connection)
        : connection_(connection), committed_(false)
    {
        connection_.begin_transaction();
    }

    ScopedTransaction::~ScopedTransaction()
    {
        if (!committed_)
        {
            try
            {
                connection_.rollback();
            }
            catch (...)
            {
                Logger::error("Error rolling back transaction in destructor");
            }
        }
    }

    void ScopedTransaction::commit()
    {
        connection_.commit();
        committed_ = true;
    }
}
