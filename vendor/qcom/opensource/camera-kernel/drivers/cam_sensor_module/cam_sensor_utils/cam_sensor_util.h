/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
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

#if defined(CONFIG_CAMERA_ADAPTIVE_MIPI)
#define SENSOR_ID_S5KGN3 0x08E3
#define SENSOR_ID_S5KHM3 0x1AD3
#define SENSOR_ID_IMX374 0x0374
#define SENSOR_ID_S5KGH1 0x0881
#define SENSOR_ID_IMX563 0x0563
#define SENSOR_ID_S5K3K1 0x30B1
#define SENSOR_ID_IMX754 0x0754

#define INVALID_MIPI_INDEX -1
#endif

#if defined(CONFIG_SENSOR_RETENTION)
#define SENSOR_RETENTION_READ_RETRY_CNT 2

#if defined(CONFIG_SEC_R0Q_PROJECT) || defined(CONFIG_SEC_G0Q_PROJECT)
#define RETENTION_SENSOR_ID 0x08E3
#elif defined(CONFIG_SEC_B0Q_PROJECT)
#define RETENTION_SENSOR_ID 0x1AD3
#endif

#define STREAM_ON_ADDR   0x100

enum sensor_retention_mode {
	RETENTION_INIT = 0,
	RETENTION_READY_TO_ON,
	RETENTION_ON,
};
#endif

#if defined(CONFIG_CAMERA_AEB_OFF_CONTROL)
#if defined(CONFIG_SEC_B0Q_PROJECT)
#define AEB_SENSOR_ID 0x1AD3
#else
#define AEB_SENSOR_ID 0
#endif
#endif // defined(CONFIG_CAMERA_AEB_OFF_CONTROL)

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
		struct cam_hw_soc_info *soc_info);

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

static inline int cam_sensor_util_aon_registration(uint32_t phy_idx, bool aon_user)
{
	CAM_DBG(CAM_SENSOR, "Regsiter phy_idx: %u for AON operatoin: %d", phy_idx, aon_user);
	return cam_csiphy_util_update_aon_registration(phy_idx, aon_user);
}

#endif /* _CAM_SENSOR_UTIL_H_ */
