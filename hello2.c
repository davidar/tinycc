#!/usr/local/bin/tcc -run -nostdlib -nostdinc -I/src/dietlibc -I/src/dietlibc/include -I/usr -I/usr/include

// dietlibc
#include "lib/__lltostr.c"
#include "lib/__ltostr.c"
#include "lib/__dtostr.c"
#include "lib/__v_printf.c"
#include "lib/memmove.c"
#include "lib/memset.c"
#include "lib/__isinf.c"
#include "lib/__isnan.c"
#include "lib/strtoul.c"
#include "lib/strtol.c"
#include "lib/strlen.c"
#include "lib/strchr.c"
#include "lib/strcpy.c"
#include "lib/isspace.c"
#include "lib/isxdigit.c"
#include "lib/isalnum.c"
#include "lib/errno_location.c"
#include "libstdio/vprintf.c"
#include "libstdio/printf.c"

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
