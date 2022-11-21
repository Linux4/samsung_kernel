/*
 * Copyright (C) 2010 Marvell International Ltd.
 *		Zhangfei Gao <zhangfei.gao@marvell.com>
 *		Kevin Wang <dwang4@marvell.com>
 *		Mingwei Wang <mwwang@marvell.com>
 *		Philip Rakity <prakity@marvell.com>
 *		Mark Brown <markb@marvell.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/err.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/slot-gpio.h>
#include <linux/platform_data/pxa_sdhci.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/of_gpio.h>

#include <plat/pm.h>

#include "sdhci.h"
#include "sdhci-pltfm.h"

#define PXAV3_RPM_DELAY_MS	50

#define SD_CLOCK_BURST_SIZE_SETUP		0x10A
#define SDCLK_SEL	0x100
#define SDCLK_DELAY_SHIFT	9
#define SDCLK_DELAY_MASK	0x1f

#define SD_CFG_FIFO_PARAM       0x100
#define SDCFG_GEN_PAD_CLK_ON	(1<<6)
#define SDCFG_GEN_PAD_CLK_CNT_MASK	0xFF
#define SDCFG_GEN_PAD_CLK_CNT_SHIFT	24

#define SD_SPI_MODE          0x108
#define SD_CE_ATA_1          0x10C

#define SD_CE_ATA_2          0x10E
#define SDCE_MISC_INT		(1<<2)
#define SDCE_MISC_INT_EN	(1<<1)

#define SD_FIFO_PARAM	0x104
#define PAD_CLK_GATE_MASK	(0x3<<11)

static unsigned long pxav3_clk_prepare(struct sdhci_host *host,
		unsigned long rate)
{
	struct platform_device *pdev = to_platform_device(mmc_dev(host->mmc));
	struct sdhci_pxa_platdata *pdata = pdev->dev.platform_data;
	struct sdhci_pxa_dtr_data *dtr_data;
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct mmc_ios *ios = &host->mmc->ios;

	unsigned long preset_rate = 0, src_rate = 0;

	if (pdata && pdata->dtr_data) {
		dtr_data = pdata->dtr_data;
		/* search the dtr table */
		while (MMC_TIMING_MAX != dtr_data->timing) {
			if (ios->timing == dtr_data->timing) {
				if ((MMC_TIMING_LEGACY == ios->timing) &&
						(rate != PXA_SDH_DTR_25M))
					preset_rate = rate;
				else
					preset_rate = dtr_data->preset_rate;
				src_rate = dtr_data->src_rate;
				break;
			}
			dtr_data++;
		};
		/* if timing mode not in the dtr table, use default src rate */
		if (MMC_TIMING_MAX == dtr_data->timing) {
			src_rate = dtr_data->src_rate;
			preset_rate = rate;
		}
		clk_set_rate(pltfm_host->clk, src_rate);

		return preset_rate;
	} else
		return rate;
	return 0;
}

static u32 pxav3_get_max_clock(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);

	return clk_get_rate(pltfm_host->clk);
}

static void pxav3_set_private_registers(struct sdhci_host *host, u8 mask)
{
	struct platform_device *pdev = to_platform_device(mmc_dev(host->mmc));
	struct sdhci_pxa_platdata *pdata = pdev->dev.platform_data;

	if (mask == SDHCI_RESET_ALL) {
		/*
		 * tune timing of read data/command when crc error happen
		 * no performance impact
		 */
		if (pdata && 0 != pdata->clk_delay_cycles) {
			u16 tmp;

			tmp = readw(host->ioaddr + SD_CLOCK_BURST_SIZE_SETUP);
			tmp |= (pdata->clk_delay_cycles & SDCLK_DELAY_MASK)
				<< SDCLK_DELAY_SHIFT;
			tmp |= SDCLK_SEL;
			writew(tmp, host->ioaddr + SD_CLOCK_BURST_SIZE_SETUP);
		}
	}
}

#define MAX_WAIT_COUNT 5
static void pxav3_gen_init_74_clocks(struct sdhci_host *host, u8 power_mode)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_pxa *pxa = pltfm_host->priv;
	u16 tmp;
	int count;

	if (pxa->power_mode == MMC_POWER_UP
			&& power_mode == MMC_POWER_ON) {

		dev_dbg(mmc_dev(host->mmc),
				"%s: slot->power_mode = %d,"
				"ios->power_mode = %d\n",
				__func__,
				pxa->power_mode,
				power_mode);

		/* set we want notice of when 74 clocks are sent */
		tmp = readw(host->ioaddr + SD_CE_ATA_2);
		tmp |= SDCE_MISC_INT_EN;
		writew(tmp, host->ioaddr + SD_CE_ATA_2);

		/* start sending the 74 clocks */
		tmp = readw(host->ioaddr + SD_CFG_FIFO_PARAM);
		tmp |= SDCFG_GEN_PAD_CLK_ON;
		writew(tmp, host->ioaddr + SD_CFG_FIFO_PARAM);

		/* slowest speed is about 100KHz or 10usec per clock */
		udelay(740);
		count = 0;

		while (count++ < MAX_WAIT_COUNT) {
			if ((readw(host->ioaddr + SD_CE_ATA_2)
						& SDCE_MISC_INT) == 0)
				break;
			udelay(10);
		}

		if (count == MAX_WAIT_COUNT)
			dev_warn(mmc_dev(host->mmc), "74 clock interrupt not cleared\n");

		/* clear the interrupt bit if posted */
		tmp = readw(host->ioaddr + SD_CE_ATA_2);
		tmp |= SDCE_MISC_INT;
		writew(tmp, host->ioaddr + SD_CE_ATA_2);
	}
	pxa->power_mode = power_mode;
}

static int pxav3_set_uhs_signaling(struct sdhci_host *host, unsigned int uhs)
{
	u16 ctrl_2;

	/*
	 * Set V18_EN -- UHS modes do not work without this.
	 * does not change signaling voltage
	 */
	ctrl_2 = sdhci_readw(host, SDHCI_HOST_CONTROL2);

	/* Select Bus Speed Mode for host */
	ctrl_2 &= ~SDHCI_CTRL_UHS_MASK;
	switch (uhs) {
	case MMC_TIMING_UHS_SDR12:
		ctrl_2 |= SDHCI_CTRL_UHS_SDR12;
		break;
	case MMC_TIMING_UHS_SDR25:
		ctrl_2 |= SDHCI_CTRL_UHS_SDR25;
		break;
	case MMC_TIMING_UHS_SDR50:
		ctrl_2 |= SDHCI_CTRL_UHS_SDR50 | SDHCI_CTRL_VDD_180;
		break;
	case MMC_TIMING_UHS_SDR104:
		ctrl_2 |= SDHCI_CTRL_UHS_SDR104 | SDHCI_CTRL_VDD_180;
		break;
	case MMC_TIMING_UHS_DDR50:
		ctrl_2 |= SDHCI_CTRL_UHS_DDR50 | SDHCI_CTRL_VDD_180;
		break;
	}

	sdhci_writew(host, ctrl_2, SDHCI_HOST_CONTROL2);
	dev_dbg(mmc_dev(host->mmc),
		"%s uhs = %d, ctrl_2 = %04X\n",
		__func__, uhs, ctrl_2);

	return 0;
}

static void pxav3_signal_vol_change(struct sdhci_host *host, u8 vol)
{
	struct platform_device *pdev = to_platform_device(mmc_dev(host->mmc));
	struct sdhci_pxa_platdata *pdata = pdev->dev.platform_data;
	unsigned int set;

	switch (vol) {
		case MMC_SIGNAL_VOLTAGE_330:
			set = 3300000;
			break;
		case MMC_SIGNAL_VOLTAGE_180:
			set = 1800000;
			break;
		case MMC_SIGNAL_VOLTAGE_120:
			set = 1200000;
			break;
		default:
			set = 3300000;
			break;
	}
	if (pdata && pdata->signal_vol_change)
		pdata->signal_vol_change(set);
}

static void pxav3_access_constrain(struct sdhci_host *host, unsigned int ac)
{
        struct platform_device *pdev = to_platform_device(mmc_dev(host->mmc));
        struct sdhci_pxa_platdata *pdata = pdev->dev.platform_data;

        if (!pdata)
                return;
        if (ac)
		pm_qos_update_request(&pdata->qos_idle,
				PM_QOS_CPUIDLE_BLOCK_AXI_VALUE);
        else
		pm_qos_update_request(&pdata->qos_idle,
				PM_QOS_CPUIDLE_BLOCK_DEFAULT_VALUE);
}

static void pxav3_clk_gate_auto(struct sdhci_host *host, unsigned int ctrl)
{
	u16 tmp;

	tmp = sdhci_readw(host, SD_FIFO_PARAM);
	tmp &= ~PAD_CLK_GATE_MASK;
	sdhci_writew(host, tmp, SD_FIFO_PARAM);

	/*
	 * FIXME: according to spec, bit SDHCI_CTRL_ASYNC_INT
	 * should be used to deliver async interrupt requested by sdio device
	 * rather than auto clock gate. But current host controller use this
	 * bit to enable/disable auto clock gate.
	 */
	tmp = sdhci_readw(host, SDHCI_HOST_CONTROL2);
	if (ctrl)
		tmp |= SDHCI_CTRL_ASYNC_INT;
	else
		tmp &= ~SDHCI_CTRL_ASYNC_INT;
	sdhci_writew(host, tmp, SDHCI_HOST_CONTROL2);
}

static void pxav3_clr_wakeup_event(struct sdhci_host *host)
{
	struct platform_device *pdev = to_platform_device(mmc_dev(host->mmc));
	struct sdhci_pxa_platdata *pdata = pdev->dev.platform_data;

	if (!pdata)
		return;

	if (pdata->clear_wakeup_event)
		pdata->clear_wakeup_event();
}

static struct sdhci_ops pxav3_sdhci_ops = {
	.get_max_clock = pxav3_get_max_clock,
	.platform_reset_exit = pxav3_set_private_registers,
	.set_uhs_signaling = pxav3_set_uhs_signaling,
	.platform_send_init_74_clocks = pxav3_gen_init_74_clocks,
	.signal_vol_change = pxav3_signal_vol_change,
	.access_constrain = pxav3_access_constrain,
	.clk_gate_auto	= pxav3_clk_gate_auto,
	.clk_prepare = pxav3_clk_prepare,
	.clr_wakeup_event = pxav3_clr_wakeup_event,
};

#ifdef CONFIG_OF
static const struct of_device_id sdhci_pxav3_of_match[] = {
	{
		.compatible = "mrvl,pxav3-mmc",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sdhci_pxav3_of_match);

static struct sdhci_pxa_platdata *pxav3_get_mmc_pdata(struct device *dev)
{
	struct sdhci_pxa_platdata *pdata;
	struct device_node *np = dev->of_node;
	u32 bus_width;
	u32 clk_delay_cycles;
	enum of_gpio_flags gpio_flags;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return NULL;

	if (of_find_property(np, "non-removable", NULL))
		pdata->flags |= PXA_FLAG_CARD_PERMANENT;

	of_property_read_u32(np, "bus-width", &bus_width);
	if (bus_width == 8)
		pdata->flags |= PXA_FLAG_SD_8_BIT_CAPABLE_SLOT;

	of_property_read_u32(np, "mrvl,clk-delay-cycles", &clk_delay_cycles);
	if (clk_delay_cycles > 0)
		pdata->clk_delay_cycles = clk_delay_cycles;

	pdata->ext_cd_gpio = of_get_named_gpio_flags(np, "cd-gpios", 0, &gpio_flags);
	if (gpio_flags != OF_GPIO_ACTIVE_LOW)
		pdata->host_caps2 |= MMC_CAP2_CD_ACTIVE_HIGH;

	return pdata;
}
#else
static inline struct sdhci_pxa_platdata *pxav3_get_mmc_pdata(struct device *dev)
{
	return NULL;
}
#endif

static int __devinit sdhci_pxav3_probe(struct platform_device *pdev)
{
	struct sdhci_pltfm_host *pltfm_host;
	struct sdhci_pxa_platdata *pdata = pdev->dev.platform_data;
	struct device *dev = &pdev->dev;
	struct sdhci_host *host = NULL;
	struct sdhci_pxa *pxa = NULL;
	const struct of_device_id *match;

	int ret;
	struct clk *clk;

	int qos_class = PM_QOS_CPUIDLE_BLOCK;

	pxa = kzalloc(sizeof(struct sdhci_pxa), GFP_KERNEL);
	if (!pxa)
		return -ENOMEM;

	host = sdhci_pltfm_init(pdev, NULL);
	if (IS_ERR(host)) {
		kfree(pxa);
		return PTR_ERR(host);
	}
	pltfm_host = sdhci_priv(host);
	pltfm_host->priv = pxa;

	clk = clk_get(dev, "PXA-SDHCLK");
	if (IS_ERR(clk)) {
		dev_err(dev, "failed to get io clock\n");
		ret = PTR_ERR(clk);
		goto err_clk_get;
	}
	pltfm_host->clk = clk;
	clk_prepare_enable(clk);

	host->quirks = SDHCI_QUIRK_DATA_TIMEOUT_USES_SDCLK
		| SDHCI_QUIRK_NO_ENDATTR_IN_NOPDESC
		| SDHCI_QUIRK_32BIT_ADMA_SIZE
		| SDHCI_QUIRK_CAP_CLOCK_BASE_BROKEN;
	host->quirks2 = SDHCI_QUIRK2_NO_CURRENT_LIMIT
		| SDHCI_QUIRK2_PRESET_VALUE_BROKEN
		| SDHCI_QUIRK2_TIMEOUT_DIVIDE_4;

	match = of_match_device(of_match_ptr(sdhci_pxav3_of_match), &pdev->dev);
	if (match)
		pdata = pxav3_get_mmc_pdata(dev);

	if (pdata) {
		pdata->qos_idle.name = mmc_hostname(host->mmc);
		pm_qos_add_request(&pdata->qos_idle, qos_class,
			PM_QOS_CPUIDLE_BLOCK_DEFAULT_VALUE);

		/* If slot design supports 8 bit data, indicate this to MMC. */
		if (pdata->flags & PXA_FLAG_SD_8_BIT_CAPABLE_SLOT)
			host->mmc->caps |= MMC_CAP_8_BIT_DATA;

		if (pdata->flags & PXA_FLAG_ENABLE_CLOCK_GATING)
			host->mmc->caps2 |= MMC_CAP2_BUS_AUTO_CLK_GATE;

		if (pdata->flags & PXA_FLAG_DISABLE_PROBE_CDSCAN)
			host->mmc->caps2 |= MMC_CAP2_DISABLE_PROBE_CDSCAN;

		if (pdata->quirks)
			host->quirks |= pdata->quirks;
		if (pdata->quirks2)
			host->quirks2 |= pdata->quirks2;
		if (pdata->host_caps)
			host->mmc->caps |= pdata->host_caps;
		if (pdata->host_caps2)
			host->mmc->caps2 |= pdata->host_caps2;
		if (pdata->pm_caps)
			host->mmc->pm_caps |= pdata->pm_caps;

		if (pdata->cd_type != PXA_SDHCI_CD_HOST)
			host->quirks |= SDHCI_QUIRK_BROKEN_CARD_DETECTION;

		if (pdata->cd_type == PXA_SDHCI_CD_GPIO &&
			gpio_is_valid(pdata->ext_cd_gpio)) {
			ret = mmc_gpio_request_cd(host->mmc, pdata->ext_cd_gpio);
			if (ret) {
				dev_err(mmc_dev(host->mmc),
					"failed to allocate card detect gpio\n");
				goto err_cd_req;
			}
		} else if (pdata->cd_type == PXA_SDHCI_CD_PERMANENT) {
			/* on-chip device */
			host->mmc->caps |= MMC_CAP_NONREMOVABLE;
		} else if (pdata->cd_type == PXA_SDHCI_CD_NONE)
			host->mmc->caps |= MMC_CAP_NEEDS_POLL;
	}
	host->ops = &pxav3_sdhci_ops;

	if (pdata && pdata->flags & PXA_FLAG_EN_PM_RUNTIME) {
		pm_runtime_set_active(&pdev->dev);
		pm_runtime_enable(&pdev->dev);
		pm_runtime_set_autosuspend_delay(&pdev->dev,
				PXAV3_RPM_DELAY_MS);
		pm_runtime_use_autosuspend(&pdev->dev);
		pm_suspend_ignore_children(&pdev->dev, 1);
	}

#ifdef _MMC_SAFE_ACCESS_
	mmc_is_available = 1;
#endif

	ret = sdhci_add_host(host);
	if (ret) {
		dev_err(&pdev->dev, "failed to add host\n");
		if (pdata && pdata->flags & PXA_FLAG_EN_PM_RUNTIME) {
			pm_runtime_forbid(&pdev->dev);
			pm_runtime_get_noresume(&pdev->dev);
		}
		goto err_add_host;
	}

	/* remove the caps that supported by the controller but not available
	 * for certain platforms.
	 */
	if (pdata && pdata->host_caps_disable)
		host->mmc->caps &= ~(pdata->host_caps_disable);

	platform_set_drvdata(pdev, host);

	if (pdata && pdata->flags & PXA_FLAG_WAKEUP_HOST) {
		device_init_wakeup(&pdev->dev, 1);
		host->mmc->pm_flags |= MMC_PM_WAKE_SDIO_IRQ;
	}
	else
		device_init_wakeup(&pdev->dev, 0);

#ifdef CONFIG_SD8XXX_RFKILL
	if (pdata && pdata->pmmc)
		*pdata->pmmc = host->mmc;
#endif

	return 0;

err_add_host:
	clk_disable_unprepare(clk);
	clk_put(clk);
	if (pdata)
		pm_qos_remove_request(&pdata->qos_idle);
err_cd_req:
	mmc_gpio_free_cd(host->mmc);
err_clk_get:
	sdhci_pltfm_free(pdev);
	kfree(pxa);
	return ret;
}

static int __devexit sdhci_pxav3_remove(struct platform_device *pdev)
{
	struct sdhci_host *host = platform_get_drvdata(pdev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_pxa *pxa = pltfm_host->priv;
	struct sdhci_pxa_platdata *pdata = pdev->dev.platform_data;

	sdhci_remove_host(host, 1);
	if (pdata && pdata->flags & PXA_FLAG_EN_PM_RUNTIME)
		pm_runtime_disable(&pdev->dev);

	if (pdata)
		pm_qos_remove_request(&pdata->qos_idle);

	clk_disable_unprepare(pltfm_host->clk);
	clk_put(pltfm_host->clk);

	if (pdata && pdata->cd_type == PXA_SDHCI_CD_GPIO &&
			gpio_is_valid(pdata->ext_cd_gpio))
		mmc_gpio_free_cd(host->mmc);

	sdhci_pltfm_free(pdev);
	kfree(pxa);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int sdhci_pxav3_suspend(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);

	return sdhci_suspend_host(host);
}

static int sdhci_pxav3_resume(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);

	return sdhci_resume_host(host);
}
#endif

#ifdef CONFIG_PM_RUNTIME
static int sdhci_pxav3_runtime_suspend(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	unsigned long flags;

	if (pltfm_host->clk) {
		/*
		 * Use flag to let sdhci.c aware this event
		 * Some SOC,like pxa988, 3 SD Hosts share the same IRQ
		 * It needs to avoid register access in shared IRQ_handle
		 */
		spin_lock_irqsave(&host->lock, flags);
		host->runtime_suspended = true;
		spin_unlock_irqrestore(&host->lock, flags);

		clk_disable_unprepare(pltfm_host->clk);
	}

	return 0;
}

static int sdhci_pxav3_runtime_resume(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	unsigned long flags;

	if (pltfm_host->clk) {
		clk_prepare_enable(pltfm_host->clk);

		spin_lock_irqsave(&host->lock, flags);
		host->runtime_suspended = false;
		spin_unlock_irqrestore(&host->lock, flags);
	}

	return 0;
}
#endif

#ifdef CONFIG_PM
static const struct dev_pm_ops sdhci_pxav3_pmops = {
	SET_SYSTEM_SLEEP_PM_OPS(sdhci_pxav3_suspend, sdhci_pxav3_resume)
	SET_RUNTIME_PM_OPS(sdhci_pxav3_runtime_suspend,
		sdhci_pxav3_runtime_resume, NULL)
};

#define SDHCI_PXAV3_PMOPS (&sdhci_pxav3_pmops)

#else
#define SDHCI_PXAV3_PMOPS NULL
#endif

static struct platform_driver sdhci_pxav3_driver = {
	.driver		= {
		.name	= "sdhci-pxav3",
#ifdef CONFIG_OF
		.of_match_table = sdhci_pxav3_of_match,
#endif
		.owner	= THIS_MODULE,
		.pm	= SDHCI_PXAV3_PMOPS,
	},
	.probe		= sdhci_pxav3_probe,
	.remove		= __devexit_p(sdhci_pxav3_remove),
};

module_platform_driver(sdhci_pxav3_driver);

MODULE_DESCRIPTION("SDHCI driver for pxav3");
MODULE_AUTHOR("Marvell International Ltd.");
MODULE_LICENSE("GPL v2");

