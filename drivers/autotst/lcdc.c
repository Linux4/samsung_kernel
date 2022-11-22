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

#include <linux/fb.h>
#include <linux/clk.h>
#include <linux/delay.h>

#include "lcdc_reg.h"


//#define  LCDC_DEBUG
#ifdef LCDC_DEBUG
#define LCDC_PRINT printk
#else
#define LCDC_PRINT(...)
#endif


int32_t lcm_send_cmd (uint32_t cmd)
{
	/* wait for that the ahb enter idle state */
	while(lcdc_read(LCM_CTRL) & BIT(20));

	lcdc_write(cmd, LCM_CMD);
	return 0;
}

int32_t lcm_send_cmd_data (uint32_t cmd, uint32_t data)
{
	/* wait for that the ahb enter idle state */
	while(lcdc_read(LCM_CTRL) & BIT(20));

	lcdc_write(cmd, LCM_CMD);

	/* wait for that the ahb enter idle state */
	while(lcdc_read(LCM_CTRL) & BIT(20));

	lcdc_write(data, LCM_DATA);
	return 0;
}

int32_t lcm_send_data (uint32_t data)
{
	/* wait for that the ahb enter idle state */
	 while(lcdc_read(LCM_CTRL) & BIT(20));

	lcdc_write(data, LCM_DATA);
	return 0;
}

uint32_t lcm_read_data (void)
{
	/* wait for that the ahb enter idle state */
	while(lcdc_read(LCM_CTRL) & BIT(20));
	lcdc_write(1 << 24, LCM_DATA);
	udelay(50);
	return lcdc_read(LCM_RDDATA);
}

static struct clk * s_lcdcclk = NULL;

int lcm_init(void)
{
	//int i;
	unsigned int ctrl;

    if( NULL == s_lcdcclk ) {
        s_lcdcclk = clk_get(NULL, "clk_lcdc");
    }
	if( s_lcdcclk == NULL ) {
		printk(KERN_ERR "can not get clk_lcdc!!!!!\n");
		return -EFAULT;
	}

	clk_enable(s_lcdcclk);

    ctrl = lcdc_read(LCM_CTRL);
	printk("reg[LCM_CTRL] = %x\n", ctrl);

	ctrl &= 0x10000;
	ctrl |= 0x02828; // bus&pixel are 24bits

    lcdc_write(ctrl, LCM_CTRL);

    //lcdc_write((1 << 8) | (5 << 4) | 5, LCM_TIMING0);
    //lcdc_write((1 << 8) | (5 << 4) | 5, LCM_TIMING1);

    //for( i = LCM_CTRL; i <= LCM_RSTN;  i += 4 ) {
    //    printk("reg[%x] = %x\n", i, lcdc_read(i));
    //}

	return 0;
}

int lcm_close(void)
{
	if( NULL != s_lcdcclk ) {
		clk_disable(s_lcdcclk);
	}
	printk("lcm_close \n");
	return 0;
}


