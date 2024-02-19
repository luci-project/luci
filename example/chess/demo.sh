#!/usr/bin/env bash

# Script directory
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

# Set terminal emulator
if [ -z "$TERMINAL" ] ; then
	TERMINAL=x-terminal-emulator
fi

# Set SDL driver
SDL_VIDEODRIVER=x11
export SDL_VIDEODRIVER

# Copy files from base directory
cd "${SCRIPT_DIR}"
echo "Copying base files and build initial version"
cp base/* .
make -B

# Create pipe for logging and standard error
ERR_PIPE=$(mktemp -u)
if [[ ! -p $ERR_PIPE ]]; then
	echo "Creating standard error pipe at $ERR_PIPE"
	mkfifo $ERR_PIPE
fi

# Opening windows for...
echo "Opening terminals..."
# ... listening on standard error
$TERMINAL -e /bin/bash -c "echo -e \"\e]0;Chess standard error\a\e[7m Luci & chess standard error \e[0m\" ; while true; do sleep 1; cat \"$ERR_PIPE\" ; done" &

# ... Luci application (with auto input)
$TERMINAL -e /bin/bash -c "cd \"$SCRIPT_DIR\" ; echo -e \"\e]0;Chess standard output\a\e[7m Chess standard output \e[0m\" ; ./autoplay.sh | /opt/luci/ld-luci.so -v 3 -s -u ai.o board.o game.o openings.o tui.o -lc $@ 2>\"$ERR_PIPE\" ; sleep 5" &

# run patcher
echo -e "\e]0;Chess interaktive programming\a\e[7m Modification (interactive programming) \e[0m"
./patch.sh
