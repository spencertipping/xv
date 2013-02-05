# `xv`: Execution virtualization

`xv` is a Linux process virtualizer that works similarly to `valgrind` but is
designed to maximize performance. To do this, it rewrites the machine code of
the process as it is running, replacing certain system call instances with
calls into userspace.

This transformation will be transparent for the vast majority of programs,
including those compiled without libc. However, there are certain pathological
cases in which programs can detect that they are being virtualized:

1. A program that retrieves the address of the `rip` register can observe that
   the address space has been shifted in memory.
2. A program whose data structures are executable x86 machine code will incur a
   performance hit anytime these structures are modified and executed again.
3. Code cache locality is not guaranteed.

## Usage

    xv --module1 --opt=x --opt=y --module2 --opt=z ... command [arguments...]

Some presets, which can be combined using normal short-option syntax:

    $ xv -n ls          # short for xv --no-side-effects ls
    $ xv -v ls          # short for xv --trace ls
    $ xv -m ls          # short for xv --memoize ls
    $ xv -c ls          # short for xv --cow ls
    $ xv -cv ls         # xv --cow --trace ls
    $ xv -vc ls         # xv --trace --cow ls (mostly useful for debugging xv)

More detailed examples:

    $ xv --httpfs cat http://google.com         # like curl http://google.com
    $ xv --cow vim README.md                    # diverges using xvcow.log
    $ xv --cow --log=foo.log vim README.md      # diverges using foo.log
    $ xv --trace find                           # show syscalls (like strace)

`xv` is sensitive to argument order; options parameterize the most recent
module. Modules themselves are order-sensitive and work like function
composition; `xv --a --b --c foo` runs `foo` transformed by `c`, then that
result by `b`, then that result by `a`.

## Filesystem modules

### `httpfs` module

Extends the semantics of `open (2)` to return special file descriptors that
represent HTTP entities. Any filename that begins with `http://` or `https://`
is handled; all others are passed through to the existing `open` handler.

Obviously HTTP as such doesn't have much hope of being POSIX-compliant, and
`httpfs` doesn't make any effort to change that. Its primary utility is to
provide basic read/write functionality for HTTP-loaded resources. There are
three basic ways to do this:

    --write=none (default)
    --write=post
    --write=put

`--write` specifies which HTTP method to use when committing file changes. The
entire file must be rewritten, and it always behaves as though `O_CREAT` has
been specified (since HTTP provides no `lstat (2)` equivalent).
