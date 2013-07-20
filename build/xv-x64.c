/* XV instruction stream rewriter: x86-64 */
/* Copyright (C) 2013, Spencer Tipping */
/* Released under the terms of the GPLv3: http://www.gnu.org/licenses/gpl-3.0.txt */

/* Introduction. */
/* Implementations of most of the functions in xv-x64.h; see also xv-x64.s for the */
/* assembly-language syscall intercept. */

#include "xv-x64.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>

/* Write buffer reallocation. */
/* This is kind of interesting. xv uses a lame allocation strategy: you allocate */
/* some initial memory, hopefully enough, then it tries to write into that memory. */
/* If it fails, you have to throw it all away, allocate more memory, and try */
/* again. The reason is that instructions change as you move them, and in some */
/* cases even their sizes change. So there is no way you can reuse the old */
/* compilation output. Fortunately, rewriting should be fast enough that this */
/* isn't a problem. */

/* Note that the first time you "reallocate" a buffer, it should have a zero */
/* start-pointer. Otherwise this call will do nothing and return an error code */
/* because it will think it couldn't free memory. */

int xv_x64_reallocate_ibuffer(xv_x64_ibuffer *const buf,
                              ssize_t         const size) {
  ssize_t const page_size = getpagesize();
  ssize_t const rounded   = size + page_size - 1 & ~(page_size - 1);

  if (buf->capacity == rounded) return 0;
  if (buf->start && munmap(buf->start, buf->capacity)) return errno;

  if (size > 0) {
    void *const region = mmap(NULL,
                              rounded,
                              PROT_READ | PROT_WRITE | PROT_EXEC,
                              MAP_PRIVATE | MAP_ANONYMOUS,
                              -1,
                              0);

    if (region == (void *const) -1) return errno;

    buf->start    = region;
    buf->capacity = rounded;
  } else {
    buf->start    = NULL;
    buf->capacity = 0;
  }

  return 0;
}

/* Instruction parser. */
/* See xv-x64.h for relevant definitions. In particular, we make heavy use of the */
/* encoding table, which tells us just about everything we need to know about the */
/* instruction. */

/* Instruction-prefix predicates for byte values */
#define XV_G1P(x) ((x) == 0xf0 || (x) == 0xf2 || (x) == 0xf3)

#define XV_G2P(x) ((x) == 0x2e || (x) == 0x36 || \
                   (x) == 0x3e || (x) == 0x26 || \
                   (x) == 0x64 || (x) == 0x65)

#define XV_G3P(x) ((x) == 0x66)
#define XV_G4P(x) ((x) == 0x67)

#define XV_REXP(x)  (((x) & 0xf0) == 0x40)
#define XV_VEX3P(x) ((x) == 0xc4)
#define XV_VEX2P(x) ((x) == 0xc5)

#define XV_OPESC1P(x) ((x) == 0x0f)
#define XV_OPESC2P(x) ((x) == 0x38 || (x) == 0x3a)

int xv_x64_read_insn(xv_x64_const_ibuffer *const buf,
                     xv_x64_insn          *const insn) {
  int const initial_offset = buf->current - buf->start;
  int       offset         = initial_offset;

  memset(insn, 0, sizeof(xv_x64_insn));

  /* Look for group 1, 2, 3, and 4 prefixes, ignoring each one after we see the
   * first. We're done looking if we hit the end of the input stream, or if we
   * see anything that isn't a prefix. */
  if (offset >= buf->capacity) return XV_READ_END;
  for (unsigned g1p, g2p, g3p, g4p, current = buf->start[offset];

       offset < buf->capacity
         && ((g1p = XV_G1P(current)) || (g2p = XV_G2P(current)) ||
             (g3p = XV_G3P(current)) || (g4p = XV_G4P(current)));

       ++offset,
       insn->p1 = !insn->p1 && g1p ? current == 0xf0 ? XV_INSN_LOCK
                                   : current == 0xf2 ? XV_INSN_REPNZ
                                                     : XV_INSN_REPZ
                                   : insn->p1,

       insn->p2 = !insn->p2 && g2p ? current == 0x2e ? XV_INSN_CS
                                   : current == 0x36 ? XV_INSN_SS
                                   : current == 0x3e ? XV_INSN_DS
                                   : current == 0x26 ? XV_INSN_ES
                                   : current == 0x64 ? XV_INSN_FS
                                                     : XV_INSN_GS
                                   : insn->p2,
       insn->p66 |= g3p,
       insn->p67 |= g4p);

  /* Now look for REX and VEX prefixes. If there are multiple (technically
   * disallowed), we will get wonky results. */
  if (offset >= buf->capacity) return XV_READ_END;
  for (unsigned rexp, vex2p, vex3p, current = buf->start[offset];

       offset < buf->capacity
         && ((rexp  =  XV_REXP(current)) ||
             (vex2p = XV_VEX2P(current)) ||
             (vex3p = XV_VEX3P(current)));

       ++offset,
       insn->vex   |= vex2p || vex3p,
       insn->rex_w |= rexp && !!(current & 0x08),
       insn->r1    |= rexp && (current & 0x04) << 1,    /* REX.R */
       insn->index |= rexp && (current & 0x02) << 2,    /* REX.X */
       insn->base  |= rexp && (current & 0x01) << 3)    /* REX.B */

    if (vex2p || vex3p) {
      if (vex2p) {
        if (++offset >= buf->capacity) return XV_READ_END;
        current = buf->start[offset];
        insn->escape = XV_INSN_ESC1;                    /* implied */
        insn->r1     = !(current & 0x80) << 3;          /* VEX.R */
      } else if (vex3p) {
        if (++offset >= buf->capacity) return XV_READ_END;
        current = buf->start[offset];
        insn->r1     = !(current & 0x80) << 3;          /* VEX.R */
        insn->index  = !(current & 0x40) << 3;          /* VEX.X */
        insn->base   = !(current & 0x20) << 3;          /* VEX.B */
        insn->escape = current & 0x1f;                  /* VEX.m-mmmm */

        if (++offset >= buf->capacity) return XV_READ_END;
        current = buf->start[offset];
        insn->rex_w = !!(current & 0x80);               /* VEX.W */
      }

      insn->r3     =   (current & 0x78) >> 3 ^ 0x0f;    /* VEX.vvvv */
      insn->vex_l  = !!(current & 0x04);
      insn->p66   |= current == 0x01;                   /* VEX.pp */
      insn->p2     = (current & 0x03) == 2 ? XV_INSN_REPZ
                   : (current & 0x03) == 3 ? XV_INSN_REPNZ
                   : insn->p2;
    }

  /* By this point we've read all prefixes except for opcode escapes. Now look
   * for those. */
  if (++offset >= buf->capacity) return XV_READ_END;
  for (unsigned current = buf->start[offset];
       offset < buf->capacity
         && (!insn->escape     && (insn->escape |= XV_OPESC1P(current)) ||
             insn->escape == 1
               && (insn->escape = current == 0x38 ? XV_INSN_ESC238
                                : current == 0x3a ? XV_INSN_ESC23A
                                : insn->escape));
      ++offset);

  if (offset >= buf->capacity) return XV_READ_END;
  insn->opcode = buf->start[offset];

  /* Now we have the opcode and all associated prefixes, so we can parse out
   * the remaining bytes using the operand encoding table. */
  xv_x64_insn_encoding const enc =
    xv_x64_insn_encodings[xv_x64_insn_key(insn)];

  /* If we use ModR/M, parse that byte and check for SIB. */
  if (enc & XV_MODRM_MASK) {
    if (++offset >= buf->capacity) return XV_READ_END;
    unsigned current = buf->start[offset];
    insn->mod  = (current & 0xc0) >> 6;
    insn->r1  |= (current & 0x38) >> 3;

    int displacement_bytes =
          insn->mod == 0 ? (current & 0x07) == 5 ? 4 : 0
        : insn->mod == 1 ? 1
        : insn->mod == 2 ? 4 : 0;

    if (insn->mod != 3 && (current & 0x07) == 4) {
      if (++offset >= buf->capacity) return XV_READ_END;
      current = buf->start[offset];
      insn->scale  = (current & 0xc0) >> 6;
      insn->index |= (current & 0x38) >> 3;
    }

    insn->base |= current & 0x07;

    /* Now read little-endian displacement. */
    if ((offset += displacement_bytes) >= buf->capacity) return XV_READ_END;
    for (int i = 0; i < displacement_bytes; ++i)
      insn->displacement = insn->displacement << 8 | buf->start[offset - i];
  }

  /* And read the immediate data, little-endian. */
  int const immediate_size = xv_x64_immediate_bytes(insn);
  if ((offset += immediate_size) >= buf->capacity) return XV_READ_END;
  for (int i = 0; i < immediate_size; ++i)
    insn->immediate = insn->immediate << 8 | buf->start[offset - i];

  /* Now update the buffer and record the instruction's logical %rip value.
   * This will allow us to track the absolute address for cases like
   * %rip-relative displacement. */
  buf->current = buf->start + ++offset;
  insn->rip    = buf->current - buf->start + buf->logical_start;

  return XV_READ_CONT;
}

/* Instruction generator. */
/* Write a structured instruction back out as x86 machine code. */

int xv_x64_write_insn(xv_x64_ibuffer    *const buf,
                      xv_x64_insn const *const insn) {
  /* TODO */
  return XV_WR_ERR;
}

/* Instruction rewriter. */
/* This is where the fun stuff happens. This function steps a rewriter forward by */
/* one instruction, returning a status code to indicate what should happen next. */

inline int xv_x64_riprelp(xv_x64_insn const *const insn) {
  return insn->mod == 0 && insn->base == 5;
}

inline int xv_x64_immrelp(xv_x64_insn const *const insn) {
  xv_x64_insn_encoding const enc =
    xv_x64_insn_encodings[xv_x64_insn_key(insn)];
  unsigned const imm = enc & XV_IMM_MASK;
  return imm == XV_IMM_D8
      || imm == XV_IMM_D32
      || imm == XV_IMM_DSZW;
}

inline int xv_x64_syscallp(xv_x64_insn const *const insn) {
  return insn->escape == XV_INSN_ESC1   /* syscall, sysenter */
           && (insn->opcode == 0x05 || insn->opcode == 0x34)
      || insn->escape == XV_INSN_ESC0   /* int 80; maybe invalid in 64-bit? */
           && insn->opcode    == 0xcd
           && insn->immediate == 0x80;
}

int xv_x64_step_rw(xv_x64_rewriter *const shared_rw) {
  /* TODO */
  return XV_RW_ERR;
}

/* Generated by SDoc */
