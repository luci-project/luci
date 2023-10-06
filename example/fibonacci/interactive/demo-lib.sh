#!/bin/bash

# Switch to example folder
cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null

# Build if required
test -e measure.o || make

# Get all shared object versions
libs=( fib_*/libfib.a )

# Randomize order
libs=( $(shuf -e "${libs[@]}") )

# Ensure clean exit
echo -e "\e[1mLuci Demo for interactive programming using static library (press Ctrl + C to abort)\e[0m"
trap "exit" INT TERM
trap "kill 0" EXIT

# Switch libraries every 10 seconds
for lib in ${libs[@]} ; do
	echo -e "\n   \e[2mSwitching to library ${lib%/*}\e[0m\n"
	cp $lib libfib.a
	sleep 10s
done &

# Wait one second (so the first library is linked)
sleep 1

# Start measure example with dynamic updates enabled
/opt/luci/ld-luci.so -v 6 -u -s measure.o -lc -lm -L. -lfib -- 60

# All good
echo -e "\n\e[1mDemo fnished\e[0m"
