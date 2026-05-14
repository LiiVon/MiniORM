#include <cstdlib>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <unordered_map>

#include <miniorm/miniorm.hpp>

#include <iostream>
#include <memory>
#include <optional>

namespace miniorm
{
	// 示例实体类和元数据定义
	struct DemoUser : public Entity<DemoUser>
	{
		int32 id = 0;
		String name;
		String email;
		Bool active = true;
	};

	template <>
	struct EntityMetadata<DemoUser>
	{
		using PrimaryKeyType = int32;

		static StringView table_name()
		{
			return "demo_users";
		}

		static StringView primary_key_name()
		{
			return "id";
		}

		static std::vector<FieldInfo> fields()
		{
			return {
				{"id", "INTEGER", FieldFlag::PrimaryKey | FieldFlag::AutoIncrement, false, ""},
				{"name", "VARCHAR(255)", FieldFlag::NotNull, false, ""},
				{"email", "VARCHAR(255)", FieldFlag::NotNull | FieldFlag::Unique, false, ""},
				{"active", "BOOLEAN", FieldFlag::NotNull, false, "TRUE"},
			};
		}

		static std::vector<StringView> field_names()
		{
			return {"id", "name", "email", "active"};
		}

		static std::vector<String> serialize(const DemoUser &user)
		{
			return {
				ToString<int32>::convert(user.id),
				SqlStringEscaper::quoted_value(user.name),
				SqlStringEscaper::quoted_value(user.email),
				user.active ? "TRUE" : "FALSE"};
		}

		static DemoUser deserialize(const IResultRow &row)
		{
			DemoUser user;
			user.id = row.get<int32>("id");
			user.name = row.get<String>("name");
			user.email = row.get<String>("email");
			user.active = row.get<Bool>("active");
			return user;
		}

		static PrimaryKeyType primary_key_value(const DemoUser &user)
		{
			return user.id;
		}

		static Bool is_new(const DemoUser &user)
		{
			return user.id == 0;
		}
	};

	void print_section(const StringView &title)
	{
		std::cout << "\n== " << title << " ==\n";
	}

	String get_env_or(const char *name, const StringView &fallback)
	{
		if (const char *value = std::getenv(name))
		{
			return String(value);
		}
		return String(fallback);
	}

	int32 get_env_int_or(const char *name, int32 fallback)
	{
		if (const char *value = std::getenv(name))
		{
			return static_cast<int32>(std::strtol(value, nullptr, 10));
		}
		return fallback;
	}

	String trim_copy(String value)
	{
		auto is_not_space = [](unsigned char ch) { return !std::isspace(ch); };
		value.erase(value.begin(), std::find_if(value.begin(), value.end(), is_not_space));
		value.erase(std::find_if(value.rbegin(), value.rend(), is_not_space).base(), value.end());
		return value;
	}

	std::unordered_map<String, String> load_key_value_file(const String &path)
	{
		std::unordered_map<String, String> values;
		if (path.empty())
		{
			return values;
		}

		std::ifstream input(path);
		if (!input)
		{
			return values;
		}

		String line;
		while (std::getline(input, line))
		{
			line = trim_copy(std::move(line));
			if (line.empty() || line.starts_with('#') || line.starts_with(';'))
			{
				continue;
			}

			auto equals_pos = line.find('=');
			if (equals_pos == String::npos)
			{
				continue;
			}

			String key = trim_copy(line.substr(0, equals_pos));
			String value = trim_copy(line.substr(equals_pos + 1));
			if (!key.empty())
			{
				values.emplace(std::move(key), std::move(value));
			}
		}

		return values;
	}

	String get_config_value(const std::unordered_map<String, String> &file_values, const char *env_name, const char *key, const StringView &fallback)
	{
		if (const char *env_value = std::getenv(env_name))
		{
			return String(env_value);
		}

		if (auto it = file_values.find(key); it != file_values.end())
		{
			return it->second;
		}

		return String(fallback);
	}

	int32 get_config_int(const std::unordered_map<String, String> &file_values, const char *env_name, const char *key, int32 fallback)
	{
		if (const char *env_value = std::getenv(env_name))
		{
			return static_cast<int32>(std::strtol(env_value, nullptr, 10));
		}

		if (auto it = file_values.find(key); it != file_values.end())
		{
			return static_cast<int32>(std::strtol(it->second.c_str(), nullptr, 10));
		}

		return fallback;
	}
}

int main(int argc, char **argv)
{
	using namespace miniorm;

	try
	{
		print_section("初始化 MySQL ORM");
		const String config_path = (argc > 1) ? String(argv[1]) : String("miniorm_mysql.conf");
		auto file_values = load_key_value_file(config_path);

		DatabaseConnectionConfig config;
		config.type = DatabaseType::MySQL;
		config.host = get_config_value(file_values, "MINIORM_MYSQL_HOST", "host", "127.0.0.1");
		config.port = get_config_int(file_values, "MINIORM_MYSQL_PORT", "port", 3306);
		config.username = get_config_value(file_values, "MINIORM_MYSQL_USER", "user", "cppdev");
		config.password = get_config_value(file_values, "MINIORM_MYSQL_PASSWORD", "password", "0330");
		config.database = get_config_value(file_values, "MINIORM_MYSQL_DATABASE", "database", "miniorm_demo");
		config.options = get_config_value(file_values, "MINIORM_MYSQL_OPTIONS", "options", "charset=utf8mb4");

		MiniORM orm(config);
		auto connection = orm.connection();
		std::cout << "数据库类型: " << database_type_name(connection->database_type()) << '\n';
		std::cout << "数据库版本: " << connection->database_version() << '\n';

		auto create_table_sql = StringFormatter::format(
			"CREATE TABLE IF NOT EXISTS {} ("
			"`id` INT NOT NULL AUTO_INCREMENT, "
			"`name` VARCHAR(255) NOT NULL, "
			"`email` VARCHAR(255) NOT NULL UNIQUE, "
			"`active` BOOLEAN NOT NULL DEFAULT TRUE, "
			"PRIMARY KEY (`id`)) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4",
			connection->escape_identifier("demo_users"));
		connection->execute_update(create_table_sql);
		connection->execute_update(StringFormatter::format("DELETE FROM {}", connection->escape_identifier("demo_users")));

		print_section("查询构建器演示");
		QueryBuilder select_builder;
		auto select_sql = select_builder.select({"id", "name", "email"})
									  .from("demo_users")
									  .where(condition("active").eq(true) && condition("name").like("John%"))
									  .order_by("id")
									  .limit(5)
									  .build();
		std::cout << select_sql << '\n';

		QueryBuilder insert_builder;
		auto insert_sql = insert_builder.insert_into("demo_users")
									  .value("id", 1)
									  .value("name", "Alice")
									  .value("email", "alice@example.com")
									  .value("active", true)
									  .build();
		std::cout << insert_sql << '\n';

		print_section("实体管理器演示");
		auto user_manager = orm.get_entity_manager<DemoUser>();

		DemoUser new_user;
		new_user.id = 0;
		new_user.name = "Alice";
		new_user.email = "alice@example.com";
		new_user.active = true;

		std::cout << "保存新用户: " << std::boolalpha << user_manager.save(new_user) << '\n';
		std::cout << "当前记录数: " << user_manager.count() << '\n';

		auto found_user = user_manager.find_by_primary_key(0);
		if (found_user)
		{
			std::cout << "查询到用户: " << found_user->name << " <" << found_user->email << ">\n";
		}
		else
		{
			std::cout << "未查到用户。\n";
		}

		DemoUser update_user;
		update_user.id = 0;
		update_user.name = "Alice Updated";
		update_user.email = "alice.updated@example.com";
		update_user.active = true;
		std::cout << "更新用户: " << user_manager.save(update_user) << '\n';

		print_section("事务演示");
		orm.transaction([&]() {
			DemoUser first;
			first.name = "Bob";
			first.email = "bob@example.com";
			first.active = true;
			user_manager.save(first);

			DemoUser second;
			second.name = "Carol";
			second.email = "carol@example.com";
			second.active = false;
			user_manager.save(second);

			std::cout << "事务内批量插入完成\n";
		});

		print_section("条件查询和删除演示");
		auto status_condition = condition("active").eq(true);
		std::cout << "条件 SQL: " << status_condition.to_sql() << '\n';
		std::cout << "条件参数数: " << status_condition.parameters().size() << '\n';

		std::cout << "删除用户: " << user_manager.remove(update_user) << '\n';

		print_section("MySQL 连接信息");
		std::cout << "最后插入ID: " << connection->last_insert_id() << '\n';
		std::cout << "受影响行数: " << connection->affected_rows() << '\n';

		print_section("演示结束");
		std::cout << "MiniORM MySQL 示例运行完成\n";
		return 0;
	}
	catch (const std::exception &ex)
	{
		std::cerr << "MySQL 示例运行失败: " << ex.what() << '\n';
		return 1;
	}
}
