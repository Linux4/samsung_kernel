/*
 * drivers/misc/samsung/scsc/mxman_sysevent.c
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "mxman_sysevent.h"
#if IS_ENABLED(CONFIG_EXYNOS_SYSTEM_EVENT)
#include <linux/platform_device.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
#include <soc/samsung/exynos/sysevent_notif.h>
#else
#include <soc/samsung/sysevent_notif.h>
#endif

int wlbt_sysevent_powerup(const struct sysevent_desc *sysevent)
{
	/*
	 * This function is called in syseventtem_get / restart
	 * TODO: Subsystem Power Up related
	 */
	 pr_info("%s: call-back function\n", __func__);
	return 0;
}

int wlbt_sysevent_shutdown(const struct sysevent_desc *sysevent, bool force_stop)
{
	/*
	 * This function is called in syseventtem_put / restart
	 * TODO: Subsystem Shutdown related
	 */
	pr_info("%s: call-back function - force_stop:%d\n", __func__, force_stop);
	return 0;
}

int wlbt_sysevent_ramdump(int enable, const struct sysevent_desc *sysevent)
{
	/*
	 * This function is called in syseventtem_put / restart
	 * TODO: Ramdump related
	 */
	pr_info("%s: call-back function - enable(%d)\n", __func__, enable);
	return 0;

}

void wlbt_sysevent_crash_shutdown(const struct sysevent_desc *sysevent)
{
	/*
	 * This function is called in panic handler
	 * TODO: Subsystem Crash Shutdown related
	 */
	pr_info("%s: call-back function\n", __func__);
}

int wlbt_sysevent_notifier_cb(struct notifier_block *nb,
						unsigned long code, void *nb_data)
{
	struct notif_data *notifdata = NULL;
	notifdata = (struct notif_data *) nb_data;

	if (!notifdata) {
		pr_info("nb_data parameter is null\n");
		return NOTIFY_DONE;
	}

	switch (code) {
	case SYSTEM_EVENT_BEFORE_SHUTDOWN:
	case SYSTEM_EVENT_AFTER_SHUTDOWN:
	case SYSTEM_EVENT_RAMDUMP_NOTIFICATION:
	case SYSTEM_EVENT_AFTER_POWERUP:
		pr_info("%s: %s: %s\n", __func__, notifdata->pdev->name,
			__stringify(code));
		break;
	case SYSTEM_EVENT_BEFORE_POWERUP:
		pr_info("%s: %s: %s, crash_status:%d, enable_ramdump:%d\n",
			__func__, notifdata->pdev->name,
			__stringify(code),
			notifdata->crashed, notifdata->enable_ramdump);
		break;
	default:
		break;
	}
	return NOTIFY_DONE;
}

#endif
