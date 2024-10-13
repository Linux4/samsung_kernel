// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
 */

#include "custom_sysfs.h"
#define DEV_T 222

struct class *g_display_class = NULL;
struct device *g_display_dev = NULL;
static struct dsi_display *gs_display = NULL;

/*M55 code for SR-QN6887A-01-432 by hehaoran at 20231019 start*/
/*M55 code for QN6887A-2738 by yuli at 20231208 start*/
static ssize_t hbm_mode_set_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
   return sprintf(buf, "%d\n", atomic_read(&gs_display->panel->dsi_hbm_enabled));
}

static ssize_t hbm_mode_set_store(struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
    int ret = 0;
    unsigned int input_val = 0;

    ret = kstrtoint(buf, 10, &input_val);
    if (ret) {
        DSI_ERR("hbm mode set input error ret=%d\n",ret);
    } else {
        DSI_INFO("hbm_mode set input_val  = %d", input_val);
    }
    /*M55 code for P231107-03127 by hehaoran at 20240128 start*/
    if (display_get_tui_is_enabled(gs_display)) {
        DSI_ERR("%s:tui can be enabled ,cant op hbm!\n",__func__);
        return count;
    }
    /*M55 code for P231107-03127 by hehaoran at 20240128 end*/
    switch (input_val) {
        case HBM_ENABEL:
            ret = dsi_panel_hbm_enable(gs_display->panel);
            if (ret) {
                DSI_ERR("hbm cmd send faild!\n");
            } else {
                DSI_INFO("hbm cmd send success! \n");
                atomic_set(&gs_display->panel->dsi_hbm_enabled, HBM_ENABEL);
            }
            break;
        case HBM_DISENABEL:
            ret = dsi_panel_hbm_disable(gs_display->panel);
            if (ret) {
                DSI_ERR("hbm send disable cmd faild!\n");
            } else {
                atomic_set(&gs_display->panel->dsi_hbm_enabled, HBM_DISENABEL);
                DSI_INFO("hbm disable success, backlight will be setted max normal!\n");
            }
            break;
        default:
            DSI_ERR("hbm mode input_val =%d is error,not this value!\n", input_val);
    }
    return count;
}
/*M55 code for SR-QN6887A-01-432 by hehaoran at 20231019 end*/
static ssize_t lhbm_mode_set_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
   return sprintf(buf, "%d\n", atomic_read(&gs_display->panel->dsi_lhbm_enabled));
}

static ssize_t lhbm_mode_set_store(struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
    int ret = 0;
    unsigned int input_val = 0;

    ret = kstrtoint(buf, 10, &input_val);
    if (ret) {
        DSI_ERR("lhbm mode set input error ret=%d\n",ret);
    } else {
        DSI_INFO("lhbm_mode set input_val  = %d", input_val);
    }
    /*M55 code for P231107-03127 by hehaoran at 20240128 start*/
    if (display_get_tui_is_enabled(gs_display)) {
        DSI_ERR("%s:tui can be enabled ,cant op lhbm!\n",__func__);
        return count;
    }
    /*M55 code for P231107-03127 by hehaoran at 20240128 end*/
    switch (input_val) {
        case LHBM_ENABEL:
            ret = dsi_panel_lhbm_enable(gs_display->panel);
            if (ret) {
                DSI_ERR("lhbm send enable cmd faild!\n");
            } else {
                atomic_set(&gs_display->panel->dsi_lhbm_enabled, LHBM_ENABEL);
                DSI_INFO("lhbm send enable cmd success!\n");
            }
            break;
        case LHBM_DISENABEL:
            ret = dsi_panel_lhbm_disable(gs_display->panel);
            if (ret) {
                DSI_ERR("lhbm send disable cmd faild!\n");
            } else {
                atomic_set(&gs_display->panel->dsi_lhbm_enabled, LHBM_DISENABEL);
                DSI_INFO("lhbm send disable cmd success!\n");
            }
            break;
        default:
            DSI_ERR("lhbm mode input_val =%d is error,not this value!\n", input_val);
    }
    return count;
}
/*M55 code for QN6887A-2738 by yuli at 20231208 end*/

/*M55 code for QN6887A-2550 by hehaoran at 20231205 start*/
/*M55 code for QN6887A-2550 by hehaoran at 20231214 start*/
static ssize_t ss_disp_cell_id_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    char cell_id[DSI_CELL_ID_LEN] = {0};
    struct dsi_display *dsi_display = gs_display;
    if (IS_ERR_OR_NULL(dsi_display)) {
        return -EFAULT;
    }
    if (IS_ERR_OR_NULL(dsi_display->panel)) {
        return -EFAULT;
    }

    dsi_display_read_cell_id(dsi_display, cell_id);
    DSI_INFO("%s:cell_id = %s",__func__,cell_id);
    return snprintf(buf, sizeof(cell_id), "%s", cell_id);
}
/*M55 code for QN6887A-2550 by hehaoran at 20231214 end*/
static ssize_t ss_disp_window_type_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    char window_type[3] = {0};
    struct dsi_display *dsi_display = gs_display;
    if (IS_ERR_OR_NULL(dsi_display)) {
        return -EFAULT;
    }
    if (IS_ERR_OR_NULL(dsi_display->panel)) {
        return -EFAULT;
    }

    if (strstr(dsi_display->panel->panel_of_node->name, "icna3512_visionox")) {
        strcpy(window_type, ICNA3512_GOV_WINDOW_TYPE);
    } else if (strstr(dsi_display->panel->panel_of_node->name, "icna3512_tianma")) {
        strcpy(window_type, ICNA3512_TM_WINDOW_TYPE);
    } else {
        DSI_ERR("%s current panel no panel info\n", __func__);
        strcpy(window_type, DEFAULT_WINDOW_TYPE);
    }

    return snprintf(buf, sizeof(window_type), "%s", window_type);
}

static DEVICE_ATTR(lhbm_mode_set, 0644, lhbm_mode_set_show,
    lhbm_mode_set_store);
/*M55 code for SR-QN6887A-01-432 by hehaoran at 20231019 start*/
static DEVICE_ATTR(hbm_mode_set, 0644, hbm_mode_set_show,
    hbm_mode_set_store);
static DEVICE_ATTR(cell_id, 0440, ss_disp_cell_id_show, NULL);
static DEVICE_ATTR(window_type, 0440, ss_disp_window_type_show, NULL);
/*M55 code for SR-QN6887A-01-390 by yuli at 20230908 start*/
static struct attribute *display_sysfs_attrs[] = {
    &dev_attr_lhbm_mode_set.attr,
    &dev_attr_hbm_mode_set.attr,
    &dev_attr_cell_id.attr,
    &dev_attr_window_type.attr,
    NULL,
};
/*M55 code for SR-QN6887A-01-390 by yuli at 20230908 end*/
/*M55 code for SR-QN6887A-01-432 by hehaoran at 20231019 end*/
/*M55 code for QN6887A-2550 by hehaoran at 20231205 end*/
struct attribute_group display_sysfs_attr_group = {
    .attrs = display_sysfs_attrs,
};

int display_create_custom_sysfs(struct dsi_display *display)
{
    int rc = 0;
    if (display == NULL) {
        DSI_ERR("display_create_custom_sysfs display is NULL\n");
        return -ENODEV;
    }

    gs_display = display;

    // sys/class/display/display_ctrl/lhbm_mode_set
    g_display_class = class_create(THIS_MODULE, "display");
    if (unlikely(IS_ERR(g_display_class))) {
        DSI_ERR("Failed to create class(display) %ld\n", PTR_ERR(g_display_class));
        return PTR_ERR(g_display_class);
    }

    g_display_dev = device_create(g_display_class, NULL, MKDEV(DEV_T, 0), NULL, "display_ctrl");
    if (IS_ERR(g_display_dev)) {
        DSI_ERR("Failed to create device(display_ctrl)!\n");
        return -ENODEV;
    }

    rc = sysfs_create_group(&g_display_dev->kobj, &display_sysfs_attr_group);
    if (rc) {
        DSI_ERR("sysfs group creation failed, rc=%d\n", rc);
    }

    /*M55 code for QN6887A-2738 by yuli at 20231208 start*/
    if (gs_display->panel) {
        atomic_set(&gs_display->panel->dsi_lhbm_enabled, LHBM_DISENABEL);
        atomic_set(&gs_display->panel->dsi_hbm_enabled, HBM_DISENABEL);
    } else {
        DSI_ERR("gs_display->panel is NULL\n");
    }
    /*M55 code for QN6887A-2738 by yuli at 20231208 end*/

    return rc;
}

void display_remove_custom_sysfs(void)
{
    sysfs_remove_group(&g_display_dev->kobj, &display_sysfs_attr_group);
}
