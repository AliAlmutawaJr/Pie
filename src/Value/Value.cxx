#include "Value.hxx"

#include <string>
#include <utility>
#include <variant>
#include <unordered_set>



namespace pie {
namespace value {

struct Members;
using Object = std::pair<type::TypePtr, std::shared_ptr<Members>>;




std::string stringify(const Value& value, const size_t indent) {
    static size_t depth = 0;
    static std::unordered_set<Object*> seen{};

    ++depth;

    std::string s;

    if (std::holds_alternative<bool>(value)) {
        s = get<bool>(value) ? "true" : "false";
    }

    else if (std::holds_alternative<BigInt>(value)) {
        const auto& v = std::get<BigInt>(value);

        s = std::to_string(v);
    }

    else if (std::holds_alternative<double>(value)) {
        const auto& v = std::get<double>(value);
        s = std::to_string(v);
    }

    else if (std::holds_alternative<std::string>(value)) {
        s = std::get<std::string>(value);
    }

    else if (std::holds_alternative<expr::Closure>(value)) {
        s = get<expr::Closure>(value).stringify(indent);
    }

    else if (std::holds_alternative<BuiltinFunction>(value)) {
        s = get<BuiltinFunction>(value).func_name;
    }

    else if (std::holds_alternative<type::TypePtr>(value)) {
        s = get<type::TypePtr>(value)->text();
    }

    else if (std::holds_alternative<Object>(value)) {
        const auto& v = std::get<Object>(value);

        if (v.second->members.empty()) {
            s = "Object { }";
        }
        else {
            s = "Object {\n";

            const std::string space(indent + 4, ' ');
            for (const auto& [name, _, value_ptr] : v.second->members) {
                s += space + name.stringify() + " = ";

                const bool is_string = std::holds_alternative<std::string>(*value_ptr);
                if (is_string) s += '\"';


                if (std::holds_alternative<Object>(*value_ptr)) {
                    if (seen.contains(&get<Object>(*value_ptr))) {
                        s += "...";
                    }
                    else {
                        seen.insert(&get<Object>(*value_ptr));
                        s += stringify(*value_ptr, indent + 4);
                    }
                }
                else {
                    s += stringify(*value_ptr, indent + 4);
                }


                if (is_string) s += '\"';

                s += ";\n";
            }

            s += std::string(indent, ' ') + '}';
        }


    }

    else if(std::holds_alternative<expr::Node>(value)) {
        const std::string space(indent + 4, ' ');

        s = "Syntax {\n" + space + std::visit(
            [indent] (const auto& v) { return v->stringify(indent + 4); },
            get<expr::Node>(value)
        ) + '\n' + std::string(indent, ' ') + '}';
    }

    else if (std::holds_alternative<Pack>(value)) {
        std::string comma = "";
        for (const auto& v : get<Pack>(value)->values) {
            s += comma + stringify(v, indent + 4);
            comma = ", ";
        }
    }

    else if (std::holds_alternative<List>(value)) {
        s += '{';

        std::string comma = "";
        for (const auto& v : get<List>(value).elts->values) {
            s += comma + stringify(v, indent + 4);
            comma = ", ";
        }

        s += '}';
    }

    else if (std::holds_alternative<Map>(value)) {
        s += '{';

        std::string comma = "";
        for (const auto& [key, value] : get<Map>(value).items->map) {
            s += comma + stringify(key, indent + 4) + ": " + stringify(value, indent + 4);
            comma = ", ";
        }

        s += '}';
    }

    else util::error("Type not found! Index: " + std::to_string(value.index()));


    --depth;

    if (depth == 0) seen.clear();

    return s;
}


inline std::ostream& operator<<(std::ostream& os, const Environment& env) {
    for (const auto& [ID, expr] : env){
        const auto& [name, value, type] = expr;
        os << '[' << ID << "] " << name.space << "::" << name.name << ": " << type->text() << " = " << stringify(*value) << std::endl;
    }

    return os;
}


[[nodiscard]] bool operator==(const Value& lhs, const Value& rhs) noexcept {
    if (std::holds_alternative<BigInt>(lhs) and std::holds_alternative<BigInt>(rhs))
        return get<BigInt>(lhs) == get<BigInt>(rhs);

    if (std::holds_alternative<double>(lhs) and std::holds_alternative<double>(rhs))
        return get<double>(lhs) == get<double>(rhs);

    if (std::holds_alternative<bool>(lhs) and std::holds_alternative<bool>(rhs))
        return get<bool>(lhs) == get<bool>(rhs);

    if (std::holds_alternative<std::string>(lhs) and std::holds_alternative<std::string>(rhs))
        return get<std::string>(lhs) == get<std::string>(rhs);

    if (std::holds_alternative<expr::Closure>(lhs) and std::holds_alternative<expr::Closure>(rhs))
        return get<expr::Closure>(lhs).stringify() == get<expr::Closure>(rhs).stringify();


    if (std::holds_alternative<BuiltinFunction>(lhs) and std::holds_alternative<BuiltinFunction>(rhs))
        return get<BuiltinFunction>(lhs).func_name == get<BuiltinFunction>(rhs).func_name;



    if (std::holds_alternative<type::TypePtr>(lhs) and std::holds_alternative<type::TypePtr>(rhs)) {

        if (type::isClass(get<type::TypePtr>(lhs)) and type::isClass(get<type::TypePtr>(rhs))) {
            const auto a = dynamic_cast<const type::LiteralType&>(*get<type::TypePtr>(lhs).get()).cls->blueprint->fields,
                       b = dynamic_cast<const type::LiteralType&>(*get<type::TypePtr>(rhs).get()).cls->blueprint->fields;

            return a.size() == b.size() and
                std::ranges::all_of(
                    std::views::zip(a, b),
                    [] (const auto& tuple) {
                        const auto& [member1, member2] = tuple;
                        const auto& [name1, type1, expr1] = member1;
                        const auto& [name2, type2, expr2] = member2;

                        return name1.stringify() == name2.stringify()
                                    and *type1 == *type2
                                    and expr1->stringify() == expr2->stringify();
                    }
                );
        }

        return *get<type::TypePtr>(lhs) == *get<type::TypePtr>(rhs);
    }

    if (std::holds_alternative<Object>(lhs) and std::holds_alternative<Object>(rhs)) {
        const auto& a = get<Object>(lhs).second->members, b = get<Object>(rhs).second->members;
        return
            // a.first == b.first and
            a.size() == b.size() and
            std::ranges::all_of(
                std::views::zip(a, b),
                [] (const auto& tuple) {
                    const auto& [member1, member2] = tuple;
                    const auto& [name1, type1, value1] = member1;
                    const auto& [name2, type2, value2] = member2;

                    return name1.stringify() == name2.stringify()
                                  and *type1 == *type2
                                  and *value1 == *value2;
                }
            );
    }

    if (std::holds_alternative<expr::Node>(lhs) and std::holds_alternative<expr::Node>(rhs))
        util::error("Can't check equality of a Syntax!");

    if (std::holds_alternative<Pack>(lhs) and std::holds_alternative<Pack>(rhs))
        return get<Pack>(lhs)->values == get<Pack>(rhs)->values;

    if (std::holds_alternative<List>(lhs) and std::holds_alternative<List>(rhs))
        return get<List>(lhs).elts->values == get<List>(rhs).elts->values;

    if (std::holds_alternative<Map>(lhs) and std::holds_alternative<Map>(rhs))
        return get<Map>(lhs).items->map == get<Map>(rhs).items->map;


    // error();
    return false;
}

} // namespace value
} // namespace pie
