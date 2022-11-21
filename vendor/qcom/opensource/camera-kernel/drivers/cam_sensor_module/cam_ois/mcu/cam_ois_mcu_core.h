/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 */
#ifndef _CAM_OIS_MCU_INTERFACE_H_
#define _CAM_OIS_MCU_INTERFACE_H_

#include "cam_ois_dev.h"

/**
 * @brief: cam_ois_mcu_get_dev_handle : get mcu Device Handle
 *
 * @o_ctrl: OIS ctrl structure
*
 * This API get dev handle for mcu.
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int cam_ois_mcu_get_dev_handle(struct cam_ois_ctrl_t *o_ctrl);


/**
 * @brief: cam_ois_release_dev_handle - release mcu device handle
 *
 * @o_ctrl:     ctrl structure
 * @arg:        Camera control command argument
 *
 * Returns success or failure
 */
int cam_ois_mcu_release_dev_handle(struct cam_ois_ctrl_t *o_ctrl, void *arg);


/**
 * @brief: cam_ois_mcu_apply_settings - apply mcu settings
 *
 * @o_ctrl:     ctrl structure
 * @arg:        Camera control command argument
 *
 * Returns success or failure
 */
int cam_ois_mcu_apply_settings(struct cam_ois_ctrl_t *o_ctrl, struct i2c_settings_list *i2c_list);

/**
 * @brief: cam_ois_mcu_create_thread - create thread for mcu
 *
 * @o_ctrl:     ctrl structure
 * @msg:        cam_ois_thread_msg_t struct
 *
 * Returns success or failure
 */
int cam_ois_mcu_create_thread (struct cam_ois_ctrl_t *o_ctrl);


/**
 * @brief: cam_ois_mcu_power_down : power down MCU
 *
 * @o_ctrl: OIS ctrl structure
*
 * This API get dev handle for mcu.
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int cam_ois_mcu_power_down(struct cam_ois_ctrl_t *o_ctrl);


/**
 * @brief: cam_ois_mcu_pkt_parser : handle OIS MCU packet
 *
 * @o_ctrl: OIS ctrl structure
 * @cmd_desc: cmd buf desc
 * This API get dev handle for mcu.
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int cam_ois_mcu_pkt_parser (struct cam_ois_ctrl_t *o_ctrl,
                            struct i2c_settings_array *i2c_reg_settings,
                            struct cam_cmd_buf_desc *cmd_desc);

/**
 * @brief: cam_ois_mcu_apply_setting : Apply OIS MCU Settings
 *
 * @o_ctrl: OIS ctrl structure
  * This API get dev handle for mcu.
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int cam_ois_mcu_add_msg_apply_settings (struct cam_ois_ctrl_t *o_ctrl,
                                        struct i2c_settings_array *i2c_reg_settings);


/**
 * @brief: cam_ois_mcu_shutdown : OIS MCU Shutdown
 *
 * @o_ctrl: OIS ctrl structure
  * This API get dev handle for mcu.
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int cam_ois_mcu_shutdown (struct cam_ois_ctrl_t *o_ctrl);

/**
 * @brief: cam_ois_mcu_start_dev : OIS MCU Start DEV
 *
 * @o_ctrl: OIS ctrl structure
  * This API get dev handle for mcu.
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int cam_ois_mcu_start_dev (struct cam_ois_ctrl_t *o_ctrl);

/**
 * @brief: cam_ois_mcu_stop_dev : OIS MCU Stop DEV
 *
 * @o_ctrl: OIS ctrl structure
  * This API get dev handle for mcu.
 *
 * @return Status of operation. Negative in case of error. Zero otherwise.
 */
int cam_ois_mcu_stop_dev(struct cam_ois_ctrl_t *o_ctrl);

#endif