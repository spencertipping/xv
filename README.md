# `xv`: Executable virtualization
`xv` is a process-level virtualizer for Linux/x86-64. It runs unmodified
binaries without root, intercepting and reinterpreting system calls. Most
programs will not detect `xv`, nor will they escape virtualization, even
through things like JIT. However, pathological cases can escape, so `xv` is not
a safe way to run untrusted code.

## Usage
```
xv [-m|-r|-v|...] command [args...]
```

For example:

```sh
$ xv -m ls.log ls               # m = memoize execution of ls
foo  bar
$ rm foo bar
$ xv -r ls.log ls               # r = replay execution
foo  bar
$
```

`xv` supports other options too; see `xv (1)` for the complete list.

## Virtualization internals
`xv` reimplements the Linux ELF loader and process-level memory mapping,
intercepting segfaults to transform code on-demand. In this case the code is
transformed in-place by rewriting all system call instructions into illegal
instructions of the same size:

```s
...
movb $1, %rax
syscall             # xv changes this to lea (%rax), %rax
sysenter            # same here
int $0x80           # same here
...
```

As a result, the process will receive SIGILL when it attempts to make a system
call. `xv` traps this signal, reading the user context and emulating the
syscall (or passing it straight through).

Code rewriting happens lazily, since mechanisms like JIT will defeat an
ahead-of-time rewriting pass. `xv` does this by removing the execute permission
from all mapped memory pages, causing a segfault as soon as execution begins.
At this point we can observe the address that caused the fault, which gives us
a base offset to rewrite things. `xv` then rewrites all statically-reachable
code, transforming any syscalls into illegal instructions. Any rewritten page
is set as executable, and the process continues. If there is any overlapping
code (a pathological case), `xv` throws an error and exits.

`xv` preserves the invariant that no page is writable and executable at the
same time. This mostly prevents the program from escaping virtualization
through self-modifying code, though there are still some loopholes. For
example, the program could map the same file into two separate addresses, using
one region for writing and the other for execution. It could then undo `xv`'s
transformations, allowing it to run system calls normally. For this reason,
`xv` should not be used to run untrusted programs.

Because `xv`'s own memory shares an address space with the virtualized process,
`xv` manages its pages by maintaining a further invariant: anytime it runs code
that belongs to the virtualized process, all but one of its pages have no
permissions. The remaining page contains the signal handlers and a pointer to
the other pages (which always exist in a contiguous region).

### Issues
`xv`'s in-place code transformation breaks in some interesting ways when the
process maps an executable file in SHARED mode. The problem is that we end up
committing `xv`'s code changes to disk, making the resulting file impossible to
execute due to SIGILL and other errors. (Alternatively, if the user can't write
to the file, then the mapping request will fail altogether.)

In our case, the workaround is to map the file in PRIVATE mode. This eliminates
our ability to modify the resulting file, which should pose a problem only in
very strange cases.
