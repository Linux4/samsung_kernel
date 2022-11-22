/*
 * opyright (C) 2012 Spreadtrum Communications Inc.
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
#include <soc/sprd/hardware.h>
#include <soc/sprd/board.h>
#include <soc/sprd/adi.h>
#include "../common/parse_hwinfo.h"

int sprd_flash_on(void)
{
    printk("sprd_flash_on \n");

    return 0;
}

int sprd_flash_high_light(void)
{
    printk("sprd_flash_high_light \n");

    return 0;
}

int sprd_flash_close(void)
{
    printk("sprd_flash_close \n");

    return 0;
}

int sprd_flash_cfg(struct sprd_flash_cfg_param *param, void *arg)
{
    return 0;
}
