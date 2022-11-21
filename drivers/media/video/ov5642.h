#ifndef OV5642_H_
#define OV5642_H_

#include <linux/types.h>
#include <media/v4l2-chip-ident.h>
#include <media/v4l2-common.h>
#include <media/soc_camera.h>

enum ov5642_register {
	ISP_INPUT_X_SZ_H	= 0x3804,
	ISP_INPUT_X_SZ_L,
	ISP_INPUT_Y_SZ_H,
	ISP_INPUT_Y_SZ_L,
	ISP_OUTPUT_X_SZ_H	= 0x3808,
	ISP_OUTPUT_X_SZ_L,
	ISP_OUTPUT_Y_SZ_H,
	ISP_OUTPUT_Y_SZ_L,
	ISP_OUTPUT_X_TSZ_H	= 0x380C,
	ISP_OUTPUT_X_TSZ_L,
	ISP_OUTPUT_Y_TSZ_H,
	ISP_OUTPUT_Y_TSZ_L,
	SCAN_ARR_X_OFF_H	= 0x3824,
	SCAN_ARR_X_OFF_L,
	SCAN_ARR_Y_OFF_H,
	SCAN_ARR_Y_OFF_L,
};

#define OV5642_END_ADDR		0xFFFF
#define OV5642_END_VAL		0xFF
#define END_SYMBOL		{OV5642_END_ADDR, OV5642_END_VAL}

struct regval_list {
	u16 reg_num;
	unsigned char value;
};

struct resv_size {
	int width;
	int height;
};

#define RESV(x)		(1 << (x))
enum ov5642_resv_support {
	OV5642_FMT_QCIF = 1,
	OV5642_FMT_QHVGA,
	OV5642_FMT_QVGA,
	OV5642_FMT_CIF,
	OV5642_FMT_HALF_VGA,
	OV5642_FMT_VGA,
	OV5642_FMT_D1,
	OV5642_FMT_WVGA,
	OV5642_FMT_720P,
	OV5642_FMT_1080P,
	OV5642_FMT_3M,
	OV5642_FMT_5M,

	OV5642_FMT_END,
};

struct ov5642_format {
	enum v4l2_mbus_pixelcode	code;
	unsigned int fmt_support;
	struct regval_list	*regs;
	struct regval_list	*def_set;
};

struct ov5642_mipi {
	struct regval_list *mipi_set_regs;
	struct regval_list *lane1_regs;
	struct regval_list *lane2_regs;
};

struct ov5642_win_size {
	enum ov5642_resv_support	resv;
	struct regval_list *regs;

	/* High resolution, 1920x1080(1080p), 1280x720(720p); */
	/* Low resolution, 640x480(480p) or smaller size */
	struct regval_list *regs_resolution;

	/* Update mipi clock setting to fix fps downgrade issue
	   on brownstone rev5 boards */
	struct regval_list  *lane_set;
};

typedef struct {
	u16 reg_base;
	unsigned char value[256];
	int len;
} OV5642_FIRMWARE_ARRAY;

struct ov5642_config {
	const char name[32];
	struct ov5642_format *fmt;
	int fmt_size;
	struct ov5642_mipi *mipi_lane;
	struct ov5642_win_size *yuv_res;
	int yuv_res_size;
	struct ov5642_win_size *jpg_res;
	int jpg_res_size;
	struct regval_list	*init;
	OV5642_FIRMWARE_ARRAY *firmware_af;
	int firmware_af_size;
};

struct ov5642 {
	struct v4l2_subdev subdev;
	int model;	/* V4L2_IDENT_OV5642* codes from v4l2-chip-ident.h */
	struct v4l2_rect rect;
	u32 pixfmt;
	int frame_rate;
	struct i2c_client *client;
	struct soc_camera_device icd;
	struct regval_list *init;
	struct regval_list *regs_fmt;
	struct regval_list *regs_size;
	struct regval_list *regs_default;
	struct regval_list *regs_lane_set;
	struct regval_list *regs_resolution;
	struct regval_list *regs_mipi_set;
	struct regval_list *regs_mipi_lane;
};

/* ov5642 has only one fixed colorspace per pixelcode */
struct ov5642_datafmt {
	enum v4l2_mbus_pixelcode	code;
	enum v4l2_colorspace		colorspace;
};



extern int ov5642_read(struct i2c_client *c, u16 reg, unsigned char *value);
extern int ov5642_write(struct i2c_client *c, u16 reg, unsigned char value);
extern int select_bus_type(const char *name);
extern int get_bus_type(void);
extern int set_stream(struct i2c_client *client, int enable);
extern int get_firmware(OV5642_FIRMWARE_ARRAY **firmware_array);
extern int set_yuv_res_array(struct v4l2_frmsizeenum *fsize);
extern int set_jpg_res_array(struct v4l2_frmsizeenum *fsize);
extern struct regval_list *get_global_init_regs(void);
extern struct regval_list *get_fmt_regs(enum v4l2_mbus_pixelcode code,
					int width, int height);
extern struct regval_list *get_fmt_default_setting( \
						enum v4l2_mbus_pixelcode code);
extern struct regval_list *get_yuv_size_regs(int width, int height);
extern struct regval_list *get_jpg_size_regs(int width, int length);
extern struct regval_list *get_yuv_lane_set(int width, int height);
extern struct regval_list *get_jpg_lane_set(int width, int height);
extern struct ov5642_win_size *get_yuv_size_array(void);
extern struct ov5642_win_size *get_jpg_size_array(void);
extern struct regval_list *get_mipi_set_regs(void);
extern struct regval_list *get_mipi_lane_regs(int num);
extern struct regval_list *get_yuv_resolution_regs(int width, int height);
extern struct regval_list *get_color_effect_regs(void);
#endif
