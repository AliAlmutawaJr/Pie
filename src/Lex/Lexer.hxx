#pragma once

#include "Token.hxx"

#include <cctype>
#include <string>
#include <string_view>


namespace pie {
namespace lex {


token::TokenKind keyword(const std::string_view word) noexcept;


bool validNameChar(const char c) noexcept;


[[nodiscard]] token::Tokens lex(const std::string& src, const bool check_for_semis = true);



} // namespace lex
} // namespace pie
