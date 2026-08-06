#include <print>
#include <iostream>
#include <exception>
#include <string_view>
#include <filesystem>


#include "CLI/CLI.hxx"


int main(int argc, char *argv[]) {

    using std::operator""sv;
    #if not WEB_PIE
    const auto canonical_root = std::filesystem::canonical(*argv);
    #endif

    bool print_tokens       = false;
    bool print_parsed       = false;
    bool print_help         = false;
    bool norun              = false;
    bool repl               = false;
    bool vm                 = false;
    bool command            = false;


    std::string_view content;

    // this would leave file name at argv[1]
    for(; argc > 1; --argc, ++argv) {
        if      (argv[1] == "-t"sv or argv[1] == "--tokens"sv ) print_tokens       = true ;
        else if (argv[1] == "-a"sv or argv[1] == "--ast"sv    ) print_parsed       = true ;
        else if (argv[1] == "-h"sv or argv[1] == "--help"sv   ) print_help         = true ;
        else if (argv[1] == "-r"sv or argv[1] == "--norun"sv  ) norun              = true;
        else if (argv[1] == "-c"sv or argv[1] == "--vm"sv     ) vm                 = true ;
        else if (argv[1] == "-c"sv or argv[1] == "--command"sv) command            = true ;
        else if (argv[1] == "-repl"sv                         ) repl               = true ;
        else content = argv[1];
    }


    try {

        if (vm) {

            pie::cli::runFile(
                std::filesystem::path(content),
                print_tokens,
                print_parsed,
                norun,
                true
            );

            return 0;
        }

        if (command) {
            pie::cli::run(std::string{content}, print_tokens, print_parsed, norun);
            return 0;
        }

        if (print_help) {
            pie::cli::help();
            return 0;
        }

        #if not WEB_PIE
        if (content.empty() or repl) {
            pie::cli::REPL(
                std::move(canonical_root),
                print_tokens, print_parsed, norun
            );
        }
        else {
            pie::cli::runFile(
                std::filesystem::path(content),
                print_tokens,
                print_parsed,
                norun
            );
        }
        #endif
    }
    catch(const std::exception& e) {
        std::println(std::cerr, "{}", e.what());
        return 1;
    }
}