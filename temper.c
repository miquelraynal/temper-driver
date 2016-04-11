/*  temper.c - Offers sysfs entries to get measured temperature
 *             from USB key "TEMPer2"
 *
 *  Copyright (C) 2016 by Miquel Raynal
 */

#include "linux/init.h"
#include "linux/kernel.h"
#include "linux/module.h"

static int __init temper_init(void)
{
	printk("Hello world\n");
	return 0;
}

static void __exit temper_exit(void)
{
	printk("bye\n");
}

module_init(temper_init);
module_exit(temper_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Miquel Raynal <raynal.miquel@gmail.com>");
MODULE_DESCRIPTION("TEMPer2 USB key driver, offering sysfs entries");
