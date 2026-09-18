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



struct Num : Expr {
    std::string num;

    explicit Num(std::string n, util::SourceSpan sp) noexcept : Expr{std::move(sp)}, num{std::move(n)} { }

    std::string stringify(const size_t = 0) const override { return num; }

    bool involvesName(const std::string_view sv) const override { return sv == stringify(); }

    Node variant() override { return this; }
};


struct Bool : Expr {
    bool boolean;

    explicit Bool(const bool b, util::SourceSpan sp) noexcept : Expr{std::move(sp)}, boolean{b} { }

    std::string stringify(const size_t = 0) const override { return boolean ? "true" : "false"; }

    bool involvesName(const std::string_view sv) const override { return sv == stringify(); }

    Node variant() override { return this; }
};


struct String : Expr {
    std::string str;

    explicit String(std::string s, util::SourceSpan sp) noexcept : Expr{std::move(sp)} , str{std::move(s)} {}

    std::string stringify(const size_t = 0) const override { return '"' + str + '"'; }

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

    bool involvesName(const std::string_view sv) const override {
        for (const auto& expr : exprs)
            if (expr.second->involvesName(sv)) return true;

        return sv == stringify();
    }

    Node variant() override { return this; }
};


struct Name : Expr {
    std::string name;

    explicit Name(std::string n, util::SourceSpan sp = {}) noexcept : Expr{std::move(sp)}, name{std::move(n)} { }

    std::string stringify(const size_t = 0) const override { return name; }

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

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify() or std::ranges::any_of(elements, [sv] (const auto& e) {
            return e->involvesName(sv);
        });
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

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify() or std::ranges::any_of(items, [sv] (const auto& e) {
            const auto& [key, value] = e;
            return key->involvesName(sv) or value->involvesName(sv); 
        });
    }

    Node variant() override { return this; }
};



struct Expansion : Expr {
    ExprPtr pack;

    explicit Expansion(ExprPtr p, util::SourceSpan sp = {}) noexcept : Expr{std::move(sp)}, pack{std::move(p)} { }


    std::string stringify(const size_t indent = 0) const override {
        return pack->stringify(indent) + "...";
    }

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify() or pack->involvesName(sv);
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

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify() or pack->involvesName(sv);
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

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify() or lhs->involvesName(sv) or rhs->involvesName(sv);
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

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify() or pack->involvesName(sv) or init->involvesName(sv);
    }

    Node variant() override { return this; }
};


struct Assignment : Expr {
    ExprPtr lhs;
    type::TypePtr type;
    ExprPtr rhs;

    bool is_syntax;


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

    bool involvesName(const std::string_view sv) const override {
        // todo: check the type
        return sv == stringify() or lhs->involvesName(sv) or rhs->involvesName(sv);
    }

    Node variant() override { return this; }
};


struct InferredAssignment : Expr{
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

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify() or name.name == sv or rhs->involvesName(sv);
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
    ExprPtr expr;
    // Pack() = default;
    Pack(ExprPtr e, type::TypePtr type = nullptr, ExprPtr value = nullptr) noexcept
    : Pattern{std::move(type), std::move(value)}, expr{std::move(e)} { }
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
        s.pop_back();
        s.pop_back();

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

        if (pack->expr) s+= pack->expr->stringify();
    }
    else util::error();


    if (pattern->type ) s += "`:` "  + pattern->type ->text     (indent + 4);
    if (pattern->value) s += " `=` " + pattern->value->stringify(indent + 4);

    return s;
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
        return pack->expr->involvesName(sv);
    }
    else util::error();

    return false;
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

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify()
            or patternInvolves(pattern.get(), sv)
            or rhs->involvesName(sv);
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

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify()
            or patternInvolves(pattern.get(), sv)
            or rhs->involvesName(sv);
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

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify() or std::ranges::any_of(fields,
            [sv] (const auto& e) {
                const auto& [_, __, expr] = e;
                return expr->involvesName(sv);
            }
        );
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

        if (els) s += "else => " + els->stringify();

        return s;
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

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify() or expr->involvesName(sv);
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

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify() or expr->involvesName(sv);
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


    bool involvesName(const std::string_view sv) const override {
        return sv == stringify()
            or (var and sv == unpack::stringifyPattern(var.get()))
            or (kind and kind->involvesName(sv))
            or (guard and guard->involvesName(sv))
            or body1->involvesName(sv)
            or body2->involvesName(sv);
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

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify() or var->involvesName(sv);
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

    bool involvesName(const std::string_view) const override {
        return false; // temp
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
        // auto path_str = path.string();
        // std::string s = path_str[0] == '/' ? "" : std::string{path_str[0]};

        // for (const char c : path.string() | std::views::drop(1)) 
        //     s.push_back(c == '/' ? '.' : c);

        return "import " + path.string();
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

    bool involvesName(const std::string_view) const override {
        return false; // qualified names don't involve other names!
    }

    Node variant() override { return this; }
};


struct Syntax : Expr {
    ExprPtr expr;
    Syntax(ExprPtr e, util::SourceSpan sp = {}) noexcept
    :
    Expr{std::move(sp)},
    expr{std::move(e)}
    { }

    std::string stringify(const size_t indent = 0) const override {
        return '`' + expr->stringify(indent + 4) + '`';
    }

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify() or expr->involvesName(sv);
    }

    Node variant() override { return this; }
};



// Only reason this is needed is to distinguish between expressions like
// 1 + 2 and (1 + 2)
// since they could get assigned 2 different values
// but is this the behaviour that I want?
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

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify() or expr->involvesName(sv);
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

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify() or expr->involvesName(sv);
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

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify() or lhs->involvesName(sv) or rhs->involvesName(sv);
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

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify() or expr->involvesName(sv);
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

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify() or expr->involvesName(sv);
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

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify() or std::ranges::any_of(exprs,
            [sv] (const auto& e) {
                return e->involvesName(sv);
            }
        );
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

    std::vector<Param> params;
    ExprPtr body;
    type::FuncType type;

    // vm::Chunk compiled_body;


    struct CapturedEnvs {
        value::Env env{};
        value::Env returned_env{};
        value::Env passed_env{};
    } envs;

    // whether it's a member function or not
    std::optional<value::Object> self{};

    std::vector<interp::NameSpace*> spaces;

    Closure(std::vector<Param> ps, ExprPtr b, type::FuncType t, util::SourceSpan sp = {}) noexcept
    :
    Expr{std::move(sp)},
    params{std::move(ps)},
    body{std::move(b)},
    type{std::move(t)}
    { }


    Closure(const Closure&) = default;
    Closure(Closure&&) = default;
    Closure& operator=(const Closure&) = default;
    Closure& operator=(Closure&&) = default;
    ~Closure() = default;

    // Closure(std::vector<std::string> ps, ExprPtr b, type::FuncType t)
    // :
    // // params{std::move(ps)},
    // body{std::move(b)}, type{std::move(t)} {
    //     for (auto& s : ps)
    //         params.emplace_back(std::move(s));


    //     if(params.size() != type.params.size()) util::error(); // should never happen anyway
    // }


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
        std::string s = "(";

        if (not params.empty()) {
            if (std::holds_alternative<RegularParam>(params[0])) {
                s += get<RegularParam>(params[0]).expr->stringify();
            }
            else {
                s += unpack::stringifyPattern(get<unpack::PatternPtr>(params[0]).get());
            }

            if (not type::shouldReassign(type.params[0]))
                s+= ": " + type.params[0]->text(indent);
        }



        for(const auto& [param, type] : std::views::zip(params, type.params) | std::views::drop(1)) {
            if (std::holds_alternative<RegularParam>(param)) {
                s += get<RegularParam>(param).expr->stringify();
            }
            else {
                s += unpack::stringifyPattern(get<unpack::PatternPtr>(param).get());
            }

            if (not type::shouldReassign(type))
                s+= ": " + type->text(indent);

            // s += ", " + name.expr->stringify() + (type::shouldReassign(type) ? "" : ": " + type->text());
        }

        return s + ")" + (type::shouldReassign(type.ret)? + "" : ": " + type.ret->text()) + " => " + body->stringify(indent);
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

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify() or std::ranges::any_of(lines,
            [sv] (const auto& e) {
                return e->involvesName(sv);
            }
        );
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


    bool involvesName(const std::string_view sv) const override {
        return sv == stringify() or std::ranges::any_of(funcs,
            [sv] (const auto& e) {
                return e->involvesName(sv);
            }
        );
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
T* is(expr::Expr* e) { return dynamic_cast<T*>(e); }

} // namespace expr

} // namespace pie