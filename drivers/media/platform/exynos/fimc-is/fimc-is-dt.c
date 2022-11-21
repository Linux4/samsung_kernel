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
#include <mach/exynos-fimc-is-sensor.h>
#include <mach/exynos-fimc-is.h>
#include <media/exynos_mc.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_gpio.h>
#endif

#include "fimc-is-core.h"
#include "fimc-is-dt.h"
#include "fimc-is-core.h"

#ifdef CONFIG_OF
int get_pin_lookup_state(struct device *dev,
	struct exynos_platform_fimc_is_sensor *pdata)
{
	int ret = 0;
	u32 i, j, k;
	char ch_name[30];
	struct exynos_sensor_pin (*pin_ctrls)[2][GPIO_CTRL_MAX];
	struct pinctrl_state *s;

	pin_ctrls = pdata->pin_ctrls;

	for (i = 0; i < SENSOR_SCENARIO_MAX; ++i) {
		for (j = 0; j < GPIO_SCENARIO_MAX; ++j) {
			for (k = 0; k < GPIO_CTRL_MAX; ++k) {
				if (pin_ctrls[i][j][k].act == PIN_FUNCTION) {
					snprintf(ch_name, sizeof(ch_name), "%s%d",
							pin_ctrls[i][j][k].name,
							pdata->csi_ch);
					s = pinctrl_lookup_state(pdata->pinctrl, ch_name);
					if (IS_ERR(s)) {
						err("cam %s, ch %d pinctrl_lookup_state is failed", ch_name, pdata->csi_ch);
						ret = -EINVAL;
						goto p_err;
					} else {
						pin_ctrls[i][j][k].pin = (unsigned long)s;
						pr_info("[%d][%d][%d][%s] gpio function cfg is seted", i, j, k, ch_name);
					}
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
	pdata->gate_info->user_clk_gate = exynos_fimc_is_set_user_clk_gate;
	pdata->gate_info->clk_on_off = exynos_fimc_is_clk_gate;

	return 0;
p_err:
	kfree(gate_info);
	return ret;
}

static int parse_dvfs_data(struct exynos_platform_fimc_is *pdata, struct device_node *np)
{
	u32 temp;
	char *pprop;

	memset(pdata->dvfs_data, 0, sizeof(pdata->dvfs_data));
	DT_READ_U32(np, "default_int", pdata->dvfs_data[FIMC_IS_SN_DEFAULT][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "default_cam", pdata->dvfs_data[FIMC_IS_SN_DEFAULT][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "default_mif", pdata->dvfs_data[FIMC_IS_SN_DEFAULT][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "default_i2c", pdata->dvfs_data[FIMC_IS_SN_DEFAULT][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "front_preview_int", pdata->dvfs_data[FIMC_IS_SN_FRONT_PREVIEW][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "front_preview_cam", pdata->dvfs_data[FIMC_IS_SN_FRONT_PREVIEW][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "front_preview_mif", pdata->dvfs_data[FIMC_IS_SN_FRONT_PREVIEW][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "front_preview_i2c", pdata->dvfs_data[FIMC_IS_SN_FRONT_PREVIEW][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "front_capture_int", pdata->dvfs_data[FIMC_IS_SN_FRONT_CAPTURE][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "front_capture_cam", pdata->dvfs_data[FIMC_IS_SN_FRONT_CAPTURE][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "front_capture_mif", pdata->dvfs_data[FIMC_IS_SN_FRONT_CAPTURE][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "front_capture_i2c", pdata->dvfs_data[FIMC_IS_SN_FRONT_CAPTURE][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "front_camcording_int", pdata->dvfs_data[FIMC_IS_SN_FRONT_CAMCORDING][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "front_camcording_cam", pdata->dvfs_data[FIMC_IS_SN_FRONT_CAMCORDING][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "front_camcording_mif", pdata->dvfs_data[FIMC_IS_SN_FRONT_CAMCORDING][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "front_camcording_i2c", pdata->dvfs_data[FIMC_IS_SN_FRONT_CAMCORDING][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "front_vt1_int", pdata->dvfs_data[FIMC_IS_SN_FRONT_VT1][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "front_vt1_cam", pdata->dvfs_data[FIMC_IS_SN_FRONT_VT1][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "front_vt1_mif", pdata->dvfs_data[FIMC_IS_SN_FRONT_VT1][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "front_vt1_i2c", pdata->dvfs_data[FIMC_IS_SN_FRONT_VT1][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "front_vt2_int", pdata->dvfs_data[FIMC_IS_SN_FRONT_VT2][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "front_vt2_cam", pdata->dvfs_data[FIMC_IS_SN_FRONT_VT2][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "front_vt2_mif", pdata->dvfs_data[FIMC_IS_SN_FRONT_VT2][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "front_vt2_i2c", pdata->dvfs_data[FIMC_IS_SN_FRONT_VT2][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "rear_preview_fhd_bns_off_int", pdata->dvfs_data[FIMC_IS_SN_REAR_PREVIEW_FHD_BNS_OFF][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "rear_preview_fhd_bns_off_cam", pdata->dvfs_data[FIMC_IS_SN_REAR_PREVIEW_FHD_BNS_OFF][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "rear_preview_fhd_bns_off_mif", pdata->dvfs_data[FIMC_IS_SN_REAR_PREVIEW_FHD_BNS_OFF][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "rear_preview_fhd_bns_off_i2c", pdata->dvfs_data[FIMC_IS_SN_REAR_PREVIEW_FHD_BNS_OFF][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "rear_preview_fhd_int", pdata->dvfs_data[FIMC_IS_SN_REAR_PREVIEW_FHD][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "rear_preview_fhd_cam", pdata->dvfs_data[FIMC_IS_SN_REAR_PREVIEW_FHD][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "rear_preview_fhd_mif", pdata->dvfs_data[FIMC_IS_SN_REAR_PREVIEW_FHD][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "rear_preview_fhd_i2c", pdata->dvfs_data[FIMC_IS_SN_REAR_PREVIEW_FHD][FIMC_IS_DVFS_I2C]);
	/* if there's no FHD preview(with BNS off) dvfa data, set value of FHD recording data */
	if (!(pdata->dvfs_data[FIMC_IS_SN_REAR_PREVIEW_FHD_BNS_OFF][FIMC_IS_DVFS_MIF])) {
		pdata->dvfs_data[FIMC_IS_SN_REAR_PREVIEW_FHD_BNS_OFF][FIMC_IS_DVFS_INT] = pdata->dvfs_data[FIMC_IS_SN_REAR_PREVIEW_FHD][FIMC_IS_DVFS_INT];
		pdata->dvfs_data[FIMC_IS_SN_REAR_PREVIEW_FHD_BNS_OFF][FIMC_IS_DVFS_CAM] = pdata->dvfs_data[FIMC_IS_SN_REAR_PREVIEW_FHD][FIMC_IS_DVFS_CAM];
		pdata->dvfs_data[FIMC_IS_SN_REAR_PREVIEW_FHD_BNS_OFF][FIMC_IS_DVFS_MIF] = pdata->dvfs_data[FIMC_IS_SN_REAR_PREVIEW_FHD][FIMC_IS_DVFS_MIF];
		pdata->dvfs_data[FIMC_IS_SN_REAR_PREVIEW_FHD_BNS_OFF][FIMC_IS_DVFS_I2C] = pdata->dvfs_data[FIMC_IS_SN_REAR_PREVIEW_FHD][FIMC_IS_DVFS_I2C];
	}
	DT_READ_U32(np, "rear_preview_whd_int", pdata->dvfs_data[FIMC_IS_SN_REAR_PREVIEW_WHD][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "rear_preview_whd_cam", pdata->dvfs_data[FIMC_IS_SN_REAR_PREVIEW_WHD][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "rear_preview_whd_mif", pdata->dvfs_data[FIMC_IS_SN_REAR_PREVIEW_WHD][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "rear_preview_whd_i2c", pdata->dvfs_data[FIMC_IS_SN_REAR_PREVIEW_WHD][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "rear_preview_uhd_int", pdata->dvfs_data[FIMC_IS_SN_REAR_PREVIEW_UHD][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "rear_preview_uhd_cam", pdata->dvfs_data[FIMC_IS_SN_REAR_PREVIEW_UHD][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "rear_preview_uhd_mif", pdata->dvfs_data[FIMC_IS_SN_REAR_PREVIEW_UHD][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "rear_preview_uhd_i2c", pdata->dvfs_data[FIMC_IS_SN_REAR_PREVIEW_UHD][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "rear_capture_int", pdata->dvfs_data[FIMC_IS_SN_REAR_CAPTURE][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "rear_capture_cam", pdata->dvfs_data[FIMC_IS_SN_REAR_CAPTURE][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "rear_capture_mif", pdata->dvfs_data[FIMC_IS_SN_REAR_CAPTURE][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "rear_capture_i2c", pdata->dvfs_data[FIMC_IS_SN_REAR_CAPTURE][FIMC_IS_DVFS_I2C]);

	DT_READ_U32(np, "rear_camcording_fhd_bns_off_int", pdata->dvfs_data[FIMC_IS_SN_REAR_CAMCORDING_FHD_BNS_OFF][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "rear_camcording_fhd_bns_off_cam", pdata->dvfs_data[FIMC_IS_SN_REAR_CAMCORDING_FHD_BNS_OFF][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "rear_camcording_fhd_bns_off_mif", pdata->dvfs_data[FIMC_IS_SN_REAR_CAMCORDING_FHD_BNS_OFF][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "rear_camcording_fhd_bns_off_i2c", pdata->dvfs_data[FIMC_IS_SN_REAR_CAMCORDING_FHD_BNS_OFF][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "rear_camcording_fhd_int", pdata->dvfs_data[FIMC_IS_SN_REAR_CAMCORDING_FHD][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "rear_camcording_fhd_cam", pdata->dvfs_data[FIMC_IS_SN_REAR_CAMCORDING_FHD][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "rear_camcording_fhd_mif", pdata->dvfs_data[FIMC_IS_SN_REAR_CAMCORDING_FHD][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "rear_camcording_fhd_i2c", pdata->dvfs_data[FIMC_IS_SN_REAR_CAMCORDING_FHD][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "rear_camcording_whd_int", pdata->dvfs_data[FIMC_IS_SN_REAR_CAMCORDING_WHD][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "rear_camcording_whd_cam", pdata->dvfs_data[FIMC_IS_SN_REAR_CAMCORDING_WHD][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "rear_camcording_whd_mif", pdata->dvfs_data[FIMC_IS_SN_REAR_CAMCORDING_WHD][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "rear_camcording_whd_i2c", pdata->dvfs_data[FIMC_IS_SN_REAR_CAMCORDING_WHD][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "rear_camcording_uhd_int", pdata->dvfs_data[FIMC_IS_SN_REAR_CAMCORDING_UHD][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "rear_camcording_uhd_cam", pdata->dvfs_data[FIMC_IS_SN_REAR_CAMCORDING_UHD][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "rear_camcording_uhd_mif", pdata->dvfs_data[FIMC_IS_SN_REAR_CAMCORDING_UHD][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "rear_camcording_uhd_i2c", pdata->dvfs_data[FIMC_IS_SN_REAR_CAMCORDING_UHD][FIMC_IS_DVFS_I2C]);
	/* if there's no FHD recording(with BNS off) dvfa data, set value of FHD recording data */
	if (!(pdata->dvfs_data[FIMC_IS_SN_REAR_CAMCORDING_FHD_BNS_OFF][FIMC_IS_DVFS_MIF])) {
		pdata->dvfs_data[FIMC_IS_SN_REAR_CAMCORDING_FHD_BNS_OFF][FIMC_IS_DVFS_INT] = pdata->dvfs_data[FIMC_IS_SN_REAR_CAMCORDING_FHD][FIMC_IS_DVFS_INT];
		pdata->dvfs_data[FIMC_IS_SN_REAR_CAMCORDING_FHD_BNS_OFF][FIMC_IS_DVFS_CAM] = pdata->dvfs_data[FIMC_IS_SN_REAR_CAMCORDING_FHD][FIMC_IS_DVFS_CAM];
		pdata->dvfs_data[FIMC_IS_SN_REAR_CAMCORDING_FHD_BNS_OFF][FIMC_IS_DVFS_MIF] = pdata->dvfs_data[FIMC_IS_SN_REAR_CAMCORDING_FHD][FIMC_IS_DVFS_MIF];
		pdata->dvfs_data[FIMC_IS_SN_REAR_CAMCORDING_FHD_BNS_OFF][FIMC_IS_DVFS_I2C] = pdata->dvfs_data[FIMC_IS_SN_REAR_CAMCORDING_FHD][FIMC_IS_DVFS_I2C];
	}
	DT_READ_U32(np, "dual_preview_int", pdata->dvfs_data[FIMC_IS_SN_DUAL_PREVIEW][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "dual_preview_cam", pdata->dvfs_data[FIMC_IS_SN_DUAL_PREVIEW][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "dual_preview_mif", pdata->dvfs_data[FIMC_IS_SN_DUAL_PREVIEW][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "dual_preview_i2c", pdata->dvfs_data[FIMC_IS_SN_DUAL_PREVIEW][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "dual_capture_int", pdata->dvfs_data[FIMC_IS_SN_DUAL_CAPTURE][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "dual_capture_cam", pdata->dvfs_data[FIMC_IS_SN_DUAL_CAPTURE][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "dual_capture_mif", pdata->dvfs_data[FIMC_IS_SN_DUAL_CAPTURE][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "dual_capture_i2c", pdata->dvfs_data[FIMC_IS_SN_DUAL_CAPTURE][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "dual_camcording_int", pdata->dvfs_data[FIMC_IS_SN_DUAL_CAMCORDING][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "dual_camcording_cam", pdata->dvfs_data[FIMC_IS_SN_DUAL_CAMCORDING][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "dual_camcording_mif", pdata->dvfs_data[FIMC_IS_SN_DUAL_CAMCORDING][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "dual_camcording_i2c", pdata->dvfs_data[FIMC_IS_SN_DUAL_CAMCORDING][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "high_speed_fps_int", pdata->dvfs_data[FIMC_IS_SN_HIGH_SPEED_FPS][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "high_speed_fps_cam", pdata->dvfs_data[FIMC_IS_SN_HIGH_SPEED_FPS][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "high_speed_fps_mif", pdata->dvfs_data[FIMC_IS_SN_HIGH_SPEED_FPS][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "high_speed_fps_i2c", pdata->dvfs_data[FIMC_IS_SN_HIGH_SPEED_FPS][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "dis_enable_int", pdata->dvfs_data[FIMC_IS_SN_DIS_ENABLE][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "dis_enable_cam", pdata->dvfs_data[FIMC_IS_SN_DIS_ENABLE][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "dis_enable_mif", pdata->dvfs_data[FIMC_IS_SN_DIS_ENABLE][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "dis_enable_i2c", pdata->dvfs_data[FIMC_IS_SN_DIS_ENABLE][FIMC_IS_DVFS_I2C]);
	DT_READ_U32(np, "max_int", pdata->dvfs_data[FIMC_IS_SN_MAX][FIMC_IS_DVFS_INT]);
	DT_READ_U32(np, "max_cam", pdata->dvfs_data[FIMC_IS_SN_MAX][FIMC_IS_DVFS_CAM]);
	DT_READ_U32(np, "max_mif", pdata->dvfs_data[FIMC_IS_SN_MAX][FIMC_IS_DVFS_MIF]);
	DT_READ_U32(np, "max_i2c", pdata->dvfs_data[FIMC_IS_SN_MAX][FIMC_IS_DVFS_I2C]);

	return 0;
}

#ifdef CONFIG_CAMERA_SYSFS_V2
static int parse_sysfs_caminfo(struct exynos_platform_fimc_is *pdata, struct device_node *np,
	struct fimc_is_cam_info * cam_infos, int camera_num)
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

static int parse_subip_info(struct exynos_platform_fimc_is *pdata, struct device_node *np)
{
	u32 temp;
	char *pprop;
	struct exynos_fimc_is_subip_info *subip_info;

	/* get subip of fimc-is info */
	subip_info = kzalloc(sizeof(struct exynos_fimc_is_subip_info), GFP_KERNEL);
	if (!subip_info) {
		printk(KERN_ERR "%s: no memory for fimc_is subip_info\n", __func__);
		return -EINVAL;
	}

	DT_READ_U32(np, "num_of_mcuctl", subip_info->_mcuctl.valid);
	DT_READ_U32(np, "num_of_3a0", subip_info->_3a0.valid);
	DT_READ_U32(np, "num_of_3a1", subip_info->_3a1.valid);
	DT_READ_U32(np, "num_of_isp", subip_info->_isp.valid);
	DT_READ_U32(np, "num_of_drc", subip_info->_drc.valid);
	DT_READ_U32(np, "num_of_scc", subip_info->_scc.valid);
	DT_READ_U32(np, "num_of_odc", subip_info->_odc.valid);
	DT_READ_U32(np, "num_of_dis", subip_info->_dis.valid);
	DT_READ_U32(np, "num_of_dnr", subip_info->_dnr.valid);
	DT_READ_U32(np, "num_of_scp", subip_info->_scp.valid);
	DT_READ_U32(np, "num_of_fd",  subip_info->_fd.valid);

	DT_READ_U32(np, "full_bypass_mcuctl", subip_info->_mcuctl.full_bypass);
	DT_READ_U32(np, "full_bypass_3a0", subip_info->_3a0.full_bypass);
	DT_READ_U32(np, "full_bypass_3a1", subip_info->_3a1.full_bypass);
	DT_READ_U32(np, "full_bypass_isp", subip_info->_isp.full_bypass);
	DT_READ_U32(np, "full_bypass_drc", subip_info->_drc.full_bypass);
	DT_READ_U32(np, "full_bypass_scc", subip_info->_scc.full_bypass);
	DT_READ_U32(np, "full_bypass_odc", subip_info->_odc.full_bypass);
	DT_READ_U32(np, "full_bypass_dis", subip_info->_dis.full_bypass);
	DT_READ_U32(np, "full_bypass_dnr", subip_info->_dnr.full_bypass);
	DT_READ_U32(np, "full_bypass_scp", subip_info->_scp.full_bypass);
	DT_READ_U32(np, "full_bypass_fd",  subip_info->_fd.full_bypass);

	DT_READ_U32(np, "version_mcuctl", subip_info->_mcuctl.version);
	DT_READ_U32(np, "version_3a0", subip_info->_3a0.version);
	DT_READ_U32(np, "version_3a1", subip_info->_3a1.version);
	DT_READ_U32(np, "version_isp", subip_info->_isp.version);
	DT_READ_U32(np, "version_drc", subip_info->_drc.version);
	DT_READ_U32(np, "version_scc", subip_info->_scc.version);
	DT_READ_U32(np, "version_odc", subip_info->_odc.version);
	DT_READ_U32(np, "version_dis", subip_info->_dis.version);
	DT_READ_U32(np, "version_dnr", subip_info->_dnr.version);
	DT_READ_U32(np, "version_scp", subip_info->_scp.version);
	DT_READ_U32(np, "version_fd",  subip_info->_fd.version);

	pdata->subip_info = subip_info;

	return 0;
}

int fimc_is_power_initpin(struct device *dev)
{
	struct exynos_platform_fimc_is_sensor *pdata;
	int gpio_none = 0;

	BUG_ON(!dev);
	BUG_ON(!dev->platform_data);

	pdata = dev->platform_data;

	if (!pdata->sensor_id || (pdata->sensor_id >= SENSOR_NAME_END)) {
		err("check the sensor id. sensor_id %d", pdata->sensor_id);
		return -ENODEV;
	}

	/* POWER ON */
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, 0, gpio_none, 0, NULL, 0, PIN_END);

	/* POWER OFF */
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, 0, gpio_none, 0, NULL, 0, PIN_END);

	return 0;
}

int fimc_is_power_setpin(struct device *dev, int position, int sensor_id)
{
	struct fimc_is_core *core;
	struct fimc_is_device_sensor *sensor;
	int ret = 0;
	int i, pos, found;

	if (!fimc_is_dev) {
		err("fimc_is_dev is not yet probed");
		return -ENODEV;
	}

	if (!sensor_id || (sensor_id >= SENSOR_NAME_END)) {
		err("check the sensor id. sensor_id %d", sensor_id);
		return -ENODEV;
	}

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core) {
		err("core is NULL");
		return -EINVAL;
	}

	if (!atomic_read(&core->resourcemgr.rsccount_module))
		err("sensor driver not probed");

	sensor = &core->sensor[position];

	/* Call power_setpin and return  */
	for (i = 0; i < atomic_read(&core->resourcemgr.rsccount_module); i++) {
		if (sensor_id == sensor->module_enum[i].id) {
			info("%s: sensor found(id %d). %s\n", __func__, sensor_id,
				(position == sensor->module_enum[i].position) ? "" : "position not matched");

			if (sensor->module_enum[i].power_setpin)
				ret = sensor->module_enum[i].power_setpin(dev);

			return ret;
		}
	}

	/* Enumerate probed sensor lists if not found */
	for (pos = 0, found = 0; pos < FIMC_IS_MAX_NODES; pos++, found = 0) {
		sensor = &core->sensor[pos];
		for (i = 0; i < atomic_read(&core->resourcemgr.rsccount_module); i++) {
			if (sensor->module_enum[i].id) {
				info("Camera sensor %d: id %3d. %s\n", pos, sensor->module_enum[i].id,
					sensor->module_enum[i].setfile_name ?
					sensor->module_enum[i].setfile_name : "");
				found++;
			}
		}

		if (!found)
			info("Camera sensor %d: none\n", pos);
	}

	err("sensor not found (pos %d, id %d)", position, sensor_id);

	return 0;
}

struct exynos_platform_fimc_is *fimc_is_parse_dt(struct device *dev)
{
	void *ret = NULL;
	struct exynos_platform_fimc_is *pdata;
	struct device_node *subip_info_np;
	struct device_node *dvfs_np;
	struct device_node *np = dev->of_node;
#ifdef CONFIG_CAMERA_SYSFS_V2
	struct device_node *camInfo_np;
	struct fimc_is_cam_info *camera_infos;
	char camInfo_string[15];
	int camera_num;
#endif
#if defined CONFIG_COMPANION_USE || defined CONFIG_USE_VENDER_FEATURE
	int retVal = 0;
#endif

	if (!np)
		return ERR_PTR(-ENOENT);

	pdata = kzalloc(sizeof(struct exynos_platform_fimc_is), GFP_KERNEL);
	if (!pdata) {
		printk(KERN_ERR "%s: no memory for platform data\n", __func__);
		return ERR_PTR(-ENOMEM);
	}

	pdata->clk_cfg = exynos_fimc_is_cfg_clk;
	pdata->clk_on = exynos_fimc_is_clk_on;
	pdata->clk_off = exynos_fimc_is_clk_off;
	pdata->print_clk = exynos_fimc_is_print_clk;
	pdata->print_cfg = exynos_fimc_is_print_cfg;
	pdata->print_pwr = exynos_fimc_is_print_pwr;

	dev->platform_data = pdata;

#ifdef CONFIG_COMPANION_USE
	retVal = of_property_read_u32(np, "companion_spi_channel", &pdata->companion_spi_channel);
	if (retVal) {
		err("spi_channel read is fail(%d)", retVal);
	}

	pdata->use_two_spi_line = of_property_read_bool(np, "use_two_spi_line");
#endif
#ifdef CONFIG_USE_VENDER_FEATURE
	retVal = of_property_read_u32(np, "use_sensor_dynamic_voltage_mode", &pdata->use_sensor_dynamic_voltage_mode);
	if (retVal) {
		err("use_sensor_dynamic_voltage_mode read is fail(%d)", retVal);
		pdata->use_sensor_dynamic_voltage_mode = 0;
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

#ifdef CONFIG_CAMERA_VARIABLE_MIPI_LANES
	if (of_find_property(np, "vtcam_mipi_lane_num", NULL)) {
		retVal = of_property_read_u32(np, "vtcam_mipi_lane_num", &pdata->vtcam_mipi_lane_num);
		if (retVal) {
			err("vtcam_mipi_lane_num read is fail(%d)", retVal);
			pdata->vtcam_mipi_lane_num = 0;
		}
		info("vtcam_mipi_lane_num (%d)", pdata->vtcam_mipi_lane_num);
	}
#endif

	pdata->skip_cal_loading = of_property_read_bool(np, "skip_cal_loading");
	if (!pdata->skip_cal_loading) {
		info("skip_cal_loading not use(%d)", pdata->skip_cal_loading);
	}
#endif
	subip_info_np = of_find_node_by_name(np, "subip_info");
	if (!subip_info_np) {
		printk(KERN_ERR "%s: can't find fimc_is subip_info node\n", __func__);
		ret = ERR_PTR(-ENOENT);
		goto p_err;
	}
	parse_subip_info(pdata, subip_info_np);

	if (parse_gate_info(pdata, np) < 0)
		printk(KERN_ERR "%s: can't parse clock gate info node\n", __func__);

	dvfs_np = of_find_node_by_name(np, "fimc_is_dvfs");
	if (!dvfs_np) {
		printk(KERN_ERR "%s: can't find fimc_is_dvfs node\n", __func__);
		ret = ERR_PTR(-ENOENT);
		goto p_err;
	}
	parse_dvfs_data(pdata, dvfs_np);

#ifdef CONFIG_CAMERA_SYSFS_V2
	retVal = of_property_read_u32(np, "total_camera_num", &pdata->total_camera_num);
	if (retVal) {
		err("total_camera_num read is fail(%d)", retVal);
		pdata->total_camera_num = 0;
	}
	camera_infos = kzalloc(sizeof(struct fimc_is_cam_info) * pdata->total_camera_num, GFP_KERNEL);
	if (!camera_infos) {
		printk(KERN_ERR "%s: no memory for fimc_is camera_infos\n", __func__);
		return ERR_PTR(-ENOMEM);
	}
	pdata->cam_infos = camera_infos;

	for (camera_num = 0; camera_num < pdata->total_camera_num; camera_num++) {
		sprintf(camInfo_string, "%s%d", "camera_info", camera_num);

		camInfo_np = of_find_node_by_name(np, camInfo_string);
		if (!camInfo_np) {
			printk(KERN_ERR "%s: can't find camInfo_string node\n", __func__);
			ret = ERR_PTR(-ENOENT);
			goto k_err;
		}
		parse_sysfs_caminfo(pdata, camInfo_np, camera_infos, camera_num);
	}
#endif

	return pdata;

#ifdef CONFIG_CAMERA_SYSFS_V2
k_err:
	kfree(camera_infos);
#endif
p_err:
	kfree(pdata);
	return ret;
}

int fimc_is_sensor_parse_dt(struct platform_device *pdev)
{
	int ret = 0;
	u32 temp;
	char *pprop;
	struct exynos_platform_fimc_is_sensor *pdata;
	struct device_node *dnode;
	struct device *dev;
	const char *name;
	u32 id;

	BUG_ON(!pdev);
	BUG_ON(!pdev->dev.of_node);

	dev = &pdev->dev;
	dnode = dev->of_node;

	pdata = kzalloc(sizeof(struct exynos_platform_fimc_is_sensor), GFP_KERNEL);
	if (!pdata) {
		pr_err("%s: no memory for platform data\n", __func__);
		return -ENOMEM;
	}

	pdata->gpio_cfg = exynos_fimc_is_sensor_pins_cfg;
	pdata->iclk_cfg = exynos_fimc_is_sensor_iclk_cfg;
	pdata->iclk_on = exynos_fimc_is_sensor_iclk_on;
	pdata->iclk_off = exynos_fimc_is_sensor_iclk_off;
	pdata->mclk_on = exynos_fimc_is_sensor_mclk_on;
	pdata->mclk_off = exynos_fimc_is_sensor_mclk_off;

	ret = of_property_read_u32(dnode, "scenario", &pdata->scenario);
	if (ret) {
		err("scenario read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "mclk_ch", &pdata->mclk_ch);
	if (ret) {
		err("mclk_ch read is fail(%d)", ret);
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

	ret = of_property_read_u32(dnode, "i2c_ch", &pdata->i2c_ch);
	if (ret) {
		err("i2c_ch read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "i2c_addr", &pdata->i2c_addr);
	if (ret) {
		err("i2c_addr read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "is_bns", &pdata->is_bns);
	if (ret) {
		err("is_bns read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "id", &id);
	if (ret) {
		err("id read is fail(%d)", ret);
		goto p_err;
	}

	ret = of_property_read_u32(dnode, "actuator_id", &pdata->actuator_id);
	if (ret) {
		pr_info("actuator_id is not exist.(%d)\n", ret);
		pdata->actuator_id = ACTUATOR_NAME_NOTHING;
	}

	ret = of_property_read_u32(dnode, "flash_id", &pdata->flash_id);
	if (ret) {
		pr_info("flash_id is not exist.(%d)\n", ret);
		pdata->flash_id = FLADRV_NAME_NOTHING;
	}

	DT_READ_U32(dnode, "flash_first_gpio", pdata->flash_first_gpio);
	DT_READ_U32(dnode, "flash_second_gpio", pdata->flash_second_gpio);

	ret = of_property_read_string(dnode, "sensor_name", &name);
	if (ret) {
		err("sensor_name read is fail(%d)", ret);
		goto p_err;
	}
	strcpy(pdata->sensor_name, name);

	ret = of_property_read_u32(dnode, "sensor_id", &pdata->sensor_id);
	if (ret) {
		err("sensor_id read is fail(%d)", ret);
		goto p_err;
	}

	dev->platform_data = pdata;

	ret = fimc_is_power_setpin(dev, id, pdata->sensor_id);
	if (ret)
		err("power_setpin failed(%d). id %d", ret, id);

	pdev->id = id;
	pdata->id = id;

	pdata->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(pdata->pinctrl)) {
		err("devm_pinctrl_get is fail");
		goto p_err;
	} else {
		ret = get_pin_lookup_state(dev, pdata);
		if (ret < 0) {
			err("fimc_is_get_pin_lookup_state is fail");
			goto p_err;
		}
	}

	return ret;

p_err:
	kfree(pdata);
	return ret;
}
#else
struct exynos_platform_fimc_is *fimc_is_parse_dt(struct device *dev)
{
	return ERR_PTR(-EINVAL);
}
#endif
