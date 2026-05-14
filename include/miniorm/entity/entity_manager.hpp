// entity_manager.hpp - 定义 EntityManager 类
// 提供基本的 CRUD 操作和查询接口，使用实体元数据进行自动映射

#ifndef MINIORM_ENTITY_ENTITY_MANAGER_HPP
#define MINIORM_ENTITY_ENTITY_MANAGER_HPP

#include "entity_traits.hpp"
#include "../query/query_builder.hpp"

namespace miniorm
{
    // 实体管理器模板类，提供基本的 CRUD 操作和查询接口
    template <typename T>
    class EntityManager
    {
    public:
        using Traits = EntityMetadata<T>;
        using PrimaryKeyType = typename Traits::PrimaryKeyType;

        static_assert(EntityModel<T>, "EntityManager requires a valid entity model");

        explicit EntityManager(std::shared_ptr<IDatabaseConnection> connection)
            : connection_(std::move(connection))
        {
            if (!connection_)
            {
                throw std::invalid_argument("EntityManager requires a valid database connection");
            }
        }

        Bool save(const T &entity)
        {
            if (Traits::is_new(entity))
            {
                return insert(entity);
            }
            return update(entity);
        }

        Bool remove(const T &entity)
        {
            QueryBuilder builder;
            builder.delete_from(Traits::table_name())
                .where(condition(Traits::primary_key_name()).eq(Traits::primary_key_value(entity)));

            return execute_update(builder.build()) > 0;
        }

        std::optional<T> find_by_primary_key(const PrimaryKeyType &primary_key)
        {
            QueryBuilder builder;
            builder.select({"*"})
                .from(Traits::table_name())
                .where(condition(Traits::primary_key_name()).eq(primary_key));

            auto result = connection_->execute_query(builder.build());
            if (result && result->next())
            {
                return Traits::deserialize(result->current_row());
            }
            return std::nullopt;
        }

        std::vector<T> find_all()
        {
            QueryBuilder builder;
            builder.select({"*"}).from(Traits::table_name());

            auto result = connection_->execute_query(builder.build());
            std::vector<T> items;
            if (!result)
            {
                return items;
            }

            while (result->next())
            {
                items.push_back(Traits::deserialize(result->current_row()));
            }
            return items;
        }

        Bool exists(const PrimaryKeyType &primary_key)
        {
            return find_by_primary_key(primary_key).has_value();
        }

        int32 count()
        {
            auto result = connection_->execute_query(StringFormatter::format("SELECT COUNT(*) FROM {}", SqlStringEscaper::escape_identifier(Traits::table_name())));
            if (result && result->next())
            {
                return FromString<int32>::parse(result->current_row().template get<String>(0));
            }
            return 0;
        }

        std::shared_ptr<IDatabaseConnection> connection() const
        {
            return connection_;
        }

    private:
        Bool insert(const T &entity)
        {
            auto columns = Traits::field_names();
            auto values = Traits::serialize(entity);

            QueryBuilder builder;
            builder.insert_into(Traits::table_name());
            for (Size i = 0; i < columns.size(); ++i)
            {
                builder.value_raw(columns[i], values[i]);
            }
            return execute_update(builder.build()) > 0;
        }

        Bool update(const T &entity)
        {
            auto columns = Traits::field_names();
            auto values = Traits::serialize(entity);
            auto primary_key = Traits::primary_key_name();
            auto primary_key_value = Traits::primary_key_value(entity);

            QueryBuilder builder;
            builder.update(Traits::table_name());
            for (Size i = 0; i < columns.size(); ++i)
            {
                if (columns[i] == primary_key)
                {
                    continue;
                }
                builder.set_raw(columns[i], values[i]);
            }
            builder.where(condition(primary_key).eq(primary_key_value));
            return execute_update(builder.build()) > 0;
        }

        int32 execute_update(const String &sql)
        {
            return connection_->execute_update(sql);
        }

    private:
        std::shared_ptr<IDatabaseConnection> connection_;
    };
}

#endif
