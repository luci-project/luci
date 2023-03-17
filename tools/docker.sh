#!/bin/bash
set -euf -o pipefail

DOCKERBASE="/builds/luci"

if [ -f "/.dockerenv" ] ; then
	ln -fs /usr/share/zoneinfo/Europe/Berlin /etc/localtime
	export DEBIAN_FRONTEND=noninteractive
	apt-get update
	apt-get install -y make libcap2-bin
	cp -a "${DOCKERBASE}-ro" "$DOCKERBASE"
	mkdir -p "/opt/luci"
	make -C "$DOCKERBASE" install-only
	export PATH="$PATH:/opt/luci/:$DOCKERBASE/bean/:$DOCKERBASE/bean/elfo/"
	cd "$DOCKERBASE"
	exec "$@"
elif [ $# -gt 1 ] ; then
	IMAGE=$1
	shift
	docker run --rm -it --cap-add=SYS_PTRACE --security-opt seccomp=unconfined --security-opt apparmor=unconfined -v $( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )/.." &> /dev/null && pwd ):${DOCKERBASE}-ro:ro "$IMAGE" "${DOCKERBASE}-ro/tools/$( basename -- "${BASH_SOURCE[0]}" )" "$@"
else
	echo "Usage: $0 [DOCKER-IMAGE] [COMMAND [ARGS]]"
	exit 1
fi
