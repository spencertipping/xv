XV Linux x86-64 ucontext.h constants | Spencer Tipping
Licensed under the terms of the MIT source code license

# Introduction

These are various offsets and register indexes from ucontext.h. We use them to
read the system state when we observe a signal, particularly when we get a
SIGILL indicating that a syscall was to be made. We can then figure out which
syscall it was by reading %rax.

    :[::%i64 = 8] :[::%i32 = 4] :[::%i16 = 2] :[::%i8 = 1]
    :[::%ptr    = :%i64]
    :[::%size   = :%i64]
    :[::%sigset = :%i64]

# General register offsets

Indexes into the gregs[] array, which is the first member of the mcontext
struct.

    :[::ngreg = 23] :[::%greg = :ngreg * :%i64]

    :[::reg-r8  = 0]  :[::reg-r9  = 1]  :[::reg-r10 = 2]  :[::reg-r11 = 3]
    :[::reg-r12 = 4]  :[::reg-r13 = 5]  :[::reg-r14 = 6]  :[::reg-r15 = 7]
    :[::reg-rdi = 8]  :[::reg-rsi = 9]  :[::reg-rbp = 10] :[::reg-rbx = 11]
    :[::reg-rdx = 12] :[::reg-rax = 13] :[::reg-rcx = 14] :[::reg-rsp = 15]
    :[::reg-rip = 16] :[::reg-efl = 17] :[::reg-cgf = 18] :[::reg-err = 19]
    :[::reg-tno = 20] :[::reg-old = 21] :[::reg-cr2 = 21]

# FP state struct

Normally we don't care about this, but we may need to get to XMM registers, and
those values are stored here. This structure contains no internal padding.

    # struct fpxreg
    :[::fpxreg-significand = 0]                                     # 4x 16-bit
    :[::fpxreg-exponent    = :fpxreg-significand + 4 * :%i16]       # 16-bit
    :[::fpxreg-padding     = :fpxreg-exponent + :%i16]              # 3x 16-bit

    :[::%fpxreg = :fpxreg-padding + 3 * :%i16]

    # struct xmmreg
    :[::%xmmreg = 4 * :%i32]                                # 4x 32-bit

    # struct fpstate
    :[::fpstate-cwd = 0]                                    # 16-bit
    :[::fpstate-swd = :fpstate-cwd + :%i16]                 # 16-bit
    :[::fpstate-ftw = :fpstate-swd + :%i16]                 # 16-bit
    :[::fpstate-fop = :fpstate-ftw + :%i16]                 # 16-bit

    :[::fpstate-rip = :fpstate-fop + :%i16]                 # 64-bit
    :[::fpstate-rdp = :fpstate-rip + :%i64]                 # 64-bit

    :[::fpstate-mxcsr     = :fpstate-rdp   + :%i64]         # 32-bit
    :[::fpstate-mxcr-mask = :fpstate-mxcsr + :%i32]         # 32-bit

    :[::fpstate-st  = :fpstate-mxcr-mask + :%i32]           # 8x fpxreg
    :[::fpstate-xmm = :fpstate-st  + 8  * :%fpxreg]         # 16x xmmreg
    :[::fpstate-pad = :fpstate-xmm + 16 * :%xmmreg]         # 24x 32-bit

    :[::%fpstate = :fpstate-pad + 24 * :%i32]

# Machine context struct

Fields into mcontext objects. No alignment worries here since each reg fills a
full slot.

    :[::mcontext-gregs    = 0]                              # greg[ngregs]
    :[::mcontext-fpregs-p = :mcontext-gregs + :%gregs]      # 64-bit pointer
    :[::mcontext-res1     = :mcontext-fpregs-p + :%ptr]     # 8x 64-bit

    :[::%mcontext = :mcontext-res1 + 8 * :%i64]

# Stack struct

The stack of the program when the signal was sent.

    :[::stack-sp    = 0]                                    # 64-bit
    :[::stack-flags = :stack-sp + :%i64]                    # 32-bit
    :[::stack-size  = :stack-flags + :%i64]                 # pad, 64-bit

    :[::%stack = :stack-size + :%i64]

# User context struct

This gives us access to the machine context, the signal mask, and the FP state
all at once.

    :[::ucontext-flags   = 0]                               # 32-bit
    :[::ucontext-link-p  = :ucontext-flags + :%i64]         # pad, 64-bit
    :[::ucontext-stack   = :ucontext-link-p + :%i64]        # stack
    :[::ucontext-mc      = :ucontext-stack + :%stack]       # mcontext
    :[::ucontext-sigmask = :ucontext-mc + :%mcontext]       # sigmask
    :[::ucontext-fpregs  = :ucontext-sigmask + :%sigset]    # fpstate

    :[::%ucontext = :ucontext-fpregs + :%fpstate]