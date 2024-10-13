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

#ifdef CONFIG_DRM
#include <drm/drm_panel.h>
#endif

void gcore_suspend(void)
{
    //struct gcore_dev *gdev = fn_data.gdev;

    GTP_DEBUG("enter gcore suspend");
    /*A06 code for AL7160A-1065 by wenghailong at 20240513 start*/
    fn_data.gdev->driver_regiser_state = 0;
    /*A06 code for AL7160A-1065 by wenghailong at 20240513 end*/

#ifdef    CONFIG_ENABLE_PROXIMITY_TP_SCREEN_OFF
    if(gcore_tpd_proximity_flag && gcore_tpd_proximity_flag_off){
        fn_data.gdev->ts_stat = TS_SUSPEND;
        GTP_DEBUG("Proximity TP Now.");
        return ;
    }

#endif

#if GCORE_WDT_RECOVERY_ENABLE
    cancel_delayed_work_sync(&fn_data.gdev->wdt_work);
#endif
    
    cancel_delayed_work_sync(&fn_data.gdev->fwu_work);

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
    if(fn_data.gdev->gesture_wakeup_en){
        enable_irq_wake(fn_data.gdev->touch_irq);
    }
#endif

#ifdef CONFIG_SAVE_CB_CHECK_VALUE
    fn_data.gdev->CB_ckstat = false;
#endif

    fn_data.gdev->ts_stat = TS_SUSPEND;

    gcore_touch_release_all_point(fn_data.gdev->input_device);
    
    GTP_DEBUG("gcore suspend end");

}

void gcore_resume(void)
{
    //struct gcore_dev *gdev = fn_data.gdev;

    GTP_DEBUG("enter gcore resume");
    /*A06 code for AL7160A-1065 by wenghailong at 20240513 start*/
    if (fn_data.gdev->driver_regiser_state == 1) {
        fn_data.gdev->driver_regiser_state = 0;
        GTP_DEBUG("TP driver register state:%d and exit gcore_resume", 
            fn_data.gdev->driver_regiser_state);
        return;
    }
    /*A06 code for AL7160A-1065 by wenghailong at 20240513 end*/

#ifdef    CONFIG_ENABLE_PROXIMITY_TP_SCREEN_OFF
            if(fn_data.gdev->PS_Enale == true){
                tpd_enable_ps(1);
            }
        
#endif


#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
    if(fn_data.gdev->gesture_wakeup_en){
        disable_irq_wake(fn_data.gdev->touch_irq);
    }
#endif
    
#ifdef CONFIG_GCORE_AUTO_UPDATE_FW_HOSTDOWNLOAD
    gcore_request_firmware_update_work(NULL);
#else
#if CONFIG_GCORE_RESUME_EVENT_NOTIFY
    queue_delayed_work(fn_data.gdev->gtp_workqueue, &fn_data.gdev->resume_notify_work, msecs_to_jiffies(200));
#endif
    gcore_touch_release_all_point(fn_data.gdev->input_device);
#endif
    fn_data.gdev->ts_stat = TS_NORMAL;

    GTP_DEBUG("gcore resume end");
}

#if defined(CONFIG_DRM)
int gcore_ts_drm_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{
    unsigned int blank;

    struct drm_panel_notifier *evdata = data;

    if (!evdata)
        return 0;

    blank = *(int *)(evdata->data);
    GTP_DEBUG("event = %d, blank = %d", event, blank);

    if (!(event == DRM_PANEL_EARLY_EVENT_BLANK || event == DRM_PANEL_EVENT_BLANK)) {
        GTP_DEBUG("event(%lu) do not need process\n", event);
        return 0;
    }

    switch (blank) {
    case DRM_PANEL_BLANK_POWERDOWN:
/* feiyu.zhu modify for tp suspend/resume */
        if (event == DRM_PANEL_EARLY_EVENT_BLANK) {
            gcore_suspend();
        }
        break;

    case DRM_PANEL_BLANK_UNBLANK:
        if (event == DRM_PANEL_EVENT_BLANK) {
            gcore_resume();
        }
        break;

    default:
        break;
    }
    return 0;
}

#elif defined(CONFIG_FB)
int gcore_ts_fb_notifier_callback(struct notifier_block *self, \
            unsigned long event, void *data)
{
    unsigned int blank;
    struct fb_event *evdata = data;

    if (!evdata)
        return 0;

    blank = *(int *)(evdata->data);
    GTP_DEBUG("event = %d, blank = %d", event, blank);

    if (!(event == FB_EARLY_EVENT_BLANK || event == FB_EVENT_BLANK)) {
        GTP_DEBUG("event(%lu) do not need process\n", event);
        return 0;
    }

    switch (blank) {
    case FB_BLANK_POWERDOWN:
        if (event == FB_EARLY_EVENT_BLANK) {
            gcore_suspend();
        }
        break;

    case FB_BLANK_UNBLANK:
        if (event == FB_EVENT_BLANK) {
            gcore_resume();
        }
        break;

    default:
        break;
    }
    return 0;

}
#endif

static int __init touch_driver_init(void)
{
    GTP_DEBUG("touch driver init.");

    if (gcore_touch_bus_init()) {
        GTP_ERROR("bus init fail!");
        return -EPERM;
    }

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
