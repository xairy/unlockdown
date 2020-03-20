#!/bin/bash

set -eux

echo 1 > /proc/sys/kernel/sysrq

gcc ./evdev-sysrq.c -o evdev-sysrq
./evdev-sysrq
