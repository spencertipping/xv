XV platform-agnostic functions | Spencer Tipping
Licensed under the terms of the MIT source code license

# Introduction

This is where we do stuff like option parsing, module setup, and utility
function definitions. XV data structures are defined in xv-self.h.

    #ifndef XV_GENERIC_H
    #define XV_GENERIC_H

    #ifndef XV_DEFINED_SYSCALL
    #error xv-generic.h requires XV_DEFINED_SYSCALL
    #endif

# Utility functions

Useful utilities that exist one layer above the platform-independent
abstractions defined by syscall layers.

    #define xv_puts(fd, s) xv_puts_(fd, s, sizeof(s))
    #define xv_log(s)      xv_puts(xv_stderr, s)

    #endif