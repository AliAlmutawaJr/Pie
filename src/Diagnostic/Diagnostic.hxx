#pragma once

#include <string>

#include "../Lex/Token.hxx"
#include "../Diagnostic/Sources.hxx"
#include "../Diagnostic/Exceptions.hxx"


namespace pie::err {

constexpr auto RED    = "\033[31m";
// constexpr auto CYAN = "\033[31m";
constexpr auto BOLD   = "\033[1m";
constexpr auto RESET  = "\033[0m";
constexpr auto PURPLE = "\033[35m";


inline std::string renderSpan(const util::SourceSpan& span) {

    const auto& [start, end] = span.span;

    std::string out = std::format("{} --> {}{}:{}:{}\n", PURPLE, RESET, span.file, start.line, start.column);

    const auto line = util::fileAtLine(std::string{span.file}, start.line);
    if (not line) return out;

    const auto number = std::to_string(start.line);
    const std::string gutter(number.size(), ' ');

    // columns are 1-based and the end is inclusive.
    // a span that continues onto later lines is underlined to the end of its first line.
    const size_t first = std::max<size_t>(start.column, 1);
    size_t last = end.line == start.line ? end.column : line->size();
    last = std::clamp(last, first, std::max(first, line->size()));

    // copy tabs from the source line so the carets line up under them
    std::string marker;
    for (size_t i = 1; i < first; ++i)
        marker += (i <= line->size() and (*line)[i - 1] == '\t') ? '\t' : ' ';
    marker.append(last - first + 1, '^');

    out += std::format("{}{} |{}\n", PURPLE, gutter, RESET);
    out += std::format("{}{} |{} {}\n", PURPLE, number, RESET, *line);
    out += std::format("{}{} |{} {}{}{}\n", PURPLE, gutter, RESET, RED, marker, RESET);

    return out;
}


inline std::string render(const std::exception& e) {
    std::string out = std::format("{}{}error:{}{} {}{}\n", BOLD, RED, RESET, BOLD, e.what(), RESET);

    if (const auto located = dynamic_cast<const except::LocatedError*>(&e)) {
        if (located->span.span.first.line != 0)
            out += renderSpan(located->span);


        // for (const auto& note : located->notes) {
        //     out += std::format("{}note:{} {}\n", BOLD, RESET, note.msg);

        //     if (note.span and hasLocation(*note.span))
        //         out += renderSpan(*note.span);
        // }
    }

    while (not out.empty() and out.back() == '\n') out.pop_back();
    return out;
}

} // namespace pie::err
