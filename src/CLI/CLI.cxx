#include "CLI.hxx"


inline namespace pie {
namespace cli {

    void help() {
        std::println("-t [--token]  :   print tokens");
        std::println("-a [--ast]    :   print parsed");
        std::println("-r [--run]    :   don't run program");
        std::println("-h [--help]   :   print this message");
        std::println("-c [--command]:   run script from command line");
    }


    void REPL(
        const std::filesystem::path canonical_root,
        const bool print_tokens,
        const bool print_parsed,
        const bool run
    ) {
        Parser parser{canonical_root};
        pie::analysis::LexicalScoping anal;
        interp::Visitor visitor{{}};

        for (;;) try {
            std::string line;
            std::print(">>> ");
            std::getline(std::cin, line);
            if (line.back() != ';') line += ';';


            Tokens v = lex::lex(std::move(line));
            if (print_tokens) std::println(std::clog, "{}", v);

            if (v.empty()) continue;

            parser.resetTokens(std::move(v));

            auto exprs = parser.parse();

            if (print_parsed)
                for(const auto& expr : exprs)
                    std::println(std::clog, "{};", expr->stringify());

            if(run and (print_parsed or print_tokens)) puts("Output:\n");


            for (auto& expr : exprs)
                std::visit(anal, expr->variant());


            if (run) {
                if (not exprs.empty()) {
                    value::Value value;
                    for (auto& expr : exprs) value = std::visit(visitor, std::move(expr)->variant());

                    std::println("{}", stringify(value));
                }
            }
        }
        catch(const std::exception &e) {
            std::clog << e.what() << std::endl;
        }
    }



    void runFile(
        const std::filesystem::path fname,
        const bool print_tokens,
        const bool print_parsed,
        const bool norun
    ) {
        auto src = util::readFile(fname.string());

        auto processed_src = std::move(src);

        Tokens v = lex::lex(std::move(processed_src));
        if (print_tokens) std::println(std::clog, "{}", v);

        if (v.empty()) return;

        Parser p{std::move(v), fname};

        auto exprs = p.parse();

        if (print_parsed)
            for(const auto& expr : exprs)
                std::println(std::clog, "{};", expr->stringify());

        if(not norun and (print_parsed or print_tokens)) puts("Output:\n");

        pie::analysis::LexicalScoping anal;
        for (auto& expr : exprs)
            std::visit(anal, expr->variant());

        if (not norun) {
            interp::Visitor visitor{std::move(anal).indeces};
            for (const auto& expr : exprs)
                std::visit(visitor, expr->variant());
        }
    }



    void run(
        std::string src,
        const bool print_tokens,
        const bool print_parsed,
        const bool norun
    ) {
        Tokens v = lex::lex(std::move(src));
        if (print_tokens) std::println(std::clog, "{}", v);

        if (v.empty()) return;

        Parser p{std::move(v)};

        auto exprs = p.parse();

        if (print_parsed)
            for(const auto& expr : exprs)
                std::println(std::clog, "{};", expr->stringify(0));

        if(not norun and (print_parsed or print_tokens)) puts("Output:\n");

        pie::analysis::LexicalScoping anal;
        for (auto& expr : exprs)
            std::visit(anal, expr->variant());

        if (not norun) {
            interp::Visitor visitor{std::move(anal).indeces};
            for (const auto& expr : exprs)
                std::visit(visitor, expr->variant());
        }
    }

} // namespace cli
} // namespace pie
