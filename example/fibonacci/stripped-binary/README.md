Example updating a stripped binary
==================================

Demonstration of *Luci* handling changes in stripped binaries.
A detailed description of the function versions for calculating Fibonacci sequence can be found in the [parent directory](../README.md)
The [`run-main`](main.c) binary calculates Fibonacci numbers with symbol table and debug information removed, the second binary [`run-measure`](measure.c) is stripped in the same way but has the ability to measure the duration of each `fib` function call.


Quick Start
-----------

Run

    ./demo.sh

to build the examples and start the `run-measure` binary, automatically replacing the binary (-> dynamically updating it) every 10 seconds with another version in random order.
The script performs the tasks described below.


Preparation
-----------

Build binaries for each versions with

    make

This will create two executables (prefixed with `run-`) in an subfolder for each version, having all symbols stripped from the binaries.
In addition, it creates a symlink to the binaries of the first version in the base directory.

You can also change the compiler (e.g., `make CC=clang` for LLVM), adjust the compiler flags (`CFLAGS=...`), or specify a different RTLD (`LD_PATH=`).
Have a look at the [Makefile](Makefile) for more details.


After a standard build, if *Luci* is installed to `/opt/luci/ld-luci.so`, the binaries will automatically use it as interpreter.
Hence, you can start the example as you are used to with any other program:

    ./run-main

Otherwise, you have to explicitly specify the *Luci* RTLD:

    ../ld-luci-ubuntu-focal.so run-main

or you could use the `elfo-setinterp` tool to change the default interpreter.


The `run-main` executable is by default a copy of the first version (`fib_1`), hence the output will look similar to

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

Now restart the example with dynamic updates enabled:

    LD_UPDATE_MODE=2 LD_DYNAMIC_UPDATE=1 ./run-main

The value of `LD_UPDATE_MODE` controls the extend of changes:

  * a value of `0` will only update the values in the global offset table (GOT) – but since most compilers will not create entries for the local (fibonacci calculation) functions, this mode will not have any effect in this example.
  * a value of `1` will also reconstruct and update relocations in the code (where possible). This is sufficient for this example, however it is possible to construct certain types of endless loops which would not be possible to updated with this technique alone.
  * a value of `2` will additionally place special instructions to redirect the code where the previous mode cannot be applied successfully.

After a certain time in a different terminal window (but same working directory), simulate an *update* of the Fibonacci binary by changing the binary:

    cp -f fib_2/run-main .

*Luci* will now detect the change, checking if the new version is compatible with the old one, and then load and relink it.
If a call to `fib` is executed during the update, it will finish in function version 1, but subsequent calls will be made in the corresponding function of version 2 - you should notice a significant performance boost and a different output in the lines starting with square brackets:

    fib(43) = 433494437
    [using Fibonacci v1: O(2^n))]
    fib(44) = 701408733
    [using Fibonacci v1: O(2^n))]
    fib(45) = 1134903170
    [using Fibonacci v2: O(n)]
    fib(46) = 1836311903
    [using Fibonacci v2: O(n)]

In the same way, you can also change to any other version (e.g., `ln -sf fib_3/run-main`).
It is not necessary to sequentially increase the versions; you can directly apply version 6.
For example, here an output running `run-measure` with two consecutive updates:

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

If you want to see further details about the dynamic linking, increase `LD_LOGLEVEL` (e.g., `6` for debugging).


Going further
-------------

Feel free to modify the functions and test the capabilities of *Luci*.
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

The writable data segment would change and updates are no longer possible using *Luci*.

The `bean` utilities might help you to detect and understand certain compatibility issues in the binaries:

    bean-update -v -r fib_3/run-main fib_4/run-main

will output an overview of all changes (together with internal ID used in *Luci*)

    # Changes at update of fib_3/run-main (23064 bytes) with fib_4/run-main (27280 bytes)
    {ID               ID refs         } [Ref / Rel / Dep] - Address              Size Type     Bind  Flag Name (Section)
    {e7220011b67a239f 0000000000000000} [  0 /   0 /   0] - 0x0000000000000360     36 unknown  local      (.note.gnu.build-id)
    {705dc35c1ddfe001 0000000000000000} [  0 /   0 /   0] - 0x00000000000003d0    288 unknown  local      (.dynsym)
    {a69598acff390db8 0000000000000000} [  0 /   0 /   0] - 0x0000000000000618    216 unknown  local      (.rela.dyn)
    {1b006775bb8aab8f 170f76b358095a94} [  9 /   0 /   0] - 0x00000000000010e0    186 function global  X  main (.text)
    {cd8b788b02cbc8d3 98a258ee9e6c34d4} [  5 /   0 /   1] - 0x0000000000001240     64 function local   X  __do_global_dtors_aux (.text)
    {bc9f7c897eeb0c7b d1b9a263b89944c2} [  4 /   0 /   2] - 0x0000000000001290    124 function local   X  multiply.constprop.0 (.text)
    {910fe8de5dfd944f ee9f057af33d2dd5} [  3 /   0 /   1] - 0x0000000000001310    232 function local   X  power.constprop.0 (.text)
    {483b5dee36bb6642 a8bb4553ba6abcc3} [  4 /   0 /   0] - 0x0000000000001400    232 function global  X  fib (.text)
    {b09c6c5361cb1c64 3b8216fabe10b794} [  2 /   0 /   0] - 0x00000000000014f0     28 function global  X  print_library_info (.text)
    {6459095ad5d7bbed 0000000000000000} [  0 /   0 /   2] - 0x0000000000002004     92 unknown  local      (.rodata)
    {7b3d36c07c3db6a5 0000000000000000} [  0 /   0 /   2] - 0x0000000000002060     32 object   local      m (.rodata)
    {4cf90c867d984a54 0000000000000000} [  0 /   0 /   0] - 0x0000000000002080      2 object   global     version (.rodata)
    {9aaba41ffa2da101 0000000000000000} [  0 /   0 /   0] - 0x0000000000002082      2 unknown  local      (.rodata)
    {59aac399d7b9be30 0000000000000000} [  0 /   0 /   0] - 0x0000000000002084     84 none     local      __GNU_EH_FRAME_HDR (.eh_frame_hdr)
    {db053b16f96c2b03 0000000000000000} [  0 /   0 /   0] - 0x00000000000020d8    400 unknown  local      (.eh_frame)
    {2794a4d04279b9b5 0000000000000000} [  0 /   0 /   0] - 0x0000000000003da8    496 object   local  R   _DYNAMIC (.dynamic)
    {40ec56b75ecf92dd 383f81ca3fe5ee30} [  1 /   1 /   0] - 0x0000000000004008     24 object   global W   __dso_handle (.data)
    {63ee4d1095c0e94d 0000000000000000} [  0 /   0 /   3] - 0x0000000000004020     32 object   local  W   f.0 (.data)
    {bb836acb4dcf3782 0000000000000000} [  0 /   0 /   0] - 0x0000000000004040      8 object   global W   stdout (.bss)
    {77c21b3a0d07366d 0000000000000000} [  0 /   0 /   1] - 0x0000000000004048      1 object   local  W   completed.0 (.bss)
    {293d9acc4aa1d5af 0000000000000000} [  0 /   0 /   0] - 0x0000000000004049      7 unknown  local  W   (.bss)

    # Critical sections have changed - not updateable...

If you want to dig deeper, try

    bean-diff -vvv -r -d fib_3/run-main fib_4/run-main

for a detailed output

    -stdout (8 bytes @ 0x4010, global, .bss [rw-]):
    -          0x4010  00 00 00 00 00 00 00 00
    -  ID: {17d6937297c998d0 0000000000000000}

    -completed.0 (1 bytes @ 0x4018, .bss [rw-]):
    -          0x4010                          00
    -  1 depending on this
    -     0x1240 <__do_global_dtors_aux/.text> {cd8b788b02cbc8d3 2938e11c0d56e8ce}
    -  ID: {a19b22b78ff4e733 0000000000000000}

    -unnamed 7 bytes @ 0x4019 [T], .bss [rw-]:
    -          0x4010                             00 00 00 00 00 00 00
    -  ID: {94bbe846070d14fc 0000000000000000}

    +f.0 (32 bytes @ 0x4020, .data [rw-]):
    +          0x4020  01 00 00 00 00 00 00 00 01 00 00 00 00 00 00 00
    +          0x4030  01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
    +  3 depending on this
    +     0x1290 <multiply.constprop.0/.text> {bc9f7c897eeb0c7b d1b9a263b89944c2}
    +     0x1310 <power.constprop.0/.text> {910fe8de5dfd944f ee9f057af33d2dd5}
    +     0x1400 <fib/.text> {483b5dee36bb6642 a8bb4553ba6abcc3}
    +  ID: {63ee4d1095c0e94d 0000000000000000}

    +stdout (8 bytes @ 0x4040, global, .bss [rw-]):
    +          0x4040  00 00 00 00 00 00 00 00
    +  ID: {bb836acb4dcf3782 0000000000000000}

    +completed.0 (1 bytes @ 0x4048, .bss [rw-]):
    +          0x4040                          00
    +  1 depending on this
    +     0x1240 <__do_global_dtors_aux/.text> {cd8b788b02cbc8d3 98a258ee9e6c34d4}
    +  ID: {77c21b3a0d07366d 0000000000000000}

    +unnamed 7 bytes @ 0x4049 [T], .bss [rw-]:
    +          0x4040                             00 00 00 00 00 00 00
    +  ID: {293d9acc4aa1d5af 0000000000000000}


Due to introduction of the new variable `f.0` in `.data`, the layout of the writable section has changed, and certain symbols have now a different page relative address.
(In case the utilities above are not in `PATH` or the assembly output is missing, read Beans build instructions and run `make -C ../bean install -B`!)


**But how does this affect updates?**

The `bean-compare` script offers you a quick overview about the compatibility of all binary version:

     bean-compare -m -r .

on default binary build outputs

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

indicating that dynamic updates to and from `fib_4` version are not possible.

For a more detailed overview on updating strictly subsequent versions, try

    bean-compare -vv -r .

which will show details and color-highlight any incompatibilities (if supported by terminal):

                                              ./run-main
    ┏━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━┓
    ┃     fib_1     ┃    fib_2     ┃     fib_3     ┃    fib_4     ┃     fib_5     ┃    fib_6     ┃
    ┡━━━━━━━━━━━━━━━╇━━━━━━━━━━━━━━╇━━━━━━━━━━━━━━━╇━━━━━━━━━━━━━━╇━━━━━━━━━━━━━━━╇━━━━━━━━━━━━━━┩
    │ (update)      │ update       │ update        │ restart      │ restart       │ update       │
    │ .build-id     │ .build-id    │ .build-id     │ .build-id    │ .build-id     │ .build-id    │
    │  9030e62ed11… │  0bb152cbda… │  83c4e020cfd… │  3c641d553b… │  cce5367f8e0… │  5249ebe633… │
    │ .init         │ .init        │ .init         │ .init        │ .init         │ .init        │
    │    2: 36B     │    2: 36B    │    2: 36B     │    2: 36B    │    2: 36B     │    2: 36B    │
    │ .fini         │ .fini        │ .fini         │ .fini        │ .fini         │ .fini        │
    │    2: 24B     │    2: 24B    │    2: 24B     │    2: 24B    │    2: 24B     │    2: 24B    │
    │ .text         │ .text        │ .text         │ .text        │ .text         │ .text        │
    │    19: 1564B  │    21: 876B  │    19: 716B   │    21: 1260B │    23: 1012B  │    19: 684B  │
    │ .rodata       │ .rodata      │ .rodata       │ .rodata      │ .rodata       │ .rodata      │
    │    3: 405B    │    3: 349B   │    3: 333B    │    3: 565B   │    3: 409B    │    3: 1117B  │
    │ .relro        │ .relro       │ .relro        │ .relro       │ .relro        │ .relro       │
    │    4: 632B    │    4: 640B   │    4: 632B    │    4: 632B   │    4: 664B    │    4: 632B   │
    │ .data         │ .data        │ .data         │ .data        │ .data         │ .data        │
    │ .bss          │ .bss         │ .bss          │ .bss         │ .bss          │ .bss         │
    │    2: 16B     │    2: 16B    │    2: 16B     │    2: 64B    │    2: 16B     │    2: 16B    │
    │    2: 16B     │    2: 16B    │    2: 16B     │    2: 16B    │    2: 16B     │    2: 16B    │
    │ .tdata        │ .tdata       │ .tdata        │ .tdata       │ .tdata        │ .tdata       │
    │    0: 0B      │    0: 0B     │    0: 0B      │    0: 0B     │    0: 0B      │    0: 0B     │
    │ .tbss         │ .tbss        │ .tbss         │ .tbss        │ .tbss         │ .tbss        │
    │    0: 0B      │    0: 0B     │    0: 0B      │    0: 0B     │    0: 0B      │    0: 0B     │
    │ .debug *      │ .debug *     │ .debug *      │ .debug *     │ .debug *      │ .debug *     │
    │  RW:fed4a683… │  RW:fed4a68… │  RW:fed4a683… │  RW:a952aed… │  RW:fed4a683… │  RW:fed4a68… │
    └───────────────┴──────────────┴───────────────┴──────────────┴───────────────┴──────────────┘
    Summary: For run-main, 3 of 5 versions (60%) can be live patched with default (extended)
    comparison and internal relocation resolving!

The different size of `.data` and hash in `.debug **RW**` prevent the update.
Hence, for subsequent updates from version 1 to 6, you would have to restart twice to run *all* versions.
Otherwise, *Luci* will just skip version 4, directly updating from 3 to 5, as indicated in previous matrix overview.


### Notes on RELRO

The example employs the [full relocation read-only (RELRO)](https://www.redhat.com/en/blog/hardening-elf-binaries-using-relocation-read-only-relro) for the binaries in the `Makefile`s `LDFLAGS`.
This common mitigation technique places the global offset table (GOT) completely in the non-writable section, requiring to bind all references on load-time (`BIND_NOW`).
Without full RELRO, calling new functions (like the math functions `sqrtl` and `powl` in `fib_5`) would not be possible because it changes the size of the GOT (➔ different writable section makes it incompatible).

In addition, the default compiler flags might include `-fstack-protector`, which will result in calls to `__stack_chk_fail` in some versions, having the same effect on the GOT.
While there is a big chance that this would be always present in bigger binaries, our example binaries are tiny and therefore this GOT entry might be omitted if the code does not contain any functions with arrays on the stack.

In LLVM, a similar case might happen with the recursion in `fib` (in the first version), which will allocate an extra GOT slot.

Without full RELRO (by omitting `BIND_NOW` / no `-z now` linker flag), GCC 11 would only have 27% compatible updates and 40% with clang/LLVM 14 (on Ubuntu Jammy) due to the reasons described above.

Nevertheless, while the described *unintended* incompatibilities are quite likely in very small binaries like the provided examples, we have observed they are rather seldom in real-world binaries — even without hardening techniques like RELRO.
