/*
 *	sound/soc/codecs/88pm886_headset.c
 *
 *	headset & hook detect driver for pm886
 *
 *	Copyright (C) 2014, Marvell Corporation
 *	Author: Zhao Ye <zhaoy@marvell.com>
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License version 2 as
 *	published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/mfd/88pm8xxx-headset.h>
#include <linux/regulator/consumer.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/mutex.h>
#include <linux/mfd/88pm88x.h>
#include <linux/mfd/88pm886.h>
#if defined (SEC_USE_SAMSUNG_JACK_API)
#if defined (CONFIG_SWITCH)
#include <linux/switch.h>
#else
#include <linux/extcon.h>
#endif
#endif

#define PM88X_GPIO_CTRL4		(0x33)
#define PM88X_HSDET3_MODE		(5 << 5)
#define PM88X_GNDDET2			(0x3b)
#define PM88X_HSDET_SAFE		(1 << 7)

#define PM886_GPIO_CTRL4		(0x35)
#define PM886_HSDET3_MODE		(5 << 5)
#define PM886_GNDDET2			(0x3b)
#define PM886_HSDET_SAFE		(1 << 7)
#define PM886_HEADSET_CNTRL		(0x38)
#define PM886_CONTINUOUSE_PERIOD	(3 << 2)
#define PM886_HEADSET_DET_EN		(1 << 7)
#define PM886_HEADSET_AUTO		(1 << 6)
#define PM886_HSDET_SLP			(1 << 1)
#define PM886_MIC_DET_TH		300
#define PM886_PRESS_RELEASE_TH		300
#define PM886_HOOK_PRESS_TH		20
#define PM886_VOC_CMD_PRESS_TH		40   //voc cmd
#define PM886_VOL_UP_PRESS_TH		60
#define PM886_VOL_DOWN_PRESS_TH		110
#define PM886_HK_DELTA			10

#define PM886_HS_DET_INVERT		1
#define PM886_MIC_CNTRL			(0x39)
#define PM886_MICDET_EN			(1 << 0)
#define PM886_HEADSET_DET_MASK		(1 << 7)
#define PM886_GPADC_MEAS_EN2		(0x02)
#define PM886_MIC_DET_MEAS_EN		(1 << 6)
#define PM860_SHORT_DETECT_REG		(0x37)
#define PM860_SHORT_DETECT_BIT		(0x3)


#define PM880_HEADSET_CNTRL3		(0x3C)
#define PM880_MIC_BIAS_AUTO_MUTE_EN		(1 << 4)

#define PM886_GPADC_AVG1		(0xAC)
#define PM886_GPADC_MIN1		(0x89)
#define PM886_GPADC_MAX1		(0x99)
#define PM886_GPADC_MEAS1		(0x5c)
#define PM886_GPADC_CONFIG5		(0x05)

#define PM886_INT_ENA_3			(0x0C)
#define PM886_MIC_INT_EN		(1 << 4)
#define PM886_MIC_DET_RDIV_DIS		(1 << 6)

#define PM886_GPADC_LOW_TH		(0x24)
#define PM886_GPADC_UPP_TH		(0x34)

#define MIC_DET_DBS		(3 << 1)
#define MIC_DET_PRD		(3 << 3)
#define MIC_DET_DBS_32MS	(3 << 1)
#define MIC_DET_PRD_CTN		(3 << 3)

struct pm886_hs_info {
	struct pm88x_chip *chip;
	struct device *dev;
	struct regmap *map;
	struct regmap *map_gpadc;
	struct regmap *map_codec;
	int irq_headset;
	int irq_hook;
	struct work_struct work_release, work_init;
	struct headset_switch_data *psw_data_headset;
	struct snd_soc_jack *hs_jack, *hk_jack;

	struct regulator *mic_bias;
	int mic_bias_volt;
	int mic_bias_range;
	int headset_flag;
	int gpio5v_detect_mode;
	int hook_press_th;
	int vol_up_press_th;
	int vol_down_press_th;
#ifdef SEC_USE_VOICE_ASSIST_KEY
	int voc_command_press_th;  //KSND voc cmd
#endif
	int mic_det_th;
	int press_release_th;
	int hs_status, hk_status;
	unsigned int hk_avg, hk_num;
	u32 hook_count;
	struct mutex hs_mutex;
	struct timer_list hook_timer;
	struct device *hsdetect_dev;
#if defined (SEC_USE_HS_SYSFS)
	int hk_adc;
#endif
};

enum {
	VOLTAGE_AVG,
	VOLTAGE_MIN,
	VOLTAGE_MAX,
	VOLTAGE_INS,
};

static struct pm886_hs_info *hs_info;

/* currently we only support button 0, 1, 2, 3  */
static char *pm886_jk_btn[] = {
	"hook",
	"vol up",
	"vol down",
	"voc assist"
};

static char *sec_jack_status[] = {
	"NONE",
	"HEADSET_4POLE",
	"HEADSET_3POLE",
};

#if defined (SEC_USE_SAMSUNG_JACK_API)
#if defined (CONFIG_SWITCH)
struct switch_dev switch_jack_detection = {
	.name = "h2w",
};
#else
struct extcon_dev extcon_jack_detection = {
	.name = "h2w",
};
#endif
#endif

static int gpadc_measure_voltage(struct pm886_hs_info *info, int which)
{
	unsigned char buf[2];
	int sum = 0, ret = -1;

	switch (which) {
	case VOLTAGE_AVG:
		ret = regmap_raw_read(info->map_gpadc,
				PM886_GPADC_AVG1, buf, 2);
		break;
	case VOLTAGE_MIN:
		ret = regmap_raw_read(info->map_gpadc,
				PM886_GPADC_MIN1, buf, 2);
		break;
	case VOLTAGE_MAX:
		ret = regmap_raw_read(info->map_gpadc,
				PM886_GPADC_MAX1, buf, 2);
		break;
	case VOLTAGE_INS:
		ret = regmap_raw_read(info->map_gpadc,
				PM886_GPADC_MEAS1, buf, 2);
		break;
	default:
		break;
	}
	if (ret < 0)
		return 0;
	/* GPADC4_dir = 1, measure(mv) = value *1.4 *1000/(2^12) */
	sum = ((buf[0] & 0xFF) << 4) | (buf[1] & 0x0F);
	sum = ((sum & 0xFFF) * info->mic_bias_range) >> 12;
	return sum;
}

static void gpadc_set_threshold(struct pm886_hs_info *info, int min,
				 int max)
{
	unsigned int data;
	if (min <= 0)
		data = 0;
	else
		data = ((min << 12) / info->mic_bias_range) >> 4;
	regmap_write(info->map_gpadc,
		PM886_GPADC_LOW_TH, data);
	if (max <= 0)
		data = 0xFF;
	else
		data = ((max << 12) / info->mic_bias_range) >> 4;
	regmap_write(info->map_gpadc,
		PM886_GPADC_UPP_TH, data);
}

static int pm886_handle_voltage(struct pm886_hs_info *info, int voltage)
{
	int report = 0, button = 0, hk_avg;
	int mask = SND_JACK_BTN_0 | SND_JACK_BTN_1 | SND_JACK_BTN_2 | SND_JACK_BTN_3;

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
#ifdef SEC_USE_VOICE_ASSIST_KEY
			} else if (voltage < info->voc_command_press_th) {  //for voc cmd key
				report = SND_JACK_BTN_3;
				button = 3;
#endif
			} else if (voltage < info->vol_up_press_th) {
				report = SND_JACK_BTN_1;
				button = 1;
			} else if (voltage < info->vol_down_press_th) {
				report = SND_JACK_BTN_2;
				button = 2;
			} else
				return -EINVAL;

			snd_soc_jack_report(info->hk_jack, report, mask);
#if defined (SEC_USE_HS_SYSFS)
			info->hk_adc = voltage;
#endif			
			info->hk_status = report;
			gpadc_set_threshold(info, 0, info->press_release_th);
  			pr_info("%s: [%s] is pressed![%d mV]\n", __func__, pm886_jk_btn[button], voltage);
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
#ifdef SEC_USE_VOICE_ASSIST_KEY
			case SND_JACK_BTN_3: //for voc cmd key
				info->hk_jack->jack->type = SND_JACK_BTN_3;
				button = 3;
				break;
#endif
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
#if defined (SEC_USE_HS_SYSFS)
			info->hk_adc = 0;
#endif			
			info->hk_status = report;
			gpadc_set_threshold(info, info->press_release_th, 0);

   			pr_info("%s: [%s] is released![%d mV]\n", __func__, pm886_jk_btn[button], voltage);
		} else
			return -EINVAL;
	}

	trace_printk("%s to %d\n", pm886_jk_btn[button], (report > 0));

	return 0;
}

static void mic_set_volt_micbias(int volt_mv, struct pm886_hs_info *info)
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

static void mic_set_power(int on, struct pm886_hs_info *info)
{
	printk("mic_set_power do nothing");
	return;
}

static void pm886_hook_int(struct pm886_hs_info *info, int enable)
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

static void pm886_hook_timer_handler(unsigned long data)
{
	struct pm886_hs_info *info = (struct pm886_hs_info *)data;

	queue_work(system_wq, &info->work_release);
}

static void pm886_hook_release_work(struct work_struct *work)
{
	struct pm886_hs_info *info =
	    container_of(work, struct pm886_hs_info, work_release);
	unsigned int voltage;

	if (info->hk_status) {
		if (info->hs_status == 0) {
			pm886_handle_voltage(info,
				info->press_release_th + 1);
			return;
		}

		voltage = gpadc_measure_voltage(info, VOLTAGE_AVG);
		if (voltage >= info->press_release_th)
			pm886_handle_voltage(info, voltage);
	}
}

static void pm886_hook_work(struct pm886_hs_info *info)
{
	int ret;
	unsigned int value, voltage;

	msleep(50);    //KSND Skip the event within 50ms
	voltage = gpadc_measure_voltage(info, VOLTAGE_AVG);

	ret = regmap_read(info->map, PM886_MIC_CNTRL, &value);
	if (ret < 0) {
		dev_err(info->dev,
			"Failed to read PM886_MIC_CTRL: 0x%x\n", ret);
		return;
	}
	value &= PM886_HEADSET_DET_MASK;

	if (info->headset_flag == PM886_HS_DET_INVERT)
		value = !value;

	if (!value) {
		/* in case of headset unpresent, do nothing */
		trace_printk("hook: false exit\n");
		goto FAKE_HOOK;
	}

	trace_printk("voltage %d avg %d\n", voltage, info->hk_avg);
	/*
	 * below numbers are got by experience:
	 * SS hs: hook vol = ~50; false hook = ~35, so set false_th = 40
	 * iphone hs: hook vol = 5; false_hook = ~33.
	 * these value can be adjust for specific hs
	 * on pxa1908 hook voltage is ~27.
	 */
#if 0 //Remove the workaround for J1LTE
	if (voltage < info->hook_press_th) {
		if (info->hk_avg
		    && (abs(info->hk_avg - voltage) > PM886_HK_DELTA))
			goto FAKE_HOOK;
		else if (!info->hk_avg)
			if (voltage > 35 && voltage < info->hook_press_th - 20)
				goto FAKE_HOOK;
	}
#endif

	pm886_handle_voltage(info, voltage);
	regmap_update_bits(info->map, PM886_INT_ENA_3,
			PM886_MIC_INT_EN,
			PM886_MIC_INT_EN);
	return;

FAKE_HOOK:
		regmap_update_bits(info->map, PM886_INT_ENA_3,
			PM886_MIC_INT_EN,
			PM886_MIC_INT_EN);

	dev_err(info->dev, "fake hook interupt\n");
}

static void check_headset_short(struct pm886_hs_info *info)
{
	unsigned int value;
	regmap_read(info->map_codec, PM860_SHORT_DETECT_REG, &value);

	if (!(value & PM860_SHORT_DETECT_BIT))
		return;
	else
		regmap_update_bits(info->map_codec, PM860_SHORT_DETECT_REG,
			PM860_SHORT_DETECT_BIT, 0);
}

static void pm886_headset_work(struct pm886_hs_info *info)
{
	unsigned int value, voltage = 0;
	int ret, report = 0;
#if defined (SEC_USE_SAMSUNG_JACK_API)
    int jack_type = 0;
#endif

	if (info == NULL)
		return;

	ret = regmap_read(info->map, PM886_MIC_CNTRL, &value);
	if (ret < 0) {
		dev_err(info->dev,
			"Failed to read PM886_MIC_CTRL: 0x%x\n", ret);
		return;
	}

	value &= PM886_HEADSET_DET_MASK;

	if (info->headset_flag == PM886_HS_DET_INVERT)
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
		regmap_update_bits(info->map, PM886_MIC_CNTRL, PM886_MICDET_EN,
				   PM886_MICDET_EN);
		regmap_update_bits(info->map_gpadc, PM886_GPADC_MEAS_EN2,
			PM886_MIC_DET_MEAS_EN, PM886_MIC_DET_MEAS_EN);
		msleep(200);
		voltage = gpadc_measure_voltage(info, VOLTAGE_AVG);

		if (voltage < info->mic_det_th) {
			report = SND_JACK_HEADPHONE;
			if (info->mic_bias)
				mic_set_power(0, info);

			/* disable MIC detection and measurement */
			regmap_update_bits(info->map, PM886_MIC_CNTRL,
					   PM886_MICDET_EN, 0);
			regmap_update_bits(info->map_gpadc,
					PM886_GPADC_MEAS_EN2,
					PM886_MIC_DET_MEAS_EN, 0);
		} else
			gpadc_set_threshold(info, info->press_release_th, 0);
		info->hs_status = report;
	} else {
		/* we already disable mic power when it is headphone */
		if ((info->hs_status == SND_JACK_HEADSET) && info->mic_bias)
			mic_set_power(0, info);

		/* disable MIC detection and measurement */
		regmap_update_bits(info->map, PM886_MIC_CNTRL, PM886_MICDET_EN,
				   0);
		regmap_update_bits(info->map_gpadc, PM886_GPADC_MEAS_EN2,
			PM886_MIC_DET_MEAS_EN, 0);

		info->hs_status = report;

		/* for telephony */
		kobject_uevent(&info->hsdetect_dev->kobj, KOBJ_REMOVE);
		/* clear the hook voltage record */
		info->hk_avg = 0;
		info->hk_num = 0;
		/* headset plug out, we can check the headset short status */
		check_headset_short(info);
	}

	mic_set_volt_micbias(info->mic_bias_volt, info);

#if defined (SEC_USE_SAMSUNG_JACK_API)
    //Convert type
    if( report == SND_JACK_HEADSET ){
        jack_type = SEC_HEADSET_4POLE;
    } else if( report == SND_JACK_HEADPHONE ){
        jack_type = SEC_HEADSET_3POLE;
    } else {
        jack_type = SEC_JACK_NO_DEVICE;
    }
#if defined (CONFIG_SWITCH)
	switch_set_state(&switch_jack_detection, jack_type);
#else	
    extcon_set_state(&extcon_jack_detection, jack_type);
#endif
#endif
    snd_soc_jack_report(info->hs_jack, report, SND_JACK_HEADSET);
	trace_printk("hs status: %d\n", report);

	pr_info("%s: %s State [%d mV] \n",__func__ , sec_jack_status[jack_type],voltage );

	/* enable hook irq if headset is present */
	if (info->hs_status == SND_JACK_HEADSET)
		pm886_hook_int(info, 1);
}

static irqreturn_t pm886_headset_handler(int irq, void *data)
{
	struct pm886_hs_info *info = data;

	pr_info("%s: KSND Headset Interrupt!! \n", __func__ );

	if (info->hs_jack != NULL) {
		/*
		 * in any case headset interrupt comes,
		 * hook interrupt is not welcomed. so just
		 * disable it.
		 */
		pm886_hook_int(info, 0);
		pm886_headset_work(info);
	}

	return IRQ_HANDLED;
}

static irqreturn_t pm886_hook_handler(int irq, void *data)
{
	struct pm886_hs_info *info = data;

	pr_info("%s: KSND Hook Interrupt!! \n", __func__ );

	mod_timer(&info->hook_timer, jiffies + msecs_to_jiffies(150));

	if (info->hk_jack != NULL) {
		/* disable hook interrupt anyway */
		regmap_update_bits(info->map, PM886_INT_ENA_3,
			PM886_MIC_INT_EN, 0);
		pm886_hook_work(info);
	}

	return IRQ_HANDLED;
}

/*
 * separate hs & hk, since snd_jack_report reports both key & switch. just to
 * avoid confusion in upper layer. when reporting hs, only headset/headphone/
 * mic status are reported. when reporting hk, hook/vol up/vol down status are
 * reported.
 */
int pm886_headset_detect(struct snd_soc_jack *jack)
{
	if (!hs_info)
		return -EINVAL;

	/* Store the configuration */
	hs_info->hs_jack = jack;

	/* detect headset status during boot up */
	queue_work(system_wq, &hs_info->work_init);

	return 0;
}
EXPORT_SYMBOL_GPL(pm886_headset_detect);

int pm886_hook_detect(struct snd_soc_jack *jack)
{
	if (!hs_info)
		return -EINVAL;

	/* Store the configuration */
	hs_info->hk_jack = jack;

	return 0;
}
EXPORT_SYMBOL_GPL(pm886_hook_detect);

#if defined (SEC_USE_HS_SYSFS)
static ssize_t  key_state_onoff_show(struct device *dev,	struct device_attribute *attr, char *buf)
{
	struct pm886_hs_info *info = (struct pm886_hs_info *)dev_get_drvdata(dev);
	return sprintf(buf, "%s\n", (info->hk_status)? "1":"0");
}

static ssize_t  earjack_state_onoff_show(struct device *dev,	struct device_attribute *attr, char *buf)
{
	struct pm886_hs_info *info = (struct pm886_hs_info *)dev_get_drvdata(dev);
	return sprintf(buf, "%s\n", (info->hs_status) ? "1":"0");
}

static ssize_t  mic_adc_show(struct device *dev,	struct device_attribute *attr, char *buf)
{
	struct pm886_hs_info *info = (struct pm886_hs_info *)dev_get_drvdata(dev);
    return sprintf(buf, "%d\n",info->hk_adc);
}

static DEVICE_ATTR(key_state, 0444 , key_state_onoff_show, NULL);
static DEVICE_ATTR(state, 0444 , earjack_state_onoff_show, NULL);
static DEVICE_ATTR(mic_adc, 0444 , mic_adc_show, NULL);

#if defined (SEC_USE_HS_INFO_SYSFS)
static ssize_t earjack_adc_show(struct device *dev,	struct device_attribute *attr, char *buf)
{
	struct pm886_hs_info *info = (struct pm886_hs_info *)dev_get_drvdata(dev);

	int voltage = 0;
	mic_set_power(1, info);

	/* enable MIC detection also enable measurement */
	regmap_update_bits(info->map, PM886_MIC_CNTRL, PM886_MICDET_EN,
			PM886_MICDET_EN);
	regmap_update_bits(info->map_gpadc, PM886_GPADC_MEAS_EN2,
			PM886_MIC_DET_MEAS_EN, PM886_MIC_DET_MEAS_EN);
	msleep(200);

	voltage = gpadc_measure_voltage(info, VOLTAGE_AVG);
	return sprintf(buf, "%d mV\n", voltage);
}

static ssize_t hook_adc_show(struct device *dev,	struct device_attribute *attr, char *buf)
{
	struct pm886_hs_info *info = (struct pm886_hs_info *)dev_get_drvdata(dev);
	int val[2]  = {0,};

	val[0] = 0;
	val[1] = info->hook_press_th;

    return sprintf(buf, "%d %d\n",val[0],val[1]);
}

#ifdef SEC_USE_VOICE_ASSIST_KEY
static ssize_t voccmd_adc_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct pm886_hs_info *info = (struct pm886_hs_info *)dev_get_drvdata(dev);
	int val[2]  = {0,};

	val[0] = info->vol_down_press_th+1;
	val[1] = info->voc_command_press_th;

	return sprintf(buf, "%d %d\n",val[0],val[1]);
}
#endif

static ssize_t volup_adc_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct pm886_hs_info *info = (struct pm886_hs_info *)dev_get_drvdata(dev);
	int val[2]  = {0,};

	val[0] = info->hook_press_th+1;
	val[1] = info->vol_up_press_th;

	return sprintf(buf, "%d %d\n",val[0],val[1]);
}

static ssize_t voldown_adc_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct pm886_hs_info *info = (struct pm886_hs_info *)dev_get_drvdata(dev);
	int val[2]  = {0,};

	val[0] = info->vol_up_press_th+1;
	val[1] = info->vol_down_press_th;

	return sprintf(buf, "%d %d\n",val[0],val[1]);
}

static DEVICE_ATTR(jack_adc, 0444, earjack_adc_show, NULL);
static DEVICE_ATTR(send_end_btn_adc, S_IRUGO, hook_adc_show, NULL);
static DEVICE_ATTR(voc_cmd_btn_adc, S_IRUGO, voccmd_adc_show, NULL);
static DEVICE_ATTR(vol_up_btn_adc, S_IRUGO, volup_adc_show, NULL);
static DEVICE_ATTR(vol_down_btn_adc, S_IRUGO, voldown_adc_show, NULL);
#endif

#endif

static void pm886_init_work(struct work_struct *work)
{
	struct pm886_hs_info *info =
	    container_of(work, struct pm886_hs_info, work_init);
	/* headset status is not stable when boot up. wait 200ms */
	msleep(200);
	pm886_headset_handler(info->irq_headset, info);
}

static int pm886_headset_suspend(struct platform_device *pdev,
					pm_message_t state)
{
	struct pm886_hs_info *info = platform_get_drvdata(pdev);

	/* it's not proper to use "pm80x_dev_suspend" here */
	if (device_may_wakeup(&pdev->dev)) {
		enable_irq_wake(info->irq_headset);
		if (info->hs_status == SND_JACK_HEADSET)
			enable_irq_wake(info->irq_hook);
	}

	regmap_update_bits(hs_info->map, PM886_HEADSET_CNTRL,
					PM886_HSDET_SLP,
					PM886_HSDET_SLP);
	return 0;
}

static int pm886_headset_resume(struct platform_device *pdev)
{
	struct pm886_hs_info *info = platform_get_drvdata(pdev);

	if (device_may_wakeup(&pdev->dev)) {
		disable_irq_wake(info->irq_headset);
		if (info->hs_status == SND_JACK_HEADSET)
			disable_irq_wake(info->irq_hook);
	}

	regmap_update_bits(hs_info->map, PM886_HEADSET_CNTRL,
					PM886_HSDET_SLP,
					0);

	return 0;
}

static struct regmap *pm80x_get_companion(void)
{
	return get_codec_companion();
}

static int pm886_headset_probe(struct platform_device *pdev)
{
	struct pm88x_chip *chip = dev_get_drvdata(pdev->dev.parent);
	int irq_headset, irq_hook, ret = 0;
	struct resource *headset_resource, *mic_resource;

#if defined (SEC_USE_HS_SYSFS)
	struct class *audio;
	struct device *earjack;
#endif	

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
	    devm_kzalloc(&pdev->dev, sizeof(struct pm886_hs_info),
			 GFP_KERNEL);
	if (!hs_info)
		return -ENOMEM;

	hs_info->chip = chip;
	hs_info->mic_bias = regulator_get(&pdev->dev, "marvell,micbias");

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
			"marvell,vol-down-press-th", &hs_info->vol_down_press_th))
			dev_dbg(&pdev->dev, "Do not get vol down threshold\n");
#ifdef SEC_USE_VOICE_ASSIST_KEY
		if (of_property_read_u32(pdev->dev.of_node,
			"marvell,voc-command-press-th", &hs_info->voc_command_press_th))
			dev_dbg(&pdev->dev, "Do not get voice command threshold\n");
#endif
		if (of_property_read_u32(pdev->dev.of_node,
			"marvell,mic-det-th", &hs_info->mic_det_th))
			dev_dbg(&pdev->dev,	"Do not get mic detect threshold\n");
		if (of_property_read_u32(pdev->dev.of_node,
			"marvell,press-release-th", &hs_info->press_release_th))
			dev_dbg(&pdev->dev,
				"Do not get press release threshold\n");
		if (of_property_read_u32(pdev->dev.of_node,
			"marvell,gpio5v-detect-mode", &hs_info->gpio5v_detect_mode))
			dev_dbg(&pdev->dev,
				"Do not get gpio5v detect mode flag\n");
		if (of_property_read_u32(pdev->dev.of_node,
			"marvell,micbias-volt", &hs_info->mic_bias_volt))
			dev_dbg(&pdev->dev, "Do not get micbias voltage\n");
	} else {
		hs_info->hook_press_th = PM886_HOOK_PRESS_TH;
#ifdef SEC_USE_VOICE_ASSIST_KEY
		hs_info->voc_command_press_th= PM886_VOC_CMD_PRESS_TH; //KSND Voc CMD key
#endif
		hs_info->vol_up_press_th = PM886_VOL_UP_PRESS_TH;
		hs_info->vol_down_press_th = PM886_VOL_DOWN_PRESS_TH;
		hs_info->mic_det_th = PM886_MIC_DET_TH;
		hs_info->press_release_th = PM886_PRESS_RELEASE_TH;
		hs_info->mic_bias_volt = 1700;
	}

	/*
	 * When MIC_DET_RDIV_DIS=1, mic bias measure range is 0~1.4V.
	 * When MIC_DET_RDIV_DIS=0, LDO16_VSET[2]=0,
	 * mic bias measure range is 0~1.85V.
	 * When MIC_DET_RDIV_DIS=0, LDO16_VSET[2]=1,
	 * mic bias measure range is 0~3.1V.
	 */
	if (hs_info->mic_bias_volt <= 2000)
		hs_info->mic_bias_range = 1850;
	else
		hs_info->mic_bias_range = 1400;

	hs_info->map = chip->base_regmap;
	hs_info->map_gpadc = chip->gpadc_regmap;
	hs_info->map_codec = pm80x_get_companion();
	hs_info->irq_headset = irq_headset;
	hs_info->irq_hook = irq_hook;
	hs_info->dev = &pdev->dev;
	hs_info->hk_status = 0;
	hs_info->hk_avg = 0;
	hs_info->hk_num = 0;
#if defined (SEC_USE_HS_SYSFS)
	hs_info->hk_adc = 0;
#endif	

	mutex_init(&hs_info->hs_mutex);

	INIT_WORK(&hs_info->work_release, pm886_hook_release_work);
	INIT_WORK(&hs_info->work_init, pm886_init_work);

	/* init timer for hook release */
	setup_timer(&hs_info->hook_timer, pm886_hook_timer_handler,
		    (unsigned long)hs_info);

	ret = devm_request_threaded_irq(&pdev->dev, hs_info->irq_headset,
			NULL, pm886_headset_handler,
			IRQF_ONESHOT | IRQF_NO_SUSPEND, "headset", hs_info);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to request IRQ: #%d: %d\n",
			hs_info->irq_headset, ret);
		goto err_out;
	}

	ret = devm_request_threaded_irq(&pdev->dev, hs_info->irq_hook,
			NULL, pm886_hook_handler,
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
	pm886_hook_int(hs_info, 0);

	platform_set_drvdata(pdev, hs_info);
	hs_info->hsdetect_dev = &pdev->dev;

#if defined (SEC_USE_SAMSUNG_JACK_API)
#if defined (CONFIG_SWITCH)
	ret = switch_dev_register(&switch_jack_detection);
#else	
    ret = extcon_dev_register(&extcon_jack_detection);
#endif
    if (ret < 0) {
        pr_err("%s : Failed to register switch device\n", __func__);
        goto err_out;
    }
#endif

#if defined (SEC_USE_HS_SYSFS)
    /* sys fs for factory test */
    audio = class_create(THIS_MODULE, "audio");
    if (IS_ERR(audio))
        pr_err("Failed to create class(audio)!\n");
        
    earjack = device_create(audio, NULL, 0, hs_info, "earjack");
    if (IS_ERR(earjack))
        pr_err("Failed to create device(earjack)!\n");

    ret = device_create_file(earjack, &dev_attr_key_state);
    if (ret)
        pr_err("Failed to create device file in sysfs entries!\n");
            
    ret = device_create_file(earjack, &dev_attr_state);
    if (ret)
        pr_err("Failed to create device file in sysfs entries!\n");

	ret = device_create_file(earjack, &dev_attr_mic_adc);
	if (ret)
		pr_err("Failed to create device file in sysfs entries!\n");

#if defined (SEC_USE_HS_INFO_SYSFS)
    ret = device_create_file(earjack, &dev_attr_jack_adc);
    if (ret)
        pr_err("Failed to create device file in sysfs entries(%s)!\n", dev_attr_jack_adc.attr.name);

    ret = device_create_file(earjack, &dev_attr_send_end_btn_adc);
    if (ret)
        pr_err("Failed to create device file in sysfs entries!\n");

    ret = device_create_file(earjack, &dev_attr_vol_up_btn_adc);
    if (ret)
        pr_err("Failed to create device file in sysfs entries!\n");

    ret = device_create_file(earjack, &dev_attr_vol_down_btn_adc);
    if (ret)
        pr_err("Failed to create device file in sysfs entries!\n");
#ifdef SEC_USE_VOICE_ASSIST_KEY
	ret = device_create_file(earjack, &dev_attr_voc_cmd_btn_adc);
	if (ret)
		pr_err("Failed to create device file in sysfs entries!\n");
#endif
#endif			

	dev_set_drvdata(earjack, hs_info);  
#endif	

	/* Hook:32 ms debounce time */
	regmap_update_bits(hs_info->map, PM886_MIC_CNTRL, MIC_DET_DBS,
			   MIC_DET_DBS_32MS);
	/* Hook:continue duty cycle */
	regmap_update_bits(hs_info->map, PM886_MIC_CNTRL, MIC_DET_PRD,
			   MIC_DET_PRD_CTN);

	if (hs_info->gpio5v_detect_mode) {
		if (chip->type == PM886) {
			//for PM886(1908)
			/* set HSDET3 mode */
			regmap_update_bits(hs_info->map, PM88X_GPIO_CTRL4,
				PM88X_HSDET3_MODE, PM88X_HSDET3_MODE);
			/* enable hsdet safe */
			regmap_update_bits(hs_info->map, PM88X_GNDDET2,
				PM88X_HSDET_SAFE, PM88X_HSDET_SAFE);
		} else {
			//for PM880(1936)
			/* set HSDET3 mode */
			regmap_update_bits(hs_info->map, PM886_GPIO_CTRL4,
				PM886_HSDET3_MODE, PM886_HSDET3_MODE);
			/* enable hsdet safe */
			regmap_update_bits(hs_info->map, PM886_GNDDET2,
				PM886_HSDET_SAFE, PM886_HSDET_SAFE);
		}
	}

	disable_irq(hs_info->irq_headset);
	regmap_update_bits(hs_info->map, PM886_HEADSET_CNTRL,
			   PM886_HEADSET_DET_EN, PM886_HEADSET_DET_EN);
	regmap_update_bits(hs_info->map, PM886_HEADSET_CNTRL,
		   PM886_HEADSET_AUTO, PM886_HEADSET_AUTO);

	enable_irq(hs_info->irq_headset);

	if (chip->type == PM880) {
		/* enable MIC bias auto mechanism */
		regmap_update_bits(hs_info->map, PM880_HEADSET_CNTRL3,
				PM880_MIC_BIAS_AUTO_MUTE_EN, PM880_MIC_BIAS_AUTO_MUTE_EN);
	}

	device_init_wakeup(&pdev->dev, 1);

	if (ret < 0)
		return ret;

	headset_detect = pm886_headset_detect;
	hook_detect = pm886_hook_detect;

	return 0;

err_out:
	return ret;
}

static int pm886_headset_remove(struct platform_device *pdev)
{
	struct pm886_hs_info *hs_info = platform_get_drvdata(pdev);

	/* disable headset detection on GPIO */
	regmap_update_bits(hs_info->map, PM886_HEADSET_CNTRL,
			   PM886_HEADSET_DET_EN, 0);

	cancel_work_sync(&hs_info->work_release);
	cancel_work_sync(&hs_info->work_init);

	devm_kfree(&pdev->dev, hs_info);

	return 0;
}

static void pm886_headset_shutdown(struct platform_device *pdev)
{
	devm_free_irq(&pdev->dev, hs_info->irq_headset, hs_info);
	devm_free_irq(&pdev->dev, hs_info->irq_hook, hs_info);

	pm886_headset_remove(pdev);
	return;
}

static struct platform_driver pm886_headset_driver = {
	.probe = pm886_headset_probe,
	.remove = pm886_headset_remove,
	.shutdown = pm886_headset_shutdown,
	.suspend = pm886_headset_suspend,
	.resume = pm886_headset_resume,
	.driver = {
		   .name = "88pm88x-headset",
		   .owner = THIS_MODULE,
		   },
};

module_platform_driver(pm886_headset_driver);

MODULE_DESCRIPTION("Marvell 88PM886 Headset driver");
MODULE_AUTHOR("Zhao Ye <zhaoy@marvell.com>");
MODULE_LICENSE("GPL");
