#include "gt1x_generic.h"

static void get_fw_ver_bin(void *device_data);
/* HS60 add for SR-ZQL1695-01-64 by liufurong at 20190731 start */
static void get_fw_ver_ic(void *device_data);
/* HS60 add for SR-ZQL1695-01-64 by liufurong at 20190731 end */
static void glove_mode(void *device_data);
static void aot_enable(void *device_data);
static void not_support_cmd(void *device_data);
/* HS60 add for HS60-212 by zhouzichun at 20190830 start */
int gt1x_glove_update_state(void);
/* HS60 add for HS60-212 by zhouzichun at 20190830 end */

struct sec_cmd gtp_commands[] = {
	{SEC_CMD("get_fw_ver_bin", get_fw_ver_bin),},
	/* HS60 add for SR-ZQL1695-01-64 by liufurong at 20190731 start */
	{SEC_CMD("get_fw_ver_ic", get_fw_ver_ic),},
	/* HS60 add for SR-ZQL1695-01-64 by liufurong at 20190731 end */
	{SEC_CMD("glove_mode", glove_mode),},
	{SEC_CMD("aot_enable", aot_enable),},
	{SEC_CMD("not_support_cmd", not_support_cmd),},
};

int gtp_get_array_size(void){
	return ARRAY_SIZE(gtp_commands);
}

/* HS60 add for SR-ZQL1695-01-64 by liufurong at 20190731 start */
static void get_fw_ver_bin(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct sec_info *info = container_of(sec, struct sec_info, sec);
	char buff[40] = { 0 };

	sec_cmd_set_default_result(sec);
	snprintf(buff, sizeof(buff), "GTP_BIN=%06x%02x",
			info->fw_version_bin,
			info->cfg_version);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &gt1x_i2c_client->dev, "%s: %s\n", __func__, buff);
	return;
}

static void get_fw_ver_ic(void *device_data)
{
	int ret = 0;
	u8 cfg_ver_info = 0;
	struct gt1x_version_info fw_ver_info;

	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct sec_info *info = container_of(sec, struct sec_info, sec);
	char buff[40] = { 0 };

	sec_cmd_set_default_result(sec);

	ret = gt1x_read_version(&fw_ver_info);
	if (ret < 0) {
		GTP_ERROR("read version failed!");
		goto err;
	}
	info->fw_version = fw_ver_info.patch_id;
	ret = gt1x_i2c_read(GTP_REG_CONFIG_DATA, &cfg_ver_info, 1);
	if (ret < 0) {
		GTP_ERROR("read data failed!");
		goto err;
	}
	info->cfg_version = cfg_ver_info;
err:
	snprintf(buff, sizeof(buff), "GTP_IC=0x%06x%02x",
			info->fw_version,
			info->cfg_version);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &gt1x_i2c_client->dev, "%s: %s\n", __func__, buff);
	return;
}


/* HS60 add for SR-ZQL1695-01-64 by liufurong at 20190731 end */

static void glove_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	//struct sec_info *info = container_of(sec, struct sec_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	sec_cmd_set_default_result(sec);
	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		/* HS60 add for HS60-212 by zhouzichun at 20190830 start */
		int ret=-1;
		if (sec->cmd_param[0] == 0) {
			GTP_INFO("exit glove mode");
			gt1x_glove_dev->state = 0;
			ret = gt1x_glove_update_state();
		} else {
			GTP_INFO("enter glove mode");
			gt1x_glove_dev->state = 1;
			ret = gt1x_glove_update_state();
		}
		if (ret < 0) {
			snprintf(buff, sizeof(buff), "%s", "NG");
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
		} else {
			snprintf(buff, sizeof(buff), "%s", "OK");
			sec->cmd_state = SEC_CMD_STATUS_OK;
		}
		/* HS60 add for HS60-212 by zhouzichun at 20190830 end */
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);
	input_info(true, &gt1x_i2c_client->dev, "%s: %s\n", __func__, buff);
}

static void aot_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	//struct sec_info *info = container_of(sec, struct sec_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		/* HS60 add for HS60-212 by zhouzichun at 20190902 start */
		if (sec->cmd_param[0] == 0) {
			gesture_enabled  = 0;
			GTP_INFO("gesture Double-click wake-up enabled:%d", gesture_enabled);
		} else {
			gesture_enabled  = 1;
			GTP_INFO("gesture Double-click wake-up disabled:%d", gesture_enabled);
		}
		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
		/* HS60 add for HS60-212 by zhouzichun at 20190902 end */
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &gt1x_i2c_client->dev, "%s: %s\n", __func__, buff);
}

static void not_support_cmd(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	//struct sec_info *info = container_of(sec, struct sec_info, sec);
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);
	snprintf(buff, sizeof(buff), "%s", "NA");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &gt1x_i2c_client->dev, "%s: %s\n", __func__, buff);
}
