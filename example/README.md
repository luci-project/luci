Luci Examples
=============

The subfolders contain small projects that can be used with *Luci*.

  * The [`fibonacci` folder](fibonacci/README.md) contains examples for all scopes of *Luci*:
    Relocatable objects (interactive programming), shared libraries, static libraries, and binaries (without debugging information).
    All versions are based on the same source -- different algorithms for computing the [Fibonacci sequence](https://en.wikipedia.org/wiki/Fibonacci_sequence), which can be switched at runtime to demonstrate the dynamic updates.
  * The [`chess` folder](chess/README.md) contains an extensive example demonstrating the capabilities of *Luci*'s static linker using a chess game:
   In the base version the moves are printed only in algebraic notation, which is then extended by a chess clock, a terminal user interface and finally graphical X output.

Each example contains a `demo.sh` script which runs the demonstration autonomously and is therefore a good starting point.
However, the documentation contains instructions for a manual run, which you can easily adapt to see the capabilities and limitations of *Luci*.
