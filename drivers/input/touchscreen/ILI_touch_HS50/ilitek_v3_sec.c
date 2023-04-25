#include "ilitek_v3.h"

#define CHAR_MASK ((1 << 8) - 1)

static void get_fw_ver_bin(void *device_data);
static void get_fw_ver_ic(void *device_data);
static void aot_enable(void *device_data);
static void glove_mode(void *device_data);
static void not_support_cmd(void *device_data);

struct sec_cmd ili_commands[] = {
	{SEC_CMD("get_fw_ver_bin", get_fw_ver_bin),},
	{SEC_CMD("get_fw_ver_ic", get_fw_ver_ic),},
	{SEC_CMD("aot_enable", aot_enable),},
	{SEC_CMD("glove_mode", glove_mode),},
	{SEC_CMD("not_support_cmd", not_support_cmd),},
};

int ili_get_array_size(void){
	return ARRAY_SIZE(ili_commands);
}

static void get_fw_ver_bin(void *device_data)
{
    struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	int temp = 0;
	char buf[30] = {0};

    sec_cmd_set_default_result(sec);
	mutex_lock(&ilits->touch_mutex);
	ili_ic_get_fw_ver();
	mutex_unlock(&ilits->touch_mutex);

	temp = ilits->chip->fw_ver;
	snprintf(buf, sizeof(buf), "ILI_IC=%d.%d.%d.%d\n",(temp >> 24) & CHAR_MASK,
                                            (temp >> 16) & CHAR_MASK,
                                            (temp >> 8) & CHAR_MASK,
                                            temp & CHAR_MASK);
    sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
    sec->cmd_state = SEC_CMD_STATUS_OK;
    ILI_INFO("sec_resault:%s\n", buf);
}

static void get_fw_ver_ic(void *device_data)
{
    struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	int temp = 0;
	char buf[30] = {0};

    sec_cmd_set_default_result(sec);
	mutex_lock(&ilits->touch_mutex);
	ili_ic_get_fw_ver();
	mutex_unlock(&ilits->touch_mutex);

	temp = ilits->chip->fw_ver;
	snprintf(buf, sizeof(buf), "ILI_IC=%d.%d.%d.%d\n",(temp >> 24) & CHAR_MASK,
                                            (temp >> 16) & CHAR_MASK,
                                            (temp >> 8) & CHAR_MASK,
                                            temp & CHAR_MASK);
    sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
    sec->cmd_state = SEC_CMD_STATUS_OK;
    ILI_INFO("sec_resault:%s\n", buf);
}


static void aot_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;

	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

    mutex_lock(&ilits->touch_mutex);
	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		if (sec->cmd_param[0] == 0) {
			ilits->gesture = DISABLE;
		} else {
			ilits->gesture = ENABLE;
		}
		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
    mutex_unlock(&ilits->touch_mutex);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);

	ILI_INFO("sec_resault:%s\n", buff);
}

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
            ilits->glove = 0;
			ret = ili_ic_func_ctrl("glove", ilits->glove);
			mutex_unlock(&ilits->touch_mutex);
		} else {
			ILI_INFO("enter glove mode");
			mutex_lock(&ilits->touch_mutex);
            ilits->glove = 1;
			ret = ili_ic_func_ctrl("glove", ilits->glove);
			mutex_unlock(&ilits->touch_mutex);

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
