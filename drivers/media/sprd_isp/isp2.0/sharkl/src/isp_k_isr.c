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

struct isp_int_num {
	uint32_t internal_num;
	uint32_t external_num;
};

static struct isp_int_num isp_int_tab[] = {
	{ISP_INT_HIST_STORE,       ISP_INT_EVT_HIST_STORE},
	{ISP_INT_STORE,            ISP_INT_EVT_STORE_DONE},
	{ISP_INT_LSC_LOAD,         ISP_INT_EVT_LSC_LOAD},
	{ISP_INT_HIST_CAL,         ISP_INT_EVT_HIST_CAL},
	{ISP_INT_HIST_RST,         ISP_INT_EVT_HIST_RST},
	{ISP_INT_FETCH_BUF_FULL,   ISP_INT_EVT_FETCH_BUF_FULL},
	{ISP_INT_STORE_BUF_FULL,   ISP_INT_EVT_STORE_BUF_FULL},
	{ISP_INT_STORE_ERR,        ISP_INT_EVT_STORE_ERR},
	{ISP_INT_SHADOW,           ISP_INT_EVT_SHADOW_DONE},
	{ISP_INT_PREVIEW_STOP,     ISP_INT_EVT_PREVIEW_STOP},
	{ISP_INT_AWB_DONE,         ISP_INT_EVT_AWBM_DONE},
	{ISP_INT_AF_DONE,          ISP_INT_EVT_AFM_Y_DONE},
	{ISP_INT_SLICE_CNT,        ISP_INT_EVT_SLICE_CNT},
	{ISP_INT_AE_DONE,          ISP_INT_EVT_AEM_DONE},
	{ISP_INT_ANTI_FLICKER,     ISP_INT_EVT_AFL_DONE},
	{ISP_INT_AWB_START,        ISP_INT_EVT_AWBM_START},
	{ISP_INT_AF_START,         ISP_INT_EVT_AFM_Y_START},
	{ISP_INT_AE_START,         ISP_INT_EVT_AEM_START},
	{ISP_INT_DCAM_SOF,         ISP_INT_EVT_DCAM_SOF},
	{ISP_INT_DCAM_EOF,         ISP_INT_EVT_DCAM_EOF},
	{ISP_INT_AFM_WIN8,         ISP_INT_EVT_AFM_Y_WIN8},
	{ISP_INT_AFM_WIN7,         ISP_INT_EVT_AFM_Y_WIN7},
	{ISP_INT_AFM_WIN6,         ISP_INT_EVT_AFM_Y_WIN6},
	{ISP_INT_AFM_WIN5,         ISP_INT_EVT_AFM_Y_WIN5},
	{ISP_INT_AFM_WIN4,         ISP_INT_EVT_AFM_Y_WIN4},
	{ISP_INT_AFM_WIN3,         ISP_INT_EVT_AFM_Y_WIN3},
	{ISP_INT_AFM_WIN2,         ISP_INT_EVT_AFM_Y_WIN2},
	{ISP_INT_AFM_WIN1,         ISP_INT_EVT_AFM_Y_WIN1},
	{ISP_INT_AFM_WIN0,         ISP_INT_EVT_AFM_Y_WIN0},
};

int32_t isp_get_int_num(struct isp_node *node)
{
	uint32_t i = 0, j = 0, cnt = 0;
	uint32_t status = 0;
	uint32_t irq_line = 0, irq_num = 0;

	status = REG_RD(ISP_INT_STATUS);
	irq_line = status & ISP_IRQ_HW_MASK;

	if (0 == irq_line) {
		printk("isp_get_int_num: int error.\n");
		return -1;
	}

	cnt = sizeof(isp_int_tab) / sizeof(isp_int_tab[0]);
	for (i = 0; i < ISP_IRQ_NUM; i++) {
		irq_num = irq_line & (1 << (uint32_t)i);
		if (irq_num) {
			if (ISP_INT_HIST_STORE == irq_num
				|| ISP_INT_HIST_CAL == irq_num
				|| ISP_INT_HIST_RST == irq_num
				|| ISP_INT_STORE_ERR == irq_num
				|| ISP_INT_PREVIEW_STOP == irq_num
				|| ISP_INT_SLICE_CNT == irq_num) {
				for (j = 0; j < cnt; j++) {
					if (irq_num == isp_int_tab[j].internal_num) {
						node->irq_val0 |= isp_int_tab[j].external_num;
						break;
					}
				}
			} else if (ISP_INT_AFM_WIN8 == irq_num
				|| ISP_INT_AFM_WIN7 == irq_num
				|| ISP_INT_AFM_WIN6 == irq_num
				|| ISP_INT_AFM_WIN5 == irq_num
				|| ISP_INT_AFM_WIN4 == irq_num
				|| ISP_INT_AFM_WIN3 == irq_num
				|| ISP_INT_AFM_WIN2 == irq_num
				|| ISP_INT_AFM_WIN1 == irq_num
				|| ISP_INT_AFM_WIN0 == irq_num) {
				for (j = 0; j < cnt; j++) {
					if (irq_num == isp_int_tab[j].internal_num) {
						node->irq_val2 |= isp_int_tab[j].external_num;
						break;
					}
				}
			} else {
				for (j = 0; j < cnt; j++) {
					if (irq_num == isp_int_tab[j].internal_num) {
						node->irq_val1 |= isp_int_tab[j].external_num;
						break;
					}
				}
			}
		}
		irq_line &= ~(uint32_t)(1 << (uint32_t)i);
		if (!irq_line)
			break;
	}

	REG_WR(ISP_INT_CLEAR, status);

	return 0;
}

void isp_clr_int(void)
{
	REG_WR(ISP_INT_CLEAR, ISP_IRQ_HW_MASK);
}

void isp_en_irq(uint32_t irq_mode)
{
	REG_WR(ISP_INT_CLEAR, ISP_IRQ_HW_MASK);

	switch(irq_mode) {
	case ISP_INT_VIDEO_MODE:
		REG_WR(ISP_INT_EN, (ISP_INT_AWB_DONE | ISP_INT_AFM_WIN0 | ISP_INT_DCAM_SOF));
		break;
	case ISP_INT_CAPTURE_MODE:
		REG_WR(ISP_INT_EN, ISP_INT_STORE);
		break;
	case ISP_INT_CLEAR_MODE:
		REG_WR(ISP_INT_EN, 0);
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

	REG_OWR(ISP_AXI_MASTER_STOP, BIT_18);

	reg_value = REG_RD(ISP_AXI_MASTER);
	while ((0x00 == (reg_value & BIT_3))
		&& (time_out_cnt < ISP_TIME_OUT_MAX)) {
		time_out_cnt++;
		udelay(50);
		reg_value = REG_RD(ISP_AXI_MASTER);
	}

	if (time_out_cnt >= ISP_TIME_OUT_MAX) {
		ret = -1;
		printk("isp_axi_bus_waiting: time out.\n");
	}

	return ret;
}
