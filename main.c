//#include <linux/config.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
 
#include <linux/kernel.h>    /* printk() */
#include <linux/slab.h>        /* kmalloc() */
#include <linux/fs.h>        /* everything */
#include <linux/errno.h>    /* error codes */
#include <linux/types.h>    /* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h>    /* O_ACCMODE */
#include <linux/seq_file.h>
#include <linux/cdev.h>
 
//#include <asm/system.h>        /* cli(), *_flags */
#include <asm/uaccess.h>    /* copy_*_user */
 
#include "scull.h"        /* local definitions */

int scull_major =   SCULL_MAJOR; // 主设备号
int scull_minor =   0; // 次设备号
int scull_nr_devs = SCULL_NR_DEVS;    /* number of bare scull devices */
int scull_quantum = SCULL_QUANTUM;  // 量子大小（字节）
int scull_qset =    SCULL_QSET; // 量子集大小
module_param(scull_major, int, S_IRUGO);
module_param(scull_minor, int, S_IRUGO);
module_param(scull_nr_devs, int, S_IRUGO);
module_param(scull_quantum, int, S_IRUGO);
module_param(scull_qset, int, S_IRUGO);//插入参数

 
MODULE_AUTHOR("Alessandro Rubini, Jonathan Corbet");
MODULE_LICENSE("Dual BSD/GPL");

struct scull_dev *scull_devices;


int scull_trim(struct scull_dev *dev)
{
    struct scull_qset *next,*dptr;
    int qset=dev->qset;
    int i;
    for (dptr=dev->data;dptr;dptr=next){
        if(dptr->data){
            for(i=0;i<qset;i++)kfree(dptr->data[i]);
            kfree(dptr->data);
            dptr->data=NULL;
        }
        next=dptr->next;
        kfree(dptr);
        dev->size= 0;
        dev->quantum=scull_quantum;
        dev->qset=scull_qset;
        dev->data=NULL;   
    }
    return 0;
}

/*
open()方法的原型为:int (*open)(struct inode *inode,struct file *filp)
*/
int scull_open(struct inode *inode,struct file *filp){
    struct scull_dev *dev;
    dev=container_of(inode->i_cdev,struct scull_dev,cdev);
    filp->private_data=dev;
    if((filp->f_flags&O_ACCMODE)==O_WRONLY){
        scull_trim(dev);
    }
    return 0;
}

int scull_release(struct inode *inode,struct file *filp){return 0;}
/*
write 和 read的原型是非常接近的, ssize_t write(struct file *filp,char __user *buffer,size_t count,loff_t *oofp);
*/
ssize_t scull_read(struct file *filp,char __user *buffer,size_t count,loff_t *fops){
    struct scull_dev  *dev=filp->private_data;
    struct scull_qset *dptr;
    int quantum=dev->quantum,qset=dev->qset;
    int itemsize=quantum*qset;
    int item ,s_pos,q_pos,rest;
    ssize_t retval =0;
    if(down_interruptible(&dev->sem)){
        return -ERESTARTSYS;
    }
    if(*fops>=dev->size)goto out;
    if(*fops+count>dev->size)count=dev->size-*fops;



out:
    up(&dev->sem);
    return retval;
}

struct file_operations scull_fops = {
    .owner =    THIS_MODULE,
//    .llseek =   scull_llseek,
    .read =     scull_read,
    .write =    scull_write,
//    .ioctl =    scull_ioctl,
    .open =     scull_open,
    .release =  scull_release,
};



static void scull_setup_cdev(struct scull_dev *dev,int index)
{
    int err,devno=MKDEV(scull_major,scull_minor+index);
    cdev_init(&dev->cdev,&scull_fops);//指针的优先级大于提引 
    dev->cdev.owner=THIS_MODULE;
    dev->cdev.ops=&scull_fops;
    err=cdev_add(&dev->cdev,devno,1);
    if(err){
        printk(KERN_WARNING "ERROR %d add scull%d ",err,index);
    }
}


void scull_cleanup_module(void){
     int i;
    dev_t devno = MKDEV(scull_major, scull_minor); // MKDEV 把主次设备号合成为一个dev_t结构
 
    /* Get rid of our char dev entries */
    // 清除字符设备入口
    if (scull_devices) {
        // 遍历释放每个设备的数据区
        for (i = 0; i < scull_nr_devs; i++) {
            scull_trim(scull_devices + i);  // 释放数据区
            cdev_del(&scull_devices[i].cdev); // 移除cdev
        }
        kfree(scull_devices);  // 释放scull_devices本身
    }
 
// 如果使用了/proc来debug，移除创建的/proc文件
#ifdef SCULL_DEBUG /* use proc only if debugging */
    scull_remove_proc();
#endif
 
    /* cleanup_module is never called if registering failed */
    // 解注册scull_nr_devs个设备号，从devno开始
    unregister_chrdev_region(devno,         // dev_t from 
                             scull_nr_devs); // unsigned count
 
    /* and call the cleanup functions for friend devices */
    scull_p_cleanup();
    scull_access_cleanup();

}


int scull_init_module(void){
    int result,i;
    dev_t dev;
    if(scull_major)
    {
        dev=MKDEV(scull_major,scull_minor);
        register_chrdev_region(dev,scull_nr_devs,"scull");
    }else{
        result=alloc_chrdev_region(&dev,scull_minor,scull_nr_devs,"scull");
        scull_major=MAJOR(dev);
    }
    if(result<0){
        printk( KERN_WARNING"scull can not get major  %d \n",scull_major);
        return result;
    }
    scull_devices=kmalloc(scull_nr_devs*sizeof(struct scull_dev),GFP_KERNEL);
    if(!scull_devices){
        result=-ENOMEM;
        goto fail;
    }
    memset(scull_devices,0,scull_nr_devs*sizeof(struct scull_dev));
    for(i=0;i<scull_nr_devs;i++){
        scull_devices[i].quantum = scull_quantum;
        scull_devices[i].qset = scull_qset;
 //       init_MUTEX(&scull_devices[i].sem); // 初始化互斥锁，把信号量sem置为1
        scull_setup_cdev(&scull_devices[i], i);
    }

fail: 
    scull_cleanup_module();
    return result;
}


module_init(scull_init_module);
module_exit(scull_cleanup_module);
