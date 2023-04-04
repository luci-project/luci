#!/bin/bash

export LC_ALL=C

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
		done < <(readelf -n "$1" 2>/dev/null | grep Build)




		OBJDUMP=$(mktemp)
		objdump -WK -t "$1" | sort -u > "$OBJDUMP"

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
		rm "$OBJDUMP"
	else
		echo "GLIBC file '$1' not found" >&2
		exit 1

	fi
}

if [[ $# -ge 1 ]] ; then
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