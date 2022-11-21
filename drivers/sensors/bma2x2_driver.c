/*  Date: 2012/11/14 16:29:00
 *  Revision: 1.7
 */

/*
 * This software program is licensed subject to the GNU General Public License
 * (GPL).Version 2,June 1991, available at http://www.fsf.org/copyleft/gpl.html

 * (C) Copyright 2011 Bosch Sensortec GmbH
 * All Rights Reserved
 */


/* file BMA2X2.c
   brief This file contains all function implementations for the BMA2X2 in linux

*/

#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#include "sensors_head.h"

#include <linux/bst_sensor_common.h>

#define	VENDOR		"BOSCH"
#define	CHIP_ID		"BMA2X2"

#define SENSOR_NAME			"bma2x2"
#define ABSMIN				-512
#define ABSMAX				512
#define SLOPE_THRESHOLD_VALUE		32
#define SLOPE_DURATION_VALUE		1
#define INTERRUPT_LATCH_MODE		13
#define INTERRUPT_ENABLE		1
#define INTERRUPT_DISABLE		0
#define MAP_SLOPE_INTERRUPT		2
#define SLOPE_X_INDEX			5
#define SLOPE_Y_INDEX			6
#define SLOPE_Z_INDEX			7
#define MAX_DELAY		200
#define RANGE_SET		3  /* +/- 2G */
#define BW_SET			12 /* 125HZ  */

#define LOW_G_INTERRUPT				REL_Z
#define HIGH_G_INTERRUPT			REL_HWHEEL
#define SLOP_INTERRUPT				REL_DIAL
#define DOUBLE_TAP_INTERRUPT			REL_WHEEL
#define SINGLE_TAP_INTERRUPT			REL_MISC
#define ORIENT_INTERRUPT			ABS_PRESSURE
#define FLAT_INTERRUPT				ABS_DISTANCE
#define SLOW_NO_MOTION_INTERRUPT		REL_Y

#define HIGH_G_INTERRUPT_X_HAPPENED			1
#define HIGH_G_INTERRUPT_Y_HAPPENED			2
#define HIGH_G_INTERRUPT_Z_HAPPENED			3
#define HIGH_G_INTERRUPT_X_NEGATIVE_HAPPENED		4
#define HIGH_G_INTERRUPT_Y_NEGATIVE_HAPPENED		5
#define HIGH_G_INTERRUPT_Z_NEGATIVE_HAPPENED		6
#define SLOPE_INTERRUPT_X_HAPPENED			7
#define SLOPE_INTERRUPT_Y_HAPPENED			8
#define SLOPE_INTERRUPT_Z_HAPPENED			9
#define SLOPE_INTERRUPT_X_NEGATIVE_HAPPENED		10
#define SLOPE_INTERRUPT_Y_NEGATIVE_HAPPENED		11
#define SLOPE_INTERRUPT_Z_NEGATIVE_HAPPENED		12
#define DOUBLE_TAP_INTERRUPT_HAPPENED			13
#define SINGLE_TAP_INTERRUPT_HAPPENED			14
#define UPWARD_PORTRAIT_UP_INTERRUPT_HAPPENED		15
#define UPWARD_PORTRAIT_DOWN_INTERRUPT_HAPPENED		16
#define UPWARD_LANDSCAPE_LEFT_INTERRUPT_HAPPENED	17
#define UPWARD_LANDSCAPE_RIGHT_INTERRUPT_HAPPENED	18
#define DOWNWARD_PORTRAIT_UP_INTERRUPT_HAPPENED	19
#define DOWNWARD_PORTRAIT_DOWN_INTERRUPT_HAPPENED	20
#define DOWNWARD_LANDSCAPE_LEFT_INTERRUPT_HAPPENED	21
#define DOWNWARD_LANDSCAPE_RIGHT_INTERRUPT_HAPPENED	22
#define FLAT_INTERRUPT_TURE_HAPPENED			23
#define FLAT_INTERRUPT_FALSE_HAPPENED			24
#define LOW_G_INTERRUPT_HAPPENED			25
#define SLOW_NO_MOTION_INTERRUPT_HAPPENED		26


#define PAD_LOWG					0
#define PAD_HIGHG					1
#define PAD_SLOP					2
#define PAD_DOUBLE_TAP					3
#define PAD_SINGLE_TAP					4
#define PAD_ORIENT					5
#define PAD_FLAT					6
#define PAD_SLOW_NO_MOTION				7

#define OFFSET_0G		0x00
#define OFFSET_P1G		0x01
#define OFFSET_M1G		0x02

#define EEP_OFFSET		0x16
#define IMAGE_BASE		0x38
#define IMAGE_LEN		22


#define CHIP_ID_REG		0x00
#define VERSION_REG		0x01
#define X_AXIS_LSB_REG		0x02
#define X_AXIS_MSB_REG		0x03
#define Y_AXIS_LSB_REG		0x04
#define Y_AXIS_MSB_REG		0x05
#define Z_AXIS_LSB_REG		0x06
#define Z_AXIS_MSB_REG		0x07
#define TEMPERATURE_REG		0x08
#define STATUS1_REG		0x09
#define STATUS2_REG		0x0A
#define STATUS_TAP_SLOPE_REG	0x0B
#define STATUS_ORIENT_HIGH_REG	0x0C
#define STATUS_FIFO_REG		0x0E
#define RANGE_SEL_REG		0x0F
#define BW_SEL_REG		0x10
#define MODE_CTRL_REG		0x11
#define LOW_NOISE_CTRL_REG	0x12
#define DATA_CTRL_REG		0x13
#define RESET_REG		0x14
#define INT_ENABLE1_REG		0x16
#define INT_ENABLE2_REG		0x17
#define INT_SLO_NO_MOT_REG	0x18
#define INT1_PAD_SEL_REG	0x19
#define INT_DATA_SEL_REG	0x1A
#define INT2_PAD_SEL_REG	0x1B
#define INT_SRC_REG		0x1E
#define INT_SET_REG		0x20
#define INT_CTRL_REG		0x21
#define LOW_DURN_REG		0x22
#define LOW_THRES_REG		0x23
#define LOW_HIGH_HYST_REG	0x24
#define HIGH_DURN_REG		0x25
#define HIGH_THRES_REG		0x26
#define SLOPE_DURN_REG		0x27
#define SLOPE_THRES_REG		0x28
#define SLO_NO_MOT_THRES_REG	0x29
#define TAP_PARAM_REG		0x2A
#define TAP_THRES_REG		0x2B
#define ORIENT_PARAM_REG	0x2C
#define THETA_BLOCK_REG		0x2D
#define THETA_FLAT_REG		0x2E
#define FLAT_HOLD_TIME_REG	0x2F
#define FIFO_WML_TRIG		0x30
#define SELF_TEST_REG		0x32
#define EEPROM_CTRL_REG		0x33
#define SERIAL_CTRL_REG		0x34
#define EXTMODE_CTRL_REG	0x35
#define OFFSET_CTRL_REG		0x36
#define OFFSET_PARAMS_REG	0x37
#define OFFSET_X_AXIS_REG	0x38
#define OFFSET_Y_AXIS_REG	0x39
#define OFFSET_Z_AXIS_REG	0x3A
#define GP0_REG			0x3B
#define GP1_REG			0x3C
#define FIFO_MODE_REG		0x3E
#define FIFO_DATA_OUTPUT_REG	0x3F

#define CHIP_ID__POS		0
#define CHIP_ID__MSK		0xFF
#define CHIP_ID__LEN		8
#define CHIP_ID__REG		CHIP_ID_REG

#define VERSION__POS		0
#define VERSION__LEN		8
#define VERSION__MSK		0xFF
#define VERSION__REG		VERSION_REG

#define BMA2x2_SLO_NO_MOT_DUR__POS	2
#define BMA2x2_SLO_NO_MOT_DUR__LEN	6
#define BMA2x2_SLO_NO_MOT_DUR__MSK	0xFC
#define BMA2x2_SLO_NO_MOT_DUR__REG	SLOPE_DURN_REG

#define NEW_DATA_X__POS		0
#define NEW_DATA_X__LEN		1
#define NEW_DATA_X__MSK		0x01
#define NEW_DATA_X__REG		X_AXIS_LSB_REG

#define ACC_X14_LSB__POS		2
#define ACC_X14_LSB__LEN		6
#define ACC_X14_LSB__MSK		0xFC
#define ACC_X14_LSB__REG		X_AXIS_LSB_REG

#define ACC_X12_LSB__POS		4
#define ACC_X12_LSB__LEN		4
#define ACC_X12_LSB__MSK		0xF0
#define ACC_X12_LSB__REG		X_AXIS_LSB_REG

#define ACC_X10_LSB__POS		6
#define ACC_X10_LSB__LEN		2
#define ACC_X10_LSB__MSK		0xC0
#define ACC_X10_LSB__REG		X_AXIS_LSB_REG

#define ACC_X8_LSB__POS		0
#define ACC_X8_LSB__LEN		0
#define ACC_X8_LSB__MSK		0x00
#define ACC_X8_LSB__REG		X_AXIS_LSB_REG

#define ACC_X_MSB__POS		0
#define ACC_X_MSB__LEN		8
#define ACC_X_MSB__MSK		0xFF
#define ACC_X_MSB__REG		X_AXIS_MSB_REG

#define NEW_DATA_Y__POS          0
#define NEW_DATA_Y__LEN          1
#define NEW_DATA_Y__MSK          0x01
#define NEW_DATA_Y__REG          Y_AXIS_LSB_REG

#define ACC_Y14_LSB__POS		2
#define ACC_Y14_LSB__LEN		6
#define ACC_Y14_LSB__MSK		0xFC
#define ACC_Y14_LSB__REG		Y_AXIS_LSB_REG

#define ACC_Y12_LSB__POS		4
#define ACC_Y12_LSB__LEN		4
#define ACC_Y12_LSB__MSK		0xF0
#define ACC_Y12_LSB__REG		Y_AXIS_LSB_REG

#define ACC_Y10_LSB__POS		6
#define ACC_Y10_LSB__LEN		2
#define ACC_Y10_LSB__MSK		0xC0
#define ACC_Y10_LSB__REG		Y_AXIS_LSB_REG

#define ACC_Y8_LSB__POS		0
#define ACC_Y8_LSB__LEN		0
#define ACC_Y8_LSB__MSK		0x00
#define ACC_Y8_LSB__REG		Y_AXIS_LSB_REG

#define ACC_Y_MSB__POS		0
#define ACC_Y_MSB__LEN		8
#define ACC_Y_MSB__MSK		0xFF
#define ACC_Y_MSB__REG		Y_AXIS_MSB_REG

#define NEW_DATA_Z__POS          0
#define NEW_DATA_Z__LEN          1
#define NEW_DATA_Z__MSK          0x01
#define NEW_DATA_Z__REG          Z_AXIS_LSB_REG

#define ACC_Z14_LSB__POS		2
#define ACC_Z14_LSB__LEN		6
#define ACC_Z14_LSB__MSK		0xFC
#define ACC_Z14_LSB__REG		Z_AXIS_LSB_REG

#define ACC_Z12_LSB__POS		4
#define ACC_Z12_LSB__LEN		4
#define ACC_Z12_LSB__MSK		0xF0
#define ACC_Z12_LSB__REG		Z_AXIS_LSB_REG

#define ACC_Z10_LSB__POS		6
#define ACC_Z10_LSB__LEN		2
#define ACC_Z10_LSB__MSK		0xC0
#define ACC_Z10_LSB__REG		Z_AXIS_LSB_REG

#define ACC_Z8_LSB__POS		0
#define ACC_Z8_LSB__LEN		0
#define ACC_Z8_LSB__MSK		0x00
#define ACC_Z8_LSB__REG		Z_AXIS_LSB_REG

#define ACC_Z_MSB__POS		0
#define ACC_Z_MSB__LEN		8
#define ACC_Z_MSB__MSK		0xFF
#define ACC_Z_MSB__REG		Z_AXIS_MSB_REG

#define TEMPERATURE__POS         0
#define TEMPERATURE__LEN         8
#define TEMPERATURE__MSK         0xFF
#define TEMPERATURE__REG         TEMP_RD_REG

#define LOWG_INT_S__POS          0
#define LOWG_INT_S__LEN          1
#define LOWG_INT_S__MSK          0x01
#define LOWG_INT_S__REG          STATUS1_REG

#define HIGHG_INT_S__POS          1
#define HIGHG_INT_S__LEN          1
#define HIGHG_INT_S__MSK          0x02
#define HIGHG_INT_S__REG          STATUS1_REG

#define SLOPE_INT_S__POS          2
#define SLOPE_INT_S__LEN          1
#define SLOPE_INT_S__MSK          0x04
#define SLOPE_INT_S__REG          STATUS1_REG


#define SLO_NO_MOT_INT_S__POS          3
#define SLO_NO_MOT_INT_S__LEN          1
#define SLO_NO_MOT_INT_S__MSK          0x08
#define SLO_NO_MOT_INT_S__REG          STATUS1_REG

#define DOUBLE_TAP_INT_S__POS     4
#define DOUBLE_TAP_INT_S__LEN     1
#define DOUBLE_TAP_INT_S__MSK     0x10
#define DOUBLE_TAP_INT_S__REG     STATUS1_REG

#define SINGLE_TAP_INT_S__POS     5
#define SINGLE_TAP_INT_S__LEN     1
#define SINGLE_TAP_INT_S__MSK     0x20
#define SINGLE_TAP_INT_S__REG     STATUS1_REG

#define ORIENT_INT_S__POS         6
#define ORIENT_INT_S__LEN         1
#define ORIENT_INT_S__MSK         0x40
#define ORIENT_INT_S__REG         STATUS1_REG

#define FLAT_INT_S__POS		7
#define FLAT_INT_S__LEN		1
#define FLAT_INT_S__MSK		0x80
#define FLAT_INT_S__REG		STATUS1_REG

#define FIFO_FULL_INT_S__POS		5
#define FIFO_FULL_INT_S__LEN		1
#define FIFO_FULL_INT_S__MSK		0x20
#define FIFO_FULL_INT_S__REG		STATUS2_REG

#define FIFO_WM_INT_S__POS		6
#define FIFO_WM_INT_S__LEN		1
#define FIFO_WM_INT_S__MSK		0x40
#define FIFO_WM_INT_S__REG		STATUS2_REG

#define DATA_INT_S__POS		7
#define DATA_INT_S__LEN		1
#define DATA_INT_S__MSK		0x80
#define DATA_INT_S__REG		STATUS2_REG

#define SLOPE_FIRST_X__POS        0
#define SLOPE_FIRST_X__LEN        1
#define SLOPE_FIRST_X__MSK        0x01
#define SLOPE_FIRST_X__REG        STATUS_TAP_SLOPE_REG

#define SLOPE_FIRST_Y__POS        1
#define SLOPE_FIRST_Y__LEN        1
#define SLOPE_FIRST_Y__MSK        0x02
#define SLOPE_FIRST_Y__REG        STATUS_TAP_SLOPE_REG

#define SLOPE_FIRST_Z__POS        2
#define SLOPE_FIRST_Z__LEN        1
#define SLOPE_FIRST_Z__MSK        0x04
#define SLOPE_FIRST_Z__REG        STATUS_TAP_SLOPE_REG

#define SLOPE_SIGN_S__POS         3
#define SLOPE_SIGN_S__LEN         1
#define SLOPE_SIGN_S__MSK         0x08
#define SLOPE_SIGN_S__REG         STATUS_TAP_SLOPE_REG

#define TAP_FIRST_X__POS        4
#define TAP_FIRST_X__LEN        1
#define TAP_FIRST_X__MSK        0x10
#define TAP_FIRST_X__REG        STATUS_TAP_SLOPE_REG

#define TAP_FIRST_Y__POS        5
#define TAP_FIRST_Y__LEN        1
#define TAP_FIRST_Y__MSK        0x20
#define TAP_FIRST_Y__REG        STATUS_TAP_SLOPE_REG

#define TAP_FIRST_Z__POS        6
#define TAP_FIRST_Z__LEN        1
#define TAP_FIRST_Z__MSK        0x40
#define TAP_FIRST_Z__REG        STATUS_TAP_SLOPE_REG

#define TAP_SIGN_S__POS         7
#define TAP_SIGN_S__LEN         1
#define TAP_SIGN_S__MSK         0x80
#define TAP_SIGN_S__REG         STATUS_TAP_SLOPE_REG

#define HIGHG_FIRST_X__POS        0
#define HIGHG_FIRST_X__LEN        1
#define HIGHG_FIRST_X__MSK        0x01
#define HIGHG_FIRST_X__REG        STATUS_ORIENT_HIGH_REG

#define HIGHG_FIRST_Y__POS        1
#define HIGHG_FIRST_Y__LEN        1
#define HIGHG_FIRST_Y__MSK        0x02
#define HIGHG_FIRST_Y__REG        STATUS_ORIENT_HIGH_REG

#define HIGHG_FIRST_Z__POS        2
#define HIGHG_FIRST_Z__LEN        1
#define HIGHG_FIRST_Z__MSK        0x04
#define HIGHG_FIRST_Z__REG        STATUS_ORIENT_HIGH_REG

#define HIGHG_SIGN_S__POS         3
#define HIGHG_SIGN_S__LEN         1
#define HIGHG_SIGN_S__MSK         0x08
#define HIGHG_SIGN_S__REG         STATUS_ORIENT_HIGH_REG

#define ORIENT_S__POS		  4
#define ORIENT_S__LEN		  3
#define ORIENT_S__MSK		  0x70
#define ORIENT_S__REG		  STATUS_ORIENT_HIGH_REG

#define FLAT_S__POS		    7
#define FLAT_S__LEN		    1
#define FLAT_S__MSK		    0x80
#define FLAT_S__REG		    STATUS_ORIENT_HIGH_REG

#define FIFO_FRAME_COUNTER_S__POS		  0
#define FIFO_FRAME_COUNTER_S__LEN		  7
#define FIFO_FRAME_COUNTER_S__MSK		  0x7F
#define FIFO_FRAME_COUNTER_S__REG		  STATUS_FIFO_REG

#define FIFO_OVERRUN_S__POS		  7
#define FIFO_OVERRUN_S__LEN		  1
#define FIFO_OVERRUN_S__MSK		  0x80
#define FIFO_OVERRUN_S__REG		  STATUS_FIFO_REG

#define RANGE_SEL__POS		  0
#define RANGE_SEL__LEN		  4
#define RANGE_SEL__MSK		  0x0F
#define RANGE_SEL__REG		  RANGE_SEL_REG

#define BANDWIDTH__POS		  0
#define BANDWIDTH__LEN		  5
#define BANDWIDTH__MSK		  0x1F
#define BANDWIDTH__REG		  BW_SEL_REG

#define SLEEP_DUR__POS		  1
#define SLEEP_DUR__LEN		  4
#define SLEEP_DUR__MSK		  0x1E
#define SLEEP_DUR__REG		  MODE_CTRL_REG

#define MODE_CTRL__POS		  5
#define MODE_CTRL__LEN             3
#define MODE_CTRL__MSK             0xE0
#define MODE_CTRL__REG             MODE_CTRL_REG

#define DEEP_SUSPEND__POS          5
#define DEEP_SUSPEND__LEN          1
#define DEEP_SUSPEND__MSK          0x20
#define DEEP_SUSPEND__REG          MODE_CTRL_REG

#define EN_LOW_POWER__POS          6
#define EN_LOW_POWER__LEN          1
#define EN_LOW_POWER__MSK          0x40
#define EN_LOW_POWER__REG          MODE_CTRL_REG

#define EN_SUSPEND__POS            7
#define EN_SUSPEND__LEN            1
#define EN_SUSPEND__MSK            0x80
#define EN_SUSPEND__REG            MODE_CTRL_REG

#define SLEEP_TIMER__POS          5
#define SLEEP_TIMER__LEN          1
#define SLEEP_TIMER__MSK          0x20
#define SLEEP_TIMER__REG          LOW_NOISE_CTRL_REG

#define LOW_POWER_MODE__POS          6
#define LOW_POWER_MODE__LEN          1
#define LOW_POWER_MODE__MSK          0x40
#define LOW_POWER_MODE__REG          LOW_NOISE_CTRL_REG

#define EN_LOW_NOISE__POS          7
#define EN_LOW_NOISE__LEN          1
#define EN_LOW_NOISE__MSK          0x80
#define EN_LOW_NOISE__REG          LOW_NOISE_CTRL_REG

#define DIS_SHADOW_PROC__POS       6
#define DIS_SHADOW_PROC__LEN       1
#define DIS_SHADOW_PROC__MSK       0x40
#define DIS_SHADOW_PROC__REG       DATA_CTRL_REG

#define EN_DATA_HIGH_BW__POS         7
#define EN_DATA_HIGH_BW__LEN         1
#define EN_DATA_HIGH_BW__MSK         0x80
#define EN_DATA_HIGH_BW__REG         DATA_CTRL_REG

#define EN_SOFT_RESET__POS         0
#define EN_SOFT_RESET__LEN         8
#define EN_SOFT_RESET__MSK         0xFF
#define EN_SOFT_RESET__REG         RESET_REG

#define EN_SOFT_RESET_VALUE        0xB6

#define EN_SLOPE_X_INT__POS         0
#define EN_SLOPE_X_INT__LEN         1
#define EN_SLOPE_X_INT__MSK         0x01
#define EN_SLOPE_X_INT__REG         INT_ENABLE1_REG

#define EN_SLOPE_Y_INT__POS         1
#define EN_SLOPE_Y_INT__LEN         1
#define EN_SLOPE_Y_INT__MSK         0x02
#define EN_SLOPE_Y_INT__REG         INT_ENABLE1_REG

#define EN_SLOPE_Z_INT__POS         2
#define EN_SLOPE_Z_INT__LEN         1
#define EN_SLOPE_Z_INT__MSK         0x04
#define EN_SLOPE_Z_INT__REG         INT_ENABLE1_REG

#define EN_DOUBLE_TAP_INT__POS      4
#define EN_DOUBLE_TAP_INT__LEN      1
#define EN_DOUBLE_TAP_INT__MSK      0x10
#define EN_DOUBLE_TAP_INT__REG      INT_ENABLE1_REG

#define EN_SINGLE_TAP_INT__POS      5
#define EN_SINGLE_TAP_INT__LEN      1
#define EN_SINGLE_TAP_INT__MSK      0x20
#define EN_SINGLE_TAP_INT__REG      INT_ENABLE1_REG

#define EN_ORIENT_INT__POS          6
#define EN_ORIENT_INT__LEN          1
#define EN_ORIENT_INT__MSK          0x40
#define EN_ORIENT_INT__REG          INT_ENABLE1_REG

#define EN_FLAT_INT__POS            7
#define EN_FLAT_INT__LEN            1
#define EN_FLAT_INT__MSK            0x80
#define EN_FLAT_INT__REG            INT_ENABLE1_REG

#define EN_HIGHG_X_INT__POS         0
#define EN_HIGHG_X_INT__LEN         1
#define EN_HIGHG_X_INT__MSK         0x01
#define EN_HIGHG_X_INT__REG         INT_ENABLE2_REG

#define EN_HIGHG_Y_INT__POS         1
#define EN_HIGHG_Y_INT__LEN         1
#define EN_HIGHG_Y_INT__MSK         0x02
#define EN_HIGHG_Y_INT__REG         INT_ENABLE2_REG

#define EN_HIGHG_Z_INT__POS         2
#define EN_HIGHG_Z_INT__LEN         1
#define EN_HIGHG_Z_INT__MSK         0x04
#define EN_HIGHG_Z_INT__REG         INT_ENABLE2_REG

#define EN_LOWG_INT__POS            3
#define EN_LOWG_INT__LEN            1
#define EN_LOWG_INT__MSK            0x08
#define EN_LOWG_INT__REG            INT_ENABLE2_REG

#define EN_NEW_DATA_INT__POS        4
#define EN_NEW_DATA_INT__LEN        1
#define EN_NEW_DATA_INT__MSK        0x10
#define EN_NEW_DATA_INT__REG        INT_ENABLE2_REG

#define INT_FFULL_EN_INT__POS        5
#define INT_FFULL_EN_INT__LEN        1
#define INT_FFULL_EN_INT__MSK        0x20
#define INT_FFULL_EN_INT__REG        INT_ENABLE2_REG

#define INT_FWM_EN_INT__POS        6
#define INT_FWM_EN_INT__LEN        1
#define INT_FWM_EN_INT__MSK        0x40
#define INT_FWM_EN_INT__REG        INT_ENABLE2_REG

#define INT_SLO_NO_MOT_EN_X_INT__POS        0
#define INT_SLO_NO_MOT_EN_X_INT__LEN        1
#define INT_SLO_NO_MOT_EN_X_INT__MSK        0x01
#define INT_SLO_NO_MOT_EN_X_INT__REG        INT_SLO_NO_MOT_REG

#define INT_SLO_NO_MOT_EN_Y_INT__POS        1
#define INT_SLO_NO_MOT_EN_Y_INT__LEN        1
#define INT_SLO_NO_MOT_EN_Y_INT__MSK        0x02
#define INT_SLO_NO_MOT_EN_Y_INT__REG        INT_SLO_NO_MOT_REG

#define INT_SLO_NO_MOT_EN_Z_INT__POS        2
#define INT_SLO_NO_MOT_EN_Z_INT__LEN        1
#define INT_SLO_NO_MOT_EN_Z_INT__MSK        0x04
#define INT_SLO_NO_MOT_EN_Z_INT__REG        INT_SLO_NO_MOT_REG

#define INT_SLO_NO_MOT_EN_SEL_INT__POS        3
#define INT_SLO_NO_MOT_EN_SEL_INT__LEN        1
#define INT_SLO_NO_MOT_EN_SEL_INT__MSK        0x08
#define INT_SLO_NO_MOT_EN_SEL_INT__REG        INT_SLO_NO_MOT_REG

#define EN_INT1_PAD_LOWG__POS        0
#define EN_INT1_PAD_LOWG__LEN        1
#define EN_INT1_PAD_LOWG__MSK        0x01
#define EN_INT1_PAD_LOWG__REG        INT1_PAD_SEL_REG

#define EN_INT1_PAD_HIGHG__POS       1
#define EN_INT1_PAD_HIGHG__LEN       1
#define EN_INT1_PAD_HIGHG__MSK       0x02
#define EN_INT1_PAD_HIGHG__REG       INT1_PAD_SEL_REG

#define EN_INT1_PAD_SLOPE__POS       2
#define EN_INT1_PAD_SLOPE__LEN       1
#define EN_INT1_PAD_SLOPE__MSK       0x04
#define EN_INT1_PAD_SLOPE__REG       INT1_PAD_SEL_REG

#define EN_INT1_PAD_SLO_NO_MOT__POS        3
#define EN_INT1_PAD_SLO_NO_MOT__LEN        1
#define EN_INT1_PAD_SLO_NO_MOT__MSK        0x08
#define EN_INT1_PAD_SLO_NO_MOT__REG        INT1_PAD_SEL_REG

#define EN_INT1_PAD_DB_TAP__POS      4
#define EN_INT1_PAD_DB_TAP__LEN      1
#define EN_INT1_PAD_DB_TAP__MSK      0x10
#define EN_INT1_PAD_DB_TAP__REG      INT1_PAD_SEL_REG

#define EN_INT1_PAD_SNG_TAP__POS     5
#define EN_INT1_PAD_SNG_TAP__LEN     1
#define EN_INT1_PAD_SNG_TAP__MSK     0x20
#define EN_INT1_PAD_SNG_TAP__REG     INT1_PAD_SEL_REG

#define EN_INT1_PAD_ORIENT__POS      6
#define EN_INT1_PAD_ORIENT__LEN      1
#define EN_INT1_PAD_ORIENT__MSK      0x40
#define EN_INT1_PAD_ORIENT__REG      INT1_PAD_SEL_REG

#define EN_INT1_PAD_FLAT__POS        7
#define EN_INT1_PAD_FLAT__LEN        1
#define EN_INT1_PAD_FLAT__MSK        0x80
#define EN_INT1_PAD_FLAT__REG        INT1_PAD_SEL_REG

#define EN_INT2_PAD_LOWG__POS        0
#define EN_INT2_PAD_LOWG__LEN        1
#define EN_INT2_PAD_LOWG__MSK        0x01
#define EN_INT2_PAD_LOWG__REG        INT2_PAD_SEL_REG

#define EN_INT2_PAD_HIGHG__POS       1
#define EN_INT2_PAD_HIGHG__LEN       1
#define EN_INT2_PAD_HIGHG__MSK       0x02
#define EN_INT2_PAD_HIGHG__REG       INT2_PAD_SEL_REG

#define EN_INT2_PAD_SLOPE__POS       2
#define EN_INT2_PAD_SLOPE__LEN       1
#define EN_INT2_PAD_SLOPE__MSK       0x04
#define EN_INT2_PAD_SLOPE__REG       INT2_PAD_SEL_REG

#define EN_INT2_PAD_SLO_NO_MOT__POS        3
#define EN_INT2_PAD_SLO_NO_MOT__LEN        1
#define EN_INT2_PAD_SLO_NO_MOT__MSK        0x08
#define EN_INT2_PAD_SLO_NO_MOT__REG        INT2_PAD_SEL_REG

#define EN_INT2_PAD_DB_TAP__POS      4
#define EN_INT2_PAD_DB_TAP__LEN      1
#define EN_INT2_PAD_DB_TAP__MSK      0x10
#define EN_INT2_PAD_DB_TAP__REG      INT2_PAD_SEL_REG

#define EN_INT2_PAD_SNG_TAP__POS     5
#define EN_INT2_PAD_SNG_TAP__LEN     1
#define EN_INT2_PAD_SNG_TAP__MSK     0x20
#define EN_INT2_PAD_SNG_TAP__REG     INT2_PAD_SEL_REG

#define EN_INT2_PAD_ORIENT__POS      6
#define EN_INT2_PAD_ORIENT__LEN      1
#define EN_INT2_PAD_ORIENT__MSK      0x40
#define EN_INT2_PAD_ORIENT__REG      INT2_PAD_SEL_REG

#define EN_INT2_PAD_FLAT__POS        7
#define EN_INT2_PAD_FLAT__LEN        1
#define EN_INT2_PAD_FLAT__MSK        0x80
#define EN_INT2_PAD_FLAT__REG        INT2_PAD_SEL_REG

#define EN_INT1_PAD_NEWDATA__POS     0
#define EN_INT1_PAD_NEWDATA__LEN     1
#define EN_INT1_PAD_NEWDATA__MSK     0x01
#define EN_INT1_PAD_NEWDATA__REG     INT_DATA_SEL_REG

#define EN_INT1_PAD_FWM__POS     1
#define EN_INT1_PAD_FWM__LEN     1
#define EN_INT1_PAD_FWM__MSK     0x02
#define EN_INT1_PAD_FWM__REG     INT_DATA_SEL_REG

#define EN_INT1_PAD_FFULL__POS     2
#define EN_INT1_PAD_FFULL__LEN     1
#define EN_INT1_PAD_FFULL__MSK     0x04
#define EN_INT1_PAD_FFULL__REG     INT_DATA_SEL_REG

#define EN_INT2_PAD_FFULL__POS     5
#define EN_INT2_PAD_FFULL__LEN     1
#define EN_INT2_PAD_FFULL__MSK     0x20
#define EN_INT2_PAD_FFULL__REG     INT_DATA_SEL_REG

#define EN_INT2_PAD_FWM__POS     6
#define EN_INT2_PAD_FWM__LEN     1
#define EN_INT2_PAD_FWM__MSK     0x40
#define EN_INT2_PAD_FWM__REG     INT_DATA_SEL_REG

#define EN_INT2_PAD_NEWDATA__POS     7
#define EN_INT2_PAD_NEWDATA__LEN     1
#define EN_INT2_PAD_NEWDATA__MSK     0x80
#define EN_INT2_PAD_NEWDATA__REG     INT_DATA_SEL_REG

#define UNFILT_INT_SRC_LOWG__POS        0
#define UNFILT_INT_SRC_LOWG__LEN        1
#define UNFILT_INT_SRC_LOWG__MSK        0x01
#define UNFILT_INT_SRC_LOWG__REG        INT_SRC_REG

#define UNFILT_INT_SRC_HIGHG__POS       1
#define UNFILT_INT_SRC_HIGHG__LEN       1
#define UNFILT_INT_SRC_HIGHG__MSK       0x02
#define UNFILT_INT_SRC_HIGHG__REG       INT_SRC_REG

#define UNFILT_INT_SRC_SLOPE__POS       2
#define UNFILT_INT_SRC_SLOPE__LEN       1
#define UNFILT_INT_SRC_SLOPE__MSK       0x04
#define UNFILT_INT_SRC_SLOPE__REG       INT_SRC_REG

#define UNFILT_INT_SRC_SLO_NO_MOT__POS        3
#define UNFILT_INT_SRC_SLO_NO_MOT__LEN        1
#define UNFILT_INT_SRC_SLO_NO_MOT__MSK        0x08
#define UNFILT_INT_SRC_SLO_NO_MOT__REG        INT_SRC_REG

#define UNFILT_INT_SRC_TAP__POS         4
#define UNFILT_INT_SRC_TAP__LEN         1
#define UNFILT_INT_SRC_TAP__MSK         0x10
#define UNFILT_INT_SRC_TAP__REG         INT_SRC_REG

#define UNFILT_INT_SRC_DATA__POS        5
#define UNFILT_INT_SRC_DATA__LEN        1
#define UNFILT_INT_SRC_DATA__MSK        0x20
#define UNFILT_INT_SRC_DATA__REG        INT_SRC_REG

#define INT1_PAD_ACTIVE_LEVEL__POS       0
#define INT1_PAD_ACTIVE_LEVEL__LEN       1
#define INT1_PAD_ACTIVE_LEVEL__MSK       0x01
#define INT1_PAD_ACTIVE_LEVEL__REG       INT_SET_REG

#define INT2_PAD_ACTIVE_LEVEL__POS       2
#define INT2_PAD_ACTIVE_LEVEL__LEN       1
#define INT2_PAD_ACTIVE_LEVEL__MSK       0x04
#define INT2_PAD_ACTIVE_LEVEL__REG       INT_SET_REG

#define INT1_PAD_OUTPUT_TYPE__POS        1
#define INT1_PAD_OUTPUT_TYPE__LEN        1
#define INT1_PAD_OUTPUT_TYPE__MSK        0x02
#define INT1_PAD_OUTPUT_TYPE__REG        INT_SET_REG

#define INT2_PAD_OUTPUT_TYPE__POS        3
#define INT2_PAD_OUTPUT_TYPE__LEN        1
#define INT2_PAD_OUTPUT_TYPE__MSK        0x08
#define INT2_PAD_OUTPUT_TYPE__REG        INT_SET_REG

#define INT_MODE_SEL__POS                0
#define INT_MODE_SEL__LEN                4
#define INT_MODE_SEL__MSK                0x0F
#define INT_MODE_SEL__REG                INT_CTRL_REG

#define RESET_INT__POS           7
#define RESET_INT__LEN           1
#define RESET_INT__MSK           0x80
#define RESET_INT__REG           INT_CTRL_REG

#define LOWG_DUR__POS                    0
#define LOWG_DUR__LEN                    8
#define LOWG_DUR__MSK                    0xFF
#define LOWG_DUR__REG                    LOW_DURN_REG

#define LOWG_THRES__POS                  0
#define LOWG_THRES__LEN                  8
#define LOWG_THRES__MSK                  0xFF
#define LOWG_THRES__REG                  LOW_THRES_REG

#define LOWG_HYST__POS                   0
#define LOWG_HYST__LEN                   2
#define LOWG_HYST__MSK                   0x03
#define LOWG_HYST__REG                   LOW_HIGH_HYST_REG

#define LOWG_INT_MODE__POS               2
#define LOWG_INT_MODE__LEN               1
#define LOWG_INT_MODE__MSK               0x04
#define LOWG_INT_MODE__REG               LOW_HIGH_HYST_REG

#define HIGHG_DUR__POS                    0
#define HIGHG_DUR__LEN                    8
#define HIGHG_DUR__MSK                    0xFF
#define HIGHG_DUR__REG                    HIGH_DURN_REG

#define HIGHG_THRES__POS                  0
#define HIGHG_THRES__LEN                  8
#define HIGHG_THRES__MSK                  0xFF
#define HIGHG_THRES__REG                  HIGH_THRES_REG

#define HIGHG_HYST__POS                  6
#define HIGHG_HYST__LEN                  2
#define HIGHG_HYST__MSK                  0xC0
#define HIGHG_HYST__REG                  LOW_HIGH_HYST_REG

#define SLOPE_DUR__POS                    0
#define SLOPE_DUR__LEN                    2
#define SLOPE_DUR__MSK                    0x03
#define SLOPE_DUR__REG                    SLOPE_DURN_REG

#define SLO_NO_MOT_DUR__POS                    2
#define SLO_NO_MOT_DUR__LEN                    6
#define SLO_NO_MOT_DUR__MSK                    0xFC
#define SLO_NO_MOT_DUR__REG                    SLOPE_DURN_REG

#define SLOPE_THRES__POS                  0
#define SLOPE_THRES__LEN                  8
#define SLOPE_THRES__MSK                  0xFF
#define SLOPE_THRES__REG                  SLOPE_THRES_REG

#define SLO_NO_MOT_THRES__POS                  0
#define SLO_NO_MOT_THRES__LEN                  8
#define SLO_NO_MOT_THRES__MSK                  0xFF
#define SLO_NO_MOT_THRES__REG		SLO_NO_MOT_THRES_REG

#define TAP_DUR__POS                    0
#define TAP_DUR__LEN                    3
#define TAP_DUR__MSK                    0x07
#define TAP_DUR__REG                    TAP_PARAM_REG

#define TAP_SHOCK_DURN__POS             6
#define TAP_SHOCK_DURN__LEN             1
#define TAP_SHOCK_DURN__MSK             0x40
#define TAP_SHOCK_DURN__REG             TAP_PARAM_REG

#define ADV_TAP_INT__POS                5
#define ADV_TAP_INT__LEN                1
#define ADV_TAP_INT__MSK                0x20
#define ADV_TAP_INT__REG                TAP_PARAM_REG

#define TAP_QUIET_DURN__POS             7
#define TAP_QUIET_DURN__LEN             1
#define TAP_QUIET_DURN__MSK             0x80
#define TAP_QUIET_DURN__REG             TAP_PARAM_REG

#define TAP_THRES__POS                  0
#define TAP_THRES__LEN                  5
#define TAP_THRES__MSK                  0x1F
#define TAP_THRES__REG                  TAP_THRES_REG

#define TAP_SAMPLES__POS                6
#define TAP_SAMPLES__LEN                2
#define TAP_SAMPLES__MSK                0xC0
#define TAP_SAMPLES__REG                TAP_THRES_REG

#define ORIENT_MODE__POS                  0
#define ORIENT_MODE__LEN                  2
#define ORIENT_MODE__MSK                  0x03
#define ORIENT_MODE__REG                  ORIENT_PARAM_REG

#define ORIENT_BLOCK__POS                 2
#define ORIENT_BLOCK__LEN                 2
#define ORIENT_BLOCK__MSK                 0x0C
#define ORIENT_BLOCK__REG                 ORIENT_PARAM_REG

#define ORIENT_HYST__POS                  4
#define ORIENT_HYST__LEN                  3
#define ORIENT_HYST__MSK                  0x70
#define ORIENT_HYST__REG                  ORIENT_PARAM_REG

#define ORIENT_AXIS__POS                  7
#define ORIENT_AXIS__LEN                  1
#define ORIENT_AXIS__MSK                  0x80
#define ORIENT_AXIS__REG                  THETA_BLOCK_REG

#define ORIENT_UD_EN__POS                  6
#define ORIENT_UD_EN__LEN                  1
#define ORIENT_UD_EN__MSK                  0x40
#define ORIENT_UD_EN__REG                  THETA_BLOCK_REG

#define THETA_BLOCK__POS                  0
#define THETA_BLOCK__LEN                  6
#define THETA_BLOCK__MSK                  0x3F
#define THETA_BLOCK__REG                  THETA_BLOCK_REG

#define THETA_FLAT__POS                  0
#define THETA_FLAT__LEN                  6
#define THETA_FLAT__MSK                  0x3F
#define THETA_FLAT__REG                  THETA_FLAT_REG

#define FLAT_HOLD_TIME__POS              4
#define FLAT_HOLD_TIME__LEN              2
#define FLAT_HOLD_TIME__MSK              0x30
#define FLAT_HOLD_TIME__REG              FLAT_HOLD_TIME_REG

#define FLAT_HYS__POS                   0
#define FLAT_HYS__LEN                   3
#define FLAT_HYS__MSK                   0x07
#define FLAT_HYS__REG                   FLAT_HOLD_TIME_REG

#define FIFO_WML_TRIG_RETAIN__POS                   0
#define FIFO_WML_TRIG_RETAIN__LEN                   6
#define FIFO_WML_TRIG_RETAIN__MSK                   0x3F
#define FIFO_WML_TRIG_RETAIN__REG                   FIFO_WML_TRIG

#define EN_SELF_TEST__POS                0
#define EN_SELF_TEST__LEN                2
#define EN_SELF_TEST__MSK                0x03
#define EN_SELF_TEST__REG                SELF_TEST_REG

#define NEG_SELF_TEST__POS               2
#define NEG_SELF_TEST__LEN               1
#define NEG_SELF_TEST__MSK               0x04
#define NEG_SELF_TEST__REG               SELF_TEST_REG

#define SELF_TEST_AMP__POS               4
#define SELF_TEST_AMP__LEN               1
#define SELF_TEST_AMP__MSK               0x10
#define SELF_TEST_AMP__REG               SELF_TEST_REG

#define UNLOCK_EE_PROG_MODE__POS     0
#define UNLOCK_EE_PROG_MODE__LEN     1
#define UNLOCK_EE_PROG_MODE__MSK     0x01
#define UNLOCK_EE_PROG_MODE__REG     EEPROM_CTRL_REG

#define START_EE_PROG_TRIG__POS      1
#define START_EE_PROG_TRIG__LEN      1
#define START_EE_PROG_TRIG__MSK      0x02
#define START_EE_PROG_TRIG__REG      EEPROM_CTRL_REG

#define EE_PROG_READY__POS          2
#define EE_PROG_READY__LEN          1
#define EE_PROG_READY__MSK          0x04
#define EE_PROG_READY__REG          EEPROM_CTRL_REG

#define UPDATE_IMAGE__POS                3
#define UPDATE_IMAGE__LEN                1
#define UPDATE_IMAGE__MSK                0x08
#define UPDATE_IMAGE__REG                EEPROM_CTRL_REG

#define EE_REMAIN__POS                4
#define EE_REMAIN__LEN                4
#define EE_REMAIN__MSK                0xF0
#define EE_REMAIN__REG                EEPROM_CTRL_REG

#define EN_SPI_MODE_3__POS              0
#define EN_SPI_MODE_3__LEN              1
#define EN_SPI_MODE_3__MSK              0x01
#define EN_SPI_MODE_3__REG              SERIAL_CTRL_REG

#define I2C_WATCHDOG_PERIOD__POS        1
#define I2C_WATCHDOG_PERIOD__LEN        1
#define I2C_WATCHDOG_PERIOD__MSK        0x02
#define I2C_WATCHDOG_PERIOD__REG        SERIAL_CTRL_REG

#define EN_I2C_WATCHDOG__POS            2
#define EN_I2C_WATCHDOG__LEN            1
#define EN_I2C_WATCHDOG__MSK            0x04
#define EN_I2C_WATCHDOG__REG            SERIAL_CTRL_REG

#define EXT_MODE__POS              7
#define EXT_MODE__LEN              1
#define EXT_MODE__MSK              0x80
#define EXT_MODE__REG              EXTMODE_CTRL_REG

#define ALLOW_UPPER__POS        6
#define ALLOW_UPPER__LEN        1
#define ALLOW_UPPER__MSK        0x40
#define ALLOW_UPPER__REG        EXTMODE_CTRL_REG

#define MAP_2_LOWER__POS            5
#define MAP_2_LOWER__LEN            1
#define MAP_2_LOWER__MSK            0x20
#define MAP_2_LOWER__REG            EXTMODE_CTRL_REG

#define MAGIC_NUMBER__POS            0
#define MAGIC_NUMBER__LEN            5
#define MAGIC_NUMBER__MSK            0x1F
#define MAGIC_NUMBER__REG            EXTMODE_CTRL_REG

#define UNLOCK_EE_WRITE_TRIM__POS        4
#define UNLOCK_EE_WRITE_TRIM__LEN        4
#define UNLOCK_EE_WRITE_TRIM__MSK        0xF0
#define UNLOCK_EE_WRITE_TRIM__REG        CTRL_UNLOCK_REG

#define EN_SLOW_COMP_X__POS              0
#define EN_SLOW_COMP_X__LEN              1
#define EN_SLOW_COMP_X__MSK              0x01
#define EN_SLOW_COMP_X__REG              OFFSET_CTRL_REG

#define EN_SLOW_COMP_Y__POS              1
#define EN_SLOW_COMP_Y__LEN              1
#define EN_SLOW_COMP_Y__MSK              0x02
#define EN_SLOW_COMP_Y__REG              OFFSET_CTRL_REG

#define EN_SLOW_COMP_Z__POS              2
#define EN_SLOW_COMP_Z__LEN              1
#define EN_SLOW_COMP_Z__MSK              0x04
#define EN_SLOW_COMP_Z__REG              OFFSET_CTRL_REG

#define FAST_CAL_RDY_S__POS             4
#define FAST_CAL_RDY_S__LEN             1
#define FAST_CAL_RDY_S__MSK             0x10
#define FAST_CAL_RDY_S__REG             OFFSET_CTRL_REG

#define CAL_TRIGGER__POS                5
#define CAL_TRIGGER__LEN                2
#define CAL_TRIGGER__MSK                0x60
#define CAL_TRIGGER__REG                OFFSET_CTRL_REG

#define RESET_OFFSET_REGS__POS           7
#define RESET_OFFSET_REGS__LEN           1
#define RESET_OFFSET_REGS__MSK           0x80
#define RESET_OFFSET_REGS__REG           OFFSET_CTRL_REG

#define COMP_CUTOFF__POS                 0
#define COMP_CUTOFF__LEN                 1
#define COMP_CUTOFF__MSK                 0x01
#define COMP_CUTOFF__REG                 OFFSET_PARAMS_REG

#define COMP_TARGET_OFFSET_X__POS        1
#define COMP_TARGET_OFFSET_X__LEN        2
#define COMP_TARGET_OFFSET_X__MSK        0x06
#define COMP_TARGET_OFFSET_X__REG        OFFSET_PARAMS_REG

#define COMP_TARGET_OFFSET_Y__POS        3
#define COMP_TARGET_OFFSET_Y__LEN        2
#define COMP_TARGET_OFFSET_Y__MSK        0x18
#define COMP_TARGET_OFFSET_Y__REG        OFFSET_PARAMS_REG

#define COMP_TARGET_OFFSET_Z__POS        5
#define COMP_TARGET_OFFSET_Z__LEN        2
#define COMP_TARGET_OFFSET_Z__MSK        0x60
#define COMP_TARGET_OFFSET_Z__REG        OFFSET_PARAMS_REG

#define FIFO_DATA_SELECT__POS                 0
#define FIFO_DATA_SELECT__LEN                 2
#define FIFO_DATA_SELECT__MSK                 0x03
#define FIFO_DATA_SELECT__REG                 FIFO_MODE_REG

#define FIFO_TRIGGER_SOURCE__POS                 2
#define FIFO_TRIGGER_SOURCE__LEN                 2
#define FIFO_TRIGGER_SOURCE__MSK                 0x0C
#define FIFO_TRIGGER_SOURCE__REG                 FIFO_MODE_REG

#define FIFO_TRIGGER_ACTION__POS                 4
#define FIFO_TRIGGER_ACTION__LEN                 2
#define FIFO_TRIGGER_ACTION__MSK                 0x30
#define FIFO_TRIGGER_ACTION__REG                 FIFO_MODE_REG

#define FIFO_MODE__POS                 6
#define FIFO_MODE__LEN                 2
#define FIFO_MODE__MSK                 0xC0
#define FIFO_MODE__REG                 FIFO_MODE_REG

#define STATUS1                             0
#define STATUS2                             1
#define STATUS3                             2
#define STATUS4                             3
#define STATUS5                             4

#define RANGE_2G                 3
#define RANGE_4G                 5
#define RANGE_8G                 8
#define RANGE_16G                12

#define BW_7_81HZ        0x08
#define BW_15_63HZ       0x09
#define BW_31_25HZ       0x0A
#define BW_62_50HZ       0x0B
#define BW_125HZ         0x0C
#define BW_250HZ         0x0D
#define BW_500HZ         0x0E
#define BW_1000HZ        0x0F

#define SLEEP_DUR_0_5MS        0x05
#define SLEEP_DUR_1MS          0x06
#define SLEEP_DUR_2MS          0x07
#define SLEEP_DUR_4MS          0x08
#define SLEEP_DUR_6MS          0x09
#define SLEEP_DUR_10MS         0x0A
#define SLEEP_DUR_25MS         0x0B
#define SLEEP_DUR_50MS         0x0C
#define SLEEP_DUR_100MS        0x0D
#define SLEEP_DUR_500MS        0x0E
#define SLEEP_DUR_1S           0x0F

#define LATCH_DUR_NON_LATCH    0x00
#define LATCH_DUR_250MS        0x01
#define LATCH_DUR_500MS        0x02
#define LATCH_DUR_1S           0x03
#define LATCH_DUR_2S           0x04
#define LATCH_DUR_4S           0x05
#define LATCH_DUR_8S           0x06
#define LATCH_DUR_LATCH        0x07
#define LATCH_DUR_NON_LATCH1   0x08
#define LATCH_DUR_250US        0x09
#define LATCH_DUR_500US        0x0A
#define LATCH_DUR_1MS          0x0B
#define LATCH_DUR_12_5MS       0x0C
#define LATCH_DUR_25MS         0x0D
#define LATCH_DUR_50MS         0x0E
#define LATCH_DUR_LATCH1       0x0F

#define MODE_NORMAL             0
#define MODE_LOWPOWER1          1
#define MODE_SUSPEND            2
#define MODE_DEEP_SUSPEND       3
#define MODE_LOWPOWER2          4
#define MODE_STANDBY            5

#define X_AXIS           0
#define Y_AXIS           1
#define Z_AXIS           2

#define Low_G_Interrupt       0
#define High_G_X_Interrupt    1
#define High_G_Y_Interrupt    2
#define High_G_Z_Interrupt    3
#define DATA_EN               4
#define Slope_X_Interrupt     5
#define Slope_Y_Interrupt     6
#define Slope_Z_Interrupt     7
#define Single_Tap_Interrupt  8
#define Double_Tap_Interrupt  9
#define Orient_Interrupt      10
#define Flat_Interrupt        11
#define FFULL_INTERRUPT       12
#define FWM_INTERRUPT         13

#define INT1_LOWG         0
#define INT2_LOWG         1
#define INT1_HIGHG        0
#define INT2_HIGHG        1
#define INT1_SLOPE        0
#define INT2_SLOPE        1
#define INT1_SLO_NO_MOT   0
#define INT2_SLO_NO_MOT   1
#define INT1_DTAP         0
#define INT2_DTAP         1
#define INT1_STAP         0
#define INT2_STAP         1
#define INT1_ORIENT       0
#define INT2_ORIENT       1
#define INT1_FLAT         0
#define INT2_FLAT         1
#define INT1_NDATA        0
#define INT2_NDATA        1
#define INT1_FWM          0
#define INT2_FWM          1
#define INT1_FFULL        0
#define INT2_FFULL        1

#define SRC_LOWG         0
#define SRC_HIGHG        1
#define SRC_SLOPE        2
#define SRC_SLO_NO_MOT   3
#define SRC_TAP          4
#define SRC_DATA         5

#define INT1_OUTPUT      0
#define INT2_OUTPUT      1
#define INT1_LEVEL       0
#define INT2_LEVEL       1

#define LOW_DURATION            0
#define HIGH_DURATION           1
#define SLOPE_DURATION          2
#define SLO_NO_MOT_DURATION     3

#define LOW_THRESHOLD            0
#define HIGH_THRESHOLD           1
#define SLOPE_THRESHOLD          2
#define SLO_NO_MOT_THRESHOLD     3

#define LOWG_HYST                0
#define HIGHG_HYST               1

#define ORIENT_THETA             0
#define FLAT_THETA               1

#define I2C_SELECT               0
#define I2C_EN                   1

#define SLOW_COMP_X              0
#define SLOW_COMP_Y              1
#define SLOW_COMP_Z              2

#define CUT_OFF                  0
#define OFFSET_TRIGGER_X         1
#define OFFSET_TRIGGER_Y         2
#define OFFSET_TRIGGER_Z         3

#define GP0                      0
#define GP1                      1

#define SLO_NO_MOT_EN_X          0
#define SLO_NO_MOT_EN_Y          1
#define SLO_NO_MOT_EN_Z          2
#define SLO_NO_MOT_EN_SEL        3

#define WAKE_UP_DUR_20MS         0
#define WAKE_UP_DUR_80MS         1
#define WAKE_UP_DUR_320MS                2
#define WAKE_UP_DUR_2560MS               3

#define SELF_TEST0_ON            1
#define SELF_TEST1_ON            2

#define EE_W_OFF                 0
#define EE_W_ON                  1

#define LOW_TH_IN_G(gthres, range)           ((256 * gthres) / range)
#define HIGH_TH_IN_G(gthres, range)          ((256 * gthres) / range)
#define LOW_HY_IN_G(ghyst, range)            ((32 * ghyst) / range)
#define HIGH_HY_IN_G(ghyst, range)           ((32 * ghyst) / range)
#define SLOPE_TH_IN_G(gthres, range)    ((128 * gthres) / range)

#define GET_BITSLICE(regvar, bitname)\
	((regvar & bitname##__MSK) >> bitname##__POS)
#define SET_BITSLICE(regvar, bitname, val)\
	((regvar & ~bitname##__MSK) | ((val<<bitname##__POS)&bitname##__MSK))

#define BMA255_CHIP_ID 0XFA
#define BMA250E_CHIP_ID 0XF9
#define BMA222E_CHIP_ID 0XF8
#define BMA280_CHIP_ID 0XFB

#define BMA255_TYPE 0
#define BMA250E_TYPE 1
#define BMA222E_TYPE 2
#define BMA280_TYPE 3

#define MAX_FIFO_F_LEVEL 32
#define MAX_FIFO_F_BYTES 6
#define BMA_MAX_RETRY_I2C_XFER (100)

#define CALIBRATION_FILE_PATH	"/efs/calibration_data"

unsigned char *sensor_name[] = { "BMA255", "BMA250E", "BMA222E", "BMA280" };

struct bma2x2acc {
	s16	x,
		y,
		z;
} ;

struct bma2x2_data {
	struct i2c_client *bma2x2_client;
	atomic_t delay;
	atomic_t enable;
	atomic_t selftest_result;
	unsigned int chip_id;
	unsigned char mode;
	signed char sensor_type;
	struct input_dev *input;
	struct bma2x2acc value;
	struct mutex value_mutex;
	struct mutex enable_mutex;
	struct mutex mode_mutex;
	struct delayed_work work;
	struct work_struct irq_work;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
	int IRQ;

#ifdef BMA_USE_PLATFORM_DATA
	struct bosch_sensor_specific *bst_pd;
#endif
};

static struct device *bma2x2_device;

#ifdef CONFIG_HAS_EARLYSUSPEND
static void bma2x2_early_suspend(struct early_suspend *h);
static void bma2x2_late_resume(struct early_suspend *h);
#endif
static int bma2x2_open_cal(struct i2c_client *client);

static void bma2x2_remap_sensor_data(struct bma2x2acc *val,
		struct bma2x2_data *client_data)
{
#ifdef BMA_USE_PLATFORM_DATA
	struct bosch_sensor_data bsd;

	if (NULL == client_data->bst_pd)
		return;

	if (BOSCH_SENSOR_PLACE_UNKNOWN == client_data->bst_pd->place) {
		pr_info("%s : BOSCH_SENSOR_PLACE_UNKNOWN\n", __func__);
		return;
	}

	bsd.x = val->x;
	bsd.y = val->y;
	bsd.z = val->z;

	bst_remap_sensor_data_dft_tab(&bsd,
			client_data->bst_pd->place);

	val->x = bsd.x;
	val->y = bsd.y;
	val->z = bsd.z;
#else
	(void)val;
	(void)client_data;
#endif
}

static int bma2x2_smbus_read_byte(struct i2c_client *client,
		unsigned char reg_addr, unsigned char *data)
{
	s32 dummy;
	dummy = i2c_smbus_read_byte_data(client, reg_addr);
	if (dummy < 0)
		return -1;
	*data = dummy & 0x000000ff;

	return 0;
}

static int bma2x2_smbus_write_byte(struct i2c_client *client,
		unsigned char reg_addr, unsigned char *data)
{
	s32 dummy;

	dummy = i2c_smbus_write_byte_data(client, reg_addr, *data);
	if (dummy < 0)
		return -1;
	return 0;
}

static int bma2x2_smbus_read_byte_block(struct i2c_client *client,
		unsigned char reg_addr, unsigned char *data, unsigned char len)
{
	s32 dummy;
	dummy = i2c_smbus_read_i2c_block_data(client, reg_addr, len, data);
	if (dummy < 0)
		return -1;
	return 0;
}

static int bma_i2c_burst_read(struct i2c_client *client, u8 reg_addr,
		u8 *data, u16 len)
{
	int retry;

	struct i2c_msg msg[] = {
		{
		 .addr = client->addr,
		 .flags = 0,
		 .len = 1,
		 .buf = &reg_addr,
		},

		{
		 .addr = client->addr,
		 .flags = I2C_M_RD,
		 .len = len,
		 .buf = data,
		 },
	};

	for (retry = 0; retry < BMA_MAX_RETRY_I2C_XFER; retry++) {
		if (i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg)) > 0)
			break;
		else
			mdelay(1);
	}

	if (BMA_MAX_RETRY_I2C_XFER <= retry) {
		pr_info("I2C xfer error");
		return -EIO;
	}

	return 0;
}

#ifdef ENABLE_INT1
static int bma2x2_set_int1_pad_sel(struct i2c_client *client, unsigned char
		int1_sel)
{
	int comres;
	unsigned char data;
	unsigned char state;
	state = 0x01;

	switch (int1_sel) {
	case 0:
		comres = bma2x2_smbus_read_byte(client,
				EN_INT1_PAD_LOWG__REG, &data);
		data = SET_BITSLICE(data, EN_INT1_PAD_LOWG,
				state);
		comres |= bma2x2_smbus_write_byte(client,
				EN_INT1_PAD_LOWG__REG, &data);
		break;
	case 1:
		comres = bma2x2_smbus_read_byte(client,
				EN_INT1_PAD_HIGHG__REG, &data);
		data = SET_BITSLICE(data, EN_INT1_PAD_HIGHG,
				state);
		comres |= bma2x2_smbus_write_byte(client,
				EN_INT1_PAD_HIGHG__REG, &data);
		break;
	case 2:
		comres = bma2x2_smbus_read_byte(client,
				EN_INT1_PAD_SLOPE__REG, &data);
		data = SET_BITSLICE(data, EN_INT1_PAD_SLOPE,
				state);
		comres |= bma2x2_smbus_write_byte(client,
				EN_INT1_PAD_SLOPE__REG, &data);
		break;
	case 3:
		comres = bma2x2_smbus_read_byte(client,
				EN_INT1_PAD_DB_TAP__REG, &data);
		data = SET_BITSLICE(data, EN_INT1_PAD_DB_TAP,
				state);
		comres |= bma2x2_smbus_write_byte(client,
				EN_INT1_PAD_DB_TAP__REG, &data);
		break;
	case 4:
		comres = bma2x2_smbus_read_byte(client,
				EN_INT1_PAD_SNG_TAP__REG, &data);
		data = SET_BITSLICE(data, EN_INT1_PAD_SNG_TAP,
				state);
		comres |= bma2x2_smbus_write_byte(client,
				EN_INT1_PAD_SNG_TAP__REG, &data);
		break;
	case 5:
		comres = bma2x2_smbus_read_byte(client,
				EN_INT1_PAD_ORIENT__REG, &data);
		data = SET_BITSLICE(data, EN_INT1_PAD_ORIENT,
				state);
		comres |= bma2x2_smbus_write_byte(client,
				EN_INT1_PAD_ORIENT__REG, &data);
		break;
	case 6:
		comres = bma2x2_smbus_read_byte(client,
				EN_INT1_PAD_FLAT__REG, &data);
		data = SET_BITSLICE(data, EN_INT1_PAD_FLAT,
				state);
		comres |= bma2x2_smbus_write_byte(client,
				EN_INT1_PAD_FLAT__REG, &data);
		break;
	case 7:
		comres = bma2x2_smbus_read_byte(client,
				EN_INT1_PAD_SLO_NO_MOT__REG, &data);
		data = SET_BITSLICE(data, EN_INT1_PAD_SLO_NO_MOT,
				state);
		comres |= bma2x2_smbus_write_byte(client,
				EN_INT1_PAD_SLO_NO_MOT__REG, &data);
		break;
	default:
		pr_info("%s : unknown int1_sel = %d\n", __func__, int1_sel);
		break;
	}

	return comres;
}
#endif /* ENABLE_INT1 */
#ifdef ENABLE_INT2
static int bma2x2_set_int2_pad_sel(struct i2c_client *client, unsigned char
		int2_sel)
{
	int comres;
	unsigned char data;
	unsigned char state;
	state = 0x01;

	switch (int2_sel) {
	case 0:
		comres = bma2x2_smbus_read_byte(client,
				EN_INT2_PAD_LOWG__REG, &data);
		data = SET_BITSLICE(data, EN_INT2_PAD_LOWG,
				state);
		comres |= bma2x2_smbus_write_byte(client,
				EN_INT2_PAD_LOWG__REG, &data);
		break;
	case 1:
		comres = bma2x2_smbus_read_byte(client,
				EN_INT2_PAD_HIGHG__REG, &data);
		data = SET_BITSLICE(data, EN_INT2_PAD_HIGHG,
				state);
		comres |= bma2x2_smbus_write_byte(client,
				EN_INT2_PAD_HIGHG__REG, &data);
		break;
	case 2:
		comres = bma2x2_smbus_read_byte(client,
				EN_INT2_PAD_SLOPE__REG, &data);
		data = SET_BITSLICE(data, EN_INT2_PAD_SLOPE,
				state);
		comres |= bma2x2_smbus_write_byte(client,
				EN_INT2_PAD_SLOPE__REG, &data);
		break;
	case 3:
		comres = bma2x2_smbus_read_byte(client,
				EN_INT2_PAD_DB_TAP__REG, &data);
		data = SET_BITSLICE(data, EN_INT2_PAD_DB_TAP,
				state);
		comres |= bma2x2_smbus_write_byte(client,
				EN_INT2_PAD_DB_TAP__REG, &data);
		break;
	case 4:
		comres = bma2x2_smbus_read_byte(client,
				EN_INT2_PAD_SNG_TAP__REG, &data);
		data = SET_BITSLICE(data, EN_INT2_PAD_SNG_TAP,
				state);
		comres |= bma2x2_smbus_write_byte(client,
				EN_INT2_PAD_SNG_TAP__REG, &data);
		break;
	case 5:
		comres = bma2x2_smbus_read_byte(client,
				EN_INT2_PAD_ORIENT__REG, &data);
		data = SET_BITSLICE(data, EN_INT2_PAD_ORIENT,
				state);
		comres |= bma2x2_smbus_write_byte(client,
				EN_INT2_PAD_ORIENT__REG, &data);
		break;
	case 6:
		comres = bma2x2_smbus_read_byte(client,
				EN_INT2_PAD_FLAT__REG, &data);
		data = SET_BITSLICE(data, EN_INT2_PAD_FLAT,
				state);
		comres |= bma2x2_smbus_write_byte(client,
				EN_INT2_PAD_FLAT__REG, &data);
		break;
	case 7:
		comres = bma2x2_smbus_read_byte(client,
				EN_INT2_PAD_SLO_NO_MOT__REG, &data);
		data = SET_BITSLICE(data, EN_INT2_PAD_SLO_NO_MOT,
				state);
		comres |= bma2x2_smbus_write_byte(client,
				EN_INT2_PAD_SLO_NO_MOT__REG, &data);
		break;
	default:
		pr_info("%s : unknown int2_sel = %d\n", __func__, int2_sel);
		break;
	}

	return comres;
}
#endif /* ENABLE_INT2 */

static int bma2x2_set_Int_Enable(struct i2c_client *client, unsigned char
		interrupttype , unsigned char value)
{
	int comres;
	unsigned char data1, data2;

	if ((11 < interrupttype) && (interrupttype < 16)) {
		switch (interrupttype) {
		case 12:
			/* slow/no motion X Interrupt  */
			comres = bma2x2_smbus_read_byte(client,
				INT_SLO_NO_MOT_EN_X_INT__REG, &data1);
			data1 = SET_BITSLICE(data1,
				INT_SLO_NO_MOT_EN_X_INT, value);
			comres |= bma2x2_smbus_write_byte(client,
				INT_SLO_NO_MOT_EN_X_INT__REG, &data1);
			break;
		case 13:
			/* slow/no motion Y Interrupt  */
			comres = bma2x2_smbus_read_byte(client,
				INT_SLO_NO_MOT_EN_Y_INT__REG, &data1);
			data1 = SET_BITSLICE(data1,
				INT_SLO_NO_MOT_EN_Y_INT, value);
			comres |= bma2x2_smbus_write_byte(client,
				INT_SLO_NO_MOT_EN_Y_INT__REG, &data1);
			break;
		case 14:
			/* slow/no motion Z Interrupt  */
			comres = bma2x2_smbus_read_byte(client,
				INT_SLO_NO_MOT_EN_Z_INT__REG, &data1);
			data1 = SET_BITSLICE(data1,
				INT_SLO_NO_MOT_EN_Z_INT, value);
			comres |= bma2x2_smbus_write_byte(client,
				INT_SLO_NO_MOT_EN_Z_INT__REG, &data1);
			break;
		case 15:
			/* slow / no motion Interrupt select */
			comres = bma2x2_smbus_read_byte(client,
				INT_SLO_NO_MOT_EN_SEL_INT__REG, &data1);
			data1 = SET_BITSLICE(data1,
				INT_SLO_NO_MOT_EN_SEL_INT, value);
			comres |= bma2x2_smbus_write_byte(client,
				INT_SLO_NO_MOT_EN_SEL_INT__REG, &data1);
		}

		return comres;
	}

	comres = bma2x2_smbus_read_byte(client, INT_ENABLE1_REG, &data1);
	comres |= bma2x2_smbus_read_byte(client, INT_ENABLE2_REG, &data2);

	value = value & 1;
	switch (interrupttype) {
	case 0:
		/* Low G Interrupt  */
		data2 = SET_BITSLICE(data2, EN_LOWG_INT, value);
		break;
	case 1:
		/* High G X Interrupt */
		data2 = SET_BITSLICE(data2, EN_HIGHG_X_INT,
				value);
		break;
	case 2:
		/* High G Y Interrupt */
		data2 = SET_BITSLICE(data2, EN_HIGHG_Y_INT,
				value);
		break;
	case 3:
		/* High G Z Interrupt */
		data2 = SET_BITSLICE(data2, EN_HIGHG_Z_INT,
				value);
		break;
	case 4:
		/* New Data Interrupt  */
		data2 = SET_BITSLICE(data2, EN_NEW_DATA_INT,
				value);
		break;
	case 5:
		/* Slope X Interrupt */
		data1 = SET_BITSLICE(data1, EN_SLOPE_X_INT,
				value);
		break;
	case 6:
		/* Slope Y Interrupt */
		data1 = SET_BITSLICE(data1, EN_SLOPE_Y_INT,
				value);
		break;
	case 7:
		/* Slope Z Interrupt */
		data1 = SET_BITSLICE(data1, EN_SLOPE_Z_INT,
				value);
		break;
	case 8:
		/* Single Tap Interrupt */
		data1 = SET_BITSLICE(data1, EN_SINGLE_TAP_INT,
				value);
		break;
	case 9:
		/* Double Tap Interrupt */
		data1 = SET_BITSLICE(data1, EN_DOUBLE_TAP_INT,
				value);
		break;
	case 10:
		/* Orient Interrupt  */
		data1 = SET_BITSLICE(data1, EN_ORIENT_INT, value);
		break;
	case 11:
		/* Flat Interrupt */
		data1 = SET_BITSLICE(data1, EN_FLAT_INT, value);
		break;
	default:
		pr_info("%s : unknown int type %d\n", __func__, interrupttype);
		break;
	}
	comres |= bma2x2_smbus_write_byte(client, INT_ENABLE1_REG,
			&data1);
	comres |= bma2x2_smbus_write_byte(client, INT_ENABLE2_REG,
			&data2);

	return comres;
}

#if defined(ENABLE_INT1) || defined(ENABLE_INT2)
static int bma2x2_get_interruptstatus1(struct i2c_client *client, unsigned char
		*intstatus)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, STATUS1_REG, &data);
	*intstatus = data;

	return comres;
}

static int bma2x2_get_HIGH_first(struct i2c_client *client, unsigned char
						param, unsigned char *intstatus)
{
	int comres;
	unsigned char data;

	switch (param) {
	case 0:
		comres = bma2x2_smbus_read_byte(client,
				STATUS_ORIENT_HIGH_REG, &data);
		data = GET_BITSLICE(data, HIGHG_FIRST_X);
		*intstatus = data;
		break;
	case 1:
		comres = bma2x2_smbus_read_byte(client,
				STATUS_ORIENT_HIGH_REG, &data);
		data = GET_BITSLICE(data, HIGHG_FIRST_Y);
		*intstatus = data;
		break;
	case 2:
		comres = bma2x2_smbus_read_byte(client,
				STATUS_ORIENT_HIGH_REG, &data);
		data = GET_BITSLICE(data, HIGHG_FIRST_Z);
		*intstatus = data;
		break;
	default:
		pr_info("%s : unknown param = %d\n", __func__, param);
		break;
	}

	return comres;
}

static int bma2x2_get_HIGH_sign(struct i2c_client *client, unsigned char
		*intstatus)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, STATUS_ORIENT_HIGH_REG,
			&data);
	data = GET_BITSLICE(data, HIGHG_SIGN_S);
	*intstatus = data;

	return comres;
}


static int bma2x2_get_slope_first(struct i2c_client *client, unsigned char
	param, unsigned char *intstatus)
{
	int comres;
	unsigned char data;

	switch (param) {
	case 0:
		comres = bma2x2_smbus_read_byte(client,
				STATUS_TAP_SLOPE_REG, &data);
		data = GET_BITSLICE(data, SLOPE_FIRST_X);
		*intstatus = data;
		break;
	case 1:
		comres = bma2x2_smbus_read_byte(client,
				STATUS_TAP_SLOPE_REG, &data);
		data = GET_BITSLICE(data, SLOPE_FIRST_Y);
		*intstatus = data;
		break;
	case 2:
		comres = bma2x2_smbus_read_byte(client,
				STATUS_TAP_SLOPE_REG, &data);
		data = GET_BITSLICE(data, SLOPE_FIRST_Z);
		*intstatus = data;
		break;
	default:
		pr_info("%s : unknown param = %d\n", __func__, param);
		break;
	}

	return comres;
}

static int bma2x2_get_slope_sign(struct i2c_client *client, unsigned char
		*intstatus)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, STATUS_TAP_SLOPE_REG,
			&data);
	data = GET_BITSLICE(data, SLOPE_SIGN_S);
	*intstatus = data;

	return comres;
}

static int bma2x2_get_orient_status(struct i2c_client *client, unsigned char
		*intstatus)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, STATUS_ORIENT_HIGH_REG,
			&data);
	data = GET_BITSLICE(data, ORIENT_S);
	*intstatus = data;

	return comres;
}

static int bma2x2_get_orient_flat_status(struct i2c_client *client, unsigned
		char *intstatus)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, STATUS_ORIENT_HIGH_REG,
			&data);
	data = GET_BITSLICE(data, FLAT_S);
	*intstatus = data;

	return comres;
}
#endif /* defined(ENABLE_INT1)||defined(ENABLE_INT2) */
static int bma2x2_set_Int_mode(struct i2c_client *client, unsigned char mode)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client,
			INT_MODE_SEL__REG, &data);
	data = SET_BITSLICE(data, INT_MODE_SEL, mode);
	comres |= bma2x2_smbus_write_byte(client,
			INT_MODE_SEL__REG, &data);

	return comres;
}

static int bma2x2_get_Int_mode(struct i2c_client *client, unsigned char *mode)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client,
			INT_MODE_SEL__REG, &data);
	data  = GET_BITSLICE(data, INT_MODE_SEL);
	*mode = data;

	return comres;
}
static int bma2x2_set_slope_duration(struct i2c_client *client, unsigned char
		duration)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client,
			SLOPE_DUR__REG, &data);
	data = SET_BITSLICE(data, SLOPE_DUR, duration);
	comres |= bma2x2_smbus_write_byte(client,
			SLOPE_DUR__REG, &data);

	return comres;
}

static int bma2x2_get_slope_duration(struct i2c_client *client, unsigned char
		*status)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client,
			SLOPE_DURN_REG, &data);
	data = GET_BITSLICE(data, SLOPE_DUR);
	*status = data;

	return comres;
}

static int bma2x2_set_slope_no_mot_duration(struct i2c_client *client,
			unsigned char duration)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client,
			BMA2x2_SLO_NO_MOT_DUR__REG, &data);
	data = SET_BITSLICE(data, BMA2x2_SLO_NO_MOT_DUR, duration);
	comres |= bma2x2_smbus_write_byte(client,
			BMA2x2_SLO_NO_MOT_DUR__REG, &data);

	return comres;
}

static int bma2x2_get_slope_no_mot_duration(struct i2c_client *client,
			unsigned char *status)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client,
			BMA2x2_SLO_NO_MOT_DUR__REG, &data);
	data = GET_BITSLICE(data, BMA2x2_SLO_NO_MOT_DUR);
	*status = data;

	return comres;
}

static int bma2x2_set_slope_threshold(struct i2c_client *client,
		unsigned char threshold)
{
	int comres;
	unsigned char data;

	data = threshold;
	comres = bma2x2_smbus_write_byte(client,
			SLOPE_THRES__REG, &data);

	return comres;
}

static int bma2x2_get_slope_threshold(struct i2c_client *client,
		unsigned char *status)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client,
			SLOPE_THRES_REG, &data);
	*status = data;

	return comres;
}

static int bma2x2_set_slope_no_mot_threshold(struct i2c_client *client,
		unsigned char threshold)
{
	int comres;
	unsigned char data;

	data = threshold;
	comres = bma2x2_smbus_write_byte(client,
			SLO_NO_MOT_THRES_REG, &data);

	return comres;
}

static int bma2x2_get_slope_no_mot_threshold(struct i2c_client *client,
		unsigned char *status)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client,
			SLO_NO_MOT_THRES_REG, &data);
	*status = data;

	return comres;
}

static int bma2x2_set_low_g_duration(struct i2c_client *client, unsigned char
		duration)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, LOWG_DUR__REG, &data);
	data = SET_BITSLICE(data, LOWG_DUR, duration);
	comres |= bma2x2_smbus_write_byte(client, LOWG_DUR__REG, &data);

	return comres;
}

static int bma2x2_get_low_g_duration(struct i2c_client *client, unsigned char
		*status)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, LOW_DURN_REG, &data);
	data = GET_BITSLICE(data, LOWG_DUR);
	*status = data;

	return comres;
}

static int bma2x2_set_low_g_threshold(struct i2c_client *client, unsigned char
		threshold)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, LOWG_THRES__REG, &data);
	data = SET_BITSLICE(data, LOWG_THRES, threshold);
	comres |= bma2x2_smbus_write_byte(client, LOWG_THRES__REG, &data);

	return comres;
}

static int bma2x2_get_low_g_threshold(struct i2c_client *client, unsigned char
		*status)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, LOW_THRES_REG, &data);
	data = GET_BITSLICE(data, LOWG_THRES);
	*status = data;

	return comres;
}

static int bma2x2_set_high_g_duration(struct i2c_client *client, unsigned char
		duration)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, HIGHG_DUR__REG, &data);
	data = SET_BITSLICE(data, HIGHG_DUR, duration);
	comres |= bma2x2_smbus_write_byte(client, HIGHG_DUR__REG, &data);

	return comres;
}

static int bma2x2_get_high_g_duration(struct i2c_client *client, unsigned char
		*status)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, HIGH_DURN_REG, &data);
	data = GET_BITSLICE(data, HIGHG_DUR);
	*status = data;

	return comres;
}

static int bma2x2_set_high_g_threshold(struct i2c_client *client, unsigned char
		threshold)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, HIGHG_THRES__REG, &data);
	data = SET_BITSLICE(data, HIGHG_THRES, threshold);
	comres |= bma2x2_smbus_write_byte(client, HIGHG_THRES__REG,
			&data);

	return comres;
}

static int bma2x2_get_high_g_threshold(struct i2c_client *client, unsigned char
		*status)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, HIGH_THRES_REG, &data);
	data = GET_BITSLICE(data, HIGHG_THRES);
	*status = data;

	return comres;
}


static int bma2x2_set_tap_duration(struct i2c_client *client, unsigned char
		duration)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, TAP_DUR__REG, &data);
	data = SET_BITSLICE(data, TAP_DUR, duration);
	comres |= bma2x2_smbus_write_byte(client, TAP_DUR__REG, &data);

	return comres;
}

static int bma2x2_get_tap_duration(struct i2c_client *client, unsigned char
		*status)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, TAP_PARAM_REG, &data);
	data = GET_BITSLICE(data, TAP_DUR);
	*status = data;

	return comres;
}

static int bma2x2_set_tap_shock(struct i2c_client *client, unsigned char setval)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, TAP_SHOCK_DURN__REG,
			&data);
	data = SET_BITSLICE(data, TAP_SHOCK_DURN, setval);
	comres |= bma2x2_smbus_write_byte(client, TAP_SHOCK_DURN__REG,
			&data);

	return comres;
}

static int bma2x2_get_tap_shock(struct i2c_client *client, unsigned char
		*status)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, TAP_PARAM_REG, &data);
	data = GET_BITSLICE(data, TAP_SHOCK_DURN);
	*status = data;

	return comres;
}

static int bma2x2_set_tap_quiet(struct i2c_client *client, unsigned char
		duration)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, TAP_QUIET_DURN__REG,
			&data);
	data = SET_BITSLICE(data, TAP_QUIET_DURN, duration);
	comres |= bma2x2_smbus_write_byte(client, TAP_QUIET_DURN__REG,
			&data);

	return comres;
}

static int bma2x2_get_tap_quiet(struct i2c_client *client, unsigned char
		*status)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, TAP_PARAM_REG, &data);
	data = GET_BITSLICE(data, TAP_QUIET_DURN);
	*status = data;

	return comres;
}

static int bma2x2_set_tap_threshold(struct i2c_client *client, unsigned char
		threshold)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, TAP_THRES__REG, &data);
	data = SET_BITSLICE(data, TAP_THRES, threshold);
	comres |= bma2x2_smbus_write_byte(client, TAP_THRES__REG, &data);

	return comres;
}

static int bma2x2_get_tap_threshold(struct i2c_client *client, unsigned char
		*status)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, TAP_THRES_REG, &data);
	data = GET_BITSLICE(data, TAP_THRES);
	*status = data;

	return comres;
}

static int bma2x2_set_tap_samp(struct i2c_client *client, unsigned char samp)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, TAP_SAMPLES__REG, &data);
	data = SET_BITSLICE(data, TAP_SAMPLES, samp);
	comres |= bma2x2_smbus_write_byte(client, TAP_SAMPLES__REG,
			&data);

	return comres;
}

static int bma2x2_get_tap_samp(struct i2c_client *client, unsigned char *status)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, TAP_THRES_REG, &data);
	data = GET_BITSLICE(data, TAP_SAMPLES);
	*status = data;

	return comres;
}

static int bma2x2_set_orient_mode(struct i2c_client *client, unsigned char mode)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, ORIENT_MODE__REG, &data);
	data = SET_BITSLICE(data, ORIENT_MODE, mode);
	comres |= bma2x2_smbus_write_byte(client, ORIENT_MODE__REG,
			&data);

	return comres;
}

static int bma2x2_get_orient_mode(struct i2c_client *client, unsigned char
		*status)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, ORIENT_PARAM_REG, &data);
	data = GET_BITSLICE(data, ORIENT_MODE);
	*status = data;

	return comres;
}

static int bma2x2_set_orient_blocking(struct i2c_client *client, unsigned char
		samp)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, ORIENT_BLOCK__REG,
			&data);
	data = SET_BITSLICE(data, ORIENT_BLOCK, samp);
	comres |= bma2x2_smbus_write_byte(client, ORIENT_BLOCK__REG,
			&data);

	return comres;
}

static int bma2x2_get_orient_blocking(struct i2c_client *client, unsigned char
		*status)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, ORIENT_PARAM_REG, &data);
	data = GET_BITSLICE(data, ORIENT_BLOCK);
	*status = data;

	return comres;
}

static int bma2x2_set_orient_hyst(struct i2c_client *client, unsigned char
		orienthyst)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, ORIENT_HYST__REG, &data);
	data = SET_BITSLICE(data, ORIENT_HYST, orienthyst);
	comres |= bma2x2_smbus_write_byte(client, ORIENT_HYST__REG,
			&data);

	return comres;
}

static int bma2x2_get_orient_hyst(struct i2c_client *client, unsigned char
		*status)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, ORIENT_PARAM_REG, &data);
	data = GET_BITSLICE(data, ORIENT_HYST);
	*status = data;

	return comres;
}
static int bma2x2_set_theta_blocking(struct i2c_client *client, unsigned char
		thetablk)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, THETA_BLOCK__REG, &data);
	data = SET_BITSLICE(data, THETA_BLOCK, thetablk);
	comres |= bma2x2_smbus_write_byte(client, THETA_BLOCK__REG,
			&data);

	return comres;
}

static int bma2x2_get_theta_blocking(struct i2c_client *client, unsigned char
		*status)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, THETA_BLOCK_REG, &data);
	data = GET_BITSLICE(data, THETA_BLOCK);
	*status = data;

	return comres;
}

static int bma2x2_set_theta_flat(struct i2c_client *client, unsigned char
		thetaflat)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, THETA_FLAT__REG, &data);
	data = SET_BITSLICE(data, THETA_FLAT, thetaflat);
	comres |= bma2x2_smbus_write_byte(client, THETA_FLAT__REG, &data);

	return comres;
}

static int bma2x2_get_theta_flat(struct i2c_client *client, unsigned char
		*status)
{
	int comres ;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, THETA_FLAT_REG, &data);
	data = GET_BITSLICE(data, THETA_FLAT);
	*status = data;

	return comres;
}

static int bma2x2_set_flat_hold_time(struct i2c_client *client, unsigned char
		holdtime)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, FLAT_HOLD_TIME__REG,
			&data);
	data = SET_BITSLICE(data, FLAT_HOLD_TIME, holdtime);
	comres |= bma2x2_smbus_write_byte(client, FLAT_HOLD_TIME__REG,
			&data);

	return comres;
}

static int bma2x2_get_flat_hold_time(struct i2c_client *client, unsigned char
		*holdtime)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, FLAT_HOLD_TIME_REG,
			&data);
	data  = GET_BITSLICE(data, FLAT_HOLD_TIME);
	*holdtime = data ;

	return comres;
}

static int bma2x2_set_mode(struct i2c_client *client, unsigned char mode)
{
	int comres;
	unsigned char data1, data2;

	if (mode < 6) {
		comres = bma2x2_smbus_read_byte(client, MODE_CTRL_REG,
				&data1);
		comres = bma2x2_smbus_read_byte(client,
				LOW_NOISE_CTRL_REG,
				&data2);
		switch (mode) {
		case MODE_NORMAL:
				data1  = SET_BITSLICE(data1,
						MODE_CTRL, 0);
				data2  = SET_BITSLICE(data2,
						LOW_POWER_MODE, 0);
				bma2x2_smbus_write_byte(client,
						MODE_CTRL_REG, &data1);
				mdelay(1);
				bma2x2_smbus_write_byte(client,
					LOW_NOISE_CTRL_REG, &data2);
				break;
		case MODE_LOWPOWER1:
				data1  = SET_BITSLICE(data1,
						MODE_CTRL, 2);
				data2  = SET_BITSLICE(data2,
						LOW_POWER_MODE, 0);
				bma2x2_smbus_write_byte(client,
						MODE_CTRL_REG, &data1);
				mdelay(1);
				bma2x2_smbus_write_byte(client,
					LOW_NOISE_CTRL_REG, &data2);
				break;
		case MODE_SUSPEND:
				data1  = SET_BITSLICE(data1,
						MODE_CTRL, 4);
				data2  = SET_BITSLICE(data2,
						LOW_POWER_MODE, 0);
				bma2x2_smbus_write_byte(client,
					LOW_NOISE_CTRL_REG, &data2);
				mdelay(1);
				bma2x2_smbus_write_byte(client,
					MODE_CTRL_REG, &data1);
				break;
		case MODE_DEEP_SUSPEND:
				data1  = SET_BITSLICE(data1,
							MODE_CTRL, 1);
				data2  = SET_BITSLICE(data2,
						LOW_POWER_MODE, 1);
				bma2x2_smbus_write_byte(client,
						MODE_CTRL_REG, &data1);
				mdelay(1);
				bma2x2_smbus_write_byte(client,
					LOW_NOISE_CTRL_REG, &data2);
				break;
		case MODE_LOWPOWER2:
				data1  = SET_BITSLICE(data1,
						MODE_CTRL, 2);
				data2  = SET_BITSLICE(data2,
						LOW_POWER_MODE, 1);
				bma2x2_smbus_write_byte(client,
						MODE_CTRL_REG, &data1);
				mdelay(1);
				bma2x2_smbus_write_byte(client,
					LOW_NOISE_CTRL_REG, &data2);
				break;
		case MODE_STANDBY:
				data1  = SET_BITSLICE(data1,
						MODE_CTRL, 4);
				data2  = SET_BITSLICE(data2,
						LOW_POWER_MODE, 1);
				bma2x2_smbus_write_byte(client,
					LOW_NOISE_CTRL_REG, &data2);
				mdelay(1);
				bma2x2_smbus_write_byte(client,
						MODE_CTRL_REG, &data1);
		break;
		}
	} else {
		comres = -1 ;
	}

	return comres;
}

static int bma2x2_get_mode(struct i2c_client *client, unsigned char *mode)
{
	int comres;
	unsigned char data1, data2;

	comres = bma2x2_smbus_read_byte(client, MODE_CTRL_REG, &data1);
	comres |= bma2x2_smbus_read_byte(client, LOW_NOISE_CTRL_REG,
			&data2);

	data1  = (data1 & 0xE0) >> 5;
	data2  = (data2 & 0x40) >> 6;

	if ((data1 == 0x00) && (data2 == 0x00)) {
		*mode  = MODE_NORMAL;
		return comres;
	}

	if ((data1 == 0x02) && (data2 == 0x00)) {
		*mode  = MODE_LOWPOWER1;
		return comres;
	}

	if ((data1 == 0x04 || data1 == 0x06) && (data2 == 0x00)) {
		*mode  = MODE_SUSPEND;
		return comres;
	}

	if ((data1 & 0x01) == 0x01) {
		*mode  = MODE_DEEP_SUSPEND;
		return comres;
	}

	if ((data1 == 0x02) && (data2 == 0x01)) {
		*mode  = MODE_LOWPOWER2;
		return comres;
	}

	if ((data1 == 0x04) && (data2 == 0x01)) {
		*mode  = MODE_STANDBY;
		return comres;
	}

	*mode = MODE_DEEP_SUSPEND;

	return comres;
}

static int bma2x2_set_range(struct i2c_client *client, unsigned char Range)
{
	int comres ;
	unsigned char data1;

	if ((Range == RANGE_2G)
		|| (Range == RANGE_4G)
		|| (Range == RANGE_8G)
		|| (Range == RANGE_16G)) {
		comres = bma2x2_smbus_read_byte(client, RANGE_SEL_REG,
				&data1);
		pr_info("%s : 0x%d -> read :0x%x\n", __func__, Range, data1);
		data1 = SET_BITSLICE(data1, RANGE_SEL, Range);

		comres |= bma2x2_smbus_write_byte(client, RANGE_SEL_REG,
				&data1);
	} else {
		pr_info("%s : range error 0x%x\n", __func__, Range);
		comres = -1 ;
	}

	return comres;
}

static int bma2x2_get_range(struct i2c_client *client, unsigned char *Range)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, RANGE_SEL__REG, &data);
	data = GET_BITSLICE(data, RANGE_SEL);
	*Range = data;

	return comres;
}

static int bma2x2_set_bandwidth(struct i2c_client *client, unsigned char BW)
{
	int comres;
	unsigned char data;
	int Bandwidth = 0;

	if (BW > 7 && BW < 16) {
		switch (BW) {
		case BW_7_81HZ:
			Bandwidth = BW_7_81HZ;
			/*  7.81 Hz      64000 uS   */
			break;
		case BW_15_63HZ:
			Bandwidth = BW_15_63HZ;
			/*  15.63 Hz     32000 uS   */
			break;
		case BW_31_25HZ:
			Bandwidth = BW_31_25HZ;
			/*  31.25 Hz     16000 uS   */
			break;
		case BW_62_50HZ:
			Bandwidth = BW_62_50HZ;
			/*  62.50 Hz     8000 uS   */
			break;
		case BW_125HZ:
			Bandwidth = BW_125HZ;
			/*  125 Hz       4000 uS   */
			break;
		case BW_250HZ:
			Bandwidth = BW_250HZ;
			/*  250 Hz       2000 uS   */
			break;
		case BW_500HZ:
			Bandwidth = BW_500HZ;
			/*  500 Hz       1000 uS   */
			break;
		case BW_1000HZ:
			Bandwidth = BW_1000HZ;
			/*  1000 Hz      500 uS   */
			break;
		}
		comres = bma2x2_smbus_read_byte(client, BANDWIDTH__REG,
				&data);
		data = SET_BITSLICE(data, BANDWIDTH, Bandwidth);
		comres |= bma2x2_smbus_write_byte(client, BANDWIDTH__REG,
				&data);
	} else {
		comres = -1 ;
	}

	return comres;
}

static int bma2x2_get_bandwidth(struct i2c_client *client, unsigned char *BW)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, BANDWIDTH__REG, &data);
	data = GET_BITSLICE(data, BANDWIDTH);
	*BW = data ;

	return comres;
}

int bma2x2_get_sleep_duration(struct i2c_client *client, unsigned char
		*sleep_dur)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client,
			SLEEP_DUR__REG, &data);
	data = GET_BITSLICE(data, SLEEP_DUR);
	*sleep_dur = data;

	return comres;
}

int bma2x2_set_sleep_duration(struct i2c_client *client, unsigned char
		sleep_dur)
{
	int comres;
	unsigned char data;
	int sleep_duration = 0;

	if (sleep_dur > 4 && sleep_dur < 16) {
		switch (sleep_dur) {
		case SLEEP_DUR_0_5MS:
			sleep_duration = SLEEP_DUR_0_5MS;
			/*  0.5 MS   */
			break;
		case SLEEP_DUR_1MS:
			sleep_duration = SLEEP_DUR_1MS;
			/*  1 MS  */
			break;
		case SLEEP_DUR_2MS:
			sleep_duration = SLEEP_DUR_2MS;
			/*  2 MS  */
			break;
		case SLEEP_DUR_4MS:
			sleep_duration = SLEEP_DUR_4MS;
			/*  4 MS   */
			break;
		case SLEEP_DUR_6MS:
			sleep_duration = SLEEP_DUR_6MS;
			/*  6 MS  */
			break;
		case SLEEP_DUR_10MS:
			sleep_duration = SLEEP_DUR_10MS;
			/*  10 MS  */
			break;
		case SLEEP_DUR_25MS:
			sleep_duration = SLEEP_DUR_25MS;
			/*  25 MS  */
			break;
		case SLEEP_DUR_50MS:
			sleep_duration = SLEEP_DUR_50MS;
			/*  50 MS   */
			break;
		case SLEEP_DUR_100MS:
			sleep_duration = SLEEP_DUR_100MS;
			/*  100 MS  */
			break;
		case SLEEP_DUR_500MS:
			sleep_duration = SLEEP_DUR_500MS;
			/*  500 MS   */
			break;
		case SLEEP_DUR_1S:
			sleep_duration = SLEEP_DUR_1S;
			/*  1 SECS   */
			break;
		}
		comres = bma2x2_smbus_read_byte(client, SLEEP_DUR__REG,
				&data);
		data = SET_BITSLICE(data, SLEEP_DUR,
				sleep_duration);
		comres = bma2x2_smbus_write_byte(client, SLEEP_DUR__REG,
				&data);
	} else {
		comres = -1 ;
	}

	return comres;
}

static int bma2x2_get_fifo_mode(struct i2c_client *client, unsigned char
		*fifo_mode)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, FIFO_MODE__REG, &data);
	*fifo_mode = GET_BITSLICE(data, FIFO_MODE);

	return comres;
}

static int bma2x2_set_fifo_mode(struct i2c_client *client, unsigned char
		fifo_mode)
{
	unsigned char data;
	int comres;

	if (fifo_mode < 4) {
		comres = bma2x2_smbus_read_byte(client, FIFO_MODE__REG,
				&data);
		data = SET_BITSLICE(data, FIFO_MODE, fifo_mode);
		comres = bma2x2_smbus_write_byte(client, FIFO_MODE__REG,
				&data);
	} else {
		comres = -1 ;
	}

	return comres;
}


static int bma2x2_get_fifo_trig(struct i2c_client *client, unsigned char
		*fifo_trig)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client,
			FIFO_TRIGGER_ACTION__REG, &data);
	*fifo_trig = GET_BITSLICE(data, FIFO_TRIGGER_ACTION);

	return comres;
}

static int bma2x2_set_fifo_trig(struct i2c_client *client, unsigned char
		fifo_trig)
{
	unsigned char data;
	int comres;

	if (fifo_trig < 4) {
		comres = bma2x2_smbus_read_byte(client,
				FIFO_TRIGGER_ACTION__REG, &data);
		data = SET_BITSLICE(data, FIFO_TRIGGER_ACTION,
				fifo_trig);
		comres = bma2x2_smbus_write_byte(client,
				FIFO_TRIGGER_ACTION__REG, &data);
	} else {
		comres = -1 ;
	}

	return comres;
}

static int bma2x2_get_fifo_trig_src(struct i2c_client *client, unsigned char
		*trig_src)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client,
			FIFO_TRIGGER_SOURCE__REG, &data);
	*trig_src = GET_BITSLICE(data, FIFO_TRIGGER_SOURCE);

	return comres;
}


static int bma2x2_set_fifo_trig_src(struct i2c_client *client, unsigned char
		trig_src)
{
	unsigned char data;
	int comres;

	if (trig_src < 4) {
		comres = bma2x2_smbus_read_byte(client,
				FIFO_TRIGGER_SOURCE__REG, &data);
		data = SET_BITSLICE(data, FIFO_TRIGGER_SOURCE,
				trig_src);
		comres = bma2x2_smbus_write_byte(client,
				FIFO_TRIGGER_SOURCE__REG, &data);
	} else {
		comres = -1 ;
	}

	return comres;
}

static int bma2x2_get_fifo_framecount(struct i2c_client *client, unsigned char
			 *framecount)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client,
			FIFO_FRAME_COUNTER_S__REG, &data);
	*framecount = GET_BITSLICE(data, FIFO_FRAME_COUNTER_S);

	return comres;
}

static int bma2x2_get_fifo_data_sel(struct i2c_client *client, unsigned char
		*data_sel)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client,
			FIFO_DATA_SELECT__REG, &data);
	*data_sel = GET_BITSLICE(data, FIFO_DATA_SELECT);

	return comres;
}

static int bma2x2_set_fifo_data_sel(struct i2c_client *client, unsigned char
		data_sel)
{
	unsigned char data;
	int comres;

	if (data_sel < 4) {
		comres = bma2x2_smbus_read_byte(client,
				FIFO_DATA_SELECT__REG,
				&data);
		data = SET_BITSLICE(data, FIFO_DATA_SELECT,
				data_sel);
		comres |= bma2x2_smbus_write_byte(client,
				FIFO_DATA_SELECT__REG,
				&data);
	} else {
		comres = -1 ;
	}

	return comres;
}

static int bma2x2_get_fifo_data_out_reg(struct i2c_client *client, unsigned char
		*out_reg)
{
	unsigned char data;
	int comres;

	comres = bma2x2_smbus_read_byte(client,
				FIFO_DATA_OUTPUT_REG, &data);
	*out_reg = data;

	return comres;
}

static int bma2x2_get_offset_target(struct i2c_client *client, unsigned char
		channel, unsigned char *offset)
{
	unsigned char data;
	int comres;

	switch (channel) {
	case CUT_OFF:
		comres = bma2x2_smbus_read_byte(client,
				COMP_CUTOFF__REG, &data);
		*offset = GET_BITSLICE(data, COMP_CUTOFF);
		break;
	case OFFSET_TRIGGER_X:
		comres = bma2x2_smbus_read_byte(client,
			COMP_TARGET_OFFSET_X__REG, &data);
		*offset = GET_BITSLICE(data,
				COMP_TARGET_OFFSET_X);
		break;
	case OFFSET_TRIGGER_Y:
		comres = bma2x2_smbus_read_byte(client,
			COMP_TARGET_OFFSET_Y__REG, &data);
		*offset = GET_BITSLICE(data,
				COMP_TARGET_OFFSET_Y);
		break;
	case OFFSET_TRIGGER_Z:
		comres = bma2x2_smbus_read_byte(client,
			COMP_TARGET_OFFSET_Z__REG, &data);
		*offset = GET_BITSLICE(data,
				COMP_TARGET_OFFSET_Z);
		break;
	default:
		pr_info("%s : unknown channel = %d\n", __func__, channel);
		comres = -1;
		break;
	}

	return comres;
}

static int bma2x2_set_offset_target(struct i2c_client *client,
		unsigned char channel, unsigned char offset)
{
	unsigned char data;
	int comres;

	switch (channel) {
	case CUT_OFF:
		comres = bma2x2_smbus_read_byte(client,
				COMP_CUTOFF__REG, &data);
		data = SET_BITSLICE(data,
				COMP_CUTOFF,
				offset);
		comres = bma2x2_smbus_write_byte(client,
				COMP_CUTOFF__REG, &data);
		break;
	case OFFSET_TRIGGER_X:
		comres = bma2x2_smbus_read_byte(client,
				COMP_TARGET_OFFSET_X__REG,
				&data);
		data = SET_BITSLICE(data,
				COMP_TARGET_OFFSET_X,
				offset);
		comres = bma2x2_smbus_write_byte(client,
				COMP_TARGET_OFFSET_X__REG,
				&data);
		break;
	case OFFSET_TRIGGER_Y:
		comres = bma2x2_smbus_read_byte(client,
				COMP_TARGET_OFFSET_Y__REG,
				&data);
		data = SET_BITSLICE(data,
				COMP_TARGET_OFFSET_Y,
				offset);
		comres = bma2x2_smbus_write_byte(client,
				COMP_TARGET_OFFSET_Y__REG,
				&data);
		break;
	case OFFSET_TRIGGER_Z:
		comres = bma2x2_smbus_read_byte(client,
				COMP_TARGET_OFFSET_Z__REG,
				&data);
		data = SET_BITSLICE(data,
				COMP_TARGET_OFFSET_Z,
				offset);
		comres = bma2x2_smbus_write_byte(client,
				COMP_TARGET_OFFSET_Z__REG,
				&data);
		break;
	default:
		pr_info("%s : unknown channel = %d\n", __func__, channel);
		comres = -1;
		break;
	}

	return comres;
}

static int bma2x2_get_cal_ready(struct i2c_client *client,
				unsigned char *calrdy)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, FAST_CAL_RDY_S__REG,
			&data);
	data = GET_BITSLICE(data, FAST_CAL_RDY_S);
	*calrdy = data;

	return comres;
}

static int bma2x2_set_cal_trigger(struct i2c_client *client,
				unsigned char caltrigger)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, CAL_TRIGGER__REG, &data);
	data = SET_BITSLICE(data, CAL_TRIGGER, caltrigger);
	comres |= bma2x2_smbus_write_byte(client, CAL_TRIGGER__REG,
			&data);

	return comres;
}

static int bma2x2_write_reg(struct i2c_client *client, unsigned char addr,
				unsigned char *data)
{
	int comres;
	comres = bma2x2_smbus_write_byte(client, addr, data);

	return comres;
}


static int bma2x2_set_offset_x(struct i2c_client *client,
				unsigned char offsetfilt)
{
	int comres;
	unsigned char data;

	data =  offsetfilt;
	comres = bma2x2_smbus_write_byte(client, OFFSET_X_AXIS_REG,
						&data);
	return comres;
}


static int bma2x2_get_offset_x(struct i2c_client *client,
				unsigned char *offsetfilt)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, OFFSET_X_AXIS_REG,
						&data);
	*offsetfilt = data;
	return comres;
}

static int bma2x2_set_offset_y(struct i2c_client *client,
				unsigned char offsetfilt)
{
	int comres;
	unsigned char data;

	data =  offsetfilt;
	comres = bma2x2_smbus_write_byte(client, OFFSET_Y_AXIS_REG,
						&data);
	return comres;
}

static int bma2x2_get_offset_y(struct i2c_client *client,
				unsigned char *offsetfilt)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, OFFSET_Y_AXIS_REG,
						&data);
	*offsetfilt = data;
	return comres;
}

static int bma2x2_set_offset_z(struct i2c_client *client,
				unsigned char offsetfilt)
{
	int comres;
	unsigned char data;

	data =  offsetfilt - 1024;
	comres = bma2x2_smbus_write_byte(client, OFFSET_Z_AXIS_REG,
						&data);
	return comres;
}

static int bma2x2_get_offset_z(struct i2c_client *client,
				unsigned char *offsetfilt)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, OFFSET_Z_AXIS_REG,
						&data);
	*offsetfilt = data + 1024;
	return comres;
}


static int bma2x2_set_selftest_st(struct i2c_client *client,
				unsigned char selftest)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, EN_SELF_TEST__REG,
			&data);
	data = SET_BITSLICE(data, EN_SELF_TEST, selftest);
	comres |= bma2x2_smbus_write_byte(client, EN_SELF_TEST__REG,
			&data);

	return comres;
}

static int bma2x2_set_selftest_stn(struct i2c_client *client,
				unsigned char stn)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, NEG_SELF_TEST__REG,
			&data);
	data = SET_BITSLICE(data, NEG_SELF_TEST, stn);
	comres |= bma2x2_smbus_write_byte(client, NEG_SELF_TEST__REG,
			&data);

	return comres;
}

static int bma2x2_set_selftest_amp(struct i2c_client *client,
				unsigned char amp)
{
	int comres;
	unsigned char data;

	comres = bma2x2_smbus_read_byte(client, SELF_TEST_AMP__REG,
			&data);
	data = SET_BITSLICE(data, SELF_TEST_AMP, amp);
	comres |= bma2x2_smbus_write_byte(client, SELF_TEST_AMP__REG,
			&data);

	return comres;
}

static int bma2x2_read_accel_x(struct i2c_client *client,
				signed char sensor_type, short *a_x)
{
	int comres;
	unsigned char data[2];

	switch (sensor_type) {
	case 0:
		comres = bma2x2_smbus_read_byte_block(client,
					ACC_X12_LSB__REG, data, 2);
		*a_x = GET_BITSLICE(data[0], ACC_X12_LSB)|
			(GET_BITSLICE(data[1],
				ACC_X_MSB)<<(ACC_X12_LSB__LEN));
		*a_x = *a_x << (sizeof(short)*8-(ACC_X12_LSB__LEN
					+ ACC_X_MSB__LEN));
		*a_x = *a_x >> (sizeof(short)*8-(ACC_X12_LSB__LEN
					+ ACC_X_MSB__LEN));
		break;
	case 1:
		comres = bma2x2_smbus_read_byte_block(client,
					ACC_X10_LSB__REG, data, 2);
		*a_x = GET_BITSLICE(data[0], ACC_X10_LSB)|
			(GET_BITSLICE(data[1],
				ACC_X_MSB)<<(ACC_X10_LSB__LEN));
		*a_x = *a_x << (sizeof(short)*8-(ACC_X10_LSB__LEN
					+ ACC_X_MSB__LEN));
		*a_x = *a_x >> (sizeof(short)*8-(ACC_X10_LSB__LEN
					+ ACC_X_MSB__LEN));
		break;
	case 2:
		comres = bma2x2_smbus_read_byte_block(client,
					ACC_X8_LSB__REG, data, 2);
		*a_x = GET_BITSLICE(data[0], ACC_X8_LSB)|
			(GET_BITSLICE(data[1],
				ACC_X_MSB)<<(ACC_X8_LSB__LEN));
		*a_x = *a_x << (sizeof(short)*8-(ACC_X8_LSB__LEN
					+ ACC_X_MSB__LEN));
		*a_x = *a_x >> (sizeof(short)*8-(ACC_X8_LSB__LEN
					+ ACC_X_MSB__LEN));
		break;
	case 3:
		comres = bma2x2_smbus_read_byte_block(client,
					ACC_X14_LSB__REG, data, 2);
		*a_x = GET_BITSLICE(data[0], ACC_X14_LSB)|
			(GET_BITSLICE(data[1],
				ACC_X_MSB)<<(ACC_X14_LSB__LEN));
		*a_x = *a_x << (sizeof(short)*8-(ACC_X14_LSB__LEN
					+ ACC_X_MSB__LEN));
		*a_x = *a_x >> (sizeof(short)*8-(ACC_X14_LSB__LEN
					+ ACC_X_MSB__LEN));
		break;
	default:
		comres = -1;
		pr_info("%s : unkown sensor type = %d\n",
				__func__, sensor_type);
		break;
	}

	return comres;
}

static int bma2x2_soft_reset(struct i2c_client *client)
{
	int comres;
	unsigned char data = EN_SOFT_RESET_VALUE ;

	comres = bma2x2_smbus_write_byte(client, EN_SOFT_RESET__REG,
					&data);

	return comres;
}

static int bma2x2_read_accel_y(struct i2c_client *client,
				signed char sensor_type, short *a_y)
{
	int comres;
	unsigned char data[2];

	switch (sensor_type) {
	case 0:
		comres = bma2x2_smbus_read_byte_block(client,
				ACC_Y12_LSB__REG, data, 2);
		*a_y = GET_BITSLICE(data[0], ACC_Y12_LSB)|
			(GET_BITSLICE(data[1],
				ACC_Y_MSB)<<(ACC_Y12_LSB__LEN));
		*a_y = *a_y << (sizeof(short)*8-(ACC_Y12_LSB__LEN
						+ ACC_Y_MSB__LEN));
		*a_y = *a_y >> (sizeof(short)*8-(ACC_Y12_LSB__LEN
						+ ACC_Y_MSB__LEN));
		break;
	case 1:
		comres = bma2x2_smbus_read_byte_block(client,
				ACC_Y10_LSB__REG, data, 2);
		*a_y = GET_BITSLICE(data[0], ACC_Y10_LSB)|
			(GET_BITSLICE(data[1],
				ACC_Y_MSB)<<(ACC_Y10_LSB__LEN));
		*a_y = *a_y << (sizeof(short)*8-(ACC_Y10_LSB__LEN
						+ ACC_Y_MSB__LEN));
		*a_y = *a_y >> (sizeof(short)*8-(ACC_Y10_LSB__LEN
						+ ACC_Y_MSB__LEN));
		break;
	case 2:
		comres = bma2x2_smbus_read_byte_block(client,
				ACC_Y8_LSB__REG, data, 2);
		*a_y = GET_BITSLICE(data[0], ACC_Y8_LSB)|
				(GET_BITSLICE(data[1],
				ACC_Y_MSB)<<(ACC_Y8_LSB__LEN));
		*a_y = *a_y << (sizeof(short)*8-(ACC_Y8_LSB__LEN
						+ ACC_Y_MSB__LEN));
		*a_y = *a_y >> (sizeof(short)*8-(ACC_Y8_LSB__LEN
						+ ACC_Y_MSB__LEN));
		break;
	case 3:
		comres = bma2x2_smbus_read_byte_block(client,
				ACC_Y14_LSB__REG, data, 2);
		*a_y = GET_BITSLICE(data[0], ACC_Y14_LSB)|
			(GET_BITSLICE(data[1],
				ACC_Y_MSB)<<(ACC_Y14_LSB__LEN));
		*a_y = *a_y << (sizeof(short)*8-(ACC_Y14_LSB__LEN
						+ ACC_Y_MSB__LEN));
		*a_y = *a_y >> (sizeof(short)*8-(ACC_Y14_LSB__LEN
						+ ACC_Y_MSB__LEN));
		break;
	default:
		comres = -1;
		pr_info("%s : unkown sensor type = %d\n",
				__func__, sensor_type);
		break;
	}

	return comres;
}

static int bma2x2_read_accel_z(struct i2c_client *client,
				signed char sensor_type, short *a_z)
{
	int comres;
	unsigned char data[2];

	switch (sensor_type) {
	case 0:
		comres = bma2x2_smbus_read_byte_block(client,
				ACC_Z12_LSB__REG, data, 2);
		*a_z = GET_BITSLICE(data[0], ACC_Z12_LSB)|
			(GET_BITSLICE(data[1],
				ACC_Z_MSB)<<(ACC_Z12_LSB__LEN));
		*a_z = *a_z << (sizeof(short)*8-(ACC_Z12_LSB__LEN
						+ ACC_Z_MSB__LEN));
		*a_z = *a_z >> (sizeof(short)*8-(ACC_Z12_LSB__LEN
						+ ACC_Z_MSB__LEN));
		break;
	case 1:
		comres = bma2x2_smbus_read_byte_block(client,
				ACC_Z10_LSB__REG, data, 2);
		*a_z = GET_BITSLICE(data[0], ACC_Z10_LSB)|
			(GET_BITSLICE(data[1],
				ACC_Z_MSB)<<(ACC_Z10_LSB__LEN));
		*a_z = *a_z << (sizeof(short)*8-(ACC_Z10_LSB__LEN
						+ ACC_Z_MSB__LEN));
		*a_z = *a_z >> (sizeof(short)*8-(ACC_Z10_LSB__LEN
						+ ACC_Z_MSB__LEN));
		break;
	case 2:
		comres = bma2x2_smbus_read_byte_block(client,
				ACC_Z8_LSB__REG, data, 2);
		*a_z = GET_BITSLICE(data[0], ACC_Z8_LSB)|
			(GET_BITSLICE(data[1],
				ACC_Z_MSB)<<(ACC_Z8_LSB__LEN));
		*a_z = *a_z << (sizeof(short)*8-(ACC_Z8_LSB__LEN
						+ ACC_Z_MSB__LEN));
		*a_z = *a_z >> (sizeof(short)*8-(ACC_Z8_LSB__LEN
						+ ACC_Z_MSB__LEN));
		break;
	case 3:
		comres = bma2x2_smbus_read_byte_block(client,
				ACC_Z14_LSB__REG, data, 2);
		*a_z = GET_BITSLICE(data[0], ACC_Z14_LSB)|
				(GET_BITSLICE(data[1],
				ACC_Z_MSB)<<(ACC_Z14_LSB__LEN));
		*a_z = *a_z << (sizeof(short)*8-(ACC_Z14_LSB__LEN
						+ ACC_Z_MSB__LEN));
		*a_z = *a_z >> (sizeof(short)*8-(ACC_Z14_LSB__LEN
						+ ACC_Z_MSB__LEN));
		break;
	default:
		comres = -1;
		pr_info("%s : unkown sensor type = %d\n",
				__func__, sensor_type);
		break;
	}

	return comres;
}

static int bma2x2_open_cal(struct i2c_client *client)
{
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	int cal_data[3];
	int err;
	mm_segment_t old_fs;
	struct file *cal_filp = NULL;

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	cal_filp = filp_open(CALIBRATION_FILE_PATH,
		O_RDONLY, S_IRUGO | S_IWUSR | S_IWGRP);
	if (IS_ERR(cal_filp)) {
		err = PTR_ERR(cal_filp);
		if (err != -ENOENT)
			pr_err("%s :  Can't open calibration file\n",
			__func__);
		set_fs(old_fs);
		err = PTR_ERR(cal_filp);
		return err;
	}

	err = cal_filp->f_op->read(cal_filp,
		(char *)cal_data,
		3 * sizeof(int), &cal_filp->f_pos);
	if (err != 3 * sizeof(int)) {
		pr_err("%s :  Can't read the cal data from file\n",
			__func__);
		filp_close(cal_filp, current->files);
		set_fs(old_fs);
		return -EIO;
	}

	bma2x2_set_offset_x(bma2x2->bma2x2_client, (unsigned char)cal_data[0]);
	bma2x2_set_offset_y(bma2x2->bma2x2_client, (unsigned char)cal_data[1]);
	bma2x2_set_offset_z(bma2x2->bma2x2_client, (unsigned char)cal_data[2]);

	return 0;
}

static int bma2x2_read_temperature(struct i2c_client *client,
					signed char *temperature)
{
	unsigned char data;
	int comres;

	comres = bma2x2_smbus_read_byte(client, TEMPERATURE_REG, &data);
	*temperature = (signed char)data;

	return comres;
}

static ssize_t bma2x2_enable_int_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int type, value;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	sscanf(buf, "%d%d", &type, &value);

	if (bma2x2_set_Int_Enable(bma2x2->bma2x2_client, type, value) < 0)
		return -EINVAL;

	return count;
}


static ssize_t bma2x2_int_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_Int_mode(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);
}

static ssize_t bma2x2_int_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoint(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_Int_mode(bma2x2->bma2x2_client, (unsigned char)data) < 0)
		return -EINVAL;

	return count;
}
static ssize_t bma2x2_slope_duration_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_slope_duration(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_slope_duration_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoint(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_slope_duration(bma2x2->bma2x2_client,
			(unsigned char)data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_slope_no_mot_duration_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_slope_no_mot_duration(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_slope_no_mot_duration_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoint(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_slope_no_mot_duration(bma2x2->bma2x2_client,
			(unsigned char)data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_slope_threshold_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_slope_threshold(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);
}

static ssize_t bma2x2_slope_threshold_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoint(buf, 10, &data);
	if (error)
		return error;
	if (bma2x2_set_slope_threshold(bma2x2->bma2x2_client,
			(unsigned char)data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_slope_no_mot_threshold_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_slope_no_mot_threshold(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);
}

static ssize_t bma2x2_slope_no_mot_threshold_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoint(buf, 10, &data);
	if (error)
		return error;
	if (bma2x2_set_slope_no_mot_threshold(bma2x2->bma2x2_client,
			(unsigned char)data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_high_g_duration_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_high_g_duration(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);
}

static ssize_t bma2x2_high_g_duration_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoint(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_high_g_duration(bma2x2->bma2x2_client,
			(unsigned char)data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_high_g_threshold_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_high_g_threshold(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);
}

static ssize_t bma2x2_high_g_threshold_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoint(buf, 10, &data);
	if (error)
		return error;
	if (bma2x2_set_high_g_threshold(bma2x2->bma2x2_client, (unsigned
					char)data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_low_g_duration_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_low_g_duration(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);
}

static ssize_t bma2x2_low_g_duration_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoint(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_low_g_duration(bma2x2->bma2x2_client, (unsigned
					char)data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_low_g_threshold_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_low_g_threshold(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_low_g_threshold_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoint(buf, 10, &data);
	if (error)
		return error;
	if (bma2x2_set_low_g_threshold(bma2x2->bma2x2_client, (unsigned
					char)data) < 0)
		return -EINVAL;

	return count;
}
static ssize_t bma2x2_tap_threshold_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_tap_threshold(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);
}

static ssize_t bma2x2_tap_threshold_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoint(buf, 10, &data);
	if (error)
		return error;
	if (bma2x2_set_tap_threshold(bma2x2->bma2x2_client, (unsigned char)data)
			< 0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_tap_duration_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_tap_duration(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);
}

static ssize_t bma2x2_tap_duration_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoint(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_tap_duration(bma2x2->bma2x2_client,
			(unsigned char)data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_tap_quiet_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_tap_quiet(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_tap_quiet_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoint(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_tap_quiet(bma2x2->bma2x2_client,
			(unsigned char)data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_tap_shock_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_tap_shock(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);
}

static ssize_t bma2x2_tap_shock_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoint(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_tap_shock(bma2x2->bma2x2_client,
			(unsigned char)data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_tap_samp_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_tap_samp(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);
}

static ssize_t bma2x2_tap_samp_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoint(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_tap_samp(bma2x2->bma2x2_client, (unsigned char)data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_orient_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_orient_mode(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);
}

static ssize_t bma2x2_orient_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoint(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_orient_mode(bma2x2->bma2x2_client,
			(unsigned char)data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_orient_blocking_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_orient_blocking(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);

}

static ssize_t bma2x2_orient_blocking_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoint(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_orient_blocking(bma2x2->bma2x2_client,
			(unsigned char)data) < 0)
		return -EINVAL;

	return count;
}
static ssize_t bma2x2_orient_hyst_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_orient_hyst(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);
}

static ssize_t bma2x2_orient_hyst_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoint(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_orient_hyst(bma2x2->bma2x2_client,
			(unsigned char)data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_orient_theta_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_theta_blocking(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);
}

static ssize_t bma2x2_orient_theta_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoint(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_theta_blocking(bma2x2->bma2x2_client,
			(unsigned char)data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_flat_theta_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_theta_flat(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);
}

static ssize_t bma2x2_flat_theta_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoint(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_theta_flat(bma2x2->bma2x2_client,
			(unsigned char)data) < 0)
		return -EINVAL;

	return count;
}
static ssize_t bma2x2_flat_hold_time_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_flat_hold_time(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);
}
static ssize_t bma2x2_selftest_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	return sprintf(buf, "%d\n", atomic_read(&bma2x2->selftest_result));
}

static ssize_t bma2x2_softreset_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_soft_reset(bma2x2->bma2x2_client) < 0)
		return -EINVAL;

	return count;
}
static ssize_t bma2x2_selftest_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int data;
	unsigned char clear_value = 0;
	int error;
	short value1 = 0;
	short value2 = 0;
	short diff = 0;
	unsigned long result = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	bma2x2_soft_reset(bma2x2->bma2x2_client);
	mdelay(5);

	error = kstrtoint(buf, 10, &data);
	if (error)
		return error;

	if (data != 1)
		return -EINVAL;

	bma2x2_write_reg(bma2x2->bma2x2_client, 0x32, &clear_value);

	if ((bma2x2->sensor_type == BMA280_TYPE) ||
			(bma2x2->sensor_type == BMA255_TYPE)) {
		/* set to 4 G range */
		if (bma2x2_set_range(bma2x2->bma2x2_client, RANGE_4G) < 0)
			return -EINVAL;
	}

	if ((bma2x2->sensor_type == BMA250E_TYPE) ||
			(bma2x2->sensor_type == BMA222E_TYPE)) {
		/* set to 8 G range */
		if (bma2x2_set_range(bma2x2->bma2x2_client, RANGE_8G) < 0)
			return -EINVAL;
		if (bma2x2_set_selftest_amp(bma2x2->bma2x2_client, 1) < 0)
			return -EINVAL;
	}
	/* 1 for x-axis*/
	bma2x2_set_selftest_st(bma2x2->bma2x2_client, 1);
	/* positive direction*/
	bma2x2_set_selftest_stn(bma2x2->bma2x2_client, 0);
	mdelay(10);
	bma2x2_read_accel_x(bma2x2->bma2x2_client,
				bma2x2->sensor_type, &value1);
	/* negative direction*/
	bma2x2_set_selftest_stn(bma2x2->bma2x2_client, 1);
	mdelay(10);
	bma2x2_read_accel_x(bma2x2->bma2x2_client,
				bma2x2->sensor_type, &value2);
	diff = value1-value2;

	pr_info("%s : diff x is %d,value1 is %d, value2 is %d\n", __func__,
			diff, value1, value2);

	if (bma2x2->sensor_type == BMA280_TYPE && abs(diff) < 1638)
			result |= 1;

	if (bma2x2->sensor_type == BMA255_TYPE && abs(diff) < 409)
			result |= 1;

	if (bma2x2->sensor_type == BMA250E_TYPE && abs(diff) < 51)
			result |= 1;

	if (bma2x2->sensor_type == BMA222E_TYPE && abs(diff) < 12)
			result |= 1;

	/* 2 for y-axis*/
	bma2x2_set_selftest_st(bma2x2->bma2x2_client, 2);
	/* positive direction*/
	bma2x2_set_selftest_stn(bma2x2->bma2x2_client, 0);
	mdelay(10);
	bma2x2_read_accel_y(bma2x2->bma2x2_client,
				bma2x2->sensor_type, &value1);
	/* negative direction*/
	bma2x2_set_selftest_stn(bma2x2->bma2x2_client, 1);
	mdelay(10);
	bma2x2_read_accel_y(bma2x2->bma2x2_client,
				bma2x2->sensor_type, &value2);
	diff = value1-value2;
	pr_info("%s : diff y is %d,value1 is %d, value2 is %d\n", __func__,
			diff, value1, value2);

	if (bma2x2->sensor_type == BMA280_TYPE) {
		if (abs(diff) < 1638)
			result |= 2;
	}
	if (bma2x2->sensor_type == BMA255_TYPE) {
		if (abs(diff) < 409)
			result |= 2;
	}
	if (bma2x2->sensor_type == BMA250E_TYPE) {
		if (abs(diff) < 51)
			result |= 2;
	}
	if (bma2x2->sensor_type == BMA222E_TYPE) {
		if (abs(diff) < 12)
			result |= 2;
	}

	/* 3 for z-axis*/
	bma2x2_set_selftest_st(bma2x2->bma2x2_client, 3);
	/* positive direction*/
	bma2x2_set_selftest_stn(bma2x2->bma2x2_client, 0);
	mdelay(10);
	bma2x2_read_accel_z(bma2x2->bma2x2_client,
				bma2x2->sensor_type, &value1);
	/* negative direction*/
	bma2x2_set_selftest_stn(bma2x2->bma2x2_client, 1);
	mdelay(10);
	bma2x2_read_accel_z(bma2x2->bma2x2_client,
				bma2x2->sensor_type, &value2);
	diff = value1-value2;

	pr_info("%s : diff z is %d,value1 is %d, value2 is %d\n", __func__,
			diff, value1, value2);

	if (bma2x2->sensor_type == BMA280_TYPE) {
		if (abs(diff) < 819)
			result |= 4;
	}
	if (bma2x2->sensor_type == BMA255_TYPE) {
		if (abs(diff) < 204)
			result |= 4;
	}
	if (bma2x2->sensor_type == BMA250E_TYPE) {
		if (abs(diff) < 25)
			result |= 4;
	}
	if (bma2x2->sensor_type == BMA222E_TYPE) {
		if (abs(diff) < 6)
			result |= 4;
	}

	/* self test for bma254 */
	if ((bma2x2->sensor_type == BMA255_TYPE) && (result > 0)) {
		result = 0;
		bma2x2_soft_reset(bma2x2->bma2x2_client);
		mdelay(5);
		bma2x2_write_reg(bma2x2->bma2x2_client, 0x32, &clear_value);
		/* set to 8 G range */
		if (bma2x2_set_range(bma2x2->bma2x2_client, RANGE_8G) < 0)
			return -EINVAL;
		if (bma2x2_set_selftest_amp(bma2x2->bma2x2_client, 1) < 0)
			return -EINVAL;
		/* 1 for x-axis*/
		bma2x2_set_selftest_st(bma2x2->bma2x2_client, 1);
		/* positive direction*/
		bma2x2_set_selftest_stn(bma2x2->bma2x2_client, 0);
		mdelay(10);
		bma2x2_read_accel_x(bma2x2->bma2x2_client,
						bma2x2->sensor_type, &value1);
		/* negative direction*/
		bma2x2_set_selftest_stn(bma2x2->bma2x2_client, 1);
		mdelay(10);
		bma2x2_read_accel_x(bma2x2->bma2x2_client,
						bma2x2->sensor_type, &value2);
		diff = value1-value2;

		pr_info("%s : diff x is %d,value1 is %d, value2 is %d\n",
				__func__, diff, value1, value2);
		if (abs(diff) < 204)
			result |= 1;
		/* 2 for y-axis*/
		bma2x2_set_selftest_st(bma2x2->bma2x2_client, 2);
		/* positive direction*/
		bma2x2_set_selftest_stn(bma2x2->bma2x2_client, 0);
		mdelay(10);
		bma2x2_read_accel_y(bma2x2->bma2x2_client,
						bma2x2->sensor_type, &value1);
		/* negative direction*/
		bma2x2_set_selftest_stn(bma2x2->bma2x2_client, 1);
		mdelay(10);
		bma2x2_read_accel_y(bma2x2->bma2x2_client,
						bma2x2->sensor_type, &value2);
		diff = value1-value2;
		pr_info("%s : diff y is %d,value1 is %d, value2 is %d\n",
				__func__, diff, value1, value2);

		if (abs(diff) < 204)
			result |= 2;
		/* 3 for z-axis*/
		bma2x2_set_selftest_st(bma2x2->bma2x2_client, 3);
		/* positive direction*/
		bma2x2_set_selftest_stn(bma2x2->bma2x2_client, 0);
		mdelay(10);
		bma2x2_read_accel_z(bma2x2->bma2x2_client,
						bma2x2->sensor_type, &value1);
		/* negative direction*/
		bma2x2_set_selftest_stn(bma2x2->bma2x2_client, 1);
		mdelay(10);
		bma2x2_read_accel_z(bma2x2->bma2x2_client,
						bma2x2->sensor_type, &value2);
		diff = value1-value2;

		pr_info("%s : diff z is %d,value1 is %d, value2 is %d\n",
				__func__, diff, value1, value2);
		if (abs(diff) < 102)
			result |= 4;
	}

	atomic_set(&bma2x2->selftest_result, (unsigned int)result);

	bma2x2_soft_reset(bma2x2->bma2x2_client);
	mdelay(5);
	pr_info("%s : self test finished\n", __func__);

	return count;
}

static ssize_t bma2x2_flat_hold_time_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoint(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_flat_hold_time(bma2x2->bma2x2_client, (unsigned
					char)data) < 0)
		return -EINVAL;

	return count;
}

static int bma2x2_read_accel_xyz(struct i2c_client *client,
		signed char sensor_type, struct bma2x2acc *acc)
{
	int comres;
	unsigned char data[6];
	struct bma2x2_data *client_data = i2c_get_clientdata(client);

#ifdef BMA2X2_SENSOR_IDENTIFICATION_ENABLE
	comres = bma2x2_smbus_read_byte_block(client,
				ACC_X12_LSB__REG, data, 6);
	acc->x = (data[1]<<8)|data[0];
	acc->y = (data[3]<<8)|data[2];
	acc->z = (data[5]<<8)|data[4];
#else
	switch (sensor_type) {
	case 0:
		comres = bma2x2_smbus_read_byte_block(client,
				ACC_X12_LSB__REG, data, 6);
		acc->x = GET_BITSLICE(data[0], ACC_X12_LSB)|
			(GET_BITSLICE(data[1],
				ACC_X_MSB)<<(ACC_X12_LSB__LEN));
		acc->x = acc->x << (sizeof(short)*8-(ACC_X12_LSB__LEN +
					ACC_X_MSB__LEN));
		acc->x = acc->x >> (sizeof(short)*8-(ACC_X12_LSB__LEN +
					ACC_X_MSB__LEN));

		acc->y = GET_BITSLICE(data[2], ACC_Y12_LSB)|
			(GET_BITSLICE(data[3],
				ACC_Y_MSB)<<(ACC_Y12_LSB__LEN
									));
		acc->y = acc->y << (sizeof(short)*8-(ACC_Y12_LSB__LEN +
					ACC_Y_MSB__LEN));
		acc->y = acc->y >> (sizeof(short)*8-(ACC_Y12_LSB__LEN +
					ACC_Y_MSB__LEN));

		acc->z = GET_BITSLICE(data[4], ACC_Z12_LSB)|
			(GET_BITSLICE(data[5],
				ACC_Z_MSB)<<(ACC_Z12_LSB__LEN));
		acc->z = acc->z << (sizeof(short)*8-(ACC_Z12_LSB__LEN +
					ACC_Z_MSB__LEN));
		acc->z = acc->z >> (sizeof(short)*8-(ACC_Z12_LSB__LEN +
					ACC_Z_MSB__LEN));
		break;
	case 1:
		comres = bma2x2_smbus_read_byte_block(client,
				ACC_X10_LSB__REG, data, 6);
		acc->x = GET_BITSLICE(data[0], ACC_X10_LSB)|
			(GET_BITSLICE(data[1],
				ACC_X_MSB)<<(ACC_X10_LSB__LEN));
		acc->x = acc->x << (sizeof(short)*8-(ACC_X10_LSB__LEN +
					ACC_X_MSB__LEN));
		acc->x = acc->x >> (sizeof(short)*8-(ACC_X10_LSB__LEN +
					ACC_X_MSB__LEN));

		acc->y = GET_BITSLICE(data[2], ACC_Y10_LSB)|
			(GET_BITSLICE(data[3],
				ACC_Y_MSB)<<(ACC_Y10_LSB__LEN
									));
		acc->y = acc->y << (sizeof(short)*8-(ACC_Y10_LSB__LEN +
					ACC_Y_MSB__LEN));
		acc->y = acc->y >> (sizeof(short)*8-(ACC_Y10_LSB__LEN +
					ACC_Y_MSB__LEN));

		acc->z = GET_BITSLICE(data[4], ACC_Z10_LSB)|
			(GET_BITSLICE(data[5],
				ACC_Z_MSB)<<(ACC_Z10_LSB__LEN));
		acc->z = acc->z << (sizeof(short)*8-(ACC_Z10_LSB__LEN +
					ACC_Z_MSB__LEN));
		acc->z = acc->z >> (sizeof(short)*8-(ACC_Z10_LSB__LEN +
					ACC_Z_MSB__LEN));
		break;
	case 2:
		comres = bma2x2_smbus_read_byte_block(client,
				ACC_X8_LSB__REG, data, 6);
		acc->x = GET_BITSLICE(data[0], ACC_X8_LSB)|
			(GET_BITSLICE(data[1],
				ACC_X_MSB)<<(ACC_X8_LSB__LEN));
		acc->x = acc->x << (sizeof(short)*8-(ACC_X8_LSB__LEN +
					ACC_X_MSB__LEN));
		acc->x = acc->x >> (sizeof(short)*8-(ACC_X8_LSB__LEN +
					ACC_X_MSB__LEN));

		acc->y = GET_BITSLICE(data[2], ACC_Y8_LSB)|
			(GET_BITSLICE(data[3],
				ACC_Y_MSB)<<(ACC_Y8_LSB__LEN
									));
		acc->y = acc->y << (sizeof(short)*8-(ACC_Y8_LSB__LEN +
					ACC_Y_MSB__LEN));
		acc->y = acc->y >> (sizeof(short)*8-(ACC_Y8_LSB__LEN +
					ACC_Y_MSB__LEN));

		acc->z = GET_BITSLICE(data[4], ACC_Z8_LSB)|
			(GET_BITSLICE(data[5],
				ACC_Z_MSB)<<(ACC_Z8_LSB__LEN));
		acc->z = acc->z << (sizeof(short)*8-(ACC_Z8_LSB__LEN +
					ACC_Z_MSB__LEN));
		acc->z = acc->z >> (sizeof(short)*8-(ACC_Z8_LSB__LEN +
					ACC_Z_MSB__LEN));
		break;
	case 3:
		comres = bma2x2_smbus_read_byte_block(client,
				ACC_X14_LSB__REG, data, 6);
		acc->x = GET_BITSLICE(data[0], ACC_X14_LSB)|
			(GET_BITSLICE(data[1],
				ACC_X_MSB)<<(ACC_X14_LSB__LEN));
		acc->x = acc->x << (sizeof(short)*8-(ACC_X14_LSB__LEN +
					ACC_X_MSB__LEN));
		acc->x = acc->x >> (sizeof(short)*8-(ACC_X14_LSB__LEN +
					ACC_X_MSB__LEN));

		acc->y = GET_BITSLICE(data[2], ACC_Y14_LSB)|
			(GET_BITSLICE(data[3],
				ACC_Y_MSB)<<(ACC_Y14_LSB__LEN
									));
		acc->y = acc->y << (sizeof(short)*8-(ACC_Y14_LSB__LEN +
					ACC_Y_MSB__LEN));
		acc->y = acc->y >> (sizeof(short)*8-(ACC_Y14_LSB__LEN +
					ACC_Y_MSB__LEN));

		acc->z = GET_BITSLICE(data[4], ACC_Z14_LSB)|
			(GET_BITSLICE(data[5],
				ACC_Z_MSB)<<(ACC_Z14_LSB__LEN));
		acc->z = acc->z << (sizeof(short)*8-(ACC_Z14_LSB__LEN +
					ACC_Z_MSB__LEN));
		acc->z = acc->z >> (sizeof(short)*8-(ACC_Z14_LSB__LEN +
					ACC_Z_MSB__LEN));
		break;
	default:
		break;
	}
#endif
	bma2x2_remap_sensor_data(acc, client_data);
	return comres;
}

static int bma2x2_raw_accel_xyz(struct i2c_client *client,
		signed char sensor_type, struct bma2x2acc *acc)
{
	int comres;
	unsigned char data[6];
	struct bma2x2_data *client_data = i2c_get_clientdata(client);

	switch (sensor_type) {
	case 0:
		comres = bma2x2_smbus_read_byte_block(client,
				ACC_X12_LSB__REG, data, 6);
		acc->x = GET_BITSLICE(data[0], ACC_X12_LSB)|
			(GET_BITSLICE(data[1],
				ACC_X_MSB)<<(ACC_X12_LSB__LEN));
		acc->x = acc->x << (sizeof(short)*8-(ACC_X12_LSB__LEN +
					ACC_X_MSB__LEN));
		acc->x = acc->x >> (sizeof(short)*8-(ACC_X12_LSB__LEN +
					ACC_X_MSB__LEN));

		acc->y = GET_BITSLICE(data[2], ACC_Y12_LSB)|
			(GET_BITSLICE(data[3],
				ACC_Y_MSB)<<(ACC_Y12_LSB__LEN
									));
		acc->y = acc->y << (sizeof(short)*8-(ACC_Y12_LSB__LEN +
					ACC_Y_MSB__LEN));
		acc->y = acc->y >> (sizeof(short)*8-(ACC_Y12_LSB__LEN +
					ACC_Y_MSB__LEN));

		acc->z = GET_BITSLICE(data[4], ACC_Z12_LSB)|
			(GET_BITSLICE(data[5],
				ACC_Z_MSB)<<(ACC_Z12_LSB__LEN));
		acc->z = acc->z << (sizeof(short)*8-(ACC_Z12_LSB__LEN +
					ACC_Z_MSB__LEN));
		acc->z = acc->z >> (sizeof(short)*8-(ACC_Z12_LSB__LEN +
					ACC_Z_MSB__LEN));
		break;
	case 1:
		comres = bma2x2_smbus_read_byte_block(client,
				ACC_X10_LSB__REG, data, 6);
		acc->x = GET_BITSLICE(data[0], ACC_X10_LSB)|
			(GET_BITSLICE(data[1],
				ACC_X_MSB)<<(ACC_X10_LSB__LEN));
		acc->x = acc->x << (sizeof(short)*8-(ACC_X10_LSB__LEN +
					ACC_X_MSB__LEN));
		acc->x = acc->x >> (sizeof(short)*8-(ACC_X10_LSB__LEN +
					ACC_X_MSB__LEN));

		acc->y = GET_BITSLICE(data[2], ACC_Y10_LSB)|
			(GET_BITSLICE(data[3],
				ACC_Y_MSB)<<(ACC_Y10_LSB__LEN
									));
		acc->y = acc->y << (sizeof(short)*8-(ACC_Y10_LSB__LEN +
					ACC_Y_MSB__LEN));
		acc->y = acc->y >> (sizeof(short)*8-(ACC_Y10_LSB__LEN +
					ACC_Y_MSB__LEN));

		acc->z = GET_BITSLICE(data[4], ACC_Z10_LSB)|
			(GET_BITSLICE(data[5],
				ACC_Z_MSB)<<(ACC_Z10_LSB__LEN));
		acc->z = acc->z << (sizeof(short)*8-(ACC_Z10_LSB__LEN +
					ACC_Z_MSB__LEN));
		acc->z = acc->z >> (sizeof(short)*8-(ACC_Z10_LSB__LEN +
					ACC_Z_MSB__LEN));
		break;
	case 2:
		comres = bma2x2_smbus_read_byte_block(client,
				ACC_X8_LSB__REG, data, 6);
		acc->x = GET_BITSLICE(data[0], ACC_X8_LSB)|
			(GET_BITSLICE(data[1],
				ACC_X_MSB)<<(ACC_X8_LSB__LEN));
		acc->x = acc->x << (sizeof(short)*8-(ACC_X8_LSB__LEN +
					ACC_X_MSB__LEN));
		acc->x = acc->x >> (sizeof(short)*8-(ACC_X8_LSB__LEN +
					ACC_X_MSB__LEN));

		acc->y = GET_BITSLICE(data[2], ACC_Y8_LSB)|
			(GET_BITSLICE(data[3],
				ACC_Y_MSB)<<(ACC_Y8_LSB__LEN
									));
		acc->y = acc->y << (sizeof(short)*8-(ACC_Y8_LSB__LEN +
					ACC_Y_MSB__LEN));
		acc->y = acc->y >> (sizeof(short)*8-(ACC_Y8_LSB__LEN +
					ACC_Y_MSB__LEN));

		acc->z = GET_BITSLICE(data[4], ACC_Z8_LSB)|
			(GET_BITSLICE(data[5],
				ACC_Z_MSB)<<(ACC_Z8_LSB__LEN));
		acc->z = acc->z << (sizeof(short)*8-(ACC_Z8_LSB__LEN +
					ACC_Z_MSB__LEN));
		acc->z = acc->z >> (sizeof(short)*8-(ACC_Z8_LSB__LEN +
					ACC_Z_MSB__LEN));
		break;
	case 3:
		comres = bma2x2_smbus_read_byte_block(client,
				ACC_X14_LSB__REG, data, 6);
		acc->x = GET_BITSLICE(data[0], ACC_X14_LSB)|
			(GET_BITSLICE(data[1],
				ACC_X_MSB)<<(ACC_X14_LSB__LEN));
		acc->x = acc->x << (sizeof(short)*8-(ACC_X14_LSB__LEN +
					ACC_X_MSB__LEN));
		acc->x = acc->x >> (sizeof(short)*8-(ACC_X14_LSB__LEN +
					ACC_X_MSB__LEN));

		acc->y = GET_BITSLICE(data[2], ACC_Y14_LSB)|
			(GET_BITSLICE(data[3],
				ACC_Y_MSB)<<(ACC_Y14_LSB__LEN
									));
		acc->y = acc->y << (sizeof(short)*8-(ACC_Y14_LSB__LEN +
					ACC_Y_MSB__LEN));
		acc->y = acc->y >> (sizeof(short)*8-(ACC_Y14_LSB__LEN +
					ACC_Y_MSB__LEN));

		acc->z = GET_BITSLICE(data[4], ACC_Z14_LSB)|
			(GET_BITSLICE(data[5],
				ACC_Z_MSB)<<(ACC_Z14_LSB__LEN));
		acc->z = acc->z << (sizeof(short)*8-(ACC_Z14_LSB__LEN +
					ACC_Z_MSB__LEN));
		acc->z = acc->z >> (sizeof(short)*8-(ACC_Z14_LSB__LEN +
					ACC_Z_MSB__LEN));
		break;
	default:
		comres = -1;
		break;
	}

	bma2x2_remap_sensor_data(acc, client_data);
	return comres;
}


static void bma2x2_work_func(struct work_struct *work)
{
	struct bma2x2_data *bma2x2 = container_of((struct delayed_work *)work,
			struct bma2x2_data, work);
	static struct bma2x2acc acc;
	unsigned long delay = msecs_to_jiffies(atomic_read(&bma2x2->delay));

	bma2x2_read_accel_xyz(bma2x2->bma2x2_client,
				bma2x2->sensor_type, &acc);
	input_report_abs(bma2x2->input, ABS_X, acc.x);
	input_report_abs(bma2x2->input, ABS_Y, acc.y);
	input_report_abs(bma2x2->input, ABS_Z, acc.z);
	input_sync(bma2x2->input);

	mutex_lock(&bma2x2->value_mutex);
	bma2x2->value.x = acc.x;
	bma2x2->value.y = acc.y;
	bma2x2->value.z = acc.z;
	mutex_unlock(&bma2x2->value_mutex);

	schedule_delayed_work(&bma2x2->work, delay);
}

static ssize_t bma2x2_register_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int address, value;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	sscanf(buf, "%d%d", &address, &value);
	if (bma2x2_write_reg(bma2x2->bma2x2_client, (unsigned char)address,
				(unsigned char *)&value) < 0)
		return -EINVAL;
	return count;
}

static ssize_t bma2x2_register_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{

	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	size_t count = 0;
	u8 reg[0x40];
	int i;

	for (i = 0; i < 0x40; i++) {
		bma2x2_smbus_read_byte(bma2x2->bma2x2_client, i, reg+i);

		count += sprintf(&buf[count], "0x%x: %d\n", i, reg[i]);
	}
	return count;
}

static ssize_t bma2x2_range_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_range(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);
}

static ssize_t bma2x2_range_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoint(buf, 10, &data);
	if (error)
		return error;
	if (bma2x2_set_range(bma2x2->bma2x2_client, (unsigned char) data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_bandwidth_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_bandwidth(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);
}

static ssize_t bma2x2_bandwidth_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoint(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2->sensor_type == BMA280_TYPE)
		if ((unsigned char) data > 14)
			return -EINVAL;

	if (bma2x2_set_bandwidth(bma2x2->bma2x2_client,
				(unsigned char) data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_mode(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);
}

static ssize_t bma2x2_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoint(buf, 10, &data);
	if (error)
		return error;
	if (bma2x2_set_mode(bma2x2->bma2x2_client, (unsigned char) data) < 0)
		return -EINVAL;

	return count;
}
static ssize_t bma2x2_value_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bma2x2_data *bma2x2 = input_get_drvdata(input);
	struct bma2x2acc acc_value;

#if 0
	bma2x2_read_accel_xyz(bma2x2->bma2x2_client, bma2x2->sensor_type,
								&acc_value);
#else
	/* cache of last input event */
	mutex_lock(&bma2x2->value_mutex);
	acc_value.x = bma2x2->value.x;
	acc_value.y = bma2x2->value.y;
	acc_value.z = bma2x2->value.z;
	mutex_unlock(&bma2x2->value_mutex);
#endif
	return sprintf(buf, "%d %d %d\n", acc_value.x, acc_value.y,
			acc_value.z);
}
/* raw-data for sensor test */
static ssize_t bma2x2_raw_data_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bma2x2_data *bma2x2 = input_get_drvdata(input);
	struct bma2x2acc acc_value;

	bma2x2_raw_accel_xyz(bma2x2->bma2x2_client, bma2x2->sensor_type,
								&acc_value);
	return sprintf(buf, "%d,%d,%d\n", acc_value.x, acc_value.y,
			acc_value.z);
}
/* raw-data for sensor test graph*/
static ssize_t bma2x2_raw_value_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct bma2x2_data *bma2x2 = input_get_drvdata(input);
	struct bma2x2acc acc_value;

	bma2x2_read_accel_xyz(bma2x2->bma2x2_client, bma2x2->sensor_type,
								&acc_value);

	return sprintf(buf, "%d,%d,%d\n", acc_value.x, acc_value.y,
			acc_value.z);
}

static ssize_t bma2x2_delay_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	return sprintf(buf, "%d\n", atomic_read(&bma2x2->delay));
}

static ssize_t bma2x2_chip_id_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	return sprintf(buf, "%d\n", bma2x2->chip_id);
}

static ssize_t bma2x2_place_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
#ifdef BMA_USE_PLATFORM_DATA
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);
#endif
	int place = BOSCH_SENSOR_PLACE_UNKNOWN;

#ifdef BMA_USE_PLATFORM_DATA
	if (NULL != bma2x2->bst_pd)
		place = bma2x2->bst_pd->place;
#endif
	return sprintf(buf, "%d\n", place);
}

static ssize_t bma2x2_delay_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoint(buf, 10, &data);
	if (error)
		return error;
	if (data > MAX_DELAY)
		data = MAX_DELAY;
	atomic_set(&bma2x2->delay, (unsigned int) data);

	return count;
}

static ssize_t bma2x2_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	return sprintf(buf, "%d\n", atomic_read(&bma2x2->enable));
}

static void bma2x2_set_enable(struct device *dev, int enable)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);
	int pre_enable = atomic_read(&bma2x2->enable);

	mutex_lock(&bma2x2->enable_mutex);
	if (enable) {
		if (pre_enable == 0) {
			bma2x2_set_mode(bma2x2->bma2x2_client,
					MODE_NORMAL);
			bma2x2_open_cal(client);
			schedule_delayed_work(&bma2x2->work,
				msecs_to_jiffies(atomic_read(&bma2x2->delay)));
			atomic_set(&bma2x2->enable, 1);
		}
	} else {
		if (pre_enable == 1) {
			bma2x2_set_mode(bma2x2->bma2x2_client, MODE_SUSPEND);
			cancel_delayed_work_sync(&bma2x2->work);
			atomic_set(&bma2x2->enable, 0);
		}
	}
	mutex_unlock(&bma2x2->enable_mutex);
}

static ssize_t bma2x2_enable_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int data;
	int error;

	error = kstrtoint(buf, 10, &data);
	if (error)
		return error;
	if ((data == 0) || (data == 1))
		bma2x2_set_enable(dev, data);

	return count;
}
static ssize_t bma2x2_fast_calibration_x_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_offset_target(bma2x2->bma2x2_client,
			OFFSET_TRIGGER_X, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);
}

static ssize_t bma2x2_fast_calibration_x_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int data;
	signed char tmp;
	unsigned char timeout = 0;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoint(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_offset_target(bma2x2->bma2x2_client,
			OFFSET_TRIGGER_X, (unsigned char)data) < 0)
		return -EINVAL;

	if (bma2x2_set_cal_trigger(bma2x2->bma2x2_client,
			OFFSET_TRIGGER_X) < 0)
		return -EINVAL;

	do {
		mdelay(2);
		bma2x2_get_cal_ready(bma2x2->bma2x2_client, &tmp);

/*		pr_info("wait 2ms cal ready flag is %d\n", tmp);
 */
		timeout++;
		if (timeout == 50) {
			pr_info("%s : get fast calibration ready error\n",
					__func__);
			return -EINVAL;
		};

	} while (tmp == 0);

	pr_info("%s : x axis fast calibration finished\n", __func__);
	return count;
}

static ssize_t bma2x2_fast_calibration_y_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_offset_target(bma2x2->bma2x2_client,
			OFFSET_TRIGGER_Y, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);
}

static ssize_t bma2x2_fast_calibration_y_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int data;
	signed char tmp;
	unsigned char timeout = 0;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoint(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_offset_target(bma2x2->bma2x2_client,
			OFFSET_TRIGGER_Y, (unsigned char)data) < 0)
		return -EINVAL;

	if (bma2x2_set_cal_trigger(bma2x2->bma2x2_client,
			OFFSET_TRIGGER_Y) < 0)
		return -EINVAL;

	do {
		mdelay(2);
		bma2x2_get_cal_ready(bma2x2->bma2x2_client, &tmp);

/*		pr_info("wait 2ms cal ready flag is %d\n", tmp);
 */
		timeout++;
		if (timeout == 50) {
			pr_info("%s : get fast calibration ready error\n",
					__func__);
			return -EINVAL;
		};

	} while (tmp == 0);

	pr_info("%s : y axis fast calibration finished\n", __func__);
	return count;
}

static ssize_t bma2x2_fast_calibration_z_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_offset_target(bma2x2->bma2x2_client,
			OFFSET_TRIGGER_Z, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);
}

static ssize_t bma2x2_fast_calibration_z_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int data;
	signed char tmp;
	unsigned char timeout = 0;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoint(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_offset_target(bma2x2->bma2x2_client,
			OFFSET_TRIGGER_Z, (unsigned char)data) < 0)
		return -EINVAL;

	if (bma2x2_set_cal_trigger(bma2x2->bma2x2_client,
			OFFSET_TRIGGER_Z) < 0)
		return -EINVAL;

	do {
		mdelay(2);
		bma2x2_get_cal_ready(bma2x2->bma2x2_client, &tmp);

/*		pr_info("wait 2ms cal ready flag is %d\n", tmp);
 */
		timeout++;
		if (timeout == 50) {
			pr_info("%s : get fast calibration ready error\n",
					__func__);
			return -EINVAL;
		};

	} while (tmp == 0);

	pr_info("%s : z axis fast calibration finished\n", __func__);
	return count;
}

static ssize_t bma2x2_calibration_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int cal_data[3];
	int err;
	mm_segment_t old_fs;
	struct file *cal_filp = NULL;
	int result = 1;

	pr_info("%s", __func__);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(CALIBRATION_FILE_PATH,
		O_RDONLY, S_IRUGO | S_IWUSR | S_IWGRP);
	if (IS_ERR(cal_filp)) {
		pr_err("%s :  Can't open calibration file\n",
			__func__);
		set_fs(old_fs);
		err = PTR_ERR(cal_filp);
		return err;
	}
	err = cal_filp->f_op->read(cal_filp,
		(char *)cal_data,
		3 * sizeof(int), &cal_filp->f_pos);
	if (err != 3 * sizeof(int)) {
		pr_err("%s :  Can't read the cal data from file\n",
			__func__);
		filp_close(cal_filp, current->files);
		set_fs(old_fs);
		return -EIO;
	}
	if (cal_data[0] == 0 && cal_data[1] == 0 && cal_data[2] == 0)
		result = 0;

	return sprintf(buf, "%d %d %d %d\n", result,
			cal_data[0], cal_data[1], cal_data[2]);
}

static ssize_t bma2x2_calibration_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	int cal_data[3];
	char offset_data[3];
	int data;
	int polarity;
	signed char tmp;
	unsigned char timeout = 0;
	int err;
	mm_segment_t old_fs;
	struct file *cal_filp = NULL;
	struct bma2x2acc acc_value;

	bma2x2_raw_accel_xyz(bma2x2->bma2x2_client, bma2x2->sensor_type,
								&acc_value);
	pr_info("%s : %d %d %d\n", __func__, acc_value.x, acc_value.y, acc_value.z);

	err = kstrtoint(buf, 10, &data);
	if (err)
		return err;

	if (data) {
		/* x axis fast calibration */
		if (bma2x2_set_offset_target(bma2x2->bma2x2_client,
				OFFSET_TRIGGER_X, OFFSET_0G) < 0)
			return -EINVAL;

		if (bma2x2_set_cal_trigger(bma2x2->bma2x2_client,
				OFFSET_TRIGGER_X) < 0)
			return -EINVAL;

		do {
			mdelay(2);
			bma2x2_get_cal_ready(bma2x2->bma2x2_client, &tmp);
			timeout++;
			if (timeout == 50) {
				pr_info("get fast calibration ready error\n");
				return -EINVAL;
			};
		} while (tmp == 0);

		/* y axis fast calibration */
		if (bma2x2_set_offset_target(bma2x2->bma2x2_client,
				OFFSET_TRIGGER_Y, OFFSET_0G) < 0)
			return -EINVAL;

		if (bma2x2_set_cal_trigger(bma2x2->bma2x2_client,
				OFFSET_TRIGGER_Y) < 0)
			return -EINVAL;

		do {
			mdelay(2);
			bma2x2_get_cal_ready(bma2x2->bma2x2_client, &tmp);
			timeout++;
			if (timeout == 50) {
				pr_info("get fast calibration ready error\n");
				return -EINVAL;
			};
		} while (tmp == 0);

		if (acc_value.z > 0)
			polarity = OFFSET_M1G;
		else
			polarity = OFFSET_P1G;

		/* z axis fast calibration */
		if (bma2x2_set_offset_target(bma2x2->bma2x2_client,
				OFFSET_TRIGGER_Z, polarity) < 0)
			return -EINVAL;

		if (bma2x2_set_cal_trigger(bma2x2->bma2x2_client,
				OFFSET_TRIGGER_Z) < 0)
			return -EINVAL;

		do {
			mdelay(2);
			bma2x2_get_cal_ready(bma2x2->bma2x2_client, &tmp);
			timeout++;
			if (timeout == 50) {
				pr_info("get fast calibration ready error\n");
				return -EINVAL;
			};
		} while (tmp == 0);

		/* calibration */
		bma2x2_get_offset_x(bma2x2->bma2x2_client,
				(unsigned char *)&offset_data[0]);
		bma2x2_get_offset_y(bma2x2->bma2x2_client,
				(unsigned char *)&offset_data[1]);
		bma2x2_get_offset_z(bma2x2->bma2x2_client,
				(unsigned char *)&offset_data[2]);

		cal_data[0] = (signed char)offset_data[0];
		cal_data[1] = (signed char)offset_data[1];
		cal_data[2] = (signed char)offset_data[2];

		pr_info("%s : (%d,%d,%d)", __func__,
			cal_data[0], cal_data[1], cal_data[2]);

		old_fs = get_fs();
		set_fs(KERNEL_DS);
		cal_filp = filp_open(CALIBRATION_FILE_PATH,
			O_CREAT | O_TRUNC | O_WRONLY | O_SYNC,
			S_IRUGO | S_IWUSR | S_IWGRP);
		if (IS_ERR(cal_filp)) {
			pr_err("%s :  Can't open calibration file\n",
				__func__);
			set_fs(old_fs);
			err = PTR_ERR(cal_filp);
			return err;
		}

		err = cal_filp->f_op->write(cal_filp,
			(char *)cal_data,
			3 * sizeof(int), &cal_filp->f_pos);
		if (err != 3 * sizeof(int)) {
			pr_err("%s :  Can't write the cal data to file\n",
				__func__);
			err = -EIO;
		}
		filp_close(cal_filp, current->files);
		set_fs(old_fs);
	} else {
		/* erase cal data */
		old_fs = get_fs();
		set_fs(KERNEL_DS);
		cal_filp = filp_open(CALIBRATION_FILE_PATH,
			O_CREAT | O_TRUNC | O_WRONLY | O_SYNC,
			S_IRUGO | S_IWUSR | S_IWGRP);
		if (IS_ERR(cal_filp)) {
			pr_err("%s :  Can't open calibration file\n",
				__func__);
			set_fs(old_fs);
			err = PTR_ERR(cal_filp);
			return err;
		}
		cal_data[0] = 0;
		cal_data[1] = 0;
		cal_data[2] = 0;

		err = cal_filp->f_op->write(cal_filp,
			(char *)cal_data,
			3 * sizeof(int), &cal_filp->f_pos);
		if (err != 3 * sizeof(int)) {
			pr_err("%s :  Can't write the cal data to file\n",
				__func__);
			err = -EIO;
		}
		filp_close(cal_filp, current->files);
		set_fs(old_fs);

		bma2x2_set_offset_x(bma2x2->bma2x2_client,
				(unsigned char)cal_data[0]);
		bma2x2_set_offset_y(bma2x2->bma2x2_client,
				(unsigned char)cal_data[1]);
		bma2x2_set_offset_z(bma2x2->bma2x2_client,
				(unsigned char)cal_data[2]);
	}

	return count;
}

static ssize_t bma2x2_SleepDur_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_sleep_duration(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);
}

static ssize_t bma2x2_SleepDur_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoint(buf, 10, &data);
	if (error)
		return error;
	if (bma2x2_set_sleep_duration(bma2x2->bma2x2_client,
				(unsigned char) data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_fifo_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_fifo_mode(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);
}

static ssize_t bma2x2_fifo_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoint(buf, 10, &data);
	if (error)
		return error;
	if (bma2x2_set_fifo_mode(bma2x2->bma2x2_client,
				(unsigned char) data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_fifo_trig_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_fifo_trig(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);
}

static ssize_t bma2x2_fifo_trig_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoint(buf, 10, &data);
	if (error)
		return error;
	if (bma2x2_set_fifo_trig(bma2x2->bma2x2_client,
				(unsigned char) data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_fifo_trig_src_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_fifo_trig_src(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);
}

static ssize_t bma2x2_fifo_trig_src_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoint(buf, 10, &data);
	if (error)
		return error;
	if (bma2x2_set_fifo_trig_src(bma2x2->bma2x2_client,
				(unsigned char) data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_fifo_data_sel_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_fifo_data_sel(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);
}

static ssize_t bma2x2_fifo_framecount_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_fifo_framecount(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);
}

static ssize_t bma2x2_temperature_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_read_temperature(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);
}

static ssize_t bma2x2_fifo_data_sel_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoint(buf, 10, &data);
	if (error)
		return error;
	if (bma2x2_set_fifo_data_sel(bma2x2->bma2x2_client,
				(unsigned char) data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_fifo_data_out_frame_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	int err, i, len;
	signed char fifo_data_out[MAX_FIFO_F_LEVEL * MAX_FIFO_F_BYTES] = {0};
	unsigned char f_count, f_len = 0;
	unsigned char fifo_datasel = 0;

	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_fifo_data_sel(bma2x2->bma2x2_client, &fifo_datasel) < 0)
		return sprintf(buf, "Read data sel error\n");

	if (fifo_datasel)
		f_len = 2;
	else
		f_len = 6;

	if (bma2x2_get_fifo_framecount(bma2x2->bma2x2_client, &f_count) < 0)
		return sprintf(buf, "Read frame count error\n");

	if (bma_i2c_burst_read(bma2x2->bma2x2_client,
			FIFO_DATA_OUTPUT_REG, fifo_data_out,
							f_count * f_len) < 0)
		return sprintf(buf, "Read byte block error\n");

	if (bma2x2_get_fifo_data_out_reg(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	err = 0;

	len = sprintf(buf, "%lu ", jiffies);
	buf += len;
	err += len;

	len = sprintf(buf, "%u ", f_count);
	buf += len;
	err += len;

	len = sprintf(buf, "%u ", f_len);
	buf += len;
	err += len;

	for (i = 0; i < f_count * f_len; i++)	{
		len = sprintf(buf, "%d ", fifo_data_out[i]);
		buf += len;
		err += len;
	}

	return err;
}

static ssize_t bma2x2_offset_x_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_offset_x(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);
}

static ssize_t bma2x2_offset_x_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoint(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_offset_x(bma2x2->bma2x2_client,
				(unsigned char)data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_offset_y_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_offset_y(bma2x2->bma2x2_client, &data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);
}

static ssize_t bma2x2_offset_y_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoint(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_offset_y(bma2x2->bma2x2_client,
				(unsigned char)data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_offset_z_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	if (bma2x2_get_offset_z(bma2x2->bma2x2_client,
				(unsigned char *)&data) < 0)
		return sprintf(buf, "Read error\n");

	return sprintf(buf, "%d\n", data);
}

static ssize_t bma2x2_offset_z_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int data;
	int error;
	struct i2c_client *client = to_i2c_client(dev);
	struct bma2x2_data *bma2x2 = i2c_get_clientdata(client);

	error = kstrtoint(buf, 10, &data);
	if (error)
		return error;

	if (bma2x2_set_offset_z(bma2x2->bma2x2_client,
				(unsigned char)data) < 0)
		return -EINVAL;

	return count;
}

static ssize_t bma2x2_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", VENDOR);
}

static ssize_t bma2x2_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s030\n", CHIP_ID);
}

static DEVICE_ATTR(range, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_range_show, bma2x2_range_store);
static DEVICE_ATTR(bandwidth, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_bandwidth_show, bma2x2_bandwidth_store);
static DEVICE_ATTR(mode, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_mode_show, bma2x2_mode_store);
static DEVICE_ATTR(value, S_IRUGO,
		bma2x2_value_show, NULL);
static DEVICE_ATTR(raw_data, S_IRUGO,
		bma2x2_raw_data_show, NULL);
static DEVICE_ATTR(raw_value, S_IRUGO,
		bma2x2_raw_value_show, NULL);
static DEVICE_ATTR(delay, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_delay_show, bma2x2_delay_store);
static DEVICE_ATTR(enable, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_enable_show, bma2x2_enable_store);
static DEVICE_ATTR(SleepDur, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_SleepDur_show, bma2x2_SleepDur_store);
static DEVICE_ATTR(fast_calibration_x, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_fast_calibration_x_show,
		bma2x2_fast_calibration_x_store);
static DEVICE_ATTR(fast_calibration_y, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_fast_calibration_y_show,
		bma2x2_fast_calibration_y_store);
static DEVICE_ATTR(fast_calibration_z, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_fast_calibration_z_show,
		bma2x2_fast_calibration_z_store);
static DEVICE_ATTR(fifo_mode, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_fifo_mode_show, bma2x2_fifo_mode_store);
static DEVICE_ATTR(fifo_framecount, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_fifo_framecount_show, NULL);
static DEVICE_ATTR(fifo_trig, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_fifo_trig_show, bma2x2_fifo_trig_store);
static DEVICE_ATTR(fifo_trig_src, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_fifo_trig_src_show, bma2x2_fifo_trig_src_store);
static DEVICE_ATTR(fifo_data_sel, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_fifo_data_sel_show, bma2x2_fifo_data_sel_store);
static DEVICE_ATTR(fifo_data_out_frame, S_IRUGO,
		bma2x2_fifo_data_out_frame_show, NULL);
static DEVICE_ATTR(reg, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_register_show, bma2x2_register_store);
static DEVICE_ATTR(chip_id, S_IRUGO,
		bma2x2_chip_id_show, NULL);
static DEVICE_ATTR(offset_x, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_offset_x_show,
		bma2x2_offset_x_store);
static DEVICE_ATTR(offset_y, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_offset_y_show,
		bma2x2_offset_y_store);
static DEVICE_ATTR(offset_z, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_offset_z_show,
		bma2x2_offset_z_store);
static DEVICE_ATTR(enable_int, S_IWUSR|S_IWGRP,
		NULL, bma2x2_enable_int_store);
static DEVICE_ATTR(int_mode, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_int_mode_show, bma2x2_int_mode_store);
static DEVICE_ATTR(slope_duration, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_slope_duration_show, bma2x2_slope_duration_store);
static DEVICE_ATTR(slope_threshold, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_slope_threshold_show, bma2x2_slope_threshold_store);
static DEVICE_ATTR(slope_no_mot_duration, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_slope_no_mot_duration_show,
			bma2x2_slope_no_mot_duration_store);
static DEVICE_ATTR(slope_no_mot_threshold, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_slope_no_mot_threshold_show,
			bma2x2_slope_no_mot_threshold_store);
static DEVICE_ATTR(high_g_duration, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_high_g_duration_show, bma2x2_high_g_duration_store);
static DEVICE_ATTR(high_g_threshold, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_high_g_threshold_show, bma2x2_high_g_threshold_store);
static DEVICE_ATTR(low_g_duration, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_low_g_duration_show, bma2x2_low_g_duration_store);
static DEVICE_ATTR(low_g_threshold, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_low_g_threshold_show, bma2x2_low_g_threshold_store);
static DEVICE_ATTR(tap_duration, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_tap_duration_show, bma2x2_tap_duration_store);
static DEVICE_ATTR(tap_threshold, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_tap_threshold_show, bma2x2_tap_threshold_store);
static DEVICE_ATTR(tap_quiet, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_tap_quiet_show, bma2x2_tap_quiet_store);
static DEVICE_ATTR(tap_shock, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_tap_shock_show, bma2x2_tap_shock_store);
static DEVICE_ATTR(tap_samp, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_tap_samp_show, bma2x2_tap_samp_store);
static DEVICE_ATTR(orient_mode, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_orient_mode_show, bma2x2_orient_mode_store);
static DEVICE_ATTR(orient_blocking, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_orient_blocking_show, bma2x2_orient_blocking_store);
static DEVICE_ATTR(orient_hyst, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_orient_hyst_show, bma2x2_orient_hyst_store);
static DEVICE_ATTR(orient_theta, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_orient_theta_show, bma2x2_orient_theta_store);
static DEVICE_ATTR(flat_theta, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_flat_theta_show, bma2x2_flat_theta_store);
static DEVICE_ATTR(flat_hold_time, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_flat_hold_time_show, bma2x2_flat_hold_time_store);
static DEVICE_ATTR(selftest, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_selftest_show, bma2x2_selftest_store);
static DEVICE_ATTR(softreset, S_IWUSR|S_IWGRP,
		NULL, bma2x2_softreset_store);
static DEVICE_ATTR(temperature, S_IRUGO,
		bma2x2_temperature_show, NULL);
static DEVICE_ATTR(place, S_IRUGO,
		bma2x2_place_show, NULL);
static DEVICE_ATTR(calibration, S_IRUGO|S_IWUSR|S_IWGRP,
		bma2x2_calibration_show,
		bma2x2_calibration_store);
static DEVICE_ATTR(vendor, S_IRUGO,
		bma2x2_vendor_show, NULL);
static DEVICE_ATTR(name, S_IRUGO,
		bma2x2_name_show, NULL);

static struct attribute *bma2x2_attributes[] = {
	&dev_attr_range.attr,
	&dev_attr_bandwidth.attr,
	&dev_attr_mode.attr,
	&dev_attr_raw_data.attr,
	&dev_attr_raw_value.attr,
	&dev_attr_value.attr,
	&dev_attr_delay.attr,
	&dev_attr_enable.attr,
	&dev_attr_SleepDur.attr,
	&dev_attr_reg.attr,
	&dev_attr_fast_calibration_x.attr,
	&dev_attr_fast_calibration_y.attr,
	&dev_attr_fast_calibration_z.attr,
	&dev_attr_fifo_mode.attr,
	&dev_attr_fifo_framecount.attr,
	&dev_attr_fifo_trig.attr,
	&dev_attr_fifo_trig_src.attr,
	&dev_attr_fifo_data_sel.attr,
	&dev_attr_fifo_data_out_frame.attr,
	&dev_attr_chip_id.attr,
	&dev_attr_offset_x.attr,
	&dev_attr_offset_y.attr,
	&dev_attr_offset_z.attr,
	&dev_attr_enable_int.attr,
	&dev_attr_int_mode.attr,
	&dev_attr_slope_duration.attr,
	&dev_attr_slope_threshold.attr,
	&dev_attr_slope_no_mot_duration.attr,
	&dev_attr_slope_no_mot_threshold.attr,
	&dev_attr_high_g_duration.attr,
	&dev_attr_high_g_threshold.attr,
	&dev_attr_low_g_duration.attr,
	&dev_attr_low_g_threshold.attr,
	&dev_attr_tap_threshold.attr,
	&dev_attr_tap_duration.attr,
	&dev_attr_tap_quiet.attr,
	&dev_attr_tap_shock.attr,
	&dev_attr_tap_samp.attr,
	&dev_attr_orient_mode.attr,
	&dev_attr_orient_blocking.attr,
	&dev_attr_orient_hyst.attr,
	&dev_attr_orient_theta.attr,
	&dev_attr_flat_theta.attr,
	&dev_attr_flat_hold_time.attr,
	&dev_attr_selftest.attr,
	&dev_attr_softreset.attr,
	&dev_attr_temperature.attr,
	&dev_attr_place.attr,
	&dev_attr_calibration.attr,
	NULL
};

static struct attribute_group bma2x2_attribute_group = {
	.attrs = bma2x2_attributes
};

static struct device_attribute *bma2x2_sysfs_attributes[] = {
	&dev_attr_mode,
	&dev_attr_raw_data,
	&dev_attr_raw_value,
	&dev_attr_delay,
	&dev_attr_enable,
	&dev_attr_selftest,
	&dev_attr_calibration,
	&dev_attr_vendor,
	&dev_attr_name,
	NULL
};

#if defined(ENABLE_INT1) || defined(ENABLE_INT2)
unsigned char *orient[] = {"upward looking portrait upright",   \
			"upward looking portrait upside-down",   \
			"upward looking landscape left",   \
			"upward looking landscape right",   \
			"downward looking portrait upright",   \
			"downward looking portrait upside-down",   \
			"downward looking landscape left",   \
			"downward looking landscape right"};

static void bma2x2_irq_work_func(struct work_struct *work)
{
	struct bma2x2_data *bma2x2 = container_of((struct work_struct *)work,
			struct bma2x2_data, irq_work);

	unsigned char status = 0;
	unsigned char i;
	unsigned char first_value = 0;
	unsigned char sign_value = 0;

	bma2x2_get_interruptstatus1(bma2x2->bma2x2_client, &status);

	switch (status) {
	case 0x01:
		pr_info("%s : Low G interrupt happened\n", __func__);
		input_report_rel(bma2x2->input, LOW_G_INTERRUPT,
				LOW_G_INTERRUPT_HAPPENED);
		break;
	case 0x02:
		for (i = 0; i < 3; i++) {
			bma2x2_get_HIGH_first(bma2x2->bma2x2_client, i,
					   &first_value);
			if (first_value == 1) {

				bma2x2_get_HIGH_sign(bma2x2->bma2x2_client,
						   &sign_value);

				if (sign_value == 1) {
					if (i == 0)
						input_report_rel(bma2x2->input,
						HIGH_G_INTERRUPT,
					HIGH_G_INTERRUPT_X_NEGATIVE_HAPPENED);
					if (i == 1)
						input_report_rel(bma2x2->input,
						HIGH_G_INTERRUPT,
					HIGH_G_INTERRUPT_Y_NEGATIVE_HAPPENED);
					if (i == 2)
						input_report_rel(bma2x2->input,
						HIGH_G_INTERRUPT,
					HIGH_G_INTERRUPT_Z_NEGATIVE_HAPPENED);
				} else {
					if (i == 0)
						input_report_rel(bma2x2->input,
						HIGH_G_INTERRUPT,
					HIGH_G_INTERRUPT_X_HAPPENED);
					if (i == 1)
						input_report_rel(bma2x2->input,
						HIGH_G_INTERRUPT,
					HIGH_G_INTERRUPT_Y_HAPPENED);
					if (i == 2)
						input_report_rel(bma2x2->input,
						HIGH_G_INTERRUPT,
					HIGH_G_INTERRUPT_Z_HAPPENED);

				}
			}
			pr_info("%s : High G interrupt happened,exis is %d," \
				"first is %d,sign is %d\n", __func__,
					i, first_value, sign_value);
		}
		break;
	case 0x04:
		for (i = 0; i < 3; i++) {
			bma2x2_get_slope_first(bma2x2->bma2x2_client, i,
						&first_value);
			if (first_value == 1) {
				bma2x2_get_slope_sign(bma2x2->bma2x2_client,
					&sign_value);

				if (sign_value == 1) {
					if (i == 0)
						input_report_rel(bma2x2->input,
						SLOP_INTERRUPT,
					SLOPE_INTERRUPT_X_NEGATIVE_HAPPENED);
					else if (i == 1)
						input_report_rel(bma2x2->input,
						SLOP_INTERRUPT,
					SLOPE_INTERRUPT_Y_NEGATIVE_HAPPENED);
					else if (i == 2)
						input_report_rel(bma2x2->input,
						SLOP_INTERRUPT,
					SLOPE_INTERRUPT_Z_NEGATIVE_HAPPENED);
				} else {
					if (i == 0)
						input_report_rel(bma2x2->input,
								SLOP_INTERRUPT,
						SLOPE_INTERRUPT_X_HAPPENED);
					else if (i == 1)
						input_report_rel(bma2x2->input,
								SLOP_INTERRUPT,
						SLOPE_INTERRUPT_Y_HAPPENED);
					else if (i == 2)
						input_report_rel(bma2x2->input,
								SLOP_INTERRUPT,
						SLOPE_INTERRUPT_Z_HAPPENED);
				}
			}

			pr_info("%s : Slop interrupt happened,exis is %d," \
				"first is %d,sign is %d\n", __func__,
					i, first_value, sign_value);
		}
		break;
	case 0x08:
		pr_info("%s : slow/ no motion interrupt happened\n", __func__);
		input_report_rel(bma2x2->input, SLOW_NO_MOTION_INTERRUPT,
					SLOW_NO_MOTION_INTERRUPT_HAPPENED);
		break;
	case 0x10:
		pr_info("%s : double tap interrupt happened\n", __func__);
		input_report_rel(bma2x2->input, DOUBLE_TAP_INTERRUPT,
					DOUBLE_TAP_INTERRUPT_HAPPENED);
		break;
	case 0x20:
		pr_info("%s : single tap interrupt happened\n", __func__);
		input_report_rel(bma2x2->input, SINGLE_TAP_INTERRUPT,
					SINGLE_TAP_INTERRUPT_HAPPENED);
		break;
	case 0x40:
		bma2x2_get_orient_status(bma2x2->bma2x2_client,
				    &first_value);
		pr_info("%s : orient interrupt happened,%s\n", __func__,
				orient[first_value]);
		if (first_value == 0)
			input_report_abs(bma2x2->input, ORIENT_INTERRUPT,
				UPWARD_PORTRAIT_UP_INTERRUPT_HAPPENED);
		else if (first_value == 1)
			input_report_abs(bma2x2->input, ORIENT_INTERRUPT,
				UPWARD_PORTRAIT_DOWN_INTERRUPT_HAPPENED);
		else if (first_value == 2)
			input_report_abs(bma2x2->input, ORIENT_INTERRUPT,
				UPWARD_LANDSCAPE_LEFT_INTERRUPT_HAPPENED);
		else if (first_value == 3)
			input_report_abs(bma2x2->input, ORIENT_INTERRUPT,
				UPWARD_LANDSCAPE_RIGHT_INTERRUPT_HAPPENED);
		else if (first_value == 4)
			input_report_abs(bma2x2->input, ORIENT_INTERRUPT,
				DOWNWARD_PORTRAIT_UP_INTERRUPT_HAPPENED);
		else if (first_value == 5)
			input_report_abs(bma2x2->input, ORIENT_INTERRUPT,
				DOWNWARD_PORTRAIT_DOWN_INTERRUPT_HAPPENED);
		else if (first_value == 6)
			input_report_abs(bma2x2->input, ORIENT_INTERRUPT,
				DOWNWARD_LANDSCAPE_LEFT_INTERRUPT_HAPPENED);
		else if (first_value == 7)
			input_report_abs(bma2x2->input, ORIENT_INTERRUPT,
				DOWNWARD_LANDSCAPE_RIGHT_INTERRUPT_HAPPENED);
		break;
	case 0x80:
		bma2x2_get_orient_flat_status(bma2x2->bma2x2_client,
				    &sign_value);
		pr_info("%s : flat interrupt happened,flat status is %d\n",
				__func__, sign_value);
		if (sign_value == 1) {
			input_report_abs(bma2x2->input, FLAT_INTERRUPT,
				FLAT_INTERRUPT_TURE_HAPPENED);
		} else {
			input_report_abs(bma2x2->input, FLAT_INTERRUPT,
				FLAT_INTERRUPT_FALSE_HAPPENED);
		}
		break;
	default:
		pr_info("%s : unkown status = %d\n", __func_, status);
		break;
	}
}

static irqreturn_t bma2x2_irq_handler(int irq, void *handle)
{
	struct bma2x2_data *data = handle;

	if (data == NULL)
		return IRQ_HANDLED;
	if (data->bma2x2_client == NULL)
		return IRQ_HANDLED;

	schedule_work(&data->irq_work);

	return IRQ_HANDLED;
}
#endif /* defined(ENABLE_INT1)||defined(ENABLE_INT2) */

static int bma2x2_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int err = 0;
	int tempvalue;
	unsigned char tmp_chip_id;
	struct bma2x2_data *data;
	struct input_dev *dev;

	pr_info("%s : enter\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_info("i2c_check_functionality error\n");
		goto exit;
	}
	data = kzalloc(sizeof(struct bma2x2_data), GFP_KERNEL);
	if (!data) {
		err = -ENOMEM;
		goto exit;
	}
	/* read chip id */
	tempvalue = i2c_smbus_read_word_data(client, CHIP_ID_REG);
	tmp_chip_id = tempvalue & 0x00ff;

	switch (tmp_chip_id) {
	case BMA255_CHIP_ID:
		data->sensor_type = BMA255_TYPE;
		break;
	case BMA250E_CHIP_ID:
		data->sensor_type = BMA250E_TYPE;
		break;
	case BMA222E_CHIP_ID:
		data->sensor_type = BMA222E_TYPE;
		break;
	case BMA280_CHIP_ID:
		data->sensor_type = BMA280_TYPE;
		break;
	default:
		data->sensor_type = -1;
	}

	if (data->sensor_type != -1) {
		data->chip_id = tmp_chip_id;
		pr_info("%s : %s registered I2C driver!\n", __func__,
				sensor_name[data->sensor_type]);
	} else{
		pr_info("%s : device not found i2c error %d\n", __func__,
				tempvalue);
		err = -ENODEV;
		goto kfree_exit;
	}

	i2c_set_clientdata(client, data);
	data->bma2x2_client = client;
	mutex_init(&data->value_mutex);
	mutex_init(&data->mode_mutex);
	mutex_init(&data->enable_mutex);
	bma2x2_set_bandwidth(client, BW_SET);
	if (bma2x2_set_range(client, RANGE_2G))
		pr_err("%s :  bma2x2_set_range error %d\n", __func__, RANGE_2G);

#if defined(ENABLE_INT1) || defined(ENABLE_INT2)
	bma2x2_set_Int_mode(client, 1);/*latch interrupt 250ms*/
#endif
	/*8,single tap
	  10,orient
	  11,flat*/
/*	bma2x2_set_Int_Enable(client, 8, 1);
	bma2x2_set_Int_Enable(client, 10, 1);
	bma2x2_set_Int_Enable(client, 11, 1);
*/

#ifdef ENABLE_INT1
	/* maps interrupt to INT1 pin */
	bma2x2_set_int1_pad_sel(client, PAD_LOWG);
	bma2x2_set_int1_pad_sel(client, PAD_HIGHG);
	bma2x2_set_int1_pad_sel(client, PAD_SLOP);
	bma2x2_set_int1_pad_sel(client, PAD_DOUBLE_TAP);
	bma2x2_set_int1_pad_sel(client, PAD_SINGLE_TAP);
	bma2x2_set_int1_pad_sel(client, PAD_ORIENT);
	bma2x2_set_int1_pad_sel(client, PAD_FLAT);
	bma2x2_set_int1_pad_sel(client, PAD_SLOW_NO_MOTION);
#endif

#ifdef ENABLE_INT2
	/* maps interrupt to INT2 pin */
	bma2x2_set_int2_pad_sel(client, PAD_LOWG);
	bma2x2_set_int2_pad_sel(client, PAD_HIGHG);
	bma2x2_set_int2_pad_sel(client, PAD_SLOP);
	bma2x2_set_int2_pad_sel(client, PAD_DOUBLE_TAP);
	bma2x2_set_int2_pad_sel(client, PAD_SINGLE_TAP);
	bma2x2_set_int2_pad_sel(client, PAD_ORIENT);
	bma2x2_set_int2_pad_sel(client, PAD_FLAT);
	bma2x2_set_int2_pad_sel(client, PAD_SLOW_NO_MOTION);
#endif

#if defined(ENABLE_INT1) || defined(ENABLE_INT2)
	data->IRQ = client->irq;
	err = request_irq(data->IRQ, bma2x2_irq_handler, IRQF_TRIGGER_RISING,
			"bma2x2", data);
	if (err)
		pr_err("could not request irq\n");

	INIT_WORK(&data->irq_work, bma2x2_irq_work_func);
#endif

	dev = input_allocate_device();
	if (!dev)
		goto error_input_allocate_device_bma2x2;

	dev->name = SENSOR_NAME;
	dev->id.bustype = BUS_I2C;

	input_set_capability(dev, EV_REL, SLOW_NO_MOTION_INTERRUPT);
	input_set_capability(dev, EV_REL, LOW_G_INTERRUPT);
	input_set_capability(dev, EV_REL, HIGH_G_INTERRUPT);
	input_set_capability(dev, EV_REL, SLOP_INTERRUPT);
	input_set_capability(dev, EV_REL, DOUBLE_TAP_INTERRUPT);
	input_set_capability(dev, EV_REL, SINGLE_TAP_INTERRUPT);
	input_set_capability(dev, EV_ABS, ORIENT_INTERRUPT);
	input_set_capability(dev, EV_ABS, FLAT_INTERRUPT);
	input_set_abs_params(dev, ABS_X, ABSMIN, ABSMAX, 0, 0);
	input_set_abs_params(dev, ABS_Y, ABSMIN, ABSMAX, 0, 0);
	input_set_abs_params(dev, ABS_Z, ABSMIN, ABSMAX, 0, 0);

	input_set_drvdata(dev, data);
	err = input_register_device(dev);
	if (err < 0) {
		input_free_device(dev);
		goto error_input_allocate_device_bma2x2;
	}
	data->input = dev;

	err = sysfs_create_group(&data->input->dev.kobj,
			&bma2x2_attribute_group);
	if (err < 0)
		goto error_sysfs;

#ifdef BMA_USE_PLATFORM_DATA
	if (NULL != client->dev.platform_data) {
		data->bst_pd = kzalloc(sizeof(*data->bst_pd),
				GFP_KERNEL);

		if (NULL != data->bst_pd) {
			memcpy(data->bst_pd, client->dev.platform_data,
					sizeof(*data->bst_pd));
			pr_info("bma2x2 place of bma in %s :  %d",
					data->bst_pd->name,
					data->bst_pd->place);
		}
	}
#endif

	INIT_DELAYED_WORK(&data->work, bma2x2_work_func);
	atomic_set(&data->delay, MAX_DELAY);
	atomic_set(&data->enable, 0);

	err = sensors_register(&bma2x2_device, data,
			bma2x2_sysfs_attributes, "accelerometer_sensor");
	if (err < 0) {
		pr_info("%s :  could not sensors_register\n", __func__);
		goto exit_bma2x2_sensors_register;
	}

#ifdef BMA_USE_PLATFORM_DATA
	if (NULL != client->dev.platform_data) {
		data->bst_pd = kzalloc(sizeof(*data->bst_pd),
				GFP_KERNEL);

		if (NULL != data->bst_pd) {
			memcpy(data->bst_pd, client->dev.platform_data,
					sizeof(*data->bst_pd));
			pr_info("bma2x2 place of bma in %s :  %d",
					data->bst_pd->name,
					data->bst_pd->place);
		}
	}
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
	data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	data->early_suspend.suspend = bma2x2_early_suspend;
	data->early_suspend.resume = bma2x2_late_resume;
	register_early_suspend(&data->early_suspend);
#endif

	return 0;

exit_bma2x2_sensors_register:
	sysfs_remove_group(&data->input->dev.kobj,
		&bma2x2_attribute_group);
error_sysfs:
	input_unregister_device(data->input);
error_input_allocate_device_bma2x2:
	mutex_destroy(&data->value_mutex);
	mutex_destroy(&data->mode_mutex);
	mutex_destroy(&data->enable_mutex);
kfree_exit:
#ifdef BMA_USE_PLATFORM_DATA
	if ((NULL != data) && (NULL != data->bst_pd)) {
		kfree(data->bst_pd);
		data->bst_pd = NULL;
	}
#endif
	kfree(data);
exit:
	return err;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void bma2x2_early_suspend(struct early_suspend *h)
{
	struct bma2x2_data *data =
		container_of(h, struct bma2x2_data, early_suspend);

	/* ACC_INT */
	unsigned long acc_int_suspend_mfpr[] = {
	GPIO010_GPIO_10 | MFP_PULL_LOW|MFP_LPM_INPUT|MFP_LPM_PULL_LOW,
	};

	mutex_lock(&data->enable_mutex);
	if (atomic_read(&data->enable) == 1) {
		bma2x2_set_mode(data->bma2x2_client, MODE_SUSPEND);
		cancel_delayed_work_sync(&data->work);
	}
	mfp_config(ARRAY_AND_SIZE(acc_int_suspend_mfpr));
	mutex_unlock(&data->enable_mutex);
}

static void bma2x2_late_resume(struct early_suspend *h)
{
	struct bma2x2_data *data =
		container_of(h, struct bma2x2_data, early_suspend);

	/* ACC_INT */
	unsigned long acc_int_resume_mfpr[] = {
	GPIO010_GPIO_10 | MFP_PULL_LOW|MFP_LPM_INPUT|MFP_LPM_PULL_LOW,
	};

	mutex_lock(&data->enable_mutex);
	if (atomic_read(&data->enable) == 1) {
		bma2x2_set_mode(data->bma2x2_client, MODE_NORMAL);
		schedule_delayed_work(&data->work,
				msecs_to_jiffies(atomic_read(&data->delay)));
	}
	mfp_config(ARRAY_AND_SIZE(acc_int_resume_mfpr));
	mutex_unlock(&data->enable_mutex);
}
#endif

static int __devexit bma2x2_remove(struct i2c_client *client)
{
	struct bma2x2_data *data = i2c_get_clientdata(client);

	if (NULL == data) {
		pr_err("%s : data is null\n", __func__);
		goto err_exit;
	}

	bma2x2_set_enable(&client->dev, 0);
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&data->early_suspend);
#endif
	sysfs_remove_group(&data->input->dev.kobj, &bma2x2_attribute_group);
	input_unregister_device(data->input);

#ifdef BMA_USE_PLATFORM_DATA
	if (NULL != data->bst_pd) {
		kfree(data->bst_pd);
		data->bst_pd = NULL;
	}
#endif

	kfree(data);
err_exit:
	return 0;
}
#ifdef CONFIG_PM
#ifndef CONFIG_HAS_EARLYSUSPEND
static int bma2x2_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct bma2x2_data *data = i2c_get_clientdata(client);

	mutex_lock(&data->enable_mutex);
	if (atomic_read(&data->enable) == 1) {
		bma2x2_set_mode(data->bma2x2_client, MODE_SUSPEND);
		cancel_delayed_work_sync(&data->work);
	}
	mutex_unlock(&data->enable_mutex);

	return 0;
}

static int bma2x2_resume(struct i2c_client *client)
{
	struct bma2x2_data *data = i2c_get_clientdata(client);

	mutex_lock(&data->enable_mutex);
	if (atomic_read(&data->enable) == 1) {
		bma2x2_set_mode(data->bma2x2_client, MODE_NORMAL);
		schedule_delayed_work(&data->work,
				msecs_to_jiffies(atomic_read(&data->delay)));
	}
	mutex_unlock(&data->enable_mutex);

	return 0;
}
#endif
#else

#define bma2x2_suspend		NULL
#define bma2x2_resume		NULL
awef
#endif /* CONFIG_PM */

static const struct i2c_device_id bma2x2_id[] = {
	{ SENSOR_NAME, 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, bma2x2_id);

static struct i2c_driver bma2x2_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= SENSOR_NAME,
	},
	.suspend	= bma2x2_suspend,
	.resume		= bma2x2_resume,
	.id_table	= bma2x2_id,
	.probe		= bma2x2_probe,
	.remove		= __devexit_p(bma2x2_remove),

};

static int __init bma2x2_init(void)
{
	return i2c_add_driver(&bma2x2_driver);
}

static void __exit bma2x2_exit(void)
{
	i2c_del_driver(&bma2x2_driver);
}

MODULE_AUTHOR("Albert Zhang <xu.zhang@bosch-sensortec.com>");
MODULE_DESCRIPTION("BMA2X2 accelerometer sensor driver");
MODULE_LICENSE("GPL");

module_init(bma2x2_init);
module_exit(bma2x2_exit);
