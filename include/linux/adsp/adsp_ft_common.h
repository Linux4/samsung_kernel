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
#ifndef __ADSP_FT_COMMON_H__
#define __ADSP_FT_COMMON_H__

#define PID 20000
#define NETLINK_ADSP_FAC 23
#define MAX_REG_NUM 128
/* ENUMS for Selecting the current sensor being used */
enum {
	ADSP_FACTORY_ACCEL,
	ADSP_FACTORY_GYRO,
	ADSP_FACTORY_MAG,
	ADSP_FACTORY_PRESSURE,
	ADSP_FACTORY_LIGHT,
	ADSP_FACTORY_PROX,
	ADSP_FACTORY_RGB,
	ADSP_FACTORY_MOBEAM,
	ADSP_FACTORY_MAG_READ_FUSE_ROM,
	ADSP_FACTORY_MAG_READ_REGISTERS,
	ADSP_FACTORY_PRESSURE_SEALEVEL,
	ADSP_FACTORY_GYRO_TEMP,
	ADSP_FACTORY_ACCEL_LPF_ON, /* 12 */
	ADSP_FACTORY_ACCEL_LPF_OFF, /* 13 */
#ifdef CONFIG_SLPI_MOTOR
	ADSP_FACTORY_ACCEL_MOTOR_ON, /* 14 */
	ADSP_FACTORY_ACCEL_MOTOR_OFF, /* 15 */
#endif
#ifdef CONFIG_SLPI_MAG_CALIB_RESET
	ADSP_FACTORY_MAG_CALIB_RESET, //16
#endif
	ADSP_FACTORY_SSC_CORE,
	ADSP_FACTORY_HH_HOLE,
	ADSP_FACTORY_SENSOR_MAX
};

enum {
	PROX_THRESHOLD,
#ifdef CONFIG_SUPPORT_PROX_AUTO_CAL
	PROX_HD_THRESHOLD,
#endif
	PROX_THRESHOLDR_MAX
};

enum {
	NETLINK_ATTR_SENSOR_TYPE,
	NETLINK_ATTR_MAX
};

/* Netlink ENUMS Message Protocols */
enum {
	NETLINK_MESSAGE_GET_STATUS,
	NETLINK_MESSAGE_GET_RAW_DATA,
	NETLINK_MESSAGE_RAW_DATA_RCVD,
	NETLINK_MESSAGE_STOP_RAW_DATA,
	NETLINK_MESSAGE_GET_CALIB_DATA,
	NETLINK_MESSAGE_CALIB_DATA_RCVD,
	NETLINK_MESSAGE_CALIB_STORE_DATA,
	NETLINK_MESSAGE_CALIB_STORE_RCVD,
	NETLINK_MESSAGE_SELFTEST_SHOW_DATA,
	NETLINK_MESSAGE_SELFTEST_SHOW_RCVD,
	NETLINK_MESSAGE_GYRO_TEMP,
	NETLINK_MESSAGE_MAG_READ_FUSE_ROM,
	NETLINK_MESSAGE_MAG_READ_REGISTERS,
	NETLINK_MESSAGE_PRESSURE_SEALEVEL,
	NETLINK_MESSAGE_GYRO_SELFTEST_SHOW_RCVD,
	NETLINK_MESSAGE_ACCEL_LPF_ON,
	NETLINK_MESSAGE_ACCEL_LPF_OFF,
#ifdef CONFIG_SLPI_MOTOR
	NETLINK_MESSAGE_ACCEL_MOTOR_ON,
	NETLINK_MESSAGE_ACCEL_MOTOR_OFF,
#endif
	NETLINK_MESSAGE_DUMP_REGISTER,
	NETLINK_MESSAGE_DUMP_REGISTER_RCVD,
	NETLINK_MESSAGE_READ_SI_PARAM,
	NETLINK_MESSAGE_MOBEAM_START,
	NETLINK_MESSAGE_MOBEAM_SEND_DATA,
	NETLINK_MESSAGE_MOBEAM_SEND_COUNT,
	NETLINK_MESSAGE_MOBEAM_SEND_REG,
	NETLINK_MESSAGE_MOBEAM_STOP,
	NETLINK_MESSAGE_DUMPSTATE,
	NETLINK_MESSAGE_THD_HI_DATA,
	NETLINK_MESSAGE_THD_LO_DATA,
	NETLINK_MESSAGE_THD_HI_LO_DATA_RCVD,
#ifdef CONFIG_SLPI_MAG_CALIB_RESET
	NETLINK_MESSAGE_MAG_CALIB_RESET,
#endif
#ifdef CONFIG_SUPPORT_PROX_AUTO_CAL
	NETLINK_MESSAGE_HD_THD_HI_DATA,
	NETLINK_MESSAGE_HD_THD_LO_DATA,
	NETLINK_MESSAGE_HD_THD_HI_LO_DATA_RCVD,
#endif
#ifdef CONFIG_SUPPORT_HIDDEN_HOLE
	NETLINK_MESSAGE_HIDDEN_HOLE_READ_DATA,
	NETLINK_MESSAGE_HIDDEN_HOLE_WRITE_DATA,
	NETLINK_MESSAGE_HIDDEN_HOLE_CHANGE_OWNER,
#endif
	NETLINK_MESSAGE_MAX
};

struct msg_data {
	int sensor_type;
	int param1;
	int param2;
	int param3;
};

#ifdef CONFIG_SUPPORT_HIDDEN_HOLE
struct msg_data_hidden_hole {
	int sensor_type;
	int d_factor;
	int r_coef;
	int g_coef;
	int b_coef;
	int c_coef;
	int ct_coef;
	int ct_offset;
	int th_high;
	int th_low;
	int irisprox_th;
};
#endif

struct msg_big_data {
	int sensor_type;
	int msg_size;
	union {
		char msg[256];
		short short_msg[128];
		int int_msg[64];
	};
};

struct sensor_value {
	unsigned int sensor_type;
	union{
		struct {
			short x;
			short y;
			short z;
		};
		struct {
			short r;
			short g;
			short b;
			short w;
			short a_time;
			short a_gain;
		};
#ifdef CONFIG_SUPPORT_PROX_AUTO_CAL
		struct {
			short prox;
			short offset;
		};
#else
		short prox;
#endif
		short reactive_alert;
		short temperature;
		int pressure_cal;
	};
};

struct prox_th_value {
	unsigned int th_high;
	unsigned int th_low;
	unsigned int hd_th_high;
	unsigned int hd_th_low;
};

struct sensor_stop_value {
	unsigned int sensor_type;
	int result;
};

#define NUM_CAL_DATA 9

/* Structs used in calibration show and store */
struct sensor_calib_value {
	unsigned int sensor_type;
	union {
		struct {
			int val1;
			int val2;
			int val3;
		};
		struct {
			int x;
			int y;
			int z;
		};
		struct {
			int offset;
			int threLo;
			int threHi;
			int cal_done;
			int trim;
#ifdef CONFIG_SUPPORT_PROX_AUTO_CAL
			int threDetectLo;
			int threDetectHi;
#endif
		};
		int si_mat[NUM_CAL_DATA];
	};
	int result;
};

/* mag factory value */
struct sensor_mag_factory_value {
	unsigned int sensor_type;
	struct {
		int fuserom_x;
		int fuserom_y;
		int fuserom_z;
	};
	uint8_t registers[14];
	int result;
};

struct sensor_calib_store_result {
	unsigned int sensor_type;
	int result;
};

struct sensor_accel_lpf_value {
	unsigned int sensor_type;
	int result;
	int lpf_on_off;
};

struct sensor_gyro_st_value {
	unsigned int sensor_type;
	int result1;
	int result2;
	int fifo_zro_x;
	int fifo_zro_y;
	int fifo_zro_z;
	int nost_x;
	int nost_y;
	int nost_z;
	int st_x;
	int st_y;
	int st_z;
	int st_diff_x;
	int st_diff_y;
	int st_diff_z;
};

/* Struct used for selftest */
struct sensor_selftest_show_result {
	unsigned int sensor_type;
	int result1;
	int result2;
	/* noise bias */
	int bias_x;
	int bias_y;
	int bias_z;
	/* hw selftest ratio */
	int ratio_x;
	int ratio_y;
	int ratio_z;

	/* FOR AK09911C */
	/* OFFSET */
	int offset_x;
	int offset_y;
	int offset_z;
	/* dac CNTL2 */
	int dac_ret;
	/* adc status */
	int adc_ret;
	/* OFFSET H */
	int ohx;
	int ohy;
	int ohz;
	int gyro_temp;
};

#ifdef CONFIG_SUPPORT_HIDDEN_HOLE
struct hidden_hole_data {
	int d_factor;
	int r_coef;
	int g_coef;
	int b_coef;
	int c_coef;
	int ct_coef;
	int ct_offset;
	int th_high;
	int th_low;
	int irisprox_th;
	int sum_crc;
};
#endif

struct sensor_status {
	unsigned char sensor_type;
	unsigned char status;
};

struct dump_register {
	uint8_t sensor_type;
	uint8_t reg[MAX_REG_NUM];
};
#endif
