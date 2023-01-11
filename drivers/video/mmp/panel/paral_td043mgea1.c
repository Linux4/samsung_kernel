/*
 * linux/drivers/video/mmp/panel/paral_td043mgea1.c
 * active panel using spi interface to do init
 *
 * Copyright (C) 2013 Marvell Technology Group Ltd.
 * Authors:  Guoqing Li <ligq@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <video/mmp_disp.h>

struct td043mgea1_plat_data {
	void (*plat_onoff)(int status);
};

static void td043mgea1_set_status(struct mmp_panel *panel, int status)
{
	struct td043mgea1_plat_data *plat = panel->plat_data;

	if (plat->plat_onoff)
		plat->plat_onoff(status_is_on(status));
}

static struct mmp_mode mmp_modes_td043mgea1[] = {
	[0] = {
		.pixclock_freq = 33264000,
		.refresh = 60,
		.real_xres = 800,
		.real_yres = 480,
		.hsync_len = 4,
		.left_margin = 212,
		.right_margin = 40,
		.vsync_len = 4,
		.upper_margin = 31,
		.lower_margin = 10,
		.pix_fmt_out = PIXFMT_BGR565,
	},
};

static int td043mgea1_get_modelist(struct mmp_panel *panel,
		struct mmp_mode **modelist)
{
	*modelist = mmp_modes_td043mgea1;
	return 1;
}

static struct mmp_panel panel_td043mgea1 = {
	.name = "td043mgea1",
	.panel_type = PANELTYPE_ACTIVE,
	.get_modelist = td043mgea1_get_modelist,
	.set_status = td043mgea1_set_status,
};

static int td043mgea1_probe(struct platform_device *pdev)
{
	struct mmp_mach_panel_info *mi;
	struct td043mgea1_plat_data *plat_data;
	struct device_node *np = pdev->dev.of_node;
	const char *path_name;
	int ret;

	plat_data = kzalloc(sizeof(*plat_data), GFP_KERNEL);
	if (!plat_data)
		return -ENOMEM;

	if (IS_ENABLED(CONFIG_OF)) {
		ret = of_property_read_string(np, "marvell,path-name",
				&path_name);
		if (ret < 0)
			return ret;
		panel_td043mgea1.plat_path_name = path_name;
	} else {
		/* get configs from platform data */
		mi = pdev->dev.platform_data;
		if (mi == NULL) {
			dev_err(&pdev->dev, "no platform data defined\n");
			return -EINVAL;
		}

		plat_data->plat_onoff = mi->plat_set_onoff;
		panel_td043mgea1.plat_path_name = mi->plat_path_name;
	}

	panel_td043mgea1.plat_data = plat_data;
	panel_td043mgea1.dev = &pdev->dev;
	mmp_register_panel(&panel_td043mgea1);

	return 0;
}

static int td043mgea1_remove(struct platform_device *dev)
{
	mmp_unregister_panel(&panel_td043mgea1);
	kfree(panel_td043mgea1.plat_data);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mmp_td043mgea1_dt_match[] = {
	{ .compatible = "marvell,mmp-td043mgea1" },
	{},
};
#endif

static struct platform_driver td043mgea1_driver = {
	.driver		= {
		.name	= "mmp-td043mgea1",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(mmp_td043mgea1_dt_match),
	},
	.probe		= td043mgea1_probe,
	.remove		= td043mgea1_remove,
};

static int td043mgea1_init(void)
{
	return platform_driver_register(&td043mgea1_driver);
}
static void td043mgea1_exit(void)
{
	platform_driver_unregister(&td043mgea1_driver);
}
module_init(td043mgea1_init);
module_exit(td043mgea1_exit);

MODULE_AUTHOR("Guoqing Li<ligq@marvell.com>");
MODULE_DESCRIPTION("Panel driver for TD043MGEA1");
MODULE_LICENSE("GPL");
