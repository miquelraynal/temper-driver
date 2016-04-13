/*  temper.c - Offers sysfs entries to get measured temperature
 *             from USB key "TEMPer2"
 *
 *  Copyright (C) 2016 by Miquel Raynal
 */

#include "linux/init.h"
#include "linux/kernel.h"
#include "linux/module.h"
#include "linux/usb.h"
#include "linux/slab.h"
#include "linux/stat.h"

#define TEMPER_VID 0x0c45
#define TEMPER_PID 0x7401

/* Peripheral definition */
struct usb_temper {
	struct usb_device *udev;
	unsigned int inner_temp; /* °C */
	unsigned int outer_temp; /* °C */
};

/* Table of devices that may be used by this driver */
static struct usb_device_id temper_id_table[] = {
	{ USB_DEVICE(TEMPER_VID, TEMPER_PID) },
	{ /* Sentinel */ },
};
MODULE_DEVICE_TABLE(usb, temper_id_table);

/* State file */
static ssize_t show_temperature(struct device *dev, struct device_attribute *attr, 
			   char *buf)
{
	struct usb_interface *intf = to_usb_interface(dev);
	struct usb_temper *temper_dev = usb_get_intfdata(intf);

	printk("TBD: get value from device\n");
	return sprintf(buf, "\
Inner temperature: %d°C\n\
Outer temperature: %d°C\n", temper_dev->inner_temp, temper_dev->outer_temp);
}
static DEVICE_ATTR(temperature, S_IRUGO, show_temperature, NULL);

/* Operations for this driver */
static int temper_probe(struct usb_interface *interface, 
			const struct usb_device_id *id)
{
	struct usb_device *udev = interface_to_usbdev(interface);
	struct usb_temper *temper_dev;

	/* Alloc structure and init it */
	temper_dev = kmalloc(sizeof(struct usb_temper), GFP_KERNEL);
	memset(temper_dev, 0x00, sizeof(struct usb_temper));
	temper_dev->udev = usb_get_dev(udev);
	temper_dev->inner_temp = 0;
	temper_dev->outer_temp = 0;

	/* Save interface data */
	usb_set_intfdata(interface, temper_dev);

	/* Create state file */
	device_create_file(&interface->dev, &dev_attr_temperature);

	printk("TEMPer module now attached\n");
	return 0;
}

static void temper_disconnect(struct usb_interface *interface)
{
	struct usb_temper *dev;

	dev = usb_get_intfdata(interface);

	/* Remove state file */
	device_remove_file(&interface->dev, &dev_attr_temperature);

	/* Free interface data */
	usb_put_dev(dev->udev);

	/* Free device structure */
	kfree(dev);

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
