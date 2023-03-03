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
#include <linux/clk.h>
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
#include "../staging/android/timed_output.h"
#include <linux/sec_class.h>
#include "vibrator.h"

struct msm_vib {
	struct device *dev;
	struct hrtimer vib_timer;
	struct timed_output_dev timed_dev;
	struct work_struct work;
	struct workqueue_struct *queue;
	int state;
	int timeout;
	int motor_pwm;
	int motor_en;
	struct mutex lock;
	u32 vdd_type;
	int intensity;
	unsigned int m_default;
	unsigned int n_default;
	unsigned int motor_strength;
	struct regulator *vdd;
	struct wake_lock wklock;
	struct clk *motor_clk;
	bool motor_enable_state;
};
#define CLK_RATE 100000000

#ifdef CONFIG_DC_MOTOR_PMIC
static int vib_volt;
#endif

#ifndef CONFIG_DC_MOTOR_PMIC
static void vibe_write_reg(void __iomem *addr,
			unsigned int mask, unsigned int val)
{
	out_dword_masked_ns(addr, mask, val,
			ioread32(addr) & MASK_VALUE);
}

static void vibe_set_pwm_freq(int intensity)
{
	int32_t calc_d;

	vibe_write_reg(gp_addr[CFG_RCGR], 0x700, 0 << 8);
	vibe_write_reg(gp_addr[CFG_RCGR], 0x1f, 31 << 0);
	vibe_write_reg(gp_addr[CFG_RCGR], 0x3000, 2 << 12);
	vibe_write_reg(gp_addr[GP_M], 0xff, g_nlra_gp_clk_m << 0);

	if (intensity > 0) {
		calc_d = g_nlra_gp_clk_n - ((intensity * g_nlra_gp_clk_pwm_mul) >> 8);
		calc_d = calc_d * motor_strength / 100;
		if (calc_d < min_strength)
			calc_d = min_strength;
	} else {
		calc_d = ((intensity * g_nlra_gp_clk_pwm_mul) >> 8) + g_nlra_gp_clk_d;
		if (g_nlra_gp_clk_n - calc_d > g_nlra_gp_clk_n * motor_strength / 100)
			calc_d = g_nlra_gp_clk_n - g_nlra_gp_clk_n * motor_strength / 100;
	}

	vibe_write_reg(gp_addr[GP_D], 0xff,
			(~((int16_t)calc_d << 1)) << 0);
	vibe_write_reg(gp_addr[GP_N], 0xff, ~(g_nlra_gp_clk_n - g_nlra_gp_clk_m) << 0);

}

static void vibe_pwm_onoff(int enable)
{
	if (enable) {
		vibe_write_reg(gp_addr[CMD_RCGR], 0x1, 1 << 0);
		vibe_write_reg(gp_addr[CMD_RCGR], 0x2, 1 << 1);
		vibe_write_reg(gp_addr[CAMSS_CBCR], 0x1, 1 << 0);
	} else {
		vibe_write_reg(gp_addr[CMD_RCGR], 0x1, 0 << 0);
		vibe_write_reg(gp_addr[CMD_RCGR], 0x2, 0 << 1);
		vibe_write_reg(gp_addr[CAMSS_CBCR], 0x1, 0 << 0);
	}
}
#endif

static void vibe_set_intensity(int intensity)
{
#ifdef CONFIG_DC_MOTOR_PMIC
	if (intensity == NO_INTENSITY)
		vib_volt = NO_INTENSITY_VOLTAGE;
	else if (intensity < LOW_INTENSITY)
		vib_volt = LOW_INTENSITY_VOLTAGE;
	else if (intensity < MID_INTENSITY)
		vib_volt = MID_INTENSITY_VOLTAGE;
	else if (intensity < HIGH_INTENSITY)
		vib_volt = HIGH_INTENSITY_VOLTAGE;
	else {
		vib_volt = HIGH_INTENSITY_VOLTAGE;
		pr_err("[VIB] intensity out of range!\n");
	}
#else

	int temp;

	pr_info("[VIB] set vib intensity = %d\n",intensity);

	if (intensity) {
		if (intensity >= MAX_INTENSITY)
			intensity = 1;
		else {
			temp = MAX_INTENSITY - intensity;
			intensity = (temp / 79);
		}
		vibe_set_pwm_freq(intensity);
	}

	vibe_pwm_onoff(intensity);
#endif
}

#define DIVIDE_VOLT	1000
static void set_vibrator(struct msm_vib *vib, int enable)
{
	int ret;
	struct pinctrl *vib_pinctrl;

	if (vib->motor_pwm) {
		gpio_set_value(vib->motor_pwm, enable);
	}

	if (!vib->vdd_type)
		gpio_set_value(vib->motor_en, enable);
	else {
		if (enable) {
			if (!regulator_is_enabled(vib->vdd)) {
#ifdef CONFIG_DC_MOTOR_PMIC
				ret = regulator_set_voltage(vib->vdd, vib_volt, vib_volt);
				pr_info("[VIB] voltage: %dmV\n", vib_volt/DIVIDE_VOLT);
#endif
				ret = regulator_enable(vib->vdd);
				if (ret) {
					pr_err("[VIB] power on error!\n");
					return;
				}
			} else
				pr_debug("[VIB] already power on\n");
		} else {
			if (regulator_is_enabled(vib->vdd)) {
				ret = regulator_disable(vib->vdd);
				if (ret) {
					pr_err("[VIB] power off error\n");
					return;
				}
			} else
				pr_debug("[VIB] already power off\n");
		}
	}

	if (enable){
		wake_lock(&vib->wklock);
		if (vib->motor_en || vib->motor_pwm) {
			vib_pinctrl = devm_pinctrl_get_select(vib->dev, "tlmm_motor_active");
			if (IS_ERR(vib_pinctrl)) {
				if (PTR_ERR(vib_pinctrl) == -EPROBE_DEFER)
					pr_err("[VIB]: Error %d\n", -EPROBE_DEFER);
				pr_debug("Target does not use pinctrl\n");
				vib_pinctrl = NULL;
			}
			if (vib->motor_pwm) {
				if(vib->motor_enable_state == false){
					ret = clk_prepare_enable(vib->motor_clk);
					if (ret < 0) {
						pr_err("[VIB]: Error cannot enable motor_clk %d\n", ret);
						return;
					}
					vib->motor_enable_state = true;
				}
			}
		}
	}
	else{
		if (vib->motor_en || vib->motor_pwm) {
			vib_pinctrl = devm_pinctrl_get_select(vib->dev, "tlmm_motor_suspend");
			if (IS_ERR(vib_pinctrl)) {
				if (PTR_ERR(vib_pinctrl) == -EPROBE_DEFER)
					pr_err("[VIB]: Error %d\n", -EPROBE_DEFER);
				pr_debug("Target does not use pinctrl\n");
				vib_pinctrl = NULL;
			}
			if (vib->motor_pwm) {
				if(vib->motor_enable_state == true){
					vib->motor_enable_state = false;
					clk_disable_unprepare(vib->motor_clk);
				}
			}
		}
		wake_unlock(&vib->wklock);
	}
	pr_info("[VIB] is %s\n",
			enable ? "enabled": "disabled");

}

static void vibrator_enable(struct timed_output_dev *dev, int value)
{
	struct msm_vib *vib = container_of(dev, struct msm_vib,
					 timed_dev);

	mutex_lock(&vib->lock);
	hrtimer_cancel(&vib->vib_timer);

	if (!value)
		pr_info("[VIB] OFF\n");
	else {
		pr_info("[VIB] ON, Duration : %d msec\n" , value);
		if (value == 0x7fffffff)
			pr_info("[VIB] No Use Timer %d \n", value);
		else {
			value = (value > vib->timeout ?	vib->timeout : value);
			hrtimer_start(&vib->vib_timer,ktime_set(value / 1000, (value % 1000) * 1000000),HRTIMER_MODE_REL);
                }
	}
	vib->state = value;
	mutex_unlock(&vib->lock);
	queue_work(vib->queue, &vib->work);
}

static void msm_vibrator_update(struct work_struct *work)
{
	struct msm_vib *vib = container_of(work, struct msm_vib,
					 work);

	set_vibrator(vib, vib->state);
}

static int vibrator_get_time(struct timed_output_dev *dev)
{
	struct msm_vib *vib = container_of(dev, struct msm_vib, timed_dev);

	if (hrtimer_active(&vib->vib_timer)) {
		ktime_t r = hrtimer_get_remaining(&vib->vib_timer);
		return (int)ktime_to_us(r);
	}

	return 0;
}

static enum hrtimer_restart vibrator_timer_func(struct hrtimer *timer)
{
	struct msm_vib *vib = container_of(timer, struct msm_vib,
							 vib_timer);

	vib->state = 0;
	queue_work(vib->queue, &vib->work);

	return HRTIMER_NORESTART;
}

static void set_pwm_register(void)
{
	int offset[] = {0x0, 0x4, 0x8, 0xc, 0x10, 0x18};
	int i;

	for (i=0 ; i< ARRAY_SIZE(offset) ; i++)
		gp_addr[i] = (void __iomem *)(virt_mmss_gp1_base + offset[i]);

}

#ifdef CONFIG_PM
static int msm_vibrator_suspend(struct device *dev)
{
	struct msm_vib *vib = dev_get_drvdata(dev);

	pr_info("[VIB] %s\n",__func__);

	hrtimer_cancel(&vib->vib_timer);
	cancel_work_sync(&vib->work);
	set_vibrator(vib, 0);

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(vibrator_pm_ops, msm_vibrator_suspend, NULL);

static ssize_t intensity_store(struct device *dev,
		struct device_attribute *devattr, const char *buf, size_t count)
{
	struct timed_output_dev *t_dev = dev_get_drvdata(dev);
	struct msm_vib *vib = container_of(t_dev, struct msm_vib, timed_dev);
	int ret, set_intensity;

	ret = kstrtoint(buf, 0, &set_intensity);

	if ((set_intensity < 0) || (set_intensity > MAX_INTENSITY)) {
		pr_err("[VIB]: %sout of rage\n", __func__);
		return -EINVAL;
	}

	vibe_set_intensity(set_intensity);
	vib->intensity = set_intensity;

	return count;
}

static ssize_t intensity_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct timed_output_dev *t_dev = dev_get_drvdata(dev);
	struct msm_vib *vib = container_of(t_dev, struct msm_vib, timed_dev);

	return sprintf(buf, "intensity: %u\n", vib->intensity);
}

static DEVICE_ATTR(intensity, S_IWUSR | S_IRUGO | S_IWGRP, intensity_show, intensity_store);

#ifdef CONFIG_DC_MOTOR_PMIC
static ssize_t show_dc_pmic(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	sprintf(buf, "It is COIN DC motor with PMIC LDO\n");
	return strlen(buf);
}

static DEVICE_ATTR(dc_pmic, S_IWUSR | S_IRUGO | S_IWGRP, show_dc_pmic, NULL);
#endif

#ifdef CONFIG_OF
static int vib_parse_dt(struct device *dev, struct msm_vib *vib)
{
	int ret;
	unsigned int gp_clk;
	struct device_node *np = dev->of_node;

	ret = of_property_read_u32(np, "motor-vdd_type", &vib->vdd_type);
	if (!vib->vdd_type)
		vib->motor_en = of_get_named_gpio(np, "motor-en", 0);
	else {
		vib->vdd = regulator_get(dev, "vibr_vdd");
		if (IS_ERR(vib->vdd)) {
			pr_err("[VIB] failed to get regulator\n");
			return -EINVAL;
		}
	}

	vib->motor_pwm = of_get_named_gpio(np, "motor-pwm", 0);
	if (vib->motor_pwm <= 0)
		vib->motor_pwm = 0;
	else {
		ret = of_property_read_u32(np, "gp_clk", &gp_clk);
		if (ret) {
			pr_err("[VIB] failed to get gp_clk address\n");
			gp_clk = 0x01854000; /* default: gp0_clk */
		}
		virt_mmss_gp1_base = ioremap(gp_clk, 0x28);
		set_pwm_register();
	}

	if (!vib->vdd_type) {
		pr_info("[VIB] motor_en: %d motor_pwm: %d\n",
					vib->motor_en, vib->motor_pwm);
		if (!gpio_is_valid(vib->motor_en)) {
			pr_err("%s: failed to get en gpio\n", __func__);
			return -EINVAL;
		}
		if (gpio_request(vib->motor_en, "motor_en")) {
			pr_err("%s: motor_en request failed\n", __func__);
			return -EINVAL;
		}
        gpio_direction_output(vib->motor_en, 0);
	}

	if (vib->motor_pwm > 0) {
		if (gpio_request(vib->motor_pwm, "motor_pwm")) {
			pr_err("%s: motor_pwm request failed\n", __func__);
			return -EINVAL;
		}
		if (!of_find_property(np, "clock-names", NULL)) {
			pr_err("%s: cannot get clock-names \n",__func__);
		} else {
			pr_info("%s: get motor-clk \n",__func__);
			vib->motor_clk = clk_get(vib->dev, "motor-clk");
			if (IS_ERR(vib->motor_clk)) {
				pr_err("%s: clk get %s failed\n", __func__, "motor_clk");
				return -EINVAL;
			}
		}
	}

	ret = of_property_read_u32(np, "m_default", &vib->m_default);
	if (ret) {
		pr_err("m_default not specified so using default address\n");
		vib->m_default = GP_CLK_M_DEFAULT;
	}

	ret = of_property_read_u32(np, "n_default", &vib->n_default);
	if (ret) {
		pr_err("n_default not specified so using default address\n");
		vib->n_default = GP_CLK_N_DEFAULT;
	}
	g_nlra_gp_clk_m = vib->m_default;
	g_nlra_gp_clk_n = vib->n_default;
	g_nlra_gp_clk_d = (vib->n_default)/2;
	g_nlra_gp_clk_pwm_mul = vib->n_default;

	ret = of_property_read_u32(np, "motor_strength",
							&vib->motor_strength);
	if (ret) {
		pr_err("motor_strength not specified so using default address\n");
		vib->motor_strength = MOTOR_STRENGTH;
	}
	motor_strength = vib->motor_strength;

	return 0;
}
#endif /* CONFIG_OF */

static int msm_vibrator_probe(struct platform_device *pdev)
{
	struct msm_vib *vib;
#ifdef CONFIG_DC_MOTOR_PMIC
	struct device *vib_dev;
#endif
	int rc;

	vib = devm_kzalloc(&pdev->dev, sizeof(*vib), GFP_KERNEL);
	if (!vib)	{
		pr_err("%s : Failed to allocate memory\n", __func__);
		return -ENOMEM;
	}
	
	vib->dev = &pdev->dev;
	
#ifdef CONFIG_OF
	rc = vib_parse_dt(&pdev->dev, vib);
	if (rc < 0) {
		pr_err("[VIB] failed to parse dt\n");
		return -EINVAL;
	}
#endif

	vib->timeout = VIB_DEFAULT_TIMEOUT;
	if (vib->motor_pwm) {
		vib->motor_enable_state = false;
		clk_set_rate(vib->motor_clk,CLK_RATE);
		vib->intensity = MAX_INTENSITY;
		vibe_set_intensity(vib->intensity);
	}
	INIT_WORK(&vib->work, msm_vibrator_update);
	mutex_init(&vib->lock);
	wake_lock_init(&vib->wklock, WAKE_LOCK_SUSPEND, "vibrator");

	vib->queue = create_singlethread_workqueue("msm_vibrator");
	if (!vib->queue) {
		pr_err("%s(): can't create workqueue\n", __func__);
		return -ENOMEM;
	}

	hrtimer_init(&vib->vib_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	vib->vib_timer.function = vibrator_timer_func;

	vib->timed_dev.name = "vibrator";
	vib->timed_dev.get_time = vibrator_get_time;
	vib->timed_dev.enable = vibrator_enable;

	dev_set_drvdata(&pdev->dev, vib);

	rc = timed_output_dev_register(&vib->timed_dev);
	if (rc < 0) {
		pr_err("[VIB] timed_output_dev_register fail (rc=%d)\n", rc);
		goto err_read_vib;
	}

	if (vib->motor_pwm) {
		rc = sysfs_create_file(&vib->timed_dev.dev->kobj, &dev_attr_intensity.attr);
		if (rc) {
			pr_err("[VIB] failed to register intensity sysfs\n");
			goto err_read_vib;
		}
	}

#ifdef CONFIG_DC_MOTOR_PMIC
	vib_dev = sec_device_create(0, NULL, "vib");
	if (IS_ERR(vib_dev)) {
		pr_err("[VIB] failed to create device\n");
		goto err_read_vib;
	}

	rc = sysfs_create_file(&vib_dev->kobj, &dev_attr_dc_pmic.attr);
	if (rc) {
		pr_err("[VIB] failed to register dc_pmic sysfs\n");
		goto err_read_vib;
	}
#endif
	return 0;
err_read_vib:
	destroy_workqueue(vib->queue);
	wake_lock_destroy(&vib->wklock);
	mutex_destroy(&vib->lock);

	return rc;
}

static int msm_vibrator_remove(struct platform_device *pdev)
{
	struct msm_vib *vib = dev_get_drvdata(&pdev->dev);

	destroy_workqueue(vib->queue);
	mutex_destroy(&vib->lock);
	wake_lock_destroy(&vib->wklock);

	return 0;
}

static const struct of_device_id vib_motor_match[] = {
	{	.compatible = "vibrator", },
	{}
};

static struct platform_driver msm_vibrator_platdrv =
{
	.driver = {
		.name = "msm_vibrator",
		.owner = THIS_MODULE,
		.of_match_table = vib_motor_match,
		.pm	= &vibrator_pm_ops,
	},
	.probe = msm_vibrator_probe,
	.remove = msm_vibrator_remove,
};

static int __init msm_timed_vibrator_init(void)
{
	return platform_driver_register(&msm_vibrator_platdrv);
}

void __exit msm_timed_vibrator_exit(void)
{
	platform_driver_unregister(&msm_vibrator_platdrv);
}
module_init(msm_timed_vibrator_init);
module_exit(msm_timed_vibrator_exit);

MODULE_DESCRIPTION("timed output vibrator device");
MODULE_LICENSE("GPL v2");
