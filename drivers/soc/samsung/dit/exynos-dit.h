/*
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Header for EXYNOS DIT(Direct IP Translator) Driver support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_DIT_H
#define __EXYNOS_DIT_H

#include <linux/of.h>
#include <linux/interrupt.h>

#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <uapi/linux/in.h>
#include <uapi/linux/if_ether.h>
#include "exynos-dit-offload.h"

#define DIT_NETDEV_EVENT	1
#define DIT_SYSFS			1
#define DIT_NETFILTER_API	1
#define DIT_PRE_PROCESS		1
#define DIT_POST_PROCESS	1
#define DIT_CLAT_PROCESS	1

//#define DIT_FEATURE_USE_WIFI
//#define DIT_FEATURE_MANDATE
#define DIT_IOCC

#define DIT_CHECK_PERF

//#define DIT_DEBUG
//#define DIT_DEBUG_LOW
//#define DIT_DEBUG_TEST_UDP
//#define DIT_DEBUG_PKT
#define DIT_DEBUG_PANIC

#define KOBJ_DIT "dit"
#define NETDEV_DIT_DUMMY "dit0"

/* HW dependent */
#define DIT_MAX_TABLE_ENTRY	2048
#define NUM_INTREQ_DIT	5
#define DIT_MAX_HOST	1 /* Only use for USB in DIT 2.0 */

#define DIT_MAX_NAME_LEN	64

/* Bank */
#define DIT_DESC_BITS	12
#define UNIT_ID_BITS		10
#define DIT_MAX_DESC		4096	/* (1 << DIT_DESC_BITS) */

#define MAX_BANK_ID			4		/* (1 << (DIT_DESC_BITS - UNIT_ID_BITS)) */
#define MAX_UNITS_IN_BANK	1024	/* (1 << UNIT_ID_BITS) */
#define DIT_BUFFER_DSIZE	2000

#define UNITS_IN_BANK(bank_key)		((bank_key) & 0x3ff) /* 0x3ff: (MAX_UNITS_IN_BANK - 1)) */
#define BANK_ID(bank_key)		((bank_key) >> UNIT_ID_BITS)
#define BANK_START_INDEX(bank_key)	((bank_key) & 0xc00) /* 0xc00: ~0x3ff */
#define DIT_NEXT_BANK	(-1)

/* Scheduling */
#define DIT_BUDGET			762		/* (MAX_UNITS_IN_BANK * (3/4) - 6) -- '6' MAX padding */
#define DIT_MAX_BACKLOG		10000
#define DIT_SCHED_BACKOFF_TIME_NS	 (100000) /* 100 us */

#define THRESHOLD_HW_FORWARD	50  /* Threshold for hw_forward */

/* Performance Test */
#define MAX_PERF_TEST_PACKET_CNT	(548000)
#define PERF_TEST_DIT_BUDGET	(548)
#define PERF_TEST_DURATION	10000	/* 10 sec */

/* structures */
struct dit_desc_t {
	union {
	struct {
	u64 saddr:36;
	u64 _resvd_1:28;

	u64 lengh:16;
	u64 _resvd_2:32;
	u64 control:8;
	u64 status:8;
	} __packed src;

	struct {
	u64 daddr:36;
	u64 _resvd_1:28;

	u64 lengh:16;
	u64 orgport:16;
	u64 xranport:16;
	u64 control:8;
	u64 status:8;
	} __packed dst;
	u64 val[2];
	};
};

struct dit_rx_table_t {
	u32 enable:1;
	u32 o_port:8;
	u32 x_port:16;
	u32 addr_sel:4;
	u32 dma_sel:2;
	u32 reserved:1;
};

struct dit_tx_table_t {
	u32 enable:1;
	u32 o_port:8;
	u32 x_port:16;
	u32 dma_sel:2;
	u32 reserved:5;
};

struct dit_table_t {
	union {
		struct dit_rx_table_t rx;
		struct dit_tx_table_t tx;
		u32 val;
	};
};

enum {
	DIT_RX__FORWARD,
	DIT_TX__FORWARD,
	DIT_MAX_FORWARD
};

enum {
	DIT_BACKLOG_SKB,	/* backlog skb queue */
	DIT_BACKLOG_FORWARD,
	DIT_MAX_BACKLOG_TYPE
};

enum {
	DIT_PERF_TEST_INIT,
	DIT_PERF_TEST_ONCE_RX,
	DIT_PERF_TEST_ONCE_TX,
	DIT_PERF_TEST_NORM,
	DIT_PERF_TEST_SKB_RXGEN, /* iperf packet gen */
};

enum {
	DIT_DST0,	/* RX: AP  , TX: AP */
	DIT_DST1,	/* RX: WiFi, TX: CP */
	DIT_DST2,	/* RX: USB , TX: .  */
	DIT_SRC,
	DIT_MAX_DST
};

enum {
	DIT_IF_USB,
	DIT_IF_WLAN,
	DIT_IF_RMNET,
	DIT_IF_MAX
};

/* DIT irq info */
struct dit_irq_t {
	char name[DIT_MAX_NAME_LEN];
	int num;
	irq_handler_t isr;
	int dst;
	bool registered;

	spinlock_t lock;
	bool active;
};

/* Statistics */
struct dit_stat_t {
	u32 injectpkt;
	u32 dit_inpkt;
	u32 dit_outpkt;
	u32 padpkt;
	u32 droppkt_busy;
	u32 droppkt_drop;
	u32 droppkt_cn;

	u32 kick_cnt;
	u32 kick_re;
	u32 rx_irq_cnt[DIT_MAX_DST];
	u32 dst_poll_cnt[DIT_MAX_DST];
	u32 dst_pkt_cnt[DIT_MAX_DST];
	u32 dst_ipv4_cnt[DIT_MAX_DST];
	u32 dst_ipv6_cnt[DIT_MAX_DST];

	u32 err_trans[DIT_MAX_DST];
	u32 err_bankfull[DIT_MAX_DST];
	u32 err_nomem;
	u32 err_busy_hw;	/* DIT HW */
	u32 err_full_bq;	/* backlog_q */
	u32 err_full_fq;	/* forward_q */
	u32 err_pend;
};

struct dit_perf_forward_t {
	struct timespec g_startT;	/* Time at test start */
	struct timespec g_endT;		/* Time at test end */
	struct timespec startTimeBusy;
	struct timespec endTimeBusy;

	u32 inpkt;
	unsigned int pktcounter;
	u32 fwd_done;
	bool busy_checking;
};

struct dit_stat_perf_t {
	int test_case; /* TEST TYPE */
	int pkts_ms; /* packets  per ms */

	u32 backlog_poll_cnt[DIT_MAX_BACKLOG_TYPE];
	u32 forward_poll_cnt;

	struct net_device *ndev;
	struct in_addr ue_ip; /* UE device IP addr */
	struct in_addr host_ip; /* Host device IP addr */

	u32 shed_req;	/* CP interface requested */
	u32 shed_try;
	u32 shed_skb;
	u32 shed_fwd;
	u32 txq_drop;
	u32 txq_inactive;

	struct task_struct *worker_task;

	struct hrtimer test_timer;
	int test_ticks;

	struct dit_perf_forward_t fwd[DIT_MAX_FORWARD];
	struct dit_perf_forward_t devfwd;
};

struct dit_ring_addr_t {
	u16 high;
	u16 low;
};

struct dit_handle_t {
	struct sk_buff_head backlog_q;
	struct sk_buff_head trash_q;
	struct sk_buff_head forward_q;
	int num_desc;
	int w_key[DIT_MAX_DST];
	int r_key[DIT_MAX_DST];
	int tx_pending; /* special to TX */
	struct dit_desc_t *desc[DIT_MAX_DST]; /* {DST0, DST1, DST2, SRC} */
#ifndef DIT_IOCC
	dma_addr_t paddr[DIT_MAX_DST];
#endif
	struct dit_ring_addr_t ring[DIT_MAX_DST];
	struct dit_ring_addr_t natdesc[DIT_MAX_DST];
	struct sk_buff **skbarray[DIT_DST2+1]; /* {DST0, DST1, DST2} */
};

struct dit_host_t {
	int id;	/* TBD: Local Address selection */
	bool enabled;
	u32 addr;
	char ifmac[ETH_ALEN];
	char hostmac[ETH_ALEN];
	struct net_device *ndev;
};

struct dit_ifdev_t {
	struct net_device *ndev;
	struct netdev_queue *txq;
	bool  enabled;
};

struct dit_napi_t {
	struct net_device dummy_ndev;
	struct napi_struct backlog_skb;
	struct napi_struct forward;
	struct napi_struct forward_dst[DIT_MAX_DST];
};

/* DIT device */
struct dit_dev_info_t {
	void __iomem *reg_base;
	void __iomem *bus_base;
	u32	sharability_offset;
	u32	sharability_value;

	struct platform_device *pdev;
	struct net_device *dummy_ndev;

	struct dit_irq_t irq[NUM_INTREQ_DIT];

	spinlock_t lock;

	/* RX/TX IN handling queue/work */
	struct dit_handle_t handle[DIT_MAX_FORWARD];

	int enable;

	/* bridged interface info */
	struct dit_ifdev_t ifdev[DIT_IF_MAX];

	int if2Dst[DIT_MAX_FORWARD][DIT_IF_MAX];
	int Dst2if[DIT_MAX_FORWARD][DIT_MAX_DST];

#ifdef DIT_FEATURE_USE_WIFI
	int last_host;
#endif
	struct dit_host_t host[DIT_MAX_HOST];
	u32 ue_addr; /* UE IP addr */

	struct dit_napi_t napi;

	struct hrtimer sched_skb_timer;
	struct hrtimer sched_fwd_timer;

	struct dit_stat_t stat[DIT_MAX_FORWARD];
#ifdef DIT_CHECK_PERF
	struct dit_stat_perf_t perf;
#endif

	/* offload interface */
	struct dit_offload_ctl_t *offload;
};

struct dit_priv_t {
	struct net_device *ndev;
	u8 status; /* csum status : [4]IGNR, [3]IPCSF, [2] TCPCF */
} __packed;

/* LOG print */
#define LOG_TAG_DIT	"dit: "

#define dit_err(fmt, ...) \
	pr_err(LOG_TAG_DIT "%s: " pr_fmt(fmt), __func__, ##__VA_ARGS__)
#define dit_err_limited(fmt, ...) \
	printk_ratelimited(KERN_ERR LOG_TAG_DIT "%s: " pr_fmt(fmt), __func__, ##__VA_ARGS__)
#define dit_info(fmt, ...) \
	pr_info(LOG_TAG_DIT "%s: " pr_fmt(fmt), __func__, ##__VA_ARGS__)
#ifdef DIT_DEBUG
#define dit_debug(fmt, ...) dit_info(fmt, ##__VA_ARGS__)
#ifdef DIT_DEBUG_LOW
#define dit_debug_low(fmt, ...)	dit_info(fmt, ##__VA_ARGS__)
#else
#define dit_debug_low(fmt, ...)
#endif
#else
#define dit_debug(fmt, ...)
#define dit_debug_low(fmt, ...)
#endif

/* function macro definition */
#define DIT_dts_idx(entry)	DIT_dts_idx ## entry
#define dit_dt_read_u32(np, prop, dest) \
	do { \
		u32 val; \
		if (of_property_read_u32(np, prop, &val)) { \
			dit_err("%s is not defined\n", prop); \
		} \
		dest = val; \
	} while (0)

/* DIT Register Map */
#define DIT_SW_COMMAND	0x0000
#define DIT_CLK_GT_OFF	0x0004
#define DIT_DMA_INIT_DATA	0x0008
#define DIT_TX_DESC_CTRL_SRC	0x000C
#define DIT_TX_DESC_CTRL_DST	0x0010
#define DIT_TX_HEAD_CTRL	0x0014
#define DIT_TX_MOD_HD_CTRL	0x0018
#define DIT_TX_PKT_CTRL	0x001C
#define DIT_TX_CHKSUM_CTRL	0x0020
#define DIT_RX_DESC_CTRL_SRC	0x0024
#define DIT_RX_DESC_CTRL_DST	0x0028
#define DIT_RX_HEAD_CTRL	0x002C
#define DIT_RX_MOD_HD_CTRL	0x0030
#define DIT_RX_PKT_CTRL	0x0034
#define DIT_RX_CHKSUM_CTRL	0x0038
#define DIT_DMA_CHKSUM_OFF	0x003C
#define DIT_SIZE_UPDATE_OFF	0x0040

#define DIT_TX_RING_START_ADDR_0_SRC	0x0044
#define DIT_TX_RING_START_ADDR_1_SRC	0x0048
#define DIT_TX_RING_START_ADDR_0_DST0	0x004C
#define DIT_TX_RING_START_ADDR_1_DST0	0x0050
#define DIT_TX_RING_START_ADDR_0_DST1	0x0054
#define DIT_TX_RING_START_ADDR_1_DST1	0x0058
#define DIT_TX_RING_START_ADDR_0_DST2	0x005C
#define DIT_TX_RING_START_ADDR_1_DST2	0x0060

#define DIT_RX_RING_START_ADDR_0_SRC	0x0064
#define DIT_RX_RING_START_ADDR_1_SRC	0x0068
#define DIT_RX_RING_START_ADDR_0_DST0	0x006C
#define DIT_RX_RING_START_ADDR_1_DST0	0x0070
#define DIT_RX_RING_START_ADDR_0_DST1	0x0074
#define DIT_RX_RING_START_ADDR_1_DST1	0x0078
#define DIT_RX_RING_START_ADDR_0_DST2	0x007C
#define DIT_RX_RING_START_ADDR_1_DST2	0x0080

#define DIT_INT_ENABLE	0x0084
#define DIT_INT_MASK	0x0088
#define DIT_INT_PENDING	0x008C
#define DIT_STATUS	0x0090
#define DIT_BUS_ERROR	0x0094
#define DIT_CACHE_AID_INFO_0	0x1000
#define DIT_CACHE_AID_INFO_1	0x1004
#define DIT_CACHE_AID_INFO_2	0x1008
#define DIT_CACHE_AID_INFO_3	0x100C

#define DIT_CLAT_TX_FILTER_A	0x2000
#define DIT_CLAT_TX_FILTER_B	0x2004
#define DIT_CLAT_TX_FILTER_C	0x2008
#define DIT_CLAT_TX_FILTER_D	0x200C
#define DIT_CLAT_TX_FILTER_E	0x2010
#define DIT_CLAT_TX_FILTER_F	0x2014
#define DIT_CLAT_TX_FILTER_G	0x2018
#define DIT_CLAT_TX_FILTER_H	0x201C
#define DIT_CLAT_TX_PLAT_PREFIX_A0	0x2020
#define DIT_CLAT_TX_PLAT_PREFIX_A1	0x2024
#define DIT_CLAT_TX_PLAT_PREFIX_A2	0x2028
#define DIT_CLAT_TX_PLAT_PREFIX_B0	0x202C
#define DIT_CLAT_TX_PLAT_PREFIX_B1	0x2030
#define DIT_CLAT_TX_PLAT_PREFIX_B2	0x2034
#define DIT_CLAT_TX_PLAT_PREFIX_C0	0x2038
#define DIT_CLAT_TX_PLAT_PREFIX_C1	0x203C
#define DIT_CLAT_TX_PLAT_PREFIX_C2	0x2040
#define DIT_CLAT_TX_PLAT_PREFIX_D0	0x2044
#define DIT_CLAT_TX_PLAT_PREFIX_D1	0x2048
#define DIT_CLAT_TX_PLAT_PREFIX_D2	0x204C
#define DIT_CLAT_TX_PLAT_PREFIX_E0	0x2050
#define DIT_CLAT_TX_PLAT_PREFIX_E1	0x2054
#define DIT_CLAT_TX_PLAT_PREFIX_E2	0x2058
#define DIT_CLAT_TX_PLAT_PREFIX_F0	0x205C
#define DIT_CLAT_TX_PLAT_PREFIX_F1	0x2060
#define DIT_CLAT_TX_PLAT_PREFIX_F2	0x2064
#define DIT_CLAT_TX_PLAT_PREFIX_G0	0x2068
#define DIT_CLAT_TX_PLAT_PREFIX_G1	0x206C
#define DIT_CLAT_TX_PLAT_PREFIX_G2	0x2070
#define DIT_CLAT_TX_PLAT_PREFIX_H0	0x2074
#define DIT_CLAT_TX_PLAT_PREFIX_H1	0x2078
#define DIT_CLAT_TX_PLAT_PREFIX_H2	0x207C
#define DIT_CLAT_TX_CLAT_SRC_A0	0x2080
#define DIT_CLAT_TX_CLAT_SRC_A1	0x2084
#define DIT_CLAT_TX_CLAT_SRC_A2	0x2088
#define DIT_CLAT_TX_CLAT_SRC_A3	0x208C
#define DIT_CLAT_TX_CLAT_SRC_B0	0x2090
#define DIT_CLAT_TX_CLAT_SRC_B1	0x2094
#define DIT_CLAT_TX_CLAT_SRC_B2	0x2098
#define DIT_CLAT_TX_CLAT_SRC_B3	0x209C
#define DIT_CLAT_TX_CLAT_SRC_C0	0x20A0
#define DIT_CLAT_TX_CLAT_SRC_C1	0x20A4
#define DIT_CLAT_TX_CLAT_SRC_C2	0x20A8
#define DIT_CLAT_TX_CLAT_SRC_C3	0x20AC
#define DIT_CLAT_TX_CLAT_SRC_D0	0x20B0
#define DIT_CLAT_TX_CLAT_SRC_D1	0x20B4
#define DIT_CLAT_TX_CLAT_SRC_D2	0x20B8
#define DIT_CLAT_TX_CLAT_SRC_D3	0x20BC
#define DIT_CLAT_TX_CLAT_SRC_E0	0x20C0
#define DIT_CLAT_TX_CLAT_SRC_E1	0x20C4
#define DIT_CLAT_TX_CLAT_SRC_E2	0x20C8
#define DIT_CLAT_TX_CLAT_SRC_E3	0x20CC
#define DIT_CLAT_TX_CLAT_SRC_F0	0x20D0
#define DIT_CLAT_TX_CLAT_SRC_F1	0x20D4
#define DIT_CLAT_TX_CLAT_SRC_F2	0x20D8
#define DIT_CLAT_TX_CLAT_SRC_F3	0x20DC
#define DIT_CLAT_TX_CLAT_SRC_G0	0x20E0
#define DIT_CLAT_TX_CLAT_SRC_G1	0x20E4
#define DIT_CLAT_TX_CLAT_SRC_G2	0x20E8
#define DIT_CLAT_TX_CLAT_SRC_G3	0x20EC
#define DIT_CLAT_TX_CLAT_SRC_H0	0x20F0
#define DIT_CLAT_TX_CLAT_SRC_H1	0x20F4
#define DIT_CLAT_TX_CLAT_SRC_H2	0x20F8
#define DIT_CLAT_TX_CLAT_SRC_H3	0x20FC

#define DIT_NAT_TX_DESC_ADDR_0_SRC	0x4000
#define DIT_NAT_TX_DESC_ADDR_1_SRC	0x4004
#define DIT_NAT_TX_DESC_ADDR_0_DST0	0x4008
#define DIT_NAT_TX_DESC_ADDR_1_DST0	0x400C
#define DIT_NAT_TX_DESC_ADDR_0_DST1	0x4010
#define DIT_NAT_TX_DESC_ADDR_1_DST1	0x4014
#define DIT_NAT_TX_DESC_ADDR_0_DST2	0x4018
#define DIT_NAT_TX_DESC_ADDR_1_DST2	0x401C

#define DIT_NAT_RX_DESC_ADDR_0_SRC	0x4020
#define DIT_NAT_RX_DESC_ADDR_1_SRC	0x4024
#define DIT_NAT_RX_DESC_ADDR_0_DST0	0x4028
#define DIT_NAT_RX_DESC_ADDR_1_DST0	0x402C
#define DIT_NAT_RX_DESC_ADDR_0_DST1	0x4030
#define DIT_NAT_RX_DESC_ADDR_1_DST1	0x4034
#define DIT_NAT_RX_DESC_ADDR_0_DST2	0x4038
#define DIT_NAT_RX_DESC_ADDR_1_DST2	0x403C

#define DIT_NAT_LOCAL_ADDR_0	0x4040
#define DIT_NAT_LOCAL_ADDR_1	0x4044
#define DIT_NAT_LOCAL_ADDR_2	0x4048
#define DIT_NAT_LOCAL_ADDR_3	0x404C
#define DIT_NAT_LOCAL_ADDR_4	0x4050
#define DIT_NAT_LOCAL_ADDR_5	0x4054
#define DIT_NAT_LOCAL_ADDR_6	0x4058
#define DIT_NAT_LOCAL_ADDR_7	0x405C
#define DIT_NAT_LOCAL_ADDR_8	0x4060
#define DIT_NAT_LOCAL_ADDR_9	0x4064
#define DIT_NAT_LOCAL_ADDR_10	0x4068
#define DIT_NAT_LOCAL_ADDR_11	0x406C
#define DIT_NAT_LOCAL_ADDR_12	0x4070
#define DIT_NAT_LOCAL_ADDR_13	0x4074
#define DIT_NAT_LOCAL_ADDR_14	0x4078
#define DIT_NAT_LOCAL_ADDR_15	0x407C

#define DIT_NAT_UE_ADDR	0x4080

#define DIT_NAT_ZERO_CHK_OFF	0x4084
#define DIT_NAT_HASH_EN	0x4088
#define DIT_NAT_ETHERNET_EN	0x408C
#define DIT_NAT_ETHERNET_DST_MAC_ADDR_0	0x6000
#define DIT_NAT_ETHERNET_DST_MAC_ADDR_1	0x6004
#define DIT_NAT_ETHERNET_SRC_MAC_ADDR_0	0x6008
#define DIT_NAT_ETHERNET_SRC_MAC_ADDR_1	0x600C
#define DIT_NAT_ETHERNET_TYPE	0x6010
#define DIT_NAT_TX_PORT_TABLE_INFO_0	0x6014
#define DIT_NAT_TX_PORT_TABLE_INFO_1	0x6018
#define DIT_NAT_TX_PORT_UPDATE_START	0x601C
#define DIT_NAT_TX_PORT_UPDATE_DONE	0x6020
#define DIT_NAT_TX_PORT_INIT_START	0x6024
#define DIT_NAT_TX_PORT_INIT_DONE	0x6028
#define DIT_NAT_RX_PORT_TABLE_INFO_0	0x602C
#define DIT_NAT_RX_PORT_TABLE_INFO_1	0x6030
#define DIT_NAT_RX_PORT_UPDATE_START	0x6034
#define DIT_NAT_RX_PORT_UPDATE_DONE	0x6038
#define DIT_NAT_RX_PORT_INIT_START	0x603C
#define DIT_NAT_RX_PORT_INIT_DONE	0x6040
#define DIT_TEST_OUT_SEL_0	0x8000
#define DIT_NAT_TX_PORT_TABLE_SLOT_0	0xA000
#define DIT_NAT_RX_PORT_TABLE_SLOT_0	0xC000


/* COMMAND mask */
enum {
	DIT_INIT_CMD,
	DIT_TX_CMD,
	DIT_RX_CMD,
};

/* STATUS mask */
#define DIT_RX_STATUS_MASK	0xF0
#define DIT_TX_STATUS_MASK	0xF

/* Field control in descriptor */
enum {
	DIT_DESC_C_RESV,	/* Reserved */
	DIT_DESC_C_END,		/* end packet of LRO */
	DIT_DESC_C_START,	/* first packet of LRO */
	DIT_DESC_C_RINGEND,	/* End of descriptor */
	DIT_DESC_C_INT,		/* Interrupt enabled */
	DIT_DESC_C_CSUM,	/* csum enabled */
	DIT_DESC_C_TAIL,	/* last buffer */
	DIT_DESC_C_HEAD		/* first buffer */
};

/* Field status in descriptor */
enum {
	DIT_DESC_S_DONE,	/* DMA done */
	DIT_DESC_S_RESV,	/* Reserved */
	DIT_DESC_S_TCPCF,	/* Failed TCP csum */
	DIT_DESC_S_IPCSF,	/* Failed IP csum */
	DIT_DESC_S_IGNR,	/* Ignore csum */
	DIT_DESC_S_TCPC,	/* TCP/UDP csum done: IGNR shold be 0 */
	DIT_DESC_S_IPCS,	/* IP header csum done: IGNR shold be 0 */
	DIT_DESC_S_PFD		/* passed packet filter */
};

/* INT */
enum { /* mapped in exynosxxxx.dts */
	DIT_dts_idx(INTREQ__DIT_RxDst0),
	DIT_dts_idx(INTREQ__DIT_RxDst1),
	DIT_dts_idx(INTREQ__DIT_RxDst2),
	DIT_dts_idx(INTREQ__DIT_Tx),
	DIT_dts_idx(INTREQ__DIT_Err)
};

/* INT_PENDING position */
enum {
	DIT_TX_DST0_INT_PENDING,
	DIT_TX_DST1_INT_PENDING,
	DIT_TX_DST2_INT_PENDING,
	DIT_RX_DST0_INT_PENDING,
	DIT_RX_DST1_INT_PENDING,
	DIT_RX_DST2_INT_PENDING,
	DIT_TX_RST_PACKET_INT_PENDING,
	DIT_TX_FIN_PACKET_INT_PENDING,
	DIT_TX_FINACK_PACKET_INT_PENDING,
	DIT_RX_RST_PACKET_INT_PENDING,
	DIT_RX_FIN_PACKET_INT_PENDING,
	DIT_RX_FINACK_PACKET_INT_PENDING,
	DIT_ERR_INT_PENDING,
};

enum {
	DIT_DEINIT_REASON_RUNTIME,
	DIT_DEINIT_REASON_FAIL,
};

enum {
	DIT_INIT_REASON_RUNTIME,
	DIT_INIT_REASON_FAIL,
};

#define DIT_TX_DST_INT_MASK	((0x1 << DIT_TX_DST0_INT_PENDING) \
							| (0x1 << DIT_TX_DST1_INT_PENDING) \
							| (0x1 << DIT_TX_DST2_INT_PENDING))

#define DIT_RX_DST_INT_MASK	((0x1 << DIT_RX_DST0_INT_PENDING) \
							| (0x1 << DIT_RX_DST1_INT_PENDING) \
							| (0x1 << DIT_RX_DST2_INT_PENDING))

/* DIT 2.0 handle little endian for port table */
#define DIT_PTABLE_IDX_MASK	0x7ff /* 11 [10:0] bits */
#define PTABLE_IDX(port)	(((port) & DIT_PTABLE_IDX_MASK) * 4)


/* LAN0: USB network, LAN1: WiFi network
 * dit_init and dit_deinit called by LAN0
 */
static inline int isLAN0device(struct net_device *ndev)
{
	if (strstr(ndev->name, "rndis") || strstr(ndev->name, "ncm"))
		return 1;
	return 0;
}

static inline int isLAN1device(struct net_device *ndev)
{
	if (strstr(ndev->name, "wlan"))
		return 1;
	return 0;
}

static inline int isLANdevice(struct net_device *ndev)
{
	if (isLAN0device(ndev) || isLAN1device(ndev))
		return 1;
	return 0;
}

/* RAN: Radio Access Network */
static inline int isRANdevice(struct net_device *ndev)
{
	if (strstr(ndev->name, "rmnet"))
		return 1;
	return 0;
}

/* DIT functions */
int dit_init(int reason);
int dit_deinit(int reason);
int dit_init_hw(void);


int dit_kick(struct dit_dev_info_t *dit, int DIR, int start);
int dit_set_nat_local_addr(int ifnet, u32 addr);

int pass_skb_to_netdev(int DIR, int dst, int start, char *tag);
int dit_forward_skb(struct sk_buff *skb, int DIR, int dst);

irqreturn_t dit_rx_irq_handler(int irq, void *arg);
irqreturn_t dit_tx_irq_handler(int irq, void *arg);
irqreturn_t dit_errbus_irq_handler(int irq, void *arg);

int dit_forward_add(__be16 reply_port, __be16 org_port, int ifnet);
int dit_forward_delete(__be16 reply_port, __be16 org_port, int ifnet);

void dit_setup_upstream_device(struct net_device *ndev);
void dit_setup_downstream_device(struct net_device *ndev);
void dit_clear_upstream_device(struct net_device *ndev);
void dit_clear_downstream_device(struct net_device *ndev);

int dit_enqueue_to_backlog(int DIR, struct sk_buff *skb);
int dit_schedule(int type);

void dit_get_plat_prefix(u32 id, struct in6_addr *paddr);
void dit_set_plat_prefix(u32 id, struct in6_addr addr);
void dit_get_clat_addr(u32 id, struct in6_addr *paddr);
void dit_set_clat_addr(u32 id, struct in6_addr addr);
void dit_get_v4_filter(u32 id, u32 *paddr);
void dit_set_v4_filter(u32 id, u32 addr);

/* extern functions */
extern void gether_get_host_addr_u8(struct net_device *net, u8 host_mac[ETH_ALEN]);


#endif /* __EXYNOS_DIT_H */
