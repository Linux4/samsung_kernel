/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define DEBUG	1

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
#if defined(CONFIG_SEC_ABC)
#include <linux/sti/abc_common.h>
#endif

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

static void sec_power_off(void)
{
	int poweroff_try = 0;
	unsigned short released;
	union power_supply_propval ac_val, usb_val, wc_val;
	struct power_supply *ac_psy = power_supply_get_by_name("ac");
	struct power_supply *usb_psy = power_supply_get_by_name("usb");
	struct power_supply *wc_psy = power_supply_get_by_name("wireless");

	psy_get_property(ac_psy, POWER_SUPPLY_PROP_ONLINE, ac_val);
	psy_get_property(usb_psy, POWER_SUPPLY_PROP_ONLINE, usb_val);
	psy_get_property(wc_psy, POWER_SUPPLY_PROP_ONLINE, wc_val);

	pr_info("[%s] AC[%d] : USB[%d] : WC[%d]\n", __func__,
		ac_val.intval, usb_val.intval, wc_val.intval);

	/* This is NOT upload case */
	LAST_RR_SET(is_upload, UPLOAD_MAGIC_NORMAL);
	pr_emerg("%s: waiting for reboot\n", __func__);
	__inner_flush_dcache_all();

	while (1) {
		/* Check reboot charging */
		if (ac_val.intval || usb_val.intval || wc_val.intval || (poweroff_try >= 5)) {
			pr_emerg("%s: charger connected or power off "
					"failed(%d), reboot!\n",
					__func__, poweroff_try);
			/* To enter LP charging */
			LAST_RR_SET(is_power_reset, SEC_POWER_OFF);
			LAST_RR_SET(power_reset_reason, SEC_RESET_REASON_IN_OFFSEQ);
			wdt_arch_reset(1);
			while (1);
		}

		/* wait for power button release */
		released = pmic_get_register_value_nolock(PMIC_PWRKEY_DEB);
		if (released) {
			pr_info("%s: PowerButton was released(%d)\n", __func__, released);
			mt_power_off();
			++poweroff_try;
			pr_emerg
			    ("%s: Should not reach here! (poweroff_try:%d)\n",
			     __func__, poweroff_try);
		} else {
		/* if power button is not released, wait and check TA again */
			pr_info("%s: PowerButton wasn't released(%d)\n", __func__, released);
		}
		mdelay(1000);
	}
}

static void sec_reboot(enum reboot_mode reboot_mode, const char *cmd)
{
	local_irq_disable();
	pr_emerg("%s (%d, %s)\n", __func__, reboot_mode, cmd ? cmd : "(null)");

	/* LPM mode prevention */
	LAST_RR_SET(is_power_reset, SEC_POWER_RESET);
	LAST_RR_SET(power_reset_reason, SEC_RESET_REASON_UNKNOWN);

	if (cmd) {
		unsigned long value;
		if (!strcmp(cmd, "fota"))
			LAST_RR_SET(power_reset_reason, SEC_RESET_REASON_FOTA);
		else if (!strcmp(cmd, "fota_bl"))
			LAST_RR_SET(power_reset_reason, SEC_RESET_REASON_FOTA_BL);
		else if (!strcmp(cmd, "recovery"))
			LAST_RR_SET(power_reset_reason, SEC_RESET_REASON_RECOVERY);
		else if (!strcmp(cmd, "download"))
			LAST_RR_SET(power_reset_reason, SEC_RESET_REASON_DOWNLOAD);
		else if (!strcmp(cmd, "bootloader"))
			LAST_RR_SET(power_reset_reason, SEC_RESET_REASON_BOOTLOADER);
		else if (!strcmp(cmd, "upload"))
			LAST_RR_SET(power_reset_reason, SEC_RESET_REASON_UPLOAD);
		else if (!strcmp(cmd, "secure"))
			LAST_RR_SET(power_reset_reason, SEC_RESET_REASON_SECURE);
		else if (!strcmp(cmd, "fwup"))
			LAST_RR_SET(power_reset_reason, SEC_RESET_REASON_FWUP);
#if defined(CONFIG_SEC_ABC)
		else if (!strncmp(cmd, "user_dram_test", 14) && sec_abc_get_enabled())
			LAST_RR_SET(power_reset_reason, SEC_RESET_REASON_USER_DRAM_TEST);
#endif
		else if (!strncmp(cmd, "emergency", 9))
			LAST_RR_SET(power_reset_reason, SEC_RESET_REASON_EMERGENCY);
		else if (!strncmp(cmd, "debug", 5)
			 && !kstrtoul(cmd + 5, 0, &value))
			LAST_RR_SET(power_reset_reason, SEC_RESET_SET_DEBUG | value);
		else if (!strncmp(cmd, "swsel", 5)
			 && !kstrtoul(cmd + 5, 0, &value))
			LAST_RR_SET(power_reset_reason, SEC_RESET_SET_SWSEL | value);
		else if (!strncmp(cmd, "sud", 3)
			 && !kstrtoul(cmd + 3, 0, &value))
			LAST_RR_SET(power_reset_reason, SEC_RESET_SET_SUD | value);
		else if (!strncmp(cmd, "cpmem_on", 8))
			LAST_RR_SET(power_reset_reason, SEC_RESET_CP_DBGMEM | 0x1);
		else if (!strncmp(cmd, "cpmem_off", 9))
			LAST_RR_SET(power_reset_reason, SEC_RESET_CP_DBGMEM | 0x2);
#ifdef CONFIG_SEC_DEBUG_MDM_SEPERATE_CRASH
		else if (!strncmp(cmd, "cpdebug", 7)
			 && !kstrtoul(cmd + 7, 0, &value))
			LAST_RR_SET(power_reset_reason, SEC_RESET_SET_CP_DEBUG | value);
#endif
#ifdef CONFIG_DIAG_MODE
		else if (!strncmp(cmd, "diag", 4)
				&& !kstrtoul(cmd + 4, 0, &value))
			LAST_RR_SET(power_reset_reason, SEC_RESET_SET_DIAG | (value & 0x1));
#endif
	}
	/* This is NOT upload case */
	LAST_RR_SET(is_upload, UPLOAD_MAGIC_NORMAL);
	pr_emerg("%s: waiting for reboot\n", __func__);

	wd_dram_reserved_mode(false);

	__inner_flush_dcache_all();
	wdt_arch_reset(1);
	while (1);
}

static int __init sec_reboot_init(void)
{
	pm_power_off = sec_power_off;
	arm_pm_restart = sec_reboot;
	return 0;
}
subsys_initcall(sec_reboot_init);
