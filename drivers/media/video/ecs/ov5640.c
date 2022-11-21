/*
 * ov5640 sensor driver based on Marvell ECS framework
 *
 * Copyright (C) 2012 Marvell Internation Ltd.
 *
 * Bin Zhou <zhoub@marvell.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/videodev2.h>
#include <mach/camera.h>
#include "ov5640.h"

static inline struct ov5640 *to_ov5640(const struct i2c_client *client)
{
	struct x_subdev *xsd = container_of(i2c_get_clientdata(client), \
					struct x_subdev, subdev);
	return container_of(xsd, struct ov5640, xsd);
}

int ov5640_firmware_download(void *hw_ctx, const void *table, int size)
{
	int ret = 0, i;
	struct x_i2c *xic = hw_ctx;
	struct i2c_client *client = xic->client;
	const OV5640_FIRMWARE_ARRAY *firmware_regs = table;

	if (unlikely(size <= 0)) {
		dev_err(&client->dev, "No AF firmware available\n");
		return -ENODEV;
	}

	/* Actually start to download firmware */
	for (i = 0; i < size; i++) {
		ret = xic_write_burst_wb(xic, firmware_regs[i].reg_base, \
						firmware_regs[i].value, \
						firmware_regs[i].len);
		if (ret < 0) {
			dev_err(&client->dev, "i2c error %s %d\n",
				__func__, __LINE__);
			break;
		}
	}

	dev_info(&client->dev, "AF firmware downloaded\n");

	return size;
}
EXPORT_SYMBOL(ov5640_firmware_download);

static int select_bus_type(struct ov5640 *ov5640, const char *name)
{
	enum _ov5640_profile new_prof;
	int ret;

	if (!strcmp(name, "generic")) {
		new_prof = GENERIC;
		goto exit;
	}
	if (!strcmp(name, "tavor-mipi")) {
		new_prof = PXA97X_DKB;
		/* creat state, replace setting here */
		goto exit;
	}
	if (!strcmp(name, "pxa98x-mipi")) {
		new_prof = PXA98X_DKB;
		/* creat state, replace setting here */
		goto exit;
	}
	return -EINVAL;
exit:
	if (new_prof == ov5640->profile) {
		printk(KERN_INFO "ov5640: profile name: %s\n", name);
		ecs_sensor_reset(ov5640->xsd.ecs);
		return 0;
	}

	strcpy(ov5640->name, name);
	strcat(ov5640->name, "-ov5640");
	ov5640->xsd.ecs = &generic_ov5640;

	/* Instantize to platform-specific driver */
	switch (new_prof) {
	case PXA97X_DKB:
		/* use mipi 2lane as interface */
		ret = ecs_sensor_merge(ov5640->xsd.ecs, &nevo_spec);
		if (ret < 0)
			return ret;
		/* Initialize ov5640 as a ecs subdev driver */
		ov5640->xsd.state_list = nevo_state_list;
		ov5640->xsd.state_cnt = ARRAY_SIZE(nevo_state_list);
		ecs_subdev_init(&ov5640->xsd);
		break;

	case PXA98X_DKB:
		/* use mipi 2lane as interface */
		ret = ecs_sensor_merge(ov5640->xsd.ecs, &pxa98x_spec);
		if (ret < 0)
			return ret;
		/* Initialize ov5640 as a ecs subdev driver */
		ov5640->xsd.state_list = pxa98x_state_list;
		ov5640->xsd.state_cnt = ARRAY_SIZE(pxa98x_state_list);
		ecs_subdev_init(&ov5640->xsd);
		break;

	case GENERIC:
		/* Initialize ov5640 as a generic ecs driver */
		ecs_sensor_init(ov5640->xsd.ecs);
		break;
	default:
		return -EPERM;
	}

	ov5640->profile = new_prof;
	ecs_sensor_reset(ov5640->xsd.ecs);
	return 0;
}

static enum _ov5640_profile __attribute__((unused)) get_bus_type(\
	struct ov5640 *ov5640)
{
	return ov5640->profile;
}

static int __attribute__((unused)) ov5640_g_frame_interval( \
	struct v4l2_subdev *sd,	struct v4l2_subdev_frame_interval *inter)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov5640 *ov5640 = to_ov5640(client);
	int pre_div, multiplier, vco_freq;
	int sys_div, pll_rdiv, bit_div, sclk_rdiv, mipi_div;
	int sys_clk, vts, hts, frame_rate, mipi_bit_rate, mipi_clk;
	int mclk;
	u8 val;

	mclk = inter->pad;
	/* get sensor system clk */
	/* vco frequency */
	ov5640_read(client, 0x3037, &val);
	/* There should be a complicated algorithm
	   including double member.
	 */
	pll_rdiv = (val >> 4) & 0x1;
	val = val & 0xf;
	pre_div = (val == 0) ? 2 : ((val == 5) ? 3 : ((val == 7) ? 5 : ((val > 8) ? 2 : (val * 2))));
	ov5640_read(client, 0x3036, &val);
	multiplier = (val < 128) ? val : (val/2*2);

	vco_freq = mclk / pre_div * multiplier * 2;
	dev_dbg(&client->dev, "vco_freq: %d, mclk:%d,pre_div:%d,"
			"multiplier:%d\n", vco_freq, mclk, pre_div, multiplier);

	ov5640_read(client, 0x3035, &val);
	val = (val >> 4) & 0xf;
	sys_div = (val > 0) ? val : 16;
	pll_rdiv = (pll_rdiv == 0) ? 1 : 2;
	ov5640_read(client, 0x3034, &val);
	val = val & 0xf;
	bit_div = (val / 4 == 2) ? 2 : 1;
	ov5640_read(client, 0x3108, &val);
	val = val & 0x3;
	sclk_rdiv = (val == 0) ? 1 : ((val == 1) ? 2
			: ((val == 2) ? 4 : 8));

	sys_clk = vco_freq / sys_div / pll_rdiv / bit_div
		/ sclk_rdiv;
	dev_dbg(&client->dev, "sys_clk: %d, sys_div:%d,pll_rdiv:%d,"
			"bit_div:%d,sclk_rdiv:%d\n", sys_clk, sys_div,
			pll_rdiv, bit_div, sclk_rdiv);

	/* mipi bit rate */
	ov5640_read(client, 0x3035, &val);
	val = val & 0xf;
	mipi_div = (val > 0) ? val : 16;

	mipi_bit_rate = vco_freq / sys_div / mipi_div;

	/* mipi clk */
	mipi_clk = mipi_bit_rate / 2;
	dev_dbg(&client->dev, "MIPI bit rate: %d, SYS clk: %d, MIPI Clock"
			"clk: %d\n", mipi_bit_rate, sys_clk, mipi_clk);

	inter->pad = mipi_clk;

	/* get sensor hts & vts */
	ov5640_read(client, 0x380c, &val);
	hts = val & 0xf;
	hts <<= 8;
	ov5640_read(client, 0x380d, &val);
	hts += val & 0xff;
	ov5640_read(client, 0x380e, &val);
	vts = val & 0xf;
	vts <<= 8;
	ov5640_read(client, 0x380f, &val);
	vts += val & 0xff;

	if (!hts || !vts)
		return -EINVAL;
	frame_rate = sys_clk * 1000000 / (hts * vts);
	ov5640->frame_rate = frame_rate;
	dev_dbg(&client->dev, "frame_rate: %d,"
			"hts:%x, vts:%x\n", frame_rate, hts, vts);

	inter->interval.numerator = frame_rate;
	inter->interval.denominator = 1;

	return 0;
}

static int ov5640_video_probe(struct soc_camera_device *icd,
				   struct i2c_client *client)
{
	struct ov5640 *ov5640 = to_ov5640(client);
	struct soc_camera_link *icl = soc_camera_i2c_to_link(client);
	int ret = 0;

	ret = (*ov5640_xic.detect)(&ov5640_xic);
	if (ret)
		goto out;
	dev_err(&client->dev, "OmniVision ov5640 sensor detected\n");

	ov5640->xsd.model = V4L2_IDENT_OV5640;

	if (icl->priv == NULL) {
		dev_err(&client->dev, "ov5640 driver need know plat-sensor\n");
		return -EINVAL;
	}

	if (icl->flags & 0x80000000) {
#ifdef CONFIG_PXA95x
			/* priv is pointing to sensor_platform_data */
			struct sensor_platform_data *sensor = icl->priv;
			char name[32] = {0};

			strcpy(name, sensor->board_name);
			/* add postfix according to interface flag */
			icl->flags |= sensor->interface;
			if (icl->flags & SOCAM_MIPI)
				strcat(name, "-mipi");
			else
				strcat(name, "-dvp");

			ret = select_bus_type(ov5640, name);
#endif
	} else
		/* priv is pointing to ov5640 profile name */
		ret = select_bus_type(ov5640, icl->priv);

	if (ret) {
		dev_err(&client->dev, "need know interface type\n");
		return ret;
	}

out:
	return ret;

}

static int ov5640_probe(struct i2c_client *client,
			    const struct i2c_device_id *did)
{
	struct ov5640 *ov5640;
	struct soc_camera_device *icd = client->dev.platform_data;
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	int ret;

	if (!icd) {
		dev_err(&client->dev, "ov5640: missing soc-camera data!\n");
		return -EINVAL;
	}

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_WORD_DATA)) {
		dev_warn(&adapter->dev,
			 "I2C-Adapter doesn't support I2C_FUNC_SMBUS_WORD\n");
		return -EIO;
	}

	ov5640 = kzalloc(sizeof(struct ov5640), GFP_KERNEL);
	if (!ov5640)
		return -ENOMEM;

	ov5640->xsd = ov5640_xsd;	/* setup xsd based on generic */
	v4l2_i2c_subdev_init(&ov5640->xsd.subdev, client, &ecs_subdev_ops);
	ov5640->profile = PROFILE_END;
	ov5640_xic.client = v4l2_get_subdevdata(&ov5640->xsd.subdev);

	ret = ov5640_video_probe(icd, client);
	if (ret)
		kfree(ov5640);

	return ret;
}

static int ov5640_remove(struct i2c_client *client)
{
	struct ov5640 *ov5640 = to_ov5640(client);

	kfree(ov5640);
	return 0;
}

static const struct i2c_device_id ov5640_idtable[] = {
	{"ov5640", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, ov5640_idtable);

static struct i2c_driver ov5640_driver = {
	.driver = {
		.name = "ov5640",
	},
	.probe		= ov5640_probe,
	.remove		= ov5640_remove,
	.id_table	= ov5640_idtable,
};

static int __init ov5640_mod_init(void)
{
	int ret = 0;
	ret = i2c_add_driver(&ov5640_driver);
	return ret;
}

static void __exit ov5640_mod_exit(void)
{
	i2c_del_driver(&ov5640_driver);
}

module_init(ov5640_mod_init);
module_exit(ov5640_mod_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Bin Zhou <zhoub@marvell.com>");
MODULE_DESCRIPTION("OmniVision OV5640 Camera Driver");
