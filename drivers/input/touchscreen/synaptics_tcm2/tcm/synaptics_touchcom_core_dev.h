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
 * @file: synaptics_touchcom_core_dev.h
 *
 * This file is the topmost header file for Synaptics TouchComm device, also
 * defines the TouchComm device context structure which will be passed to
 * all other functions that expect a device handle.
 */

#ifndef _SYNAPTICS_TOUCHCOM_CORE_DEV_H_
#define _SYNAPTICS_TOUCHCOM_CORE_DEV_H_


#include "syna_tcm2_platform.h"


#define SYNA_TCM_CORE_LIB_VERSION 0x010C


/**
 * @section: Parameters pre-defined
 *
 * @brief: MAX_NUM_OBJECTS
 *         Maximum number of objects being detected
 *
 * @brief: MAX_SIZE_GESTURE_DATA
 *         Maximum size of gesture data
 *
 * @brief: MAX_SIZE_CONFIG_ID
 *         Maximum size of customer configuration ID
 */
#define MAX_NUM_OBJECTS (10)

#define MAX_SIZE_GESTURE_DATA (8)

#define MAX_SIZE_CONFIG_ID (16)

/**
 * @section: Command-handling relevant definitions
 *
 * @brief: MESSAGE_HEADER_SIZE
 *         The size of message header
 *
 * @brief: CMD_RESPONSE_TIMEOUT_MS
 *         Time frame for a command execution
 *
 * @brief: CMD_RESPONSE_POLLING_DELAY_MS
 *         Generic time frame to check the response in polling
 *
 * @brief: RD_RETRY_US
 *         For retry reading, delay time range in microsecond
 *
 * @brief: WR_DELAY_US
 *         For continued writes, delay time range in microsecond
 *
 * @brief: TAT_DELAY_US
 *         For bus turn-around, delay time range in microsecond
 *
 * @brief: FW_MODE_SWITCH_DELAY_MS
 *         The default time for fw mode switching
 *
 * @brief: RESET_DELAY_MS
 *         The default time after reset in case it's not set properly
 *
 * @brief: FORCE_ATTN_DRIVEN
 *         Special flag to read in resp packet in ISR function
 */
#define MESSAGE_HEADER_SIZE (4)

#define CMD_RESPONSE_TIMEOUT_MS (3000)

#define CMD_RESPONSE_POLLING_DELAY_MS (10)

#define RD_RETRY_US_MIN (5000)
#define RD_RETRY_US_MAX (10000)

#define WR_DELAY_US_MIN (500)
#define WR_DELAY_US_MAX (1000)

#define TAT_DELAY_US_MIN (100)
#define TAT_DELAY_US_MAX (200)

#define FW_MODE_SWITCH_DELAY_MS (200)

#define RESET_DELAY_MS (200)

#define FORCE_ATTN_DRIVEN (~0)

/**
 * @section: Macro to show string in log
 */
#define STR(x) #x

/**
 * @section: Helpers to check the device mode
 */
#define IS_APP_FW_MODE(mode) \
	(mode == MODE_APPLICATION_FIRMWARE)

#define IS_NOT_APP_FW_MODE(mode) \
	(!IS_APP_FW_MODE(mode))

#define IS_BOOTLOADER_MODE(mode) \
	((mode == MODE_BOOTLOADER) || \
	(mode == MODE_TDDI_BOOTLOADER)  || \
	(mode == MODE_TDDI_HDL_BOOTLOADER) || \
	(mode == MODE_MULTICHIP_TDDI_BOOTLOADER))

#define IS_ROM_BOOTLOADER_MODE(mode) \
	(mode == MODE_ROMBOOTLOADER)


/**
 * @section: Types for lower-level bus being used
 */
enum bus_connection {
	BUS_TYPE_NONE,
	BUS_TYPE_I2C,
	BUS_TYPE_SPI,
	BUS_TYPE_I3C,
};

/**
 * @section: TouchComm Firmware Modes
 *
 * The current mode running is defined in Identify Info Packet.
 */
enum tcm_firmware_mode {
	MODE_UNKNOWN = 0x00,
	MODE_APPLICATION_FIRMWARE = 0x01,
	MODE_HOSTDOWNLOAD_FIRMWARE = 0x02,
	MODE_ROMBOOTLOADER = 0x04,
	MODE_BOOTLOADER = 0x0b,
	MODE_TDDI_BOOTLOADER = 0x0c,
	MODE_TDDI_HDL_BOOTLOADER = 0x0d,
	MODE_PRODUCTIONTEST_FIRMWARE = 0x0e,
	MODE_MULTICHIP_TDDI_BOOTLOADER = 0xab,
};

/**
 * @section: Status of Application Firmware
 *
 * The current status is defined in Application Info Packet.
 */
enum tcm_app_status {
	APP_STATUS_OK = 0x00,
	APP_STATUS_BOOTING = 0x01,
	APP_STATUS_UPDATING = 0x02,
	APP_STATUS_BAD_APP_CONFIG = 0xff,
};

/**
 * @section: Field IDs in Dynamic Configuration
 *
 * The codes specify the generic dynamic configuration options.
 */
enum dynamic_tcm_config_id {
	DC_UNKNOWN = 0x00,
	DC_DISABLE_DOZE = 0x01,
	DC_DISABLE_NOISE_MITIGATION = 0x02,
	DC_DISABLE_FREQUENCY_SHIFT = 0x03,
	DC_REQUEST_FREQUENCY_INDEX = 0x04,
	DC_DISABLE_HSYNC = 0x05,
	DC_REZERO_ON_EXIT_DEEP_SLEEP = 0x06,
	DC_ENABLE_CHARGER_CONNECTED = 0x07,
	DC_DISABLE_BASELINE_RELAXATION = 0x08,
	DC_ENABLE_WAKEUP_GESTURE_MODE = 0x09,
	DC_REQUEST_TESTING_FINGERS = 0x0a,
	DC_ENABLE_GRIP_SUPPRESSION = 0x0b,
	DC_ENABLE_THICK_GLOVE = 0x0c,
	DC_ENABLE_GLOVE = 0x0d,
	DC_ENABLE_FACE_DETECTION = 0x0e,
	DC_INHIBIT_ACTIVE_GESTURE = 0x0f,
	DC_DISABLE_PROXIMITY = 0x10,
};

/**
 * @section: TouchComm Commands
 *
 * List the generic commands supported in TouchComm command-response protocol.
 */
enum tcm_command {
	CMD_NONE = 0x00,
	CMD_CONTINUE_WRITE = 0x01,
	CMD_IDENTIFY = 0x02,
	CMD_RESET = 0x04,
	CMD_ENABLE_REPORT = 0x05,
	CMD_DISABLE_REPORT = 0x06,
	CMD_TCM2_ACK = 0x07,
	CMD_TCM2_RETRY = 0x08,
	CMD_TCM2_SET_MAX_READ_LENGTH = 0x09,
	CMD_TCM2_GET_REPORT = 0x0a,
	CMD_GET_BOOT_INFO = 0x10,
	CMD_ERASE_FLASH = 0x11,
	CMD_WRITE_FLASH = 0x12,
	CMD_READ_FLASH = 0x13,
	CMD_RUN_APPLICATION_FIRMWARE = 0x14,
	CMD_SPI_MASTER_WRITE_THEN_READ = 0x15,
	CMD_REBOOT_TO_ROM_BOOTLOADER = 0x16,
	CMD_RUN_BOOTLOADER_FIRMWARE = 0x1f,
	CMD_GET_APPLICATION_INFO = 0x20,
	CMD_GET_STATIC_CONFIG = 0x21,
	CMD_SET_STATIC_CONFIG = 0x22,
	CMD_GET_DYNAMIC_CONFIG = 0x23,
	CMD_SET_DYNAMIC_CONFIG = 0x24,
	CMD_GET_TOUCH_REPORT_CONFIG = 0x25,
	CMD_SET_TOUCH_REPORT_CONFIG = 0x26,
	CMD_REZERO = 0x27,
	CMD_COMMIT_CONFIG = 0x28,
	CMD_DESCRIBE_DYNAMIC_CONFIG = 0x29,
	CMD_PRODUCTION_TEST = 0x2a,
	CMD_SET_CONFIG_ID = 0x2b,
	CMD_ENTER_DEEP_SLEEP = 0x2c,
	CMD_EXIT_DEEP_SLEEP = 0x2d,
	CMD_GET_TOUCH_INFO = 0x2e,
	CMD_GET_DATA_LOCATION = 0x2f,
	CMD_DOWNLOAD_CONFIG = 0x30,
	CMD_ENTER_PRODUCTION_TEST_MODE = 0x31,
	CMD_GET_FEATURES = 0x32,
	CMD_GET_ROMBOOT_INFO = 0x40,
	CMD_WRITE_PROGRAM_RAM = 0x41,
	CMD_ROMBOOT_RUN_BOOTLOADER_FIRMWARE = 0x42,
	CMD_SPI_MASTER_WRITE_THEN_READ_EXTENDED = 0x43,
	CMD_ENTER_IO_BRIDGE_MODE = 0x44,
	CMD_ROMBOOT_DOWNLOAD = 0x45,
};

/**
 * @section: TouchComm Status Codes
 *
 * Define the following status codes for all command responses.
 *  0x00: (v1)      no commands are pending and no reports are available.
 *  0x01: (v1 & v2) the previous command succeeded.
 *  0x03: (v1 & v2) the payload continues a previous response.
 *  0x04: (v2)      command was written, but no reports were available.
 *  0x07: (v2)      the previous write was successfully received.
 *  0x08: (v2)      the previous write was corrupt. The host should resend.
 *  0x09: (v2)      the previous command failed.
 *  0x0c: (v1 & v2) write was larger than the device's receive buffer.
 *  0x0d: (v1 & v2) a command was sent before the previous command completed.
 *  0x0e: (v1 & v2) the requested command is not implemented.
 *  0x0f: (v1 & v2) generic communication error, probably incorrect payload.
 *
 *  0xfe: self-defined status for a corrupted packet.
 *  0xff: self-defined status for an invalid data.
 */
enum tcm_status_code {
	STATUS_IDLE = 0x00,
	STATUS_OK = 0x01,
	STATUS_CONTINUED_READ = 0x03,
	STATUS_NO_REPORT_AVAILABLE = 0x04,
	STATUS_ACK = 0x07,
	STATUS_RETRY_REQUESTED = 0x08,
	STATUS_CMD_FAILED = 0x09,
	STATUS_RECEIVE_BUFFER_OVERFLOW = 0x0c,
	STATUS_PREVIOUS_COMMAND_PENDING = 0x0d,
	STATUS_NOT_IMPLEMENTED = 0x0e,
	STATUS_ERROR = 0x0f,
	STATUS_PACKET_CORRUPTED = 0xfe,
	STATUS_INVALID = 0xff,
};

/**
 * @section: TouchComm Report Codes
 *
 * Define the following report codes generated by TouchComm firmware.
 *  0x10:   Identify Info Packet
 *  0x11:   Touch Report
 *  0x12:   Delta Cap. Image
 *  0x13:   Raw Cap. Image
 */
enum tcm_report_type {
	REPORT_IDENTIFY = 0x10,
	REPORT_TOUCH = 0x11,
	REPORT_DELTA = 0x12,
	REPORT_RAW = 0x13,
};

/**
 * @section: States in Command Processing
 *
 * List the states in command processing.
 */
enum tcm_command_status {
	CMD_STATE_IDLE = 0,
	CMD_STATE_BUSY = 1,
	CMD_STATE_ERROR = -1,
};

/**
 * @section: Production Test Items
 *
 * List the generic production test items
 */
enum tcm_test_code {
	TEST_NOT_IMPLEMENTED = 0x00,

	TEST_PID01_TRX_TRX_SHORTS = 0x01,
	TEST_PID02_TRX_SENSOR_OPENS = 0x02,
	TEST_PID03_TRX_GROUND_SHORTS = 0x03,
	TEST_PID05_FULL_RAW_CAP = 0x05,
	TEST_PID06_EE_SHORT = 0x06,
	TEST_PID07_DYNAMIC_RANGE = 0x07,
	TEST_PID08_HIGH_RESISTANCE = 0x08,
	TEST_PID10_DELTA_NOISE = 0x0a,
	TEST_PID11_OPEN_DETECTION = 0x0b,
	TEST_PID12 = 0x0c,
	TEST_PID13 = 0x0d,
	TEST_PID14_DOZE_DYNAMIC_RANGE = 0x0e,
	TEST_PID15_DOZE_NOISE = 0x0f,
	TEST_PID16_SENSOR_SPEED = 0x10,
	TEST_PID17_ADC_RANGE = 0x11,
	TEST_PID18_HYBRID_ABS_RAW = 0x12,
	TEST_PID29_HYBRID_ABS_NOISE = 0x1D,

	TEST_PID_MAX,
};


/**
 * @section: Internal Buffer Structure
 *
 * This structure is taken as the internal common buffer.
 */
struct tcm_buffer {
	unsigned char *buf;
	unsigned int buf_size;
	unsigned int data_length;
	syna_pal_mutex_t buf_mutex;
	unsigned char ref_cnt;
};

/**
 * @section: TouchComm Identify Info Packet
 *           Ver.1: size is 24 (0x18) bytes
 *           Ver.2: size is extended to 32 (0x20) bytes
 *
 * The identify packet provides the basic TouchComm information and indicate
 * that the device is ready to receive commands.
 *
 * The report is received whenever the device initially powers up, resets,
 * or switches fw between bootloader and application modes.
 */
struct tcm_identification_info {
	unsigned char version;
	unsigned char mode;
	unsigned char part_number[16];
	unsigned char build_id[4];
	unsigned char max_write_size[2];
	/* extension in ver.2 */
	unsigned char max_read_size[2];
	unsigned char reserved[6];
};


/**
 * @section: TouchComm Application Information Packet
 *
 * The application info packet provides the information about the application
 * firmware as well as the touch controller.
 */
struct tcm_application_info {
	unsigned char version[2];
	unsigned char status[2];
	unsigned char static_config_size[2];
	unsigned char dynamic_config_size[2];
	unsigned char app_config_start_write_block[2];
	unsigned char app_config_size[2];
	unsigned char max_touch_report_config_size[2];
	unsigned char max_touch_report_payload_size[2];
	unsigned char customer_config_id[MAX_SIZE_CONFIG_ID];
	unsigned char max_x[2];
	unsigned char max_y[2];
	unsigned char max_objects[2];
	unsigned char num_of_buttons[2];
	unsigned char num_of_image_rows[2];
	unsigned char num_of_image_cols[2];
	unsigned char has_hybrid_data[2];
	unsigned char num_of_force_elecs[2];
};

/**
 * @section: TouchComm boot information packet
 *
 * The boot info packet provides the information of TouchBoot.
 */
struct tcm_boot_info {
	unsigned char version;
	unsigned char status;
	unsigned char asic_id[2];
	unsigned char write_block_size_words;
	unsigned char erase_page_size_words[2];
	unsigned char max_write_payload_size[2];
	unsigned char last_reset_reason;
	unsigned char pc_at_time_of_last_reset[2];
	unsigned char boot_config_start_block[2];
	unsigned char boot_config_size_blocks[2];
	/* extension in ver.2 */
	unsigned char display_config_start_block[4];
	unsigned char display_config_length_blocks[2];
	unsigned char backup_display_config_start_block[4];
	unsigned char backup_display_config_length_blocks[2];
	unsigned char custom_otp_start_block[2];
	unsigned char custom_otp_length_blocks[2];
};

/**
 * @section: TouchComm ROMboot information packet
 *
 * The ROMboot info packet provides the information of ROM bootloader.
 */
struct tcm_romboot_info {
	unsigned char version;
	unsigned char status;
	unsigned char asic_id[2];
	unsigned char write_block_size_words;
	unsigned char max_write_payload_size[2];
	unsigned char last_reset_reason;
	unsigned char pc_at_time_of_last_reset[2];
};

/**
 * @section: TouchComm features description packet
 *
 * The features description packet tells which features are supported.
 */
struct tcm_features_info {
	unsigned char byte[16];
};

/**
 * @section: Data blob for touch data reported
 *
 * Once receiving a touch report generated by firmware, the touched data
 * will be parsed and converted to touch_data_blob structure as a data blob.
 *
 * @subsection: tcm_touch_data_blob
 *              The touch_data_blob contains all sorts of touched data entities.
 *
 * @subsection tcm_objects_data_blob
 *             The objects_data_blob includes the data for each active objects.
 *
 * @subsection tcm_gesture_data_blob
 *             The gesture_data_blob contains the gesture data if detected.
 */
struct tcm_objects_data_blob {
	unsigned char status;
	unsigned int x_pos;
	unsigned int y_pos;
	unsigned int x_width;
	unsigned int y_width;
	unsigned int z;
	unsigned int tx_pos;
	unsigned int rx_pos;
};
struct tcm_gesture_data_blob {
	union {
		struct {
			unsigned char tap_x[2];
			unsigned char tap_y[2];
		};
		struct {
			unsigned char swipe_x[2];
			unsigned char swipe_y[2];
			unsigned char swipe_direction[2];
		};
		unsigned char data[MAX_SIZE_GESTURE_DATA];
	};
};
struct tcm_touch_data_blob {

	/* for each active objects */
	unsigned int num_of_active_objects;
	struct tcm_objects_data_blob object_data[MAX_NUM_OBJECTS];

	/* for gesture */
	unsigned int gesture_id;
	struct tcm_gesture_data_blob gesture_data;

	/* various data */
	unsigned int timestamp;
	unsigned int buttons_state;
	unsigned int frame_rate;
	unsigned int power_im;
	unsigned int cid_im;
	unsigned int rail_im;
	unsigned int cid_variance_im;
	unsigned int nsm_frequency;
	unsigned int nsm_state;
	unsigned int num_of_cpu_cycles;
	unsigned int fd_data;
	unsigned int force_data;
	unsigned int fingerprint_area_meet;
	unsigned int sensing_mode;
};

/**
 * @section: Callback function used for custom entity parsing in touch report
 *
 * Allow to implement the custom handling for"new" custom entity in touch report
 *
 * @param
 *    [ in]    code:          the code of current touch entity
 *    [ in]    config:        the report configuration stored
 *    [in/out] config_offset: offset of current position in report config,
 *                            the updated position should be returned.
 *    [ in]    report:        touch report given
 *    [in/out] report_offset: offset of current position in touch report,
 *                            the updated position should be returned.
 *    [ in]    report_size:   size of given touch report
 *    [ in]    callback_data: pointer to caller data passed to callback function
 *
 * @return
 *    on success, 0 or positive value; otherwise, negative value on error.
 */
typedef int (*tcm_touch_data_parse_callback_t) (const unsigned char code,
		const unsigned char *config, unsigned int *config_offset,
		const unsigned char *report, unsigned int *report_offset,
		unsigned int report_size, void *callback_data);

/**
 * @section: Callback function used for custom gesture parsing in touch report
 *
 * Allow to implement the custom handling for gesture data.
 *
 * @param
 *    [ in]    code:          the code of current touch entity
 *    [ in]    config:        the report configuration stored
 *    [in/out] config_offset: offset of current position in report config,
 *                            the updated position should be returned.
 *    [ in]    report:        touch report given
 *    [in/out] report_offset: offset of current position in touch report,
 *                            the updated position should be returned.
 *    [ in]    report_size:   size of given touch report
 *    [ in]    callback_data: pointer to caller data passed to callback function
 *
 * @return
 *    on success, 0 or positive value; otherwise, negative value on error.
 */
typedef int (*tcm_gesture_parse_callback_t) (const unsigned char code,
		const unsigned char *config, unsigned int *config_offset,
		const unsigned char *report, unsigned int *report_offset,
		unsigned int report_size, void *callback_data);

/**
 * @section: TouchComm Message Handling Wrapper
 *
 * The structure contains the essential buffers and parameters to implement
 * the command-response protocol for both TouchCom ver.1 and TouchCom ver.2.
 */
struct tcm_message_data_blob {

	/* parameters for command processing */
	syna_pal_atomic_t command_status;
	unsigned char command;
	unsigned char status_report_code;
	unsigned int payload_length;
	unsigned char response_code;
	unsigned char report_code;
	unsigned char seq_toggle;

	/* completion event command processing */
	syna_pal_completion_t cmd_completion;

	/* internal buffers
	 *   in  : buffer storing the data being read 'in'
	 *   out : buffer storing the data being sent 'out'
	 *   temp: 'temp' buffer used for continued read operation
	 */
	struct tcm_buffer in;
	struct tcm_buffer out;
	struct tcm_buffer temp;

	/* mutex for the protection of command processing */
	syna_pal_mutex_t cmd_mutex;

	/* mutex for the read/write protection */
	syna_pal_mutex_t rw_mutex;

};

/**
 * @section: TouchComm core device context structure
 *
 * The device context contains parameters and internal buffers, that will
 * be passed to all other functions that expect a device handle.
 *
 * Calling syna_tcm_allocate_device() can allocate this structure, and
 * syna_tcm_remove_device() releases the structure if no longer needed.
 */
struct tcm_dev {

	/* basic device information */
	unsigned char dev_mode;
	unsigned int packrat_number;
	unsigned int max_x;
	unsigned int max_y;
	unsigned int max_objects;
	unsigned int rows;
	unsigned int cols;
	unsigned char config_id[MAX_SIZE_CONFIG_ID];

	/* capability of read/write transferred
	 * being assigned through syna_hw_interface
	 */
	unsigned int max_wr_size;
	unsigned int max_rd_size;

	/* hardware-specific data structure
	 * defined in syna_touchcom_platform.h
	 */
	struct syna_hw_interface *hw_if;

	/* TouchComm defined structures */
	struct tcm_identification_info id_info;
	struct tcm_application_info app_info;
	struct tcm_boot_info boot_info;

	/* internal buffers
	 *   report: record the TouchComm report to caller
	 *   resp  : record the command response to caller
	 */
	struct tcm_buffer report_buf;
	struct tcm_buffer resp_buf;
	struct tcm_buffer external_buf;

	/* touch report configuration */
	struct tcm_buffer touch_config;

	/* TouchComm message handling wrapper */
	struct tcm_message_data_blob msg_data;

	/* indicate that fw update is on-going */
	syna_pal_atomic_t firmware_flashing;

	/* abstraction to read a TouchComm message from device.
	 * Function will be assigned by syna_tcm_detect_device().
	 *
	 * After read_message() returned, the retrieved data will be available
	 * and stored either in buffer.report or buffer.resp based on the
	 * code returned.
	 *
	 * @param
	 *    [ in] tcm_dev:            the device handle
	 *    [out] status_report_code: status code or report code received
	 *
	 * @return
	 *    0 or positive value on success; otherwise, on error.
	 */
	int (*read_message)(struct tcm_dev *tcm_dev,
			unsigned char *status_report_code);

	/* abstraction to write a TouchComm message to device and retrieve the
	 * response to command.
	 * Function will be assigned by syna_tcm_detect_device().
	 *
	 * After calling write_message(), the response code is returned
	 * and the response data is stored in buffer.resp.
	 *
	 * @param
	 *    [ in] tcm_dev:        the device handle
	 *    [ in] command:        TouchComm command
	 *    [ in] payload:        data payload, if any
	 *    [ in] payload_length: length of data payload, if any
	 *    [out] resp_code:      response code returned
	 *    [ in] delay_ms_resp:  delay time for response reading.
	 *                          '0' is in default; and,
	 *                          'FORCE_ATTN_DRIVEN' is to read resp in ISR
	 *
	 * @return
	 *    0 or positive value on success; otherwise, on error.
	 */
	int (*write_message)(struct tcm_dev *tcm_dev,
			unsigned char command, unsigned char *payload,
			unsigned int payload_length, unsigned char *resp_code,
			unsigned int delay_ms_resp);


	/* callbacks */
	tcm_touch_data_parse_callback_t custom_touch_data_parse_func;
	void *cbdata_touch_data_parse;
	tcm_gesture_parse_callback_t custom_gesture_parse_func;
	void *cbdata_gesture_parse;

};
/* end of structure syna_tcm_dev */


/**
 * @section: Protocol detection
 *
 * @brief: syna_tcm_v1_detect
 *         Check whether TouchComm ver.1 firmware is running
 *
 * @brief: syna_tcm_v2_detect
 *         Check whether TouchComm ver.2 firmware is running
 */

/* syna_tcm_v1_detect()
 *
 * Check whether TouchComm ver.1 firmware is running.
 * Function is implemented in synaptics_tcm2_core_v1.c.
 *
 * @param
 *    [ in] tcm_dev: the device handle
 *    [ in] data:    raw 4-byte data
 *    [ in] size:    length of input data in bytes
 *
 * @return
 *    on success, 0 or positive value; otherwise, negative value on error.
 */
int syna_tcm_v1_detect(struct tcm_dev *tcm_dev, unsigned char *data,
		unsigned int data_len);

/* syna_tcm_v2_detect()
 *
 * Check whether TouchComm ver.2 firmware is running.
 * Function is implemented in synaptics_tcm2_core_v2.c.
 *
 * @param
 *    [ in] tcm_dev: the device handle
 *    [ in] data:    raw 4-byte data
 *    [ in] size:    length of input data in bytes
 *
 * @return
 *    on success, 0 or positive value; otherwise, negative value on error.
 */
int syna_tcm_v2_detect(struct tcm_dev *tcm_dev, unsigned char *data,
		unsigned int data_len);

/**
 * @section: Buffers Management helpers
 *
 * @brief: syna_tcm_buf_alloc
 *         Allocate the requested memory space for the buffer structure
 *
 * @brief: syna_tcm_buf_realloc
 *         Extend the requested memory space for the buffer structure
 *
 * @brief: syna_tcm_buf_init
 *         Initialize the buffer structure
 *
 * @brief: syna_tcm_buf_lock
 *         Protect the access of current buffer structure
 *
 * @brief: syna_tcm_buf_unlock
 *         Open the access of current buffer structure
 *
 * @brief: syna_tcm_buf_release
 *         Release the buffer structure
 */

/**
 * syna_tcm_buf_alloc()
 *
 * Allocate the requested memory space for the given buffer only if
 * the existed buffer is not enough for the requirement.
 *
 * @param
 *    [ in] pbuf: pointer to an internal buffer
 *    [ in] size: required size to be allocated
 *
 * @return
 *     0 or positive value on success; otherwise, on error.
 */
static inline int syna_tcm_buf_alloc(struct tcm_buffer *pbuf,
		unsigned int size)
{
	if (!pbuf) {
		LOGE("Invalid buffer structure\n");
		return -1;
	}

	if (size > pbuf->buf_size) {
		if (pbuf->buf)
			syna_pal_mem_free((void *)pbuf->buf);

		pbuf->buf = syna_pal_mem_alloc(size, sizeof(unsigned char));
		if (!(pbuf->buf)) {
			LOGE("Fail to allocate memory (size = %d)\n",
				(int)(size*sizeof(unsigned char)));
			pbuf->buf_size = 0;
			pbuf->data_length = 0;
			return -1;
		}
		pbuf->buf_size = size;
	}

	syna_pal_mem_set(pbuf->buf, 0x00, pbuf->buf_size);
	pbuf->data_length = 0;

	return 0;
}

/**
 * syna_tcm_buf_realloc()
 *
 * Extend the requested memory space for the given buffer only if
 * the existed buffer is not enough for the requirement.
 * Then, move the content to the new memory space.
 *
 * @param
 *    [ in] pbuf: pointer to an internal buffer
 *    [ in] size: required size to be extended
 *
 * @return
 *     0 or positive value on success; otherwise, on error.
 */
static inline int syna_tcm_buf_realloc(struct tcm_buffer *pbuf,
		unsigned int size)
{
	int retval;
	unsigned char *temp_src;
	unsigned int temp_size = 0;

	if (!pbuf) {
		LOGE("Invalid buffer structure\n");
		return -1;
	}

	if (size > pbuf->buf_size) {
		temp_src = pbuf->buf;
		temp_size = pbuf->buf_size;

		pbuf->buf = syna_pal_mem_alloc(size, sizeof(unsigned char));
		if (!(pbuf->buf)) {
			LOGE("Fail to allocate memory (size = %d)\n",
				(int)(size * sizeof(unsigned char)));
			syna_pal_mem_free((void *)temp_src);
			pbuf->buf_size = 0;
			return -1;
		}

		retval = syna_pal_mem_cpy(pbuf->buf,
				size,
				temp_src,
				temp_size,
				temp_size);
		if (retval < 0) {
			LOGE("Fail to copy data\n");
			syna_pal_mem_free((void *)temp_src);
			syna_pal_mem_free((void *)pbuf->buf);
			pbuf->buf_size = 0;
			return retval;
		}

		syna_pal_mem_free((void *)temp_src);
		pbuf->buf_size = size;
	}

	return 0;
}
/**
 * syna_tcm_buf_init()
 *
 * Initialize the buffer structure.
 *
 * @param
 *    [ in] pbuf: pointer to an internal buffer
 *
 * @return
 *     none
 */
static inline void syna_tcm_buf_init(struct tcm_buffer *pbuf)
{
	pbuf->buf_size = 0;
	pbuf->data_length = 0;
	pbuf->ref_cnt = 0;
	pbuf->buf = NULL;
	syna_pal_mutex_alloc(&pbuf->buf_mutex);
}
/**
 * syna_tcm_buf_lock()
 *
 * Protect the access of current buffer structure.
 *
 * @param
 *    [ in] pbuf: pointer to an internal buffer
 *
 * @return
 *     none
 */
static inline void syna_tcm_buf_lock(struct tcm_buffer *pbuf)
{
	if (pbuf->ref_cnt != 0) {
		LOGE("Buffer access out-of balance, %d\n", pbuf->ref_cnt);
		return;
	}

	syna_pal_mutex_lock(&pbuf->buf_mutex);
	pbuf->ref_cnt++;
}
/**
 * syna_tcm_buf_unlock()
 *
 * Open the access of current buffer structure.
 *
 * @param
 *    [ in] pbuf: pointer to an internal buffer
 *
 * @return
 *     none
 */
static inline void syna_tcm_buf_unlock(struct tcm_buffer *pbuf)
{
	if (pbuf->ref_cnt != 1) {
		LOGE("Buffer access out-of balance, %d\n", pbuf->ref_cnt);
		return;
	}

	pbuf->ref_cnt--;
	syna_pal_mutex_unlock(&pbuf->buf_mutex);
}
/**
 * syna_tcm_buf_release()
 *
 * Release the buffer structure.
 *
 * @param
 *    [ in] pbuf: pointer to an internal buffer
 *
 * @return
 *     none
 */
static inline void syna_tcm_buf_release(struct tcm_buffer *pbuf)
{
	if (pbuf->ref_cnt != 0)
		LOGE("Buffer access hold, %d\n", pbuf->ref_cnt);

	syna_pal_mutex_free(&pbuf->buf_mutex);
	syna_pal_mem_free((void *)pbuf->buf);
	pbuf->buf_size = 0;
	pbuf->data_length = 0;
	pbuf->ref_cnt = 0;
}
/**
 * syna_tcm_buf_copy()
 *
 * Helper to copy data from the source buffer to the destination buffer.
 * The size of destination buffer may be reallocated, if the size is
 * smaller than the actual data size to be copied.
 *
 * @param
 *    [out] dest: pointer to an internal buffer
 *    [ in] src:  required size to be extended
 *
 * @return
 *     0 or positive value on success; otherwise, on error.
 */
static inline int syna_tcm_buf_copy(struct tcm_buffer *dest,
		struct tcm_buffer *src)
{
	int retval = 0;

	if (dest->buf_size < src->data_length) {
		retval = syna_tcm_buf_alloc(dest, src->data_length + 1);
		if (retval < 0) {
			LOGE("Fail to reallocate the given buffer, size: %d\n",
				src->data_length + 1);
			return retval;
		}
	}

	/* copy data content to the destination */
	retval = syna_pal_mem_cpy(dest->buf,
			dest->buf_size,
			src->buf,
			src->buf_size,
			src->data_length);
	if (retval < 0) {
		LOGE("Fail to copy data to caller, size: %d\n",
			src->data_length);
		return retval;
	}

	dest->data_length = src->data_length;

	return retval;
}
/**
 * @section: Reads / Writes Abstraction Function
 *
 * @brief: syna_tcm_read
 *         Read the data from bus directly
 *
 * @brief: syna_tcm_write
 *         Write the data to bus directly
 */

/**
 * syna_tcm_read()
 *
 * The bare read function, reading in the requested data bytes
 * from bus directly.
 *
 * @param
 *    [ in] tcm_dev: the device handle
 *    [out] rd_data: buffer for storing data retrieved from device
 *    [ in] rd_len:  length of reading data in bytes
 *
 * @return
 *    the number of data bytes retrieved;
 *    otherwise, negative value, on error.
 */
static inline int syna_tcm_read(struct tcm_dev *tcm_dev,
	unsigned char *rd_data, unsigned int rd_len)
{
	struct syna_hw_interface *hw_if;

	if (!tcm_dev) {
		LOGE("Invalid tcm device handle\n");
		return _EINVAL;
	}

	hw_if = tcm_dev->hw_if;
	if (!hw_if->ops_read_data) {
		LOGE("Invalid hw ops_read function\n");
		return _ENODEV;
	}

	return hw_if->ops_read_data(hw_if, rd_data, rd_len);
}

/**
 * syna_tcm_write()
 *
 * The bare write function, writing the given data bytes to bus directly.
 *
 * @param
 *    [ in] tcm_dev:  the device handle
 *    [ in] wr_data:  written data
 *    [ in] wr_len:   length of written data in bytes
 *
 * @return
 *    the number of data bytes retrieved;
 * otherwise, negative value, on error.
 */
static inline int syna_tcm_write(struct tcm_dev *tcm_dev,
	unsigned char *wr_data, unsigned int wr_len)
{
	struct syna_hw_interface *hw_if;

	if (!tcm_dev) {
		LOGE("Invalid tcm device handle\n");
		return _EINVAL;
	}

	hw_if = tcm_dev->hw_if;
	if (!hw_if->ops_write_data) {
		LOGE("Invalid hw ops_write function\n");
		return _ENODEV;
	}

	return hw_if->ops_write_data(hw_if, wr_data, wr_len);
}


#endif /* end of _SYNAPTICS_TOUCHCOM_CORE_DEV_H_ */
