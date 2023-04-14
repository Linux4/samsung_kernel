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

#include "hip5.h"
#include "hip4_sampler.h"
#include "mbulk.h"
#include "dev.h"
#include "load_manager.h"
#include "debug.h"
#include <scsc/scsc_warn.h>

#ifdef CONFIG_SCSC_WLAN_TX_ZERO_COPY
struct hip5_tx_dma_desc {
	u16           len;
	u16           offset;
	dma_addr_t    skb_dma_addr;
};
#endif

#if defined(CONFIG_SCSC_PCIE_PAEAN_X86) || defined(CONFIG_SOC_S5E9925)
static bool hip5_scoreboard_in_ramrp = false;
#else
static bool hip5_scoreboard_in_ramrp = false;
#endif
module_param(hip5_scoreboard_in_ramrp, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(hip5_scoreboard_in_ramrp, "scoreboard location (default: Y in RAMRP, N: in DRAM)");

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
		smp_wmb();
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
	if (hip5_scoreboard_in_ramrp) {
		value = readl(hip->hip_priv->scbrd_ramrp_base + q_idx_layout[q][r_w]);
	} else {
		value = *((u16 *)(hip->hip_priv->scbrd_base + q_idx_layout[q][r_w]));

		/* Memory barrier when reading shared mailbox/memory */
		smp_rmb();
	}
	read_unlock_bh(&hip_priv->rw_scoreboard);
	return value;
}

void slsi_hip_from_host_intr_set(struct scsc_service *service, struct slsi_hip *hip)
{
	scsc_service_mifintrbit_bit_set(service, hip->hip_priv->intr_from_host_ctrl, SCSC_MIFINTR_TARGET_WLAN);
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

	if (ctrl_packet) {
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

static struct sk_buff *hip5_bd_to_skb(struct slsi_dev *sdev, struct scsc_service *service, struct hip_priv *hip_priv, struct hip5_hip_entry *hip_entry, scsc_mifram_ref *to_free,
											u16 *idx_r, u16 *idx_w)
{
	struct slsi_hip         *hip;
	struct hip5_hip_control *ctrl;
	struct slsi_skb_cb      *cb;
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

			while (num_bd) {
				mem = scsc_mx_service_mif_addr_to_ptr(service, hip_entry->bd[i].buf_addr);

				if (free > MBULK_MAX_CHAIN) {
					SLSI_ERR_NODEV("chain length exceeds MAX: trigger firmware moredump\n");
					queue_work(sdev->device_wq, &sdev->trigger_wlan_fail_work);
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

/* Add signal reference (offset in shared memory) in the selected queue */
/* This function should be called in atomic context. Callers should supply proper locking mechanism */
static int hip5_q_add_bd_entry(struct slsi_hip *hip, enum hip5_hip_q_conf conf, scsc_mifram_ref phy_m, struct scsc_service *service, void *m_ptr, void *sig_ptr)
{
	struct hip5_hip_control *ctrl = hip->hip_control;
	struct hip_priv         *hip_priv = hip->hip_priv;
	u16                      idx_w;
	u16                      idx_r;
#ifdef CONFIG_SCSC_WLAN_TX_ZERO_COPY
	struct hip5_tx_dma_desc *tx_desc = m_ptr;
#else
	struct mbulk            *m = m_ptr;
#endif
	struct hip5_hip_entry *hip5_q_entry = NULL;

	/* Read the current q write pointer */
	idx_w = hip5_read_index(hip, conf, widx);
	/* Read the current q read pointer */
	idx_r = hip5_read_index(hip, conf, ridx);
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

#ifdef CONFIG_SCSC_WLAN_TX_ZERO_COPY
	hip5_q_entry->data_len = tx_desc->len;

	/* bulk descriptor	*/
	hip5_q_entry->bd[0].buf_addr = ((tx_desc->skb_dma_addr >> 8));
	hip5_q_entry->bd[0].buf_sz = HIP5_DAT_MBULK_SIZE;
	hip5_q_entry->bd[0].extra = 0;
	hip5_q_entry->bd[0].flag = 0;
	hip5_q_entry->bd[0].extra = (1 << 1) | (1 << 3); /* addr_format: 1, Owned by Host: 1 */
	hip5_q_entry->bd[0].len = tx_desc->len;
	hip5_q_entry->bd[0].offset = tx_desc->offset;
#else
	hip5_q_entry->data_len = m->len;

	/* bulk descriptor	*/
	hip5_q_entry->bd[0].buf_addr = m->next_offset;
	hip5_q_entry->bd[0].buf_sz = HIP5_DAT_MBULK_SIZE;
	hip5_q_entry->bd[0].extra = 0;
	hip5_q_entry->bd[0].flag = m->flag;
	hip5_q_entry->bd[0].len = m->len;
	hip5_q_entry->bd[0].offset = m->head + 20;
#endif

	/* FAPI signal */
	/* the size of MA-unitdata.request signal is 34 bytes.
	 * And the signal starts at offset 32.
	 * So the whole signal can't fit into 64 bytes of HIP entry.
	 * The fix for now is to not write the last 4 bytes ie.
	 * not write the "spare_3" field which is not used now.
	 */
	memcpy(hip5_q_entry->fapi_sig + 4, sig_ptr, fapi_sig_size(ma_unitdata_req) - 4);
	/* Memory barrier before updating shared mailbox */
	smp_wmb();
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

	SCSC_HIP4_SAMPLER_Q(hip_priv->minor, conf, widx, idx_w, 1);
	SCSC_HIP4_SAMPLER_Q(hip_priv->minor, conf, ridx, idx_r, 1);

	/* Queue is full */
	if (idx_r == ((idx_w + 1) & (MAX_NUM - 1))) {
		SLSI_INFO_NODEV("q[%] is full\n", conf);
		return -ENOSPC;
	}

	/* Update array */
	ctrl->q[conf].array[idx_w] = phy_m;
	/* Memory barrier before updating shared mailbox */
	smp_wmb();
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

	spin_lock_bh(&hip_priv->rx_lock);
	service = sdev->service;
	SCSC_HIP4_SAMPLER_INT_BH(hip->hip_priv->minor, 1);
	bh_init_ctrl = ktime_get();

	idx_r = hip5_read_index(hip, HIP5_MIF_Q_TH_CTRL, ridx);
	idx_w = hip5_read_index(hip, HIP5_MIF_Q_TH_CTRL, widx);

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
		smp_rmb();
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
				SLSI_WARN_NODEV("Ctrl: Not enough space in FB, retry: %d/%d\n", retry, FB_NO_SPC_NUM_RET);
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

	if (!atomic_read(&hip->hip_priv->closing)) {
		scsc_service_mifintrbit_bit_unmask(sdev->service, hip->hip_priv->intr_to_host_ctrl);
	}
	SCSC_HIP4_SAMPLER_INT_OUT_BH(hip->hip_priv->minor, 1);

	if (slsi_wake_lock_active(&hip->hip_priv->wake_lock_ctrl)) {
		slsi_wake_unlock(&hip->hip_priv->wake_lock_ctrl);
	}

	bh_end_ctrl = ktime_get();
	spin_unlock_bh(&hip_priv->rx_lock);
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
#ifdef CONFIG_SCSC_WLAN_TX_ZERO_COPY
	u16                     i;
	u32 					skb_dma_addr;
#else
	struct mbulk            *m;
	scsc_mifram_ref			ref;
	void                    *mem;
#endif

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

	spin_lock_bh(&hip_priv->rx_lock);
	service = sdev->service;
	SCSC_HIP4_SAMPLER_INT_BH(hip->hip_priv->minor, 2);
	bh_init_fb = ktime_get();

	idx_r = hip5_read_index(hip, HIP5_MIF_Q_FH_RFBC, ridx);
	idx_w = hip5_read_index(hip, HIP5_MIF_Q_FH_RFBC, widx);

	if (idx_r != idx_w) {
		SCSC_HIP4_SAMPLER_Q(hip_priv->minor, HIP5_MIF_Q_FH_RFBC, ridx, idx_r, 1);
		SCSC_HIP4_SAMPLER_Q(hip_priv->minor, HIP5_MIF_Q_FH_RFBC, widx, idx_w, 1);
	}
	while (idx_r != idx_w) {
#ifdef CONFIG_SCSC_WLAN_TX_ZERO_COPY
		skb_dma_addr = ctrl->q[HIP5_MIF_Q_FH_RFBC].array[idx_r];

		if (!skb_dma_addr) {
			SLSI_ERR_NODEV("FB: skb is NULL\n");
			goto consume_fb_mbulk;
		}

		/* find the entry in SKB look-up table */
		for (i = 0; i < MAX_NUM; i++)
			if (hip_priv->tx_skb_table[i].in_use &&
				hip_priv->tx_skb_table[i].skb_dma_addr == skb_dma_addr)
				break;

		if (i < MAX_NUM) {
			/* get to index from SKB address */
			colour = hip_priv->tx_skb_table[i].colour;
			slsi_hip_tx_done(sdev, colour, (idx_r + 1 & (MAX_NUM - 1)) != idx_w);
			consume_skb(hip_priv->tx_skb_table[i].skb);
			hip_priv->tx_skb_table[i].in_use = false;
		}
#else
		ref = ctrl->q[HIP5_MIF_Q_FH_RFBC].array[idx_r];
#ifdef CONFIG_SCSC_WLAN_HIP5_PROFILING
		SCSC_HIP4_SAMPLER_QREF(hip_priv->minor, ref, HIP5_MIF_Q_FH_RFBC);
#endif
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
		mbulk_free_virt_host(m);
#endif
consume_fb_mbulk:
		/* Increase index */
		idx_r++;
		idx_r &= (MAX_NUM - 1);
		hip5_update_index(hip, HIP5_MIF_Q_FH_RFBC, ridx, idx_r);
	}

	if (!atomic_read(&hip->hip_priv->closing)) {
		scsc_service_mifintrbit_bit_unmask(sdev->service, hip->hip_priv->intr_to_host_ctrl_fb);
	}
	SCSC_HIP4_SAMPLER_INT_OUT_BH(hip->hip_priv->minor, 2);

	if (slsi_wake_lock_active(&hip->hip_priv->wake_lock_tx)) {
		slsi_wake_unlock(&hip->hip_priv->wake_lock_tx);
	}

	bh_end_fb = ktime_get();
	spin_unlock_bh(&hip_priv->rx_lock);
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
	u8                      todo = 0;
	int work_done = 0;

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
		napi_complete(napi);
		return 0;
	}

	spin_lock_bh(&hip_priv->rx_lock);
	SCSC_HIP4_SAMPLER_INT_BH(hip->hip_priv->minor, 0);
	if (ktime_compare(bh_init_data, bh_end_data) <= 0)
		bh_init_data = ktime_get();

	idx_r = hip5_read_index(hip, HIP5_MIF_Q_TH_DAT0, ridx);
	idx_w = hip5_read_index(hip, HIP5_MIF_Q_TH_DAT0, widx);

	service = sdev->service;

	if (idx_r == idx_w) {
		SLSI_DBG3(sdev, SLSI_RX, "nothing to do, NAPI Complete\n");

		bh_end_data = ktime_get();
		napi_complete(napi);
		if (!atomic_read(&hip->hip_priv->closing)) {
			/* Nothing more to drain, unmask interrupt */
			scsc_service_mifintrbit_bit_unmask(sdev->service, hip->hip_priv->intr_to_host_data1);
		}

		if (slsi_wake_lock_active(&hip->hip_priv->wake_lock_data)) {
			slsi_wake_unlock(&hip->hip_priv->wake_lock_data);
		}
		goto end;
	}

	if (idx_r != idx_w) {
		SCSC_HIP4_SAMPLER_Q(hip_priv->minor, HIP5_MIF_Q_TH_DAT0, ridx, idx_r, 1);
		SCSC_HIP4_SAMPLER_Q(hip_priv->minor, HIP5_MIF_Q_TH_DAT0, widx, idx_w, 1);
	}

	todo = ((idx_w - idx_r) & 0x1ff);
	SLSI_DBG2(sdev, SLSI_RX, "todo:%hhu\n", todo);

	while (idx_r != idx_w) {
		struct sk_buff *skb;
		/* TODO: currently the max number to be freed is 2. In future
		 * implementations (i.e. AMPDU) this number may be bigger
		 */
		/* list of mbulks to be freed */
		scsc_mifram_ref to_free[MBULK_MAX_CHAIN + 1] = { 0 };
		u8              i = 0;

		/* Catch-up with idx_w */
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
consume_dat_mbulk:
		/* Increase index */
		idx_r++;
		idx_r &= (MAX_NUM - 1);

		while ((i <= MBULK_MAX_CHAIN) && (ref = to_free[i++])) {
			/* Set the number of retries */
			retry = FB_NO_SPC_NUM_RET;
			while (hip5_q_add_signal(hip, HIP5_MIF_Q_TH_RFBD0, ref, service) && (!atomic_read(&hip->hip_priv->closing)) && (retry > 0)) {
				SLSI_WARN_NODEV("Dat: Not enough space in FB, retry: %d/%d\n", retry, FB_NO_SPC_NUM_RET);
				udelay(FB_NO_SPC_DELAY_US);
				retry--;

				if (retry == 0)
					SLSI_ERR_NODEV("Dat: FB has not been freed for %d us\n", FB_NO_SPC_NUM_RET * FB_NO_SPC_DELAY_US);
#ifdef CONFIG_SCSC_WLAN_HIP5_PROFILING
				SCSC_HIP4_SAMPLER_QFULL(hip_priv->minor, HIP5_MIF_Q_TH_RFBD1);
#endif
			}
		}

		work_done++;
		if (budget == work_done) {
			/* We have consumed all the bugdet */
			break;
		}
	}

	hip5_update_index(hip, HIP5_MIF_Q_TH_DAT0, ridx, idx_r);

	if (work_done < budget) {
		SLSI_DBG3(sdev, SLSI_RX, "NAPI complete (work_done:%d)\n", work_done);
		bh_end_data = ktime_get();
		napi_complete(napi);
		if (!atomic_read(&hip->hip_priv->closing)) {
			/* Nothing more to drain, unmask interrupt */
			scsc_service_mifintrbit_bit_unmask(sdev->service, hip->hip_priv->intr_to_host_data1);
		}
		if (slsi_wake_lock_active(&hip->hip_priv->wake_lock_data)) {
			slsi_wake_unlock(&hip->hip_priv->wake_lock_data);
		}
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
	struct slsi_dev     *sdev = container_of(hip, struct slsi_dev, hip);
	unsigned long flags;

	if (!hip || !sdev || !sdev->service || !hip->hip_priv) {
		SLSI_ERR_NODEV("NULLLLLLL\n");
		return;
	}

	if (atomic_read(&hip->hip_priv->closing)) {
		SLSI_ERR_NODEV("interrupt received while hip is closed: %d\n", irq);
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

int slsi_hip_init(struct slsi_hip *hip)
{
	void                    *hip_ptr;
	struct hip5_hip_control *hip_control;
	struct scsc_service     *service;
	struct slsi_dev         *sdev = container_of(hip, struct slsi_dev, hip);
	scsc_mifram_ref         ref, ref_scoreboard;
	uintptr_t               scrb_ref = 0;
	int                     i;
	int                     ret;
	u32                     total_mib_len;
	u32                     mib_file_offset;

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

	SLSI_INFO_NODEV("HIP5_WLAN_CONFIG_SIZE (%d)\n", HIP5_WLAN_CONFIG_SIZE);
	SLSI_INFO_NODEV("HIP5_WLAN_MIB_SIZE (%d)\n", HIP5_WLAN_MIB_SIZE);
	SLSI_INFO_NODEV("HIP5_WLAN_TX_DAT_SIZE (%d)\n", HIP5_WLAN_TX_DAT_SIZE);
	SLSI_INFO_NODEV("HIP5_WLAN_TX_CTL_SIZE (%d)\n", HIP5_WLAN_TX_CTL_SIZE);
	SLSI_INFO_NODEV("HIP5_WLAN_RX_SIZE (%d)\n", HIP5_WLAN_RX_SIZE);
	SLSI_INFO_NODEV("HIP5_WLAN_TOTAL_MEM (%d)\n", HIP5_WLAN_TOTAL_MEM);
	SLSI_INFO_NODEV("HIP5_DAT_SLOTS (%d)\n", HIP5_DAT_SLOTS);
	SLSI_INFO_NODEV("HIP5_CTL_SLOTS (%d)\n", HIP5_CTL_SLOTS);

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
	/* Configure mbulk allocator - Data QUEUES */
	ret = mbulk_pool_add(MBULK_POOL_ID_DATA, hip_ptr + HIP5_WLAN_TX_DAT_OFFSET,
			     hip_ptr + HIP5_WLAN_TX_DAT_OFFSET + HIP5_WLAN_TX_DAT_SIZE,
			     (HIP5_WLAN_TX_DAT_SIZE / HIP5_DAT_SLOTS) - sizeof(struct mbulk), 5,
			     hip->hip_priv->minor);
	if (ret)
		return ret;

	/* Configure mbulk allocator - Control QUEUES */
	ret = mbulk_pool_add(MBULK_POOL_ID_CTRL, hip_ptr + HIP5_WLAN_TX_CTL_OFFSET,
			     hip_ptr + HIP5_WLAN_TX_CTL_OFFSET + HIP5_WLAN_TX_CTL_SIZE,
			     (HIP5_WLAN_TX_CTL_SIZE / HIP5_CTL_SLOTS) - sizeof(struct mbulk), 0,
			     hip->hip_priv->minor);
	if (ret)
		return ret;
#else
	/* Configure mbulk allocator - Data QUEUES */
	ret = mbulk_pool_add(MBULK_POOL_ID_DATA, hip_ptr + HIP5_WLAN_TX_DAT_OFFSET,
			     hip_ptr + HIP5_WLAN_TX_DAT_OFFSET + HIP5_WLAN_TX_DAT_SIZE,
			     (HIP5_WLAN_TX_DAT_SIZE / HIP5_DAT_SLOTS) - sizeof(struct mbulk), 5);
	if (ret)
		return ret;

	/* Configure mbulk allocator - Control QUEUES */
	ret = mbulk_pool_add(MBULK_POOL_ID_CTRL, hip_ptr + HIP5_WLAN_TX_CTL_OFFSET,
			     hip_ptr + HIP5_WLAN_TX_CTL_OFFSET + HIP5_WLAN_TX_CTL_SIZE,
			    (HIP5_WLAN_TX_CTL_SIZE / HIP5_CTL_SLOTS) - sizeof(struct mbulk), 0);
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
	hip->hip_priv->intr_to_host_ctrl =
		scsc_service_mifintrbit_register_tohost(service, hip5_irq_handler_ctrl, hip, SCSC_MIFINTR_TARGET_WLAN);
	SLSI_INFO_NODEV("HIP5: intr_to_host_ctrl %d\n", hip->hip_priv->intr_to_host_ctrl);

	/* Mask the interrupt to prevent intr been kicked during start */
	scsc_service_mifintrbit_bit_mask(service, hip->hip_priv->intr_to_host_ctrl);

	hip->hip_priv->intr_to_host_ctrl_fb =
		scsc_service_mifintrbit_register_tohost(service, hip5_irq_handler_fb, hip, SCSC_MIFINTR_TARGET_WLAN);
	SLSI_INFO_NODEV("HIP5: intr_to_host_ctrl_fb %d\n", hip->hip_priv->intr_to_host_ctrl_fb);

	/* Mask the interrupt to prevent intr been kicked during start */
	scsc_service_mifintrbit_bit_mask(service, hip->hip_priv->intr_to_host_ctrl_fb);

	hip->hip_priv->intr_to_host_data1 =
		scsc_service_mifintrbit_register_tohost(service, hip5_irq_handler_dat, hip, SCSC_MIFINTR_TARGET_WLAN);

	/* Mask the interrupt to prevent intr been kicked during start */
	scsc_service_mifintrbit_bit_mask(service, hip->hip_priv->intr_to_host_data1);

	hip->hip_priv->intr_to_host_data2 =
		scsc_service_mifintrbit_register_tohost(service, hip5_irq_handler_stub, hip, SCSC_MIFINTR_TARGET_WLAN);

	/* Mask the interrupt to prevent intr been kicked during start */
	scsc_service_mifintrbit_bit_mask(service, hip->hip_priv->intr_to_host_data2);

	/* FROMHOST Handler allocator */
	hip->hip_priv->intr_from_host_ctrl =
		scsc_service_mifintrbit_alloc_fromhost(service, SCSC_MIFINTR_TARGET_WLAN);
	SLSI_INFO_NODEV("HIP5: intr_from_host_ctrl %d\n", hip->hip_priv->intr_from_host_ctrl);

	hip->hip_priv->intr_from_host_data =
		scsc_service_mifintrbit_alloc_fromhost(service, SCSC_MIFINTR_TARGET_WLAN);

	SLSI_INFO_NODEV("HIP5: intr_from_host_data %d\n", hip->hip_priv->intr_from_host_data);
	/* Get hip_control pointer on shared memory  */
	hip_control = (struct hip5_hip_control *)(hip_ptr +
		      HIP5_WLAN_CONFIG_OFFSET);

#if defined(CONFIG_SCSC_PCIE_PAEAN_X86) || defined(CONFIG_SOC_S5E9925)
	/* Initialize scoreboard */
	if (hip5_scoreboard_in_ramrp) {
		hip->hip_priv->scbrd_ramrp_base = scsc_mx_service_get_scoreboard(sdev->service);
		if (scsc_mx_service_get_scoreboard_ref(sdev->service, hip->hip_priv->scbrd_ramrp_base, &scrb_ref))
			return -EFAULT;

		/* let firmware know scoreboard is in RAMRP, by setting the LSB of address */
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

	if (total_mib_len > HIP5_WLAN_MIB_SIZE) {
		SLSI_ERR_NODEV("MIB size (%d), is bigger than the MIB AREA (%d). Aborting memcpy\n", total_mib_len, HIP5_WLAN_MIB_SIZE);
		hip_control->config_v4.mib_loc      = 0;
		hip_control->config_v4.mib_sz       = 0;
		hip_control->config_v5.mib_loc      = 0;
		hip_control->config_v5.mib_sz       = 0;
		total_mib_len = 0;
	} else if (total_mib_len) {
		SLSI_INFO_NODEV("Loading MIB into shared memory, size (%d)\n", total_mib_len);
		/* Load each MIB file into shared DRAM region */
		for (i = 0, mib_file_offset = 0;
		     i < SLSI_WLAN_MAX_MIB_FILE && sdev->mib[i].mib_file_name;
		     i++) {
			SLSI_INFO_NODEV("Loading MIB %d into shared memory, offset (%d), size (%d), total (%d)\n", i, mib_file_offset, sdev->mib[i].mib_len, total_mib_len);
			if (sdev->mib[i].mib_len) {
				memcpy((u8 *)hip_ptr + HIP5_WLAN_MIB_OFFSET + mib_file_offset, sdev->mib[i].mib_data, sdev->mib[i].mib_len);
				mib_file_offset += sdev->mib[i].mib_len;
			}
		}
		hip_control->config_v4.mib_loc      = hip->hip_ref + HIP5_WLAN_MIB_OFFSET;
		hip_control->config_v4.mib_sz       = total_mib_len;
		hip_control->config_v5.mib_loc      = hip->hip_ref + HIP5_WLAN_MIB_OFFSET;
		hip_control->config_v5.mib_sz       = total_mib_len;
	} else {
		hip_control->config_v4.mib_loc      = 0;
		hip_control->config_v4.mib_sz       = 0;
		hip_control->config_v5.mib_loc      = 0;
		hip_control->config_v5.mib_sz       = 0;
	}

	/* Initialize hip_control table for version 4 */
	/***** VERSION 4 *******/
	hip_control->config_v4.magic_number = 0xcaba0401;
	hip_control->config_v4.hip_config_ver = 5;
	hip_control->config_v4.config_len = sizeof(struct hip5_hip_config_version_1);
	hip_control->config_v4.host_cache_line = 64;
	hip_control->config_v4.host_buf_loc = hip->hip_ref + HIP5_WLAN_TX_OFFSET;
	hip_control->config_v4.host_buf_sz  = HIP5_WLAN_TX_SIZE;
	hip_control->config_v4.fw_buf_loc   = hip->hip_ref + HIP5_WLAN_RX_OFFSET;
	hip_control->config_v4.fw_buf_sz    = HIP5_WLAN_RX_SIZE;
	hip_control->config_v4.log_config_loc = 0;

	if (hip5_scoreboard_in_ramrp) {
		hip_control->config_v4.scbrd_loc = (u32)scrb_ref;
	} else {
		hip_control->config_v4.scbrd_loc = (u32)ref_scoreboard;
	}

	hip_control->config_v4.q_num = 24;

	/* Initialize q relative positions */
	for (i = 0; i < MIF_HIP_CFG_Q_NUM; i++) {
		hip_control->config_v4.q_cfg[i].q_len = MAX_NUM;
		hip_control->config_v4.q_cfg[i].q_idx_sz = 2;
		if (i > 13) {
			hip_control->config_v4.q_cfg[i].q_entry_sz = 64;
		} else {
			hip_control->config_v4.q_cfg[i].q_entry_sz = 4;
		}
		if (i < 14) {
			if (scsc_mx_service_mif_ptr_to_addr(service, &hip_control->q[i].array, &ref)) {
				return -EFAULT;
			}
		}
		hip_control->config_v4.q_cfg[i].q_loc = (u32)ref;
	}

	if (scsc_mx_service_mif_ptr_to_addr(service, &hip_control->q_tlv[0].array, &ref)) {
		return -EFAULT;
	}
	hip_control->config_v4.q_cfg[HIP5_MIF_Q_FH_DAT0].q_loc = (u32)ref;

	if (scsc_mx_service_mif_ptr_to_addr(service, &hip_control->q_tlv[1].array, &ref)) {
		return -EFAULT;
	}
	hip_control->config_v4.q_cfg[HIP5_MIF_Q_FH_DAT1].q_loc = (u32)ref;

	if (scsc_mx_service_mif_ptr_to_addr(service, &hip_control->q_tlv[2].array, &ref)) {
			return -EFAULT;
	}
	hip_control->config_v4.q_cfg[HIP5_MIF_Q_TH_DAT0].q_loc = (u32)ref;
	hip_control->config_v4.q_cfg[HIP5_MIF_Q_TH_DAT0].q_entry_sz = 64;

	/* initialize interrupts */
	hip_control->config_v4.q_cfg[HIP5_MIF_Q_FH_CTRL].int_n = hip->hip_priv->intr_from_host_ctrl;
	hip_control->config_v4.q_cfg[HIP5_MIF_Q_TH_RFBC].int_n = hip->hip_priv->intr_from_host_ctrl;
	hip_control->config_v4.q_cfg[HIP5_MIF_Q_TH_RFBD0].int_n = hip->hip_priv->intr_from_host_ctrl;
	hip_control->config_v4.q_cfg[HIP5_MIF_Q_TH_CTRL].int_n = hip->hip_priv->intr_to_host_ctrl;
	hip_control->config_v4.q_cfg[HIP5_MIF_Q_FH_RFBC].int_n = hip->hip_priv->intr_to_host_ctrl_fb;

	for (i = HIP5_MIF_Q_FH_DAT0; i < MIF_HIP_CFG_Q_NUM; i++) {
		hip_control->config_v4.q_cfg[i].int_n = hip->hip_priv->intr_from_host_data;
	}

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
	hip_control->config_v5.host_buf_sz  = HIP5_WLAN_TX_SIZE;
	hip_control->config_v5.fw_buf_loc   = hip->hip_ref + HIP5_WLAN_RX_OFFSET;
	hip_control->config_v5.fw_buf_sz    = HIP5_WLAN_RX_SIZE;
	hip_control->config_v5.log_config_loc = 0;

	hip_control->config_v5.scbrd_loc = (u32)ref_scoreboard;
	hip_control->config_v5.q_num = 24;

	/* Initialize q relative positions */
	for (i = 0; i < MIF_HIP_CFG_Q_NUM; i++) {
		hip_control->config_v5.q_cfg[i].q_len = 256;
		hip_control->config_v5.q_cfg[i].q_idx_sz = 2;
		hip_control->config_v5.q_cfg[i].q_entry_sz = 4;

		if (scsc_mx_service_mif_ptr_to_addr(service, &hip_control->q[0].array, &ref))
			return -EFAULT;
		hip_control->config_v5.q_cfg[i].q_loc = (u32)ref;
	}
	/***** END VERSION 5 *******/

	/* Initialzie hip_init configuration */
	hip_control->init.magic_number = 0xcaaa0400;
	if (scsc_mx_service_mif_ptr_to_addr(service, &hip_control->config_v4, &ref))
		return -EFAULT;
	hip_control->init.version_a_ref = ref;

	if (scsc_mx_service_mif_ptr_to_addr(service, &hip_control->config_v5, &ref))
		return -EFAULT;
	hip_control->init.version_b_ref = ref;
	/* End hip_init configuration */

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

#ifdef CONFIG_SCSC_WLAN_TX_ZERO_COPY
static int hip5_transmit_frame_zero_copy(struct slsi_hip *hip, struct sk_buff *skb, u8 vif_index, u8 peer_index, u8 priority)
{
	struct hip_priv           *hip_priv;
	struct scsc_service       *service;
	mbulk_colour              colour = 0;
	struct slsi_dev           *sdev = container_of(hip, struct slsi_dev, hip);
	struct sk_buff            *skb_fapi;
	struct sk_buff            *skb_udi;
	struct hip5_tx_dma_desc   tx_desc;
	int                       ret = 0;
	u16                       i = 0;

	hip_priv = hip->hip_priv;
	service = sdev->service;

	spin_lock_bh(&hip_priv->tx_lock);
	atomic_set(&hip_priv->in_tx, 1);

	/* clone the skb for logging */
	skb_udi = skb_clone(skb, GFP_ATOMIC);

	if (fapi_is_ma(skb))
#ifndef CONFIG_SCSC_WLAN_TX_API
		SLSI_MBULK_COLOUR_SET(colour, vif_index, peer_index, priority);
#else
		SLSI_MBULK_COLOUR_SET(colour, vif_index, priority);
#endif

	skb_fapi = skb_clone(skb, GFP_ATOMIC);
	if (!skb_fapi) {
		SLSI_WARN(sdev, "fail to clone SKB for skb_fapi\n");
		ret = -ENOMEM;
		goto error;
	}
	skb_trim(skb_fapi, fapi_sig_size(ma_unitdata_req));
	skb_pull(skb, fapi_sig_size(ma_unitdata_req));

	tx_desc.len = skb->len;
	tx_desc.offset = skb_headroom(skb);
	tx_desc.skb_dma_addr = dma_map_single(sdev->dev, skb->head, skb_headroom(skb) + skb_headlen(skb), DMA_TO_DEVICE);

	if (dma_mapping_error(sdev->dev, tx_desc.skb_dma_addr)) {
		consume_skb(skb_fapi);
		ret = -EFAULT;
		SLSI_ERR_NODEV("DMA mapping error\n");
		goto error;
	}

	if (hip5_q_add_bd_entry(hip, slsi_hip_vif_to_q_mapping(vif_index), 0, service, &tx_desc, skb_fapi->data)) {
		memcpy(skb_push(skb, fapi_sig_size(ma_unitdata_req)), skb_fapi->data, fapi_sig_size(ma_unitdata_req));
		consume_skb(skb_fapi);
		ret = -ENOSPC;
		SLSI_ERR_NODEV("No space\n");
		goto error;
	}
	consume_skb(skb_fapi);

	/* find an index in look up table */
	for (i = 0; i < MAX_NUM; i++)
		if (!hip_priv->tx_skb_table[i].in_use)
			break;

	/* store the Tx SKB */
	hip_priv->tx_skb_table[i].skb_dma_addr = (tx_desc.skb_dma_addr >> 8);
	hip_priv->tx_skb_table[i].colour = colour;
	hip_priv->tx_skb_table[i].skb = skb;
	hip_priv->tx_skb_table[i].in_use = true;

	SCSC_BIN_TAG_DEBUG(BIN_WIFI_DATA_TX, skb_udi->data, skb_headlen(skb));
	/* slsi_log_clients_log_signal_fast: skb is copied to all the log clients */
	slsi_log_clients_log_signal_fast(sdev, &sdev->log_clients, skb_udi, SLSI_LOG_DIRECTION_FROM_HOST);

	consume_skb(skb_udi);
	atomic_set(&hip->hip_priv->in_tx, 0);
	spin_unlock_bh(&hip->hip_priv->tx_lock);
	return 0;
error:
	consume_skb(skb_udi);
	atomic_set(&hip->hip_priv->in_tx, 0);
	spin_unlock_bh(&hip->hip_priv->tx_lock);
	return ret;

}
#endif
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

#ifdef CONFIG_SCSC_WLAN_TX_ZERO_COPY
	if (!ctrl_packet)
		return hip5_transmit_frame_zero_copy(hip, skb, vif_index, peer_index, priority);
#endif
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
		m->next_offset = offset;
		skb_fapi = skb_clone(skb, GFP_ATOMIC);
		if (!skb_fapi) {
			SLSI_WARN(sdev, "fail to allocate memory for FAPI SKB\n");
			ret = -ENOMEM;
			goto error;
		}
		skb_trim(skb_fapi, fapi_sig_size(ma_unitdata_req));
		skb_pull(skb, fapi_sig_size(ma_unitdata_req));

		if (hip5_q_add_bd_entry(hip, slsi_hip_vif_to_q_mapping(vif_index), offset, service, m, skb_fapi->data)) {
			mbulk_free_virt_host(m);
			memcpy(skb_push(skb, fapi_sig_size(ma_unitdata_req)), skb_fapi->data, fapi_sig_size(ma_unitdata_req));
			ret = -ENOSPC;
			SLSI_ERR_NODEV("No space\n");
			goto error;
		}
		consume_skb(skb_fapi);
		m->next_offset = 0;
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

/* HIP has been initialize, setup with values
 * provided by FW
 */
int slsi_hip_setup(struct slsi_hip *hip)
{
	struct slsi_dev     *sdev = container_of(hip, struct slsi_dev, hip);
	struct scsc_service *service;
	u32 conf_hip4_ver = 0;

	if (!sdev || !sdev->service)
		return -EIO;

	if (atomic_read(&sdev->hip.hip_state) != SLSI_HIP_STATE_STARTED)
		return -EIO;

	service = sdev->service;

	/* Get the Version reported by the FW */
	conf_hip4_ver = scsc_wifi_get_hip_config_version(&hip->hip_control->init);
	/* Check if the version is supported. And get the index */
	/* This is hardcoded and may change in future versions */
	if (conf_hip4_ver != 4 && conf_hip4_ver != 5) {
		SLSI_ERR_NODEV("FW HIP config version %d not supported\n", conf_hip4_ver);
		return -EIO;
	}

	if (conf_hip4_ver == 4) {
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
	}
	/* Unmask interrupts - now host should handle them */
	atomic_set(&sdev->debug_inds, 0);
	atomic_set(&hip->hip_priv->closing, 0);

	scsc_service_mifintrbit_bit_unmask(service, hip->hip_priv->intr_to_host_ctrl);
	scsc_service_mifintrbit_bit_unmask(service, hip->hip_priv->intr_to_host_ctrl_fb);
	scsc_service_mifintrbit_bit_unmask(service, hip->hip_priv->intr_to_host_data1);
	scsc_service_mifintrbit_bit_unmask(service, hip->hip_priv->intr_to_host_data2);
	return 0;
}

/* On suspend hip needs to ensure that TH interrupts *are* unmasked */
void slsi_hip_suspend(struct slsi_hip *hip)
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

	memset(&sdev->suspend_tm, 0, sizeof(struct rtc_time));
	if (slsi_hip_wlan_get_rtc_time(&sdev->suspend_tm))
		SLSI_ERR_NODEV("Error reading rtc\n");

	service = sdev->service;

	slsi_log_client_msg(sdev, UDI_DRV_SUSPEND_IND, 0, NULL);
	SCSC_HIP4_SAMPLER_SUSPEND(hip->hip_priv->minor);

	atomic_set(&hip->hip_priv->in_suspend, 1);

	scsc_service_mifintrbit_bit_unmask(service, hip->hip_priv->intr_to_host_ctrl);
	scsc_service_mifintrbit_bit_unmask(service, hip->hip_priv->intr_to_host_data1);
	scsc_service_mifintrbit_bit_unmask(service, hip->hip_priv->intr_to_host_data2);
}

void slsi_hip_resume(struct slsi_hip *hip)
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

	service = sdev->service;

	scsc_service_mifintrbit_bit_unmask(service, hip->hip_priv->intr_to_host_ctrl);
	scsc_service_mifintrbit_bit_unmask(service, hip->hip_priv->intr_to_host_data1);
	scsc_service_mifintrbit_bit_unmask(service, hip->hip_priv->intr_to_host_data2);

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

	service = sdev->service;

	closing = ktime_get();
	atomic_set(&hip->hip_priv->closing, 1);

	hip5_dump_dbg(hip, NULL, NULL, service);

	scsc_service_mifintrbit_bit_mask(service, hip->hip_priv->intr_to_host_ctrl);
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
	scsc_service_mifintrbit_bit_mask(service, hip->hip_priv->intr_to_host_data1);
	scsc_service_mifintrbit_bit_mask(service, hip->hip_priv->intr_to_host_data2);

	slsi_lbm_unregister_bh(hip->hip_priv->bh_dat);
	slsi_lbm_unregister_bh(hip->hip_priv->bh_ctl);
	slsi_lbm_unregister_bh(hip->hip_priv->bh_rfb);
	hip->hip_priv->bh_dat = NULL;
	hip->hip_priv->bh_ctl = NULL;
	hip->hip_priv->bh_rfb = NULL;

	scsc_service_mifintrbit_unregister_tohost(service, hip->hip_priv->intr_to_host_ctrl, SCSC_MIFINTR_TARGET_WLAN);
	scsc_service_mifintrbit_unregister_tohost(service, hip->hip_priv->intr_to_host_data1, SCSC_MIFINTR_TARGET_WLAN);
	scsc_service_mifintrbit_unregister_tohost(service, hip->hip_priv->intr_to_host_data2, SCSC_MIFINTR_TARGET_WLAN);

	flush_workqueue(hip->hip_priv->hip_workq);
	destroy_workqueue(hip->hip_priv->hip_workq);

	scsc_service_mifintrbit_free_fromhost(service, hip->hip_priv->intr_from_host_ctrl, SCSC_MIFINTR_TARGET_WLAN);
	scsc_service_mifintrbit_free_fromhost(service, hip->hip_priv->intr_from_host_data, SCSC_MIFINTR_TARGET_WLAN);

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
