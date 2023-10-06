#!/bin/bash

export LC_ALL=C
DEBUGINFOD_URL="https://debuginfod.elfutils.org/"

LIBC=( "/lib64/libc.so.6" "/lib/x86_64-linux-gnu/libc.so.6" )

function filter_symbol() {
	while read -r line ; do
		if [[ $line =~ ^[0]*([0-9a-f]+)[\	\ ]+[lg].+[\	\ ]+[0]*([0-9a-f]+)[\	\ ]+(.+)$ ]] ; then
			if [[ "${BASH_REMATCH[3]}" == "$1" ]] ; then
				echo "			{ \"${BASH_REMATCH[3]}\", 0x${BASH_REMATCH[1]}, $((16#${BASH_REMATCH[2]})), $2, reinterpret_cast<uintptr_t>($3) },"
				return 0
			fi
		fi
	done < <( grep "$1" )
	return 1
}

function dump_offsets() {
	if [[ -f "$1" ]] ; then
		BUILDID=""
		BUILDID_ARRAY=""
		while read -r line ; do
			if [[ $line =~ ^[\ ]*Build\ ID:\ ([0-9a-f]+)$ ]] ; then
				BUILDID=${BASH_REMATCH[1]}
				for (( n = 0; n < ${#BASH_REMATCH[1]}; n += 2 )) ; do
					if (( n != 0 )) ; then
						BUILDID_ARRAY+=", "
					fi
					BUILDID_ARRAY+="0x${BASH_REMATCH[1]:$n:2}"
				done
			fi
		done < <(readelf -n "$1" 2>/dev/null | grep Build)

		OBJDUMP=$(mktemp)
		DEBUGINFOD_URLS="$DEBUGINFOD_URL $DEBUGINFOD_URLS" objdump -WK -t "$1" 2>/dev/null | sort -u > "$OBJDUMP"
		if grep "^no symbols$" "$OBJDUMP" >/dev/null ; then
			DEBUGINFO=$(mktemp)
			if wget -O "$DEBUGINFO" "${DEBUGINFOD_URL}/buildid/${BUILDID}/debuginfo" >/dev/null 2>&1 ; then
				objdump -t "$DEBUGINFO" | sort -u > "$OBJDUMP"
				rm "$DEBUGINFO"
			else
				echo "no symbols" > "$OBJDUMP"
			fi
		fi
		if grep "^no symbols$" "$OBJDUMP" >/dev/null ; then
			echo "	// (no debug symbols found)" >&2
		else
			echo "	{"
			echo "		{ $BUILDID_ARRAY },"
			echo "		{"
			cat "$OBJDUMP" | filter_symbol "_dl_addr"             "nops_jmp" "_dl_addr_patch"
			cat "$OBJDUMP" | filter_symbol "__libc_dlopen_mode"   "nops_jmp" "dlopen"
			cat "$OBJDUMP" | filter_symbol "__libc_dlclose"       "nops_jmp" "dlclose"
			cat "$OBJDUMP" | filter_symbol "__libc_dlsym"         "nops_jmp" "dlsym"
			cat "$OBJDUMP" | filter_symbol "__libc_dlvsym"        "nops_jmp" "dlvsym"

			if ! cat "$OBJDUMP" | filter_symbol "_Fork"       "redirect_fork_syscall" "_fork_syscall" ; then
				cat $OBJDUMP | filter_symbol "__libc_fork" "redirect_fork_syscall" "_fork_syscall"
			fi
			echo "		}"
			echo "	},"
		fi
		rm "$OBJDUMP"
	else
		echo "GLIBC file '$1' not found" >&2
		exit 1

	fi
}

if ! command -v objdump &> /dev/null ; then
	echo "objdump not found (install binutils package)" >&2
	exit 1
elif ! command -v wget &> /dev/null ; then
	echo "wget not found (install wget package)" >&2
	exit 1
elif [[ $# -ge 1 ]] ; then
	for FILE in $@ ; do
		echo "	// File $FILE"
		dump_offsets "$FILE"
	done
else
	if [ -f "/etc/os-release" ] ; then
		source "/etc/os-release"
	fi
	if [ -z ${ID+isset} ] ; then
		ID="unknown"
	fi

	case $ID in
		ubuntu|debian)
			LIBCPKG="libc6"
			echo -n "	// ${ID} ${VERSION_ID} $LIBCPKG "
			apt show "$LIBCPKG" 2>/dev/null | grep "Version: "
			dump_offsets "/lib/x86_64-linux-gnu/libc.so.6"
			;;

		almalinux|fedora|ol|rhel)
			echo -n "	// ${ID} ${VERSION_ID} "
			yum info --installed glibc | sed -ne 's/^\(Name\|Version\|Release\)[ ]*: //p' | tr '\n' ' '
			echo
			dump_offsets "/lib64/libc.so.6"
			;;

		opensuse-*)
			echo -n "	// ${ID} ${VERSION_ID} "
			zypper info glibc | sed -ne 's/^\(Name\|Version\)[ ]*: //p' | tr '\n' ' '
			echo
			dump_offsets "/lib64/libc.so.6"
			;;

		*)
			echo "Unknown distribution"
			;;
	esac
fi
