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
#include <linux/leds/flashlight.h>

struct flashlight_device* sprd_get_flashlight_by_name(void)
{
	struct flashlight_device* flash_ptr = NULL;

	flash_ptr = find_flashlight_by_name("rt-flash-led");
	if (NULL == flash_ptr) {
		printk("sprd_get_flashlight_by_name: flash_ptr is PNULL  \n");
		return NULL;
	}
	flashlight_set_mode(flash_ptr, FLASHLIGHT_MODE_FLASH);
	return flash_ptr;
}

int sprd_flash_on(void)
{
	struct flashlight_device* flash_ptr = NULL;

	flash_ptr = sprd_get_flashlight_by_name();
	printk("sprd_flash_on kanas w \n");
	flashlight_strobe(flash_ptr);
	return 0;
}

int sprd_flash_high_light(void)
{
	struct flashlight_device* flash_ptr = NULL;

	flash_ptr = sprd_get_flashlight_by_name();
	printk("sprd_flash_high_light kanas w \n");
	flashlight_strobe(flash_ptr);
	return 0;
}

int sprd_flash_close(void)
{
	struct flashlight_device* flash_ptr = NULL;

	flash_ptr = sprd_get_flashlight_by_name();
	printk("sprd_flash_close kanas w \n");
	flashlight_set_mode(flash_ptr, FLASHLIGHT_MODE_OFF);
	return 0;
}
