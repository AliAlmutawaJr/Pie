#pragma once

#include <cctype>
#include <string>
#include <string_view>
#include <filesystem>
#include <vector>
#include <unordered_map>
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



inline namespace pie {


namespace expr {

struct StringID {
    std::string name;
    ssize_t ID = -1;
};



struct Num : Expr {
    std::string num;

    explicit Num(std::string n) noexcept : num{std::move(n)} {}

    std::string stringify(const size_t = 0) const override { return num; }

    bool involvesName(const std::string_view sv) const override { return sv == stringify(); }

    ExprPtr left() const override { return std::make_shared<Num>(*this); }

    Node variant() override { return this; }
};


struct Bool : Expr {
    bool boolean;

    explicit Bool(const bool b) noexcept : boolean{b} {}

    std::string stringify(const size_t = 0) const override { return boolean ? "true" : "false"; }

    bool involvesName(const std::string_view sv) const override { return sv == stringify(); }

    ExprPtr left() const override { return std::make_shared<Bool>(*this); }

    Node variant() override { return this; }
};


struct String : Expr {
    std::string str;

    explicit String(std::string s) noexcept : str{std::move(s)} {}

    std::string stringify(const size_t = 0) const override { return '"' + str + '"'; }

    bool involvesName(const std::string_view sv) const override { return sv == stringify(); }

    ExprPtr left() const override { return std::make_shared<String>(*this); }

    Node variant() override { return this; }
};


struct Name : Expr {
    std::string name;

    explicit Name(std::string n) noexcept : name{std::move(n)} {}

    std::string stringify(const size_t = 0) const override { return name; }

    bool involvesName(const std::string_view sv) const override { return sv == stringify(); }

    ExprPtr left() const override { return std::make_shared<Name>(*this); }

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

    explicit List(std::vector<ExprPtr> elts = {}) noexcept : elements{std::move(elts)} {}

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

    ExprPtr left() const override { return std::make_shared<List>(*this); }

    Node variant() override { return this; }
};


struct Map : Expr {
    std::vector<std::pair<ExprPtr, ExprPtr>> items;

    explicit Map(std::vector<std::pair<ExprPtr, ExprPtr>> elts = {}) noexcept : items{std::move(elts)} {}
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

    ExprPtr left() const override { return std::make_shared<Map>(*this); }

    Node variant() override { return this; }
};


struct Expansion : Expr {
    ExprPtr pack;

    explicit Expansion(ExprPtr p) noexcept : pack{std::move(p)} {}


    std::string stringify(const size_t indent = 0) const override {
        return pack->stringify(indent) + "...";
    }

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify() or pack->involvesName(sv);
    }

    ExprPtr left() const override { return std::make_shared<Expansion>(*this); }

    Node variant() override { return this; }
};


struct UnaryFold : Expr {
    ExprPtr pack;
    std::string op;
    bool left_to_right;

    UnaryFold(ExprPtr p, std::string o, const bool l2r) noexcept
    : pack{std::move(p)}, op{std::move(o)}, left_to_right{l2r}
    {}

    std::string stringify(const size_t indent = 0) const override {
        if (left_to_right)
            return '(' + pack->stringify(indent) + ' ' + op + " ...)";
        else
            return "(... " + op + ' ' + pack->stringify(indent) + ')';
    }

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify() or pack->involvesName(sv);
    }

    ExprPtr left() const override { return std::make_shared<UnaryFold>(*this); }

    Node variant() override { return this; }
};


struct SeparatedUnaryFold : Expr {
    ExprPtr lhs;
    ExprPtr rhs;
    std::string op;

    SeparatedUnaryFold(ExprPtr l, ExprPtr r, std::string o) noexcept
    : lhs{std::move(l)}, rhs{std::move(r)}, op{std::move(o)}
    {}

    std::string stringify(const size_t indent = 0) const override {
        return '(' + lhs->stringify(indent) + ' ' + op + " ... " + op + ' ' + rhs->stringify(indent) + ')';
    }

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify() or lhs->involvesName(sv) or rhs->involvesName(sv);
    }

    ExprPtr left() const override { return std::make_shared<SeparatedUnaryFold>(*this); }

    Node variant() override { return this; }
};


struct BinaryFold : Expr {
    ExprPtr pack;
    ExprPtr init;
    std::string op;
    bool left_to_right;

    ExprPtr sep;


    explicit BinaryFold(ExprPtr p, ExprPtr i, std::string o, const bool l2r, ExprPtr s = nullptr) noexcept
    : pack{std::move(p)}, init{std::move(i)}, op{std::move(o)}, left_to_right{std::move(l2r)}, sep{std::move(s)}
    {}

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

    ExprPtr left() const override { return std::make_shared<BinaryFold>(*this); }

    Node variant() override { return this; }
};


struct Assignment : Expr {
    ExprPtr lhs;
    type::TypePtr type;
    ExprPtr rhs;


    Assignment(ExprPtr l, type::TypePtr t, ExprPtr r) noexcept
    : lhs{std::move(l)}, type{std::move(t)}, rhs{std::move(r)}
    {}

    std::string stringify(const size_t indent = 0) const override {
        if (auto name = dynamic_cast<const Name*>(lhs.get()); name and not type::shouldReassign(type)) {
            return name->stringify(indent) + ": " + type->text() + " = " + rhs->stringify(indent);
        }

        return lhs->stringify(indent) + " = " + rhs->stringify(indent);
    }

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify() or lhs->involvesName(sv) or rhs->involvesName(sv);
    }

    ExprPtr left() const override { return lhs->left(); }

    Node variant() override { return this; }
};


struct Class : Expr {
    std::vector<std::tuple<Name, type::TypePtr, ExprPtr>> fields;

    explicit Class(std::vector<std::tuple<Name, type::TypePtr, ExprPtr>> f) noexcept
    : fields{std::move(f)} {}


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

    ExprPtr left() const override { return std::make_shared<Class>(*this); }

    Node variant() override { return this; }
};


struct Union : Expr {
    std::vector<type::TypePtr> types;

    Union(std::vector<type::TypePtr> ts) noexcept : types{std::move(ts)} {}

    std::string stringify(const size_t indent = 0) const override {
        std::string s = "union {\n";

        const std::string space(indent + 4, ' ');
        for (const auto& type : types) {
            s += space + type->text(indent + 4) + ";\n";
        }

        return s + std::string(indent, ' ') + "}";
    }

    bool involvesName(const std::string_view name) const override {
        const type::ExprType t{std::make_shared<expr::Name>(std::string{name})};

        for (const auto& type : types)
            if (type->involvesT(t)) return true;

         return false;
    }

    ExprPtr left() const override { return std::make_shared<Union>(*this); }

    Node variant() override { return this; }
};


struct Match : Expr {
    struct Case {
        struct Pattern {
            struct Single {
                StringID name;
                type::TypePtr type;
                ExprPtr value;
            };

            using Patterns = std::vector<std::unique_ptr<Pattern>>;
            struct Structure { StringID type_name; Patterns patterns; };


            std::variant<
                Single,
                Structure
            > pattern;

            explicit Pattern(Single single) : pattern{std::move(single)} {}

            Pattern(std::string name, Patterns structure)
            : pattern{Structure{{std::move(name)}, std::move(structure)}}
            {}
        };

        // Pattern pattern;
        std::unique_ptr<Pattern> pattern;
        ExprPtr guard;
        ExprPtr body;
    };

    ExprPtr expr;
    std::vector<Case> cases;

    Match(ExprPtr e, std::vector<Case> cs) noexcept
    : expr{std::move(e)}, cases{std::move(cs)}
    {}


    std::string stringify(const size_t indent = 0) const override {
        std::string s = "match " + expr->stringify(indent) + " {\n";

        for (const std::string space(indent + 4, ' '); auto&& kase : cases) {
            s += space;

            s += stringifyPattern(*kase.pattern, indent + 4);

            if (kase.guard) s += " & " + kase.guard->stringify(indent + 4);
            s += " => " + kase.body->stringify(indent + 4) + ";\n";
        }


        return s + std::string(indent, ' ') + "}";
    }

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify() or expr->involvesName(sv);
    }

    ExprPtr left() const override { return nullptr; }

    Node variant() override { return this; }


private:
    std::string stringifyPattern(const Case::Pattern& pattern, const size_t indent = 0) const {
        if (std::holds_alternative<Case::Pattern::Single>(pattern.pattern)) {
            const auto& pat = get<Case::Pattern::Single>(pattern.pattern);

            std::string type = pat.type->text();
            if (type == "Any") type = ""; else type = ": " + type;

            std::string def = "";
            if (pat.value) def = " = " + pat.value->stringify(indent);


            return pat.name.name + type + def;
        }

        const auto& [name, patterns] = get<Case::Pattern::Structure>(pattern.pattern);

        std::string s = name.name + '(';

        for (std::string comma = ""; const auto& pat : patterns) {
            s += comma + stringifyPattern(*pat, indent);
            comma = ", ";
        }

        return s + ')';
    }
};


struct Type : Expr {
    type::TypePtr type;

    explicit Type(type::TypePtr t) noexcept : type{std::move(t)} {}

    std::string stringify(const size_t indent = 0) const override { return type->text(indent); }

    bool involvesName(const std::string_view) const override {
        return false; // temp
    }

    ExprPtr left() const override {
        return std::make_shared<Type>(type->clone());
    }

    Node variant() override { return this; }
};


struct Loop : Expr {
    ExprPtr kind;
    // ExprPtr var;
    StringID var;
    ExprPtr body;
    ExprPtr els;

    Loop(ExprPtr b, std::string v = "", ExprPtr k = nullptr, ExprPtr e = nullptr) noexcept
    : kind{std::move(k)}, var{std::move(v)}, body{std::move(b)}, els{std::move(e)}
    {}


    std::string stringify(const size_t indent = 0) const override {
        std::string s = "loop ";

        if (kind) s += kind->stringify(indent + 4) + " => ";
        if (not var.name.empty()) s += var.name + " ";

        s += body->stringify(indent + 4);

        if (els) s += " else " + els->stringify();

        return s;
    }

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify()
            or kind->involvesName(sv)
            or sv == var.name
            or body->involvesName(sv)
            or els ->involvesName(sv);
    }

    ExprPtr left() const override { return std::make_shared<Loop>(*this); }

    Node variant() override { return this; }
};


struct Break : Expr {
    ExprPtr expr;

    explicit Break(ExprPtr e = nullptr) noexcept : expr{std::move(e)} {}

    std::string stringify(const size_t indent = 0) const override {
        if (expr) return "break " + expr->stringify(indent + 4);

        return "break";
    }

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify() or expr->involvesName(sv);
    }

    ExprPtr left() const override { return std::make_shared<Break>(*this); }

    Node variant() override { return this; }
};


struct Continue : Expr {
    ExprPtr expr;

    explicit Continue(ExprPtr e = nullptr) noexcept : expr{std::move(e)} {}

    std::string stringify(const size_t indent = 0) const override {
        if (expr) return "continue " + expr->stringify(indent + 4);

        return "continue";
    }

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify() or expr->involvesName(sv);
    }

    ExprPtr left() const override { return std::make_shared<Continue>(*this); }

    Node variant() override { return this; }
};


struct Access : Expr {
    ExprPtr var;
    std::string name;

    Access(ExprPtr v, std::string n) noexcept
    : var{std::move(v)}, name{std::move(n)}
    {}

    std::string stringify(const size_t indent = 0) const override {
        return var->stringify(indent) + '.' + name;
    }

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify() or var->involvesName(sv);
    }

    ExprPtr left() const override { return var->left(); }

    Node variant() override { return this; }
};


struct Cascade : Expr {
    ExprPtr var;
    std::vector<ExprPtr> members;
    // ExprPtr member;

    Cascade(ExprPtr v, std::vector<ExprPtr> m) noexcept
    : var{std::move(v)}, members{std::move(m)}
    {}

    std::string stringify(const size_t = 0) const override {
        // return var->stringify(indent) + ".." + 
        return "cascade";
    }

    bool involvesName(const std::string_view) const override {
        // return sv == stringify() or var->involvesName(sv) or sv == member->involvesName(sv);
        return false;
    }

    ExprPtr left() const override { return var->left(); }

    Node variant() override { return this; }
};


struct Namespace : Expr {
    std::string name;
    std::vector<ExprPtr> space;


    explicit Namespace(std::string n, std::vector<ExprPtr> exprs) noexcept
    : name{std::move(n)}, space{std::move(exprs)}
    {}

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

    ExprPtr left() const override { return std::make_shared<Namespace>(*this); }

    Node variant() override { return this; }
};


struct Use : Expr {
    bool global;
    // last name is not a space
    std::vector<std::string> spaces;
    StringID name;

    Use(bool g, std::vector<std::string> ns, std::string n) noexcept
    : global{g}, spaces{std::move(ns)}, name{std::move(n)}
    {}


    std::string stringify(const size_t = 0) const override {
        std::string s;

        for (const auto& space : spaces)
            s += space + "::";

        return (global ? "use ::" : "use " ) + s + name.name;
    }

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify();
    }

    ExprPtr left() const override { return std::make_shared<Use>(*this); }

    Node variant() override { return this; }
};


struct UseSpace : Expr {
    bool global;
    std::vector<std::string> spaces;
    ssize_t last_item_id;
    bool pull_ops;


    UseSpace(bool g, std::vector<std::string> ns, bool ops) noexcept
    : global{g}, spaces{std::move(ns)}, pull_ops{ops}
    {}


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

    ExprPtr left() const override { return std::make_shared<UseSpace>(*this); }

    Node variant() override { return this; }
};


struct UseFix : Expr {
    bool global;
    std::vector<std::string> spaces;
    TokenKind filter;
    std::string op_name;


    UseFix(bool g, std::vector<std::string> ns, const TokenKind f = TokenKind::NONE, std::string op = "") noexcept
    : global{g}, spaces{std::move(ns)}, filter{f}, op_name{std::move(op)}
    {}


    std::string stringify(const size_t = 0) const override {
        std::string s = "use ";
        if (filter != TokenKind::NONE) {
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

    ExprPtr left() const override { return std::make_shared<UseFix>(*this); }

    Node variant() override { return this; }
};


struct Import : Expr {
    std::filesystem::path path;

    explicit Import(std::filesystem::path p) noexcept
    : path{std::move(p)} {}

    std::string stringify(const size_t = 0) const override {
        std::string s;
        for (const char c : path.string())
            s.push_back(c == '/' ? '.' : c);

        return "import " + s;
    }

    bool involvesName(const std::string_view) const override { return false; }

    ExprPtr left() const override { return std::make_shared<Import>(*this); }

    Node variant() override { return this; }
};


struct SpaceAccess : Expr {
    bool global;
    std::vector<std::string> spaces;
    StringID name;


    SpaceAccess(bool g, std::vector<std::string> s, std::string n) noexcept
    : global{g}, spaces{std::move(s)}, name{std::move(n)} {}

    std::string stringify(const size_t = 0) const override {

        std::string s;

        for (const auto& sp : spaces)
            s += sp + "::";

        return (global ? "::" : "") + s + name.name;
    }

    bool involvesName(const std::string_view) const override {
        return false; // qualified names don't involve other names!
    }

    ExprPtr left() const override { return nullptr; }

    Node variant() override { return this; }
};


struct Syntax : Expr {
    ExprPtr expr;
    Syntax(ExprPtr e) noexcept : expr{std::move(e)} {}

    std::string stringify(const size_t indent = 0) const override {
        return '`' + expr->stringify(indent + 4) + '`';
    }

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify() or expr->involvesName(sv);
    }

    ExprPtr left() const override { return std::make_shared<Syntax>(*this); }

    Node variant() override { return this; }
};



// Only reason this is needed is to distinguish between expressions like
// 1 + 2 and (1 + 2)
// since they could get assigned 2 different values
// but is this the behaviour that I want?
struct Grouping : Expr {
    ExprPtr expr;
    Grouping(ExprPtr e) noexcept : expr{std::move(e)} {}

    std::string stringify(const size_t indent = 0) const override {
        // return '(' + expr->stringify(indent) + ')';
        return expr->stringify(indent);
    }

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify() or expr->involvesName(sv);
    }

    ExprPtr left() const override { return std::make_shared<Grouping>(*this); }

    Node variant() override { return this; }
};


struct UnaryOp : Expr {
    std::string op;
    ExprPtr expr;


    UnaryOp(std::string o, ExprPtr e) noexcept
    : op{std::move(o)}, expr{std::move(e)}
    {}

    std::string stringify(const size_t indent = 0) const override {
        return '(' + op + ' ' + expr->stringify(indent) + ')';
    }

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify() or expr->involvesName(sv);
    }

    ExprPtr left() const override { return std::make_shared<UnaryOp>(*this); }

    Node variant() override { return this; }
};

struct BinOp : Expr {
    ExprPtr lhs;
    std::string op;
    ExprPtr rhs;


    BinOp(ExprPtr e1, std::string o, ExprPtr e2) noexcept
    : lhs{std::move(e1)}, op{std::move(o)}, rhs{std::move(e2)}
    {}

    std::string stringify(const size_t indent = 0) const override {
        return '(' + lhs->stringify(indent) + ' ' + op + ' ' + rhs->stringify(indent) + ')';
    }

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify() or lhs->involvesName(sv) or rhs->involvesName(sv);
    }

    ExprPtr left() const override { return lhs->left(); }

    Node variant() override { return this; }
};


struct PostOp : Expr {
    std::string op;
    ExprPtr expr;


    PostOp(std::string o, ExprPtr e) noexcept
    : op{std::move(o)}, expr{std::move(e)}
    {}

    std::string stringify(const size_t indent = 0) const override {
        return '(' + expr->stringify(indent) + ' ' + op + ')';
    }

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify() or expr->involvesName(sv);
    }

    ExprPtr left() const override { return expr->left(); }

    Node variant() override { return this; }
};


// circumfix operators
struct CircumOp : Expr {
    std::string op1;
    std::string op2;
    ExprPtr expr;

    CircumOp(std::string o1, std::string o2, ExprPtr e) noexcept
    : op1{std::move(o1)}, op2{std::move(o2)}, expr{std::move(e)} {}

    std::string stringify(const size_t indent = 0) const override {
        return '(' + op1 + ' ' + expr->stringify(indent) + ' ' + op2 + ')';
    }

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify() or expr->involvesName(sv);
    }

    ExprPtr left() const override { return std::make_shared<CircumOp>(*this); }

    Node variant() override { return this; }
};


struct OpCall : Expr {
    std::string first;
    std::vector<std::string> rest;
    std::vector<ExprPtr> exprs;
    std::vector<bool> op_pos;

    OpCall(
        std::string f, std::vector<std::string> ops, std::vector<ExprPtr> ex,
        std::vector<bool> pos
    ) noexcept
    :
    first{std::move(f)}, rest{std::move(ops)}, exprs{std::move(ex)}, op_pos{std::move(pos)}
    {}


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

    ExprPtr left() const override {
        if (not op_pos[0]) return exprs[0]->left();

        return std::make_shared<OpCall>(*this); 
    }

    Node variant() override { return this; }
};


struct Call : Expr {
    ExprPtr func;

    std::unordered_map<std::string, ExprPtr> named_args;
    std::vector<ExprPtr> args;


    Call(ExprPtr function, std::unordered_map<std::string, ExprPtr> named = {}, std::vector<ExprPtr> pos = {})
    : func{std::move(function)}, named_args{std::move(named)}, args{std::move(pos)} { }

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

    ExprPtr left() const override { return func->left(); }

    Node variant() override { return this; }
};


struct Closure : Expr {
    std::vector<StringID> params;
    ExprPtr body;
    type::FuncType type;



    struct CapturedEnvs {
        value::Env env{};
        value::Env returned_env{};
        value::Env passed_env{};
    } envs;

    // whether it's a member function or not
    std::optional<value::Object> self{};

    std::vector<interp::NameSpace*> spaces;

    Closure(std::vector<StringID> ps, ExprPtr b, type::FuncType t) noexcept
    : params{std::move(ps)}, body{std::move(b)}, type{std::move(t)} { }

    Closure(std::vector<std::string> ps, ExprPtr b, type::FuncType t)
    :
    // params{std::move(ps)},
    body{std::move(b)}, type{std::move(t)} {
        for (auto& s : ps)
            params.emplace_back(std::move(s));


        if(params.size() != type.params.size()) util::error(); // should never happen anyway
    }


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

        if (not params.empty())
            s += params[0].name + (type::shouldReassign(type.params[0]) ? "" : ": " + type.params[0]->text(indent));



        for(const auto& [name, type] : std::views::zip(params, type.params) | std::views::drop(1))
            s += ", " + name.name + (type::shouldReassign(type) ? "" : ": " + type->text());

        return s + ")" + (type::shouldReassign(type.ret)? + "" : ": " + type.ret->text()) + " => " + body->stringify(indent);
    }

    bool involvesName(const std::string_view sv) const override {
        return sv == stringify() or body->involvesName(sv);
    }

    ExprPtr left() const override { return std::make_shared<Closure>(*this); }

    Node variant() override { return this; }
};


struct Block : Expr {
    std::vector<ExprPtr> lines;

    explicit Block(std::vector<ExprPtr> l) noexcept : lines{std::move(l)} {};


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

    ExprPtr left() const override { return std::make_shared<Block>(*this); }

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


    Fix(std::string n, std::string up, std::string down, const int s, std::vector<ExprPtr> cs)
    : name{std::move(n)}, high{std::move(up)}, low{std::move(down)}, shift{s}, funcs{std::move(cs)} {}


    bool involvesName(const std::string_view sv) const override {
        return sv == stringify() or std::ranges::any_of(funcs,
            [sv] (const auto& e) {
                return e->involvesName(sv);
            }
        );
    }

    virtual std::unique_ptr<Fix> clone() const = 0;
    virtual std::string OpName() const = 0;
    virtual TokenKind type() const = 0;
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

    ExprPtr left() const override { return std::make_shared<Prefix>(*this); }

    std::unique_ptr<Fix> clone() const override { return std::make_unique<Prefix>(*this); }
    std::string OpName() const override { return name; }
    TokenKind type() const override { return TokenKind::PREFIX; }
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

    ExprPtr left() const override { return std::make_shared<Infix>(*this); }

    std::unique_ptr<Fix> clone() const override { return std::make_unique<Infix>(*this); }
    std::string OpName() const override { return name; }
    TokenKind type() const override { return TokenKind::INFIX; }
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

    ExprPtr left() const override { return std::make_shared<Suffix>(*this); }

    std::unique_ptr<Fix> clone() const override { return std::make_unique<Suffix>(*this); }
    std::string OpName() const override { return name; }
    TokenKind type() const override { return TokenKind::SUFFIX; }
    bool isPrefix() const override { return false; }

    Node variant() override { return this; }
};

struct Exfix : Fix {
    std::string name2;
    Exfix(std::string n1, std::string n2, std::string up, std::string down, const int s, std::vector<ExprPtr> cs)
    : Fix{std::move(n1), std::move(up), std::move(down), s, std::move(cs)}, name2{std::move(n2)} {}

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

    ExprPtr left() const override { return std::make_shared<Exfix>(*this); }

    std::unique_ptr<Fix> clone() const override { return std::make_unique<Exfix>(*this); }
    std::string OpName() const override { return name + ':' + name2; }
    TokenKind type() const override { return TokenKind::EXFIX; }
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
        std::vector<ExprPtr> cs
    )
    : Fix{
        std::move(first),
        std::move(up), std::move(down),
        s,
        std::move(cs)
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


    ExprPtr left() const override { return std::make_shared<Operator>(*this); }


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
    TokenKind type() const override { return TokenKind::MIXFIX; }
    bool isPrefix() const override { return op_pos[0]; }

    Node variant() override { return this; }
};

} // namespace expr

} // namespace pie