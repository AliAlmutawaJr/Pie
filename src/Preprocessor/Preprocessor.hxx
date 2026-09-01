#pragma once


#include <print>
#include <cctype>
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <unordered_set>
#include <filesystem>
#include <stdexcept>

namespace pie {

[[nodiscard]] std::string readFile2(const std::string& fname);

bool findAndRemoveImport(std::string& src, size_t& index);

size_t findSpace(const std::string& src, size_t ind);

void removeBlockComments(std::string& s);

void removeLineComments(std::string& s);

std::string removeComments(std::string src);

template <bool = false>
std::string preprocess(std::string src, const std::filesystem::path& root = ".");

template <bool REPL = false>
std::string process(std::string src, const std::filesystem::path& root) {
    static std::unordered_set<std::string> cache;

    auto canonical = std::filesystem::canonical(root);

    if constexpr (not REPL) {
        if (cache.contains(canonical.string())) return "";
        cache.insert(canonical.string());
    }


    auto mainfile = root;
    mainfile.remove_filename();

    size_t index;

    while (findAndRemoveImport(src, index)) {
        auto filename = mainfile;

        const auto end_index = findSpace(src, index);

        ++index;
        auto name = src.substr(index, end_index - index);
        src.erase(index, end_index - index + (src[end_index] == ';')); // remove filename
        // std::ranges::replace(name, ':', '/');

        filename /= name;

        filename.replace_extension(".pie");

        auto path = std::filesystem::canonical(filename);
        // auto path = std::filesystem::absolute(filename);

        auto module = readFile2(path.string());

        module = preprocess(std::move(module), std::move(path));

        src.insert(index, std::move(module));
    }

    return src;
}


template <bool REPL>
std::string preprocess(std::string src, const std::filesystem::path& root) {
    return process<REPL>(removeComments(std::move(src)), root);
}


} // namespace pie