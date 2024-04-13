#!/usr/local/bin/tcc -run -nostdlib -nostdinc -I../dietlibc -I../dietlibc/include

#include "../dietlibc/lib/__lltostr.c"
#include "../dietlibc/lib/__ltostr.c"
#include "../dietlibc/lib/__dtostr.c"
#include "../dietlibc/lib/__v_printf.c"
#include "../dietlibc/lib/memmove.c"
#include "../dietlibc/lib/memset.c"
#include "../dietlibc/lib/__isinf.c"
#include "../dietlibc/lib/__isnan.c"
#include "../dietlibc/lib/strtoul.c"
#include "../dietlibc/lib/strtol.c"
#include "../dietlibc/lib/strlen.c"
#include "../dietlibc/lib/strchr.c"
#include "../dietlibc/lib/strcpy.c"
#include "../dietlibc/lib/isspace.c"
#include "../dietlibc/lib/isxdigit.c"
#include "../dietlibc/lib/isalnum.c"
#include "../dietlibc/lib/errno_location.c"

#include "../dietlibc/libstdio/vprintf.c"
#include "../dietlibc/libstdio/printf.c"

#include "lib/libtcc1.c"

#include "syscall.h"

long write(int fd, const void* buf, long unsigned cnt) {
	return __syscall3(1, fd, (long) buf, cnt);
}

int main()
{
    printf("Hello World\n");
    return 0;
}

int _start() {
    return main();
}
