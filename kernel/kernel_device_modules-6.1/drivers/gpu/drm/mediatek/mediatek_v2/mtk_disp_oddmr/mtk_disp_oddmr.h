/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef __MTK_DISP_ODDMR_H__
#define __MTK_DISP_ODDMR_H__
#include <linux/uaccess.h>
#include <uapi/drm/mediatek_drm.h>
#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/ratelimit.h>
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#include <linux/sched.h>
#include <uapi/linux/sched/types.h>
#include "../mtk_drm_crtc.h"
#include "../mtk_drm_ddp_comp.h"
#include "../mtk_dump.h"
#include "../mtk_drm_mmp.h"
#include "../mtk_drm_gem.h"
#include "../mtk_drm_fb.h"
#include "../mtk_dsi.h"


#define OD_TABLE_MAX 4
#define DMR_TABLE_MAX 2
#define DMR_GAIN_MAX 15
#define OD_GAIN_MAX 15

#define DMR_DBV_TABLE_MAX 4
#define DMR_FPS_TABLE_MAX 4
#define DBI_GET_RAW_TYPE_FRAME_NUM (10)

enum ODDMR_STATE {
	ODDMR_INVALID = 0,
	ODDMR_LOAD_PARTS,
	ODDMR_LOAD_DONE,
	ODDMR_INIT_DONE,
	ODDMR_TABLE_UPDATING,
	ODDMR_MODE_DONE,
};
enum ODDMR_USER_CMD {
	ODDMR_CMD_OD_SET_WEIGHT,
	ODDMR_CMD_OD_ENABLE,
	ODDMR_CMD_DMR_ENABLE,
	ODDMR_CMD_OD_INIT_END,
	ODDMR_CMD_EOF_CHECK_TRIGGER,
	ODDMR_CMD_OD_TUNING_WRITE_SRAM,
	ODDMR_CMD_ODDMR_DDREN_OFF,
	ODDMR_CMD_ODDMR_REMAP_EN,
	ODDMR_CMD_ODDMR_REMAP_OFF,
	ODDMR_CMD_ODDMR_REMAP_CHG,
};
enum MTK_ODDMR_MODE_CHANGE_INDEX {
	MODE_ODDMR_NOT_DEFINED = 0,
	MODE_ODDMR_DSI_VFP = BIT(0),
	MODE_ODDMR_DSI_HFP = BIT(1),
	MODE_ODDMR_DSI_CLK = BIT(2),
	MODE_ODDMR_DSI_RES = BIT(3),
	MODE_ODDMR_DDIC = BIT(4),
	MODE_ODDMR_MSYNC20 = BIT(5),
	MODE_ODDMR_DUMMY_PKG = BIT(6),
	MODE_ODDMR_LTPO = BIT(7),
	MODE_ODDMR_MAX,
};

enum MTK_ODDMR_OD_MODE_TYPE {
	OD_MODE_TYPE_RGB444 = 0,
	OD_MODE_TYPE_RGB565 = 1,
	OD_MODE_TYPE_COMPRESS_18 = 2,
	OD_MODE_TYPE_RGB666 = 4,
	OD_MODE_TYPE_COMPRESS_12 = 5,
	OD_MODE_TYPE_RGB555 = 6,
	OD_MODE_TYPE_RGB888 = 7,
};

enum MTK_ODDMR_DMR_MODE_TYPE {
	DMR_MODE_TYPE_RGB8X8L4 = 0,
	DMR_MODE_TYPE_RGB8X8L8 = 1,
	DMR_MODE_TYPE_RGB4X4L4 = 2,
	DMR_MODE_TYPE_RGB4X4L8 = 3,
	DMR_MODE_TYPE_W2X2L4 = 4,
	DMR_MODE_TYPE_W2X2Q = 5,
	DMR_MODE_TYPE_RGB4X4Q = 6,
	DMR_MODE_TYPE_W4X4Q = 7,
	DMR_MODE_TYPE_RGB7X8Q = 8,
};


struct mtk_drm_dmr_basic_info {
	unsigned int panel_id_len;
	unsigned char panel_id[16];
	unsigned int panel_width;
	unsigned int panel_height;
	unsigned int h_num;
	unsigned int v_num;
	unsigned int catch_bit;
	unsigned int partial_update_scale_factor_h;
	unsigned int partial_update_scale_factor_v;
	unsigned int partial_update_real_frame_width;
	unsigned int partial_update_real_frame_height;
	unsigned int blank_bit;
	unsigned int zero_bit;
//	unsigned int scale_factor_v; //block size
//	unsigned int real_frame_height;
};

struct mtk_drm_dmr_static_cfg {
	unsigned int reg_num;
	unsigned int *reg_offset;
	unsigned int *reg_mask;
	unsigned int *reg_value;
};

struct mtk_drm_dmr_table_index {
	unsigned int DBV_table_num;
	unsigned int FPS_table_num;
	unsigned int table_byte_num;
	unsigned int DC_table_flag;
	unsigned int *DBV_table_idx; // 0, 2048
	unsigned int *FPS_table_idx; // 0, 60

};

struct mtk_drm_dmr_table_content {
	unsigned char *table_single;     // table_single[dbv0][fps0][0~table_bit_num]
	unsigned char *table_single_DC;

	unsigned char *table_L_single;
	unsigned char *table_L_single_DC;
	unsigned char *table_R_single;
	unsigned char *table_R_single_DC;
};

struct mtk_drm_dmr_fps_dbv_node {
	unsigned int DBV_num;
	unsigned int FPS_num;
	unsigned int DC_flag;
	unsigned int remap_gain_address;
	unsigned int remap_gain_mask;
	unsigned int remap_gain_target_code;
	unsigned int remap_reduce_offset_num;
	unsigned int remap_dbv_gain_num;
	unsigned int *DBV_node; // 0, 1024, 2048, 4095
	unsigned int *FPS_node; // 0, 30, 60, 120
	unsigned int *remap_reduce_offset_node;
	unsigned int *remap_reduce_offset_value;
	unsigned int *remap_dbv_gain_node;
	unsigned int *remap_dbv_gain_value;

};

struct mtk_drm_dmr_fps_dbv_change_cfg {
	unsigned int reg_num;
	unsigned int *reg_offset;
	unsigned int *reg_mask;
	unsigned int reg_total_count;
	unsigned int *reg_value; // 3D changed_reg[DBV_ind][FPS_ind],
	unsigned int reg_DC_total_count;
	unsigned int *reg_DC_value; // 3D changed_reg_DC[DBV_ind][FPS_ind],
};

struct mtk_drm_oddmr_dbv_node {
	unsigned int DBV_num;
	unsigned int *DBV_node; // 0, 1024, 2048, 4095
};

struct mtk_drm_oddmr_dbv_chg_cfg {
	unsigned int reg_num;
	unsigned int reg_total_count;
	unsigned int *reg_offset;
	unsigned int *reg_mask;
	unsigned int *reg_value;
};

struct mtk_drm_dmr_cfg_info {
	struct mtk_drm_dmr_basic_info basic_info;
	struct mtk_drm_dmr_static_cfg static_cfg;
	struct mtk_drm_dmr_fps_dbv_node fps_dbv_node;
	struct mtk_drm_dmr_fps_dbv_change_cfg fps_dbv_change_cfg;
	struct mtk_drm_dmr_table_index table_index;
	struct mtk_drm_dmr_table_content table_content;
};

struct mtk_drm_dbi_cfg_info {
	struct mtk_drm_dmr_basic_info basic_info;
	struct mtk_drm_dmr_static_cfg static_cfg;
	struct mtk_drm_dmr_fps_dbv_node fps_dbv_node;
	struct mtk_drm_dmr_fps_dbv_change_cfg fps_dbv_change_cfg;
	struct mtk_drm_oddmr_dbv_node dbv_node;
	struct mtk_drm_oddmr_dbv_chg_cfg dbv_change_cfg;
};

struct bitstream_buffer {
	uint8_t *_buffer;
	uint32_t used_entry;
	uint32_t used_bit;
	uint32_t size;
	uint32_t read_bit;
	struct mtk_drm_dmr_static_cfg static_cfg;
	struct mtk_drm_dmr_fps_dbv_change_cfg fps_dbv_change_cfg;
};

struct mtk_disp_oddmr_data {
	bool need_bypass_shadow;
	/* dujac not support update od table */
	bool is_od_support_table_update;
	bool is_support_rtff;
	bool is_od_support_hw_skip_first_frame;
	bool is_od_need_crop_garbage;
	bool is_od_need_force_clk;
	bool is_od_support_sec;
	bool is_od_merge_lines;
	bool is_od_4_table;
	int tile_overhead;
	uint32_t dmr_buffer_size;
	uint32_t odr_buffer_size;
	uint32_t odw_buffer_size;
	/*p_num: 1tNp, pixel num*/
	uint32_t p_num;
	irqreturn_t (*irq_handler)(int irq, void *dev_id);
};

struct mtk_disp_oddmr_od_data {
	uint32_t ln_offset;
	uint32_t merge_lines;
	int od_sram_read_sel;
	uint32_t od_dram_sel[2];
	int od_sram_table_idx[2];
	/* TODO: sram 0,1 fixed pkg, need support sram1 update */
	/* od_sram_pkgs[a][b]
	 *	a:which table for dram
	 *	b:this table save in which sram
	 */
	struct cmdq_pkt *od_sram_pkgs[4][2];
	struct mtk_drm_gem_obj *r_channel;
	struct mtk_drm_gem_obj *g_channel;
	struct mtk_drm_gem_obj *b_channel;
};

struct mtk_disp_oddmr_dbi_data {
	atomic_t cur_dbv_node;
	atomic_t cur_fps_node;
	atomic_t cur_table_idx;
	atomic_t update_table_idx;
	atomic_t update_table_done;
	atomic_t enter_scp;
	struct mtk_drm_gem_obj *dbi_table[2];
	unsigned int table_size;
	unsigned int cur_max_time;
	atomic_t max_time_set_done;
	atomic_t remap_enable;
	atomic_t gain_ratio;
};


struct mtk_disp_oddmr_dmr_data {
	atomic_t cur_dbv_node;
	atomic_t cur_fps_node;
	atomic_t cur_dbv_table_idx;
	atomic_t cur_fps_table_idx;
	struct mtk_drm_gem_obj *mura_table[DMR_DBV_TABLE_MAX][DMR_FPS_TABLE_MAX];
};

struct mtk_disp_oddmr_cfg {
	uint32_t width;
	uint32_t height;
	uint32_t comp_in_width;
	uint32_t comp_overhead;
	uint32_t total_overhead;
};
struct mtk_disp_oddmr_tile_overhead_v {
	unsigned int overhead_v;
	unsigned int comp_overhead_v;
};
struct mtk_disp_oddmr_parital_data_v {
	unsigned int dbi_y_ini;
	unsigned int dbi_udma_y_ini;
	unsigned int y_idx2_ini;
	unsigned int y_remain2_ini;
};
/**
 * struct mtk_disp_oddmr - DISP_oddmr driver structure
 * @ddp_comp - structure containing type enum and hardware resources
 */
struct mtk_disp_oddmr {
	struct mtk_ddp_comp	 ddp_comp;
	const struct mtk_disp_oddmr_data *data;
	bool is_right_pipe;
	struct mtk_disp_oddmr_od_data od_data;
	struct mtk_disp_oddmr_dmr_data dmr_data;
	struct mtk_disp_oddmr_dbi_data dbi_data;
	struct mtk_disp_oddmr_cfg cfg;
	atomic_t oddmr_clock_ref;
	int od_enable_req;
	int od_enable;
	int od_enable_last;
	int od_force_off;
	int dmr_enable_req;
	int dmr_enable;
	int dbi_enable_req;
	int dbi_enable;
	unsigned int spr_enable;
	unsigned int spr_relay;
	unsigned int spr_format;
	uint32_t qos_srt_dmrr;
	uint32_t last_qos_srt_dmrr;
	uint32_t qos_srt_dbir;
	uint32_t last_qos_srt_dbir;
	uint32_t qos_srt_odr;
	uint32_t last_qos_srt_odr;
	uint32_t qos_srt_odw;
	uint32_t last_qos_srt_odw;
	struct icc_path *qos_req_dmrr;
	struct icc_path *qos_req_dbir;
	struct icc_path *qos_req_odr;
	struct icc_path *qos_req_odw;
	struct icc_path *qos_req_dmrr_hrt;
	struct icc_path *qos_req_dbir_hrt;
	struct icc_path *qos_req_odr_hrt;
	struct icc_path *qos_req_odw_hrt;
	uint32_t irq_status;
	/* larb_cons idx */
	uint32_t larb_dmrr;
	uint32_t larb_dbir;
	uint32_t larb_odr;
	uint32_t larb_odw;
	/* only use in pipe0 */
	enum ODDMR_STATE od_state;
	enum ODDMR_STATE dmr_state;
	enum ODDMR_STATE dbi_state;
	struct mtk_drm_dmr_cfg_info dmr_cfg_info;
	struct mtk_drm_dbi_cfg_info dbi_cfg_info;
	struct mtk_drm_dbi_cfg_info dbi_cfg_info_tb1;
	uint32_t od_user_gain;
	/* workqueue */
	struct workqueue_struct *oddmr_wq;
	struct work_struct update_table_work;
	/*user pq od bypass lock*/
	uint32_t pq_od_bypass;
	struct mtk_disp_oddmr_tile_overhead_v tile_overhead_v;
	bool set_partial_update;
	unsigned int roi_height;
	atomic_t hrt_weight;
};

int mtk_drm_ioctl_oddmr_load_param(struct drm_device *dev, void *data,
		struct drm_file *file_priv);
int mtk_drm_ioctl_oddmr_ctl(struct drm_device *dev, void *data,
		struct drm_file *file_priv);
void mtk_oddmr_timing_chg(struct mtk_oddmr_timing *timing, struct cmdq_pkt *handle);
void mtk_oddmr_bl_chg(uint32_t bl_level, struct cmdq_pkt *handle);
int mtk_oddmr_hrt_cal_notify(int *oddmr_hrt);
void mtk_disp_oddmr_debug(const char *opt);
void mtk_oddmr_ddren(struct cmdq_pkt *cmdq_handle,
	struct drm_crtc *crtc, int en);
unsigned int check_oddmr_err_event(void);
void clear_oddmr_err_event(void);
void mtk_oddmr_scp_status(bool enable);

void mtk_oddmr_update_larb_hrt_state(void);

#endif
