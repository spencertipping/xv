XV image functions | Spencer Tipping
Licensed under the terms of the MIT source code license

# Introduction

This module is a little strange. It contains a bunch of functions that XV uses
to inspect and relocate itself while it is running. Normal programs don't need
to do this, but XV does because it tries very hard to be invisible to the
running process, regardless of any fixed memory mappings requested by that
process.

    #ifndef XV_SELF_H
    #define XV_SELF_H

    #define XV_DEFINED_SELF

# Image structures

XV maintains a stack-local indirect pointer to this structure, which is
expected to reside somewhere in the memory image. Ideally we place it in the
header area of the process (e.g. over the ELF header), since that memory is not
useful once the process is running.

It's safe to move this structure and all of the functions defined by XV because
the structure contains no absolute data pointers and the functions use relative
addressing to refer to each other. However, we must treat any pointer to the
`self_t` as being mutable to accommodate spontaneous relocation. For example,
suppose we wrote this:

    void f(xv_self_t *self) {
      // stuff
      do_something(self);
      // stuff
    }
    void do_something(xv_self_t *self) {
      // oops, lvalue isn't deep enough
      // we need to say this: *self = xv_self_relocate(*self, new_address);
      self = xv_self_relocate(self, new_address);
    }

Instead, each function needs to take an `xv_self_t**` so that the inner
`xv_self_relocate` call can propagate outwards. We don't need to make the
pointer volatile because any image modifications will occur within the self
mutex.

    typedef struct {
      ptrdiff_t image_base;                 /* x->self == x */
      ptrdiff_t image_size;                 /* size of state + functions */
      long      flags;                      /* current flags */

      mmap_t    process_maps;               /* mapped memory of virtual process */
    } xv_self_t;

    #define xv_set -1
    #define xv_flag(n, flag) (n & (1 << (flag)))

    #define xv_flag_handling_signal 0       /* are we handling a signal? */

# Self placement and relocation

In order to create the initial `xv_self_t`, we look at the offset of the
`_start` function (which we are allowed to clobber) and round down to the
nearest page boundary (4096 bytes on x86).

    volatile void xv_end_sentinel_1(void);
    volatile void xv_end_sentinel_2(void);
    xv_self_t *xv_create_self(void) {
      xv_self_t *self = (xv_self_t*) ((ptrdiff_t) &_start & ~(xv_page_size - 1));
      ptrdiff_t  end  = (2 * (ptrdiff_t) &xv_end_sentinel_2)
                      - (ptrdiff_t) &xv_end_sentinel_1;

      self->image_base = (ptrdiff_t) self;
      self->image_size = end - self->image_base;
      self->flags      = 0;
      return self;
    }

    xv_self_t *xv_self_relocate(xv_self_t *self, void *addr) {

    }

    #endif