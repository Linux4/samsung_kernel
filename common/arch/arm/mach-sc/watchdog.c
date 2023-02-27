/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <mach/hardware.h>
#include <mach/watchdog.h>
#include <mach/adi.h>
#include <linux/string.h>
#include <asm/bitops.h>
#include <linux/debugfs.h>
#include <linux/module.h>

void sprd_set_reboot_mode(const char *cmd)
{
	if(cmd)
		printk("sprd_set_reboot_mode:cmd=%s\n",cmd);
	if (cmd && !(strncmp(cmd, "recovery", 8))) {
		sci_adi_raw_write(ANA_RST_STATUS, HWRST_STATUS_RECOVERY);
	} else if (cmd && !strncmp(cmd, "alarm", 5)) {
		sci_adi_raw_write(ANA_RST_STATUS, HWRST_STATUS_ALARM);
	} else if (cmd && !strncmp(cmd, "fastsleep", 9)) {
		sci_adi_raw_write(ANA_RST_STATUS, HWRST_STATUS_SLEEP);
	} else if (cmd && !strncmp(cmd, "bootloader", 10)) {
		sci_adi_raw_write(ANA_RST_STATUS, HWRST_STATUS_FASTBOOT);
	} else if (cmd && !strncmp(cmd, "panic", 5)) {
		sci_adi_raw_write(ANA_RST_STATUS, HWRST_STATUS_PANIC);
	} else if (cmd && !strncmp(cmd, "special", 7)) {
		sci_adi_raw_write(ANA_RST_STATUS, HWRST_STATUS_SPECIAL);
	} else if (cmd && !strncmp(cmd, "cftreboot", 9)) {
		sci_adi_raw_write(ANA_RST_STATUS, HWRST_STATUS_CFTREBOOT);
	} else if (cmd && !strncmp(cmd, "autodloader", 11)) {
		sci_adi_raw_write(ANA_RST_STATUS, HWRST_STATUS_AUTODLOADER);
	} else if (cmd && !strncmp(cmd, "iqmode", 6)) {
		sci_adi_raw_write(ANA_RST_STATUS, HWRST_STATUS_IQMODE);
	} else if(cmd){
		sci_adi_raw_write(ANA_RST_STATUS, HWRST_STATUS_NORMAL);
	}else{
		sci_adi_raw_write(ANA_RST_STATUS, HWRST_STATUS_SPECIAL);
	}
}

#ifdef CONFIG_ARCH_SCX35
void sprd_turnon_watchdog(unsigned int ms)
{
	uint32_t cnt;

	cnt = (ms * 1000) / WDG_CLK;

	/*enable interface clk*/
	sci_adi_set(ANA_AGEN, AGEN_WDG_EN);
	/*enable work clk*/
	sci_adi_set(ANA_RTC_CLK_EN, AGEN_RTC_WDG_EN);
	sci_adi_raw_write(WDG_LOCK, WDG_UNLOCK_KEY);
	sci_adi_set(WDG_CTRL, WDG_NEW_VER_EN);
	WDG_LOAD_TIMER_VALUE(cnt);
	sci_adi_set(WDG_CTRL, WDG_CNT_EN_BIT | WDG_RST_EN_BIT);
	sci_adi_raw_write(WDG_LOCK, (uint16_t) (~WDG_UNLOCK_KEY));
}

void sprd_turnoff_watchdog(void)
{
	sci_adi_raw_write(WDG_LOCK, WDG_UNLOCK_KEY);
	/*wdg counter stop*/
	sci_adi_clr(WDG_CTRL, WDG_CNT_EN_BIT);
	/*disable the reset mode*/
	sci_adi_clr(WDG_CTRL, WDG_RST_EN_BIT);
	sci_adi_raw_write(WDG_LOCK, (uint16_t) (~WDG_UNLOCK_KEY));
	/*stop the interface and work clk*/
	sci_adi_clr(ANA_AGEN, AGEN_WDG_EN);
	sci_adi_clr(ANA_RTC_CLK_EN, AGEN_RTC_WDG_EN);
}

#else

void sprd_turnon_watchdog(unsigned int ms)
{
	uint32_t cnt;

	cnt = (ms * 1000) / WDG_CLK;
	/* turn on watch dog clock */
	sci_adi_set(ANA_AGEN, AGEN_WDG_EN | AGEN_RTC_ARCH_EN | AGEN_RTC_WDG_EN);
	sci_adi_raw_write(WDG_LOCK, WDG_UNLOCK_KEY);
	sci_adi_clr(WDG_CTRL, WDG_INT_EN_BIT);
	WDG_LOAD_TIMER_VALUE(cnt);
	sci_adi_set(WDG_CTRL, WDG_CNT_EN_BIT | BIT(3));
	sci_adi_raw_write(WDG_LOCK, (uint16_t) (~WDG_UNLOCK_KEY));
}

void sprd_turnoff_watchdog(void)
{
	sci_adi_clr(ANA_AGEN, AGEN_WDG_EN | AGEN_RTC_ARCH_EN | AGEN_RTC_WDG_EN);
	sci_adi_raw_write(WDG_LOCK, WDG_UNLOCK_KEY);
	sci_adi_clr(WDG_CTRL, WDG_INT_EN_BIT);
	sci_adi_raw_write(WDG_LOCK, (uint16_t) (~WDG_UNLOCK_KEY));
}
#endif

static uint32_t * dump_flag = NULL;
#ifdef CONFIG_DEBUG_FS
static int flag_set(void *data, u64 val)
{
	if(dump_flag == NULL){
		printk("dump flag not initialized\n");
		return -EIO;
	}else{
		*dump_flag = val;
		return 0;
	}
}

static int flag_get(void *data, u64 *val)
{
	if(dump_flag == NULL){
		printk("dump flag not initialized\n");
		return -EIO;
	}else{
		*val = *dump_flag;
		return 0;
	}
}
DEFINE_SIMPLE_ATTRIBUTE(fatal_dump_fops, flag_get, flag_set, "%llu\n");
struct dentry *fatal_dump_dir;
#endif

#ifdef CONFIG_OF
static int sprd_wdt_probe(struct platform_device *pdev)
{
	int ret;
	struct resource *res;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		ret = -ENODEV;
	}
	return 0;
}

static struct of_device_id sprd_wdt_match_table[] = {
	{ .compatible = "sprd,watchdog", },
	{ },
};


static struct platform_driver sprd_wdt_driver = {
	.probe		= sprd_wdt_probe,
	.driver		= {
		.name	= "sprd-wdt",
		.owner	= THIS_MODULE,
		.of_match_table = sprd_wdt_match_table,
	},
};
#endif
static int __init fatal_flag_init(void)
{
	int ret = 0;
#ifdef CONFIG_ARCH_SC8825
	dump_flag = (void __iomem *)(SPRD_IRAM_BASE + SZ_16K - 4);
#else
	dump_flag = (void __iomem *)(SPRD_IRAM0_BASE + SZ_8K - 4);
#endif
	*dump_flag = 0;
#ifdef CONFIG_DEBUG_FS
	fatal_dump_dir = debugfs_create_dir("fatal_dump", NULL);
	if (IS_ERR(fatal_dump_dir) || !fatal_dump_dir) {
		printk("fatal dump failed to create debugfs directory\n");
		fatal_dump_dir = NULL;
		return 0;
	}
	debugfs_create_file("enable", S_IRUGO | S_IWUSR, fatal_dump_dir, NULL, &fatal_dump_fops);
#endif
#ifdef CONFIG_OF
	ret = platform_driver_register(&sprd_wdt_driver);
#endif
	return ret;
}
static __exit void fatal_flag_exit(void)
{
#ifdef CONFIG_DEBUG_FS
	debugfs_remove_recursive(fatal_dump_dir);
#endif
#ifdef CONFIG_OF
	platform_driver_unregister(&sprd_wdt_driver);
#endif
	return;
}
module_init(fatal_flag_init);
module_exit(fatal_flag_exit);
