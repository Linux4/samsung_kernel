/* Copyright (C) 2013 Samsung Electronics Co, Ltd.
 *
 * Based on mach-sc\board-huskytd\board-huskytd-mmc.c
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#include <linux/platform_device.h>
#include <linux/mmc/sdhci.h>

#include <mach/hardware.h>
#include <mach/irqs.h>
#include <mach/board.h>
#include <mach/sci.h>
#include <mach/sci_glb_regs.h>

static struct resource sprd_sdio0_resources[] = {
	[0] = {
		.start = SPRD_SDIO0_BASE,
		.end = SPRD_SDIO0_BASE + SPRD_SDIO0_SIZE-1,
		.name = "sdio0_res",
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_SDIO0_INT,
		.end = IRQ_SDIO0_INT,
		.flags = IORESOURCE_IRQ,
	},
};

static struct sprd_host_platdata sprd_sdio0_pdata = {
	.hw_name = "sprd-sdcard",
	.detect_gpio = 71,
	.vdd_name = "vddsd",
	.clk_name = "clk_sdio0",
	.clk_parent = "clk_192m",
	.max_clock = 192000000,
	.enb_bit = BIT_SDIO0_EB,
	.rst_bit = BIT_SDIO0_SOFT_RST,
};

static struct platform_device sprd_sdio0_device = {
	.name           = "sprd-sdhci",
	.id             =  0,
	.num_resources  = ARRAY_SIZE(sprd_sdio0_resources),
	.resource       = sprd_sdio0_resources,
	.dev = { .platform_data = &sprd_sdio0_pdata },
};

static struct resource sprd_sdio1_resources[] = {
	[0] = {
		.start = SPRD_SDIO1_BASE,
		.end = SPRD_SDIO1_BASE + SPRD_SDIO1_SIZE-1,
		.name = "sdio1_res",
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_SDIO1_INT,
		.end = IRQ_SDIO1_INT,
		.flags = IORESOURCE_IRQ,
	},
};

static struct sprd_host_platdata sprd_sdio1_pdata = {
	.hw_name = "sprd-sdio1",
	.clk_name = "clk_sdio1",
	.clk_parent = "clk_96m",
	.max_clock = 96000000,
	.enb_bit = BIT_SDIO1_EB,
	.rst_bit = BIT_SDIO1_SOFT_RST,
	.regs.is_valid = 1,
};

static struct platform_device sprd_sdio1_device = {
	.name           = "sprd-sdhci",
	.id             =  1,
	.num_resources  = ARRAY_SIZE(sprd_sdio1_resources),
	.resource       = sprd_sdio1_resources,
	.dev = { .platform_data = &sprd_sdio1_pdata },
};

static struct resource sprd_sdio2_resources[] = {
	[0] = {
	       .start = SPRD_SDIO2_BASE,
	       .end = SPRD_SDIO2_BASE + SPRD_SDIO2_SIZE - 1,
	       .name = "sdio2_res",
	       .flags = IORESOURCE_MEM,
	       },
	[1] = {
	       .start = IRQ_SDIO2_INT,
	       .end = IRQ_SDIO2_INT,
	       .flags = IORESOURCE_IRQ,
	       }
};

static struct sprd_host_platdata sprd_sdio2_pdata = {
	.hw_name = "sprd-sdio2",
	.clk_name = "clk_sdio2",
	.clk_parent = "clk_96m",
	.max_clock = 96000000,
	.enb_bit = BIT_SDIO2_EB,
	.rst_bit = BIT_SDIO2_SOFT_RST,
};

static struct platform_device sprd_sdio2_device = {
	.name = "sprd-sdhci",
	.id = 2,
	.num_resources = ARRAY_SIZE(sprd_sdio2_resources),
	.resource = sprd_sdio2_resources,
	.dev = { .platform_data = &sprd_sdio2_pdata },
};

void huskytd_sdio_init(void)
{
	platform_device_register(&sprd_sdio0_device);
	platform_device_register(&sprd_sdio1_device);
}

static struct resource sprd_emmc_resources[] = {
	[0] = {
	       .start = SPRD_EMMC_BASE,
	       .end = SPRD_EMMC_BASE + SPRD_EMMC_SIZE - 1,
	       .name = "sdio3_res",
	       .flags = IORESOURCE_MEM,
	       },
	[1] = {
	       .start = IRQ_EMMC_INT,
	       .end = IRQ_EMMC_INT,
	       .flags = IORESOURCE_IRQ,
	       },
};

static struct sprd_host_platdata sprd_emmc_pdata = {
	.hw_name = "sprd-emmc",
	.vdd_name = "vddemmcio",
	.vdd_ext_name = "vddemmccore",
	.clk_name = "clk_emmc",
	.clk_parent = "clk_192m",
	.max_clock = 192000000,
	.enb_bit = BIT_EMMC_EB,
	.rst_bit = BIT_EMMC_SOFT_RST,
	.regs.is_valid = 1,
};

static struct platform_device sprd_emmc_device = {
	.name = "sprd-sdhci",
	.id = 3,
	.num_resources = ARRAY_SIZE(sprd_emmc_resources),
	.resource = sprd_emmc_resources,
	.dev = { .platform_data = &sprd_emmc_pdata },
};

void huskytd_emmc_init(void)
{
	platform_device_register(&sprd_emmc_device);
}

void huskytd_mmc_init(void)
{
	huskytd_emmc_init();
	huskytd_sdio_init();
}

