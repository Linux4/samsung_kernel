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

#ifndef _SFH7832_H_
#define _SFH7832_H_

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/input.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/interrupt.h>
#include <linux/leds.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/gpio.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/file.h>
#include <linux/syscalls.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pm_qos.h>
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif

#define HEADER_VERSION		"17"

#define PWR_ON		1
#define PWR_OFF		0
#define PM_RESUME	1
#define PM_SUSPEND	0
#define NAME_LEN	32
#define MAX_BUF_LEN	512

/* #define HRM_DBG */
/* #define HRM_INFO */

#ifndef HRM_dbg
#ifdef HRM_DBG
#define HRM_dbg(format, arg...)		\
				printk(KERN_DEBUG "HRM_dbg : "format, ##arg);
#define HRM_err(format, arg...)		\
				printk(KERN_DEBUG "HRM_err : "format, ##arg);
#else
#define HRM_dbg(format, arg...)		{if (hrm_debug)\
				printk(KERN_DEBUG "HRM_dbg : "format, ##arg);\
					}
#define HRM_err(format, arg...)		{if (hrm_debug)\
				printk(KERN_DEBUG "HRM_err : "format, ##arg);\
					}
#endif
#endif

#ifndef HRM_info
#ifdef HRM_INFO
#define HRM_info(format, arg...)	\
				printk(KERN_INFO "HRM_info : "format, ##arg);
#else
#define HRM_info(format, arg...)	{if (hrm_info)\
				printk(KERN_INFO "HRM_info : "format, ##arg);\
					}
#endif
#endif

#define AFE4420_SLAVE_ADDR					0x5B
#define AFE4420_DESIGNID					0x0B	/* 4420 designid is 0xB */
#define AFE4420_DESIGNID_REV2				0x1B	/* 4420 Rev2 design id is 0x1B */

/*Global Controls*/
#define AFE4420_CONTROL0					0x00
#define AFE4420_GPIO2_CONF					0x02
#define AFE4420_INT2_STC					0x03
#define AFE4420_INT2_ENDC					0x04
#define AFE4420_TE_PRPCT					0x1D
#define AFE4420_CONTROL1					0x23
#define AFE4420_BIAS_REG					0x24
#define AFE4420_BIAS_OTP					0x27
#define AFE4420_DESIGN_ID                   0x28
#define AFE4420_GPIO2_SDOUT					0x29
#define AFE4420_INT0_STC					0x34
#define AFE4420_INT0_ENDC					0x35
#define AFE4420_INT_MUX						0x42
#define AFE4420_GPIO1						0x4B
#define AFE4420_FIFO_CONF					0x51
#define AFE4420_INT1_STC					0x57
#define AFE4420_INT1_ENDC					0x58
#define AFE4420_PDN_BG_DSLEEP				0x6C
#define AFE4420_POINTER_DIFF				0x6D
#define AFE4420_TIMEREN_Z                   0x73	/* added for AFE4420 REV2. */
#define AFE4420_FILTER_BW					0x78
#define AFE4420_HIGH_LOW_THRESHOLD			0x80
#define AFE4420_NR_RESET_NUMPHASE			0x88
#define AFE4420_TACT_TDSLEEP				0x8A
#define AFE4420_CONTROL2					0x8B
#define AFE4420_TWINDOW_MIN					0x8C
#define AFE4420_TW_TACT_DATA_RDY			0x8D
#define AFE4420_TDSLEEP_TACT_PWDN			0x8E
#define AFE4420_EXT_CLK_FREQ				0x8F
#define AFE4420_AACM_CONF					0x93
#define AFE4420_ILED_TX2_1					0xAC
#define AFE4420_ILED_TX4_3					0xAE
#define AFE4420_THR_DET_LOGIC				0xB4
#define AFE4420_FIFO_DATA					0xFF

/*Per-PD Controls*/
#define AFE4420_PDCNTRL0(x)		(0x98 + (x * 4))
#define AFE4420_PDCNTRL1(x)		(0x99 + (x * 4))
#define AFE4420_PDCNTRL2(x)		(0x9a + (x * 4))

/*Per-Phase Controls*/
#define AFE4420_PHASECNTRL0(x)		(0xb8 + (x * 3))
#define AFE4420_PHASECNTRL1(x)		(0xb9 + (x * 3))
#define AFE4420_PHASECNTRL2(x)		(0xba + (x * 3))

#define AFE4420_MAX_REG						0xE8

#define AFE4420_FIFO_REG                    0xFF

#define AFE4420_I2C_RETRY_DELAY				10
#define AFE4420_I2C_MAX_RETRIES				2

#define HRM_DATA_CNT				1
#define AWB_DATA_CNT				20
#define PHASE_PER_SAMPLES			4
#define NUM_BYTES_PER_SAMPLE		3
#define FLICKER_DATA_CNT					200
#define AFE4420_IOCTL_MAGIC					0xFD
#define AFE4420_IOCTL_READ_FLICKER			_IOR(AFE4420_IOCTL_MAGIC, 0x01, int *)

#define HRM_CLOCK					128000	/* internal clock 128khz */

#define ILED1_SHIFT					0	/* LED1 current reg */
#define ILED2_SHIFT					12	/* LED2 current reg */
#define ILED3_SHIFT					0	/* LED3 current reg */
#define ILED4_SHIFT					12	/* LED4 current reg */

#define ILED1_MASK                  0xFF	/* LED1 current - mask bits */
#define ILED2_MASK                  0xFF000	/* LED2 current - mask bits */
#define ILED3_MASK                  0xFF	/* LED3 current - mask bits */
#define ILED4_MASK                  0xFF000	/* LED4 current - mask bits */

#define IOFFDAC_MASK                  0x00FFFF	/* IOFFDAC - mask bits */
#define TIA_CF_MASK                   0xFFE3FF	/* RF TAIA CAP - mask bits */
#define TIA_RF_MASK                   0xFFFF0F	/* RF TAIA gain - mask bits */
#define TWLED_MASK                    0xFFFF00	/* TWLED - mask bits */
#define NUMAV_MASK                    0xFFFFF0	/* NUMAV - mask bits */

/* AFE4420_CONTROL0 bit */
#define TM_COUNT_RST				1
#define SW_RESET					3
#define FIFO_EN						6

/* AFE4420_CONTROL1 bit */
#define PD_DISCONNECT				23
#define TIMER_EN					23
#define ILED_2X						17
#define EN_AACM_GBL					15
#define IFS_OFFDAC					10
#define PDNAFE						0

/* PD_CONTROL BIT */
/* POL,BASE, FREEZ_AACM,NUMPHASE, EN_ACCM */
#define POL_OFFDAC_BASE					23
#define IOFFDAC_BASE					16
#define FREEZE_AACM						10
#define NUMPHASE_AACM					4
#define EN_AACM							0
/* CALIB_AACM BIT */
#define CALIB_AACM						0
/* CALIB INUT BIT */
/* POL_OFFDAC_ACCM_READ, IOFFDAC_AACM_READ */
#define POL_OFFDAC_ACCM_READ			8
#define IOFFDAC_AACM_READ				1

/*  PHASECNTRL0 control bit */
#define PCNTRL0_LED_DRV1_TX1		0
#define PCNTRL0_LED_DRV1_TX2		1
#define PCNTRL0_LED_DRV1_TX3		2
#define PCNTRL0_LED_DRV1_TX4		3
#define PCNTRL0_LED_DRV2_TX1		8
#define PCNTRL0_LED_DRV2_TX2		9
#define PCNTRL0_LED_DRV2_TX3		10
#define PCNTRL0_LED_DRV2_TX4		11

#define LED_IR_DRV1					PCNTRL0_LED_DRV1_TX1
#define LED_GREEN_DRV1				PCNTRL0_LED_DRV1_TX2
#define LED_BLUE_DRV1				PCNTRL0_LED_DRV1_TX3
#define LED_RED_DRV1				PCNTRL0_LED_DRV1_TX4
#define LED_IR_DRV2					PCNTRL0_LED_DRV2_TX1
#define LED_GREEN_DRV2				PCNTRL0_LED_DRV2_TX2
#define LED_BLUE_DRV2				PCNTRL0_LED_DRV2_TX3
#define LED_RED_DRV2				PCNTRL0_LED_DRV2_TX4

#define LED_IR_200MA_DRV	(0x010000|((1 << LED_IR_DRV1)|(1 << LED_IR_DRV2)))
#define LED_RED_200MA_DRV	(0x010000|((1 << LED_RED_DRV1)|(1 << LED_RED_DRV2)))
#define LED_GREEN_200MA_DRV	(0x010000|((1 << LED_GREEN_DRV1)|(1 << LED_GREEN_DRV2)))
#define LED_BLUE_200MA_DRV	(0x010000|((1 << LED_BLUE_DRV1)|(1 << LED_BLUE_DRV2)))
#define LED_AMBIENT				0x010000

#define PCNTRL0_PD_ON1				16
#define PCNTRL0_PD_ON2				17
#define PCNTRL0_PD_ON3				18
#define PCNTRL0_PD_ON4				19

/*  PHASECNTRL1 control bit */
#define POL_OFFDAC					23
#define I_OFFDAC					16
#define TIA_GAIN_CF					10
#define TIA_GAIN_RF					4
#define NUMAV						0

/*  PHASECNTRL2 control bit */
#define PCNTRL2_TWLED				0
#define PCNTRL2_MASK_FACTOR			9
#define PCNTRL2_STAGGER_LED			12
#define PCNTRL2_THR_SEL_DATA_CTRL	15
#define PCNTRL2_FIFO_DATA_CTRL		17

/*  AACM register fields */
#define sfh7832_AACM_IMM_REFRESH	0
#define sfh7832_AACM_QUICK_CONV		1

/* AGC */
#define CONSTRAINT_VIOLATION		-2

#define FULL_SCALE 2097152	/* if numav = 3, max value is 2097152 */

#define SFH7832_AGC_ERROR_RANGE1				8
#define SFH7832_AGC_ERROR_RANGE2				28
#define SFH7832_AGC_DEFAULT_LED_OUT_RANGE		70
#define SFH7832_AGC_DEFAULT_MIN_NUM_PERCENT	10
#define SFH7832_THRESHOLD_DEFAULT		1000000

#define AGC_LED_SKIP_CNT		16
#define MAX_LED_NUM				4
#define LED_ALL					4

#define AGC_IR					0
#define AGC_RED					1
#define AGC_GREEN				2
#define AGC_BLUE				3

#define AGC_SKIP_CNT			5

/* #define SFH7832_PRF_100HZ 0x4FF
 * #define SFH7832_PRF_400HZ 0x13F
 */
#define SFH7832_PRF_100HZ 0x500
#define SFH7832_PRF_200HZ 0x280
#define SFH7832_PRF_300HZ 0x1AA
#define SFH7832_PRF_400HZ 0x140

#define SFH7832_DEFAULT_NUMAV 2

#define SFH7832_MIN_CURRENT	0
#define SFH7832_MAX_CURRENT	150000			/* max 150 mA */
#define SFH7832_CURRENT_PER_STEP	980		/* E2X */

#define SFH7832_MIN_DIODE_VAL	0
#define SFH7832_MAX_DIODE_VAL	2096191 /* numav = 2, if numav =3 ,max value is 2097152 */

#define LSB		1747626
#define uAtoA	1000000

#define CONVERTER (2 * LSB / uAtoA)

#define SFH7832_IR_INIT_CURRENT			0x2A	/* IR */
#define SFH7832_RED_INIT_CURRENT		0x27	/* RED */
#define SFH7832_GREEN_INIT_CURRENT		0x18	/* GREEN */
#define SFH7832_BLUE_INIT_CURRENT		0x24	/* BLUE */

#define PHASE1_INDEX  0
#define PHASE2_INDEX  (PHASE1_INDEX+3)
#define PHASE3_INDEX  (PHASE2_INDEX+3)
#define PHASE4_INDEX  (PHASE3_INDEX+3)
#define PHASE5_INDEX  (PHASE4_INDEX+3)
#define PHASE6_INDEX  (PHASE5_INDEX+3)
#define PHASE7_INDEX  (PHASE6_INDEX+3)
#define PHASE8_INDEX  (PHASE7_INDEX+3)
#define PHASE9_INDEX  (PHASE8_INDEX+3)
#define PHASE10_INDEX  (PHASE9_INDEX+3)
#define PHASE11_INDEX  (PHASE10_INDEX+3)
#define PHASE12_INDEX  (PHASE11_INDEX+3)
#define PHASE13_INDEX  (PHASE12_INDEX+3)

/* trimming register positions */
#define TRM_DONE_REG			0x38
#define TRM_DONE_LSB			16
#define TRM_GLOBAL_REG			0x41
#define TRM_GLOBAL_LSB			2
#define TRM_GLOBAL_NB			5
#define TRM_LED_NB				3
#define TRM_LED1_REG			0x41
#define TRM_LED1_LSB			10
#define TRM_LED4_REG			0x41
#define TRM_LED4_LSB			7
#define TRM_LED3_REG			0x38
#define TRM_LED3_LSB			17
#define TRM_LED2_REG			0x38
#define TRM_LED2_LSB			20

/* trimming parameters */
#define LED_MAX_CUR				255
#define LED_MAX_CODE		    0xCC

#define TRMN0                   5
#define TRMN1                   3
#define TRM_GLOBAL				1        /* -> not used */
#define TRM_1					3        /* -> covers +- 45% */
#define TRM_4					3
#define TRM_3					3
#define TRM_2					3

#define TRM_B1                  750 /* 709 , 606*/
#define TRM_B2                  200 /* 206 , 248*/
#define TRM_B3                  340 /* 333 , 362*/
#define TRM_B4                  950 /* 842 , 793*/

#define SFH_EOL_RESULT 1024
#define SFH_EOL_SPEC 36
#define SFH_EOL_XTALK_SPEC 2

/* End of Line Test */
#define SELF_IR_CH		0
#define SELF_RED_CH		1
#define SELF_GREEN_CH		2
#define SELF_BLUE_CH		3
#define SELF_MAX_CH_NUM		4

enum _EOL_STATE_TYPE {
	_EOL_STATE_TYPE_INIT = 0,
	_EOL_STATE_TYPE_FLICKER_INIT,	/* step 1. flicker_step. */
	_EOL_STATE_TYPE_FLICKER_MODE,
	_EOL_STATE_TYPE_DC_INIT,	/* step 2. dc_step. */
	_EOL_STATE_TYPE_DC_MODE_MID,
	_EOL_STATE_TYPE_DC_MODE_HIGH,
	_EOL_STATE_TYPE_DC_IOFFDAC,
	_EOL_STATE_TYPE_END,
	_EOL_STATE_TYPE_STOP,
	_EOL_STATE_TYPE_XTALK_MODE,
} _EOL_STATE_TYPE;

/* turnning parameter of dc_step. */
#define EOL_MODE_DC_MID				0
#define EOL_MODE_DC_HIGH			1
#define EOL_MODE_DC_IOFFDAC			2
#define EOL_HRM_ARRY_SIZE			3

/* turnning parameter of skip count */
#define EOL_START_SKIP_CNT			20

/* turnning parameter of flicker_step. */
#define EOL_FLICKER_SKIP_CNT		20

#define EOL_DC_MIDDLE_SKIP_CNT		20
#define EOL_DC_HIGH_SKIP_CNT		20
#define EOL_DC_IOFFDAC_SKIP_CNT		20
#define EOL_STD_TOTAL_CNT           80

#define EOL_XTALK_SKIP_CNT		50

/* total buffer size. */
#define EOL_HRM_SIZE			400	/* <=this size should be lager than below size. */
#define EOL_FLICKER_SIZE		400
#define EOL_DC_MIDDLE_SIZE		80
#define EOL_DC_HIGH_SIZE		80
#define EOL_DC_IOFFDAC_SIZE		80
#define EOL_STD2_SIZE           5
#define EOL_STD2_BLOCK_SIZE		16
#define EOL_XTALK_SIZE			50

/* E2X mode 250mA */
#define EOL_DC_MID_LED_IR_CURRENT		0x66 /*100mA */
#define EOL_DC_MID_LED_RED_CURRENT		0x66
#define EOL_DC_MID_LED_GREEN_CURRENT	0x66
#define EOL_DC_MID_LED_BLUE_CURRENT		0x66
#define EOL_DC_HIGH_LED_IR_CURRENT		0x99 /*150mA */
#define EOL_DC_HIGH_LED_RED_CURRENT		0x99
#define EOL_DC_HIGH_LED_GREEN_CURRENT	0x99
#define EOL_DC_HIGH_LED_BLUE_CURRENT	0x99
#define EOL_DC_SUPER_HIGH_LED_IR_CURRENT		0xCC /*200mA */
#define EOL_DC_SUPER_HIGH_LED_RED_CURRENT		0xCC
#define EOL_DC_SUPER_HIGH_LED_GREEN_CURRENT		0xCC
#define EOL_DC_SUPER_HIGH_LED_BLUE_CURRENT		0xCC

#define EOL_HRM_COMPLETE_ALL_STEP		0x7
#define EOL_SUCCESS						0x1
#define EOL_TYPE						0x1

#define EOL_ITEM_CNT			18
#define EOL_SYSTEM_NOISE		"system noise"
#define EOL_IR_MID				"IR Mid DC Level"
#define EOL_RED_MID				"RED Mid DC Level"
#define EOL_GREEN_MID			"GREEN Mid DC Level"
#define EOL_BLUE_MID			"BLUE Mid DC Level"
#define EOL_IR_HIGH				"IR High DC Level"
#define EOL_RED_HIGH			"RED High DC Level"
#define EOL_GREEN_HIGH			"GREEN High DC Level"
#define EOL_BLUE_HIGH			"BLUE High DC Level"
#define EOL_IR_OFFDAC			"IR X-talk Cancell DC Level"
#define EOL_RED_OFFDAC			"RED X-talk Cancell DC Level"
#define EOL_GREEN_OFFDAC		"GREEN X-talk Cancell DC Level"
#define EOL_BLUE_OFFDAC			"BLUE X-talk Cancell DC Level"
#define EOL_IR_NOISE			"IR Noise"
#define EOL_RED_NOISE			"RED Noise"
#define EOL_GREEN_NOISE			"GREEN Noise"
#define EOL_BLUE_NOISE			"BLUE Noise"
#define EOL_FREQUENCY			"Frequency"
#define EOL_HRM_XTALK			"HRM WINDOW X-TALK"

#define EOL_PASS_FAIL		0
#define EOL_MEASURE			1
#define EOL_SPEC_MIN		2
#define EOL_SPEC_MAX		3
#define EOL_SPEC_NUM_MAX	4

#define EOL_15_MODE		0
#define EOL_SEMI_FT		1
#define EOL_XTALK		2

/* step flicker mode */
struct sfh7832_eol_flicker_data {
	int count;
	int index;
	s64 max;
	s64 buf[SELF_RED_CH][EOL_FLICKER_SIZE];
	s64 sum[SELF_RED_CH];
	s64 average[SELF_RED_CH];
	s64 std_sum[SELF_RED_CH];
	s64 std[SELF_RED_CH];
	struct timeval start_time_t;
	struct timeval work_time_t;
	int done;
};
/* step dc level */
struct sfh7832_eol_hrm_data {
	int count;
	int index;
	int ir_current;
	int red_current;
	int green_current;
	int blue_current;
	s64 buf[SELF_MAX_CH_NUM][EOL_HRM_SIZE];
	s64 sum[SELF_MAX_CH_NUM];
	s64 average[SELF_MAX_CH_NUM];
	s64 std_sum[SELF_MAX_CH_NUM];
	s64 std[SELF_MAX_CH_NUM];
	s64 std_sum2[SELF_MAX_CH_NUM];
	int done;
};
/* step freq_step */
struct sfh7832_eol_freq_data {
	int state;
	u64 frequency;
	unsigned long end_time;
	unsigned long start_time;
	struct timeval start_time_t;
	struct timeval work_time_t;
	int count;
	int skip_count;
	int done;
};

struct sfh7832_eol_xtalk_data {
	int count;
	int index;
	s64 buf[EOL_XTALK_SIZE];
	s64 sum;
	s64 average;
	int done;
};

struct sfh7832_eol_item_data {
	char test_type[10];
	s32 system_noise[EOL_SPEC_NUM_MAX];
	s32 ir_mid[EOL_SPEC_NUM_MAX];
	s32 red_mid[EOL_SPEC_NUM_MAX];
	s32 green_mid[EOL_SPEC_NUM_MAX];
	s32 blue_mid[EOL_SPEC_NUM_MAX];
	s32 ir_high[EOL_SPEC_NUM_MAX];
	s32 red_high[EOL_SPEC_NUM_MAX];
	s32 green_high[EOL_SPEC_NUM_MAX];
	s32 blue_high[EOL_SPEC_NUM_MAX];
	s32 ir_offdac[EOL_SPEC_NUM_MAX];
	s32 red_offdac[EOL_SPEC_NUM_MAX];
	s32 green_offdac[EOL_SPEC_NUM_MAX];
	s32 blue_offdac[EOL_SPEC_NUM_MAX];
	s32 ir_noise[EOL_SPEC_NUM_MAX];
	s32 red_noise[EOL_SPEC_NUM_MAX];
	s32 green_noise[EOL_SPEC_NUM_MAX];
	s32 blue_noise[EOL_SPEC_NUM_MAX];
	s32 frequency[EOL_SPEC_NUM_MAX];
	s32 xtalk[EOL_SPEC_NUM_MAX];
};

struct sfh7832_eol_data {
	int eol_count;
	u8 state;
	int eol_hrm_flag;
	struct timeval start_time;
	struct timeval end_time;
	struct sfh7832_eol_flicker_data eol_flicker_data;
	struct sfh7832_eol_hrm_data eol_hrm_data[EOL_HRM_ARRY_SIZE];
	struct sfh7832_eol_freq_data eol_freq_data;	/*test the ir routine. */
	struct sfh7832_eol_xtalk_data eol_xtalk_data;
	struct sfh7832_eol_item_data eol_item_data;
};

static const long long gain[11] = { 10, 25, 50, 100, 166, 200, 250, 500, 1000, 1500, 2000 };
static const long long ioff_step[8] = { 125, 250, 0, 500, 0, 1000, 0, 2000 };

/* nonlinearity correction LUT */
static const uint32_t trm_lut[4][33] = {{0, 231, 219, 214, 211, 210, 210, 211, 212, 213, 214, 216, 217, 219, 221, 223, 224, 226, 228, 230, 232, 234, 236, 238, 240, 242, 244, 247, 249, 251, 253, 256, 258},
								{0, 93, 104, 114, 123, 131, 138, 145, 151, 157, 162, 168, 173, 178, 182, 187, 192, 196, 201, 205, 209, 214, 218, 222, 226, 231, 235, 239, 243, 247, 251, 256, 260},
								{0, 126, 133, 139, 145, 151, 157, 162, 167, 172, 176, 181, 185, 189, 193, 198, 201, 205, 209, 213, 216, 220, 224, 228, 231, 235, 238, 242, 245, 249, 252, 256, 259},
								{0, 224, 220, 218, 217, 217, 218, 220, 221, 222, 223, 225, 226, 227, 229, 231, 232, 234, 235, 236, 238, 239, 241, 243, 244, 246, 248, 249, 251, 252, 254, 256, 257} };
enum {
	M_HRM,
	M_SDK,
	M_NONE
};

/* AGC */
enum sfh7832_pds {
	PD1 = 0,
	PD2,
	PD3,
	PD4,
};

enum sfh7832_phases {
	PHASE1 = 0,
	PHASE2,
	PHASE3,
	PHASE4,
	PHASE5,
	PHASE6,
	PHASE7,
	PHASE8,
	PHASE9,
	PHASE10,
	PHASE11,
	PHASE12,
	PHASE13,
	PHASE14,
	PHASE15,
	PHASE16,
};

enum sfh7832_TIA_GAIN {
	TIA_GAIN_10K = 0,
	TIA_GAIN_25K,
	TIA_GAIN_50K,
	TIA_GAIN_100K,
	TIA_GAIN_166K,
	TIA_GAIN_200K,
	TIA_GAIN_250K,
	TIA_GAIN_500K,
	TIA_GAIN_1M,
	TIA_GAIN_15M,
	TIA_GAIN_2M,
};

enum sfh7832_TIA_CAP {
	TIA_CAP_2_5P = 0,
	TIA_CAP_5P,
	TIA_CAP_7_5P,
	TIA_CAP_10P,
	TIA_CAP_17_5P,
	TIA_CAP_20P,
	TIA_CAP_22_5P,
	TIA_CAP_25P,
};

enum sfh7832_IFS_OFFDAC {
	TIA_IFS_OFFDAC_15_875uA = 0,
	TIA_IFS_OFFDAC_31_75uA,
	TIA_IFS_OFFDAC_63_5uA = 3,
	TIA_IFS_OFFDAC_127uA = 5,
	TIA_IFS_OFFDAC_254uA = 7,
};

enum sfh7832_NUMAV {
	NUMAV1 = 0,
	NUMAV2,
	NUMAV3,
	NUMAV4,
	NUMAV5,
	NUMAV6,
	NUMAV7,
	NUMAV8,
	NUMAV9,
	NUMAV10,
	NUMAV11,
	NUMAV12,
	NUMAV13,
	NUMAV14,
	NUMAV15,
	NUMAV16,
};

enum sfh7832_FIFO_DATA_CTRL {
	PHASE_N = 0,
	NO_DATA,
	PHASE_N_MINUS1,
	PHASE_N_MINUS1_AVG,
};

/* AFE4420 initial values */
static const unsigned int afe4420_reg[][2] = {
	{AFE4420_CONTROL1, 0x068C00},	/* PD_DISCONNECT, ILED_2X , EN_AACM_GBL, IFS_OFFDAC, internal OSC */
	{AFE4420_PDN_BG_DSLEEP, 0x200000},	/* PDN_BG_IN_DEEP_SLEEP */
	{AFE4420_FILTER_BW, 0x00000D},	/* FILTER_BW */
	{AFE4420_TACT_TDSLEEP, 0x026016},	/* REG_TACTIVE_PWRUP , REG_TDEEP_SLEEP_PWRUP */
	{AFE4420_CONTROL2, 0x030102},	/* REG_TLED_SAMP, REG_TSEP_CONV_LED, REG_TSEP */
	{AFE4420_TWINDOW_MIN, 0x0},	/* REG_TWINDOW_MIN */
	{AFE4420_TW_TACT_DATA_RDY, 0x000003},	/* REG_TW_DATA_RDY, REG_TACTIVE_DATA_RDY */
	{AFE4420_TDSLEEP_TACT_PWDN, 0x005001},	/* REG_TDEEP_SLEEP_PWDN, REG_TACTIVE_PWDN */
	{AFE4420_AACM_CONF, 0x000003},	/* CHANNEL_OFFSET_AACM , QUICK/IMM_REFRESH_AACM */
	{AFE4420_PDCNTRL0(0), 0x800001},	/* IOFFDAC PD1 */
	{AFE4420_PDCNTRL1(0), 0x00002B},	/* CALIB_AACM_PD1 */
	{AFE4420_MAX_REG, 0x0},
};

/* AFE4420 phase values for phase13 */
static unsigned int afe4420_phase13_reg[][2] = {    /* D,A,I,R,A,G,B */
	{AFE4420_PHASECNTRL0(PHASE1), 0x010000},		/* phase1 dummy */
	{AFE4420_PHASECNTRL1(PHASE1), 0x801C10},
	{AFE4420_PHASECNTRL2(PHASE1), 0x020008},		/* stagger=0, 70.3us*/
	{AFE4420_PHASECNTRL0(PHASE2), 0x010000},		/* phase2 amb_mask */
	{AFE4420_PHASECNTRL1(PHASE2), 0x801C00},
	{AFE4420_PHASECNTRL2(PHASE2), 0x021607},
	{AFE4420_PHASECNTRL0(PHASE3), LED_AMBIENT},		/* phase3 AMBIENT */
	{AFE4420_PHASECNTRL1(PHASE3), 0x801C12},		/* 25pF, 25kohm, numav=3  */
	{AFE4420_PHASECNTRL2(PHASE3), 0x000008},		/* stagger=0, 70.3us*/
	{AFE4420_PHASECNTRL0(PHASE4), 0x010000},		/* phase4 mask */
	{AFE4420_PHASECNTRL1(PHASE4), 0x801C00},
	{AFE4420_PHASECNTRL2(PHASE4), 0x020607},
	{AFE4420_PHASECNTRL0(PHASE5), LED_IR_200MA_DRV},	/* phase5 LED1 - IR */
	{AFE4420_PHASECNTRL1(PHASE5), 0x801C12},		/* 25pF, 25kohm, numav=3  */
	{AFE4420_PHASECNTRL2(PHASE5), 0x000008},		/* stagger=0, 70.3us*/
	{AFE4420_PHASECNTRL0(PHASE6), 0x010000},		/* phase6 mask */
	{AFE4420_PHASECNTRL1(PHASE6), 0x801C00},
	{AFE4420_PHASECNTRL2(PHASE6), 0x020607},
	{AFE4420_PHASECNTRL0(PHASE7), LED_RED_200MA_DRV},		/* phase7 LED4 - RED */
	{AFE4420_PHASECNTRL1(PHASE7), 0x801C12},		/* 25pF, 25kohm, numav=3  */
	{AFE4420_PHASECNTRL2(PHASE7), 0x000008},		/* stagger=0, 70.3us*/
	{AFE4420_PHASECNTRL0(PHASE8), 0x010000},		/* phase8 mask */
	{AFE4420_PHASECNTRL1(PHASE8), 0x801C00},
	{AFE4420_PHASECNTRL2(PHASE8), 0x020607},
	{AFE4420_PHASECNTRL0(PHASE9), LED_AMBIENT},		/* phase9 AMBIENT */
	{AFE4420_PHASECNTRL1(PHASE9), 0x801C22},		/* 25pF, 50kohm, numav=3  */
	{AFE4420_PHASECNTRL2(PHASE9), 0x000008},		/* stagger=0, 70.3us*/
	{AFE4420_PHASECNTRL0(PHASE10), 0x010000},		/* phase10 mask */
	{AFE4420_PHASECNTRL1(PHASE10), 0x801C00},
	{AFE4420_PHASECNTRL2(PHASE10), 0x020607},
	{AFE4420_PHASECNTRL0(PHASE11), LED_GREEN_200MA_DRV},	/* phase11 LED2 - GREEN */
	{AFE4420_PHASECNTRL1(PHASE11), 0x801C22},		/* 25pF, 50kohm, numav=3  */
	{AFE4420_PHASECNTRL2(PHASE11), 0x000008},		/* stagger=0, 70.3us*/
	{AFE4420_PHASECNTRL0(PHASE12), 0x010000},		/* phase12 mask */
	{AFE4420_PHASECNTRL1(PHASE12), 0x801C00},
	{AFE4420_PHASECNTRL2(PHASE12), 0x020607},
	{AFE4420_PHASECNTRL0(PHASE13), LED_BLUE_200MA_DRV},		/* phase13 LED3 - BLUE */
	{AFE4420_PHASECNTRL1(PHASE13), 0x801C22},		/* 25pF, 50kohm, numav=3  */
	{AFE4420_PHASECNTRL2(PHASE13), 0x000008},		/* stagger=0, 70.3us*/
	{AFE4420_MAX_REG, 0x0},
};

static unsigned int cal_aacm_pd_table[5][11] = {
	/*10K, 25K,  50K,  100K,  166K,  200K,  250K, 500K,  1M,   1.5M,   2M */
	{-1, -1, 21, 43, 71, 85, 107, 213, 427, 640, 853},	/*1x */
	{-1, 21, 43, 85, 142, 171, 213, 427, 853, 1280, -1},	/*2x */
	{-1, 43, 85, 171, 283, 341, 427, 853, -1, -1, -1},	/*4x */
	{34, 85, 171, 341, 567, 683, 853, -1, -1, -1, -1},	/*8x */
	{68, 171, 341, 683, 1133, 1365, -1, -1, -1, -1, -1},	/*16x */
};

enum PART_TYPE_ {
	PART_TYPE_SFH7831 = 0,
	PART_TYPE_SFH7832,
} PART_TYPE;

enum {
	DEBUG_REG_STATUS = 1,
	DEBUG_ENABLE_AGC,
	DEBUG_DISABLE_AGC,
	DEBUG_SET_TIA_GAIN,
	DEBUG_SET_TIA_CF,
	DEBUG_SET_IOFF_DAC,
	DEBUG_SET_CURRENT_RANGE,
	DEBUG_SET_NUMAV,
	DEBUG_GET_LED_TRIM,
};

static u8 agc_is_enabled = 1;

enum op_mode {
	MODE_NONE = 0,
	MODE_HRM = 1,
	MODE_AMBIENT = 2,
	MODE_PROX = 3,
	MODE_HRMLED_IR = 4,
	MODE_HRMLED_RED = 5,
	MODE_HRMLED_BOTH = 6,
	MODE_SDK_IR = 10,
	MODE_SDK_RED = 11,
	MODE_SDK_GREEN = 12,
	MODE_SDK_BLUE = 13,
	MODE_SVC_IR = 14,
	MODE_SVC_RED = 15,
	MODE_SVC_GREEN = 16,
	MODE_SVC_BLUE = 17,
	MODE_UNKNOWN = 18,
};

struct mode_count {
	s32 hrm_cnt;
	s32 amb_cnt;
	s32 prox_cnt;
	s32 sdk_cnt;
	s32 cgm_cnt;
	s32 unkn_cnt;
};

struct output_data {
	enum op_mode mode;
	s32 main_num;
	s32 data_main[20];
};

struct sfh7832_device_data {
	struct device *dev;
	struct input_dev *hrm_input_dev;
	struct mutex i2clock;
	struct mutex activelock;
	struct mutex suspendlock;
	struct pinctrl *hrm_pinctrl;
	struct pinctrl_state *pins_sleep;
	struct pinctrl_state *pins_idle;
	char *vdd_1p8;
	char *i2c_1p8;
	enum op_mode enabled_mode;
	u8 mode_sdk_enabled;
	u8 mode_svc_enabled;
	u8 regulator_state;
	s32 pin_hrm_int;
	s32 pin_hrm_en;
	s32 hrm_irq;
	atomic_t irq_state;
	u32 led_current;
	u32 xtalk_code;
	s32 hrm_threshold;
	s32 prox_threshold;
	u8 always_on;
	s32 eol_test_is_enable;
	u8 eol_test_status;
	u8 eol_test_type;
	u32 reg_read_buf;
	u8 debug_mode;
	u32 debug_value;
	struct mode_count mode_cnt;
	char *lib_ver;
#ifdef CONFIG_ARCH_QCOM
	struct pm_qos_request pm_qos_req_fpm;
#endif
	bool pm_state;

	struct i2c_client *client;
	struct mutex flickerdatalock;
	struct miscdevice miscdev;

	char *eol_test_result;
	u32 eol_spec[SFH_EOL_SPEC];
	u32 eol_semi_spec[SFH_EOL_SPEC];
	u32 eol_xtalk_spec[SFH_EOL_XTALK_SPEC];
	int *flicker_data;
	int flicker_data_cnt;
	int *hrm_data;
	u8 *recvData;
	int hrm_data_cnt;
	u8 fifo_depth;
	u8 n_phase;
	u32 i2c_err_cnt;
	u32 init_iled[4];

	u8 iled1;
	u8 iled2;
	u8 iled3;
	u8 iled4;

	u32 trim_led1;
	u32 trim_led2;
	u32 trim_led3;
	u32 trim_led4;

	s8 ioffset_led1;
	s8 ioffset_led2;
	s8 ioffset_led3;
	s8 ioffset_led4;

	s8 rfgain_amb1;
	s8 rfgain_amb2;
	s8 rfgain_led1;
	s8 rfgain_led2;
	s8 rfgain_led3;
	s8 rfgain_led4;

	s8 cfgain_amb1;
	s8 cfgain_amb2;
	s8 cfgain_led1;
	s8 cfgain_led2;
	s8 cfgain_led3;
	s8 cfgain_led4;

	s32 num_samples;
	u8 ioff_dac;
	u8 numav;
	u32 prf_count;

	u8 ir_adc;
	u8 red_adc;
	u8 part_type;
	int max_current;

	/* new trimming parameters from OTPs */
	uint8_t isTrimmed;
	uint8_t trim_version; /* 0 - pre-July 2018, 1 - post-July 2018 */

	s8 trmglobal;
	s8 trmled1;
	s8 trmled2;
	s8 trmled3;
	s8 trmled4;

	struct sfh7832_eol_data eol_data;
	/* AGC */
	int agc_mode;
	int sample_cnt;
	int agc_enabled;
	u16 agc_sample_cnt[4];
	int agc_sum[4];
	int agc_current[MAX_LED_NUM];
	int prev_ppg[MAX_LED_NUM];
	int prev_current[MAX_LED_NUM];
	int prev_agc_current[MAX_LED_NUM];

	s32 agc_led_out_percent;
	s32 agc_min_num_samples;
	s32 change_led_by_absolute_count[MAX_LED_NUM];
	s32 reached_thresh[MAX_LED_NUM];
	long long min_base_offset[MAX_LED_NUM];
	s32 remove_cnt[MAX_LED_NUM];
	s32 prev_agc_average[MAX_LED_NUM];

	int (*update_led)(int, int);
	/* AGC */
};

void sfh7832_pin_control(bool pin_set);
static int sfh7832_eol_set_mode(int onoff);
#ifdef CONFIG_ARCH_QCOM
extern int sensors_create_symlink(struct kobject *target, const char *name);
extern void sensors_remove_symlink(struct kobject *target, const char *name);
extern int sensors_register(struct device **dev, void *drvdata,
			    struct device_attribute *attributes[], char *name);
#else
extern int sensors_create_symlink(struct input_dev *inputdev);
extern void sensors_remove_symlink(struct input_dev *inputdev);
extern int sensors_register(struct device *dev, void *drvdata,
			    struct device_attribute *attributes[], char *name);
#endif
extern void sensors_unregister(struct device *dev,
			       struct device_attribute *attributes[]);

extern unsigned int lpcharge;

#endif /* _SFH7832_H_ */
