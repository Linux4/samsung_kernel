// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/* temporary solution: Do not use these sysfs as official purpose */
/* these function are not official one. only purpose is for temporary test */

#if defined(CONFIG_DEBUG_FS) && !defined(CONFIG_SAMSUNG_PRODUCT_SHIP) && defined(CONFIG_SMCDSD_LCD_DEBUG)
#include <linux/of.h>

#include "panel.h"
#include "panel_dimming.h"
#include "panel_drv.h"

//#define dbg_none(fmt, ...)		pr_info(pr_fmt("%s: %3d: %s: " fmt), "lcd panel", __LINE__, __func__, ##__VA_ARGS__)
#define dbg_none(fmt, ...)
#define dbg_info(fmt, ...)		pr_info(pr_fmt("%s: %3d: %s: " fmt), "lcd panel", __LINE__, __func__, ##__VA_ARGS__)
#define dbg_warn(fmt, ...)		pr_warn(pr_fmt("%s: %3d: %s: " fmt), "lcd panel", __LINE__, __func__, ##__VA_ARGS__)

#define PANEL_LUT_NAME			"panel-lut"

static void copy_assist_maptbl(struct maptbl *tbl, u8 *dst)
{
	int idx;
	struct panel_device *panel = NULL;
	struct panel_info *panel_data = NULL;
	struct panel_dimming_info *panel_dim_info = NULL;
	int x, y;

	if (!tbl || !dst) {
		dbg_warn("invalid parameter (tbl %pK, dst %pK\n",
				tbl, dst);
		return;
	}

	panel = (struct panel_device *)tbl->pdata;
	panel_data = &panel->panel_data;
	panel_dim_info = panel_data->panel_dim_info[SUBDEV_INDEX];

	idx = maptbl_getidx(tbl);
	if (idx < 0) {
		dbg_warn("failed to getidx %d\n", idx);
		return;
	}

	for_each_col(tbl, x) {
		if (!panel_dim_info->brightness_func[CONVERT_DBV_TO_REG])
			break;

		y = idx / sizeof_row(tbl);

		dbg_none("[%3d][%3d][%d] %4d %2x -> %2x\n", idx, y, x, panel_dim_info->brightness_code[y], tbl->arr[idx + x],
			panel_dim_info->brightness_func[CONVERT_DBV_TO_REG](panel_dim_info->brightness_code[y], x));
		tbl->arr[idx + x] = panel_dim_info->brightness_func[CONVERT_DBV_TO_REG](panel_dim_info->brightness_code[y], x);
	}

	memcpy(dst, &(tbl->arr)[idx], sizeof(u8) * tbl->sz_copy);
#ifdef DEBUG_PANEL
	panel_dbg("%s copy from %s %d %d\n",
			__func__, tbl->name, idx, tbl->sz_copy);
	print_data(dst, tbl->sz_copy);
#endif
}

static int of_find_common_panel_list(struct panel_device *panel, struct common_panel_info **panel_table)
{
	struct device_node *parent = NULL;
	int lut_count, ret = 0;
	u32 *lut_table = NULL;
	u32 lut_index, id, mask, index, ddi_index = 0;
	int panel_index, panel_count = 0;
	struct common_panel_info *info = NULL;

	parent = of_find_node_with_property(NULL, PANEL_LUT_NAME);
	if (!parent) {
		dbg_warn("%s property does not exist so skip\n", PANEL_LUT_NAME);
		return -EINVAL;
	}

	lut_count = of_property_count_u32_elems(parent, PANEL_LUT_NAME);
	if (lut_count <= 0 || lut_count % 4 || lut_count >= U8_MAX) {
		dbg_warn("%s property has invalid count(%d)\n", PANEL_LUT_NAME, lut_count);
		return -EINVAL;
	}

	lut_table = kcalloc(lut_count, sizeof(u32), GFP_KERNEL);
	if (!lut_table) {
		dbg_warn("%s property kcalloc fail\n", PANEL_LUT_NAME);
		return -EINVAL;
	}

	ret = of_property_read_u32_array(parent, PANEL_LUT_NAME, lut_table, lut_count);
	if (ret < 0) {
		dbg_warn("%s of_property_read_u32_array fail. ret(%d)\n", PANEL_LUT_NAME, ret);
		kfree(lut_table);
		return -EINVAL;
	}

	for (lut_index = 0; lut_index < lut_count; lut_index += 4) {
		id = lut_table[lut_index + 0];
		mask = lut_table[lut_index + 1];
		index = lut_table[lut_index + 2];
		ddi_index = lut_table[lut_index + 3];

		info = find_panel(panel, id);
		if (!info)
			continue;

		for (panel_index = 0; panel_index < panel_count; panel_index++) {
			if (!strcmp(panel_table[panel_index]->name, info->name) &&
					panel_table[panel_index]->id == info->id &&
					panel_table[panel_index]->rev == info->rev) {
				dbg_none("already exist panel (%s id-0x%06X rev-%d)\n",
					info->name, info->id, info->rev);
				continue;
			}
		}

		panel_table[panel_count++] = info;

		BUG_ON(panel_count == MAX_PANEL);
	}

	kfree(lut_table);

	return panel_count;
}

static int check_brightness_maptbl(struct maptbl *map)
{
	int x, y, sum;

	for_each_row(map, y) {
		sum = 0;

		for_each_col(map, x) {
			dbg_none("[%2d][%2d] %2x\n", y, x, map->arr[maptbl_index(map, 0, y, 0) + x]);
			sum += map->arr[maptbl_index(map, 0, y, 0) + x];
		}

		if (sum) {
			dbg_none("[%2d][%2d] %4d\n", y, x, sum);
			break;
		}
	}

	return sum;
}

static int check_brightness_code(unsigned int *code, int max)
{
	int y;

	for (y = 0; y < max && &code[y]; y++) {
		if (code[y])
			return code[y];
	}

	return 0;
}

static int check_common_panel_brightness_info(struct panel_device *panel)
{
	struct common_panel_info *panel_table[MAX_PANEL] = {0, };
	int panel_count = of_find_common_panel_list(panel, panel_table);
	struct seqinfo *seq = NULL;
	struct maptbl *map = NULL;
	int i, count = 0;
	struct panel_dimming_info *dim = NULL;
	int reg2dbv, dbv2reg;

	if (panel_count <= 0)
		return panel_count;

	for (i = 0; i < panel_count; i++) {
		if (!panel_table[i]->seqtbl) {
			dbg_info("\n");
			continue;
		}

		if (panel_table[i]->nr_seqtbl <= SEQTBL_INDEX) {
			dbg_info("\n");
			continue;
		}

		seq = &panel_table[i]->seqtbl[SEQTBL_INDEX];
		dim = panel_table[i]->panel_dim_info[SUBDEV_INDEX];

		if (!seq) {
			dbg_info("\n");
			continue;
		}

		if (!dim) {
			dbg_info("\n");
			continue;
		}

		map = find_brightness_maptbl(seq); {
		if (!map)
			dbg_info("\n");
			continue;
		}

		reg2dbv = check_brightness_maptbl(map);
		dbv2reg = dim->brightness_code ? check_brightness_code(dim->brightness_code, map->nrow) : 0;

		dbg_info("%s reg2dbv(%d) dbv2reg(%d)\n", panel_table[i]->name, reg2dbv, dbv2reg);

		if (reg2dbv && dbv2reg) {
			dbg_info("please del brt_maptbl or brightness_code\n", panel_table[i]->name);
			BUG();
		}

		if (!reg2dbv && !dbv2reg) {
			dbg_info("please add brt_maptbl or brightness_code\n", panel_table[i]->name);
			BUG();
		}

		if (dim->brightness_func[CONVERT_DBV_TO_REG] && !dim->brightness_func[CONVERT_DBV_TO_REG]) {
			dbg_info("please add brightness function pointer to convert dbv to reg\n", panel_table[i]->name);
			BUG();
		}

		if (!dim->brightness_func[CONVERT_DBV_TO_REG] && dim->brightness_func[CONVERT_DBV_TO_REG]) {
			dbg_info("please add brightness function pointer to convert reg to dbv\n", panel_table[i]->name);
			BUG();
		}

		if (map->ncol != 1 && !dim->brightness_func[CONVERT_DBV_TO_REG] && !dim->brightness_func[CONVERT_DBV_TO_REG]) {
			dbg_info("please add brightness function pointer to adjust ncol(%d)\n", map->ncol);
			BUG();
		}

		count++;
	}

	return count;
}

void init_brightness_assist(struct panel_device *panel)
{
	struct panel_info *panel_data = &panel->panel_data;
	struct panel_dimming_info *dim = NULL;
	struct seqinfo *seq = NULL;
	struct maptbl *map = NULL;
	int reg2dbv, dbv2reg;
	int x, y;

	check_common_panel_brightness_info(panel);

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

	map = find_brightness_maptbl(seq);
	if (!map) {
		dbg_info("\n");
		return;
	}

	reg2dbv = check_brightness_maptbl(map);
	dbv2reg = dim->brightness_code ? check_brightness_code(dim->brightness_code, map->nrow) : 0;

	if (map->ncol == 1 && !dim->brightness_func[CONVERT_DBV_TO_REG] && !dim->brightness_func[CONVERT_REG_TO_DBV]) {
		dim->brightness_func[CONVERT_DBV_TO_REG] = common_dbv_to_reg;
		dim->brightness_func[CONVERT_REG_TO_DBV] = common_reg_to_dbv;
	}

	if (!dim->brightness_func[CONVERT_DBV_TO_REG] || !dim->brightness_func[CONVERT_REG_TO_DBV]) {
		dbg_info("\n");
		return;
	}

	if (!dim->brightness_code)
		dim->brightness_code = kcalloc(map->nrow, sizeof(unsigned int), GFP_KERNEL);

	for_each_row(map, y) {
		for_each_col(map, x) {
			if (reg2dbv) {
				dim->brightness_code[y] = (y == 0) ? 0 : dim->brightness_code[y];
				dim->brightness_code[y] |= dim->brightness_func[CONVERT_REG_TO_DBV](map->arr[maptbl_index(map, 0, y, 0) + x], x);
			} else {
				map->arr[maptbl_index(map, 0, y, 0) + x] = dim->brightness_func[CONVERT_DBV_TO_REG](dim->brightness_code[y], x);
			}
		}
	}

	map->ops.copy = copy_assist_maptbl;

	init_debugfs_param("brt_maptbl", map->arr, U8_MAX, map->nrow * map->ncol, map->ncol);

if (0)
{
	char print_buf[80] = {0, };
	struct seq_file m = {
		.buf = print_buf,
		.size = sizeof(print_buf) - 1,
	};

	for_each_row(map, y) {
		m.count = 0;
		memset(print_buf, 0, sizeof(print_buf));
		for_each_col(map, x) {
			seq_printf(&m, "0x%02X, ", dim->brightness_func[CONVERT_DBV_TO_REG](dim->brightness_code[y], x));
		}

		dbg_none("[%3d] %4d, {%s},\n", y, dim->brightness_code[y], m.buf);
	}
}
}

static void init_debugfs_i2c_client(struct panel_device *panel)
{
#ifdef CONFIG_SUPPORT_I2C
	struct panel_info *panel_data = &panel->panel_data;
	struct panel_i2c_dev *i2c_dev = &panel->panel_i2c_dev;
	struct seqinfo *seq = NULL;
	struct pktinfo *pkt = NULL;
	struct i2c_client *clients[] = {i2c_dev->client, NULL};
	char *debugfs_name;

	int i, j;

	for (i = PANEL_I2C_INDEX_START; i <= PANEL_I2C_INDEX_END; i++) {
		if (i != PANEL_I2C_INIT_SEQ && i != PANEL_I2C_EXIT_SEQ)
			continue;

		seq = &panel_data->seqtbl[i];
		for (j = 0; j < seq->size; j++) {
			pkt = seq->cmdtbl[j];
			if (pkt->type != I2C_PKT_TYPE_WR)
				continue;

			dbg_none("seqname(%s) size(%d) type(%d)\n", seq->name, seq->size, seq->type);
			dbg_none("pktname(%s) size(%d) type(%d)\n", pkt->name, pkt->dlen, pkt->type);
			dbg_none("%*ph\n", (pkt->dlen > U8_MAX) ? U8_MAX : pkt->dlen, pkt->data);

			debugfs_name = (i == PANEL_I2C_INIT_SEQ) ? "blic_init" : "blic_exit";

			init_debugfs_param(debugfs_name, pkt->data, U8_MAX, pkt->dlen, 2);
		}
	}

	init_debugfs_backlight(NULL, NULL, clients);
#endif
}

void init_debugfs_brightness(struct panel_device *panel)
{
	struct panel_info *panel_data = &panel->panel_data;
	struct panel_bl_device *panel_bl = &panel->panel_bl;
	struct backlight_device *bd = panel_bl->bd;
	struct panel_dimming_info *dim = panel_data->panel_dim_info[SUBDEV_INDEX];

	init_debugfs_backlight(bd, dim->brightness_code, NULL);

	init_debugfs_i2c_client(panel);
}
#endif

