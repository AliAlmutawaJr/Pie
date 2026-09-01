#pragma once

#include <print>
#include <string>
#include <vector>
#include <unordered_map>
#include <initializer_list>


#include "../Value/Value.hxx"
#include "ByteCode.hxx"


namespace pie {
namespace vm {

using ID = ssize_t;
struct Frame {
    size_t bp, ret_address;

    std::unordered_map<ID, value::Value> env;
};

class Machine {

public:
    const std::unordered_map<ssize_t, const value::Value> constants;
    std::unordered_map<ssize_t, value::Value> globals;

    std::vector<value::Value> stack;

    std::vector<Frame> frames;
    std::unordered_map<std::string, size_t> labels;
    Chunk instructions;

    // registers
    size_t ip{};


    Machine(
        Chunk chunk,
        std::unordered_map<ssize_t, const value::Value> consts,
        const size_t index = 0
    )
    :
    constants{std::move(consts)},
    frames(1),
    instructions{std::move(chunk)}
    {
        initGlobals(index);
    }


    auto& currentEnv() { return frames.back().env; }

    ID getNextByte() {
        return static_cast<ID>(instructions[++ip]);
    }

    value::Value pop() {
        auto value = stack.back();
        stack.pop_back();
        return value;
    }

    void push(value::Value value) {
        stack.push_back(std::move(value));
    }

    void execute() {
        using enum Code;

        for (; ; ++ip) {
            const auto instruction = instructions[ip];

            switch (instruction) {
                case POP: pop();
                    break;

                case LOAD_CONST: {
                    const auto input = getNextByte();
                    const auto& value = constants.at(input);
                    push(value);
                    break;
                } break;

                case LOAD: {
                    const auto input = getNextByte();
                    const auto& value = currentEnv().at(input);
                    push(value);
                    break;
                } break;

                case STORE: {
                    const auto input = getNextByte();
                    auto value = pop();
                    currentEnv()[input] = std::move(value);
                } break;


                case LOAD_ACC :
                case STORE_ACC:

                case LOAD_GLOBAL: {
                    const auto input = getNextByte();
                    const auto& value = globals.at(input);
                    push(value);
                    break;
                } break;


                case STORE_GLOBAL: {
                    const auto input = getNextByte();
                    auto value = pop();
                    globals[input] = std::move(value);
                    break;
                } break;


                case CALL: call(getNextByte());
                    break;


                case RET:


                case LT     :
                case LEQ    :
                case EQ     :
                case GEQ    :
                case GT     :
                case JUMP_IF:
                case JUMP   :



                case HALT:
                    return;
            }
        }
    }


    void call(const size_t arg_count) {
        std::vector<value::Value> args;
        for (size_t i{}; i < arg_count; ++i) {
            args.push_back(pop());
        }

        auto func = pop();

        if (isBuiltin(value::stringify(func))) {
            handleBuiltin(std::move(get<value::BuiltinFunction>(func)), std::move(args));
        }


    }


    void handleBuiltin(value::BuiltinFunction func, std::vector<value::Value> args) {
        if (func.func_name == "__builtin_print") return builtinPrint(std::move(args));


    }



    void builtinPrint(
        std::vector<value::Value> args
    ) {

        for(const auto& arg : args) {
            std::print("{} ", value::stringify(arg));
        }
        puts("");

        push(args.back());
    }


    bool isBuiltin(std::string_view name) {
        for (const auto g : builtins)
            if (g == name) return true;

        return false;
    }


    void initGlobals(size_t index = 0) {
        for (const auto& value : GS)
            globals[index++] = std::move(value); // this move probably does nothing since init_list are immovable from
    }



    inline static const std::initializer_list<value::Value> GS = {
        type::builtins::Any(),
        type::builtins::Int(),
        type::builtins::Double(),
        type::builtins::String(),
        type::builtins::Bool(),
        type::builtins::Syntax(),
        type::builtins::Type(),


        value::BuiltinFunction{"__builtin_rand_int"},

        value::BuiltinFunction{"__builtin_print"      },
        value::BuiltinFunction{"__builtin_concat"     },
        value::BuiltinFunction{"__builtin_print_env"  },
        value::BuiltinFunction{"__builtin_panic"      },
        value::BuiltinFunction{"__builtin_id"         },
        value::BuiltinFunction{"__builtin_input_str"  },
        value::BuiltinFunction{"__builtin_input_int"  },
        value::BuiltinFunction{"__builtin_decltype"   },
        value::BuiltinFunction{"__builtin_type"       },
        value::BuiltinFunction{"__builtin_len"        },
        value::BuiltinFunction{"__builtin_reset"      },
        value::BuiltinFunction{"__builtin_eval"       },
        value::BuiltinFunction{"__builtin_neg"        },
        value::BuiltinFunction{"__builtin_abs"        },
        value::BuiltinFunction{"__builtin_not"        },
        value::BuiltinFunction{"__builtin_to_int"     },
        value::BuiltinFunction{"__builtin_to_double"  },
        value::BuiltinFunction{"__builtin_to_string"  },
        value::BuiltinFunction{"__builtin_get"        },
        value::BuiltinFunction{"__builtin_push"       },
        value::BuiltinFunction{"__builtin_push"       },
        value::BuiltinFunction{"__builtin_pop"        },
        value::BuiltinFunction{"__builtin_pop_front"  },
        value::BuiltinFunction{"__builtin_add"        },
        value::BuiltinFunction{"__builtin_sub"        },
        value::BuiltinFunction{"__builtin_mul"        },
        value::BuiltinFunction{"__builtin_div"        },
        value::BuiltinFunction{"__builtin_mod"        },
        value::BuiltinFunction{"__builtin_pow"        },
        value::BuiltinFunction{"__builtin_gt"         },
        value::BuiltinFunction{"__builtin_geq"        },
        value::BuiltinFunction{"__builtin_eq"         },
        value::BuiltinFunction{"__builtin_leq"        },
        value::BuiltinFunction{"__builtin_lt"         },
        value::BuiltinFunction{"__builtin_and"        },
        value::BuiltinFunction{"__builtin_or"         },
        value::BuiltinFunction{"__builtin_set"        },
        value::BuiltinFunction{"__builtin_conditional"},
        value::BuiltinFunction{"__builtin_str_slice"  },
        value::BuiltinFunction{"__builtin_str_split"  },

        //* File IO
        value::BuiltinFunction{"__builtin_open_file" },
        value::BuiltinFunction{"__builtin_close_file"},
        value::BuiltinFunction{"__builtin_read_file" },
        value::BuiltinFunction{"__builtin_read_line" },
        value::BuiltinFunction{"__builtin_read_word" },

        //* FFI shit
        value::BuiltinFunction{"__builtin_dlopen"          },
        value::BuiltinFunction{"__builtin_dlsym"           },
        value::BuiltinFunction{"__builtin_ffi_call"        },
        value::BuiltinFunction{"__builtin_ffi_type_void"   },
        value::BuiltinFunction{"__builtin_ffi_type_int"    },
        value::BuiltinFunction{"__builtin_ffi_type_float"  },
        value::BuiltinFunction{"__builtin_ffi_type_double" },
        value::BuiltinFunction{"__builtin_ffi_type_uint8"  },
        value::BuiltinFunction{"__builtin_ffi_type_sint8"  },
        value::BuiltinFunction{"__builtin_ffi_type_uint16" },
        value::BuiltinFunction{"__builtin_ffi_type_sint16" },
        value::BuiltinFunction{"__builtin_ffi_type_uint32" },
        value::BuiltinFunction{"__builtin_ffi_type_sint32" },
        value::BuiltinFunction{"__builtin_ffi_type_uint64" },
        value::BuiltinFunction{"__builtin_ffi_type_sint64" },
        value::BuiltinFunction{"__builtin_ffi_type_struct" },
        value::BuiltinFunction{"__builtin_ffi_type_pointer"},
        value::BuiltinFunction{"__builtin_ffi_type_cstring"},
        value::BuiltinFunction{"__builtin_ffi_type_complex"},

        value::BuiltinFunction{"__builtin_ptr_to_string"},
    };


    inline static const std::initializer_list<std::string_view> builtins = {
        // "Any"                        ,
        // "Int"                        ,
        // "Double"                     ,
        // "String"                     ,
        // "Bool"                       ,
        // "Syntax"                     ,
        // "Type"                       ,

        "__builtin_rand_int"         ,
        "__builtin_print"            ,
        "__builtin_concat"           ,
        "__builtin_print_env"       ,
        "__builtin_panic"           ,
        "__builtin_id"              ,
        "__builtin_input_str"       ,
        "__builtin_input_int"       ,
        "__builtin_decltype"        ,
        "__builtin_type"            ,
        "__builtin_len"             ,
        "__builtin_reset"           ,
        "__builtin_eval"            ,
        "__builtin_neg"             ,
        "__builtin_abs"             ,
        "__builtin_not"             ,
        "__builtin_to_int"          ,
        "__builtin_to_double"       ,
        "__builtin_to_string"       ,
        "__builtin_get"             ,
        "__builtin_push"            ,
        "__builtin_push"            ,
        "__builtin_pop"             ,
        "__builtin_pop_front"       ,
        "__builtin_add"             ,
        "__builtin_sub"             ,
        "__builtin_mul"             ,
        "__builtin_div"             ,
        "__builtin_mod"             ,
        "__builtin_pow"             ,
        "__builtin_gt"              ,
        "__builtin_geq"             ,
        "__builtin_eq"              ,
        "__builtin_leq"             ,
        "__builtin_lt"              ,
        "__builtin_and"             ,
        "__builtin_or"              ,
        "__builtin_set"             ,
        "__builtin_conditional"     ,
        "__builtin_str_slice"       ,
        "__builtin_str_split"       ,
        "__builtin_open_file"       ,
        "__builtin_close_file"      ,
        "__builtin_read_file"       ,
        "__builtin_read_line"       ,
        "__builtin_read_word"       ,
        "__builtin_dlopen"          ,
        "__builtin_dlsym"           ,
        "__builtin_ffi_call"        ,
        "__builtin_ffi_type_void"   ,
        "__builtin_ffi_type_int"    ,
        "__builtin_ffi_type_float"  ,
        "__builtin_ffi_type_double" ,
        "__builtin_ffi_type_uint8"  ,
        "__builtin_ffi_type_sint8"  ,
        "__builtin_ffi_type_uint16" ,
        "__builtin_ffi_type_sint16" ,
        "__builtin_ffi_type_uint32" ,
        "__builtin_ffi_type_sint32" ,
        "__builtin_ffi_type_uint64" ,
        "__builtin_ffi_type_sint64" ,
        "__builtin_ffi_type_struct" ,
        "__builtin_ffi_type_pointer",
        "__builtin_ffi_type_cstring",
        "__builtin_ffi_type_complex",
        "__builtin_ptr_to_string"   ,
    };
};

} // namespace vm
} // namespace pie