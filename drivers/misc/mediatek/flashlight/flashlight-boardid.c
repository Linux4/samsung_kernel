//flashlight-boardid.c

#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>//file_operations   register_chrdev_region  alloc_chrdev_regio /*register_chrdev_region  alloc_chrdev_region realize in fs/char_dev.c*/
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/cdev.h>//cdev_init cdev_add
#include <asm/io.h>
//#include <asm/system.h>
#include <linux/version.h>//define kernel version
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 3, 0)
        #include <asm/switch_to.h>
#else
       #include <asm/system.h>
#endif
#include <linux/uaccess.h>
#include <asm/uaccess.h>
#include <linux/slab.h>  //kmalloc
#include <linux/device.h>//class_creat  device_creat,realize in driver/base/core.c
#include  <linux/proc_fs.h>//proc_create

#include "flashlight-boardid.h"

static int flashlightbi_major = FLASHLIGHTBIDEV_MAJOR;
static int flashlightbi_min = FLASHLIGHTBIDEV_MIN;

static char board_id[10];
char* boardid_get(void)
{
    char* s1= "";
    char* s2="not found";
    memset(board_id,0,sizeof(board_id));

    s1 = strstr(saved_command_line, "board_id=");
    if(!s1) {
        printk("board_id not found in cmdline\n");
        return s2;
    }
    s1 += strlen("board_id=");
    strncpy(board_id, s1, 9);
    board_id[10]='\0';
    s1 = board_id;
    printk("board_id found in cmdline : %s\n", board_id);

    return s1;
}

module_param(flashlightbi_major, int, S_IRUGO);

struct flashlightbi_dev *flashlightbi_devp; /* Device structure pointer */

struct cdev cdev;

/* File open function */
int flashlightbi_open(struct inode *inode, struct file *filp)
{
    struct flashlightbi_dev *dev;

    /* Get the secondary device number */
    int num = MINOR(inode->i_rdev);
   printk(KERN_INFO "open minor= %d,%s\n", num,__func__);


    if (num >= FLASHLIGHTBIDEV_NR_DEVS)
            return -ENODEV;
    dev = &flashlightbi_devp[num];

    /* Assign the device description structure pointer to the file private data pointer */
    filp->private_data = dev; // It is convenient to use the pointer in the future

   printk(KERN_INFO "open success minor= %d,%s\n", num,__func__);
    return 0;
}

/* File release function */
int flashlightbi_release(struct inode *inode, struct file *filp)
{
  return 0;
}

/* Read function */
static ssize_t flashlightbi_read(struct file *filp, char __user *buf, size_t size, loff_t *ppos)
{
  unsigned long p =  *ppos;
  unsigned int count = size;
  int ret = 0;
  struct flashlightbi_dev *dev = filp->private_data; /* Get the device structure pointer */
  char *boardid = NULL;
  printk(KERN_INFO "%d,read %d bytes(s) from %lu,dev->data=%s,%s\n", __LINE__,count, p,dev->data,__func__);

   #ifdef MEMTEST
  /* Judge whether the read position is valid */
  if (p >= FLASHLIGHTBIDEV_SIZE) // If the reading range is exceeded, a return of 0 indicates that the data cannot be read
    return 0;
  if (count > FLASHLIGHTBIDEV_SIZE - p)
    count = FLASHLIGHTBIDEV_SIZE - p;

  /* Read data into user space */
  if (copy_to_user(buf, (void*)(dev->data + p), count))
  {
    ret =  - EFAULT;
  }
  else
  {
    *ppos += count;
    ret = count;

    printk(KERN_INFO "%d,read %d bytes(s) from %lu,%s\n", __LINE__,count, p,__func__);
  }
  #endif

    boardid=boardid_get();
    printk(KERN_INFO "%d,boardid=%s,%s\n", __LINE__,boardid,__func__);
    if (copy_to_user(buf, (void*)boardid, 9))
    {
        ret =  - EFAULT;
        printk(KERN_INFO "%d,copy_to_user fail,%s\n", __LINE__,__func__);
    }
    else
    {
        ret = 9;
        printk(KERN_INFO "%d,copy_to_user success,%s\n", __LINE__,__func__);
    }

  return ret;
}

/* Write functions */
static ssize_t flashlightbi_write(struct file *filp, const char __user *buf, size_t size, loff_t *ppos)
{
  unsigned long p =  *ppos;
  unsigned int count = size;
  int ret = 0;
  struct flashlightbi_dev *dev = filp->private_data; /* Get the device structure pointer */
  printk(KERN_INFO "%d,written %d bytes(s) from %lu\n", __LINE__,count, p);


  /* Analyze and obtain the effective write length */
  if (p >= FLASHLIGHTBIDEV_SIZE)
    return 0;
  if (count > FLASHLIGHTBIDEV_SIZE - p)
    count = FLASHLIGHTBIDEV_SIZE - p;

  /* Writes data from user space */
  if (copy_from_user(dev->data + p, buf, count))
    ret =  - EFAULT;
  else
  {
    *ppos += count;
    ret = count;

    printk(KERN_INFO "%d,written %d bytes(s) from %lu\n",__LINE__, count, p);
  }

  return ret;
}

/*  Seek file locator function  */
static loff_t flashlightbi_llseek(struct file *filp, loff_t offset, int whence)
{
    loff_t newpos;

    switch(whence) {
      case 0: /* SEEK_SET */
        newpos = offset;
        break;

      case 1: /* SEEK_CUR */
        newpos = filp->f_pos + offset;
        break;

      case 2: /* SEEK_END */
        newpos = FLASHLIGHTBIDEV_SIZE -1 + offset;
        break;

      default: /* can't happen */
        return -EINVAL;
    }
    if ((newpos<0) || (newpos>FLASHLIGHTBIDEV_SIZE))
        return -EINVAL;

    filp->f_pos = newpos;
    return newpos;

}

/* File operation structure */
static const struct file_operations flashlightbi_fops =
{
  .owner = THIS_MODULE,
  .llseek = flashlightbi_llseek,
  .read = flashlightbi_read,
  .write = flashlightbi_write,
  .open = flashlightbi_open,
  .release = flashlightbi_release,
};

#ifdef creat_show_store
int flashlightbidev_test1=0;
int flashlightbidev_test2=0;
int flashlightbidev_test3=0;
int flashlightbidev_test4=0;

static ssize_t flashlightbidev1_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    int rc=0;
    printk(KERN_INFO"flashlightbidev_test    is:%d\n", flashlightbidev_test1);
    #ifdef MEMDEBUG
    return sprintf(buf, "%d\n", flashlightbidev_test1);
    #else
    rc=sprintf(buf, "%d\n", flashlightbidev_test1);
    printk(KERN_INFO"rc    is:%d\n", rc);
    rc=snprintf(buf,10,"%d\n", flashlightbidev_test1);
    printk(KERN_INFO"rc    is:%d,buf =%s\n", rc,buf);
    #ifdef MEMDEBUG
    return 0;//if return 0,the screen will not see the result
    #else
    return rc;
    #endif
    #endif
}

static ssize_t flashlightbidev1_store(struct device *dev, struct device_attribute *attr, const char *buf,size_t size)
{
    char *pvalue=NULL;

    printk(KERN_INFO"Enter! buf=%s,size=%lu\n",buf,size);

    if((size > 2) || ((buf[0] != '0') && (buf[0] != '1'))){
        printk(" command!count: %d, buf: %c %c\n",(int)size,buf[0],buf[1]);
        //return -EINVAL;
    }

    if (buf != NULL && size != 0) {
        flashlightbidev_test1= simple_strtol(buf, &pvalue, 0);
        printk("flashduty_test1: %d\n", flashlightbidev_test1);

#ifdef MEMDEBUG
        if (*pvalue ) {
            flashlightbidev_test2 = simple_strtol((pvalue + 1), &pvalue, 0);
            printk("flashduty_test2: %d\n", flashlightbidev_test2);
        }

        if (*pvalue ) {
            flashlightbidev_test3 = simple_strtol((pvalue + 1), &pvalue, 0);
            printk("flashduty_test3: %d\n", flashlightbidev_test3);
        }

        if (*pvalue ) {
            flashlightbidev_test4 = simple_strtol((pvalue + 1), &pvalue, 0);
            printk("flashduty_test4: %d\n", flashlightbidev_test4);
        }
#else
        if (*(pvalue+1) ) {
            flashlightbidev_test2 = simple_strtol((pvalue + 1), &pvalue, 0);
            printk("flashduty_test2: %d\n", flashlightbidev_test2);
        }

        if (*(pvalue+1) ) {
            flashlightbidev_test3 = simple_strtol((pvalue + 1), &pvalue, 0);
            printk("flashduty_test3: %d\n", flashlightbidev_test3);
        }

        if (*(pvalue+1) ) {
            flashlightbidev_test4 = simple_strtol((pvalue + 1), &pvalue, 0);
            printk("flashduty_test4: %d\n", flashlightbidev_test4);
        }
#endif
    }
#ifdef MEMDEBUG
    return 0;//if return not the same as size,the kernel will always write
#else
    return size;
#endif
}

#ifdef MEMDEBUG
static DEVICE_ATTR(flashlightbidev, 0666, flashlightbidev1_show, flashlightbidev1_store);
/*if the mode set 0666,kernel build will error*/
#else
static DEVICE_ATTR(flashlightbidev, 0664 ,flashlightbidev1_show, flashlightbidev1_store);
#endif
//static DEVICE_ATTR_RW(flash4);

static ssize_t flashlight_boardid_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    int rc=0;
    char *boardid = NULL;
    printk(KERN_INFO"flashlightbidev_test    is:%d\n", flashlightbidev_test1);
    #ifdef MEMDEBUG
    return sprintf(buf, "%d\n", flashlightbidev_test1);
    #else
     rc=sprintf(buf, "%d\n", flashlightbidev_test1);
    printk(KERN_INFO"rc    is:%d\n", rc);
    rc=snprintf(buf,10,"%d\n", flashlightbidev_test1);
    printk(KERN_INFO"rc    is:%d,buf =%s\n", rc,buf);
    boardid=boardid_get();
     rc=sprintf(buf, "%s", boardid);
    #ifdef MEMDEBUG
    return 0;//if return 0,the screen will not see the result
    #else
    return rc;
    #endif
    #endif
}

static ssize_t flashlight_boardid_store(struct device *dev, struct device_attribute *attr, const char *buf,size_t size)
{
    char *pvalue=NULL;

    printk(KERN_INFO"Enter! buf=%s,size=%lu\n",buf,size);

    if((size != 2) || ((buf[0] != '0') && (buf[0] != '1'))){
        printk(" command!count: %d, buf: %c\n",(int)size,buf[0]);
        //return -EINVAL;
    }

    if (buf != NULL && size != 0) {
        flashlightbidev_test1= simple_strtol(buf, &pvalue, 0);
        printk("flashduty_test1: %d\n", flashlightbidev_test1);

#ifdef MEMDEBUG
        if (*pvalue ) {
            flashlightbidev_test2 = simple_strtol((pvalue + 1), &pvalue, 0);
            printk("flashduty_test2: %d\n", flashlightbidev_test2);
        }

        if (*pvalue ) {
            flashlightbidev_test3 = simple_strtol((pvalue + 1), &pvalue, 0);
            printk("flashduty_test3: %d\n", flashlightbidev_test3);
        }

        if (*pvalue ) {
            flashlightbidev_test4 = simple_strtol((pvalue + 1), &pvalue, 0);
            printk("flashduty_test4: %d\n", flashlightbidev_test4);
        }
#else
        if (*(pvalue+1) ) {
            flashlightbidev_test2 = simple_strtol((pvalue + 1), &pvalue, 0);
            printk("flashduty_test2: %d\n", flashlightbidev_test2);
        }

        if (*(pvalue+1) ) {
            flashlightbidev_test3 = simple_strtol((pvalue + 1), &pvalue, 0);
            printk("flashduty_test3: %d\n", flashlightbidev_test3);
        }

        if (*(pvalue+1) ) {
            flashlightbidev_test4 = simple_strtol((pvalue + 1), &pvalue, 0);
            printk("flashduty_test4: %d\n", flashlightbidev_test4);
        }
#endif
    }
#ifdef MEMDEBUG
    return 0;//if return not the same as size,the kernel will always write
#else
    return size;
#endif
}

#ifdef MEMDEBUG
static DEVICE_ATTR(flashlightbidev, 0666, flashlightbidev1_show, flashlightbidev1_store);
/*if the mode set 0666,kernel build will error*/
#else
static DEVICE_ATTR(flashlight_boardid, 0664 ,flashlight_boardid_show, flashlight_boardid_store);
#endif
//static DEVICE_ATTR_RW(flash4);

#endif

#ifdef use_udev
static struct class *flashlight_boardid_cls;
static struct device *flashlight_boardid_device;
 char nodename[20];
#endif

#ifdef proc_sys_creat
struct proc_dir_entry *flashlightbidev_dir;
#endif

/* Device driver module loading function */
static int flashlightbidev_init(void)
{
    int result;
    int i;
    dev_t devno;
    /*Step 1: apply for equipment number*/
    if(flashlightbi_major)
        devno = MKDEV(flashlightbi_major, flashlightbi_min);

    /*Application equipment number; Static direct variable transfer, dynamic variable transfer pointer*/
    /*Static application equipment No*/
    if (flashlightbi_major){
        result = register_chrdev_region(devno, FLASHLIGHTBIDEV_NR_DEVS, "flashlightbidev");//origin 2
        flashlightbi_major = MAJOR(devno);
        flashlightbi_min=MINOR(devno);
    }
    else  /*  Dynamically assign device number  */
    {
        result = alloc_chrdev_region(&devno, 0, FLASHLIGHTBIDEV_NR_DEVS, "flashlightbidev");//origin 2
        flashlightbi_major = MAJOR(devno);
        flashlightbi_min=MINOR(devno);
    }

    if (result < 0)
        return result;

    /*if the above success,we can  use this command find info adb shell cat /proc/devices*/

    /* Step 2: initialize CDEV structure */
    /* Initialize CDEV structure */
    cdev_init(&cdev, &flashlightbi_fops);// Make CDEV and flashlightbi_ Linked to FOPS
    cdev.owner = THIS_MODULE;//The owner member indicates who owns the driver and increases the "kernel reference module count" by 1; THIS_ Module indicates that this module is now used by the kernel. This is a macro defined by the kernel
    cdev.ops = &flashlightbi_fops;

    /* Step 3: register the character device */
    /*  Register character device  */
#ifdef MEMDEBUG
    if(flashlightbi_major)
        cdev_add(&cdev, MKDEV(flashlightbi_major, 10), FLASHLIGHTBIDEV_NR_DEVS);
    else{
        cdev_add(&cdev, MKDEV(flashlightbi_major, 0), FLASHLIGHTBIDEV_NR_DEVS);
    }
#else
    cdev_add(&cdev, MKDEV(flashlightbi_major, flashlightbi_min), FLASHLIGHTBIDEV_NR_DEVS);
#endif

    /*  Allocate memory for the device description structure */
    flashlightbi_devp = kmalloc(FLASHLIGHTBIDEV_NR_DEVS * sizeof(struct flashlightbi_dev), GFP_KERNEL);// So far, we have always used GFP_ KERNEL.
    if (!flashlightbi_devp)    /* Application failed */
    {
        result =  - ENOMEM;
        goto fail_malloc;
    }
    memset(flashlightbi_devp, 0, sizeof(struct flashlightbi_dev));

    /* Allocate memory for the device */
    for (i=0; i < FLASHLIGHTBIDEV_NR_DEVS; i++)
    {
        flashlightbi_devp[i].size = FLASHLIGHTBIDEV_SIZE;
        flashlightbi_devp[i].data = kmalloc(FLASHLIGHTBIDEV_SIZE, GFP_KERNEL);
        memset(flashlightbi_devp[i].data, 0, FLASHLIGHTBIDEV_SIZE);
    }


    /*Step 4: use udev instead of mknod to create a device node*/
#ifdef use_udev
    //add for malloc /dev/xxx replace mknod
    //1.class_create
    flashlight_boardid_cls = class_create(THIS_MODULE, "flashlight_boardid");  //orign is  myclass
    if(IS_ERR(flashlight_boardid_cls))
    {
        unregister_chrdev(flashlightbi_major,"flashlightbidev");
        return -EBUSY;
    }
    //2.device_create
    for(i=0;i<FLASHLIGHTBIDEV_NR_DEVS;i++){
        memset(nodename,0,sizeof(nodename));
        sprintf(nodename,"flashlightbidev%d",i);
#ifdef MEMDEBUG
        test_device = device_create(cls,NULL,devno,NULL,"flashlightbidev1");//mknod /dev/hello
#else
#if 1
        flashlight_boardid_device = device_create(flashlight_boardid_cls,NULL,devno+i,NULL,nodename);//mknod /dev/hello
        /*if the devno is the same ,the device_create second time,kernel will crash*/
#else
        test_device = device_create(cls,NULL,devno,NULL,"flashlightbidev%d",i);//mknod /dev/hello
#endif
#endif
        if(IS_ERR(flashlight_boardid_device))
        {
            class_destroy(flashlight_boardid_cls);
            unregister_chrdev(flashlightbi_major,"flashlightbidev");
            return -EBUSY;
        }
    }
#endif


    /* Step 5: create the sys show / store node as needed */
#ifdef creat_show_store
    {
        result= device_create_file(flashlight_boardid_device, &dev_attr_flashlightbidev);
        if (result) {
            printk("[flashlight_probe]device_create_file flash3 fail!\n");
        }

        result= device_create_file(flashlight_boardid_device, &dev_attr_flashlight_boardid);
        if (result) {
            printk("[flashlight_probe]device_create_file flash3 fail!\n");
        }
    }
#endif


    /* Step 6: create proc paths and nodes as needed */
#ifdef proc_sys_creat
#if 1
    /*If there is a ready-made parent node under proc, you can directly create a parent node;If there is no parent node, the creation fails */
    proc_create("driver/flashlightbidev", 0x0644, NULL, &flashlightbi_fops);
    //proc_create("flashlightbidev1/flashlightbidev", 0x0644, NULL, &flashlightbi_fops);
#else
    flashlightbidev_dir = proc_mkdir("flashlightbidev",NULL);
    proc_create("flashlightbidev", 0x0644, flashlightbidev_dir, &flashlightbi_fops);
#endif
#endif

    return 0;

    fail_malloc:
    unregister_chrdev_region(devno, FLASHLIGHTBIDEV_NR_DEVS);

    return result;
}

/* Module unload function */
static void flashlightbidev_exit(void)
{
    /* remove device file */
    device_remove_file(flashlight_boardid_device, &dev_attr_flashlightbidev);
    /* remove device */
    device_destroy(flashlight_boardid_cls, MKDEV(flashlightbi_major, 0));
    /* remove class */
    class_destroy(flashlight_boardid_cls);

    cdev_del(&cdev);   /* Log off the device */
    kfree(flashlightbi_devp);     /* Free the memory of the device structure */
    unregister_chrdev_region(MKDEV(flashlightbi_major, 0), FLASHLIGHTBIDEV_NR_DEVS); /* Release the device number */
}

MODULE_LICENSE("GPL");
module_init(flashlightbidev_init);
module_exit(flashlightbidev_exit);

