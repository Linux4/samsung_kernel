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
#ifndef _SPRD_FB_H_
#define _SPRD_FB_H_

#define SPRD_LAYERS_IMG (0x1)   /*support YUV & RGB*/
#define SPRD_LAYERS_OSD (0x2)   /*support RGB only*/

enum {
	SPRD_DATA_FORMAT_YUV422 = 0,
	SPRD_DATA_FORMAT_YUV420,
	SPRD_DATA_FORMAT_YUV400,
	SPRD_DATA_FORMAT_RGB888,
	SPRD_DATA_FORMAT_RGB666,
	SPRD_DATA_FORMAT_RGB565,
	SPRD_DATA_FORMAT_RGB555,
	SPRD_DATA_FORMAT_LIMIT
};

enum{
	SPRD_DATA_ENDIAN_B0B1B2B3 = 0,
	SPRD_DATA_ENDIAN_B3B2B1B0,
	SPRD_DATA_ENDIAN_B2B3B0B1,
	SPRD_DATA_ENDIAN_B1B0B3B2,
	SPRD_DATA_ENDIAN_LIMIT
};

enum{
	SPRD_DISPLAY_OVERLAY_ASYNC = 0,
	SPRD_DISPLAY_OVERLAY_SYNC,
	SPRD_DISPLAY_OVERLAY_LIMIT
};

typedef struct overlay_setting_rect {
	uint16_t x; //start point - x
	uint16_t y; //start point - y
	uint16_t w; //width
	uint16_t h; //height
}overlay_setting_rect;

typedef struct overlay_setting{
	int layer_index;
	int data_type;
	int y_endian;
	int uv_endian;
	bool rb_switch;
	overlay_setting_rect rect;
	unsigned char *buffer;
}overlay_setting;

typedef struct overlay_display_setting{
	int layer_index;
	overlay_setting_rect rect;
	int display_mode;
}overlay_display_setting;

/*
int sprdfb_IOinit(void);
int sprdfb_IOdeinit(void);
*/


#define SPRD_FB_IOCTL_MAGIC 'm'
#define SPRD_FB_SET_OVERLAY _IOW(SPRD_FB_IOCTL_MAGIC, 1, unsigned int)
#define SPRD_FB_DISPLAY_OVERLAY _IOW(SPRD_FB_IOCTL_MAGIC, 2, unsigned int)
#define SPRD_FB_CHANGE_FPS _IOW(SPRD_FB_IOCTL_MAGIC, 3, unsigned int)
#define SPRD_FB_IS_REFRESH_DONE _IOW(SPRD_FB_IOCTL_MAGIC, 4, unsigned int)
#endif
