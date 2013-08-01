## Syscalls
xv needs to intercept system calls in order to maintain a list of memory
mappings and to prevent the program from actually trapping segfaults. (And for
virtualization, obviously.)

## Segfaults
We need to capture segfaults to implement user-mode paging and self-modifying
code detection.

## CPUID
A tragedy. We need to checkpoint the process anytime it makes a CPUID call,
just in case the process is then migrated to another machine whose processor is
binary-incompatible (this happens with recent Intel-vs-AMD instructions).
