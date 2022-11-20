/*
 *	drivers/switch/88pm822_headset.c
 *
 *	headset detect driver for pm822
 *
 *	Copyright (C) 2013, Marvell Corporation
 *	Author: Hongyan Song <hysong@marvell.com>
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License version 2 as
 *	published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/mfd/88pm822.h>
#include <linux/switch.h>
#include <linux/workqueue.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <linux/mfd/88pm8xxx-headset.h>
#include <linux/input.h>

#if defined(CONFIG_MACH_GOLDEN)
static int voice_call_enable;
static int dock_state;
#endif

#define PM822_MIC_DET_TH		300
#define PM822_PRESS_RELEASE_TH		300
#define PM822_HOOK_PRESS_TH		20
#define PM822_VOL_UP_PRESS_TH		60
#define PM822_VOL_DOWN_PRESS_TH		110

#define PM822_HS_DET_INVERT		1
#define PM822_MIC_DET_AVG1		0xA8
#define PM822_MIC_DET_MIN1		0x88
#define PM822_MIC_DET_MAX1		0x98


#if defined(CONFIG_MACH_GOLDEN)
#include <mach/sm5502-muic.h>
int jack_is_detected = 0;
EXPORT_SYMBOL(jack_is_detected);
#endif

/* Headset detection info */
struct pm822_headset_info {
	struct pm822_chip *chip;
	struct device *dev;
	struct input_dev *idev;
	struct regmap *map;
	struct regmap *map_gpadc;
	int irq_headset;
	int irq_hook;
	struct work_struct work_headset, work_hook, work_release;
	struct headset_switch_data *psw_data_headset;
	void (*mic_set_power) (int on);
	int headset_flag;
	int hook_vol_status;
	int hook_press_th;
	int vol_up_press_th;
	int vol_down_press_th;
	int mic_det_th;
	int press_release_th;
	u32 hook_count;
	struct mutex hs_mutex;
	struct timer_list hook_timer;
};

struct headset_switch_data {
	struct switch_dev sdev;
	const char *name_on;
	const char *name_off;
	const char *state_on;
	const char *state_off;
	int irq;
	int state;
};
static struct device *hsdetect_dev;
static struct PM8XXX_HS_IOCTL hs_detect;
static struct pm822_headset_info *local_info;  // KSND add for handling info 

#if defined(CONFIG_MACH_GOLDEN)
static int dock_notify(struct notifier_block *self, unsigned long action, void *dev)
{
	dock_state = action;
	return NOTIFY_OK;
}

static struct notifier_block dock_nb = {
  .notifier_call = dock_notify,
};
#endif

enum {
	VOLTAGE_AVG,
	VOLTAGE_MIN,
	VOLTAGE_MAX,
	VOLTAGE_INS,
};

static int gpadc4_measure_voltage(struct pm822_headset_info *info)
{
	unsigned char buf[2];
	int sum = 0, ret = -1;

	ret = regmap_raw_read(info->map_gpadc, PM822_MIC_DET_AVG1, buf, 2); //KSND fixed
	if (ret < 0){
    pr_debug("%s: Fail to get the voltage!! \n", __func__);
    return 0;
  }
	/* GPADC4_dir = 1, measure(mv) = value *1.4 *1000/(2^12) */
	sum = ((buf[0] & 0xFF) << 4) | (buf[1] & 0x0F);
	sum = ((sum & 0xFFF) * 1400) >> 12;
	pr_debug("the voltage is %d mv\n", sum);
	return sum;
}

static void gpadc4_set_threshold(struct pm822_headset_info *info, int min,
				 int max)
{
	unsigned int data;
	if (min <= 0)
		data = 0;
	else
		data = ((min << 12) / 1400) >> 4;
	regmap_write(info->map_gpadc, PM822_MIC_DET_LOW_TH, data);
	if (max <= 0)
		data = 0xFF;
	else
		data = ((max << 12) / 1400) >> 4;
	regmap_write(info->map_gpadc, PM822_MIC_DET_UPP_TH, data);
}

static void pm822_hook_int(struct pm822_headset_info *info, int enable)
{
	mutex_lock(&info->hs_mutex);
	if (enable && info->hook_count == 0) {
		enable_irq(info->irq_hook);
		info->hook_count = 1;
	} else if (!enable && info->hook_count == 1) {
		disable_irq(info->irq_hook);
		info->hook_count = 0;
	}
	mutex_unlock(&info->hs_mutex);
}

static void pm822_headset_switch_work(struct work_struct *work)
{
	struct pm822_headset_info *info =
	    container_of(work, struct pm822_headset_info, work_headset);
	unsigned int value, voltage;
	int ret;
	struct headset_switch_data *switch_data;

	if (info == NULL) {
		pr_debug("Invalid headset info!\n");
		return;
	}

	switch_data = info->psw_data_headset;
	if (switch_data == NULL) {
		pr_debug("Invalid headset switch data!\n");
		return;
	}

	ret = regmap_read(info->map, PM822_MIC_CNTRL, &value);
	if (ret < 0) {
		dev_err(info->dev,
			"Failed to read PM822_MIC_CNTRL: 0x%x\n", ret);
		return;
	}

	value &= PM822_HEADSET_DET_MASK;
	if (info->headset_flag == PM822_HS_DET_INVERT)
		value = !value;

	if (value) {
		switch_data->state = PM8XXX_HEADSET_ADD;
		/* for telephony */
		kobject_uevent(&hsdetect_dev->kobj, KOBJ_ADD);
		hs_detect.hsdetect_status = PM8XXX_HEADSET_ADD;

		if (info->mic_set_power)
			info->mic_set_power(1);
		/* enable MIC detection also enable measurement */
		regmap_update_bits(info->map, PM822_MIC_CNTRL, PM822_MIC_DET_EN,
				   PM822_MIC_DET_EN);
		msleep(200);
		voltage = gpadc4_measure_voltage(info);
		if (voltage < info->mic_det_th) {
			switch_data->state = PM8XXX_HEADPHONE_ADD;
			hs_detect.hsmic_status = PM8XXX_HS_MIC_REMOVE;
			hs_detect.hsdetect_status = PM8XXX_HEADPHONE_ADD;

			if (info->mic_set_power)
				info->mic_set_power(0);
			/* disable MIC detection and measurement */
			regmap_update_bits(info->map, PM822_MIC_CNTRL,
					   PM822_MIC_DET_EN, 0);
		} else
			gpadc4_set_threshold(info, info->press_release_th, 0);
#if defined(CONFIG_MACH_GOLDEN)
			sm5502_chgpump_ctrl(1);
			if(dock_state == CABLE_TYPE2_DESKDOCK_MUIC || dock_state == CABLE_TYPE3_DESKDOCK_VB_MUIC){
				sm5502_dock_audiopath_ctrl(0);
			}
#endif
	} else {
		/* we already disable mic power when it is headphone */
		if ((switch_data->state == PM8XXX_HEADSET_ADD)
		    && info->mic_set_power)
			info->mic_set_power(0);

		/* disable MIC detection and measurement */
		regmap_update_bits(info->map, PM822_MIC_CNTRL, PM822_MIC_DET_EN,
				   0);
		switch_data->state = PM8XXX_HEADSET_REMOVE;
		info->hook_vol_status = HOOK_VOL_ALL_RELEASED;

		/* for telephony */
		kobject_uevent(&hsdetect_dev->kobj, KOBJ_REMOVE);
		hs_detect.hsdetect_status = PM8XXX_HEADSET_REMOVE;
		hs_detect.hsmic_status = PM8XXX_HS_MIC_ADD;

		
#if defined(CONFIG_MACH_GOLDEN)
		sm5502_chgpump_ctrl(0);
		if(dock_state == CABLE_TYPE2_DESKDOCK_MUIC || dock_state == CABLE_TYPE3_DESKDOCK_VB_MUIC){
			sm5502_dock_audiopath_ctrl(1);
		}
#endif
	}

	pr_info("[headset]headset_switch_work to %d\n", switch_data->state);
	switch_set_state(&switch_data->sdev, switch_data->state);
	/* enable hook irq if headset is present */
	if (switch_data->state == PM8XXX_HEADSET_ADD)
		pm822_hook_int(info, 1);
}

static int pm822_handle_voltage(struct pm822_headset_info *info, int voltage)
{
	int state;
	pr_info("[headset] hook detecting ADC [%d]!!\n",voltage);
	if (voltage < info->press_release_th) {
		/* press event */
		if (info->hook_vol_status <= HOOK_VOL_ALL_RELEASED) {
			if (voltage < info->hook_press_th)
				info->hook_vol_status = HOOK_PRESSED;
			else if (voltage < info->vol_up_press_th)
				info->hook_vol_status = VOL_UP_PRESSED;
			else if (voltage < info->vol_down_press_th)
				info->hook_vol_status = VOL_DOWN_PRESSED;
			else
				return -EINVAL;
			state = PM8XXX_HOOK_VOL_PRESSED;
			switch (info->hook_vol_status) {
			case HOOK_PRESSED:
				hs_detect.hookswitch_status = state;
				/* for telephony */
				kobject_uevent(&hsdetect_dev->kobj,
					       KOBJ_ONLINE);
				input_report_key(info->idev, KEY_MEDIA, state);
				break;
			case VOL_DOWN_PRESSED:
				input_report_key(info->idev, KEY_VOLUMEDOWN,
						 state);
				break;
			case VOL_UP_PRESSED:
				input_report_key(info->idev, KEY_VOLUMEUP,
						 state);
				break;
			default:
				break;
			}
			input_sync(info->idev);
			gpadc4_set_threshold(info, 0, info->press_release_th);
		} else
			return -EINVAL;
	} else {
		/* release event */
		if (info->hook_vol_status > HOOK_VOL_ALL_RELEASED) {
			state = PM8XXX_HOOK_VOL_RELEASED;
			switch (info->hook_vol_status) {
			case HOOK_PRESSED:
				info->hook_vol_status = HOOK_RELEASED;
				hs_detect.hookswitch_status = state;
				/* for telephony */
				kobject_uevent(&hsdetect_dev->kobj,
					       KOBJ_ONLINE);
				input_report_key(info->idev, KEY_MEDIA, state);
				break;
			case VOL_DOWN_PRESSED:
				info->hook_vol_status = VOL_DOWN_RELEASED;
				input_report_key(info->idev, KEY_VOLUMEDOWN,
						 state);
				break;
			case VOL_UP_PRESSED:
				info->hook_vol_status = VOL_UP_RELEASED;
				input_report_key(info->idev, KEY_VOLUMEUP,
						 state);
				break;
			default:
				break;
			}
			input_sync(info->idev);
			gpadc4_set_threshold(info, info->press_release_th, 0);
		} else
			return -EINVAL;
	}
	pr_info("[headset]hook_vol switch to %d\n", info->hook_vol_status);
	return 0;
}

static void pm822_hook_timer_handler(unsigned long data)
{
	struct pm822_headset_info *info = (struct pm822_headset_info *)data;

	queue_work(system_wq, &info->work_release);
}

static void pm822_hook_release_work(struct work_struct *work)
{
	struct pm822_headset_info *info =
	    container_of(work, struct pm822_headset_info, work_release);
	unsigned int voltage;

	if (info->hook_vol_status >= HOOK_PRESSED) {
		voltage = gpadc4_measure_voltage(info);
		if (voltage >= info->press_release_th)
			pm822_handle_voltage(info, voltage);
	}
}

static void pm822_hook_switch_work(struct work_struct *work)
{
	struct pm822_headset_info *info =
	    container_of(work, struct pm822_headset_info, work_hook);
	int ret;
	unsigned int value, voltage;

	if (info == NULL || info->idev == NULL) {
		pr_debug("Invalid hook info!\n");
		return;
	}
	msleep(50);
	ret = regmap_read(info->map, PM822_MIC_CNTRL, &value);
	if (ret < 0) {
		dev_err(info->dev,
			"Failed to read PM822_MIC_CNTRL: 0x%x\n", ret);
		return;
	}

	value &= PM822_HEADSET_DET_MASK;

	if (info->headset_flag == PM822_HS_DET_INVERT)
		value = !value;

	if (!value) {
		/* in case of headset unpresent, disable hook interrupt */
		pm822_hook_int(info, 0);
		pr_info("[headset] fake hook interupt\n");
		return;
	}
	voltage = gpadc4_measure_voltage(info);
	pm822_handle_voltage(info, voltage);

}

static ssize_t switch_headset_print_state(struct switch_dev *sdev, char *buf)
{
	return sprintf(buf, "%d\n", switch_get_state(sdev));
}

static irqreturn_t pm822_headset_handler(int irq, void *data)
{
	struct pm822_headset_info *info = data;
	if (irq == info->irq_headset) {
		if (info->psw_data_headset != NULL) {
			/*
			 * in any case headset interrupt comes,
			 * hook interrupt is not welcomed. so just
			 * disable it.
			 */
			pm822_hook_int(info, 0);
			queue_work(system_wq, &info->work_headset);
		}
	} else if (irq == info->irq_hook) {
		mod_timer(&info->hook_timer, jiffies + msecs_to_jiffies(150));
		/* hook interrupt */
		if (info->idev != NULL)
			queue_work(system_wq, &info->work_hook);
	}

	return IRQ_HANDLED;
}

#if 1 //KSND

/*************************************
  * add sysfs for factory test. KSND 131029
  * 
 *************************************/
/* sysfs for hook key status - KSND */
static ssize_t  key_state_onoff_show(struct device *dev,	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", (hs_detect.hookswitch_status)? "1":"0");
}

static ssize_t  earjack_state_onoff_show(struct device *dev,	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", hs_detect.hsdetect_status);
}

static ssize_t select_jack_show(struct device *dev,	struct device_attribute *attr, char *buf)
{
	pr_info("%s : operate nothing\n", __func__);
	return 0;
}

static ssize_t select_jack_store(struct device *dev, 	struct device_attribute *attr, const char *buf, size_t size)
{
	int value = 0;

	sscanf(buf, "%d", &value);
	pr_info("%s: User  selection : 0X%x", __func__, value);
	if (value == PM8XXX_HEADSET_ADD) {
		if (local_info->mic_set_power) {
			local_info->mic_set_power(1);
			msleep(100);
		}
	}
	local_info->psw_data_headset->state = value;
	return size;
}

static DEVICE_ATTR(key_state, 0664 , key_state_onoff_show, NULL);
static DEVICE_ATTR(state, 0664 , earjack_state_onoff_show, NULL);
static DEVICE_ATTR(select_jack, 0664, select_jack_show, 	select_jack_store);
#endif

#define MIC_DET_DBS		(3 << 1)
#define MIC_DET_PRD		(3 << 3)
#define MIC_DET_DBS_32MS	(3 << 1)
#define MIC_DET_PRD_CTN		(3 << 3)

static int pm822_headset_switch_probe(struct platform_device *pdev)
{
	struct pm822_chip *chip = dev_get_drvdata(pdev->dev.parent);
	struct pm822_headset_info *info;
	struct gpio_switch_platform_data *pdata_headset =
	    pdev->dev.platform_data;
	struct headset_switch_data *switch_data_headset = NULL;
	struct pm822_platform_data *pm822_pdata;
	int irq_headset, irq_hook, ret = 0;
#if 1 //KSND
	struct class *audio;
	struct device *earjack;
#endif

	if (pdev->dev.parent->platform_data) {
		pm822_pdata = pdev->dev.parent->platform_data;
	} else {
		pr_debug("Invalid pm822 platform data!\n");
		return -EINVAL;
	}
	if (!pm822_pdata->headset) {
		dev_err(&pdev->dev, "No headset platform info!\n");
		return -EINVAL;
	}
	if (pdata_headset == NULL) {
		pr_debug("Invalid gpio switch platform data!\n");
		return -EBUSY;
	}
	irq_headset = platform_get_irq(pdev, 0);
	if (irq_headset < 0) {
		dev_err(&pdev->dev, "No IRQ resource for headset!\n");
		return -EINVAL;
	}
	irq_hook = platform_get_irq(pdev, 1);
	if (irq_hook < 0) {
		dev_err(&pdev->dev, "No IRQ resource for hook/mic!\n");
		return -EINVAL;
	}
	info =
	    devm_kzalloc(&pdev->dev, sizeof(struct pm822_headset_info),
			 GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->chip = chip;
	info->mic_set_power = pm822_pdata->headset->mic_set_power;
	info->headset_flag = pm822_pdata->headset->headset_flag;
	info->map = chip->regmap;
	info->map_gpadc = chip->subchip->regmap_gpadc;
	info->dev = &pdev->dev;
	info->irq_headset = irq_headset + chip->irq_base;
	info->irq_hook = irq_hook + chip->irq_base;

	info->idev = input_allocate_device();
	if (!info->idev) {
		dev_err(&pdev->dev, "Failed to allocate input dev\n");
		ret = -ENOMEM;
		goto input_allocate_fail;
	}

	info->idev->name = "88pm822_hook_vol";
	info->idev->phys = "88pm822_hook_vol/input0";
	info->idev->id.bustype = BUS_I2C;
	info->idev->dev.parent = &pdev->dev;

	__set_bit(EV_KEY, info->idev->evbit);
	__set_bit(KEY_MEDIA, info->idev->keybit);
	__set_bit(KEY_VOLUMEUP, info->idev->keybit);
	__set_bit(KEY_VOLUMEDOWN, info->idev->keybit);
	info->hook_vol_status = HOOK_VOL_ALL_RELEASED;

	if (pm822_pdata->headset->hook_press_th)
		info->hook_press_th = pm822_pdata->headset->hook_press_th;
	else
		info->hook_press_th = PM822_HOOK_PRESS_TH;

	if (pm822_pdata->headset->vol_up_press_th)
		info->vol_up_press_th = pm822_pdata->headset->vol_up_press_th;
	else
		info->vol_up_press_th = PM822_VOL_UP_PRESS_TH;

	if (pm822_pdata->headset->vol_down_press_th)
		info->vol_down_press_th =
			pm822_pdata->headset->vol_down_press_th;
	else
		info->vol_down_press_th = PM822_VOL_DOWN_PRESS_TH;

	if (pm822_pdata->headset->mic_det_th)
		info->mic_det_th = pm822_pdata->headset->mic_det_th;
	else
		info->mic_det_th = PM822_MIC_DET_TH;

	if (pm822_pdata->headset->press_release_th)
		info->press_release_th = pm822_pdata->headset->press_release_th;
	else
		info->press_release_th = PM822_PRESS_RELEASE_TH;

	switch_data_headset =
	    devm_kzalloc(&pdev->dev, sizeof(struct headset_switch_data),
			 GFP_KERNEL);
	if (!switch_data_headset) {
		dev_err(&pdev->dev, "Failed to allocate headset data\n");
		ret = -ENOMEM;
		goto headset_allocate_fail;
	}

	switch_data_headset->sdev.name = pdata_headset->name;
	switch_data_headset->name_on = pdata_headset->name_on;
	switch_data_headset->name_off = pdata_headset->name_off;
	switch_data_headset->state_on = pdata_headset->state_on;
	switch_data_headset->state_off = pdata_headset->state_off;
	switch_data_headset->sdev.print_state = switch_headset_print_state;
	info->psw_data_headset = switch_data_headset;

	ret = switch_dev_register(&switch_data_headset->sdev);
	if (ret < 0)
		goto err_switch_dev_register;
	mutex_init(&info->hs_mutex);		

	/* init timer for hook release */
	setup_timer(&info->hook_timer, pm822_hook_timer_handler,
		    (unsigned long)info);

	ret =
	    request_threaded_irq(info->irq_headset, NULL, pm822_headset_handler,
				 IRQF_ONESHOT, "headset", info);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to request IRQ: #%d: %d\n",
			info->irq_headset, ret);
		goto out_irq_headset;
	}

	ret =
	    request_threaded_irq(info->irq_hook, NULL, pm822_headset_handler,
				 IRQF_ONESHOT, "hook", info);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to request IRQ: #%d: %d\n",
			info->irq_hook, ret);
		goto out_irq_hook;
	}

	/*
	 * disable hook irq to ensure this irq will be enabled
	 * after plugging in headset
	 */
	info->hook_count = 1;
	pm822_hook_int(info, 0);
	ret = input_register_device(info->idev);
	if (ret < 0)
		goto err_input_dev_register;

	platform_set_drvdata(pdev, info);
	hsdetect_dev = &pdev->dev;
	hs_detect.hsdetect_status = 0;
	hs_detect.hookswitch_status = 0;
	
	INIT_WORK(&info->work_headset, pm822_headset_switch_work);
	INIT_WORK(&info->work_hook, pm822_hook_switch_work);
	INIT_WORK(&info->work_release, pm822_hook_release_work);
	
	/* default 4_POLES */
	hs_detect.hsmic_status = PM8XXX_HS_MIC_ADD;

	/* Hook:32 ms debounce time */
	regmap_update_bits(info->map, PM822_MIC_CNTRL, MIC_DET_DBS,
			   MIC_DET_DBS_32MS);
	/* Hook:continue duty cycle */
	regmap_update_bits(info->map, PM822_MIC_CNTRL, MIC_DET_PRD,
			   MIC_DET_PRD_CTN);
	/* set GPADC_DIR to 0, set to enable internal register divider (1:4) */
	regmap_update_bits(info->map_gpadc, PM822_GPADC_MISC_CONFIG1,
			   PM822_GPADC4_DIR, 0);		
	/* enable headset detection on GPIO3 */
	regmap_update_bits(info->map, PM822_HEADSET_CNTRL, PM822_HEADSET_DET_EN,
			   PM822_HEADSET_DET_EN);
	pm822_headset_switch_work(&info->work_headset);

#if 1 //KSND
	/* sys fs for factory test */
	audio = class_create(THIS_MODULE, "audio");
	if (IS_ERR(audio))
		pr_err("Failed to create class(audio)!\n");

	earjack = device_create(audio, NULL, 0, NULL, "earjack");
	if (IS_ERR(earjack))
		pr_err("Failed to create device(earjack)!\n");

	ret = device_create_file(earjack, &dev_attr_key_state);
	if (ret)
		pr_err("Failed to create device file in sysfs entries!\n");

	ret = device_create_file(earjack, &dev_attr_state);
	if (ret)
		pr_err("Failed to create device file in sysfs entries!\n");
    
	ret = device_create_file(earjack, &dev_attr_select_jack);
	if (ret)
		pr_err("Failed to create device file in sysfs entries(%s)!\n", dev_attr_select_jack.attr.name);
#endif
#if defined(CONFIG_MACH_GOLDEN)
	usb_switch_register_notify(&dock_nb);
#endif

	return 0;

out_irq_hook:
	free_irq(info->irq_hook, info);
out_irq_headset:
	free_irq(info->irq_headset, info);
err_input_dev_register:
	devm_kfree(&pdev->dev, switch_data_headset);
err_switch_dev_register:
	switch_dev_unregister(&switch_data_headset->sdev);
headset_allocate_fail:
	input_free_device(info->idev);
input_allocate_fail:
	devm_kfree(&pdev->dev, info);
	return ret;
}

static int __devexit pm822_headset_switch_remove(struct platform_device *pdev)
{
	struct pm822_headset_info *info = platform_get_drvdata(pdev);
	struct headset_switch_data *switch_data_headset =
	    info->psw_data_headset;
	/* disable headset detection on GPIO3 */
	regmap_update_bits(info->map, PM822_HEADSET_CNTRL, PM822_HEADSET_DET_EN,
			   0);

	cancel_work_sync(&info->work_headset);
	cancel_work_sync(&info->work_hook);

	free_irq(info->irq_hook, info);
	free_irq(info->irq_headset, info);

	switch_dev_unregister(&switch_data_headset->sdev);
	input_unregister_device(info->idev);

	input_free_device(info->idev);
	devm_kfree(&pdev->dev, switch_data_headset);
	devm_kfree(&pdev->dev, info);
	
#if defined(CONFIG_MACH_GOLDEN)
	usb_switch_unregister_notify(&dock_nb);
#endif

	return 0;
}

static int pm822_headset_switch_suspend(struct platform_device *pdev,
					pm_message_t state)
{
	return 0;
}

static int pm822_headset_switch_resume(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver pm822_headset_switch_driver = {
	.probe = pm822_headset_switch_probe,
	.remove = __devexit_p(pm822_headset_switch_remove),
	.suspend = pm822_headset_switch_suspend,
	.resume = pm822_headset_switch_resume,
	.driver = {
		   .name = "88pm822-headset",
		   .owner = THIS_MODULE,
		   },
};

static struct miscdevice pm822_hsdetect_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "88pm822-headset",
};

static int __init headset_switch_init(void)
{
	int ret = -EINVAL;
	ret = misc_register(&pm822_hsdetect_miscdev);
	if (ret < 0)
		return ret;
	ret = platform_driver_register(&pm822_headset_switch_driver);
	return ret;
}

module_init(headset_switch_init);
static void __exit headset_switch_exit(void)
{
	platform_driver_unregister(&pm822_headset_switch_driver);
	misc_deregister(&pm822_hsdetect_miscdev);
}

module_exit(headset_switch_exit);

MODULE_DESCRIPTION("Marvell 88pm822 Headset driver");
MODULE_AUTHOR("Hongyan Song <hysong@marvell.com>");
MODULE_LICENSE("GPL");
