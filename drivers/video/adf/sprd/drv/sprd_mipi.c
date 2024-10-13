/*
 *Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 *This software is licensed under the terms of the GNU General Public
 *License version 2, as published by the Free Software Foundation, and
 *may be copied, distributed, and modified under those terms.
 *
 *This program is distributed in the hope that it will be useful,
 *but WITHOUT ANY WARRANTY; without even the implied warranty of
 *MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *GNU General Public License for more details.
 */

#include <linux/kernel.h>

#include "sprd_display.h"
#include "sprd_interface.h"
#include "sprd_dispc_reg.h"

static int32_t sprd_mipi_panel_check(struct panel_spec *panel)
{
	if (NULL == panel) {
		PERROR("(Invalid param)\n");
		return false;
	}

	if (SPRDFB_PANEL_TYPE_MIPI != panel->type) {
		PERROR("(not mipi param)\n");
		return false;
	}

	return true;
}

static void sprd_mipi_panel_mount(struct panel_if_device *intf)
{
	struct panel_spec *panel;

	if ((NULL == intf) || (NULL == intf->info.mipi->panel)) {
		PRINT("Invalid Param\n");
		return;
	}
	panel = intf->get_panel(intf);

	if (SPRDFB_MIPI_MODE_CMD == panel->work_mode)
		intf->panel_if_type = SPRDFB_PANEL_IF_EDPI;
	else
		intf->panel_if_type = SPRDFB_PANEL_IF_DPI;

	intf->info.mipi->ops = &sprd_mipi_ops;
	if (NULL == panel->ops->panel_readid) {
		PERROR("panel_readid not define\n");
		return;
	}
	/*just for write to regitster */
	intf->panel_timing.rgb_timing[RGB_LCD_H_TIMING] =
	    RGB_CALC_H_TIMING(panel->timing.rgb_timing);
	intf->panel_timing.rgb_timing[RGB_LCD_V_TIMING] =
	    RGB_CALC_V_TIMING(panel->timing.rgb_timing);

}

static bool sprd_mipi_panel_init(struct panel_if_device *intf)
{
	sprd_dsi_init(intf);
	/*set in  dispc driver */
	/* mipi_dispc_init_config(intf->info.mipi->panel); */
	/* dispc_write */
	/* mipi_dispc_set_timing(dev); */
	return true;
}

static void sprd_mipi_panel_uninit(struct panel_if_device *intf)
{
	sprd_dsi_uninit(intf->info.mipi->dsi_ctx);
}

static void sprd_mipi_panel_ready(struct panel_if_device *intf)
{
	sprd_dsi_ready(intf->info.mipi->dsi_ctx);
}

static void sprd_mipi_panel_before_reset(struct panel_if_device *intf)
{
	sprd_dsi_before_panel_reset(intf->info.mipi->dsi_ctx);
}

static void sprd_mipi_panel_enter_ulps(struct panel_if_device *intf)
{
	sprd_dsi_enter_ulps(intf->info.mipi->dsi_ctx);
}

static void sprd_mipi_panel_suspend(struct panel_if_device *intf)
{
	 /* sprd_dsi_uninit(intf->info.mipi->dsi_ctx);*/
	 sprd_dsi_suspend(intf->info.mipi->dsi_ctx);
}

static void sprd_mipi_panel_resume(struct panel_if_device *intf)
{
	sprd_dsi_init(intf);
	/* sprd_dsi_resume(dev); */
}

static uint32_t sprd_mipi_get_status(struct panel_if_device *intf)
{
	return sprd_dsi_get_status(intf->info.mipi->dsi_ctx);
}

static int32_t sprd_mipi_panel_attach(struct panel_spec *panel,
				struct panel_if_device *intf)
{

	/*we can read id in this function.
	 *the same id,the same panel_mode type.
	 */
	panel->type = SPRDFB_PANEL_TYPE_MIPI;
	if (panel->dev_id == intf->dev_id && panel->type == intf->type) {
		struct info_mipi *mipi = intf->info.mipi;
		struct sprd_dsi_context *dsi_ctx = mipi->dsi_ctx;

		mipi->panel = panel;
		panel->ops = &lcd_mipi_ops;
		dsi_ctx->is_inited = false;
		dsi_ctx->dsi_inst.dev_id = intf->dev_id;
		/*TODO: need set panel reset
		   panel_reset_dispc();
		 */
		sprd_mipi_panel_mount(intf);
		sprd_mipi_panel_init(intf);

		/*TODO : read id to check mipi if is OK. */
		return true;
	}
	return false;
}

static int32_t sprd_mipi_dphy_chg_freq(struct panel_if_device *intf,
					int dphy_freq)
{
	sprd_dsi_chg_dphy_freq(intf, dphy_freq);
	return 0;
}
static int32_t sprd_mipi_esd(struct panel_if_device *intf)
{
	struct panel_spec *panel = intf->get_panel(intf);
	if (panel) {
		PRINT("intferace%d %s do esd\n", intf->dev_id,
			intf->ctrl->if_name);
		return panel->ops->panel_esd_check(intf);
	}
	return -ENODEV;
}
struct panel_if_ctrl sprd_mipi_ctrl = {
	.if_name = "mipi",
	.panel_if_esd = sprd_mipi_esd,
	.panel_if_chg_freq = sprd_mipi_dphy_chg_freq,
	.panel_if_attach = sprd_mipi_panel_attach,
	.panel_if_check = sprd_mipi_panel_check,
	.panel_if_mount = sprd_mipi_panel_mount,
	.panel_if_init = sprd_mipi_panel_init,
	.panel_if_uninit = sprd_mipi_panel_uninit,
	.panel_if_ready = sprd_mipi_panel_ready,
	.panel_if_before_refresh = NULL,
	.panel_if_after_refresh = NULL,
	.panel_if_before_panel_reset = sprd_mipi_panel_before_reset,
	.panel_if_enter_ulps = sprd_mipi_panel_enter_ulps,
	.panel_if_suspend = sprd_mipi_panel_suspend,
	.panel_if_resume = sprd_mipi_panel_resume,
	.panel_if_get_status = sprd_mipi_get_status,
};
