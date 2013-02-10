XV Linux/x86-64 build | Spencer Tipping
Licensed under the terms of the MIT source code license

# Bootup code

The handoff here is kind of strange, but it helps partition the rest of the
source into header files. The problem is that TinyELF runs the first concrete
function, so if we were to do the normal thing of including headers first, then
those headers wouldn't be able to define concrete functions.

As it is, we can define a stub `_start` function that kicks off to the
forward-defined `xv_main`, then put `#include` directives after this.

    void xv_main(void *initial_stack);
    void _start(void) {
      void *initial_rsp;
      asm("movq %%rbp, %0;" : "=r"(initial_rsp));

      /* We can't use an absolute jump to force the tail call. If we did, then the
       * resulting executable might depend on a GOT to be relocatable, and I don't
       * want to have to rewrite that each time we move XV in memory. The quick
       * workaround is to save the original stack pointer and hand it to xv_main so
       * that we have access to the calling context whether GCC converts the tail
       * call or not. */
      xv_main(initial_rsp);
    }

# Library inclusion

At this point we can load the header files.

    #include <asm/unistd.h>

    #include "xv-x64-linux-syscall.h"
    #include "xv-x64-virt.h"
    #include "xv-generic.h"

# Main entry point

We need to parse argv, argc, and the environment variables. These will
ultimately be handed to the subprocess, but only once we've consumed some of
them.

    void xv_main(void *initial_stack) {

    }

# End header

This is required for XV to be able to figure out how large its image is in
memory. See `xv-end.h` for details about this.

    #include "xv-end.h"