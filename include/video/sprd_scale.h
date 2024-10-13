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
 #ifndef _SPRD_SCALE_H_
#define _SPRD_SCALE_H_

typedef enum {
	SCALE_DATA_YUV422 = 0,
	SCALE_DATA_YUV420,
	SCALE_DATA_YUV400,
	SCALE_DATA_YUV420_3FRAME,
	SCALE_DATA_RGB565,
	SCALE_DATA_RGB888,
	SCALE_DATA_MAX		
}SCALE_DATA_FORMAT_E;

typedef enum {
	SCALE_MODE_SCALE = 0,
	SCALE_MODE_REVIEW,
	SCALE_MODE_MAX
}SCALE_MODE_E;
typedef struct size_tag {
	uint32_t	w;
	uint32_t    h;
} SCALE_SIZE_T;

typedef struct rect_tag {
    uint32_t	x;
    uint32_t         y;
    uint32_t         w;
    uint32_t         h;
}SCALE_RECT_T;

typedef struct data_addr_tag {
	uint32_t	yaddr;
	uint32_t    uaddr;
	uint32_t    vaddr;
}SCALE_ADDRESS_T;

typedef enum { 
	SCALE_PATH_INPUT_FORMAT = 0,
	SCALE_PATH_INPUT_SIZE,
	SCALE_PATH_INPUT_RECT,
	SCALE_PATH_INPUT_ADDR,
	SCALE_PATH_OUTPUT_SIZE,
	SCALE_PATH_OUTPUT_FORMAT,
	SCALE_PATH_OUTPUT_ADDR,
	SCALE_PATH_OUTPUT_FRAME_FLAG,
	SCALE_PATH_SWAP_BUFF,
	SCALE_PATH_LINE_BUFF,
	SCALE_PATH_SUB_SAMPLE_EN,
	SCALE_PATH_SUB_SAMPLE_MOD,
	SCALE_PATH_SLICE_SCALE_EN,
	SCALE_PATH_SLICE_SCALE_HEIGHT,
	SCALE_PATH_DITHER_EN,
	SCALE_PATH_IS_IN_SCALE_RANGE,
	SCALE_PATH_IS_SCALE_EN,
	SCALE_PATH_SLICE_OUT_HEIGHT,
	SCALE_PATH_MODE,
	SCALE_PATH_INPUT_ENDIAN,
	SCALE_PATH_OUTPUT_ENDIAN,
	SCALE_PATH_ROT_MODE,
	SCALE_CFG_ID_E_MAX
} SCALE_CFG_ID_E;

typedef enum
{
	SCALE_ROTATION_0 = 0,
	SCALE_ROTATION_90,
	SCALE_ROTATION_180,
	SCALE_ROTATION_270,
	SCALE_ROTATION_MIRROR,
	SCALE_ROTATION_MAX
}SCALE_ROTATION_E;

typedef struct Parth2_config {
	SCALE_CFG_ID_E id;
	void *param;	
}SCALE_CONFIG_T;

typedef struct yuv422_yuv420 {
	uint32_t	width;
	uint32_t 	height;
	uint32_t 	src_addr;
	uint32_t 	dst_addr;
}SCALE_YUV422_YUV420_T;

typedef struct yuv420_endian	
{
	uint32_t	width;
	uint32_t	height;
	uint32_t 	src_addr;
	uint32_t 	dst_addr;
}SCALE_YUV420_ENDIAN_T;

typedef struct _isp_endian_t {
	uint32_t endian_y;
	uint32_t endian_uv;
}ISP_ENDIAN_T;

#define SCALE_IOC_MAGIC 'S'
#define SCALE_IOC_CONFIG _IOW(SCALE_IOC_MAGIC, 0, SCALE_CONFIG_T)
#define SCALE_IOC_DONE    _IOW(SCALE_IOC_MAGIC, 1, uint32_t)
#define SCALE_IOC_YUV422_YUV420 _IOW(SCALE_IOC_MAGIC, 2, SCALE_YUV422_YUV420_T)
#define SCALE_IOC_YUV420_ENDIAN _IOW(SCALE_IOC_MAGIC, 3, SCALE_YUV420_ENDIAN_T)

int _SCALE_DriverIOPathConfig(SCALE_CFG_ID_E id, void* param);
int _SCALE_DriverIODone(void);
int _SCALE_DriverIOInit(void);
int _SCALE_DriverIODeinit (void);
#endif 
