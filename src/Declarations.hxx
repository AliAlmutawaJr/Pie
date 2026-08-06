#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <memory>


#ifdef WEB_PIE
using BigInt = long long;
#else
using BigInt = ssize_t;
#endif

inline namespace pie {

namespace type {
    using TypePtr = std::shared_ptr<struct Type>;
}


namespace expr { struct Fix; }
using Operators  = std::unordered_map<std::string, std::shared_ptr<expr::Fix>>;

namespace interp { struct NameSpace; }

namespace value {
    struct Value;
    using ValuePtr = std::shared_ptr<Value>;

    struct Members;
    using Object = std::pair<type::TypePtr, std::shared_ptr<Members>>;

    struct SpaceRef {
        std::string name;
        interp::NameSpace* space = nullptr;

        bool isRef() const { return space != nullptr; }
    };

    using Environment = std::unordered_map<
        ssize_t,
        std::tuple<
            SpaceRef,
            value::ValuePtr,
            type::TypePtr
        >
    >;


    enum class EnvTag {
        NONE,
        FUNC,
        SCOPE,
    };

    struct Env {
        Environment env;
        Operators prefix_op_env;
        Operators op_env;

        EnvTag tag = EnvTag::NONE;
    };
}


namespace expr {

// has to be pointers kuz we're forward declareing
// has to be forward declared bc we're using in in the class bellow
using Node = std::variant<
    struct Num               *,
    struct Bool              *,
    struct String            *,
    struct Name              *,
 // struct Pack              *,
    struct List              *,
    struct Map               *,
    struct Expansion         *,
    struct UnaryFold         *,
    struct SeparatedUnaryFold*,
    struct BinaryFold        *,
    struct Assignment        *,
    struct InferredAssignment*,
    struct Unpackment        *,
    struct Class             *,
    struct Union             *,
    struct Match             *,
    struct Type              *,
    struct Loop              *,
    struct Break             *,
    struct Continue          *,
    struct Access            *,
    struct Cascade           *,
    struct Namespace         *,
    struct Use               *,
    struct UseSpace          *,
    struct UseFix            *,
    struct Import            *,
    struct SpaceAccess       *,
    struct Syntax            *,
    struct Grouping          *,
    struct UnaryOp           *,
    struct BinOp             *,
    struct PostOp            *,
    struct CircumOp          *,
    struct OpCall            *,
    struct Call              *,
    struct Closure           *,
    struct Block             *,
    struct Prefix            *,
    struct Infix             *,
    struct Suffix            *,
    struct Exfix             *,
    struct Operator          *
>;


using ExprPtr = std::shared_ptr<struct Expr>;

struct Expr {
    ssize_t var_ID{-1};
    ssize_t constant_ID{-1};

    virtual ~Expr() = default;
    virtual std::string stringify(const size_t = 0) const = 0;
    virtual bool involvesName(const std::string_view) const = 0;
    virtual Node variant() = 0;
};



} // namespace expr
} // namespace pie