> I want to get rid of all the diseases plaguing mankind — and replace them with worse ones!

Luci — the linker/loader daemon
===============================

*Luci* is toy linker/loader daemon experiment for academic purposes with hackability (not performance!) in mind -- a sandbox for prototypes!


Implementation
--------------

The standard libraries (neither libc nor libstdc++) are not used, as we need to take care of bootstrapping -- providing several caveats, just think of `errno` in conjunction with thread local storage (TLS)!
A static libc build is not an option either, as it would contain its own TLS setup (which would interfer with our dynamic loading).
Finally, we might include this in a kernel in the future (with has no standard libraries as well).

For Linux, all required system call wrapper are embedded (copied from musl libc and glibc).


Author & License
----------------

The *Luci*-project is being developed by [Bernhard Heinloth](https://sys.cs.fau.de/person/heinloth) of the [Department of Computer Science 4](https://sys.cs.fau.de/) at [Friedrich-Alexander-Universität Erlangen-Nürnberg](https://www.fau.eu/) and is available under the [GNU Affero General Public License, Version 3 (AGPL v3)](LICENSE.md).


Related Work
------------

 - [libpulp](https://github.com/SUSE/libpulp), a framework for userspace live patching, requires compiler to be compiled with patchable function entries and needs libpulp.so preloaded. Depends on ptrace
 - [libcare](https://github.com/cloudlinux/libcare) is employed during build time, analyzes the assembly & adds new sections (like kpatch)
