#!/bin/bash
set -euo pipefail

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

NAME="Luci"
RTLD="$(readlink -f "../ld-${NAME,,}-${ID,,}-${VERSION_CODENAME,,}-${PLATFORM,,}.so")"
if [ ! -x "${RTLD}" ] ; then
	echo "Missing RTLD ${NAME} (${RTLD})" >&2
	exit 1
fi

echo -e "\e[1;4mRunning Tests on ${ID} ${VERSION_CODENAME} (${PLATFORM})\e[0m"
echo "using ${RTLD}"
ls -lisah "${RTLD}"

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

BINARY="run"
for TEST in * ; do
	if [ -d "${TEST}" ] ; then
		echo -e "\n\e[1mTest ${TEST}\e[0m"
		# Build
		test -f "${TEST}/Makefile" &&  make RTLD="${RTLD}" BINARY="${BINARY}" -B -s -C "${TEST}"
		# Check executable
		if [ ! -x "${TEST}/${BINARY}" ] ; then
			echo "No executable file (${TEST}/${BINARY}) found" >&2
			exit 1
		fi

		LD_LOGLEVEL=6 ${TEST}/${BINARY}

		# Execute and capture stdout + stderr
		STDOUT=$(mktemp)
		STDERR=$(mktemp)
		"${TEST}/${BINARY}" >"$STDOUT" 2>"$STDERR"

		# Compare stdout + stderr with example
		check "${TEST}/.stdout" < "$STDOUT"
		check "${TEST}/.stderr" < "$STDERR"

		# Remove files
		rm "$STDOUT" "$STDERR"
	fi
done
