// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <soc/samsung/bts.h>

#include "dsp-log.h"
#include "hardware/dsp-system.h"
#include "hardware/dsp-bus.h"

int dsp_bus_mo_get(struct dsp_bus *bus, unsigned char *scenario_name)
{
	int ret;
	int idx;
	struct dsp_bus_scenario *scen = NULL;

	dsp_enter();
	for (idx = 0; idx < DSP_MO_SCENARIO_COUNT; ++idx) {
		if (!strncmp(bus->scen[idx].name, scenario_name,
					DSP_BUS_SCENARIO_NAME_LEN)) {
			scen = &bus->scen[idx];
			break;
		}
	}

	if (!scen) {
		ret = -EINVAL;
		dsp_err("scenario_name(%s) is invalid\n", scenario_name);
		goto p_err;
	}

	if (!scen->bts_scen_idx) {
		ret = -EINVAL;
		dsp_err("scenario_name(%s) was not initilized\n", scen->name);
		goto p_err;
	}
	dsp_dbg("scenario[%s] idx : %u\n", scen->name, scen->bts_scen_idx);

	mutex_lock(&scen->lock);
	if (!scen->enabled) {
		bts_add_scenario(scen->bts_scen_idx);
		scen->enabled = true;
		dsp_info("bus scenario[%s] is enabled\n", scen->name);
	} else {
		dsp_dbg("bus scenario[%s] is already enabled\n", scen->name);
	}
	mutex_unlock(&scen->lock);

	dsp_leave();
	return 0;
p_err:
	return ret;
}

int dsp_bus_mo_put(struct dsp_bus *bus, unsigned char *scenario_name)
{
	int ret;
	int idx;
	struct dsp_bus_scenario *scen = NULL;

	dsp_enter();
	for (idx = 0; idx < DSP_MO_SCENARIO_COUNT; ++idx) {
		if (!strncmp(bus->scen[idx].name, scenario_name,
					DSP_BUS_SCENARIO_NAME_LEN)) {
			scen = &bus->scen[idx];
			break;
		}
	}

	if (!scen) {
		ret = -EINVAL;
		dsp_err("scenario_name(%s) is invalid\n", scenario_name);
		goto p_err;
	}

	if (!scen->bts_scen_idx) {
		ret = -EINVAL;
		dsp_err("scenario_name(%s) was not initilized\n", scen->name);
		goto p_err;
	}
	dsp_dbg("scenario[%s] idx : %u\n", scen->name, scen->bts_scen_idx);

	mutex_lock(&scen->lock);
	if (scen->enabled) {
		bts_del_scenario(scen->bts_scen_idx);
		scen->enabled = false;
		dsp_info("bus scenario[%s] is disabled\n", scen->name);
	} else {
		dsp_warn("bus scenario[%s] is not enabled\n", scen->name);
	}
	mutex_unlock(&scen->lock);

	dsp_leave();
	return 0;
p_err:
	return ret;
}

int dsp_bus_open(struct dsp_bus *bus)
{
	int idx;
	struct dsp_bus_scenario *scen;

	dsp_enter();
	for (idx = 0; idx < DSP_MO_SCENARIO_COUNT; ++idx) {
		scen = &bus->scen[idx];

		dsp_dbg("scenario[%d/%d] info : [%s/%u]\n",
				idx, DSP_MO_SCENARIO_COUNT,
				scen->name, scen->bts_scen_idx);
	}
	dsp_leave();
	return 0;
}

int dsp_bus_close(struct dsp_bus *bus)
{
	int idx;
	struct dsp_bus_scenario *scen;

	dsp_enter();
	for (idx = 0; idx < DSP_MO_SCENARIO_COUNT; ++idx) {
		scen = &bus->scen[idx];

		mutex_lock(&scen->lock);
		if (scen->enabled) {
			bts_del_scenario(scen->bts_scen_idx);
			scen->enabled = false;
			dsp_warn("bus scenario[%s] is forcibly disabled\n",
					scen->name);
		}
		mutex_unlock(&scen->lock);
	}
	dsp_leave();
	return 0;
}

int dsp_bus_probe(struct dsp_system *sys)
{
	int ret;
	struct dsp_bus *bus;
	struct dsp_bus_scenario *scen;

	dsp_enter();
	bus = &sys->bus;
	bus->sys = sys;

	bus->scen = kzalloc(sizeof(*scen) * DSP_MO_SCENARIO_COUNT, GFP_KERNEL);
	if (!bus->scen) {
		ret = -ENOMEM;
		dsp_err("Failed to alloc dsp_bus_scenario\n");
		goto p_err;
	}

	scen = &bus->scen[DSP_MO_MAX];
	snprintf(scen->name, DSP_BUS_SCENARIO_NAME_LEN, "max");
	scen->bts_scen_idx = bts_get_scenindex("npu_performance");
	mutex_init(&scen->lock);
	scen->enabled = false;

	dsp_leave();
	return 0;
p_err:
	return ret;
}

void dsp_bus_remove(struct dsp_bus *bus)
{
	int idx;

	dsp_enter();
	for (idx = 0; idx < DSP_MO_SCENARIO_COUNT; ++idx)
		mutex_destroy(&bus->scen[idx].lock);
	kfree(bus->scen);
	dsp_leave();
}
