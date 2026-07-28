#include <print>



extern "C" {

    void greet() {
        std::println("Hello, World!");
    }


    int add(int a, int b) {
        return a + b;
    }


    struct S1 {
        int x;
    };

    struct S2 {
        int a;
        S1 n;
    };

    void printS1(S1 s) {
        std::println("a: {}", s.x);
    }

    void printS2(S2 s) {
        std::println("a: {}, Nested.x: {}", s.a, s.n.x);
    }


    S2 getS2() {
        return {1, {2}};
    }

    void modifyS1(S1 s) {
        s.x = 9;
    }

    void modifyS1Ref(S1& s) {
        s.x = 10;
    }

    void modifyS1Ptr(S1 *s) {
        s->x = 12;
    }


    const char* getStr() { return "Hello!"; }

    void printText(const char *s) {
        std::println("{}", s);
    }
}
