#pragma once

#include <print>
#include <string>
#include <filesystem>
#include <utility>

#include "../Lex/Lexer.hxx"
// #include "../Preprocessor/Preprocessor.hxx"
#include "../Parser/Parser.hxx"
#include "../Analysis/LexicalScoping.hxx"
#include "../Interp/Interpreter.hxx"



inline namespace pie {
namespace cli {

    void help();

    void REPL(
        const std::filesystem::path canonical_root,
        const bool print_tokens,
        const bool print_parsed,
        const bool run
    );

    void runFile(
        const std::filesystem::path fname,
        const bool print_tokens,
        const bool print_parsed,
        const bool norun
    );

    void run(
        std::string src,
        const bool print_tokens,
        const bool print_parsed,
        const bool norun
    );

} // namespace cli
} // namespace pie

