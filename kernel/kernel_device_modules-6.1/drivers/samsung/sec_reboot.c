/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/input.h>
#include <asm/cacheflush.h>
#include <asm/system_misc.h>
#include <linux/power_supply.h>
#include <linux/sec_debug.h>
#if IS_ENABLED(CONFIG_SEC_ABC)
#include <linux/sti/abc_common.h>
#endif

#define DEBUG	1

#define psy_get_property(psy, property, value) \
{	\
	int ret;	\
	if (!psy) {	\
		pr_err("%s: Fail to get "#psy" psy\n", __func__);	\
		value.intval = 0;	\
	} else {	\
		if (psy->desc->get_property != NULL) { \
			ret = psy->desc->get_property(psy, (property), &(value)); \
			if (ret < 0) {	\
				pr_err("%s: Fail to get property of "#psy" (%d=>%d)\n", \
						__func__, (property), ret);	\
				value.intval = 0;	\
			}	\
		}	\
	}	\
}

#if IS_ENABLED(CONFIG_SEC_DEBUG_DUMP_SINK)
#define ENABLE_SDCARD_RAMDUMP		(0x73646364)
#define MAGIC_SDR_FOR_MINFORM		(0x3)
#define ENABLE_STORAGE_RAMDUMP		(0x42544456)
#define MAGIC_STR_FOR_MINFORM		(0xC)

extern unsigned int dump_sink;
extern unsigned int dumpsink_initialized;
extern void sec_set_reboot_magic(int magic);
#endif

#if 0
extern int mtk_pmic_pwrkey_status(void);

static void sec_power_off(void)
{
	int pressed;
	union power_supply_propval ac_val = {0, }, usb_val = {0, }, wc_val = {0, };
	struct power_supply *ac_psy = power_supply_get_by_name("ac");
	struct power_supply *usb_psy = power_supply_get_by_name("usb");
	struct power_supply *wc_psy = power_supply_get_by_name("wireless");

	psy_get_property(ac_psy, POWER_SUPPLY_PROP_ONLINE, ac_val);
	psy_get_property(usb_psy, POWER_SUPPLY_PROP_ONLINE, usb_val);
	psy_get_property(wc_psy, POWER_SUPPLY_PROP_ONLINE, wc_val);

	pr_info("[%s] AC[%d] : USB[%d] : WC[%d]\n", __func__,
		ac_val.intval, usb_val.intval, wc_val.intval);

	/* This is NOT upload case */
	secdbg_set_upload_magic(UPLOAD_MAGIC_NORMAL);
	pr_info("%s: waiting for power off\n", __func__);
	/* TO_DO : check cache flush api */
	//__inner_flush_dcache_all();

	while (1) {
		/* wait for power button release */
		pressed = mtk_pmic_pwrkey_status();
		if (!pressed) {
			pr_info("%s: PowerButton was released(%d)\n", __func__, pressed);
			break;
		} else {
		/* if power button is not released, wait and check TA again */
			pr_info("%s: PowerButton wasn't released(%d)\n", __func__, pressed);
		}

		mdelay(1000);
	}
}
#endif
static int sec_reboot(struct notifier_block *this, 
	unsigned long reboot_mode, void *cmd)
{
	dump_stack();
	local_irq_disable();
	pr_emerg("%s (%lu, %s)\n", __func__, reboot_mode, (char *)(cmd ? cmd : "(null)"));

	/* LPM mode prevention */
	secdbg_set_power_flag(SEC_POWER_RESET);
	secdbg_set_power_reset_reason(SEC_RESET_REASON_UNKNOWN);

#if IS_ENABLED(CONFIG_SEC_DEBUG_DUMP_SINK)
	if (dumpsink_initialized) {
		if (dump_sink == ENABLE_SDCARD_RAMDUMP) {
			sec_set_reboot_magic(MAGIC_SDR_FOR_MINFORM);
			pr_info("%s: dump_sink set to 0x%x\n", __func__, dump_sink);
		} else if (dump_sink == ENABLE_STORAGE_RAMDUMP) {
			sec_set_reboot_magic(MAGIC_STR_FOR_MINFORM);
			pr_info("%s: dump_sink set to 0x%x\n", __func__, dump_sink);
		} else {
			pr_info("%s: not sdcard Ramdump & storage Ramdump.\n", __func__);
		}
	} else {
		pr_emerg("%s: not initialized dump_sink\n", __func__);
	}
#endif

	if (cmd) {
		unsigned long value;
		if (!strcmp(cmd, "recovery-update"))
			secdbg_set_power_reset_reason(SEC_RESET_REASON_FOTA);
		else if (!strcmp(cmd, "fota_bl"))
			secdbg_set_power_reset_reason(SEC_RESET_REASON_FOTA_BL);
		else if (!strcmp(cmd, "recovery"))
			secdbg_set_power_reset_reason(SEC_RESET_REASON_RECOVERY);
		else if (!strcmp(cmd, "download"))
			secdbg_set_power_reset_reason(SEC_RESET_REASON_DOWNLOAD);
		else if (!strcmp(cmd, "bootloader"))
			secdbg_set_power_reset_reason(SEC_RESET_REASON_BOOTLOADER);
		else if (!strcmp(cmd, "upload"))
			secdbg_set_power_reset_reason(SEC_RESET_REASON_UPLOAD);
		else if (!strcmp(cmd, "secure"))
			secdbg_set_power_reset_reason(SEC_RESET_REASON_SECURE);
		else if (!strcmp(cmd, "fwup"))
			secdbg_set_power_reset_reason(SEC_RESET_REASON_FWUP);
#if IS_ENABLED(CONFIG_SEC_ABC)
		else if (!strncmp(cmd, "user_dram_test", 14) && sec_abc_get_enabled())
			secdbg_set_power_reset_reason(SEC_RESET_REASON_USER_DRAM_TEST);
#endif
		else if (!strncmp(cmd, "emergency", 9))
			secdbg_set_power_reset_reason(SEC_RESET_REASON_EMERGENCY);
		else if (!strncmp(cmd, "debug", 5)
			 && !kstrtoul(cmd + 5, 0, &value))
			secdbg_set_power_reset_reason(SEC_RESET_SET_DEBUG | value);
#if IS_ENABLED(CONFIG_SEC_DEBUG_DUMP_SINK)
		else if (!strncmp(cmd, "dump_sink", 9)
			 && !kstrtoul(cmd + 9, 0, &value))
			secdbg_set_power_reset_reason(SEC_RESET_SET_DUMPSINK | (SEC_RESET_DUMPSINK_MASK & value));
#endif
		else if (!strncmp(cmd, "swsel", 5)
			 && !kstrtoul(cmd + 5, 0, &value))
			secdbg_set_power_reset_reason(SEC_RESET_SET_SWSEL | value);
		else if (!strncmp(cmd, "sud", 3)
			 && !kstrtoul(cmd + 3, 0, &value))
			secdbg_set_power_reset_reason(SEC_RESET_SET_SUD | value);
		else if (!strncmp(cmd, "cpmem_on", 8))
			secdbg_set_power_reset_reason(SEC_RESET_CP_DBGMEM | 0x1);
		else if (!strncmp(cmd, "cpmem_off", 9))
			secdbg_set_power_reset_reason(SEC_RESET_CP_DBGMEM | 0x2);
#ifdef CONFIG_SEC_DEBUG_MDM_SEPERATE_CRASH
		else if (!strncmp(cmd, "cpdebug", 7)
			 && !kstrtoul(cmd + 7, 0, &value))
			secdbg_set_power_reset_reason(SEC_RESET_SET_CP_DEBUG | value);
#endif
#ifdef CONFIG_DIAG_MODE
		else if (!strncmp(cmd, "diag", 4)
				&& !kstrtoul(cmd + 4, 0, &value))
			secdbg_set_power_reset_reason(SEC_RESET_SET_DIAG | (value & 0x1));
#endif
	}
	/* This is NOT upload case */
	secdbg_set_upload_magic(UPLOAD_MAGIC_NORMAL);
	pr_emerg("%s: waiting for reboot\n", __func__);

	/* TO_DO : check cache flush api */
	//__inner_flush_dcache_all();

	return NOTIFY_DONE;
}

static struct notifier_block sec_restart_nb = {
	.notifier_call = sec_reboot,
	.priority = 130,
};

static int __init sec_reboot_init(void)
{
	int err;

	pr_emerg("%s\n", __func__);

	err = register_restart_handler(&sec_restart_nb);
	if (err) {
		pr_emerg("cannot register restart handler (err=%d)\n", err);
	}

#if IS_ENABLED(CONFIG_SEC_DEBUG_DUMP_SINK)
	if (dump_sink == ENABLE_SDCARD_RAMDUMP) {
		sec_set_reboot_magic(MAGIC_SDR_FOR_MINFORM);
		pr_info("%s: dump_sink set to 0x%x\n", __func__, dump_sink);
	} else if (dump_sink == ENABLE_STORAGE_RAMDUMP) {
		sec_set_reboot_magic(MAGIC_STR_FOR_MINFORM);
		pr_info("%s: dump_sink set to 0x%x\n", __func__, dump_sink);
	} else {
		pr_info("%s: not sdcard Ramdump & storage Ramdump.\n", __func__);
	}
#endif

	//pm_power_off = sec_power_off;

	return 0;
}
subsys_initcall(sec_reboot_init);

MODULE_DESCRIPTION("Samsung Reboot driver");
MODULE_LICENSE("GPL v2");
