#include "LexicalScoping.hxx"

#include "../Lex/Lexer.hxx"
#include "../Parser/Parser.hxx"
#include "../Utils/Exceptions.hxx"


inline namespace pie {
namespace analysis {

std::string stringify(const std::vector<std::string>& spaces) {
    if (spaces.size() == 1) return spaces[0];

    std::string s = spaces[0];
    for (const auto& space : spaces | std::views::drop(1))
        s += "::" + space;

    return s;
}


LexicalScoping::LexicalScoping(const size_t index) : variable_index(index) {
    env.push_back({{}, EnvTag::SCOPE});

    const auto builtins = {
        "Any",
        "Int",
        "Double",
        "String",
        "Bool",
        "Syntax",
        "Type",

        "__builtin_rand_int",

        "__builtin_print",
        "__builtin_concat", 
        "__builtin_print_env",
        "__builtin_panic",
        "__builtin_input_str",
        "__builtin_input_int",
        "__builtin_decltype",
        "__builtin_type",
        "__builtin_len",
        "__builtin_reset",
        "__builtin_eval",
        "__builtin_neg",
        "__builtin_abs",
        "__builtin_not",
        "__builtin_to_int",
        "__builtin_to_double",
        "__builtin_to_string",
        "__builtin_get",
        "__builtin_push",
        "__builtin_push",
        "__builtin_pop",
        "__builtin_pop_front",
        "__builtin_add",
        "__builtin_sub",
        "__builtin_mul",
        "__builtin_div",
        "__builtin_mod",
        "__builtin_pow",
        "__builtin_gt",
        "__builtin_geq",
        "__builtin_eq",
        "__builtin_leq",
        "__builtin_lt",
        "__builtin_and",
        "__builtin_or",  
        "__builtin_set",
        "__builtin_conditional",
        "__builtin_str_slice",
        "__builtin_str_split",

        // //* File IO
        "__builtin_open_file",
        "__builtin_read_word",
        "__builtin_read_line",
        "__builtin_read_file",
        "__builtin_close_file",

        //* FFI shit
        "__builtin_dlopen"          ,
        "__builtin_dlsym"           ,
        "__builtin_ffi_call"        ,
        "__builtin_ffi_type_void"   ,
        "__builtin_ffi_type_int"    ,
        "__builtin_ffi_type_float"  ,
        "__builtin_ffi_type_double" ,
        "__builtin_ffi_type_uint8"  ,
        "__builtin_ffi_type_sint8"  ,
        "__builtin_ffi_type_uint16" ,
        "__builtin_ffi_type_sint16" ,
        "__builtin_ffi_type_uint32" ,
        "__builtin_ffi_type_sint32" ,
        "__builtin_ffi_type_uint64" ,
        "__builtin_ffi_type_sint64" ,
        "__builtin_ffi_type_struct" ,
        "__builtin_ffi_type_pointer",
        "__builtin_ffi_type_cstring",
        "__builtin_ffi_type_complex",

        "__builtin_ptr_to_string",
    };


    for (const auto builtin : builtins)
        env[0].first.vars[builtin] = variable_index++;
}


void LexicalScoping::operator()(expr::Num *n) {
    if (auto id = findVar(n->stringify())) {
        n->ID = *id;
        return;
    }
}
void LexicalScoping::operator()(expr::Bool *b) {
    if (auto id = findVar(b->stringify())) {
        b->ID = *id;
        return;
    }
}
void LexicalScoping::operator()(expr::String *s) {
    if (auto id = findVar(s->stringify())) {
        s->ID = *id;
        return;
    }
}

void LexicalScoping::operator()(expr::Cascade *c) {
    if (auto id = findVar(c->stringify())) {
        c->ID = *id;
        return;
    }
}
void LexicalScoping::operator()(expr::Fix *f) {
    if (auto id = findVar(f->stringify())) {
        f->ID = *id;
        return;
    }

    // std::visit(*this, f->funcs[0]->variant());
    auto func = dynamic_cast<expr::Closure*>(f->funcs[0].get());
    // std::visit(*this, func->body->variant());
    std::visit(*this, func->variant());
}


void LexicalScoping::accessAssign(expr::Access *acc, expr::Assignment *ass) {
    if (const auto id = findVar(acc->var->stringify()); id) {
        acc->var->ID = *id;
        std::visit(*this, ass->rhs->variant());
    }
    else util::error<except::NameLookup>("Name `" + acc->var->stringify() + "` not found in assignment: " + ass->stringify());
}



void LexicalScoping::spaceAccessAssign(expr::SpaceAccess *acc, expr::Assignment *ass) {
    const auto space = findSpace(acc->spaces, acc->global);
    if (not space) util::error("Namespace `" + stringify(acc->spaces) + "` is expression `" + ass->stringify() + "` was not found!");

    // for (const auto& [var, id] : namespaces[fullName(space)])
    for (const auto& [var, id] : space->members) {
        if (var == acc->name.name) {
            acc->name.ID = id;
            return;
        }
    }

    std::visit(*this, ass->rhs->variant());
}


void LexicalScoping::operator()(expr::Assignment *ass) {
    if (auto *acc = dynamic_cast<expr::Access*>(ass->lhs.get()))
        return accessAssign(acc, ass);

    if (auto *acc = dynamic_cast<expr::SpaceAccess*>(ass->lhs.get()))
        return spaceAccessAssign(acc, ass);




    // this allows for recursion
    const bool is_closure = dynamic_cast<expr::Closure*>(ass->rhs.get());
    if (is_closure) {
        // if there is a type explicitly stated, then a new variable should be create no matter what
        if (not type::shouldReassign(ass->type)) {
            ass->lhs->ID = variable_index++;
            addVar(ass->lhs->stringify(), ass->lhs->ID);
        }
        else if (const auto id = findVar(ass->lhs->stringify()); id) {
            ass->lhs->ID = *id;
        }
        else {
            ass->lhs->ID = variable_index++;
            addVar(ass->lhs->stringify(), ass->lhs->ID);
        }
    }

    std::visit(*this, ass->rhs->variant());
    if (const auto id = findVar(ass->type->text()); id) {
        ass->type->ID = *id;
    }
    else std::visit(*this, expr::Type{ass->type}.variant());


    if (not is_closure) {
        // if there is a type explicitly stated, then a new variable should be create no matter what
        if (not type::shouldReassign(ass->type)) {
            ass->lhs->ID = variable_index++;
            addVar(ass->lhs->stringify(), ass->lhs->ID);
        }
        else if (const auto id = findVar(ass->lhs->stringify()); id) {
            ass->lhs->ID = *id;
        }
        else { // I could probably shorten this to only use an "if-else" and not "if-elif-else"
            ass->lhs->ID = variable_index++;
            addVar(ass->lhs->stringify(), ass->lhs->ID);
        }
    }
}

void LexicalScoping::operator()(expr::InferredAssignment *inf) {
    // Inferred Assignment always declares a new variable

    // allows for recursion
    const bool is_closure = dynamic_cast<expr::Closure*>(inf->rhs.get());
    if (is_closure) {
        inf->name.ID = variable_index++;
        addVar(inf->name.name, inf->name.ID);
    }

    std::visit(*this, inf->rhs->variant());

    if (not is_closure) {
        inf->name.ID = variable_index++;
        addVar(inf->name.name, inf->name.ID);
    }

}


void LexicalScoping::operator()(expr::Name *name) {
    if (const auto id = findVar(name->name)) {
        name->ID = *id;
        return;
    }

    util::error<except::NameLookup>("Name `" + name->name + " with ID [" + std::to_string(name->ID) + "] not found!");
}


void LexicalScoping::operator()(expr::Block *b) {
    if (auto id = findVar(b->stringify())) {
        b->ID = *id;
        return;
    }

    ScopeGuard sg{this};

    for (const auto& e : b->lines)
        std::visit(*this, e->variant());
}

void LexicalScoping::operator()(expr::Closure *c) {
    if (auto id = findVar(c->stringify())) {
        c->ID = *id;
        return;
    }

    ScopeGuard sg{this};

    for (const auto& [name, type] : std::views::zip(c->params, c->type.params)) {
        std::visit(*this, expr::Type{type}.variant());

        if (auto expr_type = type::isExpr(type)) {
            if (const auto id = findVar(type->text()); id)
                expr_type->t->ID = *id;
        }

        name.ID = variable_index++;
        addVar(name.name, name.ID);
    }

    std::visit(*this, expr::Type{c->type.ret}.variant());
    std::visit(*this, c->body->variant());
}

void LexicalScoping::operator()(expr::Call *call) {
    if (auto id = findVar(call->stringify())) {
        call->ID = *id;
        return;
    }

    std::visit(*this, call->func->variant());

    for (const auto& [_, named_arg] : call->named_args)
        std::visit(*this, named_arg->variant());


    for (const auto& arg : call->args)
        std::visit(*this, arg->variant());
}

void LexicalScoping::operator()(expr::List *list) {
    if (auto id = findVar(list->stringify())) {
        list->ID = *id;
        return;
    }

    for (const auto& e : list->elements)
        std::visit(*this, e->variant());
}

void LexicalScoping::operator()(expr::Map *map) {
    if (auto id = findVar(map->stringify())) {
        map->ID = *id;
        return;
    }

    for (const auto& [key, value] : map->items)
        std::visit(*this, key->variant()), std::visit(*this, value->variant());
}

void LexicalScoping::operator()(expr::Expansion *e) {
    if (auto id = findVar(e->stringify())) {
        e->ID = *id;
        return;
    }

    std::visit(*this, e->pack->variant());
}

void LexicalScoping::operator()(expr::UnaryFold *fold) {
    if (auto id = findVar(fold->stringify())) {
        fold->ID = *id;
        return;
    }

    std::visit(*this, fold->pack->variant());
}

void LexicalScoping::operator()(expr::SeparatedUnaryFold *fold) {
    if (auto id = findVar(fold->stringify())) {
        fold->ID = *id;
        return;
    }

    std::visit(*this, fold->lhs->variant());
    std::visit(*this, fold->rhs->variant());
}

void LexicalScoping::operator()(expr::BinaryFold *fold) {
    if (auto id = findVar(fold->stringify())) {
        fold->ID = *id;
        return;
    }

    std::visit(*this, fold->pack->variant());
    std::visit(*this, fold->init->variant());

    if (fold->sep) std::visit(*this, fold->sep->variant());
}


void LexicalScoping::operator()(expr::Class *cls) {
    if (auto id = findVar(cls->stringify())) {
        cls->ID = *id;
        return;
    }

    ScopeGuard sg{this};
    addVar("self", variable_index++);

     // needed for methods to access member variables
    for (auto& [name, _, __] : cls->fields) {
        name.ID = variable_index++;
        addVar(name.name, name.ID);
    }

    for (const auto& [_, type, expr] : cls->fields) {
        std::visit(*this, expr::Type{type}.variant());
        std::visit(*this, expr->variant());
    }

}

void LexicalScoping::operator()(expr::Union *onion) {
    if (auto id = findVar(onion->stringify())) {
        onion->ID = *id;
        return;
    }

    ScopeGuard sg{this};

    for (auto& type : onion->types) {
        if (const auto id = findVar(type->text()); id)
            type->ID = *id;

        else std::visit(*this, expr::Type{type}.variant());
    }
}


void LexicalScoping::checkPattern(expr::Match::Case::Pattern& pat) {
    if (std::holds_alternative<expr::Match::Case::Pattern::Single>(pat.pattern)) {
        auto& pattern = get<expr::Match::Case::Pattern::Single>(pat.pattern);

        if (not pattern.name.name.empty()) {
            pattern.name.ID = variable_index++;
            addVar(pattern.name.name, pattern.name.ID);
        }

        if (pattern.type)
            std::visit(*this, expr::Type{pattern.type}.variant());

        if (pattern.value)
            std::visit(*this, pattern.value->variant());

        // if (not pattern.name.empty()) addVar(pattern.name);
    }
    else { // holds expr::Match::Case::Pattern::Structure
        auto& [name, patterns] = get<expr::Match::Case::Pattern::Structure>(pat.pattern);

        if (const auto id = findVar(name.name); not id)
            util::error<except::NameLookup>("Name `" + name.name + "` not found!");
        else name.ID = *id;

        for (const auto& pat : patterns) checkPattern(*pat);
    }
}


void LexicalScoping::operator()(expr::Match *match) {
    if (auto id = findVar(match->stringify())) {
        match->ID = *id;
        return;
    }

    ScopeGuard sg{this};

    std::visit(*this, match->expr->variant());


    for (const auto& kase : match->cases) {
        ScopeGuard sg{this};

        checkPattern(*kase.pattern);

        if (kase.guard) std::visit(*this, kase.guard->variant());

        std::visit(*this, kase.body->variant());
    }
}


void LexicalScoping::operator()(expr::Loop *loop) {
    if (auto id = findVar(loop->stringify())) {
        loop->ID = *id;
        return;
    }

    ScopeGuard sg{this};

    if (loop->kind) std::visit(*this, loop->kind->variant());
    if (not loop->var.name.empty()) {
        loop->var.ID = variable_index++;
        addVar(loop->var.name, loop->var.ID);
    }

    const auto was_in_loop = in_loop;
    in_loop = true;
    std::visit(*this, loop->body->variant());
    in_loop = was_in_loop;

    if (loop->els) std::visit(*this, loop->els ->variant());
}


void LexicalScoping::operator()(expr::Break *br) {
    if (auto id = findVar(br->stringify())) {
        br->ID = *id;
        return;
    }

    if (not in_loop) util::error("Can't use `break` outside a loop: " + br->stringify());

    std::visit(*this, br->expr->variant());
}

void LexicalScoping::operator()(expr::Continue *cont) {
    if (auto id = findVar(cont->stringify())) {
        cont->ID = *id;
        return;
    }

    if (not in_loop) util::error("Can't use `continue` outside a loop: " + cont->stringify());

    if (cont->expr) std::visit(*this, cont->expr->variant());
}


void LexicalScoping::operator()(const expr::Access *acc) {
    std::visit(*this, acc->var->variant());

    // if (auto id = findVar(acc->var->stringify())) acc->var->ID = *id;
    // else util::error("Name `" + acc->var->stringify() + "` not found in access expression: " + acc->stringify());

    // can't check the accessee
    // since the type of the accessor is not know until runtime
    // so..we just allow it (which is fine since it doesn't fall under lexical scoping)
    // checkName(acc->name);
}


void LexicalScoping::operator()(expr::Import *import) {
    if (auto id = findVar(import->stringify())) {
        import->ID = *id;
        return;
    }


    const auto src = util::readFile(auto{import->path}.replace_extension(".pie").string());
    const Tokens tokens = lex::lex(src);
    if (tokens.empty()) util::error("Can't import an empty file!");

    parse::Parser p{std::move(tokens), import->path};

    auto exprs = p.parse();

    indeces.push_back(variable_index);
    LexicalScoping v{variable_index};

    for (const auto& expr : exprs)
        std::visit(v, std::move(expr)->variant());


    // for (const auto& [ns, space] : v.global_spaces) {
    //     for (const auto& [name, id] : space->members) {
    //         namespaces[ns][name] = id;
    //     }
    // }


    // addNamespaces(current_space.empty() ? global_spaces : current_space.back()->children, v.global_spaces);

    addNamespaces(env.back().first.spaces, v.env.front().first.spaces);


    variable_index = v.variable_index;
}


void LexicalScoping::operator()(expr::Namespace *n) {
    if (auto id = findVar(n->stringify())) {
        n->ID = *id;
        return;
    }

    // ScopeGuard sg{this};
    addSpace(n->name);

    for (const auto& expr : n->space)
        std::visit(*this, expr->variant());



    // const auto ns = getNamespaceAt(space_dir, global_spaces);

    for (auto& [name, id] : env.back().first.vars) {
        current_space.back()->members[std::move(name)] = id;
    }

    popSpace();
}


void LexicalScoping::operator()(expr::UseFix *use) {
    if (auto id = findVar(use->stringify())) {
        use->ID = *id;
        return;
    }

    // util::error();
}


void LexicalScoping::operator()(expr::UseSpace *use) {
    if (auto id = findVar(use->stringify())) {
        use->ID = *id;
        return;
    }


    auto ns = findSpace(use->spaces, use->global);
    if (not ns) util::error();

    // for (const auto& [var, id] : namespaces[fullName(space)])
    for (const auto& [var, id] : ns->members) {
        addVar(var, id);
        use->last_item_id = id;
    }


    for (const auto& [name, id] : ns->members) {
        env.back().first.vars[name] = id;
    }


    for (const auto& [name, space] : ns->children){
        if (not env.back().first.spaces.contains(name)) {
            env.back().first.spaces[name] = space;
        }
        else for (const auto& [member_name, member_id] : space->members) {
            env.back().first.spaces[name]->members[member_name] = member_id;
        }

        if (env.back().second == EnvTag::SPACE) {
            if (not current_space.back()->children.contains(name)) {
                current_space.back()->children[name] = space;
            }
            else for (const auto& [member_name, member_id] : space->members) {
                current_space.back()->children[name]->members[member_name] = member_id;
            }
        }
    }
}


void LexicalScoping::operator()(expr::Use *use) {
    if (auto id = findVar(use->stringify())) {
        use->ID = *id;
        return;
    }


    const auto space = findSpace(use->spaces, use->global);
    if (not space) util::error();

    // for (const auto& [var, id] : namespaces[fullName(space)]) {
    for (const auto& [var, id] : space->members) {
        if (var == use->name.name) {
            use->name.ID = id;
            addVar(var, id);
            return;
        }
    }

    util::error<except::NameLookup>("Name " + use->name.name + " not found in space " + stringify(use->spaces));
}


void LexicalScoping::operator()(expr::SpaceAccess *acc) {
    const auto space = findSpace(acc->spaces, acc->global);

    if (not space) util::error();

    // for (const auto& [var, id] : namespaces[fullName(space)]) {
    for (const auto& [var, id] : space->members) {
        if (var == acc->name.name) {
            acc->name.ID = id;
            return;
        }
    }

    util::error<except::NameLookup>("Name `" + acc->name.name + "` not found in space " + stringify(acc->spaces));
}


void LexicalScoping::operator()(expr::Grouping *group) {
    if (auto id = findVar(group->stringify())) {
        group->ID = *id;
        return;
    }

    std::visit(*this, group->expr->variant());
}


void LexicalScoping::operator()(expr::UnaryOp *up) {
    if (auto id = findVar(up->stringify())) {
        up->ID = *id;
        return;
    }

    std::visit(*this, up->expr->variant());
}


void LexicalScoping::operator()(expr::BinOp *bp) {
    if (auto id = findVar(bp->stringify())) {
        bp->ID = *id;
        return;
    }

    std::visit(*this, bp->lhs->variant());
    std::visit(*this, bp->rhs->variant());
}


void LexicalScoping::operator()(expr::PostOp   *pp) {
    if (auto id = findVar(pp->stringify())) {
        pp->ID = *id;
        return;
    }

    std::visit(*this, pp->expr->variant());
}


void LexicalScoping::operator()(expr::CircumOp *cp) {
    if (auto id = findVar(cp->stringify())) {
        cp->ID = *id;
        return;
    }

    std::visit(*this, cp->expr->variant());
}


void LexicalScoping::operator()(expr::OpCall *oc) {
    if (auto id = findVar(oc->stringify())) {
        oc->ID = *id;
        return;
    }

    for (const auto& expr : oc->exprs)
        std::visit(*this, expr->variant());
}


void LexicalScoping::visitType(const type::TypePtr& type) {
    if (auto expr_type = type::isExpr(type)) {
        // leave em empty for now...
        if (const auto n = dynamic_cast<const expr::Num*>(expr_type->t.get()));
        else if (const auto b = dynamic_cast<const expr::Bool*>(expr_type->t.get()));
        else if (const auto s = dynamic_cast<const expr::String*>(expr_type->t.get()));

        else if (const auto id = findVar(type->text()); id) {
            expr_type->ID = *id;
            expr_type->t->ID = *id;
        }
        else if (auto unio = dynamic_cast<expr::Union*>(expr_type->t.get())) {
            for (auto& type : unio->types) {
                if (const auto id = findVar(type->text()); id) type->ID = *id;
                else visitType(type);
            }
        }

        std::visit(*this, expr_type->t->variant());
        // else util::error<except::NameLookup>("Type `" + type->text() + "` not found!");
    }
    else if (const auto func = type::isFunction(type)) {

    }

    else if(const auto var = type::isList(type)) {
        visitType(var->type);
    }
    else if(const auto var = type::isMap(type)) {
        visitType(var->key_type);
        visitType(var->val_type);
    }
    else if(const auto var = type::isVariadic(type)) {
        visitType(var->type);
    }
};


void LexicalScoping::operator()(expr::Syntax *syn) {
    if (auto id = findVar(syn->stringify())) {
        syn->ID = *id;
        return;
    }

    std::visit(*this, syn->expr->variant());
}


void LexicalScoping::operator()(expr::Type *type) {
    if (auto id = findVar(type->stringify())) {
        type->ID = *id;
        type->type->ID = *id;
        return;
    }

    visitType(type->type);
}


void LexicalScoping::addSpace(const std::string& name) {
    std::shared_ptr<NameSpace> ns;

    if (env.back().second == EnvTag::SPACE) {
        if (current_space.back()->children.contains(name)) {
            ns = current_space.back()->children[name];
        }
        else {
            ns = current_space.back()->children[name] = env.back().first.spaces[name] = std::make_shared<NameSpace>(name);
        }
    }
    else {
        if (env.back().first.spaces.contains(name)) {
            ns = env.back().first.spaces[name];
        }
        else {
            ns = env.back().first.spaces[name] = std::make_shared<NameSpace>(name);
        }
    }


    current_space.push_back(ns.get());
    scope(ns);
}


void LexicalScoping::popSpace() {
    // if (env[env.size() - 2].second == EnvTag::SPACE) {
    //     for (auto& [name, space] : env.back().first.spaces) {

    //     }
    // }

    unscope();
    current_space.pop_back();
}



NameSpace* LexicalScoping::matchChain(const std::vector<std::string>& names, NameSpace *space) {
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


// ideally, should be called findSpaces!
NameSpace* LexicalScoping::findSpace(const std::vector<std::string>& names, const bool global_search_only) {
    if (not global_search_only) {
        for (const auto space : std::views::reverse(current_space)) {
            if (const auto s = matchChain(names, space)) return s;

            for (const auto& [_, child] : space->children)
                if (const auto s = matchChain(names, child.get())) return s;
        }
    }


    for (const auto& [_, ns] : env.front().first.spaces) {
        if (const auto s = matchChain(names, ns.get()))
            return s;
    }



    util::error<except::NameLookup>("Space `" + stringify(names) + "` not found!");
}

void LexicalScoping::addVar(std::string name, const size_t index) {
    env.back().first.vars[std::move(name)] = index;
    // if (space_dir.empty()) 
    // else {
    //     const auto space = getNamespaceAt(space_dir, global_spaces);
    //     namespaces[fullName(space)][std::move(name)] = index;
    // }
}


std::optional<size_t> LexicalScoping::findVarInSpace(const std::string& name) const {

    for (const auto space : std::views::reverse(current_space)) {

        if (space->members.contains(name))
            return space->members.at(name);

        // for (const auto& child : space->children) {
        //     const auto fullname = fullName(child.get());
        //     if (namespaces.at(fullname).contains(name))
        //         return namespaces.at(fullname).at(name);
        // }
    }

    return {};
}


void LexicalScoping::addNamespaces(
    std::unordered_map<std::string, std::shared_ptr<NameSpace>>& spaces,
    const std::unordered_map<std::string, std::shared_ptr<NameSpace>>& new_spaces
) {
    for (const auto& [new_space_name, new_space] : new_spaces) {
        if (spaces.contains(new_space_name)) {
            for (const auto& [name, id] : spaces[new_space_name]->members) {
                spaces[new_space_name]->members[name] = id;
            }

            addNamespaces(spaces[new_space_name]->children, new_space->children);
        }
        // in this case, just push the new space with all its children
        else {
            spaces[new_space_name] = new_space;
        }
    }


    if (env.back().second == EnvTag::SPACE) {
        for (const auto& [new_space_name, new_space] : new_spaces) {
            if (current_space.back()->children.contains(new_space_name)) {
                for (const auto& [name, id] : new_space->children[new_space_name]->members) {
                    current_space.back()->members[name] = id;
                }
            }
            else {
                current_space.back()->children[new_space_name] = new_space;
            }
        }
    }
}


[[nodiscard]] std::optional<size_t> LexicalScoping::findVar(const std::string& name) const {
    if (name.empty()) return {}; // no reason to search

    if (not current_space.empty()) {
        if (const auto id = findVarInSpace(name); id) return id;
    }


    // previously, the order of traversal didn't matter
    // since once the name was found, that was enough proof that it was lexically available
    // now we need to assign IDs, which means a reverse traversal is needed
    for (const auto& e : std::views::reverse(env))
        if (e.first.vars.contains(name)) return e.first.vars.at(name);


    return {};
}


void LexicalScoping::scope() {
    env.push_back({{}, EnvTag::SCOPE});
}


void LexicalScoping::scope(const std::shared_ptr<NameSpace>& space) {
    env.push_back({{}, EnvTag::SPACE});

    env.back().first.spaces[space->name] = space;
}


void LexicalScoping::unscope() {
    env.pop_back();
}

} // namespace analysis
} // namespace pie
