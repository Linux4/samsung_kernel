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
#include <linux/types.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/sprd_mm.h>


#ifdef CONFIG_OF
extern uint32_t                   dcam_regbase;
#define DCAM_BASE		dcam_regbase
#else
#define DCAM_BASE		SPRD_DCAM_BASE
#endif

void   parse_baseaddress(struct device_node	*dn);
uint32_t   parse_irq(struct device_node *dn);
struct clk  * parse_clk(struct device_node *dn, char *clkname);

