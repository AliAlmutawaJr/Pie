#pragma once


#include <optional>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>


#include "../Expr/Expr.hxx"
#include "../Type/Type.hxx"


inline namespace pie {
namespace analysis {

std::string stringify(const std::vector<std::string>& spaces);

struct NameSpace {
    std::string name;

    NameSpace *parent = nullptr;
    std::unordered_map<std::string, size_t> members;
    std::unordered_map<std::string, std::shared_ptr<NameSpace>> children;
};


struct Env {
    std::unordered_map<std::string, size_t> vars;
    std::unordered_map<std::string, std::shared_ptr<NameSpace>> spaces;
};


class LexicalScoping {
    enum class EnvTag {
        SCOPE,
        SPACE,
    };

    size_t variable_index;


    // scopes
    std::vector<std::pair<Env, EnvTag>> env;
    std::vector<NameSpace*> current_space;



    bool in_loop = false;

public:

    std::vector<size_t> indeces;

    LexicalScoping(const size_t index = 0);


    // void operator()(auto *node) { }

    void operator()(expr::Num *n);
    void operator()(expr::Bool *b);
    void operator()(expr::String *s);

    void operator()(expr::Cascade *c);
    void operator()(expr::Fix *f);


    void accessAssign(expr::Access *acc, expr::Assignment *ass);
    void spaceAccessAssign(expr::SpaceAccess *acc, expr::Assignment *ass);

    void operator()(expr::Assignment *ass);
    void operator()(expr::InferredAssignment *inf);


    void operator()(expr::Name *name);


    void operator()(expr::Block *b);

    void operator()(expr::Closure *c);

    void operator()(expr::Call *call);

    void operator()(expr::List *list);

    void operator()(expr::Map *map);

    void operator()(expr::Expansion *e);

    void operator()(expr::UnaryFold *fold);

    void operator()(expr::SeparatedUnaryFold *fold);

    void operator()(expr::BinaryFold *fold);


    void operator()(expr::Class *cls);

    void operator()(expr::Union *onion);


    void checkPattern(expr::Match::Case::Pattern& pat);


    void operator()(expr::Match *match);


    void operator()(expr::Loop *loop);

    void operator()(expr::Break *br);

    void operator()(expr::Continue *cont);


    void operator()(const expr::Access *acc);


    void operator()(expr::Import *import);


    void operator()(expr::Namespace *n);


    void operator()(expr::UseFix *use);

    void operator()(expr::UseSpace *use);

    void operator()(expr::Use *use);

    void operator()(expr::SpaceAccess *acc);

    void operator()(expr::Grouping *group);

    void operator()(expr::UnaryOp *up);

    void operator()(expr::BinOp *bp);

    void operator()(expr::PostOp   *pp);

    void operator()(expr::CircumOp *cp);

    void operator()(expr::OpCall *oc);

    void visitType(const type::TypePtr& type);

    void operator()(expr::Syntax *syn);

    void operator()(expr::Type *type);



    // size_t checkName(
    //     const std::string& name,
    //     const std::source_location& location = std::source_location::current()
    // ) {
    //     if (const auto ID = findVar(name); not ID) util::error<except::NameLookup>("Name `" + name + "` not found!", location);
    //     else return *ID;
    // }


    // void checkName(
    //     const std::string& name,
    //     const std::string& space,
    //     const std::source_location& location = std::source_location::current()
    // ) {
    //     if (not namespaces.contains(space)) util::error<except::NameLookup>("Name `" + name + "` not found!", location);

    //     for (const auto& n : namespaces[space])
    //         if (n == name) return;


    //     util::error<except::NameLookup>("Name `" + name + "` not found!", location);
    // }



    // static NameSpace* getNamespaceAt(
    //     const std::string& dir,
    //     const std::unordered_map<std::string, std::shared_ptr<NameSpace>>& spaces
    // ) {
    //     if (dir.empty()) return nullptr;

    //     const size_t ind = dir[0] - 0X30;

    //     return dir.size() == 1 ?
    //         spaces[ind].get()  :
    //         getNamespaceAt(dir.substr(0, dir.size() - 1), spaces[ind]->children);
    // }


    // std::string fullName(const NameSpace* ns) const {
    //     std::vector<std::string> spaces;

    //     for (auto ptr = ns; ptr; ptr = ptr->parent)
    //         spaces.push_back(ptr->name);

    //     std::ranges::reverse(spaces);

    //     std::string name = spaces[0];

    //     for (const auto& s : spaces | std::views::drop(1))
    //         name += "::" + s;

    //     return name;
    // }


    void addSpace(const std::string& name);

    void popSpace();



    NameSpace* matchChain(const std::vector<std::string>& names, NameSpace *space);

    // ideally, should be called findSpaces!
    NameSpace* findSpace(const std::vector<std::string>& names, const bool global_search_only = false);

    void addVar(std::string name, const size_t index);


    std::optional<size_t> findVarInSpace(const std::string& name) const;


    void addNamespaces(
        std::unordered_map<std::string, std::shared_ptr<NameSpace>>& spaces,
        const std::unordered_map<std::string, std::shared_ptr<NameSpace>>& new_spaces
    );


    [[nodiscard]] std::optional<size_t> findVar(const std::string& name) const;


    void scope();


    void scope(const std::shared_ptr<NameSpace>& space);


    void unscope();

    struct ScopeGuard {
        LexicalScoping* that;
         ScopeGuard(LexicalScoping* t) : that{t} { that->scope(); }
        ~ScopeGuard() { that->unscope(); }
    };
};




} // namespace analysis
} // namespace pie
