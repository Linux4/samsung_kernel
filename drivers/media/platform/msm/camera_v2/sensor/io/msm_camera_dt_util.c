/* Copyright (c) 2013-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "msm_camera_dt_util.h"
#include "msm_camera_io_util.h"
#include "msm_camera_i2c_mux.h"
#include "msm_cci.h"

#define CAM_SENSOR_PINCTRL_STATE_SLEEP "cam_suspend"
#define CAM_SENSOR_PINCTRL_STATE_DEFAULT "cam_default"
/*#define CONFIG_MSM_CAMERA_DT_DEBUG*/

#define VALIDATE_VOLTAGE(min, max, config_val) ((config_val) && \
	(config_val >= min) && (config_val <= max))

#if defined(CONFIG_COMPANION2) || defined(CONFIG_COMPANION3)
extern bool retention_mode_pwr;
struct regulator *retention_reg;
#endif
#if defined(CONFIG_COMPANION3)
struct regulator *pwr_binning_reg;
#endif
struct regulator *ana_reg;

#undef CDBG
//#define CONFIG_MSM_CAMERA_DT_DEBUG
#ifdef CONFIG_MSM_CAMERA_DT_DEBUG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#endif

int power_state;
int sensor_power_on_cnt;

int msm_camera_fill_vreg_params(struct camera_vreg_t *cam_vreg,
	int num_vreg, struct msm_sensor_power_setting *power_setting,
	uint16_t power_setting_size)
{
	uint16_t i = 0;
	int      j = 0;

	/* Validate input parameters */
	if (!cam_vreg || !power_setting) {
		pr_err("%s:%d failed: cam_vreg %pK power_setting %pK", __func__,
			__LINE__,  cam_vreg, power_setting);
		return -EINVAL;
	}

	/* Validate size of num_vreg */
	if (num_vreg <= 0) {
		pr_err("failed: num_vreg %d", num_vreg);
		return -EINVAL;
	}

	for (i = 0; i < power_setting_size; i++) {
		if (power_setting[i].seq_type != SENSOR_VREG)
			continue;

		switch (power_setting[i].seq_val) {
		case CAM_VDIG:
			for (j = 0; j < num_vreg; j++) {
				if (!strcmp(cam_vreg[j].reg_name, "cam_vdig")) {
					CDBG("%s:%d i %d j %d cam_vdig\n",
						__func__, __LINE__, i, j);
					power_setting[i].seq_val = j;
					if (VALIDATE_VOLTAGE(
						cam_vreg[j].min_voltage,
						cam_vreg[j].max_voltage,
						power_setting[i].config_val)) {
						cam_vreg[j].min_voltage =
						cam_vreg[j].max_voltage =
						power_setting[i].config_val;
					}
					break;
				}
			}
			break;

		case CAM_VIO:
			for (j = 0; j < num_vreg; j++) {
				if (!strcmp(cam_vreg[j].reg_name, "cam_vio")) {
					CDBG("%s:%d i %d j %d cam_vio\n",
						__func__, __LINE__, i, j);
					power_setting[i].seq_val = j;
					if (VALIDATE_VOLTAGE(
						cam_vreg[j].min_voltage,
						cam_vreg[j].max_voltage,
						power_setting[i].config_val)) {
						cam_vreg[j].min_voltage =
						cam_vreg[j].max_voltage =
						power_setting[i].config_val;
					}
					break;
				}
			}
			break;

		case CAM_VANA:
			for (j = 0; j < num_vreg; j++) {
				if (!strcmp(cam_vreg[j].reg_name, "cam_vana")) {
					CDBG("%s:%d i %d j %d cam_vana\n",
						__func__, __LINE__, i, j);
					power_setting[i].seq_val = j;
					if (VALIDATE_VOLTAGE(
						cam_vreg[j].min_voltage,
						cam_vreg[j].max_voltage,
						power_setting[i].config_val)) {
						cam_vreg[j].min_voltage =
						cam_vreg[j].max_voltage =
						power_setting[i].config_val;
					}
					break;
				}
			}
			break;

		case CAM_VAF:
			for (j = 0; j < num_vreg; j++) {
				if (!strcmp(cam_vreg[j].reg_name, "cam_vaf")) {
					CDBG("%s:%d i %d j %d cam_vaf\n",
						__func__, __LINE__, i, j);
					power_setting[i].seq_val = j;
					if (VALIDATE_VOLTAGE(
						cam_vreg[j].min_voltage,
						cam_vreg[j].max_voltage,
						power_setting[i].config_val)) {
						cam_vreg[j].min_voltage =
						cam_vreg[j].max_voltage =
						power_setting[i].config_val;
					}
					break;
				}
			}
			break;

		case CAM_V_CUSTOM1:
			for (j = 0; j < num_vreg; j++) {
				if (!strcmp(cam_vreg[j].reg_name,
					"cam_v_custom1")) {
					CDBG("%s:%d i %d j %d cam_vcustom1\n",
						__func__, __LINE__, i, j);
					power_setting[i].seq_val = j;
					if (VALIDATE_VOLTAGE(
						cam_vreg[j].min_voltage,
						cam_vreg[j].max_voltage,
						power_setting[i].config_val)) {
						cam_vreg[j].min_voltage =
						cam_vreg[j].max_voltage =
						power_setting[i].config_val;
					}
					break;
				}
			}

		case CAM_V_CUSTOM2:
			for (j = 0; j < num_vreg; j++) {
				if (!strcmp(cam_vreg[j].reg_name,
					"cam_v_custom2")) {
					CDBG("%s:%d i %d j %d cam_vcustom2\n",
						__func__, __LINE__, i, j);
					power_setting[i].seq_val = j;
					if (VALIDATE_VOLTAGE(
						cam_vreg[j].min_voltage,
						cam_vreg[j].max_voltage,
						power_setting[i].config_val)) {
						cam_vreg[j].min_voltage =
						cam_vreg[j].max_voltage =
						power_setting[i].config_val;
					}
					break;
				}
			}
			break;

		case CAM_VM_OIS:
			for (j = 0; j < num_vreg; j++) {
				if (!strcmp(cam_vreg[j].reg_name, "cam_vm_ois")) {
					CDBG("%s:%d i %d j %d cam_vm_ois\n",
						__func__, __LINE__, i, j);
					power_setting[i].seq_val = j;
					if (VALIDATE_VOLTAGE(
						cam_vreg[j].min_voltage,
						cam_vreg[j].max_voltage,
						power_setting[i].config_val)) {
						cam_vreg[j].min_voltage =
						cam_vreg[j].max_voltage =
						power_setting[i].config_val;
					}
					break;
				}
			}
			break;

		case CAM_VDD_OIS2:
			for (j = 0; j < num_vreg; j++) {
				if (!strcmp(cam_vreg[j].reg_name, "cam_vdd_ois2")) {
					CDBG("%s:%d i %d j %d cam_vdd_ois2\n",
						__func__, __LINE__, i, j);
					power_setting[i].seq_val = j;
					if (VALIDATE_VOLTAGE(
						cam_vreg[j].min_voltage,
						cam_vreg[j].max_voltage,
						power_setting[i].config_val)) {
						cam_vreg[j].min_voltage =
						cam_vreg[j].max_voltage =
						power_setting[i].config_val;
					}
					break;
				}
			}
			break;

		case CAM_VDD_OIS:
			for (j = 0; j < num_vreg; j++) {
				if (!strcmp(cam_vreg[j].reg_name, "cam_vdd_ois")) {
					CDBG("%s:%d i %d j %d cam_vdd_ois\n",
						__func__, __LINE__, i, j);
					power_setting[i].seq_val = j;
					if (VALIDATE_VOLTAGE(
						cam_vreg[j].min_voltage,
						cam_vreg[j].max_voltage,
						power_setting[i].config_val)) {
						cam_vreg[j].min_voltage =
						cam_vreg[j].max_voltage =
						power_setting[i].config_val;
					}
					break;
				}
			}
			break;

		case CAM_VIO_I2C:
			for (j = 0; j < num_vreg; j++) {
				if (!strcmp(cam_vreg[j].reg_name,
					"cam_vio_i2c")) {
					CDBG("%s:%d i %d j %d cam_vio_i2c\n",
						__func__, __LINE__, i, j);
					power_setting[i].seq_val = j;
					if (VALIDATE_VOLTAGE(
						cam_vreg[j].min_voltage,
						cam_vreg[j].max_voltage,
						power_setting[i].config_val)) {
						cam_vreg[j].min_voltage =
						cam_vreg[j].max_voltage =
						power_setting[i].config_val;
					}
					break;
				}
			}
			break;

#if defined(CONFIG_COMPANION2) || defined(CONFIG_COMPANION3)
		case CAM_VDD_A:
			for (j = 0; j < num_vreg; j++) {
				if (!strcmp(cam_vreg[j].reg_name, "cam_vdd_a")) {
					CDBG("%s:%d i %d j %d cam_vdd_a\n",
						__func__, __LINE__, i, j);
					power_setting[i].seq_val = j;
					if (VALIDATE_VOLTAGE(
						cam_vreg[j].min_voltage,
						cam_vreg[j].max_voltage,
						power_setting[i].config_val)) {
						cam_vreg[j].min_voltage =
						cam_vreg[j].max_voltage =
						power_setting[i].config_val;
					}
					break;
				}
			}
			break;

		case CAM_COMP_MIPI_1P0:
			for (j = 0; j < num_vreg; j++) {
				if (!strcmp(cam_vreg[j].reg_name,
					"cam_vmipi_comp")) {
					CDBG("%s:%d i %d j %d cam_vmipi_comp\n",
						__func__, __LINE__, i, j);
					power_setting[i].seq_val = j;
					if (VALIDATE_VOLTAGE(
						cam_vreg[j].min_voltage,
						cam_vreg[j].max_voltage,
						power_setting[i].config_val)) {
						cam_vreg[j].min_voltage =
						cam_vreg[j].max_voltage =
						power_setting[i].config_val;
					}
					break;
				}
			}
			break;

		case CAM_COMP_1P8:
			for (j = 0; j < num_vreg; j++) {
				if (!strcmp(cam_vreg[j].reg_name,
					"cam_vio_comp")) {
					CDBG("%s:%d i %d j %d cam_vio_comp\n",
						__func__, __LINE__, i, j);
					power_setting[i].seq_val = j;
					if (VALIDATE_VOLTAGE(
						cam_vreg[j].min_voltage,
						cam_vreg[j].max_voltage,
						power_setting[i].config_val)) {
						cam_vreg[j].min_voltage =
						cam_vreg[j].max_voltage =
						power_setting[i].config_val;
					}
					break;
				}
			}
			break;

		case CAM_COMP_VDIG:
			for (j = 0; j < num_vreg; j++) {
				if (!strcmp(cam_vreg[j].reg_name,
					"cam_vdig_comp")) {
					CDBG("%s:%d i %d j %d cam_vdig_comp\n",
						__func__, __LINE__, i, j);
					power_setting[i].seq_val = j;
					if (VALIDATE_VOLTAGE(
						cam_vreg[j].min_voltage,
						cam_vreg[j].max_voltage,
						power_setting[i].config_val)) {
						cam_vreg[j].min_voltage =
						cam_vreg[j].max_voltage =
						power_setting[i].config_val;
					}
					break;
				}
			}
			break;

		case CAM_COMP_VNORET:
			for (j = 0; j < num_vreg; j++) {
				if (!strcmp(cam_vreg[j].reg_name,
					"cam_vnoret_comp")) {
					CDBG("%s:%d i %d j %d cam_vnoret_comp\n",
						__func__, __LINE__, i, j);
					power_setting[i].seq_val = j;
					if (VALIDATE_VOLTAGE(
						cam_vreg[j].min_voltage,
						cam_vreg[j].max_voltage,
						power_setting[i].config_val)) {
						cam_vreg[j].min_voltage =
						cam_vreg[j].max_voltage =
						power_setting[i].config_val;
					}
					break;
				}
			}
			break;


		case CAM_COMP_VANA:
			for (j = 0; j < num_vreg; j++) {
				if (!strcmp(cam_vreg[j].reg_name,
					"cam_vana_comp")) {
					CDBG("%s:%d i %d j %d cam_vana_comp\n",
						__func__, __LINE__, i, j);
					power_setting[i].seq_val = j;
					if (VALIDATE_VOLTAGE(
						cam_vreg[j].min_voltage,
						cam_vreg[j].max_voltage,
						power_setting[i].config_val)) {
						cam_vreg[j].min_voltage =
						cam_vreg[j].max_voltage =
						power_setting[i].config_val;
					}
					break;
				}
			}
			break;

		case CAM_COMP_VRET:
			for (j = 0; j < num_vreg; j++) {
				if (!strcmp(cam_vreg[j].reg_name,
					"cam_vret_comp")) {
					CDBG("%s:%d i %d j %d cam_vret_comp\n",
						__func__, __LINE__, i, j);
					power_setting[i].seq_val = j;
					if (VALIDATE_VOLTAGE(
						cam_vreg[j].min_voltage,
						cam_vreg[j].max_voltage,
						power_setting[i].config_val)) {
						cam_vreg[j].min_voltage =
						cam_vreg[j].max_voltage =
						power_setting[i].config_val;
					}
					break;
				}
			}
			break;

		case CAM_COMP_VRET_RETMODE:
			for (j = 0; j < num_vreg; j++) {
				if (!strcmp(cam_vreg[j].reg_name,
					"cam_retmode_vret_comp")) {
					CDBG("%s:%d i %d j %d cam_retmode_vret_comp\n",
						__func__, __LINE__, i, j);
					power_setting[i].seq_val = j;
					if (VALIDATE_VOLTAGE(
						cam_vreg[j].min_voltage,
						cam_vreg[j].max_voltage,
						power_setting[i].config_val)) {
						cam_vreg[j].min_voltage =
						cam_vreg[j].max_voltage =
						power_setting[i].config_val;
					}
					break;
				}
			}
			break;

		case CAM_COMP_VI2C:
			for (j = 0; j < num_vreg; j++) {
				if (!strcmp(cam_vreg[j].reg_name,
					"cam_vi2c_comp")) {
					CDBG("%s:%d i %d j %d cam_vi2c_comp\n",
						__func__, __LINE__, i, j);
					power_setting[i].seq_val = j;
					if (VALIDATE_VOLTAGE(
						cam_vreg[j].min_voltage,
						cam_vreg[j].max_voltage,
						power_setting[i].config_val)) {
						cam_vreg[j].min_voltage =
						cam_vreg[j].max_voltage =
						power_setting[i].config_val;
					}
					break;
				}
			}
			break;
#endif

		default:
			pr_err("%s:%d invalid seq_val %d\n", __func__,
				__LINE__, power_setting[i].seq_val);
			break;
		}
	}
	return 0;
}

int msm_sensor_get_sub_module_index(struct device_node *of_node,
				    struct  msm_sensor_info_t **s_info)
{
	int rc = 0, i = 0;
	uint32_t val = 0, count = 0;
	uint32_t *val_array = NULL;
	struct device_node *src_node = NULL;
	struct msm_sensor_info_t *sensor_info;

	sensor_info = kzalloc(sizeof(*sensor_info), GFP_KERNEL);
	if (!sensor_info) {
		pr_err("%s:%d failed\n", __func__, __LINE__);
		return -ENOMEM;
	}
	for (i = 0; i < SUB_MODULE_MAX; i++) {
		sensor_info->subdev_id[i] = -1;
		/* Subdev expose additional interface for same sub module*/
		sensor_info->subdev_intf[i] = -1;
	}

	src_node = of_parse_phandle(of_node, "qcom,actuator-src", 0);
	if (!src_node) {
		CDBG("%s:%d src_node NULL\n", __func__, __LINE__);
	} else {
		rc = of_property_read_u32(src_node, "cell-index", &val);
		CDBG("%s qcom,actuator cell index %d, rc %d\n", __func__,
			val, rc);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			goto ERROR;
		}
		sensor_info->subdev_id[SUB_MODULE_ACTUATOR] = val;
		of_node_put(src_node);
		src_node = NULL;
	}

	src_node = of_parse_phandle(of_node, "qcom,ois-src", 0);
	if (!src_node) {
		CDBG("%s:%d src_node NULL\n", __func__, __LINE__);
	} else {
		rc = of_property_read_u32(src_node, "cell-index", &val);
		CDBG("%s qcom,ois cell index %d, rc %d\n", __func__,
			val, rc);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			goto ERROR;
		}
		sensor_info->subdev_id[SUB_MODULE_OIS] = val;
		of_node_put(src_node);
		src_node = NULL;
	}

	src_node = of_parse_phandle(of_node, "qcom,eeprom-src", 0);
	if (!src_node) {
		CDBG("%s:%d eeprom src_node NULL\n", __func__, __LINE__);
	} else {
		rc = of_property_read_u32(src_node, "cell-index", &val);
		CDBG("%s qcom,eeprom cell index %d, rc %d\n", __func__,
			val, rc);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			goto ERROR;
		}
		sensor_info->subdev_id[SUB_MODULE_EEPROM] = val;
		of_node_put(src_node);
		src_node = NULL;
	}

	rc = of_property_read_u32(of_node, "qcom,eeprom-sd-index", &val);
	if (rc != -EINVAL) {
		CDBG("%s qcom,eeprom-sd-index %d, rc %d\n", __func__, val, rc);
		if (rc < 0) {
			pr_err("%s:%d failed rc %d\n", __func__, __LINE__, rc);
			goto ERROR;
		}
		sensor_info->subdev_id[SUB_MODULE_EEPROM] = val;
	} else {
		rc = 0;
	}

	src_node = of_parse_phandle(of_node, "qcom,led-flash-src", 0);
	if (!src_node) {
		CDBG("%s:%d src_node NULL\n", __func__, __LINE__);
	} else {
		rc = of_property_read_u32(src_node, "cell-index", &val);
		CDBG("%s qcom,led flash cell index %d, rc %d\n", __func__,
			val, rc);
		if (rc < 0) {
			pr_err("%s:%d failed %d\n", __func__, __LINE__, rc);
			goto ERROR;
		}
		sensor_info->subdev_id[SUB_MODULE_LED_FLASH] = val;
		of_node_put(src_node);
		src_node = NULL;
	}

	rc = of_property_read_u32(of_node, "qcom,strobe-flash-sd-index", &val);
	if (rc != -EINVAL) {
		CDBG("%s qcom,strobe-flash-sd-index %d, rc %d\n", __func__,
			val, rc);
		if (rc < 0) {
			pr_err("%s:%d failed rc %d\n", __func__, __LINE__, rc);
			goto ERROR;
		}
		sensor_info->subdev_id[SUB_MODULE_STROBE_FLASH] = val;
	} else {
		rc = 0;
	}

	if (of_get_property(of_node, "qcom,csiphy-sd-index", &count)) {
		count /= sizeof(uint32_t);
		if (count > 2) {
			pr_err("%s qcom,csiphy-sd-index count %d > 2\n",
				__func__, count);
			goto ERROR;
		}
		val_array = kzalloc(sizeof(uint32_t) * count, GFP_KERNEL);
		if (!val_array) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			rc = -ENOMEM;
			goto ERROR;
		}

		rc = of_property_read_u32_array(of_node, "qcom,csiphy-sd-index",
			val_array, count);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			kfree(val_array);
			goto ERROR;
		}
		for (i = 0; i < count; i++) {
			sensor_info->subdev_id[SUB_MODULE_CSIPHY + i] =
								val_array[i];
			CDBG("%s csiphy_core[%d] = %d\n",
				__func__, i, val_array[i]);
		}
		kfree(val_array);
	} else {
		pr_err("%s:%d qcom,csiphy-sd-index not present\n", __func__,
			__LINE__);
		rc = -EINVAL;
		goto ERROR;
	}

	if (of_get_property(of_node, "qcom,csid-sd-index", &count)) {
		count /= sizeof(uint32_t);
		if (count > 2) {
			pr_err("%s qcom,csid-sd-index count %d > 2\n",
				__func__, count);
			rc = -EINVAL;
			goto ERROR;
		}
		val_array = kzalloc(sizeof(uint32_t) * count, GFP_KERNEL);
		if (!val_array) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			rc = -ENOMEM;
			goto ERROR;
		}

		rc = of_property_read_u32_array(of_node, "qcom,csid-sd-index",
			val_array, count);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			kfree(val_array);
			goto ERROR;
		}
		for (i = 0; i < count; i++) {
			sensor_info->subdev_id
				[SUB_MODULE_CSID + i] = val_array[i];
			CDBG("%s csid_core[%d] = %d\n",
				__func__, i, val_array[i]);
		}
		kfree(val_array);
	} else {
		pr_err("%s:%d qcom,csid-sd-index not present\n", __func__,
			__LINE__);
		rc = -EINVAL;
		goto ERROR;
	}

#if defined(CONFIG_COMPANION2) || defined(CONFIG_COMPANION3)
	if(of_get_property(of_node, "qcom,companion-src", &count)) {
	CDBG("%s: %d count is %d\n", __func__, __LINE__, count);
		count /= sizeof(uint32_t);
		if (count > 2) {
			pr_err("%s qcom,companion count %d > 2\n",
				__func__, count);
			goto ERROR;
		}
		for (i = 0; i < count; i++) {
			src_node = of_parse_phandle(of_node,
				"qcom,companion-src", i);
			if (!src_node) {
				pr_err("%s:%d src_node NULL\n",
					__func__, __LINE__);
				goto ERROR;
			} else {
				rc = of_property_read_u32(
					src_node, "cell-index", &val);
				pr_err("%s qcom,companion cell index %d, rc %d\n",
					__func__, val, rc);
				if (rc < 0) {
					pr_err("%s failed %d\n", __func__, __LINE__);
					goto ERROR;
				}
				if (i == 0)
					sensor_info->
						subdev_id[SUB_MODULE_COMPANION]
						= val;
				else
					sensor_info->
						subdev_intf[SUB_MODULE_COMPANION]
						= val;
				of_node_put(src_node);
				src_node = NULL;
			}
		}
	}
#endif

	*s_info = sensor_info;
	return rc;
ERROR:
	kfree(sensor_info);
	return rc;
}

int msm_sensor_get_dt_actuator_data(struct device_node *of_node,
				    struct msm_actuator_info **act_info)
{
	int rc = 0;
	uint32_t val = 0;
	struct msm_actuator_info *actuator_info;

	rc = of_property_read_u32(of_node, "qcom,actuator-cam-name", &val);
	CDBG("%s qcom,actuator-cam-name %d, rc %d\n", __func__, val, rc);
	if (rc < 0)
		return 0;

	actuator_info = kzalloc(sizeof(*actuator_info), GFP_KERNEL);
	if (!actuator_info) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		rc = -ENOMEM;
		goto ERROR;
	}

	actuator_info->cam_name = val;

	rc = of_property_read_u32(of_node, "qcom,actuator-vcm-pwd", &val);
	CDBG("%s qcom,actuator-vcm-pwd %d, rc %d\n", __func__, val, rc);
	if (!rc)
		actuator_info->vcm_pwd = val;

	rc = of_property_read_u32(of_node, "qcom,actuator-vcm-enable", &val);
	CDBG("%s qcom,actuator-vcm-enable %d, rc %d\n", __func__, val, rc);
	if (!rc)
		actuator_info->vcm_enable = val;

	*act_info = actuator_info;
	return 0;
ERROR:
	kfree(actuator_info);
	return rc;
}

int msm_sensor_get_dt_csi_data(struct device_node *of_node,
	struct msm_camera_csi_lane_params **csi_lane_params)
{
	int rc = 0;
	uint32_t val = 0;
	struct msm_camera_csi_lane_params *clp;

	clp = kzalloc(sizeof(*clp), GFP_KERNEL);
	if (!clp) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		return -ENOMEM;
	}
	*csi_lane_params = clp;

	rc = of_property_read_u32(of_node, "qcom,csi-lane-assign", &val);
	CDBG("%s qcom,csi-lane-assign 0x%x, rc %d\n", __func__, val, rc);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR;
	}
	clp->csi_lane_assign = val;

	rc = of_property_read_u32(of_node, "qcom,csi-lane-mask", &val);
	CDBG("%s qcom,csi-lane-mask 0x%x, rc %d\n", __func__, val, rc);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR;
	}
	clp->csi_lane_mask = val;

	return rc;
ERROR:
	kfree(clp);
	return rc;
}

int msm_camera_get_dt_power_setting_data(struct device_node *of_node,
	struct camera_vreg_t *cam_vreg, int num_vreg,
	struct msm_camera_power_ctrl_t *power_info)
{
	int rc = 0, i, j;
	int count = 0;
	const char *seq_name = NULL;
	uint32_t *array = NULL;
	struct msm_sensor_power_setting *ps;

	struct msm_sensor_power_setting *power_setting;
	uint16_t *power_setting_size, size = 0;
	bool need_reverse = 0;

	if (!power_info)
		return -EINVAL;

	power_setting = power_info->power_setting;
	power_setting_size = &power_info->power_setting_size;

	count = of_property_count_strings(of_node, "qcom,cam-power-seq-type");
	*power_setting_size = count;

	CDBG("%s qcom,cam-power-seq-type count %d\n", __func__, count);

	if (count <= 0)
		return 0;

	ps = kzalloc(sizeof(*ps) * count, GFP_KERNEL);
	if (!ps) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		return -ENOMEM;
	}
	power_setting = ps;
	power_info->power_setting = ps;

	for (i = 0; i < count; i++) {
		rc = of_property_read_string_index(of_node,
			"qcom,cam-power-seq-type", i,
			&seq_name);
		CDBG("%s seq_name[%d] = %s\n", __func__, i,
			seq_name);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			goto ERROR1;
		}
		if (!strcmp(seq_name, "sensor_vreg")) {
			ps[i].seq_type = SENSOR_VREG;
			CDBG("%s:%d seq_type[%d] %d\n", __func__, __LINE__,
				i, ps[i].seq_type);
		} else if (!strcmp(seq_name, "sensor_gpio")) {
			ps[i].seq_type = SENSOR_GPIO;
			CDBG("%s:%d seq_type[%d] %d\n", __func__, __LINE__,
				i, ps[i].seq_type);
		} else if (!strcmp(seq_name, "sensor_clk")) {
			ps[i].seq_type = SENSOR_CLK;
			CDBG("%s:%d seq_type[%d] %d\n", __func__, __LINE__,
				i, ps[i].seq_type);
		} else if (!strcmp(seq_name, "sensor_i2c_mux")) {
			ps[i].seq_type = SENSOR_I2C_MUX;
			CDBG("%s:%d seq_type[%d] %d\n", __func__, __LINE__,
				i, ps[i].seq_type);
		} else {
			CDBG("%s: unrecognized seq-type\n", __func__);
			rc = -EILSEQ;
			goto ERROR1;
		}
	}


	for (i = 0; i < count; i++) {
		rc = of_property_read_string_index(of_node,
			"qcom,cam-power-seq-val", i,
			&seq_name);
		CDBG("%s seq_name[%d] = %s\n", __func__, i,
			seq_name);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			goto ERROR1;
		}
		switch (ps[i].seq_type) {
		case SENSOR_VREG:
			for (j = 0; j < num_vreg; j++) {
				if (!strcmp(seq_name, cam_vreg[j].reg_name))
					break;
			}
			if (j < num_vreg)
				ps[i].seq_val = j;
			else
				rc = -EILSEQ;
			break;
		case SENSOR_GPIO:
			if (!strcmp(seq_name, "sensor_gpio_reset"))
				ps[i].seq_val = SENSOR_GPIO_RESET;
			else if (!strcmp(seq_name, "sensor_gpio_standby"))
				ps[i].seq_val = SENSOR_GPIO_STANDBY;
			else if (!strcmp(seq_name, "sensor_gpio_vdig"))
				ps[i].seq_val = SENSOR_GPIO_VDIG;
			else if (!strcmp(seq_name, "sensor_gpio_vana"))
				ps[i].seq_val = SENSOR_GPIO_VANA;
			else if (!strcmp(seq_name, "sensor_gpio_vaf"))
				ps[i].seq_val = SENSOR_GPIO_VAF;
			else if (!strcmp(seq_name, "sensor_gpio_vio"))
				ps[i].seq_val = SENSOR_GPIO_VIO;
			else if (!strcmp(seq_name, "sensor_gpio_custom1"))
				ps[i].seq_val = SENSOR_GPIO_CUSTOM1;
			else if (!strcmp(seq_name, "sensor_gpio_custom2"))
				ps[i].seq_val = SENSOR_GPIO_CUSTOM2;
			else
				rc = -EILSEQ;
			break;
		case SENSOR_CLK:
			if (!strcmp(seq_name, "sensor_cam_mclk"))
				ps[i].seq_val = SENSOR_CAM_MCLK;
			else if (!strcmp(seq_name, "sensor_cam_clk"))
				ps[i].seq_val = SENSOR_CAM_CLK;
			else
				rc = -EILSEQ;
			break;
		case SENSOR_I2C_MUX:
			if (!strcmp(seq_name, "none"))
				ps[i].seq_val = 0;
			else
				rc = -EILSEQ;
			break;
		default:
			rc = -EILSEQ;
			break;
		}
		if (rc < 0) {
			CDBG("%s: unrecognized seq-val\n", __func__);
			goto ERROR1;
		}
	}

	array = kzalloc(sizeof(uint32_t) * count, GFP_KERNEL);
	if (!array) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		rc = -ENOMEM;
		goto ERROR1;
	}


	rc = of_property_read_u32_array(of_node, "qcom,cam-power-seq-cfg-val",
		array, count);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR2;
	}
	for (i = 0; i < count; i++) {
		if (ps[i].seq_type == SENSOR_GPIO) {
			if (array[i] == 0)
				ps[i].config_val = GPIO_OUT_LOW;
			else if (array[i] == 1)
				ps[i].config_val = GPIO_OUT_HIGH;
		} else {
			ps[i].config_val = array[i];
		}
		CDBG("%s power_setting[%d].config_val = %ld\n", __func__, i,
			ps[i].config_val);
	}

	rc = of_property_read_u32_array(of_node, "qcom,cam-power-seq-delay",
		array, count);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR2;
	}
	for (i = 0; i < count; i++) {
		ps[i].delay = array[i];
		CDBG("%s power_setting[%d].delay = %d\n", __func__,
			i, ps[i].delay);
	}
	kfree(array);

	size = *power_setting_size;

	if (NULL != ps && 0 != size)
		need_reverse = 1;

	power_info->power_down_setting =
		kzalloc(sizeof(*ps) * size, GFP_KERNEL);

	if (!power_info->power_down_setting) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		rc = -ENOMEM;
		goto ERROR1;
	}

	memcpy(power_info->power_down_setting,
		ps, sizeof(*ps) * size);

	power_info->power_down_setting_size = size;

	if (need_reverse) {
		int c, end = size - 1;
		struct msm_sensor_power_setting power_down_setting_t;
		for (c = 0; c < size/2; c++) {
			power_down_setting_t =
				power_info->power_down_setting[c];
			power_info->power_down_setting[c] =
				power_info->power_down_setting[end];
			power_info->power_down_setting[end] =
				power_down_setting_t;
			end--;
		}
	}
	return rc;
ERROR2:
	kfree(array);
ERROR1:
	kfree(ps);
	power_setting_size = 0;
	return rc;
}

int msm_camera_get_dt_gpio_req_tbl(struct device_node *of_node,
	struct msm_camera_gpio_conf *gconf, uint16_t *gpio_array,
	uint16_t gpio_array_size)
{
	int rc = 0, i = 0;
	uint32_t count = 0;
	uint32_t *val_array = NULL;

	if (!of_get_property(of_node, "qcom,gpio-req-tbl-num", &count))
		return 0;

	count /= sizeof(uint32_t);
	if (!count) {
		pr_err("%s qcom,gpio-req-tbl-num 0\n", __func__);
		return 0;
	}

	val_array = kzalloc(sizeof(uint32_t) * count, GFP_KERNEL);
	if (!val_array) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		return -ENOMEM;
	}

	gconf->cam_gpio_req_tbl = kzalloc(sizeof(struct gpio) * count,
		GFP_KERNEL);
	if (!gconf->cam_gpio_req_tbl) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		rc = -ENOMEM;
		goto ERROR1;
	}
	gconf->cam_gpio_req_tbl_size = count;

	rc = of_property_read_u32_array(of_node, "qcom,gpio-req-tbl-num",
		val_array, count);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR2;
	}
	for (i = 0; i < count; i++) {
		if (val_array[i] >= gpio_array_size) {
			pr_err("%s gpio req tbl index %d invalid\n",
				__func__, val_array[i]);
			return -EINVAL;
		}
		gconf->cam_gpio_req_tbl[i].gpio = gpio_array[val_array[i]];
		CDBG("%s cam_gpio_req_tbl[%d].gpio = %d\n", __func__, i,
			gconf->cam_gpio_req_tbl[i].gpio);
	}

	rc = of_property_read_u32_array(of_node, "qcom,gpio-req-tbl-flags",
		val_array, count);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR2;
	}
	for (i = 0; i < count; i++) {
		gconf->cam_gpio_req_tbl[i].flags = val_array[i];
		CDBG("%s cam_gpio_req_tbl[%d].flags = %ld\n", __func__, i,
			gconf->cam_gpio_req_tbl[i].flags);
	}

	for (i = 0; i < count; i++) {
		rc = of_property_read_string_index(of_node,
			"qcom,gpio-req-tbl-label", i,
			&gconf->cam_gpio_req_tbl[i].label);
		CDBG("%s cam_gpio_req_tbl[%d].label = %s\n", __func__, i,
			gconf->cam_gpio_req_tbl[i].label);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			goto ERROR2;
		}
	}

	kfree(val_array);
	return rc;

ERROR2:
	kfree(gconf->cam_gpio_req_tbl);
ERROR1:
	kfree(val_array);
	gconf->cam_gpio_req_tbl_size = 0;
	return rc;
}

int msm_camera_init_gpio_pin_tbl(struct device_node *of_node,
	struct msm_camera_gpio_conf *gconf, uint16_t *gpio_array,
	uint16_t gpio_array_size)
{
	int rc = 0, val = 0;

	gconf->gpio_num_info = kzalloc(sizeof(struct msm_camera_gpio_num_info),
		GFP_KERNEL);
	if (!gconf->gpio_num_info) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		rc = -ENOMEM;
		return rc;
	}
	rc = of_property_read_u32(of_node, "qcom,gpio-vana", &val);
	if (rc != -EINVAL) {
		if (rc < 0) {
			pr_err("%s:%d read qcom,gpio-vana failed rc %d\n",
				__func__, __LINE__, rc);
			goto ERROR;
		} else if (val >= gpio_array_size) {
			pr_err("%s:%d qcom,gpio-vana invalid %d\n",
				__func__, __LINE__, val);
			rc = -EINVAL;
			goto ERROR;
		}
		gconf->gpio_num_info->gpio_num[SENSOR_GPIO_VANA] =
			gpio_array[val];
		gconf->gpio_num_info->valid[SENSOR_GPIO_VANA] = 1;
		CDBG("%s qcom,gpio-vana %d\n", __func__,
			gconf->gpio_num_info->gpio_num[SENSOR_GPIO_VANA]);
	} else {
		rc = 0;
	}

	rc = of_property_read_u32(of_node, "qcom,gpio-vio", &val);
	if (rc != -EINVAL) {
		if (rc < 0) {
			pr_err("%s:%d read qcom,gpio-vio failed rc %d\n",
				__func__, __LINE__, rc);
			goto ERROR;
		} else if (val >= gpio_array_size) {
			pr_err("%s:%d qcom,gpio-vio invalid %d\n",
				__func__, __LINE__, val);
			goto ERROR;
		}
		gconf->gpio_num_info->gpio_num[SENSOR_GPIO_VIO] =
			gpio_array[val];
		gconf->gpio_num_info->valid[SENSOR_GPIO_VIO] = 1;
		CDBG("%s qcom,gpio-vio %d\n", __func__,
			gconf->gpio_num_info->gpio_num[SENSOR_GPIO_VIO]);
	} else {
		rc = 0;
	}

	rc = of_property_read_u32(of_node, "qcom,gpio-vaf", &val);
	if (rc != -EINVAL) {
		if (rc < 0) {
			pr_err("%s:%d read qcom,gpio-vaf failed rc %d\n",
				__func__, __LINE__, rc);
			goto ERROR;
		} else if (val >= gpio_array_size) {
			pr_err("%s:%d qcom,gpio-vaf invalid %d\n",
				__func__, __LINE__, val);
			rc = -EINVAL;
			goto ERROR;
		}
		gconf->gpio_num_info->gpio_num[SENSOR_GPIO_VAF] =
			gpio_array[val];
		gconf->gpio_num_info->valid[SENSOR_GPIO_VAF] = 1;
		CDBG("%s qcom,gpio-vaf %d\n", __func__,
			gconf->gpio_num_info->gpio_num[SENSOR_GPIO_VAF]);
	} else {
		rc = 0;
	}

	rc = of_property_read_u32(of_node, "qcom,gpio-vdig", &val);
	if (rc != -EINVAL) {
		if (rc < 0) {
			pr_err("%s:%d read qcom,gpio-vdig failed rc %d\n",
				__func__, __LINE__, rc);
			goto ERROR;
		} else if (val >= gpio_array_size) {
			pr_err("%s:%d qcom,gpio-vdig invalid %d\n",
				__func__, __LINE__, val);
			rc = -EINVAL;
			goto ERROR;
		}
		gconf->gpio_num_info->gpio_num[SENSOR_GPIO_VDIG] =
			gpio_array[val];
		gconf->gpio_num_info->valid[SENSOR_GPIO_VDIG] = 1;
		CDBG("%s qcom,gpio-vdig %d\n", __func__,
			gconf->gpio_num_info->gpio_num[SENSOR_GPIO_VDIG]);
	} else {
		rc = 0;
	}

	rc = of_property_read_u32(of_node, "qcom,gpio-reset", &val);
	if (rc != -EINVAL) {
		if (rc < 0) {
			pr_err("%s:%d read qcom,gpio-reset failed rc %d\n",
				__func__, __LINE__, rc);
			goto ERROR;
		} else if (val >= gpio_array_size) {
			pr_err("%s:%d qcom,gpio-reset invalid %d\n",
				__func__, __LINE__, val);
			rc = -EINVAL;
			goto ERROR;
		}
		gconf->gpio_num_info->gpio_num[SENSOR_GPIO_RESET] =
			gpio_array[val];
		gconf->gpio_num_info->valid[SENSOR_GPIO_RESET] = 1;
		CDBG("%s qcom,gpio-reset %d\n", __func__,
			gconf->gpio_num_info->gpio_num[SENSOR_GPIO_RESET]);
	} else {
		rc = 0;
	}

	rc = of_property_read_u32(of_node, "qcom,gpio-standby", &val);
	if (rc != -EINVAL) {
		if (rc < 0) {
			pr_err("%s:%d read qcom,gpio-standby failed rc %d\n",
				__func__, __LINE__, rc);
			goto ERROR;
		} else if (val >= gpio_array_size) {
			pr_err("%s:%d qcom,gpio-standby invalid %d\n",
				__func__, __LINE__, val);
			rc = -EINVAL;
			goto ERROR;
		}
		gconf->gpio_num_info->gpio_num[SENSOR_GPIO_STANDBY] =
			gpio_array[val];
		gconf->gpio_num_info->valid[SENSOR_GPIO_STANDBY] = 1;
		CDBG("%s qcom,gpio-standby %d\n", __func__,
			gconf->gpio_num_info->gpio_num[SENSOR_GPIO_STANDBY]);
	} else {
		rc = 0;
	}

	rc = of_property_read_u32(of_node, "qcom,gpio-af-pwdm", &val);
	if (rc != -EINVAL) {
		if (rc < 0) {
			pr_err("%s:%d read qcom,gpio-af-pwdm failed rc %d\n",
				__func__, __LINE__, rc);
			goto ERROR;
		} else if (val >= gpio_array_size) {
			pr_err("%s:%d qcom,gpio-af-pwdm invalid %d\n",
				__func__, __LINE__, val);
			rc = -EINVAL;
			goto ERROR;
		}
		gconf->gpio_num_info->gpio_num[SENSOR_GPIO_AF_PWDM] =
			gpio_array[val];
		gconf->gpio_num_info->valid[SENSOR_GPIO_AF_PWDM] = 1;
		CDBG("%s qcom,gpio-af-pwdm %d\n", __func__,
			gconf->gpio_num_info->gpio_num[SENSOR_GPIO_AF_PWDM]);
	} else {
		rc = 0;
	}

	rc = of_property_read_u32(of_node, "qcom,gpio-flash-en", &val);
	if (rc != -EINVAL) {
		if (rc < 0) {
			pr_err("%s:%d read qcom,gpio-flash-en failed rc %d\n",
				__func__, __LINE__, rc);
			goto ERROR;
		} else if (val >= gpio_array_size) {
			pr_err("%s:%d qcom,gpio-flash-en invalid %d\n",
				__func__, __LINE__, val);
			rc = -EINVAL;
			goto ERROR;
		}
		gconf->gpio_num_info->gpio_num[SENSOR_GPIO_FL_EN] =
			gpio_array[val];
		gconf->gpio_num_info->valid[SENSOR_GPIO_FL_EN] = 1;
		CDBG("%s qcom,gpio-flash-en %d\n", __func__,
			gconf->gpio_num_info->gpio_num[SENSOR_GPIO_FL_EN]);
	} else {
		rc = 0;
	}

	rc = of_property_read_u32(of_node, "qcom,gpio-flash-now", &val);
	if (rc != -EINVAL) {
		if (rc < 0) {
			pr_err("%s:%d read qcom,gpio-flash-now failed rc %d\n",
				__func__, __LINE__, rc);
			goto ERROR;
		} else if (val >= gpio_array_size) {
			pr_err("%s:%d qcom,gpio-flash-now invalid %d\n",
				__func__, __LINE__, val);
			rc = -EINVAL;
			goto ERROR;
		}
		gconf->gpio_num_info->gpio_num[SENSOR_GPIO_FL_NOW] =
			gpio_array[val];
		gconf->gpio_num_info->valid[SENSOR_GPIO_FL_NOW] = 1;
		CDBG("%s qcom,gpio-flash-now %d\n", __func__,
			gconf->gpio_num_info->gpio_num[SENSOR_GPIO_FL_NOW]);
	} else {
		rc = 0;
	}

	rc = of_property_read_u32(of_node, "qcom,gpio-flash-reset", &val);
	if (rc != -EINVAL) {
		if (rc < 0) {
			pr_err("%s:%dread qcom,gpio-flash-reset failed rc %d\n",
				__func__, __LINE__, rc);
			goto ERROR;
		} else if (val >= gpio_array_size) {
			pr_err("%s:%d qcom,gpio-flash-reset invalid %d\n",
				__func__, __LINE__, val);
			rc = -EINVAL;
			goto ERROR;
		}
		gconf->gpio_num_info->gpio_num[SENSOR_GPIO_FL_RESET] =
			gpio_array[val];
		gconf->gpio_num_info->valid[SENSOR_GPIO_FL_RESET] = 1;
		CDBG("%s qcom,gpio-flash-reset %d\n", __func__,
			gconf->gpio_num_info->gpio_num[SENSOR_GPIO_FL_RESET]);
	} else
		rc = 0;

	rc = of_property_read_u32(of_node, "qcom,gpio-custom1", &val);
	if (rc != -EINVAL) {
		if (rc < 0) {
			pr_err("%s:%d read qcom,gpio-custom1 failed rc %d\n",
				__func__, __LINE__, rc);
			goto ERROR;
		} else if (val >= gpio_array_size) {
			pr_err("%s:%d qcom,gpio-custom1 invalid %d\n",
				__func__, __LINE__, val);
			rc = -EINVAL;
			goto ERROR;
		}
		gconf->gpio_num_info->gpio_num[SENSOR_GPIO_CUSTOM1] =
			gpio_array[val];
		gconf->gpio_num_info->valid[SENSOR_GPIO_CUSTOM1] = 1;
		CDBG("%s qcom,gpio-custom1 %d\n", __func__,
			gconf->gpio_num_info->gpio_num[SENSOR_GPIO_CUSTOM1]);
	} else {
		rc = 0;
	}

	rc = of_property_read_u32(of_node, "qcom,gpio-custom2", &val);
	if (rc != -EINVAL) {
		if (rc < 0) {
			pr_err("%s:%d read qcom,gpio-custom2 failed rc %d\n",
				__func__, __LINE__, rc);
			goto ERROR;
		} else if (val >= gpio_array_size) {
			pr_err("%s:%d qcom,gpio-custom2 invalid %d\n",
				__func__, __LINE__, val);
			rc = -EINVAL;
			goto ERROR;
		}
		gconf->gpio_num_info->gpio_num[SENSOR_GPIO_CUSTOM2] =
			gpio_array[val];
		gconf->gpio_num_info->valid[SENSOR_GPIO_CUSTOM2] = 1;
		CDBG("%s qcom,gpio-custom2 %d\n", __func__,
			gconf->gpio_num_info->gpio_num[SENSOR_GPIO_CUSTOM2]);
	} else {
		rc = 0;
	}

	rc = of_property_read_u32(of_node, "qcom,gpio-core-en", &val);
	if (rc != -EINVAL) {
		if (rc < 0) {
			pr_err("%s:%d read qcom,gpio-core-en failed rc %d\n",
				__func__, __LINE__, rc);
			goto ERROR;
		} else if (val >= gpio_array_size) {
			pr_err("%s:%d qcom,gpio-core-en invalid %d\n",
				__func__, __LINE__, val);
			rc = -EINVAL;
			goto ERROR;
		}
		gconf->gpio_num_info->gpio_num[SENSOR_GPIO_CORE_EN] =
			gpio_array[val];
		gconf->gpio_num_info->valid[SENSOR_GPIO_CORE_EN] = 1;
		pr_err("%s qcom,gpio-core-en %d\n", __func__,
			gconf->gpio_num_info->gpio_num[SENSOR_GPIO_CORE_EN]);
	} else {
		rc = 0;
	}

#if defined(CONFIG_COMPANION2) || defined(CONFIG_COMPANION3)
	rc = of_property_read_u32(of_node, "qcom,gpio-comp", &val);
	if (rc != -EINVAL) {
		if (rc < 0) {
			pr_err("%s:%d read qcom,gpio-comp failed rc %d\n",
				__func__, __LINE__, rc);
			goto ERROR;
		} else if (val >= gpio_array_size) {
			pr_err("%s:%d qcom,gpio-comp invalid %d\n",
				__func__, __LINE__, val);
			rc = -EINVAL;
			goto ERROR;
		}
		gconf->gpio_num_info->gpio_num[SENSOR_GPIO_COMP] =
			gpio_array[val];
		gconf->gpio_num_info->valid[SENSOR_GPIO_COMP] = 1;
		CDBG("%s qcom,gpio-comp %d\n", __func__,
			gconf->gpio_num_info->gpio_num[SENSOR_GPIO_COMP]);
	} else {
		rc = 0;
	}

	rc = of_property_read_u32(of_node, "qcom,gpio-comprstn", &val);
	if (rc != -EINVAL) {
		if (rc < 0) {
			pr_err("%s:%d read qcom,gpio-comprstn failed rc %d\n",
				__func__, __LINE__, rc);
			goto ERROR;
		} else if (val >= gpio_array_size) {
			pr_err("%s:%d qcom,gpio-comprstn invalid %d\n",
				__func__, __LINE__, val);
			rc = -EINVAL;
			goto ERROR;
		}
		gconf->gpio_num_info->gpio_num[SENSOR_GPIO_COMPRSTN] =
			gpio_array[val];
		gconf->gpio_num_info->valid[SENSOR_GPIO_COMPRSTN] = 1;
		CDBG("%s qcom,gpio-comprstn %d\n", __func__,
			gconf->gpio_num_info->gpio_num[SENSOR_GPIO_COMPRSTN]);
	} else {
		rc = 0;
	}
#endif
	return rc;

ERROR:
	kfree(gconf->gpio_num_info);
	gconf->gpio_num_info = NULL;
	return rc;
}

#if defined (CONFIG_CAMERA_SYSFS_V2)
int msm_camera_get_dt_camera_info(struct device_node *of_node, char *buf)
{
	int rc = 0, val = 0;
	char camera_info[100] = {0, };

	rc = of_property_read_u32(of_node, "cam,isp",
			&val);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR1;
	}
	strcpy(camera_info, "ISP=");
	switch(val) {
		case CAM_INFO_ISP_TYPE_INTERNAL :
			strcat(camera_info, "INT;");
			break;
		case CAM_INFO_ISP_TYPE_EXTERNAL :
			strcat(camera_info, "EXT;");
			break;
		case CAM_INFO_ISP_TYPE_SOC :
			strcat(camera_info, "SOC;");
			break;
		default :
			strcat(camera_info, "NULL;");
			break;
	}

	rc = of_property_read_u32(of_node, "cam,cal_memory",
			&val);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR1;
	}
	strcat(camera_info, "CALMEM=");
	switch(val) {
		case CAM_INFO_CAL_MEM_TYPE_NONE :
			strcat(camera_info, "N;");
			break;
		case CAM_INFO_CAL_MEM_TYPE_FROM :
		case CAM_INFO_CAL_MEM_TYPE_EEPROM :
		case CAM_INFO_CAL_MEM_TYPE_OTP :
			strcat(camera_info, "Y;");
			break;
		default :
			strcat(camera_info, "NULL;");
			break;
	}

	rc = of_property_read_u32(of_node, "cam,read_version",
			&val);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR1;
	}
	strcat(camera_info, "READVER=");
	switch(val) {
		case CAM_INFO_READ_VER_SYSFS :
			strcat(camera_info, "SYSFS;");
			break;
		case CAM_INFO_READ_VER_CAMON :
			strcat(camera_info, "CAMON;");
			break;
		default :
			strcat(camera_info, "NULL;");
			break;
	}

	rc = of_property_read_u32(of_node, "cam,core_voltage",
			&val);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR1;
	}
	strcat(camera_info, "COREVOLT=");
	switch(val) {
		case CAM_INFO_CORE_VOLT_NONE :
			strcat(camera_info, "N;");
			break;
		case CAM_INFO_CORE_VOLT_USE :
			strcat(camera_info, "Y;");
			break;
		default :
			strcat(camera_info, "NULL;");
			break;
	}

	rc = of_property_read_u32(of_node, "cam,upgrade",
			&val);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR1;
	}
	strcat(camera_info, "UPGRADE=");
	switch(val) {
		case CAM_INFO_FW_UPGRADE_NONE :
			strcat(camera_info, "N;");
			break;
		case CAM_INFO_FW_UPGRADE_SYSFS :
			strcat(camera_info, "SYSFS;");
			break;
		case CAM_INFO_FW_UPGRADE_CAMON :
			strcat(camera_info, "CAMON;");
			break;
		default :
			strcat(camera_info, "NULL;");
			break;
	}

	rc = of_property_read_u32(of_node, "cam,fw_write",
			&val);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR1;
	}
	strcat(camera_info, "FWWRITE=");
	switch(val) {
		case CAM_INFO_FW_WRITE_NONE :
			strcat(camera_info, "N;");
			break;
		case CAM_INFO_FW_WRITE_OS :
			strcat(camera_info, "OS;");
			break;
		case CAM_INFO_FW_WRITE_SD :
			strcat(camera_info, "SD;");
			break;
		case CAM_INFO_FW_WRITE_ALL :
			strcat(camera_info, "ALL;");
			break;
		default :
			strcat(camera_info, "NULL;");
			break;
	}

	rc = of_property_read_u32(of_node, "cam,fw_dump",
			&val);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR1;
	}
	strcat(camera_info, "FWDUMP=");
	switch(val) {
		case CAM_INFO_FW_DUMP_NONE :
			strcat(camera_info, "N;");
			break;
		case CAM_INFO_FW_DUMP_USE :
			strcat(camera_info, "Y;");
			break;
		default :
			strcat(camera_info, "NULL;");
			break;
	}

	rc = of_property_read_u32(of_node, "cam,companion_chip",
			&val);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR1;
	}
	strcat(camera_info, "CC=");
	switch(val) {
		case CAM_INFO_COMPANION_NONE :
			strcat(camera_info, "N;");
			break;
		case CAM_INFO_COMPANION_USE :
			strcat(camera_info, "Y;");
			break;
		default :
			strcat(camera_info, "NULL;");
			break;
	}

	rc = of_property_read_u32(of_node, "cam,ois",
			&val);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR1;
	}
	strcat(camera_info, "OIS=");
	switch(val) {
		case CAM_INFO_OIS_NONE :
			strcat(camera_info, "N;");
			break;
		case CAM_INFO_OIS_USE :
			strcat(camera_info, "Y;");
			break;
		default :
			strcat(camera_info, "NULL;");
			break;
	}

	snprintf(buf, sizeof(camera_info), "%s", camera_info);
	return 0;

ERROR1:
	strcpy(camera_info, "ISP=NULL;CALMEM=NULL;READVER=NULL;COREVOLT=NULL;UPGRADE=NULL;FW_CC=NULL;OIS=NULL");
	snprintf(buf, sizeof(camera_info), "%s", camera_info);
	return 0;
}
#endif

int msm_camera_get_dt_vreg_data(struct device_node *of_node,
	struct camera_vreg_t **cam_vreg, int *num_vreg)
{
	int rc = 0, i = 0;
	int32_t count = 0;
	uint32_t *vreg_array = NULL;
	struct camera_vreg_t *vreg = NULL;
	int32_t custom_vreg_cnt = 0;

	count = of_property_count_strings(of_node, "qcom,cam-vreg-name");
	CDBG("%s qcom,cam-vreg-name count %d\n", __func__, count);

	if (!count || (count == -EINVAL)) {
		pr_err("%s:%d number of entries is 0 or not present in dts\n",
			__func__, __LINE__);
		return 0;
	}

	vreg = kzalloc(sizeof(*vreg) * count, GFP_KERNEL);
	if (!vreg) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		return -ENOMEM;
	}
	*cam_vreg = vreg;
	*num_vreg = count;
	for (i = 0; i < count; i++) {
		rc = of_property_read_string_index(of_node,
			"qcom,cam-vreg-name", i,
			&vreg[i].reg_name);
		CDBG("%s reg_name[%d] = %s\n", __func__, i,
			vreg[i].reg_name);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			goto ERROR1;
		}
	}

	custom_vreg_cnt = of_property_count_strings(of_node,
		"qcom,cam-custom-vreg-name");
	if (custom_vreg_cnt > 0) {
		for (i = 0; i < count; i++) {
			rc = of_property_read_string_index(of_node,
				"qcom,cam-custom-vreg-name", i,
				&vreg[i].custom_vreg_name);
			CDBG("%s sub reg_name[%d] = %s\n", __func__, i,
				vreg[i].custom_vreg_name);
			if (rc < 0) {
				pr_err("%s failed %d\n", __func__, __LINE__);
				goto ERROR1;
			}
		}
	}

	vreg_array = kzalloc(sizeof(uint32_t) * count, GFP_KERNEL);
	if (!vreg_array) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		rc = -ENOMEM;
		goto ERROR1;
	}

	for (i = 0; i < count; i++)
		vreg[i].type = VREG_TYPE_DEFAULT;

	rc = of_property_read_u32_array(of_node, "qcom,cam-vreg-type",
		vreg_array, count);
	if (rc != -EINVAL) {
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			goto ERROR2;
		} else {
			for (i = 0; i < count; i++) {
				vreg[i].type = vreg_array[i];
				CDBG("%s cam_vreg[%d].type = %d\n",
					__func__, i, vreg[i].type);
			}
		}
	} else {
		CDBG("%s:%d no qcom,cam-vreg-type entries in dts\n",
			__func__, __LINE__);
		rc = 0;
	}

	rc = of_property_read_u32_array(of_node, "qcom,cam-vreg-min-voltage",
		vreg_array, count);
	if (rc != -EINVAL) {
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			goto ERROR2;
		} else {
			for (i = 0; i < count; i++) {
				vreg[i].min_voltage = vreg_array[i];
				CDBG("%s cam_vreg[%d].min_voltage = %d\n",
					__func__, i, vreg[i].min_voltage);
			}
		}
	} else {
		CDBG("%s:%d no qcom,cam-vreg-min-voltage entries in dts\n",
			__func__, __LINE__);
		rc = 0;
	}

	rc = of_property_read_u32_array(of_node, "qcom,cam-vreg-max-voltage",
		vreg_array, count);
	if (rc != -EINVAL) {
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			goto ERROR2;
		} else {
			for (i = 0; i < count; i++) {
				vreg[i].max_voltage = vreg_array[i];
				CDBG("%s cam_vreg[%d].max_voltage = %d\n",
					__func__, i, vreg[i].max_voltage);
			}
		}
	} else {
		CDBG("%s:%d no qcom,cam-vreg-max-voltage entries in dts\n",
			__func__, __LINE__);
		rc = 0;
	}

	rc = of_property_read_u32_array(of_node, "qcom,cam-vreg-op-mode",
		vreg_array, count);
	if (rc != -EINVAL) {
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			goto ERROR2;
		} else {
			for (i = 0; i < count; i++) {
				vreg[i].op_mode = vreg_array[i];
				CDBG("%s cam_vreg[%d].op_mode = %d\n",
					__func__, i, vreg[i].op_mode);
			}
		}
	} else {
		CDBG("%s:%d no qcom,cam-vreg-op-mode entries in dts\n",
			__func__, __LINE__);
		rc = 0;
	}

	kfree(vreg_array);
	return rc;
ERROR2:
	kfree(vreg_array);
ERROR1:
	kfree(vreg);
	*num_vreg = 0;
	return rc;
}

static int msm_camera_enable_i2c_mux(struct msm_camera_i2c_conf *i2c_conf)
{
	struct v4l2_subdev *i2c_mux_sd =
		dev_get_drvdata(&i2c_conf->mux_dev->dev);
	v4l2_subdev_call(i2c_mux_sd, core, ioctl,
		VIDIOC_MSM_I2C_MUX_INIT, NULL);
	v4l2_subdev_call(i2c_mux_sd, core, ioctl,
		VIDIOC_MSM_I2C_MUX_CFG, (void *)&i2c_conf->i2c_mux_mode);
	return 0;
}

static int msm_camera_disable_i2c_mux(struct msm_camera_i2c_conf *i2c_conf)
{
	struct v4l2_subdev *i2c_mux_sd =
		dev_get_drvdata(&i2c_conf->mux_dev->dev);
	v4l2_subdev_call(i2c_mux_sd, core, ioctl,
		VIDIOC_MSM_I2C_MUX_RELEASE, NULL);
	return 0;
}

int msm_camera_pinctrl_init(
	struct msm_pinctrl_info *sensor_pctrl, struct device *dev) {

	sensor_pctrl->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR_OR_NULL(sensor_pctrl->pinctrl)) {
		pr_err("%s:%d Getting pinctrl handle failed\n",
			__func__, __LINE__);
		return -EINVAL;
	}
	sensor_pctrl->gpio_state_active =
		pinctrl_lookup_state(sensor_pctrl->pinctrl,
				CAM_SENSOR_PINCTRL_STATE_DEFAULT);
	if (IS_ERR_OR_NULL(sensor_pctrl->gpio_state_active)) {
		pr_err("%s:%d Failed to get the active state pinctrl handle\n",
			__func__, __LINE__);
		return -EINVAL;
	}
	sensor_pctrl->gpio_state_suspend
		= pinctrl_lookup_state(sensor_pctrl->pinctrl,
				CAM_SENSOR_PINCTRL_STATE_SLEEP);
	if (IS_ERR_OR_NULL(sensor_pctrl->gpio_state_suspend)) {
		pr_err("%s:%d Failed to get the suspend state pinctrl handle\n",
				__func__, __LINE__);
		return -EINVAL;
	}
	return 0;
}

int msm_cam_sensor_handle_reg_gpio(int seq_val,
	struct msm_camera_gpio_conf *gconf, int val) {

	int gpio_offset = -1;

	if (!gconf) {
		pr_err("ERR:%s: Input Parameters are not proper\n", __func__);
		return -EINVAL;
	}
	CDBG("%s: %d Seq val: %d, config: %d", __func__, __LINE__,
		seq_val, val);

	switch (seq_val) {
	case CAM_VDIG:
		gpio_offset = SENSOR_GPIO_VDIG;
		break;

	case CAM_VIO:
		gpio_offset = SENSOR_GPIO_VIO;
		break;

	case CAM_VANA:
		gpio_offset = SENSOR_GPIO_VANA;
		break;

	case CAM_VAF:
		gpio_offset = SENSOR_GPIO_VAF;
		break;

	case CAM_V_CUSTOM1:
		gpio_offset = SENSOR_GPIO_CUSTOM1;
		break;

	case CAM_V_CUSTOM2:
		gpio_offset = SENSOR_GPIO_CUSTOM2;
		break;

	default:
		pr_err("%s:%d Invalid VREG seq val %d\n", __func__,
			__LINE__, seq_val);
		return -EINVAL;
	}

	CDBG("%s: %d GPIO offset: %d, seq_val: %d\n", __func__, __LINE__,
		gpio_offset, seq_val);

	if ((gconf->gpio_num_info->valid[gpio_offset] == 1)) {
		gpio_set_value_cansleep(
			gconf->gpio_num_info->gpio_num
			[gpio_offset], val);
	}
	return 0;
}

int32_t msm_sensor_driver_get_gpio_data(
	struct msm_camera_gpio_conf **gpio_conf,
	struct device_node *of_node)
{
	int32_t                      rc = 0, i = 0;
	uint16_t                    *gpio_array = NULL;
	int16_t                     gpio_array_size = 0;
	struct msm_camera_gpio_conf *gconf = NULL;

	/* Validate input parameters */
	if (!of_node) {
		pr_err("failed: invalid param of_node %pK", of_node);
		return -EINVAL;
	}

	gpio_array_size = of_gpio_count(of_node);
	CDBG("gpio count %d\n", gpio_array_size);
	if (gpio_array_size <= 0)
		return 0;

	gconf = kzalloc(sizeof(struct msm_camera_gpio_conf),
		GFP_KERNEL);
	if (!gconf)
		return -ENOMEM;

	*gpio_conf = gconf;

	gpio_array = kcalloc(gpio_array_size, sizeof(uint16_t), GFP_KERNEL);
	if (!gpio_array)
		goto FREE_GPIO_CONF;

	for (i = 0; i < gpio_array_size; i++) {
		gpio_array[i] = of_get_gpio(of_node, i);
		CDBG("gpio_array[%d] = %d", i, gpio_array[i]);
	}
	rc = msm_camera_get_dt_gpio_req_tbl(of_node, gconf, gpio_array,
		gpio_array_size);
	if (rc < 0) {
		pr_err("failed in msm_camera_get_dt_gpio_req_tbl\n");
		goto FREE_GPIO_CONF;
	}

	rc = msm_camera_init_gpio_pin_tbl(of_node, gconf, gpio_array,
		gpio_array_size);
	if (rc < 0) {
		pr_err("failed in msm_camera_init_gpio_pin_tbl\n");
		goto FREE_GPIO_REQ_TBL;
	}
	kfree(gpio_array);
	return rc;

FREE_GPIO_REQ_TBL:
	kfree(gconf->cam_gpio_req_tbl);
FREE_GPIO_CONF:
	kfree(gconf);
	kfree(gpio_array);
	*gpio_conf = NULL;
	return rc;
}

int msm_camera_power_up(struct msm_camera_power_ctrl_t *ctrl,
	enum msm_camera_device_type_t device_type,
	struct msm_camera_i2c_client *sensor_i2c_client,
	uint32_t is_secure, int sub_device)
{
	int rc = 0, index = 0, no_gpio = 0, ret = 0;
	struct msm_sensor_power_setting *power_setting = NULL;

	pr_info("%s:%d [POWER_DBG] : E requested by %d sub deivce\n",
		__func__, __LINE__, sub_device);
	if (!ctrl || !sensor_i2c_client) {
		pr_err("failed ctrl %pK sensor_i2c_client %pK\n", ctrl,
			sensor_i2c_client);
		return -EINVAL;
	}
	if (ctrl->gpio_conf->cam_gpiomux_conf_tbl != NULL)
		pr_err("%s:%d mux install\n", __func__, __LINE__);

	if ((power_state & NOW_POWER_ON) && (sub_device != SUB_DEVICE_TYPE_SENSOR)) {
		pr_warn("%s:%d [POWER_DBG] already turned on by sensor\n", __func__, __LINE__);
		return NO_POWER_OFF;
	}

	if (sub_device == SUB_DEVICE_TYPE_SENSOR) {
		if (sensor_power_on_cnt >= 0) {
			sensor_power_on_cnt++;
			pr_info("%s:%d [POWER_DBG] sensor power cnt %d\n", __func__, __LINE__, sensor_power_on_cnt);
		}
	}

	if (ctrl->gpio_conf->cam_gpio_req_tbl) {
		ret = msm_camera_pinctrl_init(&(ctrl->pinctrl_info), ctrl->dev);
		if (ret < 0) {
			pr_err("%s:%d Initialization of pinctrl failed\n",
					__func__, __LINE__);
			ctrl->cam_pinctrl_status = 0;
		} else {
			ctrl->cam_pinctrl_status = 1;
		}
		rc = msm_camera_request_gpio_table(
			ctrl->gpio_conf->cam_gpio_req_tbl,
			ctrl->gpio_conf->cam_gpio_req_tbl_size, 1);
		if (rc < 0)
			no_gpio = rc;
	} else
		ctrl->cam_pinctrl_status = 0;

	if (ctrl->cam_pinctrl_status) {
		ret = pinctrl_select_state(ctrl->pinctrl_info.pinctrl,
			ctrl->pinctrl_info.gpio_state_active);
		if (ret)
			pr_err("%s:%d cannot set pin to active state",
				__func__, __LINE__);
	}
	power_state = NOW_POWER_ON;
	power_state |= 0x1 << sub_device;

	for (index = 0; index < ctrl->power_setting_size; index++) {
		CDBG("%s index %d\n", __func__, index);
		power_setting = &ctrl->power_setting[index];
		CDBG("%s type %d\n", __func__, power_setting->seq_type);
		switch (power_setting->seq_type) {
		case SENSOR_CLK:
			if (power_setting->seq_val >= ctrl->clk_info_size) {
				pr_err("%s clk index %d >= max %zu\n", __func__,
					power_setting->seq_val,
					ctrl->clk_info_size);
				goto power_up_failed;
			}
			if (power_setting->config_val)
				ctrl->clk_info[power_setting->seq_val].
					clk_rate = power_setting->config_val;
			rc = msm_camera_clk_enable(ctrl->dev,
				ctrl->clk_info, ctrl->clk_ptr,
				ctrl->clk_info_size, true);
			if (rc < 0) {
				pr_err("%s: clk enable failed\n", __func__);
				goto power_up_failed;
			}
			break;
		case SENSOR_GPIO:
			if (is_secure && power_setting->seq_val == SENSOR_GPIO_RESET) {
				if (device_type == MSM_CAMERA_PLATFORM_DEVICE) {
					rc = sensor_i2c_client->i2c_func_tbl->i2c_util(
					sensor_i2c_client, MSM_CCI_I2C_WRITE_SYNC);
					if (rc < 0) {
						pr_err("%s cci_init failed\n", __func__);
						goto power_up_failed;
					}
				}
			} else {
				if (no_gpio) {
					pr_err("%s: request gpio failed\n", __func__);
					return no_gpio;
				}
				if (power_setting->seq_val >= SENSOR_GPIO_MAX ||
					!ctrl->gpio_conf->gpio_num_info) {
					pr_err("%s gpio index %d >= max %d\n", __func__,
						power_setting->seq_val,
						SENSOR_GPIO_MAX);
					goto power_up_failed;
				}
				if (!ctrl->gpio_conf->gpio_num_info->valid
					[power_setting->seq_val])
					continue;
				CDBG("%s:%d gpio set val %d\n", __func__, __LINE__,
					ctrl->gpio_conf->gpio_num_info->gpio_num
					[power_setting->seq_val]);
				gpio_set_value_cansleep(
					ctrl->gpio_conf->gpio_num_info->gpio_num
					[power_setting->seq_val],
					(int) power_setting->config_val);
			}
			break;
		case SENSOR_VREG:
#if defined(CONFIG_COMPANION2) || defined(CONFIG_COMPANION3)
			if (retention_mode_pwr == 1
				&& ((!strcmp(ctrl->cam_vreg[power_setting->seq_val].reg_name, "cam_vio_comp"))
				|| (!strcmp(ctrl->cam_vreg[power_setting->seq_val].reg_name, "cam_vret_comp"))
				|| (!strcmp(ctrl->cam_vreg[power_setting->seq_val].reg_name, "cam_retmode_vret_comp")))) {

				if (!strcmp(ctrl->cam_vreg[power_setting->seq_val].reg_name, "cam_retmode_vret_comp")) {
					if (retention_reg) {
						int rc = 0;
						pr_err("%s cam_retmode_vret_comp voltage setting (1.05V)\n", __func__);
						rc = regulator_set_voltage(
							retention_reg, 1050000, 1050000);
						if (rc < 0) {
							pr_err("%s: %s set voltage failed\n",
							__func__, ctrl->cam_vreg[power_setting->seq_val].reg_name);
						}
					}
					usleep_range(1000, 2000);
				}
				pr_err("%s now retention mode\n", __func__);
				continue;
			} else if (retention_mode_pwr == 0
				&& (!strcmp(ctrl->cam_vreg[power_setting->seq_val].reg_name, "cam_retmode_vret_comp"))) {
				pr_err("%s skip cam_retmode_vret_comp. now normal power up\n", __func__);
				continue;
			}
#endif

			if (power_setting->seq_val == INVALID_VREG)
				break;
			if (power_setting->seq_val >= CAM_VREG_MAX) {
				pr_err("%s vreg index %d >= max %d\n", __func__,
					power_setting->seq_val,
					SENSOR_GPIO_MAX);
				goto power_up_failed;
			}
			if (power_setting->seq_val < ctrl->num_vreg)
				msm_camera_config_single_vreg(ctrl->dev,
					&ctrl->cam_vreg
					[power_setting->seq_val],
					(struct regulator **)
					&power_setting->data[0],
					1);
			else
				pr_err("%s: %d usr_idx:%d dts_idx:%d\n",
					__func__, __LINE__,
					power_setting->seq_val, ctrl->num_vreg);
#if defined(CONFIG_COMPANION2) || defined(CONFIG_COMPANION3)
			if (!strcmp(ctrl->cam_vreg[power_setting->seq_val].reg_name, "cam_vret_comp")) {
				pr_err("%s save cam_vret_comp reg pointer\n", __func__);
				retention_reg = (struct regulator *)power_setting->data[0];
			}
#endif
#if defined(CONFIG_COMPANION3)
			if (!strcmp(ctrl->cam_vreg[power_setting->seq_val].reg_name, "cam_vdig_comp")) {
				pr_err("%s save cam_vdig_comp reg pointer for pwr binning\n", __func__);
				pwr_binning_reg = (struct regulator *)power_setting->data[0];
			}
#endif

			if ((!strcmp(ctrl->cam_vreg[power_setting->seq_val].reg_name, "cam_vana"))
				&& ctrl->power_setting_size == 19) {
				pr_err("%s save cam_vana reg pointer for pwr binning\n", __func__);
				ana_reg = (struct regulator *)power_setting->data[0];
				pr_err("%s cam_vana voltage setting (2.80V)\n", __func__);
				rc = regulator_set_voltage(
					ana_reg, 2800000, 2800000);
				if (rc < 0) {
					pr_err("%s: %s set voltage failed\n",
					__func__, ctrl->cam_vreg[power_setting->seq_val].reg_name);
				}
			} else if ((!strcmp(ctrl->cam_vreg[power_setting->seq_val].reg_name, "cam_vana"))
				&& ctrl->power_setting_size == 18) {
				pr_err("%s save cam_vana reg pointer for pwr binning\n", __func__);
				ana_reg = (struct regulator *)power_setting->data[0];
				pr_err("%s cam_vana voltage setting (2.95V)\n", __func__);
				rc = regulator_set_voltage(
					ana_reg, 2950000, 2950000);
				if (rc < 0) {
					pr_err("%s: %s set voltage failed\n",
					__func__, ctrl->cam_vreg[power_setting->seq_val].reg_name);
				}
			}
#if 0//TEMP_8996_N
			rc = msm_cam_sensor_handle_reg_gpio(
				power_setting->seq_val,
				ctrl->gpio_conf, 1);
			if (rc < 0) {
				pr_err("ERR:%s Error in handling VREG GPIO\n",
					__func__);
				goto power_up_failed;
			}
#endif
			break;
		case SENSOR_I2C_MUX:
			if (ctrl->i2c_conf && ctrl->i2c_conf->use_i2c_mux)
				msm_camera_enable_i2c_mux(ctrl->i2c_conf);
			break;
		default:
			pr_err("%s error power seq type %d\n", __func__,
				power_setting->seq_type);
			break;
		}
		if (power_setting->delay > 20) {
			msleep(power_setting->delay);
		} else if (power_setting->delay) {
			usleep_range(power_setting->delay * 1000,
				(power_setting->delay * 1000) + 1000);
		}
	}

	if (device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		rc = sensor_i2c_client->i2c_func_tbl->i2c_util(
			sensor_i2c_client, MSM_CCI_INIT);
		if (rc < 0) {
			pr_err("%s cci_init failed\n", __func__);
			goto power_up_failed;
		}
	}

	pr_info("%s [POWER_DBG] : X\n", __func__);
	return 0;
power_up_failed:
	pr_err("%s:%d failed\n", __func__, __LINE__);
	if (sub_device == SUB_DEVICE_TYPE_SENSOR) {
		if (sensor_power_on_cnt > 0)
			sensor_power_on_cnt--;
		else
			pr_warn("%s [POWER_DBG] : sensor_power_on_cnt has invalid value(%d)\n", __func__, sensor_power_on_cnt);

		if (sensor_power_on_cnt == 0) {
			pr_info("%s [POWER_DBG] : sensor power was turned off\n", __func__);
			power_state = NOW_POWER_OFF;
		} else {
			pr_warn("%s [POWER_DBG] : sensor power must be turned off more(%d)\n", __func__, sensor_power_on_cnt);
		}
	} else
		power_state = NOW_POWER_OFF;

	for (index--; index >= 0; index--) {
		CDBG("%s index %d\n", __func__, index);
		power_setting = &ctrl->power_setting[index];
		CDBG("%s type %d\n", __func__, power_setting->seq_type);
		switch (power_setting->seq_type) {

		case SENSOR_CLK:
			msm_camera_clk_enable(ctrl->dev,
				ctrl->clk_info, ctrl->clk_ptr,
				ctrl->clk_info_size, false);
			break;
		case SENSOR_GPIO:
			if (!ctrl->gpio_conf->gpio_num_info)
				continue;
			if (!ctrl->gpio_conf->gpio_num_info->valid
				[power_setting->seq_val])
				continue;
			gpio_set_value_cansleep(
				ctrl->gpio_conf->gpio_num_info->gpio_num
				[power_setting->seq_val], GPIOF_OUT_INIT_LOW);
			break;
		case SENSOR_VREG:
#if defined(CONFIG_COMPANION2) || defined(CONFIG_COMPANION3)
			if (retention_mode_pwr == 1
				&& ((!strcmp(ctrl->cam_vreg[power_setting->seq_val].reg_name, "cam_vio_comp"))
				|| (!strcmp(ctrl->cam_vreg[power_setting->seq_val].reg_name, "cam_vret_comp"))
				|| (!strcmp(ctrl->cam_vreg[power_setting->seq_val].reg_name, "cam_retmode_vret_comp")))) {

				if (!strcmp(ctrl->cam_vreg[power_setting->seq_val].reg_name, "cam_retmode_vret_comp")) {
					if (retention_reg) {
						int rc = 0;
						pr_err("%s cam_retmode_vret_comp voltage setting (1.05V)\n", __func__);
						rc = regulator_set_voltage(
							retention_reg, 1050000, 1050000);
						if (rc < 0) {
							pr_err("%s: %s set voltage failed\n",
							__func__, ctrl->cam_vreg[power_setting->seq_val].reg_name);
						}
					}
				}
				pr_err("%s now retention mode\n", __func__);
				continue;
			} else if (retention_mode_pwr == 0
				&& (!strcmp(ctrl->cam_vreg[power_setting->seq_val].reg_name, "cam_retmode_vret_comp"))) {
				pr_err("%s skip cam_retmode_vret_comp. now normal power up\n", __func__);
				continue;
			}
#endif
			if (power_setting->seq_val < ctrl->num_vreg)
				msm_camera_config_single_vreg(ctrl->dev,
					&ctrl->cam_vreg
					[power_setting->seq_val],
					(struct regulator **)
					&power_setting->data[0],
					0);
			else
				pr_err("%s:%d:seq_val: %d > num_vreg: %d\n",
					__func__, __LINE__,
					power_setting->seq_val, ctrl->num_vreg);
#if 0 //TEMP_8996_N
			msm_cam_sensor_handle_reg_gpio(power_setting->seq_val,
				ctrl->gpio_conf, GPIOF_OUT_INIT_LOW);
#endif
			break;
		case SENSOR_I2C_MUX:
			if (ctrl->i2c_conf && ctrl->i2c_conf->use_i2c_mux)
				msm_camera_disable_i2c_mux(ctrl->i2c_conf);
			break;
		default:
			pr_err("%s error power seq type %d\n", __func__,
				power_setting->seq_type);
			break;
		}
		if (power_setting->delay > 20) {
			msleep(power_setting->delay);
		} else if (power_setting->delay) {
			usleep_range(power_setting->delay * 1000,
				(power_setting->delay * 1000) + 1000);
		}
	}
	if (ctrl->cam_pinctrl_status) {
		ret = pinctrl_select_state(ctrl->pinctrl_info.pinctrl,
				ctrl->pinctrl_info.gpio_state_suspend);
		if (ret)
			pr_err("%s:%d cannot set pin to suspend state\n",
				__func__, __LINE__);
		devm_pinctrl_put(ctrl->pinctrl_info.pinctrl);
	}
	ctrl->cam_pinctrl_status = 0;
	msm_camera_request_gpio_table(
		ctrl->gpio_conf->cam_gpio_req_tbl,
		ctrl->gpio_conf->cam_gpio_req_tbl_size, 0);
	return rc;
}

static struct msm_sensor_power_setting*
msm_camera_get_power_settings(struct msm_camera_power_ctrl_t *ctrl,
				enum msm_sensor_power_seq_type_t seq_type,
				uint16_t seq_val)
{
	struct msm_sensor_power_setting *power_setting, *ps = NULL;
	int idx;

	for (idx = 0; idx < ctrl->power_setting_size; idx++) {
		power_setting = &ctrl->power_setting[idx];
		if (power_setting->seq_type == seq_type &&
			power_setting->seq_val ==  seq_val) {
			ps = power_setting;
			return ps;
		}

	}
	return ps;
}

int msm_camera_power_down(struct msm_camera_power_ctrl_t *ctrl,
	enum msm_camera_device_type_t device_type,
	struct msm_camera_i2c_client *sensor_i2c_client,
	uint32_t is_secure, int sub_device)
{
	int index = 0, ret = 0, i = 0;
	struct msm_sensor_power_setting *pd = NULL;
	struct msm_sensor_power_setting *ps;

	pr_info("%s:%d [POWER_DBG] : E\n", __func__, __LINE__);
	if (!ctrl || !sensor_i2c_client) {
		pr_err("failed ctrl %pK sensor_i2c_client %pK\n", ctrl,
			sensor_i2c_client);
		return -EINVAL;
	}

	if (power_state == NOW_POWER_OFF) {
		pr_warn("%s:%d [POWER_DBG] already turned off\n", __func__, __LINE__);
		return 0;
	}
	if (power_state & NOW_POWER_ON) {
		for (i = 0; i < SUB_DEVICE_TYPE_MAX; i++) {
			if (power_state & (0x1 << i)) {
				if (sub_device == i) {
					pr_info("%s:%d [POWER_DBG] find %d sub device\n",
						__func__, __LINE__, sub_device);
					break;
				}
			}
		}
		if (i == SUB_DEVICE_TYPE_MAX) {
			pr_warn("%s:%d [POWER_DBG] discard power off control of %d sub device\n",
				__func__, __LINE__, sub_device);
			return 0;
		}
	}

	if (device_type == MSM_CAMERA_PLATFORM_DEVICE)
		sensor_i2c_client->i2c_func_tbl->i2c_util(
			sensor_i2c_client, MSM_CCI_RELEASE);

	for (index = 0; index < ctrl->power_down_setting_size; index++) {
		CDBG("%s index %d\n", __func__, index);
		pd = &ctrl->power_down_setting[index];
		ps = NULL;
		CDBG("%s type %d\n", __func__, pd->seq_type);
		switch (pd->seq_type) {
		case SENSOR_CLK:
			msm_camera_clk_enable(ctrl->dev,
				ctrl->clk_info, ctrl->clk_ptr,
				ctrl->clk_info_size, false);
			break;
		case SENSOR_GPIO:
			if (is_secure && pd->seq_val == SENSOR_GPIO_RESET) {
				if (device_type == MSM_CAMERA_PLATFORM_DEVICE) {
					ret = sensor_i2c_client->i2c_func_tbl->i2c_util(
					sensor_i2c_client, MSM_CCI_I2C_WRITE_SYNC_BLOCK);
					if (ret < 0) {
						pr_err("%s cci_init failed\n", __func__);
					}
				}
			} else {
				if (pd->seq_val >= SENSOR_GPIO_MAX ||
					!ctrl->gpio_conf->gpio_num_info) {
					pr_err("%s gpio index %d >= max %d\n", __func__,
						pd->seq_val,
						SENSOR_GPIO_MAX);
					continue;
				}
			if (!ctrl->gpio_conf->gpio_num_info->valid
				[pd->seq_val])
				continue;
			CDBG("%s:%d gpio set val %d\n", __func__, __LINE__,
				ctrl->gpio_conf->gpio_num_info->gpio_num
				[pd->seq_val]);
			gpio_set_value_cansleep(
				ctrl->gpio_conf->gpio_num_info->gpio_num
				[pd->seq_val],
				(int) pd->config_val);
			}
			break;
		case SENSOR_VREG:
#if defined(CONFIG_COMPANION2) || defined(CONFIG_COMPANION3)
			if (retention_mode_pwr == 1
				&& ((!strcmp(ctrl->cam_vreg[pd->seq_val].reg_name, "cam_vio_comp"))
				|| (!strcmp(ctrl->cam_vreg[pd->seq_val].reg_name, "cam_vret_comp"))
				|| (!strcmp(ctrl->cam_vreg[pd->seq_val].reg_name, "cam_retmode_vret_comp")))) {

				if (!strcmp(ctrl->cam_vreg[pd->seq_val].reg_name, "cam_retmode_vret_comp")) {
					if (retention_reg) {
						int rc = 0;
						pr_err("%s cam_retmode_vret_comp voltage setting (0.7V)\n", __func__);
						rc = regulator_set_voltage(
							retention_reg, 700000, 700000);
						if (rc < 0) {
							pr_err("%s: %s set voltage failed\n",
							__func__, ctrl->cam_vreg[pd->seq_val].reg_name);
						}
					}
				}
				pr_err("%s now retention mode\n", __func__);
				continue;
			} else if (retention_mode_pwr == 0
				&& (!strcmp(ctrl->cam_vreg[pd->seq_val].reg_name, "cam_retmode_vret_comp"))) {
				pr_err("%s skip cam_retmode_vret_comp. now normal power down\n", __func__);
				continue;
			}
#endif
			if (pd->seq_val == INVALID_VREG)
				break;
			if (pd->seq_val >= CAM_VREG_MAX) {
				pr_err("%s vreg index %d >= max %d\n", __func__,
					pd->seq_val,
					SENSOR_GPIO_MAX);
				continue;
			}

			ps = msm_camera_get_power_settings(ctrl,
						pd->seq_type,
						pd->seq_val);
			if (ps) {
				if (pd->seq_val < ctrl->num_vreg)
					msm_camera_config_single_vreg(ctrl->dev,
						&ctrl->cam_vreg
						[pd->seq_val],
						(struct regulator **)
						&ps->data[0],
						0);
				else
					pr_err("%s:%d:seq_val:%d > num_vreg: %d\n",
						__func__, __LINE__, pd->seq_val,
						ctrl->num_vreg);
			} else
				pr_err("%s error in power up/down seq data\n",
								__func__);
#if 0//TEMP_8996_N
			ret = msm_cam_sensor_handle_reg_gpio(pd->seq_val,
				ctrl->gpio_conf, GPIOF_OUT_INIT_LOW);
			if (ret < 0)
				pr_err("ERR:%s Error while disabling VREG GPIO\n",
					__func__);
#endif
			break;
		case SENSOR_I2C_MUX:
			if (ctrl->i2c_conf && ctrl->i2c_conf->use_i2c_mux)
				msm_camera_disable_i2c_mux(ctrl->i2c_conf);
			break;
		default:
			pr_err("%s error power seq type %d\n", __func__,
				pd->seq_type);
			break;
		}
		if (pd->delay > 20) {
			msleep(pd->delay);
		} else if (pd->delay) {
			usleep_range(pd->delay * 1000,
				(pd->delay * 1000) + 1000);
		}
	}
	if (ctrl->cam_pinctrl_status) {
		ret = pinctrl_select_state(ctrl->pinctrl_info.pinctrl,
				ctrl->pinctrl_info.gpio_state_suspend);
		if (ret)
			pr_err("%s:%d cannot set pin to suspend state",
				__func__, __LINE__);
		devm_pinctrl_put(ctrl->pinctrl_info.pinctrl);
	}
	ctrl->cam_pinctrl_status = 0;
	msm_camera_request_gpio_table(
		ctrl->gpio_conf->cam_gpio_req_tbl,
		ctrl->gpio_conf->cam_gpio_req_tbl_size, 0);

	if (sub_device == SUB_DEVICE_TYPE_SENSOR) {
		if (sensor_power_on_cnt > 0)
			sensor_power_on_cnt--;
		else
			pr_warn("%s [POWER_DBG] : sensor_power_on_cnt has invalid value(%d)\n", __func__, sensor_power_on_cnt);

		if (sensor_power_on_cnt == 0) {
			pr_info("%s [POWER_DBG] : sensor power was turned off\n", __func__);
			power_state = NOW_POWER_OFF;
		} else {
			pr_warn("%s [POWER_DBG] : sensor power must be turned off more(%d)\n", __func__, sensor_power_on_cnt);
		}
	} else
		power_state = NOW_POWER_OFF;
	pr_info("%s [POWER_DBG] : X\n", __func__);
	return 0;
}

#if 1// To check firmware in FROM
extern char fw_crc[10];

int msm_camera_write_sysfs(char* path, const char* data, uint32_t data_size)
{
  struct file *filp = NULL;
  mm_segment_t old_fs;
  int ret = 0;

  old_fs = get_fs();
  set_fs(KERNEL_DS);
  filp = filp_open(path, O_RDWR|O_CREAT, 0666);
  if(IS_ERR(filp)){
    pr_err("%s: sysfs open file failed. [%s]", __func__, path);
    return -1;
  }

  ret = vfs_write(filp, data, data_size, &filp->f_pos);
  if (ret < 0) {
    pr_err("%s: sysfs write file failed. [%s]", __func__, path);
    ret = -1;
    goto ERROR;
  }

ERROR:
  if (filp) {
    filp_close(filp, NULL);
    filp = NULL;
  }
  set_fs(old_fs);
  return ret;
}

int msm_camera_fw_check(const char read_fw_crc, uint8_t index)
{
  int ret = 0;

  fw_crc[index] = read_fw_crc;
  CDBG("%s: index: %d, read_fw_crc: %c", __func__, index, read_fw_crc);
  CDBG("%s: fw_crc: %s", __func__, fw_crc);
  ret = msm_camera_write_sysfs(SYSFS_FW_CHECK_PATH, fw_crc, sizeof(fw_crc));
  if (ret < 0) {
    pr_err("%s: msm_camera_write_sysfs failed.", __func__);
    ret = -1;
  }

  return ret;
}
#endif
