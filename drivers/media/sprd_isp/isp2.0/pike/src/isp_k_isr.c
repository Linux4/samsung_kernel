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

#include <linux/uaccess.h>
#include <linux/sprd_mm.h>
#include <video/sprd_isp.h>
#include <linux/delay.h>
#include "isp_reg.h"
#include "isp_drv.h"

#define ISP_TIME_OUT_MAX (500)

int32_t isp_get_int_num(struct isp_node *node)
{

	node->irq_val1 = REG_RD(ISP_INT_INT0);
	node->irq_val2 = REG_RD(ISP_INT_INT1);
	node->irq_val3 = REG_RD(ISP_INT_INT2);

	if ((0 == node->irq_val1)
		&& (0 == node->irq_val2)
		&& (0 == node->irq_val3)) {
		printk("isp_get_int_num: int error.\n");
		return -1;
	}

	REG_WR(ISP_INT_CLR0, ISP_IRQ_HW_MASK);
	REG_WR(ISP_INT_CLR1, ISP_IRQ_HW_MASK);
	REG_WR(ISP_INT_CLR2, ISP_IRQ_HW_MASK);

	return 0;
}

void isp_clr_int(void)
{
	REG_WR(ISP_INT_CLR0, ISP_IRQ_HW_MASK);
	REG_WR(ISP_INT_CLR1, ISP_IRQ_HW_MASK);
	REG_WR(ISP_INT_CLR2, ISP_IRQ_HW_MASK);
}

void isp_en_irq(uint32_t irq_mode)
{
	REG_WR(ISP_INT_CLR0, ISP_IRQ_HW_MASK);
	REG_WR(ISP_INT_CLR1, ISP_IRQ_HW_MASK);
	REG_WR(ISP_INT_CLR2, ISP_IRQ_HW_MASK);

	switch(irq_mode) {
	case ISP_INT_VIDEO_MODE:
		REG_WR(ISP_INT_EN0, ISP_INT_DCAMERA_SOF | ISP_INT_AEM_DONE | ISP_INT_AFM_RGB_DONE | ISP_INT_AFL_DONE);
		REG_WR(ISP_INT_EN1, 0);
		REG_WR(ISP_INT_EN2, 0);
		break;
	case ISP_INT_CAPTURE_MODE:
		REG_WR(ISP_INT_EN0, ISP_INT_STORE_DONE);
		REG_WR(ISP_INT_EN1, 0);
		REG_WR(ISP_INT_EN2, 0);
		break;
	case ISP_INT_CLEAR_MODE:
		REG_WR(ISP_INT_EN0, 0);
		REG_WR(ISP_INT_EN1, 0);
		REG_WR(ISP_INT_EN2, 0);
		break;
	default:
		printk("isp_en_irq: irq_mode error.\n");
		break;
	}
}

int32_t isp_axi_bus_waiting(void)
{
	int32_t ret = 0;
	uint32_t reg_value = 0;
	uint32_t time_out_cnt = 0;

	REG_OWR(ISP_AXI_ITI2AXIM_CTRL, BIT_18);

	reg_value = REG_RD(ISP_INT_STATUS);
	while ((0x00 == (reg_value & BIT_3))
		&& (time_out_cnt < ISP_TIME_OUT_MAX)) {
		time_out_cnt++;
		udelay(50);
		reg_value = REG_RD(ISP_INT_STATUS);
	}

	if (time_out_cnt >= ISP_TIME_OUT_MAX) {
		ret = -1;
		printk("isp_axi_bus_waiting: time out.\n");
	}

	return ret;
}
