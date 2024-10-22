// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/notifier.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/suspend.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
#if IS_ENABLED(CONFIG_DEBUG_FS)
#include <linux/debugfs.h>
#endif

#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#include <dt-bindings/clock/mmdvfs-clk.h>
#include <soc/mediatek/mmdvfs_v3.h>
#include "mtk-mmdvfs-v3-memory.h"

#include "mtk_dpc.h"
#include "mtk_dpc_mmp.h"
#include "mtk_dpc_internal.h"

#include "mtk_disp_vidle.h"
#include "mtk-mml-dpc.h"
#include "mdp_dpc.h"

int debug_runtime_ctrl;
module_param(debug_runtime_ctrl, int, 0644);
int debug_mmp;
module_param(debug_mmp, int, 0644);
int debug_dvfs;
module_param(debug_dvfs, int, 0644);
int debug_dvfs_vdisp;
module_param(debug_dvfs_vdisp, int, 0644);
int debug_check_reg;
module_param(debug_check_reg, int, 0644);
int debug_check_rtff;
module_param(debug_check_rtff, int, 0644);
int debug_check_event;
module_param(debug_check_event, int, 0644);
int debug_mtcmos_off;
module_param(debug_mtcmos_off, int, 0644);
int debug_irq;
module_param(debug_irq, int, 0644);
int debug_vidle_timeout;
module_param(debug_vidle_timeout, int, 0644);
int debug_vdisp_level;
module_param(debug_vdisp_level, int, 0644);

#define DPC_DEBUG_RTFF_CNT 10
static void __iomem *debug_rtff[DPC_DEBUG_RTFF_CNT];

enum mtk_base_id {
	DPC_BASE,
	VLP_BASE,
	SPM_BASE,
	HW_VOTE_STATE,
	VDISP_DVFSRC_DEBUG,
	VDISP_DVFSRC_EN,
	VCORE_DVFSRC_DEBUG,
	MMINFRA_HANG_FREE,
	DPC_SYS_REGS_CNT,
};

#define MTK_DPC_MMDVFS_MAX_COUNT   2
static unsigned int mtk_dpc_mmdvfs_settings_dirty;
static unsigned int mtk_dpc_mmdvfs_settings_value[MTK_DPC_MMDVFS_MAX_COUNT];
static unsigned int mt6878_mmdvfs_settings_addr[] = {
	DISP_REG_DPC_DISP_VDISP_DVFS_VAL,
	DISP_REG_DPC_MML_VDISP_DVFS_VAL,
};

static const char *reg_names[DPC_SYS_REGS_CNT] = {
	"DPC_BASE",
	"VLP_BASE",
	"SPM_BASE",
	"hw_vote_status",
	"vdisp_dvsrc_debug_sta_7",
	"dvfsrc_en",
	"dvfsrc_debug_sta_1",
	"mminfra_hangfree",
};
/* TODO: move to mtk_dpc_test.c */

static void __iomem *dpc_base;
static spinlock_t vlp_lock; /* power status protection*/
static spinlock_t dpc_lock;
static enum mtk_panel_type g_panel_type = PANEL_TYPE_COUNT;
static unsigned int g_te_duration; //us
static unsigned int g_vb_duration; //us
static unsigned int g_vdisp_level_disp;
static unsigned int g_vdisp_level_mml;
static DEFINE_MUTEX(dvfs_lock);

static const char *mtk_dpc_vidle_cap_name[DPC_VIDLE_CAP_COUNT] = {
	"MTCMOS_OFF",
	"APSRC_OFF",
	"LOWER_VCORE_DVFS",
	"LOWER_VDISP_DVFS",
	"ZERO_HRT_BW",
	"ZERO_SRT_BW", //5
	"DSI_PLL_OFF",
	"MAIN_PLL_OFF",
	"MMINFRA_PLL_OFF",
	"INFRA_REQ_OFF",
	"SMI_REQ", //10
};

static const char *mtk_dpc_idle_name[DPC_IDLE_ID_MAX] = {
	"MTCMOS_OFF_OVL0",
	"MTCMOS_OFF_MML1",
	"MTCMOS_OFF_DISP1",
	"MMINFRA_OFF",
	"APSRC_OFF",
	"VCORE_DVFS_OFF",
	"VDISP_DVFS_OFF",
	"WINDOW_DISP",
	"WINDOW_MML",
	"WINDOW_VIDLE",
};

enum mtk_dpc_state {
	DPC_STATE_NULL,
	DPC_STATE_ON,
	DPC_STATE_OFF,
};

struct mtk_dpc {
	struct platform_device *pdev;
	struct device *dev;
	struct device *pd_dev;
	struct notifier_block pm_nb;
	int disp_irq;
	int mml_irq;
	unsigned int vidle_mask;
	resource_size_t dpc_pa;
	resource_size_t vlp_pa;
	void __iomem *sys_va[DPC_SYS_REGS_CNT];
	struct cmdq_client *cmdq_client;
	atomic_t dpc_en_cnt;
	bool skip_force_power;
	spinlock_t skip_force_power_lock;
	wait_queue_head_t dpc_state_wq;
	atomic_t dpc_state;
#if IS_ENABLED(CONFIG_DEBUG_FS)
	struct dentry *fs;
#endif
	struct mtk_dpc_dvfs_bw dvfs_bw;
	unsigned int mmsys_id;
	struct mtk_dpc_dt_usage *disp_cmd_dt_usage;
	struct mtk_dpc_dt_usage *mml_cmd_dt_usage;
	struct mtk_dpc_dt_usage *disp_vdo_dt_usage;
	struct mtk_dpc_dt_usage *mml_vdo_dt_usage;
	struct timer_list dpc_timer;
	wait_queue_head_t dpc_mtcmos_wq;
	atomic_t dpc_mtcmos_timeout;
	bool mmdvfs_power_sync;
	unsigned int mmdvfs_settings_count;
	unsigned int *mmdvfs_settings_addr;
	unsigned int (*get_sys_status)(enum dpc_sys_status_id, unsigned int *status);
};
static struct mtk_dpc *g_priv;
static unsigned int g_vidle_events;
static atomic_t g_vidle_window;
static spinlock_t dpc_state_lock; /* power status protection*/
struct mtk_dpc_dt_usage *g_disp_dt_usage;
struct mtk_dpc_dt_usage *g_mml_dt_usage;
static atomic_t g_disp_mode;
static atomic_t g_mml_mode;
static unsigned int g_idle_ratio_debug;
static unsigned long long g_idle_period[DPC_IDLE_ID_MAX] = {0};
static unsigned long long g_idle_start[DPC_IDLE_ID_MAX] = {0};

#define mtk_dpc_support_cap(id) (g_priv && (g_priv->vidle_mask & (0x1 << id)))

static struct mtk_dpc_dt_usage mt6989_disp_cmd_dt_usage[DPC_DISP_DT_CNT] = {
	/* OVL0/OVL1/DISP0 */
	{0, DPC_SP_FRAME_DONE,	DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_MTCMOS},		/* OFF Time 0 */
	{1, DPC_SP_TE,		DT_1,	DPC_DISP_VIDLE_MTCMOS},		/* ON Time */
	{2, DPC_SP_TE,		DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_MTCMOS},		/* Pre-TE */
	{3, DPC_SP_TE,		DT_3,	DPC_DISP_VIDLE_MTCMOS},		/* OFF Time 1 */

	/* DISP1 */
	{4, DPC_SP_FRAME_DONE,	DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_MTCMOS_DISP1},
	{5, DPC_SP_TE,		DT_5,	DPC_DISP_VIDLE_MTCMOS_DISP1},
	{6, DPC_SP_TE,		DT_6,	DPC_DISP_VIDLE_MTCMOS_DISP1},	/* DISP1-TE */
	{7, DPC_SP_TE,		DT_7,	DPC_DISP_VIDLE_MTCMOS_DISP1},

	/* VDISP DVFS, follow DISP1 by default, or HRT BW */
	{8, DPC_SP_FRAME_DONE,	DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_VDISP_DVFS},
	{9, DPC_SP_TE,		DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_VDISP_DVFS},
	{10, DPC_SP_TE,		DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_VDISP_DVFS},

	/* HRT BW */
	{11, DPC_SP_FRAME_DONE,	DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_HRT_BW},		/* OFF Time 0 */
	{12, DPC_SP_TE,		DT_12,	DPC_DISP_VIDLE_HRT_BW},		/* ON Time */
	{13, DPC_SP_TE,		DT_13,	DPC_DISP_VIDLE_HRT_BW},		/* OFF Time 1 */

	/* SRT BW, follow HRT BW by default, or follow DISP1 */
	{14, DPC_SP_FRAME_DONE,	DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_SRT_BW},
	{15, DPC_SP_TE,		DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_SRT_BW},
	{16, DPC_SP_TE,		DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_SRT_BW},

	/* MMINFRA OFF, follow DISP1 by default, or HRT BW */
	{17, DPC_SP_FRAME_DONE,	DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_MMINFRA_OFF},
	{18, DPC_SP_TE,		DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_MMINFRA_OFF},
	{19, DPC_SP_TE,		DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_MMINFRA_OFF},

	/* INFRA OFF, follow DISP1 by default, or HRT BW */
	{20, DPC_SP_FRAME_DONE,	DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_INFRA_OFF},
	{21, DPC_SP_TE,		DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_INFRA_OFF},
	{22, DPC_SP_TE,		DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_INFRA_OFF},

	/* MAINPLL OFF, follow DISP1 by default, or HRT BW */
	{23, DPC_SP_FRAME_DONE,	DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_MAINPLL_OFF},
	{24, DPC_SP_TE,		DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_MAINPLL_OFF},
	{25, DPC_SP_TE,		DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_MAINPLL_OFF},

	/* MSYNC 2.0 */
	{26, DPC_SP_TE,		DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_MSYNC2_0},
	{27, DPC_SP_TE,		DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_MSYNC2_0},
	{28, DPC_SP_TE,		DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_MSYNC2_0},

	/* RESERVED */
	{29, DPC_SP_FRAME_DONE,	3000,	DPC_DISP_VIDLE_RESERVED},
	{30, DPC_SP_TE,		16000,	DPC_DISP_VIDLE_RESERVED},
	{31, DPC_SP_TE,		DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_RESERVED},
};

static struct mtk_dpc_dt_usage mt6989_mml_cmd_dt_usage[DPC_MML_DT_CNT] = {
	/* MML1 */
	{32, DPC_SP_RROT_DONE,	DT_MAX_TIMEOUT,	DPC_MML_VIDLE_MTCMOS},		/* OFF Time 0 */
	{33, DPC_SP_TE,		DT_1,	DPC_MML_VIDLE_MTCMOS},		/* ON Time */
	{34, DPC_SP_TE,		DT_MAX_TIMEOUT,	DPC_MML_VIDLE_MTCMOS},		/* MML-TE */
	{35, DPC_SP_TE,		DT_3,	DPC_MML_VIDLE_MTCMOS},		/* OFF Time 1 */

	/* VDISP DVFS, follow MML1 by default, or HRT BW */
	{36, DPC_SP_RROT_DONE,	DT_MAX_TIMEOUT,	DPC_MML_VIDLE_VDISP_DVFS},
	{37, DPC_SP_TE,		DT_MAX_TIMEOUT,	DPC_MML_VIDLE_VDISP_DVFS},
	{38, DPC_SP_TE,		DT_MAX_TIMEOUT,	DPC_MML_VIDLE_VDISP_DVFS},

	/* HRT BW */
	{39, DPC_SP_RROT_DONE,	DT_MAX_TIMEOUT,	DPC_MML_VIDLE_HRT_BW},		/* OFF Time 0 */
	{40, DPC_SP_TE,		DT_12,	DPC_MML_VIDLE_HRT_BW},		/* ON Time */
	{41, DPC_SP_TE,		DT_13,	DPC_MML_VIDLE_HRT_BW},		/* OFF Time 1 */

	/* SRT BW, follow HRT BW by default, or follow MML1 */
	{42, DPC_SP_RROT_DONE,	DT_MAX_TIMEOUT,	DPC_MML_VIDLE_SRT_BW},
	{43, DPC_SP_TE,		DT_MAX_TIMEOUT,	DPC_MML_VIDLE_SRT_BW},
	{44, DPC_SP_TE,		DT_MAX_TIMEOUT,	DPC_MML_VIDLE_SRT_BW},

	/* MMINFRA OFF, follow MML1 by default, or HRT BW */
	{45, DPC_SP_RROT_DONE,	DT_MAX_TIMEOUT,	DPC_MML_VIDLE_MMINFRA_OFF},
	{46, DPC_SP_TE,		DT_MAX_TIMEOUT,	DPC_MML_VIDLE_MMINFRA_OFF},
	{47, DPC_SP_TE,		DT_MAX_TIMEOUT,	DPC_MML_VIDLE_MMINFRA_OFF},

	/* INFRA OFF, follow MML1 by default, or HRT BW */
	{48, DPC_SP_RROT_DONE,	DT_MAX_TIMEOUT,	DPC_MML_VIDLE_INFRA_OFF},
	{49, DPC_SP_TE,		DT_MAX_TIMEOUT,	DPC_MML_VIDLE_INFRA_OFF},
	{50, DPC_SP_TE,		DT_MAX_TIMEOUT,	DPC_MML_VIDLE_INFRA_OFF},

	/* MAINPLL OFF, follow MML1 by default, or HRT BW */
	{51, DPC_SP_RROT_DONE,	DT_MAX_TIMEOUT,	DPC_MML_VIDLE_MAINPLL_OFF},
	{52, DPC_SP_TE,		DT_MAX_TIMEOUT,	DPC_MML_VIDLE_MAINPLL_OFF},
	{53, DPC_SP_TE,		DT_MAX_TIMEOUT,	DPC_MML_VIDLE_MAINPLL_OFF},

	/* RESERVED */
	{54, DPC_SP_RROT_DONE,	3000,	DPC_MML_VIDLE_RESERVED},
	{55, DPC_SP_TE,		16000,	DPC_MML_VIDLE_RESERVED},
	{56, DPC_SP_TE,		DT_MAX_TIMEOUT,	DPC_MML_VIDLE_RESERVED},
};

static struct mtk_dpc_dt_usage mt6878_disp_vdo_dt_usage[DPC_DISP_DT_CNT] = {
	/* OVL0/OVL1/DISP0 */
	{0, DPC_SP_FRAME_DONE,	DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_MTCMOS},		/* OFF Time 0 */
	{1, DPC_SP_SOF,		DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_MTCMOS},		/* ON Time */
	{2, DPC_SP_SOF,		DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_MTCMOS},		/* Pre-TE */
	{3, DPC_SP_SOF,		DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_MTCMOS},		/* OFF Time 1 */

	/* DISP1 */
	{4, DPC_SP_FRAME_DONE,	DT_4,	DPC_DISP_VIDLE_MTCMOS_DISP1},
	{5, DPC_SP_SOF,		DT_5,	DPC_DISP_VIDLE_MTCMOS_DISP1},
	{6, DPC_SP_SOF,		DT_6,	DPC_DISP_VIDLE_MTCMOS_DISP1},	/* DISP1-TE */
	{7, DPC_SP_SOF,		DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_MTCMOS_DISP1},

	/* VDISP DVFS, follow DISP1 by default, or HRT BW */
	{8, DPC_SP_FRAME_DONE,	DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_VDISP_DVFS},
	{9, DPC_SP_SOF,		DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_VDISP_DVFS},
	{10, DPC_SP_SOF,	DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_VDISP_DVFS},

	/* HRT BW */
	{11, DPC_SP_FRAME_DONE,	DT_11,	DPC_DISP_VIDLE_HRT_BW},		/* OFF Time 0 */
	{12, DPC_SP_SOF,	DT_12,	DPC_DISP_VIDLE_HRT_BW},		/* ON Time */
	{13, DPC_SP_SOF,	DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_HRT_BW},		/* OFF Time 1 */

	/* SRT BW, follow HRT BW by default, or follow DISP1 */
	{14, DPC_SP_FRAME_DONE,	DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_SRT_BW},
	{15, DPC_SP_SOF,	DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_SRT_BW},
	{16, DPC_SP_SOF,	DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_SRT_BW},

	/* MMINFRA OFF, follow DISP1 by default, or HRT BW */
	{17, DPC_SP_FRAME_DONE,	DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_MMINFRA_OFF},
	{18, DPC_SP_SOF,	DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_MMINFRA_OFF},
	{19, DPC_SP_SOF,	DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_MMINFRA_OFF},

	/* INFRA OFF, follow DISP1 by default, or HRT BW */
	{20, DPC_SP_FRAME_DONE,	DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_INFRA_OFF},
	{21, DPC_SP_SOF,	DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_INFRA_OFF},
	{22, DPC_SP_SOF,	DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_INFRA_OFF},

	/* MAINPLL OFF, follow DISP1 by default, or HRT BW */
	{23, DPC_SP_FRAME_DONE,	DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_MAINPLL_OFF},
	{24, DPC_SP_SOF,	DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_MAINPLL_OFF},
	{25, DPC_SP_SOF,	DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_MAINPLL_OFF},

	/* MSYNC 2.0 */
	{26, DPC_SP_SOF,	DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_MSYNC2_0},
	{27, DPC_SP_SOF,	DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_MSYNC2_0},
	{28, DPC_SP_SOF,	DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_MSYNC2_0},

	/* RESERVED */
	{29, DPC_SP_FRAME_DONE,	3000,	DPC_DISP_VIDLE_RESERVED},
	{30, DPC_SP_SOF,	16000,	DPC_DISP_VIDLE_RESERVED},
	{31, DPC_SP_SOF,	DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_RESERVED},
};

static struct mtk_dpc_dt_usage mt6878_mml_vdo_dt_usage[DPC_MML_DT_CNT] = {
	/* MML1 */
	{32, DPC_SP_RROT_DONE,	DT_4,	DPC_MML_VIDLE_MTCMOS},		/* OFF Time 0 */
	{33, DPC_SP_SOF,	DT_1,	DPC_MML_VIDLE_MTCMOS},		/* ON Time */
	{34, DPC_SP_SOF,	DT_MAX_TIMEOUT,	DPC_MML_VIDLE_MTCMOS},		/* MML-TE */
	{35, DPC_SP_SOF,	DT_MAX_TIMEOUT,	DPC_MML_VIDLE_MTCMOS},		/* OFF Time 1 */

	/* VDISP DVFS, follow MML1 by default, or HRT BW */
	{36, DPC_SP_RROT_DONE,	DT_MAX_TIMEOUT,	DPC_MML_VIDLE_VDISP_DVFS},
	{37, DPC_SP_SOF,	DT_MAX_TIMEOUT,	DPC_MML_VIDLE_VDISP_DVFS},
	{38, DPC_SP_SOF,	DT_MAX_TIMEOUT,	DPC_MML_VIDLE_VDISP_DVFS},

	/* HRT BW */
	{39, DPC_SP_RROT_DONE,	DT_11,	DPC_MML_VIDLE_HRT_BW},		/* OFF Time 0 */
	{40, DPC_SP_SOF,	DT_12,	DPC_MML_VIDLE_HRT_BW},		/* ON Time */
	{41, DPC_SP_SOF,	DT_MAX_TIMEOUT,	DPC_MML_VIDLE_HRT_BW},		/* OFF Time 1 */

	/* SRT BW, follow HRT BW by default, or follow MML1 */
	{42, DPC_SP_RROT_DONE,	DT_MAX_TIMEOUT,	DPC_MML_VIDLE_SRT_BW},
	{43, DPC_SP_SOF,	DT_MAX_TIMEOUT,	DPC_MML_VIDLE_SRT_BW},
	{44, DPC_SP_SOF,	DT_MAX_TIMEOUT,	DPC_MML_VIDLE_SRT_BW},

	/* MMINFRA OFF, follow MML1 by default, or HRT BW */
	{45, DPC_SP_RROT_DONE,	DT_MAX_TIMEOUT,	DPC_MML_VIDLE_MMINFRA_OFF},
	{46, DPC_SP_SOF,	DT_MAX_TIMEOUT,	DPC_MML_VIDLE_MMINFRA_OFF},
	{47, DPC_SP_SOF,	DT_MAX_TIMEOUT,	DPC_MML_VIDLE_MMINFRA_OFF},

	/* INFRA OFF, follow MML1 by default, or HRT BW */
	{48, DPC_SP_RROT_DONE,	DT_MAX_TIMEOUT,	DPC_MML_VIDLE_INFRA_OFF},
	{49, DPC_SP_SOF,	DT_MAX_TIMEOUT,	DPC_MML_VIDLE_INFRA_OFF},
	{50, DPC_SP_SOF,	DT_MAX_TIMEOUT,	DPC_MML_VIDLE_INFRA_OFF},

	/* MAINPLL OFF, follow MML1 by default, or HRT BW */
	{51, DPC_SP_RROT_DONE,	DT_MAX_TIMEOUT,	DPC_MML_VIDLE_MAINPLL_OFF},
	{52, DPC_SP_SOF,	DT_MAX_TIMEOUT,	DPC_MML_VIDLE_MAINPLL_OFF},
	{53, DPC_SP_SOF,	DT_MAX_TIMEOUT,	DPC_MML_VIDLE_MAINPLL_OFF},

	/* RESERVED */
	{54, DPC_SP_RROT_DONE,	3000,	DPC_MML_VIDLE_RESERVED},
	{55, DPC_SP_SOF,	16000,	DPC_MML_VIDLE_RESERVED},
	{56, DPC_SP_SOF,	DT_MAX_TIMEOUT,	DPC_MML_VIDLE_RESERVED},
};

static struct mtk_dpc_dt_usage mt6878_disp_cmd_dt_usage[DPC_DISP_DT_CNT] = {
	/* OVL0/OVL1/DISP0 */
	{0, DPC_SP_FRAME_DONE,	DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_MTCMOS},		/* OFF Time 0 */
	{1, DPC_SP_TE,		DT_1,	DPC_DISP_VIDLE_MTCMOS},		/* ON Time */
	{2, DPC_SP_TE,		DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_MTCMOS},		/* Pre-TE */
	{3, DPC_SP_TE,		DT_3,	DPC_DISP_VIDLE_MTCMOS},		/* OFF Time 1 */

	/* DISP1 */
	{4, DPC_SP_FRAME_DONE,	DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_MTCMOS_DISP1},
	{5, DPC_SP_TE,		DT_5,	DPC_DISP_VIDLE_MTCMOS_DISP1},
	{6, DPC_SP_TE,		DT_6,	DPC_DISP_VIDLE_MTCMOS_DISP1},	/* DISP1-TE */
	{7, DPC_SP_TE,		DT_7,	DPC_DISP_VIDLE_MTCMOS_DISP1},

	/* VDISP DVFS, follow DISP1 by default, or HRT BW */
	{8, DPC_SP_FRAME_DONE,	DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_VDISP_DVFS},
	{9, DPC_SP_TE,		DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_VDISP_DVFS},
	{10, DPC_SP_TE,		DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_VDISP_DVFS},

	/* HRT BW */
	{11, DPC_SP_FRAME_DONE,	DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_HRT_BW},		/* OFF Time 0 */
	{12, DPC_SP_TE,		DT_12,	DPC_DISP_VIDLE_HRT_BW},		/* ON Time */
	{13, DPC_SP_TE,		DT_13,	DPC_DISP_VIDLE_HRT_BW},		/* OFF Time 1 */

	/* SRT BW, follow HRT BW by default, or follow DISP1 */
	{14, DPC_SP_FRAME_DONE,	DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_SRT_BW},
	{15, DPC_SP_TE,		DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_SRT_BW},
	{16, DPC_SP_TE,		DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_SRT_BW},

	/* MMINFRA OFF, follow DISP1 by default, or HRT BW */
	{17, DPC_SP_FRAME_DONE,	DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_MMINFRA_OFF},
	{18, DPC_SP_TE,		DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_MMINFRA_OFF},
	{19, DPC_SP_TE,		DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_MMINFRA_OFF},

	/* INFRA OFF, follow DISP1 by default, or HRT BW */
	{20, DPC_SP_FRAME_DONE,	DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_INFRA_OFF},
	{21, DPC_SP_TE,		DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_INFRA_OFF},
	{22, DPC_SP_TE,		DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_INFRA_OFF},

	/* MAINPLL OFF, follow DISP1 by default, or HRT BW */
	{23, DPC_SP_FRAME_DONE,	DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_MAINPLL_OFF},
	{24, DPC_SP_TE,		DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_MAINPLL_OFF},
	{25, DPC_SP_TE,		DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_MAINPLL_OFF},

	/* MSYNC 2.0 */
	{26, DPC_SP_TE,		DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_MSYNC2_0},
	{27, DPC_SP_TE,		DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_MSYNC2_0},
	{28, DPC_SP_TE,		DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_MSYNC2_0},

	/* RESERVED */
	{29, DPC_SP_FRAME_DONE,	3000,	DPC_DISP_VIDLE_RESERVED},
	{30, DPC_SP_TE,		16000,	DPC_DISP_VIDLE_RESERVED},
	{31, DPC_SP_TE,		DT_MAX_TIMEOUT,	DPC_DISP_VIDLE_RESERVED},
};

static struct mtk_dpc_dt_usage mt6878_mml_cmd_dt_usage[DPC_MML_DT_CNT] = {
	/* MML1 */
	{32, DPC_SP_RROT_DONE,	DT_MAX_TIMEOUT,	DPC_MML_VIDLE_MTCMOS},		/* OFF Time 0 */
	{33, DPC_SP_TE,		DT_1,	DPC_MML_VIDLE_MTCMOS},		/* ON Time */
	{34, DPC_SP_TE,		DT_MAX_TIMEOUT,	DPC_MML_VIDLE_MTCMOS},		/* MML-TE */
	{35, DPC_SP_TE,		DT_3,	DPC_MML_VIDLE_MTCMOS},		/* OFF Time 1 */

	/* VDISP DVFS, follow MML1 by default, or HRT BW */
	{36, DPC_SP_RROT_DONE,	DT_MAX_TIMEOUT,	DPC_MML_VIDLE_VDISP_DVFS},
	{37, DPC_SP_TE,		DT_MAX_TIMEOUT,	DPC_MML_VIDLE_VDISP_DVFS},
	{38, DPC_SP_TE,		DT_MAX_TIMEOUT,	DPC_MML_VIDLE_VDISP_DVFS},

	/* HRT BW */
	{39, DPC_SP_RROT_DONE,	DT_MAX_TIMEOUT,	DPC_MML_VIDLE_HRT_BW},		/* OFF Time 0 */
	{40, DPC_SP_TE,		DT_12,	DPC_MML_VIDLE_HRT_BW},		/* ON Time */
	{41, DPC_SP_TE,		DT_13,	DPC_MML_VIDLE_HRT_BW},		/* OFF Time 1 */

	/* SRT BW, follow HRT BW by default, or follow MML1 */
	{42, DPC_SP_RROT_DONE,	DT_MAX_TIMEOUT,	DPC_MML_VIDLE_SRT_BW},
	{43, DPC_SP_TE,		DT_MAX_TIMEOUT,	DPC_MML_VIDLE_SRT_BW},
	{44, DPC_SP_TE,		DT_MAX_TIMEOUT,	DPC_MML_VIDLE_SRT_BW},

	/* MMINFRA OFF, follow MML1 by default, or HRT BW */
	{45, DPC_SP_RROT_DONE,	DT_MAX_TIMEOUT,	DPC_MML_VIDLE_MMINFRA_OFF},
	{46, DPC_SP_TE,		DT_MAX_TIMEOUT,	DPC_MML_VIDLE_MMINFRA_OFF},
	{47, DPC_SP_TE,		DT_MAX_TIMEOUT,	DPC_MML_VIDLE_MMINFRA_OFF},

	/* INFRA OFF, follow MML1 by default, or HRT BW */
	{48, DPC_SP_RROT_DONE,	DT_MAX_TIMEOUT,	DPC_MML_VIDLE_INFRA_OFF},
	{49, DPC_SP_TE,		DT_MAX_TIMEOUT,	DPC_MML_VIDLE_INFRA_OFF},
	{50, DPC_SP_TE,		DT_MAX_TIMEOUT,	DPC_MML_VIDLE_INFRA_OFF},

	/* MAINPLL OFF, follow MML1 by default, or HRT BW */
	{51, DPC_SP_RROT_DONE,	DT_MAX_TIMEOUT,	DPC_MML_VIDLE_MAINPLL_OFF},
	{52, DPC_SP_TE,		DT_MAX_TIMEOUT,	DPC_MML_VIDLE_MAINPLL_OFF},
	{53, DPC_SP_TE,		DT_MAX_TIMEOUT,	DPC_MML_VIDLE_MAINPLL_OFF},

	/* RESERVED */
	{54, DPC_SP_RROT_DONE,	3000,	DPC_MML_VIDLE_RESERVED},
	{55, DPC_SP_TE,		16000,	DPC_MML_VIDLE_RESERVED},
	{56, DPC_SP_TE,		DT_MAX_TIMEOUT,	DPC_MML_VIDLE_RESERVED},
};

static unsigned int mt6989_get_sys_status(enum dpc_sys_status_id, unsigned int *status);
static unsigned int mt6878_get_sys_status(enum dpc_sys_status_id, unsigned int *status);

static struct mtk_dpc mt6989_dpc_driver_data = {
	.mmsys_id = MMSYS_MT6989,
	.disp_cmd_dt_usage = mt6989_disp_cmd_dt_usage,
	.mml_cmd_dt_usage = mt6989_mml_cmd_dt_usage,
	.disp_vdo_dt_usage = NULL,
	.mml_vdo_dt_usage = NULL,
	.get_sys_status = mt6989_get_sys_status,
	.mmdvfs_power_sync = false,
	.mmdvfs_settings_addr = NULL,
	.mmdvfs_settings_count = 0,
};

static struct mtk_dpc mt6878_dpc_driver_data = {
	.mmsys_id = MMSYS_MT6878,
	.disp_cmd_dt_usage = mt6878_disp_cmd_dt_usage,
	.mml_cmd_dt_usage = mt6878_mml_cmd_dt_usage,
	.disp_vdo_dt_usage = mt6878_disp_vdo_dt_usage,
	.mml_vdo_dt_usage = mt6878_mml_vdo_dt_usage,
	.get_sys_status = mt6878_get_sys_status,
	.mmdvfs_power_sync = true,
	.mmdvfs_settings_addr = mt6878_mmdvfs_settings_addr,
	.mmdvfs_settings_count = 0, //pending by mmdvfs ready
};

static void dpc_analysis(bool detail);
static void mtk_disp_enable_gce_vote(bool enable);
static const struct dpc_funcs funcs;

enum mtk_dpc_vidle_cap_id mtk_dpc_get_group_cap(unsigned int group)
{
	enum mtk_dpc_vidle_cap_id cap = DPC_VIDLE_CAP_COUNT;

	switch (group) {
	case DPC_DISP_VIDLE_MTCMOS:
	case DPC_DISP_VIDLE_MTCMOS_DISP1:
	case DPC_MML_VIDLE_MTCMOS:
		cap = DPC_VIDLE_MTCMOS_OFF;
		break;
	case DPC_DISP_VIDLE_VDISP_DVFS:
	case DPC_MML_VIDLE_VDISP_DVFS:
		cap = DPC_VIDLE_LOWER_VDISP_DVFS;
		break;
	case DPC_DISP_VIDLE_HRT_BW:
	case DPC_MML_VIDLE_HRT_BW:
		cap = DPC_VIDLE_ZERO_HRT_BW;
		break;
	case DPC_DISP_VIDLE_SRT_BW:
	case DPC_MML_VIDLE_SRT_BW:
		cap = DPC_VIDLE_ZERO_SRT_BW;
		break;
	case DPC_DISP_VIDLE_MMINFRA_OFF:
	case DPC_MML_VIDLE_MMINFRA_OFF:
		cap = DPC_VIDLE_MMINFRA_PLL_OFF;
		break;
	case DPC_DISP_VIDLE_INFRA_OFF:
	case DPC_MML_VIDLE_INFRA_OFF:
		cap = DPC_VIDLE_INFRA_REQ_OFF;
		break;
	case DPC_DISP_VIDLE_MAINPLL_OFF:
	case DPC_MML_VIDLE_MAINPLL_OFF:
		cap = DPC_VIDLE_MAIN_PLL_OFF;
		break;
	default:
		cap = DPC_VIDLE_CAP_COUNT;
		break;
	}

	return cap;
};

const char *mtk_dpc_get_group_cap_name(unsigned int group)
{
	enum mtk_dpc_vidle_cap_id cap = mtk_dpc_get_group_cap(group);

	if (cap >= DPC_VIDLE_CAP_COUNT)
		return "INVALID_CAP";

	return mtk_dpc_vidle_cap_name[cap];
}

int mtk_dpc_support_group(unsigned int group)
{
	enum mtk_dpc_vidle_cap_id cap = mtk_dpc_get_group_cap(group);

	if (cap >= DPC_VIDLE_CAP_COUNT)
		return 0;

	return mtk_dpc_support_cap(cap);
}

static void mtk_dpc_dump_caps(void)
{
	char buf[256] = {'\0'};
	char buf_mid[3] = {',', ' ', '\0'};
	int i = 0;
	unsigned long buf_len = sizeof(buf);

	for (i = 0; i < DPC_VIDLE_CAP_COUNT; i++) {
		if (mtk_dpc_support_cap(i)) {
			strncat(buf, buf_mid, (buf_len - strlen(buf) - 1));
			strncat(buf, mtk_dpc_vidle_cap_name[i],
				(buf_len - strlen(buf) - 1));

			if (strlen(buf) > 500)
				break;
		}
	}
	DPCDUMP("vidle mask:0x%x%s, len:%lu", g_priv->vidle_mask, buf, strlen(buf));
}

static unsigned int mtk_dpc_get_vidle_mask(const enum mtk_dpc_subsys subsys, bool en)
{
	unsigned int mask = 0x0;

	if (MTK_DPC_OF_DISP_SUBSYS(subsys)) {
		if (en) {
			if (!mtk_dpc_support_cap(DPC_VIDLE_APSRC_OFF))
				mask = mask | DPC_DDRSRC_DISP_MASK | DPC_EMIREQ_DISP_MASK;
			if (!mtk_dpc_support_cap(DPC_VIDLE_LOWER_VDISP_DVFS))
				mask |= DPC_VDISP_DVFS_DISP_MASK;
			if (!mtk_dpc_support_cap(DPC_VIDLE_ZERO_HRT_BW))
				mask |= DPC_HRTBW_DISP_MASK;
			if (!mtk_dpc_support_cap(DPC_VIDLE_ZERO_SRT_BW))
				mask |= DPC_SRTBW_DISP_MASK;
			if (!mtk_dpc_support_cap(DPC_VIDLE_MAIN_PLL_OFF))
				mask |= DPC_MAINPLL_OFF_DISP_MASK;
			if (!mtk_dpc_support_cap(DPC_VIDLE_MMINFRA_PLL_OFF))
				mask |= DPC_MMINFRA_OFF_DISP_MASK;
			if (!mtk_dpc_support_cap(DPC_VIDLE_INFRA_REQ_OFF))
				mask |= DPC_INFRA_OFF_DISP_MASK;
		} else {
			mask = U32_MAX;
		}
	} else if (MTK_DPC_OF_MML_SUBSYS(subsys)) {
		if (en) {
			if (!mtk_dpc_support_cap(DPC_VIDLE_APSRC_OFF))
				mask = mask | DPC_DDRSRC_MML_MASK | DPC_EMIREQ_DISP_MASK;
			if (!mtk_dpc_support_cap(DPC_VIDLE_LOWER_VDISP_DVFS))
				mask |= DPC_VDISP_DVFS_MML_MASK;
			if (!mtk_dpc_support_cap(DPC_VIDLE_ZERO_HRT_BW))
				mask |= DPC_HRTBW_MML_MASK;
			if (!mtk_dpc_support_cap(DPC_VIDLE_ZERO_SRT_BW))
				mask |= DPC_SRTBW_MML_MASK;
			if (!mtk_dpc_support_cap(DPC_VIDLE_MAIN_PLL_OFF))
				mask |= DPC_MAINPLL_OFF_MML_MASK;
			if (!mtk_dpc_support_cap(DPC_VIDLE_MMINFRA_PLL_OFF))
				mask |= DPC_MMINFRA_OFF_MML_MASK;
			if (!mtk_dpc_support_cap(DPC_VIDLE_INFRA_REQ_OFF))
				mask |= DPC_INFRA_OFF_MML_MASK;
		} else {
			mask = U32_MAX;
		}
	}

	return mask;
}

static inline int dpc_pm_ctrl(bool en)
{
	int ret = 0;
	u32 mminfra_hangfree_val = 0;

	if (!g_priv->pd_dev)
		return 0;

	if (en) {
		ret = pm_runtime_resume_and_get(g_priv->pd_dev);
		if (ret) {
			DPCERR("pm_runtime_resume_and_get failed skip_force_power(%u)",
				g_priv->skip_force_power);
			return -1;
		}

		/* disable devapc power check false alarm, */
		/* DPC address is bound by power of disp1 on current platform */
		if (g_priv->sys_va[MMINFRA_HANG_FREE]) {
			mminfra_hangfree_val = readl(g_priv->sys_va[MMINFRA_HANG_FREE]);
			writel(mminfra_hangfree_val & ~0x1, g_priv->sys_va[MMINFRA_HANG_FREE]);
		}
	} else
		pm_runtime_put_sync(g_priv->pd_dev);

	return ret;
}

static inline bool dpc_pm_check_and_get(void)
{
	if (!g_priv->pd_dev)
		return false;

	return pm_runtime_get_if_in_use(g_priv->pd_dev) > 0 ? true : false;
}

static int mtk_disp_wait_pwr_ack(const enum mtk_dpc_subsys subsys)
{
	int ret = 0;
	u32 value = 0;
	u32 addr = 0;

	if (mtk_dpc_support_cap(DPC_VIDLE_MTCMOS_OFF) == 0)
		return 0;

	switch (subsys) {
	case DPC_SUBSYS_DISP0:
		addr = DISP_REG_DPC_DISP0_DEBUG1;
		break;
	case DPC_SUBSYS_DISP1:
		addr = DISP_REG_DPC_DISP1_DEBUG1;
		break;
	case DPC_SUBSYS_OVL0:
		addr = DISP_REG_DPC_OVL0_DEBUG1;
		break;
	case DPC_SUBSYS_OVL1:
		addr = DISP_REG_DPC_OVL1_DEBUG1;
		break;
	case DPC_SUBSYS_MML1:
		addr = DISP_REG_DPC_MML1_DEBUG1;
		break;
	default:
		/* unknown subsys type */
		return ret;
	}

	/* delay_us, timeout_us */
	ret = readl_poll_timeout_atomic(dpc_base + addr, value, 0xB, 1, 200);
	if (ret < 0)
		DPCERR("wait subsys(%d) power on timeout", subsys);

	return ret;
}

static unsigned int mt6989_get_sys_status(enum dpc_sys_status_id id, unsigned int *status)
{
	unsigned int mask = 0, value = 0;
	void __iomem *addr = NULL;

	switch (id) {
	case SYS_POWER_ACK_MMINFRA:
		mask = SPM_PWR_FLD_MMINFRA_MASK_MT6989;
		if (g_priv->sys_va[SPM_BASE])
			addr = g_priv->sys_va[SPM_BASE] + SPM_PWR_STATUS_MT6989;
		break;
	case SYS_POWER_ACK_DISP1_SUBSYS:
		mask = SPM_PWR_FLD_DISP1_MASK_MT6989;
		if (g_priv->sys_va[SPM_BASE])
			addr = g_priv->sys_va[SPM_BASE] + SPM_PWR_STATUS_MT6989;
		break;
	case SYS_POWER_ACK_MML1_SUBSYS:
		mask = SPM_PWR_FLD_MML1_MASK_MT6989;
		if (g_priv->sys_va[SPM_BASE])
			addr = g_priv->sys_va[SPM_BASE] + SPM_PWR_STATUS_MT6989;
		break;
	case SYS_POWER_ACK_DPC:
		mask = SPM_PWR_FLD_DISP_VCORE_MASK_MT6989;
		if (g_priv->sys_va[SPM_BASE])
			addr = g_priv->sys_va[SPM_BASE] + SPM_PWR_STATUS_MT6989;
		break;
	case SYS_STATE_MMINFRA:
		mask = SPM_REQ_MMINFRA_STATE_MT6989;
		if (g_priv->sys_va[SPM_BASE])
			addr = g_priv->sys_va[SPM_BASE] + SPM_REQ_STA_5_MT6989;
		break;
	case SYS_STATE_APSRC:
		mask = SPM_REQ_APSRC_STATE_MT6989;
		if (g_priv->sys_va[SPM_BASE])
			addr = g_priv->sys_va[SPM_BASE] + SPM_REQ_STA_4_MT6989;
		break;
	case SYS_STATE_EMI:
		mask = SPM_REQ_EMI_STATE_MT6989;
		if (g_priv->sys_va[SPM_BASE])
			addr = g_priv->sys_va[SPM_BASE] + SPM_REQ_STA_5_MT6989;
		break;
	case SYS_STATE_HRT_BW:
		mask = VCORE_DVFSRC_HRT_BW_MASK;
		if (g_priv->sys_va[VCORE_DVFSRC_DEBUG])
			addr = g_priv->sys_va[VCORE_DVFSRC_DEBUG];
		break;
	case SYS_STATE_SRT_BW:
		mask = VCORE_DVFSRC_SRT_BW_MASK;
		if (g_priv->sys_va[VCORE_DVFSRC_DEBUG])
			addr = g_priv->sys_va[VCORE_DVFSRC_DEBUG];
		break;
	case SYS_STATE_VLP_VOTE:
		mask = U32_MAX;
		if (g_priv->sys_va[VLP_BASE])
			addr = g_priv->sys_va[VLP_BASE] + VLP_DISP_SW_VOTE_CON;
		break;
	case SYS_STATE_VDISP_DVFS:
		mask = 0x3;
		if (g_priv->sys_va[VDISP_DVFSRC_DEBUG])
			addr = g_priv->sys_va[VDISP_DVFSRC_DEBUG];
		break;
	case SYS_VALUE_VDISP_DVFS_LEVEL:
		if (!debug_vdisp_level)
			return 0;
		mask = 0x1c;
		if (g_priv->sys_va[VDISP_DVFSRC_DEBUG])
			addr = g_priv->sys_va[VDISP_DVFSRC_DEBUG];
		break;
	default:
		return 0;
	}

	if (addr == NULL)
		return 0;

	value = readl(addr);
	if (status)
		*status = value;

	return (value & mask);
}

static unsigned int mt6878_get_sys_status(enum dpc_sys_status_id id, unsigned int *status)
{
	unsigned int mask = 0, value = 0;
	void __iomem *addr = NULL;

	switch (id) {
	case SYS_POWER_ACK_MMINFRA:
		mask = SPM_PWR_FLD_MMINFRA_MASK_MT6878;
		if (g_priv->sys_va[SPM_BASE])
			addr = g_priv->sys_va[SPM_BASE] + SPM_PWR_STATUS_MT6878;
		break;
	case SYS_POWER_ACK_DISP1_SUBSYS:
	case SYS_POWER_ACK_MML1_SUBSYS:
	case SYS_POWER_ACK_DPC:
		mask = SPM_PWR_FLD_DISP_VCORE_MASK_MT6878;
		if (g_priv->sys_va[SPM_BASE])
			addr = g_priv->sys_va[SPM_BASE] + SPM_PWR_STATUS_MT6878;
		break;
	case SYS_STATE_MMINFRA:
		mask = SPM_REQ_INFRA_STATE_MT6878;
		if (g_priv->sys_va[SPM_BASE])
			addr = g_priv->sys_va[SPM_BASE] + SPM_REQ_STA_4_MT6878;
		break;
	case SYS_STATE_APSRC:
		mask = SPM_REQ_APSRC_STATE_MT6878;
		if (g_priv->sys_va[SPM_BASE])
			addr = g_priv->sys_va[SPM_BASE] + SPM_REQ_STA_4_MT6878;
		break;
	case SYS_STATE_EMI:
		mask = SPM_REQ_EMI_STATE_MT6878;
		if (g_priv->sys_va[SPM_BASE])
			addr = g_priv->sys_va[SPM_BASE] + SPM_REQ_STA_4_MT6878;
		break;
	case SYS_STATE_HRT_BW:
		mask = VCORE_DVFSRC_HRT_BW_MASK;
		if (g_priv->sys_va[VCORE_DVFSRC_DEBUG])
			addr = g_priv->sys_va[VCORE_DVFSRC_DEBUG];
		break;
	case SYS_STATE_SRT_BW:
		mask = VCORE_DVFSRC_SRT_BW_MASK;
		if (g_priv->sys_va[VCORE_DVFSRC_DEBUG])
			addr = g_priv->sys_va[VCORE_DVFSRC_DEBUG];
		break;
	case SYS_STATE_VLP_VOTE:
		break;
#ifdef ENABLE_DEVAPC_PERMISSION_OF_HFRP
	case SYS_STATE_VDISP_DVFS:
		mask = 0x3;
		if (g_priv->sys_va[VDISP_DVFSRC_DEBUG])
			addr = g_priv->sys_va[VDISP_DVFSRC_DEBUG];
		break;
#endif
	case SYS_VALUE_VDISP_DVFS_LEVEL:
		if (!debug_vdisp_level)
			return 0;
		mask = 0x1c;
		if (g_priv->sys_va[VDISP_DVFSRC_DEBUG])
			addr = g_priv->sys_va[VDISP_DVFSRC_DEBUG];
		break;
	default:
		return 0;
	}

	if (addr == NULL)
		return 0;

	value = readl(addr);
	if (status)
		*status = value;

	return (value & mask);
}

static int mtk_dpc_vidle_timeout_monitor_thread(void *data)
{
	unsigned int value = 0;
	int ret = 0;

	while (!kthread_should_stop()) {
		ret = wait_event_interruptible(
			g_priv->dpc_mtcmos_wq,
			atomic_read(&g_priv->dpc_mtcmos_timeout));

		atomic_set(&g_priv->dpc_mtcmos_timeout, 0);

		if (g_priv->get_sys_status) {
			g_priv->get_sys_status(SYS_POWER_ACK_MMINFRA, &value);
			dpc_mmp(window, MMPROFILE_FLAG_PULSE, value, 0);
		}
		dpc_analysis(true);
	}

	return 0;
}

static void mtk_dpc_timer_fun(struct timer_list *timer)
{
	if (g_priv) {
		atomic_set(&g_priv->dpc_mtcmos_timeout, 1);
		wake_up_interruptible(&g_priv->dpc_mtcmos_wq);
	}
}

static void mtk_init_dpc_timer(void)
{
	if (!debug_vidle_timeout || !g_te_duration)
		return;

	DPCDUMP("g_te_duration:%uus", g_te_duration);
	timer_setup(&g_priv->dpc_timer, mtk_dpc_timer_fun, 0);
	mod_timer(&g_priv->dpc_timer,
		jiffies + msecs_to_jiffies(1000));
}

static void mtk_dpc_update_vlp_state(unsigned int user,
		unsigned int val, bool reset)
{
	static u32 vlp_state;
	unsigned long flags = 0;

	//ignore vlp vote if mminfra always on
	if (mtk_dpc_support_cap(DPC_VIDLE_MMINFRA_PLL_OFF) == 0)
		return;
	if (debug_mmp == 0)
		return;

	spin_lock_irqsave(&vlp_lock, flags);
	if (vlp_state > 0 && reset)
		dpc_mmp(vlp_vote, MMPROFILE_FLAG_END, user, val);
	else if (vlp_state > 0 && val == 0) {
		dpc_mmp(vlp_vote, MMPROFILE_FLAG_END, user, val);
		if (g_priv)
			atomic_set(&g_priv->dpc_state, DPC_STATE_OFF);
	} else if (vlp_state == 0 && val > 0) {
		dpc_mmp(vlp_vote, MMPROFILE_FLAG_START, user, val);
		if (g_priv)
			atomic_set(&g_priv->dpc_state, DPC_STATE_ON);
	}

	vlp_state = val;
	spin_unlock_irqrestore(&vlp_lock, flags);

	if (g_priv)
		wake_up_interruptible(&g_priv->dpc_state_wq);

}

static void mtk_dpc_idle_ratio_debug(enum dpc_idle_cmd cmd)
{
	static unsigned long long start, end;
	unsigned long long period = 0;
	char buf[256] = {'\0'};
	unsigned long buf_len = sizeof(buf);
	enum dpc_idle_id id;
	unsigned int ratio = 0;
	unsigned long flags = 0;
	int len = 0;

	if (cmd >= DPC_VIDLE_CMD_COUNT)
		return;

	spin_lock_irqsave(&dpc_state_lock, flags);
	switch (cmd) {
	case DPC_VIDLE_RATIO_START:
		for (id = 0; id < DPC_IDLE_ID_MAX; id++) {
			g_idle_period[id] = 0;
			g_idle_start[id] = 0;
		}
		start = sched_clock();
		end = start;
		g_idle_ratio_debug = 1;
		break;
	case DPC_VIDLE_RATIO_STOP:
		if (g_idle_ratio_debug == 0)
			break;

		end = sched_clock();
		g_idle_ratio_debug = 0;
		break;
	case DPC_VIDLE_RATIO_DUMP:
		if (start && end <= start)
			period = sched_clock() - start;
		else if (start)
			period = end - start;

		if (period < 1000000000)
			break;

		for (id = 0; id < DPC_IDLE_ID_MAX; id++) {
			char tmp[64] = {'\0'};

			ratio = g_idle_period[id] * 100 / period;
			len = snprintf(tmp, 63, " %s:%u%%,",
				mtk_dpc_idle_name[id], ratio);
			if (len >= 0 && len < 64)
				strncat(buf, tmp, (buf_len - strlen(buf) - 1));
			switch (id) {
			case DPC_IDLE_ID_MTCMOS_OFF_OVL0:
				dpc_mmp(mtcmos_ovl0, MMPROFILE_FLAG_PULSE, ratio, 0x1d1e0ff);
				break;
			case DPC_IDLE_ID_MTCMOS_OFF_MML1:
				dpc_mmp(mtcmos_mml1, MMPROFILE_FLAG_PULSE, ratio, 0x1d1e0ff);
				break;
			case DPC_IDLE_ID_MTCMOS_OFF_DISP1:
				dpc_mmp(mtcmos_off, MMPROFILE_FLAG_PULSE, ratio, 0x1d1e0ff);
				break;
			case DPC_IDLE_ID_MMINFRA_OFF:
				dpc_mmp(mminfra_off, MMPROFILE_FLAG_PULSE, ratio, 0x1d1e0ff);
				break;
			case DPC_IDLE_ID_APSRC_OFF:
				dpc_mmp(apsrc_off, MMPROFILE_FLAG_PULSE, ratio, 0x1d1e0ff);
				break;
			case DPC_IDLE_ID_DVFS_OFF:
				dpc_mmp(dvfs_off, MMPROFILE_FLAG_PULSE, ratio, 0x1d1e0ff);
				break;
			case DPC_IDLE_ID_VDISP_OFF:
				dpc_mmp(vdisp_off, MMPROFILE_FLAG_PULSE, ratio, 0x1d1e0ff);
				break;
			case DPC_IDLE_ID_WINDOW_DISP:
				dpc_mmp(disp_window, MMPROFILE_FLAG_PULSE, ratio, 0x1d1e0ff);
				break;
			case DPC_IDLE_ID_WINDOW_MML:
				dpc_mmp(mml_window, MMPROFILE_FLAG_PULSE, ratio, 0x1d1e0ff);
				break;
			case DPC_IDLE_ID_WINDOW:
				dpc_mmp(window, MMPROFILE_FLAG_PULSE, ratio, 0x1d1e0ff);
				break;
			default:
				break;
			}
		}

		DPCDUMP("period:%llums(%llu~%llu),panel:%d,dur:%u,mode(%d,%d),ratio:%s",
			period / 1000000, start / 1000000, end / 1000000,
			g_panel_type, g_te_duration, atomic_read(&g_disp_mode),
			atomic_read(&g_mml_mode), buf);
		start = 0;
		end = 0;
		break;
	default:
		break;
	}
	spin_unlock_irqrestore(&dpc_state_lock, flags);
}

static void mtk_reset_dpc_state(void)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&dpc_state_lock, flags);
	if (g_vidle_events == 0)
		goto out;

	if (g_vidle_events & DPC_VIDLE_WINDOW_MASK)
		dpc_mmp(window, MMPROFILE_FLAG_END, U32_MAX, 0);
	if (g_vidle_events & DPC_VIDLE_DISP_WINDOW)
		dpc_mmp(disp_window, MMPROFILE_FLAG_END, U32_MAX, 0);
	if (g_vidle_events & DPC_VIDLE_MMINFRA_MASK)
		dpc_mmp(mminfra_off, MMPROFILE_FLAG_END, U32_MAX, 0);
	if (g_vidle_events & DPC_VIDLE_APSRC_MASK)
		dpc_mmp(apsrc_off, MMPROFILE_FLAG_END, U32_MAX, 0);
	//if (g_vidle_events & DPC_VIDLE_EMI_MASK)
		//dpc_mmp(emi_off, MMPROFILE_FLAG_END, U32_MAX, 0);
	if (debug_mtcmos_off) {
		if (g_vidle_events & DPC_VIDLE_OVL0_MASK)
			dpc_mmp(mtcmos_ovl0, MMPROFILE_FLAG_END, U32_MAX, 0);
		if (g_vidle_events & DPC_VIDLE_DISP1_MASK)
			dpc_mmp(mtcmos_off, MMPROFILE_FLAG_END, U32_MAX, 0);
		if (g_vidle_events & DPC_VIDLE_MML1_MASK)
			dpc_mmp(mtcmos_mml1, MMPROFILE_FLAG_END, U32_MAX, 0);
	}
	if (g_vidle_events & DPC_VIDLE_MML_DC_WINDOW)
		dpc_mmp(mml_window, MMPROFILE_FLAG_END, U32_MAX, 0);
	if (g_vidle_events & DPC_VIDLE_BW_MASK)
		dpc_mmp(dvfs_off, MMPROFILE_FLAG_END, U32_MAX, 0);
	if (g_vidle_events & DPC_VIDLE_VDISP_MASK)
		dpc_mmp(vdisp_off, MMPROFILE_FLAG_END, U32_MAX, 0);
	g_vidle_events = 0;

out:
	spin_unlock_irqrestore(&dpc_state_lock, flags);
}

static void mtk_update_dpc_state(unsigned int mask, bool off)
{
	unsigned int state = 0, value = 0, value1 = 0;
	unsigned long flags = 0;
	static unsigned int hrt_bw, mmclk;
	static int dpc_dump_cnt = -1;

	if (debug_mmp == 0)
		return;

	spin_lock_irqsave(&dpc_state_lock, flags);
	if (mask & DPC_VIDLE_DISP_WINDOW) {
		if (off && !(g_vidle_events & DPC_VIDLE_DISP_WINDOW)) {
			dpc_mmp(disp_window, MMPROFILE_FLAG_START, 0, off);
			g_vidle_events |= DPC_VIDLE_DISP_WINDOW;
			if (g_idle_ratio_debug)
				g_idle_start[DPC_IDLE_ID_WINDOW_DISP] = sched_clock();
			if ((g_vidle_events & atomic_read(&g_vidle_window)) ==
				atomic_read(&g_vidle_window) &&
				(atomic_read(&g_vidle_window) & DPC_VIDLE_DISP_WINDOW) &&
				!(g_vidle_events & DPC_VIDLE_WINDOW_MASK)) {
				dpc_mmp(window, MMPROFILE_FLAG_START, mask, off);
				g_vidle_events |= DPC_VIDLE_WINDOW_MASK;
				if (g_idle_ratio_debug)
					g_idle_start[DPC_IDLE_ID_WINDOW] = sched_clock();
			}
		} else if (!off && (g_vidle_events & DPC_VIDLE_DISP_WINDOW)) {
			dpc_mmp(disp_window, MMPROFILE_FLAG_END, 0, off);
			g_vidle_events &= ~DPC_VIDLE_DISP_WINDOW;
			if (g_idle_ratio_debug && g_idle_start[DPC_IDLE_ID_WINDOW_DISP])
				g_idle_period[DPC_IDLE_ID_WINDOW_DISP] += sched_clock() -
					g_idle_start[DPC_IDLE_ID_WINDOW_DISP];
			if (g_vidle_events & DPC_VIDLE_WINDOW_MASK) {
				dpc_mmp(window, MMPROFILE_FLAG_END, mask, off);
				g_vidle_events &= ~DPC_VIDLE_WINDOW_MASK;
				if (g_idle_ratio_debug && g_idle_start[DPC_IDLE_ID_WINDOW])
					g_idle_period[DPC_IDLE_ID_WINDOW] += sched_clock() -
						g_idle_start[DPC_IDLE_ID_WINDOW];
			}
		}
	}
	if (mask & DPC_VIDLE_OVL0_MASK) {
		if (off && !(g_vidle_events & DPC_VIDLE_OVL0_MASK)) {
			dpc_mmp(mtcmos_ovl0, MMPROFILE_FLAG_START, 0, off);
			g_vidle_events |= DPC_VIDLE_OVL0_MASK;
			if (g_idle_ratio_debug)
				g_idle_start[DPC_IDLE_ID_MTCMOS_OFF_OVL0] = sched_clock();
		} else if (!off && (g_vidle_events & DPC_VIDLE_OVL0_MASK)) {
			dpc_mmp(mtcmos_ovl0, MMPROFILE_FLAG_END, 0, off);
			g_vidle_events &= ~DPC_VIDLE_OVL0_MASK;
			if (g_idle_ratio_debug && g_idle_start[DPC_IDLE_ID_MTCMOS_OFF_OVL0])
				g_idle_period[DPC_IDLE_ID_MTCMOS_OFF_OVL0] += sched_clock() -
					g_idle_start[DPC_IDLE_ID_MTCMOS_OFF_OVL0];
		} else
			dpc_mmp(mtcmos_ovl0, MMPROFILE_FLAG_PULSE, 0, off);
	}
	if (mask & DPC_VIDLE_DISP1_MASK) {
		if (off && !(g_vidle_events & DPC_VIDLE_DISP1_MASK)) {
			dpc_mmp(mtcmos_off, MMPROFILE_FLAG_START, 0, off);
			g_vidle_events |= DPC_VIDLE_DISP1_MASK;
			if (g_idle_ratio_debug)
				g_idle_start[DPC_IDLE_ID_MTCMOS_OFF_DISP1] = sched_clock();

			if (debug_vidle_timeout && g_te_duration)
				mod_timer(&g_priv->dpc_timer,
					jiffies + msecs_to_jiffies(g_te_duration));
		} else if (!off && (g_vidle_events & DPC_VIDLE_DISP1_MASK)) {
			dpc_mmp(mtcmos_off, MMPROFILE_FLAG_END, 0, off);
			g_vidle_events &= ~DPC_VIDLE_DISP1_MASK;
			if (g_idle_ratio_debug && g_idle_start[DPC_IDLE_ID_MTCMOS_OFF_DISP1])
				g_idle_period[DPC_IDLE_ID_MTCMOS_OFF_DISP1] += sched_clock() -
					g_idle_start[DPC_IDLE_ID_MTCMOS_OFF_DISP1];
		} else
			dpc_mmp(mtcmos_off, MMPROFILE_FLAG_PULSE, 0, off);
	}
	if (mask & DPC_VIDLE_MML1_MASK) {
		if (off && !(g_vidle_events & DPC_VIDLE_MML1_MASK)) {
			dpc_mmp(mtcmos_mml1, MMPROFILE_FLAG_START, 0, MML_DPC_INT_MML1_OFF);
			g_vidle_events |= DPC_VIDLE_MML1_MASK;
			if (g_idle_ratio_debug)
				g_idle_start[DPC_IDLE_ID_MTCMOS_OFF_MML1] = sched_clock();
		} else if (!off && (g_vidle_events & DPC_VIDLE_MML1_MASK)) {
			dpc_mmp(mtcmos_mml1, MMPROFILE_FLAG_END, 0, MML_DPC_INT_MML1_ON);
			g_vidle_events &= ~DPC_VIDLE_MML1_MASK;
			if (g_idle_ratio_debug && g_idle_start[DPC_IDLE_ID_MTCMOS_OFF_MML1])
				g_idle_period[DPC_IDLE_ID_MTCMOS_OFF_MML1] += sched_clock() -
					g_idle_start[DPC_IDLE_ID_MTCMOS_OFF_MML1];
		} else
			dpc_mmp(mtcmos_mml1, MMPROFILE_FLAG_PULSE, 0, MML_DPC_INT_MML1_OFF);
	}
	if ((mask & DPC_VIDLE_MMINFRA_MASK) &&
		g_priv->sys_va[SPM_BASE] &&
		g_priv->get_sys_status &&
		mtk_dpc_support_cap(DPC_VIDLE_MMINFRA_PLL_OFF)) {
		state = g_priv->get_sys_status(SYS_STATE_MMINFRA, &value);
		if (!off && state &&
			(g_vidle_events & DPC_VIDLE_MMINFRA_MASK)) {
			g_priv->get_sys_status(SYS_POWER_ACK_MMINFRA, &value1);
			dpc_mmp(mminfra_off, MMPROFILE_FLAG_END, value, value1);
			g_vidle_events &= ~DPC_VIDLE_MMINFRA_MASK;
			if (g_idle_ratio_debug && g_idle_start[DPC_IDLE_ID_MMINFRA_OFF])
				g_idle_period[DPC_IDLE_ID_MMINFRA_OFF] += sched_clock() -
					g_idle_start[DPC_IDLE_ID_MMINFRA_OFF];
		} else if (off && !(g_vidle_events & DPC_VIDLE_MMINFRA_MASK)) {
			if (state == 0) {
				dpc_mmp(mminfra_off, MMPROFILE_FLAG_START, off, value);
				g_vidle_events |= DPC_VIDLE_MMINFRA_MASK;
				if (g_idle_ratio_debug)
					g_idle_start[DPC_IDLE_ID_MMINFRA_OFF] = sched_clock();
			} else {
				//dpc_mmp(mminfra_off, MMPROFILE_FLAG_PULSE, off, value);
			}
		}
	}
	if ((mask & DPC_VIDLE_APSRC_MASK) &&
		g_priv->sys_va[SPM_BASE] &&
		g_priv->get_sys_status &&
		mtk_dpc_support_cap(DPC_VIDLE_APSRC_OFF)) {
		state = g_priv->get_sys_status(SYS_STATE_APSRC, &value);
		if (!off && state &&
			(g_vidle_events & DPC_VIDLE_APSRC_MASK)) {
			dpc_mmp(apsrc_off, MMPROFILE_FLAG_END, off, value);
			g_vidle_events &= ~DPC_VIDLE_APSRC_MASK;
			if (g_idle_ratio_debug && g_idle_start[DPC_IDLE_ID_APSRC_OFF])
				g_idle_period[DPC_IDLE_ID_APSRC_OFF] += sched_clock() -
					g_idle_start[DPC_IDLE_ID_APSRC_OFF];
			if (dpc_dump_cnt == 0) {
				dpc_analysis(true);
				dpc_dump_cnt = -1;
			} else if (dpc_dump_cnt > 0){
				dpc_dump_cnt--;
			}
		} else if (off && !(g_vidle_events & DPC_VIDLE_APSRC_MASK)) {
			if (state == 0) {
				dpc_mmp(apsrc_off, MMPROFILE_FLAG_START, off, value);
				g_vidle_events |= DPC_VIDLE_APSRC_MASK;
				if (g_idle_ratio_debug)
					g_idle_start[DPC_IDLE_ID_APSRC_OFF] = sched_clock();
			} else {
				//dpc_mmp(apsrc_off, MMPROFILE_FLAG_PULSE, off, value);
			}
		}
	}
	if ((mask & DPC_VIDLE_EMI_MASK) &&
		g_priv->sys_va[SPM_BASE] &&
		g_priv->get_sys_status &&
		mtk_dpc_support_cap(DPC_VIDLE_APSRC_OFF)) {
		state = g_priv->get_sys_status(SYS_STATE_EMI, &value);
		if (!off && state &&
			(g_vidle_events & DPC_VIDLE_EMI_MASK)) {
			//dpc_mmp(emi_off, MMPROFILE_FLAG_END, off, value);
			g_vidle_events &= ~DPC_VIDLE_EMI_MASK;
		} else if (off && state == 0 &&
			!(g_vidle_events & DPC_VIDLE_EMI_MASK)) {
			if (state == 0) {
				//dpc_mmp(emi_off, MMPROFILE_FLAG_START, off, value);
				g_vidle_events |= DPC_VIDLE_EMI_MASK;
			} else {
				//dpc_mmp(emi_off, MMPROFILE_FLAG_PULSE, off, value);
			}
		}
	}
	if (debug_vdisp_level && (mask & DPC_VIDLE_VDISP_MASK) &&
		g_priv->sys_va[VDISP_DVFSRC_DEBUG] && g_priv->get_sys_status &&
		mtk_dpc_support_cap(DPC_VIDLE_LOWER_VDISP_DVFS)) {
		state = g_priv->get_sys_status(SYS_VALUE_VDISP_DVFS_LEVEL, &value);
		if (!off)
			mmclk = state;
		if (!off && state > 0 &&
			(g_vidle_events & DPC_VIDLE_VDISP_MASK)) {
			dpc_mmp(vdisp_off, MMPROFILE_FLAG_END, off, value);
			g_vidle_events &= ~DPC_VIDLE_VDISP_MASK;
			if (g_idle_ratio_debug && g_idle_start[DPC_IDLE_ID_VDISP_OFF])
				g_idle_period[DPC_IDLE_ID_VDISP_OFF] += sched_clock() -
					g_idle_start[DPC_IDLE_ID_VDISP_OFF];
		} else if (off && state < mmclk) {
			if (!(g_vidle_events & DPC_VIDLE_VDISP_MASK)) {
				dpc_mmp(vdisp_off, MMPROFILE_FLAG_START, off, value);
				g_vidle_events |= DPC_VIDLE_VDISP_MASK;
				if (g_idle_ratio_debug)
					g_idle_start[DPC_IDLE_ID_VDISP_OFF] = sched_clock();
			} else if (state != mmclk)
				dpc_mmp(vdisp_off, MMPROFILE_FLAG_PULSE, off, value);
			mmclk = state;
		} else if (unlikely(debug_dvfs))
			dpc_mmp(vdisp_off, MMPROFILE_FLAG_PULSE, off, value);
	}
	if ((mask & DPC_VIDLE_BW_MASK) &&
		g_priv->sys_va[VCORE_DVFSRC_DEBUG] &&
		g_priv->get_sys_status &&
		mtk_dpc_support_cap(DPC_VIDLE_ZERO_HRT_BW)) {
		state = g_priv->get_sys_status(SYS_STATE_HRT_BW, &value);
		if (!off)
			hrt_bw = state;
		if (!off && state > 0 &&
			(g_vidle_events & DPC_VIDLE_BW_MASK)) {
			dpc_mmp(dvfs_off, MMPROFILE_FLAG_END, off, value);
			g_vidle_events &= ~DPC_VIDLE_BW_MASK;
			if (g_idle_ratio_debug && g_idle_start[DPC_IDLE_ID_DVFS_OFF])
				g_idle_period[DPC_IDLE_ID_DVFS_OFF] += sched_clock() -
					g_idle_start[DPC_IDLE_ID_DVFS_OFF];
		} else if (off && (state == 0 || state < hrt_bw)) {
			if (!(g_vidle_events & DPC_VIDLE_BW_MASK)) {
				dpc_mmp(dvfs_off, MMPROFILE_FLAG_START, off, value);
				g_vidle_events |= DPC_VIDLE_BW_MASK;
				if (g_idle_ratio_debug)
					g_idle_start[DPC_IDLE_ID_DVFS_OFF] = sched_clock();
			} else if (state != hrt_bw)
				dpc_mmp(dvfs_off, MMPROFILE_FLAG_PULSE, off, value);
			hrt_bw = state;
		} else if (unlikely(debug_dvfs))
			dpc_mmp(dvfs_off, MMPROFILE_FLAG_PULSE, off, value);
	}
	if (mask & DPC_VIDLE_MML_DC_WINDOW) {
		if (off && !(g_vidle_events & DPC_VIDLE_MML_DC_WINDOW)) {
			dpc_mmp(mml_window, MMPROFILE_FLAG_START, 0, off);
			g_vidle_events |= DPC_VIDLE_MML_DC_WINDOW;
			if (g_idle_ratio_debug)
				g_idle_start[DPC_IDLE_ID_WINDOW_MML] = sched_clock();
			if ((g_vidle_events & atomic_read(&g_vidle_window)) ==
				atomic_read(&g_vidle_window) &&
				(atomic_read(&g_vidle_window) & DPC_VIDLE_DISP_WINDOW) &&
				!(g_vidle_events & DPC_VIDLE_WINDOW_MASK)) {
				dpc_mmp(window, MMPROFILE_FLAG_START, mask, off);
				g_vidle_events |= DPC_VIDLE_WINDOW_MASK;
				if (g_idle_ratio_debug)
					g_idle_start[DPC_IDLE_ID_WINDOW] = sched_clock();
			}
		} else if (!off && (g_vidle_events & DPC_VIDLE_MML_DC_WINDOW)) {
			dpc_mmp(mml_window, MMPROFILE_FLAG_END, 0, off);
			g_vidle_events &= ~DPC_VIDLE_MML_DC_WINDOW;
			if (g_idle_ratio_debug && g_idle_start[DPC_IDLE_ID_WINDOW_MML])
				g_idle_period[DPC_IDLE_ID_WINDOW_MML] += sched_clock() -
					g_idle_start[DPC_IDLE_ID_WINDOW_MML];
			if (g_vidle_events & DPC_VIDLE_WINDOW_MASK) {
				dpc_mmp(window, MMPROFILE_FLAG_END, mask, off);
				g_vidle_events &= ~DPC_VIDLE_WINDOW_MASK;
				if (g_idle_ratio_debug && g_idle_start[DPC_IDLE_ID_WINDOW])
					g_idle_period[DPC_IDLE_ID_WINDOW] += sched_clock() -
						g_idle_start[DPC_IDLE_ID_WINDOW];
			}
		}
	}
	spin_unlock_irqrestore(&dpc_state_lock, flags);
}

static void dpc_dt_enable(u16 dt, bool en)
{
	u32 value = 0;
	u32 addr = 0;

	if (dt < DPC_DISP_DT_CNT) {
		addr = DISP_REG_DPC_DISP_DT_EN;
		if (debug_mmp)
			dpc_mmp(disp_dt, MMPROFILE_FLAG_PULSE, BIT(dt), en);
	} else {
		addr = DISP_REG_DPC_MML_DT_EN;
		dt -= DPC_DISP_DT_CNT;
		if (debug_mmp)
			dpc_mmp(mml_dt, MMPROFILE_FLAG_PULSE, BIT(dt), en);
	}

	value = readl(dpc_base + addr);
	if (en)
		writel(BIT(dt) | value, dpc_base + addr);
	else
		writel(~BIT(dt) & value, dpc_base + addr);
}

static void dpc_dt_set(u16 dt, u32 us)
{
	u32 value = us * 26;	/* 26M base, 20 bits range, 38.46 ns ~ 38.46 ms*/
	u32 max_counter = 0xFFFFF;

	if (value > max_counter)
		value = max_counter;

	writel(value, dpc_base + DISP_REG_DPC_DTx_COUNTER(dt));
}

static void dpc_dt_sw_trig(u16 dt)
{
	DPCFUNC("dt(%u)", dt);
	writel(1, dpc_base + DISP_REG_DPC_DTx_SW_TRIG(dt));

	if (debug_mmp && dt < DPC_DISP_DT_CNT)
		dpc_mmp(disp_dt, MMPROFILE_FLAG_PULSE, BIT(dt), 0xffffffff);
	else if (debug_mmp && dt >= DPC_DISP_DT_CNT)
		dpc_mmp(mml_dt, MMPROFILE_FLAG_PULSE, BIT(dt - DPC_DISP_DT_CNT), 0xffffffff);
}

static void dpc_dt_update_table(u16 dt, u32 us)
{
	if (g_disp_dt_usage == NULL || g_mml_dt_usage == NULL) {
		DPCERR("invalid dt usage table");
		return;
	}

	/* update DT table */
	if (dt < DPC_DISP_DT_CNT)
		g_disp_dt_usage[dt].ep = us;
	else if (dt < DPC_DISP_DT_CNT + DPC_MML_DT_CNT)
		g_mml_dt_usage[dt - DPC_DISP_DT_CNT].ep = us;

	dpc_mmp(dt, MMPROFILE_FLAG_PULSE, dt, us);
}

static unsigned int dpc_align_fps_duration(unsigned int duration)
{
	if (duration >= DT_TE_30)
		duration = DT_TE_30;
	else if (duration >= DT_TE_45)
		duration = DT_TE_45;
	else if (duration >= DT_TE_60)
		duration = DT_TE_60;
	else if (duration >= DT_TE_90)
		duration = DT_TE_90;
	else if (duration >= DT_TE_120)
		duration = DT_TE_120;
	else if (duration >= DT_TE_150)
		duration = DT_TE_150;
	else if (duration >= DT_TE_180)
		duration = DT_TE_180;
	else if (duration >= DT_TE_210)
		duration = DT_TE_210;
	else if (duration >= DT_TE_240)
		duration = DT_TE_240;
	else if (duration >= DT_TE_360)
		duration = DT_TE_360;
	else
		duration = DT_MIN_FRAME;

	return duration;
}

static int dpc_vidle_is_available(void)
{
	int ret = 0;

	if (g_priv->vidle_mask == 0)
		return 0;

	switch (g_panel_type) {
	case PANEL_TYPE_VDO:
		/* support vidle if VDO vblank period is enough */
		if (g_vb_duration > DT_MIN_VBLANK)
			ret = 1;
		break;
	case PANEL_TYPE_CMD:
		/* support vidle if CMD frame period is enough */
		if (g_te_duration > DT_MIN_FRAME)
			ret = 1;
		break;
	default:
		break;
	}

	return ret;
}

/* dur_frame and dur_vblank are in unit of us*/
static int dpc_dt_set_dur(u32 dur_frame, u32 dur_vblank)
{
	unsigned int duration = 0;
	unsigned long flags = 0;

	if (dur_frame == 0)
		return -1;

	if (dpc_pm_ctrl(true))
		return -1;

	spin_lock_irqsave(&dpc_lock, flags);
	duration = dpc_align_fps_duration(dur_frame);
	if (g_te_duration == duration && g_vb_duration == dur_vblank)
		goto out;

	dpc_mmp(dt, MMPROFILE_FLAG_START, g_te_duration, dur_frame);
	/* update DT table affected by TE duration */
	dpc_dt_update_table(1, duration - DT_OVL_OFFSET);
	dpc_dt_update_table(5, duration - DT_DISP1_OFFSET);
	dpc_dt_update_table(6, duration - DT_DISP1TE_OFFSET);
	dpc_dt_update_table(12, duration - DT_MMINFRA_OFFSET);
	dpc_dt_update_table(33, duration - DT_OVL_OFFSET);
	dpc_dt_update_table(40, duration - DT_MMINFRA_OFFSET);

	/* update DT timer affected by TE duration */
	dpc_dt_set(1, duration - DT_OVL_OFFSET);
	dpc_dt_set(5, duration - DT_DISP1_OFFSET);
	dpc_dt_set(6, duration - DT_DISP1TE_OFFSET);
	dpc_dt_set(12, duration - DT_MMINFRA_OFFSET);
	dpc_dt_set(33, duration - DT_OVL_OFFSET);
	dpc_dt_set(40, duration - DT_MMINFRA_OFFSET);

	g_te_duration = duration;
	g_vb_duration = dur_vblank;
	dpc_mmp(dt, MMPROFILE_FLAG_END, g_te_duration, g_vb_duration);

	if (dpc_vidle_is_available() == 0) {
		if (g_panel_type == PANEL_TYPE_VDO && g_priv->vidle_mask) {
			dpc_dt_update_table(4, DT_MAX_TIMEOUT);
			dpc_dt_update_table(11, DT_MAX_TIMEOUT);
			dpc_dt_update_table(32, DT_MAX_TIMEOUT);
			dpc_dt_update_table(39, DT_MAX_TIMEOUT);
			dpc_dt_set(4, DT_MAX_TIMEOUT);
			dpc_dt_set(11, DT_MAX_TIMEOUT);
			dpc_dt_set(32, DT_MAX_TIMEOUT);
			dpc_dt_set(39, DT_MAX_TIMEOUT);
		}
		duration = -1;
		goto out;
	} else if (g_panel_type == PANEL_TYPE_VDO) {
		if (g_disp_dt_usage[4].ep == DT_MAX_TIMEOUT) {
			dpc_dt_update_table(4, DT_4);
			dpc_dt_update_table(11, DT_11);
			dpc_dt_update_table(32, DT_4);
			dpc_dt_update_table(39, DT_11);
			dpc_dt_set(4, DT_4);
			dpc_dt_set(11, DT_11);
			dpc_dt_set(32, DT_4);
			dpc_dt_set(39, DT_11);
		}
	}

out:
	spin_unlock_irqrestore(&dpc_lock, flags);
	dpc_pm_ctrl(false);

	return duration;
}

static void dpc_dsi_pll_set(const u32 value)
{
	if (mtk_dpc_support_cap(DPC_VIDLE_MTCMOS_OFF) == 0)
		return;

	/* if DSI_PLL_SEL is set, power ON disp1 and set DSI_CK_KEEP_EN */
	if (value & BIT(0)) {
		dpc_mtcmos_vote(DPC_SUBSYS_DISP1, 6, 1); /* will be cleared when ff enable */
		mtk_disp_wait_pwr_ack(DPC_SUBSYS_DISP1);
		writel(BIT(5), dpc_base + DISP_REG_DPC_DISP1_MTCMOS_CFG);
	}
}

static void dpc_irq_enable(const enum mtk_dpc_subsys subsys, bool en, bool manual)
{
	u32 mask = 0;

	if (g_panel_type >= PANEL_TYPE_COUNT && en) {
		DPCDUMP("skip en:%d, invalid panel type:%d", en, g_panel_type);
		return;
	}

	if (en) {
		if (MTK_DPC_OF_DISP_SUBSYS(subsys)) {
			mask = (DISP_DPC_INT_MMINFRA_OFF_END | DISP_DPC_INT_MMINFRA_OFF_START |
				DISP_DPC_INT_DT6 | DISP_DPC_INT_DT5);
			if (g_panel_type == PANEL_TYPE_CMD)
				mask |= DISP_DPC_INT_DT7; //cmd panel off after pre-TE
			else
				mask |= DISP_DPC_INT_DT4; //vdo panel off after frame done
			if (debug_mtcmos_off)
				mask |= (DISP_DPC_INT_DISP1_ON | DISP_DPC_INT_DISP1_OFF |
					DISP_DPC_INT_OVL0_ON | DISP_DPC_INT_OVL0_OFF);
			writel(mask, dpc_base + DISP_REG_DPC_DISP_INTEN);
		} else if (MTK_DPC_OF_MML_SUBSYS(subsys)) {
			mask = (MML_DPC_INT_DT54 | MML_DPC_INT_DT55 |
				MML_DPC_INT_MML1_SOF | MML_DPC_INT_MML1_RDONE |
				MML_DPC_INT_DT33);
			if (g_panel_type == PANEL_TYPE_CMD)
				mask |= MML_DPC_INT_DT35; // cmd panel off after pre-TE
			else
				mask |= MML_DPC_INT_DT32; //vdo panel off after frame done

			//both cmd and vdo panel SW manual off w/o check frame busy for DC mode
			if (manual)
				mask |= MML_DPC_INT_DT32;

			if (debug_mtcmos_off)
				mask |= (MML_DPC_INT_MML1_OFF | MML_DPC_INT_MML1_ON);
			writel(mask, dpc_base + DISP_REG_DPC_MML_INTEN);
		}
	} else {
		mask = 0x0;
		if (MTK_DPC_OF_DISP_SUBSYS(subsys))
			writel(mask, dpc_base + DISP_REG_DPC_DISP_INTEN);
		else if (MTK_DPC_OF_MML_SUBSYS(subsys))
			writel(mask, dpc_base + DISP_REG_DPC_MML_INTEN);
	}
}

static void dpc_disp_group_enable_func(const enum mtk_dpc_disp_vidle group, bool en, bool dt_ctrl)
{
	int i, avail = 0;
	u32 value = 0, value1 = 0;

	avail = mtk_dpc_support_group(group);
	switch (group) {
	case DPC_DISP_VIDLE_MTCMOS:
		/* MTCMOS auto_on_off enable, both ack */
		value = (en && avail) ? (BIT(0) | BIT(4)) : 0;
		writel(value, dpc_base + DISP_REG_DPC_DISP0_MTCMOS_CFG);
		writel(value, dpc_base + DISP_REG_DPC_OVL0_MTCMOS_CFG);
		writel(value, dpc_base + DISP_REG_DPC_OVL1_MTCMOS_CFG);
		break;
	case DPC_DISP_VIDLE_MTCMOS_DISP1:
		/* MTCMOS auto_on_off enable, both ack, pwr off dependency */
		value = (en && avail) ? (BIT(0) | BIT(4) | BIT(6)) : 0;
		writel(value, dpc_base + DISP_REG_DPC_DISP1_MTCMOS_CFG);

		/* DDR_SRC and EMI_REQ DT is follow DISP1 */
		value1 = (en && mtk_dpc_support_cap(DPC_VIDLE_APSRC_OFF)) ?
					0x00010001 : 0x000D000D;
		writel(value1, dpc_base + DISP_REG_DPC_DISP_DDRSRC_EMIREQ_CFG);
		break;
	case DPC_DISP_VIDLE_VDISP_DVFS:
		value = (en && avail) ? 0 : 1;
		writel(value, dpc_base + DISP_REG_DPC_DISP_VDISP_DVFS_CFG);
		break;
	case DPC_DISP_VIDLE_HRT_BW:
	case DPC_DISP_VIDLE_SRT_BW:
		value = (en && avail) ? 0 : 0x00010001;
		writel(value, dpc_base + DISP_REG_DPC_DISP_HRTBW_SRTBW_CFG);
		break;
	case DPC_DISP_VIDLE_MMINFRA_OFF:
	case DPC_DISP_VIDLE_INFRA_OFF:
	case DPC_DISP_VIDLE_MAINPLL_OFF:
		/* TODO: check SEL is 0b00 or 0b10 for ALL_PWR_ACK */
		value = (en && avail) ? 0 : 0x181818;
		writel(value, dpc_base + DISP_REG_DPC_DISP_INFRA_PLL_OFF_CFG);
		break;
	default:
		break;
	}
	dpc_mmp(disp_group, MMPROFILE_FLAG_PULSE, group, value);

	if (g_disp_dt_usage == NULL || !dt_ctrl)
		return;

	if (!en) {
		for (i = 0; i < DPC_DISP_DT_CNT; ++i) {
			if (group == g_disp_dt_usage[i].group)
				dpc_dt_enable(g_disp_dt_usage[i].index, false);
		}
		return;
	}
	for (i = 0; i < DPC_DISP_DT_CNT; ++i) {
		if (group == g_disp_dt_usage[i].group) {
			dpc_dt_set(g_disp_dt_usage[i].index, g_disp_dt_usage[i].ep);
			// dpc_dt_enable(g_disp_dt_usage[i].index, true);
		}
	}
}

static void dpc_mml_group_enable_func(const enum mtk_dpc_mml_vidle group, bool en, bool dt_ctrl)
{
	int i, avail = 0;
	u32 value = 0, value1 = 0;

	avail = mtk_dpc_support_group(group);
	switch (group) {
	case DPC_MML_VIDLE_MTCMOS:
		/* MTCMOS auto_on_off enable, both ack */
		value = (en && avail) ? (BIT(0) | BIT(4)) : 0;
		writel(value, dpc_base + DISP_REG_DPC_MML1_MTCMOS_CFG);

		/* DDR_SRC and EMI_REQ DT is follow MML1 */
		value1 = (en && mtk_dpc_support_cap(DPC_VIDLE_APSRC_OFF)) ?
					0x00010001 : 0x000D000D;
		writel(value1, dpc_base + DISP_REG_DPC_MML_DDRSRC_EMIREQ_CFG);
		break;
	case DPC_MML_VIDLE_VDISP_DVFS:
		value = (en && avail) ? 0 : 1;
		writel(value, dpc_base + DISP_REG_DPC_MML_VDISP_DVFS_CFG);
		break;
	case DPC_MML_VIDLE_HRT_BW:
	case DPC_MML_VIDLE_SRT_BW:
		value = (en & avail) ? 0 : 0x00010001;
		writel(value, dpc_base + DISP_REG_DPC_MML_HRTBW_SRTBW_CFG);
		break;
	case DPC_MML_VIDLE_MMINFRA_OFF:
	case DPC_MML_VIDLE_INFRA_OFF:
	case DPC_MML_VIDLE_MAINPLL_OFF:
		/* TODO: check SEL is 0b00 or 0b10 for ALL_PWR_ACK */
		value = (en && avail) ? 0 : 0x181818;
		writel(value, dpc_base + DISP_REG_DPC_MML_INFRA_PLL_OFF_CFG);
		break;
	default:
		break;
	}
	dpc_mmp(mml_group, MMPROFILE_FLAG_PULSE, group, value);

	if (g_mml_dt_usage == NULL || !dt_ctrl)
		return;

	if (!en) {
		for (i = 0; i < DPC_MML_DT_CNT; ++i) {
			if (group == g_mml_dt_usage[i].group)
				dpc_dt_enable(g_mml_dt_usage[i].index, false);
		}
		return;
	}
	for (i = 0; i < DPC_MML_DT_CNT; ++i) {
		if (group == g_mml_dt_usage[i].group) {
			dpc_dt_set(g_mml_dt_usage[i].index, g_mml_dt_usage[i].ep);
			// dpc_dt_enable(g_mml_dt_usage[i].index, true);
		}
	}
}

static void dpc_disp_group_pause(bool en)
{
	bool enable = !en;

	dpc_disp_group_enable_func(DPC_DISP_VIDLE_MTCMOS, enable, false);
	dpc_disp_group_enable_func(DPC_DISP_VIDLE_MTCMOS_DISP1, enable, false);
	dpc_disp_group_enable_func(DPC_DISP_VIDLE_VDISP_DVFS, enable, false);
	dpc_disp_group_enable_func(DPC_DISP_VIDLE_HRT_BW, enable, false);
	dpc_disp_group_enable_func(DPC_DISP_VIDLE_MMINFRA_OFF, enable, false);
}

static void dpc_mml_group_pause(bool en)
{
	bool enable = !en;

	dpc_mml_group_enable_func(DPC_MML_VIDLE_MTCMOS, enable, false);
	dpc_mml_group_enable_func(DPC_MML_VIDLE_VDISP_DVFS, enable, false);
	dpc_mml_group_enable_func(DPC_MML_VIDLE_HRT_BW, enable, false);
	dpc_mml_group_enable_func(DPC_MML_VIDLE_MMINFRA_OFF, enable, false);
}

void dpc_ddr_force_enable(const enum mtk_dpc_subsys subsys, const bool en)
{
	u32 addr = 0;
	u32 value = en ? 0x000D000D : 0x00050005;
	unsigned long flags = 0;

	if (MTK_DPC_OF_DISP_SUBSYS(subsys))
		addr = DISP_REG_DPC_DISP_DDRSRC_EMIREQ_CFG;
	else if (MTK_DPC_OF_MML_SUBSYS(subsys))
		addr = DISP_REG_DPC_MML_DDRSRC_EMIREQ_CFG;
	else {
		DPCERR("invalid subsys:%d", subsys);
		WARN_ON(1);
		return;
	}

	if (dpc_pm_ctrl(true))
		return;

	spin_lock_irqsave(&dpc_lock, flags);
	if (mtk_dpc_support_cap(DPC_VIDLE_APSRC_OFF) == 0)
		value = 0x000D000D;
	writel(value, dpc_base + addr);
	spin_unlock_irqrestore(&dpc_lock, flags);

	dpc_pm_ctrl(false);
}
EXPORT_SYMBOL(dpc_ddr_force_enable);

void dpc_infra_force_enable(const enum mtk_dpc_subsys subsys, const bool en)
{
	u32 addr = 0;
	u32 value = en ? 0x00181818 : 0x00080808;
	unsigned long flags = 0;

	if (MTK_DPC_OF_DISP_SUBSYS(subsys))
		addr = DISP_REG_DPC_DISP_INFRA_PLL_OFF_CFG;
	else if (MTK_DPC_OF_MML_SUBSYS(subsys))
		addr = DISP_REG_DPC_MML_INFRA_PLL_OFF_CFG;
	else {
		DPCERR("invalid subsys:%d", subsys);
		WARN_ON(1);
		return;
	}

	if (dpc_pm_ctrl(true))
		return;

	spin_lock_irqsave(&dpc_lock, flags);

	if (mtk_dpc_support_cap(DPC_VIDLE_MMINFRA_PLL_OFF) == 0)
		value = 0x181818;
	writel(value, dpc_base + addr);
	spin_unlock_irqrestore(&dpc_lock, flags);

	dpc_pm_ctrl(false);
}
EXPORT_SYMBOL(dpc_infra_force_enable);

#define FIX_WORKAROUND_FOR_DC  (0)
void dpc_dc_force_enable_func(const bool en, bool lock)
{
	static bool mml_vidle;
	unsigned int val, mask;
	unsigned long flags = 0;

	if (g_panel_type >= PANEL_TYPE_COUNT)
		return;

	if (dpc_pm_ctrl(true))
		return;

	if (lock)
		spin_lock_irqsave(&dpc_lock, flags);
	//ignore mml vidle ops, since disp always on
	if (!(atomic_read(&g_vidle_window) & DPC_VIDLE_DISP_WINDOW))
		goto out;

	if (atomic_read(&g_mml_mode) == DPC_VIDLE_HW_AUTO_MODE) {
		DPCDUMP("ignore mml config, mml_mode:%d", atomic_read(&g_mml_mode));
		dpc_mmp(mml_group, MMPROFILE_FLAG_PULSE, atomic_read(&g_mml_mode), 0x5a5a0000);
		goto out;
	}

	if (en) {
		if (atomic_read(&g_mml_mode) == DPC_VIDLE_INACTIVE_MODE) {
			//set SW manual mode
			dpc_mmp(mml_group, MMPROFILE_FLAG_START, en, atomic_read(&g_mml_mode));
			atomic_set(&g_mml_mode, DPC_VIDLE_SW_MANUAL_MODE);

			if (debug_runtime_ctrl) {
				debug_mmp = 1;
				debug_irq = 1;
				debug_mtcmos_off = 1;
			}

			atomic_set(&g_vidle_window,
				atomic_read(&g_vidle_window) | DPC_VIDLE_MML_DC_WINDOW);
			dpc_mmp(group, MMPROFILE_FLAG_PULSE, BIT(DPC_SUBSYS_MML), en);

			//enable apsrc, emi, dvfs, mminfra, infra, main pll
			mask = mtk_dpc_get_vidle_mask(DPC_SUBSYS_MML, en);
			writel(mask, dpc_base + DISP_REG_DPC_MML_MASK_CFG);
			dpc_mmp(mml_group, MMPROFILE_FLAG_PULSE, DISP_REG_DPC_MML_MASK_CFG, mask);

			val = mtk_dpc_support_cap(DPC_VIDLE_APSRC_OFF) ? 0x00010001 : 0x000D000D;
			writel(val, dpc_base + DISP_REG_DPC_MML_DDRSRC_EMIREQ_CFG);
			dpc_mmp(mml_group, MMPROFILE_FLAG_PULSE, DISP_REG_DPC_MML_DDRSRC_EMIREQ_CFG, val);

			val = mtk_dpc_support_cap(DPC_VIDLE_MMINFRA_PLL_OFF) ? 0 : 0x181818;
			writel(val, dpc_base + DISP_REG_DPC_MML_INFRA_PLL_OFF_CFG);
			dpc_mmp(mml_group, MMPROFILE_FLAG_PULSE, DISP_REG_DPC_MML_INFRA_PLL_OFF_CFG, val);

			val = mtk_dpc_support_cap(DPC_VIDLE_ZERO_HRT_BW) ? 0 : 0x00010001;
			writel(val, dpc_base + DISP_REG_DPC_MML_HRTBW_SRTBW_CFG);
			dpc_mmp(mml_group, MMPROFILE_FLAG_PULSE, DISP_REG_DPC_MML_HRTBW_SRTBW_CFG, val);

			val = mtk_dpc_support_cap(DPC_VIDLE_LOWER_VDISP_DVFS) ? 0 : 1;
			writel(val, dpc_base + DISP_REG_DPC_MML_VDISP_DVFS_CFG);
			dpc_mmp(mml_group, MMPROFILE_FLAG_PULSE, DISP_REG_DPC_MML_VDISP_DVFS_CFG, val);

			//enable DT follow mode
			writel(0x3ff, dpc_base + DISP_REG_DPC_MML_DT_FOLLOW_CFG);

			//enable MML DT32, DT33 for apsrc off, DT39, DT40 for dvfsrc
			val = readl(dpc_base + DISP_REG_DPC_MML_DT_EN);
			val |= 0x183;
			val &= ~0x208; //disable DT35, DT41 to avoid of off before MML finished
			writel(val, dpc_base + DISP_REG_DPC_MML_DT_EN);  //DT32,33
			dpc_mmp(mml_group, MMPROFILE_FLAG_PULSE, DISP_REG_DPC_MML_DT_EN, val);
			dpc_mmp(mml_dt, MMPROFILE_FLAG_PULSE, val, 0xD733);

			//enable irq
			if (debug_irq)
				dpc_irq_enable(DPC_SUBSYS_MML, true, true);

			//trigger DT33:MML1 MTCMOS on
			writel(0x183, dpc_base + DISP_REG_DPC_MML_DT_SW_TRIG_EN);
			writel(0x1, dpc_base + DISP_REG_DPC_DTx_SW_TRIG(33));
			writel(0x1, dpc_base + DISP_REG_DPC_DTx_SW_TRIG(40));

			//clear interrupt
			val = readl(dpc_base + DISP_REG_DPC_MML_INTSTA);
			if (val)
				writel(~val, dpc_base + DISP_REG_DPC_MML_INTSTA);

			//enable MML1 MTCMOS auto_on_off
			if (FIX_WORKAROUND_FOR_DC && mtk_dpc_support_group(DPC_MML_VIDLE_MTCMOS))
				writel(BIT(0) | BIT(4), dpc_base + DISP_REG_DPC_MML1_MTCMOS_CFG);
			dpc_mmp(mml_group, MMPROFILE_FLAG_END, en, atomic_read(&g_mml_mode));
		} else if (mml_vidle) {
			//trigger DT33:MML1 MTCMOS on
			writel(0x183, dpc_base + DISP_REG_DPC_MML_DT_SW_TRIG_EN);
			writel(0x1, dpc_base + DISP_REG_DPC_DTx_SW_TRIG(33));
			writel(0x1, dpc_base + DISP_REG_DPC_DTx_SW_TRIG(40));
			dpc_mmp(mml_dt, MMPROFILE_FLAG_PULSE, 0xFFFFFFFF, 0xD733);
		}

		mml_vidle = false;
	} else if (!mml_vidle) {
		//trigger MML1 MTCMOS off
		writel(0x1, dpc_base + DISP_REG_DPC_DTx_SW_TRIG(32));
		writel(0x1, dpc_base + DISP_REG_DPC_DTx_SW_TRIG(39));
		dpc_mmp(mml_dt, MMPROFILE_FLAG_PULSE, 0xFFFFFFFF, 0xD732);

		mml_vidle = true;
	}

out:
	if (lock)
		spin_unlock_irqrestore(&dpc_lock, flags);
	dpc_pm_ctrl(false);
}

void dpc_dc_force_enable(const bool en)
{
	dpc_dc_force_enable_func(en, true);
}

EXPORT_SYMBOL(dpc_dc_force_enable);

void dpc_init_panel_type(enum mtk_panel_type type)
{
	if (type == g_panel_type)
		return;

	if (type >= PANEL_TYPE_COUNT) {
		DPCERR("invalid panel type:%d", type);
		return;
	}

	if (g_priv == NULL) {
		DPCERR("invalid dpc data");
		return;
	}

	if (type == PANEL_TYPE_CMD) {
		if (g_priv->disp_cmd_dt_usage == NULL ||
			g_priv->mml_cmd_dt_usage == NULL) {
			DPCERR("not support cmd dt usage");
			return;
		}

		g_disp_dt_usage = g_priv->disp_cmd_dt_usage;
		g_mml_dt_usage = g_priv->mml_cmd_dt_usage;
	} else {
		unsigned int mask = BIT(DPC_VIDLE_MTCMOS_OFF) |
				BIT(DPC_VIDLE_MMINFRA_PLL_OFF) |
				BIT(DPC_VIDLE_INFRA_REQ_OFF) |
				BIT(DPC_DISP_VIDLE_MAINPLL_OFF);

		if (g_priv->disp_vdo_dt_usage == NULL ||
			g_priv->mml_vdo_dt_usage == NULL) {
			DPCERR("not support vdo dt usage");
			return;
		}

		g_disp_dt_usage = g_priv->disp_vdo_dt_usage;
		g_mml_dt_usage = g_priv->mml_vdo_dt_usage;

		/* VDO panel not support mtcmos/mminfra/infra */
		g_priv->vidle_mask &= ~mask;
	}
	mtk_dpc_dump_caps();
	g_panel_type = type;
	DPCDUMP("type:%d", g_panel_type);
}
EXPORT_SYMBOL(dpc_init_panel_type);

void dpc_enable(bool en)
{
	unsigned int dt_mask = 0;
	unsigned long flags = 0;

	if (g_panel_type >= PANEL_TYPE_COUNT)
		return;

	if (g_priv->vidle_mask == 0)
		return;

	if (dpc_pm_ctrl(true))
		return;

	/* enable hfrp before trigger vidle if necessary */
	if (g_priv->mmdvfs_power_sync && en &&
		atomic_read(&g_priv->dpc_en_cnt) == 0) {
		if (mtk_mmdvfs_enable_vcp(true, VCP_PWR_USR_DISP) < 0) {
			DPCERR("failed to enable vcp, en:%d", en);
			goto out;
		}
	}

	spin_lock_irqsave(&dpc_lock, flags);
	if (en && dpc_vidle_is_available() == 0) {
		DPCERR("in-available, cap:0x%x, dur:%u-%u, panel:%d, en:%d",
			g_priv->vidle_mask, g_te_duration, g_vb_duration,
			g_panel_type, en);
		goto inavail;
	}

	if (en) {
		if (debug_runtime_ctrl) {
			debug_mmp = 1;
			debug_irq = 1;
			debug_mtcmos_off = 1;
			mtk_dpc_idle_ratio_debug(DPC_VIDLE_RATIO_START);
		}
		if (!(atomic_read(&g_vidle_window) & DPC_VIDLE_DISP_WINDOW)) {
			atomic_set(&g_vidle_window,
				atomic_read(&g_vidle_window) | DPC_VIDLE_DISP_WINDOW);
			mtk_init_dpc_timer();
		}

		/* CMD panel: DT enable only 1, 3, 5, 6, 7, 12, 13, 29, 30, 31
		 * VDO panel: DT enable only 4, 5, 6, 11, 12, 29, 30, 31
		 */
		if (g_panel_type == PANEL_TYPE_CMD)
			dt_mask = 0xe00030ea;
		else
			dt_mask = 0xe0001870;
		writel(dt_mask, dpc_base + DISP_REG_DPC_DISP_DT_EN);
		dpc_mmp(disp_dt, MMPROFILE_FLAG_PULSE, dt_mask, en);

		/* CMD panel: DT enable only 1, 3, 8, 9
		 * VDO panel: DT enable only 0, 1, 7, 8
		 */
		if (g_panel_type == PANEL_TYPE_CMD)
			dt_mask = 0x0000030a;
		else
			dt_mask = 0x00000183;
		writel(dt_mask, dpc_base + DISP_REG_DPC_MML_DT_EN);
		dpc_mmp(mml_dt, MMPROFILE_FLAG_PULSE, dt_mask, en);

		if (likely(debug_mmp))
			mtk_disp_enable_gce_vote(true);
		if (g_panel_type == PANEL_TYPE_CMD)
			writel(DISP_DPC_EN | DISP_DPC_DT_EN, dpc_base + DISP_REG_DPC_EN);
		else
			writel(DISP_DPC_EN | DISP_DPC_DT_EN | DISP_DPC_VDO_MODE,
				dpc_base + DISP_REG_DPC_EN);
	} else {
		if (debug_vidle_timeout && g_te_duration)
			del_timer_sync(&g_priv->dpc_timer);
		/* disable inten to avoid burst irq */
		dpc_irq_enable(DPC_SUBSYS_DISP, false, false);
		dpc_irq_enable(DPC_SUBSYS_MML, false, false);

		//stop MML vidle @DC mode
		if (atomic_read(&g_vidle_window) & DPC_VIDLE_MML_DC_WINDOW) {
			dpc_mmp(group, MMPROFILE_FLAG_PULSE, BIT(DPC_SUBSYS_MML), en);

			//leave mml vidle mode
			dpc_dc_force_enable_func(true, false);
			writel(0x0, dpc_base + DISP_REG_DPC_MML_DT_EN);
			writel(0x0, dpc_base + DISP_REG_DPC_MML_DT_SW_TRIG_EN);

			//disable mml1 mtcmos off
			if (FIX_WORKAROUND_FOR_DC && mtk_dpc_support_group(DPC_MML_VIDLE_MTCMOS))
				writel(0x0, dpc_base + DISP_REG_DPC_MML1_MTCMOS_CFG);

			//disable apsrc, emi, dvfs, mminfra, infra, main pll
			writel(U32_MAX, dpc_base + DISP_REG_DPC_MML_MASK_CFG);
			writel(0x000D000D, dpc_base + DISP_REG_DPC_MML_DDRSRC_EMIREQ_CFG);
			writel(0x181818, dpc_base + DISP_REG_DPC_MML_INFRA_PLL_OFF_CFG);
			writel(0x00010001, dpc_base + DISP_REG_DPC_MML_HRTBW_SRTBW_CFG);
			writel(1, dpc_base + DISP_REG_DPC_MML_VDISP_DVFS_CFG);

			if (g_idle_ratio_debug) {
				mtk_dpc_idle_ratio_debug(DPC_VIDLE_RATIO_STOP);
				mtk_dpc_idle_ratio_debug(DPC_VIDLE_RATIO_DUMP);
			}
			//clear SW manual mode
			atomic_set(&g_mml_mode, DPC_VIDLE_INACTIVE_MODE);
		}

		atomic_set(&g_vidle_window, 0);
		writel(0, dpc_base + DISP_REG_DPC_EN);

		/* reset dpc to clean counter start and value */
		writel(1, dpc_base + DISP_REG_DPC_RESET);
		writel(0, dpc_base + DISP_REG_DPC_RESET);
		mtk_disp_enable_gce_vote(false);
		dpc_mmp(disp_dt, MMPROFILE_FLAG_PULSE, U32_MAX, 0);
		dpc_mmp(mml_dt, MMPROFILE_FLAG_PULSE, U32_MAX, 0);

		//misc
		mtk_reset_dpc_state();
		if (debug_runtime_ctrl) {
			mtk_dpc_update_vlp_state(0, 0, true);
			debug_mmp = 0;
			debug_irq = 0;
			debug_mtcmos_off = 0;
		}

	}

	/* enable gce event */
	writel(en, dpc_base + DISP_REG_DPC_EVENT_EN);

	if (en && atomic_read(&g_priv->dpc_en_cnt) == 0)
		dpc_mmp(dpc, MMPROFILE_FLAG_START, en, g_panel_type);
	else if (!en && atomic_read(&g_priv->dpc_en_cnt) == 1)
		dpc_mmp(dpc, MMPROFILE_FLAG_END, en, g_panel_type);
	atomic_set(&g_priv->dpc_en_cnt, en ? 1 : 0);

inavail:
	spin_unlock_irqrestore(&dpc_lock, flags);

	/* disable hfrp after vidle off if necessary */
	if (g_priv->mmdvfs_power_sync && !en &&
		atomic_read(&g_priv->dpc_en_cnt) == 0) {
		mtk_mmdvfs_enable_vcp(false, VCP_PWR_USR_DISP);
	}

out:
	dpc_pm_ctrl(false);
}
EXPORT_SYMBOL(dpc_enable);

void dpc_hrt_bw_set(const enum mtk_dpc_subsys subsys, const u32 bw_in_mb, bool force)
{
	u32 addr1 = 0, addr2 = 0, avail = 0;
	static u32 disp_bw, mml_bw;
	unsigned long flags = 0;

	if (dpc_pm_ctrl(true))
		return;

	spin_lock_irqsave(&dpc_lock, flags);
	if ((disp_bw != bw_in_mb && MTK_DPC_OF_DISP_SUBSYS(subsys)) ||
		(mml_bw != bw_in_mb && MTK_DPC_OF_MML_SUBSYS(subsys)) || force) {
		dpc_mmp(hrt_bw, MMPROFILE_FLAG_PULSE,
			BIT(subsys) | (force ? 0x80000000 : 0x0), bw_in_mb);
	}
	if (MTK_DPC_OF_DISP_SUBSYS(subsys)) {
		addr1 = DISP_REG_DPC_DISP_HIGH_HRT_BW;
		addr2 = DISP_REG_DPC_DISP_HRTBW_SRTBW_CFG;
		disp_bw = bw_in_mb;
	} else if (MTK_DPC_OF_MML_SUBSYS(subsys)) {
		addr1 = DISP_REG_DPC_MML_SW_HRT_BW;
		addr2 = DISP_REG_DPC_MML_HRTBW_SRTBW_CFG;
		mml_bw = bw_in_mb;
	} else {
		DPCERR("invalid subsys:%d", subsys);
		WARN_ON(1);
		goto out;
	}
	if (bw_in_mb > 0)
		writel(bw_in_mb / 30 + 1, dpc_base + addr1); /* 30MB unit */
	else
		writel(0, dpc_base + addr1); /* 30MB unit */

	avail = mtk_dpc_support_cap(DPC_VIDLE_ZERO_HRT_BW);
	writel((force || !avail) ? 0x00010001 : 0, dpc_base + addr2);

	if (unlikely(debug_dvfs))
		DPCFUNC("subsys(%u) hrt bw(%u)MB force(%u), reg:0x%x=0x%x, reg:0x%x=0x%x",
			subsys, bw_in_mb, force, addr1, readl(dpc_base + addr1),
			addr2, readl(dpc_base + addr2));
out:
	spin_unlock_irqrestore(&dpc_lock, flags);
	dpc_pm_ctrl(false);
}
EXPORT_SYMBOL(dpc_hrt_bw_set);

void dpc_srt_bw_set(const enum mtk_dpc_subsys subsys, const u32 bw_in_mb, bool force)
{
	u32 addr1 = 0, addr2 = 0, avail = 0;
	static u32 disp_bw, mml_bw;
	unsigned long flags = 0;

	if (dpc_pm_ctrl(true))
		return;

	spin_lock_irqsave(&dpc_lock, flags);
	if (debug_dvfs &&
		((disp_bw != bw_in_mb && MTK_DPC_OF_DISP_SUBSYS(subsys)) ||
		(mml_bw != bw_in_mb && MTK_DPC_OF_MML_SUBSYS(subsys)) || force)) {
		dpc_mmp(srt_bw, MMPROFILE_FLAG_PULSE,
			BIT(subsys) | (force ? 0x80000000 : 0x0), bw_in_mb);
	}
	if (MTK_DPC_OF_DISP_SUBSYS(subsys)) {
		addr1 = DISP_REG_DPC_DISP_SW_SRT_BW;
		addr2 = DISP_REG_DPC_DISP_HRTBW_SRTBW_CFG;
		disp_bw = bw_in_mb;
	} else if (MTK_DPC_OF_MML_SUBSYS(subsys)) {
		addr1 = DISP_REG_DPC_MML_SW_SRT_BW;
		addr2 = DISP_REG_DPC_MML_HRTBW_SRTBW_CFG;
		mml_bw = bw_in_mb;
	} else {
		DPCERR("invalid subsys:%d", subsys);
		WARN_ON(1);
		goto out;
	}
	if (bw_in_mb > 0)
		writel(bw_in_mb / 100 + 1, dpc_base + addr1); /* 100MB unit */
	else
		writel(0, dpc_base + addr1); /* 100MB unit */
	avail = mtk_dpc_support_cap(DPC_VIDLE_ZERO_SRT_BW);
	writel((force || !avail)? 0x00010001 : 0, dpc_base + addr2);

	if (unlikely(debug_dvfs))
		DPCFUNC("subsys(%u) srt bw(%u)MB force(%u), reg:0x%x=0x%x, reg:0x%x=0x%x",
			subsys, bw_in_mb, force, addr1, readl(dpc_base + addr1),
			addr2, readl(dpc_base + addr2));

out:
	spin_unlock_irqrestore(&dpc_lock, flags);
	dpc_pm_ctrl(false);
}
EXPORT_SYMBOL(dpc_srt_bw_set);

static void mtk_dpc_mmdvfs_settings_backup(void)
{
	unsigned int i = 0;

	if (g_priv->mmdvfs_settings_count == 0 ||
		g_priv->mmdvfs_settings_addr == NULL)
		return;

	for (i = 0; i < g_priv->mmdvfs_settings_count; i++) {
		if (mtk_dpc_mmdvfs_settings_dirty & BIT(i))
			continue;
		mtk_dpc_mmdvfs_settings_value[i] = readl(dpc_base +
			g_priv->mmdvfs_settings_addr[i]);
		DPCDUMP("i:%d, addr:0x%x, value:0x%x, dirty:%u", i, g_priv->mmdvfs_settings_addr[i],
			mtk_dpc_mmdvfs_settings_value[i], mtk_dpc_mmdvfs_settings_dirty);
	}
}

static void mtk_dpc_mmdvfs_settings_restore(void)
{
	unsigned int i = 0;

	if (g_priv->mmdvfs_settings_count == 0 ||
		g_priv->mmdvfs_settings_addr == NULL)
		return;

	for (i = 0; i < g_priv->mmdvfs_settings_count; i++) {
		writel(mtk_dpc_mmdvfs_settings_value[i] + 1,
			dpc_base + g_priv->mmdvfs_settings_addr[i]);
		udelay(100);
		writel(mtk_dpc_mmdvfs_settings_value[i],
			dpc_base + g_priv->mmdvfs_settings_addr[i]);
		DPCDUMP("i:%u, reg:0x%x,value:0x%x",i,
			g_priv->mmdvfs_settings_addr[i],
			mtk_dpc_mmdvfs_settings_value[i]);
	}

	mtk_dpc_mmdvfs_settings_dirty = 0;
}

static void mtk_dpc_mmdvfs_settings_update(unsigned int addr, unsigned int value)
{
	unsigned int i = 0;

	if (g_priv->mmdvfs_settings_count == 0 ||
		g_priv->mmdvfs_settings_addr == NULL)
		return;

	for (i = 0; i < g_priv->mmdvfs_settings_count; i++) {
		if (addr == g_priv->mmdvfs_settings_addr[i]) {
			mtk_dpc_mmdvfs_settings_value[i] = value;
			mtk_dpc_mmdvfs_settings_dirty |= BIT(i);
			break;
		}
	}
	dpc_mmp(mmdvfs_dead, MMPROFILE_FLAG_PULSE, addr, value);
	DPCFUNC("i%d, addr:0x%x, value:0x%x, dirty:%u", i, addr,
		mtk_dpc_mmdvfs_settings_value[i], mtk_dpc_mmdvfs_settings_dirty);
}

static int mtk_dpc_mmdvfs_notifier(const bool enable, const bool wdt)
{
	static bool dead;
	unsigned long flags = 0;
	int ret = 0;

	if (wdt || (enable && !g_priv->skip_force_power))
		DPCFUNC("enable:%d, wdt:%d, count:%u, skip_power:%d",
			enable, wdt, g_priv->mmdvfs_settings_count, g_priv->skip_force_power);

	if (!enable && !dead) {
		dpc_mmp(mmdvfs_dead, MMPROFILE_FLAG_START, enable, wdt);
		dead = true;
		return 0;
	}

	if (enable && !g_priv->skip_force_power &&
		g_priv->mmdvfs_settings_count > 0 &&
		g_priv->mmdvfs_settings_addr) {
		if (dpc_pm_ctrl(true)) {
			ret = -1;
			goto out;
		}
		spin_lock_irqsave(&dpc_lock, flags);
		mtk_dpc_mmdvfs_settings_backup();
		mtk_dpc_mmdvfs_settings_restore();
		spin_unlock_irqrestore(&dpc_lock, flags);
		dpc_pm_ctrl(false);
	}

out:
	if (enable && dead) {
		dpc_mmp(mmdvfs_dead, MMPROFILE_FLAG_END, enable, wdt);
		dead = false;
	} else
		dpc_mmp(mmdvfs_dead, MMPROFILE_FLAG_PULSE, enable, wdt);

	return ret;
}

static int vdisp_level_set_vcp(const enum mtk_dpc_subsys subsys, const u8 level, bool mmdvfs_state)
{
	u32 addr = 0;
	u32 state = 0, value = 0, count = 0;
	int ret = 0;

	if (MTK_DPC_OF_DISP_SUBSYS(subsys))
		addr = DISP_REG_DPC_DISP_VDISP_DVFS_VAL;
	else if (MTK_DPC_OF_MML_SUBSYS(subsys))
		addr = DISP_REG_DPC_MML_VDISP_DVFS_VAL;
	else {
		DPCERR("invalid subsys:%d", subsys);
		WARN_ON(1);
		return -2;
	}

	if (!mmdvfs_state) {
		DPCERR("mmdvfs is not ready, addr:0x%x val:0x%x, ret:%d", addr, level, mmdvfs_state);
		mtk_dpc_mmdvfs_settings_update(addr, level);
		return 0;
	}

	/* polling vdisp dvfsrc idle */
	if (g_priv->mmsys_id == MMSYS_MT6989 && g_priv->get_sys_status) {
		do {
			state = g_priv->get_sys_status(SYS_STATE_VDISP_DVFS, &value);
			if (count++ > 500) {
				ret = -1;
				break;
			}
			udelay(1);
		} while (state);
	} else
		udelay(100);
	if (ret < 0)
		DPCERR("subsys(%d) wait vdisp dvfsrc idle timeout", subsys);

	writel(level, dpc_base + addr);
	if (MTK_DPC_OF_DISP_SUBSYS(subsys)) {
		g_vdisp_level_disp = level;
		if (MEM_BASE) /* add vdisp info to met */
			writel(4 - level, MEM_USR_OPP(VCP_PWR_USR_DISP));
		dpc_mmp(vdisp_disp, MMPROFILE_FLAG_PULSE, (level << 16) | addr,
			readl(dpc_base + addr));
	} else if (MTK_DPC_OF_MML_SUBSYS(subsys)) {
		g_vdisp_level_mml = level;
		if (MEM_BASE) /* add vdisp info to met */
			writel(4 - level, MEM_USR_OPP(VCP_PWR_USR_MML));
		dpc_mmp(vdisp_mml, MMPROFILE_FLAG_PULSE, (level << 16) | addr,
			readl(dpc_base + addr));
	}

	if (unlikely(debug_dvfs_vdisp))
		DPCFUNC("subsys:%d,disp:0x%lx=0x%x,mml:0x%lx=0x%x,vdisp:%u/%u,ret:%d,cnt:%u",
			subsys, DISP_REG_DPC_DISP_VDISP_DVFS_VAL,
			readl(dpc_base + DISP_REG_DPC_DISP_VDISP_DVFS_VAL),
			DISP_REG_DPC_MML_VDISP_DVFS_VAL,
			readl(dpc_base + DISP_REG_DPC_MML_VDISP_DVFS_VAL),
			g_vdisp_level_disp, g_vdisp_level_mml, ret, count);

	return ret;
}

#define DPC_VDISP_LEVEL_IGNORE  (0xFF)
#define DPC_SUBSYS_VDISP_LEVEL_OF_BW  (DPC_SUBSYS_DISP)
static u8 dpc_max_dvfs_level(const enum mtk_dpc_subsys subsys)
{
	u8 max_level = 0;
	u32 addr;

	if (MTK_DPC_OF_DISP_SUBSYS(subsys)) {
		/* update channel bw and disp level by disp vote */
		addr = DISP_REG_DPC_DISP_VDISP_DVFS_VAL;

		max_level = g_priv->dvfs_bw.disp_level > g_priv->dvfs_bw.bw_level?
			g_priv->dvfs_bw.disp_level : g_priv->dvfs_bw.bw_level;
		if (readl(dpc_base + addr) == max_level)
			max_level = DPC_VDISP_LEVEL_IGNORE;

		if (unlikely(debug_dvfs_vdisp) && g_vdisp_level_disp != readl(dpc_base + addr))
			DPCERR("subsys:%d, level:%u, reg:0x%x=0x%x",
				subsys, g_vdisp_level_disp, addr, readl(dpc_base + addr));
	} else if (MTK_DPC_OF_MML_SUBSYS(subsys)) {
		addr = DISP_REG_DPC_MML_VDISP_DVFS_VAL;

		/* only update mml level by mml vote */
		if (readl(dpc_base + addr) == g_priv->dvfs_bw.mml_level)
			max_level = DPC_VDISP_LEVEL_IGNORE;
		else
			max_level = g_priv->dvfs_bw.mml_level;

		if (unlikely(debug_dvfs_vdisp) && g_vdisp_level_mml != readl(dpc_base + addr))
			DPCERR("subsys:%d, level:%u, reg:0x%x=0x%x",
				subsys, g_vdisp_level_mml, addr, readl(dpc_base + addr));
	} else {
		DPCERR("invalid user:%d", subsys);
		WARN_ON(1);
		return DPC_VDISP_LEVEL_IGNORE;
	}

	return max_level;
}

u8 dpc_dvfs_bw_to_level(const u32 bw_in_mb)
{
	u8 bw_level = 0;
	u32 total_bw;

	total_bw = bw_in_mb * 10 / 7;
	if (total_bw > 6988)
		bw_level = 4;
	else if (total_bw > 5129)
		bw_level = 3;
	else if (total_bw > 4076)
		bw_level = 2;
	else if (total_bw > 3057)
		bw_level = 1;
	else
		bw_level = 0;

	return bw_level;
}

void dpc_dvfs_set(const enum mtk_dpc_subsys subsys, const u8 level, bool force)
{
	u32 addr = 0, avail = 0;
	u32 mmdvfs_user = U32_MAX;
	u8 max_level, last_level;
	unsigned long flags = 0;
	bool mmdvfs_state = true;

	/* support 575, 600, 650, 700, 750 mV */
	if (level > 4) {
		DPCERR("vdisp support only 5 levels, subsys:%d, level:%d, force:%d",
			subsys, level, force);
		return;
	}

	if (MTK_DPC_OF_DISP_SUBSYS(subsys)) {
		mmdvfs_user = VCP_PWR_USR_DISP;
	} else if (MTK_DPC_OF_MML_SUBSYS(subsys)) {
		mmdvfs_user = VCP_PWR_USR_MML;
	} else {
		DPCERR("invalid user:%d", subsys);
		WARN_ON(1);
		return;
	}

	if (!g_priv) {
		DPCERR("!g_priv, dpc is not ready");
		return;
	}

	if (dpc_pm_ctrl(true))
		return;

	mutex_lock(&dvfs_lock);
	if (MTK_DPC_OF_DISP_SUBSYS(subsys)) {
		addr = DISP_REG_DPC_DISP_VDISP_DVFS_CFG;
		last_level = g_priv->dvfs_bw.disp_level;
		g_priv->dvfs_bw.disp_level = level;
	} else if (MTK_DPC_OF_MML_SUBSYS(subsys)) {
		addr = DISP_REG_DPC_MML_VDISP_DVFS_CFG;
		last_level = g_priv->dvfs_bw.mml_level;
		g_priv->dvfs_bw.mml_level = level;
	}

	max_level = dpc_max_dvfs_level(subsys);
	if (max_level == DPC_VDISP_LEVEL_IGNORE) {
		spin_lock_irqsave(&dpc_lock, flags);
		/* switch vdisp to SW or HW mode */
		avail = mtk_dpc_support_cap(DPC_VIDLE_LOWER_VDISP_DVFS);
		writel((force || !avail)? 1 : 0, dpc_base + addr);
		spin_unlock_irqrestore(&dpc_lock, flags);
		if (unlikely(debug_dvfs_vdisp))
			DPCFUNC("subsys:%d ignore subl:%u->%u,max:%u[%u,%u,%u(%u,%u)],f:%u,reg[0x%x,0x%x]",
				subsys, last_level, level, max_level,
				g_priv->dvfs_bw.disp_level, g_priv->dvfs_bw.mml_level,
				g_priv->dvfs_bw.bw_level, g_priv->dvfs_bw.disp_bw,
				g_priv->dvfs_bw.mml_bw, force,
				readl(dpc_base + DISP_REG_DPC_DISP_VDISP_DVFS_VAL),
				readl(dpc_base + DISP_REG_DPC_MML_VDISP_DVFS_VAL));
		goto out;
	}
	if (unlikely(debug_dvfs_vdisp))
		DPCFUNC("subsys:%d update subl:%u->%u,max:%u/%u->%u[%u,%u,%u(%u,%u)],f:%u",
			subsys, last_level, level, g_vdisp_level_disp, g_vdisp_level_mml,
			max_level, g_priv->dvfs_bw.disp_level, g_priv->dvfs_bw.mml_level,
			g_priv->dvfs_bw.bw_level, g_priv->dvfs_bw.disp_bw,
			g_priv->dvfs_bw.mml_bw, force);

	if (mtk_mmdvfs_enable_vcp(true, mmdvfs_user) < 0)
		mmdvfs_state = false;

	spin_lock_irqsave(&dpc_lock, flags);
	vdisp_level_set_vcp(subsys, max_level, mmdvfs_state);

	/* switch vdisp to SW or HW mode */
	avail = mtk_dpc_support_cap(DPC_VIDLE_LOWER_VDISP_DVFS);
	writel((force || !avail)? 1 : 0, dpc_base + addr);

	dpc_mmp(vdisp_level, MMPROFILE_FLAG_PULSE,
		(g_priv->dvfs_bw.bw_level << 16) |
		(g_priv->dvfs_bw.mml_level << 8) | g_priv->dvfs_bw.disp_level,
		(BIT(subsys) << 16) | max_level);

	spin_unlock_irqrestore(&dpc_lock, flags);
	if (mmdvfs_state)
		mtk_mmdvfs_enable_vcp(false, mmdvfs_user);

out:
	mutex_unlock(&dvfs_lock);
	dpc_pm_ctrl(false);
}
EXPORT_SYMBOL(dpc_dvfs_set);

void dpc_dvfs_bw_set(const enum mtk_dpc_subsys subsys, const u32 bw_in_mb)
{
	u8 max_bw_level, last_bw_level;
	u32 total_bw = 0;
	u32 mmdvfs_user = U32_MAX;
	unsigned long flags = 0;
	bool mmdvfs_state = true;

	if (MTK_DPC_OF_DISP_SUBSYS(subsys)) {
		mmdvfs_user = VCP_PWR_USR_DISP;
	} else if (MTK_DPC_OF_MML_SUBSYS(subsys)) {
		mmdvfs_user = VCP_PWR_USR_MML;
	} else {
		DPCERR("invalid user:%d", subsys);
		WARN_ON(1);
		return;
	}

	if (dpc_pm_ctrl(true))
		return;

	mutex_lock(&dvfs_lock);
	if (MTK_DPC_OF_DISP_SUBSYS(subsys))
		g_priv->dvfs_bw.disp_bw = bw_in_mb;
	else if (MTK_DPC_OF_MML_SUBSYS(subsys))
		g_priv->dvfs_bw.mml_bw = bw_in_mb;
	if (g_priv->dvfs_bw.mml_bw < 800)
		g_priv->dvfs_bw.mml_bw = 0;
	total_bw = g_priv->dvfs_bw.disp_bw +  g_priv->dvfs_bw.mml_bw;

	last_bw_level = g_priv->dvfs_bw.bw_level;
	g_priv->dvfs_bw.bw_level = dpc_dvfs_bw_to_level(total_bw);

	max_bw_level = dpc_max_dvfs_level(DPC_SUBSYS_VDISP_LEVEL_OF_BW);
	if (max_bw_level == DPC_VDISP_LEVEL_IGNORE) {
		if (unlikely(debug_dvfs_vdisp))
			DPCFUNC("subsys:%d ignore bwl:%u->%u,max:%u[%u,%u,%u(%u,%u)],reg[0x%x,0x%x]",
				subsys, last_bw_level, g_priv->dvfs_bw.bw_level, max_bw_level,
				g_priv->dvfs_bw.disp_level, g_priv->dvfs_bw.mml_level,
				g_priv->dvfs_bw.bw_level, g_priv->dvfs_bw.disp_bw,
				g_priv->dvfs_bw.mml_bw,
				readl(dpc_base + DISP_REG_DPC_DISP_VDISP_DVFS_VAL),
				readl(dpc_base + DISP_REG_DPC_MML_VDISP_DVFS_VAL));
		goto out;
	}
	if (unlikely(debug_dvfs_vdisp))
		DPCFUNC("subsys:%d update bwl:%u->%u,max:%u/%u->%u[%u,%u,%u(%u,%u)]",
			subsys, last_bw_level, g_priv->dvfs_bw.bw_level,
			g_vdisp_level_disp, g_vdisp_level_mml, max_bw_level,
			g_priv->dvfs_bw.disp_level, g_priv->dvfs_bw.mml_level,
			g_priv->dvfs_bw.bw_level, g_priv->dvfs_bw.disp_bw,
			g_priv->dvfs_bw.mml_bw);

	if (mtk_mmdvfs_enable_vcp(true, mmdvfs_user) < 0)
		mmdvfs_state = false;

	spin_lock_irqsave(&dpc_lock, flags);
	vdisp_level_set_vcp(DPC_SUBSYS_VDISP_LEVEL_OF_BW, max_bw_level, mmdvfs_state);

	dpc_mmp(vdisp_level, MMPROFILE_FLAG_PULSE,
		(g_priv->dvfs_bw.bw_level << 16) |
		(g_priv->dvfs_bw.mml_level << 8) | g_priv->dvfs_bw.disp_level,
		(BIT(subsys) << 16) | max_bw_level);

	spin_unlock_irqrestore(&dpc_lock, flags);
	if (mmdvfs_state)
		mtk_mmdvfs_enable_vcp(false, mmdvfs_user);

out:
	mutex_unlock(&dvfs_lock);
	dpc_pm_ctrl(false);
}
EXPORT_SYMBOL(dpc_dvfs_bw_set);

void dpc_dvfs_both_set(const enum mtk_dpc_subsys subsys, const u8 level, bool force,
	const u32 bw_in_mb)
{
	u32 addr = 0, avail = 0, total_bw = 0;
	u32 mmdvfs_user = U32_MAX;
	u8 max_level, max_level_subsys, max_level_bw;
	u8 last_bw_level, last_level;
	unsigned long flags = 0;
	bool mmdvfs_state = true;

	/* support 575, 600, 650, 700, 750 mV */
	if (level > 4) {
		DPCERR("vdisp support only 5 levels");
		return;
	}

	if (MTK_DPC_OF_DISP_SUBSYS(subsys))
		mmdvfs_user = VCP_PWR_USR_DISP;
	else if (MTK_DPC_OF_MML_SUBSYS(subsys))
		mmdvfs_user = VCP_PWR_USR_MML;
	else {
		DPCERR("invalid user:%d", subsys);
		WARN_ON(1);
		return;
	}

	if (dpc_pm_ctrl(true))
		return;

	mutex_lock(&dvfs_lock);
	if (MTK_DPC_OF_DISP_SUBSYS(subsys)) {
		addr = DISP_REG_DPC_DISP_VDISP_DVFS_CFG;
		last_level = g_priv->dvfs_bw.disp_level;
		g_priv->dvfs_bw.disp_level = level;
		g_priv->dvfs_bw.disp_bw = bw_in_mb;
	} else if (MTK_DPC_OF_MML_SUBSYS(subsys)) {
		addr = DISP_REG_DPC_MML_VDISP_DVFS_CFG;
		last_level = g_priv->dvfs_bw.mml_level;
		g_priv->dvfs_bw.mml_level = level;
		g_priv->dvfs_bw.mml_bw = bw_in_mb;
	}
	if (g_priv->dvfs_bw.mml_bw < 800)
		g_priv->dvfs_bw.mml_bw = 0;
	total_bw = g_priv->dvfs_bw.disp_bw + g_priv->dvfs_bw.mml_bw;

	last_bw_level = g_priv->dvfs_bw.bw_level;
	g_priv->dvfs_bw.bw_level = dpc_dvfs_bw_to_level(total_bw);

	max_level_bw = dpc_max_dvfs_level(DPC_SUBSYS_VDISP_LEVEL_OF_BW);
	/* bw level and disp level share the same vote*/
	if (MTK_DPC_OF_DISP_SUBSYS(subsys))
		max_level_subsys = DPC_VDISP_LEVEL_IGNORE;
	else
		max_level_subsys = dpc_max_dvfs_level(subsys);

	if (max_level_subsys == DPC_VDISP_LEVEL_IGNORE &&
		max_level_bw == DPC_VDISP_LEVEL_IGNORE) {
		spin_lock_irqsave(&dpc_lock, flags);
		/* switch vdisp to SW or HW mode */
		avail = mtk_dpc_support_cap(DPC_VIDLE_LOWER_VDISP_DVFS);
		writel((force || !avail) ? 1 : 0, dpc_base + addr);
		spin_unlock_irqrestore(&dpc_lock, flags);

		if (unlikely(debug_dvfs_vdisp))
			DPCFUNC(
				"subsys:%d ignore bwl:%u->%u,subl:%u->%u,max:%u/%u[%u,%u,%u(%u,%u)],f:%u,reg[0x%x,0x%x]",
				subsys, last_bw_level, g_priv->dvfs_bw.bw_level,
				last_level, level, max_level_subsys, max_level_bw,
				g_priv->dvfs_bw.disp_level, g_priv->dvfs_bw.mml_level,
				g_priv->dvfs_bw.bw_level, g_priv->dvfs_bw.disp_bw,
				g_priv->dvfs_bw.mml_bw, force,
				readl(dpc_base + DISP_REG_DPC_DISP_VDISP_DVFS_VAL),
				readl(dpc_base + DISP_REG_DPC_MML_VDISP_DVFS_VAL));
		goto out;
	}
	if (unlikely(debug_dvfs_vdisp))
		DPCFUNC("subsys:%d update bwl:%u->%u,subl:%u->%u,max:%u/%u->%u/%u,[%u,%u,%u(%u,%u)],f:%u",
			subsys, last_bw_level, g_priv->dvfs_bw.bw_level, last_level, level,
			g_vdisp_level_disp, g_vdisp_level_mml, max_level_bw, max_level_subsys,
			g_priv->dvfs_bw.disp_level, g_priv->dvfs_bw.mml_level,
			g_priv->dvfs_bw.bw_level, g_priv->dvfs_bw.disp_bw,
			g_priv->dvfs_bw.mml_bw, force);

	if (mtk_mmdvfs_enable_vcp(true, mmdvfs_user) < 0)
		mmdvfs_state = false;

	spin_lock_irqsave(&dpc_lock, flags);
	if (max_level_subsys != DPC_VDISP_LEVEL_IGNORE) {
		max_level = max_level_subsys;
		vdisp_level_set_vcp(subsys, max_level_subsys, mmdvfs_state);
	}
	if (max_level_bw != DPC_VDISP_LEVEL_IGNORE) {
		max_level = (max_level_bw > max_level) ? max_level_bw : max_level;
		vdisp_level_set_vcp(DPC_SUBSYS_VDISP_LEVEL_OF_BW, max_level_bw, mmdvfs_state);
	}

	/* switch vdisp to SW or HW mode */
	avail = mtk_dpc_support_cap(DPC_VIDLE_LOWER_VDISP_DVFS);
	writel((force || !avail) ? 1 : 0, dpc_base + addr);

	dpc_mmp(vdisp_level, MMPROFILE_FLAG_PULSE,
		(g_priv->dvfs_bw.bw_level << 16) |
		(g_priv->dvfs_bw.mml_level << 8) | g_priv->dvfs_bw.disp_level,
		(BIT(subsys) << 16) | max_level);

	spin_unlock_irqrestore(&dpc_lock, flags);
	if (mmdvfs_state)
		mtk_mmdvfs_enable_vcp(false, mmdvfs_user);

out:
	mutex_unlock(&dvfs_lock);
	dpc_pm_ctrl(false);
}
EXPORT_SYMBOL(dpc_dvfs_both_set);

void dpc_group_enable_func(const u16 group, bool en, bool lock)
{
	unsigned long flags = 0;

	if (g_panel_type >= PANEL_TYPE_COUNT && en)
		return;

	if (dpc_pm_ctrl(true))
		return;

	if (lock)
		spin_lock_irqsave(&dpc_lock, flags);

	if (group <= DPC_DISP_VIDLE_RESERVED)
		dpc_disp_group_enable_func((enum mtk_dpc_disp_vidle)group, en, true);
	else if (group <= DPC_MML_VIDLE_RESERVED)
		dpc_mml_group_enable_func((enum mtk_dpc_mml_vidle)group, en, true);
	else
		DPCERR("group(%u) is not defined", group);

	if (lock)
		spin_unlock_irqrestore(&dpc_lock, flags);
	dpc_pm_ctrl(false);
}

void dpc_pause(const enum mtk_dpc_subsys subsys, bool en)
{
	unsigned long flags = 0;

	if (g_panel_type >= PANEL_TYPE_COUNT && !en)
		return;

	if (g_priv->vidle_mask == 0 && !en)
		return;

	if (dpc_pm_ctrl(true))
		return;

	spin_lock_irqsave(&dpc_lock, flags);
	if (!en && dpc_vidle_is_available() == 0) {
		DPCERR("in-available, cap:0x%x, dur:%u-%u, panel:%d, en:%d",
			g_priv->vidle_mask, g_te_duration, g_vb_duration,
			g_panel_type, en);
		goto out;
	}

	dpc_mmp(dpc, MMPROFILE_FLAG_PULSE, BIT(subsys), !en);
	if (MTK_DPC_OF_DISP_SUBSYS(subsys))
		dpc_disp_group_pause(en);
	else if (MTK_DPC_OF_MML_SUBSYS(subsys))
		dpc_mml_group_pause(en);
	else {
		DPCERR("invalid subsys:%d", subsys);
		WARN_ON(1);
	}

out:
	spin_unlock_irqrestore(&dpc_lock, flags);
	dpc_pm_ctrl(false);
}
EXPORT_SYMBOL(dpc_pause);

void dpc_group_enable(const u16 group, bool en)
{
	dpc_group_enable_func(group, en, true);
}
EXPORT_SYMBOL(dpc_group_enable);

void dpc_config(const enum mtk_dpc_subsys subsys, bool en)
{
	unsigned int mask = 0x0;
	unsigned long flags = 0;

	if (g_panel_type >= PANEL_TYPE_COUNT && en)
		return;

	if (g_priv->vidle_mask == 0 && en)
		return;

	if (dpc_pm_ctrl(true))
		return;

	spin_lock_irqsave(&dpc_lock, flags);
	if (en && dpc_vidle_is_available() == 0) {
		DPCERR("in-available, cap:0x%x, dur:%u-%u, panel:%d, en:%d",
			g_priv->vidle_mask, g_te_duration, g_vb_duration,
			g_panel_type, en);
		goto out;
	}

	if (en && atomic_read(&g_priv->dpc_en_cnt) == 0) {
		DPCERR("enable DT before config dpc group, en:%d, cnt:%d",
			en, atomic_read(&g_priv->dpc_en_cnt));
		goto out;
	}

	if (MTK_DPC_OF_DISP_SUBSYS(subsys) && en &&
		atomic_read(&g_disp_mode) == DPC_VIDLE_SW_MANUAL_MODE) {
		DPCDUMP("ignore disp config, disp_mode:%d, en:%d", atomic_read(&g_disp_mode), en);
		dpc_mmp(disp_group, MMPROFILE_FLAG_PULSE, atomic_read(&g_disp_mode), 0x5a5a);
		goto out;
	} else if (MTK_DPC_OF_MML_SUBSYS(subsys) && en &&
		atomic_read(&g_mml_mode) == DPC_VIDLE_SW_MANUAL_MODE) {
		DPCDUMP("ignore mml config, mml_mode:%d, en:%d", atomic_read(&g_mml_mode), en);
		dpc_mmp(mml_group, MMPROFILE_FLAG_PULSE, atomic_read(&g_mml_mode), 0x5a5a);
		goto out;
	} else if (MTK_DPC_OF_INVALID_SUBSYS(subsys)){
		DPCERR("invalid subsys:%d", subsys);
		WARN_ON(1);
		goto out;
	}

	if (MTK_DPC_OF_DISP_SUBSYS(subsys)) {
		dpc_mmp(group, MMPROFILE_FLAG_PULSE, BIT(DPC_SUBSYS_DISP), en);
		dpc_mmp(disp_group, MMPROFILE_FLAG_START, atomic_read(&g_disp_mode), en);
		if (en)
			atomic_set(&g_disp_mode, DPC_VIDLE_HW_AUTO_MODE);
	} else {
		dpc_mmp(group, MMPROFILE_FLAG_PULSE, BIT(DPC_SUBSYS_MML), en);
		dpc_mmp(mml_group, MMPROFILE_FLAG_START, atomic_read(&g_mml_mode), en);
		if (en)
			atomic_set(&g_mml_mode, DPC_VIDLE_HW_AUTO_MODE);
	}

	if (!en) {
		if (MTK_DPC_OF_DISP_SUBSYS(subsys)) {
			dpc_mtcmos_vote(DPC_SUBSYS_DISP1, 6, 1);
			mtk_disp_wait_pwr_ack(DPC_SUBSYS_DISP1);
			dpc_mtcmos_vote(DPC_SUBSYS_DISP0, 6, 1);
			mtk_disp_wait_pwr_ack(DPC_SUBSYS_DISP0);
			dpc_mtcmos_vote(DPC_SUBSYS_OVL0, 6, 1);
			mtk_disp_wait_pwr_ack(DPC_SUBSYS_OVL0);
			dpc_mtcmos_vote(DPC_SUBSYS_OVL1, 6, 1);
			mtk_disp_wait_pwr_ack(DPC_SUBSYS_OVL1);
			if (readl(dpc_base + DISP_REG_DPC_MML1_MTCMOS_CFG)) {
				dpc_mtcmos_vote(DPC_SUBSYS_MML1, 6, 1);
				mtk_disp_wait_pwr_ack(DPC_SUBSYS_MML1);
			}
		} else {
			dpc_mtcmos_vote(DPC_SUBSYS_MML1, 6, 1);
			mtk_disp_wait_pwr_ack(DPC_SUBSYS_MML1);
		}
		udelay(30);
	}

	mask = mtk_dpc_get_vidle_mask(subsys, en);
	if (MTK_DPC_OF_DISP_SUBSYS(subsys)) {
		writel(mask, dpc_base + DISP_REG_DPC_DISP_MASK_CFG);
		writel(0x1f, dpc_base + DISP_REG_DPC_DISP_EXT_INPUT_EN);
		writel(0x3ff, dpc_base + DISP_REG_DPC_DISP_DT_FOLLOW_CFG); /* all follow 11~13 */

		dpc_group_enable_func(DPC_DISP_VIDLE_MTCMOS, en, false);
		dpc_group_enable_func(DPC_DISP_VIDLE_MTCMOS_DISP1, en, false);
		dpc_group_enable_func(DPC_DISP_VIDLE_VDISP_DVFS, en, false);
		dpc_group_enable_func(DPC_DISP_VIDLE_HRT_BW, en, false);
		dpc_group_enable_func(DPC_DISP_VIDLE_MMINFRA_OFF, en, false);
	} else {
		writel(mask, dpc_base + DISP_REG_DPC_MML_MASK_CFG);
		writel(0x3, dpc_base + DISP_REG_DPC_MML_EXT_INPUT_EN);
		writel(0x3ff, dpc_base + DISP_REG_DPC_MML_DT_FOLLOW_CFG); /* all follow 39~41 */

		dpc_group_enable_func(DPC_MML_VIDLE_MTCMOS, en, false);
		dpc_group_enable_func(DPC_MML_VIDLE_VDISP_DVFS, en, false);
		dpc_group_enable_func(DPC_MML_VIDLE_HRT_BW, en, false);
		dpc_group_enable_func(DPC_MML_VIDLE_MMINFRA_OFF, en, false);
	}

	if (en) {
		/* pwr on delay default 100 + 50 us, modify to 30 us */
		writel(0x30c, dpc_base + 0xa44);
		writel(0x30c, dpc_base + 0xb44);
		writel(0x30c, dpc_base + 0xc44);
		writel(0x30c, dpc_base + 0xd44);
		writel(0x30c, dpc_base + 0xe44);

		if (MTK_DPC_OF_DISP_SUBSYS(subsys)) {
			dpc_mtcmos_vote(DPC_SUBSYS_DISP1, 6, 0);
			dpc_mtcmos_vote(DPC_SUBSYS_DISP0, 6, 0);
			dpc_mtcmos_vote(DPC_SUBSYS_OVL0, 6, 0);
			dpc_mtcmos_vote(DPC_SUBSYS_OVL1, 6, 0);
			if (readl(dpc_base + DISP_REG_DPC_MML1_MTCMOS_CFG)) {
				dpc_mtcmos_vote(DPC_SUBSYS_MML1, 6, 0);
				mtk_disp_wait_pwr_ack(DPC_SUBSYS_MML1);
			}
		} else
			dpc_mtcmos_vote(DPC_SUBSYS_MML1, 6, 0);
	}

	writel(0, dpc_base + DISP_REG_DPC_MERGE_DISP_INT_CFG);
	writel(0, dpc_base + DISP_REG_DPC_MERGE_MML_INT_CFG);

	/* wla ddren ack */
	writel(1, dpc_base + DISP_REG_DPC_DDREN_ACK_SEL);

	if (debug_irq && en) {
		dpc_irq_enable(DPC_SUBSYS_DISP, true, false);
		dpc_irq_enable(DPC_SUBSYS_MML, true, false);
	}

	if (mtk_dpc_support_cap(DPC_VIDLE_MTCMOS_OFF) == 0) {
		dpc_mtcmos_vote(DPC_SUBSYS_DISP1, 5, 1);
		dpc_mtcmos_vote(DPC_SUBSYS_DISP0, 5, 1);
		dpc_mtcmos_vote(DPC_SUBSYS_OVL0, 5, 1);
		dpc_mtcmos_vote(DPC_SUBSYS_OVL1, 5, 1);
		dpc_mtcmos_vote(DPC_SUBSYS_MML1, 5, 1);
	} else {
		dpc_mtcmos_vote(DPC_SUBSYS_DISP1, 5, 0);
		dpc_mtcmos_vote(DPC_SUBSYS_DISP0, 5, 0);
		dpc_mtcmos_vote(DPC_SUBSYS_OVL0, 5, 0);
		dpc_mtcmos_vote(DPC_SUBSYS_OVL1, 5, 0);
		dpc_mtcmos_vote(DPC_SUBSYS_MML1, 5, 0);
	}

	if (MTK_DPC_OF_DISP_SUBSYS(subsys)) {
		if (!en) {
			if (g_idle_ratio_debug) {
				mtk_dpc_idle_ratio_debug(DPC_VIDLE_RATIO_STOP);
				mtk_dpc_idle_ratio_debug(DPC_VIDLE_RATIO_DUMP);
			}
			atomic_set(&g_disp_mode, DPC_VIDLE_INACTIVE_MODE);
		}
		dpc_mmp(disp_group, MMPROFILE_FLAG_END, mask, atomic_read(&g_disp_mode));
	} else {
		if (!en) {
			if (g_idle_ratio_debug) {
				mtk_dpc_idle_ratio_debug(DPC_VIDLE_RATIO_STOP);
				mtk_dpc_idle_ratio_debug(DPC_VIDLE_RATIO_DUMP);
			}
			atomic_set(&g_mml_mode, DPC_VIDLE_INACTIVE_MODE);
		}
		dpc_mmp(mml_group, MMPROFILE_FLAG_END, mask, atomic_read(&g_mml_mode));
	}

out:
	spin_unlock_irqrestore(&dpc_lock, flags);
	dpc_pm_ctrl(false);
	if (unlikely(debug_dvfs_vdisp))
		DPCFUNC("vdisp status[%u,%u,%u(%u,%u)],reg:[0x%x,0x%x]",
			g_priv->dvfs_bw.disp_level, g_priv->dvfs_bw.mml_level,
			g_priv->dvfs_bw.bw_level, g_priv->dvfs_bw.disp_bw,
			g_priv->dvfs_bw.mml_bw,
			readl(dpc_base + DISP_REG_DPC_DISP_VDISP_DVFS_VAL),
			readl(dpc_base + DISP_REG_DPC_MML_VDISP_DVFS_VAL));
}
EXPORT_SYMBOL(dpc_config);

void dpc_mtcmos_vote(const enum mtk_dpc_subsys subsys, const u8 thread, const bool en)
{
	static u32 st_ovl0, st_disp1, st_mml1;
	u32 addr = 0, val;

	if (mtk_dpc_support_cap(DPC_VIDLE_MTCMOS_OFF) == 0)
		return;

	if (dpc_pm_ctrl(true))
		return;

	/* CLR : execute SW threads, disable auto MTCMOS */
	switch (subsys) {
	case DPC_SUBSYS_DISP0:
		addr = en ? DISP_REG_DPC_DISP0_THREADx_CLR(thread)
			  : DISP_REG_DPC_DISP0_THREADx_SET(thread);
		writel(1, dpc_base + addr);
		break;
	case DPC_SUBSYS_DISP1:
		addr = en ? DISP_REG_DPC_DISP1_THREADx_CLR(thread)
			  : DISP_REG_DPC_DISP1_THREADx_SET(thread);
		writel(1, dpc_base + addr);

		if (likely(debug_mmp)) {
			val = readl(dpc_base + DISP_REG_DPC_DISP1_THREADx_CFG(thread)) &
					DISP_DPC_SUBSYS_THREAD_EN;
			if (en && val == 0) {
				if (st_disp1 == 0)
					dpc_mmp(disp1_vote, MMPROFILE_FLAG_START, en, BIT(thread));
				st_disp1 |= BIT(thread);
			} else if (!en && st_disp1 > 0 && val == DISP_DPC_SUBSYS_THREAD_EN) {
				st_disp1 &= ~BIT(thread);
				if (st_disp1 == 0)
					dpc_mmp(disp1_vote, MMPROFILE_FLAG_END, en, BIT(thread));
			}
			dpc_mmp(disp1_vote, MMPROFILE_FLAG_PULSE, en, st_disp1);
		}
		break;
	case DPC_SUBSYS_OVL0:
		addr = en ? DISP_REG_DPC_OVL0_THREADx_CLR(thread)
			  : DISP_REG_DPC_OVL0_THREADx_SET(thread);
		writel(1, dpc_base + addr);

		if (likely(debug_mmp)) {
			val = readl(dpc_base + DISP_REG_DPC_OVL0_THREADx_CFG(thread)) &
					DISP_DPC_SUBSYS_THREAD_EN;
			if (en && val == 0) {
				if (st_ovl0 == 0)
					dpc_mmp(ovl0_vote, MMPROFILE_FLAG_START, en, BIT(thread));
				st_ovl0 |= BIT(thread);
			} else if (!en && st_ovl0 > 0 && val == DISP_DPC_SUBSYS_THREAD_EN) {
				st_ovl0 &= ~BIT(thread);
				if (st_ovl0 == 0)
					dpc_mmp(ovl0_vote, MMPROFILE_FLAG_END, en, BIT(thread));
			}
			dpc_mmp(ovl0_vote, MMPROFILE_FLAG_PULSE, en, st_ovl0);
		}
		break;
	case DPC_SUBSYS_OVL1:
		addr = en ? DISP_REG_DPC_OVL1_THREADx_CLR(thread)
			  : DISP_REG_DPC_OVL1_THREADx_SET(thread);
		writel(1, dpc_base + addr);
		break;
	case DPC_SUBSYS_MML1:
		addr = en ? DISP_REG_DPC_MML1_THREADx_CLR(thread)
			  : DISP_REG_DPC_MML1_THREADx_SET(thread);
		writel(1, dpc_base + addr);

		if (likely(debug_mmp)) {
			val = readl(dpc_base + DISP_REG_DPC_MML1_THREADx_CFG(thread)) &
					DISP_DPC_SUBSYS_THREAD_EN;
			if (en && val == 0) {
				if (st_mml1 == 0)
					dpc_mmp(mml1_vote, MMPROFILE_FLAG_START, en, BIT(thread));
				st_mml1 |= BIT(thread);
			} else if (!en && st_mml1 > 0 && val == DISP_DPC_SUBSYS_THREAD_EN) {
				st_mml1 &= ~BIT(thread);
				if (st_mml1 == 0)
					dpc_mmp(mml1_vote, MMPROFILE_FLAG_END, en, BIT(thread));
			}
			dpc_mmp(mml1_vote, MMPROFILE_FLAG_PULSE, en, st_mml1);
		}
		break;
	default:
		break;
	}

	dpc_pm_ctrl(false);
}
EXPORT_SYMBOL(dpc_mtcmos_vote);

irqreturn_t mtk_dpc_disp_irq_handler(int irq, void *dev_id)
{
	struct mtk_dpc *priv = dev_id;
	u32 status;
	irqreturn_t ret = IRQ_NONE;

	if (IS_ERR_OR_NULL(priv))
		return ret;

	if (dpc_pm_ctrl(true)) {
		dpc_mmp(mminfra_off, MMPROFILE_FLAG_PULSE, U32_MAX, 0);
		return ret;
	}

	status = readl(dpc_base + DISP_REG_DPC_DISP_INTSTA);
	if (!status)
		goto out;

	writel(~status, dpc_base + DISP_REG_DPC_DISP_INTSTA);

	if (likely(debug_mmp)) {
		dpc_mmp(disp_irq, MMPROFILE_FLAG_PULSE, 0, status);

		if (status & DISP_DPC_INT_DT6) {
			dpc_mmp(prete, MMPROFILE_FLAG_PULSE, 0, DISP_DPC_INT_DT6);
			mtk_update_dpc_state(DPC_VIDLE_BW_MASK | DPC_VIDLE_VDISP_MASK, false);
		}

		if (((status & DISP_DPC_INT_DT7) && g_panel_type == PANEL_TYPE_CMD) ||
			((status & DISP_DPC_INT_DT4) && g_panel_type == PANEL_TYPE_VDO)) {
			if ((status & DISP_DPC_INT_DT5) == 0)
				mtk_update_dpc_state(DPC_VIDLE_APSRC_MASK |
						DPC_VIDLE_DISP_WINDOW | DPC_VIDLE_BW_MASK |
						DPC_VIDLE_VDISP_MASK, true);
		}

		if (status & DISP_DPC_INT_MMINFRA_OFF_START) {
			if ((status & DISP_DPC_INT_MMINFRA_OFF_END) == 0)
				mtk_update_dpc_state(DPC_VIDLE_MMINFRA_MASK, true);
		}

		if (status & DISP_DPC_INT_MMINFRA_OFF_END)
			mtk_update_dpc_state(DPC_VIDLE_MMINFRA_MASK, false);

		if (debug_mtcmos_off && (status & DISP_DPC_INT_OVL0_OFF)) {
			if ((status & DISP_DPC_INT_OVL0_ON) == 0)
				mtk_update_dpc_state(DPC_VIDLE_OVL0_MASK, true);
		}

		if (debug_mtcmos_off && (status & DISP_DPC_INT_OVL0_ON))
			mtk_update_dpc_state(DPC_VIDLE_OVL0_MASK, false);

		if (status & DISP_DPC_INT_DT5) {
			mtk_update_dpc_state(DPC_VIDLE_DISP_WINDOW |
					DPC_VIDLE_APSRC_MASK, false);
		}

		if (debug_mtcmos_off && (status & DISP_DPC_INT_DISP1_OFF)) {
			if ((status & DISP_DPC_INT_DISP1_ON) == 0)
				mtk_update_dpc_state(DPC_VIDLE_DISP1_MASK, true);
		}

		if (debug_mtcmos_off && (status & DISP_DPC_INT_DISP1_ON))
			mtk_update_dpc_state(DPC_VIDLE_DISP1_MASK, false);
	}

	if (unlikely(debug_check_reg) && g_priv->mmsys_id == MMSYS_MT6989 &&
		priv->sys_va[HW_VOTE_STATE] && priv->sys_va[VDISP_DVFSRC_DEBUG] &&
		priv->sys_va[SPM_BASE] && priv->sys_va[VCORE_DVFSRC_DEBUG]) {
		if (status & DISP_DPC_INT_DT29) {	/* should be the last off irq */
			DPCFUNC("\tOFF MMINFRA(%u) VDISP(%u) SRT&HRT(%#x) D1(%u) D234(%u)",
				readl(priv->sys_va[HW_VOTE_STATE]) & BIT(6) ? 1 : 0,
				(readl(priv->sys_va[VDISP_DVFSRC_DEBUG]) & 0x1C) >> 2,
				readl(priv->sys_va[VCORE_DVFSRC_DEBUG]) & 0xFFFFF,
				(readl(priv->sys_va[SPM_BASE] + SPM_REQ_STA_4_MT6989) &
						SPM_REQ_APSRC_STATE_MT6989) ? 1 : 0,
				((readl(priv->sys_va[SPM_BASE] + SPM_REQ_STA_5_MT6989) &
						0x13) == 0x13) ? 1 : 0);
		}
		if (status & DISP_DPC_INT_DT30) {	/* should be the last on irq */
			DPCFUNC("\tON MMINFRA(%u) VDISP(%u) SRT&HRT(%#x) D1(%u) D234(%u)",
				readl(priv->sys_va[HW_VOTE_STATE]) & BIT(6) ? 1 : 0,
				(readl(priv->sys_va[VDISP_DVFSRC_DEBUG]) & 0x1C) >> 2,
				readl(priv->sys_va[VCORE_DVFSRC_DEBUG]) & 0xFFFFF,
				(readl(priv->sys_va[SPM_BASE] + SPM_REQ_STA_4_MT6989) &
						SPM_REQ_APSRC_STATE_MT6989) ? 1 : 0,
				((readl(priv->sys_va[SPM_BASE] + SPM_REQ_STA_5_MT6989) &
						0x13) == 0x13) ? 1 : 0);
		}
	}

	if (unlikely(debug_check_rtff && (status & 0x600000))) {
		int i, sum = 0;

		for (i = 0; i < DPC_DEBUG_RTFF_CNT; i++)
			sum += readl(debug_rtff[i]);

		if (status & DISP_DPC_INT_DT29)			/* should be the last off irq */
			DPCFUNC("\tOFF rtff(%#x)", sum);
		if (status & DISP_DPC_INT_DT30)			/* should be the last on irq */
			DPCFUNC("\tON rtff(%#x)", sum);
	}

	if (unlikely(debug_check_event)) {
		if (status & DISP_DPC_INT_DT29) {	/* should be the last off irq */
			DPCFUNC("\tOFF event(%#06x)", readl(dpc_base + DISP_REG_DPC_DUMMY0));
			writel(0, dpc_base + DISP_REG_DPC_DUMMY0);
		}
		if (status & DISP_DPC_INT_DT30) {	/* should be the last on irq */
			DPCFUNC("\tON event(%#06x)", readl(dpc_base + DISP_REG_DPC_DUMMY0));
			writel(0, dpc_base + DISP_REG_DPC_DUMMY0);
		}
	}

	if (debug_mtcmos_off && !debug_irq && (status & 0xFF000000)) {
		if (status & DISP_DPC_INT_DISP1_OFF)
			mtk_dprec_logger_pr(1, "DISP1 OFF\n");
		if (status & DISP_DPC_INT_DISP1_ON)
			mtk_dprec_logger_pr(1, "DISP1 ON\n");
	}

	ret = IRQ_HANDLED;

out:
	dpc_pm_ctrl(false);

	return ret;
}

irqreturn_t mtk_dpc_mml_irq_handler(int irq, void *dev_id)
{
	struct mtk_dpc *priv = dev_id;
	u32 status;
	irqreturn_t ret = IRQ_NONE;
	unsigned int val = 0;

	if (IS_ERR_OR_NULL(priv))
		return ret;

	if (dpc_pm_ctrl(true)) {
		dpc_mmp(mminfra_off, MMPROFILE_FLAG_PULSE, U32_MAX, 0);
		return ret;
	}

	status = readl(dpc_base + DISP_REG_DPC_MML_INTSTA);
	if (!status)
		goto out;

	writel(~status, dpc_base + DISP_REG_DPC_MML_INTSTA);

	if (likely(debug_mmp)) {
		dpc_mmp(mml_irq, MMPROFILE_FLAG_PULSE, 0, status);

		if (status & MML_DPC_INT_MML1_SOF)
			dpc_mmp(mml_sof, MMPROFILE_FLAG_PULSE, 0, MML_DPC_INT_MML1_RDONE);
		if (status & MML_DPC_INT_MML1_RDONE)
			dpc_mmp(mml_rrot_done, MMPROFILE_FLAG_PULSE, 0, MML_DPC_INT_MML1_RDONE);

		if (debug_mtcmos_off && (status & MML_DPC_INT_MML1_OFF)) {
			if ((status & MML_DPC_INT_MML1_ON) == 0)
				mtk_update_dpc_state(DPC_VIDLE_MML1_MASK, true);
		}

		if (debug_mtcmos_off && (status & MML_DPC_INT_MML1_ON))
			mtk_update_dpc_state(DPC_VIDLE_MML1_MASK, false);

		if ((status & MML_DPC_INT_DT35) || (status & MML_DPC_INT_DT32)) {
			if ((atomic_read(&g_vidle_window) & DPC_VIDLE_MML_DC_WINDOW) &&
				(status & MML_DPC_INT_DT33) == 0)
				mtk_update_dpc_state(DPC_VIDLE_MML_DC_WINDOW, true);
			atomic_set(&priv->dpc_state, DPC_STATE_OFF);
			wake_up_interruptible(&priv->dpc_state_wq);
		}

		if (status & MML_DPC_INT_DT33) {
			if (atomic_read(&g_vidle_window) & DPC_VIDLE_MML_DC_WINDOW)
				mtk_update_dpc_state(DPC_VIDLE_MML_DC_WINDOW, false);
			atomic_set(&priv->dpc_state, DPC_STATE_ON);
			wake_up_interruptible(&priv->dpc_state_wq);
		}

		if (status & MML_DPC_INT_DT54) {
			if (priv->get_sys_status)
				priv->get_sys_status(SYS_STATE_VLP_VOTE, &val);
			else
				val = 0;
			dpc_mmp(gce_vote, MMPROFILE_FLAG_PULSE, 1, val);
			mtk_dpc_update_vlp_state(0x54 << 16, val, false);
		}
		if (status & MML_DPC_INT_DT55) {
			if (priv->get_sys_status)
				priv->get_sys_status(SYS_STATE_VLP_VOTE, &val);
			else
				val = 0;
			dpc_mmp(gce_vote, MMPROFILE_FLAG_PULSE, 0, val);
			mtk_dpc_update_vlp_state(0x55 << 16, val, false);
		}
	}

	ret = IRQ_HANDLED;

out:
	dpc_pm_ctrl(false);

	return ret;
}

static int dpc_res_init(struct mtk_dpc *priv)
{
	int i;
	int ret = 0;
	struct resource *res;

	for (i = 0; i < DPC_SYS_REGS_CNT; i++) {
		res = platform_get_resource_byname(priv->pdev, IORESOURCE_MEM, reg_names[i]);
		if (res == NULL) {
			DPCERR("miss reg in node, i:%d, %s", i, reg_names[i]);
			ret = -1;
			continue;
		}
		priv->sys_va[i] = devm_ioremap_resource(priv->dev, res);
		if (IS_ERR_OR_NULL(priv->sys_va[i])) {
			if (IS_ERR_OR_NULL(res)) {
				DPCERR("failed to get %s, i:%d invalid res", reg_names[i], i);
				ret = -1;
				continue;
			}
			priv->sys_va[i] = ioremap(res->start, (res->end - res->start + 1));
			if (IS_ERR_OR_NULL(priv->sys_va[i])) {
				DPCERR("failed to force map %s, pa:0x%llx~0x%llx",
					reg_names[i], res->start, res->end);
				priv->sys_va[i] = NULL;
			} else
				DPCDUMP("force map %s, va:0x%lx, pa:0x%llx~0x%llx",
					reg_names[i], (unsigned long)priv->sys_va[i],
					res->start, res->end);
		}

		if (!priv->dpc_pa && i == DPC_BASE && res)
			priv->dpc_pa = res->start;
		if (!priv->vlp_pa && i == VLP_BASE && res)
			priv->vlp_pa = res->start;
	}

	dpc_base = priv->sys_va[DPC_BASE];

	return ret;
}

static void dpc_enable_merge_irq(bool enable)
{
	unsigned int val = enable ? 0x1 : 0x0;

	if (dpc_pm_ctrl(true))
		return;

	/* disable merge irq */
	writel(val, dpc_base + DISP_REG_DPC_MERGE_DISP_INT_CFG);
	writel(0, dpc_base + DISP_REG_DPC_MERGE_DISP_INTSTA);
	writel(val, dpc_base + DISP_REG_DPC_MERGE_MML_INT_CFG);
	writel(0, dpc_base + DISP_REG_DPC_MERGE_MML_INTSTA);

	dpc_pm_ctrl(false);
}

static int dpc_irq_init(struct mtk_dpc *priv)
{
	int ret = 0;
	int num_irqs;

	dpc_enable_merge_irq(false);

	num_irqs = platform_irq_count(priv->pdev);
	if (num_irqs <= 0) {
		DPCERR("unable to count IRQs");
		return -EPROBE_DEFER;
	} else if (num_irqs == 1) {
		priv->disp_irq = platform_get_irq(priv->pdev, 0);
	} else if (num_irqs == 2) {
		priv->disp_irq = platform_get_irq(priv->pdev, 0);
		priv->mml_irq = platform_get_irq(priv->pdev, 1);
	}

	if (priv->disp_irq > 0) {
		ret = devm_request_irq(priv->dev, priv->disp_irq, mtk_dpc_disp_irq_handler,
				IRQF_TRIGGER_NONE | IRQF_SHARED, dev_name(priv->dev), priv);
		if (ret)
			DPCERR("disp devm_request_irq %d fail: %d", priv->disp_irq, ret);
	}
	if (priv->mml_irq > 0) {
		ret = devm_request_irq(priv->dev, priv->mml_irq, mtk_dpc_mml_irq_handler,
				IRQF_TRIGGER_NONE | IRQF_SHARED, dev_name(priv->dev), priv);
		if (ret)
			DPCERR("mml devm_request_irq %d fail: %d", priv->mml_irq, ret);
	}
	DPCFUNC("disp irq %d, mml irq %d, ret %d", priv->disp_irq, priv->mml_irq, ret);

	return ret;
}

static void dpc_debug_event(void)
{
	u16 event_ovl0_on, event_ovl0_off, event_disp1_on, event_disp1_off;
	struct cmdq_pkt *pkt;

	of_property_read_u16(g_priv->dev->of_node, "event-ovl0-off", &event_ovl0_off);
	of_property_read_u16(g_priv->dev->of_node, "event-ovl0-on", &event_ovl0_on);
	of_property_read_u16(g_priv->dev->of_node, "event-disp1-off", &event_disp1_off);
	of_property_read_u16(g_priv->dev->of_node, "event-disp1-on", &event_disp1_on);

	if (!event_ovl0_off || !event_ovl0_on || !event_disp1_off || !event_disp1_on) {
		DPCERR("read event fail");
		return;
	}

	g_priv->cmdq_client = cmdq_mbox_create(g_priv->dev, 0);
	if (!g_priv->cmdq_client) {
		DPCERR("cmdq_mbox_create fail");
		return;
	}

	cmdq_mbox_enable(g_priv->cmdq_client->chan);
	pkt = cmdq_pkt_create(g_priv->cmdq_client);
	if (!pkt) {
		DPCERR("cmdq_handle is NULL");
		return;
	}

	cmdq_pkt_wfe(pkt, event_ovl0_off);
	cmdq_pkt_write(pkt, NULL, g_priv->dpc_pa + DISP_REG_DPC_DUMMY0, 0x1000, 0x1000);
	cmdq_pkt_wfe(pkt, event_disp1_off);
	cmdq_pkt_write(pkt, NULL, g_priv->dpc_pa + DISP_REG_DPC_DUMMY0, 0x0100, 0x0100);

	/* DT29 done, off irq handler read and clear */

	cmdq_pkt_wfe(pkt, event_disp1_on);
	cmdq_pkt_write(pkt, NULL, g_priv->dpc_pa + DISP_REG_DPC_DUMMY0, 0x0010, 0x0010);
	cmdq_pkt_wfe(pkt, event_ovl0_on);
	cmdq_pkt_write(pkt, NULL, g_priv->dpc_pa + DISP_REG_DPC_DUMMY0, 0x0001, 0x0001);

	/* DT30 done, on irq handler read and clear */

	cmdq_pkt_finalize_loop(pkt);
	cmdq_pkt_flush_threaded(pkt, NULL, (void *)pkt);
}

static void mtk_disp_enable_gce_vote(bool enable)
{
	static bool gce_debug;
	unsigned int val, dt_mask, dt_sw_mask;

	//ignore vlp vote if mminfra always on
	if (mtk_dpc_support_cap(DPC_VIDLE_MMINFRA_PLL_OFF) == 0)
		return;
	if (gce_debug == enable)
		return;

	if (enable) {
		// enable DT of MML_DT54, MML_DT55
		val = readl(dpc_base + DISP_REG_DPC_MML_DT_EN);
		dt_mask = val | 0x00c00000;
		writel(dt_mask, dpc_base + DISP_REG_DPC_MML_DT_EN);

		// switch to SW trig mode of MML_DT54, MML_DT55
		val = readl(dpc_base + DISP_REG_DPC_MML_DT_SW_TRIG_EN);
		dt_sw_mask = val | 0x00c00000;
		writel(dt_sw_mask, dpc_base + DISP_REG_DPC_MML_DT_SW_TRIG_EN);
	} else {
		// disable DT of MML_DT54, MML_DT55
		val = readl(dpc_base + DISP_REG_DPC_MML_DT_EN);
		dt_mask = val & 0xff3fffff;
		writel(dt_mask, dpc_base + DISP_REG_DPC_MML_DT_EN);
	}
	gce_debug = enable;
	dpc_mmp(vlp_vote, MMPROFILE_FLAG_PULSE, 0, enable);
}

static void mtk_disp_vlp_vote_by_gce(struct cmdq_pkt *pkt, bool vote_set, unsigned int thread)
{
	u32 addr = vote_set ? VLP_DISP_SW_VOTE_SET : VLP_DISP_SW_VOTE_CLR;
	u32 dt = vote_set ? 54 : 55; //MML_DT54, MML_DT55
	//u32 ack = vote_set ? BIT(thread) : 0;

	//ignore vlp vote if mminfra always on
	if (mtk_dpc_support_cap(DPC_VIDLE_MMINFRA_PLL_OFF) == 0)
		return;

	if (pkt == NULL)
		return;

	//update vote
	cmdq_pkt_write(pkt, NULL, g_priv->vlp_pa + addr, BIT(thread), U32_MAX);
	cmdq_pkt_write(pkt, NULL, g_priv->vlp_pa + addr, BIT(thread), U32_MAX);

	//notify cpu for debug trace
	if (debug_mmp)
		cmdq_pkt_write(pkt, NULL, g_priv->dpc_pa + DISP_REG_DPC_DTx_SW_TRIG(dt), 0x1, 0x1);
}

static void mtk_disp_vlp_vote_by_cpu(unsigned int vote_set, unsigned int thread)
{
	u32 addr = vote_set ? VLP_DISP_SW_VOTE_SET : VLP_DISP_SW_VOTE_CLR;
	u32 ack = vote_set ? BIT(thread) : 0;
	u32 val = 0;
	u16 i = 0;

	//ignore vlp vote if mminfra always on
	if (!g_priv->sys_va[VLP_BASE] ||
		mtk_dpc_support_cap(DPC_VIDLE_MMINFRA_PLL_OFF) == 0)
		return;

	writel_relaxed(BIT(thread), g_priv->sys_va[VLP_BASE] + addr);
	do {
		writel_relaxed(BIT(thread), g_priv->sys_va[VLP_BASE] + addr);
		if (g_priv->get_sys_status)
			g_priv->get_sys_status(SYS_STATE_VLP_VOTE, &val);
		if ((val & BIT(thread)) == ack)
			break;

		if (i > 2500) {
			DPCERR("vlp vote bit(%u) timeout", thread);
			return;
		}

		udelay(2);
		i++;
	} while (1);

	/* check voter only, later will use another API to power on mminfra */
	if (debug_mmp)
		dpc_mmp(cpu_vote, MMPROFILE_FLAG_PULSE, BIT(thread) | vote_set, val);
	mtk_dpc_update_vlp_state(thread, val, false);
}

void dpc_vidle_power_keep_by_gce(struct cmdq_pkt *pkt, const enum mtk_vidle_voter_user user,
				 const u16 gpr)
{
	mtk_disp_vlp_vote_by_gce(pkt, VOTE_SET, user);
	if (gpr)
		cmdq_pkt_poll_timeout(pkt, 0xb, SUBSYS_NO_SUPPORT,
				  g_priv->dpc_pa + DISP_REG_DPC_DISP1_DEBUG1, ~0, 0xFFFF, gpr);
}
EXPORT_SYMBOL(dpc_vidle_power_keep_by_gce);

void dpc_vidle_power_release_by_gce(struct cmdq_pkt *pkt, const enum mtk_vidle_voter_user user)
{
	mtk_disp_vlp_vote_by_gce(pkt, VOTE_CLR, user);
}
EXPORT_SYMBOL(dpc_vidle_power_release_by_gce);

int dpc_vidle_power_keep(const enum mtk_vidle_voter_user user)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&g_priv->skip_force_power_lock, flags);
	if (unlikely(g_priv->skip_force_power)) {
		spin_unlock_irqrestore(&g_priv->skip_force_power_lock, flags);
		DPCFUNC("user %u skip force power", user);
		return -1;
	}

	if (dpc_pm_ctrl(true)) {
		spin_unlock_irqrestore(&g_priv->skip_force_power_lock, flags);
		DPCFUNC("%s: user %u failed to force power", __func__, user);
		return -1;
	}
	spin_unlock_irqrestore(&g_priv->skip_force_power_lock, flags);

	mtk_disp_vlp_vote_by_cpu(VOTE_SET, user);
	udelay(50);
	return 0;
}
EXPORT_SYMBOL(dpc_vidle_power_keep);

void dpc_vidle_power_release(const enum mtk_vidle_voter_user user)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&g_priv->skip_force_power_lock, flags);
	if (unlikely(g_priv->skip_force_power)) {
		spin_unlock_irqrestore(&g_priv->skip_force_power_lock, flags);
		DPCFUNC("user %u skip release power", user);
		return;
	}
	spin_unlock_irqrestore(&g_priv->skip_force_power_lock, flags);

	mtk_disp_vlp_vote_by_cpu(VOTE_CLR, user);

	dpc_pm_ctrl(false);
}
EXPORT_SYMBOL(dpc_vidle_power_release);

static void dpc_dump_regs(unsigned int start, unsigned int end)
{
	int i = start;

	while (i <= end) {
		if (i <= end - 0xc)
			DPCDUMP("DPC[0x%x]: %#08x %#08x %#08x 0x%08x",
				i, readl(dpc_base + i),
				readl(dpc_base + i + 0x4),
				readl(dpc_base + i + 0x8),
				readl(dpc_base + i + 0xc));
		else if (i <= end - 0x8)
			DPCDUMP("DPC[0x%x]: %#08x %#08x %#08x",
				i, readl(dpc_base + i),
				readl(dpc_base + i + 0x4),
				readl(dpc_base + i + 0x8));
		else if (i <= end - 0x4)
			DPCDUMP("DPC[0x%x]: %#08x %#08x",
				i, readl(dpc_base + i),
				readl(dpc_base + i + 0x4));
		else if (i <= end)
			DPCDUMP("DPC[0x%x]: %#08x",
				i, readl(dpc_base + i));
		i += 0x10;
	}

}

static void dpc_analysis(bool detail)
{
	unsigned int status = 0, value = 0;

	if (g_priv->get_sys_status)
		status = g_priv->get_sys_status(SYS_POWER_ACK_DPC, &value);
	if (!status) {
		DPCFUNC("power_off:%d, 0x%x", status, value);
		return;
	}

	if (dpc_pm_ctrl(true))
		return;

	DPCDUMP("========== DPC analysis, base:0x%llx ,panel:%d, window:0x%x, dur:%u=========",
		(unsigned long long)g_priv->dpc_pa, g_panel_type,
		atomic_read(&g_vidle_window), g_te_duration);
	mtk_dpc_dump_caps();
	DPCDUMP("DT settings: DISP_DT:0x%x,MML_DT:0x%x,DPC_EN:0x%x",
		readl(dpc_base + DISP_REG_DPC_DISP_DT_EN),
		readl(dpc_base + DISP_REG_DPC_MML_DT_EN),
		readl(dpc_base + DISP_REG_DPC_EN));
	DPCDUMP("MASK FOLLOW: disp(%#010x %#08x),mml(%#010x %#08x)",
		readl(dpc_base + DISP_REG_DPC_DISP_MASK_CFG),
		readl(dpc_base + DISP_REG_DPC_DISP_DT_FOLLOW_CFG),
		readl(dpc_base + DISP_REG_DPC_MML_MASK_CFG),
		readl(dpc_base + DISP_REG_DPC_MML_DT_FOLLOW_CFG));
	DPCDUMP("ddremi mminfra: disp(%#010x %#08x), mml(%#010x %#08x)",
		readl(dpc_base + DISP_REG_DPC_DISP_DDRSRC_EMIREQ_CFG),
		readl(dpc_base + DISP_REG_DPC_DISP_INFRA_PLL_OFF_CFG),
		readl(dpc_base + DISP_REG_DPC_MML_DDRSRC_EMIREQ_CFG),
		readl(dpc_base + DISP_REG_DPC_MML_INFRA_PLL_OFF_CFG));
	DPCDUMP("hrt cfg val: disp(%#010x %#06x), mml(%#010x %#06x)",
		readl(dpc_base + DISP_REG_DPC_DISP_HRTBW_SRTBW_CFG),
		readl(dpc_base + DISP_REG_DPC_DISP_HIGH_HRT_BW),
		readl(dpc_base + DISP_REG_DPC_MML_HRTBW_SRTBW_CFG),
		readl(dpc_base + DISP_REG_DPC_MML_SW_HRT_BW));
	DPCDUMP("vdisp cfg val: disp(%#04x %#04x), mml(%#04x %#04x)",
		readl(dpc_base + DISP_REG_DPC_DISP_VDISP_DVFS_CFG),
		readl(dpc_base + DISP_REG_DPC_DISP_VDISP_DVFS_VAL),
		readl(dpc_base + DISP_REG_DPC_MML_VDISP_DVFS_CFG),
		readl(dpc_base + DISP_REG_DPC_MML_VDISP_DVFS_VAL));
	DPCDUMP("mtcmos: (disp:%#04x %#04x %#04x %#04x mml1:%#04x)",
		readl(dpc_base + DISP_REG_DPC_DISP0_MTCMOS_CFG),
		readl(dpc_base + DISP_REG_DPC_DISP1_MTCMOS_CFG),
		readl(dpc_base + DISP_REG_DPC_OVL0_MTCMOS_CFG),
		readl(dpc_base + DISP_REG_DPC_OVL1_MTCMOS_CFG),
		readl(dpc_base + DISP_REG_DPC_MML1_MTCMOS_CFG));

	status = g_priv->get_sys_status(SYS_STATE_APSRC, &value);
	DPCDUMP("APSRC req:0x%x, value:%#04x", status, value);
	status = g_priv->get_sys_status(SYS_STATE_MMINFRA, &value);
	DPCDUMP("MMINFRA req:0x%x, value:%#04x", status, value);
	status = g_priv->get_sys_status(SYS_STATE_EMI, &value);
	DPCDUMP("EMI req:0x%x, value:%#04x", status, value);

	if (!detail)
		goto out;

	DPCDUMP("=============== DPC basic, pa:0x%llx ================",
		(unsigned long long)g_priv->dpc_pa);
	dpc_dump_regs(0x0, 0xEC);

	DPCDUMP("=============== DPC DT counter, pa:0x%llx ================",
		(unsigned long long)g_priv->dpc_pa + 0x100);
	dpc_dump_regs(0x100, 0x1E0);

	DPCDUMP("=============== DPC DT SW trigger, pa:0x%llx ================",
		(unsigned long long)g_priv->dpc_pa + 0x200);
	dpc_dump_regs(0x200, 0x2E0);

	DPCDUMP("=============== DPC DISP0_MTCMOS, pa:0x%llx ================",
		(unsigned long long)g_priv->dpc_pa + 0x300);
	dpc_dump_regs(0x300, 0x37C);

	DPCDUMP("=============== DPC DISP1_MTCMOS, pa:0x%llx ================",
		(unsigned long long)g_priv->dpc_pa + 0x400);
	dpc_dump_regs(0x400, 0x47C);

	DPCDUMP("=============== DPC OVL0_MTCMOS, pa:0x%llx ================",
		(unsigned long long)g_priv->dpc_pa + 0x500);
	dpc_dump_regs(0x500, 0x57C);

	DPCDUMP("=============== DPC OVL1_MTCMOS, pa:0x%llx ================",
		(unsigned long long)g_priv->dpc_pa + 0x600);
	dpc_dump_regs(0x600, 0x67C);

	DPCDUMP("=============== DPC MML1_MTCMOS, pa:0x%llx ================",
		(unsigned long long)g_priv->dpc_pa + 0x700);
	dpc_dump_regs(0x700, 0x77C);

	DPCDUMP("=============== DPC debug, pa:0x%llx ================",
		(unsigned long long)g_priv->dpc_pa + 0x800);
	dpc_dump_regs(0x800, 0x878);

out:
	DPCDUMP("===================================================");
	dpc_pm_ctrl(false);
}

static int dpc_pm_notifier(struct notifier_block *notifier, unsigned long pm_event, void *unused)
{
	unsigned long flags;
	u32 force_release = 0;

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		spin_lock_irqsave(&g_priv->skip_force_power_lock, flags);
		g_priv->skip_force_power = true;
		if (g_priv->pd_dev) {
			while (atomic_read(&g_priv->pd_dev->power.usage_count) > 0) {
				force_release++;
				pm_runtime_put_sync(g_priv->pd_dev);
			}
		}
		spin_unlock_irqrestore(&g_priv->skip_force_power_lock, flags);
		if (unlikely(force_release))
			DPCFUNC("dpc_dev dpc_pm unbalanced(%u)", force_release);
		dpc_mmp(skip_vote, MMPROFILE_FLAG_PULSE, U32_MAX, 1);
		return NOTIFY_OK;
	case PM_POST_SUSPEND:
		spin_lock_irqsave(&g_priv->skip_force_power_lock, flags);
		g_priv->skip_force_power = false;
		spin_unlock_irqrestore(&g_priv->skip_force_power_lock, flags);
		dpc_mmp(skip_vote, MMPROFILE_FLAG_PULSE, U32_MAX, 0);
		return NOTIFY_OK;
	}
	return NOTIFY_DONE;
}

#if IS_ENABLED(CONFIG_DEBUG_FS)
static void process_dbg_opt(const char *opt)
{
	int ret = 0;
	u32 val = 0, v1 = 0, v2 = 0;
	u32 status1= 0, status2 = 0, value = 0;

	if (g_priv->get_sys_status) {
		status1 = g_priv->get_sys_status(SYS_POWER_ACK_DPC, &value);
		status2 = g_priv->get_sys_status(SYS_POWER_ACK_DISP1_SUBSYS, &value);
	}
	if (!status1 || !status2) {
		DPCFUNC("power_off:%d,%d, 0x%x", status1, status2, value);
		return;
	}

	if (dpc_pm_ctrl(true))
		return;

	if (strncmp(opt, "en:", 3) == 0) {
		ret = sscanf(opt, "en:%u\n", &val);
		if (ret != 1)
			goto err;
		dpc_enable((bool)val);
	} else if (strncmp(opt, "cfg:", 4) == 0) {
		ret = sscanf(opt, "cfg:%u\n", &val);
		if (ret != 1)
			goto err;
		if (val == 1) {
			const u32 dummy_regs[DPC_DEBUG_RTFF_CNT] = {
				0x14000400,
				0x1402040c,
				0x14200400,
				0x14206220,
				0x14402200,
				0x14403200,
				0x14602200,
				0x14603200,
				0x1f800400,
				0x1f81a100,
			};

			for (val = 0; val < DPC_DEBUG_RTFF_CNT; val++)
				debug_rtff[val] = ioremap(dummy_regs[val], 0x4);

			/* TODO: remove me after vcore dvfsrc ready */
			if (g_priv->sys_va[VDISP_DVFSRC_EN])
				writel(0x1, g_priv->sys_va[VDISP_DVFSRC_EN]);

			/* default value for HRT and SRT */
			writel(0x66, dpc_base + DISP_REG_DPC_DISP_HIGH_HRT_BW);
			writel(0x154, dpc_base + DISP_REG_DPC_DISP_SW_SRT_BW);

			/* debug only, skip RROT Read done */
			writel(BIT(4), dpc_base + DISP_REG_DPC_MML_DT_CFG);

			dpc_config(DPC_SUBSYS_DISP, true);
			dpc_group_enable_func(DPC_DISP_VIDLE_RESERVED, true, true);
		} else {
			dpc_config(DPC_SUBSYS_DISP, false);
		}
	} else if (strncmp(opt, "mmlmutex", 8) == 0) {
		const u32 dummy_regs[6] = {
			0x1f801040,
			0x1f801044,
			0x1f801048,
			0x1f80104C,
			0x1f801050,
			0x1f801054,
		};
		for (val = 0; val < 6; val++)
			debug_rtff[val] = ioremap(dummy_regs[val], 0x4);
	} else if (strncmp(opt, "mmlcfg:", 7) == 0) {
		ret = sscanf(opt, "mmlcfg:%u\n", &val);
		if (ret != 1)
			goto err;
		dpc_config(DPC_SUBSYS_MML, (bool)val);
	} else if (strncmp(opt, "event", 5) == 0) {
		dpc_debug_event();
	} else if (strncmp(opt, "irq", 3) == 0) {
		dpc_irq_init(g_priv);
	} else if (strncmp(opt, "dump", 4) == 0) {
		dpc_analysis(true);
	} else if (strncmp(opt, "swmode:", 7) == 0) {
		ret = sscanf(opt, "swmode:%u\n", &val);
		if (ret != 1)
			goto err;
		if (val) {
			DPCDUMP("%s, %d\n", __func__, __LINE__);
			writel(0xFFFFFFFF, dpc_base + DISP_REG_DPC_DISP_DT_EN);
			writel(0xFFFFFFFF, dpc_base + DISP_REG_DPC_DISP_DT_SW_TRIG_EN);
			dpc_mmp(disp_dt, MMPROFILE_FLAG_PULSE, 0xFFFFFFFF, 0);
		} else
			writel(0, dpc_base + DISP_REG_DPC_DISP_DT_SW_TRIG_EN);
	} else if (strncmp(opt, "trig:", 5) == 0) {
		ret = sscanf(opt, "trig:%u\n", &val);
		if (ret != 1)
			goto err;
		dpc_dt_sw_trig(val);
	} else if (strncmp(opt, "vdisp:", 6) == 0) {
		ret = sscanf(opt, "vdisp:%u\n", &val);
		if (ret != 1)
			goto err;
		dpc_dvfs_set(DPC_SUBSYS_DISP, val, true);
	} else if (strncmp(opt, "vidle_cap:", 10) == 0) {
		ret = sscanf(opt, "vidle_cap:%u\n", &val);
		if (ret != 1)
			goto err;
		g_priv->vidle_mask = val;
		mtk_dpc_dump_caps();
	} else if (strncmp(opt, "dt:", 3) == 0) {
		ret = sscanf(opt, "dt:%u,%u\n", &v1, &v2);
		if (ret != 2)
			goto err;
		dpc_dt_set((u16)v1, v2);
		if (v1 < DPC_DISP_DT_CNT && g_disp_dt_usage) {
			DPCDUMP("disp dt(%u, %u->%u)", v1, g_disp_dt_usage[v1].ep, v2);
			g_disp_dt_usage[v1].ep = v2;
		} else if (g_mml_dt_usage){
			v1 -= DPC_DISP_DT_CNT;
			DPCDUMP("mml dt(%u, %u->%u)", v1, g_mml_dt_usage[v1].ep, v2);
			g_mml_dt_usage[v1].ep = v2;
		}
	} else if (strncmp(opt, "vote:", 5) == 0) {
		ret = sscanf(opt, "vote:%u,%u\n", &v1, &v2);
		if (ret != 2)
			goto err;
		dpc_mtcmos_vote(v1, 7, (bool)v2);
	} else if (strncmp(opt, "wr:", 3) == 0) {
		ret = sscanf(opt, "wr:0x%x=0x%x\n", &v1, &v2);
		if (ret != 2)
			goto err;
		DPCFUNC("(%#llx)=(%x)", (u64)(dpc_base + v1), v2);
		writel(v2, dpc_base + v1);
	} else if (strncmp(opt, "avs:", 4) == 0) {
		ret = sscanf(opt, "avs:%u,%u\n", &v1, &v2);
		if (ret != 2)
			goto err;
		writel(v2, MEM_VDISP_AVS_STEP(v1));
		mmdvfs_force_step_by_vcp(2, 4 - v1);
	} else if (strncmp(opt, "vdo", 3) == 0) {
		writel(DISP_DPC_EN|DISP_DPC_DT_EN|DISP_DPC_VDO_MODE, dpc_base + DISP_REG_DPC_EN);
	}

	goto end;
err:
	DPCERR();
end:
	dpc_pm_ctrl(false);
}
static ssize_t fs_write(struct file *file, const char __user *ubuf, size_t count, loff_t *ppos)
{
	const u32 debug_bufmax = 512 - 1;
	size_t ret;
	char cmd_buffer[512] = {0};
	char *tok, *buf;

	ret = count;

	if (count > debug_bufmax)
		count = debug_bufmax;

	if (copy_from_user(&cmd_buffer, ubuf, count))
		return -EFAULT;

	cmd_buffer[count] = 0;
	buf = cmd_buffer;
	DPCFUNC("%s", cmd_buffer);

	while ((tok = strsep(&buf, " ")) != NULL)
		process_dbg_opt(tok);

	return ret;
}

static const struct file_operations debug_fops = {
	.write = fs_write,
};
#endif

static const struct dpc_funcs funcs = {
	.dpc_enable = dpc_enable,
	.dpc_ddr_force_enable = dpc_ddr_force_enable,
	.dpc_infra_force_enable = dpc_infra_force_enable,
	.dpc_dc_force_enable = dpc_dc_force_enable,
	.dpc_group_enable = dpc_group_enable,
	.dpc_pause = dpc_pause,
	.dpc_config = dpc_config,
	.dpc_dt_set = dpc_dt_set_dur,
	.dpc_mtcmos_vote = dpc_mtcmos_vote,
	.dpc_vidle_power_keep = dpc_vidle_power_keep,
	.dpc_vidle_power_release = dpc_vidle_power_release,
	.dpc_vidle_power_keep_by_gce = dpc_vidle_power_keep_by_gce,
	.dpc_vidle_power_release_by_gce = dpc_vidle_power_release_by_gce,
	.dpc_hrt_bw_set = dpc_hrt_bw_set,
	.dpc_srt_bw_set = dpc_srt_bw_set,
	.dpc_dvfs_set = dpc_dvfs_set,
	.dpc_dvfs_bw_set = dpc_dvfs_bw_set,
	.dpc_dvfs_both_set = dpc_dvfs_both_set,
	.dpc_analysis = dpc_analysis,
	.dpc_dsi_pll_set = dpc_dsi_pll_set,
	.dpc_init_panel_type = dpc_init_panel_type,
};

static int mtk_dpc_state_monitor_thread(void *data)
{
	int ret = 0;
	bool off = false;

	while (!kthread_should_stop()) {
		ret = wait_event_interruptible(
			g_priv->dpc_state_wq,
			atomic_read(&g_priv->dpc_state) != DPC_STATE_NULL);

		off = atomic_read(&g_priv->dpc_state) == DPC_STATE_OFF ? true : false;
		atomic_set(&g_priv->dpc_state, DPC_STATE_NULL);
		if (debug_mmp == 0)
			continue;

		if (!debug_vdisp_level) {
			mtk_update_dpc_state(DPC_VIDLE_MMINFRA_MASK |
				DPC_VIDLE_APSRC_MASK | DPC_VIDLE_BW_MASK, off);
			continue;
		}

		if (mtk_mmdvfs_enable_vcp(true, VCP_PWR_USR_DISP) < 0) {
			mtk_update_dpc_state(DPC_VIDLE_MMINFRA_MASK |
				DPC_VIDLE_APSRC_MASK | DPC_VIDLE_BW_MASK, off);
			dpc_mmp(mmdvfs_dead, MMPROFILE_FLAG_PULSE, off, 0xffffffff);
			continue;
		}
		mtk_update_dpc_state(DPC_VIDLE_MMINFRA_MASK |
				DPC_VIDLE_APSRC_MASK | DPC_VIDLE_BW_MASK |
				DPC_VIDLE_VDISP_MASK, off);
		mtk_mmdvfs_enable_vcp(false, VCP_PWR_USR_DISP);
	}

	return 0;
}

static const struct of_device_id mtk_dpc_driver_dt_match[] = {
	{.compatible = "mediatek,mt6989-disp-dpc", .data = &mt6989_dpc_driver_data},
	{.compatible = "mediatek,mt6878-disp-dpc", .data = &mt6878_dpc_driver_data},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_dpc_driver_dt_match);

static int mtk_dpc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct task_struct *task;
	struct mtk_dpc *priv;
	const struct of_device_id *of_id;
	int ret = 0;

	of_id = of_match_device(mtk_dpc_driver_dt_match, dev);
	if (!of_id) {
		DPCERR("DPC device match failed\n");
		return -EPROBE_DEFER;
	}

	priv = (struct mtk_dpc *)of_id->data;
	if (priv == NULL) {
		DPCERR("invalid priv data\n");
		return -EPROBE_DEFER;
	}
	priv->pdev = pdev;
	priv->dev = dev;
	g_priv = priv;
	platform_set_drvdata(pdev, priv);

	if (of_find_property(dev->of_node, "power-domains", NULL)) {
		priv->pd_dev = dev;
		if (!pm_runtime_enabled(priv->pd_dev))
			pm_runtime_enable(priv->pd_dev);
		pm_runtime_irq_safe(priv->pd_dev);
	}

	if (of_property_read_u32(dev->of_node, "vidle-mask", &priv->vidle_mask)) {
		DPCERR("%s:failed to get vidle mask:0x%x",
			dev_name(dev), priv->vidle_mask);
		priv->vidle_mask = 0x0;
	}

	ret = dpc_res_init(priv);
	if (ret) {
		DPCERR("res init failed:%d", ret);
		return ret;
	}

	ret = dpc_irq_init(priv);
	if (ret) {
		DPCERR("irq init failed:%d", ret);
		return ret;
	}

	priv->pm_nb.notifier_call = dpc_pm_notifier;
	ret = register_pm_notifier(&priv->pm_nb);
	if (ret) {
		DPCERR("register_pm_notifier failed %d", ret);
		return ret;
	}

	/* enable external signal from DSI and TE */
	writel(0x1F, dpc_base + DISP_REG_DPC_DISP_EXT_INPUT_EN);
	writel(0x3, dpc_base + DISP_REG_DPC_MML_EXT_INPUT_EN);

	/* keep apsrc */
	writel(0x000D000D, dpc_base + DISP_REG_DPC_DISP_DDRSRC_EMIREQ_CFG);
	writel(0x000D000D, dpc_base + DISP_REG_DPC_MML_DDRSRC_EMIREQ_CFG);

	/* keep infra */
	writel(0x181818, dpc_base + DISP_REG_DPC_DISP_INFRA_PLL_OFF_CFG);
	writel(0x181818, dpc_base + DISP_REG_DPC_MML_INFRA_PLL_OFF_CFG);

	/* keep vdisp opp */
	writel(0x1, dpc_base + DISP_REG_DPC_DISP_VDISP_DVFS_CFG);
	writel(0x1, dpc_base + DISP_REG_DPC_MML_VDISP_DVFS_CFG);

	/* keep HRT and SRT BW */
	writel(0x00010001, dpc_base + DISP_REG_DPC_DISP_HRTBW_SRTBW_CFG);
	writel(0x00010001, dpc_base + DISP_REG_DPC_MML_HRTBW_SRTBW_CFG);

#if IS_ENABLED(CONFIG_DEBUG_FS)
	priv->fs = debugfs_create_file("dpc_ctrl", S_IFREG | 0440, NULL, NULL, &debug_fops);
	if (IS_ERR(priv->fs))
		DPCERR("debugfs_create_file failed:%ld", PTR_ERR(priv->fs));
#endif

	dpc_mmp_init();

	spin_lock_init(&g_priv->skip_force_power_lock);
	spin_lock_init(&vlp_lock);
	spin_lock_init(&dpc_lock);
	spin_lock_init(&dpc_state_lock);

	mtk_vidle_register(&funcs);
	mml_dpc_register(&funcs);
	mdp_dpc_register(&funcs);

	if (priv->mmdvfs_settings_count > 0)
		mmdvfs_rc_enable_set_fp(&mtk_dpc_mmdvfs_notifier);

	init_waitqueue_head(&priv->dpc_state_wq);
	atomic_set(&priv->dpc_state, DPC_STATE_NULL);
	task = kthread_create(mtk_dpc_state_monitor_thread, NULL, "dpc-monitor");
	if (IS_ERR(task)) {
		ret = (long)PTR_ERR(task);
		DPCERR("%s failed to create dpc-monitor, ret:%d", __func__, ret);
	} else
		wake_up_process(task);

	init_waitqueue_head(&priv->dpc_mtcmos_wq);
	atomic_set(&priv->dpc_mtcmos_timeout, 0);
	task = kthread_create(mtk_dpc_vidle_timeout_monitor_thread, NULL, "vidle-timeout-thread");
	if (IS_ERR(task)) {
		ret = (long)PTR_ERR(task);
		DPCERR("%s failed to create vidle-timeout-thread, ret:%d", __func__, ret);
	} else
		wake_up_process(task);

	atomic_set(&g_disp_mode, DPC_VIDLE_INACTIVE_MODE);
	atomic_set(&g_mml_mode, DPC_VIDLE_INACTIVE_MODE);
	atomic_set(&g_vidle_window, 0);
	if (debug_runtime_ctrl) {
		debug_mmp = 0;
		debug_irq = 0;
		debug_mtcmos_off = 0;
	}
	DPCFUNC("- mmsys:0x%x, panel:%d", priv->mmsys_id, g_panel_type);
	return ret;
}

static int mtk_dpc_remove(struct platform_device *pdev)
{
	DPCFUNC();
	return 0;
}

static void mtk_dpc_shutdown(struct platform_device *pdev)
{
	struct mtk_dpc *priv = platform_get_drvdata(pdev);

	if (priv)
		priv->skip_force_power = true;
	dpc_mmp(skip_vote, MMPROFILE_FLAG_PULSE, U32_MAX, 1);
}

struct platform_driver mtk_dpc_driver = {
	.probe = mtk_dpc_probe,
	.remove = mtk_dpc_remove,
	.shutdown = mtk_dpc_shutdown,
	.driver = {
		.name = "mediatek-disp-dpc",
		.owner = THIS_MODULE,
		.of_match_table = mtk_dpc_driver_dt_match,
	},
};

static int __init mtk_dpc_init(void)
{
	platform_driver_register(&mtk_dpc_driver);
	return 0;
}

static void __exit mtk_dpc_exit(void)
{
	DPCFUNC();
}

module_init(mtk_dpc_init);
module_exit(mtk_dpc_exit);

MODULE_AUTHOR("William Yang <William-tw.Yang@mediatek.com>");
MODULE_DESCRIPTION("MTK Display Power Controller");
MODULE_LICENSE("GPL");
