/* HS70 modify for SR-ZQL1871-01-172 by zhouzichun at 20191020 start */
#include "ilitek_v3.h"

static void get_fw_ver_bin(void *device_data);
static void get_fw_ver_ic(void *device_data);
static void aot_enable(void *device_data);
/* HS70 add for SR-ZQL1871-01-309 by liufurong at 20191119 start */
static void glove_mode(void *device_data);
/* HS70 add for SR-ZQL1871-01-309 by liufurong at 20191119 end */
static void not_support_cmd(void *device_data);

struct sec_cmd ilitek_commands[] = {
	{SEC_CMD("get_fw_ver_bin", get_fw_ver_bin),},
	{SEC_CMD("get_fw_ver_ic", get_fw_ver_ic),},
	{SEC_CMD("aot_enable", aot_enable),},
	/* HS70 add for SR-ZQL1871-01-309 by liufurong at 20191119 start */
	{SEC_CMD("glove_mode", glove_mode),},
	/* HS70 add for SR-ZQL1871-01-309 by liufurong at 20191119 end */
	{SEC_CMD("not_support_cmd", not_support_cmd),},
};

int ilitek_get_array_size(void){
	return ARRAY_SIZE(ilitek_commands);
}

static void get_fw_ver_bin(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	char buff[20] = { 0 };
	u8 ch[10] = {0};
	sec_cmd_set_default_result(sec);
	mutex_lock(&ilits->touch_mutex);
	ili_ic_get_fw_ver();
	mutex_unlock(&ilits->touch_mutex);
	if (ilits->info_from_hex) {
		ch[1] = ilits->fw_info[48];
		ch[2] = ilits->fw_info[49];
		ch[3] = ilits->fw_info[50];
		ch[4] = ilits->fw_info[51];
		snprintf(buff, sizeof(buff), "ILI_BIN=%d.%d.%d.%d",ch[1], ch[2], ch[3], ch[4]);
	}
	else
	{
		snprintf(buff, sizeof(buff), "ILI_BIN=NA");
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	ILI_INFO("sec_resault:%s\n", buff);
}

static void get_fw_ver_ic(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	char buff[20] = { 0 };
	u8 ch[10] = {0};
	sec_cmd_set_default_result(sec);
	mutex_lock(&ilits->touch_mutex);
	ili_ic_get_fw_ver();
	mutex_unlock(&ilits->touch_mutex);
	if (ilits->info_from_hex) {
		ch[1] = ilits->fw_info[48];
		ch[2] = ilits->fw_info[49];
		ch[3] = ilits->fw_info[50];
		ch[4] = ilits->fw_info[51];
		snprintf(buff, sizeof(buff), "ILI_IC=%d.%d.%d.%d",ch[1], ch[2], ch[3], ch[4]);
	}
	else
	{
		snprintf(buff, sizeof(buff), "ILI_IC=NA");
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	ILI_INFO("sec_resault:%s\n", buff);
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
		/* HS70 add for SR-ZQL1871-01-177 by gaozhengwei at 2019/11/02 start */
		if (sec->cmd_param[0] == 0) {
			ilits->gesture = DISABLE;
		} else {
			ilits->gesture = ENABLE;
		}
		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
		/* HS70 add for SR-ZQL1871-01-177 by gaozhengwei at 2019/11/02 end */
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);

	ILI_INFO("sec_resault:%s\n", buff);
}
/* HS70 add for SR-ZQL1871-01-309 by liufurong at 20191119 start */
static void glove_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret = 0;

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		if (sec->cmd_param[0] == 0) {
			ILI_INFO("exit glove mode");
			mutex_lock(&ilits->touch_mutex);
			ret = ili_ic_func_ctrl("glove", 0);
			mutex_unlock(&ilits->touch_mutex);
			if (ret >= 0) {
				ilits->glove = 0;
			}
		} else {
			ILI_INFO("enter glove mode");
			mutex_lock(&ilits->touch_mutex);
			ret = ili_ic_func_ctrl("glove", 1);
			mutex_unlock(&ilits->touch_mutex);
			if (ret >= 0) {
				ilits->glove = 1;
			}
		}
		if (ret < 0) {
			snprintf(buff, sizeof(buff), "%s", "NG");
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
		} else {
			snprintf(buff, sizeof(buff), "%s", "OK");
			sec->cmd_state = SEC_CMD_STATUS_OK;
		}
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);

	ILI_INFO("sec_resault:%s\n", buff);
}
/* HS70 add for SR-ZQL1871-01-309 by liufurong at 20191119 end */
static void not_support_cmd(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	char buff[16] = { 0 };
	sec_cmd_set_default_result(sec);
	snprintf(buff, sizeof(buff), "%s", "NA");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
	sec_cmd_set_cmd_exit(sec);
	ILI_INFO("sec_resault:%s\n", buff);
}
/* HS70 modify for SR-ZQL1871-01-172 by zhouzichun at 20191020 end */
