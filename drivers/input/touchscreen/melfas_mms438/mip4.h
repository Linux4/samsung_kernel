/*
 * MELFAS MIP4 Touchscreen
 *
 * Copyright (C) 2015 MELFAS Inc.
 *
 *
 * mip4.h : Main header
 *
 */

//Config debug msg : Must be disabled for production builds
#if 0	// 0 : disable, 1 : enable
#define DEBUG
#endif

//Include
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/firmware.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/fs.h>
#include <linux/sysfs.h>
#include <linux/debugfs.h>
#include <linux/cdev.h>
#include <linux/err.h>
#include <linux/limits.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/gpio_event.h>
#include <linux/wakelock.h>
#include <asm/uaccess.h>
#include <linux/regulator/consumer.h>
#include <linux/pm_runtime.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#ifdef CONFIG_INPUT_BOOSTER
#include <linux/input/input_booster.h>
#endif

//Include platform data
#include "melfas_mip4.h"

//Include register map
#include "mip4_reg.h"

//Chip info
#ifdef CONFIG_TOUCHSCREEN_MMS438
#define CHIP_MMS438
#define CHIP_NAME		"MMS438"
#define CHIP_FW_CODE	"M4H0"
#define FW_UPDATE_TYPE	"MMS438"
#define FW_FILE_TYPE_MFSB
#endif
#ifdef CONFIG_TOUCHSCREEN_MMS449
#define CHIP_MMS449
#define CHIP_NAME		"MMS449"
#define CHIP_FW_CODE	"M4HP"
#define FW_UPDATE_TYPE	"MMS438"
#define FW_FILE_TYPE_MFSB
#endif
#ifdef CONFIG_TOUCHSCREEN_MMS458
#define CHIP_MMS458
#define CHIP_NAME		"MMS458"
#define CHIP_FW_CODE	"M4HN"
#define FW_UPDATE_TYPE	"MMS438"
#define FW_FILE_TYPE_MFSB
#endif
#ifdef CONFIG_TOUCHSCREEN_MIT300
#define CHIP_MIT300
#define CHIP_NAME		"MIT300"
#define CHIP_FW_CODE	"T3H0"
#define FW_UPDATE_TYPE	"MIT300"
#define FW_FILE_TYPE_BIN
#endif

//Config driver
#define MIP_USE_INPUT_OPEN_CLOSE		0	// 0 (default) or 1
#define MIP_AUTOSET_RESOLUTION			0	// 0 or 1 (default)
#define MIP_AUTOSET_EVENT_FORMAT		1	// 0 or 1 (default)
#define I2C_RETRY_COUNT				3	// 2~
#define RESET_ON_EVENT_ERROR			0	// 0 (default) or 1
#define ESD_COUNT_FOR_DISABLE		7	// 7~
#define MIP_USE_KEY				0
//Driver features
#define MIP_USE_DEV			0	// 0 or 1 (default) : Optional - Required for development
#define MIP_USE_SYS			0	// 0 or 1 (default) : Optional - Required for development
#define MIP_USE_CMD			1	// 0 (default) or 1 : Optional
#define MIP_USE_LPWG			0	// 0 (default) or 1 : Optional

//Module features
#define MIP_USE_WAKEUP_GESTURE	0	// 0 (default) or 1

//Input value
#define MAX_FINGER_NUM 			10
#define MAX_KEY_NUM				3
#define INPUT_PRESSURE_MIN 		0
#define INPUT_PRESSURE_MAX 		255
#define INPUT_TOUCH_MAJOR_MIN 	0
#define INPUT_TOUCH_MAJOR_MAX 	255
#define INPUT_TOUCH_MINOR_MIN 	0
#define INPUT_TOUCH_MINOR_MAX 	255
#define INPUT_ANGLE_MIN 			0
#define INPUT_ANGLE_MAX 			255
#define INPUT_PALM_MIN 			0
#define INPUT_PALM_MAX 			1

//Firmware update
#ifdef FW_FILE_TYPE_MFSB
#define FW_PATH_INTERNAL			"melfas/melfas_mip4.mfsb"	//path of firmware included in the kernel image (/firmware)
#define FW_PATH_EXTERNAL			"/sdcard/melfas_mip4.mfsb"	//path of firmware in external storage
#else
#define FW_PATH_INTERNAL			"melfas/melfas_mip4.fw"		//path of firmware included in the kernel image (/firmware)
#define FW_PATH_EXTERNAL			"/sdcard/melfas_mip4.bin"	//path of firmware in external storage
#endif
#define MIP_USE_AUTO_FW_UPDATE	1	// 0 (default) or 1
#define MIP_FW_MAX_SECT_NUM		4
#define MIP_FW_UPDATE_DEBUG		0	// 0 (default) or 1
#define MIP_FW_UPDATE_SECTION	1	// 0 (default) or 1
#define MIP_EXT_FW_FORCE_UPDATE	1	// 0 or 1 (default)

//Command function
#if MIP_USE_CMD
#define CMD_LEN 					32
#define CMD_RESULT_LEN 			512
#define CMD_PARAM_NUM 				8
#endif

#if MIP_USE_CALLBACK
//Callback functions
struct mip_callbacks {
	void (*inform_charger) (struct mip_callbacks *, int);
	//void (*inform_display) (struct mip_callbacks *, int);
	//...
};

extern struct mip_callbacks *mip_inform_callbacks;
#endif

/**
* Device info structure
*/
struct mip_ts_info {
	struct i2c_client *client;
	struct input_dev *input_dev;
	char phys[32];

	struct melfas_platform_data *pdata;

	dev_t mip_dev;
	struct class *class;

	struct mutex lock;
	struct mutex lock_test;
	struct mutex lock_cmd;
	struct mutex lock_dev;

#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif

	int irq;
	bool enabled;
	int power;
	bool init;
	char *fw_name;
	char *fw_path_ext;

	struct pinctrl *pinctrl;
	struct pinctrl_state *pins_enable;
	struct pinctrl_state *pins_disable;

	u8 product_name[16];
	int max_x;
	int max_y;
	u8 node_x;
	u8 node_y;
	u8 node_key;
	u8 fw_version[8];

	u8 touch_state[MAX_FINGER_NUM];
	u8 finger_cnt;
	u8 event_size;
	int event_format;
#if MIP_USE_KEY
	bool key_enable;
	int key_num;
	int key_code[MAX_KEY_NUM];
#endif
	u8 nap_mode;
	u8 glove_mode;
	u8 charger_mode;
	u8 cover_mode;

	u8 esd_cnt;
	bool disable_esd;
	u8 wakeup_gesture_code;

	u8 *print_buf;
	int *image_buf;

	bool test_busy;
	bool cmd_busy;
	bool dev_busy;

#if MIP_USE_CMD
	dev_t cmd_dev_t;
	struct device *cmd_dev;
	struct class *cmd_class;
	struct list_head cmd_list_head;
	u8 cmd_state;
	char cmd[CMD_LEN];
	char cmd_result[CMD_RESULT_LEN];
	int cmd_param[CMD_PARAM_NUM];
	int cmd_buffer_size;
	struct device *key_dev;
#endif

#if MIP_USE_DEV
	struct cdev cdev;
	u8 *dev_fs_buf;
#endif

#if MIP_USE_CALLBACK
	void (*register_callback)(void *);
	struct mip_callbacks callbacks;
#endif

};

/**
* Firmware binary header info
*/
struct mip_bin_hdr {
	char tag[8];
	u16	core_version;
	u16	section_num;
	u16	contains_full_binary;
	u16	reserved0;

	u32	binary_offset;
	u32	binary_length;
	u32	extention_offset;
	u32	reserved1;
} __attribute__ ((packed));

/**
* Firmware image info
*/
struct mip_fw_img {
	u16	type;
	u16	version;
	u16	start_page;
	u16	end_page;

	u32	offset;
	u32	length;
} __attribute__ ((packed));

/**
* Firmware binary tail info
*/
struct mip_bin_tail {
	u8 tail_mark[4];
	char chip_name[4];
	u32 bin_start_addr;
	u32 bin_length;

	u16 ver_boot;
	u16 ver_core;
	u16 ver_app;
	u16 ver_param;
	u8 boot_start;
	u8 boot_end;
	u8 core_start;
	u8 core_end;
	u8 app_start;
	u8 app_end;
	u8 param_start;
	u8 param_end;

	u8 checksum_type;
	u8 hw_category;
	u16 param_id;
	u32 param_length;
	u32 build_date;
	u32 build_time;

	u32 reserved1;
	u32 reserved2;
	u16 reserved3;
	u16 tail_size;
	u32 crc;
} __attribute__ ((packed));

#define MIP_BIN_TAIL_MARK		{0x4D, 0x42, 0x54, 0x01}	// M B T 0x01
#define MIP_BIN_TAIL_SIZE		64

/**
* Firmware update error code
*/
enum fw_update_errno{
	fw_err_data_alloc = -5,
	fw_err_file_read = -4,
	fw_err_file_open = -3,
	fw_err_file_type = -2,
	fw_err_download = -1,
	fw_err_none = 0,
	fw_err_uptodate = 1,
};

/**
* Function declarations
*/
//main
void mip_reboot(struct mip_ts_info *info);
int mip_i2c_read(struct mip_ts_info *info, char *write_buf, unsigned int write_len, char *read_buf, unsigned int read_len);
int mip_i2c_read_next(struct mip_ts_info *info, char *write_buf, unsigned int write_len, char *read_buf, int start_idx, unsigned int read_len);
int mip_i2c_write(struct mip_ts_info *info, char *write_buf, unsigned int write_len);
int mip_enable(struct mip_ts_info *info);
int mip_disable(struct mip_ts_info *info);
int mip_get_ready_status(struct mip_ts_info *info);
int mip_get_fw_version(struct mip_ts_info *info, u8 *ver_buf);
int mip_get_fw_version_u16(struct mip_ts_info *info, u16 *ver_buf_u16);
int mip_set_power_state(struct mip_ts_info *info, u8 mode);
int mip_set_wakeup_gesture_type(struct mip_ts_info *info, u32 type);
int mip_disable_esd_alert(struct mip_ts_info *info);
int mip_fw_update_from_kernel(struct mip_ts_info *info);
int mip_fw_update_from_storage(struct mip_ts_info *info, char *path, bool force);
int mip_suspend(struct device *dev);
int mip_resume(struct device *dev);
#ifdef CONFIG_HAS_EARLYSUSPEND
void mip_early_suspend(struct early_suspend *h);
void mip_late_resume(struct early_suspend *h);
#endif

//mod
int mip_regulator_control(struct mip_ts_info *info, int enable);
int mip_power_on(struct mip_ts_info *info);
int mip_power_off(struct mip_ts_info *info);
void mip_clear_input(struct mip_ts_info *info);
int mip_wakeup_event_handler(struct mip_ts_info *info, u8 *rbuf);
void mip_input_event_handler(struct mip_ts_info *info, u8 sz, u8 *buf);
#if MIP_USE_DEVICETREE
int mip_parse_devicetree(struct device *dev, struct mip_ts_info *info);
#endif
void mip_config_input(struct mip_ts_info *info);
#if MIP_USE_CALLBACK
void mip_config_callback(struct mip_ts_info *info);
#endif

//fw
int mip_flash_fw(struct mip_ts_info *info, const u8 *fw_data, size_t fw_size, bool force, bool section);

//debug
#if MIP_USE_DEV
int mip_dev_create(struct mip_ts_info *info);
int mip_get_log(struct mip_ts_info *info);
#endif
#if MIP_USE_SYS || MIP_USE_CMD
int mip_run_test(struct mip_ts_info *info, u8 test_type);
int mip_get_image(struct mip_ts_info *info, u8 image_type);
#endif
#if MIP_USE_SYS
int mip_sysfs_create(struct mip_ts_info *info);
void mip_sysfs_remove(struct mip_ts_info *info);
static const struct attribute_group mip_test_attr_group;
#endif

//cmd
#if MIP_USE_CMD
int mip_sysfs_cmd_create(struct mip_ts_info *info);
void mip_sysfs_cmd_remove(struct mip_ts_info *info);
static const struct attribute_group mip_cmd_attr_group;
#endif

//lpwg
#if MIP_USE_LPWG
int mip_lpwg_event_handler(struct mip_ts_info *info, u8 *rbuf, u8 size);
#endif

