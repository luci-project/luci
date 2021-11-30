#!/bin/bash
set -euo pipefail

# Default values
COMPILER="GCC"
LD_NAME="Luci"
DEBUG_OUTPUT=false
LD_LOGLEVEL=3
LD_DYNAMIC_UPDATE=0
EXEC="run"


# Options
while getopts "c:dhl:u" OPT; do
	case "${OPT}" in
		s)
			COMPILER=${OPTARG}
			;;
		d)
			DEBUG_OUTPUT=true
			;;
		l)
			LD_LOGLEVEL=${OPTARG}
			;;
		u)
			LD_DYNAMIC_UPDATE=1
			;;
		*)
			echo "$0 [-c COMPILER] [-l LOGLEVEL] [-u] [TESTS]" >&2
			echo >&2
			echo "Parameters:" >&2
			echo "	-c COMPILER  Use 'GCC' (default) or 'LLVM'" >&2
			echo "	-l LOGLEVEL  Specify log level (default: 3)" >&2
			echo "	-d           Debug: Print output" >&2
			echo "	-h           Show this help" >&2
			echo "	-u           Enable dynamic updates (disabled by default)" >&2
			echo >&2
			echo "If no TESTS are specified, alle tests in the directory are checked." >&2
			exit 0
			;;
	esac
done
shift $((OPTIND-1))

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

TESTS="*"
if [ $# -gt 0 ] ; then
	TESTS=$@
fi

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

# Check dynamic linker/loader
LD_PATH="$(readlink -f "../ld-${LD_NAME,,}-${ID,,}-${VERSION_CODENAME,,}-${PLATFORM,,}.so")"
if [ ! -x "${LD_PATH}" ] ; then
	echo "Missing RTLD ${LD_PATH} (${RTLD})" >&2
	exit 1
fi
# generate config
LD_LIBRARY_CONF=$(readlink -f "libpath.conf")
../gen-libpath.sh /etc/ld.so.conf | grep -v "i386\|i486\|i686\|lib32\|libx32" > "$LD_LIBRARY_CONF"


echo -e "\n\e[1;4mRunning Tests on ${ID} ${VERSION_CODENAME} (${PLATFORM}) with ${COMPILER}\e[0m"
echo "using ${LD_PATH}"
if [ $LD_DYNAMIC_UPDATE -ne 0 ] ; then
	echo "with dynamic updates enabled"
fi

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

function skip() {
	SKIP=".skip"
	for SKIPTEST in $1/${SKIP}{,-${ID,,}}{,-${VERSION_CODENAME}}{,-${PLATFORM}}{,-${COMPILER,,}} ; do
		if [ -f "${SKIPTEST}" ] ; then
			return 0
		fi
	done
	return 1
}


for TEST in ${TESTS} ; do
	if [ -d "${TEST}" ] ; then
		# Check if we should skip depending on variables
		if skip "${TEST}" ; then
			echo -e "\n(skipping test ${TEST})" >&2
			continue
		fi

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
		export LD_LOGLEVEL
		export LD_DYNAMIC_UPDATE
		export LD_LIBRARY_PATH=${TEST}


		# Execute and capture stdout + stderr
		STDOUT=$(mktemp)
		STDERR=$(mktemp)
		if ${DEBUG_OUTPUT} ; then
			if ! "${TEST}/${EXEC}" 2> >(tee "$STDERR" >/dev/tty) > >(tee "$STDOUT" >/dev/tty) ; then
				EXITCODE=$?
				echo "Execution of (${TEST}/${EXEC}) failed with exit code ${EXITCODE}" >&2
				rm "$STDOUT" "$STDERR"
				exit ${EXITCODE}
			fi
		elif ! "${TEST}/${EXEC}" 2>"$STDERR" >"$STDOUT" ; then
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
