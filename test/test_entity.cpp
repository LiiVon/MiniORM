// test_entity.cpp - MiniORM 实体管理系统测试
// 测试 reflection.hpp, entity_traits.hpp 和 entity_manager.hpp

#include "test_support.hpp"
#include "../include/miniorm/entity/entity_manager.hpp"
#include "../include/miniorm/entity/entity_traits.hpp"
#include "../include/miniorm/entity/reflection.hpp"

#include <cassert>
#include <iostream>
#include <vector>

namespace miniorm
{
    // 使用宏声明实体与元数据（X-macro 风格，减少样板）
    #include "../include/miniorm/entity/entity_macros.hpp"

    // 定义字段列表（X-macro）
    #define USER_FIELDS(X) \
        X(int32, id, "INTEGER", FieldFlag::PrimaryKey | FieldFlag::AutoIncrement) \
        X(String, name, "VARCHAR(255)", FieldFlag::NotNull) \
        X(String, email, "VARCHAR(255)", FieldFlag::NotNull | FieldFlag::Unique)

    // 生成实体结构体与 EntityMetadata::fields()/field_names()
    MINIORM_DECLARE_ENTITY_FROM_X(User, int32, "users", USER_FIELDS)

    // 自定义序列化/反序列化与主键逻辑
    MINIORM_METADATA_SERIALIZE_BEGIN(User)
        return {
            ToString<int32>::convert(entity.id),
            SqlStringEscaper::quoted_value(entity.name),
            SqlStringEscaper::quoted_value(entity.email)
        };
    MINIORM_METADATA_SERIALIZE_END()

    MINIORM_METADATA_DESERIALIZE_BEGIN(User)
        User user;
        user.id = row.get<int32>("id");
        user.name = row.get<String>("name");
        user.email = row.get<String>("email");
        return user;
    MINIORM_METADATA_DESERIALIZE_END()

    MINIORM_METADATA_PRIMARY_KEY_VALUE_BEGIN(int32)
        return entity.id;
    MINIORM_METADATA_PRIMARY_KEY_VALUE_END()

    MINIORM_METADATA_IS_NEW_BEGIN()
        return entity.id == 0;
    MINIORM_METADATA_IS_NEW_END()

    MINIORM_METADATA_END()

    void test_reflection_helpers()
    {
        std::cout << "\n=== 测试实体反射辅助 ===" << std::endl;

        auto field_infos = EntityMetadata<User>::fields();
        assert(field_infos.size() == 3);
        assert(field_infos[0].name == "id");
        assert(has_flag(field_infos[0].flags, FieldFlag::PrimaryKey));
        assert(has_flag(field_infos[2].flags, FieldFlag::Unique));

        std::cout << field_flag_to_string(FieldFlag::PrimaryKey) << std::endl;
    }

    void test_entity_manager_crud()
    {
        std::cout << "\n=== 测试实体管理器 CRUD ===" << std::endl;

        auto connection = std::make_shared<TestConnection>();
        connection->set_query_result({{"1", "Alice", "alice@example.com"}}, {"id", "name", "email"});

        EntityManager<User> manager(connection);

        User new_user;
        new_user.id = 0;
        new_user.name = "Alice";
        new_user.email = "alice@example.com";
        assert(manager.save(new_user));
        assert(connection->last_update() == "INSERT INTO users (id, name, email) VALUES (0, 'Alice', 'alice@example.com')");

        User existing_user;
        existing_user.id = 1;
        existing_user.name = "Bob";
        existing_user.email = "bob@example.com";
        assert(manager.save(existing_user));
        assert(connection->last_update() == "UPDATE users SET name = 'Bob', email = 'bob@example.com' WHERE id = 1");

        auto found = manager.find_by_primary_key(1);
        assert(found.has_value());
        assert(found->name == "Alice");

        auto all_users = manager.find_all();
        assert(!all_users.empty());
        assert(all_users.front().email == "alice@example.com");

        assert(manager.remove(existing_user));
        assert(connection->last_update() == "DELETE FROM users WHERE id = 1");
    }
}

int main()
{
    miniorm::test_reflection_helpers();
    miniorm::test_entity_manager_crud();

    std::cout << "\nentity tests passed" << std::endl;
    return 0;
}
