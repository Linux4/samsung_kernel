/*
 * Copyright (C) 2018 Spreadtrum Communications Inc.
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

#include <drm/drm_crtc_helper.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/sysfs.h>
#include <linux/pm_runtime.h>
#include <linux/platform_device.h>
#include <video/videomode.h>

#include "disp_lib.h"
#include "sprd_panel.h"
#include "sprd_dsi.h"
#include "sysfs_display.h"
extern const char *lcd_name;
#define host_to_dsi(host) \
	container_of(host, struct sprd_dsi, host)


static ssize_t oled_backlight_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct sprd_panel *panel = dev_get_drvdata(dev);
	int brightness;
	int ret;
	if (strstr(lcd_name,"lcd_dummy_mipi_hd") != NULL){
		printk("lcd_name = %c", lcd_name);
		return 0;
	}
	ret = kstrtoint(buf, 10, &brightness);
	if (ret) {
		pr_err("Invalid input brightness value\n");
		return -EINVAL;
	}

	panel->oled_bdev->props.brightness = brightness;
	sprd_oled_set_brightness(panel->oled_bdev);

	return count;
}
static DEVICE_ATTR_WO(oled_backlight);

static ssize_t name_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sprd_panel *panel = dev_get_drvdata(dev);
	struct panel_info *info = &panel->info;
	int ret;

	ret = snprintf(buf, PAGE_SIZE, "%s\n", info->of_node->name);

	return ret;
}
static DEVICE_ATTR_RO(name);

static ssize_t lane_num_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sprd_panel *panel = dev_get_drvdata(dev);
	struct panel_info *info = &panel->info;
	int ret;

	ret = snprintf(buf, PAGE_SIZE, "%u\n", info->lanes);

	return ret;
}
static DEVICE_ATTR_RO(lane_num);

static ssize_t pixel_clock_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sprd_panel *panel = dev_get_drvdata(dev);
	struct panel_info *info = &panel->info;
	int ret;

	ret = snprintf(buf, PAGE_SIZE, "%u\n", info->mode.clock * 1000);

	return ret;
}
static DEVICE_ATTR_RO(pixel_clock);

static ssize_t resolution_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sprd_panel *panel = dev_get_drvdata(dev);
	struct panel_info *info = &panel->info;
	int ret;

	ret = snprintf(buf, PAGE_SIZE, "%ux%u\n", info->mode.hdisplay,
						info->mode.vdisplay);

	return ret;
}
static DEVICE_ATTR_RO(resolution);

static ssize_t screen_size_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sprd_panel *panel = dev_get_drvdata(dev);
	struct panel_info *info = &panel->info;
	int ret;

	ret = snprintf(buf, PAGE_SIZE, "%umm x %umm\n", info->mode.width_mm,
						info->mode.height_mm);

	return ret;
}
static DEVICE_ATTR_RO(screen_size);

static ssize_t hporch_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct sprd_panel *panel = dev_get_drvdata(dev);
	struct videomode vm;
	int ret;

	drm_display_mode_to_videomode(&panel->info.mode, &vm);
	ret = snprintf(buf, PAGE_SIZE, "hfp=%u hbp=%u hsync=%u\n",
				   vm.hfront_porch,
				   vm.hback_porch,
				   vm.hsync_len);

	return ret;
}

static ssize_t hporch_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct sprd_panel *panel = dev_get_drvdata(dev);
	struct drm_device *drm = panel->base.drm;
	struct drm_connector *connector = panel->base.connector;
	struct videomode vm;
	u32 val[4] = {0};
	int len;

	len = str_to_u32_array(buf, 0, val, 4);
	drm_display_mode_to_videomode(&panel->info.mode, &vm);

	switch (len) {
	/* Fall through */
	case 3:
		vm.hsync_len = val[2];
	/* Fall through */
	case 2:
		vm.hback_porch = val[1];
	/* Fall through */
	case 1:
		vm.hfront_porch = val[0];
	/* Fall through */
	default:
		break;
	}

	drm_display_mode_from_videomode(&vm, &panel->info.mode);
	mutex_lock(&drm->mode_config.mutex);
	drm_helper_probe_single_connector_modes(connector, 0, 0);
	mutex_unlock(&drm->mode_config.mutex);

	return count;
}
static DEVICE_ATTR_RW(hporch);

static ssize_t vporch_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct sprd_panel *panel = dev_get_drvdata(dev);
	struct videomode vm;
	int ret;

	drm_display_mode_to_videomode(&panel->info.mode, &vm);
	ret = snprintf(buf, PAGE_SIZE, "vfp=%u vbp=%u vsync=%u\n",
				   vm.vfront_porch,
				   vm.vback_porch,
				   vm.vsync_len);

	return ret;
}

static ssize_t vporch_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct sprd_panel *panel = dev_get_drvdata(dev);
	struct drm_device *drm = panel->base.drm;
	struct drm_connector *connector = panel->base.connector;
	struct videomode vm;
	u32 val[4] = {0};
	int len;

	len = str_to_u32_array(buf, 0, val, 4);
	drm_display_mode_to_videomode(&panel->info.mode, &vm);

	switch (len) {
	/* Fall through */
	case 3:
		vm.vsync_len = val[2];
	/* Fall through */
	case 2:
		vm.vback_porch = val[1];
	/* Fall through */
	case 1:
		vm.vfront_porch = val[0];
	/* Fall through */
	default:
		break;
	}

	drm_display_mode_from_videomode(&vm, &panel->info.mode);
	mutex_lock(&drm->mode_config.mutex);
	drm_helper_probe_single_connector_modes(connector, 0, 0);
	mutex_unlock(&drm->mode_config.mutex);

	return count;
}
static DEVICE_ATTR_RW(vporch);

static ssize_t esd_check_enable_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct sprd_panel *panel = dev_get_drvdata(dev);
	/*Tab A8 code for AX6300DEV-875 by fengzhigang at 20210926 start*/
	return snprintf(buf, PAGE_SIZE, "%u\n", panel->info.esd_conf.esd_check_en);
	/*Tab A8 code for AX6300DEV-875 by fengzhigang at 20210926 end*/
}

static ssize_t esd_check_enable_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct sprd_panel *panel = dev_get_drvdata(dev);
	struct panel_info *info = &panel->info;
	int enable;

	if (kstrtoint(buf, 10, &enable)) {
		pr_err("invalid input for esd check enable\n");
		return -EINVAL;
	}

	if (enable) {
		/*Tab A8 code for AX6300DEV-875 by fengzhigang at 20210926 start*/
		info->esd_conf.esd_check_en = true;
		if (!pm_runtime_suspended(panel->slave->host->dev) &&
		    !panel->esd_work_pending) {
			pr_info("schedule esd work\n");
			schedule_delayed_work(&panel->esd_work,
			      msecs_to_jiffies(info->esd_conf.esd_check_period));
			panel->esd_work_pending = true;
		}
	} else {
		if (panel->esd_work_pending) {
			pr_info("cancel esd work\n");
			cancel_delayed_work_sync(&panel->esd_work);
			panel->esd_work_pending = false;
		}
		info->esd_conf.esd_check_en = false;
		/*Tab A8 code for AX6300DEV-875 by fengzhigang at 20210926 end*/
	}

	return count;

}
static DEVICE_ATTR_RW(esd_check_enable);

static ssize_t esd_check_mode_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct sprd_panel *panel = dev_get_drvdata(dev);
	/*Tab A8 code for AX6300DEV-875 by fengzhigang at 20210926 start*/
	return snprintf(buf, PAGE_SIZE, "%u\n", panel->info.esd_conf.esd_check_mode);
	/*Tab A8 code for AX6300DEV-875 by fengzhigang at 20210926 end*/
}

static ssize_t esd_check_mode_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct sprd_panel *panel = dev_get_drvdata(dev);
	u32 mode;

	if (kstrtouint(buf, 10, &mode)) {
		pr_err("invalid input for esd check mode\n");
		return -EINVAL;
	}
	/*Tab A8 code for AX6300DEV-875 by fengzhigang at 20210926 start*/
	panel->info.esd_conf.esd_check_mode = mode;
	/*Tab A8 code for AX6300DEV-875 by fengzhigang at 20210926 end*/

	return count;

}
static DEVICE_ATTR_RW(esd_check_mode);

static ssize_t esd_check_period_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct sprd_panel *panel = dev_get_drvdata(dev);
	/*Tab A8 code for AX6300DEV-875 by fengzhigang at 20210926 start*/
	return snprintf(buf, PAGE_SIZE, "%u\n", panel->info.esd_conf.esd_check_period);
	/*Tab A8 code for AX6300DEV-875 by fengzhigang at 20210926 end*/
}

static ssize_t esd_check_period_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct sprd_panel *panel = dev_get_drvdata(dev);
	u32 period;

	if (kstrtouint(buf, 10, &period)) {
		pr_err("invalid input for esd check period\n");
		return -EINVAL;
	}
	/*Tab A8 code for AX6300DEV-875 by fengzhigang at 20210926 start*/
	panel->info.esd_conf.esd_check_period = period;
	/*Tab A8 code for AX6300DEV-875 by fengzhigang at 20210926 end*/

	return count;

}
static DEVICE_ATTR_RW(esd_check_period);

static ssize_t esd_check_register_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct sprd_panel *panel = dev_get_drvdata(dev);
	/*Tab A8 code for AX6300DEV-875 by fengzhigang at 20210926 start*/
	int reg_len = panel->info.esd_conf.esd_check_reg_count;
	int index = 0;
	int ret = 0;

	for (index = 0; index < reg_len; index++) {
		ret += snprintf(buf + ret, PAGE_SIZE, "esd reg%d: 0x%02x\n",
				index + 1, panel->info.esd_conf.reg_seq[index]);
	}

	return ret;
	/*Tab A8 code for AX6300DEV-875 by fengzhigang at 20210926 end*/
}

static ssize_t esd_check_register_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct sprd_panel *panel = dev_get_drvdata(dev);
	/*Tab A8 code for AX6300DEV-875 by fengzhigang at 20210926 start*/
	int reg_len = panel->info.esd_conf.esd_check_reg_count;
	uint8_t input_param[2];
	int reg_index = 0;
	int len = 0;

	len = str_to_u8_array(buf, 16, input_param, 2);
	if (len != 2) {
		pr_err("invalid input for esd check register, need 2 parameters\n");
		return -EINVAL;
	}

	reg_index = input_param[0] - 1;
	if ((reg_index > (reg_len - 1)) || (reg_index < 0)) {
		pr_err("invalid input for esd check register, index out of reg size\n");
		return -EINVAL;
	}

	panel->info.esd_conf.reg_seq[reg_index] = input_param[1];
	/*Tab A8 code for AX6300DEV-875 by fengzhigang at 20210926 end*/

	return count;

}
static DEVICE_ATTR_RW(esd_check_register);

static ssize_t esd_check_value_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct sprd_panel *panel = dev_get_drvdata(dev);
	/*Tab A8 code for AX6300DEV-875 by fengzhigang at 20210926 start*/
	int reg_len = panel->info.esd_conf.esd_check_reg_count;
	int i = 0;
	int j = 0;
	int pre_reg_len = 0;
	int ret= 0;

	pre_reg_len = 0;
	for (i = 0; i < reg_len; i++) {
		for (j = 0; j < panel->info.esd_conf.val_len_array[i]; j++) {
			ret += snprintf(buf + ret, PAGE_SIZE, "esd reg%d: 0x%02x, val%d: 0x%02x\n",
					i + 1, panel->info.esd_conf.reg_seq[i], j + 1,
					panel->info.esd_conf.val_seq[j + pre_reg_len]);
		}
		pre_reg_len += panel->info.esd_conf.val_len_array[i];
	}
	return ret;
	/*Tab A8 code for AX6300DEV-875 by fengzhigang at 20210926 end*/
}
/*Tab A8 code for AX6300DEV-875 by fengzhigang at 20210926 start*/
static ssize_t esd_check_value_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct sprd_panel *panel = dev_get_drvdata(dev);
	int reg_len = panel->info.esd_conf.esd_check_reg_count;
	uint8_t input_param[6];
	uint8_t *tmp_val_seq;
	int len = 0;
	int reg_index = 0;
	int i = 0;
	int j = 0;
	int k = 0;
	int current_index = 0;
	int val_size = 0;
	int new_total_val_count = 0;
	int pre_reg_len = 0;

	len = str_to_u8_array(buf, 16, input_param, 6);
	if ((len < 3 )|| (len > 6)) {
		pr_err("invalid input for esd check value, need 2 parameters\n");
		return -EINVAL;
	}

	reg_index = input_param[0] - 1;
	if ((reg_index > (reg_len - 1)) || (reg_index < 0)) {
		pr_err("invalid input for esd check register, index out of reg size\n");
		return -EINVAL;
	}

	val_size = input_param[1];
	if ((val_size < 1) || (val_size > 4)) {
		pr_err("invalid input for esd check value count, value count out of reg size\n");
		return -EINVAL;
	}

	if (val_size != panel->info.esd_conf.val_len_array[reg_index]) {
		new_total_val_count = (panel->info.esd_conf.total_esd_val_count + val_size -
					panel->info.esd_conf.val_len_array[reg_index]);
		tmp_val_seq = kzalloc(new_total_val_count, GFP_KERNEL);
		for (i = 0; i < reg_index; i++) {
			pre_reg_len += panel->info.esd_conf.val_len_array[i];
		}

		for (i = 0; i < pre_reg_len; i++) {
			tmp_val_seq[i] = panel->info.esd_conf.val_seq[i];
		}

		for (j = 0; j < val_size; j++) {
			tmp_val_seq[i + j] = input_param[j + 2];
		}

		current_index = i + j;
		for (k = 0; (k + current_index ) < new_total_val_count; k++) {
			tmp_val_seq[current_index + k] = panel->info.esd_conf.val_seq[i +
						panel->info.esd_conf.val_len_array[reg_index] + k];
		}

		panel->info.esd_conf.val_len_array[reg_index] = val_size;
		panel->info.esd_conf.total_esd_val_count = new_total_val_count;
		kfree(panel->info.esd_conf.val_seq);
		panel->info.esd_conf.val_seq = tmp_val_seq;
	} else {
		for (i = 0; i < reg_index; i++) {
			pre_reg_len = panel->info.esd_conf.val_len_array[i];
		}

		for (j = 0; j < val_size; j++) {
			panel->info.esd_conf.val_seq[j + pre_reg_len] = input_param[j + 2];
		}
	}

	for (i = 0; i < reg_index; i++) {
		pre_reg_len += panel->info.esd_conf.val_len_array[i];
	}
	return count;

}
static DEVICE_ATTR_RW(esd_check_value);
/*Tab A8 code for AX6300DEV-875 by fengzhigang at 20210926 end*/
static ssize_t suspend_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct sprd_panel *panel = dev_get_drvdata(dev);
	//struct mipi_dsi_host *host = panel->slave->host;
	//struct sprd_dsi *dsi = host_to_dsi(host);

	//if (dsi->ctx.is_inited && panel->is_enabled) {
		drm_panel_disable(&panel->base);
		drm_panel_unprepare(&panel->base);
		//panel->is_enabled = false;
	//}

	return count;
}
static DEVICE_ATTR_WO(suspend);

static ssize_t resume_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct sprd_panel *panel = dev_get_drvdata(dev);
	//struct mipi_dsi_host *host = panel->slave->host;
	//struct sprd_dsi *dsi = host_to_dsi(host);

	//if (dsi->ctx.is_inited && (!panel->is_enabled)) {
		drm_panel_prepare(&panel->base);
		drm_panel_enable(&panel->base);
		//panel->is_enabled = true;
	//}

	return count;
}
static DEVICE_ATTR_WO(resume);

static ssize_t load_lcddtb_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct sprd_panel *panel = dev_get_drvdata(dev);
	struct device_node *root = NULL;
	struct device_node *lcdnp = NULL;
	int enable;
	static void *blob;
	int ret;

	if (kstrtoint(buf, 10, &enable)) {
		pr_err("invalid input for panelinfo enable\n");
		return -EINVAL;
	}

	if (enable > 0) {
		if (blob) {
			kfree(blob);
			blob = NULL;
		}
		ret = load_dtb_to_mem("/data/lcd.dtb", &blob);
		if (ret < 0) {
			pr_err("parse lcd dtb file failed\n");
			return -EINVAL;
		}
		of_fdt_unflatten_tree(blob, NULL, &root);
		lcdnp = root->child->child;
		if (lcdnp) {
			pr_err("lcd device node name %s\n", lcdnp->full_name);
			sprd_panel_parse_lcddtb(lcdnp, panel);
		}
	}

	return count;

}
static DEVICE_ATTR_WO(load_lcddtb);

static struct attribute *panel_attrs[] = {
	&dev_attr_name.attr,
	&dev_attr_lane_num.attr,
	&dev_attr_resolution.attr,
	&dev_attr_screen_size.attr,
	&dev_attr_pixel_clock.attr,
	&dev_attr_hporch.attr,
	&dev_attr_vporch.attr,
	&dev_attr_esd_check_enable.attr,
	&dev_attr_esd_check_mode.attr,
	&dev_attr_esd_check_period.attr,
	&dev_attr_esd_check_register.attr,
	&dev_attr_esd_check_value.attr,
	&dev_attr_suspend.attr,
	&dev_attr_resume.attr,
	&dev_attr_oled_backlight.attr,
	&dev_attr_load_lcddtb.attr,
	NULL,
};
ATTRIBUTE_GROUPS(panel);

int sprd_panel_sysfs_init(struct device *dev)
{
	int rc;

	rc = sysfs_create_groups(&(dev->kobj), panel_groups);
	if (rc)
		pr_err("create panel attr node failed, rc=%d\n", rc);

	return rc;
}
EXPORT_SYMBOL(sprd_panel_sysfs_init);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("leon.he@spreadtrum.com");
MODULE_DESCRIPTION("Add panel attribute nodes for userspace");
