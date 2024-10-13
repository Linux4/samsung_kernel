/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
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

#ifdef CONFIG_OF
#include <linux/of_device.h>
#endif
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/poll.h>
#include <linux/platform_device.h>
#include <linux/wait.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <asm/uaccess.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>

#include <linux/sipc.h>
#include <linux/sipc_priv.h>
#include <linux/smsgc.h>

struct smsgc_device {
	struct smsgc_init_data	*init;
	int				major;
	int				minor;
	struct cdev		cdev;
};

struct smsgc_priv {
	uint8_t			dst;
	uint8_t			channel;
};

static struct class		*smsgc_class;

static struct smsgc_mgr *smsgcs[SIPC_ID_NR][SMSG_VALID_CH_NR];

static int smsgc_thread(void *data)
{
	struct smsgc_mgr *smsgc = data;
	struct smsg_ipc *ipc;
	struct smsg_channel *ch;
	struct smsg mrecv;
	uint32_t wr;
	int rval;
	struct sched_param param = {.sched_priority = 90};

	ipc = smsg_ipcs[smsgc->dst];
	ch = ipc->channels[smsgc->channel];

	/*set the thread as a real time thread, and its priority is 90*/
	sched_setscheduler(current, SCHED_RR, &param);

	/* since the channel open may hang, we call it in the smsgc thread */
	rval = smsg_ch_open(smsgc->dst, smsgc->channel, -1);
	if (rval != 0) {
		printk(KERN_ERR "Smsgc: Failed to open channel %d\n", smsgc->channel);
		/* assign NULL to thread poniter as failed to open channel */
		smsgc->thread = NULL;
		return rval;
	}
	smsgc->state = SMSGC_STATE_READY;

	/* smsgc init done, handle the ring rx events */
	while (!kthread_should_stop()) {
		/* monitor smsgc rdptr/wrptr update smsg */
		smsg_set(&mrecv, smsgc->channel, 0, 0, 0);
		rval = smsg_recv(smsgc->dst, &mrecv, -1);

		if (rval == -EIO) {
			/* channel state is free */
			msleep(5);
			continue;
		}

		pr_debug("smsgc thread recv msg: dst=%d, channel=%d, "
				"type=%d, flag=0x%04x, value=0x%08x\n",
				smsgc->dst, smsgc->channel,
				mrecv.type, mrecv.flag, mrecv.value);

		switch (mrecv.type) {
			case SMSG_TYPE_OPEN:
				/* handle channel recovery */
				smsg_open_ack(smsgc->dst, smsgc->channel);
				smsgc->state = SMSGC_STATE_READY;
				break;
			case SMSG_TYPE_CLOSE:
				/* handle channel recovery */
				smsg_close_ack(smsgc->dst, smsgc->channel);
				smsgc->state = SMSGC_STATE_IDLE;
				break;
			default:
				if ((int)(readl(smsgc->wrptr) - readl(smsgc->rdptr)) >= SMSG_CACHE_NR) {
					/* smsgc cache is full, drop this msg */
					printk(KERN_ERR "smsgc: channel %d recv cache is full! "
						"drop smsg: type=%d, flag=0x%04x, value=0x%08x\n",
						mrecv.channel, mrecv.type, mrecv.flag, mrecv.value);
				} else {
					/* write to smsgc cache */
					wr = readl(smsgc->wrptr) & (SMSG_CACHE_NR - 1);
					unalign_memcpy(&(smsgc->caches[wr]), &mrecv, sizeof(struct smsg));
					writel(readl(smsgc->wrptr) + 1, smsgc->wrptr);
				}
				if ((int)(readl(ch->wrptr) - readl(ch->rdptr)) == 0 ||
					(int)(readl(smsgc->wrptr) - readl(smsgc->rdptr)) >= SMSG_CACHE_NR) {
					/* msg cache is empty, wake up rxwait*/
					wake_up_interruptible_all(&(smsgc->rxwait));
				}
				break;
		};
	}

	return 0;
}

int smsgc_create(uint8_t dst, uint8_t channel)
{
	struct smsgc_mgr *smsgc;
	int result;
	uint8_t ch_index;

	ch_index = sipc_channel2index(msg->channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("%s:channel %d is not be configed!\n", __func__, msg->channel);
		return -EINVAL;
	}

	smsgc = kzalloc(sizeof(struct smsgc_mgr), GFP_KERNEL);
	if (!smsgc) {
		printk(KERN_ERR "Failed to allocate mgr for smsgc\n");
		return -ENOMEM;
	}

	smsgc->state = SMSGC_STATE_IDLE;
	smsgc->dst = dst;
	smsgc->channel = channel;
	mutex_init(&(smsgc->txlock));
	mutex_init(&(smsgc->rxlock));
	init_waitqueue_head(&(smsgc->rxwait));
	smsgc->txbuf = kzalloc(SMSG_CACHE_NR * sizeof(struct smsg), GFP_KERNEL);
	if (!smsgc->txbuf) {
		kfree(smsgc);
		printk(KERN_ERR "Failed to allocate txbuf for smsgc\n");
		return -ENOMEM;
	}

	smsgc->thread = kthread_create(smsgc_thread, smsgc,
			"smsgc-%d-%d", dst, channel);
	if (IS_ERR(smsgc->thread)) {
		printk(KERN_ERR "Failed to create kthread: smsgc-%d-%d\n", dst, channel);
		result = PTR_ERR(smsgc->thread);
		kfree(smsgc->txbuf);
		kfree(smsgc);

		return result;
	}

	smsgcs[dst][ch_index] = smsgc;
	wake_up_process(smsgc->thread);

	return 0;
}

void smsgc_destroy(uint8_t dst, uint8_t channel)
{
	struct smsgc_mgr *smsgc;
	uint8_t ch_index;

	ch_index = sipc_channel2index(msg->channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("%s:channel %d is not be configed!\n", __func__, msg->channel);
		return -EINVAL;
	}

	smsgc = smsgcs[dst][ch_index];
	if (smsgc == NULL) {
		return;
	}

	smsgc->state = SMSGC_STATE_IDLE;
	smsg_ch_close(dst, channel, -1);

	/* stop smsgc thread if it's created successfully and still alive */
	if (!IS_ERR_OR_NULL(smsgc->thread)) {
		kthread_stop(smsgc->thread);
	}

	kfree(smsgc->txbuf);
	kfree(smsgc);
	smsgcs[dst][ch_index] = NULL;
}

int smsgc_poll_wait(uint8_t dst, uint8_t channel,
		struct file *filp, poll_table *wait)
{
	struct smsgc_mgr *smsgc;
	unsigned int mask = 0;
	uint8_t ch_index;

	ch_index = sipc_channel2index(msg->channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("%s:channel %d is not be configed!\n", __func__, msg->channel);
		return -EINVAL;
	}
	smsgc = smsgcs[dst][ch_index];

	if (!smsgc) {
		return -ENODEV;
	}

	if (smsgc->state != SMSGC_STATE_READY) {
		printk(KERN_ERR "smsgc-%d-%d not ready to poll !\n", dst, channel);
		return -ENODEV;
	}

	poll_wait(filp, &smsgc->rxwait, wait);

	if (readl(smsgc->wrptr) != readl(smsgc->rdptr)) {
		mask |= POLLIN | POLLRDNORM;
	}

	return mask;
}

int smsgc_status(uint8_t dst, uint8_t channel)
{
	struct smsgc_mgr *smsgc;
	uint8_t ch_index;

	ch_index = sipc_channel2index(msg->channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("%s:channel %d is not be configed!\n", __func__, msg->channel);
		return -EINVAL;
	}
	smsgc = smsgcs[dst][ch_index];

	if (!smsgc) {
		return -ENODEV;
	}
	if (smsgc->state != SMSGC_STATE_READY) {
		return -ENODEV;
	}

	return 0;
}

#if defined(CONFIG_DEBUG_FS)

static int smsgc_debug_show(struct seq_file *m, void *private)
{
	struct smsgc_mgr *smsgc = NULL;
	struct list_head *listhd;
	wait_queue_t *pos;
	struct task_struct *task;
	int i, j, cnt;

	for (i = 0; i < SIPC_ID_NR; i++) {
		for (j=0;  j< SMSG_VALID_CH_NR; j++) {
			smsgc = smsgcs[i][j];
			if (!smsgc) {
				continue;
			}
			seq_printf(m, "***************************************************************************************************************************\n");
			seq_printf(m, "smsgc dst %d, channel: %d, state: %d, txmsgs: %d, cache-rdptr: %d, cache-wrptr: %d \n",
				   smsgc->dst, smsgc->channel, smsgc->state, smsgc->txmsg_cnt, readl(smsgc->rdptr), readl(smsgc->wrptr));
			cnt = 0;
			list_for_each(listhd, &smsgc->rxwait.task_list) {
				pos = list_entry(listhd, wait_queue_t, task_list);
				task = pos->private;
				seq_printf(m, "rxwait task #%d: %s, pid = %d, tgid = %d\n", cnt, task->comm, task->pid, task->tgid);
				cnt++;
			}
			seq_printf(m, "total rxwait tasks: %d \n", cnt);
		}
	}
	return 0;
}

static int smsgc_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, smsgc_debug_show, inode->i_private);
}

static const struct file_operations smsgc_debug_fops = {
	.open = smsgc_debug_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

int  smsgc_init_debugfs( void *root )
{
	if (!root)
		return -ENXIO;
	debugfs_create_file("smsgc", S_IRUGO, (struct dentry *)root, NULL, &smsgc_debug_fops);
	return 0;
}

#endif /* CONFIG_DEBUG_FS */

int smsgc_wait_event(struct smsgc_mgr *smsgc, int timeout, char *str){
	ssize_t rval = 0;

	if (readl(smsgc->wrptr) == readl(smsgc->rdptr)) {
		if (timeout == 0) {
			/* no wait */
			printk(KERN_WARNING "smsgc-%d-%d rxcache is empty!\n",
				smsgc->dst, smsgc->channel);
			rval = -EAGAIN;
		} else if (timeout < 0) {
			/* wait forever */
			rval = wait_event_interruptible(smsgc->rxwait,
				readl(smsgc->wrptr) != readl(smsgc->rdptr) ||
				smsgc->state == SMSGC_STATE_IDLE);
			if (rval < 0) {
				printk(KERN_WARNING "smsgc-%d-%d %s wait interrupted!\n", smsgc->dst, smsgc->channel, str);
			}

			if (smsgc->state == SMSGC_STATE_IDLE) {
				printk(KERN_ERR "smsgc_%s smsgc-%d-%d state is idle!\n", str, smsgc->dst, smsgc->channel);
				rval = -EIO;
			}
		} else {
			/* wait timeout */
			rval = wait_event_interruptible_timeout(smsgc->rxwait,
				readl(smsgc->wrptr) != readl(smsgc->rdptr) ||
				smsgc->state == SMSGC_STATE_IDLE, timeout);
			if (rval < 0) {
				printk(KERN_WARNING "smsgc-%d-%d %s wait interrupted!\n", smsgc->dst, smsgc->channel, str);
			} else if (rval == 0) {
				printk(KERN_WARNING "smsgc-%d-%d %s wait timeout!\n", smsgc->dst, smsgc->channel, str);
				rval = -ETIME;
			}

			if (smsgc->state == SMSGC_STATE_IDLE) {
				printk(KERN_ERR "smsgc_%s smsgc-%d-%d state is idle!\n", str, smsgc->dst, smsgc->channel);
				rval = -EIO;
			}
		}
	}
	
	return rval;
}

int smsgc_recv(uint8_t dst, struct smsg *msg, int timeout)
{
	struct smsgc_mgr *smsgc;
	ssize_t rval = 0;
	ssize_t rd;
	uint8_t ch_index;

	ch_index = sipc_channel2index(msg->channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("%s:channel %d is not be configed!\n", __func__, msg->channel);
		return -EINVAL;
	}

	smsgc = smsgcs[dst][ch_index];
	if (!smsgc) {
		return -ENODEV;
	}

	if (smsgc->state != SMSGC_STATE_READY) {
		printk(KERN_ERR "smsgc-%d-%d not ready to recv!\n", smsgc->dst, smsgc->channel);
		return -ENODEV;
	}

	pr_debug("smsgc_recv: dst=%d, channel=%d, timeout=%d\n",
			smsgc->dst, smsgc->channel, timeout);
	pr_debug("smsgc_recv: dst=%d, channel=%d, wrptr=%d, rdptr=%d",
			smsgc->dst, smsgc->channel, readl(smsgc->wrptr), readl(smsgc->rdptr));

	if (timeout) {
		mutex_lock(&smsgc->rxlock);
	} else {
		if (!mutex_trylock(&(smsgc->rxlock))) {
			printk(KERN_INFO "smsgc-%d-%d recv busy!\n", smsgc->dst, smsgc->channel);
			return -EBUSY;
		}
	}

	rval = smsgc_wait_event(smsgc, timeout, "recv");
	if (!rval) {
		rd = readl(smsgc->rdptr) & (SMSG_CACHE_NR - 1);
		memcpy(msg, &(smsgc->caches[rd]), sizeof(struct smsg));
		writel(readl(smsgc->rdptr) + 1, smsgc->rdptr);
		pr_debug("smsgc_recv: wrptr=%d, rdptr=%d, rd=%d\n",
				readl(smsgc->wrptr), readl(smsgc->rdptr), rd);
		pr_debug("smsgc_recv: channel=%d, type=%d, flag=0x%04x, value=0x%08x\n",
				msg->channel, msg->type, msg->flag, msg->value);
	}
	mutex_unlock(&(smsgc->rxlock));
	
	return rval;
}

int smsgc_send(uint8_t dst, struct smsg *msg, int timeout)
{
	struct smsgc_mgr *smsgc;
	ssize_t rval = 0;
	uint8_t ch_index;

	ch_index = sipc_channel2index(msg->channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("%s:channel %d is not be configed!\n", __func__, msg->channel);
		return -EINVAL;
	}

	smsgc = smsgcs[dst][ch_index];
	if (!smsgc) {
		return -ENODEV;
	}

	if (smsgc->state != SMSGC_STATE_READY) {
		printk(KERN_ERR "smsgc-%d-%d not ready to send!\n", smsgc->dst, smsgc->channel);
		return -ENODEV;
	}
	if (!mutex_trylock(&(smsgc->txlock))) {
		printk(KERN_INFO "smsgc-%d-%d send busy!\n", smsgc->dst, smsgc->channel);
		return -EBUSY;
	}
	if (smsgc->state == SMSGC_STATE_READY) {
		/*smsg tx buf should not be full, timeout is ignored*/
		rval = smsg_send(dst, msg, timeout);
		if(!rval){
			smsgc->txmsg_cnt++;
		}
	} else {
		rval = -ENODEV;
	}
	mutex_unlock(&smsgc->txlock);

	return rval;
}

static int smsgc_open(struct inode *inode, struct file *filp)
{
	struct smsgc_device *smsgc;
	struct smsgc_priv *smsgc_p;

	smsgc = container_of(inode->i_cdev, struct smsgc_device, cdev);
	if (smsgc_status(smsgc->init->dst, smsgc->init->channel) != 0) {
		printk(KERN_ERR "smsgc %d-%d not ready to open!\n", smsgc->init->dst, smsgc->init->channel);
		filp->private_data = NULL;
		return -ENODEV;
	}

	smsgc_p = kmalloc(sizeof(struct smsgc_priv), GFP_KERNEL);
	if (!smsgc_p) {
		return -ENOMEM;
	}
	filp->private_data = smsgc_p;

	smsgc_p->dst = smsgc->init->dst;
	smsgc_p->channel = smsgc->init->channel;

	return 0;
}

static int smsgc_release(struct inode *inode, struct file *filp)
{
	struct smsgc_priv *smsgc_p = filp->private_data;

	if (smsgc_p) {
		kfree(smsgc_p);
	}

	return 0;
}

static ssize_t smsgc_read(struct file *filp,
		char __user *buf, size_t count, loff_t *ppos)
{
	struct smsgc_priv *smsgc_p = filp->private_data;
	struct smsgc_mgr *smsgc;
	int timeout = -1;
	ssize_t rval = 0;
	ssize_t rxmsgs, left, rd, tail;
	uint8_t ch_index;

	ch_index = sipc_channel2index(smsgc_p->channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("%s:channel %d is not be configed!\n", __func__, smsgc_p->channel);
		return -EINVAL;
	}
	smsgc = smsgcs[smsgc_p->dst][ch_index];

	if (filp->f_flags & O_NONBLOCK) {
		timeout = 0;
	}

	if (!smsgc) {
		return -ENODEV;
	}

	if (smsgc->state != SMSGC_STATE_READY) {
		printk(KERN_ERR "smsgc-%d-%d not ready to read!\n", smsgc->dst, smsgc->channel);
		return -ENODEV;
	}

	if((count%sizeof(struct smsg)) != 0){
		printk(KERN_WARNING "smsgc_read %d-%d: count=%d, aligned to 8 bytes, Invalid arguments.\n",
			smsgc->dst, smsgc->channel, count);
	}
	left = count/sizeof(struct smsg);

	pr_debug("smsgc_read: dst=%d, channel=%d, count=%d, timeout=%d\n",
			smsgc->dst, smsgc->channel, count, timeout);
	pr_debug("smsgc_read: dst=%d, channel=%d, wrptr=%d, rdptr=%d",
			smsgc->dst, smsgc->channel, readl(smsgc->wrptr), readl(smsgc->rdptr));

	if (timeout) {
		mutex_lock(&smsgc->rxlock);
	} else {
		if (!mutex_trylock(&(smsgc->rxlock))) {
			printk(KERN_INFO "smsgc-%d-%d read busy!\n", smsgc->dst, smsgc->channel);
			return -EBUSY;
		}
	}

	rval = smsgc_wait_event(smsgc, timeout, "read");

	while (left && (readl(smsgc->wrptr) != readl(smsgc->rdptr)) &&
			smsgc->state == SMSGC_STATE_READY) {
		/* calc rxmsgs */
		rxmsgs = (int)(readl(smsgc->wrptr) - readl(smsgc->rdptr));
		rxmsgs = min(rxmsgs, left);

		rd = readl(smsgc->rdptr) & (SMSG_CACHE_NR - 1);
		tail = rd + rxmsgs - SMSG_CACHE_NR;

		pr_debug("smsgc_read: dst= %d, channel=%d, buf=0x%p, rxmsgs=%d\n", smsgc->dst, smsgc->channel, buf, rxmsgs);

		if (tail > 0) {
			if ((uintptr_t)buf > TASK_SIZE) {
				unalign_memcpy(buf, &(smsgc->caches[rd]), (rxmsgs - tail) * sizeof(struct smsg));
				unalign_memcpy(buf + (rxmsgs - tail) * sizeof(struct smsg), &(smsgc->caches[0]), tail * sizeof(struct smsg));
			} else {
				/* handle the user space address */
				if(unalign_copy_to_user((void __user *)buf, &(smsgc->caches[rd]), (rxmsgs - tail) * sizeof(struct smsg)) ||
				    unalign_copy_to_user((void __user *)(buf + (rxmsgs - tail) * sizeof(struct smsg)),
				    &(smsgc->caches[0]), tail * sizeof(struct smsg))) {
					printk(KERN_ERR "smsgc-%d-%d read failed to copy to user!\n", smsgc->dst, smsgc->channel);
					rval = -EFAULT;
					break;
				}
			}
		} else {
			if ((uintptr_t)buf > TASK_SIZE) {
				unalign_memcpy(buf, &(smsgc->caches[rd]), rxmsgs * sizeof(struct smsg));
			} else {
				/* handle the user space address */
				if (unalign_copy_to_user((void __user *)buf, &(smsgc->caches[rd]), rxmsgs * sizeof(struct smsg))) {
					printk(KERN_ERR "smsgc-%d-%d read failed to copy to user!\n", smsgc->dst, smsgc->channel);
					rval = -EFAULT;
					break;
				}
			}
		}

		/* update rxcache rdptr */
		writel(readl(smsgc->rdptr) + rxmsgs, smsgc->rdptr);

		left -= rxmsgs;
		buf += rxmsgs * sizeof(struct smsg);
	}

	mutex_unlock(&smsgc->rxlock);

	pr_debug("smsgc_read done: dst=%d, channel=%d, count=%d, read=%d, rval=%d",
		smsgc->dst, smsgc->channel, count, count - left * sizeof(struct smsg), rval);

	if (count == left * sizeof(struct smsg)) {
		return rval;
	} else {
		return (count - left * sizeof(struct smsg));
	}
}

static ssize_t smsgc_write(struct file *filp,
		const char __user *buf, size_t count, loff_t *ppos)
{
	struct smsgc_priv *smsgc_p = filp->private_data;
	struct smsgc_mgr *smsgc;
	struct smsg msend;
	int timeout = -1;
	ssize_t rval = 0;
	ssize_t left, txmsgs, i;
	uint8_t ch_index;

	ch_index = sipc_channel2index(smsgc_p->channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("%s:channel %d is not be configed!\n", __func__, smsgc_p->channel);
		return -EINVAL;
	}
	smsgc = smsgcs[smsgc_p->dst][ch_index];

	if (filp->f_flags & O_NONBLOCK) {
		timeout = 0;
	}

	if (!smsgc) {
		return -ENODEV;
	}

	if (smsgc->state != SMSGC_STATE_READY) {
		printk(KERN_ERR "smsgc-%d-%d not ready to write!\n", smsgc->dst, smsgc->channel);
		return -ENODEV;
	}

	if((count%sizeof(struct smsg)) != 0){
		printk(KERN_ERR "smsgc_write %d-%d: count=%d, Invalid arguments.\n", smsgc->dst, smsgc->channel, count);
		return -EINVAL;
	}
	left = count/sizeof(struct smsg);

	if (!mutex_trylock(&(smsgc->txlock))) {
		printk(KERN_INFO "smsgc-%d-%d write busy!\n", smsgc->dst, smsgc->channel);
		return -EBUSY;
	}

	while (left && smsgc->state == SMSGC_STATE_READY) {
		txmsgs = min(SMSG_CACHE_NR, left);
		unalign_copy_from_user(smsgc->txbuf, (void __user *)(buf + count - left * sizeof(struct smsg)),
			txmsgs * sizeof(struct smsg));
		for(i = 0; i < txmsgs; i++) {
			/*smsg tx buf should not be full, timeout is ignored*/
			rval = smsg_send(smsgc->dst, (struct smsg *)(smsgc->txbuf + i * sizeof(struct smsg)), timeout);
			if(rval){
				goto end;
			}
			smsgc->txmsg_cnt++;
			left--;
		}
	}

end:
	mutex_unlock(&smsgc->txlock);

	pr_debug("smsgc_write done: dst=%d, channel=%d, count=%d, sent=%d, rval=%d",
		smsgc->dst, smsgc->channel, count, count - left * sizeof(struct smsg), rval);

	if (count == left * sizeof(struct smsg)) {
		return rval;
	} else {
		return (count - left * sizeof(struct smsg));
	}
}

/*only for read*/
static unsigned int smsgc_poll(struct file *filp, poll_table *wait)
{
	struct smsgc_priv *smsgc_p = filp->private_data;

	return smsgc_poll_wait(smsgc_p->dst, smsgc_p->channel,
			filp, wait);
}

static long smsgc_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	return 0;
}

static const struct file_operations smsgc_fops = {
	.open		= smsgc_open,
	.release	= smsgc_release,
	.read		= smsgc_read,
	.write		= smsgc_write,
	.poll		= smsgc_poll,
	.unlocked_ioctl	= smsgc_ioctl,
	.owner		= THIS_MODULE,
	.llseek		= default_llseek,
};

static int smsgc_parse_dt(struct smsgc_init_data **init, struct device *dev)
{
#ifdef CONFIG_OF
	struct device_node *np = dev->of_node;
	struct smsgc_init_data *pdata = NULL;
	int ret;
	uint32_t data;

	pdata = kzalloc(sizeof(struct smsgc_init_data), GFP_KERNEL);
	if (!pdata) {
		printk(KERN_ERR "Failed to allocate pdata memory\n");
		return -ENOMEM;
	}

	ret = of_property_read_string(np, "sprd,name", (const char**)&pdata->name);
	if (ret) {
		goto error;
	}

	ret = of_property_read_u32(np, "sprd,dst", (uint32_t *)&data);
	if (ret) {
		goto error;
	}
	pdata->dst = (uint8_t)data;

	ret = of_property_read_u32(np, "sprd,channel", (uint32_t *)&data);
	if (ret) {
		goto error;
	}
	pdata->channel = (uint8_t)data;

	*init = pdata;
	return ret;
error:
	kfree(pdata);
	*init = NULL;
	return ret;
#else
	return -ENODEV;
#endif
}

static inline void smsgc_destroy_pdata(struct smsgc_init_data **init)
{
#ifdef CONFIG_OF
	struct smsgc_init_data *pdata = *init;

	if (pdata) {
		kfree(pdata);
	}

	*init = NULL;
#else
	return;
#endif
}

static int smsgc_probe(struct platform_device *pdev)
{
	struct smsgc_init_data *init = pdev->dev.platform_data;
	struct smsgc_device *smsgc_dev;
	dev_t devid;
	int rval;

	if (pdev->dev.of_node && !init) {
		rval = smsgc_parse_dt(&init, &pdev->dev);
		if (rval) {
			printk(KERN_ERR "Failed to parse smsgc device tree, ret=%d\n", rval);
			return rval;
		}
	}
	pr_info("smsgc: after parse device tree, name=%s, dst=%u, channel=%u\n",
		init->name, init->dst, init->channel);


	rval = smsgc_create(init->dst, init->channel);
	if (rval != 0) {
		printk(KERN_ERR "Failed to create smsgc: %d\n", rval);
		smsgc_destroy_pdata(&init);
		return rval;
	}

	smsgc_dev = kzalloc(sizeof(struct smsgc_device), GFP_KERNEL);
	if (smsgc_dev == NULL) {
		smsgc_destroy(init->dst, init->channel);
		smsgc_destroy_pdata(&init);
		printk(KERN_ERR "Failed to allocate smsgc_device\n");
		return -ENOMEM;
	}

	rval = alloc_chrdev_region(&devid, 0, 1, init->name);
	if (rval != 0) {
		smsgc_destroy(init->dst, init->channel);
		kfree(smsgc_dev);
		smsgc_destroy_pdata(&init);
		printk(KERN_ERR "Failed to alloc smsgc chrdev\n");
		return rval;
	}

	cdev_init(&(smsgc_dev->cdev), &smsgc_fops);
	rval = cdev_add(&(smsgc_dev->cdev), devid, 1);
	if (rval != 0) {
		smsgc_destroy(init->dst, init->channel);
		kfree(smsgc_dev);
		unregister_chrdev_region(devid, 1);
		smsgc_destroy_pdata(&init);
		printk(KERN_ERR "Failed to add smsgc cdev\n");
		return rval;
	}

	smsgc_dev->major = MAJOR(devid);
	smsgc_dev->minor = MINOR(devid);

	device_create(smsgc_class, NULL,
		MKDEV(smsgc_dev->major, smsgc_dev->minor),
		NULL, "%s", init->name);

	smsgc_dev->init = init;

	platform_set_drvdata(pdev, smsgc_dev);

	return 0;
}

static int  smsgc_remove(struct platform_device *pdev)
{
	struct smsgc_device *smsgc_dev = platform_get_drvdata(pdev);

	device_destroy(smsgc_class,MKDEV(smsgc_dev->major, smsgc_dev->minor));
	cdev_del(&(smsgc_dev->cdev));
	unregister_chrdev_region(MKDEV(smsgc_dev->major, smsgc_dev->minor), 1);

	smsgc_destroy(smsgc_dev->init->dst, smsgc_dev->init->channel);

	smsgc_destroy_pdata(&smsgc_dev->init);

	kfree(smsgc_dev);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static const struct of_device_id smsgc_match_table[] = {
	{.compatible = "sprd,smsgc", },
	{ },
};

static struct platform_driver smsgc_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "smsgc",
		.of_match_table = smsgc_match_table,
	},
	.probe = smsgc_probe,
	.remove = smsgc_remove,
};

static int __init smsgc_init(void)
{
	smsgc_class = class_create(THIS_MODULE, "smsgc");
	if (IS_ERR(smsgc_class))
		return PTR_ERR(smsgc_class);

	return platform_driver_register(&smsgc_driver);
}

static void __exit smsgc_exit(void)
{
	class_destroy(smsgc_class);
	platform_driver_unregister(&smsgc_driver);
}

module_init(smsgc_init);
module_exit(smsgc_exit);

MODULE_AUTHOR("Zeng Cheng");
MODULE_DESCRIPTION("SIPC/SMSGC driver");
MODULE_LICENSE("GPL");


