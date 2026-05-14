

#ifndef MINIORM_CONNECTION_CONNECTION_POOL_HPP
#define MINIORM_CONNECTION_CONNECTION_POOL_HPP

#include "../adapter/adapter.hpp"

namespace miniorm
{
    using ConnectionPool = DatabaseConnectionPool;
    using ConnectionPoolConfig = DatabaseConnectionPool::PoolConfig;
    using ConnectionPoolStats = DatabaseConnectionPool::PoolStats;
}

#endif
