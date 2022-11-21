#ifdef CONFIG_OF

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>

#include "imgsensor_vendor.h"
#include "imgsensor_vendor_specific.h"
#include "imgsensor_sysfs.h"

static struct cam_hw_param_collector cam_hwparam_collector;

static int parse_sysfs_caminfo(struct device_node *np,
	struct imgsensor_cam_info *camera_info, int camera_num)
{
	u32 position;

	if(of_property_read_u32(np, "position", &position))
		position = camera_num;
	if (position >= CAM_INFO_MAX) {
		pr_err("invalid postion %u for camera info", position);
		return -EOVERFLOW;
	}

	of_property_read_u32(np, "isp", &camera_info[position].isp);
	of_property_read_u32(np, "cal_memory", &camera_info[position].cal_memory);
	of_property_read_u32(np, "read_version", &camera_info[position].read_version);
	of_property_read_u32(np, "core_voltage", &camera_info[position].core_voltage);
	of_property_read_u32(np, "upgrade", &camera_info[position].upgrade);
	of_property_read_u32(np, "fw_write", &camera_info[position].fw_write);
	of_property_read_u32(np, "fw_dump", &camera_info[position].fw_dump);
	of_property_read_u32(np, "companion", &camera_info[position].companion);
	of_property_read_u32(np, "ois", &camera_info[position].ois);
	of_property_read_u32(np, "valid", &camera_info[position].valid);
	of_property_read_u32(np, "dual_open", &camera_info[position].dual_open);
	if(!of_property_read_u32(np, "includes_sub", &camera_info[position].sub_sensor_id)) {
		camera_info[position].includes_sub = true;
	}
	else {
		camera_info[position].sub_sensor_id = 0;
		camera_info[position].includes_sub = false;
	}

	return 0;
}

int cam_info_probe(struct device_node *np) {
	struct device_node *vendor;
	struct device_node *camInfo_np;
	struct imgsensor_cam_info *camera_info;
	struct imgsensor_common_cam_info *common_camera_infos = NULL;
	struct imgsensor_vendor_specific *specific;
	int camera_num;
	int max_camera_num;
	int ret = 0;
	char camInfo_string[15];
	bool use_module_check;
	bool skip_cal_loading;

	specific = (struct imgsensor_vendor_specific *)kmalloc(sizeof(struct imgsensor_vendor_specific), GFP_KERNEL);

	vendor = of_find_node_by_name(np, "vendor");
	ret = of_property_read_u32(vendor, "max_camera_num", &max_camera_num);
	if (ret) {
		pr_err("max_camera_num read is fail(%d)", ret);
		max_camera_num = 0;
	}
	get_cam_info(&camera_info);

	for (camera_num = 0; camera_num < max_camera_num; camera_num++) {
		sprintf(camInfo_string, "%s%d", "camera_info", camera_num);

		camInfo_np = of_find_node_by_name(vendor, camInfo_string);
		if (!camInfo_np) {
			pr_info("%s: camera_num = %d can't find camInfo_string node\n", __func__, camera_num);
			continue;
		}
		parse_sysfs_caminfo(camInfo_np, camera_info, camera_num);
	}

	ret = of_property_read_u32(vendor, "rear_sensor_id", &specific->sensor_id[SENSOR_POSITION_REAR]);
	if (ret) {
		pr_err("rear_sensor_id read is fail(%d)", ret);
	}

	ret = of_property_read_u32(vendor, "front_sensor_id", &specific->sensor_id[SENSOR_POSITION_FRONT]);
	if (ret) {
		pr_err("front_sensor_id read is fail(%d)", ret);
	}

	ret = of_property_read_u32(vendor, "rear2_sensor_id", &specific->sensor_id[SENSOR_POSITION_REAR2]);
	if (ret) {
		pr_err("rear2_sensor_id read is fail(%d)", ret);
	}

	ret = of_property_read_u32(vendor, "rear3_sensor_id", &specific->sensor_id[SENSOR_POSITION_REAR3]);
	if (ret) {
		pr_err("rear3_sensor_id read is fail(%d)", ret);
	}

	ret = of_property_read_u32(vendor, "rear4_sensor_id", &specific->sensor_id[SENSOR_POSITION_REAR4]);
	if (ret) {
		pr_err("rear4_sensor_id read is fail(%d)", ret);
	}

	use_module_check = of_property_read_bool(vendor, "use_module_check");
	if (!use_module_check) {
		pr_err("use_module_check not use(%d)", use_module_check);
	}
	specific->use_module_check = use_module_check;

	skip_cal_loading = of_property_read_bool(vendor, "skip_cal_loading");
	if (!skip_cal_loading) {
		pr_err("skip_cal_loading not use(%d)\n", skip_cal_loading);
	}
	specific->skip_cal_loading = skip_cal_loading;
	get_specific(&specific);

	imgsensor_get_common_cam_info(&common_camera_infos);
	ret = of_property_read_u32(vendor, "max_supported_camera", &common_camera_infos->max_supported_camera);
	if (ret) {
		pr_err("supported_cameraId read is fail(%d)", ret);
	}

	ret = of_property_read_u32_array(vendor, "supported_cameraId",
		common_camera_infos->supported_camera_ids, common_camera_infos->max_supported_camera);
	if (ret) {
		pr_err("supported_cameraId read is fail(%d)", ret);
	}

	return ret;
}

void imgsensor_sec_init_err_cnt(struct cam_hw_param *hw_param)
{
        if (hw_param) {
                memset(hw_param, 0, sizeof(struct cam_hw_param));
        }
}

void imgsensor_sec_get_hw_param(struct cam_hw_param **hw_param, u32 position)
{
        switch (position) {
                case SENSOR_POSITION_REAR:
                        *hw_param = &cam_hwparam_collector.rear_hwparam;
                        break;
                case SENSOR_POSITION_REAR2:
                        *hw_param = &cam_hwparam_collector.rear2_hwparam;
                        break;
                case SENSOR_POSITION_REAR3:
                        *hw_param = &cam_hwparam_collector.rear3_hwparam;
                        break;
                case SENSOR_POSITION_REAR4:
                        *hw_param = &cam_hwparam_collector.rear4_hwparam;
                        break;
                case SENSOR_POSITION_FRONT:
                        *hw_param = &cam_hwparam_collector.front_hwparam;
                        break;
                default:
			pr_err("Sensor Position(%d) not found",position);
                        return;
        }
}

bool imgsensor_sec_is_valid_moduleid(char* moduleid)
{
        int i = 0;

        if (moduleid == NULL || strlen(moduleid) < 5) {
                goto err;
        }

        for (i = 0; i < 5; i++)
        {
                if (!((moduleid[i] > 47 && moduleid[i] < 58) || // 0 to 9
                        (moduleid[i] > 64 && moduleid[i] < 91))) {  // A to Z
                        goto err;
                }
        }

        return true;

err:
        pr_err("invalid moduleid\n");
        return false;
}
#endif
