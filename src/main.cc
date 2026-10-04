#include <print>
#include <iostream>
#include <exception>
#include <string_view>
#include <filesystem>


#include "CLI/CLI.hxx"
#include "Diagnostic/Exceptions.hxx"
#include "Diagnostic/Diagnostic.hxx"
#include "Utils/utils.hxx"



#if WEB_PIE
#include <emscripten.h>


extern "C" EMSCRIPTEN_KEEPALIVE void execute(const char *code) try {
    pie::cli::run(std::string{code}, false, false, false);
}
catch (const std::exception& e) {
    std::println(std::cerr, "{}", pie::err::render(e));
}
catch (...) {
    std::println(std::cerr, "Unknown Exception!");
}
#endif



static int pieMain(int argc, char *argv[]) {
    constexpr auto VERSION = "1.11.1";

    using std::operator""s ;
    using std::operator""sv;

    // const auto canonical_root = std::filesystem::canonical(*argv);
    const auto path = pie::util::getPiePath();
    const auto canonical_root = std::filesystem::canonical(path);

    bool print_tokens       = false;
    bool print_parsed       = false;
    bool print_help         = false;
    bool print_ins          = false;
    bool norun              = false;
    bool repl               = false;
    bool vm                 = false;
    bool command            = false;
    bool version            = false;


    std::string_view fname;

    try {
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
            else if (argv[1] == "-v"sv  or argv[1] == "--version"sv) version            = true;
            else if (not fname.empty()) pie::util::error<pie::except::UknownOption>("Unrecognized Option: "s + argv[1]);
            else fname = argv[1];
        }

        if (version)
            return std::println("Pie v{}", VERSION), 0;

        if (command)
            return pie::cli::run(std::string{fname}, print_tokens, print_parsed, norun), 0;

        if (print_help)
            return pie::cli::help(), 0;


        if (fname.empty() or repl)
            return pie::cli::REPL(std::move(canonical_root), print_tokens, print_parsed, norun), 0;


        pie::cli::runFile(
            std::filesystem::path(fname),
            print_tokens,
            print_parsed,
            norun,
            print_ins,
            vm
        );
    }
    catch(const std::exception& e) {
        std::println(std::cerr, "{}", pie::err::render(e));
        return 1;
    }
    catch (...) {
        std::println(std::cerr, "Unknown Exception!");
        return 2;
    }

    return 0;
}



int main(int argc, char *argv[]) {
    // web pie manually hooks onto pieMain above ^
    #if not WEB_PIE
        return pieMain(argc, argv);
    #endif
}
