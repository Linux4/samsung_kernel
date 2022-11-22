/*
 * Synaptics DSX touchscreen driver
 *
 * Copyright (C) 2012 Synaptics Incorporated
 *
 * Copyright (C) 2012 Alexandra Chin <alexandra.chin@tw.synaptics.com>
 * Copyright (C) 2012 Scott Lin <scott.lin@tw.synaptics.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef _SYNAPTICS_RMI4_H_
#define _SYNAPTICS_RMI4_H_

#define SYNAPTICS_RMI4_DRIVER_VERSION "DS5 1.0"
#include <linux/device.h>
#include <linux/i2c/synaptics_rmi.h>
#include <linux/regulator/consumer.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#define SYNAPTICS_DEVICE_NAME	"SEC_XX_XX"

/* To support suface touch, firmware should support data
 * which is required related app ex) MT_ANGLE, MT_PALM ...
 * Synpatics IC report those data through F51's edge swipe
 * fucntionality.
 */

/* feature define */
#define TSP_BOOSTER	/* DVFS feature : TOUCH BOOSTER */
#define	USE_OPEN_CLOSE
#define REPORT_2D_W
#define REDUCE_I2C_DATA_LENGTH

#if defined(CONFIG_SEC_MONDRIAN_PROJECT)
#define TOUCHKEY_ENABLE
#define USE_RECENT_TOUCHKEY
#define PROXIMITY
#define EDGE_SWIPE
#define REPORT_ANGLE
#define TKEY_BOOSTER

#elif defined(CONFIG_SEC_K_PROJECT)
#undef TSP_INIT_COMPLETE
#define PROXIMITY
#define EDGE_SWIPE
#define SIDE_TOUCH
#define USE_HOVER_REZERO
#define GLOVE_MODE
#undef TSP_TURNOFF_AFTER_PROBE
#define USE_SHUTDOWN_CB
#define CHECK_BASE_FIRMWARE
#define REPORT_ANGLE
#undef REPORT_ORIENTATION
#define USE_ACTIVE_REPORT_RATE
#define USE_F51_OFFSET_CALCULATE

#elif defined(CONFIG_SEC_KACTIVE_PROJECT)
#undef TSP_INIT_COMPLETE
#define PROXIMITY
#define EDGE_SWIPE
#define SIDE_TOUCH
#define USE_HOVER_REZERO
#define GLOVE_MODE
#undef TSP_TURNOFF_AFTER_PROBE
#define USE_SHUTDOWN_CB
#undef CHECK_BASE_FIRMWARE
#define REPORT_ANGLE
#undef REPORT_ORIENTATION
#define USE_ACTIVE_REPORT_RATE

#elif defined(CONFIG_SEC_H_PROJECT)
#undef TSP_INIT_COMPLETE
#define PROXIMITY
#define EDGE_SWIPE
#define USE_HOVER_REZERO
#define GLOVE_MODE
#undef TSP_TURNOFF_AFTER_PROBE
#define READ_LCD_ID
#define REPORT_ANGLE

#elif defined(CONFIG_SEC_F_PROJECT)
#define PROXIMITY
#define EDGE_SWIPE
#define USE_HOVER_REZERO
#define GLOVE_MODE
#define TOUCHKEY_ENABLE
#define EDGE_SWIPE_SCALE
#define TOUCHKEY_LED_GPIO
#define REPORT_ANGLE

#else /* default undefine all */
#undef TSP_INIT_COMPLETE
#undef TOUCHKEY_ENABLE
#undef PROXIMITY
#undef EDGE_SWIPE
#undef SIDE_TOUCH
#undef USE_HOVER_REZERO
#undef GLOVE_MODE
#undef TSP_TURNOFF_AFTER_PROBE
#undef USE_SHUTDOWN_CB
#undef READ_LCD_ID
#undef HAND_GRIP_MODE
#undef CHECK_BASE_FIRMWARE
#undef EDGE_SWIPE_SCALE
#undef TOUCHKEY_LED_GPIO
#undef REPORT_ANGLE
#undef TKEY_BOOSTER
#endif

#if defined(CONFIG_LEDS_CLASS) && defined(TOUCHKEY_ENABLE)
#include <linux/leds.h>
#define TOUCHKEY_BACKLIGHT "button-backlight"
#endif

#if defined(TSP_BOOSTER) || defined(TKEY_BOOSTER)
#define DVFS_STAGE_NINTH	9
#define DVFS_STAGE_TRIPLE	3
#define DVFS_STAGE_DUAL		2
#define DVFS_STAGE_SINGLE	1
#define DVFS_STAGE_NONE		0
#include <linux/cpufreq.h>

#define TOUCH_BOOSTER_OFF_TIME	500
#define TOUCH_BOOSTER_CHG_TIME	130
#define TOUCH_BOOSTER_HIGH_OFF_TIME	1000
#define TOUCH_BOOSTER_HIGH_CHG_TIME	500
#endif

/* TA_CON mode @ H mode */
#define TA_CON_REVISION		0xFF

#ifdef GLOVE_MODE
#define GLOVE_MODE_EN (1 << 0)
#define CLOSED_COVER_EN (1 << 1)
#define FAST_DETECT_EN (1 << 2)
#endif

#define SYNAPTICS_HW_RESET_TIME	100
#define SYNAPTICS_POWER_MARGIN_TIME	150

#define SYNAPTICS_PRODUCT_ID_NONE	0
#define SYNAPTICS_PRODUCT_ID_S5000	1
#define SYNAPTICS_PRODUCT_ID_S5050	2
#define SYNAPTICS_PRODUCT_ID_S5100	3
#define SYNAPTICS_PRODUCT_ID_S5700	4
#define SYNAPTICS_PRODUCT_ID_S5707	5
#define SYNAPTICS_PRODUCT_ID_S5708	6

#define SYNAPTICS_IC_REVISION_A0	0xA0
#define SYNAPTICS_IC_REVISION_A1	0xA1
#define SYNAPTICS_IC_REVISION_A2	0xA2
#define SYNAPTICS_IC_REVISION_A3	0xA3
#define SYNAPTICS_IC_REVISION_B0	0xB0
#define SYNAPTICS_IC_REVISION_B1	0xB1
#define SYNAPTICS_IC_REVISION_B2	0xB2
#define SYNAPTICS_IC_REVISION_AF	0xAF
#define SYNAPTICS_IC_REVISION_BF	0xBF

/* old bootloader(V606) do not supply Guest Thread image.
 * new bootloader(V646) supply Guest Thread image.
 */
#define SYNAPTICS_IC_OLD_BOOTLOADER	"06"
#define SYNAPTICS_IC_NEW_BOOTLOADER	"46"

#define FW_IMAGE_NAME_NONE		NULL
#define FW_IMAGE_NAME_S5000		"tsp_synaptics/synaptics_s5000.fw"
#define FW_IMAGE_NAME_S5050_H		"tsp_synaptics/synaptics_s5050_h.fw"
#define FW_IMAGE_NAME_S5050_K		"tsp_synaptics/synaptics_s5050_k.fw"
#define FW_IMAGE_NAME_S5100_K_WQHD	"tsp_synaptics/synaptics_s5100_k_wqhd.fw"
#define FW_IMAGE_NAME_S5100_K_FHD	"tsp_synaptics/synaptics_s5100_k_fhd.fw"
#define FW_IMAGE_NAME_S5100_K_A2_FHD	"tsp_synaptics/synaptics_s5100_k_a2_fhd.fw"
#define FW_IMAGE_NAME_S5707		"tsp_synaptics/synaptics_s5707.fw"
#define FW_IMAGE_NAME_S5708		"tsp_synaptics/synaptics_s5708.fw"
#define FW_IMAGE_NAME_S5050		"tsp_synaptics/synaptics_s5050.fw"
#define FW_IMAGE_NAME_S5050_F		"tsp_synaptics/synaptics_s5050_f.fw"

#define SYNAPTICS_FACTORY_TEST_PASS	2
#define SYNAPTICS_FACTORY_TEST_FAIL	1
#define SYNAPTICS_FACTORY_TEST_NONE	0

#define SYNAPTICS_MAX_FW_PATH		64

#define SYNAPTICS_DEFAULT_UMS_FW	"/sdcard/synaptics.fw"

#define DATE_OF_FIRMWARE_BIN_OFFSET	0xEF00
#define IC_REVISION_BIN_OFFSET		0xEF02
#define FW_VERSION_BIN_OFFSET		0xEF03

#define DATE_OF_FIRMWARE_BIN_OFFSET_S5050	0x016D00
#define IC_REVISION_BIN_OFFSET_S5050		0x016D02
#define FW_VERSION_BIN_OFFSET_S5050		0x016D03

#define DATE_OF_FIRMWARE_BIN_OFFSET_S5100_A2	0x00B0
#define IC_REVISION_BIN_OFFSET_S5100_A2		0x00B2
#define FW_VERSION_BIN_OFFSET_S5100_A2		0x00B3

#define PDT_PROPS (0X00EF)
#define PDT_START (0x00E9)
#define PDT_END (0x000A)
#define PDT_ENTRY_SIZE (0x0006)
#define PAGES_TO_SERVICE (10)
#define PAGE_SELECT_LEN (2)

#define SYNAPTICS_RMI4_F01 (0x01)
#define SYNAPTICS_RMI4_F11 (0x11)
#define SYNAPTICS_RMI4_F12 (0x12)
#define SYNAPTICS_RMI4_F1A (0x1a)
#define SYNAPTICS_RMI4_F34 (0x34)
#define SYNAPTICS_RMI4_F51 (0x51)
#define SYNAPTICS_RMI4_F54 (0x54)
#define SYNAPTICS_RMI4_F55 (0x55)
#define SYNAPTICS_RMI4_F60 (0x60)
#define SYNAPTICS_RMI4_FDB (0xdb)

#define SYNAPTICS_RMI4_PRODUCT_INFO_SIZE	2
#define SYNAPTICS_RMI4_DATE_CODE_SIZE		3
#define SYNAPTICS_RMI4_PRODUCT_ID_SIZE		10
#define SYNAPTICS_RMI4_BUILD_ID_SIZE		3
#define SYNAPTICS_RMI4_PRODUCT_ID_LENGTH	10
#define SYNAPTICS_RMI4_PACKAGE_ID_SIZE		4

#define MAX_NUMBER_OF_BUTTONS	4
#define MAX_INTR_REGISTERS	4
#define MAX_NUMBER_OF_FINGERS	10
#define F12_FINGERS_TO_SUPPORT	10

#define MASK_16BIT 0xFFFF
#define MASK_8BIT 0xFF
#define MASK_7BIT 0x7F
#define MASK_6BIT 0x3F
#define MASK_5BIT 0x1F
#define MASK_4BIT 0x0F
#define MASK_3BIT 0x07
#define MASK_2BIT 0x03
#define MASK_1BIT 0x01

#define F12_FINGERS_TO_SUPPORT 10

#define INVALID_X 65535
#define INVALID_Y 65535

#define RPT_TYPE (1 << 0)
#define RPT_X_LSB (1 << 1)
#define RPT_X_MSB (1 << 2)
#define RPT_Y_LSB (1 << 3)
#define RPT_Y_MSB (1 << 4)
#define RPT_Z (1 << 5)
#define RPT_WX (1 << 6)
#define RPT_WY (1 << 7)
#define RPT_DEFAULT (RPT_TYPE | RPT_X_LSB | RPT_X_MSB | RPT_Y_LSB | RPT_Y_MSB)

#ifdef PROXIMITY
#define F51_FINGER_TIMEOUT 50 /* ms */
#define HOVER_Z_MAX (255)

#define EDGE_SWIPE_DEGREES_MAX (90)
#define EDGE_SWIPE_DEGREES_MIN (-89)
#define EDGE_SWIPE_WIDTH_SCALING_FACTOR (9)

#define F51_PROXIMITY_ENABLES_OFFSET (0)
#define F51_SIDE_BUTTON_THRESHOLD_OFFSET	(47)
#define F51_SIDE_BUTTON_PARTIAL_ENABLE_OFFSET	(44)
#ifdef USE_F51_OFFSET_CALCULATE
#define F51_GPIP_EDGE_EXCLUSION_RX_OFFSET (47)
#else
#define F51_GPIP_EDGE_EXCLUSION_RX_OFFSET	(32)
#endif


#define FINGER_HOVER_DIS (0 << 0)
#define FINGER_HOVER_EN (1 << 0)
#define AIR_SWIPE_EN (1 << 1)
#define LARGE_OBJ_EN (1 << 2)
#define HOVER_PINCH_EN (1 << 3)
/* This command is reserved..
#define NO_PROXIMITY_ON_TOUCH_EN (1 << 5)
#define CONTINUOUS_LOAD_REPORT_EN (1 << 6)
*/
#define ENABLE_HANDGRIP_RECOG (1 << 6)
#define SLEEP_PROXIMITY (1 << 7)

#define F51_GENERAL_CONTROL_OFFSET (1)
#define F51_GENERAL_CONTROL2_OFFSET (2)
#define JIG_TEST_EN	(1 << 0)
#define JIG_COMMAND_EN	(1 << 1)
#define NO_PROXIMITY_ON_TOUCH (1 << 2)
#define CONTINUOUS_LOAD_REPORT (1 << 3)
#define HOST_REZERO_COMMAND (1 << 4)
#define EDGE_SWIPE_EN (1 << 5)
#define HSYNC_STATUS	(1 << 6)
#define F51_GENERAL_CONTROL (NO_PROXIMITY_ON_TOUCH | HOST_REZERO_COMMAND | EDGE_SWIPE_EN)
#define F51_GENERAL_CONTROL_NO_HOST_REZERO (NO_PROXIMITY_ON_TOUCH | EDGE_SWIPE_EN)

/* f51_query feature(proximity_controls) */
#define HAS_FINGER_HOVER (1 << 0)
#define HAS_AIR_SWIPE (1 << 1)
#define HAS_LARGE_OBJ (1 << 2)
#define HAS_HOVER_PINCH (1 << 3)
#define HAS_EDGE_SWIPE (1 << 4)
#define HAS_SINGLE_FINGER (1 << 5)
#define HAS_GRIP_SUPPRESSION (1 << 6)
#define HAS_PALM_REJECTION (1 << 7)
/* f51_query feature-2(proximity_controls_2) */
#define HAS_PROFILE_HANDEDNESS (1 << 0)
#define HAS_LOWG (1 << 1)
#define HAS_FACE_DETECTION (1 << 2)
#define HAS_SIDE_BUTTONS (1 << 3)
#define HAS_CAMERA_GRIP_DETECTION (1 << 4)
/* Reserved 5~7 */

#ifdef EDGE_SWIPE

#if defined(CONFIG_SEC_K_PROJECT) || defined(CONFIG_SEC_KACTIVE_PROJECT)
#define EDGE_SWIPE_DATA_OFFSET	11
#elif defined(CONFIG_SEC_MONDRIAN_PROJECT)
#define EDGE_SWIPE_DATA_OFFSET	3
#else
#define EDGE_SWIPE_DATA_OFFSET	9
#endif

#define EDGE_SWIPE_WIDTH_MAX	255
#define EDGE_SWIPE_ANGLE_MIN	(-90)
#define EDGE_SWIPE_ANGLE_MAX	90
#define EDGE_SWIPE_PALM_MAX		1
#endif

#define F51_DATA_RESERVED_SIZE	(1)
#define F51_DATA_1_SIZE (4)	/* FINGER HOVER */
#define F51_DATA_2_SIZE (1)	/* HOVER PINCH */
#define F51_DATA_3_SIZE (1)	/* AIR_SWIPE  | LARGE_OBJ */
#define F51_DATA_4_SIZE (2)	/* SIDE_BUTTON */
#define F51_DATA_5_SIZE (1)	/* CAMERA GRIP DETECTION */
#define F51_DATA_6_SIZE (1)	/* DETECTION FLAG2 */
#define F51_DATA_RESERVED_SIZE	(1) /* RESERVED DATA SIZE */
/*
 * DATA_5, DATA_6, RESERVED
 * 5: 1 byte RESERVED + CAM GRIP DETECT
 * 6: 1 byte RESERVED + SIDE BUTTON DETECT + HAND EDGE SWIPE DETECT
 * R: 1 byte RESERVED
 */
#endif

#define SYN_I2C_RETRY_TIMES 10
#define MAX_F11_TOUCH_WIDTH 15

#define CHECK_STATUS_TIMEOUT_MS 100

#define F01_STD_QUERY_LEN 21
#define F01_BUID_ID_OFFSET 18
#define F01_PACKAGE_ID_LEN	4
#define F11_STD_QUERY_LEN 9
#define F11_STD_CTRL_LEN 10
#define F11_STD_DATA_LEN 12

#define STATUS_NO_ERROR 0x00
#define STATUS_RESET_OCCURRED 0x01
#define STATUS_INVALID_CONFIG 0x02
#define STATUS_DEVICE_FAILURE 0x03
#define STATUS_CONFIG_CRC_FAILURE 0x04
#define STATUS_FIRMWARE_CRC_FAILURE 0x05
#define STATUS_CRC_IN_PROGRESS 0x06

#define NORMAL_OPERATION (0 << 0)
#define SENSOR_SLEEP (1 << 0)
#define NO_SLEEP_OFF (0 << 2)
#define NO_SLEEP_ON (1 << 2)
#define CHARGER_CONNECTED (1 << 5)
#define CHARGER_DISCONNECTED	0xDF

#define CONFIGURED (1 << 7)

#define TSP_NEEDTO_REBOOT	(-ECONNREFUSED)
#define MAX_TSP_REBOOT		3

/*
 * struct synaptics_rmi4_fn_desc - function descriptor fields in PDT
 * @query_base_addr: base address for query registers
 * @cmd_base_addr: base address for command registers
 * @ctrl_base_addr: base address for control registers
 * @data_base_addr: base address for data registers
 * @intr_src_count: number of interrupt sources
 * @fn_number: function number
 */
struct synaptics_rmi4_fn_desc {
	unsigned char query_base_addr;
	unsigned char cmd_base_addr;
	unsigned char ctrl_base_addr;
	unsigned char data_base_addr;
	unsigned char intr_src_count;
	unsigned char fn_number;
};

/*
 * synaptics_rmi4_fn_full_addr - full 16-bit base addresses
 * @query_base: 16-bit base address for query registers
 * @cmd_base: 16-bit base address for data registers
 * @ctrl_base: 16-bit base address for command registers
 * @data_base: 16-bit base address for control registers
 */
struct synaptics_rmi4_fn_full_addr {
	unsigned short query_base;
	unsigned short cmd_base;
	unsigned short ctrl_base;
	unsigned short data_base;
};

struct synaptics_rmi4_f12_extra_data {
	unsigned char data1_offset;
	unsigned char data15_offset;
	unsigned char data15_size;
	unsigned char data15_data[(F12_FINGERS_TO_SUPPORT + 7) / 8];
};

/*
 * struct synaptics_rmi4_fn - function handler data structure
 * @fn_number: function number
 * @num_of_data_sources: number of data sources
 * @num_of_data_points: maximum number of fingers supported
 * @size_of_data_register_block: data register block size
 * @data1_offset: offset to data1 register from data base address
 * @intr_reg_num: index to associated interrupt register
 * @intr_mask: interrupt mask
 * @full_addr: full 16-bit base addresses of function registers
 * @link: linked list for function handlers
 * @data_size: size of private data
 * @data: pointer to private data
 */
struct synaptics_rmi4_fn {
	unsigned char fn_number;
	unsigned char num_of_data_sources;
	unsigned char num_of_data_points;
	unsigned char size_of_data_register_block;
	unsigned char intr_reg_num;
	unsigned char intr_mask;
	struct synaptics_rmi4_fn_full_addr full_addr;
	struct list_head link;
	int data_size;
	void *data;
	void *extra;
};

/*
 * struct synaptics_rmi4_device_info - device information
 * @version_major: rmi protocol major version number
 * @version_minor: rmi protocol minor version number
 * @manufacturer_id: manufacturer id
 * @product_props: product properties information
 * @product_info: product info array
 * @date_code: device manufacture date
 * @tester_id: tester id array
 * @serial_number: device serial number
 * @product_id_string: device product id
 * @support_fn_list: linked list for function handlers
 */
struct synaptics_rmi4_device_info {
	unsigned int version_major;
	unsigned int version_minor;
	unsigned char manufacturer_id;
	unsigned char product_props;
	unsigned char product_info[SYNAPTICS_RMI4_PRODUCT_INFO_SIZE];
	unsigned char date_code[SYNAPTICS_RMI4_DATE_CODE_SIZE];
	unsigned short tester_id;
	unsigned short serial_number;
	unsigned char product_id_string[SYNAPTICS_RMI4_PRODUCT_ID_SIZE + 1];
	unsigned char build_id[SYNAPTICS_RMI4_BUILD_ID_SIZE];
	unsigned int package_id;
	unsigned int package_rev;
	unsigned int pr_number;
	struct list_head support_fn_list;
};

/**
 * struct synaptics_finger - Represents fingers.
 * @ state: finger status.
 * @ mcount: moving counter for debug.
 */
struct synaptics_finger {
	unsigned char state;
	unsigned short mcount;
};

/**
 * synaptics_rmi_f1a_button_map
 * @ nbuttons : number of buttons
 * @ map : key map
 */
struct synaptics_rmi_f1a_button_map {
	unsigned char nbuttons;
	u32 map[4];
};

/**
 * struct synaptics_device_tree_data - power supply information
 * @ coords : horizontal, vertical max width and max hight
 * @ external_ldo : sensor power supply : 3.3V, enbled by GPIO
 * @ sub_pmic : sensor power supply : 3.3V, enabled by subp_mic MAX77826
 * @ irq_gpio : interrupt GPIO PIN defined device tree files(dtsi)
 * @ project : project name string for Firmware name
 */

struct synaptics_rmi4_device_tree_data {
	int coords[2];
	int extra_config[4];
	int external_ldo;
	int tkey_led_en;
	int irq_gpio;
	int reset_gpio;

	char swap_axes;
	char x_flip;
	char y_flip;

	struct synaptics_rmi_f1a_button_map *f1a_button_map;

	const char *project;

	int num_of_supply;
	const char **name_of_supply;
};

/*
 * struct synaptics_rmi4_data - rmi4 device instance data
 * @i2c_client: pointer to associated i2c client
 * @input_dev: pointer to associated input device
 * @board: constant pointer to platform data
 * @rmi4_mod_info: device information
 * @regulator: pointer to associated regulator
 * @rmi4_io_ctrl_mutex: mutex for i2c i/o control
 * @early_suspend: instance to support early suspend power management
 * @current_page: current page in sensor to acess
 * @button_0d_enabled: flag for 0d button support
 * @full_pm_cycle: flag for full power management cycle in early suspend stage
 * @num_of_intr_regs: number of interrupt registers
 * @f01_query_base_addr: query base address for f01
 * @f01_cmd_base_addr: command base address for f01
 * @f01_ctrl_base_addr: control base address for f01
 * @f01_data_base_addr: data base address for f01
 * @irq: attention interrupt
 * @sensor_max_x: sensor maximum x value
 * @sensor_max_y: sensor maximum y value
 * @irq_enabled: flag for indicating interrupt enable status
 * @touch_stopped: flag to stop interrupt thread processing
 * @fingers_on_2d: flag to indicate presence of fingers in 2d area
 * @sensor_sleep: flag to indicate sleep state of sensor
 * @wait: wait queue for touch data polling in interrupt thread
 * @i2c_read: pointer to i2c read function
 * @i2c_write: pointer to i2c write function
 * @irq_enable: pointer to irq enable function
 */
struct synaptics_rmi4_data {
	struct i2c_client *i2c_client;
	struct input_dev *input_dev;
	struct regulator *vcc_en;
	struct regulator *sub_pmic;
	struct synaptics_rmi4_device_tree_data *dt_data;
	struct synaptics_rmi4_device_info rmi4_mod_info;
	struct regulator_bulk_data *supplies;

	struct mutex rmi4_device_mutex;
	struct mutex rmi4_reset_mutex;
	struct mutex rmi4_io_ctrl_mutex;
	struct mutex rmi4_reflash_mutex;
	struct timer_list f51_finger_timer;

#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
	const char *firmware_name;

	struct completion init_done;
	struct synaptics_finger finger[MAX_NUMBER_OF_FINGERS];

	unsigned char current_page;
	unsigned char button_0d_enabled;
	unsigned char full_pm_cycle;
	unsigned char num_of_rx;
	unsigned char num_of_tx;
	unsigned int num_of_node;
	unsigned char num_of_fingers;
	unsigned char max_touch_width;
	unsigned char feature_enable;
	unsigned char report_enable;
	unsigned char no_sleep_setting;
	unsigned char intr_mask[MAX_INTR_REGISTERS];
	unsigned char *button_txrx_mapping;
	unsigned char bootloader_id[4];

	unsigned short num_of_intr_regs;
	unsigned short f01_query_base_addr;
	unsigned short f01_cmd_base_addr;
	unsigned short f01_ctrl_base_addr;
	unsigned short f01_data_base_addr;
	unsigned short f12_ctrl11_addr;		/* for setting jitter level*/
	unsigned short f12_ctrl15_addr;		/* for getting finger amplitude threshold */
	unsigned short f12_ctrl26_addr;
	unsigned short f12_ctrl28_addr;
	unsigned short f34_ctrl_base_addr;
	unsigned short f51_ctrl_base_addr;

	int irq;
	int sensor_max_x;
	int sensor_max_y;
	int touch_threshold;
	int gloved_sensitivity;
	int ta_status;
	bool flash_prog_mode;
	bool irq_enabled;
	bool touch_stopped;
	bool fingers_on_2d;
	bool f51_finger;
	bool f51_finger_is_hover;
	bool hand_edge_down;
	bool has_edge_swipe;
	bool has_glove_mode;
	bool has_side_buttons;
	bool sensor_sleep;
	bool stay_awake;
	bool staying_awake;
	bool tsp_probe;
	bool firmware_cracked;
	bool ta_con_mode;	/* ta_con_mode ? I2C(RMI) : INT(GPIO) */
	bool hover_status_in_normal_mode;
	bool fast_glove_state;
	bool touchkey_glove_mode_status;
	bool created_sec_class;

	int ic_version;			/* define S5000, S5050, S5700, S5707, S5708 */
	int ic_revision_of_ic;
	int ic_revision_of_bin;		/* revision of reading from binary */
	int fw_version_of_ic;		/* firmware version of IC */
	int fw_version_of_bin;		/* firmware version of binary */
	int fw_release_date_of_ic;	/* Config release data from IC */
	int lcd_id;
	unsigned int fw_pr_number;

	bool doing_reflash;
	int rebootcount;

#if defined(CONFIG_LEDS_CLASS) && defined(TOUCHKEY_ENABLE)
	struct led_classdev	leds;
#endif

#ifdef TOUCHKEY_ENABLE
	int touchkey_menu;
	int touchkey_back;
	bool touchkey_led;
	bool created_sec_tkey_class;
#endif

#ifdef CONFIG_SEC_TSP_FACTORY
	int bootmode;
#endif

#ifdef TSP_BOOSTER
	struct delayed_work	work_dvfs_off;
	struct delayed_work	work_dvfs_chg;
	struct mutex		dvfs_lock;
	bool dvfs_lock_status;
	int dvfs_old_stauts;
	int dvfs_boost_mode;
	int dvfs_freq;
#endif
#ifdef TKEY_BOOSTER
	struct delayed_work	work_tkey_dvfs_off;
	struct delayed_work	work_tkey_dvfs_chg;
	struct mutex		tkey_dvfs_lock;
	bool tkey_dvfs_lock_status;
	int tkey_dvfs_old_stauts;
	int tkey_dvfs_boost_mode;
	int tkey_dvfs_freq;
#endif

#ifdef USE_HOVER_REZERO
	struct delayed_work rezero_work;
#endif

#ifdef HAND_GRIP_MODE
	unsigned int hand_grip_mode;
	unsigned int hand_grip;
	unsigned int old_hand_grip;
	unsigned int old_code;
#endif

#ifdef SIDE_TOUCH
	bool sidekey_enables;
	unsigned char sidekey_data;
#endif

#ifdef SYNAPTICS_RMI_INFORM_CHARGER
	void (*register_cb)(struct synaptics_rmi_callbacks *);
	struct synaptics_rmi_callbacks callbacks;
#endif

	int (*i2c_read)(struct synaptics_rmi4_data *pdata, unsigned short addr,
			unsigned char *data, unsigned short length);
	int (*i2c_write)(struct synaptics_rmi4_data *pdata, unsigned short addr,
			unsigned char *data, unsigned short length);
	int (*irq_enable)(struct synaptics_rmi4_data *rmi4_data, bool enable);
	int (*reset_device)(struct synaptics_rmi4_data *rmi4_data);
	int (*stop_device)(struct synaptics_rmi4_data *rmi4_data);
	int (*start_device)(struct synaptics_rmi4_data *rmi4_data);
};

enum exp_fn {
	RMI_DEV = 0,
	RMI_F54,
	RMI_FW_UPDATER,
	RMI_DB,
	RMI_GUEST,
	RMI_LAST,
};

struct synaptics_rmi4_exp_fn_ptr {
	int (*read)(struct synaptics_rmi4_data *rmi4_data, unsigned short addr,
			unsigned char *data, unsigned short length);
	int (*write)(struct synaptics_rmi4_data *rmi4_data, unsigned short addr,
			unsigned char *data, unsigned short length);
	int (*enable)(struct synaptics_rmi4_data *rmi4_data, bool enable);
};

int synaptics_rmi4_new_function(enum exp_fn fn_type,
		int (*func_init)(struct synaptics_rmi4_data *rmi4_data),
		void (*func_remove)(struct synaptics_rmi4_data *rmi4_data),
		void (*func_attn)(struct synaptics_rmi4_data *rmi4_data,
			unsigned char intr_mask));

int rmidev_module_register(void);
int rmi4_f54_module_register(void);
int rmi4_fw_update_module_register(void);
int rmidb_module_register(void);
int rmi_guest_module_register(void);

int synaptics_rmi4_f54_set_control(struct synaptics_rmi4_data *rmi4_data);

int synaptics_fw_updater(unsigned char *fw_data);
int synaptics_rmi4_fw_update_on_probe(struct synaptics_rmi4_data *rmi4_data);
int synaptics_rmi4_proximity_enables(unsigned char enables);
int synaptics_proximity_no_sleep_set(bool enables);
int synaptics_rmi4_f12_set_feature(struct synaptics_rmi4_data *rmi4_data);
int synaptics_rmi4_set_tsp_test_result_in_config(int pass_fail);
int synaptics_rmi4_tsp_read_test_result(struct synaptics_rmi4_data *rmi4_data);
int synaptics_rmi4_f51_grip_edge_exclusion_rx(bool enables);
int synaptics_rmi4_general_ctrl2_enables(bool mode);

int synaptics_rmi4_f12_ctrl11_set (struct synaptics_rmi4_data *rmi4_data,
		unsigned char data);

int synaptics_rmi4_set_custom_ctrl_register(struct synaptics_rmi4_data *rmi4_data,
					bool mode, unsigned short address,
					int length, unsigned char *value);

#ifdef TOUCHKEY_ENABLE
int synaptics_tkey_led_vdd_on(struct synaptics_rmi4_data *rmi4_data, bool onoff);
#endif

extern struct class *sec_class;
#define SEC_CLASS_DEVT_TSP	10
#define SEC_CLASS_DEVT_TKEY	11

#ifdef CONFIG_SAMSUNG_LPM_MODE
extern int poweroff_charging;
#endif

static inline ssize_t synaptics_rmi4_show_error(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	dev_warn(dev, "%s Attempted to read from write-only attribute %s\n",
			__func__, attr->attr.name);
	return -EPERM;
}

static inline ssize_t synaptics_rmi4_store_error(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	dev_warn(dev, "%s Attempted to write to read-only attribute %s\n",
			__func__, attr->attr.name);
	return -EPERM;
}

static inline void batohs(unsigned short *dest, unsigned char *src)
{
	*dest = src[1] * 0x100 + src[0];
}

static inline void hstoba(unsigned char *dest, unsigned short src)
{
	dest[0] = src % 0x100;
	dest[1] = src / 0x100;
}
#endif
