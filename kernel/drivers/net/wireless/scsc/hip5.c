/******************************************************************************
 *
 * Copyright (c) 2014 - 2021 Samsung Electronics Co., Ltd. All rights reserved
 *
 *****************************************************************************/

#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <scsc/scsc_mx.h>
#include <scsc/scsc_mifram.h>
#include <linux/ktime.h>
#include <linux/kthread.h>
#include <linux/rtc.h>
#include <scsc/scsc_logring.h>
#include <scsc/scsc_warn.h>

#include "hip5.h"
#include "hip4_sampler.h"
#include "mbulk.h"
#include "dev.h"
#include "load_manager.h"
#include "debug.h"
#include "mgt.h"

#ifdef CONFIG_SCSC_WLAN_TX_API
#include "tx_api.h"
#endif
#ifdef CONFIG_SCSC_WLAN_HOST_DPD
#include "dpd_mmap.h"
#endif

struct hip5_tx_dma_desc {
	u16           len;
	u16           offset;
	dma_addr_t    skb_dma_addr;
	mbulk_colour  colour;
	struct sk_buff *skb;
};

#if defined(CONFIG_SCSC_PCIE_PAEAN_X86) || defined(CONFIG_SOC_S5E9925)
static bool hip5_scoreboard_in_ramrp = false;
#else
static bool hip5_scoreboard_in_ramrp = false;
#endif
module_param(hip5_scoreboard_in_ramrp, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(hip5_scoreboard_in_ramrp, "scoreboard location (default: Y in RAMRP, N: in DRAM)");

#if defined(CONFIG_SCSC_BB_REDWOOD)
static bool hip5_tx_zero_copy = true;
#else
static bool hip5_tx_zero_copy = false;
#endif
module_param(hip5_tx_zero_copy, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(hip5_tx_zero_copy, "Tx zero copy (default: enable)");

static uint hip5_tx_zero_copy_tx_slots = 2048;
module_param(hip5_tx_zero_copy_tx_slots, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(hip5_tx_zero_copy_tx_slots, "num of TX buffers in zero copy mode (max: 2048)");

#if IS_ENABLED(CONFIG_SCSC_LOGRING)
static bool hip4_dynamic_logging = true;
module_param(hip4_dynamic_logging, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(hip4_dynamic_logging, "Dynamic logging, logring is disabled if tput > hip4_qos_med_tput_in_mbps. (default: Y)");

static int hip4_dynamic_logging_tput_in_mbps = 150;
module_param(hip4_dynamic_logging_tput_in_mbps, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(hip4_dynamic_logging_tput_in_mbps, "throughput (in Mbps) to apply dynamic logring logging");
#endif

#ifdef CONFIG_SCSC_QOS
static bool hip4_qos_enable = true;
module_param(hip4_qos_enable, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(hip4_qos_enable, "enable HIP4 PM QoS. (default: Y)");

static int hip4_qos_max_tput_in_mbps = 250;
module_param(hip4_qos_max_tput_in_mbps, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(hip4_qos_max_tput_in_mbps, "throughput (in Mbps) to apply Max PM QoS");

static int hip4_qos_med_tput_in_mbps = 150;
module_param(hip4_qos_med_tput_in_mbps, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(hip4_qos_med_tput_in_mbps, "throughput (in Mbps) to apply Median PM QoS");
#endif

static ktime_t intr_received_fb;
static ktime_t bh_init_fb;
static ktime_t bh_end_fb;
static ktime_t intr_received_ctrl;
static ktime_t bh_init_ctrl;
static ktime_t bh_end_ctrl;
static ktime_t intr_received_data;
static ktime_t bh_init_data;
static ktime_t bh_end_data;
static ktime_t send;
static ktime_t closing;

enum rw {
	widx,
	ridx,
};

/*offset of F/W owned indices */
#define FW_OWN_OFS      (128)
/**
 * HIP queue indices layout in the scoreboard (SC-505612-DD). v3
 *
 * The queue indices which owned by the host are only writable by the host.
 * F/W can only read them. And vice versa.
 */
static int q_idx_layout[24][2] = {
	{                0,  FW_OWN_OFS + 0},	/* HIP5_MIF_Q_FH_CTRL  : 0 */
	{                4,  FW_OWN_OFS + 4},	/* HIP5_MIF_Q_FH_PRI0  : 1 */
	{                8,  FW_OWN_OFS + 8},	/* HIP5_MIF_Q_FH_SKB0  : 2 */
	{  FW_OWN_OFS + 12,              12},	/* HIP5_MIF_Q_FH_RFBC  : 3 */
	{  FW_OWN_OFS + 16,              16},   /* HIP5_MIF_Q_FH_RFBD0 : 4 */
	{  FW_OWN_OFS + 20,              20},	/* HIP5_MIF_Q_TH_CTRL  : 5 */
	{  FW_OWN_OFS + 24,              24},	/* HIP5_MIF_Q_TH_DAT0  : 6 */
	{               28, FW_OWN_OFS + 28},	/* HIP5_MIF_Q_TH_RFBC  : 7 */
	{               32, FW_OWN_OFS + 32},	/* HIP5_MIF_Q_TH_RFBD0 : 8 */
	{				36,	FW_OWN_OFS + 36},	/* HIP5_MIF_Q_FH_PRI1  : 9 */
	{				40,	FW_OWN_OFS + 40},	/* HIP5_MIF_Q_FH_SKB1  : 10 */
	{  FW_OWN_OFS + 44,				 44},	/* HIP5_MIF_Q_FH_RFBD1 : 11 */
	{  FW_OWN_OFS + 48,				 48},	/* HIP5_MIF_Q_TH_DAT1  : 12 */
	{               52, FW_OWN_OFS + 52},	/* HIP5_MIF_Q_TH_RFBD1 : 13 */
	{               56, FW_OWN_OFS + 56},	/* HIP5_MIF_Q_FH_DAT0  : 14 */
	{				60,	FW_OWN_OFS + 60},	/* HIP5_MIF_Q_FH_DAT1  : 15 */
	{				64,	FW_OWN_OFS + 64},	/* HIP5_MIF_Q_FH_DAT2  : 16 */
	{               68, FW_OWN_OFS + 68},	/* HIP5_MIF_Q_FH_DAT3  : 17 */
	{               72, FW_OWN_OFS + 72},	/* HIP5_MIF_Q_FH_DAT4  : 18 */
	{				76,	FW_OWN_OFS + 76},	/* HIP5_MIF_Q_FH_DAT5  : 19 */
	{				80,	FW_OWN_OFS + 80},	/* HIP5_MIF_Q_FH_DAT6  : 20 */
	{               84, FW_OWN_OFS + 84},	/* HIP5_MIF_Q_FH_DAT7  : 21 */
	{               88, FW_OWN_OFS + 88},	/* HIP5_MIF_Q_FH_DAT8  : 22 */
	{				92,	FW_OWN_OFS + 92},	/* HIP5_MIF_Q_FH_DAT9  : 23 */
};

/* MAX_STORM. Max Interrupts allowed when platform is in suspend */
#define MAX_STORM            5

/* Timeout for Wakelocks in HIP  */
#define SLSI_HIP_WAKELOCK_TIME_OUT_IN_MS   (1000)

#define HIP5_TH_IDX_ERR_NUM_RETRY    4
#define FB_NO_SPC_NUM_RET    100
#define FB_NO_SPC_SLEEP_MS   10
#define FB_NO_SPC_DELAY_US   1000

static enum hip5_hip_q_conf slsi_hip_vif_to_q_mapping(u8 vif_index)
{
	switch (vif_index) {
	case 1:
		return HIP5_MIF_Q_FH_DAT0;
	default:
		return HIP5_MIF_Q_FH_DAT1;
	}
}

static bool is_fw_config_a55(void)
{
	char fw_build_id[SCSC_LOG_FW_VERSION_SIZE] = {0};
	char *p;

	mxman_get_fw_version(fw_build_id, SCSC_LOG_FW_VERSION_SIZE);
	p = strstr(fw_build_id, "a55");
	if (p) {
		/* using a55 config Firmware */
		return true;
	}
	return false;
}

/* Update scoreboard index */
static void hip5_update_index(struct slsi_hip *hip, u32 q, enum rw r_w, u16 value)
{
	struct hip_priv    *hip_priv = hip->hip_priv;

	write_lock_bh(&hip_priv->rw_scoreboard);
	if (hip5_scoreboard_in_ramrp) {
		writel(value, hip->hip_priv->scbrd_ramrp_base + q_idx_layout[q][r_w]);
	} else {
		*((u16 *)(hip->hip_priv->scbrd_base + q_idx_layout[q][r_w])) = value;

		/* Memory barrier when updating shared mailbox/memory */
		dma_wmb();
	}
	SCSC_HIP4_SAMPLER_Q(hip_priv->minor, q, r_w, value, 0);
	write_unlock_bh(&hip_priv->rw_scoreboard);
}

/* Read scoreboard index */
static u16 hip5_read_index(struct slsi_hip *hip, u32 q, enum rw r_w)
{
	struct hip_priv    *hip_priv = hip->hip_priv;
	u32                 value = 0;

	read_lock_bh(&hip_priv->rw_scoreboard);
	if (hip->hip_control->init.magic_number != (HIP5_CONFIG_INIT_MAGIC_NUM)) {
		SLSI_ERR_NODEV("incorrect magic_number (expected:0x%x, got:0x%x)\n",
			(HIP5_CONFIG_INIT_MAGIC_NUM),
			hip->hip_control->init.magic_number);
		read_unlock_bh(&hip_priv->rw_scoreboard);
		return 0xFFFF;
	}
	if (hip5_scoreboard_in_ramrp) {
		value = readl(hip->hip_priv->scbrd_ramrp_base + q_idx_layout[q][r_w]);
	} else {
		value = *((u16 *)(hip->hip_priv->scbrd_base + q_idx_layout[q][r_w]));

		/* Memory barrier when reading shared mailbox/memory */
		dma_rmb();
	}
	read_unlock_bh(&hip_priv->rw_scoreboard);
	return value;
}

int slsi_hip_wlan_get_rtc_time(struct rtc_time *tm)
{
	struct rtc_device *rtc = NULL;
	int err;

	rtc = rtc_class_open(CONFIG_RTC_HCTOSYS_DEVICE);
	if (!rtc) {
		SLSI_ERR_NODEV("Can not find RTC Device\n");
		return -ENXIO;
	}
	err = rtc_read_time(rtc, tm);

	rtc_class_close(rtc);

	if (err < 0)
		return err;

	return 0;
}

static void hip5_dump_dbg(struct slsi_hip *hip, struct mbulk *m, struct sk_buff *skb, struct scsc_service *service)
{
	unsigned int        i = 0;
	scsc_mifram_ref     ref;

	SLSI_ERR_NODEV("intr_to_host_fb 0x%x\n", hip->hip_priv->intr_to_host_ctrl_fb);
	SLSI_ERR_NODEV("intr_to_host_ctrl 0x%x\n", hip->hip_priv->intr_to_host_ctrl);
	SLSI_ERR_NODEV("intr_to_host_dat 0x%x\n", hip->hip_priv->intr_to_host_data1);
	SLSI_ERR_NODEV("intr_from_host_ctrl 0x%x\n", hip->hip_priv->intr_from_host_ctrl);
	SLSI_ERR_NODEV("intr_from_host_data 0x%x\n", hip->hip_priv->intr_from_host_data);

	/* Print scoreboard */
	for (i = 0; i < 24; i++) {
		SLSI_ERR_NODEV("Q%dW 0x%x\n", i, hip5_read_index(hip, i, widx));
		SLSI_ERR_NODEV("Q%dR 0x%x\n", i, hip5_read_index(hip, i, ridx));
	}

	if (service)
		scsc_mx_service_mif_dump_registers(service);

	if (m && service) {
		if (scsc_mx_service_mif_ptr_to_addr(service, m, &ref))
			return;
		SLSI_ERR_NODEV("m: %p 0x%x\n", m, ref);
		print_hex_dump(KERN_ERR, SCSC_PREFIX "mbulk ", DUMP_PREFIX_NONE, 16, 1, m, sizeof(struct mbulk), 0);
	}
	if (m && mbulk_has_signal(m))
		print_hex_dump(KERN_ERR, SCSC_PREFIX "sig   ", DUMP_PREFIX_NONE, 16, 1, mbulk_get_signal(m),
			       MBULK_SEG_SIG_BUFSIZE(m), 0);
	if (skb)
		print_hex_dump(KERN_ERR, SCSC_PREFIX "skb   ", DUMP_PREFIX_NONE, 16, 1, skb->data, skb->len > 0xff ? 0xff : skb->len, 0);

	SLSI_ERR_NODEV("time: now          %lld\n", ktime_to_ns(ktime_get()));
	SLSI_ERR_NODEV("time: send         %lld\n", ktime_to_ns(send));
	SLSI_ERR_NODEV("time: intr_fb      %lld\n", ktime_to_ns(intr_received_fb));
	SLSI_ERR_NODEV("time: bh_init_fb   %lld\n", ktime_to_ns(bh_init_fb));
	SLSI_ERR_NODEV("time: bh_end_fb    %lld\n", ktime_to_ns(bh_end_fb));
	SLSI_ERR_NODEV("time: intr_ctrl    %lld\n", ktime_to_ns(intr_received_ctrl));
	SLSI_ERR_NODEV("time: bh_init_ctrl %lld\n", ktime_to_ns(bh_init_ctrl));
	SLSI_ERR_NODEV("time: bh_end_ctrl  %lld\n", ktime_to_ns(bh_end_ctrl));
	SLSI_ERR_NODEV("time: intr_data    %lld\n", ktime_to_ns(intr_received_data));
	SLSI_ERR_NODEV("time: bh_init_data %lld\n", ktime_to_ns(bh_init_data));
	SLSI_ERR_NODEV("time: bh_end_data  %lld\n", ktime_to_ns(bh_end_data));
	SLSI_ERR_NODEV("time: closing      %lld\n", ktime_to_ns(closing));
}

/* Transform skb to mbulk (fapi_signal + payload) */
static struct mbulk *hip5_skb_to_mbulk(struct hip_priv *hip, struct sk_buff *skb, bool ctrl_packet, mbulk_colour colour)
{
	struct mbulk        *m = NULL;
	void                *sig = NULL, *b_data = NULL;
	size_t              payload = 0;
	u8                  pool_id = ctrl_packet ? MBULK_POOL_ID_CTRL : MBULK_POOL_ID_DATA;
	u8                  headroom = 0, tailroom = 0;
	enum mbulk_class    clas = ctrl_packet ? MBULK_CLASS_FROM_HOST_CTL : MBULK_CLASS_FROM_HOST_DAT;
	struct slsi_skb_cb *cb = slsi_skb_cb_get(skb);

	payload = skb->len - cb->sig_length;

	/* Get headroom/tailroom */
	headroom = hip->unidat_req_headroom;
	tailroom = hip->unidat_req_tailroom;

	/* Allocate mbulk */
	if (payload > 0) {
		/* If signal include payload, add headroom and tailroom */
		m = mbulk_with_signal_alloc_by_pool(pool_id, colour, clas, cb->sig_length + 4,
						    payload + headroom + tailroom);
		if (!m)
			return NULL;
		if (!mbulk_reserve_head(m, headroom))
			return NULL;
	} else {
		/* If it is only a signal do not add headroom */
		m = mbulk_with_signal_alloc_by_pool(pool_id, colour, clas, cb->sig_length + 4, 0);
		if (!m)
			return NULL;
	}

	if (ctrl_packet && (hip->version == 4)) {
		/* Get signal handler */
		sig = mbulk_get_signal(m);
		if (!sig) {
			mbulk_free_virt_host(m);
			return NULL;
		}

		/* Copy signal */
		/* 4Bytes offset is required for FW fapi header */
		memcpy(sig + 4, skb->data, cb->sig_length);
	}

	/* Copy payload */
	/* If the signal has payload memcpy the data */
	if (payload > 0) {
		/* Get head pointer */
		b_data = mbulk_dat_rw(m);
		if (!b_data) {
			mbulk_free_virt_host(m);
			return NULL;
		}

		/* Copy payload skipping the signal data */
		memcpy(b_data, skb->data + cb->sig_length, payload);
		mbulk_append_tail(m, payload);
	}
	m->flag |= MBULK_F_OBOUND;
	return m;
}

static void hip5_opt_log_wakeup_signal(struct slsi_dev *sdev, struct sk_buff *skb, bool ctrl_packet)
{
	SLSI_INFO(sdev, "WI-FI wake up by %s frame (vif:%d, sig_id:0x%4X)\n",
		ctrl_packet ? "MLME" : "DATA",
		fapi_get_vif(skb),
		fapi_get_sigid(skb));

	slsi_skb_cb_get(skb)->wakeup = true;
	if (ctrl_packet)
		SCSC_BIN_TAG_INFO(BIN_WIFI_PM, skb->data, skb->len > 128 ? 128 : skb->len);
	else
		slsi_dump_eth_packet(sdev, skb);
}

static int hip5_opt_hip_signal_to_skb(struct slsi_dev *sdev,
                                                struct scsc_service *service,
                                                struct hip_priv *hip_priv,
                                                struct hip5_hip_control *ctrl,
                                                enum hip5_hip_q_conf conf,
                                                scsc_mifram_ref *to_free,
                                                u16 *idx_r,
                                                u16 *idx_w,
                                                struct sk_buff_head *skb_list)
{
	struct hip5_opt_hip_signal *hip_signal;
	struct hip5_opt_bulk_desc  *bulk_desc = NULL;
	struct sk_buff             *skb = NULL;
	struct slsi_skb_cb         *cb;
	void                       *sig_ptr, *mem;
	u8                         num_bd = 0, free = 0, padding = 0;

	if (conf == HIP5_MIF_Q_TH_DAT0)
		hip_signal = (struct hip5_opt_hip_signal *)(&ctrl->q_tlv[2].array[*idx_r]);
	else if (conf == HIP5_MIF_Q_TH_CTRL)
		hip_signal = (struct hip5_opt_hip_signal *)(&ctrl->q_tlv[4].array[*idx_r]);
	else
		return -EINVAL;

	num_bd = hip_signal->num_bd;
	sig_ptr = (void *)hip_signal + 4; /* first 4 bytes in HIP entry is the entry header */
	padding = (8 - ((hip_signal->sig_len + 4)  % 8)) & 0x7;

	SLSI_DBG4(sdev, SLSI_HIP, "SIGNAL: idx_r:%d, idx_w:%d, ID:0x%4X, num_bd:%d, sig_len:%d, wake_up:%d\n",
				*idx_r,
				*idx_w,
				le16_to_cpu(((struct fapi_signal *)(sig_ptr))->id),
				num_bd,
				hip_signal->sig_len,
				hip_signal->wake_up);

	if (hip_signal->num_bd > HIP5_OPT_NUM_BD_PER_SIGNAL_MAX) {
		SLSI_ERR_NODEV("invalid num_bd %d\n", hip_signal->num_bd);
		return -EINVAL;
	}

	bulk_desc = (struct hip5_opt_bulk_desc *) (sig_ptr + hip_signal->sig_len + padding);
	if ((*idx_r == (MAX_NUM - 1)) && (hip_signal->sig_len > 52)) {
		/* wrap around, bulk desc at beginning of buffer */
		bulk_desc = (struct hip5_opt_bulk_desc *) (&ctrl->q_tlv[4].array[0]);
	}

	/* check if signal is complete or not? */
	if (hip_signal->num_bd > 3) {
		u8 i = 0;
		u16 todo = 0;
		u16 idx_w_temp = (*idx_r);

		for (i = 0; i < (hip_signal->num_bd - 3); i = i + 8) {
			/* one write index for every 8 BDs */
			idx_w_temp = idx_w_temp + 1;
			idx_w_temp &= (MAX_NUM - 1);
		}

		if (idx_w_temp == (*idx_w)) {
			/* signal is not fully written yet */
			return -EFAULT;
		}

		todo = (((*idx_w) - idx_w_temp) & (MAX_NUM - 1));
		if (todo > (MAX_NUM >> 1)) {
			return -EFAULT;
		}
	}

	/* signals with no Bulk data */
	/* TODO: review for signals that are longer than 64 bytes, extends to 2 HIP entries */
	if (!num_bd) {
		skb = alloc_skb(hip_signal->sig_len, GFP_ATOMIC);
		if (!skb) {
			SLSI_ERR_NODEV("Error allocating skb %d bytes\n", hip_signal->sig_len);
			return -ENOMEM;
		}

		memcpy(skb_put(skb, hip_signal->sig_len), sig_ptr, hip_signal->sig_len);
		cb = slsi_skb_cb_init(skb);
		cb->sig_length = hip_signal->sig_len;
		cb->data_length = cb->sig_length;

		if (hip_signal->wake_up)
			hip5_opt_log_wakeup_signal(sdev, skb, true);
		__skb_queue_tail(skb_list, skb);
		return 0;
	}

	while (num_bd) {
		if (!(bulk_desc->flag & HIP5_OPT_BD_CHAIN_START)) {
			/* single BD i.e. one BD for one MPDU */
			SLSI_DBG4(sdev, SLSI_HIP, "BD: buf_addr:0x%X, offset:%d, data_len:%d, flag:%d\n",
				bulk_desc->buf_addr,
				bulk_desc->offset,
				bulk_desc->data_len,
				bulk_desc->flag);

			skb = alloc_skb(hip_signal->sig_len + bulk_desc->data_len, GFP_ATOMIC);
			if (!skb) {
				SLSI_ERR_NODEV("Error allocating skb %d bytes\n", hip_signal->sig_len + bulk_desc->data_len);
				/* SKB could not be allocated, but the BD still needs to be freed
				 * and the index needs updated correctly
				 */
				to_free[free++] = bulk_desc->buf_addr;
				num_bd--;
				bulk_desc++;
				if((void *) bulk_desc == ((void *)(&ctrl->q_tlv[2].array[MAX_NUM]))) {
					bulk_desc = (struct hip5_opt_bulk_desc *)(&ctrl->q_tlv[2].array[0]);
				}
				continue;
			}

			memcpy(skb_put(skb, hip_signal->sig_len), sig_ptr, hip_signal->sig_len);

			cb = slsi_skb_cb_init(skb);
			cb->sig_length = hip_signal->sig_len;
			cb->data_length = cb->sig_length;

			mem = scsc_mx_service_mif_addr_to_ptr(service, bulk_desc->buf_addr);
			if (!mem) {
				SLSI_ERR_NODEV("bulk_desc->buf_addr NULL\n");
				SLSI_ERR_NODEV("T-H stall due to un-recoverable BD; trigger Panic\n");
				queue_work(sdev->device_wq, &sdev->trigger_wlan_fail_work);
				consume_skb(skb);
				return -EFAULT;
			}

			/* Track buf_addr that should be freed */
			to_free[free++] = bulk_desc->buf_addr;

			if (bulk_desc->data_len)
				fapi_append_data(skb, mem + bulk_desc->offset, bulk_desc->data_len);

			if (hip_signal->wake_up && skb_queue_empty(skb_list)) {
				/* It is the first frame in the list that caused the wake-up */
				hip5_opt_log_wakeup_signal(sdev, skb, (conf == HIP5_MIF_Q_TH_CTRL) ? true : false);
			}
			__skb_queue_tail(skb_list, skb);

			num_bd--;
			bulk_desc++;
			if((void *) bulk_desc == ((void *)(&ctrl->q_tlv[2].array[MAX_NUM - 1]) + 64)) {
				bulk_desc = (struct hip5_opt_bulk_desc *)(&ctrl->q_tlv[2].array[0]);
			}
		} else {
			/* chained BDs i.e. multiple BDs for one MPDU */
			u16 data_len = 0;
			struct hip5_opt_bulk_desc *bd_temp = NULL;

			/* loop through chain to find total length of data */
			bd_temp = bulk_desc;
			data_len = bd_temp->data_len;
			do {
				bd_temp++;
				if((void *) bd_temp == ((void *)(&ctrl->q_tlv[2].array[MAX_NUM - 1]) + 64)) {
					bd_temp = (struct hip5_opt_bulk_desc *)(&ctrl->q_tlv[2].array[0]);
				}
				data_len += bd_temp->data_len;
			} while ((bd_temp->flag & HIP5_OPT_BD_CHAIN_START));

			skb = alloc_skb(hip_signal->sig_len + data_len, GFP_ATOMIC);
			if (!skb) {
				SLSI_ERR_NODEV("Error allocating skb %d bytes\n", hip_signal->sig_len + data_len);
				/* SKB could not be allocated, but the BD still needs to be freed
				 * and the index needs updated correctly.
				 * Loop through the chain of BDs for which SKB was not allocated
				 */
				do {
					bd_temp = bulk_desc;

					/* Track buf_addr that should be freed */
					to_free[free++] = bulk_desc->buf_addr;
					bulk_desc++;
					if((void *) bulk_desc == ((void *)(&ctrl->q_tlv[2].array[MAX_NUM]))) {
						bulk_desc = (struct hip5_opt_bulk_desc *)(&ctrl->q_tlv[2].array[0]);
					}
					num_bd--;
				} while ((bd_temp->flag & HIP5_OPT_BD_CHAIN_START));
				continue;
			}

			memcpy(skb_put(skb, hip_signal->sig_len), sig_ptr, hip_signal->sig_len);

			cb = slsi_skb_cb_init(skb);
			cb->sig_length = hip_signal->sig_len;
			cb->data_length = cb->sig_length;

			do {
				bd_temp = bulk_desc;
				SLSI_DBG4(sdev, SLSI_HIP, "BD chain: buf_addr:0x%X, offset:%d, data_len:%d, flag:%d\n",
					bulk_desc->buf_addr,
					bulk_desc->offset,
					bulk_desc->data_len,
					bulk_desc->flag);

				mem = scsc_mx_service_mif_addr_to_ptr(service, bulk_desc->buf_addr);
				if (!mem) {
					SLSI_ERR_NODEV("bulk_desc->buf_addr NULL\n");
					SLSI_ERR_NODEV("T-H stall due to un-recoverable chained BD; trigger Panic\n");
					queue_work(sdev->device_wq, &sdev->trigger_wlan_fail_work);
					consume_skb(skb);
					return -EFAULT;
				}
				/* Track buf_addr that should be freed */
				to_free[free++] = bulk_desc->buf_addr;

				if (bulk_desc->data_len)
					fapi_append_data(skb, mem + bulk_desc->offset, bulk_desc->data_len);

				bulk_desc++;
				if((void *) bulk_desc == ((void *)(&ctrl->q_tlv[2].array[MAX_NUM - 1]) + 64)) {
					bulk_desc = (struct hip5_opt_bulk_desc *)(&ctrl->q_tlv[2].array[0]);
				}
				num_bd--;
			} while ((bd_temp->flag & HIP5_OPT_BD_CHAIN_START));

			if (hip_signal->wake_up && skb_queue_empty(skb_list)) {
				/* It is the first frame in the list that caused the wake-up */
				hip5_opt_log_wakeup_signal(sdev, skb, (conf == HIP5_MIF_Q_TH_CTRL) ? true : false);
			}
			__skb_queue_tail(skb_list, skb);
		}
	}

	/* update read index for chained HIP signals */
	if (hip_signal->sig_len >= 58) {
		/* TODO: signal length for more than 2 HIP entries */
		*idx_r = (*idx_r) + 1;
		*idx_r &= (MAX_NUM - 1);
	}

	if (hip_signal->num_bd > 3) {
		u8 i = 0;

		for (i = 0; i < (hip_signal->num_bd - 3); i = i + 8) {
			/* Increase index */
			*idx_r = (*idx_r) + 1;
			*idx_r &= (MAX_NUM - 1);
		}
	}
	return 0;
}


static struct sk_buff *hip5_bd_to_skb(struct slsi_dev *sdev, struct scsc_service *service, struct hip_priv *hip_priv, struct hip5_hip_entry *hip_entry, scsc_mifram_ref *to_free,
											u16 *idx_r, u16 *idx_w)
{
	struct slsi_hip         *hip;
	struct hip5_hip_control *ctrl;
	struct slsi_skb_cb      *cb;
	struct hip5_mbulk_bd    *bulk_desc;
	struct sk_buff          *skb = NULL;
	void                    *mem;
	u8                      free = 0;
	u8                      i = 0;
	u8                      num_bd = 0;
	u16                     idx_r_temp = 0;

	if (!hip_entry->num_bd) {
		SLSI_ERR_NODEV("Error no bulk descriptors\n");
		return NULL;
	}

	if (hip_entry->num_bd > HIP5_HIP_ENTRY_HEAD_MAX_NUM_BD) {
	    SLSI_ERR_NODEV("invalid num_bd %u\n", num_bd);
	    return NULL;
	}

	skb = alloc_skb(hip_entry->data_len + hip_entry->sig_len, GFP_ATOMIC);
	if (!skb) {
		SLSI_ERR_NODEV("Error allocating skb %d bytes\n", hip_entry->data_len + hip_entry->sig_len);
		return NULL;
	}

	cb = slsi_skb_cb_init(skb);
	cb->sig_length = hip_entry->sig_len;
	cb->data_length = hip_entry->data_len;

	/* Don't need to copy the 4Bytes header coming from the FW */
	memcpy(skb_put(skb, cb->sig_length), hip_entry->fapi_sig + 4, cb->sig_length);

	num_bd = hip_entry->num_bd;
	/* loop through HIP entries and bulk descriptors */
	while (num_bd)
	{
		mem = scsc_mx_service_mif_addr_to_ptr(service, hip_entry->bd[i].buf_addr);
		if (!mem) {
			SLSI_ERR_NODEV("bd->buf_addr NULL\n");
			consume_skb(skb);
			return NULL;
		}

		/* Track buf_addr that should be freed */
		to_free[free++] = hip_entry->bd[i].buf_addr;

		if (hip_entry->bd[i].len)
			fapi_append_data(skb, mem + hip_entry->bd[i].offset, hip_entry->bd[i].len);

		i++;
		num_bd--;
	}

	/* are there chained HIP entries */
	if (hip_entry->flag & HIP5_HIP_ENTRY_CHAIN_HEAD) {
		hip = hip_priv->hip;
		ctrl = hip->hip_control;

		do {
			i = 0;

			/* Should not read beyond the Write index
			 *
			 * Note: the below check assumes the chain is only 2 HIP entries.
			 * If in future, longer chains are used, this will need change.
			 */
			idx_r_temp = (*idx_r) + 1;
			idx_r_temp &= (MAX_NUM - 1);
			if (idx_r_temp == (*idx_w)) {
				SLSI_DBG1(sdev, SLSI_RX, "invalid Read index (r:%d, w:%d)\n", *idx_r, *idx_w);
				consume_skb(skb);
				return NULL;
			}

			/* Increase index */
			*idx_r = (*idx_r) + 1;
			*idx_r &= (MAX_NUM - 1);

			hip_entry = (struct hip5_hip_entry *)(&ctrl->q_tlv[2].array[*idx_r]);

			if (!hip_entry->flag) {
				SLSI_ERR_NODEV("invalid flag in a chained MBULK\n");
				queue_work(sdev->device_wq, &sdev->trigger_wlan_fail_work);
				consume_skb(skb);
				return NULL;
			}

			num_bd = hip_entry->num_bd;
			if (num_bd > HIP5_HIP_ENTRY_CHAIN_MAX_NUM_BD) {
			    SLSI_ERR_NODEV("invalid num_bd %u\n", num_bd);
			    queue_work(sdev->device_wq, &sdev->trigger_wlan_fail_work);
			    consume_skb(skb);
			    return NULL;
			}

			while (num_bd) {
				if (free > MBULK_MAX_CHAIN) {
					SLSI_ERR_NODEV("chain length exceeds MAX: trigger firmware moredump\n");
					queue_work(sdev->device_wq, &sdev->trigger_wlan_fail_work);
					consume_skb(skb);
					return NULL;
				}

				/* if there are more than 2 BDs in a chained HIP entry, then
				 * the 3rd BD and onwards is filled in field hip_entry->bdx[]
				 */
				if (i > 1)
					bulk_desc = &(hip_entry->bdx[i - 2]);
				else
					bulk_desc = &(hip_entry->bd[i]);

				mem = scsc_mx_service_mif_addr_to_ptr(service, bulk_desc->buf_addr);

				/* Track buf_addr that should be freed */
				to_free[free++] = bulk_desc->buf_addr;

				if (bulk_desc->len)
					fapi_append_data(skb, mem + bulk_desc->offset, bulk_desc->len);

				i++;
				num_bd--;
			}
		} while (!(hip_entry->flag & HIP5_HIP_ENTRY_CHAIN_END));
	}

	return skb;
}

/* Transform mbulk to skb (fapi_signal + payload) */
static struct sk_buff *hip5_mbulk_to_skb(struct scsc_service *service, struct hip_priv *hip_priv, struct mbulk *m, scsc_mifram_ref *to_free, bool atomic)
{
	struct slsi_skb_cb        *cb;
	struct mbulk              *next_mbulk[MBULK_MAX_CHAIN];
	struct sk_buff            *skb = NULL;
	scsc_mifram_ref           ref;
	scsc_mifram_ref           m_chain_next;
	u8                        free = 0;
	u8                        i = 0, j = 0;
	u8                        *p;
	size_t                    bytes_to_alloc = 0;

	/* Get the mif ref pointer, check for incorrect mbulk */
	if (scsc_mx_service_mif_ptr_to_addr(service, m, &ref)) {
		SLSI_ERR_NODEV("mbulk address conversion failed\n");
		return NULL;
	}

	/* Track mbulk that should be freed */
	to_free[free++] = ref;

	bytes_to_alloc += m->sig_bufsz;
	bytes_to_alloc += m->len;

	/* Detect Chained mbulk to start building the chain */
	if ((MBULK_SEG_IS_CHAIN_HEAD(m)) && (MBULK_SEG_IS_CHAINED(m))) {
		m_chain_next = mbulk_chain_next(m);
		if (!m_chain_next) {
			SLSI_ERR_NODEV("Mbulk is set MBULK_F_CHAIN_HEAD and MBULK_F_CHAIN but m_chain_next is NULL\n");
			goto cont;
		}
		while (1) {
			/* increase number mbulks in chain */
			i++;
			/* Get next_mbulk kernel address space pointer  */
			next_mbulk[i - 1] = scsc_mx_service_mif_addr_to_ptr(service, m_chain_next);
			if (!next_mbulk[i - 1]) {
				SLSI_ERR_NODEV("First Mbulk is set as MBULK_F_CHAIN but next_mbulk is NULL\n");
				return NULL;
			}
			/* Track mbulk to be freed */
			to_free[free++] = m_chain_next;
			bytes_to_alloc += next_mbulk[i - 1]->len;
			if (MBULK_SEG_IS_CHAINED(next_mbulk[i - 1])) {
				/* continue traversing the chain */
				m_chain_next = mbulk_chain_next(next_mbulk[i - 1]);
				if (!m_chain_next)
					break;

				if (i >= MBULK_MAX_CHAIN) {
					SLSI_ERR_NODEV("Max number of chained MBULK reached\n");
					return NULL;
				}
			} else {
				break;
			}
		}
	}

cont:
	if (atomic) {
		skb = alloc_skb(bytes_to_alloc, GFP_ATOMIC);
	} else {
		spin_unlock_bh(&hip_priv->rx_lock);
		skb = alloc_skb(bytes_to_alloc, GFP_KERNEL);
		spin_lock_bh(&hip_priv->rx_lock);
	}
	if (!skb) {
		SLSI_ERR_NODEV("Error allocating skb %d bytes\n", bytes_to_alloc);
		return NULL;
	}

	cb = slsi_skb_cb_init(skb);
	cb->sig_length = m->sig_bufsz;
	/* fapi_data_append adds to the data_length */
	cb->data_length = cb->sig_length;

	p = mbulk_get_signal(m);
	if (!p) {
		SLSI_ERR_NODEV("No signal in Mbulk\n");
		print_hex_dump(KERN_ERR, SCSC_PREFIX "mbulk ", DUMP_PREFIX_NONE, 16, 1, m, sizeof(struct mbulk), 0);
		kfree_skb(skb);
		return NULL;
	}

	/* Remove 4Bytes offset coming from FW */
	p += 4;

	/* Don't need to copy the 4Bytes header coming from the FW */
	memcpy(skb_put(skb, cb->sig_length), p, cb->sig_length);

	if (m->len) {
		fapi_append_data(skb, mbulk_dat_r(m), m->len);
	}
	for (j = 0; j < i; j++) {
		fapi_append_data(skb, mbulk_dat_r(next_mbulk[j]), next_mbulk[j]->len);
	}

	return skb;
}

static int hip5_opt_hip_signal_add(struct slsi_hip *hip, enum hip5_hip_q_conf conf, scsc_mifram_ref phy_m, struct scsc_service *service, void *m_ptr, struct sk_buff *skb_fapi)
{
	struct hip5_hip_control  *ctrl = hip->hip_control;
	struct hip_priv          *hip_priv = hip->hip_priv;
	u16                       idx_w;
	u16                       idx_r;
	struct mbulk             *m = m_ptr;
	struct hip5_opt_hip_signal *hip5_q_entry = NULL;
	struct hip5_opt_bulk_desc        *bd = NULL;
	u8                        padding;

	/* Read the current q write pointer */
	idx_w = hip5_read_index(hip, conf, widx);
	/* Read the current q read pointer */
	idx_r = hip5_read_index(hip, conf, ridx);

	if ((idx_r == 0xFFFF) || (idx_w == 0xFFFF)) {
		SLSI_ERR_NODEV("invalid Index\n");
		return -EINVAL;
	}

	if (idx_w >= MAX_NUM) {
		SLSI_ERR_NODEV("invalid Write index (q:%d, idx_w:%d)\n", conf, idx_w);
		return -EINVAL;
	}

	SCSC_HIP4_SAMPLER_Q(hip_priv->minor, conf, widx, idx_w, 1);
	SCSC_HIP4_SAMPLER_Q(hip_priv->minor, conf, ridx, idx_r, 1);

	/* Is queue full? */
	if (idx_r == ((idx_w + 1) & (MAX_NUM - 1))) {
		SLSI_ERR_NODEV("no space\n");
		return -ENOSPC;
	}

	/* build the HIP entry */
	if (conf == HIP5_MIF_Q_FH_CTRL)
		hip5_q_entry = (struct hip5_opt_hip_signal *)&ctrl->q_tlv[3].array[idx_w];
	else if (conf == HIP5_MIF_Q_FH_DAT0)
		hip5_q_entry = (struct hip5_opt_hip_signal *)&ctrl->q_tlv[0].array[idx_w];
	else
		hip5_q_entry = (struct hip5_opt_hip_signal *)&ctrl->q_tlv[1].array[idx_w];

	/* HIP signal header */
	hip5_q_entry->sig_format = 0; /* not in use */
	hip5_q_entry->reserved = 0;   /* not in use */
	if (m_ptr)
		hip5_q_entry->num_bd = 1;
	else
		hip5_q_entry->num_bd = 0;
	hip5_q_entry->sig_len = skb_fapi->len;

	/* FAPI signal */
	memcpy((void *)hip5_q_entry + 4, skb_fapi->data, skb_fapi->len);

	/* padding for bulk descriptor to start on 8 byte aligned */
	padding = (8 - ((skb_fapi->len + 4)  % 8)) & 0x7;

	bd = (struct hip5_opt_bulk_desc *) ((void *)hip5_q_entry + 4 + skb_fapi->len + padding);

	/* check for wrap-around if large signal length will need more than 1 HIP entries */
	if (conf == HIP5_MIF_Q_FH_CTRL)
		if((void *) bd >= ((void *)&ctrl->q_tlv[3].array[MAX_NUM])) {
			bd = (struct hip5_opt_bulk_desc *)&ctrl->q_tlv[3].array[0];
		}

	/* bulk descriptor(s) */
	if (m_ptr) {
		bd->buf_addr = phy_m;
		bd->buf_sz = 2; /* 512 * (1<<buf_sz) */
		bd->data_len = m->len;
		bd->flag = m->flag;
		bd->offset = m->head + 20;
	}

	/* Memory barrier before updating shared mailbox */
	smp_wmb();
	SCSC_HIP4_SAMPLER_QREF(hip_priv->minor, phy_m, conf);

	/* Increase index */
	idx_w++;
	idx_w &= (MAX_NUM - 1);

	if ((HIP5_OPT_HIP_HEADER_LEN + hip5_q_entry->sig_len + padding + (hip5_q_entry->num_bd * sizeof(struct hip5_opt_bulk_desc))) >
		(sizeof(struct hip5_opt_hip_signal))) {
		/* TODO: handle signal length that can be larger than 2 HIP entries;
		 * there is no such signal now.
		 */
		idx_w++;
		idx_w &= (MAX_NUM - 1);
	}

	/* Update the scoreboard */
	hip5_update_index(hip, conf, widx, idx_w);

	send = ktime_get();
	if (conf == HIP5_MIF_Q_FH_CTRL)
		scsc_service_mifintrbit_bit_set(service, hip_priv->intr_from_host_ctrl, SCSC_MIFINTR_TARGET_WLAN);

	return 0;
}

/* Add signal reference (offset in shared memory) in the selected queue */
/* This function should be called in atomic context. Callers should supply proper locking mechanism */
static int hip5_q_add_bd_entry(struct slsi_hip *hip, enum hip5_hip_q_conf conf, scsc_mifram_ref phy_m, struct scsc_service *service, void *m_ptr, void *sig_ptr)
{
	struct hip5_hip_control *ctrl = hip->hip_control;
	struct hip_priv         *hip_priv = hip->hip_priv;
	u16                      idx_w;
	u16                      idx_r;
	struct mbulk            *m = m_ptr;
	struct hip5_hip_entry *hip5_q_entry = NULL;

	/* Read the current q write pointer */
	idx_w = hip5_read_index(hip, conf, widx);
	/* Read the current q read pointer */
	idx_r = hip5_read_index(hip, conf, ridx);

	if ((idx_r >= MAX_NUM) || (idx_w >= MAX_NUM)) {
		SLSI_WARN_NODEV("invalid scoreboard index (r:%d, w:%d)\n", idx_r, idx_w);
		return -EINVAL;
	}

	SCSC_HIP4_SAMPLER_Q(hip_priv->minor, conf, widx, idx_w, 1);
	SCSC_HIP4_SAMPLER_Q(hip_priv->minor, conf, ridx, idx_r, 1);

	/* Is queue full? */
	if (idx_r == ((idx_w + 1) & (MAX_NUM - 1))) {
		SLSI_ERR_NODEV("no space\n");
		return -ENOSPC;
	}

	/* build the HIP entry */
	if (conf == HIP5_MIF_Q_FH_DAT0)
		hip5_q_entry = &ctrl->q_tlv[0].array[idx_w];
	else
		hip5_q_entry = &ctrl->q_tlv[1].array[idx_w];

	hip5_q_entry->num_bd = 1; 								/* number of BD in the HIP entry (always 1) */
	hip5_q_entry->flag = 0;	 								/* Bit0: Chain HEAD, Bit1: Chain continue, Bit2: End of chain; No chaining in from-host */
	hip5_q_entry->sig_len = fapi_sig_size(ma_unitdata_req);	/* 0 or signal length defined in FAPI, 0: signal omitted */

	hip5_q_entry->data_len = m->len;

	/* bulk descriptor	*/
	hip5_q_entry->bd[0].buf_addr = phy_m;
	hip5_q_entry->bd[0].buf_sz = HIP5_DAT_MBULK_SIZE;
	hip5_q_entry->bd[0].extra = 0;
	hip5_q_entry->bd[0].flag = m->flag;
	hip5_q_entry->bd[0].len = m->len;
	hip5_q_entry->bd[0].offset = m->head + 20;

	/* FAPI signal */
	/* the size of MA-unitdata.request signal is 34 bytes.
	 * And the signal starts at offset 32.
	 * So the whole signal can't fit into 64 bytes of HIP entry.
	 * The fix for now is to not write the last 4 bytes ie.
	 * not write the "spare_3" field which is not used now.
	 */
	memcpy(hip5_q_entry->fapi_sig + 4, sig_ptr, fapi_sig_size(ma_unitdata_req) - 4);
	/* Memory barrier before updating shared mailbox */
	dma_wmb();
	SCSC_HIP4_SAMPLER_QREF(hip_priv->minor, phy_m, conf);

	/* Increase index */
	idx_w++;
	idx_w &= (MAX_NUM - 1);

	/* Update the scoreboard */
	hip5_update_index(hip, conf, widx, idx_w);

	send = ktime_get();
#ifndef CONFIG_SCSC_WLAN_TX_API
	scsc_service_mifintrbit_bit_set(service, hip_priv->intr_from_host_ctrl, SCSC_MIFINTR_TARGET_WLAN);
#endif
	return 0;
}

static int hip5_q_add_signal(struct slsi_hip *hip, enum hip5_hip_q_conf conf, scsc_mifram_ref phy_m, struct scsc_service *service)
{
	struct hip5_hip_control *ctrl = hip->hip_control;
	struct hip_priv        *hip_priv = hip->hip_priv;
	u16                      idx_w;
	u16                      idx_r;

	/* Read the current q write pointer */
	idx_w = hip5_read_index(hip, conf, widx);
	/* Read the current q read pointer */
	idx_r = hip5_read_index(hip, conf, ridx);

	if ((idx_r >= MAX_NUM) || (idx_w >= MAX_NUM)) {
		SLSI_WARN_NODEV("invalid scoreboard index (r:%d, w:%d)\n", idx_r, idx_w);
		return -EINVAL;
	}

	SCSC_HIP4_SAMPLER_Q(hip_priv->minor, conf, widx, idx_w, 1);
	SCSC_HIP4_SAMPLER_Q(hip_priv->minor, conf, ridx, idx_r, 1);

	/* Queue is full */
	if (idx_r == ((idx_w + 1) & (MAX_NUM - 1))) {
		SLSI_INFO_NODEV("q[%d] is full\n", conf);
		return -ENOSPC;
	}

	/* Update array */
	ctrl->q[conf].array[idx_w] = phy_m;
	/* Memory barrier before updating shared mailbox */
	dma_wmb();
	SCSC_HIP4_SAMPLER_QREF(hip_priv->minor, phy_m, conf);

	/* Increase index */
	idx_w++;
	idx_w &= (MAX_NUM - 1);

	/* Update the scoreboard */
	hip5_update_index(hip, conf, widx, idx_w);

	send = ktime_get();
	scsc_service_mifintrbit_bit_set(service, hip_priv->intr_from_host_ctrl, SCSC_MIFINTR_TARGET_WLAN);

	return 0;
}

static void hip5_wq_ctrl(struct work_struct *data)
{
	struct hip_priv        *hip_priv = slsi_lbm_get_hip_priv_from_work(data);
	struct slsi_hip        *hip;
	struct hip5_hip_control *ctrl;
	struct scsc_service     *service;
	struct slsi_dev         *sdev;
	u8						retry;
	bool                    no_change = true;
	u16                      idx_r;
	u16                      idx_w;
	scsc_mifram_ref         ref;
	void                    *mem;
	struct mbulk            *m;

	if (!hip_priv || !hip_priv->hip) {
		SLSI_ERR_NODEV("hip_priv or hip_priv->hip is Null\n");
		return;
	}

	hip = hip_priv->hip;
	ctrl = hip->hip_control;

	if (!ctrl) {
		SLSI_ERR_NODEV("hip->hip_control is Null\n");
		return;
	}
	sdev = container_of(hip, struct slsi_dev, hip);

	if (!sdev || !sdev->service) {
		SLSI_ERR_NODEV("sdev or sdev->service is Null\n");
		return;
	}

#if defined(CONFIG_SCSC_PCIE_CHIP)
	if (scsc_mx_service_claim(WLAN_RX_CTRL)) {
		SLSI_ERR(sdev, "failed to claim the MX service\n");
		return;
	}
#endif

	spin_lock_bh(&hip_priv->rx_lock);
	service = sdev->service;
	SCSC_HIP4_SAMPLER_INT_BH(hip->hip_priv->minor, 1);
	bh_init_ctrl = ktime_get();

	idx_r = hip5_read_index(hip, HIP5_MIF_Q_TH_CTRL, ridx);
	idx_w = hip5_read_index(hip, HIP5_MIF_Q_TH_CTRL, widx);

	if ((idx_r >= MAX_NUM) || (idx_w >= MAX_NUM)) {
		SLSI_WARN(sdev, "invalid scoreboard index (r:%d, w:%d)\n", idx_r, idx_w);
		bh_end_ctrl = ktime_get();
		spin_unlock_bh(&hip_priv->rx_lock);
#if defined(CONFIG_SCSC_PCIE_CHIP)
		scsc_mx_service_release(WLAN_RX_CTRL);
#endif
		return;
	}
	SLSI_DBG2(sdev, SLSI_HIP, "r:%d w:%d todo:%hhu\n", idx_r, idx_w, ((idx_w - idx_r) & (MAX_NUM - 1)));

	if (idx_r != idx_w) {
		SCSC_HIP4_SAMPLER_Q(hip_priv->minor, HIP5_MIF_Q_TH_CTRL, ridx, idx_r, 1);
		SCSC_HIP4_SAMPLER_Q(hip_priv->minor, HIP5_MIF_Q_TH_CTRL, widx, idx_w, 1);
	}
	while (idx_r != idx_w) {
		struct sk_buff *skb;
		/* TODO: currently the max number to be freed is 2. In future
		 * implementations (i.e. AMPDU) this number may be bigger
		 * list of mbulks to be freedi
		 */
		scsc_mifram_ref to_free[MBULK_MAX_CHAIN + 1] = { 0 };
		u8              i = 0;
		no_change = false;
		/* Catch-up with idx_w */
		dma_rmb();

		if (hip_priv->version == 5) {
			struct sk_buff_head skb_list;

			__skb_queue_head_init(&skb_list);

			if (hip5_opt_hip_signal_to_skb(sdev, service, hip_priv, ctrl, HIP5_MIF_Q_TH_CTRL, to_free, &idx_r, &idx_w, &skb_list)) {
				SLSI_ERR_NODEV("Ctrl: Error parsing or allocating skb\n");
				hip5_dump_dbg(hip, NULL, NULL, service);
				goto consume_ctl_mbulk;
			}

			while (!skb_queue_empty(&skb_list)) {
				struct sk_buff *rx_skb;

				rx_skb = __skb_dequeue(&skb_list);
				if (slsi_hip_rx(sdev, rx_skb) < 0) {
					SLSI_ERR_NODEV("Ctrl: Error detected slsi_hip_rx\n");
					hip5_dump_dbg(hip, NULL, rx_skb, service);
					kfree_skb(rx_skb);
				}
			}
			goto consume_ctl_mbulk;
		} else {
				ref = ctrl->q[HIP5_MIF_Q_TH_CTRL].array[idx_r];
#ifdef CONFIG_SCSC_WLAN_HIP5_PROFILING
				SCSC_HIP4_SAMPLER_QREF(hip_priv->minor, ref, HIP5_MIF_Q_TH_CTRL);
#endif
				mem = scsc_mx_service_mif_addr_to_ptr(service, ref);
				m = (struct mbulk *)(mem);
				/* Process Control Signal */
				skb = hip5_mbulk_to_skb(service, hip_priv, m, to_free, false);
				if (!skb) {
					SLSI_ERR_NODEV("Ctrl: Error parsing or allocating skb\n");
					hip5_dump_dbg(hip, m, skb, service);
					goto consume_ctl_mbulk;
				}

				if (m->flag & MBULK_F_WAKEUP) {
					SLSI_INFO(sdev, "Wi-Fi wakeup by MLME frame 0x%x:\n", fapi_get_sigid(skb));
					SCSC_BIN_TAG_INFO(BINARY, skb->data, skb->len > 128 ? 128 : skb->len);
					slsi_skb_cb_get(skb)->wakeup = true;
				}
#if defined(CONFIG_SCSC_WLAN_DEBUG) || defined(CONFIG_SCSC_WLAN_HIP5_PROFILING)
			id = fapi_get_sigid(skb);
#endif
#ifdef CONFIG_SCSC_WLAN_HIP5_PROFILING
			/* log control signal, not unidata not debug  */
			if (fapi_is_mlme(skb))
				SCSC_HIP4_SAMPLER_SIGNAL_CTRLRX(hip_priv->minor, (id & 0xff00) >> 8, id & 0xff);
#endif
#ifdef CONFIG_SCSC_WLAN_DEBUG
			hip4_history_record_add(TH, id);
#endif
			if (slsi_hip_rx(sdev, skb) < 0) {
				SLSI_ERR_NODEV("Ctrl: Error detected slsi_hip_rx\n");
				hip5_dump_dbg(hip, m, skb, service);
				kfree_skb(skb);
			}
		}
consume_ctl_mbulk:
		/* Increase index */
		idx_r++;
		idx_r &= (MAX_NUM - 1);

		/* Go through the list of references to free */
		while ((ref = to_free[i++])) {
			/* Set the number of retries */
			retry = FB_NO_SPC_NUM_RET;
			/* return to the firmware */
			while (hip5_q_add_signal(hip, HIP5_MIF_Q_TH_RFBC, ref, service) && (!atomic_read(&hip->hip_priv->closing)) && (retry > 0)) {
				SLSI_WARN_NODEV("Ctrl: Not enough space in FB, retry of %d/%d\n",
						((FB_NO_SPC_NUM_RET + 1) - retry), FB_NO_SPC_NUM_RET);
				spin_unlock_bh(&hip_priv->rx_lock);
				msleep(FB_NO_SPC_SLEEP_MS);
				spin_lock_bh(&hip_priv->rx_lock);
				retry--;
				if (retry == 0)
					SLSI_ERR_NODEV("Ctrl: FB has not been freed for %d ms\n", FB_NO_SPC_NUM_RET * FB_NO_SPC_SLEEP_MS);
				SCSC_HIP4_SAMPLER_QFULL(hip_priv->minor, HIP5_MIF_Q_TH_RFBC);
			}
		}
		hip5_update_index(hip, HIP5_MIF_Q_TH_CTRL, ridx, idx_r);
	}

	if (slsi_wake_lock_active(&hip->hip_priv->wake_lock_ctrl))
		slsi_wake_unlock(&hip->hip_priv->wake_lock_ctrl);
	if (!atomic_read(&hip->hip_priv->closing))
		scsc_service_mifintrbit_bit_unmask(sdev->service, hip->hip_priv->intr_to_host_ctrl);
	SCSC_HIP4_SAMPLER_INT_OUT_BH(hip->hip_priv->minor, 1);
	bh_end_ctrl = ktime_get();
	spin_unlock_bh(&hip_priv->rx_lock);
#if defined(CONFIG_SCSC_PCIE_CHIP)
	scsc_mx_service_release(WLAN_RX_CTRL);
#endif
}

static void hip5_irq_handler_stub(int irq, void *data)
{
	struct slsi_hip    *hip = (struct slsi_hip *)data;
	struct slsi_dev     *sdev = container_of(hip, struct slsi_dev, hip);

	if (atomic_read(&hip->hip_priv->closing)) {
		SLSI_ERR_NODEV("interrupt received while hip is closed: %d\n", irq);
		scsc_service_mifintrbit_bit_mask(sdev->service, irq);
		scsc_service_mifintrbit_bit_clear(sdev->service, irq);
		return;
	}

	SLSI_ERR_NODEV("interrupt received: %d\n", irq);
	scsc_service_mifintrbit_bit_mask(sdev->service, irq);

	/* Clear interrupt */
	scsc_service_mifintrbit_bit_clear(sdev->service, irq);
}

static void hip5_irq_handler_ctrl(int irq, void *data)
{
	struct slsi_hip    *hip = (struct slsi_hip *)data;
	struct slsi_dev     *sdev = container_of(hip, struct slsi_dev, hip);

	if (atomic_read(&hip->hip_priv->closing)) {
		SLSI_ERR_NODEV("interrupt received while hip is closed: %d\n", irq);
		scsc_service_mifintrbit_bit_mask(sdev->service, irq);
		scsc_service_mifintrbit_bit_clear(sdev->service, irq);
		return;
	}

	SCSC_HIP4_SAMPLER_INT(hip->hip_priv->minor, 1);
	intr_received_ctrl = ktime_get();
	if (!slsi_wake_lock_active(&hip->hip_priv->wake_lock_ctrl)) {
		slsi_wake_lock_timeout(&hip->hip_priv->wake_lock_ctrl, msecs_to_jiffies(SLSI_HIP_WAKELOCK_TIME_OUT_IN_MS));
	}

	scsc_service_mifintrbit_bit_mask(sdev->service, irq);
	slsi_lbm_run_bh(hip->hip_priv->bh_ctl);

	/* Clear interrupt */
	scsc_service_mifintrbit_bit_clear(sdev->service, irq);
	SCSC_HIP4_SAMPLER_INT_OUT(hip->hip_priv->minor, 1);
}

void slsi_hip_sched_wq_ctrl(struct slsi_hip *hip)
{
	struct slsi_dev *sdev = NULL;

	if (!hip || !hip->hip_priv)
		return;

	sdev = container_of(hip, struct slsi_dev, hip);
	if (!sdev || !sdev->service)
		return;

	if (atomic_read(&hip->hip_priv->closing))
		return;

	hip5_dump_dbg(hip, NULL, NULL, sdev->service);

	if (!slsi_wake_lock_active(&hip->hip_priv->wake_lock_ctrl))
		slsi_wake_lock_timeout(&hip->hip_priv->wake_lock_ctrl,
				       msecs_to_jiffies(SLSI_HIP_WAKELOCK_TIME_OUT_IN_MS));

	SLSI_DBG1(sdev, SLSI_HIP, "Trigger wq for skipped ctrl BH\n");

	slsi_lbm_run_bh(hip->hip_priv->bh_ctl);
}

#if defined(CONFIG_SCSC_PCIE_CHIP)
int hip5_fb_mx_claim_complete_cb(void *service, void *data)
{
	struct hip_priv *hip_priv = (struct hip_priv *)data;

	if (!hip_priv) {
		SLSI_ERR_NODEV("invalid hip_priv\n");
		return -EINVAL;
	}
	local_bh_disable();
	hip_priv->mx_pci_claim_fb = true;
	slsi_lbm_run_bh(hip_priv->bh_rfb);
	local_bh_enable();
	return 0;
}
#endif

static mbulk_colour hip5_opt_zero_copy_free_buffer(struct hip_priv *hip_priv, u32 addr)
{
	u16 i;

	if (!addr) {
		SLSI_ERR_NODEV("addr is NULL\n");
		return 0;
	}

	/* find the entry in SKB look-up table */
	for (i = 0; i < MAX_NUM; i++)
		if (hip_priv->tx_skb_table[i].in_use &&
			hip_priv->tx_skb_table[i].skb_dma_addr == addr)
			break;

	if (i < MAX_NUM) {
		consume_skb(hip_priv->tx_skb_table[i].skb);
		hip_priv->tx_skb_table[i].in_use = false;
		hip_priv->tx_skb_free_cnt++;
		SCSC_HIP4_SAMPLER_MBULK(hip_priv->minor, (hip_priv->tx_skb_free_cnt & 0xFF00) >> 8, (hip_priv->tx_skb_free_cnt & 0xFF), MBULK_POOL_ID_DATA, hip5_tx_zero_copy_tx_slots);
		return hip_priv->tx_skb_table[i].colour;
	}
	return 0;
}

static void hip5_tl_fb(unsigned long data)
{
	struct slsi_hip		*hip = (void *)data;
	struct hip_priv		*hip_priv = hip->hip_priv;
	struct hip5_hip_control *ctrl;
	struct scsc_service 	*service;
	struct slsi_dev 		*sdev;
	u16						idx_r;
	u16						idx_w;
	mbulk_colour            colour = 0;
	struct mbulk            *m;
	scsc_mifram_ref			ref;
	void                    *mem;

	if (!hip_priv || !hip_priv->hip) {
		SLSI_ERR_NODEV("hip_priv or hip_priv->hip is Null\n");
		return;
	}

	hip = hip_priv->hip;
	ctrl = hip->hip_control;

	if (!ctrl) {
		SLSI_ERR_NODEV("hip->hip_control is Null\n");
		return;
	}
	sdev = container_of(hip, struct slsi_dev, hip);

	if (!sdev || !sdev->service) {
		SLSI_ERR_NODEV("sdev or sdev->service is Null\n");
		return;
	}

	/* synchronize with TX as it shares resources e.g tx_skb_table */
	spin_lock_bh(&hip_priv->tx_lock);
	service = sdev->service;
#if defined(CONFIG_SCSC_PCIE_CHIP)
	if (!hip_priv->mx_pci_claim_fb) {
		if (scsc_mx_service_claim_deferred(sdev->service, hip5_fb_mx_claim_complete_cb, (void *)hip_priv, WLAN_RB)) {
			spin_unlock_bh(&hip_priv->tx_lock);
			return;
		}
		hip_priv->mx_pci_claim_fb = true;}
#endif
	SCSC_HIP4_SAMPLER_INT_BH(hip->hip_priv->minor, 2);
	bh_init_fb = ktime_get();

	idx_r = hip5_read_index(hip, HIP5_MIF_Q_FH_RFBC, ridx);
	idx_w = hip5_read_index(hip, HIP5_MIF_Q_FH_RFBC, widx);

	if ((idx_r >= MAX_NUM) || (idx_w >= MAX_NUM)) {
		SLSI_WARN(sdev, "invalid scoreboard index (r:%d, w:%d)\n", idx_r, idx_w);
#if defined(CONFIG_SCSC_PCIE_CHIP)
		if (hip->hip_priv->mx_pci_claim_fb) {
			hip->hip_priv->mx_pci_claim_fb = false;
			scsc_mx_service_release(WLAN_RB);
		}
#endif
		bh_end_fb = ktime_get();
		spin_unlock_bh(&hip_priv->tx_lock);
		return;
	}

	if (idx_r != idx_w) {
		SCSC_HIP4_SAMPLER_Q(hip_priv->minor, HIP5_MIF_Q_FH_RFBC, ridx, idx_r, 1);
		SCSC_HIP4_SAMPLER_Q(hip_priv->minor, HIP5_MIF_Q_FH_RFBC, widx, idx_w, 1);
	}
	while (idx_r != idx_w) {
		ref = ctrl->q[HIP5_MIF_Q_FH_RFBC].array[idx_r];
#ifdef CONFIG_SCSC_WLAN_HIP5_PROFILING
		SCSC_HIP4_SAMPLER_QREF(hip_priv->minor, ref, HIP5_MIF_Q_FH_RFBC);
#endif
		if (ref & 0x1) {
			colour = hip5_opt_zero_copy_free_buffer(hip_priv, ref);
		} else {
			mem = scsc_mx_service_mif_addr_to_ptr(service, ref);
			m = (struct mbulk *)mem;

			if (!m) {
				SLSI_ERR_NODEV("FB: Mbulk is NULL\n");
				goto consume_fb_mbulk;
			}

			/* Account ONLY for data RFB
			 * If the returned mbulk is for control pool, the call below returns 0
			 */
			colour = mbulk_get_colour(MBULK_POOL_ID_DATA, m);
			mbulk_free_virt_host(m);
		}

		/* Ignore return value */
#ifdef CONFIG_SCSC_WLAN_TX_API
		slsi_hip_tx_done(sdev, colour, (idx_r + 1 & (MAX_NUM - 1)) != idx_w);
#else
		if (colour)
			slsi_hip_tx_done(sdev,
					 SLSI_MBULK_COLOUR_GET_VIF(colour),
					 SLSI_MBULK_COLOUR_GET_PEER_IDX(colour),
					 SLSI_MBULK_COLOUR_GET_AC(colour));
#endif
consume_fb_mbulk:
		/* Increase index */
		idx_r++;
		idx_r &= (MAX_NUM - 1);
		hip5_update_index(hip, HIP5_MIF_Q_FH_RFBC, ridx, idx_r);
	}

	if (slsi_wake_lock_active(&hip->hip_priv->wake_lock_tx))
		slsi_wake_unlock(&hip->hip_priv->wake_lock_tx);
	if (!atomic_read(&hip->hip_priv->closing))
		scsc_service_mifintrbit_bit_unmask(sdev->service, hip->hip_priv->intr_to_host_ctrl_fb);
	SCSC_HIP4_SAMPLER_INT_OUT_BH(hip->hip_priv->minor, 2);
#if defined(CONFIG_SCSC_PCIE_CHIP)
	if (hip->hip_priv->mx_pci_claim_fb) {
		hip->hip_priv->mx_pci_claim_fb = false;
		scsc_mx_service_release(WLAN_RB);}
#endif
	bh_end_fb = ktime_get();
	spin_unlock_bh(&hip_priv->tx_lock);
}

static void hip5_irq_handler_fb(int irq, void *data)
{
	struct slsi_hip    *hip = (struct slsi_hip *)data;
	struct slsi_dev     *sdev = container_of(hip, struct slsi_dev, hip);

	if (atomic_read(&hip->hip_priv->closing)) {
		SLSI_ERR_NODEV("interrupt received while hip is closed: %d\n", irq);
		scsc_service_mifintrbit_bit_mask(sdev->service, irq);
		scsc_service_mifintrbit_bit_clear(sdev->service, irq);
		return;
	}

	SCSC_HIP4_SAMPLER_INT(hip->hip_priv->minor, 2);
	intr_received_fb = ktime_get();

	if (!slsi_wake_lock_active(&hip->hip_priv->wake_lock_tx)) {
		slsi_wake_lock_timeout(&hip->hip_priv->wake_lock_tx, msecs_to_jiffies(SLSI_HIP_WAKELOCK_TIME_OUT_IN_MS));
	}
	scsc_service_mifintrbit_bit_mask(sdev->service, irq);
	slsi_lbm_run_bh(hip->hip_priv->bh_rfb);

	/* Clear interrupt */
	scsc_service_mifintrbit_bit_clear(sdev->service, irq);
	SCSC_HIP4_SAMPLER_INT_OUT(hip->hip_priv->minor, 2);
}

static void hip5_napi_complete(struct slsi_dev *sdev, struct napi_struct *napi, struct slsi_hip *hip)
{
	bh_end_data = ktime_get();
	napi_complete(napi);

	if (slsi_wake_lock_active(&hip->hip_priv->wake_lock_data))
		slsi_wake_unlock(&hip->hip_priv->wake_lock_data);
	if (!atomic_read(&hip->hip_priv->closing))
		scsc_service_mifintrbit_bit_unmask(sdev->service, hip->hip_priv->intr_to_host_data1);

#if defined(CONFIG_SCSC_PCIE_CHIP)
	if (hip->hip_priv->mx_pci_claim_data) {
		hip->hip_priv->mx_pci_claim_data = false;
		scsc_mx_service_release(WLAN_RX_DATA);}
#endif
}

#if defined(CONFIG_SCSC_PCIE_CHIP)
int hip5_napi_mx_claim_complete_cb(void *service, void *data)
{
	struct hip_priv *hip_priv = (struct hip_priv *)data;

	if (!hip_priv) {
		SLSI_ERR_NODEV("invalid hip_priv\n");
		return -EINVAL;
	}

	local_bh_disable();
	hip_priv->mx_pci_claim_data = true;
	slsi_lbm_run_bh(hip_priv->bh_dat);
	local_bh_enable();
	return 0;
	}
#endif

static int hip5_napi_poll(struct napi_struct *napi, int budget)
{
	struct hip_priv        *hip_priv = slsi_lbm_get_hip_priv_from_napi(napi);
	struct slsi_hip        *hip;
	struct hip5_hip_control *ctrl;
	struct scsc_service     *service;
	struct slsi_dev         *sdev;

#ifdef CONFIG_SCSC_WLAN_DEBUG
	int                     id;
#endif
	u16                      idx_r;
	u16                      idx_w;
	scsc_mifram_ref         ref;
	struct hip5_hip_entry   *hip_entry;
	u8                      retry;
	u16                     todo = 0;
	int                     work_done = 0;

	if (!hip_priv || !hip_priv->hip) {
		SLSI_ERR_NODEV("hip_priv or hip_priv->hip is Null\n");
		return 0;
	}

	hip = hip_priv->hip;
	if (!hip || !hip->hip_priv) {
		SLSI_ERR_NODEV("either hip or hip->hip_priv is Null\n");
		return 0;
	}

	ctrl = hip->hip_control;

	if (!ctrl) {
		SLSI_ERR_NODEV("hip->hip_control is Null\n");
		return 0;
	}
	sdev = container_of(hip, struct slsi_dev, hip);

	if (!sdev || !sdev->service) {
		SLSI_ERR_NODEV("sdev or sdev->service is Null\n");
		return 0;
	}

	if (atomic_read(&sdev->hip.hip_state) != SLSI_HIP_STATE_STARTED) {
		hip5_napi_complete(sdev, napi, hip);
		return 0;
	}

#if defined(CONFIG_SCSC_PCIE_CHIP)
	if (!hip_priv->mx_pci_claim_data) {
		if (scsc_mx_service_claim_deferred(sdev->service, hip5_napi_mx_claim_complete_cb, (void *)hip_priv, WLAN_RX_DATA)) {
			hip5_napi_complete(sdev, napi, hip);
			return 0;
		}
		hip_priv->mx_pci_claim_data = true;
	}
#endif

	spin_lock_bh(&hip_priv->rx_lock);
	SCSC_HIP4_SAMPLER_INT_BH(hip->hip_priv->minor, 0);
	if (ktime_compare(bh_init_data, bh_end_data) <= 0)
		bh_init_data = ktime_get();

	idx_r = hip5_read_index(hip, HIP5_MIF_Q_TH_DAT0, ridx);
	idx_w = hip5_read_index(hip, HIP5_MIF_Q_TH_DAT0, widx);

	service = sdev->service;

	if (idx_r == idx_w) {
		SLSI_DBG3(sdev, SLSI_RX, "nothing to do, NAPI Complete\n");
		hip5_napi_complete(sdev, napi, hip);
		goto end;
	}

	if ((idx_r >= MAX_NUM) || (idx_w >= MAX_NUM)) {
		SLSI_WARN(sdev, "invalid scoreboard index (r:%d, w:%d)\n", idx_r, idx_w);
		hip5_napi_complete(sdev, napi, hip);
		goto end;
	}

	SCSC_HIP4_SAMPLER_Q(hip_priv->minor, HIP5_MIF_Q_TH_DAT0, ridx, idx_r, 1);
	SCSC_HIP4_SAMPLER_Q(hip_priv->minor, HIP5_MIF_Q_TH_DAT0, widx, idx_w, 1);

	todo = ((idx_w - idx_r) & (MAX_NUM - 1));
	SLSI_DBG2(sdev, SLSI_RX, "r:%d w:%d todo:%d\n", idx_r, idx_w, todo);

	/* The write index written by firmware may arrive out of order
	 * at Host. This can cause the Host to read from wrong indexes.
	 *
	 * To mitigate this, apply a half buffer check to decide whether
	 * the write index is a valid one or not. More than half buffer
	 * size indicates that write index just arrived read out of order.
	 *
	 * As of now the firmware can send half buffer maximum (as we have
	 * 512 mbulk slots and 1024 entries). If the number of slots are changed
	 * the firmware must ensure that in one update it does not write more
	 * than half of the size of circular buffer.
	 */
	if (todo > (MAX_NUM >> 1)) {
		hip_priv->th_w_idx_err_count++;
		SLSI_WARN(sdev, "Out of order write index (r:%d, w:%d, retry: %d/%d)\n",
			idx_r,
			idx_w,
			hip_priv->th_w_idx_err_count,
			HIP5_TH_IDX_ERR_NUM_RETRY);

		if (hip_priv->th_w_idx_err_count >= HIP5_TH_IDX_ERR_NUM_RETRY) {
			SLSI_ERR_NODEV("T-H data stall due to invalid Write index; trigger Panic\n");
			queue_work(sdev->device_wq, &sdev->trigger_wlan_fail_work);
			hip5_napi_complete(sdev, napi, hip);
			goto end;
		}
		/* set work_done equal to budget to keep polling */
		work_done = budget;
		goto end;
	}
	hip_priv->th_w_idx_err_count = 0;

	while (idx_r != idx_w) {
		struct sk_buff *skb;
		/* TODO: currently the max number to be freed is 2. In future
		 * implementations (i.e. AMPDU) this number may be bigger
		 */
		/* list of mbulks to be freed */
		scsc_mifram_ref to_free[HIP5_OPT_NUM_BD_PER_SIGNAL_MAX + 1] = { 0 };
		u8              i = 0;

		/* Catch-up with idx_w */
		if (hip_priv->version == 5) {
			struct sk_buff_head skb_list;
			int ret;

			__skb_queue_head_init(&skb_list);
			ret = hip5_opt_hip_signal_to_skb(sdev, service, hip_priv, ctrl, HIP5_MIF_Q_TH_DAT0, to_free, &idx_r, &idx_w, &skb_list);
			if (ret) {
				if (ret == -EFAULT) {
					/* set work_done < budget to stop polling */
					work_done = 1;

					/* read index is not updated and No free buffer signal */
					break;
				}
				SLSI_ERR_NODEV("Dat: Error parsing or allocating skb\n");
				hip5_dump_dbg(hip, NULL, NULL, service);
				goto consume_dat_mbulk;
			}

			while (!skb_queue_empty(&skb_list)) {
				struct sk_buff *rx_skb;

				rx_skb = __skb_dequeue(&skb_list);
				if (slsi_hip_rx(sdev, rx_skb) < 0) {
					SLSI_ERR_NODEV("Dat: Error detected slsi_hip_rx\n");
					hip5_dump_dbg(hip, NULL, rx_skb, service);
					kfree_skb(rx_skb);
				}
			}
		} else {
			hip_entry = (struct hip5_hip_entry *)(&ctrl->q_tlv[2].array[idx_r]);
			skb = hip5_bd_to_skb(sdev, service, hip_priv, hip_entry, to_free, &idx_r, &idx_w);

			if (!skb) {
				u16 idx_r_temp = 0;

				/* is the failure due to a partial chain */
				idx_r_temp =  idx_r + 1;
				idx_r_temp &= (MAX_NUM - 1);
				if (idx_r_temp == idx_w) {
					/* set work_done < budget to stop polling */
					work_done = 1;

					/* read index is not updated and No free buffer signal */
					break;
				}

				SLSI_ERR_NODEV("Dat: Error parsing or allocating skb\n");
				hip5_dump_dbg(hip, NULL, skb, service);
				goto consume_dat_mbulk;
			}

			if (slsi_hip_rx(sdev, skb) < 0) {
				SLSI_ERR_NODEV("Dat: Error detected slsi_hip_rx\n");
				hip5_dump_dbg(hip, NULL, skb, service);
				kfree_skb(skb);
			}
		}
consume_dat_mbulk:
		/* Increase index */
		idx_r++;
		idx_r &= (MAX_NUM - 1);

		while ((i <= HIP5_OPT_NUM_BD_PER_SIGNAL_MAX) && (ref = to_free[i++])) {
			/* Set the number of retries */
			retry = FB_NO_SPC_NUM_RET;
			while (hip5_q_add_signal(hip, HIP5_MIF_Q_TH_RFBD0, ref, service) && (!atomic_read(&hip->hip_priv->closing)) && (retry > 0)) {
				SLSI_WARN_NODEV("Data: Not enough space in FB, retry of %d/%d\n",
						((FB_NO_SPC_NUM_RET + 1) - retry), FB_NO_SPC_NUM_RET);
				udelay(FB_NO_SPC_DELAY_US);
				retry--;

				if (retry == 0)
					SLSI_ERR_NODEV("Dat: FB has not been freed for %d us\n", FB_NO_SPC_NUM_RET * FB_NO_SPC_DELAY_US);
#ifdef CONFIG_SCSC_WLAN_HIP5_PROFILING
				SCSC_HIP4_SAMPLER_QFULL(hip_priv->minor, HIP5_MIF_Q_TH_RFBD1);
#endif
			}
			work_done++;
		}

		if (work_done >= budget) {
			/* We have consumed all the bugdet */
			work_done = budget;
			break;
		}
	}

	hip5_update_index(hip, HIP5_MIF_Q_TH_DAT0, ridx, idx_r);

	if (work_done < budget) {
		SLSI_DBG3(sdev, SLSI_HIP, "NAPI complete (work_done:%d)\n", work_done);
		hip5_napi_complete(sdev, napi, hip);
	}
end:
	SLSI_DBG3(sdev, SLSI_RX, "work done:%d\n", work_done);
	SCSC_HIP4_SAMPLER_INT_OUT_BH(hip->hip_priv->minor, 0);
	spin_unlock_bh(&hip_priv->rx_lock);
	return work_done;
}

static void hip5_irq_handler_dat(int irq, void *data)
{
	struct slsi_hip    *hip = (struct slsi_hip *)data;
	struct slsi_dev    *sdev = container_of(hip, struct slsi_dev, hip);
	unsigned long      flags;

	if (!hip || !sdev || !sdev->service || !hip->hip_priv) {
		SLSI_ERR_NODEV("NULL HIP instance\n");
		return;
	}

	if (atomic_read(&hip->hip_priv->closing)) {
		SLSI_ERR_NODEV("interrupt received while HIP is closed: %d\n", irq);
		scsc_service_mifintrbit_bit_mask(sdev->service, irq);
		scsc_service_mifintrbit_bit_clear(sdev->service, irq);
		return;
	}

	SCSC_HIP4_SAMPLER_INT(hip->hip_priv->minor, 0);
	intr_received_data = ktime_get();

	if (!slsi_wake_lock_active(&hip->hip_priv->wake_lock_data)) {
		slsi_wake_lock_timeout(&hip->hip_priv->wake_lock_data, msecs_to_jiffies(SLSI_HIP_WAKELOCK_TIME_OUT_IN_MS));
	}

	/* Mask interrupt to avoid interrupt storm and let BH run */
	scsc_service_mifintrbit_bit_mask(sdev->service, hip->hip_priv->intr_to_host_data1);

	spin_lock_irqsave(&hip->hip_priv->napi_cpu_lock, flags);
	if (test_bit(SLSI_HIP_NAPI_STATE_ENABLED, &hip->hip_priv->bh_dat->bh_priv.napi.napi_state)) {
		slsi_lbm_run_bh(hip->hip_priv->bh_dat);
		scsc_service_mifintrbit_bit_clear(sdev->service, hip->hip_priv->intr_to_host_data1);
	}
	spin_unlock_irqrestore(&hip->hip_priv->napi_cpu_lock, flags);
	SCSC_HIP4_SAMPLER_INT_OUT(hip->hip_priv->minor, 0);
}

#ifdef CONFIG_SCSC_QOS
static void hip5_pm_qos_work(struct work_struct *data)
{
	struct hip_priv        *hip_priv = container_of(data, struct hip_priv, pm_qos_work);
	struct slsi_hip        *hip = hip_priv->hip;
	struct slsi_dev         *sdev = container_of(hip, struct slsi_dev, hip);
	u8 state;

	if (!sdev || !sdev->service) {
		WLBT_WARN_ON(1);
		return;
	}

	SLSI_DBG1(sdev, SLSI_HIP, "update to state %d\n", hip_priv->pm_qos_state);
	spin_lock_bh(&hip_priv->pm_qos_lock);
	state = hip_priv->pm_qos_state;
	spin_unlock_bh(&hip_priv->pm_qos_lock);
	scsc_service_pm_qos_update_request(sdev->service, state);
}

static void hip5_traffic_monitor_cb(void *client_ctx, u32 state, u32 tput_tx, u32 tput_rx)
{
	struct slsi_hip *hip = (struct slsi_hip *)client_ctx;
	struct slsi_dev *sdev = container_of(hip, struct slsi_dev, hip);
	u8 state_before;

	if (!sdev)
		return;

	spin_lock_bh(&hip->hip_priv->pm_qos_lock);
	state_before = hip->hip_priv->pm_qos_state;

	SLSI_DBG1(sdev, SLSI_HIP, "state:%u --> event (state:%u, tput_tx:%u bps, tput_rx:%u bps)\n", state_before, state, tput_tx, tput_rx);

	if (state == TRAFFIC_MON_CLIENT_STATE_HIGH || state == TRAFFIC_MON_CLIENT_STATE_OVERRIDE)
		hip->hip_priv->pm_qos_state = SCSC_QOS_MAX;
	else if (state == TRAFFIC_MON_CLIENT_STATE_MID)
		hip->hip_priv->pm_qos_state = SCSC_QOS_MED;
	else
		hip->hip_priv->pm_qos_state = SCSC_QOS_DISABLED;

	spin_unlock_bh(&hip->hip_priv->pm_qos_lock);

	if (state_before != hip->hip_priv->pm_qos_state)
		schedule_work(&hip->hip_priv->pm_qos_work);
}
#endif

#if IS_ENABLED(CONFIG_SCSC_LOGRING)
static void hip5_traffic_monitor_logring_cb(void *client_ctx, u32 state, u32 tput_tx, u32 tput_rx)
{
	struct hip_priv *hip_priv = (struct hip_priv *)client_ctx;
	struct slsi_hip *hip = hip_priv->hip;
	struct slsi_dev *sdev = container_of(hip, struct slsi_dev, hip);

	if (!sdev)
		return;

	SLSI_DBG1(sdev, SLSI_HIP, "event (state:%u, tput_tx:%u bps, tput_rx:%u bps)\n", state, tput_tx, tput_rx);
	if (state == TRAFFIC_MON_CLIENT_STATE_HIGH || state == TRAFFIC_MON_CLIENT_STATE_MID || state == TRAFFIC_MON_CLIENT_STATE_OVERRIDE) {
		if (hip4_dynamic_logging)
			scsc_logring_enable(false);
	} else {
		scsc_logring_enable(true);
	}
}
#endif

static int slsi_hip_init_control_table(struct slsi_dev *sdev, struct slsi_hip *hip, void *hip_ptr,
				       struct hip5_hip_control *hip_control, struct scsc_service *service)
{
	scsc_mifram_ref ref, ref_scoreboard;
	u32             total_mib_len;
	u32             mib_file_offset;
	int             i = 0;
	uintptr_t       scrb_ref = 0;

#if defined(CONFIG_SCSC_PCIE_PAEAN_X86) || defined(CONFIG_SOC_S5E9925)
	/* Initialize scoreboard */
	if (hip5_scoreboard_in_ramrp) {
		hip->hip_priv->scbrd_ramrp_base = scsc_mx_service_get_ramrp_ptr(sdev->service);

		/* let firmware know scoreboard is in RAMRP, by setting the LSB of scoreboard location field */
		scrb_ref = scrb_ref | 0x1;

	} else {
		if (scsc_mx_service_mif_ptr_to_addr(service, &hip_control->scoreboard, &ref_scoreboard))
			return -EFAULT;
	}
#else
	if (scsc_mx_service_mif_ptr_to_addr(service, &hip_control->scoreboard, &ref_scoreboard))
		return -EFAULT;
#endif

	/* Calculate total space used by wlan*.hcf files */
	for (i = 0, total_mib_len = 0; i < SLSI_WLAN_MAX_MIB_FILE; i++)
		total_mib_len += sdev->mib[i].mib_len;

	/* Copy MIB content in shared memory if any */
	/* Clear the area to avoid picking up old values */
	memset(hip_ptr + HIP5_WLAN_MIB_OFFSET, 0, HIP5_WLAN_MIB_SIZE);

	if (total_mib_len && total_mib_len < HIP5_WLAN_MIB_SIZE) {
		SLSI_INFO_NODEV("Loading MIB into shared memory, size (%d)\n", total_mib_len);
		/* Load each MIB file into shared DRAM region */
		for (i = 0, mib_file_offset = 0;
		     i < SLSI_WLAN_MAX_MIB_FILE && sdev->mib[i].mib_file_name;
		     i++) {
			SLSI_INFO_NODEV("Loading MIB %d into shared memory, offset (%d), size (%d), total (%d)\n",
					i, mib_file_offset, sdev->mib[i].mib_len, total_mib_len);
			if (sdev->mib[i].mib_len) {
				memcpy((u8 *)hip_ptr + HIP5_WLAN_MIB_OFFSET + mib_file_offset, sdev->mib[i].mib_data,
				       sdev->mib[i].mib_len);
				mib_file_offset += sdev->mib[i].mib_len;
			}
		}
		hip_control->config_v4.mib_loc      = hip->hip_ref + HIP5_WLAN_MIB_OFFSET;
		hip_control->config_v4.mib_sz       = total_mib_len;
		hip_control->config_v5.mib_loc      = hip->hip_ref + HIP5_WLAN_MIB_OFFSET;
		hip_control->config_v5.mib_sz       = total_mib_len;
	} else {
		SLSI_ERR_NODEV("MIB size (%d), is bigger than the MIB AREA (%d) or 0. Aborting memcpy\n",
			       total_mib_len, HIP5_WLAN_MIB_SIZE);
		hip_control->config_v4.mib_loc      = 0;
		hip_control->config_v4.mib_sz       = 0;
		hip_control->config_v5.mib_loc      = 0;
		hip_control->config_v5.mib_sz       = 0;
		total_mib_len = 0;
	}

	/* Initialize hip_control table for version 4 */
	/***** VERSION 4 *******/
	hip_control->config_v4.magic_number    = 0xcaba0401;
	hip_control->config_v4.hip_config_ver  = 4;
	hip_control->config_v4.config_len      = sizeof(struct hip5_hip_config_version_1);
	hip_control->config_v4.host_cache_line = 64;
	hip_control->config_v4.host_buf_loc    = hip->hip_ref + HIP5_WLAN_TX_OFFSET;
	hip_control->config_v4.host_buf_sz     = HIP5_WLAN_TX_SIZE;
	hip_control->config_v4.fw_buf_loc      = hip->hip_ref + HIP5_WLAN_RX_OFFSET;
	hip_control->config_v4.fw_buf_sz       = HIP5_WLAN_RX_SIZE;
	hip_control->config_v4.log_config_loc  = 0;

	if (hip5_scoreboard_in_ramrp)
		hip_control->config_v4.scbrd_loc = (u32)scrb_ref;
	else
		hip_control->config_v4.scbrd_loc = (u32)ref_scoreboard;

	hip_control->config_v4.q_num = 24;

	/* Initialize q relative positions */
	for (i = 0; i < MIF_HIP_CFG_Q_NUM; i++) {
		hip_control->config_v4.q_cfg[i].q_len = MAX_NUM;
		hip_control->config_v4.q_cfg[i].q_idx_sz = 2;
		if (i > 13) {
			hip_control->config_v4.q_cfg[i].q_entry_sz = 64;
		} else {
			hip_control->config_v4.q_cfg[i].q_entry_sz = 4;
			if (scsc_mx_service_mif_ptr_to_addr(service, &hip_control->q[i].array, &ref))
				return -EFAULT;
		}
		hip_control->config_v4.q_cfg[i].q_loc = (u32)ref;
	}

	if (scsc_mx_service_mif_ptr_to_addr(service, &hip_control->q_tlv[0].array, &ref))
		return -EFAULT;
	hip_control->config_v4.q_cfg[HIP5_MIF_Q_FH_DAT0].q_loc = (u32)ref;

	if (scsc_mx_service_mif_ptr_to_addr(service, &hip_control->q_tlv[1].array, &ref))
		return -EFAULT;
	hip_control->config_v4.q_cfg[HIP5_MIF_Q_FH_DAT1].q_loc = (u32)ref;

	if (scsc_mx_service_mif_ptr_to_addr(service, &hip_control->q_tlv[2].array, &ref))
		return -EFAULT;

	hip_control->config_v4.q_cfg[HIP5_MIF_Q_TH_DAT0].q_loc = (u32)ref;
	hip_control->config_v4.q_cfg[HIP5_MIF_Q_TH_DAT0].q_entry_sz = 64;

	/* initialize interrupts */
	hip_control->config_v4.q_cfg[HIP5_MIF_Q_FH_CTRL].int_n = hip->hip_priv->intr_from_host_ctrl;
	hip_control->config_v4.q_cfg[HIP5_MIF_Q_TH_RFBC].int_n = hip->hip_priv->intr_from_host_ctrl;
	hip_control->config_v4.q_cfg[HIP5_MIF_Q_TH_RFBD0].int_n = hip->hip_priv->intr_from_host_ctrl;
	hip_control->config_v4.q_cfg[HIP5_MIF_Q_TH_CTRL].int_n = hip->hip_priv->intr_to_host_ctrl;
	hip_control->config_v4.q_cfg[HIP5_MIF_Q_FH_RFBC].int_n = hip->hip_priv->intr_to_host_ctrl_fb;

	for (i = HIP5_MIF_Q_FH_DAT0; i < MIF_HIP_CFG_Q_NUM; i++)
		hip_control->config_v4.q_cfg[i].int_n = hip->hip_priv->intr_from_host_data;

	hip_control->config_v4.q_cfg[HIP5_MIF_Q_TH_DAT0].int_n = hip->hip_priv->intr_to_host_data1;
	hip_control->config_v4.q_cfg[HIP5_MIF_Q_TH_DAT1].int_n = hip->hip_priv->intr_to_host_data2;

	/***** END VERSION 4 *******/

	/* Initialize hip_control table for version 5 */

	/***** VERSION 5 *******/
	hip_control->config_v5.magic_number = 0xcaba0401;
	hip_control->config_v5.hip_config_ver = 5;
	hip_control->config_v5.config_len = sizeof(struct hip5_hip_config_version_2);
	hip_control->config_v5.host_cache_line = 64;
	hip_control->config_v5.host_buf_loc = hip->hip_ref + HIP5_WLAN_TX_OFFSET;
	if (hip5_tx_zero_copy) {
		hip_control->config_v5.host_buf_sz	= HIP5_WLAN_ZERO_COPY_TX_SIZE;
		hip_control->config_v5.fw_buf_loc	= hip->hip_ref + HIP5_WLAN_ZERO_COPY_RX_OFFSET;
		hip_control->config_v5.fw_buf_sz	= HIP5_WLAN_ZERO_COPY_RX_SIZE;
	} else {
		hip_control->config_v5.host_buf_sz	= HIP5_WLAN_TX_SIZE;
		hip_control->config_v5.fw_buf_loc	= hip->hip_ref + HIP5_WLAN_RX_OFFSET;
		hip_control->config_v5.fw_buf_sz	= HIP5_WLAN_RX_SIZE;
	}
	hip_control->config_v5.log_config_loc = 0;

	if (hip5_scoreboard_in_ramrp) {
		hip_control->config_v5.scbrd_loc = (u32)scrb_ref;
	} else {
		hip_control->config_v5.scbrd_loc = (u32)ref_scoreboard;
	}

	hip_control->config_v5.q_num = 24;

	/* Initialize q relative positions */
	for (i = 0; i < MIF_HIP_CFG_Q_NUM; i++) {
		hip_control->config_v5.q_cfg[i].q_len = MAX_NUM;
		hip_control->config_v5.q_cfg[i].q_idx_sz = 2;
		if (i > HIP5_MIF_Q_TH_RFBD1) {
			hip_control->config_v5.q_cfg[i].q_entry_sz = 64;
		} else {
			hip_control->config_v5.q_cfg[i].q_entry_sz = 4;
		}
		if (i < HIP5_MIF_Q_FH_DAT0) {
			if (scsc_mx_service_mif_ptr_to_addr(service, &hip_control->q[i].array, &ref)) {
				return -EFAULT;
			}
		}
		hip_control->config_v5.q_cfg[i].q_loc = (u32)ref;
	}

	if (scsc_mx_service_mif_ptr_to_addr(service, &hip_control->q_tlv[0].array, &ref)) {
		return -EFAULT;
	}
	hip_control->config_v5.q_cfg[HIP5_MIF_Q_FH_DAT0].q_loc = (u32)ref;

	if (scsc_mx_service_mif_ptr_to_addr(service, &hip_control->q_tlv[1].array, &ref)) {
		return -EFAULT;
	}
	hip_control->config_v5.q_cfg[HIP5_MIF_Q_FH_DAT1].q_loc = (u32)ref;

	if (scsc_mx_service_mif_ptr_to_addr(service, &hip_control->q_tlv[2].array, &ref)) {
			return -EFAULT;
	}
	hip_control->config_v5.q_cfg[HIP5_MIF_Q_TH_DAT0].q_loc = (u32)ref;
	hip_control->config_v5.q_cfg[HIP5_MIF_Q_TH_DAT0].q_entry_sz = 64;

	if (scsc_mx_service_mif_ptr_to_addr(service, &hip_control->q_tlv[3].array, &ref)) {
			return -EFAULT;
	}
	hip_control->config_v5.q_cfg[HIP5_MIF_Q_FH_CTRL].q_loc = (u32)ref;
	hip_control->config_v5.q_cfg[HIP5_MIF_Q_FH_CTRL].q_entry_sz = 64;

	if (scsc_mx_service_mif_ptr_to_addr(service, &hip_control->q_tlv[4].array, &ref)) {
			return -EFAULT;
	}
	hip_control->config_v5.q_cfg[HIP5_MIF_Q_TH_CTRL].q_loc = (u32)ref;
	hip_control->config_v5.q_cfg[HIP5_MIF_Q_TH_CTRL].q_entry_sz = 64;

	/* initialize interrupts */
	hip_control->config_v5.q_cfg[HIP5_MIF_Q_FH_CTRL].int_n = hip->hip_priv->intr_from_host_ctrl;
	hip_control->config_v5.q_cfg[HIP5_MIF_Q_TH_RFBC].int_n = hip->hip_priv->intr_from_host_ctrl;
	hip_control->config_v5.q_cfg[HIP5_MIF_Q_TH_RFBD0].int_n = hip->hip_priv->intr_from_host_ctrl;
	hip_control->config_v5.q_cfg[HIP5_MIF_Q_TH_CTRL].int_n = hip->hip_priv->intr_to_host_ctrl;
	hip_control->config_v5.q_cfg[HIP5_MIF_Q_FH_RFBC].int_n = hip->hip_priv->intr_to_host_ctrl_fb;

	for (i = HIP5_MIF_Q_FH_DAT0; i < MIF_HIP_CFG_Q_NUM; i++) {
		hip_control->config_v5.q_cfg[i].int_n = hip->hip_priv->intr_from_host_data;
	}

	hip_control->config_v5.q_cfg[HIP5_MIF_Q_TH_DAT0].int_n = hip->hip_priv->intr_to_host_data1;
	hip_control->config_v5.q_cfg[HIP5_MIF_Q_TH_DAT1].int_n = hip->hip_priv->intr_to_host_data2;

#ifdef CONFIG_SCSC_WLAN_HOST_DPD
	hip_control->config_v5.dpd_buf_loc = hip->hip_ref + HIP5_WLAN_DPD_BUF_OFFSET;
	hip_control->config_v5.dpd_buf_sz = HIP5_WLAN_DPD_BUF_SIZE;
	hip_control->config_v5.intr_from_host_dpd = hip->hip_priv->intr_from_host_dpd;
	hip_control->config_v5.intr_to_host_dpd = hip->hip_priv->intr_to_host_dpd;
	slsi_wlan_dpd_mmap_set_buffer(sdev, (hip_ptr + HIP5_WLAN_DPD_BUF_OFFSET), HIP5_WLAN_DPD_BUF_SIZE);
#endif
	for (i = 0; i < HIP5_CONFIG_RESERVED_BYTES_NUM; i++)
		hip_control->config_v5.reserved[i] = 0;
	/***** END VERSION 5 *******/

	/* Initialzie hip_init configuration */
	hip_control->init.magic_number = HIP5_CONFIG_INIT_MAGIC_NUM;
	if (scsc_mx_service_mif_ptr_to_addr(service, &hip_control->config_v4, &ref))
		return -EFAULT;
	hip_control->init.version_a_ref = ref;

	if (scsc_mx_service_mif_ptr_to_addr(service, &hip_control->config_v5, &ref))
		return -EFAULT;
	hip_control->init.version_b_ref = ref;
	/* End hip_init configuration */
	return 0;
}

#ifdef CONFIG_SCSC_WLAN_HOST_DPD
int slsi_hip_from_host_dpd_intr_set(struct scsc_service *service, struct slsi_hip *hip)
{
	if (!hip || !service || !hip->hip_priv) {
		SLSI_WARN_NODEV("invalid HIP instance\n");
		return -ENODEV;
	}
	scsc_service_mifintrbit_bit_set(service, hip->hip_priv->intr_from_host_dpd, SCSC_MIFINTR_TARGET_WLAN);
	return 0;
}

static void hip5_irq_handler_dpd(int irq, void *data) {
	struct slsi_hip    *hip = (struct slsi_hip *)data;
	struct slsi_dev    *sdev = container_of(hip, struct slsi_dev, hip);

	if (!hip || !sdev || !sdev->service || !hip->hip_priv) {
		SLSI_WARN_NODEV("invalid HIP instance\n");
		return;
	}
	/* Clear interrupt */
	scsc_service_mifintrbit_bit_mask(sdev->service, irq);
	scsc_service_mifintrbit_bit_clear(sdev->service, irq);

	slsi_wlan_dpd_mmap_user_space_event(SLSI_WLAN_DPD_DRV_MSG_ID_WLAN_INTR);
	scsc_service_mifintrbit_bit_unmask(sdev->service, irq);
}
#endif

int slsi_hip_init(struct slsi_hip *hip)
{
	void                    *hip_ptr;
	struct hip5_hip_control *hip_control;
	struct scsc_service     *service;
	struct slsi_dev         *sdev = container_of(hip, struct slsi_dev, hip);
	int                     ret;
	u8                      i = 0;

	if (!sdev || !sdev->service)
		return -EINVAL;

	hip->hip_priv = kzalloc(sizeof(*hip->hip_priv), GFP_ATOMIC);
	if (!hip->hip_priv)
		return -ENOMEM;

	/* Initially set hip as closed status.
	 * FW can raise IRQ between scsc_service_mifintrbit_register_tohost and
	 * scsc_service_mifintrbit_bit_mask.
	 * irq handler returns without doing anything until closing is cleared by hip setup.
	 */
	atomic_set(&hip->hip_priv->closing, 1);

	/* check if TX zero copy can be supported? */
	if (hip5_tx_zero_copy) {
		hip->hip_priv->tx_skb_free_cnt = hip5_tx_zero_copy_tx_slots;
		if (dma_set_mask_and_coherent(sdev->dev, DMA_BIT_MASK(64)) != 0) {
			SLSI_ERR(sdev, "failed to set DMA for dev; default to non-zero copy\n");
			hip5_tx_zero_copy = false;
		} else {
			/* Setting of device for DMA is successful. Now check if the Firmware
			 * variant supports TX zero copy?
			 * For now only A55 variant of firmware supports zero copy.
			 * Enable only if A55 is found in build string of Firmware
			 */
			if (!is_fw_config_a55())
				hip5_tx_zero_copy = false;
		}
	}

	SLSI_INFO_NODEV("HIP5_WLAN_CONFIG_SIZE (%d/%d)\n", sizeof(struct hip5_hip_control), HIP5_WLAN_CONFIG_SIZE);
	SLSI_INFO_NODEV("HIP5_WLAN_MIB_SIZE (%d)\n", HIP5_WLAN_MIB_SIZE);
#ifdef CONFIG_SCSC_WLAN_HOST_DPD
	SLSI_INFO_NODEV("HIP5_WLAN_DPD_BUF_SIZE (%d)\n", HIP5_WLAN_DPD_BUF_SIZE);
#endif
	SLSI_INFO_NODEV("HIP5_WLAN_TX_CTL_SIZE (%d)\n", HIP5_WLAN_TX_CTRL_SIZE);
	SLSI_INFO_NODEV("HIP5_WLAN_TX_CTL_SLOTS (%d)\n", HIP5_CTL_SLOTS);
	if (hip5_tx_zero_copy) {
		SLSI_INFO_NODEV("HIP5_WLAN_ZERO_COPY_TX_DAT_SIZE (%d)\n", HIP5_WLAN_ZERO_COPY_TX_DATA_SIZE);
		SLSI_INFO_NODEV("HIP5_WLAN_ZERO_COPY_TX_DAT_SLOTS (%d)\n", hip5_tx_zero_copy_tx_slots);
		SLSI_INFO_NODEV("HIP5_WLAN_ZERO_COPY_RX_SIZE (%d)\n", HIP5_WLAN_ZERO_COPY_RX_SIZE);
		SLSI_INFO_NODEV("HIP5_WLAN_ZERO_COPY_TOTAL_MEM (%d)\n", HIP5_WLAN_ZERO_COPY_TOTAL_MEM);
	} else {
		SLSI_INFO_NODEV("HIP5_WLAN_TX_DAT_SIZE (%d)\n", HIP5_WLAN_TX_DATA_SIZE);
		SLSI_INFO_NODEV("HIP5_WLAN_RX_SIZE (%d)\n", HIP5_WLAN_RX_SIZE);
		SLSI_INFO_NODEV("HIP5_WLAN_TOTAL_MEM (%d)\n", HIP5_WLAN_TOTAL_MEM);
		SLSI_INFO_NODEV("HIP5_TX_DAT_SLOTS (%d)\n", HIP5_DAT_SLOTS);
	}

	hip->hip_priv->minor = hip4_sampler_register_hip(sdev->maxwell_core);
	if (hip->hip_priv->minor < SCSC_HIP4_INTERFACES) {
		SLSI_DBG1_NODEV(SLSI_HIP, "registered with minor %d\n", hip->hip_priv->minor);
		sdev->minor_prof = hip->hip_priv->minor;
	} else {
		SLSI_DBG1_NODEV(SLSI_HIP, "hip_sampler is not enabled\n");
	}

	/* Used in the workqueue */
	hip->hip_priv->hip = hip;

	service = sdev->service;

	hip->hip_priv->host_pool_id_dat = MBULK_POOL_ID_DATA;
	hip->hip_priv->host_pool_id_ctl = MBULK_POOL_ID_CTRL;

	/* hip_ref contains the reference of the start of shared memory allocated for WLAN */
	/* hip_ptr is the kernel address of hip_ref*/
	hip_ptr = scsc_mx_service_mif_addr_to_ptr(service, hip->hip_ref);

#ifdef CONFIG_SCSC_WLAN_DEBUG
	if (hip5_tx_zero_copy) {
		/* Configure mbulk allocator - Data QUEUES */
		ret = mbulk_pool_add(MBULK_POOL_ID_DATA, hip_ptr + HIP5_WLAN_TX_DATA_OFFSET,
				     hip_ptr + HIP5_WLAN_TX_DATA_OFFSET + HIP5_WLAN_ZERO_COPY_TX_DATA_SIZE,
				     (HIP5_DAT_MBULK_SIZE - sizeof(struct mbulk), 5,
				     hip->hip_priv->minor);
	} else {
		/* Configure mbulk allocator - Data QUEUES */
		ret = mbulk_pool_add(MBULK_POOL_ID_DATA, hip_ptr + HIP5_WLAN_TX_DATA_OFFSET,
				     hip_ptr + HIP5_WLAN_TX_DATA_OFFSET + HIP5_WLAN_TX_DATA_SIZE,
				     (HIP5_WLAN_TX_DATA_SIZE / HIP5_DAT_SLOTS) - sizeof(struct mbulk), 5,
				     hip->hip_priv->minor);
	}

	if (ret)
		return ret;

	/* Configure mbulk allocator - Control QUEUES */
	ret = mbulk_pool_add(MBULK_POOL_ID_CTRL, hip_ptr + HIP5_WLAN_TX_CTRL_OFFSET,
			     hip_ptr + HIP5_WLAN_TX_CTRL_OFFSET + HIP5_WLAN_TX_CTRL_SIZE,
			     (HIP5_WLAN_TX_CTRL_SIZE / HIP5_CTL_SLOTS) - sizeof(struct mbulk), 0,
			     hip->hip_priv->minor);
	if (ret)
		return ret;
#else
	if (hip5_tx_zero_copy) {
		/* Configure mbulk allocator - Data QUEUES */
		ret = mbulk_pool_add(MBULK_POOL_ID_DATA, hip_ptr + HIP5_WLAN_TX_DATA_OFFSET,
				     hip_ptr + HIP5_WLAN_TX_DATA_OFFSET + HIP5_WLAN_ZERO_COPY_TX_DATA_SIZE,
				     HIP5_DAT_MBULK_SIZE - sizeof(struct mbulk), 5);
	} else {
		/* Configure mbulk allocator - Data QUEUES */
		ret = mbulk_pool_add(MBULK_POOL_ID_DATA, hip_ptr + HIP5_WLAN_TX_DATA_OFFSET,
				     hip_ptr + HIP5_WLAN_TX_DATA_OFFSET + HIP5_WLAN_TX_DATA_SIZE,
				     (HIP5_WLAN_TX_DATA_SIZE / HIP5_DAT_SLOTS) - sizeof(struct mbulk), 5);
	}
	if (ret)
		return ret;

	/* Configure mbulk allocator - Control QUEUES */
	ret = mbulk_pool_add(MBULK_POOL_ID_CTRL, hip_ptr + HIP5_WLAN_TX_CTRL_OFFSET,
			     hip_ptr + HIP5_WLAN_TX_CTRL_OFFSET + HIP5_WLAN_TX_CTRL_SIZE,
			    (HIP5_WLAN_TX_CTRL_SIZE / HIP5_CTL_SLOTS) - sizeof(struct mbulk), 0);
	if (ret)
		return ret;
#endif

	/* Reset hip_control table */
	memset(hip_ptr, 0, sizeof(struct hip5_hip_control));

	/* Reset Sample q values sending 0xff */
	SCSC_HIP4_SAMPLER_RESET(hip->hip_priv->minor);

	/***** VERSION 4 *******/
	/* TOHOST Handler allocator */
	rcu_read_lock();
	hip->hip_priv->bh_dat = slsi_lbm_register_napi(sdev,
						       hip5_napi_poll,
						       hip->hip_priv->intr_to_host_data1,
						       NP_RX_0);
	if (hip->hip_priv->bh_dat) {
		slsi_lbm_register_cpu_affinity_control(hip->hip_priv->bh_dat, NP_RX_0);
		slsi_lbm_register_io_saturation_control(hip->hip_priv->bh_dat);
	} else {
		SLSI_ERR_NODEV("Error allocating bh\n");
		rcu_read_unlock();
		return -ENOMEM;
	}

	rcu_read_unlock();

	/* TOHOST Handler allocator */
	ret = scsc_service_mifintrbit_register_tohost(service, hip5_irq_handler_ctrl, hip, SCSC_MIFINTR_TARGET_WLAN, HIP5_IRQ_HANDLER_CTRL_TYPE);
	if (ret < 0) {
		SLSI_ERR_NODEV("Error allocating intr_to_host_ctrl\n");
		return ret;
	}

	hip->hip_priv->intr_to_host_ctrl = ret;
	SLSI_INFO_NODEV("HIP5: intr_to_host_ctrl %d\n", hip->hip_priv->intr_to_host_ctrl);
	/* Mask the interrupt to prevent intr been kicked during start */
	scsc_service_mifintrbit_bit_mask(service, hip->hip_priv->intr_to_host_ctrl);

	ret = scsc_service_mifintrbit_register_tohost(service, hip5_irq_handler_fb, hip, SCSC_MIFINTR_TARGET_WLAN, HIP5_IRQ_HANDLER_FB_TYPE);
	if (ret < 0) {
		SLSI_ERR_NODEV("Error allocating intr_to_host_ctrl_fb\n");
		scsc_service_mifintrbit_unregister_tohost(service, hip->hip_priv->intr_to_host_ctrl, SCSC_MIFINTR_TARGET_WLAN);
		return ret;
	}

	hip->hip_priv->intr_to_host_ctrl_fb = ret;
	SLSI_INFO_NODEV("HIP5: intr_to_host_ctrl_fb %d\n", hip->hip_priv->intr_to_host_ctrl_fb);
	/* Mask the interrupt to prevent intr been kicked during start */
	scsc_service_mifintrbit_bit_mask(service, hip->hip_priv->intr_to_host_ctrl_fb);

	ret = scsc_service_mifintrbit_register_tohost(service, hip5_irq_handler_dat, hip, SCSC_MIFINTR_TARGET_WLAN, HIP5_IRQ_HANDLER_DAT_TYPE);
	if (ret < 0) {
		SLSI_ERR_NODEV("Error allocating intr_to_host_data1\n");
		scsc_service_mifintrbit_unregister_tohost(service, hip->hip_priv->intr_to_host_ctrl, SCSC_MIFINTR_TARGET_WLAN);
		scsc_service_mifintrbit_unregister_tohost(service, hip->hip_priv->intr_to_host_ctrl_fb, SCSC_MIFINTR_TARGET_WLAN);
		return ret;
	}

	hip->hip_priv->intr_to_host_data1 = ret;
	SLSI_INFO_NODEV("HIP5: intr_to_host_data1 %d\n", hip->hip_priv->intr_to_host_data1);
	/* Mask the interrupt to prevent intr been kicked during start */
	scsc_service_mifintrbit_bit_mask(service, hip->hip_priv->intr_to_host_data1);

	ret = scsc_service_mifintrbit_register_tohost(service, hip5_irq_handler_stub, hip, SCSC_MIFINTR_TARGET_WLAN, HIP5_IRQ_HANDLER_STUB_TYPE);
	if (ret < 0) {
		SLSI_ERR_NODEV("Error allocating intr_to_host_data2\n");
		scsc_service_mifintrbit_unregister_tohost(service, hip->hip_priv->intr_to_host_ctrl, SCSC_MIFINTR_TARGET_WLAN);
		scsc_service_mifintrbit_unregister_tohost(service, hip->hip_priv->intr_to_host_ctrl_fb, SCSC_MIFINTR_TARGET_WLAN);
		scsc_service_mifintrbit_unregister_tohost(service, hip->hip_priv->intr_to_host_data1, SCSC_MIFINTR_TARGET_WLAN);
		return ret;
	}

	hip->hip_priv->intr_to_host_data2 = ret;
	/* Mask the interrupt to prevent intr been kicked during start */
	scsc_service_mifintrbit_bit_mask(service, hip->hip_priv->intr_to_host_data2);

	/* FROMHOST Handler allocator */
	ret = scsc_service_mifintrbit_alloc_fromhost(service, SCSC_MIFINTR_TARGET_WLAN);
	if (ret < 0) {
		SLSI_ERR_NODEV("Error allocating intr_from_host_ctrl\n");
		scsc_service_mifintrbit_unregister_tohost(service, hip->hip_priv->intr_to_host_ctrl, SCSC_MIFINTR_TARGET_WLAN);
		scsc_service_mifintrbit_unregister_tohost(service, hip->hip_priv->intr_to_host_ctrl_fb, SCSC_MIFINTR_TARGET_WLAN);
		scsc_service_mifintrbit_unregister_tohost(service, hip->hip_priv->intr_to_host_data1, SCSC_MIFINTR_TARGET_WLAN);
		scsc_service_mifintrbit_unregister_tohost(service, hip->hip_priv->intr_to_host_data2, SCSC_MIFINTR_TARGET_WLAN);
		return ret;
	}

	hip->hip_priv->intr_from_host_ctrl = ret;
	SLSI_INFO_NODEV("HIP5: intr_from_host_ctrl %d\n", hip->hip_priv->intr_from_host_ctrl);

	ret = scsc_service_mifintrbit_alloc_fromhost(service, SCSC_MIFINTR_TARGET_WLAN);
	if (ret < 0) {
		SLSI_ERR_NODEV("Error allocating intr_from_host_data\n");
		scsc_service_mifintrbit_unregister_tohost(service, hip->hip_priv->intr_to_host_ctrl, SCSC_MIFINTR_TARGET_WLAN);
		scsc_service_mifintrbit_unregister_tohost(service, hip->hip_priv->intr_to_host_ctrl_fb, SCSC_MIFINTR_TARGET_WLAN);
		scsc_service_mifintrbit_unregister_tohost(service, hip->hip_priv->intr_to_host_data1, SCSC_MIFINTR_TARGET_WLAN);
		scsc_service_mifintrbit_unregister_tohost(service, hip->hip_priv->intr_to_host_data2, SCSC_MIFINTR_TARGET_WLAN);
		scsc_service_mifintrbit_free_fromhost(service, hip->hip_priv->intr_from_host_ctrl, SCSC_MIFINTR_TARGET_WLAN);
		return ret;
	}

	hip->hip_priv->intr_from_host_data = ret;
	SLSI_INFO_NODEV("HIP5: intr_from_host_data %d\n", hip->hip_priv->intr_from_host_data);

#ifdef CONFIG_SCSC_WLAN_HOST_DPD
	/* TOHOST interrupt for DPD */
	ret = scsc_service_mifintrbit_register_tohost(service, hip5_irq_handler_dpd, hip, SCSC_MIFINTR_TARGET_WLAN, HIP5_IRQ_HANDLER_DPD_TYPE);
	if (ret < 0) {
		SLSI_ERR_NODEV("Error allocating intr_to_host_dpd\n");
		scsc_service_mifintrbit_unregister_tohost(service, hip->hip_priv->intr_to_host_ctrl, SCSC_MIFINTR_TARGET_WLAN);
		scsc_service_mifintrbit_unregister_tohost(service, hip->hip_priv->intr_to_host_ctrl_fb, SCSC_MIFINTR_TARGET_WLAN);
		scsc_service_mifintrbit_unregister_tohost(service, hip->hip_priv->intr_to_host_data1, SCSC_MIFINTR_TARGET_WLAN);
		scsc_service_mifintrbit_unregister_tohost(service, hip->hip_priv->intr_to_host_data2, SCSC_MIFINTR_TARGET_WLAN);
		scsc_service_mifintrbit_free_fromhost(service, hip->hip_priv->intr_from_host_ctrl, SCSC_MIFINTR_TARGET_WLAN);
		scsc_service_mifintrbit_free_fromhost(service, hip->hip_priv->intr_from_host_data, SCSC_MIFINTR_TARGET_WLAN);
		return ret;
	}

	hip->hip_priv->intr_to_host_dpd = ret;
	SLSI_INFO_NODEV("HIP5: intr_to_host_dpd %d\n", hip->hip_priv->intr_to_host_dpd);
	scsc_service_mifintrbit_bit_mask(service, hip->hip_priv->intr_to_host_dpd);

	/* FROMHOST interrupt for DPD */
	ret = scsc_service_mifintrbit_alloc_fromhost(service, SCSC_MIFINTR_TARGET_WLAN);
	if (ret < 0) {
		SLSI_ERR_NODEV("Error allocating intr_from_host_dpd\n");
		scsc_service_mifintrbit_unregister_tohost(service, hip->hip_priv->intr_to_host_ctrl, SCSC_MIFINTR_TARGET_WLAN);
		scsc_service_mifintrbit_unregister_tohost(service, hip->hip_priv->intr_to_host_ctrl_fb, SCSC_MIFINTR_TARGET_WLAN);
		scsc_service_mifintrbit_unregister_tohost(service, hip->hip_priv->intr_to_host_data1, SCSC_MIFINTR_TARGET_WLAN);
		scsc_service_mifintrbit_unregister_tohost(service, hip->hip_priv->intr_to_host_data2, SCSC_MIFINTR_TARGET_WLAN);
		scsc_service_mifintrbit_unregister_tohost(service, hip->hip_priv->intr_to_host_dpd, SCSC_MIFINTR_TARGET_WLAN);
		scsc_service_mifintrbit_free_fromhost(service, hip->hip_priv->intr_from_host_ctrl, SCSC_MIFINTR_TARGET_WLAN);
		scsc_service_mifintrbit_free_fromhost(service, hip->hip_priv->intr_from_host_data, SCSC_MIFINTR_TARGET_WLAN);
		return ret;
	}

	hip->hip_priv->intr_from_host_dpd = ret;
	SLSI_INFO_NODEV("HIP5: intr_from_host_dpd %d\n", hip->hip_priv->intr_from_host_dpd);
#endif

	/* Get hip_control pointer on shared memory  */
	hip_control = (struct hip5_hip_control *)(hip_ptr + HIP5_WLAN_CONFIG_OFFSET);

	ret = slsi_hip_init_control_table(sdev, hip, hip_ptr, hip_control, service);
	if (ret)
		return ret;

	hip->hip_control = hip_control;
	hip->hip_priv->scbrd_base = &hip_control->scoreboard;

	spin_lock_init(&hip->hip_priv->rx_lock);
	atomic_set(&hip->hip_priv->in_rx, 0);
	spin_lock_init(&hip->hip_priv->tx_lock);
	atomic_set(&hip->hip_priv->in_tx, 0);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
	slsi_wake_lock_init(NULL, &hip->hip_priv->wake_lock_tx.ws, "hip_wake_lock_tx");
	slsi_wake_lock_init(NULL, &hip->hip_priv->wake_lock_ctrl.ws, "hip_wake_lock_ctrl");
	slsi_wake_lock_init(NULL, &hip->hip_priv->wake_lock_data.ws, "hip_wake_lock_data");
#else
	slsi_wake_lock_init(&hip->hip_priv->wake_lock_tx, WAKE_LOCK_SUSPEND, "hip_wake_lock_tx");
	slsi_wake_lock_init(&hip->hip_priv->wake_lock_ctrl, WAKE_LOCK_SUSPEND, "hip_wake_lock_ctrl");
	slsi_wake_lock_init(&hip->hip_priv->wake_lock_data, WAKE_LOCK_SUSPEND, "hip_wake_lock_data");
#endif

	/* Init work structs */
	hip->hip_priv->hip_workq = create_singlethread_workqueue("hip5_work");
	if (!hip->hip_priv->hip_workq) {
		SLSI_ERR_NODEV("Error creating singlethread_workqueue\n");
		return -ENOMEM;
	}
	spin_lock_init(&hip->hip_priv->napi_cpu_lock);
	hip->hip_priv->bh_ctl = slsi_lbm_register_workqueue(sdev, hip5_wq_ctrl, -1);
	hip->hip_priv->bh_rfb = slsi_lbm_register_tasklet(sdev, hip5_tl_fb, -1);
	if (!hip->hip_priv->bh_ctl || !hip->hip_priv->bh_rfb) {
		SLSI_ERR_NODEV("Error allocating bh\n");
		return -ENOMEM;
	}

	rwlock_init(&hip->hip_priv->rw_scoreboard);

	/* init Shim layer for transmit aggregation */
	for (i = 0; i < SLSI_HIP_HIP5_OPT_TX_Q_MAX; i++) {
		skb_queue_head_init(&hip->hip_priv->hip5_opt_tx_q[i].tx_q);
	}

#ifndef CONFIG_SCSC_WLAN_TX_API
	atomic_set(&hip->hip_priv->gmod, HIP5_DAT_SLOTS);
	atomic_set(&hip->hip_priv->gactive, 1);
	spin_lock_init(&hip->hip_priv->gbot_lock);
	hip->hip_priv->saturated = 0;
#endif
#ifdef CONFIG_SCSC_QOS
	/* setup for PM QoS */
	spin_lock_init(&hip->hip_priv->pm_qos_lock);

	if (hip4_qos_enable) {
		if (!scsc_service_pm_qos_add_request(service, SCSC_QOS_DISABLED)) {
			/* register to traffic monitor for throughput events */
			if (slsi_traffic_mon_client_register(sdev, hip, TRAFFIC_MON_CLIENT_MODE_EVENTS, (hip4_qos_med_tput_in_mbps * 1000 * 1000), (hip4_qos_max_tput_in_mbps * 1000 * 1000), TRAFFIC_MON_DIR_DEFAULT, hip5_traffic_monitor_cb))
				SLSI_WARN(sdev, "failed to add PM QoS client to traffic monitor\n");
			else
				INIT_WORK(&hip->hip_priv->pm_qos_work, hip5_pm_qos_work);
		} else {
			SLSI_WARN(sdev, "failed to add PM QoS request\n");
		}
	}
#endif
#if IS_ENABLED(CONFIG_SCSC_LOGRING)
	/* register to traffic monitor for dynamic logring logging */
	if (slsi_traffic_mon_client_register(sdev, hip->hip_priv, TRAFFIC_MON_CLIENT_MODE_EVENTS, 0, (hip4_dynamic_logging_tput_in_mbps * 1000 * 1000), TRAFFIC_MON_DIR_DEFAULT, hip5_traffic_monitor_logring_cb))
		SLSI_WARN(sdev, "failed to add Logring client to traffic monitor\n");
#endif
	return 0;
}

/**
 * This function returns the number of free slots available to
 * transmit control packet.
 */
int slsi_hip_free_control_slots_count(struct slsi_hip *hip)
{
	return mbulk_pool_get_free_count(MBULK_POOL_ID_CTRL);
}

bool hip5_opt_aggr_check(struct slsi_hip *hip, struct sk_buff *skb)
{
	struct hip_priv     *hip_priv = hip->hip_priv;
	u8 i = 0;

	spin_lock_bh(&hip_priv->tx_lock);

	if (skb_headroom(skb) >= SLSI_HIP_HIP5_OPT_INVALID_OFFSET) {
		SLSI_DBG1_NODEV(SLSI_HIP, "large Headroom (%d), send in no-aggregation mode\n", skb_headroom(skb));
		spin_unlock_bh(&hip->hip_priv->tx_lock);
		return false;
	}

	for (i = 0; i < SLSI_HIP_HIP5_OPT_TX_Q_MAX; i++) {
		/* find a match */
		if (fapi_get_vif(skb) == hip_priv->hip5_opt_tx_q[i].vif_index &&
			fapi_get_u16(skb, u.ma_unitdata_req.flow_id) == hip_priv->hip5_opt_tx_q[i].flow_id &&
			fapi_get_u16(skb, u.ma_unitdata_req.configuration_option) == hip_priv->hip5_opt_tx_q[i].configuration_option) {
			if (ether_addr_equal(fapi_get_buff(skb, u.ma_unitdata_req.address), hip_priv->hip5_opt_tx_q[i].addr3)) {
				skb_queue_tail(&hip_priv->hip5_opt_tx_q[i].tx_q, skb);
				spin_unlock_bh(&hip->hip_priv->tx_lock);
				return true;
			}
		}
	}
	/* find an empty place */
	for (i = 0; i < SLSI_HIP_HIP5_OPT_TX_Q_MAX; i++) {
		if (!hip_priv->hip5_opt_tx_q[i].flow_id) {
			hip_priv->hip5_opt_tx_q[i].flow_id = fapi_get_u16(skb, u.ma_unitdata_req.flow_id);
			hip_priv->hip5_opt_tx_q[i].vif_index = fapi_get_vif(skb);
			hip_priv->hip5_opt_tx_q[i].configuration_option = fapi_get_u16(skb, u.ma_unitdata_req.configuration_option);
			memcpy(hip_priv->hip5_opt_tx_q[i].addr3, fapi_get_buff(skb, u.ma_unitdata_req.address), ETH_ALEN);
			SLSI_DBG1_NODEV(SLSI_HIP, "added %d, %pM, %d\n", i, hip_priv->hip5_opt_tx_q[i].addr3, hip_priv->hip5_opt_tx_q[i].flow_id);
			skb_queue_tail(&hip_priv->hip5_opt_tx_q[i].tx_q, skb);
			spin_unlock_bh(&hip->hip_priv->tx_lock);
			return true;
		} else {
			/* is it stale, if so delete */
			if (ktime_to_ms(ktime_sub(ktime_get(), hip_priv->hip5_opt_tx_q[i].last_sent)) > 1 * 1000) {
				if (skb_queue_empty(&hip_priv->hip5_opt_tx_q[i].tx_q)) {
					SLSI_DBG1_NODEV(SLSI_HIP, "deleted %d, %pM, %d\n", i, hip_priv->hip5_opt_tx_q[i].addr3, hip_priv->hip5_opt_tx_q[i].flow_id);
					hip_priv->hip5_opt_tx_q[i].flow_id = 0;
				}
			}
		}
	}
	spin_unlock_bh(&hip->hip_priv->tx_lock);
	return false;
}

/**
 * This function is in charge to transmit a frame through the HIP.
 * It does NOT take ownership of the SKB unless it successfully transmit it;
 * as a consequence skb is NOT freed on error.
 * We return ENOSPC on queue related troubles in order to trigger upper
 * layers of kernel to requeue/retry.
 * We free ONLY locally-allocated stuff.
 *
 * the vif_index, peer_index, priority fields are valid for data packets only
 */
int hip5_opt_tx_frame(struct slsi_hip *hip, struct sk_buff *skb, bool ctrl_packet, u8 vif_index, u8 peer_index, u8 priority)
{
	struct scsc_service 	  *service;
	struct netdev_vif *ndev_vif;
	scsc_mifram_ref 		  offset = 0;
	struct mbulk			  *m = NULL;
	mbulk_colour			  colour = 0;
	struct slsi_dev 		  *sdev = container_of(hip, struct slsi_dev, hip);
	struct fapi_signal_header *fapi_header;
	int 					  ret = 0;
	size_t              payload = 0;
	struct sk_buff *skb_fapi;
	struct sk_buff *skb_udi;
	struct slsi_skb_cb *cb = slsi_skb_cb_get(skb);

	spin_lock_bh(&hip->hip_priv->tx_lock);
	atomic_set(&hip->hip_priv->in_tx, 1);

	if (!slsi_wake_lock_active(&hip->hip_priv->wake_lock_tx))
		slsi_wake_lock_timeout(&hip->hip_priv->wake_lock_tx, msecs_to_jiffies(SLSI_HIP_WAKELOCK_TIME_OUT_IN_MS));

	/* clone the skb for logging */
	skb_udi = skb_clone(skb, GFP_ATOMIC);

	service = sdev->service;
	fapi_header = (struct fapi_signal_header *)skb->data;

	if (fapi_is_ma(skb)) {
		u8 vif_index_colour = vif_index;

		if (skb->dev) {
			ndev_vif = netdev_priv(skb->dev);
			if (ndev_vif->iftype != NL80211_IFTYPE_AP_VLAN)
				vif_index_colour = ndev_vif->ifnum;
		}
#ifndef CONFIG_SCSC_WLAN_TX_API
		SLSI_MBULK_COLOUR_SET(colour, vif_index_colour, peer_index, priority);
#else
		SLSI_MBULK_COLOUR_SET(colour, vif_index_colour, priority);
#endif
	}
	/* create a Mbulk only if the Signal has a payload (bulk data) */
	payload = skb->len - cb->sig_length;
	if (payload) {
		m = hip5_skb_to_mbulk(hip->hip_priv, skb, ctrl_packet, colour);
		if (!m) {
			SCSC_HIP4_SAMPLER_MFULL(hip->hip_priv->minor);
			ret = -ENOSPC;
			SLSI_ERR_NODEV("mbulk is NULL\n");
			goto error;
		}

		if (scsc_mx_service_mif_ptr_to_addr(service, m, &offset) < 0) {
			mbulk_free_virt_host(m);
			ret = -EFAULT;
			SLSI_ERR_NODEV("Incorrect reference memory\n");
			goto error;
		}
	}
	skb_fapi = skb_clone(skb, GFP_ATOMIC);
	if (!skb_fapi) {
		SLSI_WARN(sdev, "fail to allocate memory for FAPI SKB\n");
		ret = -ENOMEM;
		goto error;
	}
	skb_trim(skb_fapi, cb->sig_length);
	skb_pull(skb, cb->sig_length);

	if (hip5_opt_hip_signal_add(hip, ctrl_packet ? HIP5_MIF_Q_FH_CTRL : HIP5_MIF_Q_FH_DAT0, offset, service, m, skb_fapi)) {
		if (m)
			mbulk_free_virt_host(m);

		SLSI_ERR_NODEV("No space\n");
		consume_skb(skb_fapi);
		ret = -ENOSPC;
		goto error;
	}
	consume_skb(skb_fapi);

	if (ctrl_packet) {
		/* Record control signal */
		SCSC_HIP4_SAMPLER_SIGNAL_CTRLTX(hip->hip_priv->minor, (fapi_header->id & 0xff00) >> 8, fapi_header->id & 0xff);
	} else {
		SCSC_HIP4_SAMPLER_VIF_PEER(hip->hip_priv->minor, 1, vif_index, peer_index);
	}

	/* Here we push a copy of the bare skb TRANSMITTED data also to the logring
	 * as a binary record. Note that bypassing UDI subsystem as a whole
	 * means we are losing:
	 *	 UDI filtering / UDI Header INFO / UDI QueuesFrames Throttling /
	 *	 UDI Skb Asynchronous processing
	 * We keep separated DATA/CTRL paths.
	 */
	if (ctrl_packet)
		SCSC_BIN_TAG_DEBUG(BIN_WIFI_CTRL_TX, skb_udi->data, skb_headlen(skb));
	else
		SCSC_BIN_TAG_DEBUG(BIN_WIFI_DATA_TX, skb_udi->data, skb_headlen(skb));
	/* slsi_log_clients_log_signal_fast: skb is copied to all the log clients */
	slsi_log_clients_log_signal_fast(sdev, &sdev->log_clients, skb_udi, SLSI_LOG_DIRECTION_FROM_HOST);

	consume_skb(skb);
	consume_skb(skb_udi);
	atomic_set(&hip->hip_priv->in_tx, 0);
	spin_unlock_bh(&hip->hip_priv->tx_lock);
	return 0;
error:
	if (slsi_wake_lock_active(&hip->hip_priv->wake_lock_tx))
		slsi_wake_unlock(&hip->hip_priv->wake_lock_tx);
	consume_skb(skb_udi);
	atomic_set(&hip->hip_priv->in_tx, 0);
	spin_unlock_bh(&hip->hip_priv->tx_lock);
	return ret;
}

int slsi_hip_transmit_frame(struct slsi_hip *hip, struct sk_buff *skb, bool ctrl_packet, u8 vif_index, u8 peer_index, u8 priority)
{
	struct scsc_service       *service;
	scsc_mifram_ref           offset;
	struct mbulk              *m;
	mbulk_colour              colour = 0;
	struct slsi_dev           *sdev = container_of(hip, struct slsi_dev, hip);
	struct fapi_signal_header *fapi_header;
	int                       ret = 0;
	struct sk_buff *skb_fapi;
	struct sk_buff *skb_udi;
	struct netdev_vif *ndev_vif;

	if (!hip || !sdev || !sdev->service || !skb || !hip->hip_priv)
		return -EINVAL;

	if (hip->hip_priv->version == 5) {
		if (!ctrl_packet) {
			if (hip5_opt_aggr_check(hip, skb)) {
				/* frame is queued and will be transmit in batch */
				return 0;
			}
		}
		return hip5_opt_tx_frame(hip, skb, ctrl_packet, vif_index, peer_index, priority);
	}

	spin_lock_bh(&hip->hip_priv->tx_lock);
	atomic_set(&hip->hip_priv->in_tx, 1);

	if (!slsi_wake_lock_active(&hip->hip_priv->wake_lock_tx))
		slsi_wake_lock_timeout(&hip->hip_priv->wake_lock_tx, msecs_to_jiffies(SLSI_HIP_WAKELOCK_TIME_OUT_IN_MS));

	/* clone the skb for logging */
	skb_udi = skb_clone(skb, GFP_ATOMIC);

	service = sdev->service;
	fapi_header = (struct fapi_signal_header *)skb->data;

	if (fapi_is_ma(skb)) {
		u8 vif_index_colour = vif_index;

		if (skb->dev) {
			ndev_vif = netdev_priv(skb->dev);
			if (ndev_vif->iftype != NL80211_IFTYPE_AP_VLAN)
				vif_index_colour = ndev_vif->ifnum;
		}
#ifndef CONFIG_SCSC_WLAN_TX_API
		SLSI_MBULK_COLOUR_SET(colour, vif_index_colour, peer_index, priority);
#else
		SLSI_MBULK_COLOUR_SET(colour, vif_index_colour, priority);
#endif
	}
	m = hip5_skb_to_mbulk(hip->hip_priv, skb, ctrl_packet, colour);
	if (!m) {
		SCSC_HIP4_SAMPLER_MFULL(hip->hip_priv->minor);
		ret = -ENOSPC;
		SLSI_ERR_NODEV("mbulk is NULL\n");
		goto error;
	}

	if (scsc_mx_service_mif_ptr_to_addr(service, m, &offset) < 0) {
		mbulk_free_virt_host(m);
		ret = -EFAULT;
		SLSI_ERR_NODEV("Incorrect reference memory\n");
		goto error;
	}

	if (ctrl_packet) {
		if (hip5_q_add_signal(hip, HIP5_MIF_Q_FH_CTRL, offset, service)) {
			SCSC_HIP4_SAMPLER_QFULL(hip->hip_priv->minor, ctrl_packet ? HIP5_MIF_Q_FH_CTRL : HIP5_MIF_Q_FH_DAT0);
			mbulk_free_virt_host(m);
			ret = -ENOSPC;
			SLSI_ERR_NODEV("No space\n");
			goto error;
		}
	} else {
		skb_fapi = skb_clone(skb, GFP_ATOMIC);
		if (!skb_fapi) {
			SLSI_WARN(sdev, "fail to allocate memory for FAPI SKB\n");
			ret = -ENOMEM;
			goto error;
		}
		skb_trim(skb_fapi, fapi_sig_size(ma_unitdata_req));

		if (hip5_q_add_bd_entry(hip, slsi_hip_vif_to_q_mapping(vif_index), offset, service, m, skb_fapi->data)) {
			mbulk_free_virt_host(m);
			ret = -ENOSPC;
			SLSI_ERR_NODEV("No space\n");
			goto error;
		}
		consume_skb(skb_fapi);
	}

	if (ctrl_packet) {
		/* Record control signal */
		SCSC_HIP4_SAMPLER_SIGNAL_CTRLTX(hip->hip_priv->minor, (fapi_header->id & 0xff00) >> 8, fapi_header->id & 0xff);
	} else {
		SCSC_HIP4_SAMPLER_VIF_PEER(hip->hip_priv->minor, 1, vif_index, peer_index);
	}

	/* Here we push a copy of the bare skb TRANSMITTED data also to the logring
	 * as a binary record. Note that bypassing UDI subsystem as a whole
	 * means we are losing:
	 *   UDI filtering / UDI Header INFO / UDI QueuesFrames Throttling /
	 *   UDI Skb Asynchronous processing
	 * We keep separated DATA/CTRL paths.
	 */
	if (ctrl_packet)
		SCSC_BIN_TAG_DEBUG(BIN_WIFI_CTRL_TX, skb_udi->data, skb_headlen(skb));
	else
		SCSC_BIN_TAG_DEBUG(BIN_WIFI_DATA_TX, skb_udi->data, skb_headlen(skb));
	/* slsi_log_clients_log_signal_fast: skb is copied to all the log clients */
	slsi_log_clients_log_signal_fast(sdev, &sdev->log_clients, skb_udi, SLSI_LOG_DIRECTION_FROM_HOST);

	consume_skb(skb);
	consume_skb(skb_udi);
	atomic_set(&hip->hip_priv->in_tx, 0);
	spin_unlock_bh(&hip->hip_priv->tx_lock);
	return 0;
error:
	if (slsi_wake_lock_active(&hip->hip_priv->wake_lock_tx))
		slsi_wake_unlock(&hip->hip_priv->wake_lock_tx);
	consume_skb(skb_udi);
	atomic_set(&hip->hip_priv->in_tx, 0);
	spin_unlock_bh(&hip->hip_priv->tx_lock);
	return ret;
}

static int hip5_opt_zero_copy_dma_desc_to_bd(struct hip_priv *hip_priv, struct hip5_tx_dma_desc * dma_desc, struct hip5_opt_bulk_desc  *bulk_desc)
{
	u16 i = 0;

	/* find an index in look up table */
	for (i = 0; i < MAX_NUM; i++)
		if (!hip_priv->tx_skb_table[i].in_use)
			break;

	if(i >= MAX_NUM) {
		SLSI_ERR_NODEV("no space in SKB look-up table\n");
		return -ENOMEM;
	}

	hip_priv->tx_skb_free_cnt--;
	SCSC_HIP4_SAMPLER_MBULK(hip_priv->minor, (hip_priv->tx_skb_free_cnt & 0xFF00) >> 8, (hip_priv->tx_skb_free_cnt & 0xFF), MBULK_POOL_ID_DATA, hip5_tx_zero_copy_tx_slots);

	/* store the Tx SKB */
	hip_priv->tx_skb_table[i].skb_dma_addr = ((dma_desc->skb_dma_addr >> 4) | 0x1);
	hip_priv->tx_skb_table[i].colour = dma_desc->colour;
	hip_priv->tx_skb_table[i].skb = dma_desc->skb;
	hip_priv->tx_skb_table[i].in_use = true;

	bulk_desc->buf_addr = (((dma_desc->skb_dma_addr >> 4) | 0x1));
	bulk_desc->buf_sz = 2; /* 512 * (1 << buf_sz) */
	bulk_desc->data_len = dma_desc->len;
	bulk_desc->flag = 0;
	bulk_desc->offset = dma_desc->offset;
	return 0;
}


void hip5_opt_aggr_tx_frame(struct scsc_service *service, struct slsi_hip *hip)
{
	struct hip_priv           *hip_priv = hip->hip_priv;
	struct slsi_dev           *sdev = container_of(hip, struct slsi_dev, hip);
	struct hip5_hip_control   *ctrl = hip->hip_control;
	struct sk_buff            *skb_fapi = NULL;
	struct sk_buff            *skb_udi = NULL;
	struct mbulk			  *m = NULL;
	struct hip5_tx_dma_desc	  tx_desc_list[HIP5_OPT_NUM_BD_PER_SIGNAL_MAX];
	scsc_mifram_ref 		  offset;
	struct mbulk			  *m_arr[HIP5_OPT_NUM_BD_PER_SIGNAL_MAX];
	mbulk_colour			  colour = 0;
	u16                       idx_w;
	u16                       idx_r;
	struct hip5_opt_hip_signal *hip5_q_entry = NULL;
	struct hip5_opt_bulk_desc  *bulk_desc = NULL;
	u8 padding = 0;
	u8 i =0, j = 0, idx = 0;

	spin_lock_bh(&hip->hip_priv->tx_lock);
	atomic_set(&hip->hip_priv->in_tx, 1);

	if (!slsi_wake_lock_active(&hip->hip_priv->wake_lock_tx))
		slsi_wake_lock_timeout(&hip->hip_priv->wake_lock_tx, msecs_to_jiffies(SLSI_HIP_WAKELOCK_TIME_OUT_IN_MS));

	memset(m_arr, 0, sizeof(m_arr));
	memset(tx_desc_list, 0, HIP5_OPT_NUM_BD_PER_SIGNAL_MAX * sizeof(struct hip5_tx_dma_desc));
	for (i = 0; i < SLSI_HIP_HIP5_OPT_TX_Q_MAX; i++) {
		/* check if q is full before dequeing SKB */
		idx_w = hip5_read_index(hip, HIP5_MIF_Q_FH_DAT0, widx);
		idx_r = hip5_read_index(hip, HIP5_MIF_Q_FH_DAT0, ridx);

		if ((idx_r == 0xFFFF) || (idx_w == 0xFFFF)) {
			SLSI_ERR_NODEV("invalid Index\n");
			goto tx_done;
		}

		if (idx_w >= MAX_NUM) {
			SLSI_ERR_NODEV("invalid Write index (idx_w:%d)\n", idx_w);
			goto tx_done;
		}

		if (skb_queue_len(&hip_priv->hip5_opt_tx_q[i].tx_q)) {
			if (idx_r == ((idx_w + 1) & (MAX_NUM - 1))) {
				SLSI_WARN_NODEV("no space to write %d BDs (idx_r:%d, idx_w:%d)\n", skb_queue_len(&hip_priv->hip5_opt_tx_q[i].tx_q), idx_r, idx_w);
				goto tx_done;
			}
		}
		/* do we need more than 1 HIP entry */
		if (skb_queue_len(&hip_priv->hip5_opt_tx_q[i].tx_q) > 3) {
			u8 cnt = 0;
			u16 idx_w_temp = (idx_w + 1) & (MAX_NUM - 1);

			for (cnt = 0; cnt < ((skb_queue_len(&hip_priv->hip5_opt_tx_q[i].tx_q) - 3) > 64 ? 64 : skb_queue_len(&hip_priv->hip5_opt_tx_q[i].tx_q)); cnt = cnt + 8) {
				idx_w_temp = idx_w_temp + 1;
				idx_w_temp &= (MAX_NUM - 1);

				if (idx_r == idx_w_temp) {
					SLSI_WARN_NODEV("no space to write %d BDs (idx_r:%d, idx_w:%d)\n", ((skb_queue_len(&hip_priv->hip5_opt_tx_q[i].tx_q) - 3) > 64 ? 64 : skb_queue_len(&hip_priv->hip5_opt_tx_q[i].tx_q)), idx_r, idx_w);
					goto tx_done;
				}
			}
		}
		while ((idx < HIP5_OPT_NUM_BD_PER_SIGNAL_MAX) && !skb_queue_empty(&hip_priv->hip5_opt_tx_q[i].tx_q)) {
			struct sk_buff *skb;
			u8 vif_index_colour;

			skb = __skb_dequeue(&hip_priv->hip5_opt_tx_q[i].tx_q);
			if (!skb) {
				SLSI_ERR_NODEV("no SKB\n");
				goto send_signal;
			}

			/* clone the skb for logging */
			skb_udi = skb_clone(skb, GFP_ATOMIC);

			vif_index_colour = fapi_get_vif(skb);
			if (skb->dev) {
				struct netdev_vif *ndev_vif = netdev_priv(skb->dev);

				if (ndev_vif->iftype != NL80211_IFTYPE_AP_VLAN)
					vif_index_colour = ndev_vif->ifnum;
			}

#ifndef CONFIG_SCSC_WLAN_TX_API
			SLSI_MBULK_COLOUR_SET(colour,
			vif_index_colour,
			slsi_skb_cb_get(skb)->peer_idx,
			slsi_frame_priority_to_ac_queue(skb->priority));
#else
			SLSI_MBULK_COLOUR_SET(colour, vif_index_colour, slsi_frame_priority_to_ac_queue(skb->priority));
#endif
			if (!skb_fapi) {
				skb_fapi = skb_clone(skb, GFP_ATOMIC);
				if (!skb_fapi) {
					SLSI_WARN(sdev, "fail to clone SKB for skb_fapi\n");
					consume_skb(skb);
					goto tx_done;
				}
				skb_trim(skb_fapi, slsi_skb_cb_get(skb)->sig_length);
			}
			if (hip5_tx_zero_copy) {
				skb_pull(skb, fapi_sig_size(ma_unitdata_req));
				tx_desc_list[idx].len = skb->len;
				tx_desc_list[idx].offset = skb_headroom(skb);
				tx_desc_list[idx].colour = colour;
				tx_desc_list[idx].skb = skb;
				tx_desc_list[idx].skb_dma_addr = dma_map_single(sdev->dev, skb->head, skb_headroom(skb) + skb_headlen(skb), DMA_TO_DEVICE);

				if (dma_mapping_error(sdev->dev, tx_desc_list[idx].skb_dma_addr)) {
					SLSI_ERR_NODEV("DMA mapping error\n");
					consume_skb(skb);
					goto send_signal;
				}
				m_arr[idx] = (struct mbulk *)&tx_desc_list[idx];
				idx++;
			} else {
				/* create a Mbulk only if the Signal has a payload (bulk data) */
				m = hip5_skb_to_mbulk(hip->hip_priv, skb, false, colour);
				if (!m) {
					SLSI_ERR_NODEV("mbulk is NULL\n");
					consume_skb(skb);
					goto send_signal;
				}
				m_arr[idx++] = m;
				consume_skb(skb);
			}
			slsi_log_clients_log_signal_fast(sdev, &sdev->log_clients, skb, SLSI_LOG_DIRECTION_FROM_HOST);
			consume_skb(skb_udi);
		}
send_signal:
		if (skb_fapi) {
			hip5_q_entry = (struct hip5_opt_hip_signal *)&ctrl->q_tlv[0].array[idx_w];
			memset(hip5_q_entry, 0, sizeof(struct hip5_opt_hip_signal));

			if (idx > 3) {
				u8 k = 0;
				u8 mult = 0;

				for (k = 0; k < (idx - 3); k = k + 8) {
					mult++;
				}
				/* TODO: HIP entry wrap around */
				memset(hip5_q_entry + 64 , 0, (sizeof(struct hip5_opt_hip_signal) * mult));
			}

			/* HIP signal header */
			hip5_q_entry->sig_format = 0; /* reserved */
			hip5_q_entry->reserved = 0;   /* reserved */

			hip5_q_entry->sig_len = skb_fapi->len;
			SLSI_DBG4(sdev, SLSI_HIP, "idx_r:%d, idx_w:%d, vif:%d, sig_len:%d, num_bd:%d\n",
				idx_r,
				idx_w,
				fapi_get_vif(skb_fapi),
				hip5_q_entry->sig_len,
				idx);

			/* FAPI signal */
			memcpy((void *)hip5_q_entry + 4, skb_fapi->data, skb_fapi->len);

			/* padding for bulk descriptor to start on 8 byte aligned */
			padding = (8 - ((skb_fapi->len + 4)  % 8)) & 0x7;
			bulk_desc = (struct hip5_opt_bulk_desc *) ((void *)hip5_q_entry + 4 + skb_fapi->len + padding);

			for (j = 0; j < HIP5_OPT_NUM_BD_PER_SIGNAL_MAX; j++) {
				/* bulk descriptor(s) */
				if (m_arr[j]) {
					if (hip5_tx_zero_copy) {
						if (hip5_opt_zero_copy_dma_desc_to_bd(hip_priv, (struct hip5_tx_dma_desc *)m_arr[j], bulk_desc)) {
							SLSI_ERR_NODEV("invalid Tx descriptor\n");
							goto tx_done;
						}
					} else {
						if (scsc_mx_service_mif_ptr_to_addr(service, m_arr[j], &offset) < 0) {
							mbulk_free_virt_host(m_arr[j]);
							SLSI_ERR_NODEV("Incorrect reference memory\n");
							goto tx_done;
						}

						bulk_desc->buf_addr = offset;
						bulk_desc->buf_sz = 2; /* 512 * (1<<buf_sz) */
						bulk_desc->data_len = m_arr[j]->len;
						bulk_desc->flag = m_arr[j]->flag;
						bulk_desc->offset = m_arr[j]->head + 20;
					}
					SLSI_DBG4(sdev, SLSI_HIP, "idx:%d, buf_addr:0x%lX data_len:%d, flag:0x%X, offset:%d\n",
						j,
						bulk_desc->buf_addr,
						bulk_desc->data_len,
						bulk_desc->flag,
						bulk_desc->offset);

					hip5_q_entry->num_bd++;
					bulk_desc++;
					if((void *) bulk_desc == ((void *)&ctrl->q_tlv[0].array[MAX_NUM - 1] + 64)) {
						bulk_desc = (struct hip5_opt_bulk_desc *)&ctrl->q_tlv[0].array[0];
					}
				}
			}

			/* Memory barrier before updating shared mailbox */
			smp_wmb();

			/* Increase index */
			idx_w++;
			idx_w &= (MAX_NUM - 1);

			if (hip5_q_entry->num_bd > 3) {
				u8 num_bd = 0;
				u8 i = 0;
				u8 entries = 0;

				num_bd = hip5_q_entry->num_bd - 3;

				for (i = 0; i < num_bd; i = i + 8) {
					entries++;
					idx_w++;
					idx_w &= (MAX_NUM - 1);
				}
			}
			/* Update the scoreboard */
			hip5_update_index(hip, HIP5_MIF_Q_FH_DAT0, widx, idx_w);
			consume_skb(skb_fapi);
		}

		/* reset for next HIP queue */
		hip_priv->hip5_opt_tx_q[i].last_sent = ktime_get();
		skb_fapi = NULL;
		idx = 0;
		memset(m_arr, 0, sizeof(m_arr));
	}
tx_done:
	if (slsi_wake_lock_active(&hip->hip_priv->wake_lock_tx))
		slsi_wake_unlock(&hip->hip_priv->wake_lock_tx);
	atomic_set(&hip->hip_priv->in_tx, 0);
	spin_unlock_bh(&hip->hip_priv->tx_lock);
}


void slsi_hip_from_host_intr_set(struct scsc_service *service, struct slsi_hip *hip)
{
	if (hip->hip_priv->version == 5) {
		hip5_opt_aggr_tx_frame(service, hip);
	}
	scsc_service_mifintrbit_bit_set(service, hip->hip_priv->intr_from_host_ctrl, SCSC_MIFINTR_TARGET_WLAN);
}

/* HIP has been initialize, setup with values
 * provided by FW
 */
int slsi_hip_setup(struct slsi_hip *hip)
{
	struct slsi_dev     *sdev = container_of(hip, struct slsi_dev, hip);
	struct scsc_service *service;
	u32 hip_config_ver = 0;

	if (!sdev || !sdev->service)
		return -EIO;

	if (atomic_read(&sdev->hip.hip_state) != SLSI_HIP_STATE_STARTED)
		return -EIO;

	service = sdev->service;

	/* Get the Version reported by the FW */
	hip_config_ver = scsc_wifi_get_hip_config_version(&hip->hip_control->init);
	SLSI_ERR_NODEV("FW HIP config version %d\n", hip_config_ver);
	/* Check if the version is supported. And get the index */
	/* This is hardcoded and may change in future versions */
	if (hip_config_ver != 4 && hip_config_ver != 5) {
		SLSI_ERR_NODEV("FW HIP config version %d not supported\n", hip_config_ver);
		return -EIO;
	}

	if (hip_config_ver == 4) {
		hip->hip_priv->unidat_req_headroom =
			scsc_wifi_get_hip_config_u8(&hip->hip_control, unidat_req_headroom, 4);
		hip->hip_priv->unidat_req_tailroom =
			scsc_wifi_get_hip_config_u8(&hip->hip_control, unidat_req_tailroom, 4);
		hip->hip_priv->version = 4;
	} else {
		/* version 5 */
		hip->hip_priv->unidat_req_headroom =
			scsc_wifi_get_hip_config_u8(&hip->hip_control, unidat_req_headroom, 5);
		hip->hip_priv->unidat_req_tailroom =
			scsc_wifi_get_hip_config_u8(&hip->hip_control, unidat_req_tailroom, 5);
		hip->hip_priv->version = 5;

		if (hip5_scoreboard_in_ramrp) {
			SLSI_ERR_NODEV("RAMRP offset: 0x%4X\n", (scsc_wifi_get_hip_config_u32(&hip->hip_control, scbrd_loc, 5) & ~1));
			hip->hip_priv->scbrd_ramrp_base = hip->hip_priv->scbrd_ramrp_base + (scsc_wifi_get_hip_config_u32(&hip->hip_control, scbrd_loc, 5) & ~1);
		}
	}

#ifdef CONFIG_SCSC_WLAN_TX_API
	if (hip5_tx_zero_copy) {
		slsi_hip_setup_post(sdev, hip5_tx_zero_copy_tx_slots);
	}
#endif
	/* Unmask interrupts - now host should handle them */
	atomic_set(&sdev->debug_inds, 0);
	atomic_set(&hip->hip_priv->closing, 0);

	scsc_service_mifintrbit_bit_unmask(service, hip->hip_priv->intr_to_host_ctrl);
	scsc_service_mifintrbit_bit_unmask(service, hip->hip_priv->intr_to_host_ctrl_fb);
	scsc_service_mifintrbit_bit_unmask(service, hip->hip_priv->intr_to_host_data1);
	scsc_service_mifintrbit_bit_unmask(service, hip->hip_priv->intr_to_host_data2);
#ifdef CONFIG_SCSC_WLAN_HOST_DPD
	scsc_service_mifintrbit_bit_unmask(service, hip->hip_priv->intr_to_host_dpd);
	slsi_wlan_dpd_mmap_user_space_event(SLSI_WLAN_DPD_DRV_MSG_ID_WLAN_ON);
#endif
	return 0;
}

/* On suspend hip needs to ensure that TH interrupts *are* unmasked */
void slsi_hip_suspend(struct slsi_hip *hip)
{
	struct slsi_dev *sdev;

	if (!hip || !hip->hip_priv)
		return;

	sdev = container_of(hip, struct slsi_dev, hip);

	if (atomic_read(&sdev->hip.hip_state) != SLSI_HIP_STATE_STARTED)
		return;

	memset(&sdev->suspend_tm, 0, sizeof(struct rtc_time));
	if (slsi_hip_wlan_get_rtc_time(&sdev->suspend_tm))
		SLSI_ERR_NODEV("Error reading rtc\n");

	slsi_log_client_msg(sdev, UDI_DRV_SUSPEND_IND, 0, NULL);
	SCSC_HIP4_SAMPLER_SUSPEND(hip->hip_priv->minor);

	atomic_set(&hip->hip_priv->in_suspend, 1);
}

void slsi_hip_resume(struct slsi_hip *hip)
{
	struct slsi_dev *sdev;
	if (!hip || !hip->hip_priv)
		return;

	sdev = container_of(hip, struct slsi_dev, hip);

	if (atomic_read(&sdev->hip.hip_state) != SLSI_HIP_STATE_STARTED)
		return;

	slsi_log_client_msg(sdev, UDI_DRV_RESUME_IND, 0, NULL);
	SCSC_HIP4_SAMPLER_RESUME(hip->hip_priv->minor);
	atomic_set(&hip->hip_priv->in_suspend, 0);
}

void slsi_hip_freeze(struct slsi_hip *hip)
{
	struct slsi_dev *sdev;
	struct scsc_service *service;

	if (!hip || !hip->hip_priv)
		return;

	sdev = container_of(hip, struct slsi_dev, hip);
	if (!sdev || !sdev->service)
		return;

	if (atomic_read(&sdev->hip.hip_state) != SLSI_HIP_STATE_STARTED)
		return;

#ifdef CONFIG_SCSC_WLAN_HOST_DPD
	slsi_wlan_dpd_mmap_user_space_event(SLSI_WLAN_DPD_DRV_MSG_ID_WLAN_OFF);
#endif

	service = sdev->service;

	closing = ktime_get();
	atomic_set(&hip->hip_priv->closing, 1);

	hip5_dump_dbg(hip, NULL, NULL, service);

	scsc_service_mifintrbit_bit_mask(service, hip->hip_priv->intr_to_host_ctrl);
	scsc_service_mifintrbit_bit_mask(service, hip->hip_priv->intr_to_host_ctrl_fb);
	scsc_service_mifintrbit_bit_mask(service, hip->hip_priv->intr_to_host_data1);
	scsc_service_mifintrbit_bit_mask(service, hip->hip_priv->intr_to_host_data2);

	flush_workqueue(hip->hip_priv->hip_workq);
	destroy_workqueue(hip->hip_priv->hip_workq);
}

void slsi_hip_deinit(struct slsi_hip *hip)
{
	struct slsi_dev     *sdev;
	struct scsc_service *service;

	if (!hip || !hip->hip_priv)
		return;

	sdev = container_of(hip, struct slsi_dev, hip);
	if (!sdev || !sdev->service)
		return;

	service = sdev->service;

#if IS_ENABLED(CONFIG_SCSC_LOGRING)
	slsi_traffic_mon_client_unregister(sdev, hip->hip_priv);
	/* Reenable logring in case was disabled */
	scsc_logring_enable(true);
#endif
#ifdef CONFIG_SCSC_QOS
	/* de-register with traffic monitor */
	slsi_traffic_mon_client_unregister(sdev, hip);
	scsc_service_pm_qos_remove_request(service);
#endif

	closing = ktime_get();
	atomic_set(&hip->hip_priv->closing, 1);

	scsc_service_mifintrbit_bit_mask(service, hip->hip_priv->intr_to_host_ctrl);
	scsc_service_mifintrbit_bit_mask(service, hip->hip_priv->intr_to_host_ctrl_fb);
	scsc_service_mifintrbit_bit_mask(service, hip->hip_priv->intr_to_host_data1);
	scsc_service_mifintrbit_bit_mask(service, hip->hip_priv->intr_to_host_data2);

	slsi_lbm_unregister_bh(hip->hip_priv->bh_dat);
	slsi_lbm_unregister_bh(hip->hip_priv->bh_ctl);
	slsi_lbm_unregister_bh(hip->hip_priv->bh_rfb);
	hip->hip_priv->bh_dat = NULL;
	hip->hip_priv->bh_ctl = NULL;
	hip->hip_priv->bh_rfb = NULL;

	scsc_service_mifintrbit_unregister_tohost(service, hip->hip_priv->intr_to_host_ctrl, SCSC_MIFINTR_TARGET_WLAN);
	scsc_service_mifintrbit_unregister_tohost(service, hip->hip_priv->intr_to_host_ctrl_fb, SCSC_MIFINTR_TARGET_WLAN);
	scsc_service_mifintrbit_unregister_tohost(service, hip->hip_priv->intr_to_host_data1, SCSC_MIFINTR_TARGET_WLAN);
	scsc_service_mifintrbit_unregister_tohost(service, hip->hip_priv->intr_to_host_data2, SCSC_MIFINTR_TARGET_WLAN);

	flush_workqueue(hip->hip_priv->hip_workq);
	destroy_workqueue(hip->hip_priv->hip_workq);

	scsc_service_mifintrbit_free_fromhost(service, hip->hip_priv->intr_from_host_ctrl, SCSC_MIFINTR_TARGET_WLAN);
	scsc_service_mifintrbit_free_fromhost(service, hip->hip_priv->intr_from_host_data, SCSC_MIFINTR_TARGET_WLAN);

#ifdef CONFIG_SCSC_WLAN_HOST_DPD
	scsc_service_mifintrbit_bit_mask(service, hip->hip_priv->intr_to_host_dpd);
	scsc_service_mifintrbit_unregister_tohost(service, hip->hip_priv->intr_to_host_dpd, SCSC_MIFINTR_TARGET_WLAN);
	scsc_service_mifintrbit_free_fromhost(service, hip->hip_priv->intr_from_host_dpd, SCSC_MIFINTR_TARGET_WLAN);
	slsi_wlan_dpd_mmap_set_buffer(NULL, NULL, 0);
	slsi_wlan_dpd_mmap_user_space_event(SLSI_WLAN_DPD_DRV_MSG_ID_WLAN_OFF);
#endif

	slsi_wake_lock_destroy(&hip->hip_priv->wake_lock_tx);
	slsi_wake_lock_destroy(&hip->hip_priv->wake_lock_ctrl);
	slsi_wake_lock_destroy(&hip->hip_priv->wake_lock_data);

	/* If we get to that point with rx_lock/tx_lock claimed, trigger BUG() */
	WLBT_WARN_ON(atomic_read(&hip->hip_priv->in_tx));
	WLBT_WARN_ON(atomic_read(&hip->hip_priv->in_rx));

	kfree(hip->hip_priv);
	hip->hip_priv = NULL;

	/* remove the pools */
	mbulk_pool_remove(MBULK_POOL_ID_DATA);
	mbulk_pool_remove(MBULK_POOL_ID_CTRL);
}
