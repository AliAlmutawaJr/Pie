# Welcome to Pie Lang!

<img src="assets/images/PieLang_Big_Transparent.png">


[![C++ CI](https://github.com/AliAlmutawaJr/Pie/actions/workflows/cpp.yml/badge.svg)](https://github.com/AliAlmutawaJr/Pie/actions/workflows/cpp.yml)


# Website!
[PieLang.org](https://pielang.org)

## Philosophy
- Pie aims to be unique yet feel familiar
- Everything is an expression (no null/none type)
- Bare-bones (if it doesn't need to be keyword, then it isn't + no operators)
- Still quirky (different even if the difference is not good)


## Install

There are 2 ways to install Pie

### Pre-built binaries
Binaries exist for:
1- Linux
2- Macos x86
2- Macos Apple Silicon

Check the [release section](https://github.com/AliAlmutawaJr/Pie/releases) for the download link


### Build from source (MacOS & Linux)
**Prerequisites:**

- `git`
- `cmake`
- `make`
- `C++23`

Run the following in the terminal:
```
git clone https://github.com/AliAlmutawaJr/Pie
cd Pie
mkdir build
cd build
cmake ..
make
```

### Build from source (Windows)
Pie uses CMake for its build system. I'm pretty sure it should work with Windows. As I am NOT a windows developer, I haven't tried the process of installing Pie.

If you're a windows dev and would like to contribute to Pie, writing a guide on how to install Pie on Windows is a good start.


### Todo (in order of priority)

- [ ] Improve error messages (add line and column numbers)
- [ ] Add generators/coroutines!

- [ ] Make `=` and `=>` overloadable
- [ ] Better File IO
- [ ] Move from Make to Bake
- [ ] Breaking Out of Nested Loops
- [ ] World domination
- [ ] Simplify Closure Captures without performance penalty (internal)
- [ ] Clean up builtin functions code (internal)
- [ ] `Expr::involveName()` seems to be not exhastive, especially with Closure. Check it.
- [ ] Fix builtin reset (value-reset, reset/name-reset) 
- [ ] Add LLVM backend


---

### Done
- [ ] Fix defer throwing (inside destructor) ((mostly));
- [x] Add Tuples!
- [x] Use Big Int instead of `int64_t`
- [x] Simplify Pack Expansion (internal)
- [x] Add default values to function parameters
- [x] Make sure to expand args for EVERY builtin function (`ffi_call` and `defer` come to mind)
- [x] Hidden Class Members
- [x] toString methods
- [x] Better Pattern Matching
- [x] Unpackments
- [x] Added CMakeLists.txt
- [x] defer!!!
- [x] f-strings
- [x] Add recursive operators
- [x] File IO
- [x] Zen of Pie (with the pie ascii art):
- [x] Cascade operator `..`
- [x] Importing Operators
- [x] Compile to WASM & a web interface (YAAAY)
- [x] Remove preprocessor
- [x] Add `Syntax` literals instead of relying on receiver's type
- [x] Lexically Scoped Operators
- [x] Capture variables by reference in lambdas
- [x] Using declarations introduce "references"
- [x] fix `use x` and `use space ns`;
- [x] Pie's internal environment system now uses unique IDs per unique variable, which helps wit shadowing
- [x] Made namespaces parse-time things instead of runtime
- [x] Added Lexical Scoping (1.5)
- [x] Functions as types (concepts)
- [x] Eager Parameter Evaluation (Lazy Function Parameter Types)
- [x] add `lex` and `value` namespaces to codebase
- [x] Add REPL
- [x] Changed error system to exceptions (will allow REPL)
- [x] Looping....maybee (added iterators!!!);
- [x] Fix arbitrary names typing issue
- [x] Same for loop^^
- [x] Casting ~~(maybe using `as`)~~(using builtins)
- [x] Implement separated fold expressions (like C++)
- [x] Add fold expressions (like C++)
- [x] Arbitrary function parameters
- [x] Add unions
- [x] Add `use` directive
- [x] Add an import system (modules)
- [x] Added global namespace access syntax
- [x] Add namespaces
- [x] Add match expression (like scala)
- [x] Add overloaded operators at runtime (instead of parse-time)
- [x] Fixed infix operators parsing right to left!
- [x] Implemented `__builtin_eq` for all values!
- [x] Add named parameters to some builtin functions
- [x] Add variadic arguments
- [x] Add named arguments
- [x] Change __builtin_{true|false} to true/false;
- [x] Fix operator return type checking..?
- [x] Add overloading operators
- [x] Change comments to `.::` and `::.`
- [x] Added `input_{str|int}`
- [x] Add Arbitrary Operators!!!
- [x] Add circumfix operators
- [x] Add lazy evaluation
- [x] Add constructor to classes..somehow
- [x] Allow recursion..somehow
- [x] Add classes
- [x] Add assignment to any expression
- [x] Add booleans
- [x] Add closures

---

### Discarded

- ~~Remove dependency on stdx and boost~~
- ~~Defer Expr~~
- ~~Capture Environment on Closure Creation~~
- ~~Allow variadics of Syntax type~~
- ~~Add collections~~
- ~~Add iterators~~
- ~~Add looping~~
- ~~Add method operators..?~~


## Community
[Join our  discord server](https://discord.gg/4JQvGaQ3BB)

## Contributors
- [ShawSumma](https://github.com/ShawSumma)
- [DerkT](https://github.com/DrkWithT)
- [TheDevConnor](https://github.com/TheDevConnor)
- [aesmail](https://github.com/aesmail)
- [TheDevConnor](https://github.com/TheDevConnor)


## Quotes About Pie Lang:
- "The whole language is a bug"
- "can't have your ~~cake~~ Pie and eat it too!"
- "is this like forth?"
- "kinda like smalltalk"
- "Elixir does this better"
- "Lisp can do this better"
- "is this C++?"
- "the syntax is similar to Rust"
- "ugly js"
- "better js"
