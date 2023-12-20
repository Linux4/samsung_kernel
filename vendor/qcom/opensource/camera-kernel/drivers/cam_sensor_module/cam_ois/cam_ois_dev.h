/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#ifndef _CAM_OIS_DEV_H_
#define _CAM_OIS_DEV_H_

#include <linux/i2c.h>
#include <linux/gpio.h>
#include <media/v4l2-event.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-ioctl.h>
#include <media/cam_sensor.h>
#include <cam_sensor_i2c.h>
#include <cam_sensor_spi.h>
#include <cam_sensor_io.h>
#include <cam_cci_dev.h>
#include <cam_req_mgr_util.h>
#include <cam_req_mgr_interface.h>
#include <cam_mem_mgr.h>
#include <cam_subdev.h>
#include "cam_soc_util.h"
#include "cam_context.h"
#if defined(CONFIG_SAMSUNG_OIS_MCU_STM32)
#include <linux/wait.h>
#include <linux/freezer.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#endif

#define DEFINE_MSM_MUTEX(mutexname) \
	static struct mutex mutexname = __MUTEX_INITIALIZER(mutexname)

#define OIS_DRIVER_I2C "cam-i2c-ois"
#define OIS_DRIVER_I3C "i3c_camera_ois"

#if defined(CONFIG_SAMSUNG_OIS_MCU_STM32)
#if defined(CONFIG_SEC_DM3Q_PROJECT)
#define MAX_BRIDGE_COUNT (3)
#else
#define MAX_BRIDGE_COUNT (2)
#endif

#define OIS_VER_SIZE  (8)
#define NUM_AF_POSITION (4096)

struct cam_ois_shift_table_t {
	bool ois_shift_used;
	int16_t ois_shift_x[NUM_AF_POSITION];
	int16_t ois_shift_y[NUM_AF_POSITION];
};

enum cam_ois_thread_msg_type {
	CAM_OIS_THREAD_MSG_START,
	CAM_OIS_THREAD_MSG_APPLY_SETTING,
	CAM_OIS_THREAD_MSG_RESET,
	CAM_OIS_THREAD_MSG_MAX
};

struct cam_ois_thread_msg_t {
	struct list_head list;
	int msg_type;
	uint16_t ois_mode;
	struct i2c_settings_array *i2c_reg_settings;
};

typedef struct sysboot_info_type_t{
	uint32_t ver;
	uint32_t id;
} sysboot_info_type;

struct ois_sensor_interface {
	void *core;
	void (*ois_func)(void *);
};
#endif

enum cam_ois_state {
	CAM_OIS_INIT,
	CAM_OIS_ACQUIRE,
	CAM_OIS_CONFIG,
	CAM_OIS_START,
};

/**
 * struct cam_ois_i2c_info_t - I2C info
 * @slave_addr      :   slave address
 * @i2c_freq_mode   :   i2c frequency mode
 *
 */
struct cam_ois_i2c_info_t {
	uint16_t slave_addr;
	uint8_t i2c_freq_mode;
};

/**
 * struct cam_ois_soc_private - ois soc private data structure
 * @ois_name        :   ois name
 * @i2c_info        :   i2c info structure
 * @power_info      :   ois power info
 *
 */
struct cam_ois_soc_private {
	const char *ois_name;
	struct cam_ois_i2c_info_t i2c_info;
	struct cam_sensor_power_ctrl_t power_info;
};

/**
 * struct cam_ois_intf_params - bridge interface params
 * @device_hdl   : Device Handle
 * @session_hdl  : Session Handle
 * @ops          : KMD operations
 * @crm_cb       : Callback API pointers
 */
struct cam_ois_intf_params {
	int32_t device_hdl;
	int32_t session_hdl;
	int32_t link_hdl;
	struct cam_req_mgr_kmd_ops ops;
	struct cam_req_mgr_crm_cb *crm_cb;
};

#if defined(CONFIG_SAMSUNG_OIS_ADC_TEMPERATURE_SUPPORT)
/**
 * struct adc_temperature_table - adc_temperature table params
 * @adc   : adc
 * @temperature  : temperature
 */
struct adc_temperature_table {
	uint32_t adc;
	int temperature;
};
#endif

/**
 * struct cam_ois_ctrl_t - OIS ctrl private data
 * @device_name     :   ois device_name
 * @pdev            :   platform device
 * @ois_mutex       :   ois mutex
 * @soc_info        :   ois soc related info
 * @io_master_info  :   Information about the communication master
 * @cci_i2c_master  :   I2C structure
 * @v4l2_dev_str    :   V4L2 device structure
 * @is_i3c_device   :   A Flag to indicate whether this OIS is I3C Device or not.
 * @bridge_intf     :   bridge interface params
 * @i2c_fwinit_data :   ois i2c firmware init settings
 * @i2c_init_data   :   ois i2c init settings
 * @i2c_mode_data   :   ois i2c mode settings
 * @i2c_time_data   :   ois i2c time write settings
 * @i2c_calib_data  :   ois i2c calib settings
 * @ois_device_type :   ois device type
 * @cam_ois_state   :   ois_device_state
 * @ois_fw_flag     :   flag for firmware download
 * @is_ois_calib    :   flag for Calibration data
 * @opcode          :   ois opcode
 * @device_name     :   Device name
 *
 */
struct cam_ois_ctrl_t {
	char device_name[CAM_CTX_DEV_NAME_MAX_LENGTH];
	struct platform_device *pdev;
	struct mutex ois_mutex;
	struct cam_hw_soc_info soc_info;
	struct camera_io_master io_master_info;
	enum cci_i2c_master_t cci_i2c_master;
	enum cci_device_num cci_num;
	struct cam_subdev v4l2_dev_str;
	bool is_i3c_device;
#if defined(CONFIG_SAMSUNG_OIS_MCU_STM32)
	struct cam_ois_intf_params bridge_intf[MAX_BRIDGE_COUNT];
	int bridge_cnt;
#else
	struct cam_ois_intf_params bridge_intf;
#endif
	struct i2c_settings_array i2c_fwinit_data;
	struct i2c_settings_array i2c_init_data;
	struct i2c_settings_array i2c_calib_data;
	struct i2c_settings_array i2c_mode_data;
	struct i2c_settings_array i2c_time_data;
	enum msm_camera_device_type_t ois_device_type;
	enum cam_ois_state cam_ois_state;
	char ois_name[32];
	uint8_t ois_fw_flag;
	uint8_t is_ois_calib;
	struct cam_ois_opcode opcode;
#if defined(CONFIG_SAMSUNG_OIS_MCU_STM32)
	int start_cnt;
	bool is_power_up;
	bool is_servo_on;
	bool is_config;
	char cal_ver[OIS_VER_SIZE + 1];
	char module_ver[OIS_VER_SIZE + 1];
	char phone_ver[OIS_VER_SIZE + 1];
	char load_fw_name[256];
	struct cam_ois_shift_table_t shift_tbl[2];
	uint16_t module;
	uint16_t ois_mode;
	uint32_t x_center;
	uint32_t y_center;
	uint32_t err_reg;
	uint32_t gyro_raw_x;
	uint32_t gyro_raw_y;
	uint32_t gyro_raw_z;
	uint32_t efs_cal;
	uint32_t poles[6];
	uint32_t gyro_orientation;
	struct mutex ois_mode_mutex;
	struct task_struct *ois_thread;
	bool is_thread_started;
	struct cam_ois_thread_msg_t list_head_thread;
	spinlock_t thread_spinlock;
	wait_queue_head_t wait;
	struct mutex i2c_init_data_mutex;
	struct mutex i2c_mode_data_mutex;
	struct mutex i2c_time_data_mutex;

	uint32_t slave_addr;
	uint32_t slave_id;
	sysboot_info_type info;
	uint32_t reset_ctrl_gpio;
	uint32_t boot0_ctrl_gpio;
	bool sysfs_ois_power;
#if defined(CONFIG_SAMSUNG_OIS_ADC_TEMPERATURE_SUPPORT)
	struct adc_temperature_table *adc_temperature_table;
	uint32_t adc_arr_size;
	bool sysfs_ois_init;
#endif
#endif
};

/**
 * @brief : API to register OIS hw to platform framework.
 * @return struct platform_device pointer on on success, or ERR_PTR() on error.
 */
int cam_ois_driver_init(void);

/**
 * @brief : API to remove OIS Hw from platform framework.
 */
void cam_ois_driver_exit(void);
#endif /*_CAM_OIS_DEV_H_ */
