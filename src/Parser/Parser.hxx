#pragma once

#include <cstdio>
#include <print>
#include <filesystem>
#include <memory>
#include <source_location>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <algorithm>
#include <iterator>
#include <ranges>
#include <cassert>


#include "../Utils/Exceptions.hxx"
#include "../Lex/Token.hxx"
#include "../Lex/Lexer.hxx"
#include "../Expr/Expr.hxx"
#include "../Parser/Precedence.hxx"
#include "../Utils/utils.hxx"
#include "../Analysis/ExprContains.hxx"




inline namespace pie {

inline namespace parse {


struct NameSpace {
    std::string name;
    Operators prefix_ops;
    Operators ops;

    std::unordered_map<std::string, std::shared_ptr<NameSpace>> children;
};

struct Env {
    std::unordered_set<std::string> vars;
    Operators prefix_op_env;
    Operators op_env;

    std::unordered_map<std::string, std::shared_ptr<NameSpace>> spaces;
};



static std::string stringify(const std::vector<std::string>& spaces) {
    if (spaces.size() == 1) return spaces[0];

    std::string s = spaces[0];
    for (const auto& space : spaces | std::views::drop(1))
        s += "::" + space;

    return s;
}



class Parser {
    enum class Context {
        NONE,
        MATCH,
        MAP,
        CALL,
        PACK,
    };

    enum class EnvTag {
        SCOPE,
        SPACE,
    };

    const std::filesystem::path root;


    token::Tokens tokens;
    typename token::Tokens::iterator iterator_beginning;
    typename token::Tokens::iterator token_iterator;
    size_t tokens_size{};
    // deque instead of vector for pop_front
    std::deque<token::Token> red; // past tense of read lol



    std::vector<std::pair<Env, EnvTag>> env;


    std::unordered_map<std::string, std::shared_ptr<NameSpace>> global_spaces;
    std::vector<NameSpace*> current_space;

public:

    Parser(token::Tokens t, std::filesystem::path r = ".") noexcept
    : root{r.remove_filename()}         ,
      tokens{std::move(t)}              ,
      iterator_beginning{tokens.begin()},
      token_iterator{iterator_beginning},
      tokens_size{tokens.size()}        ,
      env(1)
    { }


    explicit Parser(std::filesystem::path r) noexcept
    : root{r.remove_filename()}, env(1)
    {}


    [[nodiscard]] bool atEnd(const size_t offset = 0) const noexcept {
        const auto distance = std::distance(iterator_beginning, token_iterator);
        return distance + offset >= tokens_size or std::next(token_iterator, offset)->kind == token::TokenKind::END;

        // // offsat?
        // const auto offsetted = std::next(token_iterator, offset);
        // return offsetted == tokens.end() or offsetted->kind == token::TokenKind::END;
    }


    void resetTokens(token::Tokens t) {
        tokens = std::move(t);
        iterator_beginning = tokens.begin();
        token_iterator = iterator_beginning;
        tokens_size = tokens.size();
        red.clear();
    }

    std::vector<expr::ExprPtr> parse() {
        std::vector<expr::ExprPtr> expressions;

        while (not atEnd()) {
            expressions.push_back(parseExpr());

            if (not match(token::TokenKind::SEMI)) {
                const auto t = lookAhead();
                if (t.kind == token::TokenKind::NAME) {
                    std::string msg = "Operator '" + t.text + "' not found!";

                    // most operators are 1 or 2 chars long
                    if (t.text.length() > 2) msg += " Did you, perhaps, forget a ';' on the previous line?";
                    util::error<except::OperatorError>(msg); //  + '\n' + expressions.back()->stringify()
                }
                util::expected(token::TokenKind::SEMI, t);
            }
        }


        return expressions;
    }


    template <bool PARSE_TYPE = true, Context CTX = Context::NONE>
    expr::ExprPtr parseExpr(const int precedence = 0) {

        expr::ExprPtr left = prefix<PARSE_TYPE, CTX>(consume());

        while (precedence < getPrecedence()) {
            if constexpr (not PARSE_TYPE or CTX == Context::MAP) if (check(token::TokenKind::COLON)) break;
            // // both context's need to parse comma separated lists
            // if constexpr (CTX == Context::CALL)
            //     if (check(token::TokenKind::COMMA)) break;

            left = infix<CTX>(std::move(left), consume());
        }

        return left;
    }


    template <bool PARSE_TYPE = true, Context CTX = Context::NONE>
    expr::ExprPtr prefix(token::Token token) {
        switch (token.kind) {
            using enum token::TokenKind;

            case FLOAT :
            case INT   : return std::make_shared<expr::Num   >(std::move(token).text);
            case BOOL  : return std::make_shared<expr::Bool  >(token.text == "true" ? true : false);
            case STRING: return std::make_shared<expr::String>(std::move(token).text);

            case NAME:
                if (prefixOpsContain(token.text)) return parsePrefixOperator(std::move(token));

                if constexpr (not PARSE_TYPE) return std::make_shared<expr::Name>(std::move(token).text);
                return name(std::move(token));

            case CLASS: return klass();
            case UNION: return onion();
            case MATCH: return match();
            case LOOP:  return loop ();

            case BREAK: 
                // if (check(SEMI)) return std::make_shared<expr::Break>();
                return std::make_shared<expr::Break>(parseExpr());

            case CONTINUE:
                // if (check(SEMI))
                return std::make_shared<expr::Continue>();
                // return std::make_shared<expr::Continue>(parseExpr());

            case IMPORT   : return import_directive(); // not calling the method "import" in case I update to C++ modules
            case NAMESPACE: return nameSpace();
            case USE      :
                if (match(NAMESPACE)) return useSpace();
                if (
                    check(PREFIX) or
                    check(INFIX ) or
                    check(SUFFIX) or
                    check(EXFIX ) or
                    check(MIXFIX)
                )
                    return useFix(consume().kind);

                return use();

            // global namespace
            case SCOPE_RESOLVE: {
                constexpr auto GLOBAL_ACCESS = true;
                return namespaceAccess<GLOBAL_ACCESS>(consume(NAME).text);
            }

            case COLON: return std::make_shared<expr::Type>(parseType());

            case MIXFIX:
            case PREFIX:
            case INFIX :
            case SUFFIX:
            case EXFIX :
                return fixOperator(std::move(token));

            // block (scope) or list literal or map literal
            case L_BRACE : return LBrace();

            // either a grouping or a closure - (or a closure type)
            case L_PAREN : return LParen<PARSE_TYPE, CTX>();

            case BACKTICK: return backticks(); 


            default:
                // log();
                util::error<except::SyntaxError>("Couldn't parse \"" + token.text + "\"!");
        }
    }


    template <Context CTX = Context::NONE>
    expr::ExprPtr infix(expr::ExprPtr left, token::Token token) {
        switch (token.kind) {
            using enum token::TokenKind;

            case NAME: return infixName(std::move(left), std::move(token));

            case DOT: {
                auto accessee = parseExpr(prec::HIGH_VALUE);

                // maybe this could change and i can allow object.1 + 2. :). Just a thought
                auto accessee_ptr = dynamic_cast<expr::Name*>(accessee.get());
                if (not accessee_ptr) util::error<except::SyntaxError>("Can only follow a '.' with a name: " + accessee->stringify());

                return std::make_shared<expr::Access>(std::move(left), std::move(accessee_ptr)->name);
            }


            case CASCADE: return cascade(std::move(left));

            case COLON: {
                auto type = parseType();
                if (not match(ASSIGN)) util::error();

                return std::make_shared<expr::Assignment>(
                    std::move(left),
                    std::move(type),
                    parseExpr(prec::ASSIGNMENT_VALUE - 1)
                );
            };


            case SCOPE_RESOLVE: {
                constexpr bool NOT_GLOBAL_ACCESS = false;

                auto accessee_ptr = dynamic_cast<expr::Name*>(left.get());
                if (not accessee_ptr) util::error<except::SyntaxError>("Scope resolution operator '::' applied on non-name: " + left->stringify());

                return namespaceAccess<NOT_GLOBAL_ACCESS>(accessee_ptr->name);
            }

            case WALRUS: {
                if (not dynamic_cast<expr::Name*>(left.get()))
                    util::error<except::SyntaxError>("Only proper names may appear on the LHS of the walrus operator `:=`: " + left->stringify());

                return std::make_shared<expr::InferredAssignment>(
                    std::move(left)->stringify(),
                    parseExpr(prec::ASSIGNMENT_VALUE - 1)
                );
            }


            case ASSIGN:
                if constexpr (CTX == Context::MATCH) return left;
                if constexpr (CTX == Context::PACK) return left;

                if (auto fix = analysis::exprContains<expr::Fix>(left)) {
                    // env.back().insert(fix->stringify());
                    env.back().first.vars.insert(fix->stringify());
                    unAddOp(fix);
                }

                if (auto s = expr::is<expr::Syntax>(left.get())) {
                    constexpr auto SYNTAX = true;
                    return std::make_shared<expr::Assignment>(
                        std::move(s)->expr,
                        type::builtins::_(),
                        parseExpr(prec::ASSIGNMENT_VALUE - 1),
                        SYNTAX
                    );
                }

                return std::make_shared<expr::Assignment>(
                    std::move(left),
                    type::builtins::_(),
                    parseExpr(prec::ASSIGNMENT_VALUE - 1)
                );


            case L_PAREN: return call(std::move(left));

            case ELLIPSIS:
                if (CTX == Context::CALL) return left; // in expansion
                [[fallthrough]];


            default: util::error<except::SyntaxError>("Couldn't parse \"" + token.text + "\"!!");
        }
    }


    void unAddOp(const expr::Fix* fix) {
        switch (fix->type()) {
            using enum token::TokenKind;

            case PREFIX:
                env.back().first.prefix_op_env.erase(fix->name);
                return;

            case INFIX :
            case SUFFIX:
                env.back().first.op_env.erase(fix->name);
                return;


            case EXFIX: { // name1 x name2
                auto exfix = dynamic_cast<const expr::Exfix*>(fix);
                if (not exfix) util::error();

                env.back().first.op_env.erase(exfix->name);
                env.back().first.op_env.erase(exfix->name2);
                return;
            }

            case MIXFIX: {
                auto op = dynamic_cast<const expr::Operator*>(fix);
                if (not op) util::error();

                env.back().first.op_env.erase(op->name);
                for (const auto& name : op->rest)
                    env.back().first.op_env.erase(name);
                return;
            }

            default: util::error();
        }
    }


    template <bool ALLOW_VARIADIC = true>
    type::TypePtr parseType() {
        using enum token::TokenKind;

        if (match(ELLIPSIS)) {
            if constexpr (not ALLOW_VARIADIC) util::error<except::SyntaxError>("Can't have a variadic of a variadic type!");

            if (check(COMMA) or check(R_PAREN) or check(ASSIGN))
                return std::make_shared<type::VariadicType>(type::builtins::Any());

            return std::make_shared<type::VariadicType>(parseType<false>());
        }

        // either a function type
        if (check(L_PAREN)) {
            if (check(R_PAREN, 1)) { // nullary function type
                consume(L_PAREN);
                consume(R_PAREN);
                consume(COLON);
                return std::make_shared<type::FuncType>(std::vector<type::TypePtr>{}, parseType());
            }

            const bool func_type = [this] {
                size_t i = 1; // skip the L_PAREN check above
                for (; /* not atEnd(i) and */ not check(R_PAREN , i); ++i) {
                    if (check(L_BRACE, i)) while (not check(R_BRACE, i)) ++i;
                    if (check(L_PAREN, i)) while (not check(R_PAREN, i)) ++i;

                    if (check(COMMA, i)) return true; // Comma Separated List can only appear in function arguments list
                }
                ++i;

                return check(COLON, i); // ( ... ):
            }();


            if (func_type) { // function type with more than one parameter
                consume(L_PAREN);

                type::FuncType type{{}, {}};
                type.params.push_back(parseType());
                bool seen_variadic = type::isVariadic(type.params.back());

                while(match(COMMA)) {
                    type.params.push_back(parseType());

                    if (type::isVariadic(type.params.back())) {
                        if (seen_variadic)
                            util::error<except::SyntaxError>("Variadic parameters can only appear once in parameter list!");
                        else seen_variadic = true;
                    }
                }

                consume(R_PAREN);
                consume(COLON);

                type.ret = parseType();
                return std::make_shared<type::FuncType>(std::move(type));
            }

            // just a grouping at this point, which means it's an expression
            return std::make_shared<type::ExprType>(parseExpr());
        }

        if (match(L_BRACE)) { // list or map type
            constexpr auto NO_VARIADICS = false;


            auto type1 = parseType<NO_VARIADICS>();
            if (match(R_BRACE)) return std::make_shared<type::ListType>(std::move(type1));

            consume(COLON);
            auto type2 = parseType<NO_VARIADICS>();
            consume(R_BRACE);

            return std::make_shared<type::MapType>(std::move(type1), std::move(type2));
        }


        if (check(NAME)) { // checking for builtin types..this is a quick hack I think
            if (match("Int"   )) return type::builtins::Int   ();
            if (match("Double")) return type::builtins::Double();
            if (match("Bool"  )) return type::builtins::Bool  ();
            if (match("String")) return type::builtins::String();
            if (match("Any"   )) return type::builtins::Any   ();
            if (match("Syntax")) return type::builtins::Syntax();
            if (match("Type"  )) return type::builtins::Type  ();
        }

        // or an expression
        return std::make_shared<type::ExprType>(parseExpr<false>(prec::ASSIGNMENT_VALUE));
    }


    expr::ExprPtr cascade(expr::ExprPtr expr) {
        using enum token::TokenKind;

        std::vector<expr::ExprPtr> cascaders;

        do {
            // cascaders.push_back(parseExpr(prec::CASCADE_VALUE));

            auto cascader = parseExpr(prec::CASCADE_VALUE);

            if (match(ASSIGN))
                cascader = std::make_shared<expr::Assignment>(
                    std::move(cascader), type::builtins::Any(), parseExpr(prec::CASCADE_VALUE)
                );

            cascaders.push_back(std::move(cascader));

        } while (match(CASCADE));

        auto name = std::make_shared<expr::Name>("__tmp");
        std::vector<expr::ExprPtr> cas = {
            std::make_shared<expr::Assignment>(
                name, type::builtins::Any(), std::move(expr)
            )
        };

        for (auto& cascader : cascaders) {
            if (auto n = dynamic_cast<expr::Name*>(cascader.get())) {
                cas.push_back(std::make_shared<expr::Access>(name, n->name));
            }
            else if (auto c = dynamic_cast<expr::Call*>(cascader.get())){
                if (not dynamic_cast<expr::Name*>(c->func.get()))
                    util::error<except::SyntaxError>("Can only Cascade Access a name: " + c->stringify());

                c->func = std::make_shared<expr::Access>(name, c->func->stringify());
                cas.push_back(std::move(cascader));
            }
            else if (auto a = dynamic_cast<expr::Assignment*>(cascader.get())) {
                if (not dynamic_cast<expr::Name*>(a->lhs.get()))
                    util::error<except::SyntaxError>("Can only Cascade Access a name: " + a->lhs->stringify());

                a->lhs = std::make_shared<expr::Access>(name, a->lhs->stringify());
                cas.push_back(std::move(cascader));
            }
            else util::error<except::SyntaxError>("Cannot Cascade Access a non-name: " + cascader->stringify());
        }

        cas.push_back(name);

        return std::make_shared<expr::Block>(std::move(cas));
    }


    expr::Match::Case::PatternPtr parseMatchPattern() {
        using enum token::TokenKind;
        using Pattern   = expr::Match::Case::Pattern;
        using Single    = expr::Match::Case::Pattern::Single;
        using Patterns  = expr::Match::Case::Pattern::Patterns;

        bool has_name{}, has_type{}, has_valu{};
        bool is_name_expr{};

        // std::string name;
        // if (check(NAME)) {
        //     name = consume(NAME).text;
        //     has_name = true;
        // }


        expr::ExprPtr name;
        if (check(NAME) and check(SCOPE_RESOLVE, 1)) {
            constexpr auto NOT_GLOBAL_ACCESS = false;
            auto text = consume(NAME).text;
            consume(SCOPE_RESOLVE);
            name = namespaceAccess<NOT_GLOBAL_ACCESS>(std::move(text));
            has_name = true;
        }
        else if (check(NAME)) { // just a name
            name = std::make_shared<expr::Name>(consume(NAME).text);
            has_name = true;
            is_name_expr = true;
        }
        else if (match(SCOPE_RESOLVE)) {
            constexpr auto GLOBAL_ACCESS = true;
            name = namespaceAccess<GLOBAL_ACCESS>(consume(NAME).text);
            has_name = true;
        }


        // base case
        if (not match(L_PAREN)) {
            // trying to write code that avoids move

            if (has_name and not is_name_expr)
                util::error<except::SyntaxError>("Cannot introduce a qualified name in a pattern: " + name->stringify());

            auto type = type::builtins::_();
            if (match(COLON)) {
                type = parseType();
                has_type = true;
            }

            expr::ExprPtr value;
            if (match(ASSIGN)) {
                value = parseExpr();
                has_valu = true;
            }

            if (not (has_name or has_type or has_valu))
                util::error<except::SyntaxError>("Match expression case doesn't contain a pattern!");

            return std::make_unique<Pattern>(
                Single{
                    {name ? name->stringify() : ""},
                    std::move(type),
                    std::move(value),
                }
            );
        }


        Patterns patterns{};
        if (match(R_PAREN)) return std::make_unique<Pattern>(std::move(name), std::move(patterns));

        do patterns.push_back(parseMatchPattern()); while (match(COMMA));


        consume(R_PAREN);
        return std::make_unique<Pattern>(std::move(name), std::move(patterns));
    }


    expr::ExprPtr match() {
        using enum token::TokenKind;

        auto expr = parseExpr();

        consume(L_BRACE);

        std::vector<expr::Match::Case> cases;
        size_t so_far{};

        do {
            constexpr auto OR = "|";
            constexpr auto IF = "&";
            constexpr auto EMPTY_COND = nullptr;
            constexpr auto EMPTY_BODY = nullptr;

            std::vector<std::unique_ptr<expr::Match::Case::Pattern>> patterns;
            std::vector<expr::ExprPtr> guards;

            do {
                cases.push_back({
                    parseMatchPattern(),
                    match(IF) ? parseExpr() : EMPTY_COND,
                    EMPTY_BODY
                });

            } while (match(OR));


            consume(FAT_ARROW);

            auto body = parseExpr();

            for (auto& kase : cases | std::views::drop(so_far)) {
                kase.body = body; ++so_far;
            }

            consume(SEMI);
        }
        while (not match(R_BRACE));

        return std::make_shared<expr::Match>(std::move(expr), std::move(cases));
    }


    expr::ExprPtr klass() {
        using enum token::TokenKind;

        consume(L_BRACE);

        std::vector<std::tuple<expr::Name, type::TypePtr, expr::ExprPtr>> fields;

        while (not match(R_BRACE)) {
            auto expr = parseExpr();
            consume(SEMI);

            auto ass = dynamic_cast<expr::Assignment*>(expr.get());
            if (not ass) util::error<except::SyntaxError>("Can only have assignments in class definition!");

            // const auto& n = dynamic_cast<const expr::Name*>(ass->lhs.get());
            // if (not n) error("Can only assign to names in class definition!");
            // auto name = *n; // copy so I can modify
;
            // Can't reassign variables in a class definition
            // This just means the type was not annotated. Default to "Any"
            // if (type::shouldReassign(type)) type = type::builtins::Any();


            // if (type::shouldReassign(ass->type))
            //     fields.push_back({expr::Name{ass->lhs->stringify()}, type::builtins::Any(), std::move(ass)->rhs});
            // else
                fields.push_back({expr::Name{ass->lhs->stringify()}, std::move(ass)->type , std::move(ass)->rhs});
        }

        return std::make_shared<expr::Class>(std::move(fields));
    }


    expr::ExprPtr onion() {
        using enum token::TokenKind;

        consume(L_BRACE);

        std::vector<type::TypePtr> types;

        // empty union
        if (match(R_BRACE)) [[unlikely]] return std::make_shared<expr::Union>(std::move(types));

        types.push_back(parseType());
        consume(SEMI);

        while (not match(R_BRACE)) {
            types.push_back(parseType());
            consume(SEMI);
        }

        return std::make_shared<expr::Union>(std::move(types));
    }



    void addNamespaces(
        std::unordered_map<std::string, std::shared_ptr<NameSpace>>& spaces,
        const std::unordered_map<std::string, std::shared_ptr<NameSpace>>& new_spaces
    ) {
        for (const auto& [new_space_name, new_space] : new_spaces) {
            if (spaces.contains(new_space_name)) {
                for (const auto& [name, op] : spaces[new_space_name]->prefix_ops) {
                    spaces[new_space_name]->prefix_ops[name] = op;
                }

                for (const auto& [name, op] : spaces[new_space_name]->ops) {
                    spaces[new_space_name]->ops[name] = op;
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
                    for (const auto& [name, id] : new_space->children[new_space_name]->prefix_ops) {
                        current_space.back()->prefix_ops[name] = id;
                    }

                    for (const auto& [name, id] : new_space->children[new_space_name]->ops) {
                        current_space.back()->ops[name] = id;
                    }
                }
                else {
                    current_space.back()->children[new_space_name] = new_space;
                }
            }
        }
    }


    expr::ExprPtr import_directive() {
        using enum token::TokenKind;

        auto fname = consume(NAME).text;

        // Zen of Pie!
        if (fname == "self") return std::make_shared<expr::Import>(std::move(fname));


        fname += ".pie";
        // path.append(consume(NAME).text);
        std::filesystem::path path = util::getPiePath(); // root;
        if (std::filesystem::exists(path / "std" / fname)) {
            path.append("std").append(std::move(fname));
        }
        else {
            path = std::filesystem::canonical(root / std::move(fname));
        }

        const auto src = util::readFile(path);
        const token::Tokens tokens = lex::lex(src);
        if (tokens.empty()) util::error("Can't import an empty file!");

        Parser p{std::move(tokens), path};
        p.parse();

        addNamespaces(global_spaces, p.global_spaces);


        // path.replace_extension(".pie");
        return std::make_shared<expr::Import>(std::move(path));
    }



    expr::ExprPtr nameSpace() {
        using enum token::TokenKind;

        std::string name = consume(NAME).text;

        enterNamespace(name);

        consume(L_BRACE);

        std::vector<expr::ExprPtr> space;

        while (not match(R_BRACE)) {
            space.push_back(parseExpr());
            consume(SEMI);
        }


        current_space.back()->prefix_ops = std::move(env.back()).first.prefix_op_env;
        current_space.back()->       ops = std::move(env.back()).first.       op_env;


        exitNamespace();

        return std::make_shared<expr::Namespace>(std::move(name), std::move(space));
    }


    template <bool GLOBAL_ACCESS>
    expr::ExprPtr namespaceAccess(std::string first) {
        using enum token::TokenKind;

        std::vector<std::string> spaces;
        spaces.push_back(std::move(first)); // not initialized with std::init_list bc it can't be moved from

        // non-global access have already parsed the 1st SCOPE_RESOLVE
        if constexpr (not GLOBAL_ACCESS) spaces.push_back(consume(NAME).text);

        while (match(SCOPE_RESOLVE)) spaces.push_back(consume(NAME).text);

        std::string name = std::move(spaces).back();
        spaces.pop_back();

        return std::make_shared<expr::SpaceAccess>(GLOBAL_ACCESS, std::move(spaces), std::move(name));
    }


    expr::ExprPtr use() {
        using enum token::TokenKind;

        const bool global_access = match(SCOPE_RESOLVE);

        std::vector<std::string> spaces = {consume(NAME).text, };

        if (not match(SCOPE_RESOLVE))
            util::error<except::SyntaxError>("single name after `use` directive not allowed: `use " + spaces[0] + '`');


        bool pull_ops = true;
        while (pull_ops and check(NAME)) {
            spaces.push_back(consume(NAME).text);
            pull_ops = match(SCOPE_RESOLVE);
        }


        if (pull_ops) {
            const auto space = findSpace(spaces, global_access);

            for (const auto& [op_name, op] : space->ops) {
                env.back().first.op_env[op_name] = op->clone();
            }

            for (const auto& [op_name, op] : space->prefix_ops) {
                env.back().first.prefix_op_env[op_name] = op->clone();
            }


            return std::make_shared<expr::UseFix>(global_access, std::move(spaces));
        }


        std::string name = std::move(spaces).back();
        spaces.pop_back();

        return std::make_shared<expr::Use>(global_access, std::move(spaces), std::move(name));
    }


    expr::ExprPtr useSpace() {
        using enum token::TokenKind;
        const bool global_access = match(SCOPE_RESOLVE);
        bool pull_ops;

        std::vector<std::string> spaces;
        do {
            spaces.push_back(consume(NAME).text);
            pull_ops = match(SCOPE_RESOLVE);
        } while (pull_ops and check(NAME));


        return std::make_shared<expr::UseSpace>(global_access, std::move(spaces), pull_ops);
    }


    // use infix a; .: error
    expr::ExprPtr useFix(const token::TokenKind kind) {
        using enum token::TokenKind;
        const bool global_access = match(SCOPE_RESOLVE);

        std::vector<std::string> spaces = {consume(NAME).text, };

        if (not match(SCOPE_RESOLVE))
            util::error<except::SyntaxError>("single name after `use` directive not allowed: `use " + spaces[0] + '`');


        bool pull_ops = true;
        while (pull_ops and check(NAME)) {
            spaces.push_back(consume(NAME).text);
            pull_ops = match(SCOPE_RESOLVE);
        }


        std::string name;
        if (not pull_ops) {
            name = std::move(spaces).back();
            spaces.pop_back();
        }

        const auto& ns = findSpace(spaces);

        switch (kind) {
            case PREFIX:
                for (const auto& [op_name, op] : ns->prefix_ops)
                    if (
                        kind == PREFIX and
                        (name.empty() or op_name == name)
                    )
                        env.back().first.prefix_op_env[op_name] = op;
                break;

            case INFIX:
                for (const auto& [op_name, op] : ns->ops)
                    if (
                        kind == INFIX and
                        (name.empty() or op_name == name)
                    )
                        env.back().first.op_env[op_name] = op;
            break;

            case SUFFIX:
                for (const auto& [op_name, op] : ns->ops)
                    if (
                        kind == SUFFIX and
                        (name.empty() or op_name == name)
                    )
                        env.back().first.op_env[op_name] = op;
                break;

            case EXFIX:
                for (const auto& [op_name, op] : ns->prefix_ops)
                    if (
                        kind == EXFIX and
                        (name.empty() or op_name == name)
                    ) {
                        auto exfix = dynamic_cast<const expr::Exfix*>(op.get());

                        env.back().first.prefix_op_env[exfix->name ] = op;
                        env.back().first.op_env       [exfix->name2] = op;
                    }
                break;

            case MIXFIX:
                for (const auto& [op_name, op] : ns->prefix_ops)
                    if (
                        kind == MIXFIX and
                        (name.empty() or op_name == name)
                    ) {
                        auto mixfix = dynamic_cast<const expr::Operator*>(op.get());

                        env.back().first.prefix_op_env[mixfix->name] = op;
                        for (const auto& sub_name : mixfix->rest) {
                            env.back().first.op_env[sub_name] = op;
                        }
                    }

                for (const auto& [op_name, op] : ns->ops)
                    if (
                        kind == MIXFIX and
                        (name.empty() or op_name == name)
                    ) {
                        auto mixfix = dynamic_cast<const expr::Operator*>(op.get());

                        if (op->isPrefix())
                            env.back().first.prefix_op_env[mixfix->name] = op;
                        else
                            env.back().first.op_env       [mixfix->name] = op;

                        for (const auto& sub_name : mixfix->rest) {
                            env.back().first.op_env[sub_name] = op;
                        }
                    }
                break;

            case NONE:
                // name will always be empty if no filter is specified!
                for (const auto& [op_name, op] : ns->prefix_ops)
                    env.back().first.prefix_op_env[op_name] = op;

                for (const auto& [op_name, op] : ns->ops)
                    env.back().first.op_env[op_name] = op;

                break;

            default: util::error();
        }


        return std::make_shared<expr::UseFix>(global_access, std::move(spaces), kind, std::move(name));
    }


    // '(' already consumed
    std::vector<expr::ExprPtr> parseCommaList() {
        std::vector<expr::ExprPtr> exprs;

        if (match(token::TokenKind::R_PAREN)) return exprs;

        do exprs.push_back(parseExpr()); while(match(token::TokenKind::COMMA));

        consume(token::TokenKind::R_PAREN);

        return exprs;
    }

// case 1: (... + args)                 right  unary
// case 2: (... + args + init)          right binary

// case 3: (args + ...)                 left   unary
// case 4: (args + ... +  sep )         left   unary separated
// case 5: (sep  + ... +  args)         right  unary separated
// case 6: (sep  + ... +  args + init)  right binary separated

// case 7: (init + args + ...)          left binary
// case 8: (init + args + ... + sep)    left binary separated

    // left paren already consumed
    expr::ExprPtr parseFoldExpr() {
        using enum token::TokenKind;

        // right folds (unseparated)
        if (match(ELLIPSIS)) return foldCase1And2();

        constexpr auto l2r = true;
        constexpr auto r2l = false;

        auto pack = parseExpr(prec::HIGH_VALUE);


        std::string op = consume().text;
        if (not opsContain(op)         ) util::error<except::OperatorError>("Folding over unknown ""operator: " + op);
        if (findOp(op)->type() != INFIX) util::error<except::OperatorError>("Folding over non-infix operator: " + op);


        if (match(ELLIPSIS)) {
            // left unary fold (unseparated) | case 3
            if (match(R_PAREN))  return std::make_shared<expr::UnaryFold>(std::move(pack), std::move(op), l2r);

            consume(op);

            auto rhs = parseExpr(prec::HIGH_VALUE);

            // separated unary  | cases 4 and 5
            if (match(R_PAREN)) return std::make_shared<expr::SeparatedUnaryFold>(std::move(pack), std::move(rhs), std::move(op));

            consume(op);

            // right binary separated fold | case 6
            auto separator = std::move(pack);
            pack = std::move(rhs);
            auto init = parseExpr(prec::HIGH_VALUE);

            consume(R_PAREN);
            return std::make_shared<expr::BinaryFold>(std::move(pack), std::move(init), std::move(op), r2l, std::move(separator));
        }


        // binary fold
        // pack was in fact not a pack, but init
        auto init = std::move(pack);
        pack = parseExpr(prec::HIGH_VALUE);

        consume(op);

        consume(ELLIPSIS);

        expr::ExprPtr seperator{};

        // populating the sep for case 8
        if (match(op)) seperator = parseExpr(prec::HIGH_VALUE);

        consume(R_PAREN);

        // case 7 and 8
        return std::make_shared<expr::BinaryFold>(std::move(pack), std::move(init), std::move(op), l2r, std::move(seperator));
    }


    expr::ExprPtr foldCase1And2() {
        using enum token::TokenKind;

        constexpr auto is_left_to_right = false;

        std::string op = consume().text;
        if (not opsContain(op)         ) util::error<except::OperatorError>("Folding over unknown operator: " + op);
        if (findOp(op)->type() != INFIX) util::error<except::OperatorError>("Folding over non-infix operator: " + op);

        auto pack = parseExpr(prec::HIGH_VALUE);

        // unary fold | case 1
        if (match(R_PAREN)) return std::make_shared<expr::UnaryFold>(std::move(pack), std::move(op), is_left_to_right);

        // binary fold | case 2
        consume(op);
        auto init = parseExpr(prec::HIGH_VALUE);
        consume(R_PAREN);

        return std::make_shared<expr::BinaryFold>(std::move(pack), std::move(init), std::move(op), is_left_to_right);
    }


    expr::ExprPtr loop() {
        using enum token::TokenKind;


        expr::Unpackment::PatternPtr loop_var;

        const bool has_var = [this] {
            using enum token::TokenKind;

            for (size_t i{}; /* not atEnd(i) */; ++i) {
                if (check(COLON  , i)) return true;
                if (check(SEMI   , i)) return false;
                if (check(R_BRACE, i)) return false;
                if (check(R_PAREN, i)) return false;


                for (ssize_t balance = check(L_BRACE, i); balance; ) {
                    balance += check(L_BRACE, ++i);
                    balance -= check(R_BRACE, i);
                }

                for (ssize_t balance = check(L_PAREN, i); balance; ) {
                    balance += check(L_PAREN, ++i);
                    balance -= check(R_PAREN, i);
                }
            }
        }();

        // indicates a loop variable
        if (has_var) {
            loop_var = parseUnpackmentPattern();
            consume(COLON);
        }


        // non-expr patterns MUST have loop kind to destructure!
        if (loop_var and not dynamic_cast<expr::Unpackment::Expr*>(loop_var.get())) {
            auto kind = parseExpr();
            auto body = parseExpr();

            return std::make_shared<expr::Loop>(
                std::move(body    ),
                std::move(loop_var),
                std::move(kind    ),
                match(FAT_ARROW) ? parseExpr() : nullptr
            );
        }



        auto kind_or_body = parseExpr();

        if (match(FAT_ARROW)) {
            // `kind_or_body` was actually the body, but this time with an else
            return std::make_shared<expr::Loop>(
                std::move(kind_or_body),
                std::move(loop_var),
                nullptr,
                parseExpr()
            );
        }


        auto snapshot = checkpoint();
        try {
            // test to see if there is one more expression (for the body)
            return std::make_shared<expr::Loop>(
                parseExpr(),
                std::move(loop_var),
                std::move(kind_or_body)
            );
        }
        catch (const std::runtime_error     &) { }
        catch (const except::SyntaxError    &) { }
        // catch (const except::UnexpectedToken&) { }

        restore(std::move(snapshot));
        // no more expression. `kind_or_body` was itself the body
        return std::make_shared<expr::Loop>(
            std::move(kind_or_body),
            std::move(loop_var),
            nullptr,
            match(FAT_ARROW) ? parseExpr() : nullptr
        );

        // auto kind = match(FAT_ARROW) ? nullptr : parseExpr();

        // if (kind) consume(FAT_ARROW);

        // auto var_or_body = parseExpr();

        // if (match(FAT_ARROW))
        //     return std::make_shared<expr::Loop>(std::move(var_or_body), "", std::move(kind), parseExpr());

        // if (check(SEMI))
        //     return std::make_shared<expr::Loop>(std::move(var_or_body), "", std::move(kind));


        // auto& var = var_or_body;
        // auto body = parseExpr();

        // if (match(FAT_ARROW))
        //     return std::make_shared<expr::Loop>(std::move(body), std::move(var)->stringify(), std::move(kind), parseExpr());

        // return std::make_shared<expr::Loop>(std::move(body), std::move(var)->stringify(), std::move(kind));
    }



    expr::ExprPtr closure() {
        using enum token::TokenKind;

        std::vector<expr::Closure::Param> params;
        std::vector<type::TypePtr> params_types;

        if (not match(R_PAREN)) {
            do {
                constexpr auto DONT_PARSE_TYPE = false;
                auto param = parseExpr<DONT_PARSE_TYPE>();

                if (auto s = expr::is<expr::Syntax>(param.get())) {
                    params.push_back({s->expr->stringify(), -1, true});
                    params_types.push_back(type::builtins::_());
                }
                else {
                    params.push_back({param->stringify()});

                    if (match(COLON))
                        params_types.push_back(parseType());
                    else 
                        params_types.push_back(type::builtins::_()); // not `Any`, but `_` in case `Any` was assigned to
                }
            }
            while (match(COMMA));

            consume(R_PAREN);
        }


        for (bool found{}; auto&& type : params_types) {
            if (type::isVariadic(type)) {
                if  (found) util::error<except::SyntaxError>("Variadic parameters can only appear once in parameter list!");
                else found = true;
            }
        }


        type::TypePtr return_type = match(COLON) ? parseType() : type::builtins::_();

        consume(FAT_ARROW);

        return std::make_shared<expr::Closure>(
            std::move(params), closureBody(), type::FuncType{std::move(params_types), std::move(return_type)}
        );
    }


    expr::ExprPtr closureBody() {
        scope();
        expr::ExprPtr body;
        if (match(token::TokenKind::L_BRACE)) {
            if (isScope()) body = handleScope();
            else {
                unconsume();
                body = parseExpr();
            }
        }
        else body = parseExpr();
        unscope();

        return body;
    }


    expr::ExprPtr call(expr::ExprPtr left) {
        using enum token::TokenKind;

        std::unordered_map<std::string, expr::ExprPtr> named_args;
        std::vector<expr::ExprPtr> args;

        if (not match(R_PAREN)) { // while not closing the paren for the call

            do {
                constexpr auto PARSE_TYPE = true;
                auto arg = parseExpr<PARSE_TYPE, Context::CALL>();

                if (auto ass = dynamic_cast<expr::Assignment*>(arg.get())) {
                    if (match(ELLIPSIS)) util::error<except::SyntaxError>("Cannot expand pack in named argument: " + ass->stringify());

                    if (not type::shouldReassign(ass->type)) util::error<except::SyntaxError>("Can't have type annotation for named arguments: " + ass->stringify());

                    const auto name = ass->lhs->stringify();
                    if (std::ranges::find_if(named_args, [&name] (auto&& a) { return a.first == name; }) != named_args.end())
                        util::error("Named parameter '" + name + "' passed more than once: " + ass->stringify());

                    named_args[std::move(name)] = std::move(ass)->rhs;
                }

                else if (match(ELLIPSIS)) {
                    arg = std::make_shared<expr::Expansion>(std::move(arg));

                    while(match(ELLIPSIS)) // allows back to back expansions (args... ...);
                        arg = std::make_shared<expr::Expansion>(std::move(arg));


                    args.emplace_back(std::move(arg));
                }
                else args.emplace_back(std::move(arg));
            }
            while (match(COMMA));

            consume(R_PAREN);
        }

        return std::make_shared<expr::Call>(std::move(left), std::move(named_args), std::move(args));
    }


    expr::ExprPtr handleScope() {
        using enum token::TokenKind;

        scope();

        std::vector<expr::ExprPtr> exprs;
        while(not match(R_BRACE)) {
            exprs.push_back(parseExpr());
            consume(SEMI);
        }

        unscope();

        return std::make_shared<expr::Block>(std::move(exprs));
    }


    expr::ExprPtr list() {
        using enum token::TokenKind;

        std::vector<expr::ExprPtr> exprs = { parseExpr(), };

        // the `and` check allows for trailing commas..I hope
        while (match(COMMA) /* and not check(R_BRACE) */ ) exprs.push_back(parseExpr()); 

        consume(R_BRACE);

        return std::make_shared<expr::List>(std::move(exprs));
    }


    expr::ExprPtr map() {
        using enum token::TokenKind;

        auto key = parseExpr<false, Context::MAP>();
        consume(COLON);
        std::vector<std::pair<expr::ExprPtr, expr::ExprPtr>> exprs = { {std::move(key), parseExpr(), }, };
        // std::unordered_map<expr::ExprPtr, expr::ExprPtr> exprs = { {parseExpr<false>(), parseExpr(), }, };

        while (match(COMMA)) {
            key = parseExpr<false, Context::MAP>();
            consume(COLON);
            exprs.push_back({ std::move(key), parseExpr(), });

            // auto key = parseExpr<false>();
            // exprs[std::move(key)] = parseExpr();
            // exprs.insert_or_assign(std::move(key), parseExpr());?
        }

        consume(R_BRACE);

        return std::make_shared<expr::Map>(std::move(exprs));
    }

    expr::ExprPtr comprehension() {
        using enum token::TokenKind;

        consume(LOOP);

        expr::Unpackment::PatternPtr loop_var;

        const bool has_var = [this] {
            using enum token::TokenKind;

            for (size_t i{}; /* not atEnd(i) */; ++i) {
                if (check(COLON  , i)) return true;
                if (check(SEMI   , i)) return false;
                if (check(R_BRACE, i)) return false;
                if (check(R_PAREN, i)) return false;


                for (ssize_t balance = check(L_BRACE, i); balance; ) {
                    balance += check(L_BRACE, ++i);
                    balance -= check(R_BRACE, i);
                }

                for (ssize_t balance = check(L_PAREN, i); balance; ) {
                    balance += check(L_PAREN, ++i);
                    balance -= check(R_PAREN, i);
                }
            }
        }();

        // indicates a loop variable
        if (has_var) {
            loop_var = parseUnpackmentPattern();
            consume(COLON);
        }


        // non-expr patterns MUST have loop kind to destructure!
        if (loop_var and not dynamic_cast<expr::Unpackment::Expr*>(loop_var.get())) {
            auto kind = parseExpr();

            expr::ExprPtr guard = match(COMMA) ? parseExpr() : nullptr;

            consume(FAT_ARROW);

            // try to parse as a map comprehension!
            auto snapshot = checkpoint();
            try {
                auto body1 = parseExpr<false, Context::MAP>();
                consume(COLON);
                auto body2 = parseExpr();

                consume(R_BRACE);

                return std::make_shared<expr::MapComp>(
                    std::move(body1   ),
                    std::move(body2   ),
                    std::move(loop_var),
                    std::move(kind    ),
                    std::move(guard   )
                );
            }
            catch (const std::runtime_error     &) { }
            catch (const except::SyntaxError    &) { }
            // catch (const except::UnexpectedToken&) { }

            restore(std::move(snapshot));

            auto body = parseExpr();

            consume(R_BRACE);

            return std::make_shared<expr::ListComp>(
                std::move(body    ),
                std::move(loop_var),
                std::move(kind    ),
                std::move(guard   )
            );
        }


        // body after the `=>`. No kind.
        if (match(FAT_ARROW)) {

            // try to parse as a map comprehension!
            auto snapshot = checkpoint();
            try {
                auto body1 = parseExpr<false, Context::MAP>();
                consume(COLON);
                auto body2 = parseExpr();

                consume(R_BRACE);

                return std::make_shared<expr::MapComp>(
                    std::move(body1   ),
                    std::move(body2   ),
                    std::move(loop_var)
                );
            }
            catch (const std::runtime_error     &) { }
            catch (const except::SyntaxError    &) { }
            // catch (const except::UnexpectedToken&) { }

            restore(std::move(snapshot));

            auto body = parseExpr();

            consume(R_BRACE);

            return std::make_shared<expr::ListComp>(
                std::move(body    ),
                std::move(loop_var)
            );
        }



        if (match(COMMA)) {
            auto guard = parseExpr();
            consume(FAT_ARROW);

            // try to parse as a map comprehension!
            auto snapshot = checkpoint();
            try {
                auto body1 = parseExpr<false, Context::MAP>();
                consume(COLON);
                auto body2 = parseExpr();

                consume(R_BRACE);

                return std::make_shared<expr::MapComp>(
                    std::move(body1   ),
                    std::move(body2   ),
                    std::move(loop_var),
                    nullptr,
                    std::move(guard   )
                );
            }
            catch (const std::runtime_error     &) { }
            catch (const except::SyntaxError    &) { }
            // catch (const except::UnexpectedToken&) { }

            restore(std::move(snapshot));

            auto body = parseExpr();

            consume(R_BRACE);

            return std::make_shared<expr::ListComp>(
                std::move(body    ),
                std::move(loop_var),
                nullptr,
                std::move(guard   )
            );
        }



        // loop variable is a regular expression
        // loop could still have a kind:
        auto kind = parseExpr();
        expr::ExprPtr guard = match(COMMA) ? parseExpr() : nullptr;
        consume(FAT_ARROW);

        // try to parse as a map comprehension!
        auto snapshot = checkpoint();
        try {
            auto body1 = parseExpr<false, Context::MAP>();
            consume(COLON);
            auto body2 = parseExpr();

            consume(R_BRACE);

            return std::make_shared<expr::MapComp>(
                std::move(body1   ),
                std::move(body2   ),
                std::move(loop_var),
                std::move(kind    ),
                std::move(guard   )
            );
        }
        catch (const std::runtime_error     &) { }
        catch (const except::SyntaxError    &) { }
        catch (const except::UnexpectedToken&) { }
        catch (...) { }


        restore(std::move(snapshot));

        auto body = parseExpr();

        consume(R_BRACE);

        return std::make_shared<expr::ListComp>(
            std::move(body    ),
            std::move(loop_var),
            std::move(kind    ),
            std::move(guard   )
        );
    }


    template <Context CTX = Context::NONE>
    expr::Unpackment::PatternPtr parseUnpackmentPattern() {
        using enum token::TokenKind;
        using Expr = expr::Unpackment::Expr;
        using List = expr::Unpackment::List;
        using Pack = expr::Unpackment::Pack;
        using Map  = expr::Unpackment::Map ;

        // using PatternPtr = expr::Unpackment::PatternPtr;
        using Patterns = expr::Unpackment::Patterns;

        if (match(L_BRACE)) { // either list pattern or map pattern
            auto pattern = parseUnpackmentPattern();

            if (match(R_BRACE))
                return List::with(std::move(pattern));

            if (match(COMMA)) { // list
                Patterns patterns;
                patterns.push_back(std::move(pattern));

                do patterns.push_back(parseUnpackmentPattern()); while(match(COMMA));

                consume(R_BRACE);

                return std::make_unique<List>(std::move(patterns));
            }


            if (match(COLON)) { // map
                // need to test the first pattern since the we didn't know the context back there
                if (dynamic_cast<expr::Unpackment::Pack*>(pattern.get()))
                    util::error<except::SyntaxError>("Cannot have pack patterns inside map unpackments!");

                auto map = Map::with(std::pair{std::move(pattern), parseUnpackmentPattern<Context::MAP>()});

                while (match(COMMA)) {
                    pattern = parseUnpackmentPattern<Context::MAP>();
                    consume(COLON);
                    map->patterns.emplace_back(std::move(pattern), parseUnpackmentPattern<Context::MAP>());
                }

                consume(R_BRACE);

                return map;
            }

            util::error<except::SyntaxError>("Unrecognized Pattern!");
        }
        else if (match(ELLIPSIS)) { // pack
            if constexpr (CTX == Context::MAP) util::error<except::SyntaxError>("Cannot have pack patterns inside map unpackments!");

            return std::make_unique<Pack>(
                parseExpr()
            );
        }
        else { // name pattern
            return std::make_unique<Expr>(
                parseExpr<false, CTX>()
            );
        }
    }


    expr::ExprPtr unpackment() {
        using enum token::TokenKind;

        unconsume(); // unconsume the L_RBACE token so paseUnpackmentPattern works correctly
        auto pattern = parseUnpackmentPattern();


        if (not check(ASSIGN) and not check(WALRUS))
            util::error<except::SyntaxError>("Unpackment can only be used on the LHS of an assignment!");

        const bool inferred = consume().kind == WALRUS;
        return std::make_shared<expr::Unpackment>(
            std::move(pattern),
            parseExpr(),
            inferred
        );
    }


    bool isScope() {
        using enum token::TokenKind;

        for (size_t i{}; /* not atEnd(i) */; ++i) {
            if (check(SEMI   , i)) return true;

            if (check(R_BRACE, i)) return false;
            if (check(R_PAREN, i)) return false;


            for (ssize_t balance = check(L_BRACE, i); balance; ) {
                balance += check(L_BRACE, ++i);
                balance -= check(R_BRACE, i);
            }

            for (ssize_t balance = check(L_PAREN, i); balance; ) {
                balance += check(L_PAREN, ++i);
                balance -= check(R_PAREN, i);
            }
        }
    }


    bool isUnpackment() {
        using enum token::TokenKind;

        for (size_t i{}; /* not atEnd() */ ; ++i) {
            if (check(R_BRACE, i)) {
                if (check(ASSIGN, i+1) or check(WALRUS, i+1)) return true;
                else return false;
            }


            for (ssize_t balance = check(L_BRACE, i); balance;) {
                balance += check(L_BRACE, ++i);
                balance -= check(R_BRACE, i);
            }

            for (ssize_t balance = check(L_PAREN, i); balance;) {
                balance += check(L_PAREN, ++i);
                balance -= check(R_PAREN, i);
            }
        }
    }


    bool isMap() {
        using enum token::TokenKind;

        for (size_t i{}; /* not atEnd(i) */ ; ++i) {
            // if you find a colon first, then
            // it could be a map {x: y};
            // OR it could be a declaration {x: y = 1;};
            // must find an assignment to make sure...

            // { (name1: Int = name3): name4 }
            // { name1: Int = name3 }
            if (check(COLON, i)) {
                for (size_t a = i + 1; /* not atEnd(a) */; ++a) {
                    // onto next element, it's a map
                    if (check(COMMA  , a)) return true;

                    // closed the map, it's a map
                    if (check(R_BRACE, a)) return true;

                    // this time the colon indicates a declaration {n1: n2: n3 = 4};
                    // this does NOT indicate a qualified name (namespace accesss)
                    // since 2 colons back to back are tokenized as TokenKind::SPACE_RESOLVE
                    if (check(COLON  , a)) return true;

                    // proly a declaration, it's a scope..i think :c
                    if (check(SEMI   , a)) return false;


                    for (ssize_t balance = check(L_BRACE, a); balance;) {
                        balance += check(L_BRACE, ++a);
                        balance -= check(R_BRACE, a);
                    }

                    for (ssize_t balance = check(L_PAREN, a); balance;) {
                        balance += check(L_PAREN, ++a);
                        balance -= check(R_PAREN, a);
                    }
                }
            }

            if (check(COMMA  , i)) return false; // if you find a comma first, it's a list {1, 2};
            if (check(R_BRACE, i)) return false; // finding a `}` before finding `:` means it's a list with potentially one element
            if (check(SEMI   , i)) return false; // finding a `;` means it's a scope, and that was the end of an expression...

            for (ssize_t balance = check(L_BRACE, i); balance;) {
                balance += check(L_BRACE, ++i);
                balance -= check(R_BRACE, i);
            }

            for (ssize_t balance = check(L_PAREN, i); balance;) {
                balance += check(L_PAREN, ++i);
                balance -= check(R_PAREN, i);
            }
        }

        return false; // argubaly, should be `error()`
    }



    expr::ExprPtr LBrace() {
        using enum token::TokenKind;

        // empty list `{}`
        if (match(R_BRACE)) return std::make_shared<expr::List>();

        // empty map `{:}`
        if (match(COLON)) return consume(R_BRACE), std::make_shared<expr::Map>();


        // if there is at least one top-level semicolon, it's a scope!
        if (isScope()     ) return handleScope();


        if (isUnpackment()) return  unpackment();


        const auto funcs = {
            &Parser::comprehension,
            &Parser::map,
            &Parser::list,
        };


        for (const auto& func : funcs) {
            auto snapshot = checkpoint();
            try {
                return (this->*func)();
            }
            catch (const std::runtime_error     &) { }
            catch (const except::SyntaxError    &) { }
            catch (const except::UnexpectedToken&) { }
            // catch (...) {
            //     throw; // throw last error 
            // }

            restore(std::move(snapshot));
        }

        // std::unreachable();


        util::error("Ambiguous open braces!");
    }


    template <bool PARSE_TYPE = true, Context CTX>
    expr::ExprPtr LParen() {
        using enum token::TokenKind;

        if (match(R_PAREN)) { // nullary closure
            type::TypePtr return_type = match(COLON) ? parseType() : type::builtins::_();

            consume(FAT_ARROW);
            // It's a closure
            return std::make_shared<expr::Closure>(std::vector<expr::Closure::Param>{}, closureBody(), type::FuncType{{}, std::move(return_type)});
        }

        // todo: fix this algorithm
        const bool fold_expr = [this] {
            for (size_t i{}; /* not atEnd(i) */; ++i) {
                if (check(R_PAREN , i)) return false;
                if (check(COLON   , i)) return false;
                if (check(ELLIPSIS, i)) return true ;


                if (check(L_BRACE, i)) while (not check(R_BRACE, i)) ++i;
                if (check(L_PAREN, i)) while (not check(R_PAREN, i)) ++i;
            }
            return false;
        }();

        if (fold_expr) return parseFoldExpr();

        // auto exprs = parseCommaList();


        const bool closure_expr = [this] {
            size_t i{};
            for (; /* not atEnd(i) and */ not check(R_PAREN , i); ++i) {
                if (check(L_BRACE, i)) while (not check(R_BRACE, i)) ++i;
                if (check(L_PAREN, i)) while (not check(R_PAREN, i)) ++i;
            }
            ++i;

            return (CTX != Context::MAP and check(COLON, i)) or check(FAT_ARROW, i); // ( ... ): OR ( ... ) =>
        }();


        if (closure_expr) return closure();


        // just a grouping `(x)`
        const auto expr = std::make_shared<expr::Grouping>(parseExpr());
        consume(R_PAREN);
        return expr;
        // return expr;
    }


    expr::ExprPtr backticks() {
        util::Deferred d{[this] { consume(token::TokenKind::BACKTICK); }};


        auto syn = std::make_shared<expr::Syntax>(parseExpr());

        if (auto fix = analysis::exprContains<expr::Fix>(syn->expr))
            unAddOp(fix);


        return syn;
    }


    expr::ExprPtr name(token::Token token) {
        if (match(token::TokenKind::COLON)){ // exprs preceeded by `:` are parsed as type
            // consume(/* COLON */);
            auto type = parseType();


            consume(token::TokenKind::ASSIGN);

            return std::make_shared<expr::Assignment>(
                std::make_shared<expr::Name>(std::move(token).text),
                std::move(type),
                parseExpr()
            );
        }

        return std::make_shared<expr::Name>(std::move(token).text);
    }


    expr::ExprPtr infixName(expr::ExprPtr left, token::Token token) {
        if (opsContain(token.text)) {
            using enum token::TokenKind;

            switch (const auto& op = findOp(token.text); op->type()) {
                // case TokenKind::PREFIX:
                //     return std::make_shared<UnaryOp>(token, parseExpr(precFromToken(op->prec)));

                case INFIX: {
                    const auto prec = prec::calculate(op->high, op->low, consolidateOps());
                    return std::make_shared<expr::BinOp>(std::move(left), std::move(token).text, parseExpr(prec));
                }
                case SUFFIX:
                    return std::make_shared<expr::PostOp>(std::move(token).text, std::move(left));


                //* I can fix this. Check if the name is the first or not and error accordingly!
                case EXFIX: {
                    const auto& op = dynamic_cast<const expr::Exfix*>(findOp(token.text).get());
                    if (token.text != op->name2) util::error<except::OperatorError>("Open exfix operator found where closing one was expected!");

                    return left;
                }


                // some other part of Operator. 
                case MIXFIX: {
                    const auto& op = dynamic_cast<const expr::Operator*>(findOp(token.text).get());

                    // error("Beginning operator '" + token.text  + "' found where it shouldn't be!");
                    // in the middle of parsing a OpCall. Do nothing.
                    if (token.text != op->name)  return left;
                    if (op->op_pos[0]) util::error<except::OperatorError>("Operator '" + op->name + "' has to come before an expression!");


                    // if (op->begin_expr) error("Operator '" + op->name + " ...' has to come after a name!");


                    const int prec = prec::calculate(op->high, op->low, consolidateOps());

                    std::vector<expr::ExprPtr> exprs = {std::move(left)};


                    for (size_t i{}; const auto& is_op : op->op_pos | std::views::drop(2)) {
                        // match will consume the op
                        if (is_op) {
                            if (not match(op->rest[i++]))
                                util::error<except::OperatorError>("Expected '" + op->rest[i-1] + "', got '" + lookAhead().text + "'!");
                        }
                        else exprs.push_back(parseExpr(prec));
                    }


                    return std::make_shared<expr::OpCall>(op->name, op->rest, std::move(exprs), op->op_pos);
                }

                default: util::error<except::OperatorError>("prefix operator used as [inf/suf]fix");
            }
        }

        return std::make_shared<expr::Name>(std::move(token).text);
    }


    expr::ExprPtr parsePrefixOperator(token::Token token) {
        switch (const auto& op = findPrefixOp(token.text); op->type()) {
            using enum token::TokenKind;

            case PREFIX: {
                const int prec = prec::calculate(op->high, op->low, consolidateOps());
                return std::make_shared<expr::UnaryOp>(std::move(token).text, parseExpr(prec));
            }

            case EXFIX: {
                auto op = dynamic_cast<const expr::Exfix*>(findPrefixOp(token.text).get());

                auto ret = std::make_shared<expr::CircumOp>(op->name, op->name2, parseExpr());

                if (not match(op->name2)) util::error<except::OperatorError>("Exfix operator not closed!");

                return ret;
            }

            case MIXFIX: {
                auto op = dynamic_cast<const expr::Operator*>(findPrefixOp(token.text).get());
                if (not op->op_pos[0]) util::error<except::OperatorError>("Operator '" + token.text + "' has to come after an expression!");

                const int prec = prec::calculate(op->high, op->low, consolidateOps());

                std::vector<expr::ExprPtr> exprs;
                for (size_t i{}; const auto& is_op : op->op_pos | std::views::drop(1)) {
                    // match will consume the op
                    if (is_op) {
                        if (not match(op->rest[i++]))
                            util::error<except::OperatorError>("Expected '" + op->rest[i-1] + "', got '" + lookAhead().text + "'!");
                    }
                    else exprs.push_back(parseExpr(prec));
                }

                return std::make_shared<expr::OpCall>(
                    op->name, op->rest, std::move(exprs), op->op_pos // op->begin_expr, op->end_expr
                );
            }

            default:
                // log();
                util::error<except::OperatorError>("[in/suf]fix operator '" + token.text + "' used as [pre/ex]fix");
        }
    }



    void fixPrecedence(std::string& p) {
        if (p == "(") { // precedence is `()`
            consume(")");
            p += ')';
        }

        else if (p == "[") { // precedence is `[]`
            consume("]");
            p += ']';
        }
    }



    void checkOperatpr(const token::TokenKind kind, const std::string& name, const std::string& high, const std::string& low) {
        using enum token::TokenKind;

        // gotta dry out this part
        // plus, I don't like that I made "Fix" take a "ExprPtr" rather than closure but I'll leave it for now
        switch (kind) {
            case PREFIX:
                if (prefixOpsContain(name)) {
                    auto& op = findPrefixOp(name);

                    // op->funcs.push_back(std::move(func));

                    if (op->type() != PREFIX)
                        util::error<except::OperatorError>("Overload set for operator `" + name + "` must have the same operator type!");

                    if (op->high != high or op->low != low)
                        util::error<except::OperatorError>("Overloaded set of operator `" + name + "` must all have the same precedence!");
                }
                break;

            case INFIX:
            case SUFFIX:
                if (opsContain(name)) {
                    auto& op = findOp(name);

                    // op->funcs.push_back(std::move(func));

                    if (op->type() != kind)
                        util::error<except::OperatorError>("Overload set for operator `" + name + "` must have the same operator type!");

                    if (op->high != high or op->low != low)
                        util::error<except::OperatorError>("Overloaded set of operator `" + name + "` must all have the same precedence!");
                }
                break;

            default: util::error();
        }
    }


    // operator defintion
    // fix(PREC) op = (...) => ...
    expr::ExprPtr fixOperator(token::Token token) {
        using enum token::TokenKind;

        if (token.kind == EXFIX ) return exfixOperator();
        if (token.kind == MIXFIX) return arbitraryOperator();


        int shift{};
        std::string name, low, high;
        auto consolidated = consolidateOps();


        if (match(L_PAREN)) {
            low = consume().text;
            fixPrecedence(low);

            shift = parseOperatorShift();

            high = [&consolidated, shift, &low] {
                if (shift > 0) return prec::higher(low, consolidated);
                if (shift < 0) return std::exchange(low, prec::lower(low, consolidated));
                return low;
            }();

            consume(R_PAREN);

            name = consume(NAME).text;
        }
        else name = low = high = consume(NAME).text;


        // technically I can report this error 2 lines earlier, but printing out the operator name could be very handy!
        if (high == low and (prec::precedenceOf(high, consolidated) == prec::HIGH_VALUE or prec::precedenceOf(low, consolidated) == prec::LOW_VALUE))
            util::error<except::OperatorError>("Can't have set operator precedence to only LOW/HIGH: " + name);


        consume(ASSIGN);

        expr::ExprPtr func = parseExpr();
        expr::Closure *c = dynamic_cast<expr::Closure*>(func.get());
        if (not c) util::error<except::OperatorError>("[pre/in/suf] fix operator has to be equal to a function!");


        checkOperatpr(token.kind, name, high, low);


        std::shared_ptr<expr::Fix> p;
        if (token.kind == PREFIX) {
            if (c->params.size() != 1) util::error<except::OperatorError>("Prefix operator must be assigned to a unary closure!");
            p = std::make_shared<expr::Prefix>(name, std::move(high), std::move(low), shift, std::vector<expr::ExprPtr>{/*std::move(func)*/});
        }
        else if (token.kind == INFIX) {
            if (c->params.size() != 2) util::error<except::OperatorError>("Infix operator must be assigned to a binary closure!");
            p = std::make_shared<expr::Infix> (name, std::move(high), std::move(low), shift, std::vector<expr::ExprPtr>{/*std::move(func)*/});
        }
        else /* if (token.kind == SUFFIX) */ {
            if (c->params.size() != 1) util::error<except::OperatorError>("Suffix operator must be assigned to a unary closure!");
            p = std::make_shared<expr::Suffix>(name, std::move(high), std::move(low), shift, std::vector<expr::ExprPtr>{/*std::move(func)*/});
        }

        p->funcs.push_back(func);
        if (envContains(p->stringify())) return p;


        if (token.kind == PREFIX) {
            if (not prefixOpsContain(name)) {
                env.back().first.prefix_op_env[name] = p->clone();
                env.back().first.prefix_op_env[name]->funcs.pop_back(); // probably not needed!
            }
        }
        else if (not opsContain(name)) {
            env.back().first.op_env[name] = p->clone();
            env.back().first.op_env[name]->funcs.pop_back(); // probably not needed!
        }


        return p;
    }


    expr::ExprPtr exfixOperator() {
        using enum token::TokenKind;

        std::string name1 = consume(NAME).text;
        consume(COLON);
        std::string name2 = consume(NAME).text;

        consume(ASSIGN);

        expr::ExprPtr func = parseExpr();
        expr::Closure *c = dynamic_cast<expr::Closure*>(func.get());
        if (not c                ) util::error<except::OperatorError>("Exfix operator has to be equal to a function!");
        if (c->params.size() != 1) util::error<except::OperatorError>("Exfix operator must be assigned to a unary closure!");


        std::shared_ptr<expr::Fix> p = std::make_shared<expr::Exfix>(
            name1, name2, prec::LOW, prec::LOW, 0, std::vector<expr::ExprPtr>{/* std::move(func) */}
        );


        if (prefixOpsContain(name1)) {
            const auto& op = findPrefixOp(name1);
            // op->funcs.push_back(std::move(func));

            if (op->type() != token::TokenKind::EXFIX) {
                std::println(std::cerr, "Overload set for operator '{}:{}' must have the same operator type:", name1, name2);
                util::expected(op->type(), token::TokenKind::EXFIX);
            }

            auto ex = dynamic_cast<const expr::Exfix*>(op.get());

            if (ex->name != name1 or ex->name2 != name2) {
                util::error<except::OperatorError>("Overload set of exfix operator must all have the same operator name `" + ex->name + " : " + ex->name2 + '`');
            }

            p->funcs.push_back(func);

            return p;
            // return std::make_shared<expr::Exfix>(*ex);
        }

        if (opsContain(name2)) {
            std::println(std::cerr, "Overload set for operator '{}:{}' must have the same operator type:", name1, name2);
            util::expected(findOp(name2)->type(), token::TokenKind::EXFIX);
        }


        p->funcs.push_back(func);
        if (envContains(p->stringify())) return p;


        env.back().first.prefix_op_env[name1] = p->clone();
        env.back().first.prefix_op_env[name1]->funcs.pop_back(); // probably not needed!

        // interesting idea
        env.back().first.op_env[name2] = p->clone(); //* maybe? //* maybe not...? idk
        env.back().first.op_env[name2]->funcs.pop_back(); // probably not needed!

        // pushing back after cloning so that the op table doesn't contain the closure
        // p->funcs.push_back(std::move(func));

        return p;
    };


    expr::ExprPtr arbitraryOperator() {
        using enum token::TokenKind;

        consume(L_PAREN);
        std::string low = consume().text;
        fixPrecedence(low);

        const int shift = parseOperatorShift();
        const auto consolidated = consolidateOps();

        // non-const so it's movable later
        auto high = [&consolidated, shift, &low] {
            if (shift > 0) return prec::higher(low, consolidated);
            if (shift < 0) return std::exchange(low, prec::lower(low, consolidated));
            return low;
        }();

        consume(R_PAREN);

        // const bool begins_with_expr = match(COLON);

        std::vector<bool> op_pos;
        std::string first;

        if (match(SCOPE_RESOLVE)) util::error<except::OperatorError>("Mixfix operator may only require 1 argument before an operator name!");

        if (match(COLON)) {
            op_pos.push_back(false);
            first = consume(NAME).text;
            op_pos.push_back(true);
        }
        else {
            op_pos.push_back(true);
            first = consume(NAME).text;
        }

        std::vector<std::string> rest;

        while (not check(ASSIGN)) {
            if      (match(SCOPE_RESOLVE)) op_pos.push_back(false), op_pos.push_back(false);
            else if (match(COLON))         op_pos.push_back(false);
            else                           op_pos.push_back(true );

            if (op_pos.back()) rest.push_back(consume(NAME).text);
        }

        consume(ASSIGN);

        expr::ExprPtr func = parseExpr();
        expr::Closure *c = dynamic_cast<expr::Closure*>(func.get());
        if (not c) util::error<except::OperatorError>("Operators have to be equal to a function!");


                                    // false == expression parameter
        if (const size_t param_count = std::ranges::count(op_pos, false);
            param_count != c->params.size()
        )
        {
            std::string op_name;
            for (ssize_t i = -1; const auto& field : op_pos) {
                if (field) {
                    op_name += i == -1 ? first : rest[i];
                    ++i;
                }
                else op_name += ':';
            }

            const std::string& n = std::to_string(param_count);
            util::error<except::OperatorError>("Operator '" + op_name + "' must be assigned to a closure with " + n + " parameters!");
        }

        const bool is_prefix = op_pos.front(); // assigned here bc I move op_pos in the next line
        std::shared_ptr<expr::Fix> p =
            std::make_shared<expr::Operator>(
                std::move(first),
                rest, // how can I move it?
                std::move(op_pos),
                std::move(high), std::move(low),
                shift,
                // begins_with_expr, ends_with_expr,
                std::vector<expr::ExprPtr>{/* std::move(func) */}
            );



        if (opsContain(first) or prefixOpsContain(first)) {
            const auto& op = findOp(first);
            // op->funcs.push_back(std::move(func));

            if (op->type() != MIXFIX) util::error<except::OperatorError>(); // todo: ADD ERR MSG

            auto arb = dynamic_cast<const expr::Operator*>(op.get());

            bool same = first == arb->name;
            for (auto&& [n1, n2] : std::views::zip(rest, arb->rest))
                if (n1 != n2) {
                    same = false;
                    break;
                }

            if (not same) util::error<except::OperatorError>(); // todo: ADD ERR MSG


            p->funcs.push_back(std::move(func));
            return p;
            // return std::make_shared<expr::Operator>(*arb);
        }


        p->funcs.push_back(func);
        if (envContains(p->stringify())) return p;
        p->funcs.pop_back();


        if (is_prefix) {
            env.back().first.prefix_op_env[p->name] = p;
            for (const auto& name : rest) env.back().first.op_env[name] = p;
        }
        else {
            env.back().first.op_env[p->name] = p;
            for (const auto& name : rest) env.back().first.op_env[name] = p;
        }



        // push_back after clone on purpose.
        p->funcs.push_back(std::move(func));
        return p;
    }


    int parseOperatorShift() {
        if (check(token::TokenKind::NAME)) {
            const auto shift_token = consume(token::TokenKind::NAME).text;
            // assert(shift_token.text.length() <= 1);

            if (shift_token.length() == 1){
                if (shift_token[0] != '+' and shift_token[0] != '-')
                    util::error<except::OperatorError>("Can only have '+' or '-' after precedene!");
                // if (shift_token.text.find_first_not_of(shift_token.text.front()) != std::string::npos) error("can't have a mix of + and - or any other symbol after precedene!");

                return shift_token[0] == '+' ? 1 : -1;
            }
            else if (shift_token.length() > 1) {
                if (shift_token[0] == '+' or shift_token[0] == '-')
                    util::error<except::OperatorError>("Can only have one +/- after a precedence level");
                else
                    util::error<except::OperatorError>("Can only have '+' or '-' after precedene!");
            }
        }

        return 0; // no shift
    }


    void unconsume(const ptrdiff_t n = 1) {
        const auto logical_pos = std::distance(tokens.begin(), token_iterator) - static_cast<ptrdiff_t>(red.size());

        if (n > logical_pos)
            util::error("unconsume(n): tried to undo more tokens than have actually been consumed");

        red.insert(
            red.begin(),
            std::next(tokens.begin(), logical_pos - static_cast<ptrdiff_t>(n)),
            std::next(tokens.begin(), logical_pos)
        );
    }


    token::Token consume() {
        lookAhead();

        const auto token = red.front();
        red.pop_front();
        return token;
    }


    token::Token consume(const token::TokenKind exp, const std::source_location& loc = std::source_location::current()) {
        using std::operator""s;

		if (const token::Token token = lookAhead(); token.kind != exp) [[unlikely]] {
            // log();
            util::expected(exp, token, loc);
        }

		return consume();
	}


    token::Token consume(const std::string& exp, const std::source_location& loc = std::source_location::current()) {
        using std::operator""s;

		if (const token::Token token = lookAhead(); token.text != exp) [[unlikely]] {
            // log();
            util::expected(exp, token, loc);
        }

		return consume();
	}


    [[nodiscard]] bool match(const token::TokenKind exp) {
		const token::Token token = lookAhead();

		if (token.kind != exp) return false;

		consume();
		return true;
	}


    [[nodiscard]] bool match(const std::string_view text) {
		const token::Token token = lookAhead();

		if (token.text != text) return false;

		consume();
		return true;
    }


    token::Token lookAhead(const size_t distance = 0, const std::source_location& loc = std::source_location::current()) {
        while (distance >= red.size()) {
            if (atEnd()) util::error("out of token!", loc);
            red.push_back(*token_iterator++);
        }

        // Get the queued token.
        return red[distance];
    }


    [[nodiscard]] bool check(const token::TokenKind exp, const size_t i = {}, const std::source_location& loc = std::source_location::current()) {
        return lookAhead(i, loc).kind == exp;
    }


    [[nodiscard]] bool check(const std::string_view exp, const size_t i = {}, const std::source_location& loc = std::source_location::current()) {
        return lookAhead(i, loc).text == exp;
    }


    [[nodiscard]] int getPrecedence() {

        const token::Token& token = lookAhead();
        switch (token.kind) {
            using enum token::TokenKind;

            // right associative
            // case COMMA: return prec::COMMA_VALUE;

            case WALRUS:
            case ASSIGN: return prec::ASSIGNMENT_VALUE;

            case SCOPE_RESOLVE: return prec::SCOPE_RESOLUTION_VALUE;

            case DOT    : return prec::MEMBER_ACCESS_VALUE;
            case CASCADE: return prec::CASCADE_VALUE;

            case NAME: {
                // Probably in the middle of a mixfix() that takes 2 colons ': :' or more (2 expression arguments back to back)
                // ex: mixfix(LOW +) if : : else : = (cond, thn, els) => ...;
                if (not opsContain(token.text)) {
                    // log();
                    // error("Operator '" + token.text + "' not found!");
                    return 0;
                }

                const auto& op = findOp(token.text);
                const int prec = prec::calculate(op->high, op->low, consolidateOps());

                //todo: prefix sould also be right to left
                // mix fix operators should be parsed right to left.......I think ;-;
                if (token.text == op->name and op->type() == MIXFIX)
                    return prec + 1;

                return prec;
            }

            case L_PAREN: return prec::CALL_VALUE;


            default: return 0;
        }
    }



    void log(bool shift = false, bool begin = false) {
        puts("");
        const auto iter = token_iterator;
        const ptrdiff_t dist = std::distance(tokens.begin(), iter);
        const ptrdiff_t diff = begin ? 0 : dist < 5 ? dist : 5;

        std::copy(iter - (shift ? diff : 0), tokens.end(), std::ostream_iterator<token::Token>{std::clog, " "});
        puts("");
    }



    void   scope() { env.push_back({}); }
    void unscope() { env.pop_back(); }


    void enterNamespace(const std::string& name) {
        // global_spaces.insert({name, std::make_unique<NameSpace>(name)});

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
            ns = (current_space.back()->children[name] = std::make_unique<NameSpace>(name)).get();
        }


        current_space.push_back(ns);
        scope();
    }


    void exitNamespace() {
        unscope();
        current_space.pop_back();
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


    // ideally, should be called findSpaces!
    NameSpace* findSpace(const std::vector<std::string>& names, const bool global_search_only = false) {
        if (not global_search_only) {
            for (const auto space : std::views::reverse(current_space)) {
                if (const auto s = matchChain(names, space)) return s;

                for (const auto& [_, child] : space->children)
                    if (const auto s = matchChain(names, child.get())) return s;
            }
        }


        for (const auto& [_, ns] : global_spaces) {
            if (const auto s = matchChain(names, ns.get()))
                return s;
        }

        util::error<except::NameLookup>("Space `" + stringify(names) + "` not found!");
    }



    bool envContains(const std::string& op) const {
        for (const auto &e : env)
            if (e.first.vars.contains(op)) return true;

        return false;
    }


    bool opsContain(const std::string& op) const {
        for (const auto& ops : env)
            if (ops.first.op_env.contains(op)) return true;

        return false;
    }


    bool prefixOpsContain(const std::string& op) const {
        for (const auto &e : env)
            if (e.first.prefix_op_env.contains(op)) return true;

        return false;
    }



    // todo: remove these 2 functions and make the previous 2 return the pointer or null
    const std::shared_ptr<expr::Fix>& findOp(const std::string& op, const std::source_location& loc = std::source_location::current()) const {
        for (const auto& e : std::views::reverse(env))
            if (e.first.op_env.contains(op)) return e.first.op_env.at(op);

        util::error<except::OperatorError>(loc);
    }

    const std::shared_ptr<expr::Fix>& findPrefixOp(const std::string& op, const std::source_location& loc = std::source_location::current()) const {
        for (const auto& e : std::views::reverse(env)) {
            if (e.first.prefix_op_env.contains(op)) return e.first.prefix_op_env.at(op);
        }

        util::error<except::OperatorError>(loc);
    }


    Operators consolidateOps() const {
        Operators consolidated;
        for (const auto& e : env) {
            for (const auto& [name, op] : e.first.op_env) {
                consolidated[name] = op->clone();
            }
        }

        for (const auto& e : env) {
            for (const auto& [name, op] : e.first.prefix_op_env) {
                consolidated[name] = op->clone();
            }
        }

        return consolidated;
    }



    struct Snapshot {
        token::Tokens tokens;
        typename std::iterator_traits<token::Tokens::iterator>::difference_type token_index;
        std::deque<token::Token> red;



        std::vector<std::pair<Env, EnvTag>> env;


        // std::unordered_map<std::string, std::shared_ptr<NameSpace>> global_spaces;
        // std::vector<NameSpace*> current_space;

        Snapshot(
            token::Tokens ts,
            typename std::iterator_traits<token::Tokens::iterator>::difference_type i,
            std::deque<token::Token> read,
            std::vector<std::pair<Env, EnvTag>> env
        ) :
        tokens{std::move(ts)}             ,
        token_index{i},
        red{std::move(read)},
        env{std::move(env)}
        { }
    };

    [[nodiscard]] Snapshot checkpoint() {
        return {tokens, std::distance(iterator_beginning, token_iterator), red, env};
    }

    void restore(Snapshot&& snapshot) {
        tokens             = std::move(snapshot).tokens;
        iterator_beginning = tokens.begin();
        token_iterator     = std::next(iterator_beginning, std::move(snapshot).token_index);
        red                = std::move(snapshot).red;
        env                = std::move(snapshot).env;
    }

};


} // parse
} // pie

