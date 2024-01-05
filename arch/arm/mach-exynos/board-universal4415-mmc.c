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
#include <linux/clk.h>
#include <linux/module.h>

#include <plat/gpio-cfg.h>
#include <plat/cpu.h>
#include <plat/clock.h>
#include <plat/devs.h>
#include <plat/sdhci.h>

#include <mach/dwmci.h>

#include <linux/regulator/consumer.h>

#include "board-universal4415.h"

#define GPIO_T_FLASH_DETECT	EXYNOS4_GPX3(4)
#define GPIO_VQMMC_EN		EXYNOS4_GPY0(2)
#define GPIO_VMMC_EN		EXYNOS4_GPY0(3)

static struct dw_mci_clk exynos_dwmci_clk_rates_for_epll[] = {
	{786432000 / 32  , 786432000 / 8  },
	{786432000 / 16  , 786432000 / 4  },
	{786432000 / 16  , 786432000 / 4  },
	{786432000 / 8   , 786432000 / 2  },
	{786432000 / 4   , 786432000      },
	{786432000 / 8   , 786432000 / 2  },
	{786432000 / 4   , 786432000      },
	{786432000 / 2   , 786432000      },
	{786432000 / 2000, 786432000 / 500},

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
	s5p_gpio_drvstr_t next_ds[4] = {S5P_GPIO_DRVSTR_LV4,   /* LV1 -> LV4 */
					S5P_GPIO_DRVSTR_LV2,   /* LV3 -> LV2 */
					S5P_GPIO_DRVSTR_LV1,   /* LV2 -> LV1 */
					S5P_GPIO_DRVSTR_LV3};  /* LV4 -> LV3 */

	s5p_gpio_set_drvstr(gpio, next_ds[s5p_gpio_get_drvstr(gpio)]);
}

static s8 exynos_dwmci0_extra_tuning(u8 map)
{
	s8 sel = -1;

	if ((map & 0x03) == 0x03)
		sel = 0;
	else if ((map & 0x0c) == 0x0c)
		sel = 3;
	else if ((map & 0x06) == 0x06)
		sel = 2;

	return sel;
}

static int exynos_dwmci0_get_bus_wd(u32 slot_id)
{
	return 8;
}

static void exynos_dwmci0_cfg_gpio(int width)
{
	unsigned int gpio;

	/* eMMC : CLK / CMD */
	for (gpio = EXYNOS4_GPK0(0);
			gpio < EXYNOS4_GPK0(2); gpio++) {
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV3);
	}

	switch (width) {
	case 8:
		for (gpio = EXYNOS4_GPL0(0);
				gpio <= EXYNOS4_GPL0(3); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
			s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV3);
		}
	case 4:
		for (gpio = EXYNOS4_GPK0(3);
				gpio <= EXYNOS4_GPK0(6); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
			s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV3);
		}
		break;
	case 1:
		gpio = EXYNOS4_GPK0(3);
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV3);
		break;
	case 0:
		for (gpio = EXYNOS4_GPK0(0);
				gpio <= EXYNOS4_GPK0(6); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_OUTPUT);
			gpio_set_value(gpio, 0);
		}
		for (gpio = EXYNOS4_GPL0(0);
				gpio <= EXYNOS4_GPL0(3); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_OUTPUT);
			gpio_set_value(gpio, 0);
		}
		mdelay(50);
		return;
	default:
		break;
	}

	gpio = EXYNOS4_GPK0(7);
	s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_DOWN);

	/* CDn PIN: Ouput / High */
	gpio = EXYNOS4_GPK0(2);
	s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(1));
	gpio_set_value(gpio, 1);
}

static struct dw_mci_board universal4415_dwmci0_pdata __initdata = {
	.num_slots		= 1,
	.ch_num			= 0,
	.quirks			= DW_MCI_QUIRK_BROKEN_CARD_DETECTION |
				  DW_MCI_QUIRK_HIGHSPEED |
				  DW_MCI_QUIRK_NO_DETECT_EBIT |
				  DW_MMC_QUIRK_FIXED_VOLTAGE |
				  DW_MMC_QUIRK_USE_FINE_TUNING,
	.bus_hz			= 786432000 / 4,
	.caps			= MMC_CAP_CMD23 | MMC_CAP_8_BIT_DATA |
				  MMC_CAP_UHS_DDR50 | MMC_CAP_1_8V_DDR |
				  MMC_CAP_ERASE,
	.caps2			= MMC_CAP2_HS200_1_8V_SDR | MMC_CAP2_HS200_1_8V_DDR |
				  MMC_CAP2_CACHE_CTRL | MMC_CAP2_NO_SLEEP_CMD |
				  MMC_CAP2_POWEROFF_NOTIFY,
	.fifo_depth		= 0x80,
	.detect_delay_ms	= 200,
	.only_once_tune		= true,
	.hclk_name		= "hsmmc",
	.cclk_name		= "sclk_mmc",
	.cfg_gpio		= exynos_dwmci0_cfg_gpio,
	.get_bus_wd		= exynos_dwmci0_get_bus_wd,
	.save_drv_st		= exynos_dwmci_save_drv_st,
	.restore_drv_st		= exynos_dwmci_restore_drv_st,
	.tuning_drv_st		= exynos_dwmci_tuning_drv_st,
	.sdr_timing		= 0x03020000,
	.ddr_timing		= 0x03040002,
	.clk_drv		= 0x3,
	.ddr200_timing		= 0x01020000,
	.delay_line		= 0x1c,
	.ignore_phase		= (1 << 7),
	.extra_tuning           = exynos_dwmci0_extra_tuning,
	.clk_tbl		= exynos_dwmci_clk_rates_for_epll,
	.qos_int_level          = 200000,
	.__drv_st		= {
		.pin			= EXYNOS4_GPK0(0),
		.val			= S5P_GPIO_DRVSTR_LV3,
	},
	.register_notifier	= exynos_register_notifier,
};

static int exynos_dwmci1_get_bus_wd(u32 slot_id)
{
	return 4;
}

static void exynos_dwmci1_cfg_gpio(int width)
{
	unsigned int gpio;

	for (gpio = EXYNOS4_GPK1(0); gpio < EXYNOS4_GPK1(3); gpio++) {
		if (gpio == EXYNOS4_GPK1(2))
			continue;
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
		if (gpio == EXYNOS4_GPK1(0))
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
		else
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV2);
	}

	switch (width) {
	case 8:
	case 4:
		for (gpio = EXYNOS4_GPK1(3);
				gpio <= EXYNOS4_GPK1(6); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
			s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV2);
		}
		break;
	case 1:
		gpio = EXYNOS4_GPK1(3);
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV2);
	default:
		break;
	}
}


static void (*wlan_notify_func)(struct platform_device *dev, int state);
static DEFINE_MUTEX(wlan_mutex_lock);
#if defined(CONFIG_BCM4334) || defined(CONFIG_BCM4334_MODULE)
extern void *wifi_mmc_host;
#endif /* CONFIG_BCM4334 || CONFIG_BCM4334_MODULE */

#if defined(CONFIG_BCM4334) || defined(CONFIG_BCM4334_MODULE)
static int ext_cd_init_wlan(
	void (*notify_func)(struct platform_device *dev, int state),void *mmc_host)
#else /* CONFIG_BCM4334 || CONFIG_BCM4334_MODULE */
static int ext_cd_init_wlan(
	void (*notify_func)(struct platform_device *dev, int state))
#endif /* CONFIG_BCM4334 || CONFIG_BCM4334_MODULE */
{
	mutex_lock(&wlan_mutex_lock);
	WARN_ON(wlan_notify_func);
	wlan_notify_func = notify_func;
#if defined(CONFIG_BCM4334) || defined(CONFIG_BCM4334_MODULE)
	wifi_mmc_host = mmc_host;
#endif /* CONFIG_BCM4334 || CONFIG_BCM4334_MODULE */
	mutex_unlock(&wlan_mutex_lock);

	return 0;
}

static int ext_cd_cleanup_wlan(
	void (*notify_func)(struct platform_device *dev, int state))
{
	mutex_lock(&wlan_mutex_lock);
	WARN_ON(wlan_notify_func);
	wlan_notify_func = NULL;
	mutex_unlock(&wlan_mutex_lock);

	return 0;
}

void mmc_force_presence_change(struct platform_device *pdev, int val)
{
	void (*notify_func)(struct platform_device *, int state) = NULL;

	mutex_lock(&wlan_mutex_lock);

	if (pdev == &exynos4_device_dwmci1) {
		pr_err("%s: called for device exynos4_device_dwmci1\n", __func__);
		notify_func = wlan_notify_func;
	} else
		pr_err("%s: called for device with no notifier, t\n", __func__);


	if (notify_func)
		notify_func(pdev, val);
	else
		pr_err("%s: called for device with no notifier\n", __func__);

	mutex_unlock(&wlan_mutex_lock);
}
EXPORT_SYMBOL_GPL(mmc_force_presence_change);

#if defined(CONFIG_ARM_EXYNOS4415_BUS_DEVFREQ)
#if defined(CONFIG_BCM4334)
static struct dw_mci_mon_table exynos_dwmci_tp_mon1_tbl[] = {
        /* Byte/s, MIF clk, CPU clk */
        {  6000000, 400000, 1200000},
        {  3000000,      0,  800000},
        {        0,      0,       0},
};
#endif
#endif

static struct dw_mci_board universal4415_dwmci1_pdata __initdata = {
	.num_slots		= 1,
	.ch_num			= 1,
	.quirks			= DW_MCI_QUIRK_HIGHSPEED,
#if defined(CONFIG_BCM4334) || defined(CONFIG_BCM4334_MODULE)
	.bus_hz			= 50 * 1000 * 1000,
#else /* CONFIG_BCM4334 || CONFIG_BCM4334_MODULE */
	.bus_hz			= 786432000 /4,
#endif /* CONFIG_BCM4334 || CONFIG_BCM4334_MODULE */

#if defined(CONFIG_BCM4334) || defined(CONFIG_BCM4334_MODULE)
	.caps			= MMC_CAP_SD_HIGHSPEED | MMC_CAP_4_BIT_DATA,
#else /* CONFIG_BCM4334 | CONFIG_BCM4334_MODULE*/
	.caps			= MMC_CAP_UHS_SDR104 |
				  MMC_CAP_SD_HIGHSPEED | MMC_CAP_4_BIT_DATA,
#endif /* CONFIG_BCM4334 | CONFIG_BCM4334_MODULE*/
	.caps2			= MMC_CAP2_BROKEN_VOLTAGE,
	.pm_caps		= MMC_PM_KEEP_POWER | MMC_PM_IGNORE_PM_NOTIFY,
	.fifo_depth		= 0x100,
	.detect_delay_ms	= 200,
	.only_once_tune		= true,
	.hclk_name		= "hsmmc",
	.cclk_name		= "sclk_mmc",
	.cfg_gpio		= exynos_dwmci1_cfg_gpio,
	.get_bus_wd		= exynos_dwmci1_get_bus_wd,
	.ext_cd_init		= ext_cd_init_wlan,
	.ext_cd_cleanup		= ext_cd_cleanup_wlan,
	.cd_type		= DW_MCI_CD_EXTERNAL,
	.sdr_timing		= 0x03020001,
	.ddr_timing		= 0x03020001,
	.clk_drv		= 0x2,
	.clk_tbl		= exynos_dwmci_clk_rates_for_epll,
	.register_notifier	= exynos_register_notifier,
#if defined(CONFIG_ARM_EXYNOS4415_BUS_DEVFREQ)
	.tp_mon_tbl		= exynos_dwmci_tp_mon1_tbl,
#endif
};

static int exynos_dwmci2_get_cd(u32 slot_id)
{
	return gpio_get_value(GPIO_T_FLASH_DETECT);
}

static int exynos_dwmci2_get_ro(u32 slot_id)
{
	/* universal4415 does not support card write protection checking */
	return 0;
}

static void exynos_dwmci2_cfg_gpio(int width)
{
	unsigned int gpio;

	/* SD : CLK / CMD */
	for (gpio = EXYNOS4_GPK2(0); gpio < EXYNOS4_GPK2(2); gpio++) {
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV3);
	}

	switch (width) {
	case 4:
		for (gpio = EXYNOS4_GPK2(3);
				gpio <= EXYNOS4_GPK2(6); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
			s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV3);
		}
		break;
	case 1:
		gpio = EXYNOS4_GPK2(3);
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV3);
	default:
		break;
	}
}

static int exynos_dwmci2_get_bus_wd(u32 slot_id)
{
	return 4;
}

extern struct class *sec_class;
static struct device *sd_detection_cmd_dev;

static ssize_t sd_detection_cmd_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dw_mci *host = dev_get_drvdata(dev);
	unsigned int detect;

	if (host && gpio_is_valid(host->pdata->ext_cd_gpio))
		detect = gpio_get_value(host->pdata->ext_cd_gpio);
	else {
		pr_info("%s : External SD detect pin Error\n", __func__);
		return  sprintf(buf, "Error\n");
	}

	pr_info("%s : detect = %d.\n", __func__,  !detect);
	if (!detect) {
		pr_debug("dw_mmc: card inserted.\n");
		return sprintf(buf, "Insert\n");
	} else {
		pr_debug("dw_mmc: card removed.\n");
		return sprintf(buf, "Remove\n");
	}
}

static DEVICE_ATTR(status, 0444, sd_detection_cmd_show, NULL);

struct dw_mci *host2;

/*
 * Exynos4415 + PMIC : S5M8767
 * SDcard power is contolled by GPIO settings only.
 */
static void exynos_dwmci2_ext_setpower(void *data, u32 flag)
{
	struct dw_mci *host = (struct dw_mci *)data;
	struct device *dev = &host->dev;
	unsigned int gpio;

	/* host->vmmc : SDcard power */
	/* host->vqmmc : SDcard I/F power */
	if (!host->vmmc)
		return;

	if (flag & DW_MMC_EXT_VMMC_ON) {
		/* set to func. pin for SD I/F */
		for (gpio = EXYNOS4_GPK2(0); gpio < EXYNOS4_GPK2(2); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
		}
		if (!regulator_is_enabled(host->vmmc))
			regulator_enable(host->vmmc);
	}

	if (flag & DW_MMC_EXT_VQMMC_ON) {
		if (!regulator_is_enabled(host->vqmmc))
			regulator_enable(host->vqmmc);
	}

	if (!(flag & DW_MMC_EXT_VQMMC_ON)) {
		if (regulator_is_enabled(host->vqmmc))
			regulator_disable(host->vqmmc);
	}

	if (!(flag & DW_MMC_EXT_VMMC_ON)) {
		if (regulator_is_enabled(host->vmmc))
			regulator_disable(host->vmmc);
		/* set to output pin */
		for (gpio = EXYNOS4_GPK2(0); gpio < EXYNOS4_GPK2(2); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_OUTPUT);
		}
	}
	dev_info(dev, "vmmc %s, vqmmc %s.\n",
			regulator_is_enabled(host->vmmc) ? "on" : "off",
			regulator_is_enabled(host->vqmmc) ? "on" : "off");
}

static int exynos_dwmci2_init(u32 slot_id, irq_handler_t handler, void *data)
{
	struct dw_mci *host = (struct dw_mci *)data;
	struct device *dev = &host->dev;

	host2 = host;

	/* TF_EN pin */
	if (host->vmmc) {
		s3c_gpio_cfgpin(GPIO_VMMC_EN, S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(GPIO_VMMC_EN, S3C_GPIO_PULL_NONE);
		gpio_set_value(GPIO_VMMC_EN, 0);
	}

	/* MMC2_EN pin */
	if (host->vqmmc) {
		s3c_gpio_cfgpin(GPIO_VQMMC_EN, S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(GPIO_VQMMC_EN, S3C_GPIO_PULL_NONE);
		gpio_set_value(GPIO_VQMMC_EN, 0);
	}

	if (host->pdata->cd_type == DW_MCI_CD_GPIO &&
		gpio_is_valid(GPIO_T_FLASH_DETECT)) {

		host->pdata->ext_cd_gpio = GPIO_T_FLASH_DETECT;

		s3c_gpio_setpull(GPIO_T_FLASH_DETECT, S3C_GPIO_PULL_NONE);

		if (gpio_request(GPIO_T_FLASH_DETECT, "DWMCI EXT CD") == 0) {
			host->ext_cd_irq = gpio_to_irq(GPIO_T_FLASH_DETECT);
			if (host->ext_cd_irq &&
			    request_threaded_irq(host->ext_cd_irq, NULL,
						 handler,
						 IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
						 dev_name(dev), host) == 0) {
				dev_warn(dev, "success to request irq for card detect.\n");
				enable_irq_wake(host->ext_cd_irq);

				host->pdata->ext_setpower = exynos_dwmci2_ext_setpower;

				if (host->pdata->get_cd(slot_id))
					host->pdata->ext_setpower(host, DW_MMC_EXT_VQMMC_ON);
				else
					host->pdata->ext_setpower(host, DW_MMC_EXT_VQMMC_ON | DW_MMC_EXT_VMMC_ON);
			} else {
				dev_warn(dev, "cannot request irq for card detect.\n");
				host->ext_cd_irq = 0;
			}
			gpio_free(GPIO_T_FLASH_DETECT);
		} else {
			dev_err(dev, "cannot request gpio for card detect.\n");
		}

		if (sd_detection_cmd_dev == NULL) {
			sd_detection_cmd_dev =
				device_create(sec_class, NULL, 0,
						host, "sdcard");
			if (IS_ERR(sd_detection_cmd_dev))
				pr_err("Fail to create sysfs dev\n");

			if (device_create_file(sd_detection_cmd_dev,
						&dev_attr_status) < 0)
				pr_err("Fail to create sysfs file\n");
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

static struct dw_mci_board universal4415_dwmci2_pdata __initdata = {
	.num_slots		= 1,
	.ch_num			= 2,
	.data_timeout   = 200,
	.quirks			= DW_MCI_QUIRK_HIGHSPEED |
				  DW_MMC_QUIRK_NO_VOLSW_INT |
				  DW_MMC_QUIRK_USE_FINE_TUNING,
	.bus_hz			= 786432000 / 4,
	.caps			= MMC_CAP_CMD23 |
				  MMC_CAP_4_BIT_DATA |
				  MMC_CAP_SD_HIGHSPEED |
				  MMC_CAP_MMC_HIGHSPEED |
				  MMC_CAP_UHS_SDR50 |
				  MMC_CAP_UHS_SDR104,
	.fifo_depth		= 0x80,
	.detect_delay_ms	= 10,
	.hclk_name		= "hsmmc",
	.cclk_name		= "sclk_mmc",
	.cfg_gpio		= exynos_dwmci2_cfg_gpio,
	.get_bus_wd		= exynos_dwmci2_get_bus_wd,
	.save_drv_st		= exynos_dwmci_save_drv_st,
	.restore_drv_st		= exynos_dwmci_restore_drv_st,
	.tuning_drv_st		= exynos_dwmci_tuning_drv_st,
	.sdr_timing		= 0x03020000,
	.ddr_timing		= 0x03020001,
	.get_cd			= exynos_dwmci2_get_cd,
	.get_ro			= exynos_dwmci2_get_ro,
	.clk_drv		= 0x3,
	.ignore_phase		= (1 << 7),
	.cd_type		= DW_MCI_CD_GPIO,
	.init			= exynos_dwmci2_init,
	.exit			= exynos_dwmci2_exit,
	.clk_tbl		= exynos_dwmci_clk_rates_for_epll,
	.__drv_st		= {
		.pin			= EXYNOS4_GPK2(0),
		.val			= S5P_GPIO_DRVSTR_LV3,
	},
	.register_notifier	= exynos_register_notifier,
};

static struct platform_device *universal4415_mmc_devices[] __initdata = {
	&exynos4_device_dwmci0,
	&exynos4_device_dwmci1,
	&exynos4_device_dwmci2,
};

void __init exynos4415_universal4415_mmc_init(void)
{
	exynos_dwmci_set_platdata(&universal4415_dwmci0_pdata, 0);
	exynos_dwmci_set_platdata(&universal4415_dwmci1_pdata, 1);
	exynos_dwmci_set_platdata(&universal4415_dwmci2_pdata, 2);
	platform_add_devices(universal4415_mmc_devices,
			ARRAY_SIZE(universal4415_mmc_devices));

}
