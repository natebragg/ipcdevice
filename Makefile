obj-m := ipcdevice.o
KDIR := /usr/src/linux-headers-$(shell uname -r)
PWD := $(shell pwd)

default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules
