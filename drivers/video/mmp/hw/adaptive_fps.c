/*
 * linux/drivers/video/mmp/hw/adaptive_fps.c
 * vsync driver support
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

#include <linux/device.h>
#include <linux/io.h>
#include <linux/stat.h>
#include <linux/clk.h>
#include "mmp_dsi.h"
#include <video/mipi_display.h>

extern void dsi_set_ctrl0(struct mmp_dsi_regs *dsi_regs, u32 mask, u32 set);
#define RETRY_TIMEOUT	(30)

static void adaptive_fps_handle(void *data)
{
	struct mmp_dsi *mmp_dsi = (struct mmp_dsi *)data;
	struct mmp_mode *mode = &mmp_dsi->mode;
	struct mmp_dsi_regs *dsi = mmp_dsi->reg_base;
	struct dsi_lcd_regs *dsi_lcd = &dsi->lcd1;
	static unsigned int refresh;
	u32 temp, rate, total_w, total_h, ntotal_h;

	if (atomic_read(&mmp_dsi->adaptive_fps_commit)) {
		dsi_set_ctrl0(dsi, 0, DSI_CTRL_0_CFG_LCD1_EN);
		atomic_set(&mmp_dsi->adaptive_fps_commit, 0);
		//pr_info("%s, fps %d, vfp %d\n", __func__, mode->real_refresh, mode->lower_margin);
	}
}

void mmp_register_adaptive_fps_handler(struct device *dev,
	struct mmp_vsync *vsync)
{
	struct mmp_vsync_notifier_node *adaptive_fps_notifier_node = NULL;
	struct mmp_dsi *dsi = NULL;

	if (vsync->type == DSI_SPECIAL_VSYNC)
		dsi = vsync->dsi;
	else
		BUG();

	adaptive_fps_notifier_node = devm_kzalloc(dev, sizeof(*adaptive_fps_notifier_node), GFP_KERNEL);
	if (adaptive_fps_notifier_node == NULL) {
		pr_err("dfc alloc failure\n");
		return;
	}

	adaptive_fps_notifier_node->cb_notify = adaptive_fps_handle;
	adaptive_fps_notifier_node->cb_data = dsi;
	mmp_register_vsync_cb(vsync, adaptive_fps_notifier_node);
}

void update_fps(struct mmp_dsi *mmp_dsi, unsigned int fps)
{
	struct mmp_dsi_regs *dsi = mmp_dsi->reg_base;
	struct dsi_lcd_regs *dsi_lcd = &dsi->lcd1;
	struct mmp_mode *mode = &mmp_dsi->mode;
	unsigned int temp, total_w, total_h, ntotal_h, rate, count = 0;;

	pr_debug("%s Changing fps to %d\n", __func__, fps);
	mode->real_refresh = fps;
	rate = clk_get_rate(mmp_dsi->clk);
	total_w = mode->xres + mode->left_margin +
		mode->right_margin + mode->hsync_len;
	total_h = mode->yres + mode->upper_margin +
		mode->lower_margin + mode->vsync_len;
	ntotal_h = (((rate * mmp_dsi->setting.lanes) / total_w) / 24) / mode->real_refresh;
	mode->lower_margin = ntotal_h - (mode->yres + mode->upper_margin + mode->vsync_len);
	if ((int)mode->lower_margin > 0) {
		temp = readl_relaxed(&dsi_lcd->timing2);
		temp &= ~(0xFFFF);
		total_h = mode->yres + mode->upper_margin +
			mode->lower_margin + mode->vsync_len;
		temp |= total_h;
		writel_relaxed(temp, &dsi_lcd->timing2);
		atomic_set(&mmp_dsi->adaptive_fps_commit, 1);
		do {
			mmp_wait_vsync(&mmp_dsi->special_vsync);
			count++;
		} while (atomic_read(&mmp_dsi->adaptive_fps_commit) && (count < RETRY_TIMEOUT));
		if (count == RETRY_TIMEOUT)
			pr_info("%s, Does not needs to update adaptive fps\n", __func__);
	} else {
		pr_info("%s, Does not needs to update vfp :%d rate:%d\n", __func__,
				(int)mode->lower_margin, (int)clk_get_rate(mmp_dsi->clk) / 1000000);
	}
}

static ssize_t adaptive_fps_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct mmp_dsi *mmp_dsi = dev_get_drvdata(dev);
	unsigned int temp, value;

	if (kstrtoul(buf, 0, (unsigned long *)&value))
		return -EINVAL;

	if (value < 0 || value > 65) {
		pr_err("%s, invalid fps %d\n", __func__, value);
		return -EINVAL;
	}

	update_fps(mmp_dsi, value);

	return size;
}

ssize_t adaptive_fps_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct mmp_dsi *mmp_dsi = dev_get_drvdata(dev);
	struct mmp_mode *mode = &mmp_dsi->mode;
	int s = 0;

	s += sprintf(buf + s, "refresh %d\n", mode->real_refresh);
	s += sprintf(buf + s, "vfp %d\n", mode->lower_margin);

	return s;
}

DEVICE_ATTR(adaptive_fps, 0644, adaptive_fps_show, adaptive_fps_store);

int adaptive_fps_init(struct device *dev)
{
	int ret;

	ret = device_create_file(dev, &dev_attr_adaptive_fps);
	if (ret < 0)
		goto err_refresh;

	return 0;

err_refresh:
	return ret;

}

void adaptive_fps_uninit(struct device *dev)
{
	device_remove_file(dev, &dev_attr_adaptive_fps);
}
