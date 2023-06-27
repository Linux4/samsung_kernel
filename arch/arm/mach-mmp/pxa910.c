/*
 *  linux/arch/arm/mach-mmp/pxa910.c
 *
 *  Code specific to PXA910
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gpio-pxa.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/platform_data/mmp_audio.h>
#include <linux/mfd/ds1wm.h>

#include <asm/mach/time.h>
#include <mach/addr-map.h>
#include <mach/regs-apbc.h>
#include <mach/regs-apmu.h>
#include <mach/cputype.h>
#include <mach/irqs.h>
#include <mach/dma.h>
#include <mach/mfp.h>
#include <mach/devices.h>
#include <mach/pm-pxa910.h>

#include "common.h"
#include "clock.h"

#define MFPR_VIRT_BASE	(APB_VIRT_BASE + 0x1e000)
unsigned char __iomem *dmc_membase;
EXPORT_SYMBOL(dmc_membase);

static struct mfp_addr_map pxa910_mfp_addr_map[] __initdata =
{
	MFP_ADDR_X(GPIO0, GPIO54, 0xdc),
	MFP_ADDR_X(GPIO67, GPIO98, 0x1b8),
	MFP_ADDR_X(GPIO100, GPIO109, 0x238),

	MFP_ADDR(GPIO123, 0xcc),
	MFP_ADDR(GPIO124, 0xd0),

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

	MFP_ADDR_X(MMC1_DAT7, MMC1_WP, 0x84),

	MFP_ADDR_END,
};

void __init pxa910_init_irq(void)
{
	icu_init_irq();
}

/* gssp clk ops: gssp is shared between AP and CP */
static void gssp_clk_enable(struct clk *clk)
{
	unsigned int gcer;
	/* GPB bus select: choose APB */
	__raw_writel(0x1, APBC_PXA910_GBS);
	/* GSSP clock control register: GCER */
	gcer = __raw_readl(clk->clk_rst) & ~(0x3 << 8);
	/* choose I2S clock */
	gcer |= APBC_FNCLK | (0x0 << 8);
	__raw_writel(gcer, clk->clk_rst);
	udelay(10);
	gcer |= APBC_APBCLK;
	__raw_writel(gcer, clk->clk_rst);
	udelay(10);
	gcer &= ~APBC_RST;
	__raw_writel(gcer, clk->clk_rst);
	pr_debug("gssp clk is open\n");
}

static void gssp_clk_disable(struct clk *clk)
{
	unsigned int gcer;
	gcer = __raw_readl(clk->clk_rst);
	gcer &= ~APBC_APBCLK;
	__raw_writel(gcer, clk->clk_rst);
	__raw_writel(0x0, APBC_PXA910_GBS);
	pr_debug("gssp clk is closed\n");
}

struct clkops gssp_clk_ops = {
	.enable = gssp_clk_enable,
	.disable = gssp_clk_disable,
};

static void lcd_clk_enable(struct clk *clk)
{
	__raw_writel(clk->enable_val, clk->clk_rst);
}
static void lcd_clk_disable(struct clk *clk)
{
	u32 tmp = __raw_readl(clk->clk_rst);
	tmp &= ~0x38;	/* release from reset to keep register setting */
	__raw_writel(tmp, clk->clk_rst);
}

static int lcd_clk_setrate(struct clk *clk, unsigned long val)
{
	__raw_writel(val, clk->clk_rst);
	return 0;
}
static unsigned long lcd_clk_getrate(struct clk *clk)
{
	unsigned long rate = clk->rate;
	return rate;
}

struct clkops lcd_pn1_clk_ops = {
	.enable		= lcd_clk_enable,
	.disable	= lcd_clk_disable,
	.setrate	= lcd_clk_setrate,
	.getrate	= lcd_clk_getrate,
};

/* APB peripheral clocks */
static APBC_CLK(uart1, PXA910_UART0, 1, 14745600);
static APBC_CLK(uart2, PXA910_UART1, 1, 14745600);
static APBC_CLK(twsi0, PXA168_TWSI0, 1, 33000000);
static APBC_CLK(twsi1, PXA168_TWSI1, 1, 33000000);
static APBC_CLK(pwm1, PXA910_PWM1, 1, 13000000);
static APBC_CLK(pwm2, PXA910_PWM2, 1, 13000000);
static APBC_CLK(pwm3, PXA910_PWM3, 1, 13000000);
static APBC_CLK(pwm4, PXA910_PWM4, 1, 13000000);
static APBC_CLK(gpio, PXA910_GPIO, 0, 13000000);
static APBC_CLK(rtc, PXA910_RTC, 8, 32768);
static APBC_CLK(ssp1, PXA910_SSP1, 4, 3250000);
static APBC_CLK(ssp2, PXA910_SSP2, 0, 0);
static APBC_CLK(1wire,  PXA910_ONEWIRE,  0, 26000000);
static APBC_CLK_OPS(gssp, PXA910_GCER, 0, 0, &gssp_clk_ops);

static APMU_CLK(nand, NAND, 0x19b, 156000000);
static APMU_CLK(u2o, USB, 0x9, 480000000);
static APMU_CLK(u2h, USB, 0x012, 480000000);
static APMU_CLK_OPS(lcd, LCD, 0x003f, 312000000, &lcd_pn1_clk_ops);
static APMU_CLK(ccic_rst, CCIC_RST, 0x0, 312000000);
static APMU_CLK(ccic_gate, CCIC_GATE, 0xfff, 0);
static APMU_CLK(sdh0, SDH0, 0x001b, 44500000);
static APMU_CLK(sdh1, SDH1, 0x001b, 44500000);
/* Configure the clock as 52MHz since eMMC is used on SDH2 at pxa920
 * board. If SD 2.0 card is used on SDH2, according to SD 2.0 spec,
 * the max clock is limited to 50MHz, so this patch cannot be applied.
 */
static APMU_CLK(sdh2, SDH2, 0x005b, 52000000);

/* device and clock bindings */
static struct clk_lookup pxa910_clkregs[] = {
	INIT_CLKREG(&clk_uart1, "pxa2xx-uart.0", NULL),
	INIT_CLKREG(&clk_uart2, "pxa2xx-uart.1", NULL),
	INIT_CLKREG(&clk_twsi0, "pxa2xx-i2c.0", NULL),
	INIT_CLKREG(&clk_twsi1, "pxa2xx-i2c.1", NULL),
	INIT_CLKREG(&clk_pwm1, "pxa910-pwm.0", NULL),
	INIT_CLKREG(&clk_pwm2, "pxa910-pwm.1", NULL),
	INIT_CLKREG(&clk_pwm3, "pxa910-pwm.2", NULL),
	INIT_CLKREG(&clk_pwm4, "pxa910-pwm.3", NULL),
	INIT_CLKREG(&clk_nand, "pxa3xx-nand", NULL),
	INIT_CLKREG(&clk_gpio, "pxa-gpio", NULL),
	INIT_CLKREG(&clk_1wire, NULL, "PXA-W1"),
	INIT_CLKREG(&clk_u2o, NULL, "U2OCLK"),
	INIT_CLKREG(&clk_rtc, "sa1100-rtc", NULL),
	INIT_CLKREG(&clk_ssp1, "pxa910-ssp.0", NULL),
	INIT_CLKREG(&clk_ssp2, "pxa910-ssp.1", NULL),
	INIT_CLKREG(&clk_gssp, "pxa910-ssp.4", NULL),
	INIT_CLKREG(&clk_lcd, NULL, "LCDCLK"),
	INIT_CLKREG(&clk_ccic_rst, "mmp-camera.0", "CCICRSTCLK"),
	INIT_CLKREG(&clk_ccic_gate, "mmp-camera.0", "CCICGATECLK"),
	INIT_CLKREG(&clk_sdh0, "sdhci-pxav2.0", "PXA-SDHCLK"),
	INIT_CLKREG(&clk_sdh1, "sdhci-pxav2.1", "PXA-SDHCLK"),
	INIT_CLKREG(&clk_sdh2, "sdhci-pxav2.2", "PXA-SDHCLK"),
};

static int __init pxa910_init(void)
{
	if (cpu_is_pxa910()) {
		mfp_init_base(MFPR_VIRT_BASE);
		mfp_init_addr(pxa910_mfp_addr_map);
		pxa_init_dma(IRQ_PXA910_DMA_INT0, 32);
		clkdev_add_table(ARRAY_AND_SIZE(pxa910_clkregs));
	}

	dmc_membase = ioremap(0xb0000000, 0x00001000);
	return 0;
}
postcore_initcall(pxa910_init);

/* system timer - clock enabled, 3.25MHz */
#define TIMER_CLK_RST	(APBC_APBCLK | APBC_FNCLK | APBC_FNCLKSEL(3))

static void __init pxa910_timer_init(void)
{
	/* reset and configure */
	__raw_writel(APBC_APBCLK | APBC_RST, APBC_PXA910_TIMERS);
	__raw_writel(TIMER_CLK_RST, APBC_PXA910_TIMERS);

	timer_init(IRQ_PXA910_AP1_TIMER1);
}

struct sys_timer pxa910_timer = {
	.init	= pxa910_timer_init,
};

/* on-chip devices */

/* NOTE: there are totally 3 UARTs on PXA910:
 *
 *   UART1   - Slow UART (can be used both by AP and CP)
 *   UART2/3 - Fast UART
 *
 * To be backward compatible with the legacy FFUART/BTUART/STUART sequence,
 * they are re-ordered as:
 *
 *   pxa910_device_uart1 - UART2 as FFUART
 *   pxa910_device_uart2 - UART3 as BTUART
 *
 * UART1 is not used by AP for the moment.
 */
PXA910_DEVICE(uart1, "pxa2xx-uart", 0, UART2, 0xd4017000, 0x30, 21, 22);
PXA910_DEVICE(uart2, "pxa2xx-uart", 1, UART3, 0xd4018000, 0x30, 23, 24);
PXA910_DEVICE(twsi0, "pxa2xx-i2c", 0, TWSI0, 0xd4011000, 0x28);
PXA910_DEVICE(twsi1, "pxa2xx-i2c", 1, TWSI1, 0xd4025000, 0x28);
PXA910_DEVICE(pwm1, "pxa910-pwm", 0, NONE, 0xd401a000, 0x10);
PXA910_DEVICE(pwm2, "pxa910-pwm", 1, NONE, 0xd401a400, 0x10);
PXA910_DEVICE(pwm3, "pxa910-pwm", 2, NONE, 0xd401a800, 0x10);
PXA910_DEVICE(pwm4, "pxa910-pwm", 3, NONE, 0xd401ac00, 0x10);
PXA910_DEVICE(nand, "pxa3xx-nand", -1, NAND, 0xd4283000, 0x80, 97, 99);
PXA910_DEVICE(cnm, "pxa-cnm", -1, CNM, 0xd420d000, 0x1000);
PXA910_DEVICE(asram, "asram", 0, NONE, 0xd100a000, 0x15000);
PXA910_DEVICE(ssp0, "pxa910-ssp", 0, SSP1, 0xd401b000, 0x90, 52, 53);
PXA910_DEVICE(ssp1, "pxa910-ssp", 1, SSP2, 0xd42a0c00, 0x90, 1, 2);
PXA910_DEVICE(ssp2, "pxa910-ssp", 2, SSP3, 0xd401C000, 0x90, 60, 61);
PXA910_DEVICE(gssp, "pxa910-ssp", 4, GSSP, 0xd4039000, 0x90, 6, 7);
PXA910_DEVICE(fb, "pxa168-fb", 0, LCD, 0xd420b000, 0x1ec);
PXA910_DEVICE(fb_ovly, "pxa168fb_ovly", 0, LCD, 0xd420b000, 0x1ec);
PXA910_DEVICE(camera, "mmp-camera", 0, CCIC, 0xd420a000, 0xfff);
PXA910_DEVICE(sdh0, "sdhci-pxav2", 0, MMC, 0xd4280000, 0x120);
PXA910_DEVICE(sdh1, "sdhci-pxav2", 1, MMC, 0xd4280800, 0x120);
PXA910_DEVICE(sdh2, "sdhci-pxav2", 2, MMC, 0xd4281000, 0x120);

struct resource pxa910_resource_gpio[] = {
	{
		.start	= 0xd4019000,
		.end	= 0xd4019fff,
		.flags	= IORESOURCE_MEM,
	}, {
		.start	= IRQ_PXA910_AP_GPIO,
		.end	= IRQ_PXA910_AP_GPIO,
		.name	= "gpio_mux",
		.flags	= IORESOURCE_IRQ,
	},
};

static struct pxa_gpio_platform_data pxa910_gpio_info __initdata = {
	.gpio_set_wake = pxa910_set_wake,
};

struct platform_device pxa910_device_gpio = {
	.name		= "pxa-gpio",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(pxa910_resource_gpio),
	.resource	= pxa910_resource_gpio,
	.dev            = {
		.platform_data  = &pxa910_gpio_info,
	},
};

static struct resource pxa910_resource_rtc[] = {
	{
		.start	= 0xd4010000,
		.end	= 0xd401003f,
		.flags	= IORESOURCE_MEM,
	}, {
		.start	= IRQ_PXA910_RTC_INT,
		.end	= IRQ_PXA910_RTC_INT,
		.name	= "rtc 1Hz",
		.flags	= IORESOURCE_IRQ,
	}, {
		.start	= IRQ_PXA910_RTC_ALARM,
		.end	= IRQ_PXA910_RTC_ALARM,
		.name	= "rtc alarm",
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device pxa910_device_rtc = {
	.name		= "sa1100-rtc",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(pxa910_resource_rtc),
	.resource	= pxa910_resource_rtc,
};

static struct resource pxa910_resource_squ[] = {
	{
		.start	= 0xd42a0800,
		.end	= 0xd42a08ff,
		.flags	= IORESOURCE_MEM,
	}, {
		.start	= IRQ_PXA910_HIFI_DMA,
		.end	= IRQ_PXA910_HIFI_DMA,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device pxa910_device_squ = {
	.name		= "pxa910-squ",
	.id		= 1,
	.num_resources	= ARRAY_SIZE(pxa910_resource_squ),
	.resource	= pxa910_resource_squ,
	.dev		= {
		.coherent_dma_mask = DMA_BIT_MASK(64),
	},
};

static struct resource pxa910_resource_pcm_audio[] = {
	 {
		 /* playback dma */
		.name	= "pxa910-squ",
		.start	= 0,
		.flags	= IORESOURCE_DMA,
	},
	 {
		 /* record dma */
		.name	= "pxa910-squ",
		.start	= 1,
		.flags	= IORESOURCE_DMA,
	},
};

static struct mmp_audio_platdata mmp_audio_pdata = {
	.period_max_capture = 4 * 1024,
	.buffer_max_capture = 20 * 1024,
	.period_max_playback = 4 * 1024,
	.buffer_max_playback = 56 * 1024,
};

struct platform_device pxa910_device_asoc_platform = {
	.name		= "mmp-pcm-audio",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(pxa910_resource_pcm_audio),
	.resource	= pxa910_resource_pcm_audio,
	.dev = {
		.platform_data  = &mmp_audio_pdata,
	},
};

static struct resource pxa910_resource_1wire[] = {
	{ 0xd4011800, 0xd4011814, NULL, IORESOURCE_MEM, },
	{ IRQ_PXA910_ONEWIRE, IRQ_PXA910_ONEWIRE, NULL,	\
	IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHEDGE, },
};

struct platform_device pxa910_device_1wire = {
	.name		= "pxa-w1",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(pxa910_resource_1wire),
	.resource	= pxa910_resource_1wire,
};
