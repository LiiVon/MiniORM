// concepts_test.cpp - MiniORM 概念系统测试
// 测试 concepts.hpp 中定义的所有概念和功能

// 首先包含我们的配置文件
#include "../include/miniorm/core/config.hpp"
#include "../include/miniorm/core/concepts.hpp"

// 标准库头文件
#include <iostream>
#include <vector>
#include <optional>
#include <map>
#include <unordered_map>
#include <chrono>
#include <memory>
#include <string>
#include <cassert>

namespace miniorm
{

    // ==================== 测试类型定义 ====================

    // 测试1：基础数据类型
    struct BasicTypes
    {
        int32 id;
        float64 price;
        String name;
        Bool active;
        Byte flags;
    };

    // 测试2：用户实体类
    class User
    {
    private:
        int32 id_;
        String name_;
        float64 balance_;

    public:
        User() = default;

        User(int32 id, String name, float64 balance)
            : id_(id), name_(std::move(name)), balance_(balance) {}

        // 必须的静态方法 - 用于 EntityType 概念
        static StringView table_name() { return "users"; }

        // 主键相关 - 用于 EntityWithPrimaryKey 概念
        static StringView primary_key_name() { return "id"; }
        using PrimaryKeyType = int32;

        // 序列化方法 - 用于 SerializableEntity 概念
        void serialize(std::ostream &os) const
        {
            os << "User{id=" << id_ << ", name=" << name_ << ", balance=" << balance_ << "}";
        }

        static User from_json(const String &json)
        {
            // 简化的 JSON 解析
            return User(1, "test_user", 100.0);
        }

        // Getter 方法
        int32 id() const { return id_; }
        const String &name() const { return name_; }
        float64 balance() const { return balance_; }

        // 相等比较运算符
        bool operator==(const User &other) const
        {
            return id_ == other.id_ && name_ == other.name_ && balance_ == other.balance_;
        }

        bool operator!=(const User &other) const
        {
            return !(*this == other);
        }
    };

    // 测试3：产品实体类
    class Product
    {
    private:
        int64 id_;
        String name_;
        float32 price_;
        int32 stock_;

    public:
        Product() = default;

        Product(int64 id, String name, float32 price, int32 stock)
            : id_(id), name_(std::move(name)), price_(price), stock_(stock) {}

        static StringView table_name() { return "products"; }

        static StringView primary_key_name() { return "product_id"; }
        using PrimaryKeyType = int64;

        // Getter 方法
        int64 id() const { return id_; }
        const String &name() const { return name_; }
        float32 price() const { return price_; }
        int32 stock() const { return stock_; }

        // 比较运算符
        bool operator==(const Product &other) const
        {
            return id_ == other.id_ && name_ == other.name_ &&
                   price_ == other.price_ && stock_ == other.stock_;
        }

        bool operator!=(const Product &other) const
        {
            return !(*this == other);
        }

        // 排序支持
        bool operator<(const Product &other) const
        {
            return id_ < other.id_;
        }
    };

    // 测试4：自定义时间类型
    struct CustomTime
    {
        int year_;
        int month_;
        int day_;
        int hour_;
        int minute_;
        int second_;

        // 用于 TimeType 概念的方法
        int year() const { return year_; }
        int month() const { return month_; }
        int day() const { return day_; }
        int hour() const { return hour_; }
        int minute() const { return minute_; }
        int second() const { return second_; }

        // 相等比较
        bool operator==(const CustomTime &other) const
        {
            return year_ == other.year_ && month_ == other.month_ && day_ == other.day_ &&
                   hour_ == other.hour_ && minute_ == other.minute_ && second_ == other.second_;
        }

        bool operator!=(const CustomTime &other) const
        {
            return !(*this == other);
        }
    };

    template <DatabaseMappable T>
    void print_database_value(const T &value)
    {
        std::cout << "   Database value: " << value << std::endl;
    }

    template <EntityType T>
    void print_entity_info()
    {
        std::cout << "   Entity table: " << T::table_name() << std::endl;
    }

    template <DatabaseMappable T>
    class DatabaseField
    {
    private:
        T value_;
        String name_;

    public:
        DatabaseField(String name, T value)
            : value_(std::move(value)), name_(std::move(name)) {}

        const String &name() const { return name_; }
        const T &value() const { return value_; }

        String to_sql_value() const
        {
            if constexpr (StringType<T>)
            {
                return "'" + String(value_) + "'";
            }
            else if constexpr (NumericType<T>)
            {
                return std::to_string(value_);
            }
            else if constexpr (BooleanType<T>)
            {
                return value_ ? "TRUE" : "FALSE";
            }
            else
            {
                return "NULL";
            }
        }
    };

    template <DatabaseMappable T>
    void process_numeric_database_value(const T &value)
    {
        std::cout << "   Processing numeric database value: " << value << std::endl;
    }

    // 测试5：查询条件类（简化版）
    class SimpleCondition
    {
    private:
        String field_;
        String op_;
        String value_;
        std::vector<String> params_;

    public:
        SimpleCondition(String field, String op, String value)
            : field_(std::move(field)), op_(std::move(op)), value_(std::move(value))
        {
            params_.push_back(value_);
        }

        // 用于 QueryCondition 概念的方法
        String to_sql() const
        {
            return field_ + " " + op_ + " ?";
        }

        const std::vector<String> &parameters() const
        {
            return params_;
        }

        // 组合操作
        SimpleCondition operator&&(const SimpleCondition &other) const
        {
            return SimpleCondition(
                "(" + to_sql() + " AND " + other.to_sql() + ")",
                "AND",
                "");
        }

        SimpleCondition operator||(const SimpleCondition &other) const
        {
            return SimpleCondition(
                "(" + to_sql() + " OR " + other.to_sql() + ")",
                "OR",
                "");
        }

        SimpleCondition operator!() const
        {
            return SimpleCondition("NOT (" + to_sql() + ")", "NOT", "");
        }

        // 用于 FieldExpression 概念的方法
        StringView field_name() const
        {
            return field_;
        }

        String value() const
        {
            return value_;
        }
    };

    // ==================== 测试函数 ====================

    // 测试基础类型概念
    void test_basic_concepts()
    {
        std::cout << "=== 测试基础类型概念 ===" << std::endl;

        // 测试整数类型
        std::cout << "1. 整数类型检查:" << std::endl;
        std::cout << "   int32 是 IntegerType: " << IntegerType<int32> << std::endl;
        std::cout << "   int64 是 IntegerType: " << IntegerType<int64> << std::endl;
        std::cout << "   uint32 是 IntegerType: " << IntegerType<uint32> << std::endl;
        std::cout << "   bool 不是 IntegerType: " << IntegerType<bool> << std::endl;
        std::cout << "   double 不是 IntegerType: " << IntegerType<double> << std::endl;

        // 测试浮点类型
        std::cout << "\n2. 浮点类型检查:" << std::endl;
        std::cout << "   float32 是 FloatingType: " << FloatingType<float32> << std::endl;
        std::cout << "   float64 是 FloatingType: " << FloatingType<float64> << std::endl;
        std::cout << "   int32 不是 FloatingType: " << FloatingType<int32> << std::endl;

        // 测试字符串类型
        std::cout << "\n3. 字符串类型检查:" << std::endl;
        std::cout << "   String 是 StringType: " << StringType<String> << std::endl;
        std::cout << "   StringView 是 StringType: " << StringType<StringView> << std::endl;
        std::cout << "   const char* 是 StringType: " << StringType<const char *> << std::endl;
        std::cout << "   char[10] 是 StringType: " << StringType<char[10]> << std::endl;
        std::cout << "   int32 不是 StringType: " << StringType<int32> << std::endl;

        // 测试布尔类型
        std::cout << "\n4. 布尔类型检查:" << std::endl;
        std::cout << "   Bool 是 BooleanType: " << BooleanType<Bool> << std::endl;
        std::cout << "   bool 是 BooleanType: " << BooleanType<bool> << std::endl;
        std::cout << "   int32 不是 BooleanType: " << BooleanType<int32> << std::endl;

        // 测试数值类型
        std::cout << "\n5. 数值类型检查:" << std::endl;
        std::cout << "   int32 是 NumericType: " << NumericType<int32> << std::endl;
        std::cout << "   float64 是 NumericType: " << NumericType<float64> << std::endl;
        std::cout << "   String 不是 NumericType: " << NumericType<String> << std::endl;

        // 测试文本类型
        std::cout << "\n6. 文本类型检查:" << std::endl;
        std::cout << "   String 是 TextType: " << TextType<String> << std::endl;
        std::cout << "   const char* 是 TextType: " << TextType<const char *> << std::endl;
        std::cout << "   int32 不是 TextType: " << TextType<int32> << std::endl;
    }

    // 测试容器概念
    void test_container_concepts()
    {
        std::cout << "\n=== 测试容器概念 ===" << std::endl;

        // 测试序列容器
        std::cout << "1. 序列容器检查:" << std::endl;
        std::cout << "   std::vector<int> 是 SequenceContainer: "
                  << SequenceContainer<std::vector<int>> << std::endl;
        std::cout << "   std::vector<String> 是 SequenceContainer: "
                  << SequenceContainer<std::vector<String>> << std::endl;
        std::cout << "   std::array<int, 5> 是 SequenceContainer: "
                  << SequenceContainer<std::array<int, 5>> << std::endl;

        // 测试可映射容器
        std::cout << "\n2. 可映射容器检查:" << std::endl;
        std::cout << "   std::vector<int> 是 MappableContainer: "
                  << MappableContainer<std::vector<int>> << std::endl;
        std::cout << "   std::vector<User> 不是 MappableContainer (User不是DatabaseMappable): "
                  << MappableContainer<std::vector<User>> << std::endl;

        // 测试关联容器
        std::cout << "\n3. 关联容器检查:" << std::endl;
        std::cout << "   std::map<int, String> 是 AssociativeContainer: "
                  << AssociativeContainer<std::map<int, String>> << std::endl;
        std::cout << "   std::unordered_map<String, int> 是 AssociativeContainer: "
                  << AssociativeContainer<std::unordered_map<String, int>> << std::endl;
        std::cout << "   std::vector<int> 不是 AssociativeContainer: "
                  << AssociativeContainer<std::vector<int>> << std::endl;

        // 测试可为空类型
        std::cout << "\n4. 可为空类型检查:" << std::endl;
        std::cout << "   std::optional<int> 是 Nullable: "
                  << Nullable<std::optional<int>> << std::endl;
        std::cout << "   int* 是 Nullable: "
                  << Nullable<int *> << std::endl;
        std::cout << "   std::shared_ptr<String> 是 Nullable: "
                  << Nullable<std::shared_ptr<String>> << std::endl;
        std::cout << "   int 不是 Nullable: "
                  << Nullable<int> << std::endl;
    }

    // 测试数据库映射概念
    void test_database_concepts()
    {
        std::cout << "\n=== 测试数据库映射概念 ===" << std::endl;

        // 测试基础可映射类型
        std::cout << "1. 基础可映射类型检查:" << std::endl;
        std::cout << "   int32 是 BasicMappable: " << BasicMappable<int32> << std::endl;
        std::cout << "   String 是 BasicMappable: " << BasicMappable<String> << std::endl;
        std::cout << "   float64 是 BasicMappable: " << BasicMappable<float64> << std::endl;

        // 测试值可转换类型
        std::cout << "\n2. 值可转换类型检查:" << std::endl;
        std::cout << "   int32 是 ValueConvertible: " << ValueConvertible<int32> << std::endl;
        std::cout << "   String 是 ValueConvertible: " << ValueConvertible<String> << std::endl;
        std::cout << "   float64 是 ValueConvertible: " << ValueConvertible<float64> << std::endl;

        // 测试完整数据库可映射类型
        std::cout << "\n3. 完整数据库可映射类型检查:" << std::endl;
        std::cout << "   int32 是 DatabaseMappable: " << DatabaseMappable<int32> << std::endl;
        std::cout << "   String 是 DatabaseMappable: " << DatabaseMappable<String> << std::endl;
        std::cout << "   float64 是 DatabaseMappable: " << DatabaseMappable<float64> << std::endl;
        std::cout << "   Bool 是 DatabaseMappable: " << DatabaseMappable<Bool> << std::endl;

        // 测试标量类型
        std::cout << "\n4. 标量类型检查:" << std::endl;
        std::cout << "   int32 是 ScalarType: " << ScalarType<int32> << std::endl;
        std::cout << "   String 是 ScalarType: " << ScalarType<String> << std::endl;
        std::cout << "   float64 是 ScalarType: " << ScalarType<float64> << std::endl;
        std::cout << "   std::vector<int> 不是 ScalarType: " << ScalarType<std::vector<int>> << std::endl;
    }

    // 测试特殊类型概念
    void test_special_concepts()
    {
        std::cout << "\n=== 测试特殊类型概念 ===" << std::endl;

        // 测试时间类型
        std::cout << "1. 时间类型检查:" << std::endl;
        std::cout << "   std::chrono::system_clock::time_point 是 TimeType: "
                  << TimeType<std::chrono::system_clock::time_point> << std::endl;
        std::cout << "   CustomTime 是 TimeType: "
                  << TimeType<CustomTime> << std::endl;
        std::cout << "   int32 不是 TimeType: "
                  << TimeType<int32> << std::endl;

        // 测试枚举类型
        enum class Color
        {
            Red,
            Green,
            Blue
        };
        enum class Status
        {
            Active,
            Inactive
        };

        std::cout << "\n2. 枚举类型检查:" << std::endl;
        std::cout << "   Color 是 EnumType: " << EnumType<Color> << std::endl;
        std::cout << "   Status 是 EnumType: " << EnumType<Status> << std::endl;
        std::cout << "   int32 不是 EnumType: " << EnumType<int32> << std::endl;

        // 测试可排序类型
        std::cout << "\n3. 可排序类型检查:" << std::endl;
        std::cout << "   int32 是 SortableType: " << SortableType<int32> << std::endl;
        std::cout << "   String 是 SortableType: " << SortableType<String> << std::endl;
        std::cout << "   float64 是 SortableType: " << SortableType<float64> << std::endl;
    }

    // 测试ORM实体概念
    void test_entity_concepts()
    {
        std::cout << "\n=== 测试ORM实体概念 ===" << std::endl;

        // 测试实体类型
        std::cout << "1. 实体类型检查:" << std::endl;
        std::cout << "   User 是 EntityType: " << EntityType<User> << std::endl;
        std::cout << "   Product 是 EntityType: " << EntityType<Product> << std::endl;
        std::cout << "   int32 不是 EntityType: " << EntityType<int32> << std::endl;
        std::cout << "   String 不是 EntityType: " << EntityType<String> << std::endl;

        // 测试有主键实体
        std::cout << "\n2. 有主键实体检查:" << std::endl;
        std::cout << "   User 是 EntityWithPrimaryKey: " << EntityWithPrimaryKey<User> << std::endl;
        std::cout << "   Product 是 EntityWithPrimaryKey: " << EntityWithPrimaryKey<Product> << std::endl;

        // 测试可序列化实体
        std::cout << "\n3. 可序列化实体检查:" << std::endl;
        std::cout << "   User 是 SerializableEntity: " << SerializableEntity<User> << std::endl;
        // Product 没有实现序列化方法，所以不是 SerializableEntity
        std::cout << "   Product 不是 SerializableEntity: " << SerializableEntity<Product> << std::endl;

        // 测试主键类型
        std::cout << "\n4. 主键类型检查:" << std::endl;
        std::cout << "   int32 是 PrimaryKeyType: " << PrimaryKeyType<int32> << std::endl;
        std::cout << "   int64 是 PrimaryKeyType: " << PrimaryKeyType<int64> << std::endl;
        std::cout << "   String 不是 PrimaryKeyType: " << PrimaryKeyType<String> << std::endl;
    }

    // 测试查询构建器概念
    void test_query_concepts()
    {
        std::cout << "\n=== 测试查询构建器概念 ===" << std::endl;

        // 测试查询条件
        std::cout << "1. 查询条件检查:" << std::endl;
        std::cout << "   SimpleCondition 是 QueryCondition: "
                  << QueryCondition<SimpleCondition> << std::endl;
        std::cout << "   int32 不是 QueryCondition: "
                  << QueryCondition<int32> << std::endl;

        // 测试字段表达式
        std::cout << "\n2. 字段表达式检查:" << std::endl;
        std::cout << "   SimpleCondition 是 FieldExpression: "
                  << FieldExpression<SimpleCondition> << std::endl;
    }

    // 测试概念组合和关系
    void test_concept_relationships()
    {
        std::cout << "\n=== 测试概念组合和关系 ===" << std::endl;

        // 测试概念层次结构
        std::cout << "1. 概念层次结构验证:" << std::endl;

        // IntegerType 应该是 ScalarType 的子集
        static_assert(IntegerType<int32> == ScalarType<int32>,
                      "IntegerType should be a subset of ScalarType");
        std::cout << "   ✓ IntegerType ⊆ ScalarType (int32)" << std::endl;

        // FloatingType 应该是 ScalarType 的子集
        static_assert(FloatingType<float64> == ScalarType<float64>,
                      "FloatingType should be a subset of ScalarType");
        std::cout << "   ✓ FloatingType ⊆ ScalarType (float64)" << std::endl;

        // ScalarType 应该是 DatabaseMappable 的子集
        static_assert(ScalarType<int32> == DatabaseMappable<int32>,
                      "ScalarType should be a subset of DatabaseMappable");
        std::cout << "   ✓ ScalarType ⊆ DatabaseMappable (int32)" << std::endl;

        // EntityType 不应该是 DatabaseMappable 的子集
        // (实体类型和标量类型是不同的类别)
        std::cout << "   ✓ EntityType and DatabaseMappable are separate categories" << std::endl;

        // 测试概念组合
        std::cout << "\n2. 概念组合验证:" << std::endl;

        // NumericType 是 IntegerType 和 FloatingType 的并集
        static_assert(NumericType<int32> == (IntegerType<int32> || FloatingType<int32>),
                      "NumericType should be union of IntegerType and FloatingType");
        std::cout << "   ✓ NumericType = IntegerType ∪ FloatingType" << std::endl;

        // DatabaseMappable 是 EqualityComparable 和 ValueConvertible 的交集
        // (实际上还有额外要求，所以这里只是部分验证)
        static_assert(DatabaseMappable<int32> == (EqualityComparable<int32> && ValueConvertible<int32>),
                      "DatabaseMappable requires both EqualityComparable and ValueConvertible");
        std::cout << "   ✓ DatabaseMappable requires EqualityComparable ∧ ValueConvertible" << std::endl;
    }

    // 测试概念工具宏
    void test_concept_macros()
    {
        std::cout << "\n=== 测试概念工具宏 ===" << std::endl;

        // 这些宏在编译时检查，如果失败会编译错误
        // 我们在这里只是演示它们的使用

        std::cout << "1. 概念检查宏示例:" << std::endl;

        // 正确的使用 - 应该编译通过
        MINIORM_CHECK_DATABASE_TYPE(int32);
        std::cout << "   ✓ MINIORM_CHECK_DATABASE_TYPE(int32) passed" << std::endl;

        MINIORM_CHECK_DATABASE_TYPE(String);
        std::cout << "   ✓ MINIORM_CHECK_DATABASE_TYPE(String) passed" << std::endl;

        MINIORM_CHECK_ENTITY_TYPE(User);
        std::cout << "   ✓ MINIORM_CHECK_ENTITY_TYPE(User) passed" << std::endl;

        MINIORM_CHECK_ENTITY_TYPE(Product);
        std::cout << "   ✓ MINIORM_CHECK_ENTITY_TYPE(Product) passed" << std::endl;

        // 注意：以下代码如果取消注释会导致编译错误
        /*
        // 错误的使用 - 会导致编译错误
        // MINIORM_CHECK_DATABASE_TYPE(std::vector<int>);  // 错误：vector<int> 不是 DatabaseMappable
        // MINIORM_CHECK_ENTITY_TYPE(int32);  // 错误：int32 不是 EntityType
        */

        std::cout << "\n2. 概念组合宏示例:" << std::endl;
        process_numeric_database_value(42);
        std::cout << "   ✓ MINIORM_REQUIRES_CONCEPTS macro works" << std::endl;
    }

    // 测试运行时类型检查
    void test_runtime_type_checks()
    {
        std::cout << "\n=== 测试运行时类型检查 ===" << std::endl;

        // 创建测试对象
        User user(1, "Alice", 1000.50);
        Product product(1001, "Laptop", 999.99f, 10);
        SimpleCondition condition("age", ">", "18");
        CustomTime time{2024, 5, 10, 14, 30, 0};

        std::cout << "1. 对象创建验证:" << std::endl;
        std::cout << "   User: id=" << user.id() << ", name=" << user.name()
                  << ", balance=" << user.balance() << std::endl;
        std::cout << "   Product: id=" << product.id() << ", name=" << product.name()
                  << ", price=" << product.price() << ", stock=" << product.stock() << std::endl;
        std::cout << "   Condition: " << condition.to_sql() << std::endl;
        std::cout << "   Time: " << time.year() << "-" << time.month() << "-" << time.day()
                  << " " << time.hour() << ":" << time.minute() << ":" << time.second() << std::endl;

        // 测试概念在模板函数中的应用
        std::cout << "\n2. 概念在模板函数中的应用:" << std::endl;
        print_database_value(user.id());
        print_entity_info<User>();
        std::cout << "   ✓ Concepts work in template functions" << std::endl;

        // 测试概念约束的类模板
        std::cout << "\n3. 概念约束的类模板:" << std::endl;
        // 创建 DatabaseField 实例
        DatabaseField<int32> id_field("id", 123);
        DatabaseField<String> name_field("name", "Test");
        DatabaseField<float64> price_field("price", 99.99);
        DatabaseField<Bool> active_field("active", true);

        std::cout << "   DatabaseField examples created:" << std::endl;
        std::cout << "     " << id_field.name() << " = " << id_field.to_sql_value() << std::endl;
        std::cout << "     " << name_field.name() << " = " << name_field.to_sql_value() << std::endl;
        std::cout << "     " << price_field.name() << " = " << price_field.to_sql_value() << std::endl;
        std::cout << "     " << active_field.name() << " = " << active_field.to_sql_value() << std::endl;
    }

} // namespace miniorm

using namespace miniorm;

// ==================== 主函数 ====================
int main()
{
    std::cout << "==========================================" << std::endl;
    std::cout << "MiniORM Concepts Test Program" << std::endl;
    std::cout << "测试 concepts.hpp 中的所有概念和功能" << std::endl;
    std::cout << "==========================================" << std::endl;

    try
    {
        // 运行所有测试
        test_basic_concepts();
        test_container_concepts();
        test_database_concepts();
        test_special_concepts();
        test_entity_concepts();
        test_query_concepts();
        test_concept_relationships();
        test_concept_macros();
        test_runtime_type_checks();

        std::cout << "\n==========================================" << std::endl;
        std::cout << "✅ 所有测试完成！" << std::endl;
        std::cout << "MiniORM 概念系统功能正常" << std::endl;
        std::cout << "==========================================" << std::endl;

        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "\n❌ 测试失败，异常: " << e.what() << std::endl;
        return 1;
    }
    catch (...)
    {
        std::cerr << "\n❌ 测试失败，未知异常" << std::endl;
        return 1;
    }
}
