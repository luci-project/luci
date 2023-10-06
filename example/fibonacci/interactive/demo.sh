#!/bin/bash

# Switch to example folder
cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null

# Build if required
test -e measure.o || make

# Get all shared object versions
objects=( fib_*.o )

# Randomize order
objects=( $(shuf -e "${objects[@]}") )

# Ensure clean exit
echo -e "\e[1mLuci Demo for interactive programming using object file (press Ctrl + C to abort)\e[0m"
trap "exit" INT TERM
trap "kill 0" EXIT

# Switch libraries every 10 seconds
for obj in ${objects[@]} ; do
	echo -e "\n   \e[2mSwitching to object file ${obj%/*}\e[0m\n"
	cp $obj fib.o
	sleep 10s
done &


# Wait one second (so the first library is linked)
sleep 1

# Start measure example with dynamic updates enabled
/opt/luci/ld-luci.so -u -s fib.o measure.o -lc -lm -- 60

# All good
echo -e "\n\e[1mDemo fnished\e[0m"
