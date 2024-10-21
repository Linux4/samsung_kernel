/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Chirs-YC Chen <chris-yc.chen@mediatek.com>
 */

#ifndef __MTK_MML_DPC_H__
#define __MTK_MML_DPC_H__

#include <linux/kernel.h>
#include <linux/types.h>

#include "mtk_dpc.h"

extern int mml_dl_dpc;

enum mml_dl_dpc_config {
	MML_DLDPC_VOTE = 0x1,	/* vote dpc before write and after done */
};

#define VLP_VOTE_SET		0x414
#define VLP_VOTE_CLR		0x418

static struct dpc_funcs mml_dpc_funcs;

/*
 * mml_dpc_register - register dpc driver functions.
 *
 * @funcs:	DPC driver functions.
 */
void mml_dpc_register(const struct dpc_funcs *funcs);

void mml_dpc_enable(bool en);
void mml_dpc_dc_force_enable(bool en);
void mml_dpc_group_enable(const u16 group, bool en);
void mml_dpc_config(const enum mtk_dpc_subsys subsys, bool en);
void mml_dpc_mtcmos_vote(const enum mtk_dpc_subsys subsys,
			 const u8 thread, const bool en);
int mml_dpc_power_keep(void);
void mml_dpc_power_release(void);
void mml_dpc_hrt_bw_set(const enum mtk_dpc_subsys subsys,
			const u32 bw_in_mb,
			bool force_keep);
void mml_dpc_srt_bw_set(const enum mtk_dpc_subsys subsys,
			const u32 bw_in_mb,
			bool force_keep);
void mml_dpc_dvfs_both_set(const enum mtk_dpc_subsys subsys,
			   const u8 level,
			   bool force_keep,
			   const u32 bw_in_mb);

#endif	/* __MTK_MML_H__ */
