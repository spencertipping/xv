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
#include <stdint.h>
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

# GCC builtins

These replace the libc functions we don't have access to.

```h
#define memcpy  __builtin_memcpy
#define memset  __builtin_memset
#define strlen  __builtin_strlen
#define strncpy __builtin_strncpy
```

```h
#if !XV_DEBUG_X64
/* Super-cheesy implementation; TODO: optimize (definitely will be necessary) */
static inline void *memcpy(void *const dst, void const *const src,
                           ssize_t const n) {
  uint8_t *const dst_bytes = (uint8_t*) dst;
  uint8_t *const src_bytes = (uint8_t*) src;
  for (int i = 0; i < n; ++i) dst_bytes[i] = src_bytes[i];
  return dst;
}
#endif
```

# Debugging stuff

If you define `XV_DEBUG_x` during compilation (where `x` is some aspect of the
code you want to debug), you'll get a bunch of internal messages as that thing
is running.

Note that `xv_trace` is invalid for production builds, as `xv` isn't linked to
the C library unless we're debugging.

```h
/* static_assert implementation based on
 * http://www.pixelbeat.org/programming/gcc/static_assert.html*/
#define xv_static_concat_(x, y) x##y
#define xv_static_concat(x, y) xv_static_concat_(x, y)
#define xv_static_assert(x) \
  enum { xv_static_concat(assert_on_line_, __LINE__) = 1 / !!(x) };
```

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
