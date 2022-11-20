/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/gpio.h>
#include <linux/smsc911x.h>
#include <linux/mmc/host.h>
#include <linux/delay.h>

#include <plat/gpio-cfg.h>
#include <plat/cpu.h>
#include <plat/clock.h>
#include <plat/devs.h>
#include <plat/sdhci.h>

#include <mach/dwmci.h>

#include <linux/regulator/consumer.h>

#include "board-universal5260.h"

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

	gpio = EXYNOS5260_GPA1(6);
	s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(1));
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
	s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV3);

	for (gpio = EXYNOS5260_GPC0(0);
			gpio < EXYNOS5260_GPC0(2); gpio++) {
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV3);
	}

	switch (width) {
	case 8:
		for (gpio = EXYNOS5260_GPC3(0);
				gpio <= EXYNOS5260_GPC3(3); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
			s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV3);
		}
	case 4:
		for (gpio = EXYNOS5260_GPC0(2);
				gpio <= EXYNOS5260_GPC0(5); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
			s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV3);
		}
		break;
	case 1:
		gpio = EXYNOS5260_GPC0(2);
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV3);
	default:
		break;
	}

	gpio = EXYNOS5260_GPC0(6);
	s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_DOWN);
	s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV3);
}

static void exynos_dwmci0_change_cfg_gpio(u16 pin_map, s5p_gpio_drvstr_t drvstr)
{
	int i = 0;

	for (i = 0 ; i < EXYNOS5260_GPIO_C0_NR ; i++) {
		if (pin_map & (0x1 << i)) {
			pr_info("dwmci0: C0(%d). change to = 0x%x.\n", i, drvstr);
			s5p_gpio_set_drvstr(EXYNOS5260_GPC0(i), drvstr);
		}
	}
	pin_map = pin_map >> EXYNOS5260_GPIO_C0_NR;
	for (i = 0 ; i < EXYNOS5260_GPIO_C3_NR ; i++) {
		if (pin_map & (0x1 << i))
			s5p_gpio_set_drvstr(EXYNOS5260_GPC3(i), drvstr);
	}
}

static struct dw_mci_board universal5260_dwmci0_pdata __initdata = {
	.num_slots		= 1,
	.ch_num			= 0,
	.quirks			= DW_MCI_QUIRK_BROKEN_CARD_DETECTION |
				  DW_MCI_QUIRK_HIGHSPEED |
				  DW_MCI_QUIRK_NO_DETECT_EBIT |
#if defined(CONFIG_MACH_M2ALTE) || defined(CONFIG_MACH_M2A3G)
				  DW_MMC_QUIRK_USE_FINE_TUNING |
#endif
				  DW_MMC_QUIRK_FIXED_VOLTAGE,
	.bus_hz			= 200 * 1000 * 1000,
	.caps			= MMC_CAP_CMD23 | MMC_CAP_8_BIT_DATA |
				  MMC_CAP_UHS_DDR50 | MMC_CAP_1_8V_DDR |
				  MMC_CAP_ERASE,
	.caps2			= MMC_CAP2_HS200_1_8V_SDR | MMC_CAP2_HS200_1_8V_DDR |
				  MMC_CAP2_CACHE_CTRL | MMC_CAP2_BROKEN_VOLTAGE |
				  MMC_CAP2_NO_SLEEP_CMD | MMC_CAP2_POWEROFF_NOTIFY,
	.only_once_tune		= true,
	.fifo_depth		= 0x40,
	.detect_delay_ms	= 200,
	.delay_line		= 0x38,
	.hclk_name		= "dwmci",
	.cclk_name		= "sclk_dwmci",
	.cfg_gpio		= exynos_dwmci0_cfg_gpio,
	.change_cfg_gpio	= exynos_dwmci0_change_cfg_gpio,
	.get_bus_wd		= exynos_dwmci0_get_bus_wd,
	.cd_type		= DW_MCI_CD_PERMANENT,
#if defined(CONFIG_MACH_M2ALTE) || defined(CONFIG_MACH_M2A3G)
	.sdr_timing		= 0x03030001,
	.ddr_timing		= 0x03030002,
	.dev_drv_str		= MMC_DRIVER_TYPE_1,
#else
	.sdr_timing		= 0x03020001,
	.ddr_timing		= 0x03030002,
#endif
	.clk_drv		= 0x3,
	.ddr200_timing		= 0x01060000,
	.clk_tbl		= exynos_dwmci_clk_rates,
	.__drv_st		= {
		.pin			= EXYNOS5260_GPC0(0),
#if defined(CONFIG_MACH_M2ALTE) || defined(CONFIG_MACH_M2A3G)
		.val			= S5P_GPIO_DRVSTR_LV2,
#else
		.val			= S5P_GPIO_DRVSTR_LV4,
#endif
	},
	.qos_int_level		= 266 * 1000,
	.save_drv_st		= exynos_dwmci_save_drv_st,
	.restore_drv_st		= exynos_dwmci_restore_drv_st,
	.tuning_drv_st		= exynos_dwmci_tuning_drv_st,
	.extra_tuning           = exynos_dwmci0_extra_tuning,
	.register_notifier	= exynos_register_notifier,
	.unregister_notifier	= exynos_unregister_notifier,
};

static int exynos_dwmci1_get_bus_wd(u32 slot_id)
{
	return 4;
}

static void exynos_dwmci1_cfg_gpio(int width)
{
	unsigned int gpio;

	for (gpio = EXYNOS5260_GPC1(0); gpio < EXYNOS5260_GPC1(2); gpio++) {
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
	}


	switch (width) {
	case 8:
		for (gpio = EXYNOS5260_GPC4(0);
				gpio <= EXYNOS5260_GPC4(3); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
			s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
		}
	case 4:
		for (gpio = EXYNOS5260_GPC1(2);
				gpio <= EXYNOS5260_GPC1(5); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
			s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
		}
		break;
	case 1:
		gpio = EXYNOS5260_GPC1(2);
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
	default:
		break;
	}
}

static void (*wlan_notify_func)(struct platform_device *dev, int state);
static DEFINE_MUTEX(wlan_mutex_lock);

static int ext_cd_init_wlan(
	void (*notify_func)(struct platform_device *dev, int state))
{
	mutex_lock(&wlan_mutex_lock);
	WARN_ON(wlan_notify_func);
	wlan_notify_func = notify_func;
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

	if (pdev == &exynos5_device_dwmci1) {
		pr_err("%s: called for device exynos5_device_dwmci1\n", __func__);
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

#if defined(CONFIG_ARM_EXYNOS5260_BUS_DEVFREQ)
#ifndef CONFIG_ARM_EXYNOS_MP_CPUFREQ
static struct dw_mci_mon_table exynos_dwmci_tp_mon1_tbl[] = {
	/* Byte/s, INT clk, MIF clk, CPU clk , KFC clk*/
	{  26000000, 0, 667000, 1500000},
	{  12000000, 0, 413000, 0},
	{         0, 0, 0, 0},
};
#else
#if defined(CONFIG_BCM4334)
static struct dw_mci_mon_table exynos_dwmci_tp_mon1_tbl[] = {
	/* Byte/s, INT clk, MIF clk, CPU clk , KFC clk*/
	{6000000, 266000, 543000, 1400000, 1200000},
	{3000000, 266000, 275000,       0,  800000},
	{      0, 0,      0,       0,       0},
};
#else
static struct dw_mci_mon_table exynos_dwmci_tp_mon1_tbl[] = {
	/* Byte/s, INT clk, MIF clk, CPU clk , KFC clk*/
	{  12000000, 333000, 0,    1400000, 1100000},
	{  10000000, 333000, 0,          0,    0},
	{		  0,      0, 0,          0,    0},
};
#endif
#endif
#endif


static struct dw_mci_board universal5260_dwmci1_pdata __initdata = {
	.num_slots		= 1,
	.ch_num			= 1,
	.quirks			= DW_MCI_QUIRK_HIGHSPEED |
				  DW_MMC_QUIRK_FIXED_VOLTAGE |
				  DW_MMC_QUIRK_USE_FINE_TUNING |
				  DW_MMC_QUIRK_RETRY_CRC_ERROR |
				  DW_MMC_QUIRK_WA_DMA_SUSPEND,
#if defined(CONFIG_BCM4334)
	.bus_hz			= 50 * 1000 * 1000,
	.caps			= MMC_CAP_SD_HIGHSPEED | MMC_CAP_4_BIT_DATA,
#else
	.bus_hz			= 200 * 1000 * 1000,
	.caps			= MMC_CAP_UHS_SDR104 |
				  MMC_CAP_SD_HIGHSPEED | MMC_CAP_4_BIT_DATA,
#endif
	.caps2			= MMC_CAP2_BROKEN_VOLTAGE,
	.pm_caps		= MMC_PM_KEEP_POWER | MMC_PM_IGNORE_PM_NOTIFY,
	.fifo_depth		= 0x80,
	.detect_delay_ms	= 200,
	.sw_timeout		= 500,
	.ignore_phase = (0x1 << 7),
	.hclk_name		= "dwmci",
	.cclk_name		= "sclk_dwmci",
	.cfg_gpio		= exynos_dwmci1_cfg_gpio,
	.get_bus_wd		= exynos_dwmci1_get_bus_wd,
	.ext_cd_init	= ext_cd_init_wlan,
	.ext_cd_cleanup	= ext_cd_cleanup_wlan,
	.cd_type		= DW_MCI_CD_EXTERNAL,
	.sdr_timing		= 0x03020001,
	.ddr_timing		= 0x03030002,
	.clk_drv		= 0x3,
	.clk_tbl		= exynos_dwmci_clk_rates,
	.__drv_st		= {
		.pin			= EXYNOS5260_GPC1(0),
		.val			= S5P_GPIO_DRVSTR_LV4,
	},
	.qos_int_level		= 100 * 1000,
	.save_drv_st		= exynos_dwmci_save_drv_st,
	.restore_drv_st		= exynos_dwmci_restore_drv_st,
	.tuning_drv_st		= exynos_dwmci_tuning_drv_st,
	.register_notifier	= exynos_register_notifier,
	.unregister_notifier	= exynos_unregister_notifier,
#if defined(CONFIG_ARM_EXYNOS5260_BUS_DEVFREQ)
	.tp_mon_tbl		= exynos_dwmci_tp_mon1_tbl,
#endif
};

#if defined(CONFIG_MACH_M2ALTE) || defined(CONFIG_MACH_M2A3G)
extern unsigned int system_rev;
#endif

static int exynos_dwmci_get_cd(u32 slot_id)
{
#if defined(CONFIG_MACH_M2ALTE) || defined(CONFIG_MACH_M2A3G)
	if (system_rev < 4)
		return gpio_get_value(EXYNOS5260_GPX3(6));
#endif
	return gpio_get_value(EXYNOS5260_GPX2(4));
}

static void exynos_dwmci2_cfg_gpio(int width)
{
	unsigned int gpio;

	/* set to pull up pin for write protection */

	for (gpio = EXYNOS5260_GPC2(0); gpio < EXYNOS5260_GPC2(2); gpio++) {
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
	}

	switch (width) {
	case 4:
		for (gpio = EXYNOS5260_GPC2(3);
				gpio <= EXYNOS5260_GPC2(6); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
			s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
		}
		break;
	case 1:
		gpio = EXYNOS5260_GPC2(3);
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
	default:
		break;
	}

}

static int exynos_dwmci2_get_bus_wd(u32 slot_id)
{
	return 4;
}

static int exynos5260_dwmci_get_ro(u32 slot_id)
{
	return 0;
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

static struct dw_mci *host2;

static void exynos_dwmci2_ext_setpower(void *data, u32 flag)
{
	struct dw_mci *host = (struct dw_mci *) data;
	struct device *dev = &host->dev;
	unsigned int gpio;

	/* host->vqmmc : SDcard I/F power */
	if (flag & DW_MMC_EXT_VQMMC_ON) {
		if (!regulator_is_enabled(host->vqmmc))
			regulator_enable(host->vqmmc);
	} else {
		if (regulator_is_enabled(host->vqmmc))
			regulator_disable(host->vqmmc);
	}

	/* host->vmmc : SDcard power */
	if (flag & DW_MMC_EXT_VMMC_ON) {
		if (!regulator_is_enabled(host->vmmc))
			regulator_enable(host->vmmc);
		/* set to func. pin for SD I/F */
		for (gpio = EXYNOS5260_GPC2(0); gpio < EXYNOS5260_GPC2(2); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
		}
	} else {
		if (regulator_is_enabled(host->vmmc))
			regulator_disable(host->vmmc);
		/* set to output pin */
		for (gpio = EXYNOS5260_GPC2(0); gpio < EXYNOS5260_GPC2(2); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_OUTPUT);
		}
	}
	dev_info(dev, "vmmc %s, vqmmc %s.\n",
			regulator_is_enabled(host->vmmc) ? "on" : "off",
			regulator_is_enabled(host->vqmmc) ? "on" : "off");
}

static int exynos_dwmci2_init(u32 slot_id, irq_handler_t handler, void *data)
{
	struct dw_mci *host = (struct dw_mci *) data;
	struct dw_mci_board *pdata = host->pdata;
	struct device *dev = &host->dev;

	host2 = host;

#if defined(CONFIG_MACH_M2ALTE) || defined(CONFIG_MACH_M2A3G)
	if (system_rev < 4)
		host->pdata->ext_cd_gpio = EXYNOS5260_GPX3(6);
#endif
	if (host->pdata->cd_type == DW_MCI_CD_GPIO &&
		gpio_is_valid(host->pdata->ext_cd_gpio)) {

		s3c_gpio_cfgpin(host->pdata->ext_cd_gpio, S3C_GPIO_SFN(0xF));
		s3c_gpio_setpull(host->pdata->ext_cd_gpio, S3C_GPIO_PULL_UP);

		if (gpio_request(pdata->ext_cd_gpio, "DWMCI EXT CD") == 0) {
			host->ext_cd_irq = gpio_to_irq(pdata->ext_cd_gpio);
			if (host->ext_cd_irq &&
			    request_threaded_irq(host->ext_cd_irq, NULL,
					handler,
					IRQF_TRIGGER_RISING |
					IRQF_TRIGGER_FALLING,
					dev_name(dev), host) == 0) {
				dev_warn(dev, "success to request irq for card detect.\n");

				host->pdata->ext_setpower = exynos_dwmci2_ext_setpower;

				if (pdata->get_cd(slot_id))
					host->pdata->ext_setpower(host, DW_MMC_EXT_VQMMC_ON);
				else
					host->pdata->ext_setpower(host, DW_MMC_EXT_VQMMC_ON | DW_MMC_EXT_VMMC_ON);

				/* set low XBOOTLDO for power control */
				if (gpio_request(GPIO_eMMC_EN, "DWMCI eMMC_EN") == 0)
					gpio_direction_output(GPIO_eMMC_EN, 0);
				else
					dev_warn(dev, "eMMC_EN gpio request is failed.\n");
			} else {
				dev_warn(dev, "cannot request irq for card detect.\n");
				host->ext_cd_irq = 0;
			}
			gpio_free(pdata->ext_cd_gpio);
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

static struct dw_mci_board universal5260_dwmci2_pdata __initdata = {
	.num_slots		= 1,
	.ch_num			= 2,
	.quirks			= DW_MCI_QUIRK_HIGHSPEED |
				  DW_MMC_QUIRK_WA_DMA_SUSPEND |
				  DW_MMC_QUIRK_DMA_RECOVERY |
				  DW_MMC_QUIRK_USE_FINE_TUNING |
				  DW_MMC_QUIRK_NO_VOLSW_INT,
	.bus_hz			= 200 * 1000 * 1000,
	.caps			= MMC_CAP_CMD23 |
				  MMC_CAP_4_BIT_DATA |
				  MMC_CAP_SD_HIGHSPEED |
				  MMC_CAP_MMC_HIGHSPEED |
				  MMC_CAP_UHS_SDR50 |
				  MMC_CAP_UHS_SDR104,
	.fifo_depth		= 0x80,
	.detect_delay_ms	= 10,
	.ignore_phase		= (0x1 << 7),
	.hclk_name		= "dwmci",
	.cclk_name		= "sclk_dwmci",
	.cfg_gpio		= exynos_dwmci2_cfg_gpio,
	.get_bus_wd		= exynos_dwmci2_get_bus_wd,
	.sdr_timing		= 0x03020001,
	.ddr_timing		= 0x03030002,
	.clk_drv		= 0x3,
	.cd_type		= DW_MCI_CD_GPIO,
	.clk_tbl		= exynos_dwmci_clk_rates,
	.__drv_st		= {
		.pin			= EXYNOS5260_GPC2(0),
		.val			= S5P_GPIO_DRVSTR_LV3,
	},
	.qos_int_level		= 100 * 1000,
	.save_drv_st		= exynos_dwmci_save_drv_st,
	.restore_drv_st		= exynos_dwmci_restore_drv_st,
	.tuning_drv_st		= exynos_dwmci_tuning_drv_st,
	.register_notifier	= exynos_register_notifier,
	.unregister_notifier	= exynos_unregister_notifier,
	.get_cd			= exynos_dwmci_get_cd,
	.ext_cd_gpio		= EXYNOS5260_GPX2(4),
	.init			= exynos_dwmci2_init,
	.exit			= exynos_dwmci2_exit,
	.get_ro			= exynos5260_dwmci_get_ro,
};

static struct platform_device *universal5260_mmc_devices[] __initdata = {
	&exynos5_device_dwmci0,
	&exynos5_device_dwmci1,
	&exynos5_device_dwmci2,
};

void __init exynos5_universal5260_mmc_init(void)
{
#if defined(CONFIG_MACH_M2ALTE) || defined(CONFIG_MACH_M2A3G)
	if (system_rev == 0x7) {
		universal5260_dwmci0_pdata.caps2 &=
			~(MMC_CAP2_HS200_1_8V_SDR | MMC_CAP2_HS200_1_8V_DDR);
	}
#endif
	exynos_dwmci_set_platdata(&universal5260_dwmci0_pdata, 0);
	exynos_dwmci_set_platdata(&universal5260_dwmci1_pdata, 1);
	exynos_dwmci_set_platdata(&universal5260_dwmci2_pdata, 2);

	platform_add_devices(universal5260_mmc_devices,
			ARRAY_SIZE(universal5260_mmc_devices));
}
