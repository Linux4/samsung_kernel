#ifndef _IMGSENSOR_VENDOR_H_
#define _IMGSENSOR_VENDOR_H_

#include <linux/of.h>

struct cam_hw_param {
        u32 i2c_sensor_err_cnt;
        u32 i2c_comp_err_cnt;
        u32 i2c_ois_err_cnt;
        u32 i2c_af_err_cnt;
        u32 i2c_aperture_err_cnt;
        u32 mipi_sensor_err_cnt;
        u32 mipi_comp_err_cnt;
};

struct cam_hw_param_collector {
        struct cam_hw_param rear_hwparam;
        struct cam_hw_param rear2_hwparam;
        struct cam_hw_param rear3_hwparam;
        struct cam_hw_param rear4_hwparam;
        struct cam_hw_param front_hwparam;
};

int cam_info_probe(struct device_node *np);
void imgsensor_sec_init_err_cnt(struct cam_hw_param *hw_param);
void imgsensor_sec_get_hw_param(struct cam_hw_param **hw_param, u32 position);
bool imgsensor_sec_is_valid_moduleid(char* moduleid);
#endif //_IMGSENSOR_VENDOR_H_
