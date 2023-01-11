/*
 *	sound/soc/codecs/88pm800_headset.c
 *
 *	headset & hook detect driver for pm800
 *
 *	Copyright (C) 2014, Marvell Corporation (zhouqiao@Marvell.com)
 *	Author: Qiao Zhou <zhouqiao@marvell.com>
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License version 2 as
 *	published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/mfd/88pm80x.h>
#include <linux/mfd/88pm8xxx-headset.h>
#include <linux/regulator/consumer.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/mutex.h>

#define PM800_MIC_DET_TH				300
#define PM800_PRESS_RELEASE_TH			300
#define PM800_HOOK_PRESS_TH			20
#define PM800_VOL_UP_PRESS_TH			60
#define PM800_VOL_DOWN_PRESS_TH		110
#define PM800_HK_DELTA		10

#define PM800_HS_DET_INVERT		1
#define PM800_MIC_CNTRL			(0x39)
#define PM800_MICDET_EN			(1 << 0)
#define PM822_HEADSET_DET_MASK		(1 << 7)
#define PM822_GPADC_MEAS_EN2		(0x02)
#define PM822_MIC_DET_MEAS_EN		(1 << 6)
#define PM860_GROUND_DET_REG1		(0x3A)
#define PM860_GROUND_DET_EN		(0x3 << 0)
#define PM860_SHORT_DETECT_REG		(0x37)
#define PM860_SHORT_DETECT_BIT		(0x3)

#define PM860_GPADC_AVG1		(0xAA)
#define PM800_GPADC_AVG1(x)		(0xA0 + 2*(x))
#define PM800_GPADC_MIN1(x)		(0x80 + 2*(x))
#define PM800_GPADC_MAX1(x)		(0x90 + 2*(x))
#define PM800_GPADC_MEAS1(x)		(0x54 + 2*(x))

#define PM800_INT_ENA_3		(0x0B)
#define PM800_INT_ENA_4		(0x0C)
#define PM800_GPIO_INT_ENA4(x)		(1 << (x))
#define PM800_MIC_INT_EN		(1 << 4)
#define PM800_GPADC4_DIR			(1 << 6)

#define PM800_GPADC_LOW_TH(x)		(0x20 + x)
#define PM800_GPADC_UPP_TH(x)		(0x30 + x)

#define PM800_GPIO_CNTRL(x)		(0x30 + ((x) >> 1))
#define PM800_GPIO_VAL(x)		((x)%2 ? (1 << 4) : (1 << 0))

#define MIC_DET_DBS		(3 << 1)
#define MIC_DET_PRD		(3 << 3)
#define MIC_DET_DBS_32MS	(3 << 1)
#define MIC_DET_PRD_CTN		(3 << 3)

struct pm800_hs_info {
	struct pm80x_chip *chip;
	struct device *dev;
	struct regmap *map;
	struct regmap *map_gpadc;
	struct regmap *map_codec;
	int chip_id;
	int irq_headset;
	int irq_hook;
	struct work_struct work_release, work_init;
	struct headset_switch_data *psw_data_headset;
	struct snd_soc_jack *hs_jack, *hk_jack;

	struct regulator *mic_bias;
	int mic_bias_volt;
	int headset_flag;
	int hook_press_th;
	int vol_up_press_th;
	int vol_down_press_th;
	int mic_det_th;
	int press_release_th;
	int hs_status, hk_status;
	unsigned int hk_avg, hk_num;
	u32 hook_count;
	struct mutex hs_mutex;
	struct timer_list hook_timer;
	struct device *hsdetect_dev;
	int headset_gpio_num;
	int mic_gpadc_num;
};

enum {
	VOLTAGE_AVG,
	VOLTAGE_MIN,
	VOLTAGE_MAX,
	VOLTAGE_INS,
};

static struct pm800_hs_info *hs_info;

/* currently we only support button 0, 1, 2 */
static char *pm800_jk_btn[] = {
	"hook",
	"vol up",
	"vol down",
};

static int gpadc_measure_voltage(struct pm800_hs_info *info, int which)
{
	unsigned char buf[2];
	int sum = 0, ret = -1;

	switch (which) {
	case VOLTAGE_AVG:
		if (info->chip->type == CHIP_PM86X)
			ret = regmap_raw_read(info->map_gpadc,
				PM860_GPADC_AVG1, buf, 2);
		else
			ret = regmap_raw_read(info->map_gpadc,
				PM800_GPADC_AVG1(info->mic_gpadc_num), buf, 2);

		break;
	case VOLTAGE_MIN:
		ret = regmap_raw_read(info->map_gpadc,
			PM800_GPADC_MIN1(info->mic_gpadc_num), buf, 2);
		break;
	case VOLTAGE_MAX:
		ret = regmap_raw_read(info->map_gpadc,
			PM800_GPADC_MAX1(info->mic_gpadc_num), buf, 2);
		break;
	case VOLTAGE_INS:
		ret = regmap_raw_read(info->map_gpadc,
			PM800_GPADC_MEAS1(info->mic_gpadc_num), buf, 2);
		break;
	default:
		break;
	}
	if (ret < 0)
		return 0;
	/* GPADC4_dir = 1, measure(mv) = value *1.4 *1000/(2^12) */
	sum = ((buf[0] & 0xFF) << 4) | (buf[1] & 0x0F);
	sum = ((sum & 0xFFF) * 1400) >> 12;
	return sum;
}

static void gpadc_set_threshold(struct pm800_hs_info *info, int min,
				 int max)
{
	unsigned int data;
	if (min <= 0)
		data = 0;
	else
		data = ((min << 12) / 1400) >> 4;
	regmap_write(info->map_gpadc,
		PM800_GPADC_LOW_TH(info->mic_gpadc_num), data);
	if (max <= 0)
		data = 0xFF;
	else
		data = ((max << 12) / 1400) >> 4;
	regmap_write(info->map_gpadc,
		PM800_GPADC_UPP_TH(info->mic_gpadc_num), data);
}

static int pm800_handle_voltage(struct pm800_hs_info *info, int voltage)
{
	int report = 0, button = 0, hk_avg;
	int mask = SND_JACK_BTN_0 | SND_JACK_BTN_1 | SND_JACK_BTN_2;

	if (voltage < info->press_release_th) {
		/* press event */
		if (info->hk_status == 0) {
			if (voltage < info->hook_press_th) {
				report = SND_JACK_BTN_0;
				button = 0;
				/* for telephony */
				kobject_uevent(&info->hsdetect_dev->kobj,
					       KOBJ_ONLINE);
				/* remember hook voltage */
				hk_avg = info->hk_avg * info->hk_num + voltage;
				info->hk_num = info->hk_num + 1;
				info->hk_avg = hk_avg / info->hk_num;
			} else if (voltage < info->vol_up_press_th) {
				report = SND_JACK_BTN_1;
				button = 1;
			} else if (voltage < info->vol_down_press_th) {
				report = SND_JACK_BTN_2;
				button = 2;
			} else
				return -EINVAL;

			snd_soc_jack_report(info->hk_jack, report, mask);
			info->hk_status = report;
			gpadc_set_threshold(info, 0, info->press_release_th);
		} else
			return -EINVAL;
	} else {
		/* release event */
		if (info->hk_status) {
			switch (info->hk_status) {
			case SND_JACK_BTN_0:
				info->hk_jack->jack->type = SND_JACK_BTN_0;
				button = 0;
				/* for telephony */
				kobject_uevent(&info->hsdetect_dev->kobj,
					       KOBJ_ONLINE);
				break;
			case SND_JACK_BTN_1:
				info->hk_jack->jack->type = SND_JACK_BTN_1;
				button = 1;
				break;
			case SND_JACK_BTN_2:
				info->hk_jack->jack->type = SND_JACK_BTN_2;
				button = 2;
				break;
			default:
				return -EINVAL;
			}

			snd_soc_jack_report(info->hk_jack, report, mask);
			info->hk_jack->jack->type = mask;
			info->hk_status = report;
			gpadc_set_threshold(info, info->press_release_th, 0);
		} else
			return -EINVAL;
	}

	trace_printk("%s to %d\n", pm800_jk_btn[button], (report > 0));

	return 0;
}

static void mic_set_volt_micbias(int volt_mv, struct pm800_hs_info *info)
{
	int ret = 0;

	ret = regulator_set_voltage(info->mic_bias,
			volt_mv * 1000,
			volt_mv * 1000);

	if (ret) {
		dev_err(info->dev,
			"failed to set voltage to regulator: %d\n", ret);
		return;
	}
}

static void mic_set_power(int on, struct pm800_hs_info *info)
{
	int ret = 0;

	if (on)
		ret = regulator_enable(info->mic_bias);
	else
		ret = regulator_disable(info->mic_bias);

	if (ret) {
		dev_err(info->dev,
			"failed to enable/disable regulator: %d\n", ret);
		return;
	}

}

static int get_mic_gpadc_num(struct resource *resource)
{
	if (strcmp(resource->name, "gpadc4") == 0)
		return 4;
	else if (strcmp(resource->name, "gpadc3") == 0)
		return 3;
	else if (strcmp(resource->name, "gpadc2") == 0)
		return 2;
	else if (strcmp(resource->name, "gpadc1") == 0)
		return 1;
	else
		return 0;
}

static int get_headset_gpio_num(struct resource *resource)
{
	if (strcmp(resource->name, "gpio-04") == 0)
		return 4;
	else if (strcmp(resource->name, "gpio-03") == 0)
		return 3;
	else if (strcmp(resource->name, "gpio-02") == 0)
		return 2;
	else if (strcmp(resource->name, "gpio-01") == 0)
		return 1;
	else
		return 0;
}

static void pm800_hook_int(struct pm800_hs_info *info, int enable)
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

static void pm800_hook_timer_handler(unsigned long data)
{
	struct pm800_hs_info *info = (struct pm800_hs_info *)data;

	queue_work(system_wq, &info->work_release);
}

static void pm800_hook_release_work(struct work_struct *work)
{
	struct pm800_hs_info *info =
	    container_of(work, struct pm800_hs_info, work_release);
	unsigned int voltage;

	if (info->hk_status) {
		if (info->hs_status == 0) {
			pm800_handle_voltage(info,
				info->press_release_th + 1);
			return;
		}

		voltage = gpadc_measure_voltage(info, VOLTAGE_AVG);
		if (voltage >= info->press_release_th)
			pm800_handle_voltage(info, voltage);
	}
}

static void pm800_hook_work(struct pm800_hs_info *info)
{
	int ret;
	unsigned int value, voltage;

	msleep(50);
	if (info->chip->type == CHIP_PM822 || info->chip->type == CHIP_PM86X) {
		ret = regmap_read(info->map, PM800_MIC_CNTRL, &value);
		if (ret < 0) {
			dev_err(info->dev,
				"Failed to read PM822_MIC_CTRL: 0x%x\n", ret);
			return;
		}
		value &= PM822_HEADSET_DET_MASK;
	} else {
		ret = regmap_read(info->map,
			PM800_GPIO_CNTRL(info->headset_gpio_num), &value);
		if (ret < 0) {
			dev_err(info->dev,
				"Failed to read PM800_GPIO%d_CNTRL: 0x%x\n",
					info->headset_gpio_num, ret);
			return;
		}
		value &= PM800_GPIO_VAL(info->headset_gpio_num);
	}

	if (info->headset_flag == PM800_HS_DET_INVERT)
		value = !value;

	if (!value) {
		/* in case of headset unpresent, do nothing */
		trace_printk("hook: false exit\n");
		goto FAKE_HOOK;
	}

	voltage = gpadc_measure_voltage(info, VOLTAGE_AVG);

	trace_printk("voltage %d avg %d\n", voltage, info->hk_avg);
	/*
	 * below numbers are got by experience:
	 * SS hs: hook vol = ~50; false hook = ~35, so set false_th = 40
	 * iphone hs: hook vol = 5; false_hook = ~33.
	 * these value can be adjust for specific hs
	 */
	if (voltage < info->hook_press_th) {
		if (info->hk_avg
		    && (abs(info->hk_avg - voltage) > PM800_HK_DELTA))
			goto FAKE_HOOK;
		else if (!info->hk_avg)
			if (voltage > 10 && voltage < info->hook_press_th - 20)
				goto FAKE_HOOK;
	}

	pm800_handle_voltage(info, voltage);
	regmap_update_bits(info->map, PM800_INT_ENA_3,
			PM800_MIC_INT_EN,
			PM800_MIC_INT_EN);
	return;

FAKE_HOOK:
	if (value)
		regmap_update_bits(info->map, PM800_INT_ENA_3,
			PM800_MIC_INT_EN,
			PM800_MIC_INT_EN);

	dev_err(info->dev, "fake hook interupt\n");
}

static void check_headset_short(struct pm800_hs_info *info)
{
	unsigned int value;
	regmap_read(info->map_codec, PM860_SHORT_DETECT_REG, &value);

	if (!(value & PM860_SHORT_DETECT_BIT))
		return;
	else
		regmap_update_bits(info->map_codec, PM860_SHORT_DETECT_REG,
			PM860_SHORT_DETECT_BIT, 0);
}

static void pm800_headset_work(struct pm800_hs_info *info)
{
	unsigned int value, voltage;
	int ret, report = 0;

	if (info == NULL)
		return;

	if (info->chip->type == CHIP_PM822 || info->chip->type == CHIP_PM86X) {
		ret = regmap_read(info->map, PM800_MIC_CNTRL, &value);
		if (ret < 0) {
			dev_err(info->dev,
				"Failed to read PM822_MIC_CTRL: 0x%x\n", ret);
			return;
		}
		value &= PM822_HEADSET_DET_MASK;
	} else {
		ret = regmap_read(info->map,
			PM800_GPIO_CNTRL(info->headset_gpio_num), &value);
		if (ret < 0) {
			dev_err(info->dev,
				"Failed to read PM800_GPIO%d_CNTRL: 0x%x\n",
					info->headset_gpio_num, ret);
			return;
		}
		value &= PM800_GPIO_VAL(info->headset_gpio_num);
	}
	if (info->headset_flag == PM800_HS_DET_INVERT)
		value = !value;

	if (value) {
		report = SND_JACK_HEADSET;
		/* for telephony */
		kobject_uevent(&info->hsdetect_dev->kobj, KOBJ_ADD);
		if (info->mic_bias) {
			mic_set_power(1, info);
			/* for enable iphone headset, it need higher voltage */
			mic_set_volt_micbias(2500, info);
		}
		/* enable MIC detection also enable measurement */
		regmap_update_bits(info->map, PM800_MIC_CNTRL, PM800_MICDET_EN,
				   PM800_MICDET_EN);
		regmap_update_bits(info->map_gpadc, PM822_GPADC_MEAS_EN2,
			PM822_MIC_DET_MEAS_EN, PM822_MIC_DET_MEAS_EN);
		/* enable auto ground detect for 88pm860 */
		if (info->chip->type == CHIP_PM86X)
			regmap_update_bits(info->map, PM860_GROUND_DET_REG1,
				PM860_GROUND_DET_EN, PM860_GROUND_DET_EN);
		msleep(200);
		voltage = gpadc_measure_voltage(info, VOLTAGE_AVG);
		if (voltage < info->mic_det_th) {
			report = SND_JACK_HEADPHONE;
			if (info->mic_bias)
				mic_set_power(0, info);

			/* disable MIC detection and measurement */
			regmap_update_bits(info->map, PM800_MIC_CNTRL,
					   PM800_MICDET_EN, 0);
			regmap_update_bits(info->map_gpadc, PM822_GPADC_MEAS_EN2,
				PM822_MIC_DET_MEAS_EN, 0);
			/* disable auto ground detect for 88pm860 */
			if (info->chip->type == CHIP_PM86X)
				regmap_update_bits(info->map,
					PM860_GROUND_DET_REG1,
					PM860_GROUND_DET_EN, 0);
		} else
			gpadc_set_threshold(info, info->press_release_th, 0);
		info->hs_status = report;
	} else {
		/* we already disable mic power when it is headphone */
		if ((info->hs_status == SND_JACK_HEADSET) && info->mic_bias)
			mic_set_power(0, info);

		/* disable MIC detection and measurement */
		regmap_update_bits(info->map, PM800_MIC_CNTRL, PM800_MICDET_EN,
				   0);
		regmap_update_bits(info->map_gpadc, PM822_GPADC_MEAS_EN2,
			PM822_MIC_DET_MEAS_EN, 0);
		/* disable auto ground detect for 88pm860 */
		if (info->chip->type == CHIP_PM86X)
			regmap_update_bits(info->map, PM860_GROUND_DET_REG1,
				PM860_GROUND_DET_EN, 0);

		info->hs_status = report;

		/* for telephony */
		kobject_uevent(&info->hsdetect_dev->kobj, KOBJ_REMOVE);
		/* clear the hook voltage record */
		info->hk_avg = 0;
		info->hk_num = 0;
		/* headset plug out, we can check the headset short status */
		if (info->chip->type == CHIP_PM86X)
			check_headset_short(info);
	}

	mic_set_volt_micbias(info->mic_bias_volt, info);
	snd_soc_jack_report(info->hs_jack, report, SND_JACK_HEADSET);
	trace_printk("hs status: %d\n", report);

	/* enable hook irq if headset is present */
	if (info->hs_status == SND_JACK_HEADSET)
		pm800_hook_int(info, 1);
}

static irqreturn_t pm800_headset_handler(int irq, void *data)
{
	struct pm800_hs_info *info = data;

	if (info->hs_jack != NULL) {
		/*
		 * in any case headset interrupt comes,
		 * hook interrupt is not welcomed. so just
		 * disable it.
		 */
		pm800_hook_int(info, 0);
		pm800_headset_work(info);
	}

	return IRQ_HANDLED;
}

static irqreturn_t pm800_hook_handler(int irq, void *data)
{
	struct pm800_hs_info *info = data;

	mod_timer(&info->hook_timer, jiffies + msecs_to_jiffies(150));

	if (info->hk_jack != NULL) {
		/* disable hook interrupt anyway */
		regmap_update_bits(info->map, PM800_INT_ENA_3,
			PM800_MIC_INT_EN, 0);
		pm800_hook_work(info);
	}

	return IRQ_HANDLED;
}

/*
 * separate hs & hk, since snd_jack_report reports both key & switch. just to
 * avoid confusion in upper layer. when reporting hs, only headset/headphone/
 * mic status are reported. when reporting hk, hook/vol up/vol down status are
 * reported.
 */
int pm800_headset_detect(struct snd_soc_jack *jack)
{
	if (!hs_info)
		return -EINVAL;

	/* Store the configuration */
	hs_info->hs_jack = jack;

	/* detect headset status during boot up */
	queue_work(system_wq, &hs_info->work_init);

	return 0;
}
EXPORT_SYMBOL_GPL(pm800_headset_detect);

int pm800_hook_detect(struct snd_soc_jack *jack)
{
	if (!hs_info)
		return -EINVAL;

	/* Store the configuration */
	hs_info->hk_jack = jack;

	return 0;
}
EXPORT_SYMBOL_GPL(pm800_hook_detect);

static void pm800_init_work(struct work_struct *work)
{
	struct pm800_hs_info *info =
	    container_of(work, struct pm800_hs_info, work_init);
	/* headset status is not stable when boot up. wait 200ms */
	msleep(200);
	pm800_headset_handler(info->irq_headset, info);
}

static int pm800_headset_suspend(struct platform_device *pdev,
					pm_message_t state)
{
	struct pm800_hs_info *info = platform_get_drvdata(pdev);

	/* it's not proper to use "pm80x_dev_suspend" here */
	if (device_may_wakeup(&pdev->dev)) {
		enable_irq_wake(info->irq_headset);
		if (info->hs_status == SND_JACK_HEADSET)
			enable_irq_wake(info->irq_hook);
	}

	/* enable sleep detect mode, z3's chip do not support */
	if (info->chip_id != CHIP_PM86X_ID_Z3)
		regmap_update_bits(hs_info->map, PM800_HEADSET_CNTRL,
				PM800_HSDET_SLP,
				PM800_HSDET_SLP);

	return 0;
}

static int pm800_headset_resume(struct platform_device *pdev)
{
	struct pm800_hs_info *info = platform_get_drvdata(pdev);

	if (device_may_wakeup(&pdev->dev)) {
		disable_irq_wake(info->irq_headset);
		if (info->hs_status == SND_JACK_HEADSET)
			disable_irq_wake(info->irq_hook);
	}

	/* disable sleep detect mode, z3's chip do not support */
	if (info->chip_id != CHIP_PM86X_ID_Z3)
		regmap_update_bits(hs_info->map, PM800_HEADSET_CNTRL,
				PM800_HSDET_SLP,
				0);

	return 0;
}

static struct regmap *pm80x_get_companion(struct pm80x_chip *chip)
{
	struct pm80x_chip *chip_comp;

	if (!chip->companion) {
		dev_err(chip->dev, "%s: no companion chip\n", __func__);
		return NULL;
	}

	chip_comp = i2c_get_clientdata(chip->companion);

	return chip_comp->regmap;
}

static int pm800_headset_probe(struct platform_device *pdev)
{
	struct pm80x_chip *chip = dev_get_drvdata(pdev->dev.parent);
	int irq_headset, irq_hook, ret = 0;
	struct resource *headset_resource, *mic_resource;

	headset_resource = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!headset_resource) {
		dev_err(&pdev->dev, "No resource for headset!\n");
		return -EINVAL;
	}

	mic_resource = platform_get_resource(pdev, IORESOURCE_IRQ, 1);
	if (!mic_resource) {
		dev_err(&pdev->dev, "No resource for mic!\n");
		return -EINVAL;
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
	hs_info =
	    devm_kzalloc(&pdev->dev, sizeof(struct pm800_hs_info),
			 GFP_KERNEL);
	if (!hs_info)
		return -ENOMEM;

	hs_info->chip = chip;
	hs_info->mic_bias = regulator_get(&pdev->dev, "marvell,micbias");
	hs_info->headset_gpio_num = get_headset_gpio_num(headset_resource);
	/* 88pm860 GPADC register num is bigger than 88pm822's */
	hs_info->mic_gpadc_num = get_mic_gpadc_num(mic_resource);

	if (IS_ERR(hs_info->mic_bias)) {
		hs_info->mic_bias = NULL;
		dev_err(&pdev->dev, "Get regulator error\n");
	}

	if (IS_ENABLED(CONFIG_OF)) {
		if (of_property_read_u32(pdev->dev.of_node,
			"marvell,headset-flag", &hs_info->headset_flag))
			dev_dbg(&pdev->dev, "Do not get headset flag\n");
		if (of_property_read_u32(pdev->dev.of_node,
			"marvell,hook-press-th", &hs_info->hook_press_th))
			dev_dbg(&pdev->dev,
				"Do not get hook press threshold\n");
		if (of_property_read_u32(pdev->dev.of_node,
			"marvell,vol-up-press-th", &hs_info->vol_up_press_th))
			dev_dbg(&pdev->dev, "Do not get vol up threshold\n");
		if (of_property_read_u32(pdev->dev.of_node,
					 "marvell,vol-down-press-th",
					 &hs_info->vol_down_press_th))
			dev_dbg(&pdev->dev, "Do not get vol down threshold\n");
		if (of_property_read_u32(pdev->dev.of_node,
			"marvell,mic-det-th", &hs_info->mic_det_th))
			dev_dbg(&pdev->dev,
				"Do not get mic detect threshold\n");
		if (of_property_read_u32(pdev->dev.of_node,
			"marvell,press-release-th", &hs_info->press_release_th))
			dev_dbg(&pdev->dev,
				"Do not get press release threshold\n");
		if (of_property_read_u32(pdev->dev.of_node,
			"marvell,micbias-volt", &hs_info->mic_bias_volt))
			dev_dbg(&pdev->dev, "Do not get micbias voltage\n");
	} else {
		hs_info->hook_press_th = PM800_HOOK_PRESS_TH;
		hs_info->vol_up_press_th = PM800_VOL_UP_PRESS_TH;
		hs_info->vol_down_press_th = PM800_VOL_DOWN_PRESS_TH;
		hs_info->mic_det_th = PM800_MIC_DET_TH;
		hs_info->press_release_th = PM800_PRESS_RELEASE_TH;
		hs_info->mic_bias_volt = 1700;
	}

	hs_info->map = chip->regmap;
	hs_info->chip_id = chip->chip_id;
	hs_info->map_gpadc = chip->subchip->regmap_gpadc;
	hs_info->map_codec = pm80x_get_companion(chip);
	hs_info->dev = &pdev->dev;
	hs_info->irq_headset = regmap_irq_get_virq(chip->irq_data, irq_headset);
	hs_info->irq_hook = regmap_irq_get_virq(chip->irq_data, irq_hook);

	hs_info->hk_status = 0;
	hs_info->hk_avg = 0;
	hs_info->hk_num = 0;

	mutex_init(&hs_info->hs_mutex);

	INIT_WORK(&hs_info->work_release, pm800_hook_release_work);
	INIT_WORK(&hs_info->work_init, pm800_init_work);

	/* init timer for hook release */
	setup_timer(&hs_info->hook_timer, pm800_hook_timer_handler,
		    (unsigned long)hs_info);

	ret = devm_request_threaded_irq(&pdev->dev, hs_info->irq_headset,
			NULL, pm800_headset_handler,
			IRQF_ONESHOT | IRQF_NO_SUSPEND, "headset", hs_info);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to request IRQ: #%d: %d\n",
			hs_info->irq_headset, ret);
		goto err_out;
	}

	ret = devm_request_threaded_irq(&pdev->dev, hs_info->irq_hook,
			NULL, pm800_hook_handler,
			IRQF_ONESHOT | IRQF_NO_SUSPEND, "hook", hs_info);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to request IRQ: #%d: %d\n",
			hs_info->irq_hook, ret);
		goto err_out;
	}

	/*
	 * disable hook irq to ensure this irq will be enabled
	 * after plugging in headset
	 */
	hs_info->hook_count = 1;
	pm800_hook_int(hs_info, 0);

	platform_set_drvdata(pdev, hs_info);
	hs_info->hsdetect_dev = &pdev->dev;

	/* Hook:32 ms debounce time */
	regmap_update_bits(hs_info->map, PM800_MIC_CNTRL, MIC_DET_DBS,
			   MIC_DET_DBS_32MS);
	/* Hook:continue duty cycle */
	regmap_update_bits(hs_info->map, PM800_MIC_CNTRL, MIC_DET_PRD,
			   MIC_DET_PRD_CTN);
	/* set GPADC_DIR to 1, set to 0 cause pop noise in recording */
	regmap_update_bits(hs_info->map_gpadc, PM800_GPADC_MISC_CONFIG1,
			   PM800_GPADC4_DIR, PM800_GPADC4_DIR);

	disable_irq(hs_info->irq_headset);
	regmap_update_bits(hs_info->map, PM800_HEADSET_CNTRL,
			   PM800_HEADSET_DET_EN, PM800_HEADSET_DET_EN);
	enable_irq(hs_info->irq_headset);

	device_init_wakeup(&pdev->dev, 1);

	if (ret < 0)
		return ret;
	headset_detect = pm800_headset_detect;
	hook_detect = pm800_hook_detect;

	return 0;

err_out:
	return ret;
}

static int pm800_headset_remove(struct platform_device *pdev)
{
	struct pm800_hs_info *hs_info = platform_get_drvdata(pdev);

	/* disable headset detection on GPIO */
	regmap_update_bits(hs_info->map, PM800_HEADSET_CNTRL,
			   PM800_HEADSET_DET_EN, 0);
	/* disable GPIO interrupt */
	regmap_update_bits(hs_info->map, PM800_INT_ENA_4,
		PM800_GPIO_INT_ENA4(hs_info->headset_gpio_num), 0);

	cancel_work_sync(&hs_info->work_release);
	cancel_work_sync(&hs_info->work_init);

	devm_kfree(&pdev->dev, hs_info);

	return 0;
}

static void pm800_headset_shutdown(struct platform_device *pdev)
{
	devm_free_irq(&pdev->dev, hs_info->irq_headset, hs_info);
	devm_free_irq(&pdev->dev, hs_info->irq_hook, hs_info);

	pm800_headset_remove(pdev);
	return;
}

static struct platform_driver pm800_headset_driver = {
	.probe = pm800_headset_probe,
	.remove = pm800_headset_remove,
	.shutdown = pm800_headset_shutdown,
	.suspend = pm800_headset_suspend,
	.resume = pm800_headset_resume,
	.driver = {
		   .name = "88pm800-headset",
		   .owner = THIS_MODULE,
		   },
};

module_platform_driver(pm800_headset_driver);

MODULE_DESCRIPTION("Marvell 88PM800 Headset driver");
MODULE_AUTHOR("Qiao Zhou <zhouqiao@marvell.com>");
MODULE_LICENSE("GPL");
