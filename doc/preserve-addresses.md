# Preserving original code addresses
Linux provides the `mremap` system call, which allows xv to move an
already-mapped region from one address to another while preserving any
attachment it has to other processes. This means that we can remap any existing
memory region to another location, copy executable code from that region, and
translate it in the process.

The advantage of this is that the code will retain its original location,
meaning that no stack rewriting is necessary even if xv is relocated.
