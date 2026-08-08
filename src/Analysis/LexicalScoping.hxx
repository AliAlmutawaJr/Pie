#pragma once


#include <optional>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>


#include "../Expr/Expr.hxx"
#include "../Type/Type.hxx"
#include "../Value/Value.hxx"


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


class LexicalAnalysis {
    enum class EnvTag {
        SCOPE,
        SPACE,
    };

    size_t variable_index;
    size_t constant_index;


    // scopes
    std::vector<std::pair<Env, EnvTag>> env;
    std::vector<NameSpace*> current_space;
    std::unordered_map<std::string, std::pair<size_t, const value::Value>> constants;



    bool in_loop = false;

public:

    std::vector<size_t> indeces;

    LexicalAnalysis(const size_t = 0, const size_t = 0);


    // void operator()(auto *node) { }

    void operator()(expr::Num*);
    void operator()(expr::Bool*);
    void operator()(expr::String*);

    void operator()(expr::Fix*);


    void accessAssign(expr::Access*, expr::Assignment*);
    void spaceAccessAssign(expr::SpaceAccess*, expr::Assignment*);

    void operator()(expr::Assignment*);
    void operator()(expr::InferredAssignment*);


    void checkPattern(expr::Unpackment::Pattern*);
    void checkPattern(expr::Unpackment::Pattern*, bool inferred); // tag to dispatch at compile time
    void operator()(expr::Unpackment *);


    void operator()(expr::Name*);


    void operator()(expr::Block*);

    void operator()(expr::Closure*);

    void operator()(expr::Call*);

    void operator()(expr::List*);

    void operator()(expr::Map*);

    void operator()(expr::Expansion*);

    void operator()(expr::UnaryFold*);

    void operator()(expr::SeparatedUnaryFold*);

    void operator()(expr::BinaryFold*);


    void operator()(expr::Class*);

    void operator()(expr::Union*);


    void checkPattern(expr::Match::Case::Pattern&);

    void operator()(expr::Match*);


    void operator()(expr::Loop*);

    void operator()(expr::Break*);

    void operator()(expr::Continue*);


    void operator()(const expr::Access*);


    void operator()(expr::Import*);

    void operator()(expr::Namespace*);

    void operator()(expr::UseFix*);

    void operator()(expr::UseSpace*);

    void operator()(expr::Use*);

    void operator()(expr::SpaceAccess*);


    void operator()(expr::Grouping*);

    void operator()(expr::UnaryOp*);

    void operator()(expr::BinOp*);

    void operator()(expr::PostOp*);

    void operator()(expr::CircumOp*);

    void operator()(expr::OpCall*);

    void visitType(const type::TypePtr&);

    void operator()(expr::Syntax*);

    void operator()(expr::Type*);



    void addSpace(const std::string&);

    void popSpace();



    NameSpace* matchChain(const std::vector<std::string>&, NameSpace*);

    // ideally, should be called findSpaces!
    NameSpace* findSpace(const std::vector<std::string>&, const bool global_search_only = false);

    void addVar(std::string name, const size_t);


    std::optional<size_t> findVarInSpace(const std::string&) const;


    void addNamespaces(
        std::unordered_map<std::string, std::shared_ptr<NameSpace>>& spaces,
        const std::unordered_map<std::string, std::shared_ptr<NameSpace>>& new_spaces
    );


    [[nodiscard]] std::optional<size_t> findVariable(const std::string&) const;
    [[nodiscard]] std::optional<size_t> findConstant(const std::string&) const;

    [[nodiscard]] std::unordered_map<ssize_t, const value::Value> extractConstantMap() const;


    void scope();


    void scope(const std::shared_ptr<NameSpace>&);


    void unscope();

    struct ScopeGuard {
        LexicalAnalysis* that;
         ScopeGuard(LexicalAnalysis* t) : that{t} { that->scope(); }
        ~ScopeGuard() { that->unscope(); }
    };
};




} // namespace analysis
} // namespace pie
