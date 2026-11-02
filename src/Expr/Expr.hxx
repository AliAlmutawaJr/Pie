#pragma once

#include <cctype>
#include <concepts>
#include <string>
#include <string_view>
#include <filesystem>
#include <type_traits>
#include <vector>
#include <utility>
#include <tuple>
#include <algorithm>
#include <ranges>
#include <variant>
#include <optional>
#include <memory>

#include <cctype>

#include "../Utils/utils.hxx"
#include "../Lex/Token.hxx"
#include "../Declarations.hxx"
#include "../Type/Type.hxx"



namespace pie {


namespace expr {

struct StringID {
    std::string name;
    ssize_t ID = -1;
};





inline bool optionalMatches(const ExprPtr& mine, const ExprPtr& theirs, Captures& captures) {
    if (not mine or not theirs) return not mine and not theirs;
    return mine->matches(theirs.get(), captures);
}


inline bool sameType(const type::TypePtr& mine, const type::TypePtr& theirs) {
    if (not mine or not theirs) return not mine and not theirs; // if they're both null, I guess they're equal

    return *mine == *theirs;
}



inline ExprPtr splice(const ExprPtr& e, const Fillings& fillings) {
    if (not e) return e;

    auto spliced = e->fillWith(fillings);

    return spliced ? spliced : e;
}


struct Num : Expr {
    std::string num;

    explicit Num(std::string n, util::SourceSpan sp) noexcept : Expr{std::move(sp)}, num{std::move(n)} { }

    std::string stringify(const size_t = 0) const override { return num; }



    bool matches(Expr* code, Captures&) const override {
        const auto* that = dynamic_cast<const Num*>(code);
        return that and num == that->num;
    }

    bool involvesName(const std::string_view sv) const override { return sv == stringify(); }

    Node variant() override { return this; }
};


struct Bool : Expr {
    bool boolean;

    explicit Bool(const bool b, util::SourceSpan sp) noexcept : Expr{std::move(sp)}, boolean{b} { }

    std::string stringify(const size_t = 0) const override { return boolean ? "true" : "false"; }

    bool matches(Expr* code, Captures&) const override {
        const auto* that = dynamic_cast<const Bool*>(code);
        return that and boolean == that->boolean;
    }

    bool involvesName(const std::string_view sv) const override { return sv == stringify(); }

    Node variant() override { return this; }
};


struct String : Expr {
    std::string str;

    explicit String(std::string s, util::SourceSpan sp) noexcept : Expr{std::move(sp)} , str{std::move(s)} {}

    std::string stringify(const size_t = 0) const override { return '"' + str + '"'; }

    bool matches(Expr* code, Captures&) const override {
        const auto* that = dynamic_cast<const String*>(code);
        return that and str == that->str;
    }

    bool involvesName(const std::string_view sv) const override { return sv == stringify(); }

    Node variant() override { return this; }
};


struct FString : Expr {
    std::string str;
    std::vector<std::pair<size_t, ExprPtr>> exprs;

    FString(std::string s, std::vector<std::pair<size_t, ExprPtr>> es, util::SourceSpan sp = {}) noexcept
    : Expr{std::move(sp)}, str{std::move(s)}, exprs{std::move(es)} { }

    std::string stringify(const size_t indent = 0) const override {
        std::string s;


        for (size_t i{}, e{}; i < str.size() or e < exprs.size(); ++i) {
            for (; e < exprs.size() and exprs[e].first == i; ++e) {
                s += '{' + exprs[e].second->stringify(indent + 2) + '}';
            }

            if (i < str.size()) s.push_back(str[i]);
        }

        return '"' + s + '"';
    }

    bool matches(Expr* code, Captures& captures) const override {
        const auto* that = dynamic_cast<const FString*>(code);
        if (not that or str != that->str or exprs.size() != that->exprs.size()) return false;

        for (const auto& [mine, theirs] : std::views::zip(exprs, that->exprs))
            if (mine.first != theirs.first or not mine.second->matches(theirs.second.get(), captures)) return false;

        return true;
    }

    bool involvesName(const std::string_view sv) const override {
        for (const auto& expr : exprs)
            if (expr.second->involvesName(sv)) return true;

        return sv == stringify();
    }

    ExprPtr fillWith(const Fillings& fillings) const override {
        auto copy = std::make_shared<FString>(*this);
        for (auto& [_, e] : copy->exprs) e = splice(e, fillings);
        return copy;
    }

    Node variant() override { return this; }
};


struct Name : Expr {
    std::string name;

    explicit Name(std::string n, util::SourceSpan sp = {}) noexcept : Expr{std::move(sp)}, name{std::move(n)} { }

    std::string stringify(const size_t = 0) const override { return name; }

    bool matches(Expr* code, Captures&) const override {
        const auto* that = dynamic_cast<const Name*>(code);
        return that and name == that->name;
    }

    bool involvesName(const std::string_view sv) const override { return sv == stringify(); }

    Node variant() override { return this; }
};


// struct Pack : Expr {
//     std::vector<ExprPtr> values;

//     explicit Pack(std::vector<ExprPtr> vs) : values{std::move(vs)} {}


//     std::string stringify(const size_t indent = 0) const override {
//         std::string s;
//         std::string comma;

//         for (auto&& v : values) {
//             s += comma + v->stringify(indent);
//             comma = ", ";
//         }

//         return s;
//     }

//     Node variant() override { return this; }
// };


struct List : Expr {
    std::vector<ExprPtr> elements;

    explicit List(std::vector<ExprPtr> elts = {}, util::SourceSpan sp = {}) noexcept : Expr{std::move(sp)}, elements{std::move(elts)} { }

    std::string stringify(const size_t indent = 0) const override {
        if (elements.empty()) return "{}";


        std::string s = "{";
        for (const auto& elt : elements) {
            s += elt->stringify(indent + 4) + ", ";
        }

        s.pop_back(); s.pop_back();

        return s + "}";
    }

    bool matches(Expr* code, Captures& captures) const override {
        const auto* that = dynamic_cast<const List*>(code);
        if (not that or elements.size() != that->elements.size()) return false;

        for (const auto& [mine, theirs] : std::views::zip(elements, that->elements))
            if (not mine->matches(theirs.get(), captures)) return false;

        return true;
    }

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify() or std::ranges::any_of(elements, [sv] (const auto& e) {
            return e->involvesName(sv);
        });
    }

    ExprPtr fillWith(const Fillings& fillings) const override {
        auto copy = std::make_shared<List>(*this);
        for (auto& e : copy->elements) e = splice(e, fillings);
        return copy;
    }

    Node variant() override { return this; }
};


struct Map : Expr {
    std::vector<std::pair<ExprPtr, ExprPtr>> items;

    explicit Map(std::vector<std::pair<ExprPtr, ExprPtr>> elts = {}, util::SourceSpan sp = {}) noexcept : Expr{std::move(sp)}, items{std::move(elts)} { }
    // explicit Map(std::unordered_map<ExprPtr, ExprPtr> elts = {}) noexcept : elements{std::move(elts)} {}

    std::string stringify(const size_t indent = 0) const override {
        if (items.empty()) return "{:}";


        std::string s = "{";
        for (const auto& [key, value] : items) {
            s += key->stringify(indent + 4) + ": " + value->stringify(indent + 4) + ", ";
        }

        s.pop_back(); s.pop_back();

        return s + "}";
    }

    bool matches(Expr* code, Captures& captures) const override {
        const auto* that = dynamic_cast<const Map*>(code);
        if (not that or items.size() != that->items.size()) return false;

        for (const auto& [mine, theirs] : std::views::zip(items, that->items)) {
            if (not mine.first ->matches(theirs.first .get(), captures)) return false;
            if (not mine.second->matches(theirs.second.get(), captures)) return false;
        }

        return true;
    }

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify() or std::ranges::any_of(items, [sv] (const auto& e) {
            const auto& [key, value] = e;
            return key->involvesName(sv) or value->involvesName(sv); 
        });
    }

    ExprPtr fillWith(const Fillings& fillings) const override {
        auto copy = std::make_shared<Map>(*this);
        for (auto& [key, value] : copy->items) {
            key   = splice(key  , fillings);
            value = splice(value, fillings);
        }
        return copy;
    }

    Node variant() override { return this; }
};



struct Tuple : Expr {
    std::vector<ExprPtr> elts;

    explicit Tuple(std::vector<ExprPtr> elts = {}, util::SourceSpan sp = {}) noexcept : Expr{std::move(sp)}, elts{std::move(elts)} { }
    // explicit Map(std::unordered_map<ExprPtr, ExprPtr> elts = {}) noexcept : elements{std::move(elts)} {}

    std::string stringify(const size_t indent = 0) const override {
        if (elts.empty()) return "()";
        if (elts.size() == 1) return '(' + elts.front()->stringify(indent + 4) + ",)";


        std::string s = '(' + elts.front()->stringify(indent + 4);
        for (const auto& elt : elts | std::views::drop(1)) {
            s +=  + ", " + elt->stringify(indent + 4);
        }

        s += ')';

        return s;
    }

    bool matches(Expr* code, Captures& captures) const override {
        const auto* that = dynamic_cast<const Tuple*>(code);
        if (not that or elts.size() != that->elts.size()) return false;

        for (const auto& [mine, theirs] : std::views::zip(elts, that->elts))
            if (not mine->matches(theirs.get(), captures)) return false;

        return true;
    }

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify()
            or std::ranges::any_of(
                elts, 
                [sv] (const auto& e) { return e->involvesName(sv); }
            );
    }

    ExprPtr fillWith(const Fillings& fillings) const override {
        auto copy = std::make_shared<Tuple>(*this);
        for (auto& e : copy->elts) e = splice(e, fillings);
        return copy;
    }

    Node variant() override { return this; }
};


struct Expansion : Expr {
    ExprPtr pack;

    explicit Expansion(ExprPtr p, util::SourceSpan sp = {}) noexcept : Expr{std::move(sp)}, pack{std::move(p)} { }


    std::string stringify(const size_t indent = 0) const override {
        return pack->stringify(indent) + "...";
    }

    bool matches(Expr* code, Captures& captures) const override {
        const auto* that = dynamic_cast<const Expansion*>(code);
        return that and pack->matches(that->pack.get(), captures);
    }

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify() or pack->involvesName(sv);
    }

    ExprPtr fillWith(const Fillings& fillings) const override {
        auto copy = std::make_shared<Expansion>(*this);
        copy->pack = splice(pack, fillings);
        return copy;
    }

    Node variant() override { return this; }
};


struct UnaryFold : Expr {
    ExprPtr pack;
    std::string op;
    bool left_to_right;

    UnaryFold(ExprPtr p, std::string o, const bool l2r, util::SourceSpan sp = {}) noexcept
    : Expr{std::move(sp)}, pack{std::move(p)}, op{std::move(o)}, left_to_right{l2r}
    { }

    std::string stringify(const size_t indent = 0) const override {
        if (left_to_right)
            return '(' + pack->stringify(indent) + ' ' + op + " ...)";
        else
            return "(... " + op + ' ' + pack->stringify(indent) + ')';
    }

    bool matches(Expr* code, Captures& captures) const override {
        const auto* that = dynamic_cast<const UnaryFold*>(code);
        return that and op == that->op and left_to_right == that->left_to_right and pack->matches(that->pack.get(), captures);
    }

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify() or pack->involvesName(sv);
    }

    ExprPtr fillWith(const Fillings& fillings) const override {
        auto copy = std::make_shared<UnaryFold>(*this);
        copy->pack = splice(pack, fillings);
        return copy;
    }

    Node variant() override { return this; }
};


struct SeparatedUnaryFold : Expr {
    ExprPtr lhs;
    ExprPtr rhs;
    std::string op1;
    std::string op2;

    SeparatedUnaryFold(ExprPtr l, ExprPtr r, std::string o1, std::string o2, util::SourceSpan sp = {}) noexcept
    : Expr{std::move(sp)}, lhs{std::move(l)}, rhs{std::move(r)}, op1{std::move(o1)}, op2{std::move(o2)}
    { }

    std::string stringify(const size_t indent = 0) const override {
        return '(' + lhs->stringify(indent) + ' ' + op1 + " ... " + op2 + ' ' + rhs->stringify(indent) + ')';
    }

    bool matches(Expr* code, Captures& captures) const override {
        const auto* that = dynamic_cast<const SeparatedUnaryFold*>(code);
        return that
            and op1 == that->op1
            and op2 == that->op2
            and lhs->matches(that->lhs.get(), captures)
            and rhs->matches(that->rhs.get(), captures);
    }

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify() or lhs->involvesName(sv) or rhs->involvesName(sv);
    }

    ExprPtr fillWith(const Fillings& fillings) const override {
        auto copy = std::make_shared<SeparatedUnaryFold>(*this);
        copy->lhs = splice(lhs, fillings);
        copy->rhs = splice(rhs, fillings);
        return copy;
    }

    Node variant() override { return this; }
};


struct BinaryFold : Expr {
    ExprPtr pack;
    ExprPtr init;
    std::string op;
    bool left_to_right;

    ExprPtr sep;


    explicit BinaryFold(ExprPtr p, ExprPtr i, std::string o, const bool l2r, ExprPtr s = nullptr, util::SourceSpan sp = {}) noexcept
    :
    Expr{std::move(sp)},
    pack{std::move(p)},
    init{std::move(i)},
    op{std::move(o)},
    left_to_right{std::move(l2r)},
    sep{std::move(s)}
    { }

    std::string stringify(const size_t indent = 0) const override {
        if (left_to_right and sep) 
            return '(' + init->stringify(indent) + ' ' + op + ' ' + pack->stringify(indent) + ' ' + op + " ... " + op + ' ' + sep->stringify(indent) + ')';

        if (left_to_right)
            return '(' + init->stringify(indent) + ' ' + op + ' ' + pack->stringify(indent) + ' ' + op + " ...)";

        if (sep)
            return '(' + sep->stringify(indent) + ' ' + op  + " ... " + op + ' ' + pack->stringify(indent) + ' ' + op + ' ' + init->stringify(indent) + ')';

        else // I know this else only attaches to the above if, but it looks aesthetically appealing
            return "(... " + op + ' ' + pack->stringify(indent) + ' ' + op + ' ' + init->stringify(indent) + ')';
    }

    bool matches(Expr* code, Captures& captures) const override {
        const auto* that = dynamic_cast<const BinaryFold*>(code);
        return that
            and op == that->op
            and left_to_right == that->left_to_right
            and pack->matches(that->pack.get(), captures)
            and init->matches(that->init.get(), captures)
            and optionalMatches(sep, that->sep, captures);
    }

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify() or pack->involvesName(sv) or init->involvesName(sv);
    }

    ExprPtr fillWith(const Fillings& fillings) const override {
        auto copy = std::make_shared<BinaryFold>(*this);
        copy->pack = splice(pack, fillings);
        copy->init = splice(init, fillings);
        copy->sep = splice(sep, fillings);
        return copy;
    }

    Node variant() override { return this; }
};


struct Assignment : Expr {
    ExprPtr lhs;
    type::TypePtr type;
    ExprPtr rhs;

    bool is_syntax;
    bool declares = false;


    Assignment(ExprPtr l, type::TypePtr t, ExprPtr r, const bool s = false, util::SourceSpan sp = {}) noexcept
    :
    Expr{std::move(sp)},
    lhs{std::move(l)},
    type{std::move(t)},
    rhs{std::move(r)},
    is_syntax{s}
    { }

    std::string stringify(const size_t indent = 0) const override {
        if (auto name = dynamic_cast<const Name*>(lhs.get()); name and not type::shouldReassign(type)) {
            return name->stringify(indent) + ": " + type->text(indent + 4) + " = " + rhs->stringify(indent + 4);
        }

        return lhs->stringify(indent) + " = " + rhs->stringify(indent + 4);
    }

    bool matches(Expr* code, Captures& captures) const override {
        const auto* that = dynamic_cast<const Assignment*>(code);
        return that
            and is_syntax == that->is_syntax
            and rhs->matches(that->rhs.get(), captures)
            and sameType(type, that->type)
            and lhs->matches(that->lhs.get(), captures);
    }

    bool involvesName(const std::string_view sv) const override {
        // todo: check the type
        return sv == stringify() or lhs->involvesName(sv) or rhs->involvesName(sv);
    }

    ExprPtr fillWith(const Fillings& fillings) const override {
        auto copy = std::make_shared<Assignment>(*this);
        copy->lhs = splice(lhs, fillings);
        copy->rhs = splice(rhs, fillings);
        return copy;
    }

    Node variant() override { return this; }
};


struct InferredAssignment : Expr {
    StringID name; // only proper names are allowed to have a type, hence not using ExprPtr
    ExprPtr rhs;


    InferredAssignment(std::string n, ExprPtr r, util::SourceSpan sp = {}) noexcept
    :
    Expr{std::move(sp)},
    name{std::move(n)},
    rhs{std::move(r)}
    { }

    std::string stringify(const size_t indent = 0) const override {
        return name.name + " := " + rhs->stringify(indent + 4);
    }

    bool matches(Expr* code, Captures& captures) const override {
        const auto* that = dynamic_cast<const InferredAssignment*>(code);
        return that and name.name == that->name.name and rhs->matches(that->rhs.get(), captures);
    }

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify() or name.name == sv or rhs->involvesName(sv);
    }

    ExprPtr fillWith(const Fillings& fillings) const override {
        auto copy = std::make_shared<InferredAssignment>(*this);
        copy->rhs = splice(rhs, fillings);
        return copy;
    }

    Node variant() override { return this; }
};



struct Hole : Expr {
    ExprPtr expr;


    explicit Hole(ExprPtr e, util::SourceSpan sp = {}) noexcept
    :
    Expr{std::move(sp)},
    expr{std::move(e)}
    { }


    std::string stringify([[maybe_unused]] const size_t indent = 0) const override {
        auto name = expr->stringify();

        if (dynamic_cast<const Name*>(expr.get())) return std::string{HOLE_PREFIX} + name;
        return std::string{HOLE_PREFIX} + '{' + name + '}';
    }

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify() or expr->involvesName(sv);
    }

    bool matches(Expr* code, Captures& captures) const override {
        auto name = expr->stringify();

        // the same hole twice (`\a + \a`) has to capture the same code both times
        if (const auto it = captures.find(name); it != captures.end()) {
            Captures none;
            return it->second->matches(code, none);
        }

        captures[name] = code;
        return true;
    }

    ExprPtr fillWith(const Fillings& fillings) const override {
        auto name = expr->stringify();

        if (not fillings.contains(name)) util::error("Name '" + name + "' not found in fillings!");

        return fillings.at(name);
    }

    Node variant() override { return this; }
};



struct Syntax : Expr {
    ExprPtr expr;
    std::vector<std::shared_ptr<Hole>> holes;
    Syntax(ExprPtr e, util::SourceSpan sp = {}) noexcept
    :
    Expr{std::move(sp)},
    expr{std::move(e)}
    { }

    std::string stringify(const size_t indent = 0) const override {
        return '`' + expr->stringify(indent + 4) + '`';
    }

    bool matches(Expr* code, Captures& captures) const override {
        const auto* that = dynamic_cast<const Syntax*>(code);
        return that and expr->matches(that->expr.get(), captures);
    }

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify() or expr->involvesName(sv);
    }

    ExprPtr fillWith(const Fillings& fillings) const override {
        auto copy = std::make_shared<Syntax>(*this);
        copy->expr = splice(expr, fillings);
        return copy;
    }

    Node variant() override { return this; }
};



namespace unpack {

struct Pattern {
    type::TypePtr type;
    ExprPtr value;

    Pattern() noexcept = default;
    Pattern(type::TypePtr type, ExprPtr value) noexcept
    : type{std::move(type)}, value{std::move(value)} { }

    virtual ~Pattern() = default;
};
using PatternPtr = std::shared_ptr<Pattern>;
using Patterns = std::vector<PatternPtr>;

struct Expr : Pattern {
    ExprPtr expr;
    Expr(ExprPtr e, type::TypePtr type = nullptr, ExprPtr value = nullptr) noexcept
    : Pattern{std::move(type), std::move(value)}, expr{std::move(e)} { }
};

struct List : Pattern {
    Patterns patterns;
    List(Patterns p, type::TypePtr type = nullptr, ExprPtr value = nullptr) noexcept
    : Pattern{std::move(type), std::move(value)}, patterns{std::move(p)} { }

    template <typename... Ts>
    requires (std::same_as<std::remove_cvref_t<Ts>, PatternPtr> and ...)
    static std::shared_ptr<List> with(Ts&&... members) {
        Patterns patterns;
        (..., patterns.push_back(std::forward<Ts>(members)));
        return std::make_unique<List>(std::move(patterns));
    }
};

// struct Map : Pattern {
//     std::vector<std::pair<PatternPtr, PatternPtr>> patterns;
//     Map(std::vector<std::pair<PatternPtr, PatternPtr>> p, type::TypePtr type = nullptr, ExprPtr value = nullptr) noexcept
//     : Pattern{std::move(type), std::move(value)}, patterns{std::move(p)} { }


//     template <typename... Ts>
//     requires (std::same_as<std::remove_cvref_t<Ts>, std::pair<PatternPtr, PatternPtr>> and ...)
//     static std::shared_ptr<Map> with(Ts&&... members) {
//         std::vector<std::pair<PatternPtr, PatternPtr>> patterns;
//         (..., patterns.push_back(std::forward<Ts>(members)));
//         return std::make_unique<Map>(std::move(patterns));
//     }
// };

struct Pack : Pattern {
    // ExprPtr expr;
    // Pack() = default;
    // Pack(ExprPtr e, type::TypePtr type = nullptr, ExprPtr value = nullptr) noexcept
    // : Pattern{std::move(type), std::move(value)}, expr{std::move(e)} { }

    PatternPtr pattern;
    Pack(PatternPtr pat = nullptr) noexcept
    : pattern{std::move(pat)} { }
};



struct Syntax : Pattern {
    ExprPtr expr;
    std::vector<std::shared_ptr<Hole>> holes;

    Syntax(ExprPtr e, std::vector<std::shared_ptr<Hole>> hs) noexcept
    : expr{std::move(e)}, holes{std::move(hs)} { }
};


inline std::string stringifyPattern(const unpack::Pattern *pattern, const size_t indent = 0) {
    std::string s;
    if (auto expr = dynamic_cast<const unpack::Expr*>(pattern)) {
        if (expr->expr)
            s = expr->expr->stringify();
    }
    else if (auto list = dynamic_cast<const unpack::List*>(pattern)) {
        s += '{';

        for (const auto& pat : list->patterns)
            s += stringifyPattern(pat.get(), indent + 4) + ", ";


        // removing the trailing comma
        if (not list->patterns.empty()) {
            s.pop_back();
            s.pop_back();
        }

        s += '}';
    }
    // else if (auto map = dynamic_cast<const unpack::Map*>(pattern)) {
    //     s += '{';
    //     for (const auto& [key, value] : map->patterns) {
    //         s +=
    //             stringifyPattern(key  .get(), indent + 4)
    //             + ": " +
    //             stringifyPattern(value.get(), indent + 4)
    //             + ", ";
    //     }

    //     // removing the trailing comma
    //     s.pop_back();
    //     s.pop_back();

    //     s += '}';
    // }
    else if (auto pack = dynamic_cast<const unpack::Pack*>(pattern)) {
        s += "...";

        // if (pack->expr) s += pack->expr->stringify(indent + 4);
        if (pack->pattern) s += stringifyPattern(pack->pattern.get(), indent + 4);
    }
    else if (auto syn = dynamic_cast<const unpack::Syntax*>(pattern)) {
        s = '`' + syn->expr->stringify(indent) + '`';
    }
    else util::error();


    if (pattern->type ) s += "`:` "  + pattern->type ->text     (indent + 4);
    if (pattern->value) s += " `=` " + pattern->value->stringify(indent + 4);

    return s;
}




inline bool patternMatches(const Pattern *mine, const Pattern *theirs, Captures& captures) {
    if (not mine or not theirs) return not mine and not theirs;

    if (not sameType(mine->type, theirs->type)) return false;
    if (not optionalMatches(mine->value, theirs->value, captures)) return false;

    if (auto e = dynamic_cast<const Expr*>(mine)) {
        const auto t = dynamic_cast<const Expr*>(theirs);
        return t and optionalMatches(e->expr, t->expr, captures);
    }

    if (auto list = dynamic_cast<const List*>(mine)) {
        const auto t = dynamic_cast<const List*>(theirs);
        if (not t or list->patterns.size() != t->patterns.size()) return false;

        for (const auto& [p1, p2] : std::views::zip(list->patterns, t->patterns))
            if (not patternMatches(p1.get(), p2.get(), captures)) return false;

        return true;
    }

    if (auto pack = dynamic_cast<const Pack*>(mine)) {
        const auto t = dynamic_cast<const Pack*>(theirs);
        return t and patternMatches(pack->pattern.get(), t->pattern.get(), captures);
    }

    if (auto syn = dynamic_cast<const Syntax*>(mine)) {
        const auto t = dynamic_cast<const Syntax*>(theirs);
        return t and syn->expr->matches(t->expr.get(), captures);
    }

    return false;
}


inline PatternPtr splicePattern(const Pattern *pattern, const Fillings& fillings) {
    if (not pattern) return nullptr;

    PatternPtr copy;

    if (auto e = dynamic_cast<const Expr*>(pattern)) {
        auto c = std::make_shared<Expr>(*e);
        c->expr = splice(e->expr, fillings);
        copy = std::move(c);
    }
    else if (auto list = dynamic_cast<const List*>(pattern)) {
        auto c = std::make_shared<List>(*list);
        for (auto& sub : c->patterns) sub = splicePattern(sub.get(), fillings);
        copy = std::move(c);
    }
    else if (auto pack = dynamic_cast<const Pack*>(pattern)) {
        auto c = std::make_shared<Pack>(*pack);
        c->pattern = splicePattern(pack->pattern.get(), fillings);
        copy = std::move(c);
    }
    else if (auto code = dynamic_cast<const Syntax*>(pattern)) {
        copy = std::make_shared<Syntax>(*code); // its holes are its own
    }
    else util::error();

    copy->value = splice(pattern->value, fillings);
    return copy;
}


inline bool patternInvolves(const unpack::Pattern *pattern, const std::string_view sv) {

    if (auto expr = dynamic_cast<const unpack::Expr*>(pattern)) {
        return expr->expr->involvesName(sv);
    }
    else if (auto list = dynamic_cast<const unpack::List*>(pattern)) {
        for (const auto& pat : list->patterns)
            if (patternInvolves(pat.get(), sv)) return true;
    }
    // else if (auto map = dynamic_cast<const unpack::Map*>(pattern)) {
    //     for (const auto& [key, value] : map->patterns) {
    //         if (patternInvolves(key  .get(), sv)) return true;
    //         if (patternInvolves(value.get(), sv)) return true;
    //     }
    // }
    else if (auto pack = dynamic_cast<const unpack::Pack*>(pattern)) {
        // return pack->expr->involvesName(sv);
        if (pack->pattern) return patternInvolves(pack->pattern.get(), sv);
    }
    else if (auto syn = dynamic_cast<const unpack::Syntax*>(pattern)) {
        return syn->expr->involvesName(sv);
    }
    else util::error();

    return false;
}


inline void forEach(const Pattern *pattern, const auto& fn) {
    fn(pattern);

    if (auto list = dynamic_cast<const unpack::List*>(pattern))
        for (const auto& sub : list->patterns) forEach(sub.get(), fn);
}

inline void forEachName(const Pattern *pattern, const auto& fn) {
    if (auto e = dynamic_cast<const unpack::Expr*>(pattern)) {
        if (e->expr) fn(e->expr);
        return;
    }

    if (auto p = dynamic_cast<const unpack::Pack*>(pattern)) {
        if (p->pattern) forEachName(p->pattern.get(), fn);
        return;
    }

    if (auto l = dynamic_cast<const unpack::List*>(pattern))
        for (const auto& sub : l->patterns) forEachName(sub.get(), fn);
}

} // namespace unpack



struct Unpackment : Expr {
    // guranteed to have at leats one element
    unpack::PatternPtr pattern;
    type::TypePtr type;
    ExprPtr rhs;

    Unpackment(unpack::PatternPtr p, type::TypePtr type, ExprPtr r, util::SourceSpan sp = {}) noexcept
    :
    expr::Expr{std::move(sp)}, // `Expr : Pattern` shadows expr::Expr, hence the qualified name
    pattern{std::move(p)},
    type{std::move(type)},
    rhs{std::move(r)}
    { }


    std::string stringify(const size_t indent = 0) const override {
        std::string s = stringifyPattern(pattern.get(), indent + 4);

        if (not type::shouldReassign(type)) {
            s += ": " + type->text(indent + 4);
        }

        s += " = " + rhs->stringify(indent + 4);
        return s;
    }

    bool matches(Expr* code, Captures& captures) const override {
        const auto* that = dynamic_cast<const Unpackment*>(code);
        return that
            and unpack::patternMatches(pattern.get(), that->pattern.get(), captures)
            and sameType(type, that->type)
            and rhs->matches(that->rhs.get(), captures);
    }

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify()
            or patternInvolves(pattern.get(), sv)
            or rhs->involvesName(sv);
    }

    ExprPtr fillWith(const Fillings& fillings) const override {
        auto copy = std::make_shared<Unpackment>(*this);
        copy->pattern = unpack::splicePattern(pattern.get(), fillings);
        copy->rhs = splice(rhs, fillings);
        return copy;
    }

    Node variant() override { return this; }
};



// Unpack Assignment
struct InferredUnpackment : Expr {
    // guranteed to have at leats one element
    unpack::PatternPtr pattern;
    ExprPtr rhs;

    InferredUnpackment(unpack::PatternPtr p, ExprPtr r, util::SourceSpan sp = {}) noexcept
    :
    expr::Expr{std::move(sp)}, // `Expr : Pattern` shadows expr::Expr, hence the qualified name
    pattern{std::move(p)},
    rhs{std::move(r)}
    { }


    std::string stringify(const size_t indent = 0) const override {
        return stringifyPattern(pattern.get(), indent + 4)
            + " := "
            + rhs->stringify(indent + 4);
    }

    bool matches(Expr* code, Captures& captures) const override {
        const auto* that = dynamic_cast<const InferredUnpackment*>(code);
        return that
            and unpack::patternMatches(pattern.get(), that->pattern.get(), captures)
            and rhs->matches(that->rhs.get(), captures);
    }

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify()
            or patternInvolves(pattern.get(), sv)
            or rhs->involvesName(sv);
    }

    ExprPtr fillWith(const Fillings& fillings) const override {
        auto copy = std::make_shared<InferredUnpackment>(*this);
        copy->pattern = unpack::splicePattern(pattern.get(), fillings);
        copy->rhs = splice(rhs, fillings);
        return copy;
    }

    Node variant() override { return this; }
};



struct Class : Expr {
    std::vector<std::tuple<Name, type::TypePtr, ExprPtr>> fields;

    explicit Class(std::vector<std::tuple<Name, type::TypePtr, ExprPtr>> f, util::SourceSpan sp = {}) noexcept
    :
    Expr{std::move(sp)},
    fields{std::move(f)}
    { }


    std::string stringify(const size_t indent = 0) const override {

        std::string s = "class {\n";

        const std::string space(indent + 4, ' ');
        for (const auto& [name, type, expr] : fields) {
            s += space + name.stringify() + ": ";

            if (type::shouldReassign(type)) s += "Any";
            else                            s += type->text(indent + 4);

            s += " = " + expr->stringify(indent + 4) + ";\n";
        }


        return s + std::string(indent, ' ') + "}";
    }

    bool matches(Expr* code, Captures& captures) const override {
        const auto* that = dynamic_cast<const Class*>(code);
        if (not that or fields.size() != that->fields.size()) return false;


        for (const auto& [mine, theirs] : std::views::zip(fields, that->fields)) {
            const auto& [my_name, my_type, my_expr] = mine;
            const auto& [his_name, his_type, his_expr] = theirs;

            if (my_name.name != his_name.name)                  return false;
            if (not sameType(my_type, his_type))                return false;
            if (not my_expr->matches(his_expr.get(), captures)) return false;
        }

        return true;
    }

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify() or std::ranges::any_of(fields,
            [sv] (const auto& e) {
                const auto& [_, __, expr] = e;
                return expr->involvesName(sv);
            }
        );
    }

    ExprPtr fillWith(const Fillings& fillings) const override {
        auto copy = std::make_shared<Class>(*this);
        for (auto& [_, __, value] : copy->fields) value = splice(value, fillings);
        return copy;
    }

    Node variant() override { return this; }
};


struct Union : Expr {
    std::vector<type::TypePtr> types;

    Union(std::vector<type::TypePtr> ts, util::SourceSpan sp = {}) noexcept
    :
    Expr{std::move(sp)},
    types{std::move(ts)}
    { }


    std::string stringify(const size_t indent = 0) const override {
        std::string s = "union {\n";

        const std::string space(indent + 4, ' ');
        for (const auto& type : types) {
            s += space + type->text(indent + 4) + ";\n";
        }

        return s + std::string(indent, ' ') + "}";
    }

    bool matches(Expr* code, [[maybe_unused]] Captures& captures) const override {
        const auto* that = dynamic_cast<const Union*>(code);
        if (not that or types.size() != that->types.size()) return false;

        for (const auto& [mine, theirs] : std::views::zip(types, that->types))
            if (not sameType(mine, theirs)) return false;
            // if (not mine->matches(theirs.get(), captures)) return false;

        return true;
    }

    bool involvesName(const std::string_view name) const override {
        const type::ExprType t{std::make_shared<expr::Name>(std::string{name}, util::SourceSpan{})};

        for (const auto& type : types)
            if (type->involvesT(t)) return true;

         return false;
    }

    Node variant() override { return this; }
};



struct Match : Expr {
    struct Case {
        unpack::PatternPtr pattern;
        ExprPtr guard;
        ExprPtr body;
    };

    ExprPtr expr;
    std::vector<Case> cases;

    Match(ExprPtr e, std::vector<Case> cs, util::SourceSpan sp = {}) noexcept
    :
    Expr{std::move(sp)},
    expr{std::move(e)},
    cases{std::move(cs)}
    { }


    std::string stringify(const size_t indent = 0) const override {
        std::string s = "match " + expr->stringify(indent) + " {\n";

        for (const std::string space(indent + 4, ' '); const auto& kase : cases) {
            s += space;

            s += unpack::stringifyPattern(kase.pattern.get(), indent + 4);

            if (kase.guard) s += " & " + kase.guard->stringify(indent + 4);
            s += " => " + kase.body->stringify(indent + 4) + ";\n";
        }


        return s + std::string(indent, ' ') + "}";
    }

    bool matches(Expr* code, Captures& captures) const override {
        const auto* that = dynamic_cast<const Match*>(code);
        if (not that or cases.size() != that->cases.size() or not expr->matches(that->expr.get(), captures)) return false;

        for (const auto& [mine, theirs] : std::views::zip(cases, that->cases)) {
            if (not unpack::patternMatches(mine.pattern.get(), theirs.pattern.get(), captures)) return false;
            if (not optionalMatches(mine.guard, theirs.guard, captures)) return false;
            if (not mine.body->matches(theirs.body.get(), captures)) return false;
        }

        return true;
    }

    ExprPtr fillWith(const Fillings& fillings) const override {
        auto copy = std::make_shared<Match>(*this);
        copy->expr = splice(expr, fillings);

        for (auto& kase : copy->cases) {
            kase.pattern = unpack::splicePattern(kase.pattern.get(), fillings);
            kase.guard   = splice(kase.guard, fillings);
            kase.body    = splice(kase.body , fillings);
        }

        return copy;
    }


    // todo: fix this by checking every expression. Even inside Single
    bool involvesName(const std::string_view sv) const override {
        return sv == stringify() or expr->involvesName(sv);
    }

    Node variant() override { return this; }
};


struct Type : Expr {
    type::TypePtr type;

    explicit Type(type::TypePtr t, util::SourceSpan sp = {}) noexcept
    :
    Expr{std::move(sp)},
    type{std::move(t)}
    { }

    std::string stringify(const size_t indent = 0) const override { return type->text(indent); }

    bool matches(Expr* code, Captures&) const override {
        const auto* that = dynamic_cast<const Type*>(code);
        return that and sameType(type, that->type);
    }

    bool involvesName(const std::string_view) const override {
        return false; // temp
    }

    Node variant() override { return this; }
};



struct Loop : Expr {
    ExprPtr kind;

    // changes of `var` over the years...lol
    // ExprPtr var;
    // StringID var;
    unpack::PatternPtr var;

    ExprPtr body;
    ExprPtr els;

    Loop(ExprPtr b, unpack::PatternPtr v = nullptr, ExprPtr k = nullptr, ExprPtr e = nullptr, util::SourceSpan sp = {}) noexcept
    :
    Expr{std::move(sp)},
    kind{std::move(k)},
    var{std::move(v)},
    body{std::move(b)},
    els{std::move(e)}
    { }


    std::string stringify(const size_t indent = 0) const override {
        std::string s = "loop ";

        if (var) s += unpack::stringifyPattern(var.get(), indent + 4) + (kind ? " : " : ": ");

        if (kind) s += kind->stringify(indent + 4) + ' ';

        // s += " {\n";

        s += body->stringify(indent + 4);

        // s += "\n" + std::string(indent, ' ') + "}";

        if (els) s += " => " + els->stringify();

        return s;
    }


    bool matches(Expr* code, Captures& captures) const override {
        const auto* that = dynamic_cast<const Loop*>(code);

        return that
            and unpack::patternMatches(var.get(), that->var.get(), captures)
            and optionalMatches(kind, that->kind, captures)
            and body->matches(that->body.get(), captures)
            and optionalMatches(els, that->els, captures);

        // if (not that) return false;

        // if (not var  != not that->var ) return false;
        // if (not kind != not that->kind) return false;

        // // body can never be null!
        // // if (not body != not that->body) return false;

        // if (not els  != not that->els ) return false;


        // if (var  and not unpack::patternMatches(var.get(), that->var.get(), captures)) return false;

        // if (kind and not kind->matches(that->kind.get(), captures)) return false;
        // if (els  and not els ->matches(that->els .get(), captures)) return false;

        // return body->matches(that->body.get(), captures);
    }

    ExprPtr fillWith(const Fillings& fillings) const override {
        auto copy = std::make_shared<Loop>(*this);
        copy->var  = unpack::splicePattern(var.get(), fillings);
        copy->kind = splice(kind, fillings);
        copy->body = splice(body, fillings);
        copy->els  = splice(els , fillings);
        return copy;
    }


    bool involvesName(const std::string_view sv) const override {
        if (sv == stringify()) return true;
        if (var and unpack::patternInvolves(var.get(), sv)) return true;
        if (kind and kind->involvesName(sv)) return true;
        if (body->involvesName(sv)) return true;
        if (els and els->involvesName(sv)) return true;

        return false;
    }

    Node variant() override { return this; }
};


struct Break : Expr {
    ExprPtr expr;

    explicit Break(ExprPtr e = nullptr, util::SourceSpan sp = {}) noexcept
    :
    Expr{std::move(sp)},
    expr{std::move(e)}
    { }

    std::string stringify(const size_t indent = 0) const override {
        if (expr) return "break " + expr->stringify(indent + 4);

        return "break";
    }

    bool matches(Expr* code, Captures& captures) const override {
        const auto* that = dynamic_cast<const Break*>(code);
        return that and optionalMatches(expr, that->expr, captures);
    }

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify() or expr->involvesName(sv);
    }

    ExprPtr fillWith(const Fillings& fillings) const override {
        auto copy = std::make_shared<Break>(*this);
        copy->expr = splice(expr, fillings);
        return copy;
    }

    Node variant() override { return this; }
};


struct Continue : Expr {
    ExprPtr expr;

    explicit Continue(ExprPtr e = nullptr, util::SourceSpan sp = {}) noexcept
    :
    Expr{std::move(sp)},
    expr{std::move(e)}
    { }

    std::string stringify(const size_t indent = 0) const override {
        if (expr) return "continue " + expr->stringify(indent + 4);

        return "continue";
    }

    bool matches(Expr* code, Captures& captures) const override {
        const auto* that = dynamic_cast<const Continue*>(code);
        return that and optionalMatches(expr, that->expr, captures);
    }

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify() or expr->involvesName(sv);
    }

    ExprPtr fillWith(const Fillings& fillings) const override {
        auto copy = std::make_shared<Continue>(*this);
        copy->expr = splice(expr, fillings);
        return copy;
    }

    Node variant() override { return this; }
};


struct ListComp : Expr {
    unpack::PatternPtr var;
    ExprPtr kind;
    ExprPtr guard;
    ExprPtr body;

    ListComp(ExprPtr b, unpack::PatternPtr v = nullptr, ExprPtr k = nullptr, ExprPtr g = nullptr, util::SourceSpan sp = {}) noexcept
    :
    Expr{std::move(sp)},
    var{std::move(v)},
    kind{std::move(k)},
    guard{std::move(g)},
    body{std::move(b)}
    { }


    std::string stringify(const size_t indent = 0) const override {
        std::string s = "{loop ";

        if (var ) s += unpack::stringifyPattern(var.get(), indent + 4);
        if (kind) s += ' ' + kind->stringify(indent + 4);
        if (guard) s += ", " + guard->stringify(indent + 4);

        return s + " => " + body->stringify(indent + 4) + "}";
    }


    bool matches(Expr* code, Captures& captures) const override {
        const auto* that = dynamic_cast<const ListComp*>(code);

        return that
            and unpack::patternMatches(var.get(), that->var.get(), captures)
            and optionalMatches(kind , that->kind , captures)
            and optionalMatches(guard, that->guard, captures)
            and body->matches(that->body.get(), captures);

        // if (not that) return false;

        // if (not var   != not that->var  ) return false;
        // if (not kind  != not that->kind ) return false;
        // if (not guard != not that->guard) return false;


        // if (var  and not unpack::patternMatches(var.get(), that->var.get(), captures)) return false;

        // if (kind and not kind->matches(that->kind.get(), captures)) return false;

        // return body->matches(that->body.get(), captures);
    }

    ExprPtr fillWith(const Fillings& fillings) const override {
        auto copy = std::make_shared<ListComp>(*this);
        copy->var   = unpack::splicePattern(var.get(), fillings);
        copy->kind  = splice(kind , fillings);
        copy->guard = splice(guard, fillings);
        copy->body  = splice(body , fillings);
        return copy;
    }



    bool involvesName(const std::string_view sv) const override {
        return sv == stringify()
            or (var and sv == unpack::stringifyPattern(var.get()))
            or (kind and kind->involvesName(sv))
            or (guard and guard->involvesName(sv))
            or body->involvesName(sv);
    }

    Node variant() override { return this; }
};

struct MapComp : Expr {
    unpack::PatternPtr var;
    ExprPtr kind;
    ExprPtr guard;
    ExprPtr body1;
    ExprPtr body2;

    MapComp(ExprPtr b1, ExprPtr b2, unpack::PatternPtr v = nullptr, ExprPtr k = nullptr, ExprPtr g = nullptr, util::SourceSpan sp = {}) noexcept
    :
    Expr{std::move(sp)},
    var{std::move(v)},
    kind{std::move(k)},
    guard{std::move(g)},
    body1{std::move(b1)},
    body2{std::move(b2)}
    { }


    std::string stringify(const size_t indent = 0) const override {
        std::string s = "{loop ";

        if (var  ) s += unpack::stringifyPattern(var.get(), indent + 4);
        if (kind ) s += ' ' + kind->stringify(indent + 4);
        if (guard) s += ", " + guard->stringify(indent + 4);

        return s + " => " + body1->stringify(indent + 4) + ": " + body2->stringify() + "}";
    }


    bool matches(Expr* code, Captures& captures) const override {
        const auto* that = dynamic_cast<const MapComp*>(code);

        return that
            and unpack::patternMatches(var.get(), that->var.get(), captures)
            and optionalMatches(kind , that->kind , captures)
            and optionalMatches(guard, that->guard, captures)
            and body1->matches(that->body1.get(), captures)
            and body2->matches(that->body2.get(), captures);

        // if (not that) return false;

        // if (not var   != not that->var  ) return false;
        // if (not kind  != not that->kind ) return false;
        // if (not guard != not that->guard) return false;


        // if (var  and not unpack::patternMatches(var.get(), that->var.get(), captures)) return false;

        // if (kind and not kind->matches(that->kind.get(), captures)) return false;

        // return body1->matches(that->body1.get(), captures) and body2->matches(that->body2.get(), captures);
    }

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify()
            or (var and sv == unpack::stringifyPattern(var.get()))
            or (kind and kind->involvesName(sv))
            or (guard and guard->involvesName(sv))
            or body1->involvesName(sv)
            or body2->involvesName(sv);
    }

    ExprPtr fillWith(const Fillings& fillings) const override {
        auto copy = std::make_shared<MapComp>(*this);
        copy->var   = unpack::splicePattern(var.get(), fillings);
        copy->kind  = splice(kind , fillings);
        copy->guard = splice(guard, fillings);
        copy->body1 = splice(body1, fillings);
        copy->body2 = splice(body2, fillings);
        return copy;
    }

    Node variant() override { return this; }
};


struct Access : Expr {
    ExprPtr var;
    std::string name;

    Access(ExprPtr v, std::string n, util::SourceSpan sp = {}) noexcept
    :
    Expr{std::move(sp)},
    var{std::move(v)},
    name{std::move(n)}
    { }

    std::string stringify(const size_t indent = 0) const override {
        return var->stringify(indent) + '.' + name;
    }

    bool matches(Expr* code, Captures& captures) const override {
        const auto* that = dynamic_cast<const Access*>(code);
        return that and name == that->name and var->matches(that->var.get(), captures);
    }

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify() or var->involvesName(sv);
    }

    ExprPtr fillWith(const Fillings& fillings) const override {
        auto copy = std::make_shared<Access>(*this);
        copy->var = splice(var, fillings);
        return copy;
    }

    Node variant() override { return this; }
};




struct Namespace : Expr {
    std::string name;
    std::vector<ExprPtr> space;


    explicit Namespace(std::string n, std::vector<ExprPtr> exprs, util::SourceSpan sp = {}) noexcept
    :
    Expr{std::move(sp)},
    name{std::move(n)},
    space{std::move(exprs)}
    { }

    std::string stringify(const size_t indent = 0) const override {
        std::string s = "space " + name + " {\n";

        for (const std::string spacing(indent + 4, ' '); const auto& expr : space) {
            s += spacing + expr->stringify(indent + 4) + ";\n";
        }

        return s + std::string(indent, ' ') + "}";
    }

    bool matches(Expr* code, Captures& captures) const override {
        const auto* that = dynamic_cast<const Namespace*>(code);
        if (not that or name != that->name or space.size() != that->space.size()) return false;

        for (const auto& [mine, theirs] : std::views::zip(space, that->space))
            if (not mine->matches(theirs.get(), captures)) return false;

        return true;
    }

    bool involvesName(const std::string_view) const override {
        return false; // temp
    }

    ExprPtr fillWith(const Fillings& fillings) const override {
        auto copy = std::make_shared<Namespace>(*this);
        for (auto& e : copy->space) e = splice(e, fillings);
        return copy;
    }

    Node variant() override { return this; }
};


struct Use : Expr {
    bool global;
    // last name is not a space
    std::vector<std::string> spaces;
    StringID name;

    Use(bool g, std::vector<std::string> ns, std::string n, util::SourceSpan sp = {}) noexcept
    :
    Expr{std::move(sp)},
    global{g},
    spaces{std::move(ns)},
    name{std::move(n)}
    { }


    std::string stringify(const size_t = 0) const override {
        std::string s;

        for (const auto& space : spaces)
            s += space + "::";

        return (global ? "use ::" : "use " ) + s + name.name;
    }

    bool matches(Expr* code, Captures&) const override {
        const auto* that = dynamic_cast<const Use*>(code);
        return that and global == that->global and name.name == that->name.name and spaces == that->spaces;
    }

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify();
    }

    Node variant() override { return this; }
};


struct UseSpace : Expr {
    bool global;
    std::vector<std::string> spaces;
    ssize_t last_item_id;
    bool pull_ops;


    UseSpace(bool g, std::vector<std::string> ns, bool ops, util::SourceSpan sp = {}) noexcept
    :
    Expr{std::move(sp)},
    global{g},
    spaces{std::move(ns)},
    pull_ops{ops}
    { }


    std::string stringify(const size_t = 0) const override {
        std::string s = "use space ";

        if (global) {
            for (const auto& space : spaces)
                s += "::" + space;
        }
        else {
            s += spaces[0];
            for (const auto& space : spaces | std::views::drop(1))
                s += "::" + space;
        }

        return s + (pull_ops ? "::" : "");
    }

    bool matches(Expr* code, Captures&) const override {
        const auto* that = dynamic_cast<const UseSpace*>(code);
        return that
            and global == that->global
            and pull_ops == that->pull_ops
            and spaces == that->spaces;
    }

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify();
    }

    Node variant() override { return this; }
};


struct UseFix : Expr {
    bool global;
    std::vector<std::string> spaces;
    token::TokenKind filter;
    std::string op_name;


    UseFix(bool g, std::vector<std::string> ns, const token::TokenKind f = token::TokenKind::NONE, std::string op = "", util::SourceSpan sp = {}) noexcept
    :
    Expr{std::move(sp)},
    global{g},
    spaces{std::move(ns)},
    filter{f},
    op_name{std::move(op)}
    { }


    std::string stringify(const size_t = 0) const override {
        std::string s = "use ";
        if (filter != token::TokenKind::NONE) {
            for (const char c : std::string_view{token::stringify(filter)})
                s += tolower(c);

            s += ' ';
        }



        if (global) {
            for (const auto& space : spaces)
                s += "::" + space;
        }
        else {
            s += spaces[0];
            for (const auto& space : spaces | std::views::drop(1))
                s += "::" + space;
        }

        return s + "::" + op_name;
    }


    bool matches(Expr* code, Captures&) const override {
        const auto* that = dynamic_cast<const UseFix*>(code);
        return that
            and global == that->global
            and op_name == that->op_name
            and filter == that->filter
            and spaces == that->spaces;
    }


    bool involvesName(const std::string_view sv) const override {
        return sv == stringify();
    }

    Node variant() override { return this; }
};


struct Import : Expr {
    std::filesystem::path path;

    explicit Import(std::filesystem::path p, util::SourceSpan sp = {}) noexcept
    :
    Expr{std::move(sp)},
    path{std::move(p)}
    { }

    std::string stringify(const size_t = 0) const override {
        std::string s = path.begin()->c_str();
        if (s == "/") s = "::";

        for (const auto& p : path | std::views::drop(1)) {
            if (s == "::") s +=        p.string();
            else           s += "::" + p.string();
        }


        return "import " + s;
    }

    bool matches(Expr* code, Captures&) const override {
        const auto* that = dynamic_cast<const Import*>(code);
        return that and path == that->path;
    }

    bool involvesName(const std::string_view) const override { return false; }

    Node variant() override { return this; }
};


struct SpaceAccess : Expr {
    bool global;
    std::vector<std::string> spaces;
    StringID name;


    SpaceAccess(bool g, std::vector<std::string> s, std::string n, util::SourceSpan sp = {}) noexcept
    :
    Expr{std::move(sp)},
    global{g},
    spaces{std::move(s)},
    name{std::move(n)}
    { }

    std::string stringify(const size_t = 0) const override {

        std::string s;

        for (const auto& sp : spaces)
            s += sp + "::";

        return (global ? "::" : "") + s + name.name;
    }

    bool matches(Expr* code, Captures&) const override {
        const auto* that = dynamic_cast<const SpaceAccess*>(code);
        return that and global == that->global and spaces == that->spaces and name.name == that->name.name;
    }

    bool involvesName(const std::string_view) const override {
        return false; // qualified names don't involve other names!
    }

    Node variant() override { return this; }
};


struct Grouping : Expr {
    ExprPtr expr;
    Grouping(ExprPtr e, util::SourceSpan sp = {}) noexcept
    :
    Expr{std::move(sp)},
    expr{std::move(e)}
    { }

    std::string stringify(const size_t indent = 0) const override {
        // return '(' + expr->stringify(indent) + ')';
        return expr->stringify(indent);
    }

    bool matches(Expr* code, Captures& captures) const override {
        return code and expr->matches(code, captures); // skip the parens
        // const auto* that = dynamic_cast<const Grouping*>(code);
        // return that and expr->matches(that->expr.get(), captures);
    }

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify() or expr->involvesName(sv);
    }

    ExprPtr fillWith(const Fillings& fillings) const override {
        auto copy = std::make_shared<Grouping>(*this);
        copy->expr = splice(expr, fillings);
        return copy;
    }

    Node variant() override { return this; }
};



struct UnaryOp : Expr {
    std::string op;
    ExprPtr expr;


    UnaryOp(std::string o, ExprPtr e, util::SourceSpan sp = {}) noexcept
    :
    Expr{std::move(sp)},
    op{std::move(o)},
    expr{std::move(e)}
    { }

    std::string stringify(const size_t indent = 0) const override {
        return '(' + op + ' ' + expr->stringify(indent) + ')';
    }

    bool matches(Expr* code, Captures& captures) const override {
        const auto* that = dynamic_cast<const UnaryOp*>(code);
        return that and op == that->op and expr->matches(that->expr.get(), captures);
    }

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify() or expr->involvesName(sv);
    }

    ExprPtr fillWith(const Fillings& fillings) const override {
        auto copy = std::make_shared<UnaryOp>(*this);
        copy->expr = splice(expr, fillings);
        return copy;
    }

    Node variant() override { return this; }
};



struct BinOp : Expr {
    ExprPtr lhs;
    std::string op;
    ExprPtr rhs;


    BinOp(ExprPtr e1, std::string o, ExprPtr e2, util::SourceSpan sp = {}) noexcept
    :
    Expr{std::move(sp)},
    lhs{std::move(e1)},
    op{std::move(o)},
    rhs{std::move(e2)}
    { }

    std::string stringify(const size_t indent = 0) const override {
        return '(' + lhs->stringify(indent) + ' ' + op + ' ' + rhs->stringify(indent) + ')';
    }

    bool matches(Expr* code, Captures& captures) const override {
        const auto* that = dynamic_cast<const BinOp*>(code);
        return that and op == that->op and lhs->matches(that->lhs.get(), captures) and rhs->matches(that->rhs.get(), captures);
    }

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify() or lhs->involvesName(sv) or rhs->involvesName(sv);
    }

    ExprPtr fillWith(const Fillings& fillings) const override {
        auto copy = std::make_shared<BinOp>(*this);
        copy->lhs = splice(lhs, fillings);
        copy->rhs = splice(rhs, fillings);
        return copy;
    }

    Node variant() override { return this; }
};


struct PostOp : Expr {
    std::string op;
    ExprPtr expr;


    PostOp(std::string o, ExprPtr e, util::SourceSpan sp = {}) noexcept
    :
    Expr{std::move(sp)},
    op{std::move(o)},
    expr{std::move(e)}
    { }

    std::string stringify(const size_t indent = 0) const override {
        return '(' + expr->stringify(indent) + ' ' + op + ')';
    }

    bool matches(Expr* code, Captures& captures) const override {
        const auto* that = dynamic_cast<const PostOp*>(code);
        return that and op == that->op and expr->matches(that->expr.get(), captures);
    }

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify() or expr->involvesName(sv);
    }

    ExprPtr fillWith(const Fillings& fillings) const override {
        auto copy = std::make_shared<PostOp>(*this);
        copy->expr = splice(expr, fillings);
        return copy;
    }

    Node variant() override { return this; }
};



struct CircumOp : Expr {
    std::string op1;
    std::string op2;
    ExprPtr expr;

    CircumOp(std::string o1, std::string o2, ExprPtr e, util::SourceSpan sp = {}) noexcept
    :
    Expr{std::move(sp)},
    op1{std::move(o1)},
    op2{std::move(o2)},
    expr{std::move(e)}
    { }

    std::string stringify(const size_t indent = 0) const override {
        return '(' + op1 + ' ' + expr->stringify(indent) + ' ' + op2 + ')';
    }

    bool matches(Expr* code, Captures& captures) const override {
        const auto* that = dynamic_cast<const CircumOp*>(code);
        return that and op1 == that->op1 and op2 == that->op2 and expr->matches(that->expr.get(), captures);
    }

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify() or expr->involvesName(sv);
    }

    ExprPtr fillWith(const Fillings& fillings) const override {
        auto copy = std::make_shared<CircumOp>(*this);
        copy->expr = splice(expr, fillings);
        return copy;
    }

    Node variant() override { return this; }
};


struct OpCall : Expr {
    std::string first;
    std::vector<std::string> rest;
    std::vector<ExprPtr> exprs;
    std::vector<bool> op_pos;

    OpCall(
        std::string f, std::vector<std::string> ops, std::vector<ExprPtr> ex,
        std::vector<bool> pos,
        util::SourceSpan sp = {}
    ) noexcept
    :
    Expr{std::move(sp)},
    first{std::move(f)},
    rest{std::move(ops)},
    exprs{std::move(ex)},
    op_pos{std::move(pos)}
    { }


    std::string stringify(const size_t indent = 0) const override {

        std::string s;
        for (ssize_t op = -1, i{}; const auto& field : op_pos) {
            if (field) {
                if (op == -1) s += first;
                else s += ' ' + rest[op];
                ++op;
            }
            else if (op == -1) s += exprs[i++]->stringify(indent) + ' ';
            else s += ' ' + exprs[i++]->stringify(indent);
        }

        return '(' + s + ')';
    }

    bool matches(Expr* code, Captures& captures) const override {
        const auto* that = dynamic_cast<const OpCall*>(code);
        if (not that or first != that->first or rest != that->rest or op_pos != that->op_pos) return false;

        for (const auto& [mine, theirs] : std::views::zip(exprs, that->exprs))
            if (not mine->matches(theirs.get(), captures)) return false;

        return true;
    }

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify() or std::ranges::any_of(exprs,
            [sv] (const auto& e) {
                return e->involvesName(sv);
            }
        );
    }

    ExprPtr fillWith(const Fillings& fillings) const override {
        auto copy = std::make_shared<OpCall>(*this);
        for (auto& e : copy->exprs) e = splice(e, fillings);
        return copy;
    }

    Node variant() override { return this; }
};



struct Call : Expr {
    ExprPtr func;

    std::unordered_map<std::string, ExprPtr> named_args;
    std::vector<ExprPtr> args;


    Call(ExprPtr function, std::unordered_map<std::string, ExprPtr> named = {}, std::vector<ExprPtr> pos = {}, util::SourceSpan sp = {}) noexcept
    :
    Expr{std::move(sp)},
    func{std::move(function)},
    named_args{std::move(named)},
    args{std::move(pos)}
    { }

    std::string stringify(const size_t indent = 0) const override {
        std::string s;
        s = func->stringify(indent) + '(';


        std::string_view comma = "";

        for (auto&& [name, arg] : named_args) {
            s += comma;
            s += name + '=' + arg->stringify(indent);
            comma = ", ";
        }

        for (const auto& arg : args) {
            s += comma;
            s += arg->stringify(indent);
            comma = ", ";
        }

        return s + ')';
    }

    bool matches(Expr* code, Captures& captures) const override {
        const auto* that = dynamic_cast<const Call*>(code);
        if (not that or args.size() != that->args.size() or named_args.size() != that->named_args.size()) return false;

        if (not func->matches(that->func.get(), captures)) return false;

        for (const auto& [mine, theirs] : std::views::zip(args, that->args))
            if (not mine->matches(theirs.get(), captures)) return false;

        for (const auto& [name, mine] : named_args) {
            const auto it = that->named_args.find(name);
            if (it == that->named_args.end() or not mine->matches(it->second.get(), captures)) return false;
        }

        return true;
    }

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify() or func->involvesName(sv)
            or std::ranges::any_of(named_args,
                [sv] (const auto& e) {
                    const auto& [_, expr] = e;
                    return expr->involvesName(sv);
                }
            )
            or std::ranges::any_of(args,
                [sv] (const auto& e) {
                    return e->involvesName(sv);
                }
            );
    }

    ExprPtr fillWith(const Fillings& fillings) const override {
        auto copy = std::make_shared<Call>(*this);
        copy->func = splice(func, fillings);
        for (auto& e : copy->args) e = splice(e, fillings);
        for (auto& [_, e] : copy->named_args) e = splice(e, fillings);
        return copy;
    }

    Node variant() override { return this; }
};



struct Closure : Expr {
    struct RegularParam {
        // std::string name;
        ExprPtr expr;
        ssize_t ID = -1;
        bool is_syntax = false;
    };

    using Param = std::variant<RegularParam, unpack::PatternPtr>;

    static bool paramMatches(const Param& p1, const Param& p2, Captures &captures) {
        const bool regular1 = std::holds_alternative<RegularParam>(p1);
        const bool regular2 = std::holds_alternative<RegularParam>(p2);

        if (regular1 != regular2) return false;

        if (not regular1) { // implied (and not regular2)
            const auto& pat1 = get<unpack::PatternPtr>(p1);
            const auto& pat2 = get<unpack::PatternPtr>(p2);

            return unpack::patternMatches(pat1.get(), pat2.get(), captures);
        }

        const auto& param1 = get<RegularParam>(p1);
        const auto& param2 = get<RegularParam>(p2);

        return param1.is_syntax == param2.is_syntax and param1.expr->matches(param2.expr.get(), captures);
    }


    std::vector<Param> params;
    std::vector<Param> pos_params;
    std::vector<Param> kw_params;
    type::FuncType type;
    std::optional<size_t> variadic_index;

    // invariant: these are the same length as the vecs above, respectively!
    std::vector<ExprPtr> defaults;
    std::vector<ExprPtr> pos_defaults;
    std::vector<ExprPtr> kw_defaults;


    std::vector<Param>         hidden_params;
    std::vector<type::TypePtr> hidden_types;
    std::vector<ExprPtr>       hidden_defaults;


    ExprPtr body;

    // vm::Chunk compiled_body;


    struct CapturedEnvs {
        value::Env env{};
        value::Env returned_env{};
        value::Env passed_env{};
    } envs;

    // whether it's a member function or not
    std::optional<value::Object> self{};

    std::vector<interp::NameSpace*> spaces;

    Closure(
        std::vector<Param> ps,
        std::vector<Param> pos,
        std::vector<Param> kw,
        type::FuncType t,
        std::optional<size_t> v_index,
        std::vector<ExprPtr> defaults,
        std::vector<ExprPtr> pos_defaults,
        std::vector<ExprPtr> kw_defaults,
        ExprPtr b,
        util::SourceSpan sp = {}
    ) noexcept :
    Expr{std::move(sp)},
    params{std::move(ps)},
    pos_params{std::move(pos)},
    kw_params{std::move(kw)},
    type{std::move(t)},
    variadic_index{std::move(v_index)},
    defaults{std::move(defaults)},
    pos_defaults{std::move(pos_defaults)},
    kw_defaults{std::move(kw_defaults)},
    body{std::move(b)}
    { }


    Closure(const Closure&) = default;
    Closure(Closure&&) = default;
    Closure& operator=(const Closure&) = default;
    Closure& operator=(Closure&&) = default;
    ~Closure() = default;


    void inSpace(const std::vector<interp::NameSpace*>& sps) {
        spaces = sps;
    }

    // const as in doesn't change params or body.
    void capture(const value::Environment& e) {
        for (const auto& [key, value] : e) {
            envs.env.env[key] = value;
        }
    }
    void returnCapture(const value::Environment& e) {
        for (const auto& [key, value] : e) {
            envs.returned_env.env[key] = value;
        }
    }
    void passedCapture(const value::Environment& e) {
        for (const auto& [key, value] : e) {
            envs.passed_env.env[key] = value;
        }
    }

    void captureOps(const Operators& ops) {
        for (const auto& [key, value] : ops) {
            envs.env.op_env[key] = value;
        }
    }

    void capturePrefixOps(const Operators& ops) {
        for (const auto& [key, value] : ops) {
            envs.env.prefix_op_env[key] = value;
        }
    }

    void captureThis(const value::Object& obj) { self = obj; }


    std::string stringify(const size_t indent = 0) const override {

        auto stringifyParam = [indent] (
            const Param& param,
            const type::TypePtr& type,
            const ExprPtr& def
        ) {
            std::string s;

            if (std::holds_alternative<RegularParam>(param)) {
                const auto& p = get<RegularParam>(param);
                s += p.is_syntax ? '`' + p.expr->stringify(indent + 4) + '`' : p.expr->stringify(indent + 4);
            }
            else s = unpack::stringifyPattern(get<unpack::PatternPtr>(param).get());


            if (not type::shouldReassign(type)) s += ": " + type->text();
            if (def) s += " = " + def->stringify();

            return s;
        };


        // 0: (a, b)    1: (a; b), (a, b;), (; a, b)    2: (a; b; c), (a; b;), (; a; b)
        const int walls = pos_params.empty() and kw_params.empty() ? 0
                        : params.empty() ? 1 : 2;


        std::string s = "(";

        size_t v_index {};
        for (
            std::string comma = "";
            const auto& [param, type, def] : std::views::zip(pos_params, type.pos_params, pos_defaults)
        ) {
            s += comma + (variadic_index == v_index++ ? "..." : "") + stringifyParam(param, type, def);
            comma = ", ";
        }

        if (walls >= 1) s += "; ";

        for (
            std::string comma = "";
            const auto& [param, type, def] : std::views::zip(params, type.params, defaults)
        ) {
            s += comma + (variadic_index == v_index++ ? "..." : "") + stringifyParam(param, type, def);
            comma = ", ";
        }

        if (walls >= 2) s += "; ";

        for (
            std::string comma = "";
            const auto& [param, type, def] : std::views::zip(kw_params, type.kw_params, kw_defaults)
        ) {
            s += comma + (variadic_index == v_index++ ? "..." : "") + stringifyParam(param, type, def);
            comma = ", ";
        }


        if (s.ends_with(' ')) s.pop_back();

        s += ")" + (type::shouldReassign(type.ret) ? "" : ": " + type.ret->text()) + " => " + body->stringify(indent);

        return s;
    }


    bool matches(Expr* code, Captures& captures) const override {
        const auto* that = dynamic_cast<const Closure*>(code);

        if (not that) return false;

        // do the fast checks first
        if (variadic_index != that->variadic_index) return false;

        if (pos_params.size() != that->pos_params.size()) return false;
        if (params    .size() != that->params    .size()) return false;
        if (kw_params .size() != that->kw_params .size()) return false;

        // sorta implied. Should I keep it? Same thign for type.params
        // if (pos_defaults.size() != that->pos_defaults.size()) return false;
        // if (defaults    .size() != that->defaults    .size()) return false;
        // if (kw_defaults .size() != that->kw_defaults .size()) return false;

        // if (hidden_params  .size() != that->hidden_params  .size()) return false;
        // if (hidden_types   .size() != that->hidden_types   .size()) return false;
        // if (hidden_defaults.size() != that->hidden_defaults.size()) return false;



        // then go in order so that captures happen in order!
        for (
            const auto& [
                mine, his,
                my_def, his_def,
                my_param_type, his_param_type
            ]
            :
            std::views::zip(
                pos_params, that->pos_params,
                pos_defaults, that->pos_defaults,
                type.pos_params, that->type.pos_params
            )
        ) {
            // quick checks!
            if (not my_def != not his_def) return false;
            if (not my_param_type != not his_param_type) return false;

            if (not paramMatches(mine, his, captures)) return false;
            if (my_def /* and his_def */ and not my_def->matches(his_def.get(), captures)) return false;
            if (sameType(my_param_type, his_param_type)) return false;
        }


        for (
            const auto& [
                mine, his,
                my_def, his_def,
                my_param_type, his_param_type
            ]
            :
            std::views::zip(
                params, that->params,
                defaults, that->defaults,
                type.params, that->type.params
            )
        ) {
            // quick checks!
            if (not my_def != not his_def) return false;
            if (not my_param_type != not his_param_type) return false;

            if (not paramMatches(mine, his, captures)) return false;
            if (my_def /* and his_def */ and not my_def->matches(his_def.get(), captures)) return false;
            if (sameType(my_param_type, his_param_type)) return false;
        }


        for (
            const auto& [
                mine, his,
                my_def, his_def,
                my_param_type, his_param_type
            ]
            :
            std::views::zip(
                kw_params, that->kw_params,
                kw_defaults, that->kw_defaults,
                type.kw_params, that->type.kw_params
            )
        ) {
            // quick checks!
            if (not my_def != not his_def) return false;
            if (not my_param_type != not his_param_type) return false;

            if (not paramMatches(mine, his, captures)) return false;
            if (my_def /* and his_def */ and not my_def->matches(his_def.get(), captures)) return false;
            if (sameType(my_param_type, his_param_type)) return false;
        }


        if (not sameType(type.ret, that->type.ret)) return false;

        return body->matches(that->body.get(), captures);
    }


    ExprPtr fillWith(const Fillings& fillings) const override {
        auto copy = std::make_shared<Closure>(*this);

        for (auto* zone : {&copy->pos_params, &copy->params, &copy->kw_params}) {
            for (auto& param : *zone) {
                if (std::holds_alternative<RegularParam>(param)) {
                    auto& p = get<RegularParam>(param);
                    const auto before = p.expr;
                    p.expr = splice(p.expr, fillings);

                    // a param that came from a hole is named by the code that filled it, so it takes that name's ID.
                    // that way, the rest of that code (usually the body) finds the param
                    if (p.expr != before and p.expr->var_ID != -1) p.ID = p.expr->var_ID;
                }
                else
                    get<unpack::PatternPtr>(param) = unpack::splicePattern(get<unpack::PatternPtr>(param).get(), fillings);
            }
        }

        for (auto* zone : {&copy->pos_defaults, &copy->defaults, &copy->kw_defaults})
            for (auto& def : *zone) def = splice(def, fillings);

        copy->body = splice(body, fillings);
        return copy;
    }


    bool involvesName(const std::string_view sv) const override {
        if (sv == stringify()) return true;

        for (const auto& [param, type] : std::views::zip(params, type.params)) {

            if (std::holds_alternative<RegularParam>(param)) {
                if (get<RegularParam>(param).expr->stringify() == sv) return true;
            }
            // else {
            //     // ???
            // }


            if (type->text() == sv) return true;
        }

        return body->involvesName(sv);
    }

    Node variant() override { return this; }
};



struct Block : Expr {
    std::vector<ExprPtr> lines;

    explicit Block(std::vector<ExprPtr> l, util::SourceSpan sp = {}) noexcept
    :
    Expr{std::move(sp)},
    lines{std::move(l)}
    { }


    std::string stringify(const size_t indent = 0) const override {
        std::string s = "{\n";

        // std::ranges::for_each(lines, &Expr::stringify);
        for(std::string space(indent + 4, ' '); const auto& line : lines) {
            s += space + line->stringify(indent + 4) + ";\n";
        }

        return s + std::string(indent, ' ') + "}";
    }

    bool matches(Expr* code, Captures& captures) const override {
        const auto* that = dynamic_cast<const Block*>(code);
        if (not that or lines.size() != that->lines.size()) return false;

        for (const auto& [mine, theirs] : std::views::zip(lines, that->lines))
            if (not mine->matches(theirs.get(), captures)) return false;

        return true;
    }

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify() or std::ranges::any_of(lines,
            [sv] (const auto& e) {
                return e->involvesName(sv);
            }
        );
    }

    ExprPtr fillWith(const Fillings& fillings) const override {
        auto copy = std::make_shared<Block>(*this);
        for (auto& e : copy->lines) e = splice(e, fillings);
        return copy;
    }

    Node variant() override { return this; }
};




struct Cascade : Expr {
    std::vector<ExprPtr> lines;

    explicit Cascade(std::vector<ExprPtr> l, util::SourceSpan sp = {}) noexcept
    :
    Expr{std::move(sp)},
    lines{std::move(l)}
    { }


    std::string stringify(const size_t indent = 0) const override {
        std::string s = dynamic_cast<expr::Assignment*>(lines.front().get())->rhs->stringify();

                                            // skip first and last elements
        for (const auto& cascader : lines | std::views::take(lines.size() - 1) | std::views::drop(1)) {
            if (auto acc = dynamic_cast<expr::Access*>(cascader.get())) {
                s += ".." + acc->name;
            }
            else if (auto call = dynamic_cast<expr::Call*>(cascader.get())) {
                auto copy = *call;
                if (auto acc = dynamic_cast<expr::Access*>(call->func.get())) {
                    copy.func = std::make_shared<Name>(acc->name);
                    s += ".." + copy.stringify(indent + 4);
                }
                else util::error();
            }
            else util::error();
        }

        return s;
    }

    bool matches(Expr* code, Captures& captures) const override {
        const auto* that = dynamic_cast<const Cascade*>(code);
        if (not that or lines.size() != that->lines.size()) return false;

        for (const auto& [mine, theirs] : std::views::zip(lines, that->lines))
            if (not mine->matches(theirs.get(), captures)) return false;

        return true;
    }

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify();
    }

    ExprPtr fillWith(const Fillings& fillings) const override {
        auto copy = std::make_shared<Cascade>(*this);
        for (auto& e : copy->lines) e = splice(e, fillings);
        return copy;
    }

    Node variant() override { return this; }
};



// defintions of operators. Usage is BinOp or UnaryOp
struct Fix : Expr {
    std::string name;

    // precedence level:
    std::string high; 
    std::string low; 

    int shift; // needed for printing

    std::vector<ExprPtr> funcs;
    // ExprPtr func;


    Fix(std::string n, std::string up, std::string down, const int s, util::SourceSpan sp = {}) noexcept
    :
    Expr{std::move(sp)},
    name{std::move(n)},
    high{std::move(up)},
    low{std::move(down)},
    shift{s}
    { }

    bool matches(Expr* code, Captures& captures) const override {
        const auto* that = dynamic_cast<const Fix*>(code);
        if (
            not that or
            type()   != that->type()   or
            OpName() != that->OpName() or
            high     != that->high     or
            low      != that->low      or
            shift    != that->shift    or
            funcs.size() != that->funcs.size()
        ) return false;

        for (const auto& [mine, theirs] : std::views::zip(funcs, that->funcs))
            if (not mine->matches(theirs.get(), captures)) return false;

        return true;
    }


    bool involvesName(const std::string_view sv) const override {
        return sv == stringify() or std::ranges::any_of(funcs,
            [sv] (const auto& e) {
                return e->involvesName(sv);
            }
        );
    }

    ExprPtr fillWith(const Fillings& fillings) const override {
        std::shared_ptr<Fix> copy = clone();
        for (auto& f : copy->funcs) f = splice(f, fillings);
        return copy;
    }

    virtual std::unique_ptr<Fix> clone() const = 0;
    virtual std::string OpName() const = 0;
    virtual token::TokenKind type() const = 0;
    virtual bool isPrefix() const = 0;
};



struct Prefix : Fix {
    // Prefix(Token t, const int s, ExprPtr c)
    // : Fix{std::move(t), s, std::move(c)} {}
    using Fix::Fix;

    std::string stringify(const size_t indent = 0) const override {
        const auto [c, token] = [this] -> std::pair<char, std::string> {
            if (shift < 0) return {'-', high};
            if (shift > 0) return {'+', low};
            return {'\0', high}; // or low. it doesn't matter since high == low
        }();

        // const std::string shifts(size_t(std::abs(shift)), c);
        std::string shifts;
        if (shift) shifts.append(" ").push_back(c);


        return "prefix(" + token + shifts + ") " + name + " = " + funcs[0]->stringify(indent);
    }

    std::unique_ptr<Fix> clone() const override { return std::make_unique<Prefix>(*this); }
    std::string OpName() const override { return name; }
    token::TokenKind type() const override { return token::TokenKind::PREFIX; }
    bool isPrefix() const override { return true; }

    Node variant() override { return this; }
};

struct Infix : Fix {
    using Fix::Fix;

    std::string stringify(const size_t indent = 0) const override {
        const auto [c, token] = [this] -> std::pair<char, std::string> {
            if (shift < 0) return {'-', high};
            if (shift > 0) return {'+', low};
            return {'\0', high}; // it doesn't matter. high == low
        }();

        // const std::string shifts(size_t(std::abs(shift)), c);
        std::string shifts;
        if (shift) shifts.append(" ").push_back(c);


        return "infix(" + token + shifts + ") " + name + " = " + funcs[0]->stringify(indent);
    }

    std::unique_ptr<Fix> clone() const override { return std::make_unique<Infix>(*this); }
    std::string OpName() const override { return name; }
    token::TokenKind type() const override { return token::TokenKind::INFIX; }
    bool isPrefix() const override { return false; }

    Node variant() override { return this; }
};

struct Suffix : Fix {
    using Fix::Fix;


    std::string stringify(const size_t indent = 0) const override {
        const auto [c, token] = [this] -> std::pair<char, std::string> {
            if (shift < 0) return {'-', high};
            if (shift > 0) return {'+', low};
            return {'\0', high}; // it doesn't matter. high == low
        }();

        // const std::string shifts(size_t(std::abs(shift)), c);
        std::string shifts;
        if (shift) shifts.append(" ").push_back(c);


        return "suffix(" + token + shifts + ") " + name + " = " + funcs[0]->stringify(indent);
    }


    std::unique_ptr<Fix> clone() const override { return std::make_unique<Suffix>(*this); }
    std::string OpName() const override { return name; }
    token::TokenKind type() const override { return token::TokenKind::SUFFIX; }
    bool isPrefix() const override { return false; }

    Node variant() override { return this; }
};

struct Exfix : Fix {
    std::string name2;


    Exfix(std::string n1, std::string n2, std::string up, std::string down, const int s, util::SourceSpan sp = {}) noexcept
    :
    Fix{std::move(n1), std::move(up), std::move(down), s, std::move(sp)},
    name2{std::move(n2)}
    { }

    std::string stringify(const size_t indent = 0) const override {
        const auto [c, token] = [this] -> std::pair<char, std::string> {
            if (shift < 0) return {'-', high};
            if (shift > 0) return {'+', low};
            return {'\0', high}; // it doesn't matter. high == low
        }();

        // const std::string shifts(size_t(std::abs(shift)), c);
        std::string shifts;
        if (shift) shifts.append(" ").push_back(c);


        return "exfix(" + token + shifts + ") " + name + ':' + name2 + " = " + funcs[0]->stringify(indent);
    }

    std::unique_ptr<Fix> clone() const override { return std::make_unique<Exfix>(*this); }
    std::string OpName() const override { return name + ':' + name2; }
    token::TokenKind type() const override { return token::TokenKind::EXFIX; }
    bool isPrefix() const override { return true; }

    Node variant() override { return this; }
};



struct Operator : Fix {
    std::vector<std::string> rest;
    std::vector<bool> op_pos;

    Operator(
        std::string first, std::vector<std::string> rst, std::vector<bool> pos,
        std::string up, std::string down,
        const int s,
        util::SourceSpan sp = {}
    )
    : Fix{
        std::move(first),
        std::move(up), std::move(down),
        s,
        std::move(sp)
    },
    rest{std::move(rst)}, op_pos{std::move(pos)}
    // begin_expr{begin}, end_expr{end}
    {
        if (size_t(std::ranges::count(op_pos, true)) != rest.size() + 1)// + 1 for first
            util::error();
    }


    std::string stringify(const size_t indent = 0) const override {
        const auto [c, token] = [this] -> std::pair<char, std::string> {
            if (shift < 0) return {'-', high};
            if (shift > 0) return {'+', low};
            return {'\0', high}; // it doesn't matter. high == low
        }();

        // const std::string shifts(size_t(std::abs(shift)), c);
        std::string shifts;
        if (shift) shifts.append(" ").push_back(c);


        return "mixfix (" + token + shifts + ") " + OpName() + " = " + funcs[0]->stringify(indent);
    }


    std::string OpName() const override {
        std::string op_name;
        for (ssize_t i = -1; const auto& field : op_pos) {
            if (field) {
                op_name += i == -1 ? name : rest[i];
                ++i;
            }
            else op_name += ':';
        }
        return op_name;
    }
    std::unique_ptr<Fix> clone() const override { return std::make_unique<Operator>(*this); }
    token::TokenKind type() const override { return token::TokenKind::MIXFIX; }
    bool isPrefix() const override { return op_pos[0]; }

    Node variant() override { return this; }
};

template <typename T>
T* is(Expr* e) { return dynamic_cast<T*>(e); }


inline Expr* toExpr(const Node& node) {
    return std::visit([] (auto* n) -> Expr* { return n; }, node);
}


} // namespace expr

} // namespace pie