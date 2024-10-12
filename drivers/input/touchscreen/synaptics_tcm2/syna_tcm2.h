/* SPDX-License-Identifier: GPL-2.0
 *
 * Synaptics TouchCom touchscreen driver
 *
 * Copyright (C) 2017-2020 Synaptics Incorporated. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * INFORMATION CONTAINED IN THIS DOCUMENT IS PROVIDED "AS-IS," AND SYNAPTICS
 * EXPRESSLY DISCLAIMS ALL EXPRESS AND IMPLIED WARRANTIES, INCLUDING ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE,
 * AND ANY WARRANTIES OF NON-INFRINGEMENT OF ANY INTELLECTUAL PROPERTY RIGHTS.
 * IN NO EVENT SHALL SYNAPTICS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, PUNITIVE, OR CONSEQUENTIAL DAMAGES ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OF THE INFORMATION CONTAINED IN THIS DOCUMENT, HOWEVER CAUSED
 * AND BASED ON ANY THEORY OF LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, AND EVEN IF SYNAPTICS WAS ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE. IF A TRIBUNAL OF COMPETENT JURISDICTION DOES
 * NOT PERMIT THE DISCLAIMER OF DIRECT DAMAGES OR ANY OTHER DAMAGES, SYNAPTICS'
 * TOTAL CUMULATIVE LIABILITY TO ANY PARTY SHALL NOT EXCEED ONE HUNDRED U.S.
 * DOLLARS.
 */

/**
 * @file syna_tcm2.h
 *
 * The header file is used for the Synaptics TouchComm reference driver.
 * Platform-specific functions and included headers are implemented in
 * syna_touchcom_platform.h and syna_touchcom_runtime.h.
 */

#ifndef _SYNAPTICS_TCM2_DRIVER_H_
#define _SYNAPTICS_TCM2_DRIVER_H_

#include "sec_ts.h"

#include "syna_tcm2_platform.h"
#include "synaptics_touchcom_core_dev.h"
#include "synaptics_touchcom_func_touch.h"

#define PLATFORM_DRIVER_NAME "synaptics_tcm"

#define TOUCH_INPUT_NAME "synaptics_tcm_touch"
#define TOUCH_INPUT_PHYS_PATH "synaptics_tcm/touch_input"

#define CHAR_DEVICE_NAME "tcm"
#define CHAR_DEVICE_MODE (0x0600)

#define SYNAPTICS_TCM_DRIVER_ID (1 << 0)
#define SYNAPTICS_TCM_DRIVER_VERSION 0x0100
#define SYNAPTICS_TCM_DRIVER_SUBVER 6

/**
 * @section: Driver Configurations
 *
 * The macros in the driver files below are used for doing compile time
 * configuration of the driver.
 */

/**
 * @brief: TYPE_B_PROTOCOL
 *         Open to enable the multi-touch (MT) protocol
 */
#define TYPE_B_PROTOCOL

/**
 * @brief: RESET_ON_RESUME
 *         Open if willing to issue a reset to the touch controller
 *         from suspend.
 *         Set "disable" in default.
 */
/* #define RESET_ON_RESUME */

/**
 * @brief ENABLE_WAKEUP_GESTURE
 *        Open if having wake-up gesture support.
 */
#define ENABLE_WAKEUP_GESTURE

/**
 * @brief REPORT_SWAP_XY
 *        Open if trying to swap x and y position coordinate reported.
 * @brief REPORT_FLIP_X
 *        Open if trying to flip x position coordinate reported.
 * @brief REPORT_FLIP_Y
 *        Open if trying to flip x position coordinate reported.
 */
/* #define REPORT_SWAP_XY */
/* #define REPORT_FLIP_X */
/* #define REPORT_FLIP_Y */

/**
 * @brief REPORT_TOUCH_WIDTH
 *        Open if willing to add the width data to the input event.
 */
#define REPORT_TOUCH_WIDTH

/**
 * @brief USE_CUSTOM_TOUCH_REPORT_CONFIG
 *        Open if willing to set up the format of touch report.
 *        The custom_touch_format[] array in syna_tcm2.c can be used
 *        to describe the customized report format.
 */
/* #define USE_CUSTOM_TOUCH_REPORT_CONFIG */

/**
 * @brief STARTUP_REFLASH
 *        Open if willing to do fw checking and update at startup.
 *        The firmware image will be obtained by request_firmware() API,
 *        so please ensure the image is built-in or included properly.
 *
 *        This property is available only when SYNA_TCM2_REFLASH
 *        feature is enabled.
 */
#ifdef CONFIG_TOUCHSCREEN_SYNA_TCM2_REFLASH
/* #define STARTUP_REFLASH */
#endif
/**
 * @brief  MULTICHIP_DUT_REFLASH
 *         Open if willing to do fw update and the DUT belongs to multi-chip
 *         product. This property dependent on STARTUP_REFLASH property.
 *
 *         Set "disable" in default.
 */
#if defined(CONFIG_TOUCHSCREEN_SYNA_TCM2_REFLASH) || defined(STARTUP_REFLASH)
/* #define MULTICHIP_DUT_REFLASH */
#endif

/**
 * @brief  ENABLE_DISP_NOTIFIER
 *         Open if having display notification event and willing to listen
 *         the event from display driver.
 *
 *         Set "disable" in default due to no generic notifier for DRM
 */
#if defined(CONFIG_FB) || defined(CONFIG_DRM_PANEL)
/* #define ENABLE_DISP_NOTIFIER */
#endif
/**
 * @brief RESUME_EARLY_UNBLANK
 *        Open if willing to resume in early un-blanking state.
 *
 *        This property is available only when ENABLE_DISP_NOTIFIER
 *        feature is enabled.
 */
#ifdef ENABLE_DISP_NOTIFIER
/* #define RESUME_EARLY_UNBLANK */
#endif
/**
 * @brief  USE_DRM_PANEL_NOTIFIER
 *         Open if willing to listen the notification event from
 *         DRM_PANEL. Please be noted that 'struct drm_panel_notifier'
 *         must be implemented in the target BSP.
 *
 *        This property is available only when ENABLE_DISP_NOTIFIER
 *        feature is enabled.
 *
 *         Set "disable" in default due to no generic notifier for DRM
 */
#if defined(ENABLE_DISP_NOTIFIER) && defined(CONFIG_DRM_PANEL)
#define USE_DRM_PANEL_NOTIFIER
#endif

/**
 * @brief ENABLE_EXTERNAL_FRAME_PROCESS
 *        Open if having external frame process to the userspace application.
 *
 *        Set "enable" in default
 *
 * @brief REPORT_TYPES
 *        Total types of report being used for external frame process.
 *
 * @brief EFP_ENABLE / EFP_DISABLE
 *        Specific value to label whether the report is required to be
 *        process or not.
 *
 * @brief REPORT_CONCURRENTLY
 *        Open if willing to concurrently handle reports for both kernel
 *        and userspace application.
 *
 *        Set "disable" in default
 */
#define ENABLE_EXTERNAL_FRAME_PROCESS
#define REPORT_TYPES (256)
#define EFP_ENABLE	(1)
#define EFP_DISABLE (0)
/* #define REPORT_CONCURRENTLY */

/**
 * @brief TCM_CONNECT_IN_PROBE
 *        Open if willing to detect and connect to TouchComm device at
 *        probe function; otherwise, please invoke connect() manually.
 *
 *        Set "enable" in default
 */
#define TCM_CONNECT_IN_PROBE

/**
 * @brief FORCE_CONNECTION
 *        Open if willing to connect to TouchComm device w/o error outs.
 *
 *        Set "disable" in default
 */
/* #define FORCE_CONNECTION */

#define REFLASH_ERASE_DELAY (800)
#define REFLASH_WRITE_DELAY (50)

/**
 * @brief ENABLE_CUSTOM_TOUCH_ENTITY
 *        Open if having custom requirements to parse the custom code
 *        entity in the touch report.
 *
 *        Set "disable" in default
 */
/* #define ENABLE_CUSTOM_TOUCH_ENTITY */

/**
 * @brief  #define SEC_EAR_DETECTION
 *         Open if haveing native ear(face) detection support
 */
/* #define SEC_NATIVE_EAR_DETECTION */

/**
 * @brief  #define SEC_EVENT_16_BITS
 *         Open if haveing 16-bit format generated by the firmware
 */
#define SEC_EVENT_16_BITS


/**
 * @section: SEC Power States
 */
enum power_state {
	PWR_ON = 0,
	PWR_OFF,
	LP_MODE,
};

enum display_deep_sleep_state_id {
	SLEEP_NO_CHANGE = 0,
	SLEEP_IN = 1,
	SLEEP_OUT = 2,
};

/**
 * @brief: SEC Custom Commands, Reports, or Events
 */
enum custom_command {
	CMD_BSC_DO_CALIBRATION = 0x33,
	CMD_READ_SEC_SPONGE_REG = 0xc0,
	CMD_WRITE_SEC_SPONGE_REG = 0xc1,
	CMD_GET_FACE_AREA = 0xc3,
	CMD_ACCESS_CALIB_DATA_FROM_NVM = 0xe7,
};

enum custom_report_type {
	REPORT_SEC_GESTURE_EVENT = 0xc0,
	REPORT_SEC_COORDINATE_EVENT = 0xc1,
	REPORT_SEC_STATUS_EVENT = 0xc2,
};

enum custom_touch_report_code {
	TOUCH_REPORT_DISPLAY_DEEP_SLEEP_STATE = 0xc0,
};

enum custom_test_code {
	TEST_FACE_AREA = 0xc3,
};

enum custom_dynamic_config_id {
	DC_TEST_MOISTURE_EVENT = 0xdc,
	DC_TEST_GESTURE_EVENT = 0xdd,
	DC_TSP_SNR_TEST_FRAMES = 0xe1,
};

/**
 * @brief: context of SEC touch/coordinate event data
 */
struct sec_touch_event_data {
	union {
		struct {
			unsigned char eid:2;
			unsigned char tid:4;
			unsigned char tchsta:2;
			unsigned char x_11_4;
			unsigned char y_11_4;
			unsigned char y_3_0:4;
			unsigned char x_3_0:4;
			unsigned char major;
			unsigned char minor;
			unsigned char z:6;
			unsigned char ttype_3_2:2;
#if defined(SEC_EVENT_16_BITS)
			unsigned char left_event:5;
			unsigned char max_energy_flag:1;
			unsigned char ttype_1_0:2;
			unsigned char noise_level;
			unsigned char max_sensitivity;
			unsigned char hover_id_num:4;
			unsigned char location_area:4;
			unsigned char reserved_byte11;
			unsigned char reserved_byte12;
			unsigned char reserved_byte13;
			unsigned char reserved_byte14;
			unsigned char reserved_byte15;
#else
			unsigned char left_event:6;
			unsigned char ttype_1_0:2;
#endif
		} __packed;
#if defined(SEC_EVENT_16_BITS)
		unsigned char data[16];
#else
		unsigned char data[8];
#endif
	};
};

/**
 * @brief: context of SEC status event data
 */
struct sec_status_event_data {
	union {
		struct {
			unsigned char eid:2;
			unsigned char stype:4;
			unsigned char sf:2;
			unsigned char status_id;
			unsigned char status_data_1;
			unsigned char status_data_2;
			unsigned char status_data_3;
			unsigned char status_data_4;
			unsigned char status_data_5;
			unsigned char left_event_5_0:6;
			unsigned char reserved_byte07_7_6:2;
#if defined(SEC_EVENT_16_BITS)
			unsigned char reserved_byte08;
			unsigned char reserved_byte09;
			unsigned char reserved_byte10;
			unsigned char reserved_byte11;
			unsigned char reserved_byte12;
			unsigned char reserved_byte13;
			unsigned char reserved_byte14;
			unsigned char reserved_byte15;
#endif
		} __packed;
#if defined(SEC_EVENT_16_BITS)
		unsigned char data[16];
#else
		unsigned char data[8];
#endif
	};
};

/**
 * @brief: context of SEC gesture event data
 */
struct sec_gesture_event_data {
	union {
		struct {
			unsigned char eid:2;
			unsigned char type:4;
			unsigned char sf:2;
			unsigned char gesture_id;
			unsigned char gesture_data_1;
			unsigned char gesture_data_2;
			unsigned char gesture_data_3;
			unsigned char gesture_data_4;
			unsigned char gesture_data_5;
			unsigned char left_event_5_0:6;
			unsigned char reserved_byte07_7_6:2;
#if defined(SEC_EVENT_16_BITS)
			unsigned char reserved_byte08;
			unsigned char reserved_byte09;
			unsigned char reserved_byte10;
			unsigned char reserved_byte11;
			unsigned char reserved_byte12;
			unsigned char reserved_byte13;
			unsigned char reserved_byte14;
			unsigned char reserved_byte15;
#endif
		} __packed;
#if defined(SEC_EVENT_16_BITS)
		unsigned char data[16];
#else
		unsigned char data[8];
#endif
	};
};

/**
 * @brief: context of the synaptics linux-based driver
 *
 * The structure defines the kernel specific data in linux-based driver
 */
struct syna_tcm {

	/* TouchComm device core context */
	struct tcm_dev *tcm_dev;

	/* PLatform device driver */
	struct platform_device *pdev;

	/* Generic touched data generated by tcm core lib */
	struct tcm_touch_data_blob tp_data;

	syna_pal_mutex_t tp_event_mutex;

	unsigned char prev_obj_status[MAX_NUM_OBJECTS];

	/* Buffer stored the irq event data */
	struct tcm_buffer event_data;

	/* Hardware interface layer */
	struct syna_hw_interface *hw_if;

	/* ISR-related variables */
	pid_t isr_pid;
	bool irq_wake;

	/* cdev and sysfs nodes creation */
	struct cdev char_dev;
	dev_t char_dev_num;
	int char_dev_ref_count;

	struct class *device_class;
	struct device *device;

	struct kobject *sysfs_dir;

	/* Input device registration */
	struct input_dev *input_dev;
	struct input_params {
		unsigned int max_x;
		unsigned int max_y;
		unsigned int max_objects;
	} input_dev_params;

	/* Workqueue used for fw update */
	struct delayed_work reflash_work;
	struct workqueue_struct *reflash_workqueue;

	/* IOCTL-related variables */
	pid_t proc_pid;
	struct task_struct *proc_task;

	/* flags */
	bool slept_in_early_suspend;
	bool lpwg_enabled;
	bool is_attn_redirecting;
	unsigned char fb_ready;
	bool is_connected;

	/* framebuffer callbacks notifier */
#if defined(ENABLE_DISP_NOTIFIER)
	struct notifier_block fb_notifier;
#endif

	/* fifo to pass the report to userspace */
	unsigned int fifo_remaining_frame;
	struct list_head frame_fifo_queue;
	wait_queue_head_t wait_frame;
	unsigned char report_to_queue[REPORT_TYPES];

	/* For SEC specified parameters */
	struct sec_cmd_data sec;
	struct completion resume_done;
	int ed_enable;
	long prox_power_off;
	unsigned char hover_event;
	struct input_dev *input_dev_proximity;
	unsigned int prev_fd_data;
	int lp_state;
	unsigned int display_deep_sleep_state;
	unsigned int scrub_id;
	unsigned int scrub_x;
	unsigned int scrub_y;
	struct sec_ts_coordinate ts_coord[MAX_NUM_OBJECTS];
	unsigned char pre_ttype[MAX_NUM_OBJECTS];
	unsigned char pre_action[MAX_NUM_OBJECTS];
	unsigned int wet_count;			/* wet mode count */
	unsigned int touch_count;
	int (*dev_read_sponge)(struct syna_tcm *tcm,
		unsigned char *rd_buf, int size_buf,
		unsigned short offset, unsigned int rd_len);
	int (*dev_write_sponge)(struct syna_tcm *tcm,
		unsigned char *wr_buf, int size_buf,
		unsigned short offset, unsigned int wr_len);

	/* Specific function pointer to do device connection.
	 *
	 * This function will power on and identify the connected device.
	 * At the end of function, the ISR will be registered as well.
	 *
	 * @param
	 *    [ in] tcm: the driver handle
	 *
	 * @return
	 *    on success, 0; otherwise, negative value on error.
	 */
	int (*dev_connect)(struct syna_tcm *tcm);

	/* Specific function pointer to disconnect the device
	 *
	 * This function will power off the connected device.
	 * Then, all the allocated resource will be released.
	 *
	 * @param
	 *    [ in] tcm: the driver handle
	 *
	 * @return
	 *    on success, 0; otherwise, negative value on error.
	 */
	int (*dev_disconnect)(struct syna_tcm *tcm);

	/* Specific function pointer to set up app fw firmware
	 *
	 * This function should be called whenever the device initially
	 * powers up, resets, or firmware update.
	 *
	 * @param
	 *    [ in] tcm: the driver handle
	 *
	 * @return
	 *    on success, 0; otherwise, negative value on error.
	 */
	int (*dev_set_up_app_fw)(struct syna_tcm *tcm);

	/* Specific function pointer to enter normal sensing mode
	 *
	 * @param
	 *    [ in] tcm: tcm driver handle
	 *
	 * @return
	 *    on success, 0; otherwise, negative value on error.
	 */
	int (*dev_set_normal_sensing)(struct syna_tcm *tcm);

	/* Specific function pointer to to enter power-saved sensing mode.
	 *
	 * @param
	 *    [ in] tcm: tcm driver handle
	 *
	 * @return
	 *    on success, 0; otherwise, negative value on error.
	 */
	int (*dev_set_lowpwr_sensing)(struct syna_tcm *tcm);
#if defined(SEC_NATIVE_EAR_DETECTION)
	/* Specific function pointer to enable/disable proximity detection
	 *
	 * @param
	 *    [ in] tcm: tcm driver handle
	 *    [ in] en:  flag to enable or disable
	 *
	 * @return
	 *    on success, 0; otherwise, negative value on error.
	 */
	int (*dev_detect_prox)(struct syna_tcm *tcm, bool en);
#endif

};

/**
 * @brief: Helpers for cdevice nodes and sysfs nodes creation
 *
 * These functions are implemented in syna_touchcom_sysfs.c
 * and available only when CONFIG_TOUCHSCREEN_SYNA_TCM2_SYSFS is enabled.
 */
#ifdef CONFIG_TOUCHSCREEN_SYNA_TCM2_SYSFS

int syna_cdev_create_sysfs(struct syna_tcm *ptcm,
		struct platform_device *pdev);

void syna_cdev_remove_sysfs(struct syna_tcm *ptcm);

void syna_cdev_redirect_attn(struct syna_tcm *ptcm);

#ifdef ENABLE_EXTERNAL_FRAME_PROCESS
void syna_cdev_update_report_queue(struct syna_tcm *tcm,
		unsigned char code, struct tcm_buffer *pevent_data);
#endif

#endif

/**
 * @section: Custom functions being compatible with SEC's driver
 */

/**
 * syna_sec_fn_init()
 *
 * Create and register SEC command interface, also create a sysfs
 * directory including the sec features
 *
 * @param
 *    [ in] tcm:  the driver handle
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
int syna_sec_fn_init(struct syna_tcm *ptcm);

/**
 * syna_sec_fn_remove()
 *
 * Remove the allocate sysfs directory
 *
 * @param
 *    [ in] tcm:  the driver handle
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
void syna_sec_fn_remove(struct syna_tcm *ptcm);


/**
 * syna_sec_fn_create_dir()
 *
 * Create a directory and register with sysfs.
 *
 * @param
 *    [ in] tcm:  the driver handle
 *    [ in] sysfs_dir: root directory of sysfs nodes
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
int syna_sec_fn_create_dir(struct syna_tcm *tcm,
		struct kobject *sysfs_dir);


#endif /* end of _SYNAPTICS_TCM2_DRIVER_H_ */

