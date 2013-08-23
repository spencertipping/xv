XV virtualization state
Copyright (C) 2013, Spencer Tipping
Released under the terms of the GPLv3: http://www.gnu.org/licenses/gpl-3.0.txt

# Introduction

These definitions maintain the current virtualization state, which includes
things like keeping track of xv and application pages, managing hooks, and the
translation cache. This is also where we define the semantics for moving xv
from one location to another.

```h
#ifndef XV_VIRT_H
#define XV_VIRT_H
```

# Virtualization state structure

All of xv's runtime state is linked into one structure, which gives us a couple
of very important properties. First, it means we have an upper bound over the
space of xv-visible state -- including the execution stack. Second, as a
consequence of this, it means we can define a garbage collect-by-copy
operation, which allows us to use heap-style allocation.

As usual, there's some subtlety with this. First of all, the execution stack is
inside the relocatable space, but it contains absolute addresses to xv-code
that will itself be relocated. This means we need to patch up the return
addresses when relocation happens. [1]

Second, we need to reset various xv-external state when we relocate the memory.
Specifically, xv installs traps and a system-call entry point in an executable
memory page that also contains the virtualization state structure. This page is
referenced only from the translation cache, which is also relocatable and is
discarded each time xv moves (since the translated code might contain
%rip-relative addresses).

```h
#endif
```

# Footnote 1

It can't be done after the fact by catching segfaults. The reason is that we
would need to maintain a list of previous locations of xv, which would require
space proportional to the number of relocations; since we could relocate any
number of times, this becomes a separate GC problem in itself and therefore
requires us to parse the stack.

After relocating, the translation cache will be empty; so we won't have
addresses for most of the code that the stack refers to. We fix this by
rewriting the stack addresses to point to the original code, which will cause
segfaults when those returns happen. This triggers code rewriting, and
everything continues normally from there.
