/*
 * Base driver for Marvell 88PM830
 *
 * Copyright (C) 2013 Marvell International Ltd.
 * Jett Zhou <jtzhou@marvell.com>
 * Yi Zhang <yizhang@marvell.com>
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License. See the file "COPYING" in the main directory of this
 * archive for more details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/mfd/core.h>
#include <linux/mfd/88pm830.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/debugfs.h>
#include <linux/edge_wakeup_mmp.h>

/* number of INT_ENA & INT_STATUS regs */
#define PM830_INT_REG_NUM			(2)
/* Interrupt Registers */
#define PM830_INT_STATUS1		(0x04)
#define PM830_CHG_INT_STS1		(1 << 0)
#define PM830_BAT_INT_STS1		(1 << 1)
#define PM830_CC_INT_STS1			(1 << 2)
#define PM830_OV_TEMP_INT_STS1			(1 << 3)
#define PM830_CHG_DONE_INT_STS1			(1 << 4)
#define PM830_CHG_TOUT_INT_STS1		(1 << 5)
#define PM830_CHG_FAULT_INT_STS1	(1 << 6)
#define PM830_SLP_INT_STS1		(1 << 7)

#define PM830_INT_STATUS2		(0x05)
#define PM830_VBAT_INT_STS2		(1 << 0)
#define PM830_VSYS_INT_STS2		(1 << 1)
#define PM830_VCHG_INT_STS2		(1 << 2)
#define PM830_VPWR_INT_STS2		(1 << 3)
#define PM830_ITEMP_INT_STS2		(1 << 4)
#define PM830_GPADC0_INT_STS2		(1 << 5)
#define PM830_GPADC1_INT_STS2		(1 << 6)
#define PM830_CFOUT_INT_STS2		(1 << 7)

#define PM830_INT_ENA_1		(0x08)
#define PM830_CHG_INT_ENA1		(1 << 0)
#define PM830_BAT_INT_ENA1		(1 << 1)
#define PM830_CC_INT_ENA1		(1 << 2)
#define PM830_OV_TEMP_INT_ENA1		(1 << 3)
#define PM830_CHG_DONE_INT_ENA1		(1 << 4)
#define PM830_CHG_TOUT_INT_ENA1		(1 << 5)
#define PM830_CHG_FAULT_INT_ENA1	(1 << 6)
#define PM830_SLP_INT_ENA1		(1 << 7)

#define PM830_INT_ENA_2		(0x09)
#define PM830_VBAT_INT_ENA2		(1 << 0)
#define PM830_VSYS_INT_ENA2		(1 << 1)
#define PM830_VCHG_INT_ENA2		(1 << 2)
#define PM830_VPWR_INT_ENA2		(1 << 3)
#define PM830_ITEMP_INT_ENA2		(1 << 4)
#define PM830_GPADC0_INT_ENA2		(1 << 5)
#define PM830_GPADC1_INT_ENA2		(1 << 6)
#define PM830_CFOUT_INT_ENA2		(1 << 7)

#define PM830_CHIP_ID		(0x00)

/* global pointer */
static struct regmap *pm830_map;

static const struct i2c_device_id pm830_id_table[] = {
	{"88PM830", -1},
	{} /* NULL terminated */
};
MODULE_DEVICE_TABLE(i2c, pm830_id_table);

static const struct of_device_id pm830_dt_ids[] = {
	{ .compatible = "marvell,88pm830", },
	{},
};
MODULE_DEVICE_TABLE(of, pm830_dt_ids);


/* Interrupt Number in 88PM830 */
enum {
	PM830_IRQ_CHG,		/*EN1b0 */
	PM830_IRQ_BAT,		/*EN1b1 */
	PM830_IRQ_CC,		/*EN1b2 */
	PM830_IRQ_OV_TEMP,	/*EN1b3 */
	PM830_IRQ_CHG_DONE,	/*EN1b4 */
	PM830_IRQ_CHG_TOUT,	/*EN1b5 */
	PM830_IRQ_CHG_FAULT,	/*EN1b6 */
	PM830_IRQ_SLP,		/*EN1b7 */

	PM830_IRQ_VBAT,		/*EN2b0 *//*10 */
	PM830_IRQ_VSYS,		/*EN2b1 */
	PM830_IRQ_VCHG,		/*EN2b2 */
	PM830_IRQ_VPWR,		/*EN2b3 */
	PM830_IRQ_ITEMP,	/*EN2b4 */
	PM830_IRQ_GPADC0,	/*EN2b5 */
	PM830_IRQ_GPADC1,	/*EN2b6 */
	PM830_IRQ_CFOUT,	/*EN2b7 */

	PM830_MAX_IRQ,
};

const struct regmap_config pm830_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};

static struct resource chg_resources[] = {
	{PM830_IRQ_OV_TEMP, PM830_IRQ_OV_TEMP,
		"charger internal temp", IORESOURCE_IRQ,},
	{PM830_IRQ_CHG_DONE, PM830_IRQ_CHG_DONE,
		"charge done", IORESOURCE_IRQ,},
	{PM830_IRQ_CHG_TOUT, PM830_IRQ_CHG_TOUT,
		"charge timeout", IORESOURCE_IRQ,},
	{PM830_IRQ_CHG_FAULT, PM830_IRQ_CHG_FAULT,
		"charge fault", IORESOURCE_IRQ,},
	{PM830_IRQ_VBAT, PM830_IRQ_VBAT,
		"battery voltage", IORESOURCE_IRQ,},

};

static struct resource bat_resources[] = {
	{PM830_IRQ_BAT,  PM830_IRQ_BAT,  "battery detect",  IORESOURCE_IRQ,},
	{PM830_IRQ_GPADC0, PM830_IRQ_GPADC0,  "gpadc0-tbat", IORESOURCE_IRQ,},
	{PM830_IRQ_CC,  PM830_IRQ_CC,  "columb counter", IORESOURCE_IRQ,},
	{PM830_IRQ_SLP, PM830_IRQ_SLP, "charge sleep", IORESOURCE_IRQ,},
};

static struct resource usb_resources[] = {
	{PM830_IRQ_CHG,  PM830_IRQ_CHG,  "usb detect",  IORESOURCE_IRQ,},
	{PM830_IRQ_GPADC0,  PM830_IRQ_GPADC0,  "gpadc0-id",  IORESOURCE_IRQ,},
	{PM830_IRQ_GPADC1,  PM830_IRQ_GPADC1,  "gpadc1-id",  IORESOURCE_IRQ,},
};

static struct mfd_cell chg_devs[] = {
	{
		.name = "88pm830-chg",
		.of_compatible = "marvell,88pm830-chg",
		.num_resources = ARRAY_SIZE(chg_resources),
		.resources = &chg_resources[0],
		.id = -1,
	},
};

static struct mfd_cell bat_devs[] = {
	{
		.name = "88pm830-bat",
		.of_compatible = "marvell,88pm830-bat",
		.num_resources = ARRAY_SIZE(bat_resources),
		.resources = &bat_resources[0],
		.id = -1,
	},
};

static struct mfd_cell usb_devs[] = {
	{
		.name = "88pm830-vbus",
		.of_compatible = "marvell,88pm830-vbus",
		.num_resources = ARRAY_SIZE(usb_resources),
		.resources = &usb_resources[0],
		.id = -1,
	},
};

static struct mfd_cell led_devs[] = {
	{
		.name = "88pm830-led",
		.of_compatible = "marvell,88pm830-led",
		.id = PM830_FLASH_LED,
	},
	{
		.name = "88pm830-led",
		.of_compatible = "marvell,88pm830-led",
		.id = PM830_TORCH_LED,
	},
};

static struct mfd_cell debug_devs[] = {
	{
		.name = "88pm830-debug",
		.of_compatible = "marvell,88pm830-debug",
		.id = -1,
	},
};

static const struct regmap_irq pm830_irqs[] = {
	/* INT0 */
	[PM830_IRQ_CHG] = {
		.mask = PM830_CHG_INT_ENA1,
	},
	[PM830_IRQ_BAT] = {
		.mask = PM830_BAT_INT_ENA1,
	},
	[PM830_IRQ_CC] = {
		.mask = PM830_CC_INT_ENA1,
	},
	[PM830_IRQ_OV_TEMP] = {
		.mask = PM830_OV_TEMP_INT_ENA1,
	},
	[PM830_IRQ_CHG_DONE] = {
		.mask = PM830_CHG_DONE_INT_ENA1,
	},
	[PM830_IRQ_CHG_TOUT] = {
		.mask = PM830_CHG_TOUT_INT_ENA1,
	},
	[PM830_IRQ_CHG_FAULT] = {
		.mask = PM830_CHG_FAULT_INT_ENA1,
	},
	[PM830_IRQ_SLP] = {
		.mask = PM830_SLP_INT_ENA1,
	},
	/* INT1 */
	[PM830_IRQ_VBAT] = {
		.reg_offset = 1,
		.mask = PM830_VBAT_INT_ENA2,
	},
	[PM830_IRQ_VSYS] = {
		.reg_offset = 1,
		.mask = PM830_VSYS_INT_ENA2,
	},
	[PM830_IRQ_VCHG] = {
		.reg_offset = 1,
		.mask = PM830_VCHG_INT_ENA2,
	},
	[PM830_IRQ_VPWR] = {
		.reg_offset = 1,
		.mask = PM830_VPWR_INT_ENA2,
	},
	[PM830_IRQ_ITEMP] = {
		.reg_offset = 1,
		.mask = PM830_ITEMP_INT_ENA2,
	},
	[PM830_IRQ_GPADC0] = {
		.reg_offset = 1,
		.mask = PM830_GPADC0_INT_ENA2,
	},
	[PM830_IRQ_GPADC1] = {
		.reg_offset = 1,
		.mask = PM830_GPADC1_INT_ENA2,
	},
	[PM830_IRQ_CFOUT] = {
		.reg_offset = 1,
		.mask = PM830_CFOUT_INT_ENA2,
	},
};

static int device_gpadc_init_830(struct pm830_chip *chip)
{
	struct regmap *map = chip->regmap;
	int ret = -EINVAL;

	/*
	 * 1. in case gpadc0 is the only enabled measurement
	 * and the gpadc is enabled in non-stop mode
	 * at least another measurement should be enabled
	 */
	ret = regmap_update_bits(map, PM830_GPADC_MEAS_EN,
				 PM830_VBAT_MEAS_EN, PM830_VBAT_MEAS_EN);
	if (ret < 0)
		return ret;

	/*
	 * needn't to set these registers if obm is in charge of detecting
	 * battery presentation
	 */
	if (!chip->obm_config_bat_det) {
		/*
		 * 2. enable battery detection
		 * gpadc0 is used by default: BD_GP_SEL(bit6) of 0x6e
		 * TODO: add more choice here
		 */
		ret = regmap_update_bits(map, PM830_GPADC_CONFIG2,
					 PM830_BD_EN, PM830_BD_EN);
		if (ret < 0)
			return ret;
		/*
		 * 3. in case the gpadc0 is used to measue thermistor
		 * resistor
		 */
		ret = regmap_update_bits(map, PM830_GPADC_CONFIG2,
					 PM830_BD_PREBIAS, PM830_BD_PREBIAS);
		if (ret < 0)
			return ret;
	}

	/*
	 * 4. GPADC main enable
	 * sub-gpadc enable should be done in the client-dev driver
	 */
	ret = regmap_update_bits(map, PM830_GPADC_CONFIG1,
				 PM830_GPADC_GPFSM_EN, PM830_GPADC_GPFSM_EN);
	if (ret < 0)
		return ret;

	/*
	 * 5. PREBIAS_GP0/1 can't lower than 3, it's B1 change and
	 * from silicon design team
	 */
	if (chip->version > PM830_B0_VERSION) {
		ret = regmap_update_bits(map, PM830_GPADC0_BIAS,
				GP_PREBIAS(0xf), GP_PREBIAS(0x3));
		if (ret < 0)
			return ret;
		ret = regmap_update_bits(map, PM830_GPADC1_BIAS,
				GP_PREBIAS(0xf), GP_PREBIAS(0x3));
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int device_irq_init_830(struct pm830_chip *chip)
{
	struct regmap *map = chip->regmap;
	unsigned long flags = chip->irq_flags;
	int data, mask, ret = -EINVAL;

	if (!map || !chip->irq) {
		dev_err(chip->dev, "incorrect parameters\n");
		return -EINVAL;
	}

	/*
	 * the way of clearing interrupt:
	 * it's read-clear by default, leave as it is;
	 */
	mask = PM830_INT_CLEAR | PM830_INV_INT | PM830_INT_MASK;
	data = (0 << 1);
	/*
	 * interrupt is set only when the enable bit is set
	 * interrupt active low and read cleared
	 */
	ret = regmap_update_bits(map, PM830_MISC1, mask, data);

	if (ret < 0)
		goto out;
	/* read to clear the interrupt status registers */
	regmap_read(map, PM830_INT_STATUS1, &data);
	regmap_read(map, PM830_INT_STATUS2, &data);

	ret =
		regmap_add_irq_chip(chip->regmap, chip->irq, flags, -1,
				    chip->regmap_irq_chip, &chip->irq_data);

out:
	return ret;
}

static void device_irq_exit_830(struct pm830_chip *chip)
{
	regmap_del_irq_chip(chip->irq, chip->irq_data);
}

static struct regmap_irq_chip pm830_irq_chip = {
	.name = "88pm830",
	.irqs = pm830_irqs,
	.num_irqs = ARRAY_SIZE(pm830_irqs),

	.num_regs = 2,
	.status_base = PM830_INT_STATUS1,
	.mask_base = PM830_INT_ENA_1,
	.ack_base = PM830_INT_STATUS1,
	.mask_invert = 1,
};

static int device_830_init(struct pm830_chip *chip,
			   struct pm830_platform_data *pdata)
{
	int ret = 0;
	unsigned int val;
	struct regmap *map = chip->regmap;

	if (!map) {
		dev_err(chip->dev, "regmap is invalid\n");
		return -EINVAL;
	}

	ret = regmap_read(map, PM830_CHIP_ID, &val);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to read CHIP ID: %d\n", ret);
		return ret;
	}
	chip->version = val;
	dev_dbg(chip->dev, "88pm830 version: [0x%x]\n", chip->version);

	/*
	 * Config Sigma Delta converter
	 * these registers are needed both in fuel gauge & charger.
	 * so enable them here
	 * 1) Enable system clock of fg
	 * 2) Sigma-delta power up
	 */
	regmap_update_bits(map, PM830_MISC2, FGC_CLK_EN, FGC_CLK_EN);
	regmap_update_bits(map, PM830_CC_CTRL1,
			   PM830_SD_PWRUP, PM830_SD_PWRUP);

	/* used to improve supplement mode entry in previous versions of PM830 B1, it's from silicon
	 * design team
	 */
	if (chip->version < PM830_B1_VERSION) {
		regmap_write(map, 0xf0, 0x1);
		regmap_update_bits(map, 0xd8, (0x1 << 5), (0x1 << 5));
		regmap_write(map, 0xf0, 0x0);
		regmap_update_bits(map, 0x52, (0x1 << 4), (0x1 << 4));
	}

	return 0;
}

static int device_chg_init(struct pm830_chip *chip)
{
	return mfd_add_devices(chip->dev, 0, &chg_devs[0],
			      ARRAY_SIZE(chg_devs), &chg_resources[0],
			      0, regmap_irq_get_domain(chip->irq_data));
}

static int device_bat_init(struct pm830_chip *chip)
{
	return mfd_add_devices(chip->dev, 0, &bat_devs[0],
			       ARRAY_SIZE(bat_devs), &bat_resources[0],
			       0, regmap_irq_get_domain(chip->irq_data));
}

static int device_usb_init(struct pm830_chip *chip)
{
	return mfd_add_devices(chip->dev, 0, &usb_devs[0],
			       ARRAY_SIZE(usb_devs), &usb_resources[0],
			       0, regmap_irq_get_domain(chip->irq_data));
}

static int device_led_init(struct pm830_chip *chip)
{
	return mfd_add_devices(chip->dev, 0, &led_devs[0],
			       ARRAY_SIZE(led_devs), NULL, 0, NULL);
}

static int device_debug_init(struct pm830_chip *chip)
{
	return mfd_add_devices(chip->dev, 0, &debug_devs[0],
			       ARRAY_SIZE(debug_devs), NULL, 0, NULL);
}

static int pm830_dt_init(struct device_node *np,
			 struct device *dev,
			 struct pm830_platform_data *pdata)
{
	of_property_read_u32(np, "marvell,88pm830-irq-flags",
			     &pdata->irq_flags);

	pdata->obm_config_bat_det = of_property_read_bool(np,
			"obm-config-bat-det");

	if (of_property_read_u32(np, "edge-wakeup-gpio", &pdata->edge_wakeup_gpio))
		pdata->edge_wakeup_gpio = -1;
		dev_info(dev, "no edge-wakeup-gpio defined\n");

	return 0;
}

static int pm830_probe(struct i2c_client *client,
		       const struct i2c_device_id *id)
{
	int ret = 0;
	struct pm830_chip *chip;
	struct pm830_platform_data *pdata = client->dev.platform_data;
	struct device_node *node = client->dev.of_node;
	struct regmap *map;

	if (IS_ENABLED(CONFIG_OF)) {
		if (!pdata) {
			pdata = devm_kzalloc(&client->dev,
					     sizeof(*pdata), GFP_KERNEL);
			if (!pdata)
				return -ENOMEM;
		}
		ret = pm830_dt_init(node, &client->dev, pdata);
		if (ret)
			goto err_out;
	} else if (!pdata) {
		return -EINVAL;
	}

	chip = devm_kzalloc(&client->dev,
			    sizeof(struct pm830_chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	map = devm_regmap_init_i2c(client, &pm830_regmap_config);
	if (IS_ERR(map)) {
		ret = PTR_ERR(map);
		dev_err(&client->dev,
			"Failed to allocate register map: %d\n", ret);
		goto err_out;
	}

	chip->client = client;
	chip->regmap = pm830_map = map;
	chip->dev = &client->dev;
	chip->obm_config_bat_det = pdata->obm_config_bat_det;
	chip->edge_wakeup_gpio = pdata->edge_wakeup_gpio;
	chip->get_fg_internal_soc = NULL;

	dev_set_drvdata(chip->dev, chip);
	i2c_set_clientdata(chip->client, chip);

	ret = device_830_init(chip, pdata);
	if (ret) {
		dev_err(chip->dev, "failed to init 88pm830\n");
		goto err_out;
	}

	ret = device_gpadc_init_830(chip);
	if (ret < 0) {
		dev_err(chip->dev, "failed to init 88pm830 gpadc!\n");
		goto err_out;
	}

	chip->regmap_irq_chip = &pm830_irq_chip;
	chip->irq = client->irq;
	chip->irq_flags = pdata->irq_flags;
	ret = device_irq_init_830(chip);
	if (ret < 0) {
		dev_err(chip->dev, "failed to init 88pm830 irq!\n");
		goto err_out;
	}

	if (IS_ENABLED(CONFIG_CHARGER_88PM830)) {
		ret = device_chg_init(chip);
		if (ret) {
			dev_err(chip->dev, "failed to add charger subdev\n");
			goto out_dev;
		}
	}

	if (IS_ENABLED(CONFIG_BATTERY_88PM830)) {
		ret = device_bat_init(chip);
		if (ret) {
			dev_err(chip->dev, "failed to add battery subdev\n");
			goto out_dev;
		}
	}

	if (IS_ENABLED(CONFIG_VBUS_88PM830)) {
		ret = device_usb_init(chip);
		if (ret) {
			dev_err(chip->dev, "failed to add vbus subdev\n");
			goto out_dev;
		}
	}

	if (IS_ENABLED(CONFIG_LEDS_88PM830)) {
		ret = device_led_init(chip);
		if (ret) {
			dev_err(chip->dev, "failed to add led subdev\n");
			goto out_dev;
		}
	}

	ret = device_debug_init(chip);
	if (ret) {
		dev_err(chip->dev, "failed to add debug subdev\n");
		goto out_dev;
	}

	/* use 88pm830 as wakeup source */
	device_init_wakeup(&client->dev, 1);
	if (chip->edge_wakeup_gpio >= 0) {
		ret = request_mfp_edge_wakeup(chip->edge_wakeup_gpio,
					      NULL, chip, chip->dev);
		if (ret) {
			dev_err(chip->dev, "failed to request edge wakeup.\n");
			goto err_mfp_wakeup;
		}
	}

	return 0;

err_mfp_wakeup:
	if (chip->edge_wakeup_gpio >= 0)
		remove_mfp_edge_wakeup(chip->edge_wakeup_gpio);
out_dev:
	mfd_remove_devices(chip->dev);
err_out:
	return ret;
}

static int pm830_remove(struct i2c_client *client)
{
	struct pm830_chip *chip = i2c_get_clientdata(client);

	mfd_remove_devices(chip->dev);
	device_irq_exit_830(chip);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int pm830_suspend(struct device *dev)
{
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	struct pm830_chip *chip = i2c_get_clientdata(client);
	if (chip && device_may_wakeup(chip->dev))
		enable_irq_wake(chip->irq);

	return 0;
}

static int pm830_resume(struct device *dev)
{
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	struct pm830_chip *chip = i2c_get_clientdata(client);

	if (chip && device_may_wakeup(chip->dev))
		disable_irq_wake(chip->irq);

	return 0;
}
#endif
SIMPLE_DEV_PM_OPS(pm830_pm_ops, pm830_suspend, pm830_resume);

static const struct of_device_id pm830_dt_match[] = {
	{ .compatible = "marvell,88pm830", },
	{ },
};

MODULE_DEVICE_TABLE(of, pm830_dt_match);

static struct i2c_driver pm830_driver = {
	.driver = {
		.name = "88PM830",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(pm830_dt_match),
		.pm = &pm830_pm_ops,
	},
	.probe = pm830_probe,
	.remove = pm830_remove,
	.id_table = pm830_id_table,
};

static int __init pm830_i2c_init(void)
{
	return i2c_add_driver(&pm830_driver);
}
subsys_initcall(pm830_i2c_init);

static void __exit pm830_i2c_exit(void)
{
	i2c_del_driver(&pm830_driver);
}
module_exit(pm830_i2c_exit);

MODULE_DESCRIPTION("I2C interface for Marvell 88PM830");
MODULE_AUTHOR("Jett Zhou <jtzhou@marvell.com>");
MODULE_LICENSE("GPL");
