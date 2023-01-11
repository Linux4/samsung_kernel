/*
 * Copyright (C) 2014 Marvell International Ltd.
 *		Jialing Fu <jlfu@marvell.com>
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
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/clk/mmp_sdh_tuning.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/platform_data/pxa_sdhci.h>

#include "sdhci.h"
#include "sdhci-pltfm.h"


/*
 * BackGround:
 * For most marvell pxa soc, like Pxa988, Pxa1088, Pxa1U88...
 * SDH shares the same power suppy of vcc_main with other HW modules,
 * like CPU/DDR/GC......
 *
 * As you know, vcc_main voltage may change dynamic if
 * CPU/DDR/... freq changes, which is called as DVFS.
 *
 *
 * What issue we meet:
 * We found that SD Bus signal is sensitively with vcc_main's voltage,
 * expecailly for HS200/SDR104 mode.
 * Example: The Rx/Tx delay window would shift to right with the increase
 * of vcc_main voltage in pxa1088, pxa1L88 and pxa1U88.
 *
 *
 * What need to do:
 * So we need to block dvfs or requst constain to DVFS sometimes.
 * Example:
 * When Rx/Tx transer is executing, we may hold vcc_main voltage
 * at proper value; After tranfer is done, SDH allow vcc_main voltage
 * change freely.
 *
 *
 * Why create this file:
 * It is not a good idea that SDH driver onwer need to know/learn
 * much details about DVFS.
 * So here provide some APIs relate to DVFS for SDH.
 *
 * But how to use DVFS APIs is the responsibility fo SDH.
 *
 */

#define VL_TO_RATE(level) (1000000*((level)+1))

int pxa_sdh_create_dvfs(struct sdhci_host *host)
{
	struct platform_device *pdev = to_platform_device(mmc_dev(host->mmc));
	struct sdhci_pxa_platdata *pdata = pdev->dev.platform_data;
	struct device *dev = &pdev->dev;
	struct clk *clk;

	clk = clk_get(dev, "sdh-fclk-tuned");
	if (IS_ERR_OR_NULL(clk)) {
		pdata->fakeclk_tuned = NULL;
		pr_err("%s failed to get fackclk-tuned\n", mmc_hostname(host->mmc));
	} else
		pdata->fakeclk_tuned = clk;

	clk = clk_get(dev, "sdh-fclk-fixed");
	if (IS_ERR_OR_NULL(clk)) {
		pdata->fakeclk_fixed = NULL;
		pr_err("%s failed to get fackclk-fixed\n", mmc_hostname(host->mmc));
	} else
		pdata->fakeclk_fixed = clk;

	return 0;
}
EXPORT_SYMBOL_GPL(pxa_sdh_create_dvfs);

/*
 * It is SDH driver's responsibilty to set DVFS LEVEL
 */
int pxa_sdh_request_dvfs_level(struct sdhci_host *host, int level)
{
	struct platform_device *pdev = to_platform_device(mmc_dev(host->mmc));
	struct sdhci_pxa_platdata *pdata = pdev->dev.platform_data;

	if (!pdata || !pdata->fakeclk_tuned)
		return -ENODEV;

	clk_set_rate(pdata->fakeclk_tuned, VL_TO_RATE(level));

	pr_debug("%s set to dvfs level %d\n", mmc_hostname(host->mmc), level);
	return 0;
}
EXPORT_SYMBOL_GPL(pxa_sdh_request_dvfs_level);

int pxa_sdh_release_dvfs(struct sdhci_host *host)
{
	return pxa_sdh_request_dvfs_level(host, 0);
}
EXPORT_SYMBOL_GPL(pxa_sdh_release_dvfs);

int pxa_sdh_enable_dvfs(struct sdhci_host *host)
{
	struct platform_device *pdev = to_platform_device(mmc_dev(host->mmc));
	struct sdhci_pxa_platdata *pdata = pdev->dev.platform_data;
	unsigned char timing = host->mmc->ios.timing;

	if (!pdata)
		return -ENODEV;

	if (pdata->fakeclk_tuned) {
		if (!__clk_is_prepared(pdata->fakeclk_tuned))
			clk_prepare_enable(pdata->fakeclk_tuned);
	}

	if (pdata->fakeclk_fixed) {
		if (pdata->dtr_data[timing].fakeclk_en) {
			if (!__clk_is_prepared(pdata->fakeclk_fixed))
				clk_prepare_enable(pdata->fakeclk_fixed);
		}
	}

	return 0;
}
EXPORT_SYMBOL_GPL(pxa_sdh_enable_dvfs);

int pxa_sdh_disable_dvfs(struct sdhci_host *host)
{
	struct platform_device *pdev = to_platform_device(mmc_dev(host->mmc));
	struct sdhci_pxa_platdata *pdata = pdev->dev.platform_data;
	unsigned char timing = host->mmc->ios.timing;

	if (!pdata)
		return -ENODEV;

	if (pdata->fakeclk_tuned) {
		if (__clk_is_prepared(pdata->fakeclk_tuned))
			clk_disable_unprepare(pdata->fakeclk_tuned);
	}

	if (pdata->fakeclk_fixed) {
		if (pdata->dtr_data[timing].fakeclk_en) {
			if (__clk_is_prepared(pdata->fakeclk_fixed))
				clk_disable_unprepare(pdata->fakeclk_fixed);
		}
	}

	return 0;
}
EXPORT_SYMBOL_GPL(pxa_sdh_disable_dvfs);

int pxa_sdh_destory_dvfs(struct sdhci_host *host)
{
	struct platform_device *pdev = to_platform_device(mmc_dev(host->mmc));
	struct sdhci_pxa_platdata *pdata = pdev->dev.platform_data;

	if (!pdata)
		return -ENODEV;

	if (pdata->fakeclk_tuned)
		clk_put(pdata->fakeclk_tuned);

	if (pdata->fakeclk_fixed)
		clk_put(pdata->fakeclk_fixed);

	return 0;
}
EXPORT_SYMBOL_GPL(pxa_sdh_destory_dvfs);

int pxa_sdh_dvfs_timing_prepare(struct sdhci_host *host, unsigned char timing)
{
	struct platform_device *pdev = to_platform_device(mmc_dev(host->mmc));
	struct sdhci_pxa_platdata *pdata = pdev->dev.platform_data;
	struct sdhci_pxa_dtr_data *dtr_data;

	if (pdata && pdata->dtr_data) {
		if (timing >= MMC_TIMING_MAX) {
			pr_err("%s: invalid timing %d\n", mmc_hostname(host->mmc), timing);
			return 0;
		}

		dtr_data = &pdata->dtr_data[timing];

		if (timing != dtr_data->timing)
			return 0;

		if (pdata->fakeclk_fixed) {
			if (dtr_data->fakeclk_en && !__clk_is_prepared(pdata->fakeclk_fixed))
				clk_prepare_enable(pdata->fakeclk_fixed);
			else if (!dtr_data->fakeclk_en && __clk_is_prepared(pdata->fakeclk_fixed))
				clk_disable_unprepare(pdata->fakeclk_fixed);
		}
	}
	return 0;
}
EXPORT_SYMBOL_GPL(pxa_sdh_dvfs_timing_prepare);

/*
 * Design Modle:
 * For one kind of soc, its vcc_main has 4/8/16 differents voltage level(VL)
 * Like Pxa1088/Pxa1088LTE/Pxa1U88 have only 4 levels, Pxa1928 has 16 levels
 *
 * So use a API to get the max DVFS level.
 * TODO: remove this API if possible, we can use DTB instead.
 */
#define SDH_DVFS_DEBUG 1

int pxa_sdh_get_highest_dvfs_level(struct sdhci_host *host)
{
	int dvfs_level;

	dvfs_level = plat_get_vl_max();

	if (dvfs_level <= 0) {
		/* plat_get_vl is not supported */
		BUG();
		return 0;
	}

	return dvfs_level - 1;
}
EXPORT_SYMBOL_GPL(pxa_sdh_get_highest_dvfs_level);

int pxa_sdh_get_lowest_dvfs_level(struct sdhci_host *host)
{
	int dvfs_level;

	dvfs_level = plat_get_vl_min();

	if (dvfs_level < 0) {
		/* plat_get_vl is not supported */
		BUG();
		return 0;
	}

	return dvfs_level;
}
EXPORT_SYMBOL_GPL(pxa_sdh_get_lowest_dvfs_level);
