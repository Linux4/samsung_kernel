#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/skbuff.h>
#include <linux/wlan_plat.h>
#include <linux/mmc/host.h>

#include <plat/devs.h>
#include <plat/sdhci.h>
#include <plat/gpio-cfg.h>
#include <mach/regs-gpio.h>
#include <mach/gpio.h>
#include <linux/gpio.h>
#include <plat/cpu.h>
#include <plat/clock.h>
#include <plat/devs.h>

#include <mach/dwmci.h>

#include "board-kmini-wlan.h"

#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM

#define WLAN_STATIC_SCAN_BUF0		5
#define WLAN_STATIC_SCAN_BUF1		6
#define WLAN_STATIC_DHD_INFO_BUF	7
#define WLAN_SCAN_BUF_SIZE		(64 * 1024)
#define WLAN_DHD_INFO_BUF_SIZE	(16 * 1024)
#define PREALLOC_WLAN_SEC_NUM		4
#define PREALLOC_WLAN_BUF_NUM		160
#define PREALLOC_WLAN_SECTION_HEADER	24

#define WLAN_SECTION_SIZE_0	(PREALLOC_WLAN_BUF_NUM * 128)
#define WLAN_SECTION_SIZE_1	(PREALLOC_WLAN_BUF_NUM * 128)
#define WLAN_SECTION_SIZE_2	(PREALLOC_WLAN_BUF_NUM * 512)
#define WLAN_SECTION_SIZE_3	(PREALLOC_WLAN_BUF_NUM * 1024)

#define DHD_SKB_HDRSIZE			336
#define DHD_SKB_1PAGE_BUFSIZE	((PAGE_SIZE*1)-DHD_SKB_HDRSIZE)
#define DHD_SKB_2PAGE_BUFSIZE	((PAGE_SIZE*2)-DHD_SKB_HDRSIZE)
#define DHD_SKB_4PAGE_BUFSIZE	((PAGE_SIZE*4)-DHD_SKB_HDRSIZE)

#define WLAN_SKB_BUF_NUM	17

static struct sk_buff *wlan_static_skb[WLAN_SKB_BUF_NUM];

struct wlan_mem_prealloc {
	void *mem_ptr;
	unsigned long size;
};

#if defined(CONFIG_ARM_EXYNOS3470_BUS_DEVFREQ)
#if defined(CONFIG_BCM4334)
static struct dw_mci_mon_table exynos_dwmci_tp_mon1_tbl[] = {
        /* Byte/s, MIF clk, CPU clk */
        {  6000000, 400000, 1200000},
        {  3000000,      0,  800000},
        {        0,      0,       0},
};
#else
static struct dw_mci_mon_table exynos_dwmci_tp_mon1_tbl[] = {
        /* Byte/s, MIF clk, CPU clk */
        {  9000000, 400000, 1300000},
        {         0,      0,       0},
};
#endif
#endif

static struct wlan_mem_prealloc wlan_mem_array[PREALLOC_WLAN_SEC_NUM] = {
	{NULL, (WLAN_SECTION_SIZE_0 + PREALLOC_WLAN_SECTION_HEADER)},
	{NULL, (WLAN_SECTION_SIZE_1 + PREALLOC_WLAN_SECTION_HEADER)},
	{NULL, (WLAN_SECTION_SIZE_2 + PREALLOC_WLAN_SECTION_HEADER)},
	{NULL, (WLAN_SECTION_SIZE_3 + PREALLOC_WLAN_SECTION_HEADER)}
};

void *wlan_static_scan_buf0;
void *wlan_static_scan_buf1;
void *wlan_static_dhd_info_buf;

static void *brcm_wlan_mem_prealloc(int section, unsigned long size)
{
	if (section == PREALLOC_WLAN_SEC_NUM)
		return wlan_static_skb;

	if (section == WLAN_STATIC_SCAN_BUF0)
		return wlan_static_scan_buf0;

	if (section == WLAN_STATIC_SCAN_BUF1)
		return wlan_static_scan_buf1;

	if (section == WLAN_STATIC_DHD_INFO_BUF) {
		if (size > WLAN_DHD_INFO_BUF_SIZE) {
			pr_err("request DHD_INFO size(%lu) is bigger than static size(%d).\n", size, WLAN_DHD_INFO_BUF_SIZE);
			return NULL;
		}
		return wlan_static_dhd_info_buf;
	}

	if ((section < 0) || (section > PREALLOC_WLAN_SEC_NUM))
		return NULL;

	if (wlan_mem_array[section].size < size)
		return NULL;

	return wlan_mem_array[section].mem_ptr;
}

static int brcm_init_wlan_mem(void)
{
	int i;
	int j;

	for (i = 0; i < 8; i++) {
		wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_1PAGE_BUFSIZE);
		if (!wlan_static_skb[i])
			goto err_skb_alloc;
	}

	for (; i < 16; i++) {
		wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_2PAGE_BUFSIZE);
		if (!wlan_static_skb[i])
			goto err_skb_alloc;
	}

	wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_4PAGE_BUFSIZE);
	if (!wlan_static_skb[i])
		goto err_skb_alloc;

	for (i = 0 ; i < PREALLOC_WLAN_SEC_NUM ; i++) {
		wlan_mem_array[i].mem_ptr =
				kmalloc(wlan_mem_array[i].size, GFP_KERNEL);

		if (!wlan_mem_array[i].mem_ptr)
			goto err_mem_alloc;
	}

	wlan_static_scan_buf0 = kmalloc(WLAN_SCAN_BUF_SIZE, GFP_KERNEL);
	if (!wlan_static_scan_buf0)
		goto err_mem_alloc;

	wlan_static_scan_buf1 = kmalloc(WLAN_SCAN_BUF_SIZE, GFP_KERNEL);
	if (!wlan_static_scan_buf1)
		goto err_mem_alloc;

	wlan_static_dhd_info_buf = kmalloc(WLAN_DHD_INFO_BUF_SIZE, GFP_KERNEL);
	if (!wlan_static_dhd_info_buf)
		goto err_mem_alloc;

	printk(KERN_INFO"%s: WIFI MEM Allocated\n", __func__);
	return 0;

 err_mem_alloc:
	pr_err("Failed to mem_alloc for WLAN\n");
	for (j = 0 ; j < i ; j++)
		kfree(wlan_mem_array[j].mem_ptr);

	i = WLAN_SKB_BUF_NUM;

 err_skb_alloc:
	pr_err("Failed to skb_alloc for WLAN\n");
	for (j = 0 ; j < i ; j++)
		dev_kfree_skb(wlan_static_skb[j]);

	return -ENOMEM;
}
#endif /* CONFIG_BROADCOM_WIFI_RESERVED_MEM */

static DEFINE_MUTEX(brcm_wifi_ctrl_sync);

static void brcm_wifi_ctrl_lock(void)
{
	mutex_lock(&brcm_wifi_ctrl_sync);
}

static void brcm_wifi_ctrl_unlock(void)
{
	mutex_unlock(&brcm_wifi_ctrl_sync);
}

static struct resource brcm_wlan_resources[] = {
	[0] = {
		.name	= "bcmdhd_wlan_irq",
		.start	= IRQ_EINT(16),
		.end	= IRQ_EINT(16),
#if defined(CONFIG_BCM4335) || defined(CONFIG_BCM4335_MODULE)
		.flags	= IORESOURCE_IRQ | IORESOURCE_IRQ_SHAREABLE | IORESOURCE_IRQ_HIGHLEVEL,
#else
		.flags	= IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHEDGE,
#endif
	},
};

struct wifi_gpio_sleep_data {
	uint num;
	uint cfg;
	uint pull;
};

static struct wifi_gpio_sleep_data brcm_wifi_sleep_gpios[] = {
	/* WLAN_SDIO_CMD */
	{WLAN_SDIO_CMD, S5P_GPIO_PD_INPUT, S5P_GPIO_PD_UPDOWN_DISABLE},
	/* WLAN_SDIO_D(0) */
	{WLAN_SDIO_D0, S5P_GPIO_PD_INPUT, S5P_GPIO_PD_UPDOWN_DISABLE},
	/* WLAN_SDIO_D(1) */
	{WLAN_SDIO_D1, S5P_GPIO_PD_INPUT, S5P_GPIO_PD_UPDOWN_DISABLE},
	/* WLAN_SDIO_D(2) */
	{WLAN_SDIO_D2, S5P_GPIO_PD_INPUT, S5P_GPIO_PD_UPDOWN_DISABLE},
	/* WLAN_SDIO_D(3) */
	{WLAN_SDIO_D3, S5P_GPIO_PD_INPUT, S5P_GPIO_PD_UPDOWN_DISABLE},
};

static void (*wifi_status_cb)(struct platform_device *, int state);
#if defined(CONFIG_BCM4334) || defined(CONFIG_BCM4334_MODULE)
static void *wifi_mmc_host;
extern void mmc_ctrl_power(struct mmc_host *host, bool onoff);
#endif

#if defined(CONFIG_BCM4334) || defined(CONFIG_BCM4334_MODULE)
static int ext_cd_init_wlan(
	void (*notify_func)(struct platform_device *, int), void *mmc_host)
#else
static int ext_cd_init_wlan(
	void (*notify_func)(struct platform_device *, int))
#endif
{
	pr_debug("SCSC: registered CD callback\n");
	wifi_status_cb = notify_func;
#if defined(CONFIG_BCM4334) || defined(CONFIG_BCM4334_MODULE)
	wifi_mmc_host = mmc_host;
#endif
	return 0;
}

static int ext_cd_cleanup_wlan(
	void (*notify_func)(struct platform_device *, int))
{
	pr_debug("SCSC: unregistered CD callback\n");
	wifi_status_cb = NULL;
	return 0;
}

static int brcm_wlan_set_carddetect(int val)
{
	pr_debug("SCSC: Card Detected...\n");

	if (wifi_status_cb)
		wifi_status_cb(&exynos4_device_dwmci1, val);
	else
		pr_warning("%s: Nobody to notify\n", __func__);
	return 0;
}

static int brcm_wifi_power_state;

static unsigned int wlan_sdio_on_table[][4] = {
       {WLAN_SDIO_CLK, GPIO_WLAN_SDIO_CLK_AF, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE},
       {WLAN_SDIO_CMD, GPIO_WLAN_SDIO_CMD_AF, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE},
       {WLAN_SDIO_D0,  GPIO_WLAN_SDIO_D0_AF, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE},
       {WLAN_SDIO_D1,  GPIO_WLAN_SDIO_D1_AF, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE},
       {WLAN_SDIO_D2,  GPIO_WLAN_SDIO_D2_AF, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE},
       {WLAN_SDIO_D3,  GPIO_WLAN_SDIO_D3_AF, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE},
};

static unsigned int wlan_sdio_off_table[][4] = {
       {WLAN_SDIO_CLK, 1, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE},
       {WLAN_SDIO_CMD, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE},
       {WLAN_SDIO_D0, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE},
       {WLAN_SDIO_D1, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE},
       {WLAN_SDIO_D2, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE},
       {WLAN_SDIO_D3, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE},
};

static void s3c_config_gpio_alive_table
(int array_size, unsigned int (*gpio_table)[4])
{
       u32 i, gpio;
       printk(KERN_INFO"gpio_table = [%d] \r\n" , array_size);
       for (i = 0; i < array_size; i++) {
               gpio = gpio_table[i][0];
               s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(gpio_table[i][1]));
               s3c_gpio_setpull(gpio, gpio_table[i][3]);
               if (gpio_table[i][2] != GPIO_LEVEL_NONE)
                       gpio_set_value(gpio, gpio_table[i][2]);
       }
}

static int brcm_wlan_power(int on)
{
	int ret = 0;

	printk(KERN_INFO"------------------------------------------------");
	printk(KERN_INFO"------------------------------------------------\n");
	printk(KERN_INFO"%s Enter: power %s\n", __func__, on ? "on" : "off");

	brcm_wifi_ctrl_lock();

#if defined(CONFIG_BCM4334) || defined(CONFIG_BCM4334_MODULE)
	/* Set init CLK if WIFI is turn off */
	mmc_ctrl_power((struct mmc_host *)wifi_mmc_host,on);
#endif

	if (on) {
		gpio_set_value(GPIO_WLAN_EN, 0);
		msleep(150);
	}

	gpio_set_value(GPIO_WLAN_EN, on);
	pr_debug("SCSC: Power : GPIO = %d\n",
			gpio_get_value(GPIO_WLAN_EN));

	brcm_wifi_power_state = on;
	if (on) {
		msleep(50);
		s5p_gpio_set_pd_cfg(GPIO_WLAN_EN, S5P_GPIO_PD_OUTPUT1);
		s5p_gpio_set_pd_pull(GPIO_WLAN_EN, S5P_GPIO_PD_UPDOWN_DISABLE);
		s3c_config_gpio_alive_table(ARRAY_SIZE(wlan_sdio_on_table), wlan_sdio_on_table);
	} else {
		s5p_gpio_set_pd_cfg(GPIO_WLAN_EN, S5P_GPIO_PD_OUTPUT0);
		s5p_gpio_set_pd_pull(GPIO_WLAN_EN, S5P_GPIO_PD_UPDOWN_DISABLE);
		s3c_config_gpio_alive_table(ARRAY_SIZE(wlan_sdio_off_table), wlan_sdio_off_table);
	}

	brcm_wifi_ctrl_unlock();

	return ret;
}

static int brcm_wlan_reset_state;

static int brcm_wlan_reset(int on)
{
	pr_debug("SCSC: Reset => %d\n", on);
	pr_debug("%s: do nothing\n", __func__);
	brcm_wlan_reset_state = on;
	return 0;
}

/* Customized Locale table : OPTIONAL feature */
#define WLC_CNTRY_BUF_SZ        4
struct cntry_locales_custom {
	char iso_abbrev[WLC_CNTRY_BUF_SZ];
	char custom_locale[WLC_CNTRY_BUF_SZ];
	int  custom_locale_rev;
};

static struct cntry_locales_custom brcm_wlan_translate_custom_table[] = {
	/* Table should be filled out based
 on custom platform regulatory requirement */
	{"",   "XY", 4},  /* universal */
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
	{"KR", "XY", 3},
	{"AU", "XY", 3},
	{"CN", "XY", 3},  /* input ISO "CN" to : XY regrev 03 */
	{"TW", "XY", 3},
	{"AR", "XY", 3},
	{"MX", "XY", 3}
};

static void *brcm_wlan_get_country_code(char *ccode)
{
	int size = ARRAY_SIZE(brcm_wlan_translate_custom_table);
	int i;

	if (!ccode)
		return NULL;

	for (i = 0; i < size; i++)
		if (strcmp(ccode,
		brcm_wlan_translate_custom_table[i].iso_abbrev) == 0)
			return &brcm_wlan_translate_custom_table[i];
	return &brcm_wlan_translate_custom_table[0];
}

static struct wifi_platform_data brcm_wlan_control = {
	.set_power	= brcm_wlan_power,
	.set_reset	= brcm_wlan_reset,
	.set_carddetect	= brcm_wlan_set_carddetect,
#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM
	.mem_prealloc	= brcm_wlan_mem_prealloc,
#endif
	.get_country_code = brcm_wlan_get_country_code,
};

static struct platform_device brcm_device_wlan = {
	.name		= "bcmdhd_wlan",
	.id		= 1,
	.num_resources	= ARRAY_SIZE(brcm_wlan_resources),
	.resource	= brcm_wlan_resources,
	.dev		= {
		.platform_data = &brcm_wlan_control,
	},
};

static struct dw_mci_clk exynos_dwmci1_clk_rates[] = {
	{25 * 1000 * 1000, 100 * 1000 * 1000},
	{50 * 1000 * 1000, 200 * 1000 * 1000},
	{50 * 1000 * 1000, 200 * 1000 * 1000},
	{100 * 1000 * 1000, 400 * 1000 * 1000},
	{200 * 1000 * 1000, 800 * 1000 * 1000},
	{100 * 1000 * 1000, 400 * 1000 * 1000},
	{200 * 1000 * 1000, 800 * 1000 * 1000},
	{400 * 1000 * 1000, 800 * 1000 * 1000},
};

static int exynos_dwmci1_get_bus_wd(u32 slot_id)
{
	return 4;
}

static void exynos_dwmci1_cfg_gpio(int width)
{
	unsigned int gpio;
	pr_debug("SCSC: Config GPIO: width=%d\n", width);

	for (gpio = EXYNOS4_GPK1(0); gpio < EXYNOS4_GPK1(3); gpio++) {
		if (gpio == EXYNOS4_GPK1(2)) {
			/*NC*/
			s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(0));
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_DOWN);
		} else {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
			s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV2);
		}
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

static struct dw_mci_board smdk4270_dwmci1_pdata __initdata = {
	.num_slots	= 1,
	.cd_type	= DW_MCI_CD_EXTERNAL,

	.quirks		= DW_MCI_QUIRK_HIGHSPEED |
			  DW_MCI_QUIRK_NO_DETECT_EBIT,
#if defined(CONFIG_BCM4335) || defined(CONFIG_BCM4335_MODULE)
	.bus_hz		= 200 * 1000 * 1000,
#else
	.bus_hz		= 50 * 1000 * 1000,
#endif /* CONFIG_BCM4335 || CONFIG_BCM4335_MODULE */

#if defined(CONFIG_BCM4334) || defined(CONFIG_BCM4334_MODULE)
	.caps		= MMC_CAP_4_BIT_DATA,
#elif defined(CONFIG_BCM4335) || defined(CONFIG_BCM4335_MODULE)
	.caps		= MMC_CAP_4_BIT_DATA |
				  MMC_CAP_UHS_SDR104,
#else
	.caps		= MMC_CAP_4_BIT_DATA |
				  MMC_CAP_SDIO_IRQ,
#endif /* CONFIG_BCM4335 || CONFIG_BCM4335_MODULE */
	.caps2		= MMC_CAP2_BROKEN_VOLTAGE,
	.pm_caps	= MMC_PM_KEEP_POWER | MMC_PM_IGNORE_PM_NOTIFY,
	.fifo_depth	= 0x80,
	.detect_delay_ms	= 200,
	.hclk_name	= "dwmci",
	.cclk_name	= "sclk_dwmci",
	.cfg_gpio	= exynos_dwmci1_cfg_gpio,
	.get_bus_wd	= exynos_dwmci1_get_bus_wd,
	.ext_cd_init	= ext_cd_init_wlan,
	.ext_cd_cleanup = ext_cd_cleanup_wlan,
	.sdr_timing	= 0x03040000,
	.ddr_timing	= 0x03020000,
#if defined(CONFIG_BCM4335) || defined(CONFIG_BCM4335_MODULE)
	.clk_drv	= 0x3,
#endif /* CONFIG_BCM4335 || CONFIG_BCM4335_MODULE */
	.clk_tbl	= exynos_dwmci1_clk_rates,
#if defined(CONFIG_ARM_EXYNOS3470_BUS_DEVFREQ)
    .tp_mon_tbl = exynos_dwmci_tp_mon1_tbl,
#endif
};

static struct platform_device *smdk4270_wifi_devices[] __initdata = {
	&exynos4_device_dwmci1,
	&brcm_device_wlan,
};

static void __init scsc_mmc_configure_gpio(void)
{
	int gpio;
	int i;

	/* Setup wlan Power Enable */
	gpio = GPIO_WLAN_EN;
	s3c_gpio_cfgpin(gpio, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
	s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV3);
	/* Keep power state during suspend */
	s5p_gpio_set_pd_cfg(gpio, S5P_GPIO_PD_PREV_STATE);
	gpio_set_value(gpio, 0);


	/* Setup wlan IRQ */
	gpio = GPIO_WLAN_HOST_WAKE;
	s3c_gpio_cfgpin(gpio, S3C_GPIO_INPUT);
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
	s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV3);

	brcm_wlan_resources[0].start = gpio_to_irq(gpio);
	brcm_wlan_resources[0].end = gpio_to_irq(gpio);


	/* Setup sleep GPIO for wifi */
	for (i = 0; i < ARRAY_SIZE(brcm_wifi_sleep_gpios); i++) {
		gpio = brcm_wifi_sleep_gpios[i].num;
		s5p_gpio_set_pd_cfg(gpio, brcm_wifi_sleep_gpios[i].cfg);
		s5p_gpio_set_pd_pull(gpio, brcm_wifi_sleep_gpios[i].pull);
	}
}


int __init brcm_wlan_init(void)
{
	printk(KERN_INFO"%s: start\n", __func__);

	exynos_dwmci_set_platdata(&smdk4270_dwmci1_pdata, 1);
	dev_set_name(&exynos4_device_dwmci1.dev, "exynos4-sdhci.1");
	clk_add_alias("dwmci", "dw_mmc.1", "hsmmc", &exynos4_device_dwmci1.dev);
	clk_add_alias("sclk_dwmci", "dw_mmc.1", "sclk_mmc",
		&exynos4_device_dwmci1.dev);

	scsc_mmc_configure_gpio();

#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM
	brcm_init_wlan_mem();
#endif

	return platform_add_devices(smdk4270_wifi_devices,
			ARRAY_SIZE(smdk4270_wifi_devices));
}
