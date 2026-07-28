#pragma once

#include <concepts>
#include <string>
#include <vector>
#include <memory>

#include "../Declarations.hxx"


inline namespace pie {

namespace expr {
    struct Expr;
    using ExprPtr = std::shared_ptr<expr::Expr>;
}

namespace value {
    struct ClassValue;
    struct Value;
    using ValuePtr = std::shared_ptr<Value>;
}

namespace interp { class Visitor; }

namespace type {

    using TypePtr = std::shared_ptr<struct Type>;

    struct Type {

        ssize_t ID = -1;
        virtual std::string text(const size_t = 0) const = 0;

        virtual bool involvesT(const Type&) const = 0;
        virtual bool typeCheck(interp::Visitor*, const value::Value&, const TypePtr&) const = 0;

        virtual bool operator==(const Type& other) const { return text() == other.text(); }
        virtual bool operator> (const Type&) const = 0;
        virtual bool operator>=(const Type&) const = 0;

        virtual TypePtr clone() const = 0;

        virtual ~Type() = default;
    };



    struct ExprType : Type {
        expr::ExprPtr t;

        explicit ExprType(expr::ExprPtr s) noexcept : t{std::move(s)} {}

        std::string text(const size_t indent = 0) const override;
        bool involvesT(const Type& T) const override;
        bool typeCheck(interp::Visitor*, const value::Value&, const TypePtr& other) const override { return *this >= *other; }

        bool operator>(const Type& other) const override;
        bool operator>=(const Type& other) const override;


        TypePtr clone() const override { return std::make_shared<ExprType>(*this); }
    };


    struct BuiltinType : Type {
        std::string t;

        explicit BuiltinType(std::string s) noexcept : t{std::move(s)} { }

        std::string text(const size_t = 0) const override { return t; };
        bool involvesT(const Type& T) const override { return T == *this; }
        bool typeCheck(interp::Visitor*, const value::Value&, const TypePtr& other) const override { return *this >= *other; }

        bool operator> (const Type& other) const override;
        bool operator>=(const Type& other) const override;

        TypePtr clone() const override { return std::make_shared<BuiltinType>(*this); }
    };


    struct BuiltinFunctionType : Type {

        explicit BuiltinFunctionType() noexcept { }

        std::string text(const size_t = 0) const override { return "BuiltinFunction"; }
        bool involvesT(const Type& T) const override { return T == *this; }
        bool typeCheck(interp::Visitor*, const value::Value&, const TypePtr& other) const override { return *this >= *other; }

        bool operator> (const Type&) const override { return false; }
        bool operator>=(const Type&) const override { return false; }

        TypePtr clone() const override { return std::make_shared<BuiltinFunctionType>(*this); }
    };


    struct ValueType : Type {
        std::shared_ptr<pie::value::Value> val;

        explicit ValueType(std::shared_ptr<pie::value::Value> v) noexcept : val{std::move(v)} {}

        std::string text(const size_t indent = 0) const override;
        bool involvesT(const Type& T) const override { return T == *this; }
        bool typeCheck(interp::Visitor*, const value::Value&, const TypePtr&) const override;

        // // no value is greater than another value
        bool operator>(const Type&) const override { return false; }
        // // since no value is greater than another, >= checks for equality only
        bool operator>=(const Type& other) const override {
            return *this == other; // I think that's fine
        }

        TypePtr clone() const override { return std::make_shared<ValueType>(*this); }
    };


    struct ConceptType : Type {
        value::ValuePtr func; // should this be std::shared_ptr<expr::Closure>?? or 

        explicit ConceptType(value::ValuePtr v) noexcept : func{std::move(v)} {}

        std::string text(const size_t indent = 0) const override;
        bool involvesT(const Type& T) const override { return T == *this; }
        bool typeCheck(interp::Visitor*, const value::Value&, const TypePtr&) const override;

        // // no value is greater than another value
        bool operator>(const Type&) const override { return false; }
        // // since no value is greater than another, >= checks for equality only
        bool operator>=(const Type& other) const override { return *this == other; }

        TypePtr clone() const override { return std::make_shared<ConceptType>(*this); }
    };


    struct LiteralType : Type {
        std::shared_ptr<pie::value::ClassValue> cls;
        // std::string t;

        explicit LiteralType(std::shared_ptr<pie::value::ClassValue> c) noexcept : cls{std::move(c)} {}

        std::string text(const size_t indent = 0) const override;
        bool involvesT(const Type& T) const override { return T == *this; }
        bool typeCheck(interp::Visitor*, const value::Value&, const TypePtr& other) const override {
            return *this >= *other; // not checking value...for now at least to see how things go
        }

        bool operator>(const Type& other) const override;
        bool operator>=(const Type& other) const override;

        TypePtr clone() const override { return std::make_shared<LiteralType>(*this); }
    };


    struct UnionType : Type {
        std::vector<TypePtr> types;

        explicit UnionType(std::vector<TypePtr> ts) noexcept : types{std::move(ts)} {}

        std::string text(const size_t indent = 0) const override;
        bool involvesT(const Type& T) const override;
        bool typeCheck(interp::Visitor*, const value::Value&, const TypePtr&) const override;

        bool operator>(const Type& other) const override;
        bool operator>=(const Type& other) const override;

        TypePtr clone() const override { return std::make_shared<UnionType>(*this); }
    };


    // struct SpaceType : Type {

    //     std::string text(const size_t = 0) const override { return "Space"; }
    //     bool involvesT(const Type& T) const override { return T == *this; }
    //     bool typeCheck(interp::Visitor*, [[maybe_unused]] const value::Value& v, const TypePtr& other) const override { return *this >= *other; }

    //     // a namespace is only not greater than any other type...
    //     bool operator>(const Type&) const override { return false; }
    //     // ...so >= only needs to check for equality!
    //     bool operator>=(const Type& other) const override { return *this == other; }

    //     TypePtr clone() const override { return std::make_shared<SpaceType>(*this); }
    // };


    struct FuncType final : Type {
        std::vector<TypePtr> params;
        TypePtr ret;

        FuncType(std::vector<TypePtr> ps, TypePtr r) noexcept : params{std::move(ps)}, ret{std::move(r)} {}

        std::string text(const size_t = 0) const override;
        bool involvesT(const Type& T) const override;
        bool typeCheck(interp::Visitor*, const value::Value&, const TypePtr& other) const override { return *this >= *other; }

        bool operator>(const Type& other) const override;
        bool operator>=(const Type& other) const override;

        TypePtr clone() const override { return std::make_shared<FuncType>(*this); }
    };

    struct VariadicType final : Type {
        TypePtr type;

        explicit VariadicType(TypePtr t) : type{std::move(t)} {}

        std::string text(const size_t = 0) const override;
        bool involvesT(const Type& T) const override { return type->involvesT(T); }
        bool typeCheck(interp::Visitor*, const value::Value&, const TypePtr& other) const override { return *this >= *other; }

        bool operator>(const Type& other) const override;
        bool operator>=(const Type& other) const override;

        TypePtr clone() const override { return std::make_shared<VariadicType>(*this); }
    };

    struct ListType final : Type {
        TypePtr type;

        explicit ListType(TypePtr t) : type{std::move(t)} {}

        std::string text(const size_t = 0) const override;
        bool involvesT(const Type& T) const override { return type->involvesT(T); }
        bool typeCheck(interp::Visitor*, const value::Value&, const TypePtr& other) const override; // return *this >= *other;


        bool operator>(const Type& other) const override;
        bool operator>=(const Type& other) const override;

        TypePtr clone() const override { return std::make_shared<ListType>(*this); }
    };

    struct MapType final : Type {
        TypePtr key_type;
        TypePtr val_type;

        MapType(TypePtr t1, TypePtr t2) : key_type{std::move(t1)}, val_type{std::move(t2)} {}

        std::string text(const size_t indent = 0) const override;
        bool involvesT(const Type& T) const override { return key_type->involvesT(T) or val_type->involvesT(T); }
        bool typeCheck(interp::Visitor*, const value::Value&, const TypePtr&) const override; // return *this >= *other; 

        bool operator>(const Type& other) const override;
        bool operator>=(const Type& other) const override;

        TypePtr clone() const override { return std::make_shared<MapType>(*this); }
    };


    struct TryReassign : Type {
        std::string text(const size_t = 0) const override { return ""; }
        bool involvesT(const Type&) const override { return false; }
        bool typeCheck(interp::Visitor*, const value::Value&, const TypePtr&) const override { return true; };

        bool operator> (const Type& other) const override {
            if (dynamic_cast<const TryReassign*>(&other)) return true;

            return false;
        };
        bool operator>=(const Type& other) const override {
            if (dynamic_cast<const TryReassign*>(&other)) return true;

            return false;
        };

        TypePtr clone() const override { return std::make_shared<TryReassign>(*this); }
    };


    //* these builtins could be their own types!
    namespace builtins {
        inline TypePtr Int    () { return std::make_shared<BuiltinType>("Int"   ); }
        inline TypePtr Double () { return std::make_shared<BuiltinType>("Double"); }
        inline TypePtr Bool   () { return std::make_shared<BuiltinType>("Bool"  ); }
        inline TypePtr String () { return std::make_shared<BuiltinType>("String"); }
        inline TypePtr Any    () { return std::make_shared<BuiltinType>("Any"   ); }
        inline TypePtr Syntax () { return std::make_shared<BuiltinType>("Syntax"); }
        inline TypePtr Type   () { return std::make_shared<BuiltinType>("Type"  ); }

        inline TypePtr BuiltinFunction() { return std::make_shared<BuiltinFunctionType>(); }

        inline TypePtr _      () { return std::make_shared<TryReassign>(); };
    }


    inline TypePtr VariadicOf(TypePtr type) {
        return std::make_shared<VariadicType>(std::move(type));
    }

    inline TypePtr ListOf(TypePtr type) {
        return std::make_shared<ListType>(std::move(type));
    }

    inline TypePtr MapOf(TypePtr type1, TypePtr type2) {
        return std::make_shared<MapType>(std::move(type1), std::move(type2));
    }

    template <std::same_as<TypePtr>... Ts>
    inline TypePtr UnionOf(Ts... types) {
        return std::make_shared<UnionType>(std::vector<TypePtr>{std::move(types)...});
    }

    inline TypePtr UnionOf(std::vector<TypePtr> types) {
        return std::make_shared<UnionType>(std::move(types));
    }



    inline ExprType* isExpr(const TypePtr& t) noexcept {
        return dynamic_cast<ExprType*>(t.get());
    }

    inline FuncType* isFunction(const TypePtr& t) noexcept {
        return dynamic_cast<FuncType*>(t.get());
    }

    inline LiteralType* isClass(const TypePtr& t) noexcept {
        return dynamic_cast<LiteralType*>(t.get());
    }

    inline ValueType* isValue(const TypePtr& t) noexcept {
        return dynamic_cast<ValueType*>(t.get());
    }

    inline UnionType* isUnion(const TypePtr& t) noexcept {
        return dynamic_cast<UnionType*>(t.get());
    }

    inline VariadicType* isVariadic(const TypePtr& t) noexcept {
        return dynamic_cast<VariadicType*>(t.get());
    }

    inline ListType* isList(const TypePtr& t) noexcept {
        return dynamic_cast<ListType*>(t.get());
    }

    inline MapType* isMap(const TypePtr& t) noexcept {
        return dynamic_cast<MapType*>(t.get());
    }

    inline BuiltinType* isBuiltin(const TypePtr& t) noexcept {
        return dynamic_cast<BuiltinType*>(t.get());
    }

    inline bool isAny(const TypePtr& t) noexcept {
        return isBuiltin(t) && t->text() == "Any";
    }

    inline bool isSyntax(const TypePtr& t) noexcept {
        return isBuiltin(t) && t->text() == "Syntax";
    }

    inline bool isType(const TypePtr& t) noexcept {
        return isBuiltin(t) && t->text() == "Type";
    }

    inline bool shouldReassign(const TypePtr& t) {
        return dynamic_cast<const TryReassign*>(t.get());
    }
}


} // namespace pie