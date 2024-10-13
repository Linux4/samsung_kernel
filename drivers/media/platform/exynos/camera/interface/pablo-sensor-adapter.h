/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series Pablo IS driver
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef PABLO_SENSOR_ADAPTER_H
#define PABLO_SENSOR_ADAPTER_H

#include <linux/types.h>

struct is_sensor_interface;
struct pablo_crta_sensor_info;
struct pablo_sensor_control_info;

struct pablo_sensor_adt {
	void					*priv;
	atomic_t				rsccount;

	const struct pablo_sensor_adt_ops	*ops;
};

typedef void (*pablo_sensor_adt_control_sensor_callback)(void *caller, u32 instance, u32 ret,
						   dma_addr_t dva, u32 idx);

#define CALL_SS_ADT_OPS(adt, op, args...)	\
	((adt && (adt)->ops && (adt)->ops->op) ? ((adt)->ops->op(adt, ##args)) : 0)

struct pablo_sensor_adt_ops {
	int (*open)(struct pablo_sensor_adt *adt, u32 instance,
		    struct is_sensor_interface *sensor_itf);
	int (*close)(struct pablo_sensor_adt *sensor_adt);
	int (*start)(struct pablo_sensor_adt *sensor_adt);
	int (*stop)(struct pablo_sensor_adt *sensor_adt);
	int (*update_actuator_info)(struct pablo_sensor_adt *adt, bool block);
	int (*get_sensor_info)(struct pablo_sensor_adt *adt, struct pablo_crta_sensor_info *prsi);
	int (*control_sensor)(struct pablo_sensor_adt *adt, struct pablo_sensor_control_info *psci,
		dma_addr_t dva, u32 idx);
	int (*register_control_sensor_callback)(struct pablo_sensor_adt *adt, void *caller,
		pablo_sensor_adt_control_sensor_callback callback);
};

#if IS_ENABLED(CONFIG_PABLO_SENSOR_ADT)
int pablo_sensor_adt_probe(struct pablo_sensor_adt *sensor_adt);
#else
#define pablo_sensor_adt_probe(sensor_adt)	({ 0; })
#endif

#endif /* PABLO_SENSOR_ADAPTER_H */
