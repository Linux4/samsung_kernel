/*
 * linux/driver/thermal/pxa988_thermal.c
 *
 * Author:      Hong Feng <hongfeng@marvell.com>
 * Copyright:   (C) 2012 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/thermal.h>
#include <linux/io.h>
#include <linux/syscalls.h>
#include <linux/cpufreq.h>
#include <linux/smp.h>
#include <linux/workqueue.h>
#include <mach/regs-apbc.h>
#include <mach/cputype.h>

/* debug: Use for sysfs set temp */
/* #define DEBUG_TEMPERATURE */
#define TRIP_POINTS_NUM	4
#define TRIP_POINTS_ACTIVE_NUM (TRIP_POINTS_NUM - 1)

#define THERMAL_VIRT_BASE (APB_VIRT_BASE + 0x13200)
#define THERMAL_REG(x) (THERMAL_VIRT_BASE + (x))
#define THERMAL_TS_CTRL THERMAL_REG(0x20)
#define THERMAL_TS_READ THERMAL_REG(0x24)
#define THERMAL_TS_CLR THERMAL_REG(0x28)
#define THERMAL_TS_THD THERMAL_REG(0x2c)
#define THERMAL_TS_DUR THERMAL_REG(0x30)
/* TS_CTRL 0x20 */
#define TS_HW_AUTO_ENABLE (1 << 29)
#define TS_OVER_RANGE_RST_ENABLE (1 << 28)
#define TS_WARNING_MASK (1 << 25)
#define TS_ON_INT_MASK (1 << 18)
#define TS_CTRL_TSEN_TEMP_ON (1 << 3)
#define TS_CTRL_RST_N_TSEN (1 << 2)
#define TS_CTRL_TSEN_LOW_RANGE (1 << 1)
#define TS_CTRL_TSEN_CHOP_EN (1 << 0)
/* TS_READ 0x24 */
#define TS_RST_FLAG (1 << 12)
#define TS_WRAINGING_INT (1 << 9)
#define TS_READ_TS_ON (1 << 4)
#define TS_READ_OUT_DATA (0xf << 0)
/* TS_CLR 0x28 */
#define TS_ON_INT_CLR (1 << 4)
#define TS_OVER_RANGE_INT_CLR (1 << 3)
#define TS_HIGH_INT_CLR (1 << 2)
#define TS_WAINING_INT_CLR (1 << 1)
#define TS_RST_FLAG_CLR (1 << 0)

#define COOLDEV_ACTIVE_MAX_STATE 1

struct pxa988_thermal_device {
	struct thermal_zone_device *therm_cpu;
	int temp_cpu;
	int last_temp_cpu;
	struct clk *therm_clk;
	struct thermal_cooling_device *cool_dev_active[TRIP_POINTS_ACTIVE_NUM];
	int hit_trip_cnt[TRIP_POINTS_NUM];
	int irq;
};

struct cool_dev_priv {
	int current_state;
	int max_state;
};

static struct pxa988_thermal_device pxa988_thermal_dev;

static int cpu_thermal_trips_temp[TRIP_POINTS_NUM] = {
	85000,/* bind to active type */
	95000,/* bind to active type */
	105000,/* bind to active type */
	110000,/* bind to critical type */
};
static struct cool_dev_priv cool_dev_active_priv[TRIP_POINTS_ACTIVE_NUM] = {
	{0, COOLDEV_ACTIVE_MAX_STATE},
	{0, COOLDEV_ACTIVE_MAX_STATE},
	{0, COOLDEV_ACTIVE_MAX_STATE},
};
static struct work_struct trip_ck_work;

static int is_pxa98x_Zx(void)
{
	/*
	 * TODO: workaround
	 * A0 hw auto test has possible report >= 107 degree
	 * when resume, using polling mode for workaround
	 */
	return 1;
#ifdef DEBUG_TEMPERATURE
	/*
	 * in debug mode, return Zx chip, then framework will
	 * start 2s polling
	 */
	return 1;
#endif
	return cpu_is_pxa988_z1() || cpu_is_pxa988_z2() ||
		cpu_is_pxa988_z3() || cpu_is_pxa986_z1() ||
		cpu_is_pxa986_z2() || cpu_is_pxa986_z3();
}

static int cool_dev_get_max_state(struct thermal_cooling_device *cdev,
		unsigned long *state)
{
	struct cool_dev_priv *priv = (struct cool_dev_priv *)cdev->devdata;
	*state = priv->max_state;
	return 0;
}

static int cool_dev_get_current_state(struct thermal_cooling_device *cdev,
		unsigned long *state)
{
	struct cool_dev_priv *priv = (struct cool_dev_priv *)cdev->devdata;
	*state = priv->current_state;
	return 0;
}

static int cool_dev_set_current_state(struct thermal_cooling_device *cdev,
		unsigned long state)
{
	int i;
	char *temp_info[2]    = { "TEMP=100000", NULL };
	static int drop_init_check;
	struct cool_dev_priv *priv = (struct cool_dev_priv *)cdev->devdata;
	if (state > COOLDEV_ACTIVE_MAX_STATE)
		state = COOLDEV_ACTIVE_MAX_STATE;
	/*
	 * This is extremely embarrass, thermal_zone_device_register
	 * will directly call into here, while this time kobject_uevent
	 * is not ready for use(will panic), so we drop first round check
	 */
	if (drop_init_check < TRIP_POINTS_ACTIVE_NUM) {
		drop_init_check++;
		goto out;
	}

	if (state == priv->current_state)
		goto out;
	priv->current_state = state;
	/* notify user for trip point cross */
	for (i = 0; i < TRIP_POINTS_ACTIVE_NUM; i++) {
		if (cdev == pxa988_thermal_dev.cool_dev_active[i]) {
			sprintf(temp_info[0], "TEMP=%d",
					pxa988_thermal_dev.temp_cpu);
			kobject_uevent_env(&((pxa988_thermal_dev.therm_cpu)->
				device.kobj), KOBJ_CHANGE, temp_info);

		}
	}
out:
	return 0;
}

static struct thermal_cooling_device_ops cool_dev_active_ops = {
	.get_max_state = cool_dev_get_max_state,
	.get_cur_state = cool_dev_get_current_state,
	.set_cur_state = cool_dev_set_current_state,
};

static int cpu_sys_bind(struct thermal_zone_device *tz,
		struct thermal_cooling_device *cdev)
{
	int i;
	for (i = 0; i < TRIP_POINTS_ACTIVE_NUM; i++) {
		if (cdev == pxa988_thermal_dev.cool_dev_active[i])
			break;
	}
	return thermal_zone_bind_cooling_device(tz, i, cdev);
}

#ifdef DEBUG_TEMPERATURE
static int g_test_temp = 108000;

static int thermal_temp_debug_get(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", g_test_temp);
}

static int thermal_temp_debug_set(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	sscanf(buf, "%d\n", &g_test_temp);
	return count;
}

static DEVICE_ATTR(thermal_debug_temp, 0644, thermal_temp_debug_get,
		thermal_temp_debug_set);

#endif

static int hit_trip_status_get(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int i;
	int ret = 0;
	for (i = 0; i < TRIP_POINTS_NUM; i++) {
		ret += sprintf(buf + ret, "trip %d: %d hits\n",
				cpu_thermal_trips_temp[i],
				pxa988_thermal_dev.hit_trip_cnt[i]);
	}
	return ret;
}
static DEVICE_ATTR(hit_trip_status, 0444, hit_trip_status_get, NULL);

static struct attribute *thermal_attrs[] = {
#ifdef DEBUG_TEMPERATURE
	&dev_attr_thermal_debug_temp.attr,
#endif
	&dev_attr_hit_trip_status.attr,
	NULL,
};

static struct attribute_group thermal_attr_grp = {
	.attrs = thermal_attrs,
};

static void thermal_module_reset(int interval_temp)
{
	int i;
	unsigned long ts_ctrl;

	if (is_pxa98x_Zx()) {
		ts_ctrl = __raw_readl(THERMAL_TS_CTRL);
		ts_ctrl &= ~TS_CTRL_TSEN_TEMP_ON;
		__raw_writel(ts_ctrl, THERMAL_TS_CTRL);
	}

	ts_ctrl = __raw_readl(THERMAL_TS_CTRL);
	ts_ctrl &= ~TS_CTRL_RST_N_TSEN;
	__raw_writel(ts_ctrl, THERMAL_TS_CTRL);
	/*
	 * delay 2s to reset module, during this time,
	 * no lpm allowed
	 */
	for (i = 0; i < 100000; i++)
		udelay(20);
	ts_ctrl = __raw_readl(THERMAL_TS_CTRL);
	ts_ctrl |= TS_CTRL_RST_N_TSEN;
	__raw_writel(ts_ctrl, THERMAL_TS_CTRL);

	if (is_pxa98x_Zx()) {
		ts_ctrl = __raw_readl(THERMAL_TS_CTRL);
		ts_ctrl |= TS_CTRL_TSEN_TEMP_ON;
		__raw_writel(ts_ctrl, THERMAL_TS_CTRL);
	}
	pr_info("thermal module reset, interval temp %d\n", interval_temp);
}

/* This function decode 4bit long number of gray code into original binary */
static int gray_decode(unsigned int gray)
{
	int num, i, tmp;

	if (gray >= 16)
		return 0;

	num = gray & 0x8;
	tmp = num >> 3;
	for (i = 2; i >= 0; i--) {
		tmp = ((gray & (1 << i)) >> i) ^ tmp;
		num |= tmp << i;
	}
	return num;
}

#define RETRY_TIMES (10)
static int cpu_sys_get_temp(struct thermal_zone_device *thermal,
		unsigned long *temp)
{
	int i = 0;
	unsigned long ts_read, ts_ctrl;
	int interval_temp = 0;
	int gray_code = 0;
	int ret = 0;
#ifdef DEBUG_TEMPERATURE
	*temp = g_test_temp;
#else
	if (is_pxa98x_Zx()) {
		ts_read = __raw_readl(THERMAL_TS_READ);
		if (likely(ts_read & TS_READ_TS_ON)) {
			gray_code = ts_read & TS_READ_OUT_DATA;
			*temp = (gray_decode(gray_code) * 5 / 2 + 80) * 1000;
		} else {
			for (i = 0; i < RETRY_TIMES; i++) {
				ts_read = __raw_readl(THERMAL_TS_READ);
				if (ts_read & TS_READ_TS_ON) {
					gray_code = ts_read & TS_READ_OUT_DATA;
					break;
				}
				msleep(20);
			}
			if (RETRY_TIMES == i) {
				*temp = 0;
				ret = -1;
			} else
				*temp = (gray_decode(gray_code) * 5 / 2 + 80)
					* 1000;
		}
		/* restart measure */
		ts_ctrl = __raw_readl(THERMAL_TS_CTRL);
		ts_ctrl &= ~TS_CTRL_TSEN_TEMP_ON;
		__raw_writel(ts_ctrl, THERMAL_TS_CTRL);

		ts_ctrl = __raw_readl(THERMAL_TS_CTRL);
		ts_ctrl |= TS_CTRL_TSEN_TEMP_ON;
		__raw_writel(ts_ctrl, THERMAL_TS_CTRL);
	} else {
		/* For A0 or next steping, already get temp in irq handler */
		*temp = pxa988_thermal_dev.temp_cpu;
	}
	/* TODO: workaround */
	if (!pxa988_thermal_dev.last_temp_cpu) {
		/* Let's recognize it's silicon bug first time >= 110C */
		if (*temp >= 110000) {
			thermal_module_reset(*temp - 80000);
			*temp = 80000;
		} else
			pxa988_thermal_dev.last_temp_cpu = *temp;
	} else {
		interval_temp = (*temp >= pxa988_thermal_dev.last_temp_cpu) ?
			(*temp - pxa988_thermal_dev.last_temp_cpu) :
			(pxa988_thermal_dev.last_temp_cpu - *temp);
		/*
		 * it's impossible interval temp > 20C,
		 * currently, it's for EMEI-251 silicon bug:
		 * after thermal come out of "reset" and "clock off"
		 * there is posibility second read get false high
		 * temperature
		 */
		if (interval_temp > 20000) {
			thermal_module_reset(interval_temp);
			*temp = pxa988_thermal_dev.last_temp_cpu;
		} else
			pxa988_thermal_dev.last_temp_cpu = *temp;
	}
#endif
	pxa988_thermal_dev.temp_cpu = *temp;
	for (i = (TRIP_POINTS_NUM-1); i >= 0; i--) {
		if (pxa988_thermal_dev.temp_cpu >= cpu_thermal_trips_temp[i]) {
			pxa988_thermal_dev.hit_trip_cnt[i]++;
			break;
		}
	}
	return ret;
}

static int cpu_sys_get_trip_type(struct thermal_zone_device *thermal, int trip,
		enum thermal_trip_type *type)
{
	if ((trip >= 0) && (trip < TRIP_POINTS_ACTIVE_NUM))
		*type = THERMAL_TRIP_ACTIVE;
	else if (TRIP_POINTS_ACTIVE_NUM == trip)
		*type = THERMAL_TRIP_CRITICAL;
	else
		*type = (enum thermal_trip_type)(-1);
	return 0;
}

static int cpu_sys_get_trip_temp(struct thermal_zone_device *thermal, int trip,
		unsigned long *temp)
{
	if ((trip >= 0) && (trip < TRIP_POINTS_NUM))
		*temp = cpu_thermal_trips_temp[trip];
	else
		*temp = -1;
	return 0;
}

static int cpu_sys_set_trip_temp(struct thermal_zone_device *thermal, int trip,
		unsigned long temp)
{
	if ((trip >= 0) && (trip < TRIP_POINTS_NUM))
		cpu_thermal_trips_temp[trip] = temp;
	return 0;
}

static int cpu_sys_get_crit_temp(struct thermal_zone_device *thermal,
		unsigned long *temp)
{
	return cpu_thermal_trips_temp[TRIP_POINTS_NUM - 1];
}

static int cpu_sys_notify(struct thermal_zone_device *thermal, int count,
		enum thermal_trip_type trip_type)
{
	if (THERMAL_TRIP_CRITICAL == trip_type)
		pr_info("notify critical temp hit\n");
	else
		pr_err("unexpected temp notify\n");
	/*
	 * when THERMAL_TRIP_CRITICAL, return 0
	 * will trigger shutdown in opensource framework
	 */
	return 0;
}

static struct thermal_zone_device_ops cpu_thermal_ops = {
	.bind = cpu_sys_bind,
	.get_temp = cpu_sys_get_temp,
	.get_trip_type = cpu_sys_get_trip_type,
	.get_trip_temp = cpu_sys_get_trip_temp,
	.set_trip_temp = cpu_sys_set_trip_temp,
	.get_crit_temp = cpu_sys_get_crit_temp,
	.notify = cpu_sys_notify,
};

#ifdef CONFIG_PM
static int thermal_suspend(struct device *dev)
{
	clk_disable(pxa988_thermal_dev.therm_clk);
	return 0;
}

static int thermal_resume(struct device *dev)
{
	clk_enable(pxa988_thermal_dev.therm_clk);
	return 0;
}

static const struct dev_pm_ops thermal_pm_ops = {
	.suspend = thermal_suspend,
	.resume = thermal_resume,
};
#endif

static void thermal_trip_check(struct work_struct *work)
{
	if (pxa988_thermal_dev.therm_cpu)
		thermal_zone_device_update(pxa988_thermal_dev.therm_cpu);
}

static irqreturn_t pxa988_thermal_irq(int irq, void *devid)
{
	unsigned long ts_clr, ts_read, ts_ctrl;
	int gray_code = 0;

	ts_read = __raw_readl(THERMAL_TS_READ);
	if (likely(ts_read & TS_READ_TS_ON)) {
		gray_code = ts_read & TS_READ_OUT_DATA;
		pxa988_thermal_dev.temp_cpu =
			(gray_decode(gray_code) * 5 / 2 + 80) * 1000;
		/* if temp drop to 80, then disable ON interrupt */
		if (0 == gray_code) {
			ts_ctrl = __raw_readl(THERMAL_TS_CTRL);
			ts_ctrl &= ~TS_ON_INT_MASK;
			__raw_writel(ts_ctrl, THERMAL_TS_CTRL);
		}
	}

	/* hit 85 degree, enable data ready interrput */
	if (ts_read & TS_WRAINGING_INT) {
		ts_ctrl = __raw_readl(THERMAL_TS_CTRL);
		ts_ctrl |= TS_ON_INT_MASK;
		__raw_writel(ts_ctrl, THERMAL_TS_CTRL);
	}
	/* clear interrupts */
	ts_clr = __raw_readl(THERMAL_TS_CLR);
	ts_clr |= TS_ON_INT_CLR | TS_OVER_RANGE_INT_CLR |
		TS_HIGH_INT_CLR | TS_WAINING_INT_CLR;
	__raw_writel(ts_clr, THERMAL_TS_CLR);

	queue_work(system_wq, &trip_ck_work);

	return IRQ_HANDLED;
}

static int pxa988_thermal_probe(struct platform_device *pdev)
{
	int ret, irq, i;
	unsigned long ts_ctrl, ts_read, ts_clr, ts_thd, ts_dur;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "no IRQ resource defined\n");
		return -ENXIO;
	}
	ret = request_irq(irq, pxa988_thermal_irq, IRQF_DISABLED,
			pdev->name, NULL);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to request IRQ\n");
		return -ENXIO;
	}
	pxa988_thermal_dev.irq = irq;

	/* The last trip point is critical */
	for (i = 0; i < TRIP_POINTS_ACTIVE_NUM; i++) {
		pxa988_thermal_dev.cool_dev_active[i] =
			thermal_cooling_device_register("cool_dev_active",
					(void *)&cool_dev_active_priv[i],
					&cool_dev_active_ops);
		if (IS_ERR(pxa988_thermal_dev.cool_dev_active[i])) {
			pr_err("Failed to register cooling device\n");
			ret = -EINVAL;
			goto failed_free_irq;
		}
	}

	pxa988_thermal_dev.therm_clk = clk_get(NULL, "THERMALCLK");
	if (IS_ERR(pxa988_thermal_dev.therm_clk)) {
		pr_err("Could not get thermal clock\n");
		ret = -ENXIO;
		goto failed_unregister_cooldev;
	}
	clk_enable(pxa988_thermal_dev.therm_clk);

	pxa988_thermal_dev.last_temp_cpu = 0;
#ifndef DEBUG_TEMPERATURE
	if (is_pxa98x_Zx()) {
		ts_ctrl = __raw_readl(THERMAL_TS_CTRL);
		ts_ctrl |= TS_CTRL_TSEN_CHOP_EN;
		/* we only care greater than 80 */
		ts_ctrl &= ~TS_CTRL_TSEN_LOW_RANGE;
		__raw_writel(ts_ctrl, THERMAL_TS_CTRL);

		ts_ctrl = __raw_readl(THERMAL_TS_CTRL);
		ts_ctrl |= TS_CTRL_RST_N_TSEN;
		__raw_writel(ts_ctrl, THERMAL_TS_CTRL);

		/* start first measure */
		ts_ctrl = __raw_readl(THERMAL_TS_CTRL);
		ts_ctrl |= TS_CTRL_TSEN_TEMP_ON;
		__raw_writel(ts_ctrl, THERMAL_TS_CTRL);
	} else {

		INIT_WORK(&trip_ck_work, thermal_trip_check);
		ts_read = __raw_readl(THERMAL_TS_READ);
		if (ts_read & TS_RST_FLAG) {
			ts_clr = __raw_readl(THERMAL_TS_CLR);
			ts_clr |= TS_RST_FLAG_CLR;
			__raw_writel(ts_clr, THERMAL_TS_CLR);
			pr_crit("system reboot with temperature 0x%x",
					(unsigned int)ts_read);
		}
		ts_ctrl = __raw_readl(THERMAL_TS_CTRL);
		ts_ctrl |= TS_CTRL_TSEN_CHOP_EN;
		/* we only care greater than 80 */
		ts_ctrl &= ~TS_CTRL_TSEN_LOW_RANGE;
		ts_ctrl |= TS_OVER_RANGE_RST_ENABLE;
		/*
		 * init, only set warning interrupt, if temp hit warning, then
		 * enable data ready interrupt, then if temp drops to 80 degree,
		 * disable data ready interrupt
		 */
		ts_ctrl |= TS_WARNING_MASK;
		__raw_writel(ts_ctrl, THERMAL_TS_CTRL);
		/* 32k clock --> 2s */
		ts_dur = 32000 * 2;
		__raw_writel(ts_dur, THERMAL_TS_DUR);
		ts_ctrl = __raw_readl(THERMAL_TS_CTRL);
		ts_ctrl |= TS_HW_AUTO_ENABLE;
		__raw_writel(ts_ctrl, THERMAL_TS_CTRL);
		/* set warning threshold 85 degree */
		ts_thd = 0x02;
		__raw_writel(ts_thd, THERMAL_TS_THD);

		pxa988_thermal_dev.temp_cpu = 80 * 1000;
		/* start measure */
		ts_ctrl = __raw_readl(THERMAL_TS_CTRL);
		ts_ctrl |= TS_CTRL_RST_N_TSEN;
		__raw_writel(ts_ctrl, THERMAL_TS_CTRL);
	}
#endif
	if (is_pxa98x_Zx()) {
		pxa988_thermal_dev.therm_cpu = thermal_zone_device_register(
			"pxa988-thermal", TRIP_POINTS_NUM,
			NULL, &cpu_thermal_ops, 1, 1, 2000, 2000);
	} else {
		pxa988_thermal_dev.therm_cpu = thermal_zone_device_register(
			"pxa988-thermal", TRIP_POINTS_NUM,
			NULL, &cpu_thermal_ops, 1, 1, 2000, 0);
	}
	if (IS_ERR(pxa988_thermal_dev.therm_cpu)) {
		pr_err("Failed to register thermal zone device\n");
		return -EINVAL;
	}

	ret = sysfs_create_group(&((pxa988_thermal_dev.therm_cpu->device).kobj),
			&thermal_attr_grp);
	/*
	 * if fail, just report err, debug interface register error should
	 * not terminate the whole driver probe
	 */
	if (ret < 0)
		pr_err("Failed to register private thermal interface\n");

	return 0;
failed_unregister_cooldev:
	for (i = 0; i < TRIP_POINTS_ACTIVE_NUM; i++)
		thermal_cooling_device_unregister(
				pxa988_thermal_dev.cool_dev_active[i]);
failed_free_irq:
	free_irq(pxa988_thermal_dev.irq, NULL);
	return ret;
}

static int pxa988_thermal_remove(struct platform_device *pdev)
{
	int i;
	clk_disable(pxa988_thermal_dev.therm_clk);
	thermal_zone_device_unregister(pxa988_thermal_dev.therm_cpu);
	for (i = 0; i < TRIP_POINTS_ACTIVE_NUM; i++)
		thermal_cooling_device_unregister(
				pxa988_thermal_dev.cool_dev_active[i]);
	free_irq(pxa988_thermal_dev.irq, NULL);
	pr_info("PXA988: Kernel Thermal management unregistered\n");
	return 0;
}

static struct platform_driver pxa988_thermal_driver = {
	.driver = {
		.name   = "thermal",
		.owner  = THIS_MODULE,
#ifdef CONFIG_PM
		.pm	= &thermal_pm_ops,
#endif
	},
	.probe          = pxa988_thermal_probe,
	.remove         = pxa988_thermal_remove,
};

static int __init pxa988_thermal_init(void)
{
	return platform_driver_register(&pxa988_thermal_driver);
}

static void __exit pxa988_thermal_exit(void)
{
	platform_driver_unregister(&pxa988_thermal_driver);
}

module_init(pxa988_thermal_init);
module_exit(pxa988_thermal_exit);

MODULE_AUTHOR("Marvell Semiconductor");
MODULE_DESCRIPTION("PXA988 SoC thermal driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:pxa988-thermal");
