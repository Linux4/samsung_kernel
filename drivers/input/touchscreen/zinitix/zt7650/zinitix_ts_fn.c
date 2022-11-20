#include "zinitix_ts.h"

static void fw_update(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct zt_ts_platform_data *pdata = info->pdata;
	struct i2c_client *client = info->client;
	const u8 *buff = 0;
	mm_segment_t old_fs = {0};
	struct file *fp = NULL;
	long fsize = 0, nread = 0;
	char fw_path[MAX_FW_PATH+1];
	char result[16] = {0};
	const struct firmware *tsp_fw = NULL;
	unsigned char *fw_data = NULL;
	int restore_cal;
	int ret;
	u8 update_type = 0;

	sec_cmd_set_default_result(sec);

	switch (sec->cmd_param[0]) {
	case BUILT_IN:
		update_type = TSP_TYPE_BUILTIN_FW;
		break;
	case UMS:
#ifdef CONFIG_SAMSUNG_PRODUCT_SHIP
		update_type = TSP_TYPE_EXTERNAL_FW_SIGNED;
#else
		update_type = TSP_TYPE_EXTERNAL_FW;
#endif
		break;
	case SPU:
		update_type = TSP_TYPE_SPU_FW_SIGNED;
		break;
	default:
		goto fw_update_out;
	}

	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	if (update_type == TSP_TYPE_BUILTIN_FW) {
		/* firmware update builtin binary */
		snprintf(fw_path, MAX_FW_PATH, "%s", pdata->firmware_name);

		ret = request_firmware(&tsp_fw, fw_path, &(client->dev));
		if (ret) {
			input_info(true, &client->dev,
					"%s: Firmware image %s not available\n", __func__,
					fw_path);
			if (tsp_fw)
				release_firmware(tsp_fw);

			goto fw_update_out;
		} else {
			fw_data = (unsigned char *)tsp_fw->data;
		}

#ifdef TCLM_CONCEPT
		sec_tclm_root_of_cal(info->tdata, CALPOSITION_TESTMODE);
		restore_cal = 1;
#endif
		ret = ts_upgrade_sequence(info, (u8*)fw_data, restore_cal);
		release_firmware(tsp_fw);
		if (ret < 0)
			goto fw_update_out;

		sec->cmd_state = SEC_CMD_STATUS_OK;
	} else {
		/* firmware update ums or spu */
		if (update_type == TSP_TYPE_EXTERNAL_FW)
			snprintf(fw_path, MAX_FW_PATH, TSP_PATH_EXTERNAL_FW);
		else if (update_type == TSP_TYPE_EXTERNAL_FW_SIGNED)
			snprintf(fw_path, MAX_FW_PATH, TSP_PATH_EXTERNAL_FW_SIGNED);
		else if (update_type == TSP_TYPE_SPU_FW_SIGNED)
			snprintf(fw_path, MAX_FW_PATH, TSP_PATH_SPU_FW_SIGNED);

		old_fs = get_fs();
		set_fs(get_ds());

		fp = filp_open(fw_path, O_RDONLY, 0);
		if (IS_ERR(fp)) {
			input_err(true, &client->dev, "file %s open error\n", fw_path);
			set_fs(old_fs);
			goto fw_update_out;
		}

		fsize = fp->f_path.dentry->d_inode->i_size;

		buff = vzalloc(fsize);
		if (!buff) {
			input_err(true, &client->dev, "%s: failed to alloc buffer for fw\n", __func__);
			filp_close(fp, NULL);
			set_fs(old_fs);
			goto fw_update_out;
		}

		nread = vfs_read(fp, (char __user *)buff, fsize, &fp->f_pos);
		if (nread != fsize) {
			filp_close(fp, NULL);
			set_fs(old_fs);
			goto fw_update_out;
		}

		info->cap_info.ic_fw_size = (buff[0x69] << 16) | (buff[0x6A] << 8) | buff[0x6B]; //total size
		/* signed firmware is not equal with fw.buf_size: add tag and signature */
		if (update_type == TSP_TYPE_EXTERNAL_FW) {
			if (fsize != info->cap_info.ic_fw_size) {
				input_err(true, &client->dev, "%s: invalid fw size!!\n", __func__);
				input_info(true, &client->dev, "f/w size = %ld ic_fw_size = %d \n", fsize, info->cap_info.ic_fw_size);
				filp_close(fp, NULL);
				set_fs(old_fs);
				goto fw_update_out;
			}
		} else {
			input_info(true, &client->dev, "%s: signed firmware\n", __func__);
		}

		filp_close(fp, current->files);
		set_fs(old_fs);
		input_info(true, &client->dev, "%s: ums fw is loaded!!\n", __func__);

#ifdef SPU_FW_SIGNED
		info->fw_data = (unsigned char *)buff;
		if (!(ts_check_need_upgrade(info, info->cap_info.fw_version, info->cap_info.fw_minor_version,
						info->cap_info.reg_data_version, info->cap_info.hw_id)) &&
				(update_type == TSP_TYPE_SPU_FW_SIGNED)) {
			input_info(true, &client->dev, "%s: skip ffu update\n", __func__);
			goto fw_update_out;
		}

		if (update_type == TSP_TYPE_EXTERNAL_FW_SIGNED || update_type == TSP_TYPE_SPU_FW_SIGNED) {
			int ori_size;
			int spu_ret;

			ori_size = fsize - SPU_METADATA_SIZE(TSP);

			spu_ret = spu_firmware_signature_verify("TSP", buff, fsize);
			if (ori_size != spu_ret) {
				input_err(true, &client->dev, "%s: signature verify failed, ori:%d, fsize:%ld\n",
						__func__, ori_size, fsize);

				goto fw_update_out;
			}
		}
#endif

#ifdef TCLM_CONCEPT
		sec_tclm_root_of_cal(info->tdata, CALPOSITION_TESTMODE);
		restore_cal = 1;
#endif
		ret = ts_upgrade_sequence(info, (u8*)buff, restore_cal);
		if (ret < 0)
			goto fw_update_out;

		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

fw_update_out:
#ifdef TCLM_CONCEPT
	sec_tclm_root_of_cal(info->tdata, CALPOSITION_NONE);
#endif
	if (!buff)
		kfree(buff);

	if (sec->cmd_state == SEC_CMD_STATUS_OK)
		snprintf(result, sizeof(result), "OK");
	else
		snprintf(result, sizeof(result), "NG");

	sec_cmd_set_cmd_result(sec, result, strnlen(result, sizeof(result)));
	return;
}

static void get_fw_ver_bin(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct zt_ts_platform_data *pdata = info->pdata;
	struct i2c_client *client = info->client;
	const struct firmware *tsp_fw = NULL;
	unsigned char *fw_data = NULL;
	char fw_path[MAX_FW_PATH];
	char buff[16] = { 0 };
	u16 fw_version, fw_minor_version, reg_version, hw_id, ic_revision;
	u32 version;
	int ret;

	snprintf(fw_path, MAX_FW_PATH, "%s", pdata->firmware_name);

	ret = request_firmware(&tsp_fw, fw_path, &(client->dev));
	if (ret) {
		input_info(true, &client->dev,
				"%s: Firmware image %s not available\n", __func__,
				fw_path);
		goto fw_request_fail;
	} else {
		fw_data = (unsigned char *)tsp_fw->data;
	}
	sec_cmd_set_default_result(sec);

	/* To Do */
	/* modify m_firmware_data */
	hw_id = (u16)(fw_data[48] | (fw_data[49] << 8));
	fw_version = (u16)(fw_data[52] | (fw_data[53] << 8));
	fw_minor_version = (u16)(fw_data[56] | (fw_data[57] << 8));
	reg_version = (u16)(fw_data[60] | (fw_data[61] << 8));

	ic_revision = (u16)(fw_data[68] | (fw_data[69] << 8));
	version = (u32)((u32)(ic_revision & 0xff) << 24)
		| ((fw_minor_version & 0xff) << 16)
		| ((hw_id & 0xff) << 8) | (reg_version & 0xff);

	snprintf(buff, sizeof(buff), "ZI%08X", version);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "FW_VER_BIN");
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s: %s(%d)\n", __func__, sec->cmd_result,
			(int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));

fw_request_fail:
	if (tsp_fw)
		release_firmware(tsp_fw);
	return;
}

static void get_fw_ver_ic(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[16] = { 0 };
	char model[16] = { 0 };
	u16 fw_version, fw_minor_version, reg_version, hw_id, vendor_id, ic_revision;
	u32 version, length;
	int ret;

	sec_cmd_set_default_result(sec);

	zt_ts_esd_timer_stop(info);

	mutex_lock(&info->work_lock);

	ret = ic_version_check(info);
	mutex_unlock(&info->work_lock);
	if (ret < 0) {
		input_info(true, &client->dev, "%s: version check error\n", __func__);
		return;
	}

	zt_ts_esd_timer_start(info);

	fw_version = info->cap_info.fw_version;
	fw_minor_version = info->cap_info.fw_minor_version;
	reg_version = info->cap_info.reg_data_version;
	hw_id = info->cap_info.hw_id;
	ic_revision=  info->cap_info.ic_revision;

	vendor_id = ntohs(info->cap_info.vendor_id);
	version = (u32)((u32)(ic_revision & 0xff) << 24)
		| ((fw_minor_version & 0xff) << 16)
		| ((hw_id & 0xff) << 8) | (reg_version & 0xff);

	length = sizeof(vendor_id);
	snprintf(buff, length + 1, "%s", (u8 *)&vendor_id);
	snprintf(buff + length, sizeof(buff) - length, "%08X", version);
	snprintf(model, length + 1, "%s", (u8 *)&vendor_id);
	snprintf(model + length, sizeof(model) - length, "%04X", version >> 16);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "FW_VER_IC");
		sec_cmd_set_cmd_result_all(sec, model, strnlen(model, sizeof(model)), "FW_MODEL");
	}
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s: %s(%d)\n", __func__, sec->cmd_result,
			(int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));

	return;
}

static void get_checksum_data(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[16] = { 0 };
	u16 checksum;

	sec_cmd_set_default_result(sec);

	read_data(client, ZT_CHECKSUM, (u8 *)&checksum, 2);

	snprintf(buff, sizeof(buff), "0x%X", checksum);
	input_info(true, &client->dev, "%s %d %x\n",__func__,checksum,checksum);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s: %s(%d)\n", __func__, sec->cmd_result,
			(int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));

	return;
}

static void get_threshold(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[20] = { 0 };

	sec_cmd_set_default_result(sec);

	read_data(client, ZT_THRESHOLD, (u8 *)&info->cap_info.threshold, 2);

	snprintf(buff, sizeof(buff), "%d", info->cap_info.threshold);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s: %s(%d)\n", __func__, sec->cmd_result,
			(int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));

	return;
}

static void get_module_vendor(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	char buff[16] = {0};

	sec_cmd_set_default_result(sec);

	snprintf(buff, sizeof(buff),  "%s", tostring(NA));
	sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
}

static void get_chip_vendor(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	snprintf(buff, sizeof(buff), "%s", ZT_VENDOR_NAME);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "IC_VENDOR");
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s: %s(%d)\n", __func__, sec->cmd_result,
			(int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));

	return;
}


static void get_chip_name(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct zt_ts_platform_data *pdata = info->pdata;
	struct i2c_client *client = info->client;
	const char *name_buff;
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	if (pdata->chip_name)
		name_buff = pdata->chip_name;
	else
		name_buff = ZT_CHIP_NAME;

	snprintf(buff, sizeof(buff), "%s", name_buff);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "IC_NAME");
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s: %s(%d)\n", __func__, sec->cmd_result,
			(int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));

	return;
}

static void get_x_num(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	read_data(client, ZT_TOTAL_NUMBER_OF_X, (u8 *)&info->cap_info.x_node_num, 2);

	snprintf(buff, sizeof(buff), "%u", info->cap_info.x_node_num);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s: %s(%d)\n", __func__, sec->cmd_result,
			(int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));

	return;
}

static void get_y_num(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	read_data(client, ZT_TOTAL_NUMBER_OF_Y, (u8 *)&info->cap_info.y_node_num, 2);

	snprintf(buff, sizeof(buff), "%u", info->cap_info.y_node_num);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s: %s(%d)\n", __func__, sec->cmd_result,
			(int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));

	return;
}

static void run_trx_short_test(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	struct tsp_raw_data *raw_data = info->raw_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u8 temp[10];
	char test[32];

	sec_cmd_set_default_result(sec);

	if (info->tsp_pwr_enabled == POWER_OFF) {
		input_err(true, &info->client->dev, "%s: IC is power off\n", __func__);
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	if (sec->cmd_param[1])
		snprintf(test, sizeof(test), "TEST=%d,%d", sec->cmd_param[0], sec->cmd_param[1]);
	else
		snprintf(test, sizeof(test), "TEST=%d", sec->cmd_param[0]);

	/*
	 * run_test_open_short() need to be fix for separate by test item(open, short, pattern open)
	 */
	if (sec->cmd_param[0] == 1 && sec->cmd_param[1] == 1)
		run_test_open_short(info);

	if (sec->cmd_param[0] == 1 && sec->cmd_param[1] == 1) {
		/* 1,1 : open  */
		if (raw_data->channel_test_data[0] != TEST_CHANNEL_OPEN)
			goto OK;

		memset(temp, 0x00, sizeof(temp));
		snprintf(temp, sizeof(temp), "OPEN: ");
		strlcat(buff, temp, SEC_CMD_STR_LEN);

		check_trx_channel_test(info, buff);
		input_info(true, &client->dev, "%s\n", buff);
	} else if (sec->cmd_param[0] == 1 && sec->cmd_param[1] == 2) {
		/* 1,2 : short  */
		if (raw_data->channel_test_data[0] != TEST_SHORT)
			goto OK;

		memset(temp, 0x00, sizeof(temp));
		snprintf(temp, sizeof(temp), "SHORT: ");
		strlcat(buff, temp, SEC_CMD_STR_LEN);

		check_trx_channel_test(info, buff);
		input_info(true, &client->dev, "%s\n", buff);
	} else if (sec->cmd_param[0] == 2) {
		/* 2 : micro open(pattern open)  */
		if (raw_data->channel_test_data[0] != TEST_PATTERN_OPEN)
			goto OK;

		memset(temp, 0x00, sizeof(temp));
		snprintf(temp, sizeof(temp), "CRACK: ");
		strlcat(buff, temp, SEC_CMD_STR_LEN);

		check_trx_channel_test(info, buff);
		input_info(true, &client->dev, "%s\n", buff);
	} else if (sec->cmd_param[0] == 3) {
		/* 3 : bridge short  */
		snprintf(buff, sizeof(buff), "NA");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;

		sec_cmd_send_event_to_user(sec, test, "RESULT=FAIL");

		input_info(true, &client->dev, "%s: \"%s\"(%d)\n", __func__, sec->cmd_result,
				(int)strlen(sec->cmd_result));
		return;
	} else {
		/* 0 or else : old command */
		if (raw_data->channel_test_data[0] == TEST_PASS)
			goto OK;
	}

	snprintf(buff, sizeof(buff), "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	sec_cmd_send_event_to_user(sec, test, "RESULT=FAIL");

	input_info(true, &client->dev, "%s: \"%s\"(%d)\n", __func__, sec->cmd_result,
			(int)strlen(sec->cmd_result));
	return;


OK:
	snprintf(buff, sizeof(buff), "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	sec_cmd_send_event_to_user(sec, test, "RESULT=PASS");

	input_info(true, &client->dev, "%s: \"%s\"(%d)\n", __func__, sec->cmd_result,
			(int)strlen(sec->cmd_result));
	return;

}

static void touch_aging_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	if (sec->cmd_param[0] == 1) {
		zt_ts_esd_timer_stop(info);
		ret = ts_set_touchmode(TOUCH_AGING_MODE);
	} else {
		ret = ts_set_touchmode(TOUCH_POINT_MODE);
		zt_ts_esd_timer_start(info);	
	}
	if (ret < 0) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buff, sizeof(buff), "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

out:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void run_cnd_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	struct tsp_raw_data *raw_data = info->raw_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u16 min, max;
	int i, j;
	int ret;

	zt_ts_esd_timer_stop(info);
	sec_cmd_set_default_result(sec);

	ret = ts_set_touchmode(TOUCH_RAW_MODE);
	if (ret < 0) {
		ts_set_touchmode(TOUCH_POINT_MODE);
		goto out;
	}
	get_raw_data(info, (u8 *)raw_data->cnd_data, 1);
	ts_set_touchmode(TOUCH_POINT_MODE);

	input_info(true,&client->dev, "%s: CND start\n", __func__);

	min = 0xFFFF;
	max = 0x0000;

	for (i = 0; i < info->cap_info.y_node_num; i++) {
		for (j = 0; j < info->cap_info.x_node_num; j++) {
			if (raw_data->cnd_data[i * info->cap_info.x_node_num + j] < min &&
					raw_data->cnd_data[i * info->cap_info.x_node_num + j] != 0)
				min = raw_data->cnd_data[i * info->cap_info.x_node_num + j];

			if (raw_data->cnd_data[i * info->cap_info.x_node_num + j] > max)
				max = raw_data->cnd_data[i * info->cap_info.x_node_num + j];
		}
	}
	snprintf(buff, sizeof(buff), "%d,%d", min, max);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "CND");
	sec->cmd_state = SEC_CMD_STATUS_OK;

	zt_display_rawdata(info, raw_data, TOUCH_RAW_MODE, 0);
out:
	if (ret < 0) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
			sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "CND");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}
	input_info(true, &client->dev, "%s: \"%s\"(%d)\n", __func__, sec->cmd_result,
			(int)strlen(sec->cmd_result));

	zt_ts_esd_timer_start(info);
	return;
}

static void run_cnd_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct tsp_raw_data *raw_data = info->raw_data;
	char buff[16] = { 0 };
	char *all_cmdbuff;
	int i, j;
	int ret;

	zt_ts_esd_timer_stop(info);
	sec_cmd_set_default_result(sec);

	ret = ts_set_touchmode(TOUCH_RAW_MODE);
	if (ret < 0) {
		ts_set_touchmode(TOUCH_POINT_MODE);
		goto out;
	}
	get_raw_data(info, (u8 *)raw_data->cnd_data, 1);
	ts_set_touchmode(TOUCH_POINT_MODE);

	all_cmdbuff = kzalloc(info->cap_info.x_node_num * info->cap_info.y_node_num * 6, GFP_KERNEL);
	if (!all_cmdbuff) {
		input_info(true, &info->client->dev, "%s: alloc failed\n", __func__);
		ret = -ENOMEM;
		goto out;
	}

	for (i = 0; i < info->cap_info.y_node_num; i++) {
		for (j = 0; j < info->cap_info.x_node_num; j++) {
			sprintf(buff, "%u,", raw_data->cnd_data[i * info->cap_info.x_node_num + j]);
			strcat(all_cmdbuff, buff);
		}
	}

	sec_cmd_set_cmd_result(sec, all_cmdbuff,
			strnlen(all_cmdbuff, sizeof(all_cmdbuff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	kfree(all_cmdbuff);

out:
	if (ret < 0) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}

	zt_ts_esd_timer_start(info);
	return;
}

static void run_dnd_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	struct tsp_raw_data *raw_data = info->raw_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u16 min, max;
	int i, j;
	int ret;

	zt_ts_esd_timer_stop(info);
	sec_cmd_set_default_result(sec);

	ret = ts_set_touchmode(TOUCH_DND_MODE);
	if (ret < 0) {
		ts_set_touchmode(TOUCH_POINT_MODE);
		goto out;
	}
	get_raw_data(info, (u8 *)raw_data->dnd_data, 1);
	ts_set_touchmode(TOUCH_POINT_MODE);

	min = 0xFFFF;
	max = 0x0000;

	for (i = 0; i < info->cap_info.y_node_num; i++) {
		for (j = 0; j < info->cap_info.x_node_num; j++) {
			if (raw_data->dnd_data[i * info->cap_info.x_node_num + j] < min &&
					raw_data->dnd_data[i * info->cap_info.x_node_num + j] != 0)
				min = raw_data->dnd_data[i * info->cap_info.x_node_num + j];

			if (raw_data->dnd_data[i * info->cap_info.x_node_num + j] > max)
				max = raw_data->dnd_data[i * info->cap_info.x_node_num + j];
		}
	}
	snprintf(buff, sizeof(buff), "%d,%d", min, max);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "DND");
	sec->cmd_state = SEC_CMD_STATUS_OK;

	zt_display_rawdata(info, raw_data, TOUCH_DND_MODE, 0);
out:
	if (ret < 0) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
			sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "DND");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}
	input_info(true, &client->dev, "%s: \"%s\"(%d)\n", __func__, sec->cmd_result,
			(int)strlen(sec->cmd_result));

	zt_ts_esd_timer_start(info);
	return;
}

static void run_dnd_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct tsp_raw_data *raw_data = info->raw_data;
	char buff[16] = { 0 };
	char *all_cmdbuff;
	int i, j;
	int ret;
	int buff_size = 0;

	zt_ts_esd_timer_stop(info);
	sec_cmd_set_default_result(sec);

	ret = ts_set_touchmode(TOUCH_DND_MODE);
	if (ret < 0) {
		ts_set_touchmode(TOUCH_POINT_MODE);
		goto out;
	}
	get_raw_data(info, (u8 *)raw_data->dnd_data, 1);
	ts_set_touchmode(TOUCH_POINT_MODE);

	buff_size = info->cap_info.x_node_num * info->cap_info.y_node_num * 6;
	all_cmdbuff = kzalloc(buff_size, GFP_KERNEL);
	if (!all_cmdbuff) {
		input_info(true, &info->client->dev, "%s: alloc failed\n", __func__);
		ret = -ENOMEM;
		goto out;
	}

	for (i = 0; i < info->cap_info.y_node_num; i++) {
		for (j = 0; j < info->cap_info.x_node_num; j++) {
			sprintf(buff, "%u,", raw_data->dnd_data[i * info->cap_info.x_node_num + j]);
			strcat(all_cmdbuff, buff);
		}
	}

	sec_cmd_set_cmd_result(sec, all_cmdbuff,
			strnlen(all_cmdbuff, buff_size));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	kfree(all_cmdbuff);

out:
	if (ret < 0) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}

	zt_ts_esd_timer_start(info);
	return;
}

static void run_dnd_v_gap_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	struct tsp_raw_data *raw_data = info->raw_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	char buff_onecmd_1[SEC_CMD_STR_LEN] = { 0 };
	int x_num = info->cap_info.x_node_num, y_num = info->cap_info.y_node_num;
	int i, j, offset, val, cur_val, next_val;
	u16 screen_max = 0x0000;

	sec_cmd_set_default_result(sec);

	memset(raw_data->vgap_data, 0x00, TSP_CMD_NODE_NUM);

	input_info(true, &client->dev, "%s: DND V Gap start\n", __func__);

	input_info(true, &client->dev, "%s : ++++++ DND SPEC +++++++++\n",__func__);
	for (i = 0; i < y_num - 1; i++) {
		for (j = 0; j < x_num; j++) {
			offset = (i * x_num) + j;

			cur_val = raw_data->dnd_data[offset];
			next_val = raw_data->dnd_data[offset + x_num];
			if (!next_val) {
				raw_data->vgap_data[offset] = next_val;
				continue;
			}

			if (next_val > cur_val)
				val = 100 - ((cur_val * 100) / next_val);
			else
				val = 100 - ((next_val * 100) / cur_val);

			raw_data->vgap_data[offset] = val;

			if (raw_data->vgap_data[i * x_num + j] > screen_max)
				screen_max = raw_data->vgap_data[i * x_num + j];
		}
	}

	input_info(true, &client->dev, "DND V Gap screen_max %d\n", screen_max);
	snprintf(buff, sizeof(buff), "%d", screen_max);
	snprintf(buff_onecmd_1, sizeof(buff_onecmd_1), "%d,%d", 0, screen_max);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff_onecmd_1, strnlen(buff_onecmd_1, sizeof(buff_onecmd_1)), "DND_V_GAP");

	sec->cmd_state = SEC_CMD_STATUS_OK;

	zt_display_rawdata(info, raw_data, TOUCH_DND_MODE, 1);

	return;
}

static void run_dnd_h_gap_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	struct tsp_raw_data *raw_data = info->raw_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	char buff_onecmd_1[SEC_CMD_STR_LEN] = { 0 };
	int x_num = info->cap_info.x_node_num, y_num = info->cap_info.y_node_num;
	int i, j, offset, val, cur_val, next_val;
	u16 screen_max = 0x0000;

	sec_cmd_set_default_result(sec);

	memset(raw_data->hgap_data, 0x00, TSP_CMD_NODE_NUM);

	input_info(true, &client->dev, "%s: DND H Gap start\n", __func__);

	for (i = 0; i < y_num; i++) {
		for (j = 0; j < x_num - 1; j++) {
			offset = (i * x_num) + j;

			cur_val = raw_data->dnd_data[offset];
			if (!cur_val) {
				raw_data->hgap_data[offset] = cur_val;
				continue;
			}

			next_val = raw_data->dnd_data[offset + 1];
			if (!next_val) {
				raw_data->hgap_data[offset] = next_val;
				for (++j; j < x_num - 1; j++) {
					offset = (i * x_num) + j;

					next_val = raw_data->dnd_data[offset];
					if (!next_val) {
						raw_data->hgap_data[offset] = next_val;
						continue;
					}
					break;
				}
			}

			if (next_val > cur_val)
				val = 100 - ((cur_val * 100) / next_val);
			else
				val = 100 - ((next_val * 100) / cur_val);

			raw_data->hgap_data[offset] = val;

			if (raw_data->hgap_data[i * x_num + j] > screen_max)
				screen_max = raw_data->hgap_data[i * x_num + j];
		}
	}

	input_info(true, &client->dev, "DND H Gap screen_max %d\n", screen_max);
	snprintf(buff, sizeof(buff), "%d", screen_max);
	snprintf(buff_onecmd_1, sizeof(buff_onecmd_1), "%d,%d", 0, screen_max);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff_onecmd_1, strnlen(buff_onecmd_1, sizeof(buff_onecmd_1)), "DND_H_GAP");
	sec->cmd_state = SEC_CMD_STATUS_OK;

	zt_display_rawdata(info, raw_data, TOUCH_DND_MODE, 2);

	return;
}

static void run_dnd_h_gap_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct tsp_raw_data *raw_data = info->raw_data;
	char temp[SEC_CMD_STR_LEN] = { 0 };
	char *buff = NULL;
	int total_node = info->cap_info.x_node_num * info->cap_info.y_node_num;
	int i, j, offset, val, cur_val, next_val;

	sec_cmd_set_default_result(sec);

	buff = kzalloc(total_node * CMD_RESULT_WORD_LEN, GFP_KERNEL);
	if (!buff) {
		snprintf(temp, SEC_CMD_STR_LEN, "NG");
		sec_cmd_set_cmd_result(sec, temp, SEC_CMD_STR_LEN);
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	for (i = 0; i < info->cap_info.y_node_num; i++) {
		for (j = 0; j < info->cap_info.x_node_num - 1; j++) {
			offset = (i * info->cap_info.x_node_num) + j;

			cur_val = raw_data->dnd_data[offset];
			if (!cur_val) {
				raw_data->hgap_data[offset] = cur_val;
				continue;
			}

			next_val = raw_data->dnd_data[offset + 1];
			if (!next_val) {
				raw_data->hgap_data[offset] = next_val;
				for (++j; j < info->cap_info.x_node_num - 1; j++) {
					offset = (i * info->cap_info.x_node_num) + j;

					next_val = raw_data->dnd_data[offset];
					if (!next_val) {
						raw_data->hgap_data[offset] = next_val;
						continue;
					}
					break;
				}
			}

			if (next_val > cur_val) {
				if (next_val)
					val = 100 - ((cur_val * 100) / next_val);
				else
					input_info(true, &info->client->dev, "%s: next_val is zero\n", __func__);
			} else {
				if (cur_val)
					val = 100 - ((next_val * 100) / cur_val);
				else
					input_info(true, &info->client->dev, "%s: cur_val is zero\n", __func__);
			}

			raw_data->hgap_data[offset] = val;

			snprintf(temp, CMD_RESULT_WORD_LEN, "%d,", raw_data->hgap_data[offset]);
			strncat(buff, temp, CMD_RESULT_WORD_LEN);
			memset(temp, 0x00, SEC_CMD_STR_LEN);
		}
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, total_node * CMD_RESULT_WORD_LEN));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	kfree(buff);
}

static void run_dnd_v_gap_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct tsp_raw_data *raw_data = info->raw_data;
	char temp[SEC_CMD_STR_LEN] = { 0 };
	char *buff = NULL;
	int total_node = info->cap_info.x_node_num * info->cap_info.y_node_num;
	int i, j, offset, val, cur_val, next_val;

	sec_cmd_set_default_result(sec);

	buff = kzalloc(total_node * CMD_RESULT_WORD_LEN, GFP_KERNEL);
	if (!buff) {
		snprintf(temp, SEC_CMD_STR_LEN, "NG");
		sec_cmd_set_cmd_result(sec, temp, SEC_CMD_STR_LEN);
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	for (i = 0; i < info->cap_info.y_node_num - 1; i++) {
		for (j = 0; j < info->cap_info.x_node_num; j++) {
			offset = (i * info->cap_info.x_node_num) + j;

			cur_val = raw_data->dnd_data[offset];
			next_val = raw_data->dnd_data[offset + info->cap_info.x_node_num];
			if (!next_val) {
				raw_data->vgap_data[offset] = next_val;
				continue;
			}

			if (next_val > cur_val) {
				if (!next_val)
					val = 100 - ((cur_val * 100) / next_val);
				else
					input_info(true, &info->client->dev, "%s: next_val is zero\n", __func__);

			} else {
				if (!cur_val)
					val = 100 - ((next_val * 100) / cur_val);
				else
					input_info(true, &info->client->dev, "%s: cur_val is zero\n", __func__);
			}

			raw_data->vgap_data[offset] = val;

			snprintf(temp, CMD_RESULT_WORD_LEN, "%d,", raw_data->vgap_data[offset]);
			strncat(buff, temp, CMD_RESULT_WORD_LEN);
			memset(temp, 0x00, SEC_CMD_STR_LEN);
		}
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, total_node * CMD_RESULT_WORD_LEN));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	kfree(buff);
}

static void run_selfdnd_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	struct tsp_raw_data *raw_data = info->raw_data;
	int total_node = info->cap_info.x_node_num + info->cap_info.y_node_num;
	char tx_buff[SEC_CMD_STR_LEN] = { 0 };
	char rx_buff[SEC_CMD_STR_LEN] = { 0 };
	u16 tx_min, tx_max, rx_min, rx_max;
	int j;
	int ret;

	zt_ts_esd_timer_stop(info);
	sec_cmd_set_default_result(sec);

	ret = ts_set_touchmode(TOUCH_SELF_DND_MODE);
	if (ret < 0) {
		ts_set_touchmode(TOUCH_POINT_MODE);
		goto out;
	}
	get_raw_data(info, (u8 *)raw_data->selfdnd_data, 1);
	ts_set_touchmode(TOUCH_POINT_MODE);

	input_info(true,&client->dev, "%s: SELF DND start\n", __func__);

	tx_min = 0xFFFF;
	tx_max = 0x0000;
	rx_min = 0xFFFF;
	rx_max = 0x0000;

	for (j = 0; j < info->cap_info.x_node_num; j++) {
		if (raw_data->selfdnd_data[j] < rx_min && raw_data->selfdnd_data[j] != 0)
			rx_min = raw_data->selfdnd_data[j];

		if (raw_data->selfdnd_data[j] > rx_max)
			rx_max = raw_data->selfdnd_data[j];
	}

	for (j = info->cap_info.x_node_num; j < total_node; j++) {
		if (raw_data->selfdnd_data[j] < tx_min && raw_data->selfdnd_data[j] != 0)
			tx_min = raw_data->selfdnd_data[j];

		if (raw_data->selfdnd_data[j] > tx_max)
			tx_max = raw_data->selfdnd_data[j];
	}

	input_info(true, &client->dev, "%s: SELF DND Pass\n", __func__);

	snprintf(tx_buff, sizeof(tx_buff), "%d,%d", tx_min, tx_max);
	snprintf(rx_buff, sizeof(rx_buff), "%d,%d", rx_min, rx_max);
	sec_cmd_set_cmd_result(sec, rx_buff, strnlen(rx_buff, sizeof(rx_buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		sec_cmd_set_cmd_result_all(sec, rx_buff, strnlen(rx_buff, sizeof(rx_buff)), "SELF_DND_RX");
	}
	sec->cmd_state = SEC_CMD_STATUS_OK;

out:
	if (ret < 0) {
		snprintf(rx_buff, sizeof(rx_buff), "%s", "NG");
		sec_cmd_set_cmd_result(sec, rx_buff, strnlen(rx_buff, sizeof(rx_buff)));
		if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
			sec_cmd_set_cmd_result_all(sec, rx_buff, strnlen(rx_buff, sizeof(rx_buff)), "SELF_DND_RX");
		}
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}
	input_info(true, &client->dev, "%s: \"%s\"(%d)\n", __func__, sec->cmd_result,
			(int)strlen(sec->cmd_result));

	zt_ts_esd_timer_start(info);
	return;
}

static void run_charge_pump_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	struct tsp_raw_data *raw_data = info->raw_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;

	zt_ts_esd_timer_stop(info);
	sec_cmd_set_default_result(sec);

	ret = ts_set_touchmode(TOUCH_CHARGE_PUMP_MODE);
	if (ret < 0) {
		ts_set_touchmode(TOUCH_POINT_MODE);
		goto out;
	}
	get_raw_data(info, (u8 *)raw_data->charge_pump_data, 1);
	ts_set_touchmode(TOUCH_POINT_MODE);

	snprintf(buff, sizeof(buff), "%d,%d", raw_data->charge_pump_data[0], raw_data->charge_pump_data[0]);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "CHARGE_PUMP");
	sec->cmd_state = SEC_CMD_STATUS_OK;

out:
	if (ret < 0) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
			sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "CHARGE_PUMP");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}
	input_info(true, &client->dev, "%s: \"%s\"(%d)\n", __func__, sec->cmd_result,
			(int)strlen(sec->cmd_result));

	zt_ts_esd_timer_start(info);
	return;
}

static void run_selfdnd_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct tsp_raw_data *raw_data = info->raw_data;
	char temp[SEC_CMD_STR_LEN] = { 0 };
	char *buff = NULL;
	int total_node = info->cap_info.x_node_num + info->cap_info.y_node_num;
	int j;

	zt_ts_esd_timer_stop(info);
	sec_cmd_set_default_result(sec);

	ts_set_touchmode(TOUCH_SELF_DND_MODE);

	get_raw_data(info, (u8 *)raw_data->selfdnd_data, 1);
	ts_set_touchmode(TOUCH_POINT_MODE);

	buff = kzalloc(total_node * CMD_RESULT_WORD_LEN, GFP_KERNEL);
	if (!buff)
		goto NG;

	for (j = 0; j < total_node; j++) {
		snprintf(temp, CMD_RESULT_WORD_LEN, "%d,", raw_data->selfdnd_data[j]);
		strncat(buff, temp, CMD_RESULT_WORD_LEN);
		memset(temp, 0x00, SEC_CMD_STR_LEN);
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, total_node * CMD_RESULT_WORD_LEN));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	kfree(buff);

NG:
	if (sec->cmd_state != SEC_CMD_STATUS_OK) {
		snprintf(temp, SEC_CMD_STR_LEN, "NG");
		sec_cmd_set_cmd_result(sec, temp, SEC_CMD_STR_LEN);
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}
	zt_ts_esd_timer_start(info);
}

static void run_self_saturation_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	struct tsp_raw_data *raw_data = info->raw_data;
	int total_node = info->cap_info.x_node_num + info->cap_info.y_node_num;
	char tx_buff[SEC_CMD_STR_LEN] = { 0 };
	char rx_buff[SEC_CMD_STR_LEN] = { 0 };
	u16 tx_min, tx_max, rx_min, rx_max;
	int j;
	int ret;

	zt_ts_esd_timer_stop(info);
	sec_cmd_set_default_result(sec);

	ret = ts_set_touchmode(DEF_RAW_SELF_SSR_DATA_MODE);
	if (ret < 0) {
		ts_set_touchmode(TOUCH_POINT_MODE);
		goto out;
	}
	get_raw_data(info, (u8 *)raw_data->ssr_data, 1);
	ts_set_touchmode(TOUCH_POINT_MODE);

	input_info(true,&client->dev, "%s: SELF SATURATION start\n", __func__);

	tx_min = 0xFFFF;
	tx_max = 0x0000;
	rx_min = 0xFFFF;
	rx_max = 0x0000;

	for (j = 0; j < info->cap_info.x_node_num; j++) {
		if (raw_data->ssr_data[j] < rx_min && raw_data->ssr_data[j] != 0)
			rx_min = raw_data->ssr_data[j];

		if (raw_data->ssr_data[j] > rx_max)
			rx_max = raw_data->ssr_data[j];
	}

	for (j = info->cap_info.x_node_num; j < total_node; j++) {
		if (raw_data->ssr_data[j] < tx_min && raw_data->ssr_data[j] != 0)
			tx_min = raw_data->ssr_data[j];

		if (raw_data->ssr_data[j] > tx_max)
			tx_max = raw_data->ssr_data[j];
	}

	input_info(true, &client->dev, "%s: SELF SATURATION Pass\n", __func__);

	snprintf(tx_buff, sizeof(tx_buff), "%d,%d", tx_min, tx_max);
	snprintf(rx_buff, sizeof(rx_buff), "%d,%d", rx_min, rx_max);
	sec_cmd_set_cmd_result(sec, rx_buff, strnlen(rx_buff, sizeof(rx_buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		sec_cmd_set_cmd_result_all(sec, tx_buff, strnlen(tx_buff, sizeof(tx_buff)), "SELF_SATURATION_TX");
		sec_cmd_set_cmd_result_all(sec, rx_buff, strnlen(rx_buff, sizeof(rx_buff)), "SELF_SATURATION_RX");
	}
	sec->cmd_state = SEC_CMD_STATUS_OK;

out:
	if (ret < 0) {
		snprintf(rx_buff, sizeof(rx_buff), "%s", "NG");
		sec_cmd_set_cmd_result(sec, rx_buff, strnlen(rx_buff, sizeof(rx_buff)));
		if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
			sec_cmd_set_cmd_result_all(sec, rx_buff, strnlen(rx_buff, sizeof(rx_buff)), "SELF_SATURATION_TX");
			sec_cmd_set_cmd_result_all(sec, rx_buff, strnlen(rx_buff, sizeof(rx_buff)), "SELF_SATURATION_RX");
		}
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}
	input_info(true, &client->dev, "%s: \"%s\"(%d)\n", __func__, sec->cmd_result,
			(int)strlen(sec->cmd_result));

	zt_ts_esd_timer_start(info);
	return;
}

static void run_ssr_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct tsp_raw_data *raw_data = info->raw_data;
	char buff[16] = { 0 };
	int total_node = info->cap_info.x_node_num + info->cap_info.y_node_num;
	char *all_cmdbuff;
	int j;
	int ret;
	int buff_size = 0;

	zt_ts_esd_timer_stop(info);
	sec_cmd_set_default_result(sec);

	ret = ts_set_touchmode(DEF_RAW_SELF_SSR_DATA_MODE);
	if (ret < 0) {
		ts_set_touchmode(TOUCH_POINT_MODE);
		goto out;
	}
	get_raw_data(info, (u8 *)raw_data->ssr_data, 1);
	ts_set_touchmode(TOUCH_POINT_MODE);

	buff_size = total_node * 6;
	all_cmdbuff = kzalloc(buff_size, GFP_KERNEL);
	if (!all_cmdbuff) {
		input_info(true, &info->client->dev, "%s: alloc failed\n", __func__);
		ret = -ENOMEM;
		goto out;
	}

	for (j = 0; j < total_node; j++) {
		sprintf(buff, "%u,", raw_data->ssr_data[j]);
		strcat(all_cmdbuff, buff);
	}

	sec_cmd_set_cmd_result(sec, all_cmdbuff,
			strnlen(all_cmdbuff, buff_size));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	kfree(all_cmdbuff);

out:
	if (ret < 0) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}

	zt_ts_esd_timer_start(info);
	return;
}

static void run_selfdnd_h_gap_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	struct tsp_raw_data *raw_data = info->raw_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	char buff_onecmd[SEC_CMD_STR_LEN] = { 0 };
	int j, offset, val, cur_val, next_val;
	u16 screen_max = 0x0000;

	sec_cmd_set_default_result(sec);

	memset(raw_data->self_hgap_data, 0x00, TSP_CMD_NODE_NUM);

	input_info(true, &client->dev, "%s: SELFDND H Gap start\n", __func__);

	for (j = 0; j < info->cap_info.x_node_num - 1; j++) {
		offset = j;
		cur_val = raw_data->selfdnd_data[offset];

		if (!cur_val) {
			raw_data->self_hgap_data[offset] = cur_val;
			continue;
		}

		next_val = raw_data->selfdnd_data[offset + 1];

		if (next_val > cur_val)
			val = 100 - ((cur_val * 100) / next_val);
		else
			val = 100 - ((next_val * 100) / cur_val);

		raw_data->self_hgap_data[offset] = val;

		if (raw_data->self_hgap_data[j] > screen_max)
			screen_max = raw_data->self_hgap_data[j];

	}

	input_info(true, &client->dev, "SELFDND H Gap screen_max %d\n", screen_max);
	snprintf(buff, sizeof(buff), "%d", screen_max);
	snprintf(buff_onecmd, sizeof(buff_onecmd), "%d,%d", 0, screen_max);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff_onecmd, strnlen(buff_onecmd, sizeof(buff_onecmd)), "SELF_DND_H_GAP");
	sec->cmd_state = SEC_CMD_STATUS_OK;

	return;
}

static void run_selfdnd_h_gap_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct tsp_raw_data *raw_data = info->raw_data;
	char temp[SEC_CMD_STR_LEN] = { 0 };
	char *buff = NULL;
	int total_node = info->cap_info.y_node_num;
	int j, offset, val, cur_val, next_val;

	sec_cmd_set_default_result(sec);

	buff = kzalloc(total_node * CMD_RESULT_WORD_LEN, GFP_KERNEL);
	if (!buff) {
		snprintf(temp, SEC_CMD_STR_LEN, "NG");
		sec_cmd_set_cmd_result(sec, temp, SEC_CMD_STR_LEN);
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	for (j = 0; j < info->cap_info.x_node_num - 1; j++) {
		offset = j;
		cur_val = raw_data->selfdnd_data[offset];

		if (!cur_val) {
			raw_data->self_hgap_data[offset] = cur_val;
			continue;
		}

		next_val = raw_data->selfdnd_data[offset + 1];

		if (next_val > cur_val)
			val = 100 - ((cur_val * 100) / next_val);
		else
			val = 100 - ((next_val * 100) / cur_val);

		raw_data->self_hgap_data[offset] = val;

		snprintf(temp, CMD_RESULT_WORD_LEN, "%d,", raw_data->self_hgap_data[offset]);
		strncat(buff, temp, CMD_RESULT_WORD_LEN);
		memset(temp, 0x00, SEC_CMD_STR_LEN);
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, total_node * CMD_RESULT_WORD_LEN));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	kfree(buff);
}

static void run_mis_cal_read(void * device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct zt_ts_platform_data *pdata = info->pdata;
	struct i2c_client *client = info->client;
	struct tsp_raw_data *raw_data = info->raw_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int x_num = info->cap_info.x_node_num, y_num = info->cap_info.y_node_num;
	int i, j, offset;
	char mis_cal_data = 0xF0;
	int ret = 0;
	u16 chip_eeprom_info;
	int min = 0xFFFF, max = -0xFF;

	zt_ts_esd_timer_stop(info);
	disable_irq(info->irq);
	sec_cmd_set_default_result(sec);

	if (pdata->mis_cal_check == 0) {
		input_info(true, &info->client->dev, "%s: [ERROR] not support\n", __func__);
		mis_cal_data = 0xF1;
		goto NG;
	}

	if (info->work_state == SUSPEND) {
		input_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",__func__);
		mis_cal_data = 0xF2;
		goto NG;
	}

	if (read_data(info->client, ZT_EEPROM_INFO, (u8 *)&chip_eeprom_info, 2) < 0) {
		input_info(true, &info->client->dev, "%s: read eeprom_info i2c fail!\n", __func__);
		mis_cal_data = 0xF3;
		goto NG;
	}

	if (zinitix_bit_test(chip_eeprom_info, 0)) {
		input_info(true, &info->client->dev, "%s: eeprom cal 0, skip !\n", __func__);
		mis_cal_data = 0xF1;
		goto NG;
	}

	ret = ts_set_touchmode(TOUCH_REF_ABNORMAL_TEST_MODE);
	if (ret < 0) {
		ts_set_touchmode(TOUCH_POINT_MODE);
		mis_cal_data = 0xF4;
		goto NG;
	}
	ret = get_raw_data(info, (u8 *)raw_data->reference_data_abnormal, 2);
	if (!ret) {
		input_info(true, &info->client->dev, "%s:[ERROR] i2c fail!\n", __func__);
		ts_set_touchmode(TOUCH_POINT_MODE);
		mis_cal_data = 0xF4;
		goto NG;
	}
	ts_set_touchmode(TOUCH_POINT_MODE);

	input_info(true, &info->client->dev, "%s start\n", __func__);

	for (i = 0; i < y_num; i++) {
		for (j = 0; j < x_num; j++) {
			offset = (i * x_num) + j;

			if (raw_data->reference_data_abnormal[offset] < min)
				min = raw_data->reference_data_abnormal[offset];

			if (raw_data->reference_data_abnormal[offset] > max)
				max = raw_data->reference_data_abnormal[offset];
		}
	}

	mis_cal_data = 0x00;
	snprintf(buff, sizeof(buff), "%d,%d", min, max);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "MIS_CAL");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &client->dev, "%s: \"%s\"(%d)\n", __func__, sec->cmd_result,
			(int)strlen(sec->cmd_result));
	enable_irq(info->irq);
	zt_ts_esd_timer_start(info);

	zt_display_rawdata(info, raw_data, TOUCH_REF_ABNORMAL_TEST_MODE, 0);

	return;
NG:
	snprintf(buff, sizeof(buff), "%s_%d", "NG", mis_cal_data);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "MIS_CAL");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	input_info(true, &client->dev, "%s: \"%s\"(%d)\n", __func__, sec->cmd_result,
			(int)strlen(sec->cmd_result));
	enable_irq(info->irq);
	zt_ts_esd_timer_start(info);
	return;
}

static void run_mis_cal_read_all(void * device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct tsp_raw_data *raw_data = info->raw_data;
	char temp[SEC_CMD_STR_LEN] = { 0 };
	char *buff = NULL;
	int total_node = info->cap_info.x_node_num * info->cap_info.y_node_num;
	int i, j, offset;

	zt_ts_esd_timer_stop(info);
	disable_irq(info->irq);

	sec_cmd_set_default_result(sec);

	ts_set_touchmode(TOUCH_POINT_MODE);

	buff = kzalloc(total_node * CMD_RESULT_WORD_LEN, GFP_KERNEL);
	if (!buff)
		goto NG;

	for (i = 0; i < info->cap_info.y_node_num ; i++) {
		for (j = 0; j < info->cap_info.x_node_num ; j++) {
			offset = (i * info->cap_info.x_node_num) + j;
			snprintf(temp, CMD_RESULT_WORD_LEN, "%d,", raw_data->reference_data_abnormal[offset]);
			strncat(buff, temp, CMD_RESULT_WORD_LEN);
			memset(temp, 0x00, SEC_CMD_STR_LEN);
		}
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, total_node * CMD_RESULT_WORD_LEN));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	kfree(buff);

NG:
	if (sec->cmd_state != SEC_CMD_STATUS_OK) {
		snprintf(temp, SEC_CMD_STR_LEN, "NG");
		sec_cmd_set_cmd_result(sec, temp, SEC_CMD_STR_LEN);
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}
	enable_irq(info->irq);
	zt_ts_esd_timer_start(info);
}

static void run_jitter_test(void * device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	char tbuff[2] = { 0 };
	short jitter_max;
	int ret = 0;

	zt_ts_esd_timer_stop(info);
	disable_irq(info->irq);
	sec_cmd_set_default_result(sec);

	if (info->work_state == SUSPEND) {
		input_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",__func__);
		goto NG;
	}

	if (write_reg(client, ZT_JITTER_SAMPLING_CNT, 0x0064) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: failed SAMPLING_CNT\n", __func__);
		goto NG;
	}

	ret = ts_set_touchmode(TOUCH_JITTER_MODE);
	if (ret < 0) {
		ts_set_touchmode(TOUCH_POINT_MODE);
		goto NG;
	}

	zt_delay(1500);

	ret = read_data(client, ZT_JITTER_RESULT, tbuff, 2);
	if (ret < 0) {
		ts_set_touchmode(TOUCH_POINT_MODE);
		input_err(true, &client->dev,"%s: fail fw_minor_version\n", __func__);
		goto NG;
	}

	jitter_max = tbuff[1] << 8 | tbuff[0];

	ts_set_touchmode(TOUCH_POINT_MODE);

	snprintf(buff, sizeof(buff), "%d", jitter_max);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &client->dev, "%s: %s\n", __func__, buff);
	enable_irq(info->irq);
	zt_ts_esd_timer_start(info);

	return;
NG:
	snprintf(buff, sizeof(buff), "%s", "NG");

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	input_info(true, &client->dev, "%s: Failed\n", __func__);
	enable_irq(info->irq);
	zt_ts_esd_timer_start(info);
	return;
}


static void run_factory_miscalibration(void * device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct zt_ts_platform_data *pdata = info->pdata;
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	char tbuff[2] = { 0 };
	char mis_cal_data = 0xF0;
	int ret = 0;
	u16 chip_eeprom_info;

	zt_ts_esd_timer_stop(info);
	disable_irq(info->irq);
	sec_cmd_set_default_result(sec);

	if (pdata->mis_cal_check == 0) {
		input_info(true, &info->client->dev, "%s: [ERROR] not support\n", __func__);
		mis_cal_data = 0xF1;
		goto NG;
	}

	if (info->work_state == SUSPEND) {
		input_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",__func__);
		mis_cal_data = 0xF2;
		goto NG;
	}

	if (read_data(info->client, ZT_EEPROM_INFO, (u8 *)&chip_eeprom_info, 2) < 0) {
		input_info(true, &info->client->dev, "%s: read eeprom_info i2c fail!\n", __func__);
		mis_cal_data = 0xF3;
		goto NG;
	}

	if (zinitix_bit_test(chip_eeprom_info, 0)) {
		input_info(true, &info->client->dev, "%s: eeprom cal 0, skip !\n", __func__);
		mis_cal_data = 0xF1;
		goto NG;
	}

	ret = ts_set_touchmode(TOUCH_REF_ABNORMAL_TEST_MODE);
	if (ret < 0) {
		ts_set_touchmode(TOUCH_POINT_MODE);
		mis_cal_data = 0xF4;
		goto NG;
	}

	ret = write_cmd(client, ZT_MIS_CAL_SET);
	if (ret != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s:failed mis_cal_set\n", __func__);
		mis_cal_data = 0xF4;
		ts_set_touchmode(TOUCH_POINT_MODE);
		goto NG;
	}

	zt_delay(600);

	ret = read_data(client, ZT_MIS_CAL_RESULT, tbuff, 2);
	if (ret < 0) {
		input_err(true, &client->dev,"%s: fail fw_minor_version\n", __func__);
		mis_cal_data = 0xF4;
		ts_set_touchmode(TOUCH_POINT_MODE);
		goto NG;
	}

	ts_set_touchmode(TOUCH_POINT_MODE);

	input_info(true, &info->client->dev, "%s start\n", __func__);

	if (tbuff[1] == 0x55) {
		snprintf(buff, sizeof(buff), "OK,0,%d", tbuff[0]);
	} else if (tbuff[1] == 0xAA) {
		snprintf(buff, sizeof(buff), "NG,0,%d", tbuff[0]);
	} else {
		input_err(true, &client->dev,"%s: failed tbuff[1]:0x%x, tbuff[0]:0x%x \n",
				__func__, tbuff[1], tbuff[0]);
		ts_set_touchmode(TOUCH_POINT_MODE);
		goto NG;
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &client->dev, "%s: %s\n", __func__, buff);
	enable_irq(info->irq);
	zt_ts_esd_timer_start(info);

	return;
NG:
	snprintf(buff, sizeof(buff), "%s_%d", "NG", mis_cal_data);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	input_info(true, &client->dev, "%s: %s\n", __func__, buff);
	enable_irq(info->irq);
	zt_ts_esd_timer_start(info);
	return;
}

#ifdef TCLM_CONCEPT
static void get_pat_information(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	char buff[50] = { 0 };

	sec_cmd_set_default_result(sec);

	snprintf(buff, sizeof(buff), "C%02XT%04X.%4s%s%c%d%c%d%c%d",
			info->tdata->nvdata.cal_count, info->tdata->nvdata.tune_fix_ver, info->tdata->tclm_string[info->tdata->nvdata.cal_position].f_name,
			(info->tdata->tclm_level == TCLM_LEVEL_LOCKDOWN) ? ".L " : " ",
			info->tdata->cal_pos_hist_last3[0], info->tdata->cal_pos_hist_last3[1],
			info->tdata->cal_pos_hist_last3[2], info->tdata->cal_pos_hist_last3[3],
			info->tdata->cal_pos_hist_last3[4], info->tdata->cal_pos_hist_last3[5]);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

/* FACTORY TEST RESULT SAVING FUNCTION
 * bit 3 ~ 0 : OCTA Assy
 * bit 7 ~ 4 : OCTA module
 * param[0] : OCTA module(1) / OCTA Assy(2)
 * param[1] : TEST NONE(0) / TEST FAIL(1) / TEST PASS(2) : 2 bit
 */
static void get_tsp_test_result(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	char cbuff[SEC_CMD_STR_LEN] = { 0 };
	u8 buff[2] = {0};

	sec_cmd_set_default_result(sec);

	get_zt_tsp_nvm_data(info, ZT_TS_NVM_OFFSET_FAC_RESULT, (u8 *)buff, 2);
	info->test_result.data[0] = buff[0];

	input_info(true, &info->client->dev, "%s : %X", __func__, info->test_result.data[0]);

	if (info->test_result.data[0] == 0xFF) {
		input_info(true, &info->client->dev, "%s: clear factory_result as zero\n", __func__);
		info->test_result.data[0] = 0;
	}

	snprintf(cbuff, sizeof(cbuff), "M:%s, M:%d, A:%s, A:%d",
			info->test_result.module_result == 0 ? "NONE" :
			info->test_result.module_result == 1 ? "FAIL" : "PASS",
			info->test_result.module_count,
			info->test_result.assy_result == 0 ? "NONE" :
			info->test_result.assy_result == 1 ? "FAIL" : "PASS",
			info->test_result.assy_count);

	sec_cmd_set_cmd_result(sec, cbuff, strnlen(cbuff, sizeof(cbuff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
}

static void set_tsp_test_result(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	char cbuff[SEC_CMD_STR_LEN] = { 0 };
	u8 buff[2] = {0};

	sec_cmd_set_default_result(sec);

	get_zt_tsp_nvm_data(info, ZT_TS_NVM_OFFSET_FAC_RESULT, (u8 *)buff, 2);
	info->test_result.data[0] = buff[0];

	input_info(true, &info->client->dev, "%s : %X", __func__, info->test_result.data[0]);

	if (info->test_result.data[0] == 0xFF) {
		input_info(true, &info->client->dev, "%s: clear factory_result as zero\n", __func__);
		info->test_result.data[0] = 0;
	}

	if (sec->cmd_param[0] == TEST_OCTA_ASSAY) {
		info->test_result.assy_result = sec->cmd_param[1];
		if (info->test_result.assy_count < 3)
			info->test_result.assy_count++;

	} else if (sec->cmd_param[0] == TEST_OCTA_MODULE) {
		info->test_result.module_result = sec->cmd_param[1];
		if (info->test_result.module_count < 3)
			info->test_result.module_count++;
	}

	input_info(true, &info->client->dev, "%s: [0x%X] M:%s, M:%d, A:%s, A:%d\n",
			__func__, info->test_result.data[0],
			info->test_result.module_result == 0 ? "NONE" :
			info->test_result.module_result == 1 ? "FAIL" : "PASS",
			info->test_result.module_count,
			info->test_result.assy_result == 0 ? "NONE" :
			info->test_result.assy_result == 1 ? "FAIL" : "PASS",
			info->test_result.assy_count);

	set_zt_tsp_nvm_data(info, ZT_TS_NVM_OFFSET_FAC_RESULT, &info->test_result.data[0], 1);

	snprintf(cbuff, sizeof(cbuff), "OK");
	sec_cmd_set_cmd_result(sec, cbuff, strnlen(cbuff, sizeof(cbuff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
}

static void increase_disassemble_count(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u8 count[2] = { 0 };

	sec_cmd_set_default_result(sec);

	if (info->tsp_pwr_enabled == POWER_OFF) {
		input_err(true, &info->client->dev, "%s: IC is power off\n", __func__);
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	get_zt_tsp_nvm_data(info, ZT_TS_NVM_OFFSET_DISASSEMBLE_COUNT, count, 2);
	input_info(true, &info->client->dev, "%s: current disassemble count: %d\n", __func__, count[0]);

	if (count[0] == 0xFF)
		count[0] = 0;
	if (count[0] < 0xFE)
		count[0]++;

	set_zt_tsp_nvm_data(info, ZT_TS_NVM_OFFSET_DISASSEMBLE_COUNT, count , 2);

	zt_delay(5);

	memset(count, 0x00, 2);
	get_zt_tsp_nvm_data(info, ZT_TS_NVM_OFFSET_DISASSEMBLE_COUNT, count, 2);
	input_info(true, &info->client->dev, "%s: check disassemble count: %d\n", __func__, count[0]);

	snprintf(buff, sizeof(buff), "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

}

static void get_disassemble_count(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u8 count[2] = { 0 };

	sec_cmd_set_default_result(sec);

	if (info->tsp_pwr_enabled == POWER_OFF) {
		input_err(true, &info->client->dev, "%s: IC is power off\n", __func__);
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	get_zt_tsp_nvm_data(info, ZT_TS_NVM_OFFSET_DISASSEMBLE_COUNT, count, 2);
	if (count[0] == 0xFF) {
		count[0] = 0;
		count[1] = 0;
		set_zt_tsp_nvm_data(info, ZT_TS_NVM_OFFSET_DISASSEMBLE_COUNT, count , 2);
	}

	input_info(true, &info->client->dev, "%s: read disassemble count: %d\n", __func__, count[0]);
	snprintf(buff, sizeof(buff), "%d", count[0]);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
}
#endif

/*
 *	index  0 :  set edge handler
 *		1 :  portrait (normal) mode
 *		2 :  landscape mode
 *
 *	data
 *		0, X (direction), X (y start), X (y end)
 *		direction : 0 (off), 1 (left), 2 (right)
 *			ex) echo set_grip_data,0,2,600,900 > cmd
 *
 *		1, X (edge zone), X (dead zone up x), X (dead zone down x), X (dead zone y)
 *			ex) echo set_grip_data,1,200,10,50,1500 > cmd
 *
 *		2, 1 (landscape mode), X (edge zone), X (dead zone x), X (dead zone top y), X (dead zone bottom y), X (edge zone top y), X (edge zone bottom y)
 *			ex) echo set_grip_data,2,1,200,100,120,0 > cmd
 *
 *		2, 0 (portrait mode)
 *			ex) echo set_grip_data,2,0  > cmd
 */

static void set_grip_data(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *ts = container_of(sec, struct zt_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u8 mode = G_NONE;

	sec_cmd_set_default_result(sec);

	memset(buff, 0, sizeof(buff));

	if (sec->cmd_param[0] == 0) {	// edge handler
		if (sec->cmd_param[1] == 0) {	// clear
			ts->grip_edgehandler_direction = 0;
		} else if (sec->cmd_param[1] < 3) {
			ts->grip_edgehandler_direction = sec->cmd_param[1];
			ts->grip_edgehandler_start_y = sec->cmd_param[2];
			ts->grip_edgehandler_end_y = sec->cmd_param[3];
		} else {
			input_err(true, &ts->client->dev, "%s: cmd1 is abnormal, %d (%d)\n",
					__func__, sec->cmd_param[1], __LINE__);
			goto err_grip_data;
		}

		mode = mode | G_SET_EDGE_HANDLER;
		set_grip_data_to_ic(ts, mode);
	} else if (sec->cmd_param[0] == 1) {	// normal mode
		if (ts->grip_edge_range != sec->cmd_param[1])
			mode = mode | G_SET_EDGE_ZONE;

		ts->grip_edge_range = sec->cmd_param[1];
		ts->grip_deadzone_up_x = sec->cmd_param[2];
		ts->grip_deadzone_dn_x = sec->cmd_param[3];
		ts->grip_deadzone_y = sec->cmd_param[4];
		mode = mode | G_SET_NORMAL_MODE;

		if (ts->grip_landscape_mode == 1) {
			ts->grip_landscape_mode = 0;
			mode = mode | G_CLR_LANDSCAPE_MODE;
		}
		set_grip_data_to_ic(ts, mode);
	} else if (sec->cmd_param[0] == 2) {	// landscape mode
		if (sec->cmd_param[1] == 0) {	// normal mode
			ts->grip_landscape_mode = 0;
			mode = mode | G_CLR_LANDSCAPE_MODE;
		} else if (sec->cmd_param[1] == 1) {
			ts->grip_landscape_mode = 1;
			ts->grip_landscape_edge = sec->cmd_param[2];
			ts->grip_landscape_deadzone	= sec->cmd_param[3];
			ts->grip_landscape_top_deadzone = sec->cmd_param[4];
			ts->grip_landscape_bottom_deadzone = sec->cmd_param[5];
			ts->grip_landscape_top_gripzone = sec->cmd_param[6];
			ts->grip_landscape_bottom_gripzone = sec->cmd_param[7];
			mode = mode | G_SET_LANDSCAPE_MODE;
		} else {
			input_err(true, &ts->client->dev, "%s: cmd1 is abnormal, %d (%d)\n",
					__func__, sec->cmd_param[1], __LINE__);
			goto err_grip_data;
		}
		set_grip_data_to_ic(ts, mode);
	} else {
		input_err(true, &ts->client->dev, "%s: cmd0 is abnormal, %d", __func__, sec->cmd_param[0]);
		goto err_grip_data;
	}

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;

err_grip_data:

	snprintf(buff, sizeof(buff), "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void set_touchable_area(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	input_info(true, &info->client->dev,
			"%s: set 16:9 mode %s\n", __func__, sec->cmd_param[0] ? "enable" : "disable");

	zt_set_optional_mode(info, DEF_OPTIONAL_MODE_TOUCHABLE_AREA, sec->cmd_param[0]);

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
out:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
}

static void clear_cover_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int arg = sec->cmd_param[0];

	sec_cmd_set_default_result(sec);
	snprintf(buff, sizeof(buff), "%u", (unsigned int) arg);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 3) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		if (sec->cmd_param[0] > 1) {
			info->flip_enable = true;
			info->cover_type = sec->cmd_param[1];
#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
			zt_delay(100);
			if (TRUSTEDUI_MODE_TUI_SESSION & trustedui_get_current_mode()) {
				tui_force_close(1);
				zt_delay(100);
				if (TRUSTEDUI_MODE_TUI_SESSION & trustedui_get_current_mode()) {
					trustedui_clear_mask(TRUSTEDUI_MODE_VIDEO_SECURED|TRUSTEDUI_MODE_INPUT_SECURED);
					trustedui_set_mode(TRUSTEDUI_MODE_OFF);
				}
			}
#endif // CONFIG_TRUSTONIC_TRUSTED_UI
#ifdef CONFIG_SAMSUNG_TUI
			stui_cancel_session();
#endif
		} else {
			info->flip_enable = false;
		}

		set_cover_type(info, info->flip_enable);

		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	return;
}

static void clear_reference_data(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	zt_ts_esd_timer_stop(info);

	write_reg(client, ZT_EEPROM_INFO, 0xffff);

	write_reg(client, VCMD_NVM_WRITE_ENABLE, 0x0001);
	usleep_range(100, 100);
	if (write_cmd(client, ZT_SAVE_STATUS_CMD) != I2C_SUCCESS)
		return;

	zt_delay(500);
	write_reg(client, VCMD_NVM_WRITE_ENABLE, 0x0000);
	usleep_range(100, 100);

	zt_ts_esd_timer_start(info);
	input_info(true, &client->dev, "%s: TSP clear calibration bit\n", __func__);

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s: %s(%d)\n", __func__,
			sec->cmd_result, (int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));
	return;
}

static void run_cs_raw_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int retry = 0;
	char *all_cmdbuff;
	int i, j;
	int buff_size = 0;

	sec_cmd_set_default_result(sec);

	disable_irq(info->irq);

	ts_enter_strength_mode(info, TOUCH_RAW_MODE);

	while (gpio_get_value(info->pdata->gpio_int)) {
		zt_delay(30);

		retry++;

		input_info(true, &client->dev, "%s: retry:%d\n", __func__, retry);

		if (retry > 100) {
			enable_irq(info->irq);
			goto out;
		}
	}
	ts_get_raw_data(info);

	ts_exit_strength_mode(info);

	enable_irq(info->irq);

	ts_get_strength_data(info);

	buff_size = info->cap_info.x_node_num * info->cap_info.y_node_num * 6;
	all_cmdbuff = kzalloc(buff_size, GFP_KERNEL);
	if (!all_cmdbuff) {
		input_info(true, &info->client->dev, "%s: alloc failed\n", __func__);
		goto out;
	}

	for (i = 0; i < info->cap_info.y_node_num; i++) {
		for (j = 0; j < info->cap_info.x_node_num; j++) {
			sprintf(buff, "%d,", info->cur_data[i * info->cap_info.x_node_num + j]);
			strcat(all_cmdbuff, buff);
		}
	}

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, all_cmdbuff,
			strnlen(all_cmdbuff, buff_size));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	kfree(all_cmdbuff);

	input_info(true, &client->dev, "%s: %s(%d)\n", __func__,
			sec->cmd_result, (int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));
	return;

out:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	input_info(true, &client->dev, "%s: %s(%d)\n", __func__,
			sec->cmd_result, (int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));

}

static void run_cs_delta_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int retry = 0;
	char *all_cmdbuff;
	int i, j;
	int buff_size = 0;

	sec_cmd_set_default_result(sec);

	zt_ts_esd_timer_stop(info);
	disable_irq(info->irq);

	ts_enter_strength_mode(info, TOUCH_DELTA_MODE);

	while (gpio_get_value(info->pdata->gpio_int)) {
		zt_delay(30);

		retry++;

		input_info(true, &client->dev, "%s: retry:%d\n", __func__, retry);

		if (retry > 100) {
			enable_irq(info->irq);
			zt_ts_esd_timer_start(info);
			goto out;
		}
	}
	ts_get_raw_data(info);

	ts_exit_strength_mode(info);

	enable_irq(info->irq);

	zt_ts_esd_timer_start(info);

	ts_get_strength_data(info);

	buff_size = info->cap_info.x_node_num * info->cap_info.y_node_num * 6;
	all_cmdbuff = kzalloc(buff_size, GFP_KERNEL);
	if (!all_cmdbuff) {
		input_info(true, &info->client->dev, "%s: alloc failed\n", __func__);
		goto out;
	}

	for (i = 0; i < info->cap_info.y_node_num; i++) {
		for (j = 0; j < info->cap_info.x_node_num; j++) {
			sprintf(buff, "%d,", info->cur_data[i * info->cap_info.x_node_num + j]);
			strcat(all_cmdbuff, buff);
		}
	}

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, all_cmdbuff,
			strnlen(all_cmdbuff, buff_size));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	kfree(all_cmdbuff);

	input_info(true, &client->dev, "%s: %s(%d)\n", __func__,
			sec->cmd_result, (int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));
	return;

out:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	input_info(true, &client->dev, "%s: %s(%d)\n", __func__,
			sec->cmd_result, (int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));
}

static void run_ref_calibration(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int i;
#ifdef TCLM_CONCEPT
	int ret;
#endif
	sec_cmd_set_default_result(sec);

	if (info->finger_cnt1 != 0) {
		input_info(true, &client->dev, "%s: return (finger cnt %d)\n", __func__, info->finger_cnt1);
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	disable_irq(info->irq);

	zt_ts_esd_timer_stop(info);
	zt_power_control(info, POWER_OFF);
	zt_power_control(info, POWER_ON_SEQUENCE);

	if (ts_hw_calibration(info) == true) {
#ifdef TCLM_CONCEPT
		/* devide tclm case */
		sec_tclm_case(info->tdata, sec->cmd_param[0]);

		input_info(true, &info->client->dev, "%s: param, %d, %c, %d\n", __func__,
				sec->cmd_param[0], sec->cmd_param[0], info->tdata->root_of_calibration);

		ret = sec_execute_tclm_package(info->tdata, 1);
		if (ret < 0) {
			input_err(true, &info->client->dev,
					"%s: sec_execute_tclm_package\n", __func__);
		}
		sec_tclm_root_of_cal(info->tdata, CALPOSITION_NONE);
#endif
		input_info(true, &client->dev, "%s: TSP calibration Pass\n", __func__);
		snprintf(buff, sizeof(buff), "%s", "OK");
		sec_cmd_set_cmd_result(sec, buff, (int)strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_OK;
	} else {
		input_info(true, &client->dev, "%s: TSP calibration Fail\n", __func__);
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buff, (int)strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}

	zt_power_control(info, POWER_OFF);
	zt_power_control(info, POWER_ON_SEQUENCE);
	mini_init_touch(info);

	for (i = 0; i < 5; i++) {
		write_cmd(client, ZT_CLEAR_INT_STATUS_CMD);
		usleep_range(10, 10);
	}

	clear_report_data(info);

	zt_ts_esd_timer_start(info);
	enable_irq(info->irq);
	input_info(true, &client->dev, "%s: %s(%d)\n", __func__,
			sec->cmd_result, (int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));
	return;
}

static void run_amp_check_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	struct tsp_raw_data *raw_data = info->raw_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int total_node = info->cap_info.y_node_num;
	u16 min, max;
	int i;
	int ret;

	zt_ts_esd_timer_stop(info);
	sec_cmd_set_default_result(sec);

	ret = ts_set_touchmode(TOUCH_AMP_CHECK_MODE);
	if (ret < 0) {
		ts_set_touchmode(TOUCH_POINT_MODE);
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	get_raw_data(info, (u8 *)raw_data->amp_check_data, 1);
	ts_set_touchmode(TOUCH_POINT_MODE);

	min = 0xFFFF;
	max = 0x0000;

	for (i = 0; i < total_node; i++) {
		if (raw_data->amp_check_data[i] < min)
			min = raw_data->amp_check_data[i];

		if (raw_data->amp_check_data[i] > max)
			max = raw_data->amp_check_data[i];
	}

	input_info(true, &client->dev, "%s: amp check data min:%d, max:%d\n", __func__, min, max);

	snprintf(buff, sizeof(buff), "%d,%d", min, max);
	sec->cmd_state = SEC_CMD_STATUS_OK;

out:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "AMP_CHECK");
	}

	input_info(true, &client->dev, "%s: %s(%d)\n", __func__, sec->cmd_result,
			(int)strlen(sec->cmd_result));

	zt_ts_esd_timer_start(info);
	return;
}

static void dead_zone_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	zt_set_optional_mode(info, DEF_OPTIONAL_MODE_EDGE_SELECT, !sec->cmd_param[0]);

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s, %s\n", __func__, sec->cmd_result);

	return;
}

static void spay_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	info->spay_enable = !!sec->cmd_param[0];

	if (info->pdata->use_sponge)
		zt_set_lp_mode(info, ZT_SPONGE_MODE_SPAY, info->spay_enable);
	else
		zt_set_lp_mode(info, BIT_EVENT_SPAY, info->spay_enable);

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s, %s\n", __func__, sec->cmd_result);

	return;
}

static void fod_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;
	u16 val = (u16)sec->cmd_param[0];

	sec_cmd_set_default_result(sec);

	input_info(true, &info->client->dev, "%s: fod_enable:%d, short_mode:%d, strict mode:%d\n",
			__func__, sec->cmd_param[0], sec->cmd_param[1],	sec->cmd_param[2]);

	if (info->pdata->use_sponge) {
		info->fod_enable = !!sec->cmd_param[0];
		info->fod_mode_set = (sec->cmd_param[1] & 0x01) | ((sec->cmd_param[2] & 0x01) << 1);

		zt_set_lp_mode(info, ZT_SPONGE_MODE_PRESS, info->fod_enable);
		ret = ts_write_to_sponge(info, ZT_SPONGE_FOD_PROPERTY, (u8 *)&info->fod_mode_set, 2);
		if (ret < 0)
			input_err(true, &info->client->dev, "%s: Failed to write sponge\n", __func__);

		input_info(true, &info->client->dev, "%s: %s, fast:%s, strict:%s, %02X\n",
				__func__, sec->cmd_param[0] ? "on" : "off",
				info->fod_mode_set & 1 ? "on" : "off",
				info->fod_mode_set & 2 ? "on" : "off",
				info->lpm_mode);
	} else {
		info->fod_mode_set = (u16)(((sec->cmd_param[2] << 8) & 0xFF00) |
				((sec->cmd_param[1] << 4) & 0x00F0) | (sec->cmd_param[0] & 0x000F));

		if (val) {
			info->fod_enable = 1;
		} else {
			info->fod_enable = 0;
		}

		if (write_reg(client, REG_FOD_MODE_SET, info->fod_mode_set) != I2C_SUCCESS)
			input_info(true, &client->dev, "%s, fail fod mode set\n", __func__);
	}

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s, %s\n", __func__, sec->cmd_result);

	return;
}

static void fod_lp_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int val = sec->cmd_param[0];

	sec_cmd_set_default_result(sec);

	if (val) {
		info->fod_lp_mode = 1;
	} else {
		info->fod_lp_mode = 0;
	}

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s, %s\n", __func__, sec->cmd_result);

	return;
}

static void singletap_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	info->singletap_enable = !!sec->cmd_param[0];

	if (info->pdata->use_sponge)
		zt_set_lp_mode(info, ZT_SPONGE_MODE_SINGLETAP, info->singletap_enable);
	else
		zt_set_lp_mode(info, BIT_EVENT_SINGLE_TAP, info->singletap_enable);

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s, %s\n", __func__, sec->cmd_result);

	return;
}

static void aot_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	info->aot_enable = !!sec->cmd_param[0];

	if (info->pdata->use_sponge)
		zt_set_lp_mode(info, ZT_SPONGE_MODE_DOUBLETAP_WAKEUP, info->aot_enable);
	else
		zt_set_lp_mode(info, BIT_EVENT_AOT, info->aot_enable);
	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s, %s\n", __func__, sec->cmd_result);

	return;
}

static void aod_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	info->aod_enable = !!sec->cmd_param[0];

	if (info->pdata->use_sponge)
		zt_set_lp_mode(info, ZT_SPONGE_MODE_AOD, info->aod_enable);
	else
		zt_set_lp_mode(info, BIT_EVENT_AOD, info->aod_enable);

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s, %s\n", __func__, sec->cmd_result);

	return;
}

static void set_fod_rect(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int i;

	sec_cmd_set_default_result(sec);

	input_info(true, &info->client->dev, "%s: start x:%d, start y:%d, end x:%d, end y:%d\n",
			__func__, sec->cmd_param[0], sec->cmd_param[1],
			sec->cmd_param[2], sec->cmd_param[3]);

	for (i = 0; i < 4; i++)
		info->fod_rect[i] = sec->cmd_param[i] & 0xFFFF;

	zt_set_fod_rect(info);

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s, %s\n", __func__, sec->cmd_result);

	return;
}

static void set_aod_rect(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int i;
	int ret;

	sec_cmd_set_default_result(sec);

	input_info(true, &info->client->dev, "%s: w:%d, h:%d, x:%d, y:%d\n",
			__func__, sec->cmd_param[0], sec->cmd_param[1],
			sec->cmd_param[2], sec->cmd_param[3]);

	if (info->pdata->use_sponge) {
		for (i = 0; i < 4; i++)
			info->aod_rect[i] = sec->cmd_param[i];

		ret = zt_set_aod_rect(info);
		if (ret < 0) {
			input_err(true, &info->client->dev, "%s: failed. ret: %d\n", __func__, ret);
			goto error;
		}
	} else {
		write_reg(info->client, ZT_SET_AOD_W_REG, (u16)sec->cmd_param[0]);
		write_reg(info->client, ZT_SET_AOD_H_REG, (u16)sec->cmd_param[1]);
		write_reg(info->client, ZT_SET_AOD_X_REG, (u16)sec->cmd_param[2]);
		write_reg(info->client, ZT_SET_AOD_Y_REG, (u16)sec->cmd_param[3]);
	}

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s, %s\n", __func__, sec->cmd_result);

	return;

error:
	snprintf(buff, sizeof(buff), "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);		
}

static void get_wet_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u16 temp;

	sec_cmd_set_default_result(sec);

	mutex_lock(&info->work_lock);
	read_data(client, ZT_DEBUG_REG, (u8 *)&temp, 2);
	mutex_unlock(&info->work_lock);

	input_info(true, &client->dev, "%s, %x\n", __func__, temp);

	if (zinitix_bit_test(temp, DEF_DEVICE_STATUS_WATER_MODE))
		temp = true;
	else
		temp = false;

	snprintf(buff, sizeof(buff), "%u", temp);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "WET_MODE");
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &client->dev, "%s: %s(%d)\n", __func__, sec->cmd_result,
			(int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));

	return;
}

static void glove_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		zt_set_optional_mode(info, DEF_OPTIONAL_MODE_SENSITIVE_BIT, sec->cmd_param[0]);
		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &client->dev, "%s: %s(%d)\n", __func__, sec->cmd_result,
			(int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));

	return;
}

static void pocket_mode_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		info->pocket_enable = sec->cmd_param[0];

		zt_set_optional_mode(info, DEF_OPTIONAL_MODE_POCKET_MODE, info->pocket_enable);

		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &client->dev, "%s, %s\n", __func__, sec->cmd_result);

	return;
}

static void get_crc_check(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u16 chip_check_sum = 0;

	sec_cmd_set_default_result(sec);

	if (read_data(info->client, ZT_CHECKSUM_RESULT,
				(u8 *)&chip_check_sum, 2) < 0) {
		input_err(true, &info->client->dev, "%s: read crc fail", __func__);
		goto err_get_crc_check;
	}

	input_info(true, &info->client->dev, "%s: 0x%04X\n", __func__, chip_check_sum);

	if (chip_check_sum != CORRECT_CHECK_SUM)
		goto err_get_crc_check;

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
	return;

err_get_crc_check:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
	return;
}

static void run_test_vsync(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	char buf[64] = { 0 };
	u16 data = 0;

	sec_cmd_set_default_result(sec);

	if (write_reg(info->client, ZT_POWER_STATE_FLAG, 1) != I2C_SUCCESS) {
		input_err(true, &info->client->dev, "%s: Fail to set ZT_POWER_STATE_FLAG 1\n", __func__);
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto EXIT;
	}

	zt_delay(100);
	if (read_data(info->client, ZT_VSYNC_TEST_RESULT, (u8 *)&data, 2) < 0) {
		input_err(true, &info->client->dev, "%s: read crc fail", __func__);
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto EXIT;
	}

	input_info(true, &info->client->dev, "%s: result %d\n", __func__, data);

	snprintf(buf, sizeof(buf), "%d", data);
	sec->cmd_state = SEC_CMD_STATUS_OK;

EXIT:
	if (write_reg(info->client, ZT_POWER_STATE_FLAG, 0) != I2C_SUCCESS)
		input_err(true, &info->client->dev, "%s: Fail to set ZT_POWER_STATE_FLAG 0\n", __func__);

	zt_delay(10);
	
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "VSYNC");
}

static void read_osc_value(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;
	u16 data[2] = {0};
	u32 osc_timer_val;

	sec_cmd_set_default_result(sec);

	ret = read_data(info->client, ZT_OSC_TIMER_LSB, (u8 *)&data[0], 2);
	if (ret < 0) {
		input_err(true, &info->client->dev,"%s: fail read proximity threshold\n", __func__);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto ERROR;
	}

	ret = read_data(info->client, ZT_OSC_TIMER_MSB, (u8 *)&data[1], 2);
	if (ret < 0) {
		input_err(true, &info->client->dev,"%s: fail read proximity threshold\n", __func__);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto ERROR;
	}

	osc_timer_val = (data[1] << 16) | data[0];

	input_info(true, &info->client->dev,
					"%s: osc_timer_value %08X\n", __func__, osc_timer_val);

	snprintf(buff, sizeof(buff), "%u,%u", osc_timer_val, osc_timer_val);
	sec->cmd_state = SEC_CMD_STATUS_OK;

ERROR:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "OSC_DATA");
	return;
}

static void factory_cmd_result_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;

	sec->item_count = 0;
	memset(sec->cmd_result_all, 0x00, SEC_CMD_RESULT_STR_LEN);

	if (info->tsp_pwr_enabled == POWER_OFF) {
		input_err(true, &info->client->dev, "%s: IC is power off\n", __func__);
		sec->cmd_all_factory_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	sec->cmd_all_factory_state = SEC_CMD_STATUS_RUNNING;

	get_chip_vendor(sec);
	get_chip_name(sec);
	get_fw_ver_bin(sec);
	get_fw_ver_ic(sec);

	run_dnd_read(sec);
	run_dnd_v_gap_read(sec);
	run_dnd_h_gap_read(sec);
	run_cnd_read(sec);
	run_selfdnd_read(sec);
	run_selfdnd_h_gap_read(sec);
	run_self_saturation_read(sec);
	run_charge_pump_read(sec);
	run_test_vsync(sec);
	read_osc_value(sec);
	run_amp_check_read(sec);

#ifdef TCLM_CONCEPT
	run_mis_cal_read(sec);
#endif

	/* SW RESET after selftest */
	mutex_lock(&info->work_lock);
	zt_ts_esd_timer_stop(info);
	clear_report_data(info);
	mini_init_touch(info);
	mutex_unlock(&info->work_lock);

	sec->cmd_all_factory_state = SEC_CMD_STATUS_OK;

out:
	input_info(true, &client->dev, "%s: %d%s\n", __func__, sec->item_count,
			sec->cmd_result_all);
}

static void check_connection(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[3] = { 0 };
	u8 conn_check_val;
	int ret;

	sec_cmd_set_default_result(sec);

	ret = read_data(client, ZT_CONNECTION_CHECK_REG, (u8 *)&conn_check_val, 1);
	if (ret < 0) {
		input_err(true, &info->client->dev,"%s: fail read TSP connection value\n", __func__);
		goto err_conn_check;
	}

	if (conn_check_val != 1)
		goto err_conn_check;

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
	return;

err_conn_check:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
	return;
}

static void run_prox_intensity_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u16 prox_x_data, prox_y_data;
	u16 threshold;
	int ret;

	sec_cmd_set_default_result(sec);

	zt_ts_esd_timer_stop(info);
	disable_irq(info->irq);
	mutex_lock(&info->work_lock);

	ret = read_data(client, ZT_PROXIMITY_XDATA, (u8 *)&prox_x_data, 2);
	if (ret < 0) {
		input_err(true, &info->client->dev,"%s: fail read proximity x data\n", __func__);
		goto READ_FAIL;
	}

	ret = read_data(client, ZT_PROXIMITY_YDATA, (u8 *)&prox_y_data, 2);
	if (ret < 0) {
		input_err(true, &info->client->dev,"%s: fail read proximity y data\n", __func__);
		goto READ_FAIL;
	}

	ret = read_data(client, ZT_PROXIMITY_THRESHOLD, (u8 *)&threshold, 2);
	if (ret < 0) {
		input_err(true, &info->client->dev,"%s: fail read proximity threshold\n", __func__);
		goto READ_FAIL;
	}

	mutex_unlock(&info->work_lock);
	enable_irq(info->irq);
	zt_ts_esd_timer_start(info);

	snprintf(buff, sizeof(buff), "SUM_X:%d SUM_Y:%d THD_X:%d THD_Y:%d", prox_x_data, prox_y_data, threshold, threshold);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
	return;

READ_FAIL:
	mutex_unlock(&info->work_lock);
	enable_irq(info->irq);
	zt_ts_esd_timer_start(info);

	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
	return;
}

static void ear_detect_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] != 0 && sec->cmd_param[0] != 1 && sec->cmd_param[0] != 3) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		info->ed_enable = sec->cmd_param[0];

		if (info->ed_enable == 3) {
			zt_set_optional_mode(info, DEF_OPTIONAL_MODE_EAR_DETECT, true);
			zt_set_optional_mode(info, DEF_OPTIONAL_MODE_EAR_DETECT_MUTUAL, false);
		} else if (info->ed_enable == 1) {
			zt_set_optional_mode(info, DEF_OPTIONAL_MODE_EAR_DETECT, false);
			zt_set_optional_mode(info, DEF_OPTIONAL_MODE_EAR_DETECT_MUTUAL, true);
		} else {
			zt_set_optional_mode(info, DEF_OPTIONAL_MODE_EAR_DETECT, false);
			zt_set_optional_mode(info, DEF_OPTIONAL_MODE_EAR_DETECT_MUTUAL, false);
		}

		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &client->dev, "%s, %s\n", __func__, sec->cmd_result);

	return;
}

/*	for game mode 
	byte[0]: Setting for the Game Mode with 240Hz scan rate
		- 0: Disable
		- 1: Enable

	byte[1]: Vsycn mode
		- 0: Normal 60
		- 1: HS60
		- 2: HS120
		- 3: VSYNC 48
		- 4: VSYNC 96 
*/
static void set_scan_rate(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	char tBuff[2] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1 ||
			sec->cmd_param[1] < 0 || sec->cmd_param[1] > 4) {
		input_err(true, &info->client->dev, "%s: not support param\n", __func__);
		goto NG;
	}

	tBuff[0] = sec->cmd_param[0];
	tBuff[1] = sec->cmd_param[1];

	if (write_reg(info->client, ZT_SET_SCANRATE_ENABLE, tBuff[0]) != I2C_SUCCESS) {
		input_err(true, &info->client->dev,
				"%s: failed to set scan mode enable\n", __func__);
		goto NG;
	}

	if (write_reg(info->client, ZT_SET_SCANRATE, tBuff[1]) != I2C_SUCCESS) {
		input_err(true, &info->client->dev,
				"%s: failed to set scan rate\n", __func__);
		goto NG;
	}

	input_info(true, &info->client->dev,
					"%s: set scan rate %d %d\n", __func__, tBuff[0], tBuff[1]);

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;

NG:
	snprintf(buff, sizeof(buff), "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void set_game_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	char tBuff[2] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		input_err(true, &info->client->dev, "%s: not support param\n", __func__);
		goto NG;
	}

	tBuff[0] = sec->cmd_param[0];

	if (write_reg(info->client, ZT_SET_GAME_MODE, tBuff[0]) != I2C_SUCCESS) {
		input_err(true, &info->client->dev,
				"%s: failed to set scan mode enable\n", __func__);
		goto NG;
	}

	input_info(true, &info->client->dev,
			"%s: set game mode %d\n", __func__, tBuff[0]);

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;

NG:
	snprintf(buff, sizeof(buff), "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void set_sip_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		write_reg(client, ZT_SET_SIP_MODE, (u8)sec->cmd_param[0]);
		snprintf(buff, sizeof(buff), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &client->dev, "%s: %s(%d)\n", __func__, sec->cmd_result,
			(int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));

	return;
}

static void not_support_cmd(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	snprintf(buff, sizeof(buff), "%s", "NA");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;

	sec_cmd_set_cmd_exit(sec);

	input_info(true, &client->dev, "%s: \"%s(%d)\"\n", __func__, sec->cmd_result,
			(int)strnlen(sec->cmd_result, sizeof(sec->cmd_result)));

	return;
}

static struct sec_cmd sec_cmds[] = {
	{SEC_CMD("fw_update", fw_update),},
	{SEC_CMD("get_fw_ver_bin", get_fw_ver_bin),},
	{SEC_CMD("get_fw_ver_ic", get_fw_ver_ic),},
	{SEC_CMD("get_checksum_data", get_checksum_data),},
	{SEC_CMD("get_threshold", get_threshold),},
	{SEC_CMD("get_module_vendor", get_module_vendor),},
	{SEC_CMD("get_chip_vendor", get_chip_vendor),},
	{SEC_CMD("get_chip_name", get_chip_name),},
	{SEC_CMD("get_x_num", get_x_num),},
	{SEC_CMD("get_y_num", get_y_num),},

	/* vendor dependant command */
	{SEC_CMD("run_cnd_read", run_cnd_read),},
	{SEC_CMD("run_cnd_read_all", run_cnd_read_all),},
	{SEC_CMD("run_dnd_read", run_dnd_read),},
	{SEC_CMD("run_dnd_read_all", run_dnd_read_all),},
	{SEC_CMD("run_dnd_v_gap_read", run_dnd_v_gap_read),},
	{SEC_CMD("run_dnd_v_gap_read_all", run_dnd_v_gap_read_all),},
	{SEC_CMD("run_dnd_h_gap_read", run_dnd_h_gap_read),},
	{SEC_CMD("run_dnd_h_gap_read_all", run_dnd_h_gap_read_all),},
	{SEC_CMD("run_selfdnd_read", run_selfdnd_read),},
	{SEC_CMD("run_selfdnd_read_all", run_selfdnd_read_all),},
	{SEC_CMD("run_self_saturation_read", run_self_saturation_read),},	/* self_saturation_rx */
	{SEC_CMD("run_self_saturation_read_all", run_ssr_read_all),},
	{SEC_CMD("run_selfdnd_h_gap_read", run_selfdnd_h_gap_read),},
	{SEC_CMD("run_selfdnd_h_gap_read_all", run_selfdnd_h_gap_read_all),},
	{SEC_CMD("run_trx_short_test", run_trx_short_test),},
	{SEC_CMD("run_jitter_test", run_jitter_test),},
	{SEC_CMD("run_charge_pump_read", run_charge_pump_read),},
	{SEC_CMD("run_mis_cal_read", run_mis_cal_read),},
	{SEC_CMD("run_factory_miscalibration", run_factory_miscalibration),},
	{SEC_CMD("run_mis_cal_read_all", run_mis_cal_read_all),},
	{SEC_CMD_H("touch_aging_mode", touch_aging_mode),},
#ifdef TCLM_CONCEPT
	{SEC_CMD("get_pat_information", get_pat_information),},
	{SEC_CMD("get_tsp_test_result", get_tsp_test_result),},
	{SEC_CMD("set_tsp_test_result", set_tsp_test_result),},
	{SEC_CMD("increase_disassemble_count", increase_disassemble_count),},
	{SEC_CMD("get_disassemble_count", get_disassemble_count),},
#endif
	{SEC_CMD("run_cs_raw_read_all", run_cs_raw_read_all),},
	{SEC_CMD("run_cs_delta_read_all", run_cs_delta_read_all),},
	{SEC_CMD("run_force_calibration", run_ref_calibration),},
	{SEC_CMD("run_amp_check_read", run_amp_check_read),},
	{SEC_CMD("clear_reference_data", clear_reference_data),},
	{SEC_CMD("dead_zone_enable", dead_zone_enable),},
	{SEC_CMD("set_grip_data", set_grip_data),},
	{SEC_CMD_H("set_touchable_area", set_touchable_area),},
	{SEC_CMD_H("clear_cover_mode", clear_cover_mode),},
	{SEC_CMD_H("spay_enable", spay_enable),},
	{SEC_CMD("fod_enable", fod_enable),},
	{SEC_CMD_H("fod_lp_mode", fod_lp_mode),},
	{SEC_CMD("set_fod_rect", set_fod_rect),},
	{SEC_CMD_H("singletap_enable", singletap_enable),},
	{SEC_CMD_H("aot_enable", aot_enable),},
	{SEC_CMD_H("aod_enable", aod_enable),},
	{SEC_CMD("set_aod_rect", set_aod_rect),},
	{SEC_CMD("get_wet_mode", get_wet_mode),},
	{SEC_CMD_H("glove_mode", glove_mode),},
	{SEC_CMD_H("pocket_mode_enable", pocket_mode_enable),},
	{SEC_CMD("get_crc_check", get_crc_check),},
	{SEC_CMD("run_test_vsync", run_test_vsync),},	
	{SEC_CMD("factory_cmd_result_all", factory_cmd_result_all),},
	{SEC_CMD("check_connection", check_connection),},
	{SEC_CMD("run_prox_intensity_read_all", run_prox_intensity_read_all),},
	{SEC_CMD_H("ear_detect_enable", ear_detect_enable),},
	{SEC_CMD_H("set_scan_rate", set_scan_rate),},
	{SEC_CMD("read_osc_value", read_osc_value),},
	{SEC_CMD_H("set_game_mode", set_game_mode),},
	{SEC_CMD_H("set_sip_mode", set_sip_mode),},
	{SEC_CMD("not_support_cmd", not_support_cmd),},
};


static ssize_t scrub_position_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	input_info(true, &client->dev, "%s: scrub_id: %d, X:%d, Y:%d \n", __func__,
			info->scrub_id, info->scrub_x, info->scrub_y);

	snprintf(buff, sizeof(buff), "%d %d %d", info->scrub_id, info->scrub_x, info->scrub_y);

	info->scrub_id = 0;
	return snprintf(buf, SEC_CMD_BUF_SIZE, "%s", buff);
}

static ssize_t sensitivity_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	s16 i, value[TOUCH_SENTIVITY_MEASUREMENT_COUNT];
	char buff[SEC_CMD_STR_LEN] = { 0 };

	for (i = 0; i < TOUCH_SENTIVITY_MEASUREMENT_COUNT; i++) {
		value[i] = info->sensitivity_data[i];
	}

	input_info(true, &info->client->dev, "%s: sensitivity mode,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", __func__,
			value[0], value[1], value[2], value[3], value[4], value[5], value[6], value[7], value[8]);

	snprintf(buff, sizeof(buff),"%d,%d,%d,%d,%d,%d,%d,%d,%d",
			value[0], value[1], value[2], value[3], value[4], value[5], value[6], value[7], value[8]);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%s", buff);
}

static ssize_t sensitivity_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	int ret;
	unsigned long value = 0;

	if (count > 2)
		return -EINVAL;

	ret = kstrtoul(buf, 10, &value);
	if (ret != 0)
		return ret;

	if (info->tsp_pwr_enabled == POWER_OFF) {
		input_err(true, &info->client->dev, "%s: power off in IC\n", __func__);
		return 0;
	}

	zt_ts_esd_timer_stop(info);
	input_err(true, &info->client->dev, "%s: enable:%ld\n", __func__, value);

	if (value == 1) {
		ts_set_touchmode(TOUCH_SENTIVITY_MEASUREMENT_MODE);
		input_info(true, &info->client->dev, "%s: enable end\n", __func__);
	} else {
		ts_set_touchmode(TOUCH_POINT_MODE);
		input_info(true, &info->client->dev, "%s: disable end\n", __func__);
	}

	zt_ts_esd_timer_start(info);
	input_info(true, &info->client->dev, "%s: done\n", __func__);
	return count;
}

static ssize_t fod_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	int ret;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	/* get fod information */

	if (info->pdata->use_sponge) {
		if (!info->fod_info_vi_trx[0] || !info->fod_info_vi_trx[1] || !info->fod_info_vi_data_len) {
			ret = ts_read_from_sponge(info, ZT_SPONGE_FOD_INFO, info->fod_info_vi_trx, 3);
			if (ret < 0) {
				input_err(true, &info->client->dev, "%s: fail fod channel info.\n", __func__);
				return ret;
			}
		}
		input_info(true, &info->client->dev, "%s: %d,%d,%d\n", __func__,
				info->fod_info_vi_trx[0], info->fod_info_vi_trx[1], info->fod_info_vi_data_len);

		snprintf(buff, sizeof(buff), "%d,%d,%d,%d,%d",
				info->fod_info_vi_trx[0], info->fod_info_vi_trx[1], info->fod_info_vi_data_len,
				info->cap_info.x_node_num, info->cap_info.y_node_num);
	} else {
		if (!info->fod_info_vi_trx[0] || !info->fod_info_vi_trx[1] || !info->fod_info_vi_data_len) {
			ret = read_data(info->client, REG_FOD_MODE_VI_DATA_CH, info->fod_info_vi_trx, 2);
			if (ret < 0) {
				input_err(true, &info->client->dev, "%s: fail fod channel info.\n", __func__);
				return ret;
			}

			ret = read_data(info->client, REG_FOD_MODE_VI_DATA_LEN, (u8 *)&info->fod_info_vi_data_len, 2);
			if (ret < 0) {
				input_err(true, &info->client->dev, "%s: fail fod data len.\n", __func__);
				return ret;
			}
		}

		info->fod_info_vi_data_len = info->fod_info_vi_trx[2];
		input_info(true, &info->client->dev, "%s: %d,%d,%d\n", __func__,
				info->fod_info_vi_trx[1], info->fod_info_vi_trx[0], info->fod_info_vi_data_len);

		snprintf(buff, sizeof(buff), "%d,%d,%d,%d,%d",
				info->fod_info_vi_trx[1], info->fod_info_vi_trx[0], info->fod_info_vi_data_len,
				info->cap_info.x_node_num, info->cap_info.y_node_num);
	}


	return snprintf(buf, SEC_CMD_BUF_SIZE, "%s", buff);
}

static ssize_t fod_pos_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	int i, ret = -1;
	u8 fod_info_vi_data[SEC_CMD_STR_LEN];
	char buff[SEC_CMD_STR_LEN] = { 0 };

	if (info->pdata->use_sponge) {
		if (!info->fod_info_vi_data_len) {
			input_err(true, &info->client->dev, "%s: not read fod_info yet\n", __func__);
			return snprintf(buf, SEC_CMD_BUF_SIZE, "NG");
		}

		ret = ts_read_from_sponge(info, ZT_SPONGE_FOD_POSITION, fod_info_vi_data, info->fod_info_vi_data_len);
		if (ret < 0) {
			input_err(true, &info->client->dev, "%s: fail fod data read error.\n", __func__);
			return snprintf(buf, SEC_CMD_BUF_SIZE, "NG");
		}
	} else {
		for (i = 0; i < info->fod_info_vi_data_len; i++) {
			snprintf(buff, 3, "%02X", info->fod_touch_vi_data[i]);
			strlcat(buf, buff, SEC_CMD_BUF_SIZE);
		}
	}
	return strlen(buf);
}

static ssize_t aod_active_area(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);

	input_info(true, &info->client->dev, "%s: top:%d, edge:%d, bottom:%d\n",
			__func__, info->aod_active_area[0], info->aod_active_area[1], info->aod_active_area[2]);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d,%d,%d", info->aod_active_area[0], info->aod_active_area[1], info->aod_active_area[2]);
}

static ssize_t read_ito_check_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);

	input_info(true, &info->client->dev, "%s: %02X%02X%02X%02X\n",
			__func__, info->ito_test[0], info->ito_test[1],
			info->ito_test[2], info->ito_test[3]);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%02X%02X%02X%02X",
			info->ito_test[0], info->ito_test[1],
			info->ito_test[2], info->ito_test[3]);
}

static ssize_t read_wet_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);

	input_info(true, &info->client->dev, "%s: %d\n", __func__,info->wet_count);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d", info->wet_count);
}

static ssize_t clear_wet_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);

	info->wet_count = 0;

	input_info(true, &info->client->dev, "%s: clear\n", __func__);

	return count;
}

static ssize_t read_multi_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);

	input_info(true, &info->client->dev, "%s: %d\n", __func__,
			info->multi_count);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d", info->multi_count);
}

static ssize_t clear_multi_count_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);

	info->multi_count = 0;

	input_info(true, &info->client->dev, "%s: clear\n", __func__);

	return count;
}

static ssize_t read_comm_err_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);

	input_info(true, &info->client->dev, "%s: %d\n", __func__,
			info->comm_err_count);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d", info->comm_err_count);
}

static ssize_t clear_comm_err_count_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);

	info->comm_err_count = 0;

	input_info(true, &info->client->dev, "%s: clear\n", __func__);

	return count;
}

static ssize_t read_module_id_show(struct device *dev,
		struct device_attribute *devattr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);

	input_info(true, &info->client->dev, "%s\n", __func__);
#ifdef TCLM_CONCEPT
	return snprintf(buf, SEC_CMD_BUF_SIZE, "ZI%02X%02x%c%01X%04X\n",
			info->cap_info.reg_data_version, info->test_result.data[0],
			info->tdata->tclm_string[info->tdata->nvdata.cal_position].s_name,
			info->tdata->nvdata.cal_count & 0xF, info->tdata->nvdata.tune_fix_ver);
#else
	return snprintf(buf, SEC_CMD_BUF_SIZE, "ZI%02X\n",
			info->cap_info.reg_data_version);
#endif
}

static ssize_t prox_power_off_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);

	input_info(true, &info->client->dev, "%s: %ld\n", __func__,
			info->prox_power_off);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%ld", info->prox_power_off);
}

static ssize_t prox_power_off_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	int ret;
	unsigned long value = 0;

	ret = kstrtoul(buf, 10, &value);
	if (ret != 0)
		return ret;

	input_info(true, &info->client->dev, "%s: enable:%ld\n", __func__, value);
	info->prox_power_off = value;

	return count;
}

/*
 * read_support_feature function
 * returns the bit combination of specific feature that is supported.
 */
static ssize_t read_support_feature(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	struct i2c_client *client = info->client;

	char buff[SEC_CMD_STR_LEN] = { 0 };
	u32 feature = 0;

	if (info->pdata->support_aot)
		feature |= INPUT_FEATURE_ENABLE_SETTINGS_AOT;

	snprintf(buff, sizeof(buff), "%d", feature);
	input_info(true, &client->dev, "%s: %s\n", __func__, buff);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%s\n", buff);
}

/** for protos **/
static ssize_t protos_event_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);

	input_info(true, &info->client->dev, "%s: %d\n", __func__,
			info->hover_event);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d", info->hover_event != 3 ? 0 : 3);
}

static ssize_t protos_event_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	u8 data;
	int ret;

	ret = kstrtou8(buf, 10, &data);
	if (ret < 0)
		return ret;

	input_info(true, &info->client->dev, "%s: %d\n", __func__, data);

	if (data != 0 && data != 1 && data != 3) {
		input_err(true, &info->client->dev, "%s: incorrect data\n", __func__);
		return -EINVAL;
	}

	info->ed_enable = data;

	if (info->ed_enable == 3) {
		zt_set_optional_mode(info, DEF_OPTIONAL_MODE_EAR_DETECT, true);
		zt_set_optional_mode(info, DEF_OPTIONAL_MODE_EAR_DETECT_MUTUAL, false);
	} else if (info->ed_enable == 1) {
		zt_set_optional_mode(info, DEF_OPTIONAL_MODE_EAR_DETECT, false);
		zt_set_optional_mode(info, DEF_OPTIONAL_MODE_EAR_DETECT_MUTUAL, true);
	} else {
		zt_set_optional_mode(info, DEF_OPTIONAL_MODE_EAR_DETECT, false);
		zt_set_optional_mode(info, DEF_OPTIONAL_MODE_EAR_DETECT_MUTUAL, false);
	}

	return count;
}

#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
static u16 ts_get_touch_reg(u16 addr)
{
	int ret = 1;
	u16 reg_value;

	disable_irq(misc_info->irq);

	mutex_lock(&misc_info->work_lock);
	if (misc_info->work_state != NOTHING) {
		input_info(true, &misc_info->client->dev, "other process occupied.. (%d)\n",
				misc_info->work_state);
		enable_irq(misc_info->irq);
		mutex_unlock(&misc_info->work_lock);
		return -1;
	}
	misc_info->work_state = SET_MODE;

	write_reg(misc_info->client, 0x0A, 0x0A);
	usleep_range(20 * 1000, 20 * 1000);
	write_reg(misc_info->client, 0x0A, 0x0A);

	ret = read_data(misc_info->client, addr, (u8 *)&reg_value, 2);
	if (ret < 0) {
		input_err(true, &misc_info->client->dev,"%s: fail read touch reg\n", __func__);
	}

	misc_info->work_state = NOTHING;
	enable_irq(misc_info->irq);
	mutex_unlock(&misc_info->work_lock);

	return reg_value;
}

static void ts_set_touch_reg(u16 addr, u16 value)
{
	disable_irq(misc_info->irq);

	mutex_lock(&misc_info->work_lock);
	if (misc_info->work_state != NOTHING) {
		input_info(true, &misc_info->client->dev, "other process occupied.. (%d)\n",
				misc_info->work_state);
		enable_irq(misc_info->irq);
		mutex_unlock(&misc_info->work_lock);
		return;
	}
	misc_info->work_state = SET_MODE;

	write_reg(misc_info->client, 0x0A, 0x0A);
	usleep_range(20 * 1000, 20 * 1000);
	write_reg(misc_info->client, 0x0A, 0x0A);

	if (write_reg(misc_info->client, addr, value) != I2C_SUCCESS)
		input_err(true, &misc_info->client->dev,"%s: fail write touch reg\n", __func__);

	misc_info->work_state = NOTHING;
	enable_irq(misc_info->irq);
	mutex_unlock(&misc_info->work_lock);
}

static ssize_t read_reg_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);

	input_info(true, &info->client->dev, "%s: 0x%x\n", __func__,
			info->store_reg_data);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "0x%x", info->store_reg_data);
}

static ssize_t store_read_reg(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	u32 buff[2] = {0, }; //addr, size
	int ret;

	ret = sscanf(buf, "0x%x,0x%x", &buff[0], &buff[1]);
	if (ret != 2) {
		input_err(true, &info->client->dev,
				"%s: failed read params[0x%x]\n", __func__, ret);
		return -EINVAL;
	}

	if (buff[1] != 1 && buff[1] != 2) {
		input_err(true, &info->client->dev,
				"%s: incorrect byte length [0x%x]\n", __func__, buff[1]);
		return -EINVAL;
	}

	info->store_reg_data = ts_get_touch_reg((u16)buff[0]);

	if (buff[1] == 1)
		info->store_reg_data = info->store_reg_data & 0x00FF;

	input_info(true, &info->client->dev,
			"%s: read touch reg [addr:0x%x][data:0x%x]\n",
			__func__, buff[0], info->store_reg_data);

	return size;
}

static ssize_t store_write_reg(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);
	int buff[3];
	int ret;

	ret = sscanf(buf, "0x%x,0x%x,0x%x", &buff[0], &buff[1], &buff[2]);
	if (ret != 3) {
		input_err(true, &info->client->dev,
				"%s: failed read params[0x%x]\n", __func__, ret);
		return -EINVAL;
	}

	if (buff[1] != 1 && buff[1] != 2) {
		input_err(true, &info->client->dev,
				"%s: incorrect byte length [0x%x]\n", __func__, buff[1]);
		return -EINVAL;
	}

	if (buff[1] == 1)
		buff[2] = buff[2] & 0x00FF;

	ts_set_touch_reg((u16)buff[0], (u16)buff[2]);
	input_info(true, &info->client->dev,
			"%s: write touch reg [addr:0x%x][byte:0x%x][data:0x%x]\n",
			__func__, buff[0], buff[1], buff[2]);

	return size;
}
#endif

static ssize_t get_lp_dump(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct zt_ts_info *info = container_of(sec, struct zt_ts_info, sec);

	u8 sponge_data[10] = {0, };
	u16 current_index;
	u8 dump_format, dump_num;
	u16 dump_start, dump_end;
	int i;

	if (!info->pdata->use_sponge)
		return snprintf(buf, SEC_CMD_BUF_SIZE, "No Sponge");

	if (info->tsp_pwr_enabled == POWER_OFF) {
		input_err(true, &info->client->dev, "%s: Touch is stopped!\n", __func__);
		return snprintf(buf, SEC_CMD_BUF_SIZE, "TSP turned off");
	}

	if (info->work_state == ESD_TIMER) {
		input_err(true, &info->client->dev, "%s: Reset is ongoing!\n", __func__);
		return snprintf(buf, SEC_CMD_BUF_SIZE, "Reset is ongoing");
	}

	disable_irq(info->irq);
	ts_read_from_sponge(info, ZT_SPONGE_DUMP_FORMAT, (u8*)sponge_data, 4);

	dump_format = sponge_data[0];
	dump_num = sponge_data[1];
	dump_start = ZT_SPONGE_DUMP_START;
	dump_end = dump_start + (dump_format * (dump_num - 1));

	current_index = (sponge_data[3] & 0xFF) << 8 | (sponge_data[2] & 0xFF);
	if (current_index > dump_end || current_index < dump_start) {
		input_err(true, &info->client->dev,
				"Failed to Sponge LP log %d\n", current_index);
		snprintf(buf, SEC_CMD_BUF_SIZE,
				"NG, Failed to Sponge LP log, current_index=%d",
				current_index);
		goto out;
	}

	input_info(true, &info->client->dev, "%s: DEBUG format=%d, num=%d, start=%d, end=%d, current_index=%d\n",
				__func__, dump_format, dump_num, dump_start, dump_end, current_index);

	for (i = dump_num - 1 ; i >= 0 ; i--) {
		u16 data0, data1, data2, data3, data4;
		char buff[30] = {0, };
		u16 sponge_addr;

		if (current_index < (dump_format * i))
			sponge_addr = (dump_format * dump_num) + current_index - (dump_format * i);
		else
			sponge_addr = current_index - (dump_format * i);

		if (sponge_addr < dump_start)
			sponge_addr += (dump_format * dump_num);

		ts_read_from_sponge(info, sponge_addr, (u8*)sponge_data, dump_format);

		data0 = (sponge_data[1] & 0xFF) << 8 | (sponge_data[0] & 0xFF);
		data1 = (sponge_data[3] & 0xFF) << 8 | (sponge_data[2] & 0xFF);
		data2 = (sponge_data[5] & 0xFF) << 8 | (sponge_data[4] & 0xFF);
		data3 = (sponge_data[7] & 0xFF) << 8 | (sponge_data[6] & 0xFF);
		data4 = (sponge_data[9] & 0xFF) << 8 | (sponge_data[8] & 0xFF);

		if (data0 || data1 || data2 || data3 || data4) {
			if (dump_format == 10) {
				snprintf(buff, sizeof(buff),
						"%d: %04x%04x%04x%04x%04x\n",
						sponge_addr, data0, data1, data2, data3, data4);
			} else {
				snprintf(buff, sizeof(buff),
						"%d: %04x%04x%04x%04x\n",
						sponge_addr, data0, data1, data2, data3);
			}
			strlcat(buf, buff, PAGE_SIZE);
		}
	}

out:
	enable_irq(info->irq);
	return strlen(buf);
}

static DEVICE_ATTR(scrub_pos, S_IRUGO, scrub_position_show, NULL);
static DEVICE_ATTR(sensitivity_mode, S_IRUGO | S_IWUSR | S_IWGRP, sensitivity_mode_show, sensitivity_mode_store);
static DEVICE_ATTR(ito_check, S_IRUGO | S_IWUSR | S_IWGRP, read_ito_check_show, clear_wet_mode_store);
static DEVICE_ATTR(wet_mode, S_IRUGO | S_IWUSR | S_IWGRP, read_wet_mode_show, clear_wet_mode_store);
static DEVICE_ATTR(comm_err_count, S_IRUGO | S_IWUSR | S_IWGRP, read_comm_err_count_show, clear_comm_err_count_store);
static DEVICE_ATTR(multi_count, S_IRUGO | S_IWUSR | S_IWGRP, read_multi_count_show, clear_multi_count_store);
static DEVICE_ATTR(module_id, S_IRUGO, read_module_id_show, NULL);
static DEVICE_ATTR(prox_power_off, S_IRUGO | S_IWUSR | S_IWGRP, prox_power_off_show, prox_power_off_store);
static DEVICE_ATTR(support_feature, S_IRUGO, read_support_feature, NULL);
static DEVICE_ATTR(virtual_prox, S_IRUGO | S_IWUSR | S_IWGRP, protos_event_show, protos_event_store);
static DEVICE_ATTR(fod_info, S_IRUGO, fod_info_show, NULL);
static DEVICE_ATTR(fod_pos, S_IRUGO, fod_pos_show, NULL);
static DEVICE_ATTR(aod_active_area, 0444, aod_active_area, NULL);
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
static DEVICE_ATTR(read_reg_data, S_IRUGO | S_IWUSR | S_IWGRP, read_reg_show, store_read_reg);
static DEVICE_ATTR(write_reg_data, S_IWUSR | S_IWGRP, NULL, store_write_reg);
#endif
static DEVICE_ATTR(get_lp_dump, 0444, get_lp_dump, NULL);

static struct attribute *touchscreen_attributes[] = {
	&dev_attr_scrub_pos.attr,
	&dev_attr_sensitivity_mode.attr,
	&dev_attr_ito_check.attr,
	&dev_attr_wet_mode.attr,
	&dev_attr_multi_count.attr,
	&dev_attr_comm_err_count.attr,
	&dev_attr_module_id.attr,
	&dev_attr_prox_power_off.attr,
	&dev_attr_support_feature.attr,
	&dev_attr_virtual_prox.attr,
	&dev_attr_fod_info.attr,
	&dev_attr_fod_pos.attr,
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
	&dev_attr_read_reg_data.attr,
	&dev_attr_write_reg_data.attr,
#endif
	&dev_attr_get_lp_dump.attr,
	&dev_attr_aod_active_area.attr,
	NULL,
};

static struct attribute_group touchscreen_attr_group = {
	.attrs = touchscreen_attributes,
};

#ifdef USE_MISC_DEVICE
#define TOUCH_IOCTL_BASE	0xbc
#define TOUCH_IOCTL_GET_DEBUGMSG_STATE		_IOW(TOUCH_IOCTL_BASE, 0, int)
#define TOUCH_IOCTL_SET_DEBUGMSG_STATE		_IOW(TOUCH_IOCTL_BASE, 1, int)
#define TOUCH_IOCTL_GET_CHIP_REVISION		_IOW(TOUCH_IOCTL_BASE, 2, int)
#define TOUCH_IOCTL_GET_FW_VERSION			_IOW(TOUCH_IOCTL_BASE, 3, int)
#define TOUCH_IOCTL_GET_REG_DATA_VERSION	_IOW(TOUCH_IOCTL_BASE, 4, int)
#define TOUCH_IOCTL_VARIFY_UPGRADE_SIZE		_IOW(TOUCH_IOCTL_BASE, 5, int)
#define TOUCH_IOCTL_VARIFY_UPGRADE_DATA		_IOW(TOUCH_IOCTL_BASE, 6, int)
#define TOUCH_IOCTL_START_UPGRADE			_IOW(TOUCH_IOCTL_BASE, 7, int)
#define TOUCH_IOCTL_GET_X_NODE_NUM			_IOW(TOUCH_IOCTL_BASE, 8, int)
#define TOUCH_IOCTL_GET_Y_NODE_NUM			_IOW(TOUCH_IOCTL_BASE, 9, int)
#define TOUCH_IOCTL_GET_TOTAL_NODE_NUM		_IOW(TOUCH_IOCTL_BASE, 10, int)
#define TOUCH_IOCTL_SET_RAW_DATA_MODE		_IOW(TOUCH_IOCTL_BASE, 11, int)
#define TOUCH_IOCTL_GET_RAW_DATA			_IOW(TOUCH_IOCTL_BASE, 12, int)
#define TOUCH_IOCTL_GET_X_RESOLUTION		_IOW(TOUCH_IOCTL_BASE, 13, int)
#define TOUCH_IOCTL_GET_Y_RESOLUTION		_IOW(TOUCH_IOCTL_BASE, 14, int)
#define TOUCH_IOCTL_HW_CALIBRAION			_IOW(TOUCH_IOCTL_BASE, 15, int)
#define TOUCH_IOCTL_GET_REG					_IOW(TOUCH_IOCTL_BASE, 16, int)
#define TOUCH_IOCTL_SET_REG					_IOW(TOUCH_IOCTL_BASE, 17, int)
#define TOUCH_IOCTL_SEND_SAVE_STATUS		_IOW(TOUCH_IOCTL_BASE, 18, int)
#define TOUCH_IOCTL_DONOT_TOUCH_EVENT		_IOW(TOUCH_IOCTL_BASE, 19, int)

static int ts_misc_fops_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int ts_misc_fops_close(struct inode *inode, struct file *filp)
{
	return 0;
}

static long ts_misc_fops_ioctl(struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	struct raw_ioctl raw_ioctl;
	u8 *u8Data;
	int ret = 0;
	size_t sz = 0;
	u16 mode;
	struct reg_ioctl reg_ioctl;
	u16 val;
	int nval = 0;
#ifdef CONFIG_COMPAT
	void __user *argp = compat_ptr(arg);
#else
	void __user *argp = (void __user *)arg;
#endif

	if (misc_info == NULL) {
		pr_err("%s %s misc device NULL?\n", SECLOG, __func__);
		return -1;
	}

	switch (cmd) {
	case TOUCH_IOCTL_GET_DEBUGMSG_STATE:
		ret = m_ts_debug_mode;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_SET_DEBUGMSG_STATE:
		if (copy_from_user(&nval, argp, sizeof(nval))) {
			input_err(true, &misc_info->client->dev, "%s: error : copy_from_user\n", __func__);
			return -1;
		}
		if (nval)
			input_info(true, &misc_info->client->dev, "[zinitix_touch] on debug mode (%d)\n", nval);
		else
			input_info(true, &misc_info->client->dev, "[zinitix_touch] off debug mode (%d)\n", nval);
		m_ts_debug_mode = nval;
		break;

	case TOUCH_IOCTL_GET_CHIP_REVISION:
		ret = misc_info->cap_info.ic_revision;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_GET_FW_VERSION:
		ret = misc_info->cap_info.fw_version;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_GET_REG_DATA_VERSION:
		ret = misc_info->cap_info.reg_data_version;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_VARIFY_UPGRADE_SIZE:
		if (copy_from_user(&sz, argp, sizeof(size_t)))
			return -1;
		if (misc_info->cap_info.ic_fw_size != sz) {
			input_info(true, &misc_info->client->dev, "%s: firmware size error\n", __func__);
			return -1;
		}
		break;

	case TOUCH_IOCTL_GET_X_RESOLUTION:
		ret = misc_info->pdata->x_resolution;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_GET_Y_RESOLUTION:
		ret = misc_info->pdata->y_resolution;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_GET_X_NODE_NUM:
		ret = misc_info->cap_info.x_node_num;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_GET_Y_NODE_NUM:
		ret = misc_info->cap_info.y_node_num;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_GET_TOTAL_NODE_NUM:
		ret = misc_info->cap_info.total_node_num;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_HW_CALIBRAION:
		ret = -1;
		disable_irq(misc_info->irq);
		mutex_lock(&misc_info->work_lock);
		if (misc_info->work_state != NOTHING) {
			input_info(true, &misc_info->client->dev, "[zinitix_touch]: other process occupied.. (%d)\r\n",
					misc_info->work_state);
			mutex_unlock(&misc_info->work_lock);
			return -1;
		}
		misc_info->work_state = HW_CALIBRAION;
		zt_delay(100);

		/* h/w calibration */
		if (ts_hw_calibration(misc_info) == true) {
			ret = 0;
#ifdef TCLM_CONCEPT
			sec_tclm_root_of_cal(misc_info->tdata, CALPOSITION_TESTMODE);
			sec_execute_tclm_package(misc_info->tdata, 1);
			sec_tclm_root_of_cal(misc_info->tdata, CALPOSITION_NONE);
#endif
		}

		mode = misc_info->touch_mode;
		if (write_reg(misc_info->client,
					ZT_TOUCH_MODE, mode) != I2C_SUCCESS) {
			input_info(true, &misc_info->client->dev, "[zinitix_touch]: failed to set touch mode %d.\n",
					mode);
			goto fail_hw_cal;
		}

		if (write_cmd(misc_info->client,
					ZT_SWRESET_CMD) != I2C_SUCCESS)
			goto fail_hw_cal;

		enable_irq(misc_info->irq);
		misc_info->work_state = NOTHING;
		mutex_unlock(&misc_info->work_lock);
		return ret;
fail_hw_cal:
		enable_irq(misc_info->irq);
		misc_info->work_state = NOTHING;
		mutex_unlock(&misc_info->work_lock);
		return -1;

	case TOUCH_IOCTL_SET_RAW_DATA_MODE:
		if (misc_info == NULL) {
			pr_err("%s %s misc device NULL?\n", SECLOG, __func__);
			return -1;
		}
		if (copy_from_user(&nval, argp, sizeof(nval))) {
			input_info(true, &misc_info->client->dev, "%s: error : copy_from_user\n", __func__);
			misc_info->work_state = NOTHING;
			return -1;
		}
		ts_set_touchmode((u16)nval);

		return 0;

	case TOUCH_IOCTL_GET_REG:
		if (misc_info == NULL) {
			pr_err("%s %s misc device NULL?\n", SECLOG, __func__);
			return -1;
		}
		mutex_lock(&misc_info->work_lock);
		if (misc_info->work_state != NOTHING) {
			input_info(true, &misc_info->client->dev, "[zinitix_touch]:other process occupied.. (%d)\n",
					misc_info->work_state);
			mutex_unlock(&misc_info->work_lock);
			return -1;
		}

		misc_info->work_state = SET_MODE;

		if (copy_from_user(&reg_ioctl, argp, sizeof(struct reg_ioctl))) {
			misc_info->work_state = NOTHING;
			mutex_unlock(&misc_info->work_lock);
			input_info(true, &misc_info->client->dev, "%s: error : copy_from_user\n", __func__);
			return -1;
		}

		if (read_data(misc_info->client,
					(u16)reg_ioctl.addr, (u8 *)&val, 2) < 0)
			ret = -1;

		nval = (int)val;

#ifdef CONFIG_COMPAT
		if (copy_to_user(compat_ptr(reg_ioctl.val), (u8 *)&nval, 4)) {
#else
		if (copy_to_user((void __user *)(reg_ioctl.val), (u8 *)&nval, 4)) {
#endif
			misc_info->work_state = NOTHING;
			mutex_unlock(&misc_info->work_lock);
			input_info(true, &misc_info->client->dev, "%s: error : copy_to_user\n", __func__);
			return -1;
		}

		input_info(true, &misc_info->client->dev, "%s read : reg addr = 0x%x, val = 0x%x\n", __func__,
				reg_ioctl.addr, nval);

		misc_info->work_state = NOTHING;
		mutex_unlock(&misc_info->work_lock);
		return ret;

		case TOUCH_IOCTL_SET_REG:

		mutex_lock(&misc_info->work_lock);
		if (misc_info->work_state != NOTHING) {
			input_info(true, &misc_info->client->dev, "[zinitix_touch]: other process occupied.. (%d)\n",
					misc_info->work_state);
			mutex_unlock(&misc_info->work_lock);
			return -1;
		}

		misc_info->work_state = SET_MODE;
		if (copy_from_user(&reg_ioctl,
					argp, sizeof(struct reg_ioctl))) {
			misc_info->work_state = NOTHING;
			mutex_unlock(&misc_info->work_lock);
			input_info(true, &misc_info->client->dev, "%s: error : copy_from_user(1)\n", __func__);
			return -1;
		}

#ifdef CONFIG_COMPAT
		if (copy_from_user(&val, compat_ptr(reg_ioctl.val), sizeof(val))) {
#else
		if (copy_from_user(&val,(void __user *)(reg_ioctl.val), sizeof(val))) {
#endif
			misc_info->work_state = NOTHING;
			mutex_unlock(&misc_info->work_lock);
			input_info(true, &misc_info->client->dev, "%s: error : copy_from_user(2)\n", __func__);
			return -1;
		}

		if (write_reg(misc_info->client,
					(u16)reg_ioctl.addr, val) != I2C_SUCCESS)
			ret = -1;

		input_info(true, &misc_info->client->dev, "%s write : reg addr = 0x%x, val = 0x%x\r\n", __func__,
				reg_ioctl.addr, val);
		misc_info->work_state = NOTHING;
		mutex_unlock(&misc_info->work_lock);
		return ret;

		case TOUCH_IOCTL_DONOT_TOUCH_EVENT:

		if (misc_info == NULL) {
			pr_err("%s %s misc device NULL?\n", SECLOG, __func__);
			return -1;
		}
		mutex_lock(&misc_info->work_lock);
		if (misc_info->work_state != NOTHING) {
			input_info(true, &misc_info->client->dev, "[zinitix_touch]: other process occupied.. (%d)\r\n",
					misc_info->work_state);
			mutex_unlock(&misc_info->work_lock);
			return -1;
		}

		misc_info->work_state = SET_MODE;
		if (write_reg(misc_info->client,
					ZT_INT_ENABLE_FLAG, 0) != I2C_SUCCESS)
			ret = -1;
		input_info(true, &misc_info->client->dev, "%s write : reg addr = 0x%x, val = 0x0\r\n", __func__,
				ZT_INT_ENABLE_FLAG);

		misc_info->work_state = NOTHING;
		mutex_unlock(&misc_info->work_lock);
		return ret;

		case TOUCH_IOCTL_SEND_SAVE_STATUS:
		if (misc_info == NULL) {
			pr_err("%s %s misc device NULL?\n", SECLOG, __func__);
			return -1;
		}
		mutex_lock(&misc_info->work_lock);
		if (misc_info->work_state != NOTHING) {
			input_info(true, &misc_info->client->dev, "[zinitix_touch]: other process occupied.." \
					"(%d)\r\n", misc_info->work_state);
			mutex_unlock(&misc_info->work_lock);
			return -1;
		}
		misc_info->work_state = SET_MODE;
		ret = 0;
		write_reg(misc_info->client, VCMD_NVM_WRITE_ENABLE, 0x0001);
		if (write_cmd(misc_info->client,
					ZT_SAVE_STATUS_CMD) != I2C_SUCCESS)
			ret =  -1;

		zt_delay(1000);	/* for fusing eeprom */
		write_reg(misc_info->client, VCMD_NVM_WRITE_ENABLE, 0x0000);

		misc_info->work_state = NOTHING;
		mutex_unlock(&misc_info->work_lock);
		return ret;

		case TOUCH_IOCTL_GET_RAW_DATA:
		if (misc_info == NULL) {
			pr_err("%s %s misc device NULL?\n", SECLOG, __func__);
			return -1;
		}

		if (misc_info->touch_mode == TOUCH_POINT_MODE)
			return -1;

		mutex_lock(&misc_info->raw_data_lock);
		if (misc_info->update == 0) {
			mutex_unlock(&misc_info->raw_data_lock);
			return -2;
		}

		if (copy_from_user(&raw_ioctl,
					argp, sizeof(struct raw_ioctl))) {
			mutex_unlock(&misc_info->raw_data_lock);
			input_info(true, &misc_info->client->dev, "%s: error : copy_from_user\n", __func__);
			return -1;
		}

		misc_info->update = 0;

		u8Data = (u8 *)&misc_info->cur_data[0];
		if (raw_ioctl.sz > MAX_TRAW_DATA_SZ*2)
			raw_ioctl.sz = MAX_TRAW_DATA_SZ*2;
#ifdef CONFIG_COMPAT
		if (copy_to_user(compat_ptr(raw_ioctl.buf), (u8 *)u8Data, raw_ioctl.sz)) {
#else
		if (copy_to_user((void __user *)(raw_ioctl.buf), (u8 *)u8Data, raw_ioctl.sz)) {
#endif
			mutex_unlock(&misc_info->raw_data_lock);
			return -1;
		}

		mutex_unlock(&misc_info->raw_data_lock);
		return 0;

	default:
		break;
	}
	return 0;
}

static const struct file_operations ts_misc_fops = {
	.owner = THIS_MODULE,
	.open = ts_misc_fops_open,
	.release = ts_misc_fops_close,
	//.unlocked_ioctl = ts_misc_fops_ioctl,
	.compat_ioctl = ts_misc_fops_ioctl,
};

static struct miscdevice touch_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "zinitix_touch_misc",
	.fops = &ts_misc_fops,
};
#endif

int init_sec_factory(struct zt_ts_info *info)
{
	struct tsp_raw_data *raw_data;
	int ret;

	raw_data = kzalloc(sizeof(struct tsp_raw_data), GFP_KERNEL);
	if (unlikely(!raw_data)) {
		input_err(true, &info->client->dev, "%s: Failed to allocate memory\n",
				__func__);
		ret = -ENOMEM;

		goto err_alloc;
	}

	ret = sec_cmd_init(&info->sec, sec_cmds,
			ARRAY_SIZE(sec_cmds), SEC_CLASS_DEVT_TSP);
	if (ret < 0) {
		input_err(true, &info->client->dev,
				"%s: Failed to sec_cmd_init\n", __func__);
		goto err_init_cmd;
	}

	ret = sysfs_create_group(&info->sec.fac_dev->kobj,
			&touchscreen_attr_group);
	if (ret < 0) {
		input_err(true, &info->client->dev,
				"%s: FTS Failed to create sysfs attributes\n", __func__);
		goto err_create_sysfs;
	}

	ret = sysfs_create_link(&info->sec.fac_dev->kobj,
			&info->input_dev->dev.kobj, "input");
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: Failed to create link\n", __func__);
		goto err_create_sysfs;
	}

	info->raw_data = raw_data;
#ifdef USE_MISC_DEVICE
	ret = misc_register(&touch_misc_device);
	if (ret) {
		input_err(true, &info->client->dev, "%s: Failed to register touch misc device\n", __func__);
		goto err_create_sysfs;
	}
#endif

	return ret;

err_create_sysfs:
err_init_cmd:
	kfree(raw_data);
err_alloc:

	return ret;
}

void remove_sec_factory(struct zt_ts_info *info)
{
#ifdef USE_MISC_DEVICE
	misc_deregister(&touch_misc_device);
#endif
	sysfs_delete_link(&info->sec.fac_dev->kobj, &info->input_dev->dev.kobj, "input");
	sysfs_remove_group(&info->sec.fac_dev->kobj, &touchscreen_attr_group);
}

