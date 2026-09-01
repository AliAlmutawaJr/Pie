#pragma once

#include <cstddef>

namespace pie {
namespace vm {

enum class Code : size_t {
// Variables
    LOAD          , // `LOAD  x` pushes `x` onto the stack
    LOAD_CONST    , // `LOAD  x` pushes `x` onto the stack

    STORE         , // `STORE x` creates a variable `x` with the latest value on stack

// Globals
    LOAD_GLOBAL ,
    STORE_GLOBAL,


// for objects, lists, and maps
    LOAD_ACC , // `LOAD_ACC x, y` pushes `x.y` onto stack?
    STORE_ACC, // maybe `STORE_ACC x, y` stores top value in `x.y`


// Stack Operations
    // PUSH,  // `PUSH 1` pushes 1 onto the stack
    POP ,  // `POP` pops one value off the stack


// Functions
    CALL,      // `CALL 3` top value is the function. Pop the top 3 values for the arguments
    RET,


// Conditionals
    // `OP` test the top 2 values and pushes the result back
    EQ     ,
    GT     ,
    LT     ,
    GEQ    ,
    LEQ    ,
    JUMP_IF,   // `JUMP_IF label` jumps if the top value is true
    JUMP   ,   // uncondtional jump


    // MUST BE LAST to correcrtly indicate the size
    HALT,
};

constexpr auto BYTECODE_SIZE = static_cast<size_t>(Code::HALT);





inline const char* stringify(const Code code) {
    switch (code) {
        using enum Code;
        case LOAD        : return "LOAD"        ;
        case STORE       : return "STORE"       ;
        case LOAD_ACC    : return "LOAD_ACC"    ;
        case STORE_ACC   : return "STORE_ACC"   ;
        case LOAD_GLOBAL : return "LOAD_GLOBAL" ;
        case STORE_GLOBAL: return "STORE_GLOBAL";
        case LOAD_CONST  : return "LOAD_CONST"  ;
        case POP         : return "POP"         ;
        case CALL        : return "CALL"        ;
        case RET         : return "RET"         ;
        case EQ          : return "EQ"          ;
        case GT          : return "GT"          ;
        case LT          : return "LT"          ;
        case GEQ         : return "GEQ"         ;
        case LEQ         : return "LEQ"         ;
        case JUMP_IF     : return "JUMP_IF"     ;
        case JUMP        : return "JUMP"        ;
        case HALT        : return "HALT"        ;
    }

    // return "<UNKOWN>";
}


} // namespace vm
} // namespace pie
