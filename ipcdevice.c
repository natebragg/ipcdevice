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

#include "ipcdevice.h"
#include "base64.h"

struct simplexinfo{
    char *cbuf, *rhead, *whead;
    int message_complete;
    size_t len_remaining;
    const int SIZE;
    wait_queue_head_t rq;
    wait_queue_head_t wq;
} a = {
    .SIZE = 1024,
    .message_complete = 0,
    .len_remaining = 0,
}, b = {
    .SIZE = 1024,
    .message_complete = 0,
    .len_remaining = 0,
};

struct duplexinfo{
    struct simplexinfo *w;
    struct simplexinfo *r;
    long reverse;
    long base64;
    long rot;
} pipea = {
    .w = &a,
    .r = &b,
    .reverse = 0,
    .base64 = 0,
    .rot = 0,
}, pipeb = {
    .w = &b,
    .r = &a,
    .reverse = 0,
    .base64 = 0,
    .rot = 0,
};

unsigned int connections = 0;

int simplexinfo_init(struct simplexinfo*);
void simplexinfo_destroy(struct simplexinfo*);

int ipcdevice_open(struct inode*, struct file*);
int ipcdevice_release(struct inode*, struct file*);
static ssize_t ipcdevice_read(struct file*, char __user*, size_t, loff_t*);
static ssize_t ipcdevice_write(struct file*, const char __user*, size_t, loff_t*);
long ipcdevice_unlocked_ioctl(struct file*, unsigned int, unsigned long);

inline unsigned int _min(unsigned int a, unsigned int b){
    return (a<b)?a:b;
}

inline char *circ_buf_offset(char *buf, char *basis, const size_t offset, const size_t size){
    return basis + ((buf - basis + offset) % size);
}

size_t circ_head_space(char *buf, char *buf2, const size_t size){
    return (size + (buf2 - buf)) % size;
}

size_t pop_length(char **buf, char *basis, const size_t size){
    size_t len = 0;
    int i = 0;
    for(;i<4;i++){
        len += ((*buf)[0]&0xFF)<<(8*i);
        *buf = circ_buf_offset(*buf, basis, 1, size);
    }
    return len;
}

void put_length(char **buf, char *basis, const size_t size, size_t len){
    int i = 0;
    for(;i<4;i++){
        **buf = (char)(len>>(8*i))&0xFF;
        *buf = circ_buf_offset(*buf, basis, 1, size);
    }
}

const struct file_operations ipcdevice_fops = {
    .open  = ipcdevice_open,
    .release = ipcdevice_release,
    .read  = ipcdevice_read,
    .write = ipcdevice_write,
    .unlocked_ioctl = ipcdevice_unlocked_ioctl,
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

    if( this->message_complete ){
        this->message_complete = 0;
        return 0;
    }

    len = this->len_remaining;
    if( len == 0 ){
        if( circ_head_space(this->rhead, this->whead, this->SIZE) < 4){
            wake_up_interruptible_sync(&this->wq);
            result = wait_event_interruptible(this->rq, ( circ_head_space(this->rhead, this->whead, this->SIZE) >= 4) );
            if( result != 0 )
                return result;
        }

        len = pop_length(&this->rhead, this->cbuf, this->SIZE);
    }

    do{
        if(this->rhead == this->whead){
            wake_up_interruptible_sync(&this->wq);
            result = wait_event_interruptible(this->rq, (this->rhead != this->whead) );
            if( result != 0 )
                return result;
        }

        //we can read all the way to whead, circularly
        head_space = circ_head_space(this->rhead, this->whead, this->SIZE);
        to_bb_end = this->SIZE-(this->rhead-this->cbuf);
        to_read = _min(head_space, _min(to_bb_end, _min(len, count)));
        copy_to_user(buf+bytes_read, this->rhead, to_read);
        this->rhead = circ_buf_offset(this->rhead, this->cbuf, to_read, this->SIZE);
        bytes_read += to_read;
        len -= to_read;
        count -= to_read;
    }while( count > 0 && len != 0 );

    if( len == 0 ){
        this->message_complete = 1;
    }

    this->len_remaining = len;

    wake_up_interruptible_sync(&this->wq);

    *ppos = (this->rhead-this->cbuf);
    return bytes_read;
}

static ssize_t ipcdevice_write(struct file *filp, const char __user *buf,
        size_t count, loff_t *ppos){
    size_t chunks_to_write = 0, head_space = 0, written = 0;
    int result = 0;
    struct duplexinfo *di = filp->private_data;
    struct simplexinfo *this = di->w;
    long rot = di->rot;
    long reverse = di->reverse;
    long base64 = di->base64;
    int incr = 1;
    const char __user *buf_curs = buf;
    char *wh_curs;
    char cur_char = 0;
    int in_chunk_size = 1, out_chunk_size = 1;
    int in_chunk_iter;
    size_t output_length = count;
    union base64_translator trans;

    if( this->rhead != this->whead && circ_head_space(this->whead, this->rhead, this->SIZE) < 5){
        wake_up_interruptible_sync(&this->rq);
        result = wait_event_interruptible(this->wq,
            ( circ_head_space(this->rhead, this->whead, this->SIZE) >= 5) );
        if( result != 0 )
            return result;
    }

    if( base64 ){
        in_chunk_size = 3;
        out_chunk_size = 4;
        output_length = (count/in_chunk_size + !!(count%in_chunk_size))*out_chunk_size;
        if( output_length < count ) // output_length will overflow if count > 3GB
            return -EFAULT;
    }

    put_length(&this->whead, this->cbuf, this->SIZE, output_length);

    if( reverse ){
        buf_curs = buf+count-1;
        incr = -1;
    }
    if (!access_ok(VERIFY_READ, buf, count))
        return -EFAULT;
    while( count > 0 ){
        if( this->rhead != this->whead && circ_head_space(this->whead, this->rhead, this->SIZE) <= out_chunk_size){
            wake_up_interruptible_sync(&this->rq);
            result = wait_event_interruptible(this->wq,
                ( this->rhead == this->whead || circ_head_space(this->whead, this->rhead, this->SIZE) > out_chunk_size) );
            if( result != 0 )
                return result;
        }

        //we can write all the way up to rhead, circularly
        head_space = (this->SIZE + (this->rhead - this->whead) - 1) % this->SIZE;
        chunks_to_write = _min( head_space/out_chunk_size, output_length/out_chunk_size );

        for(wh_curs = this->whead; circ_head_space(this->whead, wh_curs, this->SIZE) < chunks_to_write*out_chunk_size; output_length-=out_chunk_size){
            for(in_chunk_iter = in_chunk_size-1; in_chunk_iter >= 0 && count != 0; --in_chunk_iter, buf_curs+=incr, --count, ++written){
                if(__get_user( cur_char, buf_curs))
                    return -EFAULT;
                if( rot ){
                    if( cur_char >= 'A' && cur_char <= 'Z' )
                        cur_char = 'A' + ((cur_char - 'A' + 13)%26);
                    else if( cur_char >= 'a' && cur_char <= 'z' )
                        cur_char = 'a' + ((cur_char - 'a' + 13)%26);
                }
                if( base64 ){
                    trans.input[in_chunk_iter] = cur_char;
                }
            }
            if( base64 ){
                *(wh_curs+0) = base64_table[trans.f1];
                *(wh_curs+1) = base64_table[trans.f2];
                *(wh_curs+2) = base64_table[trans.f3];
                *(wh_curs+3) = base64_table[trans.f4];
                trans.input[0] = trans.input[1] = trans.input[2] = 0;
                for(; in_chunk_iter >= 0; --in_chunk_iter){
                    *(wh_curs+3-in_chunk_iter) = '=';
                }
            } else {
                *wh_curs = cur_char;
            }
            wh_curs = circ_buf_offset(wh_curs, this->cbuf, out_chunk_size, this->SIZE);
        }
        this->whead = wh_curs;
    }

    wake_up_interruptible_sync(&this->rq);

    *ppos = (this->whead-this->cbuf);
    return written;
}

long ipcdevice_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct duplexinfo *di = filp->private_data;

    switch( cmd ){
    case IPC_IOC_ROT13:
        di->rot = !!arg;
        break;

    case IPC_IOC_BASE64:
        di->base64 = !!arg;
        break;

    case IPC_IOC_REVERSE:
        di->reverse = !!arg;
        break;

    default:
        return -ENOTTY;
    }
    return 0;
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
