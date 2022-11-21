/*
 * MELFAS MIP4 Touchscreen
 *
 * Copyright (C) 2015 MELFAS Inc.
 *
 *
 * mip4.h : Main header
 *
 *
 * Version : 2015.10.16
 *
 */

/* Debug mode : Must be disabled for production builds */
#if 0
#define DEBUG
#endif

/* Include */
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
#include <soc/sprd/i2c-sprd.h>
#include <soc/sprd/pinmap.h>
#include <soc/sprd/sci.h>
#include <soc/sprd/sci_glb_regs.h>
#include <soc/sprd/hardware.h>
#include <soc/sprd/gpio.h>
#include <linux/power_supply.h>
#include <linux/mfd/muic_noti.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

/* Include platform data */
#include <linux/input/melfas_mip4.h>

/* Include register map */
#include "mip4_reg.h"

/* Chip info */
#define CHIP_NONE		000000
#define CHIP_MMS427	104270
#define CHIP_MMS438	104380
#define CHIP_MMS449	104490
#define CHIP_MMS458	104580
#define CHIP_MMS500	105000
#define CHIP_MCS8040L	280401
#define CHIP_MIT200	302000
#define CHIP_MIT300	303000
#define CHIP_MIT400	304000
#define CHIP_MFS10		400100

#ifdef CONFIG_TOUCHSCREEN_MELFAS_MIP4
#define CHIP_NAME		"MIP4"
#define CHIP_MODEL		CHIP_NONE
#endif
#ifdef CONFIG_TOUCHSCREEN_MELFAS_MMS427
#define CHIP_NAME		"MMS427"
#define CHIP_MODEL		CHIP_MMS427
#endif
#ifdef CONFIG_TOUCHSCREEN_MELFAS_MMS438
#define CHIP_NAME		"MMS438"
#define CHIP_MODEL		CHIP_MMS438
#endif
#ifdef CONFIG_TOUCHSCREEN_MELFAS_MMS449
#define CHIP_NAME		"MMS449"
#define CHIP_MODEL		CHIP_MMS449
#endif
#ifdef CONFIG_TOUCHSCREEN_MELFAS_MMS458
#define CHIP_NAME		"MMS458"
#define CHIP_MODEL		CHIP_MMS458
#endif
#ifdef CONFIG_TOUCHSCREEN_MELFAS_MMS500
#define CHIP_NAME		"MMS500"
#define CHIP_MODEL		CHIP_MMS500
#endif
#ifdef CONFIG_TOUCHSCREEN_MELFAS_MCS8040L
#define CHIP_NAME		"MCS8040L"
#define CHIP_MODEL		CHIP_MCS8040L
#endif
#ifdef CONFIG_TOUCHSCREEN_MELFAS_MIT200
#define CHIP_NAME		"MIT200"
#define CHIP_MODEL		CHIP_MIT200
#endif
#ifdef CONFIG_TOUCHSCREEN_MELFAS_MIT300
#define CHIP_NAME		"MIT300"
#define CHIP_MODEL		CHIP_MIT300
#endif
#ifdef CONFIG_TOUCHSCREEN_MELFAS_MIT400
#define CHIP_NAME		"MIT400"
#define CHIP_MODEL		CHIP_MIT400
#endif
#ifdef CONFIG_TOUCHSCREEN_MELFAS_MFS10
#define CHIP_NAME		"MFS10"
#define CHIP_MODEL		CHIP_MFS10
#endif

/* Config driver */
#define MIP_USE_INPUT_OPEN_CLOSE	0	/* 0 (default) or 1 */
#define MIP_INPUT_REPORT_TYPE		1	/* 0 or 1 (default) */
#define MIP_AUTOSET_RESOLUTION		1	/* 0 or 1 (default) */
#define MIP_AUTOSET_EVENT_FORMAT	1	/* 0 or 1 (default) */
#define MIP_USE_CALLBACK		1
#define I2C_RETRY_COUNT			3	/* 2~ */
#define RESET_ON_I2C_ERROR		1	/* 0 or 1 (default) */
#define RESET_ON_EVENT_ERROR		0	/* 0 (default) or 1 */
#define ESD_COUNT_FOR_DISABLE		7	/* 7~ */

/* Driver features */
#define MIP_USE_DEV			1	/* 0 or 1 (default) : Optional - Required for development */
#define MIP_USE_SYS			1	/* 0 or 1 (default) : Optional - Required for development */
#define MIP_USE_CMD			1	/* 0 (default) or 1 : Optional */
#define MIP_USE_LPWG			0	/* 0 (default) or 1 : Optional */

/* Module features */
#define MIP_USE_WAKEUP_GESTURE	0	/* 0 (default) or 1 */

/* Input value */
#define MAX_FINGER_NUM			10
#define MAX_KEY_NUM			4
#define INPUT_TOUCH_MAJOR_MIN		0
#define INPUT_TOUCH_MAJOR_MAX		255
#define INPUT_TOUCH_MINOR_MIN		0
#define INPUT_TOUCH_MINOR_MAX		255
#define INPUT_PRESSURE_MIN		0
#define INPUT_PRESSURE_MAX		4095
#define INPUT_PALM_MIN			0
#define INPUT_PALM_MAX			1

/* Firmware update */
#define FW_PATH_INTERNAL			"melfas/j1mini3g.fw"	/* path of firmware included in the kernel image (/firmware) */
#define FW_PATH_EXTERNAL			"/sdcard/j1mini3g.fw"	/* path of firmware in external storage */

#define MIP_USE_AUTO_FW_UPDATE		1	/* 0 (default) or 1 */
#define MIP_FW_MAX_SECT_NUM		4
#define MIP_FW_UPDATE_DEBUG		0	/* 0 (default) or 1 */
#define MIP_FW_UPDATE_SECTION		0	/* 0 (default) or 1 */
#define MIP_EXT_FW_FORCE_UPDATE		0	/* 0 or 1 (default) */

/**
* Firmware update error code
*/
enum fw_update_errno {
	fw_err_file_read = -4,
	fw_err_file_open = -3,
	fw_err_file_type = -2,
	fw_err_download = -1,
	fw_err_none = 0,
	fw_err_uptodate = 1,
};

/**
* Firmware file location
*/
enum fw_bin_source {
	fw_bin_source_kernel = 1,
	fw_bin_source_external = 2,
};

/* Command function */
#if MIP_USE_CMD
#define CMD_LEN				32
#define CMD_RESULT_LEN			512
#define CMD_PARAM_NUM			8
#endif

/* Callback functions */
#if MIP_USE_CALLBACK
struct mip_callbacks {
	struct notifier_block nb;
	void (*inform_charger) (struct mip_callbacks *, int);
	/* void (*inform_display) (struct mip_callbacks *, int); */
};
#endif

/**
* Device info structure
*/
struct mip_ts_info {
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct melfas_platform_data *pdata;
	char phys[32];
	dev_t mip_dev;
	struct class *class;
	struct mutex lock;
	int irq;

#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif

	struct pinctrl *pinctrl;
	struct pinctrl_state	*pins_default;
	struct pinctrl_state	*pins_sleep;

	bool init;
	bool enabled;
	bool irq_enabled;
	int power;

	char *fw_name;
	char *fw_path_ext;

	u8 product_name[16];
	int max_x;
	int max_y;
	u8 node_x;
	u8 node_y;
	u8 node_key;
	u8 fw_version[8];

	u8 event_size;
	int event_format;
	int finger_cnt;
	u8 touch_state[MAX_FINGER_NUM];
	u8 wakeup_gesture_code;

	bool key_enable;
	int key_num;
	int key_code[MAX_KEY_NUM];

	u8 gesture_wakeup_mode;
	u8 glove_mode;
	u8 charger_mode;
	u8 cover_mode;

	u8 esd_cnt;
	bool disable_esd;

	u8 *print_buf;
	u8 *debug_buf;
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
	struct mip_callbacks cb;
	int charger_state;
#endif
	int proximity_state;
	int proximity_value;
};

/**
* Function declarations
*/
/* main */
void mip_reboot(struct mip_ts_info *info);
int mip_i2c_read(struct mip_ts_info *info, char *write_buf, unsigned int write_len, char *read_buf, unsigned int read_len);
int mip_i2c_read_next(struct mip_ts_info *info, char *write_buf, unsigned int write_len, char *read_buf, int start_idx, unsigned int read_len);
int mip_i2c_write(struct mip_ts_info *info, char *write_buf, unsigned int write_len);
int mip_enable(struct mip_ts_info *info);
int mip_disable(struct mip_ts_info *info);
int mip_get_ready_status(struct mip_ts_info *info);
int mip_get_vendor_id(struct mip_ts_info *info, u8 *ven_buf);
int mip_get_hw_category(struct mip_ts_info *info, u8 *hw_buf);
int mip_get_parameter_version(struct mip_ts_info *info, u8 *param_buf);
int mip_get_fw_version(struct mip_ts_info *info, u8 *ver_buf);
int mip_get_fw_version_u16(struct mip_ts_info *info, u16 *ver_buf_u16);
int mip_get_fw_version_from_bin(struct mip_ts_info *info, u8 *ver_buf);
int mip_set_power_state(struct mip_ts_info *info, u8 mode);
int mip_set_wakeup_gesture_type(struct mip_ts_info *info, u32 type);
int mip_disable_esd_alert(struct mip_ts_info *info);
int mip_fw_update_from_kernel(struct mip_ts_info *info, bool force);
int mip_fw_update_from_storage(struct mip_ts_info *info, char *path, bool force);
int mip_init_config(struct mip_ts_info *info);
int mip_suspend(struct device *dev);
int mip_resume(struct device *dev);
#ifdef CONFIG_HAS_EARLYSUSPEND
void mip_early_suspend(struct early_suspend *h);
void mip_late_resume(struct early_suspend *h);
#endif

/* mod */
int mip_regulator_control(struct mip_ts_info *info, int enable);
int mip_power_on(struct mip_ts_info *info);
int mip_power_off(struct mip_ts_info *info);
void mip_clear_input(struct mip_ts_info *info);
int mip_gesture_wakeup_event_handler(struct mip_ts_info *info, int gesture_code);
void mip_input_event_handler(struct mip_ts_info *info, u8 sz, u8 *buf);
#if MIP_USE_DEVICETREE
int mip_parse_devicetree(struct device *dev, struct mip_ts_info *info);
#endif
void mip_config_input(struct mip_ts_info *info);
#if MIP_USE_CALLBACK
void mip_config_callback(struct mip_ts_info *info);
#endif

/* firmware */
#if (CHIP_MODEL != CHIP_NONE)
int mip_flash_fw(struct mip_ts_info *info, const u8 *fw_data, size_t fw_size, bool force, bool section);
int mip_bin_fw_version(struct mip_ts_info *info, const u8 *fw_data, size_t fw_size, u8 *ver_buf);
#endif

/* debug */
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

/* cmd */
#if MIP_USE_CMD
int mip_sysfs_cmd_create(struct mip_ts_info *info);
void mip_sysfs_cmd_remove(struct mip_ts_info *info);
static const struct attribute_group mip_cmd_attr_group;
#endif

/* lpwg */
#if MIP_USE_LPWG
int mip_lpwg_event_handler(struct mip_ts_info *info, u8 *rbuf, u8 size);
#endif

