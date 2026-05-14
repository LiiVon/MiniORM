// test_utils.cpp - MiniORM 工具函数系统测试
// 测试 utils.hpp 中定义的编译期工具、字符串处理和 SQL 构建功能

#include "../include/miniorm/core/config.hpp"
#include "../include/miniorm/core/concepts.hpp"
#include "../include/miniorm/core/traits.hpp"
#include "../include/miniorm/core/utils.hpp"

#include <array>
#include <cassert>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace miniorm
{

	struct UserProfile
	{
		String name;

		String greet(const String &prefix) const
		{
			return prefix + name;
		}
	};

	void test_compile_time_tools()
	{
		std::cout << "\n=== 测试编译期工具 ===" << std::endl;

		constexpr auto sql_prefix = join_string<"SELECT ", "id, name ", "FROM users">();
		std::cout << "1. join_string: " << sql_prefix.data() << std::endl;
		static_assert(sql_prefix[0] == 'S', "join_string result mismatch");

		std::cout << "2. type_name<int32>(): " << type_name<int32>() << std::endl;
		static_assert(type_name<int32>() == type_name_v<int32>, "type_name helper mismatch");

		constexpr Size h1 = constexpr_hash("users");
		constexpr Size h2 = constexpr_hash("users");
		std::cout << "3. constexpr_hash('users'): " << h1 << std::endl;
		static_assert(h1 == h2, "constexpr_hash should be deterministic");

		static_assert(constexpr_strcmp("abc", "abc"), "constexpr_strcmp equal failed");
		static_assert(!constexpr_strcmp("abc", "abd"), "constexpr_strcmp diff failed");
		static_assert(constexpr_strlen("miniorm") == 7, "constexpr_strlen mismatch");
		std::cout << "4. constexpr_strcmp / constexpr_strlen passed" << std::endl;

		constexpr std::array<int32, 5> values{1, 3, 5, 7, 9};
		constexpr Size idx = find_index<int32, 5, 7>(values);
		std::cout << "5. find_index<7>: " << idx << std::endl;
		static_assert(idx == 3, "find_index failed");
		static_assert(is_sorted(values), "is_sorted failed");
	}

	void test_sql_string_escaper()
	{
		std::cout << "\n=== 测试 SQL 字符串转义 ===" << std::endl;

		String raw = "O'Reilly\\book";
		String escaped = SqlStringEscaper::escape(raw);
		std::cout << "1. escape: " << escaped << std::endl;
		assert(escaped == "O''Reilly\\\\book");

		String id_plain = SqlStringEscaper::escape_identifier("user_name");
		String id_quoted = SqlStringEscaper::escape_identifier("user name");
		std::cout << "2. escape_identifier plain: " << id_plain << std::endl;
		std::cout << "3. escape_identifier quoted: " << id_quoted << std::endl;
		assert(id_plain == "user_name");
		assert(id_quoted == "\"user name\"");

		String quoted = SqlStringEscaper::quoted_value("it's ok");
		std::cout << "4. quoted_value: " << quoted << std::endl;
		assert(quoted == "'it''s ok'");

		String p3 = SqlStringEscaper::parameter_placeholder(2);
		std::cout << "5. parameter_placeholder(2): " << p3 << std::endl;
		assert(p3 == "?3");
	}

	void test_string_formatter()
	{
		std::cout << "\n=== 测试字符串格式化 ===" << std::endl;

		String msg = StringFormatter::format("User {} has {} points", "Alice", 88);
		std::cout << "1. format: " << msg << std::endl;
		assert(msg == "User Alice has 88 points");

		String sql = StringFormatter::format_sql(
			"SELECT * FROM users WHERE name = {}",
			"Alice");
		std::cout << "2. format_sql: " << sql << std::endl;
		assert(sql.find("'Alice'") != String::npos);

		constexpr auto only_fmt = StringFormatter::format_constexpr<"SELECT 1">();
		static_assert(only_fmt[0] == 'S', "format_constexpr failed");
		std::cout << "3. format_constexpr passed" << std::endl;
	}

	void test_safe_casts()
	{
		std::cout << "\n=== 测试安全类型转换 ===" << std::endl;

		int32 a = safe_cast<int32>(123);
		float64 b = safe_cast<float64>(a);
		std::cout << "1. safe_cast int32->float64: " << b << std::endl;
		assert(a == 123);
		assert(b == 123.0);

		int16 c = safe_numeric_cast<int16>(127);
		std::cout << "2. safe_numeric_cast<int16>(127): " << c << std::endl;
		assert(c == 127);

		bool overflow_caught = false;
		try
		{
			(void)safe_numeric_cast<int8>(300);
		}
		catch (const std::overflow_error &)
		{
			overflow_caught = true;
		}
		std::cout << "3. overflow check caught: " << overflow_caught << std::endl;
		assert(overflow_caught);
	}

	void test_optional_helper()
	{
		std::cout << "\n=== 测试 OptionalHelper ===" << std::endl;

		std::optional<int32> n1 = 21;
		std::optional<int32> n2;

		int32 v1 = OptionalHelper<int32>::value_or(n1, int32{0});
		int32 v2 = OptionalHelper<int32>::value_or(n2, int32{0});
		std::cout << "1. value_or with value: " << v1 << std::endl;
		std::cout << "2. value_or empty: " << v2 << std::endl;
		assert(v1 == 21 && v2 == 0);

		auto transformed = OptionalHelper<int32>::transform<int64>(n1, [](const int32 &x) {
			return static_cast<int64>(x) * 2;
		});
		std::cout << "3. transform result: " << transformed.value_or(-1) << std::endl;
		assert(transformed.has_value() && transformed.value() == 42);

		auto chained = OptionalHelper<int32>::and_then<int32>(n1, [](const int32 &x) {
			if (x > 10)
			{
				return std::optional<int32>{x + 1};
			}
			return std::optional<int32>{};
		});
		std::cout << "4. and_then result: " << chained.value_or(-1) << std::endl;
		assert(chained.has_value() && chained.value() == 22);

		auto parsed_ok = OptionalHelper<String>::safe_convert<int32>(std::optional<String>{"123"});
		auto parsed_bad = OptionalHelper<String>::safe_convert<int32>(std::optional<String>{"x12"});
		std::cout << "5. safe_convert('123') has_value: " << parsed_ok.has_value() << std::endl;
		std::cout << "6. safe_convert('x12') has_value: " << parsed_bad.has_value() << std::endl;
		assert(parsed_ok.has_value() && parsed_ok.value() == 123);
		assert(!parsed_bad.has_value());
	}

	void test_container_utils_and_sql_builder()
	{
		std::cout << "\n=== 测试容器工具与 SQL 构建 ===" << std::endl;

		std::vector<int32> nums{1, 2, 3, 4, 5};
		bool has_three = ContainerUtils<std::vector<int32>>::contains(nums, 3);
		std::cout << "1. contains(3): " << has_three << std::endl;
		assert(has_three);

		String joined = ContainerUtils<std::vector<int32>>::join(nums, "|");
		std::cout << "2. join: " << joined << std::endl;
		assert(joined == "1|2|3|4|5");

		auto evens = ContainerUtils<std::vector<int32>>::filter(nums, [](int32 x) { return (x % 2) == 0; });
		std::cout << "3. filter even size: " << evens.size() << std::endl;
		assert(evens.size() == 2);

		auto as_text = ContainerUtils<std::vector<int32>>::transform<std::vector<String>>(nums, [](int32 x) {
			return StringFormatter::format("n{}", x);
		});
		std::cout << "4. transform first: " << as_text.front() << std::endl;
		assert(as_text.front() == "n1");

		String sql_values = ContainerUtils<std::vector<int32>>::to_sql_values(nums);
		std::cout << "5. to_sql_values: " << sql_values << std::endl;
		assert(sql_values == "(1, 2, 3, 4, 5)");

		std::vector<String> columns{"id", "name", "active"};
		String select_sql = SqlBuilder::build_select("users", columns, "active = TRUE", "id DESC", 10, 20);
		std::cout << "6. build_select: " << select_sql << std::endl;
		assert(select_sql.find("SELECT 'id', 'name', 'active' FROM users") != String::npos);
		assert(select_sql.find("LIMIT 10 OFFSET 20") != String::npos);

		std::vector<String> insert_values{"1", "'Alice'", "TRUE"};
		String insert_sql = SqlBuilder::build_insert("users", columns, insert_values);
		std::cout << "7. build_insert: " << insert_sql << std::endl;
		assert(insert_sql.find("INSERT INTO users") != String::npos);

		std::vector<String> set_clauses{"name = 'Bob'", "active = FALSE"};
		String update_sql = SqlBuilder::build_update("users", set_clauses, "id = 1");
		String delete_sql = SqlBuilder::build_delete("users", "id = 1");
		std::cout << "8. build_update: " << update_sql << std::endl;
		std::cout << "9. build_delete: " << delete_sql << std::endl;
		assert(update_sql.find("UPDATE users SET") != String::npos);
		assert(delete_sql == "DELETE FROM users WHERE id = 1");
	}

	void test_exception_factory_and_macros()
	{
		std::cout << "\n=== 测试异常工厂与工具宏 ===" << std::endl;

		auto db_err = ExceptionFactory::database_error("query", "connection lost", 1001);
		std::cout << "1. database_error: " << db_err.what() << std::endl;
		assert(String(db_err.what()).find("Database error during query") != String::npos);

		auto conv_err = ExceptionFactory::type_conversion_error<String, int32>("abc");
		std::cout << "2. type_conversion_error: " << conv_err.what() << std::endl;
		assert(String(conv_err.what()).find("Cannot convert value") != String::npos);

		auto syntax_err = ExceptionFactory::sql_syntax_error("SELECT FROM", "FROM");
		std::cout << "3. sql_syntax_error: " << syntax_err.what() << std::endl;
		assert(String(syntax_err.what()).find("SQL syntax error near") != String::npos);

		int32 checked_value = MINIORM_IF_CONSTEXPR((sizeof(void *) >= 8), 64, 32);
		std::cout << "4. MINIORM_IF_CONSTEXPR result: " << checked_value << std::endl;
		assert(checked_value == 64 || checked_value == 32);

		UserProfile profile{"MiniORM"};
		UserProfile *profile_ptr = &profile;
		UserProfile *null_profile = nullptr;

		String greet_ok = MINIORM_SAFE_CALL(profile_ptr, greet, String{"Hello "});
		String greet_null = MINIORM_SAFE_CALL(null_profile, greet, String{"Hello "});
		std::cout << "5. MINIORM_SAFE_CALL non-null: " << greet_ok << std::endl;
		std::cout << "6. MINIORM_SAFE_CALL null(empty): '" << greet_null << "'" << std::endl;
		assert(greet_ok == "Hello MiniORM");
		assert(greet_null.empty());

		bool range_error_caught = false;
		try
		{
			MINIORM_CHECK_RANGE(120, 0, 100);
		}
		catch (const std::out_of_range &)
		{
			range_error_caught = true;
		}
		std::cout << "7. MINIORM_CHECK_RANGE caught: " << range_error_caught << std::endl;
		assert(range_error_caught);
	}

	void test_memory_pool()
	{
		std::cout << "\n=== 测试内存池 ===" << std::endl;

		MemoryPool<int32, 4> pool;
		int32 *p1 = pool.allocate();
		int32 *p2 = pool.allocate();
		*p1 = 10;
		*p2 = 20;

		std::cout << "1. allocated_blocks: " << pool.allocated_blocks() << std::endl;
		std::cout << "2. values: " << *p1 << ", " << *p2 << std::endl;
		assert(pool.allocated_blocks() >= 1);
		assert(*p1 == 10 && *p2 == 20);

		MemoryPool<int32, 4> moved_pool(std::move(pool));
		std::cout << "3. moved_pool blocks: " << moved_pool.allocated_blocks() << std::endl;
		assert(moved_pool.allocated_blocks() >= 1);
	}

} // namespace miniorm

using namespace miniorm;

int main()
{
	std::cout << "==========================================" << std::endl;
	std::cout << "MiniORM Utils Test Program" << std::endl;
	std::cout << "测试 utils.hpp 中的工具函数与宏" << std::endl;
	std::cout << "==========================================" << std::endl;

	try
	{
		test_compile_time_tools();
		test_sql_string_escaper();
		test_string_formatter();
		test_safe_casts();
		test_optional_helper();
		test_container_utils_and_sql_builder();
		test_exception_factory_and_macros();
		test_memory_pool();

		std::cout << "\n==========================================" << std::endl;
		std::cout << "所有测试完成！" << std::endl;
		std::cout << "MiniORM utils 系统功能正常" << std::endl;
		std::cout << "==========================================" << std::endl;

		return 0;
	}
	catch (const std::exception &e)
	{
		std::cerr << "\n测试失败，异常: " << e.what() << std::endl;
		return 1;
	}
	catch (...)
	{
		std::cerr << "\n测试失败，未知异常" << std::endl;
		return 1;
	}
}
