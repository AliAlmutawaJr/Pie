#pragma once


namespace pie {


enum class Code {
// Variables
    LOAD ,
    STORE,

// Objects
    LOAD_ACCESS , // for objects, lists, and maps
    STORE_ACCESS, // for x.y = z

// Functions
    CALL,

// Conditionals
    JUMP_IF,
};


} // namespace pie

