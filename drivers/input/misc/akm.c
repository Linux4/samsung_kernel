
/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "akm.h"

static int akm_read(struct akm_info *info, u8 *reg, u8 len, u8 *data)
{
	struct i2c_msg msg[2];
	u8 *reg_buf;
	u8 *data_buf;
	int ret;

	reg_buf = kzalloc(1, GFP_KERNEL);
	if (!reg_buf)
		return -ENOMEM;

	data_buf = kzalloc(len, GFP_KERNEL);
	if (!data_buf) {
		kfree(reg_buf);
		return -ENOMEM;
	}

	memcpy(&reg_buf[0], reg, 1);

	msg[0].addr = info->client->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = reg_buf;

	msg[1].addr = info->client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].buf = data_buf;
	msg[1].len = len;

	mutex_lock(&info->i2c_lock);
	ret = i2c_transfer(info->client->adapter, msg, 2);
	mutex_unlock(&info->i2c_lock);

	memcpy(data, data_buf, len);

	kfree(reg_buf);
	kfree(data_buf);
	return ret;
}

static int akm_write(struct akm_info *info, u8 *reg, u8 num, u8 *data)
{
	struct i2c_msg msg[1];
	u8 *reg_buf;
	int ret;

	reg_buf = kzalloc(1 + num, GFP_KERNEL);
	if (!reg_buf)
		return -ENOMEM;

	reg_buf[0] = reg[0];
	memcpy(&reg_buf[1], data, num);

	msg[0].addr = info->client->addr;
	msg[0].flags = 0;
	msg[0].len = num + 1;
	msg[0].buf = reg_buf;

	mutex_lock(&info->i2c_lock);
	ret = i2c_transfer(info->client->adapter, msg, 1);
	mutex_unlock(&info->i2c_lock);

	kfree(reg_buf);
	return ret;
}

static int akm_read_status(struct akm_info *info)
{
	int ret;
	u8 reg;
	u8 data[7];

	memset(data, 0x00, 7);
	reg = AKM_REG_WORD_ST_Z_Y_X;

	ret = akm_read(info, &reg, 7, data);
	info->state = data[0];
	info->x = (s16)(data[5] << 8 | data[6]);
	info->y = (s16)(data[3] << 8 | data[4]);
	info->z = (s16)(data[1] << 8 | data[2]);

	input_info(true, &info->client->dev, "%s: status: %X Z:%d Y:%d X:%d\n",
			__func__, info->state, info->z, info->y, info->x);
	return ret;
}

static int akm_check_and_restore_thd(struct akm_info *info)
{
	int ret;
	int ii;
	u8 reg;
	u8 data[4];
	s16 bop[3];
	s16 brp[3];
	int restore = 0;

	memset(data, 0x00, 4);
	for (ii = 0; ii < 3; ii++) {
		reg = AKM_REG_THRESHOLD_X + ii;
		ret = akm_read(info, &reg, 4, data);
		if (ret < 0) {
			input_err(true, &info->client->dev, "%s: failed to read reg: %02X ret: %d\n", __func__, reg, ret);
			return ret;
		}
		bop[ii] = (s16)((data[0] << 8) | data[1]);
		brp[ii] = (s16)((data[2] << 8) | data[3]);

		if (bop[ii] != info->cal_value[ii * 2] || brp[ii] != info->cal_value[ii * 2 + 1]) {
			restore = 1;
			break;
		}
	}

	if (restore) {
		input_info(true, &info->client->dev, "%s: reset and restore cal value\n", __func__);
		akm_setup_device_control(info);

		for (ii = 0; ii < 3; ii++) {
			reg = AKM_REG_THRESHOLD_X + ii;

			data[0] = (u8)((info->cal_value[ii * 2 + 0] >> 8) & 0xFF);
			data[1] = (u8)(info->cal_value[ii * 2 + 0] & 0xFF);
			data[2] = (u8)((info->cal_value[ii * 2 + 1] >> 8) & 0xFF);
			data[3] = (u8)(info->cal_value[ii * 2 + 1] & 0xFF);

			ret = akm_write(info, &reg, 4, data);
			if (ret < 0) {
				input_err(true, &info->client->dev, "%s: failed to write reg: %02X ret: %d\n", __func__, reg, ret);
				return ret;
			}
		}
	} else {
		input_info(true, &info->client->dev, "%s: cal value is normal\n", __func__);
	}

	return 0;
}

static irqreturn_t akm_irq(int irq, void *drv)
{
	struct akm_info *info = (struct akm_info *)drv;
	int ret;

	ret = akm_read_status(info);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: failed to read status: %d\n", __func__, ret);
		return IRQ_HANDLED;
	}

	input_info(true, &info->client->dev, "%s: Z:%d Y:%d X:%d\n",
			__func__, info->z, info->y, info->x);

	return IRQ_HANDLED;
}

static void akm_schedule_work(struct work_struct *work)
{
	struct akm_info *info = container_of(work, struct akm_info, dwork.work);
	u8 reg;
	u8 data[7];
	int ret;

	reg = AKM_REG_WORD_ST;
	ret = akm_read(info, &reg, 1, data);
	input_info(true, &info->client->dev, "%s: (ret:%d) ST: 0x%02X\n", __func__, ret, data[0]); 

	reg = AKM_REG_WORD_ST_Z_Y_X;
	ret = akm_read(info, &reg, 7, data);
	if (ret < 0)
		return;

	info->state = data[0];
	info->z = (s16)(data[1] << 8 | data[2]);
	info->y = (s16)(data[3] << 8 | data[4]);
	info->x = (s16)(data[5] << 8 | data[6]);
	input_info(true, &info->client->dev, "%s: (ret:%d) ST: 0x%02X, Z:%d, Y:%d, X:%d\n", __func__, ret, info->state, info->z, info->y, info->x);

	schedule_delayed_work(&info->dwork, msecs_to_jiffies(40));
}

static ssize_t akm_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct akm_info *info = dev_get_drvdata(dev);
	int ret;

	ret = akm_read_status(info);
	if (ret < 0)
		return ret;

	return snprintf(buf, 128, "%d,%d,%d\n", info->x, info->y, info->z);
}

static ssize_t akm_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct akm_info *info = dev_get_drvdata(dev);
	u8 buffer[128];

	snprintf(buffer, 128, "digital_hall: ST: %02X\n", info->state);
	strlcat(buf, buffer, 128);
	snprintf(buffer, 128, "digital_hall: cal: %d, %d, %d, %d, %d, %d: %s\n",
			info->cal_value[0], info->cal_value[1], info->cal_value[2],
			info->cal_value[3], info->cal_value[4], info->cal_value[5],
			info->calibrated ? "CAL DONE" : "CAL NOT");
	strlcat(buf, buffer, 128);
	input_info(true, &info->client->dev, "%s: %s\n", __func__, buf);

	return strnlen(buf, PAGE_SIZE);
}

static ssize_t akm_dbg_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct akm_info *info = dev_get_drvdata(dev);
	u8 buffer[128];
	int ret;
	u8 reg;
	u8 data[7];

	memset(data, 0x00, 7);
	reg = AKM_REG_WORD_ST_Z_Y_X;

	ret = akm_read(info, &reg, 7, data);
	if (ret < 0)
		snprintf(buffer, 128, "digital_hall: dbg: i2c error\n");
	else
		snprintf(buffer, 128, "digital_hall: dbg: %02X %02X %02X %02X %02X %02X %02X\n",
				data[0], data[1], data[2], data[3], data[4], data[5], data[6]);
	strlcat(buf, buffer, 128);

	memset(data, 0x00, 3);
	reg = AKM_REG_CNTL1;
	ret = akm_read(info, &reg, 3, data);
	if (ret < 0)
		snprintf(buffer, 128, "digital_hall: dbg: i2c error\n");
	else
		snprintf(buffer, 128, "digital_hall: dbg: %02X %02X %02X\n",
				data[0], data[1], data[2]);
	strlcat(buf, buffer, 128);
	input_info(true, &info->client->dev, "%s: %s\n", __func__, buf);

	return strnlen(buf, PAGE_SIZE);
}
static ssize_t akm_thd_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct akm_info *info = dev_get_drvdata(dev);
	char buffer[128];
	char temp[128];
	int ret;
	int ii;
	u8 reg;
	u8 data[4];
	s16 bop[3];
	s16 brp[3];

	memset(data, 0x00, 4);
	for (ii = 0; ii < 3; ii++) {
		reg = AKM_REG_THRESHOLD_X + ii;
		ret = akm_read(info, &reg, 4, data);
		if (ret < 0) {
			input_err(true, &info->client->dev, "%s: failed to read reg: %02X ret: %d\n", __func__, reg, ret);
			return ret;
		}
		bop[ii] = (s16)((data[0] << 8) | data[1]);
		brp[ii] = (s16)((data[2] << 8) | data[3]);

		memset(temp, 0x00, 128);
		if (ii == 2)
			snprintf(temp, 128, "%d,%d", bop[ii], brp[ii]);
		else
			snprintf(temp, 128, "%d,%d,", bop[ii], brp[ii]);

		strlcat(buffer, temp, 128);
	}

	input_info(true, &info->client->dev, "%s: THD: %s\n", __func__, buffer);

	return snprintf(buf, 128, "%s", buffer);
}

static ssize_t akm_thd_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct akm_info *info = dev_get_drvdata(dev);
	int ret;
	u8 data[4];
	int ii;
	u8 reg;

	sscanf(buf, "%d,%d,%d,%d,%d,%d",
			&info->cal_value[0], &info->cal_value[1], &info->cal_value[2],
			&info->cal_value[3], &info->cal_value[4], &info->cal_value[5]);

	input_info(true, &info->client->dev, "%s: %d,%d,%d,%d,%d,%d", __func__,
			info->cal_value[0], info->cal_value[1], info->cal_value[2],
			info->cal_value[3], info->cal_value[4], info->cal_value[5]);

	for (ii = 0; ii < 3; ii++) {
		reg = AKM_REG_THRESHOLD_X + ii;

		data[0] = (u8)((info->cal_value[ii * 2 + 0] >> 8) & 0xFF);
		data[1] = (u8)(info->cal_value[ii * 2 + 0] & 0xFF);
		data[2] = (u8)((info->cal_value[ii * 2 + 1] >> 8) & 0xFF);
		data[3] = (u8)(info->cal_value[ii * 2 + 1] & 0xFF);

		ret = akm_write(info, &reg, 4, data);
		if (ret < 0) {
			input_err(true, &info->client->dev, "%s: failed to write reg: %02X ret: %d\n", __func__, reg, ret);
			return ret;
		}
	}

	info->calibrated = true;
	return count;
}

static ssize_t akm_dwork_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct akm_info *info = dev_get_drvdata(dev);
	char buffer[10];
	
	snprintf(buffer, 10, "%s\n", info->dwork_state ? "dworking" : "dstop");

	return snprintf(buf, 10, "%s\n", buffer);
}

static ssize_t akm_dwork_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct akm_info *info = dev_get_drvdata(dev);

	sscanf(buf, "%d", &info->dwork_state);

	if (info->dwork_state)
		schedule_delayed_work(&info->dwork, msecs_to_jiffies(40));
	else
		cancel_delayed_work_sync(&info->dwork);

	return count;
}

static ssize_t akm_selftest_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct akm_info *info = dev_get_drvdata(dev);
	char buffer[256];
	u8 reg;
	u8 data[10];
	int ret;
	int ii;
	int i2c_check = 0;
	int is_stuck = 0;
	int is_spec_out = 0;
	s16 min_val[3];
	s16 max_val[3];
	s16 x, y, z;

	reg = AKM_REG_SRST;
	data[0] = 0x01;
	ret = akm_write(info, &reg, 1, data);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: failed to write reg: %02X ret: %d\n", __func__, reg, ret);
		return ret;
	}

	msleep(60);

	reg = AKM_READ_DEVICE_ID;
	ret = akm_read(info, &reg, 2, data);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: failed to read reg: %02X ret: %d\n", __func__, reg, ret);
		return ret;
	}
	input_info(true, &info->client->dev, "%s: AKM_DEVICE_ID: %02X %02X\n", __func__, data[0], data[1]);

	if (data[0] == 0x48 && data[1] == 0xC1)
		i2c_check = 0;
	else
		i2c_check = -1;

	reg = AKM_REG_CNTL2;
	data[0] = 0x0A;
	ret = akm_write(info, &reg, 1, data);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: failed to write reg: %02X ret: %d\n", __func__, reg, ret);
		return ret;
	}

	msleep(40);

	for (ii = 0; ii < 10; ii++) {
		reg = AKM_REG_WORD_ST_Z_Y_X;
		ret = akm_read(info, &reg, 7, data);
		if (ret < 0) {
			input_err(true, &info->client->dev, "%s: failed to read reg: %02X ret: %d\n", __func__, reg, ret);
			return ret;
		}

		x = (s16)(data[5] << 8 | data[6]);
		y = (s16)(data[3] << 8 | data[4]);
		z = (s16)(data[1] << 8 | data[2]);

		if (ii == 0) {
			min_val[0] = max_val[0] = x;
			min_val[1] = max_val[1] = y;
			min_val[2] = max_val[2] = z;
		} else {
			if (min_val[0] == x && min_val[1] == y && min_val[2] == z)
				is_stuck |= (1 << ii);
		}

		input_info(true, &info->client->dev, "%s: x:%d y:%d z:%d is_stuck:%d\n", __func__, x, y, z, is_stuck);

		min_val[0] = min(x, min_val[0]);
		min_val[1] = min(y, min_val[1]);
		min_val[2] = min(z, min_val[2]);
		max_val[0] = max(x, max_val[0]);
		max_val[1] = max(y, max_val[1]);
		max_val[2] = max(z, max_val[2]);

		msleep(20);
	}

	reg = AKM_REG_CNTL2;
	data[0] = 0x01;
	ret = akm_write(info, &reg, 1, data);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: failed to write reg: %02X ret: %d\n", __func__, reg, ret);
		return ret;
	}

	msleep(100);

	for (ii = 0; ii < 11; ii++) {
		reg = AKM_REG_WORD_ST;
		ret = akm_read(info, &reg, 1, data);
		if (ret < 0) {
			input_err(true, &info->client->dev, "%s: failed to read reg: %02X ret: %d\n", __func__, reg, ret);
			return ret;
		}
		input_info(true, &info->client->dev, "%s: AKM_REG_WORD_ST: %02X\n", __func__, data[0]);
		if (((data[0] & 0x1) == 0x1) && (((data[0] >> 5) & 0x1) == 0))
			break;
		usleep_range(10 * 1000, 10 * 1000);

		if (ii == 10)
			return -ENODEV;
	}

	reg = AKM_REG_WORD_ST_Z_Y_X;
	ret = akm_read(info, &reg, 7, data);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: failed to read reg: %02X ret: %d\n", __func__, reg, ret);
		return ret;
	}
	input_info(true, &info->client->dev, "%s: AKM_REG_WORD_ST_Z_Y_X: %02X\n", __func__, data[0]);

	reg = AKM_REG_WORD_ST;
	ret = akm_read(info, &reg, 1, data);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: failed to read reg: %02X ret: %d\n", __func__, reg, ret);
		return ret;
	}

	input_info(true, &info->client->dev, "%s: AKM_REG_WORD_ST: %02X\n", __func__, data[0]);

	reg = AKM_REG_CNTL2;
	data[0] = 0x81;
	ret = akm_write(info, &reg, 1, data);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: failed to write reg: %02X ret: %d\n", __func__, reg, ret);
		return ret;
	}

	msleep(100);

	for (ii = 0; ii < 11; ii++) {
		reg = AKM_REG_WORD_ST;
		ret = akm_read(info, &reg, 1, data);
		if (ret < 0) {
			input_err(true, &info->client->dev, "%s: failed to read reg: %02X ret: %d\n", __func__, reg, ret);
			return ret;
		}
		input_info(true, &info->client->dev, "%s: AKM_REG_WORD_ST: %02X\n", __func__, data[0]);
		if (((data[0] & 0x1) == 0x1) && (((data[0] >> 5) & 0x1) == 0))
			break;
		usleep_range(10 * 1000, 10 * 1000);

		if (ii == 10)
			return -ENODEV;
	}

	reg = AKM_REG_WORD_ST_Z_Y_X;
	ret = akm_read(info, &reg, 7, data);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: failed to read reg: %02X ret: %d\n", __func__, reg, ret);
		return ret;
	}

	x = (s16)(data[5] << 8 | data[6]);
	y = (s16)(data[3] << 8 | data[4]);
	z = (s16)(data[1] << 8 | data[2]);

	input_info(true, &info->client->dev, "%s: AKM_REG_WORD_ST_Z_Y_X: %02X x:%d y:%d z:%d\n", __func__, data[0], x, y, z);

	reg = AKM_REG_WORD_ST;
	ret = akm_read(info, &reg, 1, data);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: failed to read reg: %02X ret: %d\n", __func__, reg, ret);
		return ret;
	}

	input_info(true, &info->client->dev, "%s: AKM_REG_WORD_ST: %02X\n", __func__, data[0]);

	if (x >= -120 && x <= 120 && y >= -120 && y <= 120 && z >= 60 && z <= 400)
		is_spec_out = 0;
	else
		is_spec_out = -2;

	snprintf(buffer, 256, "%d,", i2c_check);
	strlcat(buf, buffer, PAGE_SIZE);
	if (is_stuck == 0x3FF) {
		snprintf(buffer, 256, "-1,");
		strlcat(buf, buffer, PAGE_SIZE);
	} else if (is_spec_out != 0) {
		snprintf(buffer, 256, "%d,", is_spec_out);
		strlcat(buf, buffer, PAGE_SIZE);
	} else {
		snprintf(buffer, 256, "%d,", is_stuck);
		strlcat(buf, buffer, PAGE_SIZE);
	}

	snprintf(buffer, 256, "%d,%d,%d,%d,%d,%d,%d,%d,%d\n", min_val[0], min_val[1], min_val[2], max_val[0], max_val[1], max_val[2], x, y, z);
	strlcat(buf, buffer, PAGE_SIZE);

	akm_setup_device_control(info);

	for (ii = 0; ii < 3; ii++) {
		reg = AKM_REG_THRESHOLD_X + ii;

		data[0] = (u8)((info->cal_value[ii * 2 + 0] >> 8) & 0xFF);
		data[1] = (u8)(info->cal_value[ii * 2 + 0] & 0xFF);
		data[2] = (u8)((info->cal_value[ii * 2 + 1] >> 8) & 0xFF);
		data[3] = (u8)(info->cal_value[ii * 2 + 1] & 0xFF);

		ret = akm_write(info, &reg, 4, data);
		if (ret < 0) {
			input_err(true, &info->client->dev, "%s: failed to write reg: %02X ret: %d\n", __func__, reg, ret);
			return ret;
		}
		input_info(true, &info->client->dev, "%s: set thd: reg:%02X: %02X %02X %02X %02X\n", __func__, reg, data[0], data[1], data[2], data[3]);
	}
	input_info(true, &info->client->dev, "%s: set threshold\n", __func__);

	return strnlen(buf, PAGE_SIZE);
}

static DEVICE_ATTR(digital_hall_status, 0444, akm_status_show, NULL);
static DEVICE_ATTR(digital_hall_info, 0444, akm_info_show, NULL);
static DEVICE_ATTR(digital_hall_dbg, 0644, akm_dbg_show, NULL);
static DEVICE_ATTR(digital_hall_thd, 0664, akm_thd_show, akm_thd_store);
static DEVICE_ATTR(digital_hall_dwork, 0644, akm_dwork_show, akm_dwork_store);
static DEVICE_ATTR(selftest, 0444, akm_selftest_show, NULL);

static struct attribute *akm_attrs[] = {
	&dev_attr_digital_hall_status.attr,
	&dev_attr_digital_hall_info.attr,
	&dev_attr_digital_hall_dbg.attr,
	&dev_attr_digital_hall_thd.attr,
	&dev_attr_digital_hall_dwork.attr,
	&dev_attr_selftest.attr,
	NULL,
};

static struct attribute_group akm_attrs_group = {
	.attrs = akm_attrs,
};

static int akm_setup_device_control(struct akm_info *info)
{
	int ret = 0;
	u8 data[5];
	u8 reg;

	memset(data, 0x00, 4);

	reg = AKM_READ_DEVICE_ID;
	ret = akm_read(info, &reg, 2, data);
	input_info(true, &info->client->dev, "%s: ret: %d, device id %02X %02X\n", __func__, ret, data[0], data[1]);

	reg = AKM_REG_CNTL1;

	ret = akm_read(info, &reg, 2, data);
	input_info(true, &info->client->dev, "%s; CNTL1: %02X %02X\n", __func__, data[0], data[1]);
	
	data[0] = ((info->pdata->pol_x << 0) | (info->pdata->pol_y << 1) | (info->pdata->pol_z << 2) | (info->pdata->pol_v << 3));
	data[1] = ((info->pdata->drdy_en << 0) | (info->pdata->sw_x_en << 1) | (info->pdata->sw_y_en << 2) | (info->pdata->sw_z_en << 3) | (info->pdata->err_en << 5));

	ret = akm_write(info, &reg, 2, data);
	input_info(true, &info->client->dev, "%s; ret: %d, (1) CNTL1: %02X %02X\n", __func__, ret, data[0], data[1]);

	ret = akm_read(info, &reg, 2, data);
	input_info(true, &info->client->dev, "%s; ret: %d, (2) CNTL1: %02X %02X\n", __func__, ret, data[0], data[1]);

	memset(data, 0x00, 4);

	reg = AKM_REG_CNTL2;

	ret = akm_read(info, &reg, 1, data);
	input_info(true, &info->client->dev, "%s; CNTL2: %02X\n", __func__, data[0]);
	
	data[0] = ((measurement_mode[info->pdata->measurement_number] << 0) | (info->pdata->sdr << 5));

	ret = akm_write(info, &reg, 1, data);
	input_info(true, &info->client->dev, "%s; ret: %d, (1) CNTL2: %02X\n", __func__, ret, data[0]);

	ret = akm_read(info, &reg, 1, data);
	input_info(true, &info->client->dev, "%s; ret: %d, (2) CNTL2: %02X\n", __func__, ret, data[0]);
	

	reg = AKM_REG_THRESHOLD_X;

	ret = akm_read(info, &reg, 4, data);
	input_info(true, &info->client->dev, "%s; ret: %d, X: %02X %02X %02X %02X\n", __func__, ret, data[0], data[1], data[2], data[3]);
	
	data[0] = (u8)(info->pdata->bop_x >> 8);
	data[1] = (u8)(info->pdata->bop_x & 0xFF);
	data[2] = (u8)(info->pdata->brp_x >> 8);
	data[3] = (u8)(info->pdata->brp_x & 0xFF);

	ret = akm_write(info, &reg, 4, data);
	input_info(true, &info->client->dev, "%s; ret: %d, (1) X: %02X %02X %02X %02X\n", __func__, ret, data[0], data[1], data[2], data[3]);

	ret = akm_read(info, &reg, 4, data);
	input_info(true, &info->client->dev, "%s; ret: %d, (2) X: %02X %02X %02X %02X\n", __func__, ret, data[0], data[1], data[2], data[3]);
	
	reg = AKM_REG_THRESHOLD_Y;

	ret = akm_read(info, &reg, 4, data);
	input_info(true, &info->client->dev, "%s; ret: %d, Y: %02X %02X %02X %02X\n", __func__, ret, data[0], data[1], data[2], data[3]);
	
	data[0] = (u8)(info->pdata->bop_y >> 8);
	data[1] = (u8)(info->pdata->bop_y & 0xFF);
	data[2] = (u8)(info->pdata->brp_y >> 8);
	data[3] = (u8)(info->pdata->brp_y & 0xFF);

	ret = akm_write(info, &reg, 4, data);
	input_info(true, &info->client->dev, "%s; ret: %d, (1) Y: %02X %02X %02X %02X\n", __func__, ret, data[0], data[1], data[2], data[3]);

	ret = akm_read(info, &reg, 4, data);
	input_info(true, &info->client->dev, "%s; ret: %d, (2) Y: %02X %02X %02X %02X\n", __func__, ret, data[0], data[1], data[2], data[3]);

	reg = AKM_REG_THRESHOLD_Z;

	ret = akm_read(info, &reg, 4, data);
	input_info(true, &info->client->dev, "%s; ret: %d, Z: %02X %02X %02X %02X\n", __func__, ret, data[0], data[1], data[2], data[3]);
	
	data[0] = (u8)(info->pdata->bop_z >> 8);
	data[1] = (u8)(info->pdata->bop_z & 0xFF);
	data[2] = (u8)(info->pdata->brp_z >> 8);
	data[3] = (u8)(info->pdata->brp_z & 0xFF);

	ret = akm_write(info, &reg, 4, data);
	input_info(true, &info->client->dev, "%s; ret: %d, (1) Z: %02X %02X %02X %02X\n", __func__, ret, data[0], data[1], data[2], data[3]);

	ret = akm_read(info, &reg, 4, data);
	input_info(true, &info->client->dev, "%s; ret: %d, (2) Z: %02X %02X %02X %02X\n", __func__, ret, data[0], data[1], data[2], data[3]);

	reg = AKM_REG_THRESHOLD_V;

	ret = akm_read(info, &reg, 4, data);
	input_info(true, &info->client->dev, "%s; ret: %d, V: %02X %02X %02X %02X\n", __func__, ret, data[0], data[1], data[2], data[3]);
	
	data[0] = (u8)(info->pdata->bop_v >> 8);
	data[1] = (u8)(info->pdata->bop_v & 0xFF);
	data[2] = (u8)(info->pdata->brp_v >> 8);
	data[3] = (u8)(info->pdata->brp_v & 0xFF);

	ret = akm_write(info, &reg, 4, data);
	input_info(true, &info->client->dev, "%s; ret: %d, (1) V: %02X %02X %02X %02X\n", __func__, ret, data[0], data[1], data[2], data[3]);

	ret = akm_read(info, &reg, 4, data);
	input_info(true, &info->client->dev, "%s; ret: %d, (2) V: %02X %02X %02X %02X\n", __func__, ret, data[0], data[1], data[2], data[3]);

	return 0;

}

static int akm_setup_input_device(struct akm_info *info)
{
	int ret = 0;

	info->input_dev = input_allocate_device();
	if (!info->input_dev) {
		input_err(true, &info->client->dev, "%s: allocate device err!\n", __func__);
		return -ENODEV;
	}

	info->input_dev->name = "akm-digital-hall";
	info->input_dev->id.bustype = BUS_I2C;
	info->input_dev->dev.parent = &info->client->dev;

	input_set_capability(info->input_dev, EV_REL, REL_WHEEL);

	ret = input_register_device(info->input_dev);
	if (ret) {
		input_err(true, &info->client->dev, "%s: failed to register %s input device\n", __func__, info->input_dev->name);
		return -ENODEV;
	}

	return ret;
}

static void akm_power_control(struct akm_info *info, bool on)
{
	int ret = 0;

	input_info(true, &info->client->dev, "%s: %s++: dvdd:%s\n", __func__, on ? "on" : "off",
		regulator_is_enabled(info->pdata->dvdd) ? "on" : "off");
	input_info(true, &info->client->dev, "%s: %s++: pullup:%s\n", __func__, on ? "on" : "off",
		regulator_is_enabled(info->pdata->pullup) ? "on" : "off");

	if (on) {
		ret = regulator_enable(info->pdata->dvdd);
		if (ret)
			input_err(true, &info->client->dev, "%s: failed to enable dvdd: %d\n", __func__, ret);
		ret = regulator_enable(info->pdata->pullup);
		if (ret)
			input_err(true, &info->client->dev, "%s: failed to enable pullup: %d\n", __func__, ret);
	} else {
		regulator_disable(info->pdata->dvdd);
		regulator_disable(info->pdata->pullup);
	}
	input_info(true, &info->client->dev, "%s: %s: dvdd:%s\n", __func__, on ? "on" : "off",
		regulator_is_enabled(info->pdata->dvdd) ? "on" : "off");
	input_info(true, &info->client->dev, "%s: %s: pullup:%s\n", __func__, on ? "on" : "off",
		regulator_is_enabled(info->pdata->pullup) ? "on" : "off");

	msleep(200);
}

static int akm_pinctrl_configure(struct akm_info *info, bool enable)
{
	struct pinctrl_state *state;
	int ret = 0;

	if (IS_ERR_OR_NULL(info->pdata->pinctrl))
		return -ENODEV;

	if (enable)
		state = pinctrl_lookup_state(info->pdata->pinctrl, "on_state");
	else
		state = pinctrl_lookup_state(info->pdata->pinctrl, "off_state");

	if (!IS_ERR_OR_NULL(state))
		ret = pinctrl_select_state(info->pdata->pinctrl, state);

	input_info(true, &info->client->dev, "%s: %s\n", __func__, enable ? "on_state" : "off_state");

	return ret;
}

static int akm_parse_dt(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct akm_platform_data *pdata = dev->platform_data;
	struct device_node *np = dev->of_node;
	int ret = 0;
	u32 buf[4];

	pdata->gpio_int = of_get_named_gpio(np, "akm,gpio_int", 0);
	if (gpio_is_valid(pdata->gpio_int)) {
		ret = devm_gpio_request_one(&client->dev, pdata->gpio_int, GPIOF_DIR_IN, "akm,gpio_int");
		if (ret)
			input_err(true, &client->dev, "%s: failed to request gpio_int: %d\n", __func__, pdata->gpio_int);
		input_info(true, &client->dev, "%s: gpio_int: %d\n", __func__, gpio_get_value(pdata->gpio_int));
	}

	pdata->gpio_rst = of_get_named_gpio(np, "akm,gpio_rst", 0);
	if (gpio_is_valid(pdata->gpio_rst)) {
		ret = devm_gpio_request_one(&client->dev, pdata->gpio_rst, GPIOF_DIR_IN, "akm,gpio_rst");
		if (ret)
			input_err(true, &client->dev, "%s: failed to request gpio_rst: %d\n", __func__, pdata->gpio_rst);
		input_info(true, &client->dev, "%s: gpio_rst: %d\n", __func__, gpio_get_value(pdata->gpio_rst));
	}

	pdata->dvdd = devm_regulator_get(&client->dev, "dvdd");
	if (IS_ERR_OR_NULL(pdata->dvdd)) {
		input_err(true, dev, "%s: Failed to get %s regulator.\n",
				__func__, "dvdd");
		return -EINVAL;
	}

	pdata->pullup = devm_regulator_get(&client->dev, "pullup");
	if (IS_ERR_OR_NULL(pdata->pullup)) {
		input_err(true, dev, "%s: Failed to get %s regulator.\n",
				__func__, "pullup");
		return -EINVAL;
	}

	of_property_read_u32_array(np, "akm,measurement_number", buf, 1);
	pdata->measurement_number = buf[0];

	of_property_read_u32_array(np, "akm,drdy_en", buf, 1);
	pdata->drdy_en = buf[0];

	of_property_read_u32_array(np, "akm,sw_en", buf, 4);
	pdata->sw_x_en = buf[0];
	pdata->sw_y_en = buf[1];
	pdata->sw_z_en = buf[2];
	pdata->sw_v_en = buf[3];

	of_property_read_u32_array(np, "akm,err_en", buf, 1);
	pdata->err_en = buf[0];

	of_property_read_u32_array(np, "akm,pol", buf, 4);
	pdata->pol_x = buf[0];
	pdata->pol_y = buf[1];
	pdata->pol_z = buf[2];
	pdata->pol_v = buf[3];

	of_property_read_u32_array(np, "akm,sdr", buf, 1);
	pdata->sdr = buf[0];

	of_property_read_u32_array(np, "akm,smr", buf, 1);
	pdata->smr = buf[0];

	of_property_read_u32_array(np, "akm,threshold_x", buf, 2);
	pdata->bop_x = buf[0];
	pdata->brp_x = buf[1];
	of_property_read_u32_array(np, "akm,threshold_y", buf, 2);
	pdata->bop_y = buf[0];
	pdata->brp_y = buf[1];
	input_info(true, &client->dev, "%s: bop_y: %d, brp_y: %d\n", __func__, pdata->bop_y, pdata->brp_y);
	of_property_read_u32_array(np, "akm,threshold_z", buf, 2);
	pdata->bop_z = buf[0];
	pdata->brp_z = buf[1];
	of_property_read_u32_array(np, "akm,threshold_v", buf, 2);
	pdata->bop_v = buf[0];
	pdata->brp_v = buf[1];
	
	return 0;
}

static int akm_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct akm_info *info;
	struct akm_platform_data *pdata;
	int ret = 0;

	input_info(true, &client->dev, "%s\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		input_err(true, &client->dev, "%s: EIO err!\n", __func__);
		return -EIO;
	}

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev, sizeof(struct akm_platform_data), GFP_KERNEL);
		if (!pdata)
			return -ENOMEM;

		client->dev.platform_data = pdata;

		ret = akm_parse_dt(client);
		if (ret < 0) {
#if !IS_ENABLED(CONFIG_QGKI)
			return -EPROBE_DEFER;
#else
			return -ENODEV;
#endif
		}
	} else {
		input_err(true, &client->dev, "%s: failed to find platform data\n", __func__);
		return -ENODEV;
	}

	pdata->pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR_OR_NULL(pdata->pinctrl))
		input_err(true, &client->dev, "%s: failed to get pinctrl\n", __func__);

	info = devm_kzalloc(&client->dev, sizeof(struct akm_info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->client = client;
	info->pdata = pdata;

	if (info->pdata->gpio_int > 0)
		info->client->irq = gpio_to_irq(info->pdata->gpio_int);

	i2c_set_clientdata(client, info);
	input_info(true, &info->client->dev, "%s: client slave addr: 0x%02X\n", __func__, info->client->addr);

	mutex_init(&info->i2c_lock);

	akm_pinctrl_configure(info, true);

	akm_power_control(info, true);

	akm_setup_input_device(info);

	info->cal_value[0] = info->pdata->bop_x;
	info->cal_value[1] = info->pdata->brp_x;

	info->cal_value[2] = info->pdata->bop_y;
	info->cal_value[3] = info->pdata->brp_y;

	info->cal_value[4] = info->pdata->bop_z;
	info->cal_value[5] = info->pdata->brp_z;

	akm_setup_device_control(info);

	info->sec_dev = sec_device_create(info, "digital_hall");
	if (IS_ERR(info->sec_dev)) {
		input_info(true, &info->client->dev, "%s: failed to create sec_class: %d\n", __func__, info->sec_dev);
	} else {
		ret = sysfs_create_group(&info->sec_dev->kobj, &akm_attrs_group);
		if (ret < 0)
			input_err(true, &info->client->dev, "%s: failed to create group: %d\n", __func__, ret);


	}

	if (info->pdata->gpio_int > 0) {
		ret = request_threaded_irq(info->client->irq, NULL, akm_irq,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT | IRQF_SHARED, "akm", info);
		if (ret < 0)
			input_err(true, &info->client->dev, "%s: Unable to request threaded irq\n", __func__);
		input_info(true, &info->client->dev, "%s: request_irq = %d\n", __func__, client->irq);
	}

	INIT_DELAYED_WORK(&info->dwork, akm_schedule_work);
//	schedule_delayed_work(&info->dwork, msecs_to_jiffies(40));

	input_info(true, &info->client->dev, "%s: done\n", __func__);

	return 0;
}

static int akm_remove(struct i2c_client *client)
{
	return 0;
}

static void akm_shutdown(struct i2c_client *client)
{

}

#ifdef CONFIG_PM
static int akm_pm_resume(struct device *dev)
{
	struct akm_info *info = dev_get_drvdata(dev);

	akm_read_status(info);
	akm_check_and_restore_thd(info);

	return 0;
}

static const struct dev_pm_ops akm_dev_pm_ops = {
	.resume = akm_pm_resume,
};
#endif

static const struct i2c_device_id akm_id_table[] = {
	{ "akm", 0, },
	{ }
};

static const struct of_device_id akm_of_id_table[] = {
	{ .compatible = "akm", },
	{},
};

static struct i2c_driver akm_driver = {
	.probe	= akm_probe,
	.remove	= akm_remove,
	.shutdown = akm_shutdown,
	.id_table	= akm_id_table,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "akm",
		.of_match_table = akm_of_id_table,
#ifdef CONFIG_PM
		.pm = &akm_dev_pm_ops,
#endif
	},
};

static int __init akm_init(void)
{
	int ret;

	pr_info("[sec_input] %s\n", __func__);

	ret = i2c_add_driver(&akm_driver);
	if (ret) {
		pr_err("%s:device init failed.[%d]\n", __func__, ret);
	}

	return ret;
}

static void __exit akm_exit(void)
{
	i2c_del_driver(&akm_driver);
}

module_init(akm_init);
module_exit(akm_exit);


MODULE_AUTHOR("ym48.kim@samsung.com, soonkoo.park@samsung.com");
MODULE_DESCRIPTION("wheel driver");
MODULE_LICENSE("GPL");
