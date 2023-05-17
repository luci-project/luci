Luci example
============

This directory contains six versions of a shared library calculating the Fibonacci sequence, all sharing the [same interface (API)](fib.h):

 * [`fib_1`](fib_1.c) uses the well-known recursive algorithm with exponential time complexity O(n²)
 * [`fib_2`](fib_2.c) uses dynamic programming with linear time complexity O(n), which is noticeably faster.
 * [`fib_3`](fib_3.c) employs a more space-efficient iterative algorithm with linear time complexity O(n).
 * [`fib_4`](fib_4.c) is based on a matrix algorithm with logarithmic time complexity O(log(n)) – while there shouldn't be any perceptible difference compared to the iterative approach, this example demonstrates that new helper functions can be introduced as well.
 * [`fib_5`](fib_5.c) implements Binet's formula (using the golden ratio) and therefore has constant time complexity O(1) – since this example requires math functions like square root, the library itself is linked against `libm.so`, therefore demonstrates *Luci*'s ability to handle versions with changed dependencies or its usage of new external functions.
 * [`fib_6`](fib_6.c) contains a lookup table with all relevant Fibonacci sequence numbers, having a constant time complexity O(1) while being slightly faster than the previous version. This version illustrates that changing/introducing (non-writable) data does not pose an issue for *Luci*.

(all versions are based on the examples at [geeksforgeeks.org](https://www.geeksforgeeks.org/program-for-nth-fibonacci-number/))


The [`run-main`](main.c) binary will employ the shared library interface to calculate Fibonacci numbers (endless loop, but only the first 93 values will be valid due to overflows).
To relax the CPU, it will wait a second in between calculating the next number (can be changed/disabled using the `DELAY` macro).
There is a second binary [`run-measure`](measure.c) with the ability to measure the duration of the `fib` library function call.


Preparation
-----------

Build all libraries and binaries with

    make

This will create the two executables (prefixed with `run-`) and a subfolder for each library version, containing the corresponding `libfib.so` file – and a symlink to the first version in the base directory, which is used by the binaries.

You can also change the compiler (e.g., `make CC=clang` for LLVM), adjust the compiler flags (`CFLAGS=...`), or specify a different RTLD (`LD_PATH=`).
Have a look at the [Makefile](Makefile) for more details.


After a standard build, if *Luci* is installed to `/opt/luci/ld-luci.so`, the `main` binary will automatically use it as interpreter.
Hence, you can start the example as you are used to with any other program:

    ./run-main

Otherwise, you have to explicitly specify the *Luci* RTLD:

    ../ld-luci-ubuntu-focal.so run-main

or you could use the `elfo-setinterp` tool to change the default interpreter.


The `main` executable will use `libfib.so`, which is by default a symbolic link to the first version of the library (`fib_1`), hence the output will look similar to

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

When calculating the 50th+ Fibonacci number, this version will take a significant amount of time (several dozens of seconds).


Dynamic Updates
---------------

Now restart the example with dynamic updates enabled:

    LD_DYNAMIC_UPDATE=1 ./run-main

After a certain time in a different terminal window (but same working directory), simulate an *update* of the Fibonacci library by changing the symbolic link:

    ln -sf fib_2/libfib.so

*Luci* will now detect the change, checking if the new version is compatible with the old one, and then load and relink it.
If a call to `fib` is executed during the update, it will finish in library version 1, but subsequent calls will be made in the corresponding function of library version 2 - you should notice a significant performance boost and a different output in the lines starting with square brackets:

    fib(43) = 433494437
    [using Fibonacci library v1: O(2^n))]
    fib(44) = 701408733
    [using Fibonacci library v1: O(2^n))]
    fib(45) = 1134903170
    [using Fibonacci library v2: O(n)]
    fib(46) = 1836311903
    [using Fibonacci library v2: O(n)]

In the same way, you can also change to any other version (e.g., `ln -sf fib_3/libfib.so`).
It is not necessary to sequentially increase the versions; you can directly apply version 6. For example, here an output running `run-measure` with two consecutive updates:

    fib(44) = 701408733 (in 1.574935s)
    [using Fibonacci library v1: O(2^n))]
    fib(45) = 1134903170 (in 2.627559s)
    [using Fibonacci library v1: O(2^n))]
    fib(46) = 1836311903 (in 4.181815s)
    [using Fibonacci library v2: O(n))]
    fib(47) = 2971215073 (in 0.000001s)
    [using Fibonacci library v2: O(n))]
    fib(48) = 4807526976 (in 0.000038s)
    [using Fibonacci library v6: O(1)]
    fib(49) = 7778742049 (in 0.000001s)
    [using Fibonacci library v6: O(1)]

If you want to see further details about the dynamic linking, increase `LD_LOGLEVEL` (e.g., `6` for debugging).


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

The writable data segment would change and updates are no longer possible using *Luci*.

The `bean` utilities might help you to detect and understand certain compatibility issues in libraries:

    bean-update -v -r fib_3/libfib.so fib_4/libfib.so

will output an overview of all changes (together with internal ID used in *Luci*)

    # Changes at update of  (19328 bytes) with  (23552 bytes)
    {ID               ID refs         } [Ref / Rel / Dep] - Address              Size Type     Bind  Flag Name (Section)
    {f80c01b018c54985 0000000000000000} [  0 /   0 /   0] - 0x00000000000002a8     32 unknown  local      (.note.gnu.property)
    {eb918d257d75043c 0000000000000000} [  0 /   0 /   0] - 0x00000000000002c8     36 unknown  local      (.note.gnu.build-id)
    {b667d7892fad4537 0000000000000000} [  0 /   0 /   0] - 0x00000000000002f0     48 unknown  local      (.gnu.hash)
    {66aa258c04d22837 0000000000000000} [  0 /   0 /   0] - 0x0000000000000320    216 unknown  local      (.dynsym)
    {3e7b642e83be65d0 d6000b0b9513d2c5} [  2 /   0 /   1] - 0x0000000000001060     48 function local   X  deregister_tm_clones (.text)
    {dcb5fb7d82b9ed13 b0c9b7ba5d0fe5a6} [  2 /   0 /   1] - 0x0000000000001090     64 function local   X  register_tm_clones (.text)
    {10b55b97dbdc10c6 3761c8c84a1d4901} [  5 /   0 /   1] - 0x00000000000010d0     64 function local   X  __do_global_dtors_aux (.text)
    {b90f21582963e170 d1b9a263b89944c2} [  4 /   0 /   2] - 0x0000000000001120    124 function local   X  multiply.constprop.0 (.text)
    {a469e56a2a99b430 8b9fa10e32dd055d} [  3 /   0 /   1] - 0x00000000000011a0    232 function local   X  power.constprop.0 (.text)
    {4a873eac9ad8703d 1d24abf68e47a7a0} [  4 /   0 /   0] - 0x0000000000001290    232 function global  X  fib (.text)
    {9eb208331f3826b8 48e49edff5938775} [  2 /   0 /   0] - 0x0000000000001380     28 function global  X  print_library_info (.text)
    {f85d9845faad1166 0000000000000000} [  0 /   0 /   1] - 0x0000000000002000     64 unknown  local      (.rodata)
    {7b3d36c07c3db6a5 0000000000000000} [  0 /   0 /   2] - 0x0000000000002040     32 object   local      m (.rodata)
    {4cf90c867d984a54 0000000000000000} [  0 /   0 /   0] - 0x0000000000002060      2 object   global     version (.rodata)
    {9aaba41ffa2da101 0000000000000000} [  0 /   0 /   0] - 0x0000000000002062      2 unknown  local      (.rodata)
    {c92db1cfd2ce47d2 0000000000000000} [  0 /   0 /   0] - 0x0000000000002064     68 none     local      __GNU_EH_FRAME_HDR (.eh_frame_hdr)
    {a5525bfd16cc69f9 0000000000000000} [  0 /   0 /   0] - 0x00000000000020a8    320 unknown  local      (.eh_frame)
    {ac9bb04fc48a1388 0000000000000000} [  0 /   0 /   0] - 0x0000000000003dd0    496 object   local  R   _DYNAMIC (.dynamic)
    {65b3a875a2520cd1 a540e1f9233b8648} [  1 /   1 /   2] - 0x0000000000004000     32 object   local  W   __dso_handle (.data)
    {63ee4d1095c0e94d 0000000000000000} [  0 /   0 /   3] - 0x0000000000004020     32 object   local  W   f.0 (.data)
    {6a00f430077b5d2c 0000000000000000} [  0 /   0 /   3] - 0x0000000000004040      1 object   local  W   completed.0 (.bss)
    {f94b1c0dad11489b 0000000000000000} [  0 /   0 /   0] - 0x0000000000004041      7 unknown  local  W   (.bss)
    
    # Critical sections have changed - not updateable...

If you want to dig deeper, try

    bean-diff -vvv -r -d fib_3/libfib.so fib_4/libfib.so

for a detailed output

    -completed.0 (1 bytes @ 0x4008, .bss [rw-]):
    -          0x4000                          00
    -  3 depending on this
    -  ID: {e029702676d69e52 0000000000000000}
    
    -unnamed 7 bytes @ 0x4009, .bss [rw-]:
    -          0x4000                             00 00 00 00 00 00 00
    -  ID: {e8c1a8a4ddac200f 0000000000000000}
    
    +f.0 (32 bytes @ 0x4020, .data [rw-]):
    +          0x4020  01 00 00 00 00 00 00 00 01 00 00 00 00 00 00 00
    +          0x4030  01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
    +  3 depending on this
    +  ID: {63ee4d1095c0e94d 0000000000000000}
    
    +completed.0 (1 bytes @ 0x4040, .bss [rw-]):
    +          0x4040  00
    +  3 depending on this
    +  ID: {6a00f430077b5d2c 0000000000000000}
    
    +unnamed 7 bytes @ 0x4041, .bss [rw-]:
    +          0x4040     00 00 00 00 00 00 00
    +  ID: {f94b1c0dad11489b 0000000000000000}

Due to introduction of the new variable `f.0`, the layout of the writable section has changed, and certain symbols have now a different page relative address.
(In case the utilities above are not in `PATH` or the assembly output is missing, read Beans build instructions and run `make -C ../bean install -B`!)


**But how does this affect updates?**

The `bean-compare` script offers you a quick overview about the compatibility of all library version:

     bean-compare -m -r .

on default library build outputs

    └── libfib.so
                                     ./libfib.so
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

indicating that dynamic updates to and from `fib_4` library are not possible.

For a more detailed overview on updating strictly subsequent versions, try

    bean-compare -vv -r .

which will show details and color-highlight any incompatibilities (if supported by terminal):

    ┏━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━━┳━━━━━━━━━━━━━━┓
    ┃     fib_1     ┃     fib_2     ┃     fib_3     ┃    fib_4     ┃     fib_5     ┃    fib_6     ┃
    ┡━━━━━━━━━━━━━━━╇━━━━━━━━━━━━━━━╇━━━━━━━━━━━━━━━╇━━━━━━━━━━━━━━╇━━━━━━━━━━━━━━━╇━━━━━━━━━━━━━━┩
    │ (update)      │ update        │ update        │ restart      │ restart       │ update       │
    │ .build-id     │ .build-id     │ .build-id     │ .build-id    │ .build-id     │ .build-id    │
    │  e60353b229a… │  e60353b229a… │  29b174c65a4… │  99fd1ba337… │  0fb3403b2bd… │  9100025103… │
    │ .init         │ .init         │ .init         │ .init        │ .init         │ .init        │
    │    2: 36B     │    2: 36B     │    2: 36B     │    2: 36B    │    2: 36B     │    2: 36B    │
    │ .fini         │ .fini         │ .fini         │ .fini        │ .fini         │ .fini        │
    │    2: 24B     │    2: 24B     │    2: 24B     │    2: 24B    │    2: 24B     │    2: 24B    │
    │ .text         │ .text         │ .text         │ .text        │ .text         │ .text        │
    │    10: 1194B  │    12: 505B   │    10: 344B   │    12: 872B  │    15: 643B   │    10: 308B  │
    │ .rodata       │ .rodata       │ .rodata       │ .rodata      │ .rodata       │ .rodata      │
    │    5: 260B    │    4: 216B    │    4: 200B    │    6: 428B   │    5: 272B    │    6: 980B   │
    │ .relro        │ .relro        │ .relro        │ .relro       │ .relro        │ .relro       │
    │    4: 576B    │    4: 584B    │    4: 576B    │    4: 576B   │    4: 616B    │    4: 576B   │
    │ .data         │ .data         │ .data         │ .data        │ .data         │ .data        │
    │    1: 8B      │    1: 8B      │    1: 8B      │    2: 64B    │    1: 8B      │    1: 8B     │
    │ .bss          │ .bss          │ .bss          │ .bss         │ .bss          │ .bss         │
    │    2: 8B      │    2: 8B      │    2: 8B      │    2: 8B     │    2: 8B      │    2: 8B     │
    │ .tdata        │ .tdata        │ .tdata        │ .tdata       │ .tdata        │ .tdata       │
    │    0: 0B      │    0: 0B      │    0: 0B      │    0: 0B     │    0: 0B      │    0: 0B     │
    │ .tbss         │ .tbss         │ .tbss         │ .tbss        │ .tbss         │ .tbss        │
    │    0: 0B      │    0: 0B      │    0: 0B      │    0: 0B     │    0: 0B      │    0: 0B     │
    │ .debug        │ .debug        │ .debug        │ .debug       │ .debug        │ .debug       │
    │  RW:2248270b… │  RW:2248270b… │  RW:2248270b… │  RW:24ba033… │  RW:2248270b… │  RW:2248270… │
    │  fn:3babe209… │  fn:3babe209… │  fn:3babe209… │  fn:3babe20… │  fn:3babe209… │  fn:3babe20… │
    └───────────────┴───────────────┴───────────────┴──────────────┴───────────────┴──────────────┘
    Summary: For libfib.so, 3 of 5 versions (60%) can be live patched with default (extended)
    comparison and internal relocation resolving!

The different size of `.data` and hash in `.debug RW` prevent the update.
Hence, for subsequent updates from version 1 to 6, you would have to restart twice to run *all* library versions.
Otherwise, *Luci* will just skip version 4, directly updating from 3 to 5, as indicated in previous matrix overview.


### Notes on RELRO

The example employs the [full relocation read-only (RELRO)](https://www.redhat.com/en/blog/hardening-elf-binaries-using-relocation-read-only-relro) for the libraries in the `Makefile`s `LDFLAGS`.
This common mitigation technique places the global offset table (GOT) completely in the non-writable section, requiring to bind all references on load-time (`BIND_NOW`).
Without full RELRO, calling new functions (like the math functions `sqrtl` and `powl` in `fib_5`) would not be possible because it changes the size of the GOT (➔ different writable section makes it incompatible).

In addition, the default compiler flags might include `-fstack-protector`, which will result in calls to `__stack_chk_fail` in some versions, having the same effect on the GOT.
While there is a big chance that this would be always present in bigger libraries, our example libraries are tiny and therefore this GOT entry might be omitted if the code does not contain any functions with arrays on the stack.

In LLVM, a similar case might happen with the recursion in `fib` (in the first version), which will allocate an extra GOT slot.

Without full RELRO (by omitting `BIND_NOW` / no `-z now` linker flag), GCC 11 would only have 27% compatible updates and 40% with clang/LLVM 14 (on Ubuntu Jammy) due to the reasons described above.

Nevertheless, while the described *unintended* incompatibilities are quite likely in very small libraries like the provided examples, we have observed they are rather seldom in real-world libraries — even without hardening techniques like RELRO.
