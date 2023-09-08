/* afc_charger.c
 *
 * Copyright (C) 2020 Samsung Electronics
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#define pr_fmt(fmt)    "[AFC] " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#if defined(CONFIG_DRV_SAMSUNG)
#include <linux/sec_class.h>
#endif
#if defined(CONFIG_SEC_PARAM)
#include <linux/sec_ext.h>
#endif
#include <linux/ktime.h>
#include <linux/pinctrl/consumer.h>
#include <mt-plat/mtk_battery.h>
#include <mt-plat/charger_type.h>
#include <mt-plat/mtk_boot_common.h>
#include <mt-plat/afc_charger.h>
#include "../drivers/misc/mediatek/typec/tcpc/inc/mt6360.h"
#include "../drivers/misc/mediatek/typec/tcpc/inc/tcpci.h"

#ifdef CONFIG_BATTERY_SAMSUNG
#include <dt-bindings/battery/sec-battery.h>
#include <linux/power_supply.h>
#include <../drivers/battery/common/sec_charging_common.h>
#include <../drivers/battery/common/sec_battery.h>
#endif

#if IS_ENABLED(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/common/muic_notifier.h>
#endif

struct gpio_afc_ddata *g_ddata;

#if IS_ENABLED(CONFIG_SEC_HICCUP)
static int gpio_hiccup;
#endif

/* afc_mode:
 *   0x31 : Disabled
 *   0x30 : Enabled
 */
static int afc_mode;

static int __init set_charging_mode(char *str)
{
	int mode;

	get_option(&str, &mode);
	afc_mode = (mode & 0x0000FF00) >> 8;
	pr_info("%s: afc_mode is 0x%02x\n", __func__, afc_mode);

	return 0;
}
early_param("charging_mode", set_charging_mode);

static int _get_afc_mode(void)
{
	return afc_mode;
}

#if 0
static void afc_udelay(int delay)
{
	s64 start = 0, duration = 0;

	start = ktime_to_us(ktime_get());

	udelay(delay);

	duration = ktime_to_us(ktime_get()) - start;

	if (duration > delay + 20)
		pr_err("%s %dus -> %dus\n", __func__, delay, duration);
}
#else
#define afc_udelay(d)	udelay(d)
#endif

static void gpio_afc_send_parity_bit(struct gpio_afc_ddata *ddata, int data)
{
	int cnt = 0, i = 0;
	int gpio = ddata->pdata->gpio_afc_data;

	for (i = 0; i < 8; i++) {
		if (data & (0x1 << i))
			cnt++;
	}

	cnt %= 2;

	gpio_set_value(gpio, !cnt);
	afc_udelay(UI);

	if (!cnt) {
		gpio_set_value(gpio, 0);
		afc_udelay(SYNC_PULSE);
	}
}

static void gpio_afc_sync_pulse(int gpio)
{
	gpio_set_value(gpio, 1);
	afc_udelay(SYNC_PULSE);
	gpio_set_value(gpio, 0);
	afc_udelay(SYNC_PULSE);
}

static int gpio_afc_send_mping(struct gpio_afc_ddata *ddata)
{
	int gpio = ddata->pdata->gpio_afc_data;
	unsigned long flags;
	int ret = 0;
	s64 start = 0, end = 0;

	if (ddata->gpio_input) {
		ddata->gpio_input = false;
		gpio_direction_output(ddata->pdata->gpio_afc_data, 0);
	}

	/* send mping */
	spin_lock_irqsave(&ddata->spin_lock, flags);
	gpio_set_value(gpio, 1);
	udelay(MPING);
	gpio_set_value(gpio, 0);

	start = ktime_to_us(ktime_get());
	spin_unlock_irqrestore(&ddata->spin_lock, flags);
	end = ktime_to_us(ktime_get());

	ret = (int)end-start;
	return ret;
}

static int gpio_afc_check_sping(struct gpio_afc_ddata *ddata, int delay)
{
	int gpio = ddata->pdata->gpio_afc_data;
	int ret = 0;
	s64 start = 0, end = 0, duration = 0;
	unsigned long flags;

	if (delay > (MPING+3*UI)) {
		ret = delay - (MPING+3*UI);
		return ret;
	}

	if (!ddata->gpio_input) {
		ddata->gpio_input = true;
		gpio_direction_input(gpio);
	}

	spin_lock_irqsave(&ddata->spin_lock, flags);
	if (delay < (MPING-3*UI)) {
		udelay(MPING-3*UI-delay);
		if (!gpio_get_value(gpio)) {
			ret = -EAGAIN;
			goto out;
		}
	}
	start = ktime_to_us(ktime_get());
	while (gpio_get_value(gpio)) {
		duration = ktime_to_us(ktime_get()) - start;
		if (duration > DATA_DELAY)
			break;
		udelay(UI);
	}
out:
	start = ktime_to_us(ktime_get());
	spin_unlock_irqrestore(&ddata->spin_lock, flags);
	end = ktime_to_us(ktime_get());

	if (ret == 0)
		ret = (int)end-start;

	return ret;
}

static int gpio_afc_send_data(struct gpio_afc_ddata *ddata, int data)
{
	int i = 0, ret = 0;
	int gpio = ddata->pdata->gpio_afc_data;
	unsigned long flags;
	s64 start = 0, end = 0;

	if (ddata->gpio_input) {
		ddata->gpio_input = false;
		gpio_direction_output(ddata->pdata->gpio_afc_data, 0);
	}

	spin_lock_irqsave(&ddata->spin_lock, flags);

	udelay(UI);

	/* start of transfer */
	gpio_afc_sync_pulse(gpio);
	if (!(data & 0x80)) {
		gpio_set_value(gpio, 1);
		afc_udelay(SYNC_PULSE);
	}

	for (i = 0x80; i > 0; i = i >> 1) {
		gpio_set_value(gpio, data & i);
		afc_udelay(UI);
	}

	gpio_afc_send_parity_bit(ddata, data);
	gpio_afc_sync_pulse(gpio);

	gpio_set_value(gpio, 1);
	udelay(MPING);
	gpio_set_value(gpio, 0);

	start = ktime_to_us(ktime_get());
	spin_unlock_irqrestore(&ddata->spin_lock, flags);
	end = ktime_to_us(ktime_get());

	ret = (int)end-start;

	return ret;
}

static int gpio_afc_recv_data(struct gpio_afc_ddata *ddata, int delay)
{
	int gpio = ddata->pdata->gpio_afc_data;
	int ret = 0, gpio_value = 0, reset = 1;
	s64 limit_start = 0, start = 0, end = 0, duration = 0;
	unsigned long flags;

	if (!ddata->gpio_input) {
		ddata->gpio_input = true;
		gpio_direction_input(gpio);
	}

	if (delay > DATA_DELAY+MPING) {
		ret = -EAGAIN;
	} else if (delay > DATA_DELAY && delay <= DATA_DELAY+MPING) {
		gpio_afc_check_sping(ddata, delay-DATA_DELAY);
	} else if (delay <= DATA_DELAY) {
		spin_lock_irqsave(&ddata->spin_lock, flags);

		udelay(DATA_DELAY-delay);

		limit_start = ktime_to_us(ktime_get());
		while (duration < MPING-3*UI) {
			if (reset) {
				start = 0;
				end = 0;
				duration = 0;
			}
			gpio_value = gpio_get_value(gpio);
			if (!gpio_value && !reset) {
				end = ktime_to_us(ktime_get());
				duration = end - start;
				reset = 1;
			} else if (gpio_value) {
				if (reset) {
					start = ktime_to_us(ktime_get());
					reset = 0;
				}
			}
			udelay(UI);
			if ((ktime_to_us(ktime_get()) - limit_start) > (MPING+DATA_DELAY*2)) {
				ret = -EAGAIN;
				break;
			}
		}

		spin_unlock_irqrestore(&ddata->spin_lock, flags);
	}

	return ret;
}

static int gpio_afc_set_voltage(struct gpio_afc_ddata *ddata, u32 voltage)
{
	int ret = 0;

	ret = gpio_afc_send_mping(ddata);
	ret = gpio_afc_check_sping(ddata, ret);
	if (ret < 0) {
		pr_err("Start Mping NACK\n");
		goto out;
	}

	if (voltage == 0x9)
		ret = gpio_afc_send_data(ddata, 0x46);
	else
		ret = gpio_afc_send_data(ddata, 0x08);

	ret = gpio_afc_check_sping(ddata, ret);
	if (ret < 0) {
		pr_err("sping err2 %d\n", ret);
		goto out;
	}

	ret = gpio_afc_recv_data(ddata, ret);
	if (ret < 0)
		pr_err("sping err3 %d\n", ret);

	ret = gpio_afc_send_mping(ddata);
	ret = gpio_afc_check_sping(ddata, ret);
	if (ret < 0)
		pr_err("End Mping NACK\n");
out:
	return ret;
}

static void gpio_afc_reset(struct gpio_afc_ddata *ddata)
{
	int gpio = ddata->pdata->gpio_afc_data;

	if (ddata->gpio_input) {
		ddata->gpio_input = false;
		gpio_direction_output(ddata->pdata->gpio_afc_data, 0);
	}

	/* send mping */
	gpio_set_value(gpio, 1);
	udelay(RESET_DELAY);
	gpio_set_value(gpio, 0);
}

void set_afc_voltage_for_performance(bool enable)
{
	struct gpio_afc_ddata *ddata = g_ddata;

	ddata->check_performance = enable;
}

static int vbus_level_check(void)
{
	int vbus = battery_get_vbus();

	pr_info("%s %dmV\n", __func__, vbus);

	if (vbus > 3800 && vbus < 6500)
		vbus = 0x5;
	else if (vbus > 7500 && vbus < 10500)
		vbus = 0x9;
	else
		vbus = 0x0;

	return vbus;
}

static void gpio_afc_kwork(struct kthread_work *work)
{
	struct gpio_afc_ddata *ddata =
	    container_of(work, struct gpio_afc_ddata, kwork);
#if defined(CONFIG_BATTERY_SAMSUNG)
	struct power_supply *psy = power_supply_get_by_name("battery");
#endif
	int ret = 0, i = 0;
	int vol = 0;
	int retry = 0;

	if (!ddata) {
		pr_err("driver is not ready\n");
		return;
	}

	vol = ddata->set_voltage;

	if (vol == 0x9) {
		msleep(1200);
	} else {
		gpio_afc_reset(ddata);
		msleep(200);
	}

	ret = vbus_level_check();
	pr_info("%s set vol[%dV], current[%dV]\n",
		__func__, vol, ret);
	if (ret == 0) {
		pr_err("%s disconnected\n", __func__);
		return;
	}

	ddata->curr_voltage = ret;

	mutex_lock(&ddata->mutex);
	__pm_stay_awake(&ddata->ws);

	if (ret != vol) {
		gpio_set_value(ddata->pdata->gpio_afc_switch, 1);

		for (retry = 0; retry < AFC_RETRY_CNT; retry++) {
			for (i = 0; i < AFC_RETRY_CNT ; i++) {
				gpio_afc_set_voltage(ddata, vol);
				usleep_range(1000, 1000);
			}

			ret = vbus_level_check();
			ddata->curr_voltage = ret;
			pr_info("%s current %dV\n", __func__, ret);
			if (ret == vol) {
				ret = 0;
				pr_info("%s success\n", __func__);
				break;
			} else if (ret == 0) {
				pr_err("%s disconnected\n", __func__);
				ret = -EAGAIN;
				goto err;
			}

			ret = -EAGAIN;
			pr_err("%s retry %d\n", __func__, retry + 1);
		}
	} else
		ret = 0;

#if IS_ENABLED(CONFIG_MUIC_NOTIFIER)
	if (vol == 0x9) {
		if (!ret)
			muic_notifier_attach_attached_dev(ATTACHED_DEV_AFC_CHARGER_9V_MUIC);
		else
			muic_notifier_attach_attached_dev(ATTACHED_DEV_TA_MUIC);
	} else if (vol == 0x5) {
		if (!ret)
			muic_notifier_attach_attached_dev(ATTACHED_DEV_AFC_CHARGER_5V_MUIC);
		else if (vol == 0x9)
			muic_notifier_attach_attached_dev(ATTACHED_DEV_AFC_CHARGER_9V_DUPLI_MUIC);
	}
#endif /* CONFIG_USB_TYPEC_MANAGER_NOTIFIER */

#if defined(CONFIG_BATTERY_SAMSUNG)
	if (!IS_ERR_OR_NULL(psy)) {
		union power_supply_propval val;

		val.intval = (ddata->curr_voltage == 0x9) ? AFC_CHARGER : STANDARD_CHARGER;

		pr_info("%s: curr_voltage: %d\n", __func__, ddata->curr_voltage);
		if (val.intval == AFC_CHARGER)
			val.intval = SEC_BATTERY_CABLE_9V_TA;
		else if (ddata->check_performance) {
			if (val.intval == STANDARD_CHARGER) {
				if (!ret)
					val.intval = SEC_BATTERY_CABLE_HV_TA_CHG_LIMIT;
				else {
					pr_info("%s: couldn't set 5V\n", __func__);
					val.intval = SEC_BATTERY_CABLE_9V_TA;
				}
			}
		}
		if ((val.intval == SEC_BATTERY_CABLE_HV_TA_CHG_LIMIT) ||
			(val.intval == SEC_BATTERY_CABLE_9V_TA)) {
			ret = power_supply_set_property(psy,
					POWER_SUPPLY_PROP_ONLINE, &val);
			if (ret < 0)
				pr_err("%s: Fail to set POWER_SUPPLY_PROP_ONLINE (%d)\n",
					__func__, ret);
		}
	}
#endif
err:
	gpio_set_value(ddata->pdata->gpio_afc_switch, 0);
	__pm_relax(&ddata->ws);
	mutex_unlock(&ddata->mutex);
}

int set_afc_voltage(int voltage)
{
	struct gpio_afc_ddata *ddata = g_ddata;
	int vbus = 0, cur = 0;

	if (!ddata) {
		pr_err("driver is not ready\n");
		return -EAGAIN;
	}

#if defined(CONFIG_DRV_SAMSUNG)
	if (voltage == 0x9 && ddata->afc_disable) {
		pr_err("AFC is disabled by USER\n");
		return -EINVAL;
	}
#endif

	kthread_flush_work(&ddata->kwork);

	cur = ddata->curr_voltage;
	vbus = vbus_level_check();

	pr_info("%s %d(%d) => %dV\n", __func__,
		ddata->curr_voltage, vbus, voltage);

	if (ddata->curr_voltage == voltage) {
		if (ddata->curr_voltage == vbus) {
			msleep(20);
			vbus = vbus_level_check();
			if (ddata->curr_voltage == vbus)
				return 0;
		}
	}

	ddata->curr_voltage = vbus;
	ddata->set_voltage = voltage;

	kthread_queue_work(&ddata->kworker, &ddata->kwork);

	return 0;
}

#if defined(CONFIG_DRV_SAMSUNG)
static ssize_t afc_disable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct gpio_afc_ddata *ddata = dev_get_drvdata(dev);

	if (ddata->afc_disable) {
		pr_info("%s AFC DISABLE\n", __func__);
		return sprintf(buf, "1\n");
	}

	pr_info("%s AFC ENABLE", __func__);
	return sprintf(buf, "0\n");
}

static ssize_t afc_disable_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct gpio_afc_ddata *ddata = dev_get_drvdata(dev);
	int param_val;
#if defined(CONFIG_SEC_PARAM)
	int ret = 0;
#endif
#ifdef CONFIG_BATTERY_SAMSUNG
	union power_supply_propval psy_val;
#endif

	if (!strncasecmp(buf, "1", 1)) {
		ddata->afc_disable = true;
	} else if (!strncasecmp(buf, "0", 1)) {
		ddata->afc_disable = false;
	} else {
		pr_warn("%s invalid value\n", __func__);

		return count;
	}

	param_val = ddata->afc_disable ? '1' : '0';

#if defined(CONFIG_SEC_PARAM)
	ret = sec_set_param(CM_OFFSET + 1, ddata->afc_disable ? '1' : '0');
	if (ret < 0)
		pr_err("%s:sec_set_param failed\n", __func__);
#endif

#ifdef CONFIG_BATTERY_SAMSUNG
	psy_val.intval = param_val;
	psy_do_property("battery", set, POWER_SUPPLY_EXT_PROP_HV_DISABLE, psy_val);
#endif
	pr_info("%s afc_disable(%d)\n", __func__, ddata->afc_disable);

	return count;
}

#if IS_ENABLED(CONFIG_SEC_HICCUP)
void set_sec_hiccup(bool en)
{
	int ret = 0;

	ret = get_boot_mode();
	if (ret == KERNEL_POWER_OFF_CHARGING_BOOT) {
		pr_info("%s %d, lpm mode\n", __func__, en, ret);
		return;
	}

	gpio_set_value(gpio_hiccup, en);

	ret = gpio_get_value(gpio_hiccup);

	pr_info("%s %d, %d\n", __func__, en, ret);
}

static ssize_t hiccup_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "ENABLE\n");
}

static ssize_t hiccup_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	if (!strncasecmp(buf, "DISABLE", 7))
		set_sec_hiccup(false);
	else
		pr_warn("%s invalid com : %s\n", __func__, buf);

	return count;
}
#endif /* CONFIG_SEC_HICCUP */

static ssize_t adc_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int result = 0;

#if IS_ENABLED(CONFIG_TCPC_MT6360) && IS_ENABLED(CONFIG_PDIC_NOTIFIER)
//temp	result = mt6360_usbid_check();
#endif

	pr_info("%s %d\n", __func__, result);

	return sprintf(buf, "%d\n", !!result);
}

static DEVICE_ATTR_RO(adc);
static DEVICE_ATTR_RW(afc_disable);
#if IS_ENABLED(CONFIG_SEC_HICCUP)
static DEVICE_ATTR_RW(hiccup);
#endif /* CONFIG_SEC_HICCUP */

static struct attribute *gpio_afc_attributes[] = {
	&dev_attr_adc.attr,
	&dev_attr_afc_disable.attr,
#if IS_ENABLED(CONFIG_SEC_HICCUP)
	&dev_attr_hiccup.attr,
#endif /* CONFIG_SEC_HICCUP */
	NULL
};
static const struct attribute_group gpio_afc_group = {
	.attrs = gpio_afc_attributes,
};
#endif /* CONFIG_DRV_SAMSUNG */

static struct gpio_afc_pdata *gpio_afc_get_dt(struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct gpio_afc_pdata *pdata;

	if (!np)
		return ERR_PTR(-ENODEV);

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	pdata->gpio_afc_switch = of_get_named_gpio(np, "gpio_afc_switch", 0);
	pdata->gpio_afc_data = of_get_named_gpio(np, "gpio_afc_data", 0);

	pr_info("request gpio_afc_switch %d, gpio_afc_data %d\n",
		pdata->gpio_afc_switch, pdata->gpio_afc_data);

#if IS_ENABLED(CONFIG_SEC_HICCUP)
	gpio_hiccup = of_get_named_gpio(np, "gpio_hiccup_en", 0);
	pr_info("gpio_hiccup_en : %d\n", gpio_hiccup);
#endif /* CONFIG_SEC_HICCUP */

	return pdata;
}

static int gpio_afc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct gpio_afc_pdata *pdata = dev_get_platdata(dev);
	struct gpio_afc_ddata *ddata;
	struct task_struct *kworker_task;
#if defined(CONFIG_BATTERY_SAMSUNG)
	union power_supply_propval psy_val;
#endif
	int ret = 0;

	pr_info("%s\n", __func__);

	if (!pdata)
		pdata = gpio_afc_get_dt(dev);

	ddata = devm_kzalloc(dev, sizeof(*ddata), GFP_KERNEL);
	if (!ddata)
		return -ENOMEM;

	ret = gpio_request(pdata->gpio_afc_switch, "gpio_afc_switch");
	if (ret < 0) {
		pr_err("failed to request afc switch gpio\n");
		return ret;
	}

	ret = gpio_request(pdata->gpio_afc_data, "gpio_afc_data");
	if (ret < 0) {
		pr_err("failed to request afc data gpio\n");
		return ret;
	}
#if IS_ENABLED(CONFIG_SEC_HICCUP)
	ret = gpio_request(gpio_hiccup, "hiccup_en");
	if (ret < 0) {
		pr_err("failed to request hiccup gpio\n");
		return ret;
	}
#endif /* CONFIG_SEC_HICCUP */

	wakeup_source_init(&ddata->ws, "afc_wakelock");
	spin_lock_init(&ddata->spin_lock);
	mutex_init(&ddata->mutex);
	dev_set_drvdata(dev, ddata);

	kthread_init_worker(&ddata->kworker);
	kworker_task = kthread_run(kthread_worker_fn,
		&ddata->kworker, "gpio_afc");
	if (IS_ERR(kworker_task))
		pr_err("Failed to create message pump task\n");

	kthread_init_work(&ddata->kwork, gpio_afc_kwork);

	ddata->pdata = pdata;
	ddata->gpio_input = false;

	g_ddata = ddata;
	afc_mode = 0;

#if defined(CONFIG_DRV_SAMSUNG)
	ddata->afc_disable = (_get_afc_mode() == '1' ? 1 : 0);
#ifdef CONFIG_BATTERY_SAMSUNG
	psy_val.intval = ddata->afc_disable ? '1' : '0';
	psy_do_property("battery", set, POWER_SUPPLY_EXT_PROP_HV_DISABLE, psy_val);
#endif
	ddata->dev = sec_device_create(ddata, "switch");
	if (IS_ERR(ddata->dev)) {
		pr_err("failed to create device\n");
		return -ENODEV;
	}

	ret = sysfs_create_group(&ddata->dev->kobj, &gpio_afc_group);
	if (ret) {
		pr_err("failed to create afc_disable sysfs\n");
		return ret;
	}
#endif
	gpio_direction_output(ddata->pdata->gpio_afc_switch, 0);
	gpio_direction_output(ddata->pdata->gpio_afc_data, 0);

	return 0;
}

static void gpio_afc_shutdown(struct platform_device *dev)
{
	/* TBD */
}

static const struct of_device_id gpio_afc_of_match[] = {
	{.compatible = "gpio_afc",},
	{},
};

MODULE_DEVICE_TABLE(of, gpio_afc_of_match);

static struct platform_driver gpio_afc_driver = {
	.shutdown = gpio_afc_shutdown,
	.driver = {
		   .name = "gpio_afc",
		   .of_match_table = gpio_afc_of_match,
	},
};

module_platform_driver_probe(gpio_afc_driver, gpio_afc_probe);

MODULE_DESCRIPTION("Samsung GPIO AFC driver");
MODULE_LICENSE("GPL");

