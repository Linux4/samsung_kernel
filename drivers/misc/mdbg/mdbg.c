/************************************************************************************************************	*/
/*****Copyright:	2014 Spreatrum. All Right Reserved									*/
/*****File: 		mdbg.c														*/
/*****Description: 	Marlin Debug System main file. Module,device & driver related defination.	*/
/*****Developer:	fan.kou@spreadtrum.com											*/
/*****Date:		06/01/2014													*/
/************************************************************************************************************	*/

#include "mdbg.h"
#include "mdbg_sdio.h"
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>

#define MDBG_MAX_BUFFER_LEN (PAGE_SIZE)

#define MDBG_WCN_WRITE			(5)
#define MDBG_CHANNEL_ASSERT			(13)
#define MDBG_CHANNEL_WDTIRQ			(4)
#define MDBG_CHANNEL_LOOPCHECK 		(15)
#define MDBG_CHANNEL_AT_CMD			(11)

#define MDBG_WRITE_SIZE 			(64)
#define MDBG_ASSERT_SIZE			(1024)
#define MDBG_WDTIRQ_SIZE			(128)
#define MDBG_LOOPCHECK_SIZE			(128)
#define MDBG_AT_CMD_SIZE 			(128)

unsigned int mdbg_read_count = 0;
unsigned int first_boot = 0;

struct mdbg_devvice_t *mdbg_dev=NULL;
wait_queue_head_t	mdbg_wait;

struct mdbg_proc_entry {
	char *name;
	struct proc_dir_entry *entry;
	struct completion completed;
	wait_queue_head_t	rxwait;
	unsigned int   	rcv_len;
	void *buf;
};

struct mdbg_proc_t{
	char *dir_name;
	struct proc_dir_entry		*procdir;
	struct mdbg_proc_entry		assert;
	struct mdbg_proc_entry		wdtirq;
	struct mdbg_proc_entry		loopcheck;
	struct mdbg_proc_entry		at_cmd;
	char write_buf[MDBG_WRITE_SIZE];
};

static struct mdbg_proc_t *mdbg_proc=NULL;


void mdbg_assert_interface(void)
{
	strncpy(mdbg_proc->assert.buf,"wifi channel is blocked",23);
	printk(KERN_INFO "mdbg_assert_interface:%s\n",mdbg_proc->assert.buf);
	mdbg_proc->assert.rcv_len = 23;
	complete(&mdbg_proc->assert.completed);
	wake_up_interruptible(&mdbg_proc->assert.rxwait);

	return;
}
EXPORT_SYMBOL_GPL(mdbg_assert_interface);

void mdbg_assert_read(void)
{
	int read_len;
	read_len = sdio_dev_get_chn_datalen(MDBG_CHANNEL_ASSERT);
	if(read_len <= 0){
		return;
	}

	if(read_len > MDBG_ASSERT_SIZE)
		MDBG_ERR( "The assert data len:%d, beyond max read:%d",read_len,MDBG_ASSERT_SIZE);

	sdio_dev_read(MDBG_CHANNEL_ASSERT,mdbg_proc->assert.buf,&read_len);
	printk(KERN_INFO "mdbg_assert_read:%s\n",mdbg_proc->assert.buf);
	mdbg_proc->assert.rcv_len = read_len;
	complete(&mdbg_proc->assert.completed);
	wake_up_interruptible(&mdbg_proc->assert.rxwait);

	return;
}
EXPORT_SYMBOL_GPL(mdbg_assert_read);

void mdbg_wdtirq_read(void)
{
	int read_len;
	read_len = sdio_dev_get_chn_datalen(MDBG_CHANNEL_WDTIRQ);
	if(read_len <= 0){
		return;
	}

	if(read_len > MDBG_WDTIRQ_SIZE)
		MDBG_ERR( "The assert data len:%d, beyond max read:%d",read_len,MDBG_WDTIRQ_SIZE);

	sdio_dev_read(MDBG_CHANNEL_WDTIRQ,mdbg_proc->wdtirq.buf,&read_len);
	printk(KERN_INFO "mdbg_wdtirq_read:%s\n",mdbg_proc->wdtirq.buf);
	mdbg_proc->wdtirq.rcv_len = read_len;
	complete(&mdbg_proc->wdtirq.completed);
	wake_up_interruptible(&mdbg_proc->wdtirq.rxwait);

	return;
}
EXPORT_SYMBOL_GPL(mdbg_wdtirq_read);

void mdbg_loopcheck_read(void)
{
	int read_len;

	read_len = sdio_dev_get_chn_datalen(MDBG_CHANNEL_LOOPCHECK);
	if(read_len <= 0){
		return;
	}

	if(read_len > MDBG_LOOPCHECK_SIZE)
		MDBG_ERR( "The loopcheck data len:%d, beyond max read:%d",read_len,MDBG_LOOPCHECK_SIZE);

	sdio_dev_read(MDBG_CHANNEL_LOOPCHECK,mdbg_proc->loopcheck.buf,&read_len);
	printk(KERN_INFO "mdbg_loopcheck_read:%s\n",mdbg_proc->loopcheck.buf);
	mdbg_proc->loopcheck.rcv_len = read_len;
	complete(&mdbg_proc->loopcheck.completed);

	return;
}
EXPORT_SYMBOL_GPL(mdbg_loopcheck_read);

void mdbg_at_cmd_read(void)
{
	int read_len;

	read_len = sdio_dev_get_chn_datalen(MDBG_CHANNEL_AT_CMD);
	if(read_len <= 0){
		MDBG_ERR("MDBG_CHANNEL_AT_CMD len err\n");
		return;
	}

	if(read_len > MDBG_AT_CMD_SIZE)
		MDBG_ERR( "The at cmd data len:%d, beyond max read:%d",read_len,MDBG_AT_CMD_SIZE);

	sdio_dev_read(MDBG_CHANNEL_AT_CMD,mdbg_proc->at_cmd.buf,&read_len);
	mdbg_proc->at_cmd.rcv_len = read_len;
	printk(KERN_INFO "mdbg_at_cmd_read:%s\n",mdbg_proc->at_cmd.buf);
	complete(&mdbg_proc->at_cmd.completed);

	return;
}
EXPORT_SYMBOL_GPL(mdbg_at_cmd_read);

static int mdbg_proc_open(struct inode *inode, struct file *filp)
{
	struct mdbg_proc_entry *entry = (struct mdbg_proc_entry *)PDE_DATA(inode);
	char *type = entry->name;

	filp->private_data = entry;
	//MDBG_ERR("%s type:%s\n",__func__,type);

	return 0;
}

static int mdbg_proc_release(struct inode *inode, struct file *filp)
{
	struct mdbg_proc_entry *entry = (struct mdbg_proc_entry *)PDE_DATA(inode);
	char *type = entry->name;

	//MDBG_ERR("%s type:%s\n",__func__,type);

	return 0;
}

static ssize_t mdbg_proc_read(struct file *filp,
		char __user *buf, size_t count, loff_t *ppos)
{
	struct mdbg_proc_entry *entry = (struct mdbg_proc_entry *)filp->private_data;
	char *type = entry->name;
	int timeout = -1;
	int len = 0;
	int ret;

	//printk(KERN_INFO "type:%s\n",type);
	if (filp->f_flags & O_NONBLOCK) {
		timeout = 0;
	}

	if(strcmp(type, "assert") == 0) {
		if (timeout < 0){
			while(1){
				ret = wait_for_completion_timeout(&mdbg_proc->assert.completed,msecs_to_jiffies(1000));
				if (ret != -ERESTARTSYS)
					break;
			}
		}

		if(copy_to_user((void __user *)buf, mdbg_proc->assert.buf, min(count,(size_t)MDBG_ASSERT_SIZE))){
			MDBG_ERR( "Read assert info error\n");
		}
		len = mdbg_proc->assert.rcv_len;
		mdbg_proc->assert.rcv_len = 0;
		memset(mdbg_proc->assert.buf,0,MDBG_ASSERT_SIZE);
	}

	if(strcmp(type, "wdtirq") == 0) {
		if (timeout < 0){
			while(1){
				ret = wait_for_completion_timeout(&mdbg_proc->wdtirq.completed,msecs_to_jiffies(1000));
				if (ret != -ERESTARTSYS)
					break;
			}
		}

		if(copy_to_user((void __user *)buf, mdbg_proc->wdtirq.buf, min(count,(size_t)MDBG_WDTIRQ_SIZE))){
			MDBG_ERR( "Read wdtirq info error\n");
		}

		len = mdbg_proc->wdtirq.rcv_len;
		mdbg_proc->wdtirq.rcv_len = 0;
		memset(mdbg_proc->wdtirq.buf,0,MDBG_WDTIRQ_SIZE);
	}

	if(strcmp(type, "loopcheck") == 0) {
		if (timeout < 0){
			while(1){
				ret = wait_for_completion_timeout(&mdbg_proc->loopcheck.completed,msecs_to_jiffies(1000));
				if (ret != -ERESTARTSYS)
					break;
			}
		}

		if(first_boot < 2){
			if(copy_to_user((void __user *)buf, "loopcheck_ack", 13)){
				MDBG_ERR("Read loopcheck first info error\n");
			}
			first_boot++;
			len = 13;
		}else if(get_sprd_marlin_status()){
			if(copy_to_user((void __user *)buf, "loopcheck_ack", 13)){
				MDBG_ERR("loopcheck gpio info error\n");
			}
			len = 13;
		}else{
			if(copy_to_user((void __user *)buf, mdbg_proc->loopcheck.buf, min(count,(size_t)MDBG_LOOPCHECK_SIZE))){
				MDBG_ERR("Read loopcheck sdio info error\n");
			}
			len = mdbg_proc->loopcheck.rcv_len;
		}
		memset(mdbg_proc->loopcheck.buf,0,MDBG_LOOPCHECK_SIZE);
		mdbg_proc->loopcheck.rcv_len = 0;
	}

	if(strcmp(type, "at_cmd") == 0) {
		if (timeout < 0){
			while(1){
				ret = wait_for_completion_timeout(&mdbg_proc->at_cmd.completed,msecs_to_jiffies(1000));
				if (ret != -ERESTARTSYS)
					break;
			}
		}

		if(copy_to_user((void __user *)buf, mdbg_proc->at_cmd.buf, min(count,(size_t)MDBG_AT_CMD_SIZE))){
			MDBG_ERR("Read at cmd ack info error\n");
		}

		len = mdbg_proc->at_cmd.rcv_len;
		mdbg_proc->at_cmd.rcv_len = 0;
		memset(mdbg_proc->at_cmd.buf,0,MDBG_AT_CMD_SIZE);
	}

	return len;
}

static ssize_t mdbg_proc_write(struct file *filp,
		const char __user *buf, size_t count, loff_t *ppos)
{
	int cnt = 0;
	if(count > MDBG_WRITE_SIZE){
		MDBG_ERR( "mdbg_proc_write count > MDBG_WRITE_SIZE\n");
		return -ENOMEM;
	}
	memset(mdbg_proc->write_buf,0,MDBG_WRITE_SIZE);

	if (copy_from_user(mdbg_proc->write_buf,buf,count )){
		return -EFAULT;
	}

	printk(KERN_INFO "mdbg_proc->write_buf:%s\n",mdbg_proc->write_buf);

	if(strncmp(mdbg_proc->write_buf,"startwcn",8) == 0){
		first_boot = 0;
		return count;
	}

	if(strncmp(mdbg_proc->write_buf,"dumpwcn",7) == 0){
		mdbg_channel_init();
		return count;
	}

	if(strncmp(mdbg_proc->write_buf,"at+loopcheck",12) == 0){
		printk(KERN_INFO "mdbg start wake marlin\n");
		while((set_marlin_wakeup(MDBG_CHANNEL_WRITE,0x3) < 0) && (cnt <= 3))
		{	
			msleep(300);
			cnt++;		
		}
		cnt = 0;
	}
	else{
		mdbg_send(mdbg_proc->write_buf , count, MDBG_WCN_WRITE);
	}

	return count;
}

static unsigned int mdbg_proc_poll(struct file *filp, poll_table *wait)
{
	struct mdbg_proc_entry *entry = (struct mdbg_proc_entry *)filp->private_data;
	char *type = entry->name;
	unsigned int mask = 0;
	//printk(KERN_INFO "%s type:%s\n",__func__,type);

	if(strcmp(type, "assert") == 0)
	{
		poll_wait(filp, &mdbg_proc->assert.rxwait, wait);
		if(mdbg_proc->assert.rcv_len > 0)
			mask |= POLLIN | POLLRDNORM;
	}

	if(strcmp(type, "wdtirq") == 0)
	{
		poll_wait(filp, &mdbg_proc->wdtirq.rxwait, wait);
		if(mdbg_proc->wdtirq.rcv_len > 0)
			mask |= POLLIN | POLLRDNORM;
	}

	return mask;
}

struct file_operations mdbg_proc_fops = {
	.open		= mdbg_proc_open,
	.release	= mdbg_proc_release,
	.read		= mdbg_proc_read,
	.write		= mdbg_proc_write,
	.poll		= mdbg_proc_poll,
};

LOCAL  void mdbg_fs_channel_init(void)
{
	int retval=0;

	mdbg_proc->assert.buf =  kzalloc(MDBG_ASSERT_SIZE, GFP_KERNEL);
	retval = sdiodev_readchn_init(MDBG_CHANNEL_ASSERT, mdbg_assert_read,0);

	mdbg_proc->wdtirq.buf =  kzalloc(MDBG_WDTIRQ_SIZE, GFP_KERNEL);
	retval = sdiodev_readchn_init(MDBG_CHANNEL_WDTIRQ, mdbg_wdtirq_read,0);

	mdbg_proc->loopcheck.buf =  kzalloc(MDBG_LOOPCHECK_SIZE, GFP_KERNEL);
	retval = sdiodev_readchn_init(MDBG_CHANNEL_LOOPCHECK, mdbg_loopcheck_read,0);

	mdbg_proc->at_cmd.buf =  kzalloc(MDBG_AT_CMD_SIZE, GFP_KERNEL);
	retval = sdiodev_readchn_init(MDBG_CHANNEL_AT_CMD, mdbg_at_cmd_read,0);
}

LOCAL  void mdbg_fs_channel_uninit(void)
{
	sdiodev_readchn_uninit(MDBG_CHANNEL_ASSERT);
	kfree(mdbg_proc->assert.buf);
	mdbg_proc->assert.buf= NULL;

	sdiodev_readchn_uninit(MDBG_CHANNEL_WDTIRQ);
	kfree(mdbg_proc->wdtirq.buf);
	mdbg_proc->wdtirq.buf= NULL;

	sdiodev_readchn_uninit(MDBG_CHANNEL_LOOPCHECK);
	kfree(mdbg_proc->loopcheck.buf);
	mdbg_proc->loopcheck.buf= NULL;

	sdiodev_readchn_uninit(MDBG_CHANNEL_AT_CMD);
	kfree(mdbg_proc->at_cmd.buf);
	mdbg_proc->at_cmd.buf= NULL;
}


LOCAL void mdbg_fs_init(void)
{
	mdbg_proc = kzalloc(sizeof(struct mdbg_proc_t), GFP_KERNEL);
	if (!mdbg_proc)
		return;

	mdbg_proc->dir_name = "mdbg";
	mdbg_proc->procdir = proc_mkdir(mdbg_proc->dir_name, NULL);

	mdbg_proc->assert.name = "assert";
	mdbg_proc->assert.entry = proc_create_data(mdbg_proc->assert.name, S_IRUSR |S_IWUSR,
	mdbg_proc->procdir, &mdbg_proc_fops, &(mdbg_proc->assert));

	mdbg_proc->wdtirq.name = "wdtirq";
	mdbg_proc->wdtirq.entry = proc_create_data(mdbg_proc->wdtirq.name, S_IRUSR | S_IWUSR,
	mdbg_proc->procdir, &mdbg_proc_fops, &(mdbg_proc->wdtirq));

	mdbg_proc->loopcheck.name = "loopcheck";
	mdbg_proc->loopcheck.entry = proc_create_data(mdbg_proc->loopcheck.name, S_IRUSR | S_IWUSR,
	mdbg_proc->procdir, &mdbg_proc_fops, &(mdbg_proc->loopcheck));

	mdbg_proc->at_cmd.name = "at_cmd";
	mdbg_proc->at_cmd.entry = proc_create_data(mdbg_proc->at_cmd.name, S_IRUSR | S_IWUSR,
	mdbg_proc->procdir, &mdbg_proc_fops, &(mdbg_proc->at_cmd));

	mdbg_fs_channel_init();

	init_completion(&mdbg_proc->assert.completed);
	init_completion(&mdbg_proc->wdtirq.completed);
	init_completion(&mdbg_proc->loopcheck.completed);
	init_completion(&mdbg_proc->at_cmd.completed);
	init_waitqueue_head(&mdbg_proc->assert.rxwait);
	init_waitqueue_head(&mdbg_proc->wdtirq.rxwait);
	init_waitqueue_head(&mdbg_dev->rxwait);
}

LOCAL void mdbg_fs_init_exit(void)
{
	mdbg_fs_channel_uninit();

	remove_proc_entry(mdbg_proc->assert.name , mdbg_proc->procdir);
	remove_proc_entry(mdbg_proc->wdtirq.name , mdbg_proc->procdir);
	remove_proc_entry(mdbg_proc->loopcheck.name , mdbg_proc->procdir);
	remove_proc_entry(mdbg_proc->at_cmd.name , mdbg_proc->procdir);
	remove_proc_entry(mdbg_proc->dir_name, NULL);

	kfree(mdbg_proc);
	mdbg_proc=NULL;
}


LOCAL ssize_t mdbg_read(struct file *filp,char __user *buf,size_t count,loff_t *pos)
{
	MDBG_SIZE_T read_size;;
	int timeout = -1;
	int rval;

	if (filp->f_flags & O_NONBLOCK) {
		timeout = 0;
	}

	if(count > MDBG_MAX_BUFFER_LEN)
		count = MDBG_MAX_BUFFER_LEN;

	if (timeout < 0){
		/* wait forever */
		rval = wait_event_interruptible(mdbg_wait,mdbg_read_count > 0);
		if (rval < 0){
			MDBG_ERR("mdbg_read wait interrupted!\n");
		}
	}else{
		//MDBG_ERR("mdbg_read no blok\n");
	}

	mutex_lock(&mdbg_dev->mdbg_lock);

	if(mdbg_read_count <= 0){
		mutex_unlock(&mdbg_dev->mdbg_lock);
		//MDBG_ERR("data no ready");
		return 0;
	}

	read_size = mdbg_receive(mdbg_dev->read_buf , MDBG_MAX_BUFFER_LEN);
	printk_ratelimited(KERN_INFO "%s read_size: %d mdbg_read_count:%d\n",__func__,read_size,mdbg_read_count);

	if((read_size > 0) && (read_size <= MDBG_MAX_BUFFER_LEN)){
		MDBG_LOG("Show %d bytes data.",read_size);

		if(copy_to_user((void  __user *)buf,mdbg_dev->read_buf ,read_size)){
			mutex_unlock(&mdbg_dev->mdbg_lock);
			MDBG_ERR("copy from user fail!");
			return -EFAULT;
		}
		mdbg_read_count -= read_size;
		mutex_unlock(&mdbg_dev->mdbg_lock);
		return read_size;
	}else{
		mdbg_read_count = 0;
		mutex_unlock(&mdbg_dev->mdbg_lock);
		MDBG_LOG("Show no data");
		return (0);
	}
}

LOCAL ssize_t mdbg_write(struct file *filp, const char __user *buf,size_t count,loff_t *pos)
{
	MDBG_SIZE_T sent_size = 0;

	if(NULL == buf || 0 == count){
		MDBG_ERR("Param Error!");
		return count;
	}
	mutex_lock(&mdbg_dev->mdbg_lock);

	if (count > MDBG_MAX_BUFFER_LEN ){
		mutex_unlock(&mdbg_dev->mdbg_lock);
		MDBG_ERR("write too long!");
		return -EINVAL;
	}

	memset(mdbg_dev->write_buf,0,MDBG_MAX_BUFFER_LEN);
	if (copy_from_user(mdbg_dev->write_buf ,(void  __user *)buf,count )){
		mutex_unlock(&mdbg_dev->mdbg_lock);
		MDBG_ERR("to user fail!");
		return -EFAULT;
	}

	sent_size = mdbg_send(mdbg_dev->write_buf , count, MDBG_CHANNEL_WRITE);
	mutex_unlock(&mdbg_dev->mdbg_lock);

	MDBG_LOG("sent_size = %d",sent_size);
	return count;
}

static int mdbg_open(struct inode *inode,struct file *filp)
{
	//printk(KERN_INFO "MDBG:%s entry\n",__func__);

	if (mdbg_dev->open_count!=0) {
		MDBG_ERR( "mdbg_open %d \n",mdbg_dev->open_count);
	}

	mdbg_dev->open_count++;

	return 0;
}

static int mdbg_release(struct inode *inode,struct file *filp)
{
	//printk(KERN_INFO "MDBG:%s entry\n",__func__);

	mdbg_dev->open_count--;
	return 0;
}

static unsigned int mdbg_poll(struct file *filp, poll_table *wait)
{
	unsigned int mask = 0;

	poll_wait(filp, &mdbg_dev->rxwait, wait);
	if(mdbg_read_count > 0)
		mask |= POLLIN | POLLRDNORM;

	return mask;
}

static struct file_operations mdbg_fops = {
	.owner = THIS_MODULE,
	.read  = mdbg_read,
	.write = mdbg_write,
	.open  = mdbg_open,
	.release = mdbg_release,
	.poll		= mdbg_poll,
};
static struct miscdevice mdbg_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "slog_wcn",
	.fops = &mdbg_fops,
};

LOCAL int __init mdbg_module_init(void)
{
	int err;

	MDBG_FUNC_ENTERY;

	mdbg_dev = kzalloc(sizeof(*mdbg_dev), GFP_KERNEL);
	if (!mdbg_dev)
		return -ENOMEM;

	mdbg_dev->read_buf = kzalloc(MDBG_MAX_BUFFER_LEN, GFP_KERNEL);
	if (mdbg_dev->read_buf == NULL) {
		MDBG_ERR("faile (NO MEM) Error!");
		return -ENOMEM;
	}
	mdbg_dev->write_buf = kzalloc(MDBG_MAX_BUFFER_LEN, GFP_KERNEL);
	if (mdbg_dev->write_buf == NULL) {
		kfree(mdbg_dev->read_buf);
		mdbg_dev->write_buf = NULL;
		MDBG_ERR("faile (NO MEM) Error!");
		return -ENOMEM;
	}

	mdbg_dev->open_count = 0;
	mutex_init(&mdbg_dev->mdbg_lock);
	init_waitqueue_head(&mdbg_wait);
	err = mdbg_sdio_init();
	if(err < 0)
		return -ENOMEM;

	mdbg_fs_init();

	return misc_register(&mdbg_device);
}

LOCAL void __exit mdbg_module_exit(void)
{
	MDBG_FUNC_ENTERY;
	mdbg_sdio_remove();
	mdbg_fs_init_exit();
	mutex_destroy(&mdbg_dev->mdbg_lock);
	kfree(mdbg_dev->read_buf);
	kfree(mdbg_dev->write_buf);
	mdbg_dev->read_buf = NULL;
	mdbg_dev->write_buf = NULL;
	misc_deregister(&mdbg_device);
}

module_init(mdbg_module_init);
module_exit(mdbg_module_exit);

MODULE_AUTHOR("Fan Kou<fan.kou@spreadtrum.com>");
MODULE_DESCRIPTION("MARLIN DEBUG SYSTEM.");
MODULE_LICENSE("GPL");

