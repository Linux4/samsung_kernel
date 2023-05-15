/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __DSP_BUS_H__
#define __DSP_BUS_H__

#include <linux/mutex.h>

#include "dsp-hw-bus.h"

#define DSP_BUS_SCENARIO_NAME_LEN	(32)

struct dsp_system;

struct dsp_bus_scenario {
	char		name[DSP_BUS_SCENARIO_NAME_LEN];
	unsigned int	bts_scen_idx;
	struct mutex	lock;
	bool		enabled;
};

int dsp_bus_mo_get(struct dsp_bus *bus, unsigned char *scenario_name);
int dsp_bus_mo_put(struct dsp_bus *bus, unsigned char *scenario_name);

int dsp_bus_open(struct dsp_bus *bus);
int dsp_bus_close(struct dsp_bus *bus);
int dsp_bus_probe(struct dsp_system *sys);
void dsp_bus_remove(struct dsp_bus *bus);

#endif
