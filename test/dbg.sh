#!/bin/bash

export DEBIAN_FRONTEND=noninteractive

# perform updates
apt-get --allow-unauthenticated update
apt-get --yes --force-yes dist-upgrade
apt-get --allow-unauthenticated update
apt-get --yes --force-yes install locales-all gcc g++ make tmux file libpcre3 binutils less nano

# Set language
export LC_ALL=en_US.UTF-8
export LANG=en_US.UTF-8
export LANGUAGE=en_US.UTF-8

# GDB
echo 'deb-src http://deb.debian.org/debian oldoldstable main' >> /etc/apt/sources.list
echo 'deb http://deb.debian.org/debian-debug oldoldstable-debug main' >> /etc/apt/sources.list
apt-get --allow-unauthenticated update
apt-get --yes --force-yes install curl python3 python3-pip gdb procps
curl -fsSL http://gef.blah.cat/sh | bash
pip3 install ropper keystone-engine unicorn capstone

# Luci
# generate config
LD_LIBRARY_CONF=$(readlink -f "libpath.conf")
../gen-libpath.sh /etc/ld.so.conf | grep -v "i386\|i486\|i686\|lib32\|libx32" > "$LD_LIBRARY_CONF"
export LD_LIBRARY_CONF
export LD_LOGLEVEL=6

exec /bin/bash
