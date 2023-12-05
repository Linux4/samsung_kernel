#include "jadard_common.h"

extern struct jadard_ic_data *pjadard_ic_data;
extern int smart_wakeup_open_flag;
extern void jadard_enable_gesture_wakeup(void);
extern void jadard_disable_gesture_wakeup(void);
extern void jadard_enable_high_sensitivity(void);
extern void jadard_disable_high_sensitivity(void);
static void get_fw_ver_bin(void *device_data);
static void get_fw_ver_ic(void *device_data);
static void aot_enable(void *device_data);
static void high_sensitivity_mode(void *device_data);
static void not_support_cmd(void *device_data);

struct sec_cmd jadard_commands[] = {
	{SEC_CMD("get_fw_ver_bin", get_fw_ver_bin),},
	{SEC_CMD("get_fw_ver_ic", get_fw_ver_ic),},
	{SEC_CMD("aot_enable", aot_enable),},
	{SEC_CMD("glove_mode", high_sensitivity_mode),},
	{SEC_CMD("not_support_cmd", not_support_cmd),},
};

int jadard_get_array_size(void)
{
	return ARRAY_SIZE(jadard_commands);
}

static void get_fw_ver_bin(void *device_data)
{
	char buff[20] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
//	struct chipone_ts_data *info = container_of(sec, struct chipone_ts_data, sec);

	sec_cmd_set_default_result(sec);
	/*hs04 code for AL6398ADEU-29 by tangsumian at 20221212 start*/
	snprintf(buff, sizeof(buff), "JDTP_BIN=0x%02x",pjadard_ic_data->fw_cid_ver&0xff);
	/*hs04 code for AL6398ADEU-29 by tangsumian at 20221212 end*/
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	JD_I("%s: %s\n", __func__, buff);
}

static void get_fw_ver_ic(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
//	struct chipone_ts_data *info = container_of(sec, struct chipone_ts_data, sec);
	char buff[20] = { 0 };

	sec_cmd_set_default_result(sec);
	/*hs04 code for AL6398ADEU-29 by tangsumian at 20221212 start*/
	snprintf(buff, sizeof(buff), "JDTP_IC=0x%02x",pjadard_ic_data->fw_cid_ver&0xff);
	/*hs04 code for AL6398ADEU-29 by tangsumian at 20221212 end*/
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	JD_I("%s: %s\n", __func__, buff);
}

static void aot_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
//	struct chipone_ts_data *info = container_of(sec, struct chipone_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		if (sec->cmd_param[0] == 0) {
			jadard_disable_gesture_wakeup();
            smart_wakeup_open_flag = 0;
        } else {
			jadard_enable_gesture_wakeup();
            smart_wakeup_open_flag = 1;
        }
        snprintf(buff, sizeof(buff), "%s", "OK");
        sec->cmd_state = SEC_CMD_STATUS_OK;
    }
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);

	JD_I("%s: %s\n", __func__, buff);
}

static void high_sensitivity_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
//	struct chipone_ts_data *info = container_of(sec, struct chipone_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		if (sec->cmd_param[0] == 0) {
			jadard_disable_high_sensitivity();
		} else {
			jadard_enable_high_sensitivity();
		}
		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);

	JD_I("%s: %s\n", __func__, buff);
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

	JD_I("%s: %s\n", __func__, buff);
}

