XV library and common definitions
Copyright (C) 2013, Spencer Tipping
Released under the terms of the GPLv3: http://www.gnu.org/licenses/gpl-3.0.txt

# Introduction

General definitions for XV. See xv-x64.h.sdoc (and possibly others later on)
for processor-specific stuff.

```h
#ifndef XV_H
#define XV_H
```

```h
#include <sys/types.h>
```

# Image placement

The linker defines values for these symbols, which xv uses to relocate itself
anytime there is an address space collision. See [xv.x.md](xv.x.md) for
details.

```h
extern void const *const xv_image_start;
extern void const *const xv_image_end;
```

# Debugging stuff

If you define `XV_DEBUG_x` during compilation (where `x` is some aspect of the
code you want to debug), you'll get a bunch of internal messages as that thing
is running.

Note that the `fprintf` and `fflush` functions referred to here are defined
within xv, which isn't linked to a C library.

```h
#define xv_no_trace(x, ...) (x)
#define xv_trace(x, message, ...) \
  (fprintf(stderr, (message), __VA_ARGS__), \
   fflush(stderr), \
   (x))
```

```h
#define xv_ifdebug    if (1)
#define xv_no_ifdebug if (0)
```

You'll want to do something like this to setup the debugging interface within a
module:

    #if XV_DEBUG_X64
    #include <stdio.h>
    #define xv_x64_trace xv_trace
    #else
    #define xv_x64_trace xv_no_trace
    #endif

```h
#endif

```
