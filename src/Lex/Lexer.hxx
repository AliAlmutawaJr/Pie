#pragma once

#include "Token.hxx"

#include <cctype>
#include <string>
#include <string_view>


namespace pie {
namespace lex {

enum class CharClass {
    NONE        ,
    DIGIT       ,
    NAME        ,
    ASSIGN      ,
    DOT         ,
    COMMA       ,
    COLON       ,
    SEMI        ,
    QUOTE       ,
    BACKTICK    ,
    OPEN_PAREN  ,
    CLOSED_PAREN,
    OPEN_BRACE  ,
    CLOSED_BRACE,
    NEW_LINE    ,
};


[[nodiscard]] token::TokenKind keyword(const std::string_view word) noexcept;

[[nodiscard]] bool validNameChar(const char c) noexcept;

[[nodiscard]] CharClass classify(const char) noexcept;

[[nodiscard]] token::Tokens lex(const std::string& src, const bool check_for_semis = true);



} // namespace lex
} // namespace pie
