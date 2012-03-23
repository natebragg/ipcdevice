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

struct simplexinfo{
    char *cbuf, *rhead, *whead;
    const int SIZE;
    wait_queue_head_t rq;
    wait_queue_head_t wq;
} a = {
    .SIZE = 1024,
}, b = {
    .SIZE = 1024,
};

struct duplexinfo{
    struct simplexinfo *w;
    struct simplexinfo *r;
} pipea = {
    .w = &a,
    .r = &b,
}, pipeb = {
    .w = &b,
    .r = &a,
};

unsigned int connections = 0;

int simplexinfo_init(struct simplexinfo*);
void simplexinfo_destroy(struct simplexinfo*);

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

int simplexinfo_init(struct simplexinfo *this){
    this->rhead = this->whead = this->cbuf = kmalloc(this->SIZE, GFP_KERNEL);
    if (this->cbuf == NULL){
        return -ENOMEM;
    }
    this->cbuf[0] = 0;
    init_waitqueue_head(&this->rq);
    init_waitqueue_head(&this->wq);
    return 0;
}

void simplexinfo_destroy(struct simplexinfo *this){
    if( this->cbuf != NULL ){
        kfree(this->cbuf);
    }
}

int ipcdevice_open(struct inode *inode, struct file *filp)
{
    switch(connections){
    case 0:
        filp->private_data = &pipea;
        break;

    case 1:
        filp->private_data = &pipeb;
        break;

    default:
        return -EBUSY;
    }
    connections++;
    return 0;
}

int ipcdevice_release(struct inode *inode, struct file *filp)
{
    connections--;
    return 0;
}

static ssize_t ipcdevice_read(struct file *filp, char __user *buf,
        size_t count, loff_t *ppos){
    int result;
    size_t len = 0, to_read = 0, head_space = 0, bytes_read = 0, to_bb_end = 0;
    struct duplexinfo *di = filp->private_data;
    struct simplexinfo *this = di->r;

    if( this->rhead != this->whead && *this->rhead == 0 ){
        this->rhead = this->cbuf + ((this->rhead - this->cbuf + 1) % this->SIZE);
        return 0;
    }

    do{
        if(this->rhead == this->whead){
            wake_up_interruptible_sync(&this->wq);
            result = wait_event_interruptible(this->rq, (this->rhead != this->whead) );
            if( result != 0 )
                return result;
        }

        //we can read all the way to whead, circularly
        head_space = (this->SIZE + (this->whead - this->rhead)) % this->SIZE;
        to_bb_end = this->SIZE-(this->rhead-this->cbuf);
        len = strnlen(this->rhead, _min(head_space, to_bb_end));
        to_read = _min(head_space, _min(to_bb_end, _min(len, count)));
        copy_to_user(buf+bytes_read, this->rhead, to_read);
        this->rhead = circ_buf_offset(this->rhead, this->cbuf, to_read, this->SIZE);
        bytes_read += to_read;
        count -= to_read;
    }while( count > 0 && (*this->rhead != 0 || this->rhead == this->whead) );

    if( count > 0 ){
        *(buf+bytes_read) = 0;
        bytes_read++;
    }
    wake_up_interruptible_sync(&this->wq);

    *ppos = (this->rhead-this->cbuf);
    return bytes_read;
}

static ssize_t ipcdevice_write(struct file *filp, const char __user *buf,
        size_t count, loff_t *ppos){
    size_t null_term_offset = buf[count-1] == 0 ? 0 : 1;
    size_t to_write = 0, head_space = 0, written = 0;
    int result = 0;
    struct duplexinfo *di = filp->private_data;
    struct simplexinfo *this = di->w;

    while( count > 0 ){
        if(this->rhead == circ_buf_offset(this->whead, this->cbuf, 1, this->SIZE)){
            wake_up_interruptible_sync(&this->rq);
            result = wait_event_interruptible(this->wq, (this->rhead != circ_buf_offset(this->whead, this->cbuf, 1, this->SIZE)) );
            if( result != 0 )
                return result;
        }

        //we can write all the way up to rhead, circularly
        head_space = (this->SIZE + (this->rhead - this->whead) - 1) % this->SIZE;
        to_write = _min(head_space, _min(this->SIZE-(this->whead-this->cbuf), count));
        copy_from_user(this->whead, buf+written, to_write);
        this->whead = circ_buf_offset(this->whead, this->cbuf, to_write, this->SIZE);
        written += to_write;
        count -= to_write;
    }
    if( null_term_offset > 0){
        if(this->rhead == circ_buf_offset(this->whead, this->cbuf, 1, this->SIZE)){
            wake_up_interruptible_sync(&this->rq);
            result = wait_event_interruptible(this->wq, (this->rhead != circ_buf_offset(this->whead, this->cbuf, 1, this->SIZE)) );
            if( result != 0 )
                return result;
        }

        //Counting this towards written would throw off the writer, so don't.
        *this->whead = 0;
        this->whead = circ_buf_offset(this->whead, this->cbuf, null_term_offset, this->SIZE);
    }
    wake_up_interruptible_sync(&this->rq);

    *ppos = (this->whead-this->cbuf);
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

    result = simplexinfo_init(&a);
    if (result ){
        unregister_chrdev(IPC_MAJOR, IPC_NAME);
        simplexinfo_destroy(&a);
        return result;
    }
    result = simplexinfo_init(&b);
    if (result ){
        unregister_chrdev(IPC_MAJOR, IPC_NAME);
        simplexinfo_destroy(&a);
        simplexinfo_destroy(&b);
        return result;
    }
    return 0;
}

void __exit ipcdevice_exit(void){
    unregister_chrdev(IPC_MAJOR, IPC_NAME);
    simplexinfo_destroy(&a);
    simplexinfo_destroy(&b);
}

module_init(ipcdevice_init);
module_exit(ipcdevice_exit);
MODULE_LICENSE("GPL");
