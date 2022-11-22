/*
 *
 * Copyright (C) 2012 Marvell Internation Ltd.
 *
 * Chunlin Hu <huchl@marvell.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <media/dw9714l.h>
#include <media/v4l2-ctrls.h>
#include <linux/module.h>
#include <media/v4l2-device.h>
#include <linux/regulator/machine.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/mvisp.h>
struct v4l2_vcm_register_access {
	unsigned long	reg;
	unsigned long	value;
};
struct ad5823 {
	struct v4l2_subdev subdev;
	/* Controls */
	struct v4l2_ctrl_handler ctrls;
	struct regulator *af_vcc;
	int pwdn_gpio;
	char openflag;
};

#define to_ad5823(sd) container_of(sd, struct ad5823, subdev)
static int ad5823_write(struct i2c_client *client, u8 reg, u8 val)
{
	int ret = 0;
	u8 data[2];
	data[0] = reg;
	data[1] = val;
	ret = i2c_master_send(client, data, 2);
	if (ret < 0)
		dev_err(&client->dev,
			"ad5823:failed to write val-0x%02x\n", val);
	return (ret > 0) ? 0 : ret;
}

static int ad5823_read(struct i2c_client *client, u8 reg, u8 *val)
{
	int ret;

	ret = i2c_master_send(client, &reg, 1);
	if (ret < 0) {
		dev_err(&client->dev, "ad5823: failed to read\n");
		return ret;
	}
	ret = i2c_master_recv(client, val, 1);
	if (ret < 0) {
		dev_err(&client->dev, "ad5823: failed to read\n");
		return ret;
	}
	return 0;
}

static int ad5823_set_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;
	struct ad5823 *vcm =
		container_of(ctrl->handler, struct ad5823, ctrls);
	struct i2c_client *client = v4l2_get_subdevdata(&vcm->subdev);
	switch (ctrl->id) {
	case V4L2_CID_FOCUS_ABSOLUTE:
		ret = ad5823_write(client, 0x04, (u8)((ctrl->val)>>8)&03);
		ret |= ad5823_write(client, 0x05, (u8)ctrl->val);
		return ret;

	default:
		return -EPERM;
	}

}
static int ad5823_get_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;
	u8 data[2];
	struct ad5823 *vcm =
		container_of(ctrl->handler, struct ad5823, ctrls);
	struct i2c_client *client = v4l2_get_subdevdata(&vcm->subdev);
	switch (ctrl->id) {
	case V4L2_CID_FOCUS_ABSOLUTE:
		ret = ad5823_read(client, 0x04, (u8 *)&data[1]);
		ret |= ad5823_read(client, 0x05, (u8 *)&data[0]);
		ctrl->val = data[0] | ((data[1]&0x3) << 8);
		return ret;

	default:
		return -EPERM;
	}

}
static void vcm_power(struct ad5823 *vcm, int on)
{
	/* Enable voltage for camera sensor vcm */
	if (on) {
		regulator_set_voltage(vcm->af_vcc, 2800000, 2800000);
		regulator_enable(vcm->af_vcc);
	} else {
		regulator_disable(vcm->af_vcc);
	}
}
static int ad5823_subdev_close(struct v4l2_subdev *sd,
		struct v4l2_subdev_fh *fh)
{
	struct ad5823 *core = to_ad5823(sd);
	vcm_power(core, 0);
	core->openflag = 0;
	return 0;
}

static int ad5823_subdev_open(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh)
{
	struct ad5823 *core = to_ad5823(sd);
	if (core->openflag == 1)
		return -EBUSY;
	core->openflag = 1;
	vcm_power(core, 1);
	usleep_range(12000, 12000);
	v4l2_ctrl_handler_setup(&core->ctrls);
	return 0;
}
static long ad5823_subdev_ioctl(struct v4l2_subdev *sd,
			unsigned int cmd, void *arg)
{
	int ret = -EINVAL;
	struct v4l2_vcm_register_access *tmp =
				(struct v4l2_vcm_register_access *)arg;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	switch (cmd) {
	case VIDIOC_PRIVATE_SENSER_REGISTER_GET:
		tmp->value = 0;
		ret = ad5823_read(client,
			(u8)tmp->reg,
			(u8 *)&(tmp->value));
		break;
	case VIDIOC_PRIVATE_SENSER_REGISTER_SET:
		ret = ad5823_write(client,
			(u8)tmp->reg,
			(u8)tmp->value);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

static const struct v4l2_ctrl_ops ad5823_ctrl_ops = {
	.g_volatile_ctrl = ad5823_get_ctrl,
	.s_ctrl = ad5823_set_ctrl,
};

static int ad5823_init_controls(struct ad5823 *vcm)
{
	struct v4l2_ctrl *ctrl;
	v4l2_ctrl_handler_init(&vcm->ctrls, 1);
	/* V4L2_CID_FOCUS_ABSOLUTE */
	ctrl = v4l2_ctrl_new_std(&vcm->ctrls, &ad5823_ctrl_ops,
			 V4L2_CID_FOCUS_ABSOLUTE, 0, (1<<16)-1, 1, 0x8000);
	if (ctrl != NULL)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;
	vcm->subdev.ctrl_handler = &vcm->ctrls;

	return vcm->ctrls.error;
}
/* subdev internal operations */
static const struct v4l2_subdev_internal_ops ad5823_v4l2_internal_ops = {
	.open = ad5823_subdev_open,
	.close = ad5823_subdev_close,
};

static const struct v4l2_subdev_core_ops ad5823_core_ops = {
	.ioctl = ad5823_subdev_ioctl,
	.g_ext_ctrls = v4l2_subdev_g_ext_ctrls,
	.try_ext_ctrls = v4l2_subdev_try_ext_ctrls,
	.s_ext_ctrls = v4l2_subdev_s_ext_ctrls,
	.g_ctrl = v4l2_subdev_g_ctrl,
	.s_ctrl = v4l2_subdev_s_ctrl,
	.queryctrl = v4l2_subdev_queryctrl,
	.querymenu = v4l2_subdev_querymenu,
};

static const struct v4l2_subdev_ops ad5823_ops = {
	.core = &ad5823_core_ops,
};

static int __devinit ad5823_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	int ret = -1;
	u8 vcmid;
	int pwdn = 0;
	struct ad5823 *vcm;
	struct vcm_platform_data *pdata = client->dev.platform_data;
	vcm = kzalloc(sizeof(struct ad5823), GFP_KERNEL);
	if (!vcm) {
		dev_err(&client->dev, "ad5823: failed to allocate ad5823 struct\n");
		return -ENOMEM;
	}
	if (!pdata)
		return -EIO;
	if (pdata->power_domain) {
		vcm->af_vcc = regulator_get(&client->dev,
					pdata->power_domain);
		if (IS_ERR(vcm->af_vcc))
			goto out_af_vcc;

		vcm_power(vcm, 1);
	}

	if (pdata->pwdn_gpio) {
		vcm->pwdn_gpio = pdata->pwdn_gpio;
		if (gpio_request(vcm->pwdn_gpio, "VCM_ENABLE_LOW"))  {
			printk(KERN_ERR "VCM:can't get the gpio resource!\n");
			ret = -EIO;
			goto out_gpio;
		}
		pwdn = gpio_get_value(vcm->pwdn_gpio);
		gpio_direction_output(vcm->pwdn_gpio, pdata->pwdn_en);
	}
	usleep_range(12000, 12000);
	if (ad5823_read(client, 0x01, &vcmid)) {
		printk(KERN_ERR "VCM: ad5823 vcm detect failure!\n");
		goto out_pwdn;
	}
	if (vcm->pwdn_gpio) {
		gpio_direction_output(vcm->pwdn_gpio, pwdn);
		gpio_free(vcm->pwdn_gpio);
	}
	if (vcm->af_vcc)
		vcm_power(vcm, 0);
	v4l2_i2c_subdev_init(&vcm->subdev, client, &ad5823_ops);
	vcm->subdev.internal_ops = &ad5823_v4l2_internal_ops;
	vcm->subdev.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	ret = ad5823_init_controls(vcm);
	if (ret < 0)
		goto out;
	ret = media_entity_init(&vcm->subdev.entity, 0, NULL, 0);
	if (ret < 0)
		goto out;
	printk(KERN_INFO "ad5823 detected!\n");
	return 0;

out:
	v4l2_ctrl_handler_free(&vcm->ctrls);
out_pwdn:
	if (vcm->pwdn_gpio) {
		gpio_direction_output(vcm->pwdn_gpio, pwdn);
		gpio_free(vcm->pwdn_gpio);
	}
out_gpio:
	if (vcm->af_vcc)
		vcm_power(vcm, 0);
out_af_vcc:
	regulator_put(vcm->af_vcc);
	vcm->af_vcc = NULL;
	kfree(vcm);
	return ret;
}
static int ad5823_remove(struct i2c_client *client)
{
	struct v4l2_subdev *subdev = i2c_get_clientdata(client);
	struct ad5823 *vcm = to_ad5823(subdev);

	v4l2_device_unregister_subdev(subdev);
	v4l2_ctrl_handler_free(&vcm->ctrls);
	media_entity_cleanup(&vcm->subdev.entity);
	kfree(vcm);
	return 0;
}

static const struct i2c_device_id ad5823_id[] = {
	{"ad5823", 0},
};

MODULE_DEVICE_TABLE(i2c, ad5823_id);

static struct i2c_driver ad5823_driver = {
	.driver = {
		   .owner = THIS_MODULE,
		   .name = "vcm:ov12830<ad5823>",
		   },
	.probe = ad5823_probe,
	.remove = ad5823_remove,
	.id_table = ad5823_id,
};

static int __init ad5823_mod_init(void)
{
	return i2c_add_driver(&ad5823_driver);
}

module_init(ad5823_mod_init);

static void __exit ad5823_mode_exit(void)
{
	i2c_del_driver(&ad5823_driver);
}

module_exit(ad5823_mode_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Chunlin Hu <huchl@marvell.com>");
MODULE_DESCRIPTION("ad5823 vcm driver");

