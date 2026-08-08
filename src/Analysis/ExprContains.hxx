#pragma once

#include "../Expr/Expr.hxx"
#include <cstddef>

inline namespace pie {
namespace analysis {

template <typename EXPR> const EXPR *checkPattern(expr::Match::Case::Pattern &);

template <typename EXPR> const EXPR *exprContains(expr::ExprPtr expr) {
    if (auto p = dynamic_cast<const EXPR *>(expr.get())) return p;

    if (dynamic_cast<const expr::Num  *>(expr.get())) return nullptr;
    if (dynamic_cast<const expr::Bool *>(expr.get())) return nullptr;
    if (dynamic_cast<const expr::Name *>(expr.get())) return nullptr;

    if (auto l = dynamic_cast<const expr::List *>(expr.get())) {
        for (const auto &e : l->elements)
            if (auto p = exprContains<EXPR>(e))
                return p;

        return nullptr;
    }


    if (auto m = dynamic_cast<const expr::Map *>(expr.get())) {
        for (const auto &[key, value] : m->items) {
            if (auto p = exprContains<EXPR>(key  )) return p;
            if (auto p = exprContains<EXPR>(value)) return p;
        }

        return nullptr;
    }


    if (auto e = dynamic_cast<const expr::Expansion *>(expr.get())) {
        return exprContains<EXPR>(e->pack);
    }

    if (auto f = dynamic_cast<const expr::UnaryFold *>(expr.get())) {
        return exprContains<EXPR>(f->pack);
    }


    if (auto f = dynamic_cast<const expr::SeparatedUnaryFold *>(expr.get())) {
        if (auto p = exprContains<EXPR>(f->lhs)) return p;

        return exprContains<EXPR>(f->rhs);
    }

    if (auto f = dynamic_cast<const expr::BinaryFold *>(expr.get())) {
        if (auto p = exprContains<EXPR>(f->init)) return p;
        if (auto p = exprContains<EXPR>(f->pack)) return p;

        if (f->sep) return exprContains<EXPR>(f->sep);

        return nullptr;
    }

    if (auto ass = dynamic_cast<const expr::Assignment *>(expr.get())) {
        if (auto p = exprContains<EXPR>(ass->lhs)) return p;

        // todo: check if you need to check the type

        return exprContains<EXPR>(ass->rhs);
    }

    if (auto cls = dynamic_cast<const expr::Class *>(expr.get())) {
        for (const auto &[_, __, expr] : cls->fields) {
        // todo check the types
        if (auto p = exprContains<EXPR>(expr))
            return p;
        }

        return nullptr;
    }

    if (auto onion = dynamic_cast<const expr::Union *>(expr.get())) {
        // todo: I think I need to check the types

        return nullptr;
    }

    if (auto match = dynamic_cast<const expr::Match *>(expr.get())) {
        if (auto p = exprContains<EXPR>(match->expr)) return p;

        for (const auto &kase : match->cases) {
            if (auto p = checkPattern<EXPR>(*kase.pattern)) return p;

            if (kase.guard)
                if (auto p = exprContains<EXPR>(kase.guard))

            return p;
        }

        return nullptr;
    }

    if (auto t = dynamic_cast<const expr::Type *>(expr.get())) {
        if (auto expr_type = dynamic_cast<const type::ExprType *>(t->type.get()))
            return exprContains<EXPR>(expr_type->t);

        return nullptr;
    }

    if (auto l = dynamic_cast<const expr::Loop *>(expr.get())) {
        if (l->kind)
            if (auto p = exprContains<EXPR>(l->kind)) return p;

        if (auto p = exprContains<EXPR>(l->body)) return p;

        if (l->els)
            if (auto p = exprContains<EXPR>(l->els)) return p;

        return nullptr;
    }

    if (auto b = dynamic_cast<const expr::Break *>(expr.get())) {
        return exprContains<EXPR>(b->expr);
    }

    if (dynamic_cast<const expr::Continue *>(expr.get())) {
        return nullptr;
    }

    if (auto acc = dynamic_cast<const expr::Access *>(expr.get())) {
        return exprContains<EXPR>(acc->var);
    }

    if (auto ns = dynamic_cast<const expr::Namespace *>(expr.get())) {
        for (const auto &expr : ns->space) {
        if (auto p = exprContains<EXPR>(expr))
            return p;
        }

        return nullptr;
    }

    if (auto acc = dynamic_cast<const expr::SpaceAccess *>(expr.get())) {
        return nullptr;
    }

    if (auto use = dynamic_cast<const expr::Use *>(expr.get())) {
        return nullptr;
    }

    if (auto use = dynamic_cast<const expr::UseSpace *>(expr.get())) {
        return nullptr;
    }

    if (auto import = dynamic_cast<const expr::Import *>(expr.get())) {
        return nullptr;
    }

    if (auto syn = dynamic_cast<const expr::Syntax *>(expr.get())) {
        // return exprContains<EXPR>(syn->expr);
        return nullptr;
    }

    if (auto g = dynamic_cast<const expr::Grouping *>(expr.get())) {
        return exprContains<EXPR>(g->expr);
    }

    if (auto op = dynamic_cast<const expr::UnaryOp *>(expr.get())) {
        return exprContains<EXPR>(op->expr);
    }

    if (auto op = dynamic_cast<const expr::BinOp *>(expr.get())) {
        if (auto p = exprContains<EXPR>(op->lhs)) return p;

        return exprContains<EXPR>(op->rhs);
    }

    if (auto op = dynamic_cast<const expr::PostOp *>(expr.get())) {
        return exprContains<EXPR>(op->expr);
    }

    if (auto op = dynamic_cast<const expr::CircumOp *>(expr.get())) {
        return exprContains<EXPR>(op->expr);
    }

    if (auto op = dynamic_cast<const expr::OpCall *>(expr.get())) {
        for (const auto &arg : op->exprs) {
            if (auto p = exprContains<EXPR>(arg)) return p;
        }

        return nullptr;
    }


    if (auto call = dynamic_cast<const expr::Call *>(expr.get())) {
        if (auto p = exprContains<EXPR>(call->func)) return p;

        for (const auto &arg : call->args) {
            if (auto p = exprContains<EXPR>(arg)) return p;
        }

        return nullptr;
    }

    if (auto closure = dynamic_cast<const expr::Closure *>(expr.get())) {
        return exprContains<EXPR>(closure->body);
    }

    if (auto block = dynamic_cast<const expr::Block *>(expr.get())) {
        for (const auto &expr : block->lines) {
            if (auto p = exprContains<EXPR>(expr)) return p;
        }

        return nullptr;
    }

    if (auto fix = dynamic_cast<const expr::Fix *>(expr.get())) {
        for (const auto &expr : fix->funcs) {
            if (auto p = exprContains<EXPR>(expr)) return p;
        }

        return nullptr;
    }

    return nullptr;
    }

    template <typename EXPR>
    const EXPR *checkPattern(expr::Match::Case::Pattern &pat) {
    if (std::holds_alternative<expr::Match::Case::Pattern::Single>(pat.pattern)) {
        auto &pattern = get<expr::Match::Case::Pattern::Single>(pat.pattern);

        // todo: check this type as well!
        // if (pattern.type)
        // exprContains<Expr>(expr::Type{pattern.type})

        if (pattern.value)
        return exprContains<EXPR>(pattern.value);
    } else { // holds Match::Case::Pattern::Structure
        for (const auto &pat :
            get<expr::Match::Case::Pattern::Structure>(pat.pattern).patterns) {
        if (auto p = checkPattern<EXPR>(*pat))
            return p;
        }
    }

    return nullptr;
}

} // namespace analysis
} // namespace pie
