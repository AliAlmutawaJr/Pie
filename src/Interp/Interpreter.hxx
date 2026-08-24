#pragma once

#include <concepts>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <variant>
#include <vector>
#include <unordered_map>
#include <ranges>
#include <algorithm>
#include <iterator>
#include <optional>
#include <utility>


#include <cctype>
#include <cassert>
#include <dlfcn.h>

#include <stdx/tuple.hpp>

#if not WEB_PIE
#include <ffi.h>
#include "FFI.hxx"
#endif


#include "../Functions/BuiltinFunctions.hxx"
#include "../Utils/utils.hxx"
#include "../Utils/Exceptions.hxx"
#include "../Utils/ConstexprLookup.hxx"
#include "../Lex/Lexer.hxx"
#include "../Analysis/LexicalAnalysis.hxx"
#include "../Expr/Expr.hxx"
#include "../Type/Type.hxx"
#include "../Parser/Parser.hxx"

#include "../Value/Value.hxx"


inline namespace pie {

namespace interp {

struct NameSpace {
    std::string name;
    std::unordered_map<
        ssize_t,
        std::tuple<
            value::SpaceRef,
            value::ValuePtr,
            type::TypePtr
        >
    > members;
    Operators prefix_op_env;
    Operators op_env;

    std::unordered_map<std::string, std::shared_ptr<NameSpace>> children;
};



class Visitor {

    const std::filesystem::path root;


    // maintain invariant: these two vectors MUST be the same size!
    std::vector<std::shared_ptr<value::Env>> env;
    std::vector<std::vector<std::pair<expr::ExprPtr, std::shared_ptr<value::Env>>>> deferred;

    std::unordered_map<std::string, std::shared_ptr<NameSpace>> global_spaces;
    std::vector<NameSpace*> current_space;

    // _this_ (or self) context
    std::vector<value::Object> selves{};


    // loop context
    ssize_t loop_counter{};
    bool broken{}, continued{};


    // v_table ahh name
    std::unordered_map<std::string, std::vector<size_t>> co_map;

    std::vector<size_t> import_indices;

public:


    Visitor(std::vector<size_t> indices, std::filesystem::path r = ".") noexcept
    : root{r.parent_path()}, env{{std::make_shared<value::Env>()}}, deferred{{}}, import_indices{std::move(indices)}
    { }


    ~Visitor() {
        // no need to mess with captured envs since this is global scope
        for (const auto& [expr, env] : deferred[0] | std::views::reverse) {
            ScopeGuard sg{this, env->env};
            sg.addPrefixOps(env->prefix_op_env);
            sg.addOps(env->op_env);
            std::visit(*this, expr->variant());
        }
    }

    // void addOperators(Operators os) {
    //     // ops.insert(os.begin(), os.end());
    //     // ops.merge(std::move(os));

    //     for (auto& [name, fix] : os) {
    //         if (ops.contains(name)) {
    //             for (auto& func : fix->funcs) {
    //                 ops.at(name)->funcs.push_back(std::move(func));
    //             }
    //         }
    //         else ops[std::move(name)] = std::move(fix);
    //     }
    // }

private:

    bool prefixOpsContain(const std::string& op) const {
        // no need to reverse in this case
        for (const auto& e : env) {
            if (e->prefix_op_env.contains(op)) return true;
        }

        return false;
    }



    bool opsContain(const std::string& op) const {
        // no need to reverse in this case
        for (const auto& e : env) {
            if (e->op_env.contains(op)) return true;
        }

        return false;
    }


    const std::shared_ptr<expr::Fix>& findPrefixOp(const std::string& op, const std::source_location& loc = std::source_location::current()) const {
        for (const auto& e : std::views::reverse(env)) {
            if (e->prefix_op_env.contains(op)) return e->prefix_op_env.at(op);
        }

        // util::error("Operator `" + op + "` not found!");
        util::error(loc);
    }


    const std::shared_ptr<expr::Fix>& findOp(const std::string& op, const std::source_location& loc = std::source_location::current()) const {
        for (const auto& e : std::views::reverse(env)) {
            if (e->op_env.contains(op)) return e->op_env.at(op);
        }

        // util::error("Operator `" + op + "` not found!");
        util::error(loc);
    }




public:

    struct ValueType {
        value::Value value;
        type::TypePtr type;

        ValueType(value::Value value, type::TypePtr type) noexcept
        : value{std::move(value)}, type{std::move(type)} {}

        // ValueType(value::Value value) noexcept
        // : value{std::move(value)}, type{typeOf(value)} {}

        // operator value::Value() { return value; }
    };


    // value::Value evalExpr(const expr::ExprPtr& expr) { 
    //     return std::visit(*this, expr->variant());
    // }



    // to delay stringifying until needed.
    // avoid slow recursive virtual calls and string allocations
    static auto liftName(const expr::Expr* expr) { return [expr] { return expr->stringify(); }; }


    ValueType operator()(const expr::Num *n) {
        if (const auto& var = getVar(n->var_ID, liftName(n)); var) return *var;


        // have to do an if rather than ternary so the return value isn't always coerced into doubles
        if (n->num.find('.') != std::string::npos) return {std::stod(n->num), type::builtins::Double()};
        else return {std::stoll(n->num), type::builtins::Int()};
    }


    ValueType operator()(const expr::Bool *b) {
        if (const auto& var = getVar(b->var_ID, liftName(b)); var) return *var;

        return {b->boolean, type::builtins::Bool()};
    }


    ValueType operator()(const expr::String *s) {
        if (const auto& var = getVar(s->var_ID, liftName(s)); var) return *var;

        return {s->str, type::builtins::String()};
    }


    ValueType operator()(const expr::FString *fs) {
        if (const auto& var = getVar(fs->var_ID, liftName(fs)); var) return *var;

        std::string s;

        for (size_t i{}, e{}; i < fs->str.size() or e < fs->exprs.size(); ++i) {
            for (; e < fs->exprs.size() and fs->exprs[e].first == i; ++e) {
                s += value::stringify(std::visit(*this, fs->exprs[e].second->variant()).value, 2);
            }

            if (i < fs->str.size()) s.push_back(fs->str[i]);
        }

        return {s, type::builtins::String()};
    }


    std::optional<ValueType> checkMemberInThisObject(const std::string& name) const {
        if (selves.empty()) return {};

        for (const auto& self : std::views::reverse(selves)) {
            for (const auto& [member, type, value] : self.second->members) {
                if (member.name == name) return {{*value, type}};
            }
        }

        return {};
    }

    std::optional<ValueType> checkMemberInThisObject(const ssize_t ID) const {
        if (selves.empty()) return {};

        for (const auto& self : std::views::reverse(selves)) {
            for (const auto& [member, type, value] : self.second->members) {
                if (member.var_ID == ID) return {{*value, type}};
            }
        }

        return {};
    }


    ValueType changeThis(const std::string& name, const value::Value& val) {
        if (selves.empty()) util::error();

        for (const auto& self : std::views::reverse(selves)){
            for (auto& [member, type, value] : self.second->members) {
                if (member.name == name) {
                    *value = val;
                    return {val, type};
                }
            }
        }

        util::error("Name '" + name + "' not found in object: " + stringify(selves.back()));
    }

    ValueType fetchRef(const expr::Name *n) {
        for (const auto& e : std::views::reverse(env)) {
            if (e->env.contains(n->var_ID)) {
                const auto& [named_ref, value_ptr, type_ptr] = e->env.at(n->var_ID);
                const auto& [_, space] = named_ref;

                if (not space or not space->members.contains(n->var_ID)) 
                    util::error();

                const auto& member = space->members[n->var_ID];
                return {*get<value::ValuePtr>(member), get<type::TypePtr>(member)};
            }
        }


        util::error();
    }


    ValueType operator()(const expr::Name *n) {
        // what should builtins evaluate to?
        // If I return the string back, then expressions like `"__builtin_neg"(1)` are valid now :))))
        // interesting!
        // how about a special value?

        if (const auto& var = getVar(n->var_ID, liftName(n)); var) {
            if (isRef(n->var_ID)) return fetchRef(n);

            return *var;
        }

        if (const auto var = checkMemberInThisObject(n->name); var) return *var;

        if (n->name == "self" and not selves.empty()) return {selves.back(), typeOf(selves.back())};


        // for now, buitlin functions just return their names as strings...
        // maybe i need to return some builtin type or smth. IDK
        if (isBuiltin(n->name)) return {value::BuiltinFunction{n->name}, type::builtins::BuiltinFunction()};


        if (n->name == "Any"   ) return {type::builtins::Any   (), type::builtins::Type()};
        if (n->name == "Int"   ) return {type::builtins::Int   (), type::builtins::Type()};
        if (n->name == "Double") return {type::builtins::Double(), type::builtins::Type()};
        if (n->name == "String") return {type::builtins::String(), type::builtins::Type()};
        if (n->name == "Bool"  ) return {type::builtins::Bool  (), type::builtins::Type()};
        if (n->name == "Type"  ) return {type::builtins::Type  (), type::builtins::Type()};
        if (n->name == "Syntax") return {type::builtins::Syntax(), type::builtins::Type()};


        // printEnv(env);
        util::error("Name `" + n->name + "`, with ID [" + std::to_string(n->var_ID) + "] is not defined!");
    }


    ValueType operator()(const expr::Expansion *exp) { util::error("Can only expand in function calls or fold expressions: `" + exp->stringify() + "`"); }


    ValueType operator()(const expr::List *list) {
        if (const auto& var = getVar(list->var_ID, liftName(list)); var) return *var;

        std::vector<value::Value> values;
        std::transform(
            list->elements.cbegin(), list->elements.cend(), std::back_inserter(values),
            [this] (const auto& expr) { return std::visit(*this, expr->variant()).value; }
        );

        auto list_value = value::makeList(std::move(values));
        auto type = typeOf(list_value);

        return {std::move(list_value), std::move(type)};
    }


    ValueType operator()(const expr::Map *map) {
        value::Map map_value{std::make_shared<value::Items>()};

        for (auto [key, expr] : map->items) {
            // evaluating key here first instead of inside the call to .insert_or_assign()
            // because functions arguments are indeterminantly evaluated
            // and I want the evaluation order to be l2r in case of side-effects

            auto key_value = std::visit(*this, std::move(key)->variant()).value;
            map_value.items->map.insert_or_assign(
                std::move(key_value),
                std::visit(*this, std::move(expr)->variant()).value
            );
        }

        auto type = typeOf(map_value);
        return {std::move(map_value), std::move(type)};
    }


    value::Value typeCheck(const value::Value& value, const type::TypePtr& type, std::string err_msg = "", const std::source_location& location = std::source_location::current()) {
        const auto value_type = typeOf(value);
        if (err_msg.empty()) err_msg = "Expected type '" + type->text() + "', got type '" + value_type->text() + '\'';

        if (not type->typeCheck(this, value, value_type)) util::error<except::TypeMismatch>(err_msg, location);

        if (const auto cls = type::isClass(type)) {
            auto obj = get<value::Object>(value);
            obj.second = std::make_shared<value::Members>(obj.second->members);

            std::erase_if(
                obj.second->members,
                [cls] (const auto& member) {
                    const auto& [name, _, __] = member;

                    return std::ranges::find_if(cls->cls->blueprint->fields, [&name](const auto& field) {
                        return get<expr::Name>(field).name == name.name;
                    }) == cls->cls->blueprint->fields.cend();
                }
            );
            obj.first = type;

            return obj;
        }

        return value;
    }


    ValueType operator()(const expr::UnaryFold *fold) {
        if (const auto& var = getVar(fold->var_ID, liftName(fold)); var) return *var;

        value::Value pack = std::visit(*this, fold->pack->variant()).value;

        if (not std::holds_alternative<value::Pack>(pack)) util::error("Folding over a non-pack: " + stringify(pack));

        auto& packlist = get<value::Pack>(pack);

        if (packlist->values.empty()) util::error("Folding over an empty pack: " + fold->stringify());
        if (packlist->values.size() == 1) return {packlist->values[0], typeOf(packlist->values[0])};


        value::Value ret = fold->left_to_right ? packlist->values.front() : packlist->values.back(); // [packlist->values.size() - 2];
        const auto values = fold->left_to_right?
            packlist->values |                       std::views::drop(1) | std::views::as_rvalue | std::ranges::to<std::vector<value::Value>>():
            packlist->values | std::views::reverse | std::views::drop(1) | std::views::as_rvalue | std::ranges::to<std::vector<value::Value>>();

        const auto& op = findOp(fold->op);

        // // can't have any syntax type since the pack consists of values, not expressions..
        // // unless...!
        // // TODO: allow for folding over syntax...maybe
        // checkNoSyntaxType(op->funcs);

        expr::Closure* func;

        const size_t  first_idx = 1 - fold->left_to_right;
        const size_t second_idx =     fold->left_to_right;

        // no overload resolution required
        if (op->funcs.size() == 1) {
            func = dynamic_cast<expr::Closure*>(op->funcs[0].get());

            func->type.ret =                validateType(std::move(func)->type.ret               );
            func->type.params[ first_idx] = validateType(std::move(func)->type.params[ first_idx]);
            func->type.params[second_idx] = validateType(std::move(func)->type.params[second_idx]);


            for (const auto& value : values) {

                typeCheck(ret, func->type.params[first_idx],
                    "Type mis-match in Fold expressions with Infix operator '" + fold->op + 
                    "', parameter '" + func->params[0].name +
                    "' expected: " + func->type.params[0]->text() +
                    ", got: " + stringify(ret) + " which is " + typeOf(ret)->text()
                );


                typeCheck(value, func->type.params[second_idx], 
                    "Type mis-match in Fold expressions with Infix operator '" + fold->op + 
                    "', parameter '" + func->params[1].name +
                    "' expected: " + func->type.params[1]->text() +
                    ", got: " + stringify(value) + " which is " + typeOf(value)->text()
                );

                value::Environment args_env;
                args_env[func->params[ first_idx].ID] = {{func->params[ first_idx].name}, std::make_shared<value::Value>(ret  ), func->type.params[ first_idx]};
                args_env[func->params[second_idx].ID] = {{func->params[second_idx].name}, std::make_shared<value::Value>(value), func->type.params[second_idx]};


                ScopeGuard sg{this, args_env};

                ret = checkReturnType(std::visit(*this, func->body->variant()).value, func->type.ret);
            }
        }
        else { // fuck me
            // checkNoSyntaxType(op->funcs);

            for (const auto& value : values) {

                // if the overload set is resolved, no need to type check again...i think!!!
                func = resolveOverloadSet(
                    op->OpName(), op->funcs,
                    {ret, value}
                    // {validateType(typeOf(ret)), validateType(typeOf(value))},
                );
                func->type.ret                = validateType(std::move(func)->type.ret               );
                func->type.params[ first_idx] = validateType(std::move(func)->type.params[ first_idx]);
                func->type.params[second_idx] = validateType(std::move(func)->type.params[second_idx]);


                value::Environment args_env;
                args_env[func->params[1 - fold->left_to_right].ID] =
                    {{func->params[1 - fold->left_to_right].name}, std::make_shared<value::Value>(ret)  , func->type.params[1 - fold->left_to_right]};

                args_env[func->params[    fold->left_to_right].ID] =
                    {{func->params[    fold->left_to_right].name}, std::make_shared<value::Value>(value), func->type.params[    fold->left_to_right]};


                ScopeGuard sg{this, args_env};

                ret = checkReturnType(std::visit(*this, func->body->variant()).value, func->type.ret);
            }
        }

        return {ret, func->type.ret};
    }


    ValueType operator()(const expr::SeparatedUnaryFold *fold) {
        if (const auto& var = getVar(fold->var_ID, liftName(fold)); var) return *var;


        value::Value lhs = std::visit(*this, fold->lhs->variant()).value;
        value::Value rhs = std::visit(*this, fold->rhs->variant()).value;


        const auto l_pack = std::holds_alternative<value::Pack>(lhs), l2r = l_pack;
        const auto r_pack = std::holds_alternative<value::Pack>(rhs);


        if (l_pack == r_pack) {
            std::string err = l_pack ? "Folding over 2 packs: '" : "Folding over non-packs: '";
            util::error(err + fold->lhs->stringify() + "' and '" + fold->rhs->stringify() + '\'');
        }

        value::Value pack = l_pack? std::move(lhs) : std::move(rhs);
        value::Value sep  = r_pack? std::move(lhs) : std::move(rhs);

        auto& packlist = get<value::Pack>(pack);

        if (packlist->values.empty()) util::error("Folding over an empty pack: " + fold->stringify());
        if (packlist->values.size() == 1) return {packlist->values[0], typeOf(packlist->values[0])};


        value::Value ret;
        std::vector<value::Value> values;
        values.reserve(packlist->values.size() * 2 - 1);
        if (l2r) {
            ret = std::move(packlist)->values[0];

            for (auto& value : packlist->values | std::views::drop(1)) {
                values.push_back(sep);
                values.push_back(std::move(value));
            }
        }
        else {
            ret = sep;

            const auto len = packlist->values.size();

            values.push_back(std::move(packlist)->values[len - 1]);
            values.push_back(std::move(packlist)->values[len - 2]);

            for (auto& value : packlist->values | std::views::reverse | std::views::drop(2)) {
                values.push_back(sep);
                values.push_back(std::move(value));
            }
        }




        // // can't have any syntax type since the pack consists of values, not expressions..
        // // unless...!
        // // TODO: allow for folding over syntax...maybe

        // checkNoSyntaxType(op->funcs);

        type::TypePtr ret_type;
        const size_t  first_idx = 1 - l2r;
        const size_t second_idx =     l2r;

        const auto& op1 = findOp(fold->op1);
        const auto& op2 = findOp(fold->op2);
        for (size_t i{}; const auto& value : values) {
            expr::Closure *func = 
                (i++) % 2 == 1 ? // start with the second operator
                    resolveOverloadSet(op1->OpName(), op1->funcs, {ret, value}) :
                    resolveOverloadSet(op2->OpName(), op2->funcs, {ret, value}) ;

            // I think these lines are needed. Have to check 
            func->type.ret                = validateType(std::move(func)->type.ret             );
            func->type.params[ first_idx] = validateType(std::move(func)->type.params[ first_idx]);
            func->type.params[second_idx] = validateType(std::move(func)->type.params[second_idx]);

            // this will pick the the last func called's type
            ret_type = func->type.ret;

            value::Environment args_env;
            args_env[func->params[ first_idx].ID] = {{func->params[ first_idx].name}, std::make_shared<value::Value>(ret)  , func->type.params[ first_idx]};
            args_env[func->params[second_idx].ID] = {{func->params[second_idx].name}, std::make_shared<value::Value>(value), func->type.params[second_idx]};


            ScopeGuard sg{this, args_env};

            ret = checkReturnType(std::visit(*this, func->body->variant()).value, func->type.ret);
        }

        // // no overload resolution required
        // if (op->funcs.size() == 1) {
        //     func = dynamic_cast<expr::Closure*>(op->funcs[0].get());
        //     func->type.ret                = validateType(std::move(func)->type.ret               );
        //     func->type.params[ first_idx] = validateType(std::move(func)->type.params[ first_idx]);
        //     func->type.params[second_idx] = validateType(std::move(func)->type.params[second_idx]);



        //     for (const auto& value : values) {

        //         typeCheck(ret, func->type.params[first_idx],
        //             "Type mis-match in Fold expressions with Infix operator '" + fold->op + 
        //             "', parameter '" + func->params[0].name +
        //             "' expected: " + func->type.params[0]->text() +
        //             ", got: " + stringify(ret) + " which is " + typeOf(ret)->text()
        //         );

        //         typeCheck(ret, func->type.params[second_idx],
        //             "Type mis-match in Fold expressions with Infix operator '" + fold->op + 
        //             "', parameter '" + func->params[1].name +
        //             "' expected: " + func->type.params[1]->text() +
        //             ", got: " + stringify(ret) + " which is " + typeOf(ret)->text()
        //         );


        //         value::Environment args_env;
        //         args_env[func->params[ first_idx].ID] = {{func->params[ first_idx].name}, std::make_shared<value::Value>(ret)  , func->type.params[ first_idx]};
        //         args_env[func->params[second_idx].ID] = {{func->params[second_idx].name}, std::make_shared<value::Value>(value), func->type.params[second_idx]};


        //         ScopeGuard sg{this, args_env};

        //         ret = checkReturnType(std::visit(*this, func->body->variant()).value, func->type.ret);
        //     }
        // }
        // else { // fuck me
        //     // checkNoSyntaxType(op->funcs);

        //     for (const auto& value : values) {

        //         func = resolveOverloadSet(op->OpName(), op->funcs, {ret, value});
        //         // I think these lines are needed. Have to check 
        //         func->type.ret                = validateType(std::move(func)->type.ret             );
        //         func->type.params[ first_idx] = validateType(std::move(func)->type.params[ first_idx]);
        //         func->type.params[second_idx] = validateType(std::move(func)->type.params[second_idx]);


        //         value::Environment args_env;
        //         args_env[func->params[ first_idx].ID] = {{func->params[ first_idx].name}, std::make_shared<value::Value>(ret)  , func->type.params[ first_idx]};
        //         args_env[func->params[second_idx].ID] = {{func->params[second_idx].name}, std::make_shared<value::Value>(value), func->type.params[second_idx]};


        //         ScopeGuard sg{this, args_env};

        //         ret = checkReturnType(std::visit(*this, func->body->variant()).value, func->type.ret);
        //     }
        // }

        // return {ret, typeOf(ret)};
        // return {ret, func->type.ret};

        return {ret, ret_type};
    }


    ValueType operator()(const expr::BinaryFold *fold) {
        if (const auto& var = getVar(fold->var_ID, liftName(fold)); var) return *var;


        value::Value pack = std::visit(*this, fold->pack->variant()).value;
        auto& packlist = get<value::Pack>(pack);


        if (packlist->values.empty()) return std::visit(*this, fold->init->variant());
    
        value::Value ret = std::visit(*this, fold->init->variant()).value;

        std::vector<value::Value> values;
        if (fold->left_to_right) {
            if (fold->sep) {
                const value::Value sep = std::visit(*this, fold->sep->variant()).value;

                for (auto& value : packlist->values) {
                    values.push_back(sep);
                    values.push_back(std::move(value));
                }
            }
            else for (auto& value : packlist->values) values.push_back(std::move(value));
        }
        else {
            if (fold->sep) {
                const value::Value sep = std::visit(*this, fold->sep->variant()).value;

                for (auto& value : packlist->values) {
                    values.push_back(std::move(value));
                    values.push_back(sep);
                }
            }
            else for (auto& value : packlist->values) values.push_back(std::move(value));

            std::ranges::reverse(values);
        }


        const auto& op = findOp(fold->op);

        // // can't have any syntax type since the pack consists of values, not expressions..
        // // unless...!
        // // TODO: allow for folding over syntax...maybe
        // checkNoSyntaxType(op->funcs);

        expr::Closure* func;

        const auto  first_idx = 1 - fold->left_to_right;
        const auto second_idx =     fold->left_to_right;

        // no overload resolution required
        if (op->funcs.size() == 1) {
            func = dynamic_cast<expr::Closure*>(op->funcs[0].get());
            func->type.ret                = validateType(std::move(func)->type.ret               );
            func->type.params[ first_idx] = validateType(std::move(func)->type.params[ first_idx]);
            func->type.params[second_idx] = validateType(std::move(func)->type.params[second_idx]);


            for (value::Environment args_env; const auto& value : values) {

                typeCheck(ret, func->type.params[first_idx],
                    "Type mis-match in Fold expressions with Infix operator '" + fold->op + 
                    "', parameter '" + func->params[0].name +
                    "' expected: " + func->type.params[0]->text() +
                    ", got: " + stringify(ret) + " which is " + typeOf(ret)->text()
                );


                typeCheck(ret, func->type.params[second_idx],
                        "Type mis-match in Fold expressions with Infix operator '" + fold->op + 
                        "', parameter '" + func->params[1].name +
                        "' expected: " + func->type.params[1]->text() +
                        ", got: " + stringify(ret) + " which is " + typeOf(ret)->text()
                );


                args_env[func->params[ first_idx].ID] = {{func->params[ first_idx].name}, std::make_shared<value::Value>(ret)  , func->type.params[ first_idx]};
                args_env[func->params[second_idx].ID] = {{func->params[second_idx].name}, std::make_shared<value::Value>(value), func->type.params[second_idx]};


                ScopeGuard sg{this, args_env};

                ret = checkReturnType(std::visit(*this, func->body->variant()).value, func->type.ret);
            }
        }
        else { // fuck me
            // checkNoSyntaxType(op->funcs);

            for (value::Environment args_env; const auto& value : values) {
                // auto type1 = validateType(typeOf(ret  ));
                // auto type2 = validateType(typeOf(value));

                func = resolveOverloadSet(op->OpName(), op->funcs, {ret, value});
                // * may be not needed....idk tho ;-;
                func->type.ret                = validateType(std::move(func)->type.ret               );
                func->type.params[ first_idx] = validateType(std::move(func)->type.params[ first_idx]);
                func->type.params[second_idx] = validateType(std::move(func)->type.params[second_idx]);


                args_env[func->params[ first_idx].ID] = {{func->params[ first_idx].name}, std::make_shared<value::Value>(ret)  , func->type.params[ first_idx]};
                args_env[func->params[second_idx].ID] = {{func->params[second_idx].name}, std::make_shared<value::Value>(value), func->type.params[second_idx]};


                ScopeGuard sg{this, args_env};

                ret = checkReturnType(std::visit(*this, func->body->variant()).value, func->type.ret);
            }
        }

        // return {ret, typeOf(ret)};
        return {ret, func->type.ret};
    }


    ValueType accessAssign(const expr::Assignment *ass, expr::Access *acc) {
        if (auto name = dynamic_cast<const expr::Name*>(acc->var.get()); name and name->name == "self") {
            if (selves.empty())
                util::error("Can't use 'self' outside of class scope: " + ass->stringify()); // shouldn't happen anyway

            if (not checkMemberInThisObject(acc->name))
                util::error("Name '" + acc->name + "' not found in object '" + acc->var->stringify() + "' in assignment: " + ass->stringify());

            const value::Value value = std::visit(*this, ass->rhs->variant()).value;
            return changeThis(acc->name, value);
        }

        const value::Value left = std::visit(*this, acc->var->variant()).value;

        if (not std::holds_alternative<value::Object>(left)) util::error("Can't access a non-class type!");

        const auto& obj = get<value::Object>(left);

        const auto& found = std::ranges::find_if(obj.second->members,
            [name = acc->name] (const auto& member) { return get<expr::Name>(member).stringify() == name; }
        );
        if (found == obj.second->members.end()) util::error("In assignment '" + ass->stringify() + "', Name '" + acc->name + "' doesn't exist in object: " + stringify(obj));

        value::Value value = std::visit(*this, ass->rhs->variant()).value;
        const type::TypePtr& type = get<type::TypePtr>(*found);

        value = typeCheck(value, type,
            "In assignment: " + ass->stringify() +
            "\nType mis-match! Expected: " + type->text() + ", got: " + typeOf(value)->text()
        );


        // get<value::Value>(*found) = value;
        *get<value::ValuePtr>(*found) = value;

        return {value, type};
    }



    ValueType spaceAccessAssign(const expr::Assignment *ass, expr::SpaceAccess *sa) {
        const auto space = findNS(sa->spaces, sa->global);

        auto [_, __, type] = space->members[sa->name.ID];

        value::Value value = std::visit(*this, ass->rhs->variant()).value;

        *get<value::ValuePtr>(space->members[sa->name.ID]) = typeCheck(value, type,
            "In assignment: " + ass->stringify() +
            "\nType mis-match! Expected: " + type->text() + ", got: " + typeOf(value)->text()
        );

        // was this a bug??
        // *get<value::ValuePtr>(space->members[sa->name.ID]) = std::move(value);
        return {*get<value::ValuePtr>(space->members[sa->name.ID]), type};
    }



    ValueType refAssign(const expr::Assignment *ass, const expr::Name* name) {

        for (const auto& e : std::views::reverse(env)) {
            if (e->env.contains(name->var_ID)) {
                const auto& [named_ref, value_ptr, type_ptr] = e->env.at(name->var_ID);
                const auto& [_, space] = named_ref;

                if (not space or not space->members.contains(name->var_ID)) 
                    util::error();


                // should never happen now that there is lexical analysis
                // if (not namespaces.contains(space)) util::error("Namespace `" + space + "` not found!");
                // if (not namespaces[space].contains(name->ID)) util::error("Name `" + name->name + "` with ID [" + std::to_string(name->ID) + "] not found in space " + space);

                auto [__, ___, type] = space->members[name->var_ID];

                value::Value value = std::visit(*this, ass->rhs->variant()).value;

                *get<value::ValuePtr>(space->members[name->var_ID]) = typeCheck(value, type,
                    "In assignment: " + ass->stringify() +
                    "\nType mis-match! Expected: " + type->text() + ", got: " + typeOf(value)->text()
                );

                // again, was this a bug??
                // *get<value::ValuePtr>(space->members[name->ID]) = std::move(value);

                return {*get<value::ValuePtr>(space->members[name->var_ID]), type};
            }
        }

        util::error();
    }


    ValueType nameAssign(const expr::Assignment *ass, const expr::Name* name) {

        // * walrus assignment may need to propogate the type here
        type::TypePtr type = ass->type;
        bool change{};

        // variable already exists. Check that type matches the rhs type
        if (const auto& var = getVar(name->var_ID, liftName(name)); var) {
            if (isRef(name->var_ID)) return refAssign(ass, name);

            if (type::shouldReassign(type)) {
                // no need to check if it's a valid type since that already was checked when it was creeated
                type = var->type;
                change = true;
            }
        }
        // if (checkMemberInThisObject(name->name)) {
        //     const value::Value val = std::visit(*this, ass->rhs->variant()).value;
        //     return changeThis(name->name, val);
        // }
        else { // New var
            type = type::shouldReassign(type) ? type::builtins::Any() : validateType(std::move(type));
        }


        // if (type->text() == "Syntax")
        //     return addVar(name->stringify(), name->ID, std::make_shared<value::Value>(ass->rhs->variant()), type);


        value::Value value = std::visit(*this, ass->rhs->variant()).value;


        value = typeCheck(value, type,
            "In assignment: " + ass->stringify() +
            "\nType mis-match! Expected: " + type->text() + ", got: " + typeOf(value)->text()
        );


        // casting the function type in case assigning a function to our variable
        if (std::holds_alternative<expr::Closure>(value)) {
            auto& closure = get<expr::Closure>(value);
            // we verified types are compatible so this is fine..should be...I hope
            if (const auto* t = dynamic_cast<type::FuncType*>(type.get()))
                closure.type = *t;
            else if (const auto* t = dynamic_cast<type::BuiltinType*>(type.get()); t and t->text() != "Any")
                    util::error();
            // else error("Again, Incompatible types. This should never happen. File a bug report!");
        }


        if (change) {
            if (not changeVar(name->var_ID, value)) util::error();
        }
        else addVar(name->stringify(), name->var_ID, std::make_shared<value::Value>(value), type);

        return {value, type};
    }


    ValueType operator()(const expr::Assignment *ass) {
        if (checkMemberInThisObject(ass->lhs->var_ID)) {
            const value::Value val = std::visit(*this, ass->rhs->variant()).value;
            return changeThis(ass->lhs->stringify(), val);
        }


        if (ass->is_syntax) {
            addVar(
                ass->lhs->stringify(),
                ass->lhs->var_ID,
                std::make_shared<value::Value>(ass->rhs->variant())
            );

            return {ass->rhs->variant(), type::builtins::Syntax()};
        }


        // assigning to x.y should never create a variable "x.y" bu access x and change y;
        if (auto *acc = dynamic_cast<expr::Access*>(ass->lhs.get())) return accessAssign(ass, acc);

        // ns::x = 1;
        if (auto *sa  = dynamic_cast<expr::SpaceAccess*>(ass->lhs.get())) return spaceAccessAssign(ass, sa);

        // name = 1;
        if (auto name = dynamic_cast<expr::Name*>(ass->lhs.get())) return nameAssign(ass, name);


        // // can't assign non-names to namespaces
        // // makes 1::2::3 impossible, which is consistent with the fact that 1 + 2::3 + 4; is ambiguous
        // // ... unless! (1 + 2)::3 is different than 1 + 2::3
        // if (dynamic_cast<expr::Namespace*>(ass->rhs.get()) and not dynamic_cast<expr::Name*>(ass->lhs.get()))
        //     util::error("Cannot assign namespaces to non-names: " + ass->stringify());


        // assign to the serialization (stringification) of the AST node
        const value::Value value = std::visit(*this, ass->rhs->variant()).value;
        addVar(
            ass->lhs->stringify(),
            ass->lhs->var_ID,
            std::make_shared<value::Value>(value)
        );

        return {value, typeOf(value)};
    }


    ValueType operator()(const expr::InferredAssignment *infr) {
        auto [value, type] = std::visit(*this, infr->rhs->variant());

        addVar(
            infr->name.name,
            infr->name.ID,
            std::make_shared<value::Value>(value),
            type
        );

        return {std::move(value), std::move(type)};
    }



    ValueType accessUnpackment(const auto& expr_str, expr::Access *acc, value::Value value) {

        if (auto name = dynamic_cast<const expr::Name*>(acc->var.get()); name and name->name == "self") {
            if (selves.empty())
                util::error("Can't use 'self' outside of class scope: " + expr_str()); // shouldn't happen anyway

            if (not checkMemberInThisObject(acc->name))
                util::error("Name '" + acc->name + "' not found in object '" + acc->var->stringify() + "' in assignment: " + expr_str());

            return changeThis(acc->name, value);
        }


        const value::Value left = std::visit(*this, acc->var->variant()).value;

        if (not std::holds_alternative<value::Object>(left)) util::error("Can't access a non-class type!");

        const auto& obj = get<value::Object>(left);

        const auto& found = std::ranges::find_if(obj.second->members,
            [name = acc->name] (const auto& member) { return get<expr::Name>(member).name == name; }
        );
        if (found == obj.second->members.end()) util::error("In unpackment '" + expr_str() + "', Name '" + acc->name + "' doesn't exist in object: " + stringify(obj));


        const type::TypePtr& type = get<type::TypePtr>(*found);

        value = typeCheck(value, type,
            "In unpackment: " + expr_str() +
            "\nType mis-match! Variable `" + acc->stringify() + "` expected: " + type->text() + ", got: " + typeOf(value)->text()
        );


        // get<value::Value>(*found) = value;
        *get<value::ValuePtr>(*found) = value;

        return {value, type};
    }



    ValueType spaceAccessUnpackment(const auto& expr_str, expr::SpaceAccess *sa, const value::Value& value) {
        const auto space = findNS(sa->spaces, sa->global);

        auto [_, __, type] = space->members[sa->name.ID];

        *get<value::ValuePtr>(space->members[sa->name.ID]) = typeCheck(value, type,
            "In unpackment: " + expr_str() +
            "\nType mis-match! Variable `" + sa->stringify() + "` expected: " + type->text() + ", got: " + typeOf(value)->text()
        );

        return {*get<value::ValuePtr>(space->members[sa->name.ID]), type};
    }



    ValueType refUnpackment(auto expr_str, const expr::Name* name, const value::Value& value) {
        for (const auto& e : std::views::reverse(env)) {
            if (e->env.contains(name->var_ID)) {
                const auto& [named_ref, value_ptr, type_ptr] = e->env.at(name->var_ID);
                const auto& [_, space] = named_ref;

                if (not space or not space->members.contains(name->var_ID)) 
                    util::error();


                // should never happen now that there is lexical analysis
                // if (not namespaces.contains(space)) util::error("Namespace `" + space + "` not found!");
                // if (not namespaces[space].contains(name->ID)) util::error("Name `" + name->name + "` with ID [" + std::to_string(name->ID) + "] not found in space " + space);

                auto [__, ___, type] = space->members[name->var_ID];


                *get<value::ValuePtr>(space->members[name->var_ID]) = typeCheck(value, type,
                    "In unpackment: " + expr_str() +
                    "\nType mis-match! Variable `" + name->name + "` expected: " + type->text() + ", got: " + typeOf(value)->text()
                );

                // again, was this a bug??
                // *get<value::ValuePtr>(space->members[name->ID]) = std::move(value);

                return {*get<value::ValuePtr>(space->members[name->var_ID]), type};
            }
        }

        util::error();
    }


    // newly introduced vars here will always be `Any`
    ValueType nameUnpackment(const auto& expr_str, const expr::Name* name, value::Value value) {
        type::TypePtr type = type::builtins::Any();
        bool change{};

        // variable already exists. Check that type matches the rhs type
        if (const auto& var = getVar(name->var_ID, liftName(name)); var) {
            if (isRef(name->var_ID)) return refUnpackment(expr_str, name, std::move(value));

            type = var->type;
            change = true;
        }


        value = typeCheck(value, type,
            "In unpackment: " + expr_str() +
            "\nType mis-match! Variable `" +  name->name + "` expected: " + type->text() + ", got: " + typeOf(value)->text()
        );


        // casting the function type in case assigning a function to our variable
        if (std::holds_alternative<expr::Closure>(value)) {
            auto& closure = get<expr::Closure>(value);
            // we verified types are compatible so this is fine..should be...I hope
            if (const auto* t = dynamic_cast<type::FuncType*>(type.get()))
                closure.type = *t;
            else if (const auto* t = dynamic_cast<type::BuiltinType*>(type.get()); t and t->text() != "Any")
                    util::error();
        }


        if (change) {
            if (not changeVar(name->var_ID, value)) util::error();
        }
        else addVar(name->stringify(), name->var_ID, std::make_shared<value::Value>(value), type);

        return {value, type};
    }




    // @pre-condition: values.reserve(n)
    void unpackIntoList(
        const auto& expr_str,
        std::vector<ValueType>& valuetypes,
        const value::Value& value,
        const size_t at_least,
        const bool has_pack = false
    ) {

        if (std::holds_alternative<value::Object>(value)) {
            const auto& object = get<value::Object>(value);

            if (object.second->members.size() < at_least)
                util::error("Unpacking more members than available: " + expr_str());

            for (const auto& [_, type, value_ptr] : object.second->members) {
                valuetypes.emplace_back(*value_ptr, type);
            }
        }
        else if (std::holds_alternative<value::List>(value)) {
            const auto& list = get<value::List>(value);

            if (list.elts->values.size() < at_least)
                util::error("Unpacking more elements than available: " + expr_str());

            for (const auto& value : list.elts->values) {
                valuetypes.emplace_back(value, typeOf(value));
            }
        }
        else if (std::holds_alternative<value::Map>(value)) {
            const auto& map = get<value::Map>(value);

            if (map.items->map.size() < at_least)
                util::error("Unpacking more elements than available: " + expr_str());

            for (const auto& [key, value] : map.items->map) {
                auto list = value::makeList({key, value});
                valuetypes.emplace_back(list, typeOf(list));
            }
        }
        else if (std::holds_alternative<value::Pack>(value)) {
            const auto& list = get<value::Pack>(value);

            if (not has_pack and list->values.size() != at_least) 
                util::error("Packs must be unpacked exactly: " + expr_str());


            for (const auto& value : list->values) {
                valuetypes.emplace_back(value, typeOf(value));
            }
        }
    }


    void unpackIntoMap(
        const auto& expr_str,
        std::vector<std::pair<ValueType, ValueType>>& valuetypes,
        const value::Value& value,
        const size_t at_least
    ) {

        if (std::holds_alternative<value::Object>(value)) {
            util::error("Map unpacking is not allowed on objects: " + expr_str());
            // const auto& object = get<value::Object>(value);

            // if (object.second->members.size() < at_least)
            //     util::error("Unpacking more members than available: " + unpack->stringify());

            // for (const auto& [name, type, value_ptr] : object.second->members) {
            //     valuetypes.emplace_back(ValueType{name.name, type::builtins::String()}, ValueType{*value_ptr, type});
            // }

            // // valuetypes.emplace_back(
            // //     ValueType{*get<2>(object.second->members[0]), get<1>(object.second->members[0])},
            // //     ValueType{*get<2>(object.second->members[1]), get<1>(object.second->members[1])}
            // // );
        }
        else if (std::holds_alternative<value::List>(value)) {
            const auto& list = get<value::List>(value);

            if (list.elts->values.size() < at_least)
                util::error("Unpacking more elements than available: " + expr_str());

            for (ssize_t i = -1; const auto& value : list.elts->values) {
                valuetypes.emplace_back(ValueType{++i, type::builtins::Int()}, ValueType{value, typeOf(value)});
            }
        }
        else if (std::holds_alternative<value::Map>(value)) {
            const auto& map = get<value::Map>(value);
            auto type = typeOf(map);
            auto map_type = dynamic_cast<type::MapType*>(type.get());

            if (map.items->map.size() < at_least)
                util::error("Unpacking more elements than available: " + expr_str());

            for (const auto& [key, val] : map.items->map) {
                valuetypes.emplace_back(ValueType{key, map_type->key_type}, ValueType{val, map_type->val_type});
            }
        }
    }


    template <bool INFERRED>
    void bindExpr(const auto& expr_str, const expr::ExprPtr bound, ValueType valuetype) {
        auto& [value, type] = valuetype;

        if constexpr (INFERRED) {
            addVar(
                bound->stringify(),
                bound->var_ID,
                std::make_shared<value::Value>(std::move(value)),
                std::move(type)
            );
        }
        else if (auto access = dynamic_cast<expr::Access*>(bound.get())) {
            accessUnpackment(expr_str, access, value);
        }
        else if (auto access = dynamic_cast<expr::SpaceAccess*>(bound.get())) {
            spaceAccessUnpackment(expr_str, access, value);
        }
        else if (auto name = dynamic_cast<expr::Name*>(bound.get())) {
            nameUnpackment(expr_str, name, value);
        }
        else addVar(
            bound->stringify(),
            bound->var_ID,
            std::make_shared<value::Value>(value)
        );
    }


    template <bool INFERRED>
    void bindPattern(
        const auto& expr_str,
        const expr::Unpackment::Pattern *pattern,
        ValueType valuetype
    ) {
        using Expr = expr::Unpackment::Expr;
        using List = expr::Unpackment::List;
        using Pack = expr::Unpackment::Pack;
        using Map  = expr::Unpackment::Map;

        // supposedly I don't need to check if the expression is a name
        // since LexicalAnalysis should've done it..i think :)
        if (auto expr_ptr = dynamic_cast<const Expr*>(pattern)) {
            bindExpr<INFERRED>(expr_str, expr_ptr->expr, valuetype);
        }
        else if (auto list = dynamic_cast<const List*>(pattern)) {
            const auto pack_index = [list] -> std::optional<size_t> {
                for (size_t i{}; const auto& pattern : list->patterns)
                    if (++i; dynamic_cast<expr::Unpackment::Pack*>(pattern.get())) return i - 1;

                return {};
            }();


            std::vector<ValueType> valuetypes;
            unpackIntoList(expr_str, valuetypes, valuetype.value, list->patterns.size() - pack_index.has_value(), pack_index.has_value());
            const size_t size = valuetypes.size(); // true size

            if (not pack_index) {
                for (const auto& [pattern, valuetype] : std::views::zip(list->patterns, valuetypes)) {
                    bindPattern<INFERRED>(expr_str,pattern.get(), valuetype);
                }
            }
            else {
                const size_t leading_count = *pack_index;
                const size_t trailing_count = list->patterns.size() - leading_count - 1; // minus 1 for the pack

                for (
                    const auto& [pattern, valuetype] :
                    std::views::zip(list->patterns, valuetypes) | std::views::take(leading_count)
                ) {
                    bindPattern<INFERRED>(expr_str,pattern.get(), valuetype);
                }

                const auto pack_pattern = dynamic_cast<Pack*>(list->patterns[*pack_index].get());

                auto pack = value::makePack(
                    valuetypes
                    | std::views::drop(leading_count)
                    | std::views::take(size - leading_count - trailing_count)
                    | std::views::transform([] (const auto& valuetype) { return valuetype.value; })
                    | std::ranges::to<std::vector<value::Value>>()
                );
                auto type = typeOf(pack);

                addVar(
                    pack_pattern->expr->stringify(),
                    pack_pattern->expr->var_ID,
                    std::make_shared<value::Value>(std::move(pack)),
                    std::move(type)
                );

                for (
                    size_t pat_size = list->patterns.size();

                    const auto& [pattern, valuetype] :
                    std::views::zip(
                        list->patterns | std::views::drop(pat_size - trailing_count),
                        valuetypes     | std::views::drop(size     - trailing_count)
                    )
                ) {
                    bindPattern<INFERRED>(expr_str,pattern.get(), valuetype);
                }
            }
        }
        else if (auto map = dynamic_cast<const Map*>(pattern)) {
            std::vector<std::pair<ValueType, ValueType>> valuetype_pairs;
            unpackIntoMap(expr_str, valuetype_pairs, valuetype.value, map->patterns.size());

            for (const auto& [pattern, pair] : std::views::zip(map->patterns, valuetype_pairs)) {
                bindPattern<INFERRED>(expr_str, pattern.first .get(), pair.first );
                bindPattern<INFERRED>(expr_str, pattern.second.get(), pair.second);
            }
        }
    }


    ValueType operator()(const expr::Unpackment *unpack) {
        auto rhs = std::visit(*this, unpack->rhs->variant());
        constexpr auto INFERRED = true;

        if (unpack->inferred) {
            bindPattern<    INFERRED>(liftName(unpack), unpack->pattern.get(), rhs);
        }
        else {
            bindPattern<not INFERRED>(liftName(unpack), unpack->pattern.get(), rhs);
        }


        return rhs;
    }


    ValueType createClass(
        std::vector<std::tuple<expr::Name, type::TypePtr, expr::ExprPtr>> fields,
        value::Env names = {}
    ) {

        value::Env captured;
        for (const auto& e : env) {
            for (const auto& [key, value] : e->env) captured.env[key] = value;
            for (const auto& [key, value] : e->op_env) captured.op_env[key] = value;
            for (const auto& [key, value] : e->prefix_op_env) captured.prefix_op_env[key] = value;
        }


        for (auto& [key, value] : names.env) captured.env[std::move(key)] = std::move(value);
        for (auto& [key, value] : names.op_env) captured.op_env[std::move(key)] = std::move(value);
        for (auto& [key, value] : names.prefix_op_env) captured.prefix_op_env[std::move(key)] = std::move(value);

        return {
                // getting lispy :sob: fuck this memory ass shit
                std::make_shared<type::LiteralType>(
                    std::make_shared<value::ClassValue>(
                        std::make_shared<value::Fields>(
                            std::move(fields)
                        ),
                        std::move(captured),
                        current_space
                    )
                ),
                type::builtins::Type()
            };
    }

    ValueType operator()(expr::Class *cls) {
        if (const auto& var = getVar(cls->var_ID, liftName(cls)); var) return *var;

        return createClass(std::move(cls)->fields);


        // std::vector<std::tuple<expr::Name, type::TypePtr, value::ValuePtr>> members;

        // ScopeGuard sg{this};
        // for (const auto& [name, typ, expr] : cls->fields) {

        //     type::TypePtr type = typ;

        //     value::Value v;
        //     if (type->text() == "Syntax") {
        //         // members.push_back({{field.first.stringify(), type::builtins::Syntax()}, field.second->variant()});
        //         type = type::builtins::Syntax();
        //         v = expr->variant();
        //     }
        //     else {
        //         type = validateType(std::move(type));

        //         v = std::visit(*this, expr->variant());

        //         typeCheck(v, type,
        //             "In class member assignment '" +
        //             name.stringify() + ": " + typ->text() + " = " + expr->stringify() +
        //             "Type mis-match! Expected: " + type->text() + ", got: " + typeOf(v)->text()
        //         );
        //     }

        //     // maybe not allowing the usage of previous members in the initializers of other members is the way? not sure
        //     // addVar(name.stringify(), v, type);
        //     members.push_back({name, type, std::make_shared<value::Value>(v)});
        // }

        // return // getting lispy :sob: fuck this memory ass shit
        //     std::make_shared<type::LiteralType>(
        //         std::make_shared<value::ClassValue>(
        //             std::make_shared<value::Members>(
        //                 std::move(members)
        //             )
        //         )
        //     );
    }


    ValueType operator()(const expr::Union *onion) {
        if (const auto& var = getVar(onion->var_ID, liftName(onion)); var) return *var;


        std::vector<type::TypePtr> types;
        for (auto& type : onion->types) {
            types.push_back(validateType(std::move(type)));
        }

        return {std::make_shared<type::UnionType>(std::move(types)), type::builtins::Type()};
    }


    ValueType objectAccess(const value::Object& obj, const std::string& name) {
        const auto& found = std::ranges::find_if(obj.second->members, [&name] (const auto& member) { return get<0>(member).stringify() == name; });
        if (found == obj.second->members.end()) util::error("Name '" + name + "' doesn't exist in object '" + /*acc->var->*/ stringify(obj) + '\'');


        const auto& type = get<type::TypePtr>(*found);
        auto& value = *get<value::ValuePtr>(*found);

        if (std::holds_alternative<expr::Closure>(value)) {
            auto& closure = get<expr::Closure>(value);
            closure.captureThis(obj);
        }

        return {value, type};
    }


    ValueType staticAccess(const type::LiteralType& cls, const std::string& name) {

        const auto& found = std::ranges::find_if(
            cls.cls->blueprint->fields,
            [&name] (const auto& member) { return get<0>(member).stringify() == name; }
        );

        if (found == cls.cls->blueprint->fields.end()) util::error("Name '" + name + "' doesn't exist in class `" + /*acc->var->*/ cls.text() + '`');

        const auto& type = get<type::TypePtr>(*found);

        ScopeGuard sg{this, cls.cls->env.env};
        sg.addPrefixOps(cls.cls->env.prefix_op_env);
        sg.addOps(cls.cls->env.op_env);
        auto value = typeCheck(std::visit(*this, get<expr::ExprPtr>(*found)->variant()).value, type);


        if (std::holds_alternative<expr::Closure>(value)) {
            auto& closure = get<expr::Closure>(value);
            closure.capture(cls.cls->env.env);
        }

        return {value, type};
    }


    ValueType operator()(const expr::Access *acc) {

        // in case user does self.xyz
        if (auto var = dynamic_cast<const expr::Name*>(acc->var.get()); var and var->name == "self") {
            if (selves.empty())
                util::error("Can't use 'self' outside of class scope: " + acc->stringify());

            const auto value = checkMemberInThisObject(acc->name);
            if (not value)
                util::error("Name '" + acc->name + "' not found in object '" + acc->var->stringify());

            return *value;
        }

        const value::Value left = std::visit(*this, acc->var->variant()).value;

        if (std::holds_alternative<type::TypePtr>(left)) {
            auto cls = dynamic_cast<type::LiteralType*>(get<type::TypePtr>(left).get());

            if (not cls) util::error("Cannot access a non-object value: " + acc->stringify());

            return staticAccess(*cls, acc->name);
        }

        if (not std::holds_alternative<value::Object>(left))
            util::error("Can't access a non-class type!");


        return objectAccess(get<value::Object>(left), acc->name);
    }



    NameSpace* matchChain(const std::vector<std::string>& names, NameSpace *space) {
        if (names.empty()) return nullptr;
        if (names[0] != space->name) return nullptr;


        for (const auto& name : names | std::views::drop(1)) {
            for (const auto& [child_name, child] : space->children) {
                // if the space is found
                // move the current down the chain to look for the nested name
                if (name == child_name) {
                    space = child.get();
                    goto keep_going;
                }
            }
            return nullptr;

            keep_going:
        }

        return space;
    }


    NameSpace* findNS(const std::vector<std::string>& names, const bool global_search_only) {
        if (not global_search_only) {
            for (const auto space : std::views::reverse(current_space)) {
                if (const auto s = matchChain(names, space)) return s;

                for (const auto& [_, ns] : space->children)
                    if (const auto s = matchChain(names, ns.get())) return s;
            }
        }


        for (const auto& [_, ns] : global_spaces)
            if (const auto s = matchChain(names, ns.get()))
                return s;



        util::error("Namespace `" + stringify(names) + "` not found!");

        // auto fixed_spaces = current_space;

        // // * append_range only available in gcc-15
        // // * GH Actions doesn't support gcc-15
        // #if 0
        // fixed_spaces.append_range(spaces);
        // #else
        // for (const auto& space : spaces) fixed_spaces.push_back(space);
        // #endif


        // std::string name = NSName(spaces);
        // while (not namespaces.contains(name)) {
        //     fixed_spaces.erase(fixed_spaces.end() - spaces.size(), fixed_spaces.end());

        //     if (fixed_spaces.empty()) util::error("couldn't find space: " + name);

        //     fixed_spaces.pop_back();

        //     // * append_range only available in gcc-15
        //     // * GH Actions doesn't support gcc-15
        //     #if 0
        //     fixed_spaces.append_range(spaces);
        //     #else
        //     for (const auto& space : spaces) fixed_spaces.push_back(space);
        //     #endif

        //     name = NSName(fixed_spaces);
        // }


        // return name;
    }





    void addSpace(const std::string& name) {
        NameSpace* ns;
        if (current_space.empty()) {
            if (global_spaces.contains(name)) {
                ns = global_spaces[name].get();
            }
            else {
                ns = (global_spaces[name] = std::make_shared<NameSpace>(name)).get();
            }
        }
        else if (current_space.back()->children.contains(name)) {
            ns = current_space.back()->children[name].get();
        }
        else {
            ns = (current_space.back()->children[name] = std::make_shared<NameSpace>(name)).get();
        }

        current_space.push_back(ns);

        scope();
    }


    void popSpace() {
        unscope();
        current_space.pop_back();
    }





    ValueType operator()(const expr::Namespace *ns) {
        if (const auto& var = getVar(ns->var_ID, liftName(ns)); var) return *var;


        ScopeGuard sg{this};
        addSpace(ns->name);
        sg.addEnv(current_space.back()->members);

        util::Deferred d{[this] { popSpace(); }};

        // const auto ns_name = NSName(current_space);
        // if (namespaces.contains(ns_name)) sg.addEnv(namespaces.at(ns_name));

        if (ns->space.empty()) util::error("Empty namespaces not allowed: " + ns->stringify());

        value::Value value;
        type::TypePtr type;
        // execute all the expressions in the namespace
        for (const auto& expr : ns->space) {
            const auto [v, t] = std::visit(*this, expr->variant());
            value = std::move(v);
            type  = std::move(t);
        }



        // for (auto& [id, val] : env.back().env) {
            //     auto& [name, value, type] = val;
            //     // I know this name is not a reference since each class member is responsible for its members
            //     current_space.back()->members[id] = {{std::move(name)}, std::move(value), std::move(type)};
            // }

        // then add the variables that resulted from that execution
        current_space.back()->members = std::move(env).back()->env;
        current_space.back()->prefix_op_env = std::move(env).back()->prefix_op_env;
        current_space.back()->op_env = std::move(env).back()->op_env;


        return {std::move(value), std::move(type)};
    }


    ValueType operator()(const expr::Use *use) {
        if (const auto& var = getVar(use->var_ID, liftName(use)); var) return *var;


        const auto space = findNS(use->spaces, use->global);

        // lexical scoping must've taken care of that!
        // if (not space->members.contains(use->name.ID)) util::error("Name `" + use->name.name + "` not found in space " + space->name);

        const auto& member = space->members.at(use->name.ID);

        const auto& value_ptr = get<value::ValuePtr>(member);
        const auto& type      = get< type::TypePtr>(member);

        addVar(use->name.name, use->name.ID, value_ptr, type::builtins::Any(), space);

        return {*value_ptr, type};
    }

    ValueType operator()(const expr::UseSpace *use) {
        if (const auto& var = getVar(use->var_ID, liftName(use)); var) return *var;

        const auto ns = findNS(use->spaces, use->global);

        // lexical scoping must've taken care of that
        // if (not namespaces.contains(space)) util::error("space '" + space + "' not found!");

        value::Value value;
        type::TypePtr type;
        for (const auto& [ID, t_v] : ns->members) {
            const auto& [name, value_ptr, type_ptr] = t_v;

            value = *value_ptr;
            type = type_ptr;

            // this any type should be `type_ptr`, but for some reason the language works fine without it  
            addVar(name.name, ID, value_ptr, type::builtins::Any(), ns);
        }


        // this gotta change and be bundled with ENV instead of being its own thing
        if (current_space.empty()) {
            for (const auto& [name, space] : ns->children) {
                global_spaces[name] = space;
            }
        }
        else for (const auto& [name, space] : ns->children) {
            current_space.back()->children[name] = space;
        }


        // `use space ns::;` always wil pull ALL OPS
        if (use->pull_ops) {
            for (const auto& [name, prefix_op] : ns->prefix_op_env) {
                env.back()->prefix_op_env[name] = prefix_op;
            }
            for (const auto& [name, op] : ns->op_env) {
                env.back()->op_env[name] = op;
            }
        }



        return {value, type};
    }


    ValueType operator()(const expr::UseFix *use) {
        const auto ns = findNS(use->spaces, use->global);

        std::optional<value::Value> value;
        type::TypePtr type;

        switch (use->filter) {
            using enum token::TokenKind;

            case PREFIX:
                for (const auto& [name, prefix_op] : ns->prefix_op_env)
                    if (
                        prefix_op->type() == PREFIX and
                        (use->op_name.empty() or name == use->op_name)
                    ) {
                        env.back()->prefix_op_env[name] = prefix_op;
                        const auto *func = dynamic_cast<expr::Closure*>(prefix_op->funcs[0].get());

                        value = *func;
                        type = std::make_shared<type::FuncType>(func->type);
                    }
                break;

            case INFIX:
                for (const auto& [name, op] : ns->op_env)
                    if (
                        op->type() == INFIX and
                        (use->op_name.empty() or name == use->op_name)
                    ) {
                        env.back()->op_env[name] = op;

                        const auto *func = dynamic_cast<expr::Closure*>(op->funcs[0].get());
                        value = *func;
                        type = std::make_shared<type::FuncType>(func->type);
                    }
                break;

            case SUFFIX:
                for (const auto& [name, op] : ns->op_env)
                    if (
                        op->type() == SUFFIX and
                        (use->op_name.empty() or name == use->op_name)
                    ) {
                        env.back()->op_env[name] = op;

                        const auto *func = dynamic_cast<expr::Closure*>(op->funcs[0].get());
                        value = *func;
                        type = std::make_shared<type::FuncType>(func->type);
                    }
                break;

            case EXFIX:
                for (const auto& [name, prefix_op] : ns->prefix_op_env)
                    if (
                        prefix_op->type() == EXFIX and
                        (use->op_name.empty() or name == use->op_name)
                    ) {
                        const auto exfix = dynamic_cast<const expr::Exfix*>(prefix_op.get());

                        env.back()->prefix_op_env[exfix->name ] = prefix_op;
                        env.back()->op_env       [exfix->name2] = prefix_op;


                        const auto *func = dynamic_cast<expr::Closure*>(prefix_op->funcs[0].get());
                        value = *func;
                        type = std::make_shared<type::FuncType>(func->type);
                    }
                break;

            case MIXFIX:
                for (const auto& [name, prefix_op] : ns->prefix_op_env) {
                    if (
                        prefix_op->type() == MIXFIX and
                        (use->op_name.empty() or name == use->op_name)
                    ) {
                        const auto mixfix = dynamic_cast<const expr::Operator*>(prefix_op.get());

                        env.back()->prefix_op_env[mixfix->name] = prefix_op;
                        for (const auto& sub_name : mixfix->rest) {
                            env.back()->op_env[sub_name] = prefix_op;
                        }


                        const auto *func = dynamic_cast<expr::Closure*>(prefix_op->funcs[0].get());
                        value = *func;
                        type = std::make_shared<type::FuncType>(func->type);
                    }
                }

                for (const auto& [name, op] : ns->op_env) {
                    if (
                        op->type() == MIXFIX and
                        (use->op_name.empty() or name == use->op_name)
                    ) {
                        const auto mixfix = dynamic_cast<const expr::Operator*>(op.get());

                        env.back()->prefix_op_env[mixfix->name] = op;
                        for (const auto& sub_name : mixfix->rest) {
                            env.back()->op_env[sub_name] = op;
                        }


                        const auto *func = dynamic_cast<expr::Closure*>(op->funcs[0].get());
                        value = *func;
                        type = std::make_shared<type::FuncType>(func->type);
                    }
                }
                break;

            case NONE:
                for (const auto& [name, prefix_op] : ns->prefix_op_env) {
                    if (use->op_name.empty() or name == use->op_name) {
                        env.back()->prefix_op_env[name] = prefix_op;

                        const auto *func = dynamic_cast<expr::Closure*>(prefix_op->funcs[0].get());
                        value = *func;
                        type = std::make_shared<type::FuncType>(func->type);
                    }
                }

                for (const auto& [name, op] : ns->op_env) {
                    if (use->op_name.empty() or name == use->op_name) {
                        env.back()->op_env[name] = op;

                        const auto *func = dynamic_cast<expr::Closure*>(op->funcs[0].get());
                        value = *func;
                        type = std::make_shared<type::FuncType>(func->type);
                    }
                }
                break;


            default: util::error();
        }


        if (not value) util::error("Fix operator `use` directive didn't pull any operators into scope: " + use->stringify());

        return {*std::move(value), std::move(type)};
    }



    void addNamespaces(
        std::unordered_map<std::string, std::shared_ptr<NameSpace>>& spaces,
        const std::unordered_map<std::string, std::shared_ptr<NameSpace>>& new_spaces
    ) {
        for (const auto& [new_space_name, new_space] : new_spaces) {
            if (
                auto iter = std::ranges::find_if(
                    spaces,
                    [&new_space_name] (const auto& space) { return space.first == new_space_name; }
                );
                iter != spaces.cend()
            ) {
                for (auto& [id, val] : new_space->members)
                    spaces[new_space_name]->members[id] = std::move(val);

                addNamespaces((*iter).second->children, new_space->children);
            }
            // in this case, just push the new space with all its children
            else spaces[new_space_name] = new_space;

            // else spaces.push_back(new_space);
        }
    }

    static ValueType zenOfPie() {
        const std::string_view zen =  
R"(
           .-.           
   .-..   ( __)   .--.   
  /    \  (''")  /    \  
 ' .-,  ;  | |  |  .-. ; 
 | |  . |  | |  |  | | | 
 | |  | |  | |  |  |/  | 
 | |  | |  | |  |  ' _.' 
 | |  ' |  | |  |  .'.-. 
 | `-'  '  | |  '  `-' / 
 | \__.'  (___)  `.__.'  
 | |                     
(___)                    


The Zen of Pie, by Ali Almutawa Jr.

Be unique, not unfamiliar.
Express yourself.
You decide what operates.
More control to you.
null is void.
Be whimzy, be quirky.
Break the rules.
Code is art.
You're an artist..
not a good one,
but a great one.
Steal. Every. Idea.
Don't forget to have fun.
There are no mistakes with art.)";

        std::println("{}", zen);

        static bool twice = false;
        if (twice) {
            return {314, type::builtins::Int()};
        }
        else {
            twice = true;
            return {"", type::builtins::String()};
        }
    }


    ValueType operator()(const expr::Import *import) {
        if (const auto& var = getVar(import->var_ID, liftName(import)); var) return *var;

        if (import->path == "self") return zenOfPie();


        const auto src = util::readFile(auto{import->path}.replace_extension(".pie").string());
        const token::Tokens tokens = lex::lex(src);
        if (tokens.empty()) util::error("Can't import an empty file!");

        Parser p{std::move(tokens), import->path};

        auto exprs = p.parse();



        analysis::LexicalAnalysis ls{import_indices[0]};
        import_indices.erase(import_indices.begin());

        for (auto& expr : exprs)
            std::visit(ls, expr->variant());



        value::Value value;
        Visitor v{std::move(ls).indeces};
        for (const auto& expr : exprs)
            value = std::visit(v, std::move(expr)->variant()).value;



        addNamespaces(current_space.empty() ? global_spaces : current_space.back()->children, v.global_spaces);

        return {value, typeOf(value)};
    }


    ValueType operator()(const expr::SpaceAccess *sa) {
        // x::z` Don't need to check if `z` is inside space `x`.
        // Lexical Analysis should already have taken care of it

        // Assigning to qualified names doesn't assign to the stringification of the expression,
        // So, need to check for this.
        // if (const auto& var = getVar(sa->var_ID); var) return *var;

        const auto space = findNS(sa->spaces, sa->global);
        const auto& member = space->members[sa->name.ID];
        return {*get<value::ValuePtr>(member), get<type::TypePtr>(member)};
    }



    bool match(const value::Value& value, const expr::Match::Case::Pattern& pattern) {
        if (std::holds_alternative<expr::Match::Case::Pattern::Single>(pattern.pattern)) {
            const auto& [name, typ, val_expr] = get<expr::Match::Case::Pattern::Single>(pattern.pattern);
            const auto type = validateType(typ);

            // not gonna use typeCheck for now. Let's see how it goes
            if (not (*type >= *typeOf(value))) return false;

            if (val_expr) {
                const value::Value val = std::visit(*this, val_expr->variant()).value;
                if (value != val) return false;
            }

            if (name.name.length() != 0) {
                addVar(name.name, name.ID, std::make_shared<value::Value>(value), type);
            }

            return true;
        }

        const auto& [type_name, patterns] = get<expr::Match::Case::Pattern::Structure>(pattern.pattern);

        std::optional<ValueType> var;
        if (dynamic_cast<expr::Name*>(type_name.get())) {
            var = getVar(type_name->var_ID, liftName(type_name.get()));
        }
        else {
            auto sa = dynamic_cast<expr::SpaceAccess*>(type_name.get());

            const auto space = findNS(sa->spaces, sa->global);
            const auto& member = space->members[sa->name.ID];
            var = {*get<value::ValuePtr>(member), get<type::TypePtr>(member)};
        }

        // shouldn't happen now that we have lexical analysis
        if (not var)
            util::error("Name `" + type_name->stringify() + "` in match expression does not name a constructor");


        if (not std::holds_alternative<type::TypePtr>(var->value) and not type::isClass(get<type::TypePtr>(var->value)))
            util::error("Name `" + type_name->stringify() + "` in match expression does not name a constructor");


        const auto& type = get<type::TypePtr>(var->value);
        if (not (*type == *typeOf(value))) return false;

        if (
            type::isClass(type) and
            patterns.size() > dynamic_cast<type::LiteralType*>(type.get())->cls->blueprint->fields.size()
        )
            util::error("Number of singles is greater than number of fields in class " + value::stringify(type));


        const auto& obj = get<value::Object>(value);
        if (obj.second->members.size() != obj.second->members.size()) util::error("idek what error message this should be..!");

        for (const auto& [member, pat] : std::views::zip(get<value::Object>(value).second->members, patterns)) {
            if (not match(*get<value::ValuePtr>(member), *pat)) return false;
        }

        return true;
    }


    ValueType operator()(const expr::Match *m) {
        if (const auto& var = getVar(m->var_ID, liftName(m)); var) return *var;

        const value::Value& value = std::visit(*this, m->expr->variant()).value;

        for (const auto& kase : m->cases) {
            ScopeGuard sg{this};
            if (match(value, *kase.pattern)) {
                bool guard = true;
                if (kase.guard) {
                    const value::Value& cond = std::visit(*this, kase.guard->variant()).value;

                    if (not std::holds_alternative<bool>(cond)) {
                        std::println(std::cerr, "In guard: {}", kase.guard->stringify());
                        util::error("Case guard must evaluate to a boolean. Got '" + typeOf(cond)->text() + "' instead!");
                    }

                    guard = get<bool>(cond);
                }

                if (guard) return std::visit(*this, kase.body->variant());
            }
        }


        util::error("Match expression didn't match any pattern:\n" + m->stringify() + "\nWith object:\n" + stringify(value));
    }


    ValueType operator()(const expr::Syntax *syn) {
        if (const auto& var = getVar(syn->var_ID, liftName(syn)); var) return *var;

        // return std::visit(*this, syn->expr->variant());
        return {syn->expr->variant(), type::builtins::Syntax()};
    }


    ValueType operator()(const expr::Type* type) {
        if (const auto& var = getVar(type->var_ID, liftName(type)); var) return *var;

        return {validateType(type->type), type::builtins::Type()};
    };


    void handleLoop(
        expr::Expr *body,
        expr::Unpackment::Pattern *var,
        expr::Expr *kind,
        expr::Expr *els,
        std::invocable<expr::Node> auto handle,
        const auto& expr_str
    ) {

        enum class Type { NONE = 0, INT, BOOL, LIST, STR, PACK, OBJECT };
        const auto classify = [](const value::Value& v) {
            if (std::holds_alternative<BigInt          >(v)) return Type::INT   ;
            if (std::holds_alternative<bool            >(v)) return Type::BOOL  ;
            if (std::holds_alternative<  std::string   >(v)) return Type::STR   ;
            if (std::holds_alternative<value::List     >(v)) return Type::LIST  ;
            if (std::holds_alternative<value::Pack     >(v)) return Type::PACK  ;
            if (std::holds_alternative<value::Object   >(v)) return Type::OBJECT; // iterators

            return Type::NONE;
        };

        ScopeGuard sg{this};

        // push
        const auto current_counter = loop_counter;

        if (constexpr auto CREATE_NEW_VAR = true; kind) {
            const auto& [kind_value, kind_type] = std::visit(*this, kind->variant());

            switch (classify(kind_value)) {
                // for loop
                case Type::INT: {
                    const auto limit = get<BigInt>(kind_value);
                    if (limit <= 0) {
                        if (not els) util::error("Loop which didn't run doesn't have else branch: " + expr_str());
                        handle(els->variant());
                        return;
                    }


                    if (var) {

                        for (loop_counter = 0; loop_counter < limit; ++loop_counter) {
                            continued = false;

                            bindPattern<CREATE_NEW_VAR>(expr_str, var, {loop_counter, type::builtins::Int()});

                            handle(body->variant());

                            if (broken) break;
                            // if (continued) continue;
                        }
                    }
                    else for (loop_counter = 0; loop_counter < limit; ++loop_counter) {
                        continued = false;

                        addVar(
                            "_",
                            std::to_underlying(analysis::LexicalAnalysis::ReservedIDs::UNNAMED),
                            std::make_shared<value::Value>(loop_counter),
                            type::builtins::Int()
                        );
                        handle(body->variant());

                        if (broken) break;
                    }

                } break;

                // while loop
                case Type::BOOL: {
                    if (not get<bool>(kind_value)) {
                        if (not els) util::error("Loop which didn't run doesn't have else branch: " + expr_str());
                        handle(els->variant());
                        return;
                    }

                    if (auto cond = kind_value; var) {

                        for (loop_counter = 0; get<bool>(cond); ++loop_counter) {
                            continued = false;

                            bindPattern<CREATE_NEW_VAR>(expr_str, var, {loop_counter, type::builtins::Int()});

                            handle(body->variant());


                            if (broken) break;
                            // if (continued) continue;

                            cond = std::visit(*this, kind->variant()).value;
                        }

                    }
                    else for (loop_counter = 0; get<bool>(cond); ++loop_counter) {
                        continued = false;

                        addVar(
                            "_",
                            std::to_underlying(analysis::LexicalAnalysis::ReservedIDs::UNNAMED),
                            std::make_shared<value::Value>(loop_counter),
                            type::builtins::Int()
                        );
                        handle(body->variant());

                        if (broken) break;

                        cond = std::visit(*this, kind->variant()).value;
                    }
                } break;

                case Type::LIST: {
                    const auto& list = get<value::List>(kind_value);
                    if (list.elts->values.empty()) {
                        if (not els) util::error("Loop which didn't run doesn't have else branch: " + expr_str());
                        handle(els->variant());
                        return;
                    }


                    auto list_type = type::isList(kind_type); // this is effectively a cast
                    // a place for the type to live in so that it doesn't "die" before `list_type`
                    type::TypePtr memory;
                    if (not list_type) {
                        memory = typeOf(kind_value);
                        list_type = type::isList(memory);
                    }

                    if (var) {
                        for (const auto& elt : list.elts->values) {
                            continued = false;

                            bindPattern<CREATE_NEW_VAR>(expr_str, var, {elt, list_type->type});

                            handle(body->variant());

                            if (broken) break;
                        }

                    }
                    else for ([[maybe_unused]] const auto& elt : list.elts->values) {
                        continued = false;

                        addVar(
                            "_",
                            std::to_underlying(analysis::LexicalAnalysis::ReservedIDs::UNNAMED),
                            std::make_shared<value::Value>(elt),
                            list_type->type
                        );
                        handle(body->variant());

                        if (broken) break;
                    }
                } break;

                case Type::STR: {
                    const auto& str = get<std::string>(kind_value);
                    if (str.empty()) {
                        if (not els) util::error("Loop which didn't run doesn't have else branch: " + expr_str());
                        handle(els->variant());
                        return;
                    }


                    if (var) {
                        for (const auto& elt : str) {
                            continued = false;

                            bindPattern<CREATE_NEW_VAR>(expr_str, var, {std::string{elt}, type::builtins::String()});

                            handle(body->variant());

                            if (broken) break;
                        }

                    }
                    else for ([[maybe_unused]] const auto& elt : str) {
                        continued = false;

                        addVar(
                            "_",
                            std::to_underlying(analysis::LexicalAnalysis::ReservedIDs::UNNAMED),
                            std::make_shared<value::Value>(elt),
                            type::builtins::String()
                        );
                        handle(body->variant());

                        if (broken) break;
                    }
                } break;

                case Type::PACK: {
                    const auto& pack = get<value::Pack>(kind_value);
                    if (pack->values.empty()) {
                        if (not els) util::error("Loop which didn't run doesn't have else branch: " + expr_str());
                        handle(els->variant());
                        return;
                    }


                    auto pack_type = type::isVariadic(kind_type);
                    // a place for the type to live in so that it doesn't "die" before `list_type`
                    type::TypePtr memory;
                    if (not pack_type) {
                        memory = typeOf(kind_value);
                        pack_type = type::isVariadic(memory);
                    }

                    if (var) {

                        for (const auto& elt : pack->values) {
                            continued = false;

                            bindPattern<CREATE_NEW_VAR>(expr_str, var, {elt, pack_type->type});

                            handle(body->variant());

                            if (broken) break;
                        }

                    }
                    else for ([[maybe_unused]] const auto& elt : pack->values) {
                        continued = false;

                        addVar(
                            "_",
                            std::to_underlying(analysis::LexicalAnalysis::ReservedIDs::UNNAMED),
                            std::make_shared<value::Value>(elt),
                            pack_type->type
                        );
                        handle(body->variant());

                        if (broken) break;
                    }
                } break;

                case Type::OBJECT: {
                    const auto& obj = get<value::Object>(kind_value);

                    // const auto& next_it    = std::ranges::find_if(obj.second->members, [](const auto& p) { return p.first.name == "next";    });
                    // const auto& hasNext_it = std::ranges::find_if(obj.second->members, [](const auto& p) { return p.first.name == "hasNext"; });
                    // if (next_it == obj.second->members.cend() or hasNext_it == obj.second->members.cend())
                    //     error("Object in loop: " + loop->stringify() + "\ndoesn't follow the iterator protocol!");


                    // // I know objectAccess errors if the accessee is not found, but more specific err messages are nicer
                    const value::Value hasNext = objectAccess(obj, "hasNext").value;
                    const value::Value    next = objectAccess(obj,    "next").value;

                    if (not std::holds_alternative<expr::Closure>(hasNext) or not std::holds_alternative<expr::Closure>(next))
                        util::error("Object in loop: " + expr_str() + " doesn't follow the iterator protocol!");

                    const auto& hasNext_func = get<expr::Closure>(hasNext);
                    const auto&    next_func = get<expr::Closure>(next   );

                    expr::Call hasNext_call{std::make_shared<expr::Closure>(hasNext_func)};
                    expr::Call    next_call{std::make_shared<expr::Closure>(   next_func)};

                    if (var) {


                        value::Value cond = std::visit(*this, hasNext_call.variant()).value;
                        if (not std::holds_alternative<bool>(cond))
                            util::error("Method `hasNext` did not produce a boolean value: " + hasNext_call.stringify() + "\nwhich is required in order to follow the iterator protocol!");

                        while(get<bool>(cond)) {
                            continued = false;

                            bindPattern<CREATE_NEW_VAR>(expr_str, var, std::visit(*this, next_call.variant()));

                            handle(body->variant());

                            if (broken) break;

                            cond = std::visit(*this, hasNext_call.variant()).value;
                            if (not std::holds_alternative<bool>(cond))
                                util::error("Method `hasNext` did not produce a boolean value: " + hasNext_call.stringify() + "\nwhich is required in order to follow the iterator protocol!");
                        }
                    }
                    else {
                        value::Value cond = std::visit(*this, hasNext_call.variant()).value;
                        if (not std::holds_alternative<bool>(cond))
                            util::error("Method `hasNext` did not produce a boolean value: " + hasNext_call.stringify() + "\nwhich is required in order to follow the iterator protocol!");

                        while(get<bool>(cond)) {
                            continued = false;

                            auto [elt, type] = std::visit(*this, next_call.variant());

                            addVar(
                                "_",
                                std::to_underlying(analysis::LexicalAnalysis::ReservedIDs::UNNAMED),
                                std::make_shared<value::Value>(std::move(elt)),
                                std::move(type)
                            );

                            handle(body->variant());

                            if (broken) break;

                            cond = std::visit(*this, hasNext_call.variant()).value;
                            if (not std::holds_alternative<bool>(cond))
                                util::error("Method `hasNext` did not produce a boolean value: " + hasNext_call.stringify() + "\nwhich is required in order to follow the iterator protocol!");
                        }
                    }


                } break;

                case Type::NONE:
                    util::error("Loop type no supported: " + expr_str());
            }
        }
        // loop till break
        else if (var) {
            continued = false;

            for (loop_counter = 0; ; ++loop_counter) {
                bindPattern<CREATE_NEW_VAR>(expr_str, var, {loop_counter, type::builtins::Int()});

                handle(body->variant());

                if (broken) break;
            }

        }
        else for (loop_counter = 0; ; ++loop_counter) {
            continued = false;

            handle(body->variant());

            if (broken) break;
        }

        // pop
        loop_counter = current_counter;

        broken = continued = false;
    }



    ValueType operator()(const expr::Loop *loop) {
        if (const auto& var = getVar(loop->var_ID, liftName(loop)); var) return *var;

        value::Value value;
        type::TypePtr type;

        handleLoop(
            loop->body.get(),
            loop->var.get(),
            loop->kind.get(),
            loop->els.get(),
            [this, &value, &type] (expr::Node node) {
                auto valuetype = std::visit(*this, std::move(node));
                value = std::move(valuetype).value;
                type  = std::move(valuetype).type ;
            },
            liftName(loop)
        );

        return {std::move(value), std::move(type)};
    }


    ValueType operator()(const expr::ListComp *comp) {
        if (const auto& var = getVar(comp->var_ID, liftName(comp)); var) return *var;


        auto list = value::makeList();


        if (comp->guard)
            handleLoop(
                comp->body.get(),
                comp->var.get(),
                comp->kind.get(),
                nullptr,
                [this, comp, &list, &guard = comp->guard] (expr::Node node) {
                    auto g = std::visit(*this, guard->variant());
                    if (not std::holds_alternative<bool>(g.value))
                        util::error("Comprehension guard didn't yield a boolean: " + comp->stringify());

                    if (get<bool>(g.value))
                        list.elts->values.push_back(std::visit(*this, std::move(node)).value);
                },
                liftName(comp)
            );
        else
            handleLoop(
                comp->body.get(),
                comp->var.get(),
                comp->kind.get(),
                nullptr,
                [this, &list] (expr::Node node) {
                    list.elts->values.push_back(std::visit(*this, std::move(node)).value);
                },
                liftName(comp)
            );


        auto type = typeOf(list);
        return {std::move(list), std::move(type)};
    }


    ValueType operator()(const expr::MapComp *comp) {
        if (const auto& var = getVar(comp->var_ID, liftName(comp)); var) return *var;


        auto map = value::makeMap();

        expr::ExprPtr pair = std::make_shared<expr::List>(
            std::vector<expr::ExprPtr>{comp->body1, comp->body2}
        );


        if (comp->guard)
            handleLoop(
                pair.get(),
                comp->var.get(),
                comp->kind.get(),
                nullptr,
                [this, comp, &map, &guard = comp->guard] (expr::Node node) {
                    auto g = std::visit(*this, guard->variant());
                    if (not std::holds_alternative<bool>(g.value))
                        util::error("Comprehension guard didn't yield a boolean: " + comp->stringify());


                    if (get<bool>(g.value)) {
                        auto valuetype = std::visit(*this, std::move(node));

                        auto& list = get<value::List>(valuetype.value);
                        map.items->map.insert_or_assign(std::move(list.elts->values[0]), std::move(list.elts->values[1]));
                    }
                },
                liftName(comp)
            );
        else
            handleLoop(
                pair.get(),
                comp->var.get(),
                comp->kind.get(),
                nullptr,
                [this, &map] (expr::Node node) {
                    auto valuetype = std::visit(*this, std::move(node));

                    auto& list = get<value::List>(valuetype.value);
                    map.items->map.insert_or_assign(std::move(list.elts->values[0]), std::move(list.elts->values[1]));
                },
                liftName(comp)
            );


        auto type = typeOf(map);
        return {std::move(map), std::move(type)};
    }


    ValueType operator()(const expr::Break *brake) {
        broken = true;
        if (brake->expr) return std::visit(*this, brake->expr->variant());
        // if (brake->expr) return eval(brake->expr);

        util::error();
        // return loop_counter;
    }

    ValueType operator()(const expr::Continue *cont) {
        continued = true;

        if (cont->expr) return std::visit(*this, cont->expr->variant());

        return {loop_counter, type::builtins::Int()};
    }


    //* only added to differentiate between expressions such as: 1 + 2 and (1 + 2)
    ValueType operator()(const expr::Grouping *g) {
        if (const auto& var = getVar(g->var_ID, liftName(g)); var) return *var;

        return std::visit(*this, g->expr->variant());
    }


    static void checkNoSyntaxType(const std::vector<expr::ExprPtr>& funcs) {
        for (const auto& func : funcs) {
            const auto& closure = dynamic_cast<const expr::Closure*>(func.get());
            for (
                const auto& param :closure->params
            ) {
                if (param.is_syntax) util::error("Cannot have a 'Syntax' paramater in an overload set!");
            }
        }
        return;
    }

    expr::Closure* resolveOverloadSet(
        const std::string& name,
        const std::vector<expr::ExprPtr>& funcs,
        // const std::vector< type::TypePtr >& types,
        const std::vector<value::Value>& values
    ) {

        if (funcs.size() == 1) return dynamic_cast<expr::Closure*>(funcs[0].get());

        std::vector<expr::Closure*> set;

        std::vector<type::TypePtr> types;
        types.reserve(values.size());
        for (const auto& v : values) types.push_back(validateType(typeOf(v)));


        for (const auto& func : funcs) {
            auto closure = dynamic_cast<expr::Closure*>(func.get());

            bool found = true;
            for (const auto& [arg_value, arg_type, param_type] : std::views::zip(values, types, closure->type.params)) {
                if (not param_type->typeCheck(this, arg_value, arg_type)) {
                    found = false;
                    break;
                }
            }

            if (found) set.push_back(closure);
        }


        if (set.size() > 1) {
            // remove the functions that take Any's to favour the concrete functions rather than "templates"
            std::erase_if(set, [] (const expr::Closure *c) { return std::ranges::all_of(c->type.params, &type::isAny); });
        }


        if (set.empty())
            util::error("No overload was found in overload set of operator: " + name);
        else if (set.size() != 1)
            util::error("Could not resolve overload set for operator: " + name);

        return set[0];
    }



    const value::Value& checkReturnType(const value::Value& ret, const type::TypePtr return_type, const std::source_location& location = std::source_location::current()) {

        typeCheck(ret, return_type,
            "Type mis-match! Function return type expected: " +
            return_type->text() + ", got: " + typeOf(ret)->text(),
            location
        );

        // if (not (*return_type >= *type_of_return_value))
        //     error<except::TypeMismatch>(
        //         "Type mis-match! Function return type expected: " +
        //         return_type->text() + ", got: " + type_of_return_value->text(),
        //         location
        //     );


        return ret;
    }

    ValueType operator()(const expr::UnaryOp *up) {
        if (const auto& var = getVar(up->var_ID, liftName(up)); var) return *var;


        const auto& op = findPrefixOp(up->op);
        expr::Closure* func;
        value::Environment args_env;

        if (op->funcs.size() == 1) {
            func = dynamic_cast<expr::Closure*>(op->funcs[0].get());

            if (func->params[0].is_syntax) {
                args_env[func->params[0].ID] = {
                    {func->params[0].name},
                    std::make_shared<value::Value>(up->expr->variant()),
                    type::builtins::Syntax()
                };
            }
            else {
                func->type.params[0] = validateType(std::move(func)->type.params[0]);
                const auto arg = std::visit(*this, up->expr->variant()).value;

                typeCheck(arg, func->type.params[0],
                    "Type mis-match! Prefix operator '" + up->op + 
                    "' expected: " + func->type.params[0]->text() +
                    ", got: " + stringify(arg) + " which is " + typeOf(arg)->text()
                );

                args_env[func->params[0].ID] = {{func->params[0].name}, std::make_shared<value::Value>(arg), func->type.params[0]}; //? fixed
            }

            //* maybe should use Syntax() instead of Any() for all operators?
        }
        else { // do selection based on type
            checkNoSyntaxType(op->funcs);

            const value::Value arg = std::visit(*this, up->expr->variant()).value;

            func = resolveOverloadSet(op->OpName(), op->funcs, {arg});

            args_env[func->params[0].ID] = {{func->params[0].name}, std::make_shared<value::Value>(arg), func->type.params[0]}; //? fixed
        }


        if (func->self) selves.push_back(*func->self);
        util::Deferred d1{[this, cond = static_cast<bool>(func->self)] { if (cond) selves.pop_back(); }};

        auto old_spaces = std::move(current_space);
        current_space = std::move(func->spaces);
        util::Deferred d2{[this, &old_spaces] { current_space = std::move(old_spaces); }};


        ScopeGuard sg{this, args_env};

        value::Value ret;
        if (not dynamic_cast<expr::Block*>(func->body.get())) {
            ret = std::visit(*this, func->body->variant()).value;

            if (std::holds_alternative<expr::Closure>(ret))
                captureEnvForReturnedClosure(get<expr::Closure>(ret));
        }
        else ret = std::visit(*this, func->body->variant()).value; // capturing logic will be done by the scope's visitor

        if (func->self and std::holds_alternative<expr::Closure>(ret)) {
            auto& f = get<expr::Closure>(ret);
            f.captureThis(*func->self);
        }

        checkReturnType(ret, func->type.ret);
        return {ret, func->type.ret};
    }



    ValueType operator()(const expr::BinOp *bp) {
        if (const auto& var = getVar(bp->var_ID, liftName(bp)); var) return *var;


        const auto& op = findOp(bp->op);
        expr::Closure* func;
        value::Environment args_env;


        // LHS
        if (op->funcs.size() == 1) {
            func = dynamic_cast<expr::Closure*>(op->funcs[0].get());

            if (func->params[0].is_syntax) {
                args_env[func->params[0].ID] = {
                    {func->params[0].name},
                    std::make_shared<value::Value>(bp->lhs->variant()),
                    type::builtins::Syntax()
                };
            }
            else {
                func->type.params[0] = validateType(std::move(func)->type.params[0]);

                const value::Value arg1 = std::visit(*this, bp->lhs->variant()).value;

                typeCheck(arg1, func->type.params[0],
                    "Type mis-match! Infix operator '" + bp->op + 
                    "', parameter '" + func->params[0].name +
                    "' expected: " + func->type.params[0]->text() +
                    ", got: " + stringify(arg1) + " which is " + typeOf(arg1)->text()
                );

                args_env[func->params[0].ID] = {{func->params[0].name}, std::make_shared<value::Value>(arg1), func->type.params[0]};
            }


            // RHS
            if (func->params[1].is_syntax) {
                args_env[func->params[1].ID] = {
                    {func->params[1].name},
                    std::make_shared<value::Value>(bp->lhs->variant()),
                    type::builtins::Syntax()
                };
            }
            else {
                func->type.params[1] = validateType(std::move(func)->type.params[1]);

                const value::Value arg2 = std::visit(*this, bp->rhs->variant()).value;

                typeCheck(arg2, func->type.params[1],
                    "Type mis-match! Infix operator '" + bp->op + 
                    "', parameter '" + func->params[1].name +
                    "' expected: " + func->type.params[1]->text() +
                    ", got: " + stringify(arg2) + " which is " + typeOf(arg2)->text()
                );

                args_env[func->params[1].ID] = {{func->params[1].name}, std::make_shared<value::Value>(arg2), func->type.params[1]};
            }
        }
        else {
            checkNoSyntaxType(op->funcs);

            const value::Value arg1  = std::visit(*this, bp->lhs->variant()).value;
            const value::Value arg2  = std::visit(*this, bp->rhs->variant()).value;

            func = resolveOverloadSet(op->OpName(), op->funcs, {arg1, arg2});

            args_env[func->params[0].ID] = {{func->params[0].name}, std::make_shared<value::Value>(arg1), func->type.params[0]};
            args_env[func->params[1].ID] = {{func->params[1].name}, std::make_shared<value::Value>(arg2), func->type.params[1]};
        }


        // !for binary fold
        // value::Value ret;
        // if (not dynamic_cast<expr::Block*>(func->body.get())) {
        //     ret = std::visit(*this, func->body->variant());

        //     if (std::holds_alternative<expr::Closure>(ret)) {
        //         captureEnvForReturnedClosure(get<expr::Closure>(ret));
        //     }
        // }
        // else ret = std::visit(*this, func->body->variant());

        // if (func->self and std::holds_alternative<expr::Closure>(ret)) {
        //     const auto& f = get<expr::Closure>(ret);
        //     f.captureThis(*func->self);
        // }


        if (func->self) selves.push_back(*func->self);
        util::Deferred d1{[this, cond = static_cast<bool>(func->self)] { if (cond) selves.pop_back(); }};

        auto old_spaces = std::move(current_space);
        current_space = std::move(func->spaces);
        util::Deferred d2{[this, &old_spaces] { current_space = std::move(old_spaces); }};


        ScopeGuard sg{this, args_env};

        value::Value ret;
        if (not dynamic_cast<expr::Block*>(func->body.get())) {
            ret = std::visit(*this, func->body->variant()).value;

            if (std::holds_alternative<expr::Closure>(ret))
                captureEnvForReturnedClosure(get<expr::Closure>(ret));
        }
        else ret = std::visit(*this, func->body->variant()).value;

        if (func->self and std::holds_alternative<expr::Closure>(ret)) {
            auto& f = get<expr::Closure>(ret);
            f.captureThis(*func->self);
        }

        checkReturnType(ret, func->type.ret);
        return {ret, func->type.ret};
        // return checkReturnType(std::visit(*this, func->body->variant()), func->type.ret);
    }


    ValueType operator()(const expr::PostOp *pp) {
        if (const auto& var = getVar(pp->var_ID, liftName(pp)); var) return *var;


        const auto& op = findOp(pp->op);
        expr::Closure* func;
        value::Environment args_env;

        if (op->funcs.size() == 1) {
            func = dynamic_cast<expr::Closure*>(op->funcs[0].get());

            if (func->params[0].is_syntax) {
                args_env[func->params[0].ID] = {
                    {func->params[0].name},
                    std::make_shared<value::Value>(pp->expr->variant()),
                    type::builtins::Syntax()
                };
            }
            else {
                func->type.params[0] = validateType(std::move(func)->type.params[0]);

                const value::Value arg = std::visit(*this, pp->expr->variant()).value;

                typeCheck(arg, func->type.params[0],
                    "Type mis-match! Suffix operator '" + pp->op + 
                    "', parameter '" + func->params[0].name +
                    "' expected: " + func->type.params[0]->text() +
                    ", got: " + stringify(arg) + " which is " + typeOf(arg)->text()
                );

                args_env[func->params[0].ID] = {{func->params[0].name}, std::make_shared<value::Value>(arg), func->type.params[0]}; //? fixed
            }
        }
        else {
            checkNoSyntaxType(op->funcs);

            const value::Value arg  = std::visit(*this, pp->expr->variant()).value;

            func = resolveOverloadSet(op->OpName(), op->funcs, {arg});

            args_env[func->params[0].ID] = {{func->params[0].name}, std::make_shared<value::Value>(arg), func->type.params[0]};
        }


        if (func->self) selves.push_back(*func->self);
        util::Deferred d1{[this, cond = static_cast<bool>(func->self)] { if (cond) selves.pop_back(); }};

        auto old_spaces = std::move(current_space);
        current_space = std::move(func->spaces);
        util::Deferred d2{[this, &old_spaces] { current_space = std::move(old_spaces); }};


        ScopeGuard sg{this, args_env};

        value::Value ret;
        if (not dynamic_cast<expr::Block*>(func->body.get())) {
            ret = std::visit(*this, func->body->variant()).value;

            if (std::holds_alternative<expr::Closure>(ret))
                captureEnvForReturnedClosure(get<expr::Closure>(ret));
        }
        else ret = std::visit(*this, func->body->variant()).value;

        if (func->self and std::holds_alternative<expr::Closure>(ret)) {
            auto& f = get<expr::Closure>(ret);
            f.captureThis(*func->self);
        }

        checkReturnType(ret, func->type.ret);
        return {ret, func->type.ret};
        // return checkReturnType(std::visit(*this, func->body->variant()), func->type.ret);
    }



    ValueType operator()(const expr::CircumOp *cp) {
        if (const auto& var = getVar(cp->var_ID, liftName(cp)); var) return *var;

        const auto& op = findPrefixOp(cp->op1);
        expr::Closure* func;
        value::Environment args_env;

        if (op->funcs.size() == 1) {
            func = dynamic_cast<expr::Closure*>(op->funcs[0].get());

            if (func->params[0].is_syntax) {
                args_env[func->params[0].ID] = {
                    {func->params[0].name},
                    std::make_shared<value::Value>(cp->expr->variant()),
                    type::builtins::Syntax()
                };
            }
            else {
                func->type.params[0] = validateType(std::move(func)->type.params[0]);

                const value::Value arg = std::visit(*this, cp->expr->variant()).value;

                typeCheck(arg, func->type.params[0],
                    "Type mis-match! Exfix operator '" + cp->op1 + 
                    "', parameter '" + func->params[0].name +
                    "' expected: " + func->type.params[0]->text() +
                    ", got: " + stringify(arg) + " which is " + typeOf(arg)->text()
                );

                args_env[func->params[0].ID] = {{func->params[0].name}, std::make_shared<value::Value>(arg), func->type.params[0]}; //? fixed
            }
        }
        else {
            checkNoSyntaxType(op->funcs);

            const value::Value arg  = std::visit(*this, cp->expr->variant()).value;

            func = resolveOverloadSet(op->OpName(), op->funcs, {arg});

            args_env[func->params[0].ID] = {{func->params[0].name}, std::make_shared<value::Value>(arg), func->type.params[0]};
        }


        if (func->self) selves.push_back(*func->self);
        util::Deferred d1{[this, cond = static_cast<bool>(func->self)] { if (cond) selves.pop_back(); }};

        auto old_spaces = std::move(current_space);
        current_space = std::move(func->spaces);
        util::Deferred d2{[this, &old_spaces] { current_space = std::move(old_spaces); }};


        ScopeGuard sg{this, args_env};

        value::Value ret;
        if (not dynamic_cast<expr::Block*>(func->body.get())) {
            ret = std::visit(*this, func->body->variant()).value;

            if (std::holds_alternative<expr::Closure>(ret))
                captureEnvForReturnedClosure(get<expr::Closure>(ret));
        }
        else ret = std::visit(*this, func->body->variant()).value;

        if (func->self and std::holds_alternative<expr::Closure>(ret)) {
            auto& f = get<expr::Closure>(ret);
            f.captureThis(*func->self);
        }

        checkReturnType(ret, func->type.ret);
        return {ret, func->type.ret};
        // return checkReturnType(std::visit(*this, func->body->variant()), func->type.ret);
    };

    ValueType operator()(const expr::OpCall *oc) {
        if (const auto& var = getVar(oc->var_ID, liftName(oc)); var) return *var;



        auto& op = [this, oc] -> auto& { // const std::unique_ptr<expr::Fix>&
            if (opsContain(oc->first)) return findOp(oc->first);
            return findPrefixOp(oc->first);
        }();

        expr::Closure* func;
        value::Environment args_env;

        if (op->funcs.size() == 1) {

            func = dynamic_cast<expr::Closure*>(op->funcs[0].get());

            // this may be not needed
            // if (oc->exprs.size() != func->params.size()) error();

            for (
                const auto& [arg_expr, param, param_type]
                :
                std::views::zip(oc->exprs, func->params, func->type.params)
            ) {

                if (param.is_syntax) {
                    args_env[param.ID] = {
                        {param.name},
                        std::make_shared<value::Value>(arg_expr->variant()),
                        type::builtins::Syntax()
                    };
                }
                else {
                    param_type = validateType(std::move(param_type));

                    const value::Value arg = std::visit(*this, arg_expr->variant()).value;

                    const auto op_name = op->OpName();
                    typeCheck(arg, param_type,
                        "Type mis-match! Parameter '" +
                        op_name + "' expected type: " + param_type->text() + ", got: " + typeOf(arg)->text()
                    );


                    args_env[param.ID] = {{param.name}, std::make_shared<value::Value>(arg), param_type};
                }
            }
        }
        else {
            checkNoSyntaxType(op->funcs);

            std::vector<value::Value> args;
            // std::vector<type::TypePtr> types;
            for (const auto& expr : oc->exprs) {
                args.push_back(std::visit(*this, expr->variant()).value);
                // types.push_back(validateType(typeOf(args.back())));
                // // types.back() = validateType(std::move(types).back());
            }

            func = resolveOverloadSet(op->OpName(), op->funcs, args);

            for (const auto& [param, arg, type] : std::views::zip(func->params, args, func->type.params))
                args_env[param.ID] = {{param.name}, std::make_shared<value::Value>(arg), type};
        }


        if (func->self) selves.push_back(*func->self);
        util::Deferred d1{[this, cond = static_cast<bool>(func->self)] { if (cond) selves.pop_back(); }};

        auto old_spaces = std::move(current_space);
        current_space = std::move(func->spaces);
        util::Deferred d2{[this, &old_spaces] { current_space = std::move(old_spaces); }};


        ScopeGuard sg{this, args_env};

        value::Value ret;
        if (not dynamic_cast<expr::Block*>(func->body.get())) {
            ret = std::visit(*this, func->body->variant()).value;

            if (std::holds_alternative<expr::Closure>(ret))
                captureEnvForReturnedClosure(get<expr::Closure>(ret));
        }
        else ret = std::visit(*this, func->body->variant()).value;

        if (func->self and std::holds_alternative<expr::Closure>(ret)) {
            auto& f = get<expr::Closure>(ret);
            f.captureThis(*func->self);
        }

        checkReturnType(ret, func->type.ret);
        return {ret, func->type.ret};
        // return checkReturnType(std::visit(*this, func->body->variant()), func->type.ret);
    }

    std::optional<value::Value> objectIsCallable(const value::Object& obj) {
        for (const auto& [name, type, value] : obj.second->members) {
            if (name.name == "call" and type::isFunction(typeOf(*value))) {
                get<expr::Closure>(*value).captureThis(obj);
                return *value;
            }
        }

        return {};
    };

    ValueType operator()(const expr::Call *call) {
        if (const auto& var = getVar(call->var_ID, liftName(call)); var) return *var;

        // const auto args = std::move(call)->args;
        const auto args = call->args;


        // for pack expansions at call site (there could be multiple of em)
        // i.e: func(args1..., "Hi", "hello", args2..., args3...).
        // we MUST expand before doing anything else
        // so that we can know if this is gonna be a
        // full function call or a curried call
        // if we can get the length of the pack before expanding it
        // the problem will be solved, and we'll even be able to allow
        // packs of syntax type: ...Syntax
        // for now, this is a hard problem for me
        // ===== ^^^ old ==== vvv new
        // so now that I added `Syntax` literals..
        // this problem is mostly solved?
        // but I still don't know if I wanna add implicit syntax or not
        // ===== ^^^ new old ==== vvv new new
        // I added implicit syntax
        // not sure how that affects this...:)
        // I am not schizophrenic
        std::vector<std::pair<size_t, std::vector<value::Value>>> expand_at;
        for (size_t i{}; i < args.size(); ++i) {
            if (const auto expand = dynamic_cast<const expr::Expansion*>(args[i].get())) {
                const value::Value pack = std::visit(*this, expand->pack->variant()).value;

                if (not std::holds_alternative<value::Pack>(pack))
                    util::error("Expansion applied on a non-pack variable: " + args[i]->stringify());

                expand_at.push_back({i, get<value::Pack>(pack)->values});
            }
        }


        auto [var, type] = std::visit(*this, call->func->variant());
        if (std::holds_alternative<value::BuiltinFunction>(var)) { // that dumb lol. but now it works

            // if (isBuiltin(name))
            auto value = evaluateBuiltin(
                call, 
                std::move(args),
                std::move(expand_at),
                std::move(call)->named_args,
                std::move(get<value::BuiltinFunction>(var))
            );
            auto type = typeOf(value);

            return {std::move(value), std::move(type)};
        }

        if (std::holds_alternative<type::TypePtr>(var)) return {constructorCall(call, var), get<type::TypePtr>(var)};


        if (std::holds_alternative<expr::Closure>(var)) {
            return closureCall(call, get<expr::Closure>(std::move(var)), std::move(args), std::move(expand_at));
        }


        if (std::holds_alternative<value::Object>(var)) {
            const auto& obj = get<value::Object>(var);
            if (const auto callable = objectIsCallable(obj); callable) {
                return closureCall(call, get<expr::Closure>(*std::move(callable)), std::move(args), std::move(expand_at));
            }
        }


        if (std::holds_alternative<expr::Node>(var)) {
            if (not args.empty()) util::error("Cannot pass arguments to syntax objects: " + call->stringify());

            return std::visit(*this, get<expr::Node>(var));
        }


        if (args.empty()) return {var, type};

        // if (args.size() == 1) {       // set
        //     const auto val = std::visit(*this, args[0]->variant());

        //     #if 0
        //         addVar(stringify(var), val, typeOf(val));
        //     #else
        //         addVar(call->func->stringify(), call->func->ID, val, typeOf(val));
        //     #endif

        //     return var;
        // }

        util::error("Can't pass arguments to values!");
    }



    struct ScopeGuard; // forward declaring so the below function knows about it

    void variadicCall(
        const expr::Closure& func,
        std::vector<std::pair<expr::Closure::Param, type::TypePtr>>& pos_params,
        const std::vector<std::pair<size_t, std::vector<value::Value>>>& expand_at,
        const std::vector<pie::expr::ExprPtr>& args,
        const size_t args_size,
        ScopeGuard& sg,
        value::Environment& args_env
    ) {
        const auto it = std::ranges::find_if(pos_params, [] (const auto& e) { return type::isVariadic(e.second); });
        const size_t variadic_index = std::distance(pos_params.begin(), it);

        const auto pre_variadic = std::ranges::subrange(pos_params.begin(), it);
        const auto post_variadic  = std::ranges::subrange(it + 1, pos_params.end());

        const size_t pre_variadic_size = std::ranges::size(pre_variadic);
        const size_t post_variadic_size = std::ranges::size(post_variadic);
        const size_t variadic_size = args_size - pre_variadic_size - post_variadic_size;



        const auto findType = [&func] (const size_t p, const type::TypePtr& type) {
            // it doesn't matter if there are multiple arguments with this name
            // `validateType` will choose the lastly-bounded one
            // we just need to proof that A parameter exists in order to call `validateType`
            for (size_t i{}; i <= p; ++i)
                if (type->involvesT(type::ExprType{std::make_shared<expr::Name>(func.params[i].name)}))
                    return true;

            // // look in the arguments env (from a partially evaluated function that yielded this function)
            // for (const auto& [key, _] : func.args_env)
            for (const auto& [_, obj] : func.envs.env.env) {
                const auto& [name, __, ___] = obj;
                if (type->involvesT(type::ExprType{std::make_shared<expr::Name>(name.name)})) {
                    return true;
                }
            }

            return false;
        };


        auto pack = value::makePack();
        for (
            size_t arg_index{}, param_index{}, pack_index{}, curr_expansion{};
            arg_index < args.size(); // can't be args_size since arg_index is only used to index into args
        ) {
            auto [sid, type] = pos_params[param_index];
            const auto& [name, id, is_syntax] = sid;
            type = type->clone();

            value::Value value;

            if (param_index == variadic_index){
                if (findType(param_index, type)) {
                    // ScopeGuard sg{this, func.args_env, args_env};
                    ScopeGuard sg{this, func.envs.env.env, args_env};
                    type = validateType(std::move(type));
                }


                for (size_t i{}; i < variadic_size; ++i) {
                    if (curr_expansion < expand_at.size() and arg_index == expand_at[curr_expansion].first) {
                        value = std::move(expand_at[curr_expansion].second[pack_index++]);

                        value = typeCheck(value, type,
                            "Type mis-match! Parameter '" + name + "' expected type: " + type->text() + ", got: " + typeOf(value)->text()
                        );

                        if (std::holds_alternative<expr::Closure>(value))
                            captureEnvForPassedClosure(get<expr::Closure>(value));


                        if (pack_index >= expand_at[curr_expansion].second.size()) {
                            ++arg_index;
                            ++curr_expansion;
                            pack_index = 0;
                        }
                    }
                    else {
                        const auto& expr = args[arg_index];

                        // if (type->text() == "Syntax") util::error(); //* allow this the future

                        value = std::visit(*this, expr->variant()).value;

                        value = typeCheck(value, type,
                            "Type mis-match! Parameter '" + name + "' expected type: " + type->text() + ", got: " + typeOf(value)->text()
                        );

                        if (std::holds_alternative<expr::Closure>(value))
                            captureEnvForPassedClosure(get<expr::Closure>(value));

                        ++arg_index;
                    }

                    pack->values.push_back(std::move(value));
                }

                ++param_index;

                // sg.addEnv({{name, {std::make_shared<value::Value>(value), type}}});
                args_env[id] = {{name}, std::make_shared<value::Value>(std::move(pack)), std::move(type)};
            }
            else {
                if (findType(param_index, type)) {
                    // ScopeGuard sg{this, func.args_env, args_env};
                    ScopeGuard sg{this, func.envs.env.env, args_env};
                    type = validateType(std::move(type));
                }


                if (curr_expansion < expand_at.size() and arg_index == expand_at[curr_expansion].first) {
                    value = expand_at[curr_expansion].second[pack_index++];

                    value = typeCheck(value, type,
                        "Type mis-match! Parameter '" + name + "' expected type: " + type->text() + ", got: " + typeOf(value)->text()
                    );

                    if (std::holds_alternative<expr::Closure>(value))
                        captureEnvForPassedClosure(get<expr::Closure>(value));


                    if (pack_index >= expand_at[curr_expansion].second.size()) {
                        ++arg_index;
                        ++curr_expansion;
                        pack_index = 0;
                    }
                }
                else {
                    const auto& expr = args[arg_index];

                    value = std::visit(*this, expr->variant()).value;

                    value = typeCheck(value, type,
                        "Type mis-match! Parameter '" + name + "' expected type: " + type->text() + ", got: " + typeOf(value)->text()
                    );

                    if (std::holds_alternative<expr::Closure>(value))
                        captureEnvForPassedClosure(get<expr::Closure>(value));

                    ++arg_index;
                }

                ++param_index;
                // sg.addEnv({{name, {std::make_shared<value::Value>(value), type}}});
                args_env[id] = {{name}, std::make_shared<value::Value>(std::move(value)), std::move(type)};
            }
        }


        if (variadic_size == 0) {
            sg.addEnv({{
                pos_params[variadic_index].first.ID,
                {
                    {pos_params[variadic_index].first.name},
                    std::make_shared<value::Value>(value::makePack()),
                    pos_params[variadic_index].second
                }
            }});

            args_env[pos_params[variadic_index].first.ID] = {
                {pos_params[variadic_index].first.name},
                std::make_shared<value::Value>(value::makePack()),
                std::move(pos_params)[variadic_index].second
            };
        }
    }


    // sets up the arguments
    void regularCall(
        const expr::Closure& func,
        std::vector<std::pair<expr::Closure::Param, type::TypePtr>>& pos_params,
        std::vector<std::pair<size_t, std::vector<value::Value>>>& expand_at,
        const std::vector<pie::expr::ExprPtr>& args,
        const size_t args_size,
        // ScopeGuard& sg,
        value::Environment& args_env
    ) {

        const auto findType = [&func] (const size_t p, const type::TypePtr& type) {
            // it doesn't matter if there are multiple arguments with this name
            // `validateType` will choose the lastly-bounded one
            // we just need to proof that A parameter exists in order to call `validateType`
            for (size_t i{}; i <= p; ++i)
                if (type->involvesT(type::ExprType{std::make_shared<expr::Name>(func.params[i].name)}))
                    return true;

            // // look in the arguments env (from a partially evaluated function that yielded this function)
            // for (const auto& [key, _] : func.args_env)
            for (const auto& [_, obj] : func.envs.env.env) {
                const auto& [name, __, ___] = obj;
                if (type->involvesT(type::ExprType{std::make_shared<expr::Name>(name.name)})) {
                    return true;
                }
            }

            return false;
        };

        for (size_t i{}, p{}, curr{}; p < args_size; ++p, ++i) {

            if (curr < expand_at.size() and i == expand_at[curr].first) {
                for (auto& val : expand_at[curr++].second) {
                    auto& [sid, type] = pos_params[p];
                    const auto& [name, id, is_syntax] = sid;
                    if (findType(p, type)) {
                        // ScopeGuard sg{this, func.args_env, args_env};
                        ScopeGuard sg{this, func.envs.env.env, args_env};
                        type = validateType(std::move(type));
                    }

                    ++p; // important!

                    val = typeCheck(val, type,
                        "Type mis-match! Parameter '" + name + "' expected type: " + type->text() + ", got: " + typeOf(val)->text()
                    );

                    if (std::holds_alternative<expr::Closure>(val))
                        captureEnvForPassedClosure(get<expr::Closure>(val));


                    // sg.addEnv({{name, {std::make_shared<value::Value>(val), type}}});
                    args_env[id] = {{name}, std::make_shared<value::Value>(std::move(val)), std::move(type)};
                }
                --p; // the parameter index will have gone one too far. bring it back
            }
            else {
                auto& [sid, type] = pos_params[p];
                if (findType(p, type)) {
                    // ScopeGuard sg{this, func.args_env, args_env};
                    ScopeGuard sg{this, func.envs.env.env, args_env};
                    type = validateType(std::move(type));
                }

                const auto& expr = args[i];

                const auto& [name, id, is_syntax] = sid;

                value::Value value;
                if (is_syntax) {
                    value = expr->variant();
                }
                else {
                    value = std::visit(*this, expr->variant()).value;

                    value = typeCheck(value, type,
                        "Type mis-match! Parameter '" + name + "' expected type: " + type->text() + ", got: " + typeOf(value)->text()
                    );

                    if (std::holds_alternative<expr::Closure>(value))
                        captureEnvForPassedClosure(get<expr::Closure>(value));
                }


                // sg.addEnv({{name, {std::make_shared<value::Value>(value), type}}});
                args_env[id] = {{name}, std::make_shared<value::Value>(std::move(value)), std::move(type)};
            }
        }
    }


    static size_t argsSize(
        const std::vector<pie::expr::ExprPtr>& args,
        std::vector<std::pair<size_t, std::vector<value::Value>>> expand_at
    ) {
        return args.size()
        + std::ranges::fold_left( // plus the expansions
            expand_at, 
            size_t{}, 
            [] (const auto& acc, const auto& elt) { return acc + elt.second.size(); }
        ) 
        - expand_at.size(); // minus redundant packs (already expanded)
    }


    ValueType closureCall(
        const expr::Call *call,
        expr::Closure func,
        const std::vector<pie::expr::ExprPtr>& args,
        std::vector<std::pair<size_t, std::vector<value::Value>>> expand_at
    ) {

        // // types are validate in operator()(const expr::Closure* c) for now
        // for (auto& type : func.type.params) type = validateType(std::move(type));
        // func.type.ret = validateType(std::move(func.type.ret));
        // // func.type.ret = validateType(std::move(func).type.ret); // is this better?

        if (func.self) selves.push_back(*func.self);
        util::Deferred d1{[this, cond = static_cast<bool>(func.self)] { if (cond) selves.pop_back(); }};

        auto old_spaces = std::move(current_space);
        current_space = std::move(func.spaces);
        util::Deferred d2{[this, &old_spaces] { current_space = std::move(old_spaces); }};


        // check for invalid named arguments
        for (const auto& [name, _] : call->named_args) {
            if (std::ranges::find(func.params, name, &expr::Closure::Param::name) == func.params.end())
                util::error("Named argument '" + name + "' does not name a parameter name!");
        }


        const bool is_variadic = std::ranges::any_of(func.type.params, [] (const auto& e) { return type::isVariadic(e); });
        const size_t args_size = argsSize(args, expand_at);

        if (not is_variadic and args_size + call->named_args.size() > func.params.size()) util::error("Too many arguments passed to function: " + call->stringify());

        // curry! 
        if (args_size + call->named_args.size() < func.params.size() - is_variadic) {
            return partialApplication(call, func, args_size, std::move(expand_at), args, is_variadic);
        }


        //* full call. Don't curry!
        ScopeGuard sg{this, value::EnvTag::FUNC, func.envs.env.env};
        value::Environment args_env; // in case the lambda needs to capture 


        // !
        for (const auto& [name, expr] : call->named_args) {
            type::TypePtr type;
            ssize_t id;

            for (const auto& [n, t] : std::views::zip(func.params, func.type.params)) {
                if (n.name == name) {
                    type = t;
                    id = n.ID;
                    break;
                }
            }

            if (not type) util::error(); // should never happen anyway

            value::Value value = std::visit(*this, expr->variant()).value;

            value = typeCheck(value, type,
                "Type mis-match! Parameter '" + name + "' expected type: " + type->text() + ", got: " + typeOf(value)->text()
            );

            // if (std::holds_alternative<expr::Closure>(value))
            //     captureEnvForPassedClosure(get<expr::Closure>(value));

            args_env[id] = {{name}, std::make_shared<value::Value>(std::move(value)), std::move(type)};
        }


        std::vector<std::pair<expr::Closure::Param, type::TypePtr>> pos_params;
        for (const auto& [param, type] : std::views::zip(func.params, func.type.params))
            pos_params.push_back({param, type});

        std::erase_if(pos_params, [named_args = call->named_args] (const auto& p)  mutable {
            const auto cond = std::ranges::find_if(named_args, [&p] (const auto& n) { return n.first == p.first.name; }) != named_args.cend();
            if (cond) named_args.erase(p.first.name);
            return cond;
        });


        // todo: this is needed but it doesn't work well rn
        // if (args.size() != pos_params.size())
        //     util::error(
        //         "Expected " + std::to_string(pos_params.size()) +
        //         " postional arguments. Got " + std::to_string(args.size()) +
        //         ": " + call->stringify()
        //     );


        if (is_variadic) {
            variadicCall(func, pos_params, expand_at, args, args_size, sg, args_env);
        }
        else {
            regularCall(func, pos_params, expand_at, args, args_size,    args_env);
        }



        if (
            std::ranges::find_if(
                func.params, [&type = func.type.ret](const auto& param) {
                    return type->involvesT(type::ExprType{std::make_shared<expr::Name>(param.name)});
                }
            ) != func.params.cend()
        ) {
            // ScopeGuard sg{this, func.args_env, args_env};
            ScopeGuard sg{this, func.envs.env.env, args_env};
            func.type.ret = validateType(std::move(func.type.ret));
        }


        // //* should I capture the env and bundle it with the function before returning it?
        // if (type::isSyntax(func.type.ret)) return {func.body->variant(), type::builtins::Syntax()};


        // sg.addEnv(func.args_env);
        sg.addEnv(func.envs.returned_env.env);
        sg.addEnv(args_env);
        sg.addEnv(func.envs.env.env);
        sg.addEnv(func.envs.passed_env.env);
        sg.addOps(func.envs.env.op_env);
        sg.addPrefixOps(func.envs.env.prefix_op_env);

        value::Value ret;
        if (not dynamic_cast<const expr::Block*>(func.body.get())) {
            ret = std::visit(*this, func.body->variant()).value;

            if (std::holds_alternative<expr::Closure>(ret))
                // captureEnvForPassedClosure(get<expr::Closure>(ret));
                captureEnvForReturnedClosure(get<expr::Closure>(ret));
        }
        else ret = std::visit(*this, func.body->variant()).value;

        if (func.self and std::holds_alternative<expr::Closure>(ret)) {
            get<expr::Closure>(ret).captureThis(*func.self);
        }


        checkReturnType(ret, func.type.ret);

        return {ret, func.type.ret};
    }



    // since all variables are always alive, it there is no need to capture variables...for now at least
    void captureEnvForReturnedClosure(expr::Closure& c) {
        size_t found{};

        for (size_t i{}; i < env.size(); ++i)
            if (env[i]->tag == value::EnvTag::FUNC) found = i;

        for (; found < env.size(); ++found) {
            c.returnCapture(env[found]->env);
        }
    }


    void captureEnvForPassedClosure(expr::Closure& c) {
        // starting at one so we don't capture globals
        // of course, this is just a hack and not a fix
        // the proper fix would capture a reference to the variable instead...
        size_t found1 = 1;
        size_t found2 = 1;

        for (size_t i = 1; i < env.size(); ++i)
            if (env[i]->tag == value::EnvTag::FUNC) {
                found1 = found2;
                found2 = i;
            }


        for (; found1 < found2; ++found1) {
            c.passedCapture(env[found1]->env);
        }
    }


    ValueType partialApplication(
        const expr::Call *call,
        const expr::Closure& func,
        const size_t args_size,
        std::vector<std::pair<size_t, std::vector<value::Value>>> expand_at,
        std::vector<expr::ExprPtr> args, 
        const bool is_variadic
    ) {
        // ScopeGuard sg{this, EnvTag::FUNC, func.args_env, func.env};
        ScopeGuard sg{this, value::EnvTag::FUNC, func.envs.env.env};
        value::Environment args_env = func.envs.env.env;

        for (const auto& [name, expr] : call->named_args) {
            type::TypePtr type;
            ssize_t id;
            for (const auto& [n, t] : std::views::zip(func.params, func.type.params)) {
                if (n.name == name) {
                    type = t;
                    id = n.ID;
                    break;
                }
            }

            if (not type) util::error(); // should never happen anyway

            value::Value value = std::visit(*this, expr->variant()).value;

            value = typeCheck(value, type,
                "Type mis-match! Parameter '" + name + "' expected type: " + type->text() + ", got: " + typeOf(value)->text()
            );

            // if (std::holds_alternative<expr::Closure>(value))
            //     captureEnvForPassedClosure(get<expr::Closure>(value));

            // sg.addEnv({{name, {std::make_shared<value::Value>(value), type}}});
            args_env[id] = {{name}, std::make_shared<value::Value>(std::move(value)), std::move(type)};
        }


        std::vector<std::pair<expr::Closure::Param, type::TypePtr>> pos_params;
        for (const auto& [param, type] : std::views::zip(func.params, func.type.params))
            pos_params.push_back({param, type});

        std::erase_if(pos_params, [&named_args = call->named_args] (const auto& p) {
            return std::ranges::find_if(named_args, [&p] (const auto& n) { return n.first == p.first.name; }) != named_args.cend();
        });

        std::vector<expr::Closure::Param> new_params;
        for (const auto& [name, _] : pos_params | std::views::drop(args_size))
            new_params.push_back(name);

        std::vector<type::TypePtr> new_types;
        for (const auto& name : new_params) {
            type::TypePtr type;
            for (const auto& [n, t] : std::views::zip(func.params, func.type.params)) {
                if (n.name == name.name) {
                    type = t;
                    break;
                }
            }
            new_types.push_back(std::move(type));
        }

        type::FuncType func_type{std::move(new_types), func.type.ret};
        expr::Closure closure{std::move(new_params), func.body, std::move(func_type)};


        bool normal = true;
        if (is_variadic) {
            // const auto iter = std::ranges::find_if(pos_params, type::isVariadic, &std::pair::second);
            const auto iter = std::ranges::find_if(pos_params, [] (const auto& e) { return type::isVariadic(e.second); });
            const size_t variadic_index = std::distance(pos_params.begin(), iter);

            // call->args.erase(std::next(call->args.begin(), variadic_index));

            if (args_size > variadic_index) {
                normal = false;

                // FIX: first add the empty pack
                sg.addEnv({{
                    pos_params[variadic_index].first.ID, 
                    {
                        {pos_params[variadic_index].first.name},
                        std::make_shared<value::Value>(value::makePack()),
                        pos_params[variadic_index].second
                    }
                }});

                args_env[pos_params[variadic_index].first.ID] = {
                    {pos_params[variadic_index].first.name},
                    std::make_shared<value::Value>(value::makePack()),
                    std::move(pos_params[variadic_index]).second
                };

                // only then should you remove the parameter
                // previously, I only had the following. So the pack was left as undefined instead of empty
                pos_params.erase(std::next(pos_params.begin(), variadic_index));

                // we ignore the pack, so we consume an extra argument. Remove it from the carried function
                closure.params.erase(closure.params.begin());
                closure.type.params.erase(closure.type.params.begin());

                // for(size_t i{}, curr{}; const auto& [param, expr] : std::views::zip(pos_params, args)) {
                for (size_t i{}, p{}, curr{}; p < args_size; ++p, ++i) {

                    if (curr < expand_at.size() and i == expand_at[curr].first) {
                        for (auto& val : expand_at[curr++].second) {
                            auto& [sid, type] = pos_params[p];
                            const auto& [name, id, is_syntax] = sid;
                            // if (findType(p, type)) type = validateType(std::move(type));
                            ++p;

                            val = typeCheck(val, type,
                                "Type mis-match! Parameter '" + name + "' expected type: " + type->text() + ", got: " + typeOf(val)->text()
                            );

                            if (std::holds_alternative<expr::Closure>(val))
                                captureEnvForPassedClosure(get<expr::Closure>(val));

                            // sg.addEnv({{name, {std::make_shared<value::Value>(val), type}}});
                            args_env[id] = {{name}, std::make_shared<value::Value>(std::move(val)), std::move(type)};
                        }
                        --p;
                    }
                    else {
                        auto& [sid, type] = pos_params[p];
                        const auto& [name, id, is_syntax] = sid;
                        const auto& expr = args[i];
                        // if (findType(p, type)) type = validateType(std::move(type));

                        value::Value value;
                        value = std::visit(*this, expr->variant()).value;

                        value = typeCheck(value, type,
                            "Type mis-match! Parameter '" + name + "' expected type: " + type->text() + ", got: " + typeOf(value)->text()
                        );

                        // if (std::holds_alternative<expr::Closure>(value))
                        //     captureEnvForPassedClosure(get<expr::Closure>(value));
                        // sg.addEnv({{name, {std::make_shared<value::Value>(value), type}}});
                        args_env[id] = {{name}, std::make_shared<value::Value>(std::move(value)), std::move(type)};
                    }
                }
            }
        }

        if (normal) {
            const auto findType = [&func] (const size_t p, const type::TypePtr& type) {
                // it doesn't matter if there are multiple arguments with this name
                // `validateType` will choose the lastly-bounded one
                // we just need to proof that A parameter exists in order to call `validateType`
                for (size_t i{}; i <= p; ++i)
                    if (type->involvesT(type::ExprType{std::make_shared<expr::Name>(func.params[i].name)}))
                        return true;

                // // look in the arguments env (from a partially evaluated function that yielded this function)
                for (const auto& [_, obj] : func.envs.env.env) {
                    const auto& [name, __, ___] = obj;
                    if (type->involvesT(type::ExprType{std::make_shared<expr::Name>(name.name)}))
                        return true;
                }

                return false;
            };

            // for(size_t i{}, curr{}; const auto& [param, expr] : std::views::zip(pos_params, args)) {
            for (size_t i{}, p{}, curr{}; p < args_size; ++p, ++i) {

                if (curr < expand_at.size() and i == expand_at[curr].first) {
                    for (auto& val : expand_at[curr++].second) {
                        auto& [sid, type] = pos_params[p];
                        const auto& [name, id, is_syntax] = sid;

                        if (findType(p, type)) {
                            // ScopeGuard sg{this, func.args_env, args_env};
                            ScopeGuard sg{this, func.envs.env.env, args_env};
                            type = validateType(std::move(type));
                        }
                        ++p;

                        val = typeCheck(val, type,
                            "Type mis-match! Parameter '" + name + "' expected type: " + type->text() + ", got: " + typeOf(val)->text()
                        );

                        if (std::holds_alternative<expr::Closure>(val))
                            captureEnvForPassedClosure(get<expr::Closure>(val));

                        // sg.addEnv({{name, {std::make_shared<value::Value>(val), type}}});
                        args_env[id] = {{name}, std::make_shared<value::Value>(std::move(val)), std::move(type)};
                    }
                    --p;
                }
                else {
                    auto& [sid, type] = pos_params[p];
                    const auto& [name, id, is_syntax] = sid;
                    if (findType(p, type)) {
                        // ScopeGuard sg{this, func.args_env, args_env};
                        ScopeGuard sg{this, func.envs.env.env, args_env};
                        type = validateType(std::move(type));
                    }

                    const auto& expr = args[i];

                    value::Value value = std::visit(*this, expr->variant()).value;

                    value = typeCheck(value, type,
                        "Type mis-match! Parameter '" + name + "' expected type: " + type->text() + ", got: " + typeOf(value)->text()
                    );

                    // if (std::holds_alternative<expr::Closure>(value))
                    //     captureEnvForPassedClosure(get<expr::Closure>(value));

                    // sg.addEnv({{name, {std::make_shared<value::Value>(value), type}}});
                    args_env[id] = {{name}, std::make_shared<value::Value>(std::move(value)), std::move(type)};
                }
            }
        }



        // closure.captureArgs(args_env);
        closure.capture(args_env);

        return {closure, std::make_shared<type::FuncType>(closure.type)};
    }

    value::Value handleNonClasses(const expr::Call *call, const type::TypePtr type) {
        if (not call->args.empty()) util::error("Can't pass arguments to non-class types: " + call->stringify());
        if (type::isFunction(type)) util::error("Can't default-construct a function type: " + call->stringify());

        if (type::isBuiltin(type)) {
            auto type_name = type->text();

            if (type_name == "Bool"  ) return bool  {};
            if (type_name == "Int"   ) return int   {};
            if (type_name == "Double") return double{};
            if (type_name == "String") return std::string{};

            util::error("Can't default construct type '" + type_name + "': " + call->stringify());
        }

        if (type::isVariadic(type)) return value::makePack();
        if (type::isList    (type)) return value::makeList();
        if (type::isMap     (type)) return value::makeMap ();


        if (auto onion = type::isUnion(type)) {
            if (onion->types.empty()) util::error("Cannot default construct type 'Never', or 'union { }': " + call->stringify());

            return constructorCall(call, onion->types[0]); // construct the first type in the union
        }

        util::error();
    }


    void initializeRestOfMembers(
        value::Object& obj,
        const std::vector<std::tuple<pie::expr::Name, pie::type::TypePtr, pie::expr::ExprPtr>>& fields,
        const size_t starting_index
    ) {
        ScopeGuard sg{this};
        size_t index{};
        for (; index < starting_index; ++index) {
            const auto& [name, type, value] = obj.second->members[index];

            addVar(name.name, name.var_ID, value, type);
        }

        for (; index < fields.size(); ++index) {
            const auto& [name, typ, expr] = fields[index];

            type::TypePtr type = typ;

            value::Value v;


            type = validateType(std::move(type));

            v = std::visit(*this, expr->variant()).value;

            typeCheck(v, type,
                "In class member assignment '" +
                name.stringify() + ": " + typ->text() + " = " + expr->stringify() +
                ": Type mis-match! Expected: " + type->text() + ", got: " + typeOf(v)->text()
            );


            // maybe not allowing the usage of previous members in the initializers of other members is the way? not sure
            const auto value = std::make_shared<value::Value>(v);
            addVar(name.name, name.var_ID, value, type);
            obj.second->members.push_back({name, type, value});
        }

        // return members;
    }


    value::Value constructorCall(const expr::Call *call, value::Value var) {
        auto type = validateType(get<type::TypePtr>(var));


        if (not type::isClass(type)) return handleNonClasses(call, std::move(type));

        auto *cls = dynamic_cast<const type::LiteralType*>(type.get())->cls.get();

        if (call->args.size() > cls->blueprint->fields.size())
            util::error("Too many arguments passed to constructor of class: " + value::stringify(type) + "\nin constructor call:\n" + call->stringify());


        value::Object obj{type, std::make_shared<value::Members>(
            std::vector<std::tuple<expr::Name, type::TypePtr, value::ValuePtr>>() // reserve fields.size() elements
        )};


        auto old_spaces = std::move(current_space);
        current_space = std::move(cls)->spaces;
        util::Deferred d2{[this, &old_spaces, cls] {
            cls->spaces = std::move(current_space);
            current_space = std::move(old_spaces);
        }};


        // I woulda used a range for-loop but I need `arg` to be a reference and `value` cannot be a regular ref
        // const auto& [arg, value] : std::views::zip(call->args, obj->members)
        ScopeGuard sg{this, cls->env.env};
        sg.addOps(cls->env.op_env);
        sg.addPrefixOps(cls->env.prefix_op_env);

        size_t field_idx{};
        for (const auto& arg : call->args) {

            if (const auto expand = dynamic_cast<const expr::Expansion*>(arg.get())) {
                const value::Value pack = std::visit(*this, expand->pack->variant()).value;

                if (not std::holds_alternative<value::Pack>(pack))
                    util::error("Expansion applied on a non-pack variable: " + arg->stringify());


                for (const auto& v : get<value::Pack>(pack)->values) {
                    auto& [name, type, _] = cls->blueprint->fields[field_idx++];
                    auto new_type = validateType(type);

                    typeCheck(v, new_type,
                        "Type mis-match in constructor of:\n" + value::stringify(new_type) + "\nMember `" +
                        name.stringify() + "` expected: " + new_type->text() + "\n"
                        "but got: " + arg->stringify() + " which is " + typeOf(v)->text()
                    );

                    obj.second->members.push_back({name, new_type, std::make_shared<value::Value>(v)});
                }
            }
            else {
                const value::Value v = std::visit(*this, std::move(arg)->variant()).value;
                auto& [name, type, _] = cls->blueprint->fields[field_idx++];

                auto new_type = validateType(type); // is this.....fine??

                typeCheck(v, new_type,
                    "Type mis-match in constructor of:\n" + value::stringify(new_type) + "\nMember `" +
                    name.stringify() + "` expected: " + new_type->text() + "\n"
                    "but got: " + arg->stringify() + " which is " + typeOf(v)->text()
                );

                obj.second->members.push_back({name, new_type, std::make_shared<value::Value>(v)});
            }
        }

        initializeRestOfMembers(obj, cls->blueprint->fields, field_idx);

        return obj;
    }


    ValueType operator()(const expr::Closure *c) {
        if (const auto& var = getVar(c->var_ID, liftName(c)); var) return *var;

        expr::Closure closure = *c; // copy to use for fix the types

        // for (auto& type : closure.type.params) {
        for (size_t i{}; i < closure.type.params.size(); ++i) {
            auto& type = closure.type.params[i];

            bool found{};
            for (size_t j{}; j < i; ++j) {
                if (
                    type->involvesT(
                        type::ExprType{std::make_shared<expr::Name>(closure.params[j].name)}
                    )
                ) {
                    found = true;
                    break;
                }
            }

            if (not found) type = validateType(std::move(type));
        }

        if (
            std::ranges::find_if(
                closure.params, [&type = closure.type.ret](const auto& p) {
                    return type->involvesT(type::ExprType{std::make_shared<expr::Name>(p.name)});
                }
            ) == closure.params.cend()
        )
            closure.type.ret = validateType(std::move(closure.type.ret));

        // for (auto& type : closure.type.params) { type = validateType(std::move(type)); }
        // closure.type.ret = validateType(std::move(closure.type.ret));

        closure.inSpace(current_space);

        // capture all the operators now:
        for (const auto& e : env)
            closure.captureOps(e->op_env);

        for (const auto& e : env)
            closure.capturePrefixOps(e->prefix_op_env);


        // THIS LINE HERE causes FPS to drop from around 4K to 250..
        // What gives???
        // for (const auto& [e, _, __, ___] : env)
        //     closure.capture(e);

        return {closure, std::make_shared<type::FuncType>(closure.type)};
    }


    ValueType operator()(const expr::Block *block) {
        if (const auto& var = getVar(block->var_ID, liftName(block)); var) return *var;


        ScopeGuard sg{this};


        bool last_expr_is_block{};

        value::Value ret;
        type::TypePtr type;
        for (const auto& line : block->lines) {
            last_expr_is_block = dynamic_cast<const expr::Block*>(line.get());

            auto [v, t] = std::visit(*this, line->variant()); // a scope's value is the last expression
            ret  = std::move(v);
            type = std::move(t);

            // if any expression above breaks or continues, stop execution
            if (broken or continued) break;
        }



        if (not last_expr_is_block and std::holds_alternative<expr::Closure>(ret))
            captureEnvForReturnedClosure(get<expr::Closure>(ret));

        return {ret, type};
    }


    ValueType operator()(const expr::Fix *fix) {
        if (const auto& var = getVar(fix->var_ID, liftName(fix)); var) return *var;
        // return std::visit(*this, fix->func->variant());

        if (const auto& var = getVar(fix->funcs[0]->var_ID, liftName(fix->funcs[0].get())); var)
            util::error("Can only assign operators to closure literals: " + fix->stringify());

        auto func = dynamic_cast<expr::Closure*>(fix->funcs[0].get());
        // this is needed in the case the operator is applied in another namespace (most likely)
        func->inSpace(current_space);

        for (auto& t : func->type.params) t = validateType(std::move(t));

        func->type.ret = validateType(std::move(func)->type.ret);


        switch (fix->type()) {
            using enum token::TokenKind;

            case PREFIX:
                if (prefixOpsContain(fix->name))
                    findPrefixOp(fix->name)->funcs.push_back(fix->funcs[0]); // assuming each fix expression has a single func in it
                else
                    env.back()->prefix_op_env[fix->name] = fix->clone();
                break;

            case EXFIX : {
                auto exfix = dynamic_cast<const expr::Exfix*>(fix);
                if (prefixOpsContain(fix->name)) {
                    findPrefixOp(exfix->name )->funcs.push_back(fix->funcs[0]);
                    findPrefixOp(exfix->name2)->funcs.push_back(fix->funcs[0]);
                }
                else {
                    env.back()->prefix_op_env[exfix->name ] = fix->clone();
                    env.back()->prefix_op_env[exfix->name2] = fix->clone();
                }

            }
            break;

            case INFIX :
            case SUFFIX:
                if (opsContain(fix->name))
                    findOp(fix->name)->funcs.push_back(fix->funcs[0]);
                else env.back()->op_env[fix->name] = fix->clone();
                break;

            case MIXFIX: {
                auto mixfix = dynamic_cast<const expr::Operator*>(fix);
                if (mixfix->isPrefix()) {
                    if (prefixOpsContain(mixfix->name)) {
                        findPrefixOp(mixfix->name)->funcs.push_back(mixfix->funcs[0]);
                        for (const auto& sub_name : mixfix->rest) {
                            findOp(sub_name)->funcs.push_back(mixfix->funcs[0]);
                        }
                    }
                    else {
                        env.back()->prefix_op_env[mixfix->name] = mixfix->clone();

                        for (const auto& sub_name : mixfix->rest)
                            env.back()->op_env[sub_name] = mixfix->clone();
                    }
                }
                else {
                    if (opsContain(fix->name)) {
                        findOp(fix->name)->funcs.push_back(fix->funcs[0]);
                        for (const auto& sub_name : mixfix->rest) {
                            findOp(sub_name)->funcs.push_back(mixfix->funcs[0]);
                        }
                    }
                    else {
                        env.back()->op_env[mixfix->name] = mixfix->clone();

                        for (const auto& sub_name : mixfix->rest)
                            env.back()->op_env[sub_name] = mixfix->clone();
                    }
                }
            }

            default:;
        }

        return {*func, std::make_shared<type::FuncType>(func->type)};
        // return std::visit(*this, fix->funcs[0]->variant());
    }


    [[nodiscard]] bool isBuiltin(const std::string_view func) const {
        const auto make_builtin = [] (const std::string& n) { return "__builtin_" + n; };

        for(const auto& builtin : {
            //* variadic
            "print", "concat", "defer",
            "create_class",

            // debugging
            "print_env",
            "panic",
            "id",

            //* nullary
            "input_str", "input_int",

            //* unary
            "type", "decltype",
            "len", "reverse",
            "reset", "eval",
            "neg", "abs",
            "not",
            "to_int", "to_double", "to_string",

            //* binary
            "get", "push", "pop", "pop_front", "remove_at",
            "add", "sub", "mul", "div", "mod",
            "pow",
            "gt", "geq", "eq", "leq", "lt",
            "and", "or",  
            "bit_and", "bit_or", "xor",

            "rand_int",

            //* trinary
            "set",
            "conditional",

            //* quaternary 
            "str_slice", // (str, start, end, step), should I add another overload for (str, start, length)??
            "str_split",


            //* File IO
            "open_file",
            "close_file",
            "is_file_open",
            "read_file",
            "read_line",
            "read_word",


            //* FFI shit
            "dlopen",
            "dlsym" ,
            "ffi_call"  ,

            "ffi_type_void"   ,
            "ffi_type_int"    ,
            "ffi_type_float"  ,
            "ffi_type_double" ,
            "ffi_type_long_double",
            "ffi_type_uint8"  ,
            "ffi_type_sint8"  ,
            "ffi_type_uint16" ,
            "ffi_type_sint16" ,
            "ffi_type_uint32" ,
            "ffi_type_sint32" ,
            "ffi_type_uint64" ,
            "ffi_type_sint64" ,
            "ffi_type_struct" ,
            "ffi_type_pointer",
            "ffi_type_cstring",
            "ffi_type_complex",
            "ptr_to_string",
        })
            if (make_builtin(builtin) == func) return true;

        return false;
    }



    std::vector<value::Value> expandArgs(
        std::vector<expr::ExprPtr> args,
        std::vector<std::pair<size_t, std::vector<value::Value>>> expand_at
    ) {
        std::vector<value::Value> values;

        for(size_t i{}, curr{}; auto& arg : args) {
            if (curr < expand_at.size() and i++ == expand_at[curr].first) {
                for (auto& v : expand_at[curr++].second) {
                    values.push_back(std::move(v));
                }
            }
            else
                values.push_back(std::visit(*this, arg->variant()).value);
        }

        return values;
    }




    // the gate into the META operators!
    value::Value evaluateBuiltin(
        const expr::Call *call,
        std::vector<expr::ExprPtr> args,
        std::vector<std::pair<size_t, std::vector<value::Value>>> expand_at,
        std::unordered_map<std::string, expr::ExprPtr> named_args,
        value::BuiltinFunction func
    ) {
        using std::operator""sv;


        auto name = std::move(func).func_name.substr(10); // cutout the "__builtin_"
        const auto arity_check = [&name, &args] (const size_t arity) {
            if (args.size() != arity) util::error("Wrong arity with call to \"__builtin_" + name + "\"");
        };



        if (name == "panic") {
            // for (const auto& arg : args) {
            //     std::print(std::cerr, "{} ", stringify(std::visit(*this, arg->variant()).value));
            // }
            std::println(
                std::cerr,
                "{}",
                std::accumulate(
                    args.cbegin(),
                    args.cend(),
                    std::string{},
                    [this] (const auto& acc, const auto& elt) {
                        return acc + " " + value::stringify(std::visit(*this, elt->variant()).value);
                    }
                )
            );
            util::error<std::runtime_error, false>("");
        }

        if (name == "id") {
            arity_check(1);
            return args[0]->var_ID;
        }

        if (name == "print_env") {
            for (const auto& e : env)
                printEnv(e->env);
            return 0;
        }


        if (name == "defer") {
            return defer(call, std::move(args));
        }


        if (name == "create_class") {
            auto arguments = expandArgs(std::move(args), std::move(expand_at));

            std::vector<std::tuple<expr::Name, type::TypePtr, expr::ExprPtr>> fields;
            value::Env vars;

            for (ssize_t i{-10}; auto& arg : arguments) {
                if (not std::holds_alternative<value::List>(arg))
                    util::error<except::InvalidArgument>("`__builtin_create_class` expects its arguments as lists. Got: " + value::stringify(arg));

                auto& list = get<value::List>(arg);
                if (list.elts->values.size() > 3)
                    util::error<except::InvalidArgument>("`__builtin_create_class` expects its list to have at most size 3: " + value::stringify(arg));

                if (list.elts->values.size() < 2)
                    util::error<except::InvalidArgument>("`__builtin_create_class` expects its list to have at least 2 elements {name, value}: " + value::stringify(arg));

                value::Value name = list.elts->values.front();
                type::TypePtr type;
                if (list.elts->values.size() == 3) {
                    if (not std::holds_alternative<type::TypePtr>(list.elts->values[1]))
                        util::error<except::InvalidArgument>(
                            "Second member `__builtin_create_class` list argument must be a type. Got: "
                            + value::stringify(list.elts->values[1])
                            + " which is: " + typeOf(list.elts->values[1])->text()
                        );

                    type = get<type::TypePtr>(list.elts->values[1]);
                }
                else type = type::builtins::Any();

                value::Value value = list.elts->values.back();

                if (not std::holds_alternative<std::string>(name))
                    util::error<except::InvalidArgument>(
                        "First member `__builtin_create_class` list argument must be a string. Got: "
                        + value::stringify(name)
                        + " which is: " + typeOf(name)->text()
                    );

                std::string mangled_name = "__" + get<std::string>(name) + "__";
                vars.env.insert({
                    --i,
                    {
                        value::SpaceRef{mangled_name},
                        std::make_shared<value::Value>(value),
                        type
                    }}
                );


                auto expr = std::make_shared<expr::Name>(std::move(mangled_name));
                expr->var_ID = std::to_underlying(analysis::LexicalAnalysis::ReservedIDs::DYNAMIC);
                fields.emplace_back(
                    expr::Name{get<std::string>(std::move(name))},
                    std::move(type),
                    std::move(expr)
                );
            }


            return createClass(std::move(fields), std::move(vars)).value;
        }



        // for now, can only implement variadic functions inlined in this function
        // seems like functions with default named parameters can only be implmented this way for now
        // need a way to sepcify:
        /* // TODO
            {
                name: "print",
                arg_count: VARIADIC,
                code: [] () {},
                default named: {
                    {"end", "\n"},
                    {"sep", " "},
                }
            }
        */
        // would be nice if the system above could be done for member functions on premitive types (Int, Double, String, etc...)
        if (name == "print") {
            return builtinPrint(std::move(args), std::move(expand_at), std::move(named_args));
        }

        if (name == "concat") {
            return builtinConcat(std::move(args));
        }

        #if not WEB_PIE
        if (name == "ffi_call") {
            return ffiCall(call, std::move(args), std::move(expand_at));
        }
        #endif




        const auto nullary_funcs = {
            "input_str"sv       ,
            "input_int"sv       ,
            "ffi_type_void"sv   ,
            "ffi_type_int"sv    ,
            "ffi_type_float"sv  ,
            "ffi_type_double"sv ,
            "ffi_type_long_double"sv,
            "ffi_type_uint8"sv  ,
            "ffi_type_sint8"sv  ,
            "ffi_type_uint16"sv ,
            "ffi_type_sint16"sv ,
            "ffi_type_uint32"sv ,
            "ffi_type_sint32"sv ,
            "ffi_type_uint64"sv ,
            "ffi_type_sint64"sv ,
            "ffi_type_struct"sv ,
            "ffi_type_pointer"sv,
            "ffi_type_cstring"sv,
            "ffi_type_complex"sv,
        };

        if (std::ranges::find(nullary_funcs, name) != nullary_funcs.end()) {
            arity_check(0);
            if (name == "input_str"           ) return execute<0>(stdx::get<S<"input_str"       >>(functions).value, {}, this);
            if (name == "input_int"           ) return execute<0>(stdx::get<S<"input_int"       >>(functions).value, {}, this);

            #if not WEB_PIE
            if (name == "ffi_type_int"        ) return execute<0>(stdx::get<S<"ffi_type_int"    >>(functions).value, {}, this);
            if (name == "ffi_type_pointer"    ) return execute<0>(stdx::get<S<"ffi_type_pointer">>(functions).value, {}, this);
            if (name == "ffi_type_cstring"    ) return execute<0>(stdx::get<S<"ffi_type_cstring">>(functions).value, {}, this);
            if (name == "ffi_type_void"       ) return execute<0>(stdx::get<S<"ffi_type_void"   >>(functions).value, {}, this);
            if (name == "ffi_type_float"      ) return execute<0>(stdx::get<S<"ffi_type_float"  >>(functions).value, {}, this);
            if (name == "ffi_type_double"     ) return execute<0>(stdx::get<S<"ffi_type_double" >>(functions).value, {}, this);
            if (name == "ffi_type_long_double") return execute<0>(stdx::get<S<"ffi_type_long_double">>(functions).value, {}, this);
            if (name == "ffi_type_uint8"      ) return execute<0>(stdx::get<S<"ffi_type_uint8"  >>(functions).value, {}, this);
            if (name == "ffi_type_sint8"      ) return execute<0>(stdx::get<S<"ffi_type_sint8"  >>(functions).value, {}, this);
            if (name == "ffi_type_uint16"     ) return execute<0>(stdx::get<S<"ffi_type_uint16" >>(functions).value, {}, this);
            if (name == "ffi_type_sint16"     ) return execute<0>(stdx::get<S<"ffi_type_sint16" >>(functions).value, {}, this);
            if (name == "ffi_type_uint32"     ) return execute<0>(stdx::get<S<"ffi_type_uint32" >>(functions).value, {}, this);
            if (name == "ffi_type_sint32"     ) return execute<0>(stdx::get<S<"ffi_type_sint32" >>(functions).value, {}, this);
            if (name == "ffi_type_uint64"     ) return execute<0>(stdx::get<S<"ffi_type_uint64" >>(functions).value, {}, this);
            if (name == "ffi_type_sint64"     ) return execute<0>(stdx::get<S<"ffi_type_sint64" >>(functions).value, {}, this);
            if (name == "ffi_type_struct"     ) return execute<0>(stdx::get<S<"ffi_type_struct" >>(functions).value, {}, this);
            if (name == "ffi_type_complex"    ) return execute<0>(stdx::get<S<"ffi_type_complex">>(functions).value, {}, this);
            #endif
        }


        if (name == "decltype") {
            if (args.size() != 1)
                util::error("`__builtin_decltype` takes in 1 argument only: "  + call->stringify());

            const auto *name = dynamic_cast<expr::Name*>(args[0].get());
            if (not name)
                util::error("`__builtin_decltype` takes in proper names ony: " + call->stringify());

            return declType({name->name, name->var_ID});
        }


        const auto unary_funcs = {
            "type"         ,
            "len"          ,
            "reverse"      ,
            "eval"         ,
            "abs"          ,
            "neg"          ,
            "not"          ,
            "pop"          ,
            "pop_front"    ,
            "to_int"       ,
            "to_double"    ,
            "to_string"    ,
            "dlopen"       ,
            "ptr_to_string",
            "open_file"    ,
            "close_file"   ,
            "is_file_open",
            "read_file"    ,
            "read_line"    ,
            "read_word"    ,
        };

        if (std::ranges::find(unary_funcs, name) != unary_funcs.end()) arity_check(1); // just for now..


        // evaluating arguments from left to right as needed
        // first argument is always evaluated
        const value::Value value1 = std::visit(*this, args[0]->variant()).value;

        // Since this is a meta function that operates on AST nodes rather than values
        // it gets its special treatment here..
        if (name == "reset") {
            if (const auto& v = getVar(args[0]->var_ID, liftName(args[0].get())); not v) util::error("Reseting an unset value: " + args[0]->stringify());
            else removeVar(args[0]->var_ID);

            return value1;
        }


        if (name == "type"         ) return execute<1>(stdx::get<S<"type"      >>(functions).value, {value1}, this);
        if (name == "len"          ) return execute<1>(stdx::get<S<"len"       >>(functions).value, {value1}, this);
        if (name == "reverse"      ) return execute<1>(stdx::get<S<"reverse"   >>(functions).value, {value1}, this);
        if (name == "eval"         ) return execute<1>(stdx::get<S<"eval"      >>(functions).value, {value1}, this);
        if (name == "abs"          ) return execute<1>(stdx::get<S<"abs"       >>(functions).value, {value1}, this);
        if (name == "neg"          ) return execute<1>(stdx::get<S<"neg"       >>(functions).value, {value1}, this);
        if (name == "not"          ) return execute<1>(stdx::get<S<"not"       >>(functions).value, {value1}, this);
        if (name == "pop"          ) return execute<1>(stdx::get<S<"pop"       >>(functions).value, {value1}, this);
        if (name == "pop_front"    ) return execute<1>(stdx::get<S<"pop_front" >>(functions).value, {value1}, this);
        if (name == "to_int"       ) return execute<1>(stdx::get<S<"to_int"    >>(functions).value, {value1}, this);
        if (name == "to_double"    ) return execute<1>(stdx::get<S<"to_double" >>(functions).value, {value1}, this);
        if (name == "to_string"    ) return execute<1>(stdx::get<S<"to_string" >>(functions).value, {value1}, this);

        if (name == "open_file") {
            // gotta normalize the file path first

            if (not std::holds_alternative<std::string>(value1))
                util::error<except::InvalidArgument>(
                    "`__builtin_open_file` expected a string for the first argument, got `" +
                    stringify(value1) + "`:\n" + call->stringify()
                );

            std::filesystem::path path = get<std::string>(value1);

            if (not path.is_absolute()) path = root / path;

            return execute<1>(stdx::get<S<"open_file" >>(functions).value, {value::Value(path.string())}, this);
        }
        if (name == "close_file"  ) return execute<1>(stdx::get<S<"close_file"  >>(functions).value, {value1}, this);
        if (name == "is_file_open") return execute<1>(stdx::get<S<"is_file_open">>(functions).value, {value1}, this);
        if (name == "read_file"   ) return execute<1>(stdx::get<S<"read_file"   >>(functions).value, {value1}, this);
        if (name == "read_line"   ) return execute<1>(stdx::get<S<"read_line"   >>(functions).value, {value1}, this);
        if (name == "read_word"   ) return execute<1>(stdx::get<S<"read_word"   >>(functions).value, {value1}, this);


        #if not WEB_PIE
        if (name == "dlopen"       ) return execute<1>(stdx::get<S<"dlopen"    >>(functions).value, {value1}, this);
        if (name == "ptr_to_string") return execute<1>(stdx::get<S<"ptr_to_string">>(functions).value, {value1}, this);
        #endif

        // all the rest of those funcs expect 2 arguments

        const auto eager = {
            "get"sv,
            "push"sv,
            "remove_at"sv,
            "str_split"sv,
            "add"sv,
            "sub"sv,
            "mul"sv,
            "div"sv,
            "bit_and"sv,
            "bit_or"sv,
            "xor"sv,
            "rand_int"sv,
            "mod"sv,
            "pow"sv,
            "gt"sv,
            "geq"sv,
            "eq"sv,
            "leq"sv,
            "lt"sv,
            "dlsym"sv
        };

        if (std::ranges::find(eager, name) != eager.end()) {
            arity_check(2);
            const value::Value value2 = std::visit(*this, args[1]->variant()).value;

            // this is disgusting..I know
            if (name == "get"      ) return execute<2>(stdx::get<S<"get"      >>(functions).value, {value1, value2}, this);
            if (name == "push"     ) return execute<2>(stdx::get<S<"push"     >>(functions).value, {value1, value2}, this);
            if (name == "remove_at") return execute<2>(stdx::get<S<"remove_at">>(functions).value, {value1, value2}, this);
            if (name == "str_split") return execute<2>(stdx::get<S<"str_split">>(functions).value, {value1, value2}, this);

            if (name == "add"     ) return execute<2>(stdx::get<S<"add"     >>(functions).value, {value1, value2}, this);
            if (name == "sub"     ) return execute<2>(stdx::get<S<"sub"     >>(functions).value, {value1, value2}, this);
            if (name == "mul"     ) return execute<2>(stdx::get<S<"mul"     >>(functions).value, {value1, value2}, this);
            if (name == "div"     ) return execute<2>(stdx::get<S<"div"     >>(functions).value, {value1, value2}, this);
            if (name == "mod"     ) return execute<2>(stdx::get<S<"mod"     >>(functions).value, {value1, value2}, this);
            if (name == "pow"     ) return execute<2>(stdx::get<S<"pow"     >>(functions).value, {value1, value2}, this);
            if (name == "bit_and" ) return execute<2>(stdx::get<S<"bit_and" >>(functions).value, {value1, value2}, this);
            if (name == "bit_or"  ) return execute<2>(stdx::get<S<"bit_or"  >>(functions).value, {value1, value2}, this);
            if (name == "xor"     ) return execute<2>(stdx::get<S<"xor"     >>(functions).value, {value1, value2}, this);
            if (name == "rand_int") return execute<2>(stdx::get<S<"rand_int">>(functions).value, {value1, value2}, this);
            if (name == "gt"      ) return execute<2>(stdx::get<S<"gt"      >>(functions).value, {value1, value2}, this);
            if (name == "geq"     ) return execute<2>(stdx::get<S<"geq"     >>(functions).value, {value1, value2}, this);
            if (name == "eq"      ) return execute<2>(stdx::get<S<"eq"      >>(functions).value, {value1, value2}, this);
            if (name == "leq"     ) return execute<2>(stdx::get<S<"leq"     >>(functions).value, {value1, value2}, this);
            if (name == "lt"      ) return execute<2>(stdx::get<S<"lt"      >>(functions).value, {value1, value2}, this);

            #if not WEB_PIE
            if (name == "dlsym"   ) return execute<2>(stdx::get<S<"dlsym"   >>(functions).value, {value1, value2}, this);
            #endif

            util::error();
        }


        if (name == "and") {
            arity_check(2);

            if (not std::holds_alternative<bool>(value1)) return value1; // return first falsy value
            if (not get<bool>(value1)) return value1; // first falsey value


            return std::visit(*this, args[1]->variant()).value; // last truthy value
        }

        if (name == "or" ) {
            arity_check(2);
            if (not std::holds_alternative<bool>(value1)) return std::visit(*this, args[1]->variant()).value; // last falsey value


            if(get<bool>(value1)) return value1; // first truthy value
            return std::visit(*this, args[1]->variant()).value; // last falsey value
        }


        if (name == "conditional") {
            arity_check(3);
            const auto& then      = args[1]->variant();
            const auto& otherwise = args[2]->variant();

            if (not std::holds_alternative<bool>(value1)) return std::visit(*this, otherwise).value;

            if(get<bool>(value1)) return std::visit(*this, then).value;


            return std::visit(*this, otherwise).value;
        }

        if (name == "set") {
            arity_check(3);
            const value::Value value2 = std::visit(*this, args[1]->variant()).value;
            const value::Value value3 = std::visit(*this, args[2]->variant()).value;

            return execute<3>(stdx::get<S<"set">>(functions).value, {value1, value2, value3}, this);
        }

        if (name == "str_slice") {
            arity_check(4);
            const value::Value start_v  = std::visit(*this, args[1]->variant()).value;
            const value::Value end_v    = std::visit(*this, args[2]->variant()).value;
            const value::Value stride_v = std::visit(*this, args[3]->variant()).value;

            if (
                not std::holds_alternative<std::string>(value1  ) or
                not std::holds_alternative<     BigInt>( start_v) or
                not std::holds_alternative<     BigInt>(   end_v) or
                not std::holds_alternative<     BigInt>(stride_v)
            )
                util::error<pie::except::InvalidArgument>(
                    "__builtin_str_slice("
                    + args[0]->stringify() + ", "
                    + args[1]->stringify() + ", "
                    + args[2]->stringify() + ", "
                    + args[3]->stringify() + ")"
                );


            const auto& str = get<std::string>(value1);
            auto start = std::max<BigInt> (get<BigInt>(start_v), 0);
            const auto end = std::clamp<BigInt>(get<BigInt>(  end_v), 0, (BigInt)(str.length()));
            const auto stride = get<BigInt>(stride_v);

            std::string ret;
            for (; start < end; start += stride)
                ret += str[start];

            return ret;
        }

        util::error("Calling a builtin fuction that doesn't exist!");
    }


    value::Value builtinPrint(
        std::vector<expr::ExprPtr> args,
        std::vector<std::pair<size_t, std::vector<value::Value>>> expand_at,
        std::unordered_map<std::string, expr::ExprPtr> named_args
    ) {
        // if (args.empty()) util::error("'print' requires at least 1 positional argument passed!");

        using std::operator""sv;
        const auto allowed_params = {"sep"sv, "end"sv};

        for (const auto& [name, _] : named_args)
            if (std::ranges::find(allowed_params, name) == allowed_params.end())
                util::error("Can only have the named argument 'end'/'sep' in call to '__builtin_print': found '" + name + "'!");


        const value::Value& sep =
            named_args.contains("sep") ?
                std::visit(*this, named_args.at("sep")->variant()).value : " ";

        constexpr bool no_newline = false;

        std::optional<value::Value> separator;
        value::Value ret;
        for(size_t i{}, curr{}; auto& arg : args) {
            if (curr < expand_at.size() and i++ == expand_at[curr].first) {
                for (const auto& e : expand_at[curr++].second) {
                    if (separator) print(*separator, no_newline);

                    print(e, no_newline);

                    if (not separator) separator = sep;
                }
            }
            else {
                if (separator) print(*separator, no_newline);

                ret = std::visit(*this, arg->variant()).value;
                print(ret, no_newline);

                if (not separator) separator = sep;
            }
        }

        if (named_args.contains("end"))
                print(std::visit(*this, named_args.at("end")->variant()).value, no_newline);
        else puts(""); // print the new line in the end.

        return ret;
    }


    value::Value builtinConcat(std::vector<expr::ExprPtr> args) {
        if (args.size() < 2) util::error("'concat' requires at least 2 argument passed!");

        // we try to avoid repetitive appends
        // so we reserve the space beforehand

        size_t size{};
        std::vector<std::string> strings(args.size());

        for(const auto& arg : args) {
            const value::Value& v = std::visit(*this, arg->variant()).value;

            // in the future, make this accept lists, (packs?), and maps
            if (not std::holds_alternative<std::string>(v)) util::error("'concat' only accepts strings as arguments: " + stringify(v));

            auto& string = std::move(get<std::string>(v));
            size += string.size();
            strings.push_back(std::move(string));
        }

        std::string s;
        s.reserve(size);

        for (auto& string : strings)
            s.append(std::move(string));

        return s;
    }


    value::Value defer(const expr::Call *call, std::vector<expr::ExprPtr> args) {
        if (args.empty()   ) util::error("`__builtin_defer` requires at least 1 argument: " + call->stringify());
        if (args.size() > 2) util::error("`__builtin_defer` accepts at most 2 argument: " + call->stringify());

        // 
        BigInt depth{};

        if (args.size() == 2) {
            auto depth_value = std::visit(*this, args[1]->variant()).value;
            if (not std::holds_alternative<BigInt>(depth_value))
                util::error(
                    "At call: " + call->stringify() + 
                    ", `__builtin_defer` expected an integer for optional argument `depth`. "
                    "Got: " + value::stringify(depth_value) +
                    "\nwhich is: " + typeOf(depth_value)->text()
                );

            depth = get<BigInt>(depth_value);

            if (depth < 0)
                util::error("`__builtin_defer` called with a negative depth level: " + call->stringify());

            // depth won't underflow since it's guarunteed not to be negative (checked above)
            if (size_t(depth) >= deferred.size())
                util::error("`__builtin_defer` called with a depth greater than the current depth: " + call->stringify());
        }



        deferred[deferred.size() - 1 - depth].emplace_back(args[0], env.back());

        return args.front()->variant();
    }



    #if not WEB_PIE
    value::Value ffiCall(
        const expr::Call *call,
        std::vector<expr::ExprPtr> args,
        std::vector<std::pair<size_t, std::vector<value::Value>>> expand_at
    ) {
        const auto args_size = argsSize(args, expand_at);
        if (args_size < 2) util::error("`__builtin_call` requires the symbol name and the CIF!");

        const auto sym = reinterpret_cast<void*>(get<BigInt>(std::visit(*this, args[0]->variant()).value));
        const auto pie_cif = get<value::Object>(std::visit(*this, args[1]->variant()).value);

        const auto reserve_size = args_size - 2;

        std::vector<ffi_type*> param_types;
        param_types.reserve(reserve_size);

        std::vector<void*> values_pointers;
        values_pointers.reserve(reserve_size);

        // to keep the bytes "alive" for the duration of the call
        std::deque<std::vector<std::byte>> payloads;


        // Owns the ffi_type shape (for structs, the heap-allocated ffi_type tree)
        // for every argument for the lifetime of the call
        std::vector<std::unique_ptr<ffi::FFI>> arg_ffis;


        // Pie has no pointer type: a `T*` parameter that receives an Object
        // (or a List of Objects) is understood as "pass this struct (or
        // contiguous array of structs) by reference" - the interpreter
        // builds the C-side buffer(s), and after the call writes any
        // changes the C function made back into the same Pie object(s).
        struct PointerWriteback {
            std::byte* buffer;
            ffi::FFI* elem_shape;
            // Pie Object, or ListValue, either carry a shared_ptr, so mutating through them reaches the caller's original value
            value::Value target;
            bool is_array;
            size_t count;
        };
        std::vector<PointerWriteback> writebacks;

        // `__return_type` is either a plain C type id (BigInt) for scalars,
        // or a Pie Object "template" describing a struct return value.
        value::Value return_type_desc;

        // look for `param_types` and `return_types`
        bool found_params{}, found_return{};
        for (const auto& [name, _, value_ptr] : pie_cif.second->members) {
            if (name.name == "__param_types") {
                found_params = true;

                if (not std::holds_alternative<value::List>(*value_ptr)) util::error();


                const auto& list = get<value::List>(*value_ptr);
                if (list.elts->values.size() != reserve_size) // `reserve_size` instead of `args_size` because `-2` to exclude the sym and cif
                    util::error("Wrong number of arguments passed to function: " + call->stringify());


                for (
                    size_t i{}, p{2}, curr{}, val_idx{}; // skip the first 2 arguments
                    p < args_size;
                    ++i
                ) {
                    const auto& type = list.elts->values[i];
                    const auto& arg  = args[p];

                    value::Value value;
                    if (curr < expand_at.size()) {
                        if (p == expand_at[curr].first) {
                            if (val_idx < expand_at[curr].second.size()) {
                                value = expand_at[curr].second[val_idx++];
                            }
                            else {
                                p += val_idx;
                                val_idx = {};
                                ++curr;
                                continue;
                            }
                        }
                        else {
                            value = std::visit(*this, arg->variant()).value;
                        }
                    }
                    else {
                        value = std::visit(*this, arg->variant()).value;
                        ++p;
                    }


                    if (not std::holds_alternative<BigInt>(type))
                        util::error("Invalid C Type: " + value::stringify(type));

                    const auto type_id = get<BigInt>(type);

                    if (not (type_id >= 0 or type_id <= FFI_TYPE_LAST) and type_id != FFI_TYPE_CSTRING)
                        util::error("Invalid C Type: " + std::to_string(type_id));

                    // Pointer-to-struct(s): inferred from the shape of the
                    // Pie value actually passed, not from anything declared
                    // in the CIF. An Object means "pointer to one struct";
                    // a List means "pointer to a contiguous array of
                    // structs", sized and shaped by its elements. Either
                    // way the caller just passes the object/list like any
                    // other argument.
                    if (type_id == FFI_TYPE_POINTER and
                        (std::holds_alternative<value::Object>(value) or std::holds_alternative<value::List>(value)))
                    {
                        const bool is_array = std::holds_alternative<value::List>(value);
                        size_t count = 1;
                        const value::Value* template_elem = &value;

                        if (is_array) {
                            const auto& arr_list = get<value::List>(value);
                            count = arr_list.elts->values.size();

                            if (count == 0) {
                                // nothing to point at - pass a NULL pointer, nothing to write back
                                payloads.emplace_back(sizeof(void*));
                                std::memset(payloads.back().data(), 0, sizeof(void*));

                                param_types.push_back(&ffi_type_pointer);
                                values_pointers.push_back(payloads.back().data());
                                continue;
                            }

                            template_elem = &arr_list.elts->values[0];
                        }

                        if (not std::holds_alternative<value::Object>(*template_elem))
                            util::error(
                                "A `pointer` argument was given a List, but its elements aren't structs - "
                                "pointers to arrays of raw numbers aren't supported yet: " + value::stringify(value)
                            );

                        auto elem_shape = ffi::prepareFFI(*template_elem, FFI_TYPE_STRUCT);
                        const size_t elem_size = elem_shape->type->size;
                        auto* elem_shape_ptr = elem_shape.get();

                        payloads.emplace_back(elem_size * count);
                        auto* array_buf = payloads.back().data();

                        if (is_array) {
                            const auto& arr_list = get<value::List>(value);
                            for (size_t elt{}; elt < count; ++elt)
                                ffi::pack(array_buf + elt * elem_size, elem_shape_ptr, arr_list.elts->values[elt], payloads);
                        }
                        else {
                            ffi::pack(array_buf, elem_shape_ptr, value, payloads);
                        }

                        payloads.emplace_back(sizeof(void*)); // the actual pointer slot libffi dereferences
                        *reinterpret_cast<std::byte**>(payloads.back().data()) = array_buf;

                        param_types.push_back(&ffi_type_pointer);
                        values_pointers.push_back(payloads.back().data());

                        writebacks.push_back({array_buf, elem_shape_ptr, value, is_array, count});
                        arg_ffis.push_back(std::move(elem_shape));

                        continue;
                    }

                    // Build the C-level shape for this argument (scalar or
                    // struct, uniformly), pack the Pie value into raw bytes
                    // according to that shape, and hand libffi a pointer to
                    // those bytes. Every int width/signedness and both float
                    // widths are derived here purely from `type_id` - Pie
                    // itself only ever deals in BigInt/double.
                    auto node = ffi::prepareFFI(value, type_id);
                    param_types.push_back(node->type);

                    payloads.emplace_back(node->type->size);
                    auto* payload = payloads.back().data();
                    ffi::pack(payload, node.get(), value, payloads);

                    values_pointers.push_back(payload);
                    arg_ffis.push_back(std::move(node));
                }
            }
            else if (name.name == "__return_type") {
                found_return = true;
                return_type_desc = *value_ptr;

                if (not std::holds_alternative<BigInt>(return_type_desc) and
                    not std::holds_alternative<value::Object>(return_type_desc))
                    util::error("`__return_type` must be a C Type or a struct template Object: " + value::stringify(return_type_desc));
            }
        }

        if (not found_params) util::error("Pie CIF must have member named `__param_types`: " + value::stringify(pie_cif));
        if (not found_return) util::error("Pie CIF must have member named `__return_type`: " + value::stringify(pie_cif));

        const auto return_type_id =
            std::holds_alternative<value::Object>(return_type_desc) ?
                BigInt{FFI_TYPE_STRUCT} : get<BigInt>(return_type_desc);

        auto return_shape = ffi::prepareFFI(return_type_desc, return_type_id);

        ffi_cif cif{};
        const auto result = ffi_prep_cif(
            &cif,
            FFI_DEFAULT_ABI,
            values_pointers.size(),
            return_shape->type,
            param_types.data()
        );

        if (result != FFI_OK) util::error("Failed to prepare the FFI call for: " + call->stringify());

        const auto applyWritebacks = [&writebacks] {
            for (auto& wb : writebacks) {
                if (wb.is_array) {
                    auto& list = get<value::List>(wb.target);
                    for (size_t i{}; i < wb.count; ++i)
                        ffi::unpackInto(wb.buffer + i * wb.elem_shape->type->size, wb.elem_shape, list.elts->values[i]);
                }
                else {
                    ffi::unpackInto(wb.buffer, wb.elem_shape, wb.target);
                }
            }
        };

        if (return_shape->type->type == FFI_TYPE_VOID) {
            ffi_call(&cif, reinterpret_cast<void(*)()>(sym), nullptr, values_pointers.data());
            applyWritebacks();
            return BigInt{0};
        }

        // libffi requires the return buffer to be at least `sizeof(ffi_arg)`
        // for any integer type narrower than that (it always widens small
        // integer returns internally); struct/float/double returns are
        // unaffected by the requirement but sizing generously is harmless.
        std::vector<std::byte> ret_buffer(std::max(return_shape->type->size, sizeof(ffi_arg)), std::byte{0});

        ffi_call(&cif, reinterpret_cast<void(*)()>(sym), ret_buffer.data(), values_pointers.data());
        applyWritebacks();

        // `STR` is declared distinctly from `PTR` specifically so this can happen automatically
        // A function declared to return `STR` hands back a Pie String directly (read from the returned address)
        // while `PTR` keeps returning the raw address as a BigInt.
        // The binding author picks per-function once and every call site is seamless either way.
        if (return_type_id == FFI_TYPE_CSTRING) {
            void *cstr;
            memcpy(&cstr, ret_buffer.data(), sizeof cstr);

            if (not cstr) return "";

            return std::string{reinterpret_cast<const char*>(cstr)};
        }

        auto ret_value = ffi::unpack(ret_buffer.data(), return_shape.get(), return_type_desc);

        // convenience carried over from before: a `BYTE` (uint8) return of
        // exactly 0 or 1 is treated as a Pie Bool.
        if (return_shape->type->type == FFI_TYPE_UINT8 and std::holds_alternative<BigInt>(ret_value)) {
            const auto v = get<BigInt>(ret_value);
            if (v == 0 or v == 1) ret_value = bool(v);
        }

        return ret_value;
    }
    #endif



    void print(const value::Value& value, const bool new_line = true) const { std::print("{}{}", stringify(value), new_line? '\n' : '\0'); }


    type::TypePtr validateType(const type::TypePtr& type) {
        if (type::shouldReassign(type)) return type::builtins::Any();
        // //* comment this if statement if you want builtin types to remain unchanged even when they're assigned to


        if (auto var = getVar(type->ID, [&type] { return type->text(); }); var) {
            if (auto t = typeOf(var->value); not type::isType(t)) {
                if (type::isFunction(t))
                    return std::make_shared<type::ConceptType>(std::make_shared<value::Value>(std::move(var)->value));

                return std::make_shared<type::ValueType>(std::make_shared<value::Value>(std::move(var)->value));
            }

            if (std::holds_alternative<type::TypePtr>(var->value))
                return get<type::TypePtr>(var->value);
        }

        // else
        if (type::isBuiltin(type)) return type;


        if (const auto var_type = dynamic_cast<type::ExprType*>(type.get())) {
            // if (type::isBuiltin(type)) return type;

            value::Value value = std::visit(*this, var_type->t->variant()).value; // evaluate type expression

            if (std::holds_alternative<type::TypePtr>(value))
                return get<type::TypePtr>(value);

            return 
            std::make_shared<type::ValueType>(
                std::make_shared<value::Value>(std::move(value))
            );
        }

        else if (type::isClass(type) or type::isUnion(type)) return type;

        else if (type::isFunction(type)) {
            const auto func_type = dynamic_cast<type::FuncType*>(type.get());

            for (auto& t : func_type->params) t = validateType(std::move(t));

            // all param types are valid. Only thing left to check is return type
            func_type->ret = validateType(std::move(func_type)->ret);

            return type;
        }
        else if (type::isVariadic(type)) {
            const auto variadic_type = dynamic_cast<type::VariadicType*>(type.get());

            // // todo: allow this in the future
            // if (variadic_type->type->text() == "Syntax") util::error("Variadics of 'Syntax' is not allowed!");

            variadic_type->type = validateType(std::move(variadic_type)->type);

            return type;
        }
        else if (type::isList(type)) {
            const auto list_type = dynamic_cast<type::ListType*>(type.get());

            // todo: allow this in the future
            // if (list_type->type->text() == "Syntax") util::error("List of 'Syntax' is not allowed!");


            if (type::isVariadic(list_type->type)) util::error("Lists of variadics types are not allowed!");


            list_type->type = validateType(std::move(list_type)->type);

            return type;
        }
        else if (type::isMap(type)) {
            const auto map_type = dynamic_cast<type::MapType*>(type.get());

            // todo: allow this in the future
            // if (map_type->key_type->text() == "Syntax") util::error("Map of 'Syntax' is not allowed!");
            // if (map_type->val_type->text() == "Syntax") util::error("Map of 'Syntax' is not allowed!");


            if (type::isVariadic(map_type->key_type)) util::error("Map of variadics types are not allowed!");
            if (type::isVariadic(map_type->val_type)) util::error("Map of variadics types are not allowed!");


            map_type->key_type = validateType(std::move(map_type)->key_type);
            map_type->val_type = validateType(std::move(map_type)->val_type);

            return type;
        }



        util::error("'" + type->text() + "' does not name a type!");
    }


    type::TypePtr typeOf(const value::Value& value) const {
        if (std::holds_alternative<expr::Node > (value)) return type::builtins::Syntax();
        if (std::holds_alternative<BigInt     > (value)) return type::builtins::Int();
        if (std::holds_alternative<double     > (value)) return type::builtins::Double();
        if (std::holds_alternative<bool       > (value)) return type::builtins::Bool();
        if (std::holds_alternative<std::string> (value)) return type::builtins::String();

        // Type types
        if (std::holds_alternative<type::TypePtr> (value)) return type::builtins::Type();

        if (std::holds_alternative<expr::Closure>(value)) {
            const auto& func = get<expr::Closure>(value);

            type::FuncType type{{}, {}};
            for (const auto& t : func.type.params)
                type.params.push_back(t);


            type.ret = func.type.ret;
            return std::make_shared<type::FuncType>(std::move(type));
        }

        if (std::holds_alternative<value::BuiltinFunction>(value)) {
            return type::builtins::BuiltinFunction();
        }

        if (std::holds_alternative<value::Object>(value)) {
            // ! check if type isn't a class value | not sure what this comment is about
            return std::make_shared<type::LiteralType>(
                std::make_shared<value::ClassValue>(
                    *dynamic_cast<const type::LiteralType*>(get<value::Object>(value).first.get())->cls
                )
            );
        }

        if (std::holds_alternative<value::Pack>(value)) {
            auto values = std::ranges::fold_left(
                get<value::Pack>(value)->values,
                std::vector<type::TypePtr>{},
                [this] (auto acc, const auto& elt) {
                    acc.push_back(typeOf(elt));
                    return acc;
                }
            );


            if (values.empty()) return std::make_shared<type::VariadicType>(type::builtins::_());

            const bool same = std::ranges::all_of(values, [tp = values[0]] (const auto& t) { return *t == *tp; });

            // if (same) return std::make_shared<type::VariadicType>(std::move(values)[0]);
            if (same) return type::VariadicOf(std::move(values)[0]);

            // return std::make_shared<type::VariadicType>(type::builtins::Any());
            return type::VariadicOf(type::builtins::Any());
            // return same ? std::make_shared<type::VariadicType>(values[0]) : non_typed_pack;
        }

        if (std::holds_alternative<value::List>(value)) {
            auto values = std::ranges::fold_left(
                get<value::List>(value).elts->values,
                std::vector<type::TypePtr>{},
                [this] (auto acc, const auto& elt) {
                    acc.push_back(typeOf(elt));
                    return acc;
                }
            );


            // if (values.empty()) return std::make_shared<type::ListType>(type::builtins::_());
            if (values.empty()) return type::ListOf(type::builtins::_());

            const bool same = std::ranges::all_of(values, [tp = values[0]] (const auto& t) { return *t == *tp; });

            // if (same) return std::make_shared<type::ListType>(std::move(values)[0]);
            if (same) return type::ListOf(std::move(values)[0]);


            return type::ListOf(type::UnionOf(std::move(values)));
        }

        if (std::holds_alternative<value::Map>(value)) {
            // auto values = std::ranges::fold_left(
            //     get<value::MapValue>(value).items->map,
            //     std::vector<std::pair<type::TypePtr, type::TypePtr>>{},
            //     [this] (auto acc, const auto& elt) {
            //         acc.push_back({typeOf(elt.first), typeOf(elt.second), });

            //         return acc;
            //     }
            // );
            auto keys = std::ranges::fold_left(
                get<value::Map>(value).items->map,
                std::vector<type::TypePtr>{},
                [this] (auto acc, const auto& elt) {
                    acc.push_back({typeOf(elt.first)});
                    return acc;
                }
            );
            // checking this here instead of wasting time checking the values too
            // if (keys.empty()) return std::make_shared<type::MapType>(type::builtins::_(), type::builtins::_());
            if (keys.empty()) return std::make_shared<type::MapType>(type::builtins::Any(), type::builtins::Any());

            auto values = std::ranges::fold_left(
                get<value::Map>(value).items->map,
                std::vector<type::TypePtr>{},
                [this] (auto acc, const auto& elt) {
                    acc.push_back({typeOf(elt.second)});
                    return acc;
                }
            );

            // we know it ain't empty since we already check it up there


            const bool same_key = std::ranges::all_of(keys  , [tp = keys[0]  ] (const auto& t) { return *t== *tp; });
            const bool same_val = std::ranges::all_of(values, [tp = values[0]] (const auto& t) { return *t== *tp; });

            if (same_key and same_val)
                return std::make_shared<type::MapType>(std::move(keys)[0], std::move(values)[0]);
                // return type::MapOf(std::move(keys)[0], std::move(values)[0]);

            if (same_key)
                return type::MapOf(std::move(keys)[0], type::builtins::Any());
                // return type::MapOf(std::move(keys)[0], type::UnionOf(std::move(values)));

            if (same_val)
                return type::MapOf(type::builtins::Any(), std::move(values)[0]);
                // return type::MapOf(type::UnionOf(std::move(keys)), std::move(values)[0]);


            return type::MapOf(type::builtins::Any(), type::builtins::Any());
            // return type::MapOf(type::UnionOf(std::move(keys)), type::UnionOf(std::move(values)));
        }


        util::error("Unknown Type for value: " + stringify(value));
    }


    type::TypePtr declType(const expr::StringID& name) const {
        if (const auto var = getVar(name.ID, [&name = name.name] { return name; }); var) {
            return var->type;
        }

        util::error("");
    }


    // value::Value eval(expr::ExprPtr& expr) { return std::visit(*this, expr->variant()).value; }


    struct ScopeGuard {
        Visitor* v;


        template <std::same_as<value::Environment>... E>
        ScopeGuard(Visitor* t, const E&... es) noexcept : v{t} {
            v->scope();

            (addEnv(es), ...);
        }

        template <std::same_as<value::Environment>... E>
        ScopeGuard(Visitor* t, value::EnvTag tag, const E&... es) noexcept : v{t} {
            v->scope(tag);

            (addEnv(es), ...);
        }

        void addEnv(const value::Environment& e) {
            for (const auto& [ID, var] : e) {
                const auto& [name, value, type] = var;
                v->addVar(name.name, ID, value, type);
            }
        }

        void addOps(const Operators& ops) {
            for (const auto& [name, op] : ops)
                v->env.back()->op_env[name] = op->clone();
        }

        void addPrefixOps(const Operators& ops) {
            for (const auto& [name, op] : ops)
                v->env.back()->prefix_op_env[name] = op->clone();
        }


        ScopeGuard(const ScopeGuard&) = delete;
        ScopeGuard& operator=(const ScopeGuard&) = delete;

        ScopeGuard(ScopeGuard&& other) noexcept {
            v = other.v;
            other.v = nullptr;
        }

        ScopeGuard& operator=(ScopeGuard and other) noexcept {
            v = other.v;
            other.v = nullptr;
            return *this;
        }

        ~ScopeGuard() { if (v) v->unscope(); }
    };


    void scope(const value::EnvTag tag = value::EnvTag::NONE) {
        env.push_back(std::make_shared<value::Env>(value::Env{{}, {}, {}, tag}));
        deferred.emplace_back();
    }


    void unscope() {

        for (auto& [expr, env] : deferred.back() | std::views::reverse) {
            ScopeGuard sg{this, env->env};
            sg.addPrefixOps(env->prefix_op_env);
            sg.addOps(env->op_env);

            std::visit(*this, expr->variant());
        }

        deferred.pop_back();
        env.pop_back();
    }


    value::Value addVar(
        const std::string& name,
        const size_t ID,
        const value::ValuePtr& v,
        const type::TypePtr& t = type::builtins::Any(),
        NameSpace* space = nullptr
    ) {
        env.back()->env[ID] = {{name, space}, v, t};
        return *v;
    }

    // void addEnv(const value::Environment& e) {
    //     for (const auto& [key, var] : e) {
    //         const auto& [name, value, type] = var;

    //         env.back().first[key] = {name, value, type};
    //     }
    // }


    bool isRef(const size_t ID) const {
        for (const auto& e : std::views::reverse(env)) {
            if (e->env.contains(ID)) {
                const auto& [named_ref, _, __] = e->env.at(ID);
                return named_ref.isRef();
            }
        }


        return false;
    }


    std::optional<ValueType> dynamicLookup(const std::string& name) const {
        for (const auto& e : std::views::reverse(env)) {
            for (const auto& [_, val] : e->env)
            if (get<value::SpaceRef>(val).name == name) {
                const auto& [_, value_ptr, type_ptr] = val;
                return {{*value_ptr, type_ptr}};
            }
        }

        if (const auto var = checkMemberInThisObject(name); var) return *var;


        for (const auto& ns : std::views::reverse(current_space)) {
            for (const auto& [_, val] : ns->members)
            if (get<value::SpaceRef>(val).name == name) {
                const auto& [_, value_ptr, type_ptr] = val;
                return {{*value_ptr, type_ptr}};
            }
        }


        return {};
    }

    std::optional<ValueType> getVar(const ssize_t id, auto name) const {
        if (id == -1) return {};

        if (id == std::to_underlying(analysis::LexicalAnalysis::ReservedIDs::DYNAMIC))
            return dynamicLookup(name());

        for (const auto& e : std::views::reverse(env)) {
            if (e->env.contains(id)) {
                const auto& [_, value_ptr, type_ptr] = e->env.at(id);
                return {{*value_ptr, type_ptr}};
            }
        }


        if (const auto var = checkMemberInThisObject(id); var) return *var;


        for (const auto& ns : std::views::reverse(current_space)) {
            if (ns->members.contains(id)) {
                const auto& [_, value_ptr, type_ptr] = ns->members.at(id);
                return {{*value_ptr, type_ptr}};
            }
        }

        // if (env.contains(ID)) {
        //     const auto& [_, value, type] = env.at(ID);
        //     return {{*value, type}};
        // }

        return {};
    }

    bool changeVar(const size_t ID, const value::Value& v) {
        for (auto rev_it = env.rbegin(); rev_it != env.rend(); ++rev_it)
            if ((*rev_it)->env.contains(ID)) {
                // const auto& t = rev_it->first.at(ID);
                // (*rev_it).first[name] = {std::make_shared<value::Value>(v), t};
                // get<1>(rev_it->first.at(ID)) = std::make_shared<value::Value>(v);
                *get<1>((*rev_it)->env.at(ID)) = v;

                return true;
            }

        // if (env.contains(ID)) {
        //     const auto& [_, value, __] = env.at(ID);
        //     *value = v;
        //     return true;
        // }

        return false;
    }

    // std::optional<std::pair<value::Value, type::TypePtr>> globalLookup(const std::string& name) const {
    //     if (env[0].first.contains(name)) {
    //         const auto& [value_ptr, type_ptr] = env[0].first.at(name);
    //         return {{*value_ptr, type_ptr}};
    //     }

    //     return {};
    // }


    void removeVar(const size_t ID) {
        for(auto& e : std::views::reverse(env)) {
            if (e->env.contains(ID)) {
                e->env.erase(ID);
                return;
            }
        }

        // env.erase(ID);
    }


    static value::Environment envStackToEnvMap(const std::vector<std::pair<value::Environment, value::EnvTag>>& env) {
        value::Environment e;
        for(const auto& curr_env : env)
            for(const auto& [key, value] : curr_env.first)
                e[key] = value; // I want the recent values (higher in the stack) to be the ones captured
        return e;
    }



    static void printEnv(const value::Environment& e) noexcept {
        // const auto& e = envStackToEnvMap();

        for (const auto& [ID, v] : e) {
            const auto& [name, value, type] = v;

            if (name.isRef()) {
                std::println("[{}] {}::{}: {} = {}", ID, name.space->name, name.name, type->text(), stringify(*value));
            }
            else {
                std::println("[{}] {}: {} = {}", ID, name.name, type->text(), stringify(*value));
            }
        }
    }


    // static void printEnv(const std::vector<std::pair<value::Environment, EnvTag>>& env) noexcept {
    //     const auto& e = envStackToEnvMap(env);
    //     for (const auto& [ID, v] : e) {
    //         const auto& [name, value, type] = v;
    //         std::println("[{}] {}: {} = {}", ID, name.space->name, name.name, type->text(), stringify(*value));
    //     }
    // }
};


} // namespace interp
} // namespace pie
