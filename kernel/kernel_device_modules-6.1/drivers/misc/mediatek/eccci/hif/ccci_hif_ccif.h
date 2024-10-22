/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#ifndef __MODEM_CCIF_H__
#define __MODEM_CCIF_H__

#include <linux/pm_wakeup.h>
#include <linux/dmapool.h>
#include <linux/atomic.h>
#include "mt-plat/mtk_ccci_common.h"
#include "ccci_ringbuf.h"
#include "ccci_core.h"
#include "ccci_modem.h"
#include "ccci_hif_internal.h"

#define QUEUE_NUM   16

/* speciall for user: ccci_fsd data[0] */
#define CCCI_FS_AP_CCCI_WAKEUP (0x40000000)
#define CCCI_FS_REQ_SEND_AGAIN 0x80000000

#define CCIF_TRAFFIC_MONITOR_INTERVAL 10

#define RX_BUGDET 16
#define NET_RX_QUEUE_MASK 0x4

struct ccci_clk_node {
	struct clk *clk_ref;
	unsigned char *clk_name;
};

struct ccif_flow_control {
	unsigned int head_magic;
	unsigned int ap_busy_queue;
	unsigned int md_busy_queue;
	unsigned int tail_magic;
};

struct  ccci_hif_ccif_val {
	struct regmap *infra_ao_base;
	unsigned int md_gen;
	unsigned long offset_epof_md1;
	void __iomem *md_plat_info;
};

struct ccif_sram_layout {
	struct ccci_header dl_header;
	struct md_query_ap_feature md_rt_data;
	struct ccci_header up_header;
	struct ap_query_md_feature ap_rt_data;
};

enum ringbuf_id {
	RB_NORMAL,
	RB_EXP,
	RB_MAX,
};

struct md_ccif_queue {
	enum DIRECTION dir;
	unsigned char index;
	unsigned char hif_id;
	//unsigned char resume_cnt; //used by flow control
	unsigned char debug_id;
	//unsigned char wakeup; //used by flow control
	atomic_t rx_on_going;
	int budget;
	unsigned int ccif_ch;
	//struct ccci_modem *modem;
	//struct ccci_port *napi_port;
	struct ccci_ringbuf *ringbuf;		/*ringbuf in use*/
	struct ccci_ringbuf *ringbuf_bak[RB_MAX];
	spinlock_t rx_lock;	/* lock for the counter, only for rx */
	spinlock_t tx_lock;	/* lock for the counter, only for Tx */
	//wait_queue_head_t req_wq;	/* only for Tx */
	struct work_struct qwork;
	struct workqueue_struct *worker;
};

enum hifccif_state {
	HIFCCIF_STATE_MIN  = 0,
	HIFCCIF_STATE_PWROFF,
	HIFCCIF_STATE_PWRON,
	HIFCCIF_STATE_EXCEPTION,
	HIFCCIF_STATE_MAX,
};
struct md_ccif_ctrl {
	enum hifccif_state ccif_state;
	struct md_ccif_queue txq[QUEUE_NUM];
	struct md_ccif_queue rxq[QUEUE_NUM];
	unsigned int total_smem_size; //maybe no need: local variable?

	unsigned long channel_id;	/* CCIF channel */
	unsigned int sram_size;
	struct ccif_sram_layout *ccif_sram_layout;
	struct work_struct ccif_sram_work;
	struct work_struct ccif_status_notify_work;
	//struct timer_list bus_timeout_timer;
	void __iomem *ccif_ap_base;
	void __iomem *ccif_md_base;
	void __iomem *md_ccif4_base;
	void __iomem *md_ccif5_base;
	struct regmap *pericfg_base;
	unsigned int ap_ccif_irq0_id;
	unsigned int ap_ccif_irq1_id;
	//unsigned long ap_ccif_irq0_flags;
	//unsigned long ap_ccif_irq1_flags;
	atomic_t ccif_irq_enabled;
	atomic_t ccif_irq1_enabled;
	unsigned long wakeup_ch;
	unsigned int wakeup_count;

	//struct work_struct wdt_work;

	unsigned char hif_id;
	struct ccci_hif_traffic traffic_info;
	struct timer_list traffic_monitor;

	struct ccci_hif_ops *ops;
	struct platform_device *plat_dev;
	struct ccci_hif_ccif_val plat_val;
	unsigned long long isr_cnt[CCIF_CH_NUM];

	unsigned int ccif_hw_reset_ver;
	void __iomem *infracfg_base;
	unsigned int ccif_hw_reset_bit;

	unsigned int ccif_clk_free_run;
};

#ifdef CCCI_KMODULE_ENABLE

#define ccci_write32(b, a, v)  \
do { \
	writel(v, (b) + (a)); \
	mb(); /* make sure register access in order */ \
} while (0)


#define ccci_write16(b, a, v)  \
do { \
	writew(v, (b) + (a)); \
	mb(); /* make sure register access in order */ \
} while (0)


#define ccci_write8(b, a, v)  \
do { \
	writeb(v, (b) + (a)); \
	mb(); /* make sure register access in order */ \
} while (0)

#define ccci_read32(b, a)               ioread32((void __iomem *)((b)+(a)))
#define ccci_read16(b, a)               ioread16((void __iomem *)((b)+(a)))
#define ccci_read8(b, a)                ioread8((void __iomem *)((b)+(a)))
#endif

extern struct regmap *syscon_regmap_lookup_by_phandle(struct device_node *np,
	const char *property);
extern int regmap_write(struct regmap *map, unsigned int reg, unsigned int val);
extern int regmap_read(struct regmap *map, unsigned int reg, unsigned int *val);
#if IS_ENABLED(CONFIG_MTK_IRQ_DBG)
extern void mt_irq_dump_status(unsigned int irq);
#endif

extern int md_fsm_exp_info(unsigned int channel_id);
extern char *ccci_port_get_dev_name(unsigned int rx_user_id);
/* used for throttling feature - end */
#endif				/* __MODEM_CCIF_H__ */
