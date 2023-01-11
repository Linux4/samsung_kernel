#ifndef __MMP_B52_SENSOR_H__
#define __MMP_B52_SENSOR_H__

#include <linux/types.h>
#include <linux/gpio/consumer.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-ctrls.h>
#include <uapi/linux/v4l2-mediabus.h>
#include <media/mrvl-camera.h>
#include <media/b52_api.h>
#define MAX_REGULATOR_NUM   (5)
#define MAX_SENSOR_FMT_NUM  (4)

/*
 * if want to 5m delay, set regval_tab to
 * [SENSOR_MDELAY, SENSOR_MDELAY, 5]
 */
#define SENSOR_MDELAY    (0xffff)

/* forward references */
struct b52_sensor;

#define to_b52_sensor(sd) container_of((sd), struct b52_sensor, sd);

#define b52_sensor_call(s, f, args...) \
	(!(s) ? -ENODEV : (((s)->ops.f) ? \
	(s)->ops.f((&(s)->sd) , ##args)   \
	: -ENOIOCTLCMD))

enum b52_sensor_i2c_pos {
	B52_SENSOR_I2C_NONE = 0,
	B52_SENSOR_I2C_PRIMARY_MASTER,
	B52_SENSOR_I2C_SECONDARY_MASTER,
	B52_SENSOR_I2C_BOTH,
};

enum b52_sensor_type {
	OVT_SENSOR = 0,
	SONY_SENSOR,
	SAMSUNG_SENSOR,
	HYNIX_SENSOR,
	SAMSUNG_ASNY_SENSOR,
};

enum b52_sensor_gain_type {
	B52_SENSOR_AG = 0,
	B52_SENSOR_DG,
	B52_SENSOR_GAIN_MAX,
};

enum b52_sensor_param_type {
	B52_SENSOR_GAIN = 0,
	B52_SENSOR_AGAIN,
	B52_SENSOR_DGAIN,
	B52_SENSOR_EXPO,
	B52_SENSOR_FRACTIONALEXPO,
	B52_SENSOR_VTS,
	B52_SENSOR_REQ_VTS,
	B52_SENSOR_REQ_HTS,
	B52_SENSOR_FOCUS,
	B52_SENSOR_PARAM_MAX,
};

struct regval_tab {
	u16 reg;
	u16 val;
	u16 mask;
};

struct b52_sensor_regs {
	struct regval_tab *tab;
	u32 num;
};

struct b52_sensor_mbus_fmt {
	enum v4l2_mbus_pixelcode mbus_code;
	enum v4l2_colorspace colorspace;
	struct b52_sensor_regs regs;
};

/*cropped for video*/
enum b52_sensor_res_prop {
	SENSOR_RES_BINING1,
	SENSOR_RES_BINING2,
	SENSOR_RES_CROPPED,
	SENSOR_RES_MAX,
};

/*sensor mode*/
enum b52_sensor_mode {
	SENSOR_NORMAL_MODE=0,
	SENSOR_PREVIEW_MODE,
	SENSOR_VIDEO_MODE,
	SENSOR_CAPTURE_MODE,
};

/*HB >= 7% one line length, unit pixel */
/*not include exposure, gain and VTS cfg*/
struct b52_sensor_resolution {
	u32 width;
	u32 height;
	u32 hts;
	u32 min_vts;
	enum b52_sensor_mode sensor_mode;
	enum b52_sensor_res_prop prop;
	struct b52_sensor_regs regs;
};

struct sensor_prop_range {
	u32 min;
	u32 max;
};

struct b52_sensor_vcm {
	const char *name;
	enum b52_vcm_type type;
	struct b52_sensor_i2c_attr *attr;
	u16 pos_reg_msb;
	u16 pos_reg_lsb;
	/*suppose this property belongs to some module */
	struct b52_sensor_regs id;
	struct b52_sensor_regs init;
};

struct b52_sensor_module {
	u32 id;
	u32 apeture_size;
	struct b52_sensor_vcm *vcm;
};
struct b52_sensor_otp {
	u32 customer_id;
	u32 module_id;
	u32 lens_id;
	u32 af_cal_dir;
	u32 af_inf_cur;
	u32 af_mac_cur;
	u32 rg_ratio;
	u32 bg_ratio;
	u32 gg_ratio;
	u32 golden_rg_ratio;
	u32 golden_bg_ratio;
	u32 golden_gg_ratio;
	struct sensor_otp *user_otp;
	u32 user_data[5];
	u32 otp_ctrl;
	enum OTP_TYPE	otp_type;
};

struct b52_sensor_spec_ops {
	 /*pixel clk rate unit HZ*/
	int (*get_pixel_rate)(struct v4l2_subdev *sd, u32 *rate, u32 mclk);
	int (*get_dphy_desc)(struct v4l2_subdev *sd,
			struct csi_dphy_desc *dphy_desc, u32 mclk);
	int (*update_otp)(struct v4l2_subdev *sd, struct b52_sensor_otp *otp);
	int (*s_power)(struct v4l2_subdev *sd, int onoff);
};

enum sensor_i2c_len {
	I2C_8BIT = 0,
	I2C_16BIT,
};

struct b52_sensor_i2c_attr {
	enum sensor_i2c_len reg_len;
	enum sensor_i2c_len val_len;
	u8 addr; /* 7 bit i2c address*/
};

struct b52_cmd_i2c_data {
	const struct b52_sensor_i2c_attr *attr;
	struct regval_tab *tab;
	u32 num; /* the number of sensor regs*/
	u8 pos;
};

struct b52_sensor_data {
	char * const name;
	enum b52_sensor_type type;

	struct b52_sensor_spec_ops *ops;

	struct b52_sensor_i2c_attr *i2c_attr;
	int num_i2c_attr;
	struct b52_sensor_regs id;

	struct b52_sensor_regs global_setting;
	struct b52_sensor_mbus_fmt *mbus_fmt;
	int num_mbus_fmt;
	struct b52_sensor_resolution *res;
	int num_res;

	struct b52_sensor_regs streamon;
	struct b52_sensor_regs streamoff;
	struct b52_sensor_regs esd;
	/*
	 *gain unit 0x10: 1 gain = 0x10;
	 *the precision is for B52 ISP: Q4
	 *NOTE: MIN range >= 0x10
	 */
#define B52_GAIN_UNIT (0x10)
	struct sensor_prop_range gain_range[B52_SENSOR_GAIN_MAX];
	/*if numerator = 100, denominator = 0x10
	 * iso = gain * 100 / 0x10 */
	struct v4l2_fract gain2iso_ratio;

	/*vts unit intergration line*/
	struct sensor_prop_range vts_range;
	/*expo unit intergration line*/
	/*the precision is for B52 ISP: Q4*/
	/*NOTE: MAX range < def VTS*/
	struct sensor_prop_range expo_range;
	struct sensor_prop_range frationalexp_range;
	struct sensor_prop_range focus_range;

	struct b52_sensor_module *module;
	u32 num_module;

	struct b52_sensor_regs expo_reg;
	struct b52_sensor_regs frationalexp_reg;
	struct b52_sensor_regs vts_reg;
	struct b52_sensor_regs gain_reg[B52_SENSOR_GAIN_MAX];
	struct b52_sensor_regs af_reg;
	/* the bias must have 13 item*/
	int *ev_bias_offset;
	u8 dgain_channel;
	u8 gain_shift;
	u8 expo_shift;
	int calc_dphy;
	u8 nr_lane;
	u32 mipi_clk_bps;
	/*optional*/
	u32 skip_top_lines;
	u32 skip_frames;
	struct b52_sensor_regs hflip;
	struct b52_sensor_regs vflip;
	int flip_change_phase;
	int reset_delay;
};

struct b52_sensor_ops {
 /*pixel clk rate unit HZ, mclk unit HZ*/
	int (*get_pixel_rate)(struct v4l2_subdev *, u32 *rate, u32 mclk);
	int (*get_dphy_desc)(struct v4l2_subdev *,
			struct csi_dphy_desc *, u32 mclk);
	int (*update_otp)(struct v4l2_subdev *, struct b52_sensor_otp *);
/*below func does not need to implement for each sensor*/
	int (*init)(struct v4l2_subdev *);
	int (*get_power)(struct v4l2_subdev *);
	int (*put_power)(struct v4l2_subdev *);
	int (*i2c_read)(struct v4l2_subdev *, u16 addr, u32 *val, u8 num);
	int (*i2c_write)(struct v4l2_subdev *, u16 addr, u32 val, u8 num);
	int (*detect_sensor)(struct v4l2_subdev *);
	int (*detect_vcm)(struct v4l2_subdev *);
	int (*g_cur_fmt)(struct v4l2_subdev *, struct b52_cmd_i2c_data *);
	int (*g_vcm_info)(struct v4l2_subdev *, struct b52_sensor_vcm *);
	int (*gain_to_iso)(struct v4l2_subdev *, u32 gain, u32 *iso);
	int (*iso_to_gain)(struct v4l2_subdev *, u32 iso, u32 *gain);
	int (*to_expo_line)(struct v4l2_subdev *, u32 time, u32 *lines);
	int (*to_expo_time)(struct v4l2_subdev *, u32 *time, u32 lines);
	int (*g_cur_fps)(struct v4l2_subdev *, struct v4l2_fract *fps);
	int (*s_flip)(struct v4l2_subdev *, int hflip, int on);
	int (*g_band_step)(struct v4l2_subdev *,
			u16 *band_50hz, u16 *band_60hz);
	int (*g_param_range)(struct v4l2_subdev *,
			int type, u16 *min, u16 *max);
	int (*g_aecagc_reg)(struct v4l2_subdev *,
			int type, struct b52_sensor_regs *);
	int (*g_sensor_attr)(struct v4l2_subdev *,
			struct b52_sensor_i2c_attr *);
	int (*g_csi)(struct v4l2_subdev *, struct mipi_csi2 *);

	int (*s_focus)(struct v4l2_subdev *, u16 val);
	int (*g_focus)(struct v4l2_subdev *, u16 *val);
	int (*s_expo)(struct v4l2_subdev *, u32 expo, u16 vts);
	int (*s_gain)(struct v4l2_subdev *, u32 gain);
};

struct sensor_power {
	struct regulator *af_2v8;
	struct regulator *avdd_2v8;
	struct regulator *dovdd_1v8;
	struct regulator *dvdd_1v2;
	struct gpio_desc *pwdn;
	struct gpio_desc *rst;
	struct gpio_desc *dovdd_1v8_gpio;
	int ref_cnt;
};

struct b52_sensor_flash {
	enum v4l2_flash_led_mode led_mode;
	enum v4l2_flash_strobe_source strobe_source;
	u32 timeout;
	u32 flash_current;
	u32 torch_current;
	int flash_status;
};

struct b52isp_sensor_ctrls {
	struct v4l2_ctrl_handler ctrl_hdl;

	struct v4l2_ctrl *hflip;
	struct v4l2_ctrl *vflip;
};

struct b52_sensor {
	const struct b52_sensor_data *drvdata;

	struct b52_sensor_ops ops;
	struct v4l2_subdev sd;
	struct media_pad pad;
	struct device *dev;

	struct sensor_power power;
	enum b52_sensor_i2c_pos pos;

	struct b52_sensor_otp otp;

	u32 mclk;
	u32 pixel_rate;
	struct mipi_csi2 csi;

	struct b52_sensor_flash flash;

	struct b52isp_sensor_ctrls ctrls;

	int cur_mod_id;

	struct mutex lock; /* Protects streaming, format, interval*/
	struct v4l2_mbus_framefmt mf;
	struct b52_sensor_regs mf_regs;
	u8 cur_i2c_idx;
	u8 cur_res_idx;
	u8 cur_mbus_idx;
	struct clk *clk;
	int i2c_dyn_ctrl;
	int sensor_init;
	atomic_t	stream_cnt;
	int board_prop_id;
	int sensor_pos;/* 0 is back sensor, 1 is front sensor*/

	struct blocking_notifier_head nh;
};

extern struct b52_sensor *b52_get_sensor(struct media_entity *entity);
extern int b52_cmd_read_i2c(struct b52_cmd_i2c_data *data);
extern int b52_cmd_write_i2c(struct b52_cmd_i2c_data *data);
extern int b52_isp_read_i2c(const struct b52_sensor_i2c_attr *attr,
		u16 reg, u16 *val, u8 pos);
extern struct b52_sensor_data b52_ov5642;
extern struct b52_sensor *b52_get_sensor(struct media_entity *entity);
extern struct b52_sensor_data b52_ov13850_8M;
extern struct b52_sensor_data b52_ov13850_13M;
extern struct b52_sensor_data b52_ov13850r2a_13M;
extern struct b52_sensor_data b52_imx219;
extern struct b52_sensor_data b52_ov8858;
extern struct b52_sensor_data b52_ov5648;
extern struct b52_sensor_data b52_ov2680;
extern struct b52_sensor_data b52_sr544;
extern struct b52_sensor_data b52_hi551;
extern struct b52_sensor_data b52_ov5670;
extern struct b52_sensor_data b52_s5k3l2;
extern struct b52_sensor_data b52_s5k5e3;
extern struct b52_sensor_data b52_s5k4h5;
extern const struct b52_sensor_data *memory_sensor_match(char *sensor_name);
extern void b52_init_workqueue(void *data);

/* this two api for SSG J1 only*/
int b52isp_set_focus_distance(u32 distance, int id);
int check_load_firmware(void);

#define NORMAL_STATUS 0
#define VIDEO_TO_NORMAL 1
#define NORMAL_TO_VIDEO 2
#define VIDEO_TO_CALL 3
#define V4L2_CID_PRIVATE_B52_VIDEO_MODE \
	(V4L2_CID_CAMERA_CLASS_BASE + 0x1001)

#endif
