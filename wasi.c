#include <wasi.h>

void *alloca(size_t size) {
    return NULL;
}

size_t strlen(const char *s) {
    size_t i = 0;
    while (s[i]) i = i + 1;
    return i;
}

int puts(const char *s) {
    __wasi_ciovec_t iov;
    __wasi_size_t nwritten;
    __wasi_errno_t err;
    iov.buf = s;
    iov.buf_len = strlen(s);
    err = __wasi_fd_write(1, &iov, 1, &nwritten);
    return nwritten;
}

char *memcpy(char *dest, const char *src, size_t n) {
    for (int i = 0; i < n; i++)
        dest[i] = src[i];
    return dest;
}

void *memmove(void *dest, const void *src, size_t n) {
    if (dest != src) memcpy(dest, src, n);
    return dest;
}

void _start(void) {
    main();
}
