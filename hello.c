#!/usr/local/bin/tcc -run -nostdlib

// SPDX-FileCopyrightText: 2021 Alexander Sosedkin <monk@unboiled.info>
// SPDX-License-Identifier: MIT

// syscalls (x86_64) ///////////////////////////////////////////////////////////

#include "syscall.h"

#define SYS_write 1
#define SYS_open 2
#define SYS_fork 57
#define SYS_execve 59
#define SYS_exit 60
#define SYS_wait4 61
#define SYS_getdents 78
#define SYS_mkdir 83

long write(int fd, const void* buf, long cnt) {
	return __syscall3(SYS_write, fd, (long) buf, cnt);
}

int execve(const char* fname,
		const char* const argv[], const char* const envp[]) {
	return __syscall3(SYS_execve, (long) fname, (long) argv, (long) envp);
}

void exit(int code) { __syscall1(SYS_exit, code); };

int fork() { return __syscall0(SYS_fork); }

int wait4(int pid, int* status, int options, void* rusage) {
	return __syscall4(
		SYS_wait4,
		pid, (long) status, options, (long) rusage
	);
}

int mkdir(const char *pathname, unsigned mode) {
	return __syscall2(SYS_mkdir, (long) pathname, mode);
}

int open(const char *pathname, int flags, int mode) {
	return __syscall3(SYS_open, (long) pathname, flags, mode);
}

struct linux_dirent {
	long d_ino;
	long d_off;
	unsigned short d_reclen;
	char d_name[];
};
int getdents(unsigned int fd, struct linux_dirent *dirp,
		unsigned int count) {
	return __syscall3(SYS_getdents, fd, (long) dirp, count);
}

int main() {
    write(1, "Hello, world!\n", 14);
    return 0;
}

int _start() {
    return main();
}
