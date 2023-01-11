#ifndef DB8221A_H_
#define DB8221A_H_

#include <linux/types.h>
#include <media/v4l2-common.h>
#include <media/soc_camera.h>

/* The token to indicate array termination */
#define DB8221A_TERM			0xFE
#define DB8221A_OUT_WIDTH_DEF		640
#define DB8221A_OUT_HEIGHT_DEF		480
#define DB8221A_WIN_WIDTH_MAX		1600
#define DB8221A_WIN_HEIGHT_MAX		1200
#define DB8221A_WIN_WIDTH_MIN		8
#define DB8221A_WIN_HEIGHT_MIN		8


enum db8221a_res_support {
	DB8221A_FMT_VGA = 0,
	/*DB8221A_FMT_800X600,*/
	DB8221A_FMT_2M,
	DB8221A_FMT_END,
};

struct db8221a_format {
	enum v4l2_mbus_pixelcode	code;
	unsigned int fmt_support;
	struct db8221a_regval	*regs;
	struct db8221a_regval	*def_set;
};

struct db8221a {
	struct v4l2_subdev subdev;
	struct v4l2_ctrl_handler	hdl;
	struct v4l2_rect rect;
	u32 pixfmt;
	int frame_rate;
	struct i2c_client *client;
	struct soc_camera_device icd;
};

/* db8221a has only one fixed colorspace per pixelcode */
struct db8221a_datafmt {
	enum v4l2_mbus_pixelcode	code;
	enum v4l2_colorspace		colorspace;
};

struct db8221a_resolution_table {
	int width;
	int height;
	enum db8221a_res_support res_code;
};


struct db8221a_resolution_table db8221a_resolutions[] = {
	{ 640,  480, DB8221A_FMT_VGA},	/* VGA */
#if 0
	{ 800,  600, DB8221A_FMT_800X600},	/* 800x600 */
#endif
	{1600,  1200, DB8221A_FMT_2M},	/* 2M */
};

/*this define for normal preview and camcorder different setting
 * but same resolution*/
#define NORMAL_STATUS 0
#define VIDEO_TO_NORMAL 1
#define NORMAL_TO_VIDEO 2
#define VIDEO_TO_CALL 3
#define V4L2_CID_PRIVATE_DB8221A_VIDEO_MODE \
	(V4L2_CID_CAMERA_CLASS_BASE + 0x1001)
#endif
