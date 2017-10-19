#ifndef ENERGYMETER_H_
#define ENERGYMETER_H_

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/kref.h>
#include <asm/uaccess.h>
#include <linux/usb.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/kobject.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/platform_device.h>
#include <linux/usb.h>
#include <linux/kobject.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("MEGWARE Computer Vertrieb und Service GmbH");
MODULE_DESCRIPTION("Driver for the MEGWARE SlideSX USB Energy Meter");
MODULE_VERSION("1.0");

/* character device */
#define ENERGYMETER_FIRST_MINOR 0
#define ENERGYMETER_MINOR_COUNT 1

/* USB device identification */
#define	USB_DEVICE_VENDOR_ID	0x2E57
#define	USB_DEVICE_PRODUCT_ID	0x454D

/* USB interface and alt. setting */
#define USB_INTERFACE   0x00
#define USB_ALT_SETTING 0x01

/* USB minor range */
#define USB_MINOR_BASE	0x08

/* datastructure (handle) that holds the driver context */
struct usb_energymeter_handle {
	struct usb_device     *usb_dev;
	struct usb_interface  *usb_iface;

	__u8                  endpnt_bulk_out_addr;
	__u8                  endpnt_bulk_in_addr;

	unsigned char         *bulk_out_buff;
	unsigned char         *bulk_in_buff;
	size_t                bulk_out_buff_size;
	size_t                bulk_in_buff_size;
};

/* define modules init and exit functions */
static int energymeter_init(void);
static void energymeter_exit(void);
module_init(energymeter_init);
module_exit(energymeter_exit);

/* commands for communication via USB with energymeter */
#define _GETCURRENT	0x43
#define _GETVOLTAGE	0x56
#define _GETPOWER	0x50
#define _GETENERGY	0x45

#define _OK		0x4F
#define _ERROR		0x45

#endif /* ENERGYMETER_H_ */
