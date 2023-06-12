/*
 *  STMicroelectronics k2hh acceleration sensor driver
 *
 *  Copyright (C) 2010 Samsung Electronics Co.Ltd
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
 */

#ifndef __K2HH_ACC_HEADER__
#define __K2HH__ACC_HEADER__

#include <linux/types.h>
#include <linux/ioctl.h>

struct k3dh_acceldata {
	__s16 x;
	__s16 y;
	__s16 z;
};

/* dev info */
#define ACC_DEV_NAME "accelerometer"

/* k2hh ioctl command label */
#define K3DH_IOCTL_BASE     0xBF
#define K3DH_IOCTL_SET_DELAY       _IOW(K3DH_IOCTL_BASE, 0x00, int64_t)
#define K3DH_IOCTL_GET_DELAY       _IOR(K3DH_IOCTL_BASE, 0x01, int64_t)
#define K3DH_IOCTL_SET_CALIBRATION       _IOW(K3DH_IOCTL_BASE, 0x02, short[3])
#define K3DH_IOCTL_GET_CALIBRATION       _IOR(K3DH_IOCTL_BASE, 0x03, short[3])
#define K3DH_IOCTL_DO_CALIBRATION       _IOR(K3DH_IOCTL_BASE, 0x04, short[3])
#define K3DH_IOCTL_READ_ACCEL_XYZ       _IOR(K3DH_IOCTL_BASE, 0x08, short[3])

/* k2hh i2c slave address */
#define KR3DH_I2C_ADDR          0x1d

/* k2hh registers */
#define TEMP_L			0x0B
#define TEMP_H			0x0C
#define WHO_AM_I		0x0F
#define ACT_THS			0x1E
#define ACT_DUR			0x1F
#define CTRL_REG1		0x20    /* power control reg */
#define CTRL_REG2		0x21    /* power control reg */
#define CTRL_REG3		0x22    /* power control reg */
#define CTRL_REG4		0x23    /* interrupt control reg */
#define CTRL_REG5		0x24    /* interrupt control reg */
#define CTRL_REG6		0x25
#define CTRL_REG7		0x26
#define STATUS			0x27
#define OUT_X_L			0x28
#define OUT_X_H			0x29
#define OUT_Y_L			0x2A
#define OUT_Y_H			0x2B
#define OUT_Z_L			0x2C
#define OUT_Z_H			0x2D
#define FIFO_CTRL		0x2E
#define FIFO_SRC		0x2F
#define IG_CFG1			0x30
#define IG_SRC1			0x31
#define IG_THS_X1		0x32
#define IG_THS_Y1		0x33
#define IG_THS_Z1		0x34
#define IG_DUR1			0x35
#define IG_CFG2			0x36
#define IG_SRC2			0x37
#define IG_THS2			0x38
#define IG_DUR2			0x39
#define XL_REFERENCE		0x3A
#define XH_REFERENCE		0x3B
#define YL_REFERENCE		0x3C
#define YH_REFERENCE		0x3D
#define ZL_REFERENCE		0x3E
#define ZH_REFERENCE		0x3F

/* CTRL_REG1 */
#define CTRL_REG1_HR		0x80
#define CTRL_REG1_ODR2		0x40
#define CTRL_REG1_ODR1		0x20
#define CTRL_REG1_ODR0		0x10
#define CTRL_REG1_BDU		0x08
#define CTRL_REG1_Zen		0x04
#define CTRL_REG1_Yen		0x02
#define CTRL_REG1_Xen		0x01

#define PM_OFF			0x00
#define LOW_PWR_MODE            0x4F /* 50HZ */
#define FASTEST_MODE            0x9F /* 1344Hz */
#define ENABLE_ALL_AXES		0x07

#define ODR10			0x10  /* 10Hz output data rate */
#define ODR50			0x20  /* 50Hz output data rate */
#define ODR100			0x30  /* 100Hz output data rate */
#define ODR200			0x40  /* 200Hz output data rate */
#define ODR400			0x50  /* 400Hz output data rate */
#define ODR800			0x60  /* 800Hz output data rate */
#define ODR_MASK		0x70

/* CTRL_REG2 */
#define CTRL_REG2_DFC1		(1 << 6)
#define CTRL_REG2_DFC0		(1 << 5)
#define CTRL_REG2_HPM1		(1 << 4)
#define CTRL_REG2_HPM0		(1 << 3)
#define CTRL_REG2_FDS		(1 << 2)
#define CTRL_REG2_HPIS2		(1 << 1)
#define CTRL_REG2_HPIS1		(1 << 0)

#define HPM_Normal		(CTRL_REG2_HPM1)
#define HPM_Filter		(CTRL_REG2_HPM0)

/* CTRL_REG3 */
#define FIFO_EN			(1 << 7)
#define STOP_FTH		(1 << 6)
#define INT1_INACT		(1 << 5)
#define INT1_IG2		(1 << 4)
#define INT1_IG1		(1 << 3)
#define INT1_OVR		(1 << 2)
#define INT1_FTH		(1 << 1)
#define INT1_DRDY		(1 << 0)

/* CTRL_REG4 */
#define CTRL_REG4_BW2		(1 << 7)
#define CTRL_REG4_BW1		(1 << 6)
#define CTRL_REG4_FS1		(1 << 5)
#define CTRL_REG4_FS0		(1 << 4)
#define CTRL_REG4_BW_SCALE_ODR	(1 << 3)
#define CTRL_REG4_IF_ADD_INC	(1 << 2)
#define CTRL_REG4_I2C_DISABLE	(1 << 1)
#define CTRL_REG4_SIM		(1 << 0)

#define FS2g			0x00
#define FS4g			(CTRL_REG4_FS1)
#define FS8g			(CTRL_REG4_FS1|CTRL_REG4_FS0)

/* CTRL_REG5 */
#define CTRL_REG5_DEBUG		(1 << 7)
#define CTRL_REG5_SOFT_RESET	(1 << 6)
#define CTRL_REG5_DEC1		(1 << 5)
#define CTRL_REG5_DEC0		(1 << 4)
#define CTRL_REG5_ST2		(1 << 3)
#define CTRL_REG5_ST1		(1 << 2)
#define CTRL_REG5_H_LACTIVE	(1 << 1)
#define CTRL_REG5_PP_OD		(1 << 0)

/* IG_CFG1 */
#define IG_CFG1_AOI		(1 << 7)
#define IG_CFG1_6D		(1 << 6)
#define IG_CFG1_ZHIE		(1 << 5)
#define IG_CFG1_ZLIE		(1 << 4)
#define IG_CFG1_YHIE		(1 << 3)
#define IG_CFG1_YLIE		(1 << 2)
#define IG_CFG1_XHIE		(1 << 1)
#define IG_CFG1_XLIE		(1 << 0)

/* IG_SRC1 */
#define IA			(1 << 6)
#define ZH			(1 << 5)
#define ZL			(1 << 4)
#define YH			(1 << 3)
#define YL			(1 << 2)
#define XH			(1 << 1)
#define XL			(1 << 0)

/* Register Auto-increase */
#define AC			(1 << 7)

#define K2HH_DEFAULT_BW_FS	0xCC

/* The default settings when sensor is on is for all 3 axis to be enabled
 * and output data rate set to 400Hz.  Output is via a ioctl read call.
 */
#define K2HH_DEFAULT_POWER_ON_SETTING (ODR400 | ENABLE_ALL_AXES | CTRL_REG1_BDU)

/* For movement recognition*/
#define USES_MOVEMENT_RECOGNITION

#if defined(USES_MOVEMENT_RECOGNITION)
#define DEFAULT_CTRL3_SETTING           0x08 /* INT enable */
#define DEFAULT_INTERRUPT_SETTING       0x0A /* INT1 XH,YH : enable */
#define DEFAULT_INTERRUPT2_SETTING      0x20 /* INT2 ZH enable */
#define DEFAULT_THRESHOLD               0x7F /* 2032mg (16*0x7F) */
#define DYNAMIC_THRESHOLD               300     /* mg */
#define DYNAMIC_THRESHOLD2              700     /* mg */
#define MOVEMENT_DURATION               0x00 /*INT1 (DURATION/odr)ms*/
enum {
        OFF = 0,
        ON = 1
};
#define ABS(a)          (((a) < 0) ? -(a) : (a))
#define MAX(a, b)       (((a) > (b)) ? (a) : (b))
#endif

#endif
