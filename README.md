# Welcome to Pie Lang!

![Pie Lang Logo](assets/images/PieLang_Big_Transparent.png)

[![C++ CI](https://github.com/AliAlmutawaJr/Pie/actions/workflows/cpp.yml/badge.svg)](https://github.com/AliAlmutawaJr/Pie/actions/workflows/cpp.yml)

## Philosophy
<!-- This language aims to be unique, but it also should still feel familiar. Here are some of it's "features": -->
- Pie aims to be unique yet feel familiar
- Everything is an expression (no null/unit/none type)
- Bare-bones (if it doesn't need to be keyword, then it isn't + no operators)
- Still quirky (different even if the difference is not good)


### Index:
- [Variables](#variables)
- [Closures](#closures)
- [Classes](#classes)
- [Unions](#unions)
- [Collections](#collections)
- [Loops](#loops)
- [Match Expressions](#match-expressions)
- [Namespaces](#namespaces)
- [Scopes](#scopes)
- [Import System](#import-system)
- [Operators](#operators)
- [Overloading](#overloading)
- [Packs](#packs)
- [Built-in Functions](#builtins)
- [Types](#types)
- [Keywords List](#keywords-list)
- [Reserved Punctuation](#reserved-punctuation)
- [Comments](#comments)
- [Install](#install)
- [Community](#community)
- [Contributors](#contributors)
- [What was said about Pie](#quotes-about-pie-lang)


## Variables

Define variables using assignment with an optional type:

```pie
x = 5;
y: Int = 5;
```

## Closures

Closures have a familiar syntax (JS syntax).\
A nullary closure that returns `1` when called:

```pie
() => 1;
```

A closure that takes two arguments and returns the first:

```pie
(x, y) => x;
```

Closures can be assigned to variables:

```pie
func = (x) => "yay";
```

### Named Arguments

Any parameter can be named in the argument list in any order:

```pie
func = (one, two) => one;

func(two=2, one=1);
func(two=1, 1);
```

### Variadic Functions

A function can have at most **one** variadic parameter. The variadic parameter can be anywhere in the parameter list.

The variadic argument has to be annotated with a type with leading ellipsis `...<type>`:

```pie
getLast = (all: ...Any, last) => last;

x = getLast(1, 2, 3);
```

`x` will be 3.

Use the trailing `...` to expand a pack:
```pie
getFirst = (first, rest: ...Any) => first;

forward = (args: ...Any) => getFirst(args...);
```

## Classes

A class is a block (scope) preceded by the `class` keyword. The block must consist of **ONLY** assignments:

```pie
Human: Type = class {
    name: String = "";
    age: Int = 0;

    prettyPrint = () => {
        __builtin_print(name);
        __builtin_print(age);
    };
};
```

Construct an object by calling the constructor of the class. Optionally, pass initial values to the data members:

```pie
h: Human = Human("Pie", 1);
h.age = 10;
h.prettyPrint();
```

## Unions
Unions in Pie are what other languages call "Sum Types":

<!-- deprecated:
~~U: Type = union { Int | Double | String };~~ -->
```pie
U: Type = union { Int; Double; String; };

x: U = 1;
y: U = 3.14;
z: U = "Hello";
```

**Note:** The old syntax `union { Int | Double | String }` is deprecated. Use the `union` keyword with semicolons as shown above.

Unions also work with user-defined types.


## Collections
There are 2 collections in Pie

#### Lists
```pie
list: {type} = {expr1, expr2, expr3};

__builtin_get(list, 1);
__builtin_set(list, 2, expr4);
__builtin_set(list, 5, expr5); .: Error! Out of bound access
```

#### Maps
```pie
map: {type1: type2} = {key1: value1, key2: value2};

__builtin_get(map, key1);
__builtin_set(map, key2, value4); .: changes key2's value
__builtin_set(map, key3, value3); .: inserts a new value
```


## Loops
First, let's explore the general syntax of the loop construct, then we'll explore the other kinds.


```pie
loop 10 => {
    __builtin_print("Hi");
};
```
This program prints "Hi" 10 times.\
We can also introduce a loop variable:
```pie
loop 10 => i {
    __builtin_print(i);
};
```
Note that the braces can be omitted (with or without the loop variable).


**Kinds of Loops:**

There are 4 kinds of loops in Pie. They all utilize the `loop` keyword. The kind of the loop depends on the type of the loop operand

#### For Loop
When the type of the operand is an `Int`

#### While Loop
When the type of the operand is a `Bool`

#### Pack Loop
When the type of the operand is a pack of any kind (i.e. `...Any`)

#### Iterator Loop
When the type of the operand is an Object. The object **MUST** follow the **Iterator Protocol** which is defined as follows:
For an object to be qualified as an iterator, it must define 2 methods:
- `hasNext(): Bool`
- `next()`

`hasNext` must return a boolean indicating whether the loops should continue or terminate. `next` yields the next value.


#### Break/Continue


## Match Expressions

Match expressions can match against 3 things.
1. Value
2. Type
3. Structure


**Value Matching**:
```pie
x = 1;

match x {
    =1 => print("one");
    =2 => print("two");
    ="hi" => print("some string");
};
```

**Type Matching**:
```pie
x = 1;

match x {
    :String => print("str");
    :Int => print("num");
    :Double => print("float");
};
```


**Structural Matching**:
```pie
C = class { a = 0; b = "";};
c = C(314, C(1, "two"));

match c {
    C(x: Int, ="two") => print(x);
    C(y=3, :String = "something") => print(2);
    C(n: Int = 314, C(=1, ="two")) => print(n);
};
```
The code above ends up printing `314`.

Note how you can match both the value and the type at the same time. You can even give the matched value a name.
The collection of the tokens:\
`<name>: <type> = <value>`\
is called a `Single`.



 **Guards and such**:
 You can match against multiple patterns in a single case by using the pipe symbol `|` and you can guard against any case by using the ampersand `&`:

 ```pie
x = 5;

match x {
    =1 | =2 | =3 => print("one two three");
    a & __builtin_lt(a, 0) => print("negative");
    a & __builtin_gt(a, 0) => print("positive (not 1, 2, or 3)");
};
```

Of course, you can have both conditions and pipes in the same case.


## Namespaces

Declare a namespace by using the `space` keyword followed by a proper name:

```pie
space my_space {
    __builtin_print("start");

    decl1: Int = 1;
    ID: (Any): Any = (x) => x;

    space nested {
        __builtin_print("inner");
        decl2 = "Hi";
    };

    __builtin_print("finish");
};
```

Namespaces could seem like just syntactic sugar for classes, but they're not! There is a major difference which is the fact that you can run arbitrary code inside namespaces. A class may consist of assignments only.

To access a member of a `space`, use the "scope resolution operator", or `::`:

```pie
space x { a = 1; };
__builtin_print(x::a);
```

You may extend the contents of a namespace by declaring again:

```pie
space ns {
    a = 1;
    b = 2;
    c = 3;
};

space ns {
    a = 5;
    x = 10;
    y = 20;
};

__builtin_print(ns::a); .: prints 5
__builtin_print(ns::b); .: prints 2
__builtin_print(ns::x); .: prints 10
```
This allows you to split code that belongs to a single namespace in multiple different files and have all the declarations be in the same namespace.

**Keep in mind**, that namespaces are not variables. You can have a variable named the same name as a namespace!

Sample 1:
```pie
space x { };
x = 5;
__builtin_print(x); .: prints 5
```
Sample 2:
```pie
space x { x = 2; };
__builtin_print(x); .: prints 2
```

### `use` declaration
A `use` declaration introduces a name from one namespace into another:
```pie
space x { a = 1; };
use x::a;
a = 5;

__builtin_print(a);    .: prints 5
__builtin_print(x::a); .: prints 5
```

### `use space` directive

The `use space` directive pulls all the names in from a namespace into the current namespace. It works as if all the names inside the given namespace had a `use` declaration applied on them:
```pie
space ns {
    x = 1;
    y = "hi";
    z = 3.14;
};

use space ns;

__builtin_print(x);
```

## Scopes

Since everything is an expression, so are scopes! They take the value of the last expression in them.\
Here, `x` will be assigned to 3.

```pie
x = {
    1;
    2;
    3;
};
```

<!-- Since scopes take the value of their last expression, scopes cannot be empty!\
The following line will error: -->

**Note:**

The following line is not an emtpy scope, but rather, and empty list!!
```pie
x = { };
```



## Import System

`import` runs the specified file, then pull all the _global_ namespaces into the current namespaces. Any global in the specified module will **NOT** be imported.

in `../folder/module.pie`:
```pie
global_var = 1;

space global_space {
    x = 1;
};

{
    space local_space {
        y = 2;
    };
}

__builtin_print("in module");
```

in `main.pie`
```pie
import ../folder/module;
import ../folder/module;

__builtin_print(global_space::x);
```
Running the file gives the following output:
```
in module
in module
1
```

`local_space` is not accessible from `main.pie` since it isn't a top level namespace.
The `import` expression yields the last expression inside the imported file.
Note that `.pie` is omitted in the `import` directive.


## Operators

Pie doesn't provide any operators. One has to define their own. For that reason, any operator symbol (+, -, *, /, etc...) can be used as a variable name.

There are `5` types of operators that can be defined:

- `prefix`:\
    e.g. `- 1`
- `infix`:\
    e.g. `1 + 2`
- `suffix`:\
    e.g. `5 !`
- `exfix`:\
    e.g. `[ 0 ]`
- `mixfix`:\
    e.g. `if 1 then 2 else 3`

Here is how to define your own operator:

```pie
prefix(!) always_one = (x) => 1;
```

`always_one` is now a prefix operator, with the same precedence as operator `!`, that when applied, always returns `1`\
In this example, `a` will come out as 1.

```pie
a = always_one 5;
```

`infix` has to be assigned to a binary closure while `prefix` and `suffix` have to be assigned to a unary closure.

### Precedence
What goes between the parenthesis after the keyword (i.e.`prefix(+)`) is the precedence. You can use any operator you want and Pie will figure it out automatically. For example, operators with precedence level `+` have a lower precedence than operators annotated with precedence level `*`. You can use user-defined-operators as precedence level.

One can nudge the precedence level by attaching a `+` or a `-` after a precedence level:
```pie
infix(*)   star = (a, b) => 1;
infix(* -) plus = (a, b) => 2;
```
Here, the `plus` operator has lower precedence than `star` due to the `* -` notation, which means multiplication-level lowered by one.

An operator can also have the precedence of another operator:
```pie
infix(+)     add = (a, b) => __builtin_add(a, b);
infix(add)   sub = (a, b) => __builtin_sub(a, b);
infix(add +) mul = (a, b) => __builtin_mul(a, b);
```
Operator `sub` has a precedence that is equal to operator `add`'s precedence. Operator `mul`, on the other hand, has a higher precedence.

<!-- ### Operator Example

```pie

``` -->

## Overloading

You can overload operators based on the parameter types:
```pie
infix(+) + = (a: Int, b: Int): Int => __builtin_add(a, b);
infix(+) + = (a: String, b: String): String => __builtin_concat(a, b);

1 + 2;
"Hi" + "Bye";
```
The `1 + 2` calls the first operator. `"Hi" + "Bye"` calls the second!

## Packs
Packs in Pie are analogous to C++'s packs. One can only declare a pack as a function parameter:
```pie
func = (pack: ...Any) => __builtin_print(pack);
func(1, "Hello", 3.14);
```
Note that to declare a pack, the argument **MUST** be given a type preceded by ellipses. Packs may be empty.

#### Fold Expressions:
Pie supports Fold Expressions, much like C++:

##### Unary left fold
`(pack + ...)`

##### Unary right fold
`(... + pack)`

##### Binary left fold
`(init + pack + ...)`
`init` will be used as an initial value. Helps in the case where the pack is empty:

##### Binary right fold
`(... + pack + init)`


##### Separated unary left fold
`(pack + ... + sep)`
The above expressions evaluates like this:
`((((arg1 + sep) + arg2) + sep) + arg3)`
This can be useful if you wanted to create a CSV entry from a bunch of strings for example.

##### Separated unary right fold
`(sep + ... + pack)`
The above expression evaluates right-to-left:
`(arg1 + (sep + (arg2 + (arg3 + sep))))`
<!-- `(arg1 + (sep + (arg2 + (sep + arg3))))` -->


##### Separated binary left fold
`(init + pack + ... + sep)`

##### Separated binary right fold
`(sep + ... + pack + init)`


## Builtins

Since Pie doesn't provide any operators, how does one achieve _ANYTHING_ at all with Pie?\
Pie reserves the names starting with `__builtin_`.


#### IO
- `__builtin_input_int`
- `__builtin_input_str`
- `__builtin_print` (variadic - returns the last argument)

#### Arithmatic
- `__builtin_add`
- `__builtin_div`
- `__builtin_mod`
- `__builtin_mul`
- `__builtin_neg`
- `__builtin_pow`
- `__builtin_sub`

#### predicates
- `__builtin_eq`
- `__builtin_geq`
- `__builtin_gt`
- `__builtin_leq`
- `__builtin_lt`

#### Boolean
- `__builtin_not`
- `__builtin_and`
- `__builtin_or`

#### Containers
- `__builtin_get`  (accepts strings, packs, lists, and maps)
- `__builtin_len`  (accepts strings, packs, lists, and maps)
- `__builtin_pop`  (for lists)
- `__builtin_push` (for lists)
- `__builtin_set`  (for lists and maps)

#### String
- `__builtin_concat` (variadic)
- `__builtin_str_slice(str, start, steps, end)`

#### Conversion
- `__builtin_to_double`
- `__builtin_to_int`
- `__builtin_to_string`

#### Macros
- `__builtin_eval`
- `__builtin_reset`

#### Control Flow
- `__builtin_conditional(cond, then, else)`


## Types

#### Pie has 13 types.


##### 7 builtin types
- `Int`
- `Double`
- `Bool`
- `String`
- `Any`
- `Type`
- `Syntax`

##### 2 collection types
- `{type}`: list type
- `{type1: type2}`: map type

##### Functions types
- `(T1, T2): T3`

```pie
one: (Int): Int = (x: Int): Int => 1;
```

If something is left un-typed, it will be given the `Any` type.

##### Pack Types
- `...Type`
Read more about packs in the [packs sections](#packs)

#### Custom Types
See [Unions](#unions) and [Classes](#classes)

#### Types as Values
Types are values in Pie. A simple use case to demonstrate this is `Type Aliases`:
```pie
Num = Int;
x: Num = 1;
```

Some types, like function types, are not valid expressions, and therefore, the parser needs to know if it's in typing context or an expression context. To denote a typing context, we prefix the type with a colon `:`:

```pie
Func = :(Int, Any): String;
f: Func = (x, y): String => "hi";
```
#### Values as Types
Values as types (AKA: Literal Types in TypeScript) is a feature that allows you to use values as, well, ...types!


Here, `x` has type `1`, which means it can only be assigned to the value 1. It's sorta the only way to make "constants" in Pie for now.
```pie
x: 1 = 1;
```

Values can be used in [unions](#unions) too:
```pie
OneTwoThree: union { 1; 2; 3; } = 1;
OneTwoThree = 2;
OneTwoThree = 3;

OneTwoThree = 4; .: Error!
```

Mixing values of different types as types as well:
```pie

infix | = (t1, t2) => union { t1; t2; };

x: 1 | "hi" | false = 1;
x = "hi";
x = false;
x = true;  .: Error!
```

#### Concepts
Concepts (taken from C++) are unary predicate functions which are used as types. The value assigned to a variable with such type is checked by the unary function in order to type check.
```pie
infix > = (a, b) => __builtin_gt(a, b);

MoreThan10 = (x: Int): Bool => x > 10;
a: MoreThan10 = 15; .: type checks!
a = 5; .: Error!
```
Concepts also allow for what's known as "Design by Contract" where pre-conditions are the types of the arguments, and the post-condition is the return type.

#### Syntax Type

The `Syntax` type is a special type that gives a handle onto the AST node used to represent an expression.\
Take this example:

```pie
infix + = (a, b) => __builtin_add(a, b);

x: Syntax = 1 + a;
```

`x` is a handle to the AST which represents the expression `1 + a`.
To evaluate `x`, you just need to call __builtin_eval on it:

```pie
result = __builtin_eval(x);
```

However, evaluating `x` right now will error since `a` is not defined. All we need to do is define `a` before evaluating `x`.

```pie
a = 5;
result = __builtin_eval(x);
__builtin_print(result);
```

`6` will be printed.

## Comments

### Line comments

```pie
.: this is a comment
this isn't
```

#### Block comments

```pie
.::
    this is a comment
this is also a comment
so is this::.
this isn't
```


## Keywords List

##### Modules
- `import`
- `space`
- `use`
<!-- - `space` -->

##### Operators
- `prefix`
- `infix`
- `suffix`
- `exfix`
- `mixfix`

##### Object Orient Stuff
- `class`
- `union`
- `match`

##### Control Flow
- `loop`


###### Phantom keywords
- `true`
- `false`

## Reserved Punctuation

- `__builtin_*`
- `( )`
- `{ }`
- `,`
- `.`
- `...`
- `=`
- `=>`
- `:`
- `;`
<!-- - `..` -->

## Install

There are 2 ways to install Pie

##### Pre-built binaries
Binaries exist for:
1- Linux
2- Macos x86
2- Macos Apple Silicon

Check the [release section](https://github.com/AliAlmutawaJr/Pie/releases) for the download link


#### Build from source
Make sure you have `git`, `make`, and a C++ compiler that supports C++23. Then run the following in the terminal:

```
git clone https://github.com/AliAlmutawaJr/Pie
cd Pie
make
```


### Todo

#### in order of priority


- [ ] File IO
- [ ] Simplify Closure Captures without performance penalty (internal)
- [ ] Simplify Pack Expansion (internal)
- [ ] Zen of Pie (with the pie ascii art):
<!-- 
           .-.           
   .-..   ( __)   .--.   
  /    \  (''")  /    \  
 ' .-,  ;  | |  |  .-. ; 
 | |  . |  | |  |  | | | 
 | |  | |  | |  |  |/  | 
 | |  | |  | |  |  ' _.' 
 | |  ' |  | |  |  .'.-. 
 | `-'  '  | |  '  `-' / 
 | \__.'  (___)  `.__.'  
 | |                     
(___)                    
 -->
- [ ] Cascade operator `..`
- [ ] Fix variadic expansion
- [ ] Breaking Out of Nested Loops
- [ ] Use Big Int instead of `int64_t`
- [ ] `Expr::involveName()` seems to be not exhastive, espacially with Closure. Check it.
- [ ] Add default values to function parameters
- [ ] Make `=` and `=>` overloadable
- [ ] Fix builtin reset (value-reset, reset/name-reset) 
- [ ] World domination
- [ ] Move from Make to Bake
- [ ] Improve error messages (add line and column numbers)
- [ ] Add recursive operators
- [ ] Remove dependency on stdx and boost
- [ ] Add LLVM backend

---

### Done
- [x] Importing Operators
- [x] Compile to WASM & a web interface (YAAAY)
- [x] Remove preprocessor
- [x] Add `Syntax` literals instead of relying on receiver's type
- [x] Lexically Scoped Operators
- [x] Capture variables by reference in lambdas
- [x] Using delcarations introduce "references"
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

- ~~Capture Environment on Closure Creation~~
- ~~Allow variadics of Syntax type~~
- ~~Add collections~~
- ~~Add iterators~~
- ~~Add looping~~
- ~~Add method operators..?~~


## Community
[Join our  discord server](https://discord.gg/XRpKTptU)

## Contributors
- [ShawSumma](https://github.com/ShawSumma)
- [DerkT](https://github.com/DrkWithT)
- [TheDevConnor](https://github.com/TheDevConnor)


##### Website forked from:
- [TheDevConnor](https://github.com/TheDevConnor)


## Quotes About Pie Lang:
- "The whole language is a bug"
- "can't have your ~~cake~~ Pie and eat it too!"