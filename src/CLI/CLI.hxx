#pragma once

#include <string>
#include <filesystem>



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
        const bool norun,
        const bool print_ins,
        const bool vm
    );

    void run(
        std::string src,
        const bool print_tokens,
        const bool print_parsed,
        const bool norun
    );

} // namespace cli
} // namespace pie

