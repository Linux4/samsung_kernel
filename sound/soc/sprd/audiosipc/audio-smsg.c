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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>

#include <sound/audio-smsg.h>
#include <linux/wakelock.h>
#include <linux/syscore_ops.h>
#include "sprd-asoc-common.h"

static struct saudio_ipc *saudio_ipcs[SIPC_ID_NR];

static ushort debug_enable = 0;
static ushort assert_trigger = 0;

static struct wake_lock sipc_wake_lock;

module_param_named(debug_enable, debug_enable, ushort, 0644);
module_param_named(assert_trigger, assert_trigger, ushort, 0644);

static void sprd_memcpy(char * dest_addr, char * src_addr, int n)
{
		while (n--) {
            *(char *)dest_addr = *(char *)src_addr;
            dest_addr = (char *)dest_addr + 1;
            src_addr = (char *)src_addr + 1;
		}
}

MBOX_FUNCALL saudio_irq_handler(void* ptr, void *dev_id)
{
	struct saudio_ipc *ipc = (struct saudio_ipc *)dev_id;
	struct smsg *msg;
	struct saudio_channel *ch;
	uintptr_t rxpos;
	uint32_t wr;

	while (readl(ipc->rxbuf_wrptr) != readl(ipc->rxbuf_rdptr)) {
		rxpos = (readl(ipc->rxbuf_rdptr) & (ipc->rxbuf_size - 1)) *
			sizeof (struct smsg) + ipc->rxbuf_addr;
		msg = (struct smsg *)rxpos;
		sp_asoc_pr_dbg("%s get smsg: wrptr=%u, rdptr=%u, rxpos=0x%lx\n",
			__func__, readl(ipc->rxbuf_wrptr), readl(ipc->rxbuf_rdptr), rxpos);
		sp_asoc_pr_dbg("%s read smsg: command=%d, channel=%d,"
			"parameter0=0x%08x, parameter1=0x%08x, parameter2=0x%08x, parameter3=0x%08x\n",
			__func__, msg->command, msg->channel, msg->parameter0, msg->parameter1, msg->parameter2, msg->parameter3);

		if (msg->channel >= SMSG_CH_NR/* || msg->command >= SMSG_CMD_NR */) {
			/* invalid msg */
			pr_err("%s, invalid smsg: command=%d, channel=%d,"
				"parameter0=0x%08x, parameter1=0x%08x, parameter2=0x%08x, parameter3=0x%08x\n",
				__func__, msg->command, msg->channel, msg->parameter0, msg->parameter1, msg->parameter2, msg->parameter3);

			/* update smsg rdptr */
			writel(readl(ipc->rxbuf_rdptr) + 1, ipc->rxbuf_rdptr);

			continue;
		}
		ch = ipc->channels[msg->channel];
		if (!ch) {
			if (ipc->states[msg->channel] == CHAN_STATE_UNUSED &&
					msg->command == SMSG_CMD_CONNECT &&
					msg->parameter0 == SMSG_CONNECT_MAGIC) {

				ipc->states[msg->channel] = CHAN_STATE_WAITING;
			} else {
				/* drop this bad msg since channel is not opened */
				pr_err("%s, smsg channel %d not opened! "
					"drop smsg: command=%d, parameter0=0x%08x, parameter1=0x%08x, parameter2=0x%08x, parameter3=0x%08x\n",
					__func__, msg->channel, msg->command, msg->parameter0, msg->parameter1, msg->parameter2, msg->parameter3);
			}
			/* update smsg rdptr */
			writel(readl(ipc->rxbuf_rdptr) + 1, ipc->rxbuf_rdptr);

			continue;
		}

		atomic_inc(&(ipc->busy[msg->channel]));

		if ((int)(readl(ch->wrptr) - readl(ch->rdptr)) >= SMSG_CACHE_NR) {
			/* msg cache is full, drop this msg */
			pr_err("%s, smsg channel %d recv cache is full! "
				"drop smsg: command=%d, parameter0=0x%08x, parameter1=0x%08x, parameter2=0x%08x, parameter3=0x%08x\n",
				__func__, msg->channel, msg->command, msg->parameter0, msg->parameter1, msg->parameter2, msg->parameter3);
		} else {
			/* write smsg to cache */
			wr = readl(ch->wrptr) & (SMSG_CACHE_NR - 1);
			sprd_memcpy((char*)&(ch->caches[wr]), (char *)msg, sizeof(struct smsg));
			writel(readl(ch->wrptr) + 1, ch->wrptr);
		}
		/* update smsg rdptr */
		writel(readl(ipc->rxbuf_rdptr) + 1, ipc->rxbuf_rdptr);

		wake_up_interruptible_all(&(ch->rxwait));

		atomic_dec(&(ipc->busy[msg->channel]));
	}

	//wake_lock_timeout(&sipc_wake_lock, HZ / 50);

	return IRQ_HANDLED;
}

int saudio_ipc_create(uint8_t dst, struct saudio_ipc *ipc)
{
	int rval;
	if (!ipc->irq_handler) {
		ipc->irq_handler = saudio_irq_handler;
	}
	spin_lock_init(&(ipc->txpinlock));

	saudio_ipcs[dst] = ipc;

#ifdef CONFIG_SPRD_MAILBOX
	/* explicitly call irq handler in case of missing irq on boot */
	ipc->irq_handler((void *)(&ipc->target_id), ipc);

	rval = mbox_register_irq_handle(ipc->target_id, ipc->irq_handler, ipc);
	if (rval != 0) {
		pr_err("%s, Failed to register irq handler in mailbox: %s\n",
				__func__, ipc->name);
		return rval;
	}
#endif
	return 0;
}

int saudio_ipc_destroy(uint8_t dst)
{
	struct saudio_ipc *ipc = saudio_ipcs[dst];

	//kthread_stop(ipc->thread);
	saudio_ipcs[dst] = NULL;

	return 0;
}

/* ****************************************************************** */

int saudio_ch_open(uint8_t dst, uint16_t channel, int timeout)
{
	struct saudio_ipc *ipc = saudio_ipcs[dst];
	struct saudio_channel *ch;
	struct smsg mopen, mrecv;
	int rval = 0;
	if(!ipc) {
		pr_err( "ERR:%s ipc ENODEV\n", __func__);
	    return -ENODEV;
	}

	ch = kzalloc(sizeof(struct saudio_channel), GFP_KERNEL);
	if (!ch) {
		pr_err("ERR:%s ENOMEM\n");
		return -ENOMEM;
	}

	atomic_set(&(ipc->busy[channel]), 1);
	init_waitqueue_head(&(ch->rxwait));
	mutex_init(&(ch->rxlock));
	ipc->channels[channel] = ch;
	goto open_done;
#if 0
	smsg_set(&mopen, channel, SMSG_CMD_CONNECT, SMSG_CONNECT_MAGIC, 0, 0, 0);
	printk(KERN_ERR "%s before  saudio_send\n",__func__);
	rval = saudio_send(dst, &mopen, timeout);
	if (rval != 0) {
		printk(KERN_ERR "%s smsg send error, errno %d!\n", __func__, rval);
		ipc->states[channel] = CHAN_STATE_UNUSED;
		ipc->channels[channel] = NULL;
		atomic_dec(&(ipc->busy[channel]));
		/* guarantee that channel resource isn't used in irq handler  */
		while(atomic_read(&(ipc->busy[channel]))) {
			;
		}
		kfree(ch);
		return rval;
	}

	/* open msg might be got before */
	if (ipc->states[channel] == CHAN_STATE_WAITING) {
		goto open_done;
	}
#endif

#if 0
	smsg_set(&mrecv, channel, 0, 0, 0, 0, 0);
	rval = saudio_recv(dst, &mrecv, timeout);
	if (rval != 0) {
		printk(KERN_ERR "%s smsg receive error, errno %d!\n", __func__, rval);
		ipc->states[channel] = CHAN_STATE_UNUSED;
		ipc->channels[channel] = NULL;
		atomic_dec(&(ipc->busy[channel]));
		/* guarantee that channel resource isn't used in irq handler  */
		while(atomic_read(&(ipc->busy[channel]))) {
			;
		}
		kfree(ch);
		return rval;
	}

	if (mrecv.command != SMSG_CMD_CONNECT || mrecv.parameter0 != SMSG_CONNECT_MAGIC) {
		printk(KERN_ERR "%s, Got bad open msg on channel %d-%d\n", __func__, dst, channel);
		ipc->states[channel] = CHAN_STATE_UNUSED;
		ipc->channels[channel] = NULL;
		atomic_dec(&(ipc->busy[channel]));
		/* guarantee that channel resource isn't used in irq handler  */
		while(atomic_read(&(ipc->busy[channel]))) {
			;
		}
		kfree(ch);
		return -EIO;
	}
#endif
open_done:
	ipc->states[channel] = CHAN_STATE_OPENED;
	atomic_dec(&(ipc->busy[channel]));

	return 0;
}

int saudio_ch_close(uint8_t dst, uint16_t channel,  int timeout)
{
	struct saudio_ipc *ipc = NULL;
	struct saudio_channel *ch = NULL;
	struct smsg mclose;

	if (!saudio_ipcs[dst]) {
		return 0; 
	}
	ipc = saudio_ipcs[dst];
	ch = ipc->channels[channel];
	if (!ch) {
		return 0;
	}
#if 0
	smsg_set(&mclose, channel, SMSG_CMD_DISCONNECT, SMSG_DISCONNECT_MAGIC, 0, 0, 0);
	saudio_send(dst, &mclose, timeout);
#endif
	ipc->states[channel] = CHAN_STATE_FREE;
#if 0
	wake_up_interruptible_all(&(ch->rxwait));

	/* wait for the channel beeing unused */
	while(atomic_read(&(ipc->busy[channel]))) {
		;
	}
#endif
	/* maybe channel has been free for saudio_ch_open failed */
	if (ipc->channels[channel]){
		ipc->channels[channel] = NULL;
		/* guarantee that channel resource isn't used in irq handler */
		while(atomic_read(&(ipc->busy[channel]))) {
			;
		}
		kfree(ch);
	}

	/* finally, update the channel state*/
	ipc->states[channel] = CHAN_STATE_UNUSED;

	return 0;
}

int saudio_send(uint8_t dst, struct smsg *msg, int timeout)
{
	struct saudio_ipc *ipc = saudio_ipcs[dst];
	uintptr_t txpos;
	int rval = 0;
	unsigned long flags;
	sp_asoc_pr_dbg("%s: dst=%d, channel=%d, timeout=%d\n",
			__func__, dst, msg->channel, timeout);
	if(!ipc) {
	    return -ENODEV;
	}

	if (!ipc->channels[msg->channel]) {
		return -ENODEV;
	}

	if (ipc->states[msg->channel] != CHAN_STATE_OPENED &&
		msg->command != SMSG_CMD_CONNECT &&
		msg->command != SMSG_CMD_DISCONNECT) {
		pr_err(" ERR: %s, channel %d not opened!\n", __func__, msg->channel);
		return -EINVAL;
	}

	sp_asoc_pr_dbg("%s: command=%d, channel=%d, parameter0=0x%08x, parameter1=0x%08x, parameter2=0x%08x, parameter3=0x%08x\n",
			__func__, msg->command, msg->channel, msg->parameter0, msg->parameter1, msg->parameter2, msg->parameter3);

	spin_lock_irqsave(&(ipc->txpinlock), flags);
	if ((int)(readl(ipc->txbuf_wrptr) -
		readl(ipc->txbuf_rdptr)) >= ipc->txbuf_size) {
		pr_warning("%s txbuf is full!\n", __func__);
		rval = -EBUSY;
		goto send_failed;
	}
	/* calc txpos and write smsg */
	txpos = (readl(ipc->txbuf_wrptr) & (ipc->txbuf_size - 1)) *
		sizeof(struct smsg) + ipc->txbuf_addr;
	//memcpy((void *)txpos, msg, sizeof(struct smsg));
	sprd_memcpy((char *)txpos, (char *)msg, sizeof(struct smsg));

	sp_asoc_pr_dbg("%s: wrptr=%u, rdptr=%u, txpos=0x%lx\n",
			__func__, readl(ipc->txbuf_wrptr),
			readl(ipc->txbuf_rdptr), txpos);

	/* update wrptr */
	writel(readl(ipc->txbuf_wrptr) + 1, ipc->txbuf_wrptr);
    unsigned long long msg_val = 0;
    unsigned long long command = msg->command;
    unsigned long long channel = msg->channel;
    unsigned long long parameter0 = msg->parameter0;
    msg_val =  (command << 48) | (channel << 32) | (parameter0);
	mbox_raw_sent(ipc->target_id, msg_val);

send_failed:
	spin_unlock_irqrestore(&(ipc->txpinlock), flags);

	return rval;
}

int saudio_recv(uint8_t dst, struct smsg *msg, int timeout)
{
	struct saudio_ipc *ipc = saudio_ipcs[dst];
	struct saudio_channel *ch;
	uint32_t rd;
	int rval = 0;

	if(!ipc) {
	    return -ENODEV;
	}

	atomic_inc(&(ipc->busy[msg->channel]));

	ch = ipc->channels[msg->channel];

	if (!ch) {
		pr_err("%s, channel %d not opened!\n", __func__, msg->channel);
		atomic_dec(&(ipc->busy[msg->channel]));
		return -ENODEV;
	}

	sp_asoc_pr_dbg("%s: dst=%d, channel=%d, timeout=%d\n",
			__func__, dst, msg->channel, timeout);


	if (timeout == 0) {
		if (!mutex_trylock(&(ch->rxlock))) {
			sp_asoc_pr_info("%s busy!\n", __func__);
			atomic_dec(&(ipc->busy[msg->channel]));
			return -EBUSY;
		}

		/* no wait */
		if (readl(ch->wrptr) == readl(ch->rdptr)) {
			sp_asoc_pr_info("warning %s cache is empty!\n", __func__);
			rval = -ENODATA;
			goto recv_failed;
		}
	} else if (timeout < 0) {
		mutex_lock(&(ch->rxlock));
		/* wait forever */
		rval = wait_event_interruptible(ch->rxwait,
				(readl(ch->wrptr) != readl(ch->rdptr)) ||
				(ipc->states[msg->channel] == CHAN_STATE_FREE));
		if (rval < 0) {
			sp_asoc_pr_info("%s wait interrupted!\n", __func__);
			goto recv_failed;
		}

		if (ipc->states[msg->channel] == CHAN_STATE_FREE) {
			sp_asoc_pr_info("warning %s smsg channel is free!\n", __func__);
			rval = -EIO;
			goto recv_failed;
		}
	} else {
		mutex_lock(&(ch->rxlock));
		/* wait timeout */
		rval = wait_event_interruptible_timeout(ch->rxwait,
			(readl(ch->wrptr) != readl(ch->rdptr)) ||
			(ipc->states[msg->channel] == CHAN_STATE_FREE), timeout);
		if (rval < 0) {
			sp_asoc_pr_info("warning %s wait interrupted!\n", __func__);
			goto recv_failed;
		} else if (rval == 0) {
			sp_asoc_pr_info("warning %s wait timeout!\n", __func__);
			rval = -ETIME;
			goto recv_failed;
		}

		if (ipc->states[msg->channel] == CHAN_STATE_FREE) {
			printk(KERN_ERR "%s smsg channel is free!\n", __func__);
			rval = -EIO;
			goto recv_failed;
		}
	}

	/* read smsg from cache */
	rd = readl(ch->rdptr) & (SMSG_CACHE_NR - 1);
	sprd_memcpy((char *)msg, (char *)(&(ch->caches[rd])), sizeof(struct smsg));
	writel(readl(ch->rdptr) + 1, ch->rdptr);

	sp_asoc_pr_dbg("%s: wrptr=%d, rdptr=%d, rd=%d\n",
			__func__, readl(ch->wrptr), readl(ch->rdptr), rd);
	sp_asoc_pr_dbg("%s: command=%d, channel=%d, parameter0=0x%08x, parameter1=0x%08x, parameter2=0x%08x, parameter3=0x%08x\n",
			__func__, msg->command, msg->channel, msg->parameter0, msg->parameter1, msg->parameter2, msg->parameter3);

recv_failed:
	mutex_unlock(&(ch->rxlock));
	atomic_dec(&(ipc->busy[msg->channel]));
	return rval;
}

#if defined(CONFIG_DEBUG_FS)
static int saudio_debug_show(struct seq_file *m, void *private)
{
	struct saudio_ipc *saudio_sipc = NULL;
	int i, j;

	for (i = 0; i < SIPC_ID_NR; i++) {
		saudio_sipc = saudio_ipcs[i];
		if (!saudio_sipc) {
			continue;
		}
		seq_printf(m, "sipc: %s: \n", saudio_sipc->name);
		seq_printf(m, "dst: 0x%0x \n", saudio_sipc->dst);
		seq_printf(m, "txbufAddr: 0x%0x, txbufsize: 0x%0x, txbufrdptr: [0x%0x]=%lu, txbufwrptr: [0x%0x]=%lu\n",
			   saudio_sipc->txbuf_addr, saudio_sipc->txbuf_size, saudio_sipc->txbuf_rdptr, readl(saudio_sipc->txbuf_rdptr), saudio_sipc->txbuf_wrptr, readl(saudio_sipc->txbuf_wrptr));
		seq_printf(m, "rxbufAddr: 0x%0x, rxbufsize: 0x%0x, rxbufrdptr: [0x%0x]=%lu, rxbufwrptr: [0x%0x]=%lu\n",
			   saudio_sipc->rxbuf_addr, saudio_sipc->rxbuf_size, saudio_sipc->rxbuf_rdptr, readl(saudio_sipc->rxbuf_rdptr), saudio_sipc->rxbuf_wrptr, readl(saudio_sipc->rxbuf_wrptr));

		for (j=0;  j<SMSG_CH_NR; j++) {
			seq_printf(m, "channel[%d] states: %d\n", j, saudio_sipc->states[j]);
		}
	}
	return 0;
}

static int saudio_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, saudio_debug_show, inode->i_private);
}

static const struct file_operations saudio_debug_fops = {
	.open = saudio_debug_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

int saudio_init_debugfs(void *root )
{
	if (!root)
		return -ENXIO;
	debugfs_create_file("saudio-sipc", S_IRUGO, (struct dentry *)root, NULL, &saudio_debug_fops);
	return 0;
}

#endif /* CONFIG_DEBUG_FS */

static int saudio_syscore_suspend(void)
{
	int ret = has_wake_lock(WAKE_LOCK_SUSPEND) ? -EAGAIN : 0;
	return ret;
}

static struct syscore_ops saudio_syscore_ops = {
	.suspend    = saudio_syscore_suspend,
};

int  saudio_suspend_init(void)
{
	wake_lock_init(&sipc_wake_lock, WAKE_LOCK_SUSPEND, "saudio-sipc");
	register_syscore_ops(&saudio_syscore_ops);

	return 0;
}

EXPORT_SYMBOL(saudio_ch_open);
EXPORT_SYMBOL(saudio_ch_close);
EXPORT_SYMBOL(saudio_send);
EXPORT_SYMBOL(saudio_recv);

MODULE_AUTHOR("Chen Gaopeng");
MODULE_DESCRIPTION("SIPC/SMSG driver");
MODULE_LICENSE("GPL");

