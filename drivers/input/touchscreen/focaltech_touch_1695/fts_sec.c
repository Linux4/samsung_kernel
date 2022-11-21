
#include "focaltech_core.h"

static void get_fw_ver_bin(void *device_data);
/* HS60 add for SR-ZQL1695-01-64 by liufurong at 20190731 start */
static void get_fw_ver_ic(void *device_data);
/* HS60 add for SR-ZQL1695-01-64 by liufurong at 20190731 end */
static void glove_mode(void *device_data);
static void aot_enable(void *device_data);
static void not_support_cmd(void *device_data);

struct sec_cmd fts_commands[] = {
	{SEC_CMD("get_fw_ver_bin", get_fw_ver_bin),},
/* HS60 add for SR-ZQL1695-01-64 by liufurong at 20190731 start */
	{SEC_CMD("get_fw_ver_ic", get_fw_ver_ic),},
/* HS60 add for SR-ZQL1695-01-64 by liufurong at 20190731 end */
	{SEC_CMD("glove_mode", glove_mode),},
	{SEC_CMD("aot_enable", aot_enable),},
	{SEC_CMD("not_support_cmd", not_support_cmd),},
};

int fts_get_array_size(void){
	return ARRAY_SIZE(fts_commands);
}

/* HS60 add for SR-ZQL1695-01-64 by liufurong at 20190731 start */
static void get_fw_ver_bin(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *info = container_of(sec, struct fts_ts_data, sec);
	char buff[20] = { 0 };

	sec_cmd_set_default_result(sec);

	snprintf(buff, sizeof(buff), "FTS_BIN=0x%08x",info->fw_in_binary);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
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
	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}
/* HS60 add for SR-ZQL1695-01-64 by liufurong at 20190731 end */

static void glove_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *info = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret = 0;

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		if (sec->cmd_param[0] == 0) {
			if (info->glove_mode) {
				FTS_DEBUG("exit glove mode");
				ret = fts_ex_mode_switch(MODE_GLOVE, DISABLE);
				if (ret >= 0) {
					info->glove_mode = DISABLE;
				}
			}
		} else {
			if (!info->glove_mode) {
				FTS_DEBUG("enter glove mode");
				ret = fts_ex_mode_switch(MODE_GLOVE, ENABLE);
				if (ret >= 0) {
					info->glove_mode = ENABLE;
				}
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

	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
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
			fts_gesture_data.mode = DISABLE;
		} else {
			fts_gesture_data.mode = ENABLE;
		}
		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void not_support_cmd(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *info = container_of(sec, struct fts_ts_data, sec);
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);
	snprintf(buff, sizeof(buff), "%s", "NA");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}
