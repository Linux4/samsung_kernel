// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include "mtk_dpc_mmp.h"

#if IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)
#else

static struct dpc_mmp_events_t dpc_mmp_events;

struct dpc_mmp_events_t *dpc_mmp_get_event(void)
{
	return &dpc_mmp_events;
}

void dpc_mmp_init(void)
{
	mmp_event folder;

	if (dpc_mmp_events.dpc)
		return;

	mmprofile_enable(1);
	folder = mmprofile_register_event(MMP_ROOT_EVENT, "DPC");
	dpc_mmp_events.dpc = folder;
	dpc_mmp_events.group = mmprofile_register_event(folder, "group");
	dpc_mmp_events.disp_group = mmprofile_register_event(dpc_mmp_events.group, "disp_group");
	dpc_mmp_events.mml_group = mmprofile_register_event(dpc_mmp_events.group, "mml_group");
	dpc_mmp_events.dt = mmprofile_register_event(folder, "dt");
	dpc_mmp_events.disp_dt = mmprofile_register_event(dpc_mmp_events.dt, "disp_dt");
	dpc_mmp_events.mml_dt = mmprofile_register_event(dpc_mmp_events.dt, "mml_dt");
	dpc_mmp_events.mmdvfs_dead = mmprofile_register_event(folder, "mmdvfs_dead");
	dpc_mmp_events.disp_irq = mmprofile_register_event(folder, "disp_irq");
	dpc_mmp_events.mml_irq = mmprofile_register_event(folder, "mml_irq");
	dpc_mmp_events.prete = mmprofile_register_event(dpc_mmp_events.disp_irq, "prete");

	dpc_mmp_events.vlp_vote = mmprofile_register_event(folder, "vlp_vote");
	dpc_mmp_events.cpu_vote = mmprofile_register_event(dpc_mmp_events.vlp_vote, "cpu_vote");
	dpc_mmp_events.gce_vote = mmprofile_register_event(dpc_mmp_events.vlp_vote, "gce_vote");
	dpc_mmp_events.skip_vote = mmprofile_register_event(dpc_mmp_events.vlp_vote, "skip_vote");
	dpc_mmp_events.mtcmos_vote = mmprofile_register_event(folder, "mtcmos_vote");
	dpc_mmp_events.ovl0_vote = mmprofile_register_event(dpc_mmp_events.mtcmos_vote, "ovl0_vote");
	dpc_mmp_events.disp1_vote = mmprofile_register_event(dpc_mmp_events.mtcmos_vote, "disp1_vote");
	dpc_mmp_events.mml1_vote = mmprofile_register_event(dpc_mmp_events.mtcmos_vote, "mml1_vote");

	dpc_mmp_events.mml_sof = mmprofile_register_event(dpc_mmp_events.mml_irq, "mml_sof");
	dpc_mmp_events.mml_rrot_done = mmprofile_register_event(dpc_mmp_events.mml_irq, "mml_rrot_done");
	dpc_mmp_events.window = mmprofile_register_event(folder, "window");
	dpc_mmp_events.disp_window = mmprofile_register_event(dpc_mmp_events.window, "disp");
	dpc_mmp_events.mml_window = mmprofile_register_event(dpc_mmp_events.window, "mml_dc");
	dpc_mmp_events.mminfra_off = mmprofile_register_event(folder, "mminfra_off");
	dpc_mmp_events.apsrc_off = mmprofile_register_event(folder, "apsrc_off");
	//dpc_mmp_events.emi_off = mmprofile_register_event(folder, "emi_off");

	dpc_mmp_events.mtcmos_off = mmprofile_register_event(folder, "mtcmos_off");
	dpc_mmp_events.mtcmos_ovl0 = mmprofile_register_event(dpc_mmp_events.mtcmos_off, "ovl0_off");
	dpc_mmp_events.mtcmos_mml1 = mmprofile_register_event(dpc_mmp_events.mtcmos_off, "mml1_off");
	dpc_mmp_events.dvfs_off = mmprofile_register_event(folder, "dvfs_off");
	dpc_mmp_events.vdisp_off = mmprofile_register_event(folder, "vdisp_off");
	dpc_mmp_events.vdisp_level = mmprofile_register_event(dpc_mmp_events.vdisp_off, "vdisp_level");
	dpc_mmp_events.vdisp_disp = mmprofile_register_event(dpc_mmp_events.vdisp_off, "vdisp_disp");
	dpc_mmp_events.vdisp_mml = mmprofile_register_event(dpc_mmp_events.vdisp_off, "vdisp_mml");
	dpc_mmp_events.hrt_bw = mmprofile_register_event(dpc_mmp_events.dvfs_off, "hrt_bw");
	dpc_mmp_events.srt_bw = mmprofile_register_event(dpc_mmp_events.dvfs_off, "srt_bw");

	mmprofile_enable_event_recursive(folder, 1);
	mmprofile_start(1);
}

#endif
