// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include <linux/list.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/kdev_t.h>
#include <linux/slab.h>
#include <linux/skbuff.h>
#include <linux/wait.h>
#include <linux/sched/clock.h> /* local_clock() */
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/timer.h>
#include <linux/fs.h>
#include <linux/netdevice.h>
#include <linux/random.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/clk.h> /* for clk_prepare/un* */
#include <linux/syscore_ops.h>

#include "ccci_core.h"
#include "ccci_modem.h"
#include "ccci_bm.h"
#include "ccci_hif_ccif.h"
#include "modem_secure_base.h"

#if IS_ENABLED(CONFIG_OF)
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif

#define TAG "cif"

static struct md_ccif_ctrl *ccci_ccif_ctrl;

unsigned int devapc_check_flag;
spinlock_t devapc_flag_lock;

int ccif_read32(void *b, unsigned long a)
{
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&devapc_flag_lock, flags);
	ret = ((devapc_check_flag == 1) ?
			ioread32((void __iomem *)((b)+(a))) : 0);
	spin_unlock_irqrestore(&devapc_flag_lock, flags);

	return ret;
}

void ccif_write32(void *b, unsigned long a, unsigned int v)
{
	unsigned long flags;

	spin_lock_irqsave(&devapc_flag_lock, flags);
	if (devapc_check_flag == 1) {
		writel(v, (b) + (a));
		mb(); /* make sure register access in order */
	}
	spin_unlock_irqrestore(&devapc_flag_lock, flags);
}

/* this table maybe can be set array when multi, or else. */
static struct ccci_clk_node ccif_clk_table[] = {
	{ NULL, "infra-ccif-ap"},
	{ NULL, "infra-ccif-md"},
	{ NULL, "infra-ccif1-ap"},
	{ NULL, "infra-ccif1-md"},
	{ NULL, "infra-ccif4-md"},
	{ NULL, "infra-ccif5-md"},
};
#define IS_PASS_SKB(per_md_data, qno)	\
	(!per_md_data->data_usb_bypass && (per_md_data->is_in_ee_dump == 0) \
	 && ((1<<qno) & NET_RX_QUEUE_MASK))

/*ccif share memory setting*/

/* for md gen95/97 chip */
static int rx_queue_buffer_size_up_95[QUEUE_NUM] = { 80 * 1024, 80 * 1024,
	40 * 1024, 80 * 1024, 20 * 1024, 20 * 1024, 64 * 1024, 0 * 1024,
	8 * 1024, 0 * 1024, 0 * 1024, 0 * 1024, 0 * 1024, 0 * 1024,
	0 * 1024, 0 * 1024,
};

static int tx_queue_buffer_size_up_95[QUEUE_NUM] = { 128 * 1024, 40 * 1024,
	8 * 1024, 40 * 1024, 20 * 1024, 20 * 1024, 64 * 1024, 0 * 1024,
	8 * 1024, 0 * 1024, 0 * 1024, 0 * 1024, 0 * 1024, 0 * 1024,
	0 * 1024, 0 * 1024,
};
static int rx_exp_buffer_size_up_95[QUEUE_NUM] = { 12 * 1024, 32 * 1024,
	8 * 1024, 0 * 1024, 0 * 1024, 0 * 1024, 8 * 1024, 0 * 1024,
	0 * 1024, 0 * 1024, 0 * 1024, 0 * 1024, 0 * 1024, 0 * 1024,
	0 * 1024, 0 * 1024,
};

static int tx_exp_buffer_size_up_95[QUEUE_NUM] = { 12 * 1024, 32 * 1024,
	8 * 1024, 0 * 1024, 0 * 1024, 0 * 1024, 8 * 1024, 0 * 1024,
	0 * 1024, 0 * 1024, 0 * 1024, 0 * 1024, 0 * 1024, 0 * 1024,
	0 * 1024, 0 * 1024,
};

/* for md gen98 chip */
static int rx_queue_buffer_size_up_98[QUEUE_NUM] = { 80 * 1024, 80 * 1024,
	40 * 1024, 80 * 1024, 20 * 1024, 20 * 1024, 48 * 1024, 0 * 1024,
	8 * 1024, 16 * 1024, 0 * 1024, 0 * 1024, 0 * 1024, 0 * 1024,
	0 * 1024, 0 * 1024,
};

static int tx_queue_buffer_size_up_98[QUEUE_NUM] = { 128 * 1024, 40 * 1024,
	8 * 1024, 40 * 1024, 20 * 1024, 20 * 1024, 48 * 1024, 0 * 1024,
	8 * 1024, 16 * 1024, 0 * 1024, 0 * 1024, 0 * 1024, 0 * 1024,
	0 * 1024, 0 * 1024,
};

/* for md gen93 chip */
static int rx_queue_buffer_size[QUEUE_NUM] = { 80 * 1024, 80 * 1024,
	40 * 1024, 80 * 1024, 20 * 1024, 20 * 1024, 64 * 1024, 0 * 1024,
};

static int tx_queue_buffer_size[QUEUE_NUM] = { 128 * 1024, 40 * 1024,
	8 * 1024, 40 * 1024, 20 * 1024, 20 * 1024, 64 * 1024, 0 * 1024,
};

static int rx_exp_buffer_size[QUEUE_NUM] = { 12 * 1024, 32 * 1024,
	8 * 1024, 0 * 1024, 0 * 1024, 0 * 1024, 8 * 1024, 0 * 1024,
};

static int tx_exp_buffer_size[QUEUE_NUM] = { 12 * 1024, 32 * 1024,
	8 * 1024, 0 * 1024, 0 * 1024, 0 * 1024, 8 * 1024, 0 * 1024,
};

#ifdef CCCI_KMODULE_ENABLE
/*
 * for debug log:
 * 0 to disable; 1 for print to ram; 2 for print to uart
 * other value to desiable all log
 */
#ifndef CCCI_LOG_LEVEL /* for platform override */
#define CCCI_LOG_LEVEL CCCI_LOG_CRITICAL_UART
#endif
unsigned int ccci_debug_enable = CCCI_LOG_LEVEL;
#endif

#ifdef mtk09077  //mtk09077: remove, i think should not set
void ccci_hif_set_devapc_flag(unsigned int value)
{
	devapc_check_flag = value; /* why change it? i think should not */
}
EXPORT_SYMBOL(ccci_hif_set_devapc_flag); /* why export: i think should not */
#endif

static void md_ccif_reg_dump(unsigned char *title, unsigned char hif_id)
{
	int idx;
	struct md_ccif_ctrl *ccif_ctrl = ccci_ccif_ctrl;

	if (ccci_ccif_ctrl == NULL)
		return;

	CCCI_MEM_LOG_TAG(0, TAG, "%s: %s\n", __func__, title);
	CCCI_MEM_LOG_TAG(0, TAG, "AP_CON(%p)=0x%x\n",
			ccif_ctrl->ccif_ap_base + APCCIF_CON,
			ccif_read32(ccif_ctrl->ccif_ap_base, APCCIF_CON));
	CCCI_MEM_LOG_TAG(0, TAG, "AP_BUSY(%p)=0x%x\n",
			ccif_ctrl->ccif_ap_base + APCCIF_BUSY,
			ccif_read32(ccif_ctrl->ccif_ap_base, APCCIF_BUSY));
	CCCI_MEM_LOG_TAG(0, TAG, "AP_START(%p)=0x%x\n",
			ccif_ctrl->ccif_ap_base + APCCIF_START,
			ccif_read32(ccif_ctrl->ccif_ap_base, APCCIF_START));
	CCCI_MEM_LOG_TAG(0, TAG, "AP_TCHNUM(%p)=0x%x\n",
			ccif_ctrl->ccif_ap_base + APCCIF_TCHNUM,
			ccif_read32(ccif_ctrl->ccif_ap_base, APCCIF_TCHNUM));
	CCCI_MEM_LOG_TAG(0, TAG, "AP_RCHNUM(%p)=0x%x\n",
			ccif_ctrl->ccif_ap_base + APCCIF_RCHNUM,
			ccif_read32(ccif_ctrl->ccif_ap_base, APCCIF_RCHNUM));
	CCCI_MEM_LOG_TAG(0, TAG, "AP_ACK(%p)=0x%x\n",
			ccif_ctrl->ccif_ap_base + APCCIF_ACK,
			ccif_read32(ccif_ctrl->ccif_ap_base, APCCIF_ACK));
	CCCI_MEM_LOG_TAG(0, TAG, "MD_CON(%p)=0x%x\n",
			ccif_ctrl->ccif_md_base + APCCIF_CON,
			ccif_read32(ccif_ctrl->ccif_md_base, APCCIF_CON));
	CCCI_MEM_LOG_TAG(0, TAG, "MD_BUSY(%p)=0x%x\n",
			ccif_ctrl->ccif_md_base + APCCIF_BUSY,
			ccif_read32(ccif_ctrl->ccif_md_base, APCCIF_BUSY));
	CCCI_MEM_LOG_TAG(0, TAG, "MD_START(%p)=0x%x\n",
			ccif_ctrl->ccif_md_base + APCCIF_START,
			ccif_read32(ccif_ctrl->ccif_md_base, APCCIF_START));
	CCCI_MEM_LOG_TAG(0, TAG, "MD_TCHNUM(%p)=0x%x\n",
			ccif_ctrl->ccif_md_base + APCCIF_TCHNUM,
			ccif_read32(ccif_ctrl->ccif_md_base, APCCIF_TCHNUM));
	CCCI_MEM_LOG_TAG(0, TAG, "MD_RCHNUM(%p)=0x%x\n",
			ccif_ctrl->ccif_md_base + APCCIF_RCHNUM,
			ccif_read32(ccif_ctrl->ccif_md_base, APCCIF_RCHNUM));
	CCCI_MEM_LOG_TAG(0, TAG, "MD_ACK(%p)=0x%x\n",
			ccif_ctrl->ccif_md_base + APCCIF_ACK,
			ccif_read32(ccif_ctrl->ccif_md_base, APCCIF_ACK));

	for (idx = 0; idx < ccif_ctrl->sram_size / sizeof(u32); idx += 4) {
		CCCI_MEM_LOG_TAG(0, TAG, "CHDATA(%p): %08X %08X %08X %08X\n",
			ccif_ctrl->ccif_ap_base + APCCIF_CHDATA +
						idx * sizeof(u32),
			ccif_read32(ccif_ctrl->ccif_ap_base + APCCIF_CHDATA,
						(idx + 0) * sizeof(u32)),
			ccif_read32(ccif_ctrl->ccif_ap_base + APCCIF_CHDATA,
						(idx + 1) * sizeof(u32)),
			ccif_read32(ccif_ctrl->ccif_ap_base + APCCIF_CHDATA,
						(idx + 2) * sizeof(u32)),
			ccif_read32(ccif_ctrl->ccif_ap_base + APCCIF_CHDATA,
						(idx + 3) * sizeof(u32)));
	}
}

static void md_ccif_queue_dump(unsigned char hif_id)
{
	int idx;
	unsigned long long ts = 0;
	unsigned long nsec_rem = 0;

	struct md_ccif_ctrl *ccif_ctrl = ccci_ccif_ctrl;

	if (!ccif_ctrl || !ccif_ctrl->rxq[0].ringbuf)
		return;

	CCCI_MEM_LOG_TAG(0, TAG, "Dump ccif_ctrl->channel_id 0x%lx\n",
		ccif_ctrl->channel_id);
	ts = ccif_ctrl->traffic_info.latest_isr_time;
	nsec_rem = do_div(ts, NSEC_PER_SEC);
	CCCI_MEM_LOG_TAG(0, TAG, "Dump CCIF latest isr %5llu.%06lu\n", ts,
		nsec_rem / 1000);
#ifdef DEBUG_FOR_CCB
	CCCI_MEM_LOG_TAG(0, TAG,
		"Dump CCIF latest r_ch: 0x%x\n",
		ccif_ctrl->traffic_info.last_ccif_r_ch);
	ts = ccif_ctrl->traffic_info.latest_ccb_isr_time;
	nsec_rem = do_div(ts, NSEC_PER_SEC);
	CCCI_MEM_LOG_TAG(0, TAG, "Dump CCIF latest ccb_isr %5llu.%06lu\n", ts,
		nsec_rem / 1000);
#endif

	for (idx = 0; idx < QUEUE_NUM; idx++) {
		CCCI_MEM_LOG_TAG(0, TAG, "Q%d TX: w=%d, r=%d, len=%d, %p\n",
		idx, ccif_ctrl->txq[idx].ringbuf->tx_control.write,
		ccif_ctrl->txq[idx].ringbuf->tx_control.read,
		ccif_ctrl->txq[idx].ringbuf->tx_control.length,
		ccif_ctrl->txq[idx].ringbuf);
		CCCI_MEM_LOG_TAG(0, TAG,
		"Q%d RX: w=%d, r=%d, len=%d, isr_cnt=%lld\n",
		idx, ccif_ctrl->rxq[idx].ringbuf->rx_control.write,
		ccif_ctrl->rxq[idx].ringbuf->rx_control.read,
		ccif_ctrl->rxq[idx].ringbuf->rx_control.length,
		ccif_ctrl->isr_cnt[idx]);
		ts = ccif_ctrl->traffic_info.latest_q_rx_isr_time[idx];
		nsec_rem = do_div(ts, NSEC_PER_SEC);
		CCCI_MEM_LOG_TAG(0, TAG,
		"Q%d RX: last isr %5llu.%06lu\n", idx, ts, nsec_rem / 1000);
		ts = ccif_ctrl->traffic_info.latest_q_rx_time[idx];
		nsec_rem = do_div(ts, NSEC_PER_SEC);
		CCCI_MEM_LOG_TAG(0, TAG,
		"Q%d RX: last wq  %5llu.%06lu\n", idx, ts, nsec_rem / 1000);
	}
	ccci_md_dump_log_history(&ccif_ctrl->traffic_info, 1, QUEUE_NUM, QUEUE_NUM);
}

static void md_ccif_dump_queue_history(unsigned char hif_id, unsigned int qno)
{
	struct md_ccif_ctrl *ccif_ctrl = ccci_ccif_ctrl;

	if (!ccif_ctrl || !ccif_ctrl->rxq[qno].ringbuf)
		return;

	CCCI_MEM_LOG_TAG(0, TAG,
		"Dump ccif_ctrl->channel_id 0x%lx\n", ccif_ctrl->channel_id);
	CCCI_MEM_LOG_TAG(0, TAG, "Dump CCIF Queue%d Control\n", qno);
	CCCI_MEM_LOG_TAG(0, TAG, "Q%d TX: w=%d, r=%d, len=%d\n",
		qno, ccif_ctrl->txq[qno].ringbuf->tx_control.write,
		ccif_ctrl->txq[qno].ringbuf->tx_control.read,
		ccif_ctrl->txq[qno].ringbuf->tx_control.length);
	CCCI_MEM_LOG_TAG(0, TAG, "Q%d RX: w=%d, r=%d, len=%d\n",
		qno, ccif_ctrl->rxq[qno].ringbuf->rx_control.write,
		ccif_ctrl->rxq[qno].ringbuf->rx_control.read,
		ccif_ctrl->rxq[qno].ringbuf->rx_control.length);
	ccci_md_dump_log_history(&ccif_ctrl->traffic_info, 0, qno, qno);
}

static int ccif_debug_dump_data(unsigned int hif_id, int *buff, int length)
{
	int i, sram_size;
	unsigned int *dest_buff = NULL;
	struct md_ccif_ctrl *ccif_ctrl = ccci_ccif_ctrl;

	if (!buff || length < 0 || ccif_ctrl == NULL)
		return 0;

	sram_size = ccif_ctrl->sram_size;
	if (length > sram_size)
		return 0;

	dest_buff = (unsigned int *)buff;

	for (i = 0; i < length / sizeof(unsigned int); i++) {
		*(dest_buff + i) = ccif_read32(ccif_ctrl->ccif_ap_base,
			APCCIF_CHDATA + (sram_size - length) +
			i * sizeof(unsigned int));
	}
	CCCI_MEM_LOG_TAG(0, TAG,
		"Dump CCIF SRAM (last %d bytes)\n", length);
	ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP, dest_buff, length);

	return 0;
}

static int md_ccif_op_dump_status(unsigned char hif_id,
	enum MODEM_DUMP_FLAG flag, void *buff, int length)
{
	struct md_ccif_ctrl *ccif_ctrl = ccci_ccif_ctrl;
	unsigned long long *dest_buff;

	if (!ccif_ctrl)
		return -1;

	if (ccif_ctrl->ccif_state == HIFCCIF_STATE_PWROFF
		|| ccif_ctrl->ccif_state == HIFCCIF_STATE_MIN) {
		CCCI_MEM_LOG_TAG(0, TAG, "CCIF not power on, skip dump\n");
		return -2;
	}

	/*runtime data, boot, long time no response EE */
	if (flag & DUMP_FLAG_CCIF) {
		ccif_debug_dump_data(hif_id, buff, length);
		md_ccif_reg_dump("Dump CCIF SRAM\n", hif_id);
		md_ccif_queue_dump(hif_id);
	}
	if (flag & DUMP_FLAG_IRQ_STATUS) {
#if IS_ENABLED(CONFIG_MTK_IRQ_DBG)
		CCCI_NORMAL_LOG(0, TAG,
		"Dump AP CCIF IRQ status\n");
		mt_irq_dump_status(ccif_ctrl->ap_ccif_irq0_id);
		mt_irq_dump_status(ccif_ctrl->ap_ccif_irq1_id);
#else
		CCCI_NORMAL_LOG(0, TAG,
			"Dump AP CCIF IRQ status not support\n");
#endif
	}
	if (flag & DUMP_FLAG_QUEUE_0)
		md_ccif_dump_queue_history(hif_id, 0);
	if (flag & DUMP_FLAG_QUEUE_0_1) {
		md_ccif_dump_queue_history(hif_id, 0);
		md_ccif_dump_queue_history(hif_id, 1);
	}
	if (flag & (DUMP_FLAG_CCIF_REG | DUMP_FLAG_REG))
		md_ccif_reg_dump("Dump CCIF register\n", hif_id);
	if (flag & DUMP_FLAG_GET_TRAFFIC) {
		if (buff && length == 24) { /* u64 * 3 */
			dest_buff = (unsigned long long *)buff;
			dest_buff[0] = ccif_ctrl->traffic_info.latest_isr_time;
			dest_buff[1] = ccif_ctrl->traffic_info.latest_q_rx_isr_time[0];
			dest_buff[2] = ccif_ctrl->traffic_info.latest_q_rx_time[0];
		}
	}
	return 0;
}

static void ccif_queue_status_notify_work(struct work_struct *work)
{
	struct md_ccif_ctrl *md_ctrl =
		container_of(work, struct md_ccif_ctrl, ccif_status_notify_work);

	if (md_ctrl)
		md_ctrl->traffic_info.latest_q_rx_time[AP_MD_CCB_WAKEUP] = local_clock();

	ccci_port_queue_status_notify(CCIF_HIF_ID, AP_MD_CCB_WAKEUP, -1, RX_IRQ);
}

static void md_ccif_traffic_work_func(struct work_struct *work)
{
	struct ccci_hif_traffic *traffic_inf =
		container_of(work, struct ccci_hif_traffic,
			traffic_work_struct);
	struct md_ccif_ctrl *ccif_ctrl =
		container_of(traffic_inf, struct md_ccif_ctrl, traffic_info);
	char *string = NULL;
	char *string_temp = NULL;
	int idx, ret;

	ccci_port_dump_status();
	ccci_channel_dump_packet_counter(&ccif_ctrl->traffic_info);

	string = kmalloc(1024, GFP_ATOMIC);
	string_temp = kmalloc(1024, GFP_ATOMIC);
	if (string == NULL || string_temp == NULL) {
		CCCI_ERROR_LOG(0, TAG, "Fail alloc traffic Mem for isr cnt!\n");
		goto err_exit1;
	}

	ret = snprintf(string, 1024, "total cnt=%lld;",
		ccif_ctrl->traffic_info.isr_cnt);
	if (ret < 0 || ret >= 1024) {
		CCCI_NORMAL_LOG(0, TAG, "string buffer fail %d", ret);
	}
	for (idx = 0; idx < CCIF_CH_NUM; idx++) {
		ret = snprintf(string_temp, 1024, "%srxq%d isr_cnt=%llu;",
			string, idx, ccif_ctrl->isr_cnt[idx]);
		if (ret < 0 || ret >= 1024) {
			CCCI_DEBUG_LOG(0, TAG,
				"string_temp buffer full %d", ret);
		}
		ret = snprintf(string, 1024, "%s", string_temp);
		if (ret < 0 || ret >= 1024) {
			CCCI_NORMAL_LOG(0, TAG, "string buffer full %d", ret);
			break;
		}
	}
	CCCI_NORMAL_LOG(0, TAG, "%s\n", string);

err_exit1:
	kfree(string);
	kfree(string_temp);

	mod_timer(&ccif_ctrl->traffic_monitor,
		jiffies + CCIF_TRAFFIC_MONITOR_INTERVAL * HZ);
}

static void md_ccif_traffic_monitor_func(struct timer_list *t)
{
	struct md_ccif_ctrl *ccif_ctrl = from_timer(ccif_ctrl, t, traffic_monitor);

	schedule_work(&ccif_ctrl->traffic_info.traffic_work_struct);
}

static inline void ccci_md_check_rx_seq_num(
	struct ccci_hif_traffic *traffic_info,
	struct ccci_header *ccci_h, int qno)
{
	u16 channel, seq_num, assert_bit;
	unsigned int param[3] = {0};

	channel = ccci_h->channel;
	seq_num = ccci_h->seq_num;
	assert_bit = ccci_h->assert_bit;

	if (assert_bit && traffic_info->seq_nums[IN][channel] != 0
		&& ((seq_num - traffic_info->seq_nums[IN][channel])
		& 0x7FFF) != 1) {
		CCCI_ERROR_LOG(0, CORE,
			"channel %d seq number out-of-order %d->%d (data: %X, %X)\n",
			channel, seq_num, traffic_info->seq_nums[IN][channel],
			ccci_h->data[0], ccci_h->data[1]);
		md_ccif_op_dump_status(CCIF_HIF_ID, DUMP_FLAG_CCIF, NULL, qno);
		param[0] = channel;
		param[1] = traffic_info->seq_nums[IN][channel];
		param[2] = seq_num;
		ccci_md_force_assert(MD_FORCE_ASSERT_BY_MD_SEQ_ERROR,
			(char *)param, sizeof(param));

	} else {
		traffic_info->seq_nums[IN][channel] = seq_num;
	}
}

//atomic_t lb_dl_q;
/*this function may be called from both workqueue and softirq (NAPI)*/
static unsigned long rx_data_cnt; //maybe can be phase-out: for mdlogger
static unsigned int pkg_num; //maybe can be phase-out: for mdlogger
static int ccif_rx_collect(struct md_ccif_queue *queue, int budget,
	int blocking, int *result)
{

	struct ccci_ringbuf *rx_buf = queue->ringbuf;
	unsigned char *data_ptr;
	int ret = 0, count = 0, pkg_size;
	unsigned long flags;
	int qno = queue->index;
	struct ccci_header *ccci_h = NULL;
	struct ccci_header ccci_hdr;
	struct sk_buff *skb;
	unsigned char from_pool;
	struct md_ccif_ctrl *ccif_ctrl = ccci_ccif_ctrl;
	struct ccci_per_md *per_md_data = ccci_get_per_md_data();

	if (atomic_read(&queue->rx_on_going)) {
		CCCI_DEBUG_LOG(0, TAG, "Q%d rx is on-going(%d)1\n",
			queue->index, atomic_read(&queue->rx_on_going));
		*result = 0;
		return -1;
	}
	atomic_set(&queue->rx_on_going, 1);

	if (per_md_data == NULL)
		return -1;
	if (IS_PASS_SKB(per_md_data, qno))
		from_pool = 0;
	else
		from_pool = 1;

	while (1) {
		ccif_ctrl->traffic_info.latest_q_rx_time[qno] = local_clock();
		spin_lock_irqsave(&queue->rx_lock, flags);
		pkg_size = ccci_ringbuf_readable(rx_buf);
		spin_unlock_irqrestore(&queue->rx_lock, flags);
		if (pkg_size < 0) {
			CCCI_DEBUG_LOG(0, TAG, "Q%d Rx:rbf readable ret=%d\n",
				queue->index, pkg_size);
			ret = 0;
			goto OUT;
		}

		skb = ccci_alloc_skb(pkg_size, from_pool, blocking);

		if (skb == NULL) {
			CCCI_ERROR_LOG(0, TAG,
				"Q%d Rx:ccci_alloc_skb pkg_size=%d failed,count=%d\n",
				queue->index, pkg_size, count);
			ret = -ENOMEM;
			goto OUT;
		}

		data_ptr = (unsigned char *)skb_put(skb, pkg_size);
		/*copy data into skb */
		spin_lock_irqsave(&queue->rx_lock, flags);
		ret = ccci_ringbuf_read(rx_buf, data_ptr, pkg_size);
		spin_unlock_irqrestore(&queue->rx_lock, flags);
		if (unlikely(ret < 0)) {
			ccci_free_skb(skb);
			goto OUT;
		}
		ccci_h = (struct ccci_header *)skb->data;

#ifdef CONFIG_MTK_SRIL_SUPPORT
		if (ccci_h->channel == CCCI_RIL_IPC0_RX
			|| ccci_h->channel == CCCI_RIL_IPC1_RX) {
			print_hex_dump(KERN_INFO, "1. mif: RX: ",
					DUMP_PREFIX_NONE, 32, 1, skb->data, 32, 0);
		}
#endif
		if (test_and_clear_bit(queue->index, &ccif_ctrl->wakeup_ch)) {
			CCCI_NOTICE_LOG(0, TAG, "CCIF_MD wakeup source:(%d/%d/%x)(%u) %s\n",
				queue->index, ccci_h->channel,
				ccci_h->reserved, ccif_ctrl->wakeup_count,
				ccci_port_get_dev_name(ccci_h->channel));
			if (ccci_h->channel == CCCI_FS_RX)
				ccci_h->data[0] |= CCCI_FS_AP_CCCI_WAKEUP;
			else if (ccci_h->channel >= CCCI_MIPC0_CHANNEL_RX &&
				ccci_h->channel <= CCCI_MIPC9_CHANNEL_RX) {
				/*
				 * MIPC message data struct:
				 * typedef struct {
				 * u32 magic; u16 padding[2]; u8 msg_sim_ps_id;
				 * u8 msg_flag;
				 * u16 msg_id;  //log to show,offset is 10bytes
				 * u16 msg_txid; u16 msg_len;} mipc_msg_hdr_t;
				 */
				CCCI_NOTICE_LOG(0, TAG,
					"%s:CCCI_MIPC ch%d wakeup,msg_id=0x%x\n",
					__func__, ccci_h->channel,
					*(unsigned short *)((unsigned char *)skb->data +
						sizeof(struct ccci_header) + 10));
			}
		}
		//if (ccci_h->channel == CCCI_C2K_LB_DL)
		//	atomic_set(&lb_dl_q, queue->index);

		ccci_hdr = *ccci_h;

		ret = ccci_port_recv_skb(queue->hif_id, skb, NORMAL_DATA);

		if (ret >= 0 || ret == -CCCI_ERR_DROP_PACKET) {
			count++;
			ccci_md_check_rx_seq_num(&ccif_ctrl->traffic_info,
				&ccci_hdr, queue->index);
			ccci_md_add_log_history(&ccif_ctrl->traffic_info, IN,
				(int)queue->index, &ccci_hdr,
				(ret >= 0 ? 0 : 1));
			ccci_channel_update_packet_counter(
				ccif_ctrl->traffic_info.logic_ch_pkt_cnt,
				&ccci_hdr);

			if (queue->debug_id) {
				CCCI_NORMAL_LOG(0, TAG,
					"Q%d Rx recv req ret=%d\n",
					queue->index, ret);
				queue->debug_id = 0;
			}
			spin_lock_irqsave(&queue->rx_lock, flags);
			ccci_ringbuf_move_rpointer(rx_buf, pkg_size);
			spin_unlock_irqrestore(&queue->rx_lock, flags);

			if (ccci_hdr.channel == CCCI_MD_LOG_RX) {
				rx_data_cnt += pkg_size - 16;
				pkg_num++;
				CCCI_DEBUG_LOG(0, TAG,
					"Q%d Rx buf read=%d, write=%d, pkg_size=%d, log_cnt=%ld, pkg_num=%d\n",
					queue->index, rx_buf->rx_control.read,
					rx_buf->rx_control.write, pkg_size,
					rx_data_cnt, pkg_num);
			}
			ret = 0;
		} else {
			/*leave package into share memory,
			 * and waiting ccci to receive
			 */
			ccci_free_skb(skb);

			if (queue->debug_id == 0) {
				queue->debug_id = 1;
				CCCI_ERROR_LOG(0, TAG,
					"Q%d Rx err, ret = 0x%x\n",
					queue->index, ret);
			}

			goto OUT;
		}
		if (count > budget) {
			CCCI_DEBUG_LOG(0, TAG,
				"Q%d count > budget, exit now\n",
				queue->index);
			goto OUT;
		}
	}

 OUT:
	atomic_set(&queue->rx_on_going, 0);
	*result = count;
	CCCI_DEBUG_LOG(0, TAG, "Q%d rx %d pkg,ret=%d\n",
		queue->index, count, ret);
	spin_lock_irqsave(&queue->rx_lock, flags);
	if (ret != -CCCI_ERR_PORT_RX_FULL
		&& ret != -EAGAIN) {
		pkg_size = ccci_ringbuf_readable(rx_buf);
		if (pkg_size > 0)
			ret = -EAGAIN;
	}
	spin_unlock_irqrestore(&queue->rx_lock, flags);
	return ret;
}

static void ccif_rx_work(struct work_struct *work)
{
	int result = 0, ret = 0;
	struct md_ccif_queue *queue =
		container_of(work, struct md_ccif_queue, qwork);

	ret = ccif_rx_collect(queue, queue->budget, 1, &result);
	if (ret == -EAGAIN) {
		CCCI_DEBUG_LOG(0, TAG, "Q%u queue again\n", queue->index);
		queue_work(queue->worker, &queue->qwork);
	} else {
		ccci_port_queue_status_notify(queue->hif_id,
			queue->index, IN, RX_FLUSH);
	}
}

/*exception and SRAM channel handler*/
static void md_ccif_handle_exception(struct md_ccif_ctrl *ccif_ctrl)
{
	CCCI_DEBUG_LOG(0, TAG,
		"ccif_irq_tasklet1: ch %lx\n", ccif_ctrl->channel_id);
	if (ccif_ctrl->channel_id & CCIF_HW_CH_RX_RESERVED) {
		CCCI_ERROR_LOG(0, TAG,
			"Interrupt from reserved ccif ch(%ld)\n",
			ccif_ctrl->channel_id);
		ccif_ctrl->channel_id &= ~CCIF_HW_CH_RX_RESERVED;
		CCCI_ERROR_LOG(0, TAG, "After cleared reserved ccif ch(%ld)\n",
			ccif_ctrl->channel_id);
	}
	if (ccif_ctrl->channel_id & (1 << D2H_EXCEPTION_INIT)) {
		clear_bit(D2H_EXCEPTION_INIT, &ccif_ctrl->channel_id);
		md_fsm_exp_info((1 << D2H_EXCEPTION_INIT));
	}

	if (ccif_ctrl->channel_id & (1 << AP_MD_SEQ_ERROR)) {
		clear_bit(AP_MD_SEQ_ERROR, &ccif_ctrl->channel_id);
		CCCI_ERROR_LOG(0, TAG, "MD check seq fail\n");
		md_ccif_op_dump_status(CCIF_HIF_ID, DUMP_FLAG_CCIF, NULL, 0);
	}
	if (ccif_ctrl->channel_id & (1 << (D2H_SRAM))) {
		clear_bit(D2H_SRAM, &ccif_ctrl->channel_id);
		schedule_work(&ccif_ctrl->ccif_sram_work);
	}
	CCCI_DEBUG_LOG(0, TAG, "ccif_irq_tasklet2: ch %ld\n",
		ccif_ctrl->channel_id);
}

/* ccif_ctrl->ccif_sram_work */
static void md_ccif_sram_rx_work(struct work_struct *work)
{
	struct md_ccif_ctrl *ccif_ctrl =
		container_of(work, struct md_ccif_ctrl, ccif_sram_work);
	struct ccci_header *dl_pkg =
		&ccif_ctrl->ccif_sram_layout->dl_header;
	struct ccci_header *ccci_h;
	struct ccci_header ccci_hdr;
	struct sk_buff *skb = NULL;
	int pkg_size, ret = 0, retry_cnt = 0;

	u32 i = 0;
	u8 *md_feature = (u8 *)(&ccif_ctrl->ccif_sram_layout->md_rt_data);

	CCCI_NORMAL_LOG(0, TAG,
		"%s:dk_pkg=%p, md_featrue=%p\n", __func__,
		dl_pkg, md_feature);
	pkg_size =
		sizeof(struct ccci_header) + sizeof(struct md_query_ap_feature);

	skb = ccci_alloc_skb(pkg_size, 1, 1);
	if (skb == NULL) {
		CCCI_ERROR_LOG(0, TAG,
			"%s: alloc skb size=%d, failed\n", __func__,
			pkg_size);
		return;
	}
	skb_put(skb, pkg_size);
	ccci_h = (struct ccci_header *)skb->data;
	ccci_h->data[0] = ccif_read32(&dl_pkg->data[0], 0);
	ccci_h->data[1] = ccif_read32(&dl_pkg->data[1], 0);
	/*ccci_h->channel = ccif_read32(&dl_pkg->channel,0); */
	*(((u32 *) ccci_h) + 2) = ccif_read32((((u32 *) dl_pkg) + 2), 0);
	ccci_h->reserved = ccif_read32(&dl_pkg->reserved, 0);

	/*warning: make sure struct md_query_ap_feature is 4 bypes align */
	while (i < sizeof(struct md_query_ap_feature)) {
		*((u32 *) (skb->data + sizeof(struct ccci_header) + i))
		= ccif_read32(md_feature, i);
		i += 4;
	}

	if (test_and_clear_bit((D2H_SRAM), &ccif_ctrl->wakeup_ch)) {
		CCCI_NOTICE_LOG(0, TAG,
			"CCIF_MD wakeup source:(SRX_IDX/%d)(%u), HS1\n",
			ccci_h->channel, ccif_ctrl->wakeup_count);
	}
	ccci_hdr = *ccci_h;
	ccci_md_check_rx_seq_num(&ccif_ctrl->traffic_info, &ccci_hdr, 0);

 RETRY:
	ret = ccci_port_recv_skb(ccif_ctrl->hif_id, skb,
			NORMAL_DATA);
	CCCI_DEBUG_LOG(0, TAG, "Rx msg %x %x %x %x ret=%d\n",
		ccci_hdr.data[0], ccci_hdr.data[1],
		*(((u32 *)&ccci_hdr) + 2), ccci_hdr.reserved, ret);
	if (ret >= 0 || ret == -CCCI_ERR_DROP_PACKET) {
		CCCI_NORMAL_LOG(0, TAG,
			"%s:ccci_port_recv_skb ret=%d\n", __func__,
			ret);
	} else {
		if (retry_cnt < 20) {
			CCCI_ERROR_LOG(0, TAG,
				"%s:ccci_port_recv_skb ret=%d, retry=%d\n",
				__func__, ret, retry_cnt);
			udelay(5);
			retry_cnt++;
			goto RETRY;
		}
		ccci_free_skb(skb);
		CCCI_NORMAL_LOG(0, TAG,
		"%s:ccci_port_recv_skb ret=%d\n", __func__, ret);
	}
}

static void md_ccif_launch_work(struct md_ccif_ctrl *ccif_ctrl)
{
	int i;

	if (ccif_ctrl->channel_id & (1 << (D2H_SRAM))) {
		clear_bit(D2H_SRAM, &ccif_ctrl->channel_id);
		schedule_work(&ccif_ctrl->ccif_sram_work);
	}

	if (ccif_ctrl->channel_id & (1 << AP_MD_CCB_WAKEUP)) {
		clear_bit(AP_MD_CCB_WAKEUP, &ccif_ctrl->channel_id);

#ifdef DEBUG_FOR_CCB
		/* CCB count here for channel_id of ccb is clear in this if */
		ccif_ctrl->traffic_info.latest_q_rx_isr_time[AP_MD_CCB_WAKEUP] = local_clock();
		ccif_ctrl->traffic_info.latest_ccb_isr_time = local_clock();
#endif
		schedule_work(&ccif_ctrl->ccif_status_notify_work);
	}
	for (i = 0; i < QUEUE_NUM; i++) {
		if (ccif_ctrl->channel_id & (1 << (i + D2H_RINGQ0))) {
			ccif_ctrl->traffic_info.latest_q_rx_isr_time[i] =
				local_clock();
			clear_bit(i + D2H_RINGQ0, &ccif_ctrl->channel_id);
			if (atomic_read(&ccif_ctrl->rxq[i].rx_on_going)) {
				CCCI_DEBUG_LOG(0, TAG,
					"Q%d rx is on-going(%d)2\n",
					ccif_ctrl->rxq[i].index,
					atomic_read(
					&ccif_ctrl->rxq[i].rx_on_going));
				continue;
			}
			queue_work(ccif_ctrl->rxq[i].worker,
				&ccif_ctrl->rxq[i].qwork);
		}
	}
}

/* CCIF interrupt 1 handler: mainly for communication notification */
static irqreturn_t md_ccif_isr0(int irq, void *data)
{
	struct md_ccif_ctrl *ccif_ctrl = (struct md_ccif_ctrl *)data;
	unsigned int ch_id, i;

	/*must ack first, otherwise IRQ will rush in */
	ch_id = ccif_read32(ccif_ctrl->ccif_ap_base, APCCIF_RCHNUM);

	for (i = 0; i < CCIF_CH_NUM; i++)
		if (ch_id & 0x1 << i) {
			set_bit(i, &ccif_ctrl->channel_id);
			ccif_ctrl->isr_cnt[i]++;
		}
	/* for 91/92, HIF CCIF is for C2K, only 16 CH;
	 * for 93, only lower 16 CH is for data
	 */
	ccif_write32(ccif_ctrl->ccif_ap_base, APCCIF_ACK, ch_id & 0xFFFF);

	/* igore exception queue */
	if (ch_id >> RINGQ_BASE) {
		ccif_ctrl->traffic_info.isr_cnt++;
		ccif_ctrl->traffic_info.latest_isr_time = local_clock();
#ifdef DEBUG_FOR_CCB
		/* infactly, maybe ccif_ctrl->channel_id is, which maybe cleared */
		ccif_ctrl->traffic_info.last_ccif_r_ch = ch_id;
#endif
		md_ccif_launch_work(ccif_ctrl);
	} else // before 6293, maybe can be phased-out
		md_ccif_handle_exception(ccif_ctrl);

	return IRQ_HANDLED;
}

/* RX queue work */
static int md_ccif_op_give_more(unsigned char hif_id, unsigned char qno)
{
	struct md_ccif_ctrl *ccif_ctrl = ccci_ccif_ctrl;

	if (!ccif_ctrl)
		return -CCCI_ERR_HIF_NOT_POWER_ON;

	if (qno == 0xFF)
		return -CCCI_ERR_INVALID_QUEUE_INDEX;
	queue_work(ccif_ctrl->rxq[qno].worker, &ccif_ctrl->rxq[qno].qwork);
	return 0;
}

/* TX buffer remain space */
static int md_ccif_op_write_room(unsigned char hif_id, unsigned char qno)
{
	struct md_ccif_ctrl *ccif_ctrl = ccci_ccif_ctrl;

	if (qno >= QUEUE_NUM)
		return -CCCI_ERR_INVALID_QUEUE_INDEX;
	return ccci_ringbuf_writeable(ccif_ctrl->txq[qno].ringbuf, 0);
}

/*return ap_rt_data pointer after filling header*/
static void *ccif_hif_fill_rt_header(unsigned char hif_id, int packet_size,
	unsigned int tx_ch, unsigned int txqno)
{
	struct ccci_header *ccci_h;
	struct ccci_header ccci_h_bk;
	struct md_ccif_ctrl *ccif_ctrl = ccci_ccif_ctrl;

	ccci_h =
		(struct ccci_header *)&ccif_ctrl->ccif_sram_layout->up_header;
	/*header */
	ccif_write32(&ccci_h->data[0], 0, 0x00);
	ccif_write32(&ccci_h->data[1], 0, packet_size);
	ccif_write32(&ccci_h->reserved, 0, MD_INIT_CHK_ID);
	/*ccif_write32(&ccci_h->channel,0,CCCI_CONTROL_TX); */
	/*as Runtime data always be the first packet
	 * we send on control channel
	 */
	ccif_write32((u32 *)ccci_h + 2, 0, tx_ch);
	/*ccci_header need backup for log history*/
	ccci_h_bk.data[0] = ccif_read32(&ccci_h->data[0], 0);
	ccci_h_bk.data[1] = ccif_read32(&ccci_h->data[1], 0);
	*((u32 *)&ccci_h_bk + 2) = ccif_read32((u32 *) ccci_h + 2, 0);
	ccci_h_bk.reserved = ccif_read32(&ccci_h->reserved, 0);
	ccci_md_add_log_history(&ccif_ctrl->traffic_info, OUT,
		(int)txqno, &ccci_h_bk, 0);

	return (void *)&ccif_ctrl->ccif_sram_layout->ap_rt_data;
}

/* TX: set to HW */
static int md_ccif_send(unsigned char hif_id, int channel_id)
{
	int busy = 0;
	struct md_ccif_ctrl *ccif_ctrl = ccci_ccif_ctrl;

	busy = ccif_read32(ccif_ctrl->ccif_ap_base, APCCIF_BUSY);
	if (busy & (1 << channel_id)) {
		CCCI_REPEAT_LOG(0, TAG, "CCIF channel %d busy\n", channel_id);
	} else {
		ccif_write32(ccif_ctrl->ccif_ap_base,
			APCCIF_BUSY, 1 << channel_id);
		ccif_write32(ccif_ctrl->ccif_ap_base,
			APCCIF_TCHNUM, channel_id);
		CCCI_REPEAT_LOG(0, TAG, "CCIF start=0x%x\n",
			ccif_read32(ccif_ctrl->ccif_ap_base, APCCIF_START));
	}
	return 0;
}

static void md_ccif_switch_ringbuf(unsigned char hif_id, enum ringbuf_id rb_id)
{
	int i;
	struct md_ccif_ctrl *ccif_ctrl = ccci_ccif_ctrl;
	unsigned long flags;

	CCCI_NORMAL_LOG(0, TAG, "%s to %d\n", __func__, rb_id);
	for (i = 0; i < QUEUE_NUM; ++i) {
		spin_lock_irqsave(&ccif_ctrl->rxq[i].rx_lock, flags);
		ccif_ctrl->rxq[i].ringbuf = ccif_ctrl->rxq[i].ringbuf_bak[rb_id];
		spin_unlock_irqrestore(&ccif_ctrl->rxq[i].rx_lock, flags);

		spin_lock_irqsave(&ccif_ctrl->txq[i].tx_lock, flags);
		ccif_ctrl->txq[i].ringbuf = ccif_ctrl->txq[i].ringbuf_bak[rb_id];
		spin_unlock_irqrestore(&ccif_ctrl->txq[i].tx_lock, flags);
	}
}

static void md_ccif_reset_queue(unsigned char hif_id, unsigned char for_start)
{
	int i;
	struct md_ccif_ctrl *ccif_ctrl = ccci_ccif_ctrl;
	unsigned long flags;

	CCCI_NORMAL_LOG(0, TAG, "%s: All %d queue\n", __func__, QUEUE_NUM);
	for (i = 0; i < QUEUE_NUM; ++i) {
		flush_work(&ccif_ctrl->rxq[i].qwork);
		spin_lock_irqsave(&ccif_ctrl->rxq[i].rx_lock, flags);
		ccci_ringbuf_reset(ccif_ctrl->rxq[i].ringbuf, 0); /* RX/IN/MD2AP */
		spin_unlock_irqrestore(&ccif_ctrl->rxq[i].rx_lock, flags);
		//ccif_ctrl->rxq[i].resume_cnt = 0;

		spin_lock_irqsave(&ccif_ctrl->txq[i].tx_lock, flags);
		ccci_ringbuf_reset(ccif_ctrl->txq[i].ringbuf, 1); /* TX/OUT/AP2MD */
		spin_unlock_irqrestore(&ccif_ctrl->txq[i].tx_lock, flags);

		//ccif_wake_up_tx_queue(ccif_ctrl, i);
		//ccif_ctrl->txq[i].wakeup = 0;
	}
	//ccif_reset_busy_queue(ccif_ctrl);
	/* for debug/statistical data print: traffic data */
	ccci_reset_seq_num(&ccif_ctrl->traffic_info);
	memset(ccif_ctrl->isr_cnt, 0, sizeof(ccif_ctrl->isr_cnt));
	if (for_start)
		mod_timer(&ccif_ctrl->traffic_monitor,
			jiffies + CCIF_TRAFFIC_MONITOR_INTERVAL * HZ);
	else
		del_timer(&ccif_ctrl->traffic_monitor);
}

static int md_ccif_send_data(unsigned char hif_id, int channel_id)
{
	switch (channel_id) {
	case H2D_EXCEPTION_CLEARQ_ACK:
		md_ccif_switch_ringbuf(CCIF_HIF_ID, RB_EXP);
		md_ccif_reset_queue(CCIF_HIF_ID, 0); /* use exception Q */
		break;
	case H2D_SRAM:
		break;
	default:
		break;
	}
	return md_ccif_send(hif_id, channel_id);
}

static int md_ccif_op_send_skb(unsigned char hif_id, int qno,
	struct sk_buff *skb, int skb_from_pool, int blocking)
{
	struct md_ccif_ctrl *ccif_ctrl = ccci_ccif_ctrl;
	struct md_ccif_queue *queue = NULL;
	unsigned long flags;
	struct ccci_header *ccci_h = NULL;
	struct ccci_per_md *per_md_data = ccci_get_per_md_data();
	int md_state, ret;

	if (qno == 0xFF)
		return -CCCI_ERR_INVALID_QUEUE_INDEX;

	if (per_md_data == NULL)
		return -1;

	queue = &ccif_ctrl->txq[qno];

	ccci_h = (struct ccci_header *)skb->data;

	if (ccif_ctrl->plat_val.md_gen < 6295) {
		if (qno > 7) {
			CCCI_ERROR_LOG(0, TAG, "qno error (%d)\n", qno);
			return -CCCI_ERR_INVALID_QUEUE_INDEX;
		}
	}
	queue = &ccif_ctrl->txq[qno];
 retry:
	/* we use irqsave as network require a lock in softirq,
	 * cause a potential deadlock
	 */
	spin_lock_irqsave(&queue->tx_lock, flags);

	if (ccci_ringbuf_writeable(queue->ringbuf, skb->len) > 0) {

		ccci_md_inc_tx_seq_num(&ccif_ctrl->traffic_info, ccci_h);

		ccci_channel_update_packet_counter(
			ccif_ctrl->traffic_info.logic_ch_pkt_cnt, ccci_h);

		/* copy skb to ringbuf */
		ret = ccci_ringbuf_write(queue->ringbuf, skb->data, skb->len);
		if (ret != skb->len)
			CCCI_ERROR_LOG(0, TAG,
				"TX:ERR rbf write: ret(%d)!=req(%d)\n",
				ret, skb->len);
#ifdef CONFIG_MTK_SRIL_SUPPORT
		if (ccci_h->channel == CCCI_RIL_IPC0_TX
			|| ccci_h->channel == CCCI_RIL_IPC1_TX) {
			print_hex_dump(KERN_INFO, "1. mif: TX: ",
					DUMP_PREFIX_NONE, 32, 1, skb->data, 32, 0);
		}
#endif
		ccci_md_add_log_history(&ccif_ctrl->traffic_info, OUT,
			(int)queue->index, ccci_h, 0);
		/* free request */
		ccci_free_skb(skb);

		/* send ccif request */
		md_ccif_send(hif_id, queue->ccif_ch);
		spin_unlock_irqrestore(&queue->tx_lock, flags);
	} else {
#ifdef CONFIG_MTK_SRIL_SUPPORT
		if (ccci_h->channel == CCCI_RIL_IPC0_TX
			|| ccci_h->channel == CCCI_RIL_IPC1_TX) {
			print_hex_dump(KERN_INFO, "2. mif: TX: ",
					DUMP_PREFIX_NONE, 32, 1, skb->data, 32, 0);
		}
#endif
		spin_unlock_irqrestore(&queue->tx_lock, flags);

		if (blocking) {
			md_state = ccci_fsm_get_md_state();
			if (md_state == EXCEPTION
					&& ccci_h->channel != CCCI_MD_LOG_TX
					&& ccci_h->channel != CCCI_UART1_TX
					&& ccci_h->channel != CCCI_FS_TX) {
				CCCI_REPEAT_LOG(0, TAG,
					"tx retry break for EE for Q%d, ch %d\n",
					queue->index, ccci_h->channel);
				return -ETXTBSY;
			} else if (md_state == GATED) {
				CCCI_REPEAT_LOG(0, TAG,
					"tx retry break for Gated for Q%d, ch %d\n",
					queue->index, ccci_h->channel);
				return -ETXTBSY;
			}
			CCCI_REPEAT_LOG(0, TAG,
				"tx retry for Q%d, ch %d\n",
				queue->index, ccci_h->channel);
				goto retry;
		} else {
			if (per_md_data->data_usb_bypass)
				return -ENOMEM;
			else
				return -EBUSY;
			CCCI_DEBUG_LOG(0, TAG, "tx fail on q%d\n", qno);
		}
	}
	return 0;
}

static int md_ccif_stop_queue(unsigned char hif_id,
	unsigned char qno, enum DIRECTION dir)
{
	return 0;
}

static int md_ccif_start_queue(unsigned char hif_id,
	unsigned char qno, enum DIRECTION dir)
{
	struct md_ccif_ctrl *ccif_ctrl = ccci_ccif_ctrl;
	struct md_ccif_queue *queue = NULL;
	unsigned long flags;

	if (dir == OUT
		&& likely(ccci_md_get_cap_by_id()
		& MODEM_CAP_TXBUSY_STOP
		&& (qno < QUEUE_NUM))) {
		queue = &ccif_ctrl->txq[qno];
		spin_lock_irqsave(&queue->tx_lock, flags);
		//ccif_wake_up_tx_queue(ccif_ctrl, qno);
		/*special for net queue*/
		ccci_hif_queue_status_notify(hif_id, qno, OUT, TX_IRQ);
		spin_unlock_irqrestore(&queue->tx_lock, flags);
	}
	return 0;
}

#define PCCIF_BUSY (0x4)
#define PCCIF_TCHNUM (0xC)
#define PCCIF_ACK (0x14)
#define PCCIF_CHDATA (0x100)
#define PCCIF_SRAM_SIZE (512)
void ccci_reset_ccif_hw(int ccif_id, void __iomem *baseA,
	void __iomem *baseB, struct md_ccif_ctrl *ccif_ctrl)
{
	int i;
	struct ccci_smem_region *region;
	int reset_bit = -1;

	CCCI_NORMAL_LOG(0, TAG,
		"%s, ccif_hw_reset_ver = %d, ccif_hw_reset_bit = %d\n",
		__func__, ccif_ctrl->ccif_hw_reset_ver,
		ccif_ctrl->ccif_hw_reset_bit);

	if (ccif_ctrl->ccif_hw_reset_ver == 1) {
		reset_bit = ccif_ctrl->ccif_hw_reset_bit;

		/* set ccif0 reset bit */
		ccci_write32(ccif_ctrl->infracfg_base, 0xF50, 1 << reset_bit);

		/* set ccif0 reset bit */
		ccci_write32(ccif_ctrl->infracfg_base, 0xF54, 1 << reset_bit);
	} else {
		switch (ccif_id) {
		case AP_MD1_CCIF:
			reset_bit = 8;
			break;
		}

		if (reset_bit == -1)
			return;

		/*
		 *this reset bit will clear
		 *CCIF's busy/wch/irq, but not SRAM
		 */
		/*set reset bit*/
		regmap_write(ccif_ctrl->plat_val.infra_ao_base,
			0x150, 1 << reset_bit);
		/*clear reset bit*/
		regmap_write(ccif_ctrl->plat_val.infra_ao_base,
			0x154, 1 << reset_bit);
	}

	/* clear SRAM */
	for (i = 0; i < PCCIF_SRAM_SIZE/sizeof(unsigned int); i++) {
		ccif_write32(baseA, PCCIF_CHDATA+i*sizeof(unsigned int), 0);
		ccif_write32(baseB, PCCIF_CHDATA+i*sizeof(unsigned int), 0);
	}

	/* extend from 36bytes to 72bytes in CCIF SRAM */
	/* 0~60bytes for bootup trace,
	 *last 12bytes for magic pattern,smem address and size
	 */
	region = ccci_md_get_smem_by_user_id(SMEM_USER_RAW_MDSS_DBG);
	if (region == NULL)
		return;
	ccif_write32(baseA,
		PCCIF_CHDATA + PCCIF_SRAM_SIZE - 3 * sizeof(u32),
		0x7274626E);
	ccif_write32(baseA,
		PCCIF_CHDATA + PCCIF_SRAM_SIZE - 2 * sizeof(u32),
		region->base_md_view_phy);
	ccif_write32(baseA,
		PCCIF_CHDATA + PCCIF_SRAM_SIZE - sizeof(u32),
		region->size);

	CCCI_NORMAL_LOG(-1, TAG,
		"sram info: parttern: 0x%08X, addr: 0x%08X, size: 0x%08X\n",
		ccif_read32(baseA, PCCIF_CHDATA + PCCIF_SRAM_SIZE - 3 * sizeof(u32)),
		ccif_read32(baseA, PCCIF_CHDATA + PCCIF_SRAM_SIZE - 2 * sizeof(u32)),
		ccif_read32(baseA, PCCIF_CHDATA + PCCIF_SRAM_SIZE - 1 * sizeof(u32)));
}
//EXPORT_SYMBOL(ccci_reset_ccif_hw);

static int ccif_debug(unsigned char hif_id,
		enum ccci_hif_debug_flg flag, int *para)
{
	struct md_ccif_ctrl *ccif_ctrl = ccci_ccif_ctrl;
	int ret = -1;

	switch (flag) {
	case CCCI_HIF_DEBUG_SET_WAKEUP:
		ccif_ctrl->wakeup_ch =
			ccif_read32(ccif_ctrl->ccif_ap_base, APCCIF_RCHNUM);
		CCCI_NORMAL_LOG(-1, TAG,
			"CCIF0 Wake up old path: channel_id == 0x%lx\n",
			ccif_ctrl->wakeup_ch);
		ret = 0;
		break;
	case CCCI_HIF_DEBUG_RESET:
		ccci_reset_ccif_hw(AP_MD1_CCIF, ccif_ctrl->ccif_ap_base,
			ccif_ctrl->ccif_md_base, ccif_ctrl);
		ret = 0;
		break;
	default:
		break;
	}
	return ret;
}

/* CCIF interrupt 1 handler: mainly for exception notification */
static irqreturn_t md_ccif_isr1(int irq, void *data)
{
	struct md_ccif_ctrl *ccif_ctrl = (struct md_ccif_ctrl *)data;
	int channel_id;

	/* must ack first, otherwise IRQ will rush in */
	channel_id = ccif_read32(ccif_ctrl->ccif_ap_base,
		APCCIF_RCHNUM);
	//CCCI_DEBUG_LOG(0, TAG,
	//	"MD CCIF IRQ 0x%X\n", channel_id);
	/*don't ack data queue to avoid missing rx intr*/
	ccif_write32(ccif_ctrl->ccif_ap_base, APCCIF_ACK,
		channel_id & (0xFFFF << RINGQ_EXP_BASE));

	md_fsm_exp_info(channel_id);

	return IRQ_HANDLED;
}

static int md_ccif_exp_ring_buf_init(struct md_ccif_ctrl *ccif_ctrl)
{
	int i = 0;
	unsigned char *buf;
	int bufsize = 0;
	struct ccci_ringbuf *ringbuf;
	struct ccci_smem_region *ccism;

	ccism = ccci_md_get_smem_by_user_id(SMEM_USER_CCISM_MCU_EXP);
	if (ccism == NULL)
		return -1;
	if (ccism->size)
		memset_io(ccism->base_ap_view_vir, 0, ccism->size);

	buf = (unsigned char *)ccism->base_ap_view_vir;

	for (i = 0; i < QUEUE_NUM; i++) {

		if (ccif_ctrl->plat_val.md_gen >= 6295) {
			bufsize = CCCI_RINGBUF_CTL_LEN +
				rx_exp_buffer_size_up_95[i]
				+ tx_exp_buffer_size_up_95[i];
			ringbuf =
				ccci_create_ringbuf(buf, bufsize,
				rx_exp_buffer_size_up_95[i],
				tx_exp_buffer_size_up_95[i]);
			if (ringbuf == NULL) {
				CCCI_ERROR_LOG(0, TAG,
					"ccci_create_ringbuf %d failed\n", i);
				return -11;
			}

		} else {
			bufsize = CCCI_RINGBUF_CTL_LEN + rx_exp_buffer_size[i]
				+ tx_exp_buffer_size[i];
			ringbuf =
			    ccci_create_ringbuf(buf, bufsize,
					rx_exp_buffer_size[i],
					tx_exp_buffer_size[i]);
			if (ringbuf == NULL) {
				CCCI_ERROR_LOG(0, TAG,
					"ccci_create_ringbuf %d failed\n", i);
				return -11;
			}
		}
		/*rx */
		ccif_ctrl->rxq[i].ringbuf_bak[RB_EXP] = ringbuf;
		ccif_ctrl->rxq[i].ccif_ch = D2H_RINGQ0 + i;
		/*tx */
		ccif_ctrl->txq[i].ringbuf_bak[RB_EXP] = ringbuf;
		ccif_ctrl->txq[i].ccif_ch = H2D_RINGQ0 + i;
		buf += bufsize;
	}

	return 0;
}

static int md_ccif_normal_ring_buf_init(struct md_ccif_ctrl *ccif_ctrl)
{
	struct ccci_smem_region *ccism;
	unsigned char *buf;
	struct ccci_ringbuf *ringbuf;
	int bufsize = 0, rxq_bufsize = 0, txq_bufsize = 0;
	int i = 0;

	ccism = ccci_md_get_smem_by_user_id(SMEM_USER_CCISM_MCU);
	if (ccism == NULL)
		return -1;
	if (ccism->size)
		memset_io(ccism->base_ap_view_vir, 0, ccism->size);
	ccif_ctrl->total_smem_size = 0;
	/*CCIF_MD_SMEM_RESERVE; */
	buf = (unsigned char *)ccism->base_ap_view_vir;

	for (i = 0; i < QUEUE_NUM; i++) {
		switch (ccif_ctrl->plat_val.md_gen) {
		case 6297:
		case 6295:
			rxq_bufsize = rx_queue_buffer_size_up_95[i];
			txq_bufsize = tx_queue_buffer_size_up_95[i];
			break;
		case 6293:
			rxq_bufsize = rx_queue_buffer_size[i];
			txq_bufsize = tx_queue_buffer_size[i];
			break;
		default: // >=6298
			rxq_bufsize = rx_queue_buffer_size_up_98[i];
			txq_bufsize = tx_queue_buffer_size_up_98[i];
			break;
		}

		bufsize = CCCI_RINGBUF_CTL_LEN + rxq_bufsize + txq_bufsize;
		if (ccif_ctrl->total_smem_size + bufsize > ccism->size) {
			CCCI_ERROR_LOG(0, TAG,
				"share memory too small,please check configure,smem_size=%d\n",
				ccism->size);
			return -2;
		}
		ringbuf = ccci_create_ringbuf(buf, bufsize, rxq_bufsize, txq_bufsize);
		if (ringbuf == NULL) {
			CCCI_ERROR_LOG(0, TAG,
				"ccci_create_ringbuf %d failed\n", i);
			return -2;
		}
		/*rx */
		ccif_ctrl->rxq[i].ringbuf_bak[RB_NORMAL] = ringbuf;
		ccif_ctrl->rxq[i].ringbuf = ringbuf;
		ccif_ctrl->rxq[i].ccif_ch = D2H_RINGQ0 + i;
		ccif_ctrl->rxq[i].worker = alloc_workqueue("rx%d_worker",
				WQ_UNBOUND | WQ_MEM_RECLAIM | WQ_HIGHPRI, 1, i);
		INIT_WORK(&ccif_ctrl->rxq[i].qwork, ccif_rx_work);
		/*tx */
		ccif_ctrl->txq[i].ringbuf_bak[RB_NORMAL] = ringbuf;
		ccif_ctrl->txq[i].ringbuf = ringbuf;
		ccif_ctrl->txq[i].ccif_ch = H2D_RINGQ0 + i;
		buf += bufsize;
		ccif_ctrl->total_smem_size += bufsize;
	}

	ccism->size = ccif_ctrl->total_smem_size;
	return 0;

}


static int md_ccif_ring_buf_init(unsigned char hif_id)
{
	int ret;

	if (!ccci_ccif_ctrl)
		return -1;

	ret = md_ccif_normal_ring_buf_init(ccci_ccif_ctrl);
	if (ret < 0)
		return ret;

	ret = md_ccif_exp_ring_buf_init(ccci_ccif_ctrl);

	return ret;
}

static int ccif_late_init(unsigned char hif_id)
{
	struct md_ccif_ctrl *ccif_ctrl = ccci_ccif_ctrl;
	int ret = 0;

	CCCI_NORMAL_LOG(0, TAG, "%s\n", __func__);

	/* IRQ is enabled after requested, so call enable_irq after
	 * request_irq will get a unbalance warning
	 */
	ret = request_irq(ccif_ctrl->ap_ccif_irq1_id, md_ccif_isr1,
			IRQF_TRIGGER_NONE, "CCIF_AP_DATA1", ccif_ctrl);
	if (ret) {
		CCCI_ERROR_LOG(0, TAG,
			"request CCIF_AP_DATA IRQ1(%d) error %d\n",
			ccif_ctrl->ap_ccif_irq1_id, ret);
		return -1;
	}
	ret = irq_set_irq_wake(ccif_ctrl->ap_ccif_irq1_id, 1);
	if (ret)
		CCCI_ERROR_LOG(0, TAG,
			"irq_set_irq_wake ccif ap_ccif_irq1_id(%d) error %d\n",
			ccif_ctrl->ap_ccif_irq1_id, ret);

	md_ccif_ring_buf_init(CCIF_HIF_ID);

	return 0;
}

static void ccif_set_clk_on(unsigned char hif_id)
{
	int idx, ret = 0;
	unsigned long flags;

	CCCI_NORMAL_LOG(0, TAG, "%s at the begin...\n", __func__);

	for (idx = 0; idx < ARRAY_SIZE(ccif_clk_table); idx++) {
		if (ccif_clk_table[idx].clk_ref == NULL)
			continue;

		ret = clk_prepare_enable(ccif_clk_table[idx].clk_ref);
		if (ret)
			CCCI_ERROR_LOG(0, TAG,
				"%s,ret=%d\n",
				__func__, ret);
		spin_lock_irqsave(&devapc_flag_lock, flags);
		devapc_check_flag = 1;
		spin_unlock_irqrestore(&devapc_flag_lock, flags);
	}

	CCCI_NORMAL_LOG(0, TAG, "%s at the end...\n", __func__);
}

/*
 * for ccif4,5 power off action different:
 * gen97: 0x1000330C [31:0] write 0x0
 * gen98: 0x10001BF0 [15:0] write 0xF7FF
 */
static void ccif_set_clk_off(unsigned char hif_id)
{
	struct md_ccif_ctrl *ccif_ctrl = ccci_ccif_ctrl;
	int idx, ccif_id;
	unsigned long flags;
	void __iomem *md_ccif_base;

	CCCI_NORMAL_LOG(0, TAG, "%s at the begin...\n", __func__);

	for (idx = 0; idx < ARRAY_SIZE(ccif_clk_table); idx++) {
		if (ccif_clk_table[idx].clk_ref == NULL)
			continue;
		md_ccif_base = NULL;
		ccif_id = -1;
		if (strcmp(ccif_clk_table[idx].clk_name,
			"infra-ccif4-md") == 0
			&& ccif_ctrl->md_ccif4_base) {
			ccif_id = 4;
			md_ccif_base = ccif_ctrl->md_ccif4_base;
		} else if (strcmp(ccif_clk_table[idx].clk_name,
			"infra-ccif5-md") == 0
			&& ccif_ctrl->md_ccif5_base) {
			ccif_id = 5;
			md_ccif_base = ccif_ctrl->md_ccif5_base;
		}
		if ((ccif_id == 4 || ccif_id == 5) && md_ccif_base) {
			if ((ccif_ctrl->plat_val.md_gen >= 6298) ||
				(ccif_ctrl->ccif_hw_reset_ver == 1)) {
				/* write 1 clear register */
				regmap_write(ccif_ctrl->plat_val.infra_ao_base,
					0xBF0, 0xF7FF);
			} else if (ccif_ctrl->plat_val.md_gen <= 6297) {
				/* Clean MD_PCCIF4_SW_READY and MD_PCCIF4_PWR_ON */
				if (!IS_ERR(ccif_ctrl->pericfg_base)) {
					CCCI_NORMAL_LOG(0, TAG, "%s:pericfg_base:0x%p\n",
						__func__, ccif_ctrl->pericfg_base);
					regmap_write(ccif_ctrl->pericfg_base, 0x30c, 0x0);
				}
			}
			udelay(1000);
			CCCI_NORMAL_LOG(0, TAG,
				"%s: after 1ms, set md_ccif%d_base + 0x14 = 0xFF\n",
				__func__, ccif_id);
			ccci_write32(md_ccif_base, 0x14, 0xFF); /* special use ccci_write32 */
		}

		spin_lock_irqsave(&devapc_flag_lock, flags);
			devapc_check_flag = 0; // maybe only for CCIF0
		spin_unlock_irqrestore(&devapc_flag_lock, flags);
		clk_disable_unprepare(ccif_clk_table[idx].clk_ref);
	}

	CCCI_NORMAL_LOG(0, TAG, "%s at the end...\n", __func__);
}

static void ccif_enable_irq1(unsigned char hif_id)
{
	struct md_ccif_ctrl *ccif_ctrl = ccci_ccif_ctrl;

	if (atomic_cmpxchg(&ccif_ctrl->ccif_irq1_enabled, 0, 1) == 0) {
		enable_irq(ccif_ctrl->ap_ccif_irq1_id);
		CCCI_NORMAL_LOG(0, TAG, "enable ccif irq1\n");
	}
}

static int ccif_disable_irq1(unsigned char hif_id)
{
	struct md_ccif_ctrl *ccif_ctrl = ccci_ccif_ctrl;

	if (!ccif_ctrl)
		return 0;

	if (atomic_cmpxchg(&ccif_ctrl->ccif_irq1_enabled, 1, 0) == 1) {
		disable_irq_nosync(ccif_ctrl->ap_ccif_irq1_id);
		/*CCCI_NORMAL_LOG(0, TAG, "disable ccif irq\n");*/
	}

	return 0;
}

static void md_ccif_sram_reset(unsigned char hif_id)
{
	int idx = 0;
	struct md_ccif_ctrl *ccif_ctrl = ccci_ccif_ctrl;

	CCCI_NORMAL_LOG(0, TAG,  "%s: %u\n", __func__, ccif_ctrl->sram_size);
	for (idx = 0; idx < ccif_ctrl->sram_size / sizeof(u32); idx += 1)
		ccif_write32(ccif_ctrl->ccif_ap_base + APCCIF_CHDATA,
		idx * sizeof(u32), 0);
	ccci_reset_seq_num(&ccif_ctrl->traffic_info);
}

static int ccif_start(unsigned char hif_id)
{
	unsigned long flags;
	struct md_ccif_ctrl *ccif_ctrl = ccci_ccif_ctrl;

	if (ccif_ctrl->ccif_state == HIFCCIF_STATE_PWRON)
		return 0;
	if (ccif_ctrl->ccif_state == HIFCCIF_STATE_MIN)
		ccif_late_init(hif_id);
	if (hif_id != CCIF_HIF_ID)
		CCCI_NORMAL_LOG(0, TAG, "%s but %d\n",
			__func__, hif_id);

	if (!ccif_ctrl->ccif_clk_free_run)
		ccif_set_clk_on(hif_id);
	else {
		spin_lock_irqsave(&devapc_flag_lock, flags);
		devapc_check_flag = 1;
		spin_unlock_irqrestore(&devapc_flag_lock, flags);
	}

	md_ccif_sram_reset(CCIF_HIF_ID);
	md_ccif_switch_ringbuf(CCIF_HIF_ID, RB_EXP);
	md_ccif_reset_queue(CCIF_HIF_ID, 1);
	md_ccif_switch_ringbuf(CCIF_HIF_ID, RB_NORMAL);
	md_ccif_reset_queue(CCIF_HIF_ID, 1);

	/* clear all ccif irq before enable it.*/
	ccci_reset_ccif_hw(AP_MD1_CCIF, ccif_ctrl->ccif_ap_base,
		ccif_ctrl->ccif_md_base, ccif_ctrl);
	ccif_enable_irq1(CCIF_HIF_ID);
	ccif_ctrl->ccif_state = HIFCCIF_STATE_PWRON;
	CCCI_NORMAL_LOG(0, TAG, "%s\n", __func__);
	return 0;
}

static int ccif_stop(unsigned char hif_id)
{
	struct md_ccif_ctrl *ccif_ctrl = ccci_ccif_ctrl;

	if (ccif_ctrl->ccif_state == HIFCCIF_STATE_PWROFF
		|| ccif_ctrl->ccif_state == HIFCCIF_STATE_MIN)
		return 0;
	/* ACK CCIF for MD. while entering flight mode,
	 * we may send something after MD slept
	 */
	ccif_ctrl->ccif_state = HIFCCIF_STATE_PWROFF;
	ccci_reset_ccif_hw(AP_MD1_CCIF,
		ccif_ctrl->ccif_ap_base, ccif_ctrl->ccif_md_base, ccif_ctrl);

	/* disable ccif clk */
	if (!ccif_ctrl->ccif_clk_free_run)
		ccif_set_clk_off(hif_id);

	CCCI_NORMAL_LOG(0, TAG, "%s\n", __func__);
	return 0;
}

static struct ccci_hif_ops ccci_hif_ccif_ops = {
	.send_skb = &md_ccif_op_send_skb,
	.give_more = &md_ccif_op_give_more,
	.write_room = &md_ccif_op_write_room,
	.stop_queue = &md_ccif_stop_queue,
	.start_queue = &md_ccif_start_queue,
	.dump_status = &md_ccif_op_dump_status,

	.start = &ccif_start,
	.stop = &ccif_stop,
	.debug = &ccif_debug,
	.send_data = &md_ccif_send_data,
	.fill_rt_header = &ccif_hif_fill_rt_header,
	.stop_for_ee = &ccif_disable_irq1,
};

static void ccif_clk_init(struct device *dev)
{
	unsigned int idx;

	for (idx = 0; idx < ARRAY_SIZE(ccif_clk_table); idx++) {
		ccif_clk_table[idx].clk_ref = devm_clk_get(dev,
			ccif_clk_table[idx].clk_name);
		if (IS_ERR(ccif_clk_table[idx].clk_ref)) {
			CCCI_ERROR_LOG(-1, TAG,
				 "ccif get %s failed\n",
					ccif_clk_table[idx].clk_name);
			ccif_clk_table[idx].clk_ref = NULL;
		}
	}
}

static inline void md_ccif_queue_struct_init(struct md_ccif_queue *queue,
	unsigned char hif_id, enum DIRECTION dir, unsigned char index)
{
	queue->dir = dir;
	queue->index = index;
	queue->hif_id = hif_id;

	spin_lock_init(&queue->rx_lock);
	spin_lock_init(&queue->tx_lock);
	atomic_set(&queue->rx_on_going, 0);
	queue->debug_id = 0;
	//queue->resume_cnt = 0;
	queue->budget = RX_BUGDET;
}

static u64 ccif_dmamask = DMA_BIT_MASK(36);
static int ccif_hif_hw_init(struct device *dev, struct md_ccif_ctrl *ccif_ctrl)
{
	struct device_node *node = NULL;
	int ret;

	if (!dev) {
		CCCI_ERROR_LOG(-1, TAG, "No ccif driver in dtsi\n");
		ret = -3;
		return ret;
	}
	/* get data from mddriver node */
	node = of_find_compatible_node(NULL, NULL, "mediatek,mddriver");
	of_property_read_u32(node, "mediatek,md-generation",
		&ccif_ctrl->plat_val.md_gen);
	ccif_ctrl->plat_val.infra_ao_base = syscon_regmap_lookup_by_phandle(
		node, "ccci-infracfg");
	if (IS_ERR(ccif_ctrl->plat_val.infra_ao_base)) {
		CCCI_ERROR_LOG(-1, TAG, "No infra_ao register in dtsi\n");
		ret = -4;
		return ret;
	}
	/* get data from dev->of_node */
	node = dev->of_node;
	if (!node) {
		CCCI_ERROR_LOG(-1, TAG, "No ccif node in dtsi\n");
		ret = -5;
		return ret;
	}
	ccif_ctrl->ccif_ap_base = of_iomap(node, 0);
	ccif_ctrl->ccif_md_base = of_iomap(node, 1);
	if (!ccif_ctrl->ccif_ap_base || !ccif_ctrl->ccif_md_base) {
		CCCI_ERROR_LOG(-1, TAG,
			"ap_ccif_base=NULL or ccif_md_base NULL\n");
		return -2;
	}
	CCCI_DEBUG_LOG(-1, TAG, "ap_ccif_base:0x%p, ccif_md_base:0x%p\n",
		ccif_ctrl->ccif_ap_base, ccif_ctrl->ccif_md_base);

	ccif_ctrl->ap_ccif_irq0_id = irq_of_parse_and_map(node, 0);
	ccif_ctrl->ap_ccif_irq1_id = irq_of_parse_and_map(node, 1);
	if (ccif_ctrl->ap_ccif_irq0_id == 0 || ccif_ctrl->ap_ccif_irq1_id == 0) {
		CCCI_ERROR_LOG(-1, TAG, "ccif_irq0:%d,ccif_irq1:%d\n",
			ccif_ctrl->ap_ccif_irq0_id, ccif_ctrl->ap_ccif_irq1_id);
		return -2;
	}
	CCCI_DEBUG_LOG(-1, TAG, "ccif_irq0:%d,ccif_irq1:%d\n",
		ccif_ctrl->ap_ccif_irq0_id, ccif_ctrl->ap_ccif_irq1_id);

	ret = of_property_read_u32(node, "mediatek,sram-size",
		&ccif_ctrl->sram_size);
	if (ret < 0)
		ccif_ctrl->sram_size = CCIF_SRAM_SIZE;
	ccif_ctrl->ccif_sram_layout =
		(struct ccif_sram_layout *)(ccif_ctrl->ccif_ap_base +
			APCCIF_CHDATA);

	ret = of_property_read_u32(node,
		"mediatek,ccif-clk-free-run", &ccif_ctrl->ccif_clk_free_run);
	if (ret < 0)
		ccif_ctrl->ccif_clk_free_run = 0;

	if (!ccif_ctrl->ccif_clk_free_run)
		ccif_clk_init(dev);
	else {
		devapc_check_flag = 1;
		CCCI_NORMAL_LOG(0, TAG, "No need control ccif clk\n");
	}

	dev->dma_mask = &ccif_dmamask;
	dev->coherent_dma_mask = ccif_dmamask;
	dev->platform_data = ccif_ctrl;

	ret = request_irq(ccif_ctrl->ap_ccif_irq0_id, md_ccif_isr0,
			IRQF_TRIGGER_NONE, "CCIF_AP_DATA0", ccif_ctrl);
	if (ret) {
		CCCI_ERROR_LOG(0, TAG,
			"request CCIF_AP_DATA IRQ0(%d) error %d\n",
			ccif_ctrl->ap_ccif_irq0_id, ret);
		return -1;
	}
	ret = irq_set_irq_wake(ccif_ctrl->ap_ccif_irq0_id, 1);
	if (ret)
		CCCI_ERROR_LOG(0, TAG,
			"irq_set_irq_wake ccif ap_ccif_irq0_id(%d) error %d\n",
			ccif_ctrl->ap_ccif_irq0_id, ret);

	ret = of_property_read_u32(node, "mediatek,ccif-hw-reset-ver",
			&ccif_ctrl->ccif_hw_reset_ver);
	if (ret < 0)
		ccif_ctrl->ccif_hw_reset_ver = 0;

	if (ccif_ctrl->ccif_hw_reset_ver == 1) {
		ret = of_property_read_u32(node, "mediatek,ccif-hw-reset-bit",
				&ccif_ctrl->ccif_hw_reset_bit);
		if (ret < 0) {
			CCCI_ERROR_LOG(-1, TAG,
				"[%s] error: ccif-hw-reset-bit not exist\n",
				__func__);
			return -8;
		}
		/* get data from other node */
		node = of_find_compatible_node(NULL, NULL, "mediatek,infracfg");
		if (!node) {
			CCCI_ERROR_LOG(-1, TAG,
				"[%s] error: infracfg node is not exist\n",
				__func__);
			return -8;
		}

		ccif_ctrl->infracfg_base = of_iomap(node, 0);
		if (!ccif_ctrl->infracfg_base) {
			CCCI_ERROR_LOG(-1, TAG,
				"[%s] error: infracfg_base fail\n", __func__);
			return -8;
		}
	}
	/* get data from other node */
	node = of_find_compatible_node(NULL, NULL, "mediatek,md_ccif4");
	if (node) {
		ccif_ctrl->md_ccif4_base = of_iomap(node, 0);
		if (!ccif_ctrl->md_ccif4_base) {
			CCCI_ERROR_LOG(-1, TAG, "ccif4_base fail\n");
			return -6;
		}
	}
	node = of_find_compatible_node(NULL, NULL, "mediatek,md_ccif5");
	if (node) {
		ccif_ctrl->md_ccif5_base = of_iomap(node, 0);
		if (!ccif_ctrl->md_ccif5_base) {
			CCCI_ERROR_LOG(-1, TAG, "ccif5_base fail\n");
			return -7;
		}
	}
	/* Get pericfg base(0x10003000) for ccif4,5 */
	ccif_ctrl->pericfg_base = syscon_regmap_lookup_by_phandle(
		dev->of_node, "ccif-pericfg");
	if (IS_ERR(ccif_ctrl->pericfg_base))
		CCCI_ERROR_LOG(-1, TAG, "%s: get ccif-pericfg failed\n",
			__func__);

	return 0;

}

int ccci_ccif_hif_init(struct platform_device *pdev, unsigned char hif_id)
{
	int i, ret;
	struct md_ccif_ctrl *ccif_ctrl;

	spin_lock_init(&devapc_flag_lock);

	ccif_ctrl = kzalloc(sizeof(struct md_ccif_ctrl), GFP_KERNEL);
	if (!ccif_ctrl) {
		CCCI_ERROR_LOG(-1, TAG,
			"%s:alloc hif_ctrl fail\n", __func__);
		return -1;
	}
	INIT_WORK(&ccif_ctrl->ccif_sram_work, md_ccif_sram_rx_work);
	INIT_WORK(&ccif_ctrl->ccif_status_notify_work, ccif_queue_status_notify_work);

	timer_setup(&ccif_ctrl->traffic_monitor,
		md_ccif_traffic_monitor_func, 0);
	INIT_WORK(&ccif_ctrl->traffic_info.traffic_work_struct,
		md_ccif_traffic_work_func);

	ccif_ctrl->channel_id = 0;
	ccif_ctrl->hif_id = hif_id;

	ccif_ctrl->wakeup_ch = 0;
	atomic_set(&ccif_ctrl->ccif_irq_enabled, 1);
	atomic_set(&ccif_ctrl->ccif_irq1_enabled, 1);
	ccci_reset_seq_num(&ccif_ctrl->traffic_info);

	/*init queue */
	for (i = 0; i < QUEUE_NUM; i++) {
		md_ccif_queue_struct_init(&ccif_ctrl->txq[i],
			ccif_ctrl->hif_id, OUT, i);
		md_ccif_queue_struct_init(&ccif_ctrl->rxq[i],
			ccif_ctrl->hif_id, IN, i);
	}

	ccif_ctrl->ops = &ccci_hif_ccif_ops;
	ccif_ctrl->plat_dev = pdev;
	ret = ccif_hif_hw_init(&pdev->dev, ccif_ctrl);
	if (ret < 0) {
		CCCI_ERROR_LOG(-1, TAG, "ccci ccif hw init fail");
		kfree(ccif_ctrl);
		return ret;
	}
	ccci_ccif_ctrl = ccif_ctrl;
	ccci_hif_register(ccif_ctrl->hif_id,
		(void *)ccci_ccif_ctrl, &ccci_hif_ccif_ops);

	return 0;
}

int ccci_hif_ccif_probe(struct platform_device *pdev)
{
	int ret;

	ret = ccci_ccif_hif_init(pdev, CCIF_HIF_ID);
	if (ret < 0) {
		CCCI_ERROR_LOG(-1, TAG, "ccci ccif init fail");
		return ret;
	}

	return 0;
}

static int ccif_suspend_noirq(struct device *dev)
{
	return 0;
}

static int ccif_resume_noirq(struct device *dev)
{
	struct arm_smccc_res res;
	struct md_ccif_ctrl *ccif_ctrl = ccci_ccif_ctrl;
	unsigned int ccif_ch;

	if (!ccif_ctrl) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: ccif not ready -- failed.", __func__);
		return 0;
	}

	if (ccif_ctrl && ccif_ctrl->plat_val.md_gen == 6293)
		ccif_write32(ccif_ctrl->ccif_ap_base, APCCIF_CON, 0x01);

	arm_smccc_smc(MTK_SIP_KERNEL_CCCI_CONTROL, MD_CLOCK_REQUEST,
		MD_WAKEUP_AP_SRC, WAKE_SRC_HIF_CCIF0, 0, 0, 0, 0, &res);
	CCCI_NORMAL_LOG(-1, TAG,
		"[%s] flag_1=0x%lx, flag_2=0x%lx, flag_3=0x%lx, flag_4=0x%lx\n",
		__func__, res.a0, res.a1, res.a2, res.a3);
	if (!res.a0 && res.a1 == WAKE_SRC_HIF_CCIF0) {
		ccif_ch = ccif_read32(ccif_ctrl->ccif_ap_base, APCCIF_RCHNUM);
		CCCI_NORMAL_LOG(-1, TAG,
			"CCIF 0 Wake up: channel_id == 0x%x\n", ccif_ch);
		ccif_ctrl->wakeup_ch = ccif_ch;
		ccif_ctrl->wakeup_count++;
		if (test_and_clear_bit(AP_MD_CCB_WAKEUP,
			&ccif_ctrl->wakeup_ch))
			CCCI_NOTICE_LOG(0, TAG,
				"CCIF_MD wakeup source:(CCB)(%u)\n",
				ccif_ctrl->wakeup_count);
	}
	return 0;
}

static const struct dev_pm_ops ccif_pm_ops = {
	.suspend_noirq = ccif_suspend_noirq,
	.resume_noirq = ccif_resume_noirq,
};

static const struct of_device_id ccci_ccif_of_ids[] = {
	{.compatible = "mediatek,ccci_ccif"},
	{}
};

static struct platform_driver ccci_hif_ccif_driver = {

	.driver = {
		.name = "ccci_hif_ccif",
		.of_match_table = ccci_ccif_of_ids,
		.pm = &ccif_pm_ops,
	},

	.probe = ccci_hif_ccif_probe,
};

static int __init ccci_hif_ccif_init(void)
{
	int ret;

	ret = platform_driver_register(&ccci_hif_ccif_driver);
	if (ret) {
		CCCI_ERROR_LOG(-1, TAG, "ccci hif_ccif driver init fail %d",
			ret);
		return ret;
	}
	return 0;
}

static void __exit ccci_hif_ccif_exit(void)
{
}

module_init(ccci_hif_ccif_init);
module_exit(ccci_hif_ccif_exit);

MODULE_AUTHOR("ccci");
MODULE_DESCRIPTION("ccci hif ccif driver");
MODULE_LICENSE("GPL");
