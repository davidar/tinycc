#include <stddef.h>

void *malloc(size_t size) {
    static size_t p = 1 << 16;
    size_t r = p;
    p += (size + 7) & -8;
    return (void *) r;
}

void *calloc(size_t nmemb, size_t size) {
    return malloc(nmemb * size);
}

void free(void *ptr) {}
