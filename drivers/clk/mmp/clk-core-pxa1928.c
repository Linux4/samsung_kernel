/*
 * pxa1928 core clock framework source file
 *
 * Copyright (C) 2013 Marvell
 * Yipeng Yao <ypyao@marvell.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/of_address.h>
#include <linux/cpufreq.h>
#include <linux/devfreq.h>
#include <linux/pm_qos.h>

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/clk-private.h>

#include <dt-bindings/clock/marvell-pxa1928.h>
#include <linux/clk/mmpdcstat.h>

#include "clk.h"
#include "clk-plat.h"

#define APMU_COREAPSS_CLKCTRL		(0x2E0)
#define APMU_BUS	(0x06c)
#define APMU_CC_PJ	(0x004)
#define APMU_CC2_PJ	(0x150)
#define APMU_PLL_SEL_STATUS     (0x00c4)
#define APMU_DM_CC_PJ	(0x0c)
#define APMU_DM2_CC	(0x154)
#define APMU_CC_SP	(0x0)
#define APMU_DEBUG	(0x088)
#define APMU_DEBUG2	(0x390)
#define APMU_MC_CLK_RES_CTRL	(0x258)
#define APMU_MC_MEM_CTRL	(0x198)
#define APMU_MC_CLK_STAT	(0x264)
#define APMU_CORE_PWRMODE_BASE	(0x280)
#define APMU_ACLK11_CLKCTRL	(0x120)

#define APMU_DDRDFC_CTRL	(0x0)
#define DFC_LEVEL(base, i)	(base + 0x100 + i * 8)

#define DEFAULT_CPU_MAX_SPEED	1600000
#define MAX_OP_NUM 10
#define DDR_HWDFC_FEAT BIT(0)
static void __iomem *apmu_coreapss_clkctrl;

static void __iomem *apmu_pll_sel_status;
static void __iomem *apmu_dm_cc_pj;
static void __iomem *apmu_dm2_cc;
static void __iomem *apmu_cc_pj;
static void __iomem *apmu_cc2_pj;
static void __iomem *apmu_bus;
static void __iomem *apmu_ddrdfc_ctrl;
static void __iomem *hwdfc_base;
static void __iomem *apmu_aclk11_clkctrl;

#if 0
#define AXI_DDR_COUPLE_DFC
#endif
static void __iomem *apmu_cc_sp;
static void __iomem *apmu_debug;
static void __iomem *apmu_debug2;
static void __iomem *apmu_mc_clk_res_ctrl;
static void __iomem *apmu_mc_clk_stat;
static void __iomem *apmu_mc_mem_ctrl;
static void __iomem *apmu_core_pwrmode[4];

/* bit definition of APMU_COREAPSS_CLKCTRL */
#define COREFREQ_RTCWTC_SHIFT		(27)
#define COREFREQ_AXIM_SHIFT		(8)
#define COREFREQ_FCEN_SHIFT		(3)
/* end */

#define APMU_COREAPSS_CFG		(0x39C)
#define COREAPSS_CFG_WTCRTC_MSK		(0xFFFFFF00)
#define COREAPSS_CFG_WTCRTC_NZ1_HMIPS	(0x50505000)
#define COREAPSS_CFG_WTCRTC_Z2_EHMIPS	(0x50506000)
#define COREAPSS_CFG_WTCRTC_AX_EHMIPS	(0x50506000)

#define KHZ_TO_HZ	(1000)
#define MHZ_TO_KHZ	(1000)

#define to_clk_ddr(ddr) container_of(ddr, struct clk_ddr, hw)
static DEFINE_SPINLOCK(cpu_clk_lock);
static void __iomem *apmu_coreapss_cfg;
static struct clk *pll2_clk, *pll2p_clk;
static int core_idx_restore;
static int corefreq_src_shift;
static int corefreq_src_msk;
static int discrete_ddr;

union ddrdfc_fp_lwr {
	struct {
		unsigned int ddr_vl:4;
		unsigned int mc_reg_tbl_sel:5;
		unsigned int rtcwtc_sel:1;
		unsigned int sel_4xmode:1;
		unsigned int sel_ddrpll:1;
		unsigned int reserved:20;
	} b;
	unsigned int v;
};

union ddrdfc_fp_upr {
	struct {
		unsigned int fp_upr0:3;
		unsigned int fp_upr1:5;
		unsigned int fp_upr2:7;
		unsigned int fp_upr3:3;
		unsigned int fp_upr4:1;
		unsigned int fp_upr5:3;
		unsigned int reserved:10;
	} b;
	unsigned int v;
};

union ddrdfc_ctrl {
	struct {
		unsigned int dfc_hw_en:1;
		unsigned int ddr_type:1;
		unsigned int ddrpll_ctrl:1;
		unsigned int dfc_freq_tbl_en:1;
		unsigned int reserved0:4;
		unsigned int dfc_fp_idx:5;
		unsigned int dfc_pllpark_idx:5;
		unsigned int reserved1:2;
		unsigned int dfc_fp_index_cur:5;
		unsigned int reserved2:3;
		unsigned int dfc_in_progress:1;
		unsigned int dfc_int_clr:1;
		unsigned int dfc_int_msk:1;
		unsigned int dfc_int;
	} b;
	unsigned int v;
};


union bus_res_ctrl {
	struct {
		unsigned int mcrst:1;
		unsigned int reserved3:5;
		unsigned int axiclksel:3;
		unsigned int reserved2:3;
		unsigned int dmaliteaxires:1;
		unsigned int dmaliteaxien:1;
		unsigned int reserved1:2;
		unsigned int axi1skip:1;
		unsigned int axi2skip:1;
		unsigned int axi3skip:1;
		unsigned int axi4skip:1;
		unsigned int axi5skip:1;
		unsigned int axi6skip:1;
		unsigned int ahbskip:1;
		unsigned int axi1skipratio:1;
		unsigned int axi2skipratio:1;
		unsigned int axi3skipratio:1;
		unsigned int axi4skipratio:1;
		unsigned int axi5skipratio:1;
		unsigned int axi6skipratio:1;
		unsigned int ahbskipratio:1;
		unsigned int reserved0:2;
	} b;
	unsigned int v;
};

union cc_pj {
	struct {
		unsigned int reserved3:15;
		unsigned int axi_div:3;
		unsigned int reserved2:9;
		unsigned int pj_ddrfc_vote:1;
		unsigned int reserved1:2;
		unsigned int aclk_fc_req:1;
		unsigned int reserved0:1;
	} b;
	unsigned int v;
};

union cc2_pj {
	struct {
		unsigned int axi2_div:3;
		unsigned int axi3_div:3;
		unsigned int axi4_div:3;
		unsigned int reserved:23;
	} b;
	unsigned int v;
};

union pll_sel_status {
	struct {
		unsigned int sp_pll_sel:3;
		unsigned int reserved1:6;
		unsigned int soc_axi_pll_sel:3;
		unsigned int reserved0:20;
	} b;
	unsigned int v;
};

union dm_cc_pj {
	struct {
		unsigned int reserved2:15;
		unsigned int soc_axi_clk_div:3;
		unsigned int reserved1:6;
		unsigned int sp_hw_semaphore:1;
		unsigned int pj_hw_semaphore:1;
		unsigned int reserved0:6;
	} b;
	unsigned int v;
};

union dm2_cc {
	struct {
		unsigned int soc_axi_clk2_div:3;
		unsigned int soc_axi_clk3_div:3;
		unsigned int soc_axi_clk4_div:3;
		unsigned int reserved0:23;
	} b;
	unsigned int v;
};

union mc_res_ctrl {
	struct {
		unsigned int mc_sw_rstn:1;
		unsigned int pmu_ddr_sel:2;
		unsigned int pmu_ddr_div:3;
		unsigned int reserved1:3;
		unsigned int mc_dfc_type:2;
		unsigned int mc_reg_table_en:1;
		unsigned int pll4vcodivselse:3;
		unsigned int mc_reg_table:5;
		unsigned int reserved0:3;
		unsigned int rtcwtc_sel:1;
		unsigned int reserved2:4;
		unsigned int mc_clk_dfc:1;
		unsigned int mc_clk_sfc:1;
		unsigned int mc_clk_src_sel:1;
		unsigned int mc_4x_mode:1;
	} b;
	unsigned int v;
};

union mc_clk_stat {
	struct {
		unsigned int reserved2:1;
		unsigned int pmu_ddr_clk_sel:2;
		unsigned int pmu_ddr_clk_div:3;
		unsigned int reserved1:6;
		unsigned int pll4_vcodiv_sel_se:3;
		unsigned int reserved0:15;
		unsigned int mc_clk_src_sel:1;
		unsigned int mc_4x_mode:1;
	} b;
	unsigned int v;
};

union pmua_aclk11_clkctrl {
	struct {
		unsigned int clk_div:3;
		unsigned int reserved0:1;
		unsigned int clksrc_sel:3;
		unsigned int reserved1:2;
		unsigned int fc_en:1;
		unsigned int reserved2:22;
	} b;
	unsigned int v;
};

enum dclk_sel {
	DDR_PLL1_416 = 0,
	DDR_PLL1_624,
	DDR_PLL1_1248,
	DDR_PLL5,
	/* DDR PLL4 no div, fake one to allign w/ pp table form */
	DDR_PLL4,
};

enum aclk_sel {
	AXI_PLL1_312 = 0,
	AXI_PLL1_624,
	AXI_PLL2,
	AXI_PLL1_416,
	AXI_VCXO,
};

enum aclk11_sel {
	AXI11_PLL1_624,
	AXI11_PLL2,
	AXI11_PLL1_416,
	AXI11_PLL5,
};

struct clk_ddr {
	struct clk_hw hw;
	u32 flags;
};

struct pll_freq_tb {
	unsigned long refclk;
	u32 bw_sel:1;
	u32 icp:4;
	u32 kvco:4;
	u32 refdiv:9;
	u32 fbdiv:9;
	u32 freq_offset_en:1;
	unsigned long fvco;
	unsigned long fvco_offseted;
	u32 bypass_en:1;
	u32 clk_src_sel:1;
	u32 post_div_sel:3;
	unsigned long pll_clkout;
	u32 diff_post_div_sel:3;
	unsigned long pll_clkoutp;
	/* multi-function bit */
	u32 mystical_bit:1;
};

static struct pll_freq_tb pll4_freq_tb_pop[] = {
	{
		.refclk = 26000000,
		.bw_sel = 0,
		.icp = 3,
		.kvco = 0xc,
		.refdiv = 3,
		.fbdiv = 61, /* 528mhz */
		.freq_offset_en = 1,
		.fvco = 2114666666ul,
		.fvco_offseted = 2133333333ul,
		.bypass_en = 0,
		.clk_src_sel = 1,
		.post_div_sel = 1,
		/* after fvco offseted, it should be 1066666666 */
		.pll_clkout = 1057333333ul,
		.diff_post_div_sel = 2,
		.pll_clkoutp = 528666666,
		/* select post div from ddr register */
		.mystical_bit = 1,
	},
	{
		.refclk = 26000000,
		.bw_sel = 0,
		.icp = 3,
		.kvco = 0xc,
		.refdiv = 3,
		.fbdiv = 77, /* 667mhz */
		.freq_offset_en = 1,
		.fvco = 2114666666ul,
		.fvco_offseted = 2133333333ul,
		.bypass_en = 0,
		.clk_src_sel = 1,
		.post_div_sel = 1,
		/* after fvco offseted, it should be 1066666666 */
		.pll_clkout = 1057333333ul,
		.diff_post_div_sel = 2,
		.pll_clkoutp = 528666666,
		/* select post div from ddr register */
		.mystical_bit = 1,
	},
	{
		.refclk = 26000000,
		.bw_sel = 0,
		.icp = 3,
		.kvco = 0xc,
		.refdiv = 3,
		.fbdiv = 92, /* 797mhz */
		.freq_offset_en = 1,
		.fvco = 2114666666ul,
		.fvco_offseted = 2133333333ul,
		.bypass_en = 0,
		.clk_src_sel = 1,
		.post_div_sel = 1,
		/* after fvco offseted, it should be 1066666666 */
		.pll_clkout = 1057333333ul,
		.diff_post_div_sel = 2,
		.pll_clkoutp = 528666666,
		/* select post div from ddr register */
		.mystical_bit = 1,
	},
};

static struct pll_freq_tb pll4_freq_tb_dis[] = {
#ifdef DISCRETE_4L
		{
		.refclk = 26000000,
		.bw_sel = 0,
		.icp = 3,
		.kvco = 0xc,
		.refdiv = 3,
		.fbdiv = 61, /* 528mhz */
		.freq_offset_en = 1,
		.fvco = 2114666666ul,
		.fvco_offseted = 2133333333ul,
		.bypass_en = 0,
		.clk_src_sel = 1,
		.post_div_sel = 1,
		/* after fvco offseted, it should be 1066666666 */
		.pll_clkout = 1057333333ul,
		.diff_post_div_sel = 2,
		.pll_clkoutp = 528666666,
		/* select post div from ddr register */
		.mystical_bit = 1,
	},
#else
	{
		.refclk = 26000000,
		.bw_sel = 0,
		.icp = 3,
		.kvco = 0xc,
		.refdiv = 3,
		.fbdiv = 58, /* 502mhz */
		.freq_offset_en = 1,
		.fvco = 2114666666ul,
		.fvco_offseted = 2133333333ul,
		.bypass_en = 0,
		.clk_src_sel = 1,
		.post_div_sel = 1,
		/* after fvco offseted, it should be 1066666666 */
		.pll_clkout = 1057333333ul,
		.diff_post_div_sel = 2,
		.pll_clkoutp = 528666666,
		/* select post div from ddr register */
		.mystical_bit = 1,
	},
#endif
	{
		.refclk = 26000000,
		.bw_sel = 0,
		.icp = 3,
		.kvco = 0xc,
		.refdiv = 3,
		.fbdiv = 72, /* 624mhz */
		.freq_offset_en = 1,
		.fvco = 2114666666ul,
		.fvco_offseted = 2133333333ul,
		.bypass_en = 0,
		.clk_src_sel = 1,
		.post_div_sel = 1,
		/* after fvco offseted, it should be 1066666666 */
		.pll_clkout = 1057333333ul,
		.diff_post_div_sel = 2,
		.pll_clkoutp = 528666666,
		/* select post div from ddr register */
		.mystical_bit = 1,
	},
	{
		.refclk = 26000000,
		.bw_sel = 0,
		.icp = 3,
		.kvco = 0xc,
		.refdiv = 3,
		.fbdiv = 77, /* 667mhz */
		.freq_offset_en = 1,
		.fvco = 2114666666ul,
		.fvco_offseted = 2133333333ul,
		.bypass_en = 0,
		.clk_src_sel = 1,
		.post_div_sel = 1,
		/* after fvco offseted, it should be 1066666666 */
		.pll_clkout = 1057333333ul,
		.diff_post_div_sel = 2,
		.pll_clkoutp = 528666666,
		/* select post div from ddr register */
		.mystical_bit = 1,
	},

};


enum core_clk_sel {
	CORE_PLL1 = 0,
	CORE_PLL2,
	CORE_PLL2P,
	RESERVE,
};


static enum core_clk_sel core_src_ax_maptbl[4] = {
	CORE_PLL1, RESERVE, CORE_PLL2, RESERVE,
};

struct pxa1928_core_opt {
	unsigned int freq_khz;
	enum core_clk_sel src;
	unsigned int div;
	unsigned int rtc_wtc;
	unsigned int axim_div;
	unsigned int src_freq;
	unsigned int src_sel;
};

struct pxa1928_axi11_opt {
	unsigned int aclk;
	unsigned int axi_clk_sel;
	unsigned int aclk_div;
};

struct pxa1928_axi_opt {
	unsigned int aclk1;	/* axi clock */
	unsigned int aclk2;	/* axi clock */
	unsigned int aclk3;	/* axi clock */
	unsigned int aclk4;	/* axi clock */
	enum aclk_sel axi_clk_sel;	/* axi src sel val */
	unsigned int aclk_div1;	/* axi clk divider */
	unsigned int aclk_div2;	/* axi clk divider */
	unsigned int aclk_div3;	/* axi clk divider */
	unsigned int aclk_div4;	/* axi clk divider */
};

struct pxa1928_ddr_opt {
	unsigned int dclk;		/* ddr clock */
	unsigned int ddr_tbl_index;	/* ddr FC table index */
	enum dclk_sel ddr_clk_sel;	/* ddr src sel val */
	unsigned int dclk_div;		/* ddr clk divider */
	unsigned int src_freq;
	unsigned int ddr_freq_level;
	unsigned int rtcwtc;
	unsigned int rtcwtc_lvl;
	unsigned int axi_constraint;
};

static struct pxa1928_core_opt *cur_core_opt;
static int cur_core_opt_size;
static enum core_clk_sel *core_src_maptbl;
static unsigned int cpu_max_freq = DEFAULT_CPU_MAX_SPEED;

static unsigned int pll2_oparray[] = {
	797000, 1057000, 1196000, 1386000, 1508000,
};

static struct pxa1928_core_opt core_oparray_a0[] = {
	{156000, CORE_PLL1,	4, 0, 2, 624000, 0},
	{312000, CORE_PLL1,	2, 0, 2, 624000, 0},
	{398000, CORE_PLL2,	2, 0, 2, 797000, 2},
	{624000, CORE_PLL1,	1, 0, 2, 624000, 0},
	{797000, CORE_PLL2,	1, 1, 2, 797000, 2},
	{1057000, CORE_PLL2,	1, 1, 2, 1057000, 2},
	{1196000, CORE_PLL2,	1, 1, 2, 1196000, 2},
	{1386000, CORE_PLL2,	1, 1, 2, 1386000, 2},
	{1508000, CORE_PLL2,	1, 1, 3, 1508000, 2},
};

static struct pxa1928_axi11_opt axi11_oparray[] = {
	{
	 .aclk = 83000,
	 .axi_clk_sel = AXI11_PLL1_416,
	 .aclk_div = 5,
	},
	{
	 .aclk = 104000,
	 .axi_clk_sel = AXI11_PLL1_416,
	 .aclk_div = 4,
	},
	{
	 .aclk = 156000,
	 .axi_clk_sel = AXI11_PLL1_624,
	 .aclk_div = 4,
	},
	{
	 .aclk = 208000,
	 .axi_clk_sel = AXI11_PLL1_416,
	 .aclk_div = 2,
	},
	{
	 .aclk = 312000,
	 .axi_clk_sel = AXI11_PLL1_624,
	 .aclk_div = 2,
	},
	{
	 .aclk = 416000,
	 .axi_clk_sel = AXI11_PLL1_416,
	 .aclk_div = 1,
	},
};

static struct pxa1928_axi_opt axi_oparray[] = {
	{
	 .aclk1 = 78000,
	 .aclk2 = 78000,
	 .aclk3 = 78000,
	 .aclk4 = 78000,
	 .axi_clk_sel = AXI_PLL1_624,
	 .aclk_div1 = 7,
	 .aclk_div2 = 7,
	 .aclk_div3 = 7,
	 .aclk_div4 = 7,
	 },
	{
	 .aclk1 = 104000,
	 .aclk2 = 104000,
	 .aclk3 = 104000,
	 .aclk4 = 104000,
	 .axi_clk_sel = AXI_PLL1_416,
	 .aclk_div1 = 3,
	 .aclk_div2 = 3,
	 .aclk_div3 = 3,
	 .aclk_div4 = 3,
	 },
	{
	 .aclk1 = 156000,
	 .aclk2 = 156000,
	 .aclk3 = 156000,
	 .aclk4 = 156000,
	 .axi_clk_sel = AXI_PLL1_624,
	 .aclk_div1 = 3,
	 .aclk_div2 = 3,
	 .aclk_div3 = 3,
	 .aclk_div4 = 3,
	 },
	{
	 .aclk1 = 208000,
	 .aclk2 = 208000,
	 .aclk3 = 208000,
	 .aclk4 = 208000,
	 .axi_clk_sel = AXI_PLL1_416,
	 .aclk_div1 = 1,
	 .aclk_div2 = 1,
	 .aclk_div3 = 1,
	 .aclk_div4 = 1,
	 },
	{
	 .aclk1 = 312000,
	 .aclk2 = 312000,
	 .aclk3 = 312000,
	 .aclk4 = 312000,
	 .axi_clk_sel = AXI_PLL1_624,
	 .aclk_div1 = 1,
	 .aclk_div2 = 1,
	 .aclk_div3 = 1,
	 .aclk_div4 = 1,
	 },
};

static struct pxa1928_axi_opt *cur_axi_opt;
static struct pxa1928_axi_opt *cur_axi_op;
static struct pxa1928_axi11_opt *cur_axi11_op;
static int cur_axi_opt_size;
static int cur_axi11_opt_size;
static struct pxa1928_ddr_opt *cur_ddr_op;
static struct pxa1928_ddr_opt *cur_ddr_opt;
static int cur_ddr_opt_size;
#ifdef AXI_DDR_COUPLE_DFC
static struct pm_qos_request axi_qos_req_min;
#endif

static struct clk *pll4_clk;
static DEFINE_SPINLOCK(ddr_fc_seq_lock);
static DEFINE_SPINLOCK(axi_fc_seq_lock);
static DEFINE_SPINLOCK(axi11_fc_seq_lock);
static DEFINE_SPINLOCK(cc_pj_access_lock);


static inline void core_set_rwtc(int rwtc, int idx)
{
	u32 val;
	struct pxa1928_core_opt *core_op_array;

	core_op_array = cur_core_opt;

	val = readl(apmu_coreapss_cfg);
	val &= ~(COREAPSS_CFG_WTCRTC_MSK);
	if (core_op_array[idx].freq_khz > 1100000)
		val |= (COREAPSS_CFG_WTCRTC_AX_EHMIPS);
	else
		val |= (COREAPSS_CFG_WTCRTC_NZ1_HMIPS);
	writel(val, apmu_coreapss_cfg);
}

static void core_setrate_idx(int idx)
{
	int div, rwtc, axim, src_sel;
	u32 reg;
	struct pxa1928_core_opt *core_op_array;

	core_op_array = cur_core_opt;
	src_sel = core_op_array[idx].src_sel;
	div = core_op_array[idx].div;
	rwtc = core_op_array[idx].rtc_wtc;
	axim = core_op_array[idx].axim_div;

	if (idx > core_idx_restore) {
		core_set_rwtc(rwtc, idx);

		reg = readl(apmu_coreapss_clkctrl);
		reg &= ~(0x7 << COREFREQ_AXIM_SHIFT);
		reg |= (axim << COREFREQ_AXIM_SHIFT);
		writel(reg, apmu_coreapss_clkctrl);
		/* dummy read */
		reg = readl(apmu_coreapss_clkctrl);
	}

	reg = readl(apmu_coreapss_clkctrl);
	reg &= ~(corefreq_src_msk << corefreq_src_shift);
	reg |= (src_sel << corefreq_src_shift);
	reg &= ~0x7;
	reg |= div;
	writel(reg, apmu_coreapss_clkctrl);
	/* dummy read */
	reg = readl(apmu_coreapss_clkctrl);

	if (idx < core_idx_restore) {
		reg = readl(apmu_coreapss_clkctrl);
		reg &= ~(0x7 << COREFREQ_AXIM_SHIFT);
		reg |= (axim << COREFREQ_AXIM_SHIFT);
		writel(reg, apmu_coreapss_clkctrl);
		/* dummy read */
		reg = readl(apmu_coreapss_clkctrl);

		core_set_rwtc(rwtc, idx);
	}

	core_idx_restore = idx;
}

static inline void jump_to_tmp_pll1(int idx)
{
	int i;
	struct pxa1928_core_opt *core_op_array;

	core_op_array = cur_core_opt;

	if (idx <= 0)
		return;
	for (i = idx - 1; i >= 0; i--) {
		if (core_op_array[i].src == CORE_PLL1)
			break;
	}
	core_setrate_idx(i);
	return;
}

static int cpu_clk_setrate(struct clk_hw *hw, unsigned long rate,
				unsigned long prate)
{
	u32 reg;
	int i, src, cur_src, cur_src_sel;
	int ret = 0;
	unsigned long flags;
	struct pxa1928_core_opt *core_op_array;
	unsigned int core_opt_size = 0;
	unsigned int cpu;

	if (!pll2_clk || !pll2p_clk) {
		ret = -EINVAL;
		goto cpu_clk_setrate_err;
	}

	core_op_array = cur_core_opt;
	core_opt_size = cur_core_opt_size;

	/* kHz align */
	rate /= KHZ_TO_HZ;
	pr_debug("[Core DFC]-enter: target value: %lukHZ...\n", rate);

	spin_lock_irqsave(&cpu_clk_lock, flags);
	reg = readl(apmu_coreapss_clkctrl);
	cur_src_sel = (reg >> corefreq_src_shift) & corefreq_src_msk;
	cur_src = core_src_maptbl[cur_src_sel];

	for (i = 0; i < core_opt_size; i++) {
		if (core_op_array[i].freq_khz == rate)
			break;
	}
	src = core_op_array[i].src;

	switch (cur_src) {
	case CORE_PLL1:
		if (src == CORE_PLL2) {
			clk_set_rate(pll2_clk,
				     core_op_array[i].src_freq * 1000);
			clk_prepare_enable(pll2_clk);
		} else if (src == CORE_PLL2P) {
			clk_set_rate(pll2p_clk,
				     core_op_array[i].src_freq * 1000);
			clk_prepare_enable(pll2p_clk);
		}
		core_setrate_idx(i);
		break;
	case CORE_PLL2:
		if (src == CORE_PLL1) {
			core_setrate_idx(i);
			clk_disable_unprepare(pll2_clk);
		} else if (src == CORE_PLL2P) {
			jump_to_tmp_pll1(i);
			clk_disable_unprepare(pll2_clk);
			clk_set_rate(pll2p_clk,
				     core_op_array[i].src_freq * 1000);
			clk_prepare_enable(pll2p_clk);
			core_setrate_idx(i);
		} else if (src == CORE_PLL2) {
			jump_to_tmp_pll1(i);
			clk_disable_unprepare(pll2_clk);
			clk_set_rate(pll2_clk,
				     core_op_array[i].src_freq * 1000);
			clk_prepare_enable(pll2_clk);
			core_setrate_idx(i);
		}
		break;
	case CORE_PLL2P:
		if (src == CORE_PLL1) {
			core_setrate_idx(i);
			clk_disable_unprepare(pll2p_clk);
		} else if (src == CORE_PLL2) {
			jump_to_tmp_pll1(i);
			clk_disable_unprepare(pll2p_clk);
			clk_set_rate(pll2_clk,
				     core_op_array[i].src_freq * 1000);
			clk_prepare_enable(pll2_clk);
			core_setrate_idx(i);
		}
		break;
	}
	spin_unlock_irqrestore(&cpu_clk_lock, flags);

	for_each_possible_cpu(cpu)
		cpu_dcstat_event(cpu_dcstat_clk, cpu, CLK_RATE_CHANGE, i);


cpu_clk_setrate_err:
	if (ret)
		pr_debug("[Core DFC]-exit: %lukHZ change FAIL\n", rate);
	else
		pr_debug("[Core DFC]-exit: %lukHZ change SUCCESS\n", rate);

	return ret;
}

static inline unsigned int pll2_round_rate(unsigned int raw_rate)
{
	const int gap = 20000;
	int i;

	for (i = 0; i < ARRAY_SIZE(pll2_oparray); i++) {
		if ((raw_rate > (pll2_oparray[i] - gap))
			&& (raw_rate < (pll2_oparray[i]) + gap))
			return pll2_oparray[i];
	}
	return 0;
}

static unsigned long cpu_clk_recalc(struct clk_hw *hw, unsigned long prate)
{
	int i, src, div, src_sel;
	unsigned int pll2_rate;
	unsigned int ret = 0;
	unsigned long flags;
	u32 reg;
	struct pxa1928_core_opt *core_op_array;
	unsigned int core_opt_size = 0;

	core_op_array = cur_core_opt;
	core_opt_size = cur_core_opt_size;

	spin_lock_irqsave(&cpu_clk_lock, flags);
	reg = readl(apmu_coreapss_clkctrl);
	src_sel = (reg >> corefreq_src_shift) & corefreq_src_msk;
	src = core_src_maptbl[src_sel];
	div = reg & 0x7;

	switch (src) {
	case CORE_PLL1:
	case CORE_PLL2P:
		for (i = 0; i < core_opt_size; i++) {
			if ((src == core_op_array[i].src) &&
				(div == core_op_array[i].div)) {
				ret = core_op_array[i].freq_khz;
				break;
			}
		}
		break;
	case CORE_PLL2:
		if (!pll2_clk)
			goto cpu_getrate_err;
		pll2_rate = clk_get_rate(pll2_clk) / KHZ_TO_HZ;
		pll2_rate = pll2_round_rate(pll2_rate);
		if (!pll2_rate)
			goto cpu_getrate_err;
		ret = pll2_rate / div / MHZ_TO_KHZ;	/* MHz align */
		ret *= KHZ_TO_HZ;			/* KHz align */
		break;
	default:
		goto cpu_getrate_err;
		break;
	}

	spin_unlock_irqrestore(&cpu_clk_lock, flags);
	/* Hz align */
	return ret * KHZ_TO_HZ;

cpu_getrate_err:
	pr_err("Cannot get frequency value for cpu core!\n");
	return 0;
}

static void cpu_clk_init(struct clk_hw *hw)
{
	int idx;
	struct pxa1928_core_opt *core_op_array;
	unsigned int core_opt_size = 0;

	core_op_array = cur_core_opt;
	core_opt_size = cur_core_opt_size;

	hw->clk->rate = cpu_clk_recalc(hw, 0);
	for (idx = 0; idx < core_opt_size; idx++) {
		if (core_op_array[idx].freq_khz * KHZ_TO_HZ
			>= (hw->clk->rate)) {
			core_idx_restore = idx;
			break;
		}
	}
	pr_info("CPU boot up @%luHZ, idx: %d\n",
			hw->clk->rate, core_idx_restore);
}

static long cpu_clk_round_rate(struct clk_hw *hw, unsigned long rate,
				unsigned long *prate)
{
	int i, num;
	struct pxa1928_core_opt *core_op_array;
	unsigned int core_opt_size = 0;

	core_op_array = cur_core_opt;
	core_opt_size = cur_core_opt_size;

	/* kHz align */
	rate /= 1000;

	num = core_opt_size;
	if (rate >= core_op_array[num - 1].freq_khz)
		return (core_op_array[num - 1].freq_khz) * 1000;

	for (i = 0; i < num; i++) {
		if (core_op_array[i].freq_khz >= rate)
			break;
	}
	return (core_op_array[i].freq_khz) * KHZ_TO_HZ;
}

#ifdef CONFIG_CPU_FREQ

static int pxa1928_powermode(u32 cpu)
{
	u32 val;

	val = __raw_readl(apmu_core_pwrmode[cpu]);
	val &= 0x3;
	if (val == 1)
		return LPM_C1;
	else if (val == 3)
		return LPM_C2;
	else
		return MAX_LPM_INDEX;
}

static void __init_cpufreq_table(void)
{
	int i, num;
	struct pxa1928_core_opt *core_op_array;
	unsigned int core_opt_size = 0;
	struct cpufreq_frequency_table *cpufreq_tbl;
	unsigned int pp[MAX_OP_NUM];

	core_op_array = cur_core_opt;
	core_opt_size = cur_core_opt_size;

	num = core_opt_size;
	cpufreq_tbl =
		kmalloc(sizeof(struct cpufreq_frequency_table) *
					(num + 1), GFP_KERNEL);
	if (!cpufreq_tbl)
		return;

	for (i = 0; i < num; i++) {
		if (core_op_array[i].freq_khz > cpu_max_freq)
			break;
		cpufreq_tbl[i].driver_data = i;
		cpufreq_tbl[i].frequency = core_op_array[i].freq_khz;
		/* take MHz as unit */
		pp[i] = core_op_array[i].freq_khz / 1000;
	}

	cpufreq_tbl[i].driver_data = i;
	cpufreq_tbl[i].frequency = CPUFREQ_TABLE_END;

	register_cpu_dcstat(cpu_dcstat_clk, num_possible_cpus(), pp, i,
		pxa1928_powermode, SINGLE_CLUSTER);

	for_each_possible_cpu(i)
		cpufreq_frequency_table_get_attr(cpufreq_tbl, i);
}
#else
#define __init_cpufreq_table() do {} while (0)
#endif

static struct clk_ops cpu_clk_ops = {
	.init		= cpu_clk_init,
	.set_rate	= cpu_clk_setrate,
	.round_rate	= cpu_clk_round_rate,
	.recalc_rate	= cpu_clk_recalc,
};

void __init pxa1928_core_clk_init(void __iomem *apmu_base)
{
	struct clk *clk;
	struct clk_hw *hw;
	struct clk_init_data init;
	u32 reg;
	int src, src_sel;

	apmu_coreapss_clkctrl = apmu_base + APMU_COREAPSS_CLKCTRL;
	apmu_coreapss_cfg = apmu_base + APMU_COREAPSS_CFG;

	hw = kzalloc(sizeof(struct clk_hw), GFP_KERNEL);
	if (!hw) {
		pr_err("error to alloc mem for cpu clk\n");
		return;
	}

	cur_core_opt = core_oparray_a0;
	cur_core_opt_size = ARRAY_SIZE(core_oparray_a0);
	corefreq_src_shift = 29;
	corefreq_src_msk = 0x7;
	core_src_maptbl = core_src_ax_maptbl;
	/* set bit 3 all the time */
	reg = readl(apmu_coreapss_clkctrl);
	reg |= (1 << COREFREQ_FCEN_SHIFT);
	writel(reg, apmu_coreapss_clkctrl);

	pll2p_clk = clk_get(NULL, "pll2p");
	pll2_clk = clk_get(NULL, "pll2");
	if (!pll2p_clk || !pll2_clk) {
		pr_err("Cannot find clock source for CPU!\n");
		return;
	}
	reg = readl(apmu_coreapss_clkctrl);
	src_sel = (reg >> corefreq_src_shift) & corefreq_src_msk;
	src = core_src_maptbl[src_sel];
	switch (src) {
	case CORE_PLL1:
		pll2p_clk->ops->disable(pll2p_clk->hw);
		pll2_clk->ops->disable(pll2_clk->hw);
		break;
	case CORE_PLL2:
		pll2p_clk->ops->disable(pll2p_clk->hw);
		clk_prepare_enable(pll2_clk);
		break;
	case CORE_PLL2P:
		clk_prepare_enable(pll2p_clk);
		break;
	}

	init.name = "cpu";
	init.ops = &cpu_clk_ops;
	init.flags = CLK_IS_ROOT;
	init.parent_names = NULL;
	init.num_parents = 0;
	hw->init = &init;

	clk = clk_register(NULL, hw);
	if (IS_ERR(clk)) {
		pr_err("error to register cpu clk\n");
		kfree(hw);
		return;
	}
	clk_register_clkdev(clk, "cpu", NULL);
	clk_prepare_enable(clk);
	cpu_dcstat_clk = clk;

	__init_cpufreq_table();
}

static void __init __init_axi_opt(void)
{
	struct pxa1928_axi_opt *cop;
	unsigned int i;

	for (i = 0; i < cur_axi_opt_size; i++) {
		cop = &cur_axi_opt[i];

		pr_info("%d(%d:%d,%d,%d,%d,%d,%d,%d)\n",
			cop->aclk1, cop->aclk2, cop->aclk3,
			cop->aclk4, cop->axi_clk_sel,
			cop->aclk_div1, cop->aclk_div2,
			cop->aclk_div3, cop->aclk_div4);
	}
}

static int get_cur_axi_op(void)
{
	union pll_sel_status pll_sel;
	union dm_cc_pj cc;
	union dm2_cc cc2;
	struct pxa1928_axi_opt *tmp_axi;
	u32 div1, div2, div3, div4, src;
	int i;

	pll_sel.v = readl(apmu_pll_sel_status);
	cc.v = readl(apmu_dm_cc_pj);
	cc2.v = readl(apmu_dm2_cc);

	div1 = cc.b.soc_axi_clk_div;
	div2 = cc2.b.soc_axi_clk2_div;
	div3 = cc2.b.soc_axi_clk3_div;
	div4 = cc2.b.soc_axi_clk4_div;
	src = pll_sel.b.soc_axi_pll_sel;

	for (i = 0; i < cur_axi_opt_size; i++) {
		tmp_axi = &cur_axi_opt[i];
		if (div2 == tmp_axi->aclk_div2 &&
			src == tmp_axi->axi_clk_sel)
				return i;
	}

	pr_err("APMU_PLL_SEL_STATUS[31~0]:0x%x\nAPMU_DM_CC_PJ[31~0]:0x%x\n"
		"APMU_DM2_CC[31~0]:0x%x\n"
		"Current axi freq is not in defined PP table!",
		pll_sel.v, cc.v, cc2.v);

	return -1;
}

static unsigned int axi_rate2_op_index(unsigned int rate)
{
	int i;

	if (rate > cur_axi_opt[cur_axi_opt_size - 1].aclk1)
		return cur_axi_opt_size - 1;

	for (i = 0; i < cur_axi_opt_size; i++)
		if (cur_axi_opt[i].aclk1 >= rate)
			break;

	return i;
}

static void wait4axifc_done(void)
{
	int timeout = 1000;
	/* busy wait for dfc done */
	while (((1 << 30) & readl(apmu_cc_pj)) &&
			timeout)
		timeout--;

	if (unlikely(timeout <= 0)) {
		WARN(1, "AP AXI frequency change timeout!\n");
		pr_err("APMU_CC_PJ: %x\n", readl(apmu_cc_pj));
	}
}

static void axi_fc_seq(struct pxa1928_axi_opt *top)
{
	union bus_res_ctrl bus_res;
	union cc_pj cc;
	union cc2_pj cc2;

	/* 1 Program PLL source select */
	bus_res.v = readl(apmu_bus);
	bus_res.b.axiclksel = top->axi_clk_sel;
	writel(bus_res.v, apmu_bus);

	/* 2 Program div */
	cc2.v = readl(apmu_cc2_pj);
	cc2.b.axi2_div = top->aclk_div2;
	writel(cc2.v, apmu_cc2_pj);

	/* 3 trigger AXI DFC */
	spin_lock(&cc_pj_access_lock);
	cc.v = readl(apmu_cc_pj);
	cc.b.aclk_fc_req = 1;
	writel(cc.v, apmu_cc_pj);
	spin_unlock(&cc_pj_access_lock);

	/* 4 wait for DFC done */
	wait4axifc_done();
}

static int set_axi_freq(struct pxa1928_axi_opt *old,
	struct pxa1928_axi_opt *new)
{
	int cur_index, ret = 0;
	unsigned long flags;
	struct pxa1928_axi_opt *cop, *op_array;

	op_array = cur_axi_opt;
	cur_index = get_cur_axi_op();
	if (cur_index < 0) {
		pr_err("axi op incorrect!" "fake it as axi_opt[0]\n");
		cur_index = 0;
	}
	cop = &op_array[cur_index];

	if (unlikely(cop != old)) {
		pr_err("aclk asrc adiv");
		pr_err("OLD %d %d %d\n", old->aclk1,
		       old->axi_clk_sel, old->aclk_div1);
		pr_err("CUR %d %d %d\n", cop->aclk1,
		       cop->axi_clk_sel, old->aclk_div1);
		pr_err("NEW %d %d %d\n", new->aclk1,
		       new->axi_clk_sel, new->aclk_div1);
		dump_stack();
	}

	pr_debug("[AXI DFC]-enter: target value: %ukHZ...\n", new->aclk1);
	local_irq_save(flags);
	axi_fc_seq(new);
	local_irq_restore(flags);

	cur_index = get_cur_axi_op();
	if (cur_index < 0) {
		pr_err("[AXI DFC]-failure: current AXI OP not in PP table\n");
		ret = -EAGAIN;
		goto out;
	}
	cop = &op_array[cur_index];

	if (unlikely(cop != new)) {
		pr_err("[AXI DFC]-failure\n");
		pr_err("CUR %d %d %d\n", cop->aclk1,
		       cop->axi_clk_sel, old->aclk_div1);
		pr_err("NEW %d %d %d\n", new->aclk1,
		       new->axi_clk_sel, new->aclk_div1);
		ret = -EAGAIN;
		goto out;
	}

	pr_debug("[AXI DFC]-success: old: %uKHz, current: %ukHZ...\n",
		 old->aclk1, new->aclk1);

out:
	return ret;
}

static int axi_clk_setrate(struct clk_hw *hw, unsigned long rate,
			   unsigned long prate)
{
	struct pxa1928_axi_opt *new_op, *op_array;
	int index;
	int ret;

	rate /= KHZ_TO_HZ;
	index = axi_rate2_op_index(rate);
	op_array = cur_axi_opt;

	new_op = &op_array[index];
	if (new_op == cur_axi_op)
		return 0;

	spin_lock(&axi_fc_seq_lock);
	ret = set_axi_freq(cur_axi_op, new_op);
	spin_unlock(&axi_fc_seq_lock);
	if (ret)
		goto out;

	cur_axi_op = new_op;

out:
	return ret;
}

static void axi_clk_init(struct clk_hw *hw)
{
	int op_index;

	__init_axi_opt();

	op_index = get_cur_axi_op();
	if (op_index < 0) {
		pr_err("axi initialized incorrect!" "fake it as axi_opt[0]\n");
		op_index = 0;
	}
	cur_axi_op = &cur_axi_opt[op_index];

	hw->clk->rate = cur_axi_opt[op_index].aclk1 * KHZ_TO_HZ;
	pr_info(" AXI boot up @%lukHZ\n", hw->clk->rate);
}

static long axi_clk_round_rate(struct clk_hw *hw, unsigned long rate,
			       unsigned long *prate)
{
	int i;
	struct pxa1928_axi_opt *axi_pp_table;
	unsigned int axi_pp_num;

	rate /= KHZ_TO_HZ;
	axi_pp_num = cur_axi_opt_size;
	axi_pp_table = cur_axi_opt;

	if (rate > axi_pp_table[axi_pp_num - 1].aclk1)
		return axi_pp_table[axi_pp_num - 1].aclk1 * KHZ_TO_HZ;

	for (i = 0; i < axi_pp_num; i++)
		if (axi_pp_table[i].aclk1 >= rate)
			break;

	return axi_pp_table[i].aclk1 * KHZ_TO_HZ;
}

static unsigned long axi_clk_recalc(struct clk_hw *hw, unsigned long prate)
{
	if (cur_axi_op)
		return cur_axi_op->aclk1 * KHZ_TO_HZ;
	else
		pr_err("%s: cur_axi_op NULL/n", __func__);

	return 0;
}

static struct clk_ops axi_clk_ops = {
	.init = axi_clk_init,
	.set_rate = axi_clk_setrate,
	.round_rate = axi_clk_round_rate,
	.recalc_rate = axi_clk_recalc,
};

void __init pxa1928_axi_clk_init(void __iomem *apmu_base,
			struct mmp_clk_unit *unit)
{
	struct clk *clk;
	struct clk_hw *hw;
	struct clk_init_data init;

	apmu_pll_sel_status = apmu_base + APMU_PLL_SEL_STATUS;
	apmu_dm_cc_pj = apmu_base + APMU_DM_CC_PJ;
	apmu_dm2_cc = apmu_base + APMU_DM2_CC;
	apmu_cc_pj = apmu_base + APMU_CC_PJ;
	apmu_cc2_pj = apmu_base + APMU_CC2_PJ;
	apmu_bus = apmu_base + APMU_BUS;

	hw = kzalloc(sizeof(struct clk_hw), GFP_KERNEL);
	if (!hw) {
		pr_err("error to alloc mem for axi clk\n");
		return;
	}

	init.name = "axi";
	init.ops = &axi_clk_ops;
	init.flags = CLK_IS_ROOT;
	init.parent_names = NULL;
	init.num_parents = 0;
	hw->init = &init;

	cur_axi_opt = axi_oparray;
	cur_axi_opt_size = ARRAY_SIZE(axi_oparray);

	clk = clk_register(NULL, hw);
	if (IS_ERR(clk)) {
		pr_err("error to register axi clk\n");
		kfree(hw);
		return;
	}
	clk_register_clkdev(clk, "axi", NULL);
	clk_prepare_enable(clk);
	mmp_clk_add(unit, PXA1928_CLK_AXI, clk);

}

#ifdef AXI_DDR_COUPLE_DFC
static int axi_min_notify(struct notifier_block *b,
			unsigned long min, void *v)
{
	struct clk *axi_clk;
	axi_clk = clk_get(NULL, "axi");
	clk_set_rate(axi_clk, min);

	return NOTIFY_OK;
}

static struct notifier_block axi_min_notifier = {
	.notifier_call = axi_min_notify,
};
#endif

static int get_cur_axi11_op(void)
{
	union pmua_aclk11_clkctrl aclk11_ctrl;
	u32 i, div, src;

	aclk11_ctrl.v = readl(apmu_aclk11_clkctrl);
	div = aclk11_ctrl.b.clk_div;
	src = aclk11_ctrl.b.clksrc_sel;

	for (i = 0; i < cur_axi11_opt_size; i++)
		if (div == axi11_oparray[i].aclk_div &&
			src == axi11_oparray[i].axi_clk_sel)
			break;

	return i >= cur_axi11_opt_size ? -1 : i;
}

static unsigned int axi11_rate2_op_index(unsigned int rate)
{
	int i;

	if (rate > axi11_oparray[cur_axi11_opt_size - 1].aclk)
		return cur_axi11_opt_size - 1;

	for (i = 0; i < cur_axi11_opt_size; i++)
		if (axi11_oparray[i].aclk >= rate)
			break;

	return i;
}

static void axi11_fc_seq(struct pxa1928_axi11_opt *top)
{
	union pmua_aclk11_clkctrl aclk11_ctrl;

	/* 1) Set this field to 0 to block frequency changes. */
	aclk11_ctrl.v = readl(apmu_aclk11_clkctrl);
	aclk11_ctrl.b.fc_en = 0;
	writel(aclk11_ctrl.v, apmu_aclk11_clkctrl);

	/* 2) Configure CLKSRC_SEL and CLK_DIV. */
	aclk11_ctrl.b.clksrc_sel = top->axi_clk_sel;
	aclk11_ctrl.b.clk_div = top->aclk_div;
	writel(aclk11_ctrl.v, apmu_aclk11_clkctrl);

	/* 3) Set this field to 1 to allow frequency changes.*/
	aclk11_ctrl.v = readl(apmu_aclk11_clkctrl);
	aclk11_ctrl.b.fc_en = 1;
	writel(aclk11_ctrl.v, apmu_aclk11_clkctrl);
}

static int set_axi11_freq(struct pxa1928_axi11_opt *new)
{
	unsigned long flags;
	int ret = 0, cur_index;
	struct pxa1928_axi11_opt *cop;

	pr_debug("[AXI11 DFC]-enter: target value: %ukHZ...\n", new->aclk);
	local_irq_save(flags);
	axi11_fc_seq(new);
	local_irq_restore(flags);

	cur_index = get_cur_axi11_op();
	if (cur_index < 0) {
		pr_err("[AXI11 DFC]-failure: current AXI11 OP not in PP table\n");
		ret = -EAGAIN;
		goto out;
	}
	cop = &axi11_oparray[cur_index];

	if (unlikely(cop != new)) {
		pr_err("[AXI11 DFC]-failure\n");
		ret = -EAGAIN;
		goto out;
	}

	pr_debug("[AXI11 DFC]-success: current: %ukHZ...\n", new->aclk);

out:
	return ret;
}

static int axi11_clk_setrate(struct clk_hw *hw, unsigned long rate,
			   unsigned long prate)
{
	struct pxa1928_axi11_opt *new_op;
	int index;
	int ret;

	rate /= KHZ_TO_HZ;
	index = axi11_rate2_op_index(rate);

	new_op = &axi11_oparray[index];
	if (new_op == cur_axi11_op)
		return 0;

	spin_lock(&axi11_fc_seq_lock);
	ret = set_axi11_freq(new_op);
	spin_unlock(&axi11_fc_seq_lock);
	if (ret)
		goto out;

	cur_axi11_op = new_op;

out:
	return ret;
}

static void axi11_clk_init(struct clk_hw *hw)
{
	int index;

	index = get_cur_axi11_op();
	if (index < 0) {
		pr_err("axi11 iniitialized incorrect! fake it as axi11[0]\n");
		index = 0;
	}

	cur_axi11_op = &axi11_oparray[index];

	hw->clk->rate = axi11_oparray[index].aclk;
	pr_info("AXI11 boot up @%lukHZ\n", hw->clk->rate);
}

static long axi11_clk_round_rate(struct clk_hw *hw, unsigned long rate,
			       unsigned long *prate)
{
	int i;

	rate /= KHZ_TO_HZ;

	if (rate > axi11_oparray[cur_axi11_opt_size - 1].aclk)
		return axi11_oparray[cur_axi11_opt_size - 1].aclk * KHZ_TO_HZ;

	for (i = 0; i < cur_axi11_opt_size; i++)
		if (axi11_oparray[i].aclk >= rate)
			break;

	return axi11_oparray[i].aclk * KHZ_TO_HZ;
}

static unsigned long axi11_clk_recalc(struct clk_hw *hw, unsigned long prate)
{
	if (cur_axi11_op)
		return cur_axi11_op->aclk * KHZ_TO_HZ;
	else
		pr_err("%s: cur_axi_op NULL/n", __func__);

	return 0;
}

static struct clk_ops axi11_clk_ops = {
	.init = axi11_clk_init,
	.set_rate = axi11_clk_setrate,
	.round_rate = axi11_clk_round_rate,
	.recalc_rate = axi11_clk_recalc,
};

void __init pxa1928_axi11_clk_init(void __iomem *apmu_base)
{
	struct clk *clk;
	struct clk_hw *hw;
	struct clk_init_data init;

	apmu_aclk11_clkctrl = apmu_base + APMU_ACLK11_CLKCTRL;

	hw = kzalloc(sizeof(struct clk_hw), GFP_KERNEL);
	if (!hw) {
		pr_err("error to alloc mem for axi11 clk\n");
		return;
	}

	init.name = "axi11";
	init.ops = &axi11_clk_ops;
	init.flags = CLK_IS_ROOT;
	init.parent_names = NULL;
	init.num_parents = 0;
	hw->init = &init;

	cur_axi11_opt_size = ARRAY_SIZE(axi11_oparray);

	clk = clk_register(NULL, hw);
	if (IS_ERR(clk)) {
		pr_err("error to register axi11 clk\n");
		kfree(hw);
		return;
	}
	clk_register_clkdev(clk, "axi11", NULL);
	clk_prepare_enable(clk);
}


/* set 2x dclk in apmu */
static struct pxa1928_ddr_opt ddr_discrete_oparray[] = {
#ifdef DISCRETE_4L
	{
		.dclk = 78000,
		.ddr_tbl_index = 0,
		.ddr_clk_sel = DDR_PLL1_624,
		.dclk_div = 3,
		.ddr_freq_level = 0,
		/* FIXME rtcwtc val should be filled !*/
		.rtcwtc = 0xA9AA5555,
		.rtcwtc_lvl = 0,
		.axi_constraint = 78000,
	},
	{
		.dclk = 104000,
		.ddr_tbl_index = 1,
		.ddr_clk_sel = DDR_PLL1_416,
		.dclk_div = 1,
		.ddr_freq_level = 1,
		.rtcwtc = 0xA9AA5555,
		.rtcwtc_lvl = 0,
		.axi_constraint = 78000,
	},
	{
		.dclk = 156000, /* 2x 312000 */
		.ddr_tbl_index = 2,
		.ddr_clk_sel = DDR_PLL1_624,
		.dclk_div = 1,
		.ddr_freq_level = 2,
		/* FIXME rtcwtc val should be filled !*/
		.rtcwtc = 0xA9AA5555,
		.rtcwtc_lvl = 0,
		.axi_constraint = 104000,
	},
	{
		.dclk = 208000, /* 2x 416000 */
		.ddr_tbl_index = 3,
		.ddr_clk_sel = DDR_PLL1_416,
		.dclk_div = 0,
		.ddr_freq_level = 3,
		.rtcwtc = 0xA9AA5555,
		.rtcwtc_lvl = 0,
		.axi_constraint = 104000,
	},
	{
		.dclk = 312000, /* 2x 624000 */
		.ddr_tbl_index = 4,
		.ddr_clk_sel = DDR_PLL1_624,
		.dclk_div = 0,
		.ddr_freq_level = 4,
		.rtcwtc = 0xA9AA5555,
		.rtcwtc_lvl = 0,
		.axi_constraint = 156000,
	},
	{
		/* fake as 502MHz in 2L discrete */
		.dclk = 528000,
		.ddr_tbl_index = 6,
		.ddr_clk_sel = DDR_PLL4,
		.ddr_freq_level = 5,
		.rtcwtc = 0xA9AA5555,
		.rtcwtc_lvl = 0,
		/* When PLL4 is selected, no div */
		.src_freq = 1057333333,
		.axi_constraint = 208000,
	},
	{
		.dclk = 624000, /* 2x 1057000 */
		.ddr_tbl_index = 7,
		.ddr_clk_sel = DDR_PLL4,
		.ddr_freq_level = 6,
		.rtcwtc = 0xA9AA5555,
		.rtcwtc_lvl = 0,
		/* When PLL4 is selected, no div */
		.src_freq = 1057333333,
		.axi_constraint = 312000,
	},
	{
		.dclk = 667000, /* 2x 1334000 */
		.ddr_tbl_index = 8,
		.ddr_clk_sel = DDR_PLL4,
		.ddr_freq_level = 7,
		.rtcwtc = 0xA9AA5555,
		.rtcwtc_lvl = 0,
		/* When PLL4 is selected, no div */
		.src_freq = 1057333333,
		.axi_constraint = 312000,
	},
#else
	{
		.dclk = 78000,
		.ddr_tbl_index = 0,
		.ddr_clk_sel = DDR_PLL1_624,
		.dclk_div = 3,
		.ddr_freq_level = 0,
		/* FIXME rtcwtc val should be filled !*/
		.rtcwtc = 0xA9AA5555,
		.rtcwtc_lvl = 0,
		.axi_constraint = 78000,
	},
	{
		.dclk = 104000,
		.ddr_tbl_index = 1,
		.ddr_clk_sel = DDR_PLL1_416,
		.dclk_div = 1,
		.ddr_freq_level = 1,
		.rtcwtc = 0xA9AA5555,
		.rtcwtc_lvl = 0,
		.axi_constraint = 104000,
	},
	{
		.dclk = 156000, /* 2x 312000 */
		.ddr_tbl_index = 2,
		.ddr_clk_sel = DDR_PLL1_624,
		.dclk_div = 1,
		.ddr_freq_level = 2,
		/* FIXME rtcwtc val should be filled !*/
		.rtcwtc = 0xA9AA5555,
		.rtcwtc_lvl = 0,
		.axi_constraint = 104000,
	},
	{
		.dclk = 208000, /* 2x 416000 */
		.ddr_tbl_index = 3,
		.ddr_clk_sel = DDR_PLL1_416,
		.dclk_div = 0,
		.ddr_freq_level = 3,
		.rtcwtc = 0xA9AA5555,
		.rtcwtc_lvl = 0,
		.axi_constraint = 156000,
	},
	{
		.dclk = 312000, /* 2x 624000 */
		.ddr_tbl_index = 4,
		.ddr_clk_sel = DDR_PLL1_624,
		.dclk_div = 0,
		.ddr_freq_level = 4,
		.rtcwtc = 0xA9AA5555,
		.rtcwtc_lvl = 0,
		.axi_constraint = 208000,
	},
	{
		/* fake as 502MHz in 2L discrete */
		.dclk = 528000,
		.ddr_tbl_index = 5,
		.ddr_clk_sel = DDR_PLL4,
		.ddr_freq_level = 5,
		.rtcwtc = 0xA9AA5555,
		.rtcwtc_lvl = 0,
		/* When PLL4 is selected, no div */
		.src_freq = 1057333333,
		.axi_constraint = 312000,
	},
#endif
};


/* set 2x dclk in apmu */
static struct pxa1928_ddr_opt ddr_oparray[] = {
	{
		.dclk = 78000,
		.ddr_tbl_index = 0,
		.ddr_clk_sel = DDR_PLL1_624,
		.dclk_div = 3,
		.ddr_freq_level = 0,
		/* FIXME rtcwtc val should be filled !*/
		.rtcwtc = 0xA9AA5555,
		.rtcwtc_lvl = 0,
		.axi_constraint = 78000,
	},
	{
		.dclk = 104000,
		.ddr_tbl_index = 1,
		.ddr_clk_sel = DDR_PLL1_416,
		.dclk_div = 1,
		.ddr_freq_level = 1,
		.rtcwtc = 0xA9AA5555,
		.rtcwtc_lvl = 0,
		.axi_constraint = 78000,
	},
	{
		.dclk = 156000, /* 2x 312000 */
		.ddr_tbl_index = 2,
		.ddr_clk_sel = DDR_PLL1_624,
		.dclk_div = 1,
		.ddr_freq_level = 2,
		/* FIXME rtcwtc val should be filled !*/
		.rtcwtc = 0xA9AA5555,
		.rtcwtc_lvl = 0,
		.axi_constraint = 104000,
	},
	{
		.dclk = 208000, /* 2x 416000 */
		.ddr_tbl_index = 3,
		.ddr_clk_sel = DDR_PLL1_416,
		.dclk_div = 0,
		.ddr_freq_level = 3,
		.rtcwtc = 0xA9AA5555,
		.rtcwtc_lvl = 0,
		.axi_constraint = 104000,
	},
	{
		.dclk = 312000, /* 2x 624000 */
		.ddr_tbl_index = 4,
		.ddr_clk_sel = DDR_PLL1_624,
		.dclk_div = 0,
		.ddr_freq_level = 4,
		.rtcwtc = 0xA9AA5555,
		.rtcwtc_lvl = 0,
		.axi_constraint = 156000,
	},
	{
		.dclk = 528000, /* 2x 1057000 */
		.ddr_tbl_index = 6,
		.ddr_clk_sel = DDR_PLL4,
		.ddr_freq_level = 5,
		.rtcwtc = 0xA9AA5555,
		.rtcwtc_lvl = 0,
		/* When PLL4 is selected, no div */
		.src_freq = 1057333333,
		.axi_constraint = 208000,
	},
	{
		.dclk = 667000, /* 2x 1057000 */
		.ddr_tbl_index = 7,
		.ddr_clk_sel = DDR_PLL4,
		.ddr_freq_level = 6,
		.rtcwtc = 0xA9AA5555,
		.rtcwtc_lvl = 0,
		/* When PLL4 is selected, no div */
		.src_freq = 1334333333,
		.axi_constraint = 312000,
	},
	{
		.dclk = 800000, /* 2x 1057000 */
		.ddr_tbl_index = 10,
		.ddr_clk_sel = DDR_PLL4,
		.ddr_freq_level = 7,
		.rtcwtc = 0xA9AA5555,
		.rtcwtc_lvl = 0,
		/* When PLL4 is selected, no div */
		.src_freq = 1600333333,
		.axi_constraint = 312000,
	},
};

/* FIXME need filled when ddr3 pp ready
static struct pxa1928_ddr_opt ddr3_oparray[] = {
};
*/

static void __init __init_ddr_opt(void)
{
	struct pxa1928_ddr_opt *cop;
	unsigned int i;

	for (i = 0; i < cur_ddr_opt_size; i++) {
		cop = &cur_ddr_opt[i];

		pr_info("%d(%d:%d,%d)\n",
			cop->dclk, cop->ddr_clk_sel,
			cop->dclk_div, cop->ddr_tbl_index);
	}
}

static int get_cur_ddr_op(void)
{
	int i;
	union mc_clk_stat mc_stat;
	union ddrdfc_ctrl ctrl;
	u32 div, src;

	mc_stat.v = readl(apmu_mc_clk_stat);
	ctrl.v = readl(apmu_ddrdfc_ctrl);
	div = mc_stat.b.pmu_ddr_clk_div;
	src = mc_stat.b.pmu_ddr_clk_sel;

	if (ctrl.b.dfc_hw_en)
		return ctrl.b.dfc_fp_index_cur;

	if (mc_stat.b.mc_clk_src_sel) {
		for (i = 0; i < cur_ddr_opt_size; i++)
			if (cur_ddr_opt[i].ddr_clk_sel == DDR_PLL4)
				return i;
	} else {
		for (i = 0; i < cur_ddr_opt_size; i++)
			if (cur_ddr_opt[i].dclk_div == div &&
			    cur_ddr_opt[i].ddr_clk_sel == src)
				return i;
	}

	pr_err("Current ddr freq is not in defined PP table !\n"
		"APMU_MC_CLK_STAT[31~0] 0x%x", mc_stat.v);

	return -1;
}

#ifdef CONFIG_DDR_DEVFREQ
static struct devfreq_frequency_table *ddr_devfreq_tbl;

static void __init_ddr_devfreq_table(struct devfreq_frequency_table *ddr_tbl,
						unsigned int dev_id)
{
	struct pxa1928_ddr_opt *ddr_opt;
	unsigned int ddr_opt_size = 0, i = 0;

	ddr_opt_size = cur_ddr_opt_size;
	ddr_tbl =
		kmalloc(sizeof(struct devfreq_frequency_table)
			* (ddr_opt_size + 1), GFP_KERNEL);
	if (!ddr_tbl)
		return;

	ddr_opt = cur_ddr_opt;
	for (i = 0; i < ddr_opt_size; i++) {
		ddr_tbl[i].index = i;
		ddr_tbl[i].frequency = ddr_opt[i].dclk;
	}

	ddr_tbl[i].index = i;
	ddr_tbl[i].frequency = DEVFREQ_TABLE_END;

	devfreq_frequency_table_register(ddr_tbl, dev_id);
}
#endif

static void ddr_clk_init(struct clk_hw *hw)
{
	int i, op_index;
	struct pxa1928_ddr_opt *cop;
	union ddrdfc_fp_upr upr;
	union ddrdfc_fp_lwr lwr;
	union ddrdfc_ctrl ctrl;
	struct pll_freq_tb *pll4_freq_tb;
	struct clk_ddr *ddr = to_clk_ddr(hw);
	unsigned long ddr_op[MAX_OP_NUM];

	__init_ddr_opt();

	if (discrete_ddr)
		pll4_freq_tb = pll4_freq_tb_dis;
	else
		pll4_freq_tb = pll4_freq_tb_pop;

	op_index = get_cur_ddr_op();
	if (op_index < 0) {
		pr_err("ddr initialized incorrect!" "fake it as lowest pp\n");
		op_index = 0;
	}
	cur_ddr_op = &cur_ddr_opt[op_index];

	hw->clk->rate = cur_ddr_opt[op_index].dclk * KHZ_TO_HZ;

	if (ddr->flags & DDR_HWDFC_FEAT) {
		ctrl.v = readl(apmu_ddrdfc_ctrl);
		if (!ctrl.b.dfc_hw_en || !ctrl.b.ddrpll_ctrl) {
			pr_err("DDR HW DFC not configured\n");
			BUG();
		}
	}

	for (i = 0; i < cur_ddr_opt_size; i++) {
		cop = &cur_ddr_opt[i];
		if (ddr->flags & DDR_HWDFC_FEAT) {
			lwr.v = readl(DFC_LEVEL(hwdfc_base, i));
			upr.v = readl(DFC_LEVEL(hwdfc_base, i) + 4);
#ifdef CONFIG_PXA_DVFS
			lwr.b.ddr_vl = ddr_get_dvc_level(cop->dclk * KHZ_TO_HZ);
#else
			lwr.b.ddr_vl = 0;
#endif
			lwr.b.mc_reg_tbl_sel = cop->ddr_tbl_index * 2 + 1;
			lwr.b.rtcwtc_sel = cop->rtcwtc_lvl;
			/* we don't use 4x mode for now */
			lwr.b.sel_4xmode = 0;
			lwr.b.sel_ddrpll =
				(cop->ddr_clk_sel == DDR_PLL4) ? 1 : 0;
			if (cop->ddr_clk_sel == DDR_PLL4) {
				pr_debug("index %d\n", i - 5);
				upr.b.fp_upr5 = pll4_freq_tb[i - 5].icp;
				upr.b.fp_upr4 = pll4_freq_tb[i - 5].bw_sel;
				upr.b.fp_upr3 =
					pll4_freq_tb[i - 5].post_div_sel;
				upr.b.fp_upr2 = pll4_freq_tb[i - 5].fbdiv;
				upr.b.fp_upr1 = pll4_freq_tb[i - 5].refdiv;
				upr.b.fp_upr0 = pll4_freq_tb[i - 5].kvco;
			} else {
				/* power off pll4 */
				upr.b.fp_upr4 = 1;
				upr.b.fp_upr3 = cop->dclk_div;
				upr.b.fp_upr0 = cop->ddr_clk_sel;
				upr.b.reserved = 0;
			}

			__raw_writel(lwr.v, DFC_LEVEL(hwdfc_base, i));
			__raw_writel(upr.v, DFC_LEVEL(hwdfc_base, i) + 4);
		}

		__raw_writel(cop->rtcwtc, apmu_mc_mem_ctrl);
	}

#ifdef AXI_DDR_COUPLE_DFC
	pm_qos_add_notifier(PM_QOS_AXI_MIN, &axi_min_notifier);
	pm_qos_add_request(&axi_qos_req_min, PM_QOS_AXI_MIN,
			PM_QOS_DEFAULT_VALUE);
#endif

#ifdef CONFIG_DDR_DEVFREQ
	__init_ddr_devfreq_table(ddr_devfreq_tbl, DEVFREQ_DDR);
#endif

	for (op_index = 0; op_index < cur_ddr_opt_size; op_index++)
		ddr_op[op_index] =
			(unsigned long)(&cur_ddr_opt[op_index])->dclk;

	clk_register_dcstat(hw->clk, ddr_op, op_index);

	pr_info("DDR boot up @%lukHZ\n", hw->clk->rate);
}

static unsigned int ddr_rate2_op_index(unsigned long rate)
{
	int i;

	if (rate > cur_ddr_opt[cur_ddr_opt_size - 1].dclk)
		return cur_ddr_opt_size - 1;

	for (i = 0; i < cur_ddr_opt_size; i++)
		if (cur_ddr_opt[i].dclk >= rate)
			break;

	return i;
}

static void wait4ddrfc_done(void)
{
	int timeout = 80;

	/* busy wait for dfc done */
	while (((1 << 28) & readl(apmu_mc_clk_res_ctrl)) &&
			timeout) {
		udelay(10);
		timeout--;
	}

	if (unlikely(timeout <= 0)) {
		WARN(1, "AP DDR frequency change timeout!\n");
		pr_err("APMU_MC_CLK_RES_CTRL: %x\n",
				readl(apmu_mc_clk_res_ctrl));
	}
}

static void wait4hwdfc_done(void)
{
	u32 timeout = 50;
	u32 tmp;

	tmp = readl(apmu_ddrdfc_ctrl);

	/* busy wait for hwdfc done and dfc_in_progress cleared */
	while ((!((1 << 31) & tmp) || ((1 << 28) & tmp)) &&
			timeout--) {
		udelay(10);
		tmp = readl(apmu_ddrdfc_ctrl);
	}

	pr_debug("time spent %d us, APMU_DDRDFC_CTRL: 0x%x\n",
					(20 - timeout) * 10,
					readl(apmu_ddrdfc_ctrl));

	if (unlikely(timeout <= 0)) {
		WARN(1, "AP HW DDR frequency change timeout!\n");
		pr_err("APMU_DDRDFC_CTRL: 0x%x\n",
				readl(apmu_ddrdfc_ctrl));
	}
}

static void lpddr2_ddr_fc_seq(struct pxa1928_ddr_opt *top)
{
	union mc_res_ctrl mc_res;
	int disable_pll4_later = 0;
	u32 tmp;

	/* 1 Handle source PLL */
	if (top->ddr_clk_sel == DDR_PLL4) {
		if (cur_ddr_op->ddr_clk_sel != DDR_PLL4) {
			clk_set_rate(pll4_clk, top->src_freq);
			clk_prepare_enable(pll4_clk);
		}
	} else {
		if (cur_ddr_op->ddr_clk_sel == DDR_PLL4)
			disable_pll4_later = 1;
	}

	/* 2 make sure cc_pj[27] and cc_sp[27] are set, no need on ax */

	/* 3 Igore SP status */
	tmp = readl(apmu_debug2);
	if (!((tmp >> 20) & 0x1)) {
		tmp |= 0x1 << 20;
		writel(tmp, apmu_debug2);
	}

	/* 4 Fill DFC table and RTCWTC val */
	mc_res.v = readl(apmu_mc_clk_res_ctrl);
	if (top->dclk < cur_ddr_op->dclk) {
		mc_res.b.mc_dfc_type = 0;
		mc_res.b.mc_reg_table = top->ddr_tbl_index * 2;
	} else {
		mc_res.b.mc_dfc_type = 1;
		mc_res.b.mc_reg_table = top->ddr_tbl_index * 2 + 1;
	}
	mc_res.b.mc_reg_table_en = 1;
	mc_res.b.rtcwtc_sel = top->rtcwtc_lvl;
	writel(mc_res.v, apmu_mc_clk_res_ctrl);

	/* 5 Program new clock setting */
	mc_res.v = readl(apmu_mc_clk_res_ctrl);
	if (top->ddr_clk_sel == DDR_PLL4) {
		mc_res.b.mc_clk_src_sel = 1;
	} else {
		mc_res.b.mc_clk_src_sel = 0;
		mc_res.b.pmu_ddr_sel = top->ddr_clk_sel;
		mc_res.b.pmu_ddr_div = top->dclk_div;
	}
	writel(mc_res.v, apmu_mc_clk_res_ctrl);

	/* 6 Make sure it is 2x mode */
	mc_res.v = readl(apmu_mc_clk_res_ctrl);
	if (mc_res.b.mc_4x_mode == 1) {
		mc_res.b.mc_4x_mode = 0;
		writel(mc_res.v, apmu_mc_clk_res_ctrl);
	}

	/* 7 trigger DDR DFC */
	mc_res.v = readl(apmu_mc_clk_res_ctrl);
	mc_res.b.mc_clk_dfc = 1;
	writel(mc_res.v, apmu_mc_clk_res_ctrl);

	/* 8 wait for ddr fc done */
	wait4ddrfc_done();

	/* 9 disable PLL4 if necessary */
	if (disable_pll4_later)
		clk_disable_unprepare(pll4_clk);
}

static int ddr_hwdfc_seq(unsigned int level)
{
	union ddrdfc_ctrl ctrl;

	/* 1. trigger AP HWDFC */
	ctrl.v = readl(apmu_ddrdfc_ctrl);
	pr_debug("1 ctrl.v is 0x%x\n", ctrl.v);
	ctrl.b.dfc_fp_idx = level;
	pr_debug("2 ctrl.v is 0x%x\n", ctrl.v);
	writel(ctrl.v, apmu_ddrdfc_ctrl);

	/* 2. wait for dfc completes */
	wait4hwdfc_done();

	/* 3. clear dfc interrupt */
	ctrl.v = readl(apmu_ddrdfc_ctrl);
	ctrl.b.dfc_int_clr = 1;
	writel(ctrl.v, apmu_ddrdfc_ctrl);

	return 0;
}


static int set_hwddr_freq(struct pxa1928_ddr_opt *old,
		struct pxa1928_ddr_opt *new)
{
	unsigned long flags;
	int ret;

	pr_debug("target level is %d\n", new->ddr_freq_level);
	local_irq_save(flags);
	ret = ddr_hwdfc_seq(new->ddr_freq_level);
	if (unlikely(ret == -EAGAIN)) {
		/* still stay at old freq and src */
		local_irq_restore(flags);
		goto out;
	}
	local_irq_restore(flags);

	pr_debug("DDR set_freq end: old %u, new %u\n",
			old->dclk, new->dclk);
out:
	return ret;

}

static int set_ddr_freq(struct pxa1928_ddr_opt *old,
		struct pxa1928_ddr_opt *new)
{
	int cur_index;
	int ret = 0;
	unsigned long flags;
	struct pxa1928_ddr_opt *cop, *op_array;

	op_array = cur_ddr_opt;
	cur_index = get_cur_ddr_op();
	if (cur_index < 0) {
		pr_err("ddr op incorrect! fake it as ddr_opt[0]\n");
		cur_index = 0;
	}
	cop = &op_array[cur_index];

	if (unlikely(cop != old)) {
		pr_err("dclk dsrc ddiv");
		pr_err("OLD %d %d %d\n", old->dclk,
		       cop->ddr_clk_sel, old->dclk_div);
		pr_err("CUR %d %d %d\n", cop->dclk,
		       cop->ddr_clk_sel, old->dclk_div);
		pr_err("NEW %d %d %d\n", new->dclk,
		       new->ddr_clk_sel, new->dclk_div);
		dump_stack();
	}

	pr_debug("[DDR DFC]-enter: target value: %ukHZ...\n", new->dclk);
	local_irq_save(flags);
	lpddr2_ddr_fc_seq(new);
	local_irq_restore(flags);

	cur_index = get_cur_ddr_op();
	if (cur_index < 0) {
		pr_err("[DDR DFC]-failure: current DDR OP not in PP table\n");
		ret = -EAGAIN;
		goto out;
	}
	cop = &op_array[cur_index];

	if (unlikely(cop != new)) {
		pr_err("[DDR DFC]-failure\n");
		pr_err("CUR %d %d %d\n", cop->dclk,
		       cop->ddr_clk_sel, old->dclk_div);
		pr_err("NEW %d %d %d\n", new->dclk,
		       new->ddr_clk_sel, new->dclk_div);
		ret = -EAGAIN;
		goto out;
	}

out:
	pr_debug("[DDR DFC]-success: old: %uKHz, current: %ukHZ...\n",
			old->dclk, new->dclk);
	return ret;
}

static int ddr_clk_setrate(struct clk_hw *hw, unsigned long rate,
				unsigned long prate)
{
	struct pxa1928_ddr_opt *new_op, *op_array;
	struct clk_ddr *ddr = to_clk_ddr(hw);
	int index, ret;
	union ddrdfc_ctrl ctrl;

	rate /= KHZ_TO_HZ;
	index = ddr_rate2_op_index(rate);
	op_array = cur_ddr_opt;

	new_op = &op_array[index];
	if (new_op == cur_ddr_op)
		return 0;


	spin_lock(&ddr_fc_seq_lock);
	if (ddr->flags & DDR_HWDFC_FEAT)
		ret = set_hwddr_freq(cur_ddr_op, new_op);
	else
		ret = set_ddr_freq(cur_ddr_op, new_op);

	/* hwdfc guarentee new pp >= target pp */
	ctrl.v = readl(apmu_ddrdfc_ctrl);
	if (ctrl.b.dfc_fp_index_cur < index) {
			pr_err("DDR DFC fails !!!\n"
					"target %d, cur %d\n",
					index, ctrl.b.dfc_fp_index_cur);
			spin_unlock(&ddr_fc_seq_lock);
			ret = -EAGAIN;
			goto out;
	}
	spin_unlock(&ddr_fc_seq_lock);
	if (ret)
		goto out;

	cur_ddr_op = new_op;

#ifdef AXI_DDR_COUPLE_DFC
	pm_qos_update_request(&axi_qos_req_min, new_op->axi_constraint *
			KHZ_TO_HZ);
	pr_debug("axi target is %d\n", new_op->axi_constraint * KHZ_TO_HZ);
#endif

	clk_dcstat_event(hw->clk, CLK_RATE_CHANGE, index);

out:
	return ret;
}

static long ddr_clk_round_rate(struct clk_hw *hw, unsigned long rate,
				unsigned long *prate)
{
	int i;
	struct pxa1928_ddr_opt *ddr_pp_table;
	unsigned int ddr_pp_num;

	rate /= KHZ_TO_HZ;
	ddr_pp_num = cur_ddr_opt_size;
	ddr_pp_table = cur_ddr_opt;

	if (rate > ddr_pp_table[ddr_pp_num - 1].dclk)
		return ddr_pp_table[ddr_pp_num - 1].dclk * KHZ_TO_HZ;

	for (i = 0; i < ddr_pp_num; i++)
		if (ddr_pp_table[i].dclk >= rate)
			break;

	return ddr_pp_table[i].dclk * KHZ_TO_HZ;
}

/* FIXME */
static unsigned long ddr_clk_recalc(struct clk_hw *hw, unsigned long prate)
{
	if (cur_ddr_op)
		return cur_ddr_op->dclk * KHZ_TO_HZ;
	else
		pr_err("%s: cur_ddr_op NULL\n", __func__);

	return 0;
}

static struct clk_ops ddr_clk_ops = {
	.init = ddr_clk_init,
	.set_rate = ddr_clk_setrate,
	.round_rate = ddr_clk_round_rate,
	.recalc_rate = ddr_clk_recalc,
};

void __init pxa1928_ddr_clk_init(void __iomem *apmu_base,
		void __iomem *ddrdfc_base, void __iomem *ciu_base,
		struct mmp_clk_unit *unit)
{
	struct clk *clk;
	struct clk_hw *hw;
	struct clk_init_data init;
	struct clk_ddr *ddr;
	union ddrdfc_ctrl ctrl;
	union mc_clk_stat mc_stat;
	u32 tmp;
	unsigned int core_index;

	hwdfc_base = ddrdfc_base;
	apmu_cc_pj = apmu_base + APMU_CC_PJ;
	apmu_cc_sp = apmu_base + APMU_CC_SP;
	apmu_debug = apmu_base + APMU_DEBUG;
	apmu_debug2 = apmu_base + APMU_DEBUG2;

	for (core_index = 0; core_index < 4; core_index++)
		apmu_core_pwrmode[core_index] = apmu_base
			+ APMU_CORE_PWRMODE_BASE + 4 * core_index;

	apmu_mc_clk_res_ctrl = apmu_base + APMU_MC_CLK_RES_CTRL;
	apmu_mc_clk_stat = apmu_base + APMU_MC_CLK_STAT;
	apmu_mc_mem_ctrl = apmu_base + APMU_MC_MEM_CTRL;
	apmu_ddrdfc_ctrl = hwdfc_base + APMU_DDRDFC_CTRL;
	discrete_ddr = readl(ciu_base + 0x98) & 0x1;

	hw = kzalloc(sizeof(struct clk_hw), GFP_KERNEL);
	if (!hw) {
		pr_err("error to alloc mem for ddr clk\n");
		return;
	}

	init.name = "ddr";
	init.ops = &ddr_clk_ops;
	init.flags = CLK_IS_ROOT;
	init.parent_names = NULL;
	init.num_parents = 0;
	hw->init = &init;

	if (discrete_ddr) {
		cur_ddr_opt = ddr_discrete_oparray;
		cur_ddr_opt_size = ARRAY_SIZE(ddr_discrete_oparray);
	} else {
		cur_ddr_opt = ddr_oparray;
		cur_ddr_opt_size = ARRAY_SIZE(ddr_oparray);
	}

	ddr = kzalloc(sizeof(*ddr), GFP_KERNEL);
	if (!ddr)
		return;

	ddr->hw = *hw;

	ctrl.v = readl(apmu_ddrdfc_ctrl);
	mc_stat.v = readl(apmu_mc_clk_stat);

	pll4_clk = clk_get(NULL, "pll4");
	if (IS_ERR(pll4_clk))
		return;

	/* hwdfc require to turn on pll4 */
	clk_prepare_enable(pll4_clk);

	/* ignore sp and msk sync vblank */
	tmp = readl(apmu_debug2);
	tmp |= 0x1 << 20 | 0x3 << 18;
	writel(tmp, apmu_debug2);

	/* enable DDR DWDFC */
	ddr->flags = DDR_HWDFC_FEAT;

	/* if it is not set in blf */
	if (!ctrl.b.dfc_hw_en)
		ctrl.v = 0xd; /* lpddr2/lpddr3 */

	writel(ctrl.v, apmu_ddrdfc_ctrl);


	clk = clk_register(NULL, &ddr->hw);
	if (IS_ERR(clk)) {
		pr_err("error to register ddr clk\n");
		kfree(hw);
		return;
	}
	clk_register_clkdev(clk, "ddr", NULL);
	mmp_clk_add(unit, PXA1928_CLK_DDR, clk);

	clk_prepare_enable(clk);
}

static int __init cpu_max_speed_set(char *str)
{
	int freq;

	if (!get_option(&str, &freq))
		return 0;
	if (freq < 156000) {
		pr_err("Too low cpu_max parameter!\n");
		return 0;
	}
	cpu_max_freq = freq;
	return 1;
}
__setup("cpu_max=", cpu_max_speed_set);
