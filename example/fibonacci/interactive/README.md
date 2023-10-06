Example for interactive programming
===================================

Demonstration of *Luci*s interactive programming capabilities using relocatable object files.
A detailed description of the different functions calculating Fibonacci sequence can be found in the [parent directory](../README.md)
The [`main.o`](main.c) object will use any linked `fib.o` object to calculate Fibonacci numbers, the second binary [`run-measure`](measure.c) works in a similar way but has the ability to measure the duration of each `fib` function call.


Quick Start
-----------

Run

    ./demo.sh

to compile all source files and runtime-link the `measure.o` object, while automatically replacing the fibonacci function object (→ dynamically updating it) every 10 seconds with another version in random order.

Running

    ./demo-lib.sh

will not link against the fibonacci function object, but use a static library (`libfib.a`) instead, while automatically replacing the library file every 10 seconds.

In both cases, the scripts performs the tasks described below.


Preparation
-----------

Build object files for each versions with

    make

This will create two runnable object files (containing the `main` function) and each version of the fibonacci function. In addition, it will (prefixed with `run-`) in an subfolder for each version, having all symbols stripped from the binaries.
In addition, it creates a symlink to the binaries of the first version in the base directory.

You can also change the compiler (e.g., `make CC=clang` for LLVM) or adjust the compiler flags (`CFLAGS=...`)
Have a look at the [Makefile](Makefile) for more details.


After the compilation, *Luci* can be used as static linker and loader using the parameter `-s`.
Hence, you can start it using the first version with

    /opt/luci/ld-luci.so -s fib_1/fib.o main.o -lc

(the `-lc` is required to link against the c standard library).

Another option is to use the static library instead of the object file:

    /opt/luci/ld-luci.so -s main.o -lc -Lfib_1 -lfib

The output will look similar to

    WARNING      0 25511  luci:src/object/dynamic.cpp:204 ........... Library 'libdl.so.2' will be skipped (exclude list)
    WARNING      0 25511  luci:src/object/dynamic.cpp:204 ........... Library 'ld-linux-x86-64.so.2' will be skipped (exclude list)
     ERROR       0 25511  luci:src/loader.cpp:154 ................... Library 'libfib.so' cannot be found
    WARNING      0 25511  luci:src/comp/gdb.cpp:58 .................. GDB debug structure not assigned in target
    WARNING      0 25511  luci:src/comp/glibc/rtld/audit.cpp:10 ..... GLIBC _dl_audit_preinit not implemented!
    fib(0) = 0
    [using Fibonacci v1: O(2^n))]
    fib(1) = 1
    [using Fibonacci v1: O(2^n))]
    fib(2) = 1
    [using Fibonacci v1: O(2^n))]

(the `WARNING` output on standard error is produced by *Luci* and can be ignored, similar to the `ERROR` which will appear when linking against a static library - since it first checks for a shared library)

When calculating the 50th+ Fibonacci number, this version will take a significant amount of time (several dozens of seconds).


Dynamic Updates
---------------

Now copy the object file `fib.o` into the main directory and restart with dynamic updates enabled:

    cp fib_1/fib.o .
    /opt/luci/ld-luci.so -u -s fib_1/fib.o main.o -lc -lm

(the `-lm` instructs to link against `libm.so` as well, which will be required for the 5th example)

After a certain time in a different terminal window (but same working directory), simulate an *update* of the Fibonacci object by compiling another version

    gcc -O2 -g -o fib.o fib_2.c

Of course you could also copy another compiled version of the object file instead:

    cp -f fib_2/fib.o .


Another option is to run the example using the static library `libfib,a` instead:

    cp fib_1/libfib.a .
    /opt/luci/ld-luci.so -u -s fib_1/fib.o main.o -lc -lm -L. -lfib

and replacing the static library after some time with another version:

    cp -f fib_2/libfib.a .


In all cases, *Luci* will now detect the change, checking if the new version is compatible with the old one, and then load and relink it.
If a call to `fib` is executed during the update, it will finish in function version 1, but subsequent calls will be made in the corresponding function of library version 2 - you should notice a significant performance boost and a different output in the lines starting with square brackets:

    fib(43) = 433494437
    [using Fibonacci v1: O(2^n))]
    fib(44) = 701408733
    [using Fibonacci v1: O(2^n))]
    fib(45) = 1134903170
    [using Fibonacci v2: O(n)]
    fib(46) = 1836311903
    [using Fibonacci v2: O(n)]

In the same way, you can also change to any other version (e.g., `cp -f fib_3/fib.o .`).
It is not necessary to sequentially increase the versions; you can directly apply version 6. For example, here an output running `measure.o` with two consecutive updates:

    fib(44) = 701408733 (in 1.574935s)
    [using Fibonacci v1: O(2^n))]
    fib(45) = 1134903170 (in 2.627559s)
    [using Fibonacci v1: O(2^n))]
    fib(46) = 1836311903 (in 4.181815s)
    [using Fibonacci v2: O(n))]
    fib(47) = 2971215073 (in 0.000001s)
    [using Fibonacci v2: O(n))]
    fib(48) = 4807526976 (in 0.000038s)
    [using Fibonacci v6: O(1)]
    fib(49) = 7778742049 (in 0.000001s)
    [using Fibonacci v6: O(1)]

If you want to see further details about the dynamic linking, increase the log level  (e.g., parameter `-v 6` for debugging).


Going further
-------------

Feel free to modify the libraries and test the capabilities of *Luci*.
In contrast to the other examples, *Luci* will tolerate changes to the writable data:

For example, changing the automatic variable (= located on stack) `f` in `fib_4.c` to static (= located in `.data`)

    --- a/example/fib_4.c
    +++ b/example/fib_4.c
    @@ -26,7 +26,7 @@ static void power(unsigned long f[4], unsigned long n) {
     unsigned long fib(unsigned long value) {
            if (value < 2)
                    return value;
    -       unsigned long f[4] = {1, 1, 1, 0};
    +       static unsigned long f[4] = {1, 1, 1, 0};

                    power(f, value - 1);
            return f[0];

is valid.

**Please note:** The *Bean* tools to analyze the binaries do not work properly with relocatable object files.
