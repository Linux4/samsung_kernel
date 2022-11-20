/*
 * MELFAS MIP4 Touchscreen
 *
 * Copyright (C) 2000-2018 MELFAS Inc.
 *
 *
 * mip4_ts.h
 *
 * Version : 2019.08.22
 */

#ifndef _LINUX_MIP4_TS_H
#define _LINUX_MIP4_TS_H

/* Debug mode : Disable for production build */
#if 0 /* 0 = Disable, 1 = Enable */
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
#include <linux/sysfs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/time.h>
#include <linux/completion.h>
#include <linux/fb.h>
#include <linux/notifier.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#endif
#ifdef CONFIG_ACPI
#include <linux/acpi.h>
#endif
#ifdef CONFIG_REGULATOR
#include <linux/regulator/consumer.h>
#endif
#ifdef CONFIG_ANDROID
#include <linux/wakelock.h>
#endif

/* Include SEC */
#include <asm/unaligned.h>
#include <linux/ctype.h>
#include <linux/hrtimer.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/vmalloc.h>
#include <linux/workqueue.h>
//#include <linux/input/sec_cmd.h>
#ifdef CONFIG_SEC_SYSFS
#include <linux/sec_sysfs.h>
#endif
#ifdef CONFIG_INPUT_BOOSTER
#include <linux/input/input_booster.h>
#endif
//#include "../../../i2c/busses/i2c-exynos5.h"

/* Include platform data */
#if (!defined(CONFIG_OF) && !defined(CONFIG_ACPI))
#include <linux/input/melfas_mip4_ts.h>
#endif

/* Include register map */
#include "mip4_reg.h"

/* Device info */
#ifndef MIP4_TS_DEVICE_NAME
#define MIP4_TS_DEVICE_NAME "mip4_ts"
#endif
#ifdef CONFIG_ACPI
#define ACPI_ID "MLFS0000"
#endif

/* Chip model info */
#define CHIP_NONE     000000
#define CHIP_MAT100   601000
#define CHIP_MSS100   701000

#if IS_ENABLED(CONFIG_TOUCHSCREEN_MELFAS_MIP4)
#define CHIP_NAME  "MIP4"
#define CHIP_MODEL CHIP_NONE
#elif IS_ENABLED(CONFIG_TOUCHSCREEN_MELFAS_MAT100)
#define CHIP_NAME  "MAT100"
#define CHIP_MODEL CHIP_MAT100
#define FW_VER_FORMAT_U8
#elif IS_ENABLED(CONFIG_TOUCHSCREEN_MELFAS_MSS100)
#define CHIP_NAME  "MSS100"
#define CHIP_MODEL CHIP_MSS100
#define FW_VER_FORMAT_U8
#else
#define CHIP_NAME  "MIP4"
#define CHIP_MODEL CHIP_NONE
#endif

/* Config driver */
#define USE_INPUT_OPEN_CLOSE  1 /* 0 (default) or 1 */
#define I2C_RETRY_COUNT       3 /* >= 2 */
#define RESET_ON_I2C_ERROR    0 /* 0 (default) or 1 */
#define RESET_ON_EVENT_ERROR  0 /* 0 (default) or 1 */
#define ESD_COUNT_FOR_DISABLE 7 /* >= 7 */
#define POWER_ON_DELAY        150 /* ms */
#define POWER_ON_DELAY_ISC    20 /* ms */
#define POWER_OFF_DELAY       40 /* ms */
#define USE_STARTUP_WAITING   0 /* 0 (default) or 1 */
#define STARTUP_TIMEOUT       200 /* ms */
#define USE_FW_RECOVERY       0 /* 0 (default) or 1 */
#define I2C_ERR_CNT_FOR_FW_RECOVERY 10 /* >= 3 */
#define USE_FB_NOTIFY         0 /* 0 (default) or 1 */
#define VERSION_LENGTH        8 /* >= 8 */
#define DEBUG_EVENT           0 /* 0 (default) or 1 */

/* Driver features */
#define USE_DEV  1 /* 0 or 1 (default) */
#define USE_SYS  1 /* 0 or 1 (default) */
#define USE_CMD  1 /* 0 or 1 (default) */
#define USE_QUEEXO 0 /* 0 (default) or 1 */

/* Module features */
#define USE_WAKEUP_GESTURE 0 /* 0 (default) or 1 */
#define USE_LPM            1 /* 0 (default) or 1 */
#define USE_AOD            1 /* 0 or 1 (default) */
#define USE_SPONGE         1 /* 0 or 1 (default) */

#if (USE_WAKEUP_GESTURE || USE_LPM || USE_AOD)
#define USE_WAKELOCK       1 /* 0 or 1 (default) */
#else
#define USE_WAKELOCK       0 /* 0 (default) or 1 */
#endif

/* Input value */
#define MAX_FINGER_NUM        10
#define MAX_KEY_NUM           4
#define INPUT_TOUCH_MAJOR_MIN 0
#define INPUT_TOUCH_MAJOR_MAX 255
#define INPUT_TOUCH_MINOR_MIN 0
#define INPUT_TOUCH_MINOR_MAX 255
#define INPUT_PRESSURE_MIN    0
#define INPUT_PRESSURE_MAX    255

/* Firmware update */
#define FW_PATH_INTERNAL "melfas/melfas_mip4_ts.fw"  /* path of firmware included in the kernel image (/firmware) */
#define FW_PATH_EXTERNAL "/sdcard/melfas_mip4_ts.fw" /* path of firmware in the external storage */
#define USE_AUTO_FW_UPDATE      1 /* 0 (default) or 1 */
#define FW_UPDATE_DEBUG         0 /* 0 (default) or 1 */
#define EXT_FW_FORCE_UPDATE     1 /* 0 or 1 (default) */
#define MAX_RESTORE_SECTION_NUM 3 /* >= 1 */

/*
 * Firmware update error code
 */
enum fw_update_errno {
	FW_ERR_FILE_READ = -4,
	FW_ERR_FILE_OPEN = -3,
	FW_ERR_FILE_TYPE = -2,
	FW_ERR_DOWNLOAD = -1,
	FW_ERR_NONE = 0,
	FW_ERR_UPTODATE = 1,
};

/*
 * Firmware file location
 */
enum fw_bin_source {
	FW_BIN_SOURCE_KERNEL = 1,
	FW_BIN_SOURCE_EXTERNAL = 2,
};

/*
 * Flash failure type
 */
enum flash_fail_type {
	FLASH_FAIL_NONE = 0,
	FLASH_FAIL_CRITICAL = 1,
	FLASH_FAIL_SECTION = 2,
};

/*
 * Command function
 */
#if USE_CMD
#define CMD_LEN        32
#define CMD_RESULT_LEN 512
#define CMD_PARAM_NUM  8
#endif

struct aod_data {
	u8 aod_enable;
	u8 spay_enable;
	u8 doubletap_enable;
	u16 width;
	u16 height;
	u16 x;
	u16 y;
};

/*
 * Device info structure
 */
struct mip4_ts_info {
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct mip4_ts_platform_data *pdata;
	char phys[64];
	dev_t mip4_ts_dev;
	struct class *class;
	struct mutex lock;
	int irq;

	int gpio_intr;
	int gpio_ce;
	int gpio_vd33_en;
	struct regulator *regulator_vd33;
    struct regulator *regulator_vio;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pins_enable;
	struct pinctrl_state *pins_disable;

	struct completion startup_done;
	bool startup_init;
	bool init;
	bool enabled;
	bool irq_enabled;
	int power;

#if USE_FB_NOTIFY
	struct notifier_block fb_notifier;
#endif

#ifdef CONFIG_ANDROID
#if USE_WAKELOCK
	struct wake_lock wake_lock;
#endif
#endif

	char *fw_name;
	char *fw_path_ext;

	u8 product_name[16];
	int max_x;
	int max_y;
	unsigned int ppm_x;
	unsigned int ppm_y;
	u8 node_x;
	u8 node_y;
	u8 node_key;
	u8 fw_version[8];

	u8 event_size;
	u8 event_size_type;
	int event_format;

	u8 touch_state[MAX_FINGER_NUM];
	u8 wakeup_gesture_code;
	int pressure;
	int pressure_stage;
	int proximity;

	bool use_sponge;
	unsigned int scrub_id;
	unsigned int scrub_x;
	unsigned int scrub_y;

	bool key_enable;
	int key_num;
	int key_code[MAX_KEY_NUM];
	int key_code_proximity;

	u8 low_power_mode;
	u8 gesture_wakeup_mode;
	u8 glove_mode;
	u8 charger_mode;
	u8 cover_mode;

	int i2c_error_cnt;
	u8 esd_cnt;
	bool disable_esd;

	unsigned int sram_addr_num;
	u32 sram_addr[8];

	u8 *print_buf;
	u8 *debug_buf;
	int *image_buf;
	int *diff_buf;

	int touch_count;

	/* for diff */
	int row_diff;
	int col_diff;
	int data_size_diff;

	bool test_busy;
	bool cmd_busy;
	bool dev_busy;

	bool debug_event;

#if 0 //USE_CMD
	struct sec_cmd_data cmd;
	/*
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
	*/
#endif

#if USE_DEV
	struct cdev cdev;
	u8 *dev_fs_buf;
#endif

#if USE_AOD
	struct aod_data aod;
#endif
};

/*
 * Function declarations
 */

/* Main */
void mip4_ts_reset(struct mip4_ts_info *info);
void mip4_ts_restart(struct mip4_ts_info *info);
int mip4_ts_i2c_read(struct mip4_ts_info *info, char *write_buf, unsigned int write_len, char *read_buf, unsigned int read_len);
int mip4_ts_i2c_write(struct mip4_ts_info *info, char *write_buf, unsigned int write_len);
int mip4_ts_enable(struct mip4_ts_info *info);
int mip4_ts_disable(struct mip4_ts_info *info);
int mip4_ts_get_ready_status(struct mip4_ts_info *info);
int mip4_ts_get_fw_version(struct mip4_ts_info *info, u8 *ver_buf);
int mip4_ts_get_fw_version_u16(struct mip4_ts_info *info, u16 *ver_buf_u16);
int mip4_ts_get_fw_version_from_bin(struct mip4_ts_info *info, u8 *ver_buf);
int mip4_ts_set_power_state(struct mip4_ts_info *info, u8 mode);
int mip4_ts_set_wakeup_gesture_type(struct mip4_ts_info *info, u32 type);
int mip4_ts_disable_esd_alert(struct mip4_ts_info *info);
int mip4_ts_alert_handler_flash(struct mip4_ts_info *info, u8 *data);
int mip4_ts_config(struct mip4_ts_info *info);
#if USE_FW_RECOVERY
int mip4_ts_fw_restore_critical_section(struct mip4_ts_info *info);
int mip4_ts_fw_restore_from_kernel(struct mip4_ts_info *info, u8 *sections, int retry);
#endif /* USE_FW_RECOVERY */
int mip4_ts_fw_update_from_kernel(struct mip4_ts_info *info);
int mip4_ts_fw_update_from_storage(struct mip4_ts_info *info, char *path, bool force);
int mip4_ts_suspend(struct device *dev);
int mip4_ts_resume(struct device *dev);

/* Mod */
int mip4_ts_startup_config(struct mip4_ts_info *info);
int mip4_ts_config_regulator(struct mip4_ts_info *info);
int mip4_ts_control_regulator(struct mip4_ts_info *info, int enable);
int mip4_ts_power_on(struct mip4_ts_info *info, int delay_ms);
int mip4_ts_power_off(struct mip4_ts_info *info, int delay_ms);
void mip4_ts_clear_input(struct mip4_ts_info *info);
int mip4_ts_gesture_wakeup_event_handler(struct mip4_ts_info *info, int gesture_code);
void mip4_ts_input_event_handler(struct mip4_ts_info *info, u8 sz, u8 *buf);
int mip4_ts_config_gpio(struct mip4_ts_info *info);
#ifdef CONFIG_OF
int mip4_ts_parse_devicetree(struct device *dev, struct mip4_ts_info *info);
#endif
void mip4_ts_config_input(struct mip4_ts_info *info);

/* Firmware */
#if (CHIP_MODEL != CHIP_NONE)
#ifdef USE_FLASH_CAL_TABLE
int mip4_ts_flash_cal_table(struct mip4_ts_info *info);
#endif /* USE_FLASH_CAL_TABLE */
#if USE_FW_RECOVERY
int mip4_ts_restore_fw(struct mip4_ts_info *info, const u8 *fw_data, size_t fw_size, u8 *sections);
#endif /* USE_FW_RECOVERY */
int mip4_ts_flash_fw(struct mip4_ts_info *info, const u8 *fw_data, size_t fw_size, bool force, bool section);
int mip4_ts_bin_fw_version(struct mip4_ts_info *info, const u8 *fw_data, size_t fw_size, u8 *ver_buf);
int mip4_ts_isc_for_power_off(struct mip4_ts_info *info);
#endif /* CHIP_MODEL */
 
/* Dev */
#if USE_DEV
int mip4_ts_dev_create(struct mip4_ts_info *info);
int mip4_ts_get_log(struct mip4_ts_info *info);
#endif
#if (USE_SYS || USE_CMD)
int mip4_ts_run_test(struct mip4_ts_info *info, u8 test_type);
int mip4_ts_get_image(struct mip4_ts_info *info, u8 image_type);
int mip4_ts_run_calibration(struct mip4_ts_info *info, u8 type);
int mip4_ts_calc_cm_diff(struct mip4_ts_info *info, int row, int col, int data_type_size, int diff_type);
#endif
#if USE_SYS
int mip4_ts_sysfs_create(struct mip4_ts_info *info);
void mip4_ts_sysfs_remove(struct mip4_ts_info *info);
static const struct attribute_group mip4_ts_test_attr_group;
#endif

/* CMD */
#if USE_CMD
int mip4_ts_sysfs_cmd_create(struct mip4_ts_info *info);
void mip4_ts_sysfs_cmd_remove(struct mip4_ts_info *info);
static const struct attribute_group mip4_ts_cmd_attr_group;
#endif

/* AOD */
#if USE_AOD
#if USE_SPONGE
#else
int mip4_ts_custom_event_handler(struct mip4_ts_info *info, u8 *rbuf, u8 size);
#endif
int mip4_ts_aod_config(struct mip4_ts_info *info);
#endif

/* Sponge */
#if USE_SPONGE
int mip4_ts_sponge_read(struct mip4_ts_info *info, u16 addr, u8 *buf, u8 len);
int mip4_ts_sponge_write(struct mip4_ts_info *info, u16 addr, u8 *buf, u8 len);
int mip4_ts_custom_event_handler(struct mip4_ts_info *info, u8 *rbuf, u8 size);
void mip4_ts_sponge_write_time(struct mip4_ts_info *info, u32 val);
#endif

#endif
