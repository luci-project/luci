#!/bin/bash

# Switch to example folder
cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null

# Build if required
test -x run-measure || make

# Get all shared object versions
libs=( fib_*/libfib.so )

# Randomize order
libs=( $(shuf -e "${libs[@]}") )

# Ensure clean exit
echo -e "\e[1mLuci Demo updating a shared library (press Ctrl + C to abort)\e[0m"
trap "exit" INT TERM
trap "kill 0" EXIT

# Switch libraries every 10 seconds
for lib in ${libs[@]} ; do
	echo -e "\n   \e[2mSwitching to library ${lib%/*}\e[0m\n"
	ln -sf $lib
	sleep 10s
done &

# Wait one second (so the first library is linked)
sleep 1

# Start measure example with dynamic updates enabled
LD_DYNAMIC_UPDATE=1 ./run-measure 60

# All good
echo -e "\n\e[1mDemo fnished\e[0m"
