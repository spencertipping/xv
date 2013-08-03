# Intercepts
Things about a process that xv needs to continuously intercept.

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

There may not be much we can do about this situation in practice. The best can
do is probably to checkpoint as indicated, then rewind anytime we get a
legitimate SIGILL (i.e. something that xv can parse, but that the underlying
processor can't).

This feature is somewhat involved, so not implemented for a while.
