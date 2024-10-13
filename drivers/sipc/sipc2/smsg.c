/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
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

#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/wait.h>

#include <linux/wakelock.h>
#include <linux/syscore_ops.h>

#ifdef CONFIG_SPRD_MAILBOX
#include <soc/sprd/mailbox.h>
#endif
#include <linux/sipc.h>
#include <linux/sipc_priv.h>


#define SIPC_READL(addr)      readl((void __iomem *) (addr))
#define SIPC_WRITEL(b, addr)  writel(b, (void __iomem *)(addr))

/* the array size must be 256 , because the max value of uint8 is 256*/
static uint8_t channel2index[SMSG_CH_NR + 1];
EXPORT_SYMBOL(channel2index);

struct smsg_ipc *smsg_ipcs[SIPC_ID_NR];
EXPORT_SYMBOL(smsg_ipcs);

static ushort debug_enable;

static ushort is_wklock_setup;
static struct wake_lock sipc_wake_lock;

module_param_named(debug_enable, debug_enable, ushort, 0644);

static void smsg_init_channel2index(void)
{
	uint16_t i, j;

	for (i = 0; i < sizeof(channel2index)/sizeof(uint8_t); i++) {
		for (j = 0; j < SMSG_VALID_CH_NR; j++) {
			/* find the index of channel i */
			if (sipc_cfg[j].channel == i)
				break;
		}

		/* if not find, init with INVALID_CHANEL_INDEX,
		 else init whith j */
		if(SMSG_VALID_CH_NR == j)
			channel2index[i] = INVALID_CHANEL_INDEX;
		else
			channel2index[i] = j;
	}
}

#ifdef CONFIG_SPRD_MAILBOX_FIFO
irqreturn_t smsg_irq_handler(void *ptr, void *private)
#else
irqreturn_t smsg_irq_handler(int irq, void *private)
#endif
{
	struct smsg_ipc *ipc = (struct smsg_ipc *)private;
	struct smsg *msg;
	struct smsg_channel *ch;
#ifndef CONFIG_SPRD_MAILBOX_FIFO
	uintptr_t rxpos;
#endif
	uint32_t wr;
	uint8_t ch_index;

#ifndef CONFIG_SPRD_MAILBOX
	if (ipc->rxirq_status(ipc->id))
		ipc->rxirq_clear(ipc->id);
#endif

#ifdef CONFIG_SPRD_MAILBOX_FIFO
	msg = ptr;
	do {
#else
	while (SIPC_READL(ipc->rxbuf_wrptr) != SIPC_READL(ipc->rxbuf_rdptr)) {
		rxpos = (SIPC_READL(ipc->rxbuf_rdptr) & (ipc->rxbuf_size - 1)) *
			sizeof(struct smsg) + ipc->rxbuf_addr;
		msg = (struct smsg *)rxpos;

		pr_debug("irq get smsg: wrptr=%u, rdptr=%u, rxpos=0x%lx\n",
			 SIPC_READL(ipc->rxbuf_wrptr),
			 SIPC_READL(ipc->rxbuf_rdptr),
			 rxpos);
#endif
		pr_debug("irq read smsg: dst=%d, channel=%d,type=%d, flag=0x%04x, value=0x%08x\n",
			ipc->dst,
			msg->channel,
			msg->type,
			msg->flag,
			msg->value);

		if (msg->type == SMSG_TYPE_DIE) {
			if (debug_enable)
				panic("cpcrash");
			else {
#ifndef CONFIG_SPRD_MAILBOX_FIFO
				/* update smsg rdptr */
				SIPC_WRITEL(SIPC_READL(ipc->rxbuf_rdptr) + 1,
					    ipc->rxbuf_rdptr);
#endif
				continue;
			}
		}

		ch_index = channel2index[msg->channel];
		if (msg->type >= SMSG_TYPE_NR || ch_index == INVALID_CHANEL_INDEX) {
			if (ch_index == INVALID_CHANEL_INDEX) {
				pr_err("%s:channel %d is not be configed!\n", __func__, msg->channel);
			}

			/* invalid msg */
			pr_info("invalid smsg: channel=%d, type=%d, flag=0x%04x, value=0x%08x\n",
				msg->channel, msg->type, msg->flag, msg->value);

#ifndef CONFIG_SPRD_MAILBOX_FIFO
			/* update smsg rdptr */
			SIPC_WRITEL(SIPC_READL(ipc->rxbuf_rdptr) + 1,
				    ipc->rxbuf_rdptr);
#endif
			continue;
		}

		ch = ipc->channels[ch_index];
		if (!ch) {
			if (ipc->states[ch_index] == CHAN_STATE_UNUSED &&
					msg->type == SMSG_TYPE_OPEN &&
					msg->flag == SMSG_OPEN_MAGIC) {

				ipc->states[ch_index] = CHAN_STATE_WAITING;
			} else {
				/* drop this bad msg since channel
				 * is not opened */
				pr_info("smsg channel %d not opened! drop smsg: type=%d, flag=0x%04x, value=0x%08x\n",
					msg->channel, msg->type,
					msg->flag, msg->value);
			}
#ifndef CONFIG_SPRD_MAILBOX_FIFO
			/* update smsg rdptr */
			SIPC_WRITEL(SIPC_READL(ipc->rxbuf_rdptr) + 1,
				    ipc->rxbuf_rdptr);
#endif
			continue;
		}

		atomic_inc(&(ipc->busy[ch_index]));

		if ((int)(SIPC_READL(ch->wrptr) - SIPC_READL(ch->rdptr)) >=
		    SMSG_CACHE_NR) {
			/* msg cache is full, drop this msg */
			pr_info("smsg channel %d recv cache is full! drop smsg: type=%d, flag=0x%04x, value=0x%08x\n",
				msg->channel, msg->type, msg->flag, msg->value);
		} else {
			/* write smsg to cache */
			wr = SIPC_READL(ch->wrptr) & (SMSG_CACHE_NR - 1);
			memcpy(&(ch->caches[wr]), msg, sizeof(struct smsg));
			SIPC_WRITEL(SIPC_READL(ch->wrptr) + 1, ch->wrptr);
		}
#ifndef CONFIG_SPRD_MAILBOX_FIFO
		/* update smsg rdptr */
		SIPC_WRITEL(SIPC_READL(ipc->rxbuf_rdptr) + 1, ipc->rxbuf_rdptr);
#endif

		wake_up_interruptible_all(&(ch->rxwait));

		atomic_dec(&(ipc->busy[ch_index]));
#ifdef CONFIG_SPRD_MAILBOX_FIFO
	} while (0);
#else
	}
#endif

	wake_lock_timeout(&sipc_wake_lock, HZ / 2);

	return IRQ_HANDLED;
}

int smsg_ipc_create(uint8_t dst, struct smsg_ipc *ipc)
{
	int rval;

	if (!ipc->irq_handler)
		ipc->irq_handler = smsg_irq_handler;

	spin_lock_init(&(ipc->txpinlock));
	smsg_init_channel2index();
	smsg_ipcs[dst] = ipc;

#ifdef CONFIG_SPRD_MAILBOX
	rval = mbox_register_irq_handle(ipc->core_id, ipc->irq_handler, ipc);
	if (rval != 0) {
		pr_err("Failed to register irq handler in mailbox: %s\n",
				ipc->name);
		return rval;
	}
#else
	/* explicitly call irq handler in case of missing irq on boot */
	ipc->irq_handler(ipc->irq, ipc);

	/* register IPI irq */
	rval = request_irq(ipc->irq, ipc->irq_handler,
			IRQF_NO_SUSPEND, ipc->name, ipc);
	if (rval != 0) {
		pr_err("Failed to request irq %s: %d\n",
				ipc->name, ipc->irq);
		return rval;
	}
#endif
	return 0;
}

int smsg_ipc_destroy(uint8_t dst)
{
	struct smsg_ipc *ipc = smsg_ipcs[dst];

	kthread_stop(ipc->thread);
	free_irq(ipc->irq, ipc);
	smsg_ipcs[dst] = NULL;

	return 0;
}

/* ****************************************************************** */

int smsg_ch_open(uint8_t dst, uint8_t channel, int timeout)
{
	struct smsg_ipc *ipc = smsg_ipcs[dst];
	struct smsg_channel *ch;
	struct smsg mopen, mrecv;
	int rval = 0;
	uint8_t ch_index;

	ch_index = channel2index[channel];
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("%s:channel %d is not be configed!\n", __func__, channel);
		return -EINVAL;
	}

	if (!ipc)
		return -ENODEV;

	ch = kzalloc(sizeof(struct smsg_channel), GFP_KERNEL);
	if (!ch)
		return -ENOMEM;

	atomic_set(&(ipc->busy[ch_index]), 1);
	init_waitqueue_head(&(ch->rxwait));
	mutex_init(&(ch->rxlock));
	ipc->channels[ch_index] = ch;

	smsg_set(&mopen, channel, SMSG_TYPE_OPEN, SMSG_OPEN_MAGIC, 0);
	rval = smsg_send(dst, &mopen, timeout);
	if (rval != 0) {
		pr_err("smsg_ch_open smsg send error, errno %d!\n", rval);
		ipc->states[ch_index] = CHAN_STATE_UNUSED;
		ipc->channels[ch_index] = NULL;
		atomic_dec(&(ipc->busy[ch_index]));
		/* guarantee that channel resource isn't used in irq handler  */
		while (atomic_read(&(ipc->busy[ch_index])))
			;
		kfree(ch);

		return rval;
	}

	/* open msg might be got before */
	if (ipc->states[ch_index] == CHAN_STATE_WAITING)
		goto open_done;

	do
	{
		smsg_set(&mrecv, channel, 0, 0, 0);
		rval = smsg_recv(dst, &mrecv, timeout);
		if (rval != 0) {
			pr_err("smsg_ch_open smsg receive error, errno %d!\n", rval);
			ipc->states[ch_index] = CHAN_STATE_UNUSED;
			ipc->channels[ch_index] = NULL;
			atomic_dec(&(ipc->busy[ch_index]));
			/* guarantee that channel resource isn't used in irq handler  */
			while(atomic_read(&(ipc->busy[ch_index]))) 
				;

			kfree(ch);
			return rval;
		}
	}
	while(mrecv.type != SMSG_TYPE_OPEN || mrecv.flag != SMSG_OPEN_MAGIC);

open_done:
	pr_debug("smsg_ch_open channel %d-%d success\n",dst, channel);
	ipc->states[ch_index] = CHAN_STATE_OPENED;
	atomic_dec(&(ipc->busy[ch_index]));

	return 0;
}
EXPORT_SYMBOL(smsg_ch_open);

int smsg_ch_close(uint8_t dst, uint8_t channel,  int timeout)
{
	struct smsg_ipc *ipc = smsg_ipcs[dst];
	struct smsg_channel *ch;
	struct smsg mclose;
	uint8_t ch_index;

	ch_index = channel2index[channel];
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("%s:channel %d is not be configed!\n", __func__, channel);
		return -EINVAL;
	}

	ch = ipc->channels[ch_index];
	if (!ch)
		return 0;

	smsg_set(&mclose, channel, SMSG_TYPE_CLOSE, SMSG_CLOSE_MAGIC, 0);
	smsg_send(dst, &mclose, timeout);

	ipc->states[ch_index] = CHAN_STATE_FREE;
	wake_up_interruptible_all(&(ch->rxwait));

	/* wait for the channel beeing unused */
	while (atomic_read(&(ipc->busy[ch_index])))
		;

	/* maybe channel has been free for smsg_ch_open failed */
	if (ipc->channels[ch_index]) {
		ipc->channels[ch_index] = NULL;
		/* guarantee that channel resource isn't used in irq handler */
		while (atomic_read(&(ipc->busy[ch_index])))
			;
		kfree(ch);
	}

	/* finally, update the channel state*/
	ipc->states[ch_index] = CHAN_STATE_UNUSED;

	return 0;
}
EXPORT_SYMBOL(smsg_ch_close);

int smsg_send(uint8_t dst, struct smsg *msg, int timeout)
{
	struct smsg_ipc *ipc = smsg_ipcs[dst];
#ifndef CONFIG_SPRD_MAILBOX_FIFO
	uintptr_t txpos;
#endif
	int rval = 0;
	unsigned long flags;
	uint8_t ch_index;

	ch_index = channel2index[msg->channel];
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("%s:channel %d is not be configed!\n", __func__, msg->channel);
		return -EINVAL;
	}

	if (!ipc)
		return -ENODEV;

	if (!ipc->channels[ch_index]) {
		pr_err("smsg_send: channel %d not inited!\n", msg->channel);
		return -ENODEV;
	}

	if (ipc->states[ch_index] != CHAN_STATE_OPENED &&
		msg->type != SMSG_TYPE_OPEN &&
		msg->type != SMSG_TYPE_CLOSE) {
		pr_err("smsg_send: channel %d not opened!\n", msg->channel);
		return -EINVAL;
	}

	pr_debug("smsg_send: dst=%d, channel=%d, timeout=%d\n",
			dst, msg->channel, timeout);
	pr_debug("send smsg: channel=%d, type=%d, flag=0x%04x, value=0x%08x\n",
			msg->channel, msg->type, msg->flag, msg->value);

	spin_lock_irqsave(&(ipc->txpinlock), flags);

	if (
#ifdef CONFIG_SPRD_MAILBOX_FIFO
		(ipc->rxirq_status(ipc->id) > 0)
#else
		((int)(SIPC_READL(ipc->txbuf_wrptr) -
			SIPC_READL(ipc->txbuf_rdptr)) >= ipc->txbuf_size)
#endif
	) {
		pr_info("smsg_send: smsg txbuf is full!\n");
		rval = -EBUSY;
		goto send_failed;
	}

	/* calc txpos and write smsg */
#ifdef CONFIG_SPRD_MAILBOX_FIFO
	ipc->txirq_trigger(ipc->id, *(u64 *)msg);
#else
	txpos = (SIPC_READL(ipc->txbuf_wrptr) & (ipc->txbuf_size - 1)) *
		sizeof(struct smsg) + ipc->txbuf_addr;
	memcpy((void *)txpos, msg, sizeof(struct smsg));

	pr_debug("write smsg: wrptr=%u, rdptr=%u, txpos=0x%lx\n",
			SIPC_READL(ipc->txbuf_wrptr),
			SIPC_READL(ipc->txbuf_rdptr), txpos);

	/* update wrptr */
	SIPC_WRITEL(SIPC_READL(ipc->txbuf_wrptr) + 1, ipc->txbuf_wrptr);
	ipc->txirq_trigger(ipc->id);
#endif

send_failed:
	spin_unlock_irqrestore(&(ipc->txpinlock), flags);

	return rval;
}
EXPORT_SYMBOL(smsg_send);

int smsg_recv(uint8_t dst, struct smsg *msg, int timeout)
{
	struct smsg_ipc *ipc = smsg_ipcs[dst];
	struct smsg_channel *ch;
	uint32_t rd;
	int rval = 0;
	uint8_t ch_index;

	ch_index = channel2index[msg->channel];
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("%s:channel %d is not be configed!\n", __func__, msg->channel);
		return -EINVAL;
	}

	if (!ipc)
		return -ENODEV;

	atomic_inc(&(ipc->busy[ch_index]));

	ch = ipc->channels[ch_index];

	if (!ch) {
		pr_err("smsg_recv: channel %d not opened!\n", msg->channel);
		atomic_dec(&(ipc->busy[ch_index]));
		return -ENODEV;
	}

	pr_debug("smsg_recv: dst=%d, channel=%d, timeout=%d, ch_index = %d\n",
			dst, msg->channel, timeout, ch_index);


	if (timeout == 0) {
		if (!mutex_trylock(&(ch->rxlock))) {
			pr_err("recv smsg busy!\n");
			atomic_dec(&(ipc->busy[ch_index]));

			return -EBUSY;
		}

		/* no wait */
		if (SIPC_READL(ch->wrptr) == SIPC_READL(ch->rdptr)) {
			pr_info("smsg rx cache is empty!\n");
			rval = -ENODATA;

			goto recv_failed;
		}
	} else if (timeout < 0) {
		mutex_lock(&(ch->rxlock));
		/* wait forever */
		rval = wait_event_interruptible(ch->rxwait,
				(SIPC_READL(ch->wrptr) !=
				 SIPC_READL(ch->rdptr)) ||
				(ipc->states[ch_index] ==
				 CHAN_STATE_FREE));
		if (rval < 0) {
			pr_info("smsg_recv wait interrupted!\n");

			goto recv_failed;
		}

		if (ipc->states[ch_index] == CHAN_STATE_FREE) {
			pr_info("smsg_recv smsg channel is free!\n");
			rval = -EIO;

			goto recv_failed;
		}
	} else {
		mutex_lock(&(ch->rxlock));
		/* wait timeout */
		rval = wait_event_interruptible_timeout(ch->rxwait,
			(SIPC_READL(ch->wrptr) != SIPC_READL(ch->rdptr)) ||
			(ipc->states[ch_index] == CHAN_STATE_FREE),
			timeout);
		if (rval < 0) {
			pr_info("smsg_recv wait interrupted!\n");

			goto recv_failed;
		} else if (rval == 0) {
			pr_info("smsg_recv wait timeout!\n");
			rval = -ETIME;

			goto recv_failed;
		}

		if (ipc->states[ch_index] == CHAN_STATE_FREE) {
			pr_err("smsg_recv smsg channel is free!\n");
			rval = -EIO;

			goto recv_failed;
		}
	}

	/* read smsg from cache */
	rd = SIPC_READL(ch->rdptr) & (SMSG_CACHE_NR - 1);
	memcpy(msg, &(ch->caches[rd]), sizeof(struct smsg));
	SIPC_WRITEL(SIPC_READL(ch->rdptr) + 1, ch->rdptr);
#ifndef CONFIG_SPRD_MAILBOX_FIFO
	pr_debug("read smsg: channel=%d, wrptr=%d, rdptr=%d, rd=%d\n",
			msg->channel, SIPC_READL(ch->wrptr), SIPC_READL(ch->rdptr), rd);
#endif
	pr_debug("recv smsg: channel=%d, type=%d, flag=0x%04x, value=0x%08x, rval = %d\n",
			msg->channel, msg->type, msg->flag, msg->value, rval);

recv_failed:
	mutex_unlock(&(ch->rxlock));
	atomic_dec(&(ipc->busy[ch_index]));
	return rval;
}
EXPORT_SYMBOL(smsg_recv);

uint8_t sipc_channel2index(uint8_t channel)
{
	return channel2index[channel];
}
EXPORT_SYMBOL(sipc_channel2index);

#if defined(CONFIG_DEBUG_FS)
static int smsg_debug_show(struct seq_file *m, void *private)
{
	struct smsg_ipc *smsg_sipc = NULL;
	int i, j;

#ifdef CONFIG_SPRD_MAILBOX_FIFO
	/* mailbox_debug_show(); */
#endif

	for (i = 0; i < SIPC_ID_NR; i++) {
		smsg_sipc = smsg_ipcs[i];
		if (!smsg_sipc)
			continue;
		seq_puts(m, "********************************************************************************\n");
		seq_printf(m, "sipc: %s:\n", smsg_sipc->name);
		seq_printf(m, "dst: 0x%0x, irq: 0x%0x\n",
			   smsg_sipc->dst, smsg_sipc->irq);
#ifndef CONFIG_SPRD_MAILBOX_FIFO
		seq_printf(m, "txbufAddr: 0x%p, txbufsize: 0x%x, txbufrdptr: [0x%p]=%d, txbufwrptr: [0x%p]=%d\n",
			   (void *)smsg_sipc->txbuf_addr,
			   smsg_sipc->txbuf_size,
			   (void *)smsg_sipc->txbuf_rdptr,
			   SIPC_READL(smsg_sipc->txbuf_rdptr),
			   (void *)smsg_sipc->txbuf_wrptr,
			   SIPC_READL(smsg_sipc->txbuf_wrptr));
		seq_printf(m, "rxbufAddr: 0x%p, rxbufsize: 0x%x, rxbufrdptr: [0x%p]=%d, rxbufwrptr: [0x%p]=%d\n",
			   (void *)smsg_sipc->rxbuf_addr,
			   smsg_sipc->rxbuf_size,
			   (void *)smsg_sipc->rxbuf_rdptr,
			   SIPC_READL(smsg_sipc->rxbuf_rdptr),
			   (void *)smsg_sipc->rxbuf_wrptr,
			   SIPC_READL(smsg_sipc->rxbuf_wrptr));
#endif
		for (j = 0;  j < SMSG_VALID_CH_NR; j++)
			seq_printf(m, "%2d. channel[%3d] states: %d, name: %s\n",
				j,
				sipc_cfg[j].channel,
				smsg_sipc->states[j],
				sipc_cfg[j].name);
	}
	return 0;
}

static int smsg_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, smsg_debug_show, inode->i_private);
}

static const struct file_operations smsg_debug_fops = {
	.open = smsg_debug_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

int smsg_init_debugfs(void *root)
{
	if (!root)
		return -ENXIO;
	debugfs_create_file("smsg", S_IRUGO,
			    (struct dentry *)root, NULL,
			    &smsg_debug_fops);
	return 0;
}

#endif /* CONFIG_DEBUG_FS */

static int sipc_syscore_suspend(void)
{
	int ret = 0;

#ifdef CONFIG_HAS_WAKELOCK
	ret = has_wake_lock(WAKE_LOCK_SUSPEND) ? -EAGAIN : 0;
#endif
	return ret;
}

static struct syscore_ops sipc_syscore_ops = {
	.suspend    = sipc_syscore_suspend,
};

int  smsg_suspend_init(void)
{
	if (!is_wklock_setup) {
		wake_lock_init(&sipc_wake_lock, WAKE_LOCK_SUSPEND, "sipc-smsg");
		register_syscore_ops(&sipc_syscore_ops);
		is_wklock_setup = 1;
	}

	return 0;
}

MODULE_AUTHOR("Chen Gaopeng");
MODULE_DESCRIPTION("SIPC/SMSG driver");
MODULE_LICENSE("GPL");
