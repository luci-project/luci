Luci example
============

This directory contains three versions of a library calculating fibonacci numbers:

 * [`fib_1`](fib_1.c) uses the well-known recursive algorithm with exponential time complexity O(n²)
 * [`fib_2`](fib_2.c) uses an iterative algorithm with linear time complexity O(n), which is noticeably faster.
 * [`fib_3`](fib_3.c) uses a matrix algorithm with logarithmic time complexity O(log(n)) - while there shouldn't be any perceptible difference compared to the iterative approach, it demonstrates that new helper functions can be introduced as well.

(versions based on [geeksforgeeks.org](https://www.geeksforgeeks.org/program-for-nth-fibonacci-number/))


The [`main`](main.c) binary will use this library to calculate fibonacci numbers (endless loop, but only the first 93 values will be valid due to overflows), with a second delay inbetween (to releax the CPU).
There is a second binary [`main_measure`](main_measure.c). without the delay but measuring the (clock) time of the library function call, which can be used instead.

Build all libraries and binaries with

    make

(you could also specify your compiler, e.g. `make CC=clang` for LLVM)

If *Luci* is installed to `/opt/luci/ld-luci.so`, the `main` binary will automatically use it as interpreter, hence, you can start it as you are used to:

    ./main

Otherwise you have to explicitly specify the *Luci* RTLD:

    ../ld-luci-ubuntu-focal.so main

The `main` binary will use `libfib.so`, which is by default a symbolic link to the first version of the library (`fib_1`), hence the output will look similar to

    WARNING      0 480031 luci:src/object/dynamic.cpp:200 ........... Library 'ld-linux-x86-64.so.2' will be skipped (exclude list)
    WARNING      0 480031 luci:src/object/dynamic.cpp:200 ........... Library 'libdl.so.2' will be skipped (exclude list)
    WARNING      0 480031 luci:src/comp/glibc/rtld/audit.cpp:10 ..... GLIBC _dl_audit_preinit not implemented!
    fib(0) = 0
    [using Fibonacci library v1: O(2^n))]
    fib(1) = 1
    [using Fibonacci library v1: O(2^n))]
    fib(2) = 1
    [using Fibonacci library v1: O(2^n))]

(the `WARNING` output on standard error is produced by *Luci* and can be ignored)

When calculating the 50th fibonacci number, this version will take a significant amount of time (several dozens of seconds).

Now abort the program and restart it with dynamic updates enabled:

    LD_DYNAMIC_UPDATE=1 ./main

After a certain time in a different terminal window (but same working directory), simulate an *update* of the fibonacci library by changing the symbolic link:

    ln -sf libfib_2.so libfib.so

*Luci* will detect the change, check the library for compatibility and then load and relink it.
If a call to `fib` is executed, it will finish in library version 1, but the subsequent call will be made in libray version 2 - you should notice a significant performance boost and a different output on standard error:

    fib(43) = 433494437
    [using Fibonacci library v1: O(2^n))]
    fib(44) = 701408733
    [using Fibonacci library v1: O(2^n))]
    fib(45) = 1134903170
    [using Fibonacci library v2: O(n)]
    fib(46) = 1836311903
    [using Fibonacci library v2: O(n)]

In the same way you can also change to version 3 (`ln -sf libfib_3.so libfib.so`).
It is not necessary to sequentially switch the versions, you can directly apply version 3 (here an example running `main_measure`):

    fib(43) = 433494437 (in 1.122244902ns)
    [using Fibonacci library v1: O(2^n))]
    fib(44) = 701408733 (in 1.761778022ns)
    [using Fibonacci library v1: O(2^n))]
    fib(45) = 1134903170 (in 3.106040543ns)
    [using Fibonacci library v3: O(log(n))]
    fib(46) = 1836311903 (in 0.000000178ns)
    [using Fibonacci library v3: O(log(n))]

However, a rollback to a previously loaded library version is disabled by default - to work around this limitation, you have to set the environment variable `LD_FORCE_UPDATE=1`.

If you want further details about the dynamic linking, increase `LD_LOGLEVEL` (6 for debugging).

Feel free to modify the libraries and test the capabilities of *Luci*.
The `bean` utilities might help you to understand certain compatibility issues in libraries:

    bean-diff -vvv -r -d libfib_1.so libfib_3.so

(in case the util is not in `PATH` or the assembly output is missing, run `make -C ../bean install -B`!)

