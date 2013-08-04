XV process virtualizer
Copyright (C) 2013, Spencer Tipping
Released under the terms of the GPLv3: http://www.gnu.org/licenses/gpl-3.0.txt

# Introduction

xv uses dynamic compilation to virtualize the execution of a process. It
rewrites system calls, replacing them with calls into libxv functions.

```c
#include <elf.h>
#include <sys/mman.h>
```

```c
#include "xv.h"
#include "xv-x64.h"
```

```c
void xv_start() {
  /* TODO */
}

```
