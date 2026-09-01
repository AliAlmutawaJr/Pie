#include "utils.hxx"

#include <fstream>
#include <sstream>

#include "../Lex/Token.hxx"
#include "../Utils/Exceptions.hxx"


namespace pie {
namespace util {

[[noreturn]] void expected(const token::TokenKind exp, const token::Token& got, const std::source_location& location) {
    error<except::UnexpectedToken>(std::string{"Expected token "} + stringify(exp) + " and found " + stringify(got.kind) + ": " + got.text, location);
}

[[noreturn]] void expected(const token::TokenKind exp, const token::TokenKind got, const std::source_location& location) {
    error<except::UnexpectedToken>(std::string{"Expected token "} + stringify(exp) + " and found " + stringify(got), location);
}

[[noreturn]] void expected(const std::string& exp, const token::Token& got, const std::source_location& location) {
    error<except::UnexpectedToken>(std::string{"Expected '"} + exp + "' and found " + stringify(got.kind) + ": " + got.text, location);
}


std::filesystem::path getPiePath() {
    constexpr auto MAX_PATH_SIZE = 1024;
    char buffer[MAX_PATH_SIZE];


#if defined(__APPLE__) or defined(__MACH__)

    uint32_t size = sizeof(buffer);
    if (_NSGetExecutablePath(buffer, &size) == 0) {
        return std::filesystem::canonical(buffer).parent_path(); 
    }

#elif defined(_WIN32) or defined(_WIN64)

    DWORD length = GetModuleFileNameA(NULL, buffer, MAX_PATH_SIZE); 

    if (length > 0 && length < sizeof(buffer)) {
        return std::filesystem::path(buffer).parent_path();
    }

#elif defined(__linux__)

    ssize_t length = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);

    if (length != -1) {
        buffer[length] = '\0';
        return std::filesystem::path(buffer).parent_path();
    }

#elif WEB_PIE
#else
// `-path` not supported yet!
#error "Unsupported Platform. Couldn't find executable's path!\nPass it manually by providing -path \"path/to/Pie\""
#endif

    error();
}



[[nodiscard]] std::string readFile(const std::filesystem::path& fname, const std::source_location& location) {
    const std::ifstream fin{fname};

    if (not fin.is_open()) error("File \"" + fname.string() + "\" not found!", location);

    std::stringstream ss;
    ss << fin.rdbuf();

    return ss.str();
}


} // namespace util
} // namespace pie
