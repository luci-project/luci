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

	if [ -z ${LUCI_DOCKER+isset} ] ; then
		# install required components
		case $ID in
			ubuntu|debian)
				ln -fs /usr/share/zoneinfo/Europe/Berlin /etc/localtime
				export DEBIAN_FRONTEND=noninteractive
				if [ "$VERSION_CODENAME" = "stretch" ] ; then
					sed -i -e '/stretch-updates/d' -e 's/\(security\|deb\).debian.org/archive.debian.org/' /etc/apt/sources.list
					apt-get update
					apt-get install -y build-essential clang-11 file gcc g++ less libcap2-bin make wget
					update-alternatives --install /usr/bin/clang clang /usr/bin/clang-11 110 --slave /usr/bin/clang++ clang++ /usr/bin/clang++-11
				else
					apt-get update
					apt-get install -y build-essential clang file gcc g++ less libcap2-bin make wget
				fi
				if [ $# -eq 0 ] ; then
					apt-get install -y gdb less
				fi
				;;

			almalinux|fedora|ol|rhel)
				yum install -y make clang diffutils gcc gcc-c++ file less make wget
				if [ $# -eq 0 ] ; then
					yum install -y gdb less
				fi
				;;

			opensuse-*)
				zypper install -y clang gcc gcc-c++ file less libcap-progs make wget
				if [ $# -eq 0 ] ; then
					zypper install -y gdb less
				fi
				;;

			*)
				echo "Unknown distribution"
				;;
		esac
	fi

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
	DOCKERARGS=(run)
	DOCKERARGS+=(--rm -it)
	# Permissions
	DOCKERARGS+=(--cap-add=SYS_PTRACE --security-opt seccomp=unconfined --security-opt apparmor=unconfined)
	# X
	if [ -n ${DISPLAY+set} ] && [ -d /tmp/.X11-unix ] ; then
		DOCKERARGS+=(--env="DISPLAY" -v "/tmp/.X11-unix/:/tmp/.X11-unix/")
	fi
	# Luci directory
	DOCKERARGS+=(-v "$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )/.." &> /dev/null && pwd ):${DOCKERBASE}-ro:ro")

	# Image
	IMAGE=$1
	shift
	if ! docker pull -q "${IMAGE}" 2>/dev/null ; then
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

			inf4/luci:ol*)
				IMAGE="inf4/luci:oraclelinux-${IMAGE//*:}"
				;;

			*)
				echo "Unknown docker image ${IMAGE}!" >&2
				exit 1
		esac
	fi
	DOCKERARGS+=("$IMAGE")

	# Execute script (in container)
	DOCKERARGS+=( "${DOCKERBASE}-ro/tools/$( basename -- "${BASH_SOURCE[0]}" )")

	# Script parameter
	DOCKERARGS+=("$@")

	docker ${DOCKERARGS[@]}
else
	echo "Usage: $0 [DOCKER-IMAGE] [COMMAND [ARGS]]"
	echo
	echo "Starts a container with a Luci copy at '$DOCKERBASE' (already installed)"
	echo
	echo "Supported Docker Images:"
	echo "   [original]            [prepared]"
	echo "  almalinux:9           inf4/luci:almalinux-9"
	echo "  debian:bookworm       inf4/luci:debian-bookworm"
	echo "  debian:bullseye       inf4/luci:debian-bullseye"
	echo "  debian:buster         inf4/luci:debian-buster"
	echo "  debian:stretch        inf4/luci:debian-stretch"
	echo "  fedora:36             inf4/luci:fedora-36"
	echo "  fedora:37             inf4/luci:fedora-37"
	echo "  opensuse/leap:15      inf4/luci:opensuseleap-15"
	echo "  oraclelinux:9         inf4/luci:oraclelinux-9"
	echo "  rhel:9                inf4/luci:rhel-9"
	echo "  ubuntu:focal          inf4/luci:ubuntu-focal"
	echo "  ubuntu:jammy          inf4/luci:ubuntu-jammy"
	echo
	echo "(Prepared images have all tools installed and therefore allow faster start, but they might be slightly outdated)"
	echo
	echo "If COMMAND is ommitted, Bash is executed with development utils installed"
	exit 1
fi
