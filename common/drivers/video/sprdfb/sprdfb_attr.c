/*
 * Copyright (C) 2014 Spreadtrum Communications Inc.
 *
 * Author: Haibing.Yang <haibing.yang@spreadtrum.com>
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

#define pr_fmt(fmt)		"sprdfb_sysfs: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <mach/board.h>

#include "sprdfb.h"
#include "sprdfb_panel.h"


struct attr_info {
	struct sprdfb_device *fb_dev;
	/* clock management */
	u32 origin_pclk;
	u32 curr_pclk;
	u32 origin_fps;
	u32 curr_fps;
	u32 origin_mipi_clk;
	u32 curr_mipi_clk;

	struct semaphore sem;
};

static ssize_t sysfs_rd_current_pclk(struct device *dev,
		struct device_attribute *attr, char *buf);
static ssize_t sysfs_write_pclk(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count);
static ssize_t sysfs_rd_current_fps(struct device *dev,
		struct device_attribute *attr, char *buf);
static ssize_t sysfs_write_fps(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count);
static ssize_t sysfs_rd_current_mipi_clk(struct device *dev,
		struct device_attribute *attr, char *buf);
static ssize_t sysfs_write_mipi_clk(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count);

static DEVICE_ATTR(dynamic_pclk, S_IRUGO | S_IWUSR, sysfs_rd_current_pclk,
		sysfs_write_pclk);
static DEVICE_ATTR(dynamic_fps, S_IRUGO | S_IWUSR, sysfs_rd_current_fps,
		sysfs_write_fps);
static DEVICE_ATTR(dynamic_mipi_clk, S_IRUGO | S_IWUSR,
		sysfs_rd_current_mipi_clk, sysfs_write_mipi_clk);

static struct attribute *sprdfb_fs_attrs[] = {
	&dev_attr_dynamic_pclk.attr,
	&dev_attr_dynamic_fps.attr,
	&dev_attr_dynamic_mipi_clk.attr,
	NULL,
};

static struct attribute_group sprdfb_attrs_group = {
	.attrs = sprdfb_fs_attrs,
};

/**
 * @fb_dev - sprdfb specific device
 * @type: SPRDFB_DYNAMIC_PCLK, SPRDFB_DYNAMIC_FPS, SPRDFB_DYNAMIC_MIPI_CLK
 * @new_val: new required fps, dpi clock or mipi clock
 *
 * If clk is set unsuccessfully, this function returns minus.
 * It returns 0 if clk is set successfully.
 */
int sprdfb_chg_clk_intf(struct sprdfb_device *fb_dev,
			int type, u32 new_val)
{
	int ret;
	struct attr_info *attr = fb_dev->priv1;

	down(&attr->sem);
	ret = sprdfb_dispc_chg_clk(fb_dev, type, new_val);
	up(&attr->sem);

	return ret;
}

static ssize_t sysfs_rd_current_pclk(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct sprdfb_device *fb_dev = (struct sprdfb_device *)fbi->par;
	struct attr_info *attr_info = NULL;

	if (!fb_dev) {
		pr_err("fb_dev can't be found\n");
		return -ENXIO;
	}

	attr_info = (struct attr_info *)fb_dev->priv1;

	ret = snprintf(buf, PAGE_SIZE,
			"current dpi_clk: %u\nnew dpi_clk: %u\norigin dpi_clk: %u\n",
			fb_dev->dpi_clock, attr_info->curr_pclk,
			attr_info->origin_pclk);

	return ret;
}

static ssize_t sysfs_write_pclk(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	int ret;
	int divider;
	u32 dpi_clk_src, new_pclk;
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct sprdfb_device *fb_dev = (struct sprdfb_device *)fbi->par;
	struct attr_info *attr_info = fb_dev->priv1;

	sscanf(buf, "%u,%d\n", &dpi_clk_src, &divider);

	if (divider == 0 && dpi_clk_src == 0) {
		new_pclk = attr_info->origin_pclk;
		goto DIRECT_GO;
	}

	if (divider < 1 || divider > 0xff || dpi_clk_src < 1) {
		pr_err("divider:[%d], clk_src:[%d] is invalid\n",
				divider, dpi_clk_src);
		return count;
	}

	if (dpi_clk_src < 1000)
		dpi_clk_src *= 1000000; /* MHz */

	new_pclk = dpi_clk_src / divider;

DIRECT_GO:
	if (new_pclk == fb_dev->dpi_clock) {
		/* Do nothing */
		pr_warn("new pclk is the same as current pclk\n");
		return count;
	}

	ret = sprdfb_chg_clk_intf(fb_dev, SPRDFB_DYNAMIC_PCLK, new_pclk);
	if (ret) {
		pr_err("%s: failed to change dpi clock. ret=%d\n",
				__func__, ret);
		return ret;
	}
	attr_info->curr_pclk = new_pclk;

	return count;
}

static ssize_t sysfs_rd_current_fps(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct sprdfb_device *fb_dev = (struct sprdfb_device *)fbi->par;
	struct attr_info *attr_info = fb_dev->priv1;
	struct panel_spec* panel = fb_dev->panel;

	if (!panel) {
		pr_err("panel doesn't exist.");
		return -ENXIO;
	}
	ret = snprintf(buf, PAGE_SIZE,
			"current fps: %u\nnew fps: %u\norigin fps: %u\n",
			panel->fps, attr_info->curr_fps, attr_info->origin_fps);

	return ret;
}

static ssize_t sysfs_write_fps(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	int ret, fps;
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct sprdfb_device *fb_dev = (struct sprdfb_device *)fbi->par;
	struct attr_info *attr_info = fb_dev->priv1;
	struct panel_spec* panel = fb_dev->panel;

	if (!panel) {
		pr_err("panel doesn't exist.");
		return -ENXIO;
	}
	ret = kstrtoint(buf, 10, &fps);

	fps = fps > 0 ? fps : attr_info->origin_fps;

	if (panel->fps == fps) {
		/* Do nothing */
		pr_warn("new fps is the same as current fps\n");
		return count;
	}

	ret = sprdfb_chg_clk_intf(fb_dev, SPRDFB_DYNAMIC_FPS, (u32)fps);
	if (ret) {
		pr_err("%s: failed to change dpi clock. ret=%d\n",
				__func__, ret);
		return ret;
	}
	attr_info->curr_fps = fps;

	return count;
}

static ssize_t sysfs_rd_current_mipi_clk(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0;
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct sprdfb_device *fb_dev = (struct sprdfb_device *)fbi->par;
	struct attr_info *attr_info = fb_dev->priv1;
	struct panel_spec* panel = fb_dev->panel;

	if (!panel) {
		pr_err("panel doesn't exist.");
		return -ENXIO;
	}
	if (panel->type != LCD_MODE_DSI) {
		pr_err("Current panel is not mipi dsi\n");
		ret = snprintf(buf, PAGE_SIZE,
				"Current panel is not mipi dsi\n");
		return ret;
	}

	ret = snprintf(buf, PAGE_SIZE,
			"current mipi d-phy frequency: %u\n"
			"new mipi d-phy frequency: %u\n"
			"origin mipi d-phy frequency: %u\n",
			panel->info.mipi->phy_feq,
			attr_info->curr_mipi_clk,
			attr_info->origin_mipi_clk);
	return ret;
}

static ssize_t sysfs_write_mipi_clk(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	int ret = 0;
	u32 dphy_freq;
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct sprdfb_device *fb_dev = (struct sprdfb_device *)fbi->par;
	struct attr_info *attr_info = fb_dev->priv1;
	struct panel_spec* panel = fb_dev->panel;

	if (!panel) {
		pr_err("panel doesn't exist.");
		return -ENXIO;
	}
	if (panel->type != LCD_MODE_DSI) {
		pr_err("sys write failure. Current panel is not mipi dsi\n");
		return count;
	}
	ret = kstrtoint(buf, 10, &dphy_freq);
	if (ret) {
		pr_err("Invalid input for dphy_freq\n");
		return -EINVAL;
	}

	if (dphy_freq > 0) {
		/*
		 * because of double edge trigger,
		 * the rule is actual freq * 10 / 2,
		 * Eg: Required freq is 500M
		 * Equation: 2500*2*1000/10=500*1000=2500*200=500M
		 */
		dphy_freq *= 200;
	} else
		dphy_freq = attr_info->origin_mipi_clk;

	/* dphy supported freq ranges is 90M-1500M*/
	pr_debug("input dphy_freq is %d\n", dphy_freq);

	if (dphy_freq == attr_info->curr_mipi_clk) {
		/* Do nothing */
		pr_warn("new dphy_freq is the same as current freq\n");
		return count;
	}

	if (dphy_freq <= 1500000 && dphy_freq >= 90000) {
		ret = sprdfb_chg_clk_intf(fb_dev, SPRDFB_DYNAMIC_MIPI_CLK, dphy_freq);
		if (ret) {
			pr_err("sprdfb_chg_clk_intf change d-phy freq fail.\n");
			return count;
		}
		attr_info->curr_mipi_clk = dphy_freq;
	} else {
		pr_warn("input mipi frequency:%d is out of range.\n",
				dphy_freq);
	}

	return count;
}

int sprdfb_create_sysfs(struct sprdfb_device *fb_dev)
{
	int rc;
	struct panel_spec* panel = fb_dev->panel;
	struct attr_info *attr;

	fb_dev->priv1 = kzalloc(sizeof(struct attr_info), GFP_KERNEL);
	if (!fb_dev->priv1) {
		pr_err("shortage of memory\n");
		return -ENOMEM;
	}
	attr = fb_dev->priv1;

	rc = sysfs_create_group(&fb_dev->fb->dev->kobj, &sprdfb_attrs_group);
	if (rc)
		pr_err("sysfs group creation failed, rc=%d\n", rc);

	attr->origin_pclk = fb_dev->dpi_clock;
	if (panel) {
		attr->origin_fps = panel->fps;
		if (panel->type == LCD_MODE_DSI) {
			attr->origin_mipi_clk = panel->info.mipi->phy_feq;
			attr->curr_mipi_clk = panel->info.mipi->phy_feq;
		}
	}
	sema_init(&attr->sem, 1);

	return rc;
}

void sprdfb_remove_sysfs(struct sprdfb_device *fb_dev)
{
	struct attr_info *attr = fb_dev->priv1;
	sysfs_remove_group(&fb_dev->fb->dev->kobj, &sprdfb_attrs_group);

	if (attr)
		kfree(attr);
}
