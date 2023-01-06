/*
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 *
 * EXYNOS - Stage 2 Protection Unit(S2MPU)
 * Author: Junho Choi <junhosj.choi@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/err.h>
#include <linux/module.h>
#include <linux/hvc.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>

#include <soc/samsung/exynos-s2mpu.h>

static const char *exynos_s2mpu_list[EXYNOS_MAX_S2MPU_INSTANCE];
static uint32_t s2mpu_num;

uint64_t exynos_set_dev_stage2_pcie_ap(const char *s2mpu_name,
		uint32_t vid,
		uint64_t base,
		uint64_t size,
		uint32_t ap)
{
	uint32_t s2mpu_idx;

	if (s2mpu_name == NULL) {
		pr_err("%s: Invalid S2MPU name\n", __func__);
		return ERROR_INVALID_S2MPU_NAME;
	}

	for (s2mpu_idx = 0; s2mpu_idx < s2mpu_num; s2mpu_idx++) {
		if (!strncmp(exynos_s2mpu_list[s2mpu_idx],
					s2mpu_name,
					EXYNOS_MAX_S2MPU_NAME_LEN))
			break;
	}

	if (s2mpu_idx == s2mpu_num) {
		pr_err("%s: DO NOT support %s S2MPU\n",
				__func__, s2mpu_name);
		return ERROR_DO_NOT_SUPPORT_S2MPU;
	}

	return exynos_hvc(HVC_FID_SET_S2MPU_FOR_WIFI,
			(vid << S2MPU_VID_SHIFT) |
			s2mpu_idx,
			base,
			size,
			ap);
}

uint64_t exynos_set_dev_stage2_ap(const char *s2mpu_name,
				  uint32_t vid,
				  uint64_t base,
				  uint64_t size,
				  uint32_t ap)
{
	uint32_t s2mpu_idx;

	if (s2mpu_name == NULL) {
		pr_err("%s: Invalid S2MPU name\n", __func__);
		return ERROR_INVALID_S2MPU_NAME;
	}

	for (s2mpu_idx = 0; s2mpu_idx < s2mpu_num; s2mpu_idx++) {
		if (!strncmp(exynos_s2mpu_list[s2mpu_idx],
				s2mpu_name,
				EXYNOS_MAX_S2MPU_NAME_LEN))
			break;
	}

	if (s2mpu_idx == s2mpu_num) {
		pr_err("%s: DO NOT support %s S2MPU\n",
				__func__, s2mpu_name);
		return ERROR_DO_NOT_SUPPORT_S2MPU;
	}

	return exynos_hvc(HVC_FID_SET_S2MPU,
			  (vid << S2MPU_VID_SHIFT) |
			  s2mpu_idx,
			  base,
			  size,
			  ap);
}

#ifdef CONFIG_OF_RESERVED_MEM
static int __init exynos_s2mpu_reserved_mem_setup(struct reserved_mem *remem)
{
	pr_err("%s: Reserved memory for S2MPU table: addr=%lx, size=%lx\n",
			__func__, remem->base, remem->size);

	return 0;
}
RESERVEDMEM_OF_DECLARE(s2mpu_table, "exynos,s2mpu_table", exynos_s2mpu_reserved_mem_setup);
#endif

static int __init exynos_s2mpu_init(void)
{
	struct device_node *np = NULL;
	int ret = 0;

	np = of_find_compatible_node(NULL, NULL, "samsung,exynos-s2mpu");
	if (np == NULL) {
		pr_err("%s: Do not support S2MPU\n", __func__);
		return -ENODEV;
	}

	ret = of_property_read_u32(np, "instance-num", &s2mpu_num);
	if (ret) {
		pr_err("%s: Fail to get S2MPU instance number from device tree\n",
			__func__);
		return ret;
	}

	pr_debug("%s: S2MPU instance number : %d\n", __func__, s2mpu_num);

	ret = of_property_read_string_array(np,
					    "instance-names",
					    exynos_s2mpu_list,
					    s2mpu_num);
	if (ret < 0) {
		pr_err("%s: Fail to get S2MPU instance list from device tree\n",
			__func__);
		return ret;
	}

	pr_info("%s: Making S2MPU instance list is done\n", __func__);

	return 0;
}
core_initcall(exynos_s2mpu_init);

MODULE_DESCRIPTION("Exynos Stage 2 Protection Unit(S2MPU) driver");
MODULE_AUTHOR("<junhosj.choi@samsung.com>");
MODULE_LICENSE("GPL");
