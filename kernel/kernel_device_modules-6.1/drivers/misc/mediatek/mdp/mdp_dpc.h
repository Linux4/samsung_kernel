/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Chirs-YC Chen <chris-yc.chen@mediatek.com>
 */

#ifndef __MDP_DPC_H__
#define __MDP_DPC_H__

#include <linux/kernel.h>
#include <linux/types.h>

#include "mtk_dpc.h"

/*
 * mdp_dpc_register - register dpc driver functions.
 *
 * @funcs:	DPC driver functions.
 */
void mdp_dpc_register(const struct dpc_funcs *funcs);

void mdp_dpc_dc_force_enable(bool en);
void mdp_dpc_power_keep(void);
void mdp_dpc_power_release(void);

#endif	/* __MDP_DPC_H__ */
