/*
 * File:shub_core.h
 * Author:bao.yue@spreadtrum.com
 *
 * Copyright (C) 2018 Spreadtrum Communications Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 */

#ifndef SPRD_SHUB_CORE_H
#define SPRD_SHUB_CORE_H

#include "shub_common.h"

#define SENSOR_TYPE_CALIBRATION_CFG				26
#define SHUB_NAME                   "sprd-sensor"
#define SHUB_RD                   "shub_rd"
#define SHUB_RD_NWU                   "shub_rd_nwu"
#define SHUB_LIGHT_CALI_DATA_FILE    "/mnt/vendor/productinfo/sensor_calibration_data/light"
#define SHUB_PROX_CALI_DATA_FILE    "/mnt/vendor/productinfo/sensor_calibration_data/proximity"
#define SHUB_ACC_CALI_DATA_FILE    "/mnt/vendor/productinfo/sensor_calibration_data/acc"
#define SHUB_SENSOR_NUM		7
#define SHUB_NODATA                 0xff
#define HOST_REQUEST_WRITE           0x74
#define HOST_REQUEST_READ            0x75
#define HOST_REQUEST_LEN             2
#define RECEIVE_TIMEOUT_MS           100
#define MAX_SENSOR_LOG_CTL_FLAG_LEN	8
#define LOG_CTL_OUTPUT_FLAG	5
#define SIPC_PM_BUFID0             0
#define SIPC_PM_BUFID1             1
#define SHUB_IIO_CHN_BITS             64
/* light sensor calibrate min value is 280lux */
#define LIGHT_SENSOR_MIN_VALUE  50
/* light sensor calibrate max value is 520lux */
#define LIGHT_SENSOR_MAX_VALUE  10000
#define LIGHT_CALI_DATA_COUNT   5
/* light sensor calibrate value is 400lux; Due to kernel seldom use
 * float data, so calibrate value multiply 10000
 */
#define LIGHT_SENSOR_CALI_VALUE (400 * LIGHT_SENSOR_CALI_DIV)
#define LIGHT_SENSOR_CALI_DIV   (10000)
/* prox sensor auto calibrate ground noise min value is 0 */
#define PROX_SENSOR_MIN_VALUE   0

/* ms,-1 is wait  forever */
#define SIPC_WRITE_TIMEOUT             -1

#define LIGHT_CALI_LEAK_BACKLIGHT_LV    1638//102

enum calib_cmd {
	CALIB_EN,
	CALIB_CHECK_STATUS,
	CALIB_DATA_WRITE,
	CALIB_DATA_READ,
	CALIB_FLASH_WRITE,
	CALIB_FLASH_READ,
	CALIB_ALS_DARK,
	CALIB_ALS_LEAK,
	CALIB_ALS_STD,
	CALIB_PS_NOCOVER,
	CALIB_PS_NEAR,
	CALIB_PS_FAR,
	CALIB_ACC_HORIZONTAL,
	CALIB_NONE,
};

enum calib_type {
	CALIB_TYPE_NON,
	CALIB_TYPE_DEFAULT,
	CALIB_TYPE_SELFTEST,
	CALIB_TYPE_SENSORS_ENABLE,
	CALIB_TYPE_SENSORS_DISABLE,
};

enum calib_status {
	CALIB_STATUS_OUT_OF_MINRANGE = -3,
	CALIB_STATUS_OUT_OF_RANGE = -2,
	CALIB_STATUS_FAIL = -1,
	CALIB_STATUS_NON = 0,
	CALIB_STATUS_INPROCESS = 1,
	CALIB_STATUS_PASS = 2,
};

struct  sensor_info {
	u8 en;
	u8 mode;
	u8 rate;
	u16 timeout;
};

struct sensor_report_format {
	u8 cmd;
	u8 length;
	u16 id;
	u8 data[];
};

/*
 * Description:
 * Control Handle
 */
enum handle_id {
	NON_WAKEUP_HANDLE,
	WAKEUP_HANDLE,
	INTERNAL_HANDLE,
	HANDLE_ID_END
};

enum scan_status {
	SHUB_SCAN_ID,
	SHUB_SCAN_RAW_0,
	SHUB_SCAN_RAW_1,
	SHUB_SCAN_RAW_2,
	SHUB_SCAN_TIMESTAMP,
};

enum shub_mode {
	SHUB_BOOT,
	SHUB_OPDOWNLOAD,
	SHUB_NORMAL,
	SHUB_SLEEP,
	SHUB_NO_SLEEP,
};

enum shub_cmd {
	/* MCU Internal Cmd: Range 0~63 */
	/* Driver Control enable/disable/set rate */
	MCU_CMD = 0,
	MCU_SENSOR_EN,
	MCU_SENSOR_DSB,
	MCU_SENSOR_SET_RATE,
	/* Sensors Calibrator */
	MCU_SENSOR_CALIB,
	MCU_SENSOR_SELFTEST,
	/* User profile */
	MCU_SENSOR_SETTING,
	KNL_CMD = 64,
	KNL_SEN_DATA,
	 /* for multi package use */
	KNL_SEN_DATAPAC,
	KNL_EN_LIST,
	KNL_LOG,
	KNL_RESET_SENPWR,
	KNL_CALIBINFO,
	KNL_CALIBDATA,
	HAL_CMD = 128,
	HAL_SEN_DATA,
	HAL_FLUSH,
	HAL_LOG,
	HAL_SENSOR_INFO,
	HAL_LOG_CTL,
	SPECIAL_CMD = 192,
	MCU_RDY,
	COMM_DRV_RDY,
};

enum als_c_err{
	CALI_ERR_NONE         = 0x00,    //None error
	CALI_ERR_OOR          = 0x01,    //Data out of range
	CALI_ERR_MD           = 0x02,    //The necessary data is missing
	CALI_ERR_NCMD         = 0x03,    //No such command
	CALI_ERR_BUSY         = 0x04,    //Calibration already running
	CALI_ERR_CNSAVE       = 0x05,    //Calibration data cannot save
	CALI_ERR_BLW          = 0x06,    //Backlight level wrong
	CALI_ERR_CRD          = 0x07,    //Cannot read rawdata
	CALI_ERR_CSHUB        = 0x08,    //Cannot send calibration data to hub
	CALI_ERR_NMGAIN       = 0x09,    //Again not match
	CALI_ERR_CM4CMDE      = 0x0A,    //CM4 command error
	CALI_ERR_WSTATUS      = 0x0B,    //Other cali wrong status
};

struct sensor_batch_cmd {
	int handle;
	int report_rate;
	s64 batch_timeout;
};

struct sensor_log_control {
	u8 cmd;
	u8 length;
	u8 debug_data[5];
	u32 udata[MAX_SENSOR_LOG_CTL_FLAG_LEN];
};

struct als_cali_data {
	int lux_coeff;
	int ch0_coeff;
	int ch1_coeff;
	int ch2_coeff;
	int ch3_coeff;
	int ch4_coeff;
};

struct customer_data_get {
	u8 type;
	u8 customer_data[30];
	u8 length;
};

struct shub_data {
	struct platform_device *sensor_pdev;
	enum shub_mode mcu_mode;
	/* enable & batch list */
	u32 enabled_list[HANDLE_ID_END];
	u32 interrupt_status;
	/* sipc sensorhub id value */
	u32 sipc_sensorhub_id;
	/* Calibrator status */
	int cal_cmd;
	int cal_type;
	int cal_id;
	int golden_sample;
	char calibrated_data[CALIBRATION_DATA_LENGTH];
	int cal_savests;
	s32 iio_data[6];
	struct iio_dev *indio_dev;
	struct irq_work iio_irq_work;
	struct iio_trigger  *trig;
	atomic_t pseudo_irq_enable;
	struct class *sensor_class;
	struct device *sensor_dev;
	wait_queue_head_t rxwq;
	struct mutex mutex_lock;	/* mutex for trigger and raw read */
	struct mutex mutex_read;	/* mutex for sipc read */
	struct mutex mutex_send;	/* mutex for sending event */
	bool rx_status;
	u8 *rx_buf;
	u32 rx_len;
	/* sprd begin */
	wait_queue_head_t  rw_wait_queue;
	struct sent_cmd  sent_cmddata;
	struct mutex send_command_mutex;	/* mutex for sending command */
	u8 *regs_addr_buf;
	u8 *regs_value_buf;
	u8 regs_num;
	struct file *filep;/* R/W interface */
	unsigned char readbuff[SERIAL_READ_BUFFER_MAX];
	unsigned char readbuff_nwu[SERIAL_READ_BUFFER_MAX];
	unsigned char writebuff[MAX_MSG_BUFF_SIZE];
	void (*save_mag_offset)(struct shub_data *sensor, u8 *buff, u32 len);
	void (*data_callback)(struct shub_data *sensor, u8 *buff, u32 len);
	void (*readcmd_callback)(struct shub_data *sensor, u8 *buff, u32 len);
	void (*resp_cmdstatus_callback)(struct shub_data *sensor,
					u8 *buff, u32 len);
	void (*cm4_read_callback)(struct shub_data *sensor,
				  enum shub_subtype_id subtype,
				  u8 *buff, u32 len);
	void (*dynamic_read)(struct shub_data *sensor);
	struct sensor_log_control log_control;
	struct workqueue_struct *driver_wq;
	struct delayed_work time_sync_work;
	atomic_t delay;
	struct work_struct download_cali_data_work;
	struct work_struct savecalifile_work;
	struct notifier_block early_suspend;
	struct notifier_block shub_reboot_notifier;
	int is_sensorhub;
	u8 cm4_operate_data[6];
	struct customer_data_get dynamic_data_get;
	struct mutex mutex_alsraw_lock;
	struct als_cali_data alsCoeff;
	u8 als_cali_result;
	u8 als_cali_cmd;
	bool caliRunning;
	//struct gpio_desc * vled_pin;
	int vled_pin;
};

extern struct shub_data *g_sensor;
/*hw sensor id*/
enum _id_status {
	_IDSTA_NOT = 0,
	_IDSTA_OK  = 1,
	_IDSTA_FAIL = 2,
};

enum show_order {
	ORDER_ACC = 1,
	ORDER_MAG = 2,
	ORDER_GYRO = 4,
	ORDER_LIGHT = 5,
	ORDER_PRS = 6,
	ORDER_PROX = 8,
	ORDER_COLOR_TEMP = 121,
};

#define _HW_SENSOR_TOTAL 7
#define VENDOR_ID_OFFSET 256
struct hw_sensor_id_tag {
	u8 id_status;
	u8 vendor_id;
	u8 chip_id;
	char pname[128];
};

struct id_to_name_tag {
	u32 id;
	char *pname;
};

struct acc_gyro_cali_data {
	int x_bias;
	int y_bias;
	int z_bias;
	int x_raw_data;
	int y_raw_data;
	int z_raw_data;
};

struct sensor_event {
    u8 Cmd;
    u8 Length;
    u16 HandleID;
    union {
        u8 udata[4];
        struct {
            int8_t status;
            int8_t type;
            };
        };
        union {
            float fdata[6];
            struct {
                float uncalib[3];
                float bias[3];
            };
        };
    int64_t timestamp;
};

#pragma pack(1)
struct prox_cali_data {
	int ground_noise;
	int high_threshold;
	int low_threshold;
	u8 cali_flag;
};
#pragma pack()

#define ACC_MAX_X_Y_BIAS_VALUE 20000 /* units: (1/10000) m/s^2 */
#define ACC_MAX_Z_BIAS_VALUE 25000 /* units: (1/10000) m/s^2 */
#define GYRO_MAX_X_Y_Z_BIAS_VALUE 4000 /* units: (1/10000) rad/s */

#endif
