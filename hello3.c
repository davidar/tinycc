#!/usr/local/bin/tcc -run -nostdlib -nostdinc -I/src/musl/src/include -I/src/musl/include -I/src/musl/obj/include -I/src/musl/src/internal -I/src/musl/arch/generic -I/src/musl/arch/x86_64 -I/usr
//#!/usr/local/bin/tcc -run -nostdlib -nostdinc -I../musl/src/include -I../musl/include -I../musl/obj/include -I../musl/src/internal -I../musl/arch/generic -I../musl/arch/x86_64

#include <stdio.h>
#include <stdlib.h>

#include "lib/va_list.c"

int main() {
    printf("Hello World\n");
    return 0;
}

int _start() {
    return main();
}
