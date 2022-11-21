/*
 *  wacom_sec_fac.c - Wacom G5 Digitizer Controller (I2C bus)
 *
 *
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

#include "wacom_i2c.h"
#include "wacom_i2c_flash.h"

#ifdef USE_WACOM_BLOCK_KEYEVENT
static ssize_t epen_delay_time_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u msec\n", wac_i2c->key_delay_time);
}

static ssize_t epen_delay_time_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);
	unsigned int val;

	sscanf(buf, "%u", &val);

	if (val > 0)
		wac_i2c->key_delay_time = val;

	dev_info(&wac_i2c->client->dev, "%s: delay time : %d\n",
			__func__, wac_i2c->key_delay_time);
	return count;
}
#endif

#ifdef USE_WACOM_LCD_WORKAROUND
static ssize_t epen_read_freq_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", wac_i2c->delay_time);
}

static ssize_t epen_read_freq_data_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);
	unsigned int val;

	sscanf(buf, "%d", &val);

	wac_i2c->delay_time = val;

	dev_info(&wac_i2c->client->dev, "%s: lcd noise workaround delay time is  %d\n",
			__func__, wac_i2c->delay_time);

	return count;
}
#endif

static bool check_update_condition(struct wacom_i2c *wac_i2c, const char buf)
{
	bool bUpdate = false;

	dev_info(&wac_i2c->client->dev,
			"%s: system rev is 0x%02x\n",
			__func__, system_rev);

	switch (buf) {
	case 'I':
	case 'K':
		bUpdate = true;
		break;
	case 'R':
	case 'W':
		if (wac_i2c->wac_query_data->fw_version_ic <
			wac_i2c->wac_query_data->fw_version_bin)
			bUpdate = true;
		break;
	default:
		dev_info(&wac_i2c->client->dev,
				"%s: wrong parameter\n", __func__);
		bUpdate = false;
		break;
	}

	return bUpdate;
}

static ssize_t epen_firmware_update_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);
	int ret = 1;
	u32 fw_ic_ver = wac_i2c->wac_query_data->fw_version_ic;
	bool need_update = false;

	need_update = check_update_condition(wac_i2c, *buf);
	if (need_update == false) {
		dev_info(&wac_i2c->client->dev,
				"%s:Pass Update. Cmd %c, IC ver %04x, Ker ver %04x\n",
				__func__, *buf, fw_ic_ver, wac_i2c->wac_query_data->fw_version_bin);
		return count;
	} else {
		dev_info(&wac_i2c->client->dev,
			"%s:Update Start. IC fw ver : 0x%x, new fw ver : 0x%x\n",
			__func__, wac_i2c->wac_query_data->fw_version_ic,
			wac_i2c->wac_query_data->fw_version_bin);
	}

	switch (*buf) {
	/*ums*/
	case 'I':
		ret = wacom_fw_load_from_UMS(wac_i2c);
		if (ret)
			goto failure;
		dev_info(&wac_i2c->client->dev,
			"%s: Start firmware flashing (UMS image).\n",
			__func__);
		wac_i2c->ums_binary = true;
		break;
	/*kernel*/
	case 'K':
		ret = wacom_load_fw_from_req_fw(wac_i2c);
		if (ret)
			goto failure;
		break;

	/*booting*/
	case 'R':
		ret = wacom_load_fw_from_req_fw(wac_i2c);
		if (ret)
			goto failure;
		dev_info(&wac_i2c->client->dev,
		"%s: Start firmware flashing (kernel image).\n",
		__func__);

		break;
	default:
		/*There's no default case*/
		break;
	}

	/*start firm update*/
	mutex_lock(&wac_i2c->lock);
	wac_i2c->wacom_enable_irq(wac_i2c, false);
	wac_i2c->wac_query_data->firm_update_status = 1;

	ret = wacom_i2c_firm_update(wac_i2c);
	if (ret)
		goto update_err;
	wac_i2c->fw_data= NULL;
	wac_i2c->wacom_i2c_query(wac_i2c);
	wac_i2c->wac_query_data->firm_update_status = 2;
	wac_i2c->wacom_enable_irq(wac_i2c, true);
	mutex_unlock(&wac_i2c->lock);

	return count;
 update_err:
	wac_i2c->fw_data= NULL;
 failure:
	wac_i2c->wac_query_data->firm_update_status = -1;
	wac_i2c->wacom_enable_irq(wac_i2c, true);
	mutex_unlock(&wac_i2c->lock);
	return count;
}

static ssize_t epen_firm_update_status_show(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);

	dev_info(&wac_i2c->client->dev,
			"%s:(%d)\n", __func__,
			wac_i2c->wac_query_data->firm_update_status);

	if (wac_i2c->wac_query_data->firm_update_status == 2)
		return snprintf(buf, PAGE_SIZE, "PASS\n");
	else if (wac_i2c->wac_query_data->firm_update_status == 1)
		return snprintf(buf, PAGE_SIZE, "DOWNLOADING\n");
	else if (wac_i2c->wac_query_data->firm_update_status == -1)
		return snprintf(buf, PAGE_SIZE, "FAIL\n");
	else
		return 0;
}

static ssize_t epen_firm_version_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);

	dev_info(&wac_i2c->client->dev,
			"%s: 0x%x|0x%X\n", __func__,
			wac_i2c->wac_query_data->fw_version_ic,
			wac_i2c->wac_query_data->fw_version_bin);

	return snprintf(buf, PAGE_SIZE, "%04X\t%04X\n",
			wac_i2c->wac_query_data->fw_version_ic,
			wac_i2c->wac_query_data->fw_version_bin);
}

static ssize_t epen_tuning_version_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);

	dev_info(&wac_i2c->client->dev,
			"%s: %s\n", __func__,
			wac_i2c->wac_dt_data->basic_model);

	return snprintf(buf, PAGE_SIZE, "%s_%04X\n",
			wac_i2c->wac_dt_data->basic_model,
			wac_i2c->wac_query_data->fw_version_bin);
}

static ssize_t epen_reset_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);
	int val;

	sscanf(buf, "%d", &val);

	if (val == 1) {
		wac_i2c->wacom_enable_irq(wac_i2c, false);

		/* Reset IC */
		wac_i2c->reset_platform_hw(wac_i2c);

		/* I2C Test */
		wac_i2c->wacom_i2c_query(wac_i2c);

		wac_i2c->wacom_enable_irq(wac_i2c, true);

		dev_info(&wac_i2c->client->dev,
				"%s, result %d\n", __func__,
		       wac_i2c->query_status);
	}

	return count;
}

static ssize_t epen_reset_result_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);

	if (wac_i2c->query_status) {
		dev_info(&wac_i2c->client->dev,
				"%s, PASS\n", __func__);
		return snprintf(buf, PAGE_SIZE, "PASS\n");
	} else {
		dev_info(&wac_i2c->client->dev,
				"%s, FAIL\n", __func__);
		return snprintf(buf, PAGE_SIZE, "FAIL\n");
	}
}

int wacom_checksum(struct wacom_i2c *wac_i2c)
{
	int ret = 0, retry = 10;
	int i = 0;
	u8 buf[5] = {0, };
	char firmware_checksum[] = WACOM_FW_CHECKSUM;

	buf[0] = COM_CHECKSUM;

	while (retry--) {
		ret = wac_i2c->wacom_i2c_send(wac_i2c, &buf[0], 1, false);
		if (ret < 0) {
			dev_err(&wac_i2c->client->dev,
					 "%s: i2c fail, retry, %d\n",
				      __func__, __LINE__);
			continue;
		}

		msleep(200);
		ret = wac_i2c->wacom_i2c_recv(wac_i2c, buf, 5, false);
		if (ret < 0) {
			dev_err(&wac_i2c->client->dev,
					 "%s: i2c fail, retry, %d\n",
					 __func__, __LINE__);
			continue;
		} else if (buf[0] == 0x1f)
			break;
		dev_info(&wac_i2c->client->dev,
				 "%s: checksum retry\n",
				 __func__);
	}

	if (ret >= 0) {
		dev_info(&wac_i2c->client->dev,
				 "%s: received checksum %x, %x, %x, %x, %x\n",
				__func__, buf[0], buf[1],
				buf[2], buf[3], buf[4]);
	}

	for (i = 0; i < 5; ++i) {
		if (buf[i] != firmware_checksum[i]) {
			dev_info(&wac_i2c->client->dev,
					 "%s: checksum fail %dth %x %x\n",
					__func__, i, buf[i],
					firmware_checksum[i]);
			break;
		}
	}

	wac_i2c->checksum_result = (5 == i);

	return ret;
}

static ssize_t epen_checksum_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);
	int val;

	sscanf(buf, "%d", &val);

	if (val != 1) {
		dev_info(&wac_i2c->client->dev,
				"%s: wrong cmd %d\n", __func__, val);
		return count;
	}

		wac_i2c->wacom_enable_irq(wac_i2c, false);
		wacom_checksum(wac_i2c);
		wac_i2c->wacom_enable_irq(wac_i2c, true);

	dev_info(&wac_i2c->client->dev,
			"%s: result %d\n",
			__func__, wac_i2c->checksum_result);

	return count;
}

static ssize_t epen_checksum_result_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);

	if (wac_i2c->checksum_result) {
		dev_info(&wac_i2c->client->dev,
				"%s: checksum, PASS\n", __func__);
		return snprintf(buf, PAGE_SIZE, "PASS\n");
	} else {
		dev_info(&wac_i2c->client->dev,
				"%s: checksum, FAIL\n", __func__);
		return snprintf(buf, PAGE_SIZE, "FAIL\n");
	}
}

#ifdef WACOM_CONNECTION_CHECK
static ssize_t epen_connection_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buff)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);
	u8 cmd = 0;
	u8 buf[2] = {0,};
	int ret = 0, cnt = 10;

	disable_irq(wac_i2c->client->irq);

	cmd = WACOM_I2C_STOP;
	ret = wac_i2c->wacom_i2c_send(wac_i2c, &cmd, 1, false);
	if (ret <= 0) {
		dev_err(&wac_i2c->client->dev,
			 "%s: failed to send stop command\n",
			 __func__);
		goto grid_check_error;
	}

	cmd = WACOM_I2C_GRID_CHECK;
	ret = wac_i2c->wacom_i2c_send(wac_i2c, &cmd, 1, false);
	if (ret <= 0) {
		dev_err(&wac_i2c->client->dev,
			 "%s: failed to send stop command\n",
			 __func__);
		goto grid_check_error;
	}

	cmd = WACOM_STATUS;
	do {
		msleep(50);
		if (1 == wac_i2c->wacom_i2c_send(wac_i2c, &cmd, 1, false)) {
			if (2 == wac_i2c->wacom_i2c_recv(wac_i2c,
						buf, 2, false)) {
				switch (buf[0]) {
				/*
				*	status value
				*	0 : data is not ready
				*	1 : PASS
				*	2 : Fail (coil function error)
				*	3 : Fail (All coil function error)
				*/
				case 1:
				case 2:
				case 3:
					cnt = 0;
					break;

				default:
					break;
				}
			}
		}
	} while (cnt--);

	dev_info(&wac_i2c->client->dev,
			 "%s : status: %x, error code: %x\n",
		       __func__, buf[0], buf[1]);

grid_check_error:
	cmd = WACOM_I2C_STOP;
	ret = wac_i2c->wacom_i2c_send(wac_i2c, &cmd, 1, false);
	if (ret <= 0)
		dev_err(&wac_i2c->client->dev,
			 "%s: failed to send stop command\n",
			 __func__);

	cmd = WACOM_I2C_START;
	wac_i2c->wacom_i2c_send(wac_i2c, &cmd, 1, false);
	if (ret <= 0)
		dev_err(&wac_i2c->client->dev,
			 "%s: failed to send stop command\n",
			 __func__);

	enable_irq(wac_i2c->client->irq);

	if ((buf[0] == 0x1) && (buf[1] == 0))
		return snprintf(buff, PAGE_SIZE, "%s\n", "OK");
	else
		return snprintf(buff, PAGE_SIZE, "%s\n", "NG");
}
#endif

#ifdef BATTERY_SAVING_MODE
static ssize_t epen_saving_mode_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);
	int val;


	if (sscanf(buf, "%u", &val) == 1)
		wac_i2c->battery_saving_mode = !!val;

	dev_info(&wac_i2c->client->dev, "%s: %s\n",
			__func__, val ? "checked" : "unchecked");

	if (wac_i2c->battery_saving_mode) {
		if (wac_i2c->pen_insert)
			wac_i2c->wacom_i2c_disable(wac_i2c);
	} else {
		if (wac_i2c->enabled)
			wac_i2c->wacom_i2c_enable(wac_i2c);
	}
	return count;
}
#endif

#ifdef WACOM_MAINSCREEN_DISABLE
#define MAIN_DIS 	0xDE // : Enter 2nd screen only mode command.
#define MAIN_EN	  	0xDF // : Exit 2nd screen only mode command.

static ssize_t epen_mainscreen_disable_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);
	int val, ret;
	int retry = 3; 
	u8 cmd = 0;
	int disable_cmd = 0;


	if (sscanf(buf, "%u", &val) == 1)
		disable_cmd = !!val;

	dev_info(&wac_i2c->client->dev, "%s: %s\n",	__func__, val ? "yes" : "no");

	if(disable_cmd){
		cmd = MAIN_DIS;	
	}else{
		cmd = MAIN_EN;	
	}

	while (retry--) {
		ret = wac_i2c->wacom_i2c_send(wac_i2c, &cmd, 1, false);
		if (ret < 0) {
			dev_err(&wac_i2c->client->dev, "%s: i2c fail, retry, %d\n",
					  __func__, __LINE__);
			continue;
		}
		break;
	}

	return count;
}
#endif

#ifdef WACOM_BOOSTER
static ssize_t boost_level_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);
	int val, retval, stage;

	dev_info(&wac_i2c->client->dev, "%s\n", __func__);
	sscanf(buf, "%d", &val);

	stage = 1 << val;
	if (!(wac_i2c->wacom_booster->dvfs_stage & stage)) {
		dev_err(&wac_i2c->client->dev,
				"%s: %d is not supported(%04x != %04x).\n",
				__func__, val,
				stage, wac_i2c->wacom_booster->dvfs_stage);
		return count;
	}

	wac_i2c->wacom_booster->dvfs_boost_mode = stage;
	dev_info(&wac_i2c->client->dev,
			"%s: dvfs_boost_mode = %d\n",
			__func__, wac_i2c->wacom_booster->dvfs_boost_mode);

	if (wac_i2c->wacom_booster->dvfs_boost_mode != DVFS_STAGE_TRIPLE) {
		wac_i2c->wacom_booster->dvfs_freq = -1;
	} else if (wac_i2c->wacom_booster->dvfs_boost_mode == DVFS_STAGE_NONE) {
		retval =wac_i2c->wacom_booster->dvfs_off(wac_i2c->wacom_booster);
		if (retval < 0) {
			dev_err(&wac_i2c->client->dev,
					"%s: booster stop failed(%d).\n",
					__func__, retval);
		}
	}
	return count;
}
#endif

static ssize_t epen_report_rate_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%s\n",
				(wac_i2c->sampling_rate == SAMPLING_RATE_120HZ) ? "120Hz" :
				(wac_i2c->sampling_rate == SAMPLING_RATE_80HZ) ? "80Hz" :
				(wac_i2c->sampling_rate == SAMPLING_RATE_40HZ) ? "40Hz" : "Not Defined");
}

static ssize_t epen_report_rate_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);
	unsigned int val;
	int retval;

	sscanf(buf, "%d", &val);

	if (val < 1 || val > 3 || !wac_i2c->power_enable) {
		dev_err(&wac_i2c->client->dev, "%s: not support value: %d\n",
				__func__, val);
		return count;
	}

	if (val == 1)
		wac_i2c->sampling_rate = SAMPLING_RATE_120HZ;
	else if (val == 2)
		wac_i2c->sampling_rate = SAMPLING_RATE_80HZ;
	else if (val == 3)
		wac_i2c->sampling_rate = SAMPLING_RATE_40HZ;

	retval = wac_i2c->wacom_i2c_send(wac_i2c, &wac_i2c->sampling_rate, 1, false);
	if (retval < 0)
		dev_err(&wac_i2c->client->dev, "%s: error: %d\n",
				__func__, retval);

	return count;
}

#ifdef USE_WACOM_BLOCK_KEYEVENT
static DEVICE_ATTR(epen_delay_time,
		   S_IRUGO | S_IWUSR | S_IWGRP, epen_delay_time_show, epen_delay_time_store);
#endif
#ifdef USE_WACOM_LCD_WORKAROUND
static DEVICE_ATTR(epen_read_freq,
		   S_IRUGO | S_IWUSR | S_IWGRP, epen_read_freq_show, epen_read_freq_data_store);
#endif
/* firmware update */
static DEVICE_ATTR(epen_firm_update,
		   S_IWUSR | S_IWGRP, NULL, epen_firmware_update_store);
/* return firmware update status */
static DEVICE_ATTR(epen_firm_update_status,
		   S_IRUGO, epen_firm_update_status_show, NULL);
/* return firmware version */
static DEVICE_ATTR(epen_firm_version, S_IRUGO, epen_firm_version_show, NULL);
/* return tuning data version */
static DEVICE_ATTR(epen_tuning_version, S_IRUGO,
			epen_tuning_version_show, NULL);
/* For SMD Test */
static DEVICE_ATTR(epen_reset, S_IWUSR | S_IWGRP, NULL, epen_reset_store);
static DEVICE_ATTR(epen_reset_result,
		   S_IRUSR | S_IRGRP, epen_reset_result_show, NULL);

/* For SMD Test. Check checksum */
static DEVICE_ATTR(epen_checksum, S_IWUSR | S_IWGRP, NULL, epen_checksum_store);
static DEVICE_ATTR(epen_checksum_result, S_IRUSR | S_IRGRP,
		   epen_checksum_result_show, NULL);

#ifdef WACOM_CONNECTION_CHECK
static DEVICE_ATTR(epen_connection,
		   S_IRUGO, epen_connection_show, NULL);
#endif

#ifdef BATTERY_SAVING_MODE
static DEVICE_ATTR(epen_saving_mode,
		   S_IWUSR | S_IWGRP, NULL, epen_saving_mode_store);
#endif
#ifdef WACOM_BOOSTER
static DEVICE_ATTR(boost_level,
		   S_IWUSR | S_IWGRP, NULL, boost_level_store);
#endif
#ifdef WACOM_MAINSCREEN_DISABLE
static DEVICE_ATTR(epen_main_disable,
		   S_IWUSR | S_IWGRP, NULL, epen_mainscreen_disable_store);
#endif

static DEVICE_ATTR(epen_report_rate, S_IWUSR | S_IWGRP | S_IRUGO,
			epen_report_rate_show, epen_report_rate_store);

static struct attribute *epen_attributes[] = {
#ifdef USE_WACOM_BLOCK_KEYEVENT
	&dev_attr_epen_delay_time.attr,
#endif
#ifdef USE_WACOM_LCD_WORKAROUND
	&dev_attr_epen_read_freq.attr,
#endif
	&dev_attr_epen_firm_update.attr,
	&dev_attr_epen_firm_update_status.attr,
	&dev_attr_epen_firm_version.attr,
	&dev_attr_epen_tuning_version.attr,
	&dev_attr_epen_reset.attr,
	&dev_attr_epen_reset_result.attr,
	&dev_attr_epen_checksum.attr,
	&dev_attr_epen_checksum_result.attr,
#ifdef WACOM_CONNECTION_CHECK
	&dev_attr_epen_connection.attr,
#endif
#ifdef BATTERY_SAVING_MODE
	&dev_attr_epen_saving_mode.attr,
#endif
#ifdef WACOM_BOOSTER
	&dev_attr_boost_level.attr,
#endif
#ifdef WACOM_MAINSCREEN_DISABLE
	&dev_attr_epen_main_disable.attr,
#endif

	&dev_attr_epen_report_rate.attr,
	NULL,
};

static struct attribute_group epen_attr_group = {
	.attrs = epen_attributes,
};

int wacom_factory_probe(struct device *dev){
	struct wacom_i2c *wac_i2c  = dev_get_drvdata(dev);
	int ret;

	ret = sysfs_create_group(&wac_i2c->dev->kobj, &epen_attr_group);

	return ret;
}
void wacom_factory_release(struct device *dev){
	struct wacom_i2c *wac_i2c  = dev_get_drvdata(dev);
	
	sysfs_remove_group(&wac_i2c->dev->kobj, &epen_attr_group);
}
