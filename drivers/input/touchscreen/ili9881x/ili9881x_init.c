/*
 * ILITEK Touch IC driver
 *
 * Copyright (C) 2011 ILI Technology Corporation.
 *
 * Author: Dicky Chiang <dicky_chiang@ilitek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "ili9881x.h"

void ili_tp_reset(void)
{
	input_info(true, ilits->dev, "%s edge delay = %d\n", __func__, ilits->rst_edge_delay);

	/* Need accurate power sequence, do not change it to msleep */
	gpio_direction_output(ilits->tp_rst, 1);
	usleep_range(1 * 1000, 1 * 1000);
	gpio_set_value(ilits->tp_rst, 0);
	usleep_range(5 * 1000, 5 * 1000);
	gpio_set_value(ilits->tp_rst, 1);
	msleep(ilits->rst_edge_delay);
}

static void touch_set_input_prop_proximity(struct input_dev *dev)
{
	dev->phys = ilits->phys;
	dev->dev.parent = ilits->dev;

	set_bit(EV_SYN, dev->evbit);
	set_bit(EV_SW, dev->evbit);
	set_bit(INPUT_PROP_DIRECT, dev->propbit);

	input_set_abs_params(dev, ABS_MT_CUSTOM, 0, 0xFFFFFFFF, 0, 0);
	input_set_drvdata(dev, ilits);
}

void ili_input_register(void)
{
	int ret = 0;

	input_info(true, ilits->dev, "%s\n", __func__);

	ilits->input = input_allocate_device();
	if (ERR_ALLOC_MEM(ilits->input)) {
		input_free_device(ilits->input);
		return;
	}

	if (ilits->support_ear_detect) {
		ilits->input_dev_proximity = input_allocate_device();
		if (ilits->input_dev_proximity == NULL) {
			input_err(true, ilits->dev, "%s: allocate input_dev_proximity err!\n", __func__);
			if (ilits->input) {
				input_free_device(ilits->input);
				ilits->input = NULL;
				return;
			}
		}

		ilits->input_dev_proximity->name = "sec_touchproximity";
		touch_set_input_prop_proximity(ilits->input_dev_proximity);
	}

	ilits->input->name = "sec_touchscreen";
	ilits->input->phys = ilits->phys;
	ilits->input->dev.parent = ilits->dev;
	ilits->input->id.bustype = ilits->hwif->bus_type;

	/* set the supported event type for input device */
	set_bit(EV_ABS, ilits->input->evbit);
	set_bit(EV_SYN, ilits->input->evbit);
	set_bit(EV_KEY, ilits->input->evbit);
	set_bit(BTN_TOUCH, ilits->input->keybit);
	set_bit(BTN_TOOL_FINGER, ilits->input->keybit);
	set_bit(INPUT_PROP_DIRECT, ilits->input->propbit);
	set_bit(KEY_INT_CANCEL, ilits->input->keybit);

	if (ilits->enable_settings_aot)
		set_bit(KEY_WAKEUP, ilits->input->keybit);
	if (ilits->support_spay_gesture_mode)
		set_bit(KEY_BLACK_UI_GESTURE, ilits->input->keybit);

	input_set_abs_params(ilits->input, ABS_MT_POSITION_X, TOUCH_SCREEN_X_MIN, ilits->panel_wid - 1, 0, 0);
	input_set_abs_params(ilits->input, ABS_MT_POSITION_Y, TOUCH_SCREEN_Y_MIN, ilits->panel_hei - 1, 0, 0);
	input_set_abs_params(ilits->input, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(ilits->input, ABS_MT_WIDTH_MAJOR, 0, 255, 0, 0);

	if (MT_PRESSURE)
		input_set_abs_params(ilits->input, ABS_MT_PRESSURE, 0, 255, 0, 0);

	if (MT_B_TYPE) {
#if KERNEL_VERSION(3, 7, 0) <= LINUX_VERSION_CODE
		input_mt_init_slots(ilits->input, MAX_TOUCH_NUM, INPUT_MT_DIRECT);
#else
		input_mt_init_slots(ilits->input, MAX_TOUCH_NUM);
#endif /* LINUX_VERSION_CODE */
	} else {
		input_set_abs_params(ilits->input, ABS_MT_TRACKING_ID, 0, MAX_TOUCH_NUM, 0, 0);
	}

	/* register the input device to input sub-system */
	if (input_register_device(ilits->input) < 0) {
		input_err(true, ilits->dev, "%s Failed to register touch input device\n", __func__);
		input_unregister_device(ilits->input);
		input_free_device(ilits->input);
		if (ilits->support_ear_detect) {
			if (ilits->input_dev_proximity)
				input_free_device(ilits->input_dev_proximity);
		}
	}

	if (ilits->support_ear_detect) {
		ret = input_register_device(ilits->input_dev_proximity);
		if (ret < 0) {
			input_err(true, ilits->dev, "%s: Unable to register %s input device\n",
						__func__, ilits->input_dev_proximity->name);

			input_free_device(ilits->input);
			if (ilits->support_ear_detect) {
				if (ilits->input_dev_proximity)
					input_free_device(ilits->input_dev_proximity);
			}
			input_unregister_device(ilits->input);
			ilits->input = NULL;
		}
	}
}

#if REGULATOR_POWER
void ili_plat_regulator_power_on(bool status)
{
	input_info(true, ilits->dev, "%s %s\n", __func__, status ? "POWER ON" : "POWER OFF");

	if (status) {
		if (ilits->vdd) {
			if (regulator_enable(ilits->vdd) < 0)
				input_err(true, ilits->dev, "%s regulator_enable VDD fail\n", __func__);
		}
		if (ilits->vcc) {
			if (regulator_enable(ilits->vcc) < 0)
				input_err(true, ilits->dev, "%s regulator_enable VCC fail\n", __func__);
		}
	} else {
		if (ilits->vdd) {
			if (regulator_disable(ilits->vdd) < 0)
				input_err(true, ilits->dev, "%s regulator_enable VDD fail\n", __func__);
		}
		if (ilits->vcc) {
			if (regulator_disable(ilits->vcc) < 0)
				input_err(true, ilits->dev, "%s regulator_enable VCC fail\n", __func__);
		}
	}
	atomic_set(&ilits->ice_stat, DISABLE);
	usleep_range(5 * 1000, 5 * 1000);
}

static void ilitek_plat_regulator_power_init(void)
{
	const char *vdd_name = "vdd";
	const char *vcc_name = "vcc";

	ilits->vdd = regulator_get(ilits->dev, vdd_name);
	if (ERR_ALLOC_MEM(ilits->vdd)) {
		input_err(true, ilits->dev, "%s regulator_get VDD fail\n", __func__);
		ilits->vdd = NULL;
	}
	if (regulator_set_voltage(ilits->vdd, VDD_VOLTAGE, VDD_VOLTAGE) < 0)
		input_err(true, ilits->dev, "%s Failed to set VDD %d\n", __func__, VDD_VOLTAGE);

	ilits->vcc = regulator_get(ilits->dev, vcc_name);
	if (ERR_ALLOC_MEM(ilits->vcc)) {
		input_err(true, ilits->dev, "%s regulator_get VCC fail.\n", __func__);
		ilits->vcc = NULL;
	}
	if (regulator_set_voltage(ilits->vcc, VCC_VOLTAGE, VCC_VOLTAGE) < 0)
		input_err(true, ilits->dev, "%s Failed to set VCC %d\n", __func__, VCC_VOLTAGE);

	ili_plat_regulator_power_on(true);
}
#endif

static int ilitek_plat_gpio_register(void)
{
	int ret = 0;
	int val, val2;

	input_info(true, ilits->dev, "%s TP INT: %d\n", __func__, ilits->tp_int);
	input_info(true, ilits->dev, "%s TP RESET: %d\n", __func__, ilits->tp_rst);

	if (!gpio_is_valid(ilits->tp_int)) {
		input_err(true, ilits->dev, "%s Invalid INT gpio: %d\n", __func__, ilits->tp_int);
		return -EBADR;
	}

	if (!gpio_is_valid(ilits->tp_rst)) {
		input_err(true, ilits->dev, "%s Invalid RESET gpio: %d\n", __func__, ilits->tp_rst);
		return -EBADR;
	}

	ret = gpio_request(ilits->tp_int, "TP_INT");
	if (ret < 0) {
		input_err(true, ilits->dev, "%s Request IRQ GPIO failed, ret = %d\n", __func__, ret);
		gpio_free(ilits->tp_int);
		ret = gpio_request(ilits->tp_int, "TP_INT");
		if (ret < 0) {
			input_err(true, ilits->dev, "%s Retrying request INT GPIO still failed , ret = %d\n",
				__func__, ret);
			goto out;
		}
	}

	ret = gpio_request(ilits->tp_rst, "TP_RESET");
	if (ret < 0) {
		input_err(true, ilits->dev, "%s Request RESET GPIO failed, ret = %d\n", __func__, ret);
		gpio_free(ilits->tp_rst);
		ret = gpio_request(ilits->tp_rst, "TP_RESET");
		if (ret < 0) {
			input_err(true, ilits->dev, "%s Retrying request RESET GPIO still failed , ret = %d\n",
				__func__, ret);
			goto out;
		}
	}

	if (ilits->cs_gpio > 0) {
		ret = gpio_request(ilits->cs_gpio, "TP_CS");
		if (ret < 0) {
			input_err(true, ilits->dev, "%s Request TP_CS GPIO failed, ret = %d\n", __func__, ret);
			gpio_free(ilits->cs_gpio);
			ret = gpio_request(ilits->cs_gpio, "TP_CS");
			if (ret < 0) {
				input_err(true, ilits->dev, "%s Retrying request TP_CS GPIO still failed , ret = %d\n",
					__func__, ret);
				goto out;
			}
		}
	}

out:
	gpio_direction_input(ilits->tp_int);
	gpio_direction_output(ilits->tp_rst, 1);

	gpio_set_value(ilits->tp_rst, 1);
	msleep(ilits->rst_edge_delay);

	val = gpio_get_value(ilits->tp_rst);
	val2 = gpio_get_value(ilits->tp_int);
	input_info(true, ilits->dev, "%s Probe gpio_get_val ilits->tp_rst = %d,ilits->tp_int =%d\n",
		__func__, val, val2);

	if (ilits->cs_gpio > 0) {
		gpio_direction_output(ilits->cs_gpio, 1);
		input_info(true, ilits->dev, "%s Probe gpio_get_val cs_gpio =%d\n",
			__func__, gpio_get_value(ilits->cs_gpio));
	}
	return ret;
}

void ili_irq_disable(void)
{
	unsigned long flag;

	spin_lock_irqsave(&ilits->irq_spin, flag);

	if (atomic_read(&ilits->irq_stat) == DISABLE)
		goto out;

	if (!ilits->irq_num) {
		input_err(true, ilits->dev, "%s gpio_to_irq (%d) is incorrect\n", __func__, ilits->irq_num);
		goto out;
	}

	disable_irq_nosync(ilits->irq_num);
	atomic_set(&ilits->irq_stat, DISABLE);
	ILI_DBG("%s Disable irq success\n", __func__);

out:
	spin_unlock_irqrestore(&ilits->irq_spin, flag);
}

void ili_irq_enable(void)
{
	unsigned long flag;

	spin_lock_irqsave(&ilits->irq_spin, flag);

	if (atomic_read(&ilits->irq_stat) == ENABLE)
		goto out;

	if (!ilits->irq_num) {
		input_err(true, ilits->dev, "%s gpio_to_irq (%d) is incorrect\n", __func__, ilits->irq_num);
		goto out;
	}

	enable_irq(ilits->irq_num);
	atomic_set(&ilits->irq_stat, ENABLE);
	ILI_DBG("%s Enable irq success\n", __func__);

out:
	spin_unlock_irqrestore(&ilits->irq_spin, flag);
}

void ili_irq_wake_disable(void)
{
	if (atomic_read(&ilits->irq_wake_stat) == DISABLE) {
		input_info(true, ilits->dev, "%s already disabled\n", __func__);
		return;
	}

	if (!ilits->irq_num) {
		input_err(true, ilits->dev, "%s gpio_to_irq (%d) is incorrect\n", __func__, ilits->irq_num);
		return;
	}

	disable_irq_wake(ilits->irq_num);
	atomic_set(&ilits->irq_wake_stat, DISABLE);
	ILI_DBG("%s Enable wake_irq_stat success\n", __func__);
}

void ili_irq_wake_enable(void)
{
	if (atomic_read(&ilits->irq_wake_stat) == ENABLE) {
		input_info(true, ilits->dev, "%s already enabled\n", __func__);
		return;
	}

	if (!ilits->irq_num) {
		input_err(true, ilits->dev, "%s gpio_to_irq (%d) is incorrect\n", __func__, ilits->irq_num);
		return;
	}

	enable_irq_wake(ilits->irq_num);
	atomic_set(&ilits->irq_wake_stat, ENABLE);
	ILI_DBG("%s Enable wake_irq_stat success\n", __func__);
}

static irqreturn_t ilitek_plat_isr_top_half(int irq, void *dev_id)
{
	if (irq != ilits->irq_num) {
		input_err(true, ilits->dev, "%s Incorrect irq number (%d)\n", __func__, irq);
		return IRQ_NONE;
	}

	if (atomic_read(&ilits->cmd_int_check) == ENABLE) {
		atomic_set(&ilits->cmd_int_check, DISABLE);
		ILI_DBG("%s CMD INT detected, ignore\n", __func__);
		wake_up(&(ilits->inq));
		return IRQ_HANDLED;
	}

	if (ilits->prox_near) {
		input_info(true, ilits->dev, "%s Proximity event, ignore interrupt!\n", __func__);
		return IRQ_HANDLED;
	}

	ILI_DBG("%s report: %d, rst: %d, fw: %d, switch: %d, mp: %d, sleep: %d, esd: %d\n",
			__func__,
			ilits->report,
			atomic_read(&ilits->tp_reset),
			atomic_read(&ilits->fw_stat),
			atomic_read(&ilits->tp_sw_mode),
			atomic_read(&ilits->mp_stat),
			atomic_read(&ilits->tp_sleep),
			atomic_read(&ilits->esd_stat));

	if (!ilits->report || atomic_read(&ilits->tp_reset) || atomic_read(&ilits->fw_stat) ||
			atomic_read(&ilits->tp_sw_mode) || atomic_read(&ilits->mp_stat) || atomic_read(&ilits->tp_sleep)
			|| atomic_read(&ilits->esd_stat)) {
		ILI_DBG("%s ignore interrupt !\n", __func__);
		return IRQ_HANDLED;
	}

	return IRQ_WAKE_THREAD;
}

static irqreturn_t ilitek_plat_isr_bottom_half(int irq, void *dev_id)
{
	if (mutex_is_locked(&ilits->touch_mutex)) {
		ILI_DBG("%s touch is locked, ignore\n", __func__);
		return IRQ_HANDLED;
	}
	mutex_lock(&ilits->touch_mutex);
	ili_report_handler();
	mutex_unlock(&ilits->touch_mutex);
	return IRQ_HANDLED;
}

void ili_irq_unregister(void)
{
	irq_clear_status_flags(ilits->irq_num, IRQ_DISABLE_UNLAZY);
	devm_free_irq(ilits->dev, ilits->irq_num, NULL);
}

int ili_irq_register(int type)
{
	int ret = 0;
	static bool get_irq_pin;

	atomic_set(&ilits->irq_stat, DISABLE);

	if (get_irq_pin == false) {
		ilits->irq_num  = gpio_to_irq(ilits->tp_int);
		get_irq_pin = true;
	}

	input_info(true, ilits->dev, "%s ilits->irq_num = %d\n", __func__, ilits->irq_num);

	irq_set_status_flags(ilits->irq_num, IRQ_DISABLE_UNLAZY);
	ret = devm_request_threaded_irq(ilits->dev, ilits->irq_num,
				   ilitek_plat_isr_top_half,
				   ilitek_plat_isr_bottom_half,
				   type | IRQF_ONESHOT, "ilitek", NULL);

	if (type == IRQF_TRIGGER_FALLING)
		input_info(true, ilits->dev, "%s IRQ TYPE = IRQF_TRIGGER_FALLING\n", __func__);
	if (type == IRQF_TRIGGER_RISING)
		input_info(true, ilits->dev, "%s IRQ TYPE = IRQF_TRIGGER_RISING\n", __func__);

	if (ret != 0)
		input_err(true, ilits->dev, "%s Failed to register irq handler, irq = %d, ret = %d\n",
			__func__, ilits->irq_num, ret);

	atomic_set(&ilits->irq_stat, ENABLE);

	return ret;
}

#if SPRD_SYSFS_SUSPEND_RESUME
static ssize_t ts_suspend_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", ilits->tp_suspend ? "true" : "false");
}

static ssize_t ts_suspend_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	if ((buf[0] == '1') && !ilits->tp_suspend)
		ili_sleep_handler(TP_DEEP_SLEEP);
	else if ((buf[0] == '0') && ilits->tp_suspend)
		ili_sleep_handler(TP_RESUME);

	return count;
}
static DEVICE_ATTR_RW(ts_suspend);

static struct attribute *ilitek_dev_suspend_atts[] = {
	&dev_attr_ts_suspend.attr,
	NULL
};

static const struct attribute_group ilitek_dev_suspend_atts_group = {
	.attrs = ilitek_dev_suspend_atts,
};

static const struct attribute_group *ilitek_dev_attr_groups[] = {
	&ilitek_dev_suspend_atts_group,
	NULL
};

int ili_sysfs_add_device(struct device *dev)
{
	int ret = 0, i;

	for (i = 0; ilitek_dev_attr_groups[i]; i++) {
		ret = sysfs_create_group(&dev->kobj, ilitek_dev_attr_groups[i]);
		if (ret) {
			while (--i >= 0)
				sysfs_remove_group(&dev->kobj, ilitek_dev_attr_groups[i]);
			break;
		}
	}

	return ret;
}

int ili_sysfs_remove_device(struct device *dev)
{
	int i;

	sysfs_remove_link(NULL, "touchscreen");
	for (i = 0; ilitek_dev_attr_groups[i]; i++)
		sysfs_remove_group(&dev->kobj, ilitek_dev_attr_groups[i]);

	return 0;
}
#endif

#if CHARGER_NOTIFIER_CALLBACK
#if KERNEL_VERSION(4, 1, 0) <= LINUX_VERSION_CODE
/* add_for_charger_start */
static int ilitek_charger_notifier_callback(struct notifier_block *nb, unsigned long val, void *v)
{
	int ret = 0;
	struct power_supply *psy = NULL;
	union power_supply_propval prop;


	if (ilits->fw_update_stat != 100)
		return 0;

	psy = power_supply_get_by_name("usb");
	if (!psy) {
		input_err(true, ilits->dev, "%s Couldn't get usbpsy\n", __func__);
		return -EINVAL;
	}
	if (!strcmp(psy->desc->name, "usb")) {
		if (psy && val == POWER_SUPPLY_PROP_STATUS) {
			ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_PRESENT, &prop);
			if (ret < 0) {
				input_err(true, ilits->dev, "%s Couldn't get POWER_SUPPLY_PROP_ONLINE rc=%d\n",
						__func__, ret);
				return ret;
			}
			if (ilits->usb_plug_status == 2)
				ilits->usb_plug_status = prop.intval;
			if (ilits->usb_plug_status != prop.intval) {
				input_info(true, ilits->dev, "%s usb prop.intval =%d\n", __func__, prop.intval);
				ilits->usb_plug_status = prop.intval;
				if (!ilits->tp_suspend && (ilits->charger_notify_wq != NULL))
					queue_work(ilits->charger_notify_wq, &ilits->update_charger);
			}
		}
	}
	return 0;
}
static void ilitek_update_charger(struct work_struct *work)
{
	int ret = 0;

	mutex_lock(&ilits->touch_mutex);
	ret = ili_ic_func_ctrl("plug", !ilits->usb_plug_status);// plug in
	if (ret < 0)
		input_err(true, ilits->dev, "%s Write plug in failed\n", __func__);

	mutex_unlock(&ilits->touch_mutex);
}
void ilitek_plat_charger_init(void)
{
	int ret = 0;

	ilits->usb_plug_status = 2;
	ilits->charger_notify_wq = create_singlethread_workqueue("ili_charger_wq");
	if (!ilits->charger_notify_wq) {
		input_err(true, ilits->dev, "%s allocate ili_charger_notify_wq failed\n", __func__);
		return;
	}
	INIT_WORK(&ilits->update_charger, ilitek_update_charger);
	ilits->notifier_charger.notifier_call = ilitek_charger_notifier_callback;
	ret = power_supply_reg_notifier(&ilits->notifier_charger);
	if (ret < 0)
		input_err(true, ilits->dev, "%s power_supply_reg_notifier failed\n", __func__);
}
/* add_for_charger_end */
#endif
#endif

static int parse_dt(void)
{
	int retval;
	u32 value;
	struct property *prop;
	struct device_node *np = ilits->dev->of_node;
	u32 px_zone[3] = { 0 };
#if defined(CONFIG_EXYNOS_DPU30)
	int lcdtype = 0;
	int connected = 0;
#endif
	int lcd_id1_gpio = 0, lcd_id2_gpio = 0, lcd_id3_gpio = 0;
	int fw_name_cnt = 0;
	int lcdtype_cnt = 0;
	int fw_sel_idx = 0;

#if defined(CONFIG_EXYNOS_DPU30)
	connected = get_lcd_info("connected");
	if (connected < 0) {
		input_err(true, ilits->dev, "%s: Failed to get lcd info\n", __func__);
		return -EINVAL;
	}

	if (!connected) {
		input_err(true, ilits->dev, "%s: lcd is disconnected\n", __func__);
		return -ENODEV;
	}

	input_info(true, ilits->dev, "%s: lcd is connected\n", __func__);

	lcdtype = get_lcd_info("id");
	if (lcdtype < 0) {
		input_err(true, ilits->dev, "%s: Failed to get lcd info\n", __func__);
		return -EINVAL;
	}
	input_info(true, ilits->dev, "%s: lcdtype : 0x%08X\n", __func__, lcdtype);

#else
	input_info(true, ilits->dev, "%s: lcdtype : 0x%08X\n", __func__, lcdtype);
#endif

	lcd_id1_gpio = of_get_named_gpio(np, "iliteck,lcdid1-gpio", 0);
	if (gpio_is_valid(lcd_id1_gpio))
		input_info(true, ilits->dev, "%s: lcd id1_gpio %d(%d)\n",
			__func__, lcd_id1_gpio, gpio_get_value(lcd_id1_gpio));
	else {
		input_err(true, ilits->dev, "%s: Failed to get novatek,lcdid1-gpio\n", __func__);
		return -EINVAL;
	}

	lcd_id2_gpio = of_get_named_gpio(np, "iliteck,lcdid2-gpio", 0);
	if (gpio_is_valid(lcd_id2_gpio)) {
		input_info(true, ilits->dev, "%s: lcd id2_gpio %d(%d)\n",
			__func__, lcd_id2_gpio, gpio_get_value(lcd_id2_gpio));
	} else {
		input_err(true, ilits->dev, "%s: Failed to get novatek,lcdid2-gpio\n", __func__);
		return -EINVAL;
	}

	/* support lcd id3 */
	lcd_id3_gpio = of_get_named_gpio(np, "iliteck,lcdid3-gpio", 0);
	if (gpio_is_valid(lcd_id3_gpio)) {
		input_info(true, ilits->dev, "%s: lcd id3_gpio %d(%d)\n",
			__func__, lcd_id3_gpio, gpio_get_value(lcd_id3_gpio));
		fw_sel_idx =
			(gpio_get_value(lcd_id3_gpio) << 2) | (gpio_get_value(lcd_id2_gpio) << 1) | gpio_get_value(lcd_id1_gpio);
	} else {
		input_err(true, ilits->dev, "%s: Failed to get novatek,lcdid3-gpio and use #1 &#2 id\n", __func__);
		fw_sel_idx = (gpio_get_value(lcd_id2_gpio) << 1) | gpio_get_value(lcd_id1_gpio);
	}

	fw_name_cnt = of_property_count_strings(np, "iliteck,fw_name");
	lcdtype_cnt = of_property_count_u32_elems(np, "iliteck,lcdtype");

	input_info(true, ilits->dev, "%s: fw_name_cnt(%d) & lcdtype_cnt(%d) & fw_sel_idx(%d)\n",
				__func__, fw_name_cnt, lcdtype_cnt, fw_sel_idx);

	if (lcdtype_cnt <= 0 || fw_name_cnt <= 0 || lcdtype_cnt <= fw_sel_idx || fw_name_cnt <= fw_sel_idx) {
		input_err(true, ilits->dev, "%s: abnormal lcdtype & fw name count, fw_sel_idx(%d)\n",
				__func__, fw_sel_idx);
		return -EINVAL;
	}
	of_property_read_u32_index(np, "iliteck,lcdtype", fw_sel_idx, &ilits->lcdtype);
	input_info(true, ilits->dev, "%s: lcd id(%d), ap lcdtype=0x%06X & dt lcdtype=0x%06X\n",
					__func__, fw_sel_idx, lcdtype, ilits->lcdtype);

	of_property_read_string_index(np, "iliteck,fw_name", fw_sel_idx, &ilits->fw_name);
	if (ilits->fw_name == NULL || strlen(ilits->fw_name) == 0) {
		input_err(true, ilits->dev, "%s: Failed to get fw name\n", __func__);
		return -EINVAL;
	}
	input_info(true, ilits->dev, "%s: fw name(%s)\n", __func__, ilits->fw_name);

	retval = of_property_read_string(np, "iliteck,lcd_rst", &ilits->regulator_lcd_rst);
	if (retval < 0) {
		input_err(true, ilits->dev, "%s: Failed to get regulator_lcd_rst name property\n", __func__);
	} else {
		if (!ilits->lcd_rst)
			ilits->lcd_rst = devm_regulator_get(ilits->dev, ilits->regulator_lcd_rst);
		if (IS_ERR_OR_NULL(ilits->lcd_rst)) {
			input_err(true, ilits->dev, "%s: Failed to get %s regulator.\n",
					__func__, ilits->regulator_lcd_rst);
			return -ENODEV;
		}
	}

	retval = of_property_read_string(np, "iliteck,lcd_bl_en", &ilits->regulator_lcd_bl_en);
	if (retval < 0) {
		input_err(true, ilits->dev, "%s: Failed to get regulator_lcd_bl_en name property\n", __func__);
	} else {
		if (!ilits->lcd_bl_en)
			ilits->lcd_bl_en = devm_regulator_get(ilits->dev, ilits->regulator_lcd_bl_en);
		if (IS_ERR_OR_NULL(ilits->lcd_bl_en)) {
			input_err(true, ilits->dev, "%s: Failed to get %s regulator.\n",
					__func__, ilits->regulator_lcd_bl_en);
			return -ENODEV;
		}
	}

	retval = of_property_read_string(np, "iliteck,lcd_vddi", &ilits->regulator_lcd_vddi);
	if (retval < 0) {
		input_err(true, ilits->dev, "%s: Failed to get regulator_lcd_rst name property\n", __func__);
	} else {
		if (!ilits->lcd_vddi)
			ilits->lcd_vddi = devm_regulator_get(ilits->dev, ilits->regulator_lcd_vddi);
		if (IS_ERR_OR_NULL(ilits->lcd_vddi)) {
			input_err(true, ilits->dev, "%s: Failed to get %s regulator.\n",
					__func__, ilits->regulator_lcd_vddi);
			return -ENODEV;
		}
	}

	prop = of_find_property(np, "iliteck,irq-gpio", NULL);
	if (prop && prop->length) {
		ilits->tp_int = of_get_named_gpio_flags(np,
				"iliteck,irq-gpio", 0,
				(enum of_gpio_flags *)&ilits->irq_flags);
	} else {
		ilits->tp_int = -1;
	}

	prop = of_find_property(np, "iliteck,reset-gpio", NULL);
	if (prop && prop->length) {
		ilits->tp_rst = of_get_named_gpio_flags(np,
				"iliteck,reset-gpio", 0, NULL);
	} else {
		ilits->tp_rst = -1;
	}
	input_err(true, ilits->dev, "%s ilits->tp_rst : %d\n", __func__, ilits->tp_rst);

	prop = of_find_property(np, "iliteck,cs-gpio", NULL);
	if (prop && prop->length)
		ilits->cs_gpio = of_get_named_gpio_flags(np, "iliteck,cs-gpio", 0, NULL);
	else
		ilits->cs_gpio = -1;
	input_info(true, ilits->dev, "%s ilits->cs_gpio : %d\n", __func__, ilits->cs_gpio);

	prop = of_find_property(np, "iliteck,spi-mode", NULL);
	if (prop && prop->length) {
		retval = of_property_read_u32(np, "iliteck,spi-mode", &value);
		if (retval < 0) {
			input_err(true, ilits->dev, "%s Unable to read iliteck,spi-mode property\n", __func__);
			return retval;
		}
		ilits->spi_mode = value;
	} else {
		ilits->spi_mode = 0;
	}

	prop = of_find_property(np, "iliteck,lcd_rst_delay", NULL);
	if (prop && prop->length) {
		retval = of_property_read_u32(np, "iliteck,lcd_rst_delay", &value);
		if (retval < 0) {
			input_err(true, ilits->dev, "%s Unable to read iliteck,lcd_rst_delay property\n", __func__);
			ilits->lcd_rst_delay = 0;
		} else{
			ilits->lcd_rst_delay = value;
		}
	} else {
		ilits->lcd_rst_delay = 0;
	}
	input_info(true, ilits->dev, "%s: lcd_rst_delay : %d(us)\n", __func__, ilits->lcd_rst_delay);


	if (of_property_read_u32_array(np, "iliteck,area-size", px_zone, 3)) {
		input_err(true, ilits->dev, "%s : Failed to get zone's size\n", __func__);
		ilits->area_indicator = 47;
		ilits->area_navigation = 75;
		ilits->area_edge = 45;
	} else {
		ilits->area_indicator = px_zone[0];
		ilits->area_navigation = px_zone[1];
		ilits->area_edge = px_zone[2];
	}
	input_info(true, ilits->dev, "%s : zone's size - indicator:%d, navigation:%d, edge:%d\n",
		__func__, ilits->area_indicator, ilits->area_navigation, ilits->area_edge);

	ilits->enable_settings_aot = of_property_read_bool(np, "iliteck,enable_settings_aot");
	ilits->enable_sysinput_enabled = of_property_read_bool(np, "iliteck,enable_sysinput_enabled");
	ilits->support_ear_detect = of_property_read_bool(np, "iliteck,support_ear_detect_mode");
	ilits->prox_lp_scan_enabled = of_property_read_bool(np, "iliteck,prox_lp_scan_enabled");
	ilits->support_spay_gesture_mode = of_property_read_bool(np, "iliteck,support_spay_gesture_mode");
	input_info(true, ilits->dev, "%s : supprot: %s%s%s%s%s\n",
				__func__, ilits->enable_settings_aot ? " AOT" : "",
				ilits->enable_sysinput_enabled ? " SE" : "",
				ilits->support_ear_detect ? " ED" : "",
				ilits->support_spay_gesture_mode ? "SPAY" : "",
				ilits->prox_lp_scan_enabled ? "LPSCAN" : "");

//	ilits->pinctrl = pinctrl_get_select_default(ilits->dev);
	ilits->pinctrl = devm_pinctrl_get(ilits->dev);
	if (!IS_ERR(ilits->pinctrl)) {
		ilits->pins_on_state = pinctrl_lookup_state(ilits->pinctrl, "pins_on_state");
		if (IS_ERR(ilits->pins_on_state)) {
			input_err(true, ilits->dev, "could not get pins pins_on_state (%li)\n",
				PTR_ERR(ilits->pins_on_state));
		}

		ilits->pins_off_state = pinctrl_lookup_state(ilits->pinctrl, "pins_off_state");
		if (IS_ERR(ilits->pins_off_state)) {
			input_err(true, ilits->dev, "could not get pins pins_off_state (%li)\n",
				PTR_ERR(ilits->pins_off_state));
		}
	} else {
		input_err(true, ilits->dev, "%s failed pinctrl_get\n", __func__);
	}
	ilitek_pin_control(true);

	return 0;
}

#ifdef CONFIG_VBUS_NOTIFIER
#if defined(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
static int otg_flag = 0;
#endif
#endif

#ifdef CONFIG_VBUS_NOTIFIER
#ifdef CONFIG_USB_TYPEC_MANAGER_NOTIFIER
static int ilitek_ccic_notification(struct notifier_block *nb,
	   unsigned long action, void *data)
{

	PD_NOTI_USB_STATUS_TYPEDEF usb_status = *(PD_NOTI_USB_STATUS_TYPEDEF *)data;

	if (usb_status.dest != PDIC_NOTIFY_DEV_USB) {
		return 0;
	}

	switch (usb_status.drp) {
	case USB_STATUS_NOTIFY_ATTACH_DFP:
		otg_flag = 1;
		input_info(true, ilits->dev, "%s otg_flag %d\n", __func__, otg_flag);
		break;
	case USB_STATUS_NOTIFY_DETACH:
		otg_flag = 0;
		input_info(true, ilits->dev, "%s otg_flag %d\n", __func__, otg_flag);
		break;
	default:
		break;
	}
	return 0;
}
#endif
static int ilitek_vbus_notification(struct notifier_block *nb,
		unsigned long cmd, void *data)
{
	vbus_status_t vbus_type = *(vbus_status_t *) data;
	int ret = 0;

#if defined(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	input_info(true, ilits->dev, "%s otg_flag=%d\n", __func__, otg_flag);
#endif
	input_info(true, ilits->dev, "%s cmd=%lu, vbus_type=%d\n", __func__, cmd, vbus_type);


	switch (vbus_type) {
	case STATUS_VBUS_HIGH:/* vbus_type == 2 */
#if defined(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
		if (!otg_flag)
			ilits->usb_plug_status = USB_PLUG_ATTACHED;
		else
			ret = -1;
#else
		ilits->usb_plug_status = USB_PLUG_ATTACHED;
#endif
		break;
	case STATUS_VBUS_LOW:/* vbus_type == 1 */
		ilits->usb_plug_status = USB_PLUG_DETACHED;
		break;
	default:
		ret = -1;
		break;
	}

	if (ret < 0)
		return 0;

	if (ilits->power_status == POWER_OFF_STATUS) {
		input_info(true, ilits->dev, "%s  now power off status, it'll setting after screen on(%d)\n",
				__func__ ,ilits->usb_plug_status);
		return 0;
	}

	mutex_lock(&ilits->touch_mutex);

	ret = ili_ic_func_ctrl("plug", !ilits->usb_plug_status);
	if (ret < 0)
		input_err(true, ilits->dev, "%s USB_PLUG set failed(%d)\n",
					__func__, ilits->usb_plug_status);
	mutex_unlock(&ilits->touch_mutex);
	return 0;
}
#endif

static int ilitek_plat_probe(void)
{
	int ret;

	input_info(true, ilits->dev, "%s platform probe\n", __func__);

	ret = parse_dt();
	if (ret < 0) {
		input_err(true, ilits->dev, "%s : parse_dt fail unload driver!\n", __func__);
		return -EINVAL;
	}

#ifdef CONFIG_BATTERY_SAMSUNG
	if (lpcharge) {
		input_info(true, ilits->dev, "%s: enter sleep mode in lpcharge %d\n", __func__, lpcharge);
		ilitek_pin_control(false);
		return -ENODEV;
	}
#endif

#if REGULATOR_POWER
	ilitek_plat_regulator_power_init();
#endif

	if (ilitek_plat_gpio_register() < 0)
		input_err(true, ilits->dev, "%s Register gpio failed\n", __func__);

	if (ili_tddi_init() < 0) {
		input_err(true, ilits->dev, "%s ILITEK Driver probe failed\n", __func__);
		return -ENODEV;
	}

	ili_irq_register(ilits->irq_tirgger_type);

#if SPRD_SYSFS_SUSPEND_RESUME
	ili_sysfs_add_device(ilits->dev);
	if (sysfs_create_link(NULL, &ilits->dev->kobj, "touchscreen") < 0)
		input_info(true, ilits->dev, "%s Failed to create link!\n", __func__);
#endif
	ilits->pm_suspend = false;
	init_completion(&ilits->pm_completion);
#if CHARGER_NOTIFIER_CALLBACK
#if KERNEL_VERSION(4, 1, 0) <= LINUX_VERSION_CODE
	/* add_for_charger_start */
	ilitek_plat_charger_init();
	/* add_for_charger_end */
#endif
#endif

#ifdef CONFIG_VBUS_NOTIFIER
#ifdef CONFIG_USB_TYPEC_MANAGER_NOTIFIER
	manager_notifier_register(&ilits->ccic_nb, ilitek_ccic_notification,
			MANAGER_NOTIFY_PDIC_INITIAL);
#endif
	vbus_notifier_register(&ilits->vbus_nb, ilitek_vbus_notification,
			VBUS_NOTIFY_DEV_CHARGER);
#endif

	input_info(true, ilits->dev, "%s ILITEK Driver loaded successfully!", __func__);
	return 0;
}

static int ilitek_tp_pm_suspend(struct device *dev)
{
	input_info(false, ilits->dev, "%s CALL BACK TP PM SUSPEND", __func__);
	ilits->pm_suspend = true;
	reinit_completion(&ilits->pm_completion);
	return 0;
}

static int ilitek_tp_pm_resume(struct device *dev)
{
	input_info(false, ilits->dev, "%s CALL BACK TP PM RESUME", __func__);
	ilits->pm_suspend = false;
	complete(&ilits->pm_completion);
	return 0;
}

static int ilitek_plat_remove(void)
{
	input_info(true, ilits->dev, "%s remove plat dev\n", __func__);
#if SPRD_SYSFS_SUSPEND_RESUME
	ili_sysfs_remove_device(ilits->dev);
#endif
	ili_dev_remove();
	return 0;
}

static int ilitek_plat_shutdown(void)
{
	input_info(true, ilits->dev, "%s shutdown plat dev\n", __func__);

	ili_dev_remove();

	return 0;
}

static const struct dev_pm_ops tp_pm_ops = {
	.suspend = ilitek_tp_pm_suspend,
	.resume = ilitek_tp_pm_resume,
};

static const struct of_device_id tp_match_table[] = {
	{.compatible = "iliteck,ili9882x-spi"},
	{},
};

static struct ilitek_hwif_info hwif = {
	.bus_type = TDDI_INTERFACE,
	.owner = THIS_MODULE,
	.name = TDDI_DEV_ID,
	.of_match_table = of_match_ptr(tp_match_table),
	.plat_probe = ilitek_plat_probe,
	.plat_shutdown = ilitek_plat_shutdown,
	.plat_remove = ilitek_plat_remove,
	.pm = &tp_pm_ops,
};

static int __init ilitek_plat_dev_init(void)
{
	if (ili_dev_init(&hwif) < 0) {
		printk(KERN_ERR "[sec_input] %s Failed to register i2c/spi bus driver\n", __func__);
		return -ENODEV;
	}
	return 0;
}

static void __exit ilitek_plat_dev_exit(void)
{
	input_info(true, ilits->dev, "%s remove plat dev\n", __func__);
	ili_dev_remove();
}

module_init(ilitek_plat_dev_init);
module_exit(ilitek_plat_dev_exit);
MODULE_AUTHOR("ILITEK");
MODULE_LICENSE("GPL");
