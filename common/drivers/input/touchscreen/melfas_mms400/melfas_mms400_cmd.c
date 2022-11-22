/*
 * MELFAS MMS400 Touchscreen Driver -Command Functions (Optional)
 *
 * Copyright (C) 2014 MELFAS Inc.
 *
 */

#include "melfas_mms400.h"

#if MMS_USE_CMD_MODE

enum CMD_STATUS {
	CMD_STATUS_WAITING = 0,
	CMD_STATUS_RUNNING,
	CMD_STATUS_OK,
	CMD_STATUS_FAIL,
	CMD_STATUS_NONE,
};

/**
* Clear command result
*/
static void cmd_clear_result(struct mms_ts_info *info)
{
	char delim = ':';
	memset(info->cmd_result, 0x00, ARRAY_SIZE(info->cmd_result));
	memcpy(info->cmd_result, info->cmd, strnlen(info->cmd, CMD_LEN));
	strncat(info->cmd_result, &delim, 1);
}

/**
* Set command result
*/
static void cmd_set_result(struct mms_ts_info *info, char *buf, int len)
{
	strncat(info->cmd_result, buf, len);
}

/**
* Command : Get firmware version
*/
static void cmd_get_fw_ver(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;
	char buf[64] = { 0 };
	u8 rbuf[64];
	
	cmd_clear_result(info);

	if(mms_get_fw_version(info, rbuf)){
		info->cmd_state = CMD_STATUS_FAIL;
		goto EXIT;
	}
		
	sprintf(buf, "%02X.%02X %02X.%02X %02X.%02X %02X.%02X\n", rbuf[0], rbuf[1], rbuf[2], rbuf[3], rbuf[4], rbuf[5], rbuf[6], rbuf[7]);	
	cmd_set_result(info, buf, strnlen(buf, sizeof(buf)));
	
	info->cmd_state = CMD_STATUS_OK;

EXIT:	
	dev_dbg(&info->client->dev, "%s - cmd[%s] len[%d] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), info->cmd_state);
}

/**
* Command : Get chip vendor
*/
static void cmd_get_chip_vendor(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;
	char buf[64] = { 0 };
	
	cmd_clear_result(info);

	sprintf(buf, "MELFAS");
	cmd_set_result(info, buf, strnlen(buf, sizeof(buf)));
	
	info->cmd_state = CMD_STATUS_OK;
		
	dev_dbg(&info->client->dev, "%s - cmd[%s] len[%d] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), info->cmd_state);
}

/**
* Command : Get chip name
*/
static void cmd_get_chip_name(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;
	char buf[64] = { 0 };
	
	cmd_clear_result(info);

	sprintf(buf, CHIP_NAME);
	cmd_set_result(info, buf, strnlen(buf, sizeof(buf)));
	
	info->cmd_state = CMD_STATUS_OK;
	
	dev_dbg(&info->client->dev, "%s - cmd[%s] len[%d] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), info->cmd_state);
}

/**
* Command : Get X ch num
*/
static void cmd_get_x_num(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;
	char buf[64] = { 0 };
	u8 rbuf[64];
	u8 wbuf[64];
	int val;
	
	cmd_clear_result(info);

	wbuf[0] = MIP_R0_INFO;
	wbuf[1] = MIP_R1_INFO_NODE_NUM_X;
	if(mms_i2c_read(info, wbuf, 2, rbuf, 1)){
		info->cmd_state = CMD_STATUS_FAIL;
		goto EXIT;
	}

	val = wbuf[0];
	
	sprintf(buf, "%d", val);
	cmd_set_result(info, buf, strnlen(buf, sizeof(buf)));
	
	info->cmd_state = CMD_STATUS_OK;
	
EXIT:	
	dev_dbg(&info->client->dev, "%s - cmd[%s] len[%d] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), info->cmd_state);
}

/**
* Command : Get Y ch num
*/
static void cmd_get_y_num(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;
	char buf[64] = { 0 };
	u8 rbuf[64];
	u8 wbuf[64];
	int val;
	
	cmd_clear_result(info);

	wbuf[0] = MIP_R0_INFO;
	wbuf[1] = MIP_R1_INFO_NODE_NUM_Y;
	if(mms_i2c_read(info, wbuf, 2, rbuf, 1)){
		info->cmd_state = CMD_STATUS_FAIL;
		goto EXIT;
	}

	val = wbuf[0];
	
	sprintf(buf, "%d", val);
	cmd_set_result(info, buf, strnlen(buf, sizeof(buf)));
	
	info->cmd_state = CMD_STATUS_OK;
	
EXIT:	
	dev_dbg(&info->client->dev, "%s - cmd[%s] len[%d] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), info->cmd_state);
}

/**
* Command : Get X resolution
*/
static void cmd_get_max_x(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;
	char buf[64] = { 0 };
	u8 rbuf[64];
	u8 wbuf[64];
	int val;
	
	cmd_clear_result(info);

	wbuf[0] = MIP_R0_INFO;
	wbuf[1] = MIP_R1_INFO_RESOLUTION_X;
	if(mms_i2c_read(info, wbuf, 2, rbuf, 2)){
		info->cmd_state = CMD_STATUS_FAIL;
		goto EXIT;
	}

	val = (wbuf[0] << 8) || wbuf[1];
	
	sprintf(buf, "%d", val);
	cmd_set_result(info, buf, strnlen(buf, sizeof(buf)));
	
	info->cmd_state = CMD_STATUS_OK;
	
EXIT:	
	dev_dbg(&info->client->dev, "%s - cmd[%s] len[%d] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), info->cmd_state);
}

/**
* Command : Get Y resolution
*/
static void cmd_get_max_y(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;
	char buf[64] = { 0 };
	u8 rbuf[64];
	u8 wbuf[64];
	int val;
	
	cmd_clear_result(info);

	wbuf[0] = MIP_R0_INFO;
	wbuf[1] = MIP_R1_INFO_RESOLUTION_Y;
	if(mms_i2c_read(info, wbuf, 2, rbuf, 2)){
		info->cmd_state = CMD_STATUS_FAIL;
		goto EXIT;
	}

	val = (wbuf[0] << 8) || wbuf[1];
	
	sprintf(buf, "%d", val);
	cmd_set_result(info, buf, strnlen(buf, sizeof(buf)));
	
	info->cmd_state = CMD_STATUS_OK;
	
EXIT:	
	dev_dbg(&info->client->dev, "%s - cmd[%s] len[%d] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), info->cmd_state);
}

/**
* Command : Unknown cmd
*/
static void cmd_unknown_cmd(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;
	char buf[16] = { 0 };
	
	cmd_clear_result(info);
	
	snprintf(buf, sizeof(buf), "%s", "unknown_cmd");
	cmd_set_result(info, buf, strnlen(buf, sizeof(buf)));

	info->cmd_state = CMD_STATUS_NONE;
		
	dev_dbg(&info->client->dev, "%s - cmd[%s] len[%d] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), info->cmd_state);
}

#define MMS_CMD(name, func)	.cmd_name = name, .cmd_func = func

/**
* Info of command function
*/
struct mms_cmd {
	struct list_head list;
	const char *cmd_name;
	void (*cmd_func) (void *device_data);
};

/**
* List of command functions
*/
static struct mms_cmd mms_commands[] = {
	{MMS_CMD("get_fw_ver", cmd_get_fw_ver),},
	{MMS_CMD("get_chip_vendor", cmd_get_chip_vendor),},
	{MMS_CMD("get_chip_name", cmd_get_chip_name),},
	{MMS_CMD("get_x_num", cmd_get_x_num),},
	{MMS_CMD("get_y_num", cmd_get_y_num),},
	{MMS_CMD("get_max_x", cmd_get_max_x),},
	{MMS_CMD("get_max_y", cmd_get_max_y),},
	//{MMS_CMD("glove_mode", cmd_glove_mode),},
	//{MMS_CMD("cover_mode", cmd_cover_mode),},
	//...
	{MMS_CMD("unknown_cmd", cmd_unknown_cmd),},
};

/**
* Sysfs - recv command
*/
static ssize_t mms_sys_cmd(struct device *dev, struct device_attribute *devattr, const char *buf, size_t count)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);
	int ret;
	char *cur, *start, *end;
	char cbuf[CMD_LEN] = { 0 };
	int len, i;
	struct mms_cmd *mms_cmd_ptr = NULL;
	char delim = ',';
	bool cmd_found = false;
	int param_cnt = 0;

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);
	dev_dbg(&info->client->dev, "%s - input [%s]\n", __func__, buf);

	if (!info) {
		dev_err(&info->client->dev, "%s [ERROR] mms_ts_info not found\n", __func__);
		ret = -EINVAL;
		goto ERROR;
	}
	
	if (!info->input_dev) {
		dev_err(&info->client->dev, "%s [ERROR] input_dev not found\n", __func__);
		ret = -EINVAL;
		goto ERROR;
	}

	if (info->cmd_busy == true) {
		dev_err(&info->client->dev, "%s [ERROR] previous command is not ended\n", __func__);
		ret = -1;
		goto ERROR;
	}

	mutex_lock(&info->lock);
	info->cmd_busy = true;
	mutex_unlock(&info->lock);
	
	info->cmd_state = 1;
	for (i = 0; i < ARRAY_SIZE(info->cmd_param); i++){
		info->cmd_param[i] = 0;
	}
	
	len = (int)count;
	if (*(buf + len - 1) == '\n'){
		len--;
	}
	
	memset(info->cmd, 0x00, ARRAY_SIZE(info->cmd));
	memcpy(info->cmd, buf, len);
	cur = strchr(buf, (int)delim);
	if (cur){
		memcpy(cbuf, buf, cur - buf);
	}
	else{
		memcpy(cbuf, buf, len);
	}
	dev_dbg(&info->client->dev, "%s - command [%s]\n", __func__, cbuf);

	//command
	list_for_each_entry(mms_cmd_ptr, &info->cmd_list_head, list) {
		if (!strncmp(cbuf, mms_cmd_ptr->cmd_name, CMD_LEN)) {
			cmd_found = true;
			break;
		}
	}
	if (!cmd_found) {
		list_for_each_entry(mms_cmd_ptr, &info->cmd_list_head, list) {
			if (!strncmp("unknown_cmd", mms_cmd_ptr->cmd_name, CMD_LEN)){
				break;
			}
		}
	}

	//parameter
	if (cur && cmd_found) {
		cur++;
		start = cur;
		memset(cbuf, 0x00, ARRAY_SIZE(cbuf));

		do {
			if (*cur == delim || cur - buf == len) {
				end = cur;
				memcpy(cbuf, start, end - start);
				*(cbuf + strnlen(cbuf, ARRAY_SIZE(cbuf))) = '\0';
				if (kstrtoint(cbuf, 10, info->cmd_param + param_cnt) < 0){					
					goto ERROR;
				}
				start = cur + 1;
				memset(cbuf, 0x00, ARRAY_SIZE(cbuf));
				param_cnt++;
			}
			cur++;
		} while (cur - buf <= len);
	}

	//print
	dev_dbg(&info->client->dev, "%s - cmd [%s]\n", __func__, mms_cmd_ptr->cmd_name);
	for (i = 0; i < param_cnt; i++){
		dev_dbg(&info->client->dev, "%s - param #%d [%d]\n", __func__, i, info->cmd_param[i]);
	}

	//execute
	mms_cmd_ptr->cmd_func(info);

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return count;
	
ERROR:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);	
	return count;	
}
static DEVICE_ATTR(cmd, 0666, NULL, mms_sys_cmd);

/**
* Sysfs - print command status
*/
static ssize_t mms_sys_cmd_status(struct device *dev, struct device_attribute *devattr, char *buf)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);
	int ret;
	char cbuf[32] = {0};

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	dev_dbg(&info->client->dev, "%s - status [%d]\n", __func__, info->cmd_state);

	if (info->cmd_state == CMD_STATUS_WAITING){
		snprintf(cbuf, sizeof(cbuf), "WAITING");
	}
	else if (info->cmd_state == CMD_STATUS_RUNNING){
		snprintf(cbuf, sizeof(cbuf), "RUNNING");
	}
	else if (info->cmd_state == CMD_STATUS_OK){
		snprintf(cbuf, sizeof(cbuf), "OK");
	}
	else if (info->cmd_state == CMD_STATUS_FAIL){
		snprintf(cbuf, sizeof(cbuf), "FAIL");
	}
	else if (info->cmd_state == CMD_STATUS_NONE){
		snprintf(cbuf, sizeof(cbuf), "NOT_APPLICABLE");
	}

	ret = snprintf(buf, PAGE_SIZE, "%s\n", cbuf);
	//memset(info->print_buf, 0, 4096);

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);

	return ret;
}
static DEVICE_ATTR(cmd_status, 0666, mms_sys_cmd_status, NULL);

/**
* Sysfs - print command result
*/
static ssize_t mms_sys_cmd_result(struct device *dev, struct device_attribute *devattr, char *buf)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);
	int ret;

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	dev_dbg(&info->client->dev, "%s - result [%s]\n", __func__, info->cmd_result);

	mutex_lock(&info->lock);
	info->cmd_busy = false;
	mutex_unlock(&info->lock);
	
	info->cmd_state = CMD_STATUS_WAITING;

	ret = snprintf(buf, PAGE_SIZE, "%s\n", info->cmd_result);
	//memset(info->print_buf, 0, 4096);
	
	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);

	return ret;
}
static DEVICE_ATTR(cmd_result, 0666, mms_sys_cmd_result, NULL);

/**
* Sysfs - print command list
*/
static ssize_t mms_sys_cmd_list(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);
	int ret;
	int i = 0;
	char buffer[info->cmd_buffer_size];
	char buffer_name[CMD_LEN];

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	snprintf(buffer, 30, "== Command list ==\n");
	while (strncmp(mms_commands[i].cmd_name, "unknown_cmd", CMD_LEN) != 0) {
		snprintf(buffer_name, CMD_LEN, "%s\n", mms_commands[i].cmd_name);
		strcat(buffer, buffer_name);
		i++;
	}
	
	dev_dbg(&info->client->dev, "%s - length [%u / %d]\n", __func__, strlen(buffer), info->cmd_buffer_size);

	ret = snprintf(buf, PAGE_SIZE, "%s\n", buffer);
	//memset(info->print_buf, 0, 4096);

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);

	return ret;
}
static DEVICE_ATTR(cmd_list, 0666, mms_sys_cmd_list, NULL);

/**
* Sysfs - cmd attr info
*/
static struct attribute *mms_cmd_attr[] = {
	&dev_attr_cmd.attr,
	&dev_attr_cmd_status.attr,
	&dev_attr_cmd_result.attr,
	&dev_attr_cmd_list.attr,
	NULL,
};

/**
* Sysfs - cmd attr group info
*/
static const struct attribute_group mms_cmd_attr_group = {
	.attrs = mms_cmd_attr,
};

/**
* Create sysfs command functions
*/
int mms_sysfs_cmd_create(struct mms_ts_info *info)
{
	struct i2c_client *client = info->client;
	int i = 0;
	
	INIT_LIST_HEAD(&info->cmd_list_head);

	info->cmd_buffer_size = 0;
	for (i = 0; i < ARRAY_SIZE(mms_commands); i++) {
		list_add_tail(&mms_commands[i].list, &info->cmd_list_head);
		if(mms_commands[i].cmd_name){
			info->cmd_buffer_size += strlen(mms_commands[i].cmd_name) + 1;
		}
	}

	info->cmd_busy = false;
	
	if (sysfs_create_group(&client->dev.kobj, &mms_cmd_attr_group)) {
		dev_err(&client->dev, "%s [ERROR] sysfs_create_group\n", __func__);
		return -EAGAIN;
	}
	
	info->print_buf = kzalloc(sizeof(u8) * 4096, GFP_KERNEL);

	return 0;
}

/**
* Remove sysfs command functions
*/
void mms_sysfs_cmd_remove(struct mms_ts_info *info)
{
	sysfs_remove_group(&info->client->dev.kobj, &mms_cmd_attr_group);
		
	kfree(info->print_buf);
	
	return;
}

#endif

