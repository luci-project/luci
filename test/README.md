Luci test cases
===============

To test `ld_luci.so` with dynamic updates enabled, simply run

    ./run.sh -u

in this `test` folder.
This will build, run and check the [default test cases](test/default) with your systems C/C++ compiler.
You can specify the test cases (directory names) as arguments and regex, e.g., `1-fork 1-ifunc "2-.*"`.
To use LLVM, append the `-c CLANG` flag.
Omit `-u` to run *Luci* without dynamic updates.
In case you want to see internals of *Luci*, append `-o -v 6` to show the debug output.
Parameter `-h` will list all available options.

> **Please note:** When testing dynamic updates, the shared libraries are executed after a specific time.
> Since we try to reduce the runtime of the test cases, we have only little margin for scheduling delays.
> On systems with high load, it can happen that updates will be applied too late, which will cause a different output and result into a failed test case.
> In such a case, you could either try to reduce the load on your system or slightly modify the test cases to allow more flexibility.

To run the test cases on a different (supported) distribution, first make sure that you have built all versions of *Luci* and then use the docker helper script — it will pull the official image, install all required dependencies (compiler and build utilities) and execute the provided command in the mounted *Luci* directory.
For example, run certain tests with AlmaLinux:

    make all
    ../tools/docker.sh almalinux:9 ./test/run.sh -u "2-.*"

If you want to test *Luci*'s dynamic update capabilities with different languages like Ada, Fortran, Go, Pascal, and Rust, make sure to have the correspondent compilers installed.
On Debian/Ubuntu, install them using

    apt install fpc gnat gfortran golang gccgo rustc

and run the [lang test cases](test/lang) with

    ./run.sh -g lang -u

> **Please note:** Some test cases are allowed (or even expected) to fail — they contain a `.mayfail` file in their folder, preventing a fatal exit of the test suite.
> For example, programs written in Go are not supposed to load shared libraries in Go (not related to the RTLD) since this would cause the runtime to be loaded twice. Depending on the Go version and the outcome of some racy code, test case `1-go` might work or might fail.
