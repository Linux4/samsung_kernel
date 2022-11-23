// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2021 Spreadtrum Communications Inc.

#include <asm/page.h>
#include <linux/err.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/sprd_soc_id.h>

static const char * const syscon_name[] = {
		"chip_id",
		"plat_id",
		"implement_id",
		"manufacture_id",
		"version_id"
};

struct register_gpr {
	struct regmap *gpr;
	uint32_t reg;
	uint32_t mask;
};
static struct register_gpr syscon_regs[ARRAY_SIZE(syscon_name)];

int sprd_get_soc_id(sprd_soc_id_type_t soc_id_type, u32 *id, int id_len)
{
	int ret;
	u32 chip_id[2];

	switch (soc_id_type) {
	case AON_CHIP_ID:
	case AON_PLAT_ID:
		if (id_len < 2) {
			pr_err("id_len < 2\n");
			return -EINVAL;
		}

		ret = regmap_read(syscon_regs[soc_id_type].gpr, syscon_regs[soc_id_type].reg, &chip_id[0]);
		if (ret) {
			pr_err("Failed to read chip_id[0]\n");
			return -EINVAL;
		}
		ret = regmap_read(syscon_regs[soc_id_type].gpr, syscon_regs[soc_id_type].reg + 0x4, &chip_id[1]);
		if (ret) {
			pr_err("Failed to read chip_id[1]\n");
			return -EINVAL;
		}
		*id = chip_id[0];
		*(id + 1) = chip_id[1];
		break;
	case AON_IMPL_ID:
	case AON_MFT_ID:
	case AON_VER_ID:
		ret = regmap_read(syscon_regs[soc_id_type].gpr, syscon_regs[soc_id_type].reg, id);
		if (ret) {
			pr_err("Failed to read soc id\n");
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}


static int sprd_soc_id_probe(struct platform_device *pdev)
{
	int i, ret = 0;
	struct device_node *np = pdev->dev.of_node;
	const char *pname;
	struct regmap *tregmap;
	uint32_t args[2];

	for (i = 0; i < ARRAY_SIZE(syscon_name); i++) {
		pname = syscon_name[i];
		tregmap =  syscon_regmap_lookup_by_name(np, pname);
		if (IS_ERR_OR_NULL(tregmap)) {
			pr_err("fail to read %s regmap\n", pname);
			continue;
		}
		ret = syscon_get_args_by_name(np, pname, 2, args);
		if (ret != 2) {
			pr_err("fail to read %s args, ret %d\n",
				pname, ret);
			continue;
		}
		syscon_regs[i].gpr = tregmap;
		syscon_regs[i].reg = args[0];
		syscon_regs[i].mask = args[1];
		pr_debug("dts[%s] 0x%x 0x%x\n", pname,
			syscon_regs[i].reg,
			syscon_regs[i].mask);
	}

	return 0;
}

static const struct of_device_id sprd_soc_id_of_match[] = {
	{.compatible = "sprd,soc-id"},
	{},
};

static struct platform_driver sprd_soc_id_driver = {
	.probe = sprd_soc_id_probe,
	.driver = {
		.name = "sprd-soc-id",
		.of_match_table = sprd_soc_id_of_match,
	},
};

module_platform_driver(sprd_soc_id_driver);

MODULE_AUTHOR("Luting Guo <luting.guo@spreadtrum.com>");
MODULE_DESCRIPTION("Spreadtrum soc id driver");
MODULE_LICENSE("GPL v2");

