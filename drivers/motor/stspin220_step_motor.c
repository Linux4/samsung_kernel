/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/hrtimer.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/workqueue.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/wakelock.h>
#include <asm/io.h>
#include <linux/sec_class.h>
#include <linux/pwm.h>
#include <linux/delay.h>
#include <linux/power_supply.h>

#define HIGH	1
#define LOW	0

#define ON	1
#define OFF	0

#define UP	1
#define DOWN	-1

#define STARTING_SPEED	13333
#define FULL_SPEED	10416
#define ENDING_SPEED	40000

#define STARTING_SPEED_LOW_TEMP	1280000
#define FULL_SPEED_LOW_TEMP	640000
#define ENDING_SPEED_LOW_TEMP	426667

/*
 * starting time: 30ms
 * full speed time: 800ms
 * ending time: 170ms
 * 
 * FULL_SPEED_TIME	starting time 
 * ENDING_TIME		starting time + full speed time
 * TIMEOUT_TIME		starting time + full speed time + ending time
 */
#define FULL_SPEED_TIME		30	/* starting time */
#define ENDING_TIME		860	/* starting time + full speed time*/
#define TIMEOUT_TIME		910	/* starting time + full speed time + ending time */

#define FULL_SPEED_TIME_LOW_TEMP	1000		/* starting time */
#define ENDING_TIME_LOW_TEMP		2150	/* starting time + full speed time*/
#define TIMEOUT_TIME_LOW_TEMP		2225	/* starting time + full speed time + ending time */

int starting_period = 33333;
int full_speed_period = 12987;
int ending_period = 166666;

int cold = false;

enum step_mode {
	FULL_STEP,
	ONE_THIRTYTWO_STEP,
};

struct stspin220_data {
	struct device *dev;
	struct pwm_device *pwm_dev;
	struct work_struct work_motor_off;
	struct work_struct work_change_speed;
	struct work_struct work_motor_retry_on;
	struct workqueue_struct *queue;
	struct hrtimer full_speed_timer;
	struct hrtimer ending_timer;
	struct hrtimer off_timer;
	struct hrtimer retry_on_timer;
	struct mutex lock;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pin_active;
	struct pinctrl_state *pin_disable;
	struct wake_lock motor_wake_lock;

	int state;
	int direction;
	int step_dir;
	int step_m0;
	int step_m1;
	int step_enable;
	int step_step;
	int step_sleep;
	int step_boost_en;
	int starting_speed;
	int full_speed;
	int ending_speed;
	int full_speed_change_time;
	int ending_speed_change_time;
	int timeout_speed_change_time;
	int is_retry;

	u64 period;
	u64 duty;
};
/*
static void stspin220_get_dump(struct stspin220_data *data)
{
	int val;

	val = gpio_get_value(data->step_dir);
	pr_info("[MOTOR] step_dir     : %s\n", val ? "HIGH" : "LOW");
	val = gpio_get_value(data->step_m0);
	pr_info("[MOTOR] step_m0      : %s\n", val ? "HIGH" : "LOW");
	val = gpio_get_value(data->step_m1);
	pr_info("[MOTOR] step_m1      : %s\n", val ? "HIGH" : "LOW");
	val = gpio_get_value(data->step_enable);
	pr_info("[MOTOR] step_enable  : %s\n", val ? "HIGH" : "LOW");
	val = gpio_get_value(data->step_step);
	pr_info("[MOTOR] step_step    : %s\n", val ? "HIGH" : "LOW");
	val = gpio_get_value(data->step_boost_en);
	pr_info("[MOTOR] step_boost_en: %s\n", val ? "HIGH" : "LOW");
	val = gpio_get_value(data->step_sleep);
	pr_info("[MOTOR] step_sleep   : %s\n", val ? "HIGH" : "LOW");
}
*/
static void stspin220_set_step_mode(struct stspin220_data *data, enum step_mode mode)
{
	pr_info("[MOTOR] %s, mode: %d\n",__func__, mode);

	switch (mode) {
	case FULL_STEP:
		gpio_set_value(data->step_m0, LOW);
		gpio_set_value(data->step_m1, LOW);
		break;
	case ONE_THIRTYTWO_STEP:
		gpio_set_value(data->step_m0, HIGH);
		gpio_set_value(data->step_m1, LOW);
		break;
	default:
		break;
	}
}

static int get_battery_temp(int * temp)
{
	static struct power_supply *psy_bat = NULL;
	int rc;
	union power_supply_propval value = {0, };

	if (!psy_bat) {
		psy_bat = power_supply_get_by_name("battery");
		if (!psy_bat) {
			pr_err("[MOT] %s: Failed to Register psy_bat\n", __func__);
			return -1;
		}
	}
	rc = power_supply_get_property(psy_bat,
			POWER_SUPPLY_PROP_TEMP, &value);
	if (rc < 0) {
		pr_err("%s: Fail to get usb real_type_prop (%d=>%d)\n", __func__, POWER_SUPPLY_PROP_TEMP, rc);
		return -1;
	}
	*temp = value.intval;
	
	return 0;
}

static void motor_check_temp(struct stspin220_data *data)
{
	int temp = 0;
	int rc = 0;

	rc = get_battery_temp(&temp);
	if (rc)
		pr_err("[MOT] Failed to get battery-temp, rc = %d\n", rc);
	else
		pr_info("[MOT] %s: temp: %d\n", __func__, temp);

	if (temp <= 80) {
		cold = true;
		stspin220_set_step_mode(data, FULL_STEP);
		data->starting_speed = STARTING_SPEED_LOW_TEMP;
		data->full_speed = FULL_SPEED_LOW_TEMP;
		data->ending_speed = ENDING_SPEED_LOW_TEMP;
		data->full_speed_change_time = FULL_SPEED_TIME_LOW_TEMP;
		data->ending_speed_change_time = ENDING_TIME_LOW_TEMP;
		data->timeout_speed_change_time = TIMEOUT_TIME_LOW_TEMP;
	} else {
		cold = false;
		stspin220_set_step_mode(data, ONE_THIRTYTWO_STEP);
		data->starting_speed = STARTING_SPEED;
		data->full_speed = FULL_SPEED;
		data->ending_speed = ENDING_SPEED;
		data->full_speed_change_time = FULL_SPEED_TIME;
		data->ending_speed_change_time = ENDING_TIME;
		data->timeout_speed_change_time = TIMEOUT_TIME; 
	}
}

static void stspin220_set_stck(struct stspin220_data *data, int period)
{
	struct pwm_state pstate;
	int duty;
	int ret;

	pr_info("[MOTOR] %s: period is %d\n", __func__, period);

	pwm_get_state(data->pwm_dev, &pstate);
	pstate.enabled = period ? true : false;

	/* To avoid pwm_apply_state error, we set period to 1 if it is 0 */
	if (period == OFF)
		period = 1;

	duty   = period / 2;

	pstate.period = period;
	pstate.duty_cycle = duty;
	pstate.output_type = PWM_OUTPUT_FIXED;
	/* Use default pattern in PWM device */
	pstate.output_pattern = NULL;

	ret = pwm_apply_state(data->pwm_dev, &pstate);
	if (ret < 0)
		dev_err(data->dev, "[MOTOR] Apply PWM state for sliding motor failed, rc=%d\n", ret);
}

static void stspin220_set_power(struct stspin220_data *data, int onoff)
{
	if (onoff == ON)
		gpio_set_value(data->step_boost_en, HIGH);
	else
		gpio_set_value(data->step_boost_en, LOW);
}

static void stspin220_set_rst(struct stspin220_data *data, int onoff)
{
	if (onoff == ON)
		gpio_set_value(data->step_sleep, HIGH);
	else
		gpio_set_value(data->step_sleep, LOW);
}

static void stspin220_set_dir(struct stspin220_data *data, int updown)
{
	if (updown == UP)
		gpio_set_value(data->step_dir, HIGH);
	else
		gpio_set_value(data->step_dir, LOW);
}

static void stspin220_enable(struct stspin220_data *data, int enable)
{
	if (enable == ON)
		gpio_set_value(data->step_enable, HIGH);
	else
		gpio_set_value(data->step_enable, LOW);
}

static void set_motor(struct stspin220_data *data, int enable)
{
	int set_dir = data->direction == 1 ? UP : DOWN;

	if (enable) {
		wake_lock_timeout(&data->motor_wake_lock, msecs_to_jiffies(1000));
		stspin220_set_power(data, ON);
		udelay(1);
		stspin220_set_rst(data, ON);
		stspin220_enable(data, ON);
		stspin220_set_dir(data, set_dir);
		
	pr_info("[MOTOR] set_motor: data->is_retry(%d), cold(%d)\n", data->is_retry, cold);
		if(data->is_retry)
		{
			if(data->is_retry == 3)
			{
				if(cold)
				{
					stspin220_set_stck(data, data->starting_speed);
					hrtimer_start(&data->retry_on_timer, ktime_set(200 / 1000,
					(200 % 1000) * 1000000), HRTIMER_MODE_REL);
				}
				else
				{
					stspin220_set_stck(data, 20000);
					hrtimer_start(&data->retry_on_timer, ktime_set(100 / 1000,
					(100 % 1000) * 1000000), HRTIMER_MODE_REL);
				}
			}
			if(data->is_retry == 2)
			{
				if(cold)
				{
					stspin220_set_stck(data, data->starting_speed);
					hrtimer_start(&data->retry_on_timer, ktime_set(200 / 1000,
					(200 % 1000) * 1000000), HRTIMER_MODE_REL);
				}
				else
				{
					stspin220_set_stck(data, 20000);
					hrtimer_start(&data->retry_on_timer, ktime_set(200 / 1000,
					(200 % 1000) * 1000000), HRTIMER_MODE_REL);
				}
			}
			if(data->is_retry == 1)
			{
				if(cold)
				{
					stspin220_set_stck(data, data->starting_speed);
					hrtimer_start(&data->retry_on_timer, ktime_set(200 / 1000,
					(200 % 1000) * 1000000), HRTIMER_MODE_REL);
				}
				else
				{
					stspin220_set_stck(data, 20000);
					hrtimer_start(&data->retry_on_timer, ktime_set(50 / 1000,
					(50 % 1000) * 1000000), HRTIMER_MODE_REL);
				}
			}

		}
		else
		{
		stspin220_set_stck(data, data->starting_speed);

		hrtimer_start(&data->full_speed_timer, ktime_set(data->full_speed_change_time / 1000,
					(data->full_speed_change_time % 1000) * 1000000), HRTIMER_MODE_REL);
		hrtimer_start(&data->ending_timer, ktime_set(data->ending_speed_change_time / 1000,
					(data->ending_speed_change_time % 1000) * 1000000), HRTIMER_MODE_REL);
		hrtimer_start(&data->off_timer, ktime_set(data->timeout_speed_change_time / 1000,
					(data->timeout_speed_change_time % 1000) * 1000000), HRTIMER_MODE_REL);
		}
	} else {
		stspin220_set_stck(data, OFF);
		stspin220_set_dir(data, DOWN);
		stspin220_enable(data, OFF);
		stspin220_set_rst(data, OFF);
		stspin220_set_power(data, OFF);
		wake_unlock(&data->motor_wake_lock);

		data->is_retry = false;
	}

	pr_info("[MOTOR] is %s, direction is %s\n",
			enable ? "enabled": "disabled", data->direction == 1 ? "UP" : "DOWN");
}

static void motor_enable(struct stspin220_data *data, int value, int direction)
{
	mutex_lock(&data->lock);
	hrtimer_cancel(&data->full_speed_timer);
	hrtimer_cancel(&data->ending_timer);
	hrtimer_cancel(&data->off_timer);

	if (!value)
		pr_info("[MOTOR] OFF\n");
	else
		pr_info("[MOTOR] ON, direction: %d\n" , direction);

	data->state = value;
	data->direction = direction;

	mutex_unlock(&data->lock);
	set_motor(data, data->state);
}

static ssize_t pwm_active_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
        sprintf(buf, "Starting Period: %d\nFull speed Period: %d\nEnding Period: %d\n",
			 starting_period, full_speed_period, ending_period);

        return strlen(buf);
}
static ssize_t pwm_active_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct stspin220_data *data = dev_get_drvdata(dev);
	int period1, period2, period3;

	int ret;

	ret = sscanf(buf, "%d %d %d", &period1, &period2, &period3);
	if (ret == 0) {
		pr_info("[MOTOR] sscanf fail\n");
		return -EINVAL;
	}

	data->period = period2;
	data->duty = period2 / 2;

	starting_period = period1;
	full_speed_period = period2;
	ending_period = period3;

	return size;
}
static DEVICE_ATTR(pwm_active, 0660, pwm_active_show, pwm_active_store);

static ssize_t test_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct stspin220_data *data = dev_get_drvdata(dev);
	int dir, m0, m1, enable, step, boost_en, sleep;

	dir = gpio_get_value(data->step_dir);
	m0 = gpio_get_value(data->step_m0);
	m1 = gpio_get_value(data->step_m1);
	enable = gpio_get_value(data->step_enable);
	step = gpio_get_value(data->step_step);
	boost_en = gpio_get_value(data->step_boost_en);
	sleep = gpio_get_value(data->step_sleep);

	return sprintf(buf, "[MOTOR] step_dir: %d step_m0: %d step_m1: %d step_enable: %d step_step: %d step_boost_en: %d step_sleep:%d\n",
		 dir, m0, m1, enable, step, boost_en, sleep);
}

static ssize_t test_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct stspin220_data *data = dev_get_drvdata(dev);
	int value, get_val;
	int ret;
	char gpio[10] = "";

	ret = sscanf(buf, "%s %d", gpio, &value);

	if (!strcmp(gpio, "DIR")) {
		pr_info("[MOTOR] set dir pin to %s\n", value? "HIGH" : "LOW");
		gpio_set_value(data->step_dir, value);

		get_val = gpio_get_value(data->step_dir);
		pr_info("[MOTOR] step_dir     : %s\n", get_val ? "HIGH" : "LOW");
	}
	else if (!strcmp(gpio, "M0")) {
		pr_info("[MOTOR] set m0 pin to %s\n", value? "HIGH" : "LOW");
		gpio_set_value(data->step_m0, value);

		get_val = gpio_get_value(data->step_m0);
		pr_info("[MOTOR] step_m0      : %s\n", get_val ? "HIGH" : "LOW");
	}
	else if (!strcmp(gpio, "M1")) {
		pr_info("[MOTOR] set m1 pin to %s\n", value? "HIGH" : "LOW");
		gpio_set_value(data->step_m1, value);

		get_val = gpio_get_value(data->step_m1);
		pr_info("[MOTOR] step_m1      : %s\n", get_val ? "HIGH" : "LOW");
	}
	else if (!strcmp(gpio, "ENABLE")) {
		pr_info("[MOTOR] set enable pin to %s\n", value? "HIGH" : "LOW");
		gpio_set_value(data->step_enable, value);

		get_val = gpio_get_value(data->step_enable);
		pr_info("[MOTOR] step_enable  : %s\n", get_val ? "HIGH" : "LOW");
	}
	else if (!strcmp(gpio, "STEP")) {
		pr_info("[MOTOR] set step pin to %s\n", value? "HIGH" : "LOW");
		gpio_set_value(data->step_step, value);

		get_val = gpio_get_value(data->step_step);
		pr_info("[MOTOR] step_step    : %s\n", get_val ? "HIGH" : "LOW");
	}
	else if (!strcmp(gpio, "BOOST")) {
		pr_info("[MOTOR] set boost_en pin to %s\n", value? "HIGH" : "LOW");
		gpio_set_value(data->step_boost_en, value);

		get_val = gpio_get_value(data->step_boost_en);
		pr_info("[MOTOR] step_boost_en: %s\n", get_val ? "HIGH" : "LOW");
	}
	else if (!strcmp(gpio, "SLEEP")) {
		pr_info("[MOTOR] set sleep pin to %s\n", value? "HIGH" : "LOW");
		gpio_set_value(data->step_sleep, value);

		get_val = gpio_get_value(data->step_sleep);
		pr_info("[MOTOR] step_sleep   : %s\n", get_val ? "HIGH" : "LOW");
	}

	return size;
}
static DEVICE_ATTR(test, 0660, test_show, test_store);

static ssize_t retry_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
/*
	struct stspin220_data *data = dev_get_drvdata(dev);
	struct hrtimer *timer = &data->motor_timer;
	int remaining = 0;

	if (hrtimer_active(timer)) {
		ktime_t remain = hrtimer_get_remaining(timer);
		struct timeval t = ktime_to_timeval(remain);

		remaining = t.tv_sec * 1000 + t.tv_usec / 1000;
	}

	return sprintf(buf, "%d\n", remaining);
*/
	return 0;
}
static ssize_t retry_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct stspin220_data *data = dev_get_drvdata(dev);
	int time = 0, direction = 0;
	int ret;

	pr_info("[MOTOR] retry function called\n");

	ret = sscanf(buf, "%d %d", &time, &direction);
	if (ret == 0)
		return -EINVAL;

	data->is_retry = 3;
	
	motor_enable(data, time, -direction);

	return size;
}

static DEVICE_ATTR(retry, 0660, retry_show, retry_store);


static ssize_t enable_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
/*
	struct stspin220_data *data = dev_get_drvdata(dev);
	struct hrtimer *timer = &data->motor_timer;
	int remaining = 0;

	if (hrtimer_active(timer)) {
		ktime_t remain = hrtimer_get_remaining(timer);
		struct timeval t = ktime_to_timeval(remain);

		remaining = t.tv_sec * 1000 + t.tv_usec / 1000;
	}

	return sprintf(buf, "%d\n", remaining);
*/
	return 0;
}
static ssize_t enable_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{

	struct stspin220_data *data = dev_get_drvdata(dev);
	int value = 0, direction = 0;
	int ret;

	ret = sscanf(buf, "%d %d", &value, &direction);
	if (ret == 0)
		return -EINVAL;

	data->is_retry = false;
	motor_check_temp(data);
	motor_enable(data, value, direction);

	return size;
}

static DEVICE_ATTR(enable, 0660, enable_show, enable_store);

static void stspin220_update_pwm(struct work_struct *work)
{
	struct stspin220_data *data = container_of(work, struct stspin220_data,
					 work_change_speed);

	stspin220_set_stck(data, data->period);
}

static void stspin220_set_motor_off(struct work_struct *work)
{
	struct stspin220_data *data = container_of(work, struct stspin220_data,
					 work_motor_off);

	pr_info("[MOTOR] %s\n",__func__);

	set_motor(data, OFF);
}

static void stspin220_set_motor_retry_on(struct work_struct *work)
{
	struct stspin220_data *data = container_of(work, struct stspin220_data,
					 work_motor_retry_on);

	pr_info("[MOTOR] %s\n",__func__);

	data->is_retry--;
	
	motor_enable(data, data->timeout_speed_change_time, -(data->direction));
}

static enum hrtimer_restart full_speed_timer_func(struct hrtimer *timer)
{
	struct stspin220_data *data = container_of(timer, struct stspin220_data,
							 full_speed_timer);

	pr_info("[MOTOR] %s\n",__func__);

	data->period = data->full_speed;
//	data->period = full_speed_period;
	queue_work(data->queue, &data->work_change_speed);

	return HRTIMER_NORESTART;
}

static enum hrtimer_restart ending_timer_func(struct hrtimer *timer)
{
	struct stspin220_data *data = container_of(timer, struct stspin220_data,
							 ending_timer);

	pr_info("[MOTOR] %s\n",__func__);

	data->period = data->ending_speed;
//	data->period = ending_period;
	queue_work(data->queue, &data->work_change_speed);

	return HRTIMER_NORESTART;
}

static enum hrtimer_restart off_timer_func(struct hrtimer *timer)
{
	struct stspin220_data *data = container_of(timer, struct stspin220_data,
							 off_timer);

	pr_info("[MOTOR] %s\n",__func__);

	queue_work(data->queue, &data->work_motor_off);

	return HRTIMER_NORESTART;
}

static enum hrtimer_restart retry_on_timer_func(struct hrtimer *timer)
{
	struct stspin220_data *data = container_of(timer, struct stspin220_data,
							 retry_on_timer);

	pr_info("[MOTOR] %s\n",__func__);

	queue_work(data->queue, &data->work_motor_retry_on);

	return HRTIMER_NORESTART;
}

#ifdef CONFIG_PM
static int stspin220_suspend(struct device *dev)
{
//	pr_info("[MOTOR] %s\n",__func__);
	return 0;
}
static int stspin220_resume(struct device *dev)
{
//	pr_info("[MOTOR] %s\n",__func__);
	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(stspin220_pm_ops, stspin220_suspend, stspin220_resume);

static int stspin220_gpio_init(struct stspin220_data *data)
{
	int val;
	pr_info("[MOTOR] %s\n",__func__);

	gpio_request(data->step_m0, "sliding_motor_m0");
	gpio_direction_output(data->step_m0, 0);
	gpio_set_value(data->step_m0, 0);
	val = gpio_get_value(data->step_m0);
	pr_info("[MOTOR] step_m0      : %s\n", val ? "HIGH" : "LOW");

	gpio_request(data->step_m1, "sliding_motor_m1");
	gpio_direction_output(data->step_m1, 0);
	gpio_set_value(data->step_m1, 0);
	val = gpio_get_value(data->step_m1);
	pr_info("[MOTOR] step_m1      : %s\n", val ? "HIGH" : "LOW");

	gpio_request(data->step_boost_en, "sliding_motor_boost_en");
	gpio_direction_output(data->step_boost_en, 0);
	gpio_set_value(data->step_boost_en, 0);
	val = gpio_get_value(data->step_boost_en);
	pr_info("[MOTOR] step_boost_en: %s\n", val ? "HIGH" : "LOW");

	gpio_request(data->step_sleep, "sliding_motor_sleep");
	gpio_direction_output(data->step_sleep, 0);
	gpio_set_value(data->step_sleep, 0);
	val = gpio_get_value(data->step_sleep);
	pr_info("[MOTOR] step_sleep   : %s\n", val ? "HIGH" : "LOW");

	gpio_request(data->step_enable, "sliding_motor_enable");
	gpio_direction_output(data->step_enable, 0);
	gpio_set_value(data->step_enable, 0);
	val = gpio_get_value(data->step_enable);
	pr_info("[MOTOR] step_enable  : %s\n", val ? "HIGH" : "LOW");

	gpio_request(data->step_dir, "sliding_motor_dir");
	gpio_direction_output(data->step_dir, 0);
	gpio_set_value(data->step_dir, 0);
	val = gpio_get_value(data->step_dir);
	pr_info("[MOTOR] step_dir     : %s\n", val ? "HIGH" : "LOW");

	gpio_request(data->step_step, "sliding_motor_step");
	gpio_direction_input(data->step_step);

//	stspin220_get_dump(data);

	return 0;
}

static int stspin220_parse_dt(struct stspin220_data *data)
{
	struct device_node *np = data->dev->of_node;
	int rc;
	
	pr_info("[MOTOR] %s\n",__func__);

	data->step_dir = of_get_named_gpio(np, "motor,step_dir", 0);
	if (!gpio_is_valid(data->step_dir))
		pr_err("%s:, step_dir gpio is not specified\n", __func__);

	data->step_m0 = of_get_named_gpio(np, "motor,step_m0", 0);
	if (!gpio_is_valid(data->step_m0))
		pr_err("%s:, step_m0 gpio is not specified\n", __func__);

	data->step_m1 = of_get_named_gpio(np, "motor,step_m1", 0);
	if (!gpio_is_valid(data->step_m1))
		pr_err("%s:, step_m1 gpio is not specified\n", __func__);

	data->step_enable = of_get_named_gpio(np, "motor,step_enable", 0);
	if (!gpio_is_valid(data->step_enable))
		pr_err("%s:, step_enable gpio is not specified\n", __func__);

	data->step_step = of_get_named_gpio(np, "motor,step_step", 0);
	if (!gpio_is_valid(data->step_step))
		pr_err("%s:, step_step gpio is not specified\n", __func__);

	data->step_sleep = of_get_named_gpio(np, "motor,step_sleep", 0);
	if (!gpio_is_valid(data->step_sleep))
		pr_err("%s:, step_sleep gpio is not specified\n", __func__);

	data->step_boost_en = of_get_named_gpio(np, "motor,boost_en", 0);
	if (!gpio_is_valid(data->step_boost_en))
		pr_err("%s:, step_boost_en gpio is not specified\n", __func__);

	data->pwm_dev =	devm_of_pwm_get(data->dev, np, NULL);
	if (IS_ERR(data->pwm_dev)) {
		rc = PTR_ERR(data->pwm_dev);
		if (rc != -EPROBE_DEFER)
			dev_err(data->dev, "Get pwm device for sliding motor failed, rc=%d\n", rc);
		return rc;
	}

	return 0;
}

static int stspin220_probe(struct platform_device *pdev)
{
	struct stspin220_data *pdata;
	int rc;

	pr_info("[MOTOR] %s\n", __func__);

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)	{
		pr_err("%s : Failed to allocate memory\n", __func__);
		return -ENOMEM;
	}

        pdata->dev = &pdev->dev;

	rc = stspin220_parse_dt(pdata);
	if (rc)
		return rc;

	rc = stspin220_gpio_init(pdata);
	if (rc)
		return rc;

	stspin220_set_step_mode(pdata, ONE_THIRTYTWO_STEP);

	INIT_WORK(&pdata->work_motor_off, stspin220_set_motor_off);
	INIT_WORK(&pdata->work_change_speed, stspin220_update_pwm);
	INIT_WORK(&pdata->work_motor_retry_on, stspin220_set_motor_retry_on);

	mutex_init(&pdata->lock);
	wake_lock_init(&pdata->motor_wake_lock, WAKE_LOCK_SUSPEND, "CAMERA SLIDING MOTOR WAKELOCK");

	pdata->queue = create_singlethread_workqueue("stspin220");
	if (!pdata->queue) {
		pr_err("%s(): can't create workqueue\n", __func__);
		return -ENOMEM;
	}

	hrtimer_init(&pdata->full_speed_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	pdata->full_speed_timer.function = full_speed_timer_func;

	hrtimer_init(&pdata->ending_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	pdata->ending_timer.function = ending_timer_func;

	hrtimer_init(&pdata->off_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	pdata->off_timer.function = off_timer_func;

	hrtimer_init(&pdata->retry_on_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	pdata->retry_on_timer.function = retry_on_timer_func;

	pdata->dev = sec_device_create(0, pdata, "sliding_motor");
	if (pdata->dev == NULL) {
		dev_err(&pdev->dev, "Failed to create device for motor\n");
		return -ENOMEM;
	}

	rc = sysfs_create_file(&pdata->dev->kobj, &dev_attr_enable.attr);
	if (rc < 0)
		pr_err("[MOTOR]: Failed to register sysfs enable: %d\n", rc);

	rc = sysfs_create_file(&pdata->dev->kobj, &dev_attr_test.attr);
	if (rc < 0)
		pr_err("[MOTOR]: Failed to register sysfs enable: %d\n", rc);

	rc = sysfs_create_file(&pdata->dev->kobj, &dev_attr_pwm_active.attr);
	if (rc < 0)
		pr_err("[MOTOR]: Failed to register sysfs enable: %d\n", rc);

	rc = sysfs_create_file(&pdata->dev->kobj, &dev_attr_retry.attr);
	if (rc < 0)
		pr_err("[MOTOR]: Failed to register sysfs enable: %d\n", rc);

	dev_set_drvdata(&pdev->dev, pdata);

	pdata->pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(pdata->pinctrl)) {
		pr_err("[MOTOR]: Failed to get pinctrl(%d)\n", IS_ERR(pdata->pinctrl));
	} else {
		pdata->pin_active = pinctrl_lookup_state(pdata->pinctrl, "pm_gpio6_pwm_active");
		if (IS_ERR(pdata->pin_active)) {
			pr_err("[MOTOR]: Failed to get pin_active(%d)\n", IS_ERR(pdata->pin_active));
		}
		rc = pinctrl_select_state(pdata->pinctrl, pdata->pin_active);
		if (rc)
			pr_err("[MOTOR]%s: can not change pinctrl, rc=%d\n", __func__, rc);
	}

	stspin220_set_stck(pdata, OFF);

	return 0;
}

static int stspin220_remove(struct platform_device *pdev)
{
	struct stspin220_data *pdata = dev_get_drvdata(&pdev->dev);

	destroy_workqueue(pdata->queue);
	mutex_destroy(&pdata->lock);
	wake_lock_destroy(&pdata->motor_wake_lock);

	return 0;
}

static const struct of_device_id stspin220_motor_match[] = {
	{	.compatible = "st,stspin220", },
	{}
};

static struct platform_driver stspin220_platdrv =
{
	.driver = {
		.name = "st,stspin220",
		.owner = THIS_MODULE,
		.of_match_table = stspin220_motor_match,
		.pm	= &stspin220_pm_ops,
	},
	.probe = stspin220_probe,
	.remove = stspin220_remove,
};

static int __init stspin220_init(void)
{
	return platform_driver_register(&stspin220_platdrv);
}

void __exit stspin220_exit(void)
{
	platform_driver_unregister(&stspin220_platdrv);
}
module_init(stspin220_init);
module_exit(stspin220_exit);

MODULE_DESCRIPTION("stspin220 step motor device");
