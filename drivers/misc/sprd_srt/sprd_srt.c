#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <asm/io.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/processor.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/file.h>
#include <linux/kernel.h>
#include <linux/kmod.h>
#include <linux/ioctl.h>


/**********************************************************************************/
#define SRT_IOCTL_MAGIC 'e'
#define SRT_OPEN    			_IO(SRT_IOCTL_MAGIC, 1)
#define SRT_RELEASE     		_IO(SRT_IOCTL_MAGIC, 2)
#define SRT_ENABLE      		_IO(SRT_IOCTL_MAGIC, 3)
#define SRT_READ1				_IOR(SRT_IOCTL_MAGIC, 4, struct srtinfo_struct)
#define SRT_READ2				_IOR(SRT_IOCTL_MAGIC, 5, struct srtinfo_struct)
#define SRT_WRITE1				_IOW(SRT_IOCTL_MAGIC, 6, struct srtinfo_struct)
#define SRT_WRITE2				_IOW(SRT_IOCTL_MAGIC, 7, struct srtinfo_struct)

#include "sprd_srt.h"

/**********************************************************************************/
static struct srt_dev *g_srt_dev=0;
static int srt_open(struct inode *inode, struct file *filp)
{
	return 0;
}
static int srt_release(struct inode *inode, struct file *filp)
{
	return 0;
}
static ssize_t srt_read(struct file *filp, char __user *buf, size_t size, loff_t *ppos)
{
	return 0;
}
static ssize_t srt_write(struct file *filp, const char __user *buf, size_t size, loff_t *ppos)
{
	return 0;
}
static loff_t srt_llseek(struct file *filp, loff_t offset, int orig)
{
	return 0;
}


static long srt_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int i,ret=-1;
	switch (cmd)
	{
	case SRT_READ1:
		break;
	case SRT_READ2:
		break;
	case SRT_WRITE1:
		break;
	case SRT_WRITE2:
		break;
	default:
		return  - EINVAL;
	}

	return 0;
}

static const struct file_operations srt_fops = {
	.owner = THIS_MODULE,
	.open = srt_open,
	.release = srt_release,
	.read = srt_read,
	.write = srt_write,
	.llseek = srt_llseek,
	.unlocked_ioctl = srt_ioctl,
};
/**********************************************************************************/
int srt_init(void)
{
	int err;
	dev_t devno;

	DEBUG_PRINT(LOG_TAG "%s Start\n", __func__);

	g_srt_dev = kmalloc(sizeof(struct srt_dev), GFP_KERNEL);
	if (!g_srt_dev)
	{
		DEBUG_PRINT(LOG_TAG "mem alloc fail");
		return -1;
	}
	memset(g_srt_dev, 0, sizeof(struct srt_dev));
	err = alloc_chrdev_region(&devno, 0, 1, "srt");
	g_srt_dev->dev_major = MAJOR(devno);
	if (err < 0)
	{
		DEBUG_PRINT(LOG_TAG "alloc error=%d", err);
		return -1;
	}
	cdev_init(&g_srt_dev->cdev, &srt_fops);
	g_srt_dev->cdev.owner = THIS_MODULE;
	err = cdev_add(&g_srt_dev->cdev, devno, 1);
	if (err)
	{
		DEBUG_PRINT(LOG_TAG "cdev add=%d, error=%d", devno, err);
		return -1;
	}
	g_srt_dev->pClass = class_create(THIS_MODULE, "srt_class");
	if (IS_ERR(g_srt_dev->pClass))
	{
		DEBUG_PRINT(LOG_TAG "create class error\n");
		return -1;
	}
	g_srt_dev->pClassDev = device_create(g_srt_dev->pClass, NULL, devno, NULL, "srt_dev");
	if (IS_ERR(g_srt_dev->pClassDev))
	{
		DEBUG_PRINT(LOG_TAG "create class dev error\n");
		return -1;
  }
	DEBUG_PRINT(LOG_TAG "%s End\n", __func__);
	return 0;
}

void srt_exit(void)
{
	DEBUG_PRINT(LOG_TAG "%s\n", __func__);

	cdev_del(&g_srt_dev->cdev);
  device_destroy(g_srt_dev->pClass,  MKDEV(g_srt_dev->dev_major, 0));
  class_destroy(g_srt_dev->pClass);
	unregister_chrdev_region(MKDEV(g_srt_dev->dev_major, 0), 1);
	kfree(g_srt_dev);
	g_srt_dev=0;
}

module_init(srt_init);
module_exit(srt_exit);

MODULE_AUTHOR("jie.yang <jie.yang@spreadtrum.com>");
MODULE_DESCRIPTION("SRT");
MODULE_LICENSE("GPL");
