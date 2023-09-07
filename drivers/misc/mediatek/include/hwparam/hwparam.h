#ifndef __LINUX_HW_PARAM_H
#define __LINUX_HW_PARAM_H

#if defined(CONFIG_CAMERA_HW_BIG_DATA)
#define CAM_HW_ERR_CNT_FILE_PATH "/data/camera/camera_hw_err_cnt.dat"

struct cam_hw_param {
	u32 i2c_sensor_err_cnt;
	u32 i2c_af_err_cnt;
	u32 mipi_sensor_err_cnt;
};

struct cam_hw_param_collector {
	struct cam_hw_param rear_hwparam;
	struct cam_hw_param front_hwparam;
};

void is_sec_init_err_cnt_file(struct cam_hw_param *hw_param);
bool is_sec_need_update_to_file(void);
void is_sec_copy_err_cnt_from_file(void);
void is_sec_copy_err_cnt_to_file(void);

int is_sec_get_rear_hw_param(struct cam_hw_param **hw_param);
int is_sec_get_front_hw_param(struct cam_hw_param **hw_param);
#endif		/* CONFIG_CAMERA_HW_BIG_DATA */

#endif		/* __LINUX_HW_PARAM_H */
