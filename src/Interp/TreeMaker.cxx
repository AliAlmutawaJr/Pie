#include "TreeMaker.hxx"

#include <memory>
#include <tuple>
#include <variant>
#include <vector>

#include "../Expr/Expr.hxx"
#include "../Value/Value.hxx"
#include "../Type/Type.hxx"


namespace pie {
namespace interp {

    // default type for objects
    type::TypePtr TreeMaker::objectType(
        const std::vector<std::tuple<expr::Name, type::TypePtr, value::ValuePtr>>& members
    ) {
        auto fields = std::make_shared<value::Fields>();

        for (const auto& [name, typ_e, __] : members) {
            fields->fields.emplace_back(name, type::builtins::Any(), std::make_shared<expr::Num>("0"));
        }

        return std::make_shared<type::LiteralType>(std::make_shared<value::ClassValue>(std::move(fields)));
    }


    value::Object TreeMaker::makeObject(std::vector<std::pair<std::string, value::Value>> members) {
        std::vector<std::tuple<expr::Name, type::TypePtr, value::ValuePtr>> actual_members;
        for (auto& [name, value] : members)
            actual_members.emplace_back(
                std::move(name),
                type::builtins::Any(),
                std::make_shared<value::Value>(std::move(value))
            );

        auto type = objectType(actual_members);
        return {std::move(type), std::make_shared<value::Members>(std::move(actual_members))};
    }


    value::Object TreeMaker::operator()(expr::Num *num) {
        return makeObject({
            {"node", "num"},
            {"value", num->num},
        });
    }

    value::Object TreeMaker::operator()(expr::Bool *boo) {
        return makeObject({
            {"node", "bool"},
            {"value", boo->boolean},
        });
    }

    value::Object TreeMaker::operator()(expr::String *str) {
        return makeObject({
            {"node", "string"},
            {"value", str->str},
        });
    }


    // value::Object TreeMaker::operator()(expr::FString*);

    // value::Object TreeMaker::operator()(expr::Fix*);

    value::Object TreeMaker::operator()(expr::Assignment *ass) {
        return makeObject({
            {"node", "assignment"},
            {"lhs", std::visit(*this, ass->lhs->variant())},
            // {"type", std::visit(*this, ass->type)}
            {"rhs", std::visit(*this, ass->rhs->variant())},
        });
    }

    // value::Object TreeMaker::operator()(expr::InferredAssignment*);

    // value::Object TreeMaker::operator()(expr::Unpackment *);

    value::Object TreeMaker::operator()(expr::Name *name) {
        return makeObject({
            {"node", "name"},
            {"name", name->name},
        });
    }

    value::Object TreeMaker::operator()(expr::Block *block) {
        auto list = value::makeList();
        for (const auto& expr : block->lines) {
            list.elts->values.push_back(std::visit(*this, expr->variant()));
        }

        return makeObject({
            {"node", "block"},
            {
                "exprs",
                std::move(list)
            },
        });
    }

    // value::Object TreeMaker::operator()(expr::Closure*);

    // value::Object TreeMaker::operator()(expr::Call*);

    // value::Object TreeMaker::operator()(expr::List*);

    // value::Object TreeMaker::operator()(expr::Map*);

    // value::Object TreeMaker::operator()(expr::ListComp*);

    // value::Object TreeMaker::operator()(expr::MapComp*);

    // value::Object TreeMaker::operator()(expr::Expansion*);

    // value::Object TreeMaker::operator()(expr::UnaryFold*);

    // value::Object TreeMaker::operator()(expr::SeparatedUnaryFold*);

    // value::Object TreeMaker::operator()(expr::BinaryFold*);

    // value::Object TreeMaker::operator()(expr::Class*);

    // value::Object TreeMaker::operator()(expr::Union*);

    // value::Object TreeMaker::operator()(expr::Match*);

    // value::Object TreeMaker::operator()(expr::Loop*);

    // value::Object TreeMaker::operator()(expr::Break*);

    // value::Object TreeMaker::operator()(expr::Continue*);

    // value::Object TreeMaker::operator()(expr::Access*);

    // value::Object TreeMaker::operator()(expr::Import*);

    // value::Object TreeMaker::operator()(expr::Namespace*);

    // value::Object TreeMaker::operator()(expr::UseFix*);

    // value::Object TreeMaker::operator()(expr::UseSpace*);

    // value::Object TreeMaker::operator()(expr::Use*);

    // value::Object TreeMaker::operator()(expr::SpaceAccess*);

    // value::Object TreeMaker::operator()(expr::Grouping*);

    // value::Object TreeMaker::operator()(expr::UnaryOp*);

    // value::Object TreeMaker::operator()(expr::BinOp*);

    // value::Object TreeMaker::operator()(expr::PostOp*);

    // value::Object TreeMaker::operator()(expr::CircumOp*);

    // value::Object TreeMaker::operator()(expr::OpCall*);

    // value::Object TreeMaker::operator()(expr::Syntax*);

    // value::Object TreeMaker::operator()(expr::Type*);


}
} // namespace pie


