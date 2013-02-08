XV AMD64 syscall virtualization | Spencer Tipping
Licensed under the terms of the MIT source code license

# Introduction

This module provides tools to decode x86-64 instructions and transform syscalls
into illegal instructions. The result is that we will receive a SIGILL each
time the program attempts to make a system call.

The following instructions are rewritten:

    0f05  (syscall)
    0f34  (sysenter)
    cd80  (int 0x80)

Each of these two-byte instructions is replaced with `8dc0` (`lea %eax, %eax`,
an illegal addressing mode for `lea`).

# Decoder

We can't replace those byte combinations just anywhere. Instead, we need to
make sure they're at the beginning of an instruction boundary. To do this, we
need to have a function that tells us where the next instruction begins. This
function is implemented as a finite-state machine with the following states:

    begin         expect prefix or opcode
    prefix        expect prefix or opcode
    opcode        classify the op
    nullary-op    done
    modrm-op      parse mod/rm byte, sib, and disp (if applicable)
    imm8-op       skip 1 additional byte
    imm16-op      skip 2 additional bytes
    imm32-op      skip 4 additional bytes
    imm64-op      skip 8 additional bytes

    ::xv-next-insn