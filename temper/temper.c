/*  temper.c - Offers sysfs entries to get measured temperatures
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

#define USE_URB 0

#define TEMPER_VID 0x0c45
#define TEMPER_PID 0x7401

#define TEMPER_CTRL_REQUEST_TYPE 0x21
#define TEMPER_CTRL_REQUEST      0x09
#define TEMPER_CTRL_VALUE        0x0200
#define TEMPER_CTRL_INDEX        0x0001
#define TEMPER_CTRL_BUFFER_SIZE  0x0008
#define TEMPER_INT_BUFFER_SIZE   0x0008

static char temper_buf_get_temp[] = {
	0x01, 0x80, 0x33, 0x01,
	0x00, 0x00, 0x00, 0x00};

/* Peripheral definition */
struct usb_temper {
	struct usb_device *udev;
	struct usb_interface *interface;
	/* Ctrl out EP */
	char *ctrl_out_buffer;
	struct urb *ctrl_out_urb;
	struct usb_ctrlrequest *ctrl_out_cr;
	/* Interrupt in EP */
	char *int_in_buffer;
	struct urb *int_in_urb;
	struct usb_endpoint_descriptor *int_in_endpoint;
	/* Data */
	unsigned int temp_in; /* m°C */
	unsigned int temp_out; /* m°C */
	bool int_completed;
};

/* Table of devices that may be used by this driver */
static struct usb_device_id temper_id_table[] = {
	{ USB_DEVICE(TEMPER_VID, TEMPER_PID) },
	{ /* Sentinel */ },
};
MODULE_DEVICE_TABLE(usb, temper_id_table);

static int get_temp_value (struct usb_temper *temper_dev)
{
	int rc = 0;
#if (USE_URB == 1)
	int i;
#else
	int l;
#endif

	memset(temper_dev->int_in_buffer, 0, TEMPER_INT_BUFFER_SIZE);

#if (USE_URB == 1)
	rc = usb_submit_urb(temper_dev->int_in_urb, GFP_ATOMIC);
	if (rc) {
		printk(KERN_ERR "temper: submit int in urb failed (%d)", rc);
		temper_dev->temp_in = -1;
		temper_dev->temp_out = -1;
		return rc;
	}

	rc = usb_submit_urb(temper_dev->ctrl_out_urb, GFP_ATOMIC);
	if (rc < 0) {
		printk(KERN_ERR "temper: submit ctrl failed (%d)\n", rc);
		temper_dev->temp_in = -1;
		temper_dev->temp_out = -1;
		return rc;
	}

	/* Dirty polling, just for the example */
	rc = -EIO;
	for (i = 0; i < 10; i++) {
		if (temper_dev->int_completed) {
			temper_dev->int_completed = false;
			rc = 0;
			break;
		}
		msleep(20);
	}

#else
	rc = usb_control_msg(temper_dev->udev,
		usb_sndctrlpipe(temper_dev->udev, 0),
		TEMPER_CTRL_REQUEST,
		TEMPER_CTRL_REQUEST_TYPE,
		TEMPER_CTRL_VALUE,
		TEMPER_CTRL_INDEX,
		temper_dev->ctrl_out_buffer,
		TEMPER_CTRL_BUFFER_SIZE,
		HZ * 2);

	if (rc < 0) {
	        printk(KERN_ERR "temper: control message failed (%d)", rc);
		return rc;
        }

	rc = usb_interrupt_msg (temper_dev->udev,
		usb_rcvintpipe(temper_dev->udev, 2),
		temper_dev->int_in_buffer,
		TEMPER_INT_BUFFER_SIZE,
		&l,
		2 * HZ);
	if (rc < 0) {
	        printk(KERN_ERR "temper: interrupt message failed (%d)", rc);
	        temper_dev->temp_in = -1;
	        temper_dev->temp_out = -1;
		return rc;
        }
#endif

	temper_dev->temp_in =
		(temper_dev->int_in_buffer[3] & 0xff) + 
		((temper_dev->int_in_buffer[2] & 0xff) << 8); /* Raw */
	temper_dev->temp_in *= 125 / 32; /* m°C */

	temper_dev->temp_out =
		(temper_dev->int_in_buffer[5] & 0xff) + 
		((temper_dev->int_in_buffer[4] & 0xff) << 8); /* Raw */
	temper_dev->temp_out *= 125 / 32; /* m°C */

	return rc;
}

/* State file */
static ssize_t show_temperatures(struct device *dev, struct device_attribute *attr, 
			   char *buf)
{
	struct usb_interface *intf = to_usb_interface(dev);
	struct usb_temper *temper_dev = usb_get_intfdata(intf);

	get_temp_value(temper_dev);

	return sprintf(buf, "Temperature in:  %3d.%03d°C\nTemperature out: %3d.%03d°C\n",
		       temper_dev->temp_in / 1000, temper_dev->temp_in % 1000,
		       temper_dev->temp_out / 1000, temper_dev->temp_out % 1000);
}
static DEVICE_ATTR(temperatures, S_IRUGO, show_temperatures, NULL);

/* Operations for this driver */
#if USE_URB == 1
static void temper_ctrl_out_callback(struct urb *urb)
{
        printk(KERN_INFO "temper: %s\n", __FUNCTION__); /* NOP */
}

static void temper_int_in_callback(struct urb *urb)
{
	struct usb_temper *temper_dev = urb->context;

        printk(KERN_INFO "temper: %s\n", __FUNCTION__);

	if (urb->status) {
		if (urb->status == -ENOENT ||
		    urb->status == -ECONNRESET ||
		    urb->status == -ESHUTDOWN) {
			printk(KERN_ERR "temper: non-zero urb status (%d) error",
				       urb->status);
			return;
		} else {
			printk(KERN_ERR "temper: non-zero urb status (%d) may be tried again",
				       urb->status);
			return;
		}
	}

	if (urb->actual_length > 0) {
		printk(KERN_DEBUG "temper: read %dB: %02x%02x%02x%02x %02x%02x%02x%02x\n",
		       urb->actual_length,
		       temper_dev->int_in_buffer[0],
		       temper_dev->int_in_buffer[1],
		       temper_dev->int_in_buffer[2],
		       temper_dev->int_in_buffer[3],
		       temper_dev->int_in_buffer[4],
		       temper_dev->int_in_buffer[5],
		       temper_dev->int_in_buffer[6],
		       temper_dev->int_in_buffer[7]);
	}

	temper_dev->int_completed = true;
}
#endif

static int temper_probe(struct usb_interface *interface, 
			const struct usb_device_id *id)
{
	struct usb_device *udev = interface_to_usbdev(interface);
	struct usb_temper *temper_dev;

	struct usb_host_interface *iface_desc;
	struct usb_endpoint_descriptor *endpoint;

	int rc = 0, i;

	/* Alloc structure and init it */
	temper_dev = kmalloc(sizeof(struct usb_temper), GFP_KERNEL);
	memset(temper_dev, 0x00, sizeof(struct usb_temper));
	temper_dev->udev = usb_get_dev(udev);
	temper_dev->interface = interface;

	/* Retrieve endpoint configuration */
	iface_desc = interface->cur_altsetting;
	for (i = 0; i < iface_desc->desc.bNumEndpoints; ++i) {
		endpoint = &iface_desc->endpoint[i].desc;
		
		if (((endpoint->bEndpointAddress & USB_ENDPOINT_DIR_MASK)
		     == USB_DIR_IN) &&
		    ((endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK)
		     == USB_ENDPOINT_XFER_INT))
		        temper_dev->int_in_endpoint = endpoint;

		temper_dev->int_in_endpoint = endpoint;
		printk(KERN_INFO "temper: EP addr 0x%02x\n", endpoint->bEndpointAddress);
	}

	if (!temper_dev->int_in_endpoint) {
		printk(KERN_ERR "temper: could not find interrupt in endpoint");
		rc = -ENODEV;
		goto exit_err;
	}

	/* Alloc in and out buffers */
	temper_dev->ctrl_out_buffer = kzalloc(TEMPER_CTRL_BUFFER_SIZE, GFP_KERNEL);
	if (!temper_dev->ctrl_out_buffer) {
		printk(KERN_ERR "temper: could not allocate ctrl_buffer");
		rc = -ENOMEM;
		goto exit_err;
	}
	memcpy(temper_dev->ctrl_out_buffer, temper_buf_get_temp, TEMPER_CTRL_BUFFER_SIZE);

	temper_dev->int_in_buffer = kmalloc(
		le16_to_cpu(temper_dev->int_in_endpoint->wMaxPacketSize),
		GFP_KERNEL);
	if (!temper_dev->int_in_buffer) {
		printk(KERN_ERR "temper: could not allocate int_in_buffer");
		rc = -ENOMEM;
		goto free_out_buf;
	}

#if USE_URB == 1
	/* Set up the interrupt in URB */
	temper_dev->int_in_urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!temper_dev->int_in_urb) {
		printk(KERN_ERR "temper: could not allocate int_in_urb");
		rc = -ENOMEM;
		goto free_int_buf;
	}

        /* The question is, why endpoint 0x81 is not working with URBs ? */
        temper_dev->int_in_endpoint->bEndpointAddress = 0x82;
	usb_fill_int_urb(temper_dev->int_in_urb,
			 temper_dev->udev,
			 usb_rcvintpipe(
				 temper_dev->udev,
				 temper_dev->int_in_endpoint->bEndpointAddress),
			 temper_dev->int_in_buffer,
			 le16_to_cpu(
			    temper_dev->int_in_endpoint->wMaxPacketSize),
			 temper_int_in_callback,
			 temper_dev,
			 temper_dev->int_in_endpoint->bInterval);

	/* Set up the control out URB */
	temper_dev->ctrl_out_cr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_KERNEL);
	if (!temper_dev->ctrl_out_cr) {
		printk(KERN_ERR "temper: could not allocate usb_ctrlrequest");
		rc = -ENOMEM;
		goto free_int_urb;
	}

	temper_dev->ctrl_out_cr->bRequestType = TEMPER_CTRL_REQUEST_TYPE;
	temper_dev->ctrl_out_cr->bRequest = TEMPER_CTRL_REQUEST;
	temper_dev->ctrl_out_cr->wValue = cpu_to_le16(TEMPER_CTRL_VALUE);
	temper_dev->ctrl_out_cr->wIndex = cpu_to_le16(TEMPER_CTRL_INDEX);
	temper_dev->ctrl_out_cr->wLength = cpu_to_le16(TEMPER_CTRL_BUFFER_SIZE);

	temper_dev->ctrl_out_urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!temper_dev->ctrl_out_urb) {
		printk(KERN_ERR "temper: could not allocate ctrl_out_urb");
		rc = -ENOMEM;
		goto free_out_cr;
	}

	usb_fill_control_urb(temper_dev->ctrl_out_urb,
			     temper_dev->udev,
			     usb_sndctrlpipe(temper_dev->udev, 0),
			     (unsigned char *)temper_dev->ctrl_out_cr,
			     temper_dev->ctrl_out_buffer,
			     TEMPER_CTRL_BUFFER_SIZE,
			     temper_ctrl_out_callback,
			     temper_dev);	
#endif

	/* Data */
	temper_dev->temp_in = 0;
	temper_dev->temp_out = 0;
	temper_dev->int_completed = false;
	get_temp_value(temper_dev);

	/* Save interface data */
	usb_set_intfdata(interface, temper_dev);

	/* Create state file */
	device_create_file(&interface->dev, &dev_attr_temperatures);

	printk(KERN_INFO "TEMPer module now attached and configured\n");

	return 0;

#if USE_URB == 1
free_out_cr:
	kfree(temper_dev->ctrl_out_cr);
free_int_urb:
	kfree(temper_dev->int_in_urb);
free_int_buf:
	kfree(temper_dev->int_in_buffer);
#endif
free_out_buf:
	kfree(temper_dev->ctrl_out_buffer);
exit_err:
	usb_put_dev(temper_dev->udev);
	kfree(temper_dev);
	return rc;
}

static void temper_disconnect(struct usb_interface *interface)
{
	struct usb_temper *temper_dev;

	temper_dev = usb_get_intfdata(interface);

	/* Remove state file */
	device_remove_file(&interface->dev, &dev_attr_temperatures);

	/* Free interface data */
#if USE_URB == 1
	kfree(temper_dev->ctrl_out_cr);
	kfree(temper_dev->int_in_urb);
	kfree(temper_dev->int_in_buffer);
#endif
	kfree(temper_dev->ctrl_out_buffer);
	usb_put_dev(temper_dev->udev);

	/* Free device structure */
	kfree(temper_dev);

	printk(KERN_INFO "TEMPer module now detached\n");
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
