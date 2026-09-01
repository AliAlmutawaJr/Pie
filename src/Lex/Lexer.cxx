#include "Lexer.hxx"

#include <cstdio>

#include "Token.hxx"
#include "../Utils/utils.hxx"
#include "../Utils/Exceptions.hxx"


namespace pie {
namespace lex {


token::TokenKind keyword(const std::string_view word) noexcept {
    using enum token::TokenKind;
    if (word == "mixfix") return MIXFIX;
    else if (word == "prefix") return PREFIX;
    else if (word == "infix" ) return INFIX ;
    else if (word == "suffix") return SUFFIX;
    else if (word == "exfix" ) return EXFIX ;

    else if (word == "class") return CLASS;
    else if (word == "union") return UNION;
    else if (word == "match") return MATCH;

    else if (word == "loop"    ) return LOOP;
    else if (word == "break"   ) return BREAK;
    else if (word == "continue") return CONTINUE;

    else if (word == "import") return IMPORT;
    else if (word == "space" ) return NAMESPACE;

    else if (word == "use") return USE;

    else if (word == "true"  ) return BOOL;
    else if (word == "false" ) return BOOL;

    return NAME;
}


bool validNameChar(const char c) noexcept {
    switch (c) {
        case '?':
        case '!':
        case '@':
        case '#':
        case '$':
        case '%':
        case '^':
        case '&':
        case '|':
        case '*':
        case '+':
        case '~':
        case '-':
        case '_':
        case '\\':
        case '\'':
        case '/':
        case '<':
        case '>':
        case '[':
        case ']':
        case '=': // function would only be used when checking chars that are not the first in the name
            return true;
    }

    return isalnum(c);
}

token::Tokens lex(const std::string& src, const bool check_for_semis) {
    token::TokenLines lines = {{}};
    token::Tokens line;

    const auto emplace = [&lines] (auto&&... args) {
        return lines.back().emplace_back(std::forward<decltype(args)>(args)...);
    };

    const auto push = [&lines] (token::Token token) {
        return lines.back().push_back(std::move(token));
    };


    [[maybe_unused]] size_t column_count = 1;
    size_t line_count = 1;
    size_t line_starting_index{};


    for (size_t index{}; index < src.length(); ++index) {
        try {
        switch (src[index]) {
            using enum token::TokenKind;

            #pragma GCC diagnostic push
            #pragma GCC diagnostic ignored "-Wpedantic"
            case '0' ... '9':
            #pragma GCC diagnostic pop
            {
                const auto beginning = index;
                while (++index < src.size() and isdigit(static_cast<unsigned char>(src[index])));

                bool is_name = validNameChar(src[index]);
                if (is_name) {
                    while (++index < src.size() and validNameChar(src[index]));
                    emplace(NAME, src.substr(beginning, index - beginning));
                    --index;
                    break;
                }

                bool is_float = false;
                if (src[index] == '.' and isdigit(static_cast<unsigned char>(src.at(index + 1)))) {
                    is_float = true;
                    while (isdigit(static_cast<unsigned char>(src.at(++index))));
                }

                emplace(is_float ? FLOAT : INT, src.substr(beginning, index - beginning));
                --index;
            } break;


            case '?':
            case '!':
            case '@':
            case '#':
            case '$':
            case '%':
            case '^':
            case '&':
            case '|':
            case '*':
            case '+':
            case '~':
            case '-':
            case '_':
            case '\\':
            case '\'':
            case '/':
            case '<':
            case '>':
            case '[':
            case ']':
            #pragma GCC diagnostic push
            #pragma GCC diagnostic ignored "-Wpedantic"
            case 'a' ... 'z':
            case 'A' ... 'Z':
            #pragma GCC diagnostic pop
            {
                const auto beginning = index;
                while (++index < src.size() and validNameChar(src[index]));

                const auto word = src.substr(beginning, index - beginning);


                if (word == "__TEXT__") [[unlikely]] {
                    std::string line_text;

                    for (size_t ind = line_starting_index; ind < src.size() and src[ind] != '\n'; ++ind)
                        line_text += src[ind];

                    push({STRING, line_text, {}});
                    --index;
                    break;
                }

                if (word == "__LINE__") [[unlikely]] {
                    push({INT, std::to_string(line_count), {}});
                    --index;
                    break;
                }


                const token::TokenKind token = keyword(word);

                emplace(token, word);
                --index;
            } break;


            case '=':
                if (src.at(index + 1) == '>')
                    push({FAT_ARROW, {src[index], src[++index]}, {}});
                // allows for "==" to be used as a name
                else if ((src[index + 1] == '=')) {
                    const auto beginning = index++;
                    for (; src.at(index + 1) == '='; ++index);

                    push({NAME, src.substr(beginning, index - beginning + 1), {}});
                }
                else
                    push({ASSIGN, {src[index]}, {}});

                break;

            case ',': push({COMMA, {src[index]}, {}}); break;
            case '.':
                if (src.at(index + 1) == ':') {
                    if (src.at(index + 2) == ':') {
                        for(
                            index += 2;
                            src.substr(index, 3) != "::.";
                            ++index
                        ) {
                            if (src[index] == '\n') {
                                ++line_count;
                                column_count = 1;
                            }
                        }

                        index += 2;
                    }
                    else {
                        while(++index < src.length() and src[index] != '\n');
                        ++line_count;
                        column_count = 1;
                    }
                }
                else if (src[index + 1] == '.' and src.at(index + 2) == '.')
                    push({ELLIPSIS, {src[index], src[++index], src[++index]}, {}});
                else if (src[index + 1] == '.')
                    push({CASCADE , {src[index], src[++index], {}}, {}});
                else
                    push({DOT, {src[index]}, {}});

                break;

            case ':': 
                if      (src.at(index + 1) == ':') push({SCOPE_RESOLVE, "::", {}}), ++index;
                else if (src      [index + 1] == '=') push({WALRUS       , ":=", {}}), ++index;
                else                                  push({COLON, ":", {}});
                break;

            case ';':
                push({SEMI, ";", {}});
                lines.push_back({});
                break;

            case '`': push({BACKTICK, "`", {}}); break;

            case '\n':
                ++line_count;
                column_count = 1;
                line_starting_index = index + 1;
                break;

            case '(': push({L_PAREN, {src[index]}, {}}); break;
            case ')': push({R_PAREN, {src[index]}, {}}); break;

            case '{': push({L_BRACE, {src[index]}, {}}); break;

            case '}': push({R_BRACE, {src[index]}, {}}); break;

            case '"': {
                size_t str_len{};
                std::string str;
                std::vector<std::pair<size_t, token::Tokens>> fstring_tokens;

                while(src.at(++index) != '"') {
                    const char c = src[index];

                    if (c == '\\') {
                        switch (src[++index]) {
                            // f-strings
                            case '{': str.push_back('{'); break;
                            case '}': str.push_back('}'); break;

                            case '\\': str.push_back('\\'); break;
                            case '"' : str.push_back('"' ); break;
                            case 'n' : 
                                str.push_back('\n');
                                ++line_count;
                                column_count = 1;
                                break;
                            case 't' : str.push_back('\t'); break;
                            case 'v' : str.push_back('\v'); break;
                            case 'b' : str.push_back('\b'); break;
                            case 'r' : str.push_back('\r'); break;
                            case 'f' : str.push_back('\f'); break;
                            case 'a' : str.push_back('\a'); break;

                            default:
                                util::error<except::LexerError>(std::string{"Invalid escape character: \\"} + src[index]);
                            // case '\0': str.push_back('\0');
                        }
                    }
                    else if (c == '{') { // this is an fstring
                        // find the closing `}`
                        size_t closing_brace = index;
                        for (size_t i{index + 1}, balance{}; i < src.size(); ++i) {
                            if (src[i] == '{') {
                                ++balance;
                                continue;
                            }

                            if (src[i] == '}') {
                                if (balance == 0) {
                                    closing_brace = i;
                                    break;
                                }
                                --balance;
                            }
                        }

                        // if closing brace == index or index + 1
                        if (closing_brace <= index + 1) util::error<except::LexerError>("Invalid fstring!");

                        auto substr = src.substr(index + 1, closing_brace - index - 1);

                        for (size_t i{}; i < substr.size(); ++i) {
                            if (substr[i] == ';' or substr[i] == '}' or substr[i] == ')')
                                util::error<except::LexerError>("Invalid Expression Inside f-string!");


                            size_t balance = substr[i] == '{';
                            while (++i < substr.size() and balance) {
                                balance += substr[i] == '{';
                                balance -= substr[i] == '}';
                            }

                            if (balance) util::error<except::LexerError>("Imbalanced braces inside ");
                        }

                        fstring_tokens.push_back({str_len - fstring_tokens.size(), lex(std::move(substr), false)});
                        index = closing_brace;
                    }
                    else {

                        line_count += (c == '\n');
                        column_count = 1;

                        str.push_back(c);
                    }

                    ++str_len;
                }

                if (fstring_tokens.empty())
                    push({STRING, str, {}});
                else
                    push({FSTRING, str, fstring_tokens});
            } break;


            default: break;
        }
        }
        catch(const except::LexerError& e) {
            throw;
        }
        catch (...) {
            util::error("Lexer Error!");
        }
        // catch(const std::exception& err) {
        //     util::error();
        //     // util::error(err.what());
        // }
    }


    if (check_for_semis and not lines.empty() and not lines.back().empty() and lines.back().back().kind != token::TokenKind::SEMI)
        util::error("Last line doesn't end with a ';'!");


    if (lines.size() > 1) {
        lines.pop_back();
        emplace(token::TokenKind::END, "EOF");
    }


    token::Tokens tokens;
    for (auto&& line : lines)
        for (auto&& t : line)
            tokens.push_back(std::move(t));

    return tokens;
}


} // namespace lex
} // namespace pie
