#ifndef __MACH_CLK_CORE_HELAN3_H
#define __MACH_CLK_CORE_HELAN3_H

#include <linux/clk/mmpdcstat.h>

enum {
	CLST0 = 0,
	CLST1,
	DDR,
	AXI,
};

#define CORES_PER_CLUSTER	4
#define CLST0_CORE_CLK_NAME	"clst0"
#define CLST1_CORE_CLK_NAME	"clst1"

/* RTC/WTC table used for solution change rtc/wtc on the fly */
struct cpu_rtcwtc {
	unsigned int max_pclk;	/* max rate could be used by this rtc/wtc */
	unsigned int l1_xtc;
	unsigned int l2_xtc;
};

struct cpu_opt {
	unsigned int pclk;	/* core clock */
	unsigned int core_aclk;	/* core AXI interface clock */
	unsigned int ap_clk_sel;	/* core src sel val */
	struct clk *parent;	/* core clk parent node */
	unsigned int ap_clk_src;	/* core src rate */
	unsigned int pclk_div;	/* core clk divider */
	unsigned int core_aclk_div;	/* core AXI interface clock divider */
	unsigned int l1_xtc;		/* L1 cache RTC/WTC */
	unsigned int l2_xtc;		/* L2 cache RTC/WTC */
	void __iomem *l1_xtc_addr;
	void __iomem *l2_xtc_addr;
	struct list_head node;
};

/*
 * struct core_params store core specific data
 */
struct core_params {
	void __iomem *apmu_base;
	void __iomem *mpmu_base;
	void __iomem *ciu_base;
	void __iomem *dciu_base;
	struct parents_table *parent_table;
	int parent_table_size;
	struct cpu_opt *cpu_opt;
	int cpu_opt_size;
	struct cpu_rtcwtc *cpu_rtcwtc_table;
	int cpu_rtcwtc_table_size;
	unsigned int max_cpurate;
	unsigned int bridge_cpurate;
	spinlock_t *shared_lock;

	/* dynamic dc stat support? */
	bool dcstat_support;

	powermode pxa_powermode;

};

extern struct clk *mmp_clk_register_core(const char *name,
					 const char **parent_name,
					 u8 num_parents, unsigned long flags,
					 u32 core_flags, spinlock_t *lock,
					 struct core_params *params);

/* DDR */
struct ddr_opt {
	unsigned int dclk;	/* ddr clock */
	unsigned int mode_4x_en;	/* enable dclk 4x mode */
	unsigned int ddr_tbl_index;	/* ddr FC table index */
	unsigned int ddr_lpmtbl_index;	/* ddr LPM table index */
	unsigned int ddr_freq_level;	/* ddr freq level(0~7) */
	unsigned int ddr_volt_level;	/* ddr voltage level (0~7) */
	u32 ddr_clk_sel;	/* ddr src sel val */
	unsigned int ddr_clk_src;	/* ddr src rate */
	struct clk *ddr_parent;	/* ddr clk parent node */
	unsigned int dclk_div;	/* ddr clk divider */
};

struct ddr_params {
	void __iomem *apmu_base;
	void __iomem *mpmu_base;
	void __iomem *dmcu_base;

	struct parents_table *parent_table;
	int parent_table_size;
	struct ddr_opt *ddr_opt;
	int ddr_opt_size;
	/* dynamic dc stat support? */
	bool dcstat_support;
};

struct clk *mmp_clk_register_ddr(const char *name, const char **parent_name,
				 u8 num_parents, unsigned long flags,
				 u32 ddr_flags, spinlock_t *lock,
				 struct ddr_params *params);

/*
 * Slave clock is combined with master clock. Master clock FC will trigger
 * slave clock FC automatically.
 */
struct combclk_relation {
	unsigned long mclk_rate;
	unsigned long sclk_rate;
};

struct combclk_info {
	struct clk *sclk;
	unsigned long max_rate;
	struct list_head node;
	struct combclk_relation *relation_tbl;
	unsigned int nr_relation_tbl;
};

int register_slave_clock(struct clk *sclk, int mclk_type,
			 unsigned long max_freq,
			 struct combclk_relation *relation_tbl,
			 unsigned int nr_relation_tbl);

/* RTC/WTC table used for solution change rtc/wtc on the fly */
struct axi_rtcwtc {
	unsigned int max_aclk;	/* max rate could be used by this rtc/wtc */
	unsigned int xtc_val;
};

void register_cci_notifier(void __iomem *apmu_base, struct clk *clk);

/*
 * MMP AXI:
 */
struct axi_opt {
	unsigned int aclk;	/* axi clock */
	u32 axi_clk_sel;	/* axi src sel val */
	unsigned int axi_clk_src;	/* axi src rate */
	struct clk *axi_parent;	/* axi clk parent node */
	unsigned int aclk_div;	/* axi clk divider */
	unsigned int xtc_val;		/* RTC/WTC value */
	void __iomem *xtc_addr;	/* RTC/WTC address */
};

struct axi_params {
	void __iomem *apmu_base;
	void __iomem *mpmu_base;
	void __iomem *ciu_base;
	struct parents_table *parent_table;
	int parent_table_size;
	struct axi_opt *axi_opt;
	int axi_opt_size;
	struct axi_rtcwtc *axi_rtcwtc_table;
	int axi_rtcwtc_table_size;
	/* dynamic dc stat support? */
	bool dcstat_support;
};

struct clk *mmp_clk_register_axi(const char *name, const char **parent_name,
				 u8 num_parents, unsigned long flags,
				 u32 axi_flags, spinlock_t *lock,
				 struct axi_params *params);

#endif
