#include <wasi.h>
#include <stdarg.h>

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
    if (i == 3) i = 2;
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

void puts_many(int count, const char *s, ...) {
    va_list args;
    va_start(args, s);
    for (int i = 0; i < count; i++) {
        puts(s);
        puts(va_arg(args, const char *));
    }
    va_end(args);
}

int (*print)(const char *s) = puts;
const char *hello = "Hello, world!\n";

int main(void) {
    for (int i = 0; i < 5; i++) {
        if (!i) puts("Begin\n");
        print((i < 2) ? hello : "...\n");
        if ((int) f(i).f != 42 + i)
            puts("Fail\n");
    }
    do {
        puts("Pass\n");
        continue;
        puts("Fail\n");
    } while (0);
    puts_many(2, "foo: ", "bar\n", "baz!\n");
    return 0;
}
