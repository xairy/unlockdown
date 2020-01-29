#!/bin/bash
# 
# https://github.com/xairy/unlockdown
#
# Andrey Konovalov <andreyknvl@gmail.com>

set -eux

echo 1 > /proc/sys/kernel/sysrq
echo x > /proc/sysrq-trigger

echo "Done! Check dmesg."
