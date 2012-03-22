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
#include <linux/wait.h>
#include <asm/uaccess.h>

static char *blackboard, *bb_rhead, *bb_whead;
static const int BB_SIZE = 1024;// 4096;
DECLARE_WAIT_QUEUE_HEAD (bbq_read);
DECLARE_WAIT_QUEUE_HEAD (bbq_write);

int ipcdevice_open(struct inode*, struct file*);
int ipcdevice_release(struct inode*, struct file*);
static ssize_t ipcdevice_read(struct file*, char __user*, size_t, loff_t*);
static ssize_t ipcdevice_write(struct file*, const char __user*, size_t, loff_t*);

inline unsigned int _min(unsigned int a, unsigned int b){
    return (a<b)?a:b;
}

inline char *circ_buf_offset(char *buf, char *basis, const size_t offset, const size_t size){
    return basis + ((buf - basis + offset) % size);
}

const struct file_operations ipcdevice_fops = {
    .open  = ipcdevice_open,
    .release = ipcdevice_release,
    .read  = ipcdevice_read,
    .write = ipcdevice_write,
};

int ipcdevice_open(struct inode *inode, struct file *file)
{
    return 0;
}

int ipcdevice_release(struct inode *inode, struct file *file)
{
    return 0;
}

static ssize_t ipcdevice_read(struct file *file, char __user *buf,
        size_t count, loff_t *ppos){
    int result;
    size_t len = 0, to_read = 0, head_space = 0, bytes_read = 0, to_bb_end = 0;

    if( bb_rhead != bb_whead && *bb_rhead == 0 ){
        bb_rhead = blackboard + ((bb_rhead - blackboard + 1) % BB_SIZE);
        return 0;
    }

    do{
        if(bb_rhead == bb_whead){
            wake_up_interruptible_sync(&bbq_write);
            result = wait_event_interruptible(bbq_read, (bb_rhead != bb_whead) );
            if( result != 0 )
                return result;
        }

        //we can read all the way to bb_whead, circularly
        head_space = (BB_SIZE + (bb_whead - bb_rhead)) % BB_SIZE;
        to_bb_end = BB_SIZE-(bb_rhead-blackboard);
        len = strnlen(bb_rhead, _min(head_space, to_bb_end));
        to_read = _min(head_space, _min(to_bb_end, _min(len, count)));
        copy_to_user(buf+bytes_read, bb_rhead, to_read);
        bb_rhead = circ_buf_offset(bb_rhead, blackboard, to_read, BB_SIZE);
        bytes_read += to_read;
        count -= to_read;
    }while( count > 0 && (*bb_rhead != 0 || bb_rhead == bb_whead) );

    if( count > 0 ){
        *(buf+bytes_read) = 0;
        bytes_read++;
    }
    wake_up_interruptible_sync(&bbq_write);

    *ppos = (bb_rhead-blackboard);
    return bytes_read;
}

static ssize_t ipcdevice_write(struct file *file, const char __user *buf,
        size_t count, loff_t *ppos){
    size_t null_term_offset = buf[count-1] == 0 ? 0 : 1;
    size_t to_write = 0, head_space = 0, written = 0;
    int result = 0;

    while( count > 0 ){
        if(bb_rhead == circ_buf_offset(bb_whead, blackboard, 1, BB_SIZE)){
            wake_up_interruptible_sync(&bbq_read);
            result = wait_event_interruptible(bbq_write, (bb_rhead != circ_buf_offset(bb_whead, blackboard, 1, BB_SIZE)) );
            if( result != 0 )
                return result;
        }

        //we can write all the way up to bb_rhead, circularly
        head_space = (BB_SIZE + (bb_rhead - bb_whead) - 1) % BB_SIZE;
        to_write = _min(head_space, _min(BB_SIZE-(bb_whead-blackboard), count));
        copy_from_user(bb_whead, buf+written, to_write);
        bb_whead = circ_buf_offset(bb_whead, blackboard, to_write, BB_SIZE);
        written += to_write;
        count -= to_write;
    }
    if( null_term_offset > 0){
        if(bb_rhead == circ_buf_offset(bb_whead, blackboard, 1, BB_SIZE)){
            wake_up_interruptible_sync(&bbq_read);
            result = wait_event_interruptible(bbq_write, (bb_rhead != circ_buf_offset(bb_whead, blackboard, 1, BB_SIZE)) );
            if( result != 0 )
                return result;
        }

        //Counting this towards written would throw off the writer, so don't.
        *bb_whead = 0;
        bb_whead = circ_buf_offset(bb_whead, blackboard, null_term_offset, BB_SIZE);
    }
    wake_up_interruptible_sync(&bbq_read);

    *ppos = (bb_whead-blackboard);
    return written;
}

int __init ipcdevice_init(void){
    int result;
    printk( KERN_INFO "ipcdevice: installing module\n");
    result = register_chrdev(IPC_MAJOR, IPC_NAME, &ipcdevice_fops);
    if( result < 0 ){
        printk( KERN_ERR "ipcdevice: error registering major number %d\n",
            IPC_MAJOR );
        return result;
    }

    bb_rhead = bb_whead = blackboard = kmalloc(BB_SIZE, GFP_KERNEL);
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
