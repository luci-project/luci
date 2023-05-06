#!/bin/bash
set -euf -o pipefail

DOCKERBASE="/builds/luci"
IMAGEBASE="inf4/luci"

if [ -f "/.dockerenv" ] ; then
	cp -a "${DOCKERBASE}-ro" "$DOCKERBASE"
	mkdir -p "/opt/luci"
	make -C "$DOCKERBASE" install-only || true
	export PATH="$PATH:/opt/luci/:$DOCKERBASE/bean/:$DOCKERBASE/bean/elfo/"
	cd "$DOCKERBASE"
	if [ $# -eq 0 ] ; then
		exec /bin/bash
	else
		exec "$@"
	fi
elif [ $# -ge 1 ] ; then
	DIST_ID=$1
	DIST_VERS=$2
	if [ "${DIST_ID,,}" = "ol" ] ; then
		DIST_ID="oraclelinux"
	fi
	shift 2
	IMAGE="${IMAGEBASE}:${DIST_ID,,}-${DIST_VERS,,}"
	if ! docker pull -q "${IMAGE}" 2>/dev/null ; then
		echo "Unknown docker image ${IMAGE}!" >&2
		exit 1
	fi

	docker run --rm -it --cap-add=SYS_PTRACE --security-opt seccomp=unconfined --security-opt apparmor=unconfined -v $( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )/.." &> /dev/null && pwd ):${DOCKERBASE}-ro:ro "${IMAGE}" "${DOCKERBASE}-ro/tools/$( basename -- "${BASH_SOURCE[0]}" )" "$@"
else
	echo "Usage: $0 [DISTRIBUTION NAME] [DISTRIBUTION VERSION] [COMMAND [ARGS]]"
	echo
	echo "If COMMAND is ommitted, Bash is executed with development utils installed"
	exit 1
fi
