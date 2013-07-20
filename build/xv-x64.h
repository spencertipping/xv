/* XV instruction stream rewriter: x86-64 */
/* Copyright (C) 2013, Spencer Tipping */
/* Released under the terms of the GPLv3: http://www.gnu.org/licenses/gpl-3.0.txt */

/* Introduction. */
/* This file provides all definitions relevant to instruction stream rewriting for */
/* x86-64 instructions. It assumes that the instruction stream is being */
/* interpreted in 64-bit mode, not 32-bit compatibility mode. */

#ifndef XV_X64_H
#define XV_X64_H

#include "xv.h"

#include <elf.h>
#include <stdint.h>
#include <sys/types.h>

#define forward_struct(name) \
  struct name; \
  typedef struct name name;

forward_struct(xv_x64_ibuffer)
forward_struct(xv_x64_const_ibuffer)
forward_struct(xv_x64_rewriter)
forward_struct(xv_x64_insn)
forward_struct(xv_x64_bblock)
forward_struct(xv_x64_bblock_list)

#undef forward_struct

/* Rewriting strategy. */
/* Rewriting is a closed transformation: i.e. there is no runtime interaction that */
/* needs to happen to maintain the abstraction. The only exception is runtime code */
/* generation, which is detected by listening for segfaults against the */
/* write-protected original code. (Any such segfault invalidates the cached */
/* basic-blocks that refer to it.) */

/* Internally, the transformation consists of rewriting the code to preserve the */
/* invariant that all addressing logic is done using unmodified addresses. For */
/* absolute addresses this is trivial, and for %rip-relative addressing it means */
/* adjusting (or adding) the displacement for each memory-accessing instruction. */

/* Syscall opcodes, of which there are three, are each rewritten as calls into */
/* libxv. These calls aren't standard C calls; they're literally x86-64 CALL */
/* instructions that go to a receiver site in libxv. This receiver site then reads */
/* the relevant registers and performs or emulates the system call itself. */

typedef uint8_t       xv_x64_i;         /* absolutely must be unsigned */
typedef uint8_t const xv_x64_const_i;

/* Buffers. */
/* The only interesting thing going on here is that we have two start pointers. We */
/* do this because of an unfortunate confluence of problems: */

/* | 1. The managing xv process is loaded into memory before the virtualized */
/*      executable, so it may be hogging addresses mapped by the ELF image. */
/*   2. We can't move xv because we'd probably also have to move libc itself. */
/*   3. Although ld-linux.so can move ELF code just fine, the main loadable */
/*      segment can't be relocated trivially. (At least, that's what it sounds */
/*      like; I should probably research this more.) */

/* The good news is that there's an easier way to do it. Since we're going to */
/* rewrite all of the machine code anyway, we don't need to worry about loading */
/* the original code in the right place. In fact, we don't have to load it at all */
/* apart from just mapping the ELF file into memory. All we need to do is keep */
/* track of where the code would have gone, then add the displacements together */
/* when we generate the new code. */

/* Note that these logical_start values aren't %rip offsets, they're just memory */
/* locations. We won't know the %rip offsets until we start decoding stuff. */

struct xv_x64_ibuffer {
  xv_x64_const_i *logical_start;        /* code address start */
  xv_x64_i       *start;                /* start of allocated region */
  xv_x64_i       *current;              /* current insertion point */
  ssize_t         capacity;             /* size (bytes) of allocated region */
};

struct xv_x64_const_ibuffer {
  xv_x64_const_i *logical_start;
  xv_x64_const_i *start;
  xv_x64_const_i *current;
  ssize_t         capacity;
};

struct xv_x64_rewriter {
  xv_x64_const_ibuffer src;             /* original instructions */
  xv_x64_ibuffer       dst;             /* recompiled instruction stream */
  void                *xv_entry_point;  /* system call hook address */
};

/* Free any existing buffer memory, then allocate to new size. buf is unchanged
 * if return code is nonzero. */
int xv_x64_reallocate_ibuffer(xv_x64_ibuffer *buf,
                              ssize_t         size);

/* Instruction metadata. */
/* Intel opcodes and encodings are generally horrible. Luckily, we can factor away */
/* most of the weirdness by parsing each instruction into a canonical form. This */
/* gives us the flexibility to reduce the amount of code on the machine code */
/* generator end. */

/* Broadly speaking, here are the structual variants when parsing an instruction: */

/* | 1. Opcode size (1, 2, or 3 bytes). Total of 1024 opcodes, many unused. */
/*   2. Mandatory and/or size-changing prefixes. Up to 4 variants per opcode. */
/*   3. Addressing information, e.g. ModR/M, SIB, disp8 or disp32. */
/*   4. Immediate data of 1, 2, 4, or 8 bytes. */

/* We're moving the code of the process, so we need to identify every */
/* %rip-relative memory reference (including short jumps, etc -- subtle) and */
/* modify the displacement of these references so that the code won't know the */
/* difference. Specifically, we need to always have the code refer to the original */
/* addresses, fixing things up only when we know it's safe to do so. */

/* We can make a few representational optimizations to save some work. For */
/* example, we don't need to reconstruct the instruction in its original form; if */
/* the instruction was originally encoded with a redundant VEX prefix and isn't */
/* using AVX registers, we can easily enough re-encode it with REX. So we need to */
/* capture the intent, not necessarily the form, of the instruction. */

/* This is also ideal for us because it gives us a way to uniformly represent */
/* memory accesses. In this sense our interface is more like an assembler than */
/* like a machine-code manipulation layer. */

struct xv_x64_insn {
  void const *rip;              /* %rip of original instruction */

  unsigned int p1     : 2;
  unsigned int p2     : 3;
  unsigned int p66    : 1;
  unsigned int p67    : 1;
  unsigned int rex_w  : 1;
  unsigned int vex    : 1;      /* encoded with vex? (changes semantics) */
  unsigned int vex_l  : 1;
  unsigned int escape : 2;      /* opcode prefix type */
  unsigned int        : 5;

  unsigned int opcode : 8;      /* opcode byte */

  unsigned int mod : 2;         /* ModR/M high bits */
  unsigned int r1  : 4;         /* ModR/M reg field (with REX/VEX bit) */
  unsigned int r3  : 4;         /* r3 is present only with VEX */

  unsigned int scale : 2;       /* SIB byte encoding */
  unsigned int index : 4;
  unsigned int base  : 4;       /* the /m part of ModR/M if no SIB */

  int32_t displacement;         /* memory displacement, up to 32 bits */
  int64_t immediate;            /* sometimes memory offset (e.g. JMP) */
};

/* xv_x64_insn field values */
#define XV_INSN_ESC0   0        /* xv_x64_insn.escape */
#define XV_INSN_ESC1   1        /* two-byte opcode: 0x0f prefix */
#define XV_INSN_ESC238 2        /* three-byte opcode: 0x0f 0x38 prefix */
#define XV_INSN_ESC23A 3        /* three-byte opcode: 0x0f 0x3a prefix */

#define XV_INSN_LOCK  1         /* xv_x64_insn.p1 */
#define XV_INSN_REPNZ 2
#define XV_INSN_REPZ  3

#define XV_INSN_CS 1            /* xv_x64_insn.p2 */
#define XV_INSN_SS 2            /* note that CS == branch not taken, */
#define XV_INSN_DS 3            /*           DS == branch taken */
#define XV_INSN_ES 4
#define XV_INSN_FS 5
#define XV_INSN_GS 6

/* Instruction table index of given insn */
#define xv_x64_insn_key(insn_ptr) \
  ({ \
    xv_x64_insn const _insn = *(insn_ptr); \
    _insn.opcode | _insn.escape << 2; \
  })

/* Returns nonzero if the instruction's memory operand is %rip-relative */
int xv_x64_riprelp(xv_x64_insn const *insn);

/* Returns nonzero if the instruction's immediate operand is a %rip-relative
 * memory displacement */
int xv_x64_immrelp(xv_x64_insn const *insn);

/* Returns nonzero if the instruction is a system call */
int xv_x64_syscallp(xv_x64_insn const *insn);

/* Write a single instruction into the specified buffer, resizing backing
 * allocation structures as necessary. The buffer's "current" pointer is
 * advanced to the next free position. If errors occur, *buf will be
 * unchanged. */
int xv_x64_write_insn(xv_x64_ibuffer    *buf,
                      xv_x64_insn const *insn);

/* Possible return values for xv_x64_write_insn */
#define XV_WR_ERR  -1   /* internal error; read errno */
#define XV_WR_CONT  0   /* no problems; can continue writing */
#define XV_WR_END   1   /* hit end of buffer; you need to reallocate */

/* Read a single instruction from the given const buffer, advancing the buffer
 * in the process. Writes result into *insn. If errors occur, *insn has
 * undefined state and *buf will be unchanged. */
int xv_x64_read_insn(xv_x64_const_ibuffer *buf,
                     xv_x64_insn          *insn);

/* Possible return values for xv_x64_read_insn */
#define XV_READ_ERR  -1 /* internal error; read errno */
#define XV_READ_CONT  0 /* no problems; can read next instruction */
#define XV_READ_END   1 /* hit end of stream (source is invalid) */

/* Rewrite a single instruction from src to dst, updating either both or
 * neither */
int xv_x64_step_rw(xv_x64_rewriter* rw);

/* Possible return values for xv_x64_step_rw */
#define XV_RW_ERR  -1   /* internal error; read errno */
#define XV_RW_CONT  0   /* no problems; can continue */
#define XV_RW_ENDI  1   /* hit end of input; stream is invalid */
#define XV_RW_ENDO  2   /* hit end of output; you need to reallocate */

/* Operand encodings. */
/* These are used for two purposes. First, we need them to indicate the length of */
/* the remainder of the instruction; and second, we need to figure out how memory */
/* is accessed and change the ModR/M and SIB bytes for %rip-relative instructions. */

typedef uint8_t xv_x64_insn_encoding;

/* Bit 0: does instruction use ModR/M byte? */
#define XV_MODRM_MASK 0x01
#define XV_MODRM_NONE (0 << 0)  /* no ModR/M byte */
#define XV_MODRM_MEM  (1 << 0)  /* uses ModR/M byte as memory */

/* Bits 1-4 (incl): what kind of immediate data follows? */
#define XV_IMM_MASK 0x1e

#define XV_IMM_NONE (0 << 1)    /* no immediate data */

#define XV_IMM_D8   (1 << 1)    /* 8-bit %rip-relative immediate, e.g. JMP */
#define XV_IMM_D32  (2 << 1)    /* 32-bit %rip-relative immediate */
#define XV_IMM_DSZW (3 << 1)    /* word for 16-bit, dword for larger */

#define XV_IMM_I8   (4 << 1)    /* 8-bit invariant immediate, e.g. INT */
#define XV_IMM_I16  (5 << 1)    /* 16-bit invariant immediate */
#define XV_IMM_I32  (6 << 1)    /* 32-bit invariant immediate */
#define XV_IMM_I64  (7 << 1)    /* 64-bit invariant immediate */
#define XV_IMM_ISZW (8 << 1)    /* word for 16-bit opsize, dword for larger */
#define XV_IMM_ISZQ (9 << 1)    /* word, dword, or qword based on 66 and 67 */
#define XV_IMM_I2   (10 << 1)   /* imm16, imm8 (e.g. ENTER) */

/* Special value: invalid instruction */
#define XV_INVALID_MASK 0x80
#define XV_INVALID      0x80

xv_x64_insn_encoding const xv_x64_insn_encodings[1024];

#endif

/* Generated by SDoc */