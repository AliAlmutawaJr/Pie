#pragma once

#include <initializer_list>


namespace pie {
inline namespace builtins {

inline constexpr auto NAMES = {
    "Any",
    "Int",
    "Double",
    "String",
    "Bool",
    "Syntax",
    "Type",

    "__builtin_rand_int",

    "__builtin_print",
    "__builtin_concat",

    "__builtin_create_class",
    "__builtin_parse",

    "__builtin_defer",

    "__builtin_print_env",
    "__builtin_panic",
    "__builtin_id",
    "__builtin_input_str",
    "__builtin_input_int",
    "__builtin_decltype",
    "__builtin_type",
    "__builtin_len",
    "__builtin_reset",
    "__builtin_eval",
    "__builtin_neg",
    "__builtin_abs",
    "__builtin_not",
    "__builtin_to_int",
    "__builtin_to_double",
    "__builtin_to_string",
    "__builtin_get",
    "__builtin_set",
    "__builtin_push",
    "__builtin_reverse",
    "__builtin_pop",
    "__builtin_pop_front",
    "__builtin_insert_at",
    "__builtin_remove_at",
    "__builtin_object_has",
    "__builtin_into_pack",
    "__builtin_add",
    "__builtin_sub",
    "__builtin_mul",
    "__builtin_div",
    "__builtin_mod",
    "__builtin_pow",
    "__builtin_gt",
    "__builtin_geq",
    "__builtin_eq",
    "__builtin_leq",
    "__builtin_lt",
    "__builtin_and",
    "__builtin_or",
    "__builtin_conditional",
    "__builtin_str_slice",
    "__builtin_str_split",

    // //* File IO
    "__builtin_open_file",
    "__builtin_is_file_open",
    "__builtin_close_file",
    "__builtin_write_file",
    "__builtin_read_file",
    "__builtin_read_line",
    "__builtin_read_word",

    //* FFI shit
    "__builtin_dlopen",
    "__builtin_dlsym",
    "__builtin_ffi_call",
    "__builtin_ffi_type_void",
    "__builtin_ffi_type_int",
    "__builtin_ffi_type_float",
    "__builtin_ffi_type_double",
    "__builtin_ffi_type_uint8",
    "__builtin_ffi_type_sint8",
    "__builtin_ffi_type_uint16",
    "__builtin_ffi_type_sint16",
    "__builtin_ffi_type_uint32",
    "__builtin_ffi_type_sint32",
    "__builtin_ffi_type_uint64",
    "__builtin_ffi_type_sint64",
    "__builtin_ffi_type_struct",
    "__builtin_ffi_type_pointer",
    "__builtin_ffi_type_cstring",
    "__builtin_ffi_type_complex",

    "__builtin_ptr_to_string",
};

} // namespace builtins
} // namespace pie
