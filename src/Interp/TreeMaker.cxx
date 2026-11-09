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
            fields->fields.emplace_back(name, type::builtins::Any(), std::make_shared<expr::Num>("0", util::SourceSpan{}));
        }

        return std::make_shared<type::LiteralType>(std::make_shared<value::ClassValue>(std::move(fields)));
    }


    value::Object TreeMaker::makeObject(std::vector<std::pair<std::string, value::Value>> members) {
        std::vector<std::tuple<expr::Name, type::TypePtr, value::ValuePtr>> actual_members;
        for (auto& [name, value] : members)
            actual_members.emplace_back(
                expr::Name{std::move(name), util::SourceSpan{}},
                type::builtins::Any(),
                std::make_shared<value::Value>(std::move(value))
            );

        auto type = objectType(actual_members);
        return {std::move(type), std::make_shared<value::Members>(std::move(actual_members))};
    }

    value::Value TreeMaker::patternOf(const expr::unpack::Pattern *pattern) {
        if (not pattern) return makeObject({{"node", "none"}});

        const std::string type = pattern->type ? pattern->type->text() : "";

        if (const auto expr = dynamic_cast<const expr::unpack::Expr*>(pattern))
            return makeObject({
                {"node", "expr_pattern"},
                {"expr", maybe(expr->expr)},
                {"type", type},
                {"default", maybe(pattern->value)},
            });

        if (const auto list = dynamic_cast<const expr::unpack::List*>(pattern)) {
            auto patterns = value::makeList();
            for (const auto& pat : list->patterns) patterns.elts->values.push_back(patternOf(pat.get()));

            return makeObject({
                {"node", "list_pattern"},
                {"patterns", std::move(patterns)},
                {"type", type},
                {"default", maybe(pattern->value)},
            });
        }

        if (const auto pack = dynamic_cast<const expr::unpack::Pack*>(pattern))
            return makeObject({
                {"node", "pack_pattern"},
                {"pattern", patternOf(pack->pattern.get())},
            });

        const auto syn = dynamic_cast<const expr::unpack::Syntax*>(pattern);
        return makeObject({
            {"node", "syntax_pattern"},
            {"expr", maybe(syn->expr)},
        });
    }


    value::Value TreeMaker::maybe(const expr::ExprPtr& expr) {
        if (not expr) return makeObject({{"node", "none"}});
        return std::visit(*this, expr->variant());
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


    value::Object TreeMaker::operator()(expr::FString *fs) {
        auto exprs = value::makeList();
        for (const auto& [at, expr] : fs->exprs)
            exprs.elts->values.push_back(makeObject({
                {"at", BigInt{at}},
                {"expr", std::visit(*this, expr->variant())},
            }));

        return makeObject({
            {"node", "fstring"},
            {"value", fs->str},
            {"exprs", std::move(exprs)},
        });
    }

    value::Object TreeMaker::operator()(expr::Fix *fix) {
        std::vector<std::string> parts{fix->name};
        std::vector<bool> op_pos;

        if (const auto exfix = dynamic_cast<expr::Exfix*>(fix)) parts.push_back(exfix->name2);
        if (const auto mixfix = dynamic_cast<expr::Operator*>(fix)) {
            parts.insert(parts.end(), mixfix->rest.begin(), mixfix->rest.end());
            op_pos = mixfix->op_pos;
        }

        return makeObject({
            {"node", "fix"},
            {"fixity", stringify(fix->type())},
            {"parts", listOf(parts)},
            {"op_pos", listOf(op_pos)},
            {"high", fix->high},
            {"low", fix->low},
            {"shift", BigInt{fix->shift}},
            {"funcs", listOf(fix->funcs)},
        });
    }

    // value::Object TreeMaker::operator()(expr::Prefix   *fix) { return (*this)(static_cast<expr::Fix*>(fix)); }
    // value::Object TreeMaker::operator()(expr::Infix    *fix) { return (*this)(static_cast<expr::Fix*>(fix)); }
    // value::Object TreeMaker::operator()(expr::Suffix   *fix) { return (*this)(static_cast<expr::Fix*>(fix)); }
    // value::Object TreeMaker::operator()(expr::Exfix    *fix) { return (*this)(static_cast<expr::Fix*>(fix)); }
    // value::Object TreeMaker::operator()(expr::Operator *fix) { return (*this)(static_cast<expr::Fix*>(fix)); }

    value::Object TreeMaker::operator()(expr::Assignment *ass) {
        return makeObject({
            {"node", "assignment"},
            {"lhs", std::visit(*this, ass->lhs->variant())},
            {"type", ass->type ? ass->type->text() : ""},
            {"rhs", std::visit(*this, ass->rhs->variant())},
        });
    }

    value::Object TreeMaker::operator()(expr::InferredAssignment *infr) {
        return makeObject({
            {"node", "inferred_assignment"},
            {"name", infr->name.name},
            {"rhs", std::visit(*this, infr->rhs->variant())},
        });
    }

    value::Object TreeMaker::operator()(expr::Unpackment *unpack) {

        // need a proper way to destructure (serialize) unpackment. Unsure how though.
        return makeObject({
            {"node", "unpackment"},
            {"pattern", patternOf(unpack->pattern.get())},
            {"type", unpack->type ? unpack->type->text() : ""},
            {"rhs", std::visit(*this, unpack->rhs->variant())},
        });
    }

    value::Object TreeMaker::operator()(expr::InferredUnpackment *unpack) {
        return makeObject({
            {"node", "inferred_unpackment"},
            {"pattern", patternOf(unpack->pattern.get())},
            {"rhs", std::visit(*this, unpack->rhs->variant())},
        });
    }

    value::Object TreeMaker::operator()(expr::Name *name) {
        return makeObject({
            {"node", "name"},
            {"name", name->name},
        });
    }

    value::Object TreeMaker::operator()(expr::Block *block) {
        return makeObject({
            {"node", "block"},
            {
                "exprs",
                listOf(block->lines)
            },
        });
    }

    value::Object TreeMaker::operator()(expr::Cascade *cas) {
        return makeObject({
            {"node", "cascade"},
            {"exprs", listOf(cas->lines)},
        });
    }

    value::Object TreeMaker::operator()(expr::Closure *closure) {
        const auto params = [this] (
            const std::vector<expr::Closure::Param>& ps,
            const std::vector<type::TypePtr>& types,
            const std::vector<expr::ExprPtr>& defaults
        ) {
            auto list = value::makeList();
            for (size_t i{}; i < ps.size(); ++i) {
                const bool regular = std::holds_alternative<expr::Closure::RegularParam>(ps[i]);

                list.elts->values.push_back(makeObject({
                    {"node", "param"},
                    {"name", regular ?
                        get<expr::Closure::RegularParam>(ps[i]).expr->stringify() :
                        patternOf(get<expr::unpack::PatternPtr>(ps[i]).get())
                    },
                    {"is_syntax", regular and get<expr::Closure::RegularParam>(ps[i]).is_syntax},
                    {"type", i < types.size() and types[i] ? types[i]->text() : ""},
                    {"default", maybe(i < defaults.size() ? defaults[i] : nullptr)},
                }));
            }
            return list;
        };

        return makeObject({
            {"node", "closure"},
            {"params", params(closure->params, closure->type.params, closure->defaults)},
            {"pos_params", params(closure->pos_params, closure->type.pos_params, closure->pos_defaults)},
            {"kw_params", params(closure->kw_params, closure->type.kw_params, closure->kw_defaults)},
            {"variadic_index", closure->variadic_index ? BigInt{*closure->variadic_index} : BigInt{-1}},
            {"ret", closure->type.ret ? closure->type.ret->text() : ""},
            {"body", std::visit(*this, closure->body->variant())},
        });
    }

    value::Object TreeMaker::operator()(expr::Call *call) {
        auto named_args = value::makeList();
        for (const auto& [name, arg] : call->named_args)
            named_args.elts->values.push_back(makeObject({
                {"name", name},
                {"value", std::visit(*this, arg->variant())},
            }));

        return makeObject({
            {"node", "call"},
            {"func", std::visit(*this, call->func->variant())},
            {"args", listOf(call->args)},
            {"named_args", std::move(named_args)},
        });
    }

    value::Object TreeMaker::operator()(expr::List *list) {
        return makeObject({
            {"node", "list"},
            {"elements", listOf(list->elements)},
        });
    }

    value::Object TreeMaker::operator()(expr::Tuple *tuple) {
        return makeObject({
            {"node", "tuple"},
            {"elements", listOf(tuple->elts)},
        });
    }

    value::Object TreeMaker::operator()(expr::Map *map) {
        auto items = value::makeList();
        for (const auto& [key, value] : map->items)
            items.elts->values.push_back(makeObject({
                {"key", std::visit(*this, key->variant())},
                {"value", std::visit(*this, value->variant())},
            }));

        return makeObject({
            {"node", "map"},
            {"items", std::move(items)},
        });
    }

    value::Object TreeMaker::operator()(expr::ListComp *comp) {
        return makeObject({
            {"node", "list_comprehension"},
            {"var", patternOf(comp->var.get())},
            {"kind", maybe(comp->kind)},
            {"guard", maybe(comp->guard)},
            {"body", maybe(comp->body)},
        });
    }

    value::Object TreeMaker::operator()(expr::MapComp *comp) {
        return makeObject({
            {"node", "map_comprehension"},
            {"var", patternOf(comp->var.get())},
            {"kind", maybe(comp->kind)},
            {"guard", maybe(comp->guard)},
            {"key", maybe(comp->body1)},
            {"value", maybe(comp->body2)},
        });
    }

    value::Object TreeMaker::operator()(expr::Expansion *exp) {
        return makeObject({
            {"node", "expansion"},
            {"pack", std::visit(*this, exp->pack->variant())},
        });
    }

    value::Object TreeMaker::operator()(expr::UnaryFold *fold) {
        return makeObject({
            {"node", "unary_fold"},
            {"pack", std::visit(*this, fold->pack->variant())},
            {"op", fold->op},
            {"left_to_right", fold->left_to_right},
        });
    }

    value::Object TreeMaker::operator()(expr::SeparatedUnaryFold *fold) {
        return makeObject({
            {"node", "separated_unary_fold"},
            {"lhs", std::visit(*this, fold->lhs->variant())},
            {"op1", fold->op1},
            {"op2", fold->op2},
            {"rhs", std::visit(*this, fold->rhs->variant())},
        });
    }

    value::Object TreeMaker::operator()(expr::BinaryFold *fold) {
        return makeObject({
            {"node", "binary_fold"},
            {"pack", std::visit(*this, fold->pack->variant())},
            {"init", maybe(fold->init)},
            {"op", fold->op},
            {"left_to_right", fold->left_to_right},
            {"sep", maybe(fold->sep)},
        });
    }

    value::Object TreeMaker::operator()(expr::Class *cls) {
        auto fields = value::makeList();
        for (const auto& [name, type, value] : cls->fields)
            fields.elts->values.push_back(makeObject({
                {"name", name.name},
                {"type", type ? type->text() : ""},
                {"value", maybe(value)},
            }));

        return makeObject({
            {"node", "class"},
            {"fields", std::move(fields)},
        });
    }

    value::Object TreeMaker::operator()(expr::Union *onion) {
        std::vector<std::string> types;
        for (const auto& type : onion->types) types.push_back(type->text());

        return makeObject({
            {"node", "union"},
            {"types", listOf(types)},
        });
    }

    value::Object TreeMaker::operator()(expr::Match *m) {
        auto cases = value::makeList();
        for (const auto& [pattern, guard, body] : m->cases)
            cases.elts->values.push_back(makeObject({
                {"pattern", patternOf(pattern.get())},
                {"guard", maybe(guard)},
                {"body", std::visit(*this, body->variant())},
            }));

        return makeObject({
            {"node", "match"},
            {"expr", std::visit(*this, m->expr->variant())},
            {"cases", std::move(cases)},
        });
    }

    value::Object TreeMaker::operator()(expr::Loop *loop) {
        return makeObject({
            {"node", "loop"},
            {"kind", maybe(loop->kind)},
            {"var", loop->var ? patternOf(loop->var.get()) : ""},
            {"body", std::visit(*this, loop->body->variant())},
            {"els", maybe(loop->els)},
        });
    }

    value::Object TreeMaker::operator()(expr::Break *brake) {
        return makeObject({
            {"node", "break"},
            {"expr", maybe(brake->expr)},
        });
    }

    value::Object TreeMaker::operator()(expr::Continue*) {
        return makeObject({{"node", "continue", }, });
    }

    value::Object TreeMaker::operator()(expr::Access *acc) {
        return makeObject({
            {"node", "access"},
            {"var", std::visit(*this, acc->var->variant())},
            {"name", acc->name},
        });
    }

    value::Object TreeMaker::operator()(expr::Import *import) {
        return makeObject({
            {"node", "import"},
            {"path", import->path.string()},
        });
    }

    value::Object TreeMaker::operator()(expr::Namespace *ns) {
        return makeObject({
            {"node", "namespace"},
            {"name", ns->name},
            {"exprs", listOf(ns->space)},
        });
    }

    value::Object TreeMaker::operator()(expr::UseFix *use) {
        return makeObject({
            {"node", "use_fix"},
            {"global", use->global},
            {"spaces", listOf(use->spaces)},
            {"fixity", stringify(use->filter)},
            {"name", use->op_name},
        });
    }

    value::Object TreeMaker::operator()(expr::UseSpace *use) {
        return makeObject({
            {"node", "use_space"},
            {"global", use->global},
            {"spaces", listOf(use->spaces)},
            {"pull_ops", use->pull_ops},
        });
    }

    value::Object TreeMaker::operator()(expr::Use *use) {
        return makeObject({
            {"node", "use"},
            {"global", use->global},
            {"spaces", listOf(use->spaces)},
            {"name", use->name.name},
        });
    }

    value::Object TreeMaker::operator()(expr::SpaceAccess *sa) {
        return makeObject({
            {"node", "space_access"},
            {"global", sa->global},
            {"spaces", listOf(sa->spaces)},
            {"name", sa->name.name},
        });
    }

    value::Object TreeMaker::operator()(expr::Grouping *g) {
        return makeObject({
            {"node", "grouping"},
            {"expr", std::visit(*this, g->expr->variant())},
        });
    }

    value::Object TreeMaker::operator()(expr::UnaryOp *up) {
        return makeObject({
            {"node", "unary_op"},
            {"op", up->op},
            {"expr", std::visit(*this, up->expr->variant())},
        });
    }

    value::Object TreeMaker::operator()(expr::BinOp *bp) {
        return makeObject({
            {"node", "bin_op"},
            {"lhs", std::visit(*this, bp->lhs->variant())},
            {"op", bp->op},
            {"rhs", std::visit(*this, bp->rhs->variant())},
        });
    }

    value::Object TreeMaker::operator()(expr::PostOp *pp) {
        return makeObject({
            {"node", "post_op"},
            {"expr", std::visit(*this, pp->expr->variant())},
            {"op", pp->op},
        });
    }

    value::Object TreeMaker::operator()(expr::CircumOp *cp) {
        return makeObject({
            {"node", "circum_op"},
            {"op1", cp->op1},
            {"expr", std::visit(*this, cp->expr->variant())},
            {"op2", cp->op2},
        });
    }

    value::Object TreeMaker::operator()(expr::OpCall *oc) {
        auto ops = listOf(oc->rest);
        auto& ops_list = get<value::List>(ops);
        ops_list.elts->values.insert(ops_list.elts->values.cbegin(), oc->first);

        return makeObject({
            {"node", "op_call"},
            {"ops", std::move(ops)},
            {"exprs", listOf(oc->exprs)},
            {"op_pos", listOf(oc->op_pos)},
        });
    }

    value::Object TreeMaker::operator()(expr::Hole *hole) {
        return makeObject({
            {"node", "hole"},
            {"expr", std::visit(*this, hole->expr->variant())},
        });
    }

    value::Object TreeMaker::operator()(expr::Syntax *syn) {
        return makeObject({
            {"node", "syntax"},
            {"expr", std::visit(*this, syn->expr->variant())},
        });
    }

    value::Object TreeMaker::operator()(expr::Type *type) {
        return makeObject({
            {"node", "type"},
            {"type", type->type->text()},
        });
    }


}
} // namespace pie

