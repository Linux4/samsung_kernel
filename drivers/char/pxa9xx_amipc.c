/*
 * This software program is licensed subject to the GNU General Public License
 * (GPL).Version 2,June 1991, available at http://www.fsf.org/copyleft/gpl.html

 * (C) Copyright 2014 Marvell International Ltd.
 * All Rights Reserved
 */

#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/fcntl.h>
#include <linux/poll.h>
#include <linux/aio.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/moduleparam.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/pm_wakeup.h>
#include <linux/vmalloc.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/dma-buf.h>

#include <linux/pxa9xx_amipc.h>

#define pmu_readl(off)	__raw_readl(amipc->pmu_base + (off))
#define pmu_writel(off, v)	__raw_writel((v), amipc->pmu_base + (off))
#define ciu_readl(off)	__raw_readl(amipc->ciu_base + (off))
#define ciu_writel(off, v)	__raw_writel((v), amipc->ciu_base + (off))
#define dmachain_writel(off, v)	__raw_writel((v), amipc->dmachain_base + (off))

#define E_TO_OFF(v) ((v) - 1)
#define OFF_TO_E(v) ((v) + 1)

/* APMU register */
#define GNSS_WAKEUP_CTRL	(0x1B8)

/* CIU register */
#define GNSS_HANDSHAKE		(0x168)

/* GNSS_WAKEUP_CTRL */
#define GNSS_WAKEUP_STATUS	(1 << 2)
#define GNSS_WAKEUP_CLR		(1 << 1)
#define GNSS_WAKEUP_EN		(1 << 0)

/* GNSS_HANDSHAKE */
#define GNSS_IRQ_OUT_ST		(1 << 4)
#define GNSS_CODE_INIT_RDY	(1 << 3)
#define GNSS_CODE_INIT_DONE	(1 << 2)
#define AP_GNSS_WAKEUP		(1 << 1)
#define GNSS_IRQ_CLR		(1 << 0)

#define MSG_BUFFER_LEN		(16)

struct ipc_event_type {
	enum amipc_events event;
	u32 data1, data2;
	u32 ack;
};

struct amipc_database_cell {
	enum amipc_events event;
	amipc_rec_event_callback cb;
	int tx_cnt;
	int rx_cnt;
};

struct amipc_msg_item {
	enum amipc_events event;
	u32 data1, data2;
};

struct amipc_msg_buffer {
	int ipc_wptr;
	int sdk_rptr;
	struct amipc_msg_item msg[MSG_BUFFER_LEN];
	int ov_cnt;
};

struct amipc_acq_infor {
	int wr_num;
	int rd_num;
};

struct amipc_dbg_info {
	int irq_num;
	int rst_bit1_num;
};

struct pxa9xx_amipc {
	int irq;
	void __iomem *pmu_base;
	void __iomem *ciu_base;
	void __iomem *dmachain_base;
	struct ipc_event_type *ipc_tx, *ipc_rx;
	struct amipc_database_cell amipc_db[AMIPC_EVENT_LAST];
	struct ipc_event_type user_bakup;
	wait_queue_head_t amipc_wait_q;
	int poll_status;
	struct dentry *dbg_dir;
	struct dentry *dbg_shm;
	struct dentry *dbg_pkg;
	struct dentry *dbg_ctrl;
	struct amipc_msg_buffer msg_buffer;
	struct amipc_acq_infor acq_info;
	struct amipc_dbg_info dbg_info;
	struct device *this_dev;
};
static struct pxa9xx_amipc *amipc;
static DEFINE_SPINLOCK(amipc_lock);
static DEFINE_SPINLOCK(transfer_lock);
static void amipc_ping_worker(struct work_struct *work);
static void *shm_map(phys_addr_t start, size_t size);
static DECLARE_WORK(ping_work, amipc_ping_worker);

static void init_statistic_info(void)
{
	int i;

	for (i = 0; i < AMIPC_EVENT_LAST; i++) {
		amipc->amipc_db[i].tx_cnt = 0;
		amipc->amipc_db[i].rx_cnt = 0;
	}
	memset((void *)&amipc->msg_buffer, 0,
			sizeof(struct amipc_msg_buffer));
	memset((void *)&amipc->acq_info, 0,
			sizeof(struct amipc_acq_infor));
	memset((void *)&amipc->dbg_info, 0,
			sizeof(struct amipc_dbg_info));
}

static void amipc_ping_worker(struct work_struct *work)
{
	amipc_datasend(AMIPC_LL_PING, 2, 0, DEFAULT_TIMEOUT);
}

static int amipc_txaddr_inval(void)
{
	if (amipc->ipc_tx)
		return 0;
	pr_err("tx shared mem invalid\n");
	return -EINVAL;
}

static int amipc_rxaddr_inval(void)
{
	if (amipc->ipc_rx)
		return 0;
	pr_err("rx shared mem invalid\n");
	return -EINVAL;
}

static u32 amipc_default_callback(u32 status)
{
	IPC_ENTER();

	pr_info("event %d not binded\n", status);

	IPC_LEAVE();
	return 0;
}

static u32 amipc_ll_ping_callback(u32 event)
{
	u32 data1;

	IPC_ENTER();
	if (AMIPC_RC_OK == amipc_dataread(event, &data1, NULL)) {
		if (1 == data1)
			schedule_work(&ping_work);
		else if (2 == data1)
			pr_info("get ping response\n");
		else {
			pr_err("invalid ping packet\n");
			IPC_LEAVE();
			return -EINVAL;
		}
	} else {
		IPC_LEAVE();
		return -EINVAL;
	}
	IPC_LEAVE();
	return 0;
}

static u32 amipc_acq_callback(u32 event)
{
	u32 data1, data2;
	unsigned long flags;

	IPC_ENTER();
	if (AMIPC_RC_OK == amipc_dataread(event, &data1, &data2)) {
		amipc->msg_buffer.msg[amipc->msg_buffer.ipc_wptr].event = event;
		amipc->msg_buffer.msg[amipc->msg_buffer.ipc_wptr].data1 = data1;
		amipc->msg_buffer.msg[amipc->msg_buffer.ipc_wptr].data2 = data2;
		amipc->msg_buffer.ipc_wptr = (amipc->msg_buffer.ipc_wptr + 1) %
							MSG_BUFFER_LEN;
		if (unlikely(amipc->msg_buffer.ipc_wptr ==
					amipc->msg_buffer.sdk_rptr))
			amipc->msg_buffer.ov_cnt++;

		if (AMIPC_ACQ_COMMAND == event)
			amipc->acq_info.wr_num++;

		spin_lock_irqsave(&amipc_lock, flags);
		amipc->poll_status = 1;
		spin_unlock_irqrestore(&amipc_lock, flags);
		wake_up_interruptible(&amipc->amipc_wait_q);
	} else {
		IPC_LEAVE();
		return -EINVAL;
	}
	IPC_LEAVE();
	return 0;
}

static void amipc_notify_peer(void)
{
	u32 ciu_reg;

	IPC_ENTER();
	/* read three times for clock gate off problem */
	ciu_reg = ciu_readl(GNSS_HANDSHAKE);
	ciu_reg = ciu_readl(GNSS_HANDSHAKE);
	ciu_reg = ciu_readl(GNSS_HANDSHAKE);
	ciu_reg |= AP_GNSS_WAKEUP;
	ciu_writel(GNSS_HANDSHAKE, ciu_reg);
	IPC_LEAVE();
}

static enum amipc_return_code amipc_event_set(enum amipc_events user_event,
						int timeout_ms)
{
	unsigned long end_time;
	u32 ciu_reg;
	unsigned long flags;

	IPC_ENTER();
	if (amipc_txaddr_inval())
		return AMIPC_RC_FAILURE;

	if (timeout_ms > 0) {
		end_time = jiffies + msecs_to_jiffies(timeout_ms);
		while (true) {
			if (!(amipc->ipc_tx[E_TO_OFF(user_event)].ack))
				break;
			if (time_after(jiffies, end_time)) {
				pr_warn("tx: wait rdy timeout\n");
				IPC_LEAVE();
				return AMIPC_RC_TIMEOUT;
			}
			msleep(20);
		}
	}
	if (!(amipc->ipc_tx[E_TO_OFF(user_event)].ack)) {
		amipc->ipc_tx[E_TO_OFF(user_event)].event = user_event;
		amipc->ipc_tx[E_TO_OFF(user_event)].data1 = 0;
		amipc->ipc_tx[E_TO_OFF(user_event)].data2 = 0;
		amipc->ipc_tx[E_TO_OFF(user_event)].ack = 1;
		amipc->amipc_db[E_TO_OFF(user_event)].tx_cnt++;
		spin_lock_irqsave(&transfer_lock, flags);
		amipc_notify_peer();
		spin_unlock_irqrestore(&transfer_lock, flags);
	} else {
		/* read three times for clock gate off problem */
		spin_lock_irqsave(&transfer_lock, flags);
		ciu_reg = ciu_readl(GNSS_HANDSHAKE);
		ciu_reg = ciu_readl(GNSS_HANDSHAKE);
		ciu_reg = ciu_readl(GNSS_HANDSHAKE);
		ciu_reg &= ~AP_GNSS_WAKEUP;
		ciu_writel(GNSS_HANDSHAKE, ciu_reg);
		/* add 1ms delay for sync signal to CM3 */
		mdelay(1);
		pr_info("cm3:reset handshake bit1\n");
		ciu_reg = ciu_readl(GNSS_HANDSHAKE);
		ciu_reg |= AP_GNSS_WAKEUP;
		ciu_writel(GNSS_HANDSHAKE, ciu_reg);
		/* add 1ms delay for sync signal to CM3 */
		mdelay(1);
		amipc->dbg_info.rst_bit1_num++;
		spin_unlock_irqrestore(&transfer_lock, flags);
		IPC_LEAVE();
		return AMIPC_RC_AGAIN;
	}

	IPCTRACE("amipc_event_set userEvent 0x%x\n", user_event);

	IPC_LEAVE();
	return AMIPC_RC_OK;
}

static enum amipc_return_code amipc_data_send(enum amipc_events user_event,
					u32 data1, u32 data2, int timeout_ms)
{
	unsigned long end_time;
	u32 ciu_reg;
	unsigned long flags;

	IPC_ENTER();
	if (amipc_txaddr_inval()) {
		IPC_LEAVE();
		return AMIPC_RC_FAILURE;
	}

	if (timeout_ms > 0) {
		end_time = jiffies + msecs_to_jiffies(timeout_ms);
		while (true) {
			if (!(amipc->ipc_tx[E_TO_OFF(user_event)].ack))
				break;
			if (time_after(jiffies, end_time)) {
				pr_warn("tx: wait rdy timeout\n");
				IPC_LEAVE();
				return AMIPC_RC_TIMEOUT;
			}
			msleep(20);
		}
	}
	if (!(amipc->ipc_tx[E_TO_OFF(user_event)].ack)) {
		amipc->ipc_tx[E_TO_OFF(user_event)].event = user_event;
		amipc->ipc_tx[E_TO_OFF(user_event)].data1 = data1;
		amipc->ipc_tx[E_TO_OFF(user_event)].data2 = data2;
		amipc->ipc_tx[E_TO_OFF(user_event)].ack = 1;
		amipc->amipc_db[E_TO_OFF(user_event)].tx_cnt++;
		spin_lock_irqsave(&transfer_lock, flags);
		amipc_notify_peer();
		spin_unlock_irqrestore(&transfer_lock, flags);
	} else {
		/* read three times for clock gate off problem */
		spin_lock_irqsave(&transfer_lock, flags);
		ciu_reg = ciu_readl(GNSS_HANDSHAKE);
		ciu_reg = ciu_readl(GNSS_HANDSHAKE);
		ciu_reg = ciu_readl(GNSS_HANDSHAKE);
		ciu_reg &= ~AP_GNSS_WAKEUP;
		ciu_writel(GNSS_HANDSHAKE, ciu_reg);
		/* add 1ms delay for sync signal to CM3 */
		mdelay(1);
		pr_info("cm3:reset handshake bit1\n");
		ciu_reg = ciu_readl(GNSS_HANDSHAKE);
		ciu_reg |= AP_GNSS_WAKEUP;
		ciu_writel(GNSS_HANDSHAKE, ciu_reg);
		/* add 1ms delay for sync signal to CM3 */
		mdelay(1);
		amipc->dbg_info.rst_bit1_num++;
		spin_unlock_irqrestore(&transfer_lock, flags);
		IPC_LEAVE();
		return AMIPC_RC_AGAIN;
	}

	IPCTRACE("amipc_data_send userEvent 0x%x, data 0x%x\n",
		 user_event, data);

	IPC_LEAVE();
	return AMIPC_RC_OK;
}

static enum amipc_return_code amipc_data_read(enum amipc_events user_event,
						u32 *data1, u32 *data2)
{
	IPC_ENTER();

	if (amipc_rxaddr_inval()) {
		IPC_LEAVE();
		return AMIPC_RC_FAILURE;
	}

	if (data1)
		*data1 = amipc->ipc_rx[E_TO_OFF(user_event)].data1;
	if (data2)
		*data2 = amipc->ipc_rx[E_TO_OFF(user_event)].data2;
	IPC_LEAVE();

	return AMIPC_RC_OK;
}

static enum amipc_return_code amipc_event_bind(u32 user_event,
					       amipc_rec_event_callback cb)
{
	IPC_ENTER();

	if (amipc->amipc_db[E_TO_OFF(user_event)].cb !=
			amipc_default_callback) {
		IPC_LEAVE();
		return AMIPC_EVENT_ALREADY_BIND;
	} else
		amipc->amipc_db[E_TO_OFF(user_event)].cb = cb;

	IPC_LEAVE();
	return AMIPC_RC_OK;
}

static enum amipc_return_code amipc_event_unbind(u32 user_event)
{
	IPC_ENTER();

	amipc->amipc_db[E_TO_OFF(user_event)].cb = amipc_default_callback;

	IPC_LEAVE();
	return AMIPC_RC_OK;
}

static u32 amipc_handle_events(void)
{
	int i;
	u32 events = 0;

	IPC_ENTER();
	if (amipc_rxaddr_inval())
		goto skip_cb;

	for (i = 0; i < AMIPC_EVENT_LAST; i++) {
		if (amipc->ipc_rx[i].ack) {
			/* clients fetch possible data in cb */
			amipc->amipc_db[i].cb(OFF_TO_E(i));
			amipc->ipc_rx[i].ack = 0;
			amipc->amipc_db[i].rx_cnt++;
			events++;
		}
	}

skip_cb:
	IPC_LEAVE();
	return events;
}

static irqreturn_t amipc_interrupt_handler(int irq, void *dev_id)
{
	u32 ciu_reg, pmu_reg, events;
	static int noevt_cnt;

	IPC_ENTER();

	amipc->dbg_info.irq_num++;
	/*
	 * clr interrupt
	 * read three times for clock gate off problem
	 */
	spin_lock(&transfer_lock);
	ciu_reg = ciu_readl(GNSS_HANDSHAKE);
	ciu_reg = ciu_readl(GNSS_HANDSHAKE);
	ciu_reg = ciu_readl(GNSS_HANDSHAKE);
	ciu_reg |= GNSS_IRQ_CLR;
	ciu_writel(GNSS_HANDSHAKE, ciu_reg);
	spin_unlock(&transfer_lock);
	/* clr wakeup */
	pmu_reg = pmu_readl(GNSS_WAKEUP_CTRL);
	if (pmu_reg & GNSS_WAKEUP_STATUS) {
		pmu_reg |= GNSS_WAKEUP_CLR;
		pmu_writel(GNSS_WAKEUP_CTRL, pmu_reg);
	}
	events = amipc_handle_events();
	if (!events) {
		noevt_cnt++;
		/*
		 * CM3 irq will be cleared within 22ns by HW
		 * so 1us delay is enough
		 */
		udelay(1);
		if (10 == noevt_cnt) {
			pr_err("wait GNSS clr int timeout, GNSS irq disabled\n");
			disable_irq_nosync(irq);
		}
	} else
		noevt_cnt = 0;

	IPC_LEAVE();
	return IRQ_HANDLED;
}

static u32 user_callback(u32 event)
{
	u32 data1, data2;
	unsigned long flags;

	IPC_ENTER();
	amipc->user_bakup.event = event;
	amipc_dataread(event, &data1, &data2);
	amipc->user_bakup.data1 = data1;
	amipc->user_bakup.data2 = data2;
	spin_lock_irqsave(&amipc_lock, flags);
	amipc->poll_status = 1;
	spin_unlock_irqrestore(&amipc_lock, flags);
	wake_up_interruptible(&amipc->amipc_wait_q);
	IPC_LEAVE();

	return 0;
}

#define RESERVED_PHY_ADD (0x0b000000)
/* kernel may use 64k page later, but our dma chain always 4k */
#define PAGE_LEN_4K (0x1000)
static long amipc_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct amipc_ioctl_arg amipc_arg;
	int ret = 0;

	IPC_ENTER();
	if (copy_from_user(&amipc_arg,
			   (void __user *)arg, sizeof(struct amipc_ioctl_arg)))
		return -EFAULT;

	switch (cmd) {
	case AMIPC_SET_EVENT:
		amipc_event_set(amipc_arg.event, DEFAULT_TIMEOUT);
		break;
	case AMIPC_GET_EVENT:
		amipc_arg.event = amipc->user_bakup.event;
		break;
	case AMIPC_SEND_DATA:
		amipc_data_send(amipc_arg.event, amipc_arg.data1,
				amipc_arg.data2, DEFAULT_TIMEOUT);
		break;
	case AMIPC_READ_DATA:
		amipc_data_read(amipc_arg.event, &(amipc_arg.data1),
				&(amipc_arg.data2));
		break;
	case AMIPC_BIND_EVENT:
		if (AMIPC_RC_OK != amipc_event_bind(amipc_arg.event,
					user_callback))
			pr_warn("User cb bind failed\n");
		break;
	case AMIPC_UNBIND_EVENT:
		amipc_event_unbind(amipc_arg.event);
		break;
	case AMIPC_IPC_CRL:
		if (AMIPC_ACQ_COMMAND == amipc_arg.event) {
			/* 16M continues memory */
			if (AMIPC_ACQ_GET_MEM == amipc_arg.data1) {
				amipc_arg.data2 = RESERVED_PHY_ADD;
				pr_info("Return phy ACQ mem 0x%x\n",
						amipc_arg.data2);
			} else if (AMIPC_ACQ_REL_MEM == amipc_arg.data1)
				pr_info("Free ACQ mem\n");
			else if (AMIPC_ACQ_CACHE_FLUSH == amipc_arg.data1) {
				dma_sync_single_range_for_cpu(amipc->this_dev, RESERVED_PHY_ADD,
					amipc_arg.data2, amipc_arg.more_msgs, DMA_FROM_DEVICE);
			} else if (AMIPC_ACQ_SET_TO_DMACHAIN == amipc_arg.data1) {
				struct dma_buf *dbuf;
				struct dma_buf_attachment *dba;
				struct sg_table *sgt;
				struct scatterlist *sg;
				int i, j, index = 0;
				/*
				 *	Layout:
				 *	0~205: reserved for ACQ sequential mode
				 *	206~2805: GNSS ACQ, 2600 words
				 *	2806~4095: GPS ACQ, 1290 words
				 */
				index = 206;

				pr_info("set to cm3 dma chain...\n");
				dbuf = dma_buf_get(amipc_arg.data2);
				dba = dma_buf_attach(dbuf, amipc->this_dev);
				sgt = dma_buf_map_attachment(dba, DMA_FROM_DEVICE);
				if (IS_ERR_OR_NULL(sgt)) {
					dma_buf_detach(dbuf, dba);
					dma_buf_put(dbuf);
					pr_err("dma_buf_map_attachment error\n");
					break;
				}
				sg = sgt->sgl;
				for (i = 0; i < sgt->nents; i++, sg = sg_next(sg)) {
#ifdef CONFIG_NEED_SG_DMA_LENGTH
					sg->dma_length = sg->length;
#endif
				}
				sg = sgt->sgl;
				for (i = 0; i < sgt->nents; i++, sg = sg_next(sg)) {
					for (j = 0; j < sg_dma_len(sg) / PAGE_LEN_4K; j++) {
						dmachain_writel(index * 4, sg_dma_address(sg) +
								j * PAGE_LEN_4K);
						index++;
						if (index >= 0x1000)
							goto finish;
					}
				}
finish:
				dma_buf_unmap_attachment(dba, sgt, DMA_FROM_DEVICE);
				dma_buf_detach(dbuf, dba);
				dma_buf_put(dbuf);
				pr_info("set dma chain done\n");
			} else if (AMIPC_ACQ_CLR_DMACHAIN == amipc_arg.data1) {
				int index = 0;
				pr_info("clear cm3 dma chain...\n");
				for (index = 206; index < 0x1000; index++)
					dmachain_writel(index * 4, 0);
				pr_info("clear dma chain done\n");
			}
		}
		break;
	case AMIPC_IPC_MSG_GET:
		if (amipc->msg_buffer.ipc_wptr == amipc->msg_buffer.sdk_rptr) {
			/* no message available */
			memset((void *)&amipc_arg, 0, sizeof(amipc_arg));
		} else {
			amipc_arg.event =
		amipc->msg_buffer.msg[amipc->msg_buffer.sdk_rptr].event;
			amipc_arg.data1 =
		amipc->msg_buffer.msg[amipc->msg_buffer.sdk_rptr].data1;
			amipc_arg.data2 =
		amipc->msg_buffer.msg[amipc->msg_buffer.sdk_rptr].data2;
			amipc->msg_buffer.sdk_rptr =
		(amipc->msg_buffer.sdk_rptr + 1) % MSG_BUFFER_LEN;
			amipc_arg.more_msgs = (amipc->msg_buffer.ipc_wptr +
		MSG_BUFFER_LEN - amipc->msg_buffer.sdk_rptr) % MSG_BUFFER_LEN;
			if (AMIPC_ACQ_COMMAND == amipc_arg.event)
				amipc->acq_info.rd_num++;
		}
		break;
	case AMIPC_TEST_CRL:
		if (TEST_CRL_CLR == amipc_arg.event) {
			memset((void *)amipc->ipc_tx, 0,
			sizeof(struct ipc_event_type) * AMIPC_EVENT_LAST);
			memset((void *)amipc->ipc_rx, 0,
			sizeof(struct ipc_event_type) * AMIPC_EVENT_LAST);
		} else if (TEST_CRL_SET_TX == amipc_arg.event) {
			if (0 == amipc_arg.data1)
				amipc_data_send(AMIPC_SHM_PACKET_NOTIFY,
						1, 1, DEFAULT_TIMEOUT);
			else if (1 == amipc_arg.data1)
				amipc_data_send(AMIPC_RINGBUF_FC,
						2, 2, DEFAULT_TIMEOUT);
			else if (2 == amipc_arg.data1)
				amipc_data_send(AMIPC_ACQ_COMMAND,
						3, 3, DEFAULT_TIMEOUT);
			else if (3 == amipc_arg.data1)
				amipc_data_send(AMIPC_LL_PING,
						4, 4, DEFAULT_TIMEOUT);
		} else if (TEST_CRL_SET_RX == amipc_arg.event) {
			if (4 == amipc_arg.data1) {
				amipc->ipc_rx[E_TO_OFF(AMIPC_LL_PING)].event =
					AMIPC_LL_PING;
				amipc->ipc_rx[E_TO_OFF(AMIPC_LL_PING)].data1
									= 1;
				amipc->ipc_rx[E_TO_OFF(AMIPC_LL_PING)].ack = 1;
			} else if (5 == amipc_arg.data1) {
				amipc->ipc_rx[E_TO_OFF(AMIPC_LL_PING)].event =
					AMIPC_LL_PING;
				amipc->ipc_rx[E_TO_OFF(AMIPC_LL_PING)].data1
									= 2;
				amipc->ipc_rx[E_TO_OFF(AMIPC_LL_PING)].ack = 1;
			}
		} else if (TEST_CRL_GEN_IRQ == amipc_arg.event) {
			amipc_interrupt_handler(0, NULL);
		}
		break;
	default:
		ret = -1;
		break;
	}

	if (copy_to_user((void __user *)arg, &amipc_arg,
			 sizeof(struct amipc_ioctl_arg)))
		return -EFAULT;

	IPC_LEAVE();

	return ret;
}

static unsigned int amipc_poll(struct file *file, poll_table *wait)
{
	unsigned long flags;

	IPC_ENTER();
	poll_wait(file, &amipc->amipc_wait_q, wait);
	IPC_LEAVE();

	if (amipc->poll_status == 0) {
		return 0;
	} else {
		spin_lock_irqsave(&amipc_lock, flags);
		amipc->poll_status = 0;
		spin_unlock_irqrestore(&amipc_lock, flags);
		return POLLIN | POLLRDNORM;
	}
}

static void amipc_vma_open(struct vm_area_struct *vma)
{
	pr_info("AMIPC OPEN 0x%lx -> 0x%lx\n", vma->vm_start,
			vma->vm_pgoff << PAGE_SHIFT);
}

static void amipc_vma_close(struct vm_area_struct *vma)
{
	pr_info("AMIPC CLOSE 0x%lx -> 0x%lx\n", vma->vm_start,
			vma->vm_pgoff << PAGE_SHIFT);
}

/* These are mostly for debug: do nothing useful otherwise */
static struct vm_operations_struct vm_ops = {
	.open = amipc_vma_open,
	.close = amipc_vma_close
};

int amipc_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long size = vma->vm_end - vma->vm_start;
	unsigned long pa = vma->vm_pgoff;

	/* we do not want to have this area swapped out, lock it */
	vma->vm_flags |= (VM_IO | VM_DONTEXPAND | VM_DONTDUMP);

	/* check if addr is normal memory */
	if (pfn_valid(pa)) {
		if (remap_pfn_range(vma, vma->vm_start, pa,/* physical page index */
					size, vma->vm_page_prot)) {
			pr_err("remap page range failed\n");
			return -ENXIO;
		}
	} else {
		if (io_remap_pfn_range(vma, vma->vm_start, pa,/* physical page index */
					size, vma->vm_page_prot)) {
			pr_err("remap page range failed\n");
			return -ENXIO;
		}
	}
	vma->vm_ops = &vm_ops;
	amipc_vma_open(vma);

	return 0;
}

static int shm_block_show(struct seq_file *m, void *unused)
{
	int i;
	rcu_read_lock();
	if (amipc_txaddr_inval())
		seq_puts(m, "tx shm invalid\n");
	else {
		seq_puts(m, "tx shm status:\n");
		seq_puts(m, "event\tdata1\tdata2\tack\tcount\n");
		for (i = 0; i < AMIPC_EVENT_LAST; i++)
			seq_printf(m, "%x\t%x\t%x\t%x\t%d\n",
					amipc->ipc_tx[i].event,
					amipc->ipc_tx[i].data1,
					amipc->ipc_tx[i].data2,
					amipc->ipc_tx[i].ack,
					amipc->amipc_db[i].tx_cnt);
	}
	if (amipc_rxaddr_inval())
		seq_puts(m, "rx shm invalid\n");
	else {
		seq_puts(m, "rx shm status:\n");
		seq_puts(m, "event\tdata1\tdata2\tack\tcount\n");
		for (i = 0; i < AMIPC_EVENT_LAST; i++)
			seq_printf(m, "%x\t%x\t%x\t%x\t%d\n",
					amipc->ipc_rx[i].event,
					amipc->ipc_rx[i].data1,
					amipc->ipc_rx[i].data2,
					amipc->ipc_rx[i].ack,
					amipc->amipc_db[i].rx_cnt);
	}

	rcu_read_unlock();
	return 0;
}

static int shm_block_open(struct inode *inode, struct file *file)
{
	return single_open(file, shm_block_show, NULL);
}

const struct file_operations amipc_shm_fops = {
	.owner = THIS_MODULE,
	.open = shm_block_open,
	.release = single_release,
	.read = seq_read,
	.llseek = seq_lseek,
};

static int pkgstat_block_show(struct seq_file *m, void *unused)
{
	rcu_read_lock();
	seq_printf(m, "msg buffer sdk rptr: %d ipc wprt: %d over flow: %d\n",
			amipc->msg_buffer.sdk_rptr, amipc->msg_buffer.ipc_wptr,
			amipc->msg_buffer.ov_cnt);
	seq_printf(m, "acq wr_num: %d rd_num %d\n", amipc->acq_info.wr_num,
						amipc->acq_info.rd_num);
	seq_printf(m, "reg handshake 0x%x, wakeup 0x%x\n",
			ciu_readl(GNSS_HANDSHAKE), pmu_readl(GNSS_WAKEUP_CTRL));
	seq_printf(m, "hw irq %d\n", amipc->dbg_info.irq_num);
	seq_printf(m, "reset handshake bit1 %d\n", amipc->dbg_info.rst_bit1_num);
	rcu_read_unlock();
	return 0;
}

static int pkgstat_block_open(struct inode *inode, struct file *file)
{
	return single_open(file, pkgstat_block_show, NULL);
}

const struct file_operations amipc_pkgstat_fops = {
	.owner = THIS_MODULE,
	.open = pkgstat_block_open,
	.release = single_release,
	.read = seq_read,
	.llseek = seq_lseek,
};

static u32 dbg_cmd_index;
static int command_block_show(struct seq_file *m, void *unused)
{
	rcu_read_lock();
	if (0 == dbg_cmd_index) {
		seq_puts(m, "echo x > cmd_index firstly, then cat again\n");
		seq_puts(m, "x:0 --> disable debug feature\n");
		seq_puts(m, "x:1 --> init shared memory and debug info\n");
		seq_puts(m, "x:2 --> send ping command\n");
	} else if (1 == dbg_cmd_index) {
		if (amipc->ipc_tx)
			memset((void *)amipc->ipc_tx, 0,
			sizeof(struct ipc_event_type) * AMIPC_EVENT_LAST * 2);
		init_statistic_info();
	} else if (2 == dbg_cmd_index) {
		pr_info("send ping request\n");
		amipc_data_send(AMIPC_LL_PING,
			1, 0, DEFAULT_TIMEOUT);
	}
	rcu_read_unlock();
	return 0;
}

static int command_block_open(struct inode *inode, struct file *file)
{
	return single_open(file, command_block_show, NULL);
}

const struct file_operations amipc_command_fops = {
	.owner = THIS_MODULE,
	.open = command_block_open,
	.release = single_release,
	.read = seq_read,
	.llseek = seq_lseek,
};

static const struct file_operations amipc_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = amipc_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = amipc_ioctl,
#endif
	.poll = amipc_poll,
	.mmap = amipc_mmap,
};

static struct miscdevice amipc_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "amipc",
	.fops = &amipc_fops,
};

#ifdef CONFIG_PM_SLEEP
static int pxa9xx_amipc_suspend(struct device *dev)
{
	if (device_may_wakeup(dev))
		enable_irq_wake(amipc->irq);
	return 0;
}

static int pxa9xx_amipc_resume(struct device *dev)
{
	if (device_may_wakeup(dev))
		disable_irq_wake(amipc->irq);
	return 0;
}

static SIMPLE_DEV_PM_OPS(pxa9xx_amipc_pm_ops,
		pxa9xx_amipc_suspend, pxa9xx_amipc_resume);
#endif

static void __iomem *iomap_register(const char *reg_name)
{
	void __iomem *reg_virt_addr;
	struct device_node *node;

	BUG_ON(!reg_name);
	node = of_find_compatible_node(NULL, NULL, reg_name);
	BUG_ON(!node);
	reg_virt_addr = of_iomap(node, 0);
	BUG_ON(!reg_virt_addr);

	return reg_virt_addr;
}

static int pxa9xx_amipc_probe(struct platform_device *pdev)
{
	int ret, irq, i;
	u32 pmu_reg;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;

	amipc =
	    devm_kzalloc(&pdev->dev, sizeof(struct pxa9xx_amipc), GFP_KERNEL);
	if (!amipc)
		return -ENOMEM;

	amipc->pmu_base = iomap_register("marvell,mmp-pmu-apmu");
	amipc->ciu_base = iomap_register("marvell,mmp-ciu");
	if (NULL == amipc->pmu_base || NULL == amipc->ciu_base) {
		dev_err(&pdev->dev, "get register base error\n");
		return -EINVAL;
	}
	amipc->dmachain_base = of_iomap(np, 0);
	if (!amipc->dmachain_base) {
		dev_err(&pdev->dev, "get dmachain base error\n");
		return -EINVAL;
	}

	for (i = 0; i < AMIPC_EVENT_LAST; i++)
		amipc->amipc_db[i].cb = amipc_default_callback;
	amipc->amipc_db[E_TO_OFF(AMIPC_LL_PING)].cb = amipc_ll_ping_callback;
	amipc->amipc_db[E_TO_OFF(AMIPC_ACQ_COMMAND)].cb = amipc_acq_callback;

	init_waitqueue_head(&amipc->amipc_wait_q);

	platform_set_drvdata(pdev, amipc);
	amipc->this_dev = &pdev->dev;

	irq = platform_get_irq(pdev, 0);
	if (irq >= 0) {
		ret = devm_request_irq(&pdev->dev, irq, amipc_interrupt_handler,
				IRQF_NO_SUSPEND, "PXA9XX-amipc", amipc);
	}
	if (irq < 0 || ret < 0)
		return -ENXIO;
	amipc->irq = irq;

	ret = misc_register(&amipc_miscdev);
	if (ret < 0)
		return ret;

	/* enable GNSS wakeup AP */
	pmu_reg = pmu_readl(GNSS_WAKEUP_CTRL);
	pmu_reg |= GNSS_WAKEUP_EN;
	pmu_writel(GNSS_WAKEUP_CTRL, pmu_reg);

	amipc->dbg_dir = debugfs_create_dir("amipc", NULL);
	if (amipc->dbg_dir) {
		amipc->dbg_shm = debugfs_create_file("sharemem", S_IRUGO,
				amipc->dbg_dir, NULL, &amipc_shm_fops);
		amipc->dbg_pkg = debugfs_create_file("packetstat", S_IRUGO,
				amipc->dbg_dir, NULL, &amipc_pkgstat_fops);
		amipc->dbg_ctrl = debugfs_create_file("command", S_IRUGO,
				amipc->dbg_dir, NULL, &amipc_command_fops);
		debugfs_create_u32("cmd_index", 0644, amipc->dbg_dir,
					&dbg_cmd_index);
	} else
		pr_warn("pxa9xx amipc create debugfs fail\n");

	device_init_wakeup(&pdev->dev, 1);

	pr_info("pxa9xx AM-IPC initialized!\n");

	return 0;
}

static int pxa9xx_amipc_remove(struct platform_device *pdev)
{
	debugfs_remove(amipc->dbg_shm);
	debugfs_remove(amipc->dbg_pkg);
	debugfs_remove(amipc->dbg_dir);
	misc_deregister(&amipc_miscdev);
	return 0;
}

static struct of_device_id pxa9xx_amipc_dt_ids[] = {
	{ .compatible = "marvell,mmp-amipc", },
	{ .compatible = "marvell,pxa-amipc", },
	{}
};

static struct platform_driver pxa9xx_amipc_driver = {
	.driver = {
		   .name = "pxa9xx-amipc",
		   .of_match_table = pxa9xx_amipc_dt_ids,
#ifdef CONFIG_PM_SLEEP
		   .pm = &pxa9xx_amipc_pm_ops,
#endif
		   },
	.probe = pxa9xx_amipc_probe,
	.remove = pxa9xx_amipc_remove
};

static int __init pxa9xx_amipc_init(void)
{
	return platform_driver_register(&pxa9xx_amipc_driver);
}

static void __exit pxa9xx_amipc_exit(void)
{
	platform_driver_unregister(&pxa9xx_amipc_driver);
}

static void __iomem *shm_vmap(phys_addr_t start, size_t size)
{
	struct page **pages;
	phys_addr_t page_start;
	unsigned int i, page_count;
	pgprot_t prot;
	void *vaddr;

	page_start = start - offset_in_page(start);
	page_count = DIV_ROUND_UP(size + offset_in_page(start), PAGE_SIZE);

	prot = pgprot_noncached(PAGE_KERNEL);

	pages = kmalloc(sizeof(struct page *) * page_count, GFP_KERNEL);
	if (!pages) {
		pr_err("%s: Failed to allocate array for %u pages\n", __func__,
				page_count);
		return NULL;
	}

	for (i = 0; i < page_count; i++) {
		phys_addr_t addr = page_start + i * PAGE_SIZE;
		pages[i] = phys_to_page(addr);
	}
	vaddr = vmap(pages, page_count, VM_MAP, prot);
	kfree(pages);

	return (void *__iomem)(vaddr + offset_in_page(start));
}

static void *shm_map(phys_addr_t start, size_t size)
{
	/* check if addr is normal memory */
	if (pfn_valid(__phys_to_pfn(start)))
		return shm_vmap(start, size);
	else
		return ioremap_nocache(start, size);
}

enum amipc_return_code amipc_setbase(phys_addr_t base_addr, int len)
{
	bool ddr_mem;
	void __iomem *ipc_base;
	if (len < sizeof(struct ipc_event_type) * AMIPC_EVENT_LAST * 2) {
		pr_err("share memory too small\n");
		return AMIPC_RC_FAILURE;
	}
	pr_info("amipc: set base address phys: %pa, len:%d\n",
			&base_addr, len);
	/* init debug info */
	init_statistic_info();
	/* check if addr is normal memory */
	if (pfn_valid(__phys_to_pfn(base_addr)))
		ddr_mem = true;
	else
		ddr_mem = false;

	if (!amipc->ipc_tx) {
		ipc_base = shm_map(base_addr, len);
		if (ipc_base) {
			amipc->ipc_tx = (struct ipc_event_type *)ipc_base;
			amipc->ipc_rx = amipc->ipc_tx + AMIPC_EVENT_LAST;
			pr_info("share memory remap suc\n");
		} else {
			pr_err("share memory remap fail\n");
			return AMIPC_RC_FAILURE;
		}
	} else
		pr_info("share memory already map\n");
	if (ddr_mem)
		memset((void *)amipc->ipc_tx, 0, len);
	return AMIPC_RC_OK;
}
EXPORT_SYMBOL(amipc_setbase);

enum amipc_return_code amipc_eventbind(u32 user_event,
				      amipc_rec_event_callback cb)
{
	return amipc_event_bind(user_event, cb);
}
EXPORT_SYMBOL(amipc_eventbind);

enum amipc_return_code amipc_eventunbind(u32 user_event)
{
	return amipc_event_unbind(user_event);
}
EXPORT_SYMBOL(amipc_eventunbind);

enum amipc_return_code amipc_eventset(enum amipc_events user_event,
						int timeout_ms)
{
	return amipc_event_set(user_event, timeout_ms);
}
EXPORT_SYMBOL(amipc_eventset);

enum amipc_return_code amipc_datasend(enum amipc_events user_event,
					u32 data1, u32 data2, int timeout_ms)
{
	return amipc_data_send(user_event, data1, data2, timeout_ms);
}
EXPORT_SYMBOL(amipc_datasend);

enum amipc_return_code amipc_dataread(enum amipc_events user_event,
				u32 *data1, u32 *data2)
{
	return amipc_data_read(user_event, data1, data2);
}
EXPORT_SYMBOL(amipc_dataread);

enum amipc_return_code amipc_dump_debug_info(void)
{
	int i;
	if (amipc_txaddr_inval())
		pr_info("tx shm invalid\n");
	else {
		pr_info("tx shm status:\n");
		pr_info("event\tdata1\tdata2\tack\tcount\n");
		for (i = 0; i < AMIPC_EVENT_LAST; i++)
			pr_info("%x\t%x\t%x\t%x\t%d\n",
					amipc->ipc_tx[i].event,
					amipc->ipc_tx[i].data1,
					amipc->ipc_tx[i].data2,
					amipc->ipc_tx[i].ack,
					amipc->amipc_db[i].tx_cnt);
	}
	if (amipc_rxaddr_inval())
		pr_info("rx shm invalid\n");
	else {
		pr_info("rx shm status:\n");
		pr_info("event\tdata1\tdata2\tack\tcount\n");
		for (i = 0; i < AMIPC_EVENT_LAST; i++)
			pr_info("%x\t%x\t%x\t%x\t%d\n",
					amipc->ipc_rx[i].event,
					amipc->ipc_rx[i].data1,
					amipc->ipc_rx[i].data2,
					amipc->ipc_rx[i].ack,
					amipc->amipc_db[i].rx_cnt);
	}
	pr_info("msg buffer sdk rptr: %d ipc wprt: %d over flow: %d\n",
			amipc->msg_buffer.sdk_rptr, amipc->msg_buffer.ipc_wptr,
			amipc->msg_buffer.ov_cnt);
	pr_info("acq wr_num: %d rd_num %d\n", amipc->acq_info.wr_num,
			amipc->acq_info.rd_num);
	pr_info("reg handshake 0x%x, wakeup 0x%x\n",
			ciu_readl(GNSS_HANDSHAKE), pmu_readl(GNSS_WAKEUP_CTRL));
	pr_info("hw irq %d\n", amipc->dbg_info.irq_num);
	pr_info("reset handshake bit1 %d\n", amipc->dbg_info.rst_bit1_num);
	return AMIPC_RC_OK;
}
EXPORT_SYMBOL(amipc_dump_debug_info);

module_init(pxa9xx_amipc_init);
module_exit(pxa9xx_amipc_exit);
MODULE_AUTHOR("MARVELL");
MODULE_DESCRIPTION("AM-IPC driver");
MODULE_LICENSE("GPL");
