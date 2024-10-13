/*
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS DIT(Direct IP Translator) Driver support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/smp.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include "exynos-dit.h"
#include <soc/samsung/hw_forward.h>
#include <linux/mod_devicetable.h>
#include <linux/delay.h>

#include <linux/string.h>
#include <linux/ctype.h>

#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/inet.h>

#include <net/ipv6.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/sch_generic.h>
#include <uapi/linux/ip.h>

#include "exynos-dit-ioctl.h"
#include "exynos-dit-offload.h"

#ifdef DIT_CHECK_PERF
#include <uapi/linux/sched/types.h>
#include <linux/kthread.h>
#include "exynos-dit-data.h"

int debug_print;
unsigned char *test_Rxpacket = udpRx;
unsigned char *test_Txpacket = udpTx;
int test_Rxpkt_sz;
int test_Txpkt_sz;
#endif

/* Test Devices & Addresses */
#define	PERF_PC_IP_ADDR		0xc0a82a75 /* 192.168.42.117 */
#define	PERF_UE_IP_ADDR		0xc0000004 /* 192.0.0.4 */

#define SZ_PAD_PACKET	48
unsigned char padd_packet[SZ_PAD_PACKET+SZ_HW_FWD_IFINFO] = { /* padding for ifindex */
0x00, 0x00, 0x44, 0x49, 0x54, 0x20, 0x32, 0x2E,
0x30, 0x20, 0x70, 0x61, 0x64, 0x64, 0x69, 0x6E,
0x67, 0x20, 0x70, 0x61, 0x63, 0x6B, 0x65, 0x74,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

struct dit_dev_info_t dit_dev = {
	.irq = {
		{"DIT-RxDst0", -1, dit_rx_irq_handler, DIT_DST0, 0},
		{"DIT-RxDst1", -1, dit_rx_irq_handler, DIT_DST1, 0},
		{"DIT-RxDst2", -1, dit_rx_irq_handler, DIT_DST2, 0},
		{"DIT-Tx", -1, dit_tx_irq_handler, DIT_DST0|DIT_DST1|DIT_DST2, 0},
		{"DIT-Err", -1, dit_errbus_irq_handler, -1, 0},
	},
	.handle = {
		{	.num_desc = DIT_MAX_DESC,
			.ring = {
				{ DIT_RX_RING_START_ADDR_1_DST0, DIT_RX_RING_START_ADDR_0_DST0 },
				{ DIT_RX_RING_START_ADDR_1_DST1, DIT_RX_RING_START_ADDR_0_DST1 },
				{ DIT_RX_RING_START_ADDR_1_DST2, DIT_RX_RING_START_ADDR_0_DST2 },
				{ DIT_RX_RING_START_ADDR_1_SRC,  DIT_RX_RING_START_ADDR_0_SRC }
			},
			.natdesc = {
				{ DIT_NAT_RX_DESC_ADDR_1_DST0, DIT_NAT_RX_DESC_ADDR_0_DST0 },
				{ DIT_NAT_RX_DESC_ADDR_1_DST1, DIT_NAT_RX_DESC_ADDR_0_DST1 },
				{ DIT_NAT_RX_DESC_ADDR_1_DST2, DIT_NAT_RX_DESC_ADDR_0_DST2 },
				{ DIT_NAT_RX_DESC_ADDR_1_SRC,  DIT_NAT_RX_DESC_ADDR_0_SRC }
			}
		},
		{	.num_desc = DIT_MAX_DESC,
			.ring = {
				{ DIT_TX_RING_START_ADDR_1_DST0, DIT_TX_RING_START_ADDR_0_DST0 },
				{ DIT_TX_RING_START_ADDR_1_DST1, DIT_TX_RING_START_ADDR_0_DST1 },
				{ DIT_TX_RING_START_ADDR_1_DST2, DIT_TX_RING_START_ADDR_0_DST2 },
				{ DIT_TX_RING_START_ADDR_1_SRC,  DIT_TX_RING_START_ADDR_0_SRC }
			},
			.natdesc = {
				{ DIT_NAT_TX_DESC_ADDR_1_DST0, DIT_NAT_TX_DESC_ADDR_0_DST0 },
				{ DIT_NAT_TX_DESC_ADDR_1_DST1, DIT_NAT_TX_DESC_ADDR_0_DST1 },
				{ DIT_NAT_TX_DESC_ADDR_1_DST2, DIT_NAT_TX_DESC_ADDR_0_DST2 },
				{ DIT_NAT_TX_DESC_ADDR_1_SRC,  DIT_NAT_TX_DESC_ADDR_0_SRC }
			}
		}
	},
	.if2Dst = { /* select Dst for DIR */
		/*         USB     , WLAN    , RMNET   */
		{ /* RX */ DIT_DST2, -1, -1 },
		{ /* TX */ -1, -1, DIT_DST1 }
	},
	.Dst2if = {
		/*         DST0     , DST1    , DST2   */
		{ /* RX */ -1, DIT_IF_WLAN, DIT_IF_USB },
		{ /* TX */ -1, DIT_IF_RMNET, -1 }
	}
};

char dit_dt_item[DIT_MAX_NAME_LEN];

static inline bool circ_empty(unsigned int in, unsigned int out)
{
	return (in == out);
}

static inline unsigned int circ_get_space(unsigned int qsize,
					  unsigned int in,
					  unsigned int out)
{
	return (in < out) ? (out - in - 1) : (qsize + out - in - 1);
}

static inline bool circ_full(unsigned int qsize, unsigned int in,
			     unsigned int out)
{
	return (circ_get_space(qsize, in, out) == 0);
}

static inline unsigned int circ_get_usage(unsigned int qsize,
					  unsigned int in,
					  unsigned int out)
{
	return (in >= out) ? (in - out) : (qsize - out + in);
}

static inline unsigned int circ_new_ptr(unsigned int qsize,
					unsigned int p,
					unsigned int len)
{
	unsigned int np = (p + len);

	while (np >= qsize)
		np -= qsize;
	return np;
}

static inline int dit_enable_irq(struct dit_irq_t *irq)
{
	unsigned long flags;

	spin_lock_irqsave(&irq->lock, flags);

	if (irq->active) {
		spin_unlock_irqrestore(&irq->lock, flags);
		return 0;
	}

	enable_irq(irq->num);
	irq->active = 1;

	spin_unlock_irqrestore(&irq->lock, flags);

	return 0;
}

static inline int dit_disable_irq(struct dit_irq_t *irq)
{
	unsigned long flags;

	spin_lock_irqsave(&irq->lock, flags);

	if (!irq->active) {
		spin_unlock_irqrestore(&irq->lock, flags);
		return 0;
	}

	disable_irq_nosync(irq->num);
	irq->active = 0;

	spin_unlock_irqrestore(&irq->lock, flags);

	return 0;
}

static inline void *dit_free_irq(struct dit_irq_t *irq)
{
	char *name = NULL;

	if (irq->num > 0 && irq->registered) {
		name = (char *)free_irq(irq->num, &irq->dst);
		irq->registered = 0;
		dit_debug("%s freed\n", name);
	}

	return name;
}

int dit_init_hw_port(void)
{
	int cnt = 0;

	writel(0x1, dit_dev.reg_base + DIT_NAT_RX_PORT_INIT_START);
	writel(0x1, dit_dev.reg_base + DIT_NAT_TX_PORT_INIT_START);

	udelay(1);

	while (!readl(dit_dev.reg_base + DIT_NAT_RX_PORT_INIT_DONE)) {
		mdelay(1); cnt++;
		if (cnt > 100) {
			dit_err("RX_PORT_INIT failed\n");
			return -1;
		}
	}

	cnt = 0;
	while (!readl(dit_dev.reg_base + DIT_NAT_TX_PORT_INIT_DONE)) {
		mdelay(1); cnt++;
		if (cnt > 100) {
			dit_err("TX_PORT_INIT failed\n");
			return -1;
		}
	}

	return 0;
}

int dit_init_hw(void)
{
	int DIR, dst;

	dit_debug("++\n");

	dit_init_hw_port();

	writel(0x4020, dit_dev.reg_base + DIT_DMA_INIT_DATA);
	writel(0x1 << DIT_INIT_CMD, dit_dev.reg_base + DIT_SW_COMMAND);

	/* checksum */
	writel(0x0, dit_dev.reg_base + DIT_DMA_CHKSUM_OFF);
	writel(0xf, dit_dev.reg_base + DIT_NAT_ZERO_CHK_OFF);

	/* TX control */
	writel(0x80, dit_dev.reg_base + DIT_TX_DESC_CTRL_SRC); /* TX_DESC_BURST_LEN */
	writel(0x80, dit_dev.reg_base + DIT_TX_DESC_CTRL_DST); /* TX_DESC_BURST_LEN */

	writel(0x80, dit_dev.reg_base + DIT_TX_HEAD_CTRL); /* TX_HEAD_BURST_LEN */
	writel(0x20, dit_dev.reg_base + DIT_TX_MOD_HD_CTRL); /* TX_MOD_HD_BURST_LEN */
	writel(0x0, dit_dev.reg_base + DIT_TX_PKT_CTRL); /* TX_PKT_BURST_LEN */
	writel(0x20, dit_dev.reg_base + DIT_TX_CHKSUM_CTRL); /* TX_CHK_BURST_LEN */

	/* RX control */
	writel(0x0, dit_dev.reg_base + DIT_RX_DESC_CTRL_SRC); /* RX_DESC_BURST_LEN */
	writel(0x0, dit_dev.reg_base + DIT_RX_DESC_CTRL_DST); /* RX_DESC_BURST_LEN */

	writel(0x0, dit_dev.reg_base + DIT_RX_HEAD_CTRL); /* RX_HEAD_BURST_LEN */
	writel(0x0, dit_dev.reg_base + DIT_RX_MOD_HD_CTRL); /* RX_MOD_HD_BURST_LEN */
	writel(0x0, dit_dev.reg_base + DIT_RX_PKT_CTRL); /* RX_PKT_BURST_LEN */
	writel(0x0, dit_dev.reg_base + DIT_RX_CHKSUM_CTRL); /* RX_CHK_BURST_LEN */

	writel(0x103F, dit_dev.reg_base + DIT_INT_ENABLE);
	writel(0x103F, dit_dev.reg_base + DIT_INT_MASK);

	for (DIR = 0; DIR < DIT_MAX_FORWARD; DIR++) {
		struct dit_handle_t *h = &dit_dev.handle[DIR];

		for (dst = 0; dst < DIT_MAX_DST; dst++) {
#ifdef DIT_IOCC
			dit_debug("[%d][%d] setting DIT RING (0x%llx)\n", DIR, dst, virt_to_phys(h->desc[dst]));

			writel(virt_to_phys(h->desc[dst]) & 0xffffffff, dit_dev.reg_base + h->ring[dst].low);
			dit_debug(" - RING_ST_ADDR_0 (0x%08x) : 0x%x\n",
				h->ring[dst].low, readl(dit_dev.reg_base + h->ring[dst].low));
			writel((virt_to_phys(h->desc[dst]) >> 32) & 0xf, dit_dev.reg_base + h->ring[dst].high);
			dit_debug(" - RING_ST_ADDR_1 (0x%08x) : 0x%x\n",
				h->ring[dst].high, readl(dit_dev.reg_base + h->ring[dst].high));

			writel(virt_to_phys(h->desc[dst]) & 0xffffffff, dit_dev.reg_base + h->natdesc[dst].low);
			dit_debug(" - NAT_DESC_ADDR_0 (0x%08x) : 0x%x\n",
				h->natdesc[dst].low, readl(dit_dev.reg_base + h->natdesc[dst].low));
			writel((virt_to_phys(h->desc[dst]) >> 32) & 0xf, dit_dev.reg_base + h->natdesc[dst].high);
			dit_debug(" - NAT_DESC_ADDR_1 (0x%08x) : 0x%x\n",
				h->natdesc[dst].high, readl(dit_dev.reg_base + h->natdesc[dst].high));
#else
			dit_debug("[%d][%d] setting DIT RING (0x%llx)\n", DIR, dst, h->paddr[dst]);

			writel(h->paddr[dst] & 0xffffffff, dit_dev.reg_base + h->ring[dst].low);
			dit_debug(" - RING_ST_ADDR_0 (0x%08x) : 0x%x\n",
				h->ring[dst].low, readl(dit_dev.reg_base + h->ring[dst].low));
			writel((h->paddr[dst] >> 32) & 0xf, dit_dev.reg_base + h->ring[dst].high);
			dit_debug(" - RING_ST_ADDR_1 (0x%08x) : 0x%x\n",
				h->ring[dst].high, readl(dit_dev.reg_base + h->ring[dst].high));

			writel(h->paddr[dst] & 0xffffffff, dit_dev.reg_base + h->natdesc[dst].low);
			dit_debug(" - NAT_DESC_ADDR_0 (0x%08x) : 0x%x\n",
				h->natdesc[dst].low, readl(dit_dev.reg_base + h->natdesc[dst].low));
			writel((h->paddr[dst] >> 32) & 0xf, dit_dev.reg_base + h->natdesc[dst].high);
			dit_debug(" - NAT_DESC_ADDR_1 (0x%08x) : 0x%x\n",
				h->natdesc[dst].high, readl(dit_dev.reg_base + h->natdesc[dst].high));
#endif
		}
	}

	/* clock gate */
	writel(0xfffff, dit_dev.reg_base + DIT_CLK_GT_OFF);

	/* DIT Sharability control */
	writel(dit_dev.sharability_value, dit_dev.bus_base + dit_dev.sharability_offset);

	dit_debug("--\n");

	return 1;
}

/* dump2hex
 * dump data to hex as fast as possible.
 * the length of @buff must be greater than "@len * 3"
 * it need 3 bytes per one data byte to print.
 */

#define MAX_PRN_BUFFER_LEN	1500
static const char *hex = "0123456789abcdef";
static unsigned char prn_buffer[MAX_PRN_BUFFER_LEN*3+1];

static inline void dump2hex(char *buff, size_t buff_size,
			    const char *data, size_t data_len)
{
	char *dest = buff;
	size_t len;
	size_t i;

	if (buff_size < (data_len * 3))
		len = buff_size / 3;
	else
		len = data_len;

	for (i = 0; i < len; i++) {
		*dest++ = hex[(data[i] >> 4) & 0xf];
		*dest++ = hex[data[i] & 0xf];
		*dest++ = ' ';
	}

	/* The last character must be overwritten with NULL */
	if (likely(len > 0))
		dest--;

	*dest = 0;
}

/* print buffer as hex string */
static int pr_buffer(char *tag, const char *data, size_t data_len,
							size_t max_len)
{
	size_t len = min(data_len, max_len);

	max_len = MAX_PRN_BUFFER_LEN;
	len = min(len, max_len);
	dump2hex(prn_buffer, (len ? len * 3 : 1), data, len);

	/* don't change this printk to mif_debug for print this as level7 */
	return pr_info("%s: len=%ld: %s%s\n", tag, (long)data_len,
			prn_buffer, (len == data_len) ? "" : " ...");
}

void dit_debug_print_desc(struct dit_handle_t *h, int dst, int from, int to, char *tag)
{
	int n = from;
	int end = to;
	struct dit_desc_t *pdesc;
	char *pval;
	struct sk_buff **pskbuffs = h->skbarray[dst];
	struct sk_buff *skbrx;

	if (!dit_dev.enable) {
		dit_err("NOT enabled DIT now\n");
		return;
	}

	if (to < from) {
		dit_debug_print_desc(h, dst, from, DIT_MAX_DESC, tag);
		dit_debug_print_desc(h, dst, 0, to, tag);
		return;
	}

	pdesc = h->desc[dst];
	for ( ; n < end ; n++) {
		pval = (char *)&pdesc[n].val[0];
		skbrx = pskbuffs[n];

		pr_info("%s: desc[%d]: 0x%02x(status)_%02x(control)_%02x%02x(tran)_%02x%02x(org)_%02x%02x(len)_%02x%02x%02x%02x_%02x%02x%02x%02x\n",
			tag, n, pval[15], pval[14], pval[13], pval[12], pval[11], pval[10], pval[9], pval[8],
			pval[7], pval[6], pval[5], pval[4], pval[3], pval[2], pval[1], pval[0]);

		pr_buffer(tag, skbrx->data, 40, 40);
	}
}

void dit_debug_print_dst(int DIR)
{
	struct dit_dev_info_t *dit = &dit_dev;
	int dst;
	struct dit_handle_t *h = &dit->handle[DIR];

	dit_debug("#####++\n");
	for (dst = DIT_DST0; dst <= DIT_DST2; dst++) {
		dit_debug("DST%d\n", dst);
		dit_debug_print_desc(h, dst, 0, DIT_MAX_DESC, "DSTx");
	}
	dit_debug("#####--\n");
}

#if DIT_PRE_PROCESS
/* DIT 2.0, there is no space for netdev info
 * SW should pad net_device info of netdev for correspondng skb
 * __attach_ifinfo: when pad net_device info
 * __dettach_ifinfo: when remove padded net_device info
 */
static inline void __attach_ifinfo_skb(struct sk_buff *skb)
{
	struct dit_priv_t *priv;

	priv = (struct dit_priv_t *)(skb->data + skb->len);
	priv->ndev = skb->dev;
	priv->status = skb->ip_summed;

	skb->len += (uint32_t) sizeof(struct dit_priv_t);
}

static inline void __dettach_ifinfo(struct sk_buff *skb, int size, int DIR)
{
	struct dit_priv_t *priv;
	int len = size - sizeof(struct dit_priv_t);

	priv = (struct dit_priv_t *)(skb->data + len);
	skb->dev = priv->ndev;
	skb->ip_summed = priv->status;
	skb_put(skb, len);
}

/* update DIT descriptor START address
 * idx: -1 for next bank, otherwise where updating position
 */
static void dit_update_desc_position(struct dit_handle_t *h, int dst, int idx, char *tag)
{
	int offset;

	if (idx < 0) {
		/* Next bank */
		h->w_key[dst] = circ_new_ptr(h->num_desc, BANK_START_INDEX(h->w_key[dst]), MAX_UNITS_IN_BANK);
		idx = h->w_key[dst];
		offset = idx * sizeof(struct dit_desc_t);
	} else {
		offset = idx * sizeof(struct dit_desc_t);
	}
	dit_debug("%s%d: updating w_key %d\n", tag, dst, h->w_key[dst]);

#ifdef DIT_IOCC
	writel((virt_to_phys(h->desc[dst]) + offset) & 0xffffffff, dit_dev.reg_base + h->natdesc[dst].low);
	dit_debug_low("%s%d: - NAT_DESC_ADDR_0 (0x%08x) : 0x%x\n",
		tag, dst, h->natdesc[dst].low, readl(dit_dev.reg_base + h->natdesc[dst].low));
	writel(((virt_to_phys(h->desc[dst]) + offset) >> 32) & 0xf, dit_dev.reg_base + h->natdesc[dst].high);
	dit_debug_low("%s%d: - NAT_DESC_ADDR_1 (0x%08x) : 0x%x\n",
		tag, dst, h->natdesc[dst].high, readl(dit_dev.reg_base + h->natdesc[dst].high));
#else
	writel((h->paddr[dst] + offset) & 0xffffffff, dit_dev.reg_base + h->natdesc[dst].low);
	dit_debug_low("%s%d: - NAT_DESC_ADDR_0 (0x%08x) : 0x%x\n",
		tag, dst, h->natdesc[dst].low, readl(dit_dev.reg_base + h->natdesc[dst].low));
	writel(((h->paddr[dst] + offset) >> 32) & 0xf, dit_dev.reg_base + h->natdesc[dst].high);
	dit_debug_low("%s%d: - NAT_DESC_ADDR_1 (0x%08x) : 0x%x\n",
		tag, dst, h->natdesc[dst].high, readl(dit_dev.reg_base + h->natdesc[dst].high));
#endif

}

/* make DIT descriptor for SRC from skb
 * with DIT 2.0, net_device should be passed
 */
int dit_make_desc_skb(struct dit_dev_info_t *dit, int DIR, struct sk_buff *skb)
{
	int ret = 0;
	struct dit_handle_t *h = &dit->handle[DIR];

	int inkey = h->w_key[DIT_SRC];
	struct dit_desc_t *pdesc = h->desc[DIT_SRC];
	u8 ringend = (inkey == DIT_MAX_DESC - 1) ? (0x1 << 3) : 0;

	dit_debug_low("DIR=%d, inkey=%d (ringend=%d)\n", DIR, inkey, ringend);

	__attach_ifinfo_skb(skb);

	/* Set descriptor */
	pdesc[inkey].src.status = 0;
	pdesc[inkey].src.saddr = virt_to_phys(skb->data);
	pdesc[inkey].src.lengh = skb->len;

	pdesc[inkey].src.control = (0x1 << DIT_DESC_C_END)
							| (0x1 << DIT_DESC_C_START) | ringend;

	/* maitain skb buffer to free in next make_desc */
	skb_queue_tail(&dit_dev.handle[DIR].trash_q, skb);

	if (!ringend)
		h->w_key[DIT_SRC]++;
	else
		h->w_key[DIT_SRC] = 0;

	return ret;
}

/* DIT 2.0 DMA padding */
inline int dit_padd_desc_skb(struct dit_dev_info_t *dit, int DIR, int cnt)
{
	struct sk_buff *skb;
	int i;

	dit_debug_low("%d\n", cnt);

	for (i = 0; i < cnt; i++) {
		skb = dev_alloc_skb(DIT_BUFFER_DSIZE);
		if (!skb)
			break;
		skb_put_data(skb, padd_packet, SZ_PAD_PACKET);
		skb->dev = dit_dev.dummy_ndev;
		dit_make_desc_skb(dit, DIR, skb);
	}

	return cnt;
}

inline int is_dit_busy(struct dit_dev_info_t *dit, int DIR, int start, int end)
{
	int status;
	int pending;
	struct dit_handle_t *h = &dit->handle[DIR];

	status = readl(dit->reg_base + DIT_STATUS);
	status = status & (DIR ? DIT_TX_STATUS_MASK : DIT_RX_STATUS_MASK);
	if (status) {
#ifdef DIT_CHECK_PERF_TIME
		if (dit->stat[DIR].err_busy_hw == 0) {
			getnstimeofday(&dit->perf.fwd[DIR].startTimeBusy);
			dit->perf.fwd[DIR].busy_checking = true;
		}
#endif
		dit->stat[DIR].err_busy_hw++;
		dit_debug_low("DIR=%d, -EBUSY (0x%08x)\n", DIR, status);
		return -EBUSY;
	}
#ifdef DIT_CHECK_PERF_TIME
	if (dit->perf.fwd[DIR].busy_checking) {
		struct timespec ts;

		dit->perf.fwd[DIR].busy_checking = false;
		getnstimeofday(&dit->perf.fwd[DIR].endTimeBusy);
		ts = timespec_sub(dit->perf.fwd[DIR].endTimeBusy, dit->perf.fwd[DIR].startTimeBusy);
		dit_info("HW busy duration : %ld nsecs for %d units\n", ts.tv_nsec, end - start);
	}
#endif

	/* waiting while update desc addr of DSTx to DIT HW */
	pending = readl(dit->reg_base + DIT_INT_PENDING);
	pending = pending & (DIR ? DIT_TX_DST_INT_MASK : DIT_RX_DST_INT_MASK);
	if (pending) {
		dit->stat[DIR].err_pend++;
		dit_debug_low("DIR=%d, -EBUSY pending (0x%08x)\n", DIR, pending);
		return -EAGAIN;
	}

	/* check DSTx bankfull by checking DSTx */
	if (circ_full(MAX_BANK_ID, BANK_ID(h->w_key[DIT_DST0]), BANK_ID(h->r_key[DIT_DST0]))) {
		dit->stat[DIR].err_bankfull[DIT_DST0]++;
		dit_debug_low("DIR=%d, -EBUSY bankfull (Dst=%d) [ %d/ %d/ %d]\n", DIR, DIT_DST0,
			MAX_BANK_ID, BANK_ID(h->w_key[DIT_DST0]), BANK_ID(h->r_key[DIT_DST0]));
		return -EAGAIN;
	}

	if (circ_full(MAX_BANK_ID, BANK_ID(h->w_key[DIT_DST2]), BANK_ID(h->r_key[DIT_DST2]))) {
		dit->stat[DIR].err_bankfull[DIT_DST2]++;
		dit_debug_low("DIR=%d, -EBUSY bankfull (Dst=%d) [ %d/ %d/ %d]\n", DIR, DIT_DST2,
			MAX_BANK_ID, BANK_ID(h->w_key[DIT_DST2]), BANK_ID(h->r_key[DIT_DST2]));
		return -EAGAIN;
	}
	return 0;
}

/* Multi-input handling with backlog queue
 * DIR : DIT_RX__FORWARD, DIT_TX__FORWARD
 * skb : MUST set value of dev (struct net_device *) of skb
 */
int dit_enqueue_to_backlog(int DIR, struct sk_buff *skb)
{
	struct dit_dev_info_t *dit = &dit_dev;
	struct dit_handle_t *h = &dit->handle[DIR];

	if (!dit->enable) {
		dit_err("NOT enabled DIT now\n");
		return NET_RX_DROP;
	}

	if (dit->stat[DIR].err_nomem) {
		dit_err_limited("NO MEMORY\n");
		return NET_RX_DROP;
	}

	if (h->backlog_q.qlen > DIT_MAX_BACKLOG) {
		dit->stat[DIR].err_full_bq++;
		if (napi_schedule_prep(&dit_dev.napi.backlog_skb))
			__napi_schedule(&dit_dev.napi.backlog_skb);
		dit_debug_low("Too much traffic injected!!\n");
		dev_kfree_skb_any(skb);
		return NET_RX_DROP;
	}

	skb_queue_tail(&dit->handle[DIR].backlog_q, skb);

	dit->stat[DIR].injectpkt++;
	offload_update_reqst(DIR, skb->len);

	return NET_RX_SUCCESS;
}

int dit_schedule(int type)
{
#ifdef DIT_CHECK_PERF
	dit_dev.perf.shed_try++;
#endif

	if (!dit_dev.enable)
		return -1;

	if (type == DIT_BACKLOG_SKB) {
		if (napi_schedule_prep(&dit_dev.napi.backlog_skb))
			__napi_schedule(&dit_dev.napi.backlog_skb);
	} else {
		if (napi_schedule_prep(&dit_dev.napi.forward)) {
			__napi_schedule(&dit_dev.napi.forward);
		}
	}

	return 0;
}

/* Kick DIT H/W
 * return last index of desc (error negative value)
 */
int dit_kick(struct dit_dev_info_t *dit, int DIR, int start)
{
	struct dit_handle_t *h = &dit->handle[DIR];
	struct dit_desc_t *pdesc = h->desc[DIT_SRC];
	int end = (h->w_key[DIT_SRC] - 1);
	int ret;
	unsigned long flags;

	end = (end >= 0) ? end : DIT_MAX_DESC - 1;

	ret = is_dit_busy(dit, DIR, start, end);
	if (ret < 0)
		return ret;

	spin_lock_irqsave(&dit_dev.lock, flags);

	pdesc[start].src.control |= (0x1 << DIT_DESC_C_HEAD);
	pdesc[end].src.control |= (0x1 << DIT_DESC_C_TAIL) | (0x1 << DIT_DESC_C_INT);

	dit_debug_low("DIR=%d, start=%d, end=%d\n", DIR, start, end);

	dit_update_desc_position(h, DIT_SRC, start, "SRC");

	writel(DIR ? 0x01 << DIT_TX_CMD : 0x1 << DIT_RX_CMD, dit->reg_base + DIT_SW_COMMAND);

	dit->stat[DIR].kick_cnt++;
	spin_unlock_irqrestore(&dit_dev.lock, flags);

	return end;
}

/* Handle both RX and TX forward */
static inline int dit_backlog_skb_proc(int budget, int DIR)
{
	struct dit_dev_info_t *dit = &dit_dev;
	int ndesc, npadd;

	struct dit_handle_t *h = &dit->handle[DIR];
	int start, end;

	if (UNITS_IN_BANK(h->w_key[DIT_SRC])) {
		start = BANK_START_INDEX(h->w_key[DIT_SRC]);
		dit->stat[DIR].kick_re++;
		dit_debug_low("DIT_SRC still need to kick (DIR=%d)\n", DIR);
		goto kick_again;
	}

	if (h->backlog_q.qlen == 0) {
		dit_debug_low("qlen zero (DIR=%d)\n", DIR);
		return 0;
	}

	start = h->w_key[DIT_SRC];

	skb_queue_purge(&h->trash_q);

	ndesc = 0;
	npadd = 0;
	npadd += dit_padd_desc_skb(dit, DIR, 3); /* DIT 2.0 DMA padding */
	budget -= 3;
	while ((budget-- > 3) && (h->backlog_q.qlen > 0)) {  /* DIT 2.0 DMA padding */
		if (dit_make_desc_skb(dit, DIR, skb_dequeue(&h->backlog_q)) < 0)
			break;
		ndesc++;

#ifdef DIT_CHECK_PERF
		dit->perf.fwd[DIR].inpkt++;

#ifdef DIT_CHECK_PERF_TIME
		if (dit->perf.fwd[DIR].inpkt == MAX_PERF_TEST_PACKET_CNT) {
			struct timespec ts;

			getnstimeofday(&dit->perf.fwd[DIR].g_endT);
			ts = timespec_sub(dit->perf.fwd[DIR].g_endT, dit->perf.fwd[DIR].g_startT);

			dit_info("Total duration : %ld ms / %d packets\n",
					ts.tv_sec * MSEC_PER_SEC + ts.tv_nsec / NSEC_PER_MSEC,
					MAX_PERF_TEST_PACKET_CNT);

		}
#endif
#endif
	}
	npadd += dit_padd_desc_skb(dit, DIR, (3 - ndesc%3) + 1);  /* DIT 2.0 DMA padding */

	dit->stat[DIR].dit_inpkt += ndesc;
	dit->stat[DIR].padpkt += npadd;

#ifdef DIT_DEBUG_PANIC
	if (ndesc + npadd >= MAX_UNITS_IN_BANK) {
		dit_err("[[[[DIT Too many desc kicked Total: %d (ndesc=%d, npadd=%d)\n",
			ndesc + npadd, ndesc, npadd);
		panic("Too many desc kicked]]]]");
	}
#endif

	dit_debug("kick count %3d/%3d (start=%d, end=%d)\n", ndesc, ndesc + npadd, start, end);

kick_again:
	end = dit_kick(dit, DIR, start);

	if (end == -EBUSY)	/* HW busy */
		return -1;

	if (end == -EAGAIN)
		return -1;

	/* Next bank */
	h->w_key[DIT_SRC] = circ_new_ptr(h->num_desc, BANK_START_INDEX(start), MAX_UNITS_IN_BANK);
	dit_debug("%s: updating w_key %d\n", "SRC", h->w_key[DIT_SRC]);

#ifdef DIT_DEBUG_PKT
	dit_debug_print_desc(h, DIT_SRC, start, (end + 1) % DIT_MAX_DESC, "SRC");
#endif
	if (h->backlog_q.qlen > 0)
		return -1;

	return 0;
}

static enum hrtimer_restart dit_sched_skb_func(struct hrtimer *timer)
{
#ifdef DIT_CHECK_PERF
	dit_dev.perf.shed_skb++;
#endif

	dit_schedule(DIT_BACKLOG_SKB);
	return HRTIMER_NORESTART;
}

static enum hrtimer_restart dit_sched_fwd_func(struct hrtimer *timer)
{
#ifdef DIT_CHECK_PERF
	dit_dev.perf.shed_fwd++;
#endif

	dit_schedule(DIT_BACKLOG_FORWARD);
	return HRTIMER_NORESTART;
}

static int dit_backlog_skb_poll(struct napi_struct *napi, int budget)
{
	int ret = 0;
#ifdef DIT_CHECK_PERF_TIME
	struct timespec startTime, endTime;
#endif

	dit_debug_low("budget =%d\n", budget);

	if (readl(dit_dev.reg_base + DIT_DMA_INIT_DATA) == 0)
		dit_init_hw();

#ifdef DIT_CHECK_PERF
	dit_dev.perf.backlog_poll_cnt[DIT_BACKLOG_SKB]++;
#ifdef DIT_CHECK_PERF_TIME
	if (dit_dev.perf.backlog_poll_cnt[DIT_BACKLOG_SKB] == 1) {
		getnstimeofday(&startTime);
		getnstimeofday(&dit_dev.perf.fwd[DIT_TX__FORWARD].g_startT);
		if (dit_dev.perf.test_case == DIT_PERF_TEST_SKB_RXGEN)
			getnstimeofday(&dit_dev.perf.fwd[DIT_RX__FORWARD].g_startT);
	}
#endif
#endif

	ret += dit_backlog_skb_proc(budget, DIT_RX__FORWARD);
	ret += dit_backlog_skb_proc(budget>>1, DIT_TX__FORWARD);

	napi_complete_done(napi, 0);

	if (ret < 0) {
		hrtimer_start(&dit_dev.sched_skb_timer, ns_to_ktime(DIT_SCHED_BACKOFF_TIME_NS), HRTIMER_MODE_REL);
	}

#ifdef DIT_CHECK_PERF_TIME
	if (dit_dev.perf.backlog_poll_cnt[DIT_BACKLOG_SKB] == 1) {
		struct timespec ts;

		getnstimeofday(&endTime);
		ts = timespec_sub(endTime, startTime);
		dit_info("mak_desc&kick duration : %ld nsecs\n", ts.tv_nsec);
	}
#endif

	return 0;
}
#endif

#if DIT_POST_PROCESS
/* dev core functions for forward */
static inline int dit_dev_forward_queue_skb(struct sk_buff *skb, struct Qdisc *q)
{
	spinlock_t *root_lock = qdisc_lock(q);
	struct sk_buff *to_free = NULL;
	bool contended;
	int rc;

	qdisc_calculate_pkt_len(skb, q);
	contended = qdisc_is_running(q);
	if (unlikely(contended))
		spin_lock(&q->busylock);

	spin_lock(root_lock);
	if (unlikely(test_bit(__QDISC_STATE_DEACTIVATED, &q->state))) {
		__qdisc_drop(skb, &to_free);
		rc = NET_XMIT_DROP;
#ifdef DIT_CHECK_PERF
		dit_dev.perf.txq_inactive++;
#endif
	} else {
		rc = q->enqueue(skb, q, &to_free) & NET_XMIT_MASK;
		if (unlikely(contended)) {
			spin_unlock(&q->busylock);
			contended = false;
		}
#ifdef DIT_CHECK_PERF
		if (rc == NET_XMIT_DROP) {
			dit_dev.perf.txq_drop++;
#ifdef DIT_CHECK_PERF_TIME
			if (dit_dev.perf.txq_drop == 1) {
				struct timespec ts;

				getnstimeofday(&dit_dev.perf.devfwd.g_endT);
				ts = timespec_sub(dit_dev.perf.devfwd.g_endT, dit_dev.perf.devfwd.g_startT);
				dit_info("first txq drop: %ld nsecs (%ld us) fwd_done=%d pkts\n",
					ts.tv_nsec, ts.tv_nsec / USEC_PER_MSEC, dit_dev.perf.devfwd.fwd_done);
			}
		} else {
			dit_dev.perf.devfwd.fwd_done++;
			getnstimeofday(&dit_dev.perf.devfwd.g_startT);
#endif
		}
#endif
	}
	spin_unlock(root_lock);
	if (unlikely(to_free))
		kfree_skb_list(to_free);
	if (unlikely(contended))
		spin_unlock(&q->busylock);
	return rc;
}

int dit_forward_queue_xmit(struct sk_buff *skb, int DIR)
{
	struct net_device *dev = skb->dev;
	struct netdev_queue *txq;
	struct Qdisc *q;
	int rc = NET_XMIT_DROP;
	struct dit_dev_info_t *dit = &dit_dev;

	dit_debug_low("ndev =%s\n", skb->dev->name);

	rcu_read_lock();

	qdisc_skb_cb(skb)->pkt_len = skb->len;

	skb_set_queue_mapping(skb, 0);
	txq = netdev_get_tx_queue(dev, 0);
	q = rcu_dereference(txq->qdisc);

	if (q->enqueue)
		rc = dit_dev_forward_queue_skb(skb, q);

	rcu_read_unlock();

	switch (rc) {
	case NET_XMIT_SUCCESS:
		offload_update_stat(DIR, skb->len);
		break;
	case NETDEV_TX_BUSY:
		dit->stat[DIR].droppkt_busy++;
		kfree_skb(skb);
		break;
	case NET_XMIT_DROP:
		dit->stat[DIR].droppkt_drop++;
		break;
	case NET_XMIT_CN:
		dit->stat[DIR].droppkt_cn++;
		break;
	default:
		break;
	}

	return rc;
}

int dit_forward_skb(struct sk_buff *skb, int DIR, int dst)
{
	struct net_device *ndev;
	struct ethhdr *ehdr;
	struct iphdr *iphdr;
	int ifnet;
	int ret;

	iphdr = (struct iphdr *)skb->data;
	if (iphdr->version == 0x6) {
		skb->protocol = htons(ETH_P_IPV6);
		dit_dev.stat[DIR].dst_ipv6_cnt[dst]++;
	} else {
		skb->protocol = htons(ETH_P_IP);
		dit_dev.stat[DIR].dst_ipv4_cnt[dst]++;
	}

	skb_reset_network_header(skb);

	dit_dev.stat[DIR].dst_pkt_cnt[dst]++;
	dit_dev.stat[DIR].dit_outpkt++;

	switch (dst) {
	case DIT_DST2: /* RX: USB, TX: - */
		if (DIR == DIT_TX__FORWARD)
			dit_err("unexpected recv: DIR %d, dst=%d\n", DIR, dst);
		else {
			ehdr = (struct ethhdr *)skb_push(skb, sizeof(struct ethhdr));
			memcpy(ehdr->h_dest, dit_dev.host[0].hostmac, ETH_ALEN);
			memcpy(ehdr->h_source, dit_dev.host[0].ifmac, ETH_ALEN);
			ehdr->h_proto = skb->protocol;
			skb_reset_mac_header(skb);
		}
		break;

	case DIT_DST1: /* RX: WiFi, TX: CP */
		if (DIR == DIT_TX__FORWARD) {
			skb_reset_mac_header(skb);
#ifdef DIT_FEATURE_USE_WIFI
		} else {
			ehdr = (struct ethhdr *)skb_push(skb, sizeof(struct ethhdr));
			memcpy(ehdr->h_dest, dit_dev.host[0].hostmac, ETH_ALEN);
			memcpy(ehdr->h_source, dit_dev.host[0].ifmac, ETH_ALEN);
			ehdr->h_proto = skb->protocol;
			skb_reset_mac_header(skb);
#endif
		}
		break;

	case DIT_DST0: /* AP */
	default:
		ndev = skb->dev;
		skb_reset_mac_header(skb);
		skb_reset_transport_header(skb);
		ret = netif_receive_skb(skb);
		if (ret != NET_RX_SUCCESS)
			ndev->stats.rx_dropped++;
		return 0;
	}

	ifnet = dit_dev.Dst2if[DIR][dst];
	if (ifnet < 0) {
		dit_err("wrong access for DIR=%d, dst=%d\n", DIR, dst);
		dev_kfree_skb_any(skb);
		return -1;
	}

	skb->dev = dit_dev.ifdev[ifnet].ndev;
	if (!skb->dev) {
		dit_err_limited("netdevice not connected for DIR=%d, dst=%d\n", DIR, dst);
		dev_kfree_skb_any(skb);
		return -1;
	}

	if (dit_dev.handle[DIR].forward_q.qlen > DIT_MAX_BACKLOG) {
		dev_kfree_skb_any(skb);
		dit_dev.stat[DIR].err_full_fq++;
		dit_debug_low("Too much packets in forward_q!!\n");
		return -1;
	}

	skb_queue_tail(&dit_dev.handle[DIR].forward_q, skb);

	return 0;
}

static int dit_forward_poll(struct napi_struct *napi, int budget)
{
	struct dit_dev_info_t *dit = &dit_dev;
	int DIR;
	int ret = 0;
	int i;
	bool repoll = false;

	dit_debug_low("budget =%d\n", budget);

#ifdef DIT_CHECK_PERF
	dit->perf.forward_poll_cnt++;
#endif

	for (DIR = 0; DIR < DIT_MAX_FORWARD; DIR++) {
		struct dit_handle_t *h = &dit->handle[DIR];

		if (h->forward_q.qlen == 0) {
			dit_debug_low("qlen zero (DIR=%d)\n", DIR);
			continue;
		}

		while ((h->forward_q.qlen > 0) && (budget-- > 0) && !ret)
			ret = dit_forward_queue_xmit(skb_dequeue(&h->forward_q), DIR);

		if (h->forward_q.qlen > 0 || ret)
			repoll = true;
	}

	for (i = 0; i < DIT_IF_MAX; i++)
		if (dit_dev.ifdev[i].txq)
			netif_schedule_queue(dit_dev.ifdev[i].txq);

	napi_complete_done(napi, 0);

	if (repoll) {
		hrtimer_start(&dit_dev.sched_fwd_timer, ns_to_ktime(10000), HRTIMER_MODE_REL);
	}
	return 0;
}

static int dit_dst0_poll(struct napi_struct *napi, int budget)
{
	int DIR = DIT_RX__FORWARD;
	int dst = DIT_DST0;
	struct dit_dev_info_t *dit = &dit_dev;
	struct dit_handle_t *h = &dit->handle[DIR];
	char *tag = dit->irq[dst].name;

	dit->stat[DIR].dst_poll_cnt[dst]++;
	pass_skb_to_netdev(DIR, dst, h->r_key[dst], tag);

	napi_complete_done(napi, 0);
	dit_enable_irq(&dit->irq[dst]);

	return 0;
}

static int dit_dst1_poll(struct napi_struct *napi, int budget)
{
	int DIR = DIT_RX__FORWARD;
	int dst = DIT_DST1;
	struct dit_dev_info_t *dit = &dit_dev;
	struct dit_handle_t *h = &dit->handle[DIR];
	char *tag = dit->irq[dst].name;

	dit->stat[DIR].dst_poll_cnt[dst]++;
	pass_skb_to_netdev(DIR, dst, h->r_key[dst], tag);

	dit_schedule(DIT_BACKLOG_FORWARD);

	napi_complete_done(napi, 0);
	dit_enable_irq(&dit->irq[dst]);

	return 0;
}

static int dit_dst2_poll(struct napi_struct *napi, int budget)
{
	int DIR = DIT_RX__FORWARD;
	int dst = DIT_DST2;
	struct dit_dev_info_t *dit = &dit_dev;
	struct dit_handle_t *h = &dit->handle[DIR];
	char *tag = dit->irq[dst].name;

	dit->stat[DIR].dst_poll_cnt[dst]++;
	pass_skb_to_netdev(DIR, dst, h->r_key[dst], tag);

	dit_schedule(DIT_BACKLOG_FORWARD);

	napi_complete_done(napi, 0);
	dit_enable_irq(&dit->irq[dst]);

	return 0;
}

static int dit_tx_dst_poll(struct napi_struct *napi, int budget)
{
	int DIR = DIT_TX__FORWARD;
	int dst = DIT_DST0;
	struct dit_dev_info_t *dit = &dit_dev;
	struct dit_handle_t *h = &dit->handle[DIR];
	char *tag = dit->irq[DIT_dts_idx(INTREQ__DIT_Tx)].name;
	int ret;
	int dst_pending;

	dst_pending = h->tx_pending;
	for (dst = 0 ; dst_pending ; dst_pending = dst_pending >> 1, dst++) {
		if (dst_pending & 0x1) {
			dit->stat[DIR].dst_poll_cnt[dst]++;
			//dit_err("%s: pass_skb_to_netdev DIR=%d, dst=%d\n", "TxDst", DIR, dst);
			ret = pass_skb_to_netdev(DIR, dst, h->r_key[dst], tag);
		}
	}

	dit_schedule(DIT_BACKLOG_FORWARD);

	napi_complete_done(napi, 0);
	dit_enable_irq(&dit->irq[DIT_dts_idx(INTREQ__DIT_Tx)]);

	return 0;
}

static inline void dit_free_buffer(struct dit_handle_t *h, int dst)
{
	int i;
	struct sk_buff **pskbuffs = h->skbarray[dst];

	for (i = 0; i < DIT_MAX_DESC ; i++)
		dev_kfree_skb_any(pskbuffs[i]);
}

static inline struct sk_buff *dit_prealloc_buffer(struct dit_handle_t *h, int dst, int index, char *tag)
{
	struct sk_buff *skb = dev_alloc_skb(DIT_BUFFER_DSIZE);
	struct dit_desc_t *pdesc = h->desc[dst];
	struct sk_buff **pskbuffs = h->skbarray[dst];
#ifdef DIT_DEBUG_PKT
	char *pval;
#endif
	if (!skb) {
		dit_err("prealloc fail!!!\n");
		pdesc[index].dst.daddr = virt_to_phys(NULL);
		pskbuffs[index] = NULL;
		return NULL;
	}

	pskbuffs[index] = skb;
	skb->data[0] = 0x00;
	pdesc[index].dst.daddr = virt_to_phys(skb->data);

#ifdef DIT_DEBUG_PKT
	pval = (char *)&pdesc[index].val[0];

	pr_info("%s%s: desc[%d]: 0x%02x(status)_%02x(control)_%02x%02x(tran)_%02x%02x(org)_%02x%02x(len)_%02x%02x%02x%02x_%02x%02x%02x%02x\n",
		tag, "-alloc", index, pval[15], pval[14], pval[13], pval[12], pval[11], pval[10], pval[9], pval[8],
		pval[7], pval[6], pval[5], pval[4], pval[3], pval[2], pval[1], pval[0]);
#endif
	return skb;
}

/* Pass DIT RX SKB to interfaces
 * DIR: RX or TX
 */
int pass_skb_to_netdev(int DIR, int dst, int start, char *tag)
{
	struct dit_dev_info_t *dit = &dit_dev;
	struct dit_handle_t *h = &dit->handle[DIR];
	int i = start, ret;
	u8 status, ringend;
	struct dit_desc_t *pdesc = h->desc[dst];
	struct sk_buff **pskbuffs = h->skbarray[dst];
	struct sk_buff *skbrx;
	int size;

	dit_debug("%s: start=%d\n", tag, start);

	do {
		status = pdesc[i].dst.status;
		if (~status & 0x01) {
			dit_debug("%s: dst=%d i=%d status(0x%02x) NOT DONE\n", tag, dst, i, status);
			break;
		}

		ringend = pdesc[i].dst.control & (0x1 << DIT_DESC_C_RINGEND);
		skbrx = pskbuffs[i];

#ifdef DIT_DEBUG_PANIC
		if (ringend) {
			dit_err("[[[[DIT forwarding ringend DIR=%d, dst=%d, i=%d (start=%d)\n",
				DIR, dst, i, start);
			dit_debug_print_desc(h, dst, i, (i + 1) % DIT_MAX_DESC, tag);
			panic("DIT forwarding ringend]]]]");
		}
		if (i >= DIT_MAX_DESC) {
			dit_err("[[[[DIT forwarding index(i) over DIR=%d, dst=%d, i=%d (start=%d)\n",
				DIR, dst, i, start);
			dit_debug_print_desc(h, dst, i, (i + 1) % DIT_MAX_DESC, tag);
			panic("DIT forwarding index(i) over]]]]");
		}
#endif

		size = pdesc[i].dst.lengh;
		__dettach_ifinfo(skbrx, size, DIR); /* put len here */

#ifdef DIT_DEBUG_PKT
		dit_debug("%s: pskbuffs[%d] lengh=%d\n", tag, i, skbrx->len);
		dit_debug_print_desc(h, dst, i, (i + 1) % DIT_MAX_DESC, tag);
#endif

		if (likely(skbrx->dev != dit_dev.dummy_ndev))
			dit_forward_skb(skbrx, DIR, dst);
		else {
			if ((skbrx->data[0] & 0xff) != 0x00) {
				dit->stat[DIR].err_trans[dst]++;
				dit_err("[[[[DIT err_trans DIR=%d, dst=%d, i=%d (skb->len=%d)\n",
					DIR, dst, i, skbrx->len);
				dit_debug_print_desc(h, dst, i, (i + 1) % DIT_MAX_DESC, tag);
			}
			dev_kfree_skb_any(skbrx);
		}

		if (!dit_prealloc_buffer(h, dst, i, tag)) {
			dit_err_limited("[DST%d] failed to prealloc (%d)\n", dst, i);
			dit->stat[DIR].err_nomem = 1;
			break;
		}

		pdesc[i].dst.status = 0x00;

		i = (ringend ? 0 : i + 1);
	} while (1);

	ret = (i - start) >= 0 ? (i - start) - 1 : (DIT_MAX_DESC + i - start) - 1;

	h->r_key[dst] = circ_new_ptr(h->num_desc, BANK_START_INDEX(start), MAX_UNITS_IN_BANK);	/* Next bank */
	dit_debug("%s: update r_key:%d\n", tag, h->r_key[dst]);
	dit_debug("%s: Passed to net:%d (i=%d, start=%d)\n", tag, ret, i, start);

	return ret;
}

/* IRQ handlers */
irqreturn_t dit_errbus_irq_handler(int irq, void *arg)
{
	struct dit_dev_info_t *dit = &dit_dev;

	dit_err_limited("0x%08x\n", readl(dit->reg_base + DIT_INT_PENDING));

	writel((0x1 << DIT_ERR_INT_PENDING), dit->reg_base + DIT_INT_PENDING);

	dit_err_limited("DIT_STATUS:0x%08x\n", readl(dit->reg_base + DIT_STATUS));
	dit_err_limited("DIT_BUS_ERR:0x%08x\n", readl(dit->reg_base + DIT_BUS_ERROR));

	writel(0x3, dit->reg_base + DIT_BUS_ERROR);

	dit_deinit(DIT_DEINIT_REASON_FAIL);

	if (dit_init(DIT_INIT_REASON_FAIL) < 0)
		dit_err("dit_init failed\n");

	return IRQ_HANDLED;
}

/* DST0 : AP, DST1 : WiFi, DST2 : USB */
irqreturn_t dit_rx_irq_handler(int irq, void *arg)
{
	struct dit_dev_info_t *dit = &dit_dev;
	struct dit_handle_t *h = &dit->handle[DIT_RX__FORWARD];
	int dst = *(int *)arg;
	int pending;
	int start;
	char *tag;

	start = h->r_key[dst];
	tag = dit->irq[dst].name;

	dit->stat[DIT_RX__FORWARD].rx_irq_cnt[dst]++;
	dit_update_desc_position(h, dst, DIT_NEXT_BANK, "RxDst");

	pending = DIT_RX_DST_INT_MASK & readl(dit->reg_base + DIT_INT_PENDING);
	dit_debug("pending=0x%x\n", pending);
	writel((0x1 << (DIT_RX_DST0_INT_PENDING + dst)), dit->reg_base + DIT_INT_PENDING);

	if (napi_schedule_prep(&dit->napi.forward_dst[dst])) {
		__napi_schedule(&dit->napi.forward_dst[dst]);
		dit_disable_irq(&dit->irq[dst]);
	}

	return IRQ_HANDLED;
}

/* DST0 : AP, DST1 : CP */
irqreturn_t dit_tx_irq_handler(int irq, void *arg)
{
	struct dit_dev_info_t *dit = &dit_dev;
	struct dit_handle_t *h = &dit->handle[DIT_TX__FORWARD];
	int dst = *(int *)arg;
	int dst_pending;

	dst_pending = h->tx_pending = DIT_TX_DST_INT_MASK & readl(dit->reg_base + DIT_INT_PENDING);

	dit_debug("pending=0x%x\n", dst_pending);
	for (dst = 0 ; dst_pending ; dst_pending = dst_pending >> 1, dst++) {
		if (dst_pending & 0x1) {
			dit->stat[DIT_TX__FORWARD].rx_irq_cnt[dst]++;
			dit_update_desc_position(h, dst, DIT_NEXT_BANK, "TxDst");
			//dit_err("%s: pending=0x%x\n", "TxDst", h->tx_pending);
		}
	}
	writel(h->tx_pending, dit->reg_base + DIT_INT_PENDING);

	if (napi_schedule_prep(&dit->napi.forward_dst[DIT_dts_idx(INTREQ__DIT_Tx)])) {
		__napi_schedule(&dit->napi.forward_dst[DIT_dts_idx(INTREQ__DIT_Tx)]);
		dit_disable_irq(&dit->irq[DIT_dts_idx(INTREQ__DIT_Tx)]);
	}

	return IRQ_HANDLED;
}

#endif

#if DIT_NETFILTER_API
int dit_set_nat_local_addr(int ifnet, u32 addr)
{
	struct dit_dev_info_t *dit = &dit_dev;
	unsigned long flags;

	/* TBD: for multiple host - find mac address for both ifnet and addr,
	 * finally fill ifmac and hostmac
	 */
	if (ifnet == DIT_IF_RMNET) {
		spin_lock_irqsave(&dit->lock, flags);
		if (readl(dit->reg_base + DIT_DMA_INIT_DATA) == 0)
			dit_init_hw();

		writel(addr, dit->reg_base + DIT_NAT_UE_ADDR);
		spin_unlock_irqrestore(&dit->lock, flags);

		dit_dev.ue_addr = addr;
		addr = readl(dit->reg_base + DIT_NAT_UE_ADDR);
		dit_info("UE address:%pI4\n", &addr);

		return 1;
	}

	if (ifnet == DIT_IF_USB) {
		dit->host[0].enabled = true;
		dit->host[0].addr = addr;
		dit->host[0].id = 0;

		/* TBD: for multiple host - check need to add ndev */
		dit->host[0].ndev = NULL;

		spin_lock_irqsave(&dit->lock, flags);
		if (readl(dit->reg_base + DIT_DMA_INIT_DATA) == 0)
			dit_init_hw();

		writel(addr, dit->reg_base + DIT_NAT_LOCAL_ADDR_0);
		spin_unlock_irqrestore(&dit->lock, flags);

		addr = readl(dit->reg_base + DIT_NAT_LOCAL_ADDR_0);
		dit_debug("LOCAL address (%d):%pI4\n", 0, &addr);

		return 1;
	}

#ifdef DIT_FEATURE_USE_WIFI
	/* TBD: bitmasking for 15 WLAN clients */
	for (i = dit->last_host, ntime = 0; ntime < DIT_MAX_HOST; ntime++) {
		if (!dit->host[i].enabled) {
			dit->host[i].enabled = true;
			dit->host[i].addr = addr;
			dit->host[i].id = i;

			/* TBD: check need to add ndev */
			dit->host[i].ndev = NULL;
			dit->last_host = i;

			spin_lock_irqsave(&dit->lock, flags);
			if (readl(dit->reg_base + DIT_DMA_INIT_DATA) == 0)
				dit_init_hw();

			writel(addr, dit->reg_base + DIT_NAT_LOCAL_ADDR_0 + (i * sizeof(u32)));
			spin_unlock_irqrestore(&dit->lock, flags);

			addr = readl(dit->reg_base + DIT_NAT_LOCAL_ADDR_0 + (i * sizeof(u32)));
			dit_info("LOCAL address (%d):%pI4\n", i, &addr);

			return 1;
		}

		i++; i %= DIT_MAX_HOST;
	}
#endif
	return -1;
}

void dit_clear_nat_local_addr(int ifnet)
{
	u32 addr = INADDR_ANY;
	struct dit_dev_info_t *dit = &dit_dev;
	unsigned long flags;

	if (ifnet == DIT_IF_RMNET) {
		spin_lock_irqsave(&dit->lock, flags);
		if (readl(dit->reg_base + DIT_DMA_INIT_DATA) == 0)
			dit_init_hw();

		writel(addr, dit->reg_base + DIT_NAT_UE_ADDR);
		spin_unlock_irqrestore(&dit->lock, flags);

		addr = readl(dit->reg_base + DIT_NAT_UE_ADDR);
		dit_info("UE address:%pI4\n", &addr);

		return;
	}

	if (ifnet == DIT_IF_USB) {
		dit->host[0].enabled = false;
		dit->host[0].addr = addr;
		dit->host[0].id = -1;

		dit->host[0].ndev = NULL;

		spin_lock_irqsave(&dit->lock, flags);
		if (readl(dit->reg_base + DIT_DMA_INIT_DATA) == 0)
			dit_init_hw();

		writel(addr, dit->reg_base + DIT_NAT_LOCAL_ADDR_0);
		spin_unlock_irqrestore(&dit->lock, flags);

		addr = readl(dit->reg_base + DIT_NAT_LOCAL_ADDR_0);
		dit_info("LOCAL address (%d):%pI4\n", 0, &addr);

		return;
	}

#ifdef DIT_FEATURE_USE_WIFI
	/* TBD: bitmasking for 15 WLAN clients */
	for (i = dit->last_host, ntime = DIT_MAX_HOST - 1; ntime >= 0; ntime--) {
		if (dit->host[i].enabled) {
			dit->host[i].enabled = false;
			dit->host[i].addr = addr;
			dit->host[i].id = -1;

			dit->host[i].ndev = NULL;

			spin_lock_irqsave(&dit->lock, flags);

			writel(addr, dit->reg_base + DIT_NAT_LOCAL_ADDR_0 + (i * sizeof(u32)));
			spin_unlock_irqrestore(&dit->lock, flags);

			addr = readl(dit->reg_base + DIT_NAT_LOCAL_ADDR_0 + (i * sizeof(u32)));
			dit_info("local address (%d):%pI4\n", i, &addr);

			return;
		}

		i--; i += DIT_MAX_HOST; i %= DIT_MAX_HOST;
	}
#endif
	dit_err("can't find local address:0x%08x\n", addr);

	return;

}

void dit_set_local_mac(char *addr)
{
	u32 addr32 = *(u32 *)&addr[0];
	u16 addr16 = *(u32 *)&addr[4];

	writel(addr32, dit_dev.reg_base + DIT_NAT_ETHERNET_DST_MAC_ADDR_0);
	writel(addr16 & 0x0000ffff, dit_dev.reg_base + DIT_NAT_ETHERNET_DST_MAC_ADDR_1);
}

void dit_clear_local_mac(void)
{
	u32 addr32 = 0;
	u16 addr16 = 0;

	writel(addr32, dit_dev.reg_base + DIT_NAT_ETHERNET_DST_MAC_ADDR_0);
	writel(addr16 & 0x0000ffff, dit_dev.reg_base + DIT_NAT_ETHERNET_DST_MAC_ADDR_1);
}

void dit_set_devif_mac(char *addr)
{
	u32 addr32 = *(u32 *)&addr[0];
	u16 addr16 = *(u32 *)&addr[4];

	writel(addr32, dit_dev.reg_base + DIT_NAT_ETHERNET_SRC_MAC_ADDR_0);
	writel(addr16 & 0x0000ffff, dit_dev.reg_base + DIT_NAT_ETHERNET_SRC_MAC_ADDR_1);
}

void dit_clear_devif_mac(void)
{
	u32 addr32 = 0;
	u16 addr16 = 0;

	writel(addr32, dit_dev.reg_base + DIT_NAT_ETHERNET_SRC_MAC_ADDR_0);
	writel(addr16 & 0x0000ffff, dit_dev.reg_base + DIT_NAT_ETHERNET_SRC_MAC_ADDR_1);

	writel(0x0, dit_dev.reg_base + DIT_NAT_ETHERNET_EN);
}

/* NAT filter */
void dit_set_nat_rxfilter(int dst, int hostidx, __be16 iport, __be16 xport, bool on)
{
	struct dit_dev_info_t *dit = &dit_dev;
	unsigned long flags;
	struct dit_table_t entry;
	u32 val;
	char *pval;

	dit_debug("on=%d [dst=%d, hostidx=%d, iport:0x%04x, xport:0x%04x] at table 0x%04x\n",
			on, dst, hostidx, ntohs(iport), ntohs(xport),
			PTABLE_IDX(iport));

	entry.rx.enable = on;
	entry.rx.o_port = (iport & 0xff00) >> 8;
	entry.rx.x_port = (xport);
	entry.rx.addr_sel = hostidx;
	entry.rx.dma_sel = dst;

	pval = (char *)&entry.val;
	dit_debug("entry(0x%02x%02x%02x%02x): dma=%d, x_port=0x%04x, iport(hi)=0x%02x, addr=%d, on=%d\n",
		pval[3], pval[2], pval[1], pval[0], entry.rx.dma_sel, (entry.rx.x_port),
		entry.rx.o_port, entry.rx.addr_sel, entry.rx.enable);

	spin_lock_irqsave(&dit->lock, flags);

	writel(entry.val, dit_dev.reg_base + DIT_NAT_RX_PORT_TABLE_SLOT_0 + PTABLE_IDX(iport));

	val = readl(dit_dev.reg_base + DIT_NAT_RX_PORT_TABLE_SLOT_0 + PTABLE_IDX(iport));
	pval = (char *)&val;
	dit_debug("get entry: 0x%02x%02x%02x%02x\n", pval[3], pval[2], pval[1], pval[0]);

	spin_unlock_irqrestore(&dit->lock, flags);
}

void dit_set_nat_txfilter(int dst, __be16 iport, __be16 xport, bool on)
{
	struct dit_dev_info_t *dit = &dit_dev;
	unsigned long flags;
	struct dit_table_t entry;
	u32 val;
	char *pval;

	dit_debug("on=%d [dst=%d, iport:0x%04x, xport:0x%04x] at table 0x%04x\n",
			on, dst, ntohs(iport), ntohs(xport),
			PTABLE_IDX(iport));

	entry.tx.enable = on;
	entry.tx.o_port = (iport & 0xff00) >> 8; //ntohs(org_port) >> 8;;
	entry.tx.x_port = (xport);
	entry.tx.dma_sel = dst;

	pval = (char *)&entry.val;
	dit_debug("entry(0x%02x%02x%02x%02x): dma=%d, x_port=0x%04x, iport(hi)=0x%02x, on=%d\n",
		pval[3], pval[2], pval[1], pval[0], entry.tx.dma_sel, (entry.tx.x_port), entry.tx.o_port,
		entry.tx.enable);

	spin_lock_irqsave(&dit->lock, flags);

	writel(entry.val, dit_dev.reg_base + DIT_NAT_TX_PORT_TABLE_SLOT_0 + PTABLE_IDX(iport));

	val = readl(dit_dev.reg_base + DIT_NAT_TX_PORT_TABLE_SLOT_0 + PTABLE_IDX(iport));
	pval = (char *)&val;
	dit_debug("get entry: 0x%02x%02x%02x%02x\n", pval[3], pval[2], pval[1], pval[0]);

	spin_unlock_irqrestore(&dit->lock, flags);
}

void dit_forward_set(int ifnet, struct net_device *ndev, bool enable)
{
	dit_info("idx=%d, %s (enable=%d)\n", ifnet, ndev->name, enable);
	dit_dev.ifdev[ifnet].ndev = enable ? ndev : NULL;
	dit_dev.ifdev[ifnet].enabled = enable;
	dit_dev.ifdev[ifnet].txq = netdev_get_tx_queue(ndev, 0);
}

int dit_forward_add(__be16 reply_port, __be16 org_port, int ifnet)
{
	dit_debug("[ifnet:%d, reply_port:0x%04x, org_port:0x%04x]\n",
			ifnet, ntohs(reply_port), ntohs(org_port));

#ifdef DIT_FEATURE_USE_WIFI
	if (org_port != reply_port)
		return -1; /* No supporting port translation */
#endif

	/* RX - TBD: there could be multiple host in ifnet */
	dit_set_nat_rxfilter(dit_dev.if2Dst[DIT_RX__FORWARD][ifnet], 0, reply_port, org_port, 1);

	/* TX */
	dit_set_nat_txfilter(dit_dev.if2Dst[DIT_TX__FORWARD][DIT_IF_RMNET], org_port, reply_port, 1);

	return 0;
}

int dit_forward_delete(__be16 reply_port, __be16 org_port, int ifnet)
{
	__be16 iport, xport;

	dit_debug("[ifnet:%d, reply_port:0x%04x, org_port:0x%04x]\n",
			ifnet, (reply_port), (org_port));

#ifdef DIT_FEATURE_USE_WIFI
	if (org_port != reply_port)
		return -1; /* No supporting port translation */
#endif

	/* RX - TBD: there could be multiple host in ifnet */
	iport = reply_port;
	xport = org_port;
	dit_set_nat_rxfilter(dit_dev.if2Dst[DIT_RX__FORWARD][ifnet], 0, iport, xport, 0);

	/* TX */
	iport = org_port;
	xport = reply_port;
	dit_set_nat_txfilter(dit_dev.if2Dst[DIT_TX__FORWARD][DIT_IF_RMNET], iport, xport, 0);

	return 0;
}

inline int dit_check_enable(struct net_device  *ndev)
{
	int i;

	if (dit_dev.enable) {
		for (i = 0; i < DIT_IF_MAX; i++)
			if (dit_dev.ifdev[i].ndev == ndev) {
				dit_debug("[%d] %s enabled\n", i, ndev->name);
				return i;
			}
	}
	return -1;
}

static inline bool dit_check_ue_addr(__be32 addr)
{
	dit_debug("%pI4 (UE addr: %pI4)\n", addr, dit_dev.ue_addr);
	return dit_dev.ue_addr == addr;
}

/* External  API */
bool is_hw_forward_enable(void)
{
#ifdef DIT_FEATURE_MANDATE
	if (dit_dev.enable)
		return offload_config_enabled() ? offload_keeping_bw() : true;
#else
	if (dit_dev.enable
		&& offload_keeping_bw())
		return true;
#endif

	return false;
}
EXPORT_SYMBOL(is_hw_forward_enable);

int hw_forward_add(struct nf_conn *ct)
{
	struct nf_conntrack_tuple_hash *hash = &ct->tuplehash[IP_CT_DIR_REPLY];
	struct nf_conntrack_tuple *reply = &hash->tuple;
	struct nf_conntrack_tuple_hash *org_h = &ct->tuplehash[IP_CT_DIR_ORIGINAL];
	struct nf_conntrack_tuple *orgin = &org_h->tuple;

	int ifnet;

	if (!is_hw_forward_enable())
		return -1;

#ifndef DIT_DEBUG_TEST_UDP
	/* Check TCP Protocol */
	if (reply->dst.protonum != SOL_TCP)
		return -1;
#endif

	ifnet = dit_check_enable(ct->netdev);
	if (ifnet < 0)
		return -1;

	if (!dit_check_ue_addr(reply->dst.u3.ip))
		return -1;

	dit_set_nat_local_addr(ifnet, orgin->src.u3.ip);

	if (dit_forward_add(reply->dst.u.all /* reply_port */, orgin->src.u.all, ifnet) < 0)
		return -1;

	dit_debug("add [%s] %pI4:%hu -> %pI4:%hu\n",
		ct->netdev->name,
		&reply->src.u3.ip,		/* __be32 */
		ntohs(reply->src.u.all),	/* __be16 */
		&reply->dst.u3.ip,
		ntohs(reply->dst.u.all));

	ct->forward_registered = true;

	return 0;
}
EXPORT_SYMBOL(hw_forward_add);

int hw_forward_delete(struct nf_conn *ct)
{
	struct nf_conntrack_tuple_hash *hash = &ct->tuplehash[IP_CT_DIR_REPLY];
	struct nf_conntrack_tuple *reply = &hash->tuple;
	struct nf_conntrack_tuple_hash *org_h = &ct->tuplehash[IP_CT_DIR_ORIGINAL];
	struct nf_conntrack_tuple *orgin = &org_h->tuple;

	int ifnet;

	if (!offload_config_enabled())
		return -1;

	if (!ct->forward_registered)
		return -1;

	/* Check IPv4 */
	if (reply->src.l3num != AF_INET)
		return -1;

#ifndef DIT_DEBUG_TEST_UDP
	/* Check TCP Protocol */
	if (reply->dst.protonum != SOL_TCP)
		return -1;
#endif

	ifnet = dit_check_enable(ct->netdev);
	if (ifnet < 0)
		return -1;

	if (dit_forward_delete(reply->dst.u.all /* reply_port */, orgin->src.u.all, ifnet) < 0)
		return -1;

	dit_debug("delete : %pI4:%hu -> %pI4:%hu\n",
		&reply->src.u3.ip,		/* __be32 */
		ntohs(reply->src.u.all),	/* __be16 */
		&reply->dst.u3.ip,
		ntohs(reply->dst.u.all));

	ct->forward_registered = false;

	return 0;
}
EXPORT_SYMBOL(hw_forward_delete);

void hw_forward_monitor(struct nf_conn *ct)
{
	struct nf_conntrack_tuple_hash *hash = &ct->tuplehash[0];
	struct nf_conntrack_tuple *t = &hash->tuple;
	static int cnt;

	/* Check IPv4 */
	if (t->src.l3num != AF_INET)
		return;

#ifndef DIT_DEBUG_TEST_UDP
	/* Check TCP Protocol */
	if (t->dst.protonum != SOL_TCP)
		return;
#endif

	cnt++;

	/* Increase packet count */
	ct->packet_count++;

	dit_debug("[%d] tuple %pI4:%hu -> %pI4:%hu\n",
		ct->packet_count,
		&t->src.u3.ip,		/* __be32 */
		ntohs(t->src.u.all),	/* __be16 */
		&t->dst.u3.ip,
		ntohs(t->dst.u.all));

	if (ct->packet_count == THRESHOLD_HW_FORWARD)
		hw_forward_add(ct);
}
EXPORT_SYMBOL(hw_forward_monitor);

int hw_forward_enqueue_to_backlog(int DIR, struct sk_buff *skb)
{
	return dit_enqueue_to_backlog(DIR, skb);
}
EXPORT_SYMBOL(hw_forward_enqueue_to_backlog);

int hw_forward_schedule(int type)
{
	if (!dit_dev.enable)
		return -1;

#ifdef DIT_CHECK_PERF
	dit_dev.perf.shed_req++;
#endif

	return dit_schedule(type);
}
EXPORT_SYMBOL(hw_forward_schedule);

#endif

#if DIT_CLAT_PROCESS
/* PLAT prefix of IPv6 addr */
void dit_get_plat_prefix(u32 id, struct in6_addr *paddr)
{
	struct dit_dev_info_t *dit = &dit_dev;
	unsigned long flags;

	spin_lock_irqsave(&dit->lock, flags);

	if (readl(dit->reg_base + DIT_DMA_INIT_DATA) == 0)
		dit_init_hw();

	paddr->s6_addr32[0] = readl(dit->reg_base + DIT_CLAT_TX_PLAT_PREFIX_A0 + id*12);
	paddr->s6_addr32[1] = readl(dit->reg_base + DIT_CLAT_TX_PLAT_PREFIX_A1 + id*12);
	paddr->s6_addr32[2] = readl(dit->reg_base + DIT_CLAT_TX_PLAT_PREFIX_A2 + id*12);

	spin_unlock_irqrestore(&dit->lock, flags);

	dit_info("plat_prfix(%d) address:%08x %08x %08x\n", id,
		paddr->s6_addr32[0], paddr->s6_addr32[1], paddr->s6_addr32[2]);
}

void dit_set_plat_prefix(u32 id, struct in6_addr addr)
{
	struct dit_dev_info_t *dit = &dit_dev;
	unsigned long flags;

	spin_lock_irqsave(&dit->lock, flags);

	if (readl(dit->reg_base + DIT_DMA_INIT_DATA) == 0)
		dit_init_hw();

	writel(addr.s6_addr32[0], dit->reg_base + DIT_CLAT_TX_PLAT_PREFIX_A0 + id*12);
	writel(addr.s6_addr32[1], dit->reg_base + DIT_CLAT_TX_PLAT_PREFIX_A1 + id*12);
	writel(addr.s6_addr32[2], dit->reg_base + DIT_CLAT_TX_PLAT_PREFIX_A2 + id*12);

	spin_unlock_irqrestore(&dit->lock, flags);

	dit_info("plat_prfix(%d) address:%08x %08x %08x\n", id,
		readl(dit->reg_base + DIT_CLAT_TX_PLAT_PREFIX_A0 + id*12),
		readl(dit->reg_base + DIT_CLAT_TX_PLAT_PREFIX_A1 + id*12),
		readl(dit->reg_base + DIT_CLAT_TX_PLAT_PREFIX_A2 + id*12));
}

void dit_del_plat_prefix(u32 id)
{
	struct in6_addr addr = IN6ADDR_ANY_INIT;

	dit_set_plat_prefix(id, addr);
}

/* CLAT src addr of IPv6 addr */
void dit_get_clat_addr(u32 id, struct in6_addr *paddr)
{
	struct dit_dev_info_t *dit = &dit_dev;
	unsigned long flags;

	spin_lock_irqsave(&dit->lock, flags);
	if (readl(dit->reg_base + DIT_DMA_INIT_DATA) == 0)
		dit_init_hw();

	paddr->s6_addr32[0] = readl(dit->reg_base + DIT_CLAT_TX_CLAT_SRC_A0 + id*16);
	paddr->s6_addr32[1] = readl(dit->reg_base + DIT_CLAT_TX_CLAT_SRC_A1 + id*16);
	paddr->s6_addr32[2] = readl(dit->reg_base + DIT_CLAT_TX_CLAT_SRC_A2 + id*16);
	paddr->s6_addr32[3] = readl(dit->reg_base + DIT_CLAT_TX_CLAT_SRC_A3 + id*16);

	spin_unlock_irqrestore(&dit->lock, flags);

	dit_info("plat_prfix(%d) address:%08x %08x %08x %08x\n", id,
		paddr->s6_addr32[0], paddr->s6_addr32[1], paddr->s6_addr32[2], paddr->s6_addr32[3]);
}

void dit_set_clat_addr(u32 id, struct in6_addr addr)
{
	struct dit_dev_info_t *dit = &dit_dev;
	unsigned long flags;

	spin_lock_irqsave(&dit->lock, flags);
	if (readl(dit->reg_base + DIT_DMA_INIT_DATA) == 0)
		dit_init_hw();

	writel(addr.s6_addr32[0], dit->reg_base + DIT_CLAT_TX_CLAT_SRC_A0 + id*16);
	writel(addr.s6_addr32[1], dit->reg_base + DIT_CLAT_TX_CLAT_SRC_A1 + id*16);
	writel(addr.s6_addr32[2], dit->reg_base + DIT_CLAT_TX_CLAT_SRC_A2 + id*16);
	writel(addr.s6_addr32[3], dit->reg_base + DIT_CLAT_TX_CLAT_SRC_A3 + id*16);
	spin_unlock_irqrestore(&dit->lock, flags);

	dit_info("clat filter(%d) address:%08x %08x %08x %08x\n", id,
		readl(dit->reg_base + DIT_CLAT_TX_CLAT_SRC_A0 + id*16),
		readl(dit->reg_base + DIT_CLAT_TX_CLAT_SRC_A1 + id*16),
		readl(dit->reg_base + DIT_CLAT_TX_CLAT_SRC_A2 + id*16),
		readl(dit->reg_base + DIT_CLAT_TX_CLAT_SRC_A3 + id*16));
}

void dit_del_clat_addr(u32 id)
{
	struct in6_addr addr = IN6ADDR_ANY_INIT;

	dit_set_clat_addr(id, addr);
}

/* CLAT filter of IPv4 addr */
void dit_get_v4_filter(u32 id, u32 *paddr)
{
	struct dit_dev_info_t *dit = &dit_dev;
	unsigned long flags;

	spin_lock_irqsave(&dit->lock, flags);
	if (readl(dit->reg_base + DIT_DMA_INIT_DATA) == 0)
		dit_init_hw();

	*paddr = readl(dit->reg_base + DIT_CLAT_TX_FILTER_A + id*4);
	spin_unlock_irqrestore(&dit->lock, flags);
}

void dit_set_v4_filter(u32 id, u32 addr)
{
	struct dit_dev_info_t *dit = &dit_dev;
	unsigned long flags;

	spin_lock_irqsave(&dit->lock, flags);
	if (readl(dit->reg_base + DIT_DMA_INIT_DATA) == 0)
		dit_init_hw();

	writel(addr, dit->reg_base + DIT_CLAT_TX_FILTER_A + id*4);
	spin_unlock_irqrestore(&dit->lock, flags);

	addr = readl(dit->reg_base + DIT_CLAT_TX_FILTER_A + id*4);
	dit_info("[%d] v4addr: %pI4\n", id, &addr);
}

void dit_del_v4_filter(u32 id)
{
	u32 addr = INADDR_ANY;

	dit_set_v4_filter(id, addr);
}
#endif

#ifdef DIT_CHECK_PERF
void __perftest_gen_skb(int DIR, int budget)
{
	int i = 0;
	struct sk_buff *skb;
	struct iphdr *iphdr;
	unsigned int *seq;
	unsigned char *data;
	int len;

	if (DIR == DIT_RX__FORWARD) {
		iphdr = (struct iphdr *)test_Rxpacket;
		data = test_Rxpacket;
		len = test_Rxpkt_sz;
		dit_debug("RX_FORWARD packet dst addr %pI4\n", &iphdr->daddr);
	} else {
		iphdr = (struct iphdr *)test_Txpacket;
		data = test_Txpacket;
		len = test_Txpkt_sz;
		dit_debug("TX_FORWARD packet src addr %pI4\n", &iphdr->saddr);
	}

	while (i++ < budget) {
		if (!dit_dev.perf.ndev) {
			dit_err_limited("testing perf.ndev is NOT set\n");
			return;
		}
		skb = napi_alloc_skb(&dit_dev.napi.backlog_skb, DIT_BUFFER_DSIZE);
		if (!skb) {
			dit_info("fail to gen skb\n");
			return;
		}
		dit_dev.perf.fwd[DIR].pktcounter++;
		if (iphdr->version == 0x6) {
			seq = (unsigned int *)&data[48];
			*seq = htonl(dit_dev.perf.fwd[DIR].pktcounter);
		} else {
			seq = (unsigned int *)&data[28];
			*seq = htonl(dit_dev.perf.fwd[DIR].pktcounter);
		}
		skb->ip_summed = CHECKSUM_UNNECESSARY;
		skb_put_data(skb, iphdr, len);
		skb->dev = dit_dev.perf.ndev;
		if (NET_RX_SUCCESS != hw_forward_enqueue_to_backlog(DIR, skb))
			dit_dev.perf.fwd[DIR].pktcounter--; /* rollback counter */
	}
	hw_forward_schedule(DIT_BACKLOG_SKB);
}

static enum hrtimer_restart dit_perftest_timer_func(struct hrtimer *timer)
{
	struct timespec startTime, endTime;
	struct timespec ts;
	struct dit_stat_perf_t *perf = &dit_dev.perf;
	u64 delay;

	if (perf->test_ticks >= PERF_TEST_DURATION) {
		dit_info("Timer stopped\n");
		return HRTIMER_NORESTART;
	}

	perf->test_ticks++;
	getnstimeofday(&startTime);

	if (perf->test_case == DIT_PERF_TEST_SKB_RXGEN)
		__perftest_gen_skb(DIT_RX__FORWARD, perf->pkts_ms);
	else {
		__perftest_gen_skb(DIT_TX__FORWARD, perf->pkts_ms >> 1);
	}

	getnstimeofday(&endTime);
	ts = timespec_sub(endTime, startTime);

	if (perf->test_ticks <= 10) {
		dit_info("__perftest_gen duration : %ld nsecs\n", ts.tv_nsec);
	}

	if (ts.tv_nsec < NSEC_PER_MSEC)
		delay = NSEC_PER_MSEC - ts.tv_nsec;
	else
		delay = DIT_SCHED_BACKOFF_TIME_NS;

	hrtimer_start(&perf->test_timer, ns_to_ktime(delay), HRTIMER_MODE_REL);

	return HRTIMER_NORESTART;
}

static int dit_perftest_gen_thd_fun(void *arg)
{
	struct timespec startTime, endTime;
	struct timespec ts;

	hrtimer_init(&dit_dev.perf.test_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	dit_dev.perf.test_timer.function = dit_perftest_timer_func;
	dit_dev.perf.test_ticks = 0;

	if (dit_dev.enable) {
		hrtimer_start(&dit_dev.perf.test_timer, ns_to_ktime(NSEC_PER_MSEC), HRTIMER_MODE_REL);

		getnstimeofday(&startTime);

		while (dit_dev.perf.test_ticks < PERF_TEST_DURATION)
			usleep_range(500, 1000);

		getnstimeofday(&endTime);
		ts = timespec_sub(endTime, startTime);
		dit_info("Test duration : %ld.%03ld sec\n", ts.tv_sec, ts.tv_nsec / NSEC_PER_MSEC);
	} else {
		dit_info("Need to init DIT\n");
	}
	return 0;
}

void dit_perftest_gen_thread_ex(int val)
{
	int cpu = 7;

	struct sched_param task_sched_params =	{
		.sched_priority = MAX_RT_PRIO
	};

	task_sched_params.sched_priority = 90;

	dit_dev.perf.worker_task = kthread_create_on_node(dit_perftest_gen_thd_fun,
		NULL, cpu_to_node(cpu), "dit_Test_thd_%d", cpu);
	kthread_bind(dit_dev.perf.worker_task, cpu);

	dit_dev.perf.test_case = val;

	dit_info("create kthread %s %s\n", dit_dev.perf.worker_task->comm,
			(val == DIT_PERF_TEST_SKB_RXGEN) ? "skb" : "desc");

	wake_up_process(dit_dev.perf.worker_task);
}

static void perftest_gen_skb(int DIR)
{
	__perftest_gen_skb(DIR, dit_dev.perf.pkts_ms);
}

#endif

#if DIT_SYSFS
/*
 * sysfs interfaces
 */
static ssize_t dit_enable_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "enable: %d (offload:%d, status=%d)\n", dit_dev.enable,
		offload_config_enabled(), offload_get_status());
}

/* Test purpose */
static ssize_t dit_enable_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int val = 0;
	int err;

	err = kstrtoint(buf, 0, &val);
	if (err)
		return -EINVAL;

	if (val && (dit_init(DIT_INIT_REASON_FAIL) < 0))
		dit_err("dit init failed\n");

	return count;
}

static ssize_t dit_stat_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	ssize_t count = 0;
	struct dit_dev_info_t *dit = &dit_dev;
	struct dit_stat_t *pstat;
#ifdef DIT_CHECK_PERF
	struct dit_stat_perf_t *pperf = &dit->perf;
#endif
	int DIR, dst;

	for (DIR = 0; DIR < DIT_MAX_FORWARD; DIR++) {
		pstat = &dit->stat[DIR];
		count += snprintf(buf+count, 5+1, "[%-2s]\n", DIR ? "TX" : "RX");
		count += snprintf(buf+count, 84+1,
			" %-10s\t%8d\n %-10s\t%8d\n %-10s\t%8d\n",
			"injectpkt", pstat->injectpkt,
			"dit_inpkt", pstat->dit_inpkt,
			"dit_outpkt", pstat->dit_outpkt);
		count += snprintf(buf+count, 63+1,
			" %-10s\t%8d\n %-10s\t%8d\n %-10s\t%8d\n",
			"busy_hw", pstat->err_busy_hw,
			"full_blog", pstat->err_full_bq,
			"full_fwd", pstat->err_full_fq);
		count += snprintf(buf+count, 63+1,
			" %-10s\t%8d\n %-10s\t%8d\n %-10s\t%8d\n",
			"err_pend", pstat->err_pend,
			"kick_cnt", pstat->kick_cnt,
			"kick_rtry", pstat->kick_re);
		count += snprintf(buf+count, 63+1,
			" %-10s\t%8d\n %-10s\t%8d\n %-10s\t%8d\n",
			"fwd_busy", pstat->droppkt_busy,
			"fwd_drop", pstat->droppkt_drop,
			"fwd_cn", pstat->droppkt_cn);
	}

#ifdef DIT_CHECK_PERF
	count += snprintf(buf+count, 8+1, "[%-5s]\n", "SCHED");
	count += snprintf(buf+count, 106+1,
		" %-10s\t%8d\n %-10s\t%8d\n %-10s\t%8d\n %-10s\t%8d\n %-10s\t%8d\n",
		"test_ticks", pperf->test_ticks,
		"shed_req", pperf->shed_req,
		"shed_try", pperf->shed_try,
		"shed_skb", pperf->shed_skb,
		"shed_fwd", pperf->shed_fwd);

	count += snprintf(buf+count, 43+1+1,
		" %-10s\t%8d\n %-10s\t%8d\n\n",
		"skb_poll", pperf->backlog_poll_cnt[DIT_BACKLOG_SKB],
		"fwd_poll", pperf->forward_poll_cnt);

	count += snprintf(buf+count, 1+42+1+1,
		"\n %-10s\t%8d\n %-10s\t%8d\n\n",
		"txq_drop", pperf->txq_drop,
		"txq_inact", pperf->txq_inactive);
#endif

	count += snprintf(buf+count, 1+1+78+1,
		"\n\n%-10s\t %-10s %-10s %-10s %-10s %-10s %-10s\n",
		"RX/TX-dst", "rx_irq", "dst_poll", "dst_pkt",
		"dst_ipv4", "dst_ipv6", "err_x/full");
	for (DIR = 0; DIR < DIT_MAX_FORWARD; DIR++) {
		pstat = &dit->stat[DIR];
		for (dst = 0; dst <= DIT_DST2; dst++) {
			count += snprintf(buf+count, 85+1,
				"  %-2s-dst%1d\t%8d %10d %10d %10d %10d %10d/%-10d\n",
				DIR ? "TX" : "RX", dst, pstat->rx_irq_cnt[dst],
				pstat->dst_poll_cnt[dst], pstat->dst_pkt_cnt[dst],
				pstat->dst_ipv4_cnt[dst], pstat->dst_ipv6_cnt[dst],
				pstat->err_trans[dst], pstat->err_bankfull[dst]);
		}
	}

	return count;
}

static ssize_t dit_table_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	ssize_t count = 0;
	struct dit_table_t rxtab;
	int i;

	count += snprintf(buf + count, 21+1, "%s", "reply/dst--orgin/dst\n");

	if (!dit_dev.enable) {
		dit_err("NOT enabled DIT now\n");
		return count;
	}

	for (i = 0; i < DIT_MAX_TABLE_ENTRY; i++) {
		rxtab.val = readl(dit_dev.reg_base + DIT_NAT_RX_PORT_TABLE_SLOT_0 + PTABLE_IDX(i));
		if (rxtab.rx.enable) {
			struct dit_table_t txtab;

			txtab.val = readl(dit_dev.reg_base + DIT_NAT_TX_PORT_TABLE_SLOT_0
				+ PTABLE_IDX(rxtab.rx.x_port));
			count += snprintf(buf + count, 26+1, "[%4d] %5hu/%1d -- %5hu/%1d\n",
				i, ntohs(txtab.tx.x_port), txtab.tx.dma_sel,
				ntohs(rxtab.rx.x_port), rxtab.rx.dma_sel);
		}
	}

	return count;
}

#ifdef DIT_CHECK_PERF
static ssize_t perftest_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	ssize_t count = 0;

	count += sprintf(buf, " \"0\": test init\n"
						" \"1\": rx test once\n"
						" \"2\": tx test once \n"
						" \"3\": full rx & tx test\n"
						" \"4\": full iperf test\n");

	return count;
}

static ssize_t perftest_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int param1  = 0, param2 = 0;
	u16 *src, *dst;
	__be16 reply_port, org_port;
	struct iphdr *iphdrTx, *iphdrRx;
	struct ipv6hdr *ip6hdrRx;
	int ret;

	if (!dit_dev.enable) {
		dit_err("NOT enabled DIT now\n");
		return count;
	}

	while (!isxdigit(*buf))
		buf++;

	ret = sscanf(buf, "%d %d", &param1, &param2);
	if (ret < 1)
		return -EINVAL;


	switch (param1) {
	case 0: /* add port table */
		dit_info("Test Init param2 = %d\n", param2);

		/* initialize perf stat */
		memset(dit_dev.perf.fwd, 0, sizeof(struct dit_perf_forward_t)*DIT_MAX_FORWARD);

		dit_dev.perf.backlog_poll_cnt[DIT_BACKLOG_SKB] = 0;
		dit_dev.perf.forward_poll_cnt = 0;

		dit_dev.perf.shed_req = 0;
		dit_dev.perf.shed_try = 0;
		dit_dev.perf.shed_skb = 0;
		dit_dev.perf.shed_fwd = 0;

		dit_dev.perf.txq_drop = 0;
		dit_dev.perf.txq_inactive = 0;

		dit_dev.perf.devfwd.fwd_done = 0;

		dit_dev.perf.test_ticks = 0;

		dit_dev.perf.ue_ip.s_addr = htonl(PERF_UE_IP_ADDR);
		dit_dev.perf.host_ip.s_addr = htonl(PERF_PC_IP_ADDR);

		dit_set_nat_local_addr(DIT_IF_RMNET, dit_dev.perf.ue_ip.s_addr);
		dit_set_nat_local_addr(DIT_IF_USB, dit_dev.perf.host_ip.s_addr);

		/* Adding NAT table */
		if (param2 == 3) {
			dit_info("Test CLAT\n");
			test_Rxpacket = clat_tcpRx;
			test_Rxpkt_sz = clat_tcpRxpkt_sz;
			test_Txpacket = clat_tcpTx;
			test_Txpkt_sz = clat_tcpTxpkt_sz;

			ip6hdrRx = (struct ipv6hdr *)test_Rxpacket;
			dst = (u16 *)(test_Rxpacket + 20 + 2);
			iphdrTx = (struct iphdr *)test_Txpacket;
			src = (u16 *)(test_Txpacket + 20);

			if (ip6hdrRx->version != 0x6) {
				dit_err("NOT IPv6 packet\n");
				break;
			}
		} else {
			dit_info("Test NAT\n");
			test_Rxpacket = udpRx;
			test_Rxpkt_sz = udpRxpkt_sz;
			test_Txpacket = udpTx;
			test_Txpkt_sz = udpTxpkt_sz;

			iphdrRx = (struct iphdr *)test_Rxpacket;
			dst = (u16 *)(test_Rxpacket + 20 + 2);
			iphdrTx = (struct iphdr *)test_Txpacket;
			src = (u16 *)(test_Txpacket + 20);

			if (iphdrRx->version != 0x4) {
				dit_err("NOT IPv4 packet\n");
				break;
			}
			dit_info("RX ip ver =%d, %pI4 reply_port=0x%04x, packet size=%d\n",
				iphdrRx->version, &iphdrRx->daddr, *dst, test_Rxpkt_sz);
			dit_info("TX ip ver =%d, %pI4 org_port=0x%04x, packet size=%d\n",
				iphdrTx->version, &iphdrTx->saddr, *src, test_Txpkt_sz);
		}

		if (param2 != 0) { /* if param2 is 0, then no filter setting means bypass */
			writel(0x5, dit_dev.reg_base + DIT_NAT_ZERO_CHK_OFF); /* for TEST UDP only */
			reply_port = (*dst);
			org_port = htons(0x1fb0);

			dit_forward_add(reply_port, org_port, DIT_IF_USB);
		}

		break;

	case 1 ... 5: /* RX / TX */
		if (param2 < PERF_TEST_DIT_BUDGET && 0 < param2)
			dit_dev.perf.pkts_ms = param2;
		else
			dit_dev.perf.pkts_ms = PERF_TEST_DIT_BUDGET;

		dit_info("Test packets %d / ms (param1 %x, param2 %d)\n", dit_dev.perf.pkts_ms, param1, param2);

		switch (param1) {
		case DIT_PERF_TEST_ONCE_RX: /* unit test RX */
			dit_info("Test Once RX\n");
			perftest_gen_skb(DIT_RX__FORWARD);
			break;

		case DIT_PERF_TEST_ONCE_TX: /* unit test TX */
			dit_info("Test Once TX\n");
			perftest_gen_skb(DIT_TX__FORWARD);
			break;

		case DIT_PERF_TEST_NORM: /* Bidirectional */
			dit_info("Test Bidirectional\n");
			dit_perftest_gen_thread_ex(param1);
			break;

		case DIT_PERF_TEST_SKB_RXGEN: /* iperf rx test */
			dit_info("Test iperf\n");
			dit_perftest_gen_thread_ex(param1);
			break;
		}
		break;

	case 6: /* remove port table */
		src = (unsigned short *)(test_Rxpacket + 12);
		dst = (unsigned short *)(test_Rxpacket + 12 + 4);

		reply_port = htons(*dst);
		org_port = htons(0x1fb0);
		dit_forward_delete(reply_port, org_port, DIT_IF_USB);
		break;

	case 7:
#ifdef DIT_DEBUG_PKT
		dit_debug_print_dst(DIT_RX__FORWARD);
#endif
		break;

	case 8: /* CP pktgen + USB test */
		writel(0x5, dit_dev.reg_base + DIT_NAT_ZERO_CHK_OFF); /* for TEST UDP only */
		dit_dev.perf.host_ip.s_addr = htonl(PERF_PC_IP_ADDR);
		dit_set_nat_local_addr(DIT_IF_USB, dit_dev.perf.host_ip.s_addr);
		perftest_offload_change_status(STATE_OFFLOAD_ONLINE);

		reply_port = htons(0x1389);
		org_port = htons(0x1fb0);
		if(param2 == 1)
			dit_forward_add(reply_port, org_port, DIT_IF_USB);
		else
			dit_forward_delete(reply_port, org_port, DIT_IF_USB);

		break;

	case 9:
		debug_print = 1;
		break;

	default:
		break;
	}
	return count;
}

#endif

static struct kobject *dit_kobject;
static struct kobj_attribute dit_enable_attribute = {
	.attr = {.name = "enable", .mode = 0660},
	.show = dit_enable_show,
	.store = dit_enable_store,
};
static struct kobj_attribute dit_stat_attribute = {
	.attr = {.name = "stat", .mode = 0600},
	.show = dit_stat_show,
};
static struct kobj_attribute dit_table_attribute = {
	.attr = {.name = "table", .mode = 0600},
	.show = dit_table_show,
};
#ifdef DIT_CHECK_PERF
static struct kobj_attribute perftest_attribute = {
	.attr = {.name = "perftest", .mode = 0660},
	.show = perftest_show,
	.store = perftest_store,
};
#endif

static struct attribute *dit_attrs[] = {
	&dit_enable_attribute.attr,
	&dit_stat_attribute.attr,
	&dit_table_attribute.attr,
#ifdef DIT_CHECK_PERF
	&perftest_attribute.attr,
#endif
	NULL,
};
ATTRIBUTE_GROUPS(dit);
#endif

#if DIT_NETDEV_EVENT
/* Net Device Notifier */
bool dit_get_ifaddr(struct net_device *ndev, u32 *addr)
{
	struct in_device *in_dev;
	struct in_ifaddr *ifa;

	in_dev = __in_dev_get_rcu(ndev);

	ifa = in_dev->ifa_list;

	for ( ; ifa ; ifa = ifa->ifa_next) {
		dit_info("%s %s\n", ifa->ifa_label, ndev->name);
		if (!(strcmp(ifa->ifa_label, ndev->name)))	{
			dit_info("ifa->ifa_address=%x\n", ifa->ifa_address);
			*addr = ifa->ifa_address;
			return true;
		}
	}

	return false;
}

void dit_setup_upstream_device(struct net_device *ndev)
{
	u32 addr;

	dit_info("%s\n", __func__);

	if (dit_get_ifaddr(ndev, &addr))
		dit_set_nat_local_addr(DIT_IF_RMNET, addr);
	dit_forward_set(DIT_IF_RMNET, ndev, true);

}
void dit_setup_downstream_device(struct net_device *ndev)
{
	dit_info("%s\n", ndev->name);

	if (isLAN0device(ndev)) {
		gether_get_host_addr_u8(ndev, dit_dev.host[0].hostmac);
		dit_set_local_mac(dit_dev.host[0].hostmac);
		memcpy(dit_dev.host[0].ifmac, ndev->dev_addr, ETH_ALEN);
		dit_set_devif_mac(dit_dev.host[0].ifmac);

		dit_info("HOST MAC %pM\n", dit_dev.host[0].hostmac);
		dit_info("DEV MAC %pM\n", dit_dev.host[0].ifmac);

		dit_forward_set(DIT_IF_USB, ndev, true);
#ifdef DIT_CHECK_PERF
		dit_dev.perf.ndev = ndev;
#endif
#ifdef DIT_DEBUG_TEST_UDP
		{
			__be16 reply_port, org_port;

			reply_port = htons(5001);
			org_port = htons(0x1fb0);

			writel(0x5, dit_dev.reg_base + DIT_NAT_ZERO_CHK_OFF); /* for TEST UDP only */

			dit_dev.perf.test_case = DIT_PERF_TEST_SKB_RXGEN;
			dit_dev.perf.host_ip.s_addr = htonl(PERF_PC_IP_ADDR);
			dit_set_nat_local_addr(DIT_IF_USB, dit_dev.perf.host_ip.s_addr);
			dit_forward_add(reply_port, org_port, DIT_IF_USB);
		}
#endif
#ifdef DIT_FEATURE_USE_WIFI
	} else if (isLAN1device(ndev)) {
		wlan_get_host_addr(ndev, hostmac);
		dit_set_local_mac(DIT_IF_WLAN, hostmac);

		memcpy(ifmac, ndev->dev_addr, ETH_ALEN);
		dit_set_devif_mac(DIT_IF_WLAN, ifmac);

		dit_forward_set(DIT_IF_WLAN, ndev, true);
#endif
	}

}

void dit_clear_upstream_device(struct net_device *ndev)
{
	dit_info("%s\n", ndev->name);

	dit_clear_nat_local_addr(DIT_IF_RMNET);
	dit_forward_set(DIT_IF_RMNET, ndev, false);
}

void dit_clear_downstream_device(struct net_device *ndev)
{
	dit_info("%s ++\n", ndev->name);

	if (isLAN0device(ndev)) {
		dit_deinit(DIT_DEINIT_REASON_RUNTIME);
		dit_clear_nat_local_addr(DIT_IF_USB);
		dit_clear_local_mac();
		dit_clear_devif_mac();
		dit_forward_set(DIT_IF_USB, ndev, false);
#ifdef DIT_CHECK_PERF
		dit_dev.perf.ndev = NULL;
#endif
#ifdef DIT_FEATURE_USE_WIFI
	} else if (isLAN1device(ndev)) {
		dit_clear_nat_local_addr(DIT_IF_WLAN);
		dit_clear_local_mac(DIT_IF_WLAN);
		dit_clear_devif_mac();
		dit_forward_set(DIT_IF_WLAN, ndev, false);
#ifdef DIT_CHECK_PERF
		dit_dev.perf.ndev = NULL;
#endif
#endif
	}
	dit_info("--\n");
}
#endif

static int dit_alloc_dst_buffers(void)
{
	int DIR, dst, len, i;

	/* pre-allocate dst buffers for DST0 ~ DST2 */
	for (DIR = 0; DIR < DIT_MAX_FORWARD; DIR++) {
		struct dit_handle_t *h = &dit_dev.handle[DIR];

		len = sizeof(struct sk_buff *) * h->num_desc;
		for (dst = DIT_DST0; dst <= DIT_DST2; dst++) {
			h->skbarray[dst] = kzalloc(len, GFP_KERNEL);
			if (!h->skbarray[dst]) {
				dit_err("failed to alloc skbarray\n");
				return -ENOMEM;
			}

			for (i = 0; i < DIT_MAX_DESC ; i++)
				if (!dit_prealloc_buffer(h, dst, i, "init")) {
					dit_err("failed to prealloc\n");
					return -ENOMEM;
				}
		}
	}

	return 0;
}

static int dit_free_dst_buffers(void)
{
	int DIR, dst;

	for (DIR = 0; DIR < DIT_MAX_FORWARD; DIR++) {
		struct dit_handle_t *h = &dit_dev.handle[DIR];

		for (dst = DIT_DST0; dst <= DIT_DST2; dst++) {
			struct sk_buff **pskbuffs = h->skbarray[dst];

			dit_free_buffer(h, dst);
				kfree(pskbuffs);
		}
	}

	return 0;
}

int dit_init(int reason)
{
	int irqch, DIR, dst, dsize, i, ret;
	unsigned long flags;

	dit_info("++");

	dit_dev.offload = offload_init_ctrl();

#ifdef DIT_FEATURE_MANDATE
	if (reason == DIT_INIT_REASON_RUNTIME) {
		dit_info("DIT_FEATURE_MANDATE\n");
		goto init_already;
	}
#endif

	if (dit_dev.enable) {
		dit_info("already initialized\n");
		goto init_already;
	}

	init_dummy_netdev(&dit_dev.napi.dummy_ndev);

	netif_napi_add(&dit_dev.napi.dummy_ndev, &dit_dev.napi.backlog_skb, dit_backlog_skb_poll, DIT_BUDGET);
	napi_enable(&dit_dev.napi.backlog_skb);

	netif_napi_add(&dit_dev.napi.dummy_ndev, &dit_dev.napi.forward, dit_forward_poll, DIT_BUDGET);
	napi_enable(&dit_dev.napi.forward);

	netif_napi_add(&dit_dev.napi.dummy_ndev, &dit_dev.napi.forward_dst[0], dit_dst0_poll, DIT_BUDGET);
	napi_enable(&dit_dev.napi.forward_dst[0]);

	netif_napi_add(&dit_dev.napi.dummy_ndev, &dit_dev.napi.forward_dst[1], dit_dst1_poll, DIT_BUDGET);
	napi_enable(&dit_dev.napi.forward_dst[1]);

	netif_napi_add(&dit_dev.napi.dummy_ndev, &dit_dev.napi.forward_dst[2], dit_dst2_poll, DIT_BUDGET);
	napi_enable(&dit_dev.napi.forward_dst[2]);

	netif_napi_add(&dit_dev.napi.dummy_ndev, &dit_dev.napi.forward_dst[3], dit_tx_dst_poll, DIT_BUDGET);
	napi_enable(&dit_dev.napi.forward_dst[3]);

	if (!dit_dev.reg_base) {
		dit_err("devm_ioremap_resource error\n");
		ret = -EACCES;
		goto init_error;
	}

	/* CREATE DESC and allocation */
	for (DIR = 0; DIR < DIT_MAX_FORWARD; DIR++) {
		struct dit_handle_t *h = &dit_dev.handle[DIR];

		/* init backlog for RX and TX */
		skb_queue_head_init(&h->backlog_q);
		skb_queue_head_init(&h->trash_q);
		skb_queue_head_init(&h->forward_q);

		/* CREATE DESC and allocation */
		dsize = sizeof(struct dit_desc_t) * h->num_desc;

		dit_debug("sizeof(dit_desc_t)=%zd, num_desc=%d\n", sizeof(struct dit_desc_t), h->num_desc);
		for (dst = 0; dst < DIT_MAX_DST; dst++) {
			struct dit_desc_t *pdesc;

#ifdef DIT_IOCC
			h->desc[dst] = kzalloc(dsize, GFP_KERNEL);
			dit_debug("[%d][%d] desc dma allocation (0x%llx)\n", DIR, dst, h->desc[dst]);
#else
			h->desc[dst] = dma_zalloc_coherent(&dit_dev.pdev->dev, dsize, &h->paddr[dst], GFP_KERNEL);
			dit_debug("[%d][%d] desc dma allocation (0x%llx)\n", DIR, dst, h->paddr[dst]);
#endif
			if (!h->desc[dst]) {
				dit_err("dma_zalloc_coherent() error:desc\n");
				ret = -ENOMEM;
				goto init_error;
			}

			pdesc = h->desc[dst];
			pdesc[DIT_MAX_DESC-1].dst.control = (0x1 << DIT_DESC_C_RINGEND);

			for (i = 0; i < DIT_MAX_DESC; i += MAX_UNITS_IN_BANK)
				pdesc[i+1].dst.status = 0x00;

			h->w_key[dst] = BANK_START_INDEX(0); /* it goes '1' at first DIT request */
			h->r_key[dst] = BANK_START_INDEX(1);
		}
	}

	if (dit_alloc_dst_buffers() < 0) {
		ret = -ENOMEM;
		goto init_error;
	}

	dit_init_hw();

	for (irqch = 0; irqch < NUM_INTREQ_DIT; irqch++) {
		dit_debug("enable_irq[%d]: num = %d\n", irqch, dit_dev.irq[irqch].num);
		dit_enable_irq(&dit_dev.irq[irqch]);
	}

	for (i = 0; i < DIT_MAX_HOST; i++)
		dit_dev.host[i].enabled = false;

	hrtimer_init(&dit_dev.sched_skb_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	dit_dev.sched_skb_timer.function = dit_sched_skb_func;

	hrtimer_init(&dit_dev.sched_fwd_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	dit_dev.sched_fwd_timer.function = dit_sched_fwd_func;

#ifdef DIT_FEATURE_USE_WIFI
	dit_dev.last_host = 0;
#endif
	memset(&dit_dev.stat[0], 0, sizeof(dit_dev.stat)*2);

#ifdef DIT_CHECK_PERF
	memset(&dit_dev.perf, 0, sizeof(dit_dev.perf));
#endif

	spin_lock_irqsave(&dit_dev.lock, flags);
	dit_dev.enable = 1;
	spin_unlock_irqrestore(&dit_dev.lock, flags);

init_already:

	dit_info("--");

	return 0;

init_error:

	dit_info("--");

	dit_deinit(DIT_DEINIT_REASON_FAIL);

	return ret;
}

int dit_deinit(int reason)
{
	int irqch;
	int DIR, dst, dsize;
	unsigned long flags;
	int status;

	dit_info("++");

	offload_reset();

	spin_lock_irqsave(&dit_dev.lock, flags);
	status = readl(dit_dev.reg_base + DIT_STATUS);
	dit_info("status (0x%08x)\n", status);
	while (status & (DIT_TX_STATUS_MASK | DIT_RX_STATUS_MASK)) {
		status = readl(dit_dev.reg_base + DIT_STATUS);
		dit_info("status (0x%08x)\n", status);
		udelay(1);
	}
	dit_init_hw_port();
	spin_unlock_irqrestore(&dit_dev.lock, flags);

#ifdef DIT_FEATURE_MANDATE
	if (reason == DIT_DEINIT_REASON_RUNTIME) {
		dit_info("DIT_FEATURE_MANDATE\n");
		goto exit;
	}
#endif

	if (!dit_dev.enable) {
		dit_info("already deinited\n");
		goto exit;
	}

	spin_lock_irqsave(&dit_dev.lock, flags);
	dit_dev.enable = 0;
	spin_unlock_irqrestore(&dit_dev.lock, flags);

	mdelay(10);

	for (irqch = 0; irqch < NUM_INTREQ_DIT; irqch++) {
		dit_debug("disable_irq[%d]: num = %d\n", irqch, dit_dev.irq[irqch].num);
		dit_disable_irq(&dit_dev.irq[irqch]);
	}

	napi_disable(&dit_dev.napi.backlog_skb);
	netif_napi_del(&dit_dev.napi.backlog_skb);

	napi_disable(&dit_dev.napi.forward);
	netif_napi_del(&dit_dev.napi.forward);

	for (dst = 0; dst < DIT_MAX_DST; dst++) { /* including tx_dst */
		napi_disable(&dit_dev.napi.forward_dst[dst]);
		netif_napi_del(&dit_dev.napi.forward_dst[dst]);
	}

	for (DIR = 0; DIR < DIT_MAX_FORWARD; DIR++) {
		struct dit_handle_t *h = &dit_dev.handle[DIR];

		skb_queue_purge(&h->backlog_q);
		skb_queue_purge(&h->trash_q);
		skb_queue_purge(&h->forward_q);

		for (dst = 0; dst < DIT_MAX_DST; dst++)
			if (h->desc[dst]) {
				dsize = sizeof(struct dit_desc_t) * h->num_desc;
#ifdef DIT_IOCC
				kfree(h->desc[dst]);
#else
				dma_free_coherent(&dit_dev.pdev->dev, dsize, h->desc[dst], h->paddr[dst]);
#endif
			}
	}

	dit_free_dst_buffers();

exit:
	dit_info("--");

	return 0;
}

static int dummy_net_open(struct net_device *ndev)
{
	return -EINVAL;
}
static const struct net_device_ops dummy_net_ops = {
	.ndo_open = dummy_net_open,
};

static struct net_device *dit_create_net_device(void)
{
	struct net_device *ndev;
	int ret;

	ndev = alloc_netdev(0, NETDEV_DIT_DUMMY,
			NET_NAME_UNKNOWN, ether_setup);
	if (!ndev) {
		dit_info("%s: ERR! alloc_netdev fail\n", NETDEV_DIT_DUMMY);
		return NULL;
	}

	ndev->netdev_ops = &dummy_net_ops;

	ret = register_netdev(ndev);
	if (ret) {
		dit_info("%s: ERR! register_netdev fail\n", NETDEV_DIT_DUMMY);
		free_netdev(ndev);
	}

	return ndev;
}

static int exynos_dit_probe(struct platform_device *pdev)
{
	int i, err;
	struct resource *res;
	struct device_node *np = pdev->dev.of_node;
	u32 affinity;

	dit_info("++");

	/* get resources */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "dit");
	dit_dev.reg_base = devm_ioremap_resource(&pdev->dev, res);
	if (!dit_dev.reg_base) {
		dit_err("devm_ioremap_resource error\n");
		return -1;
	}
	dit_info("base address : 0x%llX\n", res->start);

	/* IO coherency : DIT_SHARABILITY_CTRL0 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sysreg_bus");
	dit_dev.bus_base = devm_ioremap_resource(&pdev->dev, res);
	if (!dit_dev.bus_base) {
		dit_err("busc devm_ioremap_resource error\n");
		goto probe_error;
	}

	dit_dt_read_u32(np, "sharability_offset", dit_dev.sharability_offset);
	dit_dt_read_u32(np, "sharability_value", dit_dev.sharability_value);

	dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(36));
	dit_dev.pdev = pdev;

	spin_lock_init(&dit_dev.lock);

	/* REGISTER interrupt handlers */
	for (i = 0; i < NUM_INTREQ_DIT; i++) {
		struct dit_irq_t *irq = &dit_dev.irq[i];

		irq->num = platform_get_irq_byname(pdev, irq->name);
		err = request_irq(irq->num, irq->isr, 0, irq->name, &irq->dst);
		if (err) {
			dit_err("Can't request IRQ - irqhandler[%d]\n", i);
			goto probe_error;
		}
		irq->registered = 1;
		irq->active = 1;
		spin_lock_init(&irq->lock);

		/* irq affinity */
		strncpy(dit_dt_item, irq->name, DIT_MAX_NAME_LEN/2);
		dit_dt_item[DIT_MAX_NAME_LEN/2] = '\0';
		strcat(dit_dt_item, "_irq_affinity");

		dit_dt_read_u32(np, dit_dt_item, affinity);
		err = irq_set_affinity(irq->num, cpumask_of(affinity));
		if (err)
			dit_err("irq_set_affinity error %d\n", err);

		dit_disable_irq(irq);
		dit_info("irqhandler[%d]: num = %d\n", i, irq->num);
	}

	dit_kobject = kobject_create_and_add(KOBJ_DIT, kernel_kobj);
	if (!dit_kobject)
		dit_err("%s: done ---\n", KOBJ_DIT);

	if (sysfs_create_groups(dit_kobject, dit_groups))
		dit_err("failed to create clat groups node\n");

	offload_initialize();

	dit_dev.pdev = pdev;

	dit_dev.dummy_ndev = dit_create_net_device();

#ifdef DIT_FEATURE_MANDATE
	if (dit_init(DIT_INIT_REASON_FAIL) < 0)
		dit_err("dit_init failed\n");
#endif

	dit_info("--\n");

	return 0;

probe_error:

	if (dit_dev.bus_base)
		devm_iounmap(&pdev->dev, dit_dev.bus_base);
	if (dit_dev.reg_base)
		devm_iounmap(&pdev->dev, dit_dev.reg_base);

	for (i = 0; i < NUM_INTREQ_DIT; i++) {
		struct dit_irq_t *irq = &dit_dev.irq[i];

		dit_free_irq(irq);
	}

	dit_info("--\n");

	return -1;
}

static const struct of_device_id of_exynos_dit_match[] = {
	{ .compatible = "samsung,exynos-dit", },
	{ },
};

static struct platform_driver exynos_dit_driver = {
	.driver = {
		.name = "exynos-dit",
		.owner = THIS_MODULE,
		.of_match_table = of_exynos_dit_match,
	},
	.probe		= exynos_dit_probe,
};

module_platform_driver(exynos_dit_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Samsung DIT Driver");
