#include "tcclib.h"

typedef unsigned char uint8_t;
typedef unsigned short int uint16_t;
typedef unsigned uint32_t;

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

size_t strlen(const char *s) {
    // size_t i = 0;
    // while (s[i]) i = i + 1;
    // return i;
    return 14;
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

int main(void);

void _start(void) {
    main();
}
