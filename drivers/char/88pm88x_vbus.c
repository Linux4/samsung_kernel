/*
 * 88pm886 VBus driver for Marvell USB
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/mfd/88pm88x.h>
#include <linux/mfd/88pm886.h>
#include <linux/mfd/88pm886-reg.h>
#include <linux/mfd/88pm88x-reg.h>
#include <linux/delay.h>
#include <linux/of_device.h>
#include <linux/platform_data/mv_usb.h>

#define PM88X_USB_OTG_EN		(1 << 7)

#define PM88X_CHG_CONFIG4		(0x2B)
#define PM88X_VBUS_SW_EN		(1 << 0)

#define PM88X_OTG_LOG1			(0x47)
#define PM88X_OTG_UVVBAT		(1 << 0)
#define PM88X_OTG_SHORT_DET		(1 << 1)

#define PM88X_BOOST_CFG1		(0x6B)
#define PM88X_OTG_BST_VSET_MASK		(0x7)
#define PM88X_OTG_BST_VSET(x)		((x - 3750) / 250)

#define USB_OTG_MIN			4800 /* mV */
#define USB_INSERTION			4400 /* mV */

/* choose 0x100(87.5mV) as threshold */
#define OTG_IDPIN_TH			(0x100)

/* 1.367 mV/LSB */
/*
 * this should be 1.709mV/LSB. setting to 1.367mV/LSB
 * as a W/A since there's currently a BUG per JIRA PM886-9
 * will be refined once fix is available
 */
#define PM886A0_VBUS_2_VALUE(v)		((v << 9) / 700)
#define PM886A0_VALUE_2_VBUS(val)		((val * 700) >> 9)

/* 1.709 mV/LSB */
#define PM88X_VBUS_2_VALUE(v)		((v << 9) / 875)
#define PM88X_VALUE_2_VBUS(val)		((val * 875) >> 9)

struct pm88x_vbus_info {
	struct pm88x_chip	*chip;
	int			vbus_irq;
	int			id_irq;
	int			vbus_gpio;
	int			id_gpadc;
	struct delayed_work	pxa_notify;
};

static struct pm88x_vbus_info *vbus_info;

static void pm88x_vbus_check_errors(struct pm88x_vbus_info *info)
{
	int val = 0;

	regmap_read(info->chip->battery_regmap, PM88X_OTG_LOG1, &val);

	if (val & PM88X_OTG_UVVBAT)
		dev_err(info->chip->dev, "OTG error: OTG_UVVBAT\n");
	if (val & PM88X_OTG_SHORT_DET)
		dev_err(info->chip->dev, "OTG error: OTG_SHORT_DET\n");

	if (val)
		regmap_write(info->chip->battery_regmap, PM88X_OTG_LOG1, val);
}

static int pm88x_get_vbus(unsigned int *level)
{
	int ret, val, voltage;
	unsigned char buf[2];

	ret = regmap_bulk_read(vbus_info->chip->gpadc_regmap,
				PM88X_VBUS_MEAS1, buf, 2);
	if (ret)
		return ret;

	val = ((buf[0] & 0xff) << 4) | (buf[1] & 0x0f);
	switch (vbus_info->chip->type) {
	case PM886:
		if (vbus_info->chip->chip_id == PM886_A0) {
			voltage = PM886A0_VALUE_2_VBUS(val);
			break;
		}
	default:
		voltage = PM88X_VALUE_2_VBUS(val);
		break;
	}

	/* read pm886 status to decide it's cable in or out */
	regmap_read(vbus_info->chip->base_regmap, PM88X_STATUS1, &val);

	/* cable in */
	if (val & PM88X_CHG_DET) {
		if (voltage >= USB_INSERTION) {
			*level = VBUS_HIGH;
			dev_dbg(vbus_info->chip->dev,
				"%s: USB cable is valid! (%dmV)\n",
				__func__, voltage);
		} else {
			*level = VBUS_LOW;
			dev_err(vbus_info->chip->dev,
				"%s: USB cable not valid! (%dmV)\n",
				__func__, voltage);
		}
	/* cable out */
	} else {
		/* OTG mode */
		if (voltage >= USB_OTG_MIN) {
			*level = VBUS_HIGH;
			dev_dbg(vbus_info->chip->dev,
				"%s: OTG voltage detected!(%dmV)\n",
				__func__, voltage);
		} else {
			*level = VBUS_LOW;
			dev_dbg(vbus_info->chip->dev,
				"%s: Cable out / OTG disabled !(%dmV)\n", __func__, voltage);
			/* Open USB_SW in order to save power at low power mode */
			switch (vbus_info->chip->type) {
			case PM886:
				if (vbus_info->chip->chip_id == PM886_A0)
					break;
			default:
				regmap_update_bits(vbus_info->chip->battery_regmap,
					PM88X_CHG_CONFIG4, PM88X_VBUS_SW_EN, 0);
				break;
			}
			pm88x_vbus_check_errors(vbus_info);
		}
	}

	return 0;
}

static int pm88x_set_vbus(unsigned int vbus)
{
	int ret;
	unsigned int data = 0;

	if (vbus == VBUS_HIGH) {
		ret = regmap_update_bits(vbus_info->chip->battery_regmap, PM88X_CHG_CONFIG1,
					PM88X_USB_OTG_EN, PM88X_USB_OTG_EN);
		if (ret)
			return ret;

		switch (vbus_info->chip->type) {
		case PM886:
			if (vbus_info->chip->chip_id == PM886_A0)
				break;
		default:
			ret = regmap_update_bits(vbus_info->chip->battery_regmap,
				PM88X_CHG_CONFIG4, PM88X_VBUS_SW_EN, PM88X_VBUS_SW_EN);
			break;
		}
	} else
		ret = regmap_update_bits(vbus_info->chip->battery_regmap, PM88X_CHG_CONFIG1,
					PM88X_USB_OTG_EN, 0);

	if (ret)
		return ret;

	usleep_range(10000, 20000);

	ret = pm88x_get_vbus(&data);
	if (ret)
		return ret;

	if (data != vbus) {
		dev_err(vbus_info->chip->dev, "vbus set failed %x\n", vbus);
		pm88x_vbus_check_errors(vbus_info);
	} else
		dev_info(vbus_info->chip->dev, "vbus set done %x\n", vbus);

	return 0;
}

#ifndef CONFIG_USB_NOTIFY_LAYER
static int pm88x_read_id_val(unsigned int *level)
{
	unsigned int meas, upp_th, low_th;
	unsigned char buf[2];
	int ret, data;

	switch (vbus_info->id_gpadc) {
	case PM88X_GPADC0:
		meas = PM88X_GPADC0_MEAS1;
		low_th = PM88X_GPADC0_LOW_TH;
		upp_th = PM88X_GPADC0_UPP_TH;
		break;
	case PM88X_GPADC1:
		meas = PM88X_GPADC1_MEAS1;
		low_th = PM88X_GPADC1_LOW_TH;
		upp_th = PM88X_GPADC1_UPP_TH;
		break;
	case PM88X_GPADC2:
		meas = PM88X_GPADC2_MEAS1;
		low_th = PM88X_GPADC2_LOW_TH;
		upp_th = PM88X_GPADC2_UPP_TH;
		break;
	case PM88X_GPADC3:
		meas = PM88X_GPADC3_MEAS1;
		low_th = PM88X_GPADC3_LOW_TH;
		upp_th = PM88X_GPADC3_UPP_TH;
		break;
	default:
		return -ENODEV;
	}

	ret = regmap_bulk_read(vbus_info->chip->gpadc_regmap, meas, buf, 2);
	if (ret)
		return ret;

	data = ((buf[0] & 0xFF) << 4) | (buf[1] & 0xF);

	if (data > OTG_IDPIN_TH) {
		regmap_write(vbus_info->chip->gpadc_regmap, low_th, OTG_IDPIN_TH >> 4);
		regmap_write(vbus_info->chip->gpadc_regmap, upp_th, 0xff);
		*level = VBUS_HIGH;
	} else {
		regmap_write(vbus_info->chip->gpadc_regmap, low_th, 0);
		regmap_write(vbus_info->chip->gpadc_regmap, upp_th, OTG_IDPIN_TH >> 4);
		*level = VBUS_LOW;
	}

	return 0;
};

static int pm88x_init_id(void)
{
	return 0;
}

static void pm88x_pxa_notify(struct work_struct *work)
{
	struct pm88x_vbus_info *info =
		container_of(work, struct pm88x_vbus_info, pxa_notify.work);
	/*
	 * 88pm886 has no ability to distinguish
	 * AC/USB charger, so notify usb framework to do it
	 */
	pxa_usb_notify(PXA_USB_DEV_OTG, EVENT_VBUS, 0);
	dev_dbg(info->chip->dev, "88pm886 vbus pxa usb is notified..\n");
}

static irqreturn_t pm88x_vbus_handler(int irq, void *data)
{

	struct pm88x_vbus_info *info = data;
	/*
	 * Close USB_SW to allow USB 5V to USB PHY in case of cable
	 * insertion. in case of removal this will be called and the
	 * switch will be opened at pm88x_get_vbus
	 */
	switch (vbus_info->chip->type) {
	case PM886:
		if (vbus_info->chip->chip_id == PM886_A0)
			break;
	default:
		regmap_update_bits(vbus_info->chip->battery_regmap,
			PM88X_CHG_CONFIG4, PM88X_VBUS_SW_EN, PM88X_VBUS_SW_EN);
		break;
	}

	dev_dbg(info->chip->dev, "88pm886 vbus interrupt is served..\n");
	/* allowing 7.5msec for the SW to close */
	schedule_delayed_work(&info->pxa_notify, usecs_to_jiffies(7500));
	return IRQ_HANDLED;
}

static irqreturn_t pm88x_id_handler(int irq, void *data)
{
	struct pm88x_vbus_info *info = data;

	 /* notify to wake up the usb subsystem if ID pin is pulled down */
	pxa_usb_notify(PXA_USB_DEV_OTG, EVENT_ID, 0);
	dev_dbg(info->chip->dev, "88pm886 id interrupt is served..\n");
	return IRQ_HANDLED;
}
#else
static int pm88x_usb_sw_setting(bool state)
{
	if (vbus_info->chip->chip_id != PM886_A0) {
		if(state) {
			/* Close USB_SW */
			regmap_update_bits(vbus_info->chip->battery_regmap,
				PM88X_CHG_CONFIG4, PM88X_VBUS_SW_EN, PM88X_VBUS_SW_EN);
			pr_info("88pm886 %s: USB_SW manual close\n", __func__);
		} else {
			/* Open USB_SW in order to save power at low power mode */
			regmap_update_bits(vbus_info->chip->battery_regmap,
				PM88X_CHG_CONFIG4, PM88X_VBUS_SW_EN, 0);
			/* allowing 7.5msec for the SW to close */
			mdelay(10);
			pr_info("88pm886 %s: USB_SW manual open\n", __func__);
		}
	}
	return 0;
}
#endif

static void pm88x_vbus_config(struct pm88x_vbus_info *info)
{
	unsigned int en, low_th, upp_th;

	if (!info)
		return;

	/* set booster voltage to 5.0V */
	regmap_update_bits(info->chip->battery_regmap, PM88X_BOOST_CFG1,
			PM88X_OTG_BST_VSET_MASK, PM88X_OTG_BST_VSET(5000));

	/* clear OTG errors */
	pm88x_vbus_check_errors(info);

	/* set id gpadc low/upp threshold and enable it */
	switch (info->id_gpadc) {
	case PM88X_GPADC0:
		low_th = PM88X_GPADC0_LOW_TH;
		upp_th = PM88X_GPADC0_UPP_TH;
		en = PM88X_GPADC0_MEAS_EN;
		break;
	case PM88X_GPADC1:
		low_th = PM88X_GPADC1_LOW_TH;
		upp_th = PM88X_GPADC1_UPP_TH;
		en = PM88X_GPADC1_MEAS_EN;
		break;
	case PM88X_GPADC2:
		low_th = PM88X_GPADC2_LOW_TH;
		upp_th = PM88X_GPADC2_UPP_TH;
		en = PM88X_GPADC2_MEAS_EN;
		break;
	case PM88X_GPADC3:
		low_th = PM88X_GPADC3_LOW_TH;
		upp_th = PM88X_GPADC3_UPP_TH;
		en = PM88X_GPADC3_MEAS_EN;
		break;
	default:
		return;
	}

	/* set the threshold for GPADC to prepare for interrupt */
	regmap_write(info->chip->gpadc_regmap, low_th, OTG_IDPIN_TH >> 4);
	regmap_write(info->chip->gpadc_regmap, upp_th, 0xff);

	regmap_update_bits(info->chip->gpadc_regmap, PM88X_GPADC_CONFIG2, en, en);
}

static int pm88x_vbus_dt_init(struct device_node *np, struct pm88x_vbus_info *usb)
{
	return of_property_read_u32(np, "gpadc-number", &usb->id_gpadc);
}

static void pm88x_vbus_fixup(struct pm88x_vbus_info *info)
{
	if (!info || !info->chip) {
		pr_err("%s: empty device information.\n", __func__);
		return;
	}

	switch (info->chip->type) {
	case PM886:
		if (info->chip->chip_id == PM886_A0) {
			pr_info("%s: fix up for the vbus driver.\n", __func__);
			/* 1. base page 0x1f.0 = 1 --> unlock test page */
			regmap_write(info->chip->base_regmap, 0x1f, 0x1);
			/* 2. test page 0x90.[4:0] = 0, reset trimming to mid point 0 */
			regmap_update_bits(info->chip->test_regmap, 0x90, 0x1f << 0, 0);
			/* 3. base page 0x1f.0 = 0 --> lock the test page */
			regmap_write(info->chip->base_regmap, 0x1f, 0x0);
		}
		break;
	default:
		break;
	}
}

static int pm88x_vbus_probe(struct platform_device *pdev)
{
	struct pm88x_chip *chip = dev_get_drvdata(pdev->dev.parent);
	struct pm88x_vbus_info *usb;
	struct device_node *node = pdev->dev.of_node;
	int ret;

	/* vbus_info global variable used by get/set_vbus */
	vbus_info = usb = devm_kzalloc(&pdev->dev,
			   sizeof(struct pm88x_vbus_info), GFP_KERNEL);
	if (!usb)
		return -ENOMEM;

	ret = pm88x_vbus_dt_init(node, usb);
	if (ret < 0)
		usb->id_gpadc = PM88X_NO_GPADC;

	usb->chip = chip;

	/* do it before enable interrupt */
	pm88x_vbus_fixup(usb);

#ifndef CONFIG_USB_NOTIFY_LAYER
	usb->vbus_irq = platform_get_irq(pdev, 0);
	if (usb->vbus_irq < 0) {
		dev_err(&pdev->dev, "failed to get vbus irq\n");
		ret = -ENXIO;
		goto out;
	}

	/* global variable used by get/set_vbus */
	vbus_info = usb;

	INIT_DELAYED_WORK(&usb->pxa_notify, pm88x_pxa_notify);

	ret = devm_request_threaded_irq(&pdev->dev, usb->vbus_irq, NULL,
					pm88x_vbus_handler,
					IRQF_ONESHOT | IRQF_NO_SUSPEND,
					"usb detect", usb);
	if (ret) {
		dev_info(&pdev->dev,
			"cannot request irq for VBUS, return\n");
		goto out;
	}

	if (usb->id_gpadc != PM88X_NO_GPADC) {
		pm88x_vbus_config(usb);

		usb->id_irq = platform_get_irq(pdev, usb->id_gpadc + 1);
		if (usb->id_irq < 0) {
			dev_err(&pdev->dev, "failed to get idpin irq\n");
			ret = -ENXIO;
			goto out;
		}

		ret = devm_request_threaded_irq(&pdev->dev, usb->id_irq, NULL,
						pm88x_id_handler,
						IRQF_ONESHOT | IRQF_NO_SUSPEND,
						"id detect", usb);
		if (ret) {
			dev_info(&pdev->dev,
				"cannot request irq for idpin, return\n");
			goto out;
		}
	}
#else
	/* global variable used by get/set_vbus */
	vbus_info = usb;
#endif
	platform_set_drvdata(pdev, usb);
	device_init_wakeup(&pdev->dev, 1);

#ifndef CONFIG_USB_NOTIFY_LAYER
	pxa_usb_set_extern_call(PXA_USB_DEV_OTG, vbus, set_vbus,
				pm88x_set_vbus);
	pxa_usb_set_extern_call(PXA_USB_DEV_OTG, vbus, get_vbus,
				pm88x_get_vbus);
	if (usb->id_gpadc != PM88X_NO_GPADC) {
		pxa_usb_set_extern_call(PXA_USB_DEV_OTG, idpin, get_idpin,
					pm88x_read_id_val);
		pxa_usb_set_extern_call(PXA_USB_DEV_OTG, idpin, init,
					pm88x_init_id);
	}
#else
	pxa_usb_set_extern_call(PXA_USB_DEV_OTG, vbus, pm_usb_sw,
				pm88x_usb_sw_setting);
#endif
	return 0;

#ifndef CONFIG_USB_NOTIFY_LAYER
out:
	return ret;
#endif
}

static int pm88x_vbus_remove(struct platform_device *pdev)
{
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static const struct of_device_id pm88x_vbus_dt_match[] = {
	{ .compatible = "marvell,88pm88x-vbus", },
	{ },
};
MODULE_DEVICE_TABLE(of, pm88x_vbus_dt_match);

static struct platform_driver pm88x_vbus_driver = {
	.driver		= {
		.name	= "88pm88x-vbus",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(pm88x_vbus_dt_match),
	},
	.probe		= pm88x_vbus_probe,
	.remove		= pm88x_vbus_remove,
};

static int pm88x_vbus_init(void)
{
	return platform_driver_register(&pm88x_vbus_driver);
}
module_init(pm88x_vbus_init);

static void pm88x_vbus_exit(void)
{
	platform_driver_unregister(&pm88x_vbus_driver);
}
module_exit(pm88x_vbus_exit);

MODULE_DESCRIPTION("VBUS driver for Marvell Semiconductor 88PM886");
MODULE_AUTHOR("Shay Pathov <shayp@marvell.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:88pm88x-vbus");
