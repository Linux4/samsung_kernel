#include "focaltech_core.h"

static void get_fw_ver_bin(void *device_data);
static void get_fw_ver_ic(void *device_data);
static void aot_enable(void *device_data);
static void not_support_cmd(void *device_data);

extern int smart_wakeup_open_flag;

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
	if ( ret < 0 ) {
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
	if ( ret < 0 ) {
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
			smart_wakeup_open_flag = 0;
		} else {
			info->gesture_mode = ENABLE;
			smart_wakeup_open_flag = 1;
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
