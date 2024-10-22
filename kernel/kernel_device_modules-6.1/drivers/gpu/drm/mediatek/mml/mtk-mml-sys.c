// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Ping-Hsun Wu <ping-hsun.wu@mediatek.com>
 */

#include <linux/component.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <mtk_drm_ddp_comp.h>
#include <cmdq-util.h>

#include "mtk-mml-dpc.h"
#include "mtk-mml-core.h"
#include "mtk-mml-driver.h"
#include "mtk-mml-dle-adaptor.h"
#include "mtk-mml-mmp.h"
#include "mtk-mml-sys.h"

#include "tile_driver.h"
#include "mtk-mml-tile.h"
#include "tile_mdp_func.h"
#include "mtk-mml-pq-core.h"

#define SYS_MISC_REG		0x0f0
#define SYS_SW0_RST_B_REG	0x700
#define SYS_SW1_RST_B_REG	0x704
#define SYS_AID_SEL		0xfa8	/* only for mt6983/mt6895 */

#define MML_MAX_SYS_COMPONENTS	16
#define MML_MAX_SYS_MUX_PINS	88
#define MML_MAX_SYS_DL_RELAYS	7
#define MML_MAX_SYS_DBG_REGS	100

#define APU_CTRL		0x2d0
#define APU_HANDLE		0x200
#define APU_HANDLE_H		0x204
#define APU_DCEN		0x208

/* SMI offset */
#define SMI_LARB_DISABLE_ULTRA	0x70

int mml_ir_loop = 1;
module_param(mml_ir_loop, int, 0644);

int mml_racing_sleep = 16000;
module_param(mml_racing_sleep, int, 0644);

#if IS_ENABLED(CONFIG_MTK_MML_DEBUG)
int mml_ddp_dump = 1;
module_param(mml_ddp_dump, int, 0644);

int mml_dle_delay;
module_param(mml_dle_delay, int, 0644);
#endif

#define sys_msg(fmt, args...) \
do { \
	if (mtk_mml_msg || mml_rrot_msg) \
		pr_notice("[mml]" fmt "\n", ##args); \
} while (0)

enum mml_comp_type {
	MML_CT_COMPONENT = 0,
	MML_CT_SYS,
	MML_CT_PATH,
	MML_CT_DL_IN,
	MML_CT_DL_OUT,
	MML_COMP_TYPE_TOTAL
};

enum mml_aidsel_mode {
	MML_AIDSEL_REG = 0,
	MML_AIDSEL_ENGINE,
	MML_AIDSEL_ENGINEBITS,
};

struct mml_sys;

struct mml_data {
	int (*comp_inits[MML_COMP_TYPE_TOTAL])(struct device *dev,
		struct mml_sys *sys, struct mml_comp *comp);
	const struct mtk_ddp_comp_funcs *ddp_comp_funcs[MML_COMP_TYPE_TOTAL];
	void (*aid_sel)(struct mml_comp *comp, struct mml_task *task,
		struct mml_comp_config *ccfg);
	u8 gpr[MML_PIPE_CNT];
	enum mml_aidsel_mode aidsel_mode;
	u8 px_per_tick;
	bool pw_mminfra;
	bool set_mml_uid;
};

enum mml_mux_type {
	MML_MUX_UNUSED = 0,
	MML_MUX_MOUT,
	MML_MUX_SOUT,
	MML_MUX_SELIN,
};

struct mml_mux_pin {
	u16 index;
	u16 from;
	u16 to;
	u16 type;
	u16 offset;
} __attribute__ ((__packed__));

struct mml_dbg_reg {
	const char *name;
	u32 offset;
};

struct mml_sys {
	/* Device data and component bindings */
	const struct mml_data *data;
	struct device *dev;
	struct mtk_ddp_comp ddp_comps[MML_MAX_SYS_COMPONENTS];
	/* DDP component flags */
	u32 ddp_comp_en;
	/* DDP component bound index */
	u32 ddp_bound;
	struct mml_comp comps[MML_MAX_SYS_COMPONENTS];
	/* MML component count */
	u32 comp_cnt;
	/* MML component bound count */
	u32 comp_bound;

	/* MML multiplexer pins.
	 * The entry 0 leaves empty for efficiency, do not use.
	 */
	struct mml_mux_pin mux_pins[MML_MAX_SYS_MUX_PINS + 1];
	u32 mux_cnt;
	u16 dl_relays[MML_MAX_SYS_DL_RELAYS + 1];
	u32 dl_cnt;

	/* Table of component or adjacency data index.
	 *
	 * The element is an index to data arrays.
	 * The component data by type is indexed by adjacency[id][id];
	 * in the upper-right tri. are MOUTs and SOUTs by adjacency[from][to];
	 * in the bottom-left tri. are SELIN pins by adjacency[to][from].
	 * Direct-wires are not in this table.
	 *
	 * Ex.:
	 *	dl_relays[adjacency[DLI0][DLI0]] is RELAY of comp DLI0.
	 *	mux_pins[adjacency[RDMA0][RSZ0]] is MOUT from RDMA0 to RSZ0.
	 *	mux_pins[adjacency[RSZ0][RDMA0]] is SELIN from RDMA0 to RSZ0.
	 *
	 * Array data would be like:
	 *	[0] = { T  indices of },	T: indices of component data
	 *	[1] = { . T . MOUTs & },	   (by component type, the
	 *	[2] = { . . T . SOUTs },	    indices refer to different
	 *	[3] = { . . . T . . . },	    data arrays.)
	 *	[4] = { . . . . T . . },
	 *	[5] = { indices . T . },
	 *	[6] = { of SELINs . T },
	 */
	u8 adjacency[MML_MAX_COMPONENTS][MML_MAX_COMPONENTS];
	struct mml_dbg_reg dbg_regs[MML_MAX_SYS_DBG_REGS];
	u32 dbg_reg_cnt;

	/* store the bit to enable aid_sel for specific component */
	u16 aid_sel_regs[MML_MAX_AID_COMPS];
	u16 aid_sel_reg_bit[MML_MAX_AID_COMPS];
	u16 aid_sel_reg_mask[MML_MAX_AID_COMPS];
	u8 aid_sel[MML_MAX_COMPONENTS];

	/* register for racing mode select ready signal */
	u16 inline_ready_sel;

	/* component master device, i.e., mml driver device */
	struct device *master;
	/* adaptor for display addon config */
	struct mml_dle_ctx *dle_ctx;
	struct mml_dle_param dle_param;
	/* addon status */
	const struct mml_topology_path *ddp_path[MML_PIPE_CNT];
	const struct mml_topology_path *pre_ddp_path[MML_PIPE_CNT];

	/* racing mode pipe sync event */
	u16 event_racing_pipe0;
	u16 event_racing_pipe1;
	u16 event_racing_pipe1_next;
	u16 event_apu_start;

	u32 dpc_base;

#ifndef MML_FPGA
	/* for config sspm aid */
	void *mml_scmi;
#endif

	u32 apu_base;
	void __iomem *apu_base_va;
};

struct sys_frame_data {
	u32 racing_frame_offset;

	/* offset to loop begin instruction
	 * IR: begin of tile 0
	 * DL: after tile
	 */
	u32 frame_loop_offset;
	/* instruction offset to assign jump target PA to frame_loop_offset */
	u32 frame_loop_jump;

	/* fo dual pipe case, pipe 0 skip set pipe1_next event in none loop case */
	u32 frame_pipe_conti_offset;
	u32 frame_pipe_conti_jump;

	u32 tile_idx;
};

static inline struct sys_frame_data *sys_frm_data(struct mml_comp_config *ccfg)
{
	return ccfg->data;
}

static inline struct mml_sys *comp_to_sys(struct mml_comp *comp)
{
	return container_of(comp, struct mml_sys, comps[comp->sub_idx]);
}

static inline struct mml_sys *ddp_comp_to_sys(struct mtk_ddp_comp *ddp_comp)
{
	return container_of(ddp_comp, struct mml_sys, ddp_comps[ddp_comp->sub_idx]);
}

u16 mml_sys_get_reg_ready_sel(struct mml_comp *comp)
{
	return comp_to_sys(comp)->inline_ready_sel;
}

static s32 sys_config_prepare(struct mml_comp *comp, struct mml_task *task,
			      struct mml_comp_config *ccfg)
{
	struct sys_frame_data *sys_frm;

	/* initialize component frame data for current frame config */
	sys_frm = kzalloc(sizeof(*sys_frm), GFP_KERNEL);
	ccfg->data = sys_frm;
	if (!sys_frm)
		return -ENOMEM;
	return 0;
}

s32 sys_init(struct mml_comp *comp, struct mml_task *task,
	struct mml_comp_config *ccfg)
{
	struct mml_sys *sys = comp_to_sys(comp);
	struct mml_frame_config *cfg = task->config;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];

	if (cfg->info.mode == MML_MODE_DIRECT_LINK && cfg->dpc && sys->dpc_base) {
		if (mml_dl_dpc & MML_DLDPC_VOTE) {
			cmdq_pkt_write(pkt, NULL, sys->dpc_base + VLP_VOTE_SET,
				BIT(DISP_VIDLE_USER_MML_CMDQ), U32_MAX);
			cmdq_pkt_write(pkt, NULL, sys->dpc_base + VLP_VOTE_SET,
				BIT(DISP_VIDLE_USER_MML_CMDQ), U32_MAX);
		}
	}

	if (cfg->info.mode == MML_MODE_MML_DECOUPLE) {
		/* disable ultra in srt mode to avoid occupy bw */
		cmdq_pkt_write(task->pkts[ccfg->pipe], NULL,
			comp->larb_base + SMI_LARB_DISABLE_ULTRA, 0xffffffff, U32_MAX);
	} else {
		/* enable ultra */
		cmdq_pkt_write(task->pkts[ccfg->pipe], NULL,
			comp->larb_base + SMI_LARB_DISABLE_ULTRA, 0x0, U32_MAX);
	}

	return 0;
}

static void sys_sync_racing(struct mml_comp *comp, struct mml_task *task,
	struct mml_comp_config *ccfg)
{
	struct mml_dev *mml = task->config->mml;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	struct mml_frame_config *cfg = task->config;

	/* synchronize disp and mml task by for MML pkt:
	 *	set MML_READY
	 *	wait_and_clear DISP_READY
	 */
	if (cfg->disp_vdo)
		cmdq_pkt_set_event(pkt, mml_ir_get_mml_ready_event(mml));
	cmdq_pkt_wfe(pkt, mml_ir_get_disp_ready_event(mml));
	if (cfg->disp_vdo)
		cmdq_pkt_clear_event(pkt, mml_ir_get_target_event(mml));
}

static void sys_config_frame_racing(struct mml_comp *comp, struct mml_task *task,
	struct mml_comp_config *ccfg)
{
	struct sys_frame_data *sys_frm = sys_frm_data(ccfg);
	struct mml_frame_config *cfg = task->config;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];

	cmdq_pkt_assign_command(pkt, MML_CMDQ_ROUND_SPR,
		(task->job.jobid << 16) | MML_ROUND_SPR_INIT);
	sys_frm->racing_frame_offset = pkt->cmd_buf_size - CMDQ_INST_SIZE;

	/* only pipe 0 sync with disp, thus pipe 1 sync with pipe 0 */
	if (ccfg->pipe == 0) {
		struct cmdq_operand lhs, rhs;

		cmdq_pkt_clear_event(pkt, mml_ir_get_mml_stop_event(cfg->mml));

		lhs.reg = true;
		lhs.idx = MML_CMDQ_NEXT_SPR;
		rhs.reg = false;
		rhs.value = ~(u16)MML_NEXTSPR_DUAL;
		cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_AND, MML_CMDQ_NEXT_SPR,
			&lhs, &rhs);

		if (likely(!mml_racing_ut))
			sys_sync_racing(comp, task, ccfg);
	} else {
		/* clear nextspr2 and sync before everything start */
		cmdq_pkt_assign_command(pkt, MML_CMDQ_NEXT_SPR2, 0);
	}
}

static void sys_config_frame_dl(struct mml_comp *comp, struct mml_task *task,
	struct mml_comp_config *ccfg)
{
	struct sys_frame_data *sys_frm = sys_frm_data(ccfg);
	struct mml_frame_config *cfg = task->config;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];

	cmdq_pkt_assign_command(pkt, MML_CMDQ_ROUND_SPR,
		(task->job.jobid << 16) | MML_ROUND_SPR_INIT);
	sys_frm->racing_frame_offset = pkt->cmd_buf_size - CMDQ_INST_SIZE;

	/* only pipe 0 sync with disp, thus pipe 1 sync with pipe 0 */
	if (ccfg->pipe == 0) {
		struct cmdq_operand lhs, rhs;

		cmdq_pkt_clear_event(pkt, mml_ir_get_mml_stop_event(cfg->mml));

		lhs.reg = true;
		lhs.idx = MML_CMDQ_NEXT_SPR;
		rhs.reg = false;
		rhs.value = ~(u16)MML_NEXTSPR_DUAL;
		cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_AND, MML_CMDQ_NEXT_SPR,
			&lhs, &rhs);
	} else {
		/* clear nextspr2 and sync before everything start */
		cmdq_pkt_assign_command(pkt, MML_CMDQ_NEXT_SPR2, 0);
	}
}

static void sys_apu_enable(struct cmdq_pkt *pkt, u32 apu_base, u64 handle)
{
	cmdq_pkt_write(pkt, NULL, apu_base + APU_CTRL, 0xfb, U32_MAX);
	cmdq_pkt_write(pkt, NULL, apu_base + APU_HANDLE, (u32)(handle >> 32), U32_MAX);
	cmdq_pkt_write(pkt, NULL, apu_base + APU_HANDLE_H, (u32)handle, U32_MAX);
	cmdq_pkt_write(pkt, NULL, apu_base + APU_DCEN, 0, U32_MAX);
}

static void sys_apu_disable(struct cmdq_pkt *pkt, u32 apu_base, u64 handle)
{
	cmdq_pkt_write(pkt, NULL, apu_base + APU_CTRL, 0xfb, U32_MAX);
	cmdq_pkt_write(pkt, NULL, apu_base + APU_HANDLE, (u32)(handle >> 32), U32_MAX);
	cmdq_pkt_write(pkt, NULL, apu_base + APU_HANDLE_H, (u32)handle, U32_MAX);
	cmdq_pkt_write(pkt, NULL, apu_base + APU_DCEN, 1, U32_MAX);
}

static s32 sys_config_frame(struct mml_comp *comp, struct mml_task *task,
			    struct mml_comp_config *ccfg)
{
	struct mml_sys *sys = comp_to_sys(comp);
	struct mml_frame_config *cfg = task->config;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	u32 misc_reg = 0, misc_reg_mask = 0;

	if (cfg->info.mode == MML_MODE_RACING) {
		sys_config_frame_racing(comp, task, ccfg);
		misc_reg = 0x22 << 12;
		misc_reg_mask = GENMASK(19, 12);
	} else if (cfg->info.mode == MML_MODE_DDP_ADDON) {
		/* use hw reset flow */
		misc_reg_mask = 0x80000000;
	} else if (cfg->info.mode == MML_MODE_DIRECT_LINK) {
		/* use hw reset flow */
		misc_reg_mask = 0x80000000;
		sys_config_frame_dl(comp, task, ccfg);
	} else if (cfg->info.mode == MML_MODE_APUDC) {
		if (ccfg->pipe == 0)
			sys_apu_enable(pkt, sys->apu_base, task->buf.src.apu_handle);
		misc_reg = 0x22 << 12;
		misc_reg_mask = GENMASK(19, 12);
	} else {
		/* enable ddr flag during hw running */
		misc_reg = 0x22 << 12;
		misc_reg_mask = GENMASK(19, 12);
	}

	cmdq_pkt_write(pkt, NULL, comp->base_pa + SYS_MISC_REG, misc_reg, misc_reg_mask);

	/* config aid sel by platform */
	sys->data->aid_sel(comp, task, ccfg);

	return 0;
}

static void sys_config_aid_sel(struct mml_comp *comp, struct mml_task *task,
			       struct mml_comp_config *ccfg)
{
	struct mml_sys *sys = comp_to_sys(comp);
	struct mml_frame_config *cfg = task->config;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	const struct mml_topology_path *path = cfg->path[ccfg->pipe];
	u32 aid_sel = 0, mask = 0;
	u32 in_engine_id = path->nodes[path->tile_engines[0]].comp->id;
	u32 i;

	if (cfg->info.src.secure)
		aid_sel |= 1 << sys->aid_sel[in_engine_id];
	mask |= 1 << sys->aid_sel[in_engine_id];

	if (cfg->info.dest[0].pq_config.en_region_pq &&
	    cfg->info.seg_map.secure)
		aid_sel |= 1 << sys->aid_sel[path->pq_rdma_id];
	mask |= 1 << sys->aid_sel[path->pq_rdma_id];

	for (i = 0; i < cfg->info.dest_cnt; i++) {
		if (!path->out_engine_ids[i])
			continue;
		if (cfg->info.dest[i].data.secure)
			aid_sel |= 1 << sys->aid_sel[path->out_engine_ids[i]];
		mask |= 1 << sys->aid_sel[path->out_engine_ids[i]];
	}
	cmdq_pkt_write(pkt, NULL, comp->base_pa + SYS_AID_SEL, aid_sel, mask);
}

static void sys_config_aid_sel_engine(struct mml_comp *comp, struct mml_task *task,
				      struct mml_comp_config *ccfg)
{
	struct mml_sys *sys = comp_to_sys(comp);
	struct mml_frame_config *cfg = task->config;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	const struct mml_topology_path *path = cfg->path[ccfg->pipe];
	u32 in_engine_id = path->nodes[path->tile_engines[0]].comp->id;
	u32 i, ca_idx;

	ca_idx = sys->aid_sel[in_engine_id];
	cmdq_pkt_write(pkt, NULL, comp->base_pa + sys->aid_sel_regs[ca_idx],
		cfg->info.src.secure, U32_MAX);

	if (cfg->info.dest[0].pq_config.en_region_pq) {
		ca_idx = sys->aid_sel[path->pq_rdma_id];
		cmdq_pkt_write(pkt, NULL, comp->base_pa + sys->aid_sel_regs[ca_idx],
			cfg->info.seg_map.secure, U32_MAX);
	}

	for (i = 0; i < cfg->info.dest_cnt; i++) {
		/* comp to aid sel idx */
		ca_idx = sys->aid_sel[path->out_engine_ids[i]];
		/* Note here write all 32bits since bit 31:1 are ignore,
		 * and mask costs more time and instruction.
		 */
		cmdq_pkt_write(pkt, NULL, comp->base_pa + sys->aid_sel_regs[ca_idx],
			cfg->info.dest[i].data.secure, U32_MAX);
	}
}

static void sys_config_aid_sel_bits(struct mml_comp *comp, struct mml_task *task,
				    struct mml_comp_config *ccfg)
{
	struct mml_sys *sys = comp_to_sys(comp);
	struct mml_frame_config *cfg = task->config;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	const struct mml_topology_path *path = cfg->path[ccfg->pipe];
	u32 i;

	for (i = 0; i < path->aid_eng_cnt; i++) {
		u8 aid_idx = sys->aid_sel[path->aid_engine_ids[i]];
		u16 aid_bit = cfg->info.src.secure ? sys->aid_sel_reg_bit[aid_idx] : 0;

		cmdq_pkt_write(pkt, NULL, comp->base_pa + sys->aid_sel_regs[aid_idx],
			aid_bit, sys->aid_sel_reg_mask[aid_idx]);
	}
}

static void config_mux(struct mml_sys *sys, struct cmdq_pkt *pkt,
		       const phys_addr_t base_pa, u8 mux_idx, u8 sof_grp,
		       u16 *offset, u32 *mout)
{
	struct mml_mux_pin *mux;

	if (!mux_idx)
		return;
	mux = &sys->mux_pins[mux_idx];

	switch (mux->type) {
	case MML_MUX_MOUT:
		*offset = mux->offset;
		*mout |= 1 << mux->index;
		break;
	case MML_MUX_SOUT:
	case MML_MUX_SELIN:
		cmdq_pkt_write(pkt, NULL, base_pa + mux->offset,
			       mux->index + (sof_grp << 8), U32_MAX);
		break;
	default:
		break;
	}
}

static void sys_insert_begin_loop(struct mml_task *task,
	struct mml_comp_config *ccfg,
	struct mml_sys *sys,
	struct sys_frame_data *sys_frm,
	struct cmdq_pkt *pkt)
{
	struct mml_frame_config *cfg = task->config;
	struct cmdq_operand lhs, rhs;

	/* sync pipes at begin for racing mode */
	if (cfg->info.mode == MML_MODE_RACING) {
		if (ccfg->pipe == 1) {
			cmdq_pkt_set_event(pkt, sys->event_racing_pipe1);
			cmdq_pkt_wfe(pkt, sys->event_racing_pipe0);
		} else if (cfg->dual) {	/* pipe0 + dual */
			cmdq_pkt_set_event(pkt, sys->event_racing_pipe0);
			cmdq_pkt_wfe(pkt, sys->event_racing_pipe1);
		}
	}

	sys_frm->frame_loop_offset = pkt->cmd_buf_size;

	lhs.reg = true;
	lhs.idx = MML_CMDQ_ROUND_SPR;
	rhs.reg = false;
	rhs.value = 1;
	cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_ADD, MML_CMDQ_ROUND_SPR, &lhs, &rhs);

	if (ccfg->pipe == 0 && cfg->dual && cfg->disp_vdo && likely(mml_ir_loop)) {
		cmdq_pkt_assign_command(pkt, CMDQ_THR_SPR_IDX0, 0);
		sys_frm->frame_pipe_conti_jump = pkt->cmd_buf_size - CMDQ_INST_SIZE;

		/* get the MML_NEXTSPR_DUAL bit first
		 * pseudo:
		 *	SPR1 = NEXT_SPR & MML_NEXTSPR_DUAL
		 */
		lhs.reg = true;
		lhs.idx = MML_CMDQ_NEXT_SPR;
		rhs.reg = false;
		rhs.value = MML_NEXTSPR_DUAL;
		cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_AND, CMDQ_THR_SPR_IDX1, &lhs, &rhs);

		/* jump if SPR1 is 0
		 * pseudo:
		 *	if SPR1 == 0:
		 *		jump CONFIG_TILE0
		 */
		lhs.reg = true;
		lhs.idx = CMDQ_THR_SPR_IDX1;
		rhs.reg = false;
		rhs.value = 0;
		cmdq_pkt_cond_jump_abs(pkt, CMDQ_THR_SPR_IDX0, &lhs, &rhs, CMDQ_EQUAL);

		/* loop case, tell pipe1 to check loop condition */
		cmdq_pkt_set_event(pkt, sys->event_racing_pipe1_next);

		/* not loop case, skip set event_racing_pipe1_next event by jump here */
		sys_frm->frame_pipe_conti_offset = pkt->cmd_buf_size;

		lhs.reg = true;
		lhs.idx = MML_CMDQ_NEXT_SPR;
		rhs.reg = false;
		rhs.value = MML_NEXTSPR_DUAL;
		cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_OR, MML_CMDQ_NEXT_SPR, &lhs, &rhs);
	}
}

static s32 sys_config_tile(struct mml_comp *comp, struct mml_task *task,
			   struct mml_comp_config *ccfg, u32 idx)
{
	struct mml_sys *sys = comp_to_sys(comp);
	const struct mml_topology_path *path = task->config->path[ccfg->pipe];
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	const phys_addr_t base_pa = comp->base_pa;
	u8 sof_grp = path->mux_group;
	struct sys_frame_data *sys_frm = sys_frm_data(ccfg);
	u32 i, j;

	if (task->config->info.mode == MML_MODE_RACING) {
		if (!sys_frm->frame_loop_offset)
			sys_insert_begin_loop(task, ccfg, sys, sys_frm, pkt);
	} else if (task->config->info.mode == MML_MODE_APUDC && !sys_frm->tile_idx) {
		if (ccfg->pipe == 0) {
			cmdq_pkt_wfe(pkt, sys->event_apu_start);
			cmdq_pkt_set_event(pkt, sys->event_racing_pipe0);
		} else {
			cmdq_pkt_wfe(pkt, sys->event_racing_pipe0);
		}
	}
	sys_frm->tile_idx++;

	for (i = 0; i < path->node_cnt; i++) {
		const struct mml_path_node *node = &path->nodes[i];
		u16 offset = 0;
		u32 mout = 0;
		u8 from = node->id, to, mux_idx;

		/* TODO: continue if node disabled */
		for (j = 0; j < ARRAY_SIZE(node->next); j++) {
			if (node->next[j]) {	/* && next enabled */
				to = node->next[j]->id;
				mux_idx = sys->adjacency[from][to];
				config_mux(sys, pkt, base_pa, mux_idx, sof_grp,
					   &offset, &mout);
				mux_idx = sys->adjacency[to][from];
				config_mux(sys, pkt, base_pa, mux_idx, sof_grp,
					   &offset, &mout);
			}
		}
		if (mout)
			cmdq_pkt_write(pkt, NULL, base_pa + offset,
				       mout + (sof_grp << 8), U32_MAX);
	}
	return 0;
}

static s32 sys_wait(struct mml_comp *comp, struct mml_task *task,
	struct mml_comp_config *ccfg, u32 idx)
{
	struct mml_sys *sys = comp_to_sys(comp);
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	struct sys_frame_data *sys_frm = sys_frm_data(ccfg);

	/* before all module wait, insert begin loop for next round */
	if (task->config->info.mode == MML_MODE_DIRECT_LINK)
		sys_insert_begin_loop(task, ccfg, sys, sys_frm, pkt);

	return 0;
}

static void sys_addr_update(struct mml_comp *comp, struct mml_task *task,
	struct mml_comp_config *ccfg)
{
	struct mml_frame_config *cfg = task->config;
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	struct sys_frame_data *sys_frm = sys_frm_data(ccfg);
	u32 *inst;

	if (cfg->disp_vdo && likely(mml_ir_loop) && likely(mml_racing_ut != 1)) {
		inst = (u32 *)cmdq_pkt_get_va_by_offset(pkt, sys_frm->frame_loop_jump);
		*inst = (u32)CMDQ_REG_SHIFT_ADDR(cmdq_pkt_get_pa_by_offset(pkt,
			sys_frm->frame_loop_offset));
	}

	if (ccfg->pipe == 0 && cfg->dual && cfg->disp_vdo && likely(mml_ir_loop)) {
		inst = (u32 *)cmdq_pkt_get_va_by_offset(pkt,
			sys_frm->frame_pipe_conti_jump);
		*inst = (u32)CMDQ_REG_SHIFT_ADDR(cmdq_pkt_get_pa_by_offset(
			pkt, sys_frm->frame_pipe_conti_offset));
	}

	inst = (u32 *)cmdq_pkt_get_va_by_offset(pkt, sys_frm->racing_frame_offset);
	*inst = (task->job.jobid << 16) | MML_ROUND_SPR_INIT;
}

static void sys_loop_racing(struct mml_comp *comp, struct mml_task *task,
	struct mml_comp_config *ccfg)
{
	struct mml_sys *sys = comp_to_sys(comp);
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	struct sys_frame_data *sys_frm = sys_frm_data(ccfg);
	struct cmdq_operand lhs, rhs;
	struct mml_frame_config *cfg = task->config;
	u16 event_ir_eof = mml_ir_get_target_event(cfg->mml);

	if (unlikely(mml_racing_ut == 1))
		return;

	if (unlikely(mml_racing_ut))
		cmdq_pkt_sleep(pkt, CMDQ_US_TO_TICK(mml_racing_sleep),
			sys->data->gpr[ccfg->pipe]);

	/* do eoc to avoid task timeout during self-loop */
	cmdq_pkt_eoc(pkt, true);

	/* wait display frame done before checking next, so disp driver has
	 * chance to tell mml to entering next task.
	 */
	if (cfg->disp_vdo && event_ir_eof)
		cmdq_pkt_wfe(task->pkts[0], event_ir_eof);

	/* reserve assign inst for jump addr */
	cmdq_pkt_assign_command(pkt, CMDQ_THR_SPR_IDX0, 0);
	sys_frm->frame_loop_jump = pkt->cmd_buf_size - CMDQ_INST_SIZE;

	/* get the MML_NEXTSPR_NEXT bit first
	 * pseudo:
	 *	SPR1 = NEXT_SPR & MML_NEXTSPR_NEXT
	 */
	lhs.reg = true;
	lhs.idx = MML_CMDQ_NEXT_SPR;
	rhs.reg = false;
	rhs.value = MML_NEXTSPR_NEXT;
	cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_AND, CMDQ_THR_SPR_IDX1, &lhs, &rhs);

	/* loop if SPR1 is not MML_NEXTSPR_NEXT
	 * pseudo:
	 *	if SPR1 != MML_NEXTSPR_NEXT:
	 *		jump CONFIG_FRAME_LOOP
	 */
	lhs.reg = true;
	lhs.idx = CMDQ_THR_SPR_IDX1;
	rhs.reg = false;
	rhs.value = MML_NEXTSPR_NEXT;
	cmdq_pkt_cond_jump_abs(pkt, CMDQ_THR_SPR_IDX0, &lhs, &rhs, CMDQ_NOT_EQUAL);

	if (task->config->dual) {
		/* tell pipe1 to next and let pipe 1 continue */
		cmdq_pkt_assign_command(pkt, MML_CMDQ_NEXT_SPR2, MML_NEXTSPR_NEXT);
		cmdq_pkt_set_event(pkt, sys->event_racing_pipe1_next);
	}
}

static void sys_loop_pipe1(struct mml_comp *comp, struct mml_task *task,
	struct mml_comp_config *ccfg)
{
	struct mml_sys *sys = comp_to_sys(comp);
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	struct sys_frame_data *sys_frm = sys_frm_data(ccfg);
	struct cmdq_operand lhs, rhs;

	/* do eoc to avoid task timeout during self-loop */
	cmdq_pkt_eoc(pkt, true);

	/* reserve assign inst for jump addr */
	cmdq_pkt_assign_command(pkt, CMDQ_THR_SPR_IDX0, 0);
	sys_frm->frame_loop_jump = pkt->cmd_buf_size - CMDQ_INST_SIZE;

	/* wait pipe0 check and setup next flag */
	cmdq_pkt_wfe(pkt, sys->event_racing_pipe1_next);

	/* loop if NEXTSPR2 is not MML_NEXTSPR_NEXT
	 * pseudo:
	 *	if NEXTSPR2 != MML_NEXTSPR_NEXT:
	 *		jump CONFIG_FRAME_LOOP
	 */
	lhs.reg = true;
	lhs.idx = MML_CMDQ_NEXT_SPR2;
	rhs.reg = false;
	rhs.value = MML_NEXTSPR_NEXT;
	cmdq_pkt_cond_jump_abs(pkt, CMDQ_THR_SPR_IDX0, &lhs, &rhs, CMDQ_NOT_EQUAL);
}

static void sys_loop_dl(struct mml_comp *comp, struct mml_task *task,
	struct mml_comp_config *ccfg)
{
	struct mml_sys *sys = comp_to_sys(comp);
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	struct sys_frame_data *sys_frm = sys_frm_data(ccfg);
	struct cmdq_operand lhs, rhs;

	/* do eoc to avoid task timeout during self-loop */
	cmdq_pkt_eoc(pkt, true);

	/* reserve assign inst for jump addr */
	cmdq_pkt_assign_command(pkt, CMDQ_THR_SPR_IDX0, 0);
	sys_frm->frame_loop_jump = pkt->cmd_buf_size - CMDQ_INST_SIZE;

	/* get the MML_NEXTSPR_NEXT bit first
	 * pseudo:
	 *	SPR1 = NEXT_SPR & MML_NEXTSPR_NEXT
	 */
	lhs.reg = true;
	lhs.idx = MML_CMDQ_NEXT_SPR;
	rhs.reg = false;
	rhs.value = MML_NEXTSPR_NEXT;
	cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_AND, CMDQ_THR_SPR_IDX1, &lhs, &rhs);

	/* loop if SPR1 is not MML_NEXTSPR_NEXT
	 * pseudo:
	 *	if SPR1 != MML_NEXTSPR_NEXT:
	 *		jump CONFIG_FRAME_LOOP
	 */
	lhs.reg = true;
	lhs.idx = CMDQ_THR_SPR_IDX1;
	rhs.reg = false;
	rhs.value = MML_NEXTSPR_NEXT;
	cmdq_pkt_cond_jump_abs(pkt, CMDQ_THR_SPR_IDX0, &lhs, &rhs, CMDQ_NOT_EQUAL);

	if (task->config->dual) {
		/* tell pipe1 to next and let pipe 1 continue */
		cmdq_pkt_assign_command(pkt, MML_CMDQ_NEXT_SPR2, MML_NEXTSPR_NEXT);
		cmdq_pkt_set_event(pkt, sys->event_racing_pipe1_next);
	}
}

static s32 sys_post(struct mml_comp *comp, struct mml_task *task,
		    struct mml_comp_config *ccfg)
{
	enum mml_mode mode = task->config->info.mode;

	if (mode == MML_MODE_RACING) {
		/* only vdo mode need do self loop */
		if (task->config->disp_vdo && likely(mml_ir_loop)) {
			if (ccfg->pipe == 0)
				sys_loop_racing(comp, task, ccfg);
			else
				sys_loop_pipe1(comp, task, ccfg);
		}

		/* for pipe0 unlock event so disp could stop racing */
		if (ccfg->pipe == 0)
			cmdq_pkt_set_event(task->pkts[0],
				mml_ir_get_mml_stop_event(task->config->mml));

		/* Update cond jump pa for self loop,
		 * and job id for debug in both mode.
		 */
		sys_addr_update(comp, task, ccfg);
	} else if (mode == MML_MODE_DIRECT_LINK) {
		if (task->config->disp_vdo) {
			if (ccfg->pipe == 0)
				sys_loop_dl(comp, task, ccfg);
			else
				sys_loop_pipe1(comp, task, ccfg);
		}

		/* for pipe0 unlock event so disp could stop racing */
		if (ccfg->pipe == 0)
			cmdq_pkt_set_event(task->pkts[0],
				mml_ir_get_mml_stop_event(task->config->mml));

		/* Update cond jump pa for self loop,
		 * and job id for debug in both mode.
		 */
		sys_addr_update(comp, task, ccfg);
	} else if (mode == MML_MODE_APUDC) {
		if (ccfg->pipe == 0) {
			struct mml_sys *sys = comp_to_sys(comp);

			sys_apu_disable(task->pkts[ccfg->pipe],
				sys->apu_base, task->buf.src.apu_handle);
		}
	}

	return 0;
}

static s32 sys_repost(struct mml_comp *comp, struct mml_task *task,
		      struct mml_comp_config *ccfg)
{
	if (task->config->info.mode == MML_MODE_RACING)
		sys_addr_update(comp, task, ccfg);
	else if (task->config->info.mode == MML_MODE_DIRECT_LINK)
		sys_addr_update(comp, task, ccfg);
	return 0;
}

static const struct mml_comp_config_ops sys_config_ops = {
	.prepare = sys_config_prepare,
	.init = sys_init,
	.frame = sys_config_frame,
	.tile = sys_config_tile,
	.wait = sys_wait,
	.post = sys_post,
	.repost = sys_repost,
};

static void sys_debug_dump(struct mml_comp *comp)
{
	void __iomem *base = comp->base;
	struct mml_sys *sys = comp_to_sys(comp);
	u32 value;
	u32 i;

	mml_err("mml component %u dump:", comp->id);
	for (i = 0; i < sys->dbg_reg_cnt; i++) {
		value = readl(base + sys->dbg_regs[i].offset);
		mml_err("%s %#010x", sys->dbg_regs[i].name, value);
	}
}

static void sys_reset(struct mml_comp *comp, struct mml_frame_config *cfg, u32 pipe)
{
	const struct mml_topology_path *path = cfg->path[pipe];

	mml_err("[sys]reset bits %#llx for pipe %u", path->reset_bits, pipe);
	if (path->reset0 != U32_MAX) {
		writel(path->reset0, comp->base + SYS_SW0_RST_B_REG);
		writel(U32_MAX, comp->base + SYS_SW0_RST_B_REG);
	}
	if (path->reset1 != U32_MAX) {
		writel(path->reset1, comp->base + SYS_SW1_RST_B_REG);
		writel(U32_MAX, comp->base + SYS_SW1_RST_B_REG);
	}

	if (cfg->info.mode == MML_MODE_RACING ||
		cfg->info.mode == MML_MODE_DIRECT_LINK ||
		cfg->info.mode == MML_MODE_APUDC) {
		struct mml_sys *sys = comp_to_sys(comp);
		u16 event_ir_eof = mml_ir_get_target_event(cfg->mml);

		cmdq_clear_event(path->clt->chan, sys->event_racing_pipe0);
		cmdq_clear_event(path->clt->chan, sys->event_racing_pipe1);
		cmdq_clear_event(path->clt->chan, sys->event_racing_pipe1_next);
		if (event_ir_eof)
			cmdq_clear_event(path->clt->chan, event_ir_eof);
	}
}

static const struct mml_comp_debug_ops sys_debug_ops = {
	.dump = &sys_debug_dump,
	.reset = &sys_reset,
};

#ifndef MML_FPGA
s32 mml_mminfra_pw_enable(struct mml_comp *comp)
{
	struct mml_sys *sys = comp_to_sys(comp);
	int ret = 0;

	if (!sys->data->pw_mminfra)
		return ret;

	comp->mminfra_pw_cnt++;
	if (comp->mminfra_pw_cnt > 1)
		return 0;
	if (comp->mminfra_pw_cnt <= 0) {
		mml_err("%s comp %u %s cnt %d",
			__func__, comp->id, comp->name, comp->mminfra_pw_cnt);
		return -EINVAL;
	}

	if (!sys->dev) {
		mml_err("%s no mminfra pw for comp %u", __func__, comp->id);
		return 0;
	}

	/* Note: Do manually pw_enable and disable during mml/disp mtcmos on or off,
	 * cause mminfra must power on before other mtcmos, and must off after it.
	 */
	mml_msg_dpc("%s mminfra pm_runtime_resume_and_get", __func__);
	mml_mmp(dpc_pm_runtime_get, MMPROFILE_FLAG_PULSE, 0, 0);
	ret = pm_runtime_resume_and_get(sys->dev);
	if (ret)
		mml_err("%s enable pw-domain fail ret:%d", __func__, ret);

	return ret;
}

s32 mml_mminfra_pw_disable(struct mml_comp *comp)
{
	struct mml_sys *sys = comp_to_sys(comp);
	int ret = 0;

	if (!sys->data->pw_mminfra)
		return ret;

	comp->mminfra_pw_cnt--;
	if (comp->mminfra_pw_cnt > 0)
		return 0;
	if (comp->mminfra_pw_cnt < 0) {
		mml_err("%s comp %u %s cnt %d",
			__func__, comp->id, comp->name, comp->mminfra_pw_cnt);
		return -EINVAL;
	}

	if (!sys->dev) {
		mml_err("%s no mminfra pw for comp %u", __func__, comp->id);
		return 0;
	}

	mml_msg_dpc("%s mminfra pm_runtime_put_sync", __func__);
	mml_mmp(dpc_pm_runtime_put, MMPROFILE_FLAG_PULSE, 0, 0);
	pm_runtime_put_sync(sys->dev);

	return 0;
}

static s32 mml_comp_clk_aid_enable(struct mml_comp *comp)
{
	struct mml_sys *sys = comp_to_sys(comp);
	s32 ret = mml_comp_clk_enable(comp);

	if (ret < 0)
		return ret;

	if (comp->clk_cnt == 1) {
		if (sys->data->set_mml_uid)
			mml_set_uid(&sys->mml_scmi);

#ifndef MML_FPGA
		cmdq_util_set_mml_aid_selmode();
#endif
	}

	return 0;
}

static s32 mml_sys_comp_clk_enable(struct mml_comp *comp)
{
	int ret;

	/* original clk enable */
	ret = mml_comp_clk_aid_enable(comp);
	if (ret < 0)
		return ret;

	mml_update_comp_status(mml_mon_mmlsys, 1);
	mml_mmp(clk_enable, MMPROFILE_FLAG_PULSE, comp->id, 0);

	return 0;
}

static s32 mml_sys_comp_clk_disable(struct mml_comp *comp,
				    bool dpc)
{
	int ret;

	mml_update_comp_status(mml_mon_mmlsys, 0);

	/* original clk enable */
	ret = mml_comp_clk_disable(comp, dpc);
	if (ret < 0)
		return ret;
	mml_mmp(clk_disable, MMPROFILE_FLAG_PULSE, comp->id, 0);

	return 0;
}

static const struct mml_comp_hw_ops sys_hw_ops = {
	.pw_enable = mml_comp_pw_enable,
	.pw_disable = mml_comp_pw_disable,
	.clk_enable = &mml_sys_comp_clk_enable,
	.clk_disable = &mml_sys_comp_clk_disable,
};

static const struct mml_comp_hw_ops sys_hw_ops_mminfra = {
	.pw_enable = &mml_comp_pw_enable,
	.pw_disable = &mml_comp_pw_disable,
	.mminfra_pw_enable = mml_mminfra_pw_enable,
	.mminfra_pw_disable = mml_mminfra_pw_disable,
	.clk_enable = &mml_sys_comp_clk_enable,
	.clk_disable = &mml_sys_comp_clk_disable,
};

#endif

static int sys_comp_init(struct device *dev, struct mml_sys *sys,
			 struct mml_comp *comp)
{
	struct device_node *node = dev->of_node;
	int cnt, i;
	struct property *prop;
	const char *name;
	const __be32 *p;
	u32 value, comp_id, selbit, selmask;
	s32 ret;

	/* This larb_dev represent mmlsys it self for MMINFRA power domain on/off.
	 * The mml-core will manually do pw_enable/pw_disable by this dev,
	 * to power on/off mminfra mtcmos.
	 */
	if (sys->data->pw_mminfra) {
		sys->dev = dev;
		pm_runtime_enable(dev);
	}
	ret = mml_comp_init_larb(comp, dev);
	if (ret)
		mml_err("%s init larb fail %d", __func__, ret);

	/* Initialize mux-pins */
	cnt = of_property_count_elems_of_size(node, "mux-pins",
						  sizeof(struct mml_mux_pin));
	if (cnt < 0 || cnt > MML_MAX_SYS_MUX_PINS) {
		dev_err(dev, "no mux-pins or out of size in component %s: %d\n",
			node->full_name, cnt);
		return -EINVAL;
	}

	of_property_read_u16_array(node, "mux-pins", (u16 *)&sys->mux_pins[1],
		cnt * (sizeof(struct mml_mux_pin) / sizeof(u16)));
	for (i = 1; i <= cnt; i++) {
		struct mml_mux_pin *mux = &sys->mux_pins[i];

		if (mux->from >= MML_MAX_COMPONENTS ||
		    mux->to >= MML_MAX_COMPONENTS) {
			dev_err(dev, "comp idx %hu %hu out of boundary",
				mux->from, mux->to);
			continue;
		}
		if (mux->type == MML_MUX_SELIN)
			sys->adjacency[mux->to][mux->from] = i;
		else
			sys->adjacency[mux->from][mux->to] = i;
	}

	/* Initialize dbg-regs */
	i = 0;
	of_property_for_each_u32(node, "dbg-reg-offsets", prop, p, value) {
		if (i > MML_MAX_SYS_DBG_REGS) {
			dev_err(dev, "no dbg-reg-offsets or out of size in component %s: %d\n",
				node->full_name, i);
				return -EINVAL;
		}
		sys->dbg_regs[i].offset = value;
		i++;
	}
	sys->dbg_reg_cnt = i;

	i = 0;
	of_property_for_each_string(node, "dbg-reg-names", prop, name) {
		if (i > sys->dbg_reg_cnt) {
			dev_err(dev, "dbg-reg-names size over offsets size %s: %d\n",
				node->full_name, i);
				return -EINVAL;
		}
		sys->dbg_regs[i].name = name;
		i++;
	}

	if (sys->data->aidsel_mode == MML_AIDSEL_ENGINEBITS) {
		cnt = of_property_count_u32_elems(node, "aid-sel-engine");
		if (cnt / 4 > MML_MAX_AID_COMPS) {
			mml_err("count of aid-sel-engine %u more max aid comps",
				cnt);
			cnt = MML_MAX_AID_COMPS * 4;
		}
		for (i = 0; i + 3 < cnt; i += 4) {
			of_property_read_u32_index(node, "aid-sel-engine", i, &comp_id);
			of_property_read_u32_index(node, "aid-sel-engine", i + 1, &value);
			of_property_read_u32_index(node, "aid-sel-engine", i + 2, &selbit);
			of_property_read_u32_index(node, "aid-sel-engine", i + 3, &selmask);
			if (comp_id >= MML_MAX_COMPONENTS) {
				dev_err(dev, "component id %u is larger than max:%d\n",
					comp_id, MML_MAX_COMPONENTS);
				return -EINVAL;
			}
			sys->aid_sel[comp_id] = (u8)i / 4;
			sys->aid_sel_regs[i / 4] = (u16)value;
			sys->aid_sel_reg_bit[i / 4] = (u16)selbit;
			sys->aid_sel_reg_mask[i / 4] = (u16)selmask;
		}
	} else if (sys->data->aidsel_mode == MML_AIDSEL_ENGINE) {
		cnt = of_property_count_u32_elems(node, "aid-sel-engine");
		if (cnt / 2 > MML_MAX_AID_COMPS) {
			mml_err("count of aid-sel-engine %u more max aid comps",
				cnt);
			cnt = MML_MAX_AID_COMPS * 2;
		}
		for (i = 0; i + 1 < cnt; i += 2) {
			of_property_read_u32_index(node, "aid-sel-engine", i, &comp_id);
			of_property_read_u32_index(node, "aid-sel-engine", i + 1, &value);
			if (comp_id >= MML_MAX_COMPONENTS) {
				dev_err(dev, "component id %u is larger than max:%d\n",
					comp_id, MML_MAX_COMPONENTS);
				return -EINVAL;
			}
			sys->aid_sel[comp_id] = (u8)i / 2;
			sys->aid_sel_regs[i / 2] = (u16)value;
		}
	} else {
		cnt = of_property_count_u32_elems(node, "aid-sel");
		for (i = 0; i + 1 < cnt; i += 2) {
			of_property_read_u32_index(node, "aid-sel", i, &comp_id);
			of_property_read_u32_index(node, "aid-sel", i + 1, &value);
			if (comp_id >= MML_MAX_COMPONENTS) {
				dev_err(dev, "component id %u is larger than max:%d\n",
					comp_id, MML_MAX_COMPONENTS);
				return -EINVAL;
			}
			sys->aid_sel[comp_id] = (u8)value;
		}
	}

	of_property_read_u16(dev->of_node, "ready-sel", &sys->inline_ready_sel);

	comp->config_ops = &sys_config_ops;
	comp->debug_ops = &sys_debug_ops;

	mml_init_swpm_comp(mml_mon_mmlsys, comp);

#ifndef MML_FPGA
	/* scmi(sspm) config aid/uid support */
	if (sys->data->pw_mminfra)
		comp->hw_ops = &sys_hw_ops_mminfra;
	else
		comp->hw_ops = &sys_hw_ops;
#endif
	return 0;
}

static void sys_config_done_cb(struct mml_task *task, void *cb_param)
{
	struct mtk_addon_mml_config *cfg = cb_param;

	cfg->task = task;
	mml_msg("%s mml task:%p", __func__, cfg->task);
}

static struct mml_dle_ctx *sys_get_dle_ctx(struct mml_sys *sys,
					   struct mml_dle_param *dl)
{
	if (dl && (IS_ERR_OR_NULL(sys->dle_ctx) ||
	    memcmp(&sys->dle_param, dl, sizeof(*dl)))) {
		mml_dle_put_context(sys->dle_ctx);
		sys->dle_ctx = mml_dle_get_context(sys->master, dl);
		sys->dle_param = *dl;
	}
	return sys->dle_ctx;
}

#define has_cfg_op(_comp, op) \
	(_comp->config_ops && _comp->config_ops->op)
#define call_cfg_op(_comp, op, ...) \
	(has_cfg_op(_comp, op) ? \
		_comp->config_ops->op(_comp, ##__VA_ARGS__) : 0)

#define call_hw_op(_comp, op, ...) \
	(_comp->hw_ops->op ? _comp->hw_ops->op(_comp, ##__VA_ARGS__) : 0)

static void ddp_command_make(struct mml_task *task, u32 pipe,
			     struct cmdq_pkt *pkt)
{
	const struct mml_topology_path *path = task->config->path[pipe];
	struct mml_comp_config *ccfg = task->config->cache[pipe].cfg;
	struct mml_comp *comp;
	u32 i;

	/* borrow task pkt pointer */
	task->pkts[pipe] = pkt;

	/* call all component init and frame op, include mmlsys and mutex */
	for (i = 0; i < path->node_cnt; i++) {
		comp = path->nodes[i].comp;
		call_cfg_op(comp, init, task, &ccfg[i]);
	}
	for (i = 0; i < path->node_cnt; i++) {
		comp = path->nodes[i].comp;
		call_cfg_op(comp, frame, task, &ccfg[i]);
	}
	for (i = 0; i < path->node_cnt; i++) {
		comp = path->nodes[i].comp;
		call_cfg_op(comp, tile, task, &ccfg[i], 0);
	}

	for (i = 0; i < path->node_cnt; i++) {
		comp = path->nodes[i].comp;
		call_cfg_op(comp, post, task, &ccfg[i]);
	}

	/* return task pkt pointer */
	task->pkts[pipe] = NULL;
}

static void sys_ddp_disable_locked(const struct mml_topology_path *path,
				   u32 pipe,
				   struct mml_task *task)
{
	struct mml_comp *comp;
	u32 i;

	if (path->mmlsys)
		call_hw_op(path->mmlsys, mminfra_pw_enable);

	mml_trace_ex_begin("%s_%s_%u", __func__, "clk", pipe);
	for (i = 0; i < path->node_cnt; i++) {
		if (i == path->mmlsys_idx || i == path->mutex_idx)
			continue;
		comp = path->nodes[i].comp;
		call_hw_op(comp, clk_disable, task->config->dpc);
	}

	if (path->mutex)
		call_hw_op(path->mutex, clk_disable, task->config->dpc);
	if (path->mmlsys)
		call_hw_op(path->mmlsys, clk_disable, task->config->dpc);

	if (task->config->dpc) {
		/* set dpc total hrt/srt bw to 0 */
		mml_msg_dpc("%s dpc total_bw total_peak to 0", __func__);
		mml_dpc_srt_bw_set(DPC_SUBSYS_MML1, 0, false);
		mml_dpc_hrt_bw_set(DPC_SUBSYS_MML1, 0, false);
	}

	mml_trace_ex_end();

	mml_trace_ex_begin("%s_%s_%u", __func__, "pw", pipe);

	if (path->mmlsys) {
		call_hw_op(path->mmlsys, pw_disable);
		call_hw_op(path->mmlsys, mminfra_pw_disable);
	}
	mml_trace_ex_end();
}

static void sys_ddp_disable(struct mml_sys *sys, struct mml_task *task, u32 pipe)
{
	const struct mml_topology_path *path = task->config->path[pipe];

	mml_clock_lock(task->config->mml);

	/* path disconnected */
	if (!sys->ddp_path[pipe])
		goto disabled;
	/* check task path */
	if (path != sys->ddp_path[pipe])
		mml_log("[warn]%s task path found %p was not path connected %p",
			__func__, path, sys->ddp_path[pipe]);

	if (task->pipe[pipe].en.clk) {
		/* disable and pop ddp paths */
		sys_ddp_disable_locked(sys->ddp_path[pipe], pipe, task);
		if (sys->pre_ddp_path[pipe] &&
		    sys->pre_ddp_path[pipe] != sys->ddp_path[pipe])
			sys_ddp_disable_locked(sys->pre_ddp_path[pipe],
				pipe, task);
		task->pipe[pipe].en.clk = false;
	}
	sys->ddp_path[pipe] = NULL;
	sys->pre_ddp_path[pipe] = NULL;

disabled:
	mml_clock_unlock(task->config->mml);

	mml_msg("%s task %p pipe %u", __func__, task, pipe);
}

static void sys_ddp_enable(struct mml_sys *sys, struct mml_task *task, u32 pipe)
{
	const struct mml_topology_path *path = task->config->path[pipe];
	struct mml_comp *comp;
	u32 i;

	mml_msg("%s task %p pipe %u", __func__, task, pipe);

	mml_clock_lock(task->config->mml);

	/* path connected */
	if (path == sys->ddp_path[pipe])
		goto enabled;

	mml_trace_ex_begin("%s_%s_%u", __func__, "pw", pipe);

	if (path->mmlsys) {
		call_hw_op(path->mmlsys, mminfra_pw_enable);
		call_hw_op(path->mmlsys, pw_enable);
	}

	mml_trace_ex_end();

	mml_trace_ex_begin("%s_%s_%u", __func__, "clk", pipe);
	if (path->mmlsys)
		call_hw_op(path->mmlsys, clk_enable);
	if (path->mutex)
		call_hw_op(path->mutex, clk_enable);

	for (i = 0; i < path->node_cnt; i++) {
		if (i == path->mmlsys_idx || i == path->mutex_idx)
			continue;
		comp = path->nodes[i].comp;
		call_hw_op(comp, clk_enable);
	}
	mml_trace_ex_end();

	if (path->mmlsys)
		call_hw_op(path->mmlsys, mminfra_pw_disable);

#ifndef MML_FPGA
	cmdq_util_prebuilt_init(CMDQ_PREBUILT_MML);
#endif

enabled:
	/* disable pre_ddp_path if path diff */
	if (sys->pre_ddp_path[pipe] &&
	    sys->pre_ddp_path[pipe] != sys->ddp_path[pipe])
		sys_ddp_disable_locked(sys->pre_ddp_path[pipe], pipe, task);

	/* pop previous path and push paths */
	sys->pre_ddp_path[pipe] = sys->ddp_path[pipe];
	sys->ddp_path[pipe] = path;

	task->pipe[pipe].en.clk = true;
	mml_clock_unlock(task->config->mml);
}

static const struct mml_submit bypass_submit = {
	.info = {
		.src = {
			.width = U16_MAX,
			.height = U16_MAX,
			.format = MML_FMT_UYV1010102,
			.profile = MML_YCBCR_PROFILE_BT709,
		},
		.dest[0] = {
			.data = {
				.width = U16_MAX,
				.height = U16_MAX,
				.format = MML_FMT_UYV1010102,
				.profile = MML_YCBCR_PROFILE_BT709,
			},
			.crop.r = {
				.width = U16_MAX,
				.height = U16_MAX,
			},
			.compose = {
				.width = U16_MAX,
				.height = U16_MAX,
			},
		},
		.dest_cnt = 1,
		.mode = MML_MODE_DDP_ADDON,
	},
	.buffer = {
		.src.fence = -1,
		.dest[0].fence = -1,
		.dest_cnt = 1,
	},
};

static void sys_mml_calc_cfg(struct mtk_ddp_comp *ddp_comp,
			     union mtk_addon_config *addon_config,
			     struct cmdq_pkt *pkt)
{
	struct mml_sys *sys = ddp_comp_to_sys(ddp_comp);
	struct mtk_addon_mml_config *cfg = &addon_config->addon_mml_config;
	struct mml_dle_ctx *ctx;
	struct mml_dle_param dl;
	struct mml_dle_frame_info info = {0};
	struct mml_frame_tile **frame_tile;
	struct mml_frame_config *frame_cfg;
	s32 i, pipe_cnt = cfg->dual ? 2 : 1;
	s32 ret;
	u32 src_offset, sz = 0;
	char frame[64];

	if (cfg->submit.info.mode == MML_MODE_DIRECT_LINK) {
		mml_log("%s module:%d no mml calc for direct link",
			__func__, cfg->config_type.module);
		return;
	}

	mml_msg("%s mml mode %d module:%d",
		__func__, cfg->submit.info.mode, cfg->config_type.module);

	mml_mmp(addon_mml_calc_cfg, MMPROFILE_FLAG_PULSE,
		cfg->submit.info.mode, cfg->config_type.module);

	if (!cfg->submit.info.src.width ||
	    (cfg->submit.info.dest[0].pq_config.en_region_pq &&
	    !cfg->submit.info.seg_map.width)) {
		/* set test submit */
		cfg->submit = bypass_submit;
	}

	dl.dual = cfg->dual;
	dl.config_cb = sys_config_done_cb;
	ctx = sys_get_dle_ctx(sys, &dl);
	if (IS_ERR(ctx)) {
		mml_err("%s fail to get mml ctx", __func__);
		return;
	}

	for (i = 0; i < pipe_cnt; i++) {
		info.dl_out[i].left = cfg->mml_dst_roi[i].x;
		info.dl_out[i].top = cfg->mml_dst_roi[i].y;
		info.dl_out[i].width = cfg->mml_dst_roi[i].width;
		info.dl_out[i].height = cfg->mml_dst_roi[i].height;
	}

	mml_mmp(addon_dle_config, MMPROFILE_FLAG_START, 0, 0);
	ret = mml_dle_config(ctx, &cfg->submit, &info, cfg);
	mml_mmp(addon_dle_config, MMPROFILE_FLAG_END, 0, 0);
	if (ret) {
		mml_err("%s config fail", __func__);
		return;
	}

	/* wait submit_done */
	if (unlikely(pipe_cnt <= 0)) {
		mml_err("%s pipe_cnt <=0 %d", __func__, pipe_cnt);
		return;
	}
	if (!cfg->task || !cfg->task->config->frame_tile[pipe_cnt - 1]) {
		mml_err("%s no tiles for task %p pipe_cnt %d", __func__,
			cfg->task, pipe_cnt);
		/* avoid disp exception */
		for (i = 0; i < pipe_cnt; i++)
			cfg->mml_src_roi[i] = cfg->mml_dst_roi[i];
		return;
	}

	frame_cfg = cfg->task->config;
	frame_tile = cfg->task->config->frame_tile;
	src_offset = cfg->submit.info.dest[0].crop.r.left;
	for (i = 0; i < pipe_cnt; i++) {
		cfg->mml_src_roi[i].x = frame_tile[i]->src_crop.left + src_offset;
		cfg->mml_src_roi[i].y = frame_tile[i]->src_crop.top;
		cfg->mml_src_roi[i].width = frame_tile[i]->src_crop.width;
		cfg->mml_src_roi[i].height = frame_tile[i]->src_crop.height;

		mml_mmp2(addon_dle_config, MMPROFILE_FLAG_PULSE,
			cfg->mml_src_roi[i].x,
			cfg->mml_src_roi[i].y,
			cfg->mml_src_roi[i].width,
			cfg->mml_src_roi[i].height);

		if (frame_cfg->dl_in[i].width || frame_cfg->dl_in[i].height)
			continue;

		frame_cfg->dl_in[i] = frame_tile[i]->src_crop;
		frame_cfg->dl_in[i].left = cfg->mml_src_roi[i].x;

		if (sz >= sizeof(frame))
			continue;

		ret = snprintf(&frame[sz], sizeof(frame) - sz,
			" %u:(%u, %u, %u, %u)",
			i,
			frame_cfg->dl_in[i].left,
			frame_cfg->dl_in[i].top,
			frame_cfg->dl_in[i].width,
			frame_cfg->dl_in[i].height);
		if (ret > 0)
			sz += ret;
	}

	if (sz)
		mml_log("dl_in%s left offset %u", frame, src_offset);
}

static void sys_addon_connect(struct mml_sys *sys,
			      struct mtk_addon_mml_config *cfg,
			      struct cmdq_pkt *pkt)
{
	if (!cfg->task || !cfg->task->config->frame_tile[cfg->pipe]) {
		mml_err("%s no tile for task %p pipe %u", __func__,
			cfg->task, cfg->pipe);
		return;
	}

	if (cfg->task->err) {
		mml_err("%s tile error stop make command task %p pipe %u",
			__func__, cfg->task, cfg->pipe);
		return;
	}

	sys_ddp_enable(sys, cfg->task, cfg->pipe);

	ddp_command_make(cfg->task, cfg->pipe, pkt);

	if (cfg->task->config->info.mode == MML_MODE_DDP_ADDON &&
		cfg->task->config->info.dest[0].pq_config.en_hdr)
		wait_for_completion(&cfg->task->pq_task->hdr_curve_ready[cfg->pipe]);

#if IS_ENABLED(CONFIG_MTK_MML_DEBUG)
	if (mml_dle_delay)
		usleep_range(mml_dle_delay, mml_dle_delay + 10);
#endif
}

static void sys_addon_disconnect(struct mml_sys *sys,
				 struct mtk_addon_mml_config *cfg)
{
	struct mml_dle_ctx *ctx;

	ctx = sys_get_dle_ctx(sys, NULL);
	if (IS_ERR_OR_NULL(ctx)) {
		mml_err("%s fail to get mml ctx", __func__);
		return;
	}

	cfg->task = mml_dle_stop(ctx);
	if (!cfg->task) {
		mml_err("%s fail to find task", __func__);
		return;
	}
	mml_msg("%s dle stop task %p", __func__, cfg->task);

	if (!cfg->task || !cfg->task->config->path[cfg->pipe]) {
		mml_err("%s no path for task %p pipe %u", __func__, cfg->task, cfg->pipe);
		return;
	}
}

static void sys_addon_config(struct mtk_ddp_comp *ddp_comp,
			     enum mtk_ddp_comp_id prev,
			     enum mtk_ddp_comp_id next,
			     union mtk_addon_config *addon_config,
			     struct cmdq_pkt *pkt)
{
	struct mml_sys *sys = ddp_comp_to_sys(ddp_comp);
	struct mtk_addon_mml_config *cfg;

	if (IS_ERR_OR_NULL(addon_config)) {
		mml_err("%s addon config fail %pe", __func__, addon_config);
		return;
	}

	cfg = &addon_config->addon_mml_config;

	mml_mmp(addon_addon_config, MMPROFILE_FLAG_PULSE, cfg->config_type.type, 0);

	mml_msg("%s type:%d", __func__, cfg->config_type.type);
	if (cfg->config_type.type == ADDON_DISCONNECT)
		sys_addon_disconnect(sys, cfg);
	else
		sys_addon_connect(sys, cfg, pkt);
}

static void sys_start(struct mtk_ddp_comp *ddp_comp, struct cmdq_pkt *pkt)
{
	struct mml_sys *sys = ddp_comp_to_sys(ddp_comp);
	struct mml_dle_ctx *ctx = sys_get_dle_ctx(sys, NULL);

	mml_mmp(addon_start, MMPROFILE_FLAG_PULSE, 0, 0);

	if (IS_ERR_OR_NULL(ctx)) {
		mml_err("%s fail to get mml ctx", __func__);
		return;
	}

	mml_dle_start(ctx);
}

static void sys_unprepare(struct mtk_ddp_comp *ddp_comp)
{
	struct mml_sys *sys = ddp_comp_to_sys(ddp_comp);
	struct mml_dle_ctx *ctx = sys_get_dle_ctx(sys, NULL);
	struct mml_task *task;

	mml_mmp(addon_unprepare, MMPROFILE_FLAG_PULSE, 0, 0);

	if (IS_ERR_OR_NULL(ctx)) {
		mml_msg("%s fail to get mml ctx", __func__);
		return;
	}

	task = mml_dle_disable(ctx);
	if (!task) {
		mml_err("%s fail to find task", __func__);
		return;
	}

	sys_ddp_disable(sys, task, 0);
	if (task->config->dual)
		sys_ddp_disable(sys, task, 1);
}

#if IS_ENABLED(CONFIG_MTK_MML_DEBUG)
#define call_dbg_op(_comp, op, ...) \
	((_comp->debug_ops && _comp->debug_ops->op) ? \
		_comp->debug_ops->op(_comp, ##__VA_ARGS__) : 0)

static void ddp_comp_dump(const struct mml_topology_path *path)
{
	struct mml_comp *comp;
	u32 i;

	if (!path)
		return;
	for (i = 0; i < path->node_cnt; i++) {
		comp = path->nodes[i].comp;
		call_dbg_op(comp, dump);
	}
}
#endif

static void sys_ddp_dump(struct mtk_ddp_comp *ddp_comp)
{
#if IS_ENABLED(CONFIG_MTK_MML_DEBUG)
	struct mml_sys *sys;

	if (!mml_ddp_dump) {
		mml_err("Inline rotate direct link fail");
		return;
	}

	sys = ddp_comp_to_sys(ddp_comp);

	ddp_comp_dump(sys->ddp_path[0]);
	ddp_comp_dump(sys->ddp_path[1]);
#endif
}

static const struct mtk_ddp_comp_funcs sys_ddp_funcs = {
	.mml_calc_cfg = sys_mml_calc_cfg,
	.addon_config = sys_addon_config,
	.start = sys_start,
	.unprepare = sys_unprepare,
	.dump = sys_ddp_dump,
};

static s32 dli_tile_prepare(struct mml_comp *comp, struct mml_task *task,
			    struct mml_comp_config *ccfg,
			    struct tile_func_block *func,
			    union mml_tile_data *data)
{
	struct mml_frame_data *src = &task->config->info.src;
	struct mml_frame_dest *dest = &task->config->info.dest[0];

	func->type = TILE_TYPE_RDMA;
	/* height align wrot out cause height value config in inlinerot height */
	func->full_size_x_in = dest->crop.r.width;
	func->full_size_y_in = src->height;
	func->full_size_x_out = dest->crop.r.width;
	func->full_size_y_out = src->height;

	return 0;
}

static const struct mml_comp_tile_ops dli_tile_ops = {
	.prepare = dli_tile_prepare,
};

static void dlo_config_left(struct mml_frame_config *cfg,
			    struct mml_frame_dest *dest,
			    struct dlo_tile_data *data)
{
	data->enable_x_crop = true;
	data->crop.left = 0;
	data->crop.width = cfg->dl_out[0].width;

	mml_msg("%s width %u", __func__, data->crop.width);

	if (cfg->info.mode == MML_MODE_DIRECT_LINK)
		return;

	if (data->crop.width == 0 || data->crop.width >= dest->compose.width ||
	    cfg->dl_out[0].height != dest->compose.height)
		mml_err("dlo[0] (%u, %u, %u, %u) cannot match compose (%u, %u)",
			data->crop.left, 0, data->crop.width, cfg->dl_out[0].height,
			dest->compose.width, dest->compose.height);
}

static void dlo_config_right(struct mml_frame_config *cfg,
			     struct mml_frame_dest *dest,
			     struct dlo_tile_data *data)
{
	data->enable_x_crop = true;
	data->crop.left = cfg->dl_out[1].left - cfg->dl_out[0].left;
	data->crop.width = cfg->dl_out[1].width;

	mml_msg("%s left %u width %u", __func__, data->crop.left, data->crop.width);
	if (cfg->info.mode == MML_MODE_DIRECT_LINK)
		return;

	if (data->crop.left > cfg->dl_out[0].width ||
	    data->crop.left + data->crop.width != dest->compose.width ||
	    cfg->dl_out[1].height != dest->compose.height)
		mml_err("dlo[1] (%u, %u, %u, %u) cannot match compose (%u, %u) left %u",
			data->crop.left, 0, data->crop.width, cfg->dl_out[1].height,
			dest->compose.width, dest->compose.height,
			cfg->dl_out[1].left - cfg->dl_out[0].left);
}

static s32 dlo_tile_prepare(struct mml_comp *comp, struct mml_task *task,
			    struct mml_comp_config *ccfg,
			    struct tile_func_block *func,
			    union mml_tile_data *data)
{
	struct mml_frame_config *cfg = task->config;
	struct mml_frame_dest *dest = &cfg->info.dest[ccfg->node->out_idx];

	if (cfg->dual) {
		if (ccfg->pipe == 0)
			dlo_config_left(cfg, dest, &data->dlo);
		else
			dlo_config_right(cfg, dest, &data->dlo);
	} else {
		data->dlo.crop.width = dest->compose.width;
	}
	data->dlo.crop.height = dest->compose.height;

	func->type = TILE_TYPE_WDMA;
	func->back_func = tile_dlo_back;
	func->data = data;
	return 0;
}

static const struct mml_comp_tile_ops dlo_tile_ops = {
	.prepare = dlo_tile_prepare,
};

static s32 dl_config_tile(struct mml_comp *comp, struct mml_task *task,
			  struct mml_comp_config *ccfg, u32 idx)
{
	struct mml_sys *sys = comp_to_sys(comp);
	struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];
	const phys_addr_t base_pa = comp->base_pa;
	struct mml_tile_engine *tile = config_get_tile(task->config, ccfg, idx);

	u16 offset = sys->dl_relays[sys->adjacency[comp->id][comp->id]];
	u32 dl_w = tile->in.xe - tile->in.xs + 1;
	u32 dl_h = tile->in.ye - tile->in.ys + 1;
	u32 size = (dl_h << 16) + dl_w;

	cmdq_pkt_write(pkt, NULL, base_pa + offset, size, U32_MAX);
	return 0;
}

static s32 dl_wait(struct mml_comp *comp, struct mml_task *task,
	struct mml_comp_config *ccfg, u32 idx)
{
	if (task->config->info.mode == MML_MODE_DIRECT_LINK)
		cmdq_pkt_wfe(task->pkts[ccfg->pipe], task->config->info.disp_done_event);

	return 0;
}

static s32 dl_post(struct mml_comp *comp, struct mml_task *task,
		   struct mml_comp_config *ccfg)
{
	if (task->config->info.mode == MML_MODE_DIRECT_LINK) {
		struct mml_pipe_cache *cache = &task->config->cache[ccfg->pipe];
		struct mml_sys *sys = comp_to_sys(comp);
		u32 pixel;

		cache->line_bubble += task->config->dl_out[ccfg->pipe].left;
		pixel = cache_max_pixel(cache);
		if (sys->data->px_per_tick)
			pixel = pixel / sys->data->px_per_tick;
		cache->max_pixel = max(cache->max_pixel, pixel);

		sys_msg("%s task %p pipe %u bubble %u pixel %ux%u %u",
			__func__, task, ccfg->pipe, cache->line_bubble,
			cache->max_size.width, cache->max_size.height, cache->max_pixel);

		if (task->config->dpc && sys->dpc_base &&
			(mml_dl_dpc & MML_DLDPC_VOTE)) {
			struct cmdq_pkt *pkt = task->pkts[ccfg->pipe];

			cmdq_pkt_write(pkt, NULL, sys->dpc_base + VLP_VOTE_CLR,
				BIT(DISP_VIDLE_USER_MML_CMDQ), U32_MAX);
			cmdq_pkt_write(pkt, NULL, sys->dpc_base + VLP_VOTE_CLR,
				BIT(DISP_VIDLE_USER_MML_CMDQ), U32_MAX);
		}
	}

	return 0;
}

static const struct mml_comp_config_ops dl_config_ops = {
	.tile = dl_config_tile,
	.wait = dl_wait,
	.post = dl_post,
};

static const struct mml_comp_hw_ops dl_hw_ops = {
	.clk_enable = mml_comp_clk_enable,
	.clk_disable = mml_comp_clk_disable,
	/* TODO: pmqos_op
	 * .qos_datasize_get = dl_datasize_get,
	 * .qos_set = mml_comp_qos_set,
	 * .qos_clear = mml_comp_qos_clear,
	 */
};

static int dl_comp_init(struct device *dev, struct mml_sys *sys,
			struct mml_comp *comp)
{
	struct device_node *node = dev->of_node;
	char name[32] = "";
	u16 offset = 0;
	int ret = 0;

	/* init larb for mtcmos */
	ret = mml_comp_init_larb(comp, dev);
	if (ret) {
		if (ret == -EPROBE_DEFER)
			return ret;
		dev_err(dev, "fail to init component %u larb ret %d",
			comp->id, ret);
	}

	if (sys->dl_cnt >= ARRAY_SIZE(sys->dl_relays) - 1) {
		dev_err(dev, "out of dl-relay size in component %s: %d\n",
			node->full_name, sys->dl_cnt + 1);
		return -EINVAL;
	}
	if (!comp->name) {
		dev_err(dev, "no comp-name of mmlsys comp-%d (type dl-in)\n",
			comp->sub_idx);
		return -EINVAL;
	}

	ret = snprintf(name, sizeof(name), "%s-dl-relay", comp->name);
	if (ret >= sizeof(name)) {
		dev_err(dev, "len:%d over name size:%lu", ret, sizeof(name));
		name[sizeof(name) - 1] = '\0';
	}
	ret = of_property_read_u16(node, name, &offset);
	if (ret) {
		dev_err(dev, "no %s property in node %s\n",
			name, node->full_name);
		return ret;
	}

	sys->dl_relays[++sys->dl_cnt] = offset;
	sys->adjacency[comp->id][comp->id] = sys->dl_cnt;
	comp->config_ops = &dl_config_ops;
	comp->hw_ops = &dl_hw_ops;
	return 0;
}

static int dli_comp_init(struct device *dev, struct mml_sys *sys,
			 struct mml_comp *comp)
{
	int ret = dl_comp_init(dev, sys, comp);

	if (ret)
		return ret;
	comp->tile_ops = &dli_tile_ops;
	return 0;
}

static int dlo_comp_init(struct device *dev, struct mml_sys *sys,
			 struct mml_comp *comp)
{
	int ret = dl_comp_init(dev, sys, comp);

	if (ret)
		return ret;
	comp->tile_ops = &dlo_tile_ops;
	return 0;
}

static const struct mtk_ddp_comp_funcs dl_ddp_funcs = {
};

static int subcomp_init(struct platform_device *pdev, struct mml_sys *sys,
			int subcomponent)
{
	struct device *dev = &pdev->dev;
	struct mml_comp *comp = &sys->comps[subcomponent];
	const struct mml_data *data = sys->data;
	u32 comp_type = 0;
	int ret;

	ret = mml_subcomp_init(pdev, subcomponent, comp);
	if (ret)
		return ret;

	if (of_property_read_u32_index(dev->of_node, "comp-types",
				       subcomponent, &comp_type)) {
		dev_info(dev, "no comp-type of mmlsys comp-%d\n", subcomponent);
		return 0;
	}
	if (comp_type < MML_COMP_TYPE_TOTAL) {
		if (data->comp_inits[comp_type]) {
			ret = data->comp_inits[comp_type](dev, sys, comp);
			if (ret)
				return ret;
		}

		/* currently only mml-sys comp add to ddp comps */
		if (data->ddp_comp_funcs[comp_type] && comp_type == MML_CT_SYS) {
			ret = mml_ddp_comp_init(dev, &sys->ddp_comps[subcomponent],
						comp, data->ddp_comp_funcs[comp_type]);
			if (unlikely(ret)) {
				mml_log("failed to init ddp comp-%d: %d",
					subcomponent, ret);
				return ret;
			}
			sys->ddp_comp_en |= 1 << subcomponent;
		}
	} else
		mml_err("%s comp_type %d >= MML_COMP_TYPE_TOTAL", __func__, comp_type);

	return ret;
}

static int mml_sys_init(struct platform_device *pdev, struct mml_sys *sys,
			const struct component_ops *comp_ops)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	int comp_cnt, i;
	int ret;

	sys->data = of_device_get_match_data(dev);

	/* Initialize component and subcomponents */
	comp_cnt = of_mml_count_comps(node);
	if (comp_cnt <= 0) {
		dev_err(dev, "no comp-ids in component %s: %d\n",
			node->full_name, comp_cnt);
		return -EINVAL;
	}

	for (i = 0; i < comp_cnt; i++) {
		ret = subcomp_init(pdev, sys, i);
		if (ret) {
			dev_err(dev, "failed to init mmlsys comp-%d: %d\n",
				i, ret);
			return ret;
		}
	}

	ret = component_add(dev, comp_ops);
	if (ret) {
		dev_err(dev, "failed to add mmlsys comp-%d: %d\n", 0, ret);
		return ret;
	}
	for (i = 1; i < comp_cnt; i++) {
		ret = component_add_typed(dev, comp_ops, i);
		if (ret) {
			dev_err(dev, "failed to add mmlsys comp-%d: %d\n",
				i, ret);
			goto err_comp_add;
		}
	}
	sys->comp_cnt = comp_cnt;

	/* add mmlsys component to match ddp master. */
	/* unlike to mml, to ddp register all components in one binding. */
	if (sys->ddp_comp_en)
		ret = component_add(dev, comp_ops);

	/* events for racing/dl mode */
	of_property_read_u16(dev->of_node, "event-racing-pipe0",
			     &sys->event_racing_pipe0);
	of_property_read_u16(dev->of_node, "event-racing-pipe1",
			     &sys->event_racing_pipe1);
	of_property_read_u16(dev->of_node, "event-racing-pipe1-next",
			     &sys->event_racing_pipe1_next);

	/* apu direct couple mml feature */
	of_property_read_u16(dev->of_node, "event-apu-start",
			     &sys->event_apu_start);
	sys->apu_base = mml_get_node_base_pa(pdev, "apu", 0, &sys->apu_base_va);

	of_property_read_u32(dev->of_node, "dpc-base", &sys->dpc_base);
	if (sys->dpc_base)
		mml_log("sys support dpc base %#010x", sys->dpc_base);

	return 0;

err_comp_add:
	for (; i > 0; i--)
		component_del(dev, comp_ops);
	return ret;
}

static struct mml_sys *dbg_probed_components[2];
static int dbg_probed_count;

struct mml_sys *mml_sys_create(struct platform_device *pdev,
			       const struct component_ops *comp_ops)
{
	struct device *dev = &pdev->dev;
	struct mml_sys *sys;
	int ret;

	sys = devm_kzalloc(dev, sizeof(*sys), GFP_KERNEL);
	if (!sys)
		return ERR_PTR(-ENOMEM);

	ret = mml_sys_init(pdev, sys, comp_ops);
	if (ret) {
		dev_err(dev, "failed to init mml sys: %d\n", ret);
		devm_kfree(dev, sys);
		return ERR_PTR(ret);
	}

	if (unlikely(dbg_probed_count < 0))
		return ERR_PTR(-EFAULT);

	dbg_probed_components[dbg_probed_count++] = sys;
	return sys;
}

void mml_sys_destroy(struct platform_device *pdev, struct mml_sys *sys,
		     const struct component_ops *comp_ops)
{
	int i;

	mml_dle_put_context(sys->dle_ctx);
	if (sys->data->pw_mminfra)
		pm_runtime_disable(&pdev->dev);
	for (i = 0; i < sys->comp_cnt; i++)
		component_del(&pdev->dev, comp_ops);
	if (sys->ddp_comp_en)
		component_del(&pdev->dev, comp_ops);
	devm_kfree(&pdev->dev, sys);
}

void mml_sys_put_dle_ctx(void *mml)
{
	struct mml_sys *sys = mml_get_sys(mml);

	if (!sys) {
		mml_err("%s no sys to put dle context", __func__);
		return;
	}
	mml_dle_put_context(sys->dle_ctx);
	sys->dle_ctx = NULL;
}

static int bind_mml(struct device *dev, struct device *master,
		    struct mml_sys *sys)
{
	s32 ret;

	if (unlikely(!sys)) {
		dev_err(dev, "sys is NULL\n");
		return -EFAULT;
	}

	if (WARN_ON(sys->master && sys->master != master)) {
		dev_err(dev, "failed to register component %s to new master %s from old %s\n",
			dev->of_node->full_name,
			master->of_node->full_name,
			sys->master->of_node->full_name);
		return -EUSERS;
	}
	sys->master = master;

	if (WARN_ON(sys->comp_bound >= sys->comp_cnt))
		return -ERANGE;
	ret = mml_register_comp(master, &sys->comps[sys->comp_bound++]);
	if (ret) {
		dev_err(dev, "failed to register mml component %s: %d\n",
			dev->of_node->full_name, ret);
		sys->comp_bound--;
	}
	return ret;
}

static int bind_ddp_each(struct device *dev, struct drm_device *drm_dev,
			 struct mml_sys *sys)
{
	int i, ret;

	for (i = sys->ddp_bound; i < sys->comp_cnt; i++)
		if (sys->ddp_comp_en & (1 << i))
			break;
	sys->ddp_bound = i;

	if (WARN_ON(sys->ddp_bound >= sys->comp_cnt))
		return -ERANGE;
	ret = mml_ddp_comp_register(drm_dev, &sys->ddp_comps[sys->ddp_bound++]);
	if (ret) {
		dev_err(dev, "failed to register ddp component %s: %d\n",
			dev->of_node->full_name, ret);
		sys->ddp_bound--;
	}
	return ret;
}

static int bind_ddp(struct device *dev, struct drm_device *drm_dev,
		    struct mml_sys *sys)
{
	int ret = 0;

	while (sys->ddp_comp_en & GENMASK(31, sys->ddp_bound)) {
		ret = bind_ddp_each(dev, drm_dev, sys);
		if (ret)
			break;
	}
	return ret;
}

int mml_sys_bind(struct device *dev, struct device *master,
		 struct mml_sys *sys, void *data)
{
	if (!data)
		return bind_mml(dev, master, sys);
	else
		return bind_ddp(dev, data, sys);
}

static void unbind_mml(struct device *master, struct mml_sys *sys)
{
	if (WARN_ON(sys->comp_bound == 0 ||
		    sys->comp_bound > MML_MAX_SYS_COMPONENTS))
		return;
	mml_unregister_comp(master, &sys->comps[--sys->comp_bound]);
}

static void unbind_ddp(struct drm_device *drm_dev, struct mml_sys *sys)
{
	s32 i;

	for (i = sys->ddp_bound; i > 0; i--)
		if (sys->ddp_comp_en & (1 << (i - 1)))
			break;
	sys->ddp_bound = i;

	if (WARN_ON(sys->ddp_bound == 0 ||
		    sys->ddp_bound > MML_MAX_SYS_COMPONENTS))
		return;
	mml_ddp_comp_unregister(drm_dev, &sys->ddp_comps[--sys->ddp_bound]);
}

void mml_sys_unbind(struct device *dev, struct device *master,
		    struct mml_sys *sys, void *data)
{
	if (!data)
		unbind_mml(master, sys);
	else
		unbind_ddp(data, sys);
}

static int mml_bind(struct device *dev, struct device *master, void *data)
{
	return mml_sys_bind(dev, master, dev_get_drvdata(dev), data);
}

static void mml_unbind(struct device *dev, struct device *master, void *data)
{
	mml_sys_unbind(dev, master, dev_get_drvdata(dev), data);
}

static const struct component_ops mml_comp_ops = {
	.bind	= mml_bind,
	.unbind = mml_unbind,
};

static int probe(struct platform_device *pdev)
{
	struct mml_sys *priv;

	priv = mml_sys_create(pdev, &mml_comp_ops);
	if (IS_ERR(priv)) {
		dev_err(&pdev->dev, "failed to init mml sys: %ld\n",
			PTR_ERR(priv));
		return PTR_ERR(priv);
	}
	platform_set_drvdata(pdev, priv);
	return 0;
}

static int remove(struct platform_device *pdev)
{
	mml_sys_destroy(pdev, platform_get_drvdata(pdev), &mml_comp_ops);
	return 0;
}

static const struct mml_data mt6893_mml_data = {
	.comp_inits = {
		[MML_CT_SYS] = &sys_comp_init,
		[MML_CT_DL_IN] = &dl_comp_init,
	},
	.aid_sel = sys_config_aid_sel,
	.gpr = {CMDQ_GPR_R08, CMDQ_GPR_R10},
};

static const struct mml_data mt6983_mml_data = {
	.comp_inits = {
		[MML_CT_SYS] = &sys_comp_init,
		[MML_CT_DL_IN] = &dli_comp_init,
		[MML_CT_DL_OUT] = &dlo_comp_init,
	},
	.ddp_comp_funcs = {
		[MML_CT_SYS] = &sys_ddp_funcs,
		[MML_CT_DL_IN] = &dl_ddp_funcs,
		[MML_CT_DL_OUT] = &dl_ddp_funcs,
	},
	.aid_sel = sys_config_aid_sel,
	.gpr = {CMDQ_GPR_R08, CMDQ_GPR_R10},
};

static const struct mml_data mt6879_mml_data = {
	.comp_inits = {
		[MML_CT_SYS] = &sys_comp_init,
		[MML_CT_DL_IN] = &dli_comp_init,
		[MML_CT_DL_OUT] = &dlo_comp_init,
	},
	.aid_sel = sys_config_aid_sel,
	.gpr = {CMDQ_GPR_R08, CMDQ_GPR_R10},
};

static const struct mml_data mt6985_mml_data = {
	.comp_inits = {
		[MML_CT_SYS] = &sys_comp_init,
		[MML_CT_DL_IN] = &dli_comp_init,
		[MML_CT_DL_OUT] = &dlo_comp_init,
	},
	.ddp_comp_funcs = {
		[MML_CT_SYS] = &sys_ddp_funcs,
		[MML_CT_DL_IN] = &dl_ddp_funcs,
		[MML_CT_DL_OUT] = &dl_ddp_funcs,
	},
	.aid_sel = sys_config_aid_sel_engine,
	.gpr = {CMDQ_GPR_R08, CMDQ_GPR_R10},
	.aidsel_mode = MML_AIDSEL_ENGINE,
	.set_mml_uid = true,
};

static const struct mml_data mt6897_mml_data = {
	.comp_inits = {
		[MML_CT_SYS] = &sys_comp_init,
		[MML_CT_DL_IN] = &dli_comp_init,
		[MML_CT_DL_OUT] = &dlo_comp_init,
	},
	.ddp_comp_funcs = {
		[MML_CT_SYS] = &sys_ddp_funcs,
		[MML_CT_DL_IN] = &dl_ddp_funcs,
		[MML_CT_DL_OUT] = &dl_ddp_funcs,
	},
	.aid_sel = sys_config_aid_sel_engine,
	.gpr = {CMDQ_GPR_R08, CMDQ_GPR_R10},
	.aidsel_mode = MML_AIDSEL_ENGINE,
};

static const struct mml_data mt6989_mml_data = {
	.comp_inits = {
		[MML_CT_SYS] = &sys_comp_init,
		[MML_CT_DL_IN] = &dli_comp_init,
		[MML_CT_DL_OUT] = &dlo_comp_init,
	},
	.ddp_comp_funcs = {
		[MML_CT_SYS] = &sys_ddp_funcs,
		[MML_CT_DL_IN] = &dl_ddp_funcs,
		[MML_CT_DL_OUT] = &dl_ddp_funcs,
	},
	.aid_sel = sys_config_aid_sel_bits,
	.gpr = {CMDQ_GPR_R08, CMDQ_GPR_R10},
	.px_per_tick = 2,
	.aidsel_mode = MML_AIDSEL_ENGINEBITS,
	.pw_mminfra = true,
};

const struct of_device_id mtk_mml_of_ids[] = {
	{
		.compatible = "mediatek,mt6983-mml",
		.data = &mt6983_mml_data,
	},
	{
		.compatible = "mediatek,mt6893-mml",
		.data = &mt6893_mml_data,
	},
	{
		.compatible = "mediatek,mt6879-mml",
		.data = &mt6879_mml_data,
	},
	{
		.compatible = "mediatek,mt6895-mml",
		.data = &mt6983_mml_data,
	},
	{
		.compatible = "mediatek,mt6985-mml",
		.data = &mt6985_mml_data,
	},
	{
		.compatible = "mediatek,mt6886-mml",
		.data = &mt6897_mml_data,
	},
	{
		.compatible = "mediatek,mt6897-mml",
		.data = &mt6897_mml_data,
	},
	{
		.compatible = "mediatek,mt6989-mml",
		.data = &mt6989_mml_data,
	},
	{
		.compatible = "mediatek,mt6878-mml",
		.data = &mt6879_mml_data,
	},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_mml_of_ids);

/* Used in platform with more than one mml_sys */
static const struct of_device_id mml_sys_of_ids[] = {
	{
		.compatible = "mediatek,mt6893-mml_sys",
		.data = &mt6893_mml_data,
	},
	{},
};
MODULE_DEVICE_TABLE(of, mml_sys_of_ids);

struct platform_driver mml_sys_driver = {
	.probe = probe,
	.remove = remove,
	.driver = {
		.name = "mediatek-mmlsys",
		.owner = THIS_MODULE,
		.of_match_table = mml_sys_of_ids,
	},
};
//module_platform_driver(mml_sys_driver);

static s32 dbg_case;
static s32 dbg_set(const char *val, const struct kernel_param *kp)
{
	s32 result;

	result = kstrtos32(val, 0, &dbg_case);
	mml_log("%s: debug_case=%d", __func__, dbg_case);

	switch (dbg_case) {
	case 0:
		mml_log("use read to dump component status");
		break;
	default:
		mml_err("invalid debug_case: %d", dbg_case);
		break;
	}
	return result;
}

static s32 dbg_get(char *buf, const struct kernel_param *kp)
{
	s32 length = 0;
	u32 i, j;

	switch (dbg_case) {
	case 0:
		length += snprintf(buf + length, PAGE_SIZE - length,
			"[%d] probed count: %d\n", dbg_case, dbg_probed_count);
		for (j = 0; j < dbg_probed_count; j++) {
			struct mml_sys *sys = dbg_probed_components[j];

			length += snprintf(buf + length, PAGE_SIZE - length,
				"  - [%d] component count: %d bound: %d ddp_bound: %d\n", j,
				sys->comp_cnt, sys->comp_bound, sys->ddp_bound);
			for (i = 0; i < sys->comp_cnt; i++) {
				struct mml_comp *comp = &sys->comps[i];
				struct mtk_ddp_comp *ddp_comp = &sys->ddp_comps[i];

				length += snprintf(buf + length, PAGE_SIZE - length,
					"    - [%d] mml comp_id: %d.%d @%llx name: %s bound: %d\n",
					i, comp->id, comp->sub_idx, comp->base_pa,
					comp->name ? comp->name : "(null)", comp->bound);
				length += snprintf(buf + length, PAGE_SIZE - length,
					"    -         larb_port: %d @%llx pw: %d clk: %d\n",
					comp->larb_port, comp->larb_base,
					comp->pw_cnt, comp->clk_cnt);
				length += snprintf(buf + length, PAGE_SIZE - length,
					"    -     ddp comp_id: %d bound: %d\n",
					ddp_comp->id,
					(sys->ddp_comp_en >> i) & 0x1);
			}
		}
		break;
	default:
		mml_err("not support read for debug_case: %d", dbg_case);
		break;
	}
	buf[length] = '\0';

	return length;
}

static const struct kernel_param_ops dbg_param_ops = {
	.set = dbg_set,
	.get = dbg_get,
};
module_param_cb(sys_debug, &dbg_param_ops, NULL, 0644);
MODULE_PARM_DESC(sys_debug, "mml sys debug case");

MODULE_DESCRIPTION("MediaTek SoC display MMLSYS driver");
MODULE_AUTHOR("Ping-Hsun Wu <ping-hsun.wu@mediatek.com>");
MODULE_LICENSE("GPL v2");
