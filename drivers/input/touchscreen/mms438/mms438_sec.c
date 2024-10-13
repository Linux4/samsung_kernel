
struct mms_info *mms_touch_info;

static void set_cmd_result(struct mms_fac_data *fac_data, char *buff, int len)
{
	strncat(fac_data->cmd_result, buff, len);
}

static void set_default_result(struct mms_fac_data *fac_data)
{
	char delim = ':';

	memset(fac_data->cmd_result, 0x00, ARRAY_SIZE(fac_data->cmd_result));
	memset(fac_data->cmd_buff, 0x00, ARRAY_SIZE(fac_data->cmd_buff));
	memcpy(fac_data->cmd_result, fac_data->cmd, strlen(fac_data->cmd));
	strncat(fac_data->cmd_result, &delim, 1);
}

static int update_from_req_fw(struct mms_info *info)
{
	struct i2c_client *client = info->client;
	const char *path = info->pdata->inkernel_fw_name;

	dev_info(&client->dev, "%s : firmware load %s\n", __func__, path);

	return mms_flash_fw(info->fw, info, true);
}

static int update_from_ums(struct mms_info *info)
{
	struct i2c_client *client = info->client;
	const char *path = "/sdcard/mms438.fw";
	mm_segment_t old_fs;
	struct file *fp;
	long fsize, nread;
	struct firmware fw;
	u8 *buff;
	int ret = 0;

	dev_info(&client->dev, "%s : file open %s\n", __func__, path);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open(path, O_RDONLY, S_IRUSR);
	if (IS_ERR(fp)) {
		dev_err(&client->dev, "Failed to open file\n");
		ret = PTR_ERR(fp);
		goto err_file_open;
	}

	fsize = fp->f_path.dentry->d_inode->i_size;

	buff = kzalloc((size_t)fsize, GFP_KERNEL);
	if (!buff) {
		dev_err(&client->dev, "%s : Failed to allocate memory\n", __func__);
		ret = -ENOMEM;
		goto err_mem_alloc;
	}


	nread = vfs_read(fp, (char __user *)buff, fsize, &fp->f_pos);
	if (fsize != nread) {
		dev_err(&client->dev, "Failed to read file correctly\n");
		ret = -EPERM;
		goto err_file_read;
	}

	fw.size = fsize;
	fw.data = buff;
	ret = mms_flash_fw(&fw, info, true);

err_file_read:
	kfree(buff);
err_mem_alloc:
	filp_close(fp, NULL);
err_file_open:
	set_fs(old_fs);

	return ret;
}

static void fw_update(void *device_data)
{
	struct mms_info *info = (struct mms_info *)device_data;
	struct mms_fac_data *fac_data = info->fac_data;
	struct i2c_client *client = info->client;
	int fw_type = fac_data->cmd_param[0];
	int ret;

	set_default_result(fac_data);

	if (!info->enabled) {
		dev_err(&client->dev, "%s : TSP is disabled\n");
		goto out;
	}

	mutex_lock(&info->input_dev->mutex);
	disable_irq(info->irq);

	switch (fw_type) {
	case FW_FROM_BUILT_IN:
		ret = update_from_req_fw(info);
	break;
	case FW_FROM_UMS:
		ret = update_from_ums(info);
	break;
	default:
		dev_err(&client->dev,
					"%s : Invalid update type(%d)\n", __func__, fw_type);
		goto err_update;
	break;
	}

	if (ret) {
		dev_err(&client->dev, "%s : Failed fw update(%d)\n", __func__, ret);
		goto err_update;
	}

	enable_irq(info->irq);
	mutex_unlock(&info->input_dev->mutex);

	fac_data->cmd_state = (ret) ? CMD_STATUS_FAIL : CMD_STATUS_OK;
	sprintf(fac_data->cmd_buff, "%s", (ret) ? "NG" : "OK");
	set_cmd_result(fac_data, fac_data->cmd_buff, strlen(fac_data->cmd_buff));

	dev_info(&client->dev, "%s : %s\n", __func__, fac_data->cmd_buff);

	return;

err_update:
	enable_irq(info->irq);
	mutex_unlock(&info->input_dev->mutex);
out:
	fac_data->cmd_state = CMD_STATUS_FAIL;
	sprintf(fac_data->cmd_buff, "%s", "NG");
	set_cmd_result(fac_data, fac_data->cmd_buff, strlen(fac_data->cmd_buff));

	return;
}

static void get_fw_ver_bin(void *device_data)
{
	struct mms_info *info = (struct mms_info *)device_data;
	struct mms_fac_data *fac_data = info->fac_data;
	struct i2c_client *client = info->client;
	char buff[TSP_CMD_FULL_VER_LEN] = {0,};

	set_default_result(fac_data);

	snprintf(buff, 3, "%s", &info->fw->data[0x452]);
	sprintf(buff + 2, "%02X", info->fw->data[0x454]);
	sprintf(buff + 4, "%02X", info->fw->data[0x8]);
	sprintf(buff + 6, "%02X", info->fw->data[0x32]);

	fac_data->cmd_state = CMD_STATUS_OK;
	sprintf(fac_data->cmd_buff, "%s", buff);
	set_cmd_result(fac_data, fac_data->cmd_buff, strlen(fac_data->cmd_buff));

	dev_info(&client->dev, "%s : %s\n", __func__, fac_data->cmd_buff);

	return;
}

static void get_fw_ver_ic(void *device_data)
{
	struct mms_info *info = (struct mms_info *)device_data;
	struct mms_fac_data *fac_data = info->fac_data;
	struct i2c_client *client = info->client;
	char buff[TSP_CMD_FULL_VER_LEN] = {0,};
	int ret;

	set_default_result(fac_data);

	ret = i2c_smbus_read_i2c_block_data(client, MMS_VENDOR_ID, 2, buff);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to read vendor ID(%d)\n", ret);
		goto out;
	}

	ret = i2c_smbus_read_byte_data(client, MMS_HW_ID);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to read HW ID(%d)\n", ret);
		goto out;
	}

	sprintf(buff + 2, "%02X", ret);

	ret = i2c_smbus_read_byte_data(client, MMS_CORE_VERSION);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to read FW ver (%d)\n", ret);
		goto out;
	}

	sprintf(buff + 4, "%02X", ret);

	ret = i2c_smbus_read_byte_data(client, MMS_CONFIG_VERSION);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to read FW ver (%d)\n", ret);
		goto out;
	}

	sprintf(buff + 6, "%02X", ret);

	fac_data->cmd_state = CMD_STATUS_OK;
	sprintf(fac_data->cmd_buff, "%s", buff);
	set_cmd_result(fac_data, fac_data->cmd_buff, strlen(fac_data->cmd_buff));

	dev_info(&client->dev, "%s : %s\n", __func__, fac_data->cmd_buff);

	return;

out:
	fac_data->cmd_state = CMD_STATUS_FAIL;
	sprintf(fac_data->cmd_buff, "%s", "NG");
	set_cmd_result(fac_data, fac_data->cmd_buff, strlen(fac_data->cmd_buff));

	return ;
}

static void get_threshold(void *device_data)
{
	struct mms_info *info = (struct mms_info *)device_data;
	struct mms_fac_data *fac_data = info->fac_data;
	struct i2c_client *client = info->client;
	s32 buff;

	set_default_result(fac_data);

	buff = i2c_smbus_read_byte_data(info->client, MMS_THRESHOLD);
	if (buff < 0) {
		dev_err(&client->dev, "Failed to read threshold (%d)\n", buff);
		goto out;
	}

	fac_data->cmd_state = CMD_STATUS_OK;
	sprintf(fac_data->cmd_buff, "%d", buff);
	set_cmd_result(fac_data, fac_data->cmd_buff, strlen(fac_data->cmd_buff));

	dev_info(&client->dev, "%s : %s\n", __func__, fac_data->cmd_buff);

	return;

out:
	fac_data->cmd_state = CMD_STATUS_FAIL;
	sprintf(fac_data->cmd_buff, "%s", "NG");
	set_cmd_result(fac_data, fac_data->cmd_buff, strlen(fac_data->cmd_buff));

	return ;
}

static void module_off_master(void *device_data)
{
	struct mms_info *info = (struct mms_info *)device_data;
	struct mms_fac_data *fac_data = info->fac_data;
	struct i2c_client *client = info->client;
	int ret;

	set_default_result(fac_data);

	ret = mms_onoff_power(info, false);
	if (ret) {
		dev_err(&client->dev, "Failed to %s(%d)\n", __func__, ret);
		goto out;
	}

	fac_data->cmd_state = CMD_STATUS_OK;
	sprintf(fac_data->cmd_buff, "%s", "OK");
	set_cmd_result(fac_data, fac_data->cmd_buff, strlen(fac_data->cmd_buff));

	dev_info(&client->dev, "%s : %s\n", __func__, fac_data->cmd_buff);

out:
	fac_data->cmd_state = CMD_STATUS_FAIL;
	sprintf(fac_data->cmd_buff, "%s", "NG");
	set_cmd_result(fac_data, fac_data->cmd_buff, strlen(fac_data->cmd_buff));

	return;
}

static void module_on_master(void *device_data)
{
	struct mms_info *info = (struct mms_info *)device_data;
	struct mms_fac_data *fac_data = info->fac_data;
	struct i2c_client *client = info->client;
	int ret;

	set_default_result(fac_data);

	ret = mms_onoff_power(info, true);
	if (ret) {
		dev_err(&client->dev, "Failed to %s(%d)\n", __func__, ret);
		goto out;
	}

	fac_data->cmd_state = CMD_STATUS_OK;
	sprintf(fac_data->cmd_buff, "%s", "OK");
	set_cmd_result(fac_data, fac_data->cmd_buff, strlen(fac_data->cmd_buff));

	dev_info(&client->dev, "%s : %s\n", __func__, fac_data->cmd_buff);

out:
	fac_data->cmd_state = CMD_STATUS_FAIL;
	sprintf(fac_data->cmd_buff, "%s", "NG");
	set_cmd_result(fac_data, fac_data->cmd_buff, strlen(fac_data->cmd_buff));

	return;
}

static void get_chip_vendor(void *device_data)
{
	struct mms_info *info = (struct mms_info *)device_data;
	struct mms_fac_data *fac_data = info->fac_data;
	struct i2c_client *client = info->client;

	set_default_result(fac_data);

	fac_data->cmd_state = CMD_STATUS_OK;
	sprintf(fac_data->cmd_buff, "%s", "MELFAS");
	set_cmd_result(fac_data, fac_data->cmd_buff, strlen(fac_data->cmd_buff));

	dev_info(&client->dev, "%s : %s\n", __func__, fac_data->cmd_buff);

	return;
}

static void get_chip_name(void *device_data)
{
	struct mms_info *info = (struct mms_info *)device_data;
	struct mms_fac_data *fac_data = info->fac_data;
	struct i2c_client *client = info->client;

	set_default_result(fac_data);

	fac_data->cmd_state = CMD_STATUS_OK;
	sprintf(fac_data->cmd_buff, "%s", "MMS438");
	set_cmd_result(fac_data, fac_data->cmd_buff, strlen(fac_data->cmd_buff));

	dev_info(&client->dev, "%s : %s\n", __func__, fac_data->cmd_buff);

	return;
}

static void get_x_num(void *device_data)
{
	struct mms_info *info = (struct mms_info *)device_data;
	struct mms_fac_data *fac_data = info->fac_data;
	struct i2c_client *client = info->client;

	set_default_result(fac_data);

	fac_data->cmd_state = CMD_STATUS_OK;
	sprintf(fac_data->cmd_buff, "%d", fac_data->tx_num);
	set_cmd_result(fac_data, fac_data->cmd_buff, strlen(fac_data->cmd_buff));

	dev_info(&client->dev, "%s : %s\n", __func__, fac_data->cmd_buff);

	return;
}

static void get_y_num(void *device_data)
{
	struct mms_info *info = (struct mms_info *)device_data;
	struct mms_fac_data *fac_data = info->fac_data;
	struct i2c_client *client = info->client;

	set_default_result(fac_data);

	fac_data->cmd_state = CMD_STATUS_OK;
	sprintf(fac_data->cmd_buff, "%d", fac_data->rx_num);
	set_cmd_result(fac_data, fac_data->cmd_buff, strlen(fac_data->cmd_buff));

	dev_info(&client->dev, "%s : %s\n", __func__, fac_data->cmd_buff);

	return;
}

static inline void print_raw_data(const short *data, int len, int width)
{
	char *buff = kzalloc((width * 6) + 1, GFP_KERNEL);
	char *offset;
	int i, j;

	for (i = 0; i < len; i++) {
		offset = buff;

		for (j = 0; j < width; j++) {
			sprintf(offset, "%6d", data[(width * i) + j]);
			offset += 6;
		}

		pr_info("%s\n", buff);
	}

	kfree(buff);
}

static int read_raw_data_all(struct mms_info *info, u8 univ_cmd, short *buff)
{
	struct mms_fac_data *fac_data = info->fac_data;
	struct i2c_client *client = info->client;
	u8 buf[76] = {0, };
	u8 reg[4] = {0, };
	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.buf = reg,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
		},
	};
	int r, t;
	int data;
	int ret;
	int track_lev = 0;

	fac_data->max = INT_MIN;
	fac_data->min = INT_MAX;

	if (univ_cmd == MMS_UNIV_REFERENCE) {
		track_lev = i2c_smbus_read_byte_data(client, MMS_REF_TRACK_LEVEL);
		if (track_lev < 0) {
			dev_err(&client->dev,
					"Failed to read reference track level(%d_\n", track_lev);
			goto out;
		}

		dev_info(&client->dev, "track level is %d\n", track_lev);
	}

	for (r = 0 ; r < fac_data->rx_num ; r++) {
		reg[0] = MMS_UNIVERSAL_CMD;
		reg[1] = univ_cmd;
		reg[2] = 0xFF;
		reg[3] = r;
		msg[0].len = 4;

		if (i2c_transfer(client->adapter, &msg[0], 1) != 1) {
			dev_err(&client->dev, "%s : Failed to send cmd for node\n",
						__func__);
			ret = -EPERM;
			goto out;
		}

		ret = i2c_smbus_read_byte_data(client, MMS_UNIVERSAL_RESULT_SZ);
		if (ret < 0 || ret > 76) {
			dev_err(&client->dev,
				"%s : Failed to read node result size(%d)\n", __func__, ret);
			goto out;
		}

		reg[0] = MMS_UNIVERSAL_RESULT;
		msg[0].len = 1;
		msg[1].len = ret;
		msg[1].buf = buf;

		if (i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg))
			!= ARRAY_SIZE(msg)){
			dev_err(&client->dev, "%s : Failed to read result\n", __func__);
			ret = -EPERM;
			goto out;
		}

		if (univ_cmd == MMS_UNIV_REFERENCE)
			ret >>= 2;
		else
			ret >>= 1;

		for (t = 0; t < ret; t++){
			if (univ_cmd == MMS_UNIV_INTENSITY)
				data = (s16)(buf[2 * t] | buf[(2 * t) + 1] << 8);
			else if (univ_cmd == MMS_UNIV_REFERENCE)
				data = (*(s32 *)&buf[sizeof(s32) * t]) >> track_lev;
			buff[(t * fac_data->rx_num) + r] = (s16)data;

			if (fac_data->max < data)
				fac_data->max = data;

			if (fac_data->min > data)
				fac_data->min = data;
		}
	}

#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
	print_raw_data(buff, ret, fac_data->rx_num);
#endif

	ret = 0;

out:
	return ret;
}

static int enter_test_mode(struct mms_info *info)
{
	struct i2c_client *client = info->client;
	int ret = 0;

	disable_irq(info->irq);

	ret = i2c_smbus_write_byte_data(
				client, MMS_UNIVERSAL_CMD, MMS_UNIV_ENTER_TEST);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to enter test mode(%d)\n", ret);
		goto out;
	}

	do {
		udelay(100);
	} while (gpio_get_value(info->pdata->gpio_int));

	ret = i2c_smbus_read_byte_data(client, MMS_EVENT_PKT);
	dev_info(&client->dev, "event(%x)\n", ret);

	if (ret != MMS_TEST_MODE) {
		dev_err(&client->dev, "Failed to check maker(%x)\n", ret);
		ret = -EPERM;
		goto out;
	}

	return 0;

out:
	enable_irq(info->irq);

	return ret;
}

static int exit_test_mode(struct mms_info *info)
{
	struct i2c_client *client = info->client;
	int ret = 0;

	ret = i2c_smbus_write_byte_data(
				client, MMS_UNIVERSAL_CMD, MMS_UNIV_EXIT_TEST);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to exit test mode(%d)\n", ret);
		goto out;
	}

	enable_irq(info->irq);

	return 0;

out:
	enable_irq(info->irq);

	return ret;
}

static int read_cm_data_all(struct mms_info *info, u8 univ_cmd, short *buff)
{
	struct mms_fac_data *fac_data = info->fac_data;
	struct i2c_client *client = info->client;
	u8 buf[40] = {0, };
	u8 reg[4] = {0, };
	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.buf = reg,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
		},
	};
	int r, t, data;
	int ret;

	fac_data->max = INT_MIN;
	fac_data->min = INT_MAX;

	ret = i2c_smbus_write_byte_data(client, MMS_UNIVERSAL_CMD, univ_cmd);
	if (ret < 0) {
		dev_err(&client->dev, "%s : Failed to send %x cmd(%d)\n",
					__func__, univ_cmd, ret);
		goto out;
	}

	do {
		udelay(100);
	} while (gpio_get_value(info->pdata->gpio_int));

	ret = i2c_smbus_read_byte_data(client, MMS_UNIVERSAL_RESULT);
	if (ret != true) {
		dev_err(&client->dev, "%s : Failed 0x%x test(%d)\n", __func__,
					univ_cmd, ret < 0 ? ret : -EPERM);
		goto out;
	}

	for (r = 0 ; r < fac_data->rx_num; r++) {
		reg[0] = MMS_UNIVERSAL_CMD;
		reg[1] = univ_cmd + 1; /* get univ_cmd value from node */
		reg[2] = 0xFF;
		reg[3] = r;
		msg[0].len = 4;

		if (i2c_transfer(client->adapter, &msg[0], 1) != 1) {
			dev_err(&client->dev, "%s : Failed to send cmd for node\n",
						__func__);
			ret = -EPERM;
			goto out;
		}

		while (gpio_get_value(info->pdata->gpio_int));

		ret = i2c_smbus_read_byte_data(client, MMS_UNIVERSAL_RESULT_SZ);
		if (ret < 0 || ret > 40) {
			dev_err(&client->dev,
				"%s : Failed to read node result size(%d)\n", __func__, ret);
			goto out;
		}

		reg[0] = MMS_UNIVERSAL_RESULT;
		msg[0].len = 1;
		msg[1].len = ret;
		msg[1].buf = buf;

		if (i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg))
				!= ARRAY_SIZE(msg)) {
			dev_err(&client->dev, "%s : Failed to read result\n", __func__);
			ret = -EPERM;
			goto out;
		}

		ret >>= 1;

		if (univ_cmd == MMS_UNIV_CM_DELTA)
			ret -= 1;

		for (t = 0; t < ret; t++){
			data = (s16)(buf[2 * t] | buf[(2 * t) + 1] << 8);
			printk("[TSP] cm_delta[%d][%d] = %d\n",r, t, data);
			buff[(t * fac_data->rx_num) + r] = (s16)data;

			if (fac_data->max < data)
				fac_data->max = data;

			if (fac_data->min > data)
				fac_data->min = data;
		}
	}

#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
		print_raw_data(buff, ret, fac_data->rx_num);
#endif

	ret = 0;

out:
	return ret;
}

static void run_reference_read(void *device_data)
{
	struct mms_info *info = (struct mms_info *)device_data;
	struct mms_fac_data *fac_data = info->fac_data;
	struct i2c_client *client = info->client;
	int ret;

	set_default_result(fac_data);

	disable_irq(info->irq);

	ret = read_raw_data_all(info, MMS_UNIV_REFERENCE, fac_data->reference);
	if (ret < 0) {
		dev_err(&client->dev, "%s : Failed to read raw data\n", __func__);
		goto out;
	}

	enable_irq(info->irq);

	fac_data->cmd_state = CMD_STATUS_OK;
	sprintf(fac_data->cmd_buff, "%d,%d", fac_data->min, fac_data->max);
	set_cmd_result(fac_data, fac_data->cmd_buff, strlen(fac_data->cmd_buff));

	dev_info(&client->dev, "%s : %s\n", __func__, fac_data->cmd_buff);

	return;

out:
	enable_irq(info->irq);

	fac_data->cmd_state = CMD_STATUS_FAIL;
	sprintf(fac_data->cmd_buff, "%s", "NG");
	set_cmd_result(fac_data, fac_data->cmd_buff, strlen(fac_data->cmd_buff));

	return;
}

static void get_reference(void *device_data)
{
	struct mms_info *info = (struct mms_info *)device_data;
	struct mms_fac_data *fac_data = info->fac_data;
	struct i2c_client *client = info->client;
	int y = fac_data->cmd_param[0];
	int x = fac_data->cmd_param[1];
	int index;

	set_default_result(fac_data);

	if ( x < 0 || x >= fac_data->rx_num ||
		y < 0 || y >= fac_data->tx_num) {
		dev_err(&client->dev, "%s : Invaild index(%d, %d)\n", __func__, y, x);
		goto out;
	}

	index = y * fac_data->rx_num + x;

	fac_data->cmd_state = CMD_STATUS_OK;
	sprintf(fac_data->cmd_buff, "%d", fac_data->reference[index]);
	set_cmd_result(fac_data, fac_data->cmd_buff, strlen(fac_data->cmd_buff));

	dev_info(&client->dev, "%s : %s\n", __func__, fac_data->cmd_buff);

	return;

out:
	fac_data->cmd_state = CMD_STATUS_FAIL;
	sprintf(fac_data->cmd_buff, "%s", "NG");
	set_cmd_result(fac_data, fac_data->cmd_buff, strlen(fac_data->cmd_buff));

	return;
}

static void run_cm_delta_read(void *device_data)
{
	struct mms_info *info = (struct mms_info *)device_data;
	struct mms_fac_data *fac_data = info->fac_data;
	struct i2c_client *client = info->client;
	int ret;

	set_default_result(fac_data);

	ret = enter_test_mode(info);
	if (ret < 0) {
		dev_err(&client->dev, "%s : Failed to enter test mode\n", __func__);
		goto out;
	}

	ret = read_cm_data_all(info, MMS_UNIV_CM_DELTA, fac_data->delta);
	if (ret < 0) {
		dev_err(&client->dev, "%s : Failed to read cm data\n", __func__);
		goto out;
	}

	ret = exit_test_mode(info);
	if (ret < 0) {
	dev_err(&client->dev, "%s : Failed to exit test mode\n", __func__);
		goto out;
	}

	fac_data->cmd_state = CMD_STATUS_OK;
	sprintf(fac_data->cmd_buff, "%d,%d", fac_data->min, fac_data->max);
	set_cmd_result(fac_data, fac_data->cmd_buff, strlen(fac_data->cmd_buff));

	dev_info(&client->dev, "%s : %s\n", __func__, fac_data->cmd_buff);

	return;

out:
	fac_data->cmd_state = CMD_STATUS_FAIL;
	sprintf(fac_data->cmd_buff, "%s", "NG");
	set_cmd_result(fac_data, fac_data->cmd_buff, strlen(fac_data->cmd_buff));

	return;
}

static void get_cm_delta(void *device_data)
{
	struct mms_info *info = (struct mms_info *)device_data;
	struct mms_fac_data *fac_data = info->fac_data;
	struct i2c_client *client = info->client;
	int y = fac_data->cmd_param[0];
	int x = fac_data->cmd_param[1];
	int index;

	set_default_result(fac_data);

	if ( x < 0 || x >= fac_data->rx_num ||
		y < 0 || y >= fac_data->tx_num) {
		dev_err(&client->dev, "%s : Invaild index(%d, %d)\n", __func__, y, x);
		goto out;
	}

	index = y * fac_data->rx_num + x;

	fac_data->cmd_state = CMD_STATUS_OK;
	sprintf(fac_data->cmd_buff, "%d", fac_data->delta[index]);
	set_cmd_result(fac_data, fac_data->cmd_buff, strlen(fac_data->cmd_buff));

	dev_info(&client->dev, "%s : %s\n", __func__, fac_data->cmd_buff);

	return;

out:
	fac_data->cmd_state = CMD_STATUS_FAIL;
	sprintf(fac_data->cmd_buff, "%s", "NG");
	set_cmd_result(fac_data, fac_data->cmd_buff, strlen(fac_data->cmd_buff));

	return;
}

static void run_cm_abs_read(void *device_data)
{
	struct mms_info *info = (struct mms_info *)device_data;
	struct mms_fac_data *fac_data = info->fac_data;
	struct i2c_client *client = info->client;
	int ret;

	set_default_result(fac_data);

	ret = enter_test_mode(info);
	if (ret < 0) {
		dev_err(&client->dev, "%s : Failed to enter test mode\n", __func__);
		goto out;
	}

	ret = read_cm_data_all(info, MMS_UNIV_CM_ABS, fac_data->abs);
	if (ret < 0) {
		dev_err(&client->dev, "%s : Failed to read cm data\n", __func__);
		goto out;
	}

	ret = exit_test_mode(info);
	if (ret < 0) {
	dev_err(&client->dev, "%s : Failed to exit test mode\n", __func__);
		goto out;
	}

	fac_data->cmd_state = CMD_STATUS_OK;
	sprintf(fac_data->cmd_buff, "%d,%d", fac_data->min, fac_data->max);
	set_cmd_result(fac_data, fac_data->cmd_buff, strlen(fac_data->cmd_buff));

	dev_info(&client->dev, "%s : %s\n", __func__, fac_data->cmd_buff);

	return;

out:
	fac_data->cmd_state = CMD_STATUS_FAIL;
	sprintf(fac_data->cmd_buff, "%s", "NG");
	set_cmd_result(fac_data, fac_data->cmd_buff, strlen(fac_data->cmd_buff));

	return;
}

static void get_cm_abs(void *device_data)
{
	struct mms_info *info = (struct mms_info *)device_data;
	struct mms_fac_data *fac_data = info->fac_data;
	struct i2c_client *client = info->client;
	int y = fac_data->cmd_param[0];
	int x = fac_data->cmd_param[1];
	int index;

	set_default_result(fac_data);

	if ( x < 0 || x >= fac_data->rx_num ||
		y < 0 || y >= fac_data->tx_num) {
		dev_err(&client->dev, "%s : Invaild index(%d, %d)\n", __func__, y, x);
		goto out;
	}

	index = y * fac_data->rx_num + x;

	fac_data->cmd_state = CMD_STATUS_OK;
	sprintf(fac_data->cmd_buff, "%d", fac_data->abs[index]);
	set_cmd_result(fac_data, fac_data->cmd_buff, strlen(fac_data->cmd_buff));

	dev_info(&client->dev, "%s : %s\n", __func__, fac_data->cmd_buff);

	return;

out:
	fac_data->cmd_state = CMD_STATUS_FAIL;
	sprintf(fac_data->cmd_buff, "%s", "NG");
	set_cmd_result(fac_data, fac_data->cmd_buff, strlen(fac_data->cmd_buff));

	return;
}

static void run_intensity_read(void *device_data)
{
	struct mms_info *info = (struct mms_info *)device_data;
	struct mms_fac_data *fac_data = info->fac_data;
	struct i2c_client *client = info->client;
	int ret;

	set_default_result(fac_data);

	disable_irq(info->irq);

	ret = read_raw_data_all(info, MMS_UNIV_INTENSITY, fac_data->intensity);
	if (ret < 0) {
		dev_err(&client->dev, "%s : Failed to read raw data\n", __func__);
		goto out;
	}

	enable_irq(info->irq);

	fac_data->cmd_state = CMD_STATUS_OK;
	sprintf(fac_data->cmd_buff, "%d,%d", fac_data->min, fac_data->max);
	set_cmd_result(fac_data, fac_data->cmd_buff, strlen(fac_data->cmd_buff));

	dev_info(&client->dev, "%s : %s\n", __func__, fac_data->cmd_buff);

	return;

out:
	enable_irq(info->irq);

	fac_data->cmd_state = CMD_STATUS_FAIL;
	sprintf(fac_data->cmd_buff, "%s", "NG");
	set_cmd_result(fac_data, fac_data->cmd_buff, strlen(fac_data->cmd_buff));

	return;
}

static void get_intensity(void *device_data)
{
	struct mms_info *info = (struct mms_info *)device_data;
	struct mms_fac_data *fac_data = info->fac_data;
	struct i2c_client *client = info->client;
	int y = fac_data->cmd_param[0];
	int x = fac_data->cmd_param[1];
	int index;

	set_default_result(fac_data);

	if ( x < 0 || x >= fac_data->rx_num ||
		y < 0 || y >= fac_data->tx_num) {
		dev_err(&client->dev, "%s : Invaild index(%d, %d)\n", __func__, y, x);
		goto out;
	}

	index = y * fac_data->rx_num + x;

	fac_data->cmd_state = CMD_STATUS_OK;
	sprintf(fac_data->cmd_buff, "%d", fac_data->intensity[index]);
	set_cmd_result(fac_data, fac_data->cmd_buff, strlen(fac_data->cmd_buff));

	dev_info(&client->dev, "%s : %s\n", __func__, fac_data->cmd_buff);

	return;

out:
	fac_data->cmd_state = CMD_STATUS_FAIL;
	sprintf(fac_data->cmd_buff, "%s", "NG");
	set_cmd_result(fac_data, fac_data->cmd_buff, strlen(fac_data->cmd_buff));

	return;
}

static void not_support_cmd(void *device_data)
{
	struct mms_info *info = (struct mms_info *)device_data;
	struct mms_fac_data *fac_data = info->fac_data;
	struct i2c_client *client = info->client;

	set_default_result(fac_data);

	sprintf(fac_data->cmd_buff, "%s", "NA");
	set_cmd_result(fac_data, fac_data->cmd_buff, strlen(fac_data->cmd_buff));
	fac_data->cmd_state = CMD_STATUS_NOT_APPLICABLE;

	dev_info(&client->dev, "%s : %s\n", __func__, fac_data->cmd_buff);

	return;
}

#define TSP_CMD(name, func) .cmd_name = name, .cmd_func = func

struct tsp_cmd {
	struct list_head	list;
	const char			*cmd_name;
	void				(*cmd_func)(void *device_data);
};

struct tsp_cmd tsp_cmds[] = {
	{TSP_CMD("fw_update", fw_update),},
	{TSP_CMD("get_fw_ver_bin", get_fw_ver_bin),},
	{TSP_CMD("get_fw_ver_ic", get_fw_ver_ic),},
	{TSP_CMD("get_config_ver", not_support_cmd),},
	{TSP_CMD("get_threshold", get_threshold),},
	{TSP_CMD("module_off_master", module_off_master),},
	{TSP_CMD("module_on_master", module_on_master),},
	{TSP_CMD("module_off_slave", not_support_cmd),},
	{TSP_CMD("module_on_slave", not_support_cmd),},
	{TSP_CMD("get_chip_vendor", get_chip_vendor),},
	{TSP_CMD("get_chip_name", get_chip_name),},
	{TSP_CMD("get_x_num", get_x_num),},
	{TSP_CMD("get_y_num", get_y_num),},
	{TSP_CMD("run_reference_read", run_reference_read),},
	{TSP_CMD("get_reference", get_reference),},
	{TSP_CMD("run_inspection_read", run_cm_delta_read),},
	{TSP_CMD("get_inspection", get_cm_delta),},
	{TSP_CMD("run_cm_delta_read", run_cm_delta_read),},
	{TSP_CMD("get_cm_delta", get_cm_delta),},
	{TSP_CMD("run_cm_abs_read", run_cm_abs_read),},
	{TSP_CMD("get_cm_abs", get_cm_abs),},
	{TSP_CMD("run_intensity_read", run_intensity_read),},
	{TSP_CMD("get_intensity", get_intensity),},
	{TSP_CMD("not_support_cmd", not_support_cmd),},
};

static ssize_t store_cmd(struct device *dev, struct device_attribute *devattr,
						const char *buf, size_t count)
{
	struct mms_info *info = dev_get_drvdata(dev);
	struct mms_fac_data *fac_data = info->fac_data;
	struct i2c_client *client = info->client;
	char *cur, *start, *end;
	char buff[TSP_CMD_STR_LEN] = {0, };
	int len, i;
	struct tsp_cmd *tsp_cmd_ptr = NULL;
	char delim = ',';
	bool cmd_found = false;
	int param_cnt = 0;

	if (!info->enabled) {
		dev_err(&client->dev, "%s: device is disabled\n", __func__);
		goto err_out;
	}

	if (fac_data->cmd_is_running) {
		dev_err(&client->dev, "%s: other cmd is running\n", __func__);
		goto err_out;
	}

	/* check lock  */
	mutex_lock(&fac_data->cmd_lock);
	fac_data->cmd_is_running = true;
	mutex_unlock(&fac_data->cmd_lock);

	fac_data->cmd_state = CMD_STATUS_RUNNING;

	for (i = 0; i < ARRAY_SIZE(fac_data->cmd_param); i++)
		fac_data->cmd_param[i] = 0;

	len = (int)count;

	if (*(buf + len - 1) == '\n')
		len--;

	memset(fac_data->cmd, 0x00, ARRAY_SIZE(fac_data->cmd));
	memcpy(fac_data->cmd, buf, len);

	cur = strchr(buf, (int)delim);
	if (cur)
		memcpy(buff, buf, cur - buf);
	else
		memcpy(buff, buf, len);

	/* find command */
	list_for_each_entry(tsp_cmd_ptr, &fac_data->cmd_list_head, list) {
		if (!strcmp(buff, tsp_cmd_ptr->cmd_name)) {
			cmd_found = true;
			break;
		}
	}

	/* set not_support_cmd */
	if (!cmd_found) {
		list_for_each_entry(tsp_cmd_ptr, &fac_data->cmd_list_head, list) {
			if (!strcmp("not_support_cmd", tsp_cmd_ptr->cmd_name))
				break;
		}
	}

	/* parsing TSP standard tset parameter */
	if (cur && cmd_found) {
		cur++;
		start = cur;

		do {
			if (*cur == delim || cur - buf == len) {
				memset(buff, 0x00, ARRAY_SIZE(buff));
				end = cur;
				memcpy(buff, start, end - start);
				*(buff + strlen(buff)) = '\0';
				kstrtol(buff, 10, fac_data->cmd_param + param_cnt);
				start = cur + 1;
				param_cnt++;
			}
			cur++;
		} while (cur - buf <= len);
	}

	dev_info(&client->dev, "cmd = %s\n", tsp_cmd_ptr->cmd_name);

	for (i = 0; i < param_cnt; i++)
		dev_info(&client->dev, "cmd param %d = %d\n",
			i, fac_data->cmd_param[i]);

	tsp_cmd_ptr->cmd_func(info);

err_out:
	return count;
}

static ssize_t show_cmd_status(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mms_info *info = dev_get_drvdata(dev);
	struct mms_fac_data *fac_data = info->fac_data;
	struct i2c_client *client = info->client;
	char buff[16] = {0,};

	dev_info(&client->dev, "cmd status %d\n", fac_data->cmd_state);

	switch (fac_data->cmd_state) {
	case CMD_STATUS_WAITING:
		sprintf(buff, "%s", "WAITING");
		break;
	case CMD_STATUS_RUNNING:
		sprintf(buff, "%s", "RUNNING");
		break;
	case CMD_STATUS_OK:
		sprintf(buff, "%s", "OK");
		break;
	case CMD_STATUS_FAIL:
		sprintf(buff, "%s", "FAIL");
		break;
	case CMD_STATUS_NOT_APPLICABLE:
		sprintf(buff, "%s", "NOT_APPLICABLE");
		break;
	default:
		sprintf(buff, "%s", "NOT_APPLICABLE");
		break;
	}

	return sprintf(buf, "%s\n", buff);
}

static ssize_t show_cmd_result(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mms_info *info = dev_get_drvdata(dev);
	struct mms_fac_data *fac_data = info->fac_data;
	struct i2c_client *client = info->client;

	dev_info(&client->dev, "cmd result %s\n", fac_data->cmd_result);

	mutex_lock(&fac_data->cmd_lock);
	fac_data->cmd_is_running = false;
	mutex_unlock(&fac_data->cmd_lock);

	fac_data->cmd_state = CMD_STATUS_WAITING;

	return sprintf(buf, "%s\n", fac_data->cmd_result);
}

static DEVICE_ATTR(cmd, S_IWUSR | S_IWGRP, NULL, store_cmd);
static DEVICE_ATTR(cmd_status, S_IRUGO, show_cmd_status, NULL);
static DEVICE_ATTR(cmd_result, S_IRUGO, show_cmd_result, NULL);

static struct attribute *touchscreen_attributes[] = {
	&dev_attr_cmd.attr,
	&dev_attr_cmd_status.attr,
	&dev_attr_cmd_result.attr,
	NULL,
};

static struct attribute_group touchscreen_attr_group = {
	.attrs = touchscreen_attributes,
};

#if MMS_HAS_TOUCH_KEY
static ssize_t show_intensity_key(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mms_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;
	u8 reg[4] = {0, };
	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.buf = reg,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
		},
	};
	int i, ret;
	u8 buff[4] = {0, };

	disable_irq(info->irq);

	if (!strcmp(attr->attr.name, "touchkey_recent"))
		i = 0;
	else if (!strcmp(attr->attr.name, "touchkey_back"))
		i = 1;
	else {
		dev_err(&client->dev, "%s : Invalid attribute\n");
		goto out;
	}

	reg[0] = MMS_UNIVERSAL_CMD;
	reg[1] = MMS_UNIV_INTENSITY_KEY;
	reg[2] = 0xFF;
	reg[3] = 0x00;
	msg[0].len = 4;

	if (i2c_transfer(client->adapter, &msg[0], 1) != 1) {
		dev_err(&client->dev, "%s : Failed to send cmd 0x%x\n",
					__func__, MMS_UNIV_INTENSITY_KEY);
		ret = -EPERM;
		goto out;
	}

	while (gpio_get_value(info->pdata->gpio_int));

	ret = i2c_smbus_read_byte_data(client, MMS_UNIVERSAL_RESULT_SZ);
	if (ret < 0 || ret > 4) {
		dev_err(&client->dev, "%s : Failed to read key result size(%d)\n",
					__func__, ret);
		goto out;
	}

	reg[0] = MMS_UNIVERSAL_RESULT;
	msg[0].len = 1;
	msg[1].len = ret;
	msg[1].buf = buff;

	if (i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg))
		!= ARRAY_SIZE(msg)) {
		ret = -EPERM;
		goto out;
	}

	ret = (s16)(buff[2 * i] | (buff[(2 * i) + 1] << 8));

	enable_irq(info->irq);

	dev_info(&client->dev, "%s intensity %d\n", __func__,
				attr->attr.name, ret);

	return snprintf(buf, 5, "%d\n", ret);

out:
	enable_irq(info->irq);

	return sprintf(buf, "NG");
}

/* sysfs: /sys/class/sec/tsp/sec_touchkey/touchkey_threshold */
static ssize_t show_touchkey_threshold(struct device *dev, struct device_attribute *attr, char *buf)
{
	int threshold;

	threshold = 22;

	printk("%s(), %d\n", __func__, threshold);

	return snprintf(buf, sizeof(int), "%d\n", threshold);
}

static ssize_t debug_intensity_key(void)
{
	struct i2c_client *client = mms_touch_info->client;
	u8 reg[4] = {0, };
	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.buf = reg,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
		},
	};
	int i, ret;
	u8 buff[4] = {0, };

	disable_irq(mms_touch_info->irq);

	reg[0] = MMS_UNIVERSAL_CMD;
	reg[1] = MMS_UNIV_INTENSITY_KEY;
	reg[2] = 0xFF;
	reg[3] = 0x00;
	msg[0].len = 4;

	if (i2c_transfer(client->adapter, &msg[0], 1) != 1) {
		dev_err(&client->dev, "%s : Failed to send cmd 0x%x\n",
					__func__, MMS_UNIV_INTENSITY_KEY);
		ret = -EPERM;
		goto out;
	}

	while (gpio_get_value(mms_touch_info->pdata->gpio_int));

	ret = i2c_smbus_read_byte_data(client, MMS_UNIVERSAL_RESULT_SZ);
	if (ret < 0 || ret > 4) {
		dev_err(&client->dev, "%s : Failed to read key result size(%d)\n",
					__func__, ret);
		goto out;
	}

	reg[0] = MMS_UNIVERSAL_RESULT;
	msg[0].len = 1;
	msg[1].len = ret;
	msg[1].buf = buff;

	if (i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg))
		!= ARRAY_SIZE(msg)) {
		ret = -EPERM;
		goto out;
	}

	i=0;
	ret = (s16)(buff[2 * i] | (buff[(2 * i) + 1] << 8));
	printk("recent_key intensity %d\n", ret);

	i=1;
	ret = (s16)(buff[2 * i] | (buff[(2 * i) + 1] << 8));
	printk("back_key intensity %d\n", ret);

out:
	enable_irq(mms_touch_info->irq);

	return 0;
}


int get_tsp_debug_info(void)
{
	int ret;
	struct mms_fac_data *fac_data = mms_touch_info->fac_data;

	ret = read_raw_data_all(mms_touch_info, MMS_UNIV_INTENSITY, fac_data->reference);
		if (ret < 0) {
			printk("%s : Failed to read raw data\n", __func__);
			return ret;
		}
	debug_intensity_key();
	return 0;
}

static DEVICE_ATTR(touchkey_threshold, S_IRUGO, show_touchkey_threshold, NULL);
static DEVICE_ATTR(touchkey_recent, S_IRUGO, show_intensity_key, NULL);
static DEVICE_ATTR(touchkey_back, S_IRUGO, show_intensity_key, NULL);

static struct attribute *touchkey_attributes[] = {
	&dev_attr_touchkey_recent.attr,
	&dev_attr_touchkey_back.attr,
	&dev_attr_touchkey_threshold.attr,
	NULL,
};

static struct attribute_group touchkey_attr_group = {
	.attrs = touchkey_attributes,
};
#endif

static int config_sec(struct mms_info *info)
{
	struct mms_fac_data *fac_data = info->fac_data;
	struct i2c_client *client = info->client;
	int ret = 0;
	u16 total_node;

	ret = i2c_smbus_read_byte_data(client, MMS_TX_NUM);
	if (ret <= 0) {
		dev_err(&client->dev, "Failed to read xnode num(%d)\n", ret);
		goto out;
	}
	fac_data->tx_num = ret;

	ret = i2c_smbus_read_byte_data(client, MMS_RX_NUM);
	if (ret <= 0) {
		dev_err(&client->dev, "Failed to read ynode num(%d)\n", ret);
		goto out;
	}
	fac_data->rx_num = ret;

	total_node = fac_data->rx_num * fac_data->tx_num;

#if MMS_HAS_TOUCH_KEY
	ret = i2c_smbus_read_byte_data(client, MMS_KEY_NUM);
	if (ret <= 0) {
		dev_err(&client->dev, "Failed to read keynode num(%d)\n", ret);
		goto out;
	}
	fac_data->key_num = ret;
#endif

	fac_data->reference = kzalloc((total_node - 1) * sizeof(s16), GFP_KERNEL);
	if (!fac_data->reference) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		ret = -ENOMEM;
		goto out;
	}

	fac_data->delta = kzalloc((total_node - 1) * sizeof(s16), GFP_KERNEL);
	if (!fac_data->delta) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		goto err_mem2;
	}

	fac_data->abs = kzalloc(total_node * sizeof(s16), GFP_KERNEL);
	if (!fac_data->abs) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		goto err_mem3;
	}

	fac_data->intensity = kzalloc(
							(total_node - 1) * sizeof(s16), GFP_KERNEL);
	if (!fac_data->intensity) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		goto err_mem4;
	}

	return 0;

err_mem4:
	kfree(fac_data->abs);
err_mem3:
	kfree(fac_data->delta);
err_mem2:
	kfree(fac_data->reference);
	ret = -ENOMEM;
out:
	return ret;
}

static int init_sec(struct mms_info *info)
{
	struct i2c_client *client = info->client;
	struct mms_fac_data *fac_data;
	int i;
	int ret;

	fac_data = kzalloc(sizeof(struct mms_fac_data), GFP_KERNEL);
	if (!fac_data) {
		dev_err(&client->dev, "Failed to allocate factory data memory\n");
		return -ENOMEM;
	}

	info->fac_data = fac_data;
	mms_touch_info = info;

	mutex_init(&fac_data->cmd_lock);
	fac_data->cmd_is_running = false;

	INIT_LIST_HEAD(&fac_data->cmd_list_head);
	for (i = 0; i < ARRAY_SIZE(tsp_cmds); i++)
		list_add_tail(&tsp_cmds[i].list, &fac_data->cmd_list_head);

	fac_data->fac_tsp_dev = device_create(
							sec_class, NULL, (dev_t)NULL, info, "tsp");
	if (IS_ERR(fac_data->fac_tsp_dev)) {
		ret = PTR_ERR(fac_data->fac_tsp_dev);
		dev_err(&client->dev,
			"Failed to create device for touch screen(%d)\n", ret);
		ret = -ENODEV;
		goto err_create_dev_ts;
	}

	ret = sysfs_create_group(
				&fac_data->fac_tsp_dev->kobj, &touchscreen_attr_group);
	if (ret) {
		dev_err(&client->dev,
			"Failed to create sysfs for touch screen(%d)\n", ret);
		ret = (ret > 0) ? -ret : ret;
		goto err_create_ts_sysfs;
	}

#if MMS_HAS_TOUCH_KEY
	fac_data->fac_key_dev = device_create(
						sec_class, NULL, (dev_t)NULL, info, "sec_touchkey");
	if (IS_ERR(fac_data->fac_key_dev)) {
		ret = PTR_ERR(fac_data->fac_key_dev);
		dev_err(&client->dev,
			"Failed to create device for touch key(%d)\n", ret);
		ret = -ENODEV;
		goto err_create_dev_tk;
	}

	ret = sysfs_create_group(
				&fac_data->fac_key_dev->kobj, &touchkey_attr_group);
	if (ret) {
		dev_err(&client->dev,
			"Failed to create sysfs for touch key(%d)\n", ret);
		ret = (ret > 0) ? -ret : ret;
		goto err_create_tk_sysfs;
	}
#endif

	ret = config_sec(info);
	if (ret) {
		dev_err(&client->dev, "Failed to read node number(%d)\n", ret);
		goto err_config_sec;
	}

	return 0;

err_config_sec:
#if MMS_HAS_TOUCH_KEY
	sysfs_remove_group(
			&info->fac_data->fac_key_dev->kobj, &touchkey_attr_group);
err_create_tk_sysfs:
	device_destroy(sec_class, (dev_t)NULL);
err_create_dev_tk:
	sysfs_remove_group(
			&info->fac_data->fac_tsp_dev->kobj, &touchscreen_attr_group);
#endif
err_create_ts_sysfs:
	device_destroy(sec_class, (dev_t)NULL);
err_create_dev_ts:
	kfree(fac_data);

	return ret;
}

static void destroy_sec(struct mms_info *info)
{
#if MMS_HAS_TOUCH_KEY
	sysfs_remove_group(
			&info->fac_data->fac_tsp_dev->kobj, &touchkey_attr_group);
	device_destroy(sec_class, (dev_t)NULL);
#endif
	sysfs_remove_group(
			&info->fac_data->fac_tsp_dev->kobj, &touchscreen_attr_group);
	device_destroy(sec_class, (dev_t)NULL);
	kfree(info->fac_data);
}

