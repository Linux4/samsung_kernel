/*
 * DSPG DBMD2 codec driver customer interface
 *
 * Copyright (C) 2014 DSP Group
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

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/module.h>

#include "dbmd2-customer.h"

unsigned long customer_dbmd2_clk_get_rate(
		struct dbmd2_private *p, enum dbmd2_clocks clk)
{
	dev_dbg(p->dev, "%s: %s\n",
		__func__,
		clk == DBMD2_CLK_CONSTANT ? "constant" : "master");
	return 0;
}
EXPORT_SYMBOL(customer_dbmd2_clk_get_rate);

int customer_dbmd2_clk_enable(struct dbmd2_private *p, enum dbmd2_clocks clk)
{
	dev_dbg(p->dev, "%s: %s\n",
		__func__,
		clk == DBMD2_CLK_CONSTANT ? "constant" : "master");
	return 0;
}
EXPORT_SYMBOL(customer_dbmd2_clk_enable);

void customer_dbmd2_clk_disable(struct dbmd2_private *p, enum dbmd2_clocks clk)
{
	dev_dbg(p->dev, "%s: %s\n",
		__func__,
		clk == DBMD2_CLK_CONSTANT ? "constant" : "master");
}
EXPORT_SYMBOL(customer_dbmd2_clk_disable);

