/*
 * Copyright (C) 2013 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/sipc.h>
#include <linux/earlysuspend.h>
#include <soc/sprd/arch_misc.h>
#include <linux/sched.h>
#include <linux/kthread.h>


#define MAX_DDR_FREQ 640
#define MIN_DDR_FREQ 192
#define DDR_FREQ_BUF_LEN 4
enum dfs_master_cmd{
	DFS_CMD_NORMAL			= 0x0000,
	DFS_CMD_ENABLE			= 0x0300,
	DFS_CMD_DISABLE			= 0x0305,
	DFS_CMD_INQ_DDR_FREQ	= 0x0500,
	DFS_CMD_INQ_CP_FREQ		= 0x0503,
	DFS_CMD_INQ_DDR_TABLE	= 0x0505,
	DFS_CMD_SET_DDR_FREQ		= 0x0600,
	DFS_CMD_SET_CAL_FREQ		= 0x0603
};

enum dfs_slave_cmd{
	DFS_RET_ADJ_OK			= 0x0000,
	DFS_RET_ADJ_VER_FAIL		= 0x0001,
	DFS_RET_ADJ_BUSY			= 0x0002,
	DFS_RET_ADJ_NOCHANGE		= 0x0003,
	DFS_RET_ADJ_FAIL			= 0x0004,
	DFS_RET_DISABLE			= 0x0005,
	DFS_RET_ON_OFF_SUCCEED	= 0x0300,
	DFS_RET_ON_OFF_FAIL		= 0x0303,
	DFS_RET_INQ_SUCCEED		= 0x0500,
	DFS_RET_INQ_FAIL			= 0x0503,
	DFS_RET_SET_SUCCEED		= 0x0600,
	DFS_RET_SET_FAIL			= 0x0603,
	DFS_RET_INVALID_CMD 		= 0x0F0F
};

enum dfs_status_def{
	DFS_STATUS_DISABLE = 0,
	DFS_STATUS_ENABLE = 1
};

static DEFINE_SPINLOCK(dfs_request_lock);
static unsigned int freq = 0;
static unsigned int dfs_status = DFS_STATUS_DISABLE;
static struct task_struct *dfs_smsg_ch_open;

int dfs_msg_recv(struct smsg *msg, int timeout)	//make a thread later
{
	int err = 0;
	while (true) {
		err = smsg_recv(SIPC_ID_PM_SYS, msg, timeout);
		if (err < 0) {
			printk(KERN_ERR "%s, dfs receive failed\n",__func__);
			return err;
		}
		if (SMSG_CH_PM_CTRL == msg->channel &&
			SMSG_TYPE_DFS_RSP == msg->type) {
			pr_debug("%s recv smsg: channel=%d, type=%d, flag=0x%04x, value=0x%08x\n",
				__func__,msg->channel, msg->type, msg->flag, msg->value);
			return 0;
		}
	}
}

int dfs_msg_send(struct smsg *msg, unsigned int cmd, int timeout)		//make a thread later
{
	int err = 0;
	smsg_set(msg, SMSG_CH_PM_CTRL, SMSG_TYPE_DFS, cmd, freq);
	err = smsg_send(SIPC_ID_PM_SYS, msg, timeout);
	pr_debug("%s dfs send smsg: channel=%d, type=%d, flag=0x%04x, value=0x%08x\n",
				__func__,msg->channel, msg->type, msg->flag, msg->value);
	if (err < 0) {
		printk(KERN_ERR "%s, dfs send freq(%d) failed 1 time\n",__func__,freq);
		err = smsg_send(SIPC_ID_PM_SYS, msg, timeout);		//try again
		if (err < 0) {
			printk(KERN_ERR "%s, dfs send freq(%d) failed 2 time\n",__func__,freq);
			return err;
		}
	}
	return 0;
}


int dfs_request(unsigned int freq)
{
	int err = 0;
	struct smsg msg;

	if (dfs_status == DFS_STATUS_DISABLE){
		printk(KERN_ERR "%s, dfs is disabled\n",__func__);
		return -EINVAL;
	}

	err = dfs_msg_send(&msg, DFS_CMD_NORMAL, msecs_to_jiffies(100));
	if (err < 0) {
		printk(KERN_ERR "%s, dfs send freq(%d) failed\n",__func__,freq);
		return err;
	}
	// Wait for the response
	err = dfs_msg_recv(&msg, msecs_to_jiffies(2000));
	if (err < 0) {
		printk(KERN_ERR "%s, dfs receive failed\n",__func__);
		return err;
	}

	switch(msg.flag){
		case DFS_RET_ADJ_OK:
			// DFS succeeded
			printk("%s, dfs succeeded!current freq:%d\n",__func__,msg.value);
			break;
		case DFS_RET_ADJ_VER_FAIL:
			printk("%s, dfs verify fail!current freq:%d\n",__func__,msg.value);
			break;
		case DFS_RET_ADJ_BUSY:
			printk("%s, dfs busy!current freq:%d\n",__func__,msg.value);
			break;
		case DFS_RET_ADJ_NOCHANGE:
			printk("%s, dfs target no change!current freq:%d\n",__func__,msg.value);
			break;
		case DFS_RET_ADJ_FAIL:
			printk("%s, dfs fail!current freq:%d\n",__func__,msg.value);
			break;
		case DFS_RET_DISABLE:
			printk("%s, dfs is disabled!current freq:%d\n",__func__,msg.value);
			break;
		default:
			printk("%s, dfs invalid cmd:%x!current freq:%d\n",__func__, msg.flag, msg.value);
			break;
	}
		
	return 0;
}

int dfs_enable(void)
{
	int err = 0;
	struct smsg msg;
	freq = MAX_DDR_FREQ;
	err = dfs_msg_send(&msg, DFS_CMD_ENABLE, msecs_to_jiffies(100));
	if (err < 0) {
		printk(KERN_ERR "%s, dfs enable failed\n",__func__);
		return err;
	}

	// Wait for the response
	err = dfs_msg_recv(&msg, -1);
	if (err < 0) {
		printk(KERN_ERR "%s, dfs enable receive failed\n",__func__);
		return err;
	}

	if (msg.flag != DFS_RET_ON_OFF_SUCCEED){
		printk(KERN_ERR "%s, dfs enable verify failed:%x\n",__func__, msg.flag);
		return -EINVAL;
	}

	dfs_status = DFS_STATUS_ENABLE;
	pr_debug("%s, dfs_status: %d\n",__func__,dfs_status);
	return 0;
}

static int dfs_smsg_thread(void *data)
{
	int err=0;

	err = smsg_ch_open(SIPC_ID_PM_SYS, SMSG_CH_PM_CTRL, -1);
	if(err < 0){
		printk(KERN_ERR "%s, open sipc channel failed\n",__func__);
	}

	err = dfs_enable();
	if(err < 0){
		printk(KERN_ERR "%s, dfs enable failed\n",__func__);
	}
	return 0;
}

static void scenario_dfs_early_suspend(struct early_suspend *h)
{
	spin_lock(&dfs_request_lock);
	freq = MIN_DDR_FREQ;
	spin_unlock(&dfs_request_lock);
	dfs_request(freq);
}

static void scenoario_dfs_late_resume(struct early_suspend *h)
{
	spin_lock(&dfs_request_lock);
	freq = MAX_DDR_FREQ;
	spin_unlock(&dfs_request_lock);
	dfs_request(freq);
}

static struct early_suspend scenario_dfs_early_suspend_desc = {
        .level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 100,
        .suspend = scenario_dfs_early_suspend,
};

static struct early_suspend scenoario_dfs_late_resume_desc = {
        .level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 200,
        .resume = scenoario_dfs_late_resume,
};

static int scenario_dfs_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int scenario_dfs_release(struct inode *inode, struct file *filp)
{
	return 0;
}

ssize_t scenario_dfs_write(struct file *file, const char __user *buf,
		size_t count, loff_t *offset)
{
	unsigned char freq_buf[DDR_FREQ_BUF_LEN] = { 0 };
	if(count > DDR_FREQ_BUF_LEN){
		printk(KERN_ERR "%s, invalid freq parameter\n",__func__);
		return -EINVAL;
		}
	copy_from_user(freq_buf,buf,count);
	spin_lock(&dfs_request_lock);
	freq = (freq_buf[0]-'0')*100 + (freq_buf[1]-'0')*10 + (freq_buf[2]-'0');
	spin_unlock(&dfs_request_lock);
	if((freq > MAX_DDR_FREQ) || (freq < MIN_DDR_FREQ)){
		printk(KERN_ERR "%s, invalid freq value\n",__func__);
		return -EINVAL;
		}
	dfs_request(freq);
	return count;
}


static const struct file_operations scenario_dfs_fops = {
	.open           =  scenario_dfs_open,
	.write		=  scenario_dfs_write,
	.release        =  scenario_dfs_release,
	.owner          =  THIS_MODULE,
};

static struct miscdevice scenario_dfs_dev = {
	.minor =    MISC_DYNAMIC_MINOR,
	.name  =    "sprd_scenario_dfs",
	.fops  =    &scenario_dfs_fops
};

static int __init scenario_dfs_init(void)
{
	int err;
#if defined(CONFIG_SCX35L_DFS_AUTO_FIT)
	if (soc_is_scx9630_v0() &&  (sci_get_chip_id() < SPRD_CHIPID_9630MF)){
		printk(KERN_ERR "dfs is not support for id:%x\n", sci_get_chip_id());
		return -EINVAL;
	}
	pr_info("dfs is support for id: %x\n",sci_get_chip_id());
#endif

	dfs_smsg_ch_open = kthread_run(dfs_smsg_thread, NULL,
			"dfs-%d-%d", SIPC_ID_PM_SYS, SMSG_CH_PM_CTRL);
	if (IS_ERR(dfs_smsg_ch_open)) {
		printk(KERN_ERR "Failed to create kthread: dfs-%d-%d\n", SIPC_ID_PM_SYS, SMSG_CH_PM_CTRL);
		return -EINVAL;
	}

	register_early_suspend(&scenario_dfs_early_suspend_desc);
	register_early_suspend(&scenoario_dfs_late_resume_desc);
	return misc_register(&scenario_dfs_dev);
}

late_initcall(scenario_dfs_init);

static void __exit scenario_dfs_exit(void)
{
	unregister_early_suspend(&scenario_dfs_early_suspend_desc);
	unregister_early_suspend(&scenoario_dfs_late_resume_desc);
	misc_deregister(&scenario_dfs_dev);
}
module_exit(scenario_dfs_exit);

MODULE_LICENSE("GPL");

