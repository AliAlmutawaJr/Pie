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
struct ClassValue {
    std::shared_ptr<Fields> blueprint;
    value::Env env;
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

struct Fields   { std::vector<std::tuple<expr::Name, type::TypePtr, expr::ExprPtr  >> fields;  };
struct Members  { std::vector<std::tuple<expr::Name, type::TypePtr, value::ValuePtr>> members; };
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



using Environment = std::unordered_map<
    size_t,
    std::tuple<
        SpaceRef,
        value::ValuePtr,
        type::TypePtr
    >
>;

}
}

