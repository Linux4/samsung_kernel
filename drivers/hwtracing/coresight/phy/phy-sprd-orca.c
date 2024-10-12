/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * Description: CoreSight Trace Memory Controller driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/spinlock.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>
#include <linux/coresight.h>
#include <linux/amba/bus.h>

#include "../coresight-phy.h"

static struct cs_element_t s_cs_group[] = {
	{"pscp_funnel", CS_FUNNEL, 0x78106000, 0x8, 0, true, 0x640100D4, 0, 0xf00},
	{"v3phy_funnel", CS_FUNNEL, 0x78206000, 0x8, 0, true, 0x640100D4, 0, 0xf0},
	{"nrcp_funnel", CS_FUNNEL, 0x78306000, 0x8, 0, true, 0x640100D4, 0, 0xf0000},
	{"soc_funnel", CS_FUNNEL, 0x78002000, 0x8, 0, false, 0, 0, 0},
	{"soc_etb_regs", CS_TMC, 0x78003000, 0x8000, 0, false, 0, 0, 0},
	{"etb_pscp_c0_regs", CS_TMC, 0x78101000, 0x1000, 0, true, 0x640100D4, 0, 0xf00/* bit8:11 */},
	{"etb_pscp_c1_regs", CS_TMC, 0x78102000, 0x1000, 0, true, 0x640100D4, 0, 0xf00/* bit8:11 */},
	{"etb_v3phy_regs", CS_TMC, 0x78201000, 0x1000, 0, true, 0x640100D4, 0, 0xf0/* bit4:7 */},
	{"etb_nrcp_c0_regs", CS_TMC, 0x78301000, 0x1000, 0, true, 0x640100D4, 0, 0xf0000/* bit16:19 */},
	{"etb_nrcp_c1_regs", CS_TMC, 0x78302000, 0x1000, 0, true, 0x640100D4, 0, 0xf0000/* bit16:19 */},
};

int cs_group_size(void)
{
	return ARRAY_SIZE(s_cs_group);
}

struct cs_element_t *cs_group_ptr(void)
{
	return s_cs_group;
}

void enable_coresight_atb_clk(struct device *dev)
{
}