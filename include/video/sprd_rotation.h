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
#ifndef _SPRD_ROTATION_H_
#define _SPRD_ROTATION_H_

typedef enum {
	ROTATION_YUV422 = 0,
	ROTATION_YUV420,
	ROTATION_YUV400,
	ROTATION_RGB888,
	ROTATION_RGB666,
	ROTATION_RGB565,
	ROTATION_RGB555,
	ROTATION_MAX
} ROTATION_DATA_FORMAT_E;

typedef enum {
	ROTATION_90 = 0,
	ROTATION_270,
	ROTATION_180,
	ROTATION_MIRROR,
	ROTATION_DIR_MAX
} ROTATION_DIR_E;

typedef struct _rotation_size_tag {
	uint16_t w;
	uint16_t h;
} ROTATION_SIZE_T;

typedef struct _ROTATION_data_addr_tag {
	uint32_t y_addr;
	uint32_t uv_addr;
	uint32_t v_addr;
} ROTATION_DATA_ADDR_T;
typedef struct _rotation_tag {
	ROTATION_SIZE_T         img_size;
	ROTATION_DATA_FORMAT_E   data_format;
	ROTATION_DIR_E         rotation_dir; 
	ROTATION_DATA_ADDR_T    src_addr;
	ROTATION_DATA_ADDR_T    dst_addr;     
}ROTATION_PARAM_T, *ROTATION_PARAM_T_PTR;

int rotation_start(ROTATION_PARAM_T* param_ptr);
int rotation_IOinit(void);
int rotation_IOdeinit(void);

#define SPRD_ROTATION_IOCTL_MAGIC 'm'
#define SPRD_ROTATION_DONE _IOW(SPRD_ROTATION_IOCTL_MAGIC, 1, unsigned int)
#define SPRD_ROTATION_DATA_COPY _IOW(SPRD_ROTATION_IOCTL_MAGIC, 2, unsigned int)
#define SPRD_ROTATION_DATA_COPY_VIRTUAL _IOW(SPRD_ROTATION_IOCTL_MAGIC, 3, unsigned int)
#define SPRD_ROTATION_DATA_COPY_FROM_V_TO_P _IOW(SPRD_ROTATION_IOCTL_MAGIC, 4, unsigned int)
#endif
