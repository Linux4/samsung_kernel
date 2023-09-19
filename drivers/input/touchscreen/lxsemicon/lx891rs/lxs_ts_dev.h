/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * lxs_ts_dev.h
 *
 * LXS touch core layer
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _LXS_TS_H_
#define _LXS_TS_H_

#include <linux/version.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/completion.h>
#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/gpio.h>
#include <linux/hrtimer.h>
#include <linux/spi/spi.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/pm_wakeup.h>
#include <linux/workqueue.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/buffer_head.h>
#include <linux/atomic.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <asm/unaligned.h>
#include <asm/uaccess.h>

#include "../../../sec_input/sec_input.h"
#include <sec_charging_common.h>

#if IS_ENABLED(CONFIG_SPI_MT65XX)
#include <linux/platform_data/spi-mt65xx.h>
#endif

#if IS_ENABLED(CONFIG_SEC_PANEL_NOTIFIER)
#include <linux/sec_panel_notifier.h>
#endif

#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
//#define __SUPPORT_SAMSUNG_TUI
#endif

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
//#define __SUPPORT_INPUT_SEC_SECURE_TOUCH
#endif

#if IS_ENABLED(CONFIG_INPUT_TOUCHSCREEN_TCLMV2)
//#define __SUPPORT_TCLM_CONCEPT
#endif

#if defined(__SUPPORT_SAMSUNG_TUI)
#include <linux/input/stui_inf.h>
#endif

#include "lxs_ts_reg.h"

#if IS_ENABLED(CONFIG_MTK_SPI)
/* Please copy mt_spi.h file under mtk spi driver folder */
#include "mt_spi.h"
#elif IS_ENABLED(CONFIG_SPI_MT65XX)
#include <linux/platform_data/spi-mt65xx.h>
#endif

#if IS_ENABLED(CONFIG_SPU_VERIFY)
#include <linux/spu-verify.h>
#define SUPPORT_SIGNED_FW
#endif

#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
#include "../../../sec_input/sec_secure_touch.h"
#include <linux/atomic.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>

#define SECURE_TOUCH_ENABLE	1
#define SECURE_TOUCH_DISABLE	0
#endif


#define input_raw_info_d(mode, dev, fmt, ...) input_raw_info(mode, dev, fmt, ## __VA_ARGS__)

#define USE_HW_RST
#define USE_TOUCH_AUTO_START
#define USE_LOWPOWER_MODE
#define USE_RESET_RETRY_MAX

//#define USE_LCD_TYPE_CHK
//#define USE_INIT_STS_CHECK
//#define USE_RINFO

#define USE_PROXIMITY
//#define USE_GRIP_DATA

//#define USE_FW_MP

#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
#define USE_VBUS_NOTIFIER
#include <linux/vbus_notifier.h>
#endif

#define LXS_RST_PIN	980
#define LXS_INT_PIN	943

#define LXS_TS_NAME			"lxs_ts"
#define LXS_TS_DEV_NAME		"LXS_TS"

#define TOUCH_PRINT_INFO_DWORK_TIME		30000	/* 30s */
#define TOUCH_POWER_ON_DWORK_TIME		10
#define TOUCH_RESET_DWORK_TIME			5

#define MAX_SUPPORT_TOUCH_COUNT		10
#define MAX_SUPPORT_HOVER_COUNT		1

/* SIW_TS_DEBUG : Print event contents */
#define LXS_TS_DEBUG_PRINT_ALLEVENT		0x01
#define LXS_TS_DEBUG_PRINT_ONEEVENT		0x02
#define LXS_TS_DEBUG_PRINT_I2C_READ_CMD	0x04
#define LXS_TS_DEBUG_PRINT_I2C_WRITE_CMD	0x08
#define LXS_TS_DEBUG_SEND_UEVENT			0x80

#define LXS_TS_BIT_CHARGER_MODE_NO							BIT(0)
#define LXS_TS_BIT_CHARGER_MODE_WIRE_CHARGER				BIT(1)
#define LXS_TS_BIT_CHARGER_MODE_WIRELESS_CHARGER			BIT(2)
#define LXS_TS_BIT_CHARGER_MODE_WIRELESS_BATTERY_PACK	BIT(3)

enum switch_system_mode {
	TO_TOUCH_MODE			= 0,
	TO_LOWPOWER_MODE		= 1,
	TO_SELFTEST_MODE		= 2,
	TO_FLASH_MODE			= 3,
};

static inline u32 tc_sts_irq_type(int status)
{
	return ((status >> 16) & 0x0F);
}

static inline u32 tc_sts_running_sts(int status)
{
	return (status & 0x1F);
}

static inline u32 tc_sts_abnormal_type(int status)
{
	return (status >> 27);
}

/* Flag for ESD IRQ */
#define ETESDIRQ		((ENOMEDIUM<<3) + 0x0F)

enum {
	STS_AB_NONE = 0,
	STS_AB_RSVD1,
	STS_AB_TG,
	STS_AB_WD,
	STS_AB_CM,
	STS_AB_ESD_DIC,
	STS_AB_ESD_DSI,
	STS_AB_MAX
};

enum {
	TS_MODE_U0		= 0,
	TS_MODE_U3		= 3,
	TS_MODE_STOP	= 6,
	TS_MODE_MAX,
};

static const char *__tc_driving_mode_strs[TS_MODE_MAX] = {
	[TS_MODE_U0]	= "LP",
	[TS_MODE_U3]	= "NP",
	[TS_MODE_STOP]	= "STOP",
};

static inline const char *ts_driving_mode_str(int mode)
{
	const char *str = NULL;

	if (mode < TS_MODE_MAX)
		str = __tc_driving_mode_strs[mode];

	return (str) ? str : "(invalid)";
}

#define __CLOCK_KHZ(x)		((x) * 1000)
#define __CLOCK_MHZ(x)		((x) * 1000 * 1000)

enum {
	BUS_BUF_MARGIN	= 32,
	BUS_BUF_MAX_SZ	= (32<<10),
	//
	SPI_TX_HDR_SZ	= 2,
	SPI_RX_HDR_SZ	= (4+2),
	SPI_RX_HDR_DUMMY = 4,
	//
	SPI_BPW	= 8,
	SPI_SPEED_HZ = __CLOCK_MHZ(10),
	//
	SPI_RETRY_CNT = 2,
};

static inline int spi_freq_out_of_range(u32 freq)
{
	return ((freq > __CLOCK_MHZ(10)) || (freq < __CLOCK_MHZ(1)));
}

static inline u32 freq_to_mhz_unit(u32 freq)
{
	return (freq / __CLOCK_MHZ(1));
}

static inline u32 freq_to_khz_unit(u32 freq)
{
	return (freq / __CLOCK_KHZ(1));
}

static inline u32 freq_to_khz_top(u32 freq)
{
	return ((freq % __CLOCK_MHZ(1)) / __CLOCK_KHZ(100));
}

static inline int lxs_addr_is_skip(u32 addr)
{
	return (addr >= 0xFFFF);
}

static inline int lxs_addr_is_invalid(u32 addr)
{
	return (!addr || lxs_addr_is_skip(addr));
}

#define MAX_FINGER					10

struct lxs_ts_input_touch_data {
	u16 id;
	u16 x;
	u16 y;
	u16 width_major;
	u16 width_minor;
	s16 orientation;
	u16 pressure;
	/* finger, palm, pen, glove, hover */
	u16 type;
	u16 event;
};

struct lxs_ts_touch_data {
	u8 tool_type:4;
	u8 event:4;
	u8 track_id;
	u16 x;
	u16 y;
	u8 pressure;
	u8 angle;
	u16 width_major;
	u16 width_minor;
} __packed;

struct lxs_ts_report {
	u32 ic_status;
	u32 device_status;
	//
	u32 wakeup_type:8;
	u32 touch_cnt:5;
	u32 prox_sts:3;
	u32 palm_bit:1;
	u32 noise_mode:1;	//0 : Noise Mode, 1 : Normal Mode
	u32 noise_freq:2;	//0 : 210kHz, 1:110kHz, 2:83kHz
	u32 noise_meas:12;	//Measure >> 3
	//
	struct lxs_ts_touch_data data[MAX_FINGER];
} __packed;

struct lxs_ts_lpwg_point {
	int x;
	int y;
};

#define TS_MAX_LPWG_CODE	2
#define TS_MAX_SWIPE_CODE	2

#define PALM_DETECTED	1

enum {
	TS_TOUCH_IRQ_NONE		= 0,
	TS_TOUCH_IRQ_FINGER		= BIT(0),
	TS_TOUCH_IRQ_KNOCK		= BIT(1),
	TS_TOUCH_IRQ_GESTURE	= BIT(2),
	/* */
	TS_TOUCH_IRQ_PROX		= BIT(8),
};

enum {
	TOUCHSTS_IDLE = 0,
	TOUCHSTS_DOWN,
	TOUCHSTS_MOVE,
	TOUCHSTS_UP,
};

enum {
	ABS_MODE = 0,
	KNOCK,
	SWIPE,
	/* */
	PROX = 10,
	PROX_ABS,
	/* */
	CUSTOM_DBG = 200,
};

#define TC_REPORT_BASE_PKT		(1)
#define TC_REPORT_BASE_HDR		(3)

enum {
	TC_STS_IRQ_TYPE_INIT_DONE	= 2,
	TC_STS_IRQ_TYPE_ABNORMAL	= 3,
	TC_STS_IRQ_TYPE_DEBUG		= 4,
	TC_STS_IRQ_TYPE_REPORT		= 5,
};

#define PALM_ID					15

#define STS_RET_ERR				ERANGE	//ERESTART

#define STS_FILTER_FLAG_TYPE_ERROR		(1<<0)

#define _STS_FILTER(_id, _width, _pos, _flag, _str)	\
		{ .id = _id, .width = _width, .pos = _pos, .flag = _flag, .str = _str, }

#define IC_CHK_LOG_MAX		(1<<9)

enum {
	STS_ID_NONE = 0,
	STS_ID_VALID_DEV_CTL,
	STS_ID_VALID_CODE_CRC,
	STS_ID_RSVD03,
	STS_ID_RSVD04,
	STS_ID_ERROR_ABNORMAL,
	STS_ID_ERROR_SYSTEM,
	STS_ID_VALID_IRQ_PIN,
	STS_ID_VALID_IRQ_EN,
	STS_ID_RSVD10,
	STS_ID_VALID_TC_DRV,
	STS_ID_RSVD12,
};

enum {
	STS_POS_VALID_DEV_CTL	= 5,
	STS_POS_VALID_CODE_CRC	= 6,
	STS_POS_ERROR_ABNORMAL	= 9,
	STS_POS_ERROR_SYSTEM		= 10,
	STS_POS_VALID_IRQ_PIN	= 15,
	STS_POS_VALID_IRQ_EN		= 20,
	STS_POS_VALID_TC_DRV		= 22,
};

struct lxs_ts_status_filter {
	int id;
	u32 width;
	u32 pos;
	u32 flag;
	const char *str;
};

/*
 * Check bit for Normal
 */
#define STATUS_MASK_NORMAL	(u32)(0 |	\
		BIT(STS_POS_VALID_DEV_CTL) |	\
		BIT(STS_POS_VALID_CODE_CRC) |	\
		BIT(STS_POS_VALID_IRQ_PIN) |	\
		BIT(STS_POS_VALID_IRQ_EN) |	\
		BIT(STS_POS_VALID_TC_DRV) |	\
		0)

/*
 * Check bit for Logging
 */
#define STATUS_MASK_LOGGING	(u32)(0 |	\
		BIT(STS_POS_VALID_IRQ_PIN) |	\
		BIT(STS_POS_VALID_IRQ_EN) |	\
		BIT(STS_POS_VALID_TC_DRV) |	\
		0)

/*
 * Check bit for Reset
 */
#define STATUS_MASK_RESET	(u32)(0 |	\
		BIT(STS_POS_VALID_DEV_CTL) |	\
		BIT(STS_POS_VALID_CODE_CRC) |	\
		BIT(STS_POS_ERROR_ABNORMAL) |	\
		BIT(STS_POS_ERROR_SYSTEM) |	\
		0)

#define IC_STATUS_MASK_VALID		(u32)(0x0007FFFF)
#define IC_STATUS_MASK_NORMAL		(u32)(0x00060010)	//(u32)(0x00000006)
#define IC_STATUS_MASK_ERROR		(u32)(0x000000A0)	//(u32)(0x00000028)
#define IC_STATUS_MASK_ABNORMAL		(u32)(0x00000000)
//#define IC_STATUS_MASK_ABNORMAL		(u32)(0x00000001)

#define sts_snprintf(_buf, _buf_max, _size, _fmt, _args...) \
		({	\
			int _n_size = 0;	\
			if (_size < _buf_max)	\
				_n_size = snprintf(_buf + _size, _buf_max - _size,\
								(const char *)_fmt, ##_args);	\
			_n_size;	\
		})

enum {
	BIN_VER_OFFSET_POS = 0xE8,
	BIN_COD_OFFSET_POS = 0xEC,
	BIN_PID_OFFSET_POS = 0xF0,
};

struct lxs_ts_version_bin {
	u8 ver;
	u8 module;
	u16 rsvd;
} __packed;

struct lxs_ts_version {
	u8 ver;
	u8 module;
	u8 chip;
	u8 protocol:4;
	u8 rsvd:4;
} __packed;

struct lxs_ts_fw_info {
	u32 chip_id_raw;
	u8 chip_id[8];
	u32 ver_code_bin;
	u32 ver_code_dev;
	u32 bin_dev_code;
	u32 dev_code;
	struct lxs_ts_version_bin bin_ver;
	union {
		struct lxs_ts_version version;
		u32 version_raw;
	} v;
	u8 product_id[8+4];
	u8 revision;
};

enum {
	FW_TYPE_NONE = 0,
	FW_TYPE_TS,
	FW_TYPE_MP,
};

enum {
	BOOT_CHK_FW_RETRY		= 2,
	BOOT_CHK_STS_RETRY	= 2,
	/* */
	BOOT_CHK_FW_DELAY		= 10,
	BOOT_CHK_STS_DELAY	= 10,
};

enum {
	BOOT_CHK_SKIP = (1<<16),
};

enum {
	TC_IC_INIT_NEED = 0,
	TC_IC_INIT_DONE,
};

enum {
	TC_IC_BOOT_DONE = 0,
	TC_IC_BOOT_FAIL,
};

#define TC_RESET_RETRY	10

/* ts->dbg_mask */
enum {
	DBG_LOG_MOVE_FINGER	= BIT(0),
	DBG_LOG_IRQ			= BIT(1),
	/* */
	DBG_LOG_FW_WR_TRACE	= BIT(8),
	DBG_LOG_FW_WR_DBG	= BIT(9),
};

#define SEC_TS_LOCATION_DETECT_SIZE		6
#define SEC_TS_SUPPORT_TOUCH_COUNT		10
#define SEC_TS_GESTURE_REPORT_BUFF_SIZE		20

#define SEC_TS_MODE_SPONGE_SWIPE		(1 << 1)
//#define SEC_TS_MODE_SPONGE_AOD			(1 << 2)
//#define SEC_TS_MODE_SPONGE_SINGLE_TAP	(1 << 3)
//#define SEC_TS_MODE_SPONGE_PRESS		(1 << 4)
#define SEC_TS_MODE_SPONGE_DOUBLETAP_TO_WAKEUP	(1 << 5)

/*SPONGE library parameters*/
#define SEC_TS_MAX_SPONGE_DUMP_BUFFER	512
#define SEC_TS_SPONGE_DUMP_EVENT_MASK	0x7F
#define SEC_TS_SPONGE_DUMP_INF_MASK	0x80
#define SEC_TS_SPONGE_DUMP_INF_SHIFT	7

/* SEC_TS SPONGE OPCODE COMMAND */
#define SEC_TS_CMD_SPONGE_DUMP_FLUSH			0x01
#define SEC_TS_CMD_SPONGE_AOD_ACTIVE_INFO		0x0A
#define SEC_TS_CMD_SPONGE_OFFSET_UTC			0x10
#define SEC_TS_CMD_SPONGE_PRESS_PROPERTY		0x14
#define SEC_TS_CMD_SPONGE_FOD_INFO			0x15
#define SEC_TS_CMD_SPONGE_FOD_POSITION			0x19
#define SEC_TS_CMD_SPONGE_GET_INFO			0x90
#define SEC_TS_CMD_SPONGE_WRITE_PARAM			0x91
#define SEC_TS_CMD_SPONGE_READ_PARAM			0x92
#define SEC_TS_CMD_SPONGE_NOTIFY_PACKET			0x93
#define SEC_TS_CMD_SPONGE_LP_DUMP			0xF0
#define SEC_TS_CMD_SPONGE_LP_DUMP_CUR_IDX		0xF2
#define SEC_TS_CMD_SPONGE_LP_DUMP_EVENT			0xF4

#define SEC_TS_MAX_FW_PATH		64

enum {
	TSP_BUILT_IN = 0,
	TSP_UMS,
	TSP_NONE,
	TSP_SPU,
};

enum {
	TEST_RESULT_PASS = 0,
	TEST_RESULT_FAIL,
};

enum {
	OP_STATE_NP = 0,
	OP_STATE_LP,
	OP_STATE_PROX,
	OP_STATE_STOP,
};

enum {
	POWER_OFF_STATUS = 0,
	POWER_ON_STATUS,
	LP_MODE_STATUS,
	LP_MODE_EXIT,
};

enum {
	MODE_SWITCH_SIP_MODE,
	MODE_SWITCH_GAME_MODE,
	MODE_SWITCH_GLOVE_MODE,
	MODE_SWITCH_COVER_MODE,
	MODE_SWITCH_SENSITIVITY_MODE,
	MODE_SWITCH_DEAD_ZONE_ENABLE,
	/* */
	MODE_SWITCH_SET_TOUCHABLE_AREA,
	MODE_SWITCH_GRIP_EDGE_HANDLER,
	MODE_SWITCH_GRIP_PORTRAIT_MODE,
	MODE_SWITCH_GRIP_LANDSCAPE_MODE,
	/* */
	MODE_SWITCH_WIRELESS_CHARGER,
	/* */
	MODE_SWITCH_PROX_LP_SCAN_MODE,
	MODE_SWITCH_EAR_DETECT_ENABLE,
	/* */
	MODE_SWITCH_SET_NOTE_MODE,
	MODE_SWITCH_MAX,
};

struct lxs_ts_grip_data {
	/* 0 : disable, 1 : left side, 2 : right side */
	int exception_direction;
	int exception_upper_y;		/* upper y coordinate */
	int exception_lower_y;		/* lower y coordinate */
	/* grip zone (both long side) */
	int portrait_mode;
	int portrait_upper;			/* upper reject zone (both long side) */
	int portrait_lower;			/* lower reject zone (both long side) */
	int portrait_boundary_y;	/* reject zone boundary of y (divide upper and lower) */
	/*
	 * 0 : use previous portrait grip and reject settings
	 * 1 : enable landscape mode
	 */
	int landscape_mode;
	int landscape_reject_bl;	/* reject zone (both long side) */
	int landscape_grip_bl;		/* grip zone (both long side) */
	int landscape_reject_ts;	/* reject zone (top short side) */
	int landscape_reject_bs;	/* reject zone (bottom short side) */
	int landscape_grip_ts;		/* grip zone (top short side) */
	int landscape_grip_bs;		/* grip zone (top short side) */
};

#if 0
struct sec_input_notify_data {
	u8 dual_policy;
};
#endif

#define SEC_INPUT_HW_PARAM_SIZE		512

#if IS_ENABLED(CONFIG_EXYNOS_DPU30)
int get_lcd_info(char *arg);
#elif IS_ENABLED(CONFIG_SMCDSD_PANEL)
extern unsigned int lcdtype;
#else
static unsigned int lcdtype;
#endif

#define LP_DUMP_MAX		40

enum {
	LP_TYPE_COORD = 0,
	LP_TYPE_GESTURE,
	LP_TYPE_RSVD2,
	LP_TYPE_RSVD3,
	LP_TYPE_MAX,
};

enum {
	LP_TCH_STS_P = 0,	/* Press */
	LP_TCH_STS_R,		/* Release */
};

enum {
	LP_GESTURE_ID_KNOCK = 0,
	LP_GESTURE_ID_SWIP,
};

struct lxs_ts_lp_dump_coord {
	/* byte-0 */
	u8 type:2;
	u8 status:2;	//LP_TCH_STS_X
	u8 id:2;
	u8 frame_h:2;
	/* byte-1~4 */
	u32 frame_l:8;
	u32 x_h:8;
	u32 y_h:8;
	u32 y_l:4;
	u32 x_l:4;
} __packed;

struct lxs_ts_plat_data {
	int max_x;
	int max_y;

	int reset_gpio;
	int irq_gpio;
	int cs_gpio;
	u32 area_indicator;
	u32 area_navigation;
	u32 area_edge;

	const char *firmware_name_ts;
#if defined(USE_FW_MP)
	const char *firmware_name_mp;
#endif
	const char *regulator_lcd_vdd;
	const char *regulator_lcd_reset;
	const char *regulator_lcd_bl_en;
	const char *regulator_lcd_vsp;
	const char *regulator_lcd_vsn;
	const char *regulator_tsp_reset;

	bool support_spay;
	bool support_ear_detect;
	bool prox_lp_scan_enabled;
#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	int ss_touch_num;
#endif
	bool enable_settings_aot;
	bool enable_sysinput_enabled;

	u32 lcdtype;
};

struct lxs_ts_data {
	struct spi_device *client;
	struct lxs_ts_plat_data *plat_data;

	struct input_dev *input_dev;
	struct input_dev *input_dev_proximity;

	int irq;
	int irq_type;
	int irq_wake;
	int irq_enabled;
	int irq_activate;

	u32 ic_status_mask_error;
	u32 ic_status_mask_abnormal;

	unsigned int palm_flag;
	volatile u8 touch_noise_status;
	volatile u8 touch_pre_noise_status;
	int gesture_id;
	int gesture_x;
	int gesture_y;

	struct pinctrl *pinctrl;

	struct lxs_ts_grip_data grip_data;

//	int device_id;

	volatile bool enabled;
	volatile int power_state;
	volatile bool shutdown_called;
	volatile int op_state;

	int external_noise_mode;
	int touchable_area;
	int lightsensor_detect;
	int cover_type;
	int wirelesscharger_mode;
	bool force_wirelesscharger_mode;
	int wet_mode;

	struct completion resume_done;

	u32 print_info_cnt_release;
	u32 print_info_cnt_open;
	u16 print_info_currnet_mode;

	int buf_size;
	char *tx_buf;
	char *rx_buf;
#if IS_ENABLED(CONFIG_MTK_SPI)
	struct mt_chip_conf spi_conf;
#elif IS_ENABLED(CONFIG_SPI_MT65XX)
	struct mtk_chip_config spi_config;
#endif

	struct regulator *regulator_vdd;
	struct regulator *regulator_lcd_reset;
	struct regulator *regulator_lcd_bl_en;
	struct regulator *regulator_lcd_vsp;
	struct regulator *regulator_lcd_vsn;
	struct regulator *regulator_tsp_reset;

#if defined(USE_RINFO)
	u32 iinfo_addr;
	u32 rinfo_addr;
	u32 rinfo_data[10];
	int rinfo_ok;
#endif

	int reset_retry;
	atomic_t init;
	atomic_t boot;
	atomic_t fwup;
	atomic_t fwsts;

	char test_fwpath[PATH_MAX];
	struct lxs_ts_fw_info fw;
	int fw_type;
	u8 *fw_data;
	int fw_size;

#if defined(USE_VBUS_NOTIFIER)
	struct delayed_work vbus_work;
	struct notifier_block vbus_nb;
	int vbus_type;
#endif

	bool ed_reset_flag;

	bool lcdoff_test;

	int prox_state;
	int prox_power_off;

	int aot_enable;
	int spay_enable;
	int lowpower_mode;

	int charger_mode;
	int dead_zone_enable;
	int glove_mode;
	int sip_mode;
	int game_mode;
	int cover_mode;
	int ear_detect_mode;
	int prox_lp_scan_mode;
	int note_mode;
	int fw_corruption;
	u8 brush_mode;

	int driving_mode;
#if IS_ENABLED(CONFIG_INPUT_SEC_SECURE_TOUCH)
	atomic_t secure_enabled;
	atomic_t secure_pending_irqs;
	struct completion secure_powerdown;
	struct completion secure_interrupt;
	struct mutex secure_lock;
#endif
	struct lxs_ts_report report;
	struct lxs_ts_lpwg_point knock[TS_MAX_LPWG_CODE];
	struct lxs_ts_lpwg_point swipe[TS_MAX_SWIPE_CODE];
	int swipe_time;
	//
	int intr_status;
	int tcount;
	u32 new_mask;
	u32 old_mask;
	struct lxs_ts_input_touch_data tfinger[MAX_FINGER];
	int noise_mode;
	int noise_freq;
	int noise_meas;

	void *prd;
	u32 prd_quirks;

	int fps;

	u32 dbg_mask;
	u32 dbg_tc_delay;

	bool probe_done;
	struct sec_cmd_data sec;

	int touch_count;
	int tx_count;
	int rx_count;
	int res_x;
	int res_y;
	int max_x;
	int max_y;

	struct mutex bus_mutex;
	struct mutex device_mutex;
	struct mutex eventlock;
	struct mutex modechange;

	volatile bool reset_is_on_going;

	int debug_flag;

	int sensitivity_mode;

	struct delayed_work esd_reset_work;
	struct delayed_work reset_work;
	struct delayed_work work_print_info;
	struct delayed_work work_read_info;

	struct notifier_block lxs_input_nb;
	struct notifier_block lcd_nb;
	int esd_recovery;

	u8 *lpwg_dump_buf;
	u16 lpwg_dump_buf_idx;
	u16 lpwg_dump_buf_size;

	int run_prt_size;
	int (*run_open)(void *ts_data, int *min, int *max, char *prt_buf, int opt);
	int (*run_short_mux)(void *ts_data, int *min, int *max, char *prt_buf, int opt);
	int (*run_short_ch)(void *ts_data, int *min, int *max, char *prt_buf, int opt);
	int (*run_np_m1_raw)(void *ts_data, int *min, int *max, char *prt_buf, int opt);
	int (*run_np_m1_jitter)(void *ts_data, int *min, int *max, char *prt_buf, int opt);
	int (*run_np_m2_raw)(void *ts_data, int *min, int *max, char *prt_buf, int opt);
	int (*run_np_m2_gap_x)(void *ts_data, int *min, int *max, char *prt_buf, int opt);
	int (*run_np_m2_gap_y)(void *ts_data, int *min, int *max, char *prt_buf, int opt);
	int (*run_np_m2_jitter)(void *ts_data, int *min, int *max, char *prt_buf, int opt);
	int (*run_lp_m1_raw)(void *ts_data, int *min, int *max, char *prt_buf, int opt);
	int (*run_lp_m1_jitter)(void *ts_data, int *min, int *max, char *prt_buf, int opt);
	int (*run_lp_m2_raw)(void *ts_data, int *min, int *max, char *prt_buf, int opt);
	int (*run_lp_m2_jitter)(void *ts_data, int *min, int *max, char *prt_buf, int opt);
};

enum {
	PRD_QUIRK_RAW_RETURN_MODE_VAL	= BIT(0),
};

static inline void lxs_ts_delay(unsigned int ms)
{
	if (ms < 20)
		usleep_range(ms * 1000, ms * 1000);
	else
		msleep(ms);
}

static inline int ts_get_power_state(struct lxs_ts_data *ts)
{
	return ts->power_state;
}

static inline void ts_set_power_state(struct lxs_ts_data *ts, int state)
{
	ts->power_state = state;
}

static inline int is_ts_power_state_off(struct lxs_ts_data *ts)
{
	return (ts_get_power_state(ts) == POWER_OFF_STATUS);
}

static inline int is_ts_power_state_lpm(struct lxs_ts_data *ts)
{
	return ((ts_get_power_state(ts) == LP_MODE_STATUS) || (ts_get_power_state(ts) == LP_MODE_EXIT));
}

static inline int is_ts_power_state_on(struct lxs_ts_data *ts)
{
	return (ts_get_power_state(ts) == POWER_ON_STATUS);
}

static inline void ts_set_power_state_off(struct lxs_ts_data *ts)
{
	ts_set_power_state(ts, POWER_OFF_STATUS);
}

static inline void ts_set_power_state_lpm(struct lxs_ts_data *ts)
{
	ts_set_power_state(ts, LP_MODE_STATUS);
}

static inline void ts_set_power_state_on(struct lxs_ts_data *ts)
{
	ts_set_power_state(ts, POWER_ON_STATUS);
}

#if defined(USE_LCD_TYPE_CHK)
extern unsigned int lcdtype;
#endif

extern int lxs_ts_gpio_reset(struct lxs_ts_data *ts, bool on);

extern int lxs_ts_pinctrl_configure(struct lxs_ts_data *ts, bool on);

extern int lxs_ts_reg_read(struct lxs_ts_data *ts, u32 addr, void *data, int size);
extern int lxs_ts_reg_write(struct lxs_ts_data *ts, u32 addr, void *data, int size);
extern int lxs_ts_read_value(struct lxs_ts_data *ts, u32 addr, u32 *value);
extern int lxs_ts_write_value(struct lxs_ts_data *ts, u32 addr, u32 value);
extern int lxs_ts_reg_bit_set(struct lxs_ts_data *ts, u32 addr, u32 *value, u32 mask);
extern int lxs_ts_reg_bit_clr(struct lxs_ts_data *ts, u32 addr, u32 *value, u32 mask);

extern void lxs_ts_ic_reset(struct lxs_ts_data *ts);
extern void lxs_ts_ic_reset_sync(struct lxs_ts_data *ts);

extern int lxs_ts_irq_wake(struct lxs_ts_data *ts, bool enable);
extern int lxs_ts_irq_enable(struct lxs_ts_data *ts, bool enable);
extern int lxs_ts_irq_activate(struct lxs_ts_data *ts, bool enable);

extern void lxs_ts_release_pen(struct lxs_ts_data *ts);
extern void lxs_ts_locked_release_pen(struct lxs_ts_data *ts);
extern void lxs_ts_release_finger(struct lxs_ts_data *ts);
extern void lxs_ts_locked_release_finger(struct lxs_ts_data *ts);

extern int lxs_ts_tsp_reset_ctrl(struct lxs_ts_data *ts, bool on);
extern int lxs_ts_lcd_reset_ctrl(struct lxs_ts_data *ts, bool on);
extern int lxs_ts_lcd_power_ctrl(struct lxs_ts_data *ts, bool on);

extern int lxs_ts_resume_early(struct lxs_ts_data *ts);
extern int lxs_ts_resume(struct lxs_ts_data *ts);
extern int lxs_ts_suspend_early(struct lxs_ts_data *ts);
extern int lxs_ts_suspend(struct lxs_ts_data *ts);

/* FN block */
extern int lxs_ts_ic_test(struct lxs_ts_data *ts);
extern void lxs_ts_init_recon(struct lxs_ts_data *ts);
extern void lxs_ts_init_ctrl(struct lxs_ts_data *ts);
extern int lxs_ts_get_driving_opt1(struct lxs_ts_data *ts, u32 *data);
extern int lxs_ts_set_driving_opt1(struct lxs_ts_data *ts, u32 data);
extern int lxs_ts_get_driving_opt2(struct lxs_ts_data *ts, u32 *data);
extern int lxs_ts_set_driving_opt2(struct lxs_ts_data *ts, u32 data);
extern int lxs_ts_ic_lpm(struct lxs_ts_data *ts);
extern void lxs_ts_knock(struct lxs_ts_data *ts);
extern int lxs_ts_driving(struct lxs_ts_data *ts, int mode);
extern int lxs_ts_ic_info(struct lxs_ts_data *ts);
extern int lxs_ts_ic_init(struct lxs_ts_data *ts);
extern int lxs_ts_hw_init(struct lxs_ts_data *ts);
extern void lxs_ts_touch_off(struct lxs_ts_data *ts);
extern int lxs_ts_set_charger(struct lxs_ts_data *ts);
extern int lxs_ts_mode_switch(struct lxs_ts_data *ts, int mode, int value);
extern void lxs_ts_mode_restore(struct lxs_ts_data *ts);

extern void lxs_ts_change_scan_rate(struct lxs_ts_data *ts, u8 rate);
extern int lxs_ts_set_external_noise_mode(struct lxs_ts_data *ts, u8 mode);

extern void lxs_ts_lpwg_dump_buf_init(struct lxs_ts_data *ts);
extern int lxs_ts_lpwg_dump_buf_write(struct lxs_ts_data *ts, u8 *buf);
extern int lxs_ts_lpwg_dump_buf_read(struct lxs_ts_data *ts, u8 *buf);
extern int lxs_ts_get_lp_dump_data(struct lxs_ts_data *ts);

extern int lxs_ts_cmd_init(struct lxs_ts_data *ts);
extern void lxs_ts_cmd_free(struct lxs_ts_data *ts);

extern void lxs_ts_sw_reset(struct lxs_ts_data *ts);
extern int lxs_ts_boot_result(struct lxs_ts_data *ts);

extern int lxs_ts_load_fw_from_ts_bin(struct lxs_ts_data *ts);
extern int lxs_ts_load_fw_from_mp_bin(struct lxs_ts_data *ts);
extern int lxs_ts_load_fw_from_external(struct lxs_ts_data *ts, const char *file_path);
extern int lxs_ts_reload_fw(struct lxs_ts_data *ts);

extern int lxs_ts_sram_test(struct lxs_ts_data *ts, int cnt);

extern void lxs_ts_prd_init(struct lxs_ts_data *ts);
extern void lxs_ts_prd_remove(struct lxs_ts_data *ts);
extern int lxs_ts_request_fw(struct lxs_ts_data *ts, const char *fw_name);

#if IS_ENABLED(CONFIG_SEC_PANEL_NOTIFIER)
extern int ss_panel_notifier_register(struct notifier_block *nb);
extern int ss_panel_notifier_unregister(struct notifier_block *nb);
#endif
#if IS_ENABLED(CONFIG_DRM_SAMSUNG_DPU)
extern int panel_notifier_register(struct notifier_block *nb);
extern int panel_notifier_unregister(struct notifier_block *nb);
#endif

void lxs_ts_run_rawdata_all(struct lxs_ts_data *ts);

#endif /* _LXS_TS_H_ */

