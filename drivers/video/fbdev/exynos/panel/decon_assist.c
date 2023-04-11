// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "decon_board.h"

#include "panel.h"
#include "panel_dimming.h"
#include "panel_drv.h"

#include "dd.h"

//#define dbg_none(fmt, ...)		pr_info(pr_fmt("%s: %3d: %s: " fmt), "lcd panel", __LINE__, __func__, ##__VA_ARGS__)
#define dbg_none(fmt, ...)
#define dbg_info(fmt, ...)		pr_info(pr_fmt("%s: %3d: %s: " fmt), "lcd panel", __LINE__, __func__, ##__VA_ARGS__)
#define dbg_warn(fmt, ...)		pr_warn(pr_fmt("%s: %3d: %s: " fmt), "lcd panel", __LINE__, __func__, ##__VA_ARGS__)

unsigned int common_dbv_to_reg(unsigned int value, unsigned int index) { return value; };
unsigned int common_reg_to_dbv(unsigned int value, unsigned int index) { return value; };

struct maptbl *find_brightness_maptbl(struct seqinfo *seq)
{
	struct pktinfo *pkt = NULL;
	struct maptbl *map = NULL;
	char *pktname;
	int i;

	dbg_none("seqname(%s) size(%d) type(%d)\n", seq->name, seq->size, seq->type);

	for (i = 0; i < seq->size; i++) {
		pkt = seq->cmdtbl[i];

		dbg_none("pktname(%s) size(%d) type(%d)\n", pkt->name, pkt->dlen, pkt->type);

		if (pkt->type != DSI_PKT_TYPE_WR) {
			dbg_info("\n");
			continue;
		}

		if (!pkt->data) {
			dbg_info("\n");
			continue;
		}

		if (pkt->data[0] != MIPI_DCS_SET_DISPLAY_BRIGHTNESS) {
			dbg_info("\n");
			continue;
		}

		pktname = strstr(pkt->name, "brightness");
		if (!pktname || strlen(pktname) != strlen("brightness")) {
			dbg_info("\n");
			continue;
		}

		if (!pkt->pktui) {
			dbg_info("\n");
			continue;
		}

		if (!pkt->pktui->maptbl) {
			dbg_info("\n");
			continue;
		}

		if (pkt->pktui->nr_maptbl != 1) {
			dbg_info("\n");
			continue;
		}

		if (pkt->pktui->maptbl->nbox != 1) {
			dbg_info("\n");
			continue;
		}

		if (pkt->pktui->maptbl->nlayer != 1) {
			dbg_info("\n");
			continue;
		}

		if (pkt->pktui->maptbl->nrow == 0) {
			dbg_info("\n");
			continue;
		}

		if (pkt->pktui->maptbl->ncol == 0) {
			dbg_info("\n");
			continue;
		}

		map = pkt->pktui->maptbl;

		dbg_info("mapname(%s) nrow(%d) ncol(%d) type(%d)\n", map->name, map->nrow, map->ncol, map->type);

		//print_maptbl(map);

		break;
	}

	return map;
}

static void init_brightness_maptbl(struct panel_device *panel)
{
	struct panel_info *panel_data = &panel->panel_data;
	struct panel_dimming_info *dim = NULL;
	struct seqinfo *seq = NULL;
	struct maptbl *map = NULL;
	int x, y;

	seq = &panel_data->seqtbl[SEQTBL_INDEX];
	if (!seq) {
		dbg_info("\n");
		return;
	}

	dim = panel_data->panel_dim_info[SUBDEV_INDEX];
	if (!dim) {
		dbg_info("\n");
		return;
	}

	if (!dim->brightness_code) {
		dbg_info("\n");
		return;
	}

	map = find_brightness_maptbl(seq);
	if (!map) {
		dbg_info("\n");
		return;
	}

	if (map->ncol == 1 && !dim->brightness_func[CONVERT_DBV_TO_REG] && !dim->brightness_func[CONVERT_REG_TO_DBV]) {
		dim->brightness_func[CONVERT_DBV_TO_REG] = common_dbv_to_reg;
		dim->brightness_func[CONVERT_REG_TO_DBV] = common_reg_to_dbv;
	}

	if (!dim->brightness_func[CONVERT_DBV_TO_REG] || !dim->brightness_func[CONVERT_REG_TO_DBV]) {
		dbg_info("\n");
		return;
	}

	for_each_row(map, y) {
		for_each_col(map, x) {
			map->arr[maptbl_index(map, 0, y, 0) + x] = dim->brightness_func[CONVERT_DBV_TO_REG](dim->brightness_code[y], x);
		}
	}
}

void decon_assist_probe(struct panel_device *panel)
{
	struct panel_info *panel_data = &panel->panel_data;

	if (!panel_data->maptbl) {
		dbg_info("\n");
		return;
	}

	if (panel_data->nr_maptbl > SUBDEV_INDEX + 1) {
		dbg_info("\n");
		return;
	}

	if (IS_ENABLED(CONFIG_SUPPORT_INDISPLAY))
		return;

	if (IS_ENABLED(CONFIG_SUPPORT_AOD_BL))
		return;

	if (IS_ENABLED(CONFIG_EXYNOS_DECON_MDNIE))
		return;

	init_brightness_assist(panel);

	init_brightness_maptbl(panel);

	init_debugfs_brightness(panel);
}

int __init decon_assist_init(void)
{
	struct platform_device *pdev = NULL;
	struct panel_device *pdata = NULL;

	pdev = of_find_device_by_path("/panel_drv@0");
	if (!pdev) {
		dbg_info("of_find_device_by_path fail\n");
		return 0;
	}

	pdata = platform_get_drvdata(pdev);
	if (!pdata) {
		dbg_info("platform_get_drvdata fail\n");
		return 0;
	}

	decon_assist_probe(pdata);

	return 0;
}
//late_initcall_sync(decon_assist_init);

