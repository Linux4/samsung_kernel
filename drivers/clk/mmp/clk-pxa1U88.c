#include <linux/io.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#include <linux/devfreq.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include <dt-bindings/clock/marvell-pxa1U88.h>
#include <linux/debugfs-pxa.h>
#include <linux/cputype.h>

#include "clk.h"
#include "clk-pll-helanx.h"
#include "clk-core-helanx.h"
#include "clk-plat.h"

#define APBS_PLL1_CTRL		0x100

#define APBC_RTC		0x28
#define APBC_TWSI0		0x2c
#define APBC_TWSI1		0x60
#define APBC_TWSI3		0x70
#define APBC_KPC		0x30
#define APBC_UART0		0x0
#define APBC_UART1		0x4
#define APBC_GPIO		0x8
#define APBC_PWM0		0xc
#define APBC_PWM1		0x10
#define APBC_PWM2		0x14
#define APBC_PWM3		0x18
#define APBC_SSP0		0x1c
#define APBC_SSP1		0x20
#define APBC_SWJTAG		0x40
#define APBC_SSP2		0x4c
#define APBC_TERMAL		0x6c

/* IPC/RIPC clock */
#define APBC_IPC_CLK_RST	0x24
#define APBCP_AICER		0x38

#define APBCP_TWSI2		0x28
#define APBCP_UART2		0x1c

#define MPMU_UART_PLL		0x14

#define APMU_CLK_GATE_CTRL	0x40
#define APMU_CCIC0		0x50
#define APMU_CCIC1		0x24
#define APMU_ISP	    0x38
#define APMU_SDH0		0x54
#define APMU_SDH1		0x58
#define APMU_SDH2		0xe0
#define APMU_USB		0x5c
#define APMU_NF			0x60
#define APMU_TRACE		0x108

#define APMU_CORE_STATUS 0x090
#define APMU_DVC_DFC_DEBUG  0x140

/* PLL */
#define MPMU_PLL2CR	   (0x0034)
#define MPMU_PLL3CR	   (0x001c)
#define MPMU_PLL4CR	   (0x0050)
#define MPMU_POSR	   (0x0010)
#define POSR_PLL2_LOCK	   (1 << 29)
#define POSR_PLL3_LOCK	   (1 << 30)
#define POSR_PLL4_LOCK	   (1 << 31)

#define APB_SPARE_PLL2CR    (0x104)
#define APB_SPARE_PLL3CR    (0x108)
#define APB_SPARE_PLL4CR    (0x124)
#define APB_PLL2_SSC_CTRL   (0x130)
#define APB_PLL2_SSC_CONF   (0x134)
#define APB_PLL2_FREQOFFSET_CTRL    (0x138)
#define APB_PLL3_SSC_CTRL   (0x13c)
#define APB_PLL3_SSC_CONF   (0x140)
#define APB_PLL3_FREQOFFSET_CTRL    (0x144)
#define APB_PLL4_SSC_CTRL   (0x148)
#define APB_PLL4_SSC_CONF   (0x14c)
#define APB_PLL4_FREQOFFSET_CTRL    (0x150)

#define APMU_GC			0xcc
#define APMU_GC2D		0xf4

#define APMU_VPU		0xa4
#define APMU_DSI1		0x44
#define APMU_DISP1		0x4c

#define CIU_MC_CONF	0x0040
#define VPU_XTC		0x00a8
#define GPU2D_XTC	0x00a0
#define GPU_XTC		0x00a4
#define SC2_DESC	0xD420F000
#define ISP_XTC		0x84C
#define TOP_MEM_RTC_WTC_SPD 0x44

#ifdef CONFIG_SMC91X
#define APMU_SMC	0xd4
#define SMC_CLK		0xd4283890
#endif

struct pxa1U88_clk_unit {
	struct mmp_clk_unit unit;
	void __iomem *mpmu_base;
	void __iomem *apmu_base;
	void __iomem *apbc_base;
	void __iomem *apbcp_base;
	void __iomem *apbs_base;
	void __iomem *ciu_base;
};

static struct mmp_param_fixed_rate_clk fixed_rate_clks[] = {
	{PXA1U88_CLK_CLK32, "clk32", NULL, CLK_IS_ROOT, 32768},
	{PXA1U88_CLK_VCTCXO, "vctcxo", NULL, CLK_IS_ROOT, 26000000},
	{PXA1U88_CLK_PLL1_624, "pll1_624", NULL, CLK_IS_ROOT, 624000000},
	{PXA1U88_CLK_PLL1_416, "pll1_416", NULL, CLK_IS_ROOT, 416000000},
	{PXA1U88_CLK_PLL1_499, "pll1_499", NULL, CLK_IS_ROOT, 499000000},
	{PXA1U88_CLK_PLL1_832, "pll1_832", NULL, CLK_IS_ROOT, 832000000},
	{PXA1U88_CLK_PLL1_1248, "pll1_1248", NULL, CLK_IS_ROOT, 1248000000},
};

static struct mmp_param_fixed_factor_clk fixed_factor_clks[] = {
	{PXA1U88_CLK_PLL1_2, "pll1_2", "pll1_624", 1, 2, 0},
	{PXA1U88_CLK_PLL1_4, "pll1_4", "pll1_2", 1, 2, 0},
	{PXA1U88_CLK_PLL1_8, "pll1_8", "pll1_4", 1, 2, 0},
	{PXA1U88_CLK_PLL1_16, "pll1_16", "pll1_8", 1, 2, 0},
	{PXA1U88_CLK_PLL1_6, "pll1_6", "pll1_2", 1, 3, 0},
	{PXA1U88_CLK_PLL1_12, "pll1_12", "pll1_6", 1, 2, 0},
	{PXA1U88_CLK_PLL1_24, "pll1_24", "pll1_12", 1, 2, 0},
	{PXA1U88_CLK_PLL1_48, "pll1_48", "pll1_24", 1, 2, 0},
	{PXA1U88_CLK_PLL1_96, "pll1_96", "pll1_48", 1, 2, 0},
	{PXA1U88_CLK_PLL1_13, "pll1_13", "pll1_624", 1, 13, 0},
	{PXA1U88_CLK_PLL1_13_1_5, "pll1_13_1_5", "pll1_13", 2, 3, 0},
	{PXA1U88_CLK_PLL1_2_1_5, "pll1_2_1_5", "pll1_2", 2, 3, 0},
	{PXA1U88_CLK_PLL1_13_16, "pll1_13_16", "pll1_624", 3, 16, 0},
};

static struct mmp_clk_factor_masks uart_factor_masks = {
	.factor = 2,
	.num_mask = 0x1fff,
	.den_mask = 0x1fff,
	.num_shift = 16,
	.den_shift = 0,
};

static struct mmp_clk_factor_tbl uart_factor_tbl[] = {
	{.num = 8125, .den = 1536},     /*14.745MHZ */
};

static DEFINE_SPINLOCK(pll1_lock);
static struct mmp_param_general_gate_clk pll1_gate_clks[] = {
	{PXA1U88_CLK_PLL1_416_GATE, "pll1_416_gate", "pll1_416", 0, APMU_CLK_GATE_CTRL, 27, 0, &pll1_lock},
	{PXA1U88_CLK_PLL1_624_GATE, "pll1_624_gate", "pll1_624", 0, APMU_CLK_GATE_CTRL, 26, 0, &pll1_lock},
	{PXA1U88_CLK_PLL1_832_GATE, "pll1_832_gate", "pll1_832", 0, APMU_CLK_GATE_CTRL, 30, 0, &pll1_lock},
	{PXA1U88_CLK_PLL1_1248_GATE, "pll1_1248_gate", "pll1_1248", 0, APMU_CLK_GATE_CTRL, 28, 0, &pll1_lock},
	{PXA1U88_CLK_PLL1_312_GATE, "pll1_312_gate", "pll1_2", 0, APMU_CLK_GATE_CTRL, 29, 0, &pll1_lock},
};

enum pll {
	PLL2 = 0,
	PLL3,
	PLL4,
	MAX_PLL_NUM
};

enum pll_type {
	VCO,
	OUT,
	OUTP,
	MAX_PLL_TYPE,
};

static struct mmp_vco_params pllx_vco_params[MAX_PLL_NUM] = {
	{
		.vco_min = 1200000000UL,
		.vco_max = 3000000000UL,
		.lock_enable_bit = POSR_PLL2_LOCK,
	},
	{
		.vco_min = 1200000000UL,
		.vco_max = 3000000000UL,
		.lock_enable_bit = POSR_PLL3_LOCK,
	},
	{
		.vco_min = 1200000000UL,
		.vco_max = 3000000000UL,
		.lock_enable_bit = POSR_PLL4_LOCK,
	}
};

static struct mmp_pll_params pllx_pll_params[MAX_PLL_NUM] = {
};

static struct mmp_pll_params pllx_pllp_params[MAX_PLL_NUM] = {
};

struct plat_pll_info {
	spinlock_t lock;
	const char *vco_name;
	const char *out_name;
	const char *outp_name;
	const char *vco_div3_name;
	/* clk flags */
	unsigned long vco_flag;
	unsigned long vcoclk_flag;
	unsigned long out_flag;
	unsigned long outclk_flag;
	unsigned long outp_flag;
	unsigned long outpclk_flag;
	/* dt index */
	unsigned int vcodtidx;
	unsigned int outdtidx;
	unsigned int outpdtidx;
	unsigned int vcodiv3dtidx;
};

struct plat_pll_info pllx_platinfo[MAX_PLL_NUM] = {
	{
		.vco_name = "pll2_vco",
		.out_name = "pll2",
		.outp_name = "pll2p",
		.vco_div3_name = "pll2_div3",
		.vcoclk_flag = CLK_IS_ROOT,
		.out_flag = HELANX_PLLOUT,
		.outp_flag = HELANX_PLLOUTP,
		.vcodtidx = PXA1U88_CLK_PLL2VCO,
		.outdtidx = PXA1U88_CLK_PLL2,
		.outpdtidx = PXA1U88_CLK_PLL2P,
		.vcodiv3dtidx = PXA1U88_CLK_PLL2VCODIV3,
	},
	{
		.vco_name = "pll3_vco",
		.out_name = "pll3",
		.outp_name = "pll3p",
		.vco_div3_name = "pll3_div3",
		.vcoclk_flag = CLK_IS_ROOT,
		.outpclk_flag = CLK_SET_RATE_PARENT,
		.out_flag = HELANX_PLLOUT,
		.outp_flag = HELANX_PLLOUTP,
		.vcodtidx = PXA1U88_CLK_PLL3VCO,
		.outdtidx = PXA1U88_CLK_PLL3,
		.outpdtidx = PXA1U88_CLK_PLL3P,
		.vcodiv3dtidx = PXA1U88_CLK_PLL3VCODIV3,
	},
	{
		.vco_name = "pll4_vco",
		.out_name = "pll4",
		.outp_name = "pll4p",
		.vco_div3_name = "pll4_div3",
		.vcoclk_flag = CLK_IS_ROOT,
		.vco_flag = HELANX_PLL_SKIP_DEF_RATE,
		/*
		 * set pll4 flag to allow it change rate
		 * when lcd choose pll4 as clk source.
		 */
		.outclk_flag = CLK_SET_RATE_PARENT,
		.out_flag = HELANX_PLLOUT,
		.outp_flag = HELANX_PLLOUTP,
		.vcodtidx = PXA1U88_CLK_PLL4VCO,
		.outdtidx = PXA1U88_CLK_PLL4,
		.outpdtidx = PXA1U88_CLK_PLL4P,
		.vcodiv3dtidx = PXA1U88_CLK_PLL4VCODIV3,
	}
};

/* pll default rate determined by ddr_mode */
unsigned long pll_dfrate[DDR_TYPE_MAX][MAX_PLL_NUM][MAX_PLL_TYPE] = {
	[DDR_533M] = {
		{2115 * MHZ, 1057 * MHZ, 528 * MHZ},
		{1526 * MHZ, 1526 * MHZ, 1526 * MHZ},
		/* for 533M case, reserve pll4 for LCD */
		{1595 * MHZ, 1595 * MHZ, 797 * MHZ},
	},
	[DDR_667M] = {
		{2115 * MHZ, 1057 * MHZ, 528 * MHZ},
		{1526 * MHZ, 1526 * MHZ, 1526 * MHZ},
		{2670lu * MHZ, 1335 * MHZ, 667 * MHZ},
	},
	[DDR_800M] = {
		{2115 * MHZ, 1057 * MHZ, 528 * MHZ},
		{1526 * MHZ, 1526 * MHZ, 1526 * MHZ},
		{1595 * MHZ, 1595 * MHZ, 797 * MHZ},
	},
};

static int board_is_fpga(void)
{
	static int rc;

	if (!rc)
		rc = of_machine_is_compatible("marvell,pxa1908-fpga");

	return rc;
}

static void pxa1U88_dynpll_init(struct pxa1U88_clk_unit *pxa_unit)
{
	int idx;
	struct clk *clk;
	struct mmp_clk_unit *unit = &pxa_unit->unit;

	pllx_vco_params[PLL2].cr_reg = pxa_unit->mpmu_base + MPMU_PLL2CR;
	pllx_vco_params[PLL2].pll_swcr = pxa_unit->apbs_base + APB_SPARE_PLL2CR;
	pllx_vco_params[PLL3].cr_reg = pxa_unit->mpmu_base + MPMU_PLL3CR;
	pllx_vco_params[PLL3].pll_swcr = pxa_unit->apbs_base + APB_SPARE_PLL3CR;
	pllx_vco_params[PLL4].cr_reg = pxa_unit->mpmu_base + MPMU_PLL4CR;
	pllx_vco_params[PLL4].pll_swcr = pxa_unit->apbs_base + APB_SPARE_PLL4CR;

	pllx_pll_params[PLL2].pll_swcr = pxa_unit->apbs_base + APB_SPARE_PLL2CR;
	pllx_pll_params[PLL3].pll_swcr = pxa_unit->apbs_base + APB_SPARE_PLL3CR;
	pllx_pll_params[PLL4].pll_swcr = pxa_unit->apbs_base + APB_SPARE_PLL4CR;

	pllx_pllp_params[PLL2].pll_swcr = pxa_unit->apbs_base + APB_SPARE_PLL2CR;
	pllx_pllp_params[PLL3].pll_swcr = pxa_unit->apbs_base + APB_SPARE_PLL3CR;
	pllx_pllp_params[PLL4].pll_swcr = pxa_unit->apbs_base + APB_SPARE_PLL4CR;

	for (idx = 0; idx < MAX_PLL_NUM; idx++) {
		spin_lock_init(&pllx_platinfo[idx].lock);
		/* vco */
		pllx_vco_params[idx].lock_reg = pxa_unit->mpmu_base + MPMU_POSR;
		pllx_vco_params[idx].default_rate =
			pll_dfrate[ddr_mode][idx][VCO];
		clk = helanx_clk_register_vco(pllx_platinfo[idx].vco_name,
			0, pllx_platinfo[idx].vcoclk_flag, pllx_platinfo[idx].vco_flag,
			&pllx_platinfo[idx].lock, &pllx_vco_params[idx]);
		clk_set_rate(clk, pllx_vco_params[idx].default_rate);
		mmp_clk_add(unit, pllx_platinfo[idx].vcodtidx, clk);
		/* pll */
		pllx_pll_params[idx].default_rate =
			pll_dfrate[ddr_mode][idx][OUT];
		clk = helanx_clk_register_pll(pllx_platinfo[idx].out_name,
			pllx_platinfo[idx].vco_name,
			pllx_platinfo[idx].outclk_flag, pllx_platinfo[idx].out_flag,
			&pllx_platinfo[idx].lock, &pllx_pll_params[idx]);
		clk_set_rate(clk, pllx_pll_params[idx].default_rate);
		mmp_clk_add(unit, pllx_platinfo[idx].outdtidx, clk);
		/* pllp */
		pllx_pllp_params[idx].default_rate =
			pll_dfrate[ddr_mode][idx][OUTP];
		clk = helanx_clk_register_pll(pllx_platinfo[idx].outp_name,
			pllx_platinfo[idx].vco_name,
			pllx_platinfo[idx].outpclk_flag, pllx_platinfo[idx].outp_flag,
			&pllx_platinfo[idx].lock, &pllx_pllp_params[idx]);
		clk_set_rate(clk, pllx_pllp_params[idx].default_rate);
		mmp_clk_add(unit, pllx_platinfo[idx].outpdtidx, clk);
		/* vco div3 */
		clk = clk_register_fixed_factor(NULL,
			pllx_platinfo[idx].vco_div3_name,
			pllx_platinfo[idx].vco_name, 0, 1, 3);
		mmp_clk_add(unit, pllx_platinfo[idx].vcodiv3dtidx, clk);
	}
}

static void pxa1U88_pll_init(struct pxa1U88_clk_unit *pxa_unit)
{
	struct clk *clk;
	struct mmp_clk_unit *unit = &pxa_unit->unit;

	mmp_register_fixed_rate_clks(unit, fixed_rate_clks,
					ARRAY_SIZE(fixed_rate_clks));

	mmp_register_fixed_factor_clks(unit, fixed_factor_clks,
					ARRAY_SIZE(fixed_factor_clks));

	clk = clk_register_gate(NULL, "pll1_499_en", "pll1_499", 0,
				pxa_unit->apbs_base + APBS_PLL1_CTRL,
				31, 0, NULL);
	mmp_clk_add(unit, PXA1U88_CLK_PLL1_499_EN, clk);

	clk = mmp_clk_register_factor("uart_pll", "pll1_4",
				CLK_SET_RATE_PARENT,
				pxa_unit->mpmu_base + MPMU_UART_PLL,
				&uart_factor_masks, uart_factor_tbl,
				ARRAY_SIZE(uart_factor_tbl), NULL);
	mmp_clk_add(unit, PXA1U88_CLK_UART_PLL, clk);

	mmp_register_general_gate_clks(unit, pll1_gate_clks,
				pxa_unit->apmu_base,
				ARRAY_SIZE(pll1_gate_clks));

	if (!board_is_fpga())
		pxa1U88_dynpll_init(pxa_unit);
}

static DEFINE_SPINLOCK(pwm0_lock);
static DEFINE_SPINLOCK(pwm2_lock);

static struct mmp_param_gate_clk apbc_gate_clks[] = {
	{PXA1U88_CLK_TWSI0, "twsi0_clk", "pll1_13_1_5", CLK_SET_RATE_PARENT, APBC_TWSI0, 0x7, 0x3, 0x0, 0, NULL},
	{PXA1U88_CLK_TWSI1, "twsi1_clk", "pll1_13_1_5", CLK_SET_RATE_PARENT, APBC_TWSI1, 0x7, 0x3, 0x0, 0, NULL},
	{PXA1U88_CLK_TWSI3, "twsi3_clk", "pll1_13_1_5", CLK_SET_RATE_PARENT, APBC_TWSI3, 0x7, 0x3, 0x0, 0, NULL},
	{PXA1U88_CLK_GPIO, "gpio_clk", "vctcxo", CLK_SET_RATE_PARENT, APBC_GPIO, 0x7, 0x3, 0x0, 0, NULL},
	{PXA1U88_CLK_KPC, "kpc_clk", "clk32", CLK_SET_RATE_PARENT, APBC_KPC, 0x7, 0x3, 0x0, MMP_CLK_GATE_NEED_DELAY, NULL},
	{PXA1U88_CLK_RTC, "rtc_clk", "clk32", CLK_SET_RATE_PARENT, APBC_RTC, 0x87, 0x83, 0x0, MMP_CLK_GATE_NEED_DELAY, NULL},
	{PXA1U88_CLK_PWM0, "pwm0_clk", "pwm01_apb_share", CLK_SET_RATE_PARENT, APBC_PWM0, 0x2, 0x2, 0x0, 0, &pwm0_lock},
	{PXA1U88_CLK_PWM1, "pwm1_clk", "pwm01_apb_share", CLK_SET_RATE_PARENT, APBC_PWM1, 0x6, 0x2, 0x0, 0, NULL},
	{PXA1U88_CLK_PWM2, "pwm2_clk", "pwm23_apb_share", CLK_SET_RATE_PARENT, APBC_PWM2, 0x2, 0x2, 0x0, 0, &pwm2_lock},
	{PXA1U88_CLK_PWM3, "pwm3_clk", "pwm23_apb_share", CLK_SET_RATE_PARENT, APBC_PWM3, 0x6, 0x2, 0x0, 0, NULL},
};

static DEFINE_SPINLOCK(uart0_lock);
static DEFINE_SPINLOCK(uart1_lock);
static DEFINE_SPINLOCK(uart2_lock);
static const char *uart_parent_names[] = {"pll1_3_16", "uart_pll"};
static const char *ssp_parent_names[] = {"pll1_96", "pll1_48", "pll1_24", "pll1_12"};
#ifdef CONFIG_CORESIGHT_SUPPORT
static void pxa1U88_coresight_clk_init(struct pxa1U88_clk_unit *pxa_unit)
{
	struct mmp_clk_unit *unit = &pxa_unit->unit;
	struct clk *clk;

	clk = mmp_clk_register_gate(NULL, "DBGCLK", "pll1_416", 0,
			pxa_unit->apmu_base + APMU_TRACE,
			0x10008, 0x10008, 0x0, 0, NULL);
	mmp_clk_add(unit, PXA1U88_CLK_DBGCLK, clk);

	/* TMC clock */
	clk = mmp_clk_register_gate(NULL, "TRACECLK", "DBGCLK", 0,
			pxa_unit->apmu_base + APMU_TRACE,
			0x10010, 0x10010, 0x0, 0, NULL);
	mmp_clk_add(unit, PXA1U88_CLK_TRACECLK, clk);
}
#endif

static void pxa1U88_apb_periph_clk_init(struct pxa1U88_clk_unit *pxa_unit)
{
	struct clk *clk;
	struct mmp_clk_unit *unit = &pxa_unit->unit;

	clk = mmp_clk_register_gate(NULL, "pwm01_apb_share", "pll1_48",
				CLK_SET_RATE_PARENT,
				pxa_unit->apbc_base + APBC_PWM0,
				0x5, 0x1, 0x0, 0, &pwm0_lock);

	clk = mmp_clk_register_gate(NULL, "pwm23_apb_share", "pll1_48",
				CLK_SET_RATE_PARENT,
				pxa_unit->apbc_base + APBC_PWM2,
				0x5, 0x1, 0x0, 0, &pwm2_lock);

	mmp_register_gate_clks(unit, apbc_gate_clks, pxa_unit->apbc_base,
				ARRAY_SIZE(apbc_gate_clks));

	clk = mmp_clk_register_gate(NULL, "ts_clk", NULL, 0,
				pxa_unit->apbc_base + APBC_TERMAL, 0x7, 0x3, 0x0, 0, NULL);
	mmp_clk_add(unit, PXA1U88_CLK_THERMAL, clk);

	clk = mmp_clk_register_gate(NULL, "twsi2_clk", "pll1_13_1_5",
				CLK_SET_RATE_PARENT,
				pxa_unit->apbcp_base + APBCP_TWSI2,
				0x7, 0x3, 0x0, 0, NULL);
	mmp_clk_add(unit, PXA1U88_CLK_TWSI2, clk);

	clk_register_mux(NULL, "uart0_mux", uart_parent_names,
				ARRAY_SIZE(uart_parent_names),
				CLK_SET_RATE_PARENT,
				pxa_unit->apbc_base + APBC_UART0,
				4, 3, 0, &uart0_lock);

	if (board_is_fpga())
		clk = clk_register_fixed_rate(NULL,
				"uart0_clk", "uart0_mux", 0, 12500000);
	else
		clk = mmp_clk_register_gate(NULL, "uart0_clk", "uart0_mux",
					CLK_SET_RATE_PARENT,
					pxa_unit->apbc_base + APBC_UART0,
					0x7, 0x3, 0x0, 0, &uart0_lock);
	mmp_clk_add(unit, PXA1U88_CLK_UART0, clk);

	clk_register_mux(NULL, "uart1_mux", uart_parent_names,
				ARRAY_SIZE(uart_parent_names),
				CLK_SET_RATE_PARENT,
				pxa_unit->apbc_base + APBC_UART1,
				4, 3, 0, &uart1_lock);
	clk = mmp_clk_register_gate(NULL, "uart1_clk", "uart1_mux",
				CLK_SET_RATE_PARENT,
				pxa_unit->apbc_base + APBC_UART1,
				0x7, 0x3, 0x0, 0, &uart1_lock);
	mmp_clk_add(unit, PXA1U88_CLK_UART1, clk);

	clk_register_mux(NULL, "uart2_mux", uart_parent_names,
				ARRAY_SIZE(uart_parent_names),
				CLK_SET_RATE_PARENT,
				pxa_unit->apbcp_base + APBCP_UART2,
				4, 3, 0, &uart2_lock);
	clk = mmp_clk_register_gate(NULL, "uart2_clk", "uart2_mux",
				CLK_SET_RATE_PARENT,
				pxa_unit->apbcp_base + APBCP_UART2,
				0x7, 0x3, 0x0, 0, &uart2_lock);
	mmp_clk_add(unit, PXA1U88_CLK_UART2, clk);

	clk = mmp_clk_register_apbc("swjtag", NULL,
				pxa_unit->apbc_base + APBC_SWJTAG,
				10, 0, NULL);
	mmp_clk_add(unit, PXA1U88_CLK_SWJTAG, clk);

	clk = mmp_clk_register_gate(NULL, "ipc_clk", NULL, 0,
			pxa_unit->apbc_base + APBC_IPC_CLK_RST,
			0x7, 0x3, 0x0, 0, NULL);
	mmp_clk_add(unit, PXA1U88_CLK_IPC_RST, clk);
	clk_prepare_enable(clk);

	clk = mmp_clk_register_gate(NULL, "ripc_clk", NULL, 0,
			pxa_unit->apbcp_base + APBCP_AICER,
			0x7, 0x2, 0x0, 0, NULL);
	mmp_clk_add(unit, PXA1U88_CLK_AICER, clk);
	clk_prepare_enable(clk);

	clk_register_mux(NULL, "ssp0_mux", ssp_parent_names,
		ARRAY_SIZE(ssp_parent_names), 0,
		pxa_unit->apbc_base + APBC_SSP0, 4, 3, 0, NULL);
	clk = mmp_clk_register_gate(NULL, "ssp0_clk", "ssp0_mux",
				0,
				pxa_unit->apbc_base + APBC_SSP0,
				0x7, 0x3, 0x0, 0, NULL);
	mmp_clk_add(unit, PXA1U88_CLK_SSP0, clk);

	clk_register_mux(NULL, "ssp2_mux", ssp_parent_names,
		ARRAY_SIZE(ssp_parent_names), 0,
		pxa_unit->apbc_base + APBC_SSP2, 4, 3, 0, NULL);
	clk = mmp_clk_register_gate(NULL, "ssp2_clk", "ssp2_mux",
				0,
				pxa_unit->apbc_base + APBC_SSP2,
				0x7, 0x3, 0x0, 0, NULL);
	mmp_clk_add(unit, PXA1U88_CLK_SSP2, clk);

#ifdef CONFIG_CORESIGHT_SUPPORT
	pxa1U88_coresight_clk_init(pxa_unit);
#endif
}

static DEFINE_SPINLOCK(sdh0_lock);
static DEFINE_SPINLOCK(sdh1_lock);
static DEFINE_SPINLOCK(sdh2_lock);
static const char *sdh_parent_names[] = {"pll1_416", "pll1_624"};
static struct mmp_clk_mix_config sdh_mix_config = {
	.reg_info = DEFINE_MIX_REG_INFO(3, 8, 2, 6, 11),
};

/* Protect GC 3D register access APMU_GC&APMU_GC2D */
static DEFINE_SPINLOCK(gc_lock);
static DEFINE_SPINLOCK(gc2d_lock);

/* GC 3D */
static const char *gc3d_parent_names[] = {
	"pll1_832_gate", "pll1_624_gate", "pll2p", "pll2_div3"
};

static struct mmp_clk_mix_clk_table gc3d_pptbl[] = {
	{.rate = 156000000, .parent_index = 1,/* pll1_624_gate */ .xtc = 0x00055544},
	{.rate = 312000000, .parent_index = 1,/* pll1_624_gate */ .xtc = 0x00055544},
	{.rate = 416000000, .parent_index = 0,/* pll1_832_gate */ .xtc = 0x00055544},
	{.rate = 624000000, .parent_index = 1,/* pll1_624_gate */ .xtc = 0x000AAA55},
	{.rate = 705000000, .parent_index = 3, /* pll2_div3 */ .xtc = 0x000AAA55},
};

static struct mmp_clk_mix_config gc3d_mix_config = {
	.reg_info = DEFINE_MIX_REG_INFO(3, 12, 2, 6, 15),
	.table = gc3d_pptbl,
	.table_size = ARRAY_SIZE(gc3d_pptbl),
};

/* GC shader */
static const char * const gcsh_parent_names[] = {
	"pll1_832_gate", "pll1_624_gate",  "pll2p", "pll3p",
};

static struct mmp_clk_mix_clk_table gcsh_pptbl[] = {
	{.rate = 156000000, .parent_index = 1,/* pll1_624_gate */},
	{.rate = 312000000, .parent_index = 1,/* pll1_624_gate */},
	{.rate = 416000000, .parent_index = 0,/* pll1_832_gate */},
	{.rate = 624000000, .parent_index = 1,/* pll1_624_gate */},
};

static struct mmp_clk_mix_config gcsh_mix_config = {
	.reg_info = DEFINE_MIX_REG_INFO(3, 28, 2, 26, 31),
	.table = gcsh_pptbl,
	.table_size = ARRAY_SIZE(gcsh_pptbl),
};

/* GC 2D */
static const char *gc2d_parent_names[] = {
	"pll1_416_gate", "pll1_624_gate",  "pll2", "pll2p",
};

/* ulc GC2D has no compress feature, adjust the PP */
static struct mmp_clk_mix_clk_table gc2d_pptbl[] = {
	{.rate = 156000000, .parent_index = 1,/* pll1_624_gate */ .xtc = 0x00000044},
	{.rate = 312000000, .parent_index = 1,/* pll1_624_gate */ .xtc = 0x00055544},
	{.rate = 416000000, .parent_index = 0,/* pll1_416_gate */ .xtc = 0x000AAA44},
};

static struct mmp_clk_mix_config gc2d_mix_config = {
	.reg_info = DEFINE_MIX_REG_INFO(3, 12, 2, 6, 15),
	.table = gc2d_pptbl,
	.table_size = ARRAY_SIZE(gc2d_pptbl),
};

/* GC bus(shared by GC 3D and 2D) */
static const char *gcbus_parent_names[] = {
	"pll1_416_gate", "pll1_624_gate", "pll2", "pll4",
};

/* ulc GCbus adjusted from 128bit to 64bit, adjust the PP */
static struct mmp_clk_mix_clk_table gcbus_pptbl[] = {
	{.rate = 156000000, .parent_index = 1,/* pll1_624_gate */},
	{.rate = 312000000, .parent_index = 1,/* pll1_624_gate */},
	{.rate = 416000000, .parent_index = 0, /* pll1_416_gate */},
	{.rate = 624000000, .parent_index = 1, /* pll1_624_gate */},
};

static struct mmp_clk_mix_config gcbus_mix_config = {
	.reg_info = DEFINE_MIX_REG_INFO(3, 17, 2, 20, 16),
	.table = gcbus_pptbl,
	.table_size = ARRAY_SIZE(gcbus_pptbl),
};

/* Protect register access APMU_VPU */
static DEFINE_SPINLOCK(vpu_lock);

/* VPU fclk */
static const char *vpufclk_parent_names[] = {
	"pll1_416_gate", "pll1_624_gate", "pll2_div3", "pll2p",
};

static struct mmp_clk_mix_clk_table vpufclk_pptbl[] = {
	{.rate = 156000000, .parent_index = 1,/* pll1_624_gate */ .xtc = 0x00380404},
	{.rate = 208000000, .parent_index = 0,/* pll1_416_gate */ .xtc = 0x00385454},
	{.rate = 312000000, .parent_index = 1,/* pll1_624_gate */ .xtc = 0x00385454},
	{.rate = 416000000, .parent_index = 0, /* pll1_416_gate */ .xtc = 0x00B85454},
	{.rate = 528000000, .parent_index = 3, /* pll2p */ .xtc = 0x00B8A5A4},
};

static struct mmp_clk_mix_config vpufclk_mix_config = {
	.reg_info = DEFINE_MIX_REG_INFO(3, 8, 2, 6, 20),
	.table = vpufclk_pptbl,
	.table_size = ARRAY_SIZE(vpufclk_pptbl),
};

/* vpu bus */
static const char *vpubus_parent_names[] = {
	"pll1_416_gate", "pll1_624_gate", "pll2", "pll2_div3",
};

static struct mmp_clk_mix_clk_table vpubus_pptbl[] = {
	{.rate = 156000000, .parent_index = 1,/* pll1_624_gate */},
	{.rate = 208000000, .parent_index = 0,/* pll1_416_gate */},
	{.rate = 312000000, .parent_index = 1,/* pll1_624_gate */},
	{.rate = 416000000, .parent_index = 0, /* pll1_416_gate */},
	{.rate = 528500000, .parent_index = 2, /* pll2 */},
};

static struct mmp_clk_mix_config vpubus_mix_config = {
	.reg_info = DEFINE_MIX_REG_INFO(3, 13, 2, 11, 21),
	.table = vpubus_pptbl,
	.table_size = ARRAY_SIZE(vpubus_pptbl),
};

static DEFINE_SPINLOCK(disp_lock);
static const char *disp1_parent_names[] = {"pll1_624", "pll1_832", "pll1_499_en"};
static const char *disp2_parent_names[] = {"pll2", "pll2p", "pll2_div3"};
static const char *disp3_parent_names[] = {"pll3p", "pll3_div3"};
static const char *disp4_parent_names[] = {"pll4", "pll4_div3"};

static const char *disp_axi_parent_names[] = {"pll1_416", "pll1_624", "pll2", "pll2p"};
static int disp_axi_mux_table[] = {0x0, 0x1, 0x2, 0x3};
static struct mmp_clk_mix_config disp_axi_mix_config = {
	.reg_info = DEFINE_MIX_REG_INFO(2, 19, 2, 17, 22),
	.mux_table = disp_axi_mux_table,
};

/* sc2 clk */
static DEFINE_SPINLOCK(ccic0_lock);
static DEFINE_SPINLOCK(ccic1_lock);
static DEFINE_SPINLOCK(isp_lock);
static const char *sc2_4x_parent_names[] = {"pll1_832_gate", "pll1_624_gate",
		"pll2_div3", "pll2p", "pll4_div3"};
static int sc2_4x_mux_table[] = {0x0, 0x1, 0x02, 0x82, 0x03};
static struct mmp_clk_mix_config sc2_4x_mix_config = {
	.reg_info = DEFINE_MIX_REG_INFO(3, 18, 2, 16, 15),
	.mux_table = sc2_4x_mux_table,
};

static const char *sc2_csi_parent_names[] = {"pll1_416_gate", "pll1_624_gate",
		"pll2_div3", "pll2p", "pll4_div3"};
static int sc2_csi_mux_table[] = {0x0, 0x1, 0x02, 0x22, 0x03};
static struct mmp_clk_mix_config sc2_csi_mix_config = {
	.reg_info = DEFINE_MIX_REG_INFO(3, 18, 2, 16, 15),
	.mux_table = sc2_csi_mux_table,
};

static const char *sc2_axi_parent_names[] = {"pll1_416_gate", "pll1_624_gate",
		"pll2", "pll2p"};
static struct mmp_clk_mix_config sc2_axi_mix_config = {
	.reg_info = DEFINE_MIX_REG_INFO(3, 18, 2, 21, 23),
};

static const char *sc2_phy_parent_names[] = {"pll1_6", "pll1_12"};

static const char *isp_pipe_parent_names[] = {"pll1_416_gate", "pll1_624_gate",
			"pll4_div3", "pll2p",};
static struct mmp_clk_mix_config isp_pipe_mix_config = {
	.reg_info = DEFINE_MIX_REG_INFO(3, 4, 2, 2, 7),
};
#ifdef CONFIG_SMC91X
static void __init smc91x_clk_init(void __iomem *apmu_base)
{
	struct device_node *np = of_find_node_by_name(NULL, "smc91x");
	const char *str = NULL;
	void __iomem *reg;

	if (np && !of_property_read_string(np, "clksrc", &str))
		if (!strcmp(str, "smc91x")) {
			/* Enable clock to SMC Controller */
			writel(0x5b, apmu_base + APMU_SMC);
			/* Configure SMC Controller */
			/* Set CS0 to A\D type memory */
			reg = ioremap(SMC_CLK, 4);
			writel(0x52880008, reg);
			iounmap(reg);
	}
}
#endif

struct pxa1U88_clk_disp {
	struct mmp_clk_gate gate;
	struct clk_mux mux;
	struct clk_divider divider;
	const struct clk_ops *mux_ops;
	const struct clk_ops *div_ops;
	const struct clk_ops *gate_ops;
};

static struct pxa1U88_clk_disp disp1_clks;
static struct pxa1U88_clk_disp disp4_clks;

static void pxa1U88_axi_periph_clk_init(struct pxa1U88_clk_unit *pxa_unit)
{
	struct clk *clk;
	struct mmp_clk_unit *unit = &pxa_unit->unit;
	const char **parent_names;
	u32 parent_num;

	clk = mmp_clk_register_gate(NULL, "usb_clk", NULL, 0,
				pxa_unit->apmu_base + APMU_USB,
				0x9, 0x9, 0x1, 0, NULL);
	mmp_clk_add(unit, PXA1U88_CLK_USB, clk);

	/* nand flash clock, no one use it, expect to be disabled */
	mmp_clk_register_gate(NULL, "nf_clk", NULL, 0,
				pxa_unit->apmu_base + APMU_NF,
				0x1db, 0x1db, 0x0, 0, NULL);

	clk = mmp_clk_register_gate(NULL, "sdh_axi_clk", NULL, 0,
				pxa_unit->apmu_base + APMU_SDH0,
				0x8, 0x8, 0x0, 0, &sdh0_lock);
	mmp_clk_add(unit, PXA1U88_CLK_SDH_AXI, clk);

	sdh_mix_config.reg_info.reg_clk_ctrl = pxa_unit->apmu_base + APMU_SDH0;
	mmp_clk_register_mix(NULL, "sdh0_mix_clk", sdh_parent_names,
				ARRAY_SIZE(sdh_parent_names),
				CLK_SET_RATE_PARENT,
				&sdh_mix_config, &sdh0_lock);
	clk = mmp_clk_register_gate(NULL, "sdh0_clk", "sdh0_mix_clk",
				CLK_SET_RATE_PARENT | CLK_SET_RATE_ENABLED,
				pxa_unit->apmu_base + APMU_SDH0,
				0x12, 0x12, 0x0, 0, &sdh0_lock);
	mmp_clk_add(unit, PXA1U88_CLK_SDH0, clk);

	clk = mmp_clk_register_dvfs_dummy("sdh0_dummy", NULL,
			0, DUMMY_VL_TO_KHZ(0));
	mmp_clk_add(unit, PXA1U88_CLK_SDH0_DUMMY, clk);

	sdh_mix_config.reg_info.reg_clk_ctrl = pxa_unit->apmu_base + APMU_SDH1;
	mmp_clk_register_mix(NULL, "sdh1_mix_clk", sdh_parent_names,
				ARRAY_SIZE(sdh_parent_names),
				CLK_SET_RATE_PARENT,
				&sdh_mix_config, &sdh1_lock);
	clk = mmp_clk_register_gate(NULL, "sdh1_clk", "sdh1_mix_clk",
				CLK_SET_RATE_PARENT | CLK_SET_RATE_ENABLED,
				pxa_unit->apmu_base + APMU_SDH1,
				0x12, 0x12, 0x0, 0, &sdh1_lock);
	mmp_clk_add(unit, PXA1U88_CLK_SDH1, clk);

	clk = mmp_clk_register_dvfs_dummy("sdh1_dummy", NULL,
			0, DUMMY_VL_TO_KHZ(0));
	mmp_clk_add(unit, PXA1U88_CLK_SDH1_DUMMY, clk);

	sdh_mix_config.reg_info.reg_clk_ctrl = pxa_unit->apmu_base + APMU_SDH2;
	mmp_clk_register_mix(NULL, "sdh2_mix_clk", sdh_parent_names,
				ARRAY_SIZE(sdh_parent_names),
				CLK_SET_RATE_PARENT,
				&sdh_mix_config, &sdh2_lock);
	clk = mmp_clk_register_gate(NULL, "sdh2_clk", "sdh2_mix_clk",
				CLK_SET_RATE_PARENT | CLK_SET_RATE_ENABLED,
				pxa_unit->apmu_base + APMU_SDH2,
				0x12, 0x12, 0x0, 0, &sdh2_lock);
	mmp_clk_add(unit, PXA1U88_CLK_SDH2, clk);

	clk = mmp_clk_register_dvfs_dummy("sdh2_dummy", NULL,
			0, DUMMY_VL_TO_KHZ(0));
	mmp_clk_add(unit, PXA1U88_CLK_SDH2_DUMMY, clk);

	/*
	 * DE suggest SW to release GC_2D_3D_AXI_Reset
	 * before both 3D/2D power on sequence
	 */
	clk = mmp_clk_register_gate(NULL, "gc_axi_rst", NULL,
				0, pxa_unit->apmu_base + APMU_GC2D,
				0x1, 0x1, 0x0, 0, &gc2d_lock);
	clk_prepare_enable(clk);

	gc3d_mix_config.reg_info.reg_clk_ctrl = pxa_unit->apmu_base + APMU_GC;
	gc3d_mix_config.reg_info.reg_clk_xtc = pxa_unit->ciu_base + GPU_XTC;
	clk = mmp_clk_register_mix(NULL, "gc3d_mix_clk", gc3d_parent_names,
				ARRAY_SIZE(gc3d_parent_names),
				0, &gc3d_mix_config, &gc_lock);
#ifdef CONFIG_PM_DEVFREQ
	__init_comp_devfreq_table(clk, DEVFREQ_GPU_3D);
#endif
	clk = mmp_clk_register_gate(NULL, "gc3d_clk", "gc3d_mix_clk",
				CLK_SET_RATE_PARENT | CLK_SET_RATE_ENABLED,
				pxa_unit->apmu_base + APMU_GC,
				(3 << 4), (3 << 4), 0x0, 0, &gc_lock);
	clk_set_rate(clk, 312000000);
	mmp_clk_add(unit, PXA1U88_CLK_GC3D, clk);
	register_mixclk_dcstatinfo(clk);

	gcsh_mix_config.reg_info.reg_clk_ctrl = pxa_unit->apmu_base + APMU_GC;
	parent_names = (const char **)gcsh_parent_names;
	parent_num = ARRAY_SIZE(gcsh_parent_names);
	clk = mmp_clk_register_mix(NULL, "gcsh_mix_clk", parent_names,
				parent_num,
				0, &gcsh_mix_config, &gc_lock);
#ifdef CONFIG_PM_DEVFREQ
	__init_comp_devfreq_table(clk, DEVFREQ_GPU_SH);
#endif
	clk = mmp_clk_register_gate(NULL, "gcsh_clk", "gcsh_mix_clk",
				CLK_SET_RATE_PARENT | CLK_SET_RATE_ENABLED,
				pxa_unit->apmu_base + APMU_GC,
				(1 << 25), (1 << 25), 0x0, 0, &gc_lock);
	clk_set_rate(clk, 312000000);
	mmp_clk_add(unit, PXA1U88_CLK_GCSH, clk);
	register_mixclk_dcstatinfo(clk);

	gc2d_mix_config.reg_info.reg_clk_ctrl = pxa_unit->apmu_base + APMU_GC2D;
	gc2d_mix_config.reg_info.reg_clk_xtc = pxa_unit->ciu_base + GPU2D_XTC;
	clk = mmp_clk_register_mix(NULL, "gc2d_mix_clk", gc2d_parent_names,
				ARRAY_SIZE(gc2d_parent_names),
				0, &gc2d_mix_config, &gc2d_lock);
#ifdef CONFIG_PM_DEVFREQ
	__init_comp_devfreq_table(clk, DEVFREQ_GPU_2D);
#endif
	clk = mmp_clk_register_gate(NULL, "gc2d_clk", "gc2d_mix_clk",
				CLK_SET_RATE_PARENT | CLK_SET_RATE_ENABLED,
				pxa_unit->apmu_base + APMU_GC2D,
				(3 << 4), (3 << 4), 0x0, 0, &gc2d_lock);
	clk_set_rate(clk, 156000000);
	mmp_clk_add(unit, PXA1U88_CLK_GC2D, clk);
	register_mixclk_dcstatinfo(clk);

	gcbus_mix_config.reg_info.reg_clk_ctrl =
				pxa_unit->apmu_base + APMU_GC2D;
	mmp_clk_register_mix(NULL, "gcbus_mix_clk", gcbus_parent_names,
				ARRAY_SIZE(gcbus_parent_names),
				0, &gcbus_mix_config, &gc2d_lock);
	clk = mmp_clk_register_gate(NULL, "gcbus_clk", "gcbus_mix_clk",
				CLK_SET_RATE_PARENT | CLK_SET_RATE_ENABLED,
				pxa_unit->apmu_base + APMU_GC2D,
				(1 << 3), (1 << 3), 0x0, 0, &gc2d_lock);
	clk_set_rate(clk, 312000000);
	mmp_clk_add(unit, PXA1U88_CLK_GCBUS, clk);

	vpufclk_mix_config.reg_info.reg_clk_ctrl =
			pxa_unit->apmu_base + APMU_VPU;
	vpufclk_mix_config.reg_info.reg_clk_xtc =
			pxa_unit->ciu_base + VPU_XTC;
	clk = mmp_clk_register_mix(NULL, "vpufunc_mix_clk",
			vpufclk_parent_names, ARRAY_SIZE(vpufclk_parent_names),
			0, &vpufclk_mix_config, &vpu_lock);
#ifdef CONFIG_VPU_DEVFREQ
	__init_comp_devfreq_table(clk, DEVFREQ_VPU_BASE);
#endif
	clk = mmp_clk_register_gate(NULL, "vpufunc_clk", "vpufunc_mix_clk",
			CLK_SET_RATE_PARENT | CLK_SET_RATE_ENABLED,
			pxa_unit->apmu_base + APMU_VPU,
			(3 << 4), (3 << 4), 0x0, 0, &vpu_lock);
	clk_set_rate(clk, 416000000);
	mmp_clk_add(unit, PXA1U88_CLK_VPU, clk);
	register_mixclk_dcstatinfo(clk);

	vpubus_mix_config.reg_info.reg_clk_ctrl =
			pxa_unit->apmu_base + APMU_VPU;
	mmp_clk_register_mix(NULL, "vpubus_mix_clk",
			vpubus_parent_names, ARRAY_SIZE(vpubus_parent_names),
			0, &vpubus_mix_config, &vpu_lock);
	clk = mmp_clk_register_gate(NULL, "vpubus_clk", "vpubus_mix_clk",
			CLK_SET_RATE_PARENT | CLK_SET_RATE_ENABLED,
			pxa_unit->apmu_base + APMU_VPU,
			(1 << 3), (1 << 3), 0x0, 0, &vpu_lock);
	clk_set_rate(clk, 416000000);
	mmp_clk_add(unit, PXA1U88_CLK_VPUBUS, clk);

	clk = mmp_clk_register_gate(NULL, "dsi_esc_clk", NULL, 0,
			pxa_unit->apmu_base + APMU_DSI1,
			0xf, 0xc, 0x0, 0, &disp_lock);
	mmp_clk_add(unit, PXA1U88_CLK_DSI_ESC, clk);

	disp1_clks.mux_ops = &clk_mux_ops;
	disp1_clks.mux.mask = 0x2;
	disp1_clks.mux.shift = 9;
	disp1_clks.mux.reg = pxa_unit->apmu_base + APMU_DISP1;
	disp1_clks.gate.mask = 0x20;
	disp1_clks.gate.reg = pxa_unit->apmu_base + APMU_DISP1;
	disp1_clks.gate.val_disable = 0x0;
	disp1_clks.gate.val_enable = 0x20;
	disp1_clks.gate.flags = 0x0;
	disp1_clks.gate_ops = &mmp_clk_gate_ops;

	clk = clk_register_composite(NULL, "disp1_sel_clk", (const char **)disp1_parent_names,
				ARRAY_SIZE(disp1_parent_names),
				&disp1_clks.mux.hw, disp1_clks.mux_ops,
				NULL, NULL,
				&disp1_clks.gate.hw, disp1_clks.gate_ops,
				CLK_SET_RATE_PARENT);
	mmp_clk_add(unit, PXA1U88_CLK_DISP1, clk);
	clk_register_clkdev(clk, "disp1_sel_clk", NULL);

	clk = clk_register_mux(NULL, "disp2_sel_clk", disp2_parent_names,
			ARRAY_SIZE(disp2_parent_names),
			CLK_SET_RATE_PARENT,
			pxa_unit->apmu_base + APMU_DISP1,
			11, 2, 0, &disp_lock);
	mmp_clk_add(unit, PXA1U88_CLK_DISP2, clk);

	clk = mmp_clk_register_gate(NULL, "dsip2_clk", "disp2_sel_clk",
			CLK_SET_RATE_PARENT,
			pxa_unit->apmu_base + APMU_DISP1,
			0x40, 0x40, 0x0, 0, &disp_lock);
	mmp_clk_add(unit, PXA1U88_CLK_DISP2_EN, clk);

	clk = clk_register_mux(NULL, "disp3_sel_clk", disp3_parent_names,
			ARRAY_SIZE(disp3_parent_names),
			CLK_SET_RATE_PARENT,
			pxa_unit->apmu_base + APMU_DISP1,
			13, 1, 0, &disp_lock);
	mmp_clk_add(unit, PXA1U88_CLK_DISP3, clk);

	clk = mmp_clk_register_gate(NULL, "dsip3_en_clk", "disp3_sel_clk",
			CLK_SET_RATE_PARENT,
			pxa_unit->apmu_base + APMU_DISP1,
			0x80, 0x80, 0x0, 0, &disp_lock);
	mmp_clk_add(unit, PXA1U88_CLK_DISP3_EN, clk);

	disp4_clks.mux_ops = &clk_mux_ops;
	disp4_clks.mux.mask = 0x1;
	disp4_clks.mux.shift = 14;
	disp4_clks.mux.reg = pxa_unit->apmu_base + APMU_DISP1;
	disp4_clks.gate.mask = 0x100;
	disp4_clks.gate.reg = pxa_unit->apmu_base + APMU_DISP1;
	disp4_clks.gate.val_disable = 0x0;
	disp4_clks.gate.val_enable = 0x100;
	disp4_clks.gate.flags = 0x0;
	disp4_clks.gate_ops = &mmp_clk_gate_ops;

	clk = clk_register_composite(NULL, "dsi_pll", (const char **)disp4_parent_names,
				ARRAY_SIZE(disp4_parent_names),
				&disp4_clks.mux.hw, disp4_clks.mux_ops,
				NULL, NULL,
				&disp4_clks.gate.hw, disp4_clks.gate_ops,
				0);
	mmp_clk_add(unit, PXA1U88_CLK_DISP4, clk);
	clk_register_clkdev(clk, "dsi_pll", NULL);

	disp_axi_mix_config.reg_info.reg_clk_ctrl = pxa_unit->apmu_base + APMU_DISP1;
	clk = mmp_clk_register_mix(NULL, "disp_axi_sel_clk", disp_axi_parent_names,
				ARRAY_SIZE(disp_axi_parent_names),
				CLK_SET_RATE_PARENT,
				&disp_axi_mix_config, &disp_lock);
	mmp_clk_add(unit, PXA1U88_CLK_DISP_AXI_SEL_CLK, clk);
	clk_set_rate(clk, 208000000);

	clk = mmp_clk_register_gate(NULL, "disp_axi_clk", "disp_axi_sel_clk",
			CLK_SET_RATE_PARENT,
			pxa_unit->apmu_base + APMU_DISP1,
			0x10009, 0x10009, 0x10001, 0, &disp_lock);
	mmp_clk_add(unit, PXA1U88_CLK_DISP_AXI_CLK, clk);

	clk = mmp_clk_register_gate(NULL, "LCDCIHCLK", "disp_axi_clk", 0,
			pxa_unit->apmu_base + APMU_DISP1,
			0x16, 0x16, 0x4, 0, &disp_lock);
	mmp_clk_add(unit, PXA1U88_CLK_DISP_HCLK, clk);

	/* SC2 VCLK */
	clk_register_divider(NULL, "isim_vclk_div", "pll1_312_gate",
			0, pxa_unit->apmu_base + APMU_CCIC1,
			22, 4, 0, &ccic1_lock);

	clk = mmp_clk_register_gate(NULL, "isim_vclk_gate", "isim_vclk_div", 0,
			pxa_unit->apmu_base + APMU_CCIC1,
			0x4000000, 0x4000000, 0x0, 0, &ccic1_lock);
	mmp_clk_add(unit, PXA1U88_CLK_SC2_MCLK, clk);

	sc2_4x_mix_config.reg_info.reg_clk_ctrl = pxa_unit->apmu_base + APMU_CCIC0;
	mmp_clk_register_mix(NULL, "sc2_4x_mix_clk", sc2_4x_parent_names,
			ARRAY_SIZE(sc2_4x_parent_names), 0,
			&sc2_4x_mix_config, &ccic0_lock);

	clk = mmp_clk_register_gate(NULL, "sc2_4x_clk", "sc2_4x_mix_clk",
			 CLK_SET_RATE_PARENT | CLK_SET_RATE_ENABLED,
			 pxa_unit->apmu_base + APMU_CCIC0,
			 0x12, 0x12, 0x0, 0, &ccic0_lock);
	mmp_clk_add(unit, PXA1U88_CLK_SC2_4X_CLK, clk);

	sc2_csi_mix_config.reg_info.reg_clk_ctrl = pxa_unit->apmu_base + APMU_CCIC1;
	mmp_clk_register_mix(NULL, "sc2_csi_mix_clk",
			sc2_csi_parent_names,
			ARRAY_SIZE(sc2_csi_parent_names), 0,
			&sc2_csi_mix_config, &ccic1_lock);

	clk = mmp_clk_register_gate(NULL, "sc2_csi_clk", "sc2_csi_mix_clk",
			CLK_SET_RATE_PARENT | CLK_SET_RATE_ENABLED,
			pxa_unit->apmu_base + APMU_CCIC1,
			0x12, 0x12, 0x0, 0, &ccic1_lock);
	mmp_clk_add(unit, PXA1U88_CLK_SC2_CSI_CLK, clk);

	sc2_axi_mix_config.reg_info.reg_clk_ctrl = pxa_unit->apmu_base + APMU_ISP;
	mmp_clk_register_mix(NULL, "sc2_axi_mix_clk",
			sc2_axi_parent_names,
			ARRAY_SIZE(sc2_axi_parent_names), 0,
			&sc2_axi_mix_config, &isp_lock);

	clk = mmp_clk_register_gate(NULL, "sc2_axi_clk", "sc2_axi_mix_clk",
			CLK_SET_RATE_PARENT | CLK_SET_RATE_ENABLED,
			pxa_unit->apmu_base + APMU_ISP,
			0x30000, 0x30000, 0x0, 0, &isp_lock);
	mmp_clk_add(unit, PXA1U88_CLK_SC2_AXI_CLK, clk);

	clk_register_mux(NULL, "sc2_phy2ln_mux", sc2_phy_parent_names,
			ARRAY_SIZE(sc2_phy_parent_names), 0, pxa_unit->apmu_base + APMU_CCIC1,
			7, 1, 0, &ccic1_lock);

	clk = mmp_clk_register_gate(NULL, "sc2_phy2ln_clk", "sc2_phy2ln_mux",
			CLK_SET_RATE_PARENT,
			pxa_unit->apmu_base + APMU_CCIC1,
			0x24, 0x24, 0x0, 0, &ccic1_lock);
	mmp_clk_add(unit, PXA1U88_CLK_SC2_PHY2LN_CLK_EN, clk);

	clk_register_mux(NULL, "sc2_phy4ln_mux", sc2_phy_parent_names,
			ARRAY_SIZE(sc2_phy_parent_names), 0, pxa_unit->apmu_base + APMU_CCIC0,
			7, 1, 0, &ccic0_lock);

	clk = mmp_clk_register_gate(NULL, "sc2_phy4ln_clk", "sc2_phy4ln_mux",
			CLK_SET_RATE_PARENT,
			pxa_unit->apmu_base + APMU_CCIC0,
			0x24, 0x24, 0x0, 0, &ccic0_lock);
	mmp_clk_add(unit, PXA1U88_CLK_SC2_PHY4LN_CLK_EN, clk);

	isp_pipe_mix_config.reg_info.reg_clk_ctrl = pxa_unit->apmu_base + APMU_ISP;
	mmp_clk_register_mix(NULL, "isp_pipe_mix_clk",
			isp_pipe_parent_names,
			ARRAY_SIZE(isp_pipe_parent_names), 0,
			&isp_pipe_mix_config, &isp_lock);

	/* add isp ahb and isp axi reset to pipe clk enable */
	clk = mmp_clk_register_gate(NULL, "isp_pipe_clk", "isp_pipe_mix_clk",
			CLK_SET_RATE_PARENT | CLK_SET_RATE_ENABLED,
			pxa_unit->apmu_base + APMU_ISP,
			0x503, 0x503, 0x0, 0, &isp_lock);
	mmp_clk_add(unit, PXA1U88_CLK_ISP_PIPE_CLK, clk);

	clk_register_divider(NULL, "isp_core_div", "pll1_624_gate",
			0, pxa_unit->apmu_base + APMU_ISP,
			24, 3, 0, &isp_lock);

	clk = mmp_clk_register_gate(NULL, "isp_core_gate", "isp_core_div",
			CLK_SET_RATE_PARENT,
			pxa_unit->apmu_base + APMU_ISP,
			0x18000000, 0x18000000, 0x0, 0, &isp_lock);
	mmp_clk_add(unit, PXA1U88_CLK_ISP_CORE_CLK_EN, clk);

	clk = mmp_clk_register_gate(NULL, "sc2_ahb_gate", NULL, 0,
			pxa_unit->apmu_base + APMU_CCIC0,
			0x600000, 0x600000, 0x0, 0, &ccic0_lock);
	mmp_clk_add(unit, PXA1U88_CLK_SC2_AHB_CLK, clk);
}

static DEFINE_SPINLOCK(fc_seq_lock);

/* CORE */
static const char *core_parent[] = {
	"pll1_624", "pll1_1248", "pll2", "pll1_832", "pll3p",
};

static struct parents_table core_parent_table[] = {
	{
		.parent_name = "pll1_624",
		.hw_sel_val = 0x0,
	},
	{
		.parent_name = "pll1_1248",
		.hw_sel_val = 0x1,
	},
	{
		.parent_name = "pll2",
		.hw_sel_val = 0x2,
	},
	{
		.parent_name = "pll1_832",
		.hw_sel_val = 0x3,
	},
	{
		.parent_name = "pll3p",
		.hw_sel_val = 0x5,
	},
};

/*
 * For HELAN2:
 * PCLK = AP_CLK_SRC / (CORE_CLK_DIV + 1)
 * BIU_CLK = PCLK / (BIU_CLK_DIV + 1)
 * MC_CLK = PCLK / (MC_CLK_DIV + 1)
 *
 * AP clock source:
 * 0x0 = PLL1 624 MHz
 * 0x1 = PLL1 1248 MHz  or PLL3_CLKOUT
 * (depending on FCAP[2])
 * 0x2 = PLL2_CLKOUT
 * 0x3 = PLL1 832 MHZ
 * 0x5 = PLL3_CLKOUTP
 */
static struct cpu_opt helan2_op_array[] = {
	{
		.pclk = 624,
		.pdclk = 312,
		.baclk = 156,
		.ap_clk_sel = 0x0,
	},
	{
		.pclk = 832,
		.pdclk = 416,
		.baclk = 208,
		.ap_clk_sel = 0x3,
	},
	{
		.pclk = 1057,
		.pdclk = 528,
		.baclk = 264,
		.ap_clk_sel = 0x2,
	},
	{
		.pclk = 1248,
		.pdclk = 624,
		.baclk = 312,
		.ap_clk_sel = 0x1,
	},
	{
		.pclk = 1526,
		.pdclk = 763,
		.baclk = 381,
		.ap_clk_sel = 0x5,
		.ap_clk_src = 1526,
	},
	{
		.pclk = 1803,
		.pdclk = 901,
		.baclk = 450,
		.ap_clk_sel = 0x5,
		.ap_clk_src = 1803,
	}
};

static struct cpu_rtcwtc cpu_rtcwtc_1908sec[] = {
	{.max_pclk = 832, .l1_xtc = 0x55555555, .l2_xtc = 0x00005555,},
	{.max_pclk = 1057, .l1_xtc = 0x55555555, .l2_xtc = 0x0000555A,},
	{.max_pclk = 1526, .l1_xtc = 0xAAAAAAAA, .l2_xtc = 0x0000AAAA,},
};

static struct core_params core_params = {
	.parent_table = core_parent_table,
	.parent_table_size = ARRAY_SIZE(core_parent_table),
	.cpu_opt = helan2_op_array,
	.cpu_opt_size = ARRAY_SIZE(helan2_op_array),
	.cpu_rtcwtc_table = cpu_rtcwtc_1908sec,
	.cpu_rtcwtc_table_size = ARRAY_SIZE(cpu_rtcwtc_1908sec),
	.bridge_cpurate = 1248,
	.max_cpurate = 1248,
	.dcstat_support = true,
};

static struct pxa1U88_clk_unit *globla_pxa_unit;
static int pxa1u88_powermode(u32 cpu)
{
	unsigned status_temp = 0;
	status_temp = ((__raw_readl(globla_pxa_unit->apmu_base +
			APMU_CORE_STATUS)) &
			((1 << (6 + 3 * cpu)) | (1 << (7 + 3 * cpu))));
	if (!status_temp)
		return MAX_LPM_INDEX;
	if (status_temp & (1 << (6 + 3 * cpu)))
		return LPM_C1;
	else if (status_temp & (1 << (7 + 3 * cpu)))
		return LPM_C2;
	return 0;
}

/* DDR */
static const char *ddr_parent[] = {
	"pll1_624", "pll1_832", "pll1_1248", "pll2", "pll4", "pll3p",
};

/*
 * DDR clock source:
 * 0x0 = PLL1 624 MHz
 * 0x1 = PLL1 832 MHz
 * 0x4 = PLL2_CLKOUT
 * 0x5 = PLL4_CLKOUT
 * 0x6 = PLL3_CLKOUTP(helan2), PLL1_1248(ULC)
 */
static struct parents_table ddr_parent_table[] = {
	{
		.parent_name = "pll1_624",
		.hw_sel_val = 0x0,
	},
	{
		.parent_name = "pll1_832",
		.hw_sel_val = 0x1,
	},
	{
		.parent_name = "pll2",
		.hw_sel_val = 0x4,
	},
	{
		.parent_name = "pll4",
		.hw_sel_val = 0x5,
	},
	/* only cares ulc, and pll3p dedicate for cpu, ignore it */
	{
		.parent_name = "pll1_1248",
		.hw_sel_val = 0x6,
	},
};

/* helan2 only support 800M */
static struct ddr_opt lpddr800_oparray[] = {
	{
		.dclk = 156,
		.ddr_tbl_index = 2,
		.ddr_clk_sel = 0x0,
	},
	{
		.dclk = 312,
		.ddr_tbl_index = 4,
		.ddr_clk_sel = 0x0,
	},
	{
		.dclk = 416,
		.ddr_tbl_index = 6,
		.ddr_clk_sel = 0x1,
	},
	{
		.dclk = 528,
		.ddr_tbl_index = 8,
		.ddr_clk_sel = 0x4,
	},
	{
		.dclk = 797,
		.ddr_tbl_index = 10,
		.ddr_clk_sel = 0x5,
	},
};

static unsigned long hwdfc_freq_table[] = {
	0, 312000, 312000, 416000, 528000, 528000, 624000, 667000
};

/*
 * ULC support max to 667M, and has extra 624M PP
 * FIXME: ULC could assign specific lpm tbl, use 0 for bringup.
 */
static struct ddr_opt lpddr533_oparray[] = {
	{
		.dclk = 156,
		.ddr_tbl_index = 2,
		.ddr_lpmtbl_index = 0,
		.ddr_clk_sel = 0x0,
	},
	{
		.dclk = 312,
		.ddr_tbl_index = 4,
		.ddr_lpmtbl_index = 0,
		.ddr_clk_sel = 0x0,
	},
	{
		.dclk = 416,
		.ddr_tbl_index = 6,
		.ddr_lpmtbl_index = 0,
		.ddr_clk_sel = 0x1,
	},
	{
		.dclk = 528,
		.ddr_tbl_index = 8,
		.ddr_lpmtbl_index = 0,
		.ddr_clk_sel = 0x4,
	},
};

static struct ddr_opt lpddr667_oparray[] = {
	{
		.dclk = 156,
		.ddr_tbl_index = 2,
		.ddr_lpmtbl_index = 0,
		.ddr_clk_sel = 0x0,
	},
	{
		.dclk = 312,
		.ddr_tbl_index = 4,
		.ddr_lpmtbl_index = 0,
		.ddr_clk_sel = 0x0,
	},
	{
		.dclk = 416,
		.ddr_tbl_index = 6,
		.ddr_lpmtbl_index = 0,
		.ddr_clk_sel = 0x1,
	},
	{
		.dclk = 528,
		.ddr_tbl_index = 8,
		.ddr_lpmtbl_index = 0,
		.ddr_clk_sel = 0x4,
	},
	/* FIXME: if we really need 624M PP? Here for verification */
	{
		.dclk = 624,
		.ddr_tbl_index = 10,
		.ddr_lpmtbl_index = 0,
		.ddr_clk_sel = 0x6,
	},
	{
		.dclk = 667,
		.ddr_tbl_index = 12,
		.ddr_lpmtbl_index = 0,
		.ddr_clk_sel = 0x5,
	},
};

static struct ddr_params ddr_params = {
	.parent_table = ddr_parent_table,
	.parent_table_size = ARRAY_SIZE(ddr_parent_table),
	.hwdfc_freq_table = hwdfc_freq_table,
	.hwdfc_table_size = ARRAY_SIZE(hwdfc_freq_table),
	.dcstat_support = true,
};


static const char *axi_parent[] = {
	"pll1_416", "pll1_624", "pll2", "pll2p",
};

/*
 * AXI clock source:
 * 0x0 = PLL1 416 MHz
 * 0x1 = PLL1 624 MHz
 * 0x2 = PLL2_CLKOUT
 * 0x3 = PLL2_CLKOUTP
 */
static struct parents_table axi_parent_table[] = {
	{
		.parent_name = "pll1_416",
		.hw_sel_val = 0x0,
	},
	{
		.parent_name = "pll1_624",
		.hw_sel_val = 0x1,
	},
	{
		.parent_name = "pll2",
		.hw_sel_val = 0x2,
	},
	{
		.parent_name = "pll2p",
		.hw_sel_val = 0x3,
	},
};

static struct axi_opt axi_oparray[] = {
	{
		.aclk = 156,
		.axi_clk_sel = 0x1,
	},
	{
		.aclk = 208,
		.axi_clk_sel = 0x0,
	},
};

static struct axi_params axi_params = {
	.parent_table = axi_parent_table,
	.parent_table_size = ARRAY_SIZE(axi_parent_table),
	.dcstat_support = true,
};

static struct ddr_combclk_relation aclk_dclk_relationtbl_1U88[] = {
	{.dclk_rate = 156000000, .combclk_rate = 156000000},
	{.dclk_rate = 312000000, .combclk_rate = 156000000},
	{.dclk_rate = 416000000, .combclk_rate = 208000000},
	{.dclk_rate = 528000000, .combclk_rate = 208000000},
	{.dclk_rate = 624000000, .combclk_rate = 208000000},
	{.dclk_rate = 667000000, .combclk_rate = 208000000},
	{.dclk_rate = 797000000, .combclk_rate = 208000000},
};

static void __init pxa1U88_acpu_init(struct pxa1U88_clk_unit *pxa_unit)
{
	struct mmp_clk_unit *unit = &pxa_unit->unit;
	struct clk *clk;

	core_params.apmu_base = pxa_unit->apmu_base;
	core_params.mpmu_base = pxa_unit->mpmu_base;
	core_params.ciu_base = pxa_unit->ciu_base;
	/* for debug purpose, pass max cpu frequency from uboot cmdline */
	if (max_freq)
		core_params.max_cpurate = max_freq;
	core_params.pxa_powermode = pxa1u88_powermode;
	mmp_clk_parents_lookup(core_params.parent_table,
		core_params.parent_table_size);

	ddr_params.apmu_base = pxa_unit->apmu_base;
	ddr_params.mpmu_base = pxa_unit->mpmu_base;
	ddr_params.ddr_opt = lpddr533_oparray;
	ddr_params.ddr_opt_size = ARRAY_SIZE(lpddr533_oparray);
	if (ddr_mode == DDR_667M) {
		ddr_params.ddr_opt = lpddr667_oparray;
		ddr_params.ddr_opt_size =
			ARRAY_SIZE(lpddr667_oparray);
	} else if (ddr_mode == DDR_800M) {
		ddr_params.ddr_opt = lpddr800_oparray;
		ddr_params.ddr_opt_size = ARRAY_SIZE(lpddr800_oparray);
	}
	mmp_clk_parents_lookup(ddr_params.parent_table,
		ddr_params.parent_table_size);

	axi_params.apmu_base = pxa_unit->apmu_base;
	axi_params.mpmu_base = pxa_unit->mpmu_base;
	axi_params.axi_opt = axi_oparray;
	axi_params.axi_opt_size = ARRAY_SIZE(axi_oparray);
	mmp_clk_parents_lookup(axi_params.parent_table,
		axi_params.parent_table_size);

	clk = mmp_clk_register_core("cpu", core_parent,
		ARRAY_SIZE(core_parent), CLK_GET_RATE_NOCACHE,
		0, &fc_seq_lock, &core_params);
	clk_prepare_enable(clk);
	mmp_clk_add(unit, PXA1U88_CLK_CPU, clk);

	clk = mmp_clk_register_ddr("ddr", ddr_parent,
		ARRAY_SIZE(ddr_parent), CLK_GET_RATE_NOCACHE,
		0, &fc_seq_lock, &ddr_params);
	mmp_clk_add(unit, PXA1U88_CLK_DDR, clk);
	clk_prepare_enable(clk);

	clk = mmp_clk_register_axi("axi", axi_parent,
		ARRAY_SIZE(axi_parent), CLK_GET_RATE_NOCACHE,
		0, &fc_seq_lock, &axi_params);
	clk_prepare_enable(clk);
	mmp_clk_add(unit, PXA1U88_CLK_AXI, clk);
	register_clk_bind2ddr(clk,
		axi_params.axi_opt[axi_params.axi_opt_size - 1].aclk * MHZ,
		aclk_dclk_relationtbl_1U88,
		ARRAY_SIZE(aclk_dclk_relationtbl_1U88));
}

static void __init pxa1U88_misc_init(struct pxa1U88_clk_unit *pxa_unit)
{
	unsigned int val;

	/* enable all MCK and AXI fabric dynamic clk gating */
	val = __raw_readl(pxa_unit->ciu_base + CIU_MC_CONF);
	/* enable dclk gating */
	val &= ~(1 << 19);
	/* enable 1x2 fabric AXI clock dynamic gating */
	val |= (0xff << 8) |	/* MCK5 P0~P7*/
		(1 << 16) |		/* CP 2x1 fabric*/
		(1 << 17) | (1 << 18) |	/* AP&CP */
		(1 << 20) | (1 << 21) |	/* SP&CSAP 2x1 fabric */
		(1 << 26) | (1 << 27) | /* Fabric 0/1 */
		(1 << 29) | (1 << 30);	/* CA7 2x1 fabric */
	__raw_writel(val, pxa_unit->ciu_base + CIU_MC_CONF);

	/* TOP_MEM_RTC_WTC_SPD = 0xEE006656 */
	__raw_writel(0xEE006656, pxa_unit->ciu_base + TOP_MEM_RTC_WTC_SPD);

	/*enable HW-DVC and HW-DFC when CP is fast wakeup*/
	val = __raw_readl(pxa_unit->apmu_base + APMU_DVC_DFC_DEBUG);
	val |= (1 << 5);
	__raw_writel(val, pxa_unit->apmu_base + APMU_DVC_DFC_DEBUG);

}

static void __init pxa1U88_clk_init(struct device_node *np)
{
	struct pxa1U88_clk_unit *pxa_unit;

	pxa_unit = kzalloc(sizeof(*pxa_unit), GFP_KERNEL);
	if (!pxa_unit) {
		pr_err("failed to allocate memory for pxa1U88 clock unit\n");
		return;
	}

	pxa_unit->mpmu_base = of_iomap(np, 0);
	if (!pxa_unit->mpmu_base) {
		pr_err("failed to map mpmu registers\n");
		goto err;
	}

	pxa_unit->apmu_base = of_iomap(np, 1);
	if (!pxa_unit->apmu_base) {
		pr_err("failed to map apmu registers\n");
		goto err;
	}

	pxa_unit->apbc_base = of_iomap(np, 2);
	if (!pxa_unit->apbc_base) {
		pr_err("failed to map apbc registers\n");
		goto err;
	}

	pxa_unit->apbcp_base = of_iomap(np, 3);
	if (!pxa_unit->apbcp_base) {
		pr_err("failed to map apbcp registers\n");
		goto err;
	}

	pxa_unit->apbs_base = of_iomap(np, 4);
	if (!pxa_unit->apbs_base) {
		pr_err("failed to map apbs registers\n");
		goto err;
	}

	pxa_unit->ciu_base = of_iomap(np, 5);
	if (!pxa_unit->ciu_base) {
		pr_err("failed to map ciu registers\n");
		goto err;
	}

	mmp_clk_init(np, &pxa_unit->unit, PXA1U88_NR_CLKS);

	pxa1U88_misc_init(pxa_unit);
	pxa1U88_pll_init(pxa_unit);
	if (!board_is_fpga())
		pxa1U88_acpu_init(pxa_unit);
	pxa1U88_apb_periph_clk_init(pxa_unit);

	pxa1U88_axi_periph_clk_init(pxa_unit);

#ifdef CONFIG_SMC91X
	if (board_is_fpga())
		smc91x_clk_init(pxa_unit->apmu_base);
#endif

#if defined(CONFIG_PXA_DVFS)
	setup_pxa1908_dvfs_platinfo();
#endif

#ifdef CONFIG_DEBUG_FS
	globla_pxa_unit = pxa_unit;
#endif
	return;

err:
	kfree(pxa_unit);

}
CLK_OF_DECLARE(pxa1U88_clk, "marvell,pxa1U88-clock", pxa1U88_clk_init);

#ifdef CONFIG_DEBUG_FS
static struct dentry *stat;

CLK_DCSTAT_OPS(globla_pxa_unit->unit.clk_table[PXA1U88_CLK_DDR], ddr);
CLK_DCSTAT_OPS(globla_pxa_unit->unit.clk_table[PXA1U88_CLK_AXI], axi);
CLK_DCSTAT_OPS(globla_pxa_unit->unit.clk_table[PXA1U88_CLK_GC3D], gc);
CLK_DCSTAT_OPS(globla_pxa_unit->unit.clk_table[PXA1U88_CLK_GC2D], gc2d);
CLK_DCSTAT_OPS(globla_pxa_unit->unit.clk_table[PXA1U88_CLK_GCSH], gcsh);
CLK_DCSTAT_OPS(globla_pxa_unit->unit.clk_table[PXA1U88_CLK_VPU], vpu);

static int __init __init_pxa1u88_dcstat_debugfs_node(void)
{
	struct dentry *cpu_dc_stat = NULL, *ddr_dc_stat = NULL;
	struct dentry *axi_dc_stat = NULL;
	struct dentry *gc_dc_stat = NULL, *vpu_dc_stat = NULL;
	struct dentry *gc2d_dc_stat = NULL, *gcsh_dc_stat = NULL;

	if (!cpu_is_pxa1908())
		return 0;

	stat = debugfs_create_dir("stat", pxa);
	if (!stat)
		return -ENOENT;

	cpu_dc_stat = cpu_dcstat_file_create("cpu_dc_stat", stat);
	if (!cpu_dc_stat)
		goto err_cpu_dc_stat;

	ddr_dc_stat = clk_dcstat_file_create("ddr_dc_stat", stat,
			&ddr_dc_ops);
	if (!ddr_dc_stat)
		goto err_ddr_dc_stat;

	axi_dc_stat = clk_dcstat_file_create("axi_dc_stat", stat,
			&axi_dc_ops);
	if (!axi_dc_stat)
		goto err_axi_dc_stat;

	gc_dc_stat = clk_dcstat_file_create("gc3d_core0_dc_stat",
		stat, &gc_dc_ops);
	if (!gc_dc_stat)
		goto err_gc_dc_stat;

	gc2d_dc_stat = clk_dcstat_file_create("gc2d_core0_dc_stat",
		stat, &gc2d_dc_ops);
	if (!gc2d_dc_stat)
		goto err_gc2d_dc_stat;

	gcsh_dc_stat = clk_dcstat_file_create("gcsh_core0_dc_stat",
		stat, &gcsh_dc_ops);
	if (!gcsh_dc_stat)
		goto err_gcshader_dc_stat;

	vpu_dc_stat = clk_dcstat_file_create("vpu_dc_stat",
		stat, &vpu_dc_ops);
	if (!vpu_dc_stat)
		goto err_vpu_dc_stat;

	return 0;

err_vpu_dc_stat:
	debugfs_remove(gcsh_dc_stat);
err_gcshader_dc_stat:
	debugfs_remove(gc2d_dc_stat);
err_gc2d_dc_stat:
	debugfs_remove(gc_dc_stat);
err_gc_dc_stat:
	debugfs_remove(axi_dc_stat);
err_axi_dc_stat:
	debugfs_remove(ddr_dc_stat);
err_ddr_dc_stat:
	debugfs_remove(cpu_dc_stat);
err_cpu_dc_stat:
	debugfs_remove(stat);
	return -ENOENT;

}
/* clock init is before debugfs_create_dir("pxa", NULL), so
 * use arch_initcall init the pxa1u88 dcstat node.
 */
arch_initcall(__init_pxa1u88_dcstat_debugfs_node);
#endif
