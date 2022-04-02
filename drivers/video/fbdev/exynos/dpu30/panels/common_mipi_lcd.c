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
		decon_dbg("%s unknown event\n", __func__);
		return -EINVAL;
	}

	switch (ev->type) {
	case V4L2_EVENT_DECON_FRAME_DONE:
		ret = v4l2_subdev_call(sd, core, ioctl,
				PANEL_IOC_EVT_FRAME_DONE, &ev->timestamp);
		if (ret) {
			pr_err("%s failed to notify FRAME_DONE\n", __func__);
			return ret;
		}
		break;
	case V4L2_EVENT_DECON_VSYNC:
		ret = v4l2_subdev_call(sd, core, ioctl,
				PANEL_IOC_EVT_VSYNC, &ev->timestamp);
		if (ret) {
			pr_err("%s failed to notify VSYNC\n", __func__);
			return ret;
		}
		break;
	default:
		pr_warn("%s unknown event type %d\n", __func__, ev->type);
		break;
	}

	pr_debug("%s event type %d timestamp %ld %ld nsec\n",
			__func__, ev->type, ev->timestamp.tv_sec,
			ev->timestamp.tv_nsec);

	return 0;
}

static int common_panel_set_error_cb(struct exynos_panel_device *panel, void *arg)
{
	int ret;

	v4l2_set_subdev_hostdata(panel->panel_drv_sd, arg);
	ret = panel_drv_ioctl(panel, PANEL_IOC_REG_RESET_CB, NULL);
	if (ret) {
		pr_err("ERR:%s:failed to set panel error callback\n", __func__);
		return ret;
	}

	return 0;
}

static int panel_drv_probe(struct exynos_panel_device *panel)
{
	int ret;

	ret = panel_drv_ioctl(panel, PANEL_IOC_DSIM_PROBE, (void *)&panel->id);
	if (ret) {
		pr_err("ERR:%s:failed to panel dsim probe\n", __func__);
		return ret;
	}

	return ret;
}

static int panel_drv_get_state(struct exynos_panel_device *panel)
{
	int ret = 0;

	ret = panel_drv_ioctl(panel, PANEL_IOC_GET_PANEL_STATE, NULL);
	if (ret) {
		pr_err("ERR:%s:failed to get panel state", __func__);
		return ret;
	}

	panel_state = (struct panel_state *)
		v4l2_get_subdev_hostdata(panel->panel_drv_sd);
	if (IS_ERR_OR_NULL(panel_state)) {
		pr_err("ERR:%s:failed to get lcd information\n", __func__);
		return -EINVAL;
	}

	return ret;
}

static int panel_drv_get_mres(struct exynos_panel_device *panel)
{
	int ret = 0;

	ret = panel_drv_ioctl(panel, PANEL_IOC_GET_MRES, NULL);
	if (ret) {
		pr_err("ERR:%s:failed to get panel mres", __func__);
		return ret;
	}

	mres = (struct panel_mres *)
		v4l2_get_subdev_hostdata(panel->panel_drv_sd);
	if (IS_ERR_OR_NULL(mres)) {
		pr_err("ERR:%s:failed to get lcd information\n", __func__);
		return -EINVAL;
	}

	return ret;
}

int mipi_write(u32 id, u8 cmd_id, const u8 *cmd, u8 offset, int size, u32 option)
{
	int ret, retry = 3;
	unsigned long d0;
	u32 type, d1;
	bool block = (option & DSIM_OPTION_WAIT_TX_DONE);
	struct dsim_device *dsim = get_dsim_drvdata(id);

	if (!cmd) {
		pr_err("%s cmd is null\n", __func__);
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
		pr_info("%s invalid cmd_id %d\n", __func__, cmd_id);
		return -EINVAL;
	}

	mutex_lock(&cmd_lock);
	while (--retry >= 0) {
		if (offset > 0) {
			if (option & DSIM_OPTION_POINT_GPARA) {
				u8 gpara[3] = { 0xB0, offset, cmd[0] };
				if (dsim_write_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
							(unsigned long)gpara, ARRAY_SIZE(gpara), false)) {
					pr_err("%s failed to write gpara %d (retry %d)\n",
							__func__, offset, retry);
					continue;
				}
			} else {
				if (dsim_write_data(dsim,
							MIPI_DSI_DCS_SHORT_WRITE_PARAM, 0xB0, offset, false)) {
					pr_err("%s failed to write gpara %d (retry %d)\n",
							__func__, offset, retry);
					continue;
				}
			}
		}

		if (dsim_write_data(dsim, type, d0, d1, block)) {
			pr_err("%s failed to write cmd %02X size %d(retry %d)\n",
					__func__, cmd[0], size, retry);
			continue;
		}

		break;
	}

	if (retry < 0) {
		pr_err("%s failed: exceed retry count (cmd %02X)\n",
				__func__, cmd[0]);
		ret = -EIO;
		goto error;
	}

	pr_debug("%s cmd_id %d, addr %02X offset %d size %d\n",
			__func__, cmd_id, cmd[0], offset, size);
	ret = size;

error:
	mutex_unlock(&cmd_lock);
	return ret;
}

int mipi_read(u32 id, u8 addr, u8 offset, u8 *buf, int size, u32 option)
{
	int ret, retry = 3;
	struct dsim_device *dsim = get_dsim_drvdata(id);

	if (!buf) {
		pr_err("%s buf is null\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&cmd_lock);
	while (--retry >= 0) {
		if (offset > 0) {
			if (option & DSIM_OPTION_POINT_GPARA) {
				u8 gpara[3] = { 0xB0, offset, addr };
				if (dsim_write_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
							(unsigned long)gpara, ARRAY_SIZE(gpara), false)) {
					pr_err("%s failed to write gpara %d (retry %d)\n",
							__func__, offset, retry);
					continue;
				}
			} else {
				if (dsim_write_data(dsim,
							MIPI_DSI_DCS_SHORT_WRITE_PARAM, 0xB0, offset, false)) {
					pr_err("%s failed to write gpara %d (retry %d)\n",
							__func__, offset, retry);
					continue;
				}
			}
		}

		ret = dsim_read_data(dsim, MIPI_DSI_DCS_READ,
				(u32)addr, size, buf);
		if (ret != size) {
			pr_err("%s, failed to read addr %02X ofs %d size %d (ret %d, retry %d)\n",
					__func__, addr, offset, size, ret, retry);
			continue;
		}
		break;
	}

	if (retry < 0) {
		pr_err("%s failed: exceed retry count (addr %02X)\n",
				__func__, addr);
		ret = -EIO;
		goto error;
	}

	pr_debug("%s addr %02X ofs %d size %d, buf %02X done\n",
			__func__, addr, offset, size, buf[0]);

	ret = size;

error:
	mutex_unlock(&cmd_lock);
	return ret;
}

void mipi_lpdt(u32 id, u32 lpdt_on)
{
	pr_info("%s %x\n", __func__, lpdt_on);
	dsim_reg_set_cmd_transfer_mode(id, lpdt_on);
}

enum dsim_state get_dsim_state(u32 id)
{
	struct dsim_device *dsim = get_dsim_drvdata(id);

	if (dsim == NULL) {
		pr_err("ERR:%s:dsim is NULL\n", __func__);
		return -ENODEV;
	}
	return dsim->state;
}

static struct exynos_panel_info *get_lcd_info(u32 id)
{
	return &get_panel_drvdata()->lcd_info;
}

static int panel_drv_put_ops(struct exynos_panel_device *panel)
{
	int ret = 0;
	struct mipi_drv_ops mipi_ops;

	mipi_ops.read = mipi_read;
	mipi_ops.write = mipi_write;
	mipi_ops.get_state = get_dsim_state;
	mipi_ops.parse_dt = parse_lcd_info;
	mipi_ops.get_lcd_info = get_lcd_info;
	mipi_ops.set_lpdt = mipi_lpdt;

	v4l2_set_subdev_hostdata(panel->panel_drv_sd, &mipi_ops);

	ret = panel_drv_ioctl(panel, PANEL_IOC_DSIM_PUT_MIPI_OPS, NULL);
	if (ret) {
		pr_err("ERR:%s:failed to set mipi ops\n", __func__);
		return ret;
	}

	return ret;
}

static int panel_drv_init(struct exynos_panel_device *panel)
{
	int ret;

	ret = panel_drv_put_ops(panel);
	if (ret) {
		pr_err("ERR:%s:failed to put ops\n", __func__);
		goto do_exit;
	}

	ret = panel_drv_probe(panel);
	if (ret) {
		pr_err("ERR:%s:failed to probe panel", __func__);
		goto do_exit;
	}

	ret = panel_drv_get_state(panel);
	if (ret) {
		pr_err("ERR:%s:failed to get panel state\n", __func__);
		goto do_exit;
	}

	ret = panel_drv_get_mres(panel);
	if (ret) {
		pr_err("ERR:%s:failed to get panel mres\n", __func__);
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
		pr_err("ERR:%s:failed to get panel state\n", __func__);
		return ret;
	}

	ret = !(panel_state->connect_panel == PANEL_DISCONNECT);
	pr_info("%s panel %s\n", __func__,
			ret ? "connected" : "disconnected");

	return ret;
}

static int common_panel_init(struct exynos_panel_device *panel)
{
	int ret;

	ret = panel_drv_init(panel);
	if (ret) {
		pr_err("ERR:%s:failed to init common panel\n", __func__);
		return ret;
	}

	return 0;
}

static int common_panel_probe(struct exynos_panel_device *panel)
{
	int ret;

	ret = panel_drv_ioctl(panel, PANEL_IOC_PANEL_PROBE, &panel->id);
	if (ret) {
		pr_err("ERR:%s:failed to probe panel\n", __func__);
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
		pr_err("ERR:%s:failed to display on\n", __func__);
		return ret;
	}

	return 0;
}

static int common_panel_suspend(struct exynos_panel_device *panel)
{
	int ret;

	ret = panel_drv_ioctl(panel, PANEL_IOC_SLEEP_IN, NULL);
	if (ret) {
		pr_err("ERR:%s:failed to sleep in\n", __func__);
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
		pr_err("ERR:%s:failed to dump panel\n", __func__);
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
	struct dsim_device *dsim = get_dsim_drvdata(0);

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

	while (dsim_write_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				(unsigned long)column, ARRAY_SIZE(column), false) != 0) {
		pr_err("failed to write COLUMN_ADDRESS\n");
		if (--retry <= 0) {
			pr_err("COLUMN_ADDRESS is failed: exceed retry count\n");
			ret = -EINVAL;
			goto error;
		}
	}

	retry = 2;
	while (dsim_write_data(dsim, MIPI_DSI_DCS_LONG_WRITE,
				(unsigned long)page, ARRAY_SIZE(page), true) != 0) {
		pr_err("failed to write PAGE_ADDRESS\n");
		if (--retry <= 0) {
			pr_err("PAGE_ADDRESS is failed: exceed retry count\n");
			ret = -EINVAL;
			goto error;
		}
	}

	pr_debug("RECT [l:%d r:%d t:%d b:%d w:%d h:%d]\n",
			l, r, t, b, r - l + 1, b - t + 1);

error:
	mutex_unlock(&cmd_lock);
	return ret;
}

static int common_panel_reset(struct exynos_panel_device *panel)
{
	int ret;

	ret = panel_drv_ioctl(panel, PANEL_IOC_SET_RESET, NULL);
	if (ret) {
		pr_err("ERR:%s:failed to panel\n", __func__);
		return ret;
	}
	pr_debug("%s reset\n", __func__);

	return 0;
}

static int common_panel_power(struct exynos_panel_device *panel, int on)
{
	int ret;

	ret = panel_drv_ioctl(panel, PANEL_IOC_SET_POWER, (void *)&on);
	if (ret) {
		pr_err("ERR:%s:failed to panel %s\n",
				__func__, on ? "on" : "off");
		return ret;
	}
	pr_debug("%s power %s\n", __func__, on ? "on" : "off");

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
		pr_err("ERR:%s:failed to sleep in\n", __func__);
		return ret;
	}

	return 0;
}

static int common_panel_sleepout(struct exynos_panel_device *panel)
{
	int ret;

	ret = panel_drv_ioctl(panel, PANEL_IOC_SLEEP_OUT, NULL);
	if (ret) {
		pr_err("ERR:%s:failed to sleep out\n", __func__);
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
		pr_err("ERR:%s:failed to notify\n", __func__);
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
	int ret;

	ret = panel_drv_ioctl(panel, PANEL_IOC_DOZE, NULL);
	if (ret) {
		pr_err("ERR:%s:failed to sleep out\n", __func__);
		return ret;
	}

	return 0;
}

static int common_panel_doze_suspend(struct exynos_panel_device *panel)
{
	int ret;

	ret = panel_drv_ioctl(panel, PANEL_IOC_DOZE_SUSPEND, NULL);
	if (ret) {
		pr_err("ERR:%s:failed to sleep out\n", __func__);
		return ret;
	}

	return 0;
}
#endif

#ifdef CONFIG_EXYNOS_ADAPTIVE_FREQ
static int common_mipi_freq_change(struct exynos_panel_device *panel)
{
	int ret = 0;

	ret = panel_drv_ioctl(panel,
			PANEL_IOC_MIPI_FREQ_CHANGED, NULL);
	if (ret) {
		pr_err("ERR:%s:failed to change freq\n",
				__func__);
		return ret;
	}

	return ret;
}
#endif

static int common_panel_mres(struct exynos_panel_device *panel, int mres_idx)
{
	struct exynos_panel_info *info;
	struct lcd_res_info *res;
	int i, ret;

	info = &panel->lcd_info;
	res = &info->mres.res_info[mres_idx];

	if (mres->nr_resol == 0 || mres->resol == NULL) {
		pr_err("%s:panel doesn't support multi-resolution\n",
				__func__);
		return -EINVAL;
	}

	for (i = 0; i < mres->nr_resol; i++) {
		if ((mres->resol[i].w == res->width) &&
			(mres->resol[i].h == res->height))
			break;
	}

	if (i == mres->nr_resol) {
		pr_err("%s:unsupported resolution(%dx%d)\n",
				__func__, res->width, res->height);
		return -EINVAL;
	}

	ret = panel_drv_ioctl(panel, PANEL_IOC_SET_MRES, &i);
	if (ret) {
		pr_err("ERR:%s:failed to set multi-resolution\n", __func__);
		goto mres_exit;
	}

	info->mres_mode = i;
	info->xres = mres->resol[i].w;
	info->yres = mres->resol[i].h;
	info->dsc.en = !(mres->resol[i].comp_type == PN_COMP_TYPE_NONE);
	info->dsc.slice_num = info->xres / mres->resol[i].comp_param.dsc.slice_w;
	info->dsc.slice_h = mres->resol[i].comp_param.dsc.slice_h;
	info->dsc.enc_sw = exynos_panel_calc_slice_width(info->dsc.cnt,
			info->dsc.slice_num, info->xres);
	info->dsc.dec_sw = mres->resol[i].comp_param.dsc.slice_w;

	pr_info("%s update resolution resol(%dx%d) dsc(en:%d slice(%d):%dx%d)\n",
			__func__, info->xres, info->yres, info->dsc.en,
			info->dsc.slice_num,  info->dsc.dec_sw, info->dsc.slice_h);

mres_exit:
	return ret;
}

#ifdef CONFIG_SUPPORT_INDISPLAY
static int common_panel_indisplay_pre(struct exynos_panel_device *panel, bool enable)
{
	int ret;

	ret = panel_drv_ioctl(panel, PANEL_IOC_SET_INDISPLAY_PRE, &enable);
	if (ret) {
		pr_err("ERR:%s:failed to mask brightness \n", __func__);
		return ret;
	}

	return 0;
}

static int common_panel_indisplay_post(struct exynos_panel_device *panel, bool enable)
{
	int ret;

	ret = panel_drv_ioctl(panel, PANEL_IOC_SET_INDISPLAY_POST, &enable);
	if (ret) {
		pr_err("ERR:%s:failed to mask brightness \n", __func__);
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
	.reset		= common_panel_reset,
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
#ifdef CONFIG_SUPPORT_INDISPLAY
	.indisplay_pre	= common_panel_indisplay_pre,
	.indisplay_post	= common_panel_indisplay_post,
#endif
	.mres = common_panel_mres,
};
