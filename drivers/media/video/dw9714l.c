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
struct dw9714l {
	struct v4l2_subdev subdev;
	/* Controls */
	struct v4l2_ctrl_handler ctrls;
	struct regulator *af_vcc;
	int pwdn_gpio;
	char openflag;
};

#define to_dw9714l(sd) container_of(sd, struct dw9714l, subdev)
static int dw9714l_write(struct i2c_client *client, u16 val)
{
	int ret = 0;
	u8 data[2];
	data[0] = val>>8;
	data[1] = val;
	ret = i2c_master_send(client, data, 2);
	if (ret < 0)
		dev_err(&client->dev,
			"dw9714l:failed to write val-0x%02x\n", val);
	return (ret > 0) ? 0 : ret;
}

static int dw9714l_read(struct i2c_client *client, s32 *val)
{
	int ret;
	u8 data[2];
	ret = i2c_master_recv(client, data, 2);
	if (ret < 0) {
		dev_err(&client->dev, "dw9714l: failed to read\n");
		return ret;
	}
	*val = data[0]<<8 | data[1];
	return 0;
}

static int dw9714l_set_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;
	struct dw9714l *vcm =
		container_of(ctrl->handler, struct dw9714l, ctrls);
	struct i2c_client *client = v4l2_get_subdevdata(&vcm->subdev);
	switch (ctrl->id) {
	case V4L2_CID_FOCUS_ABSOLUTE:
		ret = dw9714l_write(client, ctrl->val);
		return ret;

	default:
		return -EPERM;
	}

}
static int dw9714l_get_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;
	struct dw9714l *vcm =
		container_of(ctrl->handler, struct dw9714l, ctrls);
	struct i2c_client *client = v4l2_get_subdevdata(&vcm->subdev);
	switch (ctrl->id) {
	case V4L2_CID_FOCUS_ABSOLUTE:
		ret = dw9714l_read(client, &(ctrl->val));
		return ret;

	default:
		return -EPERM;
	}

}
static void vcm_power(struct dw9714l *vcm, int on)
{
	/* Enable voltage for camera sensor vcm */
	if (on) {
		regulator_set_voltage(vcm->af_vcc, 2800000, 2800000);
		regulator_enable(vcm->af_vcc);
	} else {
		regulator_disable(vcm->af_vcc);
	}
}
static int dw9714l_subdev_close(struct v4l2_subdev *sd,
		struct v4l2_subdev_fh *fh)
{
	struct dw9714l *core = to_dw9714l(sd);
	vcm_power(core, 0);
	core->openflag = 0;
	return 0;
}

static int dw9714l_subdev_open(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh)
{
	struct dw9714l *core = to_dw9714l(sd);
	if (core->openflag == 1)
		return -EBUSY;
	core->openflag = 1;
	vcm_power(core, 1);

	return 0;
}
static const struct v4l2_ctrl_ops dw9714l_ctrl_ops = {
	.g_volatile_ctrl = dw9714l_get_ctrl,
	.s_ctrl = dw9714l_set_ctrl,
};

static int dw9714l_init_controls(struct dw9714l *vcm)
{
	struct v4l2_ctrl *ctrl;
	v4l2_ctrl_handler_init(&vcm->ctrls, 1);
	/* V4L2_CID_FOCUS_ABSOLUTE */
	ctrl = v4l2_ctrl_new_std(&vcm->ctrls, &dw9714l_ctrl_ops,
				 V4L2_CID_FOCUS_ABSOLUTE, 0, (1<<16)-1, 1, 0);
	if (ctrl != NULL)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;
	vcm->subdev.ctrl_handler = &vcm->ctrls;

	return vcm->ctrls.error;
}
/* subdev internal operations */
static const struct v4l2_subdev_internal_ops dw9714l_v4l2_internal_ops = {
	.open = dw9714l_subdev_open,
	.close = dw9714l_subdev_close,
};

static const struct v4l2_subdev_core_ops dw9714l_core_ops = {
	.g_ext_ctrls = v4l2_subdev_g_ext_ctrls,
	.try_ext_ctrls = v4l2_subdev_try_ext_ctrls,
	.s_ext_ctrls = v4l2_subdev_s_ext_ctrls,
	.g_ctrl = v4l2_subdev_g_ctrl,
	.s_ctrl = v4l2_subdev_s_ctrl,
	.queryctrl = v4l2_subdev_queryctrl,
	.querymenu = v4l2_subdev_querymenu,
};

static const struct v4l2_subdev_ops dw9714l_ops = {
	.core = &dw9714l_core_ops,
};

static int __devinit dw9714l_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	int ret = -1;
	s32 vcmid;
	int pwdn = 0;
	struct dw9714l *vcm;
	struct vcm_platform_data *pdata = client->dev.platform_data;
	vcm = kzalloc(sizeof(struct dw9714l), GFP_KERNEL);
	if (!vcm) {
		dev_err(&client->dev, "dw9714l: failed to allocate dw9714l struct\n");
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
	if (dw9714l_read(client, &vcmid)) {
		printk(KERN_ERR "VCM: DW9714L vcm detect failure!\n");
		goto out_pwdn;
	}
	if (vcm->pwdn_gpio) {
		gpio_direction_output(vcm->pwdn_gpio, pwdn);
		gpio_free(vcm->pwdn_gpio);
	}
	if (vcm->af_vcc)
		vcm_power(vcm, 0);
	v4l2_i2c_subdev_init(&vcm->subdev, client, &dw9714l_ops);
	vcm->subdev.internal_ops = &dw9714l_v4l2_internal_ops;
	vcm->subdev.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	ret = dw9714l_init_controls(vcm);
	if (ret < 0)
		goto out;
	ret = media_entity_init(&vcm->subdev.entity, 0, NULL, 0);
	if (ret < 0)
		goto out;
	v4l2_ctrl_handler_setup(&vcm->ctrls);
	printk(KERN_INFO "dw9714l detected!\n");
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
static int dw9714l_remove(struct i2c_client *client)
{
	struct v4l2_subdev *subdev = i2c_get_clientdata(client);
	struct dw9714l *vcm = to_dw9714l(subdev);

	v4l2_device_unregister_subdev(subdev);
	v4l2_ctrl_handler_free(&vcm->ctrls);
	media_entity_cleanup(&vcm->subdev.entity);
	kfree(vcm);
	return 0;
}

static const struct i2c_device_id dw9714l_id[] = {
	{"dw9714l", 0},
};

MODULE_DEVICE_TABLE(i2c, dw9714l_id);

static struct i2c_driver dw9714l_driver = {
	.driver = {
		   .owner = THIS_MODULE,
		   .name = "vcm:ov5647<dw9714l>",
		   },
	.probe = dw9714l_probe,
	.remove = dw9714l_remove,
	.id_table = dw9714l_id,
};

static int __init dw9714l_mod_init(void)
{
	return i2c_add_driver(&dw9714l_driver);
}

module_init(dw9714l_mod_init);

static void __exit dw9714l_mode_exit(void)
{
	i2c_del_driver(&dw9714l_driver);
}

module_exit(dw9714l_mode_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Chunlin Hu <huchl@marvell.com>");
MODULE_DESCRIPTION("dw9714l vcm driver");

