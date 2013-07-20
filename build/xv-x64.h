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

typedef uint8_t       xv_x64_i;
typedef uint8_t const xv_x64_const_i;

struct xv_x64_ibuffer {
  xv_x64_const_i *logical_start;
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

int xv_ibuffer_init(xv_x64_ibuffer *buf);
int xv_ibuffer_free(xv_x64_ibuffer *buf);

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

/* Read a single instruction from the given const buffer, advancing the buffer
 * in the process. Writes result into *insn. If errors occur, *insn has
 * undefined state and *buf will be unchanged. */
int xv_x64_read_insn(xv_x64_const_ibuffer *buf,
                     xv_x64_insn          *insn);

/* Possible return values for xv_read_insn */
#define XV_READ_ERR  -1 /* internal error; read errno */
#define XV_READ_CONT  0 /* no problems; can read next instruction */
#define XV_READ_END   1 /* hit end of stream (source is invalid) */

/* Rewrite a single instruction from src to dst, updating either both or
 * neither */
int xv_x64_step_rw(xv_x64_rewriter* rw);

/* Possible return values for xv_step_rw */
#define XV_RW_ERR  -1   /* internal error; read errno */
#define XV_RW_CONT  0   /* no problems; can continue */
#define XV_RW_END   1   /* hit end of stream (source is invalid) */
#define XV_RW_JMP   2   /* last instruction was unconditional jump */
#define XV_RW_JCC   3   /* last instruction was conditional jump */

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

#define R2(...)  __VA_ARGS__, __VA_ARGS__
#define R4(...)  R2(R2(__VA_ARGS__))
#define R8(...)  R4(R2(__VA_ARGS__))
#define R16(...) R8(R2(__VA_ARGS__))

xv_x64_insn_encoding const xv_x64_insn_encodings[1024] =
{
  /* one-byte opcode, no prefix */
  /* 0x00 - 0x3f */ R8(R4(XV_MODRM_MEM | XV_IMM_NONE),  /* ALU */
                       XV_MODRM_NONE | XV_IMM_I8,       /* ALU imm8 */
                       XV_MODRM_NONE | XV_IMM_ISZW,     /* ALU imm */
                       R2(XV_INVALID)),

  /* 0x40 - 0x4f */ R16(XV_INVALID),                    /* REX prefix */
  /* 0x50 - 0x5f */ R16(XV_MODRM_NONE | XV_IMM_NONE),

  /* 0x60 - 0x6f */ R8(XV_INVALID),                     /* invalid, prefixes */
                    XV_MODRM_NONE | XV_IMM_ISZW,        /* push imm */
                    XV_MODRM_MEM  | XV_IMM_ISZW,        /* three-arg imul */
                    XV_MODRM_NONE | XV_IMM_I8,          /* push imm8 */
                    XV_MODRM_MEM  | XV_IMM_I8,          /* three-arg imul */
                    R4(XV_MODRM_NONE | XV_IMM_NONE),    /* ins, outs */

  /* 0x70 - 0x7f */ R16(XV_MODRM_NONE | XV_IMM_D8),     /* jcc */

  /* 0x80 - 0x8f */ XV_MODRM_MEM | XV_IMM_I8,           /* group1 insns */
                    XV_MODRM_MEM | XV_IMM_ISZW,         /* group1 insns */
                    XV_INVALID,
                    XV_MODRM_MEM | XV_IMM_I8,           /* group1 insns */
                    R4(XV_MODRM_MEM | XV_IMM_NONE),     /* test, xchg */
                    R8(XV_MODRM_MEM | XV_IMM_NONE),     /* mov, pop, lea */

  /* 0x90 - 0x9f */ R8(XV_MODRM_NONE | XV_IMM_NONE),    /* xchg with %rax */
                    R2(XV_MODRM_NONE | XV_IMM_NONE),    /* %rax conversions */
                    XV_INVALID,                         /* callf */
                    XV_MODRM_NONE | XV_IMM_NONE,        /* wait */
                    R4(XV_MODRM_NONE | XV_IMM_NONE),    /* flag insns */

  /* 0xa0 - 0xaf */ R2(XV_MODRM_NONE | XV_IMM_I8,       /* mov %al */
                       XV_MODRM_NONE | XV_IMM_ISZQ),    /* mov %[re_]ax */
                    R4(XV_MODRM_NONE | XV_IMM_NONE),    /* movs */
                    XV_MODRM_NONE | XV_IMM_I8,          /* test %al */
                    XV_MODRM_NONE | XV_IMM_ISZW,        /* test %[re_]ax */
                    R2(XV_MODRM_NONE | XV_IMM_NONE),    /* stos[b] */
                    R4(XV_MODRM_NONE | XV_IMM_NONE),    /* lods[b], scas[b] */

  /* 0xb0 - 0xbf */ R8(XV_MODRM_NONE | XV_IMM_I8),      /* movb %rxx, ib */
                    R8(XV_MODRM_NONE | XV_IMM_ISZW),    /* mov[wlq] ... */

  /* 0xc0 - 0xcf */ R2(XV_MODRM_MEM | XV_IMM_I8),       /* group2 insns */
                    XV_MODRM_NONE | XV_IMM_ISZW,        /* ret imm */
                    XV_MODRM_NONE | XV_IMM_NONE,        /* ret */
                    R2(XV_INVALID),                     /* les, lds */
                    XV_MODRM_MEM | XV_IMM_I8,           /* movb */
                    XV_MODRM_MEM | XV_IMM_ISZW,         /* mov[wlq] */
                    XV_MODRM_NONE | XV_IMM_I2,          /* enter */
                    XV_MODRM_NONE | XV_IMM_NONE,        /* leave */
                    XV_MODRM_NONE | XV_IMM_I16,         /* retf imm16 */
                    XV_MODRM_NONE | XV_IMM_NONE,        /* retf */
                    XV_MODRM_NONE | XV_IMM_NONE,        /* int 3 */
                    XV_MODRM_NONE | XV_IMM_I8,          /* int imm8 */
                    XV_INVALID,                         /* into */
                    XV_MODRM_NONE | XV_IMM_NONE,        /* iret */

  /* 0xd0 - 0xdf */ R4(XV_MODRM_MEM | XV_IMM_NONE),     /* group2 insns */
                    R2(XV_INVALID),                     /* aam, aad */
                    XV_INVALID,
                    XV_MODRM_NONE | XV_IMM_NONE,        /* xlat[b_] */
                    R8(XV_INVALID),                     /* coprocessor */

  /* 0xe0 - 0xef */ R4(XV_MODRM_NONE | XV_IMM_D8),      /* loop, jcxz */
                    R4(XV_MODRM_NONE | XV_IMM_I8),      /* in/out %al, ... */
                    R2(XV_MODRM_NONE | XV_IMM_DSZW),    /* call/jmp dispW */
                    XV_INVALID,                         /* far jmp */
                    XV_MODRM_NONE | XV_IMM_D8,          /* short jmp disp8 */
                    R4(XV_MODRM_NONE | XV_IMM_NONE),    /* in/out %a, %d */

  /* 0xf0 - 0xff */ R4(XV_INVALID),                     /* prefixes */
                    R2(XV_MODRM_NONE | XV_IMM_NONE),    /* hlt, cmc */
                    R2(XV_MODRM_MEM | XV_IMM_NONE),     /* group3 insns */
                    R4(XV_MODRM_NONE | XV_IMM_NONE),    /* flags */
                    R2(XV_MODRM_NONE | XV_IMM_NONE),    /* flags */
                    R2(XV_MODRM_MEM | XV_IMM_NONE),     /* group4/5 insns */

  /* two-byte opcode, 0x0f prefix */
  /* 0x00 - 0x0f */ XV_MODRM_MEM | XV_IMM_NONE,         /* group6 insns */
                    XV_MODRM_MEM | XV_IMM_NONE,         /* group7 insns */
                    R2(XV_MODRM_MEM | XV_IMM_NONE),     /* lar, lsl */
                    XV_INVALID,
                    R2(XV_MODRM_NONE | XV_IMM_NONE),    /* syscall, clts */
                    XV_MODRM_NONE | XV_IMM_NONE,        /* sysret */
                    R2(XV_MODRM_NONE | XV_IMM_NONE),    /* invd, wbinvd */
                    R2(XV_INVALID),
                    XV_INVALID,
                    XV_MODRM_MEM | XV_IMM_NONE,         /* prefetchw */
                    R2(XV_INVALID),

  /* 0x10 - 0x1f */ R16(XV_MODRM_MEM | XV_IMM_NONE),    /* AVX, prefetch */
  /* 0x20 - 0x2f */ R4(XV_MODRM_MEM | XV_IMM_NONE),     /* AVX */
                    R4(XV_INVALID),
                    R8(XV_MODRM_MEM | XV_IMM_NONE),     /* SSE, AVX */

  /* 0x30 - 0x3f */ R4(XV_MODRM_NONE | XV_IMM_NONE),    /* MSR */
                    R2(XV_MODRM_NONE | XV_IMM_NONE),    /* sysenter, sysexit */
                    XV_INVALID,
                    XV_MODRM_NONE | XV_IMM_NONE,        /* getsec */
                    R8(XV_INVALID),                     /* prefix, invalid */

  /* 0x40 - 0x4f */ R16(XV_MODRM_MEM | XV_IMM_NONE),    /* cmov */
  /* 0x50 - 0x5f */ R16(XV_MODRM_MEM | XV_IMM_NONE),    /* SSE, AVX */
  /* 0x60 - 0x6f */ R16(XV_MODRM_MEM | XV_IMM_NONE),    /* SSE, AVX */

  /* 0x70 - 0x7f */ R4(XV_MODRM_MEM | XV_IMM_I8),       /* SSE, AVX imm8 */
                    R2(XV_MODRM_MEM | XV_IMM_NONE),     /* SSE, AVX */
                    XV_MODRM_MEM | XV_IMM_NONE,         /* SSE, AVX */
                    XV_MODRM_NONE | XV_IMM_NONE,        /* emms */
                    R8(XV_MODRM_MEM | XV_IMM_NONE),     /* VMX, SSE, AVX */

  /* 0x80 - 0x8f */ R16(XV_MODRM_NONE | XV_IMM_DSZW),   /* long jcc */
  /* 0x90 - 0x9f */ R16(XV_MODRM_MEM | XV_IMM_NONE),    /* setcc */

  /* 0xa0 - 0xaf */ R2(XV_MODRM_NONE | XV_IMM_NONE),    /* push fs, pop fs */
                    XV_MODRM_NONE | XV_IMM_NONE,        /* cpuid */
                    XV_MODRM_MEM | XV_IMM_NONE,         /* bt */
                    XV_MODRM_MEM | XV_IMM_I8,           /* shld imm8 */
                    XV_MODRM_MEM | XV_IMM_NONE,         /* shld %cl */
                    R2(XV_INVALID),
                    R2(XV_MODRM_NONE | XV_IMM_NONE),    /* push gs, pop gs */
                    XV_MODRM_NONE | XV_IMM_NONE,        /* rsm */
                    XV_MODRM_MEM | XV_IMM_NONE,         /* bts */
                    XV_MODRM_MEM | XV_IMM_I8,           /* shrd imm8 */
                    XV_MODRM_MEM | XV_IMM_NONE,         /* shrd %cl */
                    XV_MODRM_MEM | XV_IMM_NONE,         /* group15 insns */
                    XV_MODRM_MEM | XV_IMM_NONE,         /* imul */

  /* 0xb0 - 0xbf */ R8(XV_MODRM_MEM | XV_IMM_NONE),     /* misc */
                    XV_MODRM_MEM | XV_IMM_NONE,         /* popcnt */
                    XV_INVALID,                         /* group10 invalid */
                    XV_MODRM_MEM | XV_IMM_I8,           /* group8 insns */
                    XV_MODRM_MEM | XV_IMM_NONE,         /* btc */
                    R4(XV_MODRM_MEM | XV_IMM_NONE),     /* bsf, bsr, movsx */

  /* 0xc0 - 0xcf */ R2(XV_MODRM_MEM | XV_IMM_NONE),     /* xadd */
                    XV_MODRM_MEM | XV_IMM_I8,           /* vcmpxx */
                    XV_MODRM_MEM | XV_IMM_NONE,         /* movnti */
                    R2(XV_MODRM_MEM | XV_IMM_I8),       /* SSE, AVX */
                    XV_MODRM_MEM | XV_IMM_NONE,         /* group9 insns */
                    R8(XV_MODRM_NONE | XV_IMM_NONE),    /* bswap */

  /* 0xd0 - 0xdf */ R16(XV_MODRM_MEM | XV_IMM_NONE),    /* SSE, AVX */
  /* 0xe0 - 0xef */ R16(XV_MODRM_MEM | XV_IMM_NONE),    /* SSE, AVX */
  /* 0xf0 - 0xff */ R16(XV_MODRM_MEM | XV_IMM_NONE),    /* SSE, AVX */

  /* three-byte opcode, 0x0f 0x38 prefix */
  R16(R16(XV_MODRM_MEM | XV_IMM_NONE)),                 /* SSE, AVX */

  /* three-byte opcode, 0x0f 0x3a prefix */
  R16(R16(XV_MODRM_MEM | XV_IMM_I8))                    /* SSE, AVX */
};

#undef R2
#undef R4
#undef R8
#undef R16

inline int xv_x64_immediate_bytes(xv_x64_insn const *const insn) {
  xv_x64_insn_encoding const enc = xv_x64_insn_encodings[xv_x64_insn_key(insn)];
  switch (enc & XV_IMM_MASK) {
    case XV_IMM_NONE: return 0;
    case XV_IMM_D8:
    case XV_IMM_I8:   return 1;
    case XV_IMM_I16:  return 2;
    case XV_IMM_D32:
    case XV_IMM_I32:  return 4;
    case XV_IMM_I64:  return 8;
    case XV_IMM_DSZW:
    case XV_IMM_ISZW: return insn->p66 ? 2 : 4;
    case XV_IMM_I2:   return 3;
    case XV_IMM_ISZQ: return insn->p66 ? 2 : insn->rex_w ? 8 : 4;
    default:          return 0;
  }
}

/* Basic blocks. */
/* Each basic block knows the translation address, and basic blocks are stored in */
/* a sorted structure so we can trace segfault addresses back to the corresponding */
/* piece of code. (This will happen anytime the code attempts to modify itself.) */

/* In addition, basic blocks provide flags, which indicate various possibly-useful */
/* properties. For example, we mark any basic block that doesn't have any */
/* memory-writing operations, and we mark those that make no system calls. */

/* We also need to keep track of the kind of jump the block uses. Each basic block */
/* will end for a specific reason, often because we encountered a jump */
/* instruction. We store the type of ending on the basic block so we can recover */
/* the target address without reparsing it. */

#define XV_NO_SYS   (1 << 0)    /* the basic block makes no system calls */
#define XV_NO_WRITE (1 << 1)    /* the basic block does not write to memory */

struct xv_x64_bblock {
  xv_x64_const_i *addr;         /* starting absolute address */
  ssize_t         size;         /* size (bytes) of original */
  xv_x64_const_i *end_insn;     /* beginning of jump instruction (0 if none) */
  xv_x64_const_i *t_addr;       /* translation cache address (0 if none) */
  uint64_t        flags;        /* see above for values */
};

struct xv_x64_bblock_list {
  xv_x64_bblock *blocks;        /* array of blocks, always sorted by addr */
  int            size;          /* number of used block entries */
  int            capacity;      /* total number of block entries */
};

int xv_bblock_list_init(xv_x64_bblock_list *bblock_list);
int xv_bblock_list_free(xv_x64_bblock_list *bblock_list);

/* Return index of basic block containing specified addr, -1 if none; runs in
 * O(log n) time */
int xv_bblock_idx(xv_x64_const_i           *addr,
                  const xv_x64_bblock_list *bblock_list);

/* Add bblock to list, maintaining sort order; runs in O(n) time */
int xv_add_bblock(xv_x64_bblock const *bblock,
                  xv_x64_bblock_list  *bblock_list);

/* Rewrite jump target addresses within specified bblock, causing code to jump
 * directly to other already-rewritten basic blocks rather than into
 * untranslated code (which would cause a segfault that we would intercept) */
int xv_link_bblock(xv_x64_bblock_list *bblock_list,
                   int                 idx);

/* ELF reader. */
/* All of the machine code coming into the rewriter is probably going to be */
/* encased in ELF objects. This is useful for us because we can easily map the ELF */
/* into memory, then use the symbol table to build a list of basic blocks. */

/* Some (admittedly rare and bizarre) ELF objects have invalid symbol tables or */
/* other such nonsense. In this case, we can use the entry point (for */
/* executables), or we can wait for code to be executed and catch the resulting */
/* segfault, having then discovered the address of a basic block or fragment. */

/* Add all basic blocks from in-memory ELF image; idempotent, and runs in
 * O((k + n)²) time, where k = #bblocks in ELF and n = size of bblock list */
int xv_elf_into_bblocks(const void         *elf_image,
                        xv_x64_bblock_list *bblock_list);

#endif

/* Generated by SDoc */
