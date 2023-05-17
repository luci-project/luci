#!/bin/bash
set -euo pipefail

# Default values
COMPILER="GCC"
GROUP="default"
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
FORCE=false

# Options
while getopts "c:d:fg:hi:or:st:uv:R" OPT; do
	case "${OPT}" in
		c)
			COMPILER=${OPTARG}
			;;
		g)
			GROUP=${OPTARG}
			;;
		f)
			FORCE=true
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
			echo "$0 [-c COMPILER] [-v LOGLEVEL] [-u] [-g GROUP] [TESTS]" >&2
			echo >&2
			echo "Parameters:" >&2
			echo "	-c COMPILER  Use 'GCC' (default) or 'LLVM'" >&2
			echo "	-g GROUP     Group directory containing tests (default: $GROUP)" >&2
			echo "	-v LOGLEVEL  Specify log level (default: $LD_LOGLEVEL)" >&2
			echo "	-r PATH      (Short) Path for RTLD (default: $LD_PATH_SHORT)" >&2
			echo "	-t SECONDS   set maximum run time per test case (default: $TIMEOUT)" >&2
			echo "	-d SOCKET    Socket for debug hash (elfvarsd)" >&2
			echo "	-i MODE      Relax patchabilitiy check requirements" >&2
			echo "	-f           Force tests (do not skip)" >&2
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
   echo "Invalid timeout seconds: '$TIMEOUT'" >&2
   exit 1
fi

cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null
BASEDIR="$(pwd)"

if [ ! -d "${BASEDIR}/${GROUP}" ] ; then
	echo "Invalid group directory: '${GROUP}'" "${BASEDIR}/${GROUP}" >&2
   exit 1
fi

if [ $# -gt 0 ] ; then
	REGEX="$( printf "\)\|\(%s" "$@" )"
	REGEX="${REGEX:4}\)"
else
	REGEX=".\{1,\}"
fi
TESTS=$(find "${BASEDIR}/${GROUP}" -maxdepth 1 -type d ! -path "${BASEDIR}/${GROUP}" -exec expr match {} "^.\{1,\}/\(${REGEX}\)$" ";" | sort)

if [ -z ${OS-} -o -z ${OSVERSION-} ] ; then
	# Determine OS & its version
	if [ ! -f /etc/os-release ] ; then
		echo "Missing /etc/os-release" >&2
		exit 1;
	fi
	source /etc/os-release

	if [ -z ${ID-} ] ; then
		if [ -z ${ID_LIKE-} ] ; then
			echo "No OS information (in /etc/os-release)" >&2
			exit 1;
		else
			OS=${ID_LIKE}
		fi
	else
		OS=${ID}
	fi
	if [ -z ${VERSION_CODENAME-} ] ; then
		if [ -z ${VERSION_ID-} ] ; then
			echo "No OS version information" >&2
			exit 1;
		fi
		OSVERSION=${VERSION_ID%%.*}
	else
		OSVERSION=${VERSION_CODENAME}
	fi
fi
OS="${OS/-/}"

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
		echo "Missing RTLD ${LD_PATH}" >&2
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
../gen-libpath.sh /etc/ld.so.conf | grep -v "i386\|i486\|i686\|lib32\|libx32" > "$LD_LIBRARY_CONF" || true

if $DEBUG_OUTPUT ; then
	make -C "${BASEDIR}/../tools" stdlog
fi

# Title line
echo -e "\n\e[1;4mRunning Tests on ${OS} ${OSVERSION} (${PLATFORM}) with ${COMPILER}\e[0m"
echo "using ${LD_NAME} RTLD at ${LD_PATH}"
if [ $LD_DYNAMIC_UPDATE -ne 0 ] ; then
	echo "with dynamic updates enabled"
	UPDATEFLAG='update'
else
	UPDATEFLAG='static'
fi

# Check userfaultfd permission
USERFAULTFD="nouserfaultfd"
LD_DETECT_OUTDATED=disabled
LD_DETECT_OUTDATED_DELAY=1
if [ -f /proc/sys/vm/unprivileged_userfaultfd ] ; then
	if getcap "$(readlink -f "${LD_PATH}")" | grep -i "cap_sys_ptrace=eip" ; then
		echo "with outdated code detection (userfaultfd via capabilities)"
		LD_DETECT_OUTDATED=userfaultfd
		USERFAULTFD="userfaultfd"
	elif [[ $(cat /proc/sys/vm/unprivileged_userfaultfd) -eq 1 ]] ; then
		echo "with outdated code detection (userfaultfd via sysfs)"
		LD_DETECT_OUTDATED=userfaultfd
		USERFAULTFD="userfaultfd"
	fi
fi

function check() {
	for file in $1{-${OS,,},}{-${OSVERSION,,},}{-${PLATFORM,,},}{-${COMPILER,,},}{-${UPDATEFLAG},}{,-${USERFAULTFD,,}} ; do
		if [ -f "$file" ] ; then
			echo "Checking $file" ;
			if [ -x "$file" ] ; then
				"$file"
			else
				diff -B -w -u "$file" -
			fi
			return
		fi
	done
}

function flag() {
	for SKIPTEST in $1{,-${OS,,}}{,-${OSVERSION,,}}{,-${PLATFORM}}{,-${COMPILER,,}}{,-${UPDATEFLAG}}{,-ld_${LD_NAME,,}}{,-${USERFAULTFD,,}} ; do
		if [ -f "${SKIPTEST}" ] ; then
			return 0
		fi
	done
	return 1
}

function fail() {
	if flag "$1/.mayfail" ; then
		echo -e "\e[33m$2; however this test is allowed to fail!\e[0m" >&2
		FAILED+=( "\e[33m - $3\e[0m" )
	else
		echo -e "\e[31m$2\e[0m" >&2
		if ${STOP_ON_ERROR} ; then
			echo "SHOULDNOTBEHERE"
			exit $4
		else
			FAILED+=( "\e[31m - $3\e[0m" )
			FATAL=$((FATAL + 1))
		fi
	fi
}

FATAL=0
FAILED=()
for TEST in ${TESTS} ; do
	TESTDIR=$(readlink -f "${BASEDIR}/${GROUP}/${TEST}")
	if [ -d "${TESTDIR}" ] ; then
		# Check if we should skip depending on variables
		if ! $FORCE && flag "${TESTDIR}/.skip" ; then
			echo -e "\n(skipping test ${TEST})" >&2
			continue
		fi

		echo -e "\n\e[1mTest ${TEST}\e[0m"

		# Build
		test -f "${TESTDIR}/Makefile" && make LD_PATH="${LD_PATH}" EXEC="${EXEC}" -B -s -C "${TESTDIR}"
		# Check executable
		if [ ! -x "${TESTDIR}/${EXEC}" ] ; then
			echo "No executable file (${TESTDIR}/${EXEC}) found" >&2
			if ${STOP_ON_ERROR} ; then
				exit 1
			else
				FAILED+=( "\e[33m - ${TEST} has no executable file (${TESTDIR}/${EXEC})!\e[0m" )
				continue
			fi
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
		export LD_SKIP_IDENTICAL=1
		export LD_STATUS_INFO=$STATUS
		export LD_EARLY_STATUS_INFO=
		export LD_LIBRARY_PATH="$TESTDIR"

		# Start test thread
		cd "${TESTDIR}"
		EXITCODE=0
		SECONDS=0
		CHECK_OUTPUT=true

		(
			if ${DEBUG_OUTPUT} ; then
				exec "$BASEDIR/../tools/stdlog" -t -e "$STDERR" -e- -o "$STDOUT" -o- "./${EXEC}"
			else
				exec "./${EXEC}" 2>"$STDERR" >"$STDOUT"
			fi
		) &
		TARGET=$!

		# Wait until finished
		while [[ $TIMEOUT -gt 0 ]] ; do
			sleep 1
			if ! kill -0 $TARGET 2>/dev/null ; then
				break;
			elif [[ $SECONDS -gt TIMEOUT ]] ; then
				echo -e "\n\e[31mRuntime of $TIMEOUT seconds exceeded -- stopping PID $TARGET (${EXEC}) via SIGTERM...\e[0m" >&2
				if kill -s SIGTERM $TARGET 2>/dev/null ; then
					sleep $TIMEOUT_KILLDELAY
					if kill -s SIGKILL $TARGET 2>/dev/null ; then
						echo -e "\e[31m(stopped via SIGKILL)\e[0m" >&2
					fi
				fi
				break
			elif ! ${DEBUG_OUTPUT} ; then
				echo -n "."
			fi
		done
		if wait $TARGET ; then
			echo "(finished after ${SECONDS}s)"
		else
			EXITCODE=$?
			if ! ${DEBUG_OUTPUT} ; then
				echo -e "\e[4mstdout\e[0m" >&2
				cat "$STDOUT" >&2
				echo -e "\n\e[4mstderr\e[0m" >&2
				cat "$STDERR" >&2
				echo -e "\n\e[4mstatus\e[0m" >&2
				cat "$STATUS" >&2
			fi
			rm -f "$STDOUT" "$STDERR" "$STATUS"
			fail "${TESTDIR}" "Execution of ${EXEC} (${GROUP}/${TEST}) failed with exit code ${EXITCODE} after ${SECONDS}s" "$TEST (exit code ${EXITCODE})" ${EXITCODE}
			CHECK_OUTPUT=false
		fi
		cd "${BASEDIR}"

		if ${CHECK_OUTPUT} ; then
			# Compare stdout + stderr with example
			if check "${TESTDIR}/.stdout" < "$STDOUT" && check "${TESTDIR}/.stderr" < "$STDERR" && check "${TESTDIR}/.status" < <(test -f "$STATUS" && sed -e "s/) for .*$/)/" "$STATUS" 2>/dev/null || true); then
				rm -f "$STDOUT" "$STDERR" "$STATUS"
			else
				rm -f "$STDOUT" "$STDERR" "$STATUS"
				fail "${TESTDIR}" "Unexpected output content of ${EXEC} (${GROUP}/${TEST}) - runtime ${SECONDS}s" "$TEST (output mismatch)" ${EXITCODE}
			fi
		fi
	else
		echo "Test '${TEST}' (${TESTDIR}) does not exist!" >&2
		if ${STOP_ON_ERROR} ; then
			exit 1
		else
			FAILED+=( "\e[33m - ${TEST} does not exist!\e[0m" )
		fi
	fi
done

if [[ ${#FAILED[@]} -eq 0 ]] ; then
	echo -e "\n\e[32mTests finished successfully\e[0m"
	exit 0
else
	if [[ ${FATAL} -eq 0 ]] ; then
		echo -e "\n\e[33m${#FAILED[@]} test(s) failed (but none fatal):\e[0m"
		EXITCODE=0
	else
		echo -e "\n\e[31m${#FAILED[@]} test(s) failed (with ${FATAL} fatal):\e[0m"
		EXITCODE=1
	fi
	for TEST in "${FAILED[@]}" ; do
		echo -e "$TEST"
	done
	exit ${EXITCODE}
fi
