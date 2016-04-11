/*  temper.c - Offers sysfs entries to get measured temperature
 *             from USB key "TEMPer2"
 *
 *  Copyright (C) 2016 by Miquel Raynal
 */

#include "linux/init.h"
#include "linux/kernel.h"
#include "linux/module.h"
#include "linux/usb.h"

#define TEMPER_VID 0x0c45
#define TEMPER_PID 0x7401

/* Table of devices that may be used by this driver */
static struct usb_device_id temper_id_table[] = {
	{ USB_DEVICE(TEMPER_VID, TEMPER_PID) },
	{ /* Sentinel */ },
};
MODULE_DEVICE_TABLE(usb, temper_id_table);

/* Operations for this driver */
static int temper_probe(struct usb_interface *interface, 
			const struct usb_device_id *id)
{
	printk("TEMPer module now attached\n");
	return 0;
}

static void temper_disconnect(struct usb_interface *interface)
{
	printk("TEMPer module now detached\n");
}
	
/* Main structure */
static struct usb_driver temper_driver = {
	.name = "temper",
	.probe = temper_probe,
	.disconnect = temper_disconnect,
	.id_table = temper_id_table,
};

static int __init temper_init(void)
{
	printk("Hello world\n");
	return usb_register(&temper_driver);
}

static void __exit temper_exit(void)
{
	printk("bye\n");
	usb_deregister(&temper_driver);
}

module_init(temper_init);
module_exit(temper_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Miquel Raynal <raynal.miquel@gmail.com>");
MODULE_DESCRIPTION("TEMPer2 USB key driver, offering sysfs entries");
