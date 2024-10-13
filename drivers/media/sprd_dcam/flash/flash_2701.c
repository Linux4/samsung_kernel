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
#ifdef CONFIG_64BIT
#include <soc/sprd/board.h>
#include <soc/sprd/hardware.h>
#include <soc/sprd/adi.h>
#include <soc/sprd/sci_glb_regs.h>

#endif

extern void sprd_2701_enable_flash(int enable);
extern void sprd_2701_set_flash_brightness(unsigned char  brightness);

int sprd_flash_on(void)
{
	printk("sprd_flash_on \n");
	sprd_2701_enable_flash(1);
	sprd_2701_set_flash_brightness(0x04);
	return 0;
}

int sprd_flash_high_light(void)
{
	printk("sprd_flash_high_light \n");
	sprd_2701_enable_flash(1);
	sprd_2701_set_flash_brightness(0x0c);
	return 0;
}

int sprd_flash_close(void)
{
	printk("sprd_flash_close \n");
	sprd_2701_enable_flash(0);
	return 0;
}

int sprd_flash_cfg(void *param, void *arg)
{
	return 0;
}
