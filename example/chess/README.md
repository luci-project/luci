Luci Chess Interactive Programming
==================================

This example is based on [chessba.sh](https://github.com/thelazt/chessbash) and [SPiChess](https://gitlab.cs.fau.de/i4/spic/chess).

![Chess board](images/board.png)


Requirements
------------

  * [X Window System](https://en.wikipedia.org/wiki/X_Window_System) and [SDL](https://www.libsdl.org/) libraries
  * Terminal emulator with support for color and Unicode characters - [GNOME Terminal](https://wiki.gnome.org/Apps/Terminal) is recommended


Setup
-----

  * Configure SDL to use the X driver by setting an enivronment variable
    ```
    SDL_VIDEODRIVER=x11
    export SDL_VIDEODRIVER
    ```
  * Create a (named) pipe to display *standard error* in a separate terminal window
    ```
    mkfifo err.pipe
    ```
  * Start new terminal window, change to the example directory, and listen to the pipe
    ```
    cat err.pipe
    ```
  * Copy the base version, compile and start it using *Luci*
    (with parameter `-s` for static linking, `-u` for dynamic updates and, optionally, `-v 3` for increased verbosity)
    ```
    cp base/* .
    make ai.o board.o game.o openings.o tui.o
    /opt/luci/ld-luci.so -v 3 -s -u ai.o board.o game.o openings.o tui.o 2> err.pipe
    ```
    This will start an interactive game, player 2 has to be controlled by the user, using algebraic notation for the move.
    Alternativly, `autoplay.sh` can be used to enter dummy moves:
    ```
    ./autoplay.sh | /opt/luci/ld-luci.so -v 3 -s -u ai.o board.o game.o openings.o tui.o 2> err.pipe
    ```
  * Finally, open a new terminal window and change to the example directory, make the changes (or apply the patches) and compile the changed files:
    ```
    patch -p1 < v1.patch
    make game.o
    ```

➜ to automatically perform these steps, run `demo.sh`.


Run
---

A sequence of potential modifications of the code in the `base` directory is presented below, having each step demonstrating another feature of *Luci*.
For each described change there is also a corresponding patch file.

### Base version

The base version consists of five modules:

  * `board`: playing field and chess of rules
  * `ai`: [Negamax](https://en.wikipedia.org/wiki/Negamax) (compact [Minimax](https://en.wikipedia.org/wiki/Minimax) variant) with [α-β-pruning](https://en.wikipedia.org/wiki/Alpha%E2%80%93beta_pruning) and evaluation functions
  * `openings`: (minimalistic) opening database
  * `tui`: terminal user interface
  * `game`: manages moves

When compiling, it starts a game with the first player controlled by AI and the second by the user.
To control the move, first enter the field with the piece you want to move, then the destination field.
The output consists of a simplified [algebraic notation](https://en.wikipedia.org/wiki/Algebraic_notation_(chess)), with each move in a new line.


### Revision 1: Code change

The output is changed, for example to print the round number and then the move of both players in one line (which makes it more conform to the common algebraic notation used in chess books).
Therefore, the `fputc` call in the function `game_describe` (file `game.c`) is replaced.
For the first player, it will output the round in a new line before printing the actual move, while the move of the second player is prefixed with a spacing symbol:
```
// Show round linewise
if (player < 0)
	fprintf(out, "\n%3d. ", board_moveCounter() / 2 + 1);
else
	fputc('\t', out);
```
After the change, `make game.o` will compile the output (but you can also directly invoke the compiler with `gcc -c game.c`, of course).
*Luci* will detect the change and update the game.

Unless you are using `autoplay.sh`, you will have to finish your move before you will see the effect of the changed code -- since the old code might currently executed and therefore is still on the call stack.


### Revision 2: Change of constant / read-only variable

Now its time to let the AI against itself.
Therefore the second value of the `game_playerStrength` array (again in `game.c`) is changed, for example from `0` (for user) to `4` (which is the recursion level of *negamax* and therefore the AI strength).

Compile, finish your move, and from now on you don't have to move again.
Unless you revert this change.
Or even make this to [PvP](https://en.wikipedia.org/wiki/Player_versus_player) by changing both values to `0`.


### Revision 3: New variable

In this step, the game is extended with a chess clock, measuring the time each player takes for their move.
Therefore, a new variable has to be introduced and the code has to be extended to measure the time.
For the implementation, `gettimeofday` can be used (which also demonstraits that [vDSO](https://en.wikipedia.org/wiki/VDSO) works).

Compile it, and from now on the moves of each user are measured.
In case you use AI for both players at the same level, it is quite possible that the calculations will take a similar amount of time and you don't see a difference when outputing the time at a low resolution.

Feel free to remove the feature (and build) and then reintroduce it -- you will see that removing and the reapplying it will reset the counter.
But for other changes, the counter will continously increase.
This is an intentional behaviour.

For better testing, `v3.patch` has the preprocesor macro `CHESS_CLOCK` to control if the clock should be included or not.


### Revision 4: New function

Now its time to work on a better output.
First choice is a simple ASCII art board, implemented in a new function `tui_draw` (hence located in `tui.c`).
The new function has to be called at each `game_step`, while the algebraic notation should not be printed at standard output -- it is therefore moved to standard error.

First compile `tui.c` then `game.c`.


### Revision 5: Missing symbols

This is kinda similar to the previous one, however an enhanced colored board with [Unicode pieces](https://en.wikipedia.org/wiki/Chess_symbols_in_Unicode) is used.
And for a move helper highlights with possible fields are displayed on the board.
Hence, this requires additional variables and several new functions.

However, this time, `game.c` is compiled first -- which will now reference new functions from `tui.c`, which are not compiled yet.
*Luci* will detect the change in `game.o`, analyse the file, but reject the update due to the missing files.
Therefore, compile now `tui.c` and then recompile `game.c` to let the changes take effect.


### Revision 6a: Dynamic library

During the start, the C standard library (libc) was implicitly loaded (implicit `-lc`).
But now a new library should be loaded dynamically:
`libSDL_image.so` via `dlopen` in an existing function which gets frequently called.

Compile it - and wait a while:
This library has several dependencies, which have dependencies for themself.
Since dynamic updates of shared libraries are enabled, all files are analyzed, and this might take several seconds.


### Revision 6b: Use the new dynamic library

First, remove the code loading the dynamic library -- it is not required anymore, the library is now loaded in the process.
It is now equal to *Luci* started with `-lSDL_image`.
Lets use SDL to draw a graphical chess board, using the images from the corresponding folder.

Compile, and you should see a beautiful chess board.


Limitations
-----------
 * no customizations in `main` possible (they are ignored)
 * adjustments in the game loop will only become active in the next round
 * Visibility: no `static` functions, otherwise the compiler will already perform the linking.
