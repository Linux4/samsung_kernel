/*
 * Copyright (c) 2014-2019 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * Samsung TN common code
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/sec_ext.h>
#include <linux/sec_debug.h>

unsigned int bootmode;
EXPORT_SYMBOL(bootmode);
			        
int seccmn_chrg_set_charging_mode(char value)
{
	int ret = 0;
#if defined(CM_OFFSET)
	ret = sec_set_param(CM_OFFSET, value);
#else
	ret = -1;
#endif
	return ret;
}
EXPORT_SYMBOL(seccmn_chrg_set_charging_mode);

int seccmn_recv_is_boot_recovery(void)
{
	return (bootmode == 2);
}
EXPORT_SYMBOL(seccmn_recv_is_boot_recovery);

void seccmn_exin_set_batt_info(int cap, int volt, int temp, int curr)
{
#if defined(CONFIG_SEC_DEBUG_EXTRA_INFO)
	secdbg_exin_set_batt(cap, volt, temp, curr);
#endif
}
EXPORT_SYMBOL(seccmn_exin_set_batt_info);

static int __init bootmode_setup(char *str)
{
	get_option(&str, &bootmode);
	pr_info("%s : %d\n", __func__, bootmode);
	return 0;
}
__setup("bootmode=", bootmode_setup);
