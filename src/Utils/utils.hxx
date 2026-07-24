#pragma once

#include <iostream>
#include <filesystem>
#include <string>
#include <source_location>
#include <format>
#include <print>
#include <concepts>
#include <stdexcept>

#include "../Lex/Token.hxx"



#if defined(__APPLE__) or defined(__MACH__)
    #include <mach-o/dyld.h>
#elif defined(_WIN32) or defined(_WIN64)
    #include <windows.h>
#elif defined(__linux__)
    #include <unistd.h>
#else
    #error "unkown operating system!"
#endif


inline namespace pie {
namespace util {


template <typename Except = std::runtime_error, bool print_loc = true>
[[noreturn]] inline void error(
    std::string_view msg = "",
    [[maybe_unused]] const std::source_location& location = std::source_location::current()
)
{
    // if the err msg is empty, print the location no matter what
    if (msg.empty()) {
        std::print(std::cerr, "\033[1m{}:{}:{}: \033[31merror:\033[0m ", location.file_name(), location.line(), location.column());
        throw Except{std::string{"[no diagnostic]. If you see this, please file a bug report!"}};
    }


    #if not NO_ERR_LOC
    if constexpr (print_loc)
        std::print(std::cerr, "\033[1m{}:{}:{}: \033[31merror:\033[0m ", location.file_name(), location.line(), location.column());
    #endif


    throw Except{std::string{msg}};
}


[[noreturn]] inline void error(const std::source_location& location)
{
    error("[no diagnostic]. If you see this, please file a bug report!", location);
}


[[noreturn]] void expected(const TokenKind exp, const Token& got, const std::source_location& location = std::source_location::current());

[[noreturn]] void expected(const TokenKind exp, const TokenKind got, const std::source_location& location = std::source_location::current());

[[noreturn]] void expected(const std::string& exp, const Token& got, const std::source_location& location = std::source_location::current());


std::filesystem::path getPiePath();



template <typename F>
struct Deferred {
    F f;

    Deferred(std::invocable auto func) : f{std::move(func)} {};
    ~Deferred() { f(); }
};

template <typename F>
Deferred(F) -> Deferred<F>;




[[nodiscard]] std::string readFile(const std::filesystem::path& fname, const std::source_location& location = std::source_location::current());


} // namespace util
} // namespace pie