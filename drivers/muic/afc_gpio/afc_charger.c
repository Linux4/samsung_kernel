// SPDX-License-Identifier: GPL-2.0
/* afc_charger.c
 *
 * Copyright (C) 2022 Samsung Electronics
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
#include <linux/ktime.h>
#include <linux/pinctrl/consumer.h>

#if IS_ENABLED(CONFIG_VIRTUAL_MUIC)
#include <linux/muic/common/vt_muic/vt_muic.h>
#endif /* CONFIG_VIRTUAL_MUIC */
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
	afc_udelay(MPING);
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
	if (delay < (MPING-3*UI))
		afc_udelay(MPING-3*UI-delay);

	if (!gpio_get_value(gpio)) {
		ret = -EAGAIN;
		goto out;
	}

	start = ktime_to_us(ktime_get());
	while (gpio_get_value(gpio)) {
		duration = ktime_to_us(ktime_get()) - start;
		if (duration > DATA_DELAY)
			break;
		afc_udelay(UI);
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

	afc_udelay(UI);

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
	afc_udelay(MPING);
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

		afc_udelay(DATA_DELAY-delay);

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
			afc_udelay(UI);
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
	afc_udelay(RESET_DELAY);
	gpio_set_value(gpio, 0);
}

void set_afc_voltage_for_performance(bool enable)
{
	struct gpio_afc_ddata *ddata = g_ddata;

	ddata->check_performance = enable;
}

static int gpio_afc_get_vbus(void)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_CHARGER_SYV660)
	union power_supply_propval value = {0, };
	struct power_supply *psy;
	psy = get_power_supply_by_name("bc12");

	if (!psy)
		return -ENODEV;

	if ((psy->desc->get_property != NULL) &&
			(psy->desc->get_property(psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &value) >= 0)) {
		ret = value.intval;
		pr_info("gpio_afc_get_vbus: ret=%d\n", ret);
	}
#else
	/* to do, need to make common function */
	//ret = battery_get_vbus();
#endif
	return ret;
}

static int vbus_level_check(void)
{
	int vbus = 0;
#if IS_ENABLED(CONFIG_CHARGER_SYV660)
	struct gpio_afc_ddata *ddata = g_ddata;

	if (ddata->rp_currentlvl == RP_CURRENT_LEVEL2
			&& ddata->dpdm_ctrl_on) {
		pr_info("%s rp22k\n", __func__);
		vbus = 0x9;
		return vbus;
	}
#endif

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
static void afc_dpdm_ctrl(bool onoff)
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

static bool is_vbus_disconnected(struct gpio_afc_ddata *data)
{
	int ret = false;
#if IS_ENABLED(CONFIG_CHARGER_SYV660)
	struct gpio_afc_ddata *ddata = data;

	if (!ddata->dpdm_ctrl_on)
#else
	if(!vbus_level_check())
#endif
		ret = true;

	return ret;
}

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

	if (is_vbus_disconnected(ddata)) {
		pr_err("%s disconnected\n", __func__);
		return;
	}

#if IS_ENABLED(CONFIG_VIRTUAL_MUIC)
	vt_muic_set_attached_afc_dev(ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC);
#endif /* CONFIG_VIRTUAL_MUIC */

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

			if (is_vbus_disconnected(ddata)) {
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

		if (is_vbus_disconnected(ddata)) {
			pr_err("%s retry ta disconnected\n", __func__);
			goto err;
		}

		msleep(100);
		pr_info("%s vbus recheck cnt=%d\n", __func__, j + 1);
	}
#if IS_ENABLED(CONFIG_VIRTUAL_MUIC)
	if (set_vol == 0x9) {
		if (!ret)
			vt_muic_set_attached_afc_dev(ATTACHED_DEV_AFC_CHARGER_9V_MUIC);
		else
			vt_muic_set_attached_afc_dev(ATTACHED_DEV_TA_MUIC);
	} else if (set_vol == 0x5) {
		if (!ret) {
			if (vt_muic_get_afc_disable())
				vt_muic_set_attached_afc_dev(ATTACHED_DEV_TA_MUIC);
			else
				vt_muic_set_attached_afc_dev(ATTACHED_DEV_AFC_CHARGER_5V_MUIC);
		} else
			vt_muic_set_attached_afc_dev(ATTACHED_DEV_AFC_CHARGER_9V_DUPLI_MUIC);
	}
#endif /* CONFIG_VIRTUAL_MUIC */
err:
	gpio_set_value(ddata->pdata->gpio_afc_switch, 0);
	__pm_relax(&ddata->ws);
	mutex_unlock(&ddata->mutex);
}

#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER) && IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
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

static int afc_charger_set_voltage(int voltage)
{
	struct gpio_afc_ddata *ddata = g_ddata;
	int vbus = 0, cur = 0;

	if (!ddata) {
		pr_err("driver is not ready\n");
		return -EAGAIN;
	}

#if defined(CONFIG_DRV_SAMSUNG)
	if (voltage == 0x9 && vt_muic_get_afc_disable()) {
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

static int afc_charger_get_adc(void *mdata)
{
	int result = 0;

#if IS_ENABLED(CONFIG_TCPC_MT6360)
	result = mt6360_usbid_check();
#endif

	pr_info("%s %d\n", __func__, result);

	return result;
}

static int afc_charger_get_vbus_value(void *mdata)
{
	struct gpio_afc_ddata *ddata = mdata;
	int retval = -ENODEV;

	if (!ddata) {
		printk("%s : driver data is null\n", __func__);
		return retval;
	}

	if (ddata->curr_voltage) {
		pr_info("%s %dV\n", __func__, ddata->curr_voltage);
		return ddata->curr_voltage;
	}

	pr_info("%s No value", __func__);
	return 0;
}

static void afc_charger_set_afc_disable(void *mdata)
{
	struct gpio_afc_ddata *ddata = mdata;
#if IS_ENABLED(CONFIG_VIRTUAL_MUIC)
	int attached_dev = vt_muic_get_attached_dev();
	int vbus = 0, curr = 0;
#endif /* CONFIG_VIRTUAL_MUIC */

	pr_info("%s afc_disable(%d)\n", __func__, vt_muic_get_afc_disable());

#if IS_ENABLED(CONFIG_VIRTUAL_MUIC)
	switch (attached_dev) {
	case ATTACHED_DEV_TA_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC ... ATTACHED_DEV_AFC_CHARGER_DISABLED_MUIC:
		kthread_flush_work(&ddata->kwork);

		curr = vbus_level_check();
		vbus = vt_muic_get_afc_disable() ? 5 : 9;

		if (curr == vbus) {
			if (vt_muic_get_afc_disable() && attached_dev != ATTACHED_DEV_TA_MUIC)
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
#endif /* CONFIG_VIRTUAL_MUIC */
}

#if IS_ENABLED(CONFIG_SEC_HICCUP)
static void afc_charger_set_hiccup_mode(void *mdata, bool en)
{
	struct gpio_afc_ddata *ddata = mdata;
	struct gpio_afc_pdata *pdata = ddata->pdata;
	int ret = 0;

	ret = get_boot_mode();
	if (ret == KERNEL_POWER_OFF_CHARGING_BOOT) {
		pr_info("%s %d, lpm mode\n", __func__, en, ret);
		return;
	}

	gpio_set_value(pdata->gpio_hiccup, en);

	ret = gpio_get_value(pdata->gpio_hiccup);

	pr_info("%s %d, %d\n", __func__, en, ret);
}
#endif /* CONFIG_SEC_HICCUP */

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
	pdata->gpio_hiccup = of_get_named_gpio(np, "gpio_hiccup_en", 0);
	pr_info("gpio_hiccup_en : %d\n", pdata->gpio_hiccup);
#endif /* CONFIG_SEC_HICCUP */

	return pdata;
}

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
static void set_gpio_afc_disable_work(struct work_struct *work)
{
	union power_supply_propval psy_val;
	int ret = 0;

	psy_val.intval = vt_muic_get_afc_disable() ? '1' : '0';
	ret = psy_do_property("battery", set, POWER_SUPPLY_EXT_PROP_HV_DISABLE, psy_val);
	pr_info("set_gpio_afc_disable_work: ret=%d\n", ret);
}
#endif

static void register_muic_ops(struct vt_muic_ic_data *ic_data)
{
#if IS_ENABLED(CONFIG_CHARGER_SYV660)
	ic_data->m_ops.afc_dpdm_ctrl = afc_dpdm_ctrl;
#endif
	ic_data->m_ops.afc_set_voltage = afc_charger_set_voltage;
	ic_data->m_ops.get_adc = afc_charger_get_adc;
	ic_data->m_ops.get_vbus_value = afc_charger_get_vbus_value;
	ic_data->m_ops.set_afc_disable = afc_charger_set_afc_disable;
#if IS_ENABLED(CONFIG_HICCUP_CHARGER)
	ic_data->m_ops.set_hiccup_mode = afc_charger_set_hiccup_mode;
#endif
}

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

	ddata->afc_ic_data.mdata = ddata;
	register_muic_ops(&ddata->afc_ic_data);
	vt_muic_register_ic_data(&ddata->afc_ic_data);

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	INIT_DELAYED_WORK(&ddata->set_gpio_afc_disable,
			  set_gpio_afc_disable_work);
	schedule_delayed_work(&ddata->set_gpio_afc_disable,
		msecs_to_jiffies(1000));
#endif

	gpio_direction_output(ddata->pdata->gpio_afc_switch, 0);
	gpio_direction_output(ddata->pdata->gpio_afc_data, 0);
#if IS_ENABLED(CONFIG_CHARGER_SYV660)
	gpio_direction_output(ddata->pdata->gpio_afc_hvdcp, 0);
	afc_dpdm_ctrl(0);
#endif
#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER) && IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	ddata->rp_currentlvl = RP_CURRENT_LEVEL_NONE;
	manager_notifier_register(&ddata->typec_nb,
		gpio_afc_tcm_handle_notification, MANAGER_NOTIFY_PDIC_BATTERY);
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
