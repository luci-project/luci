> I want to get rid of all the diseases plaguing mankind — and replace them with worse ones!

Luci — the linker/loader daemon
===============================

*Luci* is toy linker/loader daemon experiment for academic purposes with hackability (not performance!) in mind -- a sandbox for prototypes!


Implementation
--------------

The standard libraries (neither libc nor libstdc++) are not used, as we need to take care of bootstrapping -- providing several caveats, just think of `errno` in conjunction with thread local storage (TLS)!
A static libc build is not an option either, as it would contain its own TLS setup (which would interfer with our dynamic loading).
Finally, we might include this in a kernel in the future (with has no standard libraries as well).
