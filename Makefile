# Generic Makefile for compiling kernel modules
KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

obj-m += temper.o

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	make -C $(KDIR) M=$(PWD) clean
