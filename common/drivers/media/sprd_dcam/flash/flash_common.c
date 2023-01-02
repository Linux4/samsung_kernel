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

int sprd_flash_on(void)
{
	printk("sprd_flash_on \n");
	sci_adi_clr(SPRD_ADISLAVE_BASE + SPRD_FLASH_OFST, SPRD_FLASH_HIGH_VAL);
	sci_adi_set(SPRD_ADISLAVE_BASE + SPRD_FLASH_OFST, SPRD_FLASH_CTRL_BIT | SPRD_FLASH_LOW_VAL); /*0x3 = 110ma*/
	return 0;
}

int sprd_flash_high_light(void)
{
	printk("sprd_flash_high_light \n");
	sci_adi_set(SPRD_ADISLAVE_BASE + SPRD_FLASH_OFST, SPRD_FLASH_CTRL_BIT | SPRD_FLASH_HIGH_VAL); /*0xf = 470ma*/
	return 0;
}

int sprd_flash_close(void)
{
	printk("sprd_flash_close \n");
	sci_adi_clr(SPRD_ADISLAVE_BASE + SPRD_FLASH_OFST, SPRD_FLASH_CTRL_BIT | SPRD_FLASH_HIGH_VAL);
	return 0;
}
