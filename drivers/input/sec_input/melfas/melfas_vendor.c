/*
 * MELFAS MMS400 Touchscreen
 *
 * Copyright (C) 2014 MELFAS Inc.
 *
 *
 * Test Functions (Optional)
 *
 */

#include "melfas_dev.h"

#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP

/**
 * Dev node output to user
 */
static ssize_t melfas_ts_dev_fs_read(struct file *fp, char *rbuf, size_t cnt, loff_t *fpos)
{
	struct melfas_ts_data *ts = fp->private_data;
	int ret = 0;

	ret = copy_to_user(rbuf, ts->dev_fs_buf, cnt);
	return ret;
}

/**
 * Dev node input from user
 */
static ssize_t melfas_ts_dev_fs_write(struct file *fp, const char *wbuf, size_t cnt, loff_t *fpos)
{
	struct melfas_ts_data *ts = fp->private_data;
	u8 *buf;
	int ret = 0;
	int cmd = 0;

	buf = kzalloc(cnt + 1, GFP_KERNEL);

	if ((buf == NULL) || copy_from_user(buf, wbuf, cnt)) {
		input_err(true, &ts->client->dev, "%s [ERROR] copy_from_user\n", __func__);
		ret = -EIO;
		goto EXIT;
	}

	cmd = buf[cnt - 1];

	if (cmd == 1) {
		ret = ts->melfas_ts_i2c_read(ts, buf, (cnt - 2), ts->dev_fs_buf, buf[cnt - 2]);
		if (ret < 0)
			input_err(true, &ts->client->dev, "%s [ERROR] melfas_ts_i2c_read\n", __func__);
	} else if (cmd == 2) {
		ret = ts->melfas_ts_i2c_write(ts, buf, (cnt - 1), NULL, 0);
		if (ret < 0)
			input_err(true, &ts->client->dev, "%s [ERROR] melfas_ts_i2c_write\n", __func__);
	}

EXIT:
	kfree(buf);

	return ret;
}

/**
 * Open dev node
 */
static int melfas_ts_dev_fs_open(struct inode *node, struct file *fp)
{
	struct melfas_ts_data *ts = container_of(node->i_cdev, struct melfas_ts_data, cdev);

	fp->private_data = ts;
	ts->dev_fs_buf = kzalloc(1024 * 4, GFP_KERNEL);
	return 0;
}

/**
 * Close dev node
 */
static int melfas_ts_dev_fs_release(struct inode *node, struct file *fp)
{
	struct melfas_ts_data *ts = fp->private_data;

	kfree(ts->dev_fs_buf);
	return 0;
}

/**
 * Dev node info
 */
static struct file_operations melfas_ts_dev_fops = {
	.owner	= THIS_MODULE,
	.open	= melfas_ts_dev_fs_open,
	.release	= melfas_ts_dev_fs_release,
	.read	= melfas_ts_dev_fs_read,
	.write	= melfas_ts_dev_fs_write,
};

/**
 * Create dev node
 */
int melfas_ts_dev_create(struct melfas_ts_data *ts)
{
	struct i2c_client *client = ts->client;
	int ret = 0;

	input_info(true, &ts->client->dev, "%s [START]\n", __func__);

	if (alloc_chrdev_region(&ts->melfas_dev, 0, 1, MELFAS_TS_DEVICE_NAME)) {
		input_err(true, &client->dev, "%s [ERROR] alloc_chrdev_region\n", __func__);
		ret = -ENOMEM;
		goto ERROR;
	}

	cdev_init(&ts->cdev, &melfas_ts_dev_fops);
	ts->cdev.owner = THIS_MODULE;

	if (cdev_add(&ts->cdev, ts->melfas_dev, 1)) {
		input_err(true, &client->dev, "%s [ERROR] cdev_add\n", __func__);
		ret = -EIO;
		goto ERROR;
	}

	input_info(true, &ts->client->dev, "%s [DONE]\n", __func__);
	return 0;

ERROR:
	input_err(true, &ts->client->dev, "%s [ERROR]\n", __func__);
	return ret;
}
/**
 * Print chip firmware version
 */
static ssize_t melfas_ts_sys_fw_version(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);
	u8 data[255];
	int ret;
	u8 rbuf[16];

	memset(ts->print_buf, 0, PAGE_SIZE);

	if (melfas_ts_get_fw_version(ts, rbuf)) {
		input_err(true, &ts->client->dev, "%s [ERROR] melfas_ts_get_fw_version\n", __func__);

		sprintf(data, "F/W Version : ERROR\n");
		goto ERROR;
	}

	input_info(true, &ts->client->dev,
		"%s - F/W Version : %02X.%02X %02X.%02X %02X.%02X %02X.%02X\n",
		__func__, rbuf[0], rbuf[1], rbuf[2], rbuf[3],
		rbuf[4], rbuf[5], rbuf[6], rbuf[7]);
	sprintf(data, "F/W Version : %02X.%02X %02X.%02X %02X.%02X %02X.%02X\n",
		rbuf[0], rbuf[1], rbuf[2], rbuf[3], rbuf[4], rbuf[5], rbuf[6], rbuf[7]);

ERROR:
	strcat(ts->print_buf, data);

	ret = snprintf(buf, PAGE_SIZE, "%s\n", ts->print_buf);
	return ret;
}

/**
 * Print channel info
 */
static ssize_t melfas_ts_sys_info(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);
	u8 data[255];
	int ret;
	u8 rbuf[32];
	u8 wbuf[8];
	int res_x, res_y;

	memset(ts->print_buf, 0, PAGE_SIZE);

	sprintf(data, "\n");
	strcat(ts->print_buf, data);

	melfas_ts_get_fw_version(ts, rbuf);
	sprintf(data, "F/W Version : %02X.%02X %02X.%02X %02X.%02X %02X.%02X\n",
		rbuf[0], rbuf[1], rbuf[2], rbuf[3], rbuf[4], rbuf[5], rbuf[6], rbuf[7]);
	strcat(ts->print_buf, data);

	wbuf[0] = MELFAS_TS_R0_INFO;
	wbuf[1] = MELFAS_TS_R1_INFO_PRODUCT_NAME;
	ts->melfas_ts_i2c_read(ts, wbuf, 2, rbuf, 16);
	sprintf(data, "Product Name : %s\n", rbuf);
	strcat(ts->print_buf, data);

	wbuf[0] = MELFAS_TS_R0_INFO;
	wbuf[1] = MELFAS_TS_R1_INFO_RESOLUTION_X;
	ts->melfas_ts_i2c_read(ts, wbuf, 2, rbuf, 7);
	res_x = (rbuf[0]) | (rbuf[1] << 8);
	res_y = (rbuf[2]) | (rbuf[3] << 8);
	sprintf(data, "Resolution : X[%d] Y[%d]\n", res_x, res_y);
	strcat(ts->print_buf, data);

	sprintf(data, "Node Num : X[%d] Y[%d] Key[%d]\n", rbuf[4], rbuf[5], rbuf[6]);
	strcat(ts->print_buf, data);

	ret = snprintf(buf, PAGE_SIZE, "%s\n", ts->print_buf);
	return ret;

}

/**
 * Device enable
 */
static ssize_t melfas_ts_sys_device_enable(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);
	struct i2c_client *client = ts->client;
	u8 data[255];
	int ret;

	memset(ts->print_buf, 0, PAGE_SIZE);

	ts->plat_data->start_device(ts);

	input_info(true, &client->dev, "%s", __func__);

	sprintf(data, "Device : Enabled\n");
	strcat(ts->print_buf, data);

	ret = snprintf(buf, PAGE_SIZE, "%s\n", ts->print_buf);
	return ret;

}

/**
 * Device disable
 */
static ssize_t melfas_ts_sys_device_disable(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);
	struct i2c_client *client = ts->client;
	u8 data[255];
	int ret;

	memset(ts->print_buf, 0, PAGE_SIZE);

	ts->plat_data->stop_device(ts);

	input_info(true, &client->dev, "%s", __func__);

	sprintf(data, "Device : Disabled\n");
	strcat(ts->print_buf, data);

	ret = snprintf(buf, PAGE_SIZE, "%s\n", ts->print_buf);
	return ret;
}

/**
 * Enable IRQ
 */
static ssize_t melfas_ts_sys_irq_enable(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);
	struct i2c_client *client = ts->client;
	u8 data[255];
	int ret;

	memset(ts->print_buf, 0, PAGE_SIZE);

	enable_irq(ts->client->irq);

	input_info(true, &client->dev, "%s\n", __func__);

	sprintf(data, "IRQ : Enabled\n");
	strcat(ts->print_buf, data);

	ret = snprintf(buf, PAGE_SIZE, "%s\n", ts->print_buf);
	return ret;
}

/**
 * Disable IRQ
 */
static ssize_t melfas_ts_sys_irq_disable(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);
	struct i2c_client *client = ts->client;
	u8 data[255];
	int ret;

	memset(ts->print_buf, 0, PAGE_SIZE);

	disable_irq(ts->client->irq);
	melfas_ts_locked_release_all_finger(ts);

	input_info(true, &client->dev, "%s\n", __func__);

	sprintf(data, "IRQ : Disabled\n");
	strcat(ts->print_buf, data);

	ret = snprintf(buf, PAGE_SIZE, "%s\n", ts->print_buf);
	return ret;
}

/**
 * Power on
 */
static ssize_t melfas_ts_sys_power_on(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);
	struct i2c_client *client = ts->client;
	u8 data[255];
	int ret;

	memset(ts->print_buf, 0, PAGE_SIZE);

	ts->plat_data->power(&client->dev, 1);

	input_info(true, &client->dev, "%s", __func__);

	sprintf(data, "Power : On\n");
	strcat(ts->print_buf, data);

	ret = snprintf(buf, PAGE_SIZE, "%s\n", ts->print_buf);
	return ret;
}

/**
 * Power off
 */
static ssize_t melfas_ts_sys_power_off(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);
	struct i2c_client *client = ts->client;
	u8 data[255];
	int ret;

	memset(ts->print_buf, 0, PAGE_SIZE);

	ts->plat_data->power(&client->dev, 0);

	input_info(true, &client->dev, "%s", __func__);

	sprintf(data, "Power : Off\n");
	strcat(ts->print_buf, data);

	ret = snprintf(buf, PAGE_SIZE, "%s\n", ts->print_buf);
	return ret;
}

/**
 * Reboot chip
 */
static ssize_t melfas_ts_sys_reboot(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);
	struct i2c_client *client = ts->client;
	u8 data[255];
	int ret;

	memset(ts->print_buf, 0, PAGE_SIZE);

	input_info(true, &client->dev, "%s", __func__);

	disable_irq(ts->client->irq);
	melfas_ts_locked_release_all_finger(ts);
	melfas_ts_reset(ts, 10);
	enable_irq(ts->client->irq);

	sprintf(data, "Reboot\n");
	strcat(ts->print_buf, data);

	ret = snprintf(buf, PAGE_SIZE, "%s\n", ts->print_buf);
	return ret;

}

/**
 * Set glove mode
 */
static ssize_t melfas_ts_sys_glove_mode_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);
	u8 wbuf[8];
	int ret = 0;
	input_info(true, &ts->client->dev, "%s [START]\n", __func__);

	wbuf[0] = MELFAS_TS_R0_CTRL;
	wbuf[1] = MELFAS_TS_R1_CTRL_GLOVE_MODE;
	wbuf[2] = buf[0];

	if ((buf[0] == 0) || (buf[0] == 1)) {
		ret = ts->melfas_ts_i2c_write(ts, wbuf, 2, &wbuf[2], 1);
		if (ret < 0)
			input_err(true, &ts->client->dev, "%s [ERROR] melfas_ts_i2c_write\n", __func__);
		else
			input_info(true, &ts->client->dev, "%s - value[%d]\n", __func__, buf[0]);
	} else
		input_err(true, &ts->client->dev, "%s [ERROR] Unknown value\n", __func__);

	input_info(true, &ts->client->dev, "%s [DONE]\n", __func__);

	return count;
}

/**
 * Get glove mode
 */
static ssize_t melfas_ts_sys_glove_mode_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);
	u8 data[255];
	int ret;
	u8 wbuf[8];
	u8 rbuf[4];

	memset(ts->print_buf, 0, PAGE_SIZE);

	input_info(true, &ts->client->dev, "%s [START]\n", __func__);

	wbuf[0] = MELFAS_TS_R0_CTRL;
	wbuf[1] = MELFAS_TS_R1_CTRL_GLOVE_MODE;
	ret = ts->melfas_ts_i2c_read(ts, wbuf, 2, rbuf, 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s [ERROR] melfas_ts_i2c_read\n", __func__);
		sprintf(data, "\nGlove Mode : ERROR\n");
	} else {
		input_info(true, &ts->client->dev, "%s - value[%d]\n", __func__, rbuf[0]);
		sprintf(data, "\nGlove Mode : %d\n", rbuf[0]);
	}

	input_info(true, &ts->client->dev, "%s [DONE]\n", __func__);

	strcat(ts->print_buf, data);
	ret = snprintf(buf, PAGE_SIZE, "%s\n", ts->print_buf);
	return ret;
}

/**
 * Set charger mode
 */
static ssize_t melfas_ts_sys_charger_mode_store(struct device *dev,
			struct device_attribute *attr, const char *buf, size_t count)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);
	u8 wbuf[8];
	int ret = 0;
	input_info(true, &ts->client->dev, "%s [START]\n", __func__);

	wbuf[0] = MELFAS_TS_R0_CTRL;
	wbuf[1] = MELFAS_TS_R1_CTRL_CHARGER_MODE;
	wbuf[2] = buf[0];

	if ((buf[0] == 0) || (buf[0] == 1)) {
		ret = ts->melfas_ts_i2c_write(ts, wbuf, 2, &wbuf[2], 1);
		if (ret < 0)
			input_err(true, &ts->client->dev, "%s [ERROR] melfas_ts_i2c_write\n", __func__);
		else
			input_info(true, &ts->client->dev, "%s - value[%d]\n", __func__, buf[0]);
	} else
		input_err(true, &ts->client->dev, "%s [ERROR] Unknown value\n", __func__);

	input_info(true, &ts->client->dev, "%s [DONE]\n", __func__);

	return count;
}

/**
 * Get charger mode
 */
static ssize_t melfas_ts_sys_charger_mode_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);
	u8 data[255];
	int ret;
	u8 wbuf[8];
	u8 rbuf[4];

	memset(ts->print_buf, 0, PAGE_SIZE);

	input_info(true, &ts->client->dev, "%s [START]\n", __func__);

	wbuf[0] = MELFAS_TS_R0_CTRL;
	wbuf[1] = MELFAS_TS_R1_CTRL_CHARGER_MODE;
	ret = ts->melfas_ts_i2c_read(ts, wbuf, 2, rbuf, 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s [ERROR] melfas_ts_i2c_read\n", __func__);
		sprintf(data, "\nCharger Mode : ERROR\n");
	} else {
		input_info(true, &ts->client->dev, "%s - value[%d]\n", __func__, rbuf[0]);
		sprintf(data, "\nCharger Mode : %d\n", rbuf[0]);
	}

	input_info(true, &ts->client->dev, "%s [DONE]\n", __func__);

	strcat(ts->print_buf, data);
	ret = snprintf(buf, PAGE_SIZE, "%s\n", ts->print_buf);
	return ret;
}

/**
 * Set cover window mode
 */
static ssize_t melfas_ts_sys_window_mode_store(struct device *dev,
			struct device_attribute *attr, const char *buf, size_t count)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);
	u8 wbuf[8];
	int ret = 0;

	input_info(true, &ts->client->dev, "%s [START]\n", __func__);

	wbuf[0] = MELFAS_TS_R0_CTRL;
	wbuf[1] = MELFAS_TS_R1_CTRL_COVER_MODE;
	wbuf[2] = buf[0];

	if ((buf[0] == 0) || (buf[0] == 1)) {
		ret = ts->melfas_ts_i2c_write(ts, wbuf, 2, &wbuf[2], 1);
		if (ret < 0)
			input_err(true, &ts->client->dev, "%s [ERROR] melfas_ts_i2c_write\n", __func__);
		else
			input_info(true, &ts->client->dev, "%s - value[%d]\n", __func__, buf[0]);
	} else
		input_err(true, &ts->client->dev, "%s [ERROR] Unknown value\n", __func__);

	input_info(true, &ts->client->dev, "%s [DONE]\n", __func__);

	return count;
}

/**
 * Get cover window mode
 */
static ssize_t melfas_ts_sys_window_mode_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);
	u8 data[255];
	int ret;
	u8 wbuf[8];
	u8 rbuf[4];

	memset(ts->print_buf, 0, PAGE_SIZE);

	input_info(true, &ts->client->dev, "%s [START]\n", __func__);

	wbuf[0] = MELFAS_TS_R0_CTRL;
	wbuf[1] = MELFAS_TS_R1_CTRL_COVER_MODE;
	ret = ts->melfas_ts_i2c_read(ts, wbuf, 2, rbuf, 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s [ERROR] melfas_ts_i2c_read\n", __func__);
		sprintf(data, "\nWindow Mode : ERROR\n");
	} else {
		input_info(true, &ts->client->dev, "%s - value[%d]\n", __func__, rbuf[0]);
		sprintf(data, "\nWindow Mode : %d\n", rbuf[0]);
	}

	input_info(true, &ts->client->dev, "%s [DONE]\n", __func__);

	strcat(ts->print_buf, data);
	ret = snprintf(buf, PAGE_SIZE, "%s\n", ts->print_buf);
	return ret;
}

/**
 * Set palm rejection mode
 */
static ssize_t melfas_ts_sys_palm_rejection_mode_store(struct device *dev,
			struct device_attribute *attr, const char *buf, size_t count)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);
	u8 wbuf[8];
	int ret = 0;

	input_info(true, &ts->client->dev, "%s [START]\n", __func__);

	wbuf[0] = MELFAS_TS_R0_CTRL;
	wbuf[1] = MELFAS_TS_R1_CTRL_PALM_REJECTION;
	wbuf[2] = buf[0];

	if ((buf[0] == 0) || (buf[0] == 1)) {
		ret = ts->melfas_ts_i2c_write(ts, wbuf, 2, &wbuf[2], 1);
		if (ret < 0)
			input_err(true, &ts->client->dev, "%s [ERROR] melfas_ts_i2c_write\n", __func__);
		else
			input_info(true, &ts->client->dev, "%s - value[%d]\n", __func__, buf[0]);
	} else
		input_err(true, &ts->client->dev, "%s [ERROR] Unknown value\n", __func__);

	input_info(true, &ts->client->dev, "%s [DONE]\n", __func__);

	return count;
}

/**
 * Get palm rejection mode
 */
static ssize_t melfas_ts_sys_palm_rejection_mode_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);
	u8 data[255];
	int ret;
	u8 wbuf[8];
	u8 rbuf[4];

	memset(ts->print_buf, 0, PAGE_SIZE);

	input_info(true, &ts->client->dev, "%s [START]\n", __func__);

	wbuf[0] = MELFAS_TS_R0_CTRL;
	wbuf[1] = MELFAS_TS_R1_CTRL_PALM_REJECTION;
	ret = ts->melfas_ts_i2c_read(ts, wbuf, 2, rbuf, 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s [ERROR] melfas_ts_i2c_read\n", __func__);
		sprintf(data, "\nPalm Rejection Mode : ERROR\n");
	} else {
		input_info(true, &ts->client->dev, "%s - value[%d]\n", __func__, rbuf[0]);
		sprintf(data, "\nPalm Rejection Mode : %d\n", rbuf[0]);
	}

	input_info(true, &ts->client->dev, "%s [DONE]\n", __func__);

	strcat(ts->print_buf, data);
	ret = snprintf(buf, PAGE_SIZE, "%s\n", ts->print_buf);
	return ret;
}

/**
 * Sysfs print intensity image
 */
static ssize_t melfas_ts_sys_intensity(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);
	int ret;

	input_info(true, &ts->client->dev, "%s [START]\n", __func__);

	if (melfas_ts_get_image(ts, MELFAS_TS_IMG_TYPE_INTENSITY)) {
		input_err(true, &ts->client->dev, "%s [ERROR] melfas_ts_get_image\n", __func__);
		return -1;
	}

	input_info(true, &ts->client->dev, "%s [DONE]\n", __func__);

	ret = snprintf(buf, PAGE_SIZE, "%s\n", ts->print_buf);
	return ret;
}

/**
 * Sysfs print rawdata image
 */
static ssize_t melfas_ts_sys_rawdata(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);
	int ret;

	input_info(true, &ts->client->dev, "%s [START]\n", __func__);

	if (melfas_ts_get_image(ts, MELFAS_TS_IMG_TYPE_RAWDATA)) {
		input_err(true, &ts->client->dev, "%s [ERROR] melfas_ts_get_image\n", __func__);
		return -1;
	}

	input_info(true, &ts->client->dev, "%s [DONE]\n", __func__);

	ret = snprintf(buf, PAGE_SIZE, "%s\n", ts->print_buf);
	return ret;
}

/**
 * Sysfs run cm delta test
 */
static ssize_t melfas_ts_sys_test_cm_delta(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);
	int ret;

	input_info(true, &ts->client->dev, "%s [START]\n", __func__);

	if (melfas_ts_run_test(ts, MELFAS_TS_TEST_TYPE_CM)) {
		input_err(true, &ts->client->dev, "%s [ERROR] melfas_ts_run_test\n", __func__);
		return -1;
	}

	input_info(true, &ts->client->dev, "%s [DONE]\n", __func__);

	ret = snprintf(buf, PAGE_SIZE, "%s\n", ts->print_buf);
	return ret;
}

/**
 * Sysfs run cm abs test
 */
static ssize_t melfas_ts_sys_test_cm_abs(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);
	int ret;

	input_info(true, &ts->client->dev, "%s [START]\n", __func__);

	if (melfas_ts_run_test(ts, MELFAS_TS_TEST_TYPE_CM_ABS)) {
		input_err(true, &ts->client->dev, "%s [ERROR] melfas_ts_run_test\n", __func__);
		return -1;
	}

	input_info(true, &ts->client->dev, "%s [DONE]\n", __func__);

	ret = snprintf(buf, PAGE_SIZE, "%s\n", ts->print_buf);
	return ret;
}

/**
 * Sysfs run cm jitter test
 */
static ssize_t melfas_ts_sys_test_cm_jitter(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);
	int ret;

	input_info(true, &ts->client->dev, "%s [START]\n", __func__);

	if (melfas_ts_run_test(ts, MELFAS_TS_TEST_TYPE_CM_JITTER)) {
		input_err(true, &ts->client->dev, "%s [ERROR] melfas_ts_run_test\n", __func__);
		return -1;
	}

	input_info(true, &ts->client->dev, "%s [DONE]\n", __func__);

	ret = snprintf(buf, PAGE_SIZE, "%s\n", ts->print_buf);
	return ret;
}

/**
 * Sysfs run short test
 */
static ssize_t melfas_ts_sys_test_short(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);
	int ret;

	input_info(true, &ts->client->dev, "%s [START]\n", __func__);

	if (melfas_ts_run_test(ts, MELFAS_TS_TEST_TYPE_SHORT)) {
		input_err(true, &ts->client->dev, "%s [ERROR] melfas_ts_run_test\n", __func__);
		return -1;
	}

	input_info(true, &ts->client->dev, "%s [DONE]\n", __func__);

	ret = snprintf(buf, PAGE_SIZE, "%s\n", ts->print_buf);
	return ret;
}

static ssize_t melfas_ts_sys_proximity(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);
	u8 data[255];
	int ret;
	u8 wbuf[8];

	input_info(true, &ts->client->dev, "%s [START]\n", __func__);

	wbuf[0] = MELFAS_TS_R0_CTRL;
	wbuf[1] = 0x23;

	if (!strcmp(attr->attr.name, "mode_proximity_on")) {
		wbuf[2] = 1;
	} else if (!strcmp(attr->attr.name, "mode_proximity_off")) {
		wbuf[2] = 0;
	} else {
		input_err(true, &ts->client->dev, "%s [ERROR] Unknown mode[%s]\n", __func__, attr->attr.name);
		snprintf(data, sizeof(data), "%s : Unknown Mode\n", attr->attr.name);
		goto exit;
	}

	ret = ts->melfas_ts_i2c_write(ts, wbuf, 3, NULL, 0);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s [ERROR] melfas_ts_i2c_write\n", __func__);
		snprintf(data, sizeof(data), "%s : ERROR\n", attr->attr.name);
	} else {
		input_info(true, &ts->client->dev, "%s - addr[0x%02X%02X] value[%d]\n", __func__, wbuf[0], wbuf[1], wbuf[2]);
		snprintf(data, sizeof(data), "%s : %d\n", attr->attr.name, wbuf[2]);
	}

	input_info(true, &ts->client->dev, "%s [DONE]\n", __func__);

exit:
	ret = snprintf(buf, 255, "%s\n", data);
	return ret;
}

/**
 * Sysfs functions
 */
static DEVICE_ATTR(fw_version, S_IWUSR | S_IWGRP, melfas_ts_sys_fw_version, NULL);
static DEVICE_ATTR(info, S_IWUSR | S_IWGRP, melfas_ts_sys_info, NULL);
static DEVICE_ATTR(device_enable, S_IWUSR | S_IWGRP, melfas_ts_sys_device_enable, NULL);
static DEVICE_ATTR(device_disable, S_IWUSR | S_IWGRP, melfas_ts_sys_device_disable, NULL);
static DEVICE_ATTR(irq_enable, S_IWUSR | S_IWGRP, melfas_ts_sys_irq_enable, NULL);
static DEVICE_ATTR(irq_disable, S_IWUSR | S_IWGRP, melfas_ts_sys_irq_disable, NULL);
static DEVICE_ATTR(power_on, S_IWUSR | S_IWGRP, melfas_ts_sys_power_on, NULL);
static DEVICE_ATTR(power_off, S_IWUSR | S_IWGRP, melfas_ts_sys_power_off, NULL);
static DEVICE_ATTR(reboot, S_IWUSR | S_IWGRP, melfas_ts_sys_reboot, NULL);
static DEVICE_ATTR(mode_glove, S_IWUSR | S_IWGRP, melfas_ts_sys_glove_mode_show, melfas_ts_sys_glove_mode_store);
static DEVICE_ATTR(mode_charger, S_IWUSR | S_IWGRP,
			melfas_ts_sys_charger_mode_show, melfas_ts_sys_charger_mode_store);
static DEVICE_ATTR(mode_cover_window, S_IWUSR | S_IWGRP,
			melfas_ts_sys_window_mode_show, melfas_ts_sys_window_mode_store);
static DEVICE_ATTR(mode_palm_rejection, S_IWUSR | S_IWGRP,
			melfas_ts_sys_palm_rejection_mode_show, melfas_ts_sys_palm_rejection_mode_store);
static DEVICE_ATTR(image_intensity, S_IWUSR | S_IWGRP, melfas_ts_sys_intensity, NULL);
static DEVICE_ATTR(image_rawdata, S_IWUSR | S_IWGRP, melfas_ts_sys_rawdata, NULL);
static DEVICE_ATTR(test_cm_delta, S_IWUSR | S_IWGRP, melfas_ts_sys_test_cm_delta, NULL);
static DEVICE_ATTR(test_cm_abs, S_IWUSR | S_IWGRP, melfas_ts_sys_test_cm_abs, NULL);
static DEVICE_ATTR(test_cm_jitter, S_IWUSR | S_IWGRP, melfas_ts_sys_test_cm_jitter, NULL);
static DEVICE_ATTR(test_short, S_IWUSR | S_IWGRP, melfas_ts_sys_test_short, NULL);
static DEVICE_ATTR(mode_proximity_on, S_IRUGO, melfas_ts_sys_proximity, NULL);
static DEVICE_ATTR(mode_proximity_off, S_IRUGO, melfas_ts_sys_proximity, NULL);

/**
 * Sysfs attr list info
 */
static struct attribute *melfas_ts_test_attr[] = {
	&dev_attr_fw_version.attr,
	&dev_attr_info.attr,
	&dev_attr_device_enable.attr,
	&dev_attr_device_disable.attr,
	&dev_attr_irq_enable.attr,
	&dev_attr_irq_disable.attr,
	&dev_attr_power_on.attr,
	&dev_attr_power_off.attr,
	&dev_attr_reboot.attr,
	&dev_attr_mode_glove.attr,
	&dev_attr_mode_charger.attr,
	&dev_attr_mode_cover_window.attr,
	&dev_attr_mode_palm_rejection.attr,
	&dev_attr_image_intensity.attr,
	&dev_attr_image_rawdata.attr,
	&dev_attr_test_cm_delta.attr,
	&dev_attr_test_cm_abs.attr,
	&dev_attr_test_cm_jitter.attr,
	&dev_attr_test_short.attr,
	&dev_attr_mode_proximity_off.attr,
	&dev_attr_mode_proximity_on.attr,
	NULL,
};

/**
 * Sysfs attr group info
 */
static const struct attribute_group melfas_ts_test_attr_group = {
	.attrs = melfas_ts_test_attr,
};

/**
 * Create sysfs test functions
 */
int melfas_ts_sysfs_create(struct melfas_ts_data *ts)
{
	struct i2c_client *client = ts->client;

	input_info(true, &ts->client->dev, "%s [START]\n", __func__);

	if (sysfs_create_group(&client->dev.kobj, &melfas_ts_test_attr_group)) {
		input_err(true, &client->dev, "%s [ERROR] sysfs_create_group\n", __func__);
		return -EAGAIN;
	}

	if (sysfs_create_link(NULL, &client->dev.kobj, MELFAS_TS_DEVICE_NAME)) {
		input_err(true, &client->dev, "%s [ERROR] sysfs_create_group\n", __func__);
		sysfs_remove_group(&ts->client->dev.kobj, &melfas_ts_test_attr_group);
		return -EAGAIN;
	}

	input_info(true, &ts->client->dev, "%s [DONE]\n", __func__);

	return 0;
}

/**
 * Remove sysfs test functions
 */
void melfas_ts_sysfs_remove(struct melfas_ts_data *ts)
{
	input_info(true, &ts->client->dev, "%s [START]\n", __func__);

	sysfs_remove_group(&ts->client->dev.kobj, &melfas_ts_test_attr_group);
	sysfs_remove_link(NULL, MELFAS_TS_DEVICE_NAME);
	device_destroy(ts->class, ts->melfas_dev);
	class_destroy(ts->class);

	input_info(true, &ts->client->dev, "%s [DONE]\n", __func__);
}

#endif
