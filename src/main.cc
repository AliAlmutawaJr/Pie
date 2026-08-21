#include <print>
#include <iostream>
#include <exception>
#include <string_view>
#include <filesystem>

#if WEB_PIE
#include <emscripten.h>
#endif


#include "CLI/CLI.hxx"
#include "Utils/Exceptions.hxx"
#include "Utils/utils.hxx"


#if WEB_PIE
extern "C" EMSCRIPTEN_KEEPALIVE void execute(const char *code) {
    pie::cli::run(std::string{code}, false, false, false);
}
#endif


static int pieMain(int argc, char *argv[]) {
    using std::operator""s ;
    using std::operator""sv;

    const auto canonical_root = std::filesystem::canonical(*argv);

    bool print_tokens       = false;
    bool print_parsed       = false;
    bool print_help         = false;
    bool print_ins          = false;
    bool norun              = false;
    bool repl               = false;
    bool vm                 = false;
    bool command            = false;


    std::string_view fname;

    // this would leave file name at argv[1]
    for(; argc > 1; --argc, ++argv) {
        if      (argv[1] == "-t"sv  or argv[1] == "--tokens"sv ) print_tokens       = true;
        else if (argv[1] == "-a"sv  or argv[1] == "--ast"sv    ) print_parsed       = true;
        else if (argv[1] == "-h"sv  or argv[1] == "--help"sv   ) print_help         = true;
        else if (argv[1] == "-i"sv  or argv[1] == "--ins"sv    ) print_ins          = true;
        else if (argv[1] == "-n"sv  or argv[1] == "--norun"sv  ) norun              = true;
        else if (argv[1] == "-vm"sv or argv[1] == "--machine"sv) vm                 = true;
        else if (argv[1] == "-c"sv  or argv[1] == "--command"sv) command            = true;
        else if (argv[1] == "-r"sv  or argv[1] == "--repl"sv   ) repl               = true;
        else if (not fname.empty()) util::error<except::UknownOption>("Unrecognized Option: "s + argv[1]);
        else fname = argv[1];
    }



    try {

        if (command) {
            pie::cli::run(std::string{fname}, print_tokens, print_parsed, norun);
            return 0;
        }

        if (print_help) {
            pie::cli::help();
            return 0;
        }

        if (fname.empty() or repl) {
            pie::cli::REPL(
                std::move(canonical_root),
                print_tokens, print_parsed, norun
            );
        }
        else {
            pie::cli::runFile(
                std::filesystem::path(fname),
                print_tokens,
                print_parsed,
                norun,
                print_ins,
                vm
            );
        }
    }
    catch(const std::exception& e) {
        std::println(std::cerr, "{}", e.what());
        return 1;
    }

    return 0;
}



int main(int argc, char *argv[]) {
    // web pie manually hooks onto pieMain above ^
    #if not WEB_PIE
        return pieMain(argc, argv);
    #endif
}
