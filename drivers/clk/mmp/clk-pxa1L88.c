#include <linux/io.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#include <linux/devfreq.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include <dt-bindings/clock/marvell-pxa1L88.h>
#include <linux/debugfs-pxa.h>

#include "clk.h"
#include "clk-pll-helanx.h"
#include "clk-core-helanx.h"
#include "clk-plat.h"
#include <linux/cputype.h>

#define APBS_PLL1_CTRL		0x100

#define APBC_RTC		0x28
#define APBC_TWSI0		0x2c
#define APBC_TWSI1		0x60
#define APBC_KPC		0x30
#define APBC_UART0		0x0
#define APBC_UART1		0x4
#define APBC_SSP0		0x1c
#define APBC_SSP1		0x20
#define APBC_SSP2		0x4c

#define APBC_GPIO		0x8
#define APBC_PWM0		0xc
#define APBC_PWM1		0x10
#define APBC_PWM2		0x14
#define APBC_PWM3		0x18
#define APBC_DROTS		0x58
#define APBC_SWJTAG		0x40

/* IPC/RIPC clock */
#define APBC_IPC_CLK_RST	0x24
#define APBCP_AICER		0x38

#define APBCP_TWSI2		0x28
#define APBCP_UART2		0x1c

#define MPMU_UART_PLL		0x14
#define MPMU_VRCR		0x18

#define APMU_SQU_CLK_GATE_CTRL	0x1C
#define APMU_ISPDXO		0x38
#define APMU_CLK_GATE_CTRL	0x40
#define APMU_SDH0		0x54
#define APMU_SDH1		0x58
#define APMU_SDH2		0xe0
#define APMU_CCIC0		0x50
#define APMU_USB		0x5c
#define APMU_NF			0x60
#define APMU_AES		0x68
#define APMU_TRACE		0x108

#define APMU_CORE_STATUS 0x090

/* PLL */
#define MPMU_PLL2CR	   (0x0034)
#define MPMU_PLL3CR	   (0x001c)
#define MPMU_POSR	   (0x0010)
#define POSR_PLL2_LOCK	   (1 << 29)
#define POSR_PLL3_LOCK	   (1 << 30)
#define APB_SPARE_PLL2CR    (0x104)
#define APB_SPARE_PLL3CR    (0x108)

#define APMU_DSI1		0x44
#define APMU_DISP1		0x4c

#define APMU_GC			0xcc
#define APMU_GC2D		0xf4
#define APMU_VPU		0xa4

#define CIU_MC_CONF		0x0040
#define CIU_GPU2D_XTC		0x00a0
#define CIU_GPU_XTC		0x00a4
#define CIU_VPU_XTC		0x00a8

/* For audio */
#define MPMU_FCCR		0x8
#define MPMU_ISCCR0		0x40
#define MPMU_ISCCR1		0x44

/* GBS: clock for GSSP */
#define APBC_GBS       0xc
#define APBC_GCER      0x34

/*
 * peripheral clock source:
 * 0x0 = PLL1 416 MHz
 * 0x1 = PLL1 624 MHz
 * 0x2 = PLL2_CLKOUT
 * 0x3 = PLL2_CLKOUTP
 */
enum periph_clk_src {
	CLK_PLL1_416 = 0x0,
	CLK_PLL1_624 = 0x1,
	CLK_PLL2 = 0x2,
	CLK_PLL2P = 0x3,
};

static unsigned long pll3_vco_default;
static unsigned long pll3_default;
static unsigned long pll3p_default;

/*
 * uboot pass pll3_vco = xxx (unit is Mhz), it is used
 * to distinguish whether core and display can share pll3.
 * if core can use it, core max_freq is setup, else core
 * will not use pll3p, pll3 will only be used by display
*/
static int __init pll3_vco_value_setup(char *str)
{
	int n;
	if (!get_option(&str, &n))
		return 0;
	pll3_vco_default = n * MHZ_TO_HZ;
	return 1;
}
__setup("pll3_vco=", pll3_vco_value_setup);

struct pxa1L88_clk_unit {
	struct mmp_clk_unit unit;
	void __iomem *mpmu_base;
	void __iomem *apmu_base;
	void __iomem *apbc_base;
	void __iomem *apbcp_base;
	void __iomem *apbs_base;
	void __iomem *ciu_base;
};

static struct mmp_param_fixed_rate_clk fixed_rate_clks[] = {
	{PXA1L88_CLK_CLK32, "clk32", NULL, CLK_IS_ROOT, 32768},
	{PXA1L88_CLK_VCTCXO, "vctcxo", NULL, CLK_IS_ROOT, 26000000},
	{PXA1L88_CLK_PLL1_624, "pll1_624", NULL, CLK_IS_ROOT, 624000000},
	{PXA1L88_CLK_PLL1_416, "pll1_416", NULL, CLK_IS_ROOT, 416000000},
	{PXA1L88_CLK_PLL1_832, "pll1_832", NULL, CLK_IS_ROOT, 832000000},
	{PXA1L88_CLK_PLL1_1248, "pll1_1248", NULL, CLK_IS_ROOT, 1248000000},
};

static struct mmp_param_fixed_factor_clk fixed_factor_clks[] = {
	{PXA1L88_CLK_PLL1_2, "pll1_2", "pll1_624", 1, 2, 0},
	{PXA1L88_CLK_PLL1_4, "pll1_4", "pll1_2", 1, 2, 0},
	{PXA1L88_CLK_PLL1_8, "pll1_8", "pll1_4", 1, 2, 0},
	{PXA1L88_CLK_PLL1_16, "pll1_16", "pll1_8", 1, 2, 0},
	{PXA1L88_CLK_PLL1_6, "pll1_6", "pll1_2", 1, 3, 0},
	{PXA1L88_CLK_PLL1_12, "pll1_12", "pll1_6", 1, 2, 0},
	{PXA1L88_CLK_PLL1_24, "pll1_24", "pll1_12", 1, 2, 0},
	{PXA1L88_CLK_PLL1_48, "pll1_48", "pll1_24", 1, 2, 0},
	{PXA1L88_CLK_PLL1_96, "pll1_96", "pll1_48", 1, 2, 0},
	{PXA1L88_CLK_PLL1_13, "pll1_13", "pll1_624", 1, 13, 0},
	{PXA1L88_CLK_PLL1_13_1_5, "pll1_13_1_5", "pll1_13", 2, 3, 0},
	{PXA1L88_CLK_PLL1_2_1_5, "pll1_2_1_5", "pll1_2", 2, 3, 0},
	{PXA1L88_CLK_PLL1_3_16, "pll1_3_16", "pll1_624", 3, 16, 0},
};

enum pll {
	PLL2 = 0,
	PLL3,
	MAX_PLL_NUM
};

struct plat_pll_info {
	spinlock_t lock;
	const char *vco_name;
	const char *out_name;
	const char *outp_name;
	unsigned long vco_flag;
	unsigned long vcoclk_flag;
	unsigned long out_flag;
	unsigned long outclk_flag;
	unsigned long outp_flag;
	unsigned long outpclk_flag;
};

/* First index is ddr_mode */
static struct mmp_vco_params pll_vco_params[][MAX_PLL_NUM] = {
	[0] = {
		{
			.vco_min = 1200000000UL,
			.vco_max = 2500000000UL,
			.lock_enable_bit = POSR_PLL2_LOCK,
			.default_rate = 1595 * MHZ,
		},
		{
			.vco_min = 1200000000UL,
			.vco_max = 2500000000UL,
			.lock_enable_bit = POSR_PLL3_LOCK,
			.default_rate = (unsigned long)2366 * MHZ,
		},
	},
	[1] = {
		{
			.vco_min = 1200000000UL,
			.vco_max = 2500000000UL,
			.lock_enable_bit = POSR_PLL2_LOCK,
			.default_rate = 2132 * MHZ,
		},
		{
			.vco_min = 1200000000UL,
			.vco_max = 2500000000UL,
			.lock_enable_bit = POSR_PLL3_LOCK,
			.default_rate = (unsigned long)2366 * MHZ,
		},
	}
};

static struct pxa1L88_clk_unit *global_pxa_unit;
int pxa1x88_powermode(u32 cpu)
{
	unsigned status_temp = 0;
	status_temp = ((__raw_readl(global_pxa_unit->apmu_base +
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


/* First index is ddr_mode */
static struct mmp_pll_params pll_params[][MAX_PLL_NUM] = {
	[0] = {
		{
			.default_rate = 797 * MHZ,
		},
		{
			.default_rate = 788 * MHZ,
		},
	},
	[1] = {
		{
			.default_rate = 710 * MHZ,
		},
		{
			.default_rate = 788 * MHZ,
		},
	}
};

/* First index is ddr_mode */
static struct mmp_pll_params pllp_params[][MAX_PLL_NUM] = {
	[0] = {
		{
			.default_rate = 797 * MHZ,
		},
		{
			.default_rate = 1183 * MHZ,
		},
	},
	[1] = {
		{
			.default_rate = 1066 * MHZ,
		},
		{
			.default_rate = 1183 * MHZ,
		},
	}
};

static struct plat_pll_info pllx_platinfo[] = {
	{
		.vco_name = "pll2_vco",
		.out_name = "pll2",
		.outp_name = "pll2p",
		.vcoclk_flag = CLK_IS_ROOT,
		.vco_flag = HELANX_PLL2CR_V1 | HELANX_PLL_40NM,
		.out_flag = HELANX_PLLOUT,
		.outp_flag = HELANX_PLLOUTP,
	},
	{
		.vco_name = "pll3_vco",
		.out_name = "pll3",
		.outp_name = "pll3p",
		.vcoclk_flag = CLK_IS_ROOT,
		.vco_flag = HELANX_PLL_40NM,
		.out_flag = HELANX_PLLOUT,
		.outp_flag = HELANX_PLLOUTP,
	},
};

/*
 * uboot may pass pll3_vco value to kernel.
 * pll3 may be shared between core and display.
 * if it can be shared, core will use pll3p and it is set to max_freq
 * or pll3 and pll3p will be set to be the same with pll3_vco
*/
static inline void setup_pll3(void)
{
	if (!pll3_vco_default)
		return;

	if (!(pll3_vco_default % max_freq))
		pll3p_default = max_freq * MHZ_TO_HZ;
	else /* Core will not use pll3p */
		pll3p_default = pll3_vco_default;

	/* LCD pll3 source can't be higher than 1Ghz */
	if (pll3_vco_default > 2000 * MHZ_TO_HZ)
		pll3_default = pll3_vco_default / 3;
	else if (pll3_vco_default > 1000 * MHZ_TO_HZ)
		pll3_default = pll3_vco_default / 2;
	else
		pll3_default = pll3_vco_default;

	pll_vco_params[ddr_mode][PLL3].default_rate = pll3_vco_default;
	pll_params[ddr_mode][PLL3].default_rate = pll3_default;
	pllp_params[ddr_mode][PLL3].default_rate = pll3p_default;
}

static void pxa1L88_dynpll_init(struct pxa1L88_clk_unit *pxa_unit)
{
	int idx;
	struct clk *clk;
	void __iomem *mpmu_base = pxa_unit->mpmu_base;
	void __iomem *apbs_base = pxa_unit->apbs_base;

	pll_vco_params[ddr_mode][PLL2].cr_reg = mpmu_base + MPMU_PLL2CR;
	pll_vco_params[ddr_mode][PLL2].pll_swcr = apbs_base + APB_SPARE_PLL2CR;
	pll_vco_params[ddr_mode][PLL3].cr_reg = mpmu_base + MPMU_PLL3CR;
	pll_vco_params[ddr_mode][PLL3].pll_swcr = apbs_base + APB_SPARE_PLL3CR;

	pll_params[ddr_mode][PLL2].pll_swcr = apbs_base + APB_SPARE_PLL2CR;
	pll_params[ddr_mode][PLL3].pll_swcr = apbs_base + APB_SPARE_PLL3CR;

	pllp_params[ddr_mode][PLL2].pll_swcr = apbs_base + APB_SPARE_PLL2CR;
	pllp_params[ddr_mode][PLL3].pll_swcr = apbs_base + APB_SPARE_PLL3CR;

	setup_pll3();

	for (idx = 0; idx < ARRAY_SIZE(pllx_platinfo); idx++) {
		spin_lock_init(&pllx_platinfo[idx].lock);
		/* vco */
		pll_vco_params[ddr_mode][idx].lock_reg = mpmu_base + MPMU_POSR;
		clk = helanx_clk_register_vco(pllx_platinfo[idx].vco_name, 0,
					      pllx_platinfo[idx].vcoclk_flag,
					      pllx_platinfo[idx].vco_flag,
					      &pllx_platinfo[idx].lock,
					      &pll_vco_params[ddr_mode][idx]);
		clk_set_rate(clk, pll_vco_params[ddr_mode][idx].default_rate);
		/* pll */
		clk = helanx_clk_register_pll(pllx_platinfo[idx].out_name,
					      pllx_platinfo[idx].vco_name,
					      pllx_platinfo[idx].outclk_flag,
					      pllx_platinfo[idx].out_flag,
					      &pllx_platinfo[idx].lock,
					      &pll_params[ddr_mode][idx]);
		clk_set_rate(clk, pll_params[ddr_mode][idx].default_rate);

		/* pllp */
		clk = helanx_clk_register_pll(pllx_platinfo[idx].outp_name,
					      pllx_platinfo[idx].vco_name,
					      pllx_platinfo[idx].outpclk_flag,
					      pllx_platinfo[idx].outp_flag,
					      &pllx_platinfo[idx].lock,
					      &pllp_params[ddr_mode][idx]);
		clk_set_rate(clk, pllp_params[ddr_mode][idx].default_rate);
	}
}

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

static struct mmp_clk_factor_masks isccr1_factor_masks = {
	.factor = 1,
	.den_mask = 0xfff,
	.num_mask = 0x7fff,
	.den_shift = 15,
	.num_shift = 0,
};

static struct mmp_clk_factor_tbl isccr1_factor_tbl[] = {
	{.num = 14906, .den = 440},	/*for 0kHz*/
	{.num = 1625, .den = 256},	/*8kHz*/
	{.num = 3042, .den = 1321},	/*44.1kHZ */
};

static const struct clk_div_table clk_ssp1_ref_table[] = {
	{ .val = 0, .div = 2 },
	{ .val = 1, .div = 4 },
	{ .val = 2, .div = 6 },
	{ .val = 3, .div = 8 },
	{ .val = 0, .div = 0 },
};

static DEFINE_SPINLOCK(pll1_lock);
static struct mmp_param_general_gate_clk pll1_gate_clks[] = {
	{PXA1L88_CLK_PLL1_416_GATE, "pll1_416_gate", "pll1_416", 0, APMU_CLK_GATE_CTRL, 27, 0, &pll1_lock},
	{PXA1L88_CLK_PLL1_624_GATE, "pll1_624_gate", "pll1_624", 0, APMU_CLK_GATE_CTRL, 26, 0, &pll1_lock},
};

static void pxa1L88_pll_init(struct pxa1L88_clk_unit *pxa_unit)
{
	struct clk *clk;
	struct mmp_clk_unit *unit = &pxa_unit->unit;

	mmp_register_fixed_rate_clks(unit, fixed_rate_clks,
					ARRAY_SIZE(fixed_rate_clks));

	mmp_register_fixed_factor_clks(unit, fixed_factor_clks,
					ARRAY_SIZE(fixed_factor_clks));

	clk = mmp_clk_register_factor("uart_pll", "pll1_4",
				CLK_SET_RATE_PARENT,
				pxa_unit->mpmu_base + MPMU_UART_PLL,
				&uart_factor_masks, uart_factor_tbl,
				ARRAY_SIZE(uart_factor_tbl), NULL);
	mmp_clk_add(unit, PXA1L88_CLK_UART_PLL, clk);

	mmp_register_general_gate_clks(unit, pll1_gate_clks,
				pxa_unit->apmu_base,
				ARRAY_SIZE(pll1_gate_clks));
	pxa1L88_dynpll_init(pxa_unit);

	clk = mmp_clk_register_gate(NULL, "vcxo_gate_share", "vctcxo",
				CLK_SET_RATE_PARENT,
				pxa_unit->mpmu_base + MPMU_VRCR,
				(1 << 8), (1 << 8), 0x0, 0, NULL);
	clk = clk_register_fixed_rate(NULL, "VCXO_OUT", "vcxo_gate_share",
				      0, 26000000);
	mmp_clk_add(unit, PXA1L88_CLK_VCXO_OUT, clk);
	/* Per AE's request: export VCXO_OUT2 clock node */
	clk = clk_register_fixed_rate(NULL, "VCXO_OUT2", "vcxo_gate_share",
				      0, 26000000);
	mmp_clk_add(unit, PXA1L88_CLK_VCXO_OUT2, clk);
}

static DEFINE_SPINLOCK(pwm0_lock);
static DEFINE_SPINLOCK(pwm2_lock);

static struct mmp_param_gate_clk apbc_gate_clks[] = {
	{PXA1L88_CLK_GPIO, "gpio_clk", "vctcxo", CLK_SET_RATE_PARENT, APBC_GPIO, 0x7, 0x3, 0x0, 0, NULL},
	{PXA1L88_CLK_KPC, "kpc_clk", "clk32", CLK_SET_RATE_PARENT, APBC_KPC, 0x7, 0x3, 0x0, MMP_CLK_GATE_NEED_DELAY, NULL},
	{PXA1L88_CLK_RTC, "rtc_clk", "clk32", CLK_SET_RATE_PARENT, APBC_RTC, 0x87, 0x83, 0x0, MMP_CLK_GATE_NEED_DELAY, NULL},
	{PXA1L88_CLK_THERMAL, "thermal", NULL, 0, APBC_DROTS, 0x7, 0x3, 0x0, 0, NULL},
	{PXA1L88_CLK_PWM0, "pwm0_fclk", "pwm01_apb_share", CLK_SET_RATE_PARENT, APBC_PWM0, 0x2, 0x2, 0x0, 0, &pwm0_lock},
	{PXA1L88_CLK_PWM1, "pwm1_fclk", "pwm01_apb_share", CLK_SET_RATE_PARENT, APBC_PWM1, 0x6, 0x2, 0x0, 0, NULL},
	{PXA1L88_CLK_PWM2, "pwm2_fclk", "pwm23_apb_share", CLK_SET_RATE_PARENT, APBC_PWM2, 0x2, 0x2, 0x0, 0, &pwm2_lock},
	{PXA1L88_CLK_PWM3, "pwm3_fclk", "pwm23_apb_share", CLK_SET_RATE_PARENT, APBC_PWM3, 0x6, 0x2, 0x0, 0, NULL},
};

static DEFINE_SPINLOCK(uart0_lock);
static DEFINE_SPINLOCK(uart1_lock);
static DEFINE_SPINLOCK(uart2_lock);
static DEFINE_SPINLOCK(ssp0_lock);
static const char *uart_parent_names[] = {"pll1_3_16", "uart_pll"};
static const char const *ssp_parent_names[] = {"pll1_96", "pll1_48", "pll1_24", "pll1_12"};
static const char const *ssp1_parent[] = {"vctcxo", "pll1_2"};
static const char const *gssp_parent[] = {"isccr0_i2sclk", "sys clk",
					"ext clk", "vctcxo"};

static DEFINE_SPINLOCK(twsi0_lock);
static DEFINE_SPINLOCK(twsi1_lock);
static DEFINE_SPINLOCK(twsi2_lock);
static const struct clk_div_table clk_twsi_ref_table[] = {
	{ .val = 0, .div = 19 },
	{ .val = 1, .div = 12 },
	{ .val = 2, .div = 10 },
	{ .val = 0, .div = 0 },
};
#ifdef CONFIG_CORESIGHT_SUPPORT
static void pxa1L88_coresight_clk_init(struct pxa1L88_clk_unit *pxa_unit)
{
	struct mmp_clk_unit *unit = &pxa_unit->unit;
	struct clk *clk;

	clk = mmp_clk_register_gate(NULL, "DBGCLK", "pll1_416", 0,
			pxa_unit->apmu_base + APMU_TRACE,
			0x10008, 0x10008, 0x0, 0, NULL);
	mmp_clk_add(unit, PXA1L88_CLK_DBGCLK, clk);

	/* TMC clock */
	clk = mmp_clk_register_gate(NULL, "TRACECLK", "DBGCLK", 0,
			pxa_unit->apmu_base + APMU_TRACE,
			0x10010, 0x10010, 0x0, 0, NULL);
	mmp_clk_add(unit, PXA1L88_CLK_TRACECLK, clk);
}
#endif

static void pxa1L88_apb_periph_clk_init(struct pxa1L88_clk_unit *pxa_unit)
{
	struct clk *clk;
	struct clk *i2s_clk;
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

	clk = clk_register_divider_table(NULL, "twsi0_div",
				"pll1_624", CLK_SET_RATE_PARENT,
				pxa_unit->apbc_base + APBC_TWSI0, 4, 3, 0,
				clk_twsi_ref_table, &twsi0_lock);
	clk = mmp_clk_register_gate(NULL, "twsi0_clk", "twsi0_div",
				CLK_SET_RATE_PARENT,
				pxa_unit->apbc_base + APBC_TWSI0,
				0x7, 0x3, 0x0, 0, &twsi0_lock);
	mmp_clk_add(unit, PXA1L88_CLK_TWSI0, clk);

	clk = clk_register_divider_table(NULL, "twsi1_div",
				"pll1_624", CLK_SET_RATE_PARENT,
				pxa_unit->apbc_base + APBC_TWSI1, 4, 3, 0,
				clk_twsi_ref_table, &twsi1_lock);
	clk = mmp_clk_register_gate(NULL, "twsi1_clk", "twsi1_div",
				CLK_SET_RATE_PARENT,
				pxa_unit->apbc_base + APBC_TWSI1,
				0x7, 0x3, 0x0, 0, &twsi1_lock);
	mmp_clk_add(unit, PXA1L88_CLK_TWSI1, clk);

	clk = clk_register_divider_table(NULL, "twsi2_div",
				"pll1_624", CLK_SET_RATE_PARENT,
				pxa_unit->apbcp_base + APBCP_TWSI2, 3, 2, 0,
				clk_twsi_ref_table, &twsi2_lock);
	clk = mmp_clk_register_gate(NULL, "twsi2_clk", "twsi2_div",
				CLK_SET_RATE_PARENT,
				pxa_unit->apbcp_base + APBCP_TWSI2,
				0x7, 0x3, 0x0, 0, &twsi2_lock);
	mmp_clk_add(unit, PXA1L88_CLK_TWSI2, clk);

	clk = clk_register_mux(NULL, "uart0_mux", uart_parent_names,
				ARRAY_SIZE(uart_parent_names),
				CLK_SET_RATE_PARENT,
				pxa_unit->apbc_base + APBC_UART0,
				4, 3, 0, &uart0_lock);
	clk = mmp_clk_register_gate(NULL, "uart0_clk", "uart0_mux",
				CLK_SET_RATE_PARENT,
				pxa_unit->apbc_base + APBC_UART0,
				0x7, 0x3, 0x0, 0, &uart0_lock);
	mmp_clk_add(unit, PXA1L88_CLK_UART0, clk);

	clk = clk_register_mux(NULL, "uart1_mux", uart_parent_names,
				ARRAY_SIZE(uart_parent_names),
				CLK_SET_RATE_PARENT,
				pxa_unit->apbc_base + APBC_UART1,
				4, 3, 0, &uart1_lock);
	clk = mmp_clk_register_gate(NULL, "uart1_clk", "uart1_mux",
				CLK_SET_RATE_PARENT,
				pxa_unit->apbc_base + APBC_UART1,
				0x7, 0x3, 0x0, 0, &uart1_lock);
	mmp_clk_add(unit, PXA1L88_CLK_UART1, clk);

	clk = clk_register_mux(NULL, "uart2_mux", uart_parent_names,
				ARRAY_SIZE(uart_parent_names),
				CLK_SET_RATE_PARENT,
				pxa_unit->apbcp_base + APBCP_UART2,
				4, 3, 0, &uart2_lock);
	clk = mmp_clk_register_gate(NULL, "uart2_clk", "uart2_mux",
				CLK_SET_RATE_PARENT,
				pxa_unit->apbcp_base + APBCP_UART2,
				0x7, 0x3, 0x0, 0, &uart2_lock);
	mmp_clk_add(unit, PXA1L88_CLK_UART2, clk);

	clk = clk_register_mux(NULL, "ssp0_mux", (const char **)ssp_parent_names,
				ARRAY_SIZE(ssp_parent_names), 0,
				pxa_unit->apbc_base + APBC_SSP0,
				4, 3, 0, &ssp0_lock);
	clk = mmp_clk_register_gate(NULL, "ssp0_clk", "ssp0_mux",
				CLK_SET_RATE_PARENT,
				pxa_unit->apbc_base + APBC_SSP0,
				0x7, 0x3, 0x0, 0, &ssp0_lock);
	mmp_clk_add(unit, PXA1L88_CLK_SSP0, clk);

	clk = clk_register_mux(NULL, "ssp1_mux", ssp1_parent,
			ARRAY_SIZE(ssp1_parent), 0,
			pxa_unit->mpmu_base + MPMU_FCCR, 28, 1, 0, NULL);
	clk = clk_register_fixed_factor(NULL, "pllclk_2", "ssp1_mux",
				CLK_SET_RATE_PARENT, 1, 2);
	clk = clk_register_gate(NULL, "sysclk_en", "pllclk_2", CLK_SET_RATE_PARENT,
				pxa_unit->mpmu_base + MPMU_ISCCR1, 31, 0, NULL);
	clk =  mmp_clk_register_factor("mn_div", "sysclk_en", CLK_SET_RATE_PARENT,
				pxa_unit->mpmu_base + MPMU_ISCCR1,
				&isccr1_factor_masks, isccr1_factor_tbl,
				ARRAY_SIZE(isccr1_factor_tbl), NULL);
	clk_set_rate(clk, 5644800);
	clk = clk_register_gate(NULL, "bitclk_en", "mn_div", CLK_SET_RATE_PARENT,
				pxa_unit->mpmu_base + MPMU_ISCCR1, 29, 0, NULL);
	clk = clk_register_divider_table(NULL, "bitclk_div_468", "bitclk_en", 0,
			pxa_unit->mpmu_base + MPMU_ISCCR1, 27, 2, 0, clk_ssp1_ref_table,
			NULL);
	clk_set_rate(clk, 1411200);
	clk = clk_register_gate(NULL, "fnclk", "bitclk_div_468", CLK_SET_RATE_PARENT,
				pxa_unit->apbc_base + APBC_SSP1, 1, 0, NULL);
	clk = mmp_clk_register_apbc("pxa988-ssp.1", "fnclk",
				pxa_unit->apbc_base + APBC_SSP1, 10, CLK_SET_RATE_PARENT, NULL);
	mmp_clk_add(unit, PXA1L88_CLK_SSP, clk);

	clk = clk_register_gate(NULL, "isccr0_i2sclk_base", "pllclk_2",
				CLK_SET_RATE_PARENT,
				pxa_unit->mpmu_base + MPMU_ISCCR0, 30, 0, NULL);
	clk = clk_register_gate(NULL, "isccr0_sysclk_en", "isccr0_i2sclk_base",
				CLK_SET_RATE_PARENT,
				pxa_unit->mpmu_base + MPMU_ISCCR0, 31, 0, NULL);
	clk =  mmp_clk_register_factor("isccr0_mn_div", "isccr0_sysclk_en",
				CLK_SET_RATE_PARENT,
				pxa_unit->mpmu_base + MPMU_ISCCR0,
				&isccr1_factor_masks, isccr1_factor_tbl,
				ARRAY_SIZE(isccr1_factor_tbl), NULL);
	clk_set_rate(clk, 2048000);
	clk = clk_register_gate(NULL, "isccr0_i2sclk_en", "isccr0_mn_div",
				CLK_SET_RATE_PARENT,
				pxa_unit->mpmu_base + MPMU_ISCCR0, 29, 0, NULL);
	i2s_clk = clk_register_divider_table(NULL, "isccr0_i2sclk",
				"isccr0_i2sclk_en", CLK_SET_RATE_PARENT,
				pxa_unit->mpmu_base + MPMU_ISCCR0, 27, 2, 0,
				clk_ssp1_ref_table,
				NULL);
	clk_set_rate(i2s_clk, 256000);
	clk = clk_register_mux(NULL, "gssp_func_mux", gssp_parent,
			ARRAY_SIZE(gssp_parent), CLK_SET_RATE_PARENT,
			pxa_unit->apbcp_base + APBC_GCER, 8, 2, 0, NULL);
	clk_set_parent(clk, i2s_clk);
	clk = clk_register_gate(NULL, "gssp_fnclk", "gssp_func_mux",
				CLK_SET_RATE_PARENT,
				pxa_unit->apbcp_base + APBC_GCER, 1, 0, NULL);
	clk = mmp_clk_register_apbc("pxa988-ssp.4", "gssp_fnclk",
				pxa_unit->apbcp_base + APBC_GCER, 10,
				CLK_SET_RATE_PARENT, NULL);
	mmp_clk_add(unit, PXA1L88_CLK_GSSP, clk);

	clk = mmp_clk_register_apbc("swjtag", NULL,
				pxa_unit->apbc_base + APBC_SWJTAG,
				10, 0, NULL);
	mmp_clk_add(unit, PXA1L88_CLK_SWJTAG, clk);

	clk = mmp_clk_register_gate(NULL, "ipc_clk", NULL, 0,
			pxa_unit->apbc_base + APBC_IPC_CLK_RST,
			0x7, 0x3, 0x0, 0, NULL);
	mmp_clk_add(unit, PXA1L88_CLK_IPC_RST, clk);
	clk_prepare_enable(clk);

	clk = mmp_clk_register_gate(NULL, "ripc_clk", NULL, 0,
			pxa_unit->apbcp_base + APBCP_AICER,
			0x7, 0x2, 0x0, 0, NULL);
	mmp_clk_add(unit, PXA1L88_CLK_AICER, clk);
	clk_prepare_enable(clk);

#ifdef CONFIG_CORESIGHT_SUPPORT
	pxa1L88_coresight_clk_init(pxa_unit);
#endif
}

static DEFINE_SPINLOCK(sdh0_lock);
static DEFINE_SPINLOCK(sdh1_lock);
static DEFINE_SPINLOCK(sdh2_lock);
static const char *sdh_parent_names[] = {"pll1_416", "pll1_624"};
static struct mmp_clk_mix_config sdh_mix_config = {
	.reg_info = DEFINE_MIX_REG_INFO(3, 8, 1, 6, 11),
};

static DEFINE_SPINLOCK(disp_lock);
static const char *disp1_parent_names[] = {"pll1_624", "pll1_416"};
static const char *dsi_parent_names[] = {"pll3", "pll2", "pll1_832"};

static const char *disp_axi_parent_names[] = {"pll1_416", "pll1_624", "pll2", "pll2p"};
static int disp_axi_mux_table[] = {0x0, 0x1, 0x2, 0x3};
static struct mmp_clk_mix_config disp_axi_mix_config = {
	.reg_info = DEFINE_MIX_REG_INFO(2, 19, 2, 17, 22),
	.mux_table = disp_axi_mux_table,
};

/* Protect GC 3D register access APMU_GC&APMU_GC2D */
static DEFINE_SPINLOCK(gc_lock);
static DEFINE_SPINLOCK(gc2d_lock);

/* GC 3D */
static const char const *gc3d_parent_names[] = {
	"pll1_416_gate", "pll1_624_gate", "pll2", "pll2p"
};

static struct mmp_clk_mix_clk_table gc3d_pptbl[] = {
	{.rate = 156000000, .parent_index = CLK_PLL1_624, },
	{.rate = 312000000, .parent_index = CLK_PLL1_624, },
	{.rate = 416000000, .parent_index = CLK_PLL1_416, },
	{.rate = 624000000, .parent_index = CLK_PLL1_624, },
};

static struct mmp_clk_mix_config gc3d_mix_config = {
	.reg_info = DEFINE_MIX_REG_INFO(3, 12, 2, 6, 15),
	.table = gc3d_pptbl,
	.table_size = ARRAY_SIZE(gc3d_pptbl),
};

/* GC shader */
static const char const *gcsh_parent_names[] = {
	"pll1_416_gate", "pll1_624_gate", "pll2", "pll2p"
};

static struct mmp_clk_mix_clk_table gcsh_pptbl[] = {
	{.rate = 156000000, .parent_index = CLK_PLL1_624, },
	{.rate = 312000000, .parent_index = CLK_PLL1_624, },
	{.rate = 416000000, .parent_index = CLK_PLL1_416, },
	{.rate = 624000000, .parent_index = CLK_PLL1_624, },
};

static struct mmp_clk_mix_config gcsh_mix_config = {
	.reg_info = DEFINE_MIX_REG_INFO(3, 28, 2, 26, 31),
	.table = gcsh_pptbl,
	.table_size = ARRAY_SIZE(gcsh_pptbl),
};

/* GC3d bus */
static const char const *gc3d_bus_parent_names[] = {
	"pll1_416_gate", "pll1_624_gate", "pll2", "pll2p"
};

static struct mmp_clk_mix_clk_table gc3d_bus_pptbl[] = {
	{.rate = 78000000, .parent_index = CLK_PLL1_624, },
	{.rate = 104000000, .parent_index = CLK_PLL1_416, },
	{.rate = 156000000, .parent_index = CLK_PLL1_624, },
	{.rate = 208000000, .parent_index = CLK_PLL1_416, },
	{.rate = 312000000, .parent_index = CLK_PLL1_624, },
	{.rate = 416000000, .parent_index = CLK_PLL1_416, },
};

static struct mmp_clk_mix_config gc3d_bus_mix_config = {
	.reg_info = DEFINE_MIX_REG_INFO(3, 17, 2, 20, 16),
	.table = gc3d_bus_pptbl,
	.table_size = ARRAY_SIZE(gc3d_bus_pptbl),
};

/* GC 2D */
static const char const *gc2d_parent_names[] = {
	"pll1_416_gate", "pll1_624_gate", "pll2", "pll2p"
};

static struct mmp_clk_mix_clk_table gc2d_pptbl[] = {
	{.rate = 156000000, .parent_index = CLK_PLL1_624, },
	{.rate = 312000000, .parent_index = CLK_PLL1_624, },
	{.rate = 416000000, .parent_index = CLK_PLL1_416, },
	{.rate = 624000000, .parent_index = CLK_PLL1_624, },
};

static struct mmp_clk_mix_config gc2d_mix_config = {
	.reg_info = DEFINE_MIX_REG_INFO(3, 12, 2, 6, 15),
	.table = gc2d_pptbl,
	.table_size = ARRAY_SIZE(gc2d_pptbl),
};

static struct mmp_clk_mix_clk_table gc2d_pptbl_umc[] = {
	{.rate = 156000000, .parent_index = CLK_PLL1_624, .xtc = 0x00055555, },
	{.rate = 312000000, .parent_index = CLK_PLL1_624, .xtc = 0x00055555, },
	{.rate = 416000000, .parent_index = CLK_PLL1_416, .xtc = 0x00055555, },
	{.rate = 624000000, .parent_index = CLK_PLL1_624, .xtc = 0x00066666, },
};

static struct mmp_clk_mix_config gc2d_mix_config_umc = {
	.reg_info = DEFINE_MIX_REG_INFO(3, 12, 2, 6, 15),
	.table = gc2d_pptbl_umc,
	.table_size = ARRAY_SIZE(gc2d_pptbl_umc),
};

/* GC2d bus */
static const char const *gc2d_bus_parent_names[] = {
	"pll1_416_gate", "pll1_624_gate", "pll2", "pll2p"
};

static struct mmp_clk_mix_clk_table gc2d_bus_pptbl[] = {
	{.rate = 78000000, .parent_index = CLK_PLL1_624, },
	{.rate = 104000000, .parent_index = CLK_PLL1_416, },
	{.rate = 156000000, .parent_index = CLK_PLL1_624, },
	{.rate = 208000000, .parent_index = CLK_PLL1_416, },
	{.rate = 312000000, .parent_index = CLK_PLL1_624, },
	{.rate = 416000000, .parent_index = CLK_PLL1_416, },
};

static struct mmp_clk_mix_config gc2d_bus_mix_config = {
	.reg_info = DEFINE_MIX_REG_INFO(3, 17, 2, 20, 16),
	.table = gc2d_bus_pptbl,
	.table_size = ARRAY_SIZE(gc2d_bus_pptbl),
};

/* Protect register access APMU_VPU */
static DEFINE_SPINLOCK(vpu_lock);

/* VPU fclk */
static const char const *vpufclk_parent_names[] = {
	"pll1_416_gate", "pll1_624_gate", "pll2", "pll2p"
};

static struct mmp_clk_mix_clk_table vpufclk_pptbl[] = {
	{.rate = 156000000, .parent_index = CLK_PLL1_624, .xtc = 0x00B04444},
	{.rate = 208000000, .parent_index = CLK_PLL1_416, .xtc = 0x00B04444},
	{.rate = 312000000, .parent_index = CLK_PLL1_624, .xtc = 0x00B05555},
	{.rate = 416000000, .parent_index = CLK_PLL1_416, .xtc = 0x00B06655},
	{.rate = 533000000, .parent_index = CLK_PLL2P, .xtc = 0x00B07777},
};

static struct mmp_clk_mix_config vpufclk_mix_config = {
	.reg_info = DEFINE_MIX_REG_INFO(3, 8, 2, 6, 20),
	.table = vpufclk_pptbl,
	.table_size = ARRAY_SIZE(vpufclk_pptbl),
};

/* vpu bus */
static const char const *vpubus_parent_names[] = {
	"pll1_416_gate", "pll1_624_gate", "pll2", "pll2p"
};

static struct mmp_clk_mix_clk_table vpubus_pptbl[] = {
	{.rate = 156000000, .parent_index = CLK_PLL1_624, },
	{.rate = 208000000, .parent_index = CLK_PLL1_416, },
	{.rate = 312000000, .parent_index = CLK_PLL1_624, },
	{.rate = 416000000, .parent_index = CLK_PLL1_416, },
	{.rate = 533000000, .parent_index = CLK_PLL2P, },
};

static struct mmp_clk_mix_config vpubus_mix_config = {
	.reg_info = DEFINE_MIX_REG_INFO(3, 13, 2, 11, 21),
	.table = vpubus_pptbl,
	.table_size = ARRAY_SIZE(vpubus_pptbl),
};

static void pxa1L88_axi_periph_clk_init(struct pxa1L88_clk_unit *pxa_unit)
{
	struct clk *clk;
	struct mmp_clk_unit *unit = &pxa_unit->unit;

	/* nand flash clock, no one use it, expect to be disabled */
	clk = mmp_clk_register_gate(NULL, "nf_clk", NULL, 0,
				pxa_unit->apmu_base + APMU_NF,
				0x1db, 0x1db, 0x0, 0, NULL);

	/* GEU/AES clock, security will enable it prior to use it */
	clk = mmp_clk_register_gate(NULL, "aes_clk", NULL, 0,
				pxa_unit->apmu_base + APMU_AES,
				0x9, 0x8, 0x0, 0, NULL);

	clk = mmp_clk_register_gate(NULL, "usb_clk", NULL, 0,
				pxa_unit->apmu_base + APMU_USB,
				0x9, 0x9, 0x1, 0, NULL);
	mmp_clk_add(unit, PXA1L88_CLK_USB, clk);

	clk = mmp_clk_register_gate(NULL, "sdh_axi_clk", NULL, 0,
				pxa_unit->apmu_base + APMU_SDH0,
				0x8, 0x8, 0x0, 0, &sdh0_lock);
	mmp_clk_add(unit, PXA1L88_CLK_SDH_AXI, clk);

	sdh_mix_config.reg_info.reg_clk_ctrl = pxa_unit->apmu_base + APMU_SDH0;
	clk = mmp_clk_register_mix(NULL, "sdh0_mix_clk", sdh_parent_names,
				ARRAY_SIZE(sdh_parent_names),
				CLK_SET_RATE_PARENT,
				&sdh_mix_config, &sdh0_lock);
	clk = mmp_clk_register_gate(NULL, "sdh0_clk", "sdh0_mix_clk",
				CLK_SET_RATE_PARENT,
				pxa_unit->apmu_base + APMU_SDH0,
				0x12, 0x12, 0x0, 0, &sdh0_lock);
	mmp_clk_add(unit, PXA1L88_CLK_SDH0, clk);

	sdh_mix_config.reg_info.reg_clk_ctrl = pxa_unit->apmu_base + APMU_SDH1;
	clk = mmp_clk_register_mix(NULL, "sdh1_mix_clk", sdh_parent_names,
				ARRAY_SIZE(sdh_parent_names),
				CLK_SET_RATE_PARENT,
				&sdh_mix_config, &sdh1_lock);
	clk = mmp_clk_register_gate(NULL, "sdh1_clk", "sdh1_mix_clk",
				CLK_SET_RATE_PARENT,
				pxa_unit->apmu_base + APMU_SDH1,
				0x12, 0x12, 0x0, 0, &sdh1_lock);
	mmp_clk_add(unit, PXA1L88_CLK_SDH1, clk);

	sdh_mix_config.reg_info.reg_clk_ctrl = pxa_unit->apmu_base + APMU_SDH2;
	clk = mmp_clk_register_mix(NULL, "sdh2_mix_clk", sdh_parent_names,
				ARRAY_SIZE(sdh_parent_names),
				CLK_SET_RATE_PARENT,
				&sdh_mix_config, &sdh2_lock);
	clk = mmp_clk_register_gate(NULL, "sdh2_clk", "sdh2_mix_clk",
				CLK_SET_RATE_PARENT,
				pxa_unit->apmu_base + APMU_SDH2,
				0x12, 0x12, 0x0, 0, &sdh2_lock);
	mmp_clk_add(unit, PXA1L88_CLK_SDH2, clk);

	gc3d_mix_config.reg_info.reg_clk_ctrl = pxa_unit->apmu_base + APMU_GC;
	gc3d_mix_config.reg_info.reg_clk_xtc =
				pxa_unit->ciu_base + CIU_GPU_XTC;
	clk = mmp_clk_register_mix(NULL, "gc3d_mix_clk",
				(const char **)gc3d_parent_names,
				ARRAY_SIZE(gc3d_parent_names),
				CLK_SET_RATE_PARENTS_ENABLED,
				&gc3d_mix_config, &gc_lock);
	clk = mmp_clk_register_gate(NULL, "gc", "gc3d_mix_clk",
				CLK_SET_RATE_PARENT,
				pxa_unit->apmu_base + APMU_GC,
				(3 << 4), (3 << 4), 0x0, 0, &gc_lock);
	clk_set_rate(clk, 416000000);
	mmp_clk_add(unit, PXA1L88_CLK_GC3D, clk);
	register_mixclk_dcstatinfo(clk);

	gcsh_mix_config.reg_info.reg_clk_ctrl = pxa_unit->apmu_base + APMU_GC;
	clk = mmp_clk_register_mix(NULL, "gcsh_mix_clk",
				(const char **)gcsh_parent_names,
				ARRAY_SIZE(gcsh_parent_names),
				CLK_SET_RATE_PARENTS_ENABLED,
				&gcsh_mix_config, &gc_lock);
	clk = mmp_clk_register_gate(NULL, "gc_shader", "gcsh_mix_clk",
				CLK_SET_RATE_PARENT,
				pxa_unit->apmu_base + APMU_GC,
				(1 << 25), (1 << 25), 0x0, 0, &gc_lock);
	clk_set_rate(clk, 416000000);
	mmp_clk_add(unit, PXA1L88_CLK_GCSH, clk);
	register_mixclk_dcstatinfo(clk);

	gc3d_bus_mix_config.reg_info.reg_clk_ctrl =
				pxa_unit->apmu_base + APMU_GC;
	clk = mmp_clk_register_mix(NULL, "gc3d_bus_mix_clk",
				(const char **)gc3d_bus_parent_names,
				ARRAY_SIZE(gc3d_bus_parent_names),
				CLK_SET_RATE_PARENTS_ENABLED,
				&gc3d_bus_mix_config, &gc_lock);
	clk = mmp_clk_register_gate(NULL, "gc_aclk", "gc3d_bus_mix_clk",
				CLK_SET_RATE_PARENT,
				pxa_unit->apmu_base + APMU_GC,
				(1 << 3), (1 << 3), 0x0, 0, &gc_lock);
	clk_set_rate(clk, 312000000);
	mmp_clk_add(unit, PXA1L88_CLK_GC3DBUS, clk);

	if (cpu_is_pxa1L88_a0c() && (get_foundry() == 2)) {
		gc2d_mix_config_umc.reg_info.reg_clk_ctrl = pxa_unit->apmu_base + APMU_GC2D;
		gc2d_mix_config_umc.reg_info.reg_clk_xtc =
					pxa_unit->ciu_base + CIU_GPU2D_XTC;
		clk = mmp_clk_register_mix(NULL, "gc2d_mix_clk",
					(const char **)gc2d_parent_names,
					ARRAY_SIZE(gc2d_parent_names),
					CLK_SET_RATE_PARENTS_ENABLED,
					&gc2d_mix_config_umc, &gc2d_lock);
	} else {
		gc2d_mix_config.reg_info.reg_clk_ctrl = pxa_unit->apmu_base + APMU_GC2D;
		gc2d_mix_config.reg_info.reg_clk_xtc =
					pxa_unit->ciu_base + CIU_GPU2D_XTC;
		clk = mmp_clk_register_mix(NULL, "gc2d_mix_clk",
					(const char **)gc2d_parent_names,
					ARRAY_SIZE(gc2d_parent_names),
					CLK_SET_RATE_PARENTS_ENABLED,
					&gc2d_mix_config, &gc2d_lock);
	}

	clk = mmp_clk_register_gate(NULL, "gc2d", "gc2d_mix_clk",
				CLK_SET_RATE_PARENT,
				pxa_unit->apmu_base + APMU_GC2D,
				(3 << 4), (3 << 4), 0x0, 0, &gc2d_lock);
	clk_set_rate(clk, 416000000);
	mmp_clk_add(unit, PXA1L88_CLK_GC2D, clk);
	register_mixclk_dcstatinfo(clk);

	gc2d_bus_mix_config.reg_info.reg_clk_ctrl =
				pxa_unit->apmu_base + APMU_GC2D;
	clk = mmp_clk_register_mix(NULL, "gc2d_bus_mix_clk",
				(const char **)gc2d_bus_parent_names,
				ARRAY_SIZE(gc2d_bus_parent_names),
				CLK_SET_RATE_PARENTS_ENABLED,
				&gc2d_bus_mix_config, &gc2d_lock);
	clk = mmp_clk_register_gate(NULL, "gc2d_aclk", "gc2d_bus_mix_clk",
				CLK_SET_RATE_PARENT,
				pxa_unit->apmu_base + APMU_GC2D,
				(1 << 3), (1 << 3), 0x0, 0, &gc2d_lock);
	clk_set_rate(clk, 312000000);
	mmp_clk_add(unit, PXA1L88_CLK_GC2DBUS, clk);

	vpufclk_mix_config.reg_info.reg_clk_ctrl =
			pxa_unit->apmu_base + APMU_VPU;
	vpufclk_mix_config.reg_info.reg_clk_xtc =
				pxa_unit->ciu_base + CIU_VPU_XTC;
	clk = mmp_clk_register_mix(NULL, "vpufunc_mix_clk",
			(const char **)vpufclk_parent_names,
			ARRAY_SIZE(vpufclk_parent_names),
			CLK_SET_RATE_PARENTS_ENABLED,
			&vpufclk_mix_config, &vpu_lock);
#ifdef CONFIG_VPU_DEVFREQ
	__init_comp_devfreq_table(clk, DEVFREQ_VPU_BASE);
#endif
	clk = mmp_clk_register_gate(NULL, "vpu", "vpufunc_mix_clk",
			CLK_SET_RATE_PARENT,
			pxa_unit->apmu_base + APMU_VPU,
			(3 << 4), (3 << 4), 0x0, 0, &vpu_lock);
	clk_set_rate(clk, 416000000);
	mmp_clk_add(unit, PXA1L88_CLK_VPU, clk);
	register_mixclk_dcstatinfo(clk);

	vpubus_mix_config.reg_info.reg_clk_ctrl =
			pxa_unit->apmu_base + APMU_VPU;
	clk = mmp_clk_register_mix(NULL, "vpubus_mix_clk",
			(const char **)vpubus_parent_names,
			ARRAY_SIZE(vpubus_parent_names),
			CLK_SET_RATE_PARENTS_ENABLED,
			&vpubus_mix_config, &vpu_lock);
	clk = mmp_clk_register_gate(NULL, "vpu_aclk", "vpubus_mix_clk",
			CLK_SET_RATE_PARENT,
			pxa_unit->apmu_base + APMU_VPU,
			(1 << 3), (1 << 3), 0x0, 0, &vpu_lock);
	clk_set_rate(clk, 416000000);
	mmp_clk_add(unit, PXA1L88_CLK_VPUBUS, clk);

	clk = mmp_clk_register_gate(NULL, "dsi_esc_clk", NULL, 0,
			pxa_unit->apmu_base + APMU_DSI1,
			0xf, 0xc, 0x0, 0, &disp_lock);
	mmp_clk_add(unit, PXA1L88_CLK_DSI_ESC, clk);

	clk = clk_register_mux(NULL, "disp1_sel_clk", disp1_parent_names,
			ARRAY_SIZE(disp1_parent_names),
			CLK_SET_RATE_PARENT,
			pxa_unit->apmu_base + APMU_DISP1,
			6, 1, 0, &disp_lock);
	mmp_clk_add(unit, PXA1L88_CLK_DISP1, clk);

	clk = clk_register_mux(NULL, "dsi_pll", dsi_parent_names,
			ARRAY_SIZE(dsi_parent_names),
			CLK_SET_RATE_PARENT,
			pxa_unit->apmu_base + APMU_DISP1,
			24, 3, 0, &disp_lock);
	mmp_clk_add(unit, PXA1L88_CLK_DSI_PLL, clk);

	clk = mmp_clk_register_gate(NULL, "dsip1_clk", "disp1_sel_clk",
			CLK_SET_RATE_PARENT,
			pxa_unit->apmu_base + APMU_DISP1,
			0x10, 0x10, 0x0, 0, &disp_lock);
	mmp_clk_add(unit, PXA1L88_CLK_DISP1_EN, clk);

	disp_axi_mix_config.reg_info.reg_clk_ctrl = pxa_unit->apmu_base + APMU_DISP1;
	clk = mmp_clk_register_mix(NULL, "disp_axi_sel_clk", disp_axi_parent_names,
				ARRAY_SIZE(disp_axi_parent_names),
				CLK_SET_RATE_PARENT,
				&disp_axi_mix_config, &disp_lock);
	mmp_clk_add(unit, PXA1L88_CLK_DISP_AXI_SEL_CLK, clk);
	clk_set_rate(clk, 208000000);

	clk = mmp_clk_register_gate(NULL, "disp_axi_clk", "disp_axi_sel_clk",
			CLK_SET_RATE_PARENT,
			pxa_unit->apmu_base + APMU_DISP1,
			0x10009, 0x10009, 0x0, 0, &disp_lock);
	mmp_clk_add(unit, PXA1L88_CLK_DISP_AXI_CLK, clk);

	clk = mmp_clk_register_gate(NULL, "LCDCIHCLK", "disp_axi_clk", 0,
			pxa_unit->apmu_base + APMU_DISP1,
			0x80000226, 0x80000226, 0x0, 0, &disp_lock);
	mmp_clk_add(unit, PXA1L88_CLK_DISP_HCLK, clk);
}


static DEFINE_SPINLOCK(fc_seq_lock);
/* CORE */
static const char *core_parent[] = {
	"pll1_624", "pll1_1248", "pll2", "pll2p", "pll3p",
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
		.parent_name = "pll2p",
		.hw_sel_val = 0x3,
	},
	{
		.parent_name = "pll3p",
		.hw_sel_val = 0x5,
	},
};

/*
 * For HELANLTE:
 * PCLK = AP_CLK_SRC / (CORE_CLK_DIV + 1)
 * BIU_CLK = PCLK / (BIU_CLK_DIV + 1)
 * MC_CLK = PCLK / (MC_CLK_DIV + 1)
 *
 * AP clock source:
 * 0x0 = PLL1 624 MHz
 * 0x1 = PLL1 1248 MHz  or PLL3_CLKOUT
 * (depending on PLL3CR[18])
 * 0x2 = PLL2_CLKOUT
 * 0x3 = PLL2_CLKOUTP
 * 0x5 = PLL3_CLKOUTP
 */
static struct cpu_opt helanLTE_op_array[] = {
	{
		.pclk = 312,
		.pdclk = 156,
		.baclk = 156,
		.ap_clk_sel = 0x0,
	},
	{
		.pclk = 624,
		.pdclk = 312,
		.baclk = 156,
		.ap_clk_sel = 0x0,
	},
	{
		.pclk = 797,
		.pdclk = 398,
		.baclk = 199,
		.ap_clk_sel = 0x3,
	},
	{
		.pclk = 1066,
		.pdclk = 533,
		.baclk = 266,
		.ap_clk_sel = 0x3,
	},
	{
		.pclk = 1183,
		.pdclk = 591,
		.baclk = 295,
		.ap_clk_sel = 0x5,
	},
	{
		.pclk = 1248,
		.pdclk = 624,
		.baclk = 312,
		.ap_clk_sel = 0x1,
	},
	{
		.pclk = 1482,
		.pdclk = 741,
		.baclk = 370,
		.ap_clk_sel = 0x5,
	},
};

static struct cpu_rtcwtc cpu_rtcwtc_tsmc[] = {
	{.max_pclk = 624, .l1_xtc = 0x02222222, .l2_xtc = 0x00002221,},
	{.max_pclk = 1066, .l1_xtc = 0x02666666, .l2_xtc = 0x00006265,},
	{.max_pclk = 1183, .l1_xtc = 0x2AAAAAA, .l2_xtc = 0x0000A2A9,},
	{.max_pclk = 1482, .l1_xtc = 0x02EEEEEE, .l2_xtc = 0x0000E2ED,},
};

static struct cpu_rtcwtc cpu_rtcwtc_tsmc_a0c_1p5g_p6top11[] = {
	{.max_pclk = 624, .l1_xtc = 0x02222222, .l2_xtc = 0x00002221,},
	{.max_pclk = 1066, .l1_xtc = 0x02666666, .l2_xtc = 0x00006265,},
	{.max_pclk = 1482, .l1_xtc = 0x02EEEEEE, .l2_xtc = 0x0000E2ED,},
};

static struct cpu_rtcwtc cpu_rtcwtc_umc_a0c[] = {
	{.max_pclk = 312, .l1_xtc = 0x02222222, .l2_xtc = 0x00002221,},
	{.max_pclk = 800, .l1_xtc = 0x02666666, .l2_xtc = 0x00006265,},
	{.max_pclk = 1248, .l1_xtc = 0x02AAAAAA, .l2_xtc = 0x0000A2A9,},
	{.max_pclk = 1482, .l1_xtc = 0x02EEEEEE, .l2_xtc = 0x0000E2ED,},
};

static struct core_params core_params = {
	.parent_table = core_parent_table,
	.parent_table_size = ARRAY_SIZE(core_parent_table),
	.cpu_opt = helanLTE_op_array,
	.cpu_opt_size = ARRAY_SIZE(helanLTE_op_array),
	.cpu_rtcwtc_table = cpu_rtcwtc_tsmc,
	.cpu_rtcwtc_table_size = ARRAY_SIZE(cpu_rtcwtc_tsmc),
	.bridge_cpurate = 1248,
	.max_cpurate = 1183,
	.dcstat_support = true,
};

/* DDR */
static const char *ddr_parent[] = {
	"pll1_416", "pll1_624", "pll2", "pll2p",
};

/*
 * DDR clock source:
 * 0x0 = PLL1 416 MHz
 * 0x1 = PLL1 624 MHz
 * 0x2 = PLL2_CLKOUT
 * 0x3 = PLL2_CLKOUTP
 */
static struct parents_table ddr_parent_table[] = {
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

static struct ddr_opt ddr_oparray[][3] = {
	[0] = {
		{
			.dclk = 156,
			.ddr_tbl_index = 2,
			.ddr_clk_sel = 0x1,
		},
		{
			.dclk = 312,
			.ddr_tbl_index = 3,
			.ddr_clk_sel = 0x1,
		},
		{
			.dclk = 398,
			.ddr_tbl_index = 4,
			.ddr_clk_sel = 0x3,
		},
	},
	[1] = {
		{
			.dclk = 156,
			.ddr_tbl_index = 2,
			.ddr_clk_sel = 0x1,
		},
		{
			.dclk = 312,
			.ddr_tbl_index = 3,
			.ddr_clk_sel = 0x1,
		},
		{
			.dclk = 533,
			.ddr_tbl_index = 4,
			.ddr_clk_sel = 0x3,
		},
	},

};

/* DDR 400 and 533 uses LV3 */
static unsigned long hwdfc_freq_table[] = {
	312000, 312000, 312000, 533000
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

static struct axi_opt axi_oparray[][3] = {
	[0] = {
		{
			.aclk = 78,
			.axi_clk_sel = 0x1,
		},
		{
			.aclk = 156,
			.axi_clk_sel = 0x1,
		},
		{
			.aclk = 199,
			.axi_clk_sel = 0x3,
		},
	},
	[1] = {
		{
			.aclk = 78,
			.axi_clk_sel = 0x1,
		},
		{
			.aclk = 156,
			.axi_clk_sel = 0x1,
		},
		{
			.aclk = 266,
			.axi_clk_sel = 0x3,
		},
	},
};

static struct axi_params axi_params = {
	.parent_table = axi_parent_table,
	.parent_table_size = ARRAY_SIZE(axi_parent_table),
	.dcstat_support = true,
};

static struct ddr_combclk_relation aclk_dclk_relationtbl_1L88[] = {
	{.dclk_rate = 156000000, .combclk_rate = 156000000},
	{.dclk_rate = 312000000, .combclk_rate = 156000000},
	{.dclk_rate = 398000000, .combclk_rate = 199000000},
	{.dclk_rate = 533000000, .combclk_rate = 266000000},
};

static inline void setup_max_freq(void)
{
	if (!max_freq)
		max_freq = core_params.max_cpurate;

	if (max_freq <= CORE_1p2G)
		max_freq = CORE_1p2G;
	else if (max_freq <= CORE_1p25G)
		max_freq = CORE_1p25G;
	else
		max_freq = CORE_1p5G;

	core_params.max_cpurate = max_freq;
}

static void __init pxa1L88_acpu_init(struct pxa1L88_clk_unit *pxa_unit)
{
	struct clk *clk;
	struct mmp_clk_unit *unit = &pxa_unit->unit;
	int size, ui_foundry;

	core_params.apmu_base = pxa_unit->apmu_base;
	core_params.mpmu_base = pxa_unit->mpmu_base;
	core_params.ciu_base = pxa_unit->ciu_base;
	core_params.pxa_powermode = pxa1x88_powermode;

	ddr_params.apmu_base = pxa_unit->apmu_base;
	ddr_params.mpmu_base = pxa_unit->mpmu_base;
	ddr_params.ddr_opt = ddr_oparray[ddr_mode];
	ddr_params.ddr_opt_size = ARRAY_SIZE(ddr_oparray[ddr_mode]);

	axi_params.apmu_base = pxa_unit->apmu_base;
	axi_params.mpmu_base = pxa_unit->mpmu_base;
	axi_params.axi_opt = axi_oparray[ddr_mode];
	axi_params.axi_opt_size = ARRAY_SIZE(axi_oparray[ddr_mode]);

	if (cpu_is_pxa1L88_a0c()) {
		ui_foundry = get_foundry();
		if (ui_foundry == 2) {
			core_params.cpu_rtcwtc_table = cpu_rtcwtc_umc_a0c;
			core_params.cpu_rtcwtc_table_size =
				ARRAY_SIZE(cpu_rtcwtc_umc_a0c);
		} else if (is_1p5G_chip &&
			  ((get_profile_pxa1L88() == 0) ||
			  ((get_profile_pxa1L88() >= 6) &&
			  (get_profile_pxa1L88() <= 11)))) {
			core_params.cpu_rtcwtc_table =
				cpu_rtcwtc_tsmc_a0c_1p5g_p6top11;
			core_params.cpu_rtcwtc_table_size =
				ARRAY_SIZE(cpu_rtcwtc_tsmc_a0c_1p5g_p6top11);
		}
	}

	clk = mmp_clk_register_core("cpu", core_parent,
				    ARRAY_SIZE(core_parent),
				    CLK_GET_RATE_NOCACHE, HELANX_FC_V1,
				    &fc_seq_lock, &core_params);
	clk_register_clkdev(clk, "cpu", NULL);
	clk_prepare_enable(clk);

	clk = mmp_clk_register_ddr("ddr", ddr_parent,
				   ARRAY_SIZE(ddr_parent),
				   CLK_GET_RATE_NOCACHE, HELANX_FC_V1,
				   &fc_seq_lock, &ddr_params);
	clk_register_clkdev(clk, "ddr", NULL);
	clk_prepare_enable(clk);
	mmp_clk_add(unit, PXA1L88_CLK_DDR, clk);


	clk = mmp_clk_register_axi("axi", axi_parent,
				  ARRAY_SIZE(axi_parent),
				  CLK_GET_RATE_NOCACHE,	HELANX_FC_V1,
				  &fc_seq_lock, &axi_params);
	clk_register_clkdev(clk, "axi", NULL);
	clk_prepare_enable(clk);
	mmp_clk_add(unit, PXA1L88_CLK_AXI, clk);

	size = axi_params.axi_opt_size;
	register_clk_bind2ddr(clk, axi_params.axi_opt[size - 1].aclk * MHZ,
			      aclk_dclk_relationtbl_1L88,
			      ARRAY_SIZE(aclk_dclk_relationtbl_1L88));
}

static void __init pxa1L88_misc_init(struct pxa1L88_clk_unit *pxa_unit)
{
	unsigned int regval = 0;

	/* DE suggest:enable SQU MP3 playback sleep mode */
	regval = readl(pxa_unit->apmu_base + APMU_SQU_CLK_GATE_CTRL);
	regval |= (1 << 30);
	writel(regval, pxa_unit->apmu_base + APMU_SQU_CLK_GATE_CTRL);

	/* enable MC4 and AXI fabric dynamic clk gating */
	regval = readl(pxa_unit->ciu_base + CIU_MC_CONF);
	/* disable cp fabric clk gating */
	regval &= ~(1 << 16);
	/* enable dclk gating */
	regval &= ~(1 << 19);
	/* enable 1x2 fabric AXI clock dynamic gating */
	regval |= (1 << 29) | (1 << 30);
	regval |= (0xff << 8) |		/* MCK4 P0~P7 */
		(1 << 17) | (1 << 18) |	/* Fabric 0 */
		(1 << 20) | (1 << 21) |	/* VPU fabric */
		(1 << 26) | (1 << 27);	/* Fabric 0/1 */
	writel(regval, pxa_unit->ciu_base + CIU_MC_CONF);

	/* init GC rtc/wtc settings */
	writel(0x00011111, pxa_unit->ciu_base + CIU_GPU_XTC);
	writel(0x00055555, pxa_unit->ciu_base + CIU_GPU2D_XTC);
}

static void __init pxa1L88_clk_init(struct device_node *np)
{
	struct pxa1L88_clk_unit *pxa_unit;

	pxa_unit = kzalloc(sizeof(*pxa_unit), GFP_KERNEL);
	if (!pxa_unit) {
		pr_err("failed to allocate memory for pxa1L88 clock unit\n");
		return;
	}

	pxa_unit->mpmu_base = of_iomap(np, 0);
	if (!pxa_unit->mpmu_base) {
		pr_err("failed to map mpmu registers\n");
		return;
	}

	pxa_unit->apmu_base = of_iomap(np, 1);
	if (!pxa_unit->mpmu_base) {
		pr_err("failed to map apmu registers\n");
		return;
	}

	pxa_unit->apbc_base = of_iomap(np, 2);
	if (!pxa_unit->apbc_base) {
		pr_err("failed to map apbc registers\n");
		return;
	}

	pxa_unit->apbcp_base = of_iomap(np, 3);
	if (!pxa_unit->mpmu_base) {
		pr_err("failed to map apbcp registers\n");
		return;
	}

	pxa_unit->apbs_base = of_iomap(np, 4);
	if (!pxa_unit->apbs_base) {
		pr_err("failed to map apbs registers\n");
		return;
	}

	pxa_unit->ciu_base = of_iomap(np, 5);
	if (!pxa_unit->ciu_base) {
		pr_err("failed to map ciu registers\n");
		return;
	}

	mmp_clk_init(np, &pxa_unit->unit, PXA1L88_NR_CLKS);
	/*
	 * FIXME: hack ddr_mode to 1 as uboot cmdline hasn't
	 *       been passed to kernel
	 */
	ddr_mode = 1;

	setup_max_freq();

	pxa1L88_misc_init(pxa_unit);
	pxa1L88_pll_init(pxa_unit);
	pxa1L88_acpu_init(pxa_unit);

	pxa1L88_apb_periph_clk_init(pxa_unit);

	pxa1L88_axi_periph_clk_init(pxa_unit);

#if defined(CONFIG_PXA_DVFS) && defined(CONFIG_CPU_PXA988)
	setup_pxa1L88_dvfs_platinfo();
#endif

	global_pxa_unit = pxa_unit;
}
CLK_OF_DECLARE(pxa1L88_clk, "marvell,pxa1L88-clock", pxa1L88_clk_init);

#ifdef CONFIG_CPU_PXA988
#ifdef CONFIG_DEBUG_FS
static struct dentry *stat;
CLK_DCSTAT_OPS(global_pxa_unit->unit.clk_table[PXA1L88_CLK_DDR], ddr);
CLK_DCSTAT_OPS(global_pxa_unit->unit.clk_table[PXA1L88_CLK_AXI], axi);
CLK_DCSTAT_OPS(global_pxa_unit->unit.clk_table[PXA1L88_CLK_GC3D], gc3d);
CLK_DCSTAT_OPS(global_pxa_unit->unit.clk_table[PXA1L88_CLK_GC2D], gc2d);
CLK_DCSTAT_OPS(global_pxa_unit->unit.clk_table[PXA1L88_CLK_GCSH], gcsh);
CLK_DCSTAT_OPS(global_pxa_unit->unit.clk_table[PXA1L88_CLK_VPU], vpu);


static ssize_t pxa1L88_clk_stats_read(struct file *filp,
	char __user *buffer, size_t count, loff_t *ppos)
{
	char *buf;
	int len = 0, ret = -EINVAL;
	unsigned int reg, size = PAGE_SIZE - 1;
	void __iomem *apbc_base, *apbcp_base, *mpmu_base, *apmu_base;

	mpmu_base = global_pxa_unit->mpmu_base;
	if (!mpmu_base) {
		pr_err("failed to map mpmu registers\n");
		return ret;
	}

	apmu_base = global_pxa_unit->apmu_base;
	if (!apmu_base) {
		pr_err("failed to map apmu registers\n");
		return ret;
	}

	apbc_base = global_pxa_unit->apbc_base;
	if (!apbc_base) {
		pr_err("failed to map apbc registers\n");
		return ret;
	}

	apbcp_base = global_pxa_unit->apbcp_base;
	if (!apbcp_base) {
		pr_err("failed to map apbcp registers\n");
		return ret;
	}

	buf = (char *)__get_free_pages(GFP_NOIO, 0);
	if (!buf)
		return -ENOMEM;

	len += snprintf(buf + len, size,
		       "|---------------|-------|\n|%14s\t|%s|\n"
		       "|---------------|-------|\n", "Clock Name", " Status");

	/* PMU */
	reg = __raw_readl(mpmu_base + MPMU_PLL2CR);
	if (((reg & (3 << 8)) >> 8) == 2)
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "PLL2", "off");
	else
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "PLL2", "on");

	reg = __raw_readl(mpmu_base + MPMU_PLL3CR);
	if (((reg & (3 << 18)) >> 18) == 0)
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "PLL3", "off");
	else
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "PLL3", "on");

	reg = __raw_readl(apmu_base + APMU_SDH0);
	if ((((reg & (1 << 3)) >> 3) == 1) && (((reg & (1 << 0)) >> 0) == 1))
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "SDH ACLK", "on");
	else
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "SDH ACLK", "off");

	if ((((reg & (1 << 4)) >> 4) == 1) && (((reg & (1 << 1)) >> 1) == 1))
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "SDH0 FCLK", "on");
	else
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "SDH0 FCLK", "off");

	reg = __raw_readl(apmu_base + APMU_SDH1);
	if ((((reg & (1 << 4)) >> 4) == 1) && (((reg & (1 << 1)) >> 1) == 1))
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "SDH1 FCLK", "on");
	else
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "SDH1 FCLK", "off");

	reg = __raw_readl(apmu_base + APMU_SDH2);
	if ((((reg & (1 << 4)) >> 4) == 1) && (((reg & (1 << 1)) >> 1) == 1))
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "SDH2 FCLK", "on");
	else
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "SDH2 FCLK", "off");

	reg = __raw_readl(apmu_base + APMU_VPU);
	if (((reg & (3 << 4)) >> 4) == 3)
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "VPU FCLK", "on");
	else
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "VPU FCLK", "off");
	if (((reg & (1 << 3)) >> 3) == 1)
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "VPU ACLK", "on");
	else
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "VPU ACLK", "off");

	reg = __raw_readl(apmu_base + APMU_GC);
	if (((reg & (3 << 4)) >> 4) == 3)
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "GC FCLK", "on");
	else
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "GC FCLK", "off");
	if (((reg & (1 << 3)) >> 3) == 1)
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "GC ACLK", "on");
	else
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "GC ACLK", "off");
	if (((reg & (1 << 25)) >> 25) == 1)
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "GC SHADERCLK", "on");
	else
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "GC SHADERCLK", "off");

	reg = __raw_readl(apmu_base + APMU_GC2D);
	if (((reg & (3 << 4)) >> 4) == 3)
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "GC2D FCLK", "on");
	else
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "GC2D FCLK", "off");
	if (((reg & (1 << 3)) >> 3) == 1)
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "GC2D ACLK", "on");
	else
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "GC2D ACLK", "off");

	reg = __raw_readl(apmu_base + APMU_DISP1);
	if ((((reg & (1 << 1)) >> 1) == 1) && (((reg & (1 << 4)) >> 4) == 1))
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "LCD FCLK", "on");
	else
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "LCD FCLK", "off");
	if (((reg & (1 << 3)) >> 3) == 1)
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "LCD ACLK", "on");
	else
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "LCD ACLK", "off");
	if ((((reg & (1 << 5)) >> 5) == 1) && (((reg & (1 << 2)) >> 2) == 1))
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "LCD HCLK", "on");
	else
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "LCD HCLK", "off");

	reg = __raw_readl(apmu_base + APMU_DSI1);
	if (((reg & (0xf << 2)) >> 2) == 0xf)
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "DSI PHYCLK", "on");
	else
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "DSI PHYCLK", "off");
	reg = __raw_readl(apmu_base + APMU_ISPDXO);
	if ((reg & 0xf03) == 0xf03)
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "ISP_DXO CLK", "on");
	else
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "ISP_DXO CLK", "off");

	reg = __raw_readl(apmu_base + APMU_USB);
	if ((reg & 0x9) == 0x9)
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "USB ACLK", "on");
	else
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "USB ACLK", "off");

	reg = __raw_readl(apmu_base + APMU_CCIC0);
	if ((((reg & (1 << 4)) >> 4) == 1) && (((reg & (1 << 1)) >> 1) == 1))
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "CCIC0 FCLK", "on");
	else
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "CCIC0 FCLK", "off");

	if ((((reg & (1 << 3)) >> 3) == 1) && (((reg & (1 << 0)) >> 0) == 1))
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "CCIC0 ACLK", "on");
	else
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "CCIC0 ACLK", "off");
	if ((((reg & (1 << 5)) >> 5) == 1) && (((reg & (1 << 2)) >> 2) == 1))
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "CCIC0 PHYCLK", "on");
	else
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "CCIC0 PHYCLK", "off");

	reg = __raw_readl(apmu_base + APMU_TRACE);
	if (((reg & (1 << 3)) >> 3) == 1)
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "DBG CLK", "on");
	else
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "DBG CLK", "off");
	if (((reg & (1 << 4)) >> 4) == 1)
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "TRACE CLK", "on");
	else
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "TRACE CLK", "off");

	reg = __raw_readl(apmu_base + APMU_AES);
	if ((reg & 0x9) == 0x8)
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "AES CLK", "on");
	else
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "AES CLK", "off");

	reg = __raw_readl(apmu_base + APMU_NF);
	if ((reg & 0x19b) == 0x19b)
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "DFC CLK", "on");
	else
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "DFC CLK", "off");

	/* APB */
	reg = __raw_readl(apbc_base + APBC_TWSI0);
	if ((reg & 0x5) == 0x1)
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "TWSI0 CLK", "on");
	else
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "TWSI0 CLK", "off");

	reg = __raw_readl(apbcp_base + APBCP_TWSI2);
	if ((reg & 0x5) == 0x1)
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "TWSI2 CLK", "on");
	else
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "TWSI2 CLK", "off");

	reg = __raw_readl(apbc_base + APBC_GPIO);
	if ((reg & 0x5) == 0x1)
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "GPIO CLK", "on");
	else
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "GPIO CLK", "off");

	reg = __raw_readl(apbc_base + APBC_KPC);
	if ((reg & 0x5) == 0x1)
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "KPC CLK", "on");
	else
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "KPC CLK", "off");

	reg = __raw_readl(apbc_base + APBC_RTC);
	if ((reg & 0x5) == 0x1)
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "RTC CLK", "on");
	else
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "RTC CLK", "off");

	reg = __raw_readl(apbc_base + APBC_UART0);
	if ((reg & 0x5) == 0x1)
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "UART0 CLK", "on");
	else
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "UART0 CLK", "off");

	reg = __raw_readl(apbc_base + APBC_UART1);
	if ((reg & 0x5) == 0x1)
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "UART1 CLK", "on");
	else
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "UART1 CLK", "off");

	reg = __raw_readl(apbc_base + APBC_SSP0);
	if ((reg & 0x5) == 0x1)
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "SSP0 CLK", "on");
	else
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "SSP0 CLK", "off");

	reg = __raw_readl(apbc_base + APBC_SSP1);
	if ((reg & 0x5) == 0x1)
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "SSP1 CLK", "on");
	else
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "SSP1 CLK", "off");

	reg = __raw_readl(apbcp_base + APBC_GCER);
	if ((reg & 0x5) == 0x1)
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "SSP.4 CLK", "on");
	else
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "SSP.4 CLK", "off");

	reg = __raw_readl(apbc_base + APBC_DROTS);
	if ((reg & 0x5) == 0x1)
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "THERMAL CLK", "on");
	else
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "THERMAL CLK", "off");

	reg = __raw_readl(apbc_base + APBC_PWM0);
	if ((reg & 0x1) == 0x1) {
		if ((reg & 0x6) == 0x2)
			len += snprintf(buf + len, size,
					"|%14s\t|%5s\t|\n", "PWM0 CLK", "on");
		else
			len += snprintf(buf + len, size,
					"|%14s\t|%5s\t|\n", "PWM0 CLK", "off");
		reg = __raw_readl(apbc_base + APBC_PWM1);
		if ((reg & 0x6) == 0x2)
			len += snprintf(buf + len, size,
					"|%14s\t|%5s\t|\n", "PWM1 CLK", "on");
		else
			len += snprintf(buf + len, size,
					"|%14s\t|%5s\t|\n", "PWM1 CLK", "off");
	} else {
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "PWM0 CLK", "off");
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "PWM1 CLK", "off");
	}

	reg = __raw_readl(apbc_base + APBC_PWM2);
	if ((reg & 0x1) == 0x1) {
		if ((reg & 0x6) == 0x2)
			len += snprintf(buf + len, size,
					"|%14s\t|%5s\t|\n", "PWM2 CLK", "on");
		else
			len += snprintf(buf + len, size,
					"|%14s\t|%5s\t|\n", "PWM2 CLK", "off");
		reg = __raw_readl(apbc_base + APBC_PWM3);
		if ((reg & 0x6) == 0x2)
			len += snprintf(buf + len, size,
					"|%14s\t|%5s\t|\n", "PWM3 CLK", "on");
		else
			len += snprintf(buf + len, size,
					"|%14s\t|%5s\t|\n", "PWM3 CLK", "off");
	} else {
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "PWM2 CLK", "off");
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "PWM3 CLK", "off");
	}

	reg = __raw_readl(apbc_base + APBC_IPC_CLK_RST);
	if ((reg & 0x5) == 0x1)
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "IPC CLK", "on");
	else
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "IPC CLK", "off");

	reg = __raw_readl(apbcp_base + APBCP_AICER);
	if ((reg & 0x5) == 0x1)
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "RIPC CLK", "on");
	else
		len += snprintf(buf + len, size,
				"|%14s\t|%5s\t|\n", "RIPC CLK", "off");

	len += snprintf(buf + len, size, "|---------------|-------|\n\n");

	ret = simple_read_from_buffer(buffer, count, ppos, buf, len);
	free_pages((unsigned long)buf, 0);
	return ret;
}

static const struct file_operations pxa1L88_clk_stats_ops = {
	.owner = THIS_MODULE,
	.read = pxa1L88_clk_stats_read,
};

static int __init __init_pxa1L88_dcstat_debugfs_node(void)
{
	struct dentry *cpu_dc_stat = NULL, *ddr_dc_stat = NULL;
	struct dentry *axi_dc_stat = NULL;
	struct dentry *gc3d_dc_stat = NULL, *vpu_dc_stat = NULL;
	struct dentry *gc2d_dc_stat = NULL, *gcsh_dc_stat = NULL;
	struct dentry *clock_status = NULL;

	if (!cpu_is_pxa1L88())
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

	gc3d_dc_stat = clk_dcstat_file_create("gc3d_core0_dc_stat",
		stat, &gc3d_dc_ops);
	if (!gc3d_dc_stat)
		goto err_gc3d_dc_stat;

	gc2d_dc_stat = clk_dcstat_file_create("gc2d_core0_dc_stat",
		stat, &gc2d_dc_ops);
	if (!gc2d_dc_stat)
		goto err_gc2d_dc_stat;

	gcsh_dc_stat = clk_dcstat_file_create("gcsh_core0_dc_stat",
		stat, &gcsh_dc_ops);
	if (!gcsh_dc_stat)
		goto err_gcsh_dc_stat;

	vpu_dc_stat = clk_dcstat_file_create("vpu_dc_stat",
		stat, &vpu_dc_ops);
	if (!vpu_dc_stat)
		goto err_vpu_dc_stat;

	clock_status = debugfs_create_file("clock_status", 0444,
			pxa, NULL, &pxa1L88_clk_stats_ops);
	if (!clock_status)
		goto err_clk_stats;

	return 0;

err_clk_stats:
	debugfs_remove(vpu_dc_stat);
err_vpu_dc_stat:
	debugfs_remove(gcsh_dc_stat);
err_gcsh_dc_stat:
	debugfs_remove(gc2d_dc_stat);
err_gc2d_dc_stat:
	debugfs_remove(gc3d_dc_stat);
err_gc3d_dc_stat:
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
 * use arch_initcall init the pxa1L88 dcstat node.
 */
arch_initcall(__init_pxa1L88_dcstat_debugfs_node);
#endif
#endif
