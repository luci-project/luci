#!/bin/bash
set -euo pipefail

if [ $# -ge 1 ] ; then
	COMPILER=$1
else
	COMPILER="GCC"
fi
case $COMPILER in
	GCC)
		export CC=gcc
		export CXX=g++
		;;
	LLVM)
		export CC=clang
		export CXX=clang++
		;;
	*)
		echo "Unsupported compiler ${COMPILER}" >&2
		exit 1
		;;
esac

source /etc/os-release

cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null

case $(uname -m) in
	x86_64)
		PLATFORM="x64"
		;;
	*)
		echo "Unsupported platform" >&2
		exit 1
		;;
esac

LD_NAME="Luci"
# Check dynamic linker/loader
LD_PATH="$(readlink -f "../ld-${LD_NAME,,}-${ID,,}-${VERSION_CODENAME,,}-${PLATFORM,,}.so")"
if [ ! -x "${LD_PATH}" ] ; then
	echo "Missing RTLD ${LD_PATH} (${RTLD})" >&2
	exit 1
fi
# generate config
LD_LIBRARY_CONF=$(readlink -f "libpath.conf")
../gen-libpath.sh /etc/ld.so.conf | grep -v "i386\|i486\|i686\|lib32\|libx32" > "$LD_LIBRARY_CONF"


echo -e "\e[1;4mRunning Tests on ${ID} ${VERSION_CODENAME} (${PLATFORM}) with ${COMPILER}\e[0m"
echo "using ${LD_PATH}"

function check() {
	if [ -f "$1" ] ; then
		echo "Checking $1" ;
		if [ -x "$1" ] ; then
			"./$1"
		else
			diff -B -w -u "$1" -
		fi
	fi
}

EXEC="run"
for TEST in * ; do
	if [ -d "${TEST}" ] ; then
		echo -e "\n\e[1mTest ${TEST}\e[0m"
		# Build
		test -f "${TEST}/Makefile" &&  make LD_PATH="${LD_PATH}" EXEC="${EXEC}" -B -s -C "${TEST}"
		# Check executable
		if [ ! -x "${TEST}/${EXEC}" ] ; then
			echo "No executable file (${TEST}/${EXEC}) found" >&2
			exit 1
		fi

		# (Re)Set environment variables
		export LD_NAME
		export LD_PATH
		export LD_LIBRARY_CONF
		export LD_LOGLEVEL=6
		export LD_LIBRARY_PATH=${TEST}


		# Execute and capture stdout + stderr
		STDOUT=$(mktemp)
		STDERR=$(mktemp)
		if ! "${TEST}/${EXEC}" >"$STDOUT" 2>"$STDERR" ; then
			EXITCODE=$?
			echo "Execution of (${TEST}/${EXEC}) failed with exit code ${EXITCODE}" >&2
			echo -e "\e[4mstdout\e[0m" >&2
			cat "$STDOUT" >&2
			echo -e "\n\e[4mstderr\e[0m" >&2
			cat "$STDERR" >&2
			rm "$STDOUT" "$STDERR"
			exit ${EXITCODE}
		fi

		# Compare stdout + stderr with example
		check "${TEST}/.stdout" < "$STDOUT"
		check "${TEST}/.stderr" < "$STDERR"

		# Remove files
		rm "$STDOUT" "$STDERR"
	fi
done
