#pragma once

#include <span>
#include <string>
#include <string_view>
#include <filesystem>



namespace pie {
namespace cli {

    void help();

    void REPL(
        const std::filesystem::path canonical_root,
        const std::span<std::string_view> argv,
        const bool print_tokens,
        const bool print_parsed,
        const bool run
    );

    void runFile(
        const std::filesystem::path,
        const std::span<std::string_view>,
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

