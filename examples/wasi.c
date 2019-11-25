#include <wasi.h>

int main(void) {
    for (int i = 0; i < 5; i++) {
        puts("Hello, world!\n");
    }
    return 0;
}
