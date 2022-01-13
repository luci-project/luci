#!/bin/bash
set -euo pipefail

# Default values
COMPILER="GCC"
LD_NAME="Luci"
LD_PATH_SHORT="/opt/luci/ld-luci.so"
DEBUG_OUTPUT=false
LD_SYSTEM=false
LD_LOGLEVEL=3
LD_DYNAMIC_UPDATE=0
EXEC="run"


# Options
while getopts "c:dhl:r:uR" OPT; do
	case "${OPT}" in
		c)
			COMPILER=${OPTARG}
			;;
		d)
			DEBUG_OUTPUT=true
			;;
		l)
			LD_LOGLEVEL=${OPTARG}
			;;
		r)
			LD_PATH_SHORT=${OPTARG}
			;;
		u)
			LD_DYNAMIC_UPDATE=1
			;;
		R)
			LD_SYSTEM=true
			;;
		*)
			echo "$0 [-c COMPILER] [-l LOGLEVEL] [-u] [TESTS]" >&2
			echo >&2
			echo "Parameters:" >&2
			echo "	-c COMPILER  Use 'GCC' (default) or 'LLVM'" >&2
			echo "	-l LOGLEVEL  Specify log level (default: $LD_LOGLEVEL)" >&2
			echo "	-r PATH      (Short) Path for RTLD (default: $LD_PATH_SHORT)" >&2
			echo "	-d           Debug: Print output" >&2
			echo "	-h           Show this help" >&2
			echo "	-u           Enable dynamic updates (disabled by default)" >&2
			echo "	-R           use default (system) RTLD, incompatible with updates" >&2
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
if $LD_SYSTEM ; then
	LD_NAME="Linux"
	LD_PATH="/lib64/ld-linux-x86-64.so.2"
	if [ $LD_DYNAMIC_UPDATE -ne 0 ] ; then
		echo "System RTLD does not support updates" >&2
		exit 1
	fi
else
	LD_PATH="$(readlink -f "../ld-${LD_NAME,,}-${ID,,}-${VERSION_CODENAME,,}-${PLATFORM,,}.so")"
	if [ ! -x "${LD_PATH}" ] ; then
		echo "Missing RTLD ${LD_PATH} (${RTLD})" >&2
		exit 1
	fi
	if [ -n "${LD_PATH_SHORT}" -a "${LD_PATH}" != "${LD_PATH_SHORT}" ] ; then
		mkdir -p "$(dirname "${LD_PATH_SHORT}")"
		ln -s -f "${LD_PATH}" "${LD_PATH_SHORT}"
		LD_PATH="${LD_PATH_SHORT}"
	fi
fi

# generate config
LD_LIBRARY_CONF=$(readlink -f "libpath.conf")
../gen-libpath.sh /etc/ld.so.conf | grep -v "i386\|i486\|i686\|lib32\|libx32" > "$LD_LIBRARY_CONF"

if $DEBUG_OUTPUT ; then
	make -C ../tools stdlog
fi

echo -e "\n\e[1;4mRunning Tests on ${ID} ${VERSION_CODENAME} (${PLATFORM}) with ${COMPILER}\e[0m"
echo "using ${LD_NAME} RTLD at ${LD_PATH}"
if [ $LD_DYNAMIC_UPDATE -ne 0 ] ; then
	echo "with dynamic updates enabled"
	UPDATEFLAG='update'
else
	UPDATEFLAG='static'
fi

function check() {
	for file in $1{-${ID,,},}{-${VERSION_CODENAME},}{-${PLATFORM},}{-${COMPILER,,},}{-${UPDATEFLAG},} ; do
		if [ -f "$file" ] ; then
			echo "Checking $file" ;
			if [ -x "$file" ] ; then
				"./$file"
			else
				diff -B -w -u "$file" -
			fi
			return
		fi
	done
}

function skip() {
	SKIP=".skip"
	for SKIPTEST in $1/${SKIP}{,-${ID,,}}{,-${VERSION_CODENAME}}{,-${PLATFORM}}{,-${COMPILER,,}}{,-${UPDATEFLAG}}{,-ld_${LD_NAME,,}} ; do
		if [ -f "${SKIPTEST}" ] ; then
			return 0
		fi
	done
	return 1
}


TESTDIR="$(pwd)"
for TEST in ${TESTS} ; do
	if [ -d "${TEST}" ] ; then
		# Check if we should skip depending on variables
		if  [ "$TESTS" = "*" ] && skip "${TEST}" ; then
			echo -e "\n(skipping test ${TEST})" >&2
			continue
		fi

		echo -e "\n\e[1mTest ${TEST}\e[0m"

		# Build
		test -f "${TEST}/Makefile" && make LD_PATH="${LD_PATH}" EXEC="${EXEC}" -B -s -C "${TEST}"
		# Check executable
		if [ ! -x "${TEST}/${EXEC}" ] ; then
			echo "No executable file (${TEST}/${EXEC}) found" >&2
			exit 1
		fi

		# (Re)Set environment variables
		export LC_ALL=C
		export LD_NAME
		export LD_PATH
		export LD_LIBRARY_CONF
		export LD_LOGLEVEL
		export LD_DYNAMIC_UPDATE
		export LD_LIBRARY_PATH=$(readlink -f "${TEST}")


		# Execute and capture stdout + stderr
		STDOUT=$(mktemp)
		STDERR=$(mktemp)

		cd "${TEST}"
		SECONDS=0
		if ${DEBUG_OUTPUT} ; then
			if ../../tools/stdlog -t -e "$STDERR" -e- -o "$STDOUT" -o- "./${EXEC}" ; then
				echo "(finished after ${SECONDS}s)"
			else
				EXITCODE=$?
				echo -e "\e[31mExecution of ${EXEC} (${TEST}) failed with exit code ${EXITCODE} after ${SECONDS}s\e[0m" >&2
				rm "$STDOUT" "$STDERR"
				exit ${EXITCODE}
			fi
		elif "./${EXEC}" 2>"$STDERR" >"$STDOUT" ; then
			echo "(finished after ${SECONDS}s)"
		else
			EXITCODE=$?
			echo -e "\e[4mstdout\e[0m" >&2
			cat "$STDOUT" >&2
			echo -e "\n\e[4mstderr\e[0m" >&2
			cat "$STDERR" >&2
			echo -e "\e[31mExecution of ${EXEC} (${TEST}) failed with exit code ${EXITCODE} after ${SECONDS}s\e[0m" >&2
			rm "$STDOUT" "$STDERR"
			exit ${EXITCODE}
		fi
		cd "${TESTDIR}"

		# Compare stdout + stderr with example
		check "${TEST}/.stdout" < "$STDOUT"
		check "${TEST}/.stderr" < "$STDERR"

		# Remove files
		rm "$STDOUT" "$STDERR"
	elif [ "$TESTS" != "*" ] ; then
		echo "Test '$TEST' does not exist!" >&2
		exit 1
	fi
done
