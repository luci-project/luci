#!/bin/bash

for file in "$@" ; do
	while IFS= read -r line ; do
		if [[ $line =~ ^[[:space:]]*include[[:space:]]+(.*)$ ]] ; then
			./$0 ${BASH_REMATCH[1]}
		elif [[ $line =~ ^[[:space:]]*(/.*)[[:space:]]*$ ]] ; then
			echo "${BASH_REMATCH[1]}"
		fi
	done < "$file"
done
