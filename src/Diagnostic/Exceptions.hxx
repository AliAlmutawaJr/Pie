#pragma once

#include "Lex/Token.hxx"
#include <optional>
#include <string>
#include <exception>



namespace pie {
namespace except {

    struct LocatedError {
        std::string msg;
        util::SourceSpan span;
    };


#define DefineError(NAME)                                             \
    class NAME : public std::exception, public LocatedError {          \
        std::string err;                                                \
    public:                                                              \
        explicit NAME(std::string msg) noexcept : err{std::move(msg)} {}  \
        const char* what() const noexcept override { return err.c_str(); } \
    }                                                                       \



    DefineError(UknownOption       );
    DefineError(LexerError         );
    DefineError(UnexpectedToken    );
    DefineError(OperatorError      );
    DefineError(SyntaxError        );
    DefineError(TypeMismatch       );
    DefineError(NameLookup         );
    DefineError(InvalidArgument    );
    DefineError(OpeningDyLib       );
    DefineError(DyLibSymbolLookup  );


class Error : public std::runtime_error, public LocatedError {
public:
    using std::runtime_error::runtime_error;
};


} // namespace except
} // namespace pie

#undef DefineError
