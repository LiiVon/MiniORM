// concepts.hpp 是 MiniORM 项目的概念定义中心
// 使用 C++20 的 concepts 特性为整个项目提供编译时类型约束和接口规范

#ifndef MINIORM_CORE_CONCEPTS_HPP
#define MINIORM_CORE_CONCEPTS_HPP

#include "config.hpp"

#if !MINIORM_CPP20
#error "concepts.hpp requires C++20 support. Please enable C++20 in your compiler settings."
#endif

//  Concepts 特性检测 检测编译器是否支持 concepts 特性
#if defined(__cpp_concepts) && __cpp_concepts >= 201907L
#define MINIORM_HAS_CONCEPTS 1
#else
#ifdef MINIORM_COMPILER_MSVC
#pragma message("Warning: Compiler may not fully support C++20 concepts. Some features may be limited.")
#define MINIORM_HAS_CONCEPTS 0
#else
#error "Compiler does not support C++20 concepts. Please upgrade your compiler or use -fconcepts flag."
#endif
#endif

#include <concepts>
#include <type_traits>
#include <string>
#include <string_view>
#include <memory>
#include <vector>
#include <optional>
#include <chrono>
#include <functional>
#include <ostream>

namespace miniorm
{

    // 标准概念别名 引入 C++20 标准库中的常用概念别名，方便在项目中使用
    using std::assignable_from;
    using std::common_reference_with;
    using std::common_with;
    using std::convertible_to;
    using std::derived_from;
    using std::destructible;
    using std::floating_point;
    using std::integral;
    using std::same_as;
    using std::swappable;

    // 基础类型概念
    template <typename T> // 可哈希
    concept Hashable = requires(T a) {
        { std::hash<T>{}(a) } -> std::convertible_to<Size>;
    };

    template <typename T> // 可相等比较
    concept EqualityComparable = requires(T a, T b) {
        { a == b } -> std::convertible_to<Bool>;
        { a != b } -> std::convertible_to<Bool>;
    };

    template <typename T> // 可完全比较大小
    concept TotallyOrdered = EqualityComparable<T> && requires(T a, T b) {
        { a < b } -> std::convertible_to<Bool>;
        { a > b } -> std::convertible_to<Bool>;
        { a <= b } -> std::convertible_to<Bool>;
        { a >= b } -> std::convertible_to<Bool>;
    };

    template <typename T> // 可流式输出
    concept Streamable = requires(std::ostream &os, T a) {
        { os << a } -> std::same_as<std::ostream &>;
    };

    template <typename T> // 数据库映射核心概念
    concept BasicMappable =
        std::is_default_constructible_v<T> &&
        destructible<T> &&
        (std::is_copy_constructible_v<T> || std::is_move_constructible_v<T>) &&
        (std::is_copy_assignable_v<T> || std::is_move_assignable_v<T>);

    template <typename T> // 值可转换类型
    concept ValueConvertible = BasicMappable<T> && (std::is_arithmetic_v<T> || same_as<T, String> || same_as<T, StringView> || same_as<T, const char *> || same_as<T, char *> || same_as<T, Bool> || same_as<T, Byte> || requires(const T &value) {
            { std::to_string(value) } -> std::convertible_to<String>; } || requires(const T &value) {
            { value.to_string() } -> std::convertible_to<String>; } || requires(const String &str) {
            { T::from_string(str) } -> std::convertible_to<T>; } || requires(const String &str) { T(str); } || requires(const StringView &str) { T(str); });

    template <typename T> // 完整数据库可映射类型
    concept DatabaseMappable = EqualityComparable<T> && ValueConvertible<T>;

    template <typename T>
    concept ScalarType = DatabaseMappable<T> && (std::is_arithmetic_v<T> ||
                                                 same_as<T, String> ||
                                                 same_as<T, StringView> ||
                                                 same_as<T, const char *> ||
                                                 same_as<T, char *> ||
                                                 same_as<T, Bool> ||
                                                 same_as<T, Byte>);

    template <typename T> // 标量类型概念，表示可以直接映射到数据库字段的类型
    concept Nullable = (requires { typename T::value_type; } || std::is_pointer_v<T> || requires(const T &val) {
            { val.has_value() } -> std::convertible_to<Bool>;
            { val.is_null() } -> std::convertible_to<Bool>; });

    template <typename T>
    concept IntegerType =
        ScalarType<T> &&
        integral<T> &&
        !same_as<T, Bool> &&
        (same_as<T, int8> || same_as<T, int16> || same_as<T, int32> || same_as<T, int64> ||
         same_as<T, uint8> || same_as<T, uint16> || same_as<T, uint32> || same_as<T, uint64> ||
         std::is_integral_v<T>);

    template <typename T>
    concept FloatingType =
        ScalarType<T> &&
        floating_point<T> &&
        (same_as<T, float32> || same_as<T, float64> || std::is_floating_point_v<T>);

    template <typename T>
    concept StringType = ScalarType<T> && (same_as<T, String> ||
                                           same_as<T, StringView> ||
                                           same_as<T, const char *> ||
                                           same_as<T, char *> ||
                                           (std::is_array_v<T> && std::is_same_v<std::remove_extent_t<T>, char>));

    template <typename T>
    concept BooleanType = ScalarType<T> && same_as<T, Bool>;

    template <typename T> // 时间点 与 自定义时间类型
    concept TimeType = DatabaseMappable<T> && (same_as<T, std::chrono::system_clock::time_point> ||
                                               same_as<T, std::chrono::steady_clock::time_point> ||
                                               requires(const T &time) {
                                                   { time.year() } -> std::convertible_to<int>;
                                                   { time.month() } -> std::convertible_to<int>;
                                                   { time.day() } -> std::convertible_to<int>;
                                                   { time.hour() } -> std::convertible_to<int>;
                                                   { time.minute() } -> std::convertible_to<int>;
                                                   { time.second() } -> std::convertible_to<int>;
                                               });

    template <typename T>
    concept EnumType = DatabaseMappable<T> && std::is_enum_v<T>;

    template <typename T> // 序列容器
    concept SequenceContainer = requires(T container) {
        typename T::value_type;
        typename T::iterator;
        typename T::const_iterator;
        typename T::size_type;

        { container.begin() } -> same_as<typename T::iterator>;
        { container.end() } -> same_as<typename T::iterator>;
        { container.size() } -> std::convertible_to<typename T::size_type>;
        { container.empty() } -> std::convertible_to<Bool>;

        requires requires(typename T::value_type value) {
            container.push_back(value);
        } || requires(typename T::const_iterator pos, typename T::value_type value) {
            container.insert(pos, value);
        };
    };

    template <typename Container> // 可映射容器
    concept MappableContainer =
        SequenceContainer<Container> &&
        DatabaseMappable<typename Container::value_type>;

    template <typename T> // 关联容器
    concept AssociativeContainer = requires(T container) {
        typename T::key_type;
        typename T::mapped_type;
        typename T::value_type;

        { container.begin() } -> same_as<typename T::iterator>;
        { container.end() } -> same_as<typename T::iterator>;
        { container.size() } -> std::convertible_to<typename T::size_type>;
        { container.empty() } -> std::convertible_to<Bool>;

        requires requires(typename T::key_type key) {
            { container[key] } -> std::convertible_to<typename T::mapped_type &>;
        };
    };

    template <typename T> // 实体类型
    concept EntityType =
        std::is_class_v<T> &&
        !std::is_union_v<T> &&
        requires {
            { T::table_name() } -> std::convertible_to<StringView>;
        };

    template <typename T> // 有主键实体
    concept EntityWithPrimaryKey = EntityType<T> && requires {
        { T::primary_key_name() } -> std::convertible_to<StringView>;
        requires DatabaseMappable<typename T::PrimaryKeyType>;

        requires IntegerType<typename T::PrimaryKeyType> ||
                     StringType<typename T::PrimaryKeyType>;
    };

    template <typename T> // 可序列化实体
    concept SerializableEntity = EntityType<T> && requires(const T &entity, std::ostream &os) {
        { entity.serialize(os) } -> same_as<void>;
        requires requires(const String &json) {
            { T::from_json(json) } -> same_as<T>;
        };
    };

    template <typename T> // 表示查询条件的类型
    concept QueryCondition = requires(const T &condition) {
        { condition.to_sql() } -> std::convertible_to<String>;
        { condition.parameters() } -> SequenceContainer;

        requires requires(const T &other) {
            { condition && other } -> same_as<T>;
            { condition || other } -> same_as<T>;
            { !condition } -> same_as<T>;
        };
    };

    template <typename T> // 字段表达式
    concept FieldExpression = QueryCondition<T> && requires(const T &expr) {
        { expr.field_name() } -> std::convertible_to<StringView>;
        requires requires {
            { expr.value() } -> DatabaseMappable;
        } || true;
    };

    
// 概念工具和辅助宏
#define MINIORM_CONCEPT_ASSERT(Type, Concept) \
    MINIORM_STATIC_ASSERT(Concept<Type>,      \
                          "Type " MINIORM_STRINGIFY(Type) " does not satisfy concept " MINIORM_STRINGIFY(Concept))

#define MINIORM_REQUIRES_CONCEPTS(...) \
    requires(__VA_ARGS__)

#define MINIORM_CHECK_DATABASE_TYPE(Type) \
    MINIORM_CONCEPT_ASSERT(Type, DatabaseMappable)

#define MINIORM_CHECK_ENTITY_TYPE(Type) \
    MINIORM_CONCEPT_ASSERT(Type, EntityType)

    template <typename T>
    concept NumericType = IntegerType<T> || FloatingType<T>;

    template <typename T>
    concept TextType = StringType<T>;

    template <typename T>
    concept PrimaryKeyType = IntegerType<T> && requires {
        requires requires(T &value) {
            ++value;
        } || true;
    };

    template <typename T>
    concept SortableType = TotallyOrdered<T> && DatabaseMappable<T>;

#ifdef MINIORM_ENABLE_COMPILE_TIME_REFLECTION

    MINIORM_CONCEPT_ASSERT(int32, IntegerType);
    MINIORM_CONCEPT_ASSERT(float64, FloatingType);
    MINIORM_CONCEPT_ASSERT(String, StringType);
    MINIORM_CONCEPT_ASSERT(Bool, BooleanType);

    MINIORM_CONCEPT_ASSERT(std::vector<int>, SequenceContainer);
    MINIORM_CONCEPT_ASSERT(std::optional<int>, Nullable);

    MINIORM_CHECK_DATABASE_TYPE(int32);
    MINIORM_CHECK_DATABASE_TYPE(float64);
    MINIORM_CHECK_DATABASE_TYPE(String);
#endif

}

#endif
