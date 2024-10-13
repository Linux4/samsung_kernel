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

#ifndef CONFIG_64BIT
#include <soc/sprd/hardware.h>
#include <soc/sprd/board.h>
#include <soc/sprd/adi.h>
#endif

#include "../common/parse_hwinfo.h"
#include "../../sprd_sensor/sensor_drv_sprd.h"

#include <asm/gpio.h>

#define SPRD_FLASH_ON		1
#define SPRD_FLASH_OFF		0
#define PNULL                             ((void *)0)


int get_gpio_flash_en_id(void)
{
    int gpio_id = 0;
    struct device_node *dn;
    dn = get_device_node();
    if (dn != PNULL) {
        get_gpio_id_ex(dn, GPIO_FLASH_EN, &gpio_id, 0);
    }

    return gpio_id;
}

int get_gpio_switch_id(void)
{
     int gpio_id = 0;
    struct device_node *dn;
    dn = get_device_node();
    if (dn != PNULL) {
        get_gpio_id_ex(dn, GPIO_SWITCH_MODE, &gpio_id, 0);
    }

    return gpio_id;
}


int sprd_flash_on(void)
{
	int gpio_flash_en = get_gpio_flash_en_id();
	int gpio_switch_en = get_gpio_switch_id();
	gpio_direction_output(gpio_flash_en, SPRD_FLASH_OFF);
	gpio_direction_output(gpio_switch_en, SPRD_FLASH_ON);

	return 0;
}

int sprd_flash_high_light(void)
{
	int gpio_flash_en = get_gpio_flash_en_id();
	int gpio_switch_en = get_gpio_switch_id();
	gpio_direction_output(gpio_flash_en, SPRD_FLASH_ON);
	gpio_direction_output(gpio_switch_en, SPRD_FLASH_OFF);

	return 0;
}

int sprd_flash_close(void)
{
	int gpio_flash_en = get_gpio_flash_en_id();
	int gpio_switch_en = get_gpio_switch_id();
	gpio_direction_output(gpio_flash_en, SPRD_FLASH_OFF);
	gpio_direction_output(gpio_switch_en, SPRD_FLASH_OFF);

	return 0;
}

int sprd_flash_cfg(struct sprd_flash_cfg_param *param, void *arg)
{
	return 0;
}
