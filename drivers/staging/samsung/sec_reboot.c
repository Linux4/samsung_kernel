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

static void sec_power_off(void)
{
	int poweroff_try = 0;
	unsigned short released;
	union power_supply_propval ac_val, usb_val, wc_val;
	struct power_supply *ac_psy = power_supply_get_by_name("ac");
	struct power_supply *usb_psy = power_supply_get_by_name("usb");
	struct power_supply *wc_psy = power_supply_get_by_name("wireless");

	ac_psy->get_property(ac_psy, POWER_SUPPLY_PROP_ONLINE, &ac_val);
	usb_psy->get_property(usb_psy, POWER_SUPPLY_PROP_ONLINE, &usb_val);
	wc_psy->get_property(wc_psy, POWER_SUPPLY_PROP_ONLINE, &wc_val);

	pr_info("[%s] AC[%d] : USB[%d] : WC[%d]\n", __func__,
		ac_val.intval, usb_val.intval, wc_val.intval);

	/* This is NOT upload case */
	SEC_LAST_RR_SET(is_upload, UPLOAD_MAGIC_NORMAL);
	pr_emerg("%s: waiting for reboot\n", __func__);
	__inner_flush_dcache_all();

	while (1) {
		/* Check reboot charging */
		if (ac_val.intval || usb_val.intval || wc_val.intval || (poweroff_try >= 5)) {
			pr_emerg("%s: charger connected or power off "
					"failed(%d), reboot!\n",
					__func__, poweroff_try);
			/* To enter LP charging */
			SEC_LAST_RR_SET(is_power_reset, SEC_POWER_OFF);
			SEC_LAST_RR_SET(power_reset_reason, SEC_RESET_REASON_IN_OFFSEQ);
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
	SEC_LAST_RR_SET(is_power_reset, SEC_POWER_RESET);
	SEC_LAST_RR_SET(power_reset_reason, SEC_RESET_REASON_UNKNOWN);

	if (cmd) {
		unsigned long value;
		if (!strcmp(cmd, "fota"))
			SEC_LAST_RR_SET(power_reset_reason, SEC_RESET_REASON_FOTA);
		else if (!strcmp(cmd, "fota_bl"))
			SEC_LAST_RR_SET(power_reset_reason, SEC_RESET_REASON_FOTA_BL);
		else if (!strcmp(cmd, "recovery"))
			SEC_LAST_RR_SET(power_reset_reason, SEC_RESET_REASON_RECOVERY);
		else if (!strcmp(cmd, "download"))
			SEC_LAST_RR_SET(power_reset_reason, SEC_RESET_REASON_DOWNLOAD);
		else if (!strcmp(cmd, "upload"))
			SEC_LAST_RR_SET(power_reset_reason, SEC_RESET_REASON_UPLOAD);
		else if (!strcmp(cmd, "secure"))
			SEC_LAST_RR_SET(power_reset_reason, SEC_RESET_REASON_SECURE);
		else if (!strcmp(cmd, "fwup"))
			SEC_LAST_RR_SET(power_reset_reason, SEC_RESET_REASON_FWUP);
		else if (!strncmp(cmd, "emergency", 9))
			SEC_LAST_RR_SET(power_reset_reason, SEC_RESET_REASON_EMERGENCY);
		else if (!strncmp(cmd, "debug", 5)
			 && !kstrtoul(cmd + 5, 0, &value))
			SEC_LAST_RR_SET(power_reset_reason, SEC_RESET_SET_DEBUG | value);
		else if (!strncmp(cmd, "swsel", 5)
			 && !kstrtoul(cmd + 5, 0, &value))
			SEC_LAST_RR_SET(power_reset_reason, SEC_RESET_SET_SWSEL | value);
		else if (!strncmp(cmd, "sud", 3)
			 && !kstrtoul(cmd + 3, 0, &value))
			SEC_LAST_RR_SET(power_reset_reason, SEC_RESET_SET_SUD | value);
#ifdef CONFIG_SEC_DEBUG_MDM_SEPERATE_CRASH
		else if (!strncmp(cmd, "cpdebug", 7)
			 && !kstrtoul(cmd + 7, 0, &value))
			SEC_LAST_RR_SET(power_reset_reason, SEC_RESET_SET_CP_DEBUG | value);
#endif
#ifdef CONFIG_DIAG_MODE
		else if (!strncmp(cmd, "diag", 4)
				&& !kstrtoul(cmd + 4, 0, &value))
			SEC_LAST_RR_SET(power_reset_reason, SEC_RESET_SET_DIAG | (value & 0x1));
#endif
	}
	/* This is NOT upload case */
	SEC_LAST_RR_SET(is_upload, UPLOAD_MAGIC_NORMAL);
	sec_debug_set_upload_magic(UPLOAD_MAGIC_NORMAL, NULL);
	pr_emerg("%s: waiting for reboot\n", __func__);
	__inner_flush_dcache_all();
	wdt_arch_reset(1);
	while (1);
}

static int __init sec_reboot_init(void)
{
	/* Originally NULL */
	arm_pm_restart = sec_reboot;
	/* Originally mt_power_off */
	pm_power_off = sec_power_off;
	return 0;
}
subsys_initcall(sec_reboot_init);
