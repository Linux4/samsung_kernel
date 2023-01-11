/*
 * drivers/video/mmp/hw/dfc.h
 *
 * Copyright (C) 2013 Marvell Technology Group Ltd.
 * Authors:  Yonghai Huang <huangyh@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef _DFC_H_
#define _DFC_H_

struct mmp_dfc {
	unsigned int name;
	atomic_t commit;
	spinlock_t lock;
	void __iomem	*apmu_reg;
	void __iomem	*sclk_reg;
	unsigned int apmu_value;
	unsigned int sclk_value;
	unsigned int old_rate;
	unsigned long dsi_rate;
	unsigned long path_rate;
	unsigned long best_parent;
	struct clk *current_parent1;
	struct clk *current_parent2;
	struct clk *parent0;
	struct clk *parent1;
	struct clk *parent2;
	struct list_head queue;
};

struct mmp_dfc_list {
	struct mmp_dfc dfc;
	struct list_head queue;
};

extern void mmp_dfc_init(struct device *dev);
extern void mmp_register_dfc_handler(struct device *dev,
	struct mmp_vsync *vsync);
#endif	/* _MMP_DFC_H_ */
