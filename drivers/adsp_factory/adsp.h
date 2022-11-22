/*
 *  Copyright (C) 2012, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */
#ifndef __ADSP_SENSOR_H__
#define __ADSP_SENSOR_H__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/fcntl.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
//#include <linux/sensors.h>
#include <linux/adsp/adsp_ft_common.h>

#define TIMEOUT_CNT 200
#define TIMEOUT_DHR_CNT 50

#define PATH_LEN                 50
#define FILE_BUF_LEN             110
#define ID_INDEX_NUMS            2
#define RETRY_MAX                3
#define VERSION_FILE_NAME_LEN    20

#ifdef CONFIG_SUPPORT_LIGHT_CALIBRATION
#define LIGHT_UB_CELL_ID_INFO_STRING_LENGTH 23
#define UB_CELL_ID_PATH "/sys/class/lcd/panel/SVC_OCTA"
#define SUB_UB_CELL_ID_PATH "/sys/class/lcd/panel/SVC_OCTA2"
#endif
enum {
	D_FACTOR,
	R_COEF,
	G_COEF,
	B_COEF,
	C_COEF,
	CT_COEF,
	CT_OFFSET,
	THD_HIGH,
	THD_LOW,
	IRIS_PROX_THD,
	SUM_CRC,
	EFS_SAVE_NUMS,
};

enum {
	ID_UTYPE,
	ID_BLACK,
	ID_WHITE,
	ID_GOLD,
	ID_SILVER,
	ID_GREEN,
	ID_BLUE,
	ID_PINKGOLD,
	ID_MAX,
};

#if defined(CONFIG_SUPPORT_BHL_COMPENSATION_FOR_LIGHT_SENSOR) || \
	defined(CONFIG_SUPPORT_BRIGHT_SYSFS_COMPENSATION_LUX) || \
	defined(CONFIG_SUPPORT_BRIGHT_COMPENSATION_LUX)
enum {
	OPTION_TYPE_COPR_ENABLE,
	OPTION_TYPE_BOLED_ENABLE,
	OPTION_TYPE_LCD_ONOFF,
	OPTION_TYPE_GET_COPR,
	OPTION_TYPE_GET_CHIP_ID,
	OPTION_TYPE_SET_HALLIC_INFO,
	OPTION_TYPE_GET_LIGHT_CAL,
	OPTION_TYPE_SET_LIGHT_CAL,
	OPTION_TYPE_SET_LCD_VERSION,
	OPTION_TYPE_SET_UB_DISCONNECT,
	OPTION_TYPE_GET_LIGHT_DEBUG_INFO,
	OPTION_TYPE_SET_DEVICE_MODE,
	OPTION_TYPE_SET_PANEL_STATE,
	OPTION_TYPE_SET_PANEL_TEST_STATE,
	OPTION_TYPE_SET_AUTO_BRIGHTNESS_HYST,
	OPTION_TYPE_SET_PANEL_SCREEN_MODE,
	OPTION_TYPE_MAX
};
#endif

/* Main struct containing all the data */
struct adsp_data {
	struct device *adsp;
	struct device *sensor_device[MSG_SENSOR_MAX];
	struct device_attribute **sensor_attr[MSG_SENSOR_MAX];
	struct sock *adsp_skt;
	int32_t *msg_buf[MSG_SENSOR_MAX];
	unsigned int ready_flag[MSG_TYPE_MAX];
	bool sysfs_created[MSG_SENSOR_MAX];
	struct mutex prox_factory_mutex;
	struct mutex light_factory_mutex;
	struct mutex accel_factory_mutex;
	struct mutex flip_cover_factory_mutex;
	struct mutex remove_sysfs_mutex;
	struct notifier_block adsp_nb;
#ifdef CONFIG_VBUS_NOTIFIER
	struct notifier_block vbus_nb;
#endif
	int32_t fac_fstate;
#ifdef CONFIG_SUPPORT_LIGHT_CALIBRATION
	struct delayed_work light_cal_work;
	int32_t light_cal_result;
	int32_t light_cal1;
	int32_t light_cal2;
	int32_t copr_w;
	char light_ub_id[LIGHT_UB_CELL_ID_INFO_STRING_LENGTH];
#endif

	int32_t temp_reg;
	uint32_t support_algo;
	bool restrict_mode;
	int32_t brightness_info[6];
	int32_t pre_bl_level;
	int32_t pre_panel_state;
	int32_t pre_panel_idx;
	int32_t pre_test_state;
	int32_t pre_screen_mode;
    int32_t light_debug_info_cmd;
#ifdef CONFIG_SUPPORT_PROX_CALIBRATION
	int32_t prox_cal;
#endif
	struct delayed_work pressure_cal_work;
	int turn_over_crash;
	int32_t hyst[4];
};

#ifdef CONFIG_SEC_FACTORY
int get_mag_raw_data(int32_t *raw_data);
#endif
int get_accel_raw_data(int32_t *raw_data);
int get_prox_raw_data(int *raw_data, int *offset);
int adsp_get_sensor_data(int sensor_type);
int adsp_factory_register(unsigned int type,
	struct device_attribute *attributes[]);
int adsp_factory_unregister(unsigned int type);
#if defined(CONFIG_VBUS_NOTIFIER)
struct adsp_data* adsp_ssc_core_register(unsigned int type,
	struct device_attribute *attributes[]);
struct adsp_data* adsp_ssc_core_unregister(unsigned int type);
#endif
int adsp_unicast(void *param, int param_size, u16 sensor_type,
	u32 portid, u16 msg_type);
int sensors_register(struct device **dev, void *drvdata,
	struct device_attribute *attributes[], char *name);
void sensors_unregister(struct device *dev,
	struct device_attribute *attributes[]);
void accel_factory_init_work(void);
#ifdef CONFIG_VBUS_NOTIFIER
void sns_vbus_init_work(void);
#endif
#ifdef CONFIG_SUPPORT_LIGHT_CALIBRATION
void light_cal_init_work(struct adsp_data *data);
void light_cal_read_work_func(struct work_struct *work);
int light_load_ub_cell_id_from_file(char *path, char *data_str);
#ifdef CONFIG_SUPPORT_DUAL_OPTIC
int light_save_ub_cell_id_to_efs(char *path, char *data_str, bool first_booting);
#else
int light_save_ub_cell_id_to_efs(char *data_str, bool first_booting);
#endif
#endif /* CONFIG_SUPPORT_LIGHT_CALIBRATION */
#ifdef CONFIG_SUPPORT_BHL_COMPENSATION_FOR_LIGHT_SENSOR
void light_factory_init_work(struct adsp_data *data);
#endif
#ifdef CONFIG_SUPPORT_PROX_CALIBRATION
void prox_cal_init_work(struct adsp_data *data);
void prox_send_cal_data(struct adsp_data *data, bool fac_cal);
#endif
#ifdef CONFIG_SUPPORT_PROX_POWER_ON_CAL
void prox_factory_init_work(void);
#endif
#ifdef CONFIG_SUPPORT_PROX_CALIBRATION
void prox_cal_init_work(struct adsp_data *data);
void prox_cal_read_work_func(struct work_struct *work);
#endif
#if defined(CONFIG_SUPPORT_BHL_COMPENSATION_FOR_LIGHT_SENSOR) || \
	defined(CONFIG_SUPPORT_BRIGHT_SYSFS_COMPENSATION_LUX)
int get_light_sidx(struct adsp_data *data);
#endif
#ifdef CONFIG_LPS22HH_FACTORY
void pressure_factory_init_work(struct adsp_data *data);
void pressure_cal_work_func(struct work_struct *work);
#endif
#endif /* __ADSP_SENSOR_H__ */
