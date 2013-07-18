# `xv`: x86-64 process-level virtualization
`xv` is an x86-64 dynamic compiler that virtualizes a process by rewriting all
system call instructions into managed function calls. These function calls are
expected to wrap the system calls, providing custom behavior.

## Usage
```sh
$ xv /bin/ls            # runs /bin/ls normally
$ xv -v /bin/ls         # traces rewriting and system call emulation
```

Further options are described in `xv (1)`.
