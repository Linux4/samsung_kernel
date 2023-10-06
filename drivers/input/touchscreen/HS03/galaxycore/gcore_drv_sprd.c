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
void gcore_suspend(void)
{
    struct gcore_dev *gdev = fn_data.gdev;
    /*HS03 code for SR-SL6215-01-586 by duanyaoming at 20220304 start*/
    if(gdev->tp_suspend == true){
        return;
    }
    /*HS03 code for SR-SL6215-01-586 by duanyaoming at 20220304 end*/
    GTP_DEBUG("enter gcore suspend");
#if GCORE_WDT_RECOVERY_ENABLE
    cancel_delayed_work_sync(&fn_data.gdev->wdt_work);
#endif
    cancel_delayed_work_sync(&fn_data.gdev->fwu_work);

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
    if(fn_data.gdev->gesture_wakeup_en) {
        enable_irq_wake(fn_data.gdev->touch_irq);
    }
#endif
    /*HS03 code for SR-SL6215-01-586 by duanyaoming at 20220304 start*/
    fn_data.gdev->ts_stat = TS_SUSPEND;
    gdev->tp_suspend = true;
    /*HS03 code for SR-SL6215-01-586 by duanyaoming at 20220304 end*/
    GTP_DEBUG("gcore suspend end");
}

void gcore_resume(void)
{
    struct gcore_dev *gdev = fn_data.gdev;
    /*HS03 code for SR-SL6215-01-586 by duanyaoming at 20220304 start*/
    if(gdev->tp_suspend == false){
        return;
    }
    /*HS03 code for SR-SL6215-01-586 by duanyaoming at 20220304 end*/
    GTP_DEBUG("enter gcore resume");
#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
    if (gdev->gesture_wakeup_en) {
        disable_irq_wake(fn_data.gdev->touch_irq);
    }
#endif

#ifdef CONFIG_GCORE_AUTO_UPDATE_FW_HOSTDOWNLOAD
    gcore_request_firmware_update_work(NULL);
#endif
    /*HS03 code for SL6215DEV-4294 by duanyaoming at 20220517 start*/
    msleep(300);
    gcore_fw_event_notify(FW_HEADSET_UNPLUG-fn_data.gdev->earphone_status);
    msleep(10);
    gcore_fw_event_notify(FW_CHARGER_UNPLUG-fn_data.gdev->usb_detect_status);
    /*HS03 code for SL6215DEV-4294 by duanyaoming at 20220517 end*/
    /*HS03 code for SR-SL6215-01-586 by duanyaoming at 20220304 start*/
    fn_data.gdev->ts_stat = TS_NORMAL;
    gdev->tp_suspend = false;
    /*HS03 code for SR-SL6215-01-586 by duanyaoming at 20220304 end*/
    GTP_DEBUG("gcore resume end");
}

static int __init touch_driver_init(void)
{
    /*HS03 code for SR-SL6215-01-1189 by duanyaoming at 20220407 start*/
    if (lcd_name) {
        if (NULL == strstr(lcd_name,"gc7202")) {
            GTP_DEBUG("it is not gcore tp probe");
            return -ENODEV;
        } else {
            GTP_DEBUG("touch driver init.");
            if (gcore_touch_bus_init()) {
                GTP_ERROR("bus init fail!");
                return -EPERM;
            }
        }
    } else {
        GTP_ERROR("lcd_name is null");
        return -EPERM;
    }
    /*HS03 code for SR-SL6215-01-1189 by duanyaoming at 20220407 end*/
    return 0;
}

/* should never be called */
static void __exit touch_driver_exit(void)
{
    gcore_touch_bus_exit();
}

module_init(touch_driver_init);
module_exit(touch_driver_exit);

MODULE_AUTHOR("GalaxyCore, Inc.");
MODULE_DESCRIPTION("GalaxyCore Touch Main Mudule");
MODULE_LICENSE("GPL");
