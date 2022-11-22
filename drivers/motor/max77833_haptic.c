/*
 * haptic motor driver for max77833 - max77673_haptic.c
 *
 * Copyright (C) 2011 ByungChang Cha <bc.cha@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/timed_output.h>
#include <linux/hrtimer.h>
#include <linux/pwm.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/i2c.h>
#include <linux/regulator/consumer.h>
#include <linux/mfd/max77833.h>
#include <linux/mfd/max77833-private.h>
#include <plat/devs.h>
#include <linux/sec_sysfs.h>
#include <linux/power_supply.h>
#include <linux/kthread.h>

#define TEST_MODE_TIME	10000
#define MAX_INTENSITY	10000

#define MOTOR_LRA			(1<<6)
#define MOTOR_EN_PWM	0x23

#define EXT_PWM				(0<<5)
#define DIVIDER_128			(1<<1)
#define MAX77833_REG_MAINCTRL1_MRDBTMER_MASK	(0x7)
#define MAX77833_REG_MAINCTRL1_MREN		(1 << 3)
#define MAX77833_REG_MAINCTRL1_BIASEN		(1 << 7)

static struct device *motor_dev;

struct max77833_haptic_data {
	struct max77833_dev *max77833;
	struct i2c_client *i2c;
	struct max77833_haptic_platform_data *pdata;

	struct pwm_device *pwm;
	struct timed_output_dev tout_dev;
	struct hrtimer timer;
	struct kthread_worker kworker;
	struct kthread_work kwork;
	spinlock_t lock;
	bool running;
	u32 intensity;
	u32 timeout;
	int duty;
};

static struct max77833_haptic_data *g_hap_data;
static int prev_duty;
static int prev_temperature_level;

static int temperature_check(void)
{
	struct power_supply *psy;
	union power_supply_propval temp;
	int value;
	int ret;

	psy = power_supply_get_by_name("ac");
	if (!psy)
		return -1;
	ret = psy->get_property(psy, POWER_SUPPLY_PROP_TEMP, &temp);
	if (ret)
		return -2;
	value = temp.intval / 10;

	printk("[VIB] %s : %d degree!\n",__func__, value);

	if (value >= 50) return 2;	/* high_temperature */
	else if (value <= 0) return 0;	/* low_temperature */
	else if (value > 0 && value < 50) return 1;	/* room_temperature */

	return value;
}

static void max77833_haptic_i2c(struct max77833_haptic_data *hap_data, bool en)
{
	int ret;

	pr_info("[VIB] %s %d\n", __func__, en);

	if (en) {
		ret = max77833_update_reg(hap_data->i2c,
				MAX77833_LRA_ENABLE_2, 0xdf, MOTOR_EN_PWM);
	} else {
		ret = max77833_update_reg(hap_data->i2c,
				MAX77833_LRA_ENABLE_2, 0x20, MOTOR_EN_PWM);
	}
	if (ret)
		pr_err("[VIB] i2c MOTOR_EN_PWM update error %d\n", ret);
}

static int haptic_get_time(struct timed_output_dev *tout_dev)
{
	struct max77833_haptic_data *hap_data
		= container_of(tout_dev, struct max77833_haptic_data, tout_dev);

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
	struct max77833_haptic_data *hap_data
		= container_of(tout_dev, struct max77833_haptic_data, tout_dev);

	struct hrtimer *timer = &hap_data->timer;
	unsigned long flags;
	int error = 0, temperature_level;

	flush_kthread_worker(&hap_data->kworker);
	hrtimer_cancel(timer);
	hap_data->timeout = value;

	if (value > 0) {
		if (!hap_data->running) {
			pwm_config(hap_data->pwm, hap_data->duty,
					hap_data->pdata->period);
			pwm_enable(hap_data->pwm);		

			temperature_level = temperature_check();
			if (temperature_level != prev_temperature_level) {
				switch(temperature_level)
				{
					case 0:
						error = max77833_write_reg(hap_data->i2c,
							MAX77833_AUTORES_MIN_FREQ_LOW, hap_data->pdata->auto_res_min_low_low_temp);
						if (error < 0) {
							pr_err("[VIB] %s Failed to write REG(0x%02x) [%d]\n",
							__func__, MAX77833_AUTORES_MIN_FREQ_LOW, error);
						}
						error = max77833_write_reg(hap_data->i2c,
							MAX77833_AUTORES_MAX_FREQ_LOW, hap_data->pdata->auto_res_max_low_low_temp);
						if (error < 0) {
							pr_err("[VIB] %s Failed to write REG(0x%02x) [%d]\n",
							__func__, MAX77833_AUTORES_MAX_FREQ_LOW, error);
						}
						error = max77833_write_reg(hap_data->i2c,
							MAX77833_AUTORES_INIT_GUESS_LOW, hap_data->pdata->auto_res_init_low_low_temp);
						if (error < 0) {
							pr_err("[VIB] %s Failed to write REG(0x%02x) [%d]\n",
							__func__, MAX77833_AUTORES_INIT_GUESS_LOW, error);
						}
						break;
					case 1:
						error = max77833_write_reg(hap_data->i2c,
							MAX77833_AUTORES_MIN_FREQ_LOW, hap_data->pdata->auto_res_min_low);
						if (error < 0) {
							pr_err("[VIB] %s Failed to write REG(0x%02x) [%d]\n",
							__func__, MAX77833_AUTORES_MIN_FREQ_LOW, error);
						}
						error = max77833_write_reg(hap_data->i2c,
							MAX77833_AUTORES_MAX_FREQ_LOW, hap_data->pdata->auto_res_max_low);
						if (error < 0) {
							pr_err("[VIB] %s Failed to write REG(0x%02x) [%d]\n",
							__func__, MAX77833_AUTORES_MAX_FREQ_LOW, error);
						}
						error = max77833_write_reg(hap_data->i2c,
							MAX77833_AUTORES_INIT_GUESS_LOW, hap_data->pdata->auto_res_init_low);
						if (error < 0) {
							pr_err("[VIB] %s Failed to write REG(0x%02x) [%d]\n",
							__func__, MAX77833_AUTORES_INIT_GUESS_LOW, error);
						}
						break;
					case 2:
						error = max77833_write_reg(hap_data->i2c,
							MAX77833_AUTORES_MIN_FREQ_LOW, hap_data->pdata->auto_res_min_low_high_temp);
						if (error < 0) {
							pr_err("[VIB] %s Failed to write REG(0x%02x) [%d]\n",
							__func__, MAX77833_AUTORES_MIN_FREQ_LOW, error);
						}
						error = max77833_write_reg(hap_data->i2c,
							MAX77833_AUTORES_MAX_FREQ_LOW, hap_data->pdata->auto_res_max_low_high_temp);
						if (error < 0) {
							pr_err("[VIB] %s Failed to write REG(0x%02x) [%d]\n",
							__func__, MAX77833_AUTORES_MAX_FREQ_LOW, error);
						}
						error = max77833_write_reg(hap_data->i2c,
							MAX77833_AUTORES_INIT_GUESS_LOW, hap_data->pdata->auto_res_init_low_high_temp);
						if (error < 0) {
							pr_err("[VIB] %s Failed to write REG(0x%02x) [%d]\n",
							__func__, MAX77833_AUTORES_INIT_GUESS_LOW, error);
						}
						break;
					default:
						pr_err("[VIB] %s Failed to read temperature [%d]\n",
						__func__, temperature_level);
						break;
				}
			}
			prev_temperature_level = temperature_level;

			max77833_haptic_i2c(hap_data, true);
			hap_data->running = true;
		}

		spin_lock_irqsave(&hap_data->lock, flags);
		pr_debug("[VIB] %s value %d\n", __func__, value);
		value = min(value, (int)hap_data->pdata->max_timeout);
		hrtimer_start(timer, ns_to_ktime((u64)value * NSEC_PER_MSEC),
				HRTIMER_MODE_REL);
		spin_unlock_irqrestore(&hap_data->lock, flags);
	}
	else
		queue_kthread_work(&hap_data->kworker, &hap_data->kwork);
}

static enum hrtimer_restart haptic_timer_func(struct hrtimer *timer)
{
	struct max77833_haptic_data *hap_data
		= container_of(timer, struct max77833_haptic_data, timer);

	hap_data->timeout = 0;
	queue_kthread_work(&hap_data->kworker, &hap_data->kwork);
	return HRTIMER_NORESTART;
}

static int vibetonz_clk_on(struct device *dev, bool en)
{
	struct clk *vibetonz_clk = NULL;

#if defined(CONFIG_OF)
	struct device_node *np;

	np = of_find_node_by_name(NULL,"pwm");
	if (np == NULL) {
		pr_err("[VIB] %s : pwm error to get dt node\n", __func__);
		return -EINVAL;
	}

	vibetonz_clk = of_clk_get_by_name(np, "gate_timers");
	if (!vibetonz_clk) {
		pr_info("[VIB] %s fail to get the vibetonz_clk\n", __func__);
		return -EINVAL;
	}
#else
	vibetonz_clk = clk_get(dev, "timers");
#endif
	pr_info("[VIB] DEV NAME %s %lu\n",
			dev_name(dev), clk_get_rate(vibetonz_clk));

	if (IS_ERR(vibetonz_clk)) {
		pr_err("[VIB] failed to get clock for the motor\n");
		goto err_clk_get;
	}

	if (en)
		clk_enable(vibetonz_clk);
	else
		clk_disable(vibetonz_clk);

	clk_put(vibetonz_clk);
	return 0;

err_clk_get:
	clk_put(vibetonz_clk);
	return -EINVAL;
}

static void haptic_work(struct kthread_work *work)
{
	struct max77833_haptic_data *hap_data
		= container_of(work, struct max77833_haptic_data, kwork);

	pr_info("[VIB] %s\n", __func__);

	if (hap_data->running) {
		pwm_disable(hap_data->pwm);
		max77833_haptic_i2c(hap_data, false);
		hap_data->running = false;
	}
}

#if defined(CONFIG_OF)
static struct max77833_haptic_platform_data *of_max77833_haptic_dt(struct device *dev)
{
	struct device_node *np_root = dev->parent->of_node;
	struct device_node *np_haptic;
	struct max77833_haptic_platform_data *pdata;
	u32 temp;
	int ret;

	pdata = kzalloc(sizeof(struct max77833_haptic_platform_data), GFP_KERNEL);
	if (!pdata) {
		pr_err("%s: failed to allocate driver data\n", __func__);
		return NULL;
	}

	printk("%s : start dt parsing\n", __func__);

	np_haptic = of_find_node_by_name(np_root, "haptic");
	if (np_haptic == NULL) {
		pr_err("[VIB] %s : error to get dt node\n", __func__);
		goto err_parsing_dt;
	}

	ret = of_property_read_u32(np_haptic, "haptic,max_timeout", &temp);
	if (IS_ERR_VALUE(ret)) {
		pr_err("[VIB] %s : error to get dt node max_timeout\n", __func__);
		goto err_parsing_dt;
	} else
		pdata->max_timeout = (u16)temp;

	ret = of_property_read_u32(np_haptic, "haptic,duty", &temp);
	if (IS_ERR_VALUE(ret)) {
		pr_err("[VIB] %s : error to get dt node duty\n", __func__);
		goto err_parsing_dt;
	} else
		pdata->duty = (u16)temp;

	ret = of_property_read_u32(np_haptic, "haptic,period", &temp);
	if (IS_ERR_VALUE(ret)) {
		pr_err("[VIB] %s : error to get dt node period\n", __func__);
		goto err_parsing_dt;
	} else
		pdata->period = (u16)temp;

	ret = of_property_read_u32(np_haptic, "haptic,pwm_id", &temp);
	if (IS_ERR_VALUE(ret)) {
		pr_err("[VIB] %s : error to get dt node pwm_id\n", __func__);
		goto err_parsing_dt;
	} else
		pdata->pwm_id = (u16)temp;

	/* auto resonance */
	ret = of_property_read_u32(np_haptic, "haptic,auto_res_min_low", &temp);
	if (IS_ERR_VALUE(ret)) {
		pr_err("[VIB] %s : error to get dt node auto_res_min_low\n", __func__);
		goto err_parsing_dt;
	} else
		pdata->auto_res_min_low = (u8)temp;

	ret = of_property_read_u32(np_haptic, "haptic,auto_res_min_low_high_temp", &temp);
        if (IS_ERR_VALUE(ret)) {
                pr_err("[VIB] %s : error to get dt node auto_res_min_low_high_temp\n", __func__);
                goto err_parsing_dt;
        } else
                pdata->auto_res_min_low_high_temp = (u8)temp;

        ret = of_property_read_u32(np_haptic, "haptic,auto_res_min_low_low_temp", &temp);
        if (IS_ERR_VALUE(ret)) {
                pr_err("[VIB] %s : error to get dt node auto_res_min_low_low_temp\n", __func__);
                goto err_parsing_dt;
        } else
                pdata->auto_res_min_low_low_temp = (u8)temp;

	ret = of_property_read_u32(np_haptic, "haptic,auto_res_max_low", &temp);
	if (IS_ERR_VALUE(ret)) {
		pr_err("[VIB] %s : error to get dt node auto_res_max_low\n", __func__);
		goto err_parsing_dt;
	} else
		pdata->auto_res_max_low = (u8)temp;

	ret = of_property_read_u32(np_haptic, "haptic,auto_res_max_low_high_temp", &temp);
        if (IS_ERR_VALUE(ret)) {
                pr_err("[VIB] %s : error to get dt node auto_res_max_low_high_temp\n", __func__);
                goto err_parsing_dt;
        } else
                pdata->auto_res_max_low_high_temp = (u8)temp;

        ret = of_property_read_u32(np_haptic, "haptic,auto_res_max_low_low_temp", &temp);
        if (IS_ERR_VALUE(ret)) {
                pr_err("[VIB] %s : error to get dt node auto_res_max_low_low_temp\n", __func__);
                goto err_parsing_dt;
        } else
                pdata->auto_res_max_low_low_temp = (u8)temp;

	ret = of_property_read_u32(np_haptic, "haptic,auto_res_init_low", &temp);
	if (IS_ERR_VALUE(ret)) {
		pr_err("[VIB] %s : error to get dt node auto_res_init_low\n", __func__);
		goto err_parsing_dt;
	} else
		pdata->auto_res_init_low = (u8)temp;

	ret = of_property_read_u32(np_haptic, "haptic,auto_res_init_low_high_temp", &temp);
        if (IS_ERR_VALUE(ret)) {
                pr_err("[VIB] %s : error to get dt node auto_res_init_low_high_temp\n", __func__);
                goto err_parsing_dt;
        } else
                pdata->auto_res_init_low_high_temp = (u8)temp;

	ret = of_property_read_u32(np_haptic, "haptic,auto_res_init_low_low_temp", &temp);
        if (IS_ERR_VALUE(ret)) {
                pr_err("[VIB] %s : error to get dt node auto_res_init_low_low_temp\n", __func__);
                goto err_parsing_dt;
        } else
                pdata->auto_res_init_low_low_temp = (u8)temp;

	ret = of_property_read_u32(np_haptic, "haptic,auto_res_min_high", &temp);
	if (IS_ERR_VALUE(ret)) {
		pr_err("[VIB] %s : error to get dt node auto_res_min_high\n", __func__);
		goto err_parsing_dt;
	} else
		pdata->auto_res_min_high = (u8)temp;

	ret = of_property_read_u32(np_haptic, "haptic,auto_res_max_high", &temp);
	if (IS_ERR_VALUE(ret)) {
		pr_err("[VIB] %s : error to get dt node auto_res_max_high\n", __func__);
		goto err_parsing_dt;
	} else
		pdata->auto_res_max_high = (u8)temp;

	ret = of_property_read_u32(np_haptic, "haptic,auto_res_init_high", &temp);
	if (IS_ERR_VALUE(ret)) {
		pr_err("[VIB] %s : error to get dt node auto_res_init_high\n", __func__);
		goto err_parsing_dt;
	} else
		pdata->auto_res_init_high = (u8)temp;

	ret = of_property_read_u32(np_haptic, "haptic,auto_res_lock_window", &temp);
	if (IS_ERR_VALUE(ret)) {
		pr_err("[VIB] %s : error to get dt node auto_res_lock_window\n", __func__);
		goto err_parsing_dt;
	} else
		pdata->auto_res_lock_window = (u8)temp;

	ret = of_property_read_u32(np_haptic, "haptic,auto_res_update_freq", &temp);
	if (IS_ERR_VALUE(ret)) {
		pr_err("[VIB] %s : error to get dt node auto_res_update_freq\n", __func__);
		goto err_parsing_dt;
	} else
		pdata->auto_res_update_freq = (u8)temp;

	ret = of_property_read_u32(np_haptic, "haptic,auto_res_enable", &temp);
	if (IS_ERR_VALUE(ret)) {
		pr_err("[VIB] %s : error to get dt node auto_res_enable\n", __func__);
		goto err_parsing_dt;
	} else
		pdata->auto_res_enable = (u8)temp;

	ret = of_property_read_u32(np_haptic, "haptic,nominal_strength", &temp);
	if (IS_ERR_VALUE(ret)) {
		pr_err("[VIB] %s : error to get dt node nominal_strength\n", __func__);
		goto err_parsing_dt;
	} else
		pdata->nominal_strength = (u8)temp;

	/* debugging */
	printk("[VIB] %s : max_timeout = %d\n", __func__, pdata->max_timeout);
	printk("[VIB] %s : duty = %d\n", __func__, pdata->duty);
	printk("[VIB] %s : period = %d\n", __func__, pdata->period);
	printk("[VIB] %s : pwm_id = %d\n", __func__, pdata->pwm_id);
	printk("[VIB] %s : auto_res_min_low = 0x%x\n", __func__, pdata->auto_res_min_low);
	printk("[VIB] %s : auto_res_max_low = 0x%x\n", __func__, pdata->auto_res_max_low);
	printk("[VIB] %s : auto_res_init_low = 0x%xd\n", __func__, pdata->auto_res_init_low);
	printk("[VIB] %s : auto_res_min_high = 0x%x\n", __func__, pdata->auto_res_min_high);
	printk("[VIB] %s : auto_res_max_high = 0x%x\n", __func__, pdata->auto_res_max_high);
	printk("[VIB] %s : auto_res_init_high = 0x%x\n", __func__, pdata->auto_res_init_high);
	printk("[VIB] %s : auto_res_lock_window = 0x%x\n", __func__, pdata->auto_res_lock_window);
	printk("[VIB] %s : auto_res_update_freq = 0x%x\n", __func__, pdata->auto_res_update_freq);
	printk("[VIB] %s : auto_res_enable = 0x%x\n", __func__, pdata->auto_res_enable);
	printk("[VIB] %s : nominal_strength = 0x%x\n", __func__, pdata->nominal_strength);

	pdata->init_hw = NULL;
	pdata->motor_en = NULL;

	return pdata;

err_parsing_dt:
	kfree(pdata);
	return NULL;
}
#endif
static ssize_t store_duty(struct device *dev,
	struct device_attribute *devattr, const char *buf, size_t count)
{
	char buff[10] = {0,};
	int cnt, ret;
	u16 duty;

	cnt = count;
	cnt = (buf[cnt-1] == '\n') ? cnt-1 : cnt;
	memcpy(buff, buf, cnt);
	buff[cnt] = '\0';

	ret = kstrtou16(buff, 0, &duty);
	if (ret != 0) {
		dev_err(dev, "[VIB] fail to get duty.\n");
		return count;
	}
	g_hap_data->pdata->duty = (u16)duty;

	return count;
}

static ssize_t store_period(struct device *dev,
	struct device_attribute *devattr, const char *buf, size_t count)
{
	char buff[10] = {0,};
	int cnt, ret;
	u16 period;

	cnt = count;
	cnt = (buf[cnt-1] == '\n') ? cnt-1 : cnt;
	memcpy(buff, buf, cnt);
	buff[cnt] = '\0';

	ret = kstrtou16(buff, 0, &period);
	if (ret != 0) {
		dev_err(dev, "[VIB] fail to get period.\n");
		return count;
	}
	g_hap_data->pdata->period = (u16)period;

	return count;
}

static ssize_t show_duty_period(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "duty: %u, period%u\n", g_hap_data->pdata->duty,
						g_hap_data->pdata->period);
}

/* below nodes is SAMSUNG specific nodes */
static DEVICE_ATTR(set_duty, 0220, NULL, store_duty);
static DEVICE_ATTR(set_period, 0220, NULL, store_period);
static DEVICE_ATTR(show_duty_period, 0440, show_duty_period, NULL);

static struct attribute *sec_motor_attributes[] = {
	&dev_attr_set_duty.attr,
	&dev_attr_set_period.attr,
	&dev_attr_show_duty_period.attr,
	NULL,
};

static struct attribute_group sec_motor_attr_group = {
	.attrs = sec_motor_attributes,
};

static ssize_t intensity_store(struct device *dev,
	struct device_attribute *devattr, const char *buf, size_t count)
{
	struct timed_output_dev *tdev = dev_get_drvdata(dev);
	struct max77833_haptic_data *drvdata
		= container_of(tdev, struct max77833_haptic_data, tout_dev);
	int duty = drvdata->pdata->period >> 1;
	int intensity = 0, ret = 0;

	ret = kstrtoint(buf, 0, &intensity);

	if (intensity < 0 || MAX_INTENSITY < intensity) {
		pr_err("out of rage\n");
		return -EINVAL;
	}

	if (MAX_INTENSITY == intensity)
		duty = drvdata->pdata->duty;
	else if (0 != intensity) {
		long long tmp = drvdata->pdata->duty >> 1;

		tmp *= (intensity / 100);
		duty += (int)(tmp / 100);
	}

	drvdata->intensity = intensity;
	drvdata->duty = duty;

	pwm_config(drvdata->pwm, duty, drvdata->pdata->period);

	return count;
}

static ssize_t intensity_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct timed_output_dev *tdev = dev_get_drvdata(dev);
	struct max77833_haptic_data *drvdata
		= container_of(tdev, struct max77833_haptic_data, tout_dev);

	return sprintf(buf, "intensity: %u\n",
			(drvdata->intensity * 100));
}

static DEVICE_ATTR(intensity, 0660, intensity_show, intensity_store);

static int max77833_haptic_probe(struct platform_device *pdev)
{
	int error = 0;
	struct max77833_dev *max77833 = dev_get_drvdata(pdev->dev.parent);
	struct max77833_platform_data *max77833_pdata
		= dev_get_platdata(max77833->dev);
	struct max77833_haptic_platform_data *pdata
		= max77833_pdata->haptic_data;
	struct max77833_haptic_data *hap_data;
	struct task_struct *kworker_task;

	pr_info("[VIB] ++ %s\n", __func__);

#if defined(CONFIG_OF)
	if (pdata == NULL) {
		pdata = of_max77833_haptic_dt(&pdev->dev);
		if (!pdata) {
			pr_err("[VIB] max77833-haptic : %s not found haptic dt!\n",
					__func__);
			return -1;
		}
	}
#else
	if (pdata == NULL) {
		pr_err("[VIB] %s: no pdata\n", __func__);
		return -ENODEV;
	}
#endif /* CONFIG_OF */

	hap_data = kzalloc(sizeof(struct max77833_haptic_data), GFP_KERNEL);
	if (!hap_data) {
		pr_err("[VIB] %s: no hap_pdata\n", __func__);
		kfree(pdata);
		return -ENOMEM;
	}
	platform_set_drvdata(pdev, hap_data);
	g_hap_data = hap_data;
	hap_data->max77833 = max77833;
	hap_data->i2c = max77833->i2c;
	hap_data->pdata = pdata;

	init_kthread_worker(&hap_data->kworker);
	kworker_task = kthread_run(kthread_worker_fn,
		   &hap_data->kworker, "max77843_haptic");
	if (IS_ERR(kworker_task)) {
		pr_err("Failed to create message pump task\n");
		error = -ENOMEM;
		goto err_kthread;
	}
	init_kthread_work(&hap_data->kwork, haptic_work);

	spin_lock_init(&(hap_data->lock));

	hap_data->pwm = pwm_request(hap_data->pdata->pwm_id, "vibrator");
	if (IS_ERR(hap_data->pwm)) {
		error = -EFAULT;
		pr_err("[VIB] Failed to request pwm, err num: %d\n", error);
		goto err_pwm_request;
	}

	pwm_config(hap_data->pwm, pdata->period / 2, pdata->period);
	prev_duty = hap_data->pdata->period / 2;
	vibetonz_clk_on(&pdev->dev, true);

	/* hrtimer init */
	hrtimer_init(&hap_data->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hap_data->timer.function = haptic_timer_func;

	/* timed_output_dev init*/
	hap_data->tout_dev.name = "vibrator";
	hap_data->tout_dev.get_time = haptic_get_time;
	hap_data->tout_dev.enable = haptic_enable;

	motor_dev = sec_device_create(hap_data, "motor");
	if (IS_ERR(motor_dev)) {
		error = -ENODEV;
		pr_err("[VIB] Failed to create device\
				for samsung specific motor, err num: %d\n", error);
		goto exit_sec_devices;
	}
	error = sysfs_create_group(&motor_dev->kobj, &sec_motor_attr_group);
	if (error) {
		error = -ENODEV;
		pr_err("[VIB] Failed to create sysfs group\
				for samsung specific motor, err num: %d\n", error);
		goto exit_sysfs;
	}

#ifdef CONFIG_ANDROID_TIMED_OUTPUT
	error = timed_output_dev_register(&hap_data->tout_dev);
	if (error < 0) {
		error = -EFAULT;
		pr_err("[VIB] Failed to register timed_output : %d\n", error);
		goto err_timed_output_register;
	}
#endif

	error = sysfs_create_file(&hap_data->tout_dev.dev->kobj,
				&dev_attr_intensity.attr);
	if (error < 0) {
		pr_err("Failed to register sysfs : %d\n", error);
		goto err_timed_output_register;
	}

	prev_temperature_level = 1;

	/* autoresonance range setting */
	error = max77833_write_reg(hap_data->i2c,
		MAX77833_AUTORES_CONFIG, 0x00);
	if (error < 0) {
		pr_err("[VIB] %s Failed to write REG(0x%02x) [%d]\n",
				__func__, MAX77833_AUTORES_CONFIG, error);
	}

	error = max77833_write_reg(hap_data->i2c,
		MAX77833_AUTORES_MIN_FREQ_LOW, pdata->auto_res_min_low);
	if (error < 0) {
		pr_err("[VIB] %s Failed to write REG(0x%02x) [%d]\n",
				__func__, MAX77833_AUTORES_MIN_FREQ_LOW, error);
	}

	error = max77833_write_reg(hap_data->i2c,
		MAX77833_AUTORES_MAX_FREQ_LOW, pdata->auto_res_max_low);
	if (error < 0) {
		pr_err("[VIB] %s Failed to write REG(0x%02x) [%d]\n",
				__func__, MAX77833_AUTORES_MAX_FREQ_LOW, error);
	}

	error = max77833_write_reg(hap_data->i2c,
		MAX77833_AUTORES_INIT_GUESS_LOW, pdata->auto_res_init_low);
	if (error < 0) {
		pr_err("[VIB] %s Failed to write REG(0x%02x) [%d]\n",
				__func__, MAX77833_AUTORES_INIT_GUESS_LOW, error);
	}

	error = max77833_write_reg(hap_data->i2c,
		MAX77833_NOMINAL_STRENGTH, pdata->nominal_strength);
	if (error < 0) {
		pr_err("[VIB] %s Failed to write REG(0x%02x) [%d]\n",
				__func__, MAX77833_NOMINAL_STRENGTH, error);
	}

	error = max77833_write_reg(hap_data->i2c,
		MAX77833_RES_MIN_FREQ_HIGH, pdata->auto_res_min_high);
	if (error < 0) {
		pr_err("[VIB] %s Failed to write REG(0x%02x) [%d]\n",
				__func__, MAX77833_RES_MIN_FREQ_HIGH, error);
	}

	error = max77833_write_reg(hap_data->i2c,
		MAX77833_RES_MAX_FREQ_HIGH, pdata->auto_res_max_high);
	if (error < 0) {
		pr_err("[VIB] %s Failed to write REG(0x%02x) [%d]\n",
				__func__, MAX77833_RES_MAX_FREQ_HIGH, error);
	}

	error = max77833_write_reg(hap_data->i2c,
		MAX77833_AUTORES_INIT_GUESS_HIGH, pdata->auto_res_init_high);
	if (error < 0) {
		pr_err("[VIB] %s Failed to write REG(0x%02x) [%d]\n",
				__func__, MAX77833_AUTORES_INIT_GUESS_HIGH, error);
	}

	error = max77833_write_reg(hap_data->i2c,
		MAX77833_AUTORES_LOCK_WINDOW, pdata->auto_res_lock_window);
	if (error < 0) {
		pr_err("[VIB] %s Failed to write REG(0x%02x) [%d]\n",
				__func__, MAX77833_AUTORES_LOCK_WINDOW, error);
	}

	error = max77833_write_reg(hap_data->i2c,
		MAX77833_AUTORES_UPDATE_FREQ, pdata->auto_res_update_freq);
	if (error < 0) {
		pr_err("[VIB] %s Failed to write REG(0x%02x) [%d]\n",
				__func__, MAX77833_AUTORES_UPDATE_FREQ, error);
	}

	if (!pdata->auto_res_enable) {
		error = max77833_write_reg(hap_data->i2c, MAX77833_OPTION_REG1, 0x11);
		if (error < 0) {
			pr_err("[VIB] %s Failed to write REG(0x%02x) [%d]\n",
					__func__, MAX77833_OPTION_REG1, error);
		}
	}
	return error;

err_timed_output_register:
	sysfs_remove_group(&motor_dev->kobj, &sec_motor_attr_group);
exit_sysfs:
	sec_device_destroy(motor_dev->devt);
exit_sec_devices:
	pwm_free(hap_data->pwm);
err_pwm_request:
err_kthread:
	kfree(hap_data);
	kfree(pdata);
	g_hap_data = NULL;
	return error;
}

static int __devexit max77833_haptic_remove(struct platform_device *pdev)
{
	struct max77833_haptic_data *data = platform_get_drvdata(pdev);
#ifdef CONFIG_ANDROID_TIMED_OUTPUT
	timed_output_dev_unregister(&data->tout_dev);
#endif

	sysfs_remove_group(&motor_dev->kobj, &sec_motor_attr_group);
	sec_device_destroy(motor_dev->devt);
	pwm_free(data->pwm);
	max77833_haptic_i2c(data, false);
	kfree(data);
	g_hap_data = NULL;

	return 0;
}

static int max77833_haptic_suspend(struct platform_device *pdev,
		pm_message_t state)
{
	struct max77833_haptic_data *data = platform_get_drvdata(pdev);
	pr_info("[VIB] %s\n", __func__);
	flush_kthread_worker(&data->kworker);
	hrtimer_cancel(&g_hap_data->timer);
	max77833_haptic_i2c(data, false);
	return 0;
}
static int max77833_haptic_resume(struct platform_device *pdev)
{
	pr_info("[VIB] %s\n", __func__);
	return 0;
}

static struct platform_driver max77833_haptic_driver = {
	.probe		= max77833_haptic_probe,
	.remove		= max77833_haptic_remove,
	.suspend	= max77833_haptic_suspend,
	.resume		= max77833_haptic_resume,
	.driver = {
		.name	= "max77833-haptic",
		.owner	= THIS_MODULE,
	},
};

static int __init max77833_haptic_init(void)
{
	pr_debug("[VIB] %s\n", __func__);
	return platform_driver_register(&max77833_haptic_driver);
}
module_init(max77833_haptic_init);

static void __exit max77833_haptic_exit(void)
{
	platform_driver_unregister(&max77833_haptic_driver);
}
module_exit(max77833_haptic_exit);

MODULE_AUTHOR("ByungChang Cha <bc.cha@samsung.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("max77833 haptic driver");

