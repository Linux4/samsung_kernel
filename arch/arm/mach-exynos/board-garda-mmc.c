/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/gpio.h>
#include <linux/mmc/host.h>
#include <linux/delay.h>

#include <plat/gpio-cfg.h>
#include <plat/cpu.h>
#include <plat/clock.h>
#include <plat/devs.h>

#include <mach/dwmci.h>
#include <mach/gpio.h>
#include <mach/gpio-shannon222ap.h>

#include "board-universal222ap.h"

#define MMC_PWR_EN		EXYNOS4_GPK0(2)
#define SDCARD_PWR_EN		EXYNOS4_GPY0(0)

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

static void exynos_dwmci_save_drv_st(void *data, u32 slot_id)
{
	struct dw_mci *host = (struct dw_mci *)data;
	struct drv_strength * drv_st = &host->pdata->__drv_st;

	drv_st->val = s5p_gpio_get_drvstr(drv_st->pin);
}

static void exynos_dwmci_restore_drv_st(void *data, u32 slot_id, int *compensation)
{
	struct dw_mci *host = (struct dw_mci *)data;
	struct drv_strength * drv_st = &host->pdata->__drv_st;

	*compensation = 0;

	s5p_gpio_set_drvstr(drv_st->pin, drv_st->val);
}

static void exynos_dwmci_tuning_drv_st(void *data, u32 slot_id)
{
	struct dw_mci *host = (struct dw_mci *)data;
	struct drv_strength * drv_st = &host->pdata->__drv_st;
	unsigned int gpio = drv_st->pin;
	s5p_gpio_drvstr_t next_ds[4] = {S5P_GPIO_DRVSTR_LV2,   /* LV1 -> LV2 */
					S5P_GPIO_DRVSTR_LV4,   /* LV3 -> LV4 */
					S5P_GPIO_DRVSTR_LV3,   /* LV2 -> LV3 */
					S5P_GPIO_DRVSTR_LV1};  /* LV4 -> LV1 */

	s5p_gpio_set_drvstr(gpio, next_ds[s5p_gpio_get_drvstr(gpio)]);
}

static int exynos_dwmci0_get_bus_wd(u32 slot_id)
{
	return 8;
}

static void exynos_dwmci0_set_power(unsigned int power)
{
	unsigned int gpio;

	gpio = EXYNOS4_GPK0(2); /*MMC_PWR_EN*/

	if (power) {
		if (!gpio_get_value(gpio)) {
			s5p_gpio_set_data(gpio, S5P_GPIO_DATA1);
			s3c_gpio_cfgpin(gpio, S3C_GPIO_OUTPUT);
			pr_info("set eMMC power %s.\n",
				gpio_get_value(gpio) ? "on" : "off");
		}
	} else {
		if (gpio_get_value(gpio)) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(1));
			s5p_gpio_set_data(gpio, S5P_GPIO_DATA0);
			pr_info("set eMMC power %s.\n",
					gpio_get_value(gpio) ? "on" : "off");
		}
	}
}


static void exynos_dwmci0_cfg_gpio(int width)
{
	unsigned int gpio;

	gpio = EXYNOS4_GPK0(2); /*MMC_PWR_EN*/
	s3c_gpio_cfgpin(gpio, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
	s5p_gpio_set_pd_cfg(gpio, S5P_GPIO_PD_OUTPUT0);
	
	for (gpio = EXYNOS4_GPK0(0);
			gpio < EXYNOS4_GPK0(2); gpio++) {
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
		s5p_gpio_set_pd_cfg(gpio, S5P_GPIO_PD_OUTPUT0);
	}

	switch (width) {
	case 8:
		for (gpio = EXYNOS4_GPL0(0);
				gpio <= EXYNOS4_GPL0(3); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
			s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
			s5p_gpio_set_pd_cfg(gpio, S5P_GPIO_PD_OUTPUT0);
		}
	case 4:
		for (gpio = EXYNOS4_GPK0(3);
				gpio <= EXYNOS4_GPK0(6); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
			s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
			s5p_gpio_set_pd_cfg(gpio, S5P_GPIO_PD_OUTPUT0);
		}
		break;
	case 1:
		gpio = EXYNOS4_GPK0(3);
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
		s5p_gpio_set_pd_cfg(gpio, S5P_GPIO_PD_OUTPUT0);
		break;
	case 0:
		for (gpio = EXYNOS4_GPK0(0); gpio < EXYNOS4_GPK0(2); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_INPUT);
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
		}
		for (gpio = EXYNOS4_GPK0(3); gpio <= EXYNOS4_GPK0(6); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_INPUT);
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
		}
		for (gpio = EXYNOS4_GPL0(0); gpio <= EXYNOS4_GPL0(3); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_INPUT);
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
		}
		gpio = MMC_PWR_EN;
		if (gpio_get_value(gpio)) {
			gpio_set_value(gpio, 0);
			pr_info("internal MMC(eMMC) card Off.\n");
		}
		return;
	default:
		break;
	}

	gpio = EXYNOS4_GPK0(7);
	s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_DOWN);
	s5p_gpio_set_pd_cfg(gpio, S5P_GPIO_PD_OUTPUT0);

	/* eMMC on */
	gpio = MMC_PWR_EN;
	if (!gpio_get_value(gpio)) {
		pr_info("internal MMC(eMMC) card On.\n");
		gpio_set_value(gpio, 1);
	}
}

static int exynos_dwmci0_init(u32 slot_id, irq_handler_t handler, void *data)
{
	s3c_gpio_cfgpin(MMC_PWR_EN, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(MMC_PWR_EN, S3C_GPIO_PULL_NONE);

	return 0;
}

static struct dw_mci_board smdk4270_dwmci0_pdata __initdata = {
	.num_slots		= 1,
	.quirks			= DW_MCI_QUIRK_BROKEN_CARD_DETECTION |
				  DW_MCI_QUIRK_HIGHSPEED |
				  DW_MMC_QUIRK_FIXED_VOLTAGE |
				  DW_MMC_QUIRK_RETRY_CRC_ERROR,
	.bus_hz			= 200 * 1000 * 1000,
	.caps			= MMC_CAP_CMD23 | MMC_CAP_8_BIT_DATA |
				  MMC_CAP_UHS_DDR50 | MMC_CAP_1_8V_DDR |
  				  MMC_CAP_ERASE,
	.caps2			= MMC_CAP2_BROKEN_VOLTAGE |
				  MMC_CAP2_POWEROFF_NOTIFY | MMC_CAP2_NO_SLEEP_CMD,
	.fifo_depth		= 0x80,
	.detect_delay_ms	= 200,
	.hclk_name		= "dwmci",
	.cclk_name		= "sclk_dwmci",
	.cfg_gpio		= exynos_dwmci0_cfg_gpio,
	.get_bus_wd		= exynos_dwmci0_get_bus_wd,
	.set_power		= exynos_dwmci0_set_power,
	.cd_type		= DW_MCI_CD_PERMANENT,
	.sdr_timing		= 0x03020001,
	.ddr_timing		= 0x03030002,
	.clk_drv		= 0x3,
	.ddr200_timing		= 0x01040002,
	.clk_tbl		= exynos_dwmci_clk_rates,
	.init			= exynos_dwmci0_init,
};

static int exynos_dwmci_get_cd(u32 slot_id)
{
	return gpio_get_value(GPIO_SDHI0_CD);
}

#if defined(CONFIG_MACH_DELOSLTE_KOR_SKT) || defined(CONFIG_MACH_DELOSLTE_KOR_KT) \
|| defined(CONFIG_MACH_DELOSLTE_KOR_LGT)
extern unsigned int system_rev;
#endif

static void exynos_dwmci2_cfg_gpio(int width)
{
	unsigned int gpio;


	for (gpio = EXYNOS4_GPK2(0); gpio < EXYNOS4_GPK2(2); gpio++) {
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
		s5p_gpio_set_pd_cfg(gpio, S5P_GPIO_PD_OUTPUT0);
	}

	switch (width) {
	case 4:
		for (gpio = EXYNOS4_GPK2(3);
				gpio <= EXYNOS4_GPK2(6); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
#if defined(CONFIG_MACH_DELOSLTE_KOR_SKT) || defined(CONFIG_MACH_DELOSLTE_KOR_KT) \
|| defined(CONFIG_MACH_DELOSLTE_KOR_LGT)
			if (system_rev < 10)
				s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
			else
#endif
				s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
			s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
			s5p_gpio_set_pd_cfg(gpio, S5P_GPIO_PD_OUTPUT0);
		}
		break;
	case 1:
		gpio = EXYNOS4_GPK2(3);
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
		s5p_gpio_set_pd_cfg(gpio, S5P_GPIO_PD_OUTPUT0);
		break;		
	case 0:
		for (gpio = EXYNOS4_GPK2(0); gpio < EXYNOS4_GPK2(2); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_INPUT);
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
		}
		for (gpio = EXYNOS4_GPK2(3); gpio <= EXYNOS4_GPK2(6); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_INPUT);
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
		}
		gpio = SDCARD_PWR_EN;
		if (gpio_get_value(gpio)) {
			gpio_set_value(gpio, 0);
			pr_info("external MMC(SD) card Off.\n");
		}
		return;
	default:
		break;
	}

	gpio = SDCARD_PWR_EN;
	if (!gpio_get_value(gpio)) {
		pr_info("external MMC(SD) card On.\n");
		gpio_set_value(gpio, 1);
	}
}

static int exynos_dwmci2_get_bus_wd(u32 slot_id)
{
	return 4;
}

static void exynos_dwmci2_set_power(unsigned int power)
{
	unsigned int gpio, en_gpio;

	en_gpio = EXYNOS4_GPY0(0); /*SDCARD_PWR_EN*/

	if (power) {
		if (!gpio_get_value(en_gpio)) {
			/* set to func. before power on */
			for (gpio = EXYNOS4_GPK2(0); gpio < EXYNOS4_GPK2(2); gpio++)
				s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));

			s5p_gpio_set_data(en_gpio, S5P_GPIO_DATA1);
			s3c_gpio_cfgpin(en_gpio, S3C_GPIO_SFN(1));
			pr_info("set external MMC(SD) power %s.\n",
				gpio_get_value(en_gpio) ? "on" : "off");
		}
	} else {
		if (gpio_get_value(en_gpio)) {
			s3c_gpio_cfgpin(en_gpio, S3C_GPIO_SFN(1));
			s5p_gpio_set_data(en_gpio, S5P_GPIO_DATA0);
			pr_info("set external MMC(SD) power %s.\n",
					gpio_get_value(en_gpio) ? "on" : "off");

			/* set to output after power off */
			for (gpio = EXYNOS4_GPK2(0); gpio < EXYNOS4_GPK2(2); gpio++)
				s3c_gpio_cfgpin(gpio, S3C_GPIO_OUTPUT);
		}
	}
}

static int smdk4270_dwmci_get_ro(u32 slot_id)
{
	/* smdk4270 did not support SD/MMC card write pritection pin. */
	return 0;
}

extern struct class *sec_class;
static struct device *sd_detection_cmd_dev;

static ssize_t sd_detection_cmd_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dw_mci_board *sc = dev_get_drvdata(dev);
	unsigned int detect;

	if (sc && sc->ext_cd_gpio)
		detect = gpio_get_value(sc->ext_cd_gpio);
	else {
		pr_info("%s : External SD detect pin Error\n", __func__);
		return  sprintf(buf, "Error\n");
	}

	pr_info("%s : detect = %d.\n", __func__,  !detect);
	if (!detect) {
		pr_debug("sd: card inserted.\n");
		return sprintf(buf, "Insert\n");
	} else {
		pr_debug("sd: card removed.\n");
		return sprintf(buf, "Remove\n");
	}
}

static DEVICE_ATTR(status, 0444, sd_detection_cmd_show, NULL);

static struct dw_mci *host2;

static int exynos_dwmci2_init(u32 slot_id, irq_handler_t handler, void *data)
{
	struct dw_mci *host = (struct dw_mci *) data;
	struct dw_mci_board *pdata = host->pdata;
	struct device *dev = &host->dev;

	host2 = host;


	if (host->pdata->cd_type == DW_MCI_CD_GPIO &&
		gpio_is_valid(host->pdata->ext_cd_gpio)) {

		s3c_gpio_cfgpin(host->pdata->ext_cd_gpio, S3C_GPIO_SFN(2));
		s3c_gpio_setpull(host->pdata->ext_cd_gpio, S3C_GPIO_PULL_UP);

		if (sd_detection_cmd_dev == NULL &&
				host->pdata->ext_cd_gpio) {
			sd_detection_cmd_dev =
				device_create(sec_class, NULL, 0,
					NULL, "sdcard");
				if (IS_ERR(sd_detection_cmd_dev))
					pr_err("Fail to create sysfs dev\n");

				if (device_create_file(sd_detection_cmd_dev,
							&dev_attr_status) < 0)
					pr_err("Fail to create sysfs file\n");

				dev_set_drvdata(sd_detection_cmd_dev, pdata);
		}

		if (gpio_request(pdata->ext_cd_gpio, "DWMCI EXT CD") == 0) {
			host->ext_cd_irq = gpio_to_irq(pdata->ext_cd_gpio);
			if (host->ext_cd_irq &&
			    request_threaded_irq(host->ext_cd_irq, NULL,
					handler,
					IRQF_TRIGGER_RISING |
					IRQF_TRIGGER_FALLING,
					dev_name(dev), host) == 0) {
				dev_warn(dev, "success to request irq for card detect.\n");
				enable_irq_wake(host->ext_cd_irq);
			} else {
				dev_warn(dev, "cannot request irq for card detect.\n");
				host->ext_cd_irq = 0;
			}
			gpio_free(pdata->ext_cd_gpio);
		} else {
			dev_err(dev, "cannot request gpio for card detect.\n");
		}
	}

	return 0;
}

static void exynos_dwmci2_exit(u32 slot_id)
{
	struct dw_mci *host = host2;

	if (host->ext_cd_irq)
		free_irq(host->ext_cd_irq, host);
}

static struct dw_mci_board smdk4270_dwmci2_pdata __initdata = {
	.num_slots		= 1,
	.quirks			= DW_MCI_QUIRK_HIGHSPEED |
				      DW_MMC_QUIRK_NO_VOLSW_INT |
				      DW_MMC_QUIRK_USE_FINE_TUNING,
	.bus_hz			= 200 * 1000 * 1000,
	.caps			= MMC_CAP_4_BIT_DATA |
				  MMC_CAP_SD_HIGHSPEED |
				  MMC_CAP_MMC_HIGHSPEED |
				  MMC_CAP_UHS_SDR50,
	.caps2			= MMC_CAP2_DETECT_ON_ERR,
	.fifo_depth		= 0x80,
	.detect_delay_ms	= 200,
	.hclk_name		= "dwmci",
	.cclk_name		= "sclk_dwmci",
	.cfg_gpio		= exynos_dwmci2_cfg_gpio,
	.get_bus_wd		= exynos_dwmci2_get_bus_wd,
	.save_drv_st		= exynos_dwmci_save_drv_st,
	.restore_drv_st 	= exynos_dwmci_restore_drv_st,
	.tuning_drv_st		= exynos_dwmci_tuning_drv_st,
	.set_power		= exynos_dwmci2_set_power,
	.sdr_timing		= 0x03020001,
	.ddr_timing		= 0x03030002,
	.clk_drv		= 0x3,
	.cd_type		= DW_MCI_CD_GPIO,
	.clk_tbl                = exynos_dwmci_clk_rates,
	.__drv_st		= {
		.pin		= EXYNOS4_GPK2(0),
		.val		= S5P_GPIO_DRVSTR_LV3,
	},
    .ignore_phase		= (1 << 7),
	.get_ro			= smdk4270_dwmci_get_ro,
	.get_cd			= exynos_dwmci_get_cd,
	.ext_cd_gpio		= GPIO_SDHI0_CD,
	.init			= exynos_dwmci2_init,
	.exit			= exynos_dwmci2_exit,
};

void __init exynos4_smdk4270_mmc0_init(void)
{
#ifdef CONFIG_MMC_DW
#ifndef CONFIG_EXYNOS_EMMC_HS200
	smdk4270_dwmci0_pdata.caps2 &=
		~MMC_CAP2_HS200_1_8V_SDR;
#endif
	exynos_dwmci_set_platdata(&smdk4270_dwmci0_pdata, 0);
	dev_set_name(&exynos4_device_dwmci0.dev, "exynos4-sdhci.0");
	clk_add_alias("dwmci", "dw_mmc.0", "hsmmc", &exynos4_device_dwmci0.dev);
	clk_add_alias("sclk_dwmci", "dw_mmc.0", "sclk_mmc",
		&exynos4_device_dwmci0.dev);

	platform_device_register(&exynos4_device_dwmci0);
#endif
}

void __init exynos4_smdk4270_mmc2_init(void)
{
#ifdef CONFIG_MMC_DW
	exynos_dwmci_set_platdata(&smdk4270_dwmci2_pdata, 2);
	dev_set_name(&exynos4_device_dwmci2.dev, "exynos4-sdhci.2");
	clk_add_alias("dwmci", "dw_mmc.2", "hsmmc", &exynos4_device_dwmci2.dev);
	clk_add_alias("sclk_dwmci", "dw_mmc.2", "sclk_mmc",
		&exynos4_device_dwmci2.dev);
	platform_device_register(&exynos4_device_dwmci2);
#endif
}
