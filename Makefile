obj-m := ipcdevice.o
KDIR := /usr/src/linux-headers-$(shell uname -r)
PWD := $(shell pwd)

.PHONY: default clean

default: ipcdevice.ko

ipcdevice.ko: ipcdevice.c
	$(MAKE) -C $(KDIR) M=$(PWD) modules

test: ipcdevice.ko test.o
	gcc -o test test.o
	@lsmod | grep ipcdevice > /dev/null; \
	if [ $$? -eq 0 ]; then \
		sudo rmmod ipcdevice; \
	fi;
	sudo insmod ipcdevice.ko
	./test
	sudo rmmod ipcdevice

demo_p_c: demo_p_c.o
	gcc -o demo_p_c demo_p_c.o

clean:
	rm -f *.o *.ko *.mod.c test demo_p_c modules.order Module.symvers
