#pragma once

#include <iostream>
#include <string>
#include <cstdio>
#include <unistd.h>


#include "../src/Lex/Lexer.hxx"
#include "../src/Parser/Parser.hxx"
#include "../src/Analysis/LexicalAnalysis.hxx"
#include "../src/Interp/Interpreter.hxx"


inline namespace pie {
namespace test {

struct Capture {
    int oldfd{-1};
    FILE* tmp{nullptr};
    std::string s;
    bool stopped{};

    Capture() {
        tmp = std::tmpfile();

        oldfd = dup(STDOUT_FILENO);

        dup2(fileno(tmp), STDOUT_FILENO);
    }

    std::string stop() {
        if (stopped) return s;
        std::cout.flush(); std::fflush(stdout);

        dup2(oldfd, STDOUT_FILENO); close(oldfd);

        std::rewind(tmp);

        char buf[4096]; size_t n;
        while ((n = std::fread(buf, 1, sizeof buf, tmp)) > 0) s.append(buf, n);

        std::fclose(tmp);
        stopped = true;

        clean();
        return s;
    }

    void clean () {
        s.erase(std::remove(s.begin(), s.end(), '\r'), s.end());
        s.erase(std::remove(s.begin(), s.end(), '\0'), s.end());

        for (size_t i{}; i < s.size(); )
            if (s[i] == 0x1B and i + 1 < s.size() and s[i+1] == '[') {

                size_t j = i + 2;
                for (; j < s.size() and (std::isdigit(s[j]) or s[j] == ';'); ++j);

                if (j < s.size()) s.erase(i, j+1-i);
                else break;
            }
            else ++i;



        for (; !s.empty() and (s.back() == '\n' or s.back() == ' '); s.pop_back());
    };

    ~Capture(){ if (not stopped) stop(); }

    const std::string& text() const { return s; }
};



[[nodiscard]] inline std::string run(const char* src) {

    token::Tokens v = lex::lex(src);

    if (v.empty()) return "";

    Parser p{std::move(v)};

    auto exprs = p.parse();


    pie::analysis::LexicalAnalysis anal;
    for (const auto& expr : exprs)
        std::visit(anal, expr->variant());

    Capture c{};

    interp::Visitor visitor{std::move(anal).indeces};
    for (const auto& expr : exprs)
        std::visit(visitor, expr->variant());

    return c.stop();
}



} // namespace test
} // namespace pie

