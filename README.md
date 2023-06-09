> I want to get rid of all the diseases plaguing mankind — and replace them with worse ones!

*Luci* — the linker/loader daemon
===============================

*Luci* is linker/loader daemon experiment for academic purposes with hackability (not performance!) in mind - a platform for prototypes!
Its main purpose is to demonstrate dynamic software updating on off-the-shelf binaries.

Currently, *Luci* is able to automatically update compatible shared libraries during runtime in a process:
If the binary is executed using the *Luci* dynamic linker/loader (for example, as parameter or by modifying the interpreter string in the ELF file) and dynamic updates are enabled (`export LD_DYNAMIC_UPDATE=1`), the shared libraries are monitored on the file system.
On a change (symbolic link or file itself) *Luci* will check if it is able to update the file (for example the writable section must be identical), load and relink it.
If the update is not possible or *Luci* detects the usage of outdated code, it informs the user.

Additional features, like [interactive/live programming](https://en.wikipedia.org/wiki/Interactive_programming) are currently in an early development stage.


Idea & Concept
--------------

Research on [dynamic software updating](https://en.wikipedia.org/wiki/Dynamic_software_updating) began four decades ago and many exciting techniques have been published so far.
However, the vast majority of these approaches require changes to the source code, compiler(-flags), or build system and, therefore, are not widely used.
Hence, *Luci* sets the hurdle of enabling generic userspace live-patching as low as possible and, thus, works purely on binary level — exactly as they are shipped nowadays from distributors.
This has the advantage of being compiler-, build-chain-, and language-agnostic, as long as it generates native binaries in the [Executable and Linkable Format (ELF)](https://en.wikipedia.org/wiki/Executable_and_Linkable_Format).

In addition, *Luci* focuses primarily on bug/security fixes, as these require rapid deployment.
Observations have shown that such fixes usually are limited to certain code changes/additions (e.g., bounds checking) and do not involve structural changes or the introduction of new global variables.
Thanks to advancements in [reproducible builds](https://reproducible-builds.org/) in recent years and common hardening techniques like relocation read-only (RELRO) it is quite likely that compilers generate identical writable sections in such cases — a necessary requirement for updates with *Luci*.

These considerations lead to the implementation of *Luci* as a dynamic linker/loader, with the ability to load new versions of a binary into the process virtual memory and relink it accordingly to the relocation information in the ELF.
The writable data sections of the old version, whose alignment, arrangement, and size must not differ from the new one, are aliased into the memory area of the updated version.
Since multiple versions of a binary can co-exist in the virtual memory, no global or local quiescence is needed for the update.


Build
-----

First make sure that you are using a `x86_64` architecture, a Linux Kernel 4.11 or newer, and a compatible distribution (e.g., Debian and Ubuntu, see below), have common build utilities (`gcc`/`g++`, `make` etc.) installed and check if all submodules are (recursively) checked out.
To do this on recent releases of Debian and Ubuntu LTS, you can run

    apt update && apt install -y build-essential file git gcc g++ libcap2-bin libc++-dev make python3 python3-pyparsing sudo
    git submodule update --init --recursive

> **Please note:** If you encounter any problems, you should consider using a fresh installation in a virtual machine or containerized environment to test this project — your system may have a configuration (e.g., `LD_PRELOAD`) that interferes with *Luci*.

If you just want to use it on your system, we strongly recommend creating the directory `/opt/luci` (and give permission to your user)

    sudo mkdir /opt/luci
    sudo chown -R $(id -u):$(id -g) /opt/luci

A subsequent

    make

will build `ld-luci-${OS_NAME}-${OS_VERSION}` for your system (as long as your distribution and version is supported; see section *Compatibility* below) and create a symlink `/opt/luci/ld-luci.so` to the RTLD.
Furthermore, this will generate a *Luci* configuration file `ld-luci.conf` (with sane default values) and a file `libpath.conf` containing library search paths (based on `/etc/ld.so.conf`).

Use

    make all

to build *Luci* for every supported distribution/version.

For a full-featured experience, please install the tools provided in the [Bean](https://gitlab.cs.fau.de/luci-project/elfo) and [Elfo](https://gitlab.cs.fau.de/luci-project/elfo) submodules.
Make sure you meet the Python3 Packages requirements by using [pip](https://pypi.org/project/pip/), for Debian/Ubuntu with

    apt install -y python3-pip
    pip3 install -r bean/requirements.txt
    make -C bean
    make -C bean/elfo


Usage
-----

**Have a look at the [`example`](example/) folder and run [`demo.sh`](example/demo.sh) for a quick demonstration!**

If you want to run a custom binary `~/a.out` (with parameter `foo`) with *Luci*, you can either start the *Luci* dynamic linker/loader with the binary as argument

    /opt/luci/ld-luci.so ~/a.out -- foo

or change its interpreter to the `ld-luci.so` path, during compilation with `-Wl,--dynamic-linker=/opt/luci/ld-luci.so` or with the `elfo-setinterp` tool

    ./bean/elfo/elfo-setinterp ~/a.out /opt/luci/ld-luci.so

> **Please note:** For this tool the new path must not be longer than the previous one - since the default is usually `/lib64/ld-linux-x86-64.so.2` (27 characters), the default path to the *ld-luci.so* symlink with only 20 characters will fit.
> Otherwise, you can try [PatchELF](https://github.com/NixOS/patchelf), however, it might reorder the sections, which could lead to issues.

After you have modified the interpreter, executing

    ~/a.out foo

will run it with *Luci*.
You are able to control the behavior not only by the configuration file but also using environment variables.
To list the available settings, run

    /opt/luci/ld-luci.so -h


### Debug Symbols

While debug symbols are not required for the update, they can contribute to the compatibility detection.
In case you have binaries with external debug symbols (either local or using [debuginfod](https://sourceware.org/elfutils/Debuginfod.html) to retrieve them), the service `bean-elfvarsd` can process them for *Luci* and respond with the resulting hash using a (tcp/unix) socket.

    bean/tools/elfvarsd.sh -c '.test-cache' 0.0.0.0:9001 /path/to/debug-dir

    export LD_DEBUG_HASH=tcp:127.0.0.1:9001


> **Please note:** Appendix E of the [DWARF4 Standard](https://dwarfstd.org/doc/DWARF4.pdf) defines compression and duplicate elimination mechanism.
> These techniques, e.g. provided by [dwz](https://sourceware.org/dwz/) and used to reduce the size of debug packages in *Ubuntu Jammy* and *Debian Bullseye*, are not supported yet by the tools.


Test cases
----------

*Luci* has several test cases to automatically test glibc compatibility and update functionality.

To test `ld_luci.so` with dynamic updates enabled, simply run

    ./test/run.sh -u

which will build, run and check the [default test cases](test/default) with your systems C/C++ compiler.
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
    ./tools/docker.sh almalinux:9 ./test/run.sh -u "2-.*"

If you want to test *Luci*'s dynamic update capabilities with different languages like Ada, Fortran, Go, Pascal, and Rust, make sure to have the correspondent compilers installed.
On Debian/Ubuntu, install them using

    apt install fpc gnat gfortran golang gccgo rustc

and run the [lang test cases](test/lang) with

    ./test/run.sh -g lang -u

> **Please note:** Some test cases are allowed (or even expected) to fail — they contain a `.mayfail` file in their folder, preventing a fatal exit of the test suite.
> For example, programs written in Go are not supposed to load shared libraries in Go (not related to the RTLD) since this would cause the runtime to be loaded twice. Depending on the Go version and the outcome of some racy code, test case `1-go` might work or might fail.

On each push to the *Luci* main repository, the [project's GitLab CI](https://gitlab.cs.fau.de/luci-project/luci/-/pipelines/) will run all default test cases on every supported distribution (using GCC and LLVM), plus the language test cases on all supported Debian and Ubuntu versions.
For this reason, we provide [Docker images](https://gitlab.cs.fau.de/luci-project/docker) on [dockerhub](https://hub.docker.com/r/inf4/luci/tags) on which the required tools are already installed.
You can use it for testing instead (and save time and bandwidth on subsequent runs):

    ./tools/docker-inf4.sh almalinux 9 ./test/run.sh -u "2-.*"


Compatibility
-------------

*Luci* aims to be compatible to the [GNU C Library (glibc)](https://www.gnu.org/software/libc/) in major desktop/server Linux distributions, so it is able to handle its binaries.

In those distributions, glibc provides the system's default dynamic linker/loader (aka *run-time link-editor* (RTLD) aka `ld.so`/`ld-linux.so`).
Because of this origin, RTLD and the actual libc shared library (`libc6.so` etc.) are closely intertwined - sharing data structures and sometimes even inlining functions of each other.
Furthermore, distributions not only tend to use different configurations but often backport several patches/features from newer versions.

Hence, *Luci* must be tailored to each distribution and version, providing the required data structures and intercepting certain inlined functions from the libc (mainly methods used to dynamically load objects, e.g., `__libc_dlopen_mode`).

Since the two main debianoid distributions (Debian and Ubuntu) have an excellent archive of previously released packages (and are a great target for backtesting), these are well-tested in *Luci*.
However, *Luci* supports additional distributions as well - with the ability to at least successfully run the [default test suite](test/default).

 * [Debian](https://www.debian.org/)
    * 9 / Stretch (with glibc 2.24) - released June 2017, end of life (LTS) since June 2022
    * 10 / Buster (with glibc 2.31) - released July 2019
    * 11 / Bullseye (with glibc 2.31) - released August 2021
    * 12 / Bookworm (with glibc 2.36) - not released yet (summer 2023?)
 * [Ubuntu](https://ubuntu.com/) LTS
    * 20.04 / Focal Fossa (with glibc 2.31) - released April 2020
    * 22.04 / Jammy Jellyfish (with glibc 2.35) - released April 2022
 * [AlmaLinux](https://almalinux.org/), [Oracle Linux](https://www.oracle.com/linux/) & [Red Hat Enterprise Linux](https://www.redhat.com/de/technologies/linux-platforms/enterprise-linux)
    * Version 9 (with glibc 2.28) - released May / June 2022
 * [Fedora](https://fedoraproject.org/)
    * Version 36 (with glibc 2.35)
    * Version 37 (with glibc 2.36)
 * [openSUSE Leap](https://get.opensuse.org/leap)
    * Version 15 (with glibc 2.31)

At the moment, *Luci* is limited to the `x86_64` architecture.

> **Please note:** Only a subset of glibc functionality is supported.

To be able to detect access to outdated code, Linux Kernel 4.11 or newer is required (for certain `userfaultfd` features).
In addition, [userfaultfd](https://docs.kernel.org/admin-guide/mm/userfaultfd.html) must be enabled for *Luci*.
To do so, either assigning the *Luci* binary the required capability by executing

    sudo setcap cap_sys_ptrace=eip $(readlink -f /opt/luci/ld-luci.so)

or globally enable unprivileged usage:

    echo 1 | sudo tee /proc/sys/vm/unprivileged_userfaultfd


Dependencies
------------

 * [Dirty Little Helper](https://gitlab.cs.fau.de/luci-project/dlh) provides the required standard library functions as well as the data structures (tree/hash set/map) for creating static freestanding applications (without glibc).
   * The standard libraries (neither libc nor libstdc++) are not used, as we need to take care of bootstrapping -- providing several caveats; just think of `errno` in conjunction with thread local storage (TLS)!
     A static libc build is not an option either, as it would contain its own TLS setup (which would interfere with our dynamic loading).
   * For Linux, all required system call wrapper are embedded in this library.
 * [Elfo](https://gitlab.cs.fau.de/luci-project/elfo) is a lightweight parser for the Executable and Linking Format, supporting common GNU/Linux extensions
 * [Bean](https://gitlab.cs.fau.de/luci-project/elfo), the binary explorer/analyzer, is employed to compare binary files (shared libraries) and detect changes to decide if an update is safely possible.
   * Bean itself uses the [Capstone Engine](http://www.capstone-engine.org/)


Author & License
----------------

The *Luci* project is being developed by [Bernhard Heinloth](https://sys.cs.fau.de/person/heinloth) of the [Department of Computer Science 4](https://sys.cs.fau.de/) at [Friedrich-Alexander-Universität Erlangen-Nürnberg](https://www.fau.eu/) and is available under the [GNU Affero General Public License, Version 3 (AGPL v3)](LICENSE.md).


Related Work
------------

### Patching shared libraries

 - [libpulp](https://github.com/SUSE/libpulp), a framework for userspace live-patching, requires the compiler to be compiled with patchable function entries and needs libpulp.so preloaded. Depends on ptrace
 - [libcare](https://github.com/cloudlinux/libcare) is employed during build time, analyzes the assembly & adds new sections (like kpatch)
