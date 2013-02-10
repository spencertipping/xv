/* XV image functions | Spencer Tipping */
/* Licensed under the terms of the MIT source code license */

/* Introduction. */
/* This module is a little strange. It contains a bunch of functions that XV uses */
/* to inspect and relocate itself while it is running. Normal programs don't need */
/* to do this, but XV does because it tries very hard to be invisible to the */
/* running process, regardless of any fixed memory mappings requested by that */
/* process. */

#ifndef XV_SELF_H
#define XV_SELF_H

#define XV_DEFINED_SELF

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include "xv-mmap-t.h"

/* Image structures. */
/* XV maintains a stack-local indirect pointer to this structure, which is */
/* expected to reside somewhere in the memory image. Ideally we place it in the */
/* header area of the process (e.g. over the ELF header), since that memory is not */
/* useful once the process is running. */

/* It's safe to move this structure and all of the functions defined by XV because */
/* the structure contains no absolute data pointers and the functions use relative */
/* addressing to refer to each other. However, we must treat any pointer to the */
/* `self_t` as being mutable to accommodate spontaneous relocation. For example, */
/* suppose we wrote this: */

/* | void f(xv_self_t *self) { */
/*     // stuff */
/*     do_something(self); */
/*     // stuff */
/*   } */
/*   void do_something(xv_self_t *self) { */
/*     // oops, lvalue isn't deep enough */
/*     // we need to say this: *self = xv_self_relocate(*self, new_address); */
/*     self = xv_self_relocate(self, new_address); */
/*   } */

/* Instead, each function needs to take an `xv_self_t**` so that the inner */
/* `xv_self_relocate` call can propagate outwards. We don't need to make the */
/* pointer volatile because any image modifications will occur within the self */
/* mutex. */

typedef struct {
  off_t         image_base;             /* x->self == x */
  size_t        image_size;             /* size of state + functions */
  volatile long flags;                  /* current flags */

  xv_mmap_t     self_maps;              /* mapped memory of xv */
  xv_mmap_t     virtual_maps;           /* mapped memory of virtual process */
} xv_self_t;

#define xv_set -1
#define xv_flag(n, flag) (n & (1 << (flag)))

#define xv_flag_handling_signal 0       /* are we handling a signal? */
#define xv_flag_writelock       1       /* are our pages write-locked? */
#define xv_flag_relocating      2       /* is this image being relocated? */

/* Self placement and relocation. */
/* In order to create the initial `xv_self_t`, we look at the offset of the */
/* `_start` function (which we are allowed to clobber) and round down to the */
/* nearest page boundary (4096 bytes on x86). */

void xv_end_sentinel_1(void);
void xv_end_sentinel_2(void);

xv_self_t *xv_create_self(void) {
  xv_self_t *self = (xv_self_t*) ((off_t) &_start & ~(xv_page_size - 1));
  off_t      end  = (2 * (off_t) &xv_end_sentinel_2)
                  - (off_t) &xv_end_sentinel_1;

  self->image_base = (off_t) self;
  self->image_size = (size_t) (end - self->image_base);
  self->flags      = 0;
  return self;
}

/* When you choose to relocate XV, make sure you have the memory reserved already. */
/* You also need to manually unmap the old image and change the self_t pointer as */
/* described earlier. */

void xv_warp(ptrdiff_t diff);

xv_self_t *xv_self_relocate(xv_self_t *self, void *addr) {
  fail_if(xv_flag(self->flags, xv_flag_writelock),
          "cannot relocate a writelocked xv self");

  fail_if(xv_flag(self->flags, xv_flag_relocating),
          "cannot relocate a self that is already being relocated");

  /* Mark this image as being relocated. An image never loses this flag; we
   * clear it only on the new image but never remove it from the old one. There
   * is a period of time during the copy when both images have the flag,
   * meaning that each one is temporarily invalid. Therefore, this whole
   * function is a critical section on the image. */
  self->flags |= xv_flag(xv_set, xv_flag_relocating);

  for (off_t src = self->image_base,
             dst = (off_t) addr,
             end = self->image_base + self->image_size;
       src < end;
       src += xv_copy_step, dst += xv_copy_step)
    *(xv_copy_type*)dst = *(xv_copy_type*)src;

  /* Now for the hard part. We need to move execution to the new image location
   * so that the original can be safely unmapped. To do this, we generate an
   * indirect jump to the absolute new location of where we are right now. This
   * can be done easily enough by taking the difference between the images and
   * computing an %rip-relative address. */
  xv_self_t *new_self = (xv_self_t*) addr;
  ptrdiff_t  diff     = (off_t) new_self - (off_t) self;

  /* This is the last thing we do inside the old image. */
  xv_warp(diff);

  new_self->flags &= ~(xv_flag(xv_set, xv_flag_relocating));

  /* At this point we still have stack references to the old image, but we are
   * executing inside the new one. All we need to do is return the new image
   * and assume that the caller will update the self_t pointers. */
  return new_self;
}

#endif

/* Generated by SDoc */
