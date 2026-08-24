#pragma once

#include <string>
#include <vector>
#include <variant>
#include <unordered_map>
#include <utility>


#include "../Expr/Expr.hxx"
#include "../Type/Type.hxx"
#include "../Declarations.hxx"



inline namespace pie {
namespace value {

struct Fields;

// idealy, should be defined in Type.cxx/hxx
struct ClassValue {
    std::shared_ptr<Fields> blueprint;
    // captured environment
    value::Env env;
    // the namespace the class was declared in
    std::vector<interp::NameSpace*> spaces;
};

struct Members;


struct Elements;
struct List { std::shared_ptr<Elements> elts; };
using Pack = std::shared_ptr<Elements>;

struct Items;
struct Map { std::shared_ptr<Items> items; };

struct BuiltinFunction {
    std::string func_name;
};

using VariantType = std::variant<
    // ssize_t,
    BigInt,
    double,
    bool,
    std::string,
    expr::Closure,
    BuiltinFunction,
    type::TypePtr,
    Object,
    expr::Node,
    Pack,
    List,
    Map
>;

struct Value : VariantType {
    using VariantType::variant;
    using VariantType::operator=;
};

using ValuePtr = std::shared_ptr<Value>;


std::string stringify(const Value& value, const size_t indent = {});
[[nodiscard]] bool operator==(const Value& lhs, const Value& rhs) noexcept;
}
}

// needed for maps
template<>
struct std::hash<value::Value> { size_t operator()(const value::Value& value) const { return std::hash<std::string>{}(pie::value::stringify(value)); } };


inline namespace pie {
namespace value {

struct Fields   { std::vector<std::tuple<expr::Name, type::TypePtr, expr::ExprPtr>> fields;  };
struct Members  { std::vector<std::tuple<expr::Name, type::TypePtr, ValuePtr>> members; };
struct Elements { std::vector<Value> values;                                                   };
struct Items    { std::unordered_map<Value, Value> map;                                        };


template <typename ...Ts>
[[nodiscard]] inline Pack makePack(Ts&&... args) {
    return std::make_shared<Elements>(std::forward<Ts>(args)...);
}

[[nodiscard]] inline Pack makePack(std::vector<Value> values) {
    return std::make_shared<Elements>(std::move(values));
}


[[nodiscard]] inline List makeList(std::vector<Value> values = {}) {
    return {std::make_shared<Elements>(std::move(values))};
}


[[nodiscard]] inline Map makeMap(std::unordered_map<Value, Value> items = {}) {
    return {std::make_shared<Items>(std::move(items))};
}


[[nodiscard]] inline Object makeObject(
    type::TypePtr type,
    std::vector<std::tuple<expr::Name, type::TypePtr, Value>> members
) {

    std::vector<std::tuple<expr::Name, type::TypePtr, ValuePtr>> actual_members;
    for (auto& [name, type, value] : members)
        actual_members.emplace_back(
            std::move(name),
            std::move(type),
            std::make_shared<Value>(std::move(value))
        );


    return {
        std::move(type),
        std::make_shared<Members>(std::move(actual_members))
    };
}



using Environment = std::unordered_map<
    ssize_t,
    std::tuple<
        SpaceRef,
        value::ValuePtr,
        type::TypePtr
    >
>;

}
}

