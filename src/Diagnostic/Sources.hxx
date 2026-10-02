#pragma once


#include <unordered_map>
#include <string>
#include <string_view>
#include <optional>

namespace pie::util {

inline std::unordered_map<std::string, std::string> sources = {{"", "<unnamed>"}};

[[nodiscard]] inline std::optional<std::string_view> fileAtLine(const std::string& fname, const size_t line) {
    if (not line) return {};

    const std::string_view content = sources.at(fname);
    if (content.empty()) return {};


    size_t begin{};
    for (size_t l = 1; l < line; ++l) {
        // find end of the current line
        begin = content.find('\n', begin);

        // didn't find
        if (begin == std::string_view::npos) return {};

        ++begin; // move to next line
    }

    if (begin > content.size()) return {};

    const size_t end = content.find('\n', begin);

    auto result = content.substr(begin, end == std::string_view::npos ? std::string_view::npos : end - begin);

    // windows is shit
    if (not result.empty() and result.back() == '\r') result.remove_suffix(1);

    return result;
}

} // namespace pie