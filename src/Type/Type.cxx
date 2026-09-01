#include "Type.hxx"
#include "../Interp/Interpreter.hxx"

#include <ranges>
#include <variant>


namespace pie {

namespace type {

    // * Any Expression * //
    std::string ExprType::text(const size_t indent) const {
        return t->stringify(indent);
    }

    bool ExprType::involvesT(const Type& T) const {
        // if (auto expr_type = dynamic_cast<const ExprType*>(&T)) {
        //     return t->
        // }

        return T == *this or t->involvesName(T.text());
    }

    bool ExprType::operator>(const Type& other) const {
        if (dynamic_cast<const TryReassign*>(&other)) return true;
        // if (not dynamic_cast<const ExprType   *>(&other)) return false;

        return false;
        // const auto& type = text();
        // return type == "Syntax" or (type == "Any" and other.text() != "Any");
    }

    bool ExprType::operator>=(const Type& other) const {
        if (dynamic_cast<const TryReassign*>(&other)) return true;

        return text() == other.text(); // only check for equality since ExprType is never greater than any other ExprType
        // const auto& type = text();
        // return type == "Syntax" or type == "Any" or type == other.text();
    }



    // * Builtins * //
    bool BuiltinType::operator>(const Type& other) const {
        if (dynamic_cast<const TryReassign*>(&other)) return true;

        const auto type = text();
        return type == "Any" and other.text() != "Any";
    }

    bool BuiltinType::operator>=(const Type& other) const {
        if (dynamic_cast<const TryReassign*>(&other)) return true;

        const auto& type = text();
        return type == "Any" or type == other.text();
    }


    // * BuiltinFunction * //
    bool BuiltinFunctionType::operator>(const Type& other) const {
        if (dynamic_cast<const TryReassign*>(&other)) return true;
        return false;
    }

    bool BuiltinFunctionType::operator>=(const Type& other) const {
        if (dynamic_cast<const TryReassign*>(&other)) return true;
        return dynamic_cast<const BuiltinFunctionType*>(&other);
    }



    // * Value Type * //
    std::string ValueType::text(const size_t indent) const { return stringify(*val, indent); }

    bool ValueType::typeCheck(interp::Visitor*, const value::Value& v, const TypePtr& other) const {
        if (std::holds_alternative<expr::Closure>(*val)) { // concept case. test the upcoming value
            util::error();
        }

        if (*val == v) return true;

        return *this >= *other;
    }



    // * Concept Type * //
    std::string ConceptType::text(const size_t indent) const { return stringify(*func, indent); }

    bool ConceptType::typeCheck(interp::Visitor* visitor, const value::Value& v, const TypePtr& other) const {
        const auto& f = get<expr::Closure>(*func);
        // interp::Visitor::ScopeGuard sg{visitor, interp::Visitor::EnvTag::FUNC, f.args_env, f.env};
        interp::Visitor::ScopeGuard sg{visitor, value::EnvTag::FUNC, f.envs.env.env};


        if (not f.type.params[0]->typeCheck(visitor, v, other)) return false;


        // concepts are unary functions, meaning 1 parameter only!
        sg.addEnv({
        {f.params[0].ID,

                {
                    {f.params[0].name},
                    std::make_shared<value::Value>(v),
                    other
                }
            }
        });


        auto ret = visitor->checkReturnType(std::visit(*visitor, f.body->variant()).value, f.type.ret);
        if (std::holds_alternative<bool>(ret)) return get<bool>(ret);

        throw except::TypeMismatch{"Concept didn't return a boolean: " + stringify(*func)};
    }


    // * Class Type * //
    std::string LiteralType::text(const size_t indent) const {
        // return stringify(*cls, indent);
        std::string s;

        if (cls->blueprint->fields.empty())
            s = "class { }";
        else {
            s = "class {\n";

            const std::string space(indent + 4, ' ');
            for (const auto& [name, type, field] : cls->blueprint->fields) {
                s += space + name.stringify() + ": ";

                if (shouldReassign(type)) s += "Any";
                else                      s += type->text(indent + 4);

                s += " = " + field->stringify(indent + 4)+ ";\n";
            }

            s += std::string(indent, ' ') + '}';
        }

        return s;
    }


    bool LiteralType::operator>(const Type& other) const {
        if (dynamic_cast<const TryReassign*>(&other)) return true;

        if (const auto other_cls = dynamic_cast<const LiteralType*>(&other)) {
            for (const auto& [name, type, _] : cls->blueprint->fields) {
                const auto& iter = std::ranges::find_if(other_cls->cls->blueprint->fields, [&name] (const auto& member) {
                    const auto& [n, _, __] = member;
                    return n.name == name.name;
                });

                if (iter == other_cls->cls->blueprint->fields.cend()) return false;

                const auto& [__, t, ___] = *iter;
                if (not (*type > *t)) return false;
            }

            return true;
        }

        const auto type = text();
        return type == "Any" and other.text() != "Any";
    }


    bool LiteralType::operator>=(const Type& other) const {
        if (dynamic_cast<const TryReassign*>(&other)) return true;
        // if (not dynamic_cast<const LiteralType*>(&other)) return false;

        if (const auto other_cls = dynamic_cast<const LiteralType*>(&other)) {
            if (text() == other.text()) return true;

            for (const auto& [name, type, _] : cls->blueprint->fields) {
                const auto& iter = std::ranges::find_if(other_cls->cls->blueprint->fields, [&name] (const auto& member) {
                    const auto& [n, _, __] = member;
                    return n.name == name.name;
                });

                if (iter == other_cls->cls->blueprint->fields.cend()) return false;

                const auto& [__, t, ___] = *iter;
                if (not (*type >= *t)) return false;
            }

            return true;
        }

        const auto type = text();
        return type == "Any"; // or type == other.text();
    }



    // * Union Type * //
    std::string UnionType::text(const size_t indent) const {
        std::string s = "union { ";

        for (std::string pipe{}; const auto& t : types) {

            s += t->text(indent + 4) + "; ";
        }

        return s + '}';
    }

    bool UnionType::involvesT(const Type& T) const {
        for (const auto& type : types)
            if (type->involvesT(T)) return true;

        return false;
    }

    bool UnionType::typeCheck(interp::Visitor* visitor, const value::Value& v, const TypePtr& other) const {
        return std::ranges::any_of(types, [visitor, &v, &other](const auto& type) { return type->typeCheck(visitor, v, other); });
    }

    bool UnionType::operator> (const Type& other) const {
        return std::ranges::any_of(types, [&other](const auto& type) { return *type > other; });
    };

    bool UnionType::operator>=(const Type& other) const {
        return std::ranges::any_of(types, [&other](const auto& type) { return *type >= other; });
    };




    // * Function Type * //
    std::string FuncType::text(const size_t indent) const {
        std::string t = params.empty() ? "" : params[0]->text(indent);
        for (size_t i = 1uz; i < params.size(); ++i)
            t += ", " + params[i]->text(indent);

        return '(' + t + "): " + ret->text(indent);
    }
    bool FuncType::involvesT(const Type& T) const {
        for (const auto& type : params)
            if (type->involvesT(T)) return true;

        return ret->involvesT(T);
    }

    bool FuncType::operator>(const Type& other) const {
        if (dynamic_cast<const TryReassign*>(&other)) return true;
        if (not dynamic_cast<const FuncType*>(&other)) return false;

        // this might throw, but it technically shouldn't
        const auto& that = dynamic_cast<const FuncType&>(other);

        for (const auto& [type1, type2] : std::views::zip(params, that.params)) {
            // if (*type1 >= *type2) return false; // args have an inverse relationship

            if (not (*type2 > *type1)) return false; // args have an inverse relationship
        }


        return *ret > *that.ret;
    }

    bool FuncType::operator>=(const Type& other) const {
        if (dynamic_cast<const TryReassign*>(&other)) return true;
        if (not dynamic_cast<const FuncType*>(&other)) return false;

        // this could throw, but it should never do since we check it above
        const auto& that = dynamic_cast<const FuncType&>(other);

        if (params.size() != that.params.size()) return false;

        for (const auto& [type1, type2] : std::views::zip(params, that.params)) {
            if (not (*type2 >= *type1)) return false; // args have an inverse relationship
        }

        return *ret >= *that.ret;
    }



    // * Variadic Type * //
    std::string VariadicType::text(const size_t indent) const { return "..." + type->text(indent); }

    bool VariadicType::operator>(const Type& other) const {
        if (auto that = dynamic_cast<const VariadicType*>(&other)) return *type > *that->type;

        // variadic type of a type is a superset of that type
        return *type == other or *type > other;
    }

    bool VariadicType::operator>=(const Type& other) const {
        if (auto that = dynamic_cast<const VariadicType*>(&other)) return *type >= *that->type;

        return *type >= other;
    }



    // * List Type * //
    std::string ListType::text(const size_t indent) const { return '{' + type->text(indent) + '}'; }


    bool ListType::typeCheck(interp::Visitor *v, const value::Value& value, const TypePtr&) const {
        if (not std::holds_alternative<value::List>(value)) return false;

        const auto& map = get<value::List>(value);

        for (const auto& val : map.elts->values)
            if (not type->typeCheck(v, val, v->typeOf(val))) return false;

        return true;
    }

    bool ListType::operator>(const Type& other) const {
        if (auto that = dynamic_cast<const ListType*>(&other)) return *type > *that->type;

        return false;
    }

    bool ListType::operator>=(const Type& other) const {
        if (auto that = dynamic_cast<const ListType*>(&other)) return *type >= *that->type;

        return false;
    }



    // * Map Type * //
    std::string MapType::text(const size_t indent) const {
        return '{' + key_type->text(indent + 4) + ": " + val_type->text(indent + 4) + '}';
    }


    bool MapType::typeCheck(interp::Visitor *v, const value::Value& value, const TypePtr&) const {
        if (not std::holds_alternative<value::Map>(value)) return false;

        const auto& map = get<value::Map>(value);

        for (const auto& [key, val] : map.items->map) {
            if (
                not key_type->typeCheck(v, key, v->typeOf(key)) or
                not val_type->typeCheck(v, val, v->typeOf(val))
            )
            return false;
        }

        return true;
    }

    bool MapType::operator>(const Type& other) const {
        if (auto that = dynamic_cast<const MapType*>(&other))
            return *key_type > *that->key_type and *val_type > *that->val_type;

        return false;
    }

    bool MapType::operator>=(const Type& other) const {
        if (auto that = dynamic_cast<const MapType*>(&other))
            return *key_type >= *that->key_type and *val_type >= *that->val_type;

        return false;
    }

} // namespace type
} // namespace pie
