/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _DCAM_DRV_8830_H_
#define _DCAM_DRV_8830_H_

#include <linux/types.h>
#include "dcam_reg.h"
#include "parse_hwinfo.h"

//#define DCAM_DEBUG

#ifdef DCAM_DEBUG
	#define DCAM_TRACE             printk
#else
	#define DCAM_TRACE             pr_debug
#endif

#define DCAM_WAIT_FOREVER                        0xFFFFFFFF
#define DCAM_PATH_1_FRM_CNT_MAX                  8
#define DCAM_PATH_2_FRM_CNT_MAX                  4
#define DCAM_PATH_0_FRM_CNT_MAX                  8
#define DCAM_FRM_CNT_MAX                         8  /* max between path_1_frm_cnt and path_2_frm_cnt */
#define DCAM_HEIGHT_MIN                          4
#define DCAM_JPEG_LENGTH_MIN                     30720 /*640X480  div 10*/

enum dcam_swtich_status {
	DCAM_SWITCH_IDLE = 0,
	DCAM_SWITCH_PAUSE,
	DCAM_SWITCH_DONE,
	DCAM_SWITCH_MAX
};

enum dcam_drv_rtn {
	DCAM_RTN_SUCCESS = 0,
	DCAM_RTN_PARA_ERR = 0x10,
	DCAM_RTN_IO_ID_ERR,
	DCAM_RTN_ISR_ID_ERR,
	DCAM_RTN_MASTER_SEL_ERR,
	DCAM_RTN_MODE_ERR,
	DCAM_RTN_TIMEOUT,

	DCAM_RTN_CAP_FRAME_SEL_ERR = 0x20,
	DCAM_RTN_CAP_IN_FORMAT_ERR,
	DCAM_RTN_CAP_IN_BITS_ERR,
	DCAM_RTN_CAP_IN_YUV_ERR,
	DCAM_RTN_CAP_SYNC_POL_ERR,
	DCAM_RTN_CAP_SKIP_FRAME_ERR,
	DCAM_RTN_CAP_FRAME_DECI_ERR,
	DCAM_RTN_CAP_XY_DECI_ERR,
	DCAM_RTN_CAP_FRAME_SIZE_ERR,
	DCAM_RTN_CAP_SENSOR_MODE_ERR,
	DCAM_RTN_CAP_JPEG_BUF_LEN_ERR,
	DCAM_RTN_CAP_IF_MODE_ERR,

	DCAM_RTN_PATH_SRC_SIZE_ERR = 0x30,
	DCAM_RTN_PATH_TRIM_SIZE_ERR,
	DCAM_RTN_PATH_DES_SIZE_ERR,
	DCAM_RTN_PATH_IN_FMT_ERR,
	DCAM_RTN_PATH_OUT_FMT_ERR,
	DCAM_RTN_PATH_SC_ERR,
	DCAM_RTN_PATH_SUB_SAMPLE_ERR,
	DCAM_RTN_PATH_ADDR_ERR,
	DCAM_RTN_PATH_FRAME_TOO_MANY,
	DCAM_RTN_PATH_FRAME_LOCKED,
	DCAM_RTN_PATH_NO_MEM,
	DCAM_RTN_PATH_GEN_COEFF_ERR,
	DCAM_RTN_PATH_SRC_ERR,
	DCAM_RTN_PATH_ENDIAN_ERR,
	DCAM_RTN_PATH_FRM_DECI_ERR,
	DCAM_RTN_MAX
};

enum dcam_rst_mode {
	DCAM_RST_ALL = 0,
	DCAM_RST_PATH0,
	DCAM_RST_PATH1,
	DCAM_RST_PATH2
};

enum dcam_path_index {
	DCAM_PATH_IDX_NONE = 0,
	DCAM_PATH_IDX_0 = 1,
	DCAM_PATH_IDX_1 = 2,
	DCAM_PATH_IDX_2 = 4,
	DCAM_PATH_IDX_ALL  = 7,
};


enum dcam_fmt {
	DCAM_YUV422 = 0,
	DCAM_YUV420,
	DCAM_YUV420_3FRAME,
	DCAM_YUV400,
	DCAM_RGB565,
	DCAM_RGB888,
	DCAM_RAWRGB,
	DCAM_JPEG,
	DCAM_FTM_MAX
};

enum dcam_cap_pattern {
	DCAM_YUYV  = 0,
	DCAM_YVYU,
	DCAM_UYVY,
	DCAM_VYUY,
	DCAM_PATTERN_MAX
};

enum dcam_capture_mode {
	DCAM_CAPTURE_MODE_SINGLE = 0,
	DCAM_CAPTURE_MODE_MULTIPLE,
	DCAM_CAPTURE_MODE_MAX
};


enum dcam_irq_id {
	DCAM_SN_SOF = 0,
	DCAM_SN_EOF,
	DCAM_CAP_SOF,
	DCAM_CAP_EOF,
	DCAM_PATH0_DONE,
	DCAM_PATH0_OV,
	DCAM_PATH1_DONE,
	DCAM_PATH1_OV,
	DCAM_PATH2_DONE,
	DCAM_PATH2_OV,
	DCAM_SN_LINE_ERR,
	DCAM_SN_FRAME_ERR,
	DCAM_JPEG_BUF_OV,
	DCAM_ISP_OV,
	DCAM_MIPI_OV,
	DCAM_ROT_DONE,
	DCAM_PATH1_SLICE_DONE,
	DCAM_PATH2_SLICE_DONE,
	DCAM_RAW_SLICE_DONE,
	DCAM_IRQ_NUMBER
};

enum dcam_cfg_id {
	DCAM_CAP_SYNC_POL = 0,
	DCAM_CAP_DATA_BITS,
	DCAM_CAP_DATA_PACKET,
	DCAM_CAP_YUV_TYPE,
	DCAM_CAP_PRE_SKIP_CNT,
	DCAM_CAP_FRM_DECI,
	DCAM_CAP_FRM_COUNT_CLR,
	DCAM_CAP_FRM_COUNT_GET,
	DCAM_CAP_INPUT_RECT,
	DCAM_CAP_IMAGE_XY_DECI,
	DCAM_CAP_JPEG_GET_LENGTH,
	DCAM_CAP_JPEG_SET_BUF_LEN,
	DCAM_CAP_TO_ISP,
	DCAM_CAP_SAMPLE_MODE,

	DCAM_PATH_INPUT_SIZE,
	DCAM_PATH_INPUT_RECT,
	DCAM_PATH_INPUT_ADDR,
	DCAM_PATH_OUTPUT_SIZE,
	DCAM_PATH_OUTPUT_FORMAT,
	DCAM_PATH_OUTPUT_ADDR,
	DCAM_PATH_FRAME_BASE_ID,
	DCAM_PATH_SWAP_BUFF,
	DCAM_PATH_SUB_SAMPLE_EN_X,
	DCAM_PATH_SUB_SAMPLE_EN_Y,
	DCAM_PATH_SUB_SAMPLE_MOD,
	DCAM_PATH_SLICE_SCALE_EN,
	DCAM_PATH_SLICE_SCALE_HEIGHT,
	DCAM_PATH_DITHER_EN,
	DCAM_PATH_IS_IN_SCALE_RANGE,
	DCAM_PATH_IS_SCALE_EN,
	DCAM_PATH_SLICE_OUT_HEIGHT,
	DCAM_PATH_DATA_ENDIAN,
	DCAM_PATH_SRC_SEL,
	DCAM_PATH_ENABLE,
	DCAM_PATH_FRAME_TYPE,
	DCAM_PATH_ROT_MODE,
	DCAM_PATH_FRM_DECI,
	DCAM_CFG_ID_E_MAX
};

/*
enum dcam_sub_sample_mode
{
	DCAM_SUB_2  = 0,
	DCAM_SUB_4,
	DCAM_SUB_8,
	DCAM_SUB_16,
	DCAM_SUB_MAX
};

enum dcam_ahb_frm
{
	DCAM_AHB_FRAME_SRC,
	DCAM_AHB_FRAME_PATH1_DST,
	DCAM_AHB_FRAME_PATH2_DST,
	DCAM_AHB_FRAME_SWAP,
	DCAM_AHB_FRAME_LINE,
	DCAM_AHB_FRAME_MAX
};
*/
enum iram_owner {
	IRAM_FOR_DCAM = 0,
	IRAM_FOR_ARM
};

enum dcam_clk_sel {
	DCAM_CLK_256M = 0,
	DCAM_CLK_128M,
	DCAM_CLK_76M8,
	DCAM_CLK_48M,
	DCAM_CLK_NONE
};

enum dcam_cap_if_mode {
	DCAM_CAP_IF_CCIR = 0,
	DCAM_CAP_IF_CSI2,
	DCAM_CAP_IF_MODE_MAX
};

enum dcam_path_src_sel {
	DCAM_PATH_FROM_CAP = 0,
	DCAM_PATH_FROM_ISP,
	DCAM_PATH_FROM_NONE
};

enum dcam_cap_sensor_mode {
	DCAM_CAP_MODE_YUV = 0,
	DCAM_CAP_MODE_SPI,
	DCAM_CAP_MODE_JPEG,
	DCAM_CAP_MODE_RAWRGB,
	DCAM_CAP_MODE_MAX
};

enum dcam_cap_data_bits {
	DCAM_CAP_12_BITS = 12,
	DCAM_CAP_10_BITS = 10,
	DCAM_CAP_8_BITS = 8,
	DCAM_CAP_4_BITS = 4,
	DCAM_CAP_2_BITS = 2,
	DCAM_CAP_1_BITS = 1,
	DCAM_CAP_BITS_MAX = 0xFF
};

enum dcam_data_endian {
	DCAM_ENDIAN_BIG = 0,
	DCAM_ENDIAN_LITTLE,
	DCAM_ENDIAN_HALFBIG,
	DCAM_ENDIAN_HALFLITTLE,
	DCAM_ENDIAN_MAX
};

enum dcam_output_mode {
	DCAM_OUTPUT_WORD = 0,
	DCAM_OUTPUT_HALF_WORD,
};

enum dcam_glb_reg_id {
	DCAM_CFG_REG = 0,
	DCAM_CONTROL_REG,
	DCAM_INIT_MASK_REG,
	DCAM_INIT_CLR_REG,
	DCAM_AHBM_STS_REG,
	DCAM_ENDIAN_REG,
	DCAM_REG_MAX
};

enum dcam_v4l2_wtite_cmd_id {
	DCAM_V4L2_WRITE_STOP = 0x5AA5,
	DCAM_V4L2_WRITE_FREE_FRAME = 0xA55A,
	DCAM_V4L2_WRITE_MAX
};

struct dcam_cap_sync_pol {
	uint8_t               vsync_pol;
	uint8_t               hsync_pol;
	uint8_t               pclk_pol;
	uint8_t               need_href;
	uint8_t               pclk_src;
	uint8_t               reserved[3];
};

struct dcam_endian_sel {
	uint8_t               y_endian;
	uint8_t               uv_endian;
	uint8_t               reserved0;
	uint8_t               reserved1;
};

struct dcam_cap_dec {
	uint8_t                x_factor;
	uint8_t                y_factor;
	uint8_t                x_mode;
	uint8_t                reserved;
};

struct dcam_path_dec {
	uint8_t                x_factor;
	uint8_t                y_factor;
	uint8_t                reserved[2];
};

struct dcam_size {
	uint32_t               w;
	uint32_t               h;
};

struct dcam_rect {
	uint32_t               x;
	uint32_t               y;
	uint32_t               w;
	uint32_t               h;
};

struct dcam_addr {
	uint32_t               yaddr;
	uint32_t               uaddr;
	uint32_t               vaddr;
};

struct dcam_sc_tap {
	uint32_t               y_tap;
	uint32_t               uv_tap;
};

struct dcam_deci {
	uint32_t               deci_x_en;
	uint32_t               deci_x;
	uint32_t               deci_y_en;
	uint32_t               deci_y;
};

struct dcam_frame {
	uint32_t               type;
	uint32_t               lock;
	uint32_t               flags;
	uint32_t               fid;
	uint32_t               width;
	uint32_t               height;
	uint32_t               yaddr;
	uint32_t               uaddr;
	uint32_t               vaddr;
	struct dcam_frame      *prev;
	struct dcam_frame      *next;
};

typedef int (*dcam_isr_func)(struct dcam_frame* frame, void* u_data);

int32_t    dcam_module_init(enum dcam_cap_if_mode if_mode,
			enum dcam_cap_sensor_mode sn_mode);
int32_t    dcam_module_deinit(enum dcam_cap_if_mode if_mode,
			enum dcam_cap_sensor_mode sn_mode);
int32_t    dcam_module_en(struct device_node *dn);
int32_t    dcam_module_dis(struct device_node *dn);
int32_t    dcam_mipi_clk_en(struct device_node *dn);
int32_t    dcam_mipi_clk_dis(struct device_node *dn);
int32_t    dcam_ccir_clk_en(void);
int32_t    dcam_ccir_clk_dis(void);
int32_t    dcam_reset(enum dcam_rst_mode reset_mode, uint32_t is_isr);
int32_t    dcam_set_clk(struct device_node *dn, enum dcam_clk_sel clk_sel);
int32_t    dcam_update_path(enum dcam_path_index path_index, struct dcam_size *in_size,
			struct dcam_rect *in_rect, struct dcam_size *out_size);
int32_t    dcam_start_path(enum dcam_path_index path_index);
int32_t    dcam_start(void);
int32_t    dcam_stop_path(enum dcam_path_index path_index);
int32_t    dcam_stop(void);
int32_t    dcam_resume(void);
int32_t    dcam_pause(void);
int32_t    dcam_reg_isr(enum dcam_irq_id id, dcam_isr_func user_func, void* u_data);
int32_t    dcam_cap_cfg(enum dcam_cfg_id id, void *param);
int32_t    dcam_cap_get_info(enum dcam_cfg_id id, void *param);
int32_t    dcam_path0_cfg(enum dcam_cfg_id id, void *param);
int32_t    dcam_path1_cfg(enum dcam_cfg_id id, void *param);
int32_t    dcam_path2_cfg(enum dcam_cfg_id id, void *param);
int32_t    dcam_get_resizer(uint32_t wait_opt);
int32_t    dcam_rel_resizer(void);
void       dcam_int_en(void);
void       dcam_int_dis(void);
int32_t    dcam_frame_is_locked(struct dcam_frame *frame);
int32_t    dcam_frame_lock(struct dcam_frame *frame);
int32_t    dcam_frame_unlock(struct dcam_frame *frame);
int32_t    dcam_read_registers(uint32_t* reg_buf, uint32_t *buf_len);
int32_t    dcam_resize_start(void);
int32_t    dcam_resize_end(void);
int32_t    dcam_stop_cap(void);
void       dcam_glb_reg_awr(uint32_t addr, uint32_t val, uint32_t reg_id);
void       dcam_glb_reg_owr(uint32_t addr, uint32_t val, uint32_t reg_id);
void       dcam_glb_reg_mwr(uint32_t addr, uint32_t mask, uint32_t val, uint32_t reg_id);
int        dcam_scale_coeff_alloc(void);
void       dcam_scale_coeff_free(void);
int32_t    dcam_rotation_start(void);
int32_t    dcam_rotation_end(void);

#endif //_DCAM_DRV_8830_H_
