#!/bin/bash
set -euf -o pipefail

DOCKERBASE="/builds/luci"

if [ -f "/.dockerenv" ] ; then
	if [ -f "/etc/os-release" ] ; then
		source "/etc/os-release"
	fi
	if [ -z ${ID+isset} ] ; then
		ID="unknown"
	fi

	case $ID in
		ubuntu|debian)
			ln -fs /usr/share/zoneinfo/Europe/Berlin /etc/localtime
			export DEBIAN_FRONTEND=noninteractive
			apt-get update
			apt-get install -y make libcap2-bin gcc g++
			if [ $# -eq 0 ] ; then
				apt-get install -y gdb less
			fi
			;;

		almalinux|fedora|ol|rhel)
			yum install -y make diffutils gcc gcc-c++
			if [ $# -eq 0 ] ; then
				yum install -y gdb less
			fi
			;;

		opensuse-*)
			zypper install -y make libcap-progs gcc gcc-c++
			if [ $# -eq 0 ] ; then
				zypper install -y gdb less
			fi
			;;

		*)
			echo "Unknown distribution"
			;;
	esac

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
	IMAGE=$1
	shift
	if [ "$(docker images -q "${IMAGE}" 2> /dev/null)" == "" ] ; then
		case "${IMAGE}" in
			ol:*)
				IMAGE="oraclelinux:${IMAGE//*:}"
				;;

			opensuseleap:*)
				IMAGE="opensuse/leap:${IMAGE//*:}"
				;;

			rhel:*)
				IMAGE="redhat/ubi${IMAGE//*:}"
				;;

			*)
				echo "Unknown docker image ${IMAGE}!" >&2
				exit 1
		esac
	fi

	docker run --rm -it --cap-add=SYS_PTRACE --security-opt seccomp=unconfined --security-opt apparmor=unconfined -v $( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )/.." &> /dev/null && pwd ):${DOCKERBASE}-ro:ro "$IMAGE" "${DOCKERBASE}-ro/tools/$( basename -- "${BASH_SOURCE[0]}" )" "$@"
else
	echo "Usage: $0 [DOCKER-IMAGE] [COMMAND [ARGS]]"
	echo
	echo "If COMMAND is ommitted, Bash is executed with development utils installed"
	exit 1
fi
