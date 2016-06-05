# Generic Makefile for compiling kernel modules and userspace test tools
KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)
CC ?= gcc

obj-m += temper.o
obj-m += temper_with_urbs.o
obj-m += temper_cdev.o

all:
	make -C $(KDIR) M=$(PWD) modules
	make test

clean:
	make -C $(KDIR) M=$(PWD) clean

test:
	$(CC) -Wall -g -o temper_get_temp temper_cdev_test.c
