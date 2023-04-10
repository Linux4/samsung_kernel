/* linux/drivers/cpufreq/coremem_ratio.c
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * Samsung CPU Support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/math64.h>
#include <linux/types.h>
#include <linux/of.h>
#include "pmu_func.h"

#define SHIFT_LF 20

#define LPKI_COND_CM_100_000		0     // INT(L2M*(1<<SHIFT_LF)/#INST) ~0.0000
#define LPKI_COND_CM_080_020		9544  // INT(L2M*(1<<SHIFT_LF)/#INST) ~0.0091
#define LPKI_COND_CM_060_040		19088 // INT(L2M*(1<<SHIFT_LF)/#INST) ~0.0182
#define LPKI_COND_CM_040_060		28632 // INT(L2M*(1<<SHIFT_LF)/#INST) ~0.0273
#define LPKI_COND_CM_020_080		38176 // INT(L2M*(1<<SHIFT_LF)/#INST) ~0.0364
#define LPKI_COND_CM_000_100		47720 // INT(L2M*(1<<SHIFT_LF)/#INST) ~0.0455

static u32 const_lpki_cond_cm_100_000 = LPKI_COND_CM_100_000;
static u32 const_lpki_cond_cm_080_020 = LPKI_COND_CM_080_020;
static u32 const_lpki_cond_cm_060_040 = LPKI_COND_CM_060_040;
static u32 const_lpki_cond_cm_040_060 = LPKI_COND_CM_040_060;
static u32 const_lpki_cond_cm_020_080 = LPKI_COND_CM_020_080;
static u32 const_lpki_cond_cm_000_100 = LPKI_COND_CM_000_100;

/*
 * @instcnt	: the number of instructions
 * @linefcnt	: the number of L2 refill
 * @Return	: Region number
 */
int coremem_ratio(u32 instcnt, u32 linefcnt)
{
	u64 L2RFn, INSTn, L2MPIx;

	if (instcnt == 0)
		return 0;

	/* dividend scaling to prevent saturation to zero */
	L2RFn  = (u64)linefcnt * (1 << (SHIFT_LF));
	INSTn  = (u64)instcnt;

	/* Calculated L2MISS^20 / INSTn */
	L2MPIx = div64_u64(L2RFn, INSTn);

	/*
	 * Judge core:mem ratio based on LPKI boundary condition
	 * And report the region
	 */
	if (L2MPIx < const_lpki_cond_cm_080_020)
		return REGION_C100_M000_C080_M020;
	if (L2MPIx < const_lpki_cond_cm_060_040)
		return REGION_C080_M020_C060_M040;
	if (L2MPIx < const_lpki_cond_cm_040_060)
		return REGION_C060_M040_C040_M060;
	if (L2MPIx < const_lpki_cond_cm_020_080)
		return REGION_C040_M060_C020_M080;

	return REGION_C020_M080_C000_M100;
}

static int __init coremem_ratio_init(void)
{
#ifdef CONFIG_OF
	struct device_node *np;
	u32 const_lpki[6];

	np = of_find_compatible_node(NULL, NULL, "samsung,exynos-pmu_coremem");
	if (!of_device_is_available(np)) {
		pr_err("%s: Failed to find compatible node\n", __func__);
		return -ENODEV;
	}

	if (of_property_read_u32_array(np, "const_lpki", (u32 *)&const_lpki,
					(size_t)ARRAY_SIZE(const_lpki))) {
		pr_err("%s: failed get property\n", __func__);
		return -ENODEV;
	}

	const_lpki_cond_cm_100_000 = const_lpki[0];
	const_lpki_cond_cm_080_020 = const_lpki[1];
	const_lpki_cond_cm_060_040 = const_lpki[2];
	const_lpki_cond_cm_040_060 = const_lpki[3];
	const_lpki_cond_cm_020_080 = const_lpki[4];
	const_lpki_cond_cm_000_100 = const_lpki[5];
#endif

	return 0;
}

fs_initcall(coremem_ratio_init);
