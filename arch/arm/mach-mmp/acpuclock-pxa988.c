/*
 *  linux/arch/arm/mach-mmp/acpuclock-pxa988.c
 *
 *  Author:	Zhoujie Wu <zjwu@marvell.com>
 *			Qiming Wu <wuqm@marvell.com>
 *  Copyright:	(C) 2012 Marvell International Ltd.
 *
 *  based on arch/arm/mach-mmp/acpuclock-pxa910.c
 *
 * This software program is licensed subject to the GNU General Public License
 * (GPL).Version 2,June 1991, available at http://www.fsf.org/copyleft/gpl.html
 *
 * (C) Copyright 2012 Marvell International Ltd.
 * All Rights Reserved
 */
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/cpumask.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/err.h>
#include <linux/hrtimer.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kernel_stat.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/tick.h>
#include <linux/uaccess.h>

#include <asm/io.h>
#include <mach/cputype.h>
#include <mach/clock-pxa988.h>
#include <mach/pxa988_ddr.h>
#include <mach/pxa988_lowpower.h>
#include <mach/regs-mpmu.h>
#include <mach/regs-apmu.h>
#include <mach/regs-ciu.h>
#include <mach/regs-mcu.h>
#include <plat/debugfs.h>

#ifdef CONFIG_EOF_FC_WORKAROUND
#include <mach/pxa168fb.h>
#endif

#define CREATE_TRACE_POINTS
#include <plat/pxa_trace.h>

static inline int has_feat_dll_bypass_opti(void)
{
	return cpu_is_pxa988() || cpu_is_pxa986();
}

/* core,ddr,axi clk src sel set register desciption */
union pmum_fccr {
	struct {
		unsigned int pll1fbd:9;
		unsigned int pll1refd:5;
		unsigned int pll1cen:1;
		unsigned int mfc:1;
		unsigned int reserved0:3;
		unsigned int axiclksel0:1;
		unsigned int reserved1:3;
		unsigned int ddrclksel:2;
		unsigned int axiclksel1:1;
		unsigned int seaclksel:2;
		unsigned int i2sclksel:1;
		unsigned int mohclksel:3;
	} b;
	unsigned int v;
};

/* core,ddr,axi clk src sel status register description */
union pmua_pllsel {
	struct {
		unsigned int cpclksel:2;
		unsigned int apclksel:2;
		unsigned int ddrclksel:2;
		unsigned int axiclksel:2;
		unsigned int reserved0:24;
	} b;
	unsigned int v;
};

/* core,ddr,axi clk div and fc trigger register description */
union pmua_cc {
	struct {
		unsigned int core_clk_div:3;
		unsigned int bus_mc_clk_div:3;
		unsigned int biu_clk_div:3;
		unsigned int l2_clk_div:3;
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

/* peri clk div set register description */
union pmua_cc2 {
	struct {
		unsigned int peri_clk_div:3;
		unsigned int peri_clk_dis:1;
		unsigned int reserved0:12;
		unsigned int cpu0_core_rst:1;
		unsigned int reserved1:1;
		unsigned int cpu0_dbg_rst:1;
		unsigned int cpu0_wdt_rst:1;
		unsigned int cpu1_core_rst:1;
		unsigned int reserved2:1;
		unsigned int cpu1_dbg_rst:1;
		unsigned int cpu1_wdt_rst:1;
		unsigned int reserved3:8;
	} b;
	unsigned int v;
};

/* core,ddr,axi div status register description */
union pmua_dm_cc {
	struct {
		unsigned int core_clk_div:3;
		unsigned int bus_mc_clk_div:3;
		unsigned int biu_clk_div:3;
		unsigned int l2_clk_div:3;
		unsigned int ddr_clk_div:3;
		unsigned int bus_clk_div:3;
		unsigned int async1:1;
		unsigned int async2:1;
		unsigned int async3:1;
		unsigned int async3_1:1;
		unsigned int async4:1;
		unsigned int async5:1;
		unsigned int cp_rd_status:1;
		unsigned int ap_rd_status:1;
		unsigned int cp_fc_done:1;
		unsigned int ap_fc_done:1;
		unsigned int dclk_fc_done:1;
		unsigned int aclk_fc_done:1;
		unsigned int reserved:2;
	} b;
	unsigned int v;
};

/* peri clk src sel status register description */
union pmua_dm_cc2 {
	struct {
		unsigned int peri_clk_div:3;
		unsigned int reserved:29;
	} b;
	unsigned int v;
};


#define AP_SRC_SEL_MASK		0x7
#define UNDEF_OP		-1
#define MHZ			(1000 * 1000)
#define MHZ_TO_KHZ		(1000)
#define KHZ_TO_HZ		(1000)
#define MAX_OP_NUM		8

/*
 * AP clock source:
 * 0x0 = PLL1 624 MHz
 * 0x1 = PLL1 1248 MHz  or PLL3_CLKOUT
 * (depending on PLL3_CR[18])
 * 0x2 = PLL2_CLKOUT
 * 0x3 = PLL2_CLKOUTP
 */
enum ap_clk_sel {
	AP_CLK_SRC_PLL1_624 = 0x0,
	AP_CLK_SRC_PLL1_1248 = 0x1,
	AP_CLK_SRC_PLL2 = 0x2,
	AP_CLK_SRC_PLL2P = 0x3,
	AP_CLK_SRC_PLL3P = 0x11,
};

/*
 * DDR/AXI clock source:
 * 0x0 = PLL1 416 MHz
 * 0x1 = PLL1 624 MHz
 * 0x2 = PLL2_CLKOUT
 * 0x3 = PLL2_CLKOUTP
 */
enum ddr_axi_clk_sel {
	DDR_AXI_CLK_SRC_PLL1_416 = 0x0,
	DDR_AXI_CLK_SRC_PLL1_624 = 0x1,
	DDR_AXI_CLK_SRC_PLL2 = 0x2,
	DDR_AXI_CLK_SRC_PLL2P = 0x3,
};

enum ddr_type {
	LPDDR2_400M = 0,
	LPDDR2_533M,
	DDR3,
};

struct cpu_rtcwtc {
	unsigned int max_pclk;	/* max rate could be used by this rtc/wtc */
	unsigned int l1_rtc;
	unsigned int l2_rtc;
};
struct pxa988_cpu_opt {
	unsigned int pclk;		/* core clock */
	unsigned int l2clk;		/* L2 cache interface clock */
	unsigned int pdclk;		/* DDR interface clock */
	unsigned int baclk;		/* bus interface clock */
	unsigned int periphclk;		/* PERIPHCLK */
	enum ap_clk_sel ap_clk_sel;	/* core src sel val */
	struct clk *parent;		/* core clk parent node */
	unsigned int ap_clk_src;	/* core src rate */
	unsigned int pclk_div;		/* core clk divider*/
	unsigned int l2clk_div;		/* L2 clock divider */
	unsigned int pdclk_div;		/* DDR interface clock divider */
	unsigned int baclk_div;		/* bus interface clock divider */
	unsigned int periphclk_div;	/* PERIPHCLK divider */
	unsigned int l1_rtc;		/* L1 cache RTC/WTC */
	unsigned int l2_rtc;		/* L2 cache RTC/WTC */
#ifdef Z1_MCK4_SYNC_WORKAROUND
	unsigned int cp_pclk;		/* cp core clock */
	unsigned int cp_busmc_clk;	/* cp bus mc clock */
	enum ap_clk_sel cp_clk_sel;	/* cp core src sel val */
	unsigned int cp_pclk_div;	/* cp core clk divider */
	unsigned int cp_busmc_clk_div;	/* cp bus mc clk divider */
	unsigned int combined_dclk;	/* combined ddr clock*/
#endif
};

struct pxa988_ddr_axi_opt {
	unsigned int dclk;		/* ddr clock */
	unsigned int ddr_tbl_index;	/* ddr FC table index */
	unsigned int aclk;		/* axi clock */
	enum ddr_axi_clk_sel ddr_clk_sel;/* ddr src sel val */
	enum ddr_axi_clk_sel axi_clk_sel;/* axi src sel val */
	unsigned int ddr_clk_src;	/* ddr src rate */
	unsigned int axi_clk_src;	/* axi src rate */
	struct clk *ddr_parent;		/* ddr clk parent node */
	struct clk *axi_parent;		/* axi clk parent node */
	unsigned int dclk_div;		/* ddr clk divider */
	unsigned int aclk_div;		/* axi clk divider */
};

/*
 * Below struct is used to describe cpu, ddr type and the corresponding
 * OPs used for the platform. chipid and ddrtype actually is SW flag.
 * 1. As HW chipid maybe the same in different chips, fuseid and chipid
 * is used to together to identify the differnet chip.
 * SW could hack below chipid to any value, as long as we could match
 * the chip and corresponding ops.
 * 2. DDRtype is neccessary, as we may use the same chip with different
 * DDR in different platform, such as LPDDR400 and LPDDR533. It also
 * possible that we could NOT run up to 533M even we use LPDDR533
 * due to silicon limitation. We could not only depend on the information
 * read from HW. It is better that platform tell us the ddrtype. Then FC
 * could know which DDR OPs could be used.
 */
struct platform_opt {
	unsigned int cpuid;
	unsigned int chipid;
	enum ddr_type ddrtype;
	char *cpu_name;
	struct pxa988_cpu_opt *cpu_opt;
	unsigned int cpu_opt_size;
	struct pxa988_ddr_axi_opt *ddr_axi_opt;
	unsigned int ddr_axi_opt_size;
};

/* DDR fc table: 0 - non flag; 1 - pause flag; 2 - end flag */
enum ddr_fc_table_flag {
	DDR_FC_TABLE_NONE = 0,
	DDR_FC_TABLE_PAUSE = 1,
	DDR_FC_TABLE_END = 2,
};

struct ddr_fc_table_cmd {
	unsigned int reg;
	unsigned int val;
	enum ddr_fc_table_flag flag;
};

/* mutex lock protecting frequency change */
static DEFINE_MUTEX(core_freqs_mutex);
static DEFINE_MUTEX(ddr_freqs_mutex);
static DEFINE_SPINLOCK(fc_seq_lock);

/* current platform OP struct */
static struct platform_opt *cur_platform_opt;

/* current core OP */
static struct pxa988_cpu_opt *cur_cpu_op;

/* current DDR/AXI OP */
static struct pxa988_ddr_axi_opt *cur_ddraxi_op;

/* record DDR request from CP, only for debugfs show function */
static bool cp_reset_block_ddr_fc;

static void get_cur_cpu_op(struct pxa988_cpu_opt *cop);
static void get_cur_ddr_axi_op(struct pxa988_ddr_axi_opt *cop);

static struct clk pxa988_ddr_clk;

static unsigned int uichipProfile;
int is_pxa988a0svc;

#ifdef CONFIG_DEBUG_FS
static DEFINE_PER_CPU(struct clk_dc_stat_info, cpu_dc_stat);
static void pxa988_cpu_dcstat_event(unsigned int cpu,
	enum clk_stat_msg msg, unsigned int tgtop);
static unsigned int pm_dro_status;
#endif

#ifdef CONFIG_EOF_FC_WORKAROUND
static int ddr_fc_failure = 0;
static struct pxa988_ddr_axi_opt *md_new_eof, *md_old_eof;
DECLARE_COMPLETION(ddr_eof_complete);
static atomic_t ddr_fc_trigger = ATOMIC_INIT(0);
extern atomic_t displayon;
#endif

/*
 * For 988:
 * L2CLK = PCLK / (L2_CLK_DIV +1)
 * BIU_CLK = L2_CLK / (BIU_CLK_DIV +1)
 * MC_CLK = L2_CLK / (MC_CLK_DIV +1)
 * Emei Zx:
 * PERIPHCLK = PCLK /2 * (PERI_CLK_DIV+1)
 * Emei Ax:
 * PERIPHCLK = PCLK /4 * (PERI_CLK_DIV+1)
 *
 * FIXME:
 * 1. pdclk/paclk can use 1:1 with l2clk when in low speed,
 * and 1:2 when pclk is in high speed
 * 2. For Emei Zx, PERIPHCLK is divided from pclk, the divider
 * is even and ranges from 2~16. It is better to select a lower
 * frequency for power saving since it does NOT have very higher
 * frequency requirement. Current DE suggests to use pclk/8 as
 * PERIPHCLK.
 * 3. For Emei Ax, PERIPHCLK divider is from 4~32.
 */

/*
 * PPs for z1z2, all are from pll2. According to DE's suggestion,
 * PLL2 has smaller clock skew between DCLK and *_ACLK than pll1.
 * Use safe PP combination for z1z2.
 */
static struct pxa988_cpu_opt pxa988_op_array_z1z2[] = {
	{
		.pclk = 150,
		.l2clk = 150,
		.pdclk = 75,
		.baclk = 75,
		.periphclk = 18,
		.ap_clk_sel = AP_CLK_SRC_PLL2,
#ifdef Z1_MCK4_SYNC_WORKAROUND
		.combined_dclk = 150,
		.cp_pclk = 300,
		.cp_busmc_clk = 150,
		.cp_clk_sel = AP_CLK_SRC_PLL2,
#endif
	},
	{
		.pclk = 300,
		.l2clk = 150,
		.pdclk = 150,
		.baclk = 150,
		.periphclk = 37,
		.ap_clk_sel = AP_CLK_SRC_PLL2,
#ifdef Z1_MCK4_SYNC_WORKAROUND
		.combined_dclk = 150,
		.cp_pclk = 300,
		.cp_busmc_clk = 150,
		.cp_clk_sel = AP_CLK_SRC_PLL2,
#endif
	},
	{
		.pclk = 600,
		.l2clk = 300,
		.pdclk = 300,
		.baclk = 150,
		.periphclk = 75,
		.ap_clk_sel = AP_CLK_SRC_PLL2,
#ifdef Z1_MCK4_SYNC_WORKAROUND
		.combined_dclk = 300,
		.cp_pclk = 300,
		.cp_busmc_clk = 150,
		.cp_clk_sel = AP_CLK_SRC_PLL2,
#endif
	},
	{
		.pclk = 800,
		.l2clk = 400,
		.pdclk = 400,
		.baclk = 200,
		.periphclk = 100,
		.ap_clk_sel = AP_CLK_SRC_PLL2P,
#ifdef Z1_MCK4_SYNC_WORKAROUND
		.combined_dclk = 400,
		.cp_pclk = 400,
		.cp_busmc_clk = 200,
		.cp_clk_sel = AP_CLK_SRC_PLL2,
#endif
	},
#if 0
	/*
	 * pll3 has duty cycle issue on Z1 if its rate is higher than 800M,
	 * disable 1G PP at first on Z1
	 */
	{
		.pclk = 1000,
		.l2clk = 500,
		.pdclk = 250,
		.baclk = 250,
		.periphclk = 125,
		.ap_clk_sel = AP_CLK_SRC_PLL3P,
	},
#endif
	{
		.pclk = 1200,
		.l2clk = 600,
		.pdclk = 300,
		.baclk = 300,
		.periphclk = 150,
		.ap_clk_sel = AP_CLK_SRC_PLL2,
#ifdef Z1_MCK4_SYNC_WORKAROUND
		.combined_dclk = 300,
		.cp_pclk = 300,
		.cp_busmc_clk = 150,
		.cp_clk_sel = AP_CLK_SRC_PLL2,
#endif
	},
};

static struct pxa988_cpu_opt pxa988_op_array_z3ax_lpddr400[] = {
	{
		.pclk = 312,
		.l2clk = 312,
		.pdclk = 156,
		.baclk = 156,
		.periphclk = 39,
		.ap_clk_sel = AP_CLK_SRC_PLL1_624,
	},
	{
		.pclk = 624,
		.l2clk = 312,
		.pdclk = 312,
		.baclk = 156,
		.periphclk = 78,
		.ap_clk_sel = AP_CLK_SRC_PLL1_624,
	},
	{
		.pclk = 800,
		.l2clk = 400,
		.pdclk = 400,
		.baclk = 200,
		.periphclk = 100,
		.ap_clk_sel = AP_CLK_SRC_PLL2,
	},
	{
		.pclk = 1205,
		.l2clk = 602,
		.pdclk = 602,
		.baclk = 301,
		.periphclk = 150,
		.ap_clk_sel = AP_CLK_SRC_PLL3P,
	},
};

static struct pxa988_cpu_opt pxa988_op_array_z3ax_lpddr533[] = {
	{
		.pclk = 312,
		.l2clk = 312,
		.pdclk = 156,
		.baclk = 156,
		.periphclk = 39,
		.ap_clk_sel = AP_CLK_SRC_PLL1_624,
	},
	{
		.pclk = 624,
		.l2clk = 312,
		.pdclk = 312,
		.baclk = 156,
		.periphclk = 78,
		.ap_clk_sel = AP_CLK_SRC_PLL1_624,
	},
	{
		.pclk = 1066,
		.l2clk = 533,
		.pdclk = 533,
		.baclk = 266,
		.periphclk = 133,
		.ap_clk_sel = AP_CLK_SRC_PLL2,
	},
	{
		.pclk = 1205,
		.l2clk = 602,
		.pdclk = 602,
		.baclk = 301,
		.periphclk = 150,
		.ap_clk_sel = AP_CLK_SRC_PLL3P,
	},
};
/*
 * 1) On Emei Z0, only support three ddr rates, be careful
 * when changing the PP table. The table should only have
 * three PPs and the PPs are ordered ascending.
 * 2) Table base DDR FC is implemented. The corresponding
 * ddr_tbl_index should be 1,3,5. If the PP tbl size is larger
 * than 3, it will only fill the first three rates' timing to tbl 1,3,5
 * 3) Make sure the ddr and axi rate's src sel is correct
 * 4) FIXME: high ddr request means high axi is NOT
 * very reasonable
 * 5) For Z1/Z2, we use PP150/300/400 from pll2 due to MCK4
 * sync issue. For Z3/Ax, we use PP156(pll1)/312(pll1)/400(pll2)
 * for bringup, may have chance to support DDR533M.
 */
static struct pxa988_ddr_axi_opt lpddr400_axi_oparray_z1z2[] = {
	{
		.dclk = 150,
		.ddr_tbl_index = 1,
		.aclk = 150,
		.ddr_clk_sel = DDR_AXI_CLK_SRC_PLL2,
		.axi_clk_sel = DDR_AXI_CLK_SRC_PLL2,
	},
	{
		.dclk = 300,
		.ddr_tbl_index = 3,
		.aclk = 150,
		.ddr_clk_sel = DDR_AXI_CLK_SRC_PLL2,
		.axi_clk_sel = DDR_AXI_CLK_SRC_PLL2,
	},
	{
		.dclk = 400,
		.ddr_tbl_index = 5,
		.aclk = 200,
		.ddr_clk_sel = DDR_AXI_CLK_SRC_PLL2P,
		.axi_clk_sel = DDR_AXI_CLK_SRC_PLL2P,
	},
};

static struct pxa988_ddr_axi_opt lpddr400_axi_oparray_z3ax[] = {
	{
		.dclk = 156,
		.ddr_tbl_index = 1,
		.aclk = 78,
		.ddr_clk_sel = DDR_AXI_CLK_SRC_PLL1_624,
		.axi_clk_sel = DDR_AXI_CLK_SRC_PLL1_624,
	},
	{
		.dclk = 312,
		.ddr_tbl_index = 3,
		.aclk = 156,
		.ddr_clk_sel = DDR_AXI_CLK_SRC_PLL1_624,
		.axi_clk_sel = DDR_AXI_CLK_SRC_PLL1_624,
	},
	{
		.dclk = 400,
		.aclk = 200,
		.ddr_tbl_index = 5,
		.ddr_clk_sel = DDR_AXI_CLK_SRC_PLL2P,
		.axi_clk_sel = DDR_AXI_CLK_SRC_PLL2P,
	},
};
static struct pxa988_ddr_axi_opt lpddr533_axi_oparray_z3ax[] = {
	{
		.dclk = 156,
		.ddr_tbl_index = 1,
		.aclk = 78,
		.ddr_clk_sel = DDR_AXI_CLK_SRC_PLL1_624,
		.axi_clk_sel = DDR_AXI_CLK_SRC_PLL1_624,
	},
	{
		.dclk = 312,
		.ddr_tbl_index = 3,
		.aclk = 156,
		.ddr_clk_sel = DDR_AXI_CLK_SRC_PLL1_624,
		.axi_clk_sel = DDR_AXI_CLK_SRC_PLL1_624,
	},
	{
		.dclk = 533,
		.ddr_tbl_index = 5,
		.aclk = 266,
		.ddr_clk_sel = DDR_AXI_CLK_SRC_PLL2P,
		.axi_clk_sel = DDR_AXI_CLK_SRC_PLL2P,
	},
};

static struct platform_opt platform_op_arrays[] = {
	{
		.cpuid = 0xc09,
		.chipid = 0xc9280,	/* dummy chipid for Z1Z2 */
		.ddrtype = LPDDR2_400M,
		.cpu_name = "PXA988_Z1Z2",
		.cpu_opt = pxa988_op_array_z1z2,
		.cpu_opt_size = ARRAY_SIZE(pxa988_op_array_z1z2),
		.ddr_axi_opt = lpddr400_axi_oparray_z1z2,
		.ddr_axi_opt_size = ARRAY_SIZE(lpddr400_axi_oparray_z1z2),
	},
	{
		.cpuid = 0xc09,
		.chipid = 0xc9281,	/* dummy chipid for Z3Ax */
		.ddrtype = LPDDR2_400M,
		.cpu_name = "PXA988_Z3Ax",
		.cpu_opt = pxa988_op_array_z3ax_lpddr400,
		.cpu_opt_size = ARRAY_SIZE(pxa988_op_array_z3ax_lpddr400),
		.ddr_axi_opt = lpddr400_axi_oparray_z3ax,
		.ddr_axi_opt_size = ARRAY_SIZE(lpddr400_axi_oparray_z3ax),
	},
	{
		.cpuid = 0xc09,
		.chipid = 0xc9281,	/* dummy chipid for Z3Ax */
		.ddrtype = LPDDR2_533M,
		.cpu_name = "PXA988_Z3Ax",
		.cpu_opt = pxa988_op_array_z3ax_lpddr533,
		.cpu_opt_size = ARRAY_SIZE(pxa988_op_array_z3ax_lpddr533),
		.ddr_axi_opt = lpddr533_axi_oparray_z3ax,
		.ddr_axi_opt_size = ARRAY_SIZE(lpddr533_axi_oparray_z3ax),
	},
};

static struct clk *cpu_sel2parent(enum ap_clk_sel ap_sel)
{
	if (ap_sel == AP_CLK_SRC_PLL1_624)
		return clk_get_sys(NULL, "pll1_624");
	else if (ap_sel == AP_CLK_SRC_PLL1_1248)
		return clk_get_sys(NULL, "pll1_1248");
	else if (ap_sel == AP_CLK_SRC_PLL2)
		return clk_get_sys(NULL, "pll2");
	else if (ap_sel == AP_CLK_SRC_PLL2P)
		return clk_get_sys(NULL, "pll2p");
	else if (ap_sel == AP_CLK_SRC_PLL3P)
		return clk_get_sys(NULL, "pll3p");
	else
		return ERR_PTR(-ENOENT);
}

static struct clk *ddr_axi_sel2parent(enum ddr_axi_clk_sel ddr_axi_sel)
{
	if (ddr_axi_sel == DDR_AXI_CLK_SRC_PLL1_416)
		return clk_get_sys(NULL, "pll1_416");
	else if (ddr_axi_sel == DDR_AXI_CLK_SRC_PLL1_624)
		return clk_get_sys(NULL, "pll1_624");
	else if (ddr_axi_sel == DDR_AXI_CLK_SRC_PLL2)
		return clk_get_sys(NULL, "pll2");
	else if (ddr_axi_sel == DDR_AXI_CLK_SRC_PLL2P)
		return clk_get_sys(NULL, "pll2p");
	else
		return ERR_PTR(-ENOENT);
}

static void __init __init_platform_opt(int ddr_mode)
{
	unsigned int i, chipid = 0;
	enum ddr_type ddrtype = LPDDR2_400M;
	struct platform_opt *proc;

	if (cpu_is_z1z2())
		chipid = 0xc9280;
	else
		chipid = 0xc9281;

	/*
	 * FIXME: ddr type is hacked here as it can not be read from
	 * HW, but FC code needs this info to identify DDR OPs.
	 */
	if (ddr_mode == 0)
	ddrtype = LPDDR2_400M;
	else if (ddr_mode == 1)
		ddrtype = LPDDR2_533M;

	for (i = 0; i < ARRAY_SIZE(platform_op_arrays); i++) {
		proc = platform_op_arrays + i;
		if ((proc->chipid == chipid) &&
			(proc->ddrtype == ddrtype))
			break;
	}
	BUG_ON(i == ARRAY_SIZE(platform_op_arrays));
	cur_platform_opt = proc;
}

static struct cpu_rtcwtc cpu_rtcwtc_z3[] = {
	{.max_pclk = 800, .l1_rtc = 0x88888888, .l2_rtc = 0x00008444,},
	{.max_pclk = 1205, .l1_rtc = 0x99999999, .l2_rtc = 0x00009555,},
};
static struct cpu_rtcwtc cpu_rtcwtc_ax[] = {
	{.max_pclk = 800, .l1_rtc = 0x88888888, .l2_rtc = 0x00008444,},
	{.max_pclk = 1066, .l1_rtc = 0x99999999, .l2_rtc = 0x00009555,},
	{.max_pclk = 1205, .l1_rtc = 0xAAAAAAAA, .l2_rtc = 0x0000A555,},
};
static void __init __init_cpu_rtcwtc(struct pxa988_cpu_opt *cpu_opt)
{
	struct cpu_rtcwtc *cpu_rtcwtc;
	unsigned int size, index;
	if (cpu_is_pxa988_z3() || cpu_is_pxa986_z3()) {
		cpu_rtcwtc = cpu_rtcwtc_z3;
		size = ARRAY_SIZE(cpu_rtcwtc_z3);
	} else if (cpu_is_pxa988_a0() || cpu_is_pxa986_a0()) {
		cpu_rtcwtc = cpu_rtcwtc_ax;
		size = ARRAY_SIZE(cpu_rtcwtc_ax);
	} else
		return;
	for (index = 0; index < size; index++)
		if (cpu_opt->pclk <= cpu_rtcwtc[index].max_pclk)
			break;
	if (index == size)
		index = size - 1;
	cpu_opt->l1_rtc = cpu_rtcwtc[index].l1_rtc;
	cpu_opt->l2_rtc = cpu_rtcwtc[index].l2_rtc;
};
static void __init __init_cpu_opt(void)
{
	struct clk *parent = NULL;
	struct pxa988_cpu_opt *cpu_opt, *cop;
	unsigned int cpu_opt_size = 0, i;

	cpu_opt = cur_platform_opt->cpu_opt;
	cpu_opt_size = cur_platform_opt->cpu_opt_size;

	pr_info("pclk(src:sel,div) l2clk(src,div)\t"\
		"pdclk(src,div)\tbaclk(src,div)\t"\
		"periphclk(src,div)\t l1_rtc:l2_rtc\n");

	for (i = 0; i < cpu_opt_size; i++) {
		cop = &cpu_opt[i];
		__init_cpu_rtcwtc(cop);
		parent = cpu_sel2parent(cop->ap_clk_sel);
		BUG_ON(IS_ERR(parent));
		cop->parent = parent;
		cop->ap_clk_src = clk_get_rate(parent) / MHZ;
		cop->pclk_div =
			cop->ap_clk_src / cop->pclk - 1;
		if (cop->l2clk) {
			cop->l2clk_div =
				cop->pclk / cop->l2clk - 1;
			cop->pdclk_div =
				cop->l2clk / cop->pdclk - 1;
			cop->baclk_div =
				cop->l2clk / cop->baclk - 1;
		} else {
			cop->pdclk_div =
				cop->pclk / cop->pdclk - 1;
			cop->baclk_div =
				cop->pclk / cop->baclk - 1;
		}
		if (cop->periphclk) {
			if (cpu_pxa98x_stepping() < PXA98X_A0)
				cop->periphclk_div =
					cop->pclk / (2 * cop->periphclk) - 1;
			else
				cop->periphclk_div =
					cop->pclk / (4 * cop->periphclk) - 1;
		}

		pr_info("%d(%d:%d,%d)\t%d([%s],%d)\t"\
			"%d([%s],%d)\t%d([%s],%d)\t%d([%s],%d)\t"\
			"0x%x:0x%x\n",
			cop->pclk, cop->ap_clk_src,
			cop->ap_clk_sel & AP_SRC_SEL_MASK,
			cop->pclk_div,
			cop->l2clk, cop->l2clk ? "pclk" : "NULL",
			cop->l2clk_div,
			cop->pdclk, cop->l2clk ? "l2clk" : "pclk",
			cop->pdclk_div,
			cop->baclk, cop->l2clk ? "l2clk" : "pclk",
			cop->baclk_div,
			cop->periphclk,
			cop->periphclk ? "pclk" : "NULL",
			cop->periphclk_div,
			cop->l1_rtc, cop->l2_rtc);

#ifdef Z1_MCK4_SYNC_WORKAROUND
		if (mck4_wr_enabled) {
			parent = cpu_sel2parent(cop->cp_clk_sel);
			BUG_ON(IS_ERR(parent));
			cop->cp_pclk_div =
				clk_get_rate(parent) / MHZ /
				cop->cp_pclk - 1;
			cop->cp_busmc_clk_div =
				clk_get_rate(parent) / MHZ /
				cop->cp_busmc_clk - 1;
			pr_info("%d(%lu:%d,%d)\t%d(%d)",\
			cop->cp_pclk, clk_get_rate(parent) / MHZ,
			cop->cp_clk_sel & AP_SRC_SEL_MASK,
			cop->cp_pclk_div, cop->cp_busmc_clk,
			cop->cp_busmc_clk_div);
		}
#endif
		pr_info("\n");
	}

}

static void __init __init_ddr_axi_opt(void)
{
	struct clk *parent = NULL;
	struct pxa988_ddr_axi_opt *ddr_axi_opt, *cop;
	unsigned int ddr_axi_opt_size = 0, i;

	ddr_axi_opt = cur_platform_opt->ddr_axi_opt;
	ddr_axi_opt_size = cur_platform_opt->ddr_axi_opt_size;

	pr_info("dclk(src:sel,div,tblindex) aclk(src:sel,div)\n");
	for (i = 0; i < ddr_axi_opt_size; i++) {
		cop = &ddr_axi_opt[i];
		parent = ddr_axi_sel2parent(cop->ddr_clk_sel);
		BUG_ON(IS_ERR(parent));
		cop->ddr_parent = parent;
		cop->ddr_clk_src =
			clk_get_rate(parent) / MHZ;
		cop->dclk_div =
			cop->ddr_clk_src / (2 * cop->dclk) - 1;

		parent = ddr_axi_sel2parent(cop->axi_clk_sel);
		BUG_ON(IS_ERR(parent));
		cop->axi_parent = parent;
		cop->axi_clk_src =
			clk_get_rate(parent) / MHZ;
		cop->aclk_div =
			cop->axi_clk_src / cop->aclk - 1;

		pr_info("%d(%d:%d,%d,%d)\t%d(%d:%d,%d)\n",
			cop->dclk, cop->ddr_clk_src,
			cop->ddr_clk_sel, cop->dclk_div,
			cop->ddr_tbl_index,
			cop->aclk, cop->axi_clk_src,
			cop->axi_clk_sel, cop->aclk_div);
	}
}

static void __init __init_fc_setting(void)
{
	unsigned int regval;
	union pmua_cc cc_ap, cc_cp;
	/*
	 * enable AP FC done interrupt for one step,
	 * while not use three interrupts by three steps
	 */
	__raw_writel((1 << 1), APMU_IMR);

	/* always vote for CP allow AP FC */
	cc_cp.v = __raw_readl(APMU_CP_CCR);
	cc_cp.b.core_allow_spd_chg = 1;
	__raw_writel(cc_cp.v, APMU_CP_CCR);

	regval = __raw_readl(APMU_DEBUG);
	/* CA9 doesn't support halt acknowledge, mask it */
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
	__raw_writel(regval, APMU_DEBUG);

	/*
	 * Always use async for DDR, AXI interface,
	 * and always vote for AP allow FC
	 */
	cc_ap.v = __raw_readl(APMU_CCR);
	cc_ap.b.async5 = 1;
	cc_ap.b.async4 = 1;
	cc_ap.b.async3_1 = 1;
	cc_ap.b.async3 = 1;
	cc_ap.b.async2 = 1;
	cc_ap.b.async1 = 1;
	cc_ap.b.core_allow_spd_chg = 1;
	__raw_writel(cc_ap.v, APMU_CCR);
}

static unsigned int cpu_rate2_op_index(unsigned int rate)
{
	unsigned int index;
	struct pxa988_cpu_opt *op_array =
		cur_platform_opt->cpu_opt;
	unsigned int op_array_size =
		cur_platform_opt->cpu_opt_size;

	if (unlikely(rate > op_array[op_array_size - 1].pclk))
		return op_array_size - 1;

	for (index = 0; index < op_array_size; index++)
		if (op_array[index].pclk >= rate)
			break;

	return index;
}

static unsigned int ddr_rate2_op_index(unsigned int rate)
{
	unsigned int index;
	struct pxa988_ddr_axi_opt *op_array =
		cur_platform_opt->ddr_axi_opt;
	unsigned int op_array_size =
		cur_platform_opt->ddr_axi_opt_size;

	if (unlikely(rate > op_array[op_array_size - 1].dclk))
		return op_array_size - 1;

	for (index = 0; index < op_array_size; index++)
		if (op_array[index].dclk >= rate)
			break;

	return index;
}

static int fc_lock_ref_cnt;
static int get_fc_lock(void)
{
	union pmua_dm_cc dm_cc_ap;

	fc_lock_ref_cnt++;

	if (fc_lock_ref_cnt == 1) {
		int timeout = 100000;

		/*
		 * AP-CP FC mutual exclusion,
		 * APMU_DM_CC_AP cp_rd_status = 0, ap_rd_status = 1
		 */
		dm_cc_ap.v = __raw_readl(APMU_CCSR);
		while (timeout) {
			if (!dm_cc_ap.b.cp_rd_status &&
				dm_cc_ap.b.ap_rd_status)
				break;
			dm_cc_ap.v = __raw_readl(APMU_CCSR);
			timeout--;
		}

		if (timeout <= 0) {
			pr_err("cp does not release its fc lock\n");
			pr_err("%s CCSR_AP when_lock:%x Now:%x CC_AP:%x\n",
				__func__, dm_cc_ap.v, __raw_readl(APMU_CCSR),
				__raw_readl(APMU_CCR));
			WARN_ON(1);
			return -EAGAIN;
		}
	}
	return 0;
}

static void put_fc_lock(void)
{
	union pmua_cc cc_ap;

	fc_lock_ref_cnt--;

	if (fc_lock_ref_cnt < 0)
		pr_err("unmatched put_fc_lock\n");

	if (fc_lock_ref_cnt == 0) {
		/* write 1 to MOH_RD_ST_CLEAR to clear MOH_RD_STATUS */
		cc_ap.v = __raw_readl(APMU_CCR);
		cc_ap.b.core_rd_st_clear = 1;
		__raw_writel(cc_ap.v, APMU_CCR);
		cc_ap.b.core_rd_st_clear = 0;
		__raw_writel(cc_ap.v, APMU_CCR);
	}
}

static void get_cur_cpu_op(struct pxa988_cpu_opt *cop)
{
	union pmua_pllsel pllsel;
	union pmua_dm_cc dm_cc_ap;
	union pmua_dm_cc2 dm_cc2_ap;
	unsigned int pll1_pll3_sel;
	struct clk *parent;

	get_fc_lock();

	dm_cc_ap.v = __raw_readl(APMU_CCSR);
	dm_cc2_ap.v = __raw_readl(APMU_CC2SR);
	pllsel.v = __raw_readl(APMU_PLL_SEL_STATUS);
	pll1_pll3_sel = __raw_readl(MPMU_PLL3CR);

	pr_debug("div%x sel%x\n", dm_cc_ap.v, pllsel.v);
	BUG_ON(!cop->parent);

	if (pllsel.b.apclksel == (cop->ap_clk_sel & AP_SRC_SEL_MASK))
		cop->ap_clk_src = clk_get_rate(cop->parent) / MHZ;
	else {
		if ((pllsel.b.apclksel == 0x1) && \
			(pll1_pll3_sel & (1 << 18))) {
			parent = cpu_sel2parent(AP_CLK_SRC_PLL3P);
			cop->ap_clk_sel = AP_CLK_SRC_PLL3P;
			cop->ap_clk_src = clk_get_rate(parent) / MHZ;
		} else {
			/* err case : current src is NOT our target */
			parent = cpu_sel2parent(pllsel.b.apclksel);
			cop->parent = parent;
			cop->ap_clk_sel = pllsel.b.apclksel;
			cop->ap_clk_src = clk_get_rate(parent) / MHZ;
			pr_err("%s cpu clk tsrc:%d csel:%d\n",
				__func__, cop->ap_clk_src, pllsel.b.apclksel);
		}
	}
	cop->pclk = cop->ap_clk_src / (dm_cc_ap.b.core_clk_div + 1);
	if (cop->l2clk) {
		cop->l2clk = cop->pclk / (dm_cc_ap.b.l2_clk_div + 1);
		cop->pdclk = cop->l2clk / (dm_cc_ap.b.bus_mc_clk_div + 1);
		cop->baclk = cop->l2clk / (dm_cc_ap.b.biu_clk_div + 1);
	} else {
		cop->pdclk = cop->pclk / (dm_cc_ap.b.bus_mc_clk_div + 1);
		cop->baclk = cop->pclk / (dm_cc_ap.b.biu_clk_div + 1);
	}
	if (cop->periphclk) {
		if (cpu_pxa98x_stepping() < PXA98X_A0)
			cop->periphclk =
				cop->pclk / (dm_cc2_ap.b.peri_clk_div + 1) / 2;
		else
			cop->periphclk =
				cop->pclk / (dm_cc2_ap.b.peri_clk_div + 1) / 4;
	}

	put_fc_lock();
}

static void get_cur_ddr_axi_op(struct pxa988_ddr_axi_opt *cop)
{
	union pmua_pllsel pllsel;
	union pmua_dm_cc dm_cc_ap;
	struct clk *parent;

	get_fc_lock();

	dm_cc_ap.v = __raw_readl(APMU_CCSR);
	pllsel.v = __raw_readl(APMU_PLL_SEL_STATUS);

	pr_debug("div%x sel%x\n", dm_cc_ap.v, pllsel.v);
	BUG_ON((!cop->ddr_parent) || (!cop->axi_parent));

	if (likely(pllsel.b.ddrclksel == cop->ddr_clk_sel))
		cop->ddr_clk_src = clk_get_rate(cop->ddr_parent) / MHZ;
	else {
		parent = ddr_axi_sel2parent(pllsel.b.ddrclksel);
		cop->ddr_parent = parent;
		cop->ddr_clk_sel = pllsel.b.ddrclksel;
		cop->ddr_clk_src = clk_get_rate(parent) / MHZ;
		pr_err("%s ddr clk tsrc:%d csel:%d parent:%s\n",
			__func__, cop->ddr_clk_src,
			pllsel.b.ddrclksel, cop->ddr_parent->name);
	}
	if (likely(pllsel.b.axiclksel == cop->axi_clk_sel))
		cop->axi_clk_src = clk_get_rate(cop->axi_parent) / MHZ;
	else {
		parent = ddr_axi_sel2parent(pllsel.b.axiclksel);
		cop->axi_parent = parent;
		cop->axi_clk_sel = pllsel.b.axiclksel;
		cop->axi_clk_src = clk_get_rate(parent) / MHZ;
		pr_err("%s axi clk tsrc:%d csel:%d parent:%s\n",
			__func__, cop->axi_clk_src,
			pllsel.b.axiclksel, cop->axi_parent->name);
	}
	cop->dclk = cop->ddr_clk_src / (dm_cc_ap.b.ddr_clk_div + 1) / 2;
	cop->aclk = cop->axi_clk_src / (dm_cc_ap.b.bus_clk_div + 1);

	put_fc_lock();
}

static void wait_for_fc_done(void)
{
	int timeout = 1000;
	while (!((1 << 1) & __raw_readl(APMU_ISR)) && timeout)
		timeout--;
	if (timeout <= 0) {
		WARN(1, "AP frequency change timeout!\n");
		pr_err("APMU_ISR %x\n", __raw_readl(APMU_ISR));
	}
	/* only clear AP fc done signal */
	__raw_writel(__raw_readl(APMU_ISR) & ~(1 << 1), APMU_ISR);
}

#ifdef Z1_MCK4_SYNC_WORKAROUND
static void wait_for_cp_fc_done(void)
{
	int timeout = 1000;
	while (!((1 << 0) & __raw_readl(APMU_ISR)) && timeout)
		timeout--;
	if (timeout <= 0)
		WARN(1, "CP frequency change timeout!\n");
	__raw_writel(0x0, APMU_ISR);
}
#endif

static void pll1_pll3_switch(enum ap_clk_sel sel)
{
	unsigned int regval;

	if ((sel != AP_CLK_SRC_PLL3P) ||
		(sel != AP_CLK_SRC_PLL1_1248))
		return;

	regval = __raw_readl(MPMU_PLL3CR);
	if (sel == AP_CLK_SRC_PLL1_1248)
		regval &= ~(1 << 18);
	else
		regval |= (1 << 18);
	__raw_writel(regval, MPMU_PLL3CR);
}

static void set_ap_clk_sel(struct pxa988_cpu_opt *top)
{
	union pmum_fccr fccr;
	unsigned int value;
	pll1_pll3_switch(top->ap_clk_sel);

	fccr.v = __raw_readl(MPMU_FCCR);
	fccr.b.mohclksel =
		top->ap_clk_sel & AP_SRC_SEL_MASK;
	__raw_writel(fccr.v, MPMU_FCCR);
	value = __raw_readl(MPMU_FCCR);
	if (value != fccr.v)
		pr_err("CORE FCCR Write failure: target 0x%X, final value 0x%X\n",
		      fccr.v, value);
}

static void set_periph_clk_div(struct pxa988_cpu_opt *top)
{
	union pmua_cc2 cc_ap2;

	cc_ap2.v = __raw_readl(APMU_CC2R);
	cc_ap2.b.peri_clk_div = top->periphclk_div;
	__raw_writel(cc_ap2.v, APMU_CC2R);
}

static void set_ddr_clk_sel(struct pxa988_ddr_axi_opt *top)
{
	union pmum_fccr fccr;
	unsigned int value;

	fccr.v = __raw_readl(MPMU_FCCR);
	fccr.b.ddrclksel = top->ddr_clk_sel;
	__raw_writel(fccr.v, MPMU_FCCR);
	value = __raw_readl(MPMU_FCCR);
	if (value != fccr.v)
		pr_err("DDR FCCR Write failure: target 0x%x, final value 0x%X\n",
		fccr.v, value);
}

static void set_axi_clk_sel(struct pxa988_ddr_axi_opt *top)
{
	union pmum_fccr fccr;
	unsigned int value;

	fccr.v = __raw_readl(MPMU_FCCR);
	fccr.b.axiclksel0 = top->axi_clk_sel & 0x1;
	fccr.b.axiclksel1 = (top->axi_clk_sel & 0x2) >> 1;
	__raw_writel(fccr.v, MPMU_FCCR);
	value = __raw_readl(MPMU_FCCR);
	if (value != fccr.v)
		pr_err("AXI FCCR Write failure: target 0x%x, final value 0x%X\n",
		fccr.v, value);
}

static void set_ddr_tbl_index(unsigned int index)
{
	unsigned int regval;

	/*
	 * FIXME: pmu_register_lock has to be added here
	 * to protect the register accessing of
	 * APMU_MC_HW_SLP_TYPE, it also be access by D1P
	 * low power mode workaround
	 */
	pmu_register_lock();
	index = (index > 0x7) ? 0x7 : index;
	regval = __raw_readl(APMU_MC_HW_SLP_TYPE);
	regval &= ~(0x1 << 6);		/* enable tbl based FC */
	regval &= ~(0x7 << 3);		/* clear ddr tbl index */
	regval |= (index << 3);
	__raw_writel(regval, APMU_MC_HW_SLP_TYPE);
	pmu_register_unlock();
}

#ifdef Z1_MCK4_SYNC_WORKAROUND
static void cp_core_fc_seq(struct pxa988_cpu_opt *cop,
			    struct pxa988_cpu_opt *top)
{
	union pmua_cc cc_cp;
	union pmum_fccr fccr;
	union pmua_pllsel pllsel;
	union pmua_dm_cc dm_cc_ap;

	/* Do not change CP rate if CP is in reset */
	if ((cop->cp_pclk == top->cp_pclk) ||\
		(__raw_readl(MPMU_APRR) & 0x1))
		return;

	/* 1) set CP pclk src */
	fccr.v = __raw_readl(MPMU_FCCR);
	fccr.b.seaclksel = top->cp_clk_sel & 0x3;
	__raw_writel(fccr.v, MPMU_FCCR);

	cc_cp.v = __raw_readl(APMU_CP_CCR);
	/* 2.1) select div for pclk, busmc clk for seguall */
	cc_cp.b.core_clk_div = top->cp_pclk_div;
	cc_cp.b.bus_mc_clk_div = top->cp_busmc_clk_div;

	/* 2.2) set div and FC req trigger core FC */
	cc_cp.b.core_allow_spd_chg = 1;
	cc_cp.b.core_freq_chg_req = 1;
	pr_debug("CORE FC APMU_CCR[%x]\n", cc_cp.v);
	__raw_writel(cc_cp.v, APMU_CP_CCR);
	wait_for_cp_fc_done();

	/* check sel and div */
	pllsel.v = __raw_readl(APMU_PLL_SEL_STATUS);
	dm_cc_ap.v = __raw_readl(APMU_CP_CCSR);
	if ((pllsel.b.cpclksel != (top->cp_clk_sel & 0x3)) ||\
		(dm_cc_ap.b.core_clk_div != top->cp_pclk_div) ||\
		(dm_cc_ap.b.bus_mc_clk_div != top->cp_busmc_clk_div)) {
		pr_info("sel %x, div %x\n", pllsel.v,
			dm_cc_ap.v);
		WARN_ON(1);
	}
}
#endif

#if defined(CONFIG_CPU_PXA988) && defined(CONFIG_SMP)
atomic_t freqchg_disable_c2 = ATOMIC_INIT(0);

static void do_nothing(void *data)
{
	;
}

static void smp_freqchg_pre(void)
{
	if (cpu_pxa98x_stepping() < PXA98X_A0)
		return ;
	atomic_set(&freqchg_disable_c2, 1);
	/* send ipi to all others online cpu */
	smp_call_function(do_nothing, NULL, 1);
}

static void smp_freqchg_post(void)
{
	if (cpu_pxa98x_stepping() < PXA98X_A0)
		return ;
	atomic_set(&freqchg_disable_c2, 0);
}
#endif

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
static void core_fc_seq(struct pxa988_cpu_opt *cop,
			    struct pxa988_cpu_opt *top)
{
	union pmua_cc cc_ap, cc_cp;

	/* low -> high */
	if ((!cpu_is_z1z2()) && \
		(cop->pclk < top->pclk) && \
		(top->l1_rtc != cop->l1_rtc)) {
		writel_relaxed(top->l1_rtc, \
			CIU_CA9_CPU_CONF_SRAM_0);
		writel_relaxed(top->l2_rtc, \
			CIU_CA9_CPU_CONF_SRAM_1);
	}

	/* 0) Pre FC : check CP allow AP FC voting */
	cc_cp.v = __raw_readl(APMU_CP_CCR);
	if (unlikely(!cc_cp.b.core_allow_spd_chg)) {
		pr_warning("%s CP doesn't allow AP FC!\n",
			__func__);
		cc_cp.b.core_allow_spd_chg = 1;
		__raw_writel(cc_cp.v, APMU_CP_CCR);
	}

	/* 1) Pre FC : AP votes allow FC */
	cc_ap.v = __raw_readl(APMU_CCR);
	cc_ap.b.core_allow_spd_chg = 1;

	/* 2) issue core FC */
	/* 2.1) set pclk src */
	set_ap_clk_sel(top);
	/* 2.2) select div for pclk, l2clk, pdclk, baclk */
	cc_ap.b.core_clk_div = top->pclk_div;
	if (top->l2clk)
		cc_ap.b.l2_clk_div = top->l2clk_div;
	cc_ap.b.bus_mc_clk_div = top->pdclk_div;
	cc_ap.b.biu_clk_div = top->baclk_div;
	/* 2.3) set periphclk div */
	if (top->periphclk)
		set_periph_clk_div(top);

	cc_ap.b.core_freq_chg_req = 1;
	/* used only for core FC, will NOT trigger fc_sm */
	/* cc_ap.b.core_dyn_fc = 1; */

	trace_pxa_core_clk_chg(CLK_CHG_ENTRY, cop->pclk, top->pclk);
	/* 2.4) set div and FC req trigger core FC */
	pr_debug("CORE FC APMU_CCR[%x]\n", cc_ap.v);
	__raw_writel(cc_ap.v, APMU_CCR);
	wait_for_fc_done();
	trace_pxa_core_clk_chg(CLK_CHG_EXIT, cop->pclk, top->pclk);

#ifdef Z1_MCK4_SYNC_WORKAROUND
	/* change CP clock here if mck4 wr enabled */
	if (mck4_wr_enabled)
		cp_core_fc_seq(cop, top);
#endif

	/* 3) Post FC : AP clear allow FC REQ */
	cc_ap.v = __raw_readl(APMU_CCR);
	cc_ap.b.core_freq_chg_req = 0;
	__raw_writel(cc_ap.v, APMU_CCR);

	/* high -> low */
	if ((!cpu_is_z1z2()) && \
		(cop->pclk > top->pclk) && \
		(top->l1_rtc != cop->l1_rtc)) {
		writel_relaxed(top->l1_rtc, \
			CIU_CA9_CPU_CONF_SRAM_0);
		writel_relaxed(top->l2_rtc, \
			CIU_CA9_CPU_CONF_SRAM_1);
	}
}

static int set_core_freq(struct pxa988_cpu_opt *old, struct pxa988_cpu_opt *new)
{
	struct pxa988_cpu_opt cop;
	struct clk *old_parent;
	int ret = 0;
	unsigned long flags;

	pr_debug("CORE set_freq start: old %u, new %u\n",
		old->pclk, new->pclk);

	memcpy(&cop, old, sizeof(struct pxa988_cpu_opt));
	get_cur_cpu_op(&cop);
	if (unlikely((cop.ap_clk_src != old->ap_clk_src) ||
		(cop.pclk != old->pclk) ||
		(cop.l2clk != old->l2clk) ||
		(cop.pdclk != old->pdclk) ||
		(cop.baclk != old->baclk) ||
		(cop.periphclk != old->periphclk))) {
		pr_err("psrc pclk l2clk pdclk baclk periphclk\n");
		pr_err("OLD %d %d %d %d %d %d\n", old->ap_clk_src,
		       old->pclk, old->l2clk, old->pdclk, old->baclk,
		       old->periphclk);
		pr_err("CUR %d %d %d %d %d %d\n", cop.ap_clk_src,
		       cop.pclk, cop.l2clk, cop.pdclk, cop.baclk,
		       cop.periphclk);
		pr_err("NEW %d %d %d %d %d %d\n", new->ap_clk_src,
		       new->pclk, new->l2clk, new->pdclk, new->baclk,
		       new->periphclk);
		dump_stack();
	}

	old_parent = cop.parent;
	clk_enable(new->parent);

#if defined(CONFIG_CPU_PXA988) && defined(CONFIG_SMP)
	smp_freqchg_pre();
#endif
	/* Get lock in irq disable status to short AP hold lock time */
	local_irq_save(flags);
	ret = get_fc_lock();
	if (ret) {
		local_irq_restore(flags);
		clk_disable(new->parent);
#if defined(CONFIG_CPU_PXA988) && defined(CONFIG_SMP)
		smp_freqchg_post();
#endif
		goto out;
	}
	core_fc_seq(&cop, new);
	local_irq_restore(flags);
#if defined(CONFIG_CPU_PXA988) && defined(CONFIG_SMP)
	smp_freqchg_post();
#endif

	memcpy(&cop, new, sizeof(struct pxa988_cpu_opt));
	get_cur_cpu_op(&cop);
	if (unlikely((cop.ap_clk_src != new->ap_clk_src) ||
		(cop.pclk != new->pclk) ||
		(cop.l2clk != new->l2clk) ||
		(cop.pdclk != new->pdclk) ||
		(cop.baclk != new->baclk) ||
		(cop.periphclk != new->periphclk))) {
		pr_err("unsuccessful frequency change!\n");
		pr_err("psrc pclk l2clk pdclk baclk periphclk\n");
		pr_err("CUR %d %d %d %d %d %d\n", cop.ap_clk_src,
		       cop.pclk, cop.l2clk, cop.pdclk, cop.baclk,
		       cop.periphclk);
		pr_err("NEW %d %d %d %d %d %d\n", new->ap_clk_src,
			new->pclk, new->l2clk, new->pdclk, new->baclk,
			new->periphclk);
		pr_err("FCCR %x, CCAP %x, PLLSEL %x, DMCCAP %x, CCCP %x\n",
			__raw_readl(MPMU_FCCR), __raw_readl(APMU_CCR),
			__raw_readl(APMU_PLL_SEL_STATUS), __raw_readl(APMU_CCSR),
			__raw_readl(APMU_CP_CCR));
		ret = -EAGAIN;
		if (cop.ap_clk_src != new->ap_clk_src) {
			/* restore current src */
			set_ap_clk_sel(&cop);
			pr_info("Recovered FCCR: %x\n",
				__raw_readl(MPMU_FCCR));
			clk_disable(new->parent);
		}
		goto out;
	}

	clk_disable(old_parent);
out:
	put_fc_lock();
	pr_debug("CORE set_freq end: old %u, new %u\n",
		old->pclk, new->pclk);
	return ret;
}

static void pxa988_cpu_init(struct clk *clk)
{
	unsigned int op_index;
	struct pxa988_cpu_opt cur_op;
	struct pxa988_cpu_opt *op_array;
#ifdef CONFIG_DEBUG_FS
	struct clk_dc_stat_info *cpu_dcstat;
	unsigned int opt_size, i, cpu;
#endif
	BUG_ON(!cur_platform_opt);

	/* get cur core rate */
	op_array = cur_platform_opt->cpu_opt;
	memcpy(&cur_op, &op_array[0], sizeof(struct pxa988_cpu_opt));
	get_cur_cpu_op(&cur_op);
	op_index = cpu_rate2_op_index(cur_op.pclk);
	cur_cpu_op = &op_array[op_index];

	clk->rate = op_array[op_index].pclk * MHZ;
	clk->parent = op_array[op_index].parent;
	clk->dynamic_change = 1;

	/* config the wtc/rtc value according to current frequency */
	if (!cpu_is_z1z2()) {
		writel_relaxed(op_array[op_index].l1_rtc, \
			CIU_CA9_CPU_CONF_SRAM_0);
		writel_relaxed(op_array[op_index].l2_rtc, \
			CIU_CA9_CPU_CONF_SRAM_1);
	}

	pr_info(" CPU boot up @%luHZ\n", clk->rate);

#ifdef Z1_MCK4_SYNC_WORKAROUND
	if (mck4_wr_enabled)
		clk_set_rate(&pxa988_ddr_clk,\
			op_array[op_index].combined_dclk * MHZ);
#endif

#ifdef CONFIG_DEBUG_FS
	opt_size = cur_platform_opt->cpu_opt_size;
	for_each_possible_cpu(cpu) {
		cpu_dcstat = &per_cpu(cpu_dc_stat, cpu);
		cpu_dcstat->ops_dcstat = kzalloc(opt_size * \
			sizeof(struct op_dcstat_info), GFP_KERNEL);
		if (!cpu_dcstat->ops_dcstat) {
			pr_err("%s clk %s memory allocate failed!\n",
				__func__, clk->name);
			return;
		}
		for (i = 0; i < opt_size; i++) {
			cpu_dcstat->ops_dcstat[i].ppindex = i;
			cpu_dcstat->ops_dcstat[i].pprate =
				op_array[i].pclk * MHZ;
		}
		cpu_dcstat->ops_stat_size = opt_size;
		cpu_dcstat->stat_start = false;
		cpu_dcstat->curopindex = op_index;
	}
#endif
}

static long pxa988_cpu_round_rate(struct clk *clk, unsigned long rate)
{
	struct pxa988_cpu_opt *op_array =
		cur_platform_opt->cpu_opt;
	unsigned int op_array_size =
		cur_platform_opt->cpu_opt_size, index;

	rate /= MHZ;

	if (unlikely(rate > op_array[op_array_size - 1].pclk))
		return op_array[op_array_size - 1].pclk * MHZ;

	for (index = 0; index < op_array_size; index++)
		if (op_array[index].pclk >= rate)
			break;

	return op_array[index].pclk * MHZ;
}

static int pxa988_cpu_setrate(struct clk *clk, unsigned long rate)
{
	struct pxa988_cpu_opt *md_new, *md_old;
	struct cpufreq_freqs freqs;
	unsigned int index, cpu;
	int ret = 0;
	struct pxa988_cpu_opt *op_array =
		cur_platform_opt->cpu_opt;

	rate /= MHZ;
	index = cpu_rate2_op_index(rate);

	md_new = &op_array[index];
	if (md_new == cur_cpu_op)
		return 0;

	mutex_lock(&core_freqs_mutex);
	md_old = cur_cpu_op;

#ifdef CONFIG_CPU_FREQ
	freqs.old = md_old->pclk * MHZ_TO_KHZ;
	freqs.new = md_new->pclk * MHZ_TO_KHZ;
	for_each_online_cpu(freqs.cpu)
		cpufreq_notify_transition(&freqs, CPUFREQ_PRECHANGE);
#endif

	/*
	 * FIXME: we do NOT enable clk here because pll3
	 * clk_enable and pll1_pll3_switch will do the
	 * same thing, we should handle it carefully.
	 * For example, pll1_1248 -> pll3, clk_enable(&pll3)
	 * will switch src to pll3, which will cause issue.
	 * clk_enable and disable will be handled in set_core_freq.
	 */
	/* clk_enable(md_new->parent); */

	spin_lock(&fc_seq_lock);

	/*
	 * Switching pll1_1248 and pll3p may generate glitch
	 * step 1),2),3) is neccessary
	 */
	if (((md_old->ap_clk_sel == AP_CLK_SRC_PLL3P) && \
		(md_new->ap_clk_sel == AP_CLK_SRC_PLL1_1248)) || \
		((md_old->ap_clk_sel == AP_CLK_SRC_PLL1_1248) && \
		(md_new->ap_clk_sel == AP_CLK_SRC_PLL3P))) {
		/* 1) use startup op(op0) as a bridge */
		ret = set_core_freq(md_old, &op_array[0]);
		if (ret)
			goto tmpout;
		/* 2) change PLL3_CR[18] to select pll1_1248 or pll3p */
		pll1_pll3_switch(md_new->ap_clk_sel);
		/* 3) switch to op which uses pll1_1248/pll3p */
		ret = set_core_freq(&op_array[0], md_new);
	} else {
		ret = set_core_freq(md_old, md_new);
	}

tmpout:
	spin_unlock(&fc_seq_lock);
	if (ret)
		goto out;
	cur_cpu_op = md_new;

	clk_reparent(clk, md_new->parent);
	/*clk_disable(md_old->parent);*/
#ifdef CONFIG_DEBUG_FS
	for_each_possible_cpu(cpu)
		pxa988_cpu_dcstat_event(cpu, CLK_RATE_CHANGE,
			index);
#endif
#ifdef Z1_MCK4_SYNC_WORKAROUND
	if (mck4_wr_enabled)
		clk_set_rate(&pxa988_ddr_clk,\
			op_array[index].combined_dclk * MHZ);
#endif

out:
#ifdef CONFIG_CPU_FREQ
	freqs.new = cur_cpu_op->pclk * MHZ_TO_KHZ;
	for_each_online_cpu(freqs.cpu)
		cpufreq_notify_transition(&freqs, CPUFREQ_POSTCHANGE);
#endif
	mutex_unlock(&core_freqs_mutex);
	return ret;
}

static unsigned long pxa988_cpu_getrate(struct clk *clk)
{
	if (cur_cpu_op)
		return cur_cpu_op->pclk * MHZ;
	else
		pr_err("%s: cur_cpu_op NULL\n", __func__);

	return 0;
}

/* do nothing only used to adjust proper clk->refcnt */
static int clk_dummy_enable(struct clk *clk)
{
	return 0;
}

static void clk_dummy_disable(struct clk *clk)
{
}

struct clkops cpu_clk_ops = {
	.init = pxa988_cpu_init,
	.enable = clk_dummy_enable,
	.disable = clk_dummy_disable,
	.round_rate = pxa988_cpu_round_rate,
	.setrate = pxa988_cpu_setrate,
	.getrate = pxa988_cpu_getrate,
};

static struct clk pxa988_cpu_clk = {
	.name = "cpu",
	.lookup = {
		.con_id = "cpu",
	},
	.ops = &cpu_clk_ops,
	.is_combined_fc = 1,
};

static void ddr_axi_fc_seq(struct pxa988_ddr_axi_opt *cop,
			    struct pxa988_ddr_axi_opt *top)
{
	union pmua_cc cc_ap, cc_cp;

	/* 0) Pre FC : check CP allow AP FC voting */
	cc_cp.v = __raw_readl(APMU_CP_CCR);
	if (unlikely(!cc_cp.b.core_allow_spd_chg)) {
		pr_warning("%s CP doesn't allow AP FC!\n",
			__func__);
		cc_cp.b.core_allow_spd_chg = 1;
		__raw_writel(cc_cp.v, APMU_CP_CCR);
	}

	/* 1) Pre FC : AP votes allow FC */
	cc_ap.v = __raw_readl(APMU_CCR);
	cc_ap.b.core_allow_spd_chg = 1;

	/* 2) issue DDR FC */
	if ((cop->ddr_clk_src != top->ddr_clk_src) || \
	    (cop->dclk != top->dclk)) {
		/* 2.1) set dclk src */
		set_ddr_clk_sel(top);
		/* 2.2) enable tbl based FC and set DDR tbl num */
		set_ddr_tbl_index(top->ddr_tbl_index);
		/* 2.3) select div for dclk */
		cc_ap.b.ddr_clk_div = top->dclk_div;
		/* 2.4) select ddr FC req bit */
		cc_ap.b.ddr_freq_chg_req = 1;
	}

	/* 3) issue AXI FC */
	if ((cop->axi_clk_src != top->axi_clk_src) || \
	    (cop->aclk != top->aclk)) {
		/* 3.1) set aclk src */
		set_axi_clk_sel(top);
		/* 3.2) select div for aclk */
		cc_ap.b.bus_clk_div = top->aclk_div;
		/* 3.3) select axi FC req bit */
		cc_ap.b.bus_freq_chg_req = 1;
	}

	trace_pxa_ddraxi_clk_chg(CLK_CHG_ENTRY, cop->dclk, top->dclk);
	/* 4) set div and FC req bit trigger DDR/AXI FC */
	pr_debug("DDR FC APMU_CCR[%x]\n", cc_ap.v);
	__raw_writel(cc_ap.v, APMU_CCR);
	wait_for_fc_done();
	trace_pxa_ddraxi_clk_chg(CLK_CHG_EXIT, cop->dclk, top->dclk);

	/* 5) Post FC : AP clear allow FC REQ */
	cc_ap.v = __raw_readl(APMU_CCR);
	cc_ap.b.ddr_freq_chg_req = 0;
	cc_ap.b.bus_freq_chg_req = 0;
	__raw_writel(cc_ap.v, APMU_CCR);
}

static int set_ddr_axi_freq(struct pxa988_ddr_axi_opt *old,
	struct pxa988_ddr_axi_opt *new)
{
	struct pxa988_ddr_axi_opt cop;
	struct clk *ddr_old_parent, *axi_old_parent;
	int ret = 0, errflag = 0;
	unsigned long flags;

	pr_debug("DDR set_freq start: old %u, new %u\n",
		old->dclk, new->dclk);

	memcpy(&cop, old, sizeof(struct pxa988_ddr_axi_opt));
	get_cur_ddr_axi_op(&cop);
	if (unlikely((cop.ddr_clk_src != old->ddr_clk_src) ||
		(cop.axi_clk_src != old->axi_clk_src) ||
		(cop.dclk != old->dclk) ||
		(cop.aclk != old->aclk))) {
		pr_err(" dsrc dclk asrc aclk");
		pr_err("OLD %d %d %d %d\n", old->ddr_clk_src,
		       old->dclk, old->axi_clk_src, old->aclk);
		pr_err("CUR %d %d %d %d\n", cop.ddr_clk_src,
		       cop.dclk, cop.axi_clk_src, cop.aclk);
		pr_err("NEW %d %d %d %d\n", new->ddr_clk_src,
		       new->dclk, new->axi_clk_src, new->aclk);
		dump_stack();
	}

	ddr_old_parent = cop.ddr_parent;
	axi_old_parent = cop.axi_parent;
	clk_enable(new->ddr_parent);
	clk_enable(new->axi_parent);
#if defined(CONFIG_CPU_PXA988) && defined(CONFIG_SMP)
	smp_freqchg_pre();
#endif
	/* Get lock in irq disable status to short AP hold lock time */
	local_irq_save(flags);
	ret = get_fc_lock();
	if (ret) {
		local_irq_restore(flags);
		clk_disable(new->ddr_parent);
		clk_disable(new->axi_parent);
#if defined(CONFIG_CPU_PXA988) && defined(CONFIG_SMP)
		smp_freqchg_post();
#endif
		goto out;
	}
	ddr_axi_fc_seq(&cop, new);
	local_irq_restore(flags);
#if defined(CONFIG_CPU_PXA988) && defined(CONFIG_SMP)
	smp_freqchg_post();
#endif

	memcpy(&cop, new, sizeof(struct pxa988_ddr_axi_opt));
	get_cur_ddr_axi_op(&cop);
	if (unlikely((cop.ddr_clk_src != new->ddr_clk_src) ||
		(cop.dclk != new->dclk))) {
		clk_disable(new->ddr_parent);
		errflag = 1;
	}
	if (unlikely((cop.axi_clk_src != new->axi_clk_src) ||
		(cop.aclk != new->aclk))) {
		clk_disable(new->axi_parent);
		errflag = 1;
	}
	if (unlikely(errflag)) {
		pr_err("DDR_AXI:unsuccessful frequency change!\n");
		pr_err(" dsrc dclk asrc aclk");
		pr_err("CUR %d %d %d %d\n", cop.ddr_clk_src,
		       cop.dclk, cop.axi_clk_src, cop.aclk);
		pr_err("NEW %d %d %d %d\n", new->ddr_clk_src,
		       new->dclk, new->axi_clk_src, new->aclk);
		pr_err("FCCR %x, CCAP %x, PLLSEL %x, DMCCAP %x, CCCP %x\n",
			__raw_readl(MPMU_FCCR), __raw_readl(APMU_CCR),
			__raw_readl(APMU_PLL_SEL_STATUS), __raw_readl(APMU_CCSR),
			__raw_readl(APMU_CP_CCR));
		/* restore current src */
		set_ddr_clk_sel(&cop);
		set_axi_clk_sel(&cop);
		pr_info("Recovered FCCR: %x\n", __raw_readl(MPMU_FCCR));
		ret = -EAGAIN;
		goto out;
	}

	clk_disable(ddr_old_parent);
	clk_disable(axi_old_parent);
out:
	put_fc_lock();
	pr_debug("DDR set_freq end: old %u, new %u\n",
		old->dclk, new->dclk);
	return ret;
}

static void ddr_lpm_tbl_update(int bypass)
{
	trace_pxa_ddr_lpm_tbl_update(bypass);

	if (bypass) {
		INSERT_ENTRY(0x0, 0xC, 0x0);
		INSERT_ENTRY(PHY_CTRL14_PHY_SYNC, 0x2024C, 0x1);
	} else {
		INSERT_ENTRY(0x0, 0xC, 0x0);
		INSERT_ENTRY(PHY_CTRL14_DLL_RESET, 0x24C, 0x1);
		INSERT_ENTRY(PHY_CTRL14_DLL_UPDATE, 0x24C, 0x2);
		INSERT_ENTRY(PHY_CTRL14_PHY_SYNC, 0x2024C, 0x3);
	}
}

static void pxa988_ddraxi_init(struct clk *clk)
{
	struct pxa988_ddr_axi_opt cur_op;
	struct pxa988_ddr_axi_opt *ddr_axi_opt;
	unsigned int op_index;
	unsigned long axi_rate;
#ifdef CONFIG_DEBUG_FS
	unsigned int op_array_size, i;
	unsigned long op[MAX_OP_NUM];
#endif
	BUG_ON(!cur_platform_opt);

	/* get core cur frequency */
	ddr_axi_opt = cur_platform_opt->ddr_axi_opt;
	memcpy(&cur_op, &ddr_axi_opt[0],
		sizeof(struct pxa988_ddr_axi_opt));
	get_cur_ddr_axi_op(&cur_op);
	op_index = ddr_rate2_op_index(cur_op.dclk);
	cur_ddraxi_op = &ddr_axi_opt[op_index];

	/*
	 * If the init DDR freq is lower than 400Mhz, optimize the
	 * DDR lpm table to bypass dll reset/update.
	 */
	if (has_feat_dll_bypass_opti()) {
		if (ddr_axi_opt[op_index].dclk < 400)
			ddr_lpm_tbl_update(1);
		else
			ddr_lpm_tbl_update(0);
	}

	clk->rate = ddr_axi_opt[op_index].dclk * MHZ;
	clk->parent = ddr_axi_opt[op_index].ddr_parent;
	clk->dynamic_change = 1;
	axi_rate = ddr_axi_opt[op_index].aclk * MHZ;
	/*
	 * As axi has no seperate clock node, so have to adjust axi's
	 * current parents' ref->cnt when system boots up
	 */
	clk_enable(ddr_axi_opt[op_index].axi_parent);

	pr_info(" DDR boot up @%luHZ\n", clk->rate);
	pr_info(" AXI boot up @%luHZ\n", axi_rate);

#ifdef CONFIG_DEBUG_FS
	op_array_size = cur_platform_opt->ddr_axi_opt_size;
	for (i = 0; i < op_array_size; i++)
		op[i] = ddr_axi_opt[i].dclk * MHZ;
	pxa988_clk_register_dcstat(clk, op, op_array_size);
#endif
}

static long pxa988_ddraxi_round_rate(struct clk *clk, unsigned long rate)
{
	struct pxa988_ddr_axi_opt *op_array =
		cur_platform_opt->ddr_axi_opt;
	unsigned int op_array_size =
		cur_platform_opt->ddr_axi_opt_size;
	unsigned int index;

	rate /= MHZ;

	if (unlikely(rate > op_array[op_array_size - 1].dclk))
		return op_array[op_array_size - 1].dclk * MHZ;

	for (index = 0; index < op_array_size; index++)
		if (op_array[index].dclk >= rate)
			break;

	return op_array[index].dclk * MHZ;
}

#ifdef CONFIG_EOF_FC_WORKAROUND
int wakeup_ddr_fc_seq(void)
{
	int ret = 0;
	if (atomic_read(&ddr_fc_trigger)) {
		atomic_set(&ddr_fc_trigger, 0);
		pr_debug("EOF_FC: start ddr fc!\n");
		spin_lock(&fc_seq_lock);
		ret = set_ddr_axi_freq(md_old_eof, md_new_eof);
		spin_unlock(&fc_seq_lock);
		if (ret)
			ddr_fc_failure = 1;

		pr_debug("EOF_FC: ddr fc done\n");
		complete(&ddr_eof_complete);
	}

	return 0;
}
#endif

/*
 * For the DDR freq < threshold, we use DLL bypass mode, so we could
 * use optimized DDR table 0 to skip DLL reset and DLL update to
 * save the DDR restore time when system exit from D1pp or deeper
 * state.
 */
static void ddr_lpm_tbl_optimize(unsigned int old, unsigned int new,
		unsigned int threshold)
{
	if (has_feat_dll_bypass_opti()) {
		if ((old >= threshold) && (new < threshold))
			ddr_lpm_tbl_update(1);
		else if ((old < threshold) && (new >= threshold))
			ddr_lpm_tbl_update(0);
	}
}

static int pxa988_ddraxi_setrate(struct clk *clk, unsigned long rate)
{
	struct pxa988_ddr_axi_opt *md_new, *md_old;
	unsigned int index;
	int ret = 0;
	struct pxa988_ddr_axi_opt *op_array =
		cur_platform_opt->ddr_axi_opt;

	rate /= MHZ;
	index = ddr_rate2_op_index(rate);

	md_new = &op_array[index];
	if (md_new == cur_ddraxi_op)
		return 0;

	mutex_lock(&ddr_freqs_mutex);
	md_old = cur_ddraxi_op;

	/* clk_enable(md_new->ddr_parent); */

#ifdef CONFIG_EOF_FC_WORKAROUND
	pr_debug("EOF_FC: ddr freq to %d\n", md_new->dclk);

	/*
	 * Only use EOF workaournd when decreasing DDR frequency to 156Mhz
	 * while LCD is on. And this workaround is only suitable for Z3 since
	 * Z3 does not require safe PP.
	 */
	ddr_fc_failure = 0;

	if ((cpu_is_pxa988_z3() || cpu_is_pxa986_z3())
			&& ((md_new->dclk == 156) || (md_new->dclk == 312))
			&& atomic_read(&displayon)) {
		md_old_eof = md_old;
		md_new_eof = md_new;
		/* Enable LCD panel path EOF interrupt before DDR freq-chg */
		irq_unmask_eof(0);
		atomic_set(&disable_c2, 1);
		atomic_set(&ddr_fc_trigger, 1);
		/*
		 * Now after the ddr fc is triggered, we will DO ddr freq-chg at
		 * first EOF, so we should never meet the 50ms timeout case.
		 */
		if (!wait_for_completion_timeout(&ddr_eof_complete,
				msecs_to_jiffies(50))) {
			if (atomic_read(&ddr_fc_trigger)) {
				atomic_set(&ddr_fc_trigger, 0);
				pr_debug("EOF_FC: wait eof timeout! force ddr fc!\n");
				spin_lock(&fc_seq_lock);
				ret = set_ddr_axi_freq(md_old, md_new);
				if (!ret)
					ddr_lpm_tbl_optimize(md_old->dclk,
						md_new->dclk, 400);
				spin_unlock(&fc_seq_lock);
				if (ret)
					ddr_fc_failure = 1;
			} else {
				pr_debug("EOF_FC: ddr fc is running, wait!\n");
				wait_for_completion(&ddr_eof_complete);
			}
		} else
			pr_debug("EOF_FC: wait eof done!\n");
		atomic_set(&disable_c2, 0);
		/* Disable LCD panel path EOF interrupt after DDR freq-chg */
		irq_mask_eof(0);
	} else {
		spin_lock(&fc_seq_lock);
		ret = set_ddr_axi_freq(md_old, md_new);
		if (!ret)
			ddr_lpm_tbl_optimize(md_old->dclk,
				md_new->dclk, 400);

		spin_unlock(&fc_seq_lock);
	}

	if (ret || (ddr_fc_failure == 1))
		goto out;
#else
	spin_lock(&fc_seq_lock);
	ret = set_ddr_axi_freq(md_old, md_new);
	if (!ret)
		ddr_lpm_tbl_optimize(md_old->dclk,
			md_new->dclk, 400);

	spin_unlock(&fc_seq_lock);
	if (ret)
		goto out;
#endif
	cur_ddraxi_op = md_new;

	clk_reparent(clk, md_new->ddr_parent);
	/* clk_disable(md_old->ddr_parent); */
#ifdef CONFIG_DEBUG_FS
	pxa988_clk_dcstat_event(clk, CLK_RATE_CHANGE, index);
#endif

#ifdef Z1_MCK4_SYNC_WORKAROUND
	trigger_bind2ddr_clk_rate(rate * MHZ);
#endif
out:
	mutex_unlock(&ddr_freqs_mutex);
	return ret;
}

static unsigned long pxa988_ddraxi_getrate(struct clk *clk)
{
	if (cur_ddraxi_op)
		return cur_ddraxi_op->dclk * MHZ;
	else
		pr_err("%s: cur_ddraxi_op NULL\n", __func__);

	return 0;
}

struct clkops ddr_clk_ops = {
	.init = pxa988_ddraxi_init,
	.enable = clk_dummy_enable,
	.disable = clk_dummy_disable,
	.round_rate = pxa988_ddraxi_round_rate,
	.setrate = pxa988_ddraxi_setrate,
	.getrate = pxa988_ddraxi_getrate,
};

static struct clk pxa988_ddr_clk = {
	.name = "ddr",
	.lookup = {
		.con_id = "ddr",
	},
	.ops = &ddr_clk_ops,
	.is_combined_fc = 1,
};

int ddr_mode = 0;
static int __init __init_ddr_mode(char *arg)
{
	int n;
	if (!get_option(&arg, &n))
		return 0;
	ddr_mode = n;
	if ((ddr_mode != 0) && (ddr_mode != 1))
		pr_info("WARNING: unknown DDR type!");
	return 1;
}
__setup("ddr_mode=", __init_ddr_mode);
/*
 * Every ddr/axi FC, fc_sm will halt AP,CP and
 * wait for the halt_ack from AP and CP.
 * If CP is in reset state, CP can not send this ack.
 * and system may hang. SW need to set a debug
 * register to ignore the CP ack if CP is in reset.
 */
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
	mutex_lock(&ddr_freqs_mutex);
	cp_reset_block_ddr_fc = true;
}
EXPORT_SYMBOL(acquire_fc_mutex);

/* called after release cp */
void release_fc_mutex(void)
{
	cp_reset_block_ddr_fc = false;
	mutex_unlock(&ddr_freqs_mutex);
}
EXPORT_SYMBOL(release_fc_mutex);

/* Interface used to get ddr op num */
unsigned int pxa988_get_ddr_op_num(void)
{
	return cur_platform_opt->ddr_axi_opt_size;
}

/* Interface used to get ddr avaliable rate, unit khz */
unsigned int pxa988_get_ddr_op_rate(unsigned int index)
{
	struct pxa988_ddr_axi_opt *ddr_opt;

	if (index >= cur_platform_opt->ddr_axi_opt_size) {
		pr_err("%s index out of range!\n", __func__);
		return -EINVAL;
	}

	ddr_opt = cur_platform_opt->ddr_axi_opt;
	return ddr_opt[index].dclk * MHZ_TO_KHZ;
}

#define MAX_PP_STR_LEN	10
#define MAX_PP_NUM	10
static unsigned int pp_disable[MAX_PP_NUM];
static unsigned int pp_discnt;
static int removepp(char *s)
{
	int tbl_size = platform_op_arrays->cpu_opt_size;
	int i, j, re_range = 0;

	for (pp_discnt = 0; pp_discnt < tbl_size; pp_discnt++) {
		pp_disable[pp_discnt] = simple_strtol(s, &s, 10);
		s++;
		if (!pp_disable[pp_discnt])
			break;
		if (pp_discnt)
			if (pp_disable[pp_discnt] < pp_disable[pp_discnt - 1])
				re_range = 1;
	}
	if (!re_range)
		return 1;
	for (i = 0; i < pp_discnt; i++) {
		for (j = 0; j < pp_discnt - i - 1; j++) {
			if (pp_disable[j] > pp_disable[j + 1]) {
				unsigned int tmp = pp_disable[j];
				pp_disable[j] = pp_disable[j + 1];
				pp_disable[j + 1] = tmp;
			}
		}
	}

	return 1;
}
__setup("core_nopp=", removepp);

#ifdef CONFIG_CPU_FREQ_TABLE
static struct cpufreq_frequency_table *cpufreq_tbl;

static void __init_cpufreq_table(void)
{
	struct pxa988_cpu_opt *cpu_opt;
	unsigned int cpu_opt_size = 0, i, j, k;

	cpu_opt = cur_platform_opt->cpu_opt;
	cpu_opt_size = cur_platform_opt->cpu_opt_size;

	cpufreq_tbl =
		kmalloc(sizeof(struct cpufreq_frequency_table) * \
					(cpu_opt_size + 1), GFP_KERNEL);
	if (!cpufreq_tbl)
		return;

	for (i = 0, j = 0, k = 0; \
		i < cpu_opt_size; i++) {
		if (pp_disable[j] && j < pp_discnt) {
			if (pp_disable[j] == cpu_opt[i].pclk) {
				j++;
				continue;
			}
		}
		cpufreq_tbl[k].index = k;
		cpufreq_tbl[k].frequency = cpu_opt[i].pclk * MHZ_TO_KHZ;
		pr_info("Actual cpufreq table [%d]'s freq : %u\n",\
			cpufreq_tbl[k].index, cpufreq_tbl[k].frequency);
		k++;
	}

	cpufreq_tbl[k].index = k;
	cpufreq_tbl[k].frequency = CPUFREQ_TABLE_END;

	for_each_possible_cpu(i)
		cpufreq_frequency_table_get_attr(cpufreq_tbl, i);
}
#else
#define __init_cpufreq_table() do {} while (0);
#endif

#define NUM_PROFILES	9
static unsigned int convert_FusesToProfile(unsigned int uiFuses)
{
	unsigned int uiProfile = 0;
	unsigned int uiTemp = 3, uiTemp2 = 3;
	unsigned int i;

	for (i = 0; i < NUM_PROFILES; i++) {
		uiTemp |= uiTemp2 << (i * 2);
		if (uiTemp == uiFuses)
			uiProfile = i + 1;
	}

	return uiProfile;
}

unsigned int get_profile(void)
{
	return uichipProfile;
}

#define UIMAINFUSE_31_00 (AXI_VIRT_BASE + 0x1410)
#define UIMAINFUSE_63_32 (AXI_VIRT_BASE + 0x1414)
#define UIMAINFUSE_95_64 (AXI_VIRT_BASE + 0x1418)
#define BLOCK0_224_255	 (AXI_VIRT_BASE + 0x1420)
static int __init __init_read_droinfo(void)
{
	unsigned int uiMainFuse_63_32 = 0;
	unsigned int uiMainFuse_95_64 = 0;
	unsigned int uiMainFuse_31_00 = 0;
	unsigned int uiAllocRev;
	unsigned int uiFab;
	unsigned int uiRun;
	unsigned int uiWafer;
	unsigned int uiX;
	unsigned int uiY;
	unsigned int uiParity;
	unsigned int uiDRO_Avg;
	unsigned int uiGeuStatus;
	unsigned int uiFuses;

	is_pxa988a0svc = 0;

	/*
	* Read out DRO value, need enable GEU clock, if already disable,
	* need enable it firstly
	*/
	uiGeuStatus = __raw_readl(APMU_GEU);
	if (!(uiGeuStatus & 0x08)) {
		__raw_writel((uiGeuStatus | 0x09), APMU_GEU);
		udelay(10);
	}

	uiMainFuse_31_00 = __raw_readl(UIMAINFUSE_31_00);
	uiMainFuse_63_32 = __raw_readl(UIMAINFUSE_63_32);
	uiMainFuse_95_64 = __raw_readl(UIMAINFUSE_95_64);
	uiFuses = __raw_readl(BLOCK0_224_255);

	__raw_writel(uiGeuStatus, APMU_GEU);

	pr_info("  0x%x   0x%x   0x%x\n",
		uiMainFuse_31_00, uiMainFuse_63_32,
		uiMainFuse_95_64);

	uiAllocRev	= uiMainFuse_31_00 & 0x7;
	uiFab		= (uiMainFuse_31_00 >>  3) & 0x1f;
	uiRun		= ((uiMainFuse_63_32 & 0x3) << 24)
		| ((uiMainFuse_31_00 >> 8) & 0xffffff);
	uiWafer		= (uiMainFuse_63_32 >>  2) & 0x1f;
	uiX		= (uiMainFuse_63_32 >>  7) & 0xff;
	uiY		= (uiMainFuse_63_32 >> 15) & 0xff;
	uiParity	= (uiMainFuse_63_32 >> 23) & 0x1;
	uiDRO_Avg	= (uiMainFuse_95_64 >>  4) & 0x3ff;
	if (cpu_is_pxa988_z1() || cpu_is_pxa986_z1())
		uiDRO_Avg = (uiDRO_Avg * 1254) / 1000;

	if (cpu_is_pxa988_a0() || cpu_is_pxa986_a0()) {
		is_pxa988a0svc = (((uiFuses >> 14) & 0x3) == 0x3) ? 1 : 0;
		if (is_pxa988a0svc && (uiMainFuse_95_64 & (0x1 << 3)))
			is_pxa988a0svc = 2;
		pr_info("is_pxa988a0svc = %d\n", is_pxa988a0svc);
	}

	if (!cpu_is_z1z2()) {
		/* bit 240 ~ 255 for Profile information */
		uiFuses = (uiFuses >> 16) & 0x0000FFFF;
		uichipProfile = convert_FusesToProfile(uiFuses);
	}


	pm_dro_status = uiDRO_Avg;

	pr_info(" ");
	pr_info("	 *************************** ");
	pr_info("	 *	ULT: %08X%08X  * ",
		uiMainFuse_63_32, uiMainFuse_31_00);
	pr_info("	 *************************** ");
	pr_info("	 ULT decoded below ");
	pr_info("		alloc_rev[2:0]	= %d", uiAllocRev);
	pr_info("		fab [ 7: 3]	= %d", uiFab);
	pr_info("		run [33: 8]	= %d (0x%X)", uiRun, uiRun);
	pr_info("		 wafer [38:34]	= %d", uiWafer);
	pr_info("		 x [46:39]	= %d", uiX);
	pr_info("		 y [54:47]	= %d", uiY);
	pr_info("		parity [55:55]	= %d", uiParity);
	pr_info("   *************************** ");
	pr_info("   *************************** ");
	pr_info("		DRO [77:68]	= %d", uiDRO_Avg);
	pr_info("		Profile	= %d", uichipProfile);
	pr_info("   *************************** ");
	pr_info(" ");

	return uiDRO_Avg;
}
core_initcall(__init_read_droinfo);

static int __init pxa988_freq_init(void)
{
	__init_platform_opt(ddr_mode);

	__init_cpu_opt();
	__init_ddr_axi_opt();
	__init_fc_setting();

	pxa988_init_one_clock(&pxa988_ddr_clk);
	clk_enable(&pxa988_ddr_clk);
	pxa988_init_one_clock(&pxa988_cpu_clk);
	clk_enable(&pxa988_cpu_clk);

#ifdef Z1_MCK4_SYNC_WORKAROUND
	trigger_bind2ddr_clk_rate(clk_get_rate(&pxa988_ddr_clk));
#endif
	__init_cpufreq_table();

	return 0;
}
postcore_initcall_sync(pxa988_freq_init);

#ifdef CONFIG_DEBUG_FS
static ssize_t pxa988_pm_dro_read(struct file *filp, char __user *buffer,
	size_t count, loff_t *ppos)
{
	char buf[256] = { 0 };
	int len = 0;
	size_t size = sizeof(buf) - 1;

	len += snprintf(buf, size,
		"DRO Fuse Value : %u MHz\n", pm_dro_status);
	if (!cpu_is_z1z2())
		len += snprintf(buf + len, size,
			"Profile : %u \n", uichipProfile);

	return simple_read_from_buffer(buffer, count, ppos, buf, len);
}

static const struct file_operations pxa988_pm_dro_ops = {
	.read = pxa988_pm_dro_read,
};

static int __init __init_create_pm_dro_node(void)
{
	struct dentry *pm;
	struct dentry *pm_dro_op;

	pm = debugfs_create_dir("PM", NULL);
	if (!pm)
		return -ENOENT;

	pm_dro_op = debugfs_create_file("DRO_Status", 0444,
		pm, NULL, &pxa988_pm_dro_ops);
	if (!pm_dro_op)
		goto err_pm;

	return 0;

err_pm:
	debugfs_remove(pm);
	return -ENOENT;
}

static int dump_cpu_op(char *buf, size_t size,
		struct pxa988_cpu_opt *q)
{
	return snprintf(buf, size, "pclk:%d pdclk:%d baclk:%d l2clk:%d "\
			"periphclk:%d ap_clk_src:%d\n",
			q->pclk, q->pdclk, q->baclk, q->l2clk,
			q->periphclk, q->ap_clk_src);
}

static int dump_ddr_axi_op(char *buf, size_t size,
		struct pxa988_ddr_axi_opt *q)
{
	return snprintf(buf, size, "dclk:%d aclk:%d ddr_clk_src:%d "\
			"axi_clk_src:%d\n",
			q->dclk, q->aclk, q->ddr_clk_src,
			q->axi_clk_src);
}

/* Display current operating point */
static ssize_t cur_cpu_op_show(struct file *filp, char __user *buffer,
	size_t count, loff_t *ppos)
{
	char buf[256];
	int len = 0;
	size_t size = sizeof(buf) - 1;

	len = dump_cpu_op(buf, size - len, cur_cpu_op);
	return simple_read_from_buffer(buffer, count, ppos, buf, len);
}

const struct file_operations dp_cur_cpu_op_fops = {
	.read = cur_cpu_op_show,
};

static ssize_t cur_ddr_axi_op_show(struct file *filp, char __user *buffer,
	size_t count, loff_t *ppos)
{
	char buf[256];
	int len = 0;
	size_t size = sizeof(buf) - 1;

	len = dump_ddr_axi_op(buf, size - len, cur_ddraxi_op);
	return simple_read_from_buffer(buffer, count, ppos, buf, len);
}

const struct file_operations dp_cur_ddr_axi_op_fops = {
	.read = cur_ddr_axi_op_show,
};

/* Dump all operating point */
static ssize_t ops_show(struct file *filp, char __user *buffer,
	size_t count, loff_t *ppos)
{
	char *p;
	int len = 0;
	size_t ret;
	unsigned int i;
	struct pxa988_cpu_opt *cpu_ops =
		cur_platform_opt->cpu_opt;
	unsigned int cpu_ops_size =
		cur_platform_opt->cpu_opt_size;
	struct pxa988_ddr_axi_opt *ddr_ops =
		cur_platform_opt->ddr_axi_opt;
	unsigned int ddr_ops_size =
		cur_platform_opt->ddr_axi_opt_size;
	size_t size = PAGE_SIZE - 1;

	p = (char *)__get_free_pages(GFP_NOIO, 0);
	if (!p)
		return -ENOMEM;

	len += snprintf(p + len, size - len, "CPU OP:\n");
	for (i = 0; i < cpu_ops_size; i++)
		len += dump_cpu_op(p + len, size - len, cpu_ops + i);
	len += snprintf(p + len, size - len, "\n");

	len += snprintf(p + len, size - len, "DDR_AXI OP:\n");
	for (i = 0; i < ddr_ops_size; i++)
		len += dump_ddr_axi_op(p + len, size - len, ddr_ops + i);
	len += snprintf(p + len, size - len, "\n");

	if (len == size)
		pr_warn("%s The dump buf is not large enough!\n", __func__);

	ret = simple_read_from_buffer(buffer, count, ppos, p, len);
	free_pages((unsigned long)p, 0);

	return ret;
}

const struct file_operations dp_ops_fops = {
	.read = ops_show,
};

/* show CP block AP DDR FC status */
static ssize_t cp_block_ddr_fc_show(struct file *filp,
	char __user *buffer, size_t count, loff_t *ppos)
{
	char buf[32];
	int len = 0;
	size_t size = sizeof(buf) - 1;

	len = snprintf(buf, size, "%d\n", cp_reset_block_ddr_fc);
	return simple_read_from_buffer(buffer, count, ppos, buf, len);
}

const struct file_operations cp_block_ddr_fc_fops = {
	.read = cp_block_ddr_fc_show,
};

static inline u64 get_cpu_idle_time_jiffy(unsigned int cpu,
			u64 *wall)
{
	u64 idle_time;
	u64 cur_wall_time;
	u64 busy_time;

	cur_wall_time = jiffies64_to_cputime64(get_jiffies_64());

	busy_time  = kcpustat_cpu(cpu).cpustat[CPUTIME_USER];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_SYSTEM];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_IRQ];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_SOFTIRQ];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_STEAL];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_NICE];

	idle_time = cur_wall_time - busy_time;
	if (wall)
		*wall = jiffies_to_usecs(cur_wall_time);

	return jiffies_to_usecs(idle_time);
}

static inline cputime64_t get_cpu_idle_time(unsigned int cpu,
			cputime64_t *wall)
{
	u64 idle_time = get_cpu_idle_time_us(cpu, NULL);

	if (idle_time == -1ULL)
		return get_cpu_idle_time_jiffy(cpu, wall);
	else
		idle_time += get_cpu_iowait_time_us(cpu, wall);

	return idle_time;
}

static void pxa988_cpu_dcstat_event(unsigned int cpu,
	enum clk_stat_msg msg, unsigned int tgtop)
{
	struct clk_dc_stat_info *dc_stat_info = NULL;
	cputime64_t cur_wall, cur_idle;
	cputime64_t prev_wall, prev_idle;
	u32 idle_time_ms, total_time_ms;
	struct op_dcstat_info *cur, *tgt;

	dc_stat_info = &per_cpu(cpu_dc_stat, cpu);
	cur = &dc_stat_info->ops_dcstat[dc_stat_info->curopindex];
	if (msg == CLK_RATE_CHANGE) {
		BUG_ON(tgtop >= dc_stat_info->ops_stat_size);
		dc_stat_info->curopindex = tgtop;
	}
	/* do nothing if no stat operation is issued */
	if (!dc_stat_info->stat_start)
		return ;

	cur_idle = get_cpu_idle_time(cpu, &cur_wall);
	prev_wall = cur->prev_cpu_wall;
	prev_idle = cur->prev_cpu_idle;
	idle_time_ms = (u32)(cur_idle - prev_idle);
	total_time_ms = (u32)(cur_wall - prev_wall);
	idle_time_ms /= 1000;
	total_time_ms /= 1000;

	switch (msg) {
	case CLK_STAT_START:
		cur->prev_cpu_wall = cur_wall;
		cur->prev_cpu_idle = cur_idle;
		break;
	case CLK_STAT_STOP:
		cur->busy_time += (total_time_ms - idle_time_ms);
		cur->idle_time += idle_time_ms;
		break;
	case CLK_RATE_CHANGE:
		/* rate change from old->new */
		cur->prev_cpu_idle = cur_idle;
		cur->prev_cpu_wall = cur_wall;
		cur->busy_time += (total_time_ms - idle_time_ms);
		cur->idle_time += idle_time_ms;
		tgt = &dc_stat_info->ops_dcstat[tgtop];
		tgt->prev_cpu_idle = cur_idle;
		tgt->prev_cpu_wall = cur_wall;
		break;
	default:
		break;
	}
}

static ssize_t pxa988_cpu_dc_read(struct file *filp,
	char __user *buffer, size_t count, loff_t *ppos)
{
	char *buf;
	int len = 0;
	size_t ret, size = PAGE_SIZE - 1;
	unsigned int cpu, i, dc_int = 0, dc_fra = 0;
	struct clk_dc_stat_info *percpu_stat = NULL;
	unsigned int total_time, run_total, idle_total, busy_time;
	u64 av_mips;

	buf = (char *)__get_free_pages(GFP_NOIO, 0);
	if (!buf)
		return -ENOMEM;

	percpu_stat = &per_cpu(cpu_dc_stat, 0);
	if (percpu_stat->stat_start) {
		len += snprintf(buf + len, size - len,
			"Please stop the cpu duty cycle stats at first\n");
		goto out;
	}

	for_each_possible_cpu(cpu) {
		percpu_stat = &per_cpu(cpu_dc_stat, cpu);
		av_mips = run_total = idle_total = 0;
		for (i = 0; i < percpu_stat->ops_stat_size; i++) {
			idle_total += percpu_stat->ops_dcstat[i].idle_time;
			run_total += percpu_stat->ops_dcstat[i].busy_time;
			av_mips += (u64)(percpu_stat->ops_dcstat[i].pprate / MHZ *\
				percpu_stat->ops_dcstat[i].busy_time);
		}
		total_time = idle_total + run_total;
		av_mips = total_time ? div_u64(av_mips * MHZ, total_time) : 0;
		dc_int = total_time ? \
			calculate_dc(run_total, total_time, &dc_fra) : 0;
		dc_fra = total_time ? dc_fra : 0;
		len += snprintf(buf + len, size - len,
			"\n| CPU %u | %10s %ums| %10s %ums| %10s %2u.%2u%%|"\
			" %10s %luHZ |\n", cpu, "idle time", idle_total,
			"total time",  total_time,
			"duty cycle", dc_int, dc_fra,
			"average mips", (unsigned long)av_mips);
		len += snprintf(buf + len, size - len,
			"| %3s | %12s | %15s | %15s | %15s |\n",
			"OP#", "rate(HZ)", "run time(ms)",
			"idle time(ms)", "rt ratio");
		for (i = 0; i < percpu_stat->ops_stat_size; i++) {
			if (total_time) {
				busy_time =
					percpu_stat->ops_dcstat[i].busy_time;
				dc_int = calculate_dc(busy_time, total_time,
							&dc_fra);
			}
			len += snprintf(buf + len, size - len,
				"| %3u | %12lu | %15ld | %15ld | %12u.%2u%%|\n",
				percpu_stat->ops_dcstat[i].ppindex,
				percpu_stat->ops_dcstat[i].pprate,
				percpu_stat->ops_dcstat[i].busy_time,
				percpu_stat->ops_dcstat[i].idle_time,
				dc_int, dc_fra);
		}

	}
out:
	if (len == size)
		pr_warn("%s The dump buf is not large enough!\n", __func__);

	ret = simple_read_from_buffer(buffer, count, ppos, buf, len);
	free_pages((unsigned long)buf, 0);
	return ret;
}

static ssize_t pxa988_cpu_dc_write(struct file *filp,
		const char __user *buffer, size_t count, loff_t *ppos)
{
	unsigned int start, cpu, i;
	char buf[10] = { 0 };
	struct clk_dc_stat_info *percpu_stat = NULL;

	if (copy_from_user(buf, buffer, count))
		return -EFAULT;

	sscanf(buf, "%d", &start);
	start = !!start;
	percpu_stat = &per_cpu(cpu_dc_stat, 0);
	if (start == percpu_stat->stat_start) {
		pr_err("[WARNING]CPU stat is already %s\n",
			percpu_stat->stat_start ?\
			"started" : "stopped");
		return -EINVAL;
	}

	/*
	 * hold the same lock of clk_enable, disable, set_rate ops
	 * here to avoid the status change when start/stop and lead
	 * to incorrect stat info
	 */
	clk_get_lock(&pxa988_cpu_clk);
	if (start) {
		/* clear old stat information */
		for_each_possible_cpu(cpu) {
			percpu_stat = &per_cpu(cpu_dc_stat, cpu);
			for (i = 0; i < percpu_stat->ops_stat_size; i++) {
				percpu_stat->ops_dcstat[i].idle_time = 0;
				percpu_stat->ops_dcstat[i].busy_time = 0;
			}
			percpu_stat->stat_start = true;
			pxa988_cpu_dcstat_event(cpu, CLK_STAT_START, 0);
		}
	} else {
		for_each_possible_cpu(cpu) {
			percpu_stat = &per_cpu(cpu_dc_stat, cpu);
			pxa988_cpu_dcstat_event(cpu, CLK_STAT_STOP, 0);
			percpu_stat->stat_start = false;
		}
	}
	clk_release_lock(&pxa988_cpu_clk);
	return count;
}

static const struct file_operations pxa988_cpu_dc_ops = {
	.owner = THIS_MODULE,
	.read = pxa988_cpu_dc_read,
	.write = pxa988_cpu_dc_write,
};

static ssize_t pxa988_ddr_dc_read(struct file *filp,
	char __user *buffer, size_t count, loff_t *ppos)
{
	char *p;
	int len = 0;
	size_t ret, size = PAGE_SIZE - 1;

	p = (char *)__get_free_pages(GFP_NOIO, 0);
	if (!p)
		return -ENOMEM;

	len = pxa988_show_dc_stat_info(&pxa988_ddr_clk, p, size);
	if (len == size)
		pr_warn("%s The dump buf is not large enough!\n", __func__);

	ret = simple_read_from_buffer(buffer, count, ppos, p, len);
	free_pages((unsigned long)p, 0);
	return ret;
}

static ssize_t pxa988_ddr_dc_write(struct file *filp,
		const char __user *buffer, size_t count, loff_t *ppos)
{
	unsigned int start;
	char buf[10] = { 0 };
	size_t ret = 0;

	if (copy_from_user(buf, buffer, count))
		return -EFAULT;

	sscanf(buf, "%d", &start);
	ret = pxa988_start_stop_dc_stat(&pxa988_ddr_clk, start);
	if (ret < 0)
		return ret;
	return count;
}

static const struct file_operations pxa988_ddr_dc_ops = {
	.owner = THIS_MODULE,
	.read = pxa988_ddr_dc_read,
	.write = pxa988_ddr_dc_write,
};

static int __init __init_cpu_ddr_dcstat_node(void)
{
	struct dentry *cpu_dc_stat, *ddr_dc_stat;

	cpu_dc_stat = debugfs_create_file("cpu_dc_stat", 0664,
		stat, NULL, &pxa988_cpu_dc_ops);
	if (!cpu_dc_stat)
		return -ENOENT;

	ddr_dc_stat = debugfs_create_file("ddr_dc_stat", 0664,
		stat, NULL, &pxa988_ddr_dc_ops);
	if (!ddr_dc_stat)
		goto err_cpu_dc_stat;
	return 0;

err_cpu_dc_stat:
	debugfs_remove(cpu_dc_stat);
	return -ENOENT;
}

static int __init __init_create_fc_debugfs_node(void)
{
	struct dentry *fc;
	struct dentry *dp_cur_cpu_op, *dp_cur_ddraxi_op, *dp_ops;
	struct dentry *dp_cp_block_ddr_fc;
	int ret = 0;

	fc = debugfs_create_dir("fc", pxa);
	if (!fc)
		return -ENOENT;

	dp_cur_cpu_op = debugfs_create_file("cur_cpu_op", 0444,
		fc, NULL, &dp_cur_cpu_op_fops);
	if (!dp_cur_cpu_op)
		goto err_cur_cpu_op;

	dp_cur_ddraxi_op = debugfs_create_file("cur_ddr_axi_op", 0444,
		fc, NULL, &dp_cur_ddr_axi_op_fops);
	if (!dp_cur_ddraxi_op)
		goto err_cur_ddraxi_op;

	dp_ops = debugfs_create_file("ops", 0444,
		fc, NULL, &dp_ops_fops);
	if (!dp_ops)
		goto err_dp_ops;

	dp_cp_block_ddr_fc = debugfs_create_file("cp_block_ddr_fc", 0444,
		fc, NULL, &cp_block_ddr_fc_fops);
	if (!dp_cp_block_ddr_fc)
		goto err_dp_cp_block_ddr_fc;

	ret = __init_cpu_ddr_dcstat_node();
	return ret;

err_dp_cp_block_ddr_fc:
	debugfs_remove(dp_ops);
	dp_ops = NULL;
err_dp_ops:
	debugfs_remove(dp_cur_ddraxi_op);
	dp_cur_ddraxi_op = NULL;
err_cur_ddraxi_op:
	debugfs_remove(dp_cur_cpu_op);
	dp_cur_cpu_op = NULL;
err_cur_cpu_op:
	debugfs_remove(fc);
	fc = NULL;
	return -ENOENT;
}
late_initcall(__init_create_fc_debugfs_node);
late_initcall(__init_create_pm_dro_node);
#endif

static unsigned long pxa988_periph_getrate(struct clk *clk)
{
	unsigned long periph_rate;

	if (likely(cur_cpu_op))
		periph_rate = cur_cpu_op->periphclk * MHZ;
	else
		/*
		* when cur_cpu_op is not ready yet, calculate it from lpj,
		* it's [core freq] / 8.
		*/
		periph_rate = loops_per_jiffy * 2 * HZ / 8;

	return periph_rate;
}

struct clkops periph_clk_ops = {
	.getrate = pxa988_periph_getrate,
};

static struct clk pxa988_periph_clk = {
	.name = "smp_twd",
	.lookup = {
		.dev_id = "smp_twd",
	},
	.ops = &periph_clk_ops,
};

void __init pxa988_init_early(void)
{
	pxa988_init_one_clock(&pxa988_periph_clk);
}
