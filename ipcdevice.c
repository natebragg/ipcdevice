/*
 * ipcdevice - a simple ipc character device driver.
 *
 * To use:
 *
 * insmod ipcdevice.ko
 * rmmod ipcdevice
 */
#include <linux/module.h>

#include <linux/init.h>
#include <linux/kernel.h>

int __init ipcdevice_init(void){
    printk( KERN_INFO "Installing ipcdevice module\n");
    return 0;
}

void __exit ipcdevice_exit(void){
}

module_init(ipcdevice_init);
module_exit(ipcdevice_exit);
