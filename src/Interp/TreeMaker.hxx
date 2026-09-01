#pragma once

#include <vector>
#include <utility>
#include <tuple>


#include "../Declarations.hxx"


inline namespace pie {
namespace interp {


class TreeMaker {

    [[nodiscard]] static value::Object makeObject(std::vector<std::pair<std::string, value::Value>> members);
    [[nodiscard]] static type::TypePtr objectType(const std::vector<std::tuple<expr::Name, type::TypePtr, value::ValuePtr>>&);

public:

    value::Object operator()(expr::Num*);

    value::Object operator()(expr::Bool*);

    value::Object operator()(expr::String*);

    // value::Object operator()(expr::FString*);

    // value::Object operator()(expr::Fix*);

    value::Object operator()(expr::Assignment*);

    // value::Object operator()(expr::InferredAssignment*);

    // value::Object operator()(expr::Unpackment *);

    value::Object operator()(expr::Name*);

    value::Object operator()(expr::Block*);

    // value::Object operator()(expr::Closure*);

    // value::Object operator()(expr::Call*);

    // value::Object operator()(expr::List*);

    // value::Object operator()(expr::Map*);

    // value::Object operator()(expr::ListComp*);

    // value::Object operator()(expr::MapComp*);

    // value::Object operator()(expr::Expansion*);

    // value::Object operator()(expr::UnaryFold*);

    // value::Object operator()(expr::SeparatedUnaryFold*);

    // value::Object operator()(expr::BinaryFold*);

    // value::Object operator()(expr::Class*);

    // value::Object operator()(expr::Union*);

    // value::Object operator()(expr::Match*);

    // value::Object operator()(expr::Loop*);

    // value::Object operator()(expr::Break*);

    // value::Object operator()(expr::Continue*);

    // value::Object operator()(expr::Access*);

    // value::Object operator()(expr::Import*);

    // value::Object operator()(expr::Namespace*);

    // value::Object operator()(expr::UseFix*);

    // value::Object operator()(expr::UseSpace*);

    // value::Object operator()(expr::Use*);

    // value::Object operator()(expr::SpaceAccess*);

    // value::Object operator()(expr::Grouping*);

    // value::Object operator()(expr::UnaryOp*);

    // value::Object operator()(expr::BinOp*);

    // value::Object operator()(expr::PostOp*);

    // value::Object operator()(expr::CircumOp*);

    // value::Object operator()(expr::OpCall*);

    // value::Object operator()(expr::Syntax*);

    // value::Object operator()(expr::Type*);


    value::Object operator()(auto*) { return {nullptr, std::make_shared<value::Members>()}; }
};

}
} // namespace pie


