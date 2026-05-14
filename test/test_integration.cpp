// test_integration.cpp - MiniORM 集成测试
// 测试 miniorm.hpp 顶层聚合接口与事务流程

#include "test_support.hpp"
#include "../include/miniorm/miniorm.hpp"

#include <cassert>
#include <iostream>

namespace miniorm
{
    struct Account : public Entity<Account>
    {
        int32 id = 0;
        String name;
    };

    template <>
    struct EntityMetadata<Account>
    {
        using PrimaryKeyType = int32;

        static StringView table_name() { return "accounts"; }
        static StringView primary_key_name() { return "id"; }
        static std::vector<FieldInfo> fields()
        {
            return {
                {"id", "INTEGER", FieldFlag::PrimaryKey | FieldFlag::AutoIncrement, false, ""},
                {"name", "VARCHAR(255)", FieldFlag::NotNull, false, ""},
            };
        }

        static std::vector<StringView> field_names()
        {
            return {"id", "name"};
        }

        static std::vector<String> serialize(const Account &account)
        {
            return {ToString<int32>::convert(account.id), SqlStringEscaper::quoted_value(account.name)};
        }

        static Account deserialize(const IResultRow &row)
        {
            Account account;
            account.id = row.get<int32>("id");
            account.name = row.get<String>("name");
            return account;
        }

        static PrimaryKeyType primary_key_value(const Account &account)
        {
            return account.id;
        }

        static Bool is_new(const Account &account)
        {
            return account.id == 0;
        }
    };

    void test_miniorm_facade()
    {
        std::cout << "\n=== 测试 MiniORM 顶层接口 ===" << std::endl;

        auto connection = std::make_shared<TestConnection>();
        connection->set_query_result({{"1", "Primary"}}, {"id", "name"});

        MiniORM orm(connection);
        auto manager = orm.get_entity_manager<Account>();

        Account account;
        account.id = 0;
        account.name = "Primary";
        assert(manager.save(account));
        assert(connection->last_update() == "INSERT INTO accounts (id, name) VALUES (0, 'Primary')");

        auto result = orm.transaction([&]() {
            Account nested;
            nested.id = 1;
            nested.name = "Nested";
            assert(manager.save(nested));
            return String("done");
        });

        assert(result == "done");
        assert(connection->begin_transaction_calls() >= 1);
        assert(connection->commit_calls() >= 1);
    }
}

int main()
{
    miniorm::test_miniorm_facade();
    std::cout << "\nintegration tests passed" << std::endl;
    return 0;
}
