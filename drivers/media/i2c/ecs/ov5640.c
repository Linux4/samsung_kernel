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

#include <linux/init.h>
#include <linux/module.h>
#include "ov5640.h"

char *ov5640_get_profile(const struct i2c_client *client)
{
	return "pxa-mipi";
}

int ov5640_firmware_download(void *hw_ctx, const void *table, int size)
{
	int ret = 0, i;
	struct x_i2c *xic = hw_ctx;
	struct i2c_client *client = xic->client;
	const struct OV5640_FIRMWARE_ARRAY *firmware_regs = table;

	if (unlikely(size <= 0)) {
		dev_err(&client->dev, "No AF firmware available\n");
		return -ENODEV;
	}

	/* Actually start to download firmware */
	for (i = 0; i < size; i++) {
		ret = xic_write_burst_wb(xic, firmware_regs[i].reg_base,
						firmware_regs[i].value,
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

static int ov5640_g_frame_interval(struct v4l2_subdev *sd,
				struct v4l2_subdev_frame_interval *inter)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct x_subdev *xsd = container_of(sd, struct x_subdev, subdev);
	struct x_i2c *xic = xsd->ecs->hw_ctx;
	int pre_div, multiplier, vco_freq;
	int sys_div, pll_rdiv, bit_div, sclk_rdiv, mipi_div;
	int sys_clk, vts, hts, frame_rate, mipi_bit_rate, mipi_clk;
	int mclk;
	u8 val;

	mclk = inter->pad;
	/* get sensor system clk */
	/* vco frequency */
	(*xic->read)(xic, 0x3037, &val);
	/* There should be a complicated algorithm
	   including double member.
	 */
	pll_rdiv = (val >> 4) & 0x1;
	val = val & 0xf;
	pre_div = (val == 0) ? 2 : ((val == 5) ? 3 : ((val == 7) ?
				5 : ((val > 8) ? 2 : (val * 2))));
	(*xic->read)(xic, 0x3036, &val);
	multiplier = (val < 128) ? val : (val/2*2);

	vco_freq = mclk / pre_div * multiplier * 2;
	dev_dbg(&client->dev, "vco_freq: %d, mclk:%d,pre_div:%d, "
			"multiplier:%d\n", vco_freq, mclk, pre_div, multiplier);

	(*xic->read)(xic, 0x3035, &val);
	val = (val >> 4) & 0xf;
	sys_div = (val > 0) ? val : 16;
	pll_rdiv = (pll_rdiv == 0) ? 1 : 2;
	(*xic->read)(xic, 0x3034, &val);
	val = val & 0xf;
	bit_div = (val / 4 == 2) ? 2 : 1;
	(*xic->read)(xic, 0x3108, &val);
	val = val & 0x3;
	sclk_rdiv = (val == 0) ? 1 : ((val == 1) ? 2
			: ((val == 2) ? 4 : 8));

	sys_clk = vco_freq / sys_div / pll_rdiv / bit_div
		/ sclk_rdiv;
	dev_dbg(&client->dev, "sys_clk: %d, sys_div:%d,pll_rdiv:%d,"
			"bit_div:%d,sclk_rdiv:%d\n", sys_clk, sys_div,
			pll_rdiv, bit_div, sclk_rdiv);

	/* mipi bit rate */
	(*xic->read)(xic, 0x3035, &val);
	val = val & 0xf;
	mipi_div = (val > 0) ? val : 16;

	mipi_bit_rate = vco_freq / sys_div / mipi_div;

	/* mipi clk */
	mipi_clk = mipi_bit_rate / 2;
	dev_dbg(&client->dev, "MIPI bit rate: %d, SYS clk: %d, MIPI Clock"
			"clk: %d\n", mipi_bit_rate, sys_clk, mipi_clk);

	inter->pad = mipi_clk;

	/* get sensor hts & vts */
	(*xic->read)(xic, 0x380c, &val);
	hts = val & 0xf;
	hts <<= 8;
	(*xic->read)(xic, 0x380d, &val);
	hts += val & 0xff;
	(*xic->read)(xic, 0x380e, &val);
	vts = val & 0xf;
	vts <<= 8;
	(*xic->read)(xic, 0x380f, &val);
	vts += val & 0xff;

	if (!hts || !vts)
		return -EINVAL;
	frame_rate = sys_clk * 1000000 / (hts * vts);
	dev_dbg(&client->dev, "frame_rate: %d,"
			"hts:%x, vts:%x\n", frame_rate, hts, vts);

	inter->interval.numerator = frame_rate;
	inter->interval.denominator = 1;

	return 0;
}

static int __init ov5640_mod_init(void)
{
	return xsd_add_driver(ov564x_drv_table);
}

static void __exit ov5640_mod_exit(void)
{
	xsd_del_driver(ov564x_drv_table);
}

module_init(ov5640_mod_init);
module_exit(ov5640_mod_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Bin Zhou <zhoub@marvell.com>");
MODULE_DESCRIPTION("OmniVision OV5640 Camera Driver");
