/*
 * drivers/video/mmp/panel/mipi_nt35565.c
 * active panel using DSI interface to do init
 *
 * Copyright (C) 2013 Marvell Technology Group Ltd.
 * Authors:  Yu Xu <yuxu@marvell.com>
 *          Guoqing Li <ligq@marvell.com>
 *          Lisa Du <cldu@marvell.com>
 *          Zhou Zhu <zzhu3@marvell.com>
 *          Jing Xiang <jxiang@marvell.com>
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
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/fb.h>
#include <video/mmp_disp.h>
#include <video/mipi_display.h>

#define NT35565_SLEEP_OUT_DELAY 200
#define NT35565_DISP_ON_DELAY	0

struct nt35565_plat_data {
	void (*plat_onoff)(int status);
};

static char exit_sleep[] = {0x11};
static char display_on[] = {0x29};

static struct mmp_dsi_cmd_desc nt35565_display_on_cmds[] = {
	{MIPI_DSI_DCS_SHORT_WRITE, 0, NT35565_SLEEP_OUT_DELAY,
		sizeof(exit_sleep), exit_sleep},
	{MIPI_DSI_DCS_SHORT_WRITE, 0, NT35565_DISP_ON_DELAY,
		sizeof(display_on), display_on},
};

static void nt35565_panel_on(struct mmp_path *path)
{
	mmp_phy_dsi_tx_cmd_array(path->phy, nt35565_display_on_cmds,
		ARRAY_SIZE(nt35565_display_on_cmds));
}

static void nt35565_onoff(struct mmp_panel *panel, int status)
{
	struct nt35565_plat_data *plat = panel->plat_data;
	struct mmp_path *path = mmp_get_path(panel->plat_path_name);

	if (status) {
		/* power on */
		if (plat->plat_onoff)
			plat->plat_onoff(1);

		nt35565_panel_on(path);
	} else {
		/* power off */
		if (plat->plat_onoff)
			plat->plat_onoff(0);
	}
}

static struct mmp_mode mmp_modes_nt35565[] = {
	[0] = {
		.pixclock_freq = 34421160,
		.refresh = 57,
		.xres = 540,
		.yres = 960,
		.hsync_len = 2,
		.left_margin = 10,
		.right_margin = 68,
		.vsync_len = 2,
		.upper_margin = 6,
		.lower_margin = 6,
		.invert_pixclock = 0,
		.pix_fmt_out = PIXFMT_RGB888PACK,
		.hsync_invert = FB_SYNC_HOR_HIGH_ACT,
		.vsync_invert = FB_SYNC_VERT_HIGH_ACT,
	},
};

static int nt35565_get_modelist(struct mmp_panel *panel,
		struct mmp_mode **modelist)
{
	*modelist = mmp_modes_nt35565;
	return 1;
}

static struct mmp_panel panel_nt35565 = {
	.name = "nt35565",
	.panel_type = PANELTYPE_DSI_VIDEO,
	.get_modelist = nt35565_get_modelist,
	.set_onoff = nt35565_onoff,
};

static int nt35565_probe(struct platform_device *pdev)
{
	struct mmp_mach_panel_info *mi;
	struct nt35565_plat_data *plat_data;

	/* get configs from platform data */
	mi = pdev->dev.platform_data;
	if (mi == NULL) {
		dev_err(&pdev->dev, "no platform data defined\n");
		return -EINVAL;
	}

	plat_data = kzalloc(sizeof(*plat_data), GFP_KERNEL);
	if (!plat_data)
		return -ENOMEM;

	plat_data->plat_onoff = mi->plat_set_onoff;
	panel_nt35565.plat_data = plat_data;
	panel_nt35565.plat_path_name = mi->plat_path_name;
	panel_nt35565.dev = &pdev->dev;

	mmp_register_panel(&panel_nt35565);

	return 0;
}

static int nt35565_remove(struct platform_device *dev)
{
	mmp_unregister_panel(&panel_nt35565);
	kfree(panel_nt35565.plat_data);

	return 0;
}

static struct platform_driver nt35565_driver = {
	.driver		= {
		.name	= "mmp-nt35565",
		.owner	= THIS_MODULE,
	},
	.probe		= nt35565_probe,
	.remove		= nt35565_remove,
};

static int nt35565_init(void)
{
	return platform_driver_register(&nt35565_driver);
}
static void nt35565_exit(void)
{
	platform_driver_unregister(&nt35565_driver);
}
module_init(nt35565_init);
module_exit(nt35565_exit);

MODULE_AUTHOR("Yu Xu <yuxu@marvell.com>");
MODULE_DESCRIPTION("Panel driver for MIPI panel NT35565");
MODULE_LICENSE("GPL");
