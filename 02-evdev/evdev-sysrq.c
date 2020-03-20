// Disables kernel lockdown on Ubuntu kernels by injecting an Alt+SysRq+X
// key combination through evdev.
// See https://github.com/xairy/unlockdown for details.
//
// Vaguely based on:
// https://github.com/darkelement/simplistic-examples/blob/master/evdev/evdev.c
// https://android.googlesource.com/product/google/common/+/refs/heads/master/keyboard_example/keyboard_example.cpp
//
// Andrey Konovalov <andreyknvl@gmail.com>

#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/input.h>

bool supports_feature(int fd, unsigned int feature) {
	unsigned long evbit = 0;
	int rv = ioctl(fd, EVIOCGBIT(0, sizeof(evbit)), &evbit);
	if (rv != sizeof(evbit)) {
		perror("ioctl(EVIOCGBIT)");
		exit(EXIT_FAILURE);
	}
	return evbit & (1 << feature);
}

bool supports_key(int fd, unsigned int key) {
	size_t nchar = KEY_MAX / 8 + 1;
	unsigned char bits[nchar];
	int rv = ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(bits)), &bits);
	if (rv != sizeof(bits)) {
		perror("ioctl(EVIOCGBIT)");
		exit(EXIT_FAILURE);
	}
	return bits[key / 8] & (1 << (key % 8));
}

bool check_device(int fd) {
	if (!supports_feature(fd, EV_KEY))
		return false;
	printf("EV_KEY supported\n");
	if (!supports_key(fd, KEY_SYSRQ))
		return false;
	printf("KEY_SYSRQ supported\n");
	if (!supports_feature(fd, EV_SYN))
		return false;
	printf("EV_SYN supported\n");
	return true;
}

int is_event_device(const struct dirent *dir) {
	return strncmp("event", dir->d_name, strlen("event")) == 0;
}

int find_device(void) {
	struct dirent **names;
	int n = scandir("/dev/input", &names, is_event_device, alphasort);
	if (n <= 0) {
		fprintf(stderr, "no input devices found\n");
		exit(EXIT_FAILURE);
	}

	for (int i = 0; i < n; i++) {
		char name[512];
		snprintf(&name[0], sizeof(name), "%s/%s", "/dev/input",
				names[i]->d_name);
		free(names[i]);

		int fd = open(name, O_RDWR);
		if (fd < 0) {
			perror("open()");
			exit(EXIT_FAILURE);
		}
		printf("checking %s\n", &name[0]);
		if (check_device(fd)) {
			printf("found device %s\n", &name[0]);
			return fd;
		}
		close(fd);
	}

	fprintf(stderr, "no input devices support sysrq injection\n");
	exit(EXIT_FAILURE);
}

void write_event(int fd, unsigned int type, unsigned int code,
				unsigned int value) {
	struct input_event event;
	memset(&event, 0, sizeof(event));
	event.type = type;
	event.code = code;
	event.value = value;
	int rv = write(fd, &event, sizeof(event));
	if (rv != sizeof(event)) {
		perror("write()");
		exit(EXIT_FAILURE);
	}
}

void disable_lockdown(int fd) {
	printf("sending Alt+SysRq+X sequence\n");

	write_event(fd, EV_KEY, KEY_LEFTALT, 1);
	write_event(fd, EV_KEY, KEY_SYSRQ, 1);
	write_event(fd, EV_KEY, KEY_X, 1);

	write_event(fd, EV_KEY, KEY_X, 0);
	write_event(fd, EV_KEY, KEY_SYSRQ, 0);
	write_event(fd, EV_KEY, KEY_LEFTALT, 0);

	write_event(fd, EV_SYN, 0, 0);

	printf("done\n");
}

int main () {
	int fd = find_device();
	disable_lockdown(fd);
}
