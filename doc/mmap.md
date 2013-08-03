# Memory map virtualization
Most `mmap` calls end up passing through, but with some important restrictions.
No memory mapped by the running process can be executable. Instead, we need to
use segfault-driven JIT to translate any code mapped into an executable region.

xv needs to track all of these mappings for two reasons. First, we need to know
whether a segfault in one of these mappings is legitimate (i.e. whether we
caused it, or whether the program really is broken); and second, we need to
know where we can put our translation cache.

## Translation cache placement
We can't put the translation cache just anywhere, at least not easily. The
problem is that it's probably going to make %rip-relative references to memory,
and x86-64 limits the displacement to a signed int32. So the translation cache
should be within 2GB of any memory it accesses. (We could use absolute
register-indirect addressing, but then we have to negotiate storing/loading the
register to restore its value.)
