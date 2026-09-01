#include "Preprocessor.hxx"


namespace pie {

[[nodiscard]] std::string readFile2(const std::string& fname) {
    const std::ifstream fin{fname};

    if (not fin.is_open()) {
        std::println(std::cerr, "{}", "File \"" + fname + " \"not found!");
        throw std::runtime_error{"file '" + fname + "' not found!"};
    }

    std::stringstream ss;
    ss << fin.rdbuf();

    return ss.str();
}


bool findAndRemoveImport(std::string& src, size_t& index) {
    using std::operator""sv;

    index = src.find("import");

    if (index == std::string::npos) return false;

    constexpr size_t import_length = "import"sv.length();

    src.erase(index, import_length);

    return true;
}

size_t findSpace(const std::string& src, size_t ind) {
    while (++ind < src.length() and not std::isspace(static_cast<unsigned char>(src[ind])) and src[ind] != ';');

    return ind;
}


void removeBlockComments(std::string& s) {
    for (size_t ind = s.find(".::"); ind != std::string::npos; ind = s.find(".::")) {
        const size_t end_ind = s.find("::.", ind);
        s.erase(ind, end_ind - ind + 3);
    }
}


void removeLineComments(std::string& s) {
    for (size_t ind = s.find(".:"); ind != std::string::npos; ind = s.find(".:")) {
        const size_t end_ind = s.find("\n", ind);
        s.erase(ind, end_ind - ind + 1);
    }
}


std::string removeComments(std::string src) {
    removeBlockComments(src);
    removeLineComments(src);

    return src;
}


} // namespace pie
