/*
 * mmp core clock operation source file
 * created for all Helan series SoC with big.LITTLE architect.
 *
 * Copyright (C) 2014 Marvell
 * Zhoujie Wu <zjwu@marvell.com>
 * Bill Zhou  <zhoub@marvell.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/cpufreq.h>
#include <linux/devfreq.h>
#include <linux/clk-private.h>
#ifdef CONFIG_PXA_DVFS
#include <linux/clk/dvfs-dvc.h>
#endif
#define CREATE_TRACE_POINTS
#include <trace/events/pxa.h>
#include "clk.h"
#include "clk-plat.h"
#include "clk-core-helan3.h"

struct clk_core {
	struct clk_hw hw;
	struct core_params *params;
	u32 flags;
	spinlock_t *lock;
};

struct clk_ddr {
	struct clk_hw hw;
	struct ddr_params *params;
	u32 flags;
	spinlock_t *lock;
};

struct clk_axi {
	struct clk_hw hw;
	struct axi_params *params;
	u32 flags;
	spinlock_t *lock;
};

#define to_clk_core(core) container_of(core, struct clk_core, hw)
#define to_clk_ddr(ddr) container_of(ddr, struct clk_ddr, hw)
#define to_clk_axi(axi) container_of(axi, struct clk_axi, hw)

#define MAX_OP_NUM 10
#define APMU_REG(apmu_base, x)	(apmu_base + (x))
#define DFC_LEVEL(c, i)		APMU_REG(c, (0x190 + ((i) << 2)))
#define DFC_STATUS(c)		APMU_REG(c, 0x188)
#define DFC_AP(c)		APMU_REG(c, 0x180)
#define DFC_CP(c)		APMU_REG(c, 0x184)
#define APMU_PLL_SEL_STATUS(c)	APMU_REG(c, 0x00c4)
#define APMU_CCR(c)		APMU_REG(c, 0x0004)
#define APMU_CCSR(c)		APMU_REG(c, 0x000c)
#define APMU_CC2R(c)		APMU_REG(c, 0x0100)
#define APMU_CC2SR(c)		APMU_REG(c, 0x0104)
#define APMU_FCLSR(c)		APMU_REG(c, 0x0334)
#define APMU_ISR(c)		APMU_REG(c, 0x00a0)
#define APMU_DEBUG(c)		APMU_REG(c, 0x0088)
#define APMU_IMR(c)		APMU_REG(c, 0x0098)
#define APMU_CP_CCR(c)		APMU_REG(c, 0x0000)
#define APMU_MC_HW_SLP_TYPE(c)	APMU_REG(c, 0x00b0)
#define APMU_CLK_GATE(c)	APMU_REG(c, 0x0040)
#define APMU_CCI(c)		APMU_REG(c, 0x0300)

#define MPMU_REG(mpmu_base, x)	(mpmu_base + (x))

/* AP/DDR/AXI src sel register */
#define MPMU_FCAP(m)	\
	MPMU_REG(m->params->mpmu_base, 0x0054)
#define MPMU_FCDCLK(m)	\
	MPMU_REG(m->params->mpmu_base, 0x005C)
#define MPMU_FCACLK(m)	\
	MPMU_REG(m->params->mpmu_base, 0x0060)

#define DCIU_REG(dciu_base, x)	(dciu_base + (x))
#define DCIU_CPU_CONF_SRAM_0(c, i)	\
	DCIU_REG(c->params->dciu_base, (0x14 + 0x40 * (i)))
#define DCIU_CPU_CONF_SRAM_1(c, i)	\
	DCIU_REG(c->params->dciu_base, (0x18 + 0x40 * (i)))
#define DCIU_CKG(c)	DCIU_REG(c, 0x88)

#define CIU_REG(ciu_base, x)	(ciu_base + (x))
#define CIU_TOP_MEM_XTC(c)	CIU_REG(c->params->ciu_base, 0x0044)

static DEFINE_SPINLOCK(fc_seq_lock);
static struct task_struct *fc_seqlock_owner;
static int fc_seqlock_cnt;

enum fc_type {
	DDR_FC = 0,
	CORE_FC,
	AXI_FC,
};

/* core,ddr,axi clk src sel register description */
union pmum_fcapclk {
	struct {
		unsigned int apc0clksel:3;
		unsigned int reserved:1;
		unsigned int apc1clksel:3;
		unsigned int reserved1:25;
	} b;
	unsigned int v;
};

union pmum_fcdclk {
	struct {
		unsigned int ddrclksel:3;
		unsigned int reserved:29;
	} b;
	unsigned int v;
};

union pmum_fcaclk {
	struct {
		unsigned int axiclksel:2;
		unsigned int reserved:30;
	} b;
	unsigned int v;
};

/* core,ddr,axi clk src sel status register description */
union pmua_pllsel {
	struct {
		unsigned int cpclksel:3;
		unsigned int ddrclksel:3;
		unsigned int axiclksel:2;
		unsigned int apc0clksel:3;
		unsigned int apc1clksel:3;
		unsigned int reserved0:18;
	} b;
	unsigned int v;
};

/* core,ddr,axi clk div and fc trigger register description */
union pmua_cc {
	struct {
		unsigned int apc0_pclk_div:3;
		unsigned int apc0_aclk_div:3;
		unsigned int apc1_pclk_div:3;
		unsigned int apc1_aclk_div:3;
		unsigned int ddr_clk_div:3;
		unsigned int bus_clk_div:3;
		unsigned int ap_allow_spd_chg:1;
		unsigned int reserved0:1;
		unsigned int apc0_freq_chg_req:1;
		unsigned int apc1_freq_chg_req:1;
		unsigned int ddr_freq_chg_req:1;
		unsigned int bus_freq_chg_req:1;
		unsigned int apc0_dyn_fc:1;
		unsigned int apc1_dyn_fc:1;
		unsigned int dclk_dyn_fc:1;
		unsigned int aclk_dyn_fc:1;
		unsigned int reserved1:3;
		unsigned int core_rd_st_clear:1;
	} b;
	unsigned int v;
};

/* core,ddr,axi clk div and fc trigger register description */
union pmua_cc_cp {
	struct {
		unsigned int core_clk_div:3;
		unsigned int reserved1:9;
		unsigned int ddr_clk_div:3;
		unsigned int bus_clk_div:3;
		unsigned int async1:1;
		unsigned int async2:1;
		unsigned int async3:1;
		unsigned int async3_1:1;
		unsigned int async4:1;
		unsigned int async5:1;
		unsigned int core_freq_chg_req:1;
		unsigned int ddr_freq_chg_req:1;
		unsigned int bus_freq_chg_req:1;
		unsigned int core_allow_spd_chg:1;
		unsigned int core_dyn_fc:1;
		unsigned int dclk_dyn_fc:1;
		unsigned int aclk_dyn_fc:1;
		unsigned int core_rd_st_clear:1;
	} b;
	unsigned int v;
};

/* core,ddr,axi div status register description */
union pmua_dm_cc {
	struct {
		unsigned int apc0_pclk_div:3;
		unsigned int apc0_aclk_div:3;
		unsigned int apc1_pclk_div:3;
		unsigned int apc1_aclk_div:3;
		unsigned int ddr_clk_div:3;
		unsigned int bus_clk_div:3;
		unsigned int reserved0:6;	/* FIXME: async bits? */
		unsigned int cp_rd_status:1;
		unsigned int ap_rd_status:1;
		unsigned int cp_fc_done:1;
		unsigned int apc0_fc_done:1;
		unsigned int dclk_fc_done:1;
		unsigned int aclk_fc_done:1;
		unsigned int apc1_fc_done:1;
		unsigned int reserved:1;
	} b;
	unsigned int v;
};

/* hwdfc related */
enum dfc_cause {
	CP_LPM_DFC = 0,
	AP_ACTIVE_DFC,
	CP_ACTIVE_DFC,
};

union dfc_ap {
	struct {
		unsigned int dfc_req:1;
		unsigned int fl:3;
		unsigned int reserved:28;
	} b;
	unsigned int v;
};

union dfc_cp {
	struct {
		unsigned int dfc_req:1;
		unsigned int freq_level:3;
		unsigned int lpm_en:1;
		unsigned int lpm_lvl:3;
		unsigned int dfc_disable:1;
		unsigned int reserved:23;
	} b;
	unsigned int v;
};

union dfc_status {
	struct {
		unsigned int dfc_status:1;
		unsigned int cfl:3;
		unsigned int tfl:3;
		unsigned int dfc_cause:2;
		unsigned int reserved:23;
	} b;
	unsigned int v;
};

union dfc_lvl {
	struct {
		unsigned int dclksrcsel:3;
		unsigned int dclkmode:1;
		unsigned int dclkdiv:3;
		unsigned int mctblnum:5;
		unsigned int reqvl:3;
		/* for ulc/helan3 */
		unsigned int mclpmtblnum:2;
		unsigned int reserved:15;
	} b;
	unsigned int v;
};

/* lock declaration */
static LIST_HEAD(apc0_core_op_list);
static LIST_HEAD(apc1_core_op_list);
static LIST_HEAD(ddr_combined_clk_list);
static LIST_HEAD(apc0_combined_clk_list);
static LIST_HEAD(apc1_combined_clk_list);

static DEFINE_MUTEX(ddraxi_freqs_mutex);
static atomic_t fc_lock_ref_cnt;
static struct cpu_opt *cur_apc0_cpu_op;
static struct cpu_opt *cur_apc1_cpu_op;
static struct cpu_opt *clst0_bridge_op;
static struct cpu_opt *clst1_bridge_op;
static struct ddr_opt *cur_ddr_op;
static struct axi_opt *cur_axi_op;
static struct pm_qos_request cci_qos_req_min[2] = {
	{
		.name = "CCI_MIN_L",
	},
	{
		.name = "CCI_MIN_B",
	},
};

static struct clk *clk_dclk;

/*
 * get_fc_lock is actually used to AP/CP/HWDFC FC mutual exclusion, it is
 * used to protect hw freq-chg state machine. But as reading APMU_CCSR
 * register means/may get this lock, so we get/put this lock before/after
 * reading register APMU_CCSR previously. Now optimize this part and
 * seperate two ways, add an extra function clr_aprd_status for purpose
 * reading register APMU_CCSR
 */
static inline void clr_aprd_status(void __iomem *apmu_base)
{
	union pmua_cc cc_ap;

	/* write 1 to MOH_RD_ST_CLEAR to clear MOH_RD_STATUS */
	cc_ap.v = readl(APMU_CCR(apmu_base));
	cc_ap.b.core_rd_st_clear = 1;
	writel(cc_ap.v, APMU_CCR(apmu_base));
	cc_ap.b.core_rd_st_clear = 0;
	writel(cc_ap.v, APMU_CCR(apmu_base));
}

/* Check if CP allow FC bit, if NOT, vote for it */
static inline void prefc_check_cpvote(void __iomem *apmu_base)
{
	union pmua_cc_cp cc_cp;

	cc_cp.v = readl(APMU_CP_CCR(apmu_base));
	if (unlikely(!cc_cp.b.core_allow_spd_chg)) {
		pr_warn("%s: CP doesn't allow AP FC.\n", __func__);
		cc_cp.b.core_allow_spd_chg = 1;
		writel(cc_cp.v, APMU_CP_CCR(apmu_base));
	}
}

/* Check AP_ISR bit before frequency change, if set, clear it */
static inline void prefc_check_isr(void __iomem *apmu_base)
{
	unsigned int isr;
	isr = readl(APMU_ISR(apmu_base));
	if (likely(!(isr & (1 << 1))))
		return;

	pr_warn("%s: ISR was not clear in last FC. ISR: %x\n", __func__, isr);
	writel(isr & ~(1 << 1), APMU_ISR(apmu_base));
}

static inline unsigned int get_dm_cc_ap(void __iomem *apmu_base)
{
	unsigned int value;
	unsigned long flags;
	local_irq_save(flags);
	value = readl(APMU_CCSR(apmu_base));
	clr_aprd_status(apmu_base);
	local_irq_restore(flags);
	return value;
}

static inline void pre_fc(void __iomem *apmu_base)
{
	/* 0.1) check CP allow AP FC voting */
	prefc_check_cpvote(apmu_base);
	/* 0.2) check if AP ISR is set, if set, clear it */
	prefc_check_isr(apmu_base);
}

/* to keep compatible with previous version, just leave *has_hwdfcwr* here. */
int get_fc_lock(void __iomem *apmu_base, int has_hwdfcwr)
{
	int timeout;
	unsigned int reg_val = 0;

	if (atomic_inc_return(&fc_lock_ref_cnt) == 1) {
		while (1) {
			timeout = 100000;

			do {
				/* Read dm_cc_ap to compete the FC lock */
				readl(APMU_CCSR(apmu_base));
				/* Read FC lock status register to query the lock status */
				reg_val = readl(APMU_FCLSR(apmu_base));
				/* AP got FC lock */
				if ((reg_val & 0x3) == 1)
					break;
			} while (timeout--);

			if (unlikely(timeout <= 0)) {
				pr_err("%s can't get AP lock: CCSR:%x CCR:%x, FCLSR:%x\n",
				       __func__,
				       readl(APMU_CCSR(apmu_base)),
				       readl(APMU_CCR(apmu_base)),
				       readl(APMU_FCLSR(apmu_base)));
				WARN_ON(1);
				return -EAGAIN;
			}

			return 0;
		}
	}

	pr_err("%s: unmatched fc_lock_ref_cnt\n", __func__);
	return -EINVAL;
}

void put_fc_lock(void __iomem *apmu_base)
{
	if (atomic_dec_return(&fc_lock_ref_cnt) == 0)
		clr_aprd_status(apmu_base);
	else
		pr_err("%s: unmatched fc_lock_ref_cnt\n", __func__);
}

void get_fc_spinlock(void)
{
	unsigned long flags;
	local_irq_save(flags);
	if (!spin_trylock(&fc_seq_lock)) {
		if (fc_seqlock_owner == current) {
			fc_seqlock_cnt++;
			local_irq_restore(flags);
			return;
		}
		spin_lock(&fc_seq_lock);
	}
	WARN_ON_ONCE(fc_seqlock_owner != NULL);
	WARN_ON_ONCE(fc_seqlock_cnt != 0);
	fc_seqlock_owner = current;
	fc_seqlock_cnt = 1;
	local_irq_restore(flags);
}

void put_fc_spinlock(void)
{
	unsigned long flags;
	local_irq_save(flags);
	WARN_ON_ONCE(fc_seqlock_owner != current);
	WARN_ON_ONCE(fc_seqlock_cnt == 0);

	if (--fc_seqlock_cnt) {
		local_irq_restore(flags);
		return;
	}
	fc_seqlock_owner = NULL;
	spin_unlock(&fc_seq_lock);
	local_irq_restore(flags);
}

int register_slave_clock(struct clk *sclk, int mclk_type,
			 unsigned long max_freq,
			 struct combclk_relation *relation_tbl,
			 unsigned int nr_relation_tbl)
{
	struct list_head *list_head = NULL;
	struct combclk_info *comclk;

	if (mclk_type == CLST0)
		list_head = &apc0_combined_clk_list;
	else if (mclk_type == CLST1)
		list_head = &apc1_combined_clk_list;
	else if (mclk_type == DDR)
		list_head = &ddr_combined_clk_list;
	else {
		pr_err("%s: unknown mclk type = %d\n", __func__, mclk_type);
		return -EINVAL;
	}

	/* search the list of the registation for this clk */
	list_for_each_entry(comclk, list_head, node)
	    if (comclk->sclk == sclk)
			break;

	/* if clk wasn't in the list, allocate new dcstat info */
	if (comclk->sclk != sclk) {
		comclk = kzalloc(sizeof(struct combclk_info), GFP_KERNEL);
		if (!comclk)
			return -ENOMEM;

		comclk->sclk = sclk;
		comclk->max_rate = max_freq;
		comclk->relation_tbl = relation_tbl;
		comclk->nr_relation_tbl = nr_relation_tbl;
		list_add(&comclk->node, list_head);
	}
	return 0;
}

static int trigger_slave_clk_fc(int mclk_type, unsigned long mclk_rate)
{
	struct list_head *list_head = NULL;
	struct combclk_info *comclk;
	unsigned long tgt, cur;
	int ret = 0, i = 0;

	if (mclk_type == CLST0)
		list_head = &apc0_combined_clk_list;
	else if (mclk_type == CLST1)
		list_head = &apc1_combined_clk_list;
	else if (mclk_type == DDR)
		list_head = &ddr_combined_clk_list;
	else {
		pr_err("%s: unknown mclk type = %d\n", __func__, mclk_type);
		return -EINVAL;
	}

	list_for_each_entry(comclk, list_head, node) {
		if (!comclk->relation_tbl)
			continue;
		i = 0;
		while (i < comclk->nr_relation_tbl - 1) {
			if ((mclk_rate >= comclk->relation_tbl[i].mclk_rate) &&
			    (mclk_rate < comclk->relation_tbl[i + 1].mclk_rate))
				break;
			i++;
		}

		if (mclk_type == DDR) {
			tgt =
			    min(comclk->relation_tbl[i].sclk_rate,
				comclk->max_rate);
			pr_debug("%s Start rate change to %lu\n",
				 comclk->sclk->name, tgt);
			ret = clk_set_rate(comclk->sclk, tgt);
			if (ret) {
				pr_err("%s failed to change clk %s rate\n",
					__func__, comclk->sclk->name);
				continue;
			}
			cur = clk_get_rate(comclk->sclk);
			if (cur != tgt) {
				pr_warn("clk %s: cur %lu, tgt %lu\n",
					comclk->sclk->name, cur, tgt);
				WARN_ON(1);
			}
		} else {
			pm_qos_update_request(&cci_qos_req_min[mclk_type],
				comclk->relation_tbl[i].sclk_rate / MHZ_TO_KHZ);
		}
	}

	return ret;
}

static struct clk *__get_core_parent(struct clk_hw *hw, struct cpu_opt *cop)
{
	union pmua_pllsel pllsel;
	u32 src_sel;
	struct clk_core *core = to_clk_core(hw);
	struct parents_table *parent_table = core->params->parent_table;
	int i, parent_table_size = core->params->parent_table_size;

	pllsel.v = readl(APMU_PLL_SEL_STATUS(core->params->apmu_base));

	if (!strcmp(hw->clk->name, CLST0_CORE_CLK_NAME))
		src_sel = pllsel.b.apc0clksel;
	else if (!strcmp(hw->clk->name, CLST1_CORE_CLK_NAME))
		src_sel = pllsel.b.apc1clksel;
	else
		BUG_ON("__get_core_parent: invalid core clock name");

	if (cop)
		if (src_sel == cop->ap_clk_sel)
			return cop->parent;

	for (i = 0; i < parent_table_size; i++) {
		if (src_sel == parent_table[i].hw_sel_val) {
			pr_debug("clksrcst reg %x, core src sel %x, clk %s\n",
				 pllsel.v, src_sel,
				 parent_table[i].parent_name);
			return parent_table[i].parent;
		}
	}

	if (i == parent_table_size)
		pr_err("%s: out of the range of parent_table\n", __func__);
	return NULL;
}

static void get_cur_cpu_op(struct clk_hw *hw, struct cpu_opt *cop)
{
	unsigned int pclk_div, core_aclk_div;
	union pmua_dm_cc dm_cc_ap;
	struct clk_core *core = to_clk_core(hw);
	void __iomem *apmu_base = core->params->apmu_base;

	dm_cc_ap.v = get_dm_cc_ap(apmu_base);
	if (!strcmp(hw->clk->name, CLST0_CORE_CLK_NAME)) {
		pclk_div = dm_cc_ap.b.apc0_pclk_div + 1;
		core_aclk_div = dm_cc_ap.b.apc0_aclk_div + 1;
	} else if (!strcmp(hw->clk->name, CLST1_CORE_CLK_NAME)) {
		pclk_div = dm_cc_ap.b.apc1_pclk_div + 1;
		core_aclk_div = dm_cc_ap.b.apc1_aclk_div + 1;
	} else
		BUG_ON("get_cur_cpu_op: invalid core clock name");

	cop->parent = __get_core_parent(hw, cop);
	WARN_ON(!cop->parent);
	if (cop->parent) {
		cop->ap_clk_src = clk_get_rate(cop->parent) / MHZ;
		cop->pclk = cop->ap_clk_src / pclk_div;
		cop->core_aclk = cop->pclk / core_aclk_div;
	}
}

static void wait_for_fc_done(enum fc_type comp, void __iomem *apmu_base)
{
	int timeout = 100;

	/* polling ISR */
	while (!((1 << 1) & readl(APMU_ISR(apmu_base))) && timeout)
		timeout--;

	if (timeout <= 0) {
		if (comp == DDR_FC) {
			/* enhancement to check DFC related status */
			pr_err
			    ("APMU_ISR %x, CUR_DLV %d, DFC_AP %x, DFC_CP %x, DFC_STATUS %x\n",
			     readl(APMU_ISR(apmu_base)),
			     cur_ddr_op->ddr_freq_level,
			     readl(DFC_AP(apmu_base)), readl(DFC_CP(apmu_base)),
			     readl(DFC_STATUS(apmu_base)));
		}
		WARN(1, "AP frequency change timeout!\n");
		pr_err("APMU_ISR %x, fc_type %u\n",
		       readl(APMU_ISR(apmu_base)), comp);
	}
	/* only clear AP fc done signal */
	writel(readl(APMU_ISR(apmu_base)) & ~(1 << 1), APMU_ISR(apmu_base));
	return;
}

#ifdef CONFIG_PXA_DVFS
static int get_dvc_level(unsigned int freq, const unsigned long *freq_volt_tbl, int nr_vl)
{
	int i;

	for (i = 0; i < nr_vl; i++)
		if (freq <= freq_volt_tbl[i])
			return i;
	WARN_ON("cannot find targe freq in map table.");
	return nr_vl - 1;
}
#endif

static void set_ap_clk_sel(struct clk_core *core, struct cpu_opt *top)
{
	union pmum_fcapclk fcap;

	fcap.v = readl(MPMU_FCAP(core));
	if (!strcmp(core->hw.clk->name, CLST0_CORE_CLK_NAME))
		fcap.b.apc0clksel = top->ap_clk_sel;
	else if (!strcmp(core->hw.clk->name, CLST1_CORE_CLK_NAME))
		fcap.b.apc1clksel = top->ap_clk_sel;
	writel(fcap.v, MPMU_FCAP(core));
}

static void trigger_ap_fc(struct clk_core *core, struct cpu_opt *top)
{
	union pmua_cc cc_ap;
	void __iomem *apmu_base = core->params->apmu_base;
	void __iomem *apmuccr = APMU_CCR(apmu_base);

	/* 2.2) AP votes allow FC */
	cc_ap.v = readl(apmuccr);
	cc_ap.b.ap_allow_spd_chg = 1;

	if (!strcmp(core->hw.clk->name, CLST0_CORE_CLK_NAME)) {
		/* 2.3) select div for pclk, core aclk */
		cc_ap.b.apc0_pclk_div = top->pclk_div;
		cc_ap.b.apc0_aclk_div = top->core_aclk_div;

		/* 2.4) set FC req trigger core FC */
		cc_ap.b.apc0_freq_chg_req = 1;
		pr_debug("C0 CORE FC APMU_CCR[%x]\n", cc_ap.v);
		writel(cc_ap.v, apmuccr);
	} else if (!strcmp(core->hw.clk->name, CLST1_CORE_CLK_NAME)) {
		/* 2.3) select div for pclk, core aclk */
		cc_ap.b.apc1_pclk_div = top->pclk_div;
		cc_ap.b.apc1_aclk_div = top->core_aclk_div;

		/* 2.4) set FC req trigger core FC */
		cc_ap.b.apc1_freq_chg_req = 1;
		pr_debug("C1 CORE FC APMU_CCR[%x]\n", cc_ap.v);
		writel(cc_ap.v, apmuccr);
	}

	/* 2.5) wait for core fc is done */
	wait_for_fc_done(CORE_FC, apmu_base);

	/* 3) Post FC : AP clear allow FC REQ */
	cc_ap.v = readl(apmuccr);
	cc_ap.b.apc0_freq_chg_req = 0;
	cc_ap.b.apc1_freq_chg_req = 0;
	writel(cc_ap.v, apmuccr);
}

/*
 * Sequence of changing RTC on the fly
 * RTC_lowpp means RTC is better for lowPP
 * RTC_highpp means RTC is better for highPP
 *
 * lowPP -> highPP:
 * 1) lowPP(RTC_lowpp) works at Vnom_lowPP(RTC_lowpp)
 * 2) Voltage increases from Vnom_lowPP(RTC_lowpp) to
 * Vnom_highPP(RTC_highpp)
 * 3) RTC changes from RTC_lowpp to RTC_highpp, lowPP(RTC_highpp)
 * could work at Vnom_highpp(RTC_highpp) as Vnom_highpp(RTC_highpp)
 * >= Vnom_lowpp(RTC_highpp)
 * 4) Core freq-chg from lowPP(RTC_highpp) to highPP(RTC_highpp)
 *
 * highPP -> lowPP:
 * 1) highPP(RTC_highpp) works at Vnom_highPP(RTC_highpp)
 * 2) Core freq-chg from highPP(RTC_highpp) to lowPP(RTC_highpp),
 * voltage could meet requirement as Vnom_highPP(RTC_highpp) >=
 * Vnom_lowpp(RTC_highpp)
 * 3) RTC changes from RTC_highpp to RTC_lowpp. Vnom_lowpp(RTC_lowpp)
 * < Vnom_lowpp(RTC_highpp), the voltage is ok
 * 4) voltage decreases from Vnom_highPP(RTC_highpp) to
 * Vnom_lowPP(RTC_lowpp)
 */
static void core_fc_seq(struct clk_hw *hw, struct cpu_opt *cop,
			struct cpu_opt *top)
{
	struct clk_core *core = to_clk_core(hw);
	void __iomem *apmu_base = core->params->apmu_base;

	/* update L1/L2 rtc/wtc if neccessary, PP low -> high */
	if ((cop->pclk < top->pclk) && (top->l1_xtc != cop->l1_xtc))
		writel(top->l1_xtc, top->l1_xtc_addr);
	if ((cop->pclk < top->pclk) && (top->l2_xtc != cop->l2_xtc))
		writel(top->l2_xtc, top->l2_xtc_addr);

	trace_pxa_core_clk_chg(CLK_CHG_ENTRY, cop->pclk, top->pclk);

	pre_fc(apmu_base);
	set_ap_clk_sel(core, top);
	trigger_ap_fc(core, top);

	trace_pxa_core_clk_chg(CLK_CHG_EXIT, cop->pclk, top->pclk);

	/*  update L1/L2 rtc/wtc if neccessary, high -> low */
	if ((cop->pclk > top->pclk) && (top->l1_xtc != cop->l1_xtc))
		writel(top->l1_xtc, top->l1_xtc_addr);
	if ((cop->pclk > top->pclk) && (top->l2_xtc != cop->l2_xtc))
		writel(top->l2_xtc, top->l2_xtc_addr);
}

static int set_core_freq(struct clk_hw *hw, struct cpu_opt *old,
			 struct cpu_opt *new)
{
	struct cpu_opt cop;
	struct clk *old_parent;
	int ret = 0;
	unsigned long flags;
	struct clk_core *core = to_clk_core(hw);
	void __iomem *apmu_base = core->params->apmu_base;

	pr_debug("CORE set_freq start: old %u, new %u\n", old->pclk, new->pclk);

	cop = *old;
	get_cur_cpu_op(hw, &cop);
	if (unlikely((cop.ap_clk_src != old->ap_clk_src) ||
		     (cop.pclk != old->pclk) ||
		     (cop.core_aclk != old->core_aclk))) {
		pr_err("psrc pclk core_aclk\n");
		pr_err("OLD %d %d %d\n", old->ap_clk_src,
		       old->pclk, old->core_aclk);
		pr_err("CUR %d %d %d\n", cop.ap_clk_src,
		       cop.pclk, cop.core_aclk);
		pr_err("NEW %d %d %d\n", new->ap_clk_src,
		       new->pclk, new->core_aclk);
		dump_stack();
	}

	old_parent = cop.parent;
	clk_prepare_enable(new->parent);

	/* Get lock in irq disable status to short AP hold lock time */
	local_irq_save(flags);
	ret = get_fc_lock(apmu_base, 0);
	if (ret) {
		put_fc_lock(apmu_base);
		local_irq_restore(flags);
		clk_disable_unprepare(new->parent);
		goto out;
	}
	core_fc_seq(hw, &cop, new);
	put_fc_lock(apmu_base);
	local_irq_restore(flags);

	cop = *new;
	get_cur_cpu_op(hw, &cop);
	if (unlikely((cop.ap_clk_src != new->ap_clk_src) ||
		     (cop.pclk != new->pclk) ||
		     (cop.core_aclk != new->core_aclk))) {
		pr_err("unsuccessful frequency change!\n");
		pr_err("psrc pclk core_aclk\n");
		pr_err("CUR %d %d %d\n", cop.ap_clk_src,
		       cop.pclk, cop.core_aclk);
		pr_err("NEW %d %d %d\n", new->ap_clk_src,
		       new->pclk, new->core_aclk);
		pr_err("FCAP %x, CCAP %x, PLLSEL %x, DMCCAP %x, CCCP %x\n",
		       readl(MPMU_FCAP(core)),
		       readl(APMU_CCR(apmu_base)),
		       readl(APMU_PLL_SEL_STATUS(apmu_base)),
		       get_dm_cc_ap(apmu_base), readl(APMU_CP_CCR(apmu_base)));
		ret = -EAGAIN;
		if (cop.ap_clk_src != new->ap_clk_src) {
			/* restore current src */
			set_ap_clk_sel(core, &cop);
			pr_info("Recovered FCAP: %x\n", readl(MPMU_FCAP(core)));
			clk_disable_unprepare(new->parent);
		}
		goto out;
	}

	clk_disable_unprepare(old_parent);
	pr_debug("CORE set_freq end: old %u, new %u\n", old->pclk, new->pclk);
out:
	return ret;
}

static struct cpu_opt *cpu_rate2_op_ptr(unsigned int rate, unsigned int *index,
					struct list_head *op_list)
{
	unsigned int idx = 0;
	struct cpu_opt *cop;

	list_for_each_entry(cop, op_list, node) {
		if ((cop->pclk >= rate) || list_is_last(&cop->node, op_list))
			break;
		idx++;
	}

	*index = idx;
	return cop;
}

static void __init __init_cpu_rtcwtc(struct clk_hw *hw, struct cpu_opt *cpu_opt)
{
	unsigned int size, index;
	struct clk_core *core = to_clk_core(hw);
	struct cpu_rtcwtc *cpu_rtcwtc_table = core->params->cpu_rtcwtc_table;
	size = core->params->cpu_rtcwtc_table_size;

	if (!cpu_rtcwtc_table || !size)
		return;

	for (index = 0; index < size; index++)
		if (cpu_opt->pclk <= cpu_rtcwtc_table[index].max_pclk)
			break;

	if (index == size)
		index = size - 1;

	cpu_opt->l1_xtc = cpu_rtcwtc_table[index].l1_xtc;
	cpu_opt->l2_xtc = cpu_rtcwtc_table[index].l2_xtc;

	if (!strcmp(hw->clk->name, CLST0_CORE_CLK_NAME)) {
		cpu_opt->l1_xtc_addr = DCIU_CPU_CONF_SRAM_0(core, 0);
		cpu_opt->l2_xtc_addr = DCIU_CPU_CONF_SRAM_1(core, 0);
	} else if (!strcmp(hw->clk->name, CLST1_CORE_CLK_NAME)) {
		cpu_opt->l1_xtc_addr = DCIU_CPU_CONF_SRAM_0(core, 1);
		cpu_opt->l2_xtc_addr = DCIU_CPU_CONF_SRAM_1(core, 1);
	}
}

#ifdef CONFIG_CPU_FREQ

static void __init_cpufreq_table(struct clk_hw *hw)
{
#ifdef CONFIG_PXA_DVFS
	int nr_volt_level;
	const unsigned long *freq_vl_tbl = NULL;
#endif
	struct cpu_opt *cop;
	unsigned int cpu_opt_size = 0, i = 0, clst_index = 0;
	struct clk_core *core = to_clk_core(hw);
	struct cpufreq_frequency_table *cpufreq_tbl;
	struct list_head *op_list = NULL;

	if (!strcmp(hw->clk->name, CLST0_CORE_CLK_NAME)) {
		op_list = &apc0_core_op_list;
		clst_index = 0;
	} else if (!strcmp(hw->clk->name, CLST1_CORE_CLK_NAME)) {
		op_list = &apc1_core_op_list;
		clst_index = 1;
	}

	cpu_opt_size = core->params->cpu_opt_size;
	cpufreq_tbl =
	    kmalloc(sizeof(struct cpufreq_frequency_table) *
		    (cpu_opt_size + 1), GFP_KERNEL);
	if (!cpufreq_tbl)
		return;

#ifdef CONFIG_PXA_DVFS
	nr_volt_level = dvfs_get_svc_freq_table(&freq_vl_tbl, hw->clk->name);
#endif

	list_for_each_entry(cop, op_list, node) {
		cpufreq_tbl[i].frequency = cop->pclk * MHZ_TO_KHZ;
#ifdef CONFIG_PXA_DVFS
		cpufreq_tbl[i].driver_data = get_dvc_level(cpufreq_tbl[i].frequency, freq_vl_tbl, nr_volt_level);
#endif
		i++;
	}
	/* driver_data in the last entry save the freq table size */
	cpufreq_tbl[i].driver_data = i;
	cpufreq_tbl[i].frequency = CPUFREQ_TABLE_END;

	for (i = clst_index * CORES_PER_CLUSTER; i < (clst_index + 1) * CORES_PER_CLUSTER; i++)
		cpufreq_frequency_table_get_attr(cpufreq_tbl, i);
}
#else
#define __init_cpufreq_table() do {} while (0)
#endif

#define MAX_PP_NUM	10
static long pp_disable[2][MAX_PP_NUM];
static unsigned int pp_discnt[2];
static int remove_clst_pp(char *s, int clst_index)
{
	char *p = s, *p1;
	while (p) {
		p1 = strchr(p, ',');
		if (p1) {
			*p1++ = '\0';
			if (kstrtol
			    (p, 10,
			     &pp_disable[clst_index][pp_discnt[clst_index]++]))
				break;
			p = p1;
		} else {
			return kstrtol(p, 10,
				       &pp_disable[clst_index][pp_discnt
							       [clst_index]++]);
		}
	}
	return 1;
}

static int remove_clst0_pp(char *s)
{
	return remove_clst_pp(s, 0);
}

static int remove_clst1_pp(char *s)
{
	return remove_clst_pp(s, 1);
}

__setup("clst0_nopp=", remove_clst0_pp);
__setup("clst1_nopp=", remove_clst1_pp);

static bool __is_cpu_op_invalid(struct clk_core *core, struct cpu_opt *cop)
{
	int i;
	unsigned int df_max_cpurate = core->params->max_cpurate;
	unsigned int index;

	/*
	 * If pclk > default support max core frequency, invalid it
	 */
	if (df_max_cpurate && (cop->pclk > df_max_cpurate))
		return true;

	/*
	 * Also ignore the PP if it is disabled from uboot cmd.
	 */
	i = (!strcmp(core->hw.clk->name, CLST0_CORE_CLK_NAME)) ? 0 : 1;
	if (pp_discnt[i]) {
		for (index = 0; index < pp_discnt[i]; index++)
			if (pp_disable[i][index] == cop->pclk)
				return true;
	}
	return false;
}

static void __init_fc_setting(struct core_params *core_params)
{
	unsigned int regval;
	union pmua_cc_cp cc_cp;
	void __iomem *apmu_base = core_params->apmu_base;
	void __iomem *dciu_base = core_params->dciu_base;
	/*
	 * enable AP FC done interrupt for one step,
	 * while not use three interrupts by three steps
	 */
	writel((1 << 1), APMU_IMR(apmu_base));

	/* always vote for CP allow AP FC */
	cc_cp.v = readl(APMU_CP_CCR(apmu_base));
	cc_cp.b.core_allow_spd_chg = 1;
	writel(cc_cp.v, APMU_CP_CCR(apmu_base));

	regval = readl(APMU_DEBUG(apmu_base));
	/* DE: Should be set to 1 as AP will NOT send out ACK */
	regval |= (1 << 1);
	/*
	 * Always set AP_WFI_FC and CP_WFI_FC, then PMU will
	 * automaticlly send out clk-off ack when core is WFI
	 */
	regval |= (1 << 21) | (1 << 22);
	/*
	 * mask CP clk-off ack and cp halt ack for DDR/AXI FC
	 * this bits should be unmasked after cp is released
	 */
	regval |= (1 << 0) | (1 << 3);
	writel(regval, APMU_DEBUG(apmu_base));

	/* clock dynamic gate enable in CKG_CTRL */
	regval = readl(DCIU_CKG(dciu_base));
	writel(regval | 0xF, DCIU_CKG(dciu_base));

	/* clock dynamic gate enable in PMU CLK_GATE_CTRL */
	regval = readl(APMU_CLK_GATE(apmu_base));
	regval &= ~(
		(1 << 0) |	/* ref clock */
		(1 << 2) |	/* cp core clock */
		(1 << 4) |	/* ap core clock */
		(1 << 8) |	/* axi clock */
		(1 << 10) |	/* cp core idle SM */
		(1 << 12) |	/* ap core idle SM */
		(1 << 14));	/* ap subsys idle SM */
	writel(regval, APMU_CLK_GATE(apmu_base));
}

static struct clk *hwsel2parent(struct parents_table *parent_table,
				int parent_table_size, u32 src_sel)
{
	int i;
	for (i = 0; i < parent_table_size; i++) {
		if (parent_table[i].hw_sel_val == src_sel)
			return parent_table[i].parent;
	}
	BUG_ON(i == parent_table_size);
	return NULL;
}

static void clk_cpu_init(struct clk_hw *hw)
{
	int mclk_type = 0;
	unsigned int op_index;
	struct clk *parent;
	struct parents_table *parent_table;
	int i, parent_table_size;
	struct cpu_opt cur_op, *op, *cop, *cur_cpu_op = NULL;
	struct clk_core *core = to_clk_core(hw);
	unsigned int pp[MAX_OP_NUM];
	struct list_head *op_list = NULL;

	if (!strcmp(hw->clk->name, CLST0_CORE_CLK_NAME)) {
		op_list = &apc0_core_op_list;
		mclk_type = CLST0;
	} else if (!strcmp(hw->clk->name, CLST1_CORE_CLK_NAME)) {
		op_list = &apc1_core_op_list;
		mclk_type = CLST1;
	}

	parent_table = core->params->parent_table;
	parent_table_size = core->params->parent_table_size;
	pr_info("%-20s%-12s%-16s",
		"pclk(src:sel,div)", "c_aclk(div)", "l1_xtc:l2_xtc");
	for (i = 0; i < core->params->cpu_opt_size; i++) {
		cop = &core->params->cpu_opt[i];
		parent = hwsel2parent(parent_table,
				      parent_table_size, cop->ap_clk_sel);
		BUG_ON(IS_ERR(parent));
		cop->parent = parent;
		if (!cop->ap_clk_src)
			cop->ap_clk_src = clk_get_rate(parent) / MHZ;
		/* check the invalid condition of this op */
		if (__is_cpu_op_invalid(core, cop))
			continue;
		/* fill the opt related setting */
		__init_cpu_rtcwtc(hw, cop);
		cop->pclk_div = cop->ap_clk_src / cop->pclk - 1;
		cop->core_aclk_div = cop->pclk / cop->core_aclk - 1;

		pr_info("%-4d(%-4d:%-d,%-d)%6s%-4d(%-d)%5s0x%08x:0x%08x\n",
			cop->pclk, cop->ap_clk_src, cop->ap_clk_sel,
			cop->pclk_div, " ", cop->core_aclk, cop->core_aclk_div, " ",
			cop->l1_xtc, cop->l2_xtc);

		/* add it into core op list */
		list_add_tail(&core->params->cpu_opt[i].node, op_list);
		if (cop->pclk == core->params->bridge_cpurate) {
			if (!strcmp(hw->clk->name, CLST0_CORE_CLK_NAME))
				clst0_bridge_op = cop;
			else if (!strcmp(hw->clk->name, CLST1_CORE_CLK_NAME))
				clst1_bridge_op = cop;
		}
	}

	/* get cur core rate */
	op = list_first_entry(op_list, struct cpu_opt, node);
	cur_op = *op;
	get_cur_cpu_op(hw, &cur_op);
	__init_fc_setting(core->params);
	cur_cpu_op = cpu_rate2_op_ptr(cur_op.pclk, &op_index, op_list);
	if ((cur_op.ap_clk_src != cur_cpu_op->ap_clk_src) ||
	    (cur_op.pclk != cur_cpu_op->pclk))
		WARN_ON("Boot CPU PP is not supported!");
	hw->clk->rate = cur_cpu_op->pclk * MHZ;
	hw->clk->parent = cur_cpu_op->parent;

	/* config the wtc/rtc value according to current frequency */
	if (cur_cpu_op->l1_xtc)
		writel(cur_cpu_op->l1_xtc, cur_cpu_op->l1_xtc_addr);
	if (cur_cpu_op->l2_xtc)
		writel(cur_cpu_op->l2_xtc, cur_cpu_op->l2_xtc_addr);

	if (!strcmp(hw->clk->name, CLST0_CORE_CLK_NAME))
		cur_apc0_cpu_op = cur_cpu_op;
	else if (!strcmp(hw->clk->name, CLST1_CORE_CLK_NAME))
		cur_apc1_cpu_op = cur_cpu_op;

	/* support dc_stat? */
	if (core->params->dcstat_support) {
		i = 0;
		list_for_each_entry(cop, op_list, node) {
			pp[i] = cop->pclk;
			i++;
		}
		register_cpu_dcstat(hw->clk, CORES_PER_CLUSTER, pp, i,
				    core->params->pxa_powermode, MULTI_CLUSTER);
		cpu_dcstat_clk = hw->clk;
	}

#ifdef CONFIG_CPU_FREQ
	__init_cpufreq_table(hw);
#endif

	/* we call *clk_set_rate* in cci_min_notify defined in clk-pxa1936.c */
	pm_qos_add_request(&cci_qos_req_min[mclk_type], PM_QOS_CCI_MIN,
			   PM_QOS_DEFAULT_VALUE);
	trigger_slave_clk_fc(mclk_type, hw->clk->rate);

	pr_info("%s boot up @%luHZ\n", core->hw.clk->name, hw->clk->rate);
}

static long clk_cpu_round_rate(struct clk_hw *hw, unsigned long rate,
			       unsigned long *parent_rate)
{
	struct cpu_opt *cop;
	struct list_head *op_list = NULL;

	if (!strcmp(hw->clk->name, CLST0_CORE_CLK_NAME))
		op_list = &apc0_core_op_list;
	else if (!strcmp(hw->clk->name, CLST1_CORE_CLK_NAME))
		op_list = &apc1_core_op_list;

	rate /= MHZ;
	list_for_each_entry(cop, op_list, node) {
		if ((cop->pclk >= rate) || list_is_last(&cop->node, op_list))
			break;
	}
	return cop->pclk * MHZ;
}

static int clk_cpu_setrate(struct clk_hw *hw, unsigned long rate,
			   unsigned long parent_rate)
{
	int mclk_type = 0;
	struct cpu_opt *md_new, *md_old, *bridge_op = NULL, *cur_cpu_op = NULL;
	unsigned int index;
	int ret = 0;
	struct clk_core *core = to_clk_core(hw);
	int cpu, j;
	struct list_head *op_list = NULL;

	if (!strcmp(hw->clk->name, CLST0_CORE_CLK_NAME)) {
		bridge_op = clst0_bridge_op;
		cur_cpu_op = cur_apc0_cpu_op;
		op_list = &apc0_core_op_list;
		mclk_type = CLST0;
	} else if (!strcmp(hw->clk->name, CLST1_CORE_CLK_NAME)) {
		bridge_op = clst1_bridge_op;
		cur_cpu_op = cur_apc1_cpu_op;
		op_list = &apc1_core_op_list;
		mclk_type = CLST1;
	}

	rate /= MHZ;
	md_new = cpu_rate2_op_ptr(rate, &index, op_list);
	if (md_new == cur_cpu_op)
		return 0;

	md_old = cur_cpu_op;

	get_fc_spinlock();

	if ((md_old->ap_clk_sel == md_new->ap_clk_sel) &&
	    (md_new->ap_clk_src != md_old->ap_clk_src)) {
		/* 1) if undefined bridge_op, set op0 as bridge */
		if (unlikely(!bridge_op))
			bridge_op = list_first_entry(op_list,
						     struct cpu_opt, node);
		/* 2) change core to bridge_op */
		ret = set_core_freq(hw, md_old, bridge_op);
		if (ret) {
			put_fc_spinlock();
			goto tmpout;
		}
		/* 3) change parent's rate */
		clk_set_rate(md_new->parent, md_new->ap_clk_src * MHZ);
		/* 4) switch to target op */
		ret = set_core_freq(hw, bridge_op, md_new);
	} else {
		clk_set_rate(md_new->parent, md_new->ap_clk_src * MHZ);
		ret = set_core_freq(hw, md_old, md_new);
	}

	put_fc_spinlock();

	trigger_slave_clk_fc(mclk_type, rate * MHZ);

tmpout:
	if (ret)
		goto out;

	if (!strcmp(hw->clk->name, CLST0_CORE_CLK_NAME))
		cur_apc0_cpu_op = md_new;
	else if (!strcmp(hw->clk->name, CLST1_CORE_CLK_NAME))
		cur_apc1_cpu_op = md_new;

	__clk_reparent(hw->clk, md_new->parent);

	if (core->params->dcstat_support) {
		if (!strcmp(hw->clk->name, CLST0_CORE_CLK_NAME)) {
			for (j = 0; j < 4; j++) {
				cpu = j;
			    cpu_dcstat_event(hw->clk, cpu, CLK_RATE_CHANGE,
					     index);
			}
		} else if (!strcmp(hw->clk->name, CLST1_CORE_CLK_NAME)) {
			for (j = 4; j < 8; j++) {
				cpu = j;
			    cpu_dcstat_event(hw->clk, cpu, CLK_RATE_CHANGE,
					     index);
			}
		}
	}
out:
	return ret;
}

static unsigned long clk_cpu_recalc_rate(struct clk_hw *hw,
					 unsigned long parent_rate)
{
	struct cpu_opt *cur_cpu_op = NULL;

	if (!strcmp(hw->clk->name, CLST0_CORE_CLK_NAME))
		cur_cpu_op = cur_apc0_cpu_op;
	else if (!strcmp(hw->clk->name, CLST1_CORE_CLK_NAME))
		cur_cpu_op = cur_apc1_cpu_op;

	if (cur_cpu_op) {
		hw->clk->rate = cur_cpu_op->pclk * MHZ;
		return cur_cpu_op->pclk * MHZ;
	} else
		pr_err("%s: cur_cpu_op NULL\n", __func__);

	return 0;
}

static u8 clk_cpu_get_parent(struct clk_hw *hw)
{
	struct clk *parent, *clk = hw->clk;
	u8 i = 0;

	parent = __get_core_parent(hw, NULL);
	WARN_ON(!parent);
	if (parent) {
		for (i = 0; i < clk->num_parents; i++) {
			if (!strcmp(clk->parent_names[i], parent->name))
				break;
		}
	}
	if (i == clk->num_parents) {
		pr_err("%s: Cannot find parent for cpu!\n", __func__);
		BUG_ON(1);
	}
	return i;
}

struct clk_ops cpu_clk_ops = {
	.init = clk_cpu_init,
	.round_rate = clk_cpu_round_rate,
	.set_rate = clk_cpu_setrate,
	.recalc_rate = clk_cpu_recalc_rate,
	.get_parent = clk_cpu_get_parent,
};

struct clk *mmp_clk_register_core(const char *name, const char **parent_name,
				  u8 num_parents, unsigned long flags,
				  u32 core_flags, spinlock_t *lock,
				  struct core_params *params)
{
	struct clk_core *core;
	struct clk *clk;
	struct clk_init_data init;

	core = kzalloc(sizeof(*core), GFP_KERNEL);
	if (!core)
		return NULL;

	init.name = name;
	init.ops = &cpu_clk_ops;
	init.flags = flags;
	init.parent_names = parent_name;
	init.num_parents = num_parents;

	core->flags = core_flags;
	core->lock = lock;
	core->params = params;
	core->hw.init = &init;

	clk = clk_register(NULL, &core->hw);
	if (IS_ERR(clk))
		kfree(core);

	return clk;
}

static unsigned int ddr_rate2_op_index(struct clk_hw *hw,
			unsigned int rate)
{
	unsigned int index;
	struct clk_ddr *ddr = to_clk_ddr(hw);
	struct ddr_opt *ddr_opt;
	unsigned int ddr_opt_size;
	ddr_opt = ddr->params->ddr_opt;
	ddr_opt_size = ddr->params->ddr_opt_size;

	if (unlikely(rate > ddr_opt[ddr_opt_size - 1].dclk))
		return ddr_opt_size - 1;

	for (index = 0; index < ddr_opt_size; index++)
		if (ddr_opt[index].dclk >= rate)
			break;

	return index;
}

static inline bool check_hwdfc_inpro(void __iomem *apmu_base,
				     unsigned int expected_lvl)
{
	union dfc_status status;
	int max_delay = 200;

	while (max_delay) {
		status.v = readl(DFC_STATUS(apmu_base));
		if ((expected_lvl <= status.b.cfl) && (!status.b.dfc_status))
			return false;
		udelay(5);
		max_delay--;
	}

	return true;
}

static int ddr_hwdfc_seq(struct clk_hw *hw, unsigned int level)
{
	union dfc_ap dfc_ap;
	union pmua_cc cc_ap;
	struct clk_ddr *ddr = to_clk_ddr(hw);
	void __iomem *apmu_base = ddr->params->apmu_base;
	void __iomem *apmuccr = APMU_CCR(apmu_base);
	bool inpro = false;
	union dfc_status status;
	int max_delay = 100;

	/* AP votes allow FC */
	cc_ap.v = readl(apmuccr);
	cc_ap.b.ap_allow_spd_chg = 1;
	writel(cc_ap.v, apmuccr);

	/* wait for DFC triggered by CP/MSA is done */
	status.v = readl(DFC_STATUS(apmu_base));
	while (max_delay && status.b.dfc_status) {
		udelay(10);
		max_delay--;
		status.v = readl(DFC_STATUS(apmu_base));
	}
	if (unlikely(max_delay <= 0)) {
		WARN(1, "AP cannot start HWDFC as DFC is in progress!\n");
		pr_err("DFCAP %x, DFCCP %x, DFCSTATUS %x,\n",
		       readl(DFC_AP(apmu_base)),
		       readl(DFC_CP(apmu_base)), readl(DFC_STATUS(apmu_base)));
		return -EAGAIN;
	}

	/* Check if AP ISR is set, if set, clear it */
	pre_fc(apmu_base);

	/* trigger AP HWDFC */
	dfc_ap.v = readl(DFC_AP(apmu_base));
	dfc_ap.b.fl = level;
	dfc_ap.b.dfc_req = 1;
	writel(dfc_ap.v, DFC_AP(apmu_base));

	/* Check dfc status and done */
	inpro = check_hwdfc_inpro(apmu_base, level);
	if (likely(!inpro))
		wait_for_fc_done(DDR_FC, apmu_base);
	else {
		WARN(1, "HW-DFC failed! expect LV %d\n", level);
		pr_err
		    ("DFCAP %x, DFCCP %x, DFCSTATUS %x, PLLSEL %x, DMCCAP %x\n",
		     readl(DFC_AP(apmu_base)), readl(DFC_CP(apmu_base)),
		     readl(DFC_STATUS(apmu_base)),
		     readl(APMU_PLL_SEL_STATUS(apmu_base)),
		     get_dm_cc_ap(apmu_base));
	}
	return 0;
}

static int set_hwdfc_freq(struct clk_hw *hw, struct ddr_opt *old,
			  struct ddr_opt *new)
{
	unsigned long flags;
	int ret = 0;

	pr_debug("DDR set_freq start: old %u, new %u\n", old->dclk, new->dclk);

	clk_prepare_enable(new->ddr_parent);
	trace_pxa_ddr_clk_chg(CLK_CHG_ENTRY, old->dclk, new->dclk);
	local_irq_save(flags);
	ret = ddr_hwdfc_seq(hw, new->ddr_freq_level);
	if (unlikely(ret == -EAGAIN)) {
		/* still stay at old freq and src */
		local_irq_restore(flags);
		clk_disable_unprepare(new->ddr_parent);
		goto out;
	}
	local_irq_restore(flags);
	trace_pxa_ddr_clk_chg(CLK_CHG_EXIT, old->dclk, new->dclk);
	clk_disable_unprepare(old->ddr_parent);

	pr_debug("DDR set_freq end: old %u, new %u\n", old->dclk, new->dclk);
out:
	return ret;
}

static inline void get_ddr_srcdiv(struct clk_ddr *ddr,
				  unsigned int *srcsel, unsigned int *div)
{
	void __iomem *apmu_base = ddr->params->apmu_base;
	union pmua_pllsel pllsel;
	union pmua_dm_cc dm_cc_ap;

	pllsel.v = readl(APMU_PLL_SEL_STATUS(apmu_base));
	dm_cc_ap.v = get_dm_cc_ap(apmu_base);
	pr_debug("div%x sel%x\n", dm_cc_ap.v, pllsel.v);
	*srcsel = pllsel.b.ddrclksel;
	*div = dm_cc_ap.b.ddr_clk_div + 1;
}

static void get_cur_ddr_op(struct clk_hw *hw, struct ddr_opt *cop)
{
	struct clk *parent;
	struct clk_ddr *ddr = to_clk_ddr(hw);
	struct parents_table *parent_table;
	int size;
	unsigned int src_sel, div;

	BUG_ON(!cop->ddr_parent);

	get_ddr_srcdiv(ddr, &src_sel, &div);
	if (likely(src_sel == cop->ddr_clk_sel))
		cop->ddr_clk_src = clk_get_rate(cop->ddr_parent) / MHZ;
	else {
		parent_table = ddr->params->parent_table;
		size = ddr->params->parent_table_size;
		parent = hwsel2parent(parent_table, size, src_sel);
		cop->ddr_parent = parent;
		cop->ddr_clk_sel = src_sel;
		cop->ddr_clk_src = clk_get_rate(parent) / MHZ;
		pr_err("%s ddr clk tsrc:%d csel:%d parent:%s\n",
		       __func__, cop->ddr_clk_src,
		       src_sel, cop->ddr_parent->name);
	}

	cop->dclk = cop->ddr_clk_src / div / 2;
}

#ifdef CONFIG_DDR_DEVFREQ
static struct devfreq_frequency_table *ddr_devfreq_tbl;

static void __init_ddr_devfreq_table(struct clk_hw *hw)
{
	struct ddr_opt *ddr_opt;
	unsigned int ddr_opt_size = 0, i = 0;
	struct clk_ddr *ddr = to_clk_ddr(hw);

	ddr_opt_size = ddr->params->ddr_opt_size;
	ddr_devfreq_tbl = kmalloc(sizeof(struct devfreq_frequency_table)
				  * (ddr_opt_size + 1), GFP_KERNEL);
	if (!ddr_devfreq_tbl)
		return;

	ddr_opt = ddr->params->ddr_opt;
	for (i = 0; i < ddr_opt_size; i++) {
		ddr_devfreq_tbl[i].index = i;
		ddr_devfreq_tbl[i].mode = 2 << (ddr_opt[i].mode_4x_en);
		ddr_devfreq_tbl[i].frequency = ddr_opt[i].dclk * MHZ_TO_KHZ;
	}

	ddr_devfreq_tbl[i].index = i;
	ddr_devfreq_tbl[i].mode = 0;
	ddr_devfreq_tbl[i].frequency = DEVFREQ_TABLE_END;

	devfreq_frequency_table_register(ddr_devfreq_tbl, DEVFREQ_DDR);
}
#endif

static inline void hwdfc_init(struct clk_ddr *ddr,
			      struct ddr_opt *cop, unsigned int lvlx)
{
	u32 regval;
	void __iomem *reg = DFC_LEVEL(ddr->params->apmu_base, lvlx);
	union dfc_lvl dfc_lvl;

	dfc_lvl.v = readl(reg);
	dfc_lvl.b.dclkmode = cop->mode_4x_en;
	dfc_lvl.b.dclksrcsel = cop->ddr_clk_sel;
	dfc_lvl.b.dclkdiv = cop->dclk_div;
	dfc_lvl.b.mctblnum = cop->ddr_tbl_index;
	dfc_lvl.b.mclpmtblnum = cop->ddr_lpmtbl_index;
	dfc_lvl.b.reqvl = 0;
	regval = dfc_lvl.v;
	writel(regval, reg);
}

static inline void hwdfc_initvl(struct clk_ddr *ddr,
				struct ddr_opt *cop, unsigned int lvlx)
{
	u32 regval;
	void __iomem *reg = DFC_LEVEL(ddr->params->apmu_base, lvlx);
	union dfc_lvl dfc_lvl;

	dfc_lvl.v = readl(reg);
	dfc_lvl.b.reqvl = cop->ddr_volt_level;
	regval = dfc_lvl.v;
	writel(regval, reg);
}

static void clk_ddr_init(struct clk_hw *hw)
{
	struct clk *parent;
	struct clk_ddr *ddr = to_clk_ddr(hw);
	struct ddr_opt *ddr_opt, *cop, cur_op;
	unsigned int ddr_opt_size = 0, i;
	unsigned int op_index;
	struct parents_table *parent_table = ddr->params->parent_table;
	int parent_table_size = ddr->params->parent_table_size;
	unsigned long op[MAX_OP_NUM], idx;
#ifdef CONFIG_PXA_DVFS
	int nr_volt_level;
	const unsigned long *freq_vl_tbl = NULL;
#endif

	ddr_opt = ddr->params->ddr_opt;
	ddr_opt_size = ddr->params->ddr_opt_size;

	pr_info("dclk(src:sel,div,tblindex)\n");
	for (i = 0; i < ddr_opt_size; i++) {
		cop = &ddr_opt[i];
		cop->ddr_freq_level = i;
		parent = hwsel2parent(parent_table, parent_table_size,
				      cop->ddr_clk_sel);
		BUG_ON(IS_ERR(parent));
		cop->ddr_parent = parent;
		cop->ddr_clk_src = clk_get_rate(parent) / MHZ;
		cop->dclk_div = cop->ddr_clk_src / (2 * cop->dclk) - 1;

		pr_info("%d(%d:%d,%d,%d)\n",
			cop->dclk, cop->ddr_clk_src,
			cop->ddr_clk_sel, cop->dclk_div, cop->ddr_tbl_index);

		/* hwdfc init */
		hwdfc_init(ddr, cop, i);
	}

	cur_op = ddr_opt[0];
	get_cur_ddr_op(hw, &cur_op);
	op_index = ddr_rate2_op_index(hw, cur_op.dclk);
	cur_ddr_op = &ddr_opt[op_index];
	if ((cur_op.ddr_clk_src != cur_ddr_op->ddr_clk_src) ||
	    (cur_op.dclk != cur_ddr_op->dclk))
		WARN_ON("Boot DDR PP is not supported!");

	/*
	 * HW thinks default DFL is 0, we have to make sure HW
	 * get the correct DFL by first change it to 0, then change
	 * it to current DFL
	 */
	ddr_hwdfc_seq(hw, 0);
	ddr_hwdfc_seq(hw, op_index);
	/*
	 * Fill dvc level in DFC_LEVEL, this will not trigger dvc
	 * Level change since default level is 0 for all DFC_LEVEL regs
	 */
#ifdef CONFIG_PXA_DVFS
	nr_volt_level = dvfs_get_svc_freq_table(&freq_vl_tbl, hw->clk->name);
#endif
	for (i = 0; i < ddr_opt_size; i++) {
		cop = &ddr_opt[i];
#ifdef CONFIG_PXA_DVFS
		cop->ddr_volt_level = get_dvc_level(cop->dclk *  MHZ_TO_KHZ, freq_vl_tbl, nr_volt_level);
#endif
		hwdfc_initvl(ddr, cop, i);
	}

	hw->clk->rate = ddr_opt[op_index].dclk * MHZ;
	pr_info(" DDR boot up @%luHZ\n", hw->clk->rate);

	if (ddr->params->dcstat_support) {
		idx = 0;
		for (i = 0; i < ddr_opt_size; i++) {
			cop = &ddr_opt[i];
			op[idx++] = cop->dclk * MHZ;
		}
		clk_register_dcstat(hw->clk, op, idx);
	}

	clk_dclk = hw->clk;
#ifdef CONFIG_DDR_DEVFREQ
	__init_ddr_devfreq_table(hw);
#endif
}

static long clk_ddr_round_rate(struct clk_hw *hw, unsigned long rate,
			       unsigned long *parent_rate)
{
	unsigned int index;
	struct clk_ddr *ddr = to_clk_ddr(hw);
	struct ddr_opt *ddr_opt;
	unsigned int ddr_opt_size;
	ddr_opt = ddr->params->ddr_opt;
	ddr_opt_size = ddr->params->ddr_opt_size;

	rate /= MHZ;

	if (unlikely(rate > ddr_opt[ddr_opt_size - 1].dclk))
		return ddr_opt[ddr_opt_size - 1].dclk * MHZ;

	for (index = 0; index < ddr_opt_size; index++)
		if (ddr_opt[index].dclk >= rate)
			break;

	return ddr_opt[index].dclk * MHZ;
}

static int clk_ddr_setrate(struct clk_hw *hw, unsigned long rate,
			   unsigned long parent_rate)
{
	struct ddr_opt *md_new, *md_old;
	unsigned int index;
	struct ddr_opt *ddr_opt;
	struct clk_ddr *ddr = to_clk_ddr(hw);
	int ret = 0;
	ddr_opt = ddr->params->ddr_opt;

	rate /= MHZ;
	index = ddr_rate2_op_index(hw, rate);

	md_new = &ddr_opt[index];
	if (md_new == cur_ddr_op)
		return 0;

	mutex_lock(&ddraxi_freqs_mutex);
	md_old = cur_ddr_op;

	get_fc_spinlock();
	ret = set_hwdfc_freq(hw, md_old, md_new);
	put_fc_spinlock();
	if (ret) {
		mutex_unlock(&ddraxi_freqs_mutex);
		goto out;
	}
	cur_ddr_op = md_new;

	mutex_unlock(&ddraxi_freqs_mutex);

	trigger_slave_clk_fc(DDR, rate * MHZ);
	__clk_reparent(hw->clk, md_new->ddr_parent);

out:
	return ret;
}

static unsigned long clk_ddr_recalc_rate(struct clk_hw *hw,
					 unsigned long parent_rate)
{
	struct clk_ddr *ddr = to_clk_ddr(hw);
	void __iomem *apmu_base = ddr->params->apmu_base;
	union dfc_status dfc_status;
	struct ddr_opt *ddr_opt = ddr->params->ddr_opt;

	if (cur_ddr_op)
		return cur_ddr_op->dclk * MHZ;
	else {
		dfc_status.v = readl(DFC_STATUS(apmu_base));
		return ddr_opt[dfc_status.b.cfl].dclk * MHZ;
	}

	return 0;
}

static u8 clk_ddr_get_parent(struct clk_hw *hw)
{
	struct clk *parent, *clk = hw->clk;
	struct clk_ddr *ddr = to_clk_ddr(hw);
	struct parents_table *parent_table;
	int parent_table_size;
	u8 i = 0;
	u32 src_sel, div;

	parent_table = ddr->params->parent_table;
	parent_table_size = ddr->params->parent_table_size;

	get_ddr_srcdiv(ddr, &src_sel, &div);

	for (i = 0; i < parent_table_size; i++) {
		if (parent_table[i].hw_sel_val == src_sel)
			break;
	}
	if (i == parent_table_size) {
		pr_err("%s: Cannot find parent for ddr!\n", __func__);
		BUG_ON(1);
	}
	parent = parent_table[i].parent;
	WARN_ON(!parent);
	if (parent) {
		for (i = 0; i < clk->num_parents; i++) {
			if (!strcmp(clk->parent_names[i], parent->name))
				break;
		}
	}
	return i;
}

struct clk_ops ddr_clk_ops = {
	.init = clk_ddr_init,
	.round_rate = clk_ddr_round_rate,
	.set_rate = clk_ddr_setrate,
	.recalc_rate = clk_ddr_recalc_rate,
	.get_parent = clk_ddr_get_parent,
};

struct clk *mmp_clk_register_ddr(const char *name, const char **parent_name,
				 u8 num_parents, unsigned long flags,
				 u32 ddr_flags, spinlock_t *lock,
				 struct ddr_params *params)
{
	struct clk_ddr *ddr;
	struct clk *clk;
	struct clk_init_data init;

	ddr = kzalloc(sizeof(*ddr), GFP_KERNEL);
	if (!ddr)
		return NULL;

	init.name = name;
	init.ops = &ddr_clk_ops;
	init.flags = flags;
	init.parent_names = parent_name;
	init.num_parents = num_parents;

	ddr->flags = ddr_flags;
	ddr->lock = lock;
	ddr->params = params;
	ddr->hw.init = &init;

	clk = clk_register(NULL, &ddr->hw);
	if (IS_ERR(clk))
		kfree(ddr);

	return clk;
}

/* AXI */
static inline void set_axi_clk_sel(struct clk_axi *axi, struct axi_opt *top)
{
	void __iomem *selreg;
	unsigned int selvalue, value;

	selreg = MPMU_FCACLK(axi);
	selvalue = top->axi_clk_sel;
	writel(selvalue, selreg);

	value = readl(selreg);
	if (value != selvalue)
		pr_err
		    ("AXI FCCR Write failure: target 0x%x, final value 0x%X\n",
		     selvalue, value);
}

static inline void trigger_axi_fc(struct clk_axi *axi, struct axi_opt *top)
{
	union pmua_cc cc_ap;
	void __iomem *apmu_base = axi->params->apmu_base;
	void __iomem *apmuccr = APMU_CCR(apmu_base);

	cc_ap.v = readl(apmuccr);
	cc_ap.b.bus_clk_div = top->aclk_div;
	cc_ap.b.bus_freq_chg_req = 1;
	/* AP votes allow FC */
	cc_ap.b.ap_allow_spd_chg = 1;
	pr_debug("AXI FC APMU_CCR[%x]\n", cc_ap.v);
	writel(cc_ap.v, apmuccr);

	wait_for_fc_done(AXI_FC, apmu_base);

	cc_ap.v = readl(apmuccr);
	cc_ap.b.bus_freq_chg_req = 0;
	writel(cc_ap.v, apmuccr);
}

static void axi_fc_seq(struct clk_axi *axi, struct axi_opt *cop,
		       struct axi_opt *top)
{
	void __iomem *apmu_base = axi->params->apmu_base;
	unsigned int needchg;

	needchg = (cop->axi_clk_src != top->axi_clk_src) ||
	    (cop->aclk != top->aclk);
	if (!needchg)
		return;

	/* update rtc/wtc if neccessary, PP low -> high */
	if ((cop->aclk < top->aclk) && (top->xtc_val != cop->xtc_val))
		writel(top->xtc_val, top->xtc_addr);

	trace_pxa_axi_clk_chg(CLK_CHG_ENTRY, cop->aclk, top->aclk);

	pre_fc(apmu_base);
	set_axi_clk_sel(axi, top);
	trigger_axi_fc(axi, top);

	trace_pxa_axi_clk_chg(CLK_CHG_EXIT, cop->aclk, top->aclk);

	/*  update rtc/wtc if neccessary, high -> low */
	if ((cop->aclk > top->aclk) && (top->xtc_val != cop->xtc_val))
		writel(top->xtc_val, top->xtc_addr);
}

static inline void get_axi_srcdiv(struct clk_axi *axi,
				  unsigned int *srcsel, unsigned int *div)
{
	void __iomem *apmu_base = axi->params->apmu_base;
	union pmua_pllsel pllsel;
	union pmua_dm_cc dm_cc_ap;

	pllsel.v = readl(APMU_PLL_SEL_STATUS(apmu_base));
	dm_cc_ap.v = get_dm_cc_ap(apmu_base);
	pr_debug("div%x sel%x\n", dm_cc_ap.v, pllsel.v);
	*srcsel = pllsel.b.axiclksel;
	*div = dm_cc_ap.b.bus_clk_div + 1;
}

static void get_cur_axi_op(struct clk_hw *hw, struct axi_opt *cop)
{
	struct clk *parent;
	struct parents_table *parent_table;
	int parent_table_size;
	struct clk_axi *axi = to_clk_axi(hw);
	u32 axiclksel, axiclkdiv;

	BUG_ON(!cop->axi_parent);

	get_axi_srcdiv(axi, &axiclksel, &axiclkdiv);
	if (likely(axiclksel == cop->axi_clk_sel))
		cop->axi_clk_src = clk_get_rate(cop->axi_parent) / MHZ;
	else {
		parent_table = axi->params->parent_table;
		parent_table_size = axi->params->parent_table_size;
		parent = hwsel2parent(parent_table,
				      parent_table_size, axiclksel);
		cop->axi_parent = parent;
		cop->axi_clk_sel = axiclksel;
		cop->axi_clk_src = clk_get_rate(parent) / MHZ;
		pr_err("%s axi clk tsrc:%d csel:%d parent:%s\n",
		       __func__, cop->axi_clk_src, axiclksel,
		       cop->axi_parent->name);
	}
	cop->aclk = cop->axi_clk_src / axiclkdiv;
}

static unsigned int axi_rate2_op_index(struct clk_hw *hw, unsigned int rate)
{
	unsigned int index;
	struct clk_axi *axi = to_clk_axi(hw);
	struct axi_opt *axi_opt;
	unsigned int axi_opt_size;
	axi_opt = axi->params->axi_opt;
	axi_opt_size = axi->params->axi_opt_size;

	if (unlikely(rate > axi_opt[axi_opt_size - 1].aclk))
		return axi_opt_size - 1;

	for (index = 0; index < axi_opt_size; index++)
		if (axi_opt[index].aclk >= rate)
			break;

	return index;
}

static int set_axi_freq(struct clk_hw *hw, struct axi_opt *old,
			struct axi_opt *new)
{
	struct axi_opt cop;
	struct clk *axi_old_parent;
	int ret = 0;
	unsigned long flags;
	struct clk_axi *axi = to_clk_axi(hw);
	void __iomem *apmu_base = axi->params->apmu_base;

	pr_debug("AXI set_freq start: old %u, new %u\n", old->aclk, new->aclk);

	cop = *old;
	get_cur_axi_op(hw, &cop);
	if (unlikely((cop.axi_clk_src != old->axi_clk_src) ||
		     (cop.aclk != old->aclk))) {
		pr_err(" asrc aclk");
		pr_err("OLD %d %d\n", old->axi_clk_src, old->aclk);
		pr_err("CUR %d %d\n", cop.axi_clk_src, cop.aclk);
		pr_err("NEW %d %d\n", new->axi_clk_src, new->aclk);
		dump_stack();
	}

	axi_old_parent = cop.axi_parent;
	clk_prepare_enable(new->axi_parent);

	/* Get lock in irq disable status to short AP hold lock time */
	local_irq_save(flags);
	ret = get_fc_lock(apmu_base, 0);
	if (ret) {
		put_fc_lock(apmu_base);
		local_irq_restore(flags);
		clk_disable_unprepare(new->axi_parent);
		goto out;
	}
	axi_fc_seq(axi, &cop, new);
	put_fc_lock(apmu_base);
	local_irq_restore(flags);

	cop = *new;
	get_cur_axi_op(hw, &cop);

	if (unlikely((cop.axi_clk_src != new->axi_clk_src) ||
		     (cop.aclk != new->aclk))) {
		clk_disable(new->axi_parent);
		pr_err("AXI:unsuccessful frequency change!\n");
		pr_err(" asrc aclk");
		pr_err("CUR %d %d\n", cop.axi_clk_src, cop.aclk);
		pr_err("NEW %d %d\n", new->axi_clk_src, new->aclk);
		pr_err("FCACLK %x, CCAP %x, PLLSEL %x, DMCCAP %x, CCCP %x\n",
		       readl(MPMU_FCACLK(axi)),
		       readl(APMU_CCR(apmu_base)),
		       readl(APMU_PLL_SEL_STATUS(apmu_base)),
		       get_dm_cc_ap(apmu_base), readl(APMU_CP_CCR(apmu_base)));
		/* restore current src */
		set_axi_clk_sel(axi, &cop);
		pr_info("Recovered FCACLK: %x\n", readl(MPMU_FCACLK(axi)));
		ret = -EAGAIN;
		goto out;
	}

	clk_disable_unprepare(axi_old_parent);
	pr_debug("AXI set_freq end: old %u, new %u\n", old->aclk, new->aclk);
out:
	return ret;
}

static void __init __init_axi_rtcwtc(struct clk_hw *hw, struct axi_opt *axi_opt)
{
	unsigned int size, index;
	struct clk_axi *axi = to_clk_axi(hw);
	struct axi_rtcwtc *axi_rtcwtc_table = axi->params->axi_rtcwtc_table;
	size = axi->params->axi_rtcwtc_table_size;

	if (!axi_rtcwtc_table || !size)
		return;

	for (index = 0; index < size; index++)
		if (axi_opt->aclk <= axi_rtcwtc_table[index].max_aclk)
			break;

	if (index == size)
		index = size - 1;

	axi_opt->xtc_val = axi_rtcwtc_table[index].xtc_val;
	axi_opt->xtc_addr = CIU_TOP_MEM_XTC(axi);
}

static void clk_axi_init(struct clk_hw *hw)
{
	struct clk *parent;
	struct clk_axi *axi = to_clk_axi(hw);
	struct axi_opt *axi_opt, *cop, cur_op;
	unsigned int axi_opt_size = 0, i;
	unsigned int op_index;
	struct parents_table *parent_table;
	int parent_table_size;
	unsigned long op[MAX_OP_NUM], idx;

	axi_opt = axi->params->axi_opt;
	axi_opt_size = axi->params->axi_opt_size;
	parent_table = axi->params->parent_table;
	parent_table_size = axi->params->parent_table_size;

	pr_info("aclk(src:sel,div)\n");
	for (i = 0; i < axi_opt_size; i++) {
		cop = &axi_opt[i];
		parent = hwsel2parent(parent_table, parent_table_size,
				      cop->axi_clk_sel);
		BUG_ON(IS_ERR(parent));
		/* fill the opt related setting */
		__init_axi_rtcwtc(hw, cop);
		cop->axi_parent = parent;
		cop->axi_clk_src = clk_get_rate(parent) / MHZ;
		cop->aclk_div = cop->axi_clk_src / cop->aclk - 1;
		pr_info("%u(%d:%d,%u)\n",
			cop->aclk, cop->axi_clk_src,
			cop->axi_clk_sel, cop->aclk_div);
	}

	cur_op = axi_opt[0];
	get_cur_axi_op(hw, &cur_op);
	op_index = axi_rate2_op_index(hw, cur_op.aclk);
	cur_axi_op = &axi_opt[op_index];
	if ((cur_op.axi_clk_src != cur_axi_op->axi_clk_src) ||
	    (cur_op.aclk != cur_axi_op->aclk))
		WARN_ON("Boot AXI PP is not supported!");

	/* config the wtc/rtc value according to current frequency */
	if (cur_axi_op->xtc_val)
		writel(cur_axi_op->xtc_val, cur_axi_op->xtc_addr);

	if (axi->params->dcstat_support) {
		idx = 0;
		for (i = 0; i < axi_opt_size; i++) {
			cop = &axi_opt[i];
			op[idx++] = cop->aclk * MHZ;
		}
		clk_register_dcstat(hw->clk, op, idx);
	}

	hw->clk->rate = axi_opt[op_index].aclk * MHZ;
	hw->clk->parent = axi_opt[op_index].axi_parent;
	pr_info(" AXI boot up @%luHZ\n", hw->clk->rate);
}

static long clk_axi_round_rate(struct clk_hw *hw, unsigned long rate,
			       unsigned long *parent_rate)
{
	unsigned int index;
	struct clk_axi *axi = to_clk_axi(hw);
	struct axi_opt *axi_opt;
	unsigned int axi_opt_size;
	axi_opt = axi->params->axi_opt;
	axi_opt_size = axi->params->axi_opt_size;

	rate /= MHZ;

	if (unlikely(rate > axi_opt[axi_opt_size - 1].aclk))
		return axi_opt[axi_opt_size - 1].aclk * MHZ;

	for (index = 0; index < axi_opt_size; index++)
		if (axi_opt[index].aclk >= rate)
			break;

	return axi_opt[index].aclk * MHZ;
}

static unsigned long clk_axi_recalc_rate(struct clk_hw *hw,
					 unsigned long parent_rate)
{
	if (cur_axi_op)
		return cur_axi_op->aclk * MHZ;
	else
		pr_err("%s: cur_axi_op NULL\n", __func__);

	return 0;
}

static u8 clk_axi_get_parent(struct clk_hw *hw)
{
	struct clk *parent, *clk = hw->clk;
	struct parents_table *parent_table;
	int parent_table_size;
	u32 i = 0, src, div;
	struct clk_axi *axi = to_clk_axi(hw);
	parent_table = axi->params->parent_table;
	parent_table_size = axi->params->parent_table_size;

	get_axi_srcdiv(axi, &src, &div);
	for (i = 0; i < parent_table_size; i++) {
		if (parent_table[i].hw_sel_val == src)
			break;
	}
	if (i == parent_table_size) {
		pr_err("%s: Cannot find parent for axi !\n", __func__);
		BUG_ON(1);
	}
	parent = parent_table[i].parent;
	WARN_ON(!parent);
	if (parent) {
		for (i = 0; i < clk->num_parents; i++) {
			if (!strcmp(clk->parent_names[i], parent->name))
				break;
		}
	}
	return i;
}

static int clk_axi_setrate(struct clk_hw *hw, unsigned long rate,
			   unsigned long parent_rate)
{
	struct axi_opt *md_new, *md_old;
	unsigned int index;
	int ret = 0;
	struct clk_axi *axi = to_clk_axi(hw);
	struct axi_opt *op_array = axi->params->axi_opt;

	rate /= MHZ;
	index = axi_rate2_op_index(hw, rate);

	md_new = &op_array[index];
	if (md_new == cur_axi_op)
		return 0;

	mutex_lock(&ddraxi_freqs_mutex);
	md_old = cur_axi_op;

	get_fc_spinlock();
	ret = set_axi_freq(hw, md_old, md_new);
	put_fc_spinlock();
	if (ret)
		goto out;

	cur_axi_op = md_new;
	__clk_reparent(hw->clk, md_new->axi_parent);

out:
	mutex_unlock(&ddraxi_freqs_mutex);
	return ret;
}

struct clk_ops axi_clk_ops = {
	.init = clk_axi_init,
	.round_rate = clk_axi_round_rate,
	.set_rate = clk_axi_setrate,
	.recalc_rate = clk_axi_recalc_rate,
	.get_parent = clk_axi_get_parent,
};

struct clk *mmp_clk_register_axi(const char *name, const char **parent_name,
				 u8 num_parents, unsigned long flags,
				 u32 axi_flags, spinlock_t *lock,
				 struct axi_params *params)
{
	struct clk_axi *axi;
	struct clk *clk;
	struct clk_init_data init;

	axi = kzalloc(sizeof(*axi), GFP_KERNEL);
	if (!axi)
		return NULL;

	init.name = name;
	init.ops = &axi_clk_ops;
	init.flags = flags;
	init.parent_names = parent_name;
	init.num_parents = num_parents;

	axi->flags = axi_flags;
	axi->lock = lock;
	axi->params = params;
	axi->hw.init = &init;

	clk = clk_register(NULL, &axi->hw);
	if (IS_ERR(clk))
		kfree(axi);

	return clk;
}

#define CCI_BUS_DIV_SHIFT	(8)
#define CCI_BUS_DIV_MASK	(0x7)
static struct clk *clk_cci_mem;
void __iomem *global_apmu_base;

static int cci_min_notify(struct notifier_block *b,
				unsigned long min, void *v)
{
	int ret = 0;
	unsigned int regval, bus_clk_div;

	bus_clk_div = ((min == 832000) ? 4 : 2);
	bus_clk_div -= 1;	/* bus_clk = mem_clk / (this field +1) */
	regval = readl(APMU_CCI(global_apmu_base));
	regval &= ~(CCI_BUS_DIV_MASK << CCI_BUS_DIV_SHIFT);
	regval |= (bus_clk_div << CCI_BUS_DIV_SHIFT);
	writel(regval, APMU_CCI(global_apmu_base));

	ret = clk_set_rate(clk_cci_mem, min * KHZ_TO_HZ);
	if (ret) {
		pr_err("%s failed to change cci mem_clk to %lu MHz\n", __func__, min);
		return NOTIFY_BAD;
	}
	return NOTIFY_OK;
}

static struct notifier_block cci_min_notifier = {
	.notifier_call = cci_min_notify,
};

void register_cci_notifier(void __iomem *apmu_base, struct clk *clk)
{
	global_apmu_base = apmu_base;
	clk_cci_mem = clk;
	pm_qos_add_notifier(PM_QOS_CCI_MIN, &cci_min_notifier);
}

/*
 * Interface used by telephony
 * cp_holdcp:
 * 1) acquire_fc_mutex
 * 2) hold CP (write APRR)
 * 3) mask the cp halt and clk-off of debug register
 * 4) release_fc_mutex
 * cp_releasecp:
 * 1) acquire_fc_mutex
 * 2) clear the cp halt and clk-off of debug register
 * 3) Write APRR to release CP from reset
 * 4) wait 10ms
 * 5) release_fc_mutex
 */
void acquire_fc_mutex(void)
{
	mutex_lock(&ddraxi_freqs_mutex);
}
EXPORT_SYMBOL(acquire_fc_mutex);

/* called after release cp */
void release_fc_mutex(void)
{
	mutex_unlock(&ddraxi_freqs_mutex);
}
EXPORT_SYMBOL(release_fc_mutex);

/* debug feature */
#ifdef CONFIG_DEBUG_FS
static ssize_t dfcstatus_read(struct file *filp,
			      char __user *buffer, size_t count, loff_t *ppos)
{
	char buf[1000];
	struct clk_ddr *dclk = to_clk_ddr(clk_dclk->hw);
	void __iomem *apmu_base = dclk->params->apmu_base;
	struct ddr_opt *ddr_opt = dclk->params->ddr_opt;
	int ddr_opt_size = dclk->params->ddr_opt_size, len = 0, idx;
	union dfc_status dfcstatus;
	union dfc_cp dfc_cp;
	union dfc_ap dfc_ap;
	union dfc_lvl dfc_lvl;
	unsigned int conf, src, div, tblnum, vl, mode;
	size_t size = sizeof(buf) - 1;

	ddr_opt = dclk->params->ddr_opt;
	ddr_opt_size = dclk->params->ddr_opt_size;

	dfcstatus.v = readl(DFC_STATUS(apmu_base));
	len += snprintf(buf + len, size,
			"|HW_DFC:\t| Curlvl: %d(%4uMHz),\tTgtlvl: %d,\tIn_Pro: %d|\n",
			dfcstatus.b.cfl, ddr_opt[dfcstatus.b.cfl].dclk,
			dfcstatus.b.tfl, dfcstatus.b.dfc_status);

	dfc_ap.v = readl(DFC_AP(apmu_base));
	len += snprintf(buf + len, size,
			"|DFC_AP:\t| Active: %u,\t\tReq: %u|\n", dfc_ap.b.fl,
			dfc_ap.b.dfc_req);

	dfc_cp.v = readl(DFC_CP(apmu_base));
	len += snprintf(buf + len, size,
			"|DFC_CP(E):%d\t| Active: %d,\t\tLpm(E): %d(%d) |\n",
			!dfc_cp.b.dfc_disable, dfc_cp.b.freq_level,
			dfc_cp.b.lpm_lvl, dfc_cp.b.lpm_en);

	len += snprintf(buf + len, size,
			"|PPidx\t|Freq\t|Src\t|div\t|Tbidx\t|VL\t|\n");

	for (idx = 0; idx < ddr_opt_size; idx++) {
		conf = readl(DFC_LEVEL(apmu_base, idx));
		dfc_lvl.v = conf;
		src = dfc_lvl.b.dclksrcsel;
		div = dfc_lvl.b.dclkdiv;
		tblnum = dfc_lvl.b.mctblnum;
		mode = dfc_lvl.b.dclkmode;
		vl = dfc_lvl.b.reqvl;
		len += snprintf(buf + len, size,
				"|%u\t|%u\t|%u\t|%u\t|%u\t|%u\t|%u\t|\n", idx,
				ddr_opt[idx].dclk, src, div, tblnum, vl, mode);
	};

	return simple_read_from_buffer(buffer, count, ppos, buf, len);
}

/* Get dfc status */
const struct file_operations dfcstatus_fops = {
	.read = dfcstatus_read,
};

static int __init dfc_create_debug_node(void)
{
	struct dentry *dfc_status;

	if (!clk_dclk)
		return 0;

	dfc_status = debugfs_create_file("dfcstatus", 0444,
					 NULL, NULL, &dfcstatus_fops);
	if (!dfc_status)
		return -ENOENT;
	return 0;
}

late_initcall(dfc_create_debug_node);
#endif
