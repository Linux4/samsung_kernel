/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_DT_H
#include <linux/slab.h>
#define FIMC_IS_DT_H

#define DT_READ_U32(node, key, value) do {\
		pprop = key; \
		if (of_property_read_u32((node), key, &temp)) \
			goto p_err; \
		(value) = temp; \
	} while (0)

#define DT_READ_STR(node, key, value) do {\
		pprop = key; \
		if (of_property_read_string((node), key, &name)) \
			goto p_err; \
		(value) = name; \
	} while (0)

#define SET_PIN(p, id1, id2, n, _pin, _act) \
		p->pin_ctrls[id1][id2][n].pin = _pin; \
		p->pin_ctrls[id1][id2][n].act = _act;

struct exynos5_platform_fimc_is *fimc_is_parse_dt(struct device *dev);
#endif
