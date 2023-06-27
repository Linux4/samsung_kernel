/*
 *  linux/arch/arm/mach-mmp/eden_fpga.c
 *
 *  Support for the Marvell EDEN FPGA Platform.
 *
 *  Copyright (C) 2009-2010 Marvell International Ltd.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  publishhed by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/smc91x.h>
#include <linux/mmc/sdhci.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/setup.h>
#include <asm/hardware/gic.h>
#include <mach/irqs.h>
#include <mach/eden.h>
#include <mach/mfp-mmp2.h>
#include <mach/regs-apmu.h>
#include <mach/mmp_device.h>

#include "common.h"
#include "onboard.h"
#include <linux/synaptics_i2c_rmi.h>

#ifdef CONFIG_MMC_SDHCI_PXAV3
#include <linux/mmc/host.h>

/* SD card */
static struct sdhci_pxa_platdata eden_sdh_platdata_mmc0 = {
	.clk_delay_cycles	= 0x1F,
	.flags			= PXA_FLAG_ENABLE_CLOCK_GATING,
	.cd_type	 = PXA_SDHCI_CD_HOST,
	/* Disable all UHS modes on fpga board */
	.host_caps_disable	=
			MMC_CAP_UHS_SDR12 |
			MMC_CAP_UHS_SDR25 |
			MMC_CAP_UHS_SDR104 |
			MMC_CAP_UHS_SDR50 |
			MMC_CAP_UHS_DDR50,
	.quirks		= SDHCI_QUIRK_INVERTED_WRITE_PROTECT,
};

#if 0
/* SDIO */
static struct sdhci_pxa_platdata eden_sdh_platdata_mmc1 = {
	.flags		= PXA_FLAG_EN_PM_RUNTIME
				| PXA_FLAG_WAKEUP_HOST,
	.cd_type	= PXA_SDHCI_CD_EXTERNAL,
	.pm_caps	= MMC_PM_KEEP_POWER,
};

/* eMMC */
static struct sdhci_pxa_platdata eden_sdh_platdata_mmc2 = {
	.flags		= PXA_FLAG_SD_8_BIT_CAPABLE_SLOT
				| PXA_FLAG_ENABLE_CLOCK_GATING
				| PXA_FLAG_EN_PM_RUNTIME,
	.cd_type	= PXA_SDHCI_CD_PERMANENT,
	.clk_delay_cycles	= 0xF,
	.host_caps	= MMC_CAP_1_8V_DDR,
};
#endif

static void __init eden_fpga_init_mmc(void)
{
	/* eden_add_sdh(2, &eden_sdh_platdata_mmc2); */	/* eMMC */
	eden_add_sdh(0, &eden_sdh_platdata_mmc0);	/* SD/MMC */
	/* eden_add_sdh(1, &eden_sdh_platdata_mmc1); */	/* SDIO */
}
#endif /* CONFIG_MMC_SDHCI_PXAV3 */

#if defined(CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI)
static int synaptic_ts_set_power(int on)
{
	/* TODO - Enable any regulator specific code
		on real EDEN platform - not needed for FPGA */
	return 1;
}

static struct synaptics_i2c_rmi_platform_data synaptics_ts_data = {
	.power	= synaptic_ts_set_power,
};
#endif

static struct i2c_board_info eden_twsi1_info[] = {
#if defined(CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI)
	{
		.type	= "synaptics-rmi-ts",
		.addr	= 0x20,
		.irq	= MMP_GPIO_TO_IRQ(101),
		.platform_data	= &synaptics_ts_data,
	},
#endif
};

#if defined(CONFIG_SMC91X)
static struct smc91x_platdata smc91x_info = {
	.flags	= SMC91X_USE_16BIT | SMC91X_NOWAIT,
};

static struct resource smc91x_resources[] = {
	[0] = {
		.start	= SMC_CS0_PHYS_BASE + 0x300,
		.end	= SMC_CS0_PHYS_BASE + 0xfffff,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start  = IRQ_EDEN_SMC,
		.end    = IRQ_EDEN_SMC,
		.flags  = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHEDGE,
	}
};

static struct platform_device smc91x_device = {
	.name		= "smc91x",
	.id		= 0,
	.dev		= {
		.platform_data = &smc91x_info,
	},
	.num_resources	= ARRAY_SIZE(smc91x_resources),
	.resource	= smc91x_resources,
};


static void eden_eth_register(void)
{
	/* Enable clock to SMC Controller */
	writel(0x5b, APMU_SMC_CLK_RES_CTRL);

	/* Configure SMC Controller */
	/* Set CS0 to A\D type memory */
	writel(0x52880008, AXI_VIRT_BASE + 0x83890);

	/* off-chip devices */
	platform_device_register(&smc91x_device);
}
#endif

/* clk usage desciption */
MMP_HW_DESC(fb, "pxa168-fb", 0, 0, "LCDCLK");
struct mmp_hw_desc *eden_fpga_hw_desc[] __initdata = {
	&mmp_device_hw_fb,
};

static void __init eden_fpga_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(eden_fpga_hw_desc); i++)
		mmp_device_hw_register(eden_fpga_hw_desc[i]);

	eden_add_uart(1);
	/* on-chip devices */
	eden_add_twsi(1, NULL, ARRAY_AND_SIZE(eden_twsi1_info));

#ifdef CONFIG_FB_PXA168
	eden_fpga_add_lcd_mipi();
#endif

#ifdef CONFIG_MMC_SDHCI_PXAV3
	eden_fpga_init_mmc();
#endif

#if defined(CONFIG_TOUCHSCREEN_VNC)
	platform_device_register(&eden_device_vnc_touch);
#endif

#ifdef CONFIG_RTC_DRV_SA1100
	platform_device_register(&eden_device_rtc);
#endif

	/* off-chip devices */
#ifdef CONFIG_SMC91X
	eden_eth_register();
#endif
}

/* Initial Setup */
MACHINE_START(EDEN_FPGA, "EDEN")
	.map_io		= mmp_map_io,
	.nr_irqs	= MMP_NR_IRQS,
	.init_irq       = eden_init_irq,
	.timer		= &eden_timer,
	.reserve	= eden_reserve,
#if ((defined(CONFIG_GIC_BYPASS) && defined(CONFIG_SMP)) ||\
	(!defined(CONFIG_GIC_BYPASS)))
	.handle_irq	= gic_handle_irq,
#endif
	.init_machine	= eden_fpga_init,
MACHINE_END
