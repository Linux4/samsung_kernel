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
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/wait.h>


#include <linux/sipc.h>
#include <linux/sipc_priv.h>
#include "sbuf.h"

#define VOLA_SBUF_SMEM volatile struct sbuf_smem_header
#define VOLA_SBUF_RING volatile struct sbuf_ring_header

static struct sbuf_mgr *sbufs[SIPC_ID_NR][SMSG_VALID_CH_NR];

static int sbuf_thread(void *data)
{
	struct sbuf_mgr *sbuf = data;
	struct smsg mcmd, mrecv;
	int rval, bufid;
	struct sched_param param = {.sched_priority = 90};

	/*set the thread as a real time thread, and its priority is 90*/
	sched_setscheduler(current, SCHED_RR, &param);

	/* since the channel open may hang, we call it in the sbuf thread */
	rval = smsg_ch_open(sbuf->dst, sbuf->channel, -1);
	if (rval != 0) {
		pr_err("Failed to open channel %d\n", sbuf->channel);
		/* assign NULL to thread poniter as failed to open channel */
		sbuf->thread = NULL;
		return rval;
	}

	/* sbuf init done, handle the ring rx events */
	while (!kthread_should_stop()) {
		/* monitor sbuf rdptr/wrptr update smsg */
		smsg_set(&mrecv, sbuf->channel, 0, 0, 0);
		rval = smsg_recv(sbuf->dst, &mrecv, -1);

		if (rval == -EIO) {
			/* channel state is free */
			msleep(20);
			continue;
		}

		pr_debug("sbuf thread recv msg: dst=%d, channel=%d, type=%d, flag=0x%04x, value=0x%08x\n",
			 sbuf->dst, sbuf->channel,
			 mrecv.type, mrecv.flag, mrecv.value);

		switch (mrecv.type) {
		case SMSG_TYPE_OPEN:
			/* handle channel recovery */
			smsg_open_ack(sbuf->dst, sbuf->channel);
			break;
		case SMSG_TYPE_CLOSE:
			/* handle channel recovery */
			smsg_close_ack(sbuf->dst, sbuf->channel);
			sbuf->state = SBUF_STATE_IDLE;
			break;
		case SMSG_TYPE_CMD:
			/* respond cmd done for sbuf init */
			WARN_ON(mrecv.flag != SMSG_CMD_SBUF_INIT);
			smsg_set(&mcmd, sbuf->channel, SMSG_TYPE_DONE,
					SMSG_DONE_SBUF_INIT, sbuf->smem_addr);
			smsg_send(sbuf->dst, &mcmd, -1);
			sbuf->state = SBUF_STATE_READY;
			break;
		case SMSG_TYPE_EVENT:
			bufid = mrecv.value;
			WARN_ON(bufid >= sbuf->ringnr);
			switch (mrecv.flag) {
			case SMSG_EVENT_SBUF_RDPTR:
				wake_up_interruptible_all(&(sbuf->
							rings[bufid].txwait));
				if (sbuf->rings[bufid].handler)
					sbuf->
					rings[bufid].handler(SBUF_NOTIFY_WRITE,
							     sbuf->
							     rings[bufid].data);
				break;
			case SMSG_EVENT_SBUF_WRPTR:
				wake_up_interruptible_all(&(sbuf->
							rings[bufid].rxwait));
				if (sbuf->rings[bufid].handler)
					sbuf->
					rings[bufid].handler(SBUF_NOTIFY_READ,
							     sbuf->
							     rings[bufid].data);
				break;
			default:
				rval = 1;
				break;
			}
			break;
		default:
			rval = 1;
			break;
		};
		if (rval) {
			pr_info("non-handled sbuf msg: %d-%d, %d, %d, %d\n",
					sbuf->dst, sbuf->channel,
					mrecv.type, mrecv.flag, mrecv.value);
			rval = 0;
		}
	}

	return 0;
}

int sbuf_create_ex(uint8_t dst, uint8_t channel, uint32_t bufnum,
		uint32_t txbufsize, uint32_t rxbufsize, char *sipc_name)
{
	struct sbuf_mgr *sbuf;
	struct sipc_device *sdev;
	VOLA_SBUF_SMEM *smem;
	VOLA_SBUF_RING *ringhd;
	int hsize, i, result;
	uint32_t len, sid;
	uint8_t ch_index;

	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("%s:channel %d is not be configed!\n", __func__, channel);
		return -EINVAL;
	}

	pr_debug("sbuf_create_ex dst=%d, chanel=%d, bufnum=%d, txbufsize=0x%x, rxbufsize=0x%x\n",
			dst, channel, bufnum, txbufsize, rxbufsize);

	if (sipc_name != NULL) {
		len = strlen(sipc_name);
		strncpy((char *)&sid, sipc_name + 4, 1);
		sid = (sid & 0xff) - '0';
		if (len != 5 || strncmp(sipc_name, "sipc", 4) != 0 || sid >= SIPC_DEV_NR) {
			pr_err("Sbuf failed to get valid sipc_name\n");
			return -ENODEV;
		}
	} else
		sid = 0;

	sdev = sipc_ap.sipc_dev[sid];

	sbuf = kzalloc(sizeof(struct sbuf_mgr), GFP_KERNEL);
	if (!sbuf) {
		pr_err("Failed to allocate mgr for sbuf\n");
		return -ENOMEM;
	}

	sbuf->state = SBUF_STATE_IDLE;
	sbuf->dst = dst;
	sbuf->channel = channel;
	sbuf->ringnr = bufnum;

	/* allocate smem */
	hsize = sizeof(struct sbuf_smem_header) +
		sizeof(struct sbuf_ring_header) * bufnum;
	sbuf->smem_size = hsize + (txbufsize + rxbufsize) * bufnum;
	sbuf->smem_addr = smem_alloc_ex(sbuf->smem_size, sdev->pdata->smem_base);
	if (!sbuf->smem_addr) {
		pr_err("Failed to allocate smem for sbuf\n");
		kfree(sbuf);
		return -ENOMEM;
	}

	pr_debug("sbuf_create_ex dst=%d, chanel=%d,bufnum=%d, smem_addr=0x%x, smem_size=0x%x\n",
			dst, channel, bufnum, sbuf->smem_addr, sbuf->smem_size);

	sbuf->smem_virt = shmem_ram_vmap_nocache(sbuf->smem_addr,
						sbuf->smem_size);
	if (!sbuf->smem_virt) {
		pr_err("Failed to map smem for sbuf\n");
		smem_free(sbuf->smem_addr, sbuf->smem_size);
		kfree(sbuf);
		return -EFAULT;
	}

	/* allocate rings description */
	sbuf->rings = kcalloc(bufnum, sizeof(struct sbuf_ring), GFP_KERNEL);
	if (!sbuf->rings) {
		shmem_ram_unmap(sbuf->smem_virt);
		smem_free(sbuf->smem_addr, sbuf->smem_size);
		kfree(sbuf);
		return -ENOMEM;
	}

	/* initialize all ring bufs */
	smem = (VOLA_SBUF_SMEM *)sbuf->smem_virt;
	smem->ringnr = bufnum;
	for (i = 0; i < bufnum; i++) {
		ringhd = (VOLA_SBUF_RING *)&(smem->headers[i]);
		ringhd->txbuf_addr = sbuf->smem_addr + hsize +
				(txbufsize + rxbufsize) * i;
		ringhd->txbuf_size = txbufsize;
		ringhd->txbuf_rdptr = 0;
		ringhd->txbuf_wrptr = 0;
		ringhd->rxbuf_addr = smem->headers[i].txbuf_addr + txbufsize;
		ringhd->rxbuf_size = rxbufsize;
		ringhd->rxbuf_rdptr = 0;
		ringhd->rxbuf_wrptr = 0;

		sbuf->rings[i].header = ringhd;
		sbuf->rings[i].txbuf_virt = sbuf->smem_virt + hsize +
				(txbufsize + rxbufsize) * i;
		sbuf->rings[i].rxbuf_virt = sbuf->rings[i].txbuf_virt +
						txbufsize;
		init_waitqueue_head(&(sbuf->rings[i].txwait));
		init_waitqueue_head(&(sbuf->rings[i].rxwait));
		mutex_init(&(sbuf->rings[i].txlock));
		mutex_init(&(sbuf->rings[i].rxlock));
	}

	sbuf->thread = kthread_create(sbuf_thread, sbuf,
			"sbuf-%d-%d", dst, channel);
	if (IS_ERR(sbuf->thread)) {
		pr_err("Failed to create kthread: sbuf-%d-%d\n", dst, channel);
		kfree(sbuf->rings);
		shmem_ram_unmap(sbuf->smem_virt);
		smem_free(sbuf->smem_addr, sbuf->smem_size);
		result = PTR_ERR(sbuf->thread);
		kfree(sbuf);

		return result;
	}

	sbufs[dst][ch_index] = sbuf;
	wake_up_process(sbuf->thread);

	return 0;
}
EXPORT_SYMBOL(sbuf_create_ex);


int sbuf_create(uint8_t dst, uint8_t channel, uint32_t bufnum,
		uint32_t txbufsize, uint32_t rxbufsize)
{
	int ret;

	ret = sbuf_create_ex(dst, channel, bufnum, txbufsize, rxbufsize, NULL);

	return ret;
}
EXPORT_SYMBOL(sbuf_create);

void sbuf_destroy(uint8_t dst, uint8_t channel)
{
	struct sbuf_mgr *sbuf;
	int i;
	uint8_t ch_index;

	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("%s:channel %d is not be configed!\n", __func__, channel);
		return;
	}

	sbuf = sbufs[dst][ch_index];
	if (sbuf == NULL)
		return;

	sbuf->state = SBUF_STATE_IDLE;
	smsg_ch_close(dst, channel, -1);

	/* stop sbuf thread if it's created successfully and still alive */
	if (!IS_ERR_OR_NULL(sbuf->thread))
		kthread_stop(sbuf->thread);

	if (sbuf->rings) {
		for (i = 0; i < sbuf->ringnr; i++) {
			wake_up_interruptible_all(&sbuf->rings[i].txwait);
			wake_up_interruptible_all(&sbuf->rings[i].rxwait);
		}
		kfree(sbuf->rings);
	}

	if (sbuf->smem_virt)
		shmem_ram_unmap(sbuf->smem_virt);
	smem_free(sbuf->smem_addr, sbuf->smem_size);
	kfree(sbuf);

	sbufs[dst][ch_index] = NULL;
}
EXPORT_SYMBOL(sbuf_destroy);

int sbuf_write(uint8_t dst, uint8_t channel, uint32_t bufid,
		void *buf, uint32_t len, int timeout)
{
	struct sbuf_mgr *sbuf;
	struct sbuf_ring *ring = NULL;
	VOLA_SBUF_RING *ringhd = NULL;
	struct smsg mevt;
	void *txpos;
	int rval, left, tail, txsize;
	uint8_t ch_index;

	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("%s:channel %d is not be configed!\n", __func__, channel);
		return -EINVAL;
	}

	sbuf = sbufs[dst][ch_index];
	if (!sbuf)
		return -ENODEV;
	ring = &(sbuf->rings[bufid]);
	ringhd = ring->header;
	if (sbuf->state != SBUF_STATE_READY) {
		pr_info("sbuf-%d-%d not ready to write!\n",
			dst, channel);
		return -ENODEV;
	}

	pr_debug("sbuf_write: dst=%d, channel=%d, bufid=%d, len=%d, timeout=%d\n",
			dst, channel, bufid, len, timeout);
	pr_debug("sbuf_write: channel=%d, wrptr=%d, rdptr=%d",
			channel, ringhd->txbuf_wrptr, ringhd->txbuf_rdptr);

	rval = 0;
	left = len;

	if (timeout) {
		mutex_lock(&ring->txlock);
	} else {
		if (!mutex_trylock(&(ring->txlock))) {
			pr_info("sbuf_write busy!\n");
			return -EBUSY;
		}
	}

	if (timeout == 0) {
		/* no wait */
		if ((int)(ringhd->txbuf_wrptr - ringhd->txbuf_rdptr) >=
				ringhd->txbuf_size) {
			pr_info("sbuf %d-%d ring %d txbuf is full!\n",
				dst, channel, bufid);
			rval = -EBUSY;
		}
	} else if (timeout < 0) {
		/* wait forever */
		rval = wait_event_interruptible(ring->txwait,
			(int)(ringhd->txbuf_wrptr - ringhd->txbuf_rdptr) <
			ringhd->txbuf_size || sbuf->state == SBUF_STATE_IDLE);
		if (rval < 0)
			pr_info("sbuf_write wait interrupted!\n");

		if (sbuf->state == SBUF_STATE_IDLE) {
			pr_err("sbuf_write sbuf state is idle!\n");
			rval = -EIO;
		}
	} else {
		/* wait timeout */
		rval = wait_event_interruptible_timeout(ring->txwait,
			(int)(ringhd->txbuf_wrptr - ringhd->txbuf_rdptr) <
			ringhd->txbuf_size || sbuf->state == SBUF_STATE_IDLE,
			timeout);
		if (rval < 0) {
			pr_info("sbuf_write wait interrupted!\n");
		} else if (rval == 0) {
			pr_info("sbuf_write wait timeout!\n");
			rval = -ETIME;
		}

		if (sbuf->state == SBUF_STATE_IDLE) {
			pr_err("sbuf_write sbuf state is idle!\n");
			rval = -EIO;
		}

	}

	while (left && (int)(ringhd->txbuf_wrptr - ringhd->txbuf_rdptr) <
	       ringhd->txbuf_size && sbuf->state == SBUF_STATE_READY) {
		/* calc txpos & txsize */
		txpos = ring->txbuf_virt +
			ringhd->txbuf_wrptr % ringhd->txbuf_size;
		txsize = ringhd->txbuf_size -
			(int)(ringhd->txbuf_wrptr - ringhd->txbuf_rdptr);
		txsize = min(txsize, left);

		tail = txpos + txsize - (ring->txbuf_virt + ringhd->txbuf_size);
		if (tail > 0) {
			/* ring buffer is rounded */
			if ((uintptr_t)buf > TASK_SIZE) {
				unalign_memcpy(txpos, buf, txsize - tail);
				unalign_memcpy(ring->txbuf_virt,
					       buf + txsize - tail, tail);
			} else {
				if (unalign_copy_from_user(txpos,
					(void __user *)buf, txsize - tail) ||
				   unalign_copy_from_user(ring->txbuf_virt,
					(void __user *)(buf + txsize - tail),
					tail)) {
					pr_err("sbuf_write: failed to copy from user!\n");
					rval = -EFAULT;
					break;
				}
			}
		} else {
			if ((uintptr_t)buf > TASK_SIZE) {
				unalign_memcpy(txpos, buf, txsize);
			} else {
				/* handle the user space address */
				if (unalign_copy_from_user(txpos,
						(void __user *)buf, txsize)) {
					pr_err("sbuf_write: failed to copy from user!\n");
					rval = -EFAULT;
					break;
				}
			}
		}


		pr_debug("sbuf_write: channel=%d, txpos=%p, txsize=%d\n",
			 channel, txpos, txsize);

		/* update tx wrptr */
		ringhd->txbuf_wrptr = ringhd->txbuf_wrptr + txsize;
		/* tx ringbuf is empty, so need to notify peer side */
		if (ringhd->txbuf_wrptr - ringhd->txbuf_rdptr == txsize) {
			smsg_set(&mevt, channel,
				 SMSG_TYPE_EVENT,
				 SMSG_EVENT_SBUF_WRPTR,
				 bufid);
			smsg_send(dst, &mevt, -1);
		}

		left -= txsize;
		buf += txsize;
	}

	mutex_unlock(&ring->txlock);

	pr_debug("sbuf_write done: channel=%d, len=%d\n", channel, len - left);

	if (len == left)
		return rval;
	else
		return (len - left);
}
EXPORT_SYMBOL(sbuf_write);

int sbuf_read(uint8_t dst, uint8_t channel, uint32_t bufid,
	      void *buf, uint32_t len, int timeout)
{
	struct sbuf_mgr *sbuf;
	struct sbuf_ring *ring = NULL;
	VOLA_SBUF_RING *ringhd = NULL;
	struct smsg mevt;
	void *rxpos;
	int rval, left, tail, rxsize;
	uint8_t ch_index;

	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("%s:channel %d is not be configed!\n", __func__, channel);
		return -EINVAL;
	}
	sbuf = sbufs[dst][ch_index];
	if (!sbuf)
		return -ENODEV;
	ring = &(sbuf->rings[bufid]);
	ringhd = ring->header;

	if (sbuf->state != SBUF_STATE_READY) {
		pr_info("sbuf-%d-%d not ready to read!\n", dst, channel);
		return -ENODEV;
	}

	pr_debug("sbuf_read:dst=%d, channel=%d, bufid=%d, len=%d, timeout=%d\n",
			dst, channel, bufid, len, timeout);
	pr_debug("sbuf_read: channel=%d, wrptr=%d, rdptr=%d",
			channel, ringhd->rxbuf_wrptr, ringhd->rxbuf_rdptr);

	rval = 0;
	left = len;

	if (timeout) {
		mutex_lock(&ring->rxlock);
	} else {
		if (!mutex_trylock(&(ring->rxlock))) {
			pr_info("sbuf_read busy!\n");
			return -EBUSY;
		}
	}

	if (ringhd->rxbuf_wrptr == ringhd->rxbuf_rdptr) {
		if (timeout == 0) {
			/* no wait */
			pr_info("sbuf %d-%d ring %d rxbuf is empty!\n",
				dst, channel, bufid);
			rval = -ENODATA;
		} else if (timeout < 0) {
			/* wait forever */
			rval = wait_event_interruptible(ring->rxwait,
				ringhd->rxbuf_wrptr != ringhd->rxbuf_rdptr ||
				sbuf->state == SBUF_STATE_IDLE);
			if (rval < 0)
				pr_info("sbuf_read wait interrupted!\n");

			if (sbuf->state == SBUF_STATE_IDLE) {
				pr_err("sbuf_read sbuf state is idle!\n");
				rval = -EIO;
			}
		} else {
			/* wait timeout */
			rval = wait_event_interruptible_timeout(ring->rxwait,
				ringhd->rxbuf_wrptr != ringhd->rxbuf_rdptr ||
				sbuf->state == SBUF_STATE_IDLE, timeout);
			if (rval < 0) {
				pr_info("sbuf_read wait interrupted!\n");
			} else if (rval == 0) {
				pr_info("sbuf_read wait timeout!\n");
				rval = -ETIME;
			}

			if (sbuf->state == SBUF_STATE_IDLE) {
				pr_err("sbuf_read sbuf state is idle!\n");
				rval = -EIO;
			}
		}
	}

	while (left && (ringhd->rxbuf_wrptr != ringhd->rxbuf_rdptr) &&
			sbuf->state == SBUF_STATE_READY) {
		/* calc rxpos & rxsize */
		rxpos = ring->rxbuf_virt +
			ringhd->rxbuf_rdptr % ringhd->rxbuf_size;
		rxsize = (int)(ringhd->rxbuf_wrptr - ringhd->rxbuf_rdptr);
		/* check overrun */
		if (rxsize > ringhd->rxbuf_size)
			pr_err("sbuf_read: bufid = %d, channel= %d rxsize=0x%x, rdptr=%d, wrptr=%d",
				bufid,
				channel,
				rxsize,
				ringhd->rxbuf_wrptr,
				ringhd->rxbuf_rdptr);

		rxsize = min(rxsize, left);

		pr_debug("sbuf_read: channel=%d, buf=%p, rxpos=%p, rxsize=%d\n",
			 channel, buf, rxpos, rxsize);

		tail = rxpos + rxsize - (ring->rxbuf_virt + ringhd->rxbuf_size);

		if (tail > 0) {
			/* ring buffer is rounded */
			if ((uintptr_t)buf > TASK_SIZE) {
				unalign_memcpy(buf, rxpos, rxsize - tail);
				unalign_memcpy(buf + rxsize - tail,
					       ring->rxbuf_virt, tail);
			} else {
				/* handle the user space address */
				if (unalign_copy_to_user((void __user *)buf,
							rxpos, rxsize - tail) ||
				    unalign_copy_to_user((void __user *)(buf
							+ rxsize - tail),
						ring->rxbuf_virt, tail)) {
					pr_err("sbuf_read: failed to copy to user!\n");
					rval = -EFAULT;
					break;
				}
			}
		} else {
			if ((uintptr_t)buf > TASK_SIZE) {
				unalign_memcpy(buf, rxpos, rxsize);
			} else {
				/* handle the user space address */
				if (unalign_copy_to_user((void __user *)buf,
							 rxpos, rxsize)) {
					pr_err("sbuf_read: failed to copy to user!\n");
					rval = -EFAULT;
					break;
				}
			}
		}

		/* update rx rdptr */
		ringhd->rxbuf_rdptr = ringhd->rxbuf_rdptr + rxsize;
		/* rx ringbuf is full ,so need to notify peer side */
		if (ringhd->rxbuf_wrptr - ringhd->rxbuf_rdptr ==
		    ringhd->rxbuf_size - rxsize) {
			smsg_set(&mevt, channel,
				 SMSG_TYPE_EVENT,
				 SMSG_EVENT_SBUF_RDPTR,
				 bufid);
			smsg_send(dst, &mevt, -1);
		}

		left -= rxsize;
		buf += rxsize;
	}

	mutex_unlock(&ring->rxlock);

	pr_debug("sbuf_read done: channel=%d, len=%d", channel, len - left);

	if (len == left)
		return rval;
	else
		return (len - left);
}
EXPORT_SYMBOL(sbuf_read);

int sbuf_poll_wait(uint8_t dst, uint8_t channel, uint32_t bufid,
		   struct file *filp, poll_table *wait)
{
	struct sbuf_mgr *sbuf;
	struct sbuf_ring *ring = NULL;
	VOLA_SBUF_RING *ringhd = NULL;
	unsigned int mask = 0;
	uint8_t ch_index;

	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("%s:channel %d is not be configed!\n", __func__, channel);
		return -EINVAL;
	}
	sbuf = sbufs[dst][ch_index];
	if (!sbuf)
		return -ENODEV;
	ring = &(sbuf->rings[bufid]);
	ringhd = ring->header;
	if (sbuf->state != SBUF_STATE_READY) {
		pr_err("sbuf-%d-%d not ready to poll !\n", dst, channel);
		return -ENODEV;
	}

	poll_wait(filp, &ring->txwait, wait);
	poll_wait(filp, &ring->rxwait, wait);

	if (ringhd->rxbuf_wrptr != ringhd->rxbuf_rdptr)
		mask |= POLLIN | POLLRDNORM;

	if (ringhd->txbuf_wrptr - ringhd->txbuf_rdptr < ringhd->txbuf_size)
		mask |= POLLOUT | POLLWRNORM;

	return mask;
}
EXPORT_SYMBOL(sbuf_poll_wait);

int sbuf_status(uint8_t dst, uint8_t channel)
{
	struct sbuf_mgr *sbuf;
	uint8_t ch_index;

	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("%s:channel %d is not be configed!\n", __func__, channel);
		return -EINVAL;
	}
	sbuf = sbufs[dst][ch_index];

	if (!sbuf)
		return -ENODEV;
	if (sbuf->state != SBUF_STATE_READY)
		return -ENODEV;

	return 0;
}
EXPORT_SYMBOL(sbuf_status);

int sbuf_register_notifier(uint8_t dst, uint8_t channel, uint32_t bufid,
			   void (*handler)(int event, void *data), void *data)
{
	struct sbuf_mgr *sbuf;
	struct sbuf_ring *ring = NULL;
	uint8_t ch_index;

	ch_index = sipc_channel2index(channel);
	if (ch_index == INVALID_CHANEL_INDEX) {
		pr_err("%s:channel %d is not be configed!\n", __func__, channel);
		return -EINVAL;
	}
	sbuf = sbufs[dst][ch_index];
	if (!sbuf)
		return -ENODEV;
	ring = &(sbuf->rings[bufid]);
	ring->handler = handler;
	ring->data = data;

	return 0;
}
EXPORT_SYMBOL(sbuf_register_notifier);

#if defined(CONFIG_DEBUG_FS)

static int sbuf_debug_show(struct seq_file *m, void *private)
{
	struct sbuf_mgr *sbuf = NULL;
	struct sbuf_ring *rings = NULL;
	VOLA_SBUF_RING *ring = NULL;
	struct list_head *listhd;
	wait_queue_t *pos;
	struct task_struct *task;
	int i, j, n, cnt;

	for (i = 0; i < SIPC_ID_NR; i++) {
		for (j = 0;  j < SMSG_VALID_CH_NR; j++) {
			sbuf = sbufs[i][j];
			if (!sbuf)
				continue;
			seq_puts(m, "***************************************************************************************************************************\n");
			seq_printf(m, "sbuf dst %d, channel: %3d, state: %d, smem_virt: 0x%lx, smem_addr: 0x%0x, smem_size: 0x%0x, ringnr: %d\n",
				   sbuf->dst,
				   sbuf->channel,
				   sbuf->state,
				   (unsigned long)sbuf->smem_virt,
				   sbuf->smem_addr,
				   sbuf->smem_size,
				   sbuf->ringnr);

			for (n = 0;  n < sbuf->ringnr;  n++) {
				rings = &(sbuf->rings[n]);
				ring = rings->header;
				cnt = 0;
				list_for_each(listhd, &rings->txwait.task_list) {
					pos = list_entry(listhd, wait_queue_t, task_list);
					task = pos->private;
					seq_printf(m, "sbuf ring[%2d]: txwait task #%d: %s, pid = %d, tgid = %d\n", n, cnt, task->comm, task->pid, task->tgid);
					cnt++;
				}
				seq_printf(m, "sbuf ring[%2d]: txbuf_addr :0x%0x, txbuf_rdptr :0x%0x, txbuf_wrptr :0x%0x, txbuf_size :0x%0x, txwait_tasks: %d\n",
					n, ring->txbuf_addr,
					ring->txbuf_rdptr,
					ring->txbuf_wrptr,
					ring->txbuf_size,
					cnt);

				cnt = 0;
				list_for_each(listhd, &rings->rxwait.task_list) {
					pos = list_entry(listhd, wait_queue_t, task_list);
					task = pos->private;
					seq_printf(m, "sbuf ring[%2d]: rxwait task #%d: %s, pid = %d, tgid = %d\n", n, cnt, task->comm, task->pid, task->tgid);
					cnt++;
				}
				seq_printf(m, "sbuf ring[%2d]: rxbuf_addr :0x%0x, rxbuf_rdptr :0x%0x, rxbuf_wrptr :0x%0x, rxbuf_size :0x%0x, rxwait_tasks: %d\n",
					n, ring->rxbuf_addr,
					ring->rxbuf_rdptr,
					ring->rxbuf_wrptr,
					ring->rxbuf_size,
					cnt);
			}
		}
	}
	return 0;
}

static int sbuf_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, sbuf_debug_show, inode->i_private);
}

static const struct file_operations sbuf_debug_fops = {
	.open = sbuf_debug_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

int sbuf_init_debugfs(void *root)
{
	if (!root)
		return -ENXIO;
	debugfs_create_file("sbuf", S_IRUGO,
			    (struct dentry *)root,
			    NULL, &sbuf_debug_fops);
	return 0;
}

#endif /* CONFIG_DEBUG_FS */

MODULE_AUTHOR("Chen Gaopeng");
MODULE_DESCRIPTION("SIPC/SBUF driver");
MODULE_LICENSE("GPL");
