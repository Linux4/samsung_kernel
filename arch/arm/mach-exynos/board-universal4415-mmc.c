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

#include <plat/gpio-cfg.h>
#include <plat/cpu.h>
#include <plat/clock.h>
#include <plat/devs.h>
#include <plat/sdhci.h>

#include <mach/dwmci.h>

#include "board-universal4415.h"

#define GPIO_T_FLASH_DETECT	EXYNOS4_GPX3(4)
#define GPIO_TF_EN		EXYNOS4_GPX2(4)

static struct dw_mci_clk exynos_dwmci_clk_rates_for_mpll[] = {
	{800 * 1000 * 1000 / 32 , 800 * 1000 * 1000 / 8},
	{800 * 1000 * 1000 / 16 , 800 * 1000 * 1000 / 4},
	{800 * 1000 * 1000 / 16 , 800 * 1000 * 1000 / 4},
	{800 * 1000 * 1000 / 8  , 800 * 1000 * 1000 / 2},
	{800 * 1000 * 1000 / 4  , 800 * 1000 * 1000    },
	{800 * 1000 * 1000 / 8  , 800 * 1000 * 1000 / 2},
	{800 * 1000 * 1000 / 4  , 800 * 1000 * 1000    },
	{800 * 1000 * 1000 / 2  , 800 * 1000 * 1000    },

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

static int exynos_dwmci0_get_bus_wd(u32 slot_id)
{
	return 8;
}

static void exynos_dwmci0_cfg_gpio(int width)
{
	unsigned int gpio;

	for (gpio = EXYNOS4_GPK0(0);
			gpio < EXYNOS4_GPK0(2); gpio++) {
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
	}

	switch (width) {
	case 8:
		for (gpio = EXYNOS4_GPL0(0);
				gpio <= EXYNOS4_GPL0(3); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
			s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
		}
	case 4:
		for (gpio = EXYNOS4_GPK0(3);
				gpio <= EXYNOS4_GPK0(6); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
			s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
		}
		break;
	case 1:
		gpio = EXYNOS4_GPK0(3);
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
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
				  DW_MMC_QUIRK_FIXED_VOLTAGE,
	.bus_hz			= 800 * 1000 * 1000 / 4,
	.caps			= MMC_CAP_CMD23 | MMC_CAP_8_BIT_DATA |
				  MMC_CAP_UHS_DDR50 | MMC_CAP_1_8V_DDR |
				  MMC_CAP_ERASE,
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
	.sdr_timing		= 0x03040000,
	.ddr_timing		= 0x03020000,
	.clk_drv		= 0x3,
	.ddr200_timing		= 0x01020002,
	.clk_tbl		= exynos_dwmci_clk_rates_for_mpll,
	.__drv_st		= {
		.pin			= EXYNOS4_GPK0(0),
		.val			= S5P_GPIO_DRVSTR_LV4,
	},
};

static int exynos_dwmci1_get_bus_wd(u32 slot_id)
{
	return 4;
}

static void exynos_dwmci1_cfg_gpio(int width)
{
	unsigned int gpio;

	for (gpio = EXYNOS4_GPK1(0); gpio < EXYNOS4_GPK1(3); gpio++) {
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
		if (gpio == EXYNOS4_GPK1(0))
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
		else
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
	}

	switch (width) {
	case 8:
	case 4:
		for (gpio = EXYNOS4_GPK1(3);
				gpio <= EXYNOS4_GPK1(6); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
			s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
		}
		break;
	case 1:
		gpio = EXYNOS4_GPK1(3);
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
	default:
		break;
	}
}

static int ext_cd_init_wlan(
	void (*notify_func)(struct platform_device *dev, int state))
{
	return 0;
}

static int ext_cd_cleanup_wlan(
	void (*notify_func)(struct platform_device *dev, int state))
{
	return 0;
}

static struct dw_mci_board universal4415_dwmci1_pdata __initdata = {
	.num_slots		= 1,
	.ch_num			= 1,
	.quirks			= DW_MCI_QUIRK_HIGHSPEED,
	.bus_hz			= 800 * 1000 * 1000 /4,
	.caps			= MMC_CAP_UHS_SDR104 |
				  MMC_CAP_SD_HIGHSPEED | MMC_CAP_4_BIT_DATA,
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
	.sdr_timing		= 0x03040000,
	.ddr_timing		= 0x03020000,
	.clk_drv		= 0x3,
	.clk_tbl		= exynos_dwmci_clk_rates_for_mpll,
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

	/* set to pull up pin for write protection */
	gpio = EXYNOS4_GPK2(0);
	s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
	s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);

	for (gpio = EXYNOS4_GPK2(0); gpio < EXYNOS4_GPK2(2); gpio++) {
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
	}

	switch (width) {
	case 4:
		for (gpio = EXYNOS4_GPK2(3);
				gpio <= EXYNOS4_GPK2(6); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
			s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
		}
		break;
	case 1:
		gpio = EXYNOS4_GPK2(3);
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
	default:
		break;
	}

	gpio = EXYNOS4_GPK2(2);
	s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
	s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
}

static int exynos_dwmci2_get_bus_wd(u32 slot_id)
{
	return 4;
}

struct dw_mci *host2;
static int ext_cd_irq = 0;

static int exynos_dwmci2_init(u32 slot_id, irq_handler_t handler, void *data)
{
	struct dw_mci *host = (struct dw_mci *)data;
	struct device *dev = &host->dev;

	host2 = host;

	/* TF_EN pin */
	s3c_gpio_cfgpin(GPIO_TF_EN, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_TF_EN, S3C_GPIO_PULL_NONE);

	if (host->pdata->cd_type == DW_MCI_CD_GPIO &&
		gpio_is_valid(GPIO_T_FLASH_DETECT)) {

		s3c_gpio_setpull(GPIO_T_FLASH_DETECT, S3C_GPIO_PULL_NONE);

		if (gpio_request(GPIO_T_FLASH_DETECT, "DWMCI EXT CD") == 0) {
			ext_cd_irq = gpio_to_irq(GPIO_T_FLASH_DETECT);
			if (ext_cd_irq &&
			    request_threaded_irq(ext_cd_irq, NULL,
						 handler,
						 IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
						 dev_name(dev), host) == 0) {
				dev_warn(dev, "success to request irq for card detect.\n");
				enable_irq_wake(ext_cd_irq);
			} else {
				dev_warn(dev, "cannot request irq for card detect.\n");
				ext_cd_irq = 0;
			}
			gpio_free(GPIO_T_FLASH_DETECT);
		} else {
			dev_err(dev, "cannot request gpio for card detect.\n");
		}
	}

	return 0;
}

static void exynos_dwmci2_exit(u32 slot_id)
{
	struct dw_mci *host = host2;

	if (ext_cd_irq)
		free_irq(ext_cd_irq, host);
}

static struct dw_mci_board universal4415_dwmci2_pdata __initdata = {
	.num_slots		= 1,
	.ch_num			= 2,
	.quirks			= DW_MCI_QUIRK_HIGHSPEED,
	.bus_hz			= 800 * 1000 * 1000 / 4,
	.caps			= MMC_CAP_CMD23 |
				  MMC_CAP_4_BIT_DATA |
				  MMC_CAP_SD_HIGHSPEED |
				  MMC_CAP_MMC_HIGHSPEED |
				  MMC_CAP_UHS_SDR50 |
				  MMC_CAP_UHS_SDR104,
	.fifo_depth		= 0x80,
	.detect_delay_ms	= 200,
	.hclk_name		= "hsmmc",
	.cclk_name		= "sclk_mmc",
	.cfg_gpio		= exynos_dwmci2_cfg_gpio,
	.get_bus_wd		= exynos_dwmci2_get_bus_wd,
	.sdr_timing		= 0x03040000,
	.ddr_timing		= 0x03020000,
	.get_cd			= exynos_dwmci2_get_cd,
	.get_ro			= exynos_dwmci2_get_ro,
	.clk_drv		= 0x3,
	.cd_type		= DW_MCI_CD_GPIO,
	.init			= exynos_dwmci2_init,
	.exit			= exynos_dwmci2_exit,
	.clk_tbl		= exynos_dwmci_clk_rates_for_mpll,
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
