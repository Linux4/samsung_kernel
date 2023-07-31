/*
 * GalaxyCore touchscreen driver
 *
 * Copyright (C) 2021 GalaxyCore Incorporated
 *
 * Copyright (C) 2021 Neo Chen <neo_chen@gcoreinc.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "gcore_drv_common.h"

static int tpd_local_init(void);
static void tpd_suspend(struct device *h);
static void tpd_resume(struct device *h);

static struct tpd_driver_t tpd_device_driver = {
	.tpd_device_name = "gcore,touchscreen",
	.tpd_local_init = tpd_local_init,
	.suspend = tpd_suspend,
	.resume = tpd_resume,
};

static int tpd_local_init(void)
{
	GTP_DEBUG("tpd_local_init start.");

	if (gcore_touch_bus_init()) {
		GTP_ERROR("bus init fail!");
		return -EPERM;
	}

	if (tpd_load_status == 0) {
		GTP_ERROR("add error touch panel driver.");
		gcore_touch_bus_exit();
		return -EPERM;
	}

	GTP_DEBUG("end %s, %d\n", __func__, __LINE__);
	tpd_type_cap = 1;

	return 0;
}

static void tpd_resume(struct device *h)
{
	GTP_ERROR("TPD resume start...");

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
	disable_irq_wake(fn_data.gdev->touch_irq);
#endif

#ifdef CONFIG_GCORE_AUTO_UPDATE_FW_HOSTDOWNLOAD
	gcore_request_firmware_update_work(NULL);
#endif
	msleep(200);
	if(fn_data.gdev->tpd_charger_flag == 1){
			gcore_fw_event_notify(FW_CHARGER_PLUG);
			GTP_ERROR("tpd_resume start FW_CHARGER_PLUG \n");
	}else if (fn_data.gdev->tpd_charger_flag == 2){
			gcore_fw_event_notify(FW_CHARGER_UNPLUG);
			GTP_ERROR("tpd_resume start FW_CHARGER_UNPLUG\n");
	}

	if(fn_data.gdev->tpd_headset_flag == 1){
			msleep(10);
			gcore_fw_event_notify(FW_HEADSET_PLUG);
			GTP_ERROR("tpd_resume start FW_HEADSET_PLUG\n");
	}else if(fn_data.gdev->tpd_headset_flag == 0){
			msleep(10);
			gcore_fw_event_notify(FW_HEADSET_UNPLUG);
			GTP_ERROR("tpd_resume start FW_HEADSET_UNPLUG\n");
	}

	fn_data.gdev->ts_stat = TS_NORMAL;
	
	GTP_ERROR("tpd resume end.");
}

static void tpd_suspend(struct device *h)
{
	GTP_ERROR("TPD suspend start...");

#if GCORE_WDT_RECOVERY_ENABLE
	cancel_delayed_work_sync(&fn_data.gdev->wdt_work);
#endif

	cancel_delayed_work_sync(&fn_data.gdev->fwu_work);

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
	enable_irq_wake(fn_data.gdev->touch_irq);
#endif

	fn_data.gdev->ts_stat = TS_SUSPEND;
		
	GTP_ERROR("TPD suspend end.");
}

static int __init tpd_driver_init(void)
{
	GTP_DEBUG("tpd_driver_init start.");
	tpd_get_dts_info();
	if (tpd_driver_add(&tpd_device_driver) < 0) {
		GTP_ERROR("add generic driver failed\n");
	}

	return 0;
}

/* should never be called */
static void __exit tpd_driver_exit(void)
{
	GTP_DEBUG("tpd_driver_exit exit\n");
	tpd_driver_remove(&tpd_device_driver);
}

module_init(tpd_driver_init);
module_exit(tpd_driver_exit);

MODULE_AUTHOR("GalaxyCore, Inc.");
MODULE_DESCRIPTION("GalaxyCore Touch Main Mudule");
MODULE_LICENSE("GPL");
