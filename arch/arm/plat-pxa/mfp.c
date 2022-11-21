/*
 * linux/arch/arm/plat-pxa/mfp.c
 *
 *   Multi-Function Pin Support
 *
 * Copyright (C) 2007 Marvell Internation Ltd.
 *
 * 2007-08-21: eric miao <eric.miao@marvell.com>
 *             initial version
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>

#ifdef CONFIG_ARCH_MMP
#include <mach/cputype.h>
#elif defined(CONFIG_ARCH_PXA)
#include <mach/hardware.h>
#endif

#ifdef CONFIG_SEC_GPIO_DVS
#include <linux/secgpio_dvs.h>
#include <linux/platform_device.h>
#include <linux/gpio-pxa.h>
#include <asm/gpio.h>
#endif

#include <plat/mfp.h>

#define MFPR_SIZE	(PAGE_SIZE)
#define APB_VIRT_BASE       IOMEM(0xfe000000)
#define MFPR_VIRT_BASE  (APB_VIRT_BASE + 0x1e000)
/* MFPR register bit definitions */
#define MFPR_PULL_SEL		(0x1 << 15)
#define MFPR_PULLUP_EN		(0x1 << 14)
#define MFPR_PULLDOWN_EN	(0x1 << 13)
#ifndef CONFIG_CPU_PXA988
#define MFPR_SLEEP_SEL		(0x1 << 9)
#else
#define MFPR_SLEEP_SEL		((0x1 << 9) | (0x1 << 3))
#endif
#define MFPR_SLEEP_OE_N		(0x1 << 7)
#define MFPR_EDGE_CLEAR		(0x1 << 6)
#define MFPR_EDGE_FALL_EN	(0x1 << 5)
#define MFPR_EDGE_RISE_EN	(0x1 << 4)
#define MFPR_EDGE_MASK		(0x7 << 4)

#define MFPR_SLEEP_DATA(x)	(((x) & 0x1) << 8)
#define MFPR_DRIVE(x)		(((x) & 0x7) << 10)
#define MFPR_AF_SEL(x)		(((x) & 0x7) << 0)

#define MFPR_EDGE_NONE		(MFPR_EDGE_CLEAR)
#define MFPR_EDGE_RISE		(MFPR_EDGE_RISE_EN)
#define MFPR_EDGE_FALL		(MFPR_EDGE_FALL_EN)
#define MFPR_EDGE_BOTH		(MFPR_EDGE_RISE | MFPR_EDGE_FALL)

static struct mfp_addr_map pxa988_addr_map[] __initdata = {

	MFP_ADDR_X(GPIO0, GPIO54, 0xdc),
	MFP_ADDR_X(GPIO67, GPIO98, 0x1b8),
	MFP_ADDR_X(GPIO100, GPIO109, 0x238),
	MFP_ADDR_X(GPIO110, GPIO116, 0x298),

	MFP_ADDR(DF_IO0, 0x40),
	MFP_ADDR(DF_IO1, 0x3c),
	MFP_ADDR(DF_IO2, 0x38),
	MFP_ADDR(DF_IO3, 0x34),
	MFP_ADDR(DF_IO4, 0x30),
	MFP_ADDR(DF_IO5, 0x2c),
	MFP_ADDR(DF_IO6, 0x28),
	MFP_ADDR(DF_IO7, 0x24),
	MFP_ADDR(DF_IO8, 0x20),
	MFP_ADDR(DF_IO9, 0x1c),
	MFP_ADDR(DF_IO10, 0x18),
	MFP_ADDR(DF_IO11, 0x14),
	MFP_ADDR(DF_IO12, 0x10),
	MFP_ADDR(DF_IO13, 0xc),
	MFP_ADDR(DF_IO14, 0x8),
	MFP_ADDR(DF_IO15, 0x4),

	MFP_ADDR(DF_nCS0_SM_nCS2, 0x44),
	MFP_ADDR(DF_nCS1_SM_nCS3, 0x48),
	MFP_ADDR(SM_nCS0, 0x4c),
	MFP_ADDR(SM_nCS1, 0x50),
	MFP_ADDR(DF_WEn, 0x54),
	MFP_ADDR(DF_REn, 0x58),
	MFP_ADDR(DF_CLE_SM_OEn, 0x5c),
	MFP_ADDR(DF_ALE_SM_WEn, 0x60),
	MFP_ADDR(SM_SCLK, 0x64),
	MFP_ADDR(DF_RDY0, 0x68),
	MFP_ADDR(SM_BE0, 0x6c),
	MFP_ADDR(SM_BE1, 0x70),
	MFP_ADDR(SM_ADV, 0x74),
	MFP_ADDR(DF_RDY1, 0x78),
	MFP_ADDR(SM_ADVMUX, 0x7c),
	MFP_ADDR(SM_RDY, 0x80),
	MFP_ADDR(ANT_SW4, 0x26c),

	MFP_ADDR_X(MMC1_DAT7, MMC1_WP, 0x84),

	MFP_ADDR(GPIO124, 0xd0),
	MFP_ADDR(VCXO_REQ, 0xd4),
	MFP_ADDR(VCXO_OUT, 0xd8),

	MFP_ADDR(CLK_REQ, 0xcc),
#ifdef CONFIG_SEC_GPIO_DVS
	MFP_ADDR(PRI_TDI, 0xB4),
	MFP_ADDR(PRI_TMS, 0xB8),
	MFP_ADDR(PRI_TCK, 0xBC),
	MFP_ADDR(PRI_TDO, 0xC0),
	MFP_ADDR(SLAVE_RESET_OUT, 0xC8),
#endif
	MFP_ADDR_END,
};

/*
 * Table that determines the low power modes outputs, with actual settings
 * used in parentheses for don't-care values. Except for the float output,
 * the configured driven and pulled levels match, so if there is a need for
 * non-LPM pulled output, the same configuration could probably be used.
 *
 * Output value  sleep_oe_n  sleep_data  pullup_en  pulldown_en  pull_sel
 *                 (bit 7)    (bit 8)    (bit 14)     (bit 13)   (bit 15)
 *
 * Input            0          X(0)        X(0)        X(0)       0
 * Drive 0          0          0           0           X(1)       0
 * Drive 1          0          1           X(1)        0	  0
 * Pull hi (1)      1          X(1)        1           0	  0
 * Pull lo (0)      1          X(0)        0           1	  0
 * Z (float)        1          X(0)        0           0	  0
 */
#ifndef CONFIG_CPU_PXA988
#define MFPR_LPM_INPUT		(0)
#define MFPR_LPM_NONE		MFPR_LPM_INPUT
#define MFPR_LPM_DRIVE_LOW	(MFPR_SLEEP_DATA(0) | MFPR_PULLDOWN_EN)
#define MFPR_LPM_DRIVE_HIGH    	(MFPR_SLEEP_DATA(1) | MFPR_PULLUP_EN)
#define MFPR_LPM_PULL_LOW      	(MFPR_LPM_DRIVE_LOW  | MFPR_SLEEP_OE_N)
#define MFPR_LPM_PULL_HIGH     	(MFPR_LPM_DRIVE_HIGH | MFPR_SLEEP_OE_N)
#define MFPR_LPM_FLOAT         	(MFPR_SLEEP_OE_N)
#define MFPR_LPM_MASK		(0xe080)
#else /* CONFIG_CPU_PXA988 */
/*
 * Output value sleep_oe_n sleep_data sleep_sel
 *               (bit 7)    (bit 8)   (bit 9/3)
 * None            X(0)       X(0)      00
 * Drive 0         0          0         11
 * Drive 1         0          1         11
 * Z (float)       1          X(0)      11
 */
#define MFPR_LPM_NONE		0
#define MFPR_LPM_DRIVE_LOW	(MFPR_SLEEP_SEL | MFPR_SLEEP_DATA(0))
#define MFPR_LPM_DRIVE_HIGH	(MFPR_SLEEP_SEL | MFPR_SLEEP_DATA(1))
#define MFPR_LPM_PULL_LOW	0 /* Not supported */
#define MFPR_LPM_PULL_HIGH	0 /* Not supported */
#define MFPR_LPM_FLOAT		(MFPR_SLEEP_SEL | MFPR_SLEEP_OE_N)
#define MFPR_LPM_INPUT		MFPR_LPM_FLOAT
#define MFPR_LPM_MASK		(0x0388)
#endif /* CONFIG_CPU_PXA988 */

/*
 * The pullup and pulldown state of the MFP pin at run mode is by default
 * determined by the selected alternate function. In case that some buggy
 * devices need to override this default behavior,  the definitions below
 * indicates the setting of corresponding MFPR bits
 *
 * Definition       pull_sel  pullup_en  pulldown_en
 * MFPR_PULL_NONE       0         0        0
 * MFPR_PULL_LOW        1         0        1
 * MFPR_PULL_HIGH       1         1        0
 * MFPR_PULL_BOTH       1         1        1
 * MFPR_PULL_FLOAT	1         0        0
 */
#define MFPR_PULL_NONE		(0)
#define MFPR_PULL_LOW		(MFPR_PULL_SEL | MFPR_PULLDOWN_EN)
#define MFPR_PULL_BOTH		(MFPR_PULL_LOW | MFPR_PULLUP_EN)
#define MFPR_PULL_HIGH		(MFPR_PULL_SEL | MFPR_PULLUP_EN)
#define MFPR_PULL_FLOAT		(MFPR_PULL_SEL)

/* mfp_spin_lock is used to ensure that MFP register configuration
 * (most likely a read-modify-write operation) is atomic, and that
 * mfp_table[] is consistent
 */
static DEFINE_SPINLOCK(mfp_spin_lock);

static void __iomem *mfpr_mmio_base;

struct mfp_pin {
	unsigned long	config;		/* -1 for not configured */
	unsigned long	mfpr_off;	/* MFPRxx Register offset */
	unsigned long	mfpr_run;	/* Run-Mode Register Value */
	unsigned long	mfpr_lpm;	/* Low Power Mode Register Value */
};

static struct mfp_pin mfp_table[MFP_PIN_MAX];

/* mapping of MFP_LPM_* definitions to MFPR_LPM_* register bits */
static const unsigned long mfpr_lpm[] = {
	MFPR_LPM_NONE,
	MFPR_LPM_DRIVE_LOW,
	MFPR_LPM_DRIVE_HIGH,
	MFPR_LPM_PULL_LOW,
	MFPR_LPM_PULL_HIGH,
	MFPR_LPM_FLOAT,
	MFPR_LPM_INPUT,
};

/* mapping of MFP_PULL_* definitions to MFPR_PULL_* register bits */
static const unsigned long mfpr_pull[] = {
	MFPR_PULL_NONE,
	MFPR_PULL_LOW,
	MFPR_PULL_HIGH,
	MFPR_PULL_BOTH,
	MFPR_PULL_FLOAT,
};

/* mapping of MFP_LPM_EDGE_* definitions to MFPR_EDGE_* register bits */
static const unsigned long mfpr_edge[] = {
	MFPR_EDGE_NONE,
	MFPR_EDGE_RISE,
	MFPR_EDGE_FALL,
	MFPR_EDGE_BOTH,
};

#define mfpr_readl(off)			\
	__raw_readl(mfpr_mmio_base + (off))

#define mfpr_writel(off, val)		\
	__raw_writel(val, mfpr_mmio_base + (off))

#define mfp_configured(p)	((p)->config != -1)

/*
 * perform a read-back of any valid MFPR register to make sure the
 * previous writings are finished
 */
static unsigned long mfpr_off_readback;
#define mfpr_sync()	(void)__raw_readl(mfpr_mmio_base + mfpr_off_readback)

static inline void __mfp_config_run(struct mfp_pin *p)
{
	if (mfp_configured(p))
		mfpr_writel(p->mfpr_off, p->mfpr_run);
}

static inline void __mfp_config_lpm(struct mfp_pin *p)
{
	if (mfp_configured(p)) {
		unsigned long mfpr_clr = (p->mfpr_run & ~MFPR_EDGE_BOTH) | MFPR_EDGE_CLEAR;
		if (mfpr_clr != p->mfpr_run)
			mfpr_writel(p->mfpr_off, mfpr_clr);
		if (p->mfpr_lpm != mfpr_clr)
			mfpr_writel(p->mfpr_off, p->mfpr_lpm);
	}
}

void mfp_config(unsigned long *mfp_cfgs, int num)
{
	unsigned long flags;
	int i, drv_b11 = 0, no_lpm = 0;

#ifdef CONFIG_ARCH_MMP
	if (cpu_is_pxa910() || cpu_is_pxa988() || cpu_is_pxa986() ||
		cpu_is_mmp2() || cpu_is_mmp3())
		drv_b11 = 1;
	if (cpu_is_pxa168() || cpu_is_pxa910())
		no_lpm = 1;
#elif defined(CONFIG_ARCH_PXA)
	if (cpu_is_pxa95x())
		drv_b11 = 1;
#endif
	spin_lock_irqsave(&mfp_spin_lock, flags);

	for (i = 0; i < num; i++, mfp_cfgs++) {
		unsigned long tmp, c = *mfp_cfgs;
		struct mfp_pin *p;
		int pin, af, drv, lpm, edge, pull;

		pin = MFP_PIN(c);
		BUG_ON(pin >= MFP_PIN_MAX);
		p = &mfp_table[pin];

		af  = MFP_AF(c);
		drv = MFP_DS(c);
		lpm = MFP_LPM_STATE(c);
		edge = MFP_LPM_EDGE(c);
		pull = MFP_PULL(c);
		if (drv_b11)
			drv = drv << 1;
		if (no_lpm)
			lpm = 0;

		tmp = MFPR_AF_SEL(af) | MFPR_DRIVE(drv);
		tmp |= mfpr_pull[pull] | mfpr_lpm[lpm] | mfpr_edge[edge];
		p->mfpr_run = tmp;
		p->mfpr_lpm = p->mfpr_run;

		p->config = c; __mfp_config_run(p);
	}

	mfpr_sync();
	spin_unlock_irqrestore(&mfp_spin_lock, flags);
}

unsigned long mfp_read(int mfp)
{
	unsigned long val, flags;

	BUG_ON(mfp < 0 || mfp >= MFP_PIN_MAX);

	spin_lock_irqsave(&mfp_spin_lock, flags);
	val = mfpr_readl(mfp_table[mfp].mfpr_off);
	spin_unlock_irqrestore(&mfp_spin_lock, flags);

	return val;
}

void mfp_write(int mfp, unsigned long val)
{
	unsigned long flags;

	BUG_ON(mfp < 0 || mfp >= MFP_PIN_MAX);

	spin_lock_irqsave(&mfp_spin_lock, flags);
	mfpr_writel(mfp_table[mfp].mfpr_off, val);
	mfpr_sync();
	spin_unlock_irqrestore(&mfp_spin_lock, flags);
}

void __init mfp_init_base(void __iomem *mfpr_base)
{
	int i;

	/* initialize the table with default - unconfigured */
	for (i = 0; i < ARRAY_SIZE(mfp_table); i++)
		mfp_table[i].config = -1;

	mfpr_mmio_base = mfpr_base;
}

#ifdef CONFIG_SEC_GPIO_DVS
struct dvs_mfpr_offset {
	unsigned long offset;
	int gpio;
	int func;
};
static struct dvs_mfpr_offset dvs_offset[] = {
	{MFP_PIN(MFP_PIN_GPIO0), 0, 0},
	{MFP_PIN(MFP_PIN_GPIO1), 1, 0},
	{MFP_PIN(MFP_PIN_GPIO2), 2, 0},
	{MFP_PIN(MFP_PIN_GPIO3), 3, 0},
	{MFP_PIN(MFP_PIN_GPIO4), 4, 0},
	{MFP_PIN(MFP_PIN_GPIO5), 5, 0},
	{MFP_PIN(MFP_PIN_GPIO6), 6, 0},
	{MFP_PIN(MFP_PIN_GPIO7), 7, 0},
	{MFP_PIN(MFP_PIN_GPIO8), 8, 0},
	{MFP_PIN(MFP_PIN_GPIO9), 9, 0},
	{MFP_PIN(MFP_PIN_GPIO10), 10, 0},
	{MFP_PIN(MFP_PIN_GPIO11), 11, 0},
	{MFP_PIN(MFP_PIN_GPIO12), 12, 0},
	{MFP_PIN(MFP_PIN_GPIO13), 13, 0},
	{MFP_PIN(MFP_PIN_GPIO14), 14, 0},
	{MFP_PIN(MFP_PIN_GPIO15), 15, 0},
	{MFP_PIN(MFP_PIN_GPIO16), 16, 0},
	{MFP_PIN(MFP_PIN_GPIO17), 17, 0},
	{MFP_PIN(MFP_PIN_GPIO18), 18, 0},
	{MFP_PIN(MFP_PIN_GPIO19), 19, 0},
	{MFP_PIN(MFP_PIN_GPIO20), 20, 0},
	{MFP_PIN(MFP_PIN_GPIO21), 21, 0},
	{MFP_PIN(MFP_PIN_GPIO22), 22, 0},
	{MFP_PIN(MFP_PIN_GPIO23), 23, 0},
	{MFP_PIN(MFP_PIN_GPIO24), 24, 0},
	{MFP_PIN(MFP_PIN_GPIO25), 25, 0},
	{MFP_PIN(MFP_PIN_GPIO26), 26, 0},
	{MFP_PIN(MFP_PIN_GPIO27), 27, 0},
	{MFP_PIN(MFP_PIN_GPIO28), 28, 0},
	{MFP_PIN(MFP_PIN_GPIO29), 29, 0},
	{MFP_PIN(MFP_PIN_GPIO30), 30, 0},
	{MFP_PIN(MFP_PIN_GPIO31), 31, 0},
	{MFP_PIN(MFP_PIN_GPIO32), 32, 0},
	{MFP_PIN(MFP_PIN_GPIO33), 33, 0},
	{MFP_PIN(MFP_PIN_GPIO34), 34, 0},
	{MFP_PIN(MFP_PIN_GPIO35), 35, 0},
	{MFP_PIN(MFP_PIN_GPIO36), 36, 0},
	{MFP_PIN(MFP_PIN_GPIO37), 37, 0},
	{MFP_PIN(MFP_PIN_GPIO38), 38, 0},
	{MFP_PIN(MFP_PIN_GPIO39), 39, 0},
	{MFP_PIN(MFP_PIN_GPIO40), 40, 0},
	{MFP_PIN(MFP_PIN_GPIO41), 41, 0},
	{MFP_PIN(MFP_PIN_GPIO42), 42, 0},
	{MFP_PIN(MFP_PIN_GPIO43), 43, 0},
	{MFP_PIN(MFP_PIN_GPIO44), 44, 0},
	{MFP_PIN(MFP_PIN_GPIO45), 45, 0},
	{MFP_PIN(MFP_PIN_GPIO46), 46, 0},
	{MFP_PIN(MFP_PIN_GPIO47), 47, 0},
	{MFP_PIN(MFP_PIN_GPIO48), 48, 0},
	{MFP_PIN(MFP_PIN_GPIO49), 49, 0},
	{MFP_PIN(MFP_PIN_GPIO50), 50, 0},
	{MFP_PIN(MFP_PIN_GPIO51), 51, 0},
	{MFP_PIN(MFP_PIN_GPIO52), 52, 0},
	{MFP_PIN(MFP_PIN_GPIO53), 53, 0},
	{MFP_PIN(MFP_PIN_GPIO54), 54, 0},
	{MFP_PIN(MFP_PIN_DF_IO15), 60, 1},
	{MFP_PIN(MFP_PIN_DF_IO14), 61, 1},
	{MFP_PIN(MFP_PIN_DF_IO13), 62, 1},
	{MFP_PIN(MFP_PIN_DF_IO12), 63, 1},
	{MFP_PIN(MFP_PIN_DF_IO11), 64, 1},
	{MFP_PIN(MFP_PIN_DF_IO10), 65, 1},
	{MFP_PIN(MFP_PIN_DF_IO9), 66, 1},
	{MFP_PIN(MFP_PIN_GPIO67), 67, 0},
	{MFP_PIN(MFP_PIN_GPIO68), 68, 0},
	{MFP_PIN(MFP_PIN_GPIO69), 69, 0},
	{MFP_PIN(MFP_PIN_GPIO70), 70, 0},
	{MFP_PIN(MFP_PIN_GPIO71), 71, 0},
	{MFP_PIN(MFP_PIN_GPIO72), 72, 0},
	{MFP_PIN(MFP_PIN_GPIO73), 73, 0},
	{MFP_PIN(MFP_PIN_GPIO74), 74, 0},
	{MFP_PIN(MFP_PIN_GPIO75), 75, 0},
	{MFP_PIN(MFP_PIN_GPIO76), 76, 0},
	{MFP_PIN(MFP_PIN_GPIO77), 77, 0},
	{MFP_PIN(MFP_PIN_GPIO78), 78, 0},
	{MFP_PIN(MFP_PIN_GPIO79), 79, 0},
	{MFP_PIN(MFP_PIN_GPIO80), 80, 0},
	{MFP_PIN(MFP_PIN_GPIO81), 81, 0},
	{MFP_PIN(MFP_PIN_GPIO82), 82, 0},
	{MFP_PIN(MFP_PIN_GPIO83), 83, 0},
	{MFP_PIN(MFP_PIN_GPIO84), 84, 0},
	{MFP_PIN(MFP_PIN_GPIO85), 85, 0},
	{MFP_PIN(MFP_PIN_GPIO86), 86, 0},
	{MFP_PIN(MFP_PIN_GPIO87), 87, 0},
	{MFP_PIN(MFP_PIN_GPIO88), 88, 0},
	{MFP_PIN(MFP_PIN_GPIO89), 89, 0},
	{MFP_PIN(MFP_PIN_GPIO90), 90, 0},
	{MFP_PIN(MFP_PIN_GPIO91), 91, 0},
	{MFP_PIN(MFP_PIN_GPIO92), 92, 0},
	{MFP_PIN(MFP_PIN_GPIO93), 93, 0},
	{MFP_PIN(MFP_PIN_GPIO94), 94, 0},
	{MFP_PIN(MFP_PIN_GPIO95), 95, 0},
	{MFP_PIN(MFP_PIN_GPIO96), 96, 0},
	{MFP_PIN(MFP_PIN_GPIO97), 97, 0},
	{MFP_PIN(MFP_PIN_GPIO98), 98, 0},
	{MFP_PIN(MFP_PIN_MMC1_WP), 99, 1},
	{MFP_PIN(MFP_PIN_DF_IO8), 100, 1},
	{MFP_PIN(MFP_PIN_DF_nCS0_SM_nCS2), 101, 1},
	{MFP_PIN(MFP_PIN_DF_nCS1_SM_nCS3), 102, 1},
	{MFP_PIN(MFP_PIN_SM_nCS0), 103, 1},
	{MFP_PIN(MFP_PIN_SM_nCS1), 104, 1},
	{MFP_PIN(MFP_PIN_DF_WEn), 105, 0},
	{MFP_PIN(MFP_PIN_DF_REn), 106, 0},
	{MFP_PIN(MFP_PIN_DF_ALE_SM_WEn), 107, 0},
	{MFP_PIN(MFP_PIN_DF_RDY0), 108, 1},
	{MFP_PIN(MFP_PIN_PRI_TDI), 117, 1},
	{MFP_PIN(MFP_PIN_PRI_TMS), 118, 1},
	{MFP_PIN(MFP_PIN_PRI_TCK), 119, 1},
	{MFP_PIN(MFP_PIN_PRI_TDO), 120, 1},
	{MFP_PIN(MFP_PIN_SLAVE_RESET_OUT), 122, 1},
	{MFP_PIN(MFP_PIN_CLK_REQ), 123, 1},
	{MFP_PIN(MFP_PIN_GPIO124), 124, 0},
	{MFP_PIN(MFP_PIN_VCXO_REQ), 125, 1},
	{MFP_PIN(MFP_PIN_SM_BE0), 126, 0},
	{MFP_PIN(MFP_PIN_SM_BE1), 127, 0},
};

/****************************************************************/
/* Define value in accordance with
	the specification of each BB vendor. */
#define AP_GPIO_COUNT	MFP_PIN_MAX
/****************************************************************/

#define GET_RESULT_GPIO(a, b, c)	\
	((a<<4 & 0xF0) | (b<<1 & 0xE) | (c & 0x1))

/****************************************************************/
/* Pre-defined variables. (DO NOT CHANGE THIS!!) */
static unsigned char checkgpiomap_result[GDVS_PHONE_STATUS_MAX][AP_GPIO_COUNT];
static struct gpiomap_result gpiomap_result = {
	.init = checkgpiomap_result[PHONE_INIT],
	.sleep = checkgpiomap_result[PHONE_SLEEP]
};
/****************************************************************/

static unsigned __check_init_direction(int gpio)
{
	int index = gpio / 32;
	int offset = gpio % 32;
	unsigned int gpdr[4];

	pxa_direction_get(gpdr);

	return (gpdr[index] & (0x1 << offset)) > 0 ? 2 : 1;
}

static unsigned __check_mfp_direction(unsigned mfpr_val, int func)
{
	u32 ret = 0;
	if (MFPR_AF_SEL(mfpr_val) != func) ret = 0x0; /* FUNC */
	else {
		if (mfpr_val & MFPR_SLEEP_OE_N) ret = 0x01; /* INPUT */
		else ret = 0x2; /*OUTPUT */
	}
	return ret;
}

static unsigned __check_mfp_pull_status(unsigned mfpr_val)
{
	u32 ret = 0;
	if (mfpr_val & MFPR_PULL_SEL) {
		if (mfpr_val & MFPR_PULLUP_EN) ret = 0x2; /* PULL_UP */
		else if (mfpr_val & MFPR_PULLDOWN_EN) ret = 0x01; /* PULL_DOWN */
		else ret = 0x0; /* NO_PULL */
	} else
		ret = 0x0; /* NO_PULL */
	return ret;
}

static unsigned __check_mfp_data_status(unsigned mfpr_val, int gpio, unsigned dir)
{
	u32 ret = 0;

	switch (dir) {
		case 0:
			ret = 0x0; /* FUNC */
			break;
		case 1:
			ret = gpio_get_value(gpio);
			break;
		case 2:
			if (mfpr_val & (0x1 << 8)) ret = 0x1; /* HIGH */
			else ret = 0x0; /* LOW */
			break;
		default:
			break;
	}

	return ret;
}

static void pxa_check_mfp_configuration(unsigned char phonestate)
{
	u8 temp_io = 0, temp_pdpu = 0, temp_lh = 0;
	int i;

	pr_info("[secgpio_dvs][%s] state : %s\n", __func__,
		(phonestate == PHONE_INIT) ? "init" : "sleep");

	for (i = 0; i < ARRAY_SIZE(dvs_offset); i++) {
		unsigned long mfpr_val, offset;
		int gpio, sleep_mode;

		offset = dvs_offset[i].offset;
		gpio = dvs_offset[i].gpio;
		mfpr_val = mfp_read(offset);
		sleep_mode = mfpr_val & ((0x1 << 9) | (0x1 << 3));

		if ((phonestate == PHONE_INIT) || (sleep_mode == 0)) {
			temp_io = __check_mfp_direction(mfpr_val, dvs_offset[i].func);
			if (temp_io == 0) {
				temp_lh = 0;
			} else {
				temp_io = __check_init_direction(gpio);
				temp_lh = gpio_get_value(gpio);
			}
		} else {
			temp_io = __check_mfp_direction(mfpr_val, dvs_offset[i].func);
			temp_lh = __check_mfp_data_status(mfpr_val, gpio, temp_io);
		}
		temp_pdpu = __check_mfp_pull_status(mfpr_val);

		checkgpiomap_result[phonestate][i] =
			GET_RESULT_GPIO(temp_io, temp_pdpu, temp_lh);
	}

	pr_info("[secgpio_dvs][%s]-\n", __func__);

	return;
}

/****************************************************************/
/* Define appropriate variable in accordance with
	the specification of each BB vendor */
static struct gpio_dvs pxa_gpio_dvs = {
	.result = &gpiomap_result,
	.check_gpio_status = pxa_check_mfp_configuration,
	.count = AP_GPIO_COUNT,
};
/****************************************************************/
#endif

void __init mfp_init_addr(struct mfp_addr_map *map)
{
	struct mfp_addr_map *p;
	unsigned long offset, flags;
	int i;

	spin_lock_irqsave(&mfp_spin_lock, flags);

#ifdef CONFIG_SEC_GPIO_DVS
	pxa_gpio_dvs.count = ARRAY_SIZE(dvs_offset);
#endif

	/* mfp offset for readback */
	mfpr_off_readback = map[0].offset;

	for (p = map; p->start != MFP_PIN_INVALID; p++) {
		offset = p->offset;
		i = p->start;

		do {
			mfp_table[i].mfpr_off = offset;
			mfp_table[i].mfpr_run = 0;
			mfp_table[i].mfpr_lpm = 0;
			offset += 4; i++;
		} while ((i <= p->end) && (p->end != -1));
	}

	spin_unlock_irqrestore(&mfp_spin_lock, flags);
}

void mfp_config_lpm(void)
{
	struct mfp_pin *p = &mfp_table[0];
	int pin;

#ifdef CONFIG_ARCH_MMP
	if (cpu_is_pxa168() || cpu_is_pxa910())
		return;
#endif
	for (pin = 0; pin < ARRAY_SIZE(mfp_table); pin++, p++)
		__mfp_config_lpm(p);
}

void mfp_config_run(void)
{
	struct mfp_pin *p = &mfp_table[0];
	int pin;

	for (pin = 0; pin < ARRAY_SIZE(mfp_table); pin++, p++)
		__mfp_config_run(p);
}

void mfp_config_edge(int mfp, unsigned long val)
{
	mfp_cfg_t m;
	m = mfp_read(mfp);
	m &= ~(MFPR_EDGE_MASK);
	m |= mfpr_edge[MFP_LPM_EDGE(val)];
	mfp_write(mfp, m);
}

#ifdef CONFIG_SEC_GPIO_DVS
static struct platform_device secgpio_dvs_device = {
	.name	= "secgpio_dvs",
	.id		= -1,
/****************************************************************/
/* Designate appropriate variable pointer
	in accordance with the specification of each BB vendor. */
	.dev.platform_data = &pxa_gpio_dvs,
/****************************************************************/
};

static struct platform_device *secgpio_dvs_devices[] __initdata = {
	&secgpio_dvs_device,
};

static int __init secgpio_dvs_device_init(void)
{
	mfp_init_base(MFPR_VIRT_BASE);
	mfp_init_addr(pxa988_addr_map);
	return platform_add_devices(
		secgpio_dvs_devices, ARRAY_SIZE(secgpio_dvs_devices));
}
arch_initcall(secgpio_dvs_device_init);
#endif