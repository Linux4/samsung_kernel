/*
 * 88PM830 camera flash/torch LED driver
 *
 * Copyright (C) 2013 Marvell International Ltd.
 *	Keith Prickett <keithp@marvell.com>
 *	Yi Zhang <yizhang@marvell.com>
 *	Fenghang Yin <yinfh@marvell.com>
 * Copyright (C) 2009 Marvell International Ltd.
 *	Haojian Zhuang <haojian.zhuang@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/leds.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/mfd/88pm830.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/delay.h>

#define PM830_BST_CTRL1			(0x59)
#define PM830_BST_CTRL2			(0x5b)
#define PM830_BST_HSOC_OFFSET		(5)
#define PM830_BST_HSOC_MASK		(0x7 << PM830_BST_HSOC_OFFSET)
#define PM830_BST_LSOC_OFFSET		(1)
#define PM830_BST_LSOC_MASK		(0x7 << PM830_BST_LSOC_OFFSET)
#define PM830_BST_CTRL3			(0x5c)
#define PM830_BST_HSZC_OFFSET		(5)
#define PM830_BST_HSZC_MASK		(0x3 << PM830_BST_HSZC_OFFSET)
#define PM830_BST_CTRL4			(0x5d)
#define PM830_BST_ILIM_DIS		(0x1 << 7)

#define PM830_CAMERA_FLASH1		(0x61)
#define PM830_FLASH_ISET_OFFSET		(0)
#define PM830_FLASH_ISET_MASK		(0x1f << PM830_FLASH_ISET_OFFSET)
#define PM830_TORCH_ISET_OFFSET		(5)
#define PM830_TORCH_ISET_MASK		(0x7 << PM830_TORCH_ISET_OFFSET)
#define PM830_CAMERA_FLASH2		(0x62)
#define PM830_BOOST_MODE		(0x1 << 7)
#define PM830_CAMERA_FLASH3		(0x63)
#define PM830_CAMERA_FLASH4		(0x64)
#define PM830_CAMERA_FLASH5		(0x65)
/* 88PM830 bitmask definitions */
#define PM830_CF_CFD_PLS_ON		(1<<0)
#define PM830_CF_BITMASK_TRCH_RESET	(1<<1)
#define PM830_CF_BITMASK_MODE		(1<<4)
#define PM830_SELECT_FLASH_MODE		(1<<4)
#define PM830_SELECT_TORCH_MODE		(0<<4)
#define PM830_CF_EN_HIGH		(1<<6)
#define PM830_CFD_ENABLED		(1<<5)
#define PM830_CFD_MASKED		(0<<5)

#define PM830_LED_ISET(x)		(((x) - 50) / 50)

static unsigned gpio_en;
module_param(gpio_en, uint, 0664);

struct pm830_led {
	struct led_classdev cdev;
	struct work_struct work;
	struct delayed_work delayed_work;
	struct pm830_chip *chip;
	struct mutex lock;
#define MFD_NAME_SIZE		(40)
	char name[MFD_NAME_SIZE];

	unsigned int brightness;
	unsigned int current_brightness;

	int id;
	/* for external CF_EN and CF_TXMSK */
	int gpio_en;
	int cf_en;
	int cf_txmsk;
};

static void clear_errors(struct pm830_led *led)
{
	struct pm830_chip *chip;
	unsigned int reg;

	/*!!! mutex must be locked upon entering this function */

	chip = led->chip;
	regmap_read(chip->regmap, PM830_CAMERA_FLASH5, &reg);
	/* clear all errors, write 1 to clear */
	regmap_write(chip->regmap, PM830_CAMERA_FLASH5, reg);
}

static void pm830_led_delayed_work(struct work_struct *work)
{
	struct pm830_led *led;
	unsigned long jiffies;

	led = container_of(work, struct pm830_led,
			   delayed_work.work);
	mutex_lock(&led->lock);
	clear_errors(led);
	/* set TRCH_TIMER_RESET to reset torch timer so
	 * led won't turn off */
	regmap_update_bits(led->chip->regmap, PM830_CAMERA_FLASH4,
			PM830_CF_BITMASK_TRCH_RESET,
			PM830_CF_BITMASK_TRCH_RESET);
	mutex_unlock(&led->lock);

	/* reschedule torch timer reset */
	jiffies = msecs_to_jiffies(5000);
	schedule_delayed_work(&led->delayed_work, jiffies);
}

static void torch_on(struct pm830_led *led)
{
	unsigned long jiffies;
	struct pm830_chip *chip;
	int ret;

	chip = led->chip;
	mutex_lock(&led->lock);
	/* clear CFD_PLS_ON to disable */
	regmap_update_bits(chip->regmap, PM830_CAMERA_FLASH4,
			PM830_CF_CFD_PLS_ON, 0);
	/* set TRCH_TIMER_RESET to reset torch timer so
	 * led won't turn off */
	regmap_update_bits(chip->regmap, PM830_CAMERA_FLASH4,
			PM830_CF_BITMASK_TRCH_RESET,
			PM830_CF_BITMASK_TRCH_RESET);
	clear_errors(led);
	/* set torch mode, set booster mode to voltage regulation mode */
	regmap_update_bits(chip->regmap, PM830_CAMERA_FLASH2,
			(PM830_CF_BITMASK_MODE | PM830_BOOST_MODE), (PM830_BOOST_MODE));
	/* disable booster HS current limit */
	regmap_update_bits(chip->regmap, PM830_BST_CTRL4,
			PM830_BST_ILIM_DIS, PM830_BST_ILIM_DIS);
	/* automatic booster mode */
	/* set torch current */
	regmap_update_bits(chip->regmap, PM830_CAMERA_FLASH1,
			   PM830_TORCH_ISET_MASK,
			   ((PM830_LED_ISET(led->brightness)) << PM830_TORCH_ISET_OFFSET));
	if (!led->gpio_en)
		/* set CFD_PLS_ON to enable */
		regmap_update_bits(chip->regmap, PM830_CAMERA_FLASH4,
				   PM830_CF_CFD_PLS_ON, PM830_CF_CFD_PLS_ON);
	else {
		ret = devm_gpio_request(led->cdev.dev, led->cf_en, "cf-gpio");
		if (ret) {
			dev_err(led->cdev.dev,
				"request cf-gpio error!\n");
			return;
		}
		gpio_direction_output(led->cf_en, 1);
		devm_gpio_free(led->cdev.dev, led->cf_en);
	}
	mutex_unlock(&led->lock);

	/* Once on, torch will only remain on for 10 seconds so we
	 * schedule work every 5 seconds (5000 msecs) to poke the
	 * chip register so it can remain on */
	jiffies = msecs_to_jiffies(5000);
	schedule_delayed_work(&led->delayed_work, jiffies);
}

static void torch_off(struct pm830_led *led)
{
	struct pm830_chip *chip;
	int ret;

	chip = led->chip;
	cancel_delayed_work_sync(&led->delayed_work);
	mutex_lock(&led->lock);
	if (!led->gpio_en)
		/* clear CFD_PLS_ON to disable */
		regmap_update_bits(chip->regmap, PM830_CAMERA_FLASH4,
				   PM830_CF_CFD_PLS_ON, 0);
	else {
		ret = devm_gpio_request(led->cdev.dev, led->cf_en, "cf-gpio");
		if (ret) {
			dev_err(led->cdev.dev,
				"request cf-gpio error!\n");
			return;
		}
		gpio_direction_output(led->cf_en, 0);
		devm_gpio_free(led->cdev.dev, led->cf_en);
	}
	mutex_unlock(&led->lock);
}

static void strobe_flash(struct pm830_led *led)
{
	struct pm830_chip *chip;
	int ret;

	chip = led->chip;

	mutex_lock(&led->lock);
	/* clear CFD_PLS_ON to disable */
	regmap_update_bits(chip->regmap, PM830_CAMERA_FLASH4,
			PM830_CF_CFD_PLS_ON, 0);
	clear_errors(led);
	/*
	 * set flash mode
	 * if flash current is above 400mA, set booster mode to current regulation mode and enable
	 * booster HS current limit
	 * otherwise set booster mode to voltage regualtion mode and disable booster HS current
	 * limit
	 */
	if (led->brightness > 400) {
		regmap_update_bits(chip->regmap, PM830_CAMERA_FLASH2,
				(PM830_CF_BITMASK_MODE | PM830_BOOST_MODE), PM830_CF_BITMASK_MODE);
		regmap_update_bits(chip->regmap, PM830_BST_CTRL4,
				PM830_BST_ILIM_DIS, 0);
	} else {
		regmap_update_bits(chip->regmap, PM830_CAMERA_FLASH2,
				(PM830_CF_BITMASK_MODE | PM830_BOOST_MODE),
				(PM830_CF_BITMASK_MODE | PM830_BOOST_MODE));
		regmap_update_bits(chip->regmap, PM830_BST_CTRL4,
				PM830_BST_ILIM_DIS, PM830_BST_ILIM_DIS);
	}
	/* automatic booster enable mode*/
	/* set flash current */
	regmap_update_bits(chip->regmap, PM830_CAMERA_FLASH1,
			   PM830_FLASH_ISET_MASK,
			   ((PM830_LED_ISET(led->brightness)) << PM830_FLASH_ISET_OFFSET));
	/* trigger flash */
	if (!led->gpio_en)
		regmap_update_bits(chip->regmap, PM830_CAMERA_FLASH4,
				   PM830_CF_CFD_PLS_ON, PM830_CF_CFD_PLS_ON);
	else {
		ret = devm_gpio_request(led->cdev.dev, led->cf_en, "cf-gpio");
		if (ret) {
			dev_err(led->cdev.dev,
				"request cf-gpio error!\n");
			return;
		}
		gpio_direction_output(led->cf_en, 1);
		usleep_range(1000, 5000);
		gpio_direction_output(led->cf_en, 0);
		devm_gpio_free(led->cdev.dev, led->cf_en);
	}
	mutex_unlock(&led->lock);
}

static void pm830_led_work(struct work_struct *work)
{
	struct pm830_led *led;

	led = container_of(work, struct pm830_led, work);

	if (led->id == PM830_FLASH_LED) {
		if (led->brightness != 0)
			strobe_flash(led);
	} else if (led->id == PM830_TORCH_LED) {
		if ((led->current_brightness == 0) && led->brightness)
			torch_on(led);
		if (led->brightness == 0)
			torch_off(led);

		led->current_brightness = led->brightness;
	}
}

static void pm830_led_bright_set(struct led_classdev *cdev,
			   enum led_brightness value)
{
	struct pm830_led *led = container_of(cdev, struct pm830_led, cdev);

	/* skip the lowest level */
	if (led->id == PM830_FLASH_LED)
		led->brightness = ((value >> 3) + 1) * 50;
	if (led->id == PM830_TORCH_LED)
		led->brightness = ((value >> 5) + 1) * 50;
	if (value == 0)
		led->brightness = 0;
	dev_dbg(led->cdev.dev, "value = %d, brightness = %d\n",
		 value, led->brightness);
	schedule_work(&led->work);
}

static ssize_t gpio_ctrl_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct pm830_led *led;
	struct led_classdev *cdev;

	int s = 0;
	cdev = dev_get_drvdata(dev);
	led = container_of(cdev, struct pm830_led, cdev);

	s += sprintf(buf, "gpio control: %d\n", led->gpio_en);
	return s;
}

static ssize_t gpio_ctrl_store(
		struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct pm830_led *led;
	struct led_classdev *cdev;
	int ret;
	unsigned int data;

	cdev = dev_get_drvdata(dev);
	led = container_of(cdev, struct pm830_led, cdev);

	ret = sscanf(buf, "%d", &gpio_en);
	if (ret < 0) {
		dev_dbg(dev, "%s: get gpio_en error, set to 0 as default\n", __func__);
		gpio_en = 0;
	}

	dev_info(dev, "%s: gpio control %s\n",
		 __func__, (gpio_en ? "enabled" : "disabled"));

	mutex_lock(&led->lock);
	led->gpio_en = gpio_en;
	if (led->id == PM830_FLASH_LED)
		regmap_update_bits(led->chip->regmap, PM830_CAMERA_FLASH2,
				  PM830_SELECT_FLASH_MODE, PM830_SELECT_FLASH_MODE);
	else
		regmap_update_bits(led->chip->regmap, PM830_CAMERA_FLASH2,
				   PM830_SELECT_FLASH_MODE, PM830_SELECT_TORCH_MODE);
	if (gpio_en)
		/* high active */
		ret = regmap_update_bits(led->chip->regmap, PM830_CAMERA_FLASH2,
					 (PM830_CF_EN_HIGH | PM830_CFD_ENABLED),
					 (PM830_CF_EN_HIGH | PM830_CFD_ENABLED));
	else
		ret = regmap_update_bits(led->chip->regmap, PM830_CAMERA_FLASH2,
					 (PM830_CF_EN_HIGH | PM830_CFD_ENABLED),
					 PM830_CFD_MASKED);
	regmap_read(led->chip->regmap, PM830_CAMERA_FLASH2, &data);
	dev_dbg(dev, "%s: [0x62]= 0x%x\n", __func__, data);

	if (ret)
		dev_err(dev, "gpio configure fail: ret = %d\n", ret);

	mutex_unlock(&led->lock);

	return size;
}

static DEVICE_ATTR(gpio_ctrl, S_IRUGO | S_IWUSR, gpio_ctrl_show, gpio_ctrl_store);

static int pm830_led_dt_init(struct device_node *np,
			 struct device *dev,
			 struct pm830_led_pdata *pdata)
{
	int ret;
	ret = of_property_read_u32(np,
				   "flash-en-gpio", &pdata->cf_en);
	if (ret)
		return ret;
	ret = of_property_read_u32(np,
				   "flash-txmsk-gpio", &pdata->cf_txmsk);
	if (ret)
		return ret;
	return 0;
}

static int pm830_led_probe(struct platform_device *pdev)
{
	struct pm830_led *led;
	struct pm830_led_pdata *pdata = pdev->dev.platform_data;
	struct pm830_chip *chip = dev_get_drvdata(pdev->dev.parent);
	struct device_node *node = pdev->dev.of_node;
	int ret;

	led = devm_kzalloc(&pdev->dev, sizeof(struct pm830_led),
			    GFP_KERNEL);
	if (led == NULL)
		return -ENOMEM;

	if (pdev->id == PM830_FLASH_LED)
		strncpy(led->name, "flash", MFD_NAME_SIZE - 1);
	else
		strncpy(led->name, "torch", MFD_NAME_SIZE - 1);

	led->chip = chip;
	led->id = pdev->id;

	if (IS_ENABLED(CONFIG_OF)) {
		if (!pdata) {
			pdata = devm_kzalloc(&pdev->dev,
					     sizeof(*pdata), GFP_KERNEL);
			if (!pdata)
				return -ENOMEM;
		}
		ret = pm830_led_dt_init(node, &pdev->dev, pdata);
		if (ret)
			goto out;
	} else if (!pdata) {
		return -EINVAL;
	}

	led->cf_en = pdata->cf_en;
	led->cf_txmsk = pdata->cf_txmsk;

	led->gpio_en = gpio_en;

	led->current_brightness = 0;
	led->cdev.name = led->name;
	led->cdev.brightness_set = pm830_led_bright_set;
	/* for camera trigger */
	led->cdev.default_trigger = led->name;

	dev_set_drvdata(&pdev->dev, led);

	if (led->gpio_en) {
		/* high active */
		regmap_update_bits(chip->regmap, PM830_CAMERA_FLASH2,
				   (PM830_CF_EN_HIGH | PM830_CFD_ENABLED),
				   (PM830_CF_EN_HIGH | PM830_CFD_ENABLED));

		ret = devm_gpio_request(&pdev->dev, led->cf_en, "cf-gpio");
		if (ret) {
			dev_err(&pdev->dev,
				"request cf-gpio error!\n");
			return ret;
		}
		gpio_direction_output(led->cf_en, 0);
		devm_gpio_free(&pdev->dev, led->cf_en);

		ret = devm_gpio_request(&pdev->dev, led->cf_txmsk,
					"cf_txmsk-gpio");
		if (ret) {
			dev_err(&pdev->dev,
				"request cf_txmsk-gpio error!\n");
			return ret;
		}
		gpio_direction_output(led->cf_txmsk, 0);
		devm_gpio_free(&pdev->dev, led->cf_txmsk);

	}

	/* TODO: hardcode the FLASH_TIMER as 250ms */
	regmap_update_bits(chip->regmap, PM830_CAMERA_FLASH2,
			   (0xf << 0), (0xa << 0));
	/* set booster high-side to 3.5A and low-side to 4.5A */
	regmap_update_bits(chip->regmap, PM830_BST_CTRL2,
			PM830_BST_HSOC_MASK, PM830_BST_HSOC_MASK);
	regmap_update_bits(chip->regmap, PM830_BST_CTRL2,
			PM830_BST_LSOC_MASK, PM830_BST_LSOC_MASK);
	/* set high-side zero current limit to 130mA */
	regmap_update_bits(chip->regmap, PM830_BST_CTRL3,
			PM830_BST_HSZC_MASK, 0);

	mutex_init(&led->lock);
	INIT_WORK(&led->work, pm830_led_work);
	INIT_DELAYED_WORK(&led->delayed_work, pm830_led_delayed_work);


	ret = led_classdev_register(&pdev->dev, &led->cdev);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to register LED: %d\n", ret);
		return ret;
	}
	pm830_led_bright_set(&led->cdev, 0);

	ret = device_create_file(led->cdev.dev, &dev_attr_gpio_ctrl);
	if (ret < 0) {
		dev_err(&pdev->dev, "device attr create fail: %d\n", ret);
		return ret;
	}

	return 0;
out:
	return ret;
}

static int pm830_led_remove(struct platform_device *pdev)
{
	struct pm830_led *led = platform_get_drvdata(pdev);

	if (led)
		led_classdev_unregister(&led->cdev);
	return 0;
}

static const struct of_device_id pm830_led_dt_match[] = {
	{ .compatible = "marvell,88pm830-led", },
	{ },
};
MODULE_DEVICE_TABLE(of, pm830_chg_dt_match);

static struct platform_driver pm830_led_driver = {
	.driver	= {
		.owner	= THIS_MODULE,
		.name	= "88pm830-led",
		.of_match_table = of_match_ptr(pm830_led_dt_match),
	},
	.probe	= pm830_led_probe,
	.remove	= pm830_led_remove,
};

module_platform_driver(pm830_led_driver);

MODULE_DESCRIPTION("Camera flash/torch driver for Marvell 88PM830");
MODULE_AUTHOR("Haojian Zhuang <haojian.zhuang@marvell.com>");
MODULE_AUTHOR("Keith Prickett <keithp@marvell.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:88PM830-led");
