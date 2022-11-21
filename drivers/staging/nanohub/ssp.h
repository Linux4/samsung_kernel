/*
 *  Copyright (C) 2011, Samsung Electronics Co. Ltd. All Rights Reserved.
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

#ifndef __SSP_PRJ_H__
#define __SSP_PRJ_H__
#include "chub.h"
#include <linux/platform_data/nanohub.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>

#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/timer.h>
#include <linux/list.h>
#include <linux/rtc.h>
#include <linux/time.h>
#include <linux/regulator/consumer.h>
#include "ssp_platformdata.h"
#include <linux/spi/spi.h>
#include <linux/sec_class.h>
#include "sensor_list.h"
#include <linux/vmalloc.h> /* memory scraps issue. */
#include <linux/platform_device.h>

#include "ssp_sensorhub.h"

#ifdef CONFIG_PANEL_NOTIFY
#include "linux/panel_notify.h"
#endif

#include "ssp_dump.h"
#include "wakelock_wrapper.h"
//#include <linux/wakelock.h>

#include "ssp_platform.h"

#define vfs_read(a,b,c,d) 0

/* SENSOR CALIBRATION INFORMATION */
#define GYRO_CAL_FILE_INDEX		0
#define GYRO_CAL_DATA_SIZE		12

#define MAG_CAL_FILE_INDEX		12
#define MAG_CAL_DATA_SIZE		13

#define LIGHT_CAL_FILE_INDEX		25
#define LIGHT_CAL_DATA_SIZE		8

#define PROX_CAL_FILE_INDEX		33
#define PROX_CAL_DATA_SIZE		8

#define SVC_OCTA_FILE_INDEX		41
#define SVC_OCTA_DATA_SIZE		22

#define SENSOR_CAL_FILE_SIZE		63
/* END OF SENSOR CALIBRATION INFORMATION */

#define SUCCESS		1
#define FAIL		0
#define ERROR		-1

#define FACTORY_DATA_MAX	99

/* ssp mcu device ID */
#define DEVICE_ID			0x55

#define ssp_dbg(format, ...) \
	pr_info(format, ##__VA_ARGS__)

#define func_dbg() \
	pr_info("[SSP]: %s\n", __func__)

#define data_dbg(format, ...) \
	pr_info(format, ##__VA_ARGS__)

#define DEFAULT_POLLING_DELAY	(200 * NSEC_PER_MSEC)
#define PROX_AVG_READ_NUM	80
#define DATA_PACKET_SIZE	2000

 // this packet size related when AP send ssp packet to MCU.
#define MAX_SSP_PACKET_SIZE	1000
#define SSP_INSTRUCTION_PACKET  9 + 16

#define SSP_DEBUG_ON		"SSP:DEBUG=1"
#define SSP_DEBUG_OFF		"SSP:DEBUG=0"

#define SSP_DEBUG_TIME_FLAG_ON		"SSP:DEBUG_TIME=1"
#define SSP_DEBUG_TIME_FLAG_OFF		"SSP:DEBUG_TIME=0"

#define SSP_HALL_IC_ON			"SSP:HALL_IC=1"
#define SSP_HALL_IC_OFF			"SSP:HALL_IC=0"

#define SSP_FSM_SETTING			"SSP:FSM_SETTING=1"
#define SSP_FSM_SETTING_PATH		"/efs/FactoryApp/fsm_setting.txt"

#define SSP_SENSOR_CAL_READ		"SSP:SENSOR_CAL_READ"

#define SSP_ACC_POSITION		"SSP:ACC_POSITION="
#define SSP_MAG_POSITION		"SSP:MAG_POSITION="

#define SSP_HUB_COMMAND			"SSP:HUB="
#define SSP_FILE_MANAGER_READ	"SSP:FM_READ="
#define SSP_FILE_MANAGER_WRITE	"SSP:FM_WRITE="

#define SSP_LIGHT_SEAMLESS_THD "SSP:LIGHT_SEAMLESS_THD="
#define SSP_SENSOR_HAL_INIT "SSP:SENSOR_HAL_INIT"

#define SSP_PMS_HYSTERESYIS		"SSP:PMS="
#define SSP_AUTO_ROTATION_ORIENTATION "SSP:AUTO_ROTATION_ORIENTATION="
#define SSP_SAR_BACKOFF_MOTION_NOTI "SSP:SAR_BACKOFF_MOTION_NOTI="

extern bool ssp_debug_time_flag;
extern bool ssp_pkt_dbg;


#define ssp_debug_time(format, ...) \
	do { \
		if (unlikely(ssp_debug_time_flag)) \
			pr_info(format, ##__VA_ARGS__); \
	} while (0)

/*
 * SENSOR_DELAY_SET_STATE
 * Check delay set to avoid sending ADD instruction twice
 */
enum {
	INITIALIZATION_STATE = 0,
	NO_SENSOR_STATE,
	ADD_SENSOR_STATE,
	RUNNING_SENSOR_STATE,
};

/* for MSG2SSP_AP_GET_THERM */
enum {
	ADC_BATT = 0,
	ADC_CHG,
};

enum {
	SENSORS_BATCH_DRY_RUN			   = 0x00000001,
	SENSORS_BATCH_WAKE_UPON_FIFO_FULL   = 0x00000002
};

enum {
	META_DATA_FLUSH_COMPLETE = 1,
};

#define SSP_INVALID_REVISION		99999

/* Gyroscope DPS */
#define GYROSCOPE_DPS250		250
#define GYROSCOPE_DPS500		500
#define GYROSCOPE_DPS1000		1000
#define GYROSCOPE_DPS2000		2000

/* AP -> SSP Instruction */
#define MSG2SSP_INST_BYPASS_SENSOR_ADD	0xA1
#define MSG2SSP_INST_BYPASS_SENSOR_REMOVE	0xA2
#define MSG2SSP_INST_REMOVE_ALL		0xA3
#define MSG2SSP_INST_CHANGE_DELAY		0xA4
#define MSG2SSP_INST_LIBRARY_ADD		0xB1
#define MSG2SSP_INST_LIBRARY_REMOVE		0xB2
#define MSG2SSP_AP_SET_THERMISTOR_TABLE	0xB3
#define MSG2SSP_INST_LIB_NOTI		0xB4
#define MSG2SSP_INST_VDIS_FLAG		0xB5
#define MSG2SSP_INST_LIB_DATA		0xC1
#define MSG2SSP_INST_CURRENT_TIMESTAMP	0xa7

#define MSG2SSP_AP_MCU_SET_GYRO_CAL		0xCD
#define MSG2SSP_AP_MCU_SET_ACCEL_CAL		0xCE
#define MSG2SSP_AP_MCU_SET_PROX_CAL		0xCF
#define MSG2SSP_AP_STATUS_SHUTDOWN		0xD0
#define MSG2SSP_AP_STATUS_WAKEUP		0xD1
#define MSG2SSP_AP_STATUS_SLEEP		0xD2
#define MSG2SSP_AP_STATUS_RESUME		0xD3
#define MSG2SSP_AP_STATUS_SUSPEND		0xD4
#define MSG2SSP_AP_STATUS_RESET		0xD5
#define MSG2SSP_AP_STATUS_POW_CONNECTED	0xD6
#define MSG2SSP_AP_STATUS_POW_DISCONNECTED	0xD7
#define MSG2SSP_AP_STATUS_SCONTEXT_WAKEUP	0x97
#define MSG2SSP_AP_STATUS_SCONTEXT_SLEEP	0x98
#define MSG2SSP_AP_SAR_BACKOFF_MOTION_NOTI	0x9B
#define MSG2SSP_AP_MCU_SET_DUMPMODE		0xDB
#define MSG2SSP_AP_MCU_DUMP_CHECK		0xDC
#define MSG2SSP_AP_MCU_BATCH_FLUSH		0xDD
#define MSG2SSP_AP_MCU_BATCH_COUNT		0xDF
#define MSG2SSP_AP_MCU_GET_ACCEL_RANGE	0xE1


#if defined(CONFIG_SEC_VIB_NOTIFIER)
#define MSG2SSP_AP_MCU_SET_MOTOR_STATUS		0xC3
#endif

#define MSG2SSP_AP_SENSORS_NAME		0x0D
#define MSG2SSP_AP_WHOAMI			0x0F
#define MSG2SSP_AP_FIRMWARE_REV		0xF0
#define MSG2SSP_AP_SENSOR_FORMATION		0xF1
#define MSG2SSP_AP_SENSOR_PROXTHRESHOLD	0xF2
#define MSG2SSP_AP_SENSOR_BARCODE_EMUL	0xF3
#define MSG2SSP_AP_SENSOR_SCANNING		0xF4
#define MSG2SSP_AP_SET_MAGNETIC_HWOFFSET	0xF5
#define MSG2SSP_AP_GET_MAGNETIC_HWOFFSET	0xF6
#define MSG2SSP_AP_SENSOR_GESTURE_CURRENT	0xF7
#define MSG2SSP_AP_GET_THERM		0xF8
#define MSG2SSP_AP_GET_BIG_DATA		0xF9
#define MSG2SSP_AP_SET_BIG_DATA		0xFA
#define MSG2SSP_AP_START_BIG_DATA		0xFB
#define MSG2SSP_AP_SET_FSM_SETTING		0xFC
#define MSG2SSP_AP_SET_MAGNETIC_STATIC_MATRIX	0xFD
#define MSG2SSP_AP_SENSOR_TILT			0xEA
#define MSG2SSP_AP_MCU_SET_TIME			0xFE
#define MSG2SSP_AP_MCU_GET_TIME			0xFF
#define MSG2SSP_AP_MOBEAM_DATA_SET		0x31
#define MSG2SSP_AP_MOBEAM_REGISTER_SET		0x32
#define MSG2SSP_AP_MOBEAM_COUNT_SET		0x33
#define MSG2SSP_AP_MOBEAM_START			0x34
#define MSG2SSP_AP_MOBEAM_STOP			0x35
#define MSG2SSP_AP_GEOMAG_LOGGING		0x36
#define MSG2SSP_AP_SENSOR_LPF			0x37
#define MSG2AP_INST_TIMESTAMP_OFFSET		0xA8
#define MSG2SSP_AP_THERMISTOR_CHANNEL_0 	0xA9
#define MSG2SSP_AP_THERMISTOR_CHANNEL_1 	0xAA
#define MSG2SSP_AP_INFORMATION			0xB6

#define MSG2SSP_AP_IRDATA_SEND		0x38
#define MSG2SSP_AP_IRDATA_SEND_RESULT 0x39
#define MSG2SSP_AP_PROX_GET_TRIM	0x40
#define MSG2SSP_AP_PROX_GET_THRESHOLD	0x47
#define MSG2SSP_AP_HUB_COMMAND	0x60
#define MSG2SSP_AP_LIGHT_SEAMLESS_THD	0x62
#define MSG2SSP_AP_PROX_CAL_START	0x94
#define MSG2AP_INST_PROX_CAL_DONE	0x97

#define SH_MSG2AP_GYRO_CALIBRATION_START   0x43
#define SH_MSG2AP_GYRO_CALIBRATION_STOP	0x44
#define SH_MSG2AP_GYRO_CALIBRATION_EVENT_OCCUR  0x45

// to set mag cal data to ssp
#define MSG2SSP_AP_MAG_CAL_PARAM	 (0x46)

// for data injection
#define MSG2SSP_AP_DATA_INJECTION_MODE_ON_OFF 0x41
#define MSG2SSP_AP_DATA_INJECTION_SEND 0x42

#define MSG2SSP_AP_SET_PROX_SETTING		0x48
#define MSG2SSP_AP_SET_LIGHT_COEF		0x49
#define MSG2SSP_AP_GET_LIGHT_COEF		0x50
#define MSG2SSP_AP_SENSOR_PROX_ALERT_THRESHOLD 0x51
#define MSG2SSP_AP_GET_LIGHT_CAL		0x52
#define MSG2SSP_AP_GET_PROX_TRIM		0x53
#define MSG2SSP_AP_SET_LIGHT_CAL		0x54
#define MSG2SSP_AP_SET_LCD_TYPE			0x56
#define MSG2SSP_AP_SET_PROX_DYNAMIC_CAL		0x57
#define MSG2AP_INST_PROX_CALL_MIN		0x58
#define MSG2SSP_AP_SET_PROX_CALL_MIN		0x58
#define MSG2SSP_AP_SET_FACTORY_BINARY_FLAG	0x59
#define MSG2SSP_AP_PROX_LED_TEST_START		0x61
#define MSG2SSP_AP_PROX_LED_TEST_DONE		0x61
#define MSG2SSP_AP_SET_FCD_COVER_STATUS		0x63
#define MSG2SSP_AP_SET_FCD_AXIS_THRES		0x64
#define MSG2SSP_AP_SET_FCD_MATRIX		0x65
#define MSG2SSP_AP_GET_LIGHT_DEBUG		0x66
#define MSG2SSP_AP_REGISTER_DUMP		0x4A
#define MSG2SSP_AP_REGISTER_SETTING		  0x4B
#define MSG2SSP_AP_GET_PROXIMITY_LIGHT_DHR_SENSOR_INFO	0x4C
#define MSG2SSP_AP_MCURESET			0x77
#define MSG2SSP_AP_MCU_PC_LR			0x78

#define MSG2SSP_AP_FUSEROM			0X01

/* PANEL data */
#define MSG2SSP_UB_CONNECTED		0x67
#define MSG2SSP_MODE_INFORMATION	0x89
#define MSG2SSP_GET_DDI_COPR		0x90
#define MSG2SSP_PANEL_INFORMATION	0x91
#define MSG2SSP_GET_TEST_COPR		0x92
#define MSG2SSP_GET_LIGHT_TEST		0x93
#define MSG2SSP_GET_COPR_ROIX		0x95
#define MSG2SSP_HALL_IC_ON_OFF		0x96
#define MSG2SSP_PMS_HYSTERESYIS		0x99
#define MSG2SSP_AUTO_ROTATION_ORIENTATION        0x9A

/* voice data */
#define TYPE_WAKE_UP_VOICE_SERVICE			0x01
#define TYPE_WAKE_UP_VOICE_SOUND_SOURCE_AM		0x01
#define TYPE_WAKE_UP_VOICE_SOUND_SOURCE_GRAMMER	0x02

/* Factory Test */
#define ACCELEROMETER_FACTORY	0x80
#define GYROSCOPE_FACTORY		0x81
#define GEOMAGNETIC_FACTORY		0x82
#define PRESSURE_FACTORY		0x85
#define GESTURE_FACTORY		0x86
#define GYROSCOPE_TEMP_FACTORY	0x8A
#define GYROSCOPE_DPS_FACTORY	0x8B
#define MCU_FACTORY		0x8C
#define MCU_SLEEP_FACTORY		0x8D
#define FCD_FACTORY			0x8E

#define DEFAULT_HIGH_THRESHOLD				2400
#define DEFAULT_LOW_THRESHOLD				1600
#define DEFAULT_DETECT_HIGH_THRESHOLD		16368
#define DEFAULT_DETECT_LOW_THRESHOLD		1000

#define DEFAULT_PROX_ALERT_HIGH_THRESHOLD			1000

/* Sensors's reporting mode */
#define REPORT_MODE_CONTINUOUS	0
#define REPORT_MODE_ON_CHANGE	1
#define REPORT_MODE_SPECIAL	2
#define REPORT_MODE_UNKNOWN	3

/* Key value for Camera - Gyroscope sync */
#define CAMERA_GYROSCOPE_SYNC 7700000ULL /*7.7ms*/
#define CAMERA_GYROSCOPE_VDIS_SYNC 6600000ULL /*6.6ms*/
#define CAMERA_GYROSCOPE_SUPER_VDIS_SYNC 5500000ULL /*5.5ms*/
#define CAMERA_GYROSCOPE_ULTRA_VDIS_SYNC 4400000ULL /*4.4ms*/
#define CAMERA_GYROSCOPE_SYNC_DELAY 10000000ULL
#define CAMERA_GYROSCOPE_VDIS_SYNC_DELAY 5000000ULL
#define CAMERA_GYROSCOPE_SUPER_VDIS_SYNC_DELAY 2000000ULL
#define CAMERA_GYROSCOPE_ULTRA_VDIS_SYNC_DELAY 1000000ULL

/** HIFI Sensor **/
#define SIZE_TIMESTAMP_BUFFER	1000

/* Magnetic cal parameter size */
#define MAC_CAL_PARAM_SIZE_AKM  13
#define MAC_CAL_PARAM_SIZE_YAS  7

/* ak0911 magnetic pdc matrix size */
#define PDC_SIZE					   27

#define REGISTER_RW_DDI			89
#define REGISTER_RW_BUFFER_MAX		256

/* SSP_INSTRUCTION_CMD */
enum {
	REMOVE_SENSOR = 0,
	ADD_SENSOR,
	CHANGE_DELAY,
	GO_SLEEP,
	REMOVE_LIBRARY,
	ADD_LIBRARY,
};

struct meta_data_event {
	s32 what;
	s32 sensor;
} __attribute__((__packed__));

struct sensor_value {
	union {
		struct {
			s16 x;
			s16 y;
			s16 z;
		};
		struct {
			s32 x;
			s32 y;
			s32 z;
		} gyro;
		struct {
			s32 x;
			s32 y;
			s32 z;
			s32 offset_x;
			s32 offset_y;
			s32 offset_z;
		} uncal_gyro;
		struct {		/*calibrated mag, gyro*/
			s32 cal_x;
			s32 cal_y;
			s32 cal_z;
			u8 accuracy;
			u8 overflow;
		};
		struct {		/*uncalibrated mag, gyro*/
			s32 uncal_x;
			s32 uncal_y;
			s32 uncal_z;
			s32 offset_x;
			s32 offset_y;
			s32 offset_z;
			u8 uncaloverflow;
		};
		struct {		/* rotation vector */
			s32 quat_a;
			s32 quat_b;
			s32 quat_c;
			s32 quat_d;
			u8 acc_rot;
		};
		struct {
			u32 lux;
			s32 cct;
			u32 r;
			u32 g;
			u32 b;
			u32 w;
			u16 a_gain;
			u16 a_time;
			u8 brightness;
			u8 min_lux_flag;
		} __attribute__((__packed__)) light_t;
		struct {
			u32 lux;
			s32 cct;
			u32 r;
			u32 g;
			u32 b;
			u32 w;
			u16 a_gain;
			u16 a_time;
			u8 brightness;
			u32 lux_raw;
			u16 roi;
		} __attribute__((__packed__)) light_cct_t;

		struct {
			u8 value;
			s8 nfc_result;
			s32 diff;
			s32 magX;
			s32 stable_min_mag;
			s32 stable_max_mag;
			u16 detach_mismatch_cnt;

			s32 uncal_mag_x;
			s32 uncal_mag_y;
			s32 uncal_mag_z;
			u8 saturation;
		} __attribute__((__packed__));

		struct {
			u32 irdata;
			u32 ir_r;
			u32 ir_g;
			u32 ir_b;
			u32 ir_w;
			u16 ir_a_gain;
			u8 ir_a_time; // ir_brightness;
		};
		struct {
			s32 cap_main;
			s16 useful;
			s16 offset;
			u8 irq_stat;
		};
		struct {
			u8 prox_detect;
			u16 prox_adc;
			u32 light;
			u32 light_diff;
		} __attribute__((__packed__));
		u8 step_det;
		u8 sig_motion;
		s16 prox_raw[4];
		 struct {
			u8 prox_alert_detect;
			u16 prox_alert_adc;
		} __attribute__((__packed__));
		struct {
			s32 pressure;
			s16 temperature;
		};
		 struct {
			u8 thermistor_type;
			s16 thermistor_raw;
		} __attribute__((__packed__));
		s16 light_flicker;
		u8 data[20];
		u32 step_diff;
		u8 tilt_detector;
		u8 pickup_gesture;
		u8 call_gesture;
		u8 wakeup_move_event[2]; // wakeup motion[0] & move[1] event come sametime
		struct {
			u8 pocket_mode;
			u8 pocket_reason;
			u32 pocket_base_proxy;
			u32 pocket_current_proxy;
			u32 pocket_lux;
			u32 pocket_release_diff;
			u32 pocket_min_release;
			u32 pocket_min_recognition;
			u32 pocket_open_data;
			u32 pocket_close_data;
			u32 pocket_high_prox_data;
			u32 pocket_base_prox_time;
			u32 pocket_current_prox_time;
			u32 pocket_high_prox_time;
			u32 pocket_temp[3];
		} __attribute__((__packed__)); // pocket_mode use 62bytes
		u8 led_cover_event;
		u8 tap_tracker_event;
		u8 shake_tracker_event;
		u32 light_seamless_event;
		u8 autorotation_event;
		u8 sar_backoff_motion_event;
		u8 scontext_buf[SCONTEXT_DATA_SIZE];
		struct {
			u8 proximity_pocket_detect;
			u16 proximity_pocket_adc;
		} __attribute__((__packed__));
		struct meta_data_event meta_data;
	};
	u64 timestamp;
} __attribute__((__packed__));

extern struct class *sensors_event_class;

struct calibraion_data {
	s32 x;
	s32 y;
	s32 z;
};

struct hw_offset_data {
	char x;
	char y;
	char z;
};

/* ssp_msg options bit*/
#define SSP_SPI		0	/* read write mask */
#define SSP_RETURN	2	/* write and read option */
#define SSP_GYRO_DPS	3	/* gyro dps mask */
#define SSP_INDEX	3	/* data index mask */

#define SSP_SPI_MASK		(3 << SSP_SPI)	/* read write mask */

struct ssp_msg {
	u8 cmd;
	u16 length;
	u16 options;
	u32 data;
    u64 ap_time_ns_64;
    u64 mcu_time_ns_64;

	struct list_head list;
	struct completion *done;
	
	char *buffer;
	u8 free_buffer;
	bool *dead_hook;
	bool dead;
} __attribute__((__packed__));

enum {
	AP2HUB_READ = 0,
	AP2HUB_WRITE,
	HUB2AP_WRITE,
	AP2HUB_READY,
	AP2HUB_RETURN
};

enum {
	BIG_TYPE_DUMP = 0,
	BIG_TYPE_READ_LIB,
	BIG_TYPE_READ_HIFI_BATCH,
	BIG_TYPE_MAX,
};

enum {
	BATCH_MODE_NONE = 0,
	BATCH_MODE_RUN,
};

extern struct mutex shutdown_lock;

struct ssp_batch_event {
	char *batch_data;
	int batch_length;
};

struct ois_sensor_interface{
	void *core;
	void (*ois_func)(void *);
};

struct ssp_contexthub_dump {
	int reason;
	int size;
	char *dump;
};

struct ssp_data {
	struct iio_dev *indio_dev[SENSOR_MAX];
	struct iio_dev *indio_scontext_dev;
	struct iio_dev *indio_file_manager_dev;
	struct iio_dev *meta_indio_dev;

	struct workqueue_struct *ssp_rx_wq;
	struct work_struct work_ssp_rx;

	struct workqueue_struct *ssp_mcu_ready_wq;
	struct work_struct work_ssp_mcu_ready;

#if defined(CONFIG_SEC_VIB_NOTIFIER)
	struct workqueue_struct *ssp_motor_wq;
	struct work_struct work_ssp_motor;
#endif

	struct delayed_work fcd_notifier_work;

	struct wakeup_source *ssp_wake_lock;
	struct wakeup_source *ssp_batch_wake_lock;
	struct wakeup_source *ssp_comm_wake_lock;
	struct timer_list debug_timer;
	struct workqueue_struct *debug_wq;
	struct work_struct work_debug;

	struct timer_list timestamp_sync_timer;
	struct workqueue_struct *timestamp_sync_wq;
	struct work_struct work_timestamp_sync;

	struct calibraion_data accelcal;
	struct calibraion_data gyrocal;
	struct hw_offset_data magoffset;
	struct sensor_value buf[SENSOR_MAX];
	struct device *sen_dev;
	struct device *mcu_device;
	struct device *acc_device;
	struct device *gyro_device;
	struct device *mag_device;
	struct device *prs_device;
	struct device *prox_device;
	struct device *grip_device;
	struct device *light_device;
	struct device *ges_device;
	struct device *fcd_device;
	struct miscdevice batch_io_device;
	struct device *thermistor_device;
	
	struct ois_sensor_interface *ois_control;
	struct ois_sensor_interface *ois_reset;

	bool bFirstRef;
	bool bSspShutdown;
	bool bAccelAlert;
	bool bProximityRawEnabled;
	bool bGeomagneticRawEnabled;
	bool bGeomagneticLogged;
	bool bBarcodeEnabled;
	bool bProbeIsDone;

	int light_coef[7];

	unsigned int uProxHiThresh;
	unsigned int uProxLoThresh;
	unsigned int uProxHiThresh_detect;
	unsigned int uProxLoThresh_detect;

	unsigned int uProxAlertHiThresh;
	unsigned int uIr_Current;
	unsigned char uFuseRomData[3];
	unsigned char uMagCntlRegData;
	int aiCheckStatus[SENSOR_MAX];

	unsigned int uComFailCnt;
	unsigned int uResetCnt;
	unsigned int uTimeOutCnt;
	unsigned int uIrqCnt;
	unsigned int uDumpCnt;

	int sealevelpressure;
	unsigned int uGyroDps;
	u64 uSensorState;
	unsigned int uCurFirmRev;
	unsigned int uFactoryProxAvg[4];
	char uLastResumeState;
	char uLastAPState;
	s32 iPressureCal;
	u64 step_count_total;

	atomic64_t aSensorEnable;
	int64_t adDelayBuf[SENSOR_MAX];
	u64 lastTimestamp[SENSOR_MAX];
	u32 IsBypassMode[SENSOR_MAX]; //only use accel and light
	s32 batchLatencyBuf[SENSOR_MAX];
	s8 batchOptBuf[SENSOR_MAX];
	bool cameraGyroSyncMode;

	/** HIFI Sensor **/
	u64 ts_index_buffer[SIZE_TIMESTAMP_BUFFER];
	unsigned int ts_stacked_cnt;

	u64 lastDeltaTimeNs[SENSOR_MAX];

	struct workqueue_struct *batch_wq;
	struct mutex batch_events_lock;
	struct ssp_batch_event batch_event;

	/* HiFi batching suspend-wakeup issue */
	u64 resumeTimestamp;
	bool bIsResumed;
	int mcu_host_wake_int;
	int mcu_host_wake_irq;
	bool bIsReset;

	int (*wakeup_mcu)(void);
	int (*set_mcu_reset)(int);

	struct ssp_sensorhub_data *hub_data;
	int ap_type;
	int ap_rev;
	int accel_position;
	int mag_position;
	u8 mag_matrix_size;
	u8 *mag_matrix;
	unsigned char pdc_matrix[PDC_SIZE];
	unsigned char fcd_matrix[PDC_SIZE];

	struct mutex comm_mutex;
	struct mutex pending_mutex;
	struct mutex enable_mutex;
	struct mutex ssp_enable_mutex;

	s16 *static_matrix;
	struct list_head pending_list;

	void (*ssp_big_task[BIG_TYPE_MAX])(struct work_struct *);
	u64 timestamp;
	int light_log_cnt;
	int light_cct_log_cnt;
	int light_ir_log_cnt;

	int gyro_lib_state;

//	#ifdef CONFIG_SENSORS_DYNAMIC_SENSOR_NAME
	char sensor_name[SENSOR_MAX][256];
//	#endif
	/* variable for sensor register dump */
	char *sensor_dump[SENSOR_MAX];

	/* data for injection */
	u8 data_injection_enable;
	struct miscdevice ssp_data_injection_device;

//#if defined (CONFIG_SENSORS_SSP_VLTE)
//	struct notifier_block hall_ic_nb;
//	int change_axis;
//#endif

#if defined(CONFIG_SEC_VIB_NOTIFIER)
	int motor_state;
#endif
#if defined(CONFIG_PANEL_NOTIFY)
	struct panel_bl_event_data panel_event_data;
	struct panel_screen_mode_data  panel_mode_data;
#endif
	bool ub_disabled;

	char sensor_state[SENSOR_MAX + 1 + ((SENSOR_MAX-1) / 10)]; // \0 + blank
	unsigned int uNoRespSensorCnt;
	unsigned int errorCount;
	unsigned int pktErrCnt;
	unsigned int mcuCrashedCnt;
	bool mcuAbnormal;
	bool IsMcuCrashed;

/* variables for conditional leveling timestamp */
	bool first_sensor_data[SENSOR_MAX];
	u64 timestamp_factor;

	bool intendedMcuReset;
	char registerValue[REGISTER_RW_BUFFER_MAX + 3];
	u8 dhrAccelScaleRange;
	bool resetting;

/* variables for timestamp sync */
	struct delayed_work work_ssp_tiemstamp_sync;
	u64 timestamp_offset;

/* information of sensorhub for big data */
	char resetInfo[2048];
        char resetInfoDebug[512];
        u64 resetInfoDebugTime;
	u32 resetCntGPSisOn;
/* thermistor (up & sub) table information*/
	short tempTable_up[23];
	short tempTable_sub[23];
        short threshold_up;
        short threshold_sub;
/* when gyro selftest, check sensor should be skip*/
	bool IsGyroselftest;
/* DVIS check flag*/
	bool IsVDIS_Enabled;
/* AP suspend check flag*/
	bool IsAPsuspend;
/* no ack about mcu_resp pin*/
	bool IsNoRespCnt;
/* hall ic */
	bool hall_ic_status; // 0: open 1: close
	bool svc_octa_change; // default, false;
	int prox_call_min; // default: -1

	struct __prox_led_test {
		u8 ret;
		short adc[4];
	} __attribute__((__packed__)) prox_led_test;

	struct flip_cover_detector_data_t {
		int factory_cover_status;
		int nfc_cover_status;
		uint32_t axis_update;
		int32_t threshold_update;
		uint32_t enable;
	} fcd_data;

	const char *light_position;
	char fw_name[256];
	int fw_type;
	unsigned int curr_fw_rev;

	struct platform_device *pdev;
	u32 mcu_pc;
	u32 mcu_lr;
	int sw_offset;
	int convert_coef;
};

struct ssp_big {
	struct ssp_data *data;
	struct work_struct work;
	u32 length;
	u32 addr;
};

int ssp_iio_configure_kfifo(struct iio_dev *indio_dev, int bytes);
void ssp_iio_unconfigure_kfifo(struct iio_dev *indio_dev);
void ssp_enable(struct ssp_data *data, bool enable);
int ssp_spi_async(struct ssp_data *data, struct ssp_msg *msg);
int ssp_spi_sync(struct ssp_data *data, struct ssp_msg *msg, int timeout);
extern void clean_msg(struct ssp_msg *msg);
void clean_pending_list(struct ssp_data *data);
void toggle_mcu_reset(struct ssp_data *data);
int initialize_mcu(struct ssp_data *data);
int initialize_input_dev(struct ssp_data *data);
int initialize_sysfs(struct ssp_data *data);
void initialize_function_pointer(struct ssp_data *data);
void initialize_accel_factorytest(struct ssp_data *data);
#ifndef CONFIG_SENSORS_SSP_CM3323
void initialize_prox_factorytest(struct ssp_data *data);
#endif
void initialize_light_factorytest(struct ssp_data *data);
void initialize_gyro_factorytest(struct ssp_data *data);
void initialize_pressure_factorytest(struct ssp_data *data);
void initialize_thermistor_factorytest(struct ssp_data *data);
void initialize_magnetic_factorytest(struct ssp_data *data);
void initialize_gesture_factorytest(struct ssp_data *data);
void initialize_fcd_factorytest(struct ssp_data *data);
void remove_accel_factorytest(struct ssp_data *data);
void remove_gyro_factorytest(struct ssp_data *data);
#ifndef CONFIG_SENSORS_SSP_CM3323
void remove_prox_factorytest(struct ssp_data *data);
#endif
void remove_light_factorytest(struct ssp_data *data);
void remove_pressure_factorytest(struct ssp_data *data);
void remove_thremistor_factorytest(struct ssp_data *data);
void remove_magnetic_factorytest(struct ssp_data *data);
void remove_fcd_factorytest(struct ssp_data *data);

void sensors_remove_symlink(struct input_dev *inputdev);
void destroy_sensor_class(void);
int sensors_create_symlink(struct input_dev *inputdev);
int accel_open_calibration(struct ssp_data *data);
int gyro_open_calibration(struct ssp_data *data);
int pressure_open_calibration(struct ssp_data *data);
int proximity_open_calibration(struct ssp_data *data);
int open_pressure_sw_offset_file(struct ssp_data *data);
int load_magnetic_cal_param_from_nvm(struct ssp_data *data, u8 *cal_data, u8 length);
int set_magnetic_cal_param_to_ssp(struct ssp_data *data);
int save_magnetic_cal_param_to_nvm(struct ssp_data *data, char *pchRcvDataFrame, int *iDataIdx);

void remove_sysfs(struct ssp_data *data);
int ssp_send_cmd(struct ssp_data *data, char command, int arg);
int send_instruction(struct ssp_data *data, u8 uInst, u8 uSensorType, u8 *uSendBuf, u16 uLength);
int flush(struct ssp_data *data, u8 uSensorType);
int get_batch_count(struct ssp_data *data, u8 uSensorType);
int get_chipid(struct ssp_data *data);
int set_big_data_start(struct ssp_data *data, u8 type, u32 length);
int mag_open_hwoffset(struct ssp_data *data);
int mag_store_hwoffset(struct ssp_data *data);
int set_hw_offset(struct ssp_data *data);
int get_hw_offset(struct ssp_data *data);
int set_gyro_cal(struct ssp_data *data);
int save_gyro_caldata(struct ssp_data *data, s32 *iCalData);
int set_accel_cal(struct ssp_data *data);
int initialize_magnetic_sensor(struct ssp_data *data);
int initialize_light_sensor(struct ssp_data *data);
int initialize_thermistor_table(struct ssp_data *data);
int set_ap_information(struct ssp_data *data);
int set_flip_cover_detector_status(struct ssp_data *data);
void check_cover_detection_factory(struct ssp_data *data, struct sensor_value *flip_cover_detector_data);
int set_sensor_position(struct ssp_data *data);

int set_magnetic_static_matrix(struct ssp_data *data);
void sync_sensor_state(struct ssp_data *data);
void set_proximity_threshold(struct ssp_data *data);
void set_proximity_alert_threshold(struct ssp_data *data);
void set_proximity_barcode_enable(struct ssp_data *data, bool bEnable);
void set_gesture_current(struct ssp_data *data, unsigned char uData1);
void set_light_coef(struct ssp_data *data);
int get_msdelay(int64_t dDelayRate);
#if defined(CONFIG_SEC_VIB_NOTIFIER)
int send_motor_state(struct ssp_data *data);
#endif
u64 get_sensor_scanning_info(struct ssp_data *data);
unsigned int get_firmware_rev(struct ssp_data *data);
u8 get_accel_range(struct ssp_data *data);
int parse_dataframe(struct ssp_data *data, char *pchRcvDataFrame, int iLength);

void enable_debug_timer(struct ssp_data *data);
void disable_debug_timer(struct ssp_data *data);
int initialize_debug_timer(struct ssp_data *data);

int initialize_timestamp_sync_timer(struct ssp_data *data);

void get_proximity_threshold(struct ssp_data *data);

bool get_sensor_data(int sensor_type, char * buf, int * idx, struct sensor_value *);
void report_sensor_data(struct ssp_data *, int sensor_type, struct sensor_value *);
void report_scontext_data(struct ssp_data *data, int sensor_type, struct sensor_value *scontextbuf);

void report_mcu_ready(struct ssp_data *data);

int print_mcu_debug(char *pchRcvDataFrame, int *pDataIdx, int iRcvDataFrameLength);

void reset_mcu(struct ssp_data *data);
int sensors_register(struct device *dev, void *drvdata,
	struct device_attribute *attributes[], char *name);
int sensors_register_dcopy(struct device **pdev, void *drvdata,
	struct device_attribute *attributes[], char *name);
void sensors_unregister(struct device *dev,
	struct device_attribute *attributes[]);
ssize_t mcu_reset_show(struct device *dev, struct device_attribute *attr, char *buf);
bool mcu_reset_by_msg(struct ssp_data *data);
ssize_t mcu_revision_show(struct device *dev, struct device_attribute *attr, char *buf);
ssize_t mcu_factorytest_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t size);
ssize_t mcu_factorytest_show(struct device *dev,
	struct device_attribute *attr, char *buf);
ssize_t mcu_model_name_show(struct device *dev,
	struct device_attribute *attr, char *buf);
ssize_t mcu_sleep_factorytest_show(struct device *dev,
	struct device_attribute *attr, char *buf);
ssize_t mcu_sleep_factorytest_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size);
unsigned int ssp_check_sec_dump_mode(void);

void ssp_read_big_library_task(struct work_struct *work);
void ssp_send_big_library_task(struct work_struct *work);
void ssp_temp_task(struct work_struct *work);

int ssp_on_control(void *ssh_data, const char *str_ctrl);
int ssp_on_mcu_ready(void *ssh_data, bool ready);
int ssp_on_packet(void *ssh_data, const char *buf, size_t size);
int ssp_on_mcu_reset(void *ssh_data, bool IsNoResp);
void ssp_mcu_ready_work_func(struct work_struct *work);
int ssp_do_transfer(struct ssp_data *data, struct ssp_msg *msg,
		struct completion *done, int timeout);
#ifdef CONFIG_SEC_VIB_NOTIFIER
void ssp_motor_work_func(struct work_struct *work);
#endif
u64 get_current_timestamp(void);
void ssp_reset_batching_resources(struct ssp_data *data);

#ifdef CONFIG_PANEL_NOTIFY 
int send_panel_information(struct panel_bl_event_data *evdata);
int send_ub_connected(bool ub_connected);
#endif
int get_patch_version(int ap_type, int hw_rev);

int send_hall_ic_status(bool enable);

int send_sensor_dump_command(struct ssp_data *data, u8 sensor_type);
int send_all_sensor_dump_command(struct ssp_data *data);

void ssp_timestamp_sync_work_func(struct work_struct *work);
void set_AccelCalibrationInfoData(char *pchRcvDataFrame, int *iDataIdx);
void set_GyroCalibrationInfoData(char *pchRcvDataFrame, int *iDataIdx);
int send_vdis_flag(struct ssp_data *data, bool bFlag);
void initialize_super_vdis_setting(void);

int proximity_save_calibration(struct ssp_data *data, int *cal_data, int size);
int set_prox_cal_to_ssp(struct ssp_data *data);
int set_prox_dynamic_cal_to_ssp(struct ssp_data *data);
int set_prox_call_min_to_ssp(struct ssp_data *data);
int set_factory_binary_flag_to_ssp(struct ssp_data *data);


char get_copr_status(struct ssp_data *data);

void file_manager_cmd(struct ssp_data *data, char flag);
int file_manager_read(struct ssp_data *data, char *filename, char *buf, int size, long long pos);
int file_manager_write(struct ssp_data *data, char *filename, char *buf, int size, long long pos);

void show_calibration_file(struct ssp_data *data);

void report_meta_data(struct ssp_data *data, int sensor_type, struct sensor_value *s);

int ssp_device_probe(struct platform_device *pdev);
void ssp_device_remove(struct platform_device *pdev);
int ssp_device_suspend(struct device *dev);
void ssp_device_resume(struct device *dev);
void ssp_platform_init(void *p_data);
void ssp_handle_recv_packet(char *packet, int packet_len);
void ssp_platform_start_refrsh_task(void);
void ssp_set_firmware_name(const char *fw_name);
void ssp_dump_write_file(void);
void sensorhub_save_ram_dump(void);
void remove_shub_dump(void);
void ssp_recv_packet(struct ssp_data *data, char *packet, int packet_len);
void ssp_recv_packet_func(void);

int ssp_do_transfer(struct ssp_data *data, struct ssp_msg *msg,
		struct completion *done, int timeout);


unsigned int get_module_rev(struct ssp_data *data);

#endif
