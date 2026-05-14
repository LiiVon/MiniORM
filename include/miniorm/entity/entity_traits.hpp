// entity_traits.hpp - 定义实体类的基本特征和接口，提供默认实现，支持用户自定义实体类

#ifndef MINIORM_ENTITY_ENTITY_TRAITS_HPP
#define MINIORM_ENTITY_ENTITY_TRAITS_HPP

#include "reflection.hpp"

namespace miniorm
{
    template <typename T>
    class EntityManager;

    // 实体基类，提供基本的接口和默认实现，用户实体类可以继承自该类并提供特化的 EntityMetadata
    template <typename Derived>
    class Entity
    {
    public:
        static StringView table_name()
        {
            return EntityMetadata<Derived>::table_name();
        }

        static StringView primary_key_name()
        {
            return EntityMetadata<Derived>::primary_key_name();
        }

        template <typename Manager>
        Bool save(Manager &manager)
        {
            return manager.save(static_cast<Derived &>(*this));
        }

        template <typename Manager>
        Bool remove(Manager &manager)
        {
            return manager.remove(static_cast<Derived &>(*this));
        }
    };

    template <typename T>
    concept EntityModel = EntityType<T> && HasEntityMetadata<T>;
}

#endif
