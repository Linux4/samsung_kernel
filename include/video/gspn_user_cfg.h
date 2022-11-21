/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
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

/*
define gspn configure data structure, HWC config this struct and pass to driver.
a struct present a command.
this file will share with user space
*/

#ifndef _GSPN_USER_CFG_H_
#define _GSPN_USER_CFG_H_

/*
GSP ERR CODE 0x00-0xFF
GSPN MODE1 HW ERR CODE 0x0100-0x01FF
GSPN MODE2 HW ERR CODE 0x0200-0x02FF
*/
typedef enum _GSPN_ERR_CODE_TAG_ {
	/*GSPN MODE1 HW defined err code, start*/
	GSPN_NO_ERR = 0,
	MOD1_SCL_MIN_SIZE_ERR = 0x0101,
	MOD1_SCL_OUT_RNG_ERR,
	MOD1_SCL_NOT_SAME_XY_COOR_ERR,
	MOD1_DST_FRMT_ERR,
	MOD1_LAYER0_FRMT_ERR,//0x0105
	MOD1_LAYER1_FRMT_ERR,
	MOD1_LAYER2_FRMT_ERR,
	MOD1_LAYER3_FRMT_ERR,
	MOD1_DST_R5_SWAP_ERR,
	MOD1_LAYER0_R5_SWAP_ERR,//0x010a
	MOD1_LAYER1_R5_SWAP_ERR,
	MOD1_LAYER2_R5_SWAP_ERR,
	MOD1_LAYER3_R5_SWAP_ERR,
	MOD1_LAYER0_CLP_SIZE_ZERO_ERR,
	MOD1_LAYER1_CLP_SIZE_ZERO_ERR,//0x010f
	MOD1_LAYER2_CLP_SIZE_ZERO_ERR,
	MOD1_LAYER3_CLP_SIZE_ZERO_ERR,
	MOD1_DES_PITCH_ZERO_ERR,
	MOD1_LAYER0_PITCH_ZERO_ERR,
	MOD1_LAYER1_PITCH_ZERO_ERR,//0x0114
	MOD1_LAYER2_PITCH_ZERO_ERR,
	MOD1_LAYER3_PITCH_ZERO_ERR,
	MOD1_LAYER0_CLP_SITUATION_ERR,
	MOD1_LAYER1_CLP_SITUATION_ERR,
	MOD1_LAYER2_CLP_SITUATION_ERR,//0x0119
	MOD1_LAYER3_CLP_SITUATION_ERR,
	MOD1_LAYER0_OUT_SITUATION_ERR,
	MOD1_LAYER1_OUT_SITUATION_ERR,
	MOD1_LAYER2_OUT_SITUATION_ERR,
	MOD1_LAYER3_OUT_SITUATION_ERR,//0x011e
	MOD1_ALL_LAYER_NOT_ENABLE_ERR,
	/*GSPN MODE1 HW defined err code, end*/


	/*GSPN MODE2 HW defined err code, start*/
	MOD2_SCL_MIN_SIZE_ERR = 0x0201,
	MOD2_SCL_OUT_RNG_ERR,
	MOD2_SCL_NOT_SAME_XY_COOR_ERR,
	MOD2_DST_FRMT_ERR,
	MOD2_LAYER0_FRMT_ERR,//0x0205
	MOD2_DST_R5_SWAP_ERR,
	MOD2_LAYER0_R5_SWAP_ERR,
	MOD2_DES_PITCH_ZERO_ERR = 0x0209,
	MOD2_LAYER0_PITCH_ZERO_ERR,//0x020a
	MOD2_LAYER0_CLP_SITUATION_ERR,
	MOD2_LAYER0_OUT_SITUATION_ERR,
	/*GSPN MODE2 HW defined err code, end*/

	GSPN_K_CTL_CODE_ERR,
	GSPN_K_CLK_CHK_ERR,
	GSPN_K_CLK_EN_ERR,

	GSPN_K_CREATE_FENCE_ERR, //0x210
	GSPN_K_PUT_FENCE_TO_USER_ERR,
	GSPN_K_GET_FENCE_BY_FD_ERR,
	GSPN_K_GET_DMABUF_BY_FD_ERR,
	GSPN_K_COPY_FROM_USER_ERR,
	GSPN_K_COPY_TO_USER_ERR,
	GSPN_K_NOT_ENOUGH_EMPTY_KCMD_ERR,
	GSPN_K_CREATE_THREAD_ERR,
	GSPN_K_IOMMU_MAP_ERR,
	GSPN_K_PARAM_CHK_ERR,

	GSPN_K_GET_FREE_CORE_ERR,
	GSPN_HAL_DRIVER_NOT_EXIST,// gsp driver nod not exist
	GSPN_HAL_PARAM_CHECK_ERR,
	GSPN_ERR_MAX,
} GSPN_ERR_CODE_E;

typedef enum {
	GSPN_LOG_EMERG = 0x0,
	GSPN_LOG_ALERT = 0x1,
	GSPN_LOG_CRIT = 0x2,
	GSPN_LOG_ERROR = 0x3,
	GSPN_LOG_WARNING = 0x4,
	GSPN_LOG_INFO = 0x6,
	GSPN_LOG_DEBUG = 0x7,
	GSPN_LOG_PERF= 0x8,//performance
}
GSPN_LOG_LEVEL_E;// echo to dev/sprd_gspn (1<<GSPN_LOG_xxx|1<<GSPN_LOG_xxx)


typedef enum {
	GSPN_ADDR_TYPE_INVALID,
	GSPN_ADDR_TYPE_PHYSICAL,
	GSPN_ADDR_TYPE_IOVIRTUAL,
	GSPN_ADDR_TYPE_MAX,
}
GSPN_ADDR_TYPE_E;// the address type of gsp can process


typedef enum {
	GSPN_LAYER0,
	GSPN_LAYER1,
	GSPN_LAYER2,
	GSPN_LAYER3,
	GSPN_LAYER_DST1,
	GSPN_LAYER_DST2,
	GSPN_LAYER_MAX,
}
GSPN_LAYER_ID_E;


typedef enum {
	GSPN_ROT_MODE_0 = 0x00,
	GSPN_ROT_MODE_90,
	GSPN_ROT_MODE_180,
	GSPN_ROT_MODE_270,
	GSPN_ROT_MODE_0_M,
	GSPN_ROT_MODE_90_M,
	GSPN_ROT_MODE_180_M,
	GSPN_ROT_MODE_270_M,
	GSPN_ROT_MODE_MAX_NUM,
} GSPN_ROT_MODE_E;

/*Original: 0xB3B2B1B0*/
typedef enum {
	GSPN_WORD_ENDN_0 = 0x00,	 /*0xB3B2B1B0*/
	GSPN_WORD_ENDN_1,			/*0xB0B1B2B3*/
	GSPN_WORD_ENDN_2,			/*0xB1B0B3B2*/
	GSPN_WORD_ENDN_3,			/*0xB2B3B0B1*/
	GSPN_WORD_ENDN_MAX_NUM,
} GSPN_WORD_ENDN_E;

/*Original: B7B6B5B4B 3B2B1B0*/
typedef enum {
	GSPN_DWORD_ENDN_0,		 /*B7B6B5B4 B3B2B1B0*/
	GSPN_DWORD_ENDN_1,
	GSPN_DWORD_ENDN_MAX,	   /*B3B2B1B0_B7B6B5B4*/
} GSPN_DWORD_ENDN_E;


typedef enum {
	GSPN_STD_BT601_NARROW = 0x01,
	GSPN_STD_BT601_FULL = 0x02,
	GSPN_STD_BT709_NARROW = 0x04,
	GSPN_STD_BT709_FULL = 0x08,
	GSPN_STD_BT2020_NARROW = 0x10,
	GSPN_STD_BT2020_FULL = 0x20,
} GSPN_STD_E;



typedef enum {
	GSPN_RGB_SWP_RGB = 0x00,
	GSPN_RGB_SWP_RBG,
	GSPN_RGB_SWP_GRB,
	GSPN_RGB_SWP_GBR,
	GSPN_RGB_SWP_BGR,
	GSPN_RGB_SWP_BRG,
	GSPN_RGB_SWP_MAX,
} GSPN_RGB565_SWAP_MOD_E;

typedef enum {
	GSPN_A_SWAP_ARGB,
	GSPN_A_SWAP_RGBA,
	GSPN_A_SWAP_MAX,
} GSPN_A_SWAP_MOD_E;

/*
both DST1 and DST2 use same format type
*/
typedef enum {
	GSPN_DST_FMT_ARGB888 = 0x00,
	GSPN_DST_FMT_RGB888,
	GSPN_DST_FMT_RGB565,
	GSPN_DST_FMT_YUV420_2P,
	GSPN_DST_FMT_YUV420_3P,
	GSPN_DST_FMT_YUV422_2P,
	GSPN_DST_FMT_MAX_NUM,
} GSPN_LAYER_DST_FMT_E;



typedef enum _GSPN_LAYER0_FMT_TAG_ {
	GSPN_LAYER0_FMT_ARGB888 = 0x00,
	GSPN_LAYER0_FMT_RGB888,
	GSPN_LAYER0_FMT_YUV422_2P,
	GSPN_LAYER0_FMT_YUV422_1P,
	GSPN_LAYER0_FMT_YUV420_2P,
	GSPN_LAYER0_FMT_YUV420_3P,
	GSPN_LAYER0_FMT_RGB565,
	GSPN_LAYER0_FMT_MAX_NUM,
} GSPN_LAYER0_FMT_E;


typedef enum _GSPN_LAYER1_FMT_TAG_ {
	GSPN_LAYER1_FMT_ARGB888 = 0x00,
	GSPN_LAYER1_FMT_RGB888,
	GSPN_LAYER1_FMT_RGB565,
	GSPN_LAYER1_FMT_MAX_NUM,
} GSPN_LAYER1_FMT_E;

typedef struct {
	uint32_t y_contrast;// 10bit 0~1023
	int32_t y_brightness;// 9bit -256~255
	uint32_t u_saturation;// 10bit 0~1023
	uint32_t u_offset;// 8bit 0~255
	uint32_t v_saturation;// 10bit 0~1023
	uint32_t v_offset;// 8bit 0~255
} GSPN_Y2R_PARAM_T;// 24B


typedef struct {
	GSPN_DWORD_ENDN_E		   y_dword_endn;
	GSPN_WORD_ENDN_E			y_word_endn;
	GSPN_DWORD_ENDN_E		   uv_dword_endn;// useless for layer1~3
	GSPN_WORD_ENDN_E			uv_word_endn;// useless for layer1~3
	GSPN_RGB565_SWAP_MOD_E	  rgb_swap_mod;
	GSPN_A_SWAP_MOD_E		   a_swap_mod;
}
GSPN_ENDIAN_INFO_T;// 24B

typedef struct {
	uint8_t b;
	uint8_t g;
	uint8_t r;
	uint8_t a;//if necessary
}
GSPN_ARGB_T;// 4B

typedef struct {
	uint16_t x;
	uint16_t y;
}
GSPN_POSITION_T;// 4B

typedef struct GSPN_RECT_SIZE_TAG {
	uint16_t w;
	uint16_t h;
}
GSPN_RECT_SIZE_T;// 4B

typedef struct {
	uint32_t plane_y;// ARGB or Y
	uint32_t plane_u;// UV or U
	uint32_t plane_v;// V
}
GSPN_BUF_ADDR_T;// 12B

typedef struct {
	int32_t is_pa;
	int32_t share_fd;
	uint32_t uv_offset;
	uint32_t v_offset;
	void* dmabuf;
}
GSPN_BUF_INFO_T;// 16B


typedef struct {
	uint8_t run_mod;// usually it should be 0, if set to 1, you should be ware of the action of the hw
	uint8_t scale_seq;
	uint8_t scale_en;
	uint8_t async_flag;// 1 asynchronization task, 0 sync task

	uint8_t htap4_en;
	uint8_t pmargb_en;
	uint8_t bg_en;
	uint8_t gap_rb;

	uint8_t gap_wb;
	uint8_t dither1_en;
	uint8_t dither2_en;
	uint8_t cmd_en;
	//uint32_t gsp_cmd_addr;

	uint32_t frame_id;// indicate a few cmds belong to a same frame, these cmds in memory should be like GSPN_CMD_INFO_T cmds[n] array form.
	uint8_t cmd_priority;// if two cmds have same priority means they can be executed parallel, or little number execute first
	uint8_t cmd_order;
	uint8_t cmd_cnt;// how many cmds in this frame

	uint32_t estimate_exe_time;// used by kernel, user space don't set it
} GSPN_MISC_INFO_T;// 24B


#define COMMON_REPLACE

//common part of every layer
#define LAYER_COMMON_INFO\
	GSPN_RECT_SIZE_T		   pitch;\
	GSPN_BUF_ADDR_T			addr;\
	GSPN_BUF_INFO_T		   addr_info;\
	int32_t acq_fen_fd;\
	int32_t rls_fen_fd;\
	uint8_t layer_en

typedef struct {
#ifdef COMMON_REPLACE
	LAYER_COMMON_INFO;
#else
	GSPN_RECT_SIZE_T		pitch;
	GSPN_BUF_ADDR_T			addr;
	GSPN_BUF_INFO_T			addr_info;
	int32_t acq_fen_fd;
	int32_t rls_fen_fd;
	uint8_t layer_en;
#endif
	uint8_t			compress_r8;// 1 output RGB888 or output XRGB8888
	uint8_t			r2y_mod;// 0-full;1-reduce
	GSPN_ARGB_T		bg_color;
	GSPN_ROT_MODE_E		rot_mod;
	GSPN_LAYER_DST_FMT_E	fmt;
	GSPN_ENDIAN_INFO_T	endian;
	GSPN_RECT_SIZE_T	scale_out_size;
	GSPN_POSITION_T		work_src_start;
	GSPN_RECT_SIZE_T	work_src_size;
	GSPN_POSITION_T		work_dst_start;
} GSPN_LAYER_DES1_INFO_T;// 92B


typedef struct {
#ifdef COMMON_REPLACE
	LAYER_COMMON_INFO;
#else
	GSPN_RECT_SIZE_T		pitch;
	GSPN_BUF_ADDR_T			addr;
	GSPN_BUF_INFO_T			addr_info;
	int32_t acq_fen_fd;
	int32_t rls_fen_fd;
	uint8_t layer_en;
#endif
	uint8_t				compress_r8;// 1 output RGB888 or output XRGB8888
	uint8_t				r2y_mod;// 0-full;1-reduce
	GSPN_ROT_MODE_E			rot_mod;
	GSPN_LAYER_DST_FMT_E		fmt;
	GSPN_ENDIAN_INFO_T		endian;
	GSPN_RECT_SIZE_T		scale_out_size;
} GSPN_LAYER_DES2_INFO_T;// 72B


typedef struct {
#ifdef COMMON_REPLACE
	LAYER_COMMON_INFO;
#else
	GSPN_RECT_SIZE_T		pitch;
	GSPN_BUF_ADDR_T			addr;
	GSPN_BUF_INFO_T			addr_info;
	int32_t acq_fen_fd;
	int32_t rls_fen_fd;
	uint8_t layer_en;
#endif
	uint8_t pallet_en;
	uint8_t ck_en;
	uint8_t y2r_mod;// 0:y2y+full; 1:y2y+reduce; 2:full; 3:reduce
	uint8_t pmargb_mod;// 1: is premultiply rgb signal
	uint8_t z_order;// 0 is bottom, 3 is toppest layer
	uint8_t alpha;// block alpha
	GSPN_LAYER0_FMT_E fmt;
	GSPN_ENDIAN_INFO_T	 endian;
	GSPN_POSITION_T clip_start;
	GSPN_RECT_SIZE_T clip_size;
	GSPN_POSITION_T dst_start;
	GSPN_ARGB_T pallet_color;
	GSPN_ARGB_T ck_color;
	GSPN_Y2R_PARAM_T y2r_param;
} GSPN_LAYER0_INFO_T;// 112B

typedef struct {
#ifdef COMMON_REPLACE
	LAYER_COMMON_INFO;
#else
	GSPN_RECT_SIZE_T		pitch;
	GSPN_BUF_ADDR_T			addr;
	GSPN_BUF_INFO_T			addr_info;
	int32_t acq_fen_fd;
	int32_t rls_fen_fd;
	uint8_t layer_en;
#endif
	uint8_t pallet_en;
	uint8_t ck_en;
	uint8_t pmargb_mod;// 1: is premultiply rgb signal
	uint8_t alpha;// block alpha
	GSPN_LAYER1_FMT_E		fmt;
	GSPN_ENDIAN_INFO_T		endian;
	GSPN_POSITION_T			clip_start;
	GSPN_RECT_SIZE_T		clip_size;
	GSPN_POSITION_T			dst_start;
	GSPN_ARGB_T			pallet_color;
	GSPN_ARGB_T			ck_color;
} GSPN_LAYER1_INFO_T; // 88B



typedef struct {
	GSPN_MISC_INFO_T	misc_info;
	GSPN_LAYER_DES1_INFO_T	des1_info;
	GSPN_LAYER_DES2_INFO_T	des2_info;
	GSPN_LAYER0_INFO_T	l0_info;
	GSPN_LAYER1_INFO_T	l1_info;
	GSPN_LAYER1_INFO_T	l2_info;
	GSPN_LAYER1_INFO_T	l3_info;
} GSPN_CMD_INFO_T;


#define GSPN_CAPABILITY_MAGIC_NUMBER 0xDEEFBEEF
typedef struct {
	uint32_t magic;//used to indicate struct is initialized

	/*version
	0x10: sharkLT8 GPP
	0x11: whale GPP
	*/
	uint8_t version;
	uint8_t block_alpha_limit;// on T8 GPP, block alpha influence the area not overlaped
	uint8_t max_video_size;// 0:1280x720 1:1920x1080 2:3840x2160
	uint8_t scale_updown_sametime;// 0:not supported; 1:supported

	uint16_t seq0_scale_range_up;// scale seq0 range, 1 means 1/16, 64 means 4
	uint16_t seq0_scale_range_down;// scale seq0 range, 1 means 1/16, 64 means 4
	uint16_t seq1_scale_range_up;// scale seq1 range, 1 means 1/16, 64 means 4
	uint16_t seq1_scale_range_down;// scale seq1 range, 1 means 1/16, 64 means 4

	uint8_t src_yuv_xywh_even_limit;// 0: even and odd both supported; 1:only even supported
	uint8_t max_layer_cnt; // max layer count of GSPN blending supported in one time, zorder should smaller than this cnt.
	uint8_t max_yuvLayer_cnt; //max video layer count, only one of the four support yuv format
	uint8_t max_scaleLayer_cnt; //

	uint16_t max_throughput;// Mega Pixel per second on out side.
	uint8_t addr_type_support;//GSPN_ADDR_TYPE_E
	/*
	bit[0]:BT601 full range
	bit[1]:BT601 narrow range

	bit[2]:BT709 full range
	bit[3]:BT709 narrow range

	bit[4]:BT2020 full range
	bit[5]:BT2020 narrow range
	*/
	uint8_t std_support_in;
	uint8_t std_support_out;
	//uint8_t dummy[1];

	GSPN_RECT_SIZE_T crop_min;
	GSPN_RECT_SIZE_T crop_max;
	GSPN_RECT_SIZE_T out_min;
	GSPN_RECT_SIZE_T out_max;
} GSPN_CAPABILITY_T;

enum gspn_ioctl_code {
	GSPN_CTL_CODE_INVALID = 0,
	GSPN_CTL_CODE_GET_CAPABILITY,
	GSPN_CTL_CODE_SET_PARAM,
	GSPN_CTL_CODE_MAX,
};

#define GSPN_IO_MAGIC					   'G'
#define GSPN_IO_GET_CAPABILITY			  _IOW(GSPN_IO_MAGIC, GSPN_CTL_CODE_GET_CAPABILITY,GSPN_CAPABILITY_T)
#if 0
#define GSPN_IO_SET_PARAM				   _IOW(GSP_IO_MAGIC, GSP_SET_PARAM,GSP_CONFIG_INFO_T)
#else
/*
0x(dir:2bit)(size:14bit)(type:8bit)			   (nr:8bit)
0x(dir:2bit)(size:14bit)(async:1bit)(cmd_cnt:7bit)(nr:8bit)
*/
#define GSPN_IO_SET_PARAM(cmd_cnt,async)	_IOW((async<<7|cmd_cnt), GSPN_CTL_CODE_SET_PARAM,GSPN_CMD_INFO_T)
#define GSPN_IO_ASYNC_MASK				  (0x80)
#define GSPN_IO_CNT_MASK					(0x7F)
#endif

#define GSPN_CMD_ARRAY_MAX  8 // the max GSPN_CMD_INFO_T[] pass to kernel at a single time

#endif

