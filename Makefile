obj-m := ipcdevice.o
KDIR := /usr/src/linux-headers-$(shell uname -r)
PWD := $(shell pwd)

default: ipcdevice.ko

ipcdevice.ko: ipcdevice.c
	$(MAKE) -C $(KDIR) M=$(PWD) modules

test: ipcdevice.ko test.o
	gcc -o ipcdevice_test test.o
	sudo insmod ipcdevice.ko
	./ipcdevice_test
	sudo rmmod ipcdevice

clean:
	rm *.o *.ko *.mod.c ipcdevice_test modules.order Module.symvers
