/*
 *  Copyright (C) 2012, Samsung Electronics Co. Ltd. All Rights Reserved.
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
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/dirent.h>
#include <linux/leds-s2mpb02.h>
#include <linux/gpio.h>
#include <linux/sec-pinmux.h>
#include "adsp.h"

#define VENDOR "AMS"
#define CHIP_ID "TCS3408"

#define FLASH_LED_TORCH_GPIO            115

#define DEFAULT_DUTY_50HZ		5000
#define DEFAULT_DUTY_60HZ		4166

struct flicker_data {
	int reg_backup[2];
	u32 eol_pulse_count;
	s16 eol_state;
	u8 eol_enable;
	u8 eol_result_status;
	u32 eol_data[2][4];
};

static struct flicker_data *pdata;

enum {
	FLICKER_CMD_TYPE_GET_TRIM_CHECK,
	FLICKER_CMD_TYPE_EOL_TEST_START,
	FLICKER_CMD_TYPE_EOL_TEST_END,
	FLICKER_CMD_TYPE_MAX
};

enum  {
	EOL_STATE_INIT = -1,
	EOL_STATE_100,
	EOL_STATE_120,
	EOL_STATE_DONE
};

#define FREQ100_SPEC_IN(X)            ((X == 100) ? "PASS" : "FAIL")
#define FREQ120_SPEC_IN(X)            ((X == 120) ? "PASS" : "FAIL")

#define IR_SPEC_IN(X)       ((X >= 0 && X <= 500000) ? "PASS" : "FAIL")
#define CLEAR_SPEC_IN(X)    ((X >= 0 && X <= 500000) ? "PASS" : "FAIL")
#define ICRATIO_SPEC_IN(X)  ((X >= 0 && X <= 10000) ? "PASS" : "FAIL")

/*************************************************************************/
/* factory Sysfs							 */
/*************************************************************************/
static ssize_t flicker_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR);
}

static ssize_t flicker_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", CHIP_ID);
}

static ssize_t flicker_wideband_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	uint8_t cnt = 0;

	mutex_lock(&data->flicker_factory_mutex);
	adsp_unicast(NULL, 0, MSG_FLICKER, 0, MSG_TYPE_GET_RAW_DATA);

	while (!(data->ready_flag[MSG_TYPE_GET_RAW_DATA] & 1 << MSG_FLICKER) &&
		cnt++ < TIMEOUT_CNT)
		usleep_range(500, 550);

	data->ready_flag[MSG_TYPE_GET_RAW_DATA] &= ~(1 << MSG_FLICKER);
	mutex_unlock(&data->flicker_factory_mutex);

	if (cnt >= TIMEOUT_CNT) {
		pr_err("[FACTORY] %s: Timeout\n", __func__);
		return snprintf(buf, PAGE_SIZE, "0\n");
	}

	return snprintf(buf, PAGE_SIZE, "%d\n", data->msg_buf[MSG_FLICKER][4]);
}

static ssize_t flicker_clear_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	uint8_t cnt = 0;

	mutex_lock(&data->flicker_factory_mutex);
	adsp_unicast(NULL, 0, MSG_FLICKER, 0, MSG_TYPE_GET_RAW_DATA);

	while (!(data->ready_flag[MSG_TYPE_GET_RAW_DATA] & 1 << MSG_FLICKER) &&
		cnt++ < TIMEOUT_CNT)
		usleep_range(500, 550);

	data->ready_flag[MSG_TYPE_GET_RAW_DATA] &= ~(1 << MSG_FLICKER);
	mutex_unlock(&data->flicker_factory_mutex);

	if (cnt >= TIMEOUT_CNT) {
		pr_err("[FACTORY] %s: Timeout\n", __func__);
		return snprintf(buf, PAGE_SIZE, "0\n");
	}

	return snprintf(buf, PAGE_SIZE, "%d\n", data->msg_buf[MSG_FLICKER][3]);
}

static ssize_t flicker_raw_data_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	uint8_t cnt = 0;

	mutex_lock(&data->flicker_factory_mutex);
	adsp_unicast(NULL, 0, MSG_FLICKER, 0, MSG_TYPE_GET_RAW_DATA);

	while (!(data->ready_flag[MSG_TYPE_GET_RAW_DATA] & 1 << MSG_FLICKER) &&
		cnt++ < TIMEOUT_CNT)
		usleep_range(500, 550);

	data->ready_flag[MSG_TYPE_GET_RAW_DATA] &= ~(1 << MSG_FLICKER);
	mutex_unlock(&data->flicker_factory_mutex);

	if (cnt >= TIMEOUT_CNT) {
		pr_err("[FACTORY] %s: Timeout\n", __func__);
		return snprintf(buf, PAGE_SIZE, "0,0,0,0,0,0\n");
	}

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d,%d,%d,%d\n",
		data->msg_buf[MSG_FLICKER][0], data->msg_buf[MSG_FLICKER][1],
		data->msg_buf[MSG_FLICKER][2], data->msg_buf[MSG_FLICKER][3],
		data->msg_buf[MSG_FLICKER][4], data->msg_buf[MSG_FLICKER][5]);
}

static ssize_t flicker_register_read_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	int cnt = 0;
	int32_t msg_buf[1];

	msg_buf[0] = pdata->reg_backup[0];

	mutex_lock(&data->flicker_factory_mutex);
	adsp_unicast(msg_buf, sizeof(msg_buf),
		MSG_FLICKER, 0, MSG_TYPE_GET_REGISTER);

	while (!(data->ready_flag[MSG_TYPE_GET_REGISTER] & 1 << MSG_FLICKER) &&
		cnt++ < TIMEOUT_CNT)
		usleep_range(500, 550);

	data->ready_flag[MSG_TYPE_GET_REGISTER] &= ~(1 << MSG_FLICKER);
	mutex_unlock(&data->flicker_factory_mutex);

	if (cnt >= TIMEOUT_CNT)
		pr_err("[SSC_FAC] %s: Timeout!!!\n", __func__);

	pdata->reg_backup[1] = data->msg_buf[MSG_FLICKER][0];
	pr_info("[SSC_FAC] %s: [0x%x]: %d\n",
		__func__, pdata->reg_backup[0], pdata->reg_backup[1]);

	return snprintf(buf, PAGE_SIZE, "%d\n", pdata->reg_backup[1]);
}

static ssize_t flicker_register_read_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int reg = 0;

	if (sscanf(buf, "%3d", &reg) != 1) {
		pr_err("[SSC_FAC]: %s - The number of data are wrong\n",
			__func__);
		return -EINVAL;
	}

	pdata->reg_backup[0] = reg;
	pr_info("[SSC_FAC] %s: [0x%x]\n", __func__, pdata->reg_backup[0]);

	return size;
}

static ssize_t flicker_register_write_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	int cnt = 0;
	int32_t msg_buf[2];

	if (sscanf(buf, "%3d,%5d", &msg_buf[0], &msg_buf[1]) != 2) {
		pr_err("[SSC_FAC]: %s - The number of data are wrong\n",
			__func__);
		return -EINVAL;
	}

	mutex_lock(&data->flicker_factory_mutex);
	adsp_unicast(msg_buf, sizeof(msg_buf),
		MSG_FLICKER, 0, MSG_TYPE_SET_REGISTER);

	while (!(data->ready_flag[MSG_TYPE_SET_REGISTER] & 1 << MSG_FLICKER) &&
		cnt++ < TIMEOUT_CNT)
		usleep_range(500, 550);

	data->ready_flag[MSG_TYPE_SET_REGISTER] &= ~(1 << MSG_FLICKER);
	mutex_unlock(&data->flicker_factory_mutex);

	if (cnt >= TIMEOUT_CNT)
		pr_err("[SSC_FAC] %s: Timeout!!!\n", __func__);

	pdata->reg_backup[0] = msg_buf[0];
	pr_info("[SSC_FAC] %s: 0x%x - %d\n",
		__func__, msg_buf[0], data->msg_buf[MSG_FLICKER][0]);

	return size;
}

static ssize_t flicker_trim_check_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	int cnt = 0;
	int32_t msg = FLICKER_CMD_TYPE_GET_TRIM_CHECK;
        data->msg_buf[MSG_FLICKER][0] = 0;

	mutex_lock(&data->flicker_factory_mutex);
	adsp_unicast(&msg, sizeof(int32_t), MSG_FLICKER,
		0, MSG_TYPE_GET_CAL_DATA);

	while (!(data->ready_flag[MSG_TYPE_GET_CAL_DATA] & 1 << MSG_FLICKER) &&
		cnt++ < TIMEOUT_CNT)
		usleep_range(500, 550);

	data->ready_flag[MSG_TYPE_GET_CAL_DATA] &= ~(1 << MSG_FLICKER);
	mutex_unlock(&data->flicker_factory_mutex);

	if (cnt >= TIMEOUT_CNT) {
		pr_err("[SSC_FAC] %s: Timeout!!!\n", __func__);
	}

	pr_info("[SSC_FAC] %s: %d (%d)\n", __func__,
		data->msg_buf[MSG_FLICKER][0], data->msg_buf[MSG_FLICKER][1]);

	return snprintf(buf, PAGE_SIZE, "%d\n", data->msg_buf[MSG_FLICKER][0]);
}

static ssize_t flicker_eol_spec_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d\n",
		0, 500000, 0, 500000, 0, 500000, 0 ,500000, 0, 10000, 0, 10000);
}

static ssize_t flicker_eol_mode_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int icRatio100 = 0;
	int icRatio120 = 0;

	if (pdata->eol_enable == 1) {
		pr_info("[SSC_FAC] %s - EOL_RUNNING\n", __func__);
		return snprintf(buf, PAGE_SIZE, "%s\n", "EOL_RUNNING");
	} else if (pdata->eol_enable == 0 && pdata->eol_result_status == 0) {
		pr_info("[SSC_FAC] %s - NO_EOL_TEST\n", __func__);
		return snprintf(buf, PAGE_SIZE, "%s\n", "NO_EOL_TEST");
	}
	pdata->eol_result_status = 0;

	icRatio100 = pdata->eol_data[0][1] * 100;
        if (pdata->eol_data[0][2] != 0)
		icRatio100 = icRatio100 / pdata->eol_data[0][2];

	icRatio120 = pdata->eol_data[1][1] * 100;
        if (pdata->eol_data[1][2] != 0)
		icRatio120 = icRatio120 / pdata->eol_data[1][2];

	return snprintf(buf, PAGE_SIZE,
		"%d, %s, %d, %s, %d, %s, %d, %s, %d, %s, %d, %s, %d, %s, %d, %s\n",
		pdata->eol_data[0][0], FREQ100_SPEC_IN(pdata->eol_data[0][0]),
		pdata->eol_data[1][0], FREQ120_SPEC_IN(pdata->eol_data[1][0]),
		pdata->eol_data[0][3], IR_SPEC_IN(pdata->eol_data[0][3]),
		pdata->eol_data[1][3], IR_SPEC_IN(pdata->eol_data[1][3]),
		pdata->eol_data[0][2], CLEAR_SPEC_IN(pdata->eol_data[0][2]),
		pdata->eol_data[1][2], CLEAR_SPEC_IN(pdata->eol_data[1][2]),
		icRatio100, ICRATIO_SPEC_IN(icRatio100),
		icRatio120, ICRATIO_SPEC_IN(icRatio120));
}

static int flicker_eol_cmd(struct adsp_data *data, int32_t cmd)
{
	int cnt = 0;

	mutex_lock(&data->flicker_factory_mutex);
	adsp_unicast(&cmd, sizeof(int32_t), MSG_FLICKER,
		0, MSG_TYPE_GET_CAL_DATA);

	while (!(data->ready_flag[MSG_TYPE_GET_CAL_DATA] & 1 << MSG_FLICKER) &&
		cnt++ < TIMEOUT_CNT)
		usleep_range(500, 550);

	data->ready_flag[MSG_TYPE_GET_CAL_DATA] &= ~(1 << MSG_FLICKER);
	mutex_unlock(&data->flicker_factory_mutex);

	if (cnt >= TIMEOUT_CNT) {
		pr_err("[SSC_FAC] %s: Timeout!!!\n", __func__);
		return -1;
	}

	pr_info("[SSC_FAC] %s: done!\n", __func__);
	return 0;
}

void flicker_eol_work_func(struct work_struct *work)
{
	struct adsp_data *data = container_of((struct delayed_work *)work,
		struct adsp_data, flicker_eol_work);
	uint8_t cnt = 0;

	mutex_lock(&data->flicker_factory_mutex);
	adsp_unicast(NULL, 0, MSG_FLICKER, 0, MSG_TYPE_GET_RAW_DATA);

	while (!(data->ready_flag[MSG_TYPE_GET_RAW_DATA] & 1 << MSG_FLICKER) &&
		cnt++ < TIMEOUT_CNT)
		usleep_range(500, 550);

	data->ready_flag[MSG_TYPE_GET_RAW_DATA] &= ~(1 << MSG_FLICKER);
	mutex_unlock(&data->flicker_factory_mutex);

	if (cnt >= TIMEOUT_CNT) {
		pr_err("[FACTORY] %s: Timeout\n", __func__);
		goto exit;
	}
	else if ((pdata->eol_state <= EOL_STATE_INIT) ||
		(pdata->eol_state >= EOL_STATE_DONE)) {
		pr_err("[FACTORY] %s: test error %d\n",
			__func__, (int)pdata->eol_state);
		return;
	}

	pr_info("[SSC_FAC] %s: [%d] %d, %d, %d %d\n",
		__func__, pdata->eol_state,
		data->msg_buf[MSG_FLICKER][0], data->msg_buf[MSG_FLICKER][1],
		data->msg_buf[MSG_FLICKER][2], data->msg_buf[MSG_FLICKER][3]);

	pdata->eol_data[pdata->eol_state][0] = data->msg_buf[MSG_FLICKER][0];
	pdata->eol_data[pdata->eol_state][1] = data->msg_buf[MSG_FLICKER][1];
	pdata->eol_data[pdata->eol_state][2] = data->msg_buf[MSG_FLICKER][2];
	pdata->eol_data[pdata->eol_state][3] = data->msg_buf[MSG_FLICKER][3];
	pdata->eol_state++;
exit:
	if (pdata->eol_state != EOL_STATE_DONE)
		schedule_delayed_work(&data->flicker_eol_work,
			msecs_to_jiffies(1000));
}

static int flicker_eol_mode(struct adsp_data *data)
{
	int pulse_duty = 0;
	int curr_state = EOL_STATE_INIT;
	int ret = 0, retries = 0;
#if IS_ENABLED(CONFIG_SEC_PM)
	int led_gpio = FLASH_LED_TORCH_GPIO + get_msm_gpio_chip_base();
#else
	int led_gpio = FLASH_LED_TORCH_GPIO + 308;
#endif

	pdata->eol_state = EOL_STATE_100;
	pdata->eol_enable = 1;
	pdata->eol_result_status = 1;
	pdata->eol_pulse_count = 0;

	ret = gpio_request(led_gpio, NULL);
	if (ret < 0) {
		pr_err("[SSC_FAC] %s - gpio_request fail (%d)\n", __func__, ret);
		pdata->eol_enable = 0;
		return ret;
	}

	s2mpb02_led_en(S2MPB02_TORCH_LED_1, 0, S2MPB02_LED_TURN_WAY_GPIO);

	while (retries++ < 3) {
		ret = flicker_eol_cmd(data, FLICKER_CMD_TYPE_EOL_TEST_START);
		if (ret >= 0) {
			break;
		} else {
			pr_err("[SSC_FAC] %s - %d cmd retries (%d)\n", __func__,
				FLICKER_CMD_TYPE_EOL_TEST_START, retries);
			msleep(20);
		}
	}
	schedule_delayed_work(&data->flicker_eol_work, msecs_to_jiffies(1200));

	pr_info("[SSC_FAC] %s - eol_loop start\n", __func__);
	while ((pdata->eol_state < EOL_STATE_DONE) &&
		(pdata->eol_pulse_count < 500)) {
		switch (pdata->eol_state) {
		case EOL_STATE_INIT:
			break;
		case EOL_STATE_100:
			pulse_duty = DEFAULT_DUTY_50HZ;
			break;
		case EOL_STATE_120:
			pulse_duty = DEFAULT_DUTY_60HZ;
			break;
		default:
			break;
		}

		if (pdata->eol_state >= EOL_STATE_100) {
			if (curr_state != pdata->eol_state) {
				s2mpb02_led_en(S2MPB02_TORCH_LED_1,
					S2MPB02_TORCH_OUT_I_80MA,
					S2MPB02_LED_TURN_WAY_GPIO);
				curr_state = pdata->eol_state;
			} else {
				gpio_direction_output(led_gpio, 1);
			}
			udelay(pulse_duty);
			gpio_direction_output(led_gpio, 0);
			pdata->eol_pulse_count++;
		}
		udelay(pulse_duty);
	}

	pr_info("[SSC_FAC] %s - eol loop end\n", __func__);

	retries = 0;
	while (retries++ < 3) {
		ret = flicker_eol_cmd(data, FLICKER_CMD_TYPE_EOL_TEST_END);
		if (ret >= 0) {
			break;
		} else {
			pr_err("[SSC_FAC] %s - %d cmd retries (%d)\n", __func__,
				FLICKER_CMD_TYPE_EOL_TEST_START, retries);
			msleep(20);
		}
	}
	cancel_delayed_work_sync(&data->flicker_eol_work);
	s2mpb02_led_en(S2MPB02_TORCH_LED_1, 0, S2MPB02_LED_TURN_WAY_GPIO);
	gpio_free(led_gpio);

	pdata->eol_enable = 0;

	if (pdata->eol_state < EOL_STATE_DONE)
		pr_info("[SSC_FAC] %s - timeout fail\n", __func__);

	return 0;
}

static ssize_t flicker_eol_mode_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	int err = 0;
	int mode = 0;

	err = kstrtoint(buf, 10, &mode);
	if (err < 0) {
		pr_err("%s - kstrtoint failed.(%d)\n", __func__, err);
		return size;
	}

	if (pdata->eol_enable > 0) {
		pr_err("%s - eol already enabled\n", __func__);
		return size;
	}

	flicker_eol_mode(data);

	return size;
}

static DEVICE_ATTR(vendor, 0444, flicker_vendor_show, NULL);
static DEVICE_ATTR(name, 0444, flicker_name_show, NULL);
static DEVICE_ATTR(raw_data, 0444, flicker_raw_data_show, NULL);
static DEVICE_ATTR(als_wideband, 0444, flicker_wideband_show, NULL);
static DEVICE_ATTR(als_clear, 0444, flicker_clear_show, NULL);
static DEVICE_ATTR(als_factory_cmd, 0444, flicker_trim_check_show, NULL);
static DEVICE_ATTR(eol_spec, 0444, flicker_eol_spec_show, NULL);
static DEVICE_ATTR(eol_mode, 0664,
	flicker_eol_mode_show, flicker_eol_mode_store);
static DEVICE_ATTR(register_write, 0220, NULL, flicker_register_write_store);
static DEVICE_ATTR(register_read, 0664,
	flicker_register_read_show, flicker_register_read_store);

static struct device_attribute *flicker_attrs[] = {
	&dev_attr_vendor,
	&dev_attr_name,
	&dev_attr_raw_data,
	&dev_attr_als_wideband,
	&dev_attr_als_clear,
	&dev_attr_als_factory_cmd,
	&dev_attr_eol_spec,
	&dev_attr_eol_mode,
	&dev_attr_register_write,
	&dev_attr_register_read,
	NULL,
};

static int __init tcs370x_flicker_factory_init(void)
{
	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	adsp_factory_register(MSG_FLICKER, flicker_attrs);

	return 0;
}

static void __exit tcs370x_flicker_factory_exit(void)
{
	adsp_factory_unregister(MSG_FLICKER);
	kfree(pdata);
}

module_init(tcs370x_flicker_factory_init);
module_exit(tcs370x_flicker_factory_exit);
