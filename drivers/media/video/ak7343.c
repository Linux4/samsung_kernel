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
#include <media/v4l2-ctrls.h>
#include <media/ak7343.h>
#include <linux/module.h>
#include <media/v4l2-device.h>
#include <linux/regulator/machine.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <mach/mfp-pxa988.h>
#include <linux/mvisp.h>
struct v4l2_vcm_register_access {
	unsigned long	reg;
	unsigned long	value;
};
struct ak7343 {
	struct v4l2_subdev subdev;
	/* Controls */
	struct v4l2_ctrl_handler ctrls;
	struct regulator *af_vcc;
	int pwdn_gpio;
	char openflag;
};
static void i2c_start(unsigned long scl, unsigned long sda)
{
	gpio_direction_output(sda, 0);
	udelay(1);
	gpio_direction_output(scl, 0);
	udelay(1);
}
static int i2c_sendbyte(unsigned long scl, unsigned long sda, u8 val)
{
	u8 ccnt;
	int ret;
	for (ccnt = 0; ccnt < 8; ccnt++) {
		gpio_direction_output(sda, val&0x80);
		gpio_direction_output(scl, 1);
		udelay(1);
		val <<= 0x01;
		gpio_direction_output(scl, 0);
		udelay(1);
	}
	gpio_direction_input(sda);
	gpio_direction_output(scl, 1);
	udelay(1);
	if (!gpio_get_value(sda))
		ret = 0;
	else
		ret = -1;
	gpio_direction_output(scl, 0);
	udelay(1);
	return ret;
}
static int i2c_rcvbyte(unsigned long scl, unsigned long sda, u8 *val)
{
	u8 ccnt;
	int ret;
	gpio_direction_input(sda);
	for (ccnt = 0; ccnt < 8; ccnt++) {
		gpio_direction_output(scl, 1);
		udelay(1);
		*val <<= 0x01;
		if (gpio_get_value(sda))
			*val |= 0x01;
		gpio_direction_output(scl, 0);
		udelay(1);
	}
	gpio_direction_output(scl, 1);
	udelay(1);
	if (!gpio_get_value(sda))
		ret = 0;
	else
		ret = -1;

	gpio_direction_output(scl, 0);
	return ret;
}
static void i2c_stop(unsigned long scl, unsigned long sda)
{
	gpio_direction_output(sda, 0);
	gpio_direction_output(scl, 1);
	gpio_direction_output(sda, 1);
}
#define to_ak7343(sd) container_of(sd, struct ak7343, subdev)
static int ak7343_write(struct i2c_client *client,
			unsigned long reg, unsigned long val)
{
	int ret = 0;
	unsigned long mfp_pin[2];
	unsigned long i2c0_mfps[] = {
		GPIO053_GPIO_53,		/* SCL */
		GPIO054_GPIO_54,		/* SDA */
	};
	unsigned long scl, sda;
	scl = MFP_PIN_GPIO53;
	sda = MFP_PIN_GPIO54;
	i2c_lock_adapter(client->adapter);
	if (gpio_request(scl, "SCL")) {
		pr_err("Failed to request GPIO for SCL pin!\n");
		ret = -1;
		goto out0;
	}
	if (gpio_request(sda, "SDA")) {
		pr_err("Failed to request GPIO for SDA pin!\n");
		ret = -1;
		goto out1;
	};
	mfp_pin[0] = mfp_read(MFP_PIN_GPIO53);
	mfp_pin[1] = mfp_read(MFP_PIN_GPIO54);
	mfp_config(i2c0_mfps, ARRAY_SIZE(i2c0_mfps));
	i2c_start(scl, sda);
	if (i2c_sendbyte(scl, sda,  (client->addr<<1) | 0) < 0) {
		ret = -1;
		goto out2;
	}
	if (i2c_sendbyte(scl, sda, (u8)reg) < 0) {
		ret = -1;
		goto out2;
	}
	if (i2c_sendbyte(scl, sda, (u8)val) < 0) {
		ret = -1;
		goto out2;
	}
out2:
	i2c_stop(scl, sda);
	mfp_write(MFP_PIN_GPIO53, mfp_pin[0]);
	mfp_write(MFP_PIN_GPIO54, mfp_pin[1]);
	gpio_free(sda);

out1:
	gpio_free(scl);
out0:
	i2c_unlock_adapter(client->adapter);
	return ret;
}

static int ak7343_read(struct i2c_client *client,
			unsigned long reg, unsigned long *val)
{
	unsigned long mfp_pin[2];
	unsigned long i2c0_mfps[] = {
		GPIO053_GPIO_53,		/* SCL */
		GPIO054_GPIO_54,		/* SDA */
	};
	unsigned long scl, sda;
	int ret = 0;
	scl = MFP_PIN_GPIO53;
	sda = MFP_PIN_GPIO54;
	i2c_lock_adapter(client->adapter);
	if (gpio_request(scl, "SCL")) {
		pr_err("Failed to request GPIO for SCL pin!\n");
		ret = -1;
		goto out0;
	}
	if (gpio_request(sda, "SDA")) {
		pr_err("Failed to request GPIO for SDA pin!\n");
		ret = -1;
		goto out1;
	};
	mfp_pin[0] = mfp_read(MFP_PIN_GPIO53);
	mfp_pin[1] = mfp_read(MFP_PIN_GPIO54);
	mfp_config(i2c0_mfps, ARRAY_SIZE(i2c0_mfps));
	i2c_start(scl, sda);
	if (i2c_sendbyte(scl, sda, (client->addr<<1) | 1) < 0) {
		ret = -1;
		goto out2;
	}
	if (i2c_sendbyte(scl, sda, (u8)reg) < 0) {
		ret = -1;
		goto out2;
	}
	if (i2c_rcvbyte(scl, sda, (u8 *)val) < 0) {
		ret = -1;
		goto out2;
	}
out2:
	i2c_stop(scl, sda);
	mfp_write(MFP_PIN_GPIO53, mfp_pin[0]);
	mfp_write(MFP_PIN_GPIO54, mfp_pin[1]);
	gpio_free(sda);
out1:
	gpio_free(scl);
out0:
	i2c_unlock_adapter(client->adapter);
	return ret;
}
static int ak7343_set_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;
	struct ak7343 *vcm =
		container_of(ctrl->handler, struct ak7343, ctrls);
	struct i2c_client *client = v4l2_get_subdevdata(&vcm->subdev);
	switch (ctrl->id) {
	case V4L2_CID_FOCUS_ABSOLUTE:
		ret = ak7343_write(client, 0, ctrl->val);
		return ret;

	default:
		return -EPERM;
	}

}
static int ak7343_get_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;
	struct ak7343 *vcm =
		container_of(ctrl->handler, struct ak7343, ctrls);
	struct i2c_client *client = v4l2_get_subdevdata(&vcm->subdev);
	switch (ctrl->id) {
	case V4L2_CID_FOCUS_ABSOLUTE:
		ret = ak7343_read(client, 0, (unsigned long *)&(ctrl->val));
		return ret;

	default:
		return -EPERM;
	}

}
static void vcm_power(struct ak7343 *vcm, int on)
{
	/* Enable voltage for camera sensor vcm */
	if (on) {
		regulator_set_voltage(vcm->af_vcc, 2800000, 2800000);
		regulator_enable(vcm->af_vcc);
	} else {
		regulator_disable(vcm->af_vcc);
	}
}
static int ak7343_subdev_close(struct v4l2_subdev *sd,
		struct v4l2_subdev_fh *fh)
{
	struct ak7343 *core = to_ak7343(sd);
	vcm_power(core, 0);
	core->openflag = 0;
	return 0;
}

static int ak7343_subdev_open(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh)
{
	struct ak7343 *core = to_ak7343(sd);
	struct i2c_client *client = v4l2_get_subdevdata(&core->subdev);
	unsigned long mfp_pin[2];
	unsigned long i2c0_mfps[] = {
		GPIO053_GPIO_53,
		GPIO054_GPIO_54,
	};
	unsigned long scl, sda;
	if (core->openflag == 1)
		return -EBUSY;
	core->openflag = 1;

	scl = MFP_PIN_GPIO53;
	sda = MFP_PIN_GPIO54;
	i2c_lock_adapter(client->adapter);
	if (gpio_request(scl, "SCL"))
		pr_err("Failed to request GPIO for SCL pin!\n");
	if (gpio_request(sda, "SDA"))
		pr_err("Failed to request GPIO for SDA pin!\n");
	mfp_pin[0] = mfp_read(MFP_PIN_GPIO53);
	mfp_pin[1] = mfp_read(MFP_PIN_GPIO54);
	mfp_config(i2c0_mfps, ARRAY_SIZE(i2c0_mfps));
	gpio_direction_output(sda, 0);
	gpio_direction_output(scl, 0);
	mdelay(100);
	vcm_power(core, 1);
	mdelay(10);
	gpio_direction_output(sda, 1);
	gpio_direction_output(scl, 1);

	mfp_write(MFP_PIN_GPIO53, mfp_pin[0]);
	mfp_write(MFP_PIN_GPIO54, mfp_pin[1]);
	gpio_free(sda);
	gpio_free(scl);
	i2c_unlock_adapter(client->adapter);
	mdelay(120);
	ak7343_write(client, 0x05, 0x7a);
	ak7343_write(client, 0x01, 0x01);
	mdelay(200);
	ak7343_write(client, 0x02, 0x01);
	mdelay(120);
	ak7343_write(client, 0x01, 0x00);
	return 0;
}
static long ak7343_subdev_ioctl(struct v4l2_subdev *sd,
			unsigned int cmd, void *arg)
{
	int ret = -EINVAL;
	struct v4l2_vcm_register_access *tmp =
				(struct v4l2_vcm_register_access *)arg;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	switch (cmd) {
	case VIDIOC_PRIVATE_SENSER_REGISTER_GET:
		ret = ak7343_read(client,
			tmp->reg,
			(unsigned long *)&(tmp->value));
		break;
	case VIDIOC_PRIVATE_SENSER_REGISTER_SET:
		ret = ak7343_write(client,
			tmp->reg,
			tmp->value);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}
static const struct v4l2_ctrl_ops ak7343_ctrl_ops = {
	.g_volatile_ctrl = ak7343_get_ctrl,
	.s_ctrl = ak7343_set_ctrl,
};

static int ak7343_init_controls(struct ak7343 *vcm)
{
	struct v4l2_ctrl *ctrl;
	v4l2_ctrl_handler_init(&vcm->ctrls, 1);
	/* V4L2_CID_FOCUS_ABSOLUTE */
	ctrl = v4l2_ctrl_new_std(&vcm->ctrls, &ak7343_ctrl_ops,
				 V4L2_CID_FOCUS_ABSOLUTE, 0, (1<<16)-1, 1, 0);
	if (ctrl != NULL)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;
	vcm->subdev.ctrl_handler = &vcm->ctrls;

	return vcm->ctrls.error;
}
/* subdev internal operations */
static const struct v4l2_subdev_internal_ops ak7343_v4l2_internal_ops = {
	.open = ak7343_subdev_open,
	.close = ak7343_subdev_close,
};

static const struct v4l2_subdev_core_ops ak7343_core_ops = {
	.ioctl = ak7343_subdev_ioctl,
	.g_ext_ctrls = v4l2_subdev_g_ext_ctrls,
	.try_ext_ctrls = v4l2_subdev_try_ext_ctrls,
	.s_ext_ctrls = v4l2_subdev_s_ext_ctrls,
	.g_ctrl = v4l2_subdev_g_ctrl,
	.s_ctrl = v4l2_subdev_s_ctrl,
	.queryctrl = v4l2_subdev_queryctrl,
	.querymenu = v4l2_subdev_querymenu,
};

static const struct v4l2_subdev_ops ak7343_ops = {
	.core = &ak7343_core_ops,
};

static int __devinit ak7343_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	int ret = -1;
	unsigned long vcmid;
	int pwdn = 0;
	struct ak7343 *vcm;
	struct vcm_platform_data *pdata = client->dev.platform_data;
	vcm = kzalloc(sizeof(struct ak7343), GFP_KERNEL);
	if (!vcm) {
		dev_err(&client->dev, "ak7343: failed to allocate ak7343 struct\n");
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
	mdelay(20);
	if (ak7343_read(client, 0x00, &vcmid)) {
		printk(KERN_ERR "VCM: ak7343 vcm detect failure!\n");
		goto out_pwdn;
	}
	if (vcm->pwdn_gpio) {
		gpio_direction_output(vcm->pwdn_gpio, pwdn);
		gpio_free(vcm->pwdn_gpio);
	}
	if (vcm->af_vcc)
		vcm_power(vcm, 0);
	v4l2_i2c_subdev_init(&vcm->subdev, client, &ak7343_ops);
	vcm->subdev.internal_ops = &ak7343_v4l2_internal_ops;
	vcm->subdev.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	ret = ak7343_init_controls(vcm);
	if (ret < 0)
		goto out;
	ret = media_entity_init(&vcm->subdev.entity, 0, NULL, 0);
	if (ret < 0)
		goto out;
	v4l2_ctrl_handler_setup(&vcm->ctrls);
	printk(KERN_INFO "ak7343 detected!\n");
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
static int ak7343_remove(struct i2c_client *client)
{
	struct v4l2_subdev *subdev = i2c_get_clientdata(client);
	struct ak7343 *vcm = to_ak7343(subdev);

	v4l2_device_unregister_subdev(subdev);
	v4l2_ctrl_handler_free(&vcm->ctrls);
	media_entity_cleanup(&vcm->subdev.entity);
	kfree(vcm);
	return 0;
}

static const struct i2c_device_id ak7343_id[] = {
	{"ak7343", 0},
};

MODULE_DEVICE_TABLE(i2c, ak7343_id);

static struct i2c_driver ak7343_driver = {
	.driver = {
		   .owner = THIS_MODULE,
		   .name = "vcm:LSI3H5<ak7343>",
		   },
	.probe = ak7343_probe,
	.remove = ak7343_remove,
	.id_table = ak7343_id,
};

static int __init ak7343_mod_init(void)
{
	return i2c_add_driver(&ak7343_driver);
}

module_init(ak7343_mod_init);

static void __exit ak7343_mode_exit(void)
{
	i2c_del_driver(&ak7343_driver);
}

module_exit(ak7343_mode_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Chunlin Hu <huchl@marvell.com>");
MODULE_DESCRIPTION("ak7343 vcm driver");

