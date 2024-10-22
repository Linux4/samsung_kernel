// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Chirs-YC Chen <chris-yc.chen@mediatek.com>
 */

#include "mdp_dpc.h"
#include "cmdq_helper_ext.h"
#include <linux/atomic.h>

static struct dpc_funcs mdp_dpc_funcs;
atomic_t mdp_dpc_exc_pw_cnt;

void mdp_dpc_register(const struct dpc_funcs *funcs)
{
	mdp_dpc_funcs.dpc_dc_force_enable = funcs->dpc_dc_force_enable;
	mdp_dpc_funcs.dpc_vidle_power_keep = funcs->dpc_vidle_power_keep;
	mdp_dpc_funcs.dpc_vidle_power_release = funcs->dpc_vidle_power_release;
}
EXPORT_SYMBOL_GPL(mdp_dpc_register);

void mdp_dpc_dc_force_enable(bool en)
{
	if (mdp_dpc_funcs.dpc_dc_force_enable == NULL)
		return;

	mdp_dpc_funcs.dpc_dc_force_enable(en);
}

void mdp_dpc_power_keep(void)
{
	s32 cur_dpc_exc_pw_cnt;

	if (mdp_dpc_funcs.dpc_vidle_power_keep == NULL)
		return;

	cur_dpc_exc_pw_cnt = atomic_inc_return(&mdp_dpc_exc_pw_cnt);

	if (cur_dpc_exc_pw_cnt > 1)
		return;
	if (cur_dpc_exc_pw_cnt <= 0) {
		CMDQ_ERR("%s cnt %d", __func__, cur_dpc_exc_pw_cnt);
		return;
	}

	mdp_dpc_funcs.dpc_vidle_power_keep(DISP_VIDLE_USER_MDP);
}

void mdp_dpc_power_release(void)
{
	s32 cur_dpc_exc_pw_cnt;

	if (mdp_dpc_funcs.dpc_vidle_power_release == NULL)
		return;

	cur_dpc_exc_pw_cnt = atomic_dec_return(&mdp_dpc_exc_pw_cnt);

	if (cur_dpc_exc_pw_cnt > 0)
		return;
	if (cur_dpc_exc_pw_cnt < 0) {
		CMDQ_ERR("%s cnt %d", __func__, cur_dpc_exc_pw_cnt);
		return;
	}

	mdp_dpc_funcs.dpc_vidle_power_release(DISP_VIDLE_USER_MDP);
}
