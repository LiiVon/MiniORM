// entity_macros.hpp - 简易实体/元数据声明宏，提供实体与元数据的骨架，减少样板代码

#ifndef MINIORM_ENTITY_ENTITY_MACROS_HPP
#define MINIORM_ENTITY_ENTITY_MACROS_HPP

#include "reflection.hpp"

namespace miniorm
{

// 实体结构体声明辅助宏
#define MINIORM_ENTITY_BEGIN(Ent)   \
    struct Ent : public Entity<Ent> \
    {

#define MINIORM_ENTITY_FIELD(type, name) \
    type name;

#define MINIORM_ENTITY_END(Ent) \
    }                           \
    ;

// 元数据字段条目宏（用于 fields() 返回值构建）
#define MINIORM_METADATA_BEGIN(Ent, PKType, TableName)        \
    template <>                                               \
    struct EntityMetadata<Ent>                                \
    {                                                         \
        using PrimaryKeyType = PKType;                        \
        static StringView table_name() { return TableName; }  \
        static StringView primary_key_name() { return "id"; } \
        static std::vector<FieldInfo> fields()                \
        {                                                     \
            return                                            \
            {

#define MINIORM_METADATA_FIELD(name_literal, sql_type_literal, flags_val, nullable_val, default_val) \
    {name_literal, sql_type_literal, flags_val, nullable_val, default_val},

#define MINIORM_METADATA_FIELDS_END() \
    }                                 \
    ;                                 \
    }

// serialize / deserialize / primary_key_value / is_new 的骨架宏，用户在宏之间填写具体实现
#define MINIORM_METADATA_SERIALIZE_BEGIN(Ent)               \
    static std::vector<String> serialize(const Ent &entity) \
    {

#define MINIORM_METADATA_SERIALIZE_END() \
    }

#define MINIORM_METADATA_DESERIALIZE_BEGIN(Ent)   \
    static Ent deserialize(const IResultRow &row) \
    {

#define MINIORM_METADATA_DESERIALIZE_END() \
    }

#define MINIORM_METADATA_PRIMARY_KEY_VALUE_BEGIN(PKType) \
    static PKType primary_key_value(const auto &entity)  \
    {

#define MINIORM_METADATA_PRIMARY_KEY_VALUE_END() \
    }

#define MINIORM_METADATA_IS_NEW_BEGIN()    \
    static Bool is_new(const auto &entity) \
    {

#define MINIORM_METADATA_IS_NEW_END() \
    }

#define MINIORM_METADATA_END() \
    }                          \
    ;

} // namespace miniorm

#endif // MINIORM_ENTITY_ENTITY_MACROS_HPP

// 扩展：支持 X-macro 风格的字段列表声明
// 使用方法示例（在调用处定义）：
// #define USER_FIELDS(X) \
//   X(int32, id, "INTEGER", FieldFlag::PrimaryKey | FieldFlag::AutoIncrement) \
//   X(String, name, "VARCHAR(255)", FieldFlag::NotNull) \
//   X(String, email, "VARCHAR(255)", FieldFlag::NotNull | FieldFlag::Unique)
// 然后调用：
// MINIORM_DECLARE_ENTITY_FROM_X(User, int32, "users", USER_FIELDS)

#define MINIORM_X_MEMBER(type, name, sql, flags) type name;
#define MINIORM_X_FIELDINFO(type, name, sql, flags) {#name, sql, flags, false, ""},
#define MINIORM_X_FIELDNAME(type, name, sql, flags) #name,

#define MINIORM_DECLARE_ENTITY_FROM_X(Ent, PKType, TableName, XMACRO)                    \
    struct Ent : public Entity<Ent>                                                      \
    {                                                                                    \
        XMACRO(MINIORM_X_MEMBER)                                                         \
    };                                                                                   \
    template <>                                                                          \
    struct EntityMetadata<Ent>                                                           \
    {                                                                                    \
        using PrimaryKeyType = PKType;                                                   \
        static StringView table_name() { return TableName; }                             \
        static StringView primary_key_name() { return "id"; }                            \
        static std::vector<FieldInfo> fields() { return {XMACRO(MINIORM_X_FIELDINFO)}; } \
        static std::vector<StringView> field_names() { return {XMACRO(MINIORM_X_FIELDNAME)}; }
