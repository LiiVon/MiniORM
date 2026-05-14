// test_query.cpp - MiniORM 查询构建器系统测试
// 测试 condition.hpp 和 query_builder.hpp

#include "test_support.hpp"
#include "../include/miniorm/query/condition.hpp"
#include "../include/miniorm/query/query_builder.hpp"

#include <cassert>
#include <iostream>

namespace miniorm
{
    void test_condition_and_expression()
    {
        std::cout << "\n=== 测试查询条件和表达式 ===" << std::endl;

        auto name_condition = condition("name").like("john%");
        auto age_condition = condition("age").ge(18);
        auto active_condition = condition("is_active").eq(true);

        auto combined = name_condition && age_condition && active_condition;

        assert(name_condition.to_sql() == "name LIKE 'john%'");
        assert(age_condition.to_sql() == "age >= 18");
        assert(active_condition.to_sql() == "is_active = TRUE");
        assert(combined.to_sql() == "((name LIKE 'john%') AND (age >= 18)) AND (is_active = TRUE)");
        assert(combined.parameters().size() == 3);

        auto negated = !condition("deleted_at").is_null();
        assert(negated.to_sql() == "NOT (deleted_at IS NULL)");
    }

    void test_query_builder_select()
    {
        std::cout << "\n=== 测试 SELECT 查询构建器 ===" << std::endl;

        QueryBuilder builder;
        builder.select({"id", "name"})
               .from("users")
               .where(condition("name").like("john%") && condition("age").ge(18))
               .order_by("created_at", false)
               .limit(10)
               .offset(20);

        String sql = builder.build();
        std::cout << sql << std::endl;

        assert(sql == "SELECT id, name FROM users WHERE (name LIKE 'john%') AND (age >= 18) ORDER BY created_at DESC LIMIT 10 OFFSET 20");
    }

    void test_query_builder_insert_update_delete()
    {
        std::cout << "\n=== 测试 INSERT / UPDATE / DELETE 构建器 ===" << std::endl;

        QueryBuilder insert_builder;
        insert_builder.insert_into("users")
                      .value("id", 1)
                      .value("name", "Alice")
                      .value("email", "alice@example.com");
        assert(insert_builder.build() == "INSERT INTO users (id, name, email) VALUES (1, 'Alice', 'alice@example.com')");

        QueryBuilder update_builder;
        update_builder.update("users")
                      .set("name", "Bob")
                      .where(condition("id").eq(1));
        assert(update_builder.build() == "UPDATE users SET name = 'Bob' WHERE id = 1");

        QueryBuilder delete_builder;
        delete_builder.delete_from("users")
                      .where(condition("id").eq(1));
        assert(delete_builder.build() == "DELETE FROM users WHERE id = 1");
    }
}

int main()
{
    miniorm::test_condition_and_expression();
    miniorm::test_query_builder_select();
    miniorm::test_query_builder_insert_update_delete();

    std::cout << "\nquery tests passed" << std::endl;
    return 0;
}
