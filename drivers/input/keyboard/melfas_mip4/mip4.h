/*
 * MELFAS MIP4 Touchkey
 *
 * Copyright (C) 2016 MELFAS Inc.
 *
 *
 * mip4.h
 *
 *
 * Version : 2016.02.11
 *
 */

//Debug mode : Must be disabled for production builds
#if 1	// 0 : disable, 1 : enable
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
#include <linux/uaccess.h>
#include <linux/regulator/consumer.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#include <linux/kernel.h>
#include <asm/unaligned.h>
//#include <mach/cpufreq.h>
#include <linux/input/mt.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/earlysuspend.h>
//#include <linux/i2c/touchkey_i2c.h>
#include <linux/regulator/consumer.h>
#include <asm/mach-types.h>
#include <linux/device.h>
#include <linux/of_gpio.h>
#include <linux/firmware.h>
#include <linux/uaccess.h>
#include <linux/mfd/pm8xxx/pm8921.h>

//Include platform data
#include <linux/input/melfas_mip4_tk.h>

//Include register map
#include "mip4_reg.h"

//Chip info
#define CHIP_MHS204	002040

#ifdef CONFIG_KEYBOARD_MELFAS_MHS204
#define CHIP_NAME		"MHS204"
#define CHIP_MODEL		CHIP_MHS204
#endif

//Config driver
#define MIP_USE_INPUT_OPEN_CLOSE		1	// 0 (default) or 1
#define MIP_AUTOSET_EVENT_FORMAT		1	// 0 or 1 (default)
#define I2C_RETRY_COUNT				3	// 2~
#define RESET_ON_I2C_ERROR			1	// 0 or 1 (default)
#define RESET_ON_EVENT_ERROR			0	// 0 (default) or 1
#define ESD_COUNT_FOR_DISABLE		7	// 7~

//Driver features
#define MIP_USE_DEV			1	// 0 or 1 (default) : Optional - Required for development
#define MIP_USE_SYS			1	// 0 or 1 (default) : Optional - Required for development
#define MIP_USE_CMD			1	// 0 (default) or 1 : Optional

//Module features

//Input value
#define MAX_KEY_NUM				5

//Firmware update
#define FW_PATH_INTERNAL			"melfas/melfas_mip4_tk_j2xlte.fw"		//path of firmware included in the kernel image (/firmware)
#define FW_PATH_EXTERNAL			"/sdcard/melfas_mip4_tk_j2xlte.fw"	//path of firmware in external storage

#define MIP_USE_AUTO_FW_UPDATE	1	// 0 (default) or 1
#define MIP_FW_MAX_SECT_NUM		4
#define MIP_FW_UPDATE_DEBUG		1	// 0 (default) or 1
#define MIP_FW_UPDATE_SECTION	1	// 0 (default) or 1
#define MIP_EXT_FW_FORCE_UPDATE	1	// 0 or 1 (default)

/**
* Firmware update error code
*/
enum fw_update_errno{
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
enum fw_bin_source{
	fw_bin_source_kernel = 1,	
	fw_bin_source_external = 2,	
};

//Command function
#if MIP_USE_CMD
#define CMD_LEN 					32
#define CMD_RESULT_LEN 			512
#define CMD_PARAM_NUM 				8
#endif

//Callback functions
#if MIP_USE_CALLBACK
extern struct mip4_tk_callbacks *mip4_tk_inform_callbacks;

struct mip4_tk_callbacks {
	void (*inform_charger) (struct mip4_tk_callbacks *, int);
	//void (*inform_display) (struct mip4_tk_callbacks *, int);
	//...
};
#endif

/**
* Device info structure
*/
struct mip4_tk_info {
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct mip4_tk_platform_data *pdata;
	char phys[32];
	dev_t mip4_tk_dev;
	struct class *class;
	struct mutex lock;
	int irq;
	
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif

	unsigned int vcc_en_regulator_volt;
	const char *vcc_en_regulator_name;
	struct regulator *regulator_vd33;
	struct regulator *regulator_led;
	
	struct pinctrl *pinctrl;
	struct pinctrl_state *pins_enable;
	struct pinctrl_state *pins_disable;
	
	bool init;	
	bool enabled;
	bool irq_enabled;
	int power;
	
	char *fw_name;
	char *fw_path_ext;	
	
	u8 product_name[16];
	u8 node_key;
	u8 fw_version[8];

	u8 event_size;
	int event_format;

	int key_num;
	int key_code[MAX_KEY_NUM];
	bool key_code_loaded;
	
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
	void (*register_callback)(void *);
	struct mip4_tk_callbacks callbacks;
#endif
};

/**
* Function declarations
*/
//main
void mip4_tk_reboot(struct mip4_tk_info *info);
int mip4_tk_i2c_read(struct mip4_tk_info *info, char *write_buf, unsigned int write_len, char *read_buf, unsigned int read_len);
int mip4_tk_i2c_read_next(struct mip4_tk_info *info, char *write_buf, unsigned int write_len, char *read_buf, int start_idx, unsigned int read_len);
int mip4_tk_i2c_write(struct mip4_tk_info *info, char *write_buf, unsigned int write_len);
int mip4_tk_enable(struct mip4_tk_info *info);
int mip4_tk_disable(struct mip4_tk_info *info);
int mip4_tk_get_ready_status(struct mip4_tk_info *info);
int mip4_tk_get_fw_version(struct mip4_tk_info *info, u8 *ver_buf);
int mip4_tk_get_fw_version_u16(struct mip4_tk_info *info, u16 *ver_buf_u16);
int mip4_tk_get_fw_version_from_bin(struct mip4_tk_info *info, u8 *ver_buf);
int mip4_tk_set_power_state(struct mip4_tk_info *info, u8 mode);
int mip4_tk_disable_esd_alert(struct mip4_tk_info *info);
int mip4_tk_fw_update_from_kernel(struct mip4_tk_info *info);
int mip4_tk_fw_update_from_storage(struct mip4_tk_info *info, char *path, bool force);
int mip4_tk_suspend(struct device *dev);
int mip4_tk_resume(struct device *dev);
#ifdef CONFIG_HAS_EARLYSUSPEND
void mip4_tk_early_suspend(struct early_suspend *h);
void mip4_tk_late_resume(struct early_suspend *h);
#endif

//mod
int mip4_tk_regulator_control(struct mip4_tk_info *info, int enable);
int mip4_tk_power_on(struct mip4_tk_info *info);
int mip4_tk_power_off(struct mip4_tk_info *info);
void mip4_tk_clear_input(struct mip4_tk_info *info);
void mip4_tk_input_event_handler(struct mip4_tk_info *info, u8 sz, u8 *buf);
#if MIP_USE_DEVICETREE
int mip4_tk_parse_devicetree(struct device *dev, struct mip4_tk_info *info);
#endif
void mip4_tk_config_input(struct mip4_tk_info *info);
#if MIP_USE_CALLBACK
void mip4_tk_config_callback(struct mip4_tk_info *info);
#endif

//firmware
int mip4_tk_flash_fw(struct mip4_tk_info *info, const u8 *fw_data, size_t fw_size, bool force, bool section);
int mip4_tk_bin_fw_version(struct mip4_tk_info *info, const u8 *fw_data, size_t fw_size, u8 *ver_buf);

//debug
#if MIP_USE_DEV
int mip4_tk_dev_create(struct mip4_tk_info *info);
#endif
#if MIP_USE_SYS || MIP_USE_CMD
int mip4_tk_run_test(struct mip4_tk_info *info, u8 test_type);
int mip4_tk_get_image(struct mip4_tk_info *info, u8 image_type);
#endif
#if MIP_USE_SYS
int mip4_tk_sysfs_create(struct mip4_tk_info *info);
void mip4_tk_sysfs_remove(struct mip4_tk_info *info);
static const struct attribute_group mip4_tk_test_attr_group;
#endif

//cmd
#if MIP_USE_CMD
int mip4_tk_sysfs_cmd_create(struct mip4_tk_info *info);
void mip4_tk_sysfs_cmd_remove(struct mip4_tk_info *info);
static const struct attribute_group mip4_tk_cmd_attr_group;
#endif

