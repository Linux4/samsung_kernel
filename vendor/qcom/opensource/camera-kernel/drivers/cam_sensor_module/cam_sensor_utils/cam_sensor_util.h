/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _CAM_SENSOR_UTIL_H_
#define _CAM_SENSOR_UTIL_H_

#include <linux/kernel.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/of.h>
#include "cam_sensor_cmn_header.h"
#include "cam_req_mgr_util.h"
#include "cam_req_mgr_interface.h"
#include <cam_mem_mgr.h>
#include "cam_soc_util.h"
#include "cam_debug_util.h"
#include "cam_sensor_io.h"
#include "cam_csiphy_core.h"
#include "cam_sensor_dev.h"

#define INVALID_VREG 100
#define RES_MGR_GPIO_NEED_HOLD   1
#define RES_MGR_GPIO_CAN_FREE    2

/*
 * Constant Factors needed to change QTimer ticks to nanoseconds
 * QTimer Freq = 19.2 MHz
 * Time(us) = ticks/19.2
 * Time(ns) = ticks/19.2 * 1000
 */
#define QTIMER_MUL_FACTOR   10000
#define QTIMER_DIV_FACTOR   192

#define SENSOR_ID_HI847_HI1337 0x2000  // HI847 and HI1337 have same sensor id.

#if defined(CONFIG_CAMERA_ADAPTIVE_MIPI) || defined(CONFIG_SEC_GTS9_PROJECT) || defined(CONFIG_SEC_GTS9P_PROJECT) || defined(CONFIG_SEC_GTS9U_PROJECT)
#define SENSOR_ID_S5KGN3 0x08E3
#define SENSOR_ID_S5K3K1 0x30B1
#define SENSOR_ID_IMX754 0x0754
#define SENSOR_ID_S5K3LU 0x34CB
#define SENSOR_ID_IMX564 0x0564
#define SENSOR_ID_S5KHP2 0x1B72
#define SENSOR_ID_IMX258 0x0258
#define SENSOR_ID_IMX471 0x0471
#define SENSOR_ID_IMX374 0x0374
#define SENSOR_ID_S5K2LD 0x20CD
#define SENSOR_ID_S5K3J1 0x30A1

#define INVALID_MIPI_INDEX -1
#endif

#if defined(CONFIG_SENSOR_RETENTION)
#define SENSOR_RETENTION_READ_RETRY_CNT 5

#if defined(CONFIG_SEC_DM1Q_PROJECT) || defined(CONFIG_SEC_DM2Q_PROJECT) || defined(CONFIG_SEC_Q5Q_PROJECT)
#define RETENTION_SENSOR_ID 0x08E3
#elif defined(CONFIG_SEC_DM3Q_PROJECT)
#define RETENTION_SENSOR_ID 0x1B72
#elif defined(CONFIG_SEC_B5Q_PROJECT)
#define RETENTION_SENSOR_ID 0x20CD
#endif

#define STREAM_ON_ADDR   0x100

enum sensor_retention_mode {
	RETENTION_INIT = 0,
	RETENTION_READY_TO_ON,
	RETENTION_ON,
};
#endif

#if defined(CONFIG_SAMSUNG_DEBUG_SENSOR_I2C)
extern int to_do_print_vc__sen_id;
extern int to_dump_when_sof_freeze__sen_id;

typedef enum {
	e_seq_sensor_idn_s5khp2,
	e_seq_sensor_idn_s5kgn3,
	e_seq_sensor_idn_s5k3lu,
	e_seq_sensor_idn_imx564,
	e_seq_sensor_idn_imx754,
	e_seq_sensor_idn_max,
} e_seq_sensor_idnum;
#endif

int cam_sensor_count_elems_i3c_device_id(struct device_node *dev,
	int *num_entries, char *sensor_id_table_str);

int cam_sensor_fill_i3c_device_id(struct device_node *dev, int num_entries,
	char *sensor_id_table_str, struct i3c_device_id *sensor_i3c_device_id);

int cam_get_dt_power_setting_data(struct device_node *of_node,
	struct cam_hw_soc_info *soc_info,
	struct cam_sensor_power_ctrl_t *power_info);

int msm_camera_pinctrl_init
	(struct msm_pinctrl_info *sensor_pctrl, struct device *dev);

int32_t cam_sensor_util_get_current_qtimer_ns(uint64_t *qtime_ns);

int32_t cam_sensor_util_write_qtimer_to_io_buffer(
	uint64_t qtime_ns, struct cam_buf_io_cfg *io_cfg);

int cam_sensor_i2c_command_parser(struct camera_io_master *io_master,
	struct i2c_settings_array *i2c_reg_settings,
	struct cam_cmd_buf_desc *cmd_desc, int32_t num_cmd_buffers,
	struct cam_buf_io_cfg *io_cfg);

int cam_sensor_util_i2c_apply_setting(struct camera_io_master *io_master_info,
	struct i2c_settings_list *i2c_list);

int32_t cam_sensor_i2c_read_data(
	struct i2c_settings_array *i2c_settings,
	struct camera_io_master *io_master_info);

int32_t delete_request(struct i2c_settings_array *i2c_array);
int cam_sensor_util_request_gpio_table(
	struct cam_hw_soc_info *soc_info, int gpio_en);

int cam_sensor_util_init_gpio_pin_tbl(
	struct cam_hw_soc_info *soc_info,
	struct msm_camera_gpio_num_info **pgpio_num_info);
int cam_sensor_core_power_up(struct cam_sensor_power_ctrl_t *ctrl,
		struct cam_hw_soc_info *soc_info, struct completion *i3c_probe_status);

int cam_sensor_util_power_down(struct cam_sensor_power_ctrl_t *ctrl,
		struct cam_hw_soc_info *soc_info);

int msm_camera_fill_vreg_params(struct cam_hw_soc_info *soc_info,
	struct cam_sensor_power_setting *power_setting,
	uint16_t power_setting_size);

int32_t cam_sensor_update_power_settings(void *cmd_buf,
	uint32_t cmd_length, struct cam_sensor_power_ctrl_t *power_info,
	size_t cmd_buf_len);

int cam_sensor_bob_pwm_mode_switch(struct cam_hw_soc_info *soc_info,
	int bob_reg_idx, bool flag);

bool cam_sensor_util_check_gpio_is_shared(struct cam_hw_soc_info *soc_info);

static inline int cam_sensor_util_aon_ops(bool get_access, uint32_t phy_idx)
{
	CAM_DBG(CAM_SENSOR, "Updating Main/Aon operation");
	return cam_csiphy_util_update_aon_ops(get_access, phy_idx);
}

static inline int cam_sensor_util_aon_registration(uint32_t phy_idx, uint8_t aon_camera_id)
{
	CAM_DBG(CAM_SENSOR, "Register phy_idx: %u for AON_Camera_ID: %d", phy_idx, aon_camera_id);
	return cam_csiphy_util_update_aon_registration(phy_idx, aon_camera_id);
}

#endif /* _CAM_SENSOR_UTIL_H_ */
