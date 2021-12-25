// Disables kernel lockdown on Ubuntu kernels by emulating a USB keyboard
// over USB/IP and sending a Alt+SysRq+X key combination.
// See https://github.com/xairy/unlockdown for usage details.
//
// Derived from:
// - https://github.com/xairy/raw-gadget/blob/master/examples/keyboard.c
// - https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/tools/usb/usbip/libsrc/usbip_common.h
// - https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/drivers/usb/usbip/usbip_common.h
// - https://github.com/lcgamboa/USBIP-Virtual-USB-Device
//
// Andrey Konovalov <andreyknvl@gmail.com>

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

#include <linux/hid.h>
#include <linux/usb/ch9.h>

#define USBIP_PORT 3240

/*----------------------------------------------------------------------*/

struct hid_class_descriptor {
	__u8 bDescriptorType;
	__le16 wDescriptorLength;
} __attribute__((packed));

struct hid_descriptor {
	__u8 bLength;
	__u8 bDescriptorType;
	__le16 bcdHID;
	__u8 bCountryCode;
	__u8 bNumDescriptors;

	struct hid_class_descriptor desc[1];
} __attribute__((packed));

/*----------------------------------------------------------------------*/

#define MAX_PACKET_SIZE 64

#define USB_VENDOR 0x046d
#define USB_PRODUCT 0xc312

#define STRING_ID_MANUFACTURER 0
#define STRING_ID_PRODUCT 1
#define STRING_ID_SERIAL 2
#define STRING_ID_CONFIG 3
#define STRING_ID_INTERFACE 4

struct usb_device_descriptor usb_device = {
	.bLength = USB_DT_DEVICE_SIZE,
	.bDescriptorType = USB_DT_DEVICE,
	.bcdUSB = __constant_cpu_to_le16(0x0200),
	.bDeviceClass = 0,
	.bDeviceSubClass = 0,
	.bDeviceProtocol = 0,
	.bMaxPacketSize0 = MAX_PACKET_SIZE,
	.idVendor = __constant_cpu_to_le16(USB_VENDOR),
	.idProduct = __constant_cpu_to_le16(USB_PRODUCT),
	.bcdDevice = 0,
	.iManufacturer = STRING_ID_MANUFACTURER,
	.iProduct = STRING_ID_PRODUCT,
	.iSerialNumber = STRING_ID_SERIAL,
	.bNumConfigurations = 1,
};

struct usb_qualifier_descriptor usb_qualifier = {
	.bLength = sizeof(struct usb_qualifier_descriptor),
	.bDescriptorType = USB_DT_DEVICE_QUALIFIER,
	.bcdUSB = __constant_cpu_to_le16(0x0200),
	.bDeviceClass = 0,
	.bDeviceSubClass = 0,
	.bDeviceProtocol = 0,
	.bMaxPacketSize0 = MAX_PACKET_SIZE,
	.bNumConfigurations = 1,
	.bRESERVED = 0,
};

struct usb_config_descriptor usb_config = {
	.bLength =		USB_DT_CONFIG_SIZE,
	.bDescriptorType =	USB_DT_CONFIG,
	.wTotalLength =		0,  // computed later
	.bNumInterfaces =	1,
	.bConfigurationValue =	1,
	.iConfiguration = 	STRING_ID_CONFIG,
	.bmAttributes =		USB_CONFIG_ATT_ONE | USB_CONFIG_ATT_SELFPOWER,
	.bMaxPower =		0x32,
};

struct usb_interface_descriptor usb_interface = {
	.bLength =		USB_DT_INTERFACE_SIZE,
	.bDescriptorType =	USB_DT_INTERFACE,
	.bInterfaceNumber =	0,
	.bAlternateSetting =	0,
	.bNumEndpoints =	1,
	.bInterfaceClass =	USB_CLASS_HID,
	.bInterfaceSubClass =	1,
	.bInterfaceProtocol =	1,
	.iInterface =		STRING_ID_INTERFACE,
};

struct usb_endpoint_descriptor usb_endpoint = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN | 1,
	.bmAttributes =		USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize =	8,
	.bInterval =		5,
};

char usb_hid_report[] = {
	0x05, 0x01,                    // Usage Page (Generic Desktop)        0
	0x09, 0x06,                    // Usage (Keyboard)                    2
	0xa1, 0x01,                    // Collection (Application)            4
	0x05, 0x07,                    //  Usage Page (Keyboard)              6
	0x19, 0xe0,                    //  Usage Minimum (224)                8
	0x29, 0xe7,                    //  Usage Maximum (231)                10
	0x15, 0x00,                    //  Logical Minimum (0)                12
	0x25, 0x01,                    //  Logical Maximum (1)                14
	0x75, 0x01,                    //  Report Size (1)                    16
	0x95, 0x08,                    //  Report Count (8)                   18
	0x81, 0x02,                    //  Input (Data,Var,Abs)               20
	0x95, 0x01,                    //  Report Count (1)                   22
	0x75, 0x08,                    //  Report Size (8)                    24
	0x81, 0x01,                    //  Input (Cnst,Arr,Abs)               26
	0x95, 0x03,                    //  Report Count (3)                   28
	0x75, 0x01,                    //  Report Size (1)                    30
	0x05, 0x08,                    //  Usage Page (LEDs)                  32
	0x19, 0x01,                    //  Usage Minimum (1)                  34
	0x29, 0x03,                    //  Usage Maximum (3)                  36
	0x91, 0x02,                    //  Output (Data,Var,Abs)              38
	0x95, 0x05,                    //  Report Count (5)                   40
	0x75, 0x01,                    //  Report Size (1)                    42
	0x91, 0x01,                    //  Output (Cnst,Arr,Abs)              44
	0x95, 0x06,                    //  Report Count (6)                   46
	0x75, 0x08,                    //  Report Size (8)                    48
	0x15, 0x00,                    //  Logical Minimum (0)                50
	0x26, 0xff, 0x00,              //  Logical Maximum (255)              52
	0x05, 0x07,                    //  Usage Page (Keyboard)              55
	0x19, 0x00,                    //  Usage Minimum (0)                  57
	0x2a, 0xff, 0x00,              //  Usage Maximum (255)                59
	0x81, 0x00,                    //  Input (Data,Arr,Abs)               62
	0xc0,                          // End Collection                      64
};

struct hid_descriptor usb_hid = {
	.bLength =		9,
	.bDescriptorType =	HID_DT_HID,
	.bcdHID =		__constant_cpu_to_le16(0x0110),
	.bCountryCode =		0,
	.bNumDescriptors =	1,
	.desc =			{
		{
			.bDescriptorType =	HID_DT_REPORT,
			.wDescriptorLength =	sizeof(usb_hid_report),
		}
	},
};

int build_config(char *data, int length) {
	struct usb_config_descriptor *config =
		(struct usb_config_descriptor *)data;
	int total_length = 0;

	assert(length >= sizeof(usb_config));
	memcpy(data, &usb_config, sizeof(usb_config));
	data += sizeof(usb_config);
	length -= sizeof(usb_config);
	total_length += sizeof(usb_config);

	assert(length >= sizeof(usb_interface));
	memcpy(data, &usb_interface, sizeof(usb_interface));
	data += sizeof(usb_interface);
	length -= sizeof(usb_interface);
	total_length += sizeof(usb_interface);

	assert(length >= sizeof(usb_hid));
	memcpy(data, &usb_hid, sizeof(usb_hid));
	data += sizeof(usb_hid);
	length -= sizeof(usb_hid);
	total_length += sizeof(usb_hid);

	assert(length >= USB_DT_ENDPOINT_SIZE);
	memcpy(data, &usb_endpoint, USB_DT_ENDPOINT_SIZE);
	data += USB_DT_ENDPOINT_SIZE;
	length -= USB_DT_ENDPOINT_SIZE;
	total_length += USB_DT_ENDPOINT_SIZE;

	config->wTotalLength = __cpu_to_le16(total_length);
	printf("config->wTotalLength: %d\n", total_length);

	return total_length;
}

/*----------------------------------------------------------------------*/

// tools/usb/usbip/libsrc/usbip_common.h

#define SYSFS_PATH_MAX		256
#define SYSFS_BUS_ID_SIZE	32

#define OP_REQUEST		(0x80 << 8)
#define OP_REPLY		(0x00 << 8)

#define OP_IMPORT		0x03
#define OP_REQ_IMPORT		(OP_REQUEST | OP_IMPORT)
#define OP_REP_IMPORT   	(OP_REPLY   | OP_IMPORT)

#define USBIP_CMD_SUBMIT	0x0001
#define USBIP_CMD_UNLINK	0x0002
#define USBIP_RET_SUBMIT	0x0003
#define USBIP_RET_UNLINK	0x0004

struct usbip_usb_device {
	char path[SYSFS_PATH_MAX];
	char busid[SYSFS_BUS_ID_SIZE];

	uint32_t busnum;
	uint32_t devnum;
	uint32_t speed;

	uint16_t idVendor;
	uint16_t idProduct;
	uint16_t bcdDevice;

	uint8_t bDeviceClass;
	uint8_t bDeviceSubClass;
	uint8_t bDeviceProtocol;
	uint8_t bConfigurationValue;
	uint8_t bNumConfigurations;
	uint8_t bNumInterfaces;
} __attribute__((packed));

struct usbip_usb_interface {
        uint8_t bInterfaceClass;
        uint8_t bInterfaceSubClass;
        uint8_t bInterfaceProtocol;
        uint8_t padding;
} __attribute__((packed));

struct usbip_op_common {
	uint16_t version;
	uint16_t code;
	uint32_t status;
} __attribute__((packed));

struct usbip_op_import_request {
	char busid[SYSFS_BUS_ID_SIZE];
} __attribute__((packed));

struct usbip_op_import_reply {
	struct usbip_usb_device udev;
	// struct usbip_usb_interface uinf[];
} __attribute__((packed));

struct usbip_op {
	struct usbip_op_common common;

	union {
		struct usbip_op_import_request	import_request;
		struct usbip_op_import_reply	import_reply;
	} u;
};

#define USBIP_OP_IMPORT_REPLY_SIZE \
	(sizeof(struct usbip_op_common) + sizeof(struct usbip_op_import_reply))

// drivers/usb/usbip/usbip_common.h

struct usbip_header_basic {
	__u32 command;
	__u32 seqnum;
	__u32 devid;
	__u32 direction;
	__u32 ep;
} __attribute__((packed));

struct usbip_header_cmd_submit {
	__u32 transfer_flags;
	__s32 transfer_buffer_length;
	__s32 start_frame;
	__s32 number_of_packets;
	__s32 interval;
	unsigned char setup[8];
} __attribute__((packed));

struct usbip_header_ret_submit {
	__s32 status;
	__s32 actual_length;
	__s32 start_frame;
	__s32 number_of_packets;
	__s32 error_count;
} __attribute__((packed));

struct usbip_header_cmd_unlink {
	__u32 seqnum;
} __attribute__((packed));

struct usbip_header_ret_unlink {
	__s32 status;
} __attribute__((packed));

struct usbip_header {
	struct usbip_header_basic base;

	union {
		struct usbip_header_cmd_submit	cmd_submit;
		struct usbip_header_ret_submit	ret_submit;
		struct usbip_header_cmd_unlink	cmd_unlink;
		struct usbip_header_ret_unlink	ret_unlink;
	} u;
} __attribute__((packed));

/*----------------------------------------------------------------------*/

void unpack_usbip_op_common(struct usbip_op_common *s) {
	s->version = ntohs(s->version);
	s->code = ntohs(s->code);
	s->status = ntohl(s->status);
}

void pack_usbip_op_common(struct usbip_op_common *s) {
	s->version = htons(s->version);
	s->code = htons(s->code);
	s->status = htonl(s->status);
}

void pack_usbip_op_import_reply(struct usbip_op_import_reply *s) {
	s->udev.busnum = htonl(s->udev.busnum);
	s->udev.devnum = htonl(s->udev.devnum);
	s->udev.speed = htonl(s->udev.speed);
}

void unpack_usbip_header_basic(struct usbip_header_basic *s) {
	s->command = ntohl(s->command);
	s->seqnum = ntohl(s->seqnum);
	s->devid = ntohl(s->devid);
	s->direction = ntohl(s->direction);
	s->ep = ntohl(s->ep);
}

void unpack_usbip_header_cmd_submit(struct usbip_header_cmd_submit *s) {
	s->transfer_flags = ntohl(s->transfer_flags);
	s->transfer_buffer_length = ntohl(s->transfer_buffer_length);
	s->start_frame = ntohl(s->start_frame);
	s->number_of_packets = ntohl(s->number_of_packets);
	s->interval = ntohl(s->interval);
}

void pack_usbip_header_basic(struct usbip_header_basic *s) {
	s->command = htonl(s->command);
	s->seqnum = htonl(s->seqnum);
	s->devid = htonl(s->devid);
	s->direction = htonl(s->direction);
	s->ep = htonl(s->ep);
}

void pack_usbip_header_ret_submit(struct usbip_header_ret_submit *s) {
	s->status = htonl(s->status);
	s->actual_length = htonl(s->actual_length);
	s->start_frame = htonl(s->start_frame);
	s->number_of_packets = htonl(s->number_of_packets);
	s->error_count = htonl(s->error_count);
}

/*----------------------------------------------------------------------*/

void usbip_reply(int fd, __u32 seqnum, void *data, unsigned int size) {
	struct usbip_header uh;
	memset(&uh, 0, sizeof(uh));
	uh.base.command = USBIP_RET_SUBMIT;
	uh.base.seqnum = seqnum;
	uh.u.ret_submit.actual_length = size;
	pack_usbip_header_basic(&uh.base);
	pack_usbip_header_ret_submit(&uh.u.ret_submit);

	if (send(fd, &uh, sizeof(uh), 0) != sizeof(uh)) {
		perror("send()");
		exit(EXIT_FAILURE);
	}
	if (size > 0) {
		if (send(fd, data, size, 0) != size) {
			perror("send()");
			exit(EXIT_FAILURE);
		}
	}
}

void init_import_reply(struct usbip_op* op) {
	memset(op, 0, sizeof(*op));

	op->common.version = 273;
	op->common.code = OP_REP_IMPORT;
	op->common.status = 0;
	pack_usbip_op_common(&op->common);

	struct usbip_op_import_reply *rep = &op->u.import_reply;
	strcpy(rep->udev.path, "/sys/devices/pci0000:00/0000:00:01.2/usb1/1-1");
	strcpy(rep->udev.busid, "1-1");
	rep->udev.busnum = 1;
	rep->udev.devnum = 2;
	rep->udev.speed = USB_SPEED_HIGH;
	rep->udev.idVendor = usb_device.idVendor;
	rep->udev.idProduct = usb_device.idProduct;
	rep->udev.bcdDevice = usb_device.bcdDevice;
	rep->udev.bDeviceClass = usb_device.bDeviceClass;
	rep->udev.bDeviceSubClass = usb_device.bDeviceSubClass;
	rep->udev.bDeviceProtocol = usb_device.bDeviceProtocol;
	rep->udev.bNumConfigurations = usb_device.bNumConfigurations;
	rep->udev.bConfigurationValue = usb_config.bConfigurationValue;
	rep->udev.bNumInterfaces = usb_config.bNumInterfaces;
	pack_usbip_op_import_reply(rep);
}

void handle_control_request(int fd, struct usbip_header *uh) {
	struct usb_ctrlrequest *ctrl =
		(struct usb_ctrlrequest *)&uh->u.cmd_submit.setup[0];

	printf("bRequestType: 0x%x (%s), bRequest: 0x%x, wValue: 0x%x, "
		"wIndex: 0x%x, wLength: %d\n", ctrl->bRequestType,
		(ctrl->bRequestType & USB_DIR_IN) ? "IN" : "OUT",
		ctrl->bRequest, ctrl->wValue, ctrl->wIndex, ctrl->wLength);

	switch (ctrl->bRequestType & USB_TYPE_MASK) {
	case USB_TYPE_STANDARD:
		switch (ctrl->bRequest) {
		case USB_REQ_GET_DESCRIPTOR:
			switch (ctrl->wValue >> 8) {
			case USB_DT_DEVICE:
				usbip_reply(fd, uh->base.seqnum,
					&usb_device, sizeof(usb_device));
				break;
			case USB_DT_DEVICE_QUALIFIER:
				usbip_reply(fd, uh->base.seqnum,
					&usb_qualifier, sizeof(usb_qualifier));
				break;
			case USB_DT_CONFIG: {
				char data[256];
				int len = build_config(&data[0], sizeof(data));
				if (len > ctrl->wLength)
					len = ctrl->wLength;
				usbip_reply(fd, uh->base.seqnum, &data[0], len);
			} break;
			case USB_DT_STRING: {
				char data[4];
				data[0] = 4;
				data[1] = USB_DT_STRING;
				if ((ctrl->wValue & 0xff) == 0) {
					data[2] = 0x09;
					data[3] = 0x04;
				} else {
					data[2] = 'x';
					data[3] = 0x00;
				}
				usbip_reply(fd, uh->base.seqnum,
					&data[0], sizeof(data));
			} break;
			case HID_DT_REPORT:
				usbip_reply(fd, uh->base.seqnum,
					&usb_hid_report[0],
					sizeof(usb_hid_report));
				break;
			default:
				fprintf(stderr, "unknown descriptor\n");
				exit(EXIT_FAILURE);
			}
			break;
		case USB_REQ_SET_CONFIGURATION:
			usbip_reply(fd, uh->base.seqnum, "", 0);
			break;
		default:
			fprintf(stderr, "unknown request type\n");
			exit(EXIT_FAILURE);
		}
		break;
	case USB_TYPE_CLASS:
		switch (ctrl->bRequest) {
		case HID_REQ_SET_REPORT: {
			char data[128];
			int rv = recv(fd, data, ctrl->wLength, 0);
			if (rv != ctrl->wLength) {
				fprintf(stderr, "recv() failed\n");
				exit(EXIT_FAILURE);
			}
			usbip_reply(fd, uh->base.seqnum, "", 0);
		} break;
		case HID_REQ_SET_IDLE:
			usbip_reply(fd, uh->base.seqnum, "", 0);
			break;
		default:
			fprintf(stderr, "unknown request type\n");
			exit(EXIT_FAILURE);
		}
		break;
	default:
		fprintf(stderr, "unknown request type\n");
		exit(EXIT_FAILURE);
	}
};

void handle_data_request(int fd, struct usbip_header *cmd) {
	static int count = 0;
	char data[5][8] = {
	    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	    {0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	    {0x04, 0x00, 0x46, 0x00, 0x00, 0x00, 0x00, 0x00},
	    {0x04, 0x00, 0x46, 0x1b, 0x00, 0x00, 0x00, 0x00},
	    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	};
	if (count < 5)
		usbip_reply(fd, cmd->base.seqnum,
			data[count], sizeof(data[count]));
	else
		exit(0);
	count++;
	usleep(50 * 1000);
};

void handle_usb_request(int fd, struct usbip_header *uh) {
	if (uh->base.ep == 0) {
		printf("control request\n");
		handle_control_request(fd, uh);
	} else {
		printf("data request\n");
		handle_data_request(fd, uh);
	}
};

void main() {
	printf("waiting for connection...\n");

	int server_fd = socket(PF_INET, SOCK_STREAM, 0);
	if (server_fd < 0) {
		perror("socket()");
		exit(EXIT_FAILURE);
	}

	int reuse = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR,
		       (const char*)&reuse, sizeof(reuse)) < 0)
		perror("setsockopt(SO_REUSEADDR)");

	struct sockaddr_in serv;
	memset(&serv, 0, sizeof(serv));
	serv.sin_family = AF_INET;
	serv.sin_addr.s_addr = htonl(INADDR_ANY);
	serv.sin_port = htons(USBIP_PORT);

	if (bind(server_fd, (struct sockaddr*)&serv, sizeof(serv)) < 0) {
		perror("bind()");
		exit(EXIT_FAILURE);
	}

	if (listen(server_fd, SOMAXCONN) < 0) {
		perror("listen()");
		exit(EXIT_FAILURE);
	}

	while (true) {
		struct sockaddr_in client;
		unsigned int addrlen = sizeof(client);
		int fd = accept(server_fd, (struct sockaddr*)&client, &addrlen);
		if (fd < 0) {
			perror("accept()");
			exit(EXIT_FAILURE);
		}
		printf("connection from %s\n", inet_ntoa(client.sin_addr));

		bool attached = false;
		while (!attached) {
			struct usbip_op op, ret;
			int rv = recv(fd, &op.common, sizeof(op.common), 0);
			if (rv != sizeof(op.common)) {
				fprintf(stderr, "recv() failed\n");
				exit(EXIT_FAILURE);
			}
			unpack_usbip_op_common(&op.common);

			switch (op.common.code) {
			case OP_REQ_IMPORT:
				printf("OP_REQ_IMPORT\n");
				rv = recv(fd, &op.u.import_request,
					sizeof(op.u.import_request), 0);
				if (rv != sizeof(op.u.import_request)) {
					fprintf(stderr, "recv() failed\n");
					break;
				}
				init_import_reply(&ret);
				rv = send(fd, &ret,
					USBIP_OP_IMPORT_REPLY_SIZE, 0);
				if (rv != USBIP_OP_IMPORT_REPLY_SIZE) {
					perror("send()");
					break;
				}
				attached = true;
				break;
			default:
				fprintf(stderr, "unsupported op 0x%02hx\n",
					op.common.code);
				exit(EXIT_FAILURE);
			}
		}

		while (true) {
			struct usbip_header uh;
			int rv = recv(fd, &uh, sizeof(uh), 0);
			if (rv != sizeof(uh)) {
				fprintf(stderr, "recv() failed\n");
				break;
			}
			unpack_usbip_header_basic(&uh.base);

			switch (uh.base.command) {
			case USBIP_CMD_SUBMIT:
				printf("USBIP_CMD_SUBMIT\n");
				unpack_usbip_header_cmd_submit(
					&uh.u.cmd_submit);
				handle_usb_request(fd, &uh);
				break;
			default:
				fprintf(stderr, "unsupported command %d",
					uh.base.command);
				exit(EXIT_FAILURE);
			}
		}

		close(fd);
	}
}
