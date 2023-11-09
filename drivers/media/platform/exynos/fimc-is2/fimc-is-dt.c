/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is core functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/sched.h>
#include <mach/exynos-fimc-is2-module.h>
#include <mach/exynos-fimc-is2-sensor.h>
#include <mach/exynos-fimc-is2.h>
#include <media/exynos_mc.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_gpio.h>
#endif

#include "fimc-is-config.h"
#include "fimc-is-dt.h"
#include "fimc-is-core.h"
#include "fimc-is-dvfs.h"
#include "fimc-is-sysfs.h"

#ifdef CONFIG_CAMERA_USE_SOC_SENSOR
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#include <mach/map.h>
#include <mach/regs-clock.h>
#include <media/v4l2-subdev.h>
#include <plat/map-s5p.h>
#include <plat/cpu.h>
#endif

#if defined(CONFIG_CAMERA_OTPROM_SUPPORT_FRONT)
int front_otprom_reg;
#endif

#ifdef CONFIG_OF
static int get_pin_lookup_state(struct pinctrl *pinctrl,
	struct exynos_sensor_pin (*pin_ctrls)[GPIO_SCENARIO_MAX][GPIO_CTRL_MAX])
{
	int ret = 0;
	u32 i, j, k;
	char pin_name[30];
	struct pinctrl_state *s;

	for (i = 0; i < SENSOR_SCENARIO_MAX; ++i) {
		for (j = 0; j < GPIO_SCENARIO_MAX; ++j) {
			for (k = 0; k < GPIO_CTRL_MAX; ++k) {
				if (pin_ctrls[i][j][k].act == PIN_FUNCTION) {
					snprintf(pin_name, sizeof(pin_name), "%s%d",
						pin_ctrls[i][j][k].name,
						pin_ctrls[i][j][k].value);
					s = pinctrl_lookup_state(pinctrl, pin_name);
					if (IS_ERR_OR_NULL(s)) {
						err("pinctrl_lookup_state(%s) is failed", pin_name);
						ret = -EINVAL;
						goto p_err;
					}

					pin_ctrls[i][j][k].pin = (ulong)s;
				}
			}
		}
	}

p_err:
	return ret;
}

static int parse_gate_info(struct exynos_platform_fimc_is *pdata, struct device_node *np)
{
	int ret = 0;
	struct device_node *group_np = NULL;
	struct device_node *gate_info_np;
	struct property *prop;
	struct property *prop2;
	const __be32 *p;
	const char *s;
	u32 i = 0, u = 0;
	struct exynos_fimc_is_clk_gate_info *gate_info;

	/* get subip of fimc-is info */
	gate_info = kzalloc(sizeof(struct exynos_fimc_is_clk_gate_info), GFP_KERNEL);
	if (!gate_info) {
		printk(KERN_ERR "%s: no memory for fimc_is gate_info\n", __func__);
		return -EINVAL;
	}

	s = NULL;
	/* get gate register info */
	prop2 = of_find_property(np, "clk_gate_strs", NULL);
	of_property_for_each_u32(np, "clk_gate_enums", prop, p, u) {
		printk(KERN_INFO "int value: %d\n", u);
		s = of_prop_next_string(prop2, s);
		if (s != NULL) {
			printk(KERN_INFO "String value: %d-%s\n", u, s);
			gate_info->gate_str[u] = s;
		}
	}

	/* gate info */
	gate_info_np = of_find_node_by_name(np, "clk_gate_ctrl");
	if (!gate_info_np) {
		printk(KERN_ERR "%s: can't find fimc_is clk_gate_ctrl node\n", __func__);
		ret = -ENOENT;
		goto p_err;
	}
	i = 0;
	while ((group_np = of_get_next_child(gate_info_np, group_np))) {
		struct exynos_fimc_is_clk_gate_group *group =
				&gate_info->groups[i];
		of_property_for_each_u32(group_np, "mask_clk_on_org", prop, p, u) {
			printk(KERN_INFO "(%d) int1 value: %d\n", i, u);
			group->mask_clk_on_org |= (1 << u);
		}
		of_property_for_each_u32(group_np, "mask_clk_off_self_org", prop, p, u) {
			printk(KERN_INFO "(%d) int2 value: %d\n", i, u);
			group->mask_clk_off_self_org |= (1 << u);
		}
		of_property_for_each_u32(group_np, "mask_clk_off_depend", prop, p, u) {
			printk(KERN_INFO "(%d) int3 value: %d\n", i, u);
			group->mask_clk_off_depend |= (1 << u);
		}
		of_property_for_each_u32(group_np, "mask_cond_for_depend", prop, p, u) {
			printk(KERN_INFO "(%d) int4 value: %d\n", i, u);
			group->mask_cond_for_depend |= (1 << u);
		}
		i++;
		printk(KERN_INFO "(%d) [0x%x , 0x%x, 0x%x, 0x%x\n", i,
			group->mask_clk_on_org,
			group->mask_clk_off_self_org,
			group->mask_clk_off_depend,
			group->mask_cond_for_depend
		);
	}

	pdata->gate_info = gate_info;
	pdata->gate_info->clk_on_off = exynos_fimc_is_clk_gate;

	return 0;
p_err:
	kfree(gate_info);
	return ret;
}

static int parse_dvfs_data(struct exynos_platform_fimc_is *pdata, struct device_node *np, int index)
{
	u32 temp;
	char *pprop;

	if (index < 0 || index >= FIMC_IS_DVFS_TABLE_IDX_MAX) {
		err("dvfs index(%d) is invalid", index);
		return -EINVAL;
	}

	DT_READ_U32(np, "default_int", pdata->dvfs_data[index][FIMC_IS_SN_DEFAULT][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "default_cam", pdata->dvfs_data[index][FIMC_IS_SN_DEFAULT][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "default_mif", pdata->dvfs_data[index][FIMC_IS_SN_DEFAULT][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "default_i2c", pdata->dvfs_data[index][FIMC_IS_SN_DEFAULT][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "front_preview_int", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_PREVIEW][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "front_preview_cam", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_PREVIEW][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "front_preview_mif", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_PREVIEW][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "front_preview_i2c", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_PREVIEW][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "front_preview_binning_int", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_PREVIEW_BINNING][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "front_preview_binning_cam", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_PREVIEW_BINNING][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "front_preview_binning_mif", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_PREVIEW_BINNING][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "front_preview_binning_i2c", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_PREVIEW_BINNING][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "front_capture_int", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_CAPTURE][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "front_capture_cam", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_CAPTURE][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "front_capture_mif", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_CAPTURE][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "front_capture_i2c", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_CAPTURE][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "front_video_int", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_CAMCORDING][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "front_video_cam", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_CAMCORDING][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "front_video_mif", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_CAMCORDING][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "front_video_i2c", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_CAMCORDING][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "front_video_whd_int", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_CAMCORDING_WHD][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "front_video_whd_cam", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_CAMCORDING_WHD][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "front_video_whd_mif", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_CAMCORDING_WHD][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "front_video_whd_i2c", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_CAMCORDING_WHD][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "front_video_capture_int", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_CAMCORDING_CAPTURE][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "front_video_capture_cam", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_CAMCORDING_CAPTURE][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "front_video_capture_mif", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_CAMCORDING_CAPTURE][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "front_video_capture_i2c", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_CAMCORDING_CAPTURE][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "front_video_whd_capture_int", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_CAMCORDING_WHD_CAPTURE][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "front_video_whd_capture_cam", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_CAMCORDING_WHD_CAPTURE][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "front_video_whd_capture_mif", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_CAMCORDING_WHD_CAPTURE][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "front_video_whd_capture_i2c", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_CAMCORDING_WHD_CAPTURE][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "front_vt1_int", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_VT1][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "front_vt1_cam", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_VT1][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "front_vt1_mif", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_VT1][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "front_vt1_i2c", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_VT1][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "front_vt2_int", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_VT2][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "front_vt2_cam", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_VT2][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "front_vt2_mif", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_VT2][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "front_vt2_i2c", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_VT2][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "front_companion_preview_int", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_COMPANION_PREVIEW][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "front_companion_preview_cam", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_COMPANION_PREVIEW][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "front_companion_preview_mif", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_COMPANION_PREVIEW][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "front_companion_preview_i2c", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_COMPANION_PREVIEW][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "front_companion_capture_int", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_COMPANION_CAPTURE][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "front_companion_capture_cam", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_COMPANION_CAPTURE][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "front_companion_capture_mif", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_COMPANION_CAPTURE][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "front_companion_capture_i2c", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_COMPANION_CAPTURE][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "front_companion_video_int", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_COMPANION_CAMCORDING][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "front_companion_video_cam", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_COMPANION_CAMCORDING][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "front_companion_video_mif", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_COMPANION_CAMCORDING][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "front_companion_video_i2c", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_COMPANION_CAMCORDING][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "front_companion_video_whd_int", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_COMPANION_CAMCORDING_WHD][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "front_companion_video_whd_cam", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_COMPANION_CAMCORDING_WHD][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "front_companion_video_whd_mif", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_COMPANION_CAMCORDING_WHD][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "front_companion_video_whd_i2c", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_COMPANION_CAMCORDING_WHD][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "front_companion_video_capture_int", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_COMPANION_CAMCORDING_CAPTURE][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "front_companion_video_capture_cam", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_COMPANION_CAMCORDING_CAPTURE][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "front_companion_video_capture_mif", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_COMPANION_CAMCORDING_CAPTURE][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "front_companion_video_capture_i2c", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_COMPANION_CAMCORDING_CAPTURE][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "front_companion_video_whd_capture_int", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_COMPANION_CAMCORDING_WHD_CAPTURE][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "front_companion_video_whd_capture_cam", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_COMPANION_CAMCORDING_WHD_CAPTURE][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "front_companion_video_whd_capture_mif", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_COMPANION_CAMCORDING_WHD_CAPTURE][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "front_companion_video_whd_capture_i2c", pdata->dvfs_data[index][FIMC_IS_SN_FRONT_COMPANION_CAMCORDING_WHD_CAPTURE][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "rear_preview_fhd_int", pdata->dvfs_data[index][FIMC_IS_SN_REAR_PREVIEW_FHD][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "rear_preview_fhd_cam", pdata->dvfs_data[index][FIMC_IS_SN_REAR_PREVIEW_FHD][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "rear_preview_fhd_mif", pdata->dvfs_data[index][FIMC_IS_SN_REAR_PREVIEW_FHD][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "rear_preview_fhd_i2c", pdata->dvfs_data[index][FIMC_IS_SN_REAR_PREVIEW_FHD][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "rear_preview_whd_int", pdata->dvfs_data[index][FIMC_IS_SN_REAR_PREVIEW_WHD][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "rear_preview_whd_cam", pdata->dvfs_data[index][FIMC_IS_SN_REAR_PREVIEW_WHD][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "rear_preview_whd_mif", pdata->dvfs_data[index][FIMC_IS_SN_REAR_PREVIEW_WHD][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "rear_preview_whd_i2c", pdata->dvfs_data[index][FIMC_IS_SN_REAR_PREVIEW_WHD][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "rear_preview_uhd_int", pdata->dvfs_data[index][FIMC_IS_SN_REAR_PREVIEW_UHD][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "rear_preview_uhd_cam", pdata->dvfs_data[index][FIMC_IS_SN_REAR_PREVIEW_UHD][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "rear_preview_uhd_mif", pdata->dvfs_data[index][FIMC_IS_SN_REAR_PREVIEW_UHD][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "rear_preview_uhd_i2c", pdata->dvfs_data[index][FIMC_IS_SN_REAR_PREVIEW_UHD][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "rear_preview_binning_int", pdata->dvfs_data[index][FIMC_IS_SN_REAR_PREVIEW_BINNING][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "rear_preview_binning_cam", pdata->dvfs_data[index][FIMC_IS_SN_REAR_PREVIEW_BINNING][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "rear_preview_binning_mif", pdata->dvfs_data[index][FIMC_IS_SN_REAR_PREVIEW_BINNING][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "rear_preview_binning_i2c", pdata->dvfs_data[index][FIMC_IS_SN_REAR_PREVIEW_BINNING][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "rear_capture_int", pdata->dvfs_data[index][FIMC_IS_SN_REAR_CAPTURE][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "rear_capture_cam", pdata->dvfs_data[index][FIMC_IS_SN_REAR_CAPTURE][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "rear_capture_mif", pdata->dvfs_data[index][FIMC_IS_SN_REAR_CAPTURE][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "rear_capture_i2c", pdata->dvfs_data[index][FIMC_IS_SN_REAR_CAPTURE][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "rear_video_fhd_int", pdata->dvfs_data[index][FIMC_IS_SN_REAR_CAMCORDING_FHD][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "rear_video_fhd_cam", pdata->dvfs_data[index][FIMC_IS_SN_REAR_CAMCORDING_FHD][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "rear_video_fhd_mif", pdata->dvfs_data[index][FIMC_IS_SN_REAR_CAMCORDING_FHD][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "rear_video_fhd_i2c", pdata->dvfs_data[index][FIMC_IS_SN_REAR_CAMCORDING_FHD][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "rear_video_uhd_int", pdata->dvfs_data[index][FIMC_IS_SN_REAR_CAMCORDING_UHD][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "rear_video_uhd_cam", pdata->dvfs_data[index][FIMC_IS_SN_REAR_CAMCORDING_UHD][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "rear_video_uhd_mif", pdata->dvfs_data[index][FIMC_IS_SN_REAR_CAMCORDING_UHD][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "rear_video_uhd_i2c", pdata->dvfs_data[index][FIMC_IS_SN_REAR_CAMCORDING_UHD][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "rear_video_fhd_capture_int", pdata->dvfs_data[index][FIMC_IS_SN_REAR_CAMCORDING_FHD_CAPTURE][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "rear_video_fhd_capture_cam", pdata->dvfs_data[index][FIMC_IS_SN_REAR_CAMCORDING_FHD_CAPTURE][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "rear_video_fhd_capture_mif", pdata->dvfs_data[index][FIMC_IS_SN_REAR_CAMCORDING_FHD_CAPTURE][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "rear_video_fhd_capture_i2c", pdata->dvfs_data[index][FIMC_IS_SN_REAR_CAMCORDING_FHD_CAPTURE][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "rear_video_uhd_capture_int", pdata->dvfs_data[index][FIMC_IS_SN_REAR_CAMCORDING_UHD_CAPTURE][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "rear_video_uhd_capture_cam", pdata->dvfs_data[index][FIMC_IS_SN_REAR_CAMCORDING_UHD_CAPTURE][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "rear_video_uhd_capture_mif", pdata->dvfs_data[index][FIMC_IS_SN_REAR_CAMCORDING_UHD_CAPTURE][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "rear_video_uhd_capture_i2c", pdata->dvfs_data[index][FIMC_IS_SN_REAR_CAMCORDING_UHD_CAPTURE][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "dual_preview_int", pdata->dvfs_data[index][FIMC_IS_SN_DUAL_PREVIEW][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "dual_preview_cam", pdata->dvfs_data[index][FIMC_IS_SN_DUAL_PREVIEW][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "dual_preview_mif", pdata->dvfs_data[index][FIMC_IS_SN_DUAL_PREVIEW][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "dual_preview_i2c", pdata->dvfs_data[index][FIMC_IS_SN_DUAL_PREVIEW][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "dual_capture_int", pdata->dvfs_data[index][FIMC_IS_SN_DUAL_CAPTURE][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "dual_capture_cam", pdata->dvfs_data[index][FIMC_IS_SN_DUAL_CAPTURE][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "dual_capture_mif", pdata->dvfs_data[index][FIMC_IS_SN_DUAL_CAPTURE][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "dual_capture_i2c", pdata->dvfs_data[index][FIMC_IS_SN_DUAL_CAPTURE][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "dual_video_int", pdata->dvfs_data[index][FIMC_IS_SN_DUAL_CAMCORDING][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "dual_video_cam", pdata->dvfs_data[index][FIMC_IS_SN_DUAL_CAMCORDING][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "dual_video_mif", pdata->dvfs_data[index][FIMC_IS_SN_DUAL_CAMCORDING][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "dual_video_i2c", pdata->dvfs_data[index][FIMC_IS_SN_DUAL_CAMCORDING][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "dual_video_capture_int", pdata->dvfs_data[index][FIMC_IS_SN_DUAL_CAMCORDING_CAPTURE][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "dual_video_capture_cam", pdata->dvfs_data[index][FIMC_IS_SN_DUAL_CAMCORDING_CAPTURE][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "dual_video_capture_mif", pdata->dvfs_data[index][FIMC_IS_SN_DUAL_CAMCORDING_CAPTURE][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "dual_video_capture_i2c", pdata->dvfs_data[index][FIMC_IS_SN_DUAL_CAMCORDING_CAPTURE][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "preivew_high_speed_fps_int", pdata->dvfs_data[index][FIMC_IS_SN_PREVIEW_HIGH_SPEED_FPS][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "preivew_high_speed_fps_cam", pdata->dvfs_data[index][FIMC_IS_SN_PREVIEW_HIGH_SPEED_FPS][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "preivew_high_speed_fps_mif", pdata->dvfs_data[index][FIMC_IS_SN_PREVIEW_HIGH_SPEED_FPS][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "preivew_high_speed_fps_i2c", pdata->dvfs_data[index][FIMC_IS_SN_PREVIEW_HIGH_SPEED_FPS][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "video_high_speed_60fps_int", pdata->dvfs_data[index][FIMC_IS_SN_VIDEO_HIGH_SPEED_60FPS][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "video_high_speed_60fps_cam", pdata->dvfs_data[index][FIMC_IS_SN_VIDEO_HIGH_SPEED_60FPS][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "video_high_speed_60fps_mif", pdata->dvfs_data[index][FIMC_IS_SN_VIDEO_HIGH_SPEED_60FPS][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "video_high_speed_60fps_i2c", pdata->dvfs_data[index][FIMC_IS_SN_VIDEO_HIGH_SPEED_60FPS][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "video_high_speed_120fps_int", pdata->dvfs_data[index][FIMC_IS_SN_VIDEO_HIGH_SPEED_120FPS][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "video_high_speed_120fps_cam", pdata->dvfs_data[index][FIMC_IS_SN_VIDEO_HIGH_SPEED_120FPS][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "video_high_speed_120fps_mif", pdata->dvfs_data[index][FIMC_IS_SN_VIDEO_HIGH_SPEED_120FPS][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "video_high_speed_120fps_i2c", pdata->dvfs_data[index][FIMC_IS_SN_VIDEO_HIGH_SPEED_120FPS][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "video_high_speed_240fps_int", pdata->dvfs_data[index][FIMC_IS_SN_VIDEO_HIGH_SPEED_240FPS][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "video_high_speed_240fps_cam", pdata->dvfs_data[index][FIMC_IS_SN_VIDEO_HIGH_SPEED_240FPS][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "video_high_speed_240fps_mif", pdata->dvfs_data[index][FIMC_IS_SN_VIDEO_HIGH_SPEED_240FPS][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "video_high_speed_240fps_i2c", pdata->dvfs_data[index][FIMC_IS_SN_VIDEO_HIGH_SPEED_240FPS][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "max_int", pdata->dvfs_data[index][FIMC_IS_SN_MAX][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "max_cam", pdata->dvfs_data[index][FIMC_IS_SN_MAX][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "max_mif", pdata->dvfs_data[index][FIMC_IS_SN_MAX][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "max_i2c", pdata->dvfs_data[index][FIMC_IS_SN_MAX][FIMC_IS_DVFS_I2C]);

	return 0;
}

#ifdef CAMERA_SYSFS_V2
static int parse_sysfs_caminfo(struct exynos_platform_fimc_is *pdata, struct device_node *np,
	struct fimc_is_cam_info *cam_infos, int camera_num)
{
	u32 temp;
	char *pprop;

	DT_READ_U32(np, "isp", cam_infos[camera_num].isp);
	DT_READ_U32(np, "cal_memory", cam_infos[camera_num].cal_memory);
	DT_READ_U32(np, "read_version", cam_infos[camera_num].read_version);
	DT_READ_U32(np, "core_voltage", cam_infos[camera_num].core_voltage);
	DT_READ_U32(np, "upgrade", cam_infos[camera_num].upgrade);
	DT_READ_U32(np, "companion", cam_infos[camera_num].companion);
	DT_READ_U32(np, "ois", cam_infos[camera_num].ois);

	return 0;
}
#endif

int fimc_is_parse_dt(struct platform_device *pdev)
{
	int ret = 0;
	struct exynos_platform_fimc_is *pdata;
	struct device *dev;
	struct device_node *dvfs_np = NULL;
	struct device_node *dvfs_table_np = NULL;
	struct device_node *np;
	const char *dvfs_table_desc;
	int i = 0;
#ifdef CAMERA_SYSFS_V2
	struct device_node *camInfo_np;
	struct fimc_is_cam_info *camera_infos;
	char camInfo_string[15];
	int camera_num;
	int total_camera_num;
#endif

	BUG_ON(!pdev);

	dev = &pdev->dev;
	np = dev->of_node;

	pdata = kzalloc(sizeof(struct exynos_platform_fimc_is), GFP_KERNEL);
	if (!pdata) {
		err("no memory for platform data");
		return -ENOMEM;
	}

	pdata->clk_cfg = exynos_fimc_is_cfg_clk;
	pdata->clk_on = exynos_fimc_is_clk_on;
	pdata->clk_off = exynos_fimc_is_clk_off;
	pdata->print_clk = exynos_fimc_is_print_clk;

	if (parse_gate_info(pdata, np) < 0)
		err("can't parse clock gate info node");

	ret = of_property_read_u32(np, "rear_sensor_id", &pdata->rear_sensor_id);
	if (ret) {
		err("rear_sensor_id read is fail(%d)", ret);
	}

	ret = of_property_read_u32(np, "front_sensor_id", &pdata->front_sensor_id);
	if (ret) {
		err("front_sensor_id read is fail(%d)", ret);
	}

	pdata->check_sensor_vendor = of_property_read_bool(np, "check_sensor_vendor");
	if (!pdata->check_sensor_vendor) {
		pr_info("check_sensor_vendor not use(%d)", pdata->check_sensor_vendor);
	}

#ifdef CONFIG_OIS_USE
	pdata->use_ois = of_property_read_bool(np, "use_ois");
	if (!pdata->use_ois) {
		err("use_ois not use(%d)", pdata->use_ois);
	}
#endif /* CONFIG_OIS_USE */

	pdata->use_ois_hsi2c = of_property_read_bool(np, "use_ois_hsi2c");
	if (!pdata->use_ois_hsi2c) {
		err("use_ois_hsi2c not use(%d)", pdata->use_ois_hsi2c);
	}

	pdata->use_module_check = of_property_read_bool(np, "use_module_check");
	if (!pdata->use_module_check) {
		err("use_module_check not use(%d)", pdata->use_module_check);
	}

	pdata->skip_cal_loading = of_property_read_bool(np, "skip_cal_loading");
	if (!pdata->skip_cal_loading) {
		pr_info("skip_cal_loading not use(%d)", pdata->skip_cal_loading);
	}

	i = 0;
	dvfs_np = of_find_node_by_name(np, "fimc_is_dvfs");
	if (!dvfs_np) {
		err("can't find fimc_is_dvfs node");
		ret = -ENOENT;
		goto p_err;
	}

	/* Maybe there are two version of DVFS table for 1.0 and 3.2 HAL */
	ret = of_property_read_u32(dvfs_np,
			GET_KEY_FOR_DVFS_TBL_IDX(IS_HAL_VER_1_0),
			&pdata->dvfs_hal_1_0_table_idx);
	if (ret) {
		probe_info("dvfs_hal_1_0_table idx was not existed(%d)", ret);
		pdata->dvfs_hal_1_0_table_idx = 0;
	}

	ret = of_property_read_u32(dvfs_np,
			GET_KEY_FOR_DVFS_TBL_IDX(IS_HAL_VER_3_2),
			&pdata->dvfs_hal_3_2_table_idx);
	if (ret) {
		probe_info("dvfs_hal_3_2_table idx was not existed(%d)", ret);
		pdata->dvfs_hal_3_2_table_idx = 0;
	}

	while ((dvfs_table_np = of_get_next_child(dvfs_np, dvfs_table_np))) {
		ret = of_property_read_string(dvfs_table_np, "desc", &dvfs_table_desc);
		if (ret)
			dvfs_table_desc = "Not Defined";
		probe_info("dvfs table[%d] %s", i, dvfs_table_desc);
		dvfs_table_desc = NULL;
		ret = parse_dvfs_data(pdata, dvfs_table_np, i);
		if (ret)
			err("parse_dvfs_data is fail(%d, idx:%d)", ret, i);
		i++;
	}
	/* if i is 0, dvfs table is only one */
	if (i == 0) {
		pdata->dvfs_hal_1_0_table_idx = 0;
		pdata->dvfs_hal_3_2_table_idx = 0;
		probe_info("dvfs table is only one");
		ret = parse_dvfs_data(pdata, dvfs_np, 0);
		if (ret)
			err("parse_dvfs_data is fail(%d)", ret);
	}

#ifdef CAMERA_SYSFS_V2
	ret = of_property_read_u32(np, "total_camera_num", &total_camera_num);
	if (ret) {
		err("total_camera_num read is fail(%d)", ret);
		total_camera_num = 0;
	}
	fimc_is_get_cam_info(&camera_infos);

	for (camera_num = 0; camera_num < total_camera_num; camera_num++) {
		sprintf(camInfo_string, "%s%d", "camera_info", camera_num);

		camInfo_np = of_find_node_by_name(np, camInfo_string);
		if (!camInfo_np) {
			printk(KERN_ERR "%s: can't find camInfo_string node\n", __func__);
			ret = -ENOENT;
			goto p_err;
		}
		parse_sysfs_caminfo(pdata, camInfo_np, camera_infos, camera_num);
	}
#endif

#if defined(CONFIG_CAMERA_OTPROM_SUPPORT_FRONT)
	ret = of_property_read_u32(np, "front_otprom_reg", &front_otprom_reg);
	if (ret) {
		err("front_otprom_reg read is fail(%d)", ret);
		front_otprom_reg = 0;
	}
	info("%s front_otprom_reg(0x%x)", __func__, front_otprom_reg);
#endif

	dev->platform_data = pdata;

	return 0;

p_err:
	kfree(pdata);
	return ret;
}

int fimc_is_sensor_parse_dt(struct platform_device *pdev)
{
	int ret = 0;
	struct exynos_platform_fimc_is_sensor *pdata;
	struct device_node *dnode;
	struct device *dev;

	BUG_ON(!pdev);
	BUG_ON(!pdev->dev.of_node);

	dev = &pdev->dev;
	dnode = dev->of_node;

	pdata = kzalloc(sizeof(struct exynos_platform_fimc_is_sensor), GFP_KERNEL);
	if (!pdata) {
		err("%s: no memory for platform data", __func__);
		return -ENOMEM;
	}

	pdata->iclk_get = exynos_fimc_is_sensor_iclk_get;
	pdata->iclk_cfg = exynos_fimc_is_sensor_iclk_cfg;
	pdata->iclk_on = exynos_fimc_is_sensor_iclk_on;
	pdata->iclk_off = exynos_fimc_is_sensor_iclk_off;
	pdata->mclk_on = exynos_fimc_is_sensor_mclk_on;
	pdata->mclk_off = exynos_fimc_is_sensor_mclk_off;

	ret = of_property_read_u32(dnode, "id", &pdata->id);
	if (ret) {
		err("scenario read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "scenario", &pdata->scenario);
	if (ret) {
		err("scenario read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "csi_ch", &pdata->csi_ch);
	if (ret) {
		err("csi_ch read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "flite_ch", &pdata->flite_ch);
	if (ret) {
		err("flite_ch read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "is_bns", &pdata->is_bns);
	if (ret) {
		err("is_bns read is fail(%d)", ret);
		goto p_err;
	}

	pdev->id = pdata->id;
	dev->platform_data = pdata;

	return ret;

p_err:
	kfree(pdata);
	return ret;
}

static int parse_af_data(struct exynos_platform_fimc_is_module *pdata, struct device_node *dnode)
{
	u32 temp;
	char *pprop;

	DT_READ_U32(dnode, "product_name", pdata->af_product_name);
	DT_READ_U32(dnode, "i2c_addr", pdata->af_i2c_addr);
	DT_READ_U32(dnode, "i2c_ch", pdata->af_i2c_ch);

	return 0;
}

static int parse_flash_data(struct exynos_platform_fimc_is_module *pdata, struct device_node *dnode)
{
	u32 temp;
	char *pprop;

	DT_READ_U32(dnode, "product_name", pdata->flash_product_name);
	DT_READ_U32(dnode, "flash_first_gpio", pdata->flash_first_gpio);
	DT_READ_U32(dnode, "flash_second_gpio", pdata->flash_second_gpio);

	return 0;
}

static int parse_companion_data(struct exynos_platform_fimc_is_module *pdata, struct device_node *dnode)
{
	u32 temp;
	char *pprop;

	DT_READ_U32(dnode, "product_name", pdata->companion_product_name);
	DT_READ_U32(dnode, "spi_channel", pdata->companion_spi_channel);
	DT_READ_U32(dnode, "i2c_addr", pdata->companion_i2c_addr);
	DT_READ_U32(dnode, "i2c_ch", pdata->companion_i2c_ch);

	return 0;
}

static int parse_ois_data(struct exynos_platform_fimc_is_module *pdata, struct device_node *dnode)
{
	u32 temp;
	char *pprop;

	DT_READ_U32(dnode, "product_name", pdata->ois_product_name);
	DT_READ_U32(dnode, "i2c_addr", pdata->ois_i2c_addr);
	DT_READ_U32(dnode, "i2c_ch", pdata->ois_i2c_ch);

	return 0;
}

int fimc_is_sensor_module_parse_dt(struct platform_device *pdev,
	fimc_is_moudle_dt_callback module_callback)
{
	int ret = 0;
	struct exynos_platform_fimc_is_module *pdata;
	struct device_node *dnode;
	struct device_node *af_np;
	struct device_node *flash_np;
	struct device_node *companion_np;
	struct device_node *ois_np;
	struct device *dev;

	BUG_ON(!pdev);
	BUG_ON(!pdev->dev.of_node);
	BUG_ON(!module_callback);

	dev = &pdev->dev;
	dnode = dev->of_node;

	pdata = kzalloc(sizeof(struct exynos_platform_fimc_is_module), GFP_KERNEL);
	if (!pdata) {
		pr_err("%s: no memory for platform data\n", __func__);
		return -ENOMEM;
	}

	pdata->gpio_cfg = exynos_fimc_is_module_pins_cfg;
	pdata->gpio_dbg = exynos_fimc_is_module_pins_dbg;
#ifdef CONFIG_CAMERA_USE_SOC_SENSOR
	pdata->gpio_soc_cfg = NULL;
	pdata->gpio_soc_dbg = NULL;
#endif
	ret = of_property_read_u32(dnode, "id", &pdata->id);
	if (ret) {
		err("id read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "mclk_ch", &pdata->mclk_ch);
	if (ret) {
		err("mclk_ch read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "sensor_i2c_ch", &pdata->sensor_i2c_ch);
	if (ret) {
		err("i2c_ch read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "sensor_i2c_addr", &pdata->sensor_i2c_addr);
	if (ret) {
		err("i2c_addr read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "position", &pdata->position);
	if (ret) {
		err("id read is fail(%d)", ret);
		goto p_err;
	}

	af_np = of_find_node_by_name(dnode, "af");
	if (!af_np) {
		pdata->af_product_name = ACTUATOR_NAME_NOTHING;
	} else {
		parse_af_data(pdata, af_np);
	}

	flash_np = of_find_node_by_name(dnode, "flash");
	if (!flash_np) {
		pdata->flash_product_name = FLADRV_NAME_NOTHING;
	} else {
		parse_flash_data(pdata, flash_np);
	}

	companion_np = of_find_node_by_name(dnode, "companion");
	if (!companion_np) {
		pdata->companion_product_name = COMPANION_NAME_NOTHING;
	} else {
		parse_companion_data(pdata, companion_np);
	}

	ois_np = of_find_node_by_name(dnode, "ois");
	if (!ois_np) {
		pdata->ois_product_name = OIS_NAME_NOTHING;
	} else {
		parse_ois_data(pdata, ois_np);
	}

	ret = module_callback(pdev, pdata);
	if (ret) {
		err("sensor dt callback is fail(%d)", ret);
		goto p_err;
	}

	pdata->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(pdata->pinctrl)) {
		err("devm_pinctrl_get is fail");
		goto p_err;
	}

	ret = get_pin_lookup_state(pdata->pinctrl, pdata->pin_ctrls);
	if (ret) {
		err("get_pin_lookup_state is fail(%d)", ret);
		goto p_err;
	}

	dev->platform_data = pdata;

	return ret;

p_err:
	kfree(pdata);
	return ret;
}

#ifdef CONFIG_CAMERA_USE_SOC_SENSOR
static int exynos_fimc_is_module_soc_pin_control(struct i2c_client *client,
	struct pinctrl *pinctrl, struct exynos_sensor_pin *pin_ctrls)
{
	char* name = pin_ctrls->name;
	ulong pin = pin_ctrls->pin;
	u32 delay = pin_ctrls->delay;
	u32 value = pin_ctrls->value;
	u32 voltage = pin_ctrls->voltage;
	enum pin_act act = pin_ctrls->act;
	int ret = 0;

	switch (act) {
	case PIN_NONE:
		usleep_range(delay, delay);
		break;
	case PIN_OUTPUT:
		if (gpio_is_valid(pin)) {
			if (value)
				gpio_request_one(pin, GPIOF_OUT_INIT_HIGH, "CAM_GPIO_OUTPUT_HIGH");
			else
				gpio_request_one(pin, GPIOF_OUT_INIT_LOW, "CAM_GPIO_OUTPUT_LOW");
			usleep_range(delay, delay);
			gpio_free(pin);
		}
		break;
	case PIN_INPUT:
		if (gpio_is_valid(pin)) {
			gpio_request_one(pin, GPIOF_IN, "CAM_GPIO_INPUT");
			gpio_free(pin);
		}
		break;
	case PIN_RESET:
		if (gpio_is_valid(pin)) {
			gpio_request_one(pin, GPIOF_OUT_INIT_HIGH, "CAM_GPIO_RESET");
			usleep_range(1000, 1000);
			__gpio_set_value(pin, 0);
			usleep_range(1000, 1000);
			__gpio_set_value(pin, 1);
			usleep_range(1000, 1000);
			gpio_free(pin);
		}
		break;
	case PIN_FUNCTION:
		{
			struct pinctrl_state *s = (struct pinctrl_state *)pin;

			ret = pinctrl_select_state(pinctrl, s);
			if (ret < 0) {
				pr_err("pinctrl_select_state(%s) is fail(%d)\n", name, ret);
				return ret;
			}
			usleep_range(delay, delay);
		}
		break;
	case PIN_REGULATOR:
		{
			struct regulator *regulator = NULL;

			regulator = regulator_get(&client->dev, name);
			if (IS_ERR_OR_NULL(regulator)) {
				pr_err("%s : regulator_get(%s) fail\n", __func__, name);
				regulator_put(regulator);
				return -EINVAL;
			}

			if (value) {
				if(voltage > 0) {
					pr_info("%s : regulator_set_voltage(%d)\n",__func__, voltage);
					ret = regulator_set_voltage(regulator, voltage, voltage);
					if(ret) {
						pr_err("%s : regulator_set_voltage(%d) fail\n", __func__, ret);
					}
				}

				if (regulator_is_enabled(regulator)) {
					pr_warning("%s regulator is already enabled\n", name);
					regulator_put(regulator);
					return 0;
				}

				ret = regulator_enable(regulator);
				if (ret) {
					pr_err("%s : regulator_enable(%s) fail\n", __func__, name);
					regulator_put(regulator);
					return ret;
				}
			} else {
				if (!regulator_is_enabled(regulator)) {
					pr_warning("%s regulator is already disabled\n", name);
					regulator_put(regulator);
					return 0;
				}

				ret = regulator_disable(regulator);
				if (ret) {
					pr_err("%s : regulator_disable(%s) fail\n", __func__, name);
					regulator_put(regulator);
					return ret;
				}
			}

			usleep_range(delay, delay);
			regulator_put(regulator);
		}
		break;
	default:
		pr_err("unknown act for pin\n");
		break;
	}

	return ret;
}

int exynos_fimc_is_module_soc_pins_cfg(struct i2c_client *client,
	u32 scenario,
	u32 enable)
{
	int ret = 0;
	u32 idx_max, idx;
	struct pinctrl *pinctrl;
	struct exynos_sensor_pin (*pin_ctrls)[GPIO_SCENARIO_MAX][GPIO_CTRL_MAX];
	struct exynos_platform_fimc_is_module *pdata;
	struct fimc_is_module_enum *module;
	struct v4l2_subdev *subdev_module;

	BUG_ON(!client);
	subdev_module = (struct v4l2_subdev *)i2c_get_clientdata(client);

	BUG_ON(!subdev_module);
	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev_module);

	BUG_ON(!module);
	BUG_ON(!module->pdata);

	pdata = module->pdata;

	BUG_ON(enable > 1);
	BUG_ON(scenario > SENSOR_SCENARIO_MAX);

	pinctrl = pdata->pinctrl;
	pin_ctrls = pdata->pin_ctrls;
	idx_max = pdata->pinctrl_index[scenario][enable];

	/* print configs */
	for (idx = 0; idx < idx_max; ++idx) {
		printk(KERN_DEBUG "[@] pin_ctrl(act(%d), pin(%ld), val(%d), nm(%s)\n",
			pin_ctrls[scenario][enable][idx].act,
			(pin_ctrls[scenario][enable][idx].act == PIN_FUNCTION) ? 0 : pin_ctrls[scenario][enable][idx].pin,
			pin_ctrls[scenario][enable][idx].value,
			pin_ctrls[scenario][enable][idx].name);
	}

	/* do configs */
	for (idx = 0; idx < idx_max; ++idx) {
		ret = exynos_fimc_is_module_soc_pin_control(client, pinctrl, &pin_ctrls[scenario][enable][idx]);
		if (ret) {
			pr_err("[@] exynos_fimc_is_sensor_gpio(%d) is fail(%d)", idx, ret);
			goto p_err;
		}
	}

p_err:
	return ret;
}

static int exynos_fimc_is_module_soc_pin_debug(struct i2c_client *client,
	struct pinctrl *pinctrl, struct exynos_sensor_pin *pin_ctrls)
{
	int ret = 0;
	ulong pin = pin_ctrls->pin;
	char* name = pin_ctrls->name;
	enum pin_act act = pin_ctrls->act;

	switch (act) {
	case PIN_NONE:
		break;
	case PIN_OUTPUT:
	case PIN_INPUT:
	case PIN_RESET:
		if (gpio_is_valid(pin))
			pr_info("[@] pin %s : %d\n", name, gpio_get_value(pin));
		break;
	case PIN_FUNCTION:
#if 0 // TEMP_CARMEN2
		{
			/* there is no way to get pin by name after probe */
			ulong base, pin;
			u32 index;

			base = (ulong)ioremap_nocache(0x13470000, 0x10000);

			index = 0x60; /* GPC2 */
			pin = base + index;
			pr_info("[@] CON[0x%X] : 0x%X\n", index, readl((void *)pin));
			pr_info("[@] DAT[0x%X] : 0x%X\n", index, readl((void *)(pin + 4)));

			index = 0x160; /* GPD7 */
			pin = base + index;
			pr_info("[@] CON[0x%X] : 0x%X\n", index, readl((void *)pin));
			pr_info("[@] DAT[0x%X] : 0x%X\n", index, readl((void *)(pin + 4)));

			iounmap((void *)base);
		}
#endif
		break;
	case PIN_REGULATOR:
		{
			struct regulator *regulator;
			int voltage;

			regulator = regulator_get(&client->dev, name);
			if (IS_ERR(regulator)) {
				pr_err("%s : regulator_get(%s) fail\n", __func__, name);
				regulator_put(regulator);
				return -EINVAL;
			}

			if (regulator_is_enabled(regulator))
				voltage = regulator_get_voltage(regulator);
			else
				voltage = 0;

			regulator_put(regulator);

			pr_info("[@] %s LDO : %d\n", name, voltage);
		}
		break;
	default:
		pr_err("unknown act for pin\n");
		break;
	}

	return ret;
}

int exynos_fimc_is_module_soc_pins_dbg(struct i2c_client *client,
	u32 scenario,
	u32 enable)
{
	int ret = 0;
	u32 idx_max, idx;
	struct pinctrl *pinctrl;
	struct exynos_sensor_pin (*pin_ctrls)[GPIO_SCENARIO_MAX][GPIO_CTRL_MAX];
	struct exynos_platform_fimc_is_module *pdata;
	struct fimc_is_module_enum *module;
	struct v4l2_subdev *subdev_module;

	BUG_ON(!client);
	subdev_module = (struct v4l2_subdev *)i2c_get_clientdata(client);

	BUG_ON(!subdev_module);
	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev_module);
	BUG_ON(!module);
	BUG_ON(!module->pdata);

	pdata = module->pdata;

	pinctrl = pdata->pinctrl;
	pin_ctrls = pdata->pin_ctrls;
	idx_max = pdata->pinctrl_index[scenario][enable];

	/* print configs */
	for (idx = 0; idx < idx_max; ++idx) {
		printk(KERN_DEBUG "[@] pin_ctrl(act(%d), pin(%ld), val(%d), nm(%s)\n",
			pin_ctrls[scenario][enable][idx].act,
			(pin_ctrls[scenario][enable][idx].act == PIN_FUNCTION) ? 0 : pin_ctrls[scenario][enable][idx].pin,
			pin_ctrls[scenario][enable][idx].value,
			pin_ctrls[scenario][enable][idx].name);
	}

	/* do configs */
	for (idx = 0; idx < idx_max; ++idx) {
		ret = exynos_fimc_is_module_soc_pin_debug(client, pinctrl, &pin_ctrls[scenario][enable][idx]);
		if (ret) {
			pr_err("[@] exynos_fimc_is_sensor_gpio(%d) is fail(%d)", idx, ret);
			goto p_err;
		}
	}

p_err:
	return ret;
}

int fimc_is_sensor_module_soc_parse_dt(struct i2c_client *client,
	struct exynos_platform_fimc_is_module *pdata,
	fimc_is_moudle_soc_dt_callback module_callback)
{
	int ret = 0;
	struct device_node *dnode;
	struct device_node *af_np;
	struct device_node *flash_np;
	struct device_node *companion_np;
	struct device_node *ois_np;

	BUG_ON(!client);
	BUG_ON(!client->dev.of_node);
	BUG_ON(!module_callback);

	dnode = client->dev.of_node;

	pdata->gpio_cfg = NULL;
	pdata->gpio_dbg = NULL;
	pdata->gpio_soc_cfg = exynos_fimc_is_module_soc_pins_cfg;
	pdata->gpio_soc_dbg = exynos_fimc_is_module_soc_pins_dbg;

	ret = of_property_read_u32(dnode, "id", &pdata->id);
	if (ret) {
		err("id read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "mclk_ch", &pdata->mclk_ch);
	if (ret) {
		err("mclk_ch read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "sensor_i2c_ch", &pdata->sensor_i2c_ch);
	if (ret) {
		err("i2c_ch read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "sensor_i2c_addr", &pdata->sensor_i2c_addr);
	if (ret) {
		err("i2c_addr read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "position", &pdata->position);
	if (ret) {
		err("id read is fail(%d)", ret);
		goto p_err;
	}

	af_np = of_find_node_by_name(dnode, "af");
	if (!af_np) {
		pdata->af_product_name = ACTUATOR_NAME_NOTHING;
	} else {
		parse_af_data(pdata, af_np);
	}

	flash_np = of_find_node_by_name(dnode, "flash");
	if (!flash_np) {
		pdata->flash_product_name = FLADRV_NAME_NOTHING;
	} else {
		parse_flash_data(pdata, flash_np);
	}

	companion_np = of_find_node_by_name(dnode, "companion");
	if (!companion_np) {
		pdata->companion_product_name = COMPANION_NAME_NOTHING;
	} else {
		parse_companion_data(pdata, companion_np);
	}

	ois_np = of_find_node_by_name(dnode, "ois");
	if (!ois_np) {
		pdata->ois_product_name = OIS_NAME_NOTHING;
	} else {
		parse_ois_data(pdata, ois_np);
	}

	ret = module_callback(client, pdata);
	if (ret) {
		err("sensor dt callback is fail(%d)", ret);
		goto p_err;
	}

	pdata->pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR(pdata->pinctrl)) {
		err("devm_pinctrl_get is fail");
		goto p_err;
	}

	ret = get_pin_lookup_state(pdata->pinctrl, pdata->pin_ctrls);
	if (ret) {
		err("get_pin_lookup_state is fail(%d)", ret);
		goto p_err;
	}

	return ret;

p_err:
	return ret;
}
#endif /*CONFIG_CAMERA_USE_SOC_SENSOR*/

int fimc_is_spi_parse_dt(struct fimc_is_spi *spi)
{
	int ret = 0;
	struct device_node *np;
	struct fimc_is_spi_gpio *gpio;

	BUG_ON(!spi);

	gpio = &spi->gpio;

	np = of_find_compatible_node(NULL,NULL, spi->node);
	if(np == NULL) {
		pr_err("compatible: fail to read, spi_parse_dt\n");
		ret = -ENODEV;
		goto p_err;
	}

	ret = of_property_read_string(np, "fimc_is_spi_clk", (const char **) &gpio->clk);
	if (ret) {
		pr_err("spi gpio: fail to read, spi_parse_dt\n");
		ret = -ENODEV;
		goto p_err;
	}

	ret = of_property_read_string(np, "fimc_is_spi_ssn",(const char **) &gpio->ssn);
	if (ret) {
		pr_err("spi gpio: fail to read, spi_parse_dt\n");
		ret = -ENODEV;
		goto p_err;
	}

	ret = of_property_read_string(np, "fimc_is_spi_miso",(const char **) &gpio->miso);
	if (ret) {
		pr_err("spi gpio: fail to read, spi_parse_dt\n");
		ret = -ENODEV;
		goto p_err;
	}

	ret = of_property_read_string(np, "fimc_is_spi_mosi",(const char **) &gpio->mosi);
	if (ret) {
		pr_err("spi gpio: fail to read, spi_parse_dt\n");
		ret = -ENODEV;
		goto p_err;
	}

	ret = of_property_read_string(np, "fimc_is_spi_pinname", (const char **) &gpio->pinname);
	if (ret) {
		pr_err("spi gpio: fail to read, spi_parse_dt\n");
		ret = -ENODEV;
		goto p_err;
	}

	info("[SPI] clk = %s, ssn = %s, miso = %s, mosi = %s\n", gpio->clk, gpio->ssn,
		gpio->miso, gpio->mosi);

p_err:
	return ret;
}
#else
struct exynos_platform_fimc_is *fimc_is_parse_dt(struct device *dev)
{
	return ERR_PTR(-EINVAL);
}
#endif
