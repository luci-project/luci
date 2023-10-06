Fibonacci Examples
==================

The [source folder](_source/) contains six versions of a function calculating the Fibonacci sequence, all sharing the [same interface (API)](_source/fib.h):

 * [`fib_1`](_source/fib_1.c) uses the well-known recursive algorithm with exponential time complexity O(n²)
 * [`fib_2`](_source/fib_2.c) uses dynamic programming with linear time complexity O(n), which is noticeably faster.
 * [`fib_3`](_source/fib_3.c) employs a more space-efficient iterative algorithm with linear time complexity O(n).
 * [`fib_4`](_source/fib_4.c) is based on a matrix algorithm with logarithmic time complexity O(log(n)) – while there shouldn't be any perceptible difference compared to the iterative approach, this example demonstrates that new helper functions can be introduced as well.
 * [`fib_5`](_source/fib_5.c) implements Binet's formula (using the golden ratio) and therefore has constant time complexity O(1) – since this example requires math functions like square root, the binary needs to be linked against `libm.so`, therefore demonstrates *Luci*'s ability to handle versions with changed dependencies or its usage of new external functions.
 * [`fib_6`](_source/fib_6.c) contains a lookup table with all relevant Fibonacci sequence numbers, having a constant time complexity O(1) while being slightly faster than the previous version. This version illustrates that changing/introducing (non-writable) data does not pose an issue for *Luci*.

(all versions are based on the examples at [geeksforgeeks.org](https://www.geeksforgeeks.org/program-for-nth-fibonacci-number/))

The [main](_source/main.c) function has to be compiled with one of these files to calculate Fibonacci numbers in endless loop (but only the first 93 values will be valid due to overflows).
To relax the CPU, it will wait a second in between calculating the next number (can be changed/disabled using the `DELAY` macro).

A slightly modifified version, [`measure`](measure.c), has the ability to measure the duration of the `fib` function call, which highlights the effect of changed versions.


These files are used in a very similar way in every example, hence demonstrating the capabilities of *Luci*:

 * [Interactive programming](interactive/) using only relocatable object files with *Luci* as static linker (and loader). When the object files are changed during runtime, *Luci* wil detect the changes, load the new version and link it accordingly. The folder also includes an example with static libraries changed during runtime.
 * [Shared library](shared-library/) containing the fibonacci function and dynamically linked by the application binary. *Luci* detects changes to the shared object and updates it.
 * [Static library](static-library/) containing the fibonacci function and statically linked into the application binary. During runtime, changes to application binary (linked against different library version) are detected and updated by *Luci*.
 * [Stripped binary](stripped-binary/) of the application binary compiled with a fibonacci function, with all symbol information stripped from the resulting binary file. If the application binary gets replaced, *Luci* detects the change, reconstructs the symbols and relocations and updates the file dynamically.


Quick Start
-----------

Each of the example folder contains a

    ./demo.sh

to build and run the `measure` application, automatically replacing the binary (→ dynamically updating it) every 10 seconds with another version in random order.
The steps performed by the script are described in further detail in each folders `README.md` file.


Preparation
-----------

Make sure to have [*Luci*](../..) installed on your system.

In each example folder, build the objects/libraries/binaries for each versions with

    make

You can also change the compiler (e.g., `make CC=clang` for LLVM), adjust the compiler flags (`CFLAGS=...`), or specify a different RTLD (`LD_PATH=`).
Have a look at the corresponding `Makefile` for more details.

The `main` applicatiion is by default linked to the first version of the library (`fib_1`), hence the output will look similar to

    WARNING      0 480031 luci:src/object/dynamic.cpp:200 ........... Library 'ld-linux-x86-64.so.2' will be skipped (exclude list)
    WARNING      0 480031 luci:src/object/dynamic.cpp:200 ........... Library 'libdl.so.2' will be skipped (exclude list)
    WARNING      0 480031 luci:src/comp/glibc/rtld/audit.cpp:10 ..... GLIBC _dl_audit_preinit not implemented!
    fib(0) = 0
    [using Fibonacci v1: O(2^n))]
    fib(1) = 1
    [using Fibonacci v1: O(2^n))]
    fib(2) = 1
    [using Fibonacci v1: O(2^n))]

(the `WARNING` output on standard error is produced by *Luci* and can be ignored)

When calculating the 50th+ Fibonacci number, this version will take a significant amount of time (several dozens of seconds).


Dynamic Updates
---------------

Start the same examples with dynamic updates enabled, and after a certain time in a different terminal window (but same working directory), simulate an *update* of the Fibonacci binary by changing the version.

*Luci* will automatically detect the change, checking if the new version is compatible with the old one, and then load and relink it.
If a call to `fib` is executed during the update, it will finish in function version 1, but subsequent calls will be made in the corresponding function of version 2 - you should notice a significant performance boost and a different output in the lines starting with square brackets:

    fib(43) = 433494437
    [using Fibonacci v1: O(2^n))]
    fib(44) = 701408733
    [using Fibonacci v1: O(2^n))]
    fib(45) = 1134903170
    [using Fibonacci v2: O(n)]
    fib(46) = 1836311903
    [using Fibonacci v2: O(n)]

In the same way, you can also change to any other version
It is not necessary to sequentially increase the versions; you can directly apply version 6.
For example, here an output running `measure` with two consecutive updates:

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

If you want to see further details about the dynamic linking, increase the log level (e.g., `6` for debugging).


Going further
-------------

Feel free to modify the libraries and test the capabilities of *Luci*.
Sooner or later, you might stumble across an incompatibility, at which point it starts getting interesting:

For example, if we change the automatic variable (= located on stack) `f` in `fib_4.c` to static (= located in `.data`)

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

the writable data segment would change and updates on binaries are no longer possible using *Luci* (except interactive programming).

The `bean` utilities might help you to detect and understand certain compatibility issues in libraries:

    bean-update -v -r fib_3/run-main fib_4/run-main

will output an overview of all changes (together with internal ID used in *Luci*)

    # Changes at update of fib_3/run-main (14472 bytes) with fib_4/run-main (14520 bytes)
    {ID               ID refs         } [Ref / Rel / Dep] - Address              Size Type     Bind  Flag Name (Section)
    {a96c9348b62381e0 0000000000000000} [  0 /   0 /   0] - 0x0000000000000360     36 unknown  local      (.note.gnu.build-id)
    {8178fd2d97355493 0000000000000000} [  0 /   0 /   0] - 0x00000000000003d0    288 unknown  local      (.dynsym)
    {60a97facd7581c26 0000000000000000} [  0 /   0 /   0] - 0x0000000000000648    216 unknown  local      (.rela.dyn)
    {1b006775bb8aab8f 6ccf0fce4a000671} [  9 /   0 /   1] - 0x00000000000010e0    192 unknown  local   X  (.text)
    {cd8b788b02cbc8d3 42bac54717955c64} [  5 /   0 /   1] - 0x0000000000001240     64 function local   X  (.text)
    {bc9f7c897eeb0c7b 3be02abb5f607179} [  4 /   0 /   2] - 0x0000000000001290    128 function local   X  (.text)
    {910fe8de5dfd944f 51d5ae8ee5fbdf50} [  3 /   0 /   1] - 0x0000000000001310    240 function local   X  (.text)
    {483b5dee36bb6642 a907d4b4b7d4a118} [  4 /   0 /   1] - 0x0000000000001400    240 function local   X  (.text)
    {b09c6c5361cb1c64 8f6cdc2ea56ec44e} [  2 /   0 /   1] - 0x00000000000014f0     28 function local   X  (.text)
    {dedafdd7038b12cc 0000000000000000} [  0 /   0 /   4] - 0x0000000000002000    136 unknown  local      (.rodata)
    {463ace204730f11c 0000000000000000} [  0 /   0 /   0] - 0x0000000000002088     84 unknown  local      (.eh_frame_hdr)
    {1243de798256cb2a 0000000000000000} [  0 /   0 /   0] - 0x00000000000020e0    408 unknown  local      (.eh_frame)
    {e9ffa43124efbcc4 0000000000000000} [  0 /   0 /   0] - 0x0000000000003d98    512 unknown  local  R   (.dynamic)
    {e2b0da5b11a358fa 32bfc2ec313a9228} [  1 /   1 /   5] - 0x0000000000004008     56 unknown  local  W   (.data)
    {bb836acb4dcf3782 0000000000000000} [  0 /   0 /   0] - 0x0000000000004040      8 object   global W   stdout (.bss)
    {82f374cece14ee33 0000000000000000} [  0 /   0 /   1] - 0x0000000000004048      8 unknown  local  W   (.bss)

    # Critical sections have changed - not updateable...

If you want to dig deeper, try

    bean-diff -vvv -r -d fib_3/run-main fib_4/run-main

for a detailed output

    -unnamed 8 bytes @ 0x4008, .data [rw-]:
    -          0x4000                          08 40 00 00 00 00 00 00  # 0x4008 <0x4008/.data>
    -  1 Reference
    -     0x4008 <0x4008/.data> {91bee412b1d974db 44da30a2570fded5}
    -  1 Relocation
    -     *0x4008 = R_X86_64_RELATIVE <0x4008> {91bee412b1d974db 44da30a2570fded5} +16392
    -  2 depending on this
    -     0x1180 <0x1180/.text> {cd8b788b02cbc8d3 f2f9ba74be81c5e2}
    -     0x4008 <0x4008/.data> {91bee412b1d974db 44da30a2570fded5}
    -  ID: {91bee412b1d974db 44da30a2570fded5}

    +unnamed 56 bytes @ 0x4008, .data [rw-]:
    +          0x4000                          08 40 00 00 00 00 00 00  # 0x4008 <0x4008/.data>
    +          0x4010  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
    +          0x4020  01 00 00 00 00 00 00 00 01 00 00 00 00 00 00 00
    +          0x4030  01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
    +  1 Reference
    +     0x4008 <0x4008/.data> {e2b0da5b11a358fa 32bfc2ec313a9228}
    +  1 Relocation
    +     *0x4008 = R_X86_64_RELATIVE <0x4008> {e2b0da5b11a358fa 32bfc2ec313a9228} +16392
    +  3 depending on this
    +     0x1180 <0x1180/.text> {cd8b788b02cbc8d3 42bac54717955c64}
    +     0x1368 <0x1368/.text> {ade7733b87fcf92a 4a588b9c9b2ab8da}
    +     0x4008 <0x4008/.data> {e2b0da5b11a358fa 32bfc2ec313a9228}
    +  ID: {e2b0da5b11a358fa 32bfc2ec313a9228}


Due to introduction of the new variable `f` in `.data`, the layout of the writable section has changed, and certain symbols have now a different page relative address.
(In case the utilities above are not in `PATH` or the assembly output is missing, read Beans build instructions and run `make -C ../bean install -B`!)


**But how does this affect updates?**

The `bean-compare` script offers you a quick overview about the compatibility of all library version:

     bean-compare -m -r .

on default library build outputs

    ├── run-main
    └── run-measure
                                     ./run-main
    ┏━━━━━━━━━┳━━━━━━━━━━┳━━━━━━━━━━┳━━━━━━━━━━┳━━━━━━━━━━┳━━━━━━━━━━┳━━━━━━━━━━┓
    ┃ Package ┃  fib_1   ┃  fib_2   ┃  fib_3   ┃  fib_4   ┃  fib_5   ┃  fib_6   ┃
    ┡━━━━━━━━━╇━━━━━━━━━━╇━━━━━━━━━━╇━━━━━━━━━━╇━━━━━━━━━━╇━━━━━━━━━━╇━━━━━━━━━━┩
    │ fib_1   │ (update) │ update   │ update   │ restart  │ update   │ update   │
    │ fib_2   │ update   │ (update) │ update   │ restart  │ update   │ update   │
    │ fib_3   │ update   │ update   │ (update) │ restart  │ update   │ update   │
    │ fib_4   │ restart  │ restart  │ restart  │ (update) │ restart  │ restart  │
    │ fib_5   │ update   │ update   │ update   │ restart  │ (update) │ update   │
    │ fib_6   │ update   │ update   │ update   │ restart  │ update   │ (update) │
    └─────────┴──────────┴──────────┴──────────┴──────────┴──────────┴──────────┘
    Summary: For run-main, 3 of 5 versions (60%) can be live patched with default (extended) comparison and internal relocation resolving!

                                    ./run-measure
    ┏━━━━━━━━━┳━━━━━━━━━━┳━━━━━━━━━━┳━━━━━━━━━━┳━━━━━━━━━━┳━━━━━━━━━━┳━━━━━━━━━━┓
    ┃ Package ┃  fib_1   ┃  fib_2   ┃  fib_3   ┃  fib_4   ┃  fib_5   ┃  fib_6   ┃
    ┡━━━━━━━━━╇━━━━━━━━━━╇━━━━━━━━━━╇━━━━━━━━━━╇━━━━━━━━━━╇━━━━━━━━━━╇━━━━━━━━━━┩
    │ fib_1   │ (update) │ update   │ update   │ restart  │ update   │ update   │
    │ fib_2   │ update   │ (update) │ update   │ restart  │ update   │ update   │
    │ fib_3   │ update   │ update   │ (update) │ restart  │ update   │ update   │
    │ fib_4   │ restart  │ restart  │ restart  │ (update) │ restart  │ restart  │
    │ fib_5   │ update   │ update   │ update   │ restart  │ (update) │ update   │
    │ fib_6   │ update   │ update   │ update   │ restart  │ update   │ (update) │
    └─────────┴──────────┴──────────┴──────────┴──────────┴──────────┴──────────┘
    Summary: For run-measure, 3 of 5 versions (60%) can be live patched with default (extended) comparison and internal relocation resolving!

indicating that dynamic updates to and from `fib_4` library are not possible.

For a more detailed overview on updating strictly subsequent versions, try

    bean-compare -vv -r .

which will show details and color-highlight any incompatibilities (if supported by terminal):

    ├── run-main
    └── run-measure
                                              ./run-main
    ┏━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━┓
    ┃     fib_1     ┃    fib_2     ┃     fib_3     ┃    fib_4     ┃     fib_5     ┃    fib_6     ┃
    ┡━━━━━━━━━━━━━━━╇━━━━━━━━━━━━━━╇━━━━━━━━━━━━━━━╇━━━━━━━━━━━━━━╇━━━━━━━━━━━━━━━╇━━━━━━━━━━━━━━┩
    │ (update)      │ update       │ update        │ restart      │ restart       │ update       │
    │ .build-id     │ .build-id    │ .build-id     │ .build-id    │ .build-id     │ .build-id    │
    │  e6ef57d53a3… │  45faea63d2… │  43de24d8984… │  ad82833e85… │  768909f41ab… │  716921b4cd… │
    │ .init         │ .init        │ .init         │ .init        │ .init         │ .init        │
    │    2: 36B     │    2: 36B    │    2: 36B     │    2: 36B    │    2: 36B     │    2: 36B    │
    │ .fini         │ .fini        │ .fini         │ .fini        │ .fini         │ .fini        │
    │    2: 24B     │    2: 24B    │    2: 24B     │    2: 24B    │    2: 24B     │    2: 24B    │
    │ .text         │ .text        │ .text         │ .text        │ .text         │ .text        │
    │    20: 1546B  │    22: 857B  │    20: 696B   │    22: 1224B │    24: 990B   │    20: 660B  │
    │ .rodata       │ .rodata      │ .rodata       │ .rodata      │ .rodata       │ .rodata      │
    │    9: 433B    │    8: 389B   │    8: 373B    │    10: 593B  │    9: 453B    │    10: 1145B │
    │ .relro        │ .relro       │ .relro        │ .relro       │ .relro        │ .relro       │
    │    4: 616B    │    4: 624B   │    4: 616B    │    4: 616B   │    4: 648B    │    4: 616B   │
    │ .data         │ .data        │ .data         │ .data        │ .data         │ .data        │
    │    3: 16B     │    3: 16B    │    3: 16B     │    4: 64B    │    3: 16B     │    3: 16B    │
    │ .bss          │ .bss         │ .bss          │ .bss         │ .bss          │ .bss         │
    │    3: 16B     │    3: 16B    │    3: 16B     │    3: 16B    │    3: 16B     │    3: 16B    │
    │ .tdata        │ .tdata       │ .tdata        │ .tdata       │ .tdata        │ .tdata       │
    │    0: 0B      │    0: 0B     │    0: 0B      │    0: 0B     │    0: 0B      │    0: 0B     │
    │ .tbss         │ .tbss        │ .tbss         │ .tbss        │ .tbss         │ .tbss        │
    │    0: 0B      │    0: 0B     │    0: 0B      │    0: 0B     │    0: 0B      │    0: 0B     │
    │ .debug        │ .debug       │ .debug        │ .debug       │ .debug        │ .debug       │
    │  RW:03f4c2b4… │  RW:03f4c2b… │  RW:03f4c2b4… │  RW:c9ab727… │  RW:03f4c2b4… │  RW:03f4c2b… │
    │  fn:824782f6… │  fn:824782f… │  fn:824782f6… │  fn:824782f… │  fn:824782f6… │  fn:824782f… │
    └───────────────┴──────────────┴───────────────┴──────────────┴───────────────┴──────────────┘
    Summary: For run-main, 3 of 5 versions (60%) can be live patched with default (extended)
    comparison and internal relocation resolving!

The different size of `.data` and hash in `.debug RW` prevent the update.
Hence, for subsequent updates from version 1 to 6, you would have to restart twice to run *all* library versions.
Otherwise, *Luci* will just skip version 4, directly updating from 3 to 5, as indicated in previous matrix overview.


Notes on RELRO
--------------

The examples using the systems linker employ the [full relocation read-only (RELRO)](https://www.redhat.com/en/blog/hardening-elf-binaries-using-relocation-read-only-relro) for the binaries in the `Makefile`s `LDFLAGS`.
This common mitigation technique places the global offset table (GOT) completely in the non-writable section, requiring to bind all references on load-time (`BIND_NOW`).
Without full RELRO, calling new functions (like the math functions `sqrtl` and `powl` in `fib_5`) would not be possible because it changes the size of the GOT (➔ different writable section makes it incompatible).

In addition, the default compiler flags might include `-fstack-protector`, which will result in calls to `__stack_chk_fail` in some versions, having the same effect on the GOT.
While there is a big chance that this would be always present in bigger binaries, our example binaries are tiny and therefore this GOT entry might be omitted if the code does not contain any functions with arrays on the stack.

In LLVM, a similar case might happen with the recursion in `fib` (in the first version), which will allocate an extra GOT slot.

Without full RELRO (by omitting `BIND_NOW` / no `-z now` linker flag), GCC 11 would only have 27% compatible updates and 40% with clang/LLVM 14 (on Ubuntu Jammy) due to the reasons described above.

Nevertheless, while the described *unintended* incompatibilities are quite likely in very small binaries like the provided examples, we have observed they are rather seldom in real-world binaries — even without hardening techniques like RELRO.
