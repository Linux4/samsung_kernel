/*
 * driver/adc/pm6150l_rid_adc.c - rid pm6150l device driver
 *
 * Copyright (C) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/iio/consumer.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sec_class.h>
#include <linux/delay.h>
#if defined(CONFIG_USB_CCIC_NOTIFIER_USING_QC)
#include <linux/usb/typec/pm6150/pm6150_typec.h>
#endif
#if defined(CONFIG_BATTERY_SAMSUNG_USING_QC)
#include "../battery_qc/include/sec_battery_qc.h"
#include <linux/pmic-voter.h>
#if defined(CONFIG_QPNP_SMB5)
#if defined(CONFIG_SEC_A90Q_PROJECT) || defined(CONFIG_SEC_A70S_PROJECT) || defined(CONFIG_SEC_A70Q_PROJECT)
#include "../power/supply/qcom_r1/smb5-lib.h"
#else
#include "../power/supply/qcom/smb5-lib.h"
#endif
#endif
#endif

enum {
	RID_ADC_GND		= 0,
	RID_ADC_SHORT,
	RID_ADC_56K,
	RID_ADC_255K,
	RID_ADC_301K,
	RID_ADC_523K,
	RID_ADC_619K,
	RID_ADC_OPEN,
};

#define	ADC_READ_CNT	5

/* rid_adc channel */
static struct iio_channel *adc_channel = NULL;
/* jigon */
static int jigon_gpio = 0;
#if defined(CONFIG_USB_CCIC_NOTIFIER_USING_QC)
extern void pm6150_ccic_event_work(int dest,
		int id, int attach, int event, int sub);
extern void set_factory_force_usb(bool enable);
#endif

/* temprory variable */
static int rid_adc = 0, raw_rid_adc = 0;
static int rid_adc_gpio = 0;
#if defined(CONFIG_PM6150_USB_FALSE_DETECTION_WA_BY_GND) && !defined(CONFIG_SEC_FACTORY)
static struct smb_charger * g_smb_chg;
static ktime_t drp_none_time;
#endif

int get_rid_type(void)
{
	return rid_adc;
}

static int check_rid_adc(int val)
{
	pr_info("%s: ADC range: %d\n", __func__, val);

	if (val < 30000)
		return RID_ADC_GND;
	else if ((30000 <= val) && (val < 100000))
		return RID_ADC_56K;
	else if ((100000 <= val) && (val < 210000))
		return RID_ADC_255K;
	else if ((210000 <= val) && (val < 290000))
		return RID_ADC_301K;
	else if ((290000 <= val) && (val < 390000))
		return RID_ADC_523K;
	else if ((390000 <= val) && (val < 750000))
		return RID_ADC_619K;
	else if (750000 <= val)
		return RID_ADC_OPEN;
	else {
		pr_info("%s: Undefined ADC range!\n", __func__);
		return -1;
	}

	return 0;
}

static int handle_rid_adc(int val)
{
#if defined(CONFIG_BATTERY_SAMSUNG_USING_QC)
	struct sec_battery_info *battery = get_sec_battery();
	struct smb_charger *chg;
#endif
	pr_info("%s\n", __func__);

	rid_adc = check_rid_adc(val);

	switch(rid_adc) {
	case RID_ADC_GND:
		pr_info("%s: RID_GND\n", __func__);
#if defined(CONFIG_USB_CCIC_NOTIFIER_USING_QC)
		pm6150_ccic_event_work(CCIC_NOTIFY_DEV_MUIC, CCIC_NOTIFY_ID_RID,
			RID_GND/*rid*/, USB_STATUS_NOTIFY_DETACH, 0);
#endif
#if defined(CONFIG_PM6150_USB_FALSE_DETECTION_WA_BY_GND) && !defined(CONFIG_SEC_FACTORY)
		//smblib_drp_enable(g_smb_chg);
#endif
		break;
	case RID_ADC_56K:
		pr_info("%s: RID_56K\n", __func__);
#if defined(CONFIG_SEC_FACTORY)
#if defined(CONFIG_USB_CCIC_NOTIFIER_USING_QC)
		pr_info("%s: RID_56K notified in FACTORY binary\n", __func__);
		pm6150_ccic_event_work(CCIC_NOTIFY_DEV_MUIC, CCIC_NOTIFY_ID_RID,
			RID_056K/*rid*/, USB_STATUS_NOTIFY_DETACH, 0);
#endif
#endif
		break;
	case RID_ADC_255K:
		pr_info("%s: RID_255K\n", __func__);
#if defined(CONFIG_USB_CCIC_NOTIFIER_USING_QC)
		pm6150_ccic_event_work(CCIC_NOTIFY_DEV_MUIC, CCIC_NOTIFY_ID_RID,
			RID_255K/*rid*/, USB_STATUS_NOTIFY_DETACH, 0);
#endif
#if defined(CONFIG_PM6150_USB_FALSE_DETECTION_WA_BY_GND) && !defined(CONFIG_SEC_FACTORY)
		//smblib_drp_enable(g_smb_chg);
#endif
		break;
	case RID_ADC_301K:
		pr_info("%s: RID_301K, Jig on - LOW\n", __func__);
#if defined(CONFIG_USB_CCIC_NOTIFIER_USING_QC)
		pm6150_ccic_event_work(CCIC_NOTIFY_DEV_MUIC, CCIC_NOTIFY_ID_RID,
			RID_301K/*rid*/, USB_STATUS_NOTIFY_DETACH, 0);
#endif
#if defined(CONFIG_PM6150_USB_FALSE_DETECTION_WA_BY_GND) && !defined(CONFIG_SEC_FACTORY)
		//smblib_drp_enable(g_smb_chg);
#endif
#if defined(CONFIG_BATTERY_SAMSUNG_USING_QC)
		if (battery != NULL) {
			sec_bat_set_ship_mode(0);
			msleep(1000);
		}
#endif
		gpio_direction_output(jigon_gpio, 0);
		break;
	case RID_ADC_523K:
		pr_info("%s: RID_523K\n", __func__);
#if defined(CONFIG_USB_CCIC_NOTIFIER_USING_QC)
		set_factory_force_usb(1);
		pm6150_ccic_event_work(CCIC_NOTIFY_DEV_MUIC, CCIC_NOTIFY_ID_RID,
			RID_523K/*rid*/, USB_STATUS_NOTIFY_DETACH, 0);
#endif
#if defined(CONFIG_SEC_FACTORY)
		pr_info("%s: Jig on - HIGH\n", __func__);
		gpio_direction_output(jigon_gpio, 1);
#if defined(CONFIG_BATTERY_SAMSUNG_USING_QC)
		if (battery != NULL) {
			qg_set_sdam_prifile_version(0xfa);	// fg_reset in next booting
			msleep(1000);
			sec_bat_set_ship_mode(1);
		}
#endif
#endif
		break;
	case RID_ADC_619K:
		pr_info("%s: RID_619K, Jig on - LOW\n", __func__);
#if defined(CONFIG_USB_CCIC_NOTIFIER_USING_QC)
		pm6150_ccic_event_work(CCIC_NOTIFY_DEV_MUIC, CCIC_NOTIFY_ID_RID,
			RID_619K/*rid*/, USB_STATUS_NOTIFY_DETACH, 0);
#endif
#if defined(CONFIG_BATTERY_SAMSUNG_USING_QC)
		if (battery != NULL) {
			chg = power_supply_get_drvdata(battery->psy_bat);
			vote(chg->usb_icl_votable, USB_PSY_VOTER, false, 0);

			sec_bat_set_ship_mode(0);
			msleep(1000);
		}
#endif
		gpio_direction_output(jigon_gpio, 0);
		break;
	case RID_ADC_OPEN:
		pr_info("%s: RID OPEN\n", __func__);
#if defined(CONFIG_USB_CCIC_NOTIFIER_USING_QC)
		set_factory_force_usb(0);
		pm6150_ccic_event_work(CCIC_NOTIFY_DEV_MUIC, CCIC_NOTIFY_ID_RID,
			RID_OPEN/*rid*/, USB_STATUS_NOTIFY_DETACH, 0);
#endif
#if defined(CONFIG_PM6150_USB_FALSE_DETECTION_WA_BY_GND) && !defined(CONFIG_SEC_FACTORY)
		//smblib_drp_disable(g_smb_chg);
#endif
		break;
	default:
		break;
	}

	return 0;
}

static int detect_rid_adc(struct platform_device *pdev)
{
	int i = 0, rid_adc_value = 0, rid_adc_arr[ADC_READ_CNT] = {0,};
	int min = 0, max = 0, min_idx = 0, max_idx = 0, sum = 0, err = 1;

	pr_info("%s\n", __func__);

	if (IS_ERR(adc_channel)) {
		adc_channel = iio_channel_get(&pdev->dev, "rid_adc_channel");
		if (IS_ERR(adc_channel)) { 
			pr_info("%s: channel unavailable\n", __func__);
			return -1;
		}
	}

	for (i = 0; i < ADC_READ_CNT; i++) {
		/* Delay must be added for exact result */
		if (i == 0)
			msleep(200);
		else		
			msleep(10);

		/* get rid_adc */
		err = iio_read_channel_processed(adc_channel, &rid_adc_value);
		if (err < 0) {
			pr_info("read adc error, just return!\n");
			return 0;
		} else {
			pr_info("%s: rid_adc[%d]: %d\n", __func__, i, rid_adc_value);
			rid_adc_arr[i] = rid_adc_value;
		}
	}

	min = max = rid_adc_arr[0];
	for (i = 1; i < ADC_READ_CNT; i++) {
		if (min > rid_adc_arr[i]) {
			min = rid_adc_arr[i];
			min_idx = i;
		} else if (max <= rid_adc_arr[i]) {
			max = rid_adc_arr[i];
			max_idx = i;
		}
	}

	for (i = 0; i < ADC_READ_CNT; i++) {
		if ((i == min_idx) || (i == max_idx))
			continue;
		pr_info("%s: sum += %d\n", __func__, rid_adc_arr[i]);
		sum += rid_adc_arr[i];
	}

	pr_info("%s: min_idx: %d, min: %d, max_idx:%d, max: %d, sum: %d\n", __func__, 
		min_idx, rid_adc_arr[min_idx], max_idx, rid_adc_arr[max_idx], sum);

	raw_rid_adc = sum/(ADC_READ_CNT-2);
	pr_info("%s: raw_rid_adc: %d\n", __func__, raw_rid_adc);

	return raw_rid_adc;
}

static int rid_adc_iio_init(struct platform_device *pdev)
{
	int val = 0;

	adc_channel = iio_channel_get(&pdev->dev, "rid_adc_channel");
	if (IS_ERR(adc_channel)) { 
		pr_info("%s: channel unavailable\n", __func__);
		return -1;
	} else
		pr_info("%s: channel: %d\n", __func__, adc_channel);

	/* Read once at probe */
	val = detect_rid_adc(pdev);
	if (val < 0) 
		return -1;

	handle_rid_adc(val);

	return 0;
}

#if defined(CONFIG_PM6150_USB_FALSE_DETECTION_WA_BY_GND) && !defined(CONFIG_SEC_FACTORY)
static void rid_handle_drp_mode(void) 
{
	if(gpio_get_value(rid_adc_gpio)) {
		drp_none_time = ktime_to_ms(ktime_get_boottime());
		smblib_drp_disable(g_smb_chg);
	}
	else {
		ktime_t time_diff = ktime_to_ms(ktime_get_boottime()) - drp_none_time;
		
		pr_info("[USB_WA] %s timediff: %lld last disable: %lld \n", \
					__func__, time_diff, drp_none_time);
		
		//PR_NONE to PR_DUAL, needs 50ms.
		if(time_diff < 50)
			msleep(50 - time_diff);
		
		smblib_drp_enable(g_smb_chg);
	}
}
#endif

static irqreturn_t rid_adc_irq_handler(int irq, void *data)
{
	struct platform_device *pdev = data;
	int val;

	pr_info("%s\n", __func__);
	
#if defined(CONFIG_PM6150_USB_FALSE_DETECTION_WA_BY_GND) && !defined(CONFIG_SEC_FACTORY)
	rid_handle_drp_mode();
#endif

	val = detect_rid_adc(pdev);
	if (val < 0) 
		return -1;

	handle_rid_adc(val);

	return 0;
}

static int rid_adc_irq_init(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np= dev->of_node;
	int adc_irq = 0, ret = 0;

	pr_info("%s\n", __func__);

	rid_adc_gpio = of_get_named_gpio(np, "pm6150l_rid_adc,irq-gpio", 0);

	pr_info("%s, rid_adc_gpio: %d\n", __func__, rid_adc_gpio);

	adc_irq = gpio_to_irq(rid_adc_gpio);

	pr_info("%s, rid_adc_irq: %d\n", __func__, adc_irq);

	ret = request_threaded_irq(adc_irq, NULL, rid_adc_irq_handler,
		IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING | IRQF_ONESHOT,
		"rid_adc_irq", pdev);

	pr_info("%s, ret: %d\n", __func__, ret);

	enable_irq_wake(adc_irq);

	return 0;
}

static ssize_t jig_on_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	int val = 0, ret = 0;

	sscanf(buf, "%d", &val);
	pr_info("%s, val: %d\n", __func__, val);

	gpio_direction_output(jigon_gpio, val);
#if defined(CONFIG_BATTERY_SAMSUNG_USING_QC)
	msleep(1000);
	if (val == 1) {
		qg_set_sdam_prifile_version(0xfa);	// fg_reset in next booting
		sec_bat_set_fet_control(1);
	} else {
		sec_bat_set_fet_control(0);
	}
#endif
	ret = gpio_get_value(jigon_gpio);
	pr_info("%s, jigon_gpio status: %d\n", __func__, ret);

	return count;
}
static DEVICE_ATTR(jig_on, 0664, NULL, jig_on_store);

static ssize_t rid_adc_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	pr_info("%s\n", __func__);

	switch(rid_adc) {
	case RID_ADC_GND:
		return sprintf(buf, "GND\n");
	case RID_ADC_SHORT:
		return sprintf(buf, "SHORT\n");
	case RID_ADC_56K:
		return sprintf(buf, "56K\n");
	case RID_ADC_255K:
		return sprintf(buf, "255K\n");
	case RID_ADC_301K:
		return sprintf(buf, "301K\n");
	case RID_ADC_523K:
		return sprintf(buf, "523K\n");
	case RID_ADC_619K:
		return sprintf(buf, "619K\n");
	case RID_ADC_OPEN:
		return sprintf(buf, "OPEN\n");
	default:
		break;
	}

	return sprintf(buf, "NONE\n");
}
static DEVICE_ATTR(rid_adc, 0664, rid_adc_show, NULL);

static ssize_t raw_rid_adc_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	pr_info("%s, %d uV\n", __func__, raw_rid_adc);

	return sprintf(buf, "%d uV\n", raw_rid_adc);
}
static DEVICE_ATTR(raw_rid_adc, 0664, raw_rid_adc_show, NULL);

static ssize_t usb_ctrl_store(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t count)
{
	int val;

	sscanf(buf, "%d", &val);
	pr_info("%s, val: %d\n", __func__, val);

#if defined(CONFIG_USB_CCIC_NOTIFIER_USING_QC)
	if (val) {
		set_factory_force_usb(1);
		pm6150_ccic_event_work(CCIC_NOTIFY_DEV_USB, CCIC_NOTIFY_ID_USB,
			CCIC_NOTIFY_ATTACH/*attach*/, USB_STATUS_NOTIFY_ATTACH_UFP/*drp*/, 0);
	} else {
		set_factory_force_usb(0);
		pm6150_ccic_event_work(CCIC_NOTIFY_DEV_USB, CCIC_NOTIFY_ID_USB,
			CCIC_NOTIFY_DETACH/*attach*/, USB_STATUS_NOTIFY_DETACH/*drp*/, 0);
	}
#endif

	return count;
}
static DEVICE_ATTR(usb_ctrl, 0664, NULL, usb_ctrl_store);

static struct attribute *rid_adc_attributes[] = {
	&dev_attr_jig_on.attr,
	&dev_attr_rid_adc.attr,
	&dev_attr_raw_rid_adc.attr,
	&dev_attr_usb_ctrl.attr,
	NULL
};

static const struct attribute_group rid_adc_group = {
	.attrs = rid_adc_attributes,
};

static int rid_adc_sysfs_init(void)
{
	static struct device *rid_adc_dev;
	int ret = 0;

	rid_adc_dev = sec_device_create(0, NULL, "rid_adc");
	if (IS_ERR(rid_adc_dev)) {
		pr_err("%s Failed to create device(rid_adc)!\n", __func__);
		ret = -ENODEV;
	}

	ret = sysfs_create_group(&rid_adc_dev->kobj, &rid_adc_group);
	if (ret) {
		pr_err("%s: rid_adc sysfs fail, ret %d", __func__, ret);
	}

    	return ret;
}

static int rid_pm6150l_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np= dev->of_node;

	/* output */
	jigon_gpio = of_get_named_gpio(np,"jigon-gpios", 0);
	gpio_request(jigon_gpio, "GPIO11");

	/* sysfs */
	rid_adc_sysfs_init();

	/* iio */
	rid_adc_iio_init(pdev);

	/* irq */
	rid_adc_irq_init(pdev);

    return 0;
}

static int rid_pm6150l_remove(struct platform_device *pdev)
{
	//...
	return 0;
}

#if defined(CONFIG_PM6150_USB_FALSE_DETECTION_WA_BY_GND) && !defined(CONFIG_SEC_FACTORY)
void smb5_rid_pm6150l_init(struct smb_charger *chg)
{
	g_smb_chg = chg;
}
EXPORT_SYMBOL(smb5_rid_pm6150l_init);
#endif

#if defined(CONFIG_OF)
static const struct of_device_id rid_pm6150l_of_match_table[] = {
    {
        .compatible = "pm6150l_rid_adc",
    },
    { },
};
MODULE_DEVICE_TABLE(of, rid_pm6150l_of_match_table);
#endif

static struct platform_driver rid_pm6150l_driver = {
    .probe      = rid_pm6150l_probe,
	.remove		= rid_pm6150l_remove,
    .driver     = {
        .name   = "pm6150l_rid_adc",
#if defined(CONFIG_OF)
        .of_match_table = rid_pm6150l_of_match_table,
#endif
    }
};

static int __init rid_pm6150l_init(void)
{
	return platform_driver_register(&rid_pm6150l_driver);
}
late_initcall(rid_pm6150l_init);

static void __exit rid_pm6150l_exit(void)
{
	platform_driver_unregister(&rid_pm6150l_driver);
}
module_exit(rid_pm6150l_exit);

MODULE_DESCRIPTION("RID PM6150L driver");
MODULE_AUTHOR("Hak-Young Kim <hak0.kim@samsung.com>");
MODULE_LICENSE("GPL");
