// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <exynos-is.h>

#include "is-vendor.h"
#include "is-dt.h"
#include "is-core.h"
#include "pablo-kunit-test.h"

static struct pablo_dt_test_ctx {
	struct device_node dnode;
	struct is_dvfs_ctrl dvfs_ctrl;
	struct platform_device dev;
	struct exynos_platform_is_module module_data;
} pkt_ctx;

/* Define the test cases. */
static struct pkt_dt_ops *pkt_ops;
static void pablo_set_prop(struct property *prop, char *name, u32 len, u32 *value)
{
	prop[0].name = name;
	prop[0].length = len;
	prop[0].value = value;
}

static void pablo_set_node(struct device_node *node, char *name, struct property *prop)
{
	node[0].name = name;
	node[0].full_name = name;
	node[0].properties = prop;
}

static void pablo_dt_parse_qos_table_negative_kunit_test(struct kunit *test)
{
	struct device_node *np = &pkt_ctx.dnode;
	struct device_node child;
	struct is_dvfs_ctrl *dvfs_ctrl = &pkt_ctx.dvfs_ctrl;
	int test_result;
	struct property *prop;
	const int prop_count = 2;
	u32 value = 0x02000000;

	prop = kunit_kzalloc(test, sizeof(struct property) * prop_count, GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, prop);

	pablo_set_prop(&prop[0], "typ", 4, &value);
	pablo_set_prop(&prop[1], "lev", 80, &value);

	/* TC : dvfs_typ read is fail */
	np->child = &child;
	test_result = pkt_ops->parse_qos_table(dvfs_ctrl, np);
	KUNIT_EXPECT_EQ(test, test_result, -EINVAL);

	/* TC : too many elements */
	prop[0].next = &prop[1];
	child.properties = prop;
	test_result = pkt_ops->parse_qos_table(dvfs_ctrl, np);
	KUNIT_EXPECT_EQ(test, test_result, 0);

	kunit_kfree(test, prop);
}

static void pablo_dt_chain_dev_parse_negative_kunit_test(struct kunit *test)
{
	struct device_node *node = &pkt_ctx.dnode;
	struct platform_device *pdev = &pkt_ctx.dev;
	int test_result;
	struct is_core *core;
	struct property *prop;
	const int prop_count = 2;
	u32 value[] = { 0x04000000, 0x38B00000 };

	prop = kunit_kzalloc(test, sizeof(struct property) * prop_count, GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, prop);

	pablo_set_prop(&prop[0], "secure_mem_info", 20, value);
	pablo_set_prop(&prop[1], "non_secure_mem_info", 28, value);

	node->name = "is_qos";
	/* TC : core is null */
	test_result = pkt_ops->chain_dev_parse(pdev);
	KUNIT_EXPECT_EQ(test, test_result, -ENOMEM);

	/* TC : secure_mem_info & non_secure_mem_info are not found */
	pdev->dev.driver_data =
		(struct device *)kunit_kzalloc(test, sizeof(struct is_core), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, pdev->dev.driver_data);
	pdev->dev.of_node = node;
	pdev->dev.of_node->properties = &prop[0];
	prop[0].next = &prop[1];
	test_result = pkt_ops->chain_dev_parse(pdev);

	core = dev_get_drvdata(&pdev->dev);
	KUNIT_EXPECT_EQ(test, (u32)core->secure_mem_info[0], be32_to_cpup(&value[0]));
	KUNIT_EXPECT_EQ(test, (u32)core->secure_mem_info[1], be32_to_cpup(&value[1]));
	KUNIT_EXPECT_EQ(test, (u32)core->non_secure_mem_info[0], be32_to_cpup(&value[0]));
	KUNIT_EXPECT_EQ(test, (u32)core->non_secure_mem_info[1], be32_to_cpup(&value[1]));
	KUNIT_EXPECT_EQ(test, test_result, 0);

	kunit_kfree(test, pdev->dev.driver_data);
	kunit_kfree(test, prop);
}

static void pablo_dt_sensor_dev_parse_negative_kunit_test(struct kunit *test)
{
	struct platform_device *pdev = &pkt_ctx.dev;
	int test_result;
	const int prop_count = 3;
	struct property *prop;
	char name[][10] = { "id", "scenario", "csi_ch" };
	u32 value = 0x02000000;
	int i;

	prop = kunit_kzalloc(test, sizeof(struct property) * prop_count, GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, prop);

	for (i = 0; i < prop_count; i++)
		pablo_set_prop(&prop[i], name[i], 4, &value);

	/* TC : id is not found */
	pdev->dev.of_node = &pkt_ctx.dnode;
	test_result = pkt_ops->sensor_dev_parse(pdev);
	KUNIT_EXPECT_EQ(test, test_result, -EINVAL);

	/* TC : scenario is not found */
	pdev->dev.of_node->properties = &prop[0];
	test_result = pkt_ops->sensor_dev_parse(pdev);
	KUNIT_EXPECT_EQ(test, test_result, -EINVAL);

	/* TC : csi_ch is not found */
	prop[0].next = &prop[1];
	test_result = pkt_ops->sensor_dev_parse(pdev);
	KUNIT_EXPECT_EQ(test, test_result, -EINVAL);

	/* TC : After csi_ch is found */
	prop[1].next = &prop[2];
	test_result = pkt_ops->sensor_dev_parse(pdev);
	KUNIT_EXPECT_EQ(test, test_result, 0);
	KUNIT_EXPECT_EQ(test, pdev->id, (int)be32_to_cpup(&value));

	kunit_kfree(test, prop);
}

static void pablo_dt_board_cfg_parse_kunit_test(struct kunit *test)
{
	struct device_node *np = &pkt_ctx.dnode;
	struct exynos_platform_is_module *pdata = &pkt_ctx.module_data;
	struct exynos_platform_is_module backup;
	int test_result;
	int i;
	enum prop_idx {
		IDX_ID,
		IDX_MCLK_CH,
		IDX_MCLK_FREQ,
		IDX_I3C_CH,
		IDX_POS,
		IDX_MAX,
	};
	struct property *prop;
	char name[IDX_MAX][20] = { "id", "mclk_ch", "mclk_freq", "sensor_i3c_ch", "position" };
	u32 value[] = { 2, 3, 19200, 3, 2 };

	prop = kunit_kzalloc(test, sizeof(struct property) * IDX_MAX, GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, prop);

	for (i = 0; i < IDX_MAX; i++)
		pablo_set_prop(&prop[i], name[i], 4, &value[i]);

	/* TC : id read is fail */
	test_result = pkt_ops->board_cfg_parse(np, pdata);
	KUNIT_EXPECT_EQ(test, test_result, -EINVAL);

	/* TC : mclk_ch read is fail */
	np->properties = &prop[IDX_ID];
	test_result = pkt_ops->board_cfg_parse(np, pdata);
	KUNIT_EXPECT_EQ(test, test_result, -EINVAL);

	/* TC : skipped config */
	backup.mclk_freq_khz = 26000;
	backup.sensor_i2c_ch = pdata->sensor_i2c_ch = 1;
	backup.cis_ixc_type = pdata->cis_ixc_type = 1;
	backup.sensor_i2c_addr = pdata->sensor_i2c_addr = 0x12;
	backup.position = pdata->position = 1;

	prop[IDX_ID].next = &prop[IDX_MCLK_CH];
	test_result = pkt_ops->board_cfg_parse(np, pdata);
	KUNIT_EXPECT_EQ(test, pdata->mclk_freq_khz, backup.mclk_freq_khz);
	KUNIT_EXPECT_EQ(test, pdata->sensor_i2c_ch, backup.sensor_i2c_ch);
	KUNIT_EXPECT_EQ(test, pdata->cis_ixc_type, backup.cis_ixc_type);
	KUNIT_EXPECT_EQ(test, pdata->sensor_i2c_addr, backup.sensor_i2c_addr);
	KUNIT_EXPECT_EQ(test, test_result, -EINVAL);

	/* TC : After checking to read mclk_freq */
	for (i = IDX_MCLK_CH; i < IDX_MAX - 1; i++)
		prop[i].next = &prop[i + 1];
	test_result = pkt_ops->board_cfg_parse(np, pdata);
	KUNIT_EXPECT_EQ(test, pdata->mclk_freq_khz, be32_to_cpup(&value[IDX_MCLK_FREQ]));
	KUNIT_EXPECT_EQ(test, test_result, 0);

	kunit_kfree(test, prop);
}

static void pablo_dt_module_cfg_parse_skipped_kunit_test(struct kunit *test)
{
	struct device_node *np = &pkt_ctx.dnode;
	struct exynos_platform_is_module *pdata = &pkt_ctx.module_data;
	struct exynos_platform_is_module backup;

	backup.sensor_id = pdata->sensor_id = 1;
	backup.active_width = pdata->active_width = 640;
	backup.active_height = pdata->active_height = 480;
	backup.margin_left = pdata->margin_left = 1;
	backup.margin_right = pdata->margin_right = 2;
	backup.margin_top = pdata->margin_top = 3;
	backup.margin_bottom = pdata->margin_bottom = 4;
	backup.max_framerate = pdata->max_framerate = 30;
	backup.bitwidth = pdata->bitwidth = 16;
	backup.use_retention_mode = pdata->use_retention_mode = SENSOR_RETENTION_ACTIVATED;
	backup.use_binning_ratio_table = pdata->use_binning_ratio_table = 0x10;
	backup.sensor_maker = pdata->sensor_maker = "KUNIT";
	backup.sensor_name = pdata->sensor_name = "KUNIT";
	backup.setfile_name = pdata->setfile_name = "KUNIT";
	backup.sensor_module_type = pdata->sensor_module_type = SENSOR_TYPE_MONO;

	pkt_ops->module_cfg_parse(np, pdata);

	KUNIT_EXPECT_EQ(test, pdata->sensor_id, backup.sensor_id);
	KUNIT_EXPECT_EQ(test, pdata->active_width, backup.active_width);
	KUNIT_EXPECT_EQ(test, pdata->active_height, backup.active_height);
	KUNIT_EXPECT_EQ(test, pdata->margin_left, backup.margin_left);
	KUNIT_EXPECT_EQ(test, pdata->margin_right, backup.margin_right);
	KUNIT_EXPECT_EQ(test, pdata->margin_top, backup.margin_top);
	KUNIT_EXPECT_EQ(test, pdata->margin_bottom, backup.margin_bottom);
	KUNIT_EXPECT_EQ(test, pdata->max_framerate, backup.max_framerate);
	KUNIT_EXPECT_EQ(test, pdata->bitwidth, backup.bitwidth);
	KUNIT_EXPECT_EQ(test, pdata->use_retention_mode, backup.use_retention_mode);
	KUNIT_EXPECT_EQ(test, pdata->use_binning_ratio_table, backup.use_binning_ratio_table);
	KUNIT_EXPECT_TRUE(test, !strcmp(pdata->sensor_maker, backup.sensor_maker));
	KUNIT_EXPECT_TRUE(test, !strcmp(pdata->sensor_name, backup.sensor_name));
	KUNIT_EXPECT_TRUE(test, !strcmp(pdata->setfile_name, backup.setfile_name));
	KUNIT_EXPECT_EQ(test, pdata->sensor_module_type, (u32)SENSOR_TYPE_RGB);
}

static void pablo_dt_module_rom_parse_skipped_kunit_test(struct kunit *test)
{
	struct device_node *np = &pkt_ctx.dnode;
	struct exynos_platform_is_module *pdata = &pkt_ctx.module_data;

	pkt_ops->module_rom_parse(np, pdata);
	KUNIT_EXPECT_EQ(test, pdata->rom_id, (u32)ROM_ID_NOTHING);
	KUNIT_EXPECT_EQ(test, pdata->rom_type, (u32)ROM_TYPE_NONE);
	KUNIT_EXPECT_EQ(test, pdata->rom_cal_index, (u32)ROM_CAL_NOTHING);
	KUNIT_EXPECT_EQ(test, pdata->rom_dualcal_id, (u32)ROM_ID_NOTHING);
	KUNIT_EXPECT_EQ(test, pdata->rom_dualcal_index, (u32)ROM_DUALCAL_NOTHING);
}

static void pablo_dt_module_peris_parse_skipped_kunit_test(struct kunit *test)
{
	struct device_node *np = &pkt_ctx.dnode;
	struct exynos_platform_is_module *pdata = &pkt_ctx.module_data;

	pkt_ops->module_peris_parse(np, pdata);
	KUNIT_EXPECT_EQ(test, pdata->af_product_name, (u32)ACTUATOR_NAME_NOTHING);
	KUNIT_EXPECT_EQ(test, pdata->flash_product_name, (u32)FLADRV_NAME_NOTHING);
	KUNIT_EXPECT_EQ(test, pdata->ois_product_name, (u32)OIS_NAME_NOTHING);
	KUNIT_EXPECT_EQ(test, pdata->mcu_product_name, (u32)MCU_NAME_NOTHING);
	KUNIT_EXPECT_EQ(test, pdata->aperture_product_name, (u32)APERTURE_NAME_NOTHING);
	KUNIT_EXPECT_EQ(test, pdata->eeprom_product_name, (u32)EEPROM_NAME_NOTHING);
	KUNIT_EXPECT_EQ(test, pdata->laser_af_product_name, (u32)LASER_AF_NAME_NOTHING);
}

static void pablo_dt_module_peris_parse_kunit_test(struct kunit *test)
{
	struct device_node *np = &pkt_ctx.dnode;
	struct exynos_platform_is_module *pdata = &pkt_ctx.module_data;
	int i;
	struct device_node *child_node;
	char node_name[][20] = { "cis", "af",       "flash",  "ois",
				 "mcu", "aperture", "eeprom", "laser_af" };
	u32 value[] = { /* cis */ 100,  /* af */ 18,     /* flash */ 19,
			/* ois */ 100,  /* mcu */ 2,     /* aperture */ 100,
			/* eeprom */ 1, /* laser_af */ 4 };
	struct property *prop;
	const int node_count = 8;
	u32 *conv_value;

	child_node = kunit_kzalloc(test, sizeof(struct device_node) * node_count, GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, child_node);

	prop = kunit_kzalloc(test, sizeof(struct property) * node_count, GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, prop);

	conv_value = kunit_kzalloc(test, sizeof(u32) * node_count, GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, conv_value);

	for (i = 0; i < node_count; i++) {
		conv_value[i] = cpu_to_be32(value[i]);
		pablo_set_prop(&prop[i], "product_name", 4, &conv_value[i]);
		pablo_set_node(&child_node[i], node_name[i], &prop[i]);
	}

	for (i = 0; i < node_count - 1; i++)
		child_node[i].sibling = &child_node[i + 1];
	np->child = child_node;

	pkt_ops->module_peris_parse(np, pdata);
	KUNIT_EXPECT_EQ(test, pdata->af_product_name, value[1]);
	KUNIT_EXPECT_EQ(test, pdata->flash_product_name, value[2]);
	KUNIT_EXPECT_EQ(test, pdata->ois_product_name, value[3]);
	KUNIT_EXPECT_EQ(test, pdata->mcu_product_name, value[4]);
	KUNIT_EXPECT_EQ(test, pdata->aperture_product_name, value[5]);
	KUNIT_EXPECT_EQ(test, pdata->eeprom_product_name, value[6]);
	KUNIT_EXPECT_EQ(test, pdata->laser_af_product_name, value[7]);

	kunit_kfree(test, conv_value);
	kunit_kfree(test, prop);
	kunit_kfree(test, child_node);
}

static void pablo_dt_cis_opt_parse_kunit_test(struct kunit *test)
{
	struct device_node *np = &pkt_ctx.dnode;
	struct exynos_platform_is_module *pdata = &pkt_ctx.module_data;
	struct is_sensor_cfg *cfg;
	int i;
	struct property *prop;
	enum prop_idx {
		PROP_VOTF,
		PROP_HINT,
		PROP_MXFPS,
		PROP_BINNING,
		PROP_VVALID,
		PROP_REQVVALID,
		PROP_SPEMODE,
		PROP_DVFS,
		PROP_EXMODE,
		PROP_SMODE,
		PROP_LOC,
		PROP_DUMMY,
		PROP_MAX,
	};
	char prop_name[][20] = {
		"votf",		 "wdma_ch_hint",     "max_fps",      "binning",
		"vvalid_time",   "req_vvalid_time",  "special_mode", "dvfs_lv_csis",
		"ex_mode_extra", "stat_sensor_mode",
	};
	struct device_node child_node;
	u32 value[] = {
		1, INIT_WDMA_CH_HINT, 30, 1, 1000000U, 1000000U, 0, 10, 0, VC_SENSOR_MODE_2PD_MODE1,
	};
	u32 loc_value[] = { 0, 27 };

	prop = kunit_kzalloc(test, sizeof(struct property) * PROP_MAX, GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, prop);

	for (i = 0; i < PROP_LOC; i++)
		pablo_set_prop(&prop[i], prop_name[i], 20, &value[i]);

	/* TC : vc_num must be less than CSI_VIRTUAL_CH_MAX */
	pablo_set_prop(&prop[PROP_LOC], "fid_loc", 20, loc_value);
	pablo_set_prop(&prop[PROP_DUMMY], "dummy_pixel", 8, loc_value);

	pablo_set_node(&child_node, "option", prop);

	np->child = &child_node;
	for (i = 0; i < PROP_MAX - 1; i++)
		prop[i].next = &prop[i + 1];

	cfg = kunit_kzalloc(test, sizeof(struct is_sensor_cfg), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, cfg);

	pkt_ops->cis_opt_parse(np, pdata, cfg);

	KUNIT_EXPECT_EQ(test, cfg->votf, be32_to_cpup(&value[PROP_VOTF]));
	KUNIT_EXPECT_EQ(test, cfg->wdma_ch_hint, be32_to_cpup(&value[PROP_HINT]));
	KUNIT_EXPECT_EQ(test, cfg->max_fps, be32_to_cpup(&value[PROP_MXFPS]));
	KUNIT_EXPECT_EQ(test, cfg->binning, be32_to_cpup(&value[PROP_BINNING]));
	KUNIT_EXPECT_EQ(test, cfg->vvalid_time, be32_to_cpup(&value[PROP_VVALID]));
	KUNIT_EXPECT_EQ(test, cfg->req_vvalid_time, be32_to_cpup(&value[PROP_REQVVALID]));
	KUNIT_EXPECT_EQ(test, cfg->special_mode, (int)be32_to_cpup(&value[PROP_SPEMODE]));
	KUNIT_EXPECT_EQ(test, cfg->dvfs_lv_csis, be32_to_cpup(&value[PROP_DVFS]));
	KUNIT_EXPECT_EQ(test, cfg->ex_mode_extra, be32_to_cpup(&value[PROP_EXMODE]));
	KUNIT_EXPECT_EQ(test, cfg->special_vc_sensor_mode, (int)be32_to_cpup(&value[PROP_SMODE]));
	KUNIT_EXPECT_EQ(test, cfg->fid_loc.line, be32_to_cpup(&loc_value[0]));
	KUNIT_EXPECT_EQ(test, cfg->fid_loc.byte, be32_to_cpup(&loc_value[1]));

	kunit_kfree(test, cfg);
	kunit_kfree(test, prop);
}

static void pablo_dt_module_match_seq_parse_kunit_test(struct kunit *test)
{
	struct device_node *np = &pkt_ctx.dnode;
	u32 value[5] = { 0x96000000, 0x38B00000, 0x96100000, 0x38C00000, 0x96200000 };
	struct exynos_platform_is_module *pdata = &pkt_ctx.module_data;
	struct exynos_sensor_module_match *entry = &pdata->match_entry[0][0];
	struct property prop = {
		.name = "entry",
		.length = 20,
		.value = value,
	};
	struct device_node child[2] = {
		{
			.name = "match_seq",
			.full_name = "match_seq",
			.properties = &prop,
		},
		{
			.name = "option",
			.full_name = "option",
			.properties = &prop,
		},
	};

	child[0].child = &child[1];
	np->child = &child[0];
	pkt_ops->module_match_seq_parse(np, pdata);
	KUNIT_EXPECT_EQ(test, pdata->num_of_match_entry[0], 1); /* 20 /4 /5 == 1 */
	KUNIT_EXPECT_EQ(test, pdata->num_of_match_groups, 1);
	KUNIT_EXPECT_EQ(test, entry->slave_addr, be32_to_cpup(&value[0]));
	KUNIT_EXPECT_EQ(test, entry->reg, be32_to_cpup(&value[1]));
	KUNIT_EXPECT_EQ(test, entry->reg_type, be32_to_cpup(&value[2]));
	KUNIT_EXPECT_EQ(test, entry->expected_data, be32_to_cpup(&value[3]));
	KUNIT_EXPECT_EQ(test, entry->data_type, be32_to_cpup(&value[4]));
}

static void pablo_dt_sensor_module_parse_negative_kunit_test(struct kunit *test)
{
	struct device *dev;
	struct device_node *np = &pkt_ctx.dnode;
	int i;
	int test_result;
	struct property *prop;
	char prop_name[][20] = { "id", "mclk_ch", "position" };
	const int prop_count = 3;
	u32 value = 0x96000000;

	prop = kunit_kzalloc(test, sizeof(struct property) * prop_count, GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, prop);

	for (i = 0; i < prop_count; i++)
		pablo_set_prop(&prop[i], prop_name[i], 4, &value);

	dev = kunit_kzalloc(test, sizeof(struct device), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev);

	dev->of_node = np;

	/* TC : is_board_cfg_parse_dt is fail */
	test_result = pkt_ops->sensor_module_parse(dev, NULL);
	KUNIT_EXPECT_EQ(test, test_result, -EINVAL);

	/* TC : sensor dt callback is NULL */
	prop[0].next = &prop[1];
	prop[1].next = &prop[2];
	np->properties = prop;
	test_result = pkt_ops->sensor_module_parse(dev, NULL);
	KUNIT_EXPECT_EQ(test, test_result, -EINVAL);

	kunit_kfree(test, dev);
	kunit_kfree(test, prop);
}
static int pablo_dt_kunit_test_init(struct kunit *test)
{
	memset(&pkt_ctx, 0, sizeof(pkt_ctx));

	pkt_ops = pablo_kunit_get_dt();

	return 0;
}

static void pablo_dt_kunit_test_exit(struct kunit *test)
{
	pkt_ops = NULL;
}

static struct kunit_case pablo_dt_kunit_test_cases[] = {
	KUNIT_CASE(pablo_dt_parse_qos_table_negative_kunit_test),
	KUNIT_CASE(pablo_dt_chain_dev_parse_negative_kunit_test),
	KUNIT_CASE(pablo_dt_sensor_dev_parse_negative_kunit_test),
	KUNIT_CASE(pablo_dt_board_cfg_parse_kunit_test),
	KUNIT_CASE(pablo_dt_module_cfg_parse_skipped_kunit_test),
	KUNIT_CASE(pablo_dt_module_rom_parse_skipped_kunit_test),
	KUNIT_CASE(pablo_dt_module_peris_parse_skipped_kunit_test),
	KUNIT_CASE(pablo_dt_module_peris_parse_kunit_test),
	KUNIT_CASE(pablo_dt_cis_opt_parse_kunit_test),
	KUNIT_CASE(pablo_dt_module_match_seq_parse_kunit_test),
	KUNIT_CASE(pablo_dt_sensor_module_parse_negative_kunit_test),
	{},
};

struct kunit_suite pablo_dt_kunit_test_suite = {
	.name = "pablo-dt-kunit-test",
	.init = pablo_dt_kunit_test_init,
	.exit = pablo_dt_kunit_test_exit,
	.test_cases = pablo_dt_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_dt_kunit_test_suite);

MODULE_LICENSE("GPL");
