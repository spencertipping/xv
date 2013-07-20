# `xv`: x86-64 process-level virtualization
**NOTE: This project is nowhere near finished, nor is it likely to be for some
time.** And when it does start working, it will probably be awful for another
long time. It might never work because it might not be possible.

`xv` is an x86-64 dynamic compiler that virtualizes a process by rewriting its
machine code to capture system calls.

## Usage
```sh
$ xv /bin/ls            # runs /bin/ls normally
$ xv -v /bin/ls         # traces rewriting and system call emulation
```

Further options are described in `xv (1)`.
