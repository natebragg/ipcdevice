/*
 * ipcdevice - a simple ipc character device driver.
 * Copyright (C) 2012  Nate Bragg
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * For first use (as a superuser):
 *
 * mknod /dev/ipcdevice c 42 0
 * chmod 666 /dev/ipcdevice
 *
 * Afterwards, to use:
 *
 * insmod ipcdevice.ko
 * echo some text > /dev/ipcdevice
 * cat /dev/ipcdevice
 * rmmod ipcdevice
 */
#define IPC_MAJOR 42
#define IPC_NAME "ipcdevice"

#include <linux/module.h>

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <asm/uaccess.h>

static char *blackboard;
static const int bb_size = 4096;
DECLARE_WAIT_QUEUE_HEAD (bbq_read);

int ipcdevice_open(struct inode*, struct file*);
int ipcdevice_release(struct inode*, struct file*);
static ssize_t ipcdevice_read(struct file*, char __user*, size_t, loff_t*);
static ssize_t ipcdevice_write(struct file*, const char __user*, size_t, loff_t*);

const struct file_operations ipcdevice_fops = {
    .open  = ipcdevice_open,
    .release = ipcdevice_release,
    .read  = ipcdevice_read,
    .write = ipcdevice_write,
};

int ipcdevice_open(struct inode *inode, struct file *filp)
{
    return 0;
}

int ipcdevice_release(struct inode *inode, struct file *filp)
{
    return 0;
}

static ssize_t ipcdevice_read(struct file *file, char __user *buf,
        size_t count, loff_t *ppos){
    const size_t len = strnlen(blackboard, bb_size) + 1;
    int result;
    int to_end;
    size_t to_read;

    if( blackboard[0] == 0 ){
        //pipe isn't ready
        if (file->f_flags & O_NONBLOCK)
            return -EAGAIN;
        result = wait_event_interruptible(bbq_read, (blackboard[0] != 0) );
        if( result != 0 )
            return result;
    }

    to_end = len-(int)*ppos;
    to_end = to_end > 0 ? to_end : 0;
    to_read = (to_end >= count) ? count : to_end;
    if ( to_read == 0 )
        return 0;
    copy_to_user(buf, blackboard+*ppos, to_read);
    *ppos += to_read;
    return to_read;
}

static ssize_t ipcdevice_write(struct file *file, const char __user *buf,
        size_t count, loff_t *ppos){
    size_t null_term_offset = buf[count-1] == 0 ? 0 : 1;
    if( count+null_term_offset > bb_size ){
        return 0;
    }
    copy_from_user(blackboard,buf,count);
    blackboard[count-1+null_term_offset] = 0;
    wake_up_interruptible_sync(&bbq_read);

    return count;
}

int __init ipcdevice_init(void){
    int result;
    printk( KERN_INFO "Installing ipcdevice module\n");
    result = register_chrdev(IPC_MAJOR, IPC_NAME, &ipcdevice_fops);
    if( result < 0 ){
        printk( KERN_ERR "ipcdevice: error registering major number %d\n",
            IPC_MAJOR );
        return result;
    }

    blackboard = kmalloc(bb_size, GFP_KERNEL);
    if (blackboard == NULL){
        unregister_chrdev(IPC_MAJOR, IPC_NAME);
        return -ENOMEM;
    }
    blackboard[0] = 0;
    return 0;
}

void __exit ipcdevice_exit(void){
    unregister_chrdev(IPC_MAJOR, IPC_NAME);

    if( blackboard != NULL ){
        kfree(blackboard);
    }
}

module_init(ipcdevice_init);
module_exit(ipcdevice_exit);
MODULE_LICENSE("GPL");
