
PLEASE NOTE THAT THE SPEC IS NOT DONE YET!
------------------------------------------

# The Pie Programming Language Official Specification


### Index:
- [Chapter 1: Philosophy](#chapter-1-philosophy)
- [Chapter 2: Lexical Grammer](#chapter-2-lexical-grammer)
    - [2.1 Reserved Keywords](#21-reserved-keywords)
    - [2.2 Reserved Punctuation](#22-reserved-punctuation)
    - [2.3 Literal Values](#23-literal-values)
    - [2.4 Comments](#24-comments)
- [Chapter 3: Grammar](#chapter-3-grammar)
- [Chapter 4: Expressions](#chapter-4-expressions)
    - [4.1 Numbers](#41-numbers)
    - [4.2 Booleans](#42-booleans)
    - [4.3 Strings](#43-strings)
    - [4.4 Identifiers](#44-identifiers)
    - [4.5 Assignments](#45-assignments)
    - [4.6 Blocks](#46-blocks)
    - [4.7 Functions](#47-functions)
    - [4.8 Packs](#48-packs)
    - [4.9 Classes](#49-classes)
    - [4.10 Unions](#410-union)
    - [4.11 Types](#411-types)
    - [4.12 Collections](#412-collections)
- [Chapter 5: Types](#chapter-5-types)
    - [5.1 Where Can Types Appear?](#51-where-can-types-appear)
    - [5.2 Builtin Types](#52-builtin-types)
    - [5.3 Custom Types](#53-custom-types)
    - [5.4 Values as Types](#54-values-as-types)
    - [5.5 Concepts](#55-concepts)
    - [5.6 Type Conversions](#56-type-conversions)
    - [5.7 Type Aliases](#57-type-aliases)
- [Chapter 6: Operators](#chapter-6-operators)


## Chapter 1: Philosophy
Pie is an expression-only language with dynamic strong typing & user-defined operators.
At its core, Pie is just a collection of features deemed "cool" by its creator, Ali Almutawa Jr.




## Chapter 2: Lexical Grammer


### 2.1 Reserved Keywords
<!-- Before listing out the keyword, punctuation, and values lists, it's important to define what each of those words mean in Pie. -->

**Keywords** are modifiers, in the sense that they modify the next token (or tokens) to something of a <!-- potentially -->  different meaning than it would be otherwise. Keywords may **not** be assigned to.

Pie has 12 keywords:
- `class`
- `exfix`
- `import`
- `infix`
- `loop`
- `match`
- `mixfix`
- `prefix`
- `space`
- `suffix`
- `union`
- `use`

### 2.2 Reserved Punctuation

**Punctuation** are sigils and symbols (not including letters).
Punctuation may **not** be assigned to.


Pie reserves 12 punctuation:
- `(`
- `)`
- `{`
- `}`
- `,`
- `.`
- `..`
- `...`
- `=`
- `=>`
- `:`
- `;`


### 2.3 Literal Values
**Literal Values or Built-in Values** are values which need not any imports. They are readily available in the language. These values include numbers, strings, built-in types, and functions with names that start with `__builtin_`.
These values may be assigned to.

Literal Values are summerized in the following list:
<!-- since types are values, we'll add them here too! -->
- `Any`
- `Bool`
- `Double`
- `false`
- `Int`
- `String`
- `Syntax`
- `true`
- `Type`
- `__builtin_*`
- All String Literals
- All number literals


### 2.4 Comments
There 2 types of comments in Pie:
#### 2.4.1 Line Comments
Any text that is between `.:` and either a newline character `'\n'` or the end of file.

#### 2.4.2 Block Comments
Any text that is between `.::` and `::.`.



## Chapter 3: Grammar
A Pie program consists of 1 or more expressions.
The grammar is stateful, so the grammar below is ambiguous, espacially in `unary_op`, `binary_op`, `post_op`, `cirum_op`, and `op_call`.

A more specified grammar with judgment rules to disambiguate it will be provided soon in newer versions of Pie. As of now, this should suffice.

<!-- 
$\pi$ consist of a
The environment $\delta$ acts like a stack of lists. The lists contain operator info.

Operator info consists of:
- Operator Name
- Operator Kind
- Operator Precedence
-->

```
program := expr+

expr :=
      num
    | bool
    | string
    | name
    | list
    | map
    | grouping
    | block
    | typing_context_type
    | assignment
    | fix
    | unary_op
    | binary_op
    | post_op
    | circum_op
    | op_call
    | closure
    | call
    | class
    | access
    | namespace
    | space_access
    | use
    | use_space
    | use_fix
    | import
    | union
    | match
    | loop
    | break
    | continue
    | unary_fold
    | separated_unary_fold
    | binary_fold
    | separated_binary_fold



int :=
      -?[0-9]+


double :=
      \-?[0-9]+\.[0-9]+


num :=
      int | double


bool :=
      "true"|"false"


string :=
      \"[^\"]*\"


name :=
      ^(?!\d+$)[a-zA-Z0-9\?!@#$%^&|*+~\-_\\\'\/<>\[\]]+$


list :=
      "{" (expr ("," expr)*)? "}"


map :=
      "{" ":" "}"
    | "{" expr : expr ("," expr : expr)* "}"


grouping :=
      "(" expr ")"


block :=
      "{" (expr ";")+ "}"


type :=
      expr
    | "(" (type ("," type)*)? ")" ":" type
    | "{" type "}"
    | "{" type ":" type "}"


typing_context_type :=
      ":" type


type_annotation :=
      ":" type


assignment :=
      expr "=" expr
    | name type_annotation? "=" expr


inferred_assignment :=
      expr ":=" expr


unpack :=
      "{" (expr ("," expr)*)? "}"
    | "{" expr : expr ("," expr : expr)* "}"

unpackment :=
      unpack ("="|":=") expr


expansion :=
      expr "..."

closure :=
      "(" ((expr|name type_annotation?))* ")" type_annotation?  "=>" expr


fix :=
      prefix
    | infix
    | suffix
    | exfix
    | mixfix


prefix :=
      "prefix" ("(" prec ")")? name "=" "(" expr type_annotation? ")" "=>" expr


infix :=
      "infix" ("(" prec ")")? name "=" "(" expr type_annotation? "," expr type_annotation? ")" "=>" expr


suffix :=
      "suffix" ("(" prec ")")? name "=" "(" expr type_annotation? ")" "=>" expr


exfix :=
      "exfix" name ":" name "=" "(" expr type_annotation? ")" "=>" expr


exfix :=
      "exfix" "(" prec ")" ":"? name (name | ":")* "=" "(" (expr type_annotation)* ")" "=>" expr


unary_op :=
      name expr


binary_op :=
      expr name expr


post_op :=
      expr name


circum_op :=
      name expr name


op_call :=
      (name (name|expr)*) | (expr name (name|expr)*)


call :=
      expr "(" (expr ("," expr)*)? ")"


class :=
      "class" "{" (assignment ";")* "}"


union :=
      "union" "{" (type ";")* "}"


access :=
      expr "." name


namespace :=
      "space" "{" (expr ";")* "}"


space_access :=
      name "::" name


use :=
      "use" name ("::" name)+


use_space :=
      "use space" name ("::" name)* "::"?


use_fix :=
      "use" ("prefix"|"infix"|"suffix"|"exfix"|"mixfix") name ("::" name)* "::"


import :=
      "import" name


single :=
      expr? ("=" expr)? (":" type)?


structure :=
      name "(" (pattern ("," pattern)*)? ")"


pattern :=
      single | structure


match :=
      "match" expr "{" (pattern ("&" expr)? ("|" pattern ("&" expr)?)* "=>" expr ";")* "}"


loop :=
    "loop" (((unpack|expr) ":")? expr)? expr ("=>" expr)?


break :=
      "break" expr


continue :=
      "continue"


unary_fold :=
      "(" expr name "..." ")"
    | "(" "..." name expr ")"


separated_unary_fold :=
      "(" expr name "... name expr ")"


binary_fold :=
      "(" expr name expr name "..." ")"
    | "(" "..." name expr name expr ")"


separated_binary_fold :=
      "(" expr name expr name "..." name expr ")"
    | "(" expr name "..." name expr name expr ")"

```

<!--
```
$$
\frac{\text{Premise 1} \quad \text{Premise 2}}{\text{Conclusion}} \text{ (Operator Definition)}
$$
```
-->




## Chapter 4: Expressions

### 4.1 Numbers
Any number, integer or double, is a valid expression in Pie.
Pie uses big nums for its integer types

### 4.2 Booleans
Pie booleans have 2 literals: `true` and `false`.

### 4.3 Strings
Anything that starts and ends with quotes (`"`) is considered a string.

### 4.4 Identifiers
Identifiers are names that bind to values. What you know as "variables".

Identifier names can either be proper or improper.
#### 4.4.1 Proper Names
A name is proper if it has these three properties:
- Contain **NO** spaces
- Must **NOT** purely consist of numbers.
- Consist of the following sigils only:
    `abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$%^&|*+~-_\'/<>[]`

These identifiers are special because they can be annotated with a type, unlike improper names.

#### 4.4.1 Improper Names
Any valid Pie expression, that is not a proper name already, is a valid improper name. This means that all the following are valid identifiers:
- `5`
- `"hello"`
- `func(x, y)`
- `1 + 2`

These identifiers cannot be annotated with a type.


### 4.5 Assignments
When using the infix operator `=`, its LHS gets assigned to the value of the RHS.
`x = y` will set `x` to the value of `y`.

Assignments yield their new value.

#### 4.5.1 Order Of Evaluation
Assignments evaluate their RHS before evaluating the LHS.

This means chained assignments will work as expected:
`x = y = z` will assign both `x` and `y` to the value of `z`.

#### 4.5.2 Generality Of Assignments
Any expression may be assigned to any other expression, which means the following assignments are all valid:
- `1 + 2 = "hi"`
- `func(1, 2) = true`
- `3 = 2 + 3`
- `1 = 0`

#### 4.5.3 Type Annotations
Assignments can optionally have a type annotation.
Only proper names may be annotated:
- `x: Int = 1;`
- `y: String = "hi";`
- `true: Int = 5;`

The following programs are ill-formed:
- `1: Int = 5;`
- `1: String = "hi";`
- `1 + 2: Any = "hi";`
- `"bye": Any = true;`

#### Declarations vs Assignments
When assigning to a variable for the first time, the assignment also declares the variable as a new variable. Any subsequent assignments are will reassign to the existing variable.
*However*, if a subsequent assignment contained a type annotation, then that assignment will declare a new variable with a possible different type.

**Example:**
The following program is well-formed:
```pie
x = 1;
x = 2;
x = "hi";


a: Bool = true;
a = false;


var: Int = 5;
var: String = "five";
```

The following program is ill-formed:
```pie
var: Int = 5;
var = "five";
```

Bringing scopes into it, the following program is well-formed:
```pie
x = 5;
y = "hi";
{
    x = 10;
    y: Any = "bye";
};
.: x is 10 here
.: y is "hi" here
```


### 4.6 Blocks
Blocks allow you to run multiple expression-statements as a single expression. They evaluate the expressions in order before then yielding the last value in them.
```pie
a = {
    x = 1;
    func(true);
    "hi";
};
```
`a` is assigned to `"hi"`.

<!-- todo: add docu-link -->
<!-- See the scopes sections for the scoping rules of blocks. -->

**Note:** there is no such thing as an empty scope. The expression `{}` is an empty list.

### 4.7 Functions

Functions consist of 2 main part, the parameter list, and the body.
`(a, b) => 1;`
This is a function that takes 2 arguments, `a` and `b`, and returns 1.

The parameter list may optionally contain type annotations for any of the parameters. The return type is also optioinal (before the fat arrow):
```pie
(i: Int, s: String, b: Bool, a: Any): Int => 0;
```
If a parameter or the return type is left un-annotated, its type becomes `Any`.

#### 4.7.1 Named arguments
```pie
func = (a, b, c) => b;
result = func(5, c=10, a = 20);
```
`result` will be `5`.

Mixing between named arguments and positional arguments is allowed. If the name of a named-argument argument is not found in the parameter list, the program is ill-formed.

#### 4.7.2 Parameter Packs
A parameter pack is a parameter that swallows all the extra arguments in a function call.
To indicate that a parameter is a pack, it must be annotated with a type preceded by an ellipsis (`...`).
Packs are allowed to be empty!
```pie
reduce_to_one = (x: ...Any): Any => 1;

one      = reduce_to_one(1, "hi", true, {});
also_one = reduce_to_one();
```
At most, one parameter pack may be introduced per function.
The following program is ill-formed:
`(p1: ...Int, p2: ...String) => 0;`


However, other regular parameters are allowed:

```pie
getFirstNum  = (first: Int, rest: ...Int) => first;
getFirstNum(1);       .: returns 1
getFirstNum(1, 2, 3); .: returns 1

getLastString = (pack: ...String, last: String) => last;
getLastString("hi");                 .: returns "hi"
getLastString("hi", "bye", "what!"); .: returns "what!"

someOtherFunc = (a, b, pack: ...Any, z) => z;
someOtherFunc(1, 2, 3);          .: returns 3
someOtherFunc(1, 2, 3, 4, 5, 6); .: returns 6
```

For more information about packs, check out the [section 4.8 Packs](#48-packs)

#### 4.7.3 Auto Currying
Functions in Pie will be curried if the number of arguments is less than the number of parameters.

```pie
add = (a, b) => __builtin_add(a, b);
add3 = add(3);

eight = add3(5);
```

Calling a function that expects arguments without any arguments will yield the same function back:
```pie
f1 = (a, b, c) => 1;
f2 = f1();
```
`f1` and `f2` are the same function.

\
If a parameter pack is present in the function's parameter list, then currying will only happen if the number of arguments is less than the number of non-pack parameters.
If the number of arguments is more than the index of the position of the pack in the parameter list, then the pack is consumed from the parameter list and is introduced as an empty pack inside the environment of the resulting function. Basically, as if an empty pack was passed as an implicit argument to the pack parameter.
Example:
```pie
func = (a, b, pack: ...Any, c, d) => 0;

f1 = func(1);
f2 = func(1, 2);
f3 = func(1, 2, 3);
f4 = func(1, 2, 3, 4);
f5 = func(1, 2, 3, 4, 5);
```
- `f1` is `(b, pack: ...Any, c, d) => 0`
- `f2` is `(pack: ...Any, c, d) => 0`
- `f3` is `(d) => 0`
- `f4` is `0`
- `f5` is `0`

#### 4.7.4 Function Types
Variables assigned to functions could be annotated with function types:
```pie
Int2Int: (Int): Int = (x: Int): Int => 1;

String2Int: (String): Int = (s: String): Int => 1;

getBool: (): Bool = (): Bool => true;
```

<!-- todo: add a docu-link -->
See the types section to see what conversions are allowed.


#### 4.7.5 Eveything Is A Function
Any variable is considered to be a nullary function which yields its own value:
```pie
x = 5;
a = x;
b = x();
```
Both `a` and `b` are `5`.


### 4.8 Packs
Packs can only be introduced as parameters to functions as shown in section [4.7.2 (Parameter Packs)](#472-parameter-packs).

#### 4.8.1 Expansions
Packs may be expanded into other function calls using trailing ellipsis (`...`).
```pie
makePack = (ps: ...Any) => ps;
func = (a, b, c) => c;

pack = makePack(1, 2, 3);

func(pack...);
```
The last line is the expansion. Parameters `a`, `b`, and `c` become `1`, `2`, and `3` respectively.

Note that if the ellipsis weren't present, then parameter `a` would become the pack containing `1, 2, 3`:
```pie
func(pack, true, 3.14);
```
`b` and `c` become `true` and `3.14` respectively.


#### 4.8.2 Fold Expressions
Fold expressions are expressions that operate on packs (see section [4.7.2 Parameter Packs](#472-parameter-packs)).

A fold expression is a binary operator that operates on a pack and ellipsis. The epxression must be surrounded by parenthesis.

There are 8 kinds of fold expressions, which are the cartesian product of these types of folds:
- Unary vs Binary Fold
- Left vs Right Fold
- Normal vs Separated Fold


##### 4.8.2.1 Unary Fold Expression:
Unary fold expressions do **not** have an initial value. This means one **cannot** fold over an empty pack.

- Left Folds:
    ```pie
    func = (pack: ...Any) => (pack + ...);
    func(1, 2, 3, 4);
    ```
    The expression above unfolds to:
    `(((1 + 2) + 3) + 4)`.

- Right Folds:
    ```pie
    func = (pack: ...Any) => (... + pack);
    func(1, 2, 3, 4);
    ```
    The expression above unfolds to:
    `(1 + (2 + (3 + 4)))`.

The following program is ill-formed because the pack is empty:
```pie
func = (pack: ...Any) => (pack + ...);
func();
```


##### 4.8.2.2 Binary Fold Expression
Binary fold expressions have an initial value, which goes at the opposite side of the ellipsis.

- Left Folds:
    ```pie
    init = 10;
    func = (pack: ...Any) => (init + pack + ...);
    func(1, 2, 3, 4);
    ```
    The expression above unfolds to:
    `((((10 + 1) + 2) + 3) + 4)`.

- Right Folds:
    ```pie
    init = 10;
    func = (pack: ...Any) => (... + pack + init);
    func(1, 2, 3, 4);
    ```
    The expression above unfolds to:
    `(1 + (2 + (3 + (4 + 10))))`.

In the case of an empty pack, the expression yields the initial value:
```pie
inti = 10;
func = (pack: ...Any) => (init + pack + ...);
x = func();
```
`x` is `10`.



##### 4.8.2.3 Separated Fold Expressions
Separated fold expressions add the ability to add seperators between each operation. The separator goes on the opposite side of the pack.
Spearated fold expressions, too, have a left-right, unary-binary counterpart.

- Unary Left Folds:
    ```pie
    seperator = 5;
    func = (pack: ...Any) => (pack + ... + seperator);
    func(1, 2, 3, 4);
    ```
    The expression above unfolds to:
    `(((((((1 + 5) + 2) + 5) + 3) + 5) + 4)`.

- Unary Right Folds:
    ```pie
    seperator = 5;
    func = (pack: ...Any) => (seperator + ... + pack);
    func(1, 2, 3, 4);
    ```
    The expression above unfolds to:
    `(1 + (5 + (2 + (5 + (3 + (4 + 5))))))`.

Separated unary folds also don't support empty packs.

<!-- these seem wrong...check them again -->
- Binary Left Folds:
    ```pie
    init = 10;
    seperator = 5;
    func = (pack: ...Any) => (init + pack + ... + seperator);
    func(1, 2, 3, 4);
    ```
    The expression above unfolds to:
    `((((((((10 + 1) + 5) + 2) + 5) + 3) + 5) + 4)`.

- Unary Right Folds:
    ```pie
    init = 10;
    seperator = 5;
    func = (pack: ...Any) => (seperator + ... + pack + init);
    func(1, 2, 3, 4);
    ```
    The expression above unfolds to:
    `(1 + (5 + (2 + (5 + (3 + (5 + (4 + (5 + 10))))))))`.

Just like normal binary fold expression, if the pack is empty, the expression yields the initial value


### 4.9 Classes
Classes are first class citizens in Pie:
```pie
Human = class {
    name: String = "";
    age = 0;

    setAge = (a: Int) => age = a;
};
```
The class must consist of assignment expressions only.

<!-- todo: add docu-link -->
<!-- See the scopes sections for the scoping rules of classes. -->

#### 4.9.1 Self
Data members that are assigned to functions, AKA methods, have access to a special variable with the name `self`. This variable is a reference to the object itself. Anything that can be done to an object, can also be done with a `self` reference.

#### 4.9.2 Constructors
Every class is provided with a constructor that initializes the data members in the order they appear in the class.

If a class member is initialized through the constructor, its initializer expression is never executed.

If no value is provided for a data members in the constructor call, its initializer value is used as a default.

```pie
C = class {
    a = 0;
    b: String = "";
    c: Bool = false;
};

object1 = C();
object2 = C(10, "hi");
object3 = C(10, "hi", true);
```
- In `object1`, `a` is `0`, `b` is `""`, and `c` is `false`
- In `object2`, `a` is `10`, `b` is `"hi"`, and `c` is `false`
- In `object3`, `a` is `10`, `b` is `"hi"`, and `c` is `true`



### 4.10 Unions
Unions allow for a varibale to have multiple types:

```pie
U: Type = union {
    Int;
    Double;
    String;
};

x: U = 1;
y: U = 3.14;
z: U = "Hello";
```
Unions also work with user-defined types.


### 4.11 Types
See [Chapter 5: Types](#chapter-5-types).


### 4.12 Collections

#### 4.12.1 Lists
Lists in Pie are a comma-separated-expressions wrapped with openning and closing curly braces.

- `{}` is an empty list
- `{1}` is a list containing the integer `1`
- `{true, 5, "hi"}` is a list with 3 elements


#### 4.12.2 Maps
Key-value pairs where the key and the value are separated by a colon (`:`) and the pairs are separated by commas (`,`).
- `{:}` is an empty map
- `{"one": 1}` map with key `"one"` mapped to value `1`
- `{1: 2, true: "yes"}` map containing 2 pairs




## Chapter 5: Types

### 5.1 Where Can Types Appear?

#### 5.1.1 Variable Declarations
`name: type = expr`
#### 5.1.2 Function Parameters
`(x: type) => body`

#### 5.1.3 Function Return Type
`(): type => body`

#### 5.1.4 Typing Context
A typing context is anywhere in the program that an expression is expected, but a type is needed. To enter a typing context, prefix the type with a colon (`:`). This tells the parser to expect a type rather than an expression.
For example, the following program is ill-formed:
```pie
Int2Bool = (Int): Bool;
```
It can be fixed using the typing-context-operator:
```pie
Int2Bool = :(Int): Bool;

func: Int2Bool = (x: Int): Bool => true;
```
Typing context can also be entered even if there isn't a neeed to. The following program is well-formed:
```pie
i1 = Int;
i2 = :Int;
```


### 5.2 Builtin Types
#### 5.2.1 Primitive Types:
- `Any`: can represent any value
- `Bool`: represents only `true` and `false`
- `Double`
- `Int`
- `String`
- `Syntax`
- `Type`


#### 5.2.2 Collection Types
- `{type}`: list type
- `{type1: type2}`: map type

#### 5.2.3 Function Types
Examples:
- `(Int, Double): String`
    A function that takes an `Int` and a `Double` and returns a `String`.

- `(): Any`
    A function that takes no arguments and returns `Any`.

#### 5.2.4 Pack Types
Any type that is preceeded with ellipsis:
- `...type`

### 5.3 Custom Types
#### 5.3.1 Classes
##### 5.3.1.1 Syntax & Semantics
See [4.9 Classes](#49-classes).

##### 5.3.1.2 Subtyping
Pie is **structurally typed**, which means it follows a structural heirarchy.

`ClassA` is considered a subtype of `ClassB` if and only if all the members of `ClassB` exist in `ClassA` with the same type.

Example:
```pie
Human: Type = class {
    name: String = "";
    age = 0;
};

Named: Type = class {
    name: String = "default";
};

h = Human("Pie", 2);
n: Named = h;

n.name = "Cake"; .: changes h.name too
```

#### 5.3.2 Unions
See [4.10 Unions](#410-unions).


### 5.4 Values as Types
Values can be used as types in Pie. A varaible with a given type that is a value will only be able to be assigned to that value itself:
```pie
x: 1 = 1;
y: "hi" = "hi";
z: true = true;
```
This, on its own, is not useful. However, paired with unions, it can be very powerful:
```pie
OneToThree: Type = union { 1; 2; 3; };

x: OneToThree = 1;
x = 2;
x = 3;
```

### 5.5 Concepts
Concepts are unary predicate functions which are used as types. The value assigned to a variable with such type is checked by the unary function in order to type check.

This program is ill-formed:
```pie
MoreThan10 = (x) => __builtin_gt(x, 10);
a: MoreThan10 = 5;
```
This program is well-formed:
```pie
MoreThan10 = (x) => __builtin_gt(x, 10);
a: MoreThan10 = 15;
```

Concepts also allow for what's known as "Design by Contract" where pre-conditions are the types of the arguments, and the post-condition is the return type.


### 5.6 Type Conversions

#### 5.6.1 Implicit Conversions
Implicit Conversion is the event of assigning a value of some type to a variable declared with some other type without explicit casting.

A type is said to be converitble to to another type if it can be implicitly convertible to the other type.


#### 5.6.2 Allowed Conversions
Only the following conversions are allowed to happen implicitly.
- Any type -> `Any`
<!-- - Any type -> `Syntax` -->
- Any type `T` -> `union { ...; T; ...; }`


#### 5.6.3 Function Types Conversions
A function type `F1` is convertible to another function type `F2` if and only if:
- The number of parameters of `F1` and `F2` are equal.
- All the paramater types of `F2` are convertible to the parameter types of `F1` respectively.
- The return type of `F1` is convertible to the return type of `F2`.

---
Note that the type of the value is used instead of the type of the variable when doing these conversion checks.

The following program is well-formed.
```pie
Number = union { Int; Double; };

x: Number = 5;
a: Int = x;
```
Even though the union type is not convertible to `Int`, the program is valid because the type of the value of `x` is `Int`.


### 5.7 Type Aliases
Since types are valid expressions, type aliases are as easy as declaring a new variable:
```pie
Integer = Int;
f64 = Double;

x: Integer = 42;
y: f64 = 3.14;
```


<!-- 
### 5.6 Syntax Type

The `Syntax` type is a special type that gives a handle onto the AST node used to represent an expression.\
Take this example:

```pie
add = (x, y) => __builtin_add(x, y);

s: Syntax = add(a, 1);
```

`s` is a handle to the AST which represents the expression `add(a, 1)`.
To evaluate `s`, you just need to apply `__builtin_eval` on it:

```pie
result = __builtin_eval(s);
```

However, evaluating `s` right now will error since `a` is not defined. All we need to do is define `a` before evaluating `s`.

```pie
a = 5;
result = __builtin_eval(s);
__builtin_print(result);
```

`6` will be printed.
-->



## Chapter 6: Operators


### 6.1 Syntax
There are 4 main parts to declaring a new operator.
`<kind> <(precedence)>? <operator_name> = <closure>`
- Operator Kind
- Precedence Level
- Operator Name
- Closure Literal

example:
`infix + = (a: Int, b: Int): Int = __builtin_add(a, b);`

#### 6.1.1 Kind:
The kind of the operator based on how it should be be parsed. There are 5 kinds:
- `prefix`:
    - `++ x`: `++` is the prefix operator
    - `! true`: `!` is the prefix operator
- `infix`:
    - `1 + 2`: `+` is the infix operator
    - `1 * 2`: `*` is the infix operator
- `suffix`:
    - `c ++`: `++` is the suffix operator
- `exfix`
    - `[ x ]`: The surrounding `[ ]` is the operator.
- `mixfix`
    - `if cond then x else y`: `if`, `then`, and `else` are the operator. `cond`, `x`, and `y` are the arguments.


#### 6.1.2 Precedence:
Precedence dictates the order the parser should parse the operator in a compound expression.

##### 6.1.2.1 Available Precedence Levels
Pie understand the precendence levels of these operators, which are ordered from the lowest to hight level:
- `=`
- `||`
- `&&`
- `|`
- `^`
- `&`
- `==`, `!=`
- `<=`, `<`, `>`, `>=`
- `<=>`
- `<<`, `>>`
- `+`, `-`
- `*`, `/`, `%`
- `!`, `~`
- `[]`
- `()`
- `::`

In addition to those, there are 2 more special precedence levels: `HIGH`, and `LOW`. These 2 special values, which may not be used on their own. They may only be used as nudged precedences.

Note that any user-defined-operators may also be used as a precedence level.

##### 6.1.2.2 Nudging Precedence
Precedence levels may be used as-is, or nudged. Nudging can be used to allow for more precise control over the precedence of an operator.

To nudge a precedence level, use a single `+` or `-` sign:
```pie
infix(+     ) plus  = (a, b) => __builtin_add(a, b);
infix(plus +) times = (a, b) => __builtin_mul(a, b);


x = 1 plus 2 times 3;
```
`x` will be `7` since operator `times` has a precedence level that is higher than operator `plus`.


Note than a higher precedence level will always be higher than a lower precedence level no matter how many times it gets nudged down:

```pie
infix(*  -) L1 = (a, b) => 1;
infix(L1 -) L2 = (a, b) => 2;
infix(L2 -) L3 = (a, b) => 3;
...
infix(L9 -) L10 = (a, b) => 10;

infix(+) p = (a, b) => 0;

x = 1 L10 2 p 3;
```
`x` will be `0` because `p` has a lower precedence even though `L10` has been nudged down from `*` 10 times!



##### 6.1.2.3 Precedence Omission
In cases where the operator matches a precedence level, the precedence may be omitted:
```pie
infix + = (a, b) => __builtin_add(a, b);
infix * = (a, b) => __builtin_mul(a, b);

x = 1 + 2 * 3;
```
---
Note that `exfix` operators don't take any precedence level.

#### 6.1.3 Operator Name
Any proper name is a valid name for an operator. See [section 4.4.1 Proper Names](#441-proper-names)


##### 6.1.3.1 Exfix Operators
`exfix` operators are special because they have 2 names. To differentiate between the first name and the second name, a colon (`:`) is inserted between them:
```pie
exfix op1 : op2 = (a) => __builtin_print(a);

op1 "hello" op2;
```
The string `"hello"` should be printed.

##### 6.1.3.2 Mixfix Operators
`mixfix` operators are special since they could contain more than 1 name.
- A space is required between each name
- Insert a comma where an argument would go.
```pie
mixfix(LOW +) please print : to the terminal = (a) => __builtin_print(a);
mixfix(LOW +) this operator takes no arguments = () => __builtin_print("wow");
mixfix(LOW +) add : and : and maybe : too = (a, b, c) => __builtin_add(a, __builtin_add(b, c));


please print "hi" to the terminal;
this operator takes no arguments;
x = add 1 and 2 and maybe 3 too;
__builtin_print(x);
```
The following should be printed:
```
hi
wow
6
```



#### 6.1.4 Closure Literal
The operators **MUST** be assigned to closure literals. The arity of the closure depends on the kind of the operator:

- `prefix`: Unary closure
- `infix`: Binary closure
- `suffix`: Unary closure
- `exfix`: Unary closure
- `mixfix`: n-ary closure when `n` equals the number of colons (`:`) within the operator name.


### 6.2 Operator Overloading
Operators with the same name be overloaded based on the types of the parameters:
```pie
infix + = (a: Int, Int): Int => __builtin_add(a, b);
infix + = (a: String, String): String => __builtin_concat(a, b);


x = 1 + 2;
s = "Pie" + " is cool";
```
`x` will be `3`, and `s` will be `"Pie is cool"`.

#### 6.2.1 Overload Resolution

...
<!-- 
Resolving which overload Pie chooses depends on the type of the value passed to the operator. The operator with a perfect match **MUST** be picked. If there any no perfect matches... -->



