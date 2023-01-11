/*
 * mck_memorybus: devfreq device driver for MCK memory controller.
 *
 * Copyright (C) 2013 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LINUX_MCK_MEMORYBUS_H__
#define __LINUX_MCK_MEMORYBUS_H__

#define DDR_FREQ_MAX 20

#define DEFAULT_MCK_BASE_ADDR	(0xc0100000)
#define DEFAULT_MCK_REG_SIZE	(SZ_2K)
#define DEFAULT_PERCNT_IN_USE	(4)

#define MCK4_VER_SHIFT		(12)
#define MCK4_VER_MASK		(0xf << MCK4_VER_SHIFT)

#define MCK4_SDRAM_CTRL4		(0x058)
#define MCK4_SDRAM_CTRL4_BL_SHIFT	(22)
#define MCK4_SDRAM_CTRL4_BL_MASK	(0x3 << MCK4_SDRAM_CTRL4_BL_SHIFT)

#define MCK4_PERF_CONFIG	(0x440)
#define MCK4_PERF_STATUS	(0x444)
#define MCK4_PERF_CONTRL	(0x448)
#define MCK4_PERF_CNT_0		(0x450)
#define MCK4_PERF_CNT_1		(0x454)
#define MCK4_PERF_CNT_2		(0x458)
#define MCK4_PERF_CNT_3		(0x45C)
#define MCK4_PERF_CNT_BASE	MCK4_PERF_CNT_0
#define MCK4_INTR_STATUS	(0x480)
#define MCK4_INTR_EN		(0x484)

#define MCK4_PERCNT_NUM		(4)

#define MCK5_VER_SHIFT		(24)
#define MCK5_VER_MASK		(0xf << MCK5_VER_SHIFT)

#define MCK5_WCB_DRAINING	(0x1 << 28)
#define MCK5_WP_STATUS		(0x10)

#define MCK5_WCB_DRAIN_REQ	(0x1 << 1)
#define MCK5_USER_CMD0		(0x20)

#define MCK5_CH0_SDRAM_CFG1		(0x300)
#define MCK5_CH0_SDRAM_CFG1_BL_SHIFT	(20)
#define MCK5_CH0_SDRAM_CFG1_BL_MASK	(0x3 << MCK5_CH0_SDRAM_CFG1_BL_SHIFT)

#define MCK5_PERF_CONFIG	(0x100)
#define MCK5_PERF_STATUS	(0x108)
#define MCK5_PERF_CONTRL	(0x10C)
#define MCK5_PERF_CNT_0		(0x110)
#define MCK5_PERF_CNT_1		(0x114)
#define MCK5_PERF_CNT_2		(0x118)
#define MCK5_PERF_CNT_3		(0x11C)
#define MCK5_PERF_CNT_4		(0x120)
#define MCK5_PERF_CNT_5		(0x124)
#define MCK5_PERF_CNT_6		(0x128)
#define MCK5_PERF_CNT_7		(0x12C)
#define MCK5_PERF_CNT_BASE	MCK5_PERF_CNT_0
#define MCK5_INTR_STATUS	(0x140)
#define MCK5_INTR_EN		(0x144)

#define MCK5_PERCNT_NUM		(8)

enum mck_version {
	MCK_UNKNOWN = 0,
	MCK4 = 4,
	MCK5 = 5,
};

/* struct of pmu related registers' offset */
struct mck_pmu_regs_offset {
	unsigned int cfg;	/* PC Configuration register */
	unsigned int cnt_stat;	/* PC Status register */
	unsigned int ctrl;	/* PC Control register */
	unsigned int cnt_base;	/* PC register 0 */
	unsigned int intr_stat;	/* MCK Interrupt Status register */
	unsigned int intr_en;	/* MCK Interrupt Enable register*/
};

/*
 *  reg[0] ddr_totalticks
 *  reg[1] ddr_DPC_idle
 *  reg[2] ddr_rw_cmd
 *  reg[3] ddr_nobus_notidle
 */
struct perf_counters {
	u64 *reg;
};

struct mck_ppmu {
	void __iomem *hw_base;
	/* the version of mck controller */
	unsigned int version;
	/* number of performance counters which are used for profiler */
	unsigned int pmucnt_in_use;
	/* offset of all pmu related registers */
	struct mck_pmu_regs_offset mck_regs;
	/*
	 * SW u64 performance counters for each DDR frequencies.
	 * It can store all events number from SOD without overflow.
	 * To get a accurate value, please call update before read it.
	 */
	struct perf_counters ddr_ticks[DDR_FREQ_MAX];
};

struct ddr_stats_data {
	/* snapshot of ddr_ticks for ddr stats */
	struct perf_counters ddr_ticks_base[DDR_FREQ_MAX];
	/* diff value of current ddr_ticks and ddr_ticks_base */
	struct perf_counters ddr_ticks_diff[DDR_FREQ_MAX];
	/* indicating if ddr stats are working */
	int is_ddr_stats_working;
};

/* snapshot of ddr_ticks to get tick diff in profiling window */
struct ddr_profiler_data {
	u64 total_ticks_base[DDR_FREQ_MAX];
	u64 data_ticks_base[DDR_FREQ_MAX];
};

struct ddr_devfreq_data {
	struct devfreq *devfreq;
	struct clk *ddr_clk;
	struct clk *clst0_clk;
	struct clk *clst1_clk;
	struct clk *cpu_clk;
	struct mck_ppmu dmc;
	unsigned int bst_len; /* ddr burst length */
	unsigned long last_polled_at;
	spinlock_t lock;

	/* used for performance optimization */
	u32 multi_clst;
	u32 high_upthrd_swp_clst0;
	u32 high_upthrd_swp_clst1;
	u32 high_upthrd_swp;
	u32 high_upthrd;
	u32 cpu_up;
	u32 gpu_up;
	/* notifier block for ddr upthreshold change */
	struct notifier_block freq_transition;

	/* DDR frequency table used for platform */
	unsigned int ddr_freq_tbl[DDR_FREQ_MAX]; /* unit Khz */
	unsigned int ddr_freq_tbl_len;
	unsigned int cur_ddr_idx;

	/* for ddr_profiling file node */
	struct ddr_stats_data ddr_stats;

	/* for ddr freq profiler */
	struct ddr_profiler_data ddr_profiler;

	int irq;
	struct work_struct overflow_work;

	/* used for debug interface */
	atomic_t is_disabled;
	atomic_t is_stopped;

	struct timespec start_ts;
	struct timespec stop_ts;
};

extern void get_fc_spinlock(void);
extern void put_fc_spinlock(void);
extern void put_fc_lock(void __iomem *apmu_base);
extern int get_fc_lock(void __iomem *apmu_base, int has_hwdfcwr);
#endif /* __LINUX_MCK_MEMORYBUS_H__ */
