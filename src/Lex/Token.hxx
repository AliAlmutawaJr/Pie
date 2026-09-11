#pragma once

#include <format>
#include <iostream>
#include <ostream>
#include <sstream>
#include <utility>
#include <vector>
#include <string>
#include <ranges>

// #include "../Declarations.hxx"

namespace pie {

namespace util {

    struct SourceLocation { size_t line, column; };
    using SourceSpan = std::pair<SourceLocation, SourceLocation>;
    // struct SourceSpan { std::pair<size_t, size_t> lines, columnsI ; };

}


namespace token {

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
    FSTRING,

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
        case NAME:          return "NAME"   ;
        case INT:           return "INT"    ;
        case FLOAT:         return "FLOAT"  ;
        case BOOL:          return "BOOL"   ;
        case STRING :       return "STRING" ;
        case FSTRING:       return "FSTRING";
        case END:           return "END"    ;

        // punctuation
        case L_BRACE :      return "L_BRACE"      ;
        case R_BRACE :      return "R_BRACE"      ;
        case L_PAREN :      return "L_PAREN"      ;
        case R_PAREN :      return "R_PAREN"      ;
        case COMMA   :      return "COMMA"        ;
        case BACKTICK:      return "BACKTICK"     ;
        case CASCADE :      return "CASCADE"      ;
        case ELLIPSIS:      return "ELLIPSIS"     ;
        case DOT     :      return "DOT"          ;
        case SEMI    :      return "SEMI"         ;
        case COLON   :      return "COLON"        ;
        case WALRUS  :      return "WALRUS"       ;
        case SCOPE_RESOLVE: return "SCOPE_RESOLVE";

        // should make them weak keywords
        case ASSIGN:        return "ASSIGN"   ;
        case FAT_ARROW:     return "FAT_ARROW";

        // keywords
        case MIXFIX:        return "MIXFIX";
        case PREFIX:        return "PREFIX";
        case INFIX:         return "INFIX" ;
        case SUFFIX:        return "SUFFIX";
        case EXFIX:         return "EXFIX" ;
        case CLASS:         return "CLASS" ;
        case UNION:         return "UNION" ;
        case MATCH:         return "MATCH" ;

        case LOOP    :      return "LOOP"    ;
        case BREAK   :      return "BREAK"   ;
        case CONTINUE:      return "CONTINUE";

        case IMPORT:        return "IMPORT"   ;
        case NAMESPACE:     return "NAMESPACE";
        case USE:           return "USE"      ;

        case NONE:
            std::println(std::cerr, "Couldn't stringify token: {}", std::to_underlying(token));
            exit(1);
    }

    return "<UNNAMED>";
}


struct Token {
    TokenKind kind;
    std::string text;

    util::SourceSpan span;
    std::vector<std::pair<size_t, std::vector<Token>>> fstring_tokens;


    bool operator==(const Token& that) const { return kind == that.kind and text == that.text; }
};


using Tokens = std::vector<Token>;
using TokenLines = std::vector<Tokens>;


inline std::ostream& operator<<(std::ostream& os, const Token& token) {
    os << "Token{" << stringify(token.kind) << ", ";

    if (token.kind == TokenKind::FSTRING) {
        os << '"' << token.text << '"' << ", [";
        for (const auto& [ind, tokens] : token.fstring_tokens) {
            // guaranteed by the lexer to have at least one element
            os << "{" << ind << ": " << tokens.front() << "}";
            for (const auto& token : tokens | std::views::drop(1)) {
                os << ", {" << ind << ": " << token << "}";
            }
        }

        os << ']';
    }
    else if (token.kind == TokenKind::STRING) {
        os << '"' << token.text << '"';
    }

    #if !NO_ERR_LOC
        os << ", <" << token.span.first .line << ":" << token.span.first .column
           << ", "  << token.span.second.line << ":" << token.span.second.column << ">";
    #endif

    return os << '}';
}


} // namespace token
} // namespace pie


template <>
struct std::formatter<pie::token::Token> : std::formatter<std::string> {
    auto format(const pie::token::Token& token, auto& ctx) const {
        if (
            token.kind == pie::token::TokenKind::FSTRING or
            token.kind == pie::token::TokenKind::STRING
        ) {
            std::stringstream ss;
            ss << token;
            return std::format_to(ctx.out(), "{}", ss.str());
        }
        else {
            #if NO_ERR_LOC
                return std::format_to(ctx.out(), "Token{{{}, '{}'}}", stringify(token.kind), token.text);
            #else
                return std::format_to(
                    ctx.out(),
                    "Token{{{}, '{}', <{}:{}, {}:{}>}}",
                    stringify(token.kind),
                    token.text,
                    token.span.first .line, token.span.first .column,
                    token.span.second.line, token.span.second.column
                );
            #endif
        }
    }
};


template <>
struct std::formatter<std::vector<pie::token::Token>> : std::formatter<std::string_view> {
    auto format(const std::vector<pie::token::Token>& vec, auto& ctx) const {
        std::string result = "[\n";
        bool first = true;
        for (const auto& token : vec) {
            if (!first) result += ", \n";
            result += std::format("{}", token);
            first = false;
        }
        result += "\n]";
        return std::format_to(ctx.out(), "{}", result);
    }
};
