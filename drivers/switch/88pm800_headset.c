/*
 *	drivers/switch/88pm800_headset.c
 *
 *	headset & hook detect driver for pm800
 *
 *	Copyright (C) 2012, Marvell Corporation (wzch@Marvell.com)
 *	Author: Wenzeng Chen <wzch@marvell.com>
 *				 Mike Lockwood <lockwood@android.com>
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License version 2 as
 *	published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/mfd/88pm80x.h>
#include <linux/mfd/88pm8xxx-headset.h>
#include <linux/switch.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/input.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>

#define	PM800_HEADSET_PROC_FILE		"driver/pm800_headset"
static int voice_call_enable;

#define PM800_MIC_DET_TH				300
#define PM800_PRESS_RELEASE_TH			300
#define PM800_HOOK_PRESS_TH			20
#define PM800_VOL_UP_PRESS_TH			60
#define PM800_VOL_DOWN_PRESS_TH		110

#define PM800_HS_DET_INVERT		1
#define PM800_MIC_CNTRL			(0x39)
#define PM800_MICDET_EN			(1 << 0)

#define PM800_GPADC4_AVG1		0xA8
#define PM800_GPADC4_MIN1		0x88
#define PM800_GPADC4_MAX1		0x98

#define PM800_INT_ENA_3		(0x0B)
#define PM800_INT_ENA_4		(0x0C)
#define PM800_GPIO3_INT_ENA4		(1 << 3)
#define PM800_GPADC4_INT_ENA3		(1 << 4)
#define PM800_GPADC4_DIR			(1 << 6)

struct pm800_headset_info {
	struct pm80x_chip *chip;
	struct device *dev;
	struct input_dev *idev;
	struct regmap *map;
	struct regmap *map_gpadc;
	int irq_headset;
	int irq_hook;
	struct work_struct work_headset, work_hook;
	struct headset_switch_data *psw_data_headset;
	void (*mic_set_power) (int on);
	int headset_flag;
	int hook_vol_status;
	int hook_press_th;
	int vol_up_press_th;
	int vol_down_press_th;
	int mic_det_th;
	int press_release_th;
	struct proc_dir_entry *proc_file;
	u32 hook_count;
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

enum {
	VOLTAGE_AVG,
	VOLTAGE_MIN,
	VOLTAGE_MAX,
	VOLTAGE_INS,
};

static int gpadc4_measure_voltage(struct pm800_headset_info *info, int which)
{
	unsigned char buf[2];
	int sum = 0, ret = -1;
	switch (which) {
	case VOLTAGE_AVG:
		ret =
		    regmap_raw_read(info->map_gpadc, PM800_GPADC4_AVG1, buf, 2);
		break;
	case VOLTAGE_MIN:
		ret =
		    regmap_raw_read(info->map_gpadc, PM800_GPADC4_MIN1, buf, 2);
		break;
	case VOLTAGE_MAX:
		ret =
		    regmap_raw_read(info->map_gpadc, PM800_GPADC4_MAX1, buf, 2);
		break;
	case VOLTAGE_INS:
		ret =
		    regmap_raw_read(info->map_gpadc, PM800_GPADC4_MEAS1, buf,
				    2);
		break;
	default:
		break;
	}
	if (ret < 0)
		return 0;
	/* GPADC4_dir = 1, measure(mv) = value *1.4 *1000/(2^12) */
	sum = ((buf[0] & 0xFF) << 4) | (buf[1] & 0x0F);
	sum = ((sum & 0xFFF) * 1400) >> 12;
	pr_debug("the voltage is %d mv\n", sum);
	return sum;
}

static void gpadc4_set_threshold(struct pm800_headset_info *info, int min,
				 int max)
{
	unsigned int data;
	if (min <= 0)
		data = 0;
	else
		data = ((min << 12) / 1400) >> 4;
	regmap_write(info->map_gpadc, PM800_GPADC4_LOW_TH, data);
	if (max <= 0)
		data = 0xFF;
	else
		data = ((max << 12) / 1400) >> 4;
	regmap_write(info->map_gpadc, PM800_GPADC4_UPP_TH, data);
}

static void pm800_hook_int(struct pm800_headset_info *info, int enable)
{
	if (enable && info->hook_count == 0) {
		enable_irq(info->irq_hook);
		info->hook_count = 1;
	} else if (!enable && info->hook_count == 1) {
		disable_irq(info->irq_hook);
		info->hook_count = 0;
	}
}

static int pm800_handle_voltage(struct pm800_headset_info *info, int voltage)
{
	int state;
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
			gpadc4_set_threshold(info, 8, info->press_release_th);
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
	pr_info("hook_vol switch to %d\n", info->hook_vol_status);
	return 0;
}

static void pm800_hook_switch_work(struct work_struct *work)
{
	struct pm800_headset_info *info =
	    container_of(work, struct pm800_headset_info, work_hook);
	int ret;
	unsigned int value, voltage;

	if (info == NULL || info->idev == NULL) {
		pr_debug("Invalid hook info!\n");
		return;
	}
	msleep(50);
	ret = regmap_read(info->map, PM800_GPIO_2_3_CNTRL, &value);
	if (ret < 0) {
		dev_err(info->dev,
			"Failed to read PM800_GPIO_2_3_CNTRL: 0x%x\n", ret);
		return;
	}

	value &= PM800_GPIO3_VAL;

	if (info->headset_flag == PM800_HS_DET_INVERT)
		value = !value;

	if (!value) {
		/* in case of headset unpresent, disable hook interrupt */
		pm800_hook_int(info, 0);
		pr_info("fake hook interupt\n");
		return;
	}
	voltage = gpadc4_measure_voltage(info, VOLTAGE_AVG);
	pm800_handle_voltage(info, voltage);
	regmap_update_bits(info->map, PM800_INT_ENA_3, PM800_GPADC4_INT_ENA3,
			   PM800_GPADC4_INT_ENA3);
}

static void pm800_headset_switch_work(struct work_struct *work)
{
	struct pm800_headset_info *info =
	    container_of(work, struct pm800_headset_info, work_headset);
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

	ret = regmap_read(info->map, PM800_GPIO_2_3_CNTRL, &value);
	if (ret < 0) {
		dev_err(info->dev,
			"Failed to read PM800_GPIO_2_3_CNTRL: 0x%x\n", ret);
		return;
	}

	value &= PM800_GPIO3_VAL;
	if (info->headset_flag == PM800_HS_DET_INVERT)
		value = !value;

	if (value) {
		switch_data->state = PM8XXX_HEADSET_ADD;
		/* for telephony */
		kobject_uevent(&hsdetect_dev->kobj, KOBJ_ADD);
		hs_detect.hsdetect_status = PM8XXX_HEADSET_ADD;
		if (info->mic_set_power)
			info->mic_set_power(1);
		/* enable MIC detection also enable measurement */
		regmap_update_bits(info->map, PM800_MIC_CNTRL, PM800_MICDET_EN,
				   PM800_MICDET_EN);
		msleep(200);
		voltage = gpadc4_measure_voltage(info, VOLTAGE_AVG);
		if (voltage < info->mic_det_th) {
			switch_data->state = PM8XXX_HEADPHONE_ADD;
			hs_detect.hsmic_status = PM8XXX_HS_MIC_REMOVE;
			if (info->mic_set_power)
				info->mic_set_power(0);
			/* disable MIC detection and measurement */
			regmap_update_bits(info->map, PM800_MIC_CNTRL,
					   PM800_MICDET_EN, 0);
		} else
			gpadc4_set_threshold(info, info->press_release_th, 0);
	} else {
		/* we already disable mic power when it is headphone */
		if ((switch_data->state == PM8XXX_HEADSET_ADD)
		    && info->mic_set_power)
			info->mic_set_power(0);

		/* disable MIC detection and measurement */
		regmap_update_bits(info->map, PM800_MIC_CNTRL, PM800_MICDET_EN,
				   0);
		switch_data->state = PM8XXX_HEADSET_REMOVE;
		info->hook_vol_status = HOOK_VOL_ALL_RELEASED;

		/* for telephony */
		kobject_uevent(&hsdetect_dev->kobj, KOBJ_REMOVE);
		hs_detect.hsdetect_status = PM8XXX_HEADSET_REMOVE;
		hs_detect.hsmic_status = PM8XXX_HS_MIC_ADD;
	}

	pr_info("headset_switch_work to %d\n", switch_data->state);
	switch_set_state(&switch_data->sdev, switch_data->state);
	/* enable hook irq if headset is present */
	if (switch_data->state == PM8XXX_HEADSET_ADD)
		pm800_hook_int(info, 1);
}

static ssize_t switch_headset_print_state(struct switch_dev *sdev, char *buf)
{
	struct headset_switch_data *switch_data =
	    container_of(sdev, struct headset_switch_data, sdev);
	const char *state;
	if (switch_get_state(sdev))
		state = switch_data->state_on;
	else
		state = switch_data->state_off;
	if (state)
		return sprintf(buf, "%s\n", state);
	return -1;
}

static long pm800_hsdetect_ioctl(struct file *file, unsigned int cmd,
				 unsigned long arg)
{
	struct PM8XXX_HS_IOCTL hs_ioctl;
	if (copy_from_user(&hs_ioctl,
			   (void *)arg, sizeof(struct PM8XXX_HS_IOCTL)))
		return -EFAULT;
	switch (cmd) {
	case PM8XXX_HSDETECT_STATUS:
		hs_ioctl.hsdetect_status = hs_detect.hsdetect_status;
		hs_ioctl.hookswitch_status = hs_detect.hookswitch_status;

#if defined(ENABLE_HS_DETECT_POLES)
		hs_ioctl.hsmic_status = hs_detect.hsmic_status;
#endif

		pr_info("hsdetect_ioctl PM8XXX_HSDETECT_STATUS\n");
		break;
	case PM8XXX_HOOKSWITCH_STATUS:
		hs_ioctl.hookswitch_status = hs_detect.hookswitch_status;
		hs_ioctl.hsdetect_status = hs_detect.hsdetect_status;

#if defined(ENABLE_HS_DETECT_POLES)
		hs_ioctl.hsmic_status = hs_detect.hsmic_status;

#endif /* */
		pr_info("hsdetect_ioctl PM8XXX_HOOKSWITCH_STATUS\n");
		break;
	default:
		return -ENOTTY;
	}
	return copy_to_user((void *)arg, &hs_ioctl,
			    sizeof(struct PM8XXX_HS_IOCTL));
}

static irqreturn_t pm800_headset_handler(int irq, void *data)
{
	struct pm800_headset_info *info = data;
	if (irq == info->irq_headset) {
		/* headset interrupt */
		if (info->psw_data_headset != NULL) {
			/*
			 * in any case headset interrupt comes,
			 * hook interrupt is not welcomed. so just
			 * disable it.
			 */
			pm800_hook_int(info, 0);
			queue_work(system_wq, &info->work_headset);
		}
	} else if (irq == info->irq_hook) {
		/* hook interrupt */
		if (info->idev != NULL) {
			regmap_update_bits(info->map, PM800_INT_ENA_3,
					   PM800_GPADC4_INT_ENA3, 0);
			queue_work(system_wq, &info->work_hook);
		}
	}

	return IRQ_HANDLED;
}

static ssize_t pm800_headset_proc_read(char *buf, char **start, off_t off,
		int count, int *eof, void *data)
{
	int len = 0;

	len = sprintf(buf, "headset detect voice call case is  %s\n", voice_call_enable?"enabled":"disabled");

	return len;
}

static ssize_t pm800_headset_proc_write(struct file *filp, const char *buff,
		size_t len, void *data)
{
	char is_voice_call[2];

	if (len > 2)
		return -EFAULT;

	if (copy_from_user(is_voice_call, buff, len))
		return -EFAULT;

	if('0' == is_voice_call[0]) {
		voice_call_enable = 0;
	} else {
		voice_call_enable = 1;
	}

	return len;
}


#define MIC_DET_DBS		(3 << 1)
#define MIC_DET_PRD		(3 << 3)
#define MIC_DET_DBS_32MS	(3 << 1)
#define MIC_DET_PRD_CTN		(3 << 3)
#define MIC_DET_PRD_CTN_256MS		(1 << 3)

static int pm800_headset_switch_probe(struct platform_device *pdev)
{
	struct pm80x_chip *chip = dev_get_drvdata(pdev->dev.parent);
	struct pm800_headset_info *info;
	struct gpio_switch_platform_data *pdata_headset =
	    pdev->dev.platform_data;
	struct headset_switch_data *switch_data_headset = NULL;
	struct pm80x_platform_data *pm80x_pdata;
	int irq_headset, irq_hook, ret = 0;

	if (pdev->dev.parent->platform_data) {
		pm80x_pdata = pdev->dev.parent->platform_data;
	} else {
		pr_debug("Invalid pm800 platform data!\n");
		return -EINVAL;
	}
	if (!pm80x_pdata->headset) {
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
	    devm_kzalloc(&pdev->dev, sizeof(struct pm800_headset_info),
			 GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->chip = chip;
	info->mic_set_power = pm80x_pdata->headset->mic_set_power;
	info->headset_flag = pm80x_pdata->headset->headset_flag;
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

	info->idev->name = "88pm800_hook_vol";
	info->idev->phys = "88pm800_hook_vol/input0";
	info->idev->id.bustype = BUS_I2C;
	info->idev->dev.parent = &pdev->dev;

	__set_bit(EV_KEY, info->idev->evbit);
	__set_bit(KEY_MEDIA, info->idev->keybit);
	__set_bit(KEY_VOLUMEUP, info->idev->keybit);
	__set_bit(KEY_VOLUMEDOWN, info->idev->keybit);
	info->hook_vol_status = HOOK_VOL_ALL_RELEASED;

	if (pm80x_pdata->headset->hook_press_th)
		info->hook_press_th = pm80x_pdata->headset->hook_press_th;
	else
		info->hook_press_th = PM800_HOOK_PRESS_TH;

	if (pm80x_pdata->headset->vol_up_press_th)
		info->vol_up_press_th = pm80x_pdata->headset->vol_up_press_th;
	else
		info->vol_up_press_th = PM800_VOL_UP_PRESS_TH;

	if (pm80x_pdata->headset->vol_down_press_th)
		info->vol_down_press_th =
			pm80x_pdata->headset->vol_down_press_th;
	else
		info->vol_down_press_th = PM800_VOL_DOWN_PRESS_TH;

	if (pm80x_pdata->headset->mic_det_th)
		info->mic_det_th = pm80x_pdata->headset->mic_det_th;
	else
		info->mic_det_th = PM800_MIC_DET_TH;

	if (pm80x_pdata->headset->press_release_th)
		info->press_release_th = pm80x_pdata->headset->press_release_th;
	else
		info->press_release_th = PM800_PRESS_RELEASE_TH;

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

	ret =
	    request_threaded_irq(info->irq_headset, NULL, pm800_headset_handler,
				 IRQF_ONESHOT, "headset", info);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to request IRQ: #%d: %d\n",
			info->irq_headset, ret);
		goto out_irq_headset;
	}

	ret =
	    request_threaded_irq(info->irq_hook, NULL, pm800_headset_handler,
				 IRQF_ONESHOT, "hook", info);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to request IRQ: #%d: %d\n",
			info->irq_hook, ret);
		goto out_irq_hook;
	}

	voice_call_enable = 0;
	if (info->proc_file == NULL) {
		info->proc_file =
			create_proc_entry(PM800_HEADSET_PROC_FILE, 0644, NULL);
		if (info->proc_file) {
			info->proc_file->read_proc = pm800_headset_proc_read;
			info->proc_file->write_proc = (write_proc_t  *)pm800_headset_proc_write;
			info->proc_file->data = info;
		} else
			pr_info("pm800 headset proc file create failed!\n");
	}

	/*
	 * disable hook irq to ensure this irq will be enabled
	 * after plugging in headset
	 */
	info->hook_count = 1;
	pm800_hook_int(info, 0);
	ret = input_register_device(info->idev);
	if (ret < 0)
		goto err_input_dev_register;

	platform_set_drvdata(pdev, info);
	INIT_WORK(&info->work_headset, pm800_headset_switch_work);
	INIT_WORK(&info->work_hook, pm800_hook_switch_work);
	hsdetect_dev = &pdev->dev;
	hs_detect.hsdetect_status = 0;
	hs_detect.hookswitch_status = 0;
	/* default 4_POLES */
	hs_detect.hsmic_status = PM8XXX_HS_MIC_ADD;

	/* Hook:32 ms debounce time */
	regmap_update_bits(info->map, PM800_MIC_CNTRL, MIC_DET_DBS,
			   MIC_DET_DBS_32MS);
	/* Hook:256ms period duty cycle */
	regmap_update_bits(info->map, PM800_MIC_CNTRL, MIC_DET_PRD,
			   MIC_DET_PRD_CTN_256MS);
	/* set GPADC_DIR to 1, set to 0 cause pop noise in recording */
	regmap_update_bits(info->map_gpadc, PM800_GPADC_MISC_CONFIG1,
			   PM800_GPADC4_DIR, PM800_GPADC4_DIR);
	/* enable headset detection on GPIO3 */
	regmap_update_bits(info->map, PM800_HEADSET_CNTRL, PM800_HEADSET_DET_EN,
			   PM800_HEADSET_DET_EN);
	pm800_headset_switch_work(&info->work_headset);

	return 0;

err_input_dev_register:
	free_irq(info->irq_hook, info);
out_irq_hook:
	free_irq(info->irq_headset, info);
out_irq_headset:
	switch_dev_unregister(&switch_data_headset->sdev);
err_switch_dev_register:
	devm_kfree(&pdev->dev, switch_data_headset);
headset_allocate_fail:
	input_free_device(info->idev);
input_allocate_fail:
	devm_kfree(&pdev->dev, info);
	return ret;
}

static int __devexit pm800_headset_switch_remove(struct platform_device *pdev)
{
	struct pm800_headset_info *info = platform_get_drvdata(pdev);
	struct headset_switch_data *switch_data_headset =
	    info->psw_data_headset;
	/* disable headset detection on GPIO3 */
	regmap_update_bits(info->map, PM800_HEADSET_CNTRL, PM800_HEADSET_DET_EN,
			   0);
	/* disable GPIO3 interrupt */
	regmap_update_bits(info->map, PM800_INT_ENA_4, PM800_GPIO3_INT_ENA4, 0);

	cancel_work_sync(&info->work_headset);
	cancel_work_sync(&info->work_hook);

	free_irq(info->irq_hook, info);
	free_irq(info->irq_headset, info);

	switch_dev_unregister(&switch_data_headset->sdev);
	input_unregister_device(info->idev);

	devm_kfree(&pdev->dev, switch_data_headset);
	devm_kfree(&pdev->dev, info);

	return 0;
}

static int pm800_headset_switch_suspend(struct platform_device *pdev,
					pm_message_t state)
{
	struct pm800_headset_info *info = platform_get_drvdata(pdev);
	struct headset_switch_data *switch_data;

	switch_data = info->psw_data_headset;

	/* enable low power mode headset detection */
	regmap_update_bits(info->map, PM800_HEADSET_CNTRL, PM800_HSDET_SLP,
                          PM800_HSDET_SLP);

	/* LDO2(mic) power off when headset inserted except voice call case*/
	  if (switch_data->state == 1 && !(voice_call_enable))
		info->mic_set_power(0);

	return 0;
}

static int pm800_headset_switch_resume(struct platform_device *pdev)
{
	struct pm800_headset_info *info = platform_get_drvdata(pdev);
	struct headset_switch_data *switch_data;

	switch_data = info->psw_data_headset;

	/* disable low power mode headset detection */
	regmap_update_bits(info->map, PM800_HEADSET_CNTRL, PM800_HSDET_SLP, 0);

	/* Enable LDO2 again if we detect headset(not headphone).
	 * since we use LDO2 for mic detect in active mode.
	 */
	if (switch_data->state == 1 &&  !(voice_call_enable))
		info->mic_set_power(1);

	return 0;
}

static struct platform_driver pm800_headset_switch_driver = {
	.probe = pm800_headset_switch_probe,
	.remove = __devexit_p(pm800_headset_switch_remove),
	.suspend = pm800_headset_switch_suspend,
	.resume = pm800_headset_switch_resume,
	.driver = {
		   .name = "88pm800-headset",
		   .owner = THIS_MODULE,
		   },
};

static const struct file_operations pm800_hsdetect_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = pm800_hsdetect_ioctl,
};

static struct miscdevice pm800_hsdetect_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "88pm800-headset",
	.fops = &pm800_hsdetect_fops,
};

static int __init headset_switch_init(void)
{
	int ret = -EINVAL;
	ret = misc_register(&pm800_hsdetect_miscdev);
	if (ret < 0)
		return ret;
	ret = platform_driver_register(&pm800_headset_switch_driver);
	return ret;
}

module_init(headset_switch_init);
static void __exit headset_switch_exit(void)
{
	platform_driver_unregister(&pm800_headset_switch_driver);
	misc_deregister(&pm800_hsdetect_miscdev);
}

module_exit(headset_switch_exit);

MODULE_DESCRIPTION("Marvell 88PM800 Headset driver");
MODULE_AUTHOR("Wenzeng Chen <wzch@marvell.com>");
MODULE_LICENSE("GPL");
