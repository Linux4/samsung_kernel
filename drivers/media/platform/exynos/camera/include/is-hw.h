/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_HW_H
#define IS_HW_H

#include <linux/phy/phy.h>
#include <linux/interrupt.h>

#include "is-type.h"
#include "is-hw-chain.h"
#include "pablo-framemgr.h"
#include "is-device-ischain.h"
#include "is-hw-control.h"
#include "is-config.h"
#include "is-groupmgr.h"
#include "is-interface.h"
#include "exynos-is-sensor.h"
#include "is-err.h"
#include "is-hw-phy.h"
#include "pablo-hw-chain-info.h"
#include "pablo-mmio.h"
#include "is-hw-dm.h"
#include "pablo-hw-api-common.h"
#include "pablo-debug.h"

struct pablo_crta_buf_info;
struct is_core;

#define IS_CSIS_VERSION(a, b, c, d) \
	(((a) << 24) + ((b) << 16) + ((c) << 8) + (d))

#define F_ID_SIZE		4	/* for frame id decoder: 4 bit */
#define CHECK_ID_60FPS(id)	((((id) >> 0) & 0xF) == 1 ||	\
				(((id) >>  4) & 0xF) == 1 ||	\
				(((id) >>  8) & 0xF) == 1 ||	\
				(((id) >> 12) & 0xF) == 1 ||	\
				(((id) >> 16) & 0xF) == 1 ||	\
				(((id) >> 20) & 0xF) == 1 ||	\
				(((id) >> 24) & 0xF) == 1 ||	\
				(((id) >> 28) & 0xF) == 1)
#define MAX_NUM_CSIS_OTF_CH	4

#define SENSOR_DUMMY_ELEMS	2

#define CSIS_PPC_8		8
#define CSIS_PPC_4		4
#define CSIS_PPC_2		2
#define CSIS_PPC_1		1

/*
 * Get each lane speed (Mbps)
 * w : width, h : height, fps : framerate
 * bit : bit width per pixel, lanes : total lane number (1 ~ 4)
 * margin : S/W margin (15 means 15%)
 */
#define CSI_GET_LANE_SPEED(w, h, fps, bit, lanes, margin) \
	({u64 tmp; tmp = ((u64)w) * ((u64)h) * ((u64)fps) * ((u64)bit) / (lanes); \
	  tmp *= (100 + margin); tmp /= 100; tmp /= 1000000; (u32)tmp;})

/*
 * Get binning ratio.
 * The ratio is expressed by 0.5 step(500)
 * The improtant thing is that it was round off the ratio to the closet 500 unit.
 */
#define BINNING(x, y) (rounddown((x) * 1000 / (y), 500))

/*
 * Get size ratio.
 * w : width, h : height
 * The return value is defined of the enum ratio_size.
 */
#define SIZE_RATIO(w, h) ((w) * 10 / (h))

#define DEBUG_EXT_MAX			2

#define DUMP_SPLIT_MAX			10
#define IS_MAX_SCENARIO             (128)
#define IS_MAX_SETFILE              (128)
/*
 * This enum will be used in order to know the size ratio based upon RATIO mecro return value.
 */
enum ratio_size {
	RATIO_1_1		= 1,
	RATIO_11_9		= 12,
	RATIO_4_3		= 13,
	RATIO_3_2		= 15,
	RATIO_5_3		= 16,
	RATIO_16_9		= 17,
};

/*
 * This enum will be used for masking each interrupt masking.
 * The irq_ids params which masked by shifting this bit(id)
 * was sended to flite_hw_s_irq_msk.
 */
enum flite_hw_irq_id {
	FLITE_MASK_IRQ_START		= 0,
	FLITE_MASK_IRQ_END		= 1,
	FLITE_MASK_IRQ_OVERFLOW		= 2,
	FLITE_MASK_IRQ_LAST_CAPTURE	= 3,
	FLITE_MASK_IRQ_LINE		= 4,
	FLITE_MASK_IRQ_ALL,
};

/*
 * This enum will be used for current status by reading interrupt source.
 */
enum flite_hw_status_id {
	FLITE_STATUS_IRQ_SRC_START		= 0,
	FLITE_STATUS_IRQ_SRC_END		= 1,
	FLITE_STATUS_IRQ_SRC_OVERFLOW		= 2,
	FLITE_STATUS_IRQ_SRC_LAST_CAPTURE	= 3,
	FLITE_STATUS_OFY			= 4,
	FLITE_STATUS_OFCR			= 5,
	FLITE_STATUS_OFCB			= 6,
	FLITE_STATUS_MIPI_VALID			= 7,
	FLITE_STATUS_IRQ_SRC_LINE		= 8,
	FLITE_STATUS_ALL,
};

/*
 * This enum will be used in flite_hw_s_control api to set specific functions.
 */
enum flite_hw_control_id {
	FLITE_CTRL_TEST_PATTERN,
	FLITE_CTRL_LINE_RATIO,
};

/*
 * ******************
 * MIPI-PHY H/W APIS
 * ******************
 */
/* CAUTION: This struct must be same as struct in 'phy-exynos-mipi.c'. */
struct mipi_phy_desc {
	struct phy *phy;
	struct exynos_mipi_phy_data *data;
	unsigned int index;
	unsigned int iso_offset;
	unsigned int pg_offset;
	unsigned int rst_bit;
	void __iomem *regs; /* clock */
	void __iomem *regs_lane; /* lane */
};

int csi_hw_s_phy_set(struct phy *phy, u32 lanes, u32 mipi_speed,
		u32 settle, u32 instance, u32 use_cphy,
		struct phy_setfile_table *sf_table,
		void __iomem *phy_reg, void __iomem *csi_reg);
u32 csi_hw_g_mapped_phy_port(u32 csi_ch);

/*
 * ************************************
 * ISCHAIN AND CAMIF CONFIGURE H/W APIS
 * ************************************
 */
/*
 * It's for hw version
 */
#define HW_SET_VERSION(first, second, third, fourth) \
	(((first) << 24) | ((second) << 16) | ((third) << 8) | ((fourth) << 0))

/*
 * This enum will be used in is_hw_s_ctrl api.
 */
enum hw_s_ctrl_id {
	HW_S_CTRL_FULL_BYPASS,
	HW_S_CTRL_CHAIN_IRQ,
	HW_S_CTRL_HWFC_IDX_RESET,
	HW_S_CTRL_MCSC_SET_INPUT,
};

/*
 * This enum will be used in is_hw_g_ctrl api.
 */
enum mcsc_cap_enum {
	MCSC_CAP_NOT_SUPPORT = 0,
	MCSC_CAP_SUPPORT,
};

/**
 * struct is_hw_mcsc_cap - capability of mcsc
 *  This Structure specified the spec of mcsc.
 * @hw_ver: type is hexa. eg. 1.22.0 -> 0b0001_0022_0000_0000
 * @max_output: the number of output port to support
 * <below fields has the value in enum mcsc_cap_enum>
 * @in_otf: capability of input otf
 * @in_dma: capability of input dma
 * @hw_fc: capability of hardware flow control
 * @out_otf: capability of output otf
 * @out_dma: capability of output dma
 * @out_hwfc: capability of output dma (each output)
 * @tdnr: capability of 3DNR feature
 */
struct is_hw_mcsc_cap {
	u32			hw_ver;
	u32			max_output;
	u32			max_djag;
	u32			max_cac;
	u32			max_uvsp;
	enum mcsc_cap_enum	in_otf;
	enum mcsc_cap_enum	in_dma;
	enum mcsc_cap_enum	hwfc;
	enum mcsc_cap_enum	out_otf[MCSC_OUTPUT_MAX];
	enum mcsc_cap_enum	out_dma[MCSC_OUTPUT_MAX];
	enum mcsc_cap_enum	out_hwfc[MCSC_OUTPUT_MAX];
	enum mcsc_cap_enum	out_post[MCSC_OUTPUT_MAX];
	bool			enable_shared_output;
	enum mcsc_cap_enum	tdnr;
	enum mcsc_cap_enum	djag;
	enum mcsc_cap_enum	cac;
	enum mcsc_cap_enum	uvsp;
	enum mcsc_cap_enum	ysum;
	enum mcsc_cap_enum	ds_vra;
};

enum is_shot_type {
	SHOT_TYPE_INTERNAL = 1,
	SHOT_TYPE_EXTERNAL,
	SHOT_TYPE_LATE,
	SHOT_TYPE_MULTI,
	SHOT_TYPE_END
};

/**
 * enum is_hw_state - the definition of HW state
 * @HW_OPEN : setbit @ open / clearbit @ close
 *            the upper layer is going to use this HW IP
 *            initialize frame manager for output or input frame control
 *            set by open stage and cleared by close stage
 *            multiple open is permitted but initialization is done
 *            at the first open only
 * @HW_INIT : setbit @ init / clearbit @ close
 *            define HW path at each instance
 *            the HW prepares context for this instance
 *            multiple init is premitted to support multi instance
 * @HW_CONFIG : setbit @ shot / clearbit @ frame start
 *              Update configuration parameters to apply each HW settings
 *              config operation must be done at least one time to run HW
 * @HW_RUN : setbit @ frame start / clearbit @ frame end
 *           running state of each HW
 *         OPEN --> INIT --> CONFIG ---> RUN
 *         | ^      | ^^     | ^           |
 *         |_|      |_||     |_|           |
 *                     |___________________|
 */
enum is_hw_state {
	HW_OPEN,
	HW_INIT,
	HW_CONFIG,
	HW_RUN,
	HW_SUSPEND,
	HW_SUSPENDING,
	HW_TUNESET,
	HW_VRA_CH1_START,
	HW_MCS_YSUM_CFG,
	HW_MCS_DS_CFG,
	HW_OVERFLOW_RECOVERY,
	HW_PAFSTAT_RDMA_CFG,
	HW_END
};

enum lboffset_trigger {
	LBOFFSET_NONE,
	LBOFFSET_A_TO_B,
	LBOFFSET_B_TO_A,
	LBOFFSET_A_DIRECT,
	LBOFFSET_B_DIRECT,
	LBOFFSET_READ,
	LBOFFSET_MAX,
};

/* CAUTION: Same HWs must be continuously defined. */
enum is_hardware_id {
	DEV_HW_3AA0	= 1,
	DEV_HW_3AA1,
	DEV_HW_3AA2,
	DEV_HW_3AA3,
	DEV_HW_3AA_MAX = DEV_HW_3AA3,
	DEV_HW_ISP0,
	DEV_HW_ISP1,
	DEV_HW_MCSC0,
	DEV_HW_MCSC1,
	DEV_HW_VRA,
	DEV_HW_PAF0,	/* PAF RDMA */
	DEV_HW_PAF1,
	DEV_HW_PAF2,
	DEV_HW_PAF3,
	DEV_HW_PAF_MAX = DEV_HW_PAF3,
	DEV_HW_YPP, /* YUVPP */
	DEV_HW_SHRP,
	DEV_HW_LME0,
	DEV_HW_LME1,
	DEV_HW_BYRP,
	DEV_HW_RGBP,
	DEV_HW_MCFP,
	DEV_HW_ORB0,
	DEV_HW_ORB1,
	DEV_HW_DLFE,
	DEV_HW_END
};

enum corex_index {
	COREX_SETA = 0,
	COREX_SETB,
	COREX_SET_MAX,
};

enum lic_sram_index {
	LIC_OFFSET_0 = 0,
	LIC_OFFSET_1,
	LIC_OFFSET_2,
	LIC_TRIGGER_MODE,
	LIC_OFFSET_MAX,
};

enum lic_trigger_index {
	LIC_NO_TRIGGER = 0,
	LIC_CONTEXT0_TRIGGER,
	LIC_CONTEXT1_TRIGGER,
	LIC_CONTEXT2_TRIGGER,
	LIC_TRIGGER_MAX,
};

enum v_enum {
	V_BLANK = 0,
	V_VALID
};

enum cmdq_setting_mode {
	CTRL_MODE_DMA_PRELOADING = 0,
	CTRL_MODE_DMA_DIRECT,
	CTRL_MODE_COREX,
	CTRL_MODE_APB_DIRECT,
	CTRL_MODET_MAX,
};

struct cal_info {
	/* case CAL_TYPE_AF:
	 * Not implemented yet
	 */
	/* case CLA_TYPE_LSC_UVSP
	 * data[0]: lsc_center_x;
	 * data[1]: lsc_center_y;
	 * data[2]: lsc_radial_biquad_a;
	 * data[3]: lsc_radial_biquad_b;
	 * data[4] - data[15]: reserved
	 */
	u32 data[16];
};

struct hw_ip_count{
	atomic_t		fs;
	atomic_t		cl;
	atomic_t		fe;
	atomic_t		dma;
};

struct hw_ip_status {
	atomic_t		Vvalid;
	wait_queue_head_t	wait_queue;
};

struct setfile_table_entry {
	ulong addr;
	u32 size;
};

struct is_hw_ip_setfile {
	int				version;
	/* the number of setfile each sub ip has */
	u32				using_count;
	/* which subindex is used at this scenario */
	u32				index[IS_MAX_SCENARIO];
	struct setfile_table_entry	table[IS_MAX_SETFILE];
};

struct is_hw_sfr_dump_region {
	resource_size_t			start;
	resource_size_t			end;
};

enum is_reg_dump_type {
	IS_REG_DUMP_TO_ARRAY,
	IS_REG_DUMP_TO_LOG,
	IS_REG_DUMP_DMA,
};

enum corex_set {
	COREX_SET_A,
	COREX_SET_B,
	COREX_SET_C,
	COREX_SET_D,
	COREX_DIRECT,
	COREX_MAX
};

struct is_hw_iq {
	struct cr_set	*regs;
	u32		size;
	spinlock_t	slock;

	u32		fcount;
	unsigned long	state;
};

/**
 * struct is_hw - fimc-is hw data structure
 * @id: unique id to represent sub IP of IS chain
 * @state: HW state flags
 * @base_addr: Each IP mmaped registers region
 * @mcuctl_addr: MCUCTL mmaped registers region
 * @priv_info: the specific structure pointer for each HW IP
 * @group: pointer to indicate the HW IP belongs to the group
 * @region: pointer to parameter region for HW setting
 * @framemgr: pointer to frame manager to manager frame list HW handled
 * @hardware: pointer to device hardware
 * @itf: pointer to interface stucture to reply command
 * @itfc: pointer to chain interface for HW interrupt
 */
struct is_hw_ip {
	u32				id;
	char				name[IS_STR_LEN];
	bool				is_leader;
	ulong				state;
	const struct is_hw_ip_ops	*ops;
	const struct is_hardware_ops	*hw_ops;
	u32				debug_index[2];
	struct hw_debug_info		debug_info[DEBUG_FRAME_COUNT];
	struct hw_debug_info		debug_ext_info[DEBUG_EXT_MAX];
	struct hw_ip_count		count;
	struct hw_ip_status		status;
	atomic_t			fcount;
	char				*stm[IS_STREAM_COUNT];
	atomic_t			instance;
	void __iomem			*regs[REG_SET_MAX];
	resource_size_t			regs_start[REG_SET_MAX];
	resource_size_t			regs_end[REG_SET_MAX];
	void				*priv_info;
	struct is_hw_ip			*changed_hw_ip[IS_STREAM_COUNT];
	struct is_group			*group[IS_STREAM_COUNT];
	struct is_region		*region[IS_STREAM_COUNT];
	u32				internal_fcount[IS_STREAM_COUNT];
	struct is_framemgr		*framemgr;
	struct is_hardware		*hardware;
	/* callback interface */
	struct is_interface		*itf;
	/* control interface */
	struct is_interface_ischain	*itfc;
	struct is_priv_buf 		*pb_setfile[SENSOR_POSITION_MAX];
	ulong				kvaddr_setfile[SENSOR_POSITION_MAX];
	struct is_hw_ip_setfile		setfile[SENSOR_POSITION_MAX];
	u32				applied_scenario;
	u32				lib_mode;
	u32				frame_type;
	/* for dump sfr */
	void				*sfr_dump[REG_SET_MAX];
	bool				sfr_dump_flag;
	struct is_hw_sfr_dump_region	dump_region[REG_SET_MAX][DUMP_SPLIT_MAX];
	struct is_reg			*dump_for_each_reg;
	u32				dump_reg_list_size;

	atomic_t			rsccount;
	unsigned long			run_rsc_state;

	struct timer_list		shot_timer;
	struct timer_list		lme_frame_start_timer;
	struct timer_list		lme_frame_end_timer;
	struct timer_list		orbmch_stuck_timer;

	/* fast read out in hardware */
	bool				hw_fro_en;

	/* multi-buffer */
	struct is_frame			*mframe; /* CAUTION: read only */
	u32				num_buffers; /* total number of buffers per frame */
	u32				cur_s_int; /* count of start interrupt in multi-buffer */
	u32				cur_e_int; /* count of end interrupt in multi-buffer */
	struct tasklet_struct		tasklet_mshot;

	/* currently used for subblock inside MCSC */
	u32				subblk_ctrl;

	struct hwip_intr_handler	*intr_handler[INTR_HWIP_MAX];

	struct is_hw_iq			iq_set[IS_STREAM_COUNT];
	struct is_hw_iq			cur_hw_iq_set[COREX_MAX];

	const struct pablo_hw_helper_ops	*help_ops;

	void __iomem			*mmio_base;

	struct pmio_config		pmio_config;
	struct pablo_mmio		*pmio;
	struct pmio_field		*pmio_fields;
	struct pmio_reg_seq		*pmio_reg_seqs;
};

#define CALL_HWIP_OPS(hw_ip, op, args...)	\
	(((hw_ip)->ops && (hw_ip)->ops->op) ? ((hw_ip)->ops->op(hw_ip, args)) : 0)

struct is_hw_ip_ops {
	int (*open)(struct is_hw_ip *hw_ip, u32 instance);
	int (*init)(struct is_hw_ip *hw_ip, u32 instance,
				bool flag, u32 frame_type);
	int (*deinit)(struct is_hw_ip *hw_ip, u32 instance);
	int (*close)(struct is_hw_ip *hw_ip, u32 instance);
	int (*enable)(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map);
	int (*disable)(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map);
	int (*shot)(struct is_hw_ip *hw_ip, struct is_frame *frame, ulong hw_map);
	int (*set_param)(struct is_hw_ip *hw_ip, struct is_region *region,
		IS_DECLARE_PMAP(pmap), u32 instance, ulong hw_map);
	int (*get_meta)(struct is_hw_ip *hw_ip, struct is_frame *frame,
		ulong hw_map);
	int (*get_cap_meta)(struct is_hw_ip *hw_ip, ulong hw_map, u32 instance,
		u32 fcount, u32 size, ulong addr);
	int (*frame_ndone)(struct is_hw_ip *hw_ip, struct is_frame *frame,
			enum ShotErrorType done_type);
	int (*load_setfile)(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map);
	int (*apply_setfile)(struct is_hw_ip *hw_ip, u32 scenario, u32 instance,
		ulong hw_map);
	int (*delete_setfile)(struct is_hw_ip *hw_ip, u32 instance, ulong hw_map);
	void (*size_dump)(struct is_hw_ip *hw_ip);
	int (*restore)(struct is_hw_ip *hw_ip, u32 instance);
	int (*sensor_start)(struct is_hw_ip *hw_ip, u32 instance);
	int (*sensor_stop)(struct is_hw_ip *hw_ip, u32 instance);
	int (*change_chain)(struct is_hw_ip *hw_ip, u32 instance,
		u32 next_id, struct is_hardware *hardware);
	int (*set_regs)(struct is_hw_ip *hw_ip, u32 chain_id,
		u32 instance, u32 fcount, struct cr_set *regs, u32 regs_size);
	int (*dump_regs)(struct is_hw_ip *hw_ip, u32 instance, u32 fcount, struct cr_set *regs, u32 regs_size,
		enum is_reg_dump_type dump_type);
	int (*set_config)(struct is_hw_ip *hw_ip, u32 chain_id, u32 instance, u32 fcount, void *conf);
	int (*get_offline_data)(struct is_hw_ip *hw_ip, u32 instance,
		void *data_desc, int fcount);
	int (*notify_timeout)(struct is_hw_ip *hw_ip, u32 instance);
	void (*show_status)(struct is_hw_ip *hw_ip, u32 instance);
	void (*suspend)(struct is_hw_ip *hw_ip, u32 instance);
	void (*query)(struct is_hw_ip *ip, u32 instance, u32 type, void *in, void *out);
	int (*reset)(struct is_hw_ip *hw_ip, u32 instance);
	int (*wait_idle)(struct is_hw_ip *ip, u32 instance);
};

/* query types */
enum pablo_query_type {
	PABLO_QUERY_GET_PCFI,
};

#define CALL_HW_HELPER_OPS(hw_ip, op, args...)	\
	(((hw_ip)->help_ops && (hw_ip)->help_ops->op) ? ((hw_ip)->help_ops->op(hw_ip, ##args)) : 0)

struct pablo_hw_helper_ops {
	bool (*set_rta_regs)(struct is_hw_ip *hw_ip, u32 instance, u32 set_id, bool skip,
			struct is_frame *frame, void *buf);
	int (*open)(struct is_hw_ip *hw_ip, u32 instance, void *lib,
			u32 lf_type);
	int (*close)(struct is_hw_ip *hw_ip, u32 instance, void *lib);
	int (*init)(struct is_hw_ip *hw_ip, u32 instance, void *lib,
			bool flag, u32 f_type, u32 lf_type);
	int (*deinit)(struct is_hw_ip *hw_ip, u32 instance, void *lib);
	int (*disable)(struct is_hw_ip *hw_ip, u32 instance, void *lib);
	int (*lib_shot)(struct is_hw_ip *hw_ip, u32 instance, bool skip,
			struct is_frame *frame, void *lib, void *param_set);
	int (*get_meta)(struct is_hw_ip *hw_ip, u32 instance,
			struct is_frame *frame, void *lib);
	int (*get_cap_meta)(struct is_hw_ip *hw_ip, u32 instance,
			void *lib, u32 fcount, u32 size, ulong addr);
	int (*load_cal_data)(struct is_hw_ip *hw_ip, u32 instance, void *lib);
	int (*load_setfile)(struct is_hw_ip *hw_ip, u32 instance, void *lib);
	int (*apply_setfile)(struct is_hw_ip *hw_ip, u32 instance, void *lib, u32 scenario);
	int (*delete_setfile)(struct is_hw_ip *hw_ip, u32 instance, void *lib);
	int (*restore)(struct is_hw_ip *hw_ip, u32 instance, void *lib);
	void (*free_iqset)(struct is_hw_ip *hw_ip);
	int (*alloc_iqset)(struct is_hw_ip *hw_ip, u32 reg_cnt);
	int (*set_regs)(struct is_hw_ip *hw_ip, u32 instance,
			struct cr_set *regs, u32 regs_size);
	int (*notify_timeout)(struct is_hw_ip *hw_ip, u32 instance, void *lib);
};

typedef int (*hw_ip_probe_fn_t)(struct is_hw_ip *hw_ip, struct is_interface *itf,
	struct is_interface_ischain *itfc, int id, const char *name);

#define CALL_HW_OPS(hw_ip, op, args...)	\
	(((hw_ip)->hw_ops && (hw_ip)->hw_ops->op) ? ((hw_ip)->hw_ops->op(args)) : 0)

struct is_hardware_ops {
	void (*frame_start)(struct is_hw_ip *hw_ip, u32 instance);
	int (*config_lock)(struct is_hw_ip *hw_ip, u32 instance, u32 framenum);
	int (*frame_done)(struct is_hw_ip *hw_ip, struct is_frame *frame,
		int wq_id, u32 output_id, enum ShotErrorType done_type, bool get_meta);
	int (*frame_ndone)(struct is_hw_ip *ldr_hw_ip,
		struct is_frame *frame, enum ShotErrorType done_type);
	void (*flush_frame)(struct is_hw_ip *hw_ip,
		enum is_frame_state state,
		enum ShotErrorType done_type);
	void (*dbg_trace)(struct is_hw_ip *hw_ip, u32 fcount, u32 dbg_pts);
	u32 (*dma_cfg)(char *name, struct is_hw_ip *hw_ip, struct is_frame *frame,
			int cur_idx, u32 num_buffers,
			u32 *cmd, u32 plane,
			pdma_addr_t *dst_dva, dma_addr_t *src_dva);
	void (*dma_cfg_kva64)(char *name, struct is_hw_ip *hw_ip, struct is_frame *frame,
			int cur_idx,
			u32 *cmd, u32 plane,
			u64 *dst_kva, u64 *src_kva);
};

#define CALL_HW_GRP_OPS(grp, op, args...)	\
	(((grp)->hw_grp_ops && (grp)->hw_grp_ops->op) ? ((grp)->hw_grp_ops->op(args)) : 0)

struct is_hw_group_ops {
	int (*shot)(struct is_hardware *hw, u32 instance, struct is_frame *frame);
	int (*open)(struct is_hardware *hw, u32 instance, u32 *hw_slot_id);
	int (*close)(struct is_hardware *hw, u32 instance, u32 *hw_slot_id);
	int (*stop)(struct is_hardware *hw, u32 instance, u32 mode, u32 *hw_slot_id);
	int (*s_param)(struct is_hardware *hw, u32 instance,
			struct is_group *grp, struct is_frame *frame);
};

/**
 * struct is_hardware - common HW chain structure
 * @taa0: 3AA0 HW IP structure
 * @taa1: 3AA1 HW IP structure
 * @isp0: ISP0 HW IP structure
 * @isp1: ISP1 HW IP structure
 * @fd: LHFD HW IP structure
 * @framemgr: frame manager structure. each group has its own frame manager
 */
struct is_hardware {
	struct is_hw_ip		hw_ip[HW_SLOT_MAX];
	struct is_framemgr		framemgr[DEV_HW_END];
	atomic_t			rsccount;
	struct mutex            itf_lock;
	const struct pablo_hw_chain_info_ops *chain_info_ops;

	/* keep last configuration */
	ulong				logical_hw_map[IS_STREAM_COUNT]; /* logical */
	ulong				hw_map[IS_STREAM_COUNT]; /* physical */
	u32				sensor_position[IS_STREAM_COUNT];

	struct cal_info			cal_info[SENSOR_POSITION_MAX];
	atomic_t			streaming[SENSOR_POSITION_MAX];
	atomic_t			bug_count;
	atomic_t			log_count;

	bool				video_mode;

	unsigned long			hw_recovery_flag;

	/*
	 * To deliver MCSC noise index.
	 * 0: MCSC0, 1: MCSC1
	 */
	struct camera2_ni_udm		ni_udm[2][NI_BACKUP_MAX];
	u32				lic_offset[LIC_CHAIN_NUM][LIC_CHAIN_OFFSET_NUM];
	u32				lic_offset_def[LIC_CHAIN_OFFSET_NUM];
	atomic_t			lic_updated;

	u32				ypp_internal_buf_max_width;

	atomic_t			slot_rsccount[GROUP_SLOT_MAX];

	struct pablo_icpu_adt		*icpu_adt;
	struct pablo_crta_bufmgr	*crta_bufmgr[IS_STREAM_COUNT];
};

int is_hw_group_cfg(void *group_data);
int is_hw_group_open(void *group_data);
void is_hw_camif_init(void);
void is_hw_s2mpu_cfg(void);
int is_hw_camif_cfg(void *sensor_data);
void is_hw_ischain_qe_cfg(void);
int is_hw_ischain_cfg(void *ischain_data);
int is_hw_ischain_enable(struct is_core *core);
int is_hw_ischain_disable(struct is_core *core);
int is_hw_lme_isr_clear_register(u32 hw_id, bool enable);
int is_hw_get_address(void *itfc_data, void *pdev_data, int hw_id);
int is_hw_get_irq(void *itfc_data, void *pdev_data, int hw_id);
int is_hw_request_irq(void *itfc_data, int hw_id);
int is_get_hw_list(int group_id, int *hw_list);
int is_hw_s_ctrl(void *itfc_data, int hw_id, enum hw_s_ctrl_id id, void *val);
int is_hw_query_cap(void *cap_data, int hw_id);
u32 is_hw_find_settle(u32 mipi_speed, u32 use_cphy);
#ifdef ENABLE_FULLCHAIN_OVERFLOW_RECOVERY
int is_hw_overflow_recovery(void);
#endif
void is_hw_interrupt_relay(struct is_group *group, void *hw_ip);
void is_hw_configure_llc(bool on, struct is_device_ischain *device, ulong *llc_state);
void is_hw_configure_bts_scen(struct is_resourcemgr *resourcemgr, int scenario_id);
int is_hw_get_output_slot(u32 vid);
int is_hw_get_capture_slot(struct is_frame *frame, dma_addr_t **taddr, u64 **taddr_k, u32 vid);
void * is_get_dma_blk(int type);
int is_get_dma_id(u32 vid);
void is_hw_fill_target_address(u32 hw_id, struct is_frame *dst,	struct is_frame *src,
	bool reset);
#define TADDR_COPY(dst, src, taddr, reset)				\
	do {								\
		memcpy(dst->taddr, src->taddr, sizeof(src->taddr));	\
		if (reset)						\
			memset(src->taddr, 0x0, sizeof(src->taddr));	\
	} while (0);
void is_hw_chain_probe(void *core_data);
struct is_mem *is_hw_get_iommu_mem(u32 vid);
#define IS_PRINT_TARGET_DVA(TARGET) \
	if (leader_frame->TARGET[i]) \
		pr_err("[@][%d] %s[%d]: %pad\n", \
			instance, #TARGET, i, &leader_frame->TARGET[i]);
void is_hw_print_target_dva(struct is_frame *leader_frame, u32 instance);
int is_hw_config(struct is_hw_ip *hw_ip, struct pablo_crta_buf_info *buf_info);
void is_hw_update_pcfi(struct is_hardware *hardware, struct is_group *group,
			struct is_frame *frame, struct pablo_crta_buf_info *pcfi_buf);

/*
 * ********************
 * RUNTIME-PM FUNCTIONS
 * ********************
 */
int is_sensor_runtime_suspend_pre(struct device *dev);
int is_sensor_runtime_resume_pre(struct device *dev);

int is_ischain_runtime_suspend_post(struct device *dev);
int is_ischain_runtime_resume_pre(struct device *dev);
int is_ischain_runtime_resume_post(struct device *dev);

int is_ischain_runtime_suspend(struct device *dev);
int is_ischain_runtime_resume(struct device *dev);

int is_runtime_suspend_pre(struct device *dev);
int is_runtime_suspend_post(struct device *dev);
int is_runtime_resume_pre(struct device *dev);
int is_runtime_resume_post(struct device *dev);

void is_hw_djag_get_input(struct is_device_ischain *ischain, u32 *djag_in);

#endif
