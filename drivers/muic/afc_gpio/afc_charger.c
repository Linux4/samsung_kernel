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
#include <linux/regmap.h>
#if defined(CONFIG_DRV_SAMSUNG)
#include <linux/sec_class.h>
#endif
#if defined(CONFIG_SEC_PARAM)
#include <linux/sec_ext.h>
#endif
#include <linux/ktime.h>
#include <linux/pinctrl/consumer.h>
#include <linux/muic/afc_gpio/gpio_afc_charger.h>

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
#include <dt-bindings/battery/sec-battery.h>
#include <linux/power_supply.h>
#include <../drivers/battery/common/sec_charging_common.h>
#include <../drivers/battery/common/sec_battery.h>
#endif

#if IS_ENABLED(CONFIG_CHARGER_SYV660)
#include <../drivers/battery/charger/syv660_charger/syv660_charger.h>
#endif

#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
#include <linux/usb/typec/manager/usb_typec_manager_notifier.h>
#endif

struct gpio_afc_ddata *g_ddata;

#if IS_ENABLED(CONFIG_SEC_HICCUP)
static int gpio_hiccup;
#endif

/* afc_mode:
 *   0x31 : Disabled
 *   0x30 : Enabled
 */
static int afc_mode = 0;

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
#if 0
	afc_udelay(UI >> 2);
#endif
}

static void gpio_afc_sync_pulse(int gpio)
{
	gpio_set_value(gpio, 1);
	afc_udelay(SYNC_PULSE);
	gpio_set_value(gpio, 0);
	afc_udelay(SYNC_PULSE);
}

static void gpio_afc_send_mping(struct gpio_afc_ddata *ddata)
{
	int gpio = ddata->pdata->gpio_afc_data;

	if (ddata->gpio_input) {
		ddata->gpio_input = false;
		gpio_direction_output(ddata->pdata->gpio_afc_data, 0);
	}

	/* send mping */
	gpio_set_value(gpio, 1);
	afc_udelay(MPING);
	gpio_set_value(gpio, 0);
}

static int gpio_afc_check_sping(struct gpio_afc_ddata *ddata)
{
	int gpio = ddata->pdata->gpio_afc_data;
	int ret = 0, i = 0;
	s64 start = 0, duration = 0;

	if (!ddata->gpio_input) {
		ddata->gpio_input = true;
		gpio_direction_input(gpio);
	}

	for (i = 0; i < AFC_SPING_CNT; i++) {
		udelay(UI);
		if (!!gpio_get_value(gpio))
			break;
	}

	if (i == AFC_SPING_CNT) {
		ret = -EAGAIN;
		goto out;
	}

	start = ktime_to_us(ktime_get());

	for (i = 0; i < AFC_SPING_MAX; i++) {
		udelay(UI);
		if (!gpio_get_value(gpio))
			break;
	}

	duration = ktime_to_us(ktime_get()) - start;

	if (duration < (MPING - 500)) {
		ret = -EAGAIN;
	} else if (duration > (MPING + 500)) {
		if (i == AFC_SPING_MAX)
			ret = -EAGAIN;
	}
out:
	return ret;
}

static void gpio_afc_send_data(struct gpio_afc_ddata *ddata, int data)
{
	int gpio = ddata->pdata->gpio_afc_data;
	int i = 0;
	unsigned long flags;

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

	spin_unlock_irqrestore(&ddata->spin_lock, flags);

	/* end of transfer */
	gpio_afc_send_mping(ddata);
}

static int gpio_afc_recv_data(struct gpio_afc_ddata *ddata)
{
#if 1
	int ret = 0, retry = 0;

	/* wating for charger data */
	udelay(DATA_DELAY);

	for (retry = 0; retry < AFC_RETRY_CNT; retry++) {
		if (gpio_afc_check_sping(ddata))
			pr_err("%s cnt=%d\n", __func__, retry + 1);
		else
			break;
	}

	udelay(UI);
	return ret;
#else
	 bool ack = false;
	 int gpio = ddata->pdata->gpio_afc_data;
	 int i = 0, retry = 0;

	 while (retry++ < AFC_SPING_MAX && !ack) {
		for (i = 0; i < 50; i++) {
			udelay(UI);
			if (!!gpio_get_value(gpio))
				break;
		}

		if (i == 50) {
			pr_err("%s timeout\n", __func__);
			continue;
		}

		for (i = 0; i < AFC_SPING_MAX; i++) {
			udelay(UI << 1);
			if (!gpio_get_value(gpio))
				break;
		}

		if (i >= AFC_SPING_MIN && i < AFC_SPING_MAX)
			ack = true;
		else
			pr_err("%s cnt %d retry %d\n", __func__, i, retry);
	}
#endif

	return 0;
}

static int gpio_afc_set_voltage(struct gpio_afc_ddata *ddata, u32 voltage)
{
	int ret = 0;

	gpio_afc_send_mping(ddata);
	ret = gpio_afc_check_sping(ddata);
	if (ret < 0) {
		pr_err("Start Mping NACK\n");
		goto out;
	}

	if (voltage == 0x9)
		gpio_afc_send_data(ddata, 0x46);
	else
		gpio_afc_send_data(ddata, 0x08);

	ret = gpio_afc_check_sping(ddata);
	if (ret < 0) {
		pr_err("sping err2 %d\n", ret);
		goto out;
	}

	ret = gpio_afc_recv_data(ddata);
	if (ret < 0)
		pr_err("sping err3 %d\n", ret);

	gpio_afc_send_mping(ddata);
	ret = gpio_afc_check_sping(ddata);
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

static int gpio_afc_get_vbus(void)
{
	union power_supply_propval value = {0, };
	struct power_supply *psy;
	int ret = 0;
	psy = get_power_supply_by_name("bc12");

	if (!psy)
		return -ENODEV;

	if ((psy->desc->get_property != NULL) &&
			(psy->desc->get_property(psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &value) >= 0)) {
		ret = value.intval;
		pr_info("gpio_afc_get_vbus: ret=%d\n", ret);
	}
	return ret;
}

static int vbus_level_check(void)
{
	struct gpio_afc_ddata *ddata = g_ddata;

	int vbus = 0;

	if (ddata->rp_currentlvl == RP_CURRENT_LEVEL2
			&& ddata->dpdm_ctrl_on) {
		pr_info("%s rp22k\n", __func__);
		vbus = 0x9;
		return vbus;
	}

	vbus = gpio_afc_get_vbus();

	pr_info("%s %dmV \n", __func__, vbus);

	if (vbus > 3800 && vbus <= 7500)
		vbus = 0x5;
	else if (vbus > 7500 && vbus < 10500)
		vbus = 0x9;
	else
		vbus = 0x0;

	return vbus;
}

#if IS_ENABLED(CONFIG_CHARGER_SYV660)
void afc_dpdm_ctrl(bool onoff)
{
	struct gpio_afc_ddata *ddata = g_ddata;
	struct gpio_afc_pdata *pdata = ddata->pdata;
	int ret = 0;

	ddata->dpdm_ctrl_on = onoff;
	gpio_set_value(pdata->gpio_afc_hvdcp, onoff);
	ret = gpio_get_value(pdata->gpio_afc_hvdcp);

	if (!onoff)
		ddata->curr_voltage = 0;

	pr_info("%s: onoff=%d, pdata->gpio_afc_hvdcp=%d ret=%d\n",
				__func__, onoff, pdata->gpio_afc_hvdcp, ret);
}
#endif

static void gpio_afc_kwork(struct kthread_work *work)
{
	struct gpio_afc_ddata *ddata =
	    container_of(work, struct gpio_afc_ddata, kwork);

	int ret = -EAGAIN, i = 0, j = 0;
	int set_vol = 0, cur_vol = 0;
	int retry = 0;

	if (!ddata) {
		pr_err("driver is not ready\n");
		return;
	}

	set_vol = ddata->set_voltage;
	pr_info("%s set vol[%dV]\n", __func__, set_vol);

	if (set_vol == 0x9) {
		msleep(1200);
	} else {
		gpio_afc_reset(ddata);
		msleep(200);
	}

	if (!ddata->dpdm_ctrl_on) {
		pr_err("%s disconnected\n", __func__);
		return;
	}

#if IS_ENABLED(CONFIG_MUIC_NOTIFIER)
	vt_muic_set_attached_afc_dev(ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC);
#endif /* CONFIG_MUIC_NOTIFIER */

	mutex_lock(&ddata->mutex);
	__pm_stay_awake(&ddata->ws);

	gpio_set_value(ddata->pdata->gpio_afc_switch, 1);

	if (set_vol) {
		for (retry = 0; retry < AFC_RETRY_CNT; retry++) {
			if (ret < 0) {			
				for (i = 0 ; i < AFC_OP_CNT ; i++) {
					gpio_afc_set_voltage(ddata, set_vol);
					msleep(20);
					cur_vol = vbus_level_check();
					ddata->curr_voltage = cur_vol;
					pr_info("%s current %dV\n", __func__, cur_vol);
					if (cur_vol == set_vol) {
						ret = 0;
						break;
					}
				}
			}
			msleep(50);
			cur_vol = vbus_level_check();
			ddata->curr_voltage = cur_vol;
			pr_info("%s current %dV\n", __func__, cur_vol);
			if (cur_vol == set_vol) {
				ret = 0;
				pr_info("%s success\n", __func__);
			} else
				ret = -EAGAIN;

			if (!ddata->dpdm_ctrl_on) {
				pr_err("%s ta disconnected\n", __func__);
				goto err;
			}
			pr_err("%s retry %d\n", __func__, retry + 1);
		}
	} else {
		ret = 0;
		goto err;
	}

	for (j = 0 ; j < VBUS_RETRY_MAX ; j++) {
		cur_vol = vbus_level_check();
		ddata->curr_voltage = cur_vol;
		if (cur_vol == set_vol) {
			ret = 0;
			pr_info("%s vbus retry success\n", __func__);
			break;
		} else 
			ret = -EAGAIN;

		if (!ddata->dpdm_ctrl_on) {
			pr_err("%s retry ta disconnected\n", __func__);
			goto err;
		}

		msleep(100);
		pr_info("%s vbus recheck cnt=%d\n", __func__, j + 1);
	}
#if IS_ENABLED(CONFIG_MUIC_NOTIFIER)
	if (set_vol == 0x9) {
		if (!ret)
			vt_muic_set_attached_afc_dev(ATTACHED_DEV_AFC_CHARGER_9V_MUIC);
		else
			vt_muic_set_attached_afc_dev(ATTACHED_DEV_TA_MUIC);
	} else if (set_vol == 0x5) {
		if (!ret) {
			if (ddata->afc_disable)
				vt_muic_set_attached_afc_dev(ATTACHED_DEV_TA_MUIC);
			else
				vt_muic_set_attached_afc_dev(ATTACHED_DEV_AFC_CHARGER_5V_MUIC);
		} else
			vt_muic_set_attached_afc_dev(ATTACHED_DEV_AFC_CHARGER_9V_DUPLI_MUIC);
	}
#endif /* CONFIG_MUIC_NOTIFIER */
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
		return - EINVAL;
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

#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
static int gpio_afc_tcm_handle_notification(struct notifier_block *nb,
		unsigned long action, void *data)
{
	struct gpio_afc_ddata *ddata =
			container_of(nb, struct gpio_afc_ddata, typec_nb);

	PD_NOTI_TYPEDEF *pdata = (PD_NOTI_TYPEDEF *)data;
	struct pdic_notifier_struct *pd_noti = pdata->pd;

	if (pdata->dest != PDIC_NOTIFY_DEV_BATT) {
		return 0;
	}

	ddata->rp_currentlvl = RP_CURRENT_LEVEL_NONE;

	if (!pd_noti) {
		pr_info("%s: pd_noti(pdata->pd) is NULL\n", __func__);
	} else {
		ddata->rp_currentlvl = pd_noti->sink_status.rp_currentlvl;
		pr_info("%s rplvl=%d\n", __func__, ddata->rp_currentlvl);
	}

	return 0;
}
#endif

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
#if IS_ENABLED(CONFIG_MUIC_NOTIFIER)
	int attached_dev = vt_muic_get_attached_dev();
	int vbus = 0, curr = 0;
#endif /* CONFIG_MUIC_NOTIFIER */
#if defined(CONFIG_SEC_PARAM)
	int ret = 0;
#endif
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
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

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	psy_val.intval = param_val;
	psy_do_property("battery", set, POWER_SUPPLY_EXT_PROP_HV_DISABLE, psy_val);
#endif
	pr_info("%s afc_disable(%d)\n", __func__, ddata->afc_disable);

#if IS_ENABLED(CONFIG_MUIC_NOTIFIER)
	switch (attached_dev) {
	case ATTACHED_DEV_TA_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC ... ATTACHED_DEV_AFC_CHARGER_DISABLED_MUIC:
		kthread_flush_work(&ddata->kwork);

		curr = vbus_level_check();
		vbus = ddata->afc_disable ? 5 : 9;

		if (curr == vbus) {
			if (ddata->afc_disable && attached_dev != ATTACHED_DEV_TA_MUIC)
				vt_muic_set_attached_afc_dev(ATTACHED_DEV_TA_MUIC);
		} else {
			ddata->curr_voltage = curr;
			ddata->set_voltage = vbus;
			kthread_queue_work(&ddata->kworker, &ddata->kwork);
		}
		break;
	default:
		break;
	}
#endif /* CONFIG_MUIC_NOTIFIER */

	return count;
}

static ssize_t vbus_value_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct gpio_afc_ddata *ddata = dev_get_drvdata(dev);
	int retval = -ENODEV;

	if (!ddata) {
		printk("%s : driver data is null\n", __func__);
		return retval;
	}

	if (ddata->curr_voltage) {
		pr_info("%s %dV\n", __func__, ddata->curr_voltage);
		return sprintf(buf, "%d\n", ddata->curr_voltage);
	}

	pr_info("%s No value", __func__);
	return sprintf(buf, "0\n");
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

#if IS_ENABLED(CONFIG_TCPC_MT6360)
	result = mt6360_usbid_check();
#endif

	pr_info("%s %d\n", __func__, result);

	return sprintf(buf, "%d\n", !!result);
}

static DEVICE_ATTR_RO(adc);
static DEVICE_ATTR_RW(afc_disable);
static DEVICE_ATTR_RO(vbus_value);
#if IS_ENABLED(CONFIG_SEC_HICCUP)
static DEVICE_ATTR_RW(hiccup);
#endif /* CONFIG_SEC_HICCUP */

static struct attribute *gpio_afc_attributes[] = {
	&dev_attr_adc.attr,
	&dev_attr_afc_disable.attr,
	&dev_attr_vbus_value.attr,
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
	struct device_node *np= dev->of_node;
	struct gpio_afc_pdata *pdata;

	if (!np)
		return ERR_PTR(-ENODEV);

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	pdata->gpio_afc_switch = of_get_named_gpio(np, "gpio_afc_switch", 0);
	pdata->gpio_afc_data = of_get_named_gpio(np, "gpio_afc_data", 0);
	pdata->gpio_afc_hvdcp = of_get_named_gpio(np, "gpio_afc_hvdcp", 0);

	pr_info("gpio_afc_switch %d, gpio_afc_data %d, gpio_afc_hvdcp %d\n",
		pdata->gpio_afc_switch, pdata->gpio_afc_data, pdata->gpio_afc_hvdcp);

#if IS_ENABLED(CONFIG_SEC_HICCUP)
	gpio_hiccup = of_get_named_gpio(np, "gpio_hiccup_en", 0);
	pr_info("gpio_hiccup_en : %d\n", gpio_hiccup);
#endif /* CONFIG_SEC_HICCUP */

	return pdata;
}

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
static void set_gpio_afc_disable_work(struct work_struct *work)
{
	union power_supply_propval psy_val;
	struct gpio_afc_ddata *ddata = g_ddata;
	int ret = 0;

	psy_val.intval = ddata->afc_disable ? '1' : '0';
	ret = psy_do_property("battery", set, POWER_SUPPLY_EXT_PROP_HV_DISABLE, psy_val);
	pr_info("set_gpio_afc_disable_work: ret=%d\n", ret);
}
#endif

static int gpio_afc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct gpio_afc_pdata *pdata = dev_get_platdata(dev);
	struct gpio_afc_ddata *ddata;
	struct task_struct *kworker_task;

	int ret = 0;

	pr_info("%s\n", __func__);

	if (!pdata)
		pdata = gpio_afc_get_dt(dev);

	ddata = devm_kzalloc(dev, sizeof(*ddata), GFP_KERNEL);
	if (!ddata)
		return -ENOMEM;

	ret = gpio_request(pdata->gpio_afc_switch, "gpio_afc_switch");
	if (ret < 0) {
		pr_err("%s: failed to request afc switch gpio\n", __func__);
		return ret;
	}

	ret = gpio_request(pdata->gpio_afc_data, "gpio_afc_data");
	if (ret < 0) {
		pr_err("%s: failed to request afc data gpio\n", __func__);
		return ret;
	}

#if IS_ENABLED(CONFIG_CHARGER_SYV660)
	ret = gpio_request(pdata->gpio_afc_hvdcp, "gpio_afc_hvdcp");
	if (ret < 0) {
		pr_err("%s: failed to request afc hvdcp gpio\n", __func__);
		return ret;
	}
#endif
#if IS_ENABLED(CONFIG_SEC_HICCUP)
	ret = gpio_request(gpio_hiccup, "hiccup_en");
	if (ret < 0) {
		pr_err("%s: failed to request hiccup gpio\n", __func__);
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
	if (IS_ERR(kworker_task)) {
		pr_err("Failed to create message pump task\n");
		ret = -ENOMEM;
	}
	kthread_init_work(&ddata->kwork, gpio_afc_kwork);

	ddata->pdata = pdata;
	ddata->gpio_input = false;

	g_ddata = ddata;

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	INIT_DELAYED_WORK(&ddata->set_gpio_afc_disable,
			  set_gpio_afc_disable_work);
	schedule_delayed_work(&ddata->set_gpio_afc_disable,
		msecs_to_jiffies(1000));
#endif

#if defined(CONFIG_DRV_SAMSUNG)
	ddata->afc_disable = (_get_afc_mode() == '1' ? 1 : 0);
	ddata->dev = sec_device_find("switch");
	if (IS_ERR_OR_NULL(ddata->dev)) {
		pr_info("failed to find switch device\n");

		ddata->dev = sec_device_create(ddata, "switch");
		if (IS_ERR(ddata->dev)) {
			pr_err("failed to create switch device\n");
			return -ENODEV;
		}
	}
	dev_set_drvdata(ddata->dev, ddata);
	ret = sysfs_create_group(&ddata->dev->kobj, &gpio_afc_group);
	if (ret) {
		pr_err("failed to create afc_disable sysfs\n");
		return ret;
	}
#endif
	gpio_direction_output(ddata->pdata->gpio_afc_switch, 0);
	gpio_direction_output(ddata->pdata->gpio_afc_data, 0);
#if IS_ENABLED(CONFIG_CHARGER_SYV660)
	gpio_direction_output(ddata->pdata->gpio_afc_hvdcp, 0);
	afc_dpdm_ctrl(0);
#endif
#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	ddata->rp_currentlvl = RP_CURRENT_LEVEL_NONE;
	manager_notifier_register(&ddata->typec_nb,
		gpio_afc_tcm_handle_notification, MANAGER_NOTIFY_PDIC_BATTERY);
#endif
#if IS_ENABLED(CONFIG_MUIC_NOTIFIER)
	ddata->muic_pdata = &muic_pdata;
	ddata->muic_pdata->muic_afc_set_voltage_cb = set_afc_voltage;
#endif
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
