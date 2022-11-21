/*
 * haptic motor driver for s2mu106 - s2mu106_haptic.c
 *
 * Copyright (C) 2018 Suji Lee <suji0908.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define pr_fmt(fmt) "[VIB] " fmt
#define PM_QOS_NONIDLE_VALUE			300

#include <linux/module.h>
#include <linux/kernel.h>
#include "timed_output.h"
#include <linux/hrtimer.h>
#include <linux/pwm.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/i2c.h>
#include <linux/regulator/consumer.h>
#include <linux/s2mu106_haptic.h>
#include <linux/kthread.h>
#include <linux/mfd/samsung/s2mu106.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/kthread.h>
#include <linux/wakelock.h>
#include <linux/pm_qos.h>

#if defined(CONFIG_SSP_MOTOR_CALLBACK)
#include <linux/ssp_motorcallback.h>
#endif

#if defined(CONFIG_BATTERY_SAMSUNG_V2)
#include "../battery_v2/include/sec_charging_common.h"
#endif

const char s2mu106_sec_vib_event_cmd[EVENT_CMD_MAX][MAX_STR_LEN_EVENT_CMD] = {
	[EVENT_CMD_NONE]					= "NONE",
	[EVENT_CMD_FOLDER_CLOSE]				= "FOLDER_CLOSE",
	[EVENT_CMD_FOLDER_OPEN]					= "FOLDER_OPEN",
	[EVENT_CMD_ACCESSIBILITY_BOOST_ON]			= "ACCESSIBILITY_BOOST_ON",
	[EVENT_CMD_ACCESSIBILITY_BOOST_OFF]			= "ACCESSIBILITY_BOOST_OFF",
};

char s2mu106_sec_prev_event_cmd[MAX_STR_LEN_EVENT_CMD];
#if defined(CONFIG_FOLDER_HALL)
static const int FOLDER_TYPE = 1;
#else
static const int FOLDER_TYPE = 0;
#endif

static struct pm_qos_request pm_qos_req;
static struct wake_lock vib_wake_lock;

struct s2mu106_haptic_data {
	struct s2mu106_dev *s2mu106;
	struct i2c_client *i2c;
	struct s2mu106_haptic_platform_data *pdata;
	struct device *dev;

	enum s2mu106_haptic_operation_type hap_mode;
	u32 intensity;
	int motor_en;
	struct pwm_device *pwm;
	struct mutex mutex;
	spinlock_t lock;

	struct timed_output_dev tout_dev;
	struct hrtimer timer;

	struct kthread_worker kworker;
	struct kthread_work kwork;
};

static void s2mu106_set_boost_voltage(struct s2mu106_haptic_data *haptic, int voltage)
{
	u8 data;
	if (voltage <= 3150)
		data = 0x00;
	else if (voltage > 3150 && voltage <= 6300)
		data = (voltage - 3150) / 50;
	else
		data = 0xFF;
	pr_info("%s: boost voltage %d, 0x%02x\n", __func__, voltage, data);

	s2mu106_update_reg(haptic->i2c, S2MU106_REG_HBST_CTRL1,
				data, HAPTIC_BOOST_VOLTAGE_MASK);
}

static void s2mu106_haptic_set_freq(struct s2mu106_haptic_data *haptic, int freq)
{
	struct s2mu106_haptic_platform_data *pdata = haptic->pdata;

	if (freq >= 0 && freq < pdata->multi_frequency)
		pdata->period = pdata->multi_freq_period[freq];
	else if (freq >= HAPTIC_ENGINE_FREQ_MIN && freq <= HAPTIC_ENGINE_FREQ_MAX)
		pdata->period = MULTI_FREQ_DIVIDER / freq;
	else {
		pr_err("%s out of range %d\n", __func__, freq);
		return;
	}

	pdata->freq = freq;
}

#if defined(CONFIG_BATTERY_SAMSUNG_V2)
static int s2mu106_get_temperature_duty_ratio(struct s2mu106_haptic_data *haptic)
{
	union power_supply_propval value = {0, };
	int ret = haptic->pdata->ratio;

	psy_do_property("battery", get, POWER_SUPPLY_PROP_TEMP, value);
	if (value.intval >= haptic->pdata->temperature)
		ret = haptic->pdata->high_temp_ratio;
	pr_info("%s temp:%d duty:%d\n", __func__, value.intval, ret);
	return ret;
}
#endif

static void s2mu106_haptic_set_intensity(struct s2mu106_haptic_data *haptic, int intensity)
{
	struct s2mu106_haptic_platform_data *pdata = haptic->pdata;
	int data = 0x3FFFF;
	int max = 0x7FFFF;
	u8 val1, val2, val3;
	int duty_ratio = pdata->ratio;

	intensity = intensity / 100;
	haptic->intensity = intensity;

	if (intensity == 100)
		data = max;
	else if (intensity != 0) {
		long long tmp;

		tmp = (intensity * max) / 100;
		data = (int)tmp;
	} else
		data = 0;

	data = (data * haptic->pdata->intensity) / 100;	
	data &= 0x7FFFF;
	val1 = data & 0x0000F;
	val2 = (data & 0x00FF0) >> 4;
	val3 = (data & 0x7F000) >> 12;

	s2mu106_update_reg(haptic->i2c, S2MU106_REG_AMPCOEF1, val3, 0x7F);
	s2mu106_write_reg(haptic->i2c, S2MU106_REG_AMPCOEF2, val2);
	s2mu106_update_reg(haptic->i2c, S2MU106_REG_AMPCOEF3, val1 << 4, 0xF0);

#if defined(CONFIG_BATTERY_SAMSUNG_V2)
	if (haptic->pdata->high_temp_ratio)
		duty_ratio = s2mu106_get_temperature_duty_ratio(haptic);

	pdata->ratio = duty_ratio;
#endif
	pdata->duty = (pdata->period * duty_ratio) / 100;
}

static void s2mu106_haptic_onoff(struct s2mu106_haptic_data *haptic, bool en)
{
	if (en) {
		pr_info("Motor Enable\n");

		wake_lock(&vib_wake_lock);
		pm_qos_update_request(&pm_qos_req, PM_QOS_NONIDLE_VALUE);

		switch (haptic->hap_mode) {
		case S2MU106_HAPTIC_LRA:
			s2mu106_write_reg(haptic->i2c, S2MU106_REG_HAPTIC_MODE, LRA_MODE_EN);
			if (haptic->pdata->hbst.en) {
				s2mu106_update_reg(haptic->i2c, S2MU106_REG_HBST_CTRL0,
                  			SEL_HBST_HAPTIC_MASK, SEL_HBST_HAPTIC_MASK);
				s2mu106_update_reg(haptic->i2c, S2MU106_REG_OV_BK_OPTION,
					0, LRA_BST_MODE_SET_MASK);
			}
			pwm_config(haptic->pwm, haptic->pdata->duty,
					haptic->pdata->period);
			pwm_enable(haptic->pwm);
			break;
		case S2MU106_HAPTIC_ERM_GPIO:
			if (gpio_is_valid(haptic->motor_en))
				gpio_direction_output(haptic->motor_en, 1);
			break;
		case S2MU106_HAPTIC_ERM_I2C:
			s2mu106_write_reg(haptic->i2c, S2MU106_REG_HAPTIC_MODE, ERM_MODE_ON);
			break;
		default:
			break;
		}
	} else {
		pr_info("Motor Disable\n");

		switch (haptic->hap_mode) {
		case S2MU106_HAPTIC_LRA:
			pwm_disable(haptic->pwm);
			s2mu106_write_reg(haptic->i2c, S2MU106_REG_HAPTIC_MODE, HAPTIC_MODE_OFF);
			if (haptic->pdata->hbst.en) {
				s2mu106_update_reg(haptic->i2c, S2MU106_REG_HBST_CTRL0,
					0, SEL_HBST_HAPTIC_MASK);
				s2mu106_update_reg(haptic->i2c, S2MU106_REG_OV_BK_OPTION,
					LRA_BST_MODE_SET_MASK, LRA_BST_MODE_SET_MASK);
			}
			break;
		case S2MU106_HAPTIC_ERM_GPIO:
			if (gpio_is_valid(haptic->motor_en))
				gpio_direction_output(haptic->motor_en, 0);
			break;
		case S2MU106_HAPTIC_ERM_I2C:
			s2mu106_write_reg(haptic->i2c, S2MU106_REG_HAPTIC_MODE, HAPTIC_MODE_OFF);
			break;
		default:
			break;
		}

		wake_unlock(&vib_wake_lock);
		pm_qos_update_request(&pm_qos_req, PM_QOS_DEFAULT_VALUE);
	}
}

static void s2mu106_haptic_set_overdrive(struct s2mu106_haptic_platform_data *pdata, bool en)  
{
	pdata->ratio = en ? pdata->overdrive_ratio : pdata->normal_ratio;
}

static void s2mu106_haptic_engine_run_packet(struct s2mu106_haptic_data *hap_data,
		struct vib_packet packet)
{
	struct s2mu106_haptic_platform_data *pdata = hap_data->pdata;
	int frequency = packet.freq;
	int intensity = packet.intensity;
	int overdrive = packet.overdrive;

	if (!pdata->f_packet_en) {
		pr_err("haptic packet is empty\n");
		return;
	}

	s2mu106_haptic_set_overdrive(pdata, overdrive);
	s2mu106_haptic_set_freq(hap_data, frequency);
	s2mu106_haptic_set_intensity(hap_data, intensity);

	if (intensity) {
		pr_info("[haptic engine] motor run\n");
		s2mu106_haptic_onoff(hap_data, true);
	} else {
		pr_info("[haptic engine] motor stop\n");
		s2mu106_haptic_onoff(hap_data, false);
	}

	pr_info("%s [%d] time:%d, intensity:%d, freq:%d od: %d ratio: %d\n", __func__,
		pdata->packet_cnt, pdata->timeout, intensity, pdata->freq, overdrive, pdata->ratio);
}

static int haptic_get_time(struct timed_output_dev *tout_dev)
{
	struct s2mu106_haptic_data *hap_data
		= container_of(tout_dev, struct s2mu106_haptic_data, tout_dev);

	struct hrtimer *timer = &hap_data->timer;
	if (hrtimer_active(timer)) {
		ktime_t remain = hrtimer_get_remaining(timer);
		struct timeval t = ktime_to_timeval(remain);
		return t.tv_sec * 1000 + t.tv_usec / 1000;
	}
	return 0;
}

static void haptic_enable(struct timed_output_dev *tout_dev, int value)
{
	struct s2mu106_haptic_data *hap_data
		= container_of(tout_dev, struct s2mu106_haptic_data, tout_dev);
	struct s2mu106_haptic_platform_data *pdata = hap_data->pdata;
	struct hrtimer *timer = &hap_data->timer;

	kthread_flush_worker(&hap_data->kworker);
	hrtimer_cancel(timer);

	value = min_t(int, value, (int)pdata->max_timeout);
	pdata->timeout = value;

	if (value > 0) {
		mutex_lock(&hap_data->mutex);
		if (pdata->f_packet_en) {
			pdata->packet_running = false;
			pdata->timeout = pdata->haptic_engine[0].time;
			s2mu106_haptic_engine_run_packet(hap_data, pdata->haptic_engine[0]);
		} else {
			/* motor run */
			s2mu106_haptic_onoff(hap_data, true);
#if defined(CONFIG_SSP_MOTOR_CALLBACK)
			setSensorCallback(true, value);
#endif
			if (pdata->multi_frequency)
				pr_info("%s freq: %d intensity: %u duty: %d %u ms\n", __func__, pdata->freq, hap_data->intensity, pdata->ratio, pdata->timeout);
			else
				pr_info("%s intensity: %u duty: %d %u ms\n", __func__, hap_data->intensity, pdata->ratio, pdata->timeout);
		}

		mutex_unlock(&hap_data->mutex);
		hrtimer_start(timer, ns_to_ktime((u64)pdata->timeout * NSEC_PER_MSEC),
					HRTIMER_MODE_REL);
	} else {
		mutex_lock(&hap_data->mutex);
		/* motor stop */
                pdata->f_packet_en = false; 
                pdata->packet_cnt = 0;
                pdata->packet_size = 0;
		s2mu106_haptic_onoff(hap_data, false);

#if defined(CONFIG_SSP_MOTOR_CALLBACK)
		setSensorCallback(false, 0);
#endif
		mutex_unlock(&hap_data->mutex);
		pr_debug("%s : off\n", __func__);
	}

}

static enum hrtimer_restart haptic_timer_func(struct hrtimer *timer)
{
	struct s2mu106_haptic_data *hap_data
		= container_of(timer, struct s2mu106_haptic_data, timer);

	pr_info("%s\n", __func__);

	kthread_queue_work(&hap_data->kworker, &hap_data->kwork);
	return HRTIMER_NORESTART;
}

static void haptic_work(struct kthread_work *work)
{
	struct s2mu106_haptic_data *hap_data
		= container_of(work, struct s2mu106_haptic_data, kwork);
	struct s2mu106_haptic_platform_data *pdata = hap_data->pdata;
	struct hrtimer *timer = &hap_data->timer;
	
	mutex_lock(&hap_data->mutex);

	if (pdata->f_packet_en) {
		pdata->packet_cnt++;
		if (pdata->packet_cnt < pdata->packet_size) {
			pdata->timeout = pdata->haptic_engine[pdata->packet_cnt].time;
			s2mu106_haptic_engine_run_packet(hap_data, pdata->haptic_engine[pdata->packet_cnt]);
			hrtimer_start(timer, ns_to_ktime((u64)pdata->timeout * NSEC_PER_MSEC),
					HRTIMER_MODE_REL);

			goto unlock_without_vib_off;
		} else {
			pdata->f_packet_en = false;
			pdata->packet_cnt = 0;
			pdata->packet_size = 0;
		}
	}

	s2mu106_haptic_onoff(hap_data, false);

#if defined(CONFIG_SSP_MOTOR_CALLBACK)
	setSensorCallback(false,0);
#endif

unlock_without_vib_off:
	mutex_unlock(&hap_data->mutex);
	return;
}

static void set_ratio_for_event(struct s2mu106_haptic_platform_data *pdata, int event_idx)
{
	switch(event_idx) {
	case EVENT_CMD_NONE:
		pdata->event_idx = VIB_EVENT_NONE;
		break;
	case EVENT_CMD_FOLDER_CLOSE:
		pdata->event_idx = VIB_EVENT_FOLDER_CLOSE;
		pdata->ratio = pdata->folder_ratio;
		break;
	case EVENT_CMD_FOLDER_OPEN:
		pdata->event_idx = VIB_EVENT_FOLDER_OPEN;
		pdata->ratio = pdata->normal_ratio;
		break;
	case EVENT_CMD_ACCESSIBILITY_BOOST_ON:
	case EVENT_CMD_ACCESSIBILITY_BOOST_OFF:
	default:
		break;
	}

	pr_info("%s: event: %d, ratio set to %d\n", __func__, pdata->event_idx, pdata->ratio);
}

static int get_event_index_by_command(char *cur_cmd)
{
	int event_idx = 0;
	int cmd_idx = 0;

	pr_info("%s: current state=%s\n", __func__, cur_cmd);

	for(cmd_idx = 0; cmd_idx < EVENT_CMD_MAX; cmd_idx++) {
		if(!strcmp(cur_cmd, s2mu106_sec_vib_event_cmd[cmd_idx])) {
			break;
		}
	}

	switch(cmd_idx) {
		case EVENT_CMD_NONE:
			event_idx = VIB_EVENT_NONE;
			break;
		case EVENT_CMD_FOLDER_CLOSE:
			event_idx = VIB_EVENT_FOLDER_CLOSE;
			break;
		case EVENT_CMD_FOLDER_OPEN:
			event_idx = VIB_EVENT_FOLDER_OPEN;
			break;
		case EVENT_CMD_ACCESSIBILITY_BOOST_ON:
			event_idx = VIB_EVENT_ACCESSIBILITY_BOOST_ON;
			break;
		case EVENT_CMD_ACCESSIBILITY_BOOST_OFF:
			event_idx = VIB_EVENT_ACCESSIBILITY_BOOST_OFF;
			break;
		default:
			break;
	}

	pr_info("%s: cmd=%d event=%d\n", __func__, cmd_idx, event_idx);

	return event_idx;
}

static ssize_t intensity_store(struct device *dev,
        struct device_attribute *devattr, const char *buf, size_t count)
{
	struct timed_output_dev *tdev = dev_get_drvdata(dev);
	struct s2mu106_haptic_data *haptic = container_of(tdev, struct s2mu106_haptic_data, tout_dev);

        int intensity = 0, ret = 0;

        ret = kstrtoint(buf, 0, &intensity);

	if (intensity < 0 || MAX_INTENSITY < intensity) {
		pr_err("%s: intensity is out of rage\n", __func__);
		return -EINVAL;
	}

	s2mu106_haptic_set_intensity(haptic, intensity);

        return count;
}

static ssize_t intensity_show(struct device *dev,
                        struct device_attribute *attr, char *buf)
{
        struct timed_output_dev *tdev = dev_get_drvdata(dev);
	struct s2mu106_haptic_data *haptic = container_of(tdev, struct s2mu106_haptic_data, tout_dev);

        return sprintf(buf, "intensity: %u\n", haptic->intensity);
}

static DEVICE_ATTR(intensity, 0660, intensity_show, intensity_store);

static ssize_t multi_freq_store(struct device *dev,
		struct device_attribute *devattr, const char *buf, size_t count)
{
	struct timed_output_dev *tdev = dev_get_drvdata(dev);
	struct s2mu106_haptic_data *haptic = container_of(tdev, struct s2mu106_haptic_data, tout_dev);

	int set_freq = 0, ret = 0;

	ret = kstrtoint(buf, 0, &set_freq);
	if (ret) {
		pr_err("%s failed to get multi_freq value", __func__);
		return ret;
	}

	if ((set_freq < 0) || (set_freq >= MAX_FREQUENCY)) {
		pr_err("%s freq is out of range\n", __func__);
		return -EINVAL;
	}

	s2mu106_haptic_set_freq(haptic, set_freq);

	return count;
}

static ssize_t multi_freq_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct timed_output_dev *tdev = dev_get_drvdata(dev);
	struct s2mu106_haptic_data *haptic = container_of(tdev, struct s2mu106_haptic_data, tout_dev);
	struct s2mu106_haptic_platform_data *pdata = haptic->pdata;

	return sprintf(buf, "%s %d\n", pdata->multi_frequency ? "MULTI" : "FIXED", pdata->freq);
}

static DEVICE_ATTR(multi_freq, 0660, multi_freq_show, multi_freq_store);

static ssize_t haptic_engine_store(struct device *dev,
	struct device_attribute *devattr, const char *buf, size_t count)
{
	struct timed_output_dev *tdev = dev_get_drvdata(dev);
	struct s2mu106_haptic_data *haptic = container_of(tdev, struct s2mu106_haptic_data, tout_dev);
	struct s2mu106_haptic_platform_data *pdata = haptic->pdata;

	int i = 0, _data = 0, tmp = 0;

	if (sscanf(buf, "%6d", &_data) != 1)
		return count;

	if (_data > PACKET_MAX_SIZE * VIB_PACKET_MAX)
		pr_info("%s, [%d] packet size over\n", __func__, _data);
	else {
		pdata->packet_size = _data / VIB_PACKET_MAX;
		pdata->packet_cnt = 0;
		pdata->f_packet_en = true;

		buf = strstr(buf, " ");

		for (i = 0; i < pdata->packet_size; i++) {
			for (tmp = 0; tmp < VIB_PACKET_MAX; tmp++) {
				if (buf == NULL) {
					pr_err("%s, buf is NULL, Please check packet data again\n",
							__func__);
					pdata->f_packet_en = false;
					return count;
				}

				if (sscanf(buf++, "%6d", &_data) != 1) {
					pr_err("%s, packet data error, Please check packet data again\n",
							__func__);
					pdata->f_packet_en = false;
					return count;
				}

				switch (tmp) {
				case VIB_PACKET_TIME:
					pdata->haptic_engine[i].time = _data;
					break;
				case VIB_PACKET_INTENSITY:
					pdata->haptic_engine[i].intensity = _data;
					break;
				case VIB_PACKET_FREQUENCY:
					pdata->haptic_engine[i].freq = _data;
					break;
				case VIB_PACKET_OVERDRIVE:
					pdata->haptic_engine[i].overdrive = _data;
					break;
				}
				buf = strstr(buf, " ");
			}
		}
	}

	return count;
}

static ssize_t haptic_engine_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct timed_output_dev *tdev = dev_get_drvdata(dev);
	struct s2mu106_haptic_data *haptic = container_of(tdev, struct s2mu106_haptic_data, tout_dev);
	struct s2mu106_haptic_platform_data *pdata = haptic->pdata;

	int i = 0;
	size_t size = 0;

	for (i = 0; i < pdata->packet_size && pdata->f_packet_en &&
			((4 * VIB_BUFSIZE + size) < PAGE_SIZE); i++) {
		size += snprintf(&buf[size], VIB_BUFSIZE, "%u ", pdata->haptic_engine[i].time);
		size += snprintf(&buf[size], VIB_BUFSIZE, "%u ", pdata->haptic_engine[i].intensity);
		size += snprintf(&buf[size], VIB_BUFSIZE, "%u ", pdata->haptic_engine[i].freq);
		size += snprintf(&buf[size], VIB_BUFSIZE, "%u\n", pdata->haptic_engine[i].overdrive);
	}

	return size;
}

static DEVICE_ATTR(haptic_engine, 0660, haptic_engine_show, haptic_engine_store);

static ssize_t vib_enable_store(struct device *dev,
        struct device_attribute *devattr, const char *buf, size_t count)
{
	struct timed_output_dev *tdev = dev_get_drvdata(dev);
	struct s2mu106_haptic_data *hap_data = container_of(tdev, struct s2mu106_haptic_data, tout_dev);

        int enable = 0;
	int ret;
	
        ret = kstrtoint(buf, 0, &enable);
		
	if (enable == 1)
		s2mu106_haptic_onoff(hap_data, true);
	else if (enable == 0)
		s2mu106_haptic_onoff(hap_data, false);
        else {
		s2mu106_haptic_onoff(hap_data, false);
		pr_err("out of rage\n");
		return -EINVAL;
	}

	pr_info("%s, VIB %s\n", __func__, ((enable == 1) ? "ENABLE" : "DISABLE") );

        return count;
}

static ssize_t vib_enable_show(struct device *dev,
                        struct device_attribute *attr, char *buf)
{
        return sprintf(buf, "echo 1 > vib_enable\necho 0 > vib_enable\n");
}

static DEVICE_ATTR(vib_enable, 0660, vib_enable_show, vib_enable_store);

static ssize_t motor_type_show(struct device *dev,
                        struct device_attribute *attr, char *buf)
{
	struct timed_output_dev *tdev = dev_get_drvdata(dev);
	struct s2mu106_haptic_data *haptic = container_of(tdev, struct s2mu106_haptic_data, tout_dev);

	pr_info("%s: %s\n", __func__, haptic->pdata->vib_type);
        return sprintf(buf, "%s\n", haptic->pdata->vib_type);
}

DEVICE_ATTR(motor_type, 0660, motor_type_show, NULL);

static ssize_t event_cmd_show(struct device *dev, 
		struct device_attribute *attr, char *buf)
{
	pr_info("%s: [%s]\n", __func__, s2mu106_sec_prev_event_cmd);
	return snprintf(buf, MAX_STR_LEN_EVENT_CMD, "%s\n", s2mu106_sec_prev_event_cmd);
}

static ssize_t event_cmd_store(struct device *dev,
		struct device_attribute *devattr, const char *buf, size_t size)
{
	struct timed_output_dev *tdev = dev_get_drvdata(dev);
	struct s2mu106_haptic_data *haptic = container_of(tdev, struct s2mu106_haptic_data, tout_dev);
	struct s2mu106_haptic_platform_data *pdata = haptic->pdata;

	char *cmd;
	int idx = 0;
	int ret = 0;

	pr_info("%s\n", __func__);

	if (size > MAX_STR_LEN_EVENT_CMD) {
		pr_err("%s: size(%zu) is too long.\n", __func__, size);
		goto error;
	}

	cmd = kzalloc(size+1, GFP_KERNEL);
	if (!cmd)
		goto error;

	ret = sscanf(buf, "%s", cmd);
	if (ret != 1)
		goto error1;

	idx = get_event_index_by_command(cmd);
	set_ratio_for_event(pdata, idx);

	ret = sscanf(cmd, "%s", s2mu106_sec_prev_event_cmd);
	if (ret != 1)
		goto error1;

error1:
	kfree(cmd);
error:
	return size;
}

static DEVICE_ATTR(event_cmd, 0660, event_cmd_show, event_cmd_store);

static ssize_t pwm_duty_store(struct device *dev,
        struct device_attribute *devattr, const char *buf, size_t count)
{
	struct timed_output_dev *tdev = dev_get_drvdata(dev);
	struct s2mu106_haptic_data *hap_data = container_of(tdev, struct s2mu106_haptic_data, tout_dev);
	struct s2mu106_haptic_platform_data *pdata = hap_data->pdata;

        int set_duty = 0;
	int ret;
	
        ret = kstrtoint(buf, 0, &set_duty);

	pdata->ratio = set_duty;

	s2mu106_haptic_set_intensity(hap_data, hap_data->intensity);

	pr_info("%s: pwm_duty: %d\n", __func__, pdata->ratio);

        return count;
}

static ssize_t pwm_duty_show(struct device *dev,
                        struct device_attribute *attr, char *buf)
{
	struct timed_output_dev *tdev = dev_get_drvdata(dev);
	struct s2mu106_haptic_data *hap_data = container_of(tdev, struct s2mu106_haptic_data, tout_dev);
	struct s2mu106_haptic_platform_data *pdata = hap_data->pdata;

	pr_info("%s: pwm_duty: %d\n", __func__, pdata->ratio);
        return sprintf(buf, "pwm_duty: %d\n", pdata->ratio);
}

static DEVICE_ATTR(pwm_duty, 0660, pwm_duty_show, pwm_duty_store);


#if defined(CONFIG_OF)
static int s2mu106_haptic_parse_dt(struct device *dev,
			struct s2mu106_haptic_data *haptic)
{
	struct device_node *np = of_find_node_by_name(NULL, "s2mu106-haptic");
	struct s2mu106_haptic_platform_data *pdata = haptic->pdata;
	u32 temp;
	int ret = 0, i = 0;
	const char *type;

	pr_info("%s : start dt parsing\n", __func__);

	if (np == NULL) {
		pr_err("%s : error to get dt node\n", __func__);
		goto err_parsing_dt;
	}

	ret = of_property_read_u32(np, "haptic,normal_ratio", &temp);
	if (ret) {
		pr_err("%s : error to get dt node normal_ratio\n", __func__);
		goto err_parsing_dt;
	} else
		pdata->normal_ratio = (int)temp;

	pr_info("normal ratio = %d\n", pdata->normal_ratio);

	ret = of_property_read_u32(np, "haptic,overdrive_ratio", &temp);
	if (ret) {
		pr_err("%s : error to get dt node overdrive_ratio\n", __func__);
		goto err_parsing_dt;
	} else
		pdata->overdrive_ratio = (int)temp;

	pr_info("overdrive ratio = %d\n", pdata->overdrive_ratio);

	ret = of_property_read_u32(np, "haptic,folder_ratio", &temp);
	if (ret) {
		pr_err("%s : error to get dt node folder_ratio\n", __func__);
		goto err_parsing_dt;
	} else
		pdata->folder_ratio = (int)temp;

	pr_info("folder ratio = %d\n", pdata->folder_ratio);

	ret = of_property_read_u32(np, "haptic,high_temp_ratio",
			&pdata->high_temp_ratio);
	if (ret) {
		pr_err("%s: temp_duty_ratio isn't used\n", __func__);
		pdata->high_temp_ratio = 0;
	}

	pr_info("high temp ratio = %d\n", pdata->high_temp_ratio);

	ret = of_property_read_u32(np, "haptic,temperature",
			&pdata->temperature);
	if (ret) {
		pr_err("%s: temperature isn't used\n", __func__);
		pdata->temperature = 0;
	}

	pr_info("temperature = %d\n", pdata->temperature);

	/* initial pwm duty ratio to normal ratio */
	pdata->ratio = pdata->normal_ratio;

	ret = of_property_read_u32(np, "haptic,multi_frequency", &temp);
	if (ret) {
		pr_err("%s : error to get dt node multi_frequency\n", __func__);
		pdata->multi_frequency = 0;
	} else
		pdata->multi_frequency = (int)temp;

		pr_info("multi frequency = %d\n", pdata->multi_frequency);

	if (pdata->multi_frequency) {
		pdata->multi_freq_period
			= devm_kzalloc(dev, sizeof(u32)*pdata->multi_frequency, GFP_KERNEL);
		if (!pdata->multi_freq_period) {
			pr_err("%s: failed to allocate period data\n", __func__);
			goto err_parsing_dt;
		}

		ret = of_property_read_u32_array(np, "haptic,period", pdata->multi_freq_period,
				pdata->multi_frequency);
		if (ret) {
			pr_err("%s : error to get dt node period\n", __func__);
			goto err_parsing_dt;
		}

		for (i = 0; i < pdata->multi_frequency; i++) {
			pr_info("period[%d] = %d\n", i, pdata->multi_freq_period[i]);
		}

		pdata->period = pdata->multi_freq_period[0];
		pdata->duty = (pdata->period *(pdata->ratio)) / 100;
		pdata->freq = 0;

		pr_info("%s, initial ratio = %d, duty = %d, period = %d\n", __func__, pdata->ratio, pdata->duty, pdata->period);
	} else {
		ret = of_property_read_u32(np, "haptic,period", &temp);
		if (ret < 0)
			/* 205Hz */
			pdata->period = 38109;
		else
			pdata->period = temp;

		pdata->duty = (pdata->period *(pdata->ratio)) / 100;

		pr_info("duty = %d\n", pdata->duty);
		pr_info("period = %d\n", pdata->period);
	}

	ret = of_property_read_u32(np, "haptic,intensity", &temp);
	if (ret < 0) {
		pr_info("%s : intensity set to 100%%\n", __func__);
		pdata->intensity = 100;
	} else {
		pr_info("%s : intensity set to %d%%\n", __func__,temp);
		pdata->intensity = (u32)temp;
	}

	/*	Haptic operation mode
		0 : S2MU106_HAPTIC_ERM_I2C
		1 : S2MU106_HAPTIC_ERM_GPIO
		2 : S2MU106_HAPTIC_LRA
		default : S2MU106_HAPTIC_ERM_GPIO
	*/
	pdata->hap_mode = 1;
	ret = of_property_read_u32(np, "haptic,operation_mode", &temp);
	if (ret < 0) {
		pr_err("%s : eror to get operation mode\n", __func__);
		goto err_parsing_dt;
	} else
		pdata->hap_mode = temp;

	if (pdata->hap_mode == S2MU106_HAPTIC_LRA) {
		haptic->pwm = devm_of_pwm_get(dev, np, NULL);
		if (IS_ERR(haptic->pwm)) {
			pr_err("%s: error to get pwms\n", __func__);
			goto err_parsing_dt;
		}
	}

	ret = pdata->motor_en = of_get_named_gpio(np, "haptic,motor_en", 0);
	if (ret < 0) {
		pr_err("%s : can't get motor_en\n", __func__);
	}
	
	ret = of_property_read_u32(np, "haptic,max_timeout", &temp);
	if (ret < 0) {
		pr_err("%s : error to get dt node max_timeout\n", __func__);
	} else
		pdata->max_timeout = (u16)temp;
	
	ret = of_property_read_string(np,
				"haptic,type", &type);
	if (ret) {
		pr_err("%s : error to get dt node type\n", __func__);
		pdata->vib_type = NULL;
	} else
		pdata->vib_type = type;

	/* Haptic boost setting */
	pdata->hbst.en = (of_find_property(np, "haptic,hbst_en", NULL)) ? true : false;

	pdata->hbst.automode =
		(of_find_property(np, "haptic,hbst_automode", NULL)) ? true : false;

	ret = of_property_read_u32(np, "haptic,boost_level", &temp);
	if (ret < 0)
		pdata->hbst.level = 5500;
	else
		pdata->hbst.level = temp;

	/* parsing info */
	pr_info("%s :operation_mode = %d, HBST_EN %s, HBST_AUTO_MODE %s\n", __func__,
			pdata->hap_mode,
			pdata->hbst.en ? "enabled" : "disabled",
			pdata->hbst.automode ? "enabled" : "disabled");
	pdata->init_hw = NULL;

	return 0;

err_parsing_dt:
	return -1;
}
#endif

static void s2mu106_haptic_initial(struct s2mu106_haptic_data *haptic)
{
	u8 data;

	haptic->hap_mode = haptic->pdata->hap_mode;

	/* Haptic Boost initial setting */
	if (haptic->pdata->hbst.en){
		pr_info("%s : Haptic Boost Enable - Auto mode(%s)\n", __func__,
				haptic->pdata->hbst.automode ? "enabled" : "disabled");
		/* Boost voltage level setting
			default : 5.5V */
		s2mu106_set_boost_voltage(haptic, haptic->pdata->hbst.level);

		/* check haptic boost voltage before turning on haptic */
		s2mu106_update_reg(haptic->i2c, S2MU106_REG_HT_OTP0,
			0, HBST_OK_MASK_EN);

		s2mu106_update_reg(haptic->i2c, S2MU106_REG_HBST_CTRL0,
			0, SEL_HBST_HAPTIC_MASK);
	} else {
		/* haptic operating without haptic boost */
		s2mu106_update_reg(haptic->i2c,	S2MU106_REG_HBST_CTRL0,
			0, (SEL_HBST_HAPTIC_MASK | EN_HBST_EXT_MASK));
		/* not check haptic boost voltage */
		s2mu106_update_reg(haptic->i2c, S2MU106_REG_HT_OTP0,
			HBST_OK_MASK_EN, (HBST_OK_MASK_EN | HBST_OK_FORCE_EN));

		pr_info("%s : HDVIN - Vsys HDVIN voltage : Min 3.5V\n", __func__);
#if IS_ENABLED(CONFIG_MOTOR_VOLTAGE_3P3)
		s2mu106_update_reg(haptic->i2c, S2MU106_REG_HT_OTP2, 0x40, VCEN_SEL_MASK);
		s2mu106_update_reg(haptic->i2c, S2MU106_REG_HT_OTP3, 0x01, VCENUP_TRIM_MASK);
#else
		s2mu106_update_reg(haptic->i2c, S2MU106_REG_HT_OTP2, 0x00, VCEN_SEL_MASK);
		s2mu106_update_reg(haptic->i2c, S2MU106_REG_HT_OTP3, 0x03, VCENUP_TRIM_MASK);
#endif
	}
	
	/* Intensity setting */
	s2mu106_haptic_set_intensity(haptic, haptic->intensity);

	/* mode setting */
	switch (haptic->hap_mode) {
	case S2MU106_HAPTIC_LRA:
		data = HAPTIC_MODE_OFF;
		pwm_config(haptic->pwm, haptic->pdata->duty,
				haptic->pdata->period);
		if (haptic->pdata->hbst.en){
			s2mu106_update_reg(haptic->i2c, S2MU106_REG_OV_BK_OPTION,
				LRA_MODE_SET_MASK | LRA_BST_MODE_SET_MASK,
				LRA_MODE_SET_MASK | LRA_BST_MODE_SET_MASK);
		} else {
			s2mu106_update_reg(haptic->i2c, S2MU106_REG_OV_BK_OPTION,
				LRA_MODE_SET_MASK, LRA_MODE_SET_MASK);
		}
		s2mu106_write_reg(haptic->i2c, S2MU106_REG_FILTERCOEF1, 0x7F);
		s2mu106_write_reg(haptic->i2c, S2MU106_REG_FILTERCOEF2, 0x5A);
		s2mu106_write_reg(haptic->i2c, S2MU106_REG_FILTERCOEF3, 0x02);
		s2mu106_write_reg(haptic->i2c, S2MU106_REG_PWM_CNT_NUM, 0x40);
		s2mu106_update_reg(haptic->i2c, S2MU106_REG_OV_WAVE_NUM, 0xF0, 0xF0);
		break;
	case S2MU106_HAPTIC_ERM_GPIO:
		data = ERM_HDPWM_MODE_EN;
		if (gpio_is_valid(haptic->motor_en)) {
			pr_info("%s : MOTOR_EN enable\n", __func__);
			haptic->motor_en = haptic->pdata->motor_en;
			gpio_request_one(haptic->motor_en, GPIOF_OUT_INIT_LOW, "MOTOR_EN");
			gpio_free(haptic->motor_en);
		}
		break;
	case S2MU106_HAPTIC_ERM_I2C:
		data = HAPTIC_MODE_OFF;
		break;
	default:
		data = ERM_HDPWM_MODE_EN;
		break;
	}
	s2mu106_write_reg(haptic->i2c, S2MU106_REG_HAPTIC_MODE, data);

	if (haptic->hap_mode == S2MU106_HAPTIC_ERM_I2C ||
		haptic->hap_mode == S2MU106_HAPTIC_ERM_GPIO) {
		s2mu106_write_reg(haptic->i2c, S2MU106_REG_PERI_TAR1, 0x00);
		s2mu106_write_reg(haptic->i2c, S2MU106_REG_PERI_TAR2, 0x00);
		s2mu106_write_reg(haptic->i2c, S2MU106_REG_DUTY_TAR1, 0x00);
		s2mu106_write_reg(haptic->i2c, S2MU106_REG_DUTY_TAR2, 0x00);
#if IS_ENABLED(CONFIG_MOTOR_VOLTAGE_3P3)
		s2mu106_write_reg(haptic->i2c, S2MU106_REG_AMPCOEF1, 0x5D);
#else
		s2mu106_write_reg(haptic->i2c, S2MU106_REG_AMPCOEF1, 0x74);
#endif
	}

	pr_info("%s, haptic operation mode = %d\n", __func__, haptic->hap_mode);

	sscanf(s2mu106_sec_vib_event_cmd[0], "%s", s2mu106_sec_prev_event_cmd);
	haptic->pdata->save_vib_event.DATA = 0;
#if defined(CONFIG_FOLDER_HALL)
	haptic->pdata->save_vib_event.EVENTS.FOLDER_STATE = 1; // init CLOSE
#endif

}

static struct of_device_id s2mu106_haptic_match_table[] = {
	{ .compatible = "sec,s2mu106-haptic",},
	{},
};

static int s2mu106_haptic_probe(struct platform_device *pdev)
{
	struct s2mu106_dev *s2mu106 = dev_get_drvdata(pdev->dev.parent);
	struct s2mu106_haptic_data *haptic;
	struct task_struct *kworker_task;
	int ret = 0;
	int error = 0;

	pr_info("%s Start\n", __func__);
	haptic = devm_kzalloc(&pdev->dev,
			sizeof(struct s2mu106_haptic_data), GFP_KERNEL);

	if (!haptic) {
		pr_err("%s: Failed to allocate memory\n", __func__);
		return -ENOMEM;
	}

	haptic->dev = &pdev->dev;
	haptic->i2c = s2mu106->haptic;

	haptic->pdata = devm_kzalloc(&pdev->dev, sizeof(*(haptic->pdata)), GFP_KERNEL);
	if (!haptic->pdata) {
		dev_err(&pdev->dev, "Failed to allocate memory\n");
		ret = -ENOMEM;
		goto err_kthread;
	}

	ret = s2mu106_haptic_parse_dt(&pdev->dev, haptic);
	if (ret < 0)
		goto err_kthread;

	platform_set_drvdata(pdev, haptic);

	kthread_init_worker(&haptic->kworker);
	kworker_task = kthread_run(kthread_worker_fn, &haptic->kworker, "s2mu106_haptic");
	if (IS_ERR(kworker_task)) {
		pr_err("Failed to create message pump task\n");
		error = -ENOMEM;
		goto err_kthread;
	}

	kthread_init_work(&haptic->kwork, haptic_work);
	spin_lock_init(&(haptic->lock));
	mutex_init(&haptic->mutex);

	if (haptic->pdata->hap_mode == S2MU106_HAPTIC_LRA) {
		pwm_config(haptic->pwm, haptic->pdata->duty, haptic->pdata->period);
		pr_info("%s : duty: %d period: %d\n", __func__, haptic->pdata->duty, haptic->pdata->period);
	}

	/* hrtimer init */
	hrtimer_init(&haptic->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	haptic->timer.function = haptic_timer_func;

	/* timed_output_dev init*/
	haptic->tout_dev.name = "vibrator";
	haptic->tout_dev.get_time = haptic_get_time;
	haptic->tout_dev.enable = haptic_enable;

	wake_lock_init(&vib_wake_lock, WAKE_LOCK_SUSPEND, "vib_present");	
	pm_qos_add_request(&pm_qos_req, PM_QOS_CPU_DMA_LATENCY, PM_QOS_DEFAULT_VALUE);

	haptic->intensity = 100;
	error = timed_output_dev_register(&haptic->tout_dev);
	if (error < 0) {
		error = -EFAULT;
		pr_err("Failed to register timed_output : %d\n", error);
		goto err_timed_output_register;
	}
	ret = sysfs_create_file(&haptic->tout_dev.dev->kobj,
				&dev_attr_motor_type.attr);
	ret = sysfs_create_file(&haptic->tout_dev.dev->kobj,
				&dev_attr_event_cmd.attr);
	ret = sysfs_create_file(&haptic->tout_dev.dev->kobj,
				&dev_attr_vib_enable.attr);

	if (haptic->pdata->hap_mode == S2MU106_HAPTIC_LRA) {
		ret = sysfs_create_file(&haptic->tout_dev.dev->kobj,
					&dev_attr_intensity.attr);
		ret = sysfs_create_file(&haptic->tout_dev.dev->kobj,
					&dev_attr_pwm_duty.attr);
		if (haptic->pdata->multi_frequency) {
			ret = sysfs_create_file(&haptic->tout_dev.dev->kobj,
						&dev_attr_multi_freq.attr);
			ret = sysfs_create_file(&haptic->tout_dev.dev->kobj,
						&dev_attr_haptic_engine.attr);
		}
	}

	if (ret < 0) {
		pr_err("Failed to register sysfs : %d\n", ret);
		goto err_timed_output_register;
	}

	s2mu106_haptic_initial(haptic);

	return error;

err_timed_output_register:
	if (haptic->pdata->hap_mode == S2MU106_HAPTIC_LRA)
		pwm_free(haptic->pwm);
err_kthread:
	haptic = NULL;
	return error;
}

static int s2mu106_haptic_remove(struct platform_device *pdev)
{
	struct s2mu106_haptic_data *haptic = platform_get_drvdata(pdev);

	pm_qos_remove_request(&pm_qos_req);
	timed_output_dev_unregister(&haptic->tout_dev);
	if (haptic->hap_mode == S2MU106_HAPTIC_LRA)
		pwm_free(haptic->pwm);
	s2mu106_haptic_onoff(haptic, false);
	return 0;
}

static int s2mu106_haptic_suspend(struct device *dev)
{
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct s2mu106_haptic_data *haptic = platform_get_drvdata(pdev);

	pr_info("%s\n", __func__);

	if (haptic == NULL) {
		pr_info("%s: data is NULL, return\n", __func__);
		return 0;
	}

	kthread_flush_worker(&haptic->kworker);
	hrtimer_cancel(&haptic->timer);
	s2mu106_haptic_onoff(haptic, false);
	return 0;
}

static int s2mu106_haptic_resume(struct device *dev)
{
	pr_info("%s\n", __func__);
	return 0;
}


static SIMPLE_DEV_PM_OPS(s2mu106_haptic_pm_ops, s2mu106_haptic_suspend, s2mu106_haptic_resume);
static struct platform_driver s2mu106_haptic_driver = {
	.driver = {
		.name	= "s2mu106-haptic",
		.owner	= THIS_MODULE,
		.pm	= &s2mu106_haptic_pm_ops,
		.of_match_table = s2mu106_haptic_match_table,
	},
	.probe		= s2mu106_haptic_probe,
	.remove		= s2mu106_haptic_remove,
};

static int __init s2mu106_haptic_init(void)
{
	pr_info("%s\n", __func__);
	return platform_driver_register(&s2mu106_haptic_driver);
}
late_initcall(s2mu106_haptic_init);

static void __exit s2mu106_haptic_exit(void)
{
	platform_driver_unregister(&s2mu106_haptic_driver);
}
module_exit(s2mu106_haptic_exit);

MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("s2mu106 haptic driver");
