// reflection.hpp - 定义实体类的元数据结构和反射机制，支持自动映射数据库表和字段

#ifndef MINIORM_ENTITY_REFLECTION_HPP
#define MINIORM_ENTITY_REFLECTION_HPP

#include "../core/config.hpp"
#include "../core/concepts.hpp"
#include "../core/traits.hpp"
#include "../core/utils.hpp"
#include "../adapter/adapter.hpp"

#if !MINIORM_CPP20
#error "reflection.hpp requires C++20 support. Please enable C++20 in your compiler settings."
#endif

#include <cstdint>
#include <vector>

namespace miniorm
{
    enum class FieldFlag : uint32
    {
        None = 0,
        PrimaryKey = 1 << 0,
        AutoIncrement = 1 << 1,
        NotNull = 1 << 2,
        Unique = 1 << 3,
        Indexed = 1 << 4,
        DefaultValue = 1 << 5
    };

    inline constexpr FieldFlag operator|(FieldFlag lhs, FieldFlag rhs)
    {
        return static_cast<FieldFlag>(static_cast<uint32>(lhs) | static_cast<uint32>(rhs));
    }

    inline constexpr FieldFlag operator&(FieldFlag lhs, FieldFlag rhs)
    {
        return static_cast<FieldFlag>(static_cast<uint32>(lhs) & static_cast<uint32>(rhs));
    }

    inline constexpr FieldFlag operator~(FieldFlag value)
    {
        return static_cast<FieldFlag>(~static_cast<uint32>(value));
    }

    inline constexpr Bool has_flag(FieldFlag value, FieldFlag flag)
    {
        return (static_cast<uint32>(value) & static_cast<uint32>(flag)) != 0;
    }

    struct FieldInfo
    {
        StringView name;
        StringView sql_type;
        FieldFlag flags = FieldFlag::None;
        Bool nullable = true;
        String default_value;
    };

    String field_flag_to_string(FieldFlag flag);

    template <typename T>
    struct EntityMetadata
    {
        using EntityType = T;
        using PrimaryKeyType = int64;

        static StringView table_name()
        {
            return "";
        }

        static StringView primary_key_name()
        {
            return "id";
        }

        static std::vector<FieldInfo> fields()
        {
            return {};
        }

        static std::vector<StringView> field_names()
        {
            return {};
        }

        static std::vector<String> serialize(const T &)
        {
            return {};
        }

        static T deserialize(const IResultRow &)
        {
            return T{};
        }

        static PrimaryKeyType primary_key_value(const T &)
        {
            return PrimaryKeyType{};
        }

        static Bool is_new(const T &)
        {
            return true;
        }
    };

    template <typename T>
    concept HasEntityMetadata = requires(const T &entity, const IResultRow &row) {
        typename EntityMetadata<T>::PrimaryKeyType;
        { EntityMetadata<T>::table_name() } -> std::convertible_to<StringView>;
        { EntityMetadata<T>::primary_key_name() } -> std::convertible_to<StringView>;
        { EntityMetadata<T>::field_names() } -> std::same_as<std::vector<StringView>>;
        { EntityMetadata<T>::serialize(entity) } -> std::same_as<std::vector<String>>;
        { EntityMetadata<T>::deserialize(row) } -> std::same_as<T>;
        { EntityMetadata<T>::primary_key_value(entity) } -> std::convertible_to<typename EntityMetadata<T>::PrimaryKeyType>;
    };
}

#endif
