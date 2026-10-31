# Contributing to Pie

Hi! Thanks for wanting to help with Pie!!

Somethings to keep in mind:
- read the [Philosophy](README.md#philosophy).
- Keep in mind that Pie is quirky on purpose
- Keep it whimsical
- Keep it fun! and creative.


## Getting Started

### Clone and Build
Fork the repo, then
```
git clone https://github.com/<you>/Pie
cd Pie
mkdir build
cd build
cmake ..
make Pie
```

You'll need `git`, `cmake`, `make`, and a compiler with C++23 support.

- The first `cmake ..` downloads all the dependencies, so it takes a while and needs internet.
- The targets you'll care about:
    - `make Pie`: the interpreter
    - `make Pie_debug`: same thing, with debug info
    - `make test`: builds and runs the tests (with sanitizers)
    - `make gh-actions`: what the CI runs. Don't run it
    - `make web`: the WebAssembly build for the website (needs `emcc`)


If you're on Windows, I don't know if it builds, or how it builds. If you get it working, please write down how. That alone is a great first contribution.


### Running Pie
```
./Pie                   # the REPL
./Pie file.pie          # run a file
./Pie -c "code here"    # run a string
```

When something goes wrong, these show you what each stage produced:

```
./Pie -t file.pie       # the tokens
./Pie -a file.pie       # the AST, printed back as code
./Pie -a -n file.pie    # the AST, without running it
```

`-a` is the most useful one. It prints the program the way the parser read it, so you can see right away if something got grouped wrong.


## Where Things Live
```
src/
    main.cc         command line flags
    CLI/            reads the file, runs the stages below in order
    Lex/            source -> tokens. Token.hxx has every token kind
    Parser/         tokens -> AST. Precedence.hxx has the precedence levels
    Expr/           the AST nodes
    Analysis/       name resolution (LexicalAnalysis): gives every name an ID
    Interp/         the tree-walking interpreter (and FFI)
    Type/           types and type checking
    Value/          runtime values
    Builtins/       the builtin function names
    Diagnostic/     errors and exceptions
    Utils/          small things everything uses
    Compiler/, VM/  the bytecode side (WIP, don't touch these yet)
    Declarations.hxx  shared declarations, including `Node` (the variant of every AST node)
std/                the standard library, written in Pie
Tests/              the tests
spec.md             the spec
```

### The Life of a Program
`CLI::runFile` does it all, in this order:

1- **Lex**: `lex::lex` turns the source into tokens.

2- **Parse**: `Parser` turns the tokens into a list of `expr::ExprPtr`. Operators are known while parsing, so the parser keeps track of which ones are declared in each scope. An `import` parses the imported file right there.

3- **Analyze**: `analysis::LexicalAnalysis` walks the AST and gives every name an ID, so the interpreter doesn't look names up by string.

4- **Run**: `interp::Visitor` walks the AST and evaluates it.

Visitors are used extensively. That is `LexicalAnalysis` and `Visitor` which is the interpreter.

A visitor is a class with an `operator()` for each AST node. You apply a visitor by calling `std::visit(*this, expr->variant())`.


## Adding a Feature
Talk about the design in an issue first (see [Changing the Language](#changing-the-language)). Once it's settled, a new piece of syntax usually goes through these, in order:

1- **Tokens** (`Lex/Token.hxx`, `Lex/Lexer.cxx`): only if you need a new keyword or punctuation. Remember that most symbols are name characters, so anything new you reserve is something a user can no longer name an operator.

2- **The AST node** (`Expr/Expr.hxx`): a struct deriving from `Expr`, with its fields, a constructor, `stringify`, `involvesName`, and `variant`. Don't forget to add it to `Node` in `Declarations.hxx`.

3- **Parsing** (`Parser/Parser.hxx`): usually a new case in `prefix` (things that start an expression) or `infix` (things that come after one).

4- **Analysis** (`Analysis/LexicalAnalysis.cxx`): an `operator()` for the node. If it declares names, this is where they get their IDs.

5- **Running** (`Interp/Interpreter.hxx`): an `operator()` for the node.

6- **Tests**: see [Tests](#tests).

7- **The spec**: update `spec.md`.

Check each step with `-a` before moving to the next one. If it doesn't print back right, it's not parsing right.


## Fixing a Bug
1- Make the smallest `.pie` file that shows the bug.

2- Turn it into a test that fails (see [Tests](#tests)). If you can't fix it, stop here: add the test with the `[!mayfail]` tag and a comment saying what's wrong.

3- Find which stage is wrong. `-t` for the lexer, `-a` for the parser. If both look right, it's the analysis or the interpreter.

4- Fix it.

5- Run all the tests along with your new ones.


## Code Style
Look at the code around what you're changing and match it. Also:

### General
- 4 spaces, no tabs.

- Headers are `.hxx`, sources are `.cxx`. Only `main.cc` has a `.cc` extension.

- Use `not`, `and`, and `or` instead of `!`, `&&`, and `||`.
- Use `using enum` inside a `switch` instead of repeating the enum name, unless the names of said enum clash with another enum whose names are already pulled into scope. In such case, use `using ShortEnumName = LongEnumName;`

- Errors go through `util::error`.



### Naming
- Types: `PascalCase`
- Functions: `camelCase`
- Variables and members: `snake_case`
- Enum values and constants: `UPPER_CASE`


### C Functions Are Not Qualified
C functions and types are used without `std::`:

```cpp
size_t i{};    // not std::size_t
puts("Hello"); // not std::puts
```

Everything from the C++ library keeps its `std::` (`std::vector`, `std::move`, `std::ranges::find`). Don't shorten namespace names. Instead, introduce a local alias for the type and/or function you're trying to use.

### Align Similar Lines
When lines do the same thing, line them up so the differences stand out:

```cpp
if      (argv[1] == "--tokens"sv) print_tokens = true;
else if (argv[1] == "--ast"sv   ) print_parsed = true;
else if (argv[1] == "--norun"sv ) norun        = true;
```

```cpp
case FLOAT :
case INT   : return expr::Num   {token};
case STRING: return expr::String{token};
```

### Name Your Boolean Arguments
A bare `true` or `false` in a call doesn't say anything. Name it first:

```cpp
constexpr auto PARSE_TYPE = true;
auto expr = parseExpr<PARSE_TYPE>();
```


## Tests
All the tests live in `Tests/Test.cc`.

### Running Them
Make sure you are in `Pie/build/`
```
make test                     # build and run everything
```

If you want to run the tests executable `run_tests` yourself, make sure you do so from `Pie/` instead of from inside `Pie/build/`.

```
./build/run_tests "[Tuple]"   # just one tag
./build/run_tests "Tuples!"   # just one test case, by name
```

### Writing Them
A test case looks like this:

```cpp
TEST_CASE("Some Feature", "[Tag][OtherTag]") {
{
    const auto src = R"PIE(
x = 1;
__builtin_print(x);
)PIE";

    REQUIRE(pie::test::run(src) == "1");
}
{
    const auto src = R"PIE(
x: String = 1;
)PIE";

    REQUIRE_THROWS_AS(pie::test::run(src), pie::except::TypeMismatch);
}
}
```

- `pie::test::run` returns everything the program printed.

- Group related checks into one program with one expected output.

- Use the builtins (`__builtin_print`, `__builtin_add`, ...) instead of `std`, so a test only breaks when the thing it tests breaks.

- Test the weird cases. That's where Pie breaks.


### Before You Open a PR
1- Every bug fix comes with a test that failed before the fix.

2- Every feature comes with tests.

3- Run `make test` and make sure every test passes!


## Common Changes

### Adding a Builtin
A builtin has to be added in a few places, and missing one gives you very confusing errors:

1- Add it to `NAMES` in `src/Builtins/Builtins.hxx`. A new name can go wherever it fits, except in the first 7 names of `NAMES`. Those are the builtin types.

2- Add it to the list in `isBuiltin`

3- Implement the function inside `src/Builtins/BuiltinFunctions.hxx`

4- `evaluateBuiltin` routes the call into the proper function implementation.


### Changing the Language
Open an issue first. Talk about the design before writing any code. Most of the work in Pie is figuring out what something should do, not making it do it.

If a change touches the grammar or how something behaves, update `spec.md` and the docs in the same PR.

## Pull Requests
- One thing per PR.
- The CI has to pass.
- Tests included (see above).
- Explain why, not just what.


## Help Wanted
- Windows build instructions
- Anything from the [Todo](README.md#todo-in-order-of-priority) list. Open an issue first, so we don't both work on the same thing.
- Better error messages. There's always room..:)
