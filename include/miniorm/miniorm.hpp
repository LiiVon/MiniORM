// miniorm.hpp - MiniORM 主头文件，包含所有核心组件和功能的统一接口

#ifndef MINIORM_MINIORM_HPP
#define MINIORM_MINIORM_HPP

#include "core/config.hpp"
#include "core/concepts.hpp"
#include "core/traits.hpp"
#include "core/utils.hpp"
#include "adapter/adapter.hpp"
#include "connection/database_connection.hpp"
#include "connection/connection_pool.hpp"
#include "connection/transaction.hpp"
#include "query/condition.hpp"
#include "query/query_builder.hpp"
#include "entity/reflection.hpp"
#include "entity/entity_traits.hpp"
#include "entity/entity_manager.hpp"

namespace miniorm
{
    class MiniORM
    {
    public:
        explicit MiniORM(std::shared_ptr<IDatabaseConnection> connection)
            : connection_(std::move(connection))
        {
            if (!connection_)
            {
                throw std::invalid_argument("MiniORM requires a valid database connection");
            }
        }

        explicit MiniORM(const DatabaseConnectionConfig &config)
            : connection_(DatabaseAdapterFactory::create(config))
        {
        }

        template <EntityModel T>
        EntityManager<T> get_entity_manager()
        {
            return EntityManager<T>(connection_);
        }

        std::shared_ptr<IDatabaseConnection> connection() const
        {
            return connection_;
        }

        template <typename Func>
        auto transaction(Func &&func)
        {
            if (!connection_->begin_transaction())
            {
                throw DatabaseException("Failed to begin transaction");
            }

            try
            {
                using ResultT = std::invoke_result_t<Func>;
                if constexpr (std::is_void_v<ResultT>)
                {
                    func();
                    if (!connection_->commit())
                    {
                        throw DatabaseException("Failed to commit transaction");
                    }
                    return;
                }
                else
                {
                    auto result = func();
                    if (!connection_->commit())
                    {
                        throw DatabaseException("Failed to commit transaction");
                    }
                    return result;
                }
            }
            catch (...)
            {
                connection_->rollback();
                throw;
            }
        }

    private:
        std::shared_ptr<IDatabaseConnection> connection_;
    };
}

#endif
