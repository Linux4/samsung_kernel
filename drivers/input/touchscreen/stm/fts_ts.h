#ifndef _LINUX_FTS_TS_H_
#define _LINUX_FTS_TS_H_

#include <linux/device.h>
#include <linux/hrtimer.h>
#include <linux/wakelock.h>
#ifdef CONFIG_INPUT_BOOSTER
#include <linux/input/input_booster.h>
#endif
#include <linux/clk.h>
#include <linux/pinctrl/consumer.h>

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
		dev_err(dev, fmt, ## __VA_ARGS__);	\
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

#define USE_OPEN_CLOSE

#ifdef USE_OPEN_DWORK
#define TOUCH_OPEN_DWORK_TIME 10
#endif

//#undef CONFIG_SECURE_TOUCH

/* Feature define */
#define FTS_SUPPORT_EDGE_POSITION	/* GRIP, edge revolving position */
#define FTS_SUPPORT_SIDE_GESTURE	/* GRIP, Wakeup */
#undef FTS_SUPPORT_WATER_MODE		/* Wet mode, water proof */
#undef FTS_SUPPORT_NOISE_PARAM		/* save & resrote noise parameter */
#define FTS_SUPPORT_ESD_CHECK		/* support ESD interrupt, handle reset */.
#define SEC_TSP_FACTORY_TEST

#define FTS_SUPPORT_TOUCH_KEY

#undef FTS_SUPPORT_TA_MODE
#undef FTS_SUPPORT_2NDSCREEN
#undef FTS_SUPPORT_SIDE_SCROLL
#undef FTS_SUPPORT_HOVER

#if defined(CONFIG_SEC_HERO2QLTE_PROJECT)
#define FTS_SUPPORT_PARTIAL_DOWNLOAD
#elif defined(CONFIG_SEC_POSEIDONLTE_PROJECT)
#define FTS_SUPPORT_PARTIAL_DOWNLOAD
#else
#undef FTS_SUPPORT_PARTIAL_DOWNLOAD
#endif

#undef TSP_RUN_AUTOTUNE_DEFAULT

#undef USE_RESET_WORK_EXIT_LOWPOWERMODE
#undef USE_STYLUS_PEN
#define ST_INT_COMPLETE
#undef CONFIG_GLOVE_TOUCH

#define FIRMWARE_IC					"fts_ic"
#define FTS_MAX_FW_PATH					64
#define FTS_TS_DRV_NAME					"fts_touch"
#define FTS_DEVICE_NAME					"STM"

#define FTS_FIRMWARE_NAME_NULL				NULL


/*
#define CONFIG_ID_D2_TR					0x2E
#define CONFIG_ID_D2_TB					0x30
*/
#define CONFIG_OFFSET_BIN_D1				0xf822
#define CONFIG_OFFSET_BIN_D2				0x1E822
#define CONFIG_OFFSET_BIN_D3				0x3F022

#define RX_OFFSET_BIN_D2				0x1E834
#define TX_OFFSET_BIN_D2				0x1E835

#define STM_VER4		0x04
#define STM_VER5		0x05
#define STM_VER7		0x07

#define FTS_ID0						0x39
#define FTS_ID1						0x80
#define FTS_ID2						0x6C

#define FTS_7_ID0					0x36
#define FTS_7_ID1					0x70

#define FTS_SLAVE_ADDRESS				0x49
#define FTS_SLAVE_ADDRESS_SUB				0x48

#define FTS_DIGITAL_REV_1				0x01
#define FTS_DIGITAL_REV_2				0x02
#define FTS_DIGITAL_REV_3				0x03
#define FTS_FIFO_MAX					32
#define FTS_EVENT_SIZE					8

#define PRESSURE_MIN					0
#define PRESSURE_MAX					127
#define P70_PATCH_ADDR_START				0x00420000
#define FINGER_MAX					10
#define AREA_MIN					PRESSURE_MIN
#define AREA_MAX					PRESSURE_MAX

#define EVENTID_NO_EVENT				0x00
#define EVENTID_ENTER_POINTER				0x03
#define EVENTID_LEAVE_POINTER				0x04
#define EVENTID_MOTION_POINTER				0x05
#define EVENTID_HOVER_ENTER_POINTER			0x07
#define EVENTID_HOVER_LEAVE_POINTER			0x08
#define EVENTID_HOVER_MOTION_POINTER			0x09
#define EVENTID_PROXIMITY_IN				0x0B
#define EVENTID_PROXIMITY_OUT				0x0C
#define EVENTID_MSKEY					0x0E
#define EVENTID_ERROR					0x0F
#define EVENTID_CONTROLLER_READY			0x10
#define EVENTID_SLEEPOUT_CONTROLLER_READY		0x11
#define EVENTID_RESULT_READ_REGISTER			0x12
#define	EVENTID_STATUS_REQUEST_COMP			0x13
#define EVENTID_STATUS_EVENT				0x16
#define EVENTID_INTERNAL_RELEASE_INFO			0x19
#define EVENTID_EXTERNAL_RELEASE_INFO			0x1A

#define EVENTID_SIDE_TOUCH_DEBUG			0xDB
#define EVENTID_SIDE_TOUCH				0x0B

#define EVENTID_FROM_STRING				0x80
#define EVENTID_GESTURE					0x20

#define STATUS_EVENT_MUTUAL_AUTOTUNE_DONE		0x01
#define STATUS_EVENT_SELF_AUTOTUNE_DONE			0x42
#define STATUS_EVENT_SELF_AUTOTUNE_DONE_D3		0x02
#define STATUS_EVENT_WATER_SELF_AUTOTUNE_DONE		0x4E
#define STATUS_EVENT_FORCE_CAL_DONE			0x15
#define STATUS_EVENT_FORCE_CAL_DONE_D3			0x06
#ifdef FTS_SUPPORT_WATER_MODE
#define STATUS_EVENT_WATER_SELF_DONE			0x17
#endif
#define STATUS_EVENT_FLASH_WRITE_CONFIG			0x03
#define STATUS_EVENT_FLASH_WRITE_CXTUNE_VALUE		0x04
#define STATUS_EVENT_FORCE_CAL_MUTUAL_SELF		0x05
#define STATUS_EVENT_FORCE_CAL_MUTUAL			0x15
#define STATUS_EVENT_FORCE_CAL_SELF			0x06
#define STATUS_EVENT_WATERMODE_ON			0x07
#define STATUS_EVENT_WATERMODE_OFF			0x08
#define STATUS_EVENT_RTUNE_MUTUAL			0x09
#define STATUS_EVENT_RTUNE_SELF				0x0A
#define STATUS_EVENT_PANEL_TEST_RESULT			0x0B
#define STATUS_EVENT_GLOVE_MODE				0x0C
#define STATUS_EVENT_RAW_DATA_READY			0x0D
#ifdef FTS_SUPPORT_PARTIAL_DOWNLOAD
#define STATUS_EVENT_PURE_AUTOTUNE_FLAG_WRITE_FINISH	0x10
#define STATUS_EVENT_PURE_AUTOTUNE_FLAG_ERASE_FINISH	0x11
#endif
#define STATUS_EVENT_MUTUAL_CAL_FRAME_CHECK		0xC1
#define STATUS_EVENT_SELF_CAL_FRAME_CHECK		0xC2
#define STATUS_EVENT_CHARGER_CONNECTED			0xCC
#define STATUS_EVENT_CHARGER_DISCONNECTED		0xCD

#define INT_ENABLE					0x41
#define INT_DISABLE					0x00
#define INT_ENABLE_D3					0x48
#define INT_DISABLE_D3					0x08

#define READ_STATUS					0x84
#define READ_ONE_EVENT					0x85
#define READ_ALL_EVENT					0x86

#define SLEEPIN						0x90
#define SLEEPOUT					0x91
#define SENSEOFF					0x92
#define SENSEON						0x93
#define FTS_CMD_HOVER_OFF				0x94
#define FTS_CMD_HOVER_ON				0x95

#define FTS_CMD_MSKEY_AUTOTUNE				0x96
#define FTS_CMD_TRIM_LOW_POWER_OSCILLATOR		0x97

#ifdef FTS_SUPPORT_PARTIAL_DOWNLOAD
#define FTS_CMD_FORCE_AUTOTUNE				0x98
#endif

#define FTS_CMD_KEY_SENSE_OFF				0x9A
#define FTS_CMD_KEY_SENSE_ON				0x9B
#define SENSEON_SLOW					0x9C

#define FTS_CMD_SET_FAST_GLOVE_MODE			0x9D

#define FTS_CMD_GLOVE_MODE_OFF				0x9E
#define FTS_CMD_GLOVE_MODE_ON				0x9F
#define FTS_CMD_SET_NOR_GLOVE_MODE			0x9F

#define FLUSHBUFFER					0xA1
#define FORCECALIBRATION				0xA2
#define CX_TUNNING					0xA3
#define SELF_AUTO_TUNE					0xA4

#define FTS_CMD_CHARGER_PLUGGED				0xA8
#define FTS_CMD_CHARGER_UNPLUGGED			0xAB

#define FTS_CMD_RELEASEINFO				0xAA
#define FTS_CMD_STYLUS_OFF				0xAB
#define FTS_CMD_STYLUS_ON				0xAC
#define FTS_CMD_LOWPOWER_MODE				0xAD

#define FTS_CMS_ENABLE_FEATURE				0xC1
#define FTS_CMS_DISABLE_FEATURE				0xC2

#define FTS_CMD_WRITE_PRAM				0xF0
#define FTS_CMD_BURN_PROG_FLASH				0xF2
#define FTS_CMD_ERASE_PROG_FLASH			0xF3
#define FTS_CMD_READ_FLASH_STAT				0xF4
#define FTS_CMD_UNLOCK_FLASH				0xF7
#define FTS_CMD_SAVE_FWCONFIG				0xFB
#define FTS_CMD_SAVE_CX_TUNING				0xFC

#define FTS_CMD_FAST_SCAN				0x1
#define REPORT_RATE_90HZ				0

#define FTS_CMD_SLOW_SCAN				0x2
#define REPORT_RATE_60HZ				1

/* To Do */
#define FTS_CMD_USLOW_SCAN				0x3
#define REPORT_RATE_30HZ				2

#define FTS_CMD_STRING_ACCESS				0x3000

#define FTS_CMD_NOTIFY					0xC0

#define FTS_RETRY_COUNT					10

/* SCRUB, SPAY */
#define FTS_STRING_EVENT_AOD_TRIGGER			(1 << 0)
#define FTS_STRING_EVENT_WATCH_STATUS			(1 << 2)
#define FTS_STRING_EVENT_FAST_ACCESS			(1 << 3)
#define FTS_STRING_EVENT_DIRECT_INDICATOR		(1 << 3) | (1 << 2)
#define FTS_STRING_EVENT_SPAY				(1 << 4)
#define FTS_STRING_EVENT_SPAY1				(1 << 5)
#define FTS_STRING_EVENT_SPAY2				(1 << 4) | (1 << 5)
#define FTS_STRING_EVENT_EDGE_SWIPE_RIGHT		(1 << 6)
#define FTS_STRING_EVENT_EDGE_SWIPE_LEFT		(1 << 7)

#define FTS_SIDEGESTURE_EVENT_SINGLE_STROKE		0xE0
#define FTS_SIDEGESTURE_EVENT_DOUBLE_STROKE		0xE1
#define FTS_SIDEGESTURE_EVENT_INNER_STROKE		0xE3

#define FTS_SIDETOUCH_EVENT_LONG_PRESS			0xBB
#define FTS_SIDETOUCH_EVENT_REBOOT_BY_ESD		0xED

#define FTS_ENABLE					1
#define FTS_DISABLE					0

#define FTS_MODE_SCRUB					(1 << 0)
#define FTS_MODE_SPAY					(1 << 1)
#define FTS_MODE_AOD_TRIGGER				(1 << 2)

#define FTS_LOWP_FLAG_2ND_SCREEN			(1 << 0)
#define FTS_LOWP_FLAG_BLACK_UI				(1 << 1)
#define FTS_LOWP_FLAG_SPAY				(1 << 2)
#define FTS_LOWP_FLAG_TEMP_CMD				(1 << 3)

#define EDGEWAKE_RIGHT					0
#define EDGEWAKE_LEFT					1

#define SMARTCOVER_COVER	// for  Various Cover
#ifdef SMARTCOVER_COVER
#define MAX_W						16		// zero is 16 x 28
#define MAX_H						32		// byte size to IC
#define MAX_TX						MAX_W
#define MAX_BYTE					MAX_H
#endif

#define STATE_POWEROFF					(1 << 0)
#define STATE_LOWPOWER_SUSPEND				(1 << 1)
#define STATE_LOWPOWER_RESUME				(1 << 2)
#define STATE_LOWPOWER					(STATE_LOWPOWER_SUSPEND | STATE_LOWPOWER_RESUME)
#define STATE_ACTIVE					(1 << 3)

/* OCTA revision
 * seperate OLD / NEW OCTA by ID2 value(4 bit)
 * 0b 0000 : OLD
 * 0b 0010 : NEW (change Hsync GPIO in IC side)
 */
#define FTS_OCTA_TYPE_OLD				0x0
#define FTS_OCTA_TYPE_NEW				0x1
#define FTS_OCTA_TYPE_SEPERATOR				0x2

#if !defined(FTS_SUPPORT_TOUCH_KEY) && defined(TKEY_BOOSTER)
#undef TKEY_BOOSTER
#endif

#ifdef FTS_SUPPORT_TOUCH_KEY
#define KEY_PRESS		1
#define KEY_RELEASE		0
#define TOUCH_KEY_NULL		0

#define TOUCH_KEY_RECENT	0x01
#define TOUCH_KEY_BACK		0x02

struct fts_touchkey {
	unsigned int value;
	unsigned int keycode;
	char *name;
};
#endif

#ifdef CONFIG_GLOVE_TOUCH
enum TOUCH_MODE {
	FTS_TM_NORMAL = 0,
	FTS_TM_GLOVE,
};
#endif

/* for get_all_data in factory cmd */
typedef void (*run_func_t)(void *);
enum data_type {
	DATA_UNSIGNED_CHAR,
	DATA_SIGNED_CHAR,
	DATA_UNSIGNED_SHORT,
	DATA_SIGNED_SHORT,
	DATA_UNSIGNED_INT,
	DATA_SIGNED_INT,
};

enum fts_error_return {
	FTS_NOT_ERROR = 0,
	FTS_ERROR_INVALID_CHIP_ID,
	FTS_ERROR_INVALID_CHIP_VERSION_ID,
	FTS_ERROR_INVALID_SW_VERSION,
	FTS_ERROR_EVENT_ID,
	FTS_ERROR_TIMEOUT,
	FTS_ERROR_FW_UPDATE_FAIL,
};

#ifdef FTS_SUPPORT_NOISE_PARAM
#define MAX_NOISE_PARAM 5
struct fts_noise_param {
	unsigned short pAddr[MAX_NOISE_PARAM];
	unsigned short pData[MAX_NOISE_PARAM];
};
#endif

#ifdef SEC_TSP_FACTORY_TEST
#include "linux/input/sec_cmd.h"
extern struct class *sec_class;

/* ----------------------------------------
 * write 0xE4 [ 11 | 10 | 01 | 00 ]
 * MSB <-------------------> LSB
 * read 0xE4
 * mapping sequnce : LSB -> MSB
 * struct sec_ts_test_result {
 * * assy : front + OCTA assay
 * * module : only OCTA
 *	 union {
 *		 struct {
 *			 u8 assy_count:2;		-> 00
 *			 u8 assy_result:2;		-> 01
 *			 u8 module_count:2;	-> 10
 *			 u8 module_result:2;	-> 11
 *		 } __attribute__ ((packed));
 *		 unsigned char data[1];
 *	 };
 *};
 * ---------------------------------------- */
struct fts_ts_test_result {
	union {
		struct {
			u8 assy_count:2;
			u8 assy_result:2;
			u8 module_count:2;
			u8 module_result:2;
		} __attribute__ ((packed));
		unsigned char data[1];
	};
};

#define TEST_OCTA_MODULE	1
#define TEST_OCTA_ASSAY		2

#define TEST_OCTA_NONE		0
#define TEST_OCTA_FAIL		1
#define TEST_OCTA_PASS		2

#endif

struct fts_device_tree_data {
	int max_x;
	int max_y;
#ifdef FTS_SUPPORT_TOUCH_KEY
	int support_mskey;
	int num_touchkey;
	struct fts_touchkey *touchkey;
#endif
	int irq_gpio;
	const char *project;
	const char *model;
	const char *firmware_name;

#ifdef FTS_SUPPORT_SIDE_GESTURE
	int support_sidegesture;
#endif
	int grip_area;
	bool support_string_lib;
	int bringup;
	int tsp_id;
	int tsp_id2;
};

#define RAW_MAX	3750
/**
 * struct fts_finger - Represents fingers.
 * @ state: finger status (Event ID).
 * @ mcount: moving counter for debug.
 */
struct fts_finger {
	unsigned char state;
	unsigned short mcount;
	int lx;
	int ly;
};

enum fts_cover_id {
	FTS_FLIP_WALLET = 0,
	FTS_VIEW_COVER,
	FTS_COVER_NOTHING1,
	FTS_VIEW_WIRELESS,
	FTS_COVER_NOTHING2,
	FTS_CHARGER_COVER,
	FTS_VIEW_WALLET,
	FTS_LED_COVER,
	FTS_CLEAR_FLIP_COVER,
	FTS_QWERTY_KEYBOARD_EUR,
	FTS_QWERTY_KEYBOARD_KOR,
	FTS_MONTBLANC_COVER = 100,
};

enum fts_customer_feature {
	FTS_FEATURE_ORIENTATION_GESTURE = 1,
	FTS_FEATURE_STYLUS,
	FTS_FEATURE_QUICK_SHORT_CAMERA_ACCESS,
	FTS_FEATURE_SIDE_GESTURE,
	FTS_FEATURE_COVER_GLASS,
	FTS_FEATURE_COVER_WALLET,
	FTS_FEATURE_COVER_LED,
	FTS_FEATURE_COVER_CLEAR_FLIP,
	FTS_FEATURE_DUAL_SIDE_GUSTURE,
	FTS_FEATURE_CUSTOM_COVER_GLASS_ON,
	FTS_FEATURE_NONE,
	FTS_FEATURE_DUAL_SIDE_GESTURE,
};

struct fts_ts_info {
	struct device *dev;
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct hrtimer timer;
	struct fts_device_tree_data *dt_data;
#ifdef TSP_BOOSTER
	struct input_booster *tsp_booster;
#endif
#ifdef TKEY_BOOSTER
	struct input_booster *tkey_booster;
#endif
	struct pinctrl *pinctrl;

	const char *firmware_name;
	int irq;
	bool irq_enabled;

#ifdef FTS_SUPPORT_TA_MODE
	struct fts_callbacks callbacks;
	bool TA_Pluged;
#endif
#ifdef SEC_TSP_FACTORY_TEST
	struct sec_cmd_data sec;
	int SenseChannelLength;
	int ForceChannelLength;
	short *pFrame;
	unsigned char *cx_data;
	bool get_all_data;
	struct fts_ts_test_result test_result;
#endif
#ifdef FTS_SUPPORT_HOVER
	bool hover_ready;
	bool hover_enabled;
	int retry_hover_enable_after_wakeup;
#endif
	/* glove */
	bool glove_enabled;
	int touch_mode;
	bool fast_glove_enabled;

	bool flip_enable;
	bool flip_state;
	bool mainscr_disable;
	bool run_autotune;

	unsigned char lowpower_flag;
	bool lowpower_mode;
	bool deepsleep_mode;
	bool wirelesscharger_mode;
	int power_state;
	int cover_type;
	int wakeful_edge_side;
	bool edge_grip_mode;
#ifdef USE_STYLUS_PEN
	bool use_stylus;
#endif
	unsigned char fts_mode;
	struct completion resume_done;
	struct wake_lock wakelock;

#ifdef FTS_SUPPORT_2NDSCREEN_FLAG
	u8 SIDE_Flag;
	u8 previous_SIDE_value;
#endif

#ifdef FTS_SUPPORT_TOUCH_KEY
	unsigned char tsp_keystatus;
	int touchkey_threshold;
	struct device *fac_dev_tk;
	struct regulator *keyled_vreg;
#endif

	int digital_rev;
	int stm_ver;
	int touch_count;
	struct fts_finger finger[FINGER_MAX];

	int ic_product_id;			/* product id of ic */
	int ic_revision_of_ic;			/* revision of reading from IC */
	int ic_revision_of_bin;			/* revision of reading from binary */
	int fw_version_of_ic;			/* firmware version of IC */
	int fw_version_of_bin;			/* firmware version of binary */
	int config_version_of_ic;		/* Config release data from IC */
	int config_version_of_bin;		/* Config release data from binary */
	unsigned short fw_main_version_of_ic;	/* firmware main version of IC */
	unsigned short fw_main_version_of_bin;	/* firmware main version of binary */
	int panel_revision;			/* Octa panel revision */

#ifdef USE_OPEN_DWORK
	struct delayed_work open_work;
#endif

#ifdef FTS_SUPPORT_NOISE_PARAM
	struct fts_noise_param *noise_param;
#endif

	unsigned int delay_time;
	unsigned int temp;
#if defined(USE_RESET_WORK_EXIT_LOWPOWERMODE) || defined(FTS_SUPPORT_ESD_CHECK)
	struct delayed_work reset_work;
#endif
	struct delayed_work work_read_nv;

	unsigned int scrub_id;
	unsigned int scrub_x;
	unsigned int scrub_y;
	unsigned short string_adrr;
#if defined(CONFIG_SECURE_TOUCH)
	atomic_t st_enabled;
	atomic_t st_pending_irqs;
	struct completion st_powerdown;
	struct completion st_interrupt;
	struct clk *core_clk;
	struct clk *iface_clk;
#endif
	struct mutex i2c_mutex;
	struct mutex device_mutex;
	bool touch_stopped;
	bool reinit_done;
	bool hover_present;

#ifdef SMARTCOVER_COVER
	bool smart_cover[MAX_BYTE][MAX_TX];
	bool changed_table[MAX_TX][MAX_BYTE];
	u8 send_table[MAX_TX][4];
#endif

#ifdef CONFIG_TOUCHSCREEN_DUMP_MODE
		struct delayed_work ghost_check;
		u8 tsp_dump_lock;
#endif

#ifdef FTS_SUPPORT_PARTIAL_DOWNLOAD
	unsigned char o_afe_ver;
	unsigned char afe_ver;
#endif
	int pat;

	unsigned char data[FTS_EVENT_SIZE * FTS_FIFO_MAX];
	unsigned char lcd_id[4];
	int (*fts_power_ctrl)(struct i2c_client *client, bool enable);

	int (*stop_device)(struct fts_ts_info *info);
	int (*start_device)(struct fts_ts_info *info);
	int (*fts_write_reg)(struct fts_ts_info *info, unsigned char *reg, unsigned short num_com);
	int (*fts_read_reg)(struct fts_ts_info *info, unsigned char *reg, int cnum, unsigned char *buf, int num);
	int (*fts_get_version_info)(struct fts_ts_info *info);
	int (*fts_wait_for_ready)(struct fts_ts_info *info);
	int (*fts_command)(struct fts_ts_info *info, unsigned char cmd);
	int (*fts_systemreset)(struct fts_ts_info *info);
	int (*fts_write_to_string)(struct fts_ts_info *info, unsigned short *reg, unsigned char *data, int length);
	int (*fts_read_from_string)(struct fts_ts_info *info, unsigned short *reg, unsigned char *data, int length);
	int (*fts_change_scan_rate)(struct fts_ts_info *info, unsigned char cmd);
#ifdef FTS_SUPPORT_NOISE_PARAM
	int (*fts_get_noise_param_address) (struct fts_ts_info *info);
#endif
#ifdef FTS_SUPPORT_TOUCH_KEY
	int (*led_power)(struct fts_ts_info *info, bool on);
#endif
	void (*fts_delay)(unsigned int ms);
};

int fts_fw_update_on_probe(struct fts_ts_info *info);
int fts_fw_update_on_hidden_menu(struct fts_ts_info *info, int update_type);
void fts_fw_init(struct fts_ts_info *info);
void fts_execute_autotune(struct fts_ts_info *info);
int fts_fw_wait_for_event(struct fts_ts_info *info, unsigned char eid);
int fts_fw_wait_for_event_D3(struct fts_ts_info *info, unsigned char eid0, unsigned char eid1);
int fts_fw_wait_for_specific_event(struct fts_ts_info *info, unsigned char eid0, unsigned char eid1, unsigned char eid2);
void fts_irq_enable(struct fts_ts_info *info, bool enable);
int fts_interrupt_set(struct fts_ts_info *info, int enable);
int fts_get_tsp_test_result(struct fts_ts_info *info);
#ifdef FTS_SUPPORT_PARTIAL_DOWNLOAD
bool get_PureAutotune_status(struct fts_ts_info *info);
#endif


/*
 * get_lcd_id : return lcd_id, 3bytes, id1, id2, id3, from command line
 * get_samsung_lcd_attached : return lcd attched or detached
 */
extern unsigned int system_rev;
extern int get_lcd_attached(char *);
extern int boot_mode_recovery;
extern bool tsp_init_done;

#ifdef CONFIG_SAMSUNG_LPM_MODE
extern int poweroff_charging;
#endif

/*
 * If in DeviceTree structure, using below tree.
 * below tree is connected fts_parse_dt() in fts_ts.c file.
 */
/*
fts_i2c@49 {
	compatible = "stm,fts_ts";
	reg = <0x49>;
	interrupt-parent = <&tlmm>;
	interrupts = <123 0x0>;
	avdd-supply = <&max77838_l4>;	// 3.3V
	vddo-supply = <&max77838_l2>;	// 1.8V
	pinctrl-names = "tsp_active", "tsp_suspend";
	pinctrl-0 = <&tsp_int_active>;
	pinctrl-1 = <&tsp_int_suspend>;
	stm,irq-gpio = <&tlmm 123 0x0>;
	stm,tsp-id = <&pm8994_gpios 11 0x1>;
	stm,tsp-id2 = <&pm8994_gpios 20 0x1>;
	stm,tsp-coords = <1440 2560>;
	stm,grip_area = <512>;
	stm,i2c-pull-up = <1>;
	stm,tsp-project = "HERO2";
	stm,tsp-model = "G935";
	stm,bringup = <0>;
	stm,support_gesture = <1>;
	stm,string-lib;
	clock-names = "iface_clk", "core_clk";
	clocks = <&clock_gcc clk_gcc_blsp2_ahb_clk>,
		<&clock_gcc clk_gcc_blsp2_qup3_i2c_apps_clk>;
};

*/
#endif	//_LINUX_FTS_TS_H_
