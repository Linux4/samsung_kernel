/***************************************************************
    A globalmem driver as an example of char device drivers

    The initial developer of the original code is Baohua Song
    <author@linuxdriver.cn>. All Rights Reserved.
****************************************************************/
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <asm/io.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include "ipc_info.h"

#define MCOM_FIFO_SIZE    0x1000    /*全局内存最大4K字节*/

/*globalmem设备结构体*/
struct mcom_dev {
        dev_t devno ;
        struct cdev cdev; /*cdev结构体*/
        char buf[MCOM_FIFO_SIZE]; /*全局内存*/
};

struct mcom_dev *mcom_devp; /*设备结构体指针*/

/*文件打开函数*/
int mcom_open(struct inode *inode, struct file *filp)
{
        /*将设备结构体指针赋值给文件私有数据指针*/
        filp->private_data = mcom_devp;
        printk("%s[%d] %s\r\n", __FILE__, __LINE__, __func__);
        return 0;
}

/*文件释放函数*/
int mcom_release(struct inode *inode, struct file *filp)
{
        return 0;
}


const char* ipc_info_str[] = {
        "IDLE",
        "IRQ_IND",
        "CONNECT_REQ",
        "CONNECTED",
        "DISCONNECT_REQ",
        "DISCONNECTED",
        "IPC_STATUS_FLOW_STOP",
        "IPC_STATUS_INVALID_PACKET",
        "CRC_ERROR",
        "PACKET_ERROR",
        "INVAILD"
};



const char* _get_status_string(u32 status)
{
        if(status >= IPC_STATUS_INVAILD) {
                return ipc_info_str[IPC_STATUS_INVAILD];
        }

        return ipc_info_str[status];
}


#define MAX_MCOM_DELAY_SECOND    3
#define MAX_MCOM_READ_DELAY_MS   (MAX_MCOM_DELAY_SECOND*1000)

static u32 s_write_sdio_count = 0;
static u32 s_read_sdio_count = 0;
static u32 s_write_payload_count = 0;
static u32 s_read_payload_count = 0;

static u32 s_prv_read_time = 0;


/*读函数*/
static ssize_t mcom_read(struct file *filp, char __user *buf, size_t size,loff_t *ppos)
{
        int ret = 0;
        int len = 0;
        int speed = 0;
        int payload_speed = 0;
        struct mcom_dev *dev = filp->private_data; /*获得设备结构体指针*/
        char* buf_ptr = &dev->buf[0];
        IPC_INFO_T* ipc_ptr = NULL;

        msleep(MAX_MCOM_READ_DELAY_MS);

        len = sprintf(buf_ptr, "\r\n\t\tphy_speed\tpayload_speed\t\trate\tmux\t\tsdio\t\t saved\tflow_stop\tinvalid\tovfl\tperr\tcerr\tstatus\r\n");

        ret = len;

        ipc_ptr = ipc_info_getinfo(IPC_TX_CHANNEL);

        if(!s_prv_read_time) {
                speed = 0;
                payload_speed = 0;
        } else {
                speed = (ipc_ptr->sdio_count - s_write_sdio_count)/(jiffies_to_msecs(jiffies) - jiffies_to_msecs(s_prv_read_time))*1000;
                payload_speed = (ipc_ptr->payload_count - s_write_payload_count)/(jiffies_to_msecs(jiffies) - jiffies_to_msecs(s_prv_read_time))*1000;
        }

        len = sprintf(&buf_ptr[ret], "MCOM Write:\t%u\t\t%u\t\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%s\r\n",  \
                      speed,payload_speed, ipc_ptr->rate, ipc_ptr->mux_count,ipc_ptr->sdio_count, ipc_ptr->saved_count,
                      ipc_ptr->flow_stop_count,ipc_ptr->invalid_pkt_count, ipc_ptr->overflow_count,  ipc_ptr->packet_error_count, \
                      ipc_ptr->crc_error_count,_get_status_string(ipc_ptr->status));

        ipc_ptr->rate = 0;
        s_write_sdio_count =  ipc_ptr->sdio_count;
        s_write_payload_count = ipc_ptr->payload_count;
        ret += len;

        ipc_ptr = ipc_info_getinfo(IPC_RX_CHANNEL);

        if(!s_prv_read_time) {
                speed = 0;
                payload_speed = 0;
        } else {
                speed = (ipc_ptr->sdio_count - s_read_sdio_count)/(jiffies_to_msecs(jiffies) - jiffies_to_msecs(s_prv_read_time))*1000;
                payload_speed = (ipc_ptr->payload_count - s_read_payload_count)/(jiffies_to_msecs(jiffies) - jiffies_to_msecs(s_prv_read_time))*1000;
        }

        len = sprintf(&buf_ptr[ret], "MCOM Read:\t%u\t\t%u\t\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%s\r\n",
                      speed, payload_speed, ipc_ptr->rate,ipc_ptr->mux_count,ipc_ptr->sdio_count, ipc_ptr->saved_count,
                      ipc_ptr->flow_stop_count, ipc_ptr->invalid_pkt_count, ipc_ptr->overflow_count,  ipc_ptr->packet_error_count,
                      ipc_ptr->crc_error_count, _get_status_string(ipc_ptr->status));

        s_read_sdio_count =  ipc_ptr->sdio_count;
        s_read_payload_count = ipc_ptr->payload_count;
        ipc_ptr->rate = 0;

        ret += len;

        buf_ptr[ret] = 0;

        s_prv_read_time = jiffies;

        len = (ret < size) ? ret : size;

        /*内核空间->用户空间*/
        if (copy_to_user(buf, (void*)buf_ptr, len)) {
                len = - EFAULT;
        }

        return len;
}

/*写函数*/
static ssize_t mcom_write(struct file *filp, const char __user *buf, size_t size, loff_t *ppos)
{

        return - EFAULT;
}



/*文件操作结构体*/
static const struct file_operations mcom_fops = {
        .owner = THIS_MODULE,
        .read = mcom_read,
        .open = mcom_open,
        .release = mcom_release,
};

static char *mcom_devnode(struct device *dev, mode_t *mode)
{
        return kasprintf(GFP_KERNEL, "mcom/%s", dev_name(dev));
}



static struct class *mcom_class;
static int mcom_major = 0;
/*设备驱动模块加载函数*/
int __init mcom_init(void)
{
        int err = 0;
        int result;
        dev_t devno ;

        /* 申请设备号*/
        result = alloc_chrdev_region(&devno, 0, 1, "mcom");
        mcom_major = MAJOR(devno);
        if (result < 0)
                return result;

        /* 动态申请设备结构体的内存*/
        mcom_devp = kmalloc(sizeof(struct mcom_dev), GFP_KERNEL);
        if (!mcom_devp) {  /*申请失败*/
                result = - ENOMEM;
                goto fail_malloc;
        }

        memset(mcom_devp, 0, sizeof(struct mcom_dev));

        /*初始化并注册cdev*/
        cdev_init(&mcom_devp->cdev, &mcom_fops);
        mcom_devp->cdev.owner = THIS_MODULE;
        mcom_devp->cdev.ops = &mcom_fops;
        err = cdev_add(&mcom_devp->cdev, devno, 1);
        if (err)
                printk(KERN_NOTICE "%s:%sError %d\r\n",__FILE__, __func__, err);

        mcom_devp->devno = devno;

        mcom_class = class_create(THIS_MODULE, "mcom");
        if (IS_ERR(mcom_class)) {
                printk(KERN_ERR "Error creating mcom class.\n");
                cdev_del(&mcom_devp->cdev);
                goto fail_malloc;
        }
        mcom_class->devnode = mcom_devnode;
        device_create(mcom_class, NULL, devno, NULL, "mcom");

        return 0;

fail_malloc:
        unregister_chrdev_region(devno, 1);
        return result;
}

/*模块卸载函数*/
void __init mcom_exit(void)
{
        cdev_del(&mcom_devp->cdev);   /*注销cdev*/
        kfree(mcom_devp);     /*释放设备结构体内存*/
        unregister_chrdev_region(&mcom_devp->devno, 1); /*释放设备号*/
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Harald Welte <laforge@openezx.org>");
MODULE_DESCRIPTION("GSM TS 07.10 Multiplexer");

module_init(mcom_init);
module_exit(mcom_exit);

