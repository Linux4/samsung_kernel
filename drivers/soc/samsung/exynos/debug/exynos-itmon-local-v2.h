/*
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * IPs Traffic Monitor(ITMON) Driver for Samsung Exynos SOC
 * By Hosung Kim (hosung0.kim@samsung.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/bitops.h>
#include <linux/of_irq.h>
#include <linux/delay.h>
#include <linux/pm_domain.h>
#include <soc/samsung/exynos/exynos-itmon.h>
#include <soc/samsung/exynos/debug-snapshot.h>
#include <soc/samsung/exynos/exynos-adv-tracer-ipc.h>

/* #define MULTI_IRQ_SUPPORT_ITMON */
#define DBG_CTL				(0x1800)
#define ERR_LOG_STAT			(0x1804)
#define ERR_LOG_POP			(0x1808)
#define ERR_LOG_CLR			(0x180C)
#define ERR_LOG_EN_NODE			(0x1810)
#define ERR_LOG_INFO_0			(0x1840)
#define ERR_LOG_INFO_1			(0x1844)
#define ERR_LOG_INFO_2			(0x1848)
#define ERR_LOG_INFO_3			(0x184C)
#define ERR_LOG_INFO_4			(0x1850)
#define ERR_LOG_INFO_5			(0x1854)
#define ERR_LOG_INFO_6			(0x1858)

#define INT_EN				BIT(0)
#define ERR_LOG_EN			BIT(1)
#define TMOUT_EN			BIT(2)
#define	PROT_CHK_EN			BIT(3)
#define FIXED_DET_EN			BIT(4)

#define PRTCHK_M_CTL(x)			(0x1C00 + (x * 4))
#define PRTCHK_S_CTL(x)			(0x1D00 + (x * 4))
#define TMOUT_CTL(x)			(0x1900 + (x * 4))

#define OFFSET_TMOUT_REG		(0x1A00)
#define TMOUT_POINT_ADDR		(0x40)
#define TMOUT_RW_OFFSET			(0x20)
#define TMOUT_ID			(0x44)
#define TMOUT_PAYLOAD_0			(0x48)
#define TMOUT_PAYLOAD_1			(0x4C)
#define TMOUT_PAYLOAD_2			(0x50)
#define TMOUT_PAYLOAD_3			(0x54)
#define TMOUT_PAYLOAD_4			(0x58)
#define TMOUT_PAYLOAD_5			(0x5C)

#define PRTCHK_START_ADDR_LOW		(0x1B00)
#define PRTCHK_END_ADDR_LOW		(0x1B04)
#define PRTCHK_START_END_ADDR_UPPER	(0x1B08)

#define RD_RESP_INT_EN			(1 << 0)
#define WR_RESP_INT_EN			(1 << 1)
#define ARLEN_RLAST_INT_EN		(1 << 2)
#define AWLEN_WLAST_INT_EN		(1 << 3)
#define INTEND_ACCESS_INT_EN		(1 << 4)

#define ERR_CODE(x)			(x & (0xF))
#define MNODE_ID(x)			(((x) & (0xFF << 24)) >> 24)
#define DET_ID(x)			(((x) & (0xFF << 16)) >> 16)
#define RW(x)				(((x) & (0x1 << 12)) >> 12)

#define AXDOMAIN(x)			(((x) & ((0x1) << 31)) >> 31)
#define AXSIZE(x)			(((x) & (0x7 << 28)) >> 28)
#define AXCACHE(x)			(((x) & (0xF << 24)) >> 24)
#define AXQOD(x)			(((x) & (0xF << 20)) >> 20)
#define AXLEN(x)			(((x) & (0xF << 16)) >> 16)

#define AXID(x)				(((x) & (0xFFFFFFFF)))
#define AXUSER(x, y)			((u64)((x) & (0xFFFFFFFF)) | (((y) & (0xFFFFFFFF)) << 32ULL))
#define ADDRESS(x, y)			((u64)((x) & (0xFFFFFFFF)) | (((y) & (0xFFFF)) << 32ULL))

#define AXBURST(x)			(((x) & (0x3 << 2)) >> 2)
#define AXPROT(x)			((x) & (0x3))

#define TMOUT_BIT_ERR_CODE(x)			(((x) & (0xF << 28)) >> 28)
#define TMOUT_BIT_ERR_OCCURRED(x)		(((x) & (0x1 << 27)) >> 27)
#define TMOUT_BIT_ERR_VALID(x)		(((x) & (0x1 << 26)) >> 26)
#define TMOUT_BIT_AXID(x)			(((x) & (0xFFFF)))
#define TMOUT_BIT_AXUSER(x)			(((x) & (0xFFFFFFFF)))
#define TMOUT_BIT_AXUSER_PERI(x)		(((x) & (0xFFFF << 16)) >> 16)
#define TMOUT_BIT_AXBURST(x)			(((x) & (0x3)))
#define TMOUT_BIT_AXPROT(x)			(((x) & (0x3 << 2)) >> 2)
#define TMOUT_BIT_AXLEN(x)			(((x) & (0xF << 16)) >> 16)
#define TMOUT_BIT_AXSIZE(x)			(((x) & (0x7 << 28)) >> 28)

#define ERR_SLVERR			(0)
#define ERR_DECERR			(1)
#define ERR_UNSUPPORTED			(2)
#define ERR_POWER_DOWN			(3)
#define ERR_INTEND			(4)
#define ERR_TMOUT			(6)
#define ERR_PRTCHK_ARID_RID		(8)
#define ERR_PRTCHK_AWID_RID		(9)
#define ERR_PRTCHK_ARLEN_RLAST_EL	(10)
#define ERR_PRTCHK_ARLEN_RLAST_NL	(11)
#define ERR_PRTCHK_AWLEN_WLAST_EL	(12)
#define ERR_PRTCHK_AWLEN_WLAST_NL	(13)
#define ERR_TMOUT_FREEZE		(15)

#define DATA				(0)
#define CONFIG				(1)
#define BUS_PATH_TYPE			(2)

#define TRANS_TYPE_WRITE		(0)
#define TRANS_TYPE_READ			(1)
#define TRANS_TYPE_NUM			(2)

#define CP_COMMON_STR			"MODEM_"
#define NO_NAME				"N/A"

#define TMOUT_VAL			(0xFFFFF)
#define TMOUT_TEST			(0x1)

#define ERR_THRESHOLD			(5)

/* This value will be fixed */
#define INTEND_ADDR_START		(0)
#define INTEND_ADDR_END			(0)

#define GET_IRQ(x)			(x & 0xFFFF)
#define GET_AFFINITY(x)			((x >> 16) & 0xFFFF)

#define SET_IRQ(x)			(x & 0xFFFF)
#define SET_AFFINITY(x)			((x & 0xFFFF) << 16)

#define OFFSET_DSS_CUSTOM		(0xF000)
#define CUSTOM_MAX_PRIO			(7)

#define	NOT_SUPPORT			(0xFF)

#define HISTORY_MAGIC			(0xCAFECAFE)

enum err_type {
	TMOUT,
	PRTCHKER,
	DECERR,
	SLVERR,
	FATAL,
	TYPE_MAX,
};

struct itmon_policy {
	char *name;
	int policy_def;
	int policy;
	int prio;
	bool error;
};

struct itmon_rpathinfo {
	unsigned short id;
	char port_name[16];
	char dest_name[16];
	unsigned short bits;
} __packed;

struct itmon_masterinfo {
	char port_name[16];
	unsigned short user;
	char master_name[24];
	unsigned short bits;
} __packed;

struct itmon_nodegroup;
struct itmon_nodeinfo;

struct itmon_traceinfo {
	u32 m_id;
	u32 s_id;
	char *master;
	char *src;
	char *dest;
	struct itmon_nodeinfo *m_node;
	struct itmon_nodeinfo *s_node;
	u64 target_addr;
	u32 err_code;
	bool read;
	bool onoff;
	bool path_dirty;
	bool dirty;
	u32 path_type;
	char buf[SZ_32];
	u32 axsize;
	u32 axlen;
	u32 axburst;
	u32 axprot;
	bool baaw_prot;
	bool cpu_tran;
	struct list_head list;
};

struct itmon_tracedata {
	u32 info_0;
	u32 info_1;
	u32 info_2;
	u32 info_3;
	u32 info_4;
	u32 info_5;
	u32 info_6;
	u32 en_node;
	u32 m_id;
	u32 det_id;
	u32 err_code;
	struct itmon_traceinfo *ref_info;
	struct itmon_nodegroup *group;
	struct itmon_nodeinfo *m_node;
	struct itmon_nodeinfo *det_node;
	bool read;
	struct list_head list;
};

struct itmon_nodepolicy {
	union {
		u64 en;
		struct {
			u64 chk_set : 1;
			u64 prio : 3;
			u64 chk_errrpt : 1;
			u64 en_errrpt : 1;
			u64 chk_tmout : 1;
			u64 en_tmout : 1;
			u64 chk_prtchk : 1;
			u64 en_prtchk : 1;
			u64 chk_tmout_val : 1;
			u64 en_tmout_val : 28;
			u64 chk_freeze : 1;
			u64 en_freeze : 1;
			u64 chk_irq_mask : 1;
			u64 en_irq_mask : 1;
			u64 chk_job : 1;
			u64 chk_decerr_job : 1;
			u64 en_decerr_job : 3;
			u64 chk_slverr_job : 1;
			u64 en_slverr_job : 3;
			u64 chk_tmout_job : 1;
			u64 en_tmout_job : 3;
			u64 chk_prtchk_job : 1;
			u64 en_prtchk_job : 3;
		};
	};
} __packed;

struct itmon_nodeinfo {
	char name[16];
	u32 type : 3;
	u32 id : 5;
	u32 err_en : 1;
	u32 prt_chk_en : 1;
	u32 addr_detect_en : 1;
	u32 retention : 1;
	u32 tmout_en : 1;
	u32 tmout_frz_en : 1;
	u32 time_val : 28;
	u32 err_id : 8;
	u32 tmout_offset : 8;
	u32 prtchk_offset : 8;
	struct itmon_nodegroup *group;
	struct itmon_nodepolicy policy;
} __packed;

struct itmon_nodegroup {
	char name[16];
	u64 phy_regs;
	bool ex_table;
	struct itmon_nodeinfo *nodeinfo_phy;
	struct itmon_nodeinfo *nodeinfo;
	u32 nodesize;
	u32 path_type;
	void __iomem *regs;
	u32 irq;
	bool pd_support;
	bool pd_status;
	char pd_name[16];
	u32 src_in;
	bool frz_support;
} __packed;

struct itmon_keepdata {
	u32 magic;
	u64 base;
	u64 mem_rpathinfo;
	u32 size_rpathinfo;
	u32 num_rpathinfo;
	u64 mem_masterinfo;
	u32 size_masterinfo;
	u32 num_masterinfo;
	u64 mem_nodegroup;
	u32 size_nodegroup;
	u32 num_nodegroup;
} __packed;

struct itmon_platdata {
	struct itmon_rpathinfo *rpathinfo;
	struct itmon_masterinfo *masterinfo;
	struct itmon_nodegroup *nodegroup;

	int num_rpathinfo;
	int num_masterinfo;
	int num_nodegroup;

	struct list_head infolist[TRANS_TYPE_NUM];
	struct list_head datalist[TRANS_TYPE_NUM];
	unsigned long last_time;
	unsigned long errcnt_window_start_time;
	int last_errcnt;

	struct itmon_policy *policy;
	bool cpu_parsing;
	bool irq_oring;
	bool def_en;
	bool probed;
	struct adv_tracer_plugin *itmon_adv;
	bool adv_en;
	u32 sysfs_tmout_val;
	bool en;
};

struct itmon_dev {
	struct device *dev;
	struct itmon_platdata *pdata;
	struct itmon_keepdata *kdata;
	struct of_device_id *match;
	u32 irq;
	int id;
	void __iomem *regs;
	spinlock_t ctrl_lock;
	struct itmon_notifier notifier_info;
};

struct itmon_panic_block {
	struct notifier_block nb_panic_block;
	struct itmon_dev *pdev;
};
