#!/bin/bash
set -euo pipefail

# Default values
COMPILER="GCC"
LD_NAME="Luci"
LD_PATH_SHORT="/opt/luci/ld-luci.so"
DEBUG_OUTPUT=false
STOP_ON_ERROR=false
LD_SYSTEM=false
LD_LOGLEVEL=3
LD_DYNAMIC_UPDATE=0
LD_RELAX_CHECK=0
LD_PRELOAD=""
LD_DEBUG_HASH=""
EXEC="run"
TIMEOUT=120
TIMEOUT_KILLDELAY=1

# Options
while getopts "c:d:hi:or:st:uv:R" OPT; do
	case "${OPT}" in
		c)
			COMPILER=${OPTARG}
			;;
		o)
			DEBUG_OUTPUT=true
			;;
		r)
			LD_PATH_SHORT=${OPTARG}
			;;
		d)
			LD_DEBUG_HASH=${OPTARG}
			;;
		i)
			LD_RELAX_CHECK=${OPTARG}
			;;
		s)
			STOP_ON_ERROR=true
			;;
		t)
			TIMEOUT=${OPTARG}
			;;
		u)
			LD_DYNAMIC_UPDATE=1
			;;
		v)
			LD_LOGLEVEL=${OPTARG}
			;;
		R)
			LD_SYSTEM=true
			;;
		*)
			echo "$0 [-c COMPILER] [-v LOGLEVEL] [-u] [TESTS]" >&2
			echo >&2
			echo "Parameters:" >&2
			echo "	-c COMPILER  Use 'GCC' (default) or 'LLVM'" >&2
			echo "	-v LOGLEVEL  Specify log level (default: $LD_LOGLEVEL)" >&2
			echo "	-r PATH      (Short) Path for RTLD (default: $LD_PATH_SHORT)" >&2
			echo "	-t SECONDS   set maximum run time per test case (default: $TIMEOUT)" >&2
			echo "	-d SOCKET    Socket for debug hash (elfvarsd)" >&2
			echo "	-i MODE      Relax patchabilitiy check requirements" >&2
			echo "	-s           Stop on (first) Error" >&2
			echo "	-o           Debug: Print output" >&2
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

if [[ $TIMEOUT =~ [^0-9] ]] ; then
   echo "Invalid timeout seconds:'$TIMEOUT'" >&2
   exit 1
fi

TESTS="*"
if [ $# -gt 0 ] ; then
	TESTS=$@
fi

if [ -z ${OS+empty} -o -z ${OSVERSION+empty} ] ; then
	# Determine OS & its version
	if [ ! -f /etc/os-release ] ; then
		echo "Missing /etc/os-release" >&2
		exit 1;
	fi
	source /etc/os-release

	if [ -z ${ID+empty} ] ; then
		if [ -z ${ID_LIKE+empty} ] ; then
			echo "No OS information (in /etc/os-release)" >&2
			exit 1;
		else
			OS=${ID_LIKE}
		fi
	else
		OS=${ID}
	fi
	if [ -z ${VERSION_CODENAME+empty} ] ; then
		if [ -z ${VERSION_ID+empty} ] ; then
			echo "No OS version information" >&2
			exit 1;
		fi
		OSVERSION=${VERSION_ID%%.*}
	else
		OSVERSION=${VERSION_CODENAME}
	fi
fi

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
	LD_PATH="$(readlink -f "../ld-${LD_NAME,,}-${OS,,}-${OSVERSION,,}-${PLATFORM,,}.so")"
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

# Check userfaultfd permission
USERFAULTFD="nouserfaultfd"
LD_DETECT_OUTDATED=disabled
LD_DETECT_OUTDATED_DELAY=1
if [ -f /proc/sys/vm/unprivileged_userfaultfd ] ; then
	if getcap "$(readlink -f "${LD_PATH}")" | grep -i "cap_sys_ptrace=eip" ; then
		LD_DETECT_OUTDATED=userfaultfd
		USERFAULTFD="userfaultfd"
	elif [[ $(cat /proc/sys/vm/unprivileged_userfaultfd) -eq 1 ]] ; then
		LD_DETECT_OUTDATED=userfaultfd
		USERFAULTFD="userfaultfd"
	fi
fi

# generate config
LD_LIBRARY_CONF=$(readlink -f "libpath.conf")
../gen-libpath.sh /etc/ld.so.conf | grep -v "i386\|i486\|i686\|lib32\|libx32" > "$LD_LIBRARY_CONF" || true

if $DEBUG_OUTPUT ; then
	make -C ../tools stdlog
fi

echo -e "\n\e[1;4mRunning Tests on ${OS} ${OSVERSION} (${PLATFORM}) with ${COMPILER}\e[0m"
echo "using ${LD_NAME} RTLD at ${LD_PATH}"
if [ $LD_DYNAMIC_UPDATE -ne 0 ] ; then
	echo "with dynamic updates enabled"
	UPDATEFLAG='update'
else
	UPDATEFLAG='static'
fi

function check() {
	for file in $1{-${OS,,},}{-${OSVERSION,,},}{-${PLATFORM,,},}{-${COMPILER,,},}{-${UPDATEFLAG},}{,-${USERFAULTFD,,}} ; do
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
	for SKIPTEST in $1/${SKIP}{,-${OS,,}}{,-${OSVERSION,,}}{,-${PLATFORM}}{,-${COMPILER,,}}{,-${UPDATEFLAG}}{,-ld_${LD_NAME,,}}{,-${USERFAULTFD,,}} ; do
		if [ -f "${SKIPTEST}" ] ; then
			return 0
		fi
	done
	return 1
}

FAILED=()
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
			if ${STOP_ON_ERROR} ; then
				exit 1
			else
				continue
			fi
		fi

		# Set timeout thread
		if [[ $TIMEOUT -gt 0 ]] ; then
			(
				sleep 1
				if TARGET=$(ps -o pid=,cmd= --ppid $$ 2>/dev/null | sed -ne "s|^[ ]*\([0-9]*\) .*\./${EXEC}\$|\1|p") ; then
					for ((t=1;t<$TIMEOUT;t++)) ; do
						sleep 1
						kill -0 $TARGET 2>/dev/null || exit 0
					done
					echo -e "\e[31mRuntime of $TIMEOUT seconds exceeded -- stopping PID $TARGET (${EXEC}) via SIGTERM...\e[0m" >&2
					if kill -s SIGTERM $TARGET 2>/dev/null ; then
						sleep $TIMEOUT_KILLDELAY
						if kill -s SIGKILL $TARGET 2>/dev/null ; then
							echo -e "\e[31m(stopped via SIGKILL)\e[0m" >&2
						fi
					fi
				fi
			) &
			TIMEOUT_PID=$!
		fi

		# Execute and capture stdout, stderr and status
		STDOUT=$(mktemp)
		STDERR=$(mktemp)
		STATUS=$(mktemp)

		# (Re)Set environment variables
		export LC_ALL=C
		export LD_NAME
		export LD_PATH
		export LD_LIBRARY_CONF
		export LD_LOGLEVEL
		export LD_DYNAMIC_UPDATE
		export LD_DETECT_OUTDATED
		export LD_DEBUG_HASH
		export LD_RELAX_CHECK
		export LD_RELOCATE_OUTDATED=1
		export LD_STATUS_INFO=$STATUS
		export LD_LIBRARY_PATH=$(readlink -f "${TEST}")

		cd "${TEST}"
		EXITCODE=0
		SECONDS=0
		CHECK_OUTPUT=true
		if ${DEBUG_OUTPUT} ; then
			if ../../tools/stdlog -t -e "$STDERR" -e- -o "$STDOUT" -o- "./${EXEC}" ; then
				echo "(finished after ${SECONDS}s)"
			else
				EXITCODE=$?
				echo -e "\e[31mExecution of ${EXEC} (${TEST}) failed with exit code ${EXITCODE} after ${SECONDS}s\e[0m" >&2
				rm "$STDOUT" "$STDERR"
				if ${STOP_ON_ERROR} ; then
					exit ${EXITCODE}
				else
					FAILED+=( "$TEST (exit code ${EXITCODE})" )
				fi
			fi
		elif "./${EXEC}" 2>"$STDERR" >"$STDOUT" ; then
			echo "(finished after ${SECONDS}s)"
		else
			EXITCODE=$?
			echo -e "\e[4mstdout\e[0m" >&2
			cat "$STDOUT" >&2
			echo -e "\n\e[4mstderr\e[0m" >&2
			cat "$STDERR" >&2
			echo -e "\n\e[4mstatus\e[0m" >&2
			cat "$STATUS" >&2
			echo -e "\e[31mExecution of ${EXEC} (${TEST}) failed with exit code ${EXITCODE} after ${SECONDS}s\e[0m" >&2
			rm -f "$STDOUT" "$STDERR" "$STATUS"
			if ${STOP_ON_ERROR} ; then
				exit ${EXITCODE}
			else
				FAILED+=( "$TEST (exit code ${EXITCODE})" )
				CHECK_OUTPUT=false
			fi
		fi
		cd "${TESTDIR}"

		# Stop Timeout process
		if [[ $TIMEOUT -gt 0 ]] ; then
			wait $TIMEOUT_PID 2>/dev/null || true
		fi

		if ${CHECK_OUTPUT} ; then
			# Compare stdout + stderr with example
			if ! ( check "${TEST}/.stdout" < "$STDOUT" && check "${TEST}/.stderr" < "$STDERR" && check "${TEST}/.status" < <(test -f "$STATUS" && sed -e "s/) for .*$/)/" "$STATUS" 2>/dev/null || true)  ) ; then
				echo -e "\e[31mUnexpected output content of ${EXEC} (${TEST}) -- runtime ${SECONDS}s\e[0m" >&2
				if ${STOP_ON_ERROR} ; then
					exit ${EXITCODE}
				else
					FAILED+=( "$TEST (output mismatch)" )
				fi
			fi

			# Remove files
			rm -f "$STDOUT" "$STDERR" "$STATUS"
		fi
	elif [ "$TESTS" != "*" ] ; then
		echo "Test '$TEST' does not exist!" >&2
		if ${STOP_ON_ERROR} ; then
			exit 1
		fi
	fi
done

if [[ ${#FAILED[@]} -eq 0 ]] ; then
	echo -e "\n\e[32mTests finished successfully\e[0m"
	exit 0
else
	echo -e "\n\e[31m${#FAILED[@]} test(s) failed:\e[0m"
	for TEST in "${FAILED[@]}" ; do
		echo -e "\e[31m - $TEST\e[0m"
	done
	exit 1
fi
