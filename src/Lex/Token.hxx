#pragma once

#include <iostream>
#include <ostream>
#include <vector>
#include <string>

inline namespace pie {
inline namespace token {

enum class TokenKind {
    NONE = 0,



    SEMI,
    SCOPE_RESOLVE,
    COLON,
    WALRUS, // :=
    ASSIGN,
    FAT_ARROW,

    COMMA,

    BACKTICK,
    DOT,
    CASCADE, // for future support
    ELLIPSIS,

    NAME,
    INT,
    FLOAT,
    BOOL,
    STRING,

    L_BRACE,
    R_BRACE,

    L_PAREN,
    R_PAREN,


// Keywords
    MIXFIX,
    PREFIX,
    INFIX,
    SUFFIX,
    EXFIX,
    CLASS,
    UNION,
    MATCH,
    LOOP,
    BREAK,
    CONTINUE,
    IMPORT,
    NAMESPACE,
    USE,


    END,
};


constexpr const char* stringify(const TokenKind token) noexcept {
    switch (token) {
        using enum TokenKind;
        case NAME:          return "NAME";
        case INT:           return "INT";
        case FLOAT:         return "FLOAT";
        case BOOL:          return "BOOL";
        case STRING:        return "STRING";
        case END:           return "END";

        // punctuation
        case L_BRACE :      return "L_BRACE";
        case R_BRACE :      return "R_BRACE";
        case L_PAREN :      return "L_PAREN";
        case R_PAREN :      return "R_PAREN";
        case COMMA   :      return "COMMA";
        case BACKTICK:      return "BACKTICK";
        case CASCADE :      return "CASCADE";
        case ELLIPSIS:      return "ELLIPSIS";
        case DOT     :      return "DOT";
        case SEMI    :      return "SEMI";
        case COLON   :      return "COLON";
        case WALRUS  :      return "WALRUS";
        case SCOPE_RESOLVE: return "SCOPE_RESOLVE";

        // should make them weak keywords
        case ASSIGN:        return "ASSIGN";
        case FAT_ARROW:     return "FAT_ARROW";

        // keywords
        case MIXFIX:        return "MIXFIX";
        case PREFIX:        return "PREFIX";
        case INFIX:         return "INFIX";
        case SUFFIX:        return "SUFFIX";
        case EXFIX:         return "EXFIX";
        case CLASS:         return "CLASS";
        case UNION:         return "UNION";
        case MATCH:         return "MATCH";

        case LOOP    :      return "LOOP";
        case BREAK   :      return "BREAK";
        case CONTINUE:      return "CONTINUE";

        case IMPORT:        return "IMPORT";
        case NAMESPACE:     return "NAMESPACE";
        case USE:           return "USE";

        case NONE:
            std::println(std::cerr, "Internal Error!");
            exit(1);
    }

    return "<UNNAMED>";
}



struct Token {
    TokenKind kind;
    std::string text;

    bool operator==(const Token&) const = default;
};


using Tokens = std::vector<Token>;
using TokenLines = std::vector<Tokens>;


inline std::ostream& operator<<(std::ostream& os, const Token& token) {
    return os << "Token{" << stringify(token.kind) << ", '" << token.text << "'}";
}


} // namespace token
} // namespace pie


template <>
struct std::formatter<Token> : std::formatter<std::string> {
    auto format(const Token& token, auto& ctx) const {
        return std::format_to(ctx.out(), "Token{{{}, '{}'}}", stringify(token.kind), token.text);
    }
};


template <>
struct std::formatter<std::vector<Token>> : std::formatter<std::string_view> {
    auto format(const std::vector<Token>& vec, auto& ctx) const {
        std::string result = "[";
        bool first = true;
        for (const auto& token : vec) {
            if (!first) result += ", ";
            result += std::format("{}", token);
            first = false;
        }
        result += "]";
        return std::format_to(ctx.out(), "{}", result);
    }
};