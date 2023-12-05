#include "gcore_drv_common.h"
#include <linux/input.h>
#include <linux/input/sec_cmd.h>

static void get_fw_ver_bin(void *device_data);
static void get_fw_ver_ic(void *device_data);
static void aot_enable(void *device_data);
static void high_sensitivity_mode(void *device_data);
static void not_support_cmd(void *device_data);

extern void enable_gesture_wakeup(void);
extern int gcore_sec_fn_init(struct gcore_dev *ts);
extern int smart_wakeup_open_flag;
/*hs04 code for AL6398ADEU-29 by tangsumian at 20221212 start*/
extern struct gcore_dev *gdev_fwu;
/*hs04 code for AL6398ADEU-29 by tangsumian at 20221212 end*/
struct sec_cmd gcore_sec_cmds[] = {
    {SEC_CMD("get_fw_ver_bin", get_fw_ver_bin),},
    {SEC_CMD("get_fw_ver_ic", get_fw_ver_ic),},
    {SEC_CMD("aot_enable", aot_enable),},
    {SEC_CMD("glove_mode", high_sensitivity_mode),},
    {SEC_CMD("not_support_cmd", not_support_cmd),},
};

int gcore_get_array_size(void)
{
    return ARRAY_SIZE(gcore_sec_cmds);
}

static void get_fw_ver_bin(void *device_data)
{
    char buff[SEC_CMD_STR_LEN] = { 0 };
    struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
    sec_cmd_set_default_result(sec);
    /*hs04 code for AL6398ADEU-29 by tangsumian at 20221212 start*/
    snprintf(buff, sizeof(buff), "GCORETP_BIN: %02x\n",gdev_fwu->fw_ver[2]&0xff);
    /*hs04 code for AL6398ADEU-29 by tangsumian at 20221212 end*/
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
    /*hs04 code for AL6398ADEU-29 by tangsumian at 20221212 start*/
    snprintf(buff, sizeof(buff), "GCORETP_IC=%02x\n",gdev_fwu->fw_ver[2]&0xff);
    /*hs04 code for AL6398ADEU-29 by tangsumian at 20221212 end*/
    sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
    sec->cmd_state = SEC_CMD_STATUS_OK;
    sec_cmd_set_cmd_exit(sec);
    GTP_DEBUG("[SEC]: %s\n", buff);
}

void gcore_enable_gesture_wakeup(void)
{
    GTP_DEBUG("Enable gesture wakeup");
    fn_data.gdev->gesture_wakeup_en = true;
}

void gcore_disable_gesture_wakeup(void)
{
    GTP_DEBUG("Disable gesture wakeup");
    fn_data.gdev->gesture_wakeup_en = false;
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
            gcore_disable_gesture_wakeup();
            smart_wakeup_open_flag = 0;
#endif
        } else {
#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
            gcore_enable_gesture_wakeup();
            smart_wakeup_open_flag = 1;
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
    /*hs03s_NM code for SR-AL5625-01-627 by zhawei at 20220419 start*/
    gcore_fw_event_notify(FW_REPORT_NORMAL_SENSITIVITY);
    /*hs03s_NM code for SR-AL5625-01-627 by zhawei at 20220419 end*/
}

void enable_high_sensitivity(void)
{
    GTP_DEBUG("Enable high sensitivity");
    /*hs03s_NM code for SR-AL5625-01-627 by zhawei at 20220419 start*/
    gcore_fw_event_notify(FW_REPORT_HIGH_SENSITIVITY);
    /*hs03s_NM code for SR-AL5625-01-627 by zhawei at 20220419 end*/
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

int gcore_sec_fn_init(struct gcore_dev *ts)
{
    int ret;

    ret = sec_cmd_init(&ts->sec, gcore_sec_cmds, gcore_get_array_size(), SEC_CLASS_DEVT_TSP);
    if (ret < 0) {
        GTP_ERROR("failed to sec cmd init\n");
        return ret;
    }else{
        GTP_DEBUG("sec cmd init success\n");
    }

    GTP_DEBUG("OK\n");
    return 0;
}