#include <wasi.h>

struct s {
    int i;
    float f;
};

struct s f(long long i) {
    struct s s;
    s.i = i;
    s.f = 42;
    s.f += .5;
    s.f += i;
    switch(i) {
        case 1: goto label1;
        case 2: goto label2;
        default: puts("baz\n");
    }
    return s;
    puts("Fail\n");
label1:
    puts("foo1\n");
    puts("foo2\n");
label2:
    puts("bar\n");
    return s;
}

int main(void) {
    int (*print)(const char *s);
    print = &puts;
    for (int i = 0; i < 5; i++) {
        if (!i) puts("Begin\n");
        print((i < 2) ? "Hello, world!\n" : "...\n");
        if ((int) f(i).f != 42 + i)
            puts("Fail\n");
    }
    do {
        puts("Pass\n");
        continue;
        puts("Fail\n");
    } while (0);
    return 0;
}
