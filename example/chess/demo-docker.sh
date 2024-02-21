#!/bin/bash
set -euf -o pipefail

REL_DIR="example/chess"
DOCKERBASE="/builds/luci"

if [ -f "/.dockerenv" ] ; then
	if [ -f "/etc/os-release" ] ; then
		source "/etc/os-release"
	fi
	if [ -z ${ID+isset} ] ; then
		ID="unknown"
	fi

	# install required components
	case $ID in
		ubuntu|debian)
			ln -fs /usr/share/zoneinfo/Europe/Berlin /etc/localtime
			export DEBIAN_FRONTEND=noninteractive
			apt-get update
			apt-get install -y xterm libsdl-image1.2-dev fonts-ubuntu-console
			TERMINAL=xterm $REL_DIR/demo.sh
			;;

		almalinux)
			dnf install -y epel-release almalinux-release-devel
			dnf install -y patch SDL_image-devel xterm dejavu-sans-mono-fonts

			# Full support
			TERMINAL=xterm $REL_DIR/demo.sh --no-pie
			;;

		fedora)
			# official SDL2
			#dnf install -y patch SDL_image-devel xterm dejavu-sans-mono-fonts
			#echo -e "\n\e[31mIncompatible SDL2 - Luci might currently not able to load some of the shared objects due to TLS DESC relocations!\e[0m\n"

			# custom SDL (1.2)
			dnf install -y patch xterm dejavu-sans-mono-fonts
			rpm -i "https://www.libsdl.org/release/SDL-1.2.15-1.x86_64.rpm"
			rpm -i "https://www.libsdl.org/release/SDL-devel-1.2.15-1.x86_64.rpm"
			rpm -i "https://www.libsdl.org/projects/SDL_image/release/SDL_image-1.2.12-1.x86_64.rpm"
			rpm -i "https://www.libsdl.org/projects/SDL_image/release/SDL_image-devel-1.2.12-1.x86_64.rpm"
			ln -s libpng16.so.16 /lib64/libpng.so.3
			echo -e "\n\e[31mIncompatible libPNG - SDL chess pieces might not be visible!\e[0m\n"

			TERMINAL=xterm $REL_DIR/demo.sh --no-pie
			;;

		rhel)
			dnf install -y --disablerepo=* --enablerepo=ubi-9-appstream-rpms --enablerepo=ubi-9-baseos-rpms xorg* patch
			# xterm
			rpm -i "https://repo.almalinux.org/almalinux/9/AppStream/x86_64/kickstart/Packages/libXpm-3.5.13-8.el9_1.x86_64.rpm"
			rpm -i "https://repo.almalinux.org/almalinux/9/AppStream/x86_64/kickstart/Packages/libXaw-1.0.13-19.el9.x86_64.rpm"
			rpm -i "https://repo.almalinux.org/almalinux/9/AppStream/x86_64/kickstart/Packages/libXft-2.3.3-8.el9.x86_64.rpm"
			rpm -i "https://repo.almalinux.org/almalinux/9/AppStream/x86_64/kickstart/Packages/xterm-resize-366-9.el9.x86_64.rpm"
			rpm -i "https://repo.almalinux.org/almalinux/9/AppStream/x86_64/kickstart/Packages/xterm-366-9.el9.x86_64.rpm"

			# SDL
			rpm -i "https://www.libsdl.org/release/SDL-1.2.15-1.x86_64.rpm"
			rpm -i "https://www.libsdl.org/release/SDL-devel-1.2.15-1.x86_64.rpm"
			rpm -i "https://www.libsdl.org/projects/SDL_image/release/SDL_image-1.2.12-1.x86_64.rpm"
			rpm -i "https://www.libsdl.org/projects/SDL_image/release/SDL_image-devel-1.2.12-1.x86_64.rpm"
			ln -s libpng.so /lib64/libpng.so.3
			echo -e "\n\e[31mIncompatible libPNG - SDL chess pieces might not be visible!\e[0m\n"

			TERMINAL=xterm $REL_DIR/demo.sh --no-pie
			;;

		ol)
			dnf install -y epel-release
			rpm -i "https://www.libsdl.org/release/SDL-1.2.15-1.x86_64.rpm"
			rpm -i "https://www.libsdl.org/release/SDL-devel-1.2.15-1.x86_64.rpm"
			dnf install -y patch SDL_image-devel xterm dejavu-sans-mono-fonts

			# Full support
			TERMINAL=xterm $REL_DIR/demo.sh --no-pie
			;;

		opensuse-*)
			zypper install -y patch SDL_image-devel xterm ubuntu-fonts
			echo -e "\n\e[31mNo unicode fonts installed, hence the Unicode example might look weird!\e[0m\n"
			TERMINAL=xterm $REL_DIR/demo.sh --no-pie
			;;

		*)
			echo "Unknown/unsupported distribution"
			exit 1
			;;
	esac

	# Spawn bash to examine files
	echo -e "\n\e[2mDemo finished. Starting Bash in container - type 'exit' to quit!\e[0m\n"
	cd $REL_DIR
	exec /bin/bash
elif [ $# -eq 1 ] ; then
	if [ -z "$DISPLAY" ] ; then
		export DISPLAY=:0.0
	fi
	xhost +local:docker

	SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]%""}" )/../../" &> /dev/null && pwd)
	"${SCRIPT_DIR%"$REL_DIR/demo-docker.sh"}/tools/docker.sh" $1 "$DOCKERBASE/$REL_DIR/demo-docker.sh"
else
	echo "Usage: $0 [DOCKER-IMAGE]"
	echo
	echo "Starts a container for the chess demo (with X)"
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
	exit 1
fi
