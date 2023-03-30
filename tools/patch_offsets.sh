#!/bin/bash

export LC_ALL=C

LIBCPKG="libc6"
LIBC="/lib/x86_64-linux-gnu/libc.so.6"

if [[ $# -ge 1 ]] ; then
	LIBC="$1"
else
	echo -n "	// $LIBCPKG "
	apt show "$LIBCPKG" 2>/dev/null | grep "Version: "
fi
if [[ ! -f "$LIBC" ]] ; then
	echo "GLIBC file '$LIBC' not found" >&2
	exit 1
fi

echo "	{"
while read -r line ; do
	if [[ $line =~ ^[\ ]*Build\ ID:\ ([0-9a-f]+)$ ]] ; then
		echo -n "		{ "
		for (( n = 0; n < ${#BASH_REMATCH[1]}; n += 2 )) ; do
			if (( n != 0 )) ; then
				echo -n ", "
			fi
			echo -n "0x${BASH_REMATCH[1]:$n:2}"
		done
		echo " },"
	fi
done < <(readelf -n "$LIBC" 2>/dev/null | grep Build)


function filter_symbol() {
	grep "$1" | while read -r line ; do
		if [[ $line =~ ^[0]*([0-9a-f]+)\ [lg].+\	[0]*([0-9a-f]+)\ (.+)$ ]] ; then
			if [[ "${BASH_REMATCH[3]}" == "$1" ]] ; then
				echo "			{ \"${BASH_REMATCH[3]}\", 0x${BASH_REMATCH[1]}, $((16#${BASH_REMATCH[2]})), $2, reinterpret_cast<uintptr_t>($3) },"
			fi
		fi
	done
}

OBJDUMP=$(mktemp)
objdump -WK -t "$LIBC" | sort -u > $OBJDUMP

echo "		{ "
cat $OBJDUMP | filter_symbol "_dl_addr"             "nops_jmp" "_dl_addr_patch"
cat $OBJDUMP | filter_symbol "__libc_dlopen_mode"   "nops_jmp" "dlopen"
cat $OBJDUMP | filter_symbol "__libc_dlclose"       "nops_jmp" "dlclose"
cat $OBJDUMP | filter_symbol "__libc_dlsym"         "nops_jmp" "dlsym"
cat $OBJDUMP | filter_symbol "__libc_dlvsym"        "nops_jmp" "dlvsym"

cat $OBJDUMP | filter_symbol "_Fork"       "redirect_fork_syscall" "_fork_syscall"
#cat $OBJDUMP | filter_symbol "__libc_fork" "redirect_fork_syscall" "_fork_syscall"
echo "		} "
echo "	},"
