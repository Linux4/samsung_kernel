#ifndef _LINUX_FTS_TS_H_
#define _LINUX_FTS_TS_H_

#include <linux/device.h>
#include <linux/hrtimer.h>
#include <linux/wakelock.h>
#ifdef CONFIG_INPUT_BOOSTER
#include <linux/input/input_booster.h>
#endif

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

#define USE_OPEN_CLOSE

#ifdef USE_OPEN_DWORK
#define TOUCH_OPEN_DWORK_TIME 10
#endif

#define FIRMWARE_IC					"fts_ic"
#define FTS_MAX_FW_PATH					64
#define FTS_TS_DRV_NAME					"fts_touch"
#define FTS_TS_DRV_VERSION				"0132"

#define FTS_DEVICE_NAME					"STM"

#ifdef CONFIG_SEC_TBLTE_PROJECT
#define FTS_FIRMWARE_NAME				"tsp_stm/stm_tb.fw"
#define FTS_MODEL_CONFIG_NAME				"N915"
#define TSP_RUN_AUTOTUNE_DEFAULT
#else
#define FTS_FIRMWARE_NAME				"tsp_stm/stm_tr.fw"
#define FTS_MODEL_CONFIG_NAME				"N910"
#endif

#ifdef CONFIG_SEC_STRING_ENABLE
#define USE_SEC_STRING_FEATURE
#endif

#define FTS_SUPPORT_NOISE_PARAM
#undef USE_RESET_WORK_EXIT_LOWPOWERMODE
#define CONFIG_SECURE_TOUCH
#define ST_INT_COMPLETE
#define CONFIG_GLOVE_TOUCH

#define FTS_FIRMWARE_NAME_TB_ID02			"tsp_stm/stm_tb_id02.fw"

#define FTS_FIRMWARE_NAME_NULL	NULL

#define CONFIG_ID_D2_TR					0x2E
#define CONFIG_ID_D2_TB					0x30
#define CONFIG_ID_D2_TB_SEPERATE			0x31

#define CONFIG_OFFSET_BIN_D1				0xf822
#define CONFIG_OFFSET_BIN_D2				0x1E822

#define RX_OFFSET_BIN_D2				0x1E834
#define TX_OFFSET_BIN_D2				0x1E835

#define FTS_ID0						0x39
#define FTS_ID1						0x80
#define FTS_ID2						0x6C

#define FTS_SLAVE_ADDRESS				0x49
#define FTS_SLAVE_ADDRESS_SUB				0x48

#define FTS_DIGITAL_REV_1				0x01
#define FTS_DIGITAL_REV_2				0x02
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
#define EVENTID_STATUS_EVENT				0x16
#define EVENTID_INTERNAL_RELEASE_INFO			0x19
#define EVENTID_EXTERNAL_RELEASE_INFO			0x1A

#define EVENTID_FROM_STRING				0x80
#define EVENTID_GESTURE					0x20

#define STATUS_EVENT_MUTUAL_AUTOTUNE_DONE		0x01
#define STATUS_EVENT_SELF_AUTOTUNE_DONE			0x42

#define INT_ENABLE					0x41
#define INT_DISABLE					0x00

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

#define FTS_CMD_KEY_SENSE_OFF				0x9A
#define FTS_CMD_KEY_SENSE_ON				0x9B
#define SENSEON_SLOW					0x9C

#define FTS_CMD_SET_FAST_GLOVE_MODE			0x9D

#define FTS_CMD_MSHOVER_OFF				0x9E
#define FTS_CMD_MSHOVER_ON				0x9F
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

#define FTS_CMD_STRING_ACCESS				0xF000
#define FTS_CMD_NOTIFY					0xC0

#define FTS_RETRY_COUNT					10

/* QUICK SHOT : Quick Camera Launching */
#define FTS_STRING_EVENT_REAR_CAM			(1 << 0)
#define FTS_STRING_EVENT_FRONT_CAM			(1 << 1)

/* SCRUB : Display Watch, Event Status / Fast Access Event */
#define FTS_STRING_EVENT_WATCH_STATUS		(1 << 2)
#define FTS_STRING_EVENT_FAST_ACCESS		(1 << 3)
#define FTS_STRING_EVENT_DIRECT_INDICATOR	(1 << 3) | (1 << 2)

#define FTS_ENABLE					1
#define FTS_DISABLE					0

#define FTS_MODE_QUICK_SHOT				(1 << 0)
#define FTS_MODE_SCRUB					(1 << 1)
#define FTS_MODE_QUICK_APP_ACCESS			(1 << 2)
#define FTS_MODE_DIRECT_INDICATOR			(1 << 3)

#define FTS_POWER_STATE_ACTIVE				0
#define FTS_POWER_STATE_LOWPOWER			1
#define FTS_POWER_STATE_LOWPOWER_RESUME			2
#define FTS_POWER_STATE_LOWPOWER_SUSPEND		3
#define FTS_POWER_STATE_POWER_DOWN			4

#define FTS_LOWP_FLAG_QUICK_CAM				(1 << 0)
#define FTS_LOWP_FLAG_2ND_SCREEN			(1 << 1)
#define FTS_LOWP_FLAG_BLACK_UI				(1 << 2)
#define FTS_LOWP_FLAG_QUICK_APP_ACCESS			(1 << 3)
#define FTS_LOWP_FLAG_DIRECT_INDICATOR			(1 << 4)
#define FTS_LOWP_FLAG_TEMP_CMD				(1 << 5)

/* OCTA revision
 * seperate OLD / NEW OCTA by ID2 value(4 bit)
 * 0b 0000 : OLD
 * 0b 0010 : NEW (change Hsync GPIO in IC side)
 */
#define FTS_OCTA_TYPE_OLD				0x0
#define FTS_OCTA_TYPE_NEW				0x1
#define FTS_OCTA_TYPE_SEPERATOR				0x2

#ifdef CONFIG_SEC_TBLTE_PROJECT
#define FTS_SUPPORT_TOUCH_KEY
#define FTS_SUPPORT_SIDE_GESTURE	// GRIP, Wakeup
#define FTS_SUPPORT_EDGE_POSITION
#define FTS_SUPPORT_MAINSCREEN_DISBLE
#undef FTS_SUPPORT_2NDSCREEN_FLAG
#undef FTS_USE_SIDE_SCROLL_FLAG
//#define EVENTID_SIDE_SCROLL_FLAG	0x40
#endif

#undef FTS_SUPPORT_QEEXO_ROI	// work with Display lab Mr.her for QEEXO.

#define ESD_CHECK	// for ESD, reset is ok.
#if defined(CONFIG_SEC_TBLTE_PROJECT) || defined(CONFIG_SEC_TRLTE_PROJECT)
#define TSP_RAWDATA_DUMP
#endif

#define FTS_ADDED_RESETCODE_IN_LPLM		//all TB, TR chn for lpm mode.

#ifdef FTS_SUPPORT_TOUCH_KEY
#define KEY_PRESS		1
#define KEY_RELEASE		0
#define TOUCH_KEY_NULL	0

#define TOUCH_KEY_RECENT	0x01
#define TOUCH_KEY_BACK		0x02

struct fts_touchkey {
	unsigned int value;
	unsigned int keycode;
	char *name;
};
#endif

#define TSP_BUF_SIZE 1024
#define CMD_STR_LEN 32
#define CMD_RESULT_STR_LEN 512
#define CMD_PARAM_NUM 8

#ifdef CONFIG_GLOVE_TOUCH
enum TOUCH_MODE {
	FTS_TM_NORMAL = 0,
	FTS_TM_GLOVE,
};
#endif

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

struct fts_i2c_platform_data {
	bool factory_flatform;
	bool recovery_mode;
	bool support_hover;
	bool support_mshover;
	int max_x;
	int max_y;
	int max_width;

	const char *firmware_name;
	const char *project_name;

	unsigned gpio;
	int irq_type;
};

#define SEC_TSP_FACTORY_TEST

#undef FTS_SUPPORT_TA_MODE

#ifdef SEC_TSP_FACTORY_TEST
extern struct class *sec_class;
#endif

struct fts_device_tree_data {
	int max_x;
	int max_y;
	int support_hover;
	int support_mshover;
#ifdef FTS_SUPPORT_TOUCH_KEY
	int support_mskey;
	int num_touchkey;
	struct fts_touchkey *touchkey;
#endif
	int extra_config[4];
	int external_ldo;
	int irq_gpio;
	const char *sub_pmic;
	const char *project;

	int num_of_supply;
	const char **name_of_supply;
#ifdef FTS_SUPPORT_SIDE_GESTURE
	bool support_sidegesture;
#endif
	
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

enum tsp_power_mode {
	TSP_POWERDOWN_MODE = 0,
	TSP_DEEPSLEEP_MODE,
	TSP_LOWPOWER_MODE,
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
	FTS_MONTBLANC_COVER = 100,
};

enum fts_customer_feature {
	FTS_FEATURE_ORIENTATION_GESTURE = 1,
	FTS_FEATURE_STYLUS,
	FTS_FEATURE_QUICK_SHORT_CAMERA_ACCESS,
	FTS_FEATURE_SIDE_GUSTURE,
	FTS_FEATURE_COVER_GLASS,
	FTS_FEATURE_COVER_WALLET,
	FTS_FEATURE_COVER_LED,
};

struct fts_ts_info {
	struct device *dev;
	struct i2c_client *client;
	struct i2c_client *client_sub;
	struct input_dev *input_dev;
	struct hrtimer timer;
	struct timer_list timer_charger;
	struct timer_list timer_firmware;
	struct work_struct work;
	struct fts_device_tree_data *dt_data;
	struct regulator *vcc_en;
	struct regulator *sub_pmic;
	struct regulator_bulk_data *supplies;
#ifdef TSP_BOOSTER
	struct input_booster *tsp_booster;
#endif

	unsigned short slave_addr;

	const char *firmware_name;
	int irq;
	int irq_type;
	bool irq_enabled;

#ifdef FTS_SUPPORT_TA_MODE 	
	struct fts_callbacks callbacks;
#endif
	struct mutex lock;
	bool enabled;
#ifdef SEC_TSP_FACTORY_TEST
	struct device *fac_dev_ts;
	struct list_head cmd_list_head;
	u8 cmd_state;
	char cmd[CMD_STR_LEN];
	int cmd_param[CMD_PARAM_NUM];
	char *cmd_result;
	int cmd_buffer_size;
	struct mutex cmd_lock;
	bool cmd_is_running;
	int SenseChannelLength;
	int ForceChannelLength;
	short *pFrame;
	unsigned char *cx_data;
	struct delayed_work cover_cmd_work;
	int delayed_cmd_param[2];
#endif

	bool hover_ready;
	bool hover_enabled;
	bool mshover_enabled;
	bool fast_mshover_enabled;
	bool flip_enable;
	bool flip_state;
	bool run_autotune;
	bool mainscr_disable;

	unsigned char lowpower_flag;
	bool lowpower_mode;
	bool deepsleep_mode;
	int fts_power_mode;
	int cover_type;
	bool edge_grip_mode;

	unsigned char fts_mode;
	int fts_pm_state;
	struct completion resume_done;
	struct wake_lock wakelock;

#ifdef FTS_SUPPORT_TA_MODE
	bool TA_Pluged;
#endif

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

#ifdef FTS_SUPPORT_QEEXO_ROI
	short roi_addr;
#endif

	int digital_rev;
	int touch_count;
	struct fts_finger finger[FINGER_MAX];

	int touch_mode;
	int retry_hover_enable_after_wakeup;

	int ic_product_id;			/* product id of ic */
	int ic_revision_of_ic;		/* revision of reading from IC */
	int fw_version_of_ic;		/* firmware version of IC */
	int ic_revision_of_bin;		/* revision of reading from binary */
	int fw_version_of_bin;		/* firmware version of binary */
	int config_version_of_ic;		/* Config release data from IC */
	int config_version_of_bin;	/* Config release data from IC */
	unsigned short fw_main_version_of_ic;	/* firmware main version of IC */
	unsigned short fw_main_version_of_bin;	/* firmware main version of binary */
	int panel_revision;			/* Octa panel revision */

#ifdef USE_OPEN_DWORK
	struct delayed_work open_work;
#endif

#ifdef FTS_SUPPORT_NOISE_PARAM
	struct fts_noise_param *noise_param;
#endif

	struct delayed_work cam_work;
	unsigned int delay_time;
	unsigned int temp;
#if defined(USE_RESET_WORK_EXIT_LOWPOWERMODE) || defined(ESD_CHECK)
	struct delayed_work reset_work;
#endif
#ifdef TSP_RAWDATA_DUMP
	struct delayed_work ghost_check;
#endif
	unsigned int scrub_id;
	unsigned int scrub_x;
	unsigned int scrub_y;

#if defined(CONFIG_SECURE_TOUCH)
	atomic_t st_enabled;
	atomic_t st_pending_irqs;
	struct completion st_powerdown;
	struct completion st_interrupt;
#endif
	struct mutex i2c_mutex;
	struct mutex device_mutex;
	bool touch_stopped;
	bool reinit_done;
	bool hover_present;

	unsigned char data[FTS_EVENT_SIZE * FTS_FIFO_MAX];
	unsigned char lcd_id[4];
	void (*fts_power_ctrl)(struct fts_ts_info *info, bool enable);

	int (*stop_device) (struct fts_ts_info * info);
	int (*start_device) (struct fts_ts_info * info);
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
#ifdef FTS_SUPPORT_QEEXO_ROI
	int (*get_fts_roi) (struct fts_ts_info *info);
#endif

#ifdef FTS_SUPPORT_TOUCH_KEY
	int (*led_power)(struct fts_ts_info *info, bool on);
#endif
};

int fts_fw_update_on_probe(struct fts_ts_info *info);
int fts_fw_update_on_hidden_menu(struct fts_ts_info *info, int update_type);
void fts_fw_init(struct fts_ts_info *info);
void fts_release_all_finger(struct fts_ts_info *info);
/* get_lcd_attached : return lcd_id, 3bytes, id1, id2, id3, from command line
 * get_samsung_lcd_attached : return lcd attched or detached
 */
extern int get_lcd_attached(char *mode);
extern int get_samsung_lcd_attached(void);

/*
 * If in DeviceTree structure, using below tree.
 * below tree is connected fts_parse_dt() in fts_ts.c file.
 */
/*
fts_i2c@49 {
	compatible = "stm,fts_ts";
	reg = <0x49>;
	interrupt-parent = <&msmgpio>;
	interrupts = <79 0>;
	vcc_en-supply = <&pma8084_lvs3>;
	stm,supply-num = <2>;		// the number of regulators
	stm,supply-name = "8084_l17", "8084_lvs3";	// the name of regulators, in power-on order
	stm,irq-gpio = <&msmgpio 79 0x1>;
	stm,tsp-coords = <1079 1919>;
	stm,support_func = <1 1>; // <support_hover, support_mshover>
	stm,i2c-pull-up = <1>;
	stm,tsp-project = "TR_LTE";
	stm,tsp-extra_config = <0 0 0 0>;// <"pwrctrl", "pwrctrl(sub)", "octa_id", "N">
};

*/
#endif				//_LINUX_FTS_TS_H_
