/* OmniVision OV882x Sensor Driver
 *
 * Copyright (C) 2009-2010 Marvell International Ltd.
 *
 * Based on mt9v011 -Micron 1/4-Inch VGA Digital Image Sensor
 *
 * Copyright (c) 2009 Mauro Carvalho Chehab (mchehab@redhat.com)
 * This code is placed under the terms of the GNU General Public License v2
 */

#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/videodev2.h>
#include <linux/delay.h>
#include <linux/mvisp.h>
#include <linux/slab.h>
#include <linux/gpio.h>

#include <asm/div64.h>
#include <media/v4l2-device.h>
#include <media/v4l2-chip-ident.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-ctrls.h>
#include <media/ov882x.h>
#include <mach/camera.h>
#include <linux/module.h>

MODULE_DESCRIPTION("ov882x sensor driver");
MODULE_AUTHOR("Henry Zhao <xzhao10@marvell.com>");
MODULE_LICENSE("GPL");

#define MAX_DETECT_NUM			3
#define MAX_OV882X_PADS_NUM		1

#define OV882X_PAD_SOURCE		0
#define SENSOR_OPEN 1
#define SENSOR_CLOSE 0

static const struct ov882x_datafmt ov882x_colour_fmts[] = {
	{V4L2_MBUS_FMT_SBGGR10_1X10, V4L2_COLORSPACE_SRGB},
};

enum ov882x_output_type {
	OV882X_OUTPUT_NONE = 0x0,
	OV882X_OUTPUT_CCIC = 0x1,
};

struct ov882x_core {
	struct v4l2_subdev			sd;
	struct v4l2_ctrl_handler	hdl;
	struct sensor_platform_data	*plat_data;
	struct media_pad pads[MAX_OV882X_PADS_NUM];
	struct v4l2_mbus_framefmt formats[MAX_OV882X_PADS_NUM];
	u32 width;
	u32 height;
	enum ov882x_output_type	output;
	struct mutex			sensor_mutex;
};

enum ov882x_register_access_e {
	OV882X_REG_INVALID = 0,
	OV882X_REG_READ,
	OV882X_REG_WRITE,
	OV882X_REG_WRITE_L,
};

/* supported controls */
static struct v4l2_queryctrl ov882x_qctrl[] = {
	{
		.id = V4L2_CID_GAIN,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Gain",
		.minimum = 0,
		.maximum = (1 << 10) - 1,
		.step = 1,
		.default_value = 0x0020,
		.flags = 0,
	}, {
		.id = V4L2_CID_RED_BALANCE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Red Balance",
		.minimum = -1 << 9,
		.maximum = (1 << 9) - 1,
		.step = 1,
		.default_value = 0,
		.flags = 0,
	}, {
		.id = V4L2_CID_BLUE_BALANCE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Blue Balance",
		.minimum = -1 << 9,
		.maximum = (1 << 9) - 1,
		.step = 1,
		.default_value = 0,
		.flags = 0,
	}, {
		.id      = V4L2_CID_HFLIP,
		.type    = V4L2_CTRL_TYPE_BOOLEAN,
		.name    = "Mirror",
		.minimum = 0,
		.maximum = 1,
		.step    = 1,
		.default_value = 0,
		.flags = 0,
	}, {
		.id      = V4L2_CID_VFLIP,
		.type    = V4L2_CTRL_TYPE_BOOLEAN,
		.name    = "Vflip",
		.minimum = 0,
		.maximum = 1,
		.step    = 1,
		.default_value = 0,
		.flags = 0,
	}, {
	}
};



static inline struct ov882x_core *to_ov882x_core(struct v4l2_subdev *sd)
{
	return container_of(sd, struct ov882x_core, sd);
}

static inline struct v4l2_subdev *to_sd(struct v4l2_ctrl *ctrl)
{
	return &container_of(ctrl->handler, struct ov882x_core, hdl)->sd;
}

static int ov882x_read(struct v4l2_subdev *sd, u16 reg, unsigned char *value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = 0;
	u8 data;
	u8 address[2];

	address[0] = reg>>8;
	address[1] = reg;
	ret = i2c_smbus_write_byte_data(client, address[0], address[1]);
	if (ret)
		return ret;
	data = i2c_smbus_read_byte(client);

	*value = data;

	return 0;
}

static int ov882x_write(struct v4l2_subdev *sd, u16 reg, unsigned char value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = 0;
	u8 data[3];

	data[0] = reg >> 8;
	data[1] = reg;
	data[2] =  value;
	ret = i2c_master_send(client, data, 3);
	if (ret < 0)
		return ret;
	if (reg == REG_RESET && (value == RESET_ACT))
		msleep(20); /* Wait for reset to run */

	return 0;
}

static int ov882x_write_l(struct v4l2_subdev *sd,
			struct v4l2_sensor_list_regs_access *param)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = 0;
	int num;
	int i;
	u8 *data;
	struct v4l2_sensor_register_access *regs;

	if (!param)
		return -EINVAL;
	regs = param->regs;

	if (param->continuous > 0) {
		num = param->num;
		if (num < 1)
			return -EINVAL;
	} else {
		num = 1;
	}
	data = kmalloc((num+2) * sizeof(u8), GFP_KERNEL);
	if (!data)
		return -EINVAL;

	if (param->continuous > 0) {
		data[0] = regs[0].reg >> 8;
		data[1] = regs[0].reg;

		for (i = 0; i < num; i++) {
			if (regs[i].reg == REG_RESET &&
					regs[i].value == RESET_ACT) {
				ret = -EINVAL;
				goto out_write;
			}
			data[i+2] = (unsigned char)(regs[i].value);
		}
		ret = i2c_master_send(client, data, num+2);
		if (ret < 0)
			goto out_write;
	} else {
		for (i = 0; i < param->num; i++) {
			data[0] = regs[i].reg >> 8;
			data[1] = regs[i].reg;
			data[2] = (unsigned char)(regs[i].value);

			if (regs[i].reg == REG_RESET &&
					regs[i].value == RESET_ACT) {
				ret = -EINVAL;
				goto out_write;
			}
			ret = i2c_master_send(client, data, 3);
			if (ret < 0)
				goto out_write;
		}
	}

	ret = 0;

out_write:
	kfree(data);
	return ret;
}

static int ov882x_detect(struct v4l2_subdev *sd, struct i2c_client *client)
{
	unsigned char v = 0;
	int ret = 0;

	ret = ov882x_read(sd, REG_PIDH, &v);
	if (ret < 0) {
		printk(KERN_NOTICE "cam: ov882x: Sensor not mounted / I2C address error\n");
		return -ENXIO;
	}
	if (v != REG_PIDH_VALUE) {
		printk(KERN_ERR "cam: ov882x: Not a OV882x sensor: ID_HIGH = 0x%X\n"
			, (unsigned char)v);
		return -ENODEV;
	}

	ret = ov882x_read(sd, REG_PIDM, &v);
	if (ret < 0) {
		printk(KERN_NOTICE "cam: ov882x: Sensor not mounted / I2C address error\n");
		return -ENXIO;
	}
	if (v != REG_PIDM_VALUE_8820 && v != REG_PIDM_VALUE_8825) {
		printk(KERN_ERR "cam: ov882x: Not a OV882x sensor: ID_LOW = 0x%X\n"
			, (unsigned char)v);
		return -ENODEV;
	} else {
		snprintf(sd->name, sizeof(sd->name), "%s %d-%04x",
			client->name, i2c_adapter_id(client->adapter),
			client->addr);

		printk(KERN_ERR "cam: %s: sensor detected!\n", client->name);
	}

	return 0;
}

static int ov882x_reset(struct v4l2_subdev *sd, u32 val)
{
	int ret = 0;
	struct ov882x_core *core = to_ov882x_core(sd);

	mutex_lock(&core->sensor_mutex);

	ret = ov882x_write(sd, REG_RESET, RESET_ACT);
	printk(KERN_NOTICE "cam: ov882x: Reset sensor\n");
	msleep(20);
	ret |= ov882x_write(sd, REG_RESET, RESET_DIS);

	mutex_unlock(&core->sensor_mutex);
	return (ret < 0);
};

static int ov882x_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct ov882x_core *core = to_ov882x_core(sd);
	int ret = 0;

	mutex_lock(&core->sensor_mutex);
	switch (ctrl->id) {
	case V4L2_CID_GAIN:
		break;
	case V4L2_CID_RED_BALANCE:
		break;
	case V4L2_CID_BLUE_BALANCE:
		break;
	case V4L2_CID_HFLIP:
		break;
	case V4L2_CID_VFLIP:
		break;
	default:
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&core->sensor_mutex);
	return ret;
}

static int ov882x_queryctrl(struct v4l2_subdev *sd, struct v4l2_queryctrl *qc)
{
	int i, ret = -EINVAL;
	struct ov882x_core *core = to_ov882x_core(sd);

	mutex_lock(&core->sensor_mutex);

	for (i = 0; i < ARRAY_SIZE(ov882x_qctrl); i++)
		if (qc->id && qc->id == ov882x_qctrl[i].id) {
			memcpy(qc, &(ov882x_qctrl[i]),
			       sizeof(*qc));
			ret = 0;
			break;
		}

	mutex_unlock(&core->sensor_mutex);
	return ret;
}

static int sensor_control_flash(struct v4l2_subdev *sd,
		int on)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct sensor_platform_data *pdata = client->dev.platform_data;

	if (pdata) {
		int flash_on = pdata->flash_enable;
		if (flash_on >= 0) {
			if (gpio_request(flash_on, "SENSOR_SET_POWER_ON")) {
				printk(KERN_ERR "Request GPIO failed,gpio:%d\n",
						flash_on);
				return -EIO;
			}

			gpio_direction_output(flash_on, on);

			gpio_free(flash_on);
		} else
			printk(KERN_ERR "GPIO is invalid");
	}

	return 0;
}

static int ov882x_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct v4l2_subdev *sd = to_sd(ctrl);
	struct ov882x_core *core = to_ov882x_core(sd);
	u8 i, n;
	int ret = 0;

	mutex_lock(&core->sensor_mutex);

	n = ARRAY_SIZE(ov882x_qctrl);

	for (i = 0; i < n; i++) {
		if (ctrl->id != ov882x_qctrl[i].id)
			continue;
		if (ctrl->val < ov882x_qctrl[i].minimum ||
		    ctrl->val > ov882x_qctrl[i].maximum) {
			ret = -ERANGE;
			goto error;
		}
	}

	switch (ctrl->id) {
	case V4L2_CID_FLASH_STROBE_STOP:
		ret = sensor_control_flash(sd, 0);
		break;
	case V4L2_CID_FLASH_STROBE:
		ret = sensor_control_flash(sd, 1);
		break;
	default:
		ret = -EINVAL;
		break;
	}

error:
	mutex_unlock(&core->sensor_mutex);

	return ret;
}

static int ov882x_try_format(struct v4l2_subdev *sd,
		struct v4l2_mbus_framefmt *fmt)
{
	int ret = 0;

	fmt->field = V4L2_FIELD_NONE;

	switch (fmt->code) {
	case V4L2_MBUS_FMT_SBGGR10_1X10:
		fmt->colorspace = V4L2_COLORSPACE_SRGB;
		break;
	default:
		printk(KERN_ERR "ov882x doesn't support code 0x%08X\n"
			, fmt->code);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int ov882x_enum_mbus_code(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_mbus_code_enum *code)
{
	struct ov882x_core *core = to_ov882x_core(sd);
	int ret = 0;

	mutex_lock(&core->sensor_mutex);

	if (code->pad > 0) {
		ret = -EINVAL;
		goto error;
	}

	if (code->index >= ARRAY_SIZE(ov882x_colour_fmts)) {
		ret = -EINVAL;
		goto error;
	}

	code->code = ov882x_colour_fmts[code->index].code;

error:
	mutex_unlock(&core->sensor_mutex);

	return ret;
}

static int ov882x_enum_frame_size(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_frame_size_enum *fse)
{
	struct v4l2_mbus_framefmt format;
	struct ov882x_core *core = to_ov882x_core(sd);
	int ret = 0;

	mutex_lock(&core->sensor_mutex);

	if (fse->index != 0) {
		ret = -EINVAL;
		goto error;
	}

	format.code = fse->code;
	format.width = 1;
	format.height = 1;
	ov882x_try_format(sd, &format);

	fse->min_width = format.width;
	fse->min_height = format.height;

	if (format.code != fse->code) {
		ret = -EINVAL;
		goto error;
	}

	format.code = fse->code;
	format.width = -1;
	format.height = -1;

	ov882x_try_format(sd, &format);
	fse->max_width = format.width;
	fse->max_height = format.height;

error:
	mutex_unlock(&core->sensor_mutex);
	return ret;
}

static int ov882x_s_stream(struct v4l2_subdev *sd, int enable)
{
	int ret = 0;
	struct ov882x_core *core = to_ov882x_core(sd);

	mutex_lock(&core->sensor_mutex);
#if 0
	if (!enable)
		ret = ov882x_write(sd, 0x0100, 0x00);
#endif
	dev_warn(sd->v4l2_dev->dev, "ov882x_s_stream %d\n", enable);
	mutex_unlock(&core->sensor_mutex);

	return ret;
}

static int ov882x_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	struct v4l2_captureparm *cp = &parms->parm.capture;
	struct ov882x_core *core = to_ov882x_core(sd);
	int ret = 0;
	mutex_lock(&core->sensor_mutex);
	if (parms->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		ret = -EINVAL;
		goto error;
	}

	memset(cp, 0, sizeof(struct v4l2_captureparm));
	cp->capability = V4L2_CAP_TIMEPERFRAME;

error:
	mutex_unlock(&core->sensor_mutex);

	return ret;
}

static int ov882x_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	struct v4l2_captureparm *cp = &parms->parm.capture;
	struct ov882x_core *core = to_ov882x_core(sd);
	int ret = 0;
	mutex_lock(&core->sensor_mutex);
	if (parms->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		ret = -EINVAL;
		goto error;
	}
	if (cp->extendedmode != 0) {
		ret = -EINVAL;
		goto error;
	}

error:
	mutex_unlock(&core->sensor_mutex);

	return ret;
}

static struct v4l2_mbus_framefmt *__ov882x_get_format(
			struct ov882x_core *core, struct v4l2_subdev_fh *fh,
			unsigned int pad, enum v4l2_subdev_format_whence which)
{
	if (which == V4L2_SUBDEV_FORMAT_TRY)
		return v4l2_subdev_get_try_format(fh, pad);
	else
		return &core->formats[pad];
}

static int ov882x_set_format(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_format *fmt)
{
	struct ov882x_core *core = to_ov882x_core(sd);
	struct v4l2_mbus_framefmt *format;
	int ret = 0;

	mutex_lock(&core->sensor_mutex);
	format = __ov882x_get_format(core, fh, fmt->pad, fmt->which);
	if (format == NULL) {
		ret = -EINVAL;
		goto error;
	}

	if (fmt->pad > 0) {
		ret = -EINVAL;
		goto error;
	}


	ret = ov882x_try_format(sd, &fmt->format);
	if (ret < 0)
		goto error;

	*format = fmt->format;

	if (fmt->which != V4L2_SUBDEV_FORMAT_TRY) {
		core->width = fmt->format.width;
		core->height = fmt->format.height;
	}

error:
	mutex_unlock(&core->sensor_mutex);

	return ret;
}

static int ov882x_get_format(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_format *fmt)
{
	struct ov882x_core *core = to_ov882x_core(sd);
	struct v4l2_mbus_framefmt *format;
	int ret = 0;

	mutex_lock(&core->sensor_mutex);

	format = __ov882x_get_format(core, fh, fmt->pad, fmt->which);
	if (format == NULL) {
		ret = -EINVAL;
		goto error;
	}

	fmt->format = *format;

error:
	mutex_unlock(&core->sensor_mutex);

	return ret;
}

static int ov882x_g_chip_ident(struct v4l2_subdev *sd,
				struct v4l2_dbg_chip_ident *chip)
{
	u16 version = 0;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov882x_core *core = to_ov882x_core(sd);
	int ret;

	mutex_lock(&core->sensor_mutex);

	ret = v4l2_chip_ident_i2c_client(client, chip,
			V4L2_IDENT_OV882X, version);

	mutex_unlock(&core->sensor_mutex);

	return ret;
}

static long ov882x_get_driver_name(struct v4l2_subdev *sd,
		struct v4l2_sensor_get_driver_name *drv_name)
{
	if (!drv_name)
		return -EINVAL;

	strcpy(drv_name->driver, "ov882x");
	return 0;
}

static long ov882x_register_access(struct v4l2_subdev *sd,
			struct v4l2_sensor_register_access *param,
			enum ov882x_register_access_e access_type)
{
	int ret = -EINVAL;

	switch (access_type) {
	case OV882X_REG_READ:
		ret = ov882x_read(sd, param->reg,
			(unsigned char *)&param->value);
		break;
	case OV882X_REG_WRITE:
		ret = ov882x_write(sd, param->reg,
			(unsigned char) param->value);
		break;
	default:
		break;
	}

	return ret;
}

static long ov882x_register_access_l(struct v4l2_subdev *sd,
			struct v4l2_sensor_list_regs_access *param,
			enum ov882x_register_access_e access_type)
{
	int ret = -EINVAL;

	switch (access_type) {
	case OV882X_REG_WRITE_L:
		ret = ov882x_write_l(sd, param);
		break;
	default:
		break;
	}

	return ret;
}

static long ov882x_subdev_ioctl(struct v4l2_subdev *sd,
			unsigned int cmd, void *arg)
{
	int ret = -EINVAL;
	struct ov882x_core *core = to_ov882x_core(sd);

	mutex_lock(&core->sensor_mutex);
	switch (cmd) {
	case VIDIOC_PRIVATE_SENSER_REGISTER_GET:
		ret = ov882x_register_access(sd,
			(struct v4l2_sensor_register_access *)arg,
			OV882X_REG_READ);
		break;
	case VIDIOC_PRIVATE_SENSER_REGISTER_SET:
		ret = ov882x_register_access(sd,
			(struct v4l2_sensor_register_access *)arg,
			OV882X_REG_WRITE);
		break;
	case VIDIOC_PRIVATE_SENSER_REGS_LIST_SET:
		ret = ov882x_register_access_l(sd,
			(struct v4l2_sensor_list_regs_access *)arg,
			OV882X_REG_WRITE_L);
		break;
	case VIDIOC_PRIVATE_SENSER_GET_DRIVER_NAME:
		ret = ov882x_get_driver_name(sd,
			(struct v4l2_sensor_get_driver_name *)arg);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	mutex_unlock(&core->sensor_mutex);
	return ret;
}

static int ov882x_subdev_close(struct v4l2_subdev *sd,
		struct v4l2_subdev_fh *fh)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct sensor_platform_data *pdata = client->dev.platform_data;
	struct ov882x_core *core = to_ov882x_core(sd);

	mutex_lock(&core->sensor_mutex);

	if (pdata)
		if (pdata->power_on)
			pdata->power_on(SENSOR_CLOSE);

	mutex_unlock(&core->sensor_mutex);

	return 0;
}

static int ov882x_subdev_open(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct sensor_platform_data *pdata = client->dev.platform_data;
	struct ov882x_core *core = to_ov882x_core(sd);

	mutex_lock(&core->sensor_mutex);

	if (pdata)
		if (pdata->power_on)
			pdata->power_on(SENSOR_OPEN);

	mutex_unlock(&core->sensor_mutex);

	return 0;
}

static int ov882x_link_setup(struct media_entity *entity,
			   const struct media_pad *local,
			   const struct media_pad *remote, u32 flags)
{
	struct v4l2_subdev *sd = media_entity_to_v4l2_subdev(entity);
	struct ov882x_core *core = to_ov882x_core(sd);
	int ret = 0;

	mutex_lock(&core->sensor_mutex);

	switch (local->index | media_entity_type(remote->entity)) {
	case OV882X_PAD_SOURCE | MEDIA_ENT_T_V4L2_SUBDEV:
		if (flags & MEDIA_LNK_FL_ENABLED)
			core->output |= OV882X_OUTPUT_CCIC;
		else
			core->output &= ~OV882X_OUTPUT_CCIC;
		break;
	default:
		/* Link from camera to CCIC is fixed... */
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&core->sensor_mutex);

	return ret;
}

static int sensor_log_status(struct v4l2_subdev *sd)
{
	struct ov882x_core *info = to_ov882x_core(sd);
	v4l2_ctrl_handler_log_status(&info->hdl, sd->name);

	return 0;
}

static const struct v4l2_subdev_pad_ops ov882x_pad_ops = {
	.enum_mbus_code = ov882x_enum_mbus_code,
	.enum_frame_size = ov882x_enum_frame_size,
	.get_fmt = ov882x_get_format,
	.set_fmt = ov882x_set_format,
};

static const struct v4l2_ctrl_ops sensor_ctrl_ops = {
	.s_ctrl = ov882x_s_ctrl,
};

static const struct v4l2_subdev_core_ops ov882x_core_ops = {
	.ioctl = ov882x_subdev_ioctl,
	.log_status = sensor_log_status,
	.queryctrl = ov882x_queryctrl,
	.g_ctrl = ov882x_g_ctrl,
	.s_ctrl = v4l2_subdev_s_ctrl,
	.querymenu = v4l2_subdev_querymenu,
	.g_ext_ctrls = v4l2_subdev_g_ext_ctrls,
	.try_ext_ctrls = v4l2_subdev_try_ext_ctrls,
	.s_ext_ctrls = v4l2_subdev_s_ext_ctrls,
	.reset = ov882x_reset,
	.g_chip_ident = ov882x_g_chip_ident,
};

static const struct v4l2_subdev_video_ops ov882x_video_ops = {
	.g_parm = ov882x_g_parm,
	.s_parm = ov882x_s_parm,
	.s_stream = ov882x_s_stream,
};

static const struct v4l2_subdev_ops ov882x_ops = {
	.core  = &ov882x_core_ops,
	.video = &ov882x_video_ops,
	.pad = &ov882x_pad_ops,
};

/* subdev internal operations */
static const struct v4l2_subdev_internal_ops ov882x_v4l2_internal_ops = {
	.open = ov882x_subdev_open,
	.close = ov882x_subdev_close,
};

/* media operations */
static const struct media_entity_operations ov882x_media_ops = {
	.link_setup = ov882x_link_setup,
};


/****************************************************************************
			I2C Client & Driver
 ****************************************************************************/

static int ov882x_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	int i, ret;
	struct ov882x_core *core;
	struct v4l2_subdev *sd;
	struct sensor_platform_data *pdata = client->dev.platform_data;

	struct media_entity *me;
	struct media_pad *pads;

	if (!pdata)
		return -EINVAL;

	/* Check if the adapter supports the needed features */
	if (!i2c_check_functionality(client->adapter,
	     I2C_FUNC_SMBUS_READ_BYTE | I2C_FUNC_SMBUS_WRITE_BYTE_DATA))
		return -EIO;

	core = kzalloc(sizeof(struct ov882x_core), GFP_KERNEL);
	if (!core)
		return -ENOMEM;

	mutex_init(&core->sensor_mutex);

	if (pdata->power_on) {
		ret = pdata->power_on(SENSOR_OPEN);
		msleep(20);
	}

	sd = &core->sd;
	v4l2_i2c_subdev_init(sd, client, &ov882x_ops);
	sd->internal_ops = &ov882x_v4l2_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	pads = core->pads;

	v4l2_ctrl_handler_init(&core->hdl, 3);

	v4l2_ctrl_new_std(&core->hdl, &sensor_ctrl_ops,
			V4L2_CID_FLASH_CLASS, 0, 0, 0, 0);
	v4l2_ctrl_new_std(&core->hdl, &sensor_ctrl_ops,
			V4L2_CID_FLASH_STROBE, 0, 1, 1, 0);
	v4l2_ctrl_new_std(&core->hdl, &sensor_ctrl_ops,
			V4L2_CID_FLASH_STROBE_STOP, 0, 1, 1, 0);
	sd->ctrl_handler = &core->hdl;

	ret = core->hdl.error;
	if (ret)
		goto ov882x_flash_error;

	me = &sd->entity;
	pads[OV882X_PAD_SOURCE].flags = MEDIA_PAD_FL_SOURCE;
	me->ops = &ov882x_media_ops;
	ret = media_entity_init(me, MAX_OV882X_PADS_NUM, pads, 0);
	if (ret < 0)
		goto ov882x_media_init_error;
	/* Check if the sensor is really a OV882x */
	for (i = MAX_DETECT_NUM; i > 0; --i) {
		ret = ov882x_detect(sd, client);
		if (!ret)
			break;

		if (ret == -ENXIO) {
			goto ov882x_detect_error;
		} else {
			printk(KERN_ERR \
				"camera: OV882x sensor detect failure, will retry %d times!\n"
				, i);
		}
	}
	if (i == 0)
		goto ov882x_detect_error;

	core->plat_data = pdata;

	if (pdata->power_on)
		pdata->power_on(SENSOR_CLOSE);

	return 0;

ov882x_detect_error:
	media_entity_cleanup(&sd->entity);
ov882x_flash_error:
	v4l2_ctrl_handler_free(&core->hdl);
	v4l2_device_unregister_subdev(sd);
ov882x_media_init_error:
	kfree(core);
	if (pdata->power_on)
		pdata->power_on(SENSOR_CLOSE);
	return ret;
}

static int ov882x_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sensor_platform_data *pdata = client->dev.platform_data;
	struct ov882x_core *core = to_ov882x_core(sd);

	if (pdata)
		if (pdata->power_on)
			pdata->power_on(SENSOR_CLOSE);

	media_entity_cleanup(&sd->entity);
	v4l2_ctrl_handler_free(&core->hdl);
	v4l2_device_unregister_subdev(sd);
	kfree(core);
	return 0;
}

/* ----------------------------------------------------------------------- */

static const struct i2c_device_id ov882x_id[] = {
	{ "ov8820", 0 },
	{ "ov8825", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ov882x_id);

static struct i2c_driver ov882x_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "ov882x",
	},
	.probe		= ov882x_probe,
	.remove		= ov882x_remove,
	.id_table	= ov882x_id,
};

static __init int init_ov882x(void)
{
	return i2c_add_driver(&ov882x_driver);
}

static __exit void exit_ov882x(void)
{
	i2c_del_driver(&ov882x_driver);
}

module_init(init_ov882x);
module_exit(exit_ov882x);
