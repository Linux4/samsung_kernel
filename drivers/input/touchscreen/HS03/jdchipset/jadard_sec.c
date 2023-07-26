#include "jadard_common.h"

#define JD9365T_ID                         (0x9383)

extern struct jadard_ic_data *pjadard_ic_data;
#ifdef JD_SMART_WAKEUP
extern void jadard_enable_gesture_wakeup(void);
extern void jadard_disable_gesture_wakeup(void);
#endif
#ifdef JD_HIGH_SENSITIVITY
extern void jadard_enable_high_sensitivity(void);
extern void jadard_disable_high_sensitivity(void);
#endif
extern const char *lcd_name;
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

	sec_cmd_set_default_result(sec);

    snprintf(buff, sizeof(buff), "JDTP_BIN: %08x\n",pjadard_ic_data->fw_cid_ver);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
    sec_cmd_set_cmd_exit(sec);
	JD_I("[SEC]%s: %s\n", __func__, buff);
}

static void get_fw_ver_ic(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	char buff[20] = { 0 };

	sec_cmd_set_default_result(sec);

	snprintf(buff, sizeof(buff), "JDTP_IC: %08x\n",pjadard_ic_data->fw_cid_ver);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
    sec_cmd_set_cmd_exit(sec);
	JD_I("[SEC]%s: %s\n", __func__, buff);
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
            #ifdef JD_SMART_WAKEUP
			jadard_disable_gesture_wakeup();
            #endif
        } else {
            #ifdef JD_SMART_WAKEUP
			jadard_enable_gesture_wakeup();
            #endif
        }
        snprintf(buff, sizeof(buff), "%s", "OK");
        sec->cmd_state = SEC_CMD_STATUS_OK;
    }
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);

	JD_I("[SEC]%s: %s\n", __func__, buff);
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
            #ifdef JD_HIGH_SENSITIVITY
			jadard_disable_high_sensitivity();
            #endif
		} else {
            #ifdef JD_HIGH_SENSITIVITY
			jadard_enable_high_sensitivity();
            #endif
		}
		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);

	JD_I("[SEC]%s: %s\n", __func__, buff);
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

	JD_E("[SEC]%s: %s\n", __func__, buff);
}
