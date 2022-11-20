/*
 * Samsung Exynos5 SoC series Sensor driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/videodev2.h>
#include <linux/videodev2_exynos_camera.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/vmalloc.h>
#include <linux/platform_device.h>
#include <mach/regs-gpio.h>
#include <mach/regs-clock.h>
#include <plat/clock.h>
#include <plat/gpio-cfg.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <mach/exynos-fimc-is-sensor.h>

#include "../fimc-is-core.h"
#include "../fimc-is-device-sensor.h"
#include "../fimc-is-resourcemgr.h"
#include "../fimc-is-hw.h"
#include "fimc-is-device-4ec.h"

#define SENSOR_NAME "M7MU"
#define DEFAULT_SENSOR_WIDTH	640
#define DEFAULT_SENSOR_HEIGHT	480
#define SENSOR_MEMSIZE DEFAULT_SENSOR_WIDTH * DEFAULT_SENSOR_HEIGHT

/* Category */
#define M7MU_CATEGORY_SYS	0x00
#define M7MU_CATEGORY_PARM	0x01
#define M7MU_CATEGORY_MON	0x02
#define M7MU_CATEGORY_AE	0x03
#define M7MU_CATEGORY_NEW	0x04
#define M7MU_CATEGORY_PRO_MODE	0x05
#define M7MU_CATEGORY_WB	0x06
#define M7MU_CATEGORY_EXIF	0x07
#define M7MU_CATEGORY_OT    	0x08
#define M7MU_CATEGORY_FD	0x09
#define M7MU_CATEGORY_LENS	0x0A
#define M7MU_CATEGORY_CAPPARM	0x0B
#define M7MU_CATEGORY_CAPCTRL	0x0C
#define M7MU_CATEGORY_TEST	0x0D
#define M7MU_CATEGORY_ADJST	0x0E
#define M7MU_CATEGORY_FLASH	0x0F    /* F/W update */

/* M7MU_CATEGORY_SYS: 0x00 */
#define M7MU_SYS_PJT_CODE	0x01
#define M7MU_SYS_VER_FW		0x02
#define M7MU_SYS_VER_HW		0x04
#define M7MU_SYS_VER_PARAM	0x06
#define M7MU_SYS_VER_AWB	0x08
#define M7MU_SYS_USER_VER	0x0A
#define M7MU_SYS_MODE		0x0B
#define M7MU_SYS_ESD_INT	0x0E

#define M7MU_SYS_INT_EN		0x10
#define M7MU_SYS_INT_FACTOR	0x1C
#define M7MU_SYS_FRAMESYNC_CNT	0x14
#define M7MU_SYS_LENS_TIMER	0x28

/* M7MU_CATEGORY_PARAM: 0x01 */
#define M7MU_PARM_OUT_SEL	0x00
#define M7MU_PARM_MON_SIZE	0x01
#define M7MU_PARM_MON_FPS	0x02
#define M7MU_PARM_EFFECT	0x0B
#define M7MU_PARM_FLEX_FPS	0x67
#define M7MU_PARM_HDMOVIE	0x32
#define M7MU_PARM_VIDEO_SNAP_IMG_TRANSFER_START 0x3A
#define M7MU_PARM_SEL_FRAME_VIDEO_SNAP 0x3B
#define M7MU_PARM_MON_MOVIE_SELECT	0x3C
#define M7MU_PARM_VSS_MODE 0x6E
#define M7MU_PARM_SET_PRE_LIVEVIEW 0x7C
#define M7MU_PARM_SET_ERASER_MODE 0x10


/* M7MU_CATEGORY_MON: 0x02 */
#define M7MU_MON_ZOOM		0x01
#define M7MU_MON_HR_ZOOM    0x04
#define M7MU_MON_MON_REVERSE	0x05
#define M7MU_MON_MON_MIRROR	0x06
#define M7MU_MON_SHOT_REVERSE	0x07
#define M7MU_MON_SHOT_MIRROR	0x08
#define M7MU_MON_CFIXB		0x09
#define M7MU_MON_CFIXR		0x0A
#define M7MU_MON_COLOR_EFFECT	0x0B
#define M7MU_MON_CHROMA_LVL	0x0F
#define M7MU_MON_EDGE_LVL	0x11
#define M7MU_MON_EDGE_CTRL	0x20
#define M7MU_MON_POINT_COLOR	0x22
#define M7MU_MON_TONE_CTRL	0x25
#define M7MU_MON_AF_SET_TOUCH		0x46
#define M7MU_MON_START_VIDEO_SNAP_SHOT 0x56
#define M7MU_MON_VIDEO_SNAP_SHOT_FRAME_COUNT 0x57
#define M7MU_MON_AF_FPS_CHECK 0xAF

/* M7MU_CATEGORY_AE: 0x03 */
#define M7MU_AE_LOCK		0x00
#define M7MU_AE_MODE		0x01
#define M7MU_AE_ISOSEL		0x05
#define M7MU_AE_FLICKER		0x06
#define M7MU_AE_INDEX		0x09
#define M7MU_AE_EP_MODE_MON	0x0A
#define M7MU_AE_EP_MODE_CAP	0x0B
#define M7MU_AF_AE_LOCK		0x0D
#define M7MU_AE_AUTO_BRACKET_EV	0x20
#define M7MU_AE_STABILITY	0x21
#define M7MU_AE_EV_PRG_MODE_CAP	0x34
#define M7MU_AE_EV_PRG_MODE_MON	0x35
#define M7MU_AE_EV_PRG_F_NUMBER	0x36
#define M7MU_AE_EV_PRG_SS_NUMERATOR		0x37
#define M7MU_AE_EV_PRG_SS_DENOMINATOR	0x39
#define M7MU_AE_EV_PRG_ISO_VALUE	0x3B
#define M7MU_AE_EV_PRG_F_NUMBER_MON				0x3D
#define M7MU_AE_EV_PRG_SS_NUMERATOR_MON		0x3E
#define M7MU_AE_EV_PRG_SS_DENOMINATOR_MON		0x40
#define M7MU_AE_EV_PRG_ISO_VALUE_MON			0x42
#define M7MU_AE_NOW_AV	0x54
#define M7MU_AE_NOW_TV	0x58
#define M7MU_AE_NOW_SV	0x5C
#define M7MU_AE_NOW_LV	0x52

/* M7MU_CATEGORY_NEW: 0x04 */
#define M7MU_NEW_TIME_INFO		0x02
#define M7MU_NEW_OIS_CUR_MODE	0x06
#define M7MU_NEW_OIS_TIMER		0x07
#define M7MU_NEW_DETECT_SCENE	0x0B
#define M7MU_NEW_OIS_VERSION	0x1B

/* M7MU_CATEGORY_PRO_MODE: 0x05 */
#define M7MU_PRO_SMART_READ1		0x20
#define M7MU_PRO_SMART_READ2		0x24
#define M7MU_PRO_SMART_READ3		0x28

/* M7MU_CATEGORY_WB: 0x06 */
#define M7MU_AWB_LOCK		0x00
#define M7MU_WB_AWB_MODE	0x02
#define M7MU_WB_AWB_MANUAL	0x03
#define M7MU_WB_GBAM_MODE	0x8D
#define M7MU_WB_G_VALUE		0x8E
#define M7MU_WB_B_VALUE		0x8F
#define M7MU_WB_A_VALUE		0x90
#define M7MU_WB_M_VALUE		0x91
#define M7MU_WB_K_VALUE		0x92
#define M7MU_WB_CWB_MODE		0x93
#define M7MU_WB_SET_CUSTOM_RG	0x94
#define M7MU_WB_SET_CUSTOM_BG	0x96
#define M7MU_WB_GET_CUSTOM_RG	0x98
#define M7MU_WB_GET_CUSTOM_BG	0x9A
#define M7MU_WB_WBB_MODE		0x9C
#define M7MU_WB_WBB_AB		0x9D
#define M7MU_WB_WBB_GM		0x9E
#define M7MU_WB_COLOR_ADJ	0x9F

/* M7MU_CATEGORY_EXIF: 0x07 */
#define M7MU_EXIF_EXPTIME_NUM	0x00
#define M7MU_EXIF_EXPTIME_DEN	0x04
#define M7MU_EXIF_TV_NUM	0x08
#define M7MU_EXIF_TV_DEN	0x0C
#define M7MU_EXIF_BV_NUM	0x18
#define M7MU_EXIF_BV_DEN	0x1C
#define M7MU_EXIF_EBV_NUM	0x20
#define M7MU_EXIF_EBV_DEN	0x24
#define M7MU_EXIF_ISO		0x28
#define M7MU_EXIF_FLASH		0x2A
#define M7MU_EXIF_AV_NUM	0x10
#define M7MU_EXIF_AV_DEN	0x14
#define M7MU_EXIF_FL	0x11
#define M7MU_EXIF_FL_35	0x13

/* M7MU_CATEGORY_OT: 0x08 */
#define M7MU_OT_TRACKING_CTL		0x00
#define M7MU_OT_INFO_READY			0x01
#define M7MU_OT_X_START_LOCATION	0x05
#define M7MU_OT_Y_START_LOCATION	0x07
#define M7MU_OT_X_END_LOCATION		0x09
#define M7MU_OT_Y_END_LOCATION		0x0B
#define M7MU_OT_TRACKING_X_LOCATION	0x10
#define M7MU_OT_TRACKING_Y_LOCATION	0x12
#define M7MU_OT_TRACKING_FRAME_WIDTH	0x14
#define M7MU_OT_TRACKING_FRAME_HEIGHT	0x16
#define M7MU_OT_TRACKING_STATUS		0x18
#define M7MU_OT_FRAME_WIDTH			0x30

/* M7MU_CATEGORY_FD: 0x09 */
#define M7MU_FD_CTL					0x00
#define M7MU_FD_SIZE				0x01
#define M7MU_FD_MAX					0x02
#define M7MU_FD_RED_EYE				0x55
#define M7MU_FD_RED_DET_STATUS		0x56
#define M7MU_FD_BLINK_FRAMENO		0x59
#define M7MU_FD_BLINK_LEVEL_1		0x5A
#define M7MU_FD_BLINK_LEVEL_2		0x5B
#define M7MU_FD_BLINK_LEVEL_3		0x5C

/* M7MU_CATEGORY_LENS: 0x0A */
#define M7MU_LENS_AF_INITIAL		0x00
#define M7MU_LENS_AF_LENS_CLOSE		0x01
#define M7MU_LENS_AF_ZOOM_CTRL		0x02
#define M7MU_LENS_AF_START_STOP		0x03
#define M7MU_LENS_AF_IRIS_STEP		0x05
#define M7MU_LENS_AF_ZOOM_LEVEL		0x06
#define M7MU_LENS_AF_SCAN_RANGE		0x07
#define M7MU_LENS_AF_MODE			0x08
#define M7MU_LENS_AF_WINDOW_MODE	0x09
#define M7MU_LENS_AF_BACKLASH_ADJ	0x0A
#define M7MU_LENS_AF_FOCUS_ADJ		0x0B
#define M7MU_LENS_AF_TILT_ADJ		0x0C
#define M7MU_LENS_AF_AF_ADJ			0x0D
#define M7MU_LENS_AF_PUNT_ADJ		0x0E
#define M7MU_LENS_AF_ZOOM_ADJ		0x0F
#define M7MU_LENS_AF_ADJ_TEMP_VALUE	0x0C
#define M7MU_LENS_AF_ALGORITHM		0x0D
#define M7MU_LENS_ZOOM_LEVEL_INFO	0x10
#define M7MU_LENS_AF_LED			0x1C
#define M7MU_LENS_AF_CAL			0x1D
#define M7MU_LENS_AF_RESULT			0x20
#define M7MU_LENS_ZOOM_SET_INFO		0x22
#define M7MU_LENS_ZOOM_SPEED		0x25
#define M7MU_LENS_ZOOM_STATUS		0x26
#define M7MU_LENS_LENS_STATUS		0x28
#define M7MU_LENS_ZOOM_LENS_STATUS	0x2A
#define M7MU_LENS_AF_TEMP_INDICATE	0x2B
#define M7MU_LENS_TIMER_LED			0x2D
#define M7MU_LENS_AF_TOUCH_POSX		0x30
#define M7MU_LENS_AF_TOUCH_POSY		0x32
#define M7MU_LENS_AF_ADJ_OK_NG		0x40
#define M7MU_LENS_AF_FOCUS_CHECK		0x42
#define M7MU_LENS_MF_DEFAULT_INDEX	0x50
#define M7MU_LENS_MF_NEARLIMIT_INDEX	0x51
#define M7MU_LENS_MF_MAX_INDEX		0x52
#define M7MU_LENS_MF_RUN			0x53
#define M7MU_LENS_ZOOM_ACTION_METHOD	0x54
#define M7MU_LENS_AF_VERSION		0x60
#define M7MU_LENS_MEM_INIT_TARGET	0x96
#define M7MU_LENS_CHECK_SUM			0xC3

/* M7MU_CATEGORY_CAPPARM: 0x0B */
#define M7MU_CAPPARM_YUVOUT_MAIN	0x00
#define M7MU_CAPPARM_MAIN_IMG_SIZE	0x01
#define M7MU_CAPPARM_YUVOUT_PREVIEW	0x05
#define M7MU_CAPPARM_PREVIEW_IMG_SIZE	0x06
#define M7MU_CAPPARM_YUVOUT_THUMB	0x0A
#define M7MU_CAPPARM_THUMB_IMG_SIZE	0x0B
#define M7MU_CAPPARM_JPEG_SIZE_MAX	0x0F
#define M7MU_CAPPARM_JPEG_SIZE_MIN	0x13
#define M7MU_CAPPARM_JPEG_RATIO		0x17
#define M7MU_CAPPARM_MCC_MODE		0x1D
#define M7MU_CAPPARM_STROBE_EN		0x22
#define M7MU_CAPPARM_STROBE_CHARGE	0x27
#define M7MU_CAPPARM_STROBE_EVC		0x28
#define M7MU_CAPPARM_STROBE_UP_DOWN	0x29
#define M7MU_CAPPARM_WDR_EN			0x2C
#define M7MU_CAPPARM_JPEG_RATIO_OFS	0x1B
#define M7MU_CAPPARM_THUMB_JPEG_MAX	0x3C
#define M7MU_CAPPARM_STROBE_BATT_INFO	0x3F
#define M7MU_CAPPARM_AFB_CAP_EN		0x53

/* M7MU_CATEGORY_CAPCTRL: 0x0C */
#define M7MU_CAPCTRL_CAP_MODE	0x00
#define M7MU_CAPCTRL_CAP_FRM_INTERVAL 0x01
#define M7MU_CAPCTRL_CAP_FRM_COUNT 0x02
#define M7MU_CAPCTRL_START_DUALCAP 0x05
#define M7MU_CAPCTRL_FRM_SEL	0x06
#define M7MU_CAPCTRL_FRM_PRV_SEL	0x07
#define M7MU_CAPCTRL_FRM_THUMB_SEL	0x08
#define M7MU_CAPCTRL_TRANSFER	0x09
#define M7MU_CAPCTRL_IMG_SIZE	0x0D
#define M7MU_CAPCTRL_THUMB_SIZE	0x11
#define M7MU_CAPCTRL_AUTOMATIC_SHIFT_EN	0x25

/* M7MU_CATEGORY_CAPCTRL: 0x0C  M7MU_CAPCTRL_CAP_MODE: 0x00 */
#define M7MU_CAP_MODE_SINGLE_CAPTURE		(0x00)
#define M7MU_CAP_MODE_MULTI_CAPTURE			(0x01)
#define M7MU_CAP_MODE_DUAL_CAPTURE			(0x05)
#define M7MU_CAP_MODE_BRACKET_CAPTURE		(0x06)
#define M7MU_CAP_MODE_ADDPIXEL_CAPTURE		(0x08)
#define M7MU_CAP_MODE_PANORAMA_CAPTURE		(0x0B)
#define M7MU_CAP_MODE_BLINK_CAPTURE			(0x0C)
#define M7MU_CAP_MODE_RAW			(0x0D)

/* M7MU_CATEGORY_ADJST: 0x0E */
#define M7MU_ADJST_SHUTTER_MODE	0x33
#define M7MU_ADJST_CLIP_VALUE1	0x34
#define M7MU_ADJST_CLIP_VALUE2	0x35
#define M7MU_ADJST_AWB_RG_H	0x3C
#define M7MU_ADJST_AWB_RG_L	0x3D
#define M7MU_ADJST_AWB_BG_H	0x3E
#define M7MU_ADJST_AWB_BG_L	0x3F

/* M7MU_CATEGORY_FLASH: 0x0F */
#define M7MU_FLASH_ADDR		0x00
#define M7MU_FLASH_BYTE		0x04
#define M7MU_FLASH_ERASE	0x06
#define M7MU_FLASH_WR		0x07
#define M7MU_FLASH_RAM_CLEAR	0x08
#define M7MU_FLASH_CAM_START_AD		0x0C
#define M7MU_FLASH_CAM_START	0x12
#define M7MU_FLASH_SEL		0x13
#define M7MU_FLASH_DATA_RAM_ADDR 	0x14
#define M7MU_FLASH_SIO_CS_POL		0x49
#define M7MU_FLASH_RAM_START		0x4A
#define M7MU_FLASH_SMR_VALUE		0x4B
#define M7MU_FLASH_SDITOPSCNF2		0x4c
#define M7MU_FLASH_SDITOPSCNF3		0x50
#define M7MU_FLASH_SDITOPSCNF4		0x54
#define M7MU_FLASH_SDIPHYIMP1		0x58
#define M7MU_FLASH_SDIPHYIMP2		0x5c
#define M7MU_FLASH_SDIPHYIOD1		0x60
#define M7MU_FLASH_SDIPHYIOD2		0x64
#define M7MU_FLASH_SDIPHYIOD3		0x68
#define M7MU_FLASH_SDIPHYIOD4		0x6c
#define M7MU_FLASH_SDIDDRMR0		0x70
#define M7MU_FLASH_SDIDDRMR1		0x74
#define M7MU_FLASH_SDIDDRMR2		0x78
#define M7MU_FLASH_SDIDDRMR3		0x7c
#define M7MU_FLASH_SDIDDRCNF1		0x80
#define M7MU_FLASH_SDIDDRAC1		0x84
#define M7MU_FLASH_SDIDDRAC2		0x88
#define M7MU_FLASH_SDIDDRAC5		0x8c
#define M7MU_FLASH_SDIDDRAC6		0x90
#define M7MU_FLASH_SDIDDRAC7		0x94
#define M7MU_FLASH_SDIDDRAC8		0x98
#define M7MU_FLASH_SDIDDRAC9		0x9c
#define M7MU_FLASH_SDIDDRACA		0xA0
#define M7MU_FLASH_SDIDDRACC		0xA4
#define M7MU_FLASH_SDIDDRACD		0xA8
#define M7MU_FLASH_SDIDDRPHY1		0xAC
#define M7MU_FLASH_SDIDDRAR1		0xB0
#define M7MU_FLASH_SDIDDRZQ1		0xB4
#define M7MU_FLASH_DPLSEL		0xB8

/* M7MU_CATEGORY_TEST:	0x0D */
#define M7MU_TEST_OUTPUT_YCO_TEST_DATA	0x1B
#define M7MU_TEST_ISP_PROCESS		0x59

/* M7MU Sensor Mode */
#define M7MU_SYSINIT_MODE	0x0
#define M7MU_PARMSET_MODE	0x1
#define M7MU_MONITOR_MODE	0x2
#define M7MU_STILLCAP_MODE	0x3

/* Interrupt Factor */
#define M7MU_INT_SOUND		(1 << 15)
#define M7MU_INT_LENS_INIT	(1 << 14)
#define M7MU_INT_FD		(1 << 13)
#define M7MU_INT_FRAME_SYNC	(1 << 12)
#define M7MU_INT_CAPTURE	(1 << 11)
#define M7MU_INT_ZOOM		(1 << 10)
#define M7MU_INT_AF		(1 << 9)
#define M7MU_INT_MODE		(1 << 8)
#define M7MU_INT_ATSCENE	(1 << 7)
#define M7MU_INT_ATSCENE_UPDATE	(1 << 6)
#define M7MU_INT_AF_STATUS	(1 << 5)
#define M7MU_INT_OIS_SET	(1 << 4)
#define M7MU_INT_OIS_INIT	(1 << 3)
#define M7MU_INT_STNW_DETECT	(1 << 2)
#define M7MU_INT_SCENARIO_FIN	(1 << 1)
#define M7MU_INT_PRINT		(1 << 0)

/* ESD Interrupt */
#define M7MU_INT_ESD		(1 << 0)

#define m7mu_readb(sd, g, b, v) m7mu_read(__LINE__, sd, 1, g, b, v, true)
#define m7mu_readw(sd, g, b, v) m7mu_read(__LINE__, sd, 2, g, b, v, true)
#define m7mu_readl(sd, g, b, v) m7mu_read(__LINE__, sd, 4, g, b, v, true)

#define m7mu_writeb(sd, g, b, v) m7mu_write(__LINE__, sd, 1, g, b, v, true)
#define m7mu_writew(sd, g, b, v) m7mu_write(__LINE__, sd, 2, g, b, v, true)
#define m7mu_writel(sd, g, b, v) m7mu_write(__LINE__, sd, 4, g, b, v, true)

#define m7mu_readb2(sd, g, b, v) m7mu_read(__LINE__, sd, 1, g, b, v, false)
#define m7mu_readw2(sd, g, b, v) m7mu_read(__LINE__, sd, 2, g, b, v, false)
#define m7mu_readl2(sd, g, b, v) m7mu_read(__LINE__, sd, 4, g, b, v, false)

#define m7mu_writeb2(sd, g, b, v) m7mu_write(__LINE__, sd, 1, g, b, v, false)
#define m7mu_writew2(sd, g, b, v) m7mu_write(__LINE__, sd, 2, g, b, v, false)
#define m7mu_writel2(sd, g, b, v) m7mu_write(__LINE__, sd, 4, g, b, v, false)

#define M7MU_I2C_RETRY		5
#define M7MU_I2C_VERIFY		100

#define M7MU_ISP_TIMEOUT		10000
#define M7MU_ISP_CAPTURE_TIMEOUT	35000
#define M7MU_SOUND_TIMEOUT		35000
#define M7MU_ISP_AFB_TIMEOUT	15000 /* FIXME */
#define M7MU_ISP_ESD_TIMEOUT	1000

#define CHECK_ERR(x)	if ((x) <= 0) { \
				err("i2c failed, err %d\n", x); \
				return x; \
			}

static struct fimc_is_sensor_cfg config_m7m[] = {
	/* 1456x1090@24fps */
	FIMC_IS_SENSOR_CFG(1456, 1090, 24, 13, 0),
	/* 1936x1090@24fps */
	FIMC_IS_SENSOR_CFG(1936, 1090, 24, 13, 1),
	/* 1456x1090@30fps */
	FIMC_IS_SENSOR_CFG(1456, 1090, 30, 16, 2),
	/* 1936x1090@30fps */
	FIMC_IS_SENSOR_CFG(1456, 1090, 30, 16, 3),
	/* 1456x1090@24fps */
	FIMC_IS_SENSOR_CFG(1456, 1090, 24, 13, 4),
	/* 1456x1090@15fps */
	FIMC_IS_SENSOR_CFG(1456, 1090, 15, 13, 5),
	/* 1456x1090@15fps */
	FIMC_IS_SENSOR_CFG(960, 720, 0, 12, 5)
};

static int m7mu_read(int _line, struct i2c_client *client,
	u8 len, u8 category, u8 byte, int *val, bool log)
{
	struct i2c_msg msg;
	unsigned char data[5];
	unsigned char recv_data[len + 1];
	int i, err = 0;
	int retry = 3;

	if (!client->adapter)
		return -ENODEV;

	if (len != 0x01 && len != 0x02 && len != 0x04)
		return -EINVAL;

i2c_retry:
	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = sizeof(data);
	msg.buf = data;

	/* high byte goes out first */
	data[0] = msg.len;
	data[1] = 0x01;			/* Read category parameters */
	data[2] = category;
	data[3] = byte;
	data[4] = len;

	for (i = M7MU_I2C_RETRY; i; i--) {
		err = i2c_transfer(client->adapter, &msg, 1);
		if (err == 1)
			break;
		msleep(20);
	}

	if (err != 1) {
		err("category %#x, byte %#x, err %d\n", category, byte, err);
		return err;
	}

	msg.flags = I2C_M_RD;
	msg.len = sizeof(recv_data);
	msg.buf = recv_data;
	for (i = M7MU_I2C_RETRY; i; i--) {
		err = i2c_transfer(client->adapter, &msg, 1);
		if (err == 1)
			break;
		msleep(20);
	}

	if (err != 1) {
		err("RD category %#x, byte %#x, err %d\n",
			category, byte, err);
		return err;
	}

	if (recv_data[0] != sizeof(recv_data)) {
#if 0
		cam_i2c_dbg("expected length %d, but return length %d\n",
				 sizeof(recv_data), recv_data[0]);
#endif
		if (retry > 0) {
			retry--;
			msleep(20);
			goto i2c_retry;
		} else {
			err("Retry all failed for expected length error.");
			return -1;
		}
	}

	if (len == 0x01)
		*val = recv_data[1];
	else if (len == 0x02)
		*val = recv_data[1] << 8 | recv_data[2];
	else
		*val = recv_data[1] << 24 | recv_data[2] << 16 |
				recv_data[3] << 8 | recv_data[4];

	if (log)
		cam_info("[ %4d ] Read %s %#02x, byte %#x, value %#x\n",
			_line, (len == 4 ? "L" : (len == 2 ? "W" : "B")),
			category, byte, *val);

	return err;
}

static int m7mu_write(int _line, struct i2c_client *client,
	u8 len, u8 category, u8 byte, int val, bool log)
{
	struct i2c_msg msg;
	unsigned char data[len + 4];
	int i, err;

	if (!client->adapter)
		return -ENODEV;

	if (len != 0x01 && len != 0x02 && len != 0x04)
		return -EINVAL;

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = sizeof(data);
	msg.buf = data;

	data[0] = msg.len;
	data[1] = 0x02;			/* Write category parameters */
	data[2] = category;
	data[3] = byte;
	if (len == 0x01) {
		data[4] = val & 0xFF;
	} else if (len == 0x02) {
		data[4] = (val >> 8) & 0xFF;
		data[5] = val & 0xFF;
	} else {
		data[4] = (val >> 24) & 0xFF;
		data[5] = (val >> 16) & 0xFF;
		data[6] = (val >> 8) & 0xFF;
		data[7] = val & 0xFF;
	}

	if (log)
		cam_info("[ %4d ] Write %s %#x, byte %#x, value %#x\n",
			_line, (len == 4 ? "L" : (len == 2 ? "W" : "B")),
			category, byte, val);

	for (i = M7MU_I2C_RETRY; i; i--) {
		err = i2c_transfer(client->adapter, &msg, 1);
		if (err == 1)
			break;
		msleep(20);
	}

	return err;
}

//static u32 sensor_m7mu_wait_interrupt(struct v4l2_subdev *sd, unsigned int timeout);
//static int m7mu_wait_boot_interrupt(struct v4l2_subdev *sd, unsigned int timeout);

static int m7mu_set_mode(struct fimc_is_module_enum *module, u32 mode)
{
	int ret = 0;
	int i, err;
	u32 old_mode, val;
	int retry_mode_change = 1;
	struct fimc_is_module_4ec *module_m7mu;
	struct i2c_client *client;
#if 0
	struct v4l2_subdev *sd = module->subdev;
#endif
	unsigned int int_factor;
	int try_cnt = 60;

	module_m7mu = module->private_data;
	if (unlikely(!module_m7mu)) {
		err("module_m7mu is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	client = module->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	//msleep(1000);
	err = m7mu_readb(client, M7MU_CATEGORY_SYS, M7MU_SYS_MODE, &old_mode);
	CHECK_ERR(err);

	if (module_m7mu->samsung_app) {
		/* don't change mode when cap -> param */
		if (old_mode == M7MU_STILLCAP_MODE && mode == M7MU_PARMSET_MODE)
			return 10;
	}

	/* Dual Capture */
	if (module_m7mu->dual_capture_start && mode == M7MU_STILLCAP_MODE)
		mode = M7MU_PARMSET_MODE;

	if (old_mode == mode) {
		cam_info("%#x -> %#x\n", old_mode, mode);
		return old_mode;
	}

	cam_info("%#x -> %#x\n", old_mode, mode);

	if(mode == M7MU_MONITOR_MODE)
	{
		m7mu_writeb(client, 0x01, 0x01, 0x34);
		m7mu_writeb(client, 0x00, 0x10, 0x01);
	}

retry_mode_set:
	switch (old_mode) {
	case M7MU_SYSINIT_MODE:
		warn("sensor is initializing\n");
		err = -EBUSY;
		break;

	case M7MU_PARMSET_MODE:
		if (mode == M7MU_STILLCAP_MODE) {
			err = m7mu_writeb(client, M7MU_CATEGORY_SYS,
				M7MU_SYS_MODE, M7MU_MONITOR_MODE);
			if (err <= 0)
				break;
			for (i = M7MU_I2C_VERIFY; i; i--) {
				err = m7mu_readb(client, M7MU_CATEGORY_SYS,
					M7MU_SYS_MODE, &val);
				if (val == M7MU_MONITOR_MODE)
					break;
				msleep(20);
			}
		}
		//break;
	case M7MU_STILLCAP_MODE: 
	case M7MU_MONITOR_MODE:
		m7mu_writeb(client, M7MU_CATEGORY_CAPPARM, M7MU_CAPPARM_YUVOUT_MAIN, 0x01);
		m7mu_writeb(client, M7MU_CATEGORY_CAPPARM, M7MU_CAPPARM_MAIN_IMG_SIZE, 0x33);
		err = m7mu_writeb(client, M7MU_CATEGORY_SYS, M7MU_SYS_MODE, mode);
#if 0
		m7mu_writew(client, M7MU_CATEGORY_SYS, M7MU_SYS_INT_EN,
			M7MU_INT_MODE | M7MU_INT_CAPTURE | M7MU_INT_FRAME_SYNC
			| M7MU_INT_ATSCENE_UPDATE | M7MU_INT_AF
			| /* M7MU_INT_SOUND | */M7MU_INT_ZOOM);
#endif	
		if(mode == M7MU_STILLCAP_MODE)
		{
#if 0		
			/* Clear Interrupt factor */
			int_factor = sensor_m7mu_wait_interrupt(sd, M7MU_ISP_CAPTURE_TIMEOUT);
			if (!(int_factor & M7MU_INT_CAPTURE)) {
				cam_warn("M7MU_INT_CAPTURE isn't issued, %#x\n",
						int_factor);
				//return -ETIMEDOUT;
			}
#endif		
			
		}
		break;

	default:
		warn("current mode is unknown, %d\n", old_mode);
		err = 1;/* -EINVAL; */
	}

	if (err <= 0)
		return err;

	for (i = M7MU_I2C_VERIFY; i; i--) {
		err = m7mu_readb(client, M7MU_CATEGORY_SYS, M7MU_SYS_MODE, &val);
		if (val == mode)
			break;
		msleep(20);
	}

	do
	{
		m7mu_readw(client, M7MU_CATEGORY_SYS, M7MU_SYS_INT_FACTOR, &int_factor);
		cam_info(" int_factor : %x\n", int_factor);
		if(int_factor == 0xffff)
		{
			try_cnt--;
			msleep(10);
		}else
			try_cnt = 0;
	}while(try_cnt);

	if (val != mode) {
		if (retry_mode_change) {
			retry_mode_change = 0;
			goto retry_mode_set;
		} else {
			warn("ISP mode not change, %d -> %d\n", val, mode);
			return -ETIMEDOUT;
		}
	}
#if 0 //Not supported cateParam on M7MU
	if ((mode == M7MU_PARMSET_MODE) && (state->mode == MODE_ERASER)) {
		err = m7mu_writeb(sd, M7MU_CATEGORY_PARM,
			M7MU_PARM_SET_ERASER_MODE, 0x01);
		if (err < 0)
			return err;
	} else if ((mode == M7MU_PARMSET_MODE) && (state->mode != MODE_ERASER)){
		err = m7mu_writeb(sd, M7MU_CATEGORY_PARM,
			M7MU_PARM_SET_ERASER_MODE, 0x00);
		if (err < 0)
			return err;
	}
#endif

	module_m7mu->isp_mode = mode;

p_err:
	cam_info("X\n");
	return old_mode;
}

static inline struct m7mu_state *to_state(struct v4l2_subdev *sd)
{
	return container_of(sd, struct m7mu_state, sd);
}

static int sensor_m7m_init(struct v4l2_subdev *subdev, u32 val)
{
	int ret = 0;
	struct fimc_is_module_enum *module;
	struct fimc_is_module_4ec *module_4ec;
	struct i2c_client *client;

	BUG_ON(!subdev);

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	module_4ec = module->private_data;
	client = module->client;

	//m7mu_wait_boot_interrupt(subdev, 3000);

	cam_info("[MOD:D:%d] (%d)\n", module->id, val);

	return ret;
}

#if 0
static u32 sensor_m7mu_wait_interrupt(struct v4l2_subdev *sd,
	unsigned int timeout)
{
	struct m7mu_state *state = to_state(sd);
	struct fimc_is_module_enum *module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(sd);
	struct i2c_client *client = module->client;
	int try_cnt = 60;
#ifdef ISP_LOGWRITE
	char filepath[256];

	sprintf(filepath, "ISP_interrupt_error_%d.log", state->log_num);
#endif
	cam_trace("E\n");

#if 0
	if (wait_event_interruptible_timeout(state->isp.wait,
		state->isp.issued == 1,
		msecs_to_jiffies(timeout)) == 0) {
		cam_err("timeout ~~~~~~~~~~~~~~~~~~~~~\n");
		return 0;
	}
#else
	if (wait_event_timeout(state->isp.wait,
				state->isp.issued == 1,
				msecs_to_jiffies(timeout)) == 0) {
		int err = 0;
		u32 int_en = 0, int_factor = 0, mode = 0, sys_status = 0;
#ifdef ISP_LOGWRITE
		m7mu_makeLog(sd, filepath, true);
#endif
		cam_err("timeout ~~~~~~~~~~~~~~~~~~~~~~~\n");
		err = m7mu_readw(client, M7MU_CATEGORY_SYS, M7MU_SYS_INT_EN, &int_en);
		CHECK_ERR(err);
		err = m7mu_readw(client, M7MU_CATEGORY_SYS,
			M7MU_SYS_INT_FACTOR, &int_factor);
		CHECK_ERR(err);
		err = m7mu_readb(client, M7MU_CATEGORY_SYS, M7MU_SYS_MODE, &mode);
		CHECK_ERR(err);
		err = m7mu_readb(client, M7MU_CATEGORY_SYS,
			0x0c, &sys_status);
		CHECK_ERR(err);
		cam_err("int_en=%d, int_factor=%d, mode_status=%d, sys_status=%d\n",
			int_en, int_factor, mode, sys_status);
		return 0;
	}
#endif

	state->isp.issued = 0;

	do {
		m7mu_readw(client, M7MU_CATEGORY_SYS,
			M7MU_SYS_INT_FACTOR, &state->isp.int_factor);
		cam_err(": state->isp.int_factor = %x\n",
					state->isp.int_factor);
		if (state->isp.int_factor == 0xFFFF) {
			try_cnt--;
			msleep(10);
		} else
			try_cnt = 0;
	} while (try_cnt);

	cam_trace("X %s\n",
		(state->isp.int_factor == 0xFFFF ? "fail(0xFFFF)" : ""));
	return state->isp.int_factor;
}
#endif
static int m7mu_set_oprmode_mode(struct v4l2_subdev *sd, int oprmode)
{
	struct m7mu_state *state =
		container_of(sd, struct m7mu_state, sd);

	cam_info("set oprmode to %d\n", oprmode);
	switch(oprmode) {
	case M7MU_OPRMODE_VIDEO:
		state->oprmode = M7MU_OPRMODE_VIDEO;
		break;
	case M7MU_OPRMODE_IMAGE:
		state->oprmode = M7MU_OPRMODE_IMAGE;
		break;
	case M7MU_OPRMODE_HDR:
		state->oprmode = M7MU_OPRMODE_HDR;
		break;
	default:
		cam_err("Oprmode %d is not supported.", oprmode);
		return -1;
	}

	return 0;
}

static int m7mu_start_capture(struct v4l2_subdev *sd, int frame_num)
{
	struct m7mu_state *state = to_state(sd);
	int err;
	//int int_factor;
	struct fimc_is_module_enum *module;
	struct i2c_client *client;
	int ret;
	
	cam_trace("E : %d frame\n", frame_num);

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(sd);
	if (unlikely(!module)) {
		err("module is NULL");
		ret = -EINVAL;
		return ret;
	}

	client = module->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		return ret;
	}

	/* Select image number of frame */
	err = m7mu_writeb(client, M7MU_CATEGORY_CAPCTRL,
			M7MU_CAPCTRL_FRM_SEL, 0x01);

	/* Get main JPEG data */
	err = m7mu_writeb(client, M7MU_CATEGORY_CAPCTRL,
			M7MU_CAPCTRL_TRANSFER, 0x01);

#if 0
	 /*wait M7MU_INT_CAPTURE*/
	 int_factor = sensor_m7mu_wait_interrupt(sd, M7MU_ISP_TIMEOUT);
	 if(!(int_factor & M7MU_INT_CAPTURE))
	 {
	 	cam_warn("M7MU_INT_CAPTURE isn't issued on transfer, %#x\n", int_factor);
		//return -ETIMEDOUT;
	}
#endif
	msleep(1000);
	/* Get JPEG Size */
	 err = m7mu_readl(client, M7MU_CATEGORY_CAPCTRL, M7MU_CAPCTRL_IMG_SIZE,
				&state->jpeg.main_size);
	CHECK_ERR(err);
	cam_dbg("   ==> jpeg size=%d\n", state->jpeg.main_size);

	return err;
}


static int sensor_m7m_s_ctrl(struct v4l2_subdev *subdev, struct v4l2_control *ctrl)
{
	int ret = 0;
	//struct m7mu_state *state = to_state(subdev);
	//struct v4l2_subdev *sd = subdev;

	switch (ctrl->id) {
	case V4L2_CID_SCENEMODE:
		printk("V4L2_CID_SCENEMODE : %d\n", ctrl->value);
		break;
	case V4L2_CID_FOCUS_MODE:
		printk("V4L2_CID_FOCUS_MODE : %d\n", ctrl->value);
		break;
	case V4L2_CID_WHITE_BALANCE_PRESET:
		printk("V4L2_CID_WHITE_BALANCE_PRESET : %d\n", ctrl->value);
		break;
	case V4L2_CID_IMAGE_EFFECT:
		printk("V4L2_CID_IMAGE_EFFECT : %d\n", ctrl->value);
		break;
	case V4L2_CID_CAM_ISO:
		printk("V4L2_CID_CAM_ISO : %d\n", ctrl->value);
		break;
	case V4L2_CID_CAM_CONTRAST:
		printk("V4L2_CID_CAM_CONTRAST : %d\n", ctrl->value);
		break;
	case V4L2_CID_CAM_SATURATION:
		printk("V4L2_CID_CAM_SATURATION : %d\n", ctrl->value);
		break;
	case V4L2_CID_CAM_SHARPNESS:
		printk("V4L2_CID_CAM_SHARPNESS : %d\n", ctrl->value);
		break;
	case V4L2_CID_CAM_BRIGHTNESS:
		printk("V4L2_CID_CAM_BRIGHTNESS : %d\n", ctrl->value);
		break;
	case V4L2_CID_CAM_METERING:
		printk("V4L2_CID_CAM_METERING : %d\n", ctrl->value);
		break;
	case V4L2_CID_CAM_SET_AUTO_FOCUS:
		printk("V4L2_CID_CAM_SET_AUTO_FOCUS : %d\n", ctrl->value);
		break;
	case V4L2_CID_CAM_OBJECT_POSITION_X:
		printk("V4L2_CID_CAM_OBJECT_POSITION_X : %d\n", ctrl->value);
		break;
	case V4L2_CID_CAM_OBJECT_POSITION_Y:
		printk("V4L2_CID_CAM_OBJECT_POSITION_Y : %d\n", ctrl->value);
		break;
	case V4L2_CID_CAM_FACE_DETECTION:
		printk("V4L2_CID_CAM_FACE_DETECTION : %d\n", ctrl->value);
		break;
	case V4L2_CID_CAM_WDR:
		printk("V4L2_CID_CAM_WDR : %d\n", ctrl->value);
		break;
	case V4L2_CID_CAM_AUTO_FOCUS_RESULT:
		printk("V4L2_CID_CAM_AUTO_FOCUS_RESULT : %d\n", ctrl->value);
		break;
	case V4L2_CID_JPEG_QUALITY:
		printk("V4L2_CID_JPEG_QUALITY : %d\n", ctrl->value);
		break;
	case V4L2_CID_CAM_AEAWB_LOCK_UNLOCK:
		printk("V4L2_CID_CAM_AEAWB_LOCK_UNLOCK : %d\n", ctrl->value);
		break;
	case V4L2_CID_CAM_CAF_START_STOP:
		printk("V4L2_CID_CAM_CAF_START_STOP : %d\n", ctrl->value);
		break;
	case V4L2_CID_CAM_ZOOM:
		printk("V4L2_CID_CAM_ZOOM : %d\n", ctrl->value);
		break;
	case V4L2_CID_CAM_SINGLE_AUTO_FOCUS:
		printk("V4L2_CID_CAM_SINGLE_AUTO_FOCUS : %d\n", ctrl->value);
		break;
	case V4L2_CID_CAPTURE:
		m7mu_set_oprmode_mode(subdev, ctrl->value);
		printk("V4L2_CID_CAPTURE : %d\n", ctrl->value);
		break;
	case V4L2_CID_CAM_FLASH_MODE:
		cam_info("V4L2_CID_CAM_FLASH_MODE : %d\n", ctrl->value);
		break;
	case V4L2_CID_START_CAPTURE_KIND:
		cam_info("V4L2_CID_START_CAPTURE_KIND : %d\n", ctrl->value);
		//state->start_cap_kind = ctrl->value;
		break;
	case V4L2_CID_CAMERA_TRANSFER:
		cam_info("V4L2_CID_CAMERA_TRANSFER : %d\n", ctrl->value);
		m7mu_start_capture(subdev, ctrl->value);
		break;
	default:
		err("invalid ioctl(0x%08X) is requested", ctrl->id);
		//ret = -EINVAL;
		break;
	}

//p_err:
	return ret;
}

static const struct v4l2_subdev_core_ops core_ops = {
	.init		= sensor_m7m_init,
	.s_ctrl		= sensor_m7m_s_ctrl
};

static int sensor_m7m_s_format(struct v4l2_subdev *subdev, struct v4l2_mbus_framefmt *fmt)
{
	int ret = 0;
	struct s5k4ecgx_framesize const *size;
	struct fimc_is_module_enum *module;
	struct fimc_is_module_4ec *module_4ec;
	struct i2c_client *client;

	BUG_ON(!subdev);
	BUG_ON(!fmt);

	cam_info("%s\n", __func__);

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (!module) {
		err("module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	module_4ec = module->private_data;
	if (unlikely(!module_4ec)) {
		err("module_4ec is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	client = module->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	module_4ec->image.window.offs_h = 0;
	module_4ec->image.window.offs_v = 0;
	module_4ec->image.window.width = fmt->width;
	module_4ec->image.window.height = fmt->height;
	module_4ec->image.window.o_width = fmt->width;
	module_4ec->image.window.o_height = fmt->height;
	module_4ec->image.format.pixelformat = fmt->code;

	size = NULL;

p_err:
	return ret;
}

static int sensor_m7m_s_stream(struct v4l2_subdev *subdev, int enable)
{
	int ret = 0;
	struct fimc_is_module_enum *module;

	cam_info("%s\n", __func__);

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (!module) {
		err("module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	if (enable) {
		ret = CALL_MOPS(module, stream_on, subdev);
		if (ret) {
			err("stream_on is fail(%d)", ret);
			goto p_err;
		}
	} else {
		ret = CALL_MOPS(module, stream_off, subdev);
		if (ret) {
			err("stream_off is fail(%d)", ret);
			goto p_err;
		}
	}

p_err:
	return 0;
}

static int sensor_m7m_s_param(struct v4l2_subdev *subdev, struct v4l2_streamparm *param)
{
	int ret = 0;
	struct fimc_is_module_enum *module;
	struct v4l2_captureparm *cp;
	struct v4l2_fract *tpf;
	u64 framerate;

	BUG_ON(!subdev);
	BUG_ON(!param);

	cam_info("%s\n", __func__);

	cp = &param->parm.capture;
	tpf = &cp->timeperframe;

	if (!tpf->numerator) {
		err("numerator is 0");
		ret = -EINVAL;
		goto p_err;
	}

	framerate = tpf->denominator;

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (!module) {
		err("module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = CALL_MOPS(module, s_duration, subdev, framerate);
	if (ret) {
		err("s_duration is fail(%d)", ret);
		goto p_err;
	}

p_err:
	return ret;
}

static const struct v4l2_subdev_video_ops video_ops = {
	.s_stream = sensor_m7m_s_stream,
	.s_parm = sensor_m7m_s_param,
	.s_mbus_fmt = sensor_m7m_s_format
};

static const struct v4l2_subdev_ops subdev_ops = {
	.core = &core_ops,
	.video = &video_ops
};

int sensor_m7m_stream_on(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct fimc_is_module_enum *module;
	struct fimc_is_module_4ec *module_4ec;
	struct i2c_client *client;
	struct m7mu_state *state = to_state(subdev);
	u32 old_mode;
	
	BUG_ON(!subdev);

	cam_info("stream on\n");

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (unlikely(!module)) {
		err("module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	module_4ec = module->private_data;
	if (unlikely(!module_4ec)) {
		err("module_4ec is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	client = module->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	switch( state->oprmode )
	{
		case M7MU_OPRMODE_VIDEO:
		
			old_mode = m7mu_set_mode(module, M7MU_MONITOR_MODE);
			if (old_mode <= 0) {
				err("failed to set mode\n");
				return old_mode;
			}

			if (old_mode != M7MU_MONITOR_MODE) {
				/*int_factor = m7mu_wait_interrupt(sd, M7MU_ISP_TIMEOUT);
				if (!(int_factor & M7MU_INT_MODE)) {
					cam_err("M7MU_INT_MODE isn't issued, %#x\n",
						int_factor);
					return -ETIMEDOUT;
				}*/
				msleep(5000);
				cam_info("check point\n");
			}
			
			break;
		case M7MU_OPRMODE_IMAGE:
			old_mode = m7mu_set_mode(module, M7MU_STILLCAP_MODE);
			if (old_mode <= 0) {
				err("failed to set mode\n");
				return old_mode;
			}

			if (old_mode != M7MU_STILLCAP_MODE) {
				/*int_factor = m7mu_wait_interrupt(sd, M7MU_ISP_TIMEOUT);
				if (!(int_factor & M7MU_INT_MODE)) {
					cam_err("M7MU_INT_MODE isn't issued, %#x\n",
						int_factor);
					return -ETIMEDOUT;
				}*/
				msleep(5000);
				cam_info("check point\n");
			}
			break;
		case M7MU_OPRMODE_HDR:
			break;
		//case M7MU_OPRMODE_MAX:
		default:
			break;
	}

p_err:
	return ret;
}

int sensor_m7m_stream_off(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct fimc_is_module_enum *sensor;
	struct i2c_client *client;

	BUG_ON(!subdev);

	sensor = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (unlikely(!sensor)) {
		err("sensor is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	client = sensor->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

p_err:
	return ret;
}

#if 1
/*
 * @ brief
 * frame duration time
 * @ unit
 * nano second
 * @ remarks
 */
int sensor_m7m_s_duration(struct v4l2_subdev *subdev, u64 duration)
{
	int ret = 0;
	u32 framerate;
	struct fimc_is_module_enum *module;
	struct fimc_is_module_4ec *module_4ec;
	struct i2c_client *client;

	BUG_ON(!subdev);

	cam_info("%s\n", __func__);

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (unlikely(!module)) {
		err("module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	module_4ec = module->private_data;
	client = module->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	framerate = duration;
	if (framerate > V4L2_FRAME_RATE_MAX) {
		err("framerate is invalid(%d)", framerate);
		ret = -EINVAL;
		goto p_err;
	}

p_err:
	return ret;
}

int sensor_m7m_g_min_duration(struct v4l2_subdev *subdev)
{
	int ret = 0;
	return ret;
}

int sensor_m7m_g_max_duration(struct v4l2_subdev *subdev)
{
	int ret = 0;
	return ret;
}

int sensor_m7m_s_exposure(struct v4l2_subdev *subdev, u64 exposure)
{
	int ret = 0;
	u8 value;
	struct fimc_is_module_enum *sensor;
	struct i2c_client *client;

	BUG_ON(!subdev);

	cam_info("%s(%d)\n", __func__, (u32)exposure);

	sensor = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (unlikely(!sensor)) {
		err("sensor is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	client = sensor->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	value = exposure & 0xFF;


p_err:
	return ret;
}

int sensor_m7m_g_min_exposure(struct v4l2_subdev *subdev)
{
	int ret = 0;
	return ret;
}

int sensor_m7m_g_max_exposure(struct v4l2_subdev *subdev)
{
	int ret = 0;
	return ret;
}

int sensor_m7m_s_again(struct v4l2_subdev *subdev, u64 sensitivity)
{
	int ret = 0;

	cam_info("%s\n", __func__);

	return ret;
}

int sensor_m7m_g_min_again(struct v4l2_subdev *subdev)
{
	int ret = 0;
	return ret;
}

int sensor_m7m_g_max_again(struct v4l2_subdev *subdev)
{
	int ret = 0;
	return ret;
}

int sensor_m7m_s_dgain(struct v4l2_subdev *subdev)
{
	int ret = 0;
	return ret;
}

int sensor_m7m_g_min_dgain(struct v4l2_subdev *subdev)
{
	int ret = 0;
	return ret;
}

int sensor_m7m_g_max_dgain(struct v4l2_subdev *subdev)
{
	int ret = 0;
	return ret;
}

#endif

//#define ENABLE_ISP_IRQ
#if defined(ENABLE_ISP_IRQ)
#define ENABLE_IRQ_BOOT
//#define ENABLE_IRQ_STATUS

#if defined(ENABLE_IRQ_STATUS)
//#define GPIO_ISP_INT1 EXYNOS5260_GPK0(1)
static irqreturn_t sensor_m7mu_isp_isr(int irq, void *dev_id)
{
	struct v4l2_subdev *sd = (struct v4l2_subdev *)dev_id;
	struct m7mu_state *state = to_state(sd);

	cam_trace("**************** interrupt ****************\n");
	state->isp.issued = 1;
	wake_up(&state->isp.wait);

	return IRQ_HANDLED;
}
#endif
#if defined(ENABLE_IRQ_BOOT)
#define GPIO_ISP_STATUS_INT EXYNOS5260_GPD2(0)

static int m7mu_wait_boot_interrupt(struct v4l2_subdev *sd, unsigned int timeout)
{
	struct m7mu_state *state = to_state(sd);
	cam_dbg("Wait ISP Boot Complete ~~~\n");
	if (wait_event_timeout(state->isp.boot_wait,
			state->isp.boot_issued == 1,
			msecs_to_jiffies(timeout)) == 0) {
		cam_dbg("~~~ waiting boot timeout ~~~\n");
		return -ENOSYS;
	}
	state->isp.boot_issued = 0;
	/*
	if(state->isp.boot_lens)
	{
		m7mu_power(sd, M7MU_HW_POWER_OFF);
		state->isp.boot_lens = 0;
	}
	*/
	return 0;
}

static irqreturn_t sensor_m7mu_isp_boot_isr(int irq, void *dev_id)
{
	struct v4l2_subdev *sd = (struct v4l2_subdev *)dev_id;
	struct m7mu_state *state = to_state(sd);

	cam_trace("**************** isp boot interrupt ****************\n");
	state->isp.boot_issued = 1;
	wake_up(&state->isp.boot_wait);

	return IRQ_HANDLED;
}
#endif


static ssize_t sensor_m7mu_register_irq(struct v4l2_subdev *sd)
{

	//struct fimc_is_module_enum *module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(sd);
	//struct i2c_client *client = module->client;
	//struct m7mu_platform_data *pdata = client->dev.platform_data;
	struct m7mu_state *state = to_state(sd);
	//unsigned int irq;

	int err = 0;
	int ret = 0;
#if 0
	if (pdata->config_isp_irq)
		pdata->config_isp_irq();
#endif

#if defined(ENABLE_IRQ_STATUS)
	printk(KERN_INFO "m7mu_config_isp_irq\n");
	gpio_request(GPIO_ISP_INT1, "M7MU IRQ");
	s3c_gpio_cfgpin(GPIO_ISP_INT1, S3C_GPIO_SFN(0xF));
	s3c_gpio_setpull(GPIO_ISP_INT1, S3C_GPIO_PULL_DOWN);
	
	ret = s5p_register_gpio_interrupt(GPIO_ISP_INT1);
	if (ret < 0) {
		printk(KERN_ERR "%s: s5p_register_gpio_interrupt is failed. %d",
				__func__, ret);
	}
	gpio_free(GPIO_ISP_INT1);

	state->isp.irq = gpio_to_irq(GPIO_ISP_INT1);

	/* wait queue initialize */
	init_waitqueue_head(&state->isp.wait);

	err = request_irq(state->isp.irq,
		sensor_m7mu_isp_isr, IRQF_TRIGGER_RISING, "m7mu isp", sd);
	if (err) {
		cam_err("failed to request irq ~~~~~~~~~~~~~\n");
		return err;
	}
	//state->isp.irq = pdata->irq;
	state->isp.issued = 0;
#endif

#if defined(ENABLE_IRQ_BOOT)
	printk(KERN_INFO "m7mu_config_boot_isp_irq\n");
	gpio_request(GPIO_ISP_STATUS_INT, "M7MU BOOT IRQ");
	s3c_gpio_cfgpin(GPIO_ISP_STATUS_INT, S3C_GPIO_SFN(0xF));
	s3c_gpio_setpull(GPIO_ISP_STATUS_INT, S3C_GPIO_PULL_DOWN);
	
	ret = s5p_register_gpio_interrupt(GPIO_ISP_STATUS_INT);
	if (ret < 0) {
		printk(KERN_ERR "%s: s5p_register_gpio_interrupt is failed. %d",
				__func__, ret);
	}
	gpio_free(GPIO_ISP_STATUS_INT);

	/* set boot_int to irq */
	state->isp.boot_irq = gpio_to_irq(GPIO_ISP_STATUS_INT);

	/* wait queue initialize */
	init_waitqueue_head(&state->isp.boot_wait);

	err = request_irq(state->isp.boot_irq,
		sensor_m7mu_isp_boot_isr, IRQF_TRIGGER_RISING, "m7mu boot isp", sd);
	if (err) {
		cam_err("failed to request boot irq ~~~~~~~~~~~~~\n");
		return err;
	}
	state->isp.boot_issued = 0;
#endif

	return err;
}
#endif

struct fimc_is_sensor_ops module_m7m_ops = {
	.stream_on	= sensor_m7m_stream_on,
	.stream_off	= sensor_m7m_stream_off,
	.s_duration	= sensor_m7m_s_duration,
	.g_min_duration	= sensor_m7m_g_min_duration,
	.g_max_duration	= sensor_m7m_g_max_duration,
	.s_exposure	= sensor_m7m_s_exposure,
	.g_min_exposure	= sensor_m7m_g_min_exposure,
	.g_max_exposure	= sensor_m7m_g_max_exposure,
	.s_again	= sensor_m7m_s_again,
	.g_min_again	= sensor_m7m_g_min_again,
	.g_max_again	= sensor_m7m_g_max_again,
	.s_dgain	= sensor_m7m_s_dgain,
	.g_min_dgain	= sensor_m7m_g_min_dgain,
	.g_max_dgain	= sensor_m7m_g_max_dgain
};

int sensor_m7m_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret = 0;
	struct fimc_is_core *core;
	struct v4l2_subdev *subdev_module;
	struct fimc_is_module_enum *module;
	struct fimc_is_device_sensor *device;

	BUG_ON(!fimc_is_dev);

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core) {
		err("core device is not yet probed");
		return -EPROBE_DEFER;
	}

	device = &core->sensor[SENSOR_M7MU_INSTANCE];

	subdev_module = kzalloc(sizeof(struct v4l2_subdev), GFP_KERNEL);
	if (!subdev_module) {
		err("subdev_module is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	/* S5K6B2 */
	module = &device->module_enum[atomic_read(&core->resourcemgr.rsccount_module)];
	atomic_inc(&core->resourcemgr.rsccount_module);
	module->id = SENSOR_M7MU_NAME;
	module->subdev = subdev_module;
	module->device = SENSOR_M7MU_INSTANCE;
	module->ops = &module_m7m_ops;
	module->client = client;
	module->pixel_width = 1920 + 16;
	module->pixel_height = 1080 + 10;
	module->active_width = 1920;
	module->active_height = 1080;
	module->max_framerate = 120;
	module->position = SENSOR_POSITION_REAR;
	module->mode = CSI_MODE_VC_ONLY;
	module->lanes = CSI_DATA_LANES_4;
	module->setfile_name = "setfile_m7m.bin";
	module->cfgs = ARRAY_SIZE(config_m7m);
	module->cfg = config_m7m;
	module->private_data = kzalloc(sizeof(struct fimc_is_module_4ec), GFP_KERNEL);
	if (!module->private_data) {
		err("private_data is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

#ifdef DEFAULT_S5K4EC_DRIVING
	v4l2_i2c_subdev_init(subdev_module, client, &subdev_ops);
#else
	v4l2_subdev_init(subdev_module, &subdev_ops);
#endif
	v4l2_set_subdevdata(subdev_module, module);
	v4l2_set_subdev_hostdata(subdev_module, device);
	snprintf(subdev_module->name, V4L2_SUBDEV_NAME_SIZE, "sensor-subdev.%d", module->id);

#if defined(ENABLE_ISP_IRQ)
	sensor_m7mu_register_irq(subdev_module);
#endif
p_err:
	cam_info("%s(%d)\n", __func__, ret);
	return ret;
}

static int sensor_m7m_remove(struct i2c_client *client)
{
	int ret = 0;
	return ret;
}

#ifdef CONFIG_OF
static const struct of_device_id exynos_fimc_is_sensor_m7m_match[] = {
	{
		.compatible = "samsung,exynos5-fimc-is-sensor-4ec",
	},
	{},
};
#endif

static const struct i2c_device_id sensor_m7m_idt[] = {
	{ SENSOR_NAME, 0 },
};

static struct i2c_driver sensor_m7m_driver = {
	.driver = {
		.name	= SENSOR_NAME,
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = exynos_fimc_is_sensor_m7m_match
#endif
	},
	.probe	= sensor_m7m_probe,
	.remove	= sensor_m7m_remove,
	.id_table = sensor_m7m_idt
};

static int __init sensor_m7m_load(void)
{
        return i2c_add_driver(&sensor_m7m_driver);
}

static void __exit sensor_m7m_unload(void)
{
        i2c_del_driver(&sensor_m7m_driver);
}

module_init(sensor_m7m_load);
module_exit(sensor_m7m_unload);

MODULE_AUTHOR("Gilyeon lim");
MODULE_DESCRIPTION("Sensor m7mu driver");
MODULE_LICENSE("GPL v2");

