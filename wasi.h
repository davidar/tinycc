#ifndef _WASI_H
#define _WASI_H

#include <stddef.h>
#include <stdint.h>

/**
 * Error codes returned by functions.
 * Not all of these error codes are returned by the functions provided by this
 * API; some are used in higher-level library layers, and others are provided
 * merely for alignment with POSIX.
 */
typedef uint16_t __wasi_errno_t;

typedef __SIZE_TYPE__ __wasi_size_t;

/**
 * A region of memory for scatter/gather writes.
 */
typedef struct __wasi_ciovec_t {
    /**
     * The address of the buffer to be written.
     */
    const uint8_t * buf;

    /**
     * The length of the buffer to be written.
     */
    __wasi_size_t buf_len;

} __wasi_ciovec_t;

/**
 * Write to a file descriptor.
 * Note: This is similar to `writev` in POSIX.
 */
__wasi_errno_t __wasi_fd_write(
    uint32_t fd,

    /**
     * List of scatter/gather vectors from which to retrieve data.
     */
    const __wasi_ciovec_t *iovs,

    /**
     * The length of the array pointed to by `iovs`.
     */
    size_t iovs_len,

    /**
     * The number of bytes written.
     */
    __wasi_size_t *nwritten
);

size_t strlen(const char *s);
int puts(const char *s);
char *memcpy(char *dest, const char *src, size_t n);
void *memmove(void *dest, const void *src, size_t n);
int main(void);
void _start(void);

#endif
