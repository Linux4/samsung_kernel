/*
 * Copyright (C) 2018 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*! \file
 * \brief Device driver for monitoring ambient light intensity in (lux)
 * proximity detection (prox), and Beam functionality within the
 * AMS TMX49xx family of devices.
 */

#ifndef __AMS_TCS3407_H
#define __AMS_TCS3407_H

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/gpio.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/ktime.h>
#include <linux/timer.h>
#include <linux/file.h>
#include <linux/syscalls.h>
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#endif

#include <linux/wait.h>

#define HEADER_VERSION		"14"

#define CONFIG_AMS_ALS_CRGBW
#define CONFIG_AMS_OPTICAL_SENSOR_ALS
#define CONFIG_AMS_OPTICAL_SENSOR_ALS_CCB

//if define below feature then polling method will be operating
#define CONFIG_AMS_OPTICAL_SENSOR_POLLING
#define HIGH    0xFF
#define LOW     0x00

#if !defined(CONFIG_ALS_CAL_TARGET)
#define CONFIG_ALS_CAL_TARGET          300 /* lux */
#endif
#if !defined(CONFIG_ALS_CAL_TARGET_TOLERANCE)
#define CONFIG_ALS_CAL_TARGET_TOLERANCE  15 /* lux */
#endif

#define AMS_LUX_AVERAGE_COUNT    8

// LUX Coefficient 
#define AMS_ALS_C_COEF                  (1340)//(1340.6946f)//(1115)
#define AMS_ALS_R_COEF                  (-796)//(-796.478f)//(-888)
#define AMS_ALS_G_COEF                  (214)//(214.5138f)//(217)
#define AMS_ALS_B_COEF                  (-893)//(-893.1761f)//(-444)
#define AMS_ALS_D_FACTOR                (1)//(3673)
#define AMS_ALS_CT_COEF                 (1994)
#define AMS_ALS_CT_OFFSET               (1452)

typedef enum _amsAlsAdaptive {
	ADAPTIVE_ALS_NO_REQUEST,
	ADAPTIVE_ALS_TIME_INC_REQUEST,
	ADAPTIVE_ALS_TIME_DEC_REQUEST,
	ADAPTIVE_ALS_GAIN_INC_REQUEST,
	ADAPTIVE_ALS_GAIN_DEC_REQUEST
} amsAlsAdaptive_t;

typedef enum _amsAlsStatus {
	ALS_STATUS_IRQ  = (1 << 0),
	ALS_STATUS_RDY  = (1 << 1),
	ALS_STATUS_OVFL = (1 << 2)
} amsAlsStatus_t;

typedef struct _alsData {
	uint16_t clearADC;
	uint16_t redADC;
	uint16_t greenADC;
	uint16_t blueADC;
	uint16_t widebandADC;
} alsData_t;

typedef struct _amsAlsCalibration {
	uint32_t Time_base; /* in uSec */
	uint32_t adcMaxCount;
	uint16_t calibrationFactor; /* default 1000 */
	uint8_t thresholdLow;
	uint8_t thresholdHigh;
	int32_t Wbc;
} amsAlsCalibration_t;

typedef struct _amsAlsInitData {
	bool adaptive;
	bool irRejection;
	uint32_t time_us;
	uint32_t gain;
	amsAlsCalibration_t calibration;
} amsAlsInitData_t;

typedef struct _amsALSConf {
	uint32_t time_us;
	uint32_t gain;
	uint8_t thresholdLow;
	uint8_t thresholdHigh;
} amsAlsConf_t;

typedef struct _amsAlsDataSet {
	alsData_t *datasetArray;
	uint64_t timeStamp;
	uint8_t status;
} amsAlsDataSet_t;

typedef struct _amsAlsResult {
	uint32_t irrClear;
	uint32_t irrRed;
	uint32_t irrGreen;
	uint32_t irrBlue;
	uint32_t IR;
	uint32_t irrWideband;
	uint32_t mLux;
	uint32_t mMaxLux;
	uint32_t mLux_ave;
	uint32_t CCT;
	amsAlsAdaptive_t adaptive;
	uint32_t rawClear;
	uint32_t rawRed;
	uint32_t rawGreen;
	uint32_t rawBlue;
	uint32_t rawWideband;
} amsAlsResult_t;

typedef struct _amsAlsContext {
	uint64_t lastTimeStamp;
	uint32_t ave_lux[AMS_LUX_AVERAGE_COUNT];
	uint32_t ave_lux_index;
	uint32_t cpl;
	uint32_t uvir_cpl;
	uint32_t time_us;
	amsAlsCalibration_t calibration;
	amsAlsResult_t results;
	bool adaptive;
	uint16_t saturation;
	uint32_t gain;
	uint32_t previousGain;
	uint32_t previousLux;
	bool notStableMeasurement;
} amsAlsContext_t;

typedef struct _amsAlsAlgInfo {
	char *algName;
	uint16_t contextMemSize;
	uint16_t scratchMemSize;
	amsAlsCalibration_t calibrationData;
	int (*initAlg)(amsAlsContext_t *ctx, amsAlsInitData_t *initData);
	int (*processData)(amsAlsContext_t *ctx, amsAlsDataSet_t *inputData);
	int (*getResult)(amsAlsContext_t *ctx, amsAlsResult_t *outData);
	int (*setConfig)(amsAlsContext_t *ctx, amsAlsConf_t *inputData);
	int (*getConfig)(amsAlsContext_t *ctx, amsAlsConf_t *outputData);
} amsAlsAlgoInfo_t;

typedef struct {
	uint32_t calibrationFactor;
	uint32_t Time_base; /* in uSec */
	uint32_t adcMaxCount;
	uint8_t thresholdLow;
	uint8_t thresholdHigh;
	int32_t Wbc;
} ams_ccb_als_calibration_t;

typedef struct {
	uint32_t uSecTime;
	uint32_t gain;
	uint8_t threshold;
} ams_ccb_als_config_t;

typedef struct {
	bool calibrate;
	bool autoGain;
	uint8_t hysteresis;
	uint16_t sampleRate;
	ams_ccb_als_config_t configData;
	ams_ccb_als_calibration_t calibrationData;
} ams_ccb_als_init_t;

typedef enum {
	AMS_CCB_ALS_INIT,
	AMS_CCB_ALS_RGB,
	AMS_CCB_ALS_AUTOGAIN,
	AMS_CCB_ALS_CALIBRATION_INIT,
	AMS_CCB_ALS_CALIBRATION_COLLECT_DATA,
	AMS_CCB_ALS_CALIBRATION_CHECK,
	AMS_CCB_ALS_LAST_STATE
} ams_ccb_als_state_t;

typedef struct {
	char *algName;
	uint16_t contextMemSize;
	uint16_t scratchMemSize;
	ams_ccb_als_calibration_t defaultCalibrationData;
} ams_ccb_als_info_t;

typedef struct {
	ams_ccb_als_state_t state;
	amsAlsContext_t ctxAlgAls;
	ams_ccb_als_init_t initData;
	uint16_t bufferCounter;
	uint16_t shadowAiltReg;
	uint16_t shadowAihtReg;
} ams_ccb_als_ctx_t;

typedef struct {
	uint8_t  statusReg;
} ams_ccb_als_dataSet_t;

typedef struct {
	uint32_t mLux;
	uint32_t ir;
	uint32_t red;
	uint32_t green;
	uint32_t blue;
	uint32_t clear;
	uint32_t wideband;
	uint32_t rawClear;
	uint32_t rawRed;
	uint32_t rawGreen;
	uint32_t rawBlue;
	uint32_t rawWideband;
	uint32_t time_us;
	uint32_t gain;
} ams_ccb_als_result_t;

#define PWR_ON		1
#define PWR_OFF		0
#define PM_RESUME	1
#define PM_SUSPEND	0
#define NAME_LEN		32

#define CONFIG_AMS_LITTLE_ENDIAN 1
#ifdef CONFIG_AMS_LITTLE_ENDIAN
#define AMS_ENDIAN_1    0
#define AMS_ENDIAN_2    8
#else
#define AMS_ENDIAN_2    0
#define AMS_ENDIAN_1    8
#endif

#define AMS_PORT_portHndl   struct i2c_client

typedef struct {
	bool     nearBy;
	uint16_t proximity;
} ams_apiPrx_t;

typedef enum {
	NOT_VALID,
	PRESENT,
	ABSENT
} ternary;

typedef struct {
	ternary freq100Hz;
	ternary freq120Hz;
	uint32_t mHz;
} ams_apiAlsFlicker_t;

typedef struct {
	uint32_t mLux;
	uint32_t ir;
	uint32_t red;
	uint32_t green;
	uint32_t blue;
	uint32_t clear;
	uint32_t wideband;
	uint32_t rawClear;
	uint32_t rawRed;
	uint32_t rawGreen;
	uint32_t rawBlue;
	uint32_t rawWideband;
	uint32_t time_us;
	uint32_t gain;
} ams_apiAls_t;

#ifdef __cplusplus
extern "C" {
#endif

#define AMS_USEC_PER_TICK			(2780)
#define ACTUAL_USEC(x)	(((x + AMS_USEC_PER_TICK / 2) / AMS_USEC_PER_TICK) * AMS_USEC_PER_TICK)
#define AMS_ALS_USEC_TO_REG(x)		(256 - (x / AMS_USEC_PER_TICK))
#define AMS_DEFAULT_REPORTTIME_US	(1000000) /* Max 8 seconds */
#define AMS_PRX_PGLD_TO_REG(x)		((x-4)/8)

#ifndef UINT_MAX_VALUE
#define UINT_MAX_VALUE      (-1)
#endif

#define AMS_CALIBRATION_DONE                (-1)
#define AMS_CALIBRATION_DONE_BUT_FAILED     (-2)

typedef enum _deviceIdentifier_e {
	AMS_UNKNOWN_DEVICE,
	AMS_TCS3407,
	AMS_TCS3407_UNTRIM,
	AMS_TCS3408,
	AMS_TCS3408_UNTRIM,
	AMS_LAST_DEVICE
} ams_deviceIdentifier_e;

/*TCS3407 */
/*0x92 ID(0x18), 0x91 REVID(0x51)*/
#define AMS_DEVICE_ID       0x18
#define AMS_DEVICE_ID_MASK  0xFF
#define AMS_REV_ID         0x51
#define AMS_REV_ID_UNTRIM   0x01
#define AMS_REV_ID_MASK     0xFF


/*TCS3408 */
/*0x92 ID(0x18), 0x91 REVID(0x53)*/
#define AMS_DEVICE_ID2       0x18
#define AMS_DEVICE_ID2_MASK  0xFF
#define AMS_REV_ID2          0x53
#define AMS_REV_ID2_UNTRIM   0x03
#define AMS_REV_ID2_MASK     0xFF


/*NOT USED*/
#define AMS_DEVICE_ID3       0xEC
#define AMS_DEVICE_ID3_MASK  0xFC
#define AMS_REV_ID3          0x00
#define AMS_REV_ID3_MASK     0x07



#define AMS_PRX_PERS_TO_REG(x)		(x << 4)
#define AMS_PRX_REG_TO_PERS(x)		(x >> 4)
#define AMS_PRX_CURRENT_TO_REG(mA)	((((mA) > 257) ? 127 : (((mA) - 4) >> 1)) << 0)
#define AMS_ALS_PERS_TO_REG(x)		(x << 0)
#define AMS_ALS_REG_TO_PERS(x)		(x >> 0)

typedef enum _deviceRegisters {
	DEVREG_ENABLE,
	DEVREG_ATIME,
	DEVREG_PTIME,
	DEVREG_WTIME,
	DEVREG_AILTL,
	DEVREG_AILTH,
	DEVREG_AIHTL,
	DEVREG_AIHTH,
	DEVREG_AUXID,
	DEVREG_REVID,
	DEVREG_ID,
	DEVREG_STATUS,
	DEVREG_ASTATUS,
	DEVREG_ADATA0L,
	DEVREG_ADATA0H,
	DEVREG_ADATA1L,
	DEVREG_ADATA1H,
	DEVREG_ADATA2L,
	DEVREG_ADATA2H,
	DEVREG_ADATA3L,
	DEVREG_ADATA3H,
	DEVREG_ADATA4L,
	DEVREG_ADATA4H,
	DEVREG_ADATA5L,
	DEVREG_ADATA5H,
	DEVREG_STATUS2,
	DEVREG_STATUS3,
	DEVREG_STATUS5,
	DEVREG_STATUS4,
	DEVREG_CFG0,
	DEVREG_CFG1,
	DEVREG_CFG3,
	DEVREG_CFG4,
	DEVREG_CFG6,
	DEVREG_CFG8,
	DEVREG_CFG9,
	DEVREG_CFG10,
	DEVREG_CFG11,
	DEVREG_CFG12,
	DEVREG_PERS,
	DEVREG_GPIO,
	DEVREG_ASTEPL,
	DEVREG_ASTEPH,
	DEVREG_AGC_GAIN_MAX,
	DEVREG_AZ_CONFIG,
	DEVREG_FD_CFG0,
	DEVREG_FD_CFG1,
	DEVREG_FD_CFG2,
	DEVREG_FD_CFG3,
	DEVREG_FD_STATUS,
	DEVREG_INTENAB,
	DEVREG_CONTROL,
	DEVREG_FIFO_MAP,
	DEVREG_FIFO_STATUS,
	DEVREG_FDATAL,
	DEVREG_FDATAH,
	DEVREG_FLKR_WA_RAMLOC_1,
	DEVREG_FLKR_WA_RAMLOC_2,
	DEVREG_SOFT_RESET,     /* 0xF3 */

	DEVREG_RAM_START,   /* pseudoreg, 0x00 */

	DEVREG_REG_MAX
} ams_deviceRegister_t;

typedef enum _3407_regOptions {
	PON             = 0x01, /* register 0x80 */
	AEN             = 0x02,
	PEN             = 0x04,
	WEN             = 0x08,
	SMUXEN          = 0x10,
	/* reserved: 0x20 */
	FDEN            = 0x40,
	IBEN            = 0x80,

	/* STATUS REG:  Interrupts.  All but FIFOINT are cleared by writing
	 * back a '1' bit.  FIFOINT only clearable by emptying the FIFO.
	 */
	SINT            = 0x01, /* register 0x93 */
	CINT            = 0x02,
	FIFOINT         = 0x04,
	AINT            = 0x08,
	PINT0           = 0x10,
	PINT1           = 0x20,
	PSAT            = 0x40,
	ASAT_FDSAT      = 0x80,
	ALS_INT_ALL     = (AINT + ASAT_FDSAT),
	PROX_INT_ALL_0  = (CINT + PINT0),
	PROX_INT_ALL_1  = (CINT + PINT1),
	PROX_INT_ALL_0_1 = (CINT + PINT0 + PINT1),
	/*PROX_INT_ALL    = (CINT + PINT0),*/
	FLICKER_INT_ALL = (SINT + FIFOINT + ASAT_FDSAT),
	IRBEAM_INT      = SINT,

	FDSAT_DIGITAL   = 0x01, /* register 0xA3 */
	FDSAT_ANALOG    = 0x02,
	ASAT_ANALOG     = 0x08,
	ASAT_DIGITAL    = 0x10,
	AVALID          = 0x40,
	PVALID0         = 0x80,

	PSAT_AMB        = 0x01, /* register 0xA4 */
	PSAT_REFL       = 0x02,
	PSAT_ADC        = 0x04,

	PINT0_PILT      = 0x01, /* register 0xA5 */
	PINT0_PIHT      = 0x02,
	PINT1_PILT      = 0x04,
	PINT1_PIHT      = 0x08,

	/* ambient, prox threshold/hysteresis bits in 0xA4-A5:  not used (yet?) */

	IBUSY           = 0x01, /* register 0xA6 */
	SINT_IRBEAM     = 0x02,
	SINT_SMUX       = 0x04,
	SINT_FD         = 0x08,
	SINT_ALS_MAN_AZ = 0x10,
	SINT_AUX        = 0x20,

	INIT_BUSY       = 0x01, /* register 0xA7 */

	RAM_BANK_0      = 0x00, /* register 0xA9 */
	RAM_BANK_1      = 0x01,
	RAM_BANK_2      = 0x02,
	RAM_BANK_3      = 0x03,
	ALS_TRIG_LONG   = 0x04,
	PRX_TRIG_LONG   = 0x08,
	REG_BANK        = 0x10,
	LOWPWR_IDLE     = 0x20,
	PRX_OFFSET2X    = 0x40,
	PGOFF_HIRES     = 0x80,

	AGAIN_1_HALF    = 0x00, /* register 0xAA */
	AGAIN_1         = 0x01,
	AGAIN_2         = 0x02,
	AGAIN_4         = 0x03,
	AGAIN_8         = 0x04,
	AGAIN_16        = 0x05,
	AGAIN_32        = 0x06,
	AGAIN_64        = 0x07,
	AGAIN_128       = 0x08,
	AGAIN_256       = 0x09,
	ALS_TRIG_FAST   = 0x40,
	S4S_MODE        = 0x80,

	HXTALK_MODE1    = 0x20, /* register 0xAC */

	SMUX_CMD_ROM_INIT   = 0x00, /* register 0xAF */
	SMUX_CMD_READ       = 0x08,
	SMUX_CMD_WRITE      = 0x10,
	SMUX_CMD_ARRAY_MODE = 0x18,

	SWAP_PROX_ALS5  = 0x01, /* register 0xB1 */
	ALS_AGC_ENABLE  = 0x04,
	FD_AGC_ENABLE   = 0x08,
	PROX_BEFORE_EACH_ALS    = 0x10,

	SIEN_AUX        = 0x08, /* register 0xB2 */
	SIEN_SMUX       = 0x10,
	SIEN_ALS_MAN_AZ = 0x20,
	SIEN_FD         = 0x40,
	SIEN_IRBEAM     = 0x80,

	FD_PERS_ALWAYS  = 0x00, /* register 0xB3 */
	FD_PERS_1       = 0x01,
	FD_PERS_2       = 0x02,
	FD_PERS_4       = 0x03,
	FD_PERS_8       = 0x04,
	FD_PERS_16      = 0x05,
	FD_PERS_32      = 0x06,
	FD_PERS_64      = 0x07,

	TRIGGER_APF_ALIGN = 0x10, /* register 0xB4 */
	PRX_TRIGGER_FAST = 0x20,
	PINT_DIRECT     = 0x40,
	AINT_DIRECT     = 0x80,

	PROX_FILTER_1   = 0x00, /* register 0xB8 */
	PROX_FILTER_2   = 0x01,
	PROX_FILTER_4   = 0x02,
	PROX_FILTER_8   = 0x03,
	HXTALK_MODE2    = 0x80,

	PGAIN_1         = 0x00, /* register 0xBB */
	PGAIN_2         = 0x01,
	PGAIN_4         = 0x02,
	PGAIN_8         = 0x03,

	PPLEN_4uS       = 0x00, /* register 0xBC */
	PPLEN_8uS       = 0x01,
	PPLEN_16uS      = 0x02,
	PPLEN_32uS      = 0x03,

	FD_COMPARE_32_32NDS = 0x00, /* register 0xD7 */
	FD_COMPARE_24_32NDS = 0x01,
	FD_COMPARE_16_32NDS = 0x02,
	FD_COMPARE_12_32NDS = 0x03,
	FD_COMPARE_8_32NDS  = 0x04,
	FD_COMPARE_6_32NDS  = 0x05,
	FD_COMPARE_4_32NDS  = 0x06,
	FD_COMPARE_3_32NDS  = 0x07,
	FD_SAMPLES_8    = 0x00,
	FD_SAMPLES_16   = 0x08,
	FD_SAMPLES_32   = 0x10,
	FD_SAMPLES_64   = 0x18,
	FD_SAMPLES_128  = 0x20,
	FD_SAMPLES_256  = 0x28,
	FD_SAMPLES_512  = 0x30,
	FD_SAMPLES_1024 = 0x38,

	FD_GAIN_1_HALF  = 0x00, /* register 0xDA */
	FD_GAIN_1       = 0x08,
	FD_GAIN_2       = 0x10,
	FD_GAIN_4       = 0x18,
	FD_GAIN_8       = 0x20,
	FD_GAIN_16      = 0x28,
	FD_GAIN_32      = 0x30,
	FD_GAIN_64      = 0x38,
	FD_GAIN_128     = 0x40,
	FD_GAIN_256     = 0x48,

	FD_100HZ_FLICKER = 0x01, /* register 0xDB */
	FD_120HZ_FLICKER = 0x02,
	FD_100HZ_VALID  = 0x04,
	FD_120HZ_VALID  = 0x08,
	FD_SAT_DETECTED = 0x10,
	FD_MEAS_VALID   = 0x20,

	ISTART_MOBEAM   = 0x01, /* register 0xE8 */
	ISTART_REMCON   = 0x02,

	START_OFFSET_CALIB  = 0x01, /* register 0xEA */

	DCAVG_AUTO_BSLN = 0x80, /* register 0xEB */
	DCAVG_AUTO_OFFSET_ADJUST = 0x40,
	BINSRCH_SKIP    = 0x08,

	BASELINE_ADJUSTED = 0x04, /* register 0xEE */
	OFFSET_ADJUSTED = 0x02,
	CALIB_FINISHED    = 0x01,

	SIEN            = 0x01, /* register 0xF9 */
	CIEN            = 0x02,
	FIEN            = 0x04,
	AIEN            = 0x08,
	PIEN0           = 0x10,
	PIEN1           = 0x20,
	PSIEN           = 0x40,
	ASIEN_FDSIEN    = 0x80,
	AMS_ALL_IENS    = (AIEN+PIEN0+PIEN1+FIEN+CIEN),
	LAST_IN_ENUM_LIST
} ams_regOptions_t;

typedef enum _3407_regMasks {
	MASK_PON            = 0x01, /* register 0x80 */
	MASK_AEN            = 0x02,
	MASK_PEN            = 0x04,
	MASK_WEN            = 0x08,
	MASK_SMUXEN         = 0x10,
	MASK_FDEN           = 0x40,
	MASK_IBEN           = 0x80,

	MASK_ATIME          = 0xFF, /* register 0x81 */

	MASK_PTIME          = 0xFF, /* register 0x82 */

	MASK_WTIME          = 0xFF, /* register 0x83 */

	MASK_AILT           = 0xFFFF, /* register 0x84 */
	MASK_AILH           = 0xFFFF, /* register 0x86 */

	MASK_PILT           = 0xFFFF, /* register 0x88 */
	MASK_PILH           = 0xFFFF, /* register 0x8A */

	MASK_AUXID          = 0x0F, /* register 0x90 */

	MASK_REV_ID         = 0x07, /* register 0x91 */

	MASK_ID             = 0xFC, /* register 0x92 */

	MASK_SINT           = 0x01, /* register 0x93 */
	MASK_CINT           = 0x02,
	MASK_FINT           = 0x04,
	MASK_AINT           = 0x08,
	MASK_PINT0          = 0x10,
	MASK_PINT1          = 0x20,
	MASK_PSAT           = 0x40,
	MASK_ASAT_FDSAT     = 0x80,
	MASK_ALS_INT_ALL    = MASK_AINT,
	MASK_PROX_INT_ALL_0 = (MASK_PINT0 | MASK_PSAT),
	MASK_PROX_INT_ALL_1 = (MASK_PINT1 | MASK_PSAT),
	MASK_PROX_INT_ALL_0_1 = (MASK_PINT0 | MASK_PINT1 | MASK_PSAT),
	/*MASK_PROX_INT_ALL   = (MASK_PINT0),*/
	MASK_INT_ALL        = (0xFF),

	MASK_ADATA          = 0xFFFF, /* registers 0x95-0xA0 */

	MASK_ASAT_ANALOG    = 0x08, /* register 0xA3 */
	MASK_ASAT_DIGITAL   = 0x10,
	MASK_AVALID         = 0x40,
	MASK_PVALID0        = 0x80,

	MASK_PSAT_AMB       = 0x01, /* register 0xA4 */
	MASK_PSAT_REFL      = 0x02,
	MASK_PSAT_ADC       = 0x04,

	MASK_RAM_BANK       = 0x03, /* register 0xA9 */

	MASK_AGAIN          = 0x1F, /* register 0xAA */

	MASK_HXTALK_MODE1   = 0x20, /* register 0xAC */

	MASK_SMUX_CMD       = 0x18, /* register 0xAF */

	MASK_AUTOGAIN               = 0x04, /* register 0xB1 */
	MASK_PROX_BEFORE_EACH_ALS   = 0x10,

	MASK_SIEN_FD        = 0x40, /* register 0xB2 */
	MASK_SIEN_IRBEAM    = 0x80,

	MASK_FD_PERS        = 0x07, /* register 0xB3 */
	MASK_AGC_HYST_LOW   = 0x30,
	MASK_AGC_HYST_HIGH  = 0xC0,

	MASK_PRX_TRIGGER_FAST   = 0x20, /* register 0xB4 */
	MASK_PINT_DIRECT    = 0x40,
	MASK_AINT_DIRECT    = 0x80,

	MASK_PROX_FILTER    = 0x03, /* register 0xB8 */
	MASK_HXTALK_MODE2   = 0x80,

	MASK_PLDRIVE0       = 0x7f, /* register 0xB9 */

	MASK_APERS          = 0x0F, /* register 0xBD */
	MASK_PPERS          = 0xF0,

	MASK_PPULSE         = 0x3F, /* register 0xBC */
	MASK_PPLEN          = 0xC0,

	MASK_PGAIN          = 0x03, /* register 0xBB */

	MASK_PLDRIVE        = 0x7F, /* Registers 0xB9, 0xBA */

	MASK_FD_TIME_MSBits = 0x07, /* Register 0xDA */

	MASK_100HZ_FLICKER  = 0x05, /* Register 0xDB */
	MASK_120HZ_FLICKER  = 0x0A,
	MASK_CLEAR_FLICKER_STATUS  = 0x3C,
	MASK_FLICKER_VALID  = 0x2C,

	MASK_SLEW           = 0x10, /* register E0 */
	MASK_ISQZT          = 0x07,

	MASK_OFFSET_CALIB   = 0x01, /* register 0xEA */

	MASK_DCAVG_AUTO_BSLN = 0x80, /* register 0xEB */
	MASK_BINSRCH_SKIP    = 0x08,

	MASK_PROX_AUTO_OFFSET_ADJUST    = 0x40, /* register 0xEC */
	MASK_PXAVG_AUTO_BSLN    = 0x08, /* register 0xEC */

	MASK_BINSRCH_TARGET = 0xE0, /* register 0xED */

	MASK_SIEN           = 0x01, /* register 0xF9 */
	MASK_CIEN           = 0x02,
	MASK_FIEN           = 0x04,
	MASK_AIEN           = 0x08,
	MASK_PIEN0          = 0x10,
	MASK_PIEN1          = 0x20,
	MASK_PSIEN          = 0x40,
	MASK_ASIEN_FDSIEN   = 0x80,

	MASK_AGC_HYST       = 0x30,
	MASK_AGC_LOW_HYST   = 0x30,
	MASK_AGC_HIGH_HYST  = 0xC0,

	MASK_SOFT_RESET     = 0x04, /* register 0xF3 */

	MASK_LAST_IN_ENUMLIST
} ams_regMask_t;


#define AMS_ENABLE_ALS_EX()         {ctx->shadowEnableReg |= (AEN); \
	ams_setByte(ctx->portHndl, DEVREG_ENABLE, ctx->shadowEnableReg); \
	ams_setField(ctx->portHndl, DEVREG_INTENAB, HIGH, (MASK_AIEN | MASK_ASIEN_FDSIEN));\
	}
#define AMS_DISABLE_ALS_EX()        {ctx->shadowEnableReg &= ~(AEN); \
	ams_setByte(ctx->portHndl, DEVREG_ENABLE, ctx->shadowEnableReg); \
	ams_setField(ctx->portHndl, DEVREG_INTENAB, LOW, (MASK_AIEN | MASK_ASIEN_FDSIEN));\
	}

typedef struct _deviceRegisterTable {
	uint8_t address;
	uint8_t resetValue;
} deviceRegisterTable_t;

typedef enum _3407_config_options {
	AMS_CONFIG_ENABLE,
	AMS_CONFIG_THRESHOLD,
	AMS_CONFIG_OPTION_LAST
} deviceConfigOptions_t;

typedef enum _3407_mode {
	MODE_OFF            = (0),
	MODE_ALS_LUX        = (1 << 0),
	MODE_ALS_RGB        = (1 << 1),
	MODE_ALS_CT         = (1 << 2),
	MODE_ALS_WIDEBAND   = (1 << 3),
	MODE_ALS_ALL        = (MODE_ALS_LUX | MODE_ALS_RGB | MODE_ALS_CT | MODE_ALS_WIDEBAND),
	MODE_FLICKER        = (1 << 4), /* is independent of ALS in this model */
	MODE_PROX           = (1 << 5),
	MODE_IRBEAM         = (1 << 6),
	MODE_UNKNOWN    /* must be in last position */
} ams_mode_t;

typedef enum _3407_configureFeature {
	AMS_CONFIG_PROX,
	AMS_CONFIG_ALS_LUX,
	AMS_CONFIG_ALS_RGB,
	AMS_CONFIG_ALS_CT,
	AMS_CONFIG_ALS_WIDEBAND,
	AMS_CONFIG_FLICKER,
	AMS_CONFIG_MOBEAM,
	AMS_CONFIG_REMCON,
	AMS_CONFIG_FEATURE_LAST
} ams_configureFeature_t;

typedef struct _calibrationData {
	uint32_t timeBase_us;
	uint32_t adcMaxCount;
	uint8_t alsThresholdHigh; /* in % */
	uint8_t alsThresholdLow;  /* in % */
	uint16_t alsCalibrationFactor;        /* multiplicative factor default 1000 */
	char deviceName[8];
	int32_t alsCoefC;
	int32_t alsCoefR;
	int32_t alsCoefG;
	int32_t alsCoefB;
	int16_t alsDfg;
	uint16_t alsCctOffset;
	uint16_t alsCctCoef;
	int32_t Wbc;
} ams_calibrationData_t;


typedef enum _sensorType {
	AMS_NO_SENSOR_AVAILABLE,
	AMS_AMBIENT_SENSOR,
	AMS_FLICKER_SENSOR,
	AMS_PROXIMITY_SENSOR,
	AMS_WIDEBAND_ALS_SENSOR,
	AMS_LAST_SENSOR,
	AMS_ALS_RGB_GAIN_CHANGED
} ams_sensorType_t;

typedef struct _sensorInfo {
	uint32_t standbyCurrent_uA;
	uint32_t activeCurrent_uA;
	uint32_t rangeMin;
	uint32_t rangeMax;
	char *driverName;
	uint8_t maxPolRate;
	uint8_t adcBits;
} ams_SensorInfo_t;

typedef struct _deviceInfo {
	uint32_t	memorySize;
	ams_calibrationData_t defaultCalibrationData;
	ams_SensorInfo_t proxSensor;
	ams_SensorInfo_t alsSensor;
	ams_SensorInfo_t mobeamSensor;
	ams_SensorInfo_t remconSensor;
	ams_sensorType_t tableSubSensors[10];
	uint8_t numberOfSubSensors;
	char *driverVersion;
	char *deviceModel;
	char *deviceName;
} ams_deviceInfo_t;


enum {
	DEBUG_REG_STATUS = 1,
	DEBUG_VAR,
};

struct mode_count {
	s32 hrm_cnt;
	s32 amb_cnt;
	s32 prox_cnt;
	s32 sdk_cnt;
	s32 cgm_cnt;
	s32 unkn_cnt;
};


struct tcs3407_parameters {
	/* Common */
	u8 persist;
	/* ALS / Color */
	u8 als_gain;
	u8 als_auto_gain;
	u16 als_deltaP;
	u8 als_time;
};

typedef struct _fifo {
	uint16_t AdcClear;
	uint16_t AdcRed;
	uint16_t AdcGreen;
	uint16_t AdcBlue;
	uint16_t AdcWb;
} adcDataSet_t;

struct tcs3407_data {
	struct device *dev;
	struct i2c_client *client;
	struct input_dev *input_dev;

	u8 enabled;
	int delay;

	adcDataSet_t rawdata;
	ams_deviceIdentifier_e device_id;

	u32 itime;
	u32 again;
	int lux;
	int previousGain;
	int count_log_time;
		

	struct mutex i2clock;
	struct mutex activelock;
	
	
	u32 sampling_period_ns;
	u8 regulator_state;
	u32 reg_read_buf;
	u8 debug_mode;
	struct mode_count mode_cnt;
	bool pm_state;
	int isTrimmed;

	u8 part_type;
	u32 i2c_err_cnt;
	u32 user_ir_data;
	u32 user_flicker_data;

	ams_mode_t mode;
#ifdef AMS_PHY_SUPPORT_SHADOW
	uint8_t shadow[DEVREG_REG_MAX];
#endif
	ams_ccb_als_ctx_t ccbAlsCtx;
	ams_calibrationData_t *systemCalibrationData;

	bool alwaysReadAls;			/* read ADATA every ams_deviceEventHandler call regardless of xINT bits */
	bool alwaysReadProx;		/* ditto PDATA */
	bool alwaysReadFlicker;

	uint32_t updateAvailable;
	uint8_t shadowEnableReg;
	uint8_t shadowIntenabReg;
	uint8_t shadowStatus1Reg;
	uint8_t shadowStatus2Reg;
	

#ifdef CONFIG_AMS_OPTICAL_SENSOR_POLLING
	struct hrtimer timer;
	ktime_t light_poll_delay;
	struct workqueue_struct *wq;
	struct work_struct work_light;
#endif        
};



#define AMSDRIVER_ALS_ENABLE 1
#define AMSDRIVER_ALS_DISABLE 0
#define AMSDRIVER_FLICKER_ENABLE 1
#define AMSDRIVER_FLICKER_DISABLE 0


extern unsigned int lpcharge;

#endif /* __AMS_TCS3407_H */
