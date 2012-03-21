/*
 * ipcdevice - a simple ipc character device driver.
 *
 * For first use (as a superuser):
 *
 * mknod /dev/ipcdevice c 42 0
 * chmod 666 /dev/ipcdevice
 *
 * Afterwards, to use:
 *
 * insmod ipcdevice.ko
 * rmmod ipcdevice
 */
#define IPC_MAJOR 42
#define IPC_NAME "ipcdevice"

#include <linux/module.h>

#include <linux/init.h>
#include <linux/fs.h>

static ssize_t ipcdevice_read(struct file *file, char __user *buf,
        size_t count, loff_t *ppos){
    return 0;
}

static ssize_t ipcdevice_write(struct file *file, const char __user *buf,
        size_t count, loff_t *ppos){
    int i;
    for( i=0; i<count; i++ ){
        printk( "<1>%c", buf[i] );
    }
    return count;
}

const struct file_operations ipcdevice_fops = {
    .read  = ipcdevice_read,
    .write = ipcdevice_write,
};

int __init ipcdevice_init(void){
    int result;
    printk( KERN_INFO "Installing ipcdevice module\n");
    result = register_chrdev(IPC_MAJOR, IPC_NAME, &ipcdevice_fops);
    if( result < 0 ){
        printk( KERN_ERR "ipcdevice: error registering major number %d\n",
            IPC_MAJOR );
        return result;
    }
    return 0;
}

void __exit ipcdevice_exit(void){
    unregister_chrdev(IPC_MAJOR, IPC_NAME);
}

module_init(ipcdevice_init);
module_exit(ipcdevice_exit);
