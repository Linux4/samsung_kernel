// SPDX-License-Identifier: GPL-2.0-only
/*
 * MIPI-DSI based Samsung common panel driver.
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/debugfs.h>
#include <linux/init.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>

#include <drm/drm_panel.h>
#include <drm/drm_encoder.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_edid.h>
#include <drm/drm_print.h>

#include <exynos_drm_decon.h>
#include <exynos_drm_dsim.h>
#include <exynos_display_common.h>
#include "../exynos_drm_crtc.h"
#include "../exynos_drm_connector.h"
#include "../exynos_drm_tui.h"
#include "../exynos_drm_dqe.h"
#include "panel-samsung-drv.h"
#include "../exynos_drm_dsc.h"

static int panel_log_level = 6;
module_param(panel_log_level, int, 0644);
MODULE_PARM_DESC(panel_log_level, "log level for panel drv [default : 6]");

int get_panel_log_level(void)
{
	return panel_log_level;
}
EXPORT_SYMBOL(get_panel_log_level);

#include <video/of_display_timing.h>

struct edid panel_edid;
static struct exynos_panel *base_ctx;

/* string should sync-up with exynos_panel_cmd enum in panel-samsung-drv.h */
const char exynos_panel_cmd_string[][DRM_PROP_NAME_LEN] = {
	"exynos,on-cmds",
	"exynos,disp-on-cmds",
	"exynos,off-cmds",
	"exynos,pre-pps-cmds",
	"exynos,pps-cmds",
	"exynos,post-pps-cmds",
	"exynos,mres-cmds",
	"exynos,vref-cmds",
	"exynos,tectrl-cmds",
	"exynos,lp-mode-cmds",
	"cmd:out-of-range",
};

const char exynos_rcd_pos_string[][DRM_PROP_NAME_LEN] = {
	"exynos,rcd-top-pos",
	"exynos,rcd-bot-pos",
};

const char exynos_rcd_img_string[][DRM_PROP_NAME_LEN] = {
	"exynos,rcd-top-img",
	"exynos,rcd-bot-img",
};

static inline bool is_emul_mode(struct exynos_panel *ctx)
{
	struct mipi_dsi_device *dsi;
	struct dsim_device *dsim;

	dsi = to_mipi_dsi_device(ctx->dev);
	if (!dsi || !dsi->host)
		return false;

	dsim = container_of(dsi->host, struct dsim_device, dsi_host);

	return dsim->emul_mode;
}

static void notify_lp11_reset(struct exynos_panel *ctx, bool en)
{
	struct mipi_dsi_device *dsi;
	struct dsim_device *dsim;

	dsi = to_mipi_dsi_device(ctx->dev);
	if (!dsi || !dsi->host) {
		panel_err(ctx, "dsi not attached yet\n");
		return;
	}

	dsim = container_of(dsi->host, struct dsim_device, dsi_host);

	dsim->lp11_reset = en;
}

static void notify_lp11_wait(struct exynos_panel *ctx, bool en)
{
	struct mipi_dsi_device *dsi;
	struct dsim_device *dsim;

	dsi = to_mipi_dsi_device(ctx->dev);
	if (!dsi || !dsi->host) {
		panel_err(ctx, "dsi not attached yet\n");
		return;
	}

	dsim = container_of(dsi->host, struct dsim_device, dsi_host);

	dsim->wait_lp11_after_cmds = en;
}

static int exynos_panel_parse_gpios(struct exynos_panel *ctx)
{
	struct device *dev = ctx->dev;

	panel_debug(ctx, "+\n");

	if (IS_ENABLED(CONFIG_BOARD_EMULATOR) || is_emul_mode(ctx)) {
		panel_info(ctx, "no reset/enable pins on emulator\n");
		return 0;
	}

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_ASIS);
	if (IS_ERR(ctx->reset_gpio)) {
		panel_err(ctx, "failed to get reset-gpios %ld",
				PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}

	ctx->enabled = gpiod_get_raw_value(ctx->reset_gpio) > 0;
	if (!ctx->enabled)
		gpiod_direction_output(ctx->reset_gpio, 0);

	ctx->enable_gpio = devm_gpiod_get(dev, "enable", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->enable_gpio))
		ctx->enable_gpio = NULL;

	ctx->vddi_gpio = devm_gpiod_get(dev, "vddi", GPIOD_ASIS);
	if (!IS_ERR(ctx->vddi_gpio) && !ctx->enabled) {
		gpiod_direction_output(ctx->vddi_gpio, 0);
	}

	ctx->vpos_gpio = devm_gpiod_get(dev, "vpos", GPIOD_ASIS);
	if (!IS_ERR(ctx->vpos_gpio) && !ctx->enabled) {
		gpiod_direction_output(ctx->vpos_gpio, 0);
	}

	ctx->vneg_gpio = devm_gpiod_get(dev, "vneg", GPIOD_ASIS);
	if (!IS_ERR(ctx->vpos_gpio) && !ctx->enabled) {
		gpiod_direction_output(ctx->vneg_gpio, 0);
	}

	panel_debug(ctx, "-\n");

	return 0;
}

static int exynos_panel_parse_regulators(struct exynos_panel *ctx)
{
	struct device *dev = ctx->dev;

	ctx->vddi = devm_regulator_get_optional(dev, "vddi");
	if (IS_ERR(ctx->vddi)) {
		panel_warn(ctx, "optional : no vddi.\n");
		ctx->vddi = NULL;
	}

	ctx->vddr = devm_regulator_get_optional(dev, "vddr");
	if (IS_ERR(ctx->vddr)) {
		panel_warn(ctx, "optional : no vddr.\n");
		ctx->vddr = NULL;
	}

	ctx->vci = devm_regulator_get_optional(dev, "vci");
	if (IS_ERR(ctx->vci)) {
		panel_warn(ctx, "optional : no vci.\n");
		ctx->vci = NULL;
	}

	ctx->blrdev = devm_regulator_get_optional(dev, "blrdev");
	if (IS_ERR(ctx->blrdev)) {
		panel_warn(ctx, "optional : no blrdev.\n");
		ctx->blrdev = NULL;
	}

	if (ctx->enabled) {
		if (ctx->vddi != NULL && regulator_enable(ctx->vddi))
			panel_warn(ctx, "vddi enable failed\n");

		if (ctx->vddr != NULL && regulator_enable(ctx->vddr))
			panel_warn(ctx, "vddr enable failed\n");

		if (ctx->vci != NULL && regulator_enable(ctx->vci))
			panel_warn(ctx, "vci enable failed\n");
	}

	return 0;
}

#define PANEL_ID_READ_SIZE	3
static int exynos_panel_read_id(struct exynos_panel *ctx)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	char buf[4] = {0, };
	int ret;

	ret = mipi_dsi_dcs_read(dsi, MIPI_DCS_GET_DISPLAY_ID, buf,
			PANEL_ID_READ_SIZE);
	if (ret != PANEL_ID_READ_SIZE) {
		panel_warn(ctx, "Unable to read panel id (%d)\n", ret);
		return ret;
	}
	panel_info(ctx, "read panel buf(0x%x%x%x%x)\n", buf[3], buf[2], buf[1], buf[0]);

	return 0;
}

void exynos_panel_reset(struct exynos_panel *ctx)
{
	int i;
	struct exynos_panel_desc *pdesc = ctx->desc;
	unsigned long delay;

	panel_debug(ctx, "+\n");

	if (IS_ENABLED(CONFIG_BOARD_EMULATOR) || is_emul_mode(ctx))
		return;

	if (pdesc->delay_before_reset > 0) {
		delay = (unsigned long)pdesc->delay_before_reset;
		usleep_range(delay, delay + 1000);
	}

	for (i = 0; i < 3; ++i) {
		gpiod_set_value(ctx->reset_gpio, pdesc->reset_seq[i]);
		if (pdesc->reset_delay[i] > 0) {
			delay = (unsigned long)pdesc->reset_delay[i];
			usleep_range(delay, delay + 1000);
		}
	}

	if (exynos_panel_get_reset_position(ctx) == PANEL_RESET_LP11_HS)
		exynos_panel_read_id(ctx);

	panel_debug(ctx, "-\n");
}
EXPORT_SYMBOL(exynos_panel_reset);

enum exynos_reset_pos exynos_panel_get_reset_position(struct exynos_panel *ctx)
{
	struct exynos_panel_desc *pdesc;
	enum exynos_reset_pos ret = PANEL_RESET_LP11_HS;

	if (!ctx)
		return ret;

	pdesc = ctx->desc;
	if (!pdesc)
		return ret;

	return pdesc->reset_pos;
}
EXPORT_SYMBOL(exynos_panel_get_reset_position);

int exynos_panel_set_power(struct exynos_panel *ctx, bool on)
{
	int ret;

	if (IS_ENABLED(CONFIG_BOARD_EMULATOR) || is_emul_mode(ctx))
		return 0;

#if 0
	if (!ctx->vddi) {
		panel_debug(ctx, "trying to get vddi regulator\n");
		ctx->vddi = devm_regulator_get(ctx->dev, "vddi");
		if (IS_ERR(ctx->vddi)) {
			panel_warn(ctx, "failed to get vddi regulator\n");
			ctx->vddi = NULL;
		}
	}

	if (!ctx->vci) {
		panel_debug(ctx, "trying to get vci regulator\n");
		ctx->vci = devm_regulator_get(ctx->dev, "vci");
		if (IS_ERR(ctx->vci)) {
			panel_warn(ctx, "failed to get vci regulator\n");
			ctx->vci = NULL;
		}
	}
#endif

	if (on) {
		if (ctx->enable_gpio) {
			gpiod_set_value(ctx->enable_gpio, 1);
			usleep_range(10000, 11000);
		}

		if (ctx->blen_gpio) {
			gpiod_set_value(ctx->blen_gpio, 1);
			usleep_range(10000, 11000);
		}

		if (ctx->vsp_gpio) {
			gpiod_set_value(ctx->vsp_gpio, 1);
			usleep_range(10000, 11000);
		}

		if (ctx->vsn_gpio) {
			gpiod_set_value(ctx->vsn_gpio, 1);
			usleep_range(10000, 11000);
		}

		if (ctx->vddi) {
			ret = regulator_enable(ctx->vddi);
			if (ret) {
				panel_err(ctx, "vddi enable failed\n");
				return ret;
			}
			usleep_range(5000, 6000);
		}

		if (ctx->vddi_gpio) {
			gpiod_set_value(ctx->vddi_gpio, 1);
			usleep_range(2000, 3000);
		}


		if (ctx->vddr) {
			ret = regulator_enable(ctx->vddr);
			if (ret) {
				panel_err(ctx, "vddr enable failed\n");
				return ret;
			}
		}

		if (ctx->vci) {
			ret = regulator_enable(ctx->vci);
			if (ret) {
				panel_err(ctx, "vci enable failed\n");
				return ret;
			}
		}

		if (ctx->vpos_gpio) {
			gpiod_set_value(ctx->vpos_gpio, 1);
			usleep_range(2000, 3000);
		}

		if (ctx->vneg_gpio) {
			gpiod_set_value(ctx->vneg_gpio, 1);
			usleep_range(15000, 16000);
		}

		if (ctx->blrdev) {
			ret = regulator_set_mode(ctx->blrdev, REGULATOR_MODE_NORMAL);
			if (ret)
				panel_err(ctx, "blrdev enabled failed\n");
		}
	} else {
		if (ctx->blrdev) {
			ret = regulator_set_mode(ctx->blrdev, REGULATOR_MODE_STANDBY);
			if (ret)
				panel_err(ctx, "blrdev disabled failed\n");
		gpiod_set_value(ctx->reset_gpio, 0);

		if (ctx->vsn_gpio) {
			gpiod_set_value(ctx->vsn_gpio, 0);
			usleep_range(10000, 11000);
		}

		if (ctx->vsp_gpio) {
			gpiod_set_value(ctx->vsp_gpio, 0);
			usleep_range(10000, 11000);
		}

		if (ctx->blen_gpio) {
			//todo: set blen gpio to 0 when i2c bringup is done
			//gpiod_set_value(ctx->blen_gpio, 0);
			usleep_range(10000, 11000);
		}


		if (ctx->enable_gpio)
			gpiod_set_value(ctx->enable_gpio, 0);

		if (ctx->vddr) {
			ret = regulator_disable(ctx->vddr);
			if (ret) {
				panel_err(ctx, "vddr disable failed\n");
				return ret;
			}
		}

		if (ctx->vddi) {
			ret = regulator_disable(ctx->vddi);
			if (ret) {
				panel_err(ctx, "vddi disable failed\n");
				return ret;
			}
		}

		if (ctx->vci > 0) {
			ret = regulator_disable(ctx->vci);
			if (ret) {
				panel_err(ctx, "vci disable failed\n");
				return ret;
			}
		}
	}

	ctx->bl->props.power = on ? FB_BLANK_UNBLANK : FB_BLANK_POWERDOWN;

	return 0;
}
EXPORT_SYMBOL(exynos_panel_set_power);

static int exynos_panel_parse_timings(struct exynos_panel *ctx);
static int exynos_panel_parse_dt(struct exynos_panel *ctx)
{
	int ret = 0;

	if (IS_ERR_OR_NULL(ctx->dev->of_node)) {
		panel_err(ctx, "no device tree information of exynos panel\n");
		return -EINVAL;
	}

	ret = exynos_panel_parse_gpios(ctx);
	if (ret)
		goto err;

	ret = exynos_panel_parse_regulators(ctx);
	if (ret)
		goto err;

	ret = exynos_panel_parse_timings(ctx);
	if (ret)
		goto err;

err:
	return ret;
}

int exynos_panel_get_modes(struct drm_panel *panel, struct drm_connector *conn)
{
	struct exynos_panel *ctx = container_of(panel, struct exynos_panel, panel);
	const struct exynos_panel_mode *pmode;
	struct drm_display_mode *mode;
	int num_added_modes = 0;
	int i;

	panel_debug(ctx, "+\n");

	for_each_panel_normal_mode(ctx->desc, pmode, i) {
		mode = drm_mode_duplicate(conn->dev, &pmode->mode);
		if (!mode) {
			panel_err(ctx, "failed to add mode %s\n", pmode->mode.name);
			return -ENOMEM;
		}

		drm_mode_probed_add(conn, mode);

		panel_debug(ctx, "added display mode: %s\n", mode->name);

		if ((mode->type & DRM_MODE_TYPE_PREFERRED))
			panel_debug(ctx, "preferred display mode: %s\n", mode->name);

		++num_added_modes;
	}

	panel_debug(ctx, "-\n");

	return num_added_modes;
}
EXPORT_SYMBOL(exynos_panel_get_modes);

void exynos_panel_set_lp_mode(struct exynos_panel *ctx, const struct exynos_panel_mode *pmode)
{
	if (!ctx->enabled)
		return;

	panel_info(ctx, "enter %dhz LP mode\n", drm_mode_vrefresh(&pmode->mode));
}
EXPORT_SYMBOL(exynos_panel_set_lp_mode);

static int exynos_get_brightness(struct backlight_device *bl)
{
	int ret;
	u16 brightness;
	struct exynos_panel *ctx = bl_get_data(bl);

	if (!ctx->enabled) {
		panel_err(ctx, "panel is not enabled\n");
		return -EPERM;
	}

	ret = exynos_dcs_get_brightness(ctx, &brightness);
	if (ret < 0)
		return ret;

	return brightness;
}

static int exynos_update_status(struct backlight_device *bl)
{
	int ret;
	struct exynos_panel *ctx = bl_get_data(bl);
	const struct exynos_panel_funcs *exynos_panel_func;
	int brightness = bl->props.brightness;

	panel_debug(ctx, "br: %d, max br: %d\n", brightness,
		bl->props.max_brightness);

	if (!ctx->enabled) {
		panel_err(ctx, "panel is not enabled\n");
		return -EPERM;
	}

	/* check if backlight is forced off */
	if (bl->props.power != FB_BLANK_UNBLANK)
		brightness = 0;

	exynos_panel_func = ctx->func_set->exynos_panel_func;
	if (exynos_panel_func && exynos_panel_func->set_brightness)
		ret = exynos_panel_func->set_brightness(ctx, brightness);
	else
		ret = exynos_dcs_set_brightness(ctx, brightness);

	return ret;
}

static const struct backlight_ops exynos_backlight_ops = {
	.get_brightness = exynos_get_brightness,
	.update_status = exynos_update_status,
};

static void exynos_panel_connector_print_state(struct drm_printer *p,
		const struct exynos_drm_connector_state *exynos_conn_state)
{
	const struct exynos_drm_connector *exynos_conn =
		to_exynos_connector(exynos_conn_state->base.connector);
	const struct exynos_display_mode *exynos_mode =
		&exynos_conn_state->exynos_mode;
	const struct exynos_panel *ctx = exynos_connector_to_panel(exynos_conn);
	const struct exynos_panel_desc *desc = ctx->desc;

	drm_printf(p, "\tenabled: %d\n", ctx->enabled);
	if (ctx->current_mode) {
		const struct drm_display_mode *m = &ctx->current_mode->mode;

		drm_printf(p, "\tcurrent mode: %s\n", m->name);
	}
	if (exynos_mode) {
		const struct exynos_display_dsc *dsc = &exynos_mode->dsc;

		drm_printf(p, "\tcurrent exynos_mode:\n");
		drm_printf(p, "\t\tdsc: en=%d dsc_cnt=%d slice_cnt=%d slice_h=%d\n",
				dsc->enabled, dsc->dsc_count, dsc->slice_count,
				dsc->slice_height);
		drm_printf(p, "\t\tpanel bpc: %d\n", exynos_mode->bpc);
		drm_printf(p, "\t\top_mode: %s\n", exynos_mode->mode_flags &
				MIPI_DSI_MODE_VIDEO ? "video" : "cmd");
		drm_printf(p, "\t\tlp_mode_state: %d\n", exynos_mode->is_lp_mode);
		drm_printf(p, "\t\tbts_fps: %d\n", exynos_mode->bts_fps);
	}
	drm_printf(p, "\tluminance: [%u, %u] avg: %u\n",
		   desc->min_luminance, desc->max_luminance,
		   desc->max_avg_luminance);
	drm_printf(p, "\thdr_formats: 0x%x\n", desc->hdr_formats);
	drm_printf(p, "\tadjusted_fps: %d\n", exynos_conn_state->adjusted_fps);
}

int exynos_drm_cmdset_add(struct exynos_panel *ctx, u8 type, size_t size,
			  const u8 *data)
{
	u8 *buf;
	int index;

	if (data == NULL || size <= 0)
		return -EINVAL;

	if (exynos_mipi_dsi_packet_format_is_read(type))
		return -EINVAL;

	if (ctx->cmdset_msg_total >= MAX_CMDSET_NUM ||
	    ctx->cmdset_payload_total + size >= MAX_CMDSET_PAYLOAD) {
		panel_err(ctx, "Command set buffer is full\n");
		return -EINVAL;
	}

	buf = kzalloc(sizeof(u8) * size, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;

	memcpy(buf, data, size);
	index = ctx->cmdset_msg_total++;
	ctx->msg[index].type = type;
	ctx->msg[index].tx_buf = buf;
	ctx->msg[index].tx_len = size;
	ctx->cmdset_payload_total += size;

	panel_debug(ctx, "%d msgs, %d payload\n",
		 ctx->cmdset_msg_total, ctx->cmdset_payload_total);

	return 0;
}
EXPORT_SYMBOL(exynos_drm_cmdset_add);

int exynos_drm_cmdset_cleanup(struct exynos_panel *ctx)
{
	int i;
	int msg_total = ctx->cmdset_msg_total;

	panel_debug(ctx, "msg:%d\n", ctx->cmdset_msg_total);

	for (i = 0; i < msg_total; i++) {
		if (ctx->msg[i].tx_buf != NULL)
			kfree(ctx->msg[i].tx_buf);
		ctx->msg[i].tx_buf = NULL;
		ctx->msg[i].type = 0;
		ctx->msg[i].tx_len = 0;
	}
	ctx->cmdset_msg_total = 0;
	ctx->cmdset_payload_total = 0;

	panel_debug(ctx, "-\n");

	return 0;
}
EXPORT_SYMBOL(exynos_drm_cmdset_cleanup);

int exynos_drm_cmdset_flush(struct exynos_panel *ctx, bool wait_vsync,
							bool wait_fifo)
{
	int ret;
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	struct mipi_dsi_msg *msg_head;

	if (!ctx->cmdset_msg_total) {
		panel_err(ctx, "there is no MIPI command to transfer\n");
		return -EINVAL;
	}

	msg_head = ctx->msg;
	if (dsi->mode_flags & MIPI_DSI_MODE_LPM)
		msg_head->flags |= MIPI_DSI_MSG_USE_LPM;

	ret = dsim_host_cmdset_transfer(dsi->host, ctx->msg, ctx->cmdset_msg_total,
									wait_vsync, wait_fifo);
	if (ret < 0)
		panel_err(ctx, "failed to tx command set data\n");

	ret = exynos_drm_cmdset_cleanup(ctx);
	if (ret < 0)
		panel_err(ctx, "failed to cleanup command set data\n");

	return ret;
}
EXPORT_SYMBOL(exynos_drm_cmdset_flush);

static int
exynos_panel_connector_set_property(struct exynos_drm_connector *exynos_conn,
			struct exynos_drm_connector_state *exynos_conn_state,
			struct drm_property *property, uint64_t val)
{
	/* if necessary, fill it up */
	return 0;
}

static int
exynos_panel_connector_get_property(struct exynos_drm_connector *exynos_conn,
			const struct exynos_drm_connector_state *exynos_conn_state,
			struct drm_property *property, uint64_t *val)
{
	struct exynos_panel *ctx = exynos_connector_to_panel(exynos_conn);
	const struct exynos_drm_connector_properties *p =
		exynos_drm_connector_get_properties(&ctx->exynos_connector);

	if (property == p->max_luminance)
		*val = ctx->desc->max_luminance;
	else if (property == p->max_avg_luminance)
		*val = ctx->desc->max_avg_luminance;
	else if (property == p->min_luminance)
		*val = ctx->desc->min_luminance;
	else if (property == p->hdr_formats)
		*val = ctx->desc->hdr_formats;
	else if (property == p->adjusted_fps)
		*val = exynos_conn_state->adjusted_fps;
	else
		return -EINVAL;

	return 0;
}

#define PANEL_QUERY_CMD_COUNT	3
/*
 * - MIPI_DCS_GET_ERROR_COUNT_ON_DSI
 * : The display module returns the number of corrupted packets previously
 *   received on the DSI link.
 *
 * - MIPI_DCS_GET_POWER_MODE
 * : The display module returns the current power mode.
 *   D2 : display off/on
 *   D3 : normal mode off/on
 *   D4 : sleep mode on/off ('0' = sleep mode on, '1' = sleep mode off)
 *   D5 : partial mode off/on
 *   D6 : idle mode off/on
 *
 * - MIPI_DCS_GET_SIGNAL_MODE
 * : The display module returns the Display Signal Mode.
 *   D0 : error on dsi
 *   D6 : TE output mode
 *   D7 : TE off/on
 */
static void
exynos_panel_connector_query_status(struct exynos_drm_connector *exynos_conn)
{
	struct exynos_panel *ctx = exynos_connector_to_panel(exynos_conn);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	u32 i;
	u8 dcs_cmd[PANEL_QUERY_CMD_COUNT] = {MIPI_DCS_GET_ERROR_COUNT_ON_DSI,
			MIPI_DCS_GET_POWER_MODE, MIPI_DCS_GET_SIGNAL_MODE};
	char status[PANEL_QUERY_CMD_COUNT];
	int ret[PANEL_QUERY_CMD_COUNT];

	if (!ctx->enabled) {
		panel_info(ctx, "panel is not enabled\n");
		return;
	}

	for (i = 0; i < PANEL_QUERY_CMD_COUNT; i++) {
		ret[i] = mipi_dsi_dcs_read(dsi, dcs_cmd[i], &status[i], 1);
		if (ret[i] <= 0)
			goto query_end;
	}

query_end:
	for (i = 0; i < PANEL_QUERY_CMD_COUNT; i++) {
		if (ret[i] == 1)
			panel_info(ctx, "0x%02X = 0x%02x\n", dcs_cmd[i], status[i]);
		else {
			if (ret[i] == 0)
				ret[i] = -ENODATA;

			panel_err(ctx, "Unable to read 0x%02X(ret = %d)\n",
					dcs_cmd[i], ret[i]);
			break;
		}
	}
}

static bool
exynos_panel_connector_check_connection(struct exynos_drm_connector *exynos_conn)
{
	struct exynos_panel *ctx = exynos_connector_to_panel(exynos_conn);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	u8 dcs_cmd = MIPI_DCS_GET_POWER_MODE;
	char status;
	int ret;
	bool connected = true;

	if (!ctx->enabled) {
		panel_info(ctx, "panel is not enabled\n");
		goto exit;
	}

	ret = mipi_dsi_dcs_read(dsi, dcs_cmd, &status, 1);
	if (ret == -ETIMEDOUT)
		connected = false;

exit:
	return connected;
}

static const struct exynos_drm_connector_funcs exynos_panel_connector_funcs = {
	.atomic_print_state = exynos_panel_connector_print_state,
	.atomic_set_property = exynos_panel_connector_set_property,
	.atomic_get_property = exynos_panel_connector_get_property,
	.query_status = exynos_panel_connector_query_status,
	.check_connection = exynos_panel_connector_check_connection,
};

static int exynos_drm_connector_modes(struct drm_connector *connector)
{
	struct exynos_panel *ctx = connector_to_exynos_panel(connector);
	int ret;

	ret = drm_panel_get_modes(&ctx->panel, connector);
	if (ret < 0) {
		panel_err(ctx, "failed to get panel display modes\n");
		return ret;
	}

	return ret;
}

static const struct exynos_panel_mode *
exynos_panel_get_mode(struct exynos_panel *ctx,
			const struct drm_display_mode *mode)
{
	const struct exynos_panel_mode *pmode;
	int i;

	for_each_all_panel_mode(ctx->desc, pmode, i) {
		if (drm_mode_equal(&pmode->mode, mode))
			return pmode;
	}

	panel_err(ctx, "fail to get panel mode matching w/ mode(%s)\n",
			mode->name);

	return NULL;
}

static void exynos_drm_connector_check_seamless_modeset(
			struct exynos_drm_connector_state *exynos_conn_state,
			const struct exynos_panel_mode *new_pmode,
			const struct exynos_panel_mode *old_pmode)
{
	const struct drm_display_mode *new_mode;
	const struct drm_display_mode *old_mode;

	if (!old_pmode)
		return;

	new_mode = &new_pmode->mode;
	old_mode = &old_pmode->mode;

	if (!drm_mode_match(new_mode, old_mode, DRM_MODE_MATCH_TIMINGS))
		exynos_conn_state->seamless_modeset |= SEAMLESS_MODESET_MRES;
	if (drm_mode_vrefresh(new_mode) != drm_mode_vrefresh(old_mode))
		exynos_conn_state->seamless_modeset |= SEAMLESS_MODESET_VREF;
	if (new_pmode->exynos_mode.is_lp_mode != old_pmode->exynos_mode.is_lp_mode)
		exynos_conn_state->seamless_modeset |= SEAMLESS_MODESET_LP;
}

static inline bool is_seamless_mode_change(struct drm_crtc_state *crtc_state,
					const struct exynos_panel_mode *pmode)
{
	return (crtc_state->active && !crtc_state->active_changed);
}

static int exynos_drm_connector_atomic_check(struct drm_connector *connector,
				      struct drm_atomic_state *state)
{
	struct drm_connector_state *connector_state =
		drm_atomic_get_new_connector_state(state, connector);
	struct exynos_drm_connector_state *exynos_conn_state =
		to_exynos_connector_state(connector_state);
	struct drm_crtc_state *new_crtc_state, *old_crtc_state;
	struct exynos_panel *ctx = connector_to_exynos_panel(connector);
	const struct exynos_panel_mode *pmode;
	const struct exynos_panel_mode *old_pmode;

	if (!connector_state->best_encoder) {
		panel_err(ctx, "encoder is null\n");
		return 0;
	}

	old_crtc_state = drm_atomic_get_old_crtc_state(state, connector_state->crtc);
	new_crtc_state = drm_atomic_get_new_crtc_state(state, connector_state->crtc);

	pmode = exynos_panel_get_mode(ctx, &new_crtc_state->mode);
	if (!pmode) {
		panel_err(ctx, "%s can't support none panel mode\n",
				new_crtc_state->mode.name);
		return -EINVAL;
	}

	old_pmode = exynos_panel_get_mode(ctx, &old_crtc_state->mode);
	if (old_pmode) {
		if ((old_pmode->exynos_mode.is_lp_mode && pmode->exynos_mode.is_lp_mode) &&
			((old_crtc_state->active == 1 && new_crtc_state->active == 0) ||	/* doze->doze_suspend */
			(old_crtc_state->active == 0 && new_crtc_state->active == 1)))		/* doze_suspend->doze */
			exynos_conn_state->is_lp_transition = 1;

		panel_debug(ctx, "old lp_mode(%d), old crtc active(%d)\n",
			old_pmode->exynos_mode.is_lp_mode, old_crtc_state->active);
		panel_debug(ctx, "new lp_mode(%d), new crtc active(%d)\n",
			pmode->exynos_mode.is_lp_mode, new_crtc_state->active);
		panel_debug(ctx, "is_lp_transition(%d)\n",
			exynos_conn_state->is_lp_transition);
	}

	if (!new_crtc_state->mode_changed)
		return 0;

	exynos_conn_state->exynos_mode = pmode->exynos_mode;
	if (!exynos_conn_state->exynos_mode.bts_fps)
		exynos_conn_state->exynos_mode.bts_fps =
				drm_mode_vrefresh(&pmode->mode);

	if (is_seamless_mode_change(new_crtc_state, pmode)) {
		exynos_drm_connector_check_seamless_modeset(
				exynos_conn_state, pmode, old_pmode);
	}

	panel_debug(ctx, "old mode_changed(%d), active_changed(%d)\n",
			old_crtc_state->mode_changed,
			old_crtc_state->active_changed);
	panel_debug(ctx, "new mode_changed(%d), active_changed(%d)\n",
			new_crtc_state->mode_changed,
			new_crtc_state->active_changed);

	return 0;
}

static const struct drm_connector_helper_funcs exynos_connector_helper_funcs = {
	.atomic_check = exynos_drm_connector_atomic_check,
	.get_modes = exynos_drm_connector_modes,
};

static u8 exynos_drm_connector_edid_get_checksum(const u8 *raw_edid)
{
	int i;
	u8 csum = 0;

	for (i = 0; i < EDID_LENGTH; i++)
		csum += raw_edid[i];

	return csum;
}

static void exynos_drm_connector_edid(struct drm_connector *connector)
{
	int ret;
	struct exynos_panel *ctx = connector_to_exynos_panel(connector);
	struct drm_display_info *display_info = &connector->display_info;
	struct edid *edid = &panel_edid;
	const u8 edid_header[] = {0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00};
	const u8 edid_display_name[] = {0x73, 0x61, 0x6d, 0x73, 0x75, 0x6e, 0x67, 0x20, 0x6c, 0x63, 0x64, 0x20, 0x20};

	memset(edid, 0, sizeof(struct edid));
	memcpy(edid, edid_header, sizeof(edid_header));

	/*
	 * If you want to manipulate EDID information, use member variables
	 * of edid structure in here.
	 *
	 * ex) edid.width_cm = xx; edid.height_cm = yy
	 */
	edid->mfg_id[0] = 0x4C;    // manufacturer ID for samsung
	edid->mfg_id[1] = 0x2D;
	edid->mfg_week = 0x10;	/* 16 week */
	edid->mfg_year = 0x1E;	/* 1990 + 30 = 2020 year */
	edid->detailed_timings[0].data.other_data.type = 0xfc;  // for display name
	memcpy(edid->detailed_timings[0].data.other_data.data.str.str, edid_display_name, 13);
	/* sum of all 128 bytes should equal 0 (mod 0x100) */
	edid->checksum = 0x100 - exynos_drm_connector_edid_get_checksum((const u8 *)edid);

	pr_info("%s: checksum(0x%x)\n", __func__,
			exynos_drm_connector_edid_get_checksum((const u8 *)edid));

	connector->override_edid = false;
	ret = drm_connector_update_edid_property(connector, edid);
	display_info->width_mm = ctx->desc->width_mm;
	display_info->height_mm = ctx->desc->height_mm;
}

static int exynos_panel_attach_lp_mode(struct exynos_drm_connector *exynos_conn,
				       const struct exynos_panel_desc *desc)
{
	struct exynos_drm_connector_properties *p =
		exynos_drm_connector_get_properties(exynos_conn);
	struct drm_mode_modeinfo *umodes;
	struct drm_property_blob *blob;
	const struct exynos_panel_mode *pmode;
	int num_lp_modes;
	int ret = 0;
	int i;

	num_lp_modes = get_lp_mode_count(desc);
	umodes = kzalloc(num_lp_modes * sizeof(struct drm_mode_modeinfo), GFP_KERNEL);
	if (!umodes) {
		ret = -ENOMEM;
		goto err;
	}

	num_lp_modes = 0;
	for_each_panel_lp_mode(desc, pmode, i)
		drm_mode_convert_to_umode(&umodes[num_lp_modes++], &pmode->mode);

	blob = drm_property_create_blob(exynos_conn->base.dev,
			num_lp_modes * sizeof(struct drm_mode_modeinfo), umodes);
	if (IS_ERR(blob)) {
		ret = PTR_ERR(blob);
		goto err_blob;
	}

	drm_object_attach_property(&exynos_conn->base.base, p->lp_mode, blob->base.id);

err_blob:
	kfree(umodes);
err:
	return ret;
}

static int exynos_panel_attach_properties(struct exynos_panel *ctx)
{
	const struct exynos_drm_connector_properties *p =
		exynos_drm_connector_get_properties(&ctx->exynos_connector);
	struct drm_mode_object *obj = &ctx->exynos_connector.base.base;
	const struct exynos_panel_desc *desc = ctx->desc;
	int ret = 0;

	if (!p || !desc)
		return -ENOENT;

	drm_object_attach_property(obj, p->min_luminance, desc->min_luminance);
	drm_object_attach_property(obj, p->max_luminance, desc->max_luminance);
	drm_object_attach_property(obj, p->max_avg_luminance,
			desc->max_avg_luminance);
	drm_object_attach_property(obj, p->hdr_formats, desc->hdr_formats);
	drm_object_attach_property(obj, p->adjusted_fps, 0);

	if (desc->doze_supported) {
		ret = exynos_panel_attach_lp_mode(&ctx->exynos_connector, desc);
		if (ret)
			panel_err(ctx, "Failed to attach lp mode (%d)\n", ret);
	}

	return ret;
}

static int exynos_panel_bridge_attach(struct drm_bridge *bridge,
					enum drm_bridge_attach_flags flags)
{
	struct exynos_panel *ctx = bridge_to_exynos_panel(bridge);
	struct drm_connector *connector = &ctx->exynos_connector.base;
	int ret;

	ret = exynos_drm_connector_init(bridge->dev, &ctx->exynos_connector,
				 &exynos_panel_connector_funcs,
				 DRM_MODE_CONNECTOR_DSI);
	if (ret) {
		panel_err(ctx, "failed to initialize connector with drm\n");
		return ret;
	}

	ret = exynos_panel_attach_properties(ctx);
	if (ret) {
		panel_err(ctx, "failed to attach connector properties\n");
		return ret;
	}

	drm_connector_helper_add(connector, &exynos_connector_helper_funcs);

	ret = drm_connector_register(connector);
	if (ret)
		goto err;

	drm_connector_attach_encoder(connector, bridge->encoder);
	connector->funcs->reset(connector);
	connector->status = connector_status_connected;

	drm_kms_helper_hotplug_event(connector->dev);

	exynos_drm_connector_edid(connector);

	return 0;

err:
	drm_connector_unregister(connector);
	drm_connector_cleanup(connector);
	return ret;
}

static void exynos_panel_bridge_detach(struct drm_bridge *bridge)
{
	struct exynos_panel *ctx = bridge_to_exynos_panel(bridge);

	drm_connector_unregister(&ctx->exynos_connector.base);
	drm_connector_cleanup(&ctx->exynos_connector.base);
}

static inline bool is_bypass_panel(struct exynos_panel *ctx)
{
	struct exynos_drm_connector *exynos_conn = &ctx->exynos_connector;
	struct exynos_drm_connector_state *exynos_conn_state =
		to_exynos_connector_state(exynos_conn->base.state);

	return exynos_conn_state->bypass_panel;
}

static void exynos_panel_activate_encoder(struct drm_bridge *bridge)
{
	struct exynos_drm_encoder *exynos_encoder = to_exynos_encoder(bridge->encoder);
	const struct exynos_drm_encoder_funcs *funcs = exynos_encoder->funcs;

	if (funcs && funcs->atomic_activate)
		funcs->atomic_activate(exynos_encoder);
}

static void exynos_panel_enable(struct drm_bridge *bridge)
{
	struct exynos_panel *ctx = bridge_to_exynos_panel(bridge);
	struct exynos_drm_connector *exynos_conn = &ctx->exynos_connector;
	struct exynos_drm_connector_state *exynos_conn_state =
		to_exynos_connector_state(exynos_conn->base.state);
	const struct drm_display_mode *current_mode = &ctx->current_mode->mode;

	if (is_bypass_panel(ctx)) {
		panel_info(ctx, "bypass\n");
		return;
	}

	if (ctx->enabled) {
		panel_info(ctx, "panel is already initialized\n");
		return;
	}

	if (exynos_panel_get_reset_position(ctx) == PANEL_RESET_LP11) {
		exynos_panel_reset(ctx);
		exynos_panel_activate_encoder(bridge);
	}

	if (drm_panel_enable(&ctx->panel))
		return;

	exynos_conn_state->adjusted_fps = drm_mode_vrefresh(current_mode);
}

static void exynos_panel_pre_enable(struct drm_bridge *bridge)
{
	struct exynos_panel *ctx = bridge_to_exynos_panel(bridge);

	if (ctx->enabled) {
		panel_info(ctx, "panel is already initialized\n");
		return;
	}

	if (is_bypass_panel(ctx)) {
		panel_info(ctx, "bypass\n");
		return;
	}

	drm_panel_prepare(&ctx->panel);
}

static void exynos_panel_disable(struct drm_bridge *bridge)
{
	struct exynos_panel *ctx = bridge_to_exynos_panel(bridge);

	if (is_bypass_panel(ctx)) {
		panel_info(ctx, "bypass\n");
		return;
	}

	drm_panel_disable(&ctx->panel);
}

static void exynos_panel_post_disable(struct drm_bridge *bridge)
{
	struct exynos_panel *ctx = bridge_to_exynos_panel(bridge);

	if (is_bypass_panel(ctx)) {
		panel_info(ctx, "bypass\n");
		return;
	}

	drm_panel_unprepare(&ctx->panel);
}

static void exynos_panel_update_rcd(struct exynos_panel *ctx,
				    const struct exynos_panel_mode *pmode)
{
	struct drm_device *drm_dev = NULL;
	struct drm_crtc *crtc = NULL;
	struct exynos_drm_crtc *exynos_crtc = NULL;
	struct rcd_device *rcd;
	const struct exynos_display_rcd_desc *desc = &pmode->rcd_desc;
	struct rcd_params_info *param;
	int i;

	if (!IS_ENABLED(CONFIG_EXYNOS_DMA_RCD))
		return;

	panel_info(ctx, "+\n");

	drm_dev = ctx->exynos_connector.base.dev;
	if (!drm_dev) {
		panel_warn(ctx, "drm_dev has null\n");
		return;
	}

	drm_for_each_crtc(crtc, drm_dev)
		if (to_exynos_crtc(crtc)->possible_type & EXYNOS_DISPLAY_TYPE_DSI) {
			exynos_crtc = to_exynos_crtc(crtc);
			break;
		}

	if (!exynos_crtc) {
		panel_warn(ctx, "exynos_crtc has null\n");
		return;
	}

	if (!desc) {
		panel_warn(ctx, "rcd description is null\n");
		return;
	}

	rcd = exynos_crtc->rcd;
	if (!rcd || !rcd->funcs) {
		panel_warn(ctx, "cannot update rcd\n");
		return;
	}

	param = &rcd->param;
	param->total_width = pmode->mode.hdisplay;
	param->total_height = pmode->mode.vdisplay;
	param->num_win = desc->win_cnt;
	param->block_en = desc->block_en;
	param->block_rect.x = desc->block_rect[RCD_RECT_X];
	param->block_rect.y = desc->block_rect[RCD_RECT_Y];
	param->block_rect.w = desc->block_rect[RCD_RECT_W];
	param->block_rect.h = desc->block_rect[RCD_RECT_H];

	panel_debug(ctx,
		"total_w:%d, total_h:%d, win_cnt:%d, block en:%d, block pos:%d,%d,%d,%d\n",
		param->total_width, param->total_height, param->num_win, param->block_en,
		param->block_rect.x, param->block_rect.y, param->block_rect.w,
		param->block_rect.h);

	for (i = 0; i < desc->win_cnt; i++) {
		param->imgs[i].size = desc->rcd_wins[i]->img_size;
		param->imgs[i].buf = desc->rcd_wins[i]->img_buf;
		param->win_pos[i] = (u32*)&desc->rcd_wins[i]->position;

		panel_debug(ctx, "pmode:%p pos:%d,%d, size:%d\n", pmode,
			param->win_pos[i][0], param->win_pos[i][1], param->imgs[i].size);
	}

	rcd->funcs->update(rcd);

	panel_info(ctx, "-\n");
}

static void exynos_panel_bridge_mode_set(struct drm_bridge *bridge,
				  const struct drm_display_mode *mode,
				  const struct drm_display_mode *adjusted_mode)
{
	struct exynos_panel *ctx = bridge_to_exynos_panel(bridge);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	const struct exynos_panel_funcs *funcs = ctx->func_set->exynos_panel_func;
	const struct exynos_panel_mode *curr_pmode = ctx->current_mode;
	const struct exynos_panel_mode *pmode =
				exynos_panel_get_mode(ctx, adjusted_mode);

	panel_debug(ctx, "+\n");

	if (WARN_ON(!pmode))
		return;

	dsi->mode_flags = pmode->exynos_mode.mode_flags;

	if (funcs) {
		const bool is_lp_mode = pmode->exynos_mode.is_lp_mode;
		const struct drm_connector *conn = &ctx->exynos_connector.base;
		struct exynos_drm_connector_state *exynos_state =
			to_exynos_connector_state(conn->state);

		if (is_lp_mode && funcs->set_lp_mode)
			funcs->set_lp_mode(ctx, pmode);

		if (funcs->mode_set) {

			if (!curr_pmode)
				exynos_state->seamless_modeset |= SEAMLESS_MODESET_VREF;

			funcs->mode_set(ctx, pmode, exynos_state->seamless_modeset);
		}

		if (SEAMLESS_MODESET_VREF & exynos_state->seamless_modeset &&
				ctx->enabled)
			exynos_state->adjusted_fps = drm_mode_vrefresh(&pmode->mode);

		exynos_panel_update_rcd(ctx, pmode);
	}

	panel_info(ctx, "change the panel(%s) display mode (%s -> %s)\n",
		ctx->enabled ? "on" : "off", curr_pmode ?
		curr_pmode->mode.name : "none", pmode->mode.name);
	ctx->current_mode = pmode;

	panel_debug(ctx, "-\n");
}

void exynos_panel_active_off(struct exynos_panel *panel)
{
	struct drm_crtc *crtc = panel->bridge.encoder->crtc;
	struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(crtc);;

	if (exynos_crtc->ops->emergency_off)
		exynos_crtc->ops->emergency_off(exynos_crtc);
}

static const struct drm_bridge_funcs exynos_panel_bridge_funcs = {
	.attach = exynos_panel_bridge_attach,
	.detach = exynos_panel_bridge_detach,
	.pre_enable = exynos_panel_pre_enable,
	.enable = exynos_panel_enable,
	.disable = exynos_panel_disable,
	.post_disable = exynos_panel_post_disable,
	.mode_set = exynos_panel_bridge_mode_set,
};

static void exynos_panel_parse_vendor_params(struct device *dev, struct exynos_panel *ctx)
{
		struct device_node *np = dev->of_node;
		struct drm_device *drm_dev = NULL;
		struct drm_crtc *crtc;
		struct exynos_drm_crtc *exynos_crtc = NULL;
		struct decon_device *decon;
		const char *panel_name = NULL;
		struct mipi_dsi_device *dsi = to_mipi_dsi_device(dev);
		struct dsim_device *dsim =
		container_of(dsi->host, struct dsim_device, dsi_host);

		drm_dev = ctx->exynos_connector.base.dev;
		if (!drm_dev) {
			panel_info(ctx, "drm_dev has null\n");
			return;
		}

		drm_for_each_crtc(crtc, drm_dev)
			if (to_exynos_crtc(crtc)->possible_type & EXYNOS_DISPLAY_TYPE_DSI) {
				exynos_crtc = to_exynos_crtc(crtc);
				break;
			}

		if (!exynos_crtc) {
			panel_info(ctx, "exynos_crtc has null\n");
			return;
		}

		decon = exynos_crtc->ctx;
		if (!decon) {
			panel_info(ctx, "decon has null\n");
			return;
		}

		if (of_property_read_string(np, "connect-panel", &panel_name) == 0 ||
			of_property_read_string(np, "default-panel", &panel_name) == 0) {
			np = of_find_node_by_name(dev->of_node, panel_name);
			if (!np) {
				panel_err(ctx, "failed to find panel\n");
				return;
			}
		}

		decon->config.vendor_pps_en =
					of_property_read_bool(np, "vendor_pps_enable");
		of_property_read_u32(np, "initial_xmit_delay",
				&decon->config.vendor_pps.initial_xmit_delay);
		of_property_read_u32(np, "initial_dec_delay",
				&decon->config.vendor_pps.initial_dec_delay);
		of_property_read_u32(np, "scale_increment_interval",
				&decon->config.vendor_pps.scale_increment_interval);
		of_property_read_u32(np, "final_offset",
				&decon->config.vendor_pps.final_offset);
		of_property_read_u32(np, "comp_cfg",
				&decon->config.vendor_pps.comp_cfg);

		if (dsim != NULL)
			dsim->config.lp_force_en = of_property_read_bool(
				np, "samsung,force-seperate-trans");
}

static void exynos_panel_parse_vfp_detail(struct exynos_panel *ctx)
{
	struct device *dev = ctx->dev;
	struct device_node *np = dev->of_node;
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(dev);
	struct dsim_device *dsim =
		container_of(dsi->host, struct dsim_device, dsi_host);

	if (dsim != NULL) {
		if (of_property_read_u32(np, "lines-cmd-allow",
					 &dsim->config.line_cmd_allow))
			dsim->config.line_cmd_allow = 4;

		if (of_property_read_u32(np, "lines-stable-vfp",
					 &dsim->config.line_stable_vfp))
			dsim->config.line_stable_vfp = 2;
	} else {
		panel_err(ctx, "DSIM is not found\n");
	}
}

static void exynos_panel_set_dqe_xml(struct device *dev, struct exynos_panel *ctx)
{
	struct device_node *np = dev->of_node;
	struct drm_device *drm_dev = NULL;
	struct drm_crtc *crtc;
	struct exynos_drm_crtc *exynos_crtc = NULL;
	struct exynos_dqe *dqe;
	const char *xml_suffix = NULL;
	const char *panel_info = NULL;

	drm_dev = ctx->exynos_connector.base.dev;
	if (!drm_dev) {
		panel_info(ctx, "drm_dev has null\n");
		return;
	}

	drm_for_each_crtc(crtc, drm_dev)
		if (to_exynos_crtc(crtc)->possible_type & EXYNOS_DISPLAY_TYPE_DSI) {
			exynos_crtc = to_exynos_crtc(crtc);
			break;
		}

	if (!exynos_crtc) {
		panel_info(ctx, "exynos_crtc has null\n");
		return;
	}

	dqe = exynos_crtc->dqe;
	if (!dqe) {
		panel_info(ctx, "dqe has null\n");
		return;
	}


	if (of_property_read_string(np, "connect-panel", &panel_info) == 0 ||
		of_property_read_string(np, "default-panel", &panel_info) == 0) {
		np = of_find_node_by_name(dev->of_node, panel_info);
		if (np && of_property_read_string(np, "dqe-suffix", &xml_suffix) != 0)
			xml_suffix = panel_info;
	}

	if (xml_suffix != NULL)
		strlcpy(dqe->xml_suffix, xml_suffix, DQE_XML_SUFFIX_SIZE);

	panel_info(ctx, "DQE XML Suffix (%s)\n", dqe->xml_suffix);
}

#if defined(CONFIG_EXYNOS_DMA_DSIMFC)
#define FCMD_DATA_MAX_SIZE	0x00100000
struct dsim_fcmd *create_dsim_fast_cmd(void *vaddr, const u8 *payload, u32 size, u32 align)
{
	struct dsim_fcmd *fcmd;
	struct mipi_dsi_msg *msg;
	u32 padd_u = 0, padd_c, padd_t;
	int xfer_cnt, xfer_sz, xfer_unit, xfer_unit_aligned;
	u32 payload_xfer_unit;
	u32 remaineder = size;
	u32 size_aligned;
	u8 *tx_buf = (u8 *)vaddr;
	int i;

	if (!vaddr || !payload)
		return ERR_PTR(-EINVAL);

	if (size == 0 || size > FCMD_DATA_MAX_SIZE)
		return ERR_PTR(-EINVAL);

	fcmd = kzalloc(sizeof(*fcmd), GFP_KERNEL);
	if (!fcmd)
		return ERR_PTR(-ENOMEM);

	/* minimum alignment size is 1 */
	align = max(1U, align);
	size_aligned = roundup(size, align);

	payload_xfer_unit = rounddown(min((u32)(DSIM_PL_FIFO_THRESHOLD - 1), size_aligned), align);
	xfer_cnt = roundup(size_aligned, payload_xfer_unit) / payload_xfer_unit;
	xfer_unit = payload_xfer_unit + 1;
	xfer_unit_aligned = roundup(xfer_unit, DSIM_FCMD_ALIGN_CONSTRAINT);

	/* add address(e.g. 4C ,5C..) payload count */
	xfer_sz = size_aligned + xfer_cnt;

	/* padding is necessary if next xfer payload exists. so add (xfer_cnt - 1) times */
	if (xfer_unit % DSIM_FCMD_ALIGN_CONSTRAINT) {
		padd_u = DSIM_FCMD_ALIGN_CONSTRAINT - (xfer_unit % DSIM_FCMD_ALIGN_CONSTRAINT);
		padd_c = xfer_cnt - 1;
		padd_t = padd_u * padd_c;
		xfer_sz += padd_t;
	}

	for (i = 0; i < xfer_cnt; i++) {
		int copy_size = min(remaineder, payload_xfer_unit);

		tx_buf[i * xfer_unit_aligned] = (i == 0) ? 0x4C : 0x5C;
		memcpy(&tx_buf[i * xfer_unit_aligned + 1],
				&payload[i * payload_xfer_unit], copy_size);
		remaineder -= copy_size;
	}
	fcmd->xfer_unit = xfer_unit;
	msg = &fcmd->msg;
	msg->type = MIPI_DSI_DCS_LONG_WRITE;
	msg->tx_buf = (const u8 *)tx_buf;
	msg->tx_len = xfer_sz;

	return fcmd;
}

void destroy_dsim_fast_cmd(struct dsim_fcmd *fcmd)
{
	kfree(fcmd);
}

int drm_mipi_fcmd_write(void *_ctx, const u8 *payload, int size, u32 align)
{
	struct exynos_panel *ctx = (struct exynos_panel *)_ctx;
	struct mipi_dsi_device *dsi;
	struct dsim_device *dsim;
	struct dsim_fcmd *fcmd;
	int ret = 0;
	static DEFINE_MUTEX(lock);

	if (!ctx || !ctx->dev) {
		pr_err("%s: invalid ctx\n", __func__);
		return -EINVAL;
	}

	if (!payload) {
		panel_err(ctx, "invalid payloda\n");
		return -ENODATA;
	}

	dsi = to_mipi_dsi_device(ctx->dev);
	if (!dsi->host) {
		panel_err(ctx, "invalid dsi host\n");
		return -EINVAL;
	}

	dsim = container_of(dsi->host, struct dsim_device, dsi_host);
	if (!dsim->fcmd_buf_allocated) {
		panel_err(ctx, "fcmd_buf not allocated\n");
		return -ENOMEM;
	}

	mutex_lock(&lock);
	fcmd = create_dsim_fast_cmd(dsim->fcmd_buf_vaddr, payload, size, align);
	if (IS_ERR(fcmd)) {
		panel_err(ctx, "failed to create fast-command\n");
		ret = PTR_ERR(fcmd);
		goto out;
	}

	ret = dsim_host_fcmd_transfer(dsi->host, &fcmd->msg);
	if (ret < 0)
		panel_err(ctx, "failed to transfer fast-command\n");

out:
	mutex_unlock(&lock);
	destroy_dsim_fast_cmd(fcmd);

	return ret;
}
EXPORT_SYMBOL(drm_mipi_fcmd_write);
#endif

static ssize_t active_off_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t active_off_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t len)
{
	exynos_panel_active_off(base_ctx);
	return len;
}
static DEVICE_ATTR_RW(active_off);

static int get_cmd_count(u32 *data, int total, int min_cnt)
{
	int i, cnt, msg_cnt = 0;

	for (i = 0; i < total;) {
		cnt = data[i] + min_cnt;
		msg_cnt++;
		i += cnt;
	}

	return msg_cnt;
}

static int create_mipi_dsi_message(u32 *data, struct exynos_mipi_dsi_msg *emsg, int cnt)
{
	int i, j, len;
	u8 *payload;
	int offset = 0;
	struct mipi_dsi_msg *msg;

	for (i = 0; i < cnt; ++i) {
		len = data[offset++];
		msg = &emsg[i].msg;
		msg->tx_len = len;
		payload = kzalloc(sizeof(u8) * len, GFP_KERNEL);
		if (!payload) {
			pr_err("failed to create mipi msg\n");
			return -ENOMEM;
		}
		for (j = 0; j < len; ++j)
			payload[j] = (u8)(data[offset++] & 0xFF);
		msg->tx_buf = payload;
		emsg[i].wait_ms = data[offset++];
	}

	return 0;
}

static int of_parse_exynos_display_cmds(const struct device_node *np,
					struct exynos_display_cmd_set *clist)
{
	struct exynos_mipi_dsi_msg *emsg;
	int i;
	int ret = 0, cnt = 0, cmd_cnt = 0;
	int min_cnt = 2; /* num of cmd & delay */
	u32 *buf;

	buf = kzalloc(MAX_CMDSET_PAYLOAD * sizeof(u32), GFP_KERNEL);

	for (i = 0; i < NUM_OF_CMDS; ++i) {
		cnt = of_property_count_u32_elems(np, exynos_panel_cmd_string[i]);
		if (cnt <= min_cnt)
			continue; /* not definded */

		of_property_read_u32_array(np, exynos_panel_cmd_string[i], buf, cnt);
		cmd_cnt = get_cmd_count(buf, cnt, min_cnt);
		if (cmd_cnt < 1)
			continue; /* invalid format */

		clist[i].cmd_cnt = cmd_cnt;
		emsg = kzalloc(sizeof(struct exynos_mipi_dsi_msg) * clist[i].cmd_cnt,
				GFP_KERNEL);
		if (!emsg) {
			pr_err("failed to create mipi emsg\n");
			//return -EINVAL;
		}
		clist[i].emsg = emsg;

		create_mipi_dsi_message(buf, clist[i].emsg, clist[i].cmd_cnt);
	}

	kfree(buf);
	return ret;
}

static int of_parse_exynos_rcd(const struct device_node *np,
			       struct exynos_display_rcd_desc *rcd_desc)
{
	int cnt;
	int i;
	struct exynos_rcd_win *rcd_win;
	int win_cnt = 0;
	u8 *buf;

	if (!IS_ENABLED(CONFIG_EXYNOS_DMA_RCD))
		return 0;

	for (i = 0; i < MAX_RCD_WIN; i++) {
		cnt = of_property_count_u32_elems(np, exynos_rcd_pos_string[i]);
		if (cnt != POS_DESC_SIZE) {
			pr_info("%pOF: %s is not defined in DT\n", np,
					exynos_rcd_pos_string[i]);
			continue;
		}

		rcd_win = kzalloc(sizeof(*rcd_win), GFP_KERNEL);
		if (of_property_read_u32_array(np, exynos_rcd_pos_string[i],
					(u32 *)rcd_win->position, POS_DESC_SIZE)) {
			pr_err("%pOF: %s is invalid\n", np, exynos_rcd_pos_string[i]);
			kfree(rcd_win);
			continue;
		}

		cnt = of_property_count_u8_elems(np, exynos_rcd_img_string[i]);
		if (cnt < 1) {
			kfree(rcd_win);
			continue;
		}

		pr_debug("%s, cnt:%d\n", __func__, cnt);

		buf = kzalloc(sizeof(u8) * cnt, GFP_KERNEL);
		if (!buf) {
			pr_err("%pOF: could not allocate rcd buffer(%d)\n", np, i);
			kfree(rcd_win);
			continue;
		}

		if (of_property_read_u8_array(np, exynos_rcd_img_string[i], buf, cnt)) {
			pr_err("%pOF: failed to parse rcd img(%d)\n", np, i);
			kfree(rcd_win);
			kfree(buf);
			continue;
		}

		pr_debug("%s, win:%d(x:%d,y:%d)\n", __func__, win_cnt,
				rcd_win->position[0], rcd_win->position[1]);

		rcd_win->img_size = cnt;
		rcd_win->img_buf = buf;
		win_cnt++;
		rcd_desc->win_cnt = win_cnt;
		rcd_desc->rcd_wins[i] = rcd_win;
	}

	if (!of_property_read_u32_array(np, "exynos,rcd-block-rect",
				(u32 *)rcd_desc->block_rect, RECT_DESC_SIZE))
		rcd_desc->block_en = true;

	pr_debug("%s, block en:%d, pos:(%d %d %d %d)\n", __func__, rcd_desc->block_en,
			rcd_desc->block_rect[0], rcd_desc->block_rect[1],
			rcd_desc->block_rect[2], rcd_desc->block_rect[3]);

	return 0;
}

static int of_parse_exynos_videomode(const struct device_node *np,
				     struct exynos_videomode *vm)
{
	const char *node_name;
	u32 val = 0;
	int ret = 0, len;

	memset(vm, 0, sizeof(*vm));

	node_name = kbasename(np->full_name);
	len = strlen(node_name);
	strlcpy(vm->mode, node_name, len + 1);

	ret |= of_property_read_u32(np, "exynos,mode-type", &vm->type);
	ret |= of_property_read_u32(np, "exynos,refresh-rate", &vm->fps);
	ret |= of_property_read_u32(np, "exynos,hback-porch", &vm->hback_porch);
	ret |= of_property_read_u32(np, "exynos,hfront-porch", &vm->hfront_porch);
	ret |= of_property_read_u32(np, "exynos,hactive", &vm->hactive);
	ret |= of_property_read_u32(np, "exynos,hsync-len", &vm->hsync_len);
	ret |= of_property_read_u32(np, "exynos,vback-porch", &vm->vback_porch);
	ret |= of_property_read_u32(np, "exynos,vfront-porch", &vm->vfront_porch);
	ret |= of_property_read_u32(np, "exynos,vactive", &vm->vactive);
	ret |= of_property_read_u32(np, "exynos,vsync-len", &vm->vsync_len);

	of_property_read_u32(np, "exynos,clock-frequency", &vm->pixelclock);

	vm->flags = 0;
	if (!of_property_read_u32(np, "exynos,vsync-active", &val))
		vm->flags |= val ? DISPLAY_FLAGS_VSYNC_HIGH :
				DISPLAY_FLAGS_VSYNC_LOW;
	if (!of_property_read_u32(np, "exynos,hsync-active", &val))
		vm->flags |= val ? DISPLAY_FLAGS_HSYNC_HIGH :
				DISPLAY_FLAGS_HSYNC_LOW;
	if (!of_property_read_u32(np, "exynos,de-active", &val))
		vm->flags |= val ? DISPLAY_FLAGS_DE_HIGH :
				DISPLAY_FLAGS_DE_LOW;
	if (!of_property_read_u32(np, "exynos,pixelclk-active", &val))
		vm->flags |= val ? DISPLAY_FLAGS_PIXDATA_POSEDGE :
				DISPLAY_FLAGS_PIXDATA_NEGEDGE;

	if (!of_property_read_u32(np, "exynos,syncclk-active", &val))
		vm->flags |= val ? DISPLAY_FLAGS_SYNC_POSEDGE :
				DISPLAY_FLAGS_SYNC_NEGEDGE;
	else if (vm->flags & (DISPLAY_FLAGS_PIXDATA_POSEDGE |
			      DISPLAY_FLAGS_PIXDATA_NEGEDGE))
		vm->flags |= vm->flags & DISPLAY_FLAGS_PIXDATA_POSEDGE ?
				DISPLAY_FLAGS_SYNC_POSEDGE :
				DISPLAY_FLAGS_SYNC_NEGEDGE;

	if (of_property_read_bool(np, "exynos,interlaced"))
		vm->flags |= DISPLAY_FLAGS_INTERLACED;
	if (of_property_read_bool(np, "exynos,doublescan"))
		vm->flags |= DISPLAY_FLAGS_DOUBLESCAN;
	if (of_property_read_bool(np, "exynos,doubleclk"))
		vm->flags |= DISPLAY_FLAGS_DOUBLECLK;

	of_property_read_u32(np, "exynos,bts_fps", &vm->bts_fps);

	ret |= of_property_read_u32(np, "exynos,mode-flags", &vm->mode_flags);
	ret |= of_property_read_u32(np, "exynos,bpc", &vm->bpc);
	ret |= of_property_read_u32(np, "exynos,lp-mode", &vm->is_lp_mode);
	ret |= of_property_read_u32(np, "exynos,dsc-en", &vm->dsc_enabled);
	if (vm->dsc_enabled) {
		ret |= of_property_read_u32(np, "exynos,dsc-count",
				&vm->dsc_count);
		ret |= of_property_read_u32(np, "exynos,dsc-slice-count",
				&vm->dsc_slice_count);
		ret |= of_property_read_u32(np, "exynos,dsc-slice-height",
				&vm->dsc_slice_height);
	}

	if (ret) {
		pr_err("%pOF: error reading timing properties\n", np);
		return -EINVAL;
	}

	return ret;
}

static void drm_display_mode_from_exynos_videomode(const struct exynos_videomode *vm,
				     struct drm_display_mode *dmode)
{
	dmode->type = vm->type;

	dmode->hdisplay = vm->hactive;
	dmode->hsync_start = dmode->hdisplay + vm->hfront_porch;
	dmode->hsync_end = dmode->hsync_start + vm->hsync_len;
	dmode->htotal = dmode->hsync_end + vm->hback_porch;

	dmode->vdisplay = vm->vactive;
	dmode->vsync_start = dmode->vdisplay + vm->vfront_porch;
	dmode->vsync_end = dmode->vsync_start + vm->vsync_len;
	dmode->vtotal = dmode->vsync_end + vm->vback_porch;

	if (vm->pixelclock)
		dmode->clock = vm->pixelclock;
	else
		dmode->clock = (dmode->htotal * dmode->vtotal * vm->fps) / 1000;

	dmode->flags = 0;
	if (vm->flags & DISPLAY_FLAGS_HSYNC_HIGH)
		dmode->flags |= DRM_MODE_FLAG_PHSYNC;
	else if (vm->flags & DISPLAY_FLAGS_HSYNC_LOW)
		dmode->flags |= DRM_MODE_FLAG_NHSYNC;
	if (vm->flags & DISPLAY_FLAGS_VSYNC_HIGH)
		dmode->flags |= DRM_MODE_FLAG_PVSYNC;
	else if (vm->flags & DISPLAY_FLAGS_VSYNC_LOW)
		dmode->flags |= DRM_MODE_FLAG_NVSYNC;
	if (vm->flags & DISPLAY_FLAGS_INTERLACED)
		dmode->flags |= DRM_MODE_FLAG_INTERLACE;
	if (vm->flags & DISPLAY_FLAGS_DOUBLESCAN)
		dmode->flags |= DRM_MODE_FLAG_DBLSCAN;
	if (vm->flags & DISPLAY_FLAGS_DOUBLECLK)
		dmode->flags |= DRM_MODE_FLAG_DBLCLK;
	strlcpy(dmode->name, vm->mode, strlen(vm->mode) + 1);
}

static void exynos_display_mode_from_exynos_videomode(const struct exynos_videomode *vm,
				     struct exynos_display_mode *emode)
{
	emode->bts_fps = vm->bts_fps;
	emode->en_bts_fps = (!!vm->bts_fps ? true : false);
	emode->mode_flags = BIT(vm->mode_flags);
	emode->bpc = vm->bpc;
	emode->is_lp_mode = (!!vm->is_lp_mode ? true : false);
	emode->dsc.enabled = (!!vm->dsc_enabled ? true : false);
	emode->dsc.dsc_count = vm->dsc_count;
	emode->dsc.slice_count = vm->dsc_slice_count;
	emode->dsc.slice_height = vm->dsc_slice_height;
}

static int exynos_display_mode_set_pps_info(struct exynos_display_mode *emode,
					    struct exynos_display_cmd_set *clist)
{
	struct exynos_display_cmd_set *pps_cset;
	const u8 *data;

	pps_cset = &clist[PANEL_PPS_CMDS];

	if (!pps_cset->emsg || pps_cset->cmd_cnt == 0)
		return -EINVAL;

	data = (const u8*)pps_cset->emsg->msg.tx_buf;
	convert_pps_to_exynos_pps(&emode->pps, data);
	exynos_drm_dsc_print_pps_info(&emode->pps, data);

	return 0;
}

/**
 * of_parse_display_timing - parse display_timing entry from device_node
 * @np: device_node with the properties
 **/
static int of_parse_exynos_display_timing(const struct device_node *np,
					  struct exynos_panel_mode *pmode)
{
	struct exynos_videomode vm;
	struct drm_display_mode *dmode = &pmode->mode;
	struct exynos_display_mode *emode = &pmode->exynos_mode;
	struct exynos_display_cmd_set *clist = pmode->cmd_list;
	int ret = 0;

	ret = of_parse_exynos_videomode(np, &vm);
	if (ret) {
		pr_err("%pOF: error in timing\n", np);
		return ret;
	}

	drm_display_mode_from_exynos_videomode(&vm, dmode);
	exynos_display_mode_from_exynos_videomode(&vm, emode);
	drm_mode_debug_printmodeline(dmode);

	of_parse_exynos_rcd(np, &pmode->rcd_desc);
	of_parse_exynos_display_cmds(np, clist);

	if (emode->dsc.enabled)
		exynos_display_mode_set_pps_info(emode, clist);

	return ret;
}

/**
 * of_get_exynos_display_info - parse all display_timing entries and cmds entries
 *                                 from a device_node
 * @np: device_node with the subnodes
 * @ctx: exynos panel context
 **/
static int of_get_exynos_display_info(const struct device_node *np,
				      struct exynos_panel *ctx)
{
	struct exynos_panel_desc *desc = ctx->desc;
	struct exynos_panel_mode *pmode;
	struct device_node *comm_np, *timings_np = NULL, *mode_np = NULL;
	char mode_name[DRM_DISPLAY_MODE_LEN];
	char *resol_name, *fps_name;
	int ret = 0;

	timings_np = of_get_child_by_name(np, "display-timings");
	if (!timings_np) {
		panel_err(ctx, "%pOF: could not find display-timings node\n", np);
		ret = -ENOENT;
		goto err;
	}
	desc->num_modes = of_get_child_count(timings_np);
	if (desc->num_modes == 0) {
		panel_err(ctx, "%pOF: no timings specified\n", np);
		ret = -ENOENT;
		goto err;
	}

	desc->modes = devm_kcalloc(ctx->dev, desc->num_modes,
			sizeof(struct exynos_panel_mode), GFP_KERNEL);
	if (!desc->modes) {
		panel_err(ctx, "%pOF: could not allocate timings array\n", np);
		ret = -ENOMEM;
		goto err;
	}

	pmode = (struct exynos_panel_mode *)desc->modes;
	for_each_child_of_node(timings_np, mode_np) {
		comm_np = of_get_child_by_name(np, "common");
		exynos_duplicate_display_props(ctx->dev, mode_np, comm_np);
		of_node_put(comm_np);

		strlcpy(mode_name, mode_np->full_name, strlen(mode_np->full_name) + 1);
		resol_name = mode_name;
		fps_name = strsep(&resol_name, "@");
		comm_np = of_get_child_by_name(np, resol_name);
		exynos_duplicate_display_props(ctx->dev, mode_np, comm_np);
		of_node_put(comm_np);

		comm_np = of_get_child_by_name(np, fps_name);
		exynos_duplicate_display_props(ctx->dev, mode_np, comm_np);
		of_node_put(comm_np);

		ret = of_parse_exynos_display_timing(mode_np, pmode);
		if (ret)
			goto err;

		if (pmode->mode.type & DRM_MODE_TYPE_PREFERRED)
			desc->default_mode = pmode;
		++pmode;
	}

	if (desc->default_mode)
		panel_info(ctx, "default_mode :%s\n", desc->default_mode->mode.name);

err:
	of_node_put(mode_np);
	of_node_put(timings_np);

	return 0;
}

static int exynos_panel_parse_timings(struct exynos_panel *ctx)
{
	struct device *dev = ctx->dev;
	struct device_node *np;
	const char *panel_info = NULL;
	struct exynos_panel_desc *pdesc;
	char info_str[128];
	u32 data[5];
	int ret;

	ret = of_property_read_string(ctx->dev->of_node, "connect-panel", &panel_info);
	if (!ret) {
		panel_err(ctx, "panel detected\n");
		strncpy(&info_str[0], panel_info, 128);
	} else {
		ret = of_property_read_string(ctx->dev->of_node, "default-panel", &panel_info);
		if (!ret) {
			panel_err(ctx, "bootup with default panel\n");
			strncpy(&info_str[0], panel_info, 128);
		} else {
			panel_err(ctx, "failed to find panel string\n");
			return -EINVAL;
		}
	}

	np = of_find_node_by_name(dev->of_node, info_str);
	if (!np) {
		panel_err(ctx, "failed to find select_panel\n");
		return -EINVAL;
	}

	memset(&info_str, 0, sizeof(char) * 128);
	ret = of_property_read_string(np, "samsung,panel-name", &panel_info);
	if (!ret) {
		strncpy(&info_str[0], panel_info, 128);
		panel_info(ctx, "selected panel info [%s]\n", info_str);
	}

	ctx->desc = devm_kzalloc(dev, sizeof(struct exynos_panel_desc), GFP_KERNEL);
	if (!ctx->desc) {
		panel_err(ctx, "failed to get memory for desc\n");
		return -ENOMEM;
	}

	pdesc = ctx->desc;

	ret = of_property_read_u32(np, "samsung,data-lane-count", &data[0]);
	if (ret == -EINVAL)
		pdesc->data_lane_cnt = 4;
	else
		pdesc->data_lane_cnt = data[0];

	ret = of_property_read_u32(np, "samsung,max-brightness", &data[0]);
	if (ret == -EINVAL)
		pdesc->max_brightness = 1023;
	else
		pdesc->max_brightness = data[0];

	ret = of_property_read_u32(np, "samsung,default-brightness", &data[0]);
	if (ret == -EINVAL)
		pdesc->dft_brightness = 512;
	else
		pdesc->dft_brightness = data[0];

	pdesc->bl_endian_swap =
		of_property_read_bool(np, "samsung,bl-endian-swap");

	ret = of_property_read_u32(np, "samsung,hdr-formats", &data[0]);
	if (ret == -EINVAL)
		pdesc->hdr_formats = 0;
	else
		pdesc->hdr_formats = data[0];

	ret = of_property_read_u32(np, "samsung,max-luminance", &data[0]);
	if (ret == -EINVAL)
		pdesc->max_luminance = 0;
	else
		pdesc->max_luminance = data[0];

	ret = of_property_read_u32(np, "samsung,max-avg-luminance", &data[0]);
	if (ret == -EINVAL)
		pdesc->max_avg_luminance = 0;
	else
		pdesc->max_avg_luminance = data[0];

	ret = of_property_read_u32(np, "samsung,min-luminance", &data[0]);
	if (ret == -EINVAL)
		pdesc->min_luminance = 0;
	else
		pdesc->min_luminance = data[0];

	pdesc->doze_supported = of_property_read_bool(np, "samsung,doze-supported");
	panel_info(ctx, "doze-supported:%s\n", pdesc->doze_supported ?
			"true" : "false");

	ret = of_property_read_u32(np, "samsung,reset-position", &data[0]);
	if (ret == -EINVAL)
		pdesc->reset_pos = PANEL_RESET_LP11_HS;
	else
		pdesc->reset_pos = data[0];

	if (pdesc->reset_pos == PANEL_RESET_LP11)
		notify_lp11_reset(ctx, true);

	ret = of_property_read_u32_array(np, "samsung,reset-requence", data, 3);
	if (ret == -EINVAL) {
		pdesc->reset_seq[0] = 1;
		pdesc->reset_seq[1] = 0;
		pdesc->reset_seq[2] = 1;
	} else {
		pdesc->reset_seq[0] = data[0];
		pdesc->reset_seq[1] = data[1];
		pdesc->reset_seq[2] = data[2];
	}

	ret = of_property_read_u32_array(np, "samsung,reset-udelay", data, 4);
	if (ret == -EINVAL) {
		pdesc->delay_before_reset = 0;
		pdesc->reset_delay[0] = 5000;
		pdesc->reset_delay[1] = 5000;
		pdesc->reset_delay[2] = 10000;
	} else {
		pdesc->delay_before_reset = data[0];
		pdesc->reset_delay[0] = data[1];
		pdesc->reset_delay[1] = data[2];
		pdesc->reset_delay[2] = data[3];
	}

	ret = of_property_read_u32_array(np, "samsung,lcd-size-mm", data, 2);
	if (ret == -EINVAL) {
		pdesc->width_mm = 75;
		pdesc->height_mm = 165;
	} else {
		pdesc->width_mm = data[0];
		pdesc->height_mm = data[1];
	}

	if (of_property_read_bool(np, "samsung,wait-lp11-after-sending-cmds"))
		notify_lp11_wait(ctx, true);

	ret = of_get_exynos_display_info(np, ctx);
	if (ret) {
		panel_err(ctx, "%pOF: no timings specified\n", np);
		return -EINVAL;
	}

	return 0;
}

int exynos_panel_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct exynos_panel *ctx;
	int ret = 0;
	char name[32];

	ctx = devm_kzalloc(dev, sizeof(struct exynos_panel), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	pr_info("%s: %s+\n", dev->driver->name, __func__);

	mipi_dsi_set_drvdata(dsi, ctx);
	ctx->dev = dev;
	ctx->func_set = of_device_get_match_data(dev);

	ret = exynos_panel_parse_dt(ctx);
	if (ret)
		return ret;

	dsi->lanes = ctx->desc->data_lane_cnt;
	dsi->format = MIPI_DSI_FMT_RGB888;

	snprintf(name, sizeof(name), "panel%d-backlight", dsi->channel);
	ctx->bl = devm_backlight_device_register(ctx->dev, name, NULL,
			ctx, &exynos_backlight_ops, NULL);
	if (IS_ERR(ctx->bl)) {
		panel_err(ctx, "failed to register backlight device\n");
		return PTR_ERR(ctx->bl);
	}
	ctx->bl->props.max_brightness = ctx->desc->max_brightness;
	ctx->bl->props.brightness = ctx->desc->dft_brightness;

	drm_panel_init(&ctx->panel, dev, ctx->func_set->panel_func, DRM_MODE_CONNECTOR_DSI);

	drm_panel_add(&ctx->panel);

	ctx->bridge.funcs = &exynos_panel_bridge_funcs;

#ifdef CONFIG_OF
	ctx->bridge.of_node = ctx->dev->of_node;
#endif

	drm_bridge_add(&ctx->bridge);

	ret = mipi_dsi_attach(dsi);
	if (ret)
		goto err_panel;

	exynos_panel_set_dqe_xml(dev, ctx);

	exynos_panel_parse_vendor_params(dev, ctx);

	exynos_panel_parse_vfp_detail(ctx);

	exynos_panel_update_rcd(ctx, ctx->desc->default_mode);

	ret = device_create_file(dev, &dev_attr_active_off);
	if (ret < 0)
		panel_err(ctx, "failed to add sysfs entries\n");

	base_ctx = ctx;

	panel_info(ctx, "samsung common panel driver has been probed\n");

	return 0;

err_panel:
	drm_panel_remove(&ctx->panel);
	panel_err(ctx, "failed to probe samsung panel driver(%d)\n", ret);

	return ret;
}
EXPORT_SYMBOL(exynos_panel_probe);

int exynos_panel_remove(struct mipi_dsi_device *dsi)
{
	struct exynos_panel *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);
	drm_bridge_remove(&ctx->bridge);
	devm_backlight_device_unregister(ctx->dev, ctx->bl);
	device_remove_file(&dsi->dev, &dev_attr_active_off);

	return 0;
}
EXPORT_SYMBOL(exynos_panel_remove);

MODULE_AUTHOR("Jiun Yu <jiun.yu@samsung.com>");
MODULE_DESCRIPTION("MIPI-DSI based Samsung common panel driver");
MODULE_LICENSE("GPL");
