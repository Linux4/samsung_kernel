// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "mtk-mml-mmp.h"

#ifndef MML_FPGA

static struct mml_mmp_events_t mml_mmp_events;

struct mml_mmp_events_t *mml_mmp_get_event(void)
{
	return &mml_mmp_events;
}

void mml_mmp_init(void)
{
	mmp_event mml, command, addon, dle, dpc, clock;

	if (mml_mmp_events.mml)
		return;

	mmprofile_enable(1);
	mml = mmprofile_register_event(MMP_ROOT_EVENT, "MML");
	mml_mmp_events.mml = mml;
	mml_mmp_events.query_mode = mmprofile_register_event(mml, "query_mode");
	mml_mmp_events.submit = mmprofile_register_event(mml, "submit");
	mml_mmp_events.submit_info = mmprofile_register_event(mml_mmp_events.submit, "info");
	mml_mmp_events.config = mmprofile_register_event(mml, "config");
	mml_mmp_events.config_dle = mmprofile_register_event(mml, "config_dle");
	mml_mmp_events.dumpinfo = mmprofile_register_event(mml, "dumpinfo");
	mml_mmp_events.task_create = mmprofile_register_event(mml, "task_create");
	mml_mmp_events.buf_map = mmprofile_register_event(mml, "buf_map");
	mml_mmp_events.comp_prepare = mmprofile_register_event(mml, "comp_prepare");
	mml_mmp_events.buf_prepare = mmprofile_register_event(mml, "buf_prepare");
	mml_mmp_events.tile_alloc = mmprofile_register_event(mml, "tile_alloc");
	mml_mmp_events.tile_calc = mmprofile_register_event(mml, "tile_calc");
	mml_mmp_events.tile_calc_frame = mmprofile_register_event(mml, "tile_calc_frame");
	mml_mmp_events.tile_prepare_tile = mmprofile_register_event(mml, "tile_prepare_tile");
	mml_mmp_events.command = mmprofile_register_event(mml, "command");
	mml_mmp_events.fence = mmprofile_register_event(mml, "fence");
	mml_mmp_events.fence_timeout = mmprofile_register_event(mml, "fence_timeout");
	mml_mmp_events.wait_ready = mmprofile_register_event(mml, "wait_ready");
	mml_mmp_events.throughput = mmprofile_register_event(mml, "throughput");
	mml_mmp_events.bandwidth = mmprofile_register_event(mml, "bandwidth");
	mml_mmp_events.both_set = mmprofile_register_event(mml, "both_set");
	mml_mmp_events.flush = mmprofile_register_event(mml, "flush");
	mml_mmp_events.submit_cb = mmprofile_register_event(mml, "submit_cb");
	mml_mmp_events.taskdone = mmprofile_register_event(mml, "taskdone");
	mml_mmp_events.racing_enter = mmprofile_register_event(mml, "racing_enter");
	mml_mmp_events.racing_stop = mmprofile_register_event(mml, "racing_stop");
	mml_mmp_events.racing_stop_sync = mmprofile_register_event(mml, "racing_stop_sync");
	mml_mmp_events.irq_loop = mmprofile_register_event(mml, "irq_loop");
	mml_mmp_events.irq_err = mmprofile_register_event(mml, "irq_err");
	mml_mmp_events.irq_done = mmprofile_register_event(mml, "irq_done");
	mml_mmp_events.irq_stop = mmprofile_register_event(mml, "irq_stop");
	mml_mmp_events.fence_sig = mmprofile_register_event(mml, "fence_sig");
	mml_mmp_events.exec = mmprofile_register_event(mml, "exec");

	command = mml_mmp_events.command;
	mml_mmp_events.command0 = mmprofile_register_event(command, "command0");
	mml_mmp_events.command1 = mmprofile_register_event(command, "command1");
	mml_mmp_events.mutex_mod = mmprofile_register_event(command, "mutex_mod");
	mml_mmp_events.mutex_en = mmprofile_register_event(command, "mutex_en");

	addon = mmprofile_register_event(mml, "addon");
	mml_mmp_events.addon = addon;
	mml_mmp_events.addon_mml_calc_cfg = mmprofile_register_event(addon, "mml_calc_cfg");
	mml_mmp_events.addon_addon_config = mmprofile_register_event(addon, "addon_config");
	mml_mmp_events.addon_start = mmprofile_register_event(addon, "start");
	mml_mmp_events.addon_unprepare = mmprofile_register_event(addon, "unprepare");
	mml_mmp_events.addon_dle_config = mmprofile_register_event(addon, "dle_config");

	dle = mmprofile_register_event(mml, "dle");
	mml_mmp_events.dle = dle;
	mml_mmp_events.dle_config_create = mmprofile_register_event(dle, "config_create");
	mml_mmp_events.dle_buf = mmprofile_register_event(dle, "buf");
	mml_mmp_events.dle_aal_irq_done = mmprofile_register_event(dle, "aal_irq_done");

	dpc = mmprofile_register_event(mml, "dpc");
	mml_mmp_events.dpc = dpc;
	mml_mmp_events.dpc_cfg = mmprofile_register_event(dpc, "dpc_cfg");
	mml_mmp_events.dpc_exception_flow = mmprofile_register_event(dpc, "dpc_exception_flow");
	mml_mmp_events.dpc_pm_runtime_get = mmprofile_register_event(dpc, "dpc_pm_runtime_get");
	mml_mmp_events.dpc_pm_runtime_put = mmprofile_register_event(dpc, "dpc_pm_runtime_put");

	clock = mmprofile_register_event(mml, "clock");
	mml_mmp_events.clock = clock;
	mml_mmp_events.clk_enable = mmprofile_register_event(clock, "clk_enable");
	mml_mmp_events.clk_disable = mmprofile_register_event(clock, "clk_disable");

	mmprofile_enable_event_recursive(mml, 1);
	mmprofile_start(1);
}

#endif
