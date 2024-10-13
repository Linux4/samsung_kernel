/*
 *Copyright (C) 2021 Motorola Mobility LLC,
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/file.h>
#include <linux/slab.h>
#include <linux/pwm.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <media/rc-core.h>
#include <linux/platform_device.h>
#include "pm6125_flash_gpio.h"

#include "cam_flash_dev.h"
#include "cam_flash_soc.h"
#include "cam_flash_core.h"
#include "cam_common_util.h"
#include "cam_res_mgr_api.h"

#define TORCH_CLASS_NAME "camera"
#define TORCH_DEV_NAME "flash"

/*****************************************************************************
 * Data Structure
 *****************************************************************************/
struct pwm_device *pwm;
static dev_t torch_devno;
static struct class *torch_class;
static struct device *torch_device;
static unsigned int flashlight_enable = CAMERA_SENSOR_FLASH_STATUS_OFF;
static unsigned int torch_mode = 0;

/*****************************************************************************
 * Function
 *****************************************************************************/
void pm6125_flash_gpio_select_state(PM6125_FLASH_GPIO_STATE s, enum camera_flash_opcode opcode, u64 flash_current)
{
	struct pwm_state pstate;
	int rc = 0;
	u64 real_current = 0;

	pwm_get_state(pwm, &pstate);
	pstate.period = PM6125_PWM_PERIOD; // 50000
	switch (s)
	{
	case PM6125_FLASH_GPIO_STATE_ACTIVE:
		switch (opcode)
		{
		case CAMERA_SENSOR_FLASH_OP_FIRELOW:
			real_current = (flash_current > FLASH_FIRE_LOW_MAXCURRENT ? FLASH_FIRE_LOW_MAXCURRENT : flash_current);
			pstate.duty_cycle = PM6125_PWM_PERIOD * real_current / FLASH_FIRE_LOW_MAXCURRENT;
			break;
		case CAMERA_SENSOR_FLASH_OP_FIREHIGH:
			real_current = (flash_current > FLASH_FIRE_HIGH_MAXCURRENT ? FLASH_FIRE_HIGH_MAXCURRENT : flash_current);
			pstate.duty_cycle = PM6125_PWM_PERIOD * real_current / FLASH_FIRE_HIGH_MAXCURRENT;
			break;
		default:
			pstate.duty_cycle = PM6125_PWM_PERIOD;
			break;
		}

		pstate.enabled = true;
		rc = pwm_apply_state(pwm, &pstate);

		CAM_INFO(CAM_FLASH, "active duty_cycle = %u, period = %u, opcode = %u, real_current = %u",
				pstate.duty_cycle, pstate.period, opcode, real_current);
		break;
	case PM6125_FLASH_GPIO_STATE_SUSPEND:
		pstate.enabled = false;
		rc = pwm_apply_state(pwm, &pstate);
		CAM_INFO(CAM_FLASH, "suspend torch_mode = %d", torch_mode);
		torch_mode = 0;
		break;
	default:
		CAM_ERR(CAM_FLASH, "failed to control PWM use a err state!");
	}

	if (rc < 0)
	{
		CAM_ERR(CAM_FLASH, "apply PWM state fail, rc = %d", rc);
	}
}

static ssize_t pm6125_flash_enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	CAM_INFO(CAM_FLASH, "success, flashlight_enable=%d", flashlight_enable);
	return sprintf(buf, "%u\n", flashlight_enable);
}

static ssize_t pm6125_flash_enable_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned long state_to_set;
	int ret = 0;

	CAM_INFO(CAM_FLASH, "flash enable command: %s", buf);
	ret = kstrtoul(buf, 10, &state_to_set);
	if (ret)
	{
		CAM_ERR(CAM_FLASH, "kstrtoul fail, ret = %d", ret);
		return ret;
	}

	CAM_INFO(CAM_FLASH, "state_to_set = %lu, flashlight_enable = %d, torch_mode = %d", state_to_set, flashlight_enable, torch_mode);

	if (CAMERA_SENSOR_FLASH_STATUS_OFF != state_to_set)
	{
		// enable flash
		flashlight_enable = state_to_set;
		if(torch_mode == 0)
		{
		    CAM_INFO(CAM_FLASH, "state_to_set torch_mode = %d", torch_mode);
		    cam_res_mgr_gpio_request(dev, 396, 0, "torch_enable");
		    cam_res_mgr_gpio_set_value(396, 1);
		    usleep_range(5000, 6000);
		    cam_res_mgr_gpio_set_value(396, 0);
		    cam_res_mgr_gpio_free(dev, 396);
		}

		switch (flashlight_enable)
		{
		case 1001:
			//pm6125_flash_gpio_select_state(PM6125_FLASH_GPIO_STATE_ACTIVE, CAMERA_SENSOR_FLASH_OP_FIRELOW, FLASH_FIRE_LOW_MAXCURRENT);
			torch_mode = 1;
			pm6125_flash_gpio_select_state(
				PM6125_FLASH_GPIO_STATE_ACTIVE,
				flashlight_enable == CAMERA_SENSOR_FLASH_STATUS_HIGH ? CAMERA_SENSOR_FLASH_OP_FIREHIGH : CAMERA_SENSOR_FLASH_OP_FIRELOW,
				FLASH_FIRE_BRIGHTNESS_LEVEL_1);
			break;
		case 1002:
			//pm6125_flash_gpio_select_state(PM6125_FLASH_GPIO_STATE_ACTIVE, CAMERA_SENSOR_FLASH_OP_FIRELOW, FLASH_FIRE_LOW_MAXCURRENT);
			torch_mode = 1;
			pm6125_flash_gpio_select_state(
				PM6125_FLASH_GPIO_STATE_ACTIVE,
				flashlight_enable == CAMERA_SENSOR_FLASH_STATUS_HIGH ? CAMERA_SENSOR_FLASH_OP_FIREHIGH : CAMERA_SENSOR_FLASH_OP_FIRELOW,
				FLASH_FIRE_BRIGHTNESS_LEVEL_2);
			break;
		case 1003:
			//pm6125_flash_gpio_select_state(PM6125_FLASH_GPIO_STATE_ACTIVE, CAMERA_SENSOR_FLASH_OP_FIRELOW, FLASH_FIRE_LOW_MAXCURRENT);
			torch_mode = 1;
			pm6125_flash_gpio_select_state(
				PM6125_FLASH_GPIO_STATE_ACTIVE,
				flashlight_enable == CAMERA_SENSOR_FLASH_STATUS_HIGH ? CAMERA_SENSOR_FLASH_OP_FIREHIGH : CAMERA_SENSOR_FLASH_OP_FIRELOW,
				FLASH_FIRE_BRIGHTNESS_LEVEL_3);
			break;
		case 1005:
			//pm6125_flash_gpio_select_state(PM6125_FLASH_GPIO_STATE_ACTIVE, CAMERA_SENSOR_FLASH_OP_FIRELOW, FLASH_FIRE_LOW_MAXCURRENT);
			torch_mode = 1;
			pm6125_flash_gpio_select_state(
				PM6125_FLASH_GPIO_STATE_ACTIVE,
				flashlight_enable == CAMERA_SENSOR_FLASH_STATUS_HIGH ? CAMERA_SENSOR_FLASH_OP_FIREHIGH : CAMERA_SENSOR_FLASH_OP_FIRELOW,
				FLASH_FIRE_BRIGHTNESS_LEVEL_5);
			break;
		case 1007:
			//pm6125_flash_gpio_select_state(PM6125_FLASH_GPIO_STATE_ACTIVE, CAMERA_SENSOR_FLASH_OP_FIRELOW, FLASH_FIRE_LOW_MAXCURRENT);
			torch_mode = 1;
			pm6125_flash_gpio_select_state(
				PM6125_FLASH_GPIO_STATE_ACTIVE,
				flashlight_enable == CAMERA_SENSOR_FLASH_STATUS_HIGH ? CAMERA_SENSOR_FLASH_OP_FIREHIGH : CAMERA_SENSOR_FLASH_OP_FIRELOW,
				FLASH_FIRE_BRIGHTNESS_LEVEL_7);
			break;
		default:
			//pm6125_flash_gpio_select_state(PM6125_FLASH_GPIO_STATE_ACTIVE, CAMERA_SENSOR_FLASH_OP_FIRELOW, FLASH_FIRE_LOW_MAXCURRENT);
			torch_mode = 1;
			pm6125_flash_gpio_select_state(
				PM6125_FLASH_GPIO_STATE_ACTIVE,
				flashlight_enable == CAMERA_SENSOR_FLASH_STATUS_HIGH ? CAMERA_SENSOR_FLASH_OP_FIREHIGH : CAMERA_SENSOR_FLASH_OP_FIRELOW,
				FLASH_FIRE_BRIGHTNESS_LEVEL_3);
			break;
		}
	}
	else
	{
		// disable flash
		torch_mode = 0;
		CAM_INFO(CAM_FLASH, "state_to_set = %lu, flashlight_enable = %d, torch_mode = %d", state_to_set, flashlight_enable, torch_mode);
		flashlight_enable = CAMERA_SENSOR_FLASH_STATUS_OFF;
		pm6125_flash_gpio_select_state(PM6125_FLASH_GPIO_STATE_SUSPEND, CAMERA_SENSOR_FLASH_OP_OFF, 0);
	}

	return size;
}

static DEVICE_ATTR(rear_flash, S_IRUGO | S_IWUSR, pm6125_flash_enable_show, pm6125_flash_enable_store);

void pm6125_flash_control_create_device(void)
{
	int ret = 0;

	ret = alloc_chrdev_region(&torch_devno, 0, 1, TORCH_DEV_NAME);
	if (ret)
	{
		CAM_ERR(CAM_FLASH, "alloc_chrdev_region fail  ret = %d", ret);
	}
	else
	{
		CAM_INFO(CAM_FLASH, "tourch init major: %d, minor: %d, ret = %d", MAJOR(torch_devno), MINOR(torch_devno), ret);
	}

	torch_class = class_create(THIS_MODULE, TORCH_CLASS_NAME);
	if (IS_ERR(torch_class))
	{
		CAM_ERR(CAM_FLASH, "class_create fail");
	}

	// sys/class/camera/flash/
	torch_device = device_create(torch_class, NULL, torch_devno,
								 NULL, TORCH_DEV_NAME);
	if (NULL == torch_device)
	{
		CAM_ERR(CAM_FLASH, "device_create fail");
	}

	// sys/class/camera/flash/rear_flash
	ret = device_create_file(torch_device, &dev_attr_rear_flash);
	if (ret < 0)
	{
		CAM_ERR(CAM_FLASH, "device_create_file fail");
	}
	CAM_INFO(CAM_FLASH, "pm6125_flash_control_create_device end");
}

static int pm6125_flash_pwm_probe(struct platform_device *pdev)
{
	int rc = 0;

	pwm = devm_pwm_get(&pdev->dev, NULL);
	if (IS_ERR(pwm))
	{
		CAM_ERR(CAM_FLASH, "pm6125_flash_pwm_probe failed");
		return PTR_ERR(pwm);
	}
	CAM_INFO(CAM_FLASH, "pm6125_flash_pwm_probe success");
	pm6125_flash_control_create_device();

	return rc;
}

static const struct of_device_id gpio_of_match[] = {
	{
		.compatible = "qualcomm,pm6125_flash_gpio",
	},
	{},
};

static struct platform_driver pm6125_flash_gpio_platform_driver = {
	.probe = pm6125_flash_pwm_probe,
	.driver = {
		.name = "PM6125_FLASH_GPIO_DTS",
		.of_match_table = gpio_of_match,
	},
};

int pm6125_flash_gpio_init_module(void)
{
	if (platform_driver_register(&pm6125_flash_gpio_platform_driver))
	{
		CAM_ERR(CAM_FLASH, "Failed to register pm6125_flash_gpio_platform_driver!");
		return -1;
	}
	return 0;
}

void pm6125_flash_gpio_exit_module(void)
{
	platform_driver_unregister(&pm6125_flash_gpio_platform_driver);
}

MODULE_DESCRIPTION("CONTROL PM6125 FLASH GPIO Driver");
MODULE_LICENSE("GPL");
