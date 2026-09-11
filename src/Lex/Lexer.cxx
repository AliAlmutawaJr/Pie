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


CharClass classify(const char c) noexcept {
    using enum CharClass;

    if (c >= '0' and c <= '9') return DIGIT;
    if (
        (c >= 'a' and c <= 'z') or
        (c >= 'A' and c <= 'Z')
    ) return NAME;

    switch (c) {
        case '=': return ASSIGN  ;
        case ':': return COLON   ;
        case ',': return COMMA   ;
        case '.': return DOT     ;
        case ';': return SEMI    ;

        case '"': return QUOTE   ;
        case '`': return BACKTICK;

        case '(': return OPEN_PAREN  ;
        case ')': return CLOSED_PAREN;

        case '{': return OPEN_BRACE  ;
        case '}': return CLOSED_BRACE;

        case '\n': return NEW_LINE;

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
        return NAME;
    }


    return NONE;
    // util::error<except::LexerError>(std::string{"Uknown char: "} + c);
}


token::Tokens lex(const std::string& src, const bool check_for_semis) {
    token::TokenLines lines = {{}};
    token::Tokens line;


    size_t from_line   = 1, to_line = 1;
    size_t from_column = 1, to_column = 1;
    size_t line_starting_index{};


    const auto emplace = [&] (auto&&... args) {

        if constexpr (sizeof...(args) == 2) {
            return lines.back().emplace_back(
                std::forward<decltype(args)>(args)...,
                util::SourceSpan{{from_line, from_column}, {to_line, to_column}}
            );
        }
        else
            return lines.back().emplace_back(std::forward<decltype(args)>(args)...);
    };

    // const auto push = [&] (token::Token token) {
    //     return lines.back().push_back(std::move(token));
    // };



    for (size_t index{}; index < src.length(); ++index, ++to_column) {
        // catch up!
        from_line   = to_line  ;
        from_column = to_column;


        try {
        switch (classify(src[index])) {
            using enum token::TokenKind;
            // using enum CharClass;
            using CC = CharClass;
            using util::SourceSpan;

            case CC::DIGIT: {
                const auto beginning = index;
                // idky i obsecure code so much but it's kinda fun
                for (++to_column; ++index < src.size() and isdigit(static_cast<unsigned char>(src[index])); ++to_column);

                bool is_name = validNameChar(src[index]);
                if (is_name) {
                    for (++to_column; ++index < src.size() and validNameChar(src[index]); ++to_column);
                    --to_column;
                    emplace(NAME, src.substr(beginning, index - beginning));
                    --index;
                    break;
                }

                bool is_float = false;
                if (src[index] == '.' and isdigit(static_cast<unsigned char>(src.at(index + 1)))) {
                    is_float = true;
                    for (++to_column; isdigit(static_cast<unsigned char>(src.at(++index))); ++to_column);
                }

                --to_column;
                emplace(is_float ? FLOAT : INT, src.substr(beginning, index - beginning));
                --index;
            } break;


            case CC::NAME: {
                const auto beginning = index;
                for (++to_column; ++index < src.size() and validNameChar(src[index]); ++to_column);

                --to_column;
                const auto word = src.substr(beginning, index - beginning);
                --index;

                if (word == "__TEXT__") [[unlikely]] {
                    std::string line_text;

                    for (size_t ind = line_starting_index; ind < src.size() and src[ind] != '\n'; ++ind)
                        line_text += src[ind];

                    emplace(STRING, line_text);
                    break;
                }

                if (word == "__LINE__") [[unlikely]] {
                    emplace(INT, std::to_string(to_line));
                    break;
                }


                const token::TokenKind token = keyword(word);

                emplace(token, word);
            } break;


            case CC::ASSIGN:
                if (src.at(index + 1) == '>') {
                    ++to_column;
                    emplace(FAT_ARROW, std::string{src[index], src[++index]});
                }
                // allows for "==" to be used as a name
                else if ((src[index + 1] == '=')) {
                    const auto beginning = index++;
                    for (++to_column; src.at(index + 1) == '='; ++index, ++to_column);

                    emplace(NAME, src.substr(beginning, index - beginning + 1));
                }
                else
                    emplace(ASSIGN, std::string{src[index]});

                break;

            case CC::COMMA: emplace(COMMA, std::string{src[index]}); break;
            case CC::DOT:
                if (src.at(index + 1) == ':') {
                    if (src.at(index + 2) == ':') {
                        for(
                            index += 2;
                            src.substr(index, 3) != "::.";
                            ++index
                        ) {
                            if (src[index] == '\n') {
                                ++to_line;
                                to_column = 0;
                            }
                        }

                        index += 2;
                    }
                    else {
                        while(++index < src.length() and src[index] != '\n');
                        ++to_line;
                        to_column = 0;
                    }
                }
                else if (src[index + 1] == '.' and src.at(index + 2) == '.') {
                    to_column += 2;
                    emplace(ELLIPSIS, std::string{src[index], src[++index], src[++index]});
                }
                else if (src[index + 1] == '.') {
                    ++to_column;
                    emplace(CASCADE ,std::string {src[index], src[++index]});
                }
                else
                    emplace(DOT, std::string{src[index]});

                break;

            case CC::COLON: 
                if (src.at(index + 1) == ':') {
                    ++to_column;
                    ++index;
                    emplace(SCOPE_RESOLVE, "::");
                }
                else if (src[index + 1] == '=') {
                    ++to_column;
                    ++index;
                    emplace(WALRUS, ":=");
                }
                else
                    emplace(COLON, ":" );

                break;

            case CC::SEMI:
                emplace(SEMI, ";");
                lines.emplace_back();
                break;

            case CC::BACKTICK: emplace(BACKTICK, "`"); break;

            case CC::NEW_LINE:
                ++to_line;
                to_column = 0;
                line_starting_index = index + 1;
                break;

            case CC::OPEN_PAREN  : emplace(L_PAREN, std::string{src[index]}); break;
            case CC::CLOSED_PAREN: emplace(R_PAREN, std::string{src[index]}); break;


            case CC::OPEN_BRACE  : emplace(L_BRACE, std::string{src[index]}); break;
            case CC::CLOSED_BRACE: emplace(R_BRACE, std::string{src[index]}); break;


            case CC::QUOTE: {
                size_t str_len{};
                std::string str;
                std::vector<std::pair<size_t, token::Tokens>> fstring_tokens;

                while(src.at(++index) != '"') {
                    ++to_column;

                    const char c = src[index];
                    if (c == '\\') {
                        ++to_column;
                        switch (src[++index]) {
                            // to avoid f-strings
                            case '{': str.push_back('{'); break;
                            case '}': str.push_back('}'); break;

                            case '\\': str.push_back('\\'); break;
                            case '"' : str.push_back('"' ); break;
                            case 'n' : 
                                str.push_back('\n');
                                ++to_line;
                                to_column = 1;
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
                        to_column += closing_brace - index;

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

                        index = closing_brace;
                        fstring_tokens.push_back({str_len - fstring_tokens.size(), lex(std::move(substr), false)});
                    }
                    else {
                        // if (c == '\n') {
                            // ++to_line;
                            // to_column = 0
                        // };

                        to_line += (c == '\n');
                        to_column *= (1 - (c == '\n'));

                        str.push_back(c);
                    }

                    ++str_len;
                }
                ++to_column;

                if (fstring_tokens.empty())
                    emplace(STRING, str);
                else
                    emplace(FSTRING, str, SourceSpan{{from_line, from_column}, {to_line, to_column}}, fstring_tokens);
            } break;


            default:
            // ++to_column;
                break;
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
