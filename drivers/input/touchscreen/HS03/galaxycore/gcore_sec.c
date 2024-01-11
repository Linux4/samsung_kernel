#include "gcore_drv_common.h"
#include <linux/input.h>
#include <linux/input/sec_cmd.h>
/*HS03 code for SR-SL6215-01-589 by duanyaoming at 20220304 start*/
static void get_fw_ver_bin(void *device_data);
static void get_fw_ver_ic(void *device_data);
static void aot_enable(void *device_data);
static void high_sensitivity_mode(void *device_data);
static void not_support_cmd(void *device_data);

extern void enable_gesture_wakeup(void);
extern int gcore_sec_fn_init(struct gcore_dev *ts);
extern int tp_gesture;

struct sec_cmd sec_cmds[] = {
    {SEC_CMD("get_fw_ver_bin", get_fw_ver_bin),},
    {SEC_CMD("get_fw_ver_ic", get_fw_ver_ic),},
    {SEC_CMD("aot_enable", aot_enable),},
    {SEC_CMD("glove_mode", high_sensitivity_mode),},
    {SEC_CMD("not_support_cmd", not_support_cmd),},
};

static void get_fw_ver_bin(void *device_data)
{
    char buff[SEC_CMD_STR_LEN] = { 0 };
    struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
    sec_cmd_set_default_result(sec);
    snprintf(buff, sizeof(buff), "GCORETP_BIN: %08x\n",fn_data.gdev->fw_ver);
    sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
    sec->cmd_state = SEC_CMD_STATUS_OK;
    sec_cmd_set_cmd_exit(sec);
    GTP_DEBUG("[SEC]: %s\n", buff);
}

static void get_fw_ver_ic(void *device_data)
{
    struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
    char buff[20] = { 0 };

    sec_cmd_set_default_result(sec);
    snprintf(buff, sizeof(buff), "GCORETP_IC=%08x\n",fn_data.gdev->fw_ver);
    sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
    sec->cmd_state = SEC_CMD_STATUS_OK;
    sec_cmd_set_cmd_exit(sec);
    GTP_DEBUG("[SEC]: %s\n", buff);
}

void enable_gesture_wakeup(void)
{
    GTP_DEBUG("Enable gesture wakeup");
    fn_data.gdev->gesture_wakeup_en = true;
    if(fn_data.gdev->tp_suspend == false){
        tp_gesture = true;
    }else{
        GTP_ERROR("Gesture wakeup fail,please brighten the screen\n");
    }
}

void disable_gesture_wakeup(void)
{
    GTP_DEBUG("Disable gesture wakeup");
    fn_data.gdev->gesture_wakeup_en = false;
    tp_gesture = false;
}

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
#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
            disable_gesture_wakeup();
#endif
        } else {
#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
            enable_gesture_wakeup();
#endif
        }
        snprintf(buff, sizeof(buff), "%s", "OK");
        sec->cmd_state = SEC_CMD_STATUS_OK;
    }
    sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
    sec->cmd_state = SEC_CMD_STATUS_WAITING;
    sec_cmd_set_cmd_exit(sec);
    GTP_DEBUG("[SEC]: %s\n", buff);
}

void disable_high_sensitivity(void)
{
    GTP_DEBUG("Disable high sensitivity");
    fn_data.gdev->glove_mode_enable= false;
    /*HS03 code for SR-SL6215-01-589 by duanyaoming at 20220317 start*/
    gcore_fw_event_notify(FW_REPORT_NORMAL_SENSITIVITY);
    /*HS03 code for SR-SL6215-01-589 by duanyaoming at 20220317 end*/
}

void enable_high_sensitivity(void)
{
    GTP_DEBUG("Enable high sensitivity");
    fn_data.gdev->glove_mode_enable= true;
    /*HS03 code for SR-SL6215-01-589 by duanyaoming at 20220317 start*/
    gcore_fw_event_notify(FW_REPORT_HIGH_SENSITIVITY);
    /*HS03 code for SR-SL6215-01-589 by duanyaoming at 20220317 end*/
}

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

    GTP_DEBUG("[SEC]: %s\n",buff);
}

static void not_support_cmd(void *device_data)
{
    struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
    char buff[16] = { 0 };

    sec_cmd_set_default_result(sec);
    snprintf(buff, sizeof(buff), "%s", "NA");
    sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
    sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
    sec_cmd_set_cmd_exit(sec);

    GTP_DEBUG("[SEC]: %s\n", buff);
}

static ssize_t read_support_feature(struct device *dev,
    struct device_attribute *attr, char *buf)
{
    u32 feature = 0;

    feature |= INPUT_FEATURE_ENABLE_SETTINGS_AOT;
    GTP_DEBUG("aot = %d", feature & INPUT_FEATURE_ENABLE_SETTINGS_AOT);

    return snprintf(buf, SEC_CMD_BUF_SIZE, "%d", feature);
}

static DEVICE_ATTR(support_feature, 0444, read_support_feature, NULL);

static struct attribute *cmd_attributes[] = {
    &dev_attr_support_feature.attr,
    NULL,
};

static struct attribute_group cmd_attr_group = {
    .attrs = cmd_attributes,
};

int gcore_sec_fn_init(struct gcore_dev *ts)
{
    int ret;

    ret = sec_cmd_init(&ts->sec, sec_cmds, ARRAY_SIZE(sec_cmds), SEC_CLASS_DEVT_TSP);
    if (ret < 0) {
        GTP_ERROR("failed to sec cmd init\n");
        return ret;
    }else{
        GTP_DEBUG("sec cmd init success\n");
    }

    ret = sysfs_create_group(&ts->sec.fac_dev->kobj, &cmd_attr_group);
    if (ret) {
        sysfs_remove_group(&ts->sec.fac_dev->kobj, &cmd_attr_group);
        GTP_ERROR("failed to create sysfs attributes\n");
        goto out;
    }

    ret = sysfs_create_link(&ts->sec.fac_dev->kobj, &ts->input_device->dev.kobj, "input");
    if (ret) {
        sysfs_delete_link(&ts->sec.fac_dev->kobj, &ts->input_device->dev.kobj, "input");
        GTP_ERROR("failed to creat sysfs link\n");
        goto out;
    }
    GTP_DEBUG("OK\n");
    return 0;

out:
    sec_cmd_exit(&ts->sec, SEC_CLASS_DEVT_TSP);
    return ret;
}
/*HS03 code for SR-SL6215-01-589 by duanyaoming at 20220304 end*/