#ifndef SR200_H_
#define SR200_H_

#include <linux/types.h>
#include <media/v4l2-common.h>
#include <media/soc_camera.h>

/* The token to indicate array termination */
#define SR200_TERM			0xff
#define SR200_OUT_WIDTH_DEF		640
#define SR200_OUT_HEIGHT_DEF		480
#define SR200_WIN_WIDTH_MAX		1600
#define SR200_WIN_HEIGHT_MAX		1200
#define SR200_WIN_WIDTH_MIN		8
#define SR200_WIN_HEIGHT_MIN		8

enum sr200_res_support {
	SR200_FMT_QVGA = 0,
	SR200_FMT_VGA,
	/*SR200_FMT_800X600,*/
	SR200_FMT_2M,
	SR200_FMT_END,
};

struct sr200_format {
	enum v4l2_mbus_pixelcode	code;
	unsigned int fmt_support;
	struct sr200_regval	*regs;
	struct sr200_regval	*def_set;
};

struct sr200 {
	struct v4l2_subdev subdev;
	struct v4l2_ctrl_handler	hdl;
	struct v4l2_rect rect;
	u32 pixfmt;
	int frame_rate;
	struct i2c_client *client;
	struct soc_camera_device icd;
};

/* sr200 has only one fixed colorspace per pixelcode */
struct sr200_datafmt {
	enum v4l2_mbus_pixelcode	code;
	enum v4l2_colorspace		colorspace;
};

struct sr200_resolution_table {
	int width;
	int height;
	enum sr200_res_support res_code;
};

struct sr200_resolution_table sr200_resolutions[] = {
	{ 320,  240, SR200_FMT_QVGA},	/* VGA */
	{ 640,  480, SR200_FMT_VGA},	/* VGA */
#if 0
	{ 800,  600, SR200_FMT_800X600},	/* 800x600 */
#endif
	{1600,  1200, SR200_FMT_2M},	/* 2M */
};



/*
 * this define for normal preview and camcorder different setting
 * but same resolution
 */
#define NORMAL_STATUS 0
#define VIDEO_TO_NORMAL 1
#define NORMAL_TO_VIDEO 2
#define VIDEO_TO_CALL 3
#define V4L2_CID_PRIVATE_SR200_VIDEO_MODE \
	(V4L2_CID_CAMERA_CLASS_BASE + 0x1001)
#endif
