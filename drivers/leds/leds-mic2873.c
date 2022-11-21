/*
 * LED driver for Maxim MIC2873 - leds-mic2873.c
 *
 * Copyright (C) 2013 Samsung co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/slab.h>
#include <linux/leds-mic2873.h>
#include <linux/ctype.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/sysfs.h>
#include <linux/stat.h>

#include <linux/of.h>
#include <linux/of_platform.h>

#if defined(CONFIG_DEBUG_FLASH)
#define DEBUG_MIC2873(fmt, args...) pr_err(fmt, ##args)
#else
#define DEBUG_MIC2873(fmt, args...) do{}while(0)
#endif

static DEFINE_SPINLOCK(flash_ctrl_lock);

extern struct class *camera_class; /*sys/class/camera*/
extern struct device *flash_dev;
static int flash_torch_en;
static bool sysfs_flash_op;

extern int led_torch_en;
extern int led_flash_en;

static int mic2873_flash_ctrl (int isOn, int cur)
{
	int ret = 0;
	int16_t cnt = 0;
	unsigned long flags;

	spin_lock_irqsave(&flash_ctrl_lock, flags);

	/* GPIO Config */
	ret = gpio_request(led_flash_en, "mic2873_flash_en"); /* DC PIN - Config */
	if (ret) {
		pr_err("can't get mic2873_flash_en");
		spin_unlock_irqrestore(&flash_ctrl_lock, flags);
		return -1;
	}

	ret = gpio_request(led_torch_en, "mic2873_torch_en");/* FEN PIN - Config */
	if (ret) {
		pr_err("can't get mic2873_torch_en");
		gpio_free(led_flash_en);
		spin_unlock_irqrestore(&flash_ctrl_lock, flags);
		return -1;
	}

	/* Init State */
	gpio_direction_output(led_flash_en, 0);		/* DC PIN - OFF */
	gpio_direction_output(led_torch_en, 0);	/* FEN PIN - OFF */
	udelay(450);

	if (isOn == MIC2873_OP_SWITCH_OFF) {
		pr_info("[LED] %s: MIC2873 FLASH OFF \n", __func__);
	}else {
		pr_info("[LED] %s: MIC2873 FLASH ON \n", __func__);

		/* Start State */
		gpio_direction_output(led_flash_en, 1);		/* DC PIN - OFF */

		/* Set Low Voltage Threshold Reg */
                /*	Address Setting */
                for(cnt = 0; cnt < MIC2873_REG_ADDR_LOW_BATTERY_VOLTAGE_DETECTION_THRESHOLD; cnt++) {
                    gpio_direction_output(led_flash_en, 0);		/* DC PIN - OFF */
                    udelay(1);

                    gpio_direction_output(led_flash_en, 1);		/* DC PIN - ON */
                    udelay(1);
                }
                udelay(110);

                /*	Data Setting */
                for(cnt = 0; cnt < MIC2873_LOW_VOLTAGE_THRESHOLD_DISABLE; cnt++) {
                    gpio_direction_output(led_flash_en, 0);		/* DC PIN - OFF */
                    udelay(1);

                    gpio_direction_output(led_flash_en, 1);		/* DC PIN - ON */
                    udelay(1);
                }
                udelay(410);

		/* Set FEN / Flash Current Reg*/
                /*	Address Setting */
                for(cnt = 0; cnt < MIC2873_REG_ADDR_FLASH_CURRENT; cnt++) {
                    gpio_direction_output(led_flash_en, 0);		/* DC PIN - OFF */
                    udelay(1);

                    gpio_direction_output(led_flash_en, 1);		/* DC PIN - ON */
                    udelay(1);
                }
                udelay(110);

                /*	Data Setting */
                /*for(cnt = 0; cnt < MIC2873_FLASH_CURRENT_FEN0_1P200 ; cnt++) {*/ /* default Setting Value */
                for(cnt = 0; cnt < cur; cnt++) {
                    gpio_direction_output(led_flash_en, 0);		/* DC PIN - OFF */
                    udelay(1);

                    gpio_direction_output(led_flash_en, 1);		/* DC PIN - ON */
                    udelay(1);
                }
                udelay(410);

		/* flash enable */
		gpio_direction_output(led_torch_en, 1);	/* FEN PIN - ON */
	}
	spin_unlock_irqrestore(&flash_ctrl_lock, flags);
	gpio_free(led_flash_en);
	gpio_free(led_torch_en);

	return 0;
}

static int mic2873_torch_ctrl(int isOn, int cur)
{
	int ret = 0;
	int16_t cnt = 0;
	unsigned long flags;
	spin_lock_irqsave(&flash_ctrl_lock, flags);

	/* GPIO Config */
	ret = gpio_request(led_flash_en, "mic2873_flash_en"); /* DC PIN - Config */
	if (ret) {
		pr_err("can't get mic2873_flash_en");
		spin_unlock_irqrestore(&flash_ctrl_lock, flags);
		return -1;
	}

	ret = gpio_request(led_torch_en, "mic2873_torch_en");/* FEN PIN - Config */
	if (ret) {
		pr_err("can't get mic2873_torch_en");
		gpio_free(led_flash_en);
		spin_unlock_irqrestore(&flash_ctrl_lock, flags);
		return -1;
	}

	if (isOn == MIC2873_OP_SWITCH_OFF) {
		pr_info("[LED] %s: MIC2873 TORCH OFF \n", __func__);
		gpio_direction_output(led_flash_en, 0);		/* DC PIN - OFF */
	}else {
 		pr_info("[LED] %s: MIC2873 TORCH ON : %d\n", __func__, cur);

		/* Start State */
		gpio_direction_output(led_flash_en, 1);		/* DC PIN - OFF */

		/* Set Low Voltage Threshold Reg*/
                /*	Address Setting */
                for(cnt = 0; cnt < MIC2873_REG_ADDR_LOW_BATTERY_VOLTAGE_DETECTION_THRESHOLD; cnt++) {
                    gpio_direction_output(led_flash_en, 0);		/* DC PIN - OFF */
                    udelay(1);

                    gpio_direction_output(led_flash_en, 1);		/* DC PIN - ON */
                    udelay(1);
                }
                udelay(110);

                /*	Data Setting */
                for(cnt = 0; cnt < MIC2873_LOW_VOLTAGE_THRESHOLD_DISABLE; cnt++) {
                    gpio_direction_output(led_flash_en, 0);		/* DC PIN - OFF */
                    udelay(1);

                    gpio_direction_output(led_flash_en, 1);		/* DC PIN - ON */
                    udelay(1);
                }
                udelay(410);

		/* Set Safety Timer Current Threshold Reg*/
                /*	Address Setting */
                for(cnt = 0; cnt < MIC2873_REG_ADDR_SAFETY_TIMER_CURRENT_DETECTION_THRESHOLD; cnt++) {
                    gpio_direction_output(led_flash_en, 0);		/* DC PIN - OFF */
                    udelay(1);

                    gpio_direction_output(led_flash_en, 1);		/* DC PIN - ON */
                    udelay(1);
                }
                udelay(110);

                /*	Data Setting */
                for(cnt = 0; cnt < MIC2873_SAFETY_TIMER_CURRENT_THRESHOLD_300MA; cnt++) {
                    gpio_direction_output(led_flash_en, 0);		/* DC PIN - OFF */
                    udelay(1);

                    gpio_direction_output(led_flash_en, 1);		/* DC PIN - ON */
                    udelay(1);
                }
                udelay(410);

		/* Set TEN / Torch Current Reg*/
                /*	Address Setting */
                for(cnt = 0; cnt < MIC2873_REG_ADDR_TORCH_CURRENT; cnt++) {
                    gpio_direction_output(led_flash_en, 0);		/* DC PIN - OFF */
                    udelay(1);

                    gpio_direction_output(led_flash_en, 1);		/* DC PIN - ON */
                    udelay(1);
                }
                udelay(110);

                /*	Data Setting */
                /*for(cnt = 0; cnt < MIC2873_TORCH_CURRENT_TEN1_175P0; cnt++) {*/ /* default Setting Value */
                for(cnt = 0; cnt < cur; cnt++) {
                    gpio_direction_output(led_flash_en, 0);		/* DC PIN - OFF */
                    udelay(1);

                    gpio_direction_output(led_flash_en, 1);		/* DC PIN - ON */
                    udelay(1);
                }
                udelay(410);
	}
	spin_unlock_irqrestore(&flash_ctrl_lock, flags);
	gpio_free(led_flash_en);
	gpio_free(led_torch_en);

	return 0;
}

int mic2873_led_en(int onoff, int mode)
{
	int ret = 0;

        if (sysfs_flash_op) {
            pr_warn("%s : The camera led control is not allowed"
                    " because sysfs led control already used it\n", __FUNCTION__);
            return 0; //no error
        }

	if (flash_torch_en) {
	    if (onoff) { 	/* enable */
		if (mode) { 	/* flash */
			DEBUG_MIC2873("[LED] %s: mic2873_flash_en set 1\n", __func__);
			mic2873_flash_ctrl(MIC2873_OP_SWITCH_ON, MIC2873_FLASH_CURRENT_FEN0_1P200);
		} else { 	/* torch */
			DEBUG_MIC2873("[LED] %s: mic2873_torch_en set 1\n", __func__);
			mic2873_torch_ctrl(MIC2873_OP_SWITCH_ON, MIC2873_TORCH_CURRENT_TEN1_175P0);
		}
	    } else { 		/* disable */
		if (mode) { 	/* flash */
			DEBUG_MIC2873("[LED] %s: mic2873_flash_en set 0\n", __func__);
			mic2873_flash_ctrl(MIC2873_OP_SWITCH_OFF, MIC2873_FLASH_CURRENT_FEN0_1P200);
		} else { 	/* torch */
			DEBUG_MIC2873("[LED] %s: mic2873_torch_en set 0\n", __func__);
			mic2873_torch_ctrl(MIC2873_OP_SWITCH_OFF, MIC2873_TORCH_CURRENT_TEN1_175P0);
		}
	    }
	} else {
	    pr_err("%s : Error!!, find gpio\n", __func__);
	    ret = -EINVAL;
	}

	return ret;
}
EXPORT_SYMBOL(mic2873_led_en);

static ssize_t mic2873_flash_show(struct device *dev,
                                   struct device_attribute *attr, char *buf)
{
    char temp_print[] = "rear flash : mic2873\n";

    return snprintf(buf, sizeof(temp_print), "%s", temp_print);
}
static ssize_t mic2873_flash(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	ssize_t ret = -EINVAL;
	char *after;
	unsigned long state = simple_strtoul(buf, &after, 10);
	size_t count = after - buf;
        unsigned int cur = 0;

	DEBUG_MIC2873("[LED] %s\n", __func__);

        sysfs_flash_op = 0;

	if (isspace(*after))
		count++;

	if (count == size) {
		ret = count;

                if (flash_torch_en) {
                    if (state != 0) { 	/* enable */
                        cur = (unsigned int)(MIC2873_TORCH_CURRENT_TEN1_062P5 - state + 1);
                        DEBUG_MIC2873("[LED] %s: mic2873_torch_en set cur %d\n", __func__, cur);
                        mic2873_torch_ctrl(MIC2873_OP_SWITCH_ON, cur);
                        sysfs_flash_op = 1;
                    } else { 		/* disable */
                        DEBUG_MIC2873("[LED] %s: mic2873_torch_en set cur %d\n", __func__, cur);
                        mic2873_torch_ctrl(MIC2873_OP_SWITCH_OFF, cur);
                    }
                } else {
                    pr_err("%s : Error!!, find gpio", __func__);
                    ret = -EINVAL;
                }
	}

	return ret;
}
static DEVICE_ATTR(rear_flash, S_IWUSR|S_IWGRP|S_IROTH,
	mic2873_flash_show, mic2873_flash);
static int mic2873_led_probe(struct platform_device *pdev)
{
	int ret = 0;

	if (led_torch_en && led_flash_en) {
		DEBUG_MIC2873("[LED] %s, led_torch %d, led_flash %d\n", __func__,
			   led_torch_en, led_flash_en);
		flash_torch_en = 1;
	} else {
		pr_err("%s : can't find gpio", __func__);
		flash_torch_en = 0;
                ret = -EINVAL;
	}

        sysfs_flash_op = 0; //default off
        if (!IS_ERR(camera_class)) {
		flash_dev = device_create(camera_class, NULL, 0, NULL, "flash");
		if (flash_dev < 0) {
			pr_err("Failed to create device(flash)!\n");
                        ret = -ENOENT;
                }

		if (device_create_file(flash_dev, &dev_attr_rear_flash) < 0) {
			pr_err("failed to create device file, %s\n",
				dev_attr_rear_flash.attr.name);
                        ret = -ENOENT;
		}
	} else {
		pr_err("Failed to create device(flash) because of nothing camera class!\n");
                ret = -EINVAL;
        }
	if (!ret)
		pr_info("LED PROBED!\n");

	return ret;
}

static const struct of_device_id msm_mic2873_dt_match[] = {
	{.compatible = "mic2873-led"},
	{}
};

static struct platform_driver mic2873_led_driver = {
	.driver		= {
		.name	= "mic2873-led",
		.owner	= THIS_MODULE,
		.of_match_table = msm_mic2873_dt_match,
	},
};

static int __init mic2873_led_init(void)
{
    int rc;

    pr_info("%s : E", __FUNCTION__);

    rc = platform_driver_probe(&mic2873_led_driver,
                               mic2873_led_probe);
    if (rc)
        pr_err("%s : failed\n", __FUNCTION__);
    else
        pr_info("%s : X\n", __FUNCTION__);

    return rc;

}
module_init(mic2873_led_init);

MODULE_AUTHOR("Camera Development Group in Samsung co. Ltd.");
MODULE_DESCRIPTION("MIC2873 LED driver");
MODULE_LICENSE("GPL");
