/**
*Name：<focaltech_sec.c>
*Author：<gaozhengwei@huaqin.com>
*Date：<2021.11.22>
*Purpose：<Create sysfs cmd, cmd_status, cmd_result>
*/

#include "focaltech_core.h"

static void get_fw_ver_bin(void *device_data);
static void get_fw_ver_ic(void *device_data);
static void aot_enable(void *device_data);
static void not_support_cmd(void *device_data);

struct sec_cmd fts_commands[] = {
    {SEC_CMD("get_fw_ver_bin", get_fw_ver_bin),},
    {SEC_CMD("get_fw_ver_ic", get_fw_ver_ic),},
    {SEC_CMD("aot_enable", aot_enable),},
    {SEC_CMD("not_support_cmd", not_support_cmd),},
};

int fts_get_array_size(void){
    return ARRAY_SIZE(fts_commands);
}

static void get_fw_ver_bin(void *device_data)
{
    u8 fwver = 0;
    int ret = 0;
    struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
    struct fts_ts_data *info = container_of(sec, struct fts_ts_data, sec);
    char buff[20] = { 0 };

    sec_cmd_set_default_result(sec);
    ret = fts_read_reg(FTS_REG_FW_VER, &fwver);
    if (ret < 0) {
        FTS_ERROR("read FW from HOST error.");
        fwver = info->fw_in_binary;
    }
    info->fw_in_binary = fwver;

    snprintf(buff, sizeof(buff), "FTS_BIN=0x%08x",fwver);

    sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
    sec->cmd_state = SEC_CMD_STATUS_OK;
    //FTS_INFO(true, &info->spi->dev, "%s: %s\n", __func__, buff);
    FTS_INFO("%s: %s\n",__func__,buff);
}

static void get_fw_ver_ic(void *device_data)
{
    u8 fwver = 0;
    int ret = 0;
    struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
    struct fts_ts_data *info = container_of(sec, struct fts_ts_data, sec);
    char buff[20] = { 0 };

    sec_cmd_set_default_result(sec);
    ret = fts_read_reg(FTS_REG_FW_VER, &fwver);
    if (ret < 0) {
        FTS_ERROR("read FW from IC error.");
        fwver = info->fw_in_ic;
    }
    info->fw_in_ic = fwver;
    snprintf(buff, sizeof(buff), "FTS_IC=0x%08x",fwver);

    sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
    sec->cmd_state = SEC_CMD_STATUS_OK;
    FTS_INFO("%s: %s\n",__func__,buff);
}

static void aot_enable(void *device_data)
{
    struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
    struct fts_ts_data *info = container_of(sec, struct fts_ts_data, sec);
    char buff[SEC_CMD_STR_LEN] = { 0 };

    sec_cmd_set_default_result(sec);

    if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
        snprintf(buff, sizeof(buff), "%s", "NG");
        sec->cmd_state = SEC_CMD_STATUS_FAIL;
    } else {
        if (sec->cmd_param[0] == 0) {
            info->gesture_mode = DISABLE;
            tp_gesture = 0;
        } else {
            info->gesture_mode = ENABLE;
            tp_gesture = 1;
        }
        snprintf(buff, sizeof(buff), "%s", "OK");
        sec->cmd_state = SEC_CMD_STATUS_OK;
    }
    sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
    sec->cmd_state = SEC_CMD_STATUS_WAITING;
    sec_cmd_set_cmd_exit(sec);
    FTS_INFO("%s: %s\n",__func__,buff);
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

    FTS_INFO("%s: %s\n",__func__,buff);
}

static ssize_t read_support_feature(struct device *dev,
    struct device_attribute *attr, char *buf)
{
    u32 feature = 0;

    feature |= INPUT_FEATURE_ENABLE_SETTINGS_AOT;

    FTS_INFO("%s: %d%s\n",
                __func__, feature,
                feature & INPUT_FEATURE_ENABLE_SETTINGS_AOT ? " aot" : "");

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

int fts_ts_sec_fn_init(struct fts_ts_data *ts)
{
    int ret;

    ret = sec_cmd_init(&ts->sec, fts_commands, ARRAY_SIZE(fts_commands), SEC_CLASS_DEVT_TSP);
    if (ret < 0) {
        FTS_ERROR("%s: failed to sec cmd init\n",
            __func__);
            return ret;
    }

    ret = sysfs_create_group(&ts->sec.fac_dev->kobj, &cmd_attr_group);
    if (ret) {
        FTS_ERROR("%s: failed to create sysfs attributes\n",
            __func__);
            goto out;
    }

    /* Tab A8 code for AX6300DEV-3297 by yuli at 2021/12/6 start */
    ret = sysfs_create_link(&ts->sec.fac_dev->kobj, &ts->input_dev->dev.kobj, "input");
    if (ret) {
        FTS_ERROR("%s: failed to creat sysfs link\n", __func__);
        sysfs_remove_group(&ts->sec.fac_dev->kobj, &cmd_attr_group);
        goto out;
    }
    /* Tab A8 code for AX6300DEV-3297 by yuli at 2021/12/6 end */

    FTS_INFO("%s\n", __func__);

    return 0;

out:
    sec_cmd_exit(&ts->sec, SEC_CLASS_DEVT_TSP);

    return ret;
}

void fts_ts_sec_fn_remove(struct fts_ts_data *ts)
{
    /* Tab A8 code for AX6300DEV-3297 by yuli at 2021/12/6 start */
    sysfs_delete_link(&ts->sec.fac_dev->kobj, &ts->input_dev->dev.kobj, "input");
    /* Tab A8 code for AX6300DEV-3297 by yuli at 2021/12/6 end */

    sysfs_remove_group(&ts->sec.fac_dev->kobj, &cmd_attr_group);

    sec_cmd_exit(&ts->sec, SEC_CLASS_DEVT_TSP);
}