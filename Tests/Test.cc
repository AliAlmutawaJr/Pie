#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include <stdexcept>
#include "TestSuite.hxx"

#include "../src/Type/Type.hxx"




// LOOK AT THIS AT SOME POINT
// C = class {
//     pack: ...Int = 0;
// };
// makeC = (x: ...Int) => C(x);
// std::print((makeC(1, 2, 3).pack + ...));




TEST_CASE("Map and List Comprehensions", "[Comp]") {
    const auto src1 = R"(
l = { loop i: 10 => i };
m = { loop i: 10 => i: __builtin_mul(i, 2) };

__builtin_print(l);

loop key : 10 {
    value = __builtin_get(m, key);
    __builtin_print(key, ": ", value, sep = "");
};

)";

    REQUIRE(pie::test::run(src1) == R"({0, 1, 2, 3, 4, 5, 6, 7, 8, 9}
0: 0
1: 2
2: 4
3: 6
4: 8
5: 10
6: 12
7: 14
8: 16
9: 18)");
}



TEST_CASE("Loop Expression Used As NOT A Statement", "[Loop]") {
    const auto src1 = R"(
x = {loop i: {
    __builtin_print("hello");
    __builtin_conditional(__builtin_eq(i, 2), break "done", 0);
}};

__builtin_print(x);
)";

    REQUIRE(pie::test::run(src1) == "hello\nhello\nhello\n{done}");
}




TEST_CASE("Chained Unpackment!", "[Unpack]") {
    const auto src1 = R"(
print = __builtin_print;


{...w: Int = 5, end}    = 
{x: a, y: {...b}, z: c} =
{1, 2.5, 3}             =
{"one", {"two", "two and a half"}, "three", "four", "five", "six"};


print((w: Int = 5));
print(end);

print(1);
print(2.5);
print(3);

print(x, a);
print(y, b);
print(z, c);

)";

    REQUIRE(pie::test::run(src1) == R"(one, {two, two and a half}, three, four, five
six
one
{two, two and a half}
three
0 one
1 two, two and a half
2 three)");
}



TEST_CASE("Unpackment Reassignments!", "[Unpack]") {
    const auto src1 = R"(
infix + = (_, _) => _; .: I just need it to parse

Human = class {
    name = "";
    age = 0;
    human = 0;
};

me = Human("Ali", 24, Human("Meow", 23, Human("Pie", 3)));

space x {
    name = "haha";
};

another_human = Human();


{x::name, 1 + 5, {name2, another_human.age, {...2 + 2, _}}} = me;


__builtin_print(x::name, another_human.age, 1 + 5, 2 + 2);
)";

    REQUIRE(pie::test::run(src1) == "Ali 23 24 Pie, 3");
}


TEST_CASE("Sub Typing a Member Variable", "[Class][Type]") {
    const auto src1 = R"(
Named = class { name = ""; };
Human = class {
    name = "";
    age = 0;
};


C = class {
    n: Named = Named("named");
};

c = C();
__builtin_print(c.n);
h = Human("Ali", 20);
c.n = h;
__builtin_print(c.n);
c.n.name = "meow";
__builtin_print(h);
)";

    REQUIRE(pie::test::run(src1) == R"(Object {
    name = "named";
}
Object {
    name = "Ali";
}
Object {
    name = "meow";
    age = 20;
})");
}




TEST_CASE("Unpackment", "[Unpack]") {
    const auto src1 = R"(
a = 10;
c: Int = 20;

Human = class {
    name = "";
    age = 0;
    height = 100;
    weight = 50;

    p = () => __builtin_print(name, ":", age);
};


h = Human("Moew", 33, 57, 32);
{z} = h;
__builtin_print(z);

{
    1;

    {n, a, h, w} := h;

    __builtin_print(n, a, h, w);
};

__builtin_print(a, h.age);

{
    {x, y, a} = Human("", 0, 50);
};

__builtin_print(a);
)";

    REQUIRE(pie::test::run(src1) == R"(Moew
Moew 33 57 32
10 33
50)");
}




TEST_CASE("Exprs (improper names) as Members", "[Class]") {
    const auto src1 = R"(
x = 1;
C = class {
    x = 2;

    10 = 20;

    f = () => {
        x = 5;
    };

    p = () => __builtin_print(x, 10);
};


c = C();

__builtin_print(x);
c.p();
c.f();
c.p();
__builtin_print(x);
)";

    REQUIRE(pie::test::run(src1) == R"(1
2 20
5 20
1)");
}



TEST_CASE("Dylib & FFI Calls with Pointers!", "[FFI][DYLIB]") {
    const auto src1 = R"(
V = __builtin_ffi_type_void();
I = __builtin_ffi_type_int();
S = __builtin_ffi_type_struct();

CIF = class {
    __param_types = {};
    __return_type = V;
};

S1 = class {
    x = 0;
    __types = {I};
};

S2 = class {
    a = 0;
    n = S1();
    __types = {I, S};
};

getS2_cif = CIF({}, S2());

dylib = __builtin_dlopen("Tests/dylib");

getS2_handle = __builtin_dlsym(dylib, "getS2");


result = __builtin_ffi_call(getS2_handle, getS2_cif);

__builtin_print(result.a);
__builtin_print(result.n.x);
)";

    REQUIRE(pie::test::run(src1) == "1\n2");


    const auto src2 = R"(
V = __builtin_ffi_type_void();
I = __builtin_ffi_type_int();
S = __builtin_ffi_type_struct();
P = __builtin_ffi_type_pointer();

CIF = class {
    __param_types = {};
    __return_type = V;
};

S1 = class {
    x = 0;
    __types = {I};
};

S2 = class {
    a = 0;
    n = S1();
    __types = {I, S};
};


modify_cif = CIF({S});
modifyS1_cif = CIF({P});

dylib = __builtin_dlopen("Tests/dylib");

modifyS1_handle    = __builtin_dlsym(dylib, "modifyS1"   );
modifyS1Ref_handle = __builtin_dlsym(dylib, "modifyS1Ref");
modifyS1Ptr_handle = __builtin_dlsym(dylib, "modifyS1Ptr");


s = S1(7);
__builtin_print(s.x);
__builtin_ffi_call(modifyS1_handle, modify_cif, s);
__builtin_print(s.x);

s = S1(5);
__builtin_print(s.x);
__builtin_ffi_call(modifyS1Ref_handle, modifyS1_cif, s);
__builtin_print(s.x);

s = S1(3);
__builtin_print(s.x);
__builtin_ffi_call(modifyS1Ptr_handle, modifyS1_cif, s);
__builtin_print(s.x);
)";

    REQUIRE(pie::test::run(src2) == "7\n7\n5\n10\n3\n12");


    const auto src3 = R"(
V = __builtin_ffi_type_void();
C = __builtin_ffi_type_cstring();

CIF = class {
    __param_types = {};
    __return_type = V;
};


getStr_cif = CIF({}, C);

dylib = __builtin_dlopen("Tests/dylib");

getStr_handle = __builtin_dlsym(dylib, "getStr");


s = __builtin_ffi_call(getStr_handle, getStr_cif);

__builtin_print(s);
)";

    REQUIRE(pie::test::run(src3) == R"(Hello!)");


    const auto src4 = R"(
V = __builtin_ffi_type_void();
C = __builtin_ffi_type_cstring();

CIF = class {
    __param_types = {};
    __return_type = V;
};


printText_cif = CIF({C});

dylib = __builtin_dlopen("Tests/dylib");

printText_handle = __builtin_dlsym(dylib, "printText");


__builtin_ffi_call(printText_handle, printText_cif, "Meow :3");
)";

    REQUIRE(pie::test::run(src4) == R"(Meow :3)");
}



TEST_CASE("Walrus Operator with Fold Expression", "[Assign][Fold]") {
    const auto src1 = R"(
infix + = (a, b) => __builtin_add(a, b);

func = (ints: ...) => {
    x := (0 + ints + ...);

    __builtin_print(__builtin_decltype(x));
};

func();
)";

    REQUIRE(pie::test::run(src1) == R"(Int)");


    const auto src2 = R"(
infix + = (a, b) => __builtin_add(a, b);

func = (ints: ...) => {
    x := (0 + ints + ...);

    __builtin_print(__builtin_decltype(x));
};

func(1);
)";

    REQUIRE(pie::test::run(src2) == R"(Any)");
}


TEST_CASE("Walrus Operator (inferred assignment)", "[Assign]") {
    const auto src1 = R"(
x = 1;
y := 1;
__builtin_print(__builtin_decltype(x));
__builtin_print(__builtin_decltype(y));
)";

    REQUIRE(pie::test::run(src1) == R"(Any
Int)");


    const auto src2 = R"(
func = (): union { Int; String; } => "hi";

y := func();

__builtin_print(__builtin_type(y));
__builtin_print(__builtin_decltype(y));
)";

    REQUIRE(pie::test::run(src2) == R"(String
union { Int; String; })");


    const auto src3 = R"(
infix(+) add1 = (a, b): Int => __builtin_add(a, b);
infix(+) add2 = (a, b) => __builtin_add(a, b);

a := 1 add1 2;
b := 1 add2 2;
__builtin_print(__builtin_decltype(a));
__builtin_print(__builtin_decltype(b));
)";

    REQUIRE(pie::test::run(src3) == R"(Int
Any)");
}


TEST_CASE("Assigning to Closures in Operators", "[Operator]") {
    const auto src1 = R"(
((a, b) => __builtin_add(a, b)) = "tf!";
__builtin_add(a, b) = "ok?";

infix + = (a, b) => __builtin_add(a, b);

__builtin_print(5 + 2);
__builtin_print((a, b) => __builtin_add(a, b));
)";

//     REQUIRE(pie::test::run(src1) == R"(ok?
// tf!)");

    REQUIRE_THROWS(pie::test::run(src1));
}



TEST_CASE("Prefix Operators as Parameters", "[Param]") {
    const auto src1 = R"(
prefix(!) not = (x) => __builtin_not(x);
f = (not true) => __builtin_print(not true);
f("meow");
)";

    REQUIRE(pie::test::run(src1) == "meow");
}



TEST_CASE("Dylib & FFI Calls!", "[FFI]") {
    const auto src1 = R"(
V = __builtin_ffi_type_void();

CIF = class {
    __param_types = {};
    __return_type = V;
};

void_cif = CIF();

dylib = __builtin_dlopen("Tests/dylib");

greet_handle = __builtin_dlsym(dylib, "greet");


__builtin_ffi_call(greet_handle, void_cif); .: prints "Hello, World!"
)";

    REQUIRE(pie::test::run(src1) == "Hello, World!");


    const auto src2 = R"(
I = __builtin_ffi_type_int();

CIF = class {
    __param_types = {};
    __return_type = 0;
};

dylib = __builtin_dlopen("Tests/dylib");

add_handle = __builtin_dlsym(dylib, "add");


add_cif = CIF({I, I}, I);

x = __builtin_ffi_call(add_handle, add_cif, 1, 2);
y = __builtin_ffi_call(add_handle, add_cif, 3, 4);

__builtin_print(x, y);
)";

    REQUIRE(pie::test::run(src2) == "3 7");


    const auto src3 = R"(
V = __builtin_ffi_type_void  ();
S = __builtin_ffi_type_struct();
I = __builtin_ffi_type_int   ();

dylib = __builtin_dlopen("Tests/dylib");

printS_handle = __builtin_dlsym(dylib, "printS2");


CIF = class {
    __param_types = {};
    __return_type = V;
};


printS_cif = CIF({S});


S1 = class {
    x = 0;

    __types = {I};
};

S2 = class {
    a = 0;
    n = S1();

    __types = {I, S};
};


__builtin_ffi_call(printS_handle, printS_cif, S2(1, S1(2)));
__builtin_ffi_call(printS_handle, printS_cif, S2(3, S1(4)));
)";

    REQUIRE(pie::test::run(src3) == R"(a: 1, Nested.x: 2
a: 3, Nested.x: 4)");
}



TEST_CASE("Accessing Unqualified Namespace Objects outside of Namesapce (via function call)", "[Space][Var]") {
    const auto src1 = R"(

space x {
    a = 1;

    printA = () => __builtin_print(a);
};

x::printA();
)";

    REQUIRE(pie::test::run(src1) == R"(1)");
}



TEST_CASE("Expansion Expression inside Constructor Call", "[Variadic][Class]") {
    const auto src1 = R"(
C = class {
    a = 0;
    b = 0;
    c = 0;
    d = a;
};


makeC = (args: ...) => C(args...);

c = makeC(1, 2);

__builtin_print(c);

)";

    REQUIRE(pie::test::run(src1) == R"(Object {
    a = 1;
    b = 2;
    c = 0;
    d = 1;
})");
}



TEST_CASE("Invalid Operators Import", "[Operator]") {
    const auto src1 = R"(
space std {
    mixfix(LOW +) if : : else : =
        (cond: Bool, thn, els) =>
            __builtin_conditional(cond, thn, els);
};


use std::if;
)";

    REQUIRE_THROWS(pie::test::run(src1));
}



TEST_CASE("Importing Mixfix With Prefix and Latter Operator Name", "[Operator]") {
    const auto src1 = R"(
space std {
    mixfix(LOW +) if : : else : =
        (cond: Bool, thn, els) =>
            __builtin_conditional(cond, thn, els);
};


use mixfix std::else;

print = __builtin_print;

if (false) 1 else 2;
)";

    REQUIRE_NOTHROW(pie::test::run(src1));

    const auto src2 = R"(
space std {
    mixfix(LOW +) if : : else : =
        (cond: Bool, thn, els) =>
            __builtin_conditional(cond, thn, els);
};


use mixfix std::if;

print = __builtin_print;

if (false) 1 else 2;
)";

    REQUIRE_NOTHROW(pie::test::run(src2));
}




TEST_CASE("Importing Operators Filter By Type", "[Operator]") {
    const auto src1 = R"(
import Tests/module;

use prefix std::-;

std::print(- 1);
)";

    REQUIRE(pie::test::run(src1) == "-1");

    const auto src2 = R"(
import Tests/module;

use infix std::-;

std::print(1 - 1);
)";

    REQUIRE(pie::test::run(src2) == "0");

    const auto src3 = R"(
import Tests/module;

use infix std::-;

std::print(- 1);
)";

    REQUIRE_THROWS(pie::test::run(src3));

    const auto src4 = R"(
import Tests/module;

use prefix std::-;

std::print(1 - 1);
)";

    REQUIRE_THROWS(pie::test::run(src4));
}



TEST_CASE("Importing All Operators", "[Operator]") {
    const auto src1 = R"(
import Tests/module;
use std::;

std::print(- 10);
std::print(1 + 2);
std::print("1" + "2");
std::print(3 - 4);
)";

    REQUIRE(pie::test::run(src1) == "-10\n3\n12\n-1");
}


TEST_CASE("Importing Operators Filter By Symbol", "[Operator]") {
    const auto src1 = R"(
import Tests/module;

use infix std::+;

std::print(1 + 2);
std::print("1" + "2");
)";

    REQUIRE(pie::test::run(src1) == "3\n12");


    const auto src2 = R"(
import Tests/module;
use infix std::+;

std::print(3 - 4);
)";

    REQUIRE_THROWS(pie::test::run(src2));
}



TEST_CASE("Unions in Collection Types", "[Type][List][Map]") {
    const auto src1 = R"(
Number = union { Int; Double; };

m: {Number: Bool} = {1: true, 1.2: false};
l: {Number} = {1, 2.3, 4, 5.6};
)";

    REQUIRE_NOTHROW(pie::test::run(src1));
}



TEST_CASE("Use Directives for Names Introduced in Use Declarations", "[Space][Use][Var]") {
    const auto src1 = R"(
print = __builtin_print;

space x {
    space detail {
        x = 1;
    };

    use detail::x;
};


use space x;

print(x);

)";

    REQUIRE(pie::test::run(src1) == "1");
}



TEST_CASE("Dynamic...Operators", "[Operator][Overload]") {
    const auto src1 = R"(
print = __builtin_print;


infix + = (a, b) => print("Any + Any");

func = () => "" + "";
func();

infix + = (a: String, b: String) => print("String + String");

func();
)";

    REQUIRE(pie::test::run(src1) == R"(Any + Any
String + String)");
}



TEST_CASE("Presistent Operators in Returned/Pulled Closures", "[Operator][Scope]") {
    const auto src1 = R"(
print = __builtin_print;


space lib {
    __C = class { };
    infix + = (a: __C, b: __C) => print("added __C");

    space provide {
        C = __C;
        doWork = (c1: C, c2: C) => c1 + c2;
    };
};


use space lib::provide;

a = C();
b = C();

doWork(a, b);
)";

    REQUIRE(pie::test::run(src1) == "added __C");
}




TEST_CASE("Lexical Scoping", "[Scope]") {
    const auto src1 = R"(
print = __builtin_print;

var = 1;

func = () => {
    inner = () => print(var);
    inner();

    var: Int = 2;

    inner();
    inner;
};


inner = func();
inner();

)";

    REQUIRE(pie::test::run(src1) == "1\n1\n1");
}




TEST_CASE("Namespaces and Imports", "[Space][Import]") {
    const auto src1 = R"(
print = __builtin_print;

space g {
    space sheesh {
        import Tests/module;
    };
};

space wow {
    use space g::sheesh::testing;
};

use wow::x;
x = 10;

print(g::sheesh::testing::x);
print(wow::x);
)";

    REQUIRE(pie::test::run(src1) == R"(10
10)");



    const auto src2 = R"(
print = __builtin_print;

space g {
    space sheesh {
        import Tests/module;
    };
};

space wow {
    use space g::sheesh::testing;
};


space ok {
    use space g::sheesh;
};

g::sheesh::testing::x = 5;

print(g::sheesh::testing::x);
print(wow::x);
print(ok::testing::x);
)";

    REQUIRE(pie::test::run(src2) == R"(5
5
5)");
}



TEST_CASE("Scoped Namespaces", "[Space]") {
    const auto src1 = R"(
{
    space wow {
        x = 1;
    };
};

wow::x;
)";

    REQUIRE_THROWS(pie::test::run(src1));
}


TEST_CASE("Valid vs Invalid use space directive", "[Space]") {
    const auto src1 = R"(
print = __builtin_print;

space interesting {

    space mayo { x = 2; };

    fuck = 1;
};

space wow { use space interesting; };

print(interesting::mayo::x);
print(wow::fuck);
print(wow::mayo::x);
)";

    REQUIRE(pie::test::run(src1) == R"(2
1
2)");


    const auto src2 = R"(
print = __builtin_print;

space interesting { fuck = 1; };

space wow { use space interesting; };

print(mayo::x);
)";

    REQUIRE_THROWS(pie::test::run(src2));
}



TEST_CASE("Names Inside Scopes in Namespaces", "[Var][Space]") {
    const auto src1 = R"(
print = __builtin_print;


space ns {
    infix(+) add = (a, n) => __builtin_add(a, n);

    x = 1;
    y = 2;

    space nested {
        a = 3;
        b = 4;
    };
};

print(ns::nested::a);

)";

    REQUIRE(pie::test::run(src1) == R"(3)");
}



TEST_CASE("Different Prefix/Infix Operators", "[Operator]") {
    const auto src1 = R"(
print = __builtin_print;

prefix - = (a)    => __builtin_neg(a);
infix  - = (a, b) => __builtin_sub(a, b);


print(- - - 2 - - - - - - 5);

)";

    REQUIRE(pie::test::run(src1) == R"(3)");


    const auto src2 = R"(
exfix [ : ] = (a)    => __builtin_print("exfix");
suffix  ] = (a) => __builtin_print("suffix");
)";

    REQUIRE_THROWS(pie::test::run(src2));


    const auto src3 = R"(
print = __builtin_print;

prefix - = (x)    => __builtin_neg(x);
infix  - = (a, b) => __builtin_sub(a, b);

infix < = (a, b) => __builtin_lt(a, b);

exfix | : | = (x) => __builtin_conditional(x < 0, - x, x);


print(| 1 |);
print(| - 1 - 5 |);
)";

    REQUIRE(pie::test::run(src3) == "1\n6");
}



TEST_CASE("Assigned Operators", "[Operator]") {
    const auto src1 = R"(
(infix + = (a, b) => __builtin_add(a, b)) = 5;
__builtin_print(infix + = (a, b) => __builtin_add(a, b));
)";

    REQUIRE(pie::test::run(src1) == R"(5)");


    const auto src2 = R"(
(infix + = (a, b) => __builtin_add(a, b)) = 5;
infix + = (a, b) => __builtin_add(a, b);

1 + 2;
)";

    REQUIRE_THROWS(pie::test::run(src2));
}




TEST_CASE("Scoped Operators", "[Operator]") {
    const auto src1 = R"(
{
    infix + = (a, b) => __builtin_add(a, b);
    __builtin_print(1 + 2);
};
)";

    REQUIRE(pie::test::run(src1) == R"(3)");


    const auto src2 = R"(
{
    infix + = (a, b) => __builtin_add(a, b);
};
1 + 2;
)";

    REQUIRE_THROWS(pie::test::run(src2));
}




TEST_CASE("Shaw's Test", "[Var]") {
    const auto src1 = R"(
print = __builtin_print;

infix + = (a, b) => __builtin_add(a, b);
infix - = (a, b) => __builtin_sub(a, b);

infix > = (a, b) => __builtin_gt(a, b);
infix < = (a, b) => __builtin_lt(a, b);


xact = (f, g, r) => __builtin_conditional(
    g.k,
    {
        g.k = f();
        __builtin_conditional(r > 0, xact(f, g, r - 1), "");
    },
    ""
);

act = (f) => {
  xact(f, class{ k = true; }(), 5);
};

while = (c, f) => {
    act(() => __builtin_conditional(c(), { f(); true; }, false));
};


main = () => {
    x = "meow";
    x = 0;
    while(() => x < 10, () => {
        x = x + 1;
        print(x);
    });
};

main();
)";

    REQUIRE(pie::test::run(src1) == R"(1
2
3
4
5
6)");



    const auto src2 = R"(

print = __builtin_print;

infix + = (a, b) => __builtin_add(a, b);
infix - = (a, b) => __builtin_sub(a, b);

infix > = (a, b) => __builtin_gt(a, b);
infix < = (a, b) => __builtin_lt(a, b);


xact = (f, g, r) => __builtin_conditional(
    g.k,
    {
        g.k = f();
        __builtin_conditional(r > 0, xact(f, g, r - 1), "");
    },
    ""
);

act = (f) => {
  xact(f, class{ k = true; }(), 5);
};

while = (c, f) => {
    act(() => {
        __builtin_conditional(c(), { f(); true; }, false);
    });
};



x = 0;
while(() => x < 10, () => {
    x = x + 1;
    print(x);
});
)";

    REQUIRE(pie::test::run(src2) == R"(1
2
3
4
5
6)");
}





TEST_CASE("Shadowing Loop Variables", "[Loop][Var]") {
    const auto src1 = R"(
loop i : 5 {
    loop i : 3 {
        __builtin_print(i);
    };
    __builtin_print(i);
};
)";

    REQUIRE(pie::test::run(src1) == R"(0
1
2
0
0
1
2
1
0
1
2
2
0
1
2
3
0
1
2
4)");
}


TEST_CASE("Nested Loops", "[Loop]") {
    const auto src1 = R"(
loop i : 5 {
    x = loop e : 3 {
        __builtin_conditional(
            __builtin_eq(e, 2), break "broken", __builtin_print(i)
        );
    };
    __builtin_conditional(
        __builtin_eq(i, 3), continue, 0
    );

    __builtin_print(x);
};
)";

    REQUIRE(pie::test::run(src1) == R"(0
0
broken
1
1
broken
2
2
broken
3
3
4
4
broken)");
}



TEST_CASE("Break 1", "[Loop]") {
    const auto src1 = R"(
r = loop i : 5
    __builtin_conditional(__builtin_eq(i, 2), break "meow", __builtin_print(i));

__builtin_print(r);
)";

    REQUIRE(pie::test::run(src1) == R"(0
1
meow)");
}



TEST_CASE("Continue 1", "[Loop]") {
    const auto src1 = R"(
loop i : 5 __builtin_conditional(__builtin_eq(i, 2), __builtin_print(continue), __builtin_print(i));
)";

    REQUIRE(pie::test::run(src1) == R"(0
1
2
3
4)");


    const auto src2 = R"(
loop i : 5 __builtin_conditional(__builtin_eq(i, 2), continue, __builtin_print(i));
)";

    REQUIRE(pie::test::run(src2) == R"(0
1
3
4)");
}


TEST_CASE("Break/Continue Outside Loop", "[Loop]") {
    const auto src1 = R"(
func1 = (x) => continue;
func2 = (x, y) => __builtin_print(y);

loop e : {1, 2, 3} {
  func2(func1(e), "hey");
  __builtin_print("hi");
};
)";

    REQUIRE_THROWS(pie::test::run(src1));
}



TEST_CASE("Fib 10", "[Func]") {
    const auto src1 = R"(
fib = (n) => __builtin_conditional(
    __builtin_lt(n, 2),
    1,
    __builtin_add(fib(__builtin_sub(n, 1)), fib(__builtin_sub(n, 2)))
);

loop i : 10 __builtin_print(fib(i));
)";

    REQUIRE(pie::test::run(src1) == R"(1
1
2
3
5
8
13
21
34
55)");
}



TEST_CASE("Arguments of Recursive Functions", "[Func][Param][Var]") {
    const auto src1 = R"(
func = (a, b) => __builtin_conditional(
    a,
    __builtin_print(a, b),
    {
        func(true, 100);
        __builtin_print(a, b);
    }
);

func(false, 1);
)";

    REQUIRE(pie::test::run(src1) == R"(true 100
false 1)");
}



TEST_CASE("Transitive Namespace", "[Space][Var]") {
    const auto src1 = R"(
space ns { a = 1; };

space ns { x = a; };

__builtin_print(ns::x);
)";

    REQUIRE(pie::test::run(src1) == "1");
}



TEST_CASE("Namespace Extension", "[Space][Var]") {
    const auto src1 = R"(
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

__builtin_print(ns::a);
__builtin_print(ns::b);
__builtin_print(ns::x);
)";

    REQUIRE(pie::test::run(src1) == "5\n2\n10");
}



TEST_CASE("Namespaces Aliases", "[Space][Var]") {
    const auto src1 = R"(

print = __builtin_print;

space x {
    a = 1;
    space y {
        b = 2;
    };
};

print(x::a);
print(x::y::b);

x::y::b = 5;

use space x;

print(x::y::b);
print(y::b);


y::b = 10;

print(x::y::b);
print(y::b);

use y::b;

print(b);

b = 20;

print(b);
print(x::y::b);
print(y::b);

)";

    REQUIRE(pie::test::run(src1) == R"(1
2
5
5
10
10
10
20
20
20)");
}



TEST_CASE("Namespaces 2", "[Space]") {
    const auto src1 = R"(
print = __builtin_print;
space x { a = 1; };
print(a);
)";

    REQUIRE_THROWS_AS(pie::test::run(src1), pie::except::NameLookup);
}


TEST_CASE("Using Declaration Introduces References", "[Space][var]") {
    const auto src1 = R"(
print = __builtin_print;

space x { a = 1; };

use x::a;

a = 5;
print(a);
print(x::a);

)";

    REQUIRE(pie::test::run(src1) == "5\n5");
}




TEST_CASE("Operator Precedence", "[Operator][Prec]") {
    const auto src1 = R"(
infix(*  -) L1 = (a, b) => 1;
infix(L1 -) L2 = (a, b) => 2;
infix(L2 -) L3 = (a, b) => 3;

infix(+) p = (a, b) => 0;

x = 1 L3 2 p 3;

__builtin_print(x);
)";

    REQUIRE(pie::test::run(src1) == "0");
}



TEST_CASE("Passing Variadic Eager Param Function", "[Func][Param][Variadic]") {
    const auto src1 = R"(
callFunc = (f) => {
    T = Bool;

    f(Double);
};

func = (T, x: ...T) => 0;

callFunc(func);
)";

    REQUIRE_NOTHROW(pie::test::run(src1));
}



TEST_CASE("Returning Variadic Eager Param Function", "[Func][Param][Variadic]") {
    const auto src1 = R"(
getFunc = () => {
    T = Double;

    (a: ...T, T, x: T) => "";
};

func = getFunc();

func(2.1, 5.4, 9.8, Int, 1);

)";

    REQUIRE_NOTHROW(pie::test::run(src1));


    const auto src2 = R"(
getFunc = () => {
    T = Double;

    (a, T, x: ...T) => "";
};

func = getFunc();

func(true, Int, 1, 2, 3);
)";

    REQUIRE_NOTHROW(pie::test::run(src2));
}


TEST_CASE("Returning Eager Param Function", "[Func][Param]") {
    const auto src1 = R"(
getFunc = () => {
    T = Double;

    (a: T, T, x: T) => "";
};

func = getFunc();

func(2.1, Int, 1);
)";

    REQUIRE_NOTHROW(pie::test::run(src1));
}


TEST_CASE("Closure Captures - Blocks vs Non-blocks", "[Func]") {
    const auto src1 = R"(
print = __builtin_print;

f = (a, s) => a();

s = "Hello";
func1 = () => { print(s); };
func2 = () => print(s);

f(func1, 3);
f(func2, 3);
)";

    REQUIRE(pie::test::run(src1) == "Hello\nHello");
}


TEST_CASE("Variadic Function Eager Parameter", "[Func][Param][Variadic]") {
    const auto src1 = R"(
func = (T: Type, args: ...T, w, z) => 1;

func(Int, 1, 2, 3, "meow", 3.14);
func(Bool, true, false, "meow", 3.14);
)";

    REQUIRE_NOTHROW(pie::test::run(src1));


    const auto src2 = R"(
func = (T: Type, args: ...T, w, z) => 1;

func(Int, 1, "", 2, 3);
)";

    REQUIRE_THROWS_AS(pie::test::run(src2), pie::except::TypeMismatch);


    const auto src3 = R"(
func = (T: Type, args: ...T, w, z) => 1;

call = (args: ...Any) => func(Int, args...);

call(1, 2, 3);
)";

    REQUIRE_NOTHROW(pie::test::run(src3));


    const auto src4 = R"(
func = (T: Type, args: ...T, w, z) => 1;

call = (args: ...Any) => func(Int, args...);

call(true, false, 3.14, 2);
)";

    REQUIRE_THROWS_AS(pie::test::run(src4), pie::except::TypeMismatch);
}




TEST_CASE("Union Eager Type Params", "[Param][Union][Func]") {
    const auto src1 = R"(
func = (T, x: T, y: union { T; Int; }) => __builtin_print(T, x, y);
func(Double, 1.2, 3);
func(Int, 1, 3);
)";

    REQUIRE_NOTHROW(pie::test::run(src1));
}


TEST_CASE("Names Across Functions", "[Var][Func]") {
    const auto src1 = R"(

func = () => {
    x = __builtin_add(x, 1);
    std::print(x);
};


f = (a) => {
    x: Any = 10;
    a();
    a();
    a();
};

f(func);

)";

    REQUIRE_THROWS_AS(pie::test::run(src1), pie::except::NameLookup);
}


TEST_CASE("Comments", "") {
    const auto src1 = R"(
.: wuegfiqywefi; x: String = 1; f()
x = 1;
.:: meow
oweuh
random stuff 
ddf .:
hi
:.
:: .
: :.
: : .
::.
x = 2;
)";

    REQUIRE_NOTHROW(pie::test::run(src1));
}



TEST_CASE("Refering to Previous Members in Member Init", "[Class]") {
    const auto src = R"(
print = __builtin_print;


C = class {
    x = print(5);
    y = __builtin_add(x, 20);
};

x = C();
y = C(10);

print(x);
print(y);
)";

    REQUIRE(pie::test::run(src) == R"(5
Object {
    x = 5;
    y = 25;
}
Object {
    x = 10;
    y = 30;
})");
}





TEST_CASE("Implicit Parameter Pack", "[Func][Param][Variadic]") {

    const auto src1 =
R"(
print = __builtin_print;

func: (Int, ...Bool, String): Any = (a, b, c) => print(b);
func(1, true, false, true, true, "what");
)";


    REQUIRE(pie::test::run(src1) == "true, false, true, true");


    const auto src2 =
R"(
print = __builtin_print;

func: (Int, ...Bool, String): Any = (a, b, c) => print(a, b, c);
func(1, false, "what");
)";


    REQUIRE(pie::test::run(src2) == "1 false what");


    const auto src3 =
R"(
print = __builtin_print;

func: (Int, ...Bool, String): Any = (a, b, c) => print(a, b, c);
func(1, 2, 3);
)";


    REQUIRE_THROWS_AS(pie::test::run(src3), pie::except::TypeMismatch);


    const auto src4 =
R"(
print = __builtin_print;

func: (Int, ...Bool, String): Any = (a, b, c) => print(a, b, c);
func(1, true, 3);
)";


    REQUIRE_THROWS_AS(pie::test::run(src4), pie::except::TypeMismatch);
}



TEST_CASE("Double as Object", "[Double][Object]") {

    const auto src1 =
R"(

C = class {
    x = "hi";
};

2 = C();
__builtin_print(2.x);
__builtin_print(2.1);

)";


    REQUIRE(pie::test::run(src1) == "hi\n2.100000");



    const auto src2 =
R"(
C = class {  x = "hi"; };

2.1 = C();
__builtin_print(2.1);
__builtin_print(2.1.x);
)";


    REQUIRE(pie::test::run(src2) ==
R"(Object {
    x = "hi";
}
hi)");
}


TEST_CASE("Invalid Double", "[Double]") {

    const auto src =
R"(
print = __builtin_print;
add = (x: Int, y, a: Int, b, c, d: Int, e: String, f, g, z: Double): Int => 0;

print(add(g = 7, 1, c = 3, 2, 4, 5, f = 6, 1, "", 1.));
)";


    REQUIRE_THROWS(pie::test::run(src));
}



TEST_CASE("Structural Subtyping + Functions", "[Class][Func]") {
    const auto src = R"(
print = __builtin_print;

Named = class { name = "doesn't matter"; };

changeName = (x: Named) => x.name = "bye";

Human = class {
    name = "";
    age = 0;
};

h = Human("hi");
print(h);
changeName(h);
print(h);
)";

    REQUIRE(pie::test::run(src) == R"(Object {
    name = "hi";
    age = 0;
}
Object {
    name = "bye";
    age = 0;
})");
}


TEST_CASE("Shaw's Shinanegans", "[Class]") {
    const auto src = R"(
print = __builtin_print;

A = class {};
B = class {};
f = (x: B) => x;

a = A();
print(__builtin_eq(f(a), a));
)";

    REQUIRE(pie::test::run(src) == "true");
}



TEST_CASE("Structural Subtyping", "[Class]") {
    const auto src = R"(
print = __builtin_print;

Named = class {
    name = "";
};

Human = class {
    name = "John";
    age = 0;
};


h = Human("Ali", 22);

x: Named = h;

print(h);
print(x);

x.name = "meow";

print(h);
print(x);

)";

    REQUIRE(pie::test::run(src) == R"(Object {
    name = "Ali";
    age = 22;
}
Object {
    name = "Ali";
}
Object {
    name = "meow";
    age = 22;
}
Object {
    name = "meow";
})");
}


TEST_CASE("Self Outside Class", "[Class]") {
    const auto src = R"(
f = () => print(self);
C = class {
    x = 1;

    func = () => {
        f();
    };
};
C(10).func();
)";

    REQUIRE_THROWS_AS(pie::test::run(src), pie::except::NameLookup);
}


TEST_CASE("Using Variable Before Definition", "[Var]") {
    const auto src = R"(
func1 = () => print(x);
func2 = () => {
    x = 22;
    func1();
};
func2();

)";

    REQUIRE_THROWS_AS(pie::test::run(src), pie::except::NameLookup);
}



TEST_CASE("Mutual Rec still Valid in Class Definition", "[Class]") {
    const auto src = R"(
    class {
        f1 = () => f2();
        f2 = () => f1();
    };
)";

    REQUIRE_NOTHROW(pie::test::run(src));
}




TEST_CASE("Previous Member in Other Member Init", "[Class]") {
    const auto src = R"(
    class {
        x = 1;
        y = x;
    };
)";

    REQUIRE_NOTHROW(pie::test::run(src));
}


TEST_CASE("Match Invalid Name Case Scope", "[Match][Type][Scope]") {
    const auto src = R"(
print = __builtin_print;

C = class {
    T: Type = Int;
    v = 0;
};


c = C(Bool, "meow");

match c {
    C(type, v: type) => print(type, v);

    C(type) => print("not matched:", v);
};
)";

    REQUIRE_THROWS_AS(pie::test::run(src), pie::except::NameLookup);
}


TEST_CASE("Match Invalid Structure Type Name", "[Match][Type][Scope]") {
    const auto src = R"(
print = __builtin_print;

C = class {
    T: Type = Int;
    v = 0;
};

c = C(Bool, "meow");

match c {
    C(type, v: type) => print(type, v);

    SomeNonexistantTypeName(type) => print("not matched");
};
)";

    REQUIRE_THROWS_AS(pie::test::run(src), pie::except::NameLookup);
}



TEST_CASE("Assignment in Union", "[Type][Union]") {
    const auto src = R"(
U = union {
    (T = Int);
    T;
};
__builtin_print(U);
)";

    REQUIRE(pie::test::run(src) == R"(union { Int; Int; })");
}



TEST_CASE("Type Of", "[Type][Builtin]") {
    const auto src1 = R"(
type = __builtin_type;

f = (x, y: type(x), z: type(y)) => 1;
f("1", "2", "3");


f = (x, y: type(x), z: type(y)) => 2;
f(1, 2, 3);
)";

    REQUIRE_NOTHROW(pie::test::run(src1));

    const auto src2 = R"(
type = __builtin_type;

f = (x, y: type(x), z: type(y)) => 3;
f("1", "2", 3);
)";

    REQUIRE_THROWS(pie::test::run(src2));
}


TEST_CASE("Self 2", "[Class][Var]") {
    const auto src1 = R"(
    self = 5;
    __builtin_print(self);
)";

    REQUIRE(pie::test::run(src1) == R"(5)");


    const auto src2 = R"(
    __builtin_print(self);
)";

    REQUIRE_THROWS(pie::test::run(src2));
}



TEST_CASE("Self", "[Class][Var]") {
    const auto src = R"(
Human = class {
    name = "";
    age = 0;

    printSelf = () => __builtin_print(self);
};

Human("x", 2).printSelf();
)";

    REQUIRE(pie::test::run(src) == R"(Object {
    name = "x";
    age = 2;
    printSelf = (): Any => __builtin_print(self);
})");
}



TEST_CASE("Types as Values 2", "[Func][Type]") {
    const auto src = R"(
:Int = 4;
f = (x: Int): Int => x;
f(5);
)";

    REQUIRE_THROWS(pie::test::run(src));
}

TEST_CASE("Types as Values", "[Func][Type]") {
    const auto src = R"(
:(Int, Double): String = :(Any, Any): Any;
f: (Int, Double): String = (a, b) => 1;
f(4, false);
)";

    REQUIRE_NOTHROW(pie::test::run(src));
}

TEST_CASE("Lazy Function Return Type", "[Func][Param][Type]") {
    const auto src = R"(
f = (T): T => 1;
f(Int);
)";

    REQUIRE_NOTHROW(pie::test::run(src));
}


TEST_CASE("Lazy Function Parameter Types", "[Func][Param][Type]") {
    const auto src = R"(
House1 = class { T = String; };
House2 = class { T = Int; };

X = Int;
func = (h, a: h.T, x: X) => 1;

func(House1(), "1", 1);

X = Bool;
func(House2(), 1, 1);
)";

    REQUIRE_NOTHROW(pie::test::run(src));
}



TEST_CASE("Function Type Checking", "[Func][Type]") {
    const auto src = R"(
func: (Int): Bool = (x: Int, s: String): Bool => true;
)";

    REQUIRE_THROWS(pie::test::run(src));
}


TEST_CASE("Complex Lazy Param Types", "[Func][Param][Type]") {
    const auto src1 = R"(
infix + = (a, b) => __builtin_add(a, b);
infix | = (a, b) => union {a; b;};

func = (1 + 2, x: (1 + 2) | 4) => x;
func(String, "hi");
func(String, 4);
)";

    REQUIRE_NOTHROW(pie::test::run(src1));

    const auto src2 = R"(
infix + = (a, b) => __builtin_add(a, b);
infix | = (a, b) => union {a; b;};

func = (1 + 2, x: (1 + 2) | 4) => x;
func(String, 1 + 2);
)";

    REQUIRE_THROWS(pie::test::run(src2));
}


TEST_CASE("Leaky Argument 2", "[Func][Param][Type]") {
    const auto src1 = R"(
    func = (T, a: T) => __builtin_print(1);
    func2 = func(Type);
    func2(T);
)";

    REQUIRE_THROWS(pie::test::run(src1));
}


TEST_CASE("Testing Leaky Argument", "[Func][Param][Type]") {
    const auto src1 = R"(
func = (T, x: T) => __builtin_print(x);
func(Int, 1);
)";

    REQUIRE(pie::test::run(src1) == "1");


    const auto src2 = R"(
func = (T, x: T) => __builtin_print(x);
func(Int, T);
)";

    REQUIRE_THROWS_WITH(pie::test::run(src2), Catch::Matchers::ContainsSubstring("not found"));
}


TEST_CASE("Concepts 2", "[Type]") {
    const auto src = R"(
MoreThan10 = (x: Int) => __builtin_gt(x, 10);
LessThan20 = (x: Int) => __builtin_lt(x, 20);
func = (x: MoreThan10): LessThan20 => __builtin_add(x, 5);
func("meow");
)";

    REQUIRE_THROWS_AS(pie::test::run(src), pie::except::TypeMismatch);
}


TEST_CASE("Concepts", "[Type]") {
    const auto src1 = R"(
MoreThan10 = (x: Int) => __builtin_gt(x, 10);
LessThan20 = (x: Int) => __builtin_lt(x, 20);
a: MoreThan10 = 15;
func = (x: MoreThan10): LessThan20 => __builtin_add(x, 5);
func(13);
)";

    REQUIRE_NOTHROW(pie::test::run(src1));

    const auto src2 = R"(
MoreThan10 = (x: Int) => __builtin_gt(x, 10);
a: MoreThan10 = 5;
)";

    REQUIRE_THROWS_AS(pie::test::run(src2), pie::except::TypeMismatch);

    const auto src3 = R"(
MoreThan10 = (x: Int) => __builtin_gt(x, 10);
LessThan20 = (x: Int) => __builtin_lt(x, 20);
func = (x: MoreThan10): LessThan20 => __builtin_add(x, 5);
func(17);
)";

    REQUIRE_THROWS_AS(pie::test::run(src3), pie::except::TypeMismatch);
}



TEST_CASE("Types as Values - List of Bool vs {Bool}", "[Type]") {
    const auto src1 = R"(a: {Bool} = {true, false};)";
    REQUIRE_NOTHROW(pie::test::run(src1));

    const auto src2 = R"(v: {Type} = {Bool};)";
    REQUIRE_NOTHROW(pie::test::run(src2));

    const auto src3 = R"(v: Type = {Bool};)";
    REQUIRE_THROWS_AS(pie::test::run(src3), pie::except::TypeMismatch);

    const auto src4 = R"(v: Type = :{Bool};)";
    REQUIRE_NOTHROW(pie::test::run(src4));

    const auto src5 = R"(x: {Bool} = {Bool};)";
    REQUIRE_THROWS_AS(pie::test::run(src5), pie::except::TypeMismatch);
}



TEST_CASE("Expanding Pack in Partial Application", "[Func]") {
    const auto src = R"(
print = __builtin_print;
f = (a, b, c, z) => print(a, b, c, z);
caller = (args: ...Any) => f(10, args...);
new_closure = caller(1, 2);
new_closure(1000);
)";


    REQUIRE(pie::test::run(src) == "10 1 2 1000");
}




TEST_CASE("Values as types w unions", "[Type]") {
    const auto src1 = R"(
print = __builtin_print;

l1 = {1, 2, 3};
l2 = {5, 6};
x: union { l1; l2; } = {5, 6};
print(x);
x = {1, 2,3};
print(x);

y: union { 1; "hi"; Bool; } = 1;
print(y);
y = "hi";
print(y);
y = true;
print(y);
y = false;
print(y);
)";

    REQUIRE(pie::test::run(src1) == R"({5, 6}
{1, 2, 3}
1
hi
true
false)");

    const auto src2 = R"(
l1 = {1, 2, 3};
l2 = {5, 6};
x: union { l1; l2; } = {1, 2, 3, 5, 6};
)";

    REQUIRE_THROWS_AS(pie::test::run(src2), pie::except::TypeMismatch);

    const auto src3 = R"(
y: union { 1; "hi"; Bool; } = "bye";
)";

    REQUIRE_THROWS_AS(pie::test::run(src3), pie::except::TypeMismatch);
}


TEST_CASE("Redefining builtin type, espacially Any", "[Type]") {
    const auto src = R"(
Any = class { };

f = (a) => a;
f("");

f = (a: Any): Any => a;
f(Any());
)";


    REQUIRE_NOTHROW(pie::test::run(src));
}


// TEST_CASE("Class + Object Equality", "[Object][Class]") {
//     const auto src = R"(
// print = __builtin_print;

// x = 1;
// y = 1;
// ns = space {
//     x: Any = 2;
//     y = 2;

//     printX1 = () => print(::x);
//     printX2 = () => print(x);

//     printY1 = () => print(::y);
//     printY2 = () => print(y);
// };

// ns::printX1();
// ns::printX2();

// ns::printY1();
// ns::printY2();
// )";


//     REQUIRE(pie::test::run(src) == R"(1
// 2
// 2
// 2)");
// }


// TEST_CASE("Class + Object Equality", "[Object][Class]") {
//     const auto src = R"(
// print = __builtin_print;

// X = class { a = 1; b = a; };
// Y = class { a = 1; b = 1; };
// print(__builtin_eq(X, Y));
// print(__builtin_eq(X(2), Y(2)));
// print(X(2));
// print(Y(2));
// )";

// // maybe the first shouldn't be true
//     REQUIRE(pie::test::run(src) == R"(true
// true
// Object {
//     a = 2;
//     b = 1;
// }
// Object {
//     a = 2;
//     b = 1;
// })");
// }


TEST_CASE("Class + Object Equality with NaN", "[Object][Class][Type]") {
    const auto src = R"(
print = __builtin_print;

A = class { value = __builtin_div(0.0, 0.0); };
x = __builtin_div(0.0, 0.0);
print(__builtin_eq(A(), A()));
print(__builtin_eq(x, x));
)";

    REQUIRE(pie::test::run(src) == "false\nfalse");
}



TEST_CASE("Class + Object Equality", "[Object][Class][Type]") {
    const auto src = R"(
print = __builtin_print;

B = class { value = __builtin_neg(0.0); };
C = class { value = 0.0; };
print(__builtin_eq(B, C));
print(__builtin_eq(B(), C()));

)";

    REQUIRE(pie::test::run(src) == "false\ntrue");
}


TEST_CASE("Returning Function with Member Variable Capture", "[Var][Func][Class]") {
    const auto src = R"(
print = __builtin_print;

C = class {
    s = "";

    makeSModifier = () => {
        (new_s) => s = new_s;
    };
};

o = C();

func = o.makeSModifier();
func("hmm");
print(o.s);
)";

    REQUIRE(pie::test::run(src) == "hmm");
}


TEST_CASE("Returning Function with Local Variable Capture - Multiple Scopes", "[Var][Func]") {
    const auto src = R"(
print = __builtin_print;

makeFunc = (z) => {
    x: Int = 1;
    {
        x: Int = 2;
        () => print(x);
    };
};

func = makeFunc(100);
func();
)";

    REQUIRE(pie::test::run(src) == "2");
}


TEST_CASE("Returning Function with Local Variable Capture (Eager Params) - Scope", "[Var][Func]") {
    const auto src = R"(
print = __builtin_print;

makeFunc = () => {
    f = (x, T, a: T) => print(T);

    T = Double;
    f(5);
};
func = makeFunc();
func(Int, 10);
)";

    REQUIRE(pie::test::run(src) == R"(Int)");
}


TEST_CASE("Returning Function with Local Variable Capture (Multiple Decls) - Scope", "[Var][Func]") {
    const auto src = R"(
print = __builtin_print;


makeFunc = (z) => {
    print(z);
    z: Any = 999;
    print(z);
    z: String = "what!";
    print(z);
    () => print(z);
};
func = makeFunc(100);
func();
)";

    REQUIRE(pie::test::run(src) == R"(100
999
what!
what!)");
}


TEST_CASE("Returning Function with Local Variable Capture (Declare new var) - Scope", "[Var][Func]") {
    const auto src = R"(
print = __builtin_print;

makeFunc = (z) => {
    {
        z: Any = "Fuck";
        print("z =", z);
    };
    print("z =", z);
    () => print("z =", z);
};
func = makeFunc(100);
func();

)";

    REQUIRE(pie::test::run(src) == R"(z = Fuck
z = 100
z = 100)");
}


TEST_CASE("Returning Function with Local Variable Capture (Reassign) - Scope", "[Var][Func]") {
    const auto src = R"(
print = __builtin_print;

makeFunc = (z) => {
    {
        z = "Fuck";
        print("z =", z);
    };
    print("z =", z);
    () => print("z =", z);
};
func = makeFunc(100);
func();

)";

    REQUIRE(pie::test::run(src) == R"(z = Fuck
z = Fuck
z = Fuck)");
}


TEST_CASE("Returning Function with Local Variable Capture - No Scope", "[Var][Func]") {
    const auto src = R"(
print = __builtin_print;

makeFunc = (z) => () => print(z);

func = makeFunc(100);
func();
)";

    REQUIRE(pie::test::run(src) == "100");
}


TEST_CASE("Returning Function with Local Variable Capture", "[Var][Func]") {
    const auto src = R"(
print = __builtin_print;

makeFunc = (z) => {
    () => print(z);
};

func = makeFunc(100);
func();
)";

    REQUIRE(pie::test::run(src) == "100");
}


TEST_CASE("Reassignment", "[Var][Func]") {
    const auto src = R"(
print = __builtin_print;

makeFunc = (z) => {
    z = 999;
    () => print(z);
};

func = makeFunc(100);
func();
)";

    REQUIRE(pie::test::run(src) == "999");
}


TEST_CASE("Shadowing", "[Var][Func]") {
    const auto src = R"(
print = __builtin_print;

makeFunc = (z) => {
    z: Any = 999;
    z: String = "what!";
    () => print(z);
};

func = makeFunc(100);
func();
)";

    REQUIRE(pie::test::run(src) == "what!");
}


TEST_CASE("Eager Type Parameters For Expanded Calls", "[Type][Func]") {
    const auto src = R"(
func = (T, a: Int, x) => __builtin_print("called with T =", T, "and a =", a, "and x =", x);
call = (args: ...Any) => func(args...);
f = call(Int, 1, "hi");
)";

    REQUIRE(pie::test::run(src) == "called with T = Int and a = 1 and x = hi");
}


TEST_CASE("Eager Type Parameters For Partial Application", "[Type][Func]") {
    const auto src = R"(
T = Type;
func = (t: T, T: t, a: T, T) => T;
f = func(Type, Int);
f(1, String);
)";

    REQUIRE_NOTHROW(pie::test::run(src));
}


TEST_CASE("Eager Type Parameters 2", "[Type]") {
    const auto src = R"(
func = (T, a: T) => T;

func(Int, 1);
)";

    REQUIRE_NOTHROW(pie::test::run(src));
}


TEST_CASE("Multiple Named Arguments", "[Func]") {
    const auto src = R"(
func = (T, T: T) => T;

func(T=Type, T=Int);
)";

    REQUIRE_THROWS(pie::test::run(src));
}


TEST_CASE("Invalid Eager Type Parameters", "[Type]") {
    const auto src = R"(
func = (T, T: T) => T;

func(Type, T=Int);
)";

    REQUIRE_THROWS(pie::test::run(src));
}

TEST_CASE("Eager Type Parameters", "[Type]") {
    const auto src = R"(
func = (T, T: T) => T;

func(Type, Int);
)";

    REQUIRE_NOTHROW(pie::test::run(src));
}


TEST_CASE("Type Alias 3", "[Type]") {
    const auto src = R"(
print = __builtin_print;

Func = :(Int, Int, Double): Any;

f: Func = (a: Int, c: Any, b: String): String => "hi";
)";

    REQUIRE_THROWS(pie::test::run(src));
}


TEST_CASE("Type Alias 2", "[Type]") {
    const auto src = R"(
print = __builtin_print;

Func = :(Int, Int, String): Any;

f: Func = (a: Int, c: Any, b: String): String => "hi";

print(Func);
)";

    REQUIRE(pie::test::run(src) == R"((Int, Int, String): Any)");
}


// do I want this behaviour to change?
TEST_CASE("Eager Function Parameter Type Evaluation", "[Type]") {
    const auto src = R"(
print = __builtin_print;


d: Type = Double;

func = (x: d) => print(x);

d = Int;

func(1.2);
)";

    REQUIRE(pie::test::run(src) == R"(1.200000)");
}


TEST_CASE("Type Alias", "[Type]") {
    const auto src = R"(
d: Type = Double;
i = Int;
x: i = 1;
i = d;
y: i = 1.2;
)";

    REQUIRE_NOTHROW(pie::test::run(src));
}


TEST_CASE("Invalid Type Alias", "[Type]") {
    const auto src = R"(
d = Int;
x: d = 1.2;
)";

    REQUIRE_THROWS(pie::test::run(src));
    REQUIRE_THROWS_MATCHES(pie::test::run(src), pie::except::TypeMismatch, Catch::Matchers::MessageMatches(Catch::Matchers::ContainsSubstring("Type mis-match!")));
}


TEST_CASE("Lists vs Maps 2", "[List][Map]") {
    const auto src = R"(
print = __builtin_print;


.: m: {Any: Any} = {"one": 2, class {}: 3, 4: "five", 0: () => 1, () => 0: 1};
.: print(m);

m: {Any: Any} = {"one": 2};
print(m);

m: {Any: Any} = {class {}: 3};
print(m);

m: {Any: Any} = {4: "five"};
print(m);

m: {Any: Any} = {0: () => 1};
print(m);

m: {Any: Any} = {() => 0: 1};
print(m);


name1 = 1;
name3 = 3;
name4 = 4;

res = { name1: Int = name3; }; 
print(res);

res = { name1: String = "hello" }; 
print(res);

res = { (name1: Int = name3): name4 }; .: { expr: expr }; MAP
print(res);

res = { class { }; };
print(res);

res = { class { func = (): Int => 1; }; };
print(res);

res = { class { func = (): Int => 1; } };
print(res);

res = { class { func = (): Int => 1; }: 1 };
print(res);
)";

    REQUIRE(pie::test::run(src) == R"({one: 2}
{class { }: 3}
{4: five}
{0: (): Any => 1}
{(): Any => 0: 1}
3
{1: hello}
{3: 4}
class { }
class {
    func: Any = (): Int => 1;
}
{class {
    func: Any = (): Int => 1;
}}
{class {
    func: Any = (): Int => 1;
}: 1})");
}



TEST_CASE("Lists vs Maps", "[List][Map]") {
    const auto src = R"(
print = __builtin_print;
name1 = 1;
name3 = 3;
name4 = 4;

scope =  {    name1: Int = name3; };
map1  =  {    name1: String = name3  };
map2  =  {    name1: String: Int = name3  };
list  =  { 1, name1: Int = name3  };

print(scope);
print(map1);
print(map2);
print(list);
)";

    REQUIRE(pie::test::run(src) == R"(3
{1: 3}
{1: 3}
{1, 3})");
}


TEST_CASE("Single Element List", "[List]") {
    const auto src = R"(
print = __builtin_print;

list1 = {1};
list2: {Int} = {2};
list3: {Any} = {3};

print(list1);
print(list2);
print(list3);
)";

    REQUIRE(pie::test::run(src) == R"({1}
{2}
{3})");
}

TEST_CASE("Empty List", "[List]") {
    const auto src = R"(
print = __builtin_print;

list1 = {};
list2: {Int} = {};
list3: {Any} = {};

print(list1);
print(list2);
print(list3);
)";

    REQUIRE(pie::test::run(src) == R"({}
{}
{})");
}

TEST_CASE("Looping Over List", "[List][Loop]") {
    const auto src = R"(
print = __builtin_print;

list = {1, "Hi", 3.14, true, class {}};


loop e : list print(e);
)";

    REQUIRE(pie::test::run(src) == R"(1
Hi
3.140000
true
class { })");
}


TEST_CASE("List Types", "[Type]") {
    auto Any  = type::builtins::Any();
    auto Int  = type::builtins::Int();
    auto Int_list = std::make_shared<type::ListType>(type::builtins::Int());
    auto Any_list = std::make_shared<type::ListType>(type::builtins::Any());


    REQUIRE_FALSE(*Int_list >  *Int);
    REQUIRE_FALSE(*Int_list >= *Int);

    REQUIRE(*Any_list >  *Int_list);
    REQUIRE(*Any_list >= *Int_list);

    REQUIRE_FALSE(*Any_list >  *Any);
    REQUIRE_FALSE(*Any_list >= *Any);

    REQUIRE(*Any >  *Any_list);
    REQUIRE(*Any >= *Any_list);

    REQUIRE_FALSE(*Int_list == *Int);
    REQUIRE_FALSE(*Int == *Int_list);

    REQUIRE_FALSE(*Int >  *Int_list);
    REQUIRE_FALSE(*Int >= *Int_list);
}



//* Unwanted Behaviour Anyway
// TEST_CASE("See Through Value Mutability", "[Mutability]") {
//     const auto src = R"(
// print = __builtin_print;

// infix + = (a, b) => __builtin_add(a, b);


// x = 1;

// old = x("Hello 1");
// x = "Hello x"; .: different

// print(old);
// print(1);
// print(x);

// (6 + 7)("Hi");
// 6 + 7 = "Bye";

// print(7 + 6); .: 13
// print(13); .: Hi
// print(6 + 7); .: Bye
// )";

//     REQUIRE(pie::test::run(src) == R"(1
// Hello 1
// Hello x
// 13
// Hi
// Bye)");
// }



TEST_CASE("Looping Over Pack Without Loop Var", "[Loop]") {
    const auto src = R"(
func = (pack: ...Any) => loop pack __builtin_print(0);

func(1, "Hi", 3.14);
)";

    REQUIRE(pie::test::run(src) == R"(0
0
0)");
}


TEST_CASE("Looping Over Pack", "[Loop]") {
    const auto src = R"(
func = (pack: ...Any) => loop e : pack __builtin_print(e);

func(1, "Hi", 3.14);
)";

    REQUIRE(pie::test::run(src) == R"(1
Hi
3.140000)");
}


TEST_CASE("Looping Over Range Without Loop Var", "[Loop]") {
    const auto src = R"(
loop 5 __builtin_print("fuck");
)";

    REQUIRE(pie::test::run(src) == R"(fuck
fuck
fuck
fuck
fuck)");
}


TEST_CASE("Looping Over Range", "[Loop]") {
    const auto src = R"(
loop i : 5 __builtin_print(i);
)";

    REQUIRE(pie::test::run(src) == R"(0
1
2
3
4)");
}


TEST_CASE("Iterator", "[Loop][Self Mutation]") {
    const auto src = R"(
Iota: Type = class {
    limit = __builtin_neg(1);

    i = 0;

    hasNext = (): Bool => __builtin_not(__builtin_eq(i, limit));
    next = () => {
        x = i;
        i = __builtin_add(i, 1);
        x;
    };
};

loop num: Iota(10) __builtin_print(num);
)";

    REQUIRE(pie::test::run(src) == R"(0
1
2
3
4
5
6
7
8
9)");
}


TEST_CASE("Empty Pack", "[General]") {
    const auto src = R"(
print = __builtin_print;

printPack = (first, rest: ...Any) => __builtin_conditional(
    __builtin_eq(
        __builtin_len(rest), 0),
            print(first),
            {
                print(first);
                printPack(rest...);
            }
    );

printPack("Ali", "Ben", "Con", "Byte");
)";

    REQUIRE(pie::test::run(src) == R"(Ali
Ben
Con
Byte)");
}



TEST_CASE("linked lists", "[General]") {
    const auto src = R"(
print = __builtin_print;


Node: Type = class {
    val = "None";
    next = "None";
};

printList = (l: Node) => match l {
    Node(n, :Node) => {
        print(n);
        printList(l.next);
    };

    Node(n: Int, ="None") => print(n);
    Node(_, ="None") => print("Empty List!");
};

list = Node(1, Node(2, Node(3)));

printList(Node());
printList(list);
)";

    REQUIRE(pie::test::run(src) == R"(Empty List!
1
2
3)");
}


TEST_CASE("unary separated left fold", "[Fold]") {
    const auto src = R"(
print = __builtin_print;
infix - = (a: Int, b: Int) => __builtin_sub(a, b);

func = (args: ...Any) => (args - ... - 10); .: (1 - 10 - 2 - 10 - 3 - 10 - 4)
print(func(1, 2, 3, 4));
)";

    REQUIRE(pie::test::run(src) == R"(-38)");
}


TEST_CASE("unary separated right fold", "[Fold]") {
    const auto src = R"(
print = __builtin_print;
infix - = (a: Int, b: Int) => __builtin_sub(a, b);

func = (args: ...Any) => (10 - ... - args); .: (1 - (10 - (2 - (10 - (3 - (4 - 10))))))
print(func(1, 2, 3, 4));
)";

    REQUIRE(pie::test::run(src) == R"(-8)");
}


TEST_CASE("binary separated left fold", "[Fold]") {
    const auto src = R"(
print = __builtin_print;
infix + = (a: Int, b: Int) => __builtin_add(a, b);
infix + = (a: String, b: String) => __builtin_concat(a, b);


greet = (greetings: String, names: ...String, delim: String) => greetings + ("Teach" + names + ... + delim) + "!";
print(greet("Hello ", "Ali", "Ben", "Byt", ", "));
)";

    REQUIRE(pie::test::run(src) == R"(Hello Teach, Ali, Ben, Byt!)");
}


TEST_CASE("binary left fold", "[Fold]") {
    const auto src = R"(
print = __builtin_print;
infix - = (a: Int, b: Int) => __builtin_sub(a, b);

func = (args: ...Any) => (10 - args - ...); .: ((((10 - 1) - 2) - 3) - 4)

print(func(1, 2, 3, 4));
)";

    REQUIRE(pie::test::run(src) == R"(0)");
}



TEST_CASE("unary right fold", "[Fold]") {
    const auto src = R"(
print = __builtin_print;
infix - = (a: Int, b: Int) => __builtin_sub(a, b);

func = (args: ...Any) => (... - args); .: (1 - (2 - (3 - 4)))

print(func(1, 2, 3, 4));
)";

    REQUIRE(pie::test::run(src) == R"(-2)");
}



TEST_CASE("binary right fold", "[Fold]") {
    const auto src = R"(
print = __builtin_print;
infix - = (a: Int, b: Int) => __builtin_sub(a, b);

func = (args: ...Any) => (... - args - 10); .: (1 - (2 - (3 - (4 - 10))))

print(func(1, 2, 3, 4));
)";

    REQUIRE(pie::test::run(src) == R"(8)");
}


TEST_CASE("left fold", "[Fold]") {
    const auto src = R"(
print = __builtin_print;
infix - = (a: Int, b: Int) => __builtin_sub(a, b);

func = (args: ...Any) => (args - ...); .: (((1 - 2) - 3) - 4)

print(func(1, 2, 3, 4));
)";

    REQUIRE(pie::test::run(src) == R"(-8)");
}



TEST_CASE("literals", "[Assingment]") {
    const auto src = R"(
print = __builtin_print;
1 = "hi";
true = 5;
print(1);
print(true);
)";

    REQUIRE(pie::test::run(src) == R"(hi
5)");
}



TEST_CASE("Arbitrary function", "[Params]") {
    const auto src = R"(
print = __builtin_print;
infix(+) + = (a, b) => __builtin_add(a, b);

x: Int = 5;

func = (x: String, x + 2, "hi") => {
    print(x);
    print(x + 2);
    print("hi");
};

func("meow", "hehe", "bye");
)";

    REQUIRE(pie::test::run(src) == R"(meow
hehe
bye)");
}



TEST_CASE("Operator Overloading", "[Overload]") {
    const auto src1 = R"(
print = __builtin_print;

mixfix(HIGH -) if : : else : = (cond: Bool  , thn, els) => 1;
mixfix(HIGH -) if : : else : = (cond: Int   , thn, els) => 2;
mixfix(HIGH -) if::else: = (cond: String, thn, els) => 3;

print(if (true) { 1; } else { 2; });
print(if (0) { 1; } else { 2; });
print(if ("") { 1; } else { 2; });

)";

    REQUIRE(pie::test::run(src1) == R"(1
2
3)");

    const auto src2 = R"(
print = __builtin_print;

infix + = (a: Int, b: Int) => __builtin_add(a, b); 
infix + = (a: String, b: String) => __builtin_concat(a, b); 

print(1 + 2);
print("1" + "2");
)";

    REQUIRE(pie::test::run(src2) == R"(3
12)");

}



TEST_CASE("nested unions", "[Union]") {
    const auto src = R"(
print = __builtin_print;

Class = class {
    x: Int = 0;
    s: String = "";
};

Num = union { Int; Double; };
Union = union { Num; Class; };

u: Union = 1;
print(u);
u: Union = 1.4;
print(u);
u = Class(10, "hi");
print(u);
WithStr = union { Union; String; };
u: WithStr = "a string";
print(u);

print(Union);
)";

    REQUIRE(pie::test::run(src) == R"(1
1.400000
Object {
    x = 10;
    s = "hi";
}
a string
union { union { Int; Double; }; class {
        x: Int = 0;
        s: String = "";
    }; })");

}


TEST_CASE("operator overloading", "[Type]") {
    const auto src = R"(
print = __builtin_print;

cls = class { woof: String = ""; };

infix(+) + = (a: cls, b: cls) => 1;
infix(+) + = (a: Int, b: Int) => 2;

a = cls();
b: cls = cls();
print(cls() + cls());
print(a + b);
print(1 + 1);
)";

    REQUIRE(pie::test::run(src) == "1\n1\n2");
}


// TEST_CASE("recursive namespaces", "[Space]") {
//     const auto src = R"(
// print = __builtin_print;

// x = space {
//     a = 1;
//     y = 1;
// };

// x::y = x;

// print(x::y::y::y::y::a);

// )";

//     REQUIRE(pie::test::run(src) == "1");
// }



// TEST_CASE("namespaces2", "[Space]") {
//     const auto src = R"(
// print = __builtin_print;

// x = space {
//     cls = class { meow: Int = 0; };

//     v: String = "hi";

//     o: cls = cls(1);

//     func = (e: cls) => e.meow;

//     infix(+) + = (a: cls, b: cls) => __builtin_add(a.meow, b.meow);

//     y = space {
//         n = 1;
//     };
// };


// a = x::y::n;
// print(a);
// print(x::y::n);
// x::y::n = 10;
// print(a);
// print(x::y::n);
// a = 20;
// print(a);
// print(x::y::n);

// )";

//     REQUIRE(pie::test::run(src) == "1\n1\n1\n10\n20\n10");
// }



TEST_CASE("namespaces1", "[Space]") {
    const auto src = R"(
print = __builtin_print;

space x {
    cls = class { meow: Int = 0; };

    v: String = "hi";

    o: cls = cls(1);

    func = (e: cls) => e.meow;

    infix(+) + = (a: cls, b: cls) => __builtin_add(a.meow, b.meow);

    space y {
        n = 1;
    };
};


v = x :: v;
print(v);
print(x::v);

x::v = "hello";
print(v);
print(x::v);

v = "bye";
print(v);
print(x::v);

)";

    REQUIRE(pie::test::run(src) == "hi\nhi\nhi\nhello\nbye\nhello");
}





TEST_CASE("Trees", "[Match]") {
    const auto src = R"(
print = __builtin_print;
Nil = "";
Node = class {
    v = 0;
    l = Nil;
    r = Nil;
};
Leaf = class { v = 0; };


test = (x) => match x {
    Leaf(k) & __builtin_geq(k, 0) => 1;
    Leaf(k) & __builtin_leq(k, 0) => 2;
    Leaf(k) & __builtin_eq (k, 0) => 3;
    Node(k, Leaf(_), rChild) & __builtin_geq(k, 0) => 4;
    Node(k, _, _) => 5;
};


x = Leaf(10);
result = test(x);
print(result);

x = Leaf(0);
result = test(x);
print(result);

x = Node(10, Leaf(20), Node(5, Leaf(10), Leaf(10)));
result = test(x);
print(result);

x = Node(10, Node(5, Leaf(10), Leaf(10)), Leaf(20));
result = test(x);
print(result);

x = Node(__builtin_neg(5), Leaf(__builtin_neg(5)), Node(5, Leaf(10), Leaf(10)));
result = test(x);
print(result);
)";


    REQUIRE(pie::test::run(src) == "1\n1\n4\n5\n5");

}



TEST_CASE("Class member", "[Type]") {
    const auto src = R"(
wow = class { inner = class { hmm = 1; }; };
w = wow();
a: w.inner = w.inner();
)";

    REQUIRE_NOTHROW(pie::test::run(src));
}



TEST_CASE("Var args4", "[Variadic]") {
    const auto src = R"(
print = __builtin_print;
f = (a, b, c, es: ...String, x, y, z) => {
    print(a);
    print(b);
    print(c);
    print(es);
    print(x);
    print(y);
    print(z);
};

print(f(4, 2, 0, "Hi", "there", 1, 0, 1));
)";


    REQUIRE(pie::test::run(src) == "4\n2\n0\nHi, there\n1\n0\n1\n1");
}



TEST_CASE("Infix operators", "[Parsing]") {
    const auto src =
R"(
print = __builtin_print;
infix(+) + = (a, b) => __builtin_add(a, b);
infix(+) - = (a, b) => __builtin_sub(a, b);
x = 1 + 2 + 3 + 4;
y = 4 - 3 - 2 - 1;
print(x);
print(y);
)";

    REQUIRE(pie::test::run(src) == "10\n-2");
}


TEST_CASE("Var args3", "[Variadic]") {
    const auto src =
R"(
print = __builtin_print;

func1 = (a: String, args: ...Any, b: String): ...Int => args;

x: ...Int = func1("first", "last");
y: ...Int = func1("first", 1, 2, 3, "last");
print(x);
print(x...);
)";

    REQUIRE(pie::test::run(src) == ""); // not sure why it's not "\n\n" :)! I think Catch2 strips it
}


TEST_CASE("Var args2", "[Variadic]") {
    const auto src =
R"(
print = __builtin_print;

(es) => 1;
f = (es: ...Any) => 1;
f();

f = (x, es: ...Any) => 1;
f();

f = (x, y, es: ...Any) => 1;
f();

f = (es: ...Any, a) => 1;
f();

f = (es: ...Any, a, b) => 1;
f();

f = (a, es: ...Any, b) => 1;
f();

)";

    REQUIRE_NOTHROW(pie::test::run(src));
}


TEST_CASE("Var args", "[Variadic]") {
    const auto src =
R"(
print = __builtin_print;

func2 = (x, y, z, a) => {
    print("x = ", x);
    print("y = ", y);
    print("z = ", z);
    print("a = ", a);

    "done";
};

out = (As: ...Any) => {
    func = (a, b, c, args: ...Any) => func2(a = 300, As..., args...);

    func(1, 2, 3, 5);
};

out(10, 20);
)";



    REQUIRE(
        pie::test::run(src)
        == R"(x =  10
y =  20
z =  5
a =  300)"
    );
}



TEST_CASE("Any vs Func", "[Type]") {
    auto Any  = type::builtins::Any();

    auto func = std::make_shared<type::FuncType>( // functions taking 2 ints and returns an int
        std::vector{type::builtins::Int(), type::builtins::Int()}, type::builtins::Int()
    );


    REQUIRE(*Any  >  *func);
    REQUIRE(*Any  >= *func);

    REQUIRE_FALSE(*func >  *Any);
    REQUIRE_FALSE(*func >= *Any);
}


TEST_CASE("Variadics", "[Type]") {
    auto Any  = type::builtins::Any();
    auto Int  = type::builtins::Int();
    auto Int_var = std::make_shared<type::VariadicType>(type::builtins::Int());
    auto Any_var = std::make_shared<type::VariadicType>(type::builtins::Any());


    REQUIRE(*Int_var >  *Int);
    REQUIRE(*Int_var >= *Int);

    REQUIRE(*Any_var >  *Int_var);
    REQUIRE(*Any_var >= *Int_var);

    REQUIRE(*Any_var >  *Any);
    REQUIRE(*Any_var >= *Any);

    REQUIRE(*Any >  *Any_var);
    REQUIRE(*Any >= *Any_var);

    REQUIRE_FALSE(*Int_var == *Int);
    REQUIRE_FALSE(*Int == *Int_var);

    REQUIRE_FALSE(*Int >  *Int_var);
    REQUIRE_FALSE(*Int >= *Int_var);
}



TEST_CASE("Invalid Named Arguments", "[Params]") {
    const auto src1 =
R"(
print = __builtin_print;
add = (a, z: Double): Int => 0;

add(h = 1);
)";

    const auto src2 =
R"(
print = __builtin_print;
add = (a, z: Double): Int => 0;

add(z = 1);
)";


    REQUIRE_THROWS_MATCHES(pie::test::run(src1), std::runtime_error, Catch::Matchers::MessageMatches(Catch::Matchers::ContainsSubstring("Named argument")));
    REQUIRE_THROWS_MATCHES(pie::test::run(src2), pie::except::TypeMismatch, Catch::Matchers::MessageMatches(Catch::Matchers::ContainsSubstring("Type mis-match!")));
}


TEST_CASE("Named Arguments", "[Params]") {

    const auto src1 =
R"(
print = __builtin_print;
add = (x: Int, y, a: Int, b, c, d: Int, e: String, f, g, z: Double): Int => 0;

print(add(g = 7, 1, c = 3, 2, 4, 5, f = 6, 1, "", 1.0));
)";


    REQUIRE(pie::test::run(src1) == "0");
}

