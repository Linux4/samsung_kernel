/****************************************************************************
 ****************************************************************************
 ***
 ***   This file defines the adapters for imgsensor sysfs functions.
 ***   The adapters are made for preventing build error, when CONFIG_IMGSENSOR_SYSFS is not defined.
 ***   If CONFIG_IMGSENSOR_SYSFS is not defined, below functions do nothing or return error.
 ***
 ****************************************************************************
 ****************************************************************************/
#ifndef _KD_IMGSENSOR_SYSFS_ADAPTER_H
#define _KD_IMGSENSOR_SYSFS_ADAPTER_H

#include "imgsensor_sysfs.h"

#ifdef CONFIG_IMGSENSOR_SYSFS
#define IMGSENSOR_GET_CAL_SIZE_BY_SENSOR_IDX(sensor_idx)                                        imgsensor_get_cal_size_by_sensor_idx(sensor_idx)
#define IMGSENSOR_GET_CAL_BUF_BY_SENSOR_IDX(sensor_idx, buf)                                    imgsensor_get_cal_buf_by_sensor_idx(sensor_idx, buf)
#define CAM_INFO_PROB(device_node_ptr)                                                          cam_info_probe(device_node_ptr)
#define IMGSENSOR_SYSGET_ROM_ADDR_BY_ID(dual_device_id, sensor_id)                              imgsensor_sys_get_rom_addr_by_id(dual_device_id, sensor_id)
#define IMGSENSER_READ_OTP_CAL(dual_device_id, sensor_id, addr, data, size)                     imgsensor_read_otp_cal(dual_device_id, sensor_id, addr, data, size)
#define IMGSENSOR_SYSFS_UPDATE(rom_data, dual_device_id, sensor_id, offset, length, ret_value)  imgsensor_sysfs_update(rom_data, dual_device_id, sensor_id, offset, length, ret_value)
#define IMGSENSOR_GET_SAC_VALUE_BY_SENSOR_IDX(sensor_idx, ac_mode, ac_time)                     imgsensor_get_sac_value_by_sensor_idx(sensor_idx, ac_mode, ac_time)
#define IMGSENSOR_GET_CAL_ADDR_BY_SENSOR_IDX(sensor_idx, command)                               imgsensor_get_cal_addr_by_sensor_idx(sensor_idx, command)
#define IMGSENOSR_GET_ADAPTIVE_MIPI_STATUS(position)                                            imgsensor_get_adaptive_mipi_status(position)
#define IMGSENSOR_IS_DEFAULT_CAL(sensor_idx)                                                    imgsensor_is_default_cal(sensor_idx)
#define IMGSENSOR_SYSFS_GET_CAMERA_CLASS(cam_class)                                             imgsensor_sysfs_get_camera_class(cam_class)
#define IMGSENOSR_GET_SENSOR_IDX(sensor_id)                                                     imgsensor_get_sensor_idx(sensor_id)
#else
static inline int IMGSENSOR_GET_CAL_SIZE_BY_SENSOR_IDX(int sensor_idx) {
	return -1;
}
static inline int IMGSENSOR_GET_CAL_BUF_BY_SENSOR_IDX(int sensor_idx, char **buf) {
	return -1;
}
static inline int CAM_INFO_PROB(struct device_node *np) {
	return 0;
}
static inline const struct imgsensor_vendor_rom_addr *IMGSENSOR_SYSGET_ROM_ADDR_BY_ID(unsigned int dualDeviceId, unsigned int sensorId) {
	return NULL;
}
static inline unsigned int IMGSENSER_READ_OTP_CAL(unsigned int dual_device_id, unsigned int sensor_id,unsigned int addr, unsigned char *data, unsigned int size) {
	return -1;
}
static inline int IMGSENSOR_SYSFS_UPDATE(unsigned char* pRomData, unsigned int dualDeviceId, unsigned int sensorId, unsigned int offset, unsigned int length, int i4RetValue) {
	return -1;
}
static inline bool IMGSENSOR_GET_SAC_VALUE_BY_SENSOR_IDX(int sensor_idx, u8 *ac_mode, u8 *ac_time) {
	return false;
}
static inline const struct rom_cal_addr *IMGSENSOR_GET_CAL_ADDR_BY_SENSOR_IDX(int sensor_idx, enum imgsensor_cal_command command) {
	return NULL;
}
static inline bool IMGSENOSR_GET_ADAPTIVE_MIPI_STATUS(int position) {
	return false;
}
static inline int IMGSENSOR_SYSFS_GET_CAMERA_CLASS(struct class **cam_calss)
{
	return -1;
}
static inline enum IMGSENSOR_SENSOR_IDX IMGSENOSR_GET_SENSOR_IDX(unsigned short sensor_id)
{
	return IMGSENSOR_SENSOR_IDX_NONE;
}
#endif

#endif
