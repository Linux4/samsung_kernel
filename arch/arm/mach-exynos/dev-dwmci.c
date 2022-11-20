/*
 * linux/arch/arm/mach-exynos/dev-dwmci.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Platform device for Synopsys DesignWare Mobile Storage IP
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/mmc/dw_mmc.h>
#include <linux/mmc/host.h>
#include <linux/clk.h>

#include <plat/devs.h>
#include <plat/cpu.h>

#include <mach/map.h>
#include <mach/dwmci.h>
#include <mach/exynos-pm.h>


/* SFR save/restore for LPA */
struct dw_mci *dw_mci_lpa_host[3] = {0, 0, 0};
unsigned int dw_mci_host_count = 0;
unsigned int dw_mci_save_sfr[3][30];

static struct dw_mci_clk exynos_dwmci_clk_rates[] = {
	{25 * 1000 * 1000, 100 * 1000 * 1000},
	{50 * 1000 * 1000, 200 * 1000 * 1000},
	{50 * 1000 * 1000, 200 * 1000 * 1000},
	{100 * 1000 * 1000, 400 * 1000 * 1000},
	{200 * 1000 * 1000, 800 * 1000 * 1000},
	{100 * 1000 * 1000, 400 * 1000 * 1000},
	{200 * 1000 * 1000, 800 * 1000 * 1000},
	{400 * 1000 * 1000, 800 * 1000 * 1000},
};

static int exynos_dwmci_get_ocr(u32 slot_id)
{
	u32 ocr_avail = MMC_VDD_165_195 | MMC_VDD_32_33 | MMC_VDD_33_34;

	return ocr_avail;
}

static int exynos_dwmci_get_bus_wd(u32 slot_id)
{
	return 4;
}

static int exynos_dwmci_init(u32 slot_id, irq_handler_t handler, void *data)
{
	return 0;
}

static void exynos_dwmci_set_io_timing(void *data, unsigned int tuning, unsigned char timing)
{
	struct dw_mci *host = (struct dw_mci *)data;
	struct dw_mci_board *pdata = host->pdata;
	struct dw_mci_clk *clk_tbl = pdata->clk_tbl;
	u32 clksel, rddqs, dline;
	u32 sclkin, cclkin;

	if (timing > MMC_TIMING_MMC_HS200_DDR) {
		pr_err("%s: timing(%d): not suppored\n", __func__, timing);
		return;
	}

	sclkin = clk_tbl[timing].sclkin;
	cclkin = clk_tbl[timing].cclkin;
	rddqs = DWMCI_DDR200_RDDQS_EN_DEF;
	dline = DWMCI_DDR200_DLINE_CTRL_DEF;
	clksel = __raw_readl(host->regs + DWMCI_CLKSEL);

	if (host->bus_hz != cclkin) {
		host->bus_hz = cclkin;
		host->current_speed = 0;
		clk_set_rate(host->cclk, sclkin);
	}

	if (timing == MMC_TIMING_MMC_HS200_DDR) {
		clksel = (pdata->ddr200_timing & 0xfffffff8) | pdata->clk_smpl;
		if (pdata->is_fine_tuned)
			clksel |= BIT(6);

		if (!tuning) {
			rddqs |= DWMCI_RDDQS_EN;
			if (host->pdata->delay_line)
				dline = DWMCI_FIFO_CLK_DELAY_CTRL(0x2) |
					DWMCI_RD_DQS_DELAY_CTRL(host->pdata->delay_line);
			else
				dline = DWMCI_FIFO_CLK_DELAY_CTRL(0x2) |
					DWMCI_RD_DQS_DELAY_CTRL(90);
			host->quirks &= ~DW_MCI_QUIRK_NO_DETECT_EBIT;
		}
	} else if (timing == MMC_TIMING_MMC_HS200 ||
			timing == MMC_TIMING_UHS_SDR104) {
		clksel = (clksel & 0xfff8ffff) | (pdata->clk_drv << 16);
	} else if (timing == MMC_TIMING_UHS_SDR50) {
		clksel = (clksel & 0xfff8ffff) | (pdata->clk_drv << 16);
	} else if (timing == MMC_TIMING_UHS_DDR50) {
		clksel = pdata->ddr_timing;
	} else {
		clksel = pdata->sdr_timing;
	}

	__raw_writel(clksel, host->regs + DWMCI_CLKSEL);

	if (soc_is_exynos4415() || soc_is_exynos5420() || soc_is_exynos5260()) {
		__raw_writel(rddqs, host->regs + DWMCI_DDR200_RDDQS_EN + 0x70);
		__raw_writel(dline, host->regs + DWMCI_DDR200_DLINE_CTRL + 0x70);
		if (timing == MMC_TIMING_MMC_HS200_DDR)
			__raw_writel(0x1, host->regs + DWMCI_DDR200_ASYNC_FIFO_CTRL + 0x70);
	} else {
		__raw_writel(rddqs, host->regs + DWMCI_DDR200_RDDQS_EN);
		__raw_writel(dline, host->regs + DWMCI_DDR200_DLINE_CTRL);
		if (timing == MMC_TIMING_MMC_HS200_DDR)
			__raw_writel(0x1, host->regs + DWMCI_DDR200_ASYNC_FIFO_CTRL);
	}
}

static void exynos_dwmci_cfg_smu(void *data, u32 action)
{
	struct dw_mci *host = (struct dw_mci *)data;

	if ((!soc_is_exynos5420() && !soc_is_exynos5260() && !soc_is_exynos4415()) || action != 0)
		return;

	/* bypass mode */
	if (host->pdata->ch_num != 2) {
		__raw_writel(0, host->regs + DWMCI_MPSBEGIN0);
		__raw_writel(0xFFFFFFFF, host->regs + DWMCI_MPSEND0);
		__raw_writel(DWMCI_MPSCTRL_SECURE_READ_BIT |
			DWMCI_MPSCTRL_SECURE_WRITE_BIT |
			DWMCI_MPSCTRL_NON_SECURE_READ_BIT |
			DWMCI_MPSCTRL_NON_SECURE_WRITE_BIT |
			DWMCI_MPSCTRL_VALID, host->regs + DWMCI_MPSCTRL0);
	}
}

#ifdef CONFIG_ARCH_EXYNOS5
#ifdef CONFIG_CPU_IDLE
/*
 * MSHC SFR save/restore
 */
int dw_mci_exynos_request_status(void)
{
	int ret = DW_MMC_REQ_BUSY, i;

	for (i = 0;i < dw_mci_host_count; i++) {
		struct dw_mci *host = dw_mci_lpa_host[i];
		if (host->req_state == DW_MMC_REQ_BUSY) {
			ret = DW_MMC_REQ_BUSY;
			break;
		} else
			ret = DW_MMC_REQ_IDLE;
	}

	return ret;
}

static void exynos_sfr_save(unsigned int i)
{
	struct dw_mci *host = dw_mci_lpa_host[i];

	dw_mci_save_sfr[i][0] = __raw_readl(host->regs + DWMCI_CTRL);
	dw_mci_save_sfr[i][1] = __raw_readl(host->regs + DWMCI_PWREN);
	dw_mci_save_sfr[i][2] = __raw_readl(host->regs + DWMCI_CLKDIV);
	dw_mci_save_sfr[i][3] = __raw_readl(host->regs + DWMCI_CLKSRC);
	dw_mci_save_sfr[i][4] = __raw_readl(host->regs + DWMCI_CLKENA);
	dw_mci_save_sfr[i][5] = __raw_readl(host->regs + DWMCI_TMOUT);
	dw_mci_save_sfr[i][6] = __raw_readl(host->regs + DWMCI_CTYPE);
	dw_mci_save_sfr[i][7] = __raw_readl(host->regs + DWMCI_INTMASK);
	dw_mci_save_sfr[i][8] = __raw_readl(host->regs + DWMCI_FIFOTH);
	dw_mci_save_sfr[i][9] = __raw_readl(host->regs + DWMCI_UHS_REG);
	dw_mci_save_sfr[i][10] = __raw_readl(host->regs + DWMCI_BMOD);
	dw_mci_save_sfr[i][11] = __raw_readl(host->regs + DWMCI_PLDMND);
	dw_mci_save_sfr[i][12] = __raw_readl(host->regs + DWMCI_DBADDR);
	dw_mci_save_sfr[i][13] = __raw_readl(host->regs + DWMCI_IDINTEN);
	dw_mci_save_sfr[i][14] = __raw_readl(host->regs + DWMCI_CLKSEL);
	dw_mci_save_sfr[i][15] = __raw_readl(host->regs + DWMCI_CDTHRCTL);

	if (soc_is_exynos4415() || soc_is_exynos5420() || soc_is_exynos5260()) {
		dw_mci_save_sfr[i][16] = __raw_readl(host->regs + DWMCI_DDR200_RDDQS_EN + 0x70);
		dw_mci_save_sfr[i][17] = __raw_readl(host->regs + DWMCI_DDR200_DLINE_CTRL + 0x70);
	} else {
		dw_mci_save_sfr[i][16] = __raw_readl(host->regs + DWMCI_DDR200_RDDQS_EN);
		dw_mci_save_sfr[i][17] = __raw_readl(host->regs + DWMCI_DDR200_DLINE_CTRL);
	}
}

static void exynos_sfr_restore(unsigned int i)
{
	struct dw_mci *host = dw_mci_lpa_host[i];

	int startbit_clear = false;
	unsigned int cmd_status = 0;
	unsigned long timeout = jiffies + msecs_to_jiffies(500);

	/*
	 * To update clock configurations, CIU should be enabled.
	 */
	spin_lock(&host->cclk_lock);
	dw_mci_ciu_clk_en(host);
	spin_unlock(&host->cclk_lock);

	__raw_writel(dw_mci_save_sfr[i][0], host->regs + DWMCI_CTRL);
	__raw_writel(dw_mci_save_sfr[i][1], host->regs + DWMCI_PWREN);
	__raw_writel(dw_mci_save_sfr[i][2], host->regs + DWMCI_CLKDIV);
	__raw_writel(dw_mci_save_sfr[i][3], host->regs + DWMCI_CLKSRC);
	__raw_writel(dw_mci_save_sfr[i][4], host->regs + DWMCI_CLKENA);
	__raw_writel(dw_mci_save_sfr[i][5], host->regs + DWMCI_TMOUT);
	__raw_writel(dw_mci_save_sfr[i][6], host->regs + DWMCI_CTYPE);
	__raw_writel(dw_mci_save_sfr[i][7], host->regs + DWMCI_INTMASK);
	__raw_writel(dw_mci_save_sfr[i][8], host->regs + DWMCI_FIFOTH);
	__raw_writel(dw_mci_save_sfr[i][9], host->regs + DWMCI_UHS_REG);
	__raw_writel(dw_mci_save_sfr[i][10], host->regs + DWMCI_BMOD);
	__raw_writel(dw_mci_save_sfr[i][11], host->regs + DWMCI_PLDMND);
	__raw_writel(dw_mci_save_sfr[i][12], host->regs + DWMCI_DBADDR);
	__raw_writel(dw_mci_save_sfr[i][13], host->regs + DWMCI_IDINTEN);
	__raw_writel(dw_mci_save_sfr[i][14], host->regs + DWMCI_CLKSEL);
	__raw_writel(dw_mci_save_sfr[i][15], host->regs + DWMCI_CDTHRCTL);

	if (soc_is_exynos4415() || soc_is_exynos5420() || soc_is_exynos5260()) {
		__raw_writel(dw_mci_save_sfr[i][16],
			host->regs + DWMCI_DDR200_RDDQS_EN + 0x70);
		__raw_writel(dw_mci_save_sfr[i][17],
			host->regs + DWMCI_DDR200_DLINE_CTRL + 0x70);
	} else {
		__raw_writel(dw_mci_save_sfr[i][16],
			host->regs + DWMCI_DDR200_RDDQS_EN);
		__raw_writel(dw_mci_save_sfr[i][17],
			host->regs + DWMCI_DDR200_DLINE_CTRL);
	}

	__raw_writel(0, host->regs + DWMCI_CMDARG);
	wmb();

#ifdef CONFIG_BCM4334
	if (i == 1)
		return;
#endif

	__raw_writel((DWMCI_CMD_START | DWMCI_CMD_UPD_CLK | DWMCI_CMD_PRV_DAT_WAIT),
					host->regs + DWMCI_CMD);

	while (time_before(jiffies, timeout)) {
		cmd_status = __raw_readl(host->regs + DWMCI_CMD);
		if (!(cmd_status & DWMCI_CMD_START)) {
			startbit_clear = true;
			return;
		}
	}

	if (startbit_clear == false)
		dev_err(&host->dev, "CMD start bit stuck %02d\n", i);
}

static int dw_mmc_exynos_notifier0(struct notifier_block *self,
									unsigned long cmd, void *v)
{																		\
	switch (cmd) {
	case LPA_ENTER:
		exynos_sfr_save(0);
		break;
	case LPA_ENTER_FAIL:
		break;
	case LPA_EXIT:
		exynos_sfr_restore(0);
		break;
	}

	return 0;
}

static int dw_mmc_exynos_notifier1(struct notifier_block *self,
									unsigned long cmd, void *v)
{
	switch (cmd) {
	case LPA_ENTER:
		exynos_sfr_save(1);
		break;
	case LPA_ENTER_FAIL:
		break;
	case LPA_EXIT:
		exynos_sfr_restore(1);
		break;
	}

	return 0;
}

static int dw_mmc_exynos_notifier2(struct notifier_block *self,
									unsigned long cmd, void *v)
{
	switch (cmd) {
	case LPA_ENTER:
		exynos_sfr_save(2);
		break;
	case LPA_ENTER_FAIL:
		break;
	case LPA_EXIT:
		exynos_sfr_restore(2);
		break;
	}

	return 0;
}

static struct notifier_block dw_mmc_exynos_notifier_block[3] = {
	[0] = {
		.notifier_call = dw_mmc_exynos_notifier0,
	},
	[1] = {
		.notifier_call = dw_mmc_exynos_notifier1,
	},
	[2] = {
		.notifier_call = dw_mmc_exynos_notifier2,
	},
};
#endif
#endif

void exynos_register_notifier(void *data)
{
#ifdef CONFIG_CPU_IDLE
	struct dw_mci *host = (struct dw_mci *)data;

	/*
	 * Should be called sequentially
	 */
	dw_mci_lpa_host[dw_mci_host_count] = host;
	exynos_pm_register_notifier(&(dw_mmc_exynos_notifier_block[dw_mci_host_count++]));
#endif
}

void exynos_unregister_notifier(void *data)
{
#ifdef CONFIG_CPU_IDLE
	struct dw_mci *host = (struct dw_mci *)data;
	int i;

	for (i = 0; i < 3; i++) {
		if (host == dw_mci_lpa_host[i])
			exynos_pm_unregister_notifier(&(dw_mmc_exynos_notifier_block[i]));
	}
#endif
}

static struct dw_mci_board exynos4_dwmci_pdata = {
	.num_slots		= 1,
	.quirks			= DW_MCI_QUIRK_BROKEN_CARD_DETECTION,
	.bus_hz			= 100 * 1000 * 1000,
	.detect_delay_ms	= 200,
	.init			= exynos_dwmci_init,
	.get_bus_wd		= exynos_dwmci_get_bus_wd,
	.set_io_timing		= exynos_dwmci_set_io_timing,
	.clk_tbl		= exynos_dwmci_clk_rates,
};

static u64 exynos_dwmci_dmamask = DMA_BIT_MASK(32);

static struct resource exynos4_dwmci_resources[] = {
	[0] = DEFINE_RES_MEM(EXYNOS4_PA_DWMCI, SZ_4K),
	[1] = DEFINE_RES_IRQ(EXYNOS4_IRQ_DWMCI),
};

struct platform_device exynos4_device_dwmci = {
	.name		= "dw_mmc",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(exynos4_dwmci_resources),
	.resource	= exynos4_dwmci_resources,
	.dev		= {
		.dma_mask		= &exynos_dwmci_dmamask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
		.platform_data		= &exynos4_dwmci_pdata,
	},
};

#define EXYNOS4_DWMCI_RESOURCE(_channel)			\
static struct resource exynos4_dwmci##_channel##_resource[] = {	\
	[0] = DEFINE_RES_MEM(S3C_PA_HSMMC##_channel, SZ_8K),	\
	[1] = DEFINE_RES_IRQ(IRQ_HSMMC##_channel),		\
}

EXYNOS4_DWMCI_RESOURCE(0);
EXYNOS4_DWMCI_RESOURCE(1);
EXYNOS4_DWMCI_RESOURCE(2);

#define EXYNOS4_DWMCI_DEF_PLATDATA(_channel)			\
struct dw_mci_board exynos4_dwmci##_channel##_def_platdata = {	\
	.num_slots		= 1,				\
	.quirks			=				\
		DW_MCI_QUIRK_BROKEN_CARD_DETECTION,		\
	.bus_hz			= 200 * 1000 * 1000,		\
	.detect_delay_ms	= 200,				\
	.init			= exynos_dwmci_init,		\
	.get_bus_wd		= exynos_dwmci_get_bus_wd,	\
	.set_io_timing		= exynos_dwmci_set_io_timing,	\
	.get_ocr		= exynos_dwmci_get_ocr,		\
	.cd_type		= DW_MCI_CD_PERMANENT,		\
	.clk_tbl		= exynos_dwmci_clk_rates,	\
	.cfg_smu		= exynos_dwmci_cfg_smu,		\
}

EXYNOS4_DWMCI_DEF_PLATDATA(0);
EXYNOS4_DWMCI_DEF_PLATDATA(1);
EXYNOS4_DWMCI_DEF_PLATDATA(2);

#define EXYNOS4_DWMCI_PLATFORM_DEVICE(_channel)			\
struct platform_device exynos4_device_dwmci##_channel =		\
{								\
	.name		= "dw_mmc",				\
	.id		= _channel,				\
	.num_resources	=					\
	ARRAY_SIZE(exynos4_dwmci##_channel##_resource),		\
	.resource	= exynos4_dwmci##_channel##_resource,	\
	.dev		= {					\
		.dma_mask		= &exynos_dwmci_dmamask,\
		.coherent_dma_mask	= DMA_BIT_MASK(32),	\
		.platform_data		=			\
			&exynos4_dwmci##_channel##_def_platdata,\
	},							\
}

EXYNOS4_DWMCI_PLATFORM_DEVICE(0);
EXYNOS4_DWMCI_PLATFORM_DEVICE(1);
EXYNOS4_DWMCI_PLATFORM_DEVICE(2);

static struct platform_device *exynos4_dwmci_devs[] = {
	&exynos4_device_dwmci0,
	&exynos4_device_dwmci1,
	&exynos4_device_dwmci2,
};

#define EXYNOS5_DWMCI_RESOURCE(_channel)			\
static struct resource exynos5_dwmci##_channel##_resource[] = {	\
	[0] = DEFINE_RES_MEM(S3C_PA_HSMMC##_channel, SZ_8K),	\
	[1] = DEFINE_RES_IRQ(IRQ_HSMMC##_channel),		\
}

EXYNOS5_DWMCI_RESOURCE(0);
EXYNOS5_DWMCI_RESOURCE(1);
EXYNOS5_DWMCI_RESOURCE(2);
#if !defined(CONFIG_SOC_EXYNOS5420) && !defined(CONFIG_SOC_EXYNOS5260) && !defined(CONFIG_SOC_EXYNOS4415)
EXYNOS5_DWMCI_RESOURCE(3);
#endif

#define EXYNOS5_DWMCI_DEF_PLATDATA(_channel)			\
struct dw_mci_board exynos5_dwmci##_channel##_def_platdata = {	\
	.num_slots		= 1,				\
	.quirks			=				\
		DW_MCI_QUIRK_BROKEN_CARD_DETECTION,		\
	.bus_hz			= 200 * 1000 * 1000,		\
	.detect_delay_ms	= 200,				\
	.init			= exynos_dwmci_init,		\
	.get_bus_wd		= exynos_dwmci_get_bus_wd,	\
	.set_io_timing		= exynos_dwmci_set_io_timing,	\
	.get_ocr		= exynos_dwmci_get_ocr,		\
	.cd_type		= DW_MCI_CD_PERMANENT,		\
	.clk_tbl		= exynos_dwmci_clk_rates,	\
	.cfg_smu		= exynos_dwmci_cfg_smu,		\
}

EXYNOS5_DWMCI_DEF_PLATDATA(0);
EXYNOS5_DWMCI_DEF_PLATDATA(1);
EXYNOS5_DWMCI_DEF_PLATDATA(2);
#if !defined(CONFIG_SOC_EXYNOS5420) && !defined(CONFIG_SOC_EXYNOS5260) && !defined(CONFIG_SOC_EXYNOS4415)
EXYNOS5_DWMCI_DEF_PLATDATA(3);
#endif

#define EXYNOS5_DWMCI_PLATFORM_DEVICE(_channel)			\
struct platform_device exynos5_device_dwmci##_channel =		\
{								\
	.name		= "dw_mmc",				\
	.id		= _channel,				\
	.num_resources	=					\
	ARRAY_SIZE(exynos5_dwmci##_channel##_resource),		\
	.resource	= exynos5_dwmci##_channel##_resource,	\
	.dev		= {					\
		.dma_mask		= &exynos_dwmci_dmamask,\
		.coherent_dma_mask	= DMA_BIT_MASK(32),	\
		.platform_data		=			\
			&exynos5_dwmci##_channel##_def_platdata,\
	},							\
}

EXYNOS5_DWMCI_PLATFORM_DEVICE(0);
EXYNOS5_DWMCI_PLATFORM_DEVICE(1);
EXYNOS5_DWMCI_PLATFORM_DEVICE(2);
#if !defined(CONFIG_SOC_EXYNOS5420) && !defined(CONFIG_SOC_EXYNOS5260) && !defined(CONFIG_SOC_EXYNOS4415)
EXYNOS5_DWMCI_PLATFORM_DEVICE(3);
#endif

static struct platform_device *exynos5_dwmci_devs[] = {
	&exynos5_device_dwmci0,
	&exynos5_device_dwmci1,
	&exynos5_device_dwmci2,
#if !defined(CONFIG_SOC_EXYNOS5420) && !defined(CONFIG_SOC_EXYNOS5260) && !defined(CONFIG_SOC_EXYNOS4415)
	&exynos5_device_dwmci3,
#endif
};

void __init exynos_dwmci_set_platdata(struct dw_mci_board *pd, u32 slot_id)
{
	struct dw_mci_board *npd = NULL;

	if ((soc_is_exynos4210()) || soc_is_exynos4212() ||
		soc_is_exynos4412()) {
		npd = s3c_set_platdata(pd, sizeof(struct dw_mci_board),
				&exynos4_device_dwmci);
	} else if (soc_is_exynos5250() || soc_is_exynos5410() ||
			soc_is_exynos5420() || soc_is_exynos5260()) {
		if (slot_id < ARRAY_SIZE(exynos5_dwmci_devs))
			npd = s3c_set_platdata(pd, sizeof(struct dw_mci_board),
					       exynos5_dwmci_devs[slot_id]);
		else
			pr_err("%s: slot %d is not supported\n", __func__,
			       slot_id);
	} else if (soc_is_exynos4415()) {
		if (slot_id < ARRAY_SIZE(exynos4_dwmci_devs))
			npd = s3c_set_platdata(pd, sizeof(struct dw_mci_board),
					       exynos4_dwmci_devs[slot_id]);
		else
			pr_err("%s: slot %d is not supported\n", __func__,
			       slot_id);
	}


	if (!npd)
		return;

	if (!npd->init)
		npd->init = exynos_dwmci_init;
	if (!npd->get_bus_wd)
		npd->get_bus_wd = exynos_dwmci_get_bus_wd;
	if (!npd->set_io_timing)
		npd->set_io_timing = exynos_dwmci_set_io_timing;
	if (!npd->get_ocr)
		npd->get_ocr = exynos_dwmci_get_ocr;
	if (!npd->clk_tbl)
		npd->clk_tbl = exynos_dwmci_clk_rates;
	if (!npd->cfg_smu)
		npd->cfg_smu = exynos_dwmci_cfg_smu;
}
