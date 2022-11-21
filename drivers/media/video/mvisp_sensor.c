/* Marvell ISP Sensor Driver
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
#include <linux/regulator/machine.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/fixed.h>

#include <asm/div64.h>
#include <media/v4l2-device.h>
#include <media/v4l2-chip-ident.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-ctrls.h>
#include <media/mvisp_sensor.h>
#include <mach/camera.h>
#include <mach/sensor-data.h>
#include <linux/module.h>

MODULE_DESCRIPTION("Marvell ISP sensor driver");
MODULE_AUTHOR("Henry Zhao <xzhao10@marvell.com>");
MODULE_LICENSE("GPL");

#define MAX_DETECT_NUM			3
#define MAX_SENSOR_PADS_NUM		1

#define SENSOR_PAD_SOURCE		0
unsigned char *sensor_pname;
static const struct sensor_datafmt sensor_colour_fmts[] = {
	{V4L2_MBUS_FMT_SBGGR10_1X10, V4L2_COLORSPACE_SRGB},
};

enum sensor_output_type {
	SENSOR_OUTPUT_NONE = 0x0,
	SENSOR_OUTPUT_CCIC = 0x1,
};
struct sensor_power {
	struct regulator *af_vcc;
	struct regulator *avdd;
	int pwdn;
	int rst;
};
struct sensor_core {
	struct v4l2_subdev		sd;
	struct v4l2_ctrl_handler	hdl;
	struct sensor_platform_data	*plat_data;
	struct media_pad pads[MAX_SENSOR_PADS_NUM];
	struct v4l2_mbus_framefmt formats[MAX_SENSOR_PADS_NUM];
	u32 width;
	u32 height;
	int cam_id;
	struct sensor_power		power;
	enum sensor_output_type		output;
	struct mutex			sensor_mutex;
	u8				openflag;
};

enum sensor_register_access_e {
	SENSOR_REG_INVALID = 0,
	SENSOR_REG_READ,
	SENSOR_REG_WRITE,
	SENSOR_REG_WRITE_L,
};

/* supported controls */
static struct v4l2_queryctrl sensor_qctrl[] = {
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



static inline struct sensor_core *to_sensor_core(struct v4l2_subdev *sd)
{
	return container_of(sd, struct sensor_core, sd);
}

static inline struct v4l2_subdev *to_sd(struct v4l2_ctrl *ctrl)
{
	return &container_of(ctrl->handler, struct sensor_core, hdl)->sd;
}

static int sensor_read(struct v4l2_subdev *sd, u16 reg, unsigned char *value)
{
	unsigned char msgbuf0[2];
	unsigned char msgbuf1[1];
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct i2c_adapter *adap = client->adapter;
	struct i2c_msg msg[2] = {{client->addr, client->flags, 2, msgbuf0},
				{client->addr, client->flags | I2C_M_RD,
					1, msgbuf1},
					};

	int num = 2, ret;

	if (value == NULL)
		return -EINVAL;

	msgbuf0[0] = (unsigned char)(reg>>8);
	msgbuf0[1] = (unsigned char)reg;

	ret = i2c_transfer(adap, msg, num);
	if (ret < 0)
		goto out;
	memcpy(value, msgbuf1, 1);

out:
	return (ret < 0) ? ret : 0;
}

static int sensor_write(struct v4l2_subdev *sd, u16 reg, unsigned char value)
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

	return 0;
}

static int sensor_write_l(struct v4l2_subdev *sd,
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
		for (i = 0; i < num; i++)
			data[i+2] = (unsigned char)(regs[i].value);
		ret = i2c_master_send(client, data, num+2);
		if (ret < 0)
			goto out_write;
	} else {
		for (i = 0; i < param->num; i++) {
			data[0] = regs[i].reg >> 8;
			data[1] = regs[i].reg;
			data[2] = (unsigned char)(regs[i].value);
			}

			ret = i2c_master_send(client, data, 3);
			if (ret < 0)
				goto out_write;
		}

	ret = 0;

out_write:
	kfree(data);
	return ret;
}

static int sensor_detect(struct v4l2_subdev *sd, struct i2c_client *client)
{
	struct sensor_platform_data *pdata = client->dev.platform_data;
	unsigned char v = 0;
	unsigned char *chip_name;
	int ret = 0;
	int i = 0;
	int j = 0;
	int detect_flag = 0;
	int num;
	if (!pdata)
		goto detect_sensor;
	num = pdata->sensor_num;
	for (i = 0; i < num; i++) {
		if (strcmp(pdata->sensor_cid[i].sensor_name,
			sensor_pname))
			continue;
		for (j = 0; j < pdata->sensor_cid[i].reg_num;
				j++) {
			ret = sensor_read(sd,
				pdata->sensor_cid[i].reg_pid[j], &v);
			if (ret < 0)
				break;
			if (v != pdata->sensor_cid[i].reg_pid_val[j])
				break;
			else if (v == pdata->sensor_cid[i].chip_id) {
				detect_flag = 1;
				chip_name = pdata->sensor_cid[i].sensor_name;
				goto detect_sensor;
			} else
				continue;
		}

	}

detect_sensor:
	if (detect_flag) {
		snprintf(sd->name, sizeof(sd->name), "%s %d-%04x",
				chip_name, i2c_adapter_id(client->adapter),
				client->addr);
		printk(KERN_ERR "cam: %s: sensor detected!\n", chip_name);
		return 1;
	} else {
		printk(KERN_ERR "cam: automatic detect sensor: Ignore the I2C errors\n");
		return 0;
	}
}

static int sensor_power_on(struct sensor_power *power,
			struct i2c_client *client, int sensor_id)
{
	struct sensor_platform_data *pdata = client->dev.platform_data;
	int ret = 0;

	if (sensor_id > pdata->sensor_num)
		goto error;

	power->rst = pdata->sensor_pwd[sensor_id].rst_gpio;
	power->pwdn = pdata->sensor_pwd[sensor_id].pwdn_gpio;
	if (power->pwdn) {
		if (gpio_request(power->pwdn, "CAM_ENABLE_LOW")) {
			ret = -EIO;
			goto error;
		}
	}
	if (power->rst) {
		if (gpio_request(power->rst, "CAM_RESET_LOW")) {
			ret = -EIO;
			goto out;
		}
	}
	if (pdata->sensor_pwd[sensor_id].afvcc) {
		if (!power->af_vcc) {
			power->af_vcc = regulator_get(&client->dev,
					pdata->sensor_pwd[sensor_id].afvcc);
			if (IS_ERR(power->af_vcc)) {
				ret = -EIO;
				goto out_af_vcc;
			}
		}
		regulator_set_voltage(power->af_vcc,
				pdata->sensor_pwd[sensor_id].afvcc_uV,
				pdata->sensor_pwd[sensor_id].afvcc_uV);
		regulator_enable(power->af_vcc);
	} else {
		if (power->af_vcc)
			regulator_put(power->af_vcc);
		power->af_vcc = NULL;
	}
	if (pdata->sensor_pwd[sensor_id].avdd) {
		if (!power->avdd) {
			power->avdd = regulator_get(&client->dev,
					pdata->sensor_pwd[sensor_id].avdd);
			if (IS_ERR(power->avdd)) {
				ret = -EIO;
				goto out_avdd;
			}
		}
		regulator_set_voltage(power->avdd,
				pdata->sensor_pwd[sensor_id].avdd_uV,
				pdata->sensor_pwd[sensor_id].avdd_uV);
		regulator_enable(power->avdd);
	} else {
		if (power->avdd)
			regulator_put(power->avdd);
		power->avdd = NULL;
	}
	/*    Enable voltage for Marvell ISP sensor*/
	gpio_direction_output(power->pwdn,
			pdata->sensor_pwd[sensor_id].pwdn_en);
	mdelay(1);
	gpio_direction_output(power->rst,
			!pdata->sensor_pwd[sensor_id].rst_en);
	mdelay(20);
	gpio_direction_output(power->rst,
			pdata->sensor_pwd[sensor_id].rst_en);
	mdelay(20);
	sensor_pname = pdata->sensor_pwd[sensor_id].sensor_name;
	return ret;

out_avdd:
	power->avdd = NULL;
	regulator_put(power->af_vcc);
out_af_vcc:
	power->af_vcc = NULL;
	gpio_free(power->rst);
out:
	gpio_free(power->pwdn);
error:
	printk(KERN_ERR "sensor cannot power on or sensor detect error!\n");

	return ret;
}

static void sensor_power_down(struct sensor_power *power,
		struct i2c_client *client, int sensor_id)
{
	struct sensor_platform_data *pdata = client->dev.platform_data;

	if (sensor_id > pdata->sensor_num)
		goto error;
	/*    Disable voltage for Marvell ISP sensor*/
	if (power->rst) {
		gpio_direction_output(power->rst,
			~(pdata->sensor_pwd[sensor_id].rst_en));
		gpio_free(power->rst);
	}
	if (power->af_vcc)
		regulator_disable(power->af_vcc);
	if (power->avdd)
		regulator_disable(power->avdd);
	if (power->pwdn) {
		gpio_direction_output(power->pwdn,
			~(pdata->sensor_pwd[sensor_id].pwdn_en));
		gpio_free(power->pwdn);
	}
	return;
error:
	printk(KERN_ERR "sensor cannot power on or sensor detect error!\n");
}

static int sensor_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct sensor_core *core = to_sensor_core(sd);
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

static int sensor_queryctrl(struct v4l2_subdev *sd, struct v4l2_queryctrl *qc)
{
	int i, ret = -EINVAL;
	struct sensor_core *core = to_sensor_core(sd);

	mutex_lock(&core->sensor_mutex);

	for (i = 0; i < ARRAY_SIZE(sensor_qctrl); i++)
		if (qc->id && qc->id == sensor_qctrl[i].id) {
			memcpy(qc, &(sensor_qctrl[i]),
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

static int sensor_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct v4l2_subdev *sd = to_sd(ctrl);
	struct sensor_core *core = to_sensor_core(sd);
	u8 i, n;
	int ret = 0;

	mutex_lock(&core->sensor_mutex);

	n = ARRAY_SIZE(sensor_qctrl);

	for (i = 0; i < n; i++) {
		if (ctrl->id != sensor_qctrl[i].id)
			continue;
		if (ctrl->val < sensor_qctrl[i].minimum ||
		    ctrl->val > sensor_qctrl[i].maximum) {
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

static int sensor_try_format(struct v4l2_subdev *sd,
		struct v4l2_mbus_framefmt *fmt)
{
	int ret = 0;

	fmt->field = V4L2_FIELD_NONE;

	switch (fmt->code) {
	case V4L2_MBUS_FMT_SBGGR10_1X10:
		fmt->colorspace = V4L2_COLORSPACE_SRGB;
		break;
	default:
		printk(KERN_ERR "sensor doesn't support code 0x%08X\n"
			, fmt->code);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int sensor_enum_mbus_code(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_mbus_code_enum *code)
{
	struct sensor_core *core = to_sensor_core(sd);
	int ret = 0;

	mutex_lock(&core->sensor_mutex);

	if (code->pad > 0) {
		ret = -EINVAL;
		goto error;
	}

	if (code->index >= ARRAY_SIZE(sensor_colour_fmts)) {
		ret = -EINVAL;
		goto error;
	}

	code->code = sensor_colour_fmts[code->index].code;

error:
	mutex_unlock(&core->sensor_mutex);

	return ret;
}

static int sensor_enum_frame_size(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_frame_size_enum *fse)
{
	struct v4l2_mbus_framefmt format;
	struct sensor_core *core = to_sensor_core(sd);
	int ret = 0;

	mutex_lock(&core->sensor_mutex);

	if (fse->index != 0) {
		ret = -EINVAL;
		goto error;
	}

	format.code = fse->code;
	format.width = 1;
	format.height = 1;
	sensor_try_format(sd, &format);

	fse->min_width = format.width;
	fse->min_height = format.height;

	if (format.code != fse->code) {
		ret = -EINVAL;
		goto error;
	}

	format.code = fse->code;
	format.width = -1;
	format.height = -1;

	sensor_try_format(sd, &format);
	fse->max_width = format.width;
	fse->max_height = format.height;

error:
	mutex_unlock(&core->sensor_mutex);
	return ret;
}

static int sensor_s_stream(struct v4l2_subdev *sd, int enable)
{
	int ret = 0;
	struct sensor_core *core = to_sensor_core(sd);

	mutex_lock(&core->sensor_mutex);
	dev_warn(sd->v4l2_dev->dev, "sensor_s_stream %d\n", enable);
	mutex_unlock(&core->sensor_mutex);

	return ret;
}

static int sensor_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	struct v4l2_captureparm *cp = &parms->parm.capture;
	struct sensor_core *core = to_sensor_core(sd);
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

static int sensor_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	struct v4l2_captureparm *cp = &parms->parm.capture;
	struct sensor_core *core = to_sensor_core(sd);
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

static struct v4l2_mbus_framefmt *__sensor_get_format(
			struct sensor_core *core, struct v4l2_subdev_fh *fh,
			unsigned int pad, enum v4l2_subdev_format_whence which)
{
	if (which == V4L2_SUBDEV_FORMAT_TRY)
		return v4l2_subdev_get_try_format(fh, pad);
	else
		return &core->formats[pad];
}

static int sensor_set_format(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_format *fmt)
{
	struct sensor_core *core = to_sensor_core(sd);
	struct v4l2_mbus_framefmt *format;
	int ret = 0;

	mutex_lock(&core->sensor_mutex);
	format = __sensor_get_format(core, fh, fmt->pad, fmt->which);
	if (format == NULL) {
		ret = -EINVAL;
		goto error;
	}

	if (fmt->pad > 0) {
		ret = -EINVAL;
		goto error;
	}


	ret = sensor_try_format(sd, &fmt->format);
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

static int sensor_get_format(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_format *fmt)
{
	struct sensor_core *core = to_sensor_core(sd);
	struct v4l2_mbus_framefmt *format;
	int ret = 0;

	mutex_lock(&core->sensor_mutex);

	format = __sensor_get_format(core, fh, fmt->pad, fmt->which);
	if (format == NULL) {
		ret = -EINVAL;
		goto error;
	}

	fmt->format = *format;

error:
	mutex_unlock(&core->sensor_mutex);

	return ret;
}

static int sensor_g_chip_ident(struct v4l2_subdev *sd,
				struct v4l2_dbg_chip_ident *chip)
{
	u16 version = 0;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct sensor_core *core = to_sensor_core(sd);
	int ret;

	mutex_lock(&core->sensor_mutex);

	ret = v4l2_chip_ident_i2c_client(client, chip,
			chip_ident_id[core->cam_id], version);

	mutex_unlock(&core->sensor_mutex);

	return ret;
}

static long sensor_get_driver_name(struct v4l2_subdev *sd,
		struct v4l2_sensor_get_driver_name *drv_name)
{
	if (!drv_name)
		return -EINVAL;

	strcpy(drv_name->driver, "sensor");
	return 0;
}

static long sensor_register_access(struct v4l2_subdev *sd,
			struct v4l2_sensor_register_access *param,
			enum sensor_register_access_e access_type)
{
	int ret = -EINVAL;

	switch (access_type) {
	case SENSOR_REG_READ:
		ret = sensor_read(sd, param->reg,
			(unsigned char *)&param->value);
		break;
	case SENSOR_REG_WRITE:
		ret = sensor_write(sd, param->reg,
			(unsigned char) param->value);
		break;
	default:
		break;
	}

	return ret;
}

static long sensor_register_access_l(struct v4l2_subdev *sd,
			struct v4l2_sensor_list_regs_access *param,
			enum sensor_register_access_e access_type)
{
	int ret = -EINVAL;

	switch (access_type) {
	case SENSOR_REG_WRITE_L:
		ret = sensor_write_l(sd, param);
		break;
	default:
		break;
	}

	return ret;
}

static long sensor_subdev_ioctl(struct v4l2_subdev *sd,
			unsigned int cmd, void *arg)
{
	int ret = -EINVAL;
	struct sensor_core *core = to_sensor_core(sd);

	mutex_lock(&core->sensor_mutex);
	switch (cmd) {
	case VIDIOC_PRIVATE_SENSER_REGISTER_GET:
		ret = sensor_register_access(sd,
			(struct v4l2_sensor_register_access *)arg,
			SENSOR_REG_READ);
		break;
	case VIDIOC_PRIVATE_SENSER_REGISTER_SET:
		ret = sensor_register_access(sd,
			(struct v4l2_sensor_register_access *)arg,
			SENSOR_REG_WRITE);
		break;
	case VIDIOC_PRIVATE_SENSER_REGS_LIST_SET:
		ret = sensor_register_access_l(sd,
			(struct v4l2_sensor_list_regs_access *)arg,
			SENSOR_REG_WRITE_L);
		break;
	case VIDIOC_PRIVATE_SENSER_GET_DRIVER_NAME:
		ret = sensor_get_driver_name(sd,
			(struct v4l2_sensor_get_driver_name *)arg);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	mutex_unlock(&core->sensor_mutex);
	return ret;
}

static int sensor_subdev_close(struct v4l2_subdev *sd,
		struct v4l2_subdev_fh *fh)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct sensor_core *core = to_sensor_core(sd);

	mutex_lock(&core->sensor_mutex);
	sensor_power_down(&core->power, client, core->cam_id);
	core->openflag = 0;
	mutex_unlock(&core->sensor_mutex);
	return 0;
}

static int sensor_subdev_open(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct sensor_core *core = to_sensor_core(sd);
	mutex_lock(&core->sensor_mutex);
	if (core->openflag == 1) {
		mutex_unlock(&core->sensor_mutex);
		return -EBUSY;
	}
	core->openflag = 1;
	sensor_power_on(&core->power, client, core->cam_id);

	mutex_unlock(&core->sensor_mutex);

	return 0;
}

static int sensor_link_setup(struct media_entity *entity,
			   const struct media_pad *local,
			   const struct media_pad *remote, u32 flags)
{
	struct v4l2_subdev *sd = media_entity_to_v4l2_subdev(entity);
	struct sensor_core *core = to_sensor_core(sd);
	int ret = 0;

	mutex_lock(&core->sensor_mutex);

	switch (local->index | media_entity_type(remote->entity)) {
	case SENSOR_PAD_SOURCE | MEDIA_ENT_T_V4L2_SUBDEV:
		if (flags & MEDIA_LNK_FL_ENABLED)
			core->output |= SENSOR_OUTPUT_CCIC;
		else
			core->output &= ~SENSOR_OUTPUT_CCIC;
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
	struct sensor_core *info = to_sensor_core(sd);
	v4l2_ctrl_handler_log_status(&info->hdl, sd->name);

	return 0;
}

static const struct v4l2_subdev_pad_ops sensor_pad_ops = {
	.enum_mbus_code = sensor_enum_mbus_code,
	.enum_frame_size = sensor_enum_frame_size,
	.get_fmt = sensor_get_format,
	.set_fmt = sensor_set_format,
};

static const struct v4l2_ctrl_ops sensor_ctrl_ops = {
	.s_ctrl = sensor_s_ctrl,
};

static const struct v4l2_subdev_core_ops sensor_core_ops = {
	.ioctl = sensor_subdev_ioctl,
	.log_status = sensor_log_status,
	.queryctrl = sensor_queryctrl,
	.g_ctrl = sensor_g_ctrl,
	.s_ctrl = v4l2_subdev_s_ctrl,
	.querymenu = v4l2_subdev_querymenu,
	.g_ext_ctrls = v4l2_subdev_g_ext_ctrls,
	.try_ext_ctrls = v4l2_subdev_try_ext_ctrls,
	.s_ext_ctrls = v4l2_subdev_s_ext_ctrls,
	.g_chip_ident = sensor_g_chip_ident,
};

static const struct v4l2_subdev_video_ops sensor_video_ops = {
	.g_parm = sensor_g_parm,
	.s_parm = sensor_s_parm,
	.s_stream = sensor_s_stream,
};

static const struct v4l2_subdev_ops sensor_ops = {
	.core  = &sensor_core_ops,
	.video = &sensor_video_ops,
	.pad = &sensor_pad_ops,
};

/* subdev internal operations */
static const struct v4l2_subdev_internal_ops sensor_v4l2_internal_ops = {
	.open = sensor_subdev_open,
	.close = sensor_subdev_close,
};

/* media operations */
static const struct media_entity_operations sensor_media_ops = {
	.link_setup = sensor_link_setup,
};


/****************************************************************************
			I2C Client & Driver
 ****************************************************************************/

static int sensor_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{

	int i, ret;
	struct sensor_core *core;
	struct v4l2_subdev *sd;
	struct sensor_platform_data *pdata = client->dev.platform_data;

	struct media_entity *me;
	struct media_pad *pads;

	int num = pdata->sensor_num;
	int flag = 0;
	if (!pdata)
		return -EINVAL;
	core = kzalloc(sizeof(struct sensor_core), GFP_KERNEL);
	if (!core)
		return -ENOMEM;

	mutex_init(&core->sensor_mutex);
	sd = &core->sd;
	v4l2_i2c_subdev_init(sd, client, &sensor_ops);
	sd->internal_ops = &sensor_v4l2_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	pads = core->pads;
	for (i = 0; i < num; i++) {
		sensor_power_on(&core->power, client, i);
		flag = sensor_detect(sd, client);
		sensor_power_down(&core->power, client, i);
		if (flag) {
			core->cam_id = i;
			break;
		}
	}
	if (!flag) {
		ret = -1;
		goto error;
	}
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
		goto sensor_flash_error;
	me = &sd->entity;
	pads[SENSOR_PAD_SOURCE].flags = MEDIA_PAD_FL_SOURCE;
	me->ops = &sensor_media_ops;
	ret = media_entity_init(me, MAX_SENSOR_PADS_NUM, pads, 0);
	if (ret < 0)
		goto sensor_media_init_error;
	core->plat_data = pdata;
	return 0;
sensor_media_init_error:
	media_entity_cleanup(&sd->entity);
sensor_flash_error:
	v4l2_ctrl_handler_free(&core->hdl);
error:
	if (core->power.avdd)
		regulator_put(core->power.avdd);
	if (core->power.af_vcc)
		regulator_put(core->power.af_vcc);
	v4l2_device_unregister_subdev(sd);

	kfree(core);

	return ret;

}

static int sensor_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sensor_core *core = to_sensor_core(sd);

	if (core->power.avdd)
		regulator_put(core->power.avdd);
	if (core->power.af_vcc)
		regulator_put(core->power.af_vcc);
	media_entity_cleanup(&sd->entity);
	v4l2_ctrl_handler_free(&core->hdl);
	v4l2_device_unregister_subdev(sd);
	kfree(core);
	return 0;
}

/* ----------------------------------------------------------------------- */

static const struct i2c_device_id sensor_id[] = {
	{ "sensor", 0 },
};
MODULE_DEVICE_TABLE(i2c, sensor_id);

static struct i2c_driver sensor_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "mrvisp sensor",
	},
	.probe		= sensor_probe,
	.remove		= sensor_remove,
	.id_table	= sensor_id,
};

static __init int init_sensor(void)
{
	return i2c_add_driver(&sensor_driver);
}

static __exit void exit_sensor(void)
{
	i2c_del_driver(&sensor_driver);
}

module_init(init_sensor);
module_exit(exit_sensor);
