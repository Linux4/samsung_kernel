/*
 * 88pm830 VBus driver for Marvell USB
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/mfd/88pm830.h>
#include <linux/delay.h>
#include <linux/of_device.h>
#include <linux/platform_data/mv_usb.h>

#define PM830_OTG_CTRL1		(0x59)
#define OTG_VOLT_MASK		(0x7)
#define OTG_VOLT_SET(x)		(x << 0)
#define PM830_OTG_CTRL2		(0x52)
#define OTG_SR_BYP_EN		(1 << 1)
#define PM830_OTG_CTRL3		(0x5B)
#define OTG_HIGH_SIDE_MASK	(0x7 << 5)
#define OTG_HIGH_SIDE_SET(x)	(x << 5)
#define OTG_LOW_SIDE_MASK	(0x7 << 1)
#define OTG_LOW_SIDE_SET(x)	(x << 1)
#define USB_OTG_MIN		4800 /* mV */
#define USB_INSERTION		4400 /* mV */

struct pm830_vbus_info {
	struct pm830_chip	*chip;
	int			vbus_irq;
	int			id_irq;
	int			vbus_gpio;
	int			id_gpadc;
};

static struct pm830_vbus_info *vbus_info;

static int pm830_get_vbus(unsigned int *level)
{
	int ret, val, voltage;
	unsigned char buf[2];

	ret = regmap_bulk_read(vbus_info->chip->regmap,
				PM830_VCHG_MEAS1, buf, 2);
	if (ret)
		return ret;

	val = ((buf[0] & 0xff) << 4) | (buf[1] & 0x0f);
	voltage = pm830_get_adc_volt(PM830_GPADC_VCHG, val);

	/* read pm830 status to decide it's cable in or out */
	regmap_read(vbus_info->chip->regmap, PM830_STATUS, &val);

	/* cable in */
	if (val & PM830_CHG_DET) {
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
				"%s: Cable out !(%dmV)\n", __func__, voltage);
		}
	}

	return 0;
}

static int read_voltage(int flag)
{
	unsigned char buf[2];
	int volt, val;

	if (flag == 1) {
		/* read VBAT average value */
		regmap_bulk_read(vbus_info->chip->regmap, PM830_VBAT_AVG,
				buf, 2);
		val = ((buf[0] & 0xff) << 4) | (buf[1] & 0xf);
		volt = pm830_get_adc_volt(PM830_GPADC_VBAT, val);
	} else {
		/* read VCHG average value */
		regmap_bulk_read(vbus_info->chip->regmap, PM830_VCHG_AVG,
				buf, 2);
		val = ((buf[0] & 0xff) << 4) | (buf[1] & 0xf);
		volt = pm830_get_adc_volt(PM830_GPADC_VCHG, val);
	}
	return volt;
}

static int pm830_set_vbus(unsigned int vbus)
{
	int ret, i, volt;
	unsigned int data = 0;
	int booster_volt = 4500;

	if (vbus == VBUS_HIGH) {
		/* 1. A0/A1/B0 - open test page
		 *    B1/B2 - don't need to operate test page
		 */
		if (vbus_info->chip->version <= PM830_B0_VERSION)
			regmap_update_bits(vbus_info->chip->regmap, 0xf0, (1 << 0), (1 << 0));

		/* 2. enable OTG mode */
		ret = regmap_update_bits(vbus_info->chip->regmap, PM830_PRE_REGULATOR,
				PM830_USB_OTG_EN, PM830_USB_OTG_EN);

		/*
		 * 3. need to make sure VCHG >= 1500mV three times, this will
		 * prevent powering-off system if a real short is present on
		 * VBUS pin. checked with silicon design team.
		 */
		for (i = 0; i < 3; i++) {
			usleep_range(1000, 2000);
			volt = read_voltage(0);
			if (volt < 1500) {
				/* voltage too low, it should be  shorted */
				pr_info("VBUS pin shorted, stop boost!\n");
				regmap_update_bits(vbus_info->chip->regmap, PM830_PRE_REGULATOR,
						PM830_USB_OTG_EN, 0);
				regmap_update_bits(vbus_info->chip->regmap, 0xf0, (1 << 0), 0);
				goto out;
			}
		}

		/*
		 * 4. booster high-side/low-side over current protection must be
		 * increased
		 */
		regmap_update_bits(vbus_info->chip->regmap, PM830_OTG_CTRL3,
				OTG_HIGH_SIDE_MASK, OTG_HIGH_SIDE_SET(6));
		regmap_update_bits(vbus_info->chip->regmap, PM830_OTG_CTRL3,
				OTG_LOW_SIDE_MASK, OTG_LOW_SIDE_SET(4));

		/* 5. A0/A1/B0 - [0xdb].[3,2,0] = 3'b1
		 *    B1/B2 - set OTG_STR_BYP_EN
		 */
		if (vbus_info->chip->version <= PM830_B0_VERSION)
			regmap_update_bits(vbus_info->chip->regmap, 0xdb, 0xd, 0xd);
		else
			regmap_update_bits(vbus_info->chip->regmap, PM830_OTG_CTRL2,
					OTG_SR_BYP_EN, OTG_SR_BYP_EN);

		/* 6. wait for 2ms */
		usleep_range(2000, 3000);

		/* 7. make sure VCHG voltage is above 90% of booster voltage */
		for (i = 0; i < 3; i++) {
			usleep_range(1000, 2000);
			volt = read_voltage(0);
			if (volt < booster_volt) {
				/* voltage too low, should stop it */
				pr_info("boost fail, should stop it!\n");
				regmap_update_bits(vbus_info->chip->regmap, PM830_PRE_REGULATOR,
						PM830_USB_OTG_EN, 0);
				break;
			}
		}

		/* 8. A0/A1/B0 - [0xdb].[3,2,0] = 3'b0
		 *    B1/B2 - clear OTG_STR_BYP_EN
		 */
		if (vbus_info->chip->version <= PM830_B0_VERSION)
			regmap_update_bits(vbus_info->chip->regmap, 0xdb, 0xd, 0x0);
		else
			regmap_update_bits(vbus_info->chip->regmap, PM830_OTG_CTRL2,
					OTG_SR_BYP_EN, 0);

		/*
		 * 9. restore booster high-side/low-side over current
		 * protection threshold
		 */
		regmap_update_bits(vbus_info->chip->regmap, PM830_OTG_CTRL3,
				OTG_HIGH_SIDE_MASK, OTG_HIGH_SIDE_SET(4));
		regmap_update_bits(vbus_info->chip->regmap, PM830_OTG_CTRL3,
				OTG_LOW_SIDE_MASK, OTG_LOW_SIDE_SET(0));

		/* 10. A0/A1/B0 - close test page
		 *     B1/B2 - don't need to operate test page
		 */
		if (vbus_info->chip->version <= PM830_B0_VERSION)
			regmap_update_bits(vbus_info->chip->regmap, 0xf0, (1 << 0), 0);
	} else
		ret = regmap_update_bits(vbus_info->chip->regmap,
					 PM830_PRE_REGULATOR,
					 PM830_USB_OTG_EN, 0);

	if (ret)
		return ret;

out:
	usleep_range(10000, 20000);

	ret = pm830_get_vbus(&data);
	if (ret)
		return ret;

	if (data != vbus)
		pr_info("vbus set failed %x\n", vbus);
	else
		pr_info("vbus set done %x\n", vbus);

	return 0;
}

static int pm830_read_id_val(unsigned int *level)
{
	int ret, data;
	unsigned int val;
	unsigned int meas1, meas2, upp_th, low_th;

	switch (vbus_info->id_gpadc) {
	case PM830_GPADC0:
		meas1 = PM830_GPADC0_MEAS1;
		meas2 = PM830_GPADC0_MEAS2;
		low_th = PM830_GPADC0_LOW_TH;
		upp_th = PM830_GPADC0_UPP_TH;
		break;
	case PM830_GPADC1:
		meas1 = PM830_GPADC1_MEAS1;
		meas2 = PM830_GPADC1_MEAS2;
		low_th = PM830_GPADC1_LOW_TH;
		upp_th = PM830_GPADC1_UPP_TH;
		break;
	default:
		return -ENODEV;
	}

	ret = regmap_read(vbus_info->chip->regmap, meas1, &val);
	data = val << 4;
	if (ret)
		return ret;

	ret = regmap_read(vbus_info->chip->regmap, meas2, &val);
	data |= val & 0x0f;
	if (ret)
		return ret;

	/* choose 0x100(87.5mV) as threshold */
	if (data > 0x100) {
		regmap_write(vbus_info->chip->regmap, low_th, 0x10);
		regmap_write(vbus_info->chip->regmap, upp_th, 0xff);
		*level = 1;
	} else {
		regmap_write(vbus_info->chip->regmap, low_th, 0);
		regmap_write(vbus_info->chip->regmap, upp_th, 0x10);
		*level = 0;
	}

	return 0;
};

int pm830_init_id(void)
{
	return 0;
}

static irqreturn_t pm830_vbus_handler(int irq, void *data)
{

	struct pm830_vbus_info *info = data;
	/*
	 * 88pm830 has no ability to distinguish
	 * AC/USB charger, so notify usb framework to do it
	 */
	pxa_usb_notify(PXA_USB_DEV_OTG, EVENT_VBUS, 0);
	dev_dbg(info->chip->dev, "88pm830 vbus interrupt is served..\n");
	return IRQ_HANDLED;
}

static irqreturn_t pm830_id_handler(int irq, void *data)
{
	struct pm830_vbus_info *info = data;

	 /* notify to wake up the usb subsystem if ID pin is pulled down */
	pxa_usb_notify(PXA_USB_DEV_OTG, EVENT_ID, 0);
	dev_dbg(info->chip->dev, "88pm830 id interrupt is served..\n");
	return IRQ_HANDLED;
}

static void pm830_vbus_config(struct pm830_vbus_info *info)
{
	unsigned int en, low_th, upp_th;

	if (!info)
		return;

	/* 1. set booster voltage to 5.0V */
	regmap_update_bits(info->chip->regmap, PM830_OTG_CTRL1,
			OTG_VOLT_MASK, OTG_VOLT_SET(5));

	/* 2. set id gpadc low/upp threshold and enable it */
	switch (info->id_gpadc) {
	case PM830_GPADC0:
		low_th = PM830_GPADC0_LOW_TH;
		upp_th = PM830_GPADC0_UPP_TH;
		en = PM830_GPADC0_MEAS_EN;
		break;
	case PM830_GPADC1:
		low_th = PM830_GPADC1_LOW_TH;
		upp_th = PM830_GPADC1_UPP_TH;
		en = PM830_GPADC1_MEAS_EN;
		break;
	default:
		return;
	}

	/* set the threshold for GPADC to prepare for interrupt */
	regmap_write(info->chip->regmap, low_th, 0x10);
	regmap_write(info->chip->regmap, upp_th, 0xff);

	regmap_update_bits(info->chip->regmap, PM830_GPADC_MEAS_EN, en, en);
}

static int pm830_vbus_dt_init(struct device_node *np,
			     struct device *dev,
			     struct pm830_usb_pdata *pdata)
{
	int ret;
	ret = of_property_read_u32(np, "gpadc-number", &pdata->id_gpadc);
	if (ret) {
		dev_info(dev, "There is no valid gpadc for id detection\n");
		pdata->id_gpadc = -1;
	}
	return 0;
}

static int pm830_vbus_probe(struct platform_device *pdev)
{
	struct pm830_chip *chip = dev_get_drvdata(pdev->dev.parent);
	struct pm830_usb_pdata *pdata;
	struct pm830_vbus_info *usb;
	struct device_node *node = pdev->dev.of_node;
	int ret;

	usb = devm_kzalloc(&pdev->dev,
			   sizeof(struct pm830_vbus_info), GFP_KERNEL);
	if (!usb)
		return -ENOMEM;

	pdata = pdev->dev.platform_data;
	if (IS_ENABLED(CONFIG_OF)) {
		if (!pdata) {
			pdata = devm_kzalloc(&pdev->dev,
					     sizeof(*pdata), GFP_KERNEL);
			if (!pdata)
				return -ENOMEM;
		}
		ret = pm830_vbus_dt_init(node, &pdev->dev, pdata);
		if (ret)
			goto out;
	} else if (!pdata) {
		return -EINVAL;
	}

	usb->chip = chip;
	usb->vbus_gpio = pdata->vbus_gpio;
	usb->id_gpadc = pdata->id_gpadc;

	pm830_vbus_config(usb);

	usb->vbus_irq = platform_get_irq(pdev, 0);
	if (usb->vbus_irq < 0) {
		dev_err(&pdev->dev, "failed to get vbus irq\n");
		ret = -ENXIO;
		goto out;
	}

	ret = devm_request_threaded_irq(&pdev->dev, usb->vbus_irq, NULL,
					pm830_vbus_handler,
					IRQF_ONESHOT | IRQF_NO_SUSPEND,
					"usb detect", usb);
	if (ret) {
		dev_info(&pdev->dev,
			"cannot request irq for VBUS, return\n");
		goto out;
	}

	if (usb->id_gpadc != -1) {
		usb->id_irq = platform_get_irq(pdev, usb->id_gpadc + 1);
		if (usb->id_irq < 0) {
			dev_err(&pdev->dev, "failed to get idpin irq\n");
			ret = -ENXIO;
			goto out;
		}

		ret = devm_request_threaded_irq(&pdev->dev, usb->id_irq, NULL,
						pm830_id_handler,
						IRQF_ONESHOT | IRQF_NO_SUSPEND,
						"id detect", usb);
		if (ret) {
			dev_info(&pdev->dev,
				"cannot request irq for idpin, return\n");
			goto out;
		}
	}

	/* global variable used by get/set_vbus */
	vbus_info = usb;

	platform_set_drvdata(pdev, usb);
	device_init_wakeup(&pdev->dev, 1);

	pxa_usb_set_extern_call(PXA_USB_DEV_OTG, vbus, set_vbus,
				pm830_set_vbus);
	pxa_usb_set_extern_call(PXA_USB_DEV_OTG, vbus, get_vbus,
				pm830_get_vbus);
	if (usb->id_gpadc != -1) {
		pxa_usb_set_extern_call(PXA_USB_DEV_OTG, idpin, get_idpin,
					pm830_read_id_val);
		pxa_usb_set_extern_call(PXA_USB_DEV_OTG, idpin, init,
					pm830_init_id);
	}

	/*
	 * GPADC is enabled in otg driver via calling pm830_init_id()
	 * without pm830_init_id(), the GPADC is disabled
	 * pm830_init_id();
	 */

	return 0;

out:
	return ret;
}

static int pm830_vbus_remove(struct platform_device *pdev)
{
	platform_set_drvdata(pdev, NULL);

	return 0;
}

#ifdef CONFIG_PM
static SIMPLE_DEV_PM_OPS(pm830_vbus_pm_ops, pm830_dev_suspend,
			 pm830_dev_resume);
#endif

static const struct of_device_id pm830_vbus_dt_match[] = {
	{ .compatible = "marvell,88pm830-vbus", },
	{ },
};
MODULE_DEVICE_TABLE(of, pm830_fg_dt_match);

static struct platform_driver pm830_vbus_driver = {
	.driver		= {
		.name	= "88pm830-vbus",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(pm830_vbus_dt_match),
	},
	.probe		= pm830_vbus_probe,
	.remove		= pm830_vbus_remove,
};

static int pm830_vbus_init(void)
{
	return platform_driver_register(&pm830_vbus_driver);
}
module_init(pm830_vbus_init);

static void pm830_vbus_exit(void)
{
	platform_driver_unregister(&pm830_vbus_driver);
}
module_exit(pm830_vbus_exit);

MODULE_DESCRIPTION("VBUS driver for Marvell Semiconductor 88PM830");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:88pm830-vbus");
