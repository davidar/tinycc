#include <wasi.h>

void f(int i) {
    switch(i) {
        case 1: puts("foo");
        case 2: puts("bar"); break;
        default: puts("baz");
    }
    puts("\n\n");
}

int main(void) {
    for (int i = 0; i < 5; i++) {
        puts("Hello, world!\n");
        f(i);
    }
    do {
        puts("Pass\n");
        continue;
        puts("Fail\n");
    } while (0);
    return 0;
}
