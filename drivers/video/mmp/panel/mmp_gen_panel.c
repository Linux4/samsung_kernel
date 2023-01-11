/* drivers/video/mmp/panel/mmp-gen-panel.c
 *
 * Copyright (C) 2015 Samsung Electronics Co, Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/lcd.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/fb.h>
#include <video/mmp_disp.h>
#include <video/mmp_esd.h>
#include <video/mipi_display.h>
#include <linux/platform_data/gen-panel.h>

#include "../fb/mmpfb.h"
#include "../hw/mmp_dsi.h"
#include "../hw/mmp_ctrl.h"

static struct mmp_panel mmp_dsi_panel;

static inline struct lcd *panel_to_lcd(const struct mmp_panel *panel)
{
	return (struct lcd *)panel->plat_data;
}
static inline struct mmp_panel *lcd_to_panel(const struct lcd *lcd)
{
	return (struct mmp_panel *)lcd->pdata;
}

static void gpmode_to_mmpmode(struct mmp_mode *mmpmode,
		struct gen_panel_mode *gpmode)
{
	mmpmode->refresh = gpmode->refresh;
	mmpmode->xres = gpmode->xres;
	mmpmode->yres = gpmode->yres;
	mmpmode->real_xres = gpmode->real_xres;
	mmpmode->real_yres = gpmode->real_yres;

	mmpmode->left_margin = gpmode->left_margin;
	mmpmode->right_margin = gpmode->right_margin;
	mmpmode->upper_margin = gpmode->upper_margin;
	mmpmode->lower_margin = gpmode->lower_margin;
	mmpmode->hsync_len = gpmode->hsync_len;
	mmpmode->vsync_len = gpmode->vsync_len;

	mmpmode->pixclock_freq = gpmode->refresh *
		(gpmode->xres + gpmode->right_margin +
		 gpmode->left_margin + gpmode->hsync_len) *
		(gpmode->yres + gpmode->upper_margin +
		 gpmode->lower_margin + gpmode->vsync_len);

	mmpmode->height = gpmode->height;
	mmpmode->width = gpmode->width;
}

static inline int mmp_panel_tx_cmd_array(const struct lcd *lcd,
		const void *cmds, const int count)
{
	struct mmp_panel *panel = lcd_to_panel(lcd);
	int ret, retry = 3;

	/* TODO: Retry send cmds */
	do {
		ret = mmp_panel_dsi_tx_cmd_array(panel,
				(struct mmp_dsi_cmd_desc *)cmds, count);
	} while (retry-- && ret < 0);

	return ret;
}

static inline int mmp_panel_rx_cmd_array(const struct lcd *lcd,
		u8 *buf, const void *cmds, const int count)
{
	struct mmp_panel *panel = lcd_to_panel(lcd);
	struct mmp_dsi_buf dbuf;
	int ret;

	ret = mmp_panel_dsi_rx_cmd_array(panel, &dbuf,
			(struct mmp_dsi_cmd_desc *)cmds, count);

	memcpy(buf, dbuf.data, dbuf.length);

	return dbuf.length;
}

int mmp_panel_parse_dt(const struct device_node *np)
{
	struct mmp_panel *panel = &mmp_dsi_panel;
	struct mmp_path *path = mmp_get_path(panel->plat_path_name);
	struct mmp_dsi *dsi = mmp_path_to_dsi(path);
#ifdef CONFIG_MMP_ADAPTIVE_FPS
	int ret = 0;
#endif

	pr_debug("%s enter\n", __func__);
	if (!np) {
		pr_err("%s, device_node is null\n", __func__);
		return -EINVAL;
	}

	if (!dsi) {
		pr_err("%s, dsi is null\n", __func__);
		return -EINVAL;
	}

	if (of_property_read_bool(np, "gen-panel-master-mode")) {
		pr_debug("%s, master mode\n", __func__);
		dsi->setting.master_mode = 1;
	} else {
		pr_debug("%s, slave mode\n", __func__);
		dsi->setting.master_mode = 0;
	}
	if (of_property_read_bool(np, "gen-panel-hfp-en")) {
		pr_debug("%s, hfp_en\n", __func__);
		dsi->setting.hfp_en = 1;
	}
	if (of_property_read_bool(np, "gen-panel-hbp-en")) {
		pr_debug("%s, hbp_en\n", __func__);
		dsi->setting.hbp_en = 1;
	}
	if (of_property_read_bool(np, "gen-panel-non-continous-clk")) {
		pr_debug("%s, non-continuous clock enable\n", __func__);
		dsi->setting.non_continuous_clk = 1;
	}
#ifdef CONFIG_MMP_ADAPTIVE_FPS
	if (of_property_read_bool(np, "gen-panel-adaptive-fps-en")) {
		pr_debug("%s, adaptive_fps enable\n", __func__);
		dsi->framecnt->adaptive_fps_en = 1;
	}
	ret = of_property_read_u32(np, "gen-panel-switch-fps",
			&dsi->framecnt->switch_fps);
	ret = of_property_read_u32(np, "gen-panel-default-fps",
			&dsi->framecnt->default_fps);
	ret = of_property_read_u32(np, "gen-panel-flipcnt-wait",
			&dsi->framecnt->wait_time);
#endif

	return 0;
}

#define MAIN_FB_NODE	0	/* main FB */
static inline void mmp_panel_dump(const struct lcd *lcd)
{
	struct mmp_panel *panel = (struct mmp_panel *) lcd->pdata;
	struct mmp_path *path = mmp_get_path(panel->plat_path_name);
	struct mmp_dsi *dsi = mmp_path_to_dsi(path);
	struct mmp_dsi_regs *dsi_reg = dsi->reg_base;
	struct mmphw_ctrl *ctrl = path_to_ctrl(path);

	struct fb_info *info = registered_fb[MAIN_FB_NODE];
	struct mmpfb_info *fbi;

	if (!info) {
		pr_err("%s: err: panel or info is NULL\n", __func__);
		return;
	}
	fbi = (struct mmpfb_info *) info->par;
	if (!fbi) {
		pr_err("%s: err: fbi is NULL\n", __func__);
		return;
	}

	pr_info("last vsync ts=%lld\n", fbi->vsync.ts_nano);
	pr_info("fb addr: phy=0x%lx, virt=0x%p\n", info->fix.smem_start,
			phys_to_virt(info->fix.smem_start));
	if (lcd->bd)
		pr_info("backlight brt = %d\n", lcd->bd->props.brightness);

	/* dump DSI registers */
	if (dsi->status == MMP_OFF) {
		pr_info("%s: dsi is off\n", __func__);
		return;
	}

	if (!ctrl) {
		pr_err("%s: err: ctrl is NULL\n", __func__);
		return;
	}
	pr_info("display controller regs base 0x%p\n", ctrl->reg_base);
	pr_info("LCD_IRQ_ENA: 0x%x\n",
			readl_relaxed(ctrl->reg_base + SPU_IRQ_ENA));
	pr_info("LCD_IRQ_ISR: 0x%x\n",
			readl_relaxed(ctrl->reg_base + SPU_IRQ_ISR));
	pr_info("LCD_SCLK_DIV: 0x%x\n",
			readl_relaxed(ctrl->reg_base + LCD_SCLK_DIV));
	pr_info("LCD_TCLK_DIV: 0x%x\n",
			 readl_relaxed(ctrl->reg_base + LCD_TCLK_DIV));

	pr_info("dsi regs base 0x%p\n", dsi_reg);
	pr_info("ctrl0      (@%p): 0x%x\n",
		&dsi_reg->ctrl0, readl(&dsi_reg->ctrl0));
	pr_info("ctrl1      (@%p): 0x%x\n",
		&dsi_reg->ctrl1, readl(&dsi_reg->ctrl1));
	pr_info("irq_status (@%p): 0x%x\n",
		&dsi_reg->irq_status, readl(&dsi_reg->irq_status));
	pr_info("irq_mask   (@%p): 0x%x\n",
		&dsi_reg->irq_mask, readl(&dsi_reg->irq_mask));
	pr_info("cmd0       (@%p): 0x%x\n",
		&dsi_reg->cmd0, readl(&dsi_reg->cmd0));
	pr_info("cmd1       (@%p): 0x%x\n",
		&dsi_reg->cmd1, readl(&dsi_reg->cmd1));
	pr_info("cmd2       (@%p): 0x%x\n",
		&dsi_reg->cmd2, readl(&dsi_reg->cmd2));
	pr_info("cmd3       (@%p): 0x%x\n",
		&dsi_reg->cmd3, readl(&dsi_reg->cmd3));
	pr_info("dat0       (@%p): 0x%x\n",
		&dsi_reg->dat0, readl(&dsi_reg->dat0));
	pr_info("status0    (@%p): 0x%x\n",
		&dsi_reg->status0, readl(&dsi_reg->status0));
	pr_info("status1    (@%p): 0x%x\n",
		&dsi_reg->status1, readl(&dsi_reg->status1));
	pr_info("status2    (@%p): 0x%x\n",
		&dsi_reg->status2, readl(&dsi_reg->status2));
	pr_info("status3    (@%p): 0x%x\n",
		&dsi_reg->status3, readl(&dsi_reg->status3));
	pr_info("status4    (@%p): 0x%x\n",
		&dsi_reg->status4, readl(&dsi_reg->status4));
	pr_info("smt_cmd    (@%p): 0x%x\n",
		&dsi_reg->smt_cmd, readl(&dsi_reg->smt_cmd));
	pr_info("smt_ctrl0  (@%p): 0x%x\n",
		&dsi_reg->smt_ctrl0, readl(&dsi_reg->smt_ctrl0));
	pr_info("smt_ctrl1  (@%p): 0x%x\n",
		&dsi_reg->smt_ctrl1, readl(&dsi_reg->smt_ctrl1));
	pr_info("rx0_status (@%p): 0x%x\n",
		&dsi_reg->rx0_status, readl(&dsi_reg->rx0_status));
	pr_info("rx0_header (@%p): 0x%x\n",
		&dsi_reg->rx0_header, readl(&dsi_reg->rx0_header));
	pr_info("rx1_status (@%p): 0x%x\n",
		&dsi_reg->rx1_status, readl(&dsi_reg->rx1_status));
	pr_info("rx1_header (@%p): 0x%x\n",
		&dsi_reg->rx1_header, readl(&dsi_reg->rx1_header));
	pr_info("rx_ctrl    (@%p): 0x%x\n",
		&dsi_reg->rx_ctrl, readl(&dsi_reg->rx_ctrl));
	pr_info("rx_ctrl1   (@%p): 0x%x\n",
		&dsi_reg->rx_ctrl1, readl(&dsi_reg->rx_ctrl1));
	pr_info("rx2_status (@%p): 0x%x\n",
		&dsi_reg->rx2_status, readl(&dsi_reg->rx2_status));
	pr_info("rx2_header (@%p): 0x%x\n",
		&dsi_reg->rx2_header, readl(&dsi_reg->rx2_header));

	if (DISP_GEN4(dsi->version)) {
		pr_info("DPHY regs\n");
		pr_info("phy_ctrl1  (@%p): 0x%x\n",
			&dsi_reg->phy.phy_ctrl1, readl(&dsi_reg->phy.phy_ctrl1));
		pr_info("phy_ctrl2  (@%p): 0x%x\n",
			&dsi_reg->phy.phy_ctrl2, readl(&dsi_reg->phy.phy_ctrl2));
		pr_info("phy_ctrl3  (@%p): 0x%x\n",
			&dsi_reg->phy.phy_ctrl3, readl(&dsi_reg->phy.phy_ctrl3));
		pr_info("phy_status0(@%p): 0x%x\n",
			&dsi_reg->phy.phy_status0, readl(&dsi_reg->phy.phy_status0));
		pr_info("phy_status1(@%p): 0x%x\n",
			&dsi_reg->phy.phy_status1, readl(&dsi_reg->phy.phy_status1));
		pr_info("phy_status2(@%p): 0x%x\n",
			&dsi_reg->phy.phy_status2, readl(&dsi_reg->phy.phy_status2));
		pr_info("phy_rcomp0 (@%p): 0x%x\n",
			&dsi_reg->phy.phy_rcomp0, readl(&dsi_reg->phy.phy_rcomp0));
		pr_info("phy_timing0(@%p): 0x%x\n",
			&dsi_reg->phy.phy_timing0, readl(&dsi_reg->phy.phy_timing0));
		pr_info("phy_timing1(@%p): 0x%x\n",
			&dsi_reg->phy.phy_timing1, readl(&dsi_reg->phy.phy_timing1));
		pr_info("phy_timing2(@%p): 0x%x\n",
			&dsi_reg->phy.phy_timing2, readl(&dsi_reg->phy.phy_timing2));
		pr_info("phy_timing3(@%p): 0x%x\n",
			&dsi_reg->phy.phy_timing3, readl(&dsi_reg->phy.phy_timing3));
		pr_info("phy_code_0 (@%p): 0x%x\n",
			&dsi_reg->phy.phy_code_0, readl(&dsi_reg->phy.phy_code_0));
		pr_info("phy_code_1 (@%p): 0x%x\n",
			&dsi_reg->phy.phy_code_1, readl(&dsi_reg->phy.phy_code_1));
	} else {
		pr_info("phy_ctrl1  (@%p): 0x%x\n",
			&dsi_reg->phy_ctrl1, readl(&dsi_reg->phy_ctrl1));
		pr_info("phy_ctrl2  (@%p): 0x%x\n",
			&dsi_reg->phy_ctrl2, readl(&dsi_reg->phy_ctrl2));
		pr_info("phy_ctrl3  (@%p): 0x%x\n",
			&dsi_reg->phy_ctrl3, readl(&dsi_reg->phy_ctrl3));
		pr_info("phy_status0(@%p): 0x%x\n",
			&dsi_reg->phy_status0, readl(&dsi_reg->phy_status0));
		pr_info("phy_status1(@%p): 0x%x\n",
			&dsi_reg->phy_status1, readl(&dsi_reg->phy_status1));
		pr_info("phy_status2(@%p): 0x%x\n",
			&dsi_reg->phy_status2, readl(&dsi_reg->phy_status2));
		pr_info("phy_rcomp0 (@%p): 0x%x\n",
			&dsi_reg->phy_rcomp0, readl(&dsi_reg->phy_rcomp0));
		pr_info("phy_timing0(@%p): 0x%x\n",
			&dsi_reg->phy_timing0, readl(&dsi_reg->phy_timing0));
		pr_info("phy_timing1(@%p): 0x%x\n",
			&dsi_reg->phy_timing1, readl(&dsi_reg->phy_timing1));
		pr_info("phy_timing2(@%p): 0x%x\n",
			&dsi_reg->phy_timing2, readl(&dsi_reg->phy_timing2));
		pr_info("phy_timing3(@%p): 0x%x\n",
			&dsi_reg->phy_timing3, readl(&dsi_reg->phy_timing3));
		pr_info("phy_code_0 (@%p): 0x%x\n",
			&dsi_reg->phy_code_0, readl(&dsi_reg->phy_code_0));
		pr_info("phy_code_1 (@%p): 0x%x\n",
			&dsi_reg->phy_code_1, readl(&dsi_reg->phy_code_1));
	}
	pr_info("mem_ctrl   (@%p): 0x%x\n",
		&dsi_reg->mem_ctrl, readl(&dsi_reg->mem_ctrl));
	pr_info("tx_timer   (@%p): 0x%x\n",
		&dsi_reg->tx_timer, readl(&dsi_reg->tx_timer));
	pr_info("rx_timer   (@%p): 0x%x\n",
		&dsi_reg->rx_timer, readl(&dsi_reg->rx_timer));
	pr_info("turn_timer (@%p): 0x%x\n",
		&dsi_reg->turn_timer, readl(&dsi_reg->turn_timer));

	pr_info("lcd1 regs\n");
	pr_info("ctrl0     (@%p): 0x%x\n",
		&dsi_reg->lcd1.ctrl0, readl(&dsi_reg->lcd1.ctrl0));
	pr_info("ctrl1     (@%p): 0x%x\n",
		&dsi_reg->lcd1.ctrl1, readl(&dsi_reg->lcd1.ctrl1));
	pr_info("timing0   (@%p): 0x%x\n",
		&dsi_reg->lcd1.timing0, readl(&dsi_reg->lcd1.timing0));
	pr_info("timing1   (@%p): 0x%x\n",
		&dsi_reg->lcd1.timing1, readl(&dsi_reg->lcd1.timing1));
	pr_info("timing2   (@%p): 0x%x\n",
		&dsi_reg->lcd1.timing2, readl(&dsi_reg->lcd1.timing2));
	pr_info("timing3   (@%p): 0x%x\n",
		&dsi_reg->lcd1.timing3, readl(&dsi_reg->lcd1.timing3));
	pr_info("wc0       (@%p): 0x%x\n",
		&dsi_reg->lcd1.wc0, readl(&dsi_reg->lcd1.wc0));
	pr_info("wc1       (@%p): 0x%x\n",
		&dsi_reg->lcd1.wc1, readl(&dsi_reg->lcd1.wc1));
	pr_info("wc2       (@%p): 0x%x\n",
		&dsi_reg->lcd1.wc2, readl(&dsi_reg->lcd1.wc2));
	pr_info("slot_cnt0 (@%p): 0x%x\n",
		&dsi_reg->lcd1.slot_cnt0, readl(&dsi_reg->lcd1.slot_cnt0));
	pr_info("slot_cnt1 (@%p): 0x%x\n",
		&dsi_reg->lcd1.slot_cnt1, readl(&dsi_reg->lcd1.slot_cnt1));
	pr_info("status_0  (@%p): 0x%x\n",
		&dsi_reg->lcd1.status_0, readl(&dsi_reg->lcd1.status_0));
	pr_info("status_1  (@%p): 0x%x\n",
		&dsi_reg->lcd1.status_1, readl(&dsi_reg->lcd1.status_1));
	pr_info("status_2  (@%p): 0x%x\n",
		&dsi_reg->lcd1.status_2, readl(&dsi_reg->lcd1.status_2));
	pr_info("status_3  (@%p): 0x%x\n",
		&dsi_reg->lcd1.status_3, readl(&dsi_reg->lcd1.status_3));
	pr_info("status_4  (@%p): 0x%x\n",
		&dsi_reg->lcd1.status_4, readl(&dsi_reg->lcd1.status_4));
}

static const struct gen_panel_ops gp_ops = {
	.tx_cmds = mmp_panel_tx_cmd_array,
	.rx_cmds = mmp_panel_rx_cmd_array,
#if CONFIG_OF
	.parse_dt = mmp_panel_parse_dt,
#endif
	.dump = mmp_panel_dump,
};

static void mmp_panel_set_power(struct mmp_panel *panel, int status)
{
	struct lcd *lcd = panel_to_lcd(panel);

	gen_panel_set_power_status(lcd, GEN_PANEL_PWR_ON_0);

	return;
}

static void mmp_panel_set_status(struct mmp_panel *panel, int status)
{
	struct lcd *lcd = panel_to_lcd(panel);

	gen_panel_set_status(lcd, status);

	return;
}

static void mmp_panel_start(struct mmp_panel *panel, int status)
{
	struct lcd *lcd = panel_to_lcd(panel);

	gen_panel_start(lcd, status);

	return;
}

static void mmp_panel_esd_onoff(struct mmp_panel *panel, int status)
{
	struct lcd *lcd = panel_to_lcd(panel);

	if (!lcd->esd_en)
		return;

	if (status)
		esd_start(&panel->esd);
	else
		esd_stop(&panel->esd);
}

static int mmp_panel_get_status(struct mmp_panel *panel)
{
	return ESD_STATUS_OK;
}

static void mmp_panel_esd_recover(struct mmp_panel *panel)
{
	struct mmp_path *path =
		mmp_get_path(panel->plat_path_name);

	esd_panel_recover(path);
	pr_info("%s done\n", __func__);
}

static struct mmp_mode mmp_panel_modes[] = {
	[0] = {
		.refresh = 60,
		.invert_pixclock = 0,
		.pix_fmt_out = PIXFMT_BGR888PACK,
		.hsync_invert = 0,
		.vsync_invert = 0,
	},
};

static int mmp_panel_get_modelist(struct mmp_panel *panel,
		struct mmp_mode **modelist)
{
	*modelist = mmp_panel_modes;
	return 1;
}

static struct mmp_panel mmp_dsi_panel = {
	.name = "dummy panel",
	.panel_type = PANELTYPE_DSI_VIDEO,
	.get_modelist = mmp_panel_get_modelist,
	.set_power = mmp_panel_set_power,
	.set_status = mmp_panel_set_status,
	.panel_start = mmp_panel_start,
	.panel_esd_recover = mmp_panel_esd_recover,
	.get_status = mmp_panel_get_status,
	.esd_set_onoff = mmp_panel_esd_onoff,
};

static int mmp_panel_probe(struct platform_device *pdev)
{
	struct lcd *lcd;
	struct mmp_panel *panel = &mmp_dsi_panel;
	int ret;

	pr_info("called %s\n", __func__);

	lcd = kzalloc(sizeof(*lcd), GFP_KERNEL);
	if (unlikely(!lcd))
		return -ENOMEM;

	if (IS_ENABLED(CONFIG_OF)) {
		/* 0. Parse : path-name*/
		ret = of_property_read_string(pdev->dev.of_node,
				"marvell,path-name", &panel->plat_path_name);
		if (unlikely(ret)) {
			dev_err(&pdev->dev, "fail to parse path-name\n");
			goto err_parse_path_name;
		}
	}

	lcd->pdata = (void *)panel;
	lcd->ops = &gp_ops;

	ret = gen_panel_probe(pdev->dev.of_node, lcd);
	if (unlikely(ret))
		goto err_gen_panel_probe;

	gpmode_to_mmpmode(&mmp_panel_modes[0], &lcd->mode);

	if (lcd->esd_en) {
		panel->esd_type = lcd->esd_type;
		panel->esd_gpio = lcd->esd_gpio;
		esd_init(panel);
	}
	panel->plat_data = lcd;
	panel->dev = &pdev->dev;
	panel->name = lcd->panel_name;
	mmp_register_panel(panel);

	return 0;

err_parse_path_name:
err_gen_panel_probe:
	pr_info("%s dts parsing error\n", __func__);
	kfree(lcd);
	return ret;
}

static int mmp_panel_remove(struct platform_device *pdev)
{
	struct lcd *lcd = dev_get_drvdata(&pdev->dev);
	struct mmp_panel *panel = lcd_to_panel(lcd);

	gen_panel_remove(lcd);
	kfree(lcd);
	mmp_unregister_panel(panel);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mmp_panel_dt_match[] = {
	{ .compatible = "marvell,mmp-dsi-panel" },
	{},
};
#endif
static struct platform_driver mmp_panel_driver = {
	.driver     = {
		.name   = "mmp-dsi-panel",
		.owner  = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(mmp_panel_dt_match),
#endif
	},

	.probe      = mmp_panel_probe,
	.remove     = mmp_panel_remove,
};

static int mmp_panel_module_init(void)
{
	return platform_driver_register(&mmp_panel_driver);
}
static void mmp_panel_module_exit(void)
{
	platform_driver_unregister(&mmp_panel_driver);
}
module_init(mmp_panel_module_init);
module_exit(mmp_panel_module_exit);

MODULE_DESCRIPTION("MMP DSI PANEL DRIVER");
MODULE_LICENSE("GPL");
