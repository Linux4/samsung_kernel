/*
 * gc2035 sensor driver based on Marvell ECS framework
 *
 * Copyright (C) 2012 Marvell Internation Ltd.
 *
 * Jianle Wang <wangjl@marvell.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include "gc2035.h"

char *gc2035_get_profile(const struct i2c_client *client)
{
	const struct soc_camera_link *icl = soc_camera_i2c_to_link(client);
	struct sensor_board_data *sensor = icl->priv;
	struct mmp_cam_pdata *pdata = sensor->plat;

	if (!strcmp(pdata->name, "EMEI"))
		return "pxa98x-mipi";
	return NULL;
}

static int gc2035_g_frame_interval(struct v4l2_subdev *sd, \
				struct v4l2_subdev_frame_interval *inter)
{
	inter->interval.numerator = 24;
	inter->interval.denominator = 1;

	return 0;
}

static int __init gc2035_mod_init(void)
{
	return xsd_add_driver(gc2035_drv_table);
}

static void __exit gc2035_mod_exit(void)
{
	xsd_del_driver(gc2035_drv_table);
}

module_init(gc2035_mod_init);
module_exit(gc2035_mod_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jianle Wang <wangjl@marvell.com>");
MODULE_DESCRIPTION("GC2035 Camera Driver");
