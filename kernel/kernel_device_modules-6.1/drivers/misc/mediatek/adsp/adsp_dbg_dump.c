// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */


#include <linux/module.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include "adsp_platform.h"
#include "adsp_platform_driver.h"
#include "adsp_feature_define.h"
#include "adsp_core.h"
#include "adsp_dbg_dump.h"
#include "clk-fmeter.h"

#define CLK_DUMP_TIME (5)
#define ADSP_FREQ_RETRY_COUNT  (10)

static bool adsp_clk_freq_init;
static void __iomem *spm_sema;
static unsigned int spm_sema_func_bit;

struct adsp_clk_fmeter {
	unsigned int id;
	unsigned int type;
	unsigned int freq;
};

struct adsp_clk_freq_dump {
	char *name;
	bool init;
	struct adsp_clk_fmeter adsp_fmeter;
};

/* clock name to read property */
static struct adsp_clk_freq_dump adsp_clk_frq[NUM_OF_ADSP_FMETER] = {
	{"adsp-fmeter-adspplldiv", false, {0, 0, 0}},
};

static void adsp_sema_lock(void)
{
	int reyry_count = ADSP_FREQ_RETRY_COUNT;

	if (!spm_sema)
		return;

	writel (0x1 << spm_sema_func_bit, spm_sema);
	while((readl(spm_sema) & (0x1 << spm_sema_func_bit)) != (0x1 << spm_sema_func_bit)
	      && reyry_count > 0) {
		writel (0x1 << spm_sema_func_bit, spm_sema);
		reyry_count--;
		if (reyry_count == 0)
			pr_info("%s err *spm_sema = 0x%x\n", __func__, readl(spm_sema));
		udelay(10);
	}
}

static void adsp_sema_unlock(void)
{
	if (!spm_sema)
		return;

	writel (0x1 << spm_sema_func_bit, spm_sema);
	if ((readl(spm_sema) & (0x1 << spm_sema_func_bit)))
		pr_info("%s err *spm_sema = 0x%x\n", __func__, readl(spm_sema));
}


int adsp_dbg_dump_init(struct platform_device *pdev)
{
	struct resource *res;
	struct device *dev;
	struct device_node *node;

	int i = 0, ret = 0;

	if (pdev == NULL)
		return -ENODEV;

	dev = &pdev->dev;
	if (dev == NULL)
		return -ENODEV;

	node = dev->of_node;
	if (node == NULL)
		return -ENODEV;

	/* get adsp using freq meter */
	for (i = 0 ; i < NUM_OF_ADSP_FMETER; i++) {
		ret = of_property_read_u32_array(node, adsp_clk_frq[i].name,
						 (u32 *)&adsp_clk_frq[i].adsp_fmeter,
						 sizeof(struct adsp_clk_fmeter) / sizeof(unsigned int));
		if (ret)
			pr_info("%s read property error name:%s err\n", __func__, adsp_clk_frq[i].name);
		else {
			adsp_clk_frq[i].init = true;
			pr_info("%s name %s id: %u type: %u freq: %u\n",
				__func__,
				adsp_clk_frq[i].name,
				adsp_clk_frq[i].adsp_fmeter.id,
				adsp_clk_frq[i].adsp_fmeter.type,
				adsp_clk_frq[i].adsp_fmeter.freq);
		}
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "spm_sema_m1");
	if (!res) {
		pr_info("%s get resource spm_sema_m1 fail\n", __func__);
		goto EXIT;
	}

	spm_sema = devm_ioremap(dev, res->start, resource_size(res));
	if (unlikely(!spm_sema)) {
		pr_info("%s get ioremap IPIC_CHN fail 0x%llx\n", __func__, res->start);
		goto EXIT;
	}

	ret = of_property_read_u32(dev->of_node, "adsp-pll-sema-bits", &spm_sema_func_bit);
	if (ret) {
		pr_info("%s get adsp-pll-sema-bits fail\n", __func__);
		goto EXIT;
	}

	adsp_clk_freq_init = true;
	return 0;
EXIT:
	return -1;
}
EXPORT_SYMBOL(adsp_dbg_dump_init);

/* return false if freq is not expect */
static bool adsp_freq_compare(unsigned int target_freq, unsigned int meter_freq)
{
	unsigned int diff = 0;

	if (target_freq == 0 || meter_freq == 0)
		return false;

	diff =  abs(target_freq - meter_freq);
	if (meter_freq > target_freq * 10 / 8){
		pr_info("%s target_freq = %u meter_freq = %u diff = %u\n",
			__func__, target_freq, meter_freq, diff);
		return false;
	}

	return true;
}

static unsigned int adsp_get_freq(unsigned int id, unsigned int type)
{
	unsigned int freq = 0;

	adsp_sema_lock();
	freq = mt_get_fmeter_freq(id, type);
	adsp_sema_unlock();

	return freq;
}

/*
 * check adsp pll freq return true if correct
 * return true if freq is okay
 */
bool adsp_check_adsppll_freq(unsigned int adsp_meter_type)
{
	unsigned int freq, retry_count = ADSP_FREQ_RETRY_COUNT;
	bool freq_check;

	if (adsp_clk_freq_init == false) {
		pr_info("%s adsp_clk_freq_init = %u\n", __func__, adsp_clk_freq_init);
		return false;
	}

	if (!spm_sema)
		return false;

	if (adsp_meter_type >= NUM_OF_ADSP_FMETER)
		return false;

	if (adsp_clk_frq[adsp_meter_type].init == false)
		return false;

	do {
		freq = adsp_get_freq(adsp_clk_frq[adsp_meter_type].adsp_fmeter.id,
				     adsp_clk_frq[adsp_meter_type].adsp_fmeter.type);
		freq_check = adsp_freq_compare(adsp_clk_frq[adsp_meter_type].adsp_fmeter.freq, freq);
		pr_info("%s freq = %u adsp_meter_type = %u", __func__, freq, adsp_meter_type);
		retry_count--;
	} while((freq_check == false) && retry_count);

	return freq_check;
}
EXPORT_SYMBOL(adsp_check_adsppll_freq);

