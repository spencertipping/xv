# Call/return emulation
xv can't assume any normal (i.e. C-style) calling convention on the part of the
virtualized process. (If we were willing to assume any kind of binary
compatibility with C, we'd just replace libc with `LD_PRELOAD` instead of
rewriting every byte of the program.) As a result, we have to be very careful
anytime we change any address that the program may use.

Preserving the original code location is out of the question, as the
instructions we generate won't be the same size as the originals. So the
program needs to run from a different place in memory but still do most of its
work with original addresses.

The only drawback to this approach is that it is very expensive to convert
original addresses to translated ones, especially if the mechanism involves
segfaulting.
