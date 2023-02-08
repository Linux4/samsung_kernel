/* drivers/lights/rt5033_fled.c
 * RT5033 Flash LED Driver
 *
 * Copyright (C) 2013 Richtek Technology Corp.
 * Author: Patrick Chang <patrick_chang@richtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mfd/rt5033.h>
#include <linux/power_supply.h>
#include <linux/module.h>
#include <linux/rt5033_led.h>

#define ALIAS_NAME "r5033-fled"

#define GPIO_CAM_FLASH_EN       EXYNOS4_GPM3(4)
#define GPIO_CAM_TORCH_EN       EXYNOS4_GPM3(6)
#define GPIO_CAM_SENSOR_CORE    EXYNOS4_GPM1(6)

extern struct class *camera_class;
struct device *flash_dev;

static struct i2c_client *fled_i2c_client;

typedef enum flashlight_type {
	FLASHLIGHT_TYPE_XENON = 0,
	FLASHLIGHT_TYPE_LED,
	FLASHLIFHT_TYPE_BULB,
	FLASHLIGHT_TYPE_MAX,
} flashlight_type_t;

typedef enum flashlight_mode {
    FLASHLIGHT_MODE_OFF = 0,
    FLASHLIGHT_MODE_TORCH,
    FLASHLIGHT_MODE_FLASH,
    /* MIXED mode means TORCH + FLASH */
    FLASHLIGHT_MODE_MIXED,
    FLASHLIGHT_MODE_MAX,
} flashlight_mode_t;

typedef struct rt5033_fled_info {
  //  rt_fled_info_t base;
    const rt5033_fled_platform_data_t *pdata;
    rt5033_mfd_chip_t *chip;
    struct mutex io_lock;
    struct i2c_client *i2c_client;
    int torch_current;
    int strobe_current;
    struct power_supply *psy_chg;
} rt5033_fled_info_t;

struct flashlight_properties {
    /* Flashlight type */
	enum flashlight_type type;
    /* Xenon type flashlight doesn't support torch mode */
    enum flashlight_mode mode;
    /* Color temperature, unit: K, 0 means unknow */
    int color_temperature;
    int torch_brightness;
//    int torch_max_brightness;
    int strobe_brightness;
    int strobe_max_brightness;
    int strobe_delay;
    int strobe_timeout;
    const char *alias_name;
};

#if 0
static struct flashlight_properties rt5033_fled_props =
{
	.type = FLASHLIGHT_TYPE_LED,
    .torch_brightness = 0,
 //   .torch_max_brightness = ARRAY_SIZE(torch_current) - 1,
    .strobe_brightness = 0,
    .strobe_max_brightness = 31 - 1,
    .strobe_delay = 2,
    .strobe_timeout = 64,
    .alias_name = "rt5033-fled",
};
#endif

const static rt5033_fled_platform_data_t rt5033_default_fled_pdata = {
	.fled1_en = 1,
	.fled2_en = 1,
	.fled_mid_track_alive = 0,
	.fled_mid_auto_track_en = 1,
	.fled_timeout_current_level = RT5033_TIMEOUT_LEVEL(50),
	.fled_strobe_current = RT5033_STROBE_CURRENT(750),
	.fled_strobe_timeout = RT5033_STROBE_TIMEOUT(544),
	.fled_torch_current = RT5033_TORCH_CURRENT(38),
	.fled_lv_protection = RT5033_LV_PROTECTION(3100),
	.fled_mid_level = RT5033_MID_REGULATION_LEVEL(5000),
};

int rt5033_fled_mode_ctrl(int state)
{
	flashlight_mode_t mode = state;
	switch(mode)
	{
		case FLASHLIGHT_MODE_OFF:
			pr_info("%s, FLASHLIGHT_MODE_OFF%d\n\n",__func__,mode);
			rt5033_reg_write(fled_i2c_client, RT5033_FLED_FUNCTION2, 0x0);
			break;
		case FLASHLIGHT_MODE_TORCH:
			pr_info("%s, FLASHLIGHT_MODE_TORCH%d\n\n",__func__,mode);
			rt5033_clr_bits(fled_i2c_client, RT5033_FLED_FUNCTION1, 0x04);
			rt5033_reg_write(fled_i2c_client, RT5033_FLED_FUNCTION2, 0x01);
			break;
		case FLASHLIGHT_MODE_FLASH:
			pr_info("%s, FLASHLIGHT_MODE_FLASH%d\n\n",__func__,mode);
			rt5033_reg_write(fled_i2c_client, RT5033_FLED_FUNCTION2, 0x0);
			rt5033_set_bits(fled_i2c_client, RT5033_FLED_FUNCTION1, 0x04);
			break;
		default:
			return -EINVAL;
	}
	return 0;
}
EXPORT_SYMBOL(rt5033_fled_mode_ctrl);

#if 0
static int __init camera_class_init(void)
{
        camera_class = class_create(THIS_MODULE, "camera");
        if (IS_ERR(camera_class)) {
                pr_err("Failed to create class(camera)!\n");
                return PTR_ERR(camera_class);
        }

        return 0;
}

subsys_initcall(camera_class_init);
#endif

static ssize_t flash_store(struct device *dev, struct device_attribute *attr,
                         const char *buf, size_t count)
{
	if(*buf == '0') {
		pr_err("flash off\n");
		rt5033_reg_write(fled_i2c_client, RT5033_FLED_FUNCTION2, 0x0);
	//        gpio_direction_output(GPIO_CAM_TORCH_EN, 0);
	//        gpio_direction_output(GPIO_CAM_FLASH_EN, 0);
        } else {
                pr_err("flash torch on\n");
		rt5033_clr_bits(fled_i2c_client, RT5033_FLED_FUNCTION1, 0x04);
		rt5033_reg_write(fled_i2c_client, RT5033_FLED_FUNCTION2, 0x01);
	//        gpio_direction_output(GPIO_CAM_FLASH_EN, 1);
        }
#if 0
	else {
                pr_err("flash on\n");
		rt5033_set_bits(fled_i2c_client, RT5033_FLED_FUNCTION1, 0x04);
	//        gpio_direction_output(GPIO_CAM_TORCH_EN, 1);
	//        gpio_direction_output(GPIO_CAM_FLASH_EN, 1);
	}
#endif
	return count;
}

static DEVICE_ATTR(rear_flash,  0644, NULL, flash_store);

int create_flash_sysfs(void)
{
        int err = -ENODEV;

        pr_err("flash_sysfs: sysfs test!!!! (%s)\n",__func__);

	if (IS_ERR_OR_NULL(camera_class)) {
		pr_err("flash_sysfs: error, camera class not exist");
		return -ENODEV;
	}

        flash_dev = device_create(camera_class, NULL, 0, NULL, "flash");
        if (IS_ERR(flash_dev)) {
                pr_err("flash_sysfs: failed to create device(flash)\n");
                return -ENODEV;
        }

        err = device_create_file(flash_dev, &dev_attr_rear_flash);
        if (unlikely(err < 0))
                pr_err("flash_sysfs: failed to create device file, %s\n",
                        dev_attr_rear_flash.attr.name);

        return 0;
}

static int rt5033_fled_init(struct rt5033_fled_info *fled_info)
{
	rt5033_fled_info_t *info = (rt5033_fled_info_t *)fled_info;
	rt5033_mfd_platform_data_t *mfd_pdata;
	BUG_ON(info == NULL);
	mfd_pdata = info->chip->pdata;
	mutex_lock(&info->io_lock);

	printk("Richtek RT5033 FlashLED init!!!!!!!!!!...\n");

	rt5033_set_bits(info->i2c_client,RT5033_FLED_RESET,0x01);

	if (!info->pdata->fled1_en)
		rt5033_clr_bits(info->i2c_client, RT5033_FLED_FUNCTION1, 0x01);
	if (!info->pdata->fled2_en)
		rt5033_clr_bits(info->i2c_client, RT5033_FLED_FUNCTION1, 0x02);
	if (info->pdata->fled_mid_track_alive)
		rt5033_set_bits(info->i2c_client, RT5033_FLED_CONTROL2, (1 << 6));
	if (info->pdata->fled_mid_auto_track_en)
		rt5033_set_bits(info->i2c_client, RT5033_FLED_CONTROL2, (1 << 7));
	rt5033_reg_write(info->i2c_client, RT5033_FLED_STROBE_CONTROL1,
			(info->pdata->fled_timeout_current_level << 5) |
			info->pdata->fled_strobe_current);

//	info->base.flashlight_dev->props.strobe_brightness =
//		info->pdata->fled_strobe_current;

	rt5033_reg_write(info->i2c_client, RT5033_FLED_STROBE_CONTROL2,
		info->pdata->fled_strobe_timeout);
  //	info->base.flashlight_dev->props.strobe_timeout =
//		info->base.hal->fled_strobe_timeout_list(fled_info,
//		info->pdata->fled_strobe_timeout);

//	RTINFO("Strobe timeout = %d ms\n",
//           info->base.flashlight_dev->props.strobe_timeout);
	rt5033_reg_write(info->i2c_client, RT5033_FLED_CONTROL1,
		(info->pdata->fled_torch_current << 4) |
		info->pdata->fled_lv_protection);
//	info->base.flashlight_dev->props.torch_brightness =
//		info->pdata->fled_torch_current;
	rt5033_assign_bits(info->i2c_client, RT5033_FLED_CONTROL2,
		0x3f, info->pdata->fled_mid_level);
//	rt5033_reg_write(info->i2c_client, RT5033_CHG_FLED_CTRL,
//		mfd_pdata->rt5033_mfd_irq_ctrl->led_irq_ctrl[0]);

	mutex_unlock(&info->io_lock);
	return 0;
}

static __devinit int rt5033_fled_probe(struct platform_device *pdev)
{
	int ret=0;
	struct rt5033_mfd_chip *chip = dev_get_drvdata(pdev->dev.parent);
	struct rt5033_mfd_platform_data *mfd_pdata = chip->dev->platform_data;
	const struct rt5033_fled_platform_data* pdata;
	rt5033_fled_info_t *fled_info;

	printk("Richtek RT5033 FlashLED driver probing...\n");

	BUG_ON(mfd_pdata == NULL);
	if (mfd_pdata->fled_platform_data)
		pdata = mfd_pdata->fled_platform_data;
	else
		pdata = &rt5033_default_fled_pdata;

	fled_info = kzalloc(sizeof(*fled_info), GFP_KERNEL);
	if (!fled_info) {
		ret = -ENOMEM;
		goto err_fled_nomem;
	}

	mutex_init(&fled_info->io_lock);
	fled_info->i2c_client = chip->i2c_client;
	fled_i2c_client = chip->i2c_client;
	fled_info->pdata = pdata;
	fled_info->chip = chip;
	chip->fled_info = fled_info;
	platform_set_drvdata(pdev, fled_info);


	rt5033_fled_init(fled_info);

	create_flash_sysfs();
//        gpio_direction_output(GPIO_CAM_SENSOR_CORE, 1);

	if (ret<0)
		goto err_register_pdev;
	return 0;

err_register_pdev:
	kfree(fled_info);
err_fled_nomem:
	return ret;
}

static __devexit int rt5033_fled_remove(struct platform_device *pdev)
{
	struct rt5033_fled_info *fled_info;
	printk("Richtek RT5033 FlashLED driver removing...\n");
	fled_info = platform_get_drvdata(pdev);
	mutex_destroy(&fled_info->io_lock);
	kfree(fled_info);

	device_remove_file(flash_dev, &dev_attr_rear_flash);
	device_destroy(camera_class, 0);
	class_destroy(camera_class);

	return 0;
}

static struct platform_driver rt5033_fled_driver = {
	.probe	= rt5033_fled_probe,
	.remove	= __devexit_p(rt5033_fled_remove),
	.driver	= {
		.name	= "rt5033-led",
	},
};

static int __init rt5033_fled_module_init(void)
{
#if 0
	 if (gpio_request(GPIO_CAM_TORCH_EN, "GPM3_6") < 0) {
                pr_err("failed gpio_request(GPX1_2) for camera control\n");
                return -1;
        }
	 if (gpio_request(GPIO_CAM_FLASH_EN, "GPM3_6") < 0) {
                pr_err("failed gpio_request(GPX1_2) for camera control\n");
                return -1;
        }
#endif
#if 0
	 if (gpio_request(GPIO_CAM_SENSOR_CORE, "GPM1_6") < 0) {
                pr_err("failed gpio_request(GPX1_2) for camera control\n");
                return -1;
        }
#endif
	return platform_driver_register(&rt5033_fled_driver);
}
module_init(rt5033_fled_module_init);

static void __exit rt5033_fled_module_exit(void)
{
#if 0
	gpio_free(GPIO_CAM_TORCH_EN);
	gpio_free(GPIO_CAM_FLASH_EN);
#endif
//	gpio_free(GPIO_CAM_SENSOR_CORE);
	device_remove_file(flash_dev, &dev_attr_rear_flash);
	device_destroy(camera_class, 0);
	class_destroy(camera_class);
	platform_driver_unregister(&rt5033_fled_driver);
}
module_exit(rt5033_fled_module_exit);

MODULE_DESCRIPTION("Richtek RT5033 FlashLED Driver");
MODULE_AUTHOR("Patrick Chang <patrick_chang@richtek.com>");
MODULE_LICENSE("GPL");

