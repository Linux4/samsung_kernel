/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <mach/hardware.h>
#include <mach/board.h>
#include <mach/adi.h>
#include <asm/gpio.h>

#define SPRD_FLASH_ON		1
#define SPRD_FLASH_OFF		0

int sprd_flash_on(void)
{
	printk("sprd_flash_on \n");
	gpio_request(GPIO_CAM_FLASH_EN,"cam_flash_en");
	gpio_direction_output(GPIO_CAM_FLASH_EN, SPRD_FLASH_ON);
	gpio_free(GPIO_CAM_FLASH_EN);

	gpio_request(GPIO_CAM_FLASH_MODE,"cam_flash_mode");
	gpio_direction_output(GPIO_CAM_FLASH_MODE, SPRD_FLASH_OFF);
	gpio_free(GPIO_CAM_FLASH_MODE);
	return 0;
}

int sprd_flash_high_light(void)
{
	printk("sprd_flash_high_light \n");
	gpio_request(GPIO_CAM_FLASH_EN,"cam_flash_en");
	gpio_direction_output(GPIO_CAM_FLASH_EN, SPRD_FLASH_ON);
	gpio_free(GPIO_CAM_FLASH_EN);

	gpio_request(GPIO_CAM_FLASH_MODE,"cam_flash_mode");
	gpio_direction_output(GPIO_CAM_FLASH_MODE, SPRD_FLASH_ON);
	gpio_free(GPIO_CAM_FLASH_MODE);
	return 0;
}

int sprd_flash_close(void)
{
	printk("sprd_flash_close \n");
	gpio_request(GPIO_CAM_FLASH_EN,"cam_flash_en");
	gpio_direction_output(GPIO_CAM_FLASH_EN, SPRD_FLASH_OFF);
	gpio_free(GPIO_CAM_FLASH_EN);

	gpio_request(GPIO_CAM_FLASH_MODE,"cam_flash_mode");
	gpio_direction_output(GPIO_CAM_FLASH_MODE, SPRD_FLASH_OFF);
	gpio_free(GPIO_CAM_FLASH_MODE);
	return 0;
}
