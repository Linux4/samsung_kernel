/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef MTK_MMDVFS_V3_H
#define MTK_MMDVFS_V3_H

#include <linux/clk-provider.h>

#include <linux/remoteproc/mtk_ccu.h>

struct mtk_mux_user {
	int id;
	const char *name;
	const char *target_name;
	unsigned long rate;
	const struct clk_ops *ops;
	unsigned int flags;
	u8 target_id;
	unsigned long undo_rate;
};

typedef int (*call_ccu)(struct platform_device *pdev,
	enum mtk_ccu_feature_type featureType,
	uint32_t msgId, void *inDataPtr, uint32_t inDataSize);

typedef int (*rc_enable)(const bool enable, const bool wdt);

enum {
	CCU_PWR_USR_MMDVFS,
	CCU_PWR_USR_IMG,
	CCU_PWR_USR_NUM
};

enum {
	VCP_PWR_USR_MMDVFS_INIT,
	VCP_PWR_USR_MMDVFS_GENPD,
	VCP_PWR_USR_MMDVFS_FORCE,
	VCP_PWR_USR_MMDVFS_VOTE,
	VCP_PWR_USR_MMDVFS_CCU,
	VCP_PWR_USR_MMDVFS_RST,
	VCP_PWR_USR_MMQOS,
	VCP_PWR_USR_CAM,
	VCP_PWR_USR_IMG,
	VCP_PWR_USR_PDA,
	VCP_PWR_USR_SENIF,
	VCP_PWR_USR_VDEC,
	VCP_PWR_USR_VFMT,
	VCP_PWR_USR_SMI,
	VCP_PWR_USR_DISP,
	VCP_PWR_USR_MDP,
	VCP_PWR_USR_MML,
	VCP_PWR_USR_VENC,
	VCP_PWR_USR_JPEGDEC,
	VCP_PWR_USR_JPEGENC,
	VCP_PWR_USR_NUM
};

enum {
	VMM_USR_CAM,
	VMM_USR_IMG,
	VMM_USR_VDE = 1,
	VMM_USR_NUM
};

enum {
	VMM_CEIL_USR_CAM,
	VMM_CEIL_USR_ADB,
	VMM_CEIL_USR_NUM
};

/* vcp/.../mmdvfs_public.h */
enum {
	USER_DISP,
	USER_DISP_AP,
	USER_MDP,
	USER_MML,
	USER_MMINFRA,
	USER_VENC,
	USER_VENC_AP,
	USER_VDEC,
	USER_VDEC_AP,
	USER_IMG,
	USER_CAM,
	USER_AOV,
	USER_VCORE,
	USER_VMM,
	USER_NUM
};

#if IS_ENABLED(CONFIG_MTK_MMDVFS)
int mtk_mmdvfs_get_ipi_status(void);
int mtk_mmdvfs_enable_vcp(const bool enable, const u8 idx);
int mtk_mmdvfs_enable_ccu(const bool enable, const u8 idx);

int mtk_mmdvfs_camera_notify(const bool enable);
int mtk_mmdvfs_genpd_notify(const u8 idx, const bool enable);
int mtk_mmdvfs_set_avs(const u8 idx, const u32 aging, const u32 fresh);
int mtk_mmdvfs_camsv_dc_enable(const u8 idx, const bool enable);

int mtk_mmdvfs_v3_set_force_step(const u16 pwr_idx, const s16 opp, const bool cmd);
int mtk_mmdvfs_v3_set_vote_step(const u16 pwr_idx, const s16 opp, const bool cmd);
int mtk_mmdvfs_fmeter_register_notifier(struct notifier_block *nb);

void mmdvfs_set_lp_mode(bool lp_mode);
void mmdvfs_call_ccu_set_fp(call_ccu fp);
void mmdvfs_rc_enable_set_fp(rc_enable fp);

int mmdvfs_set_lp_mode_by_vcp(const bool enable);
int mmdvfs_get_version(void);

int mmdvfs_force_step_by_vcp(const u8 pwr_idx, const s8 opp);
int mmdvfs_force_voltage_by_vcp(const u8 pwr_idx, const s8 opp);
int mmdvfs_force_rc_clock_by_vcp(const u8 pwr_idx, const s8 opp);
int mmdvfs_force_single_clock_by_vcp(const u8 mux_idx, const s8 opp);
int mmdvfs_vote_step_by_vcp(const u8 pwr_idx, const s8 opp);

int mmdvfs_mux_set_opp(const char *name, unsigned long rate);
#else
static inline int mtk_mmdvfs_get_ipi_status(void) { return 0; }
static inline int mtk_mmdvfs_enable_vcp(const bool enable, const u8 idx) { return 0; }
static inline int mtk_mmdvfs_enable_ccu(const bool enable, const u8 idx) { return 0; }

static inline int mtk_mmdvfs_camera_notify(const bool enable) { return 0; }
static inline int mtk_mmdvfs_genpd_notify(const u8 idx, const bool enable) { return 0; }
static inline int mtk_mmdvfs_set_avs(const u8 idx, const u32 aging, const u32 fresh) { return 0; }
static inline int mtk_mmdvfs_camsv_dc_enable(const u8 idx, const bool enable) { return 0; }

static inline int mtk_mmdvfs_v3_set_force_step(const u16 pwr_idx, const s16 opp, const bool cmd) { return 0; }
static inline int mtk_mmdvfs_v3_set_vote_step(const u16 pwr_idx, const s16 opp, const bool cmd) { return 0; }
static inline int mtk_mmdvfs_fmeter_register_notifier(struct notifier_block *nb) { return 0; }

static inline void mmdvfs_set_lp_mode(bool lp_mode) { return; }
static inline void mmdvfs_call_ccu_set_fp(call_ccu fp) {return; }
static inline void mmdvfs_rc_enable_set_fp(rc_enable fp) { return; }

static inline int mmdvfs_set_lp_mode_by_vcp(const bool enable) { return 0; }
static inline int mmdvfs_get_version(void) { return 0; }
static inline int mmdvfs_force_step_by_vcp(const u8 pwr_idx, const s8 opp) { return 0; }
static inline int mmdvfs_force_voltage_by_vcp(const u8 pwr_idx, const s8 opp) { return 0; }
static inline int mmdvfs_force_rc_clock_by_vcp(const u8 pwr_idx, const s8 opp) { return 0; }
static inline int mmdvfs_force_single_clock_by_vcp(const u8 mux_idx, const s8 opp) { return 0; }
static inline int mmdvfs_vote_step_by_vcp(const u8 pwr_idx, const s8 opp) { return 0; }
static inline int mmdvfs_mux_set_opp(const char *name, unsigned long rate) { return 0; }
#endif

#endif /* MTK_MMDVFS_V3_H */
