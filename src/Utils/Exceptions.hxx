#pragma once

#include <string>
#include <exception>


#define DefineError(NAME)                                             \
    class NAME : public std::exception {                               \
        std::string err;                                                \
    public:                                                              \
        explicit NAME(std::string msg) noexcept : err{std::move(msg)} {}  \
        const char* what() const noexcept override { return err.c_str(); } \
    }                                                                       \


namespace pie {
namespace except {

    // struct 


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
} // namespace except
} // namespace pie

#undef DefineError
