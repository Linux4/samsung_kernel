/*
 * linux/drivers/video/mmp/hw/dfc.c
 * dfc driver support
 *
 * Copyright (C) 2014 Marvell Technology Group Ltd.
 * Authors:  huangyh <huangyh@marvell.com>
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

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/device.h>
#include <linux/stat.h>
#include "mmp_dsi.h"
#include "mmp_ctrl.h"
#include <video/mipi_display.h>
#include <linux/clk-private.h>
#include "clk.h"
#include "clk-pll-helanx.h"

#define ROUND_RATE_RANGE_MHZ		2  /* unit: MHz */
#define DFC_RETRY_TIMEOUT	60

#define LCD_PN_SCLK	(0x1a8)
#define	SCLK_SOURCE_SELECT_DC4_LITE(src)		(((src)<<29) | ((src)<<12))
#define	SCLK_SOURCE_SELECT_MASK_DC4_LITE	0x60003000
#define	DSI1_BITCLK_DIV(div)			(div<<8)
#define	DSI1_BITCLK_DIV_MASK			0x00000F00
#define	CLK_INT_DIV(div)				(div)
#define	CLK_INT_DIV_MASK				0x000000FF

/*
 * - MAX_RATE_NUM: max numbers of rate from lcd&dsi's parent clk;
 * - MAX_CLK_NUM: max numbers of rate from lcd&dsi's parent1 and parent2 clk;
 *		  parent1 is parent of parent0, parent2 is parent of parent1;
 * - MAX_PARENT_CLK_NUM: max number of all lcd&dsi's parent clk;
 */
#define MAX_RATE_NUM 8
#define MAX_CLK_NUM 8
#define MAX_PARENT_CLK_NUM 16
static unsigned int parent_rate_tbl[MAX_RATE_NUM];
static int parent_rate_count;
static const char * const parent0_clk_tbl[] = {"dsi_sclk"};
static struct clk *parent1_clk_tbl[MAX_CLK_NUM];
static struct clk *parent2_clk_tbl[MAX_CLK_NUM];
static struct clk *parent_clk_tbl[MAX_PARENT_CLK_NUM];
static int parent_clk_count;
unsigned char parent2_count;
static unsigned long dsipll_vco_default;
static unsigned long dsipll_default;
static unsigned int lcd_clk_en;
static unsigned int plat;
static unsigned int sclk_div;
static unsigned int current_dip_status;

static struct clk *hclk;
static struct clk *dsipll;
static struct clk *dsipll_vco;
static struct clk *clk_parent0;

#if 0
/* PLL post divider table */
static struct div_map pll_post_div_tbl[] = {
	/* divider, reg vaule */
	{1, 0},
	{2, 2},
	{3, 4},
	{4, 5},
	{6, 7},
	{8, 8},
};
#else
/* PLL post divider table for 28nm pll */
static struct div_map pll_post_div_tbl[] = {
	/* divider, reg vaule */
	{1, 0x0},
	{2, 0x1},
	{4, 0x2},
	{8, 0x3},
	{16, 0x4},
	{32, 0x5},
	{64, 0x6},
	{128, 0x7},
};
#endif
struct parent_index {
	int parent1;
	int parent2;
};

enum {
	PXA1088,
	PXA1L88,
	PXA1928,
	PXA1U88,
};

enum {
	DFC_FREQ_PARENT2_PLL_NONE,
	DFC_FREQ_PARENT2_PLL1_624M,
	DFC_FREQ_PARENT2_PLL1_832M,
	DFC_FREQ_PARENT2_PLL1_499M,
	DFC_FREQ_PARENT2_DSIPLL,
	DFC_FREQ_PARENT2_DSIPLL_DIV3,
};

enum {
	DFC_FREQ_PARENT1_NONE,
	DFC_FREQ_PARENT1_DISPLAY1,
	DFC_FREQ_PARENT1_DSIPLL,
};

enum {
	DFC_CHANGE_LCD,
	DFC_DISABLE_DSIPLL,
	DFC_ENABLE_DSIPLL,
	DFC_CHANGE_DSIPLL,
	DFC_RESTORE_DSIPLL,
};

static u32 dsi_get_bpp_from_inputfmt(int fmt)
{
	switch (fmt) {
	case PIXFMT_BGR565:
		return 16;
	case PIXFMT_BGR666PACK:
		return 18;
	case PIXFMT_BGR666UNPACK:
		return 24;
	case PIXFMT_BGR888PACK:
		return 24;
	default:
		pr_err("%s, err: fmt not support or mode not set\n", __func__);
		return 0;
	}
}

static unsigned long set_pixel_clk(struct mmp_dsi *dsi, unsigned long dsi_rate)
{
	unsigned long pixel_rate;
	int x;
	u32 bpp = dsi_get_bpp_from_inputfmt(
			dsi->mode.pix_fmt_out);

	x = bpp / dsi->setting.lanes;
	pixel_rate = dsi_rate / x;
	/*
	 * FIXME: On ULC&helan3:
	 * For WVGA with 2 lanes whose pixel_clk is lower than
	 * esc clk, we need to set pixel >= esc clk, so that LP
	 * cmd and video mode cna work.
	 */
	if (DISP_GEN4_LITE(dsi->version) || DISP_GEN4_PLUS(dsi->version)) {
		if (pixel_rate < ESC_52M)
			pixel_rate = ESC_52M;
	}

	return pixel_rate;
}

static void get_parent_index(unsigned long rate, struct parent_index *index)
{
	int i = 0;
	struct clk *clk;

	for (i = 0; i < parent2_count; i++) {
		clk = parent2_clk_tbl[i];
		if ((!IS_ERR(clk)) && (rate == (clk->rate / MHZ_TO_HZ)))
			index->parent2 = i + DFC_FREQ_PARENT2_PLL1_624M;
	}

	if ((index->parent2 == DFC_FREQ_PARENT2_PLL1_624M) ||
		(index->parent2 == DFC_FREQ_PARENT2_PLL1_832M) ||
		(index->parent2 == DFC_FREQ_PARENT2_PLL1_499M))
		index->parent1 = DFC_FREQ_PARENT1_DISPLAY1;

	if ((index->parent2 == DFC_FREQ_PARENT2_DSIPLL) ||
		(index->parent2 == DFC_FREQ_PARENT2_DSIPLL_DIV3))
		index->parent1 = DFC_FREQ_PARENT1_DSIPLL;
}

static void add_to_parent_rate_tbl(unsigned long rate)
{
	int i;

	rate /= MHZ_TO_HZ;
	if (rate > 1600)
		return;
	for (i = 0; i < MAX_RATE_NUM; i++) {
		if (parent_rate_tbl[i] == rate)
			return;
	}
	parent_rate_tbl[parent_rate_count] = rate;
	parent_rate_count++;
}

/*
 * create clk table for lcd&dsi's parent clk in init stage;
 */
static void create_parent_tbl(struct device *dev, struct clk *clk11)
{
	int i;
	struct clk *clk1;

	for (i = 0; i < clk11->num_parents; i++) {
		clk1 = devm_clk_get(dev, clk11->parent_names[i]);
		if (clk1) {
			parent_clk_tbl[parent_clk_count++] = clk1;
			add_to_parent_rate_tbl(clk1->rate);
			create_parent_tbl(dev, clk1);
		}
	}
}

/*
 * For each dfc, first update parent_rate_tbl from clk of parent_clk_table;
 */
static void update_rate_tbl(void)
{
	int i;
	struct clk *clk;

	for (i = 0; i < MAX_RATE_NUM; i++) {
		parent_rate_tbl[i] = 0x0;
		parent_rate_count = 0;
	}

	for (i = 0; i < parent_clk_count; i++) {
		clk = parent_clk_tbl[i];
		if (clk)
			add_to_parent_rate_tbl(clk->rate);
	}
}

static int find_best_parent(unsigned int rate, unsigned int *count)
{
	int i, j;
	unsigned int tmp = 0xff;
	unsigned int real_rate;
	int ret = 0;

	for (i = 0; i < parent_rate_count; i++) {
		for (j = 1; j < 15; j++) {
			real_rate = parent_rate_tbl[i] / j;
			if (rate > real_rate + ROUND_RATE_RANGE_MHZ)
				break;
			else if ((rate <= (real_rate + ROUND_RATE_RANGE_MHZ)) &&
				 (rate >= (real_rate - ROUND_RATE_RANGE_MHZ))) {
				/* round clock rate in 1MHz range */
				tmp = real_rate;
				*count = i;
				ret = 1;
				break;
			}
		}

		if (tmp != 0xff)
			break;
	}

	return ret;
}

static void get_current_parent_index(struct clk *clk,
	struct parent_index *index)
{
	struct clk *temp = clk;
	int i;

	while (!IS_ERR_OR_NULL(temp)) {
		for (i = 0; i < parent2_count; i++) {
			if (!strcmp(temp->name, parent2_clk_tbl[i]->name)) {
				index->parent2 = i + DFC_FREQ_PARENT2_PLL1_624M;
				break;
			}
		}
		temp = clk_get_parent(temp);
	}

	if ((index->parent2 == DFC_FREQ_PARENT2_PLL1_624M) ||
		(index->parent2 == DFC_FREQ_PARENT2_PLL1_832M) ||
		(index->parent2 == DFC_FREQ_PARENT2_PLL1_499M))
		index->parent1 = DFC_FREQ_PARENT1_DISPLAY1;

	if ((index->parent2 == DFC_FREQ_PARENT2_DSIPLL) ||
		(index->parent2 == DFC_FREQ_PARENT2_DSIPLL_DIV3))
		index->parent1 = DFC_FREQ_PARENT1_DSIPLL;
}

static int is_parent1_changed(struct mmphw_ctrl *ctrl)
{
	return strcmp(ctrl->dfc.current_parent1->name, ctrl->dfc.parent1->name);
}

static int is_parent2_changed(struct mmphw_ctrl *ctrl)
{
	return strcmp(ctrl->dfc.current_parent2->name, ctrl->dfc.parent2->name);
}

static void calculate_reg_value(struct mmphw_ctrl *ctrl)
{
	unsigned int sclk;
	unsigned int apmu;
	struct mmp_dfc *dfc = &ctrl->dfc;
	u32  bclk_div, pclk_div;

	sclk = readl_relaxed(dfc->sclk_reg);
	apmu = readl_relaxed(dfc->apmu_reg);
	sclk &= ~(SCLK_SOURCE_SELECT_MASK_DC4_LITE |
			DSI1_BITCLK_DIV_MASK |
			CLK_INT_DIV_MASK);
	bclk_div = (dfc->parent2->rate + dfc->dsi_rate / 2) / dfc->dsi_rate;
	pclk_div = (dfc->parent2->rate + dfc->path_rate / 2) / dfc->path_rate;
	if (dfc->parent2->rate / pclk_div < ESC_52M)
		/*
		 * For GEN4 and GEN4_PLUS, it has requirement that need
		 * path_clk > ESC_clk,when we set PATH_CLK = ESC_CLK, pclk_div
		 * is calculated by ROUND_UP, so the final path_clk rate may be
		 * less than ESC_CLK, need to check here and round down.
		 */
		pclk_div = (dfc->parent2->rate - dfc->path_rate / 2) / dfc->path_rate;

	sclk |= DSI1_BITCLK_DIV(bclk_div) | CLK_INT_DIV(pclk_div);

	if (!strcmp(dfc->parent2->name,
		parent2_clk_tbl[DFC_FREQ_PARENT2_PLL1_624M - 1]->name)) {
		apmu = 0x9003f;
		sclk |= SCLK_SOURCE_SELECT_DC4_LITE(0);
	} else if (!strcmp(dfc->parent2->name,
		parent2_clk_tbl[DFC_FREQ_PARENT2_PLL1_832M - 1]->name)) {
		apmu = 0x9023f;
		sclk |= SCLK_SOURCE_SELECT_DC4_LITE(0);
	} else if (!strcmp(dfc->parent2->name,
		parent2_clk_tbl[DFC_FREQ_PARENT2_PLL1_499M - 1]->name)) {
		apmu = 0x9043f;
		sclk |= SCLK_SOURCE_SELECT_DC4_LITE(0);
	} else if (!strcmp(dfc->parent2->name,
		parent2_clk_tbl[DFC_FREQ_PARENT2_DSIPLL - 1]->name)) {
		apmu = 0x9011f;
		sclk |= SCLK_SOURCE_SELECT_DC4_LITE(3);
	} else if (!strcmp(dfc->parent2->name,
		parent2_clk_tbl[DFC_FREQ_PARENT2_DSIPLL_DIV3 - 1]->name)) {
		apmu = 0x9411f;
		sclk |= SCLK_SOURCE_SELECT_DC4_LITE(3);
	}
	dfc->apmu_value = apmu;
	dfc->sclk_value = sclk;
}

/* rate unit is Mhz */
static void dynamic_change_dsipll(unsigned int rate, int type)
{
	unsigned long i, dsipll_rate, dsipllvco_rate = 0;

	switch (type) {
	case DFC_RESTORE_DSIPLL:
		if (!(clk_get_rate(dsipll) == dsipll_default)) {
			dsipll_rate = dsipll_default;
			clk_set_rate(dsipll, dsipll_rate);
		}
		break;
	case DFC_DISABLE_DSIPLL:
		clk_disable_unprepare(dsipll);
		break;
	case DFC_ENABLE_DSIPLL:
		clk_prepare_enable(dsipll);
		break;
	default:
		if (PXA1928 != plat) {
			for (i = 0; i < ARRAY_SIZE(pll_post_div_tbl); i++) {
				dsipllvco_rate = rate * pll_post_div_tbl[i].div;
				if ((dsipllvco_rate > 1500) &&
						(dsipllvco_rate < 3000))
					break;
			}

			if (i == ARRAY_SIZE(pll_post_div_tbl))
				BUG_ON("Multiplier is out of range\n");

			dsipll_rate = rate * MHZ_TO_HZ;
			dsipllvco_rate *= MHZ_TO_HZ;
			clk_set_rate(dsipll_vco, dsipllvco_rate);
			clk_set_rate(dsipll, dsipll_rate);
		}
		break;
	}

	usleep_range(500, 1000);
}

static int dynamic_change_lcd(struct mmphw_ctrl *ctrl, struct mmp_dfc *dfc)
{
	struct mmp_path *path = ctrl->path_plats[0].path;
	struct mmp_dsi *dsi = mmp_path_to_dsi(path);
	struct parent_index source;
	int count = 0;
	struct clk *dsi_clk = dsi->clk;
	unsigned long flags;
	int ret = 0;

	memset(&source, 0, sizeof(source));
	get_current_parent_index(dsi_clk, &source);
	dfc->current_parent1 = parent1_clk_tbl[source.parent1-1];
	dfc->current_parent2 = parent2_clk_tbl[source.parent2-1];
	memcpy(&ctrl->dfc, dfc, sizeof(*dfc));

	calculate_reg_value(ctrl);
	if (path->status) {
		atomic_set(&ctrl->dfc.commit, 1);
		do {
			mmp_wait_vsync(&dsi->special_vsync);
			count++;
		} while (atomic_read(&ctrl->dfc.commit) && (count < DFC_RETRY_TIMEOUT));
	} else {
		writel_relaxed(ctrl->dfc.apmu_value, ctrl->dfc.apmu_reg);
		writel_relaxed(ctrl->dfc.sclk_value, ctrl->dfc.sclk_reg);
		sclk_div = ctrl->dfc.sclk_value;
	}

	spin_lock_irqsave(&ctrl->dfc.lock, flags);
	if (unlikely(count == DFC_RETRY_TIMEOUT)
		&& atomic_read(&ctrl->dfc.commit)) {
		atomic_set(&ctrl->dfc.commit, 0);
		spin_unlock_irqrestore(&ctrl->dfc.lock, flags);
		ret = -EFAULT;
	} else {
		spin_unlock_irqrestore(&ctrl->dfc.lock, flags);

		if (is_parent1_changed(ctrl))
			clk_set_parent(ctrl->dfc.parent0, ctrl->dfc.parent1);
		if (is_parent2_changed(ctrl))
			clk_set_parent(ctrl->dfc.parent1, ctrl->dfc.parent2);
	}

	return ret;
}

static int dfc_prepare(struct mmphw_ctrl *ctrl, unsigned int rate)
{
	struct mmp_path *path = ctrl->path_plats[0].path;
	struct mmp_dsi *dsi = mmp_path_to_dsi(path);
	struct parent_index target, source;
	int best_parent_count = 0;
	struct clk *dsi_clk = dsi->clk;
	struct mmp_dfc *dfc;
	struct mmp_dfc temp;
	int ret = 0;

	memset(&target, 0, sizeof(target));
	memset(&temp, 0, sizeof(temp));
	memcpy(&temp, &ctrl->dfc, sizeof(temp));

	temp.dsi_rate = rate * MHZ_TO_HZ;
	temp.path_rate = set_pixel_clk(dsi, temp.dsi_rate);

	temp.parent0 = clk_parent0;

	update_rate_tbl();
	if (!find_best_parent(rate, &best_parent_count))
		temp.best_parent = DFC_FREQ_PARENT2_PLL_NONE;
	else {
		get_parent_index(parent_rate_tbl[best_parent_count], &target);
		temp.best_parent = target.parent2;
	}

	memset(&source, 0, sizeof(source));
	get_current_parent_index(dsi_clk, &source);
	temp.current_parent1 =
		parent1_clk_tbl[source.parent1-1];
	temp.current_parent2 =
		parent2_clk_tbl[source.parent2-1];
	while (!list_empty(&ctrl->dfc_list.queue)) {
		/* free the waitlist elements if any */
		dfc = list_first_entry(&ctrl->dfc_list.queue,
			struct mmp_dfc, queue);
		list_del(&dfc->queue);
		kfree(dfc);
	}

	switch (temp.best_parent) {
	case DFC_FREQ_PARENT2_PLL1_624M:
	case DFC_FREQ_PARENT2_PLL1_832M:
	case DFC_FREQ_PARENT2_PLL1_499M:
		/* prepare the first change */
		temp.parent2 =
			parent2_clk_tbl[target.parent2-1];
		temp.parent1 =
			parent1_clk_tbl[target.parent1-1];

		/* add dfc request to list */
		dfc = kzalloc(sizeof(struct mmp_dfc),	GFP_KERNEL);
		if (dfc == NULL)
			goto prepare_fail;
		memcpy(dfc, &temp, sizeof(*dfc));
		dfc->name = DFC_CHANGE_LCD;
		list_add_tail(&dfc->queue, &ctrl->dfc_list.queue);

		/* prepare the second change */
		dfc = kzalloc(sizeof(struct mmp_dfc), GFP_KERNEL);
		if (dfc == NULL)
			goto prepare_fail;
		dfc->name = DFC_RESTORE_DSIPLL;
		list_add_tail(&dfc->queue, &ctrl->dfc_list.queue);
		break;
	case DFC_FREQ_PARENT2_DSIPLL:
		/* prepare enable dsipll change */
		dfc = kzalloc(sizeof(struct mmp_dfc), GFP_KERNEL);
		if (dfc == NULL)
			return -EFAULT;
		dfc->name = DFC_ENABLE_DSIPLL;
		list_add_tail(&dfc->queue, &ctrl->dfc_list.queue);

		/* prepare the first change */
		temp.parent2 =
			parent2_clk_tbl[target.parent2-1];
		temp.parent1 =
			parent1_clk_tbl[target.parent1-1];

		/* add dfc reqyest to list */
		dfc = kzalloc(sizeof(struct mmp_dfc), GFP_KERNEL);
		if (dfc == NULL)
			goto prepare_fail;
		memcpy(dfc, &temp, sizeof(*dfc));
		dfc->name = DFC_CHANGE_LCD;
		list_add_tail(&dfc->queue, &ctrl->dfc_list.queue);

		/* prepare disable dsipll change */
		dfc = kzalloc(sizeof(struct mmp_dfc), GFP_KERNEL);
		if (dfc == NULL)
			goto prepare_fail;
		dfc->name = DFC_DISABLE_DSIPLL;
		list_add_tail(&dfc->queue, &ctrl->dfc_list.queue);
		break;
	case DFC_FREQ_PARENT2_PLL_NONE:
		if (!strcmp(temp.current_parent2->name,
					parent2_clk_tbl[DFC_FREQ_PARENT2_DSIPLL - 1]->name) ||
				!strcmp(temp.current_parent2->name,
					parent2_clk_tbl[DFC_FREQ_PARENT2_DSIPLL_DIV3 - 1]->name)) {
			/* prepare the first change */
			temp.parent2 =
				parent2_clk_tbl[DFC_FREQ_PARENT2_PLL1_832M - 1];
			temp.parent1 =
				parent1_clk_tbl[DFC_FREQ_PARENT1_DISPLAY1 - 1];
			temp.dsi_rate = temp.parent2->rate/2;
			temp.path_rate = set_pixel_clk(dsi, temp.dsi_rate);
			/* add dfc reqyest to list */
			dfc = kzalloc(sizeof(struct mmp_dfc),	GFP_KERNEL);
			if (dfc == NULL)
				goto prepare_fail;
			memcpy(dfc, &temp, sizeof(*dfc));
			dfc->name = DFC_CHANGE_LCD;
			list_add_tail(&dfc->queue, &ctrl->dfc_list.queue);
		}

		/* prepare the second change */
		temp.dsi_rate = rate * MHZ_TO_HZ;
		temp.path_rate = set_pixel_clk(dsi, temp.dsi_rate);
		dfc = kzalloc(sizeof(struct mmp_dfc),	GFP_KERNEL);
		if (dfc == NULL)
			goto prepare_fail;
		memcpy(dfc, &temp, sizeof(*dfc));
		dfc->name = DFC_CHANGE_DSIPLL;
		list_add_tail(&dfc->queue, &ctrl->dfc_list.queue);

		/* prepare enable dsipll change */
		dfc = kzalloc(sizeof(struct mmp_dfc), GFP_KERNEL);
		if (dfc == NULL)
			return -EFAULT;
		dfc->name = DFC_ENABLE_DSIPLL;
		list_add_tail(&dfc->queue, &ctrl->dfc_list.queue);

		/* prepare the third change */
		temp.parent2 =
			parent2_clk_tbl[DFC_FREQ_PARENT2_DSIPLL - 1];
		temp.parent1 =
			parent1_clk_tbl[DFC_FREQ_PARENT1_DSIPLL - 1];
		dfc = kzalloc(sizeof(struct mmp_dfc), GFP_KERNEL);
		if (dfc == NULL)
			goto prepare_fail;
		memcpy(dfc, &temp, sizeof(*dfc));
		dfc->name = DFC_CHANGE_LCD;
		list_add_tail(&dfc->queue, &ctrl->dfc_list.queue);

		/* prepare disable dsipll change */
		dfc = kzalloc(sizeof(struct mmp_dfc), GFP_KERNEL);
		if (dfc == NULL)
			goto prepare_fail;
		dfc->name = DFC_DISABLE_DSIPLL;
		list_add_tail(&dfc->queue, &ctrl->dfc_list.queue);
		break;
	default:
		break;
	}
	return ret;
prepare_fail:
	while (!list_empty(&ctrl->dfc_list.queue)) {
		/* free the waitlist elements if any */
		dfc = list_first_entry(&ctrl->dfc_list.queue,
			struct mmp_dfc, queue);
		list_del(&dfc->queue);
		kfree(dfc);
	}
	return -EFAULT;
}

static int dfc_commit(struct mmphw_ctrl *ctrl)
{
	struct mmp_dfc *dfc;
	int ret = 0;

	while (!list_empty(&ctrl->dfc_list.queue)) {
		/* free the waitlist elements if any */
		dfc = list_first_entry(&ctrl->dfc_list.queue,
			struct mmp_dfc, queue);
		list_del(&dfc->queue);
		switch (dfc->name) {
		case DFC_CHANGE_LCD:
			ret = dynamic_change_lcd(ctrl, dfc);
			break;
		case DFC_DISABLE_DSIPLL:
		case DFC_ENABLE_DSIPLL:
		case DFC_CHANGE_DSIPLL:
		case DFC_RESTORE_DSIPLL:
			dynamic_change_dsipll(dfc->dsi_rate / MHZ_TO_HZ, dfc->name);
			break;
		default:
			break;
		}
		kfree(dfc);
	}

	return ret;
}

static void dfc_handle(void *data)
{
	u32 temp;

	struct mmphw_ctrl *ctrl = (struct mmphw_ctrl *)data;

	if (atomic_read(&ctrl->dfc.commit)) {
		/* disable lcd&dsi sclk firstly */
		temp = readl_relaxed(ctrl->dfc.sclk_reg);
		temp |= 0x1<<28;
		temp &= ~(0xf<<8);
		writel_relaxed(temp, ctrl->dfc.sclk_reg);
		/* change the source */
		writel_relaxed(ctrl->dfc.apmu_value, ctrl->dfc.apmu_reg);
		temp = (ctrl->dfc.sclk_value | (0x1<<28));
		writel_relaxed(ctrl->dfc.sclk_value, ctrl->dfc.sclk_reg);

		sclk_div = ctrl->dfc.sclk_value;
		/* enable lcd&dsi sclk end */
		temp = readl_relaxed(ctrl->dfc.sclk_reg);
		temp &= ~(0x1<<28);
		writel_relaxed(temp, ctrl->dfc.sclk_reg);
		atomic_set(&ctrl->dfc.commit, 0);

	}
}

static int dfc_set_rate_no_reparent(struct mmphw_ctrl *ctrl, unsigned int rate)
{
	struct mmp_path *path = ctrl->path_plats[0].path;
	struct mmp_dfc *dfc = &ctrl->dfc;
	struct mmp_dsi *dsi = mmp_path_to_dsi(path);
	struct parent_index source;
	int count = 0;
	struct clk *dsi_clk = dsi->clk;
	unsigned long flags;
	int ret = 0;

	dfc->dsi_rate = rate * MHZ_TO_HZ;
	dfc->path_rate = set_pixel_clk(dsi, dfc->dsi_rate);

	memset(&source, 0, sizeof(source));
	get_current_parent_index(dsi_clk, &source);
	dfc->parent0 = clk_parent0;
	dfc->current_parent1 = parent1_clk_tbl[source.parent1-1];
	dfc->current_parent2 = parent2_clk_tbl[source.parent2-1];
	dfc->parent1 = dfc->current_parent1;
	dfc->parent2 = dfc->current_parent2;

	calculate_reg_value(ctrl);
	if (path->status) {
		atomic_set(&ctrl->dfc.commit, 1);
		do {
			mmp_wait_vsync(&dsi->special_vsync);
			count++;
		} while (atomic_read(&ctrl->dfc.commit) && (count < DFC_RETRY_TIMEOUT));
	} else {
		writel_relaxed(ctrl->dfc.apmu_value, ctrl->dfc.apmu_reg);
		writel_relaxed(ctrl->dfc.sclk_value, ctrl->dfc.sclk_reg);
		sclk_div = ctrl->dfc.sclk_value;
	}

	spin_lock_irqsave(&ctrl->dfc.lock, flags);
	if (unlikely(count == DFC_RETRY_TIMEOUT)
		&& atomic_read(&ctrl->dfc.commit)) {
		atomic_set(&ctrl->dfc.commit, 0);
		ret = -EFAULT;
	}
	spin_unlock_irqrestore(&ctrl->dfc.lock, flags);

	return ret;
}

static int dfc_set_rate(struct mmp_path *path, unsigned long rate, unsigned long req)
{
	struct mmphw_ctrl *ctrl = path_to_ctrl(path);
	int ret;

	mutex_lock(&ctrl->access_ok);

	pr_info("%s: req:%s, rate:%ld\n", __func__,
			(req < ARRAY_SIZE(dip_req_names)) ?
			dip_req_names[req] : "Unknown", rate);
	/*
	 * record the dip channel request
	 */
	if (req == DIP_START || req == DIP_END)
		current_dip_status = req;
	/*
	 * If dip start has been working, others request won't
	 * work until dip end works. so DIP_START is first priority;
	 * DIP_END is the second priority, other requests is lower
	 * priority.
	 */
	if (current_dip_status == DIP_START && req != DIP_START) {
		dev_dbg(path->dev, "Dip channel is working, ignore other request!\n");
		mutex_unlock(&ctrl->access_ok);
		return 0;
	}

	if (!path->status) {
		/*
		 * FIXME: enable hclk to do register operation.
		 * TODO: Better use power domain on off
		 */
		clk_prepare_enable(hclk);
		writel_relaxed(ctrl->regs_store[LCD_SCLK_DIV/4],
				ctrl->reg_base + LCD_SCLK_DIV);
	}

	if (req == GC_REQUEST) {
		ret = dfc_set_rate_no_reparent(ctrl, rate);
		if (ret < 0) {
			pr_err("dfc prepare failure\n");
			mutex_unlock(&ctrl->access_ok);
			return -1;
		}
	} else {
		ret = dfc_prepare(ctrl, rate);
		if (ret < 0) {
			pr_err("dfc prepare failure\n");
			mutex_unlock(&ctrl->access_ok);
			return -1;
		}
		ret = dfc_commit(ctrl);
		if (ret < 0) {
			pr_err("dfc commit failure\n");
			mutex_unlock(&ctrl->access_ok);
			return -1;
		}
	}
	if (!path->status) {
		ctrl->regs_store[LCD_SCLK_DIV/4] = readl_relaxed(ctrl->reg_base + LCD_SCLK_DIV);
		clk_disable_unprepare(hclk);
	}

	mutex_unlock(&ctrl->access_ok);
	return 0;
}

static unsigned long dfc_get_rate(struct mmp_path *path)
{
	struct mmphw_ctrl *ctrl = path_to_ctrl(path);
	unsigned long rate;
	int val;

	mutex_lock(&ctrl->access_ok);
	if (!path->status) {
		clk_prepare_enable(hclk);
		writel_relaxed(ctrl->regs_store[LCD_SCLK_DIV/4],
				ctrl->reg_base + LCD_SCLK_DIV);
	}

	/*
	 * Show mipi dsi rate in sprintf buffer so that
	 * upper side can get this rate directly by read
	 * sysfs node.
	 */
	rate = clk_get_rate(clk_parent0);
	val = sclk_div;
	val &= DSI1_BITCLK_DIV_MASK;
	val = val >> 8;
	rate = rate / val / MHZ_TO_HZ;

	if (!path->status) {
		ctrl->regs_store[LCD_SCLK_DIV/4] = readl_relaxed(ctrl->reg_base + LCD_SCLK_DIV);
		clk_disable_unprepare(hclk);
	}
	mutex_unlock(&ctrl->access_ok);
	return rate;
}

int dfc_request(struct notifier_block *b, unsigned long val, void *v)
{
	struct mmp_path *path = mmp_get_path("mmp_pnpath");
	u32 rate;
	int ret;

	if (val == DIP_START) {
		rate = *((u32 *)v);
		if (rate == 0) {
			pr_err("LCD DFC Failed: The rate is 0\n");
			return NOTIFY_DONE;
		}
	} else {
		rate = path->original_rate;
		/*
		 * For example, if orginal rate is from PLL4, whose
		 * rate is 450.66666M, the value from clk_get is 450M,
		 * then when DIP_END comes, it will set rate 450M, which
		 * will cause set rate to 442M, rather than 450.6666M.
		 * To fix the float issue, we need to round up the orignal
		 * rate, it ensure come back to the right rate for original
		 * rate from PLL1 and PLL4.
		 */
		rate = rate + 1;
	}

	pr_info("dfc_request: val = %ld, rate = %d\n", val, rate);
	ret = dfc_set_rate(path, (unsigned long)rate, val);
	if (ret < 0)
		return NOTIFY_BAD;

	return NOTIFY_OK;
}

static struct notifier_block dip_disp_notifier = {
	.notifier_call = dfc_request,
};

void dfc_gc_request(unsigned long val, unsigned int fps)
{
	struct mmp_path *path = mmp_get_path("mmp_pnpath");

	path->rate = path->original_rate * fps / 60;
	queue_work(path->vsync.dfc_wq, &path->vsync.dfc_work);

	return;
}
EXPORT_SYMBOL(dfc_gc_request);

ssize_t freq_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct mmphw_ctrl *ctrl = dev_get_drvdata(dev);
	struct mmp_path *path = ctrl->path_plats[0].path;
	struct clk *path_clk = path_to_path_plat(path)->clk;
	struct clk *clk;
	unsigned long rate;
	int s = 0;

	clk = path_clk;
	while (!IS_ERR_OR_NULL(clk)) {
		rate = clk_get_rate(clk);
		dev_dbg(path->dev, "current clk parent : %s ...num:%d...rate: %lu..count : %d.\n",
			clk->name, clk->num_parents, rate, clk->enable_count);
		clk = clk_get_parent(clk);
	}

	rate = dfc_get_rate(path);
	s += sprintf(buf + s, "%ld\n", rate);

	return s;
}

static ssize_t freq_store(
		struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct mmphw_ctrl *ctrl = dev_get_drvdata(dev);
	struct mmp_path *path = ctrl->path_plats[0].path;
	unsigned long rate;
	int ret;

	if (size > 30) {
		pr_err("%s size = %zd > max 30 chars\n", __func__, size);
		return size;
	}

	ret = (int)kstrtoul(buf, 0, &rate);
	if (ret < 0) {
		dev_err(dev, "strtoul err.\n");
		return ret;
	}

	if (rate == 0) {
		pr_err("LCD DFC Failed: The rate is 0\n");
		return -1;
	}
	ret = dfc_set_rate(path, rate, LCD_FREQ);
	if (ret < 0)
		return ret;

	return size;
}
DEVICE_ATTR(freq, S_IRUGO | S_IWUSR, freq_show, freq_store);

void mmp_register_dfc_handler(struct device *dev,
	struct mmp_vsync *vsync)
{
	struct mmp_vsync_notifier_node *dfc_notifier_node1 = NULL;
	struct mmp_path *path = NULL;
	struct mmphw_ctrl *ctrl = NULL;

	if (vsync->type == DSI_SPECIAL_VSYNC) {
		struct mmp_dsi *dsi = vsync->dsi;
		path = mmp_get_path(dsi->plat_path_name);
	} else if (vsync->type == LCD_SPECIAL_VSYNC)
		path = vsync->path;
	else
		BUG();

	ctrl = path_to_ctrl(path);

	dfc_notifier_node1 = devm_kzalloc(dev, sizeof(*dfc_notifier_node1), GFP_KERNEL);
	if (dfc_notifier_node1 == NULL) {
		pr_err("dfc alloc failure\n");
		return;
	}

	dfc_notifier_node1->cb_notify = dfc_handle;
	dfc_notifier_node1->cb_data = ctrl;
	mmp_register_vsync_cb(vsync, dfc_notifier_node1);
}

void mmp_dfc_init(struct device *dev)
{
	struct mmphw_ctrl *ctrl = dev_get_drvdata(dev);
	struct mmp_path *path = ctrl->path_plats[0].path;
	struct clk *path_clk = path_to_path_plat(path)->clk;
	unsigned int apmu_lcd;
	int count, i;
	struct device_node *np = of_find_node_by_name(NULL, "disp_apmu");
	const char *clk_name = NULL;

	if (np && IS_ENABLED(CONFIG_OF)) {
		if (of_property_read_u32(np, "apmu-reg", &apmu_lcd)) {
			pr_err("%s: No apmu_reg in dts\n", __func__);
			return;
		}
		if (of_property_read_u32(np, "clksrc-bit", &lcd_clk_en)) {
			pr_err("%s: No clksrc choose bit in dts\n", __func__);
			return;
		}

		if (of_property_read_u32(np, "plat", &plat)) {
			pr_err("%s: No plat id in dts\n", __func__);
			return;
		}
		count = of_property_count_strings(np, "parent1-clk-tbl");
		if (count < 0) {
			pr_err("%s: parent1_names property missing or empty\n", __func__);
			return;
		}
		for (i = 0; i < count; i++) {
			if (of_property_read_string_index(np, "parent1-clk-tbl", i, &clk_name))
				continue;
			parent1_clk_tbl[i] = devm_clk_get(dev, clk_name);
		}

		parent2_count = of_property_count_strings(np, "parent2-clk-tbl");
		if (parent2_count < 0) {
			pr_err("%s: parent2_names property missing or empty\n", __func__);
			return;
		}
		for (i = 0; i < parent2_count; i++) {
			if (of_property_read_string_index(np, "parent2-clk-tbl", i, &clk_name))
				continue;
			parent2_clk_tbl[i] = devm_clk_get(dev, clk_name);
		}
	} else {
		pr_err("%s: dts is not suport, or disp_apmu_ver missing\n", __func__);
		return;
	}

	create_parent_tbl(dev, path_clk);
	atomic_set(&ctrl->dfc.commit, 0);
	spin_lock_init(&ctrl->dfc.lock);
	INIT_LIST_HEAD(&ctrl->dfc_list.queue);

	ctrl->dfc.apmu_reg = ioremap(apmu_lcd , 4);
	ctrl->dfc.sclk_reg = ctrl->reg_base + LCD_PN_SCLK;

	sclk_div = readl_relaxed(ctrl->dfc.sclk_reg);
	clk_parent0 = devm_clk_get(dev, parent0_clk_tbl[0]);

	/*
	 * When other platforms add dedicate dsi pll like pll3,
	 * please add "if (plat == PXAxxx)" to update dsipll;
	 */
	dsipll = devm_clk_get(dev, "pll4");
	dsipll_vco = devm_clk_get(dev, "pll4_vco");
	hclk = devm_clk_get(dev, "LCDCIHCLK");

	dsipll_vco_default = clk_get_rate(dsipll_vco);
	dsipll_default = clk_get_rate(dsipll);

	device_create_file(dev, &dev_attr_freq);

	path->ops.set_dfc_rate = dfc_set_rate;
	path->ops.get_dfc_rate = dfc_get_rate;

	dip_register_notifier(&dip_disp_notifier, 0);
}
