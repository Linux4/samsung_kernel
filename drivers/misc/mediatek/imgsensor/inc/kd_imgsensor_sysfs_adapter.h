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

#define PASSTHROUGH

#ifdef CONFIG_IMGSENSOR_SYSFS
#define IMGSENSOR_GET_CAL_SIZE_BY_SENSOR_IDX(sensor_idx)                                        imgsensor_get_cal_size_by_sensor_idx(sensor_idx)
#define IMGSENSOR_GET_CAL_BUF_BY_SENSOR_IDX(sensor_idx, buf)                                    imgsensor_get_cal_buf_by_sensor_idx(sensor_idx, buf)
#define CAM_INFO_PROB(device_node_ptr)                                                          cam_info_probe(device_node_ptr)
#define IMGSENSOR_SYSGET_ROM_ADDR_BY_ID(dual_device_id, sensor_id)                              imgsensor_sys_get_rom_addr_by_id(dual_device_id, sensor_id)
#define IMGSENSOR_SYSFS_UPDATE(rom_data, dual_device_id, sensor_id, offset, length, ret_value)  imgsensor_sysfs_update(rom_data, dual_device_id, sensor_id, offset, length, ret_value)
#define IMGSENSOR_GET_SAC_VALUE_BY_SENSOR_IDX(sensor_idx, ac_mode, ac_time)                     imgsensor_get_sac_value_by_sensor_idx(sensor_idx, ac_mode, ac_time)
#define IMGSENSOR_GET_CAL_ADDR_BY_SENSOR_IDX(sensor_idx, command)                               imgsensor_get_cal_addr_by_sensor_idx(sensor_idx, command)
#define IMGSENOSR_GET_ADAPTIVE_MIPI_STATUS(position)                                            imgsensor_get_adaptive_mipi_status(position)
#else
#define IMGSENSOR_GET_CAL_SIZE_BY_SENSOR_IDX(sensor_idx)                                        -1
#define IMGSENSOR_GET_CAL_BUF_BY_SENSOR_IDX(sensor_idx, buf)                                    -1
#define CAM_INFO_PROB(device_node_ptr)                                                          PASSTHROUGH
#define IMGSENSOR_SYSGET_ROM_ADDR_BY_ID(dual_device_id, sensor_id)                              NULL
#define IMGSENSOR_SYSFS_UPDATE(rom_data, dual_device_id, sensor_id, offset, length, ret_value)  -1
#define IMGSENSOR_GET_SAC_VALUE_BY_SENSOR_IDX(sensor_idx, ac_mode, ac_time)                     false
#define IMGSENSOR_GET_CAL_ADDR_BY_SENSOR_IDX(sensor_idx, command)                               NULL
#define IMGSENOSR_GET_ADAPTIVE_MIPI_STATUS(position)                                            false
#endif

#endif
