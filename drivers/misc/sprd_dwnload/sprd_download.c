#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/string.h>
#include <linux/major.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <asm/uaccess.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/timer.h>
#include <linux/wakelock.h>
#include "../sdiodev/sdio_dev.h"
#include "../mdbg/mdbg_sdio.h"

#define  DLOADER_NAME        "download"

#define DOWNLOAD_CHANNEL_READ 12
#define DOWNLOAD_CHANNEL_WRITE 3
#define DOWNLOAD_CALI_WRITE 0


#define		READ_BUFFER_SIZE	(4096)
#define		WRITE_BUFFER_SIZE	(33*1024)

bool flag_read = 0;
uint32 read_len;

struct dloader_dev{
	int 			open_count;
	atomic_t 		read_excl;
	atomic_t 		write_excl;
	char			*read_buffer;
	char			*write_buffer;
};
static struct dloader_dev *download_dev=NULL;
static struct wake_lock download_wake_lock;
static bool flag_cali = false;

extern void set_sprd_download_fin(int dl_tag);
static inline int sprd_download_lock(atomic_t *excl)
{
	if (atomic_inc_return(excl) == 1) {
		return 0;
	} else {
		atomic_dec(excl);
		return -1;
	}
}

static inline void sprd_download_unlock(atomic_t *excl)
{
	atomic_dec(excl);
}

void sprd_download_sdio_read(void)
{
	read_len = sdio_dev_get_chn_datalen(DOWNLOAD_CHANNEL_READ);	
	if(read_len <= 0){
		return;
	}

	sdio_dev_read(DOWNLOAD_CHANNEL_READ,download_dev->read_buffer,&read_len);
	flag_read = 1;
	return;
}

int sprd_download_sdio_write(char *buffer, uint32 size)
{	
	if(true == flag_cali)
		sdio_dev_write(DOWNLOAD_CALI_WRITE, buffer, size);
	else
		sdio_dev_write(DOWNLOAD_CHANNEL_WRITE, buffer, size);
	printk_ratelimited(KERN_INFO "%s size: %d\n",__func__,size);
	return size;
}

int sprd_download_sdio_init(void)
{
	int retval=0;

	retval = sdiodev_readchn_init(DOWNLOAD_CHANNEL_READ, sprd_download_sdio_read,0);
	if(retval!= 0){
		printk(KERN_ERR "Sdio dev read channel init failed!");
		retval = -1;
	}	
		
	return retval;
}


static int sprd_download_open(struct inode *inode,struct file *filp)
{
	if (download_dev==NULL) {
		return -EIO;
	}
	if (download_dev->open_count!=0) {
		printk(KERN_ERR "dloader_open %d \n",download_dev->open_count);
		return -EBUSY;
	}

	marlin_sdio_init();
	mdbg_channel_init();

	download_dev->open_count++;
	wake_lock(&download_wake_lock);
	if(sprd_download_sdio_init()< 0){
		printk(KERN_ERR "sprd_download_sdio_init failed \n");
		return -EBUSY;
	}

	printk(KERN_INFO "sprd_download_open %d\n", download_dev->open_count);
	return 0;
}
static int sprd_download_read(struct file *filp,char __user *buf,size_t count,loff_t *pos)
{
	if ((download_dev==NULL)||(download_dev->open_count!=1)) {
		printk(KERN_ERR "%s return point 1.\n",__func__);
		return -EIO;
	}

	if (sprd_download_lock(&download_dev->read_excl)){
		printk(KERN_ERR "%s return point 2.\n",__func__);
		return -EBUSY;
	}

	if(count > READ_BUFFER_SIZE)
		count = READ_BUFFER_SIZE;
	
	if(!flag_read){
		//printk(KERN_INFO "%s return point 3.\n",__func__);
		sprd_download_unlock(&download_dev->read_excl);
		return 0;
	}

		if(copy_to_user(buf,download_dev->read_buffer,read_len)){
			sprd_download_unlock(&download_dev->read_excl);
			return -EFAULT;
		}
	flag_read = 0;
	sprd_download_unlock(&download_dev->read_excl);

	return read_len;
}

static int sprd_download_write(struct file *filp, const char __user *buf,size_t count,loff_t *pos)
{
	if ((download_dev==NULL)||(download_dev->open_count!=1))
		return -EIO;

	if (sprd_download_lock(&download_dev->write_excl))
		return -EBUSY;

	if (count > WRITE_BUFFER_SIZE ) {
		sprd_download_unlock(&download_dev->write_excl);
		return -EINVAL;
	}

	if (copy_from_user(download_dev->write_buffer,buf,count )){
		sprd_download_unlock(&download_dev->write_excl);
		return -EFAULT;
	}

	while(1 != get_apsdiohal_status()){
		printk("SDIO dev not ready, wait 50ms and try again!\n");
		msleep(50);
	}

	if(strncmp(download_dev->write_buffer,"start_calibration",17) == 0)
	{
		printk("wait marlin ready start\n");
		/*wait marlin ready*/
		while(1){
			if(1 == get_sdiohal_status()){
				marlin_sdio_sync_uninit();
				flag_cali = true;
				printk("wait marlin ready ok\n");
				break;
			}
			msleep(50);
		}
	}else if(strncmp(download_dev->write_buffer,"end_calibration",15) == 0)
		flag_cali = false;
	else
		sprd_download_sdio_write(download_dev->write_buffer,count);

	sprd_download_unlock(&download_dev->write_excl);

	return count;
}

static int sprd_download_release(struct inode *inode,struct file *filp)
{
	printk(KERN_INFO "%s\n",__func__);
	sdiodev_readchn_uninit(DOWNLOAD_CHANNEL_READ);
	wake_unlock(&download_wake_lock);
	download_dev->open_count--;
	set_sprd_download_fin(1);
	return 0;
}

static struct file_operations sprd_download_fops = {
	.owner = THIS_MODULE,
	.read  = sprd_download_read,
	.write = sprd_download_write,
	.open  = sprd_download_open,
	.release = sprd_download_release,
};
static struct miscdevice sprd_download_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = DLOADER_NAME,
	.fops = &sprd_download_fops,
};
static int __init sprd_download_init(void)
{
	struct dloader_dev *dev;
	int		   err=0;
	
	printk(KERN_INFO "%s\n",__func__);
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;
	
	dev->open_count = 0;

	atomic_set(&dev->read_excl, 0);
	atomic_set(&dev ->write_excl, 0);
	download_dev = dev;
	wake_lock_init(&download_wake_lock,WAKE_LOCK_SUSPEND,"download_wake_lock");
	err = misc_register(&sprd_download_device);

	if(err){
		printk(KERN_INFO "download dev add failed!!!\n");
		kfree(dev);
	}

	download_dev->read_buffer = kzalloc(READ_BUFFER_SIZE, GFP_KERNEL);
	if (download_dev->read_buffer == NULL) {
		printk(KERN_ERR "DLoade_open fail(NO MEM) \n");
		return -ENOMEM;
	}
	download_dev->write_buffer = kzalloc(WRITE_BUFFER_SIZE+4, GFP_KERNEL);
	if (download_dev->write_buffer == NULL) {
		kfree(download_dev->read_buffer);
		download_dev->read_buffer = NULL;
		printk(KERN_ERR "DLoade_open fail(NO MEM) \n");
		return -ENOMEM;
	}
	return err;
}
static void __exit sprd_download_cleanup(void)
{
	kfree(download_dev->read_buffer);
	kfree(download_dev->write_buffer);
	download_dev->read_buffer = NULL;
	download_dev->write_buffer = NULL;
	misc_deregister(&sprd_download_device);
	kfree(download_dev);
	wake_lock_destroy(&download_wake_lock);
	download_dev = NULL;
}
module_init(sprd_download_init);
module_exit(sprd_download_cleanup);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("sprd download img driver");

