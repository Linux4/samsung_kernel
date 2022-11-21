/* drivers/video/exynos/decon/panels/common_mipi_lcd.c
 *
 * Samsung SoC MIPI LCD driver.
 *
 * Copyright (c) 2017 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/mutex.h>
#include <video/mipi_display.h>
#include <linux/platform_device.h>

#include "../disp_err.h"
#include "../decon.h"
#include "../dsim.h"
#include "../../panel/panel.h"
#include "../../panel/panel_drv.h"
#include "exynos_panel_drv.h"
#include "exynos_panel.h"
#include "exynos_panel_modes.h"

static DEFINE_MUTEX(cmd_lock);
struct panel_state *panel_state;
struct panel_mres *mres;

static int panel_drv_ioctl(struct exynos_panel_device *panel, u32 cmd, void *arg)
{
	int ret;

	if (unlikely(!panel || !panel->panel_drv_sd))
		return -EINVAL;

	ret = v4l2_subdev_call(panel->panel_drv_sd, core, ioctl, cmd, arg);

	return ret;
}

static int panel_drv_notify(struct v4l2_subdev *sd,
		unsigned int notification, void *arg)
{
	struct v4l2_event *ev = (struct v4l2_event *)arg;
	int ret;

	if (notification != V4L2_DEVICE_NOTIFY_EVENT) {
		panel_dbg("unknown event\n");
		return -EINVAL;
	}

	switch (ev->type) {
	case V4L2_EVENT_DECON_FRAME_DONE:
		ret = v4l2_subdev_call(sd, core, ioctl,
				PANEL_IOC_EVT_FRAME_DONE, &ev->timestamp);
		if (ret) {
			panel_err("failed to notify FRAME_DONE\n");
			return ret;
		}
		break;
	case V4L2_EVENT_DECON_VSYNC:
		ret = v4l2_subdev_call(sd, core, ioctl,
				PANEL_IOC_EVT_VSYNC, &ev->timestamp);
		if (ret) {
			panel_err("failed to notify VSYNC\n");
			return ret;
		}
		break;
	default:
		panel_warn("unknown event type %d\n", ev->type);
		break;
	}

	panel_dbg("event type %d timestamp %ld %ld nsec\n",
			ev->type, ev->timestamp.tv_sec,
			ev->timestamp.tv_nsec);

	return 0;
}

static int common_panel_set_error_cb(struct exynos_panel_device *panel, void *arg)
{
	int ret;

	v4l2_set_subdev_hostdata(panel->panel_drv_sd, arg);
	ret = panel_drv_ioctl(panel, PANEL_IOC_REG_RESET_CB, NULL);
	if (ret) {
		panel_err("failed to set panel error callback\n");
		return ret;
	}

	return 0;
}

static int panel_drv_probe(struct exynos_panel_device *panel)
{
	int ret;

	ret = panel_drv_ioctl(panel, PANEL_IOC_DSIM_PROBE, (void *)&panel->id);
	if (ret) {
		panel_err("failed to panel dsim probe\n");
		return ret;
	}

	return ret;
}

static int panel_drv_get_state(struct exynos_panel_device *panel)
{
	int ret = 0;

	ret = panel_drv_ioctl(panel, PANEL_IOC_GET_PANEL_STATE, NULL);
	if (ret) {
		panel_err("failed to get panel state");
		return ret;
	}

	panel_state = (struct panel_state *)
		v4l2_get_subdev_hostdata(panel->panel_drv_sd);
	if (IS_ERR_OR_NULL(panel_state)) {
		panel_err("failed to get lcd information\n");
		return -EINVAL;
	}

	return ret;
}

#if defined(CONFIG_PANEL_DISPLAY_MODE)
static int panel_drv_get_panel_display_mode(struct exynos_panel_device *panel)
{
	struct exynos_panel_info *info;
	struct panel_display_modes *panel_modes;
	struct exynos_display_modes *exynos_modes;
	int ret;

	if (!panel)
		return -EINVAL;

	info = &panel->lcd_info;
	ret = panel_drv_ioctl(panel, PANEL_IOC_GET_DISPLAY_MODE, NULL);
	if (ret < 0) {
		panel_err("failed to ioctl(PANEL_IOC_GET_DISPLAY_MODE)\n");
		return ret;
	}

	panel_modes = (struct panel_display_modes *)
		v4l2_get_subdev_hostdata(panel->panel_drv_sd);
	if (IS_ERR_OR_NULL(panel_modes)) {
		panel_err("failed to get panel_display_modes using v4l2_subdev\n");
		return -EINVAL;
	}

	/* create unique exynos_display_mode array using panel_display_modes */
	exynos_modes =
		exynos_display_modes_create_from_panel_display_modes(panel, panel_modes);
	if (!exynos_modes) {
		panel_err("could not create exynos_display_modes\n");
		return -ENOMEM;
	}

	/*
	 * initialize display mode information of exynos_panel_info
	 * using exynos_display_modes.
	 */
	exynos_display_modes_update_panel_info(panel, exynos_modes);
	info->panel_modes = panel_modes;

	return ret;
}
#endif

static int panel_drv_get_mres(struct exynos_panel_device *panel)
{
	int ret = 0;

	ret = panel_drv_ioctl(panel, PANEL_IOC_GET_MRES, NULL);
	if (ret) {
		panel_err("failed to get panel mres");
		return ret;
	}

	mres = (struct panel_mres *)
		v4l2_get_subdev_hostdata(panel->panel_drv_sd);
	if (IS_ERR_OR_NULL(mres)) {
		panel_err("failed to get lcd information\n");
		return -EINVAL;
	}

	return ret;
}

static int common_panel_vrr_changed(struct exynos_panel_device *panel, void *arg)
{
	struct exynos_panel_info *lcd_info;
	struct vrr_config_data *vrr_config;
	struct decon_device *decon = get_decon_drvdata(0);

	if (!panel || !arg || !decon)
		return -EINVAL;

	lcd_info = &panel->lcd_info;
	vrr_config = arg;

	/*
	 * decon->lcd_info->fps : panel's current fps setting
	 * decon->lcd_info->req_vrr_fps : decon requested fps
	 * decon->bts.next_fps : next_fps will be applied after 1-VSYNC and FrameStart
	 * decon->bts.next_fps_vsync_count : timeline of next_fps will be applied.
	 */
#if defined(CONFIG_DECON_BTS_VRR_ASYNC)
	if (lcd_info->fps == vrr_config->fps &&
			vrr_config->fps < decon->bts.next_fps) {
		decon->bts.next_fps = lcd_info->fps;
		decon->bts.next_fps_vsync_count = decon->vsync.count + 1;
		DPU_DEBUG_BTS("\tupdate next_fps(%d) next_fps_vsync_count(%llu)\n",
				decon->bts.next_fps, decon->bts.next_fps_vsync_count);
	}
#endif

	if (lcd_info->fps != vrr_config->fps ||
		lcd_info->vrr_mode != vrr_config->mode)
		panel_warn("[VRR] decon(%d%s) panel(%d%s) mismatch\n",
				lcd_info->fps, EXYNOS_VRR_MODE_STR(lcd_info->vrr_mode),
				vrr_config->fps, EXYNOS_VRR_MODE_STR(vrr_config->mode));
	else
		panel_warn("[VRR] panel(%d%s) updated\n",
				vrr_config->fps, EXYNOS_VRR_MODE_STR(vrr_config->mode));

	return 0;
}

static int panel_drv_set_vrr_cb(struct exynos_panel_device *panel)
{
	int ret;
	struct disp_cb_info vrr_cb_info = {
		.cb = (disp_cb *)common_panel_vrr_changed,
		.data = panel,
	};

	v4l2_set_subdev_hostdata(panel->panel_drv_sd, &vrr_cb_info);
	ret = panel_drv_ioctl(panel, PANEL_IOC_REG_VRR_CB, NULL);
	if (ret < 0) {
		panel_err("failed to set panel error callback\n");
		return ret;
	}

	return 0;
}

#define DSIM_TX_FLOW_CONTROL
static void print_tx(u8 cmd_id, const u8 *cmd, int size)
{
	char data[128];
	int i, len;
	bool newline = false;

	/*
	len = snprintf(data, ARRAY_SIZE(data), "(%02X) ", cmd_id);
	for (i = 0; i < min((int)size, 128); i++) {
		len += snprintf(data + len, ARRAY_SIZE(data) - len,
				"%02X ", cmd[i]);
		panel_info("%s\n", data);
	}
	*/

	len = snprintf(data, ARRAY_SIZE(data), "(%02X) ", cmd_id);
	for (i = 0; i < min((int)size, 128); i++) {
		if (newline)
			len += snprintf(data + len, ARRAY_SIZE(data) - len, "     ");
		newline = (!((i + 1) % 16) || (i + 1 == size)) ? true : false;
		len += snprintf(data + len, ARRAY_SIZE(data) - len,
				"%02X%s", cmd[i], newline ? "\n" : " ");
		if (newline) {
			panel_info("%s", data);
			len = 0;
		}
	}
}

static void print_rx(u8 addr, u8 *buf, int size)
{
	char data[128];
	int i, len;

	len = snprintf(data, ARRAY_SIZE(data), "(%02X) ", addr);
	for (i = 0; i < min((int)size, 32); i++)
		len += snprintf(data + len, ARRAY_SIZE(data) - len,
				"%02X ", buf[i]);
	panel_info("%s\n", data);
}

static void print_dsim_cmd(const struct exynos_dsim_cmd *cmd_set, int size)
{
	int i;

	for (i = 0; i < size; i++)
		print_tx(cmd_set[i].type,
				cmd_set[i].data_buf,
				cmd_set[i].data_len);
}

static int mipi_write(u32 id, u8 cmd_id, const u8 *cmd, u32 offset, int size, u32 option)
{
	int ret, retry = 3;
	unsigned long d0;
	u32 type, d1;
	bool block = (option & DSIM_OPTION_WAIT_TX_DONE);
	struct dsim_device *dsim = get_dsim_drvdata(id);

	if (!cmd) {
		panel_err("cmd is null\n");
		return -EINVAL;
	}

	if (cmd_id == MIPI_DSI_WR_DSC_CMD) {
		type = MIPI_DSI_DSC_PRA;
		d0 = (unsigned long)cmd[0];
		d1 = 0;
	} else if (cmd_id == MIPI_DSI_WR_PPS_CMD) {
		type = MIPI_DSI_DSC_PPS;
		d0 = (unsigned long)cmd;
		d1 = size;
	} else if (cmd_id == MIPI_DSI_WR_GEN_CMD) {
		if (size == 1) {
			type = MIPI_DSI_DCS_SHORT_WRITE;
			d0 = (unsigned long)cmd[0];
			d1 = 0;
		} else {
			type = MIPI_DSI_DCS_LONG_WRITE;
			d0 = (unsigned long)cmd;
			d1 = size;
		}
	} else {
		panel_info("invalid cmd_id %d\n", cmd_id);
		return -EINVAL;
	}

	mutex_lock(&cmd_lock);
	while (--retry >= 0) {
		if (offset > 0) {
			int gpara_len = 1;
			u8 gpara[4] = { 0xB0, 0x00 };

			/* gpara 16bit offset */
			if (option & DSIM_OPTION_2BYTE_GPARA)
				gpara[gpara_len++] = (offset >> 8) & 0xFF;

			gpara[gpara_len++] = offset & 0xFF;

			/* pointing gpara */
			if (option & DSIM_OPTION_POINT_GPARA)
				gpara[gpara_len++] = cmd[0];

			if (panel_cmd_log_enabled(PANEL_CMD_LOG_DSI_TX))
				print_tx(MIPI_DSI_DCS_LONG_WRITE, gpara, gpara_len);
			if (dsim_write_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
						(unsigned long)gpara, gpara_len, false)) {
				panel_err("failed to write gpara %d (retry %d)\n",
						offset, retry);
				continue;
			}
		}
		if (panel_cmd_log_enabled(PANEL_CMD_LOG_DSI_TX))
			print_tx(type, cmd, size);
		if (dsim_write_data(dsim, type, d0, d1, block)) {
			panel_err("failed to write cmd %02X size %d(retry %d)\n",
					cmd[0], size, retry);
			continue;
		}

		break;
	}

	if (retry < 0) {
		panel_err("failed: exceed retry count (cmd %02X)\n",
				cmd[0]);
		ret = -EIO;
		goto error;
	}

	panel_dbg("cmd_id %d, addr %02X offset %d size %d\n",
			cmd_id, cmd[0], offset, size);
	ret = size;

error:
	mutex_unlock(&cmd_lock);
	return ret;
}

#define MAX_DSIM_PH_SIZE (32)
#define MAX_DSIM_PL_SIZE (DSIM_PL_FIFO_THRESHOLD)
#define MAX_CMD_SET_SIZE (1024)
static struct exynos_dsim_cmd cmd_set[MAX_CMD_SET_SIZE];
static int mipi_write_table(u32 id, const struct cmd_set *cmd, int size, u32 option)
{
	int ret, total_size = 0;
	struct dsim_device *dsim = get_dsim_drvdata(id);
	int i, from = 0, sz_pl = 0;
	s64 elapsed_usec;
	struct timespec cur_ts, last_ts, delta_ts;

	if (!cmd) {
		panel_err("cmd is null\n");
		return -EINVAL;
	}

	if (size <= 0) {
		panel_err("invalid cmd size %d\n", size);
		return -EINVAL;
	}

	if (size > MAX_CMD_SET_SIZE) {
		panel_err("exceeded MAX_CMD_SET_SIZE(%d) %d\n",
				MAX_CMD_SET_SIZE, size);
		return -EINVAL;
	}

	ktime_get_ts(&last_ts);
	mutex_lock(&cmd_lock);
	for (i = 0; i < size; i++) {
		if (cmd[i].buf == NULL) {
			panel_err("cmd[%d].buf is null\n", i);
			continue;
		}

		if (cmd[i].cmd_id == MIPI_DSI_WR_DSC_CMD) {
			cmd_set[i].type = MIPI_DSI_DSC_PRA;
			cmd_set[i].data_buf = cmd[i].buf;
			cmd_set[i].data_len = 1;
		} else if (cmd[i].cmd_id == MIPI_DSI_WR_PPS_CMD) {
			cmd_set[i].type = MIPI_DSI_DSC_PPS;
			cmd_set[i].data_buf = cmd[i].buf;
			cmd_set[i].data_len = cmd[i].size;
		} else if (cmd[i].cmd_id == MIPI_DSI_WR_GEN_CMD) {
			if (cmd[i].size == 1) {
				cmd_set[i].type = MIPI_DSI_DCS_SHORT_WRITE;
				cmd_set[i].data_buf = cmd[i].buf;
				cmd_set[i].data_len = 1;
			} else {
				cmd_set[i].type = MIPI_DSI_DCS_LONG_WRITE;
				cmd_set[i].data_buf = cmd[i].buf;
				cmd_set[i].data_len = cmd[i].size;
			}
		} else {
			panel_info("invalid cmd_id %d\n", cmd[i].cmd_id);
			ret = -EINVAL;
			goto error;
		}

#if defined(DSIM_TX_FLOW_CONTROL)
		if ((i - from >= MAX_DSIM_PH_SIZE) ||
			(sz_pl + ALIGN(cmd_set[i].data_len, 4) >= MAX_DSIM_PL_SIZE)) {
			if (dsim_write_cmd_set(dsim, &cmd_set[from], i - from, false)) {
				panel_err("failed to write cmd_set\n");
				ret = -EIO;
				goto error;
			}
			panel_dbg("cmd_set:%d pl:%d\n", i - from, sz_pl);
			if (panel_cmd_log_enabled(PANEL_CMD_LOG_DSI_TX))
				print_dsim_cmd(&cmd_set[from], i - from);
			from = i;
			sz_pl = 0;
		}
#endif
		sz_pl += ALIGN(cmd_set[i].data_len, 4);
		total_size += cmd_set[i].data_len;
	}

	if (dsim_write_cmd_set(dsim, &cmd_set[from], i - from, false)) {
		panel_err("failed to write cmd_set\n");
		ret = -EIO;
		goto error;
	}

	ktime_get_ts(&cur_ts);
	delta_ts = timespec_sub(cur_ts, last_ts);
	elapsed_usec = timespec_to_ns(&delta_ts) / 1000;
	panel_dbg("done (cmd_set:%d size:%d elapsed %2lld.%03lld msec)\n",
			size, total_size,
			elapsed_usec / 1000, elapsed_usec % 1000);
	if (panel_cmd_log_enabled(PANEL_CMD_LOG_DSI_TX))
		print_dsim_cmd(&cmd_set[from], i - from);

	ret = total_size;

error:
	mutex_unlock(&cmd_lock);

	return ret;
}

static int mipi_sr_write(u32 id, u8 cmd_id, const u8 *cmd, u32 offset, int size, u32 option)
{
	int ret = 0;
	struct dsim_device *dsim = get_dsim_drvdata(id);
	s64 elapsed_usec;
	struct timespec cur_ts, last_ts, delta_ts;
	int align = 0;

	if (!cmd) {
		panel_err("cmd is null\n");
		return -EINVAL;
	}

	if (option & PKT_OPTION_SR_ALIGN_12)
		align = 12;
	else if (option & PKT_OPTION_SR_ALIGN_16)
		align = 16;

	if (align == 0) {
		/* protect for already released panel: 16byte align */
		panel_warn("sram packets need to align option, set force to 16\n");
		align = 16;
	}
	ktime_get_ts(&last_ts);

	mutex_lock(&cmd_lock);
	ret = dsim_sr_write_data(dsim, cmd, size, align);
	mutex_unlock(&cmd_lock);

	ktime_get_ts(&cur_ts);
	delta_ts = timespec_sub(cur_ts, last_ts);
	elapsed_usec = timespec_to_ns(&delta_ts) / 1000;
	panel_dbg("done (size:%d elapsed %2lld.%03lld msec)\n",
			size, elapsed_usec / 1000, elapsed_usec % 1000);

	return ret;
}

static int mipi_read(u32 id, u8 addr, u32 offset, u8 *buf, int size, u32 option)
{
	int ret, retry = 3;
	struct dsim_device *dsim = get_dsim_drvdata(id);

	if (!buf) {
		panel_err("buf is null\n");
		return -EINVAL;
	}

	mutex_lock(&cmd_lock);
	while (--retry >= 0) {
		if (offset > 0) {
			int gpara_len = 1;
			u8 gpara[4] = { 0xB0, 0x00 };

			/* gpara 16bit offset */
			if (option & DSIM_OPTION_2BYTE_GPARA)
				gpara[gpara_len++] = (offset >> 8) & 0xFF;

			gpara[gpara_len++] = offset & 0xFF;

			/* pointing gpara */
			if (option & DSIM_OPTION_POINT_GPARA)
				gpara[gpara_len++] = addr;

			if (panel_cmd_log_enabled(PANEL_CMD_LOG_DSI_TX))
				print_tx(MIPI_DSI_DCS_LONG_WRITE, gpara, gpara_len);
			if (dsim_write_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
						(unsigned long)gpara, gpara_len, false)) {
				panel_err("failed to write gpara %d (retry %d)\n",
						offset, retry);
				continue;
			}
		}

		if (panel_cmd_log_enabled(PANEL_CMD_LOG_DSI_TX)) {
			u8 read_cmd1[] = { size };
			u8 read_cmd2[] = { addr };

			print_tx(MIPI_DSI_SET_MAXIMUM_RETURN_PACKET_SIZE,
					read_cmd1, ARRAY_SIZE(read_cmd1));
			print_tx(MIPI_DSI_DCS_READ, read_cmd2, ARRAY_SIZE(read_cmd2));
		}
		ret = dsim_read_data(dsim, MIPI_DSI_DCS_READ,
				(u32)addr, size, buf);
		if (ret != size) {
			panel_err("failed to read addr %02X ofs %d size %d (ret %d, retry %d)\n",
					addr, offset, size, ret, retry);
			continue;
		}
		if (panel_cmd_log_enabled(PANEL_CMD_LOG_DSI_RX))
			print_rx(addr, buf, size);
		break;
	}

	if (retry < 0) {
		panel_err("failed: exceed retry count (addr %02X)\n", addr);
		ret = -EIO;
		goto error;
	}

	panel_dbg("addr %02X ofs %d size %d, buf %02X done\n",
			addr, offset, size, buf[0]);

	ret = size;

error:
	mutex_unlock(&cmd_lock);
	return ret;
}

enum dsim_state get_dsim_state(u32 id)
{
	struct dsim_device *dsim = get_dsim_drvdata(id);

	if (dsim == NULL) {
		panel_err("dsim is NULL\n");
		return -ENODEV;
	}
	return dsim->state;
}

static struct exynos_panel_info *get_lcd_info(u32 id)
{
	return &get_panel_drvdata(id)->lcd_info;
}

static int wait_for_vsync(u32 id, u32 timeout)
{
	struct decon_device *decon = get_decon_drvdata(0);
	int ret;

	if (!decon)
		return -EINVAL;

	decon_hiber_block_exit(decon);
	ret = decon_wait_for_vsync(decon, timeout);
	decon_hiber_unblock(decon);

	return ret;
}

static int panel_drv_put_ops(struct exynos_panel_device *panel)
{
	int ret = 0;
	struct mipi_drv_ops mipi_ops;

	mipi_ops.read = mipi_read;
	mipi_ops.write = mipi_write;
	mipi_ops.write_table = mipi_write_table;
	mipi_ops.sr_write = mipi_sr_write;
	mipi_ops.get_state = get_dsim_state;
	mipi_ops.parse_dt = parse_lcd_info;
	mipi_ops.get_lcd_info = get_lcd_info;
	mipi_ops.wait_for_vsync = wait_for_vsync;

	v4l2_set_subdev_hostdata(panel->panel_drv_sd, &mipi_ops);

	ret = panel_drv_ioctl(panel, PANEL_IOC_DSIM_PUT_MIPI_OPS, NULL);
	if (ret) {
		panel_err("failed to set mipi ops\n");
		return ret;
	}

	return ret;
}

static int panel_drv_init(struct exynos_panel_device *panel)
{
	int ret;

	ret = panel_drv_put_ops(panel);
	if (ret) {
		panel_err("failed to put ops\n");
		goto do_exit;
	}

	ret = panel_drv_probe(panel);
	if (ret) {
		panel_err("failed to probe panel");
		goto do_exit;
	}

	ret = panel_drv_get_state(panel);
	if (ret) {
		panel_err("failed to get panel state\n");
		goto do_exit;
	}

#if defined(CONFIG_PANEL_DISPLAY_MODE)
	ret = panel_drv_get_panel_display_mode(panel);
	if (ret < 0) {
		panel_err("failed to get panel_display_modes\n");
		goto do_exit;
	}
#endif

	ret = panel_drv_get_mres(panel);
	if (ret) {
		panel_err("failed to get panel mres\n");
		goto do_exit;
	}

	ret = panel_drv_set_vrr_cb(panel);
	if (ret) {
		panel_err("failed to set vrr callback\n");
		goto do_exit;
	}

	return ret;

do_exit:
	return ret;
}

static int common_panel_connected(struct exynos_panel_device *panel)
{
	int ret;

	ret = panel_drv_get_state(panel);
	if (ret) {
		panel_err("failed to get panel state\n");
		return ret;
	}

	ret = !(panel_state->connect_panel == PANEL_DISCONNECT);
	panel_info("panel %s\n",
			ret ? "connected" : "disconnected");

	return ret;
}

static int common_panel_init(struct exynos_panel_device *panel)
{
	int ret;

	ret = panel_drv_init(panel);
	if (ret) {
		panel_err("failed to init common panel\n");
		return ret;
	}

	return 0;
}

static int common_panel_probe(struct exynos_panel_device *panel)
{
	int ret;

	ret = panel_drv_ioctl(panel, PANEL_IOC_PANEL_PROBE, &panel->id);
	if (ret) {
		panel_err("failed to probe panel\n");
		return ret;
	}

	return 0;
}

static int common_panel_displayon(struct exynos_panel_device *panel)
{
	int ret;
	int disp_on = 1;

	ret = panel_drv_ioctl(panel, PANEL_IOC_DISP_ON, (void *)&disp_on);
	if (ret) {
		panel_err("failed to display on\n");
		return ret;
	}

	return 0;
}

static int common_panel_suspend(struct exynos_panel_device *panel)
{
	int ret;

	ret = panel_drv_ioctl(panel, PANEL_IOC_SLEEP_IN, NULL);
	if (ret) {
		panel_err("failed to sleep in\n");
		return ret;
	}

	return 0;
}

static int common_panel_resume(struct exynos_panel_device *panel)
{
	return 0;
}

static int common_panel_dump(struct exynos_panel_device *panel)
{
	int ret;

	ret = panel_drv_ioctl(panel, PANEL_IOC_PANEL_DUMP, NULL);
	if (ret) {
		panel_err("failed to dump panel\n");
		return ret;
	}

	return 0;
}

static int common_panel_setarea(struct exynos_panel_device *panel, u32 l, u32 r, u32 t, u32 b)
{
	int ret = 0;
	char column[5];
	char page[5];
	int retry;
	struct dsim_device *dsim = get_dsim_drvdata(panel->id);

	column[0] = MIPI_DCS_SET_COLUMN_ADDRESS;
	column[1] = (l >> 8) & 0xff;
	column[2] = l & 0xff;
	column[3] = (r >> 8) & 0xff;
	column[4] = r & 0xff;

	page[0] = MIPI_DCS_SET_PAGE_ADDRESS;
	page[1] = (t >> 8) & 0xff;
	page[2] = t & 0xff;
	page[3] = (b >> 8) & 0xff;
	page[4] = b & 0xff;

	mutex_lock(&cmd_lock);
	retry = 2;

	if (panel_cmd_log_enabled(PANEL_CMD_LOG_DSI_TX))
		print_tx(MIPI_DSI_DCS_LONG_WRITE, column, ARRAY_SIZE(column));
	while (dsim_write_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				(unsigned long)column, ARRAY_SIZE(column), false) != 0) {
		panel_err("failed to write COLUMN_ADDRESS\n");
		if (--retry <= 0) {
			panel_err("COLUMN_ADDRESS is failed: exceed retry count\n");
			ret = -EINVAL;
			goto error;
		}
	}

	retry = 2;
	if (panel_cmd_log_enabled(PANEL_CMD_LOG_DSI_TX))
		print_tx(MIPI_DSI_DCS_LONG_WRITE, page, ARRAY_SIZE(page));
	while (dsim_write_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				(unsigned long)page, ARRAY_SIZE(page), true) != 0) {
		panel_err("failed to write PAGE_ADDRESS\n");
		if (--retry <= 0) {
			panel_err("PAGE_ADDRESS is failed: exceed retry count\n");
			ret = -EINVAL;
			goto error;
		}
	}

	panel_dbg("RECT [l:%d r:%d t:%d b:%d w:%d h:%d]\n",
			l, r, t, b, r - l + 1, b - t + 1);

error:
	mutex_unlock(&cmd_lock);
	return ret;
}

static int common_panel_power(struct exynos_panel_device *panel, int on)
{
	int ret;

	ret = panel_drv_ioctl(panel, PANEL_IOC_SET_POWER, (void *)&on);
	if (ret) {
		panel_err("failed to panel %s\n", on ? "on" : "off");
		return ret;
	}
	panel_dbg("power %s\n", on ? "on" : "off");

	return 0;
}

static int common_panel_poweron(struct exynos_panel_device *panel)
{
	return common_panel_power(panel, 1);
}

static int common_panel_poweroff(struct exynos_panel_device *panel)
{
	return common_panel_power(panel, 0);
}

static int common_panel_sleepin(struct exynos_panel_device *panel)
{
	int ret;

	ret = panel_drv_ioctl(panel, PANEL_IOC_SLEEP_IN, NULL);
	if (ret) {
		panel_err("failed to sleep in\n");
		return ret;
	}

	return 0;
}

static int common_panel_sleepout(struct exynos_panel_device *panel)
{
	int ret;

	ret = panel_drv_ioctl(panel, PANEL_IOC_SLEEP_OUT, NULL);
	if (ret) {
		panel_err("failed to sleep out\n");
		return ret;
	}

	return 0;
}

static int common_panel_notify(struct exynos_panel_device *panel, void *data)
{
	int ret;

	ret = panel_drv_notify(panel->panel_drv_sd,
			V4L2_DEVICE_NOTIFY_EVENT, data);
	if (ret) {
		panel_err("failed to notify\n");
		return ret;
	}

	return 0;
}

static int common_panel_read_state(struct exynos_panel_device *panel)
{
	return true;
}

#ifdef CONFIG_EXYNOS_DOZE
static int common_panel_doze(struct exynos_panel_device *panel)
{
#ifdef CONFIG_SUPPORT_DOZE
	int ret;

	ret = panel_drv_ioctl(panel, PANEL_IOC_DOZE, NULL);
	if (ret) {
		panel_err("failed to sleep out\n");
		return ret;
	}
#endif

	return 0;
}

static int common_panel_doze_suspend(struct exynos_panel_device *panel)
{
#ifdef CONFIG_SUPPORT_DOZE
	int ret;

	ret = panel_drv_ioctl(panel, PANEL_IOC_DOZE_SUSPEND, NULL);
	if (ret) {
		panel_err("failed to sleep out\n");
		return ret;
	}
#endif

	return 0;
}
#endif

#if defined(CONFIG_PANEL_DISPLAY_MODE)
static int common_panel_get_display_mode(struct exynos_panel_device *panel, void *data)
{
	int ret;

	ret = panel_drv_get_panel_display_mode(panel);
	if (ret < 0) {
		panel_err("failed to get panel_display_modes\n");
		return ret;
	}

	/*
	 * TODO : implement get current panel_display_mode
	 */
	return 0;
}

static int common_panel_set_display_mode(struct exynos_panel_device *panel, void *data)
{
	struct panel_display_modes *panel_modes;
	struct exynos_panel_info *lcd_info;
	int ret, panel_mode_idx;

	if (!data)
		return -EINVAL;

	lcd_info = &panel->lcd_info;
	panel_modes = lcd_info->panel_modes;
	if (!panel_modes) {
		panel_err("panel_modes not prepared\n");
		return -EPERM;
	}

	panel_mode_idx = *(int *)data;
	if (panel_mode_idx < 0 ||
		panel_mode_idx >= panel_modes->num_modes) {
		panel_err("invalid panel_mode_idx(%d)\n", panel_mode_idx);
		return -EINVAL;
	}

	ret = panel_drv_ioctl(panel,
			PANEL_IOC_SET_DISPLAY_MODE,
			panel_modes->modes[panel_mode_idx]);
	if (ret < 0) {
		panel_err("failed to set panel_display_mode(%d)\n",
				panel_mode_idx);
		return ret;
	}

	return 0;
}
#endif

struct exynos_panel_ops common_panel_ops = {
	.init		= common_panel_init,
	.probe		= common_panel_probe,
	.suspend	= common_panel_suspend,
	.resume		= common_panel_resume,
	.dump		= common_panel_dump,
	.connected	= common_panel_connected,
	.setarea	= common_panel_setarea,
	.poweron	= common_panel_poweron,
	.poweroff	= common_panel_poweroff,
	.sleepin	= common_panel_sleepin,
	.sleepout	= common_panel_sleepout,
	.displayon	= common_panel_displayon,
	.notify		= common_panel_notify,
	.read_state	= common_panel_read_state,
	.set_error_cb	= common_panel_set_error_cb,
#ifdef CONFIG_EXYNOS_DOZE
	.doze		= common_panel_doze,
	.doze_suspend	= common_panel_doze_suspend,
#endif
	.mres = NULL,
	.set_vrefresh = NULL,
#if defined(CONFIG_PANEL_DISPLAY_MODE)
	.get_display_mode = common_panel_get_display_mode,
	.set_display_mode = common_panel_set_display_mode,
#endif
};
