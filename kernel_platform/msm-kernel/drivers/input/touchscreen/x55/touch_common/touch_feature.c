#include "touch_feature.h"

static struct platform_device hwinfo_device = {
    .name = HWINFO_NAME,
    .id = -1,
};

static struct proc_dir_entry *proc_tpinfo_file = NULL;

static void get_fw_ver_bin(void *device_data);
static void get_fw_ver_ic(void *device_data);
static void aot_enable(void *device_data);
static void high_sensitivity_mode(void *device_data);
static void not_support_cmd(void *device_data);

/*M55 code for SR-QN6887A-01-370|431 by yuli at 20231012 start*/
struct lcm_info *gs_common_lcm_info = NULL;
static void singletap_enable(void *device_data);
static void fod_enable(void *device_data);
static void set_fod_rect(void *device_data);
/*M55 code for P231221-04897 by yuli at 20240106 start*/
static void set_aod_rect(void *device_data);
/*M55 code for P231221-04897 by yuli at 20240106 end*/
/*M55 code for P240111-04531 by yuli at 20240116 start*/
static void spay_enable(void *device_data);
/*M55 code for P240111-04531 by yuli at 20240116 end*/

struct sec_cmd sec_cmds[] = {
    {SEC_CMD("get_fw_ver_bin", get_fw_ver_bin),},
    {SEC_CMD("get_fw_ver_ic", get_fw_ver_ic),},
    {SEC_CMD("aot_enable", aot_enable),},
    {SEC_CMD("glove_mode", high_sensitivity_mode),},
    {SEC_CMD("not_support_cmd", not_support_cmd),},
    {SEC_CMD("singletap_enable", singletap_enable),},
    {SEC_CMD_H("fod_enable", fod_enable),},
    {SEC_CMD("set_fod_rect", set_fod_rect),},
    /*M55 code for P231221-04897 by yuli at 20240106 start*/
    {SEC_CMD("set_aod_rect", set_aod_rect),},
    /*M55 code for P231221-04897 by yuli at 20240106 end*/
    /*M55 code for P240111-04531 by yuli at 20240116 start*/
    {SEC_CMD_H("spay_enable", spay_enable),},
    /*M55 code for P240111-04531 by yuli at 20240116 end*/
};
/*M55 code for SR-QN6887A-01-370|431 by yuli at 20231012 end*/

/*M55 code for SR-QN6887A-01-365 by wenghailong at 202300821 start*/
bool tp_get_boot_mode(void) {
    struct device_node *np_chosen = NULL;
    const char *touchload = NULL;

    np_chosen = of_find_node_by_path("/chosen");
    if (!np_chosen) {
        np_chosen = of_find_node_by_path("/chosen@0");
    }
    of_property_read_string(np_chosen, "bootargs", &touchload);
    if (!touchload) {
        TP_ERROR("%s: it is not normal boot!", __func__);
    } else if (strnstr(touchload, "touch.load=1", strlen(touchload))) {
        TP_INFO("%s: it is normal boot!", __func__);
        return true;
    }

    return false;
}
EXPORT_SYMBOL(tp_get_boot_mode);
/*M55 code for SR-QN6887A-01-365 by wenghailong at 202300821 end*/

/*M55 code for QN6887A-45 by yubo at 20230911 start*/
bool tp_get_pcbainfo(const char *pcbainfo)
{
    const char *bootparams = NULL;
    bool found = false;
    struct device_node *np = of_find_node_by_path("/chosen");
    if (!np) {
        TP_ERROR("%s: failed to find node", __func__);
        return false;
    }

    if (of_property_read_string(np, "bootargs", &bootparams)) {
        TP_ERROR("%s: failed to get bootargs property", __func__);
        of_node_put(np); // Free the node reference
        return false;
    }

    found = strnstr(bootparams, pcbainfo, strlen(bootparams)) ? true : false;
    if (found) {
        TP_INFO("%s: %s", __func__, pcbainfo);
    }

    of_node_put(np); // Free the node reference
    return found;
}
EXPORT_SYMBOL(tp_get_pcbainfo);
/*M55 code for QN6887A-45 by yubo at 20230911 end*/

/**
 * This function get_fw_ver_bin
 *
 * @Param[]：void *device_data
 * @Return ：
 */
static void get_fw_ver_bin(void *device_data)
{
    char buff[SEC_CMD_STR_LEN] = { 0 };
    struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
    sec_cmd_set_default_result(sec);
    if (gs_common_lcm_info->module_name != NULL) {
        snprintf(buff, sizeof(buff), "%s_BIN=V%02x", gs_common_lcm_info->module_name,
            *(gs_common_lcm_info->fw_version) & 0xFF);
    } else {
        snprintf(buff, sizeof(buff), "NA_BIN=VNA");
    }
    sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
    sec->cmd_state = SEC_CMD_STATUS_OK;
    sec_cmd_set_cmd_exit(sec);
    TP_INFO("[SEC]: %s", buff);
}
/**
 * This function get_fw_ver_ic
 *
 * @Param[]：void *device_data
 * @Return ：
 */
static void get_fw_ver_ic(void *device_data)
{
    struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
    char buff[SEC_CMD_STR_LEN] = { 0 };
    sec_cmd_set_default_result(sec);

    if (gs_common_lcm_info->module_name != NULL) {
        snprintf(buff, sizeof(buff), "IC=%s",gs_common_lcm_info->module_name);
    } else {
        snprintf(buff, sizeof(buff), "IC=NA");
    }

    sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
    sec->cmd_state = SEC_CMD_STATUS_OK;
    sec_cmd_set_cmd_exit(sec);
    TP_INFO("[SEC]: %s\n",buff);
}
/**
 * This function disable_gesture_wakeup
 *
 * @Param[]：void
 * @Return ：
 */
void disable_gesture_wakeup(void)
{
    TP_INFO("Disable gesture wakeup");
    if (gs_common_lcm_info->gesture_wakeup_disable != NULL) {
        gs_common_lcm_info->gesture_wakeup_disable();
    }
}
/**
 * This function enable_gesture_wakeup
 *
 * @Param[]：void
 * @Return ：
 */
void enable_gesture_wakeup(void)
{
    TP_INFO("Enable gesture wakeup");
    if (gs_common_lcm_info->gesture_wakeup_enable != NULL) {
        gs_common_lcm_info->gesture_wakeup_enable();
    }
}
/**
 * This function aot_enable
 *
 * @Param[]：void *device_data
 * @Return ：
 */
static void aot_enable(void *device_data)
{
    struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
    char buff[SEC_CMD_STR_LEN] = { 0 };

    sec_cmd_set_default_result(sec);

    if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
        snprintf(buff, sizeof(buff), "%s", "NG");
        sec->cmd_state = SEC_CMD_STATUS_FAIL;
    } else {
        if (sec->cmd_param[0] == 0) {
            disable_gesture_wakeup();
        } else {
            enable_gesture_wakeup();
        }
        snprintf(buff, sizeof(buff), "%s", "OK");
        sec->cmd_state = SEC_CMD_STATUS_OK;
    }
    sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
    sec->cmd_state = SEC_CMD_STATUS_WAITING;
    sec_cmd_set_cmd_exit(sec);

    TP_INFO("[SEC]%s: %s\n", __func__, buff);

}

/**
 * This function disable_high_sensitivity
 *
 * @Param[]：void
 * @Return ：
 */
void disable_high_sensitivity(void)
{
    TP_INFO("Disable high sensitivity");
}
/**
 * This function enable_high_sensitivity
 *
 * @Param[]：void
 * @Return ：
 */
void enable_high_sensitivity(void)
{
    TP_INFO("Enable high sensitivity");
}
/**
 * This function high_sensitivity_mode
 *
 * @Param[]：void *device_data
 * @Return ：
 */
static void high_sensitivity_mode(void *device_data)
{
    struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
    char buff[SEC_CMD_STR_LEN] = { 0 };

    sec_cmd_set_default_result(sec);

    if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
        snprintf(buff, sizeof(buff), "%s", "NG");
        sec->cmd_state = SEC_CMD_STATUS_FAIL;
    } else {
        if (sec->cmd_param[0] == 0) {
            disable_high_sensitivity();
        } else {
            enable_high_sensitivity();
        }
        snprintf(buff, sizeof(buff), "%s", "OK");
        sec->cmd_state = SEC_CMD_STATUS_OK;
    }
    sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
    sec->cmd_state = SEC_CMD_STATUS_WAITING;
    sec_cmd_set_cmd_exit(sec);

    TP_INFO("[SEC]%s: %s\n", __func__, buff);

}

/**
 * This function not_support_cmd
 *
 * @Param[]：void *device_data
 * @Return ：
 */
static void not_support_cmd(void *device_data)
{
    struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
    char buff[SEC_CMD_STR_LEN] = { 0 };

    sec_cmd_set_default_result(sec);
    snprintf(buff, sizeof(buff), "%s", "NA");
    sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
    sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
    sec_cmd_set_cmd_exit(sec);

    TP_INFO("[SEC]%s: %s\n", __func__, buff);
}

/*M55 code for SR-QN6887A-01-370|431 by yuli at 20231012 start*/
static void singletap_enable(void *device_data)
{
    struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
    char buff[SEC_CMD_STR_LEN] = { 0 };

    TP_INFO("+++");
    sec_cmd_set_default_result(sec);

    if (!gs_common_lcm_info) {
        TP_INFO("%s %s: gs_common_lcm_info is NULL");
        snprintf(buff, sizeof(buff), "%s", "NG");
        sec->cmd_state = SEC_CMD_STATUS_FAIL;
        goto END;
    }

    /*M55 code for P231023-06212 by yuli at 20231024 start*/
    switch (sec->cmd_param[0]) {
        case 0:
            if (gs_common_lcm_info->single_tap_disable != NULL) {
                gs_common_lcm_info->single_tap_disable();
                snprintf(buff, sizeof(buff), "%s", "OK");
                sec->cmd_state = SEC_CMD_STATUS_OK;
            } else {
                TP_INFO("single_tap_disable is NULL");
                snprintf(buff, sizeof(buff), "%s", "NG");
                sec->cmd_state = SEC_CMD_STATUS_FAIL;
            }
            break;
        case 1:
            if (gs_common_lcm_info->single_tap_enable != NULL) {
                gs_common_lcm_info->single_tap_enable();
                snprintf(buff, sizeof(buff), "%s", "OK");
                sec->cmd_state = SEC_CMD_STATUS_OK;
            } else {
                TP_INFO("single_tap_enable is NULL");
                snprintf(buff, sizeof(buff), "%s", "NG");
                sec->cmd_state = SEC_CMD_STATUS_FAIL;
            }
            break;
        default:
            snprintf(buff, sizeof(buff), "%s", "NG");
            sec->cmd_state = SEC_CMD_STATUS_FAIL;
            break;
    }
    /*M55 code for P231023-06212 by yuli at 20231024 end*/

END:
    sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
    sec->cmd_state = SEC_CMD_STATUS_WAITING;
    sec_cmd_set_cmd_exit(sec);
    TP_INFO("%s, ---", buff);
}

static void fod_enable(void *device_data)
{
    struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
    char buff[SEC_CMD_STR_LEN] = { 0 };
    u8 value = 0;

    TP_INFO("+++");
    sec_cmd_set_default_result(sec);

    if (!gs_common_lcm_info) {
        TP_INFO("%s %s: gs_common_lcm_info is NULL");
        goto NG;
    }

    if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
        goto NG;
    } else {
        TP_INFO("fod: %s, fast:%s, strict:%s\n", sec->cmd_param[0] ? "on" : "off",
            sec->cmd_param[1] ? "on" : "off", sec->cmd_param[2] ? "on" : "off");
        if (sec->cmd_param[0] == 0) {
            value = sec->cmd_param[0];
        } else {
            value = ((sec->cmd_param[0] & 0x01) << 1) | ((sec->cmd_param[1] & 0x01) << 6)
                | ((sec->cmd_param[2] & 0x01) << 7);
        }

        TP_INFO("value = 0x%x", value);
        if (gs_common_lcm_info->fod_enable != NULL) {
            gs_common_lcm_info->fod_enable(value);
            goto OK;
        } else {
            TP_INFO("fod_enable is NULL");
            goto NG;
        }
    }

OK:
    snprintf(buff, sizeof(buff), "%s", "OK");
    sec->cmd_state = SEC_CMD_STATUS_OK;
    sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
    sec_cmd_set_cmd_exit(sec);
    TP_INFO("---");
    return;
NG:
    snprintf(buff, sizeof(buff), "%s", "NG");
    sec->cmd_state = SEC_CMD_STATUS_FAIL;
    sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
    sec_cmd_set_cmd_exit(sec);
}

static void set_fod_rect(void *device_data)
{
    struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
    char buff[SEC_CMD_STR_LEN] = { 0 };
    int i = 0, ret = 0;

    TP_INFO("+++");
    sec_cmd_set_default_result(sec);
    if (!gs_common_lcm_info) {
        TP_INFO("%s %s: gs_common_lcm_info is NULL");
        goto NG;
    }
    TP_INFO("l:%d, t:%d, r:%d, b:%d", sec->cmd_param[0], sec->cmd_param[1],
            sec->cmd_param[2], sec->cmd_param[3]);

    /*M55 code for P231023-06212 by yuli at 20231024 start*/
    if (!gs_common_lcm_info->ts_data_info) {
        TP_INFO("%s %s: ts_data_info is NULL", SECLOG, __func__);
        goto NG;
    }
    /*M55 code for P231023-06212 by yuli at 20231024 end*/

    for (i = 0; i < 4; i++){
        gs_common_lcm_info->ts_data_info->fod_rect_data[i] = sec->cmd_param[i];
    }

    if (gs_common_lcm_info->set_fod_rect != NULL) {
        ret = gs_common_lcm_info->set_fod_rect(gs_common_lcm_info->ts_data_info->fod_rect_data);
        if (ret) {
            goto NG;
        }
        goto OK;
    } else {
        TP_INFO("set_fod_rect is NULL");
        goto NG;
    }

OK:
    snprintf(buff, sizeof(buff), "OK");
    sec->cmd_state = SEC_CMD_STATUS_OK;
    sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
    sec_cmd_set_cmd_exit(sec);
    TP_INFO("---");
    return;

NG:
    snprintf(buff, sizeof(buff), "NG");
    sec->cmd_state = SEC_CMD_STATUS_FAIL;
    sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
    sec_cmd_set_cmd_exit(sec);
}
/*M55 code for SR-QN6887A-01-370|431 by yuli at 20231012 end*/

/*M55 code for P231221-04897 by yuli at 20240106 start*/
static void set_aod_rect(void *device_data)
{
    struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
    char buff[SEC_CMD_STR_LEN] = { 0 };
    int i = 0, ret = 0;

    TP_INFO("+++");
    sec_cmd_set_default_result(sec);
    if (!gs_common_lcm_info) {
        TP_INFO("%s %s: gs_common_lcm_info is NULL");
        goto NG;
    }
    TP_INFO("w:%d, h:%d, x:%d, y:%d", sec->cmd_param[0], sec->cmd_param[1],
            sec->cmd_param[2], sec->cmd_param[3]);

    if (!gs_common_lcm_info->ts_data_info) {
        TP_INFO("%s %s: ts_data_info is NULL", SECLOG, __func__);
        goto NG;
    }

    for (i = 0; i < 4; i++){
        gs_common_lcm_info->ts_data_info->aod_rect_data[i] = sec->cmd_param[i];
    }

    if (gs_common_lcm_info->set_aod_rect != NULL) {
        ret = gs_common_lcm_info->set_aod_rect(gs_common_lcm_info->ts_data_info->aod_rect_data);
        if (ret) {
            goto NG;
        }
        goto OK;
    } else {
        TP_INFO("aod_rect_data is NULL");
        goto NG;
    }

OK:
    snprintf(buff, sizeof(buff), "OK");
    sec->cmd_state = SEC_CMD_STATUS_OK;
    sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
    sec_cmd_set_cmd_exit(sec);
    TP_INFO("---");
    return;

NG:
    snprintf(buff, sizeof(buff), "NG");
    sec->cmd_state = SEC_CMD_STATUS_FAIL;
    sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
    sec_cmd_set_cmd_exit(sec);
}
/*M55 code for P231221-04897 by yuli at 20240106 end*/

/*M55 code for P240111-04531 by yuli at 20240116 start*/
static void spay_enable(void *device_data)
{
   struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
    char buff[SEC_CMD_STR_LEN] = { 0 };

    TP_INFO("+++");
    sec_cmd_set_default_result(sec);

    if (!gs_common_lcm_info) {
        TP_INFO("%s %s: gs_common_lcm_info is NULL");
        snprintf(buff, sizeof(buff), "%s", "NG");
        sec->cmd_state = SEC_CMD_STATUS_FAIL;
        goto END;
    }

    switch (sec->cmd_param[0]) {
        case 0:
            if (gs_common_lcm_info->spay_disable != NULL) {
                gs_common_lcm_info->spay_disable();
                snprintf(buff, sizeof(buff), "%s", "OK");
                sec->cmd_state = SEC_CMD_STATUS_OK;
            } else {
                TP_INFO("spay_disable is NULL");
                snprintf(buff, sizeof(buff), "%s", "NG");
                sec->cmd_state = SEC_CMD_STATUS_FAIL;
            }
            break;
        case 1:
            if (gs_common_lcm_info->spay_enable != NULL) {
                gs_common_lcm_info->spay_enable();
                snprintf(buff, sizeof(buff), "%s", "OK");
                sec->cmd_state = SEC_CMD_STATUS_OK;
            } else {
                TP_INFO("spay_enable is NULL");
                snprintf(buff, sizeof(buff), "%s", "NG");
                sec->cmd_state = SEC_CMD_STATUS_FAIL;
            }
            break;
        default:
            snprintf(buff, sizeof(buff), "%s", "NG");
            sec->cmd_state = SEC_CMD_STATUS_FAIL;
            break;
    }

END:
    sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
    sec->cmd_state = SEC_CMD_STATUS_WAITING;
    sec_cmd_set_cmd_exit(sec);
    TP_INFO("%s, ---", buff);
}
/*M55 code for P240111-04531 by yuli at 20240116 end*/

int sec_fn_init(struct sec_cmd_data *sec)
{
    int ret = 0;

    if (sec == NULL) {
        return -EFAULT;
    }

    ret = sec_cmd_init(sec, sec_cmds, ARRAY_SIZE(sec_cmds), SEC_CLASS_DEVT_TSP);
    if (ret !=0) {
        TP_ERROR("failed to sec cmd init");
        sec_cmd_exit(sec, SEC_CLASS_DEVT_TSP);
        return ret;
    }

    TP_INFO("OK\n");
    return 0;

}

static ssize_t ito_test_show(struct device *dev,struct device_attribute *attr,char *buf)
{
    int count = 0;
    int ret = 0;

    TP_INFO("Run MP test with LCM on");
    if (gs_common_lcm_info->ito_test != NULL) {
        ret = gs_common_lcm_info->ito_test ();
        if(ret != 0) {
            TP_ERROR("ITO test failed!");
        } else {
            TP_INFO("ITO test success!");
        }
    } else {
        TP_ERROR("gs_common_lcm_info->ito_test is NULL, ITO test failed!");
        ret = 1;
    }
    count = sprintf(buf, "%s\n", (ret == 0) ? "1" : "0");//return test  result
    return count;
}

static ssize_t ito_test_store(struct device *dev,struct device_attribute *attr,const char *buf, size_t count)
{
    return count;
}

static DEVICE_ATTR(factory_check, 0644, ito_test_show, ito_test_store);
static struct attribute *factory_sysfs_attrs[] = {
    &dev_attr_factory_check.attr,
    NULL,
};

static const struct attribute_group factory_group = {
    .attrs = factory_sysfs_attrs,
};

int ito_test_node_init(struct platform_device *tp_device)
{
    int ret = 0;
    ret = platform_device_register(tp_device);
    if (ret != 0) {
        platform_device_unregister(tp_device);
        return ret;
    }
    ret = sysfs_create_group(&tp_device->dev.kobj, &factory_group);
    if (ret != 0) {
        sysfs_remove_group(&tp_device->dev.kobj, &factory_group);
        return ret;
    }
    return 0;
}

static ssize_t tp_info_read(struct file *file, char *buff,
                                size_t len, loff_t *pos)
{
    char buf[TPINFO_STR_LEN] = {0};
    int rc = 0;

    if ((gs_common_lcm_info->module_name != NULL) && (gs_common_lcm_info->fw_version != NULL)) {
        snprintf(buf, sizeof(buf), "Module_name: %s\nTouch_FW_ver:V%02x\n",
            gs_common_lcm_info->module_name, *(gs_common_lcm_info->fw_version) & 0xFF);
     } else {
        TP_ERROR("module_name or fw_version is NULL\n");
        return rc;
    }

    rc = simple_read_from_buffer(buff, len, pos, buf, strlen(buf));
    return rc;
}

static const struct proc_ops proc_tp_info_ops = {
    .proc_read = tp_info_read,
};

static int proc_tpinfo_init(void)
{
    proc_tpinfo_file = proc_create(PROC_TPINFO_FILE, 0666, NULL, &proc_tp_info_ops);
    if (proc_tpinfo_file == NULL) {
        TP_ERROR("proc tp_info file create failed!\n");
        return -EPERM;
    }
    return 0;
}

bool get_tp_enable(void)
{
    return g_tp_is_enabled;
}
EXPORT_SYMBOL(get_tp_enable);

void tp_common_init(struct lcm_info *g_lcm_info)
{
    int ret = 0;
    gs_common_lcm_info = g_lcm_info;

    if (gs_common_lcm_info == NULL) {
        TP_ERROR("gs_common_lcm_info is NULL\n");
    } else {
        g_tp_is_enabled = true;

        ret = proc_tpinfo_init();
        if (ret) {
            TP_ERROR("proc_tpinfo_init failed\n");
        }

        ret = ito_test_node_init(&hwinfo_device);
        if (ret) {
            TP_ERROR("ito_test_node_init failed\n");
        }

        ret = sec_fn_init(&gs_common_lcm_info->sec);
        if (ret) {
            TP_ERROR("sec_fn_init failed\n");
        }

        #if defined(CONFIG_CUSTOM_D85_BUILD)
        if (gs_common_lcm_info->gesture_wakeup_enable != NULL) {
            gs_common_lcm_info->gesture_wakeup_enable();
            TP_INFO("Double click gesture default on in D85 version\n");
        }
        #endif
    }
}
EXPORT_SYMBOL(tp_common_init);