#ifndef _LINUX_ZINITIX_TS_H
#define _LINUX_ZINITIX_TS_H

#include <linux/device.h>
#include <linux/hrtimer.h>
#include <linux/i2c/zinitix.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#ifdef CONFIG_SEC_DEBUG_TSP_LOG
#include <mach/sec_debug.h>
#endif
#ifdef CONFIG_INPUT_BOOSTER
#include <linux/input/input_booster.h>
#endif

#define TSP_VERBOSE_DEBUG
#define SEC_FACTORY_TEST
#define SUPPORTED_TOUCH_KEY

#ifdef CONFIG_SEC_DEBUG_TSP_LOG
#define tsp_debug_dbg(mode, dev, fmt, ...)	\
({								\
	if (mode) {					\
		dev_dbg(dev, fmt, ## __VA_ARGS__);	\
		sec_debug_tsp_log(fmt, ## __VA_ARGS__);		\
	}				\
	else					\
		dev_dbg(dev, fmt, ## __VA_ARGS__);	\
})

#define tsp_debug_info(mode, dev, fmt, ...)	\
({								\
	if (mode) {							\
		dev_info(dev, fmt, ## __VA_ARGS__);		\
		sec_debug_tsp_log(fmt, ## __VA_ARGS__);		\
	}				\
	else					\
		dev_info(dev, fmt, ## __VA_ARGS__);	\
})

#define tsp_debug_err(mode, dev, fmt, ...)	\
({								\
	if (mode) {					\
		dev_err(dev, fmt, ## __VA_ARGS__);	\
		sec_debug_tsp_log(fmt, ## __VA_ARGS__);	\
	}				\
	else					\
		dev_err(dev, fmt, ## __VA_ARGS__); \
})
#else
#define tsp_debug_dbg(mode, dev, fmt, ...)	dev_dbg(dev, fmt, ## __VA_ARGS__)
#define tsp_debug_info(mode, dev, fmt, ...)	dev_info(dev, fmt, ## __VA_ARGS__)
#define tsp_debug_err(mode, dev, fmt, ...)	dev_err(dev, fmt, ## __VA_ARGS__)
#endif

#ifdef CONFIG_SEC_DVFS
#include <linux/cpufreq.h>
#define TOUCH_BOOSTER_DVFS

#define DVFS_STAGE_TRIPLE       3

#define DVFS_STAGE_DUAL         2
#define DVFS_STAGE_SINGLE       1
#define DVFS_STAGE_NONE         0
#endif

#ifdef TOUCH_BOOSTER_DVFS
#define TOUCH_BOOSTER_OFF_TIME	500
#define TOUCH_BOOSTER_CHG_TIME	500//130
#endif

#define ZINITIX_DEBUG			1
#define TOUCH_POINT_MODE			2

#define MAX_SUPPORTED_FINGER_NUM	5 /* max 10 */
#ifdef SUPPORTED_TOUCH_KEY
#define MAX_SUPPORTED_BUTTON_NUM	2 // 6 /* max 8 */
#define SUPPORTED_BUTTON_NUM		2
#endif

/* Upgrade Method*/
#define TOUCH_ONESHOT_UPGRADE		1
/* if you use isp mode, you must add i2c device :
name = "zinitix_isp" , addr 0x50*/

/* resolution offset */
#define ABS_PT_OFFSET				(-1)

#define TOUCH_FORCE_UPGRADE			1
#define USE_CHECKSUM				1
#define CHECK_HWID				0

#define CHIP_OFF_DELAY				50 /*ms*/
#define CHIP_ON_DELAY				20 /*ms*/
#define FIRMWARE_ON_DELAY			40 /*ms*/

#define DELAY_FOR_SIGNAL_DELAY			30 /*us*/
#define DELAY_FOR_TRANSCATION			50
#define DELAY_FOR_POST_TRANSCATION		10

enum power_control {
	POWER_OFF,
	POWER_ON,
	POWER_ON_SEQUENCE,
};

/* Key Enum */
enum key_event {
	ICON_BUTTON_UNCHANGE,
	ICON_BUTTON_DOWN,
	ICON_BUTTON_UP,
};

/* ESD Protection */
/*second : if 0, no use. if you have to use, 3 is recommended*/
#define ESD_TIMER_INTERVAL			1
#define SCAN_RATE_HZ				100
#define CHECK_ESD_TIMER				3

 /*Test Mode (Monitoring Raw Data) */
#define SEC_DND_N_COUNT				10
#define SEC_DND_FREQUENCY			90
#define SEC_PDND_N_COUNT			14
#define SEC_PDND_U_COUNT			24
#define SEC_PDND_FREQUENCY			59

#define MAX_RAW_DATA_SZ				576 /* 32x18 */
#define MAX_TRAW_DATA_SZ	\
	(MAX_RAW_DATA_SZ + 4*MAX_SUPPORTED_FINGER_NUM + 2)
/* preriod raw data interval */

#define RAWDATA_DELAY_FOR_HOST		100

struct raw_ioctl {
	int sz;
	u8 *buf;
};

struct reg_ioctl {
	int addr;
	int *val;
};

#define TOUCH_SEC_MODE			48
#define TOUCH_REF_MODE			10
#define TOUCH_NORMAL_MODE		5
#define TOUCH_DELTA_MODE		3
#define TOUCH_DND_MODE			6
#define TOUCH_PDND_MODE			11

/*  Other Things */
#define INIT_RETRY_CNT			5
#define I2C_SUCCESS			0
#define I2C_FAIL			1

/*---------------------------------------------------------------------*/

/* Register Map*/
#define BT532_SWRESET_CMD			0x0000
#define BT532_WAKEUP_CMD			0x0001

#define BT532_IDLE_CMD				0x0004
#define BT532_SLEEP_CMD				0x0005

#define BT532_CLEAR_INT_STATUS_CMD		0x0003
#define BT532_CALIBRATE_CMD			0x0006
#define BT532_SAVE_STATUS_CMD			0x0007
#define BT532_SAVE_CALIBRATION_CMD		0x0008
#define BT532_RECALL_FACTORY_CMD		0x000f

#define BT532_THRESHOLD				0x0020

#define BT532_DEBUG_REG				0x0115 /* 0~7 */

#define BT532_TOUCH_MODE			0x0010
#define BT532_CHIP_REVISION			0x0011
#define BT532_FIRMWARE_VERSION			0x0012

#define BT532_MINOR_FW_VERSION			0x0121

#define BT532_VENDOR_ID				0x001C
#define BT532_HW_ID				0x0014

#define BT532_DATA_VERSION_REG			0x0013
#define BT532_SUPPORTED_FINGER_NUM		0x0015
#define BT532_EEPROM_INFO			0x0018
#define BT532_INITIAL_TOUCH_MODE		0x0019

#define BT532_TOTAL_NUMBER_OF_X			0x0060
#define BT532_TOTAL_NUMBER_OF_Y			0x0061

#define BT532_DELAY_RAW_FOR_HOST		0x007f

#define BT532_BUTTON_SUPPORTED_NUM		0x00B0
#define BT532_BUTTON_SENSITIVITY		0x00B2
#define BT532_DUMMY_BUTTON_SENSITIVITY		0X00C8

#define BT532_X_RESOLUTION			0x00C0
#define BT532_Y_RESOLUTION			0x00C1
#define BT532_HOLD_POINT_THRESHOLD		0x00C3

#define BT532_POINT_STATUS_REG			0x0080
#define BT532_ICON_STATUS_REG			0x00AA

#define BT532_AFE_FREQUENCY			0x0100
#define BT532_DND_N_COUNT			0x0122
#define BT532_DND_U_COUNT			0x0135

#define BT532_RAWDATA_REG			0x0200

#define BT532_EEPROM_INFO_REG			0x0018

#define BT532_INT_ENABLE_FLAG			0x00f0
#define BT532_PERIODICAL_INTERRUPT_INTERVAL	0x00f1

#define BT532_BTN_WIDTH				0x016d

#define BT532_CHECKSUM_RESULT			0x012c

#define BT532_INIT_FLASH			0x01d0
#define BT532_WRITE_FLASH			0x01d1
#define BT532_READ_FLASH			0x01d2

#define DEBUG_I2C_CHECKSUM

#ifdef DEBUG_I2C_CHECKSUM
#define	ZINITIX_I2C_CHECKSUM_WCNT	0x016a
#define	ZINITIX_I2C_CHECKSUM_RESULT	0x016c
#define	ZINITIX_INTERNAL_FLAG_02 0x011e
#endif

/* Interrupt & status register flag bit
-------------------------------------------------
*/
#define BIT_PT_CNT_CHANGE	0
#define BIT_DOWN		1
#define BIT_MOVE		2
#define BIT_UP			3
#define BIT_PALM		4
#define BIT_PALM_REJECT		5
#define RESERVED_0		6
#define RESERVED_1		7
#define BIT_WEIGHT_CHANGE	8
#define BIT_PT_NO_CHANGE	9
#define BIT_REJECT		10
#define BIT_PT_EXIST		11
#define RESERVED_2		12
#define BIT_MUST_ZERO		13
#define BIT_DEBUG		14
#define BIT_ICON_EVENT		15

/* button */
#define BIT_O_ICON0_DOWN	0
#define BIT_O_ICON1_DOWN	1
#define BIT_O_ICON2_DOWN	2
#define BIT_O_ICON3_DOWN	3
#define BIT_O_ICON4_DOWN	4
#define BIT_O_ICON5_DOWN	5
#define BIT_O_ICON6_DOWN	6
#define BIT_O_ICON7_DOWN	7

#define BIT_O_ICON0_UP		8
#define BIT_O_ICON1_UP		9
#define BIT_O_ICON2_UP		10
#define BIT_O_ICON3_UP		11
#define BIT_O_ICON4_UP		12
#define BIT_O_ICON5_UP		13
#define BIT_O_ICON6_UP		14
#define BIT_O_ICON7_UP		15


#define SUB_BIT_EXIST		0
#define SUB_BIT_DOWN		1
#define SUB_BIT_MOVE		2
#define SUB_BIT_UP		3
#define SUB_BIT_UPDATE		4
#define SUB_BIT_WAIT		5


#define zinitix_bit_set(val, n)		((val) &= ~(1<<(n)), (val) |= (1<<(n)))
#define zinitix_bit_clr(val, n)		((val) &= ~(1<<(n)))
#define zinitix_bit_test(val, n)	((val) & (1<<(n)))
#define zinitix_swap_v(a, b, t)		((t) = (a), (a) = (b), (b) = (t))
#define zinitix_swap_16(s)		(((((s) & 0xff) << 8) | (((s) >> 8) \
								& 0xff)))

#define TSP_NORMAL_EVENT_MSG 1
static int m_ts_debug_mode = ZINITIX_DEBUG;

struct coord {
	u16	x;
	u16	y;
	u8	width;
	u8	sub_status;
#if (TOUCH_POINT_MODE == 2)
	u8	minor_width;
	u8	angle;
#endif
};

struct point_info {
	u16	status;
#if (TOUCH_POINT_MODE == 1)
	u16	event_flag;
#else
	u8	finger_cnt;
	u8	time_stamp;
#endif
	struct coord coord[MAX_SUPPORTED_FINGER_NUM];
};

#define TOUCH_V_FLIP	0x01
#define TOUCH_H_FLIP	0x02
#define TOUCH_XY_SWAP	0x04

struct capa_info {
	u16	vendor_id;
	u16	ic_revision;
	u16	fw_version;
	u16	fw_minor_version;
	u16	reg_data_version;
	u16	threshold;
	u16	key_threshold;
	//u16	dummy_threshold;
	u32	ic_fw_size;
	u32	MaxX;
	u32	MaxY;
	u32	MinX;
	u32	MinY;
	u8	gesture_support;
	u16	multi_fingers;
	u16	button_num;
	u16	ic_int_mask;
	u16	x_node_num;
	u16	y_node_num;
	u16	total_node_num;
	u16	hw_id;
	u16	afe_frequency;
#ifdef DEBUG_I2C_CHECKSUM
	u16	i2s_checksum;
#endif
};

enum work_state {
	NOTHING = 0,
	NORMAL,
	ESD_TIMER,
	EALRY_SUSPEND,
	SUSPEND,
	RESUME,
	LATE_RESUME,
	UPGRADE,
	REMOVE,
	SET_MODE,
	HW_CALIBRAION,
	RAW_DATA,
};

enum {
	BUILT_IN = 0,
	UMS,
	REQ_FW,
};

struct bt532_ts_info {
	struct i2c_client		*client;
	struct input_dev		*input_dev;
	struct bt532_ts_platform_data	*pdata;
	char				phys[32];
	struct mutex			prob_int_sync;

	struct capa_info		cap_info;
	struct point_info		touch_info;
	struct point_info		reported_touch_info;
	u16				icon_event_reg;
	u16				prev_icon_event;

	int				irq;
	u8				button[MAX_SUPPORTED_BUTTON_NUM];
	u8				work_state;
	struct semaphore		work_lock;

#if ESD_TIMER_INTERVAL
	struct workqueue_struct		*esd_tmr_workqueue;
	struct work_struct		tmr_work;
	struct timer_list		esd_timeout_tmr;
	struct timer_list		*p_esd_timeout_tmr;
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend		early_suspend;
#endif
	struct semaphore		raw_data_lock;
	u16				touch_mode;
	s16				cur_data[MAX_TRAW_DATA_SZ];
	u8				update;

#ifdef SEC_FACTORY_TEST
	struct tsp_factory_info		*factory_info;
	struct tsp_raw_data		*raw_data;
	struct device			*fac_dev_ts;
#ifdef SUPPORTED_TOUCH_KEY
	struct device			*fac_dev_tk;
#endif
#endif

#ifdef TOUCH_BOOSTER_DVFS
	struct delayed_work	work_dvfs_off;
	struct delayed_work	work_dvfs_chg;
	struct mutex		dvfs_lock;
	bool dvfs_lock_status;
	u8								finger_cnt1;
	int dvfs_boost_mode;
	int dvfs_freq;
	int dvfs_old_stauts;
	bool stay_awake;
#endif
};

/*<= you must set key button mapping*/
static u32 BUTTON_MAPPING_KEY[MAX_SUPPORTED_BUTTON_NUM] = {
KEY_MENU, KEY_BACK};
//	KEY_DUMMY_MENU, KEY_MENU, KEY_DUMMY_HOME1,
//	KEY_DUMMY_HOME2, KEY_BACK, KEY_DUMMY_BACK};

#endif				//_LINUX_ZINITIX_TS_H