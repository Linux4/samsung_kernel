#include "himax_platform.h"
#include "himax_common.h"
#include "himax_ic_core.h"
static void get_fw_ver_bin(void *device_data);
static void get_fw_ver_ic(void *device_data);
static void aot_enable(void *device_data);
static void high_sensitivity_mode(void *device_data);
static void not_support_cmd(void *device_data);

extern int smart_wakeup_open_flag;

struct sec_cmd himax_commands[] = {
	{SEC_CMD("get_fw_ver_bin", get_fw_ver_bin),},
	{SEC_CMD("get_fw_ver_ic", get_fw_ver_ic),},
	{SEC_CMD("aot_enable", aot_enable),},
	{SEC_CMD("glove_mode", high_sensitivity_mode),},
	{SEC_CMD("not_support_cmd", not_support_cmd),},
};

int himax_get_array_size(void)
{
	return ARRAY_SIZE(himax_commands);
}

static void get_fw_ver_bin(void *device_data)
{
	char buff[20] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	//struct himax_ts_data *info = container_of(sec, struct himax_ts_data, sec);

	sec_cmd_set_default_result(sec);

	snprintf(buff, sizeof(buff), "HX_BIN=0x%x",ic_data->vendor_fw_ver);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	I("%s: %s\n", __func__, buff);
}

static void get_fw_ver_ic(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	//struct himax_ts_data *info = container_of(sec, struct himax_ts_data, sec);
	char buff[20] = { 0 };

	sec_cmd_set_default_result(sec);

	snprintf(buff, sizeof(buff), "HX_IC=0x%x",ic_data->vendor_fw_ver);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	I("%s: %s\n", __func__, buff);
}

static void aot_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct himax_ts_data *info = container_of(sec, struct himax_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		if (sec->cmd_param[0] == 0) {
			info->SMWP_enable = 0;
			info->gesture_cust_en[0] = 0;
		} else {
			info->SMWP_enable = 1;
			info->gesture_cust_en[0] = 1;
		}
#if defined(HX_SMART_WAKEUP)
		if(info->SMWP_enable == 1){
			smart_wakeup_open_flag = 1;
		}else if (info->SMWP_enable == 0){
			smart_wakeup_open_flag = 0;
		}
		g_core_fp.fp_set_SMWP_enable(info->SMWP_enable, private_ts->suspended);
#endif
		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);

	I("%s: %s\n", __func__, buff);
}

static void high_sensitivity_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	//struct himax_ts_data *info = container_of(sec, struct himax_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		if (sec->cmd_param[0] == 0) {
	//		info->HSEN_enable = 0;
		} else {
	//		info->HSEN_enable = 1;
		}
	//	g_core_fp.fp_set_HSEN_enable(info->HSEN_enable, info->suspended);
		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);

	I("%s: %s\n", __func__, buff);
}

static void not_support_cmd(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	//struct himax_ts_data *info = container_of(sec, struct himax_ts_data, sec);
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);
	snprintf(buff, sizeof(buff), "%s", "NA");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
	sec_cmd_set_cmd_exit(sec);

	I("%s: %s\n", __func__, buff);
}
