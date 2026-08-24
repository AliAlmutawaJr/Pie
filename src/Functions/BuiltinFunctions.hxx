#pragma once

#include <fstream>
#include <iterator>
#include <cstdio>
#include <random>
#include <cmath>

#include <dlfcn.h>

#include <ranges>
#include <stdx/tuple.hpp>
#include <ffi.h>
#include <type_traits>

#include "../Utils/ConstexprLookup.hxx"
#include "../Utils/Exceptions.hxx"
#include "../Utils/utils.hxx"


// for libffi
#ifndef FFI_TYPE_CSTRING 
#define FFI_TYPE_CSTRING 100
#endif


inline namespace pie {
inline namespace funcs {
inline namespace builtins {


//* ============================ FUNCTIONS ============================
static constexpr auto functions = stdx::make_indexed_tuple<KeyFor>(
    //* NULLARY FUNCTIONS
    MapEntry<
        S<"input_str">,
        Func<
            decltype([](const auto&) {
                std::string out;
                std::getline(std::cin, out);
                return out;
            }),
            void
        >
    >{},
    MapEntry<
        S<"input_int">,
        Func<
            decltype([](const auto&) {
                std::string out;
                std::getline(std::cin, out);
                if (not std::ranges::all_of(out, isdigit)) util::error("'__builtin_input_int' recieved a non-int \"" + out + "\"!");

                return std::stoll(out);
            }),
            void
        >
    >{},

    //* UNARY FUNCTIONS

    MapEntry<
        S<"len">,
        Func<
            decltype([](const auto& x, const auto&) {
                if constexpr (std::is_same_v<std::remove_cvref_t<decltype(x)>, std::string>)
                        return static_cast<BigInt>(x.length());

                else if constexpr (std::is_same_v<std::remove_cvref_t<decltype(x)>, value::Pack>)
                    return static_cast<BigInt>(x->values.size());

                else if constexpr (std::is_same_v<std::remove_cvref_t<decltype(x)>, value::List>)
                    return static_cast<BigInt>(x.elts->values.size());

                else // map value
                    return static_cast<BigInt>(x.items->map.size());
            }),
            TypeList<std::string>,
            TypeList<value::Pack>,
            TypeList<value::List>,
            TypeList<value::Map>
        >
    >{},

    MapEntry<
        S<"type">,
        Func<
            decltype([](const auto& x, const auto& that) {
                return that->typeOf(x);
            }),
            TypeList<Any>
        >
    >{},

    MapEntry<
        S<"neg">,
        Func<
            decltype([](const auto& x, const auto&) { return -x; }),
            TypeList<BigInt>,
            TypeList<double>
        >
    >{},

    MapEntry<
        S<"abs">,
        Func<
            decltype([](const auto& x, const auto&) { return std::abs(x); }),
            TypeList<BigInt>,
            TypeList<double>
        >
    >{},

    MapEntry<
        S<"not">,
        Func<
            decltype([](const auto& x, const auto&) { return not x; }),
            TypeList<bool>
        >
    >{},

    MapEntry<
        S<"to_int">,
        Func<
            decltype([](const auto& x, const auto&) -> BigInt {
                if constexpr (std::is_same_v<std::remove_cvref_t<decltype(x)>, std::string>)
                    return std::stoll(x);

                // if constexpr (
                //     std::is_same_v<std::remove_cvref_t<decltype(x)>, BigInt> or
                //     std::is_same_v<std::remove_cvref_t<decltype(x)>, double> or
                //     std::is_same_v<std::remove_cvref_t<decltype(x)>, bool>
                // )
                else return x;
            }),
            TypeList<BigInt>,
            TypeList<double>,
            TypeList<bool>,
            TypeList<std::string>
        >
    >{},

    MapEntry<
        S<"to_double">,
        Func<
            decltype([](const auto& x, const auto&) -> double {
                if constexpr (std::is_same_v<std::remove_cvref_t<decltype(x)>, std::string>)
                    return std::stod(x);

                // if constexpr (
                //     std::is_same_v<std::remove_cvref_t<decltype(x)>, BigInt> or
                //     std::is_same_v<std::remove_cvref_t<decltype(x)>, double> or
                //     std::is_same_v<std::remove_cvref_t<decltype(x)>, bool>
                // )
                else return x;
            }),
            TypeList<BigInt>,
            TypeList<double>,
            TypeList<bool>,
            TypeList<std::string>
        >
    >{},

    MapEntry<
        S<"to_string">,
        Func<
            decltype([](const auto& x, const auto&) { return stringify(x); }),
            TypeList<Any>
        >
    >{},

    MapEntry<
        S<"str_split">,
        Func<
            decltype([](const auto& input, const auto& delim, const auto&) {
                return value::makeList(
                    input
                    | std::views::split(delim)
                    | std::views::transform([] (const auto& str) -> value::Value {
                        return std::string{std::string_view{str}};
                    })
                    | std::ranges::to<std::vector<value::Value>>()
                );
            }),
            TypeList<std::string, std::string>
        >
    >{},


    MapEntry<
        S<"open_file">,
        Func<
            decltype([](const auto& fname, const auto&) {
                auto file = new std::fstream{
                    fname,
                    std::ios::in | std::ios::out
                };

                // if (not file->is_open()) util::error("Couldn't open file: " + fname);

                return reinterpret_cast<BigInt>(file);
            }),
            TypeList<std::string, std::string>
        >
    >{},


    MapEntry<
        S<"is_file_open">,
        Func<
            decltype([](const auto& stream, const auto&) {
                auto file = reinterpret_cast<std::fstream*>(stream);

                return file->is_open();
            }),
            TypeList<BigInt>
        >
    >{},

    MapEntry<
        S<"close_file">,
        Func<
            decltype([](const auto& stream, const auto&) {
                auto file = reinterpret_cast<std::fstream*>(stream);
                file->close();

                delete file;
                return stream;
            }),
            TypeList<BigInt>
        >
    >{},

    MapEntry<
        S<"read_file">,
        Func<
            decltype([](const auto& stream, const auto&) -> value::Value {
                auto file = reinterpret_cast<std::fstream*>(stream);
                // std::stringstream ss;
                // ss << file->rdbuf();
                // return ss.str();

                std::string content{
                    std::istreambuf_iterator<char>{*file},
                    std::istreambuf_iterator<char>{}
                };

                return content;
            }),
            TypeList<BigInt>
        >
    >{},

    MapEntry<
        S<"read_line">,
        Func<
            decltype([](const auto& stream, const auto&) -> value::Value {
                auto file = reinterpret_cast<std::fstream*>(stream);

                std::string line;
                std::getline(*file, line);

                return line;
            }),
            TypeList<BigInt>
        >
    >{},

    MapEntry<
        S<"read_word">,
        Func<
            decltype([](const auto& stream, const auto&) -> value::Value {
                auto file = reinterpret_cast<std::fstream*>(stream);

                std::string word;
                *file >> word;

                return word;
            }),
            TypeList<BigInt>
        >
    >{},

    MapEntry<
        S<"read_char">,
        Func<
            decltype([](const auto& stream, const auto&) -> value::Value {
                auto file = reinterpret_cast<std::fstream*>(stream);

                char c;
                if(not file->get(c)) util::error("Tried to read EOF");

                return std::string{c};
            }),
            TypeList<BigInt>
        >
    >{},



    MapEntry<
        S<"eval">,
        Func<
            decltype([](const auto& x, const auto& that) {
                return std::visit(*that, x).value;
            }),
            TypeList<expr::Node>
        >
    >{},

    MapEntry<
        S<"pop">,
        Func<
            decltype([](const auto& cont, const auto&) -> value::Value {
                if (cont.elts->values.empty()) util::error("Cannot `pop` from an empty list!");

                const auto back = cont.elts->values.back();
                cont.elts->values.pop_back();
                return back;
            }),
            TypeList<value::List>
        >
    >{},

    MapEntry<
        S<"pop_front">,
        Func<
            decltype([](const auto& cont, const auto&) -> value::Value {
                if (cont.elts->values.empty()) util::error("Cannot `pop_front` from an empty list!");

                const auto front = cont.elts->values.front();
                cont.elts->values.erase(cont.elts->values.begin());
                return front;
            }),
            TypeList<value::List>
        >
    >{},

    MapEntry<
        S<"remove_at">,
        Func<
            decltype([](auto& cont, const auto& ind, const auto&) -> value::Value {
                using T = std::remove_cvref_t<decltype(cont)>;

                if constexpr (std::is_same_v<T, value::List>) {
                    if (size_t(ind) >= cont.elts->values.size()) {
                        if (cont.elts->values.empty()) util::error("Cannot `remove_at` from an empty list!");
                        else util::error("Index out of range inside call to `remove_at`!");
                    }

                    const auto value = cont.elts->values[ind];
                    cont.elts->values.erase(std::next(cont.elts->values.begin(), ind));
                    return value;
                }
                else if constexpr (std::is_same_v<T, value::Map>) {
                    if (not cont.items->map.contains(ind))
                        util::error("Tried to remove element that doesn't exist from map!");


                    const auto item = cont.items->map.at(ind);
                    cont.items->map.erase(ind);
                    return item;
                }
                else { // has to be a std::string
                    if (size_t(ind) >= cont.size()) {
                        if (cont.empty()) util::error("Cannot `remove_at` from an empty string!");
                        else util::error("Index out of range inside call to `remove_at`!");
                    }

                    const std::string elt = {cont[ind]};
                    cont.erase(ind, 1);
                    return elt;
                }

            }),
            TypeList<value::List, BigInt>,
            TypeList<value::Map , Any>,
            TypeList<std::string, BigInt>
        >
    >{},


    //* BINARY FUNCTIONS
    MapEntry<
        S<"get">,
        Func<
            decltype([](const auto& a, const auto& ind, const auto&) -> value::Value {
                using T = std::remove_cvref_t<decltype(a)>;

                if constexpr (std::is_same_v<T, value::List>) {
                    if (ind < 0 or size_t(ind) >= a.elts->values.size())
                        util::error("Accessing list '" + stringify(a) + "' at index '" + std::to_string(ind) + "' which is out of bounds!");

                    return a.elts->values[ind]; 
                }
                else if constexpr (std::is_same_v<T, value::Map>) {
                    if (not a.items->map.contains(ind))
                        util::error("Accessing Map '" + stringify(a) + "' at key `" + stringify(ind) + "` which doesn't exist!");

                    return a.items->map.at(ind);
                }
                else if constexpr (std::is_same_v<T, value::Object>) {
                    auto iter = std::ranges::find_if(
                        a.second->members,
                        [&ind] (const auto& member) { return get<expr::Name>(member).name == ind; }
                    );
                    if (iter == a.second->members.cend())
                        util::error("Accessing Object `" + stringify(a) + "` with member name `" + ind + "` which doesn't exist!");

                    return *get<value::ValuePtr>(*iter);
                }
                else if constexpr (std::is_same_v<T, value::Pack>) {
                    if (ind < 0 or size_t(ind) >= a->values.size())
                        util::error("Accessing list '" + stringify(a) + "' at index '" + std::to_string(ind) + "' which is out of bounds!");

                    return a->values[ind]; 
                }
                else { // if constexpr (std::is_same_v<std::remove_cvref_t<decltype(a)>, std::string>) {
                    if (ind < 0 or size_t(ind) >= a.length())
                        util::error("Accessing string '" + a + "' at index '" + std::to_string(ind) + "' which is out of bounds!");
                    return std::string{a[ind]};
                }
            }),
            TypeList<value::List, BigInt>,
            TypeList<value::Map, Any>,
            TypeList<value::Object, std::string>,
            TypeList<value::Pack, BigInt>,
            TypeList<std::string, BigInt>
        >
    >{},

    MapEntry<
        S<"set">,
        Func<
            decltype([](const auto& cont, const auto& at, const auto& elt, const auto& that) -> value::Value {
                using T = std::remove_cvref_t<decltype(cont)>;

                if constexpr (std::is_same_v<T, value::List>) {
                    const auto type = that->typeOf(cont);
                    const auto& list_type = dynamic_cast<const type::ListType&>(*type);

                    if (at < 0 or size_t(at) >= cont.elts->values.size())
                        util::error("Accessing list '" + stringify(cont) + "' at index '" + std::to_string(at) + "' which is out of bounds!");

                    return cont.elts->values[at] = that->typeCheck(elt, list_type.type);
                }
                else if constexpr (std::is_same_v<T, value::Map>) {
                    const auto type = that->typeOf(cont);
                    const auto& map_type = dynamic_cast<const type::MapType&>(*type);

                    auto key = that->typeCheck(at, map_type.key_type);
                    return cont.items->map[key] = that->typeCheck(elt, map_type.val_type);
                }
                else if constexpr (std::is_same_v<T, value::Object>) {
                    auto iter = std::ranges::find_if(
                        cont.second->members,
                        [&at] (const auto& member) { return get<expr::Name>(member).name == at; }
                    );
                    if (iter == cont.second->members.cend())
                        util::error("Accessing Object `" + stringify(cont) + "` with member name `" + at + "` which doesn't exist!");


                    return *get<value::ValuePtr>(*iter) = that->typeCheck(elt, get<type::TypePtr>(*iter));
                }
            }),
            TypeList<value::List, BigInt, Any>,
            TypeList<value::Map, Any, Any>,
            TypeList<value::Object, std::string, Any>
            // TypeList<std::string, BigInt>
        >
    >{},

    MapEntry<
        S<"push">,
        Func<
            decltype([](const auto& cont, const auto& elt, const auto& that) -> value::Value {
                // added type checking step since
                // the interpreter only type checks on assignments
                const auto cont_type = that->typeOf(cont);
                const auto& list_type = type::isList(cont_type);
                if (not list_type)
                    util::error("push called on non-list type: " + cont_type->text());

                cont.elts->values.push_back(that->typeCheck(elt, list_type->type));

                return elt;
            }),
            TypeList<value::List, Any>
        >
    >{},

    MapEntry<
        S<"reverse">,
        Func<
            decltype([](auto& cont, const auto&) -> value::Value {
                using T = std::remove_cvref_t<decltype(cont)>;

                if constexpr (std::is_same_v<T, value::List>) {
                    cont.elts->values =
                        cont.elts->values | std::views::reverse | std::ranges::to<std::vector>();
                }
                else if constexpr (std::is_same_v<T, value::Pack>) {
                    cont->values = cont->values | std::views::reverse | std::ranges::to<std::vector>();
                }
                else { // std::string
                    cont = cont | std::views::reverse | std::ranges::to<std::string>();
                }

                return cont;
            }),
            TypeList<value::List>,
            TypeList<value::Pack>,
            TypeList<std::string>
        >
    >{},


    MapEntry<
        S<"add">,
        Func<
            decltype([](const auto& a, const auto& b, const auto&) { return a + b; }),
            TypeList<BigInt, BigInt>,
            TypeList<BigInt, double>,
            TypeList<double, BigInt>,
            TypeList<double, double>
        >
    >{},

    MapEntry<
        S<"sub">,
        Func<
            decltype([](const auto& a, const auto& b, const auto&) { return a - b; }),
            TypeList<BigInt, BigInt>,
            TypeList<BigInt, double>,
            TypeList<double, BigInt>,
            TypeList<double, double>
        >
    >{},

    MapEntry<
        S<"mul">,
        Func<
            decltype([](const auto& a, const auto& b, const auto&) { return a * b; }),
            TypeList<BigInt, BigInt>,
            TypeList<BigInt, double>,
            TypeList<double, BigInt>,
            TypeList<double, double>
        >
    >{},

    MapEntry<
        S<"div">,
        Func<
            decltype([](const auto& a, const auto& b, const auto&) {
                if (b == 0) util::error("Division by 0(!)");

                return a / b;
            }),
            TypeList<BigInt, BigInt>,
            TypeList<BigInt, double>,
            TypeList<double, BigInt>,
            TypeList<double, double>
        >
    >{},

    MapEntry<
        S<"mod">,
        Func<
            decltype([](const auto& a, const auto& b, const auto&) { return a % b; }),
            TypeList<BigInt, BigInt>
        >
    >{},

    MapEntry<
        S<"bit_and">,
        Func<
            decltype([](const auto& a, const auto& b, const auto&) { return a & b; }),
            TypeList<BigInt, BigInt>
        >
    >{},

    MapEntry<
        S<"bit_or">,
        Func<
            decltype([](const auto& a, const auto& b, const auto&) { return a | b; }),
            TypeList<BigInt, BigInt>
        >
    >{},

    MapEntry<
        S<"xor">,
        Func<
            decltype([](const auto& a, const auto& b, const auto&) { return a ^ b; }),
            TypeList<BigInt, BigInt>
        >
    >{},

    MapEntry<
        S<"rand_int">,
        Func<
            decltype([](const auto& min, const auto& max, const auto&) {
                static std::random_device rd{};
                static std::mt19937 gen{rd()};

                std::uniform_int_distribution<BigInt> dist{min, max};

                return dist(gen);

            }),
            TypeList<BigInt, BigInt>
        >
    >{},

    MapEntry<
        S<"pow">,
        Func<
            decltype(
                [](const auto& a, const auto& b, const auto&) -> std::common_type_t<decltype(a), decltype(b)> { return std::pow(a, b); }
            ),
            TypeList<BigInt, BigInt>,
            TypeList<BigInt, double>,
            TypeList<double, BigInt>,
            TypeList<double, double>
        >
    >{},

    MapEntry<
        S<"gt">,
        Func<
            decltype([](const auto& a, const auto& b, const auto&) { return a > b; }),
            TypeList<BigInt, BigInt>,
            TypeList<BigInt, double>,
            TypeList<double, BigInt>,
            TypeList<double, double>
        >
    >{},

    MapEntry<
        S<"geq">,
        Func<
            decltype([](const auto& a, const auto& b, const auto&) { return a >= b; }),
            TypeList<BigInt, BigInt>,
            TypeList<BigInt, double>,
            TypeList<double, BigInt>,
            TypeList<double, double>
        >
    >{},

    MapEntry<
        S<"eq">,
        Func<
            decltype([](auto a, auto b, const auto&) { return a == b; }),
            TypeList<Any, Any>
        >
    >{},

    MapEntry<
        S<"leq">,
        Func<
            decltype([](const auto& a, const auto& b, const auto&) { return a <= b; }),
            TypeList<BigInt, BigInt>,
            TypeList<BigInt, double>,
            TypeList<double, BigInt>,
            TypeList<double, double>
        >
    >{},

    MapEntry<
        S<"lt">,
        Func<
            decltype([](const auto& a, const auto& b, const auto&) { return a < b; }),
            TypeList<BigInt, BigInt>,
            TypeList<BigInt, double>,
            TypeList<double, BigInt>,
            TypeList<double, double>
        >
    >{}

#if not WEB_PIE
,

    //* FFI 
    MapEntry<
        S<"dlopen">,
        Func<
            decltype([](const auto& path, const auto&) {
                auto dll = dlopen(path.data(), RTLD_NOW);
                if (not dll) util::error<except::OpeningDyLib>(dlerror());

                return reinterpret_cast<BigInt>(dll);
            }),
            TypeList<std::string>
        >
    >{},

    MapEntry<
        S<"dlsym">,
        Func<
            decltype([](const auto& dll, const auto& func_name, const auto&) {
                auto handle = reinterpret_cast<void*>(dll);

                auto sym = dlsym(handle, func_name.data());
                if (not sym) util::error<except::DyLibSymbolLookup>(dlerror());

                return reinterpret_cast<BigInt>(sym);
            }),
            TypeList<BigInt, std::string>
        >
    >{},

    MapEntry<
        S<"ffi_type_void">,
        Func<
            decltype([](const auto&) { return FFI_TYPE_VOID; }),
            void
        >
    >{},

    MapEntry<
        S<"ffi_type_int">,
        Func<
            decltype([](const auto&) { return FFI_TYPE_INT; }),
            void
        >
    >{},

    MapEntry<
        S<"ffi_type_float">,
        Func<
            decltype([](const auto&) { return FFI_TYPE_FLOAT; }),
            void
        >
    >{},

    MapEntry<
        S<"ffi_type_double">,
        Func<
            decltype([](const auto&) { return FFI_TYPE_DOUBLE; }),
            void
        >
    >{},

    MapEntry<
        S<"ffi_type_long_double">,
        Func<
            decltype([](const auto&) { return FFI_TYPE_LONGDOUBLE; }),
            void
        >
    >{},

    MapEntry<
        S<"ffi_type_uint8">,
        Func<
            decltype([](const auto&) { return FFI_TYPE_UINT8; }),
            void
        >
    >{},

    MapEntry<
        S<"ffi_type_sint8">,
        Func<
            decltype([](const auto&) { return FFI_TYPE_SINT8; }),
            void
        >
    >{},

    MapEntry<
        S<"ffi_type_uint16">,
        Func<
            decltype([](const auto&) { return FFI_TYPE_UINT16; }),
            void
        >
    >{},

    MapEntry<
        S<"ffi_type_sint16">,
        Func<
            decltype([](const auto&) { return FFI_TYPE_SINT16; }),
            void
        >
    >{},

    MapEntry<
        S<"ffi_type_uint32">,
        Func<
            decltype([](const auto&) { return FFI_TYPE_UINT32; }),
            void
        >
    >{},

    MapEntry<
        S<"ffi_type_sint32">,
        Func<
            decltype([](const auto&) { return FFI_TYPE_SINT32; }),
            void
        >
    >{},

    MapEntry<
        S<"ffi_type_uint64">,
        Func<
            decltype([](const auto&) { return FFI_TYPE_UINT64; }),
            void
        >
    >{},

    MapEntry<
        S<"ffi_type_sint64">,
        Func<
            decltype([](const auto&) { return FFI_TYPE_SINT64; }),
            void
        >
    >{},

    MapEntry<
        S<"ffi_type_struct">,
        Func<
            decltype([](const auto&) { return FFI_TYPE_STRUCT; }),
            void
        >
    >{},

    MapEntry<
        S<"ffi_type_pointer">,
        Func<
            decltype([](const auto&) { return FFI_TYPE_POINTER; }),
            void
        >
    >{},

    MapEntry<
        S<"ffi_type_cstring">,
        Func<
            // any arbitrary sentinel value that is different from any FFI_TYPE_*
            decltype([](const auto&) { return FFI_TYPE_CSTRING; }),
            void
        >
    >{},

    MapEntry<
        S<"ffi_type_complex">,
        Func<
            decltype([](const auto&) { return FFI_TYPE_COMPLEX; }),
            void
        >
    >{},

    // Pointers are just addresses jammed into a BigInt in Pie. A C function
    // that hands back a `const char*` (e.g. raylib's TextFormat) returns
    // that raw address; this reads the NUL-terminated string living there.
    MapEntry<
        S<"ptr_to_string">,
        Func<
            decltype([](const auto& ptr, const auto&) -> std::string {
                if (ptr == 0) util::error("`ptr_to_string` received a null pointer!");
                return std::string{reinterpret_cast<const char*>(ptr)};
            }),
            TypeList<BigInt>
        >
    >{}

#endif

);


} // namespace builtins
} // namespace funcs
} // namespace pie