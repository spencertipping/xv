XV x86-64 Linux syscall interface | Spencer Tipping
Licensed under the terms of the MIT source code license

# Introduction

XV ends up making a lot of syscalls, generally on behalf of the running
process. We abstract this away into a syscall function and a series of wrapper
macros for it.

    #ifndef XV_X64_LINUX_SYSCALL_H
    #define XV_X64_LINUX_SYSCALL_H

    #define XV_SYSCALLS_DEFINED

    #include <stdint.h>
    #include <asm/types.h>
    #include <asm/unistd.h>

    int64_t xv_syscall6(int64_t n, int64_t a, int64_t b, int64_t c,
                                   int64_t d, int64_t e, int64_t f) {
      asm volatile ("movq %3, %%r10;"
                    "movq %4, %%r8;"
                    "movq %5, %%r9;"
                    "syscall;"
                  : "=a"(n)
                  : "a"(n), "D"(a), "S"(b), "d"(c), "r"(d), "r"(e), "r"(f)
                  : "%r10", "%r8", "%r9", "%rcx");
      return n;
    }

    #define xv_syscall5(n,a,b,c,d,e) xv_syscall6(n,a,b,c,d,e,0)
    #define xv_syscall4(n,a,b,c,d)   xv_syscall5(n,a,b,c,d,0)
    #define xv_syscall3(n,a,b,c)     xv_syscall4(n,a,b,c,0)
    #define xv_syscall2(n,a,b)       xv_syscall3(n,a,b,0)
    #define xv_syscall1(n,a)         xv_syscall2(n,a,0)
    #define xv_syscall0(n)           xv_syscall1(n,0)

    #define xv_stdin  0
    #define xv_stdout 1
    #define xv_stderr 2

# Utility functions

These are tied to the syscall interface and are likely to change between
operating systems.

    ssize_t xv_puts_(int fd, const void *buf, ssize_t len) {
      xv_syscall3(__NR_write, fd, buf, len);
    }

    #endif