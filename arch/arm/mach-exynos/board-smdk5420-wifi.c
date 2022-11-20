/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/
#include <linux/gpio.h>
#include <linux/mmc/host.h>
#include <linux/delay.h>
#include <linux/random.h>
#include <linux/if.h>
#include <linux/wlan_plat.h>

#include <plat/gpio-cfg.h>
#include <plat/cpu.h>
#include <plat/clock.h>
#include <plat/devs.h>

#include <mach/dwmci.h>

#include "board-smdk5420.h"
#define GPIO_WLAN_PMENA EXYNOS5420_GPX2(7)
#define GPIO_WLAN_IRQ   EXYNOS5420_GPX3(0)

#define WLAN_SDIO_CMD   EXYNOS5420_GPC1(1)
#define WLAN_SDIO_DATA0 EXYNOS5420_GPC1(3)
#define WLAN_SDIO_DATA1 EXYNOS5420_GPC1(4)
#define WLAN_SDIO_DATA2 EXYNOS5420_GPC1(5)
#define WLAN_SDIO_DATA3 EXYNOS5420_GPC1(6)

static DEFINE_MUTEX(scsc_mmc_ctrl_sync);

static void scsc_mmc_ctrl_lock(void)
{
	mutex_lock(&scsc_mmc_ctrl_sync);
}

static void scsc_mmc_ctrl_unlock(void)
{
	mutex_unlock(&scsc_mmc_ctrl_sync);
}

static struct resource scsc_mmc_resources[] = {
	[0] = {
	.name   = "scsc_mmc_irq",
	.start  = GPIO_WLAN_IRQ,
	.end    = GPIO_WLAN_IRQ,
	.flags  = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL |
		IORESOURCE_IRQ_SHAREABLE,
	},
};

struct wifi_gpio_sleep_data {
	uint num;
	uint cfg;
	uint pull;
};

static struct wifi_gpio_sleep_data scsc_mmc_sleep_gpios[] = {
	/* WLAN_SDIO_CMD */
	{WLAN_SDIO_CMD, S5P_GPIO_PD_INPUT, S5P_GPIO_PD_UPDOWN_DISABLE},
	/* WLAN_SDIO_D(0) */
	{WLAN_SDIO_DATA0, S5P_GPIO_PD_INPUT, S5P_GPIO_PD_UPDOWN_DISABLE},
	/* WLAN_SDIO_D(1) */
	{WLAN_SDIO_DATA1, S5P_GPIO_PD_INPUT, S5P_GPIO_PD_UPDOWN_DISABLE},
	/* WLAN_SDIO_D(2) */
	{WLAN_SDIO_DATA2, S5P_GPIO_PD_INPUT, S5P_GPIO_PD_UPDOWN_DISABLE},
	/* WLAN_SDIO_D(3) */
	{WLAN_SDIO_DATA3, S5P_GPIO_PD_INPUT, S5P_GPIO_PD_UPDOWN_DISABLE},
};

static void (*wifi_status_cb)(struct platform_device *, int state);

static int exynos5_scsc_mmc_ext_cd_init(
	void (*notify_func)(struct platform_device *, int))
{
	pr_debug("SCSC: registered CD callback\n");
	wifi_status_cb = notify_func;
	return 0;
}

static int exynos5_scsc_mmc_ext_cd_cleanup(
	void (*notify_func)(struct platform_device *, int))
{
	pr_debug("SCSC: unregistered CD callback\n");
	wifi_status_cb = NULL;
	return 0;
}

static int scsc_mmc_set_carddetect(int val)
{
	pr_debug("SCSC: Card Detected...\n");

	if (wifi_status_cb)
		wifi_status_cb(&exynos5_device_dwmci1, val);
	else
		pr_warning("%s: Nobody to notify\n", __func__);
	return 0;
}

static int scsc_mmc_power_state;

static int scsc_mmc_power(int on)
{
	int ret = 0;

	pr_debug("SCSC: Power => %d\n", on);

	scsc_mmc_ctrl_lock();

	if (on) {
		gpio_set_value(GPIO_WLAN_PMENA, 0);
		msleep(150);
	}

	gpio_set_value(GPIO_WLAN_PMENA, on);
	pr_debug("SCSC: Power : GPIO = %d\n",
			gpio_get_value(GPIO_WLAN_PMENA));

	scsc_mmc_power_state = on;
	if (on) {
		msleep(50);
		s5p_gpio_set_pd_cfg(GPIO_WLAN_PMENA, S5P_GPIO_PD_OUTPUT1);
		s5p_gpio_set_pd_pull(GPIO_WLAN_PMENA, S5P_GPIO_PD_UPDOWN_DISABLE);
	} else {
		s5p_gpio_set_pd_cfg(GPIO_WLAN_PMENA, S5P_GPIO_PD_OUTPUT0);
		s5p_gpio_set_pd_pull(GPIO_WLAN_PMENA, S5P_GPIO_PD_UPDOWN_DISABLE);
	}

	scsc_mmc_ctrl_unlock();

	return ret;
}

static int scsc_mmc_reset_state;

static int scsc_mmc_reset(int on)
{
	pr_debug("SCSC: Reset => %d\n", on);
	pr_debug("%s: do nothing\n", __func__);
	scsc_mmc_reset_state = on;
	return 0;
}

static unsigned char scsc_wifi_mac_addr[IFHWADDRLEN] \
					= { 0, 0x90, 0x4c, 0, 0, 0 };

static int __init scsc_wifi_mac_addr_setup(char *str)
{
	char macstr[IFHWADDRLEN*3];
	char *macptr = macstr;
	char *token;
	int i = 0;

	if (!str)
		return 0;
	pr_debug("wlan MAC = %s\n", str);
	if (strlen(str) >= sizeof(macstr))
		return 0;
	strcpy(macstr, str);

	while ((token = strsep(&macptr, ":")) != NULL) {
		unsigned long val;
		int res;

		if (i >= IFHWADDRLEN)
			break;
		res = kstrtoul(token, 0x10, &val);
		if (res < 0)
			return 0;
		scsc_wifi_mac_addr[i++] = (u8)val;
	}
	return 1;
}

__setup("androidboot.wifimacaddr=", scsc_wifi_mac_addr_setup);

static int scsc_mmc_get_mac_addr(unsigned char *buf)
{
	uint rand_mac;

	if (!buf)
		return -EFAULT;

	if ((scsc_wifi_mac_addr[4] == 0) && (scsc_wifi_mac_addr[5] == 0)) {
		srandom32((uint)jiffies);
		rand_mac = random32();
		scsc_wifi_mac_addr[3] = (unsigned char)rand_mac;
		scsc_wifi_mac_addr[4] = (unsigned char)(rand_mac >> 8);
		scsc_wifi_mac_addr[5] = (unsigned char)(rand_mac >> 16);
	}
	memcpy(buf, scsc_wifi_mac_addr, IFHWADDRLEN);
	return 0;
}

/* Customized Locale table : OPTIONAL feature */
#define WLC_CNTRY_BUF_SZ    4
struct cntry_locales_custom {
	char iso_abbrev[WLC_CNTRY_BUF_SZ];
	char custom_locale[WLC_CNTRY_BUF_SZ];
	int  custom_locale_rev;
};

static struct cntry_locales_custom scsc_mmc_translate_custom_table[] = {
/* Table should be filled out based on custom platform regulatory requirement */
	{"",   "XY", 7},  /* universal */
	{"US", "US", 69}, /* input ISO "US" to : US regrev 69 */
	{"CA", "US", 69}, /* input ISO "CA" to : US regrev 69 */
	{"EU", "EU", 5},  /* European union countries */
	{"AT", "EU", 5},
	{"BE", "EU", 5},
	{"BG", "EU", 5},
	{"CY", "EU", 5},
	{"CZ", "EU", 5},
	{"DK", "EU", 5},
	{"EE", "EU", 5},
	{"FI", "EU", 5},
	{"FR", "EU", 5},
	{"DE", "EU", 5},
	{"GR", "EU", 5},
	{"HU", "EU", 5},
	{"IE", "EU", 5},
	{"IT", "EU", 5},
	{"LV", "EU", 5},
	{"LI", "EU", 5},
	{"LT", "EU", 5},
	{"LU", "EU", 5},
	{"MT", "EU", 5},
	{"NL", "EU", 5},
	{"PL", "EU", 5},
	{"PT", "EU", 5},
	{"RO", "EU", 5},
	{"SK", "EU", 5},
	{"SI", "EU", 5},
	{"ES", "EU", 5},
	{"SE", "EU", 5},
	{"GB", "EU", 5},  /* input ISO "GB" to : EU regrev 05 */
	{"IL", "IL", 0},
	{"CH", "CH", 0},
	{"TR", "TR", 0},
	{"NO", "NO", 0},
	{"KR", "KR", 25},
	{"AU", "XY", 3},
	{"CN", "CN", 0},
	{"TW", "XY", 3},
	{"AR", "XY", 3},
	{"MX", "XY", 3},
	{"JP", "EU", 0},
	{"BR", "KR", 25}
};

static void *scsc_mmc_get_country_code(char *ccode)
{
	int size = ARRAY_SIZE(scsc_mmc_translate_custom_table);
	int i;

	if (!ccode)
		return NULL;

	for (i = 0; i < size; i++) {
		if (strcmp(ccode,
		scsc_mmc_translate_custom_table[i].iso_abbrev) == 0)
			return &scsc_mmc_translate_custom_table[i];
	}
	return &scsc_mmc_translate_custom_table[0];
}

static struct wifi_platform_data scsc_mmc_control = {
	.set_power		= scsc_mmc_power,
	.set_reset		= scsc_mmc_reset,
	.set_carddetect		= scsc_mmc_set_carddetect,
	.mem_prealloc		= NULL,
	.get_mac_addr		= scsc_mmc_get_mac_addr,
	.get_country_code	= scsc_mmc_get_country_code,
};

static struct platform_device scsc_mmc_device = {
	.name		= "scsc_mmc_ctrl",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(scsc_mmc_resources),
	.resource	= scsc_mmc_resources,
	.dev		= {
		.platform_data = &scsc_mmc_control,
	},
};

static struct dw_mci_clk exynos_dwmci_clk_rates_for_spll[] = {
	{25 * 1000 * 1000, 50 * 1000 * 1000},
	{50 * 1000 * 1000, 100 * 1000 * 1000},
	{50 * 1000 * 1000, 100 * 1000 * 1000},
	{100 * 1000 * 1000, 200 * 1000 * 1000},
	{200 * 1000 * 1000, 400 * 1000 * 1000},
	{100 * 1000 * 1000, 200 * 1000 * 1000},
	{200 * 1000 * 1000, 400 * 1000 * 1000},
	{200 * 1000 * 1000, 400 * 1000 * 1000},
};

static int exynos_dwmci1_get_bus_wd(u32 slot_id)
{
	return 8;
}

static void exynos_dwmci1_cfg_gpio(int width)
{
	unsigned int gpio;

	for (gpio = EXYNOS5420_GPC1(0); gpio < EXYNOS5420_GPC1(3); gpio++) {
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
		if (gpio == EXYNOS5420_GPC1(0))
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
		else
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
	}

	gpio = EXYNOS5420_GPD1(1);
	s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
	s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);

	switch (width) {
	case 8:
	case 4:
		for (gpio = EXYNOS5420_GPC1(3);
				gpio <= EXYNOS5420_GPC1(6); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
			s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
		}

		for (gpio = EXYNOS5420_GPD1(4);
				gpio <= EXYNOS5420_GPD1(7); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
			s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
		}

		break;
	case 1:
		gpio = EXYNOS5420_GPC1(3);
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV4);
	default:
		break;
	}
}
static struct dw_mci_board smdk5420_dwmci1_pdata __initdata = {
	.num_slots		= 1,
	.ch_num			= 1,
	.cd_type		= DW_MCI_CD_EXTERNAL,
	.quirks			= DW_MCI_QUIRK_HIGHSPEED |
				  DW_MMC_QUIRK_FIXED_VOLTAGE,
	.bus_hz			= 50 * 1000 * 1000,
	.caps			= MMC_CAP_4_BIT_DATA | MMC_CAP_SDIO_IRQ,
	.caps2			= MMC_CAP2_BROKEN_VOLTAGE,
	.pm_caps		= MMC_PM_KEEP_POWER | MMC_PM_IGNORE_PM_NOTIFY,
	.fifo_depth		= 0x40,
	.detect_delay_ms	= 200,
	.only_once_tune		= true,
	.hclk_name		= "dwmci",
	.cclk_name		= "sclk_dwmci",
	.cfg_gpio		= exynos_dwmci1_cfg_gpio,
	.get_bus_wd		= exynos_dwmci1_get_bus_wd,
	.ext_cd_init		= exynos5_scsc_mmc_ext_cd_init,
	.ext_cd_cleanup		= exynos5_scsc_mmc_ext_cd_cleanup,
	.sdr_timing		= 0x03040000,
	.ddr_timing		= 0x03020000,
	.clk_tbl		= exynos_dwmci_clk_rates_for_spll,
	.qos_int_level		= 111 * 1000,
};

static struct platform_device *smdk5420_wifi_devices[] __initdata = {
	&exynos5_device_dwmci1,
	&scsc_mmc_device,
};

static void __init scsc_mmc_configure_gpio(void)
{
	int gpio;
	int i;

	/* Setup wlan Power Enable */
	gpio = GPIO_WLAN_PMENA;
	s3c_gpio_cfgpin(gpio, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
	s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV3);
	/* Keep power state during suspend */
	s5p_gpio_set_pd_cfg(gpio, S5P_GPIO_PD_PREV_STATE);
	gpio_set_value(gpio, 0);

	/* Setup wlan IRQ */
	gpio = GPIO_WLAN_IRQ;
	s3c_gpio_cfgpin(gpio, S3C_GPIO_INPUT);
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
	s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV3);

	scsc_mmc_resources[0].start = gpio_to_irq(gpio);
	scsc_mmc_resources[0].end = gpio_to_irq(gpio);

	/* Setup sleep GPIO for wifi */
	for (i = 0; i < ARRAY_SIZE(scsc_mmc_sleep_gpios); i++) {
		gpio = scsc_mmc_sleep_gpios[i].num;
		s5p_gpio_set_pd_cfg(gpio, scsc_mmc_sleep_gpios[i].cfg);
		s5p_gpio_set_pd_pull(gpio, scsc_mmc_sleep_gpios[i].pull);
	}
}

void __init exynos5_smdk5420_mmc1_init(void)
{
	exynos_dwmci_set_platdata(&smdk5420_dwmci1_pdata, 1);

	scsc_mmc_configure_gpio();

	platform_add_devices(smdk5420_wifi_devices,
			ARRAY_SIZE(smdk5420_wifi_devices));
}
