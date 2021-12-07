#!/bin/bash
set -euo pipefail

status=0
prefix="luci"
helpmap="/tmp/${prefix}_uprobes"
tracing="/sys/kernel/debug/tracing"
if [[ ! -d "$tracing" ]] ; then
	echo "Unable to access '$tracing'!" >&2
	status=1
elif [[ $# -eq 0 ]] ; then
	echo "Usage:"
	echo "	$0 [FILE] [[PID]]"
	echo "	$0 func"
	echo "	$0 show"
	echo "	$0 clear"
	echo "	$0 stop"
elif [[ "${1,,}" == "func" ]] ; then
	cat $tracing/trace | sed -n -e 's/^[ ]*\([a-zA-Z0-9_-]\+\)-\([0-9]\+\) .*\.[0-9]\+: \([^:]\+\): (.*$/    \3 (\1 \/ PID \2) /p' | sort -u
elif [[ "${1,,}" == "show" ]] ; then
	boottime=$(sed -n -e 's/btime //p' /proc/stat)
	while IFS= read -r line ; do
		eval $line  # Ugly, dangerous, but fuck it.
		if [[ "$LOGADDR" == *" <- "* ]] ; then
			RETADDR=${LOGADDR%% <- *}
			LOGADDR=${LOGADDR##* <- }
		fi
		mapdata=$(sed "${MAPID}q;d" $helpmap)
		echo -n "$(date -d @$(( boottime + LOGSEC )) +"%Y-%m-%d %H:%M:%S").$LOGMILLIS $mapdata @ $LOGADDR in $LOGCOMM / PID $LOGPID (Core $(( LOGCORE + 0 ))) "
		ret=$LOGADDR
		# try to resolve function
		retrel=$(( RETADDR - LOGADDR + SYMOFFSET ))
		if [[ $retrel -ge 0 ]] ; then
			retrelhex=$(printf '%x\n' $retrel)
			func=$(addr2line -e "${mapdata%%:*}" -f $retrelhex 2>/dev/null | head -n1 || true)
			if [[ -n "$func" && "$func" != "??" ]] ; then
				ret="$func (0x$retrelhex)"
			fi
		fi
		if [[ "$line" != *" RETVAL="* ]] ; then
			echo "called from $ret"
		else
			echo "return $RETVAL to $ret"
		fi
	done < <(cat $tracing/trace | sed -n -e 's/^[ ]*\([a-zA-Z0-9_-]\+\)-\([0-9]\+\)[ ]\+\[\([0-9]\+\)\][ ]\+....[ ]\+\([0-9]\+\).\([0-9]\+\): \([^:]\+\): (\([0-9a-fx <-]\+\))/LOGCOMM="\1" LOGPID=\2 LOGCORE="\3" LOGSEC=\4 LOGMILLIS=\5 LOGSYM="\6" LOGADDR="\7"/p')
elif [[ "${1,,}" == "clear" ]] ; then
	echo "Clear trace log..."
	if ! echo > $tracing/trace ; then
		echo "Clearing trace failed" >&2
		status=1
	fi
elif [[ "${1,,}" == "stop" ]] ; then
	echo "Disabling tracing and removing all uprobes"
	if ! echo 0 > $tracing/tracing_on ; then
		echo "Stopping tracing failed" >&2
		status=1
	fi
	if ! echo > $tracing/trace ; then
		echo "Clearing trace failed" >&2
		status=1
	fi
	if ! echo -n > $helpmap ; then
		echo "Unable to clear $helpmap" >&2
		status=1
	fi
	while IFS= read -r name ; do
		if [[ -d "$tracing/events/$name/" ]] ; then
			if ! echo 0 > $tracing/events/$name/enable ; then
				echo "Unable to disable event '$line'" >&2
			fi
		fi
	done < <(cat $tracing/uprobe_events | sed -e 's|^[rp]:\([^/]\+\)/.*$|\1|' | sort -u)
	if ! echo > $tracing/uprobe_events ; then
		echo "Clearing uprobe events failed" >&2
		status=1
	fi
elif [[ ! -f "$1" ]] ; then
	echo "File '$1' does not exist!" >&2
	status=1
else
	path=$(readlink -f "$1")
	name="$prefix$(echo "$path" | sed -e 's/[^a-z0-9]\+/_/Ig')"
	shortname="$(basename "$path" | sed -e 's/[^a-z0-9]\+/_/Ig')"
	symfiles=( $path )
	if buildid=$(readelf -n "$path" | sed -n -e 's/.*Build-ID: \([0-9a-f]*\)$/\1/p') ; then
		symfiles+=( "/usr/lib/debug/.build-id/${buildid:0:2}/${buildid:2}.debug" )
	fi
	dbgpath="$(dirname "$path")/.debug/$(basename "$path")"
	symfiles+=( "$path.debug" )
	symfiles+=( "$dbgpath.debug" )
	symfiles+=( "/usr/lib/debug$path.debug" )
	# inoffical
	symfiles+=( "$dbgpath" )
	symfiles+=( "/usr/lib/debug$path" )

	# Get all symbols
	baseaddr=$(readelf -W -l "$path" | sed -n -e 's/^[ ]*LOAD[ ]\+0x[0-9a-f]\+ \(0x[0-9a-f]\+\) 0x.*$/\1/p' | head -n1)
	for symfile in $symfiles ; do
		if [[ -f "$symfile" ]] ; then
			while IFS= read -r line ; do
				offset="0x${line%% *}"
				sym=${line##* }
				echo "$path:$sym ($offset)" >> $helpmap
				addr=$(( offset - baseaddr ))
				mapid=$(cat $helpmap | wc -l)
				if ! echo "p:${name}/${shortname}_${sym} $path:$addr STACK=\$stack RETADDR=\$stack0 SYMOFFSET=\\${offset} MAPID=\\${mapid}:u32" >> $tracing/uprobe_events ; then
					echo "Adding entry ${name}/${shortname}_${sym} @ $path:$addr ($offset - $baseaddr) failed" >&2
					status=1
				fi
				if ! echo "r:${name}/${shortname}_${sym}_ret $path:$addr STACK=\$stack SYMOFFSET=\\${offset} RETVAL=\$retval MAPID=\\${mapid}:u32" >> $tracing/uprobe_events ; then
					echo "Adding return ${name}/${shortname}_${sym}_ret @ $path:$addr ($offset - $baseaddr) failed" >&2
					status=1
				fi
			done < <(objdump -tT "$symfile" | sed -n -e 's/^[0]\+\([0-9a-f]\+\) .*F \.text\t[0-9a-f].* \([^ ]*\)$/\1 \2/p' | sort -n -s -k1,1 -u)
		fi
	done

	entries=$(cat $tracing/uprobe_events | grep "^p:$name/" | wc -l)
	if [[ $entries -eq 0 ]] ; then
		echo "No symbols have been added...." >&2
	else
		echo "Added $entries symbols to uprobes (event $name)"

		if [[ $# -gt 1 ]] ; then
			# Recursivly get PIDs
			declare -A pids
			function get_pids_rec() {
				for pid in $@ ; do
					if [[ ! ${pids[$pid]+exist} ]] ; then
						pids[$pid]=$pid
						for taskdir in /proc/$pid/task/* ; do
							tid=${taskdir##*/}
							pids[$tid]=$tid
							get_pids_rec $(cat $taskdir/children)
						done
					fi
				done
			}
			get_pids_rec $2

			if [[ ${#pids[@]} -eq 0 ]] ; then
				echo "No PID found (given: $2)" >&2
				status=1
			else
				echo "Limit to ${#pids[@]} PIDs (${!pids[@]})"
				filter=""
				for pid in "${!pids[@]}" ; do
					filter+=" || common_pid == $pid"
				done
				if ! echo "${filter:3}" > $tracing/events/$name/filter ; then
					echo "Applying PID filter failed..." >&2
					status=1
				fi
			fi
		fi

		# enable
		if ! echo 1 > $tracing/events/$name/enable ; then
			echo "Enabling $name events failed" >&2
			status=1
		fi
		if ! echo nop > $tracing/current_tracer ; then
			echo "Setting nop tracer failed" >&2
			status=1
		fi
		if ! echo 1 > $tracing/tracing_on ; then
			echo "Starting tracing failed" >&2
			status=1
		fi
	fi
fi
exit $status
