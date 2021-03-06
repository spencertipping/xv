XV linker script
Copyright (C) 2013, Spencer Tipping
Released under the terms of the GPLv3: http://www.gnu.org/licenses/gpl-3.0.txt

# Introduction

XV doesn't behave like a normal program. In particular, it moves itself to
accommodate memory mapping requests made by the application. This is a
necessary evil of sharing an address space with the virtualized process.

Note that this dynamic relocation has all kinds of implications throughout xv's
code. In particular, every piece of global state must have some kind of
well-defined semantics around being moved; for most data structures, we do a
structured copy operation to reinitialize at the new location. This means that
we implicitly have a copying garbage collector, a fact that we use to clean out
the translation cache.

```x
ENTRY(xv_start)
SECTIONS
{
  . = 0x20000;
  .text : {
    xv_image_start = .;
    *(.text)
  }
  .data : {
    *(.data)
    *(.bss)
    xv_image_end = .;
  }
}

```
