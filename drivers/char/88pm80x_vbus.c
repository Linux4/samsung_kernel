/*
 * 88pm80x vbus driver for Marvell USB
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/mfd/88pm80x.h>
#include <linux/delay.h>
#include <linux/platform_data/mv_usb.h>
#include <linux/of.h>
#include <linux/of_device.h>

struct pm80x_vbus_info {
	struct pm80x_chip	*chip;
	struct pm80x_subchip	*subchip;
	int			irq;
	int			id_irq;
	int			vbus_gpio;
	int			id_gpadc;
};

static struct pm80x_vbus_info *vbus_info;

static int pm80x_read_vbus_val(unsigned int *level)
{
	int ret;
	unsigned int val;

	ret = regmap_read(vbus_info->chip->regmap, PM800_STATUS_1, &val);
	if (ret)
		return ret;

	if (val & PM800_CHG_STS1)
		*level = VBUS_HIGH;
	else
		*level = VBUS_LOW;
	return 0;
}

static int pm80x_read_id_val(unsigned int *level)
{
	int ret, data;
	unsigned int val;
	unsigned int meas1, meas2, upp_th, low_th;

	switch (vbus_info->id_gpadc) {
	case PM800_GPADC0:
		meas1 = PM800_GPADC0_MEAS1;
		meas2 = PM800_GPADC0_MEAS2;
		low_th = PM800_GPADC0_LOW_TH;
		upp_th = PM800_GPADC0_UPP_TH;
		break;
	case PM800_GPADC1:
		meas1 = PM800_GPADC1_MEAS1;
		meas2 = PM800_GPADC1_MEAS2;
		low_th = PM800_GPADC1_LOW_TH;
		upp_th = PM800_GPADC1_UPP_TH;
		break;
	case PM800_GPADC2:
		meas1 = PM800_GPADC2_MEAS1;
		meas2 = PM800_GPADC2_MEAS2;
		low_th = PM800_GPADC2_LOW_TH;
		upp_th = PM800_GPADC2_UPP_TH;
		break;
	case PM800_GPADC3:
		meas1 = PM800_GPADC3_MEAS1;
		meas2 = PM800_GPADC3_MEAS2;
		low_th = PM800_GPADC3_LOW_TH;
		upp_th = PM800_GPADC3_UPP_TH;
		break;
	case PM800_GPADC4:
		meas1 = PM800_GPADC4_MEAS1;
		meas2 = PM800_GPADC4_MEAS2;
		low_th = PM800_GPADC4_LOW_TH;
		upp_th = PM800_GPADC4_UPP_TH;
		break;
	default:
		return -ENODEV;
	}

	ret = regmap_read(vbus_info->subchip->regmap_gpadc, meas1, &val);
	data = val << 4;
	if (ret)
		return ret;

	ret = regmap_read(vbus_info->subchip->regmap_gpadc, meas2, &val);
	data |= val & 0x0F;
	if (ret)
		return ret;
	if (data > 0x100) {
		regmap_write(vbus_info->subchip->regmap_gpadc, low_th, 0x10);
		if (ret)
			return ret;

		regmap_write(vbus_info->subchip->regmap_gpadc, upp_th, 0xff);
		if (ret)
			return ret;

		*level = 1;
	} else {
		regmap_write(vbus_info->subchip->regmap_gpadc, low_th, 0);
		if (ret)
			return ret;

		regmap_write(vbus_info->subchip->regmap_gpadc, upp_th, 0x10);
		if (ret)
			return ret;

		*level = 0;
	}

	return 0;
};

int pm80x_init_id(void)
{
	int ret;
	unsigned int en;

	switch (vbus_info->id_gpadc) {
	case PM800_GPADC0:
		en = PM800_MEAS_GP0_EN;
		break;
	case PM800_GPADC1:
		en = PM800_MEAS_GP1_EN;
		break;
	case PM800_GPADC2:
		en = PM800_MEAS_GP2_EN;
		break;
	case PM800_GPADC3:
		en = PM800_MEAS_GP3_EN;
		break;
	case PM800_GPADC4:
		en = PM800_MEAS_GP4_EN;
		break;
	default:
		return -ENODEV;
	}


	ret = regmap_update_bits(vbus_info->subchip->regmap_gpadc,
					PM800_GPADC_MEAS_EN2, en, en);
	if (ret)
		return ret;

	ret = regmap_update_bits(vbus_info->subchip->regmap_gpadc,
		PM800_GPADC_MISC_CONFIG2, PM800_GPADC_MISC_GPFSM_EN,
		PM800_GPADC_MISC_GPFSM_EN);
	if (ret)
		return ret;

	return 0;
}

static int pm80x_set_vbus(unsigned int vbus)
{
	int ret;
	unsigned int data = 0, mask, reg = 0;

	switch (vbus_info->vbus_gpio) {
	case PM800_NO_GPIO:
		/* OTG5V not supported - Do nothing */
		return 0;

	case PM800_GPIO0:
		/* OTG5V Enable/Disable is connected to GPIO_0 */
		mask = PM800_GPIO0_GPIO_MODE(0x01) | PM800_GPIO0_VAL;
		reg = PM800_GPIO_0_1_CNTRL;
		break;

	case PM800_GPIO1:
		/* OTG5V Enable/Disable is connected to GPIO_1 */
		mask = PM800_GPIO1_GPIO_MODE(0x01) | PM800_GPIO1_VAL;
		reg = PM800_GPIO_0_1_CNTRL;
		break;

	case PM800_GPIO2:
		/* OTG5V Enable/Disable is connected to GPIO_2 */
		mask = PM800_GPIO2_GPIO_MODE(0x01) | PM800_GPIO2_VAL;
		reg = PM800_GPIO_2_3_CNTRL;
		break;

	case PM800_GPIO3:
		/* OTG5V Enable/Disable is connected to GPIO_3 */
		mask = PM800_GPIO3_GPIO_MODE(0x01) | PM800_GPIO3_VAL;
		reg = PM800_GPIO_2_3_CNTRL;
		break;

	case PM800_GPIO4:
		/* OTG5V Enable/Disable is connected to GPIO_4 */
		mask = PM800_GPIO4_GPIO_MODE(0x01) | PM800_GPIO4_VAL;
		reg = PM800_GPIO_4_5_CNTRL;
		break;

	default:
		return -ENODEV;
	}

	if (vbus == VBUS_HIGH)
		data = mask;

	ret = regmap_update_bits(vbus_info->chip->regmap, reg, mask, data);
	if (ret)
		return ret;

	mdelay(20);

	ret = pm80x_read_vbus_val(&data);
	if (ret)
		return ret;

	if (ret != vbus)
		pr_info("vbus set failed %x\n", vbus);
	else
		pr_info("vbus set done %x\n", vbus);

	return 0;
}

static irqreturn_t vbus_irq(int irq, void *dev)
{
	pxa_usb_notify(PXA_USB_DEV_OTG, EVENT_VBUS, 0);

	return IRQ_HANDLED;
}

static irqreturn_t vbus_id_irq(int irq, void *dev)
{
	pxa_usb_notify(PXA_USB_DEV_OTG, EVENT_ID, 0);

	return IRQ_HANDLED;
}

static int pm80x_vbus_probe(struct platform_device *pdev)
{
	struct pm80x_chip *chip = dev_get_drvdata(pdev->dev.parent);
	struct pm80x_vbus_info *vbus;
	struct device_node *np = pdev->dev.of_node;
	int ret;

	vbus = kzalloc(sizeof(struct pm80x_vbus_info), GFP_KERNEL);
	if (!vbus)
		return -ENOMEM;

	vbus->chip = chip;
	vbus->subchip = chip->subchip;

	ret = of_property_read_u32(np, "vbus-gpio", &vbus->vbus_gpio);
	if (ret) {
		dev_err(&pdev->dev, "failed to get vbus-gpio.\n");
		return ret;
	}
	if (vbus->vbus_gpio == 0xff)
		vbus->vbus_gpio = -1;

	ret = of_property_read_u32(np, "id-gpadc", &vbus->id_gpadc);
	if (ret) {
		dev_err(&pdev->dev, "failed to get id-gpadc.\n");
		return ret;
	}
	if (vbus->id_gpadc == 0xff)
		vbus->id_gpadc = -1;

	/* request vbus irq */
	if (vbus->vbus_gpio != -1) {
		vbus->irq = platform_get_irq(pdev, 0);
		if (vbus->irq < 0) {
			dev_err(&pdev->dev, "failed to get vbus irq\n");
			ret = -ENXIO;
			goto out;
		}

		ret = pm80x_request_irq(vbus->chip, vbus->irq, vbus_irq,
					   IRQF_ONESHOT | IRQF_NO_SUSPEND, "88pm800-vbus", vbus);
		if (ret) {
			dev_info(&pdev->dev,
				"Can not request irq for VBUS, "
				"disable clock gating\n");
		}
	}

	/* request usb id irq */
	if (vbus->id_gpadc != -1) {
		vbus->id_irq = platform_get_irq(pdev, vbus->id_gpadc + 1);
		if (vbus->id_irq < 0) {
			dev_err(&pdev->dev, "failed to get idpin irq\n");
			ret = -ENXIO;
			goto out;
		}
		ret = pm80x_request_irq(vbus->chip, vbus->id_irq, vbus_id_irq,
				IRQF_ONESHOT | IRQF_NO_SUSPEND, "88pm800-vbus-id", vbus);
		if (ret)
			dev_info(&pdev->dev, "Can not request irq for VBUS id\n");
	}

	vbus_info = vbus;
	platform_set_drvdata(pdev, vbus);
	device_init_wakeup(&pdev->dev, 1);

	if (vbus->vbus_gpio != -1) {
		pxa_usb_set_extern_call(PXA_USB_DEV_OTG, vbus, set_vbus,
				pm80x_set_vbus);
		pxa_usb_set_extern_call(PXA_USB_DEV_OTG, vbus, get_vbus,
				pm80x_read_vbus_val);
	}

	if (vbus->id_gpadc != -1) {
		pxa_usb_set_extern_call(PXA_USB_DEV_OTG, idpin, get_idpin,
				pm80x_read_id_val);
		pxa_usb_set_extern_call(PXA_USB_DEV_OTG, idpin, init,
				pm80x_init_id);
	}

	return 0;

out:
	kfree(vbus);
	return ret;
}

static int pm80x_vbus_remove(struct platform_device *pdev)
{
	struct pm80x_vbus_info *vbus = platform_get_drvdata(pdev);

	if (vbus) {
		platform_set_drvdata(pdev, NULL);
		kfree(vbus);
	}

	return 0;
}

#ifdef CONFIG_PM
static int pm80x_vbus_suspend(struct device *dev)
{
	return pm80x_dev_suspend(dev);
}

static int pm80x_vbus_resume(struct device *dev)
{
	return pm80x_dev_resume(dev);
}

static const struct dev_pm_ops pm80x_vbus_pm_ops = {
	.suspend	= pm80x_vbus_suspend,
	.resume		= pm80x_vbus_resume,
};
#endif

static struct platform_driver pm80x_vbus_driver = {
	.driver		= {
		.name	= "88pm80x-usb",
		.owner	= THIS_MODULE,
#ifdef CONFIG_PM
		.pm	= &pm80x_vbus_pm_ops,
#endif
	},
	.probe		= pm80x_vbus_probe,
	.remove		= pm80x_vbus_remove,
};

static int __init pm80x_vbus_init(void)
{
	return platform_driver_register(&pm80x_vbus_driver);
}
module_init(pm80x_vbus_init);

static void __exit pm80x_vbus_exit(void)
{
	platform_driver_unregister(&pm80x_vbus_driver);
}
module_exit(pm80x_vbus_exit);

MODULE_DESCRIPTION("VBUS driver for Marvell Semiconductor 88PM80x");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:88pm80x-usb");
