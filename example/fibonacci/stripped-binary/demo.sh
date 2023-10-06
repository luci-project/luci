#!/bin/bash

# Switch to example folder
cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null

# Build if required
test -x run-measure || make

# Get all shared object versions
binaries=( fib_*/run-measure )

# Randomize order
binaries=( $(shuf -e "${binaries[@]}") )

# Ensure clean exit
echo -e "\e[1mLuci Demo updating a stripped binary (press Ctrl + C to abort)\e[0m"
trap "exit" INT TERM
trap "kill 0" EXIT

# Switch libraries every 10 seconds
for bin in ${binaries[@]} ; do
	echo -e "\n   \e[2mSwitching to ${bin%/*}\e[0m\n"
	cp -f $bin .
	sleep 10s
done &


# Wait one second (so the first library is linked)
sleep 1

# Start measure example with dynamic updates enabled
LD_UPDATE_MODE=2 LD_DYNAMIC_UPDATE=1 ./run-measure 60

# All good
echo -e "\n\e[1mDemo fnished\e[0m"
