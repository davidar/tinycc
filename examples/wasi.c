#include <wasi.h>

int f(int i) {
    switch(i) {
        case 1: puts("foo");
        case 2: puts("bar"); break;
        default: puts("baz");
    }
    puts("\n\n");
    return 42;
    puts("Fail\n");
}

int main(void) {
    int (*print)(const char *s);
    print = &puts;
    for (int i = 0; i < 5; i++) {
        if (!i) puts("Begin\n");
        print((i < 2) ? "Hello, world!\n" : "...\n");
        if (f(i) != 42)
            puts("Fail\n");
    }
    do {
        puts("Pass\n");
        continue;
        puts("Fail\n");
    } while (0);
    return 0;
}
