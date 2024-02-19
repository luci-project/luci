#!/usr/bin/env bash
set -euo pipefail

# Script directory
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

# delay before each step in seconds
DEFAULT_DELAY=10

function wait() {
	if [[ $# -eq 1 ]] ; then
		PATCH_DELAY=$1
	else
		PATCH_DELAY=$DEFAULT_DELAY
	fi
	echo -ne "\n\e[2m(waiting ${PATCH_DELAY}s)\e[0m\r"
	sleep ${PATCH_DELAY}
}

## Base directory
BASE_DIR="base"

cd "${SCRIPT_DIR}"
for basefile in ${BASE_DIR}/* ; do
	filename="${basefile#${BASE_DIR}/}"
	if [[ ! -f "$filename" ]] ; then
		echo -e "\e[31mFile $filename missing (copy from $basefile) - abort\e[0m"
		exit 1
	elif ! diff "$filename" "$basefile" >/dev/null ; then
		echo -e "\e[31mFile $filename differs from $basefile - abort\e[0m"
		exit 1
	fi
done
wait


# 1. Code change
echo -e "\e[1mRevision 1\e[0m (code changes)"
patch -p1 < v1.patch
make game.o
wait


# 2. Change of static variable (AI vs AI)
echo -e "\e[1mRevision 2\e[0m (modifing global constant)"
patch -p1 < v2.patch
make game.o
wait


# 3. CHESS_CLOCK: Add new variable, turn off, on again
echo -e "\e[1mRevision 3a\e[0m (introducing new global variable)"
patch -p1 < v3.patch
# define CHESS_CLOCK in source
make CDEFS=CHESS_CLOCK game.o
wait
echo -e "\e[1mRevision 3b\e[0m (version without global variable)"
make -B game.o
wait
echo -e "\e[1mRevision 3c\e[0m (re-introducing global variable)"
make -B CDEFS=CHESS_CLOCK game.o
wait


# 4. Notation to stdout, ASCII Art Board -> new function
echo -e "\e[1mRevision 4\e[0m (ASCII board output)"
patch -p1 < v4.patch
make -B CDEFS=CHESS_CLOCK tui.o
sleep 1
make -B CDEFS=CHESS_CLOCK game.o
wait


# 5. Unicode version -> new functions, wrong order
echo -e "\e[1mRevision 5a\e[0m (Unicode output, but referencing non-existing symbol)"
patch -p1 < v5.patch
make CDEFS=CHESS_CLOCK game.o
wait
echo -e "\e[1mRevision 5b\e[0m (Unicode output, now with present symbol)"
make -B CDEFS=CHESS_CLOCK tui.o game.o
wait


# 6. SDL
echo -e "\e[1mRevision 6a\e[0m (dynamically load SDL shared library)"
patch -p1 < v6-pre.patch
make CDEFS=CHESS_CLOCK tui.o
# We need some time until libraries are loaded
wait 30
echo -e "\e[1mRevision 6b\e[0m (SDL/graphical board)"
patch -R -p1 < v6-pre.patch
patch -p1 < v6.patch
make CDEFS=CHESS_CLOCK tui.o
wait


# Final version
echo -e "\e[1mBuilding standalone binary\e[0m based on last revision"
make CDEFS=CHESS_CLOCK chess
