/*
 * mms_ts.c - Touchscreen driver for Melfas MMS-series touch controllers
 *
 * Copyright (C) 2011 Google Inc.
 * Author: Dima Zavin <dima@android.com>
 *         Simon Wilson <simonwilson@google.com>
 *
 * ISP reflashing code based on original code from Melfas.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#define DEBUG
/* #define VERBOSE_DEBUG */
#define SEC_TSP_DEBUG
#define SEC_TSP_VERBOSE_DEBUG

/* #define FORCE_FW_FLASH */
/* #define FORCE_FW_PASS */
/* #define ESD_DEBUG */

#define SEC_TSP_FACTORY_TEST
#define SEC_TKEY_FACTORY_TEST
#define SEC_TSP_FW_UPDATE
#define TSP_BUF_SIZE 1024
#define FAIL -1
#include <linux/delay.h>
#include <linux/earlysuspend.h>
#include <linux/firmware.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <mach/gpio.h>
#include <linux/uaccess.h>
#include <asm/mach-types.h>

#include <linux/i2c/mms_ts.h>

#include <asm/unaligned.h>
#include "mms_ts_fw.h"

#ifdef CONFIG_LEDS_CLASS
#include <linux/leds.h>
#define TOUCHKEY_BACKLIGHT	"button-backlight"
#endif

#define MAX_FINGERS		10
#define MAX_WIDTH		30
#define MAX_PRESSURE		255
#define MAX_ANGLE		90
#define MIN_ANGLE		-90

/* Registers */
#define MMS_MODE_CONTROL	0x01
#define MMS_XYRES_HI		0x02
#define MMS_XRES_LO		0x03
#define MMS_YRES_LO		0x04

#define MMS_INPUT_EVENT_PKT_SZ	0x0F
#define MMS_INPUT_EVENT0	0x10
#ifdef CONFIG_TOUCHSCREEN_MMS144
#define FINGER_EVENT_SZ	8
#elif defined(CONFIG_TOUCHSCREEN_MMS136)
#define FINGER_EVENT_SZ	6
#endif
#define MMS_CORE_VERSION	0xE1
#define MMS_TSP_REVISION	0xF0
#define MMS_HW_REVISION		0xF1
#define MMS_COMPAT_GROUP	0xF2
#define MMS_FW_VERSION		0xE3 /*core 5.3: F3->E3*/

enum {
	ISP_MODE_FLASH_ERASE	= 0x59F3,
	ISP_MODE_FLASH_WRITE	= 0x62CD,
	ISP_MODE_FLASH_READ	= 0x6AC9,
};

/* each address addresses 4-byte words */
#define ISP_MAX_FW_SIZE		(0x1F00 * 4)
#define ISP_IC_INFO_ADDR	0x1F00
#define ISP_CAL_DATA_SIZE	256

#ifdef CONFIG_SEC_DVFS
#define TOUCH_BOOSTER		1
#define TOUCH_BOOSTER_OFF_TIME	100
#if TOUCH_BOOSTER
#include <linux/cpufreq.h>
#endif
#endif

#ifdef SEC_TSP_FW_UPDATE

#define WORD_SIZE		4

#define ISC_PKT_SIZE			1029
#define ISC_PKT_DATA_SIZE		1024
#define ISC_PKT_HEADER_SIZE		3
#define ISC_PKT_NUM			31

#define ISC_ENTER_ISC_CMD		0x5F
#define ISC_ENTER_ISC_DATA		0x01
#define ISC_CMD				0xAE
#define ISC_ENTER_UPDATE_DATA		0x55
#define ISC_ENTER_UPDATE_DATA_LEN	9
#define ISC_DATA_WRITE_SUB_CMD		0xF1
#define ISC_EXIT_ISC_SUB_CMD		0x0F
#define ISC_EXIT_ISC_SUB_CMD2		0xF0
#define ISC_CHECK_STATUS_CMD		0xAF
#define ISC_CONFIRM_CRC			0x03
#define ISC_DEFAULT_CRC			0xFFFF

#endif

#ifdef SEC_TSP_FACTORY_TEST
#ifdef CONFIG_TOUCHSCREEN_MMS144
#define TX_NUM		25
#define RX_NUM		14
#define NODE_NUM	350 /* 26x14 */
#elif defined(CONFIG_TOUCHSCREEN_MMS136)
#define TX_NUM      20
#define RX_NUM		12
#define NODE_NUM	240 /* 20x12 */
#endif

/* self diagnostic */
#define ADDR_CH_NUM		0x0B
#define ADDR_UNIV_CMD		0xA0
#define CMD_ENTER_TEST		0x40
#define CMD_EXIT_TEST		0x4F
#define CMD_CM_DELTA		0x41
#define CMD_GET_DELTA		0x42
#define CMD_CM_ABS		0X43
#define CMD_GET_ABS		0X44
#define CMD_CM_JITTER		0X45
#define CMD_GET_JITTER		0X46
#define CMD_GET_KEY_DELTA	0x4A
#define CMD_GET_KEY_ABS		0x4B
#define CMD_CM_KEY_JITTER	0x4C

#define CMD_GET_INTEN		0x70
#define CMD_GET_INTEN_KEY	0x71
#define CMD_GET_REFER		0x72
#define CMD_GET_REFER_KEY	0x73
#define CMD_RESULT_SZ		0XAE
#define CMD_RESULT		0XAF

/* VSC(Vender Specific Command)  */
#define MMS_VSC_CMD			0xB0	/* vendor specific command */
#define MMS_VSC_MODE			0x1A	/* mode of vendor */

#define MMS_VSC_CMD_ENTER		0X01
#define MMS_VSC_CMD_CM_DELTA		0X02
#define MMS_VSC_CMD_CM_ABS		0X03
#define MMS_VSC_CMD_EXIT		0X05
#define MMS_VSC_CMD_INTENSITY		0X04
#define MMS_VSC_CMD_RAW			0X06
#define MMS_VSC_CMD_REFER		0X07

#ifdef SEC_TKEY_FACTORY_TEST
#define VSC_INTENSITY_TK		0x14
#define VSC_RAW_TK			0x16
#define VSC_THRESHOLD_TK		0x18
#endif

#define TSP_CMD_STR_LEN 32
#define TSP_CMD_RESULT_STR_LEN 512
#define TSP_CMD_PARAM_NUM 8
#endif /* SEC_TSP_FACTORY_TEST */

#define ISC_DL_MODE 0

#if ISC_DL_MODE	/* ISC_DL_MODE start */

/*
 *      Default configuration of ISC mode
 */
#define DEFAULT_SLAVE_ADDR	0x48

#define SECTION_NUM		3
#define SECTION_NAME_LEN	5

#define PAGE_HEADER		3
#define PAGE_DATA		1024
#define PAGE_TAIL		2
#define PACKET_SIZE		(PAGE_HEADER + PAGE_DATA + PAGE_TAIL)
#define TS_WRITE_REGS_LEN		1030

#define TIMEOUT_CNT		10
#define STRING_BUF_LEN		100


/* State Registers */
#define MIP_ADDR_INPUT_INFORMATION	0x01

#define ISC_ADDR_VERSION		0xE1
#define ISC_ADDR_SECTION_PAGE_INFO	0xE5

/* Config Update Commands */
#define ISC_CMD_ENTER_ISC		0x5F
#define ISC_CMD_ENTER_ISC_PARA1		0x01
#define ISC_CMD_UPDATE_MODE		0xAE
#define ISC_SUBCMD_ENTER_UPDATE		0x55
#define ISC_SUBCMD_DATA_WRITE		0XF1
#define ISC_SUBCMD_LEAVE_UPDATE_PARA1	0x0F
#define ISC_SUBCMD_LEAVE_UPDATE_PARA2	0xF0
#define ISC_CMD_CONFIRM_STATUS		0xAF

#define ISC_STATUS_UPDATE_MODE		0x01
#define ISC_STATUS_CRC_CHECK_SUCCESS	0x03

#define ISC_CHAR_2_BCD(num)	(((num/10)<<4) + (num%10))
#define ISC_MAX(x, y)		(((x) > (y)) ? (x) : (y))

static const char section_name[SECTION_NUM][SECTION_NAME_LEN] = {
	"BOOT", "CORE", "CONF"
};

static const unsigned char crc0_buf[31] = {
	0x1D, 0x2C, 0x05, 0x34, 0x95, 0xA4, 0x8D, 0xBC,
	0x59, 0x68, 0x41, 0x70, 0xD1, 0xE0, 0xC9, 0xF8,
	0x3F, 0x0E, 0x27, 0x16, 0xB7, 0x86, 0xAF, 0x9E,
	0x7B, 0x4A, 0x63, 0x52, 0xF3, 0xC2, 0xEB
};

static const unsigned char crc1_buf[31] = {
	0x1E, 0x9C, 0xDF, 0x5D, 0x76, 0xF4, 0xB7, 0x35,
	0x2A, 0xA8, 0xEB, 0x69, 0x42, 0xC0, 0x83, 0x01,
	0x04, 0x86, 0xC5, 0x47, 0x6C, 0xEE, 0xAD, 0x2F,
	0x30, 0xB2, 0xF1, 0x73, 0x58, 0xDA, 0x99
};

enum {
	ISC_NONE = -1,
	ISC_SUCCESS = 0,
	ISC_FILE_OPEN_ERROR,
	ISC_FILE_CLOSE_ERROR,
	ISC_FILE_FORMAT_ERROR,
	ISC_WRITE_BUFFER_ERROR,
	ISC_I2C_ERROR,
	ISC_UPDATE_MODE_ENTER_ERROR,
	ISC_CRC_ERROR,
	ISC_VALIDATION_ERROR,
	ISC_COMPATIVILITY_ERROR,
	ISC_UPDATE_SECTION_ERROR,
	ISC_SLAVE_ERASE_ERROR,
	ISC_SLAVE_DOWNLOAD_ERROR,
	ISC_DOWNLOAD_WHEN_SLAVE_IS_UPDATED_ERROR,
	ISC_INITIAL_PACKET_ERROR,
	ISC_NO_NEED_UPDATE_ERROR,
	ISC_LIMIT
};

enum {
	EC_NONE = -1,
	EC_DEPRECATED = 0,
	EC_BOOTLOADER_RUNNING = 1,
	EC_BOOT_ON_SUCCEEDED = 2,
	EC_ERASE_END_MARKER_ON_SLAVE_FINISHED = 3,
	EC_SLAVE_DOWNLOAD_STARTS = 4,
	EC_SLAVE_DOWNLOAD_FINISHED = 5,
	EC_2CHIP_HANDSHAKE_FAILED = 0x0E,
	EC_ESD_PATTERN_CHECKED = 0x0F,
	EC_LIMIT
};

struct tISCFWInfo_t {
	unsigned char version;
	unsigned char compatible_version;
	unsigned char start_addr;
	unsigned char end_addr;
};

static struct tISCFWInfo_t mbin_info[SECTION_NUM];
static struct tISCFWInfo_t ts_info[SECTION_NUM];
static bool section_update_flag[SECTION_NUM];

const struct firmware *fw_mbin[SECTION_NUM];

static unsigned char g_wr_buf[1024 + 3 + 2];
#endif

enum {
	SEC_NONE = -1,
	SEC_BOOTLOADER = 0,
	SEC_CORE,
	SEC_CONFIG,
	SEC_LIMIT
};

int touch_is_pressed;
EXPORT_SYMBOL(touch_is_pressed);

#define NUM_OF_KEY	4

enum fw_flash_mode {
	ISP_FLASH,
	ISC_FLASH,
};

enum {
	BUILT_IN = 0,
	UMS,
};

struct mms_ts_info {
	struct i2c_client		*client;
	struct input_dev		*input_dev;
	char				phys[32];

	int				max_x;
	int				max_y;

	bool				invert_x;
	bool				invert_y;
	int				irq;

	struct mms_ts_platform_data	*pdata;

	char				*fw_name;
	struct early_suspend		early_suspend;
#ifdef CONFIG_SEC_DVFS
#if TOUCH_BOOSTER
	struct delayed_work work_dvfs_off;
	bool	dvfs_lock_status;
	struct mutex dvfs_lock;
#endif
#endif

	/* protects the enabled flag */
	struct mutex			lock;
	bool				enabled;

#ifdef SEC_TKEY_FACTORY_TEST
	struct device			*dev_tk;
	bool				*key_pressed;
#endif
	enum fw_flash_mode		fw_flash_mode;

	void (*register_cb)(struct tsp_callbacks *);
	struct tsp_callbacks callbacks;

	bool			ta_status;
	bool			noise_mode;
	bool	use_surface_touch;
	bool	use_touchkey;
	unsigned char   keycode[NUM_OF_KEY];

#if defined(SEC_TSP_DEBUG) || defined(SEC_TSP_VERBOSE_DEBUG)
	unsigned char finger_state[MAX_FINGERS];
#endif

#if defined(SEC_TSP_FW_UPDATE)
	u8			fw_update_state;
#endif
	u8			fw_boot_ver;
	u8			fw_core_ver;
	u8			fw_ic_ver;
	u8			palm_status;
#if defined(SEC_TSP_FACTORY_TEST)
	struct list_head			cmd_list_head;
	u8			cmd_state;
	char			cmd[TSP_CMD_STR_LEN];
	int			cmd_param[TSP_CMD_PARAM_NUM];
	char			cmd_result[TSP_CMD_RESULT_STR_LEN];
	struct mutex			cmd_lock;
	bool			cmd_is_running;

	unsigned int reference[NODE_NUM];
	unsigned int raw[NODE_NUM]; /* CM_ABS */
	unsigned int inspection[NODE_NUM];/* CM_DELTA */
	unsigned int intensity[NODE_NUM];
	bool ft_flag;
#endif /* SEC_TSP_FACTORY_TEST */
#ifdef CONFIG_LEDS_CLASS
	struct led_classdev	leds;
	bool	tkey_led_reserved;
#endif
};

struct mms_fw_image {
	__le32 hdr_len;
	__le32 data_len;
	__le32 fw_ver;
	__le32 hdr_ver;
	u8 data[0];
} __packed;

#ifdef CONFIG_HAS_EARLYSUSPEND
static void mms_ts_early_suspend(struct early_suspend *h);
static void mms_ts_late_resume(struct early_suspend *h);
#endif

#if defined(SEC_TSP_FACTORY_TEST)
#define TSP_CMD(name, func) .cmd_name = name, .cmd_func = func

struct tsp_cmd {
	struct list_head	list;
	const char	*cmd_name;
	void	(*cmd_func)(void *device_data);
};

static void fw_update(void *device_data);
static void get_fw_ver_bin(void *device_data);
static void get_fw_ver_ic(void *device_data);
static void get_config_ver(void *device_data);
static void get_threshold(void *device_data);
static void module_off_master(void *device_data);
static void module_on_master(void *device_data);
static void get_chip_vendor(void *device_data);
static void get_chip_name(void *device_data);
static void get_reference(void *device_data);
static void get_cm_abs(void *device_data);
static void get_cm_delta(void *device_data);
static void get_intensity(void *device_data);
static void get_x_num(void *device_data);
static void get_y_num(void *device_data);
static void run_reference_read(void *device_data);
static void run_cm_abs_read(void *device_data);
static void run_cm_delta_read(void *device_data);
static void run_intensity_read(void *device_data);
static void not_support_cmd(void *device_data);

struct tsp_cmd tsp_cmds[] = {
	{TSP_CMD("fw_update", fw_update),},
	{TSP_CMD("get_fw_ver_bin", get_fw_ver_bin),},
	{TSP_CMD("get_fw_ver_ic", get_fw_ver_ic),},
	{TSP_CMD("get_config_ver", get_config_ver),},
	{TSP_CMD("get_threshold", get_threshold),},
	{TSP_CMD("module_off_master", module_off_master),},
	{TSP_CMD("module_on_master", module_on_master),},
	{TSP_CMD("module_off_slave", not_support_cmd),},
	{TSP_CMD("module_on_slave", not_support_cmd),},
	{TSP_CMD("get_chip_vendor", get_chip_vendor),},
	{TSP_CMD("get_chip_name", get_chip_name),},
	{TSP_CMD("get_x_num", get_x_num),},
	{TSP_CMD("get_y_num", get_y_num),},
	{TSP_CMD("get_reference", get_reference),},
	{TSP_CMD("get_cm_abs", get_cm_abs),},
	{TSP_CMD("get_cm_delta", get_cm_delta),},
	{TSP_CMD("get_intensity", get_intensity),},
	{TSP_CMD("run_reference_read", run_reference_read),},
	{TSP_CMD("run_cm_abs_read", run_cm_abs_read),},
	{TSP_CMD("run_cm_delta_read", run_cm_delta_read),},
	{TSP_CMD("run_intensity_read", run_intensity_read),},
	{TSP_CMD("not_support_cmd", not_support_cmd),},
};
#endif

#ifdef CONFIG_LEDS_CLASS
static void msm_tkey_led_set(struct led_classdev *led_cdev,
	enum led_brightness value)
{
	bool tkey_led_on;

	struct mms_ts_info *info =
		container_of(led_cdev, struct mms_ts_info, leds);

	if (value)
		tkey_led_on = true;
	else
		tkey_led_on = false;

	if (info->enabled) {
		info->tkey_led_reserved = false;
		info->pdata->tkey_led_vdd_on(tkey_led_on);
	} else {
		if (value != LED_OFF)
			info->tkey_led_reserved = true;
	}
}
#endif

#ifdef CONFIG_SEC_DVFS
#if TOUCH_BOOSTER
static void set_dvfs_off(struct work_struct *work)
{
	struct mms_ts_info *info = container_of(work,
				struct mms_ts_info, work_dvfs_off.work);
	mutex_lock(&info->dvfs_lock);
	set_freq_limit(DVFS_TOUCH_ID, -1);
	info->dvfs_lock_status = false;
	mutex_unlock(&info->dvfs_lock);
}

static void set_dvfs_lock(struct mms_ts_info *info, uint32_t on)
{
	int ret = 0;

	mutex_lock(&info->dvfs_lock);

	if (on == 0) {
		if (info->dvfs_lock_status) {
			schedule_delayed_work(&info->work_dvfs_off,
				msecs_to_jiffies(TOUCH_BOOSTER_OFF_TIME));
		}
	} else if (on == 1) {
		cancel_delayed_work(&info->work_dvfs_off);
		if (!info->dvfs_lock_status) {
			ret = set_freq_limit(DVFS_TOUCH_ID, MIN_TOUCH_LIMIT);
			if (ret < 0)
				dev_err(&info->client->dev, "%s: cpu lock failed(%d)\n",
					__func__, ret);
			info->dvfs_lock_status = true;
		}
	} else if (on == 2) {
		cancel_delayed_work(&info->work_dvfs_off);
		schedule_work(&info->work_dvfs_off.work);
	}

	mutex_unlock(&info->dvfs_lock);
}
#endif
#endif

static inline void mms_pwr_on_reset(struct mms_ts_info *info)
{
	struct i2c_adapter *adapter = to_i2c_adapter(info->client->dev.parent);

	if (!info->pdata->mux_fw_flash) {
		dev_err(&info->client->dev, "missing platform data, can't do power-on-reset\n");
		return;
	}

	i2c_lock_adapter(adapter);
	info->pdata->mux_fw_flash(true);

	info->pdata->vdd_on(0);
	gpio_direction_output(info->pdata->gpio_sda, 1);
	gpio_direction_output(info->pdata->gpio_scl, 1);
	gpio_direction_output(info->pdata->gpio_resetb, 1);
	msleep(50);
	info->pdata->vdd_on(1);
	msleep(50);

	info->pdata->mux_fw_flash(false);
	i2c_unlock_adapter(adapter);

	/* TODO: Seems long enough for the firmware to boot.
	 * Find the right value */
	msleep(250);
}
static void release_all_fingers(struct mms_ts_info *info)
{
	struct i2c_client *client = info->client;

	int i;

	dev_dbg(&info->client->dev, "%s\n", __func__);

	for (i = 0; i < MAX_FINGERS; i++) {
#if defined(SEC_TSP_DEBUG) || defined(SEC_TSP_VERBOSE_DEBUG)
		if (info->finger_state[i] == 1)
			dev_notice(&client->dev, "finger %d up(force)\n", i);
#endif
		info->finger_state[i] = 0;
		input_mt_slot(info->input_dev, i);
		input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER,
					   false);
	}
	input_sync(info->input_dev);
#ifdef CONFIG_SEC_DVFS
#if TOUCH_BOOSTER
	set_dvfs_lock(info, 2);
	dev_info(&client->dev, "dvfs_lock free.\n");
#endif
#endif
}

static void mms_set_noise_mode(struct mms_ts_info *info)
{
	struct i2c_client *client = info->client;

	if (!(info->noise_mode && info->enabled))
		return;
	dev_notice(&client->dev, "%s\n", __func__);

	if (info->ta_status) {
		dev_notice(&client->dev, "noise_mode & TA connect!!!\n");
		i2c_smbus_write_byte_data(info->client, 0x30, 0x1);
	} else {
		dev_notice(&client->dev, "noise_mode & TA disconnect!!!\n");
		i2c_smbus_write_byte_data(info->client, 0x30, 0x2);
		info->noise_mode = 0;
	}
}

static void reset_mms_ts(struct mms_ts_info *info)
{
	struct i2c_client *client = info->client;

	if (info->enabled == false)
		return;

	dev_notice(&client->dev, "%s++\n", __func__);
	disable_irq_nosync(info->irq);
	info->enabled = false;
	touch_is_pressed = 0;

	release_all_fingers(info);

	mms_pwr_on_reset(info);
	enable_irq(info->irq);
	info->enabled = true;

	if (info->ta_status) {
		dev_notice(&client->dev, "TA connect!!!\n");
		i2c_smbus_write_byte_data(info->client, 0x33, 0x1);
	} else {
		dev_notice(&client->dev, "TA disconnect!!!\n");
		i2c_smbus_write_byte_data(info->client, 0x33, 0x2);
		mms_set_noise_mode(info);
	}

	dev_notice(&client->dev, "%s--\n", __func__);
}

static void melfas_ta_cb(struct tsp_callbacks *cb, bool ta_status)
{
	struct mms_ts_info *info =
			container_of(cb, struct mms_ts_info, callbacks);
	struct i2c_client *client = info->client;

	dev_notice(&client->dev, "%s\n", __func__);

	info->ta_status = ta_status;

	if (info->enabled) {
		if (info->ta_status) {
			dev_notice(&client->dev, "TA connect!!!\n");
			i2c_smbus_write_byte_data(info->client, 0x33, 0x1);
		} else {
			dev_notice(&client->dev, "TA disconnect!!!\n");
			i2c_smbus_write_byte_data(info->client, 0x33, 0x2);
			mms_set_noise_mode(info);
		}
	}
}

static irqreturn_t mms_ts_interrupt(int irq, void *dev_id)
{
	struct mms_ts_info *info = dev_id;
	struct i2c_client *client = info->client;
	u8 buf[MAX_FINGERS*FINGER_EVENT_SZ] = { 0 };
	int ret;
	int i;
	int sz;
	u8 reg = MMS_INPUT_EVENT0;
	struct i2c_msg msg[] = {
		{
			.addr   = client->addr,
			.flags  = 0,
			.buf    = &reg,
			.len    = 1,
		}, {
			.addr   = client->addr,
			.flags  = I2C_M_RD,
			.buf    = buf,
		},
	};

	sz = i2c_smbus_read_byte_data(client, MMS_INPUT_EVENT_PKT_SZ);
	if (sz < 0) {
		pr_err("%s bytes=%d\n", __func__, sz);
		for (i = 0; i < 50; i++) {
			sz = i2c_smbus_read_byte_data(client,
						MMS_INPUT_EVENT_PKT_SZ);
			if (sz > 0)
				break;
		}

		if (i == 50) {
			pr_err("i2c failed... reset!!\n");
			reset_mms_ts(info);
			goto out;
		}
	}
	/* BUG_ON(sz > MAX_FINGERS*FINGER_EVENT_SZ); */
	if (sz == 0)
		goto out;

	if (sz > MAX_FINGERS*FINGER_EVENT_SZ) {
		pr_err("abnormal data inputed.\n");
		goto out;
	}

	msg[1].len = sz;
	ret = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));
	if (ret != ARRAY_SIZE(msg)) {
		pr_err("failed to read %d bytes of touch data (%d)\n",
			sz, ret);
		goto out;
	}

#if defined(VERBOSE_DEBUG)
	print_hex_dump(KERN_DEBUG, "mms_ts raw: ",
		DUMP_PREFIX_OFFSET, 32, 1, buf, sz, false);

#endif
	if (buf[0] == 0x0F) { /* ESD */
		pr_debug("ESD DETECT.... reset!!\n");
		reset_mms_ts(info);
		goto out;
	}

	if (buf[0] == 0x0E) { /* NOISE MODE */
		pr_debug("noise mode enter!!\n");
		info->noise_mode = 1 ;
		mms_set_noise_mode(info);
		goto out;
	}

	for (i = 0; i < sz; i += FINGER_EVENT_SZ) {
		u8 *tmp = &buf[i];
		int id = (tmp[0] & 0xf) - 1;
		int x = tmp[2] | ((tmp[1] & 0xf) << 8);
		int y = tmp[3] | (((tmp[1] >> 4) & 0xf) << 8);
		int angle = (tmp[5] >= 127) ? (-(256 - tmp[5])) : tmp[5];
		int palm = (buf[0] & 0x10) >> 4;
		int type = (tmp[0] & 0x60) >> 5;
		int action = (tmp[0] & 0x80) >> 7;

		if (info->use_surface_touch) {
			if (palm) {
				if (info->palm_status == 3) {
					info->palm_status = 1;
				} else {
					info->palm_status = 3;
					palm = 3;
				}
			} else {
				if (info->palm_status == 2) {
					info->palm_status = 0;
				} else {
					info->palm_status = 2;
					palm = 2;
				}
			}
		}

		if (info->use_touchkey && type == 0x02) {
			input_report_key(info->input_dev,
				info->keycode[id], action);
#ifdef SEC_TSP_DEBUG
			pr_info("touchkey : keycode=%d action=%d\n",
					info->keycode[id], action);
#else
			pr_info("touchkey action=%d\n", action);
#endif
#ifdef SEC_TKEY_FACTORY_TEST
			info->key_pressed[id] = action;
#endif
		} else {

			if (info->invert_x) {
				x = info->max_x - x;
				if (x < 0)
					x = 0;
			}
			if (info->invert_y) {
				y = info->max_y - y;
				if (y < 0)
					y = 0;
			}
			if (id >= MAX_FINGERS) {
				pr_err("finger id error [%d]\n", id);
				reset_mms_ts(info);
				goto out;
			}

			if ((tmp[0] & 0x80) == 0) {
#if defined(SEC_TSP_DEBUG)
				pr_info("fid[%d]: x=%d y=%d p=%d w=%d ",
					id, x, y, tmp[5], tmp[4]);
				if (info->use_surface_touch)
					pr_cont("major=%d minor=%d angle=%d palm=%d\n",
						tmp[6], tmp[7], angle, palm);
				else
					pr_cont("\n");
#else
				pr_info("finger [%d] up\n", id);
#endif
				input_mt_slot(info->input_dev, id);
				input_mt_report_slot_state(info->input_dev,
					MT_TOOL_FINGER, false);

#if defined(SEC_TSP_DEBUG) || defined(SEC_TSP_VERBOSE_DEBUG)
				info->finger_state[id] = 0;
#endif
				continue;
			}

			//input_report_key(info->input_dev, BTN_TOUCH, 1);

			input_mt_slot(info->input_dev, id);
			input_mt_report_slot_state(info->input_dev,
					MT_TOOL_FINGER, true);
			#if 1
			input_report_abs(info->input_dev,
					ABS_MT_WIDTH_MAJOR, tmp[4]);
			#endif
			input_report_abs(info->input_dev,
					ABS_MT_POSITION_X, x);
			input_report_abs(info->input_dev,
					ABS_MT_POSITION_Y, y);
			#if 1

			if (info->use_surface_touch) {
				input_report_abs(info->input_dev,
						ABS_MT_TOUCH_MAJOR, tmp[6]);
				input_report_abs(info->input_dev,
						ABS_MT_TOUCH_MINOR, tmp[7]);

#if 0 /* SPRD's test code */
				input_report_abs(info->input_dev,
						ABS_MT_ANGLE, angle);
				input_report_abs(info->input_dev,
						ABS_MT_PALM, palm);
#endif
			}
			#endif
			//input_mt_sync(info->input_dev);

#if defined(SEC_TSP_DEBUG)
			if (info->finger_state[id] == 0) {
				info->finger_state[id] = 1;
				pr_info("finger id[%d]: x=%d y=%d p=%d w=%d ",
					id, x, y, tmp[5], tmp[4]);

				if (info->use_surface_touch)
					pr_cont("major=%d minor=%d angle=%d palm=%d\n",
					tmp[6], tmp[7], angle, palm);
				else
					pr_cont("\n");
			}
#else
			if (info->finger_state[id] == 0) {
				info->finger_state[id] = 1;
				pr_info("finger [%d] down\n", id);
			}
#endif
		}
	}

	input_sync(info->input_dev);
	touch_is_pressed = 0;

	for (i = 0; i < MAX_FINGERS; i++) {
		if (info->finger_state[i] == 1)
			touch_is_pressed++;
	}

#ifdef CONFIG_SEC_DVFS
#if TOUCH_BOOSTER
	set_dvfs_lock(info, !!touch_is_pressed);
#endif
#endif

out:
	return IRQ_HANDLED;
}

#if ISC_DL_MODE
static int mms100_i2c_read(struct i2c_client *client,
		u16 addr, u16 length, u8 *value)
{
	struct i2c_adapter *adapter = client->adapter;
	struct i2c_msg msg;
	int ret = -1;

	msg.addr = client->addr;
	msg.flags = 0x00;
	msg.len = 1;
	msg.buf = (u8 *) &addr;

	ret = i2c_transfer(adapter, &msg, 1);

	if (ret >= 0) {
		msg.addr = client->addr;
		msg.flags = I2C_M_RD;
		msg.len = length;
		msg.buf = (u8 *) value;

		ret = i2c_transfer(adapter, &msg, 1);
	}

	if (ret < 0)
		dev_err(&client->dev, "I2C read error : [%d]\n", ret);

	return ret;
}


static int mms100_reset(struct mms_ts_info *info)
{
	info->pdata->vdd_on(0);
	msleep(30);
	info->pdata->vdd_on(1);
	msleep(300);

	return ISC_SUCCESS;
}
/*
static int mms100_check_operating_mode(struct i2c_client *_client,
		const int _error_code)
{
	int ret;
	unsigned char rd_buf = 0x00;
	unsigned char count = 0;

	if(_client == NULL)
		pr_err("[TSP ISC] _client is null\n");

	ret = mms100_i2c_read(_client, ISC_ADDR_VERSION, 1, &rd_buf);

	if (ret<0) {
		pr_info("[TSP ISC] %s,%d: i2c read fail[%d]\n",
			__func__, __LINE__, ret);
		return _error_code;
	}

	return ISC_SUCCESS;
}
*/
static int mms100_get_version_info(struct i2c_client *_client)
{
	int i, ret;
	unsigned char rd_buf[8];

	/* config version brust read (core, private, public) */
	ret = mms100_i2c_read(_client, ISC_ADDR_VERSION, 3, rd_buf);
	if (ret < 0) {
		dev_err(&_client->dev, "[TSP ISC] %s,%d: i2c read fail[%d]\n",
			__func__, __LINE__, ret);
		return ISC_I2C_ERROR;
	}

	for (i = 0; i < SECTION_NUM; i++)
		ts_info[i].version = rd_buf[i];

	ts_info[SEC_CORE].compatible_version =
		ts_info[SEC_BOOTLOADER].version;
	ts_info[SEC_CONFIG].compatible_version =
		ts_info[SEC_CORE].version;

	ret = mms100_i2c_read(_client, ISC_ADDR_SECTION_PAGE_INFO, 8, rd_buf);
	if (ret < 0) {
		dev_err(&_client->dev, "[TSP ISC] %s,%d: i2c read fail[%d]\n",
			__func__, __LINE__, ret);
		return ISC_I2C_ERROR;
	}

	for (i = 0; i < SECTION_NUM; i++) {
		ts_info[i].start_addr = rd_buf[i];
		ts_info[i].end_addr = rd_buf[i + SECTION_NUM + 1];
	}

	for (i = 0; i < SECTION_NUM; i++) {
		dev_info(&_client->dev, "TS : Section(%d) version: 0x%02X\n",
			i, ts_info[i].version);
		dev_info(&_client->dev, "TS : Section(%d) Start Address: 0x%02X\n",
			i, ts_info[i].start_addr);
		dev_info(&_client->dev, "TS : Section(%d) End Address: 0x%02X\n",
			i, ts_info[i].end_addr);
		dev_info(&_client->dev, "TS : Section(%d) Compatibility: 0x%02X\n",
			i, ts_info[i].compatible_version);
	}

	return ISC_SUCCESS;
}

static int mms100_seek_section_info(struct i2c_client *_client)
{
	int i;
	char str_buf[STRING_BUF_LEN];
	char name_buf[SECTION_NAME_LEN];
	int version;
	int page_num;

	const unsigned char *buf;
	int next_ptr;

	for (i = 0; i < SECTION_NUM; i++) {
		if (fw_mbin[i] == NULL) {
			buf = NULL;
			dev_err(&_client->dev,
				"[TSP ISC] fw_mbin[%d]->data is NULL\n", i);
		} else {
			buf = fw_mbin[i]->data;
		}

		if (buf == NULL) {
			mbin_info[i].version = ts_info[i].version;
			mbin_info[i].compatible_version =
				ts_info[i].compatible_version;
			mbin_info[i].start_addr = ts_info[i].start_addr;
			mbin_info[i].end_addr = ts_info[i].end_addr;
		} else {
			next_ptr = 0;

			do {
				sscanf(buf + next_ptr, "%s", str_buf);
				next_ptr += strlen(str_buf) + 1;
			} while (!strstr(str_buf, "SECTION_NAME"));

			sscanf(buf + next_ptr, "%s%s", str_buf, name_buf);

			if (strncmp(section_name[i], name_buf,
				SECTION_NAME_LEN))
				return ISC_FILE_FORMAT_ERROR;

			do {
				sscanf(buf + next_ptr, "%s", str_buf);
				next_ptr += strlen(str_buf) + 1;
			} while (!strstr(str_buf, "SECTION_VERSION"));

			sscanf(buf + next_ptr, "%s%d", str_buf, &version);
			mbin_info[i].version = ISC_CHAR_2_BCD(version);

			do {
				sscanf(buf + next_ptr, "%s", str_buf);
				next_ptr += strlen(str_buf) + 1;
			} while (!strstr(str_buf, "START_PAGE_ADDR"));

			sscanf(buf + next_ptr, "%s%d", str_buf, &page_num);
			mbin_info[i].start_addr = page_num;

			do {
				sscanf(buf + next_ptr, "%s", str_buf);
				next_ptr += strlen(str_buf) + 1;
			} while (!strstr(str_buf, "END_PAGE_ADDR"));

			sscanf(buf + next_ptr, "%s%d", str_buf, &page_num);
			mbin_info[i].end_addr = page_num;

			do {
				sscanf(buf + next_ptr, "%s", str_buf);
				next_ptr += strlen(str_buf) + 1;
			} while (!strstr(str_buf, "COMPATIBLE_VERSION"));

			sscanf(buf + next_ptr, "%s%d", str_buf, &version);
			mbin_info[i].compatible_version =
				ISC_CHAR_2_BCD(version);

			do {
				sscanf(buf + next_ptr, "%s", str_buf);
				next_ptr += strlen(str_buf) + 1;
			} while (!strstr(str_buf, "[Binary]"));

			if (mbin_info[i].version == 0xFF)
				return ISC_FILE_FORMAT_ERROR;
		}
	}

	for (i = 0; i < SECTION_NUM; i++) {
		dev_info(&_client->dev, "[TSP ISC] MBin : Section(%d) Version: 0x%02X\n",
			i, mbin_info[i].version);
		dev_info(&_client->dev, "[TSP ISC] MBin : Section(%d) Start Address: 0x%02X\n",
			i, mbin_info[i].start_addr);
		dev_info(&_client->dev, "[TSP ISC] MBin : Section(%d) End Address: 0x%02X\n",
			i, mbin_info[i].end_addr);
		dev_info(&_client->dev, "[TSP ISC] MBin : Section(%d) Compatibility: 0x%02X\n",
			i, mbin_info[i].compatible_version);
	}

	return ISC_SUCCESS;
}

static int mms100_compare_version_info(struct i2c_client *_client)
{
	int i, ret;
	unsigned char expected_compatibility[SECTION_NUM];

	if (mms100_get_version_info(_client) != ISC_SUCCESS)
		return ISC_I2C_ERROR;

	ret = mms100_seek_section_info(_client);

	/* Check update areas , 0 : bootloader 1: core 2: private 3: public */
	for (i = 0; i < SECTION_NUM; i++) {
		if ((mbin_info[i].version == 0) ||
			(mbin_info[i].version != ts_info[i].version)) {
			section_update_flag[i] = true;
			dev_info(&_client->dev,
				"[TSP ISC] [%d] section will be updated!\n", i);
		}
	}
	section_update_flag[0] = false;
	section_update_flag[1] = false;
	dev_info(&_client->dev, "[TSP ISC] [%d] [%d] [%d]",
		section_update_flag[0], section_update_flag[1],
		section_update_flag[2]);

	if (section_update_flag[SEC_BOOTLOADER]) {
		expected_compatibility[SEC_CORE] =
		mbin_info[SEC_BOOTLOADER].version;
	} else {
		expected_compatibility[SEC_CORE] =
		ts_info[SEC_BOOTLOADER].version;
	}

	if (section_update_flag[SEC_CORE]) {
		expected_compatibility[SEC_CONFIG] =
		mbin_info[SEC_CORE].version;
	} else {
		expected_compatibility[SEC_CONFIG] =
		ts_info[SEC_CORE].version;
	}

	for (i = SEC_CORE; i < SEC_CONFIG; i++) {
		if (section_update_flag[i]) {
			dev_info(&_client->dev, "[TSP ISC] section_update_flag(%d), 0x%02x, 0x%02x\n",
				i, expected_compatibility[i],
				mbin_info[i].compatible_version);

			if (expected_compatibility[i] !=
				mbin_info[i].compatible_version)
				return ISC_COMPATIVILITY_ERROR;
		} else {
			dev_info(&_client->dev, "[TSP ISC] !section_update_flag(%d), 0x%02x, 0x%02x\n",
				i, expected_compatibility[i],
				ts_info[i].compatible_version);
			if (expected_compatibility[i] !=
				ts_info[i].compatible_version)
				return ISC_COMPATIVILITY_ERROR;
		}
	}
	return ISC_SUCCESS;
}

static int mms100_enter_ISC_mode(struct i2c_client *_client)
{
	int ret;
	unsigned char wr_buf[2];

	dev_info(&_client->dev, "[TSP ISC] %s\n", __func__);

	wr_buf[0] = ISC_CMD_ENTER_ISC;
	wr_buf[1] = ISC_CMD_ENTER_ISC_PARA1;

	ret = i2c_master_send(_client, wr_buf, 2);

	if (ret < 0) {
		dev_err(&_client->dev, "[TSP ISC] %s,%d: i2c write fail[%d]\n",
			__func__, __LINE__, ret);
		return ISC_I2C_ERROR;
	}

	msleep(50);

	return ISC_SUCCESS;
}

static int mms100_enter_config_update(struct i2c_client *_client)
{
	int ret;
	unsigned char wr_buf[10] = {0,};
	unsigned char rd_buf;

	wr_buf[0] = ISC_CMD_UPDATE_MODE;
	wr_buf[1] = ISC_SUBCMD_ENTER_UPDATE;

	ret = i2c_master_send(_client, wr_buf, 10);

	if (ret < 0) {
		dev_err(&_client->dev, "[TSP ISC] %s,%d: i2c write fail[%d]\n",
			__func__, __LINE__, ret);
		return ISC_I2C_ERROR;
	}

	ret = mms100_i2c_read(_client, ISC_CMD_CONFIRM_STATUS, 1, &rd_buf);
	if (ret < 0) {
		dev_err(&_client->dev, "[TSP ISC] %s,%d: i2c read fail[%d]\n",
			__func__, __LINE__, ret);
		return ISC_I2C_ERROR;
	}

	if (rd_buf != ISC_STATUS_UPDATE_MODE)
		return ISC_UPDATE_MODE_ENTER_ERROR;

	dev_info(&_client->dev, "[TSP ISC] End mms100_enter_config_update()\n");

	return ISC_SUCCESS;
}

static int mms100_ISC_clear_page(struct i2c_client *_client,
		unsigned char _page_addr)
{
	int ret;
	unsigned char rd_buf;

	memset(&g_wr_buf[3], 0xFF, PAGE_DATA);

	g_wr_buf[0] = ISC_CMD_UPDATE_MODE;	/* command */
	g_wr_buf[1] = ISC_SUBCMD_DATA_WRITE;	/* sub_command */
	g_wr_buf[2] = _page_addr;

	g_wr_buf[PAGE_HEADER + PAGE_DATA] = crc0_buf[_page_addr];
	g_wr_buf[PAGE_HEADER + PAGE_DATA + 1] = crc1_buf[_page_addr];

	ret = i2c_master_send(_client, g_wr_buf, PACKET_SIZE);

	if (ret < 0) {
		dev_err(&_client->dev, "[TSP ISC] %s,%d: i2c write fail[%d]\n",
			__func__, __LINE__, ret);
		return ISC_I2C_ERROR;
	}

	ret = mms100_i2c_read(_client, ISC_CMD_CONFIRM_STATUS, 1, &rd_buf);

	if (ret < 0) {
		dev_err(&_client->dev, "[TSP ISC] %s,%d: i2c read fail[%d]\n",
			__func__, __LINE__, ret);
		return ISC_I2C_ERROR;
	}

	if (rd_buf != ISC_STATUS_CRC_CHECK_SUCCESS)
		return ISC_UPDATE_MODE_ENTER_ERROR;

	dev_info(&_client->dev, "[TSP ISC] End mms100_ISC_clear_page()\n");
	return ISC_SUCCESS;

}

static int mms100_ISC_clear_validate_markers(struct i2c_client *_client)
{
	int ret_msg;
	int i, j;
	bool is_matched_address;

	for (i = SEC_CORE; i <= SEC_CONFIG; i++) {
		if (section_update_flag[i]) {
			if (ts_info[i].end_addr <= 30 &&
				ts_info[i].end_addr > 0) {
				ret_msg = mms100_ISC_clear_page(_client,
					ts_info[i].end_addr);

				if (ret_msg != ISC_SUCCESS)
					return ret_msg;
			}
		}
	}

	for (i = SEC_CORE; i <= SEC_CONFIG; i++) {
		if (section_update_flag[i]) {
			is_matched_address = false;
			for (j = SEC_CORE; j <= SEC_CONFIG; j++) {
				if (mbin_info[i].end_addr ==
					ts_info[i].end_addr) {
					is_matched_address = true;
					break;
				}
			}

			if (!is_matched_address) {
				if (mbin_info[i].end_addr <= 30 &&
					mbin_info[i].end_addr > 0) {
					ret_msg = mms100_ISC_clear_page(_client,
						mbin_info[i].end_addr);

				if (ret_msg != ISC_SUCCESS)
					return ret_msg;
				}
			}
		}
	}

	return ISC_SUCCESS;
}

static void mms100_calc_crc(unsigned char *crc,
		int page_addr, unsigned char *ptr_fw)
{
	int	i, j;

	unsigned char  ucData;

	unsigned short SeedValue;
	unsigned short CRC_check_buf;
	unsigned short CRC_send_buf;
	unsigned short IN_data;
	unsigned short XOR_bit_1;
	unsigned short XOR_bit_2;
	unsigned short XOR_bit_3;

	CRC_check_buf = 0xFFFF;
	SeedValue = (unsigned short)page_addr;

	for (i = 7; i >= 0; i--) {
		IN_data = (SeedValue >> i) & 0x01;
		XOR_bit_1 = (CRC_check_buf & 0x0001) ^ IN_data;
		XOR_bit_2 = XOR_bit_1^(CRC_check_buf >> 11 & 0x01);
		XOR_bit_3 = XOR_bit_1^(CRC_check_buf >> 4 & 0x01);
		CRC_send_buf = (XOR_bit_1 << 4) | (CRC_check_buf >> 12 & 0x0F);
		CRC_send_buf =
			(CRC_send_buf << 7) | (XOR_bit_2 << 6) |
			(CRC_check_buf >> 5 & 0x3F);
		CRC_send_buf =
			(CRC_send_buf << 4) | (XOR_bit_3 << 3) |
			(CRC_check_buf >> 1 & 0x0007);
		CRC_check_buf = CRC_send_buf;
	}

	for (j = 0; j < 1024; j++) {
		ucData = ptr_fw[j];

		for (i = 7; i >= 0; i--) {
			IN_data = (ucData >> i) & 0x0001;
			XOR_bit_1 = (CRC_check_buf & 0x0001) ^ IN_data;
			XOR_bit_2 = XOR_bit_1^(CRC_check_buf >> 11 & 0x01);
			XOR_bit_3 = XOR_bit_1^(CRC_check_buf >> 4 & 0x01);
			CRC_send_buf =
				(XOR_bit_1 << 4) | (CRC_check_buf >> 12 & 0x0F);
			CRC_send_buf =
				(CRC_send_buf << 7) | (XOR_bit_2 << 6) |
				(CRC_check_buf >> 5 & 0x3F);
			CRC_send_buf =
				(CRC_send_buf << 4) | (XOR_bit_3 << 3) |
				(CRC_check_buf >> 1 & 0x0007);
			CRC_check_buf = CRC_send_buf;
		}
	}

	crc[0] = (unsigned char)((CRC_check_buf >> 8) & 0xFF);
	crc[1] = (unsigned char)((CRC_check_buf	>> 0) & 0xFF);
}

static int mms100_update_section_data(struct i2c_client *_client)
{
	int i, j, ret;
	unsigned char rd_buf;
	unsigned char crc[2];
	const unsigned char *ptr_fw;
	char str_buf[STRING_BUF_LEN];
	int page_addr;

	for (i = 0; i < SECTION_NUM; i++) {
		if (section_update_flag[i]) {
			dev_info(&_client->dev,
				"[TSP ISC] section data i2c flash : [%d]\n", i);

			ptr_fw = fw_mbin[i]->data;

			do {
				sscanf(ptr_fw, "%s", str_buf);
				ptr_fw += strlen(str_buf) + 1;
/*
	pr_info("[TSP ISC] Section[%d] %s", i, str_buf );
*/
			} while (!strstr(str_buf, "[Binary]"));
			ptr_fw += 1;

			for (page_addr = mbin_info[i].start_addr;
				page_addr <= mbin_info[i].end_addr;
				page_addr++) {
				if (page_addr - mbin_info[i].start_addr > 0)
					ptr_fw += 1024;

				g_wr_buf[0] = ISC_CMD_UPDATE_MODE;
				g_wr_buf[1] = ISC_SUBCMD_DATA_WRITE;
				g_wr_buf[2] = (unsigned char)page_addr;

				for (j = 0; j < 1024; j += 4) {
					g_wr_buf[3+j] = ptr_fw[j+3];
					g_wr_buf[3+j+1] = ptr_fw[j+2];
					g_wr_buf[3+j+2] = ptr_fw[j+1];
					g_wr_buf[3+j+3] = ptr_fw[j+0];
				}

				mms100_calc_crc(crc, page_addr, &g_wr_buf[3]);

				g_wr_buf[1027] = crc[0];
				g_wr_buf[1028] = crc[1];
/*
	pr_info("[TSP ISC] [%d] DATA %02X %02X %02X %02X   CRC %02X %02X ",
		page_addr, g_wr_buf[3], g_wr_buf[4],
		g_wr_buf[5], g_wr_buf[6] , crc[0] , crc[1] );
*/
				ret = i2c_master_send(_client,
					g_wr_buf, PACKET_SIZE);
				if (ret < 0) {
					dev_err(&_client->dev, "[TSP ISC] %s,%d: i2c write fail[%d]\n",
						__func__, __LINE__, ret);
					return ISC_I2C_ERROR;
				}

				ret = mms100_i2c_read(_client,
					ISC_CMD_CONFIRM_STATUS, 1, &rd_buf);
				if (ret < 0) {
					dev_err(&_client->dev, "[TSP ISC] %s,%d: i2c read fail[%d]\n",
						__func__, __LINE__, ret);
					return ISC_I2C_ERROR;
				}

				if (rd_buf != ISC_STATUS_CRC_CHECK_SUCCESS)
					return ISC_CRC_ERROR;

				section_update_flag[i] = false;
			}
		}
	}

	return ISC_SUCCESS;
}

static int mms100_open_mbinary(struct mms_ts_info *info)
{
	struct i2c_client *_client = info->client;
	int ret = 0;

	/*to do fw update */
	{
		ret += request_firmware(&(fw_mbin[0]),\
			"tsp_melfas/BOOT_H.fw", &_client->dev);
		ret += request_firmware(&(fw_mbin[1]),\
			"tsp_melfas/CORE_H.fw", &_client->dev);
		ret += request_firmware(&(fw_mbin[2]),\
			"tsp_melfas/CONF_H.fw", &_client->dev);
	}

	if (!ret)
		return ISC_SUCCESS;
	else {
		dev_err(&_client->dev, "[TSP ISC] request_firmware fail\n");
		return ret;
	}
}

static int mms100_close_mbinary(void)
{
	int i;

	for (i = 0; i < SECTION_NUM; i++) {
		if (fw_mbin[i] != NULL)
			release_firmware(fw_mbin[i]);
	}
	return ISC_SUCCESS;
}

int mms100_ISC_download_mbinary(struct mms_ts_info *info)
{
	struct i2c_client *_client = info->client;
	int ret_msg = ISC_NONE;

	dev_info(&_client->dev, "[TSP ISC] %s\n", __func__);

	mms100_reset(info);

/*	ret_msg = mms100_check_operating_mode(_client, EC_BOOT_ON_SUCCEEDED);
	if (ret_msg != ISC_SUCCESS)
		goto ISC_ERROR_HANDLE;
*/
	ret_msg = mms100_open_mbinary(info);
	if (ret_msg != ISC_SUCCESS)
		goto ISC_ERROR_HANDLE;

	/* Config version Check */
	ret_msg = mms100_compare_version_info(_client);
	if (ret_msg != ISC_SUCCESS)
		goto ISC_ERROR_HANDLE;

	ret_msg = mms100_enter_ISC_mode(_client);
	if (ret_msg != ISC_SUCCESS)
		goto ISC_ERROR_HANDLE;

	ret_msg = mms100_enter_config_update(_client);
	if (ret_msg != ISC_SUCCESS)
		goto ISC_ERROR_HANDLE;

	ret_msg = mms100_ISC_clear_validate_markers(_client);
	if (ret_msg != ISC_SUCCESS)
		goto ISC_ERROR_HANDLE;

	dev_info(&_client->dev, "[TSP ISC] mms100_update_section_data start\n");

	ret_msg = mms100_update_section_data(_client);
	if (ret_msg != ISC_SUCCESS)
		goto ISC_ERROR_HANDLE;

	dev_info(&_client->dev, "[TSP ISC] mms100_update_section_data end\n");

/*	mms100_reset(info); */

	dev_info(&_client->dev, "[TSP ISC] FIRMWARE_UPDATE_FINISHED!!!\n");

	ret_msg = ISC_SUCCESS;

ISC_ERROR_HANDLE:
	if (ret_msg != ISC_SUCCESS)
		dev_err(&_client->dev,
			"[TSP ISC] ISC_ERROR_CODE: %d\n", ret_msg);

	mms100_reset(info);
	mms100_close_mbinary();

	return ret_msg;
}
#endif	/* ISC_DL_MODE end */

static void hw_reboot(struct mms_ts_info *info, bool bootloader)
{
	info->pdata->vdd_on(0);
	gpio_direction_output(info->pdata->gpio_sda, bootloader ? 0 : 1);
	gpio_direction_output(info->pdata->gpio_scl, bootloader ? 0 : 1);
	gpio_direction_output(info->pdata->gpio_resetb, 0);
	msleep(30);
	info->pdata->vdd_on(1);
	msleep(30);

	if (bootloader) {
		gpio_set_value(info->pdata->gpio_scl, 0);
		gpio_set_value(info->pdata->gpio_sda, 1);
	} else {
		gpio_set_value(info->pdata->gpio_resetb, 1);
		gpio_direction_input(info->pdata->gpio_resetb);
		gpio_direction_input(info->pdata->gpio_scl);
		gpio_direction_input(info->pdata->gpio_sda);
	}
	msleep(40);
}

static inline void hw_reboot_bootloader(struct mms_ts_info *info)
{
	hw_reboot(info, true);
}

static inline void hw_reboot_normal(struct mms_ts_info *info)
{
	hw_reboot(info, false);
}

static void isp_toggle_clk(struct mms_ts_info *info, int start_lvl, int end_lvl,
			   int hold_us)
{
	gpio_set_value(info->pdata->gpio_scl, start_lvl);
	udelay(hold_us);
	gpio_set_value(info->pdata->gpio_scl, end_lvl);
	udelay(hold_us);
}

/* 1 <= cnt <= 32 bits to write */
static void isp_send_bits(struct mms_ts_info *info, u32 data, int cnt)
{
	gpio_direction_output(info->pdata->gpio_resetb, 0);
	gpio_direction_output(info->pdata->gpio_scl, 0);
	gpio_direction_output(info->pdata->gpio_sda, 0);

	/* clock out the bits, msb first */
	while (cnt--) {
		gpio_set_value(info->pdata->gpio_sda, (data >> cnt) & 1);
		udelay(3);
		isp_toggle_clk(info, 1, 0, 3);
	}
}

/* 1 <= cnt <= 32 bits to read */
static u32 isp_recv_bits(struct mms_ts_info *info, int cnt)
{
	u32 data = 0;

	gpio_direction_output(info->pdata->gpio_resetb, 0);
	gpio_direction_output(info->pdata->gpio_scl, 0);
	gpio_set_value(info->pdata->gpio_sda, 0);
	gpio_direction_input(info->pdata->gpio_sda);

	/* clock in the bits, msb first */
	while (cnt--) {
		isp_toggle_clk(info, 0, 1, 1);
		data = (data << 1) | (!!gpio_get_value(info->pdata->gpio_sda));
	}

	gpio_direction_output(info->pdata->gpio_sda, 0);
	return data;
}

static void isp_enter_mode(struct mms_ts_info *info, u32 mode)
{
	int cnt;
	unsigned long flags;

	local_irq_save(flags);
	gpio_direction_output(info->pdata->gpio_resetb, 0);
	gpio_direction_output(info->pdata->gpio_scl, 0);
	gpio_direction_output(info->pdata->gpio_sda, 1);

	mode &= 0xffff;
	for (cnt = 15; cnt >= 0; cnt--) {
		gpio_set_value(info->pdata->gpio_resetb, (mode >> cnt) & 1);
		udelay(3);
		isp_toggle_clk(info, 1, 0, 3);
	}

	gpio_set_value(info->pdata->gpio_resetb, 0);
	local_irq_restore(flags);
}

static void isp_exit_mode(struct mms_ts_info *info)
{
	int i;
	unsigned long flags;

	local_irq_save(flags);
	gpio_direction_output(info->pdata->gpio_resetb, 0);
	udelay(3);

	for (i = 0; i < 10; i++)
		isp_toggle_clk(info, 1, 0, 3);
	local_irq_restore(flags);
}

static void isp_flash_set_address(struct mms_ts_info *info, u16 addr)
{
	/* Only 13 bits of addr are valid.
	 * The addr is in bits 13:1 of cmd */
	isp_send_bits(info, (u32)(addr & 0x1fff) << 1, 18);
}

static void isp_flash_erase(struct mms_ts_info *info)
{
	isp_enter_mode(info, ISP_MODE_FLASH_ERASE);

	gpio_direction_output(info->pdata->gpio_resetb, 0);
	gpio_direction_output(info->pdata->gpio_scl, 0);
	gpio_direction_output(info->pdata->gpio_sda, 1);

	/* 4 clock cycles with different timings for the erase to
	 * get processed, clk is already 0 from above */
	udelay(7);
	isp_toggle_clk(info, 1, 0, 3);
	udelay(7);
	isp_toggle_clk(info, 1, 0, 3);
	usleep_range(25000, 35000);
	isp_toggle_clk(info, 1, 0, 3);
	usleep_range(150, 200);
	isp_toggle_clk(info, 1, 0, 3);

	gpio_set_value(info->pdata->gpio_sda, 0);

	isp_exit_mode(info);
}

static u32 isp_flash_readl(struct mms_ts_info *info, u16 addr)
{
	int i;
	u32 val;
	unsigned long flags;

	local_irq_save(flags);
	isp_enter_mode(info, ISP_MODE_FLASH_READ);
	isp_flash_set_address(info, addr);

	gpio_direction_output(info->pdata->gpio_scl, 0);
	gpio_direction_output(info->pdata->gpio_sda, 0);
	udelay(40);

	/* data load cycle */
	for (i = 0; i < 6; i++)
		isp_toggle_clk(info, 1, 0, 10);

	val = isp_recv_bits(info, 32);
	isp_exit_mode(info);
	local_irq_restore(flags);

	return val;
}

static void isp_flash_writel(struct mms_ts_info *info, u16 addr, u32 val)
{
	unsigned long flags;

	local_irq_save(flags);
	isp_enter_mode(info, ISP_MODE_FLASH_WRITE);
	isp_flash_set_address(info, addr);
	isp_send_bits(info, val, 32);

	gpio_direction_output(info->pdata->gpio_sda, 1);
	/* 6 clock cycles with different timings for the data to get written
	 * into flash */
	isp_toggle_clk(info, 0, 1, 3);
	isp_toggle_clk(info, 0, 1, 3);
	isp_toggle_clk(info, 0, 1, 6);
	isp_toggle_clk(info, 0, 1, 12);
	isp_toggle_clk(info, 0, 1, 3);
	isp_toggle_clk(info, 0, 1, 3);

	isp_toggle_clk(info, 1, 0, 1);

	gpio_direction_output(info->pdata->gpio_sda, 0);
	isp_exit_mode(info);
	local_irq_restore(flags);
	usleep_range(300, 400);
}

static bool isp_flash_is_erased(struct mms_ts_info *info)
{
	struct i2c_client *client = info->client;
	u32 val;
	u16 addr;

	for (addr = 0; addr < (ISP_MAX_FW_SIZE / 4); addr++) {
		udelay(40);
		val = isp_flash_readl(info, addr);

		if (val != 0xffffffff) {
			dev_dbg(&client->dev,
				"addr 0x%x not erased: 0x%08x != 0xffffffff\n",
				addr, val);
			return false;
		}
	}
	return true;
}

static int isp_fw_write_image(struct mms_ts_info *info, const u8 *data, size_t len)
{
	struct i2c_client *client = info->client;
	u16 addr = 0;

	for (addr = 0; addr < (len / 4); addr++, data += 4) {
		u32 val = get_unaligned_le32(data);
		u32 verify_val;
		int retries = 3;

		while (retries--) {
			isp_flash_writel(info, addr, val);
			verify_val = isp_flash_readl(info, addr);
			if (val == verify_val)
				break;
			dev_err(&client->dev,
				"mismatch @ addr 0x%x: 0x%x != 0x%x\n",
				addr, verify_val, val);
			continue;
		}
		if (retries < 0)
			return -ENXIO;
	}

	return 0;
}

static int isp_fw_download(struct mms_ts_info *info, const u8 *data, size_t len)
{
	struct i2c_client *client = info->client;
	u32 val;
	int ret = 0;
	int i;
	u32 *buf = kzalloc(ISP_CAL_DATA_SIZE * 4, GFP_KERNEL);
	if (!buf) {
		dev_err(&info->client->dev, "%s: failed to allocate memory\n",
			__func__);
		return -ENOMEM;
	}

	if (len % 4) {
		dev_err(&client->dev,
			"fw image size (%d) must be a multiple of 4 bytes\n",
			len);
		kfree(buf);
		return -EINVAL;
	} else if (len > ISP_MAX_FW_SIZE) {
		dev_err(&client->dev,
			"fw image is too big, %d > %d\n", len, ISP_MAX_FW_SIZE);
		kfree(buf);
		return -EINVAL;
	}

	dev_info(&client->dev, "fw download start\n");

	info->pdata->vdd_on(0);
	gpio_direction_output(info->pdata->gpio_sda, 0);
	gpio_direction_output(info->pdata->gpio_scl, 0);
	gpio_direction_output(info->pdata->gpio_resetb, 0);

	hw_reboot_bootloader(info);

	dev_info(&client->dev, "calibration data backup\n");
	for (i = 0; i < ISP_CAL_DATA_SIZE; i++)
		buf[i] = isp_flash_readl(info, ISP_IC_INFO_ADDR);

	val = isp_flash_readl(info, ISP_IC_INFO_ADDR);
	dev_info(&client->dev, "IC info: 0x%02x (%x)\n", val & 0xff, val);

	dev_info(&client->dev, "fw erase...\n");
	isp_flash_erase(info);
	if (!isp_flash_is_erased(info)) {
		ret = -ENXIO;
		goto err;
	}

	dev_info(&client->dev, "fw write...\n");
	/* XXX: what does this do?! */
	isp_flash_writel(info, ISP_IC_INFO_ADDR, 0xffffff00 | (val & 0xff));
	usleep_range(1000, 1500);
	ret = isp_fw_write_image(info, data, len);
	if (ret)
		goto err;
	usleep_range(1000, 1500);

	dev_info(&client->dev, "restoring data\n");
	for (i = 0; i < ISP_CAL_DATA_SIZE; i++)
		isp_flash_writel(info, ISP_IC_INFO_ADDR, buf[i]);
	kfree(buf);

	dev_info(&client->dev, "fw download done...\n");
	hw_reboot_normal(info);
	msleep(200);
	return 0;

err:
	dev_err(&client->dev, "fw download failed...\n");
	kfree(buf);
	hw_reboot_normal(info);
	return ret;
}

static int get_fw_version(struct mms_ts_info *info, u8 area)
{
	struct i2c_client *client = info->client;
	struct i2c_adapter *adapter = client->adapter;
	struct i2c_msg msg;
	u8 reg = MMS_CORE_VERSION;
	int ret;
	unsigned char buf[4];

	msg.addr = client->addr;
	msg.flags = 0x00;
	msg.len = 1;
	msg.buf = &reg;

	disable_irq(info->irq);
	ret = i2c_transfer(adapter, &msg, 1);

	if (ret >= 0) {
		msg.addr = client->addr;
		msg.flags = I2C_M_RD;
		msg.len = 4;
		msg.buf = buf;

		ret = i2c_transfer(adapter, &msg, 1);
	}
	enable_irq(info->irq);
	if (ret < 0) {
		dev_err(&client->dev, "I2C read error : [%d]\n", ret);
		return ret;
	}

	if (area == SEC_BOOTLOADER)
		return buf[0];
	else if (area == SEC_CORE)
		return buf[1];
	else if (area == SEC_CONFIG)
		return buf[2];
	else
		return 0;
}

static int get_hw_version(struct mms_ts_info *info)
{
	int ret;
	int retries = 3;

	/* this seems to fail sometimes after a reset.. retry a few times */
	disable_irq(info->irq);
	do {
		ret = i2c_smbus_read_byte_data(info->client, MMS_HW_REVISION);
	} while (ret < 0 && retries-- > 0);
	enable_irq(info->irq);

	return ret;
}

static int mms_ts_finish_config(struct mms_ts_info *info)
{
	struct i2c_client *client = info->client;
	int ret;

	ret = request_threaded_irq(client->irq, NULL, mms_ts_interrupt,
				   IRQF_TRIGGER_LOW | IRQF_ONESHOT,
				   "mms_ts", info);
	if (ret < 0) {
		ret = 1;
		dev_err(&client->dev, "Failed to register interrupt\n");
		goto err_req_irq;
	}

	info->irq = client->irq;
	barrier();

	dev_info(&client->dev, "Melfas MMS-series touch controller initialized\n");

	return 0;

err_req_irq:
	return ret;
}

static int isp_fw_update(struct mms_ts_info *info)
{
	struct i2c_client *client = info->client;
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	int ret = 0;
	int retries = 3;
	int ver;

	ver = get_fw_version(info, SEC_CONFIG);
	info->fw_ic_ver = ver;
	dev_err(&client->dev, "fw version 0x%02x !!!!\n", ver);

	if (!info->pdata || !info->pdata->mux_fw_flash) {
		ret = 1;
		dev_err(&client->dev, "fw cannot be updated, missing platform data\n");
		return ret;
	}

	disable_irq(info->irq);
	while (retries--) {
		i2c_lock_adapter(adapter);
		info->pdata->mux_fw_flash(true);

		ret = isp_fw_download(info, MELFAS_binary,
				MELFAS_binary_nLength);

		info->pdata->mux_fw_flash(false);
		i2c_unlock_adapter(adapter);

		if (ret < 0) {
			dev_err(&client->dev, "retrying flashing\n");
			continue;
		}

		ver = get_fw_version(info, SEC_CONFIG);
		info->fw_ic_ver = ver;

		if (ver == FW_VERSION) {
			dev_info(&client->dev,
				"fw update done. ver = 0x%02x\n", ver);
			enable_irq(info->irq);
			goto done;
		} else {
			dev_err(&client->dev,
				"ERROR: fw update succeeded, but fw version is still wrong (0x%x != 0x%x)\n",
				ver, FW_VERSION);
			enable_irq(info->irq);
		}
		dev_err(&client->dev, "retrying flashing\n");
	}

	return ret;
done:
	return ret;
}

#if ISC_DL_MODE
static int isc_fw_update(struct mms_ts_info *info)
{
	struct i2c_client *client = info->client;
	int ret = 0;
	int ver;
	int bin_ver;
	int retries = 3;

	ver = get_fw_version(info, SEC_CONFIG);
	info->fw_ic_ver = ver;
	dev_info(&client->dev, "fw version 0x%02x !!!!\n", ver);

	if (!info->pdata || !info->pdata->mux_fw_flash) {
		ret = 1;
		dev_err(&client->dev, "fw cannot be updated, missing platform data\n");
		goto out;
	}

	bin_ver = FW_VERSION;

	while (retries--) {
		ret = mms100_ISC_download_mbinary(info);

		ver = get_fw_version(info, SEC_CONFIG);
		info->fw_ic_ver = ver;

		if (ret == 0) {
			dev_info(&client->dev, "mms100_ISC_download_mbinary success\n");
			goto done;
		} else {
			dev_err(&client->dev, "mms100_ISC_download_mbinary fail [%d]\n",
						ret);
			ret = 1;
		}

		dev_err(&client->dev, "retrying flashing\n");
	}
out:
done:
	return ret;
}
#endif

#ifdef SEC_TSP_FACTORY_TEST
/* SPRD remove this function */
#if 0
static inline int msm_irq_to_gpio(unsigned irq)
{
	/* TODO : Need to verify chip->base=0 */
	return irq - MSM_GPIO_TO_INT(0);
}
#endif
static void set_default_result(struct mms_ts_info *info)
{
	char delim = ':';

	memset(info->cmd_result, 0x00, ARRAY_SIZE(info->cmd_result));
	memcpy(info->cmd_result, info->cmd, strlen(info->cmd));
	strncat(info->cmd_result, &delim, 1);
}

static void set_cmd_result(struct mms_ts_info *info, char *buff, int len)
{
	strncat(info->cmd_result, buff, len);
}

static int get_data(struct mms_ts_info *info, u8 addr, u8 size, u8 *array)
{
	struct i2c_client *client = info->client;
	struct i2c_adapter *adapter = client->adapter;
	struct i2c_msg msg;
	u8 reg = addr;
	unsigned char buf[size];
	int ret;

	msg.addr = client->addr;
	msg.flags = 0x00;
	msg.len = 1;
	msg.buf = &reg;

	ret = i2c_transfer(adapter, &msg, 1);

	if (ret >= 0) {
		msg.addr = client->addr;
		msg.flags = I2C_M_RD;
		msg.len = size;
		msg.buf = buf;

		ret = i2c_transfer(adapter, &msg, 1);
	}
	if (ret < 0) {
		dev_err(&client->dev, "I2C read error : [%d]\n", ret);
		return ret;
	}

	memcpy(array, &buf, size);
	return size;
}

static void get_intensity_data(struct mms_ts_info *info)
{
	u8 w_buf[4];
	u8 r_buf;
	u8 read_buffer[60] = {0};
	int i, j;
	int ret;
	u16 max_value = 0, min_value = 0;
	u16 raw_data;
	char buff[TSP_CMD_STR_LEN] = {0};

	disable_irq(info->irq);

	w_buf[0] = ADDR_UNIV_CMD;
	w_buf[1] = CMD_GET_INTEN;
	w_buf[2] = 0xFF;
	for (i = 0; i < RX_NUM; i++) {
		w_buf[3] = i;

		ret = i2c_smbus_write_i2c_block_data(info->client,
			w_buf[0], 3, &w_buf[1]);
		if (ret < 0)
			goto err_i2c;
		usleep_range(1, 5);

		ret = i2c_smbus_read_i2c_block_data(info->client,
			CMD_RESULT_SZ, 1, &r_buf);
		if (ret < 0)
			goto err_i2c;

		ret = get_data(info, CMD_RESULT, r_buf, read_buffer);
		if (ret < 0)
			goto err_i2c;

		for (j = 0; j < r_buf / 2; j++) {
			raw_data =
			read_buffer[2 * j] | (read_buffer[2 * j + 1] << 8);
			if (raw_data > 32767)
				raw_data = 0;
			if (i == 0 && j == 0) {
				max_value = min_value = raw_data;
			} else {
				max_value = max(max_value, raw_data);
				min_value = min(min_value, raw_data);
			}
			info->intensity[i * TX_NUM + j] = raw_data;
			dev_dbg(&info->client->dev,
				"intensity[%d][%d] = %d\n", j, i,
				info->intensity[i * TX_NUM + j]);
		}
	}

	snprintf(buff, sizeof(buff), "%d,%d", min_value, max_value);
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));

	enable_irq(info->irq);

	return;

err_i2c:
	dev_err(&info->client->dev, "%s: fail to i2c (cmd=%d)\n",
		__func__, MMS_VSC_CMD_INTENSITY);
}

static void get_raw_data(struct mms_ts_info *info, u8 cmd)
{
	u8 w_buf[4];
	u8 r_buf = 0;
	u8 read_buffer[60] = {0};
	int ret;
	int i, j;
	int max_value = 0, min_value = 0;
	int raw_data;
	int retry;
	char buff[TSP_CMD_STR_LEN] = {0};
	int gpio = info->pdata->gpio_int;/* msm_irq_to_gpio(info->irq); */

	disable_irq(info->irq);

	ret = i2c_smbus_write_byte_data(info->client,
		ADDR_UNIV_CMD, CMD_ENTER_TEST);
	if (ret < 0)
		goto err_i2c;

	/* event type check */
	retry = 1;
	while (retry) {
		while (gpio_get_value(gpio))
			udelay(100);

		ret = i2c_smbus_read_i2c_block_data(info->client,
			0x0F, 1, &r_buf);
		if (ret < 0)
			goto err_i2c;

		ret = i2c_smbus_read_i2c_block_data(info->client,
			0x10, 1, &r_buf);
		if (ret < 0)
			goto err_i2c;

		dev_info(&info->client->dev, "event type = 0x%x\n", r_buf);
		if (r_buf == 0x0C)
			retry = 0;
	}

	w_buf[0] = ADDR_UNIV_CMD;
	if (cmd == MMS_VSC_CMD_CM_DELTA)
		w_buf[1] = CMD_CM_DELTA;
	else
		w_buf[1] = CMD_CM_ABS;
	ret = i2c_smbus_write_i2c_block_data(info->client,
		 w_buf[0], 1, &w_buf[1]);
	if (ret < 0)
		goto err_i2c;
	while (gpio_get_value(gpio))
		udelay(100);

	ret = i2c_smbus_read_i2c_block_data(info->client,
		CMD_RESULT_SZ, 1, &r_buf);
	if (ret < 0)
		goto err_i2c;
	ret = i2c_smbus_read_i2c_block_data(info->client,
		CMD_RESULT, 1, &r_buf);
	if (ret < 0)
		goto err_i2c;

	if (r_buf == 1)
		dev_info(&info->client->dev, "PASS\n");
	else
		dev_info(&info->client->dev, "FAIL\n");

	if (cmd == MMS_VSC_CMD_CM_DELTA)
		w_buf[1] = CMD_GET_DELTA;
	else
		w_buf[1] = CMD_GET_ABS;
	w_buf[2] = 0xFF;

	for (i = 0; i < RX_NUM; i++) {
		w_buf[3] = i;

		ret = i2c_smbus_write_i2c_block_data(info->client,
			w_buf[0], 3, &w_buf[1]);
		if (ret < 0)
			goto err_i2c;

		while (gpio_get_value(gpio))
			udelay(100);

		ret = i2c_smbus_read_i2c_block_data(info->client,
			CMD_RESULT_SZ, 1, &r_buf);
		if (ret < 0)
			goto err_i2c;

		ret = get_data(info, CMD_RESULT, r_buf, read_buffer);
		if (ret < 0)
			goto err_i2c;

		for (j = 0; j < TX_NUM; j++) {
			raw_data =
			read_buffer[2 * j] | (read_buffer[2 * j + 1] << 8);
			if (i == 0 && j == 0) {
				max_value = min_value = raw_data;
			} else {
				max_value = max(max_value, raw_data);
				min_value = min(min_value, raw_data);
			}

			if (cmd == MMS_VSC_CMD_CM_DELTA) {
				info->inspection[i * TX_NUM + j] =
					raw_data;
				dev_dbg(&info->client->dev,
					"delta[%d][%d] = %d\n", j, i,
					info->inspection[i * TX_NUM + j]);
			} else if (cmd == MMS_VSC_CMD_CM_ABS) {
				info->raw[i * TX_NUM + j] =
					raw_data;
				dev_dbg(&info->client->dev,
					"raw[%d][%d] = %d\n", j, i,
					info->raw[i * TX_NUM + j]);
			} else if (cmd == MMS_VSC_CMD_REFER) {
				info->reference[i * TX_NUM + j] =
					raw_data;
				dev_dbg(&info->client->dev,
					"reference[%d][%d] = %d\n", j, i,
					info->reference[i * TX_NUM + j]);
			}
		}
	}

	ret = i2c_smbus_write_byte_data(info->client,
		ADDR_UNIV_CMD, CMD_EXIT_TEST);

	snprintf(buff, sizeof(buff), "%d,%d", min_value, max_value);
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));

	touch_is_pressed = 0;
	release_all_fingers(info);

	info->pdata->vdd_on(0);
	msleep(30);
	info->pdata->vdd_on(1);
	msleep(250);

	if (info->ta_status) {
		dev_notice(&info->client->dev, "TA connect!!!\n");
		i2c_smbus_write_byte_data(info->client, 0x33, 0x1);
	} else {
		dev_notice(&info->client->dev, "TA disconnect!!!\n");
		i2c_smbus_write_byte_data(info->client, 0x33, 0x2);
	}
	mms_set_noise_mode(info);

	enable_irq(info->irq);

	return;

err_i2c:
	dev_err(&info->client->dev, "%s: fail to i2c (cmd=%d)\n",
		__func__, cmd);
}

static void get_raw_data_all(struct mms_ts_info *info, u8 cmd)
{
	u8 w_buf[6];
	u8 read_buffer[2]; /* 52 */
	char buff[TSP_CMD_STR_LEN] = {0};
	int gpio;
	int ret;
	int i, j;
	u32 max_value, min_value;
	u32 raw_data;

	gpio = info->pdata->gpio_int;/*msm_irq_to_gpio(info->irq);*/
	disable_irq(info->irq);

	w_buf[0] = MMS_VSC_CMD;		/* vendor specific command id */
	w_buf[1] = MMS_VSC_MODE;	/* mode of vendor */
	w_buf[2] = 0;			/* tx line */
	w_buf[3] = 0;			/* rx line */
	w_buf[4] = 0;			/* reserved */
	w_buf[5] = 0;			/* sub command */

	if (cmd == MMS_VSC_CMD_EXIT) {
		w_buf[5] = MMS_VSC_CMD_EXIT; /* exit test mode */

		ret = i2c_smbus_write_i2c_block_data(info->client,
			w_buf[0], 5, &w_buf[1]);
		if (ret < 0)
			goto err_i2c;
		touch_is_pressed = 0;
		release_all_fingers(info);
#if defined(CONFIG_MIPI_SAMSUNG_ESD_REFRESH)
		set_esd_disable();
#endif
		info->pdata->vdd_on(0);
		msleep(30);
		info->pdata->vdd_on(1);
		msleep(120);
		enable_irq(info->irq);
#if defined(CONFIG_MIPI_SAMSUNG_ESD_REFRESH)
		set_esd_enable();
#endif
		return ;
	}

	/* MMS_VSC_CMD_CM_DELTA or MMS_VSC_CMD_CM_ABS
	 * this two mode need to enter the test mode
	 * exit command must be followed by testing.
	 */
	if (cmd == MMS_VSC_CMD_CM_DELTA || cmd == MMS_VSC_CMD_CM_ABS) {
		/* enter the debug mode */
		w_buf[2] = 0x0; /* tx */
		w_buf[3] = 0x0; /* rx */
		w_buf[5] = MMS_VSC_CMD_ENTER;

		ret = i2c_smbus_write_i2c_block_data(info->client,
			w_buf[0], 5, &w_buf[1]);
		if (ret < 0)
			goto err_i2c;

		/* wating for the interrupt */
		while (gpio_get_value(gpio))
			udelay(100);
	}

	max_value = 0;
	min_value = 0;

	for (i = 0; i < RX_NUM; i++) {
		for (j = 0; j < TX_NUM; j++) {

			w_buf[2] = j; /* tx */
			w_buf[3] = i; /* rx */
			w_buf[5] = cmd;

			ret = i2c_smbus_write_i2c_block_data(info->client,
					w_buf[0], 5, &w_buf[1]);
			if (ret < 0)
				goto err_i2c;

			usleep_range(1, 5);

			ret = i2c_smbus_read_i2c_block_data(info->client, 0xBF,
					2, read_buffer);
			if (ret < 0)
				goto err_i2c;

			raw_data = ((u16)read_buffer[1] << 8) | read_buffer[0];

			if (i == 0 && j == 0) {
				max_value = min_value = raw_data;
			} else {
				max_value = max(max_value, raw_data);
				min_value = min(min_value, raw_data);
			}

			if (cmd == MMS_VSC_CMD_INTENSITY) {
				info->intensity[j * RX_NUM + i] = raw_data;
				dev_dbg(&info->client->dev, "intensity[%d][%d] = %d\n",
					i, j, info->intensity[j * RX_NUM + i]);
			} else if (cmd == MMS_VSC_CMD_CM_DELTA) {
				info->inspection[j * RX_NUM + i] = raw_data;
				dev_dbg(&info->client->dev, "delta[%d][%d] = %d\n",
					i, j, info->inspection[j * RX_NUM + i]);
			} else if (cmd == MMS_VSC_CMD_CM_ABS) {
				info->raw[j * RX_NUM + i] = raw_data;
				dev_dbg(&info->client->dev, "raw[%d][%d] = %d\n",
					i, j, info->raw[j * RX_NUM + i]);
			} else if (cmd == MMS_VSC_CMD_REFER) {
				info->reference[j * RX_NUM + i] =
						raw_data >> 3;
				dev_dbg(&info->client->dev, "reference[%d][%d] = %d\n",
					i, j, info->reference[j * RX_NUM + i]);
			}
		}
	}

	snprintf(buff, sizeof(buff), "%d,%d", min_value, max_value);
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));

	enable_irq(info->irq);

err_i2c:
	dev_err(&info->client->dev, "%s: fail to i2c (cmd=%d)\n",
			__func__, cmd);
}

#if defined(ESD_DEBUG)
static u32 get_raw_data_one(struct mms_ts_info *info, u16 rx_idx, u16 tx_idx,
		u8 cmd)
{
	u8 w_buf[6];
	u8 read_buffer[2];
	int ret;
	u32 raw_data;

	w_buf[0] = MMS_VSC_CMD;		/* vendor specific command id */
	w_buf[1] = MMS_VSC_MODE;	/* mode of vendor */
	w_buf[2] = 0;			/* tx line */
	w_buf[3] = 0;			/* rx line */
	w_buf[4] = 0;			/* reserved */
	w_buf[5] = 0;			/* sub command */

	if (cmd != MMS_VSC_CMD_INTENSITY && cmd != MMS_VSC_CMD_RAW &&
		cmd != MMS_VSC_CMD_REFER && cmd != VSC_INTENSITY_TK &&
		cmd != VSC_RAW_TK) {
		dev_err(&info->client->dev, "%s: not profer command(cmd=%d)\n",
				__func__, cmd);
		return FAIL;
	}

	w_buf[2] = tx_idx;	/* tx */
	w_buf[3] = rx_idx;	/* rx */
	w_buf[5] = cmd;		/* sub command */

	ret = i2c_smbus_write_i2c_block_data(info->client, w_buf[0], 5,
			&w_buf[1]);
	if (ret < 0)
		goto err_i2c;

	ret = i2c_smbus_read_i2c_block_data(info->client, 0xBF, 2,
			read_buffer);
	if (ret < 0)
		goto err_i2c;

	raw_data = ((u16)read_buffer[1] << 8) | read_buffer[0];
	if (cmd == MMS_VSC_CMD_REFER)
		raw_data = raw_data >> 4;

	return raw_data;

err_i2c:
	dev_err(&info->client->dev, "%s: fail to i2c (cmd=%d)\n",
			__func__, cmd);
	return FAIL;
}
#endif

#if defined(SEC_TKEY_FACTORY_TEST)
static u32 get_raw_data_tkey(struct mms_ts_info *info, u16 key_idx, u8 cmd)
{
	u8 w_buf[4];
	u8 r_buf = 0;
	u8 read_buffer[60] = {0};
	int ret, raw_data;
	int gpio = info->pdata->gpio_int;/*msm_irq_to_gpio(info->irq);*/

	disable_irq(info->irq);

	w_buf[0] = ADDR_UNIV_CMD;		/* Universal CMD ID */
	w_buf[1] = CMD_ENTER_TEST;		/* mode of vendor */
	w_buf[2] = 0xFF;		/* Exciting CH*/
	w_buf[3] = 0;		/* Sensing CH */

	if (cmd != VSC_INTENSITY_TK && cmd != VSC_RAW_TK) {
		dev_err(&info->client->dev, "%s: not profer command(cmd=%d)\n",
				__func__, cmd);
		return FAIL;
	}

	if (cmd == VSC_INTENSITY_TK)
		w_buf[1] = CMD_GET_INTEN_KEY;
	else
		w_buf[1] = CMD_GET_REFER_KEY;

	ret = i2c_smbus_write_i2c_block_data(info->client,
			w_buf[0], 3, &w_buf[1]);

	if (ret < 0)
		goto err_i2c;
	while (gpio_get_value(gpio))
		udelay(100);

	ret = i2c_smbus_read_i2c_block_data(info->client,
			CMD_RESULT_SZ, 1, &r_buf);
	if (ret < 0)
		goto err_i2c;
	ret = get_data(info, CMD_RESULT, r_buf, read_buffer);
	if (ret < 0)
		goto err_i2c;

	raw_data = read_buffer[2*key_idx] | (read_buffer[2*key_idx+1] << 8);

	/* if raw_data < 0, return 0*/
	if (read_buffer[2*key_idx+1] & (1 << 7))
		raw_data = 0;

	touch_is_pressed = 0;
	info->enabled = true;

	enable_irq(info->irq);
	return raw_data;

err_i2c:
	dev_err(&info->client->dev, "%s: fail to i2c (cmd=%d)\n",
			__func__, cmd);
	return FAIL;
}
#endif

static ssize_t show_close_tsp_test(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);

	get_raw_data_all(info, MMS_VSC_CMD_EXIT);
	info->ft_flag = 0;

	return snprintf(buf, TSP_BUF_SIZE, "%u\n", 0);
}

static int check_rx_tx_num(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;

	char buff[TSP_CMD_STR_LEN] = {0};
	int node;

	if (info->cmd_param[0] < 0 ||
			info->cmd_param[0] >= TX_NUM  ||
			info->cmd_param[1] < 0 ||
			info->cmd_param[1] >= RX_NUM) {
		snprintf(buff, sizeof(buff) , "%s", "NG");
		set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
		info->cmd_state = 3;

		dev_err(&info->client->dev, "%s: parameter error: %u,%u\n",
			__func__, info->cmd_param[0], info->cmd_param[1]);
		node = -1;
		return node;
	}
	node = info->cmd_param[1] * RX_NUM + info->cmd_param[0];
	dev_info(&info->client->dev, "%s: node = %d\n", __func__,
			node);
	return node;

}

static void not_support_cmd(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;
	char buff[16] = {0};

	set_default_result(info);
	sprintf(buff, "%s", "NA");
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = 4;
	dev_info(&info->client->dev, "%s: \"%s(%d)\"\n", __func__,
				buff, strnlen(buff, sizeof(buff)));
	return;
}

static void fw_update(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;
	struct i2c_client *client = info->client;
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	int ret = 0;
	int fw_ver = 0, ver = 0, hw_rev = 0, fw_bin_ver = 0;
	int retries = 5;
	const u8 *buff = 0;
	mm_segment_t old_fs = {0};
	struct file *fp = NULL;
	long fsize = 0, nread = 0;

	#define MMS_TS "/sdcard/melfas_fw.bin"

	set_default_result(info);

	hw_rev = get_hw_version(info);
	fw_bin_ver = FW_VERSION;
	fw_ver = get_fw_version(info, SEC_CONFIG);
	dev_info(&client->dev, "fw_ic_ver = 0x%02x, fw_bin_ver = 0x%02x\n",
		fw_ver, fw_bin_ver);

	if (info->cmd_param[0] == 0
			&& fw_ver >= fw_bin_ver) {
		dev_info(&client->dev, "fw version update does not need\n");
		goto do_not_need_update;
	}

	switch (info->cmd_param[0]) {
	case BUILT_IN:
		buff = MELFAS_binary;
		fsize = MELFAS_binary_nLength;
		dev_info(&client->dev, "built in fw(0x%02x) is loaded!!\n",
					fw_bin_ver);
#if ISC_DL_MODE
		ret = isc_fw_update(info);
#else
		ret = isp_fw_update(info);
#endif

		if (ret) {
			dev_err(&client->dev,
				"failed to initialize (%d)\n", ret);
		}

		ver = get_fw_version(info, SEC_CONFIG);
		info->fw_ic_ver = ver;

		if (info->cmd_param[0] == 1) {
			dev_info(&client->dev,
				"fw update done. ver = 0x%02x\n", ver);
			info->cmd_state = 2;
			enable_irq(info->irq);
			return;
		} else if (ver == fw_bin_ver) {
			dev_info(&client->dev,
				"fw update done. ver = 0x%02x\n", ver);
			info->cmd_state = 2;
			enable_irq(info->irq);
			return;
		} else {
			dev_err(&client->dev, "ERROR : fw version is still wrong (0x%x != 0x%x)\n",
				ver, FW_VERSION);
		}

		break;

	case UMS:
		old_fs = get_fs();
		set_fs(get_ds());

		fp = filp_open(MMS_TS, O_RDONLY, 0);
		if (IS_ERR(fp)) {
			dev_err(&client->dev,
				"file %s open error:%d\n", MMS_TS, (s32)fp);
			info->cmd_state = 3;
			goto err_open;
		}

		fsize = fp->f_path.dentry->d_inode->i_size;

		buff = kzalloc((size_t)fsize, GFP_KERNEL);
		if (!buff) {
			dev_err(&client->dev, "fail to alloc buffer for fw\n");
			info->cmd_state = 3;
			goto err_alloc;
		}

		nread = vfs_read(fp, (char __user *)buff, fsize, &fp->f_pos);
		if (nread != fsize) {
			dev_err(&client->dev, "fail to read file %s (nread = %ld)\n",
					MMS_TS, nread);
			info->cmd_state = 3;
			goto err_fw_size;
		}

		filp_close(fp, current->files);
		set_fs(old_fs);
		dev_info(&client->dev, "ums fw is loaded!!\n");
		break;

	default:
		dev_err(&client->dev, "invalid fw file type!!\n");
		goto not_support;
	}

	disable_irq(info->irq);
	while (retries--) {
		i2c_lock_adapter(adapter);
		info->pdata->mux_fw_flash(true);

		ret = isp_fw_download(info, (const u8 *)buff,
				(const size_t)fsize);

		info->pdata->mux_fw_flash(false);
		i2c_unlock_adapter(adapter);

		if (ret < 0) {
			dev_err(&client->dev, "retrying flashing\n");
			continue;
		}

		ver = get_fw_version(info, SEC_CONFIG);
		info->fw_ic_ver = ver;

		if (info->cmd_param[0] == 1) {
			dev_info(&client->dev,
				"fw update done. ver = 0x%02x\n", ver);
			info->cmd_state = 2;
			enable_irq(info->irq);
			return;
		} else if (ver == fw_bin_ver) {
			dev_info(&client->dev,
				"fw update done. ver = 0x%02x\n", ver);
			info->cmd_state = 2;
			enable_irq(info->irq);
			return;
		} else {
			dev_err(&client->dev, "ERROR : fw version is still wrong (0x%x != 0x%x)\n",
				ver, FW_VERSION);
		}
		dev_err(&client->dev, "retrying flashing\n");
	}

err_fw_size:
	kfree(buff);
err_alloc:
	filp_close(fp, NULL);
err_open:
	set_fs(old_fs);
not_support:
do_not_need_update:
	info->cmd_state = 2;
	return;
}

static void get_fw_ver_bin(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;

	char buff[16] = {0};
	int hw_rev;

	set_default_result(info);
	hw_rev = get_hw_version(info);

	snprintf(buff, sizeof(buff), "ME0453%02x", FW_VERSION);

	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = 2;
	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));
}

static void get_fw_ver_ic(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;

	char buff[16] = {0};

	set_default_result(info);

	if (info->enabled) {
		info->fw_boot_ver = get_fw_version(info, SEC_BOOTLOADER);
		info->fw_core_ver = get_fw_version(info, SEC_CORE);
		info->fw_ic_ver = get_fw_version(info, SEC_CONFIG);
	}
	snprintf(buff, sizeof(buff), "ME%02x%02x%02x",
		info->fw_boot_ver, info->fw_core_ver, info->fw_ic_ver);

	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = 2;
	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));
}

static void get_config_ver(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;

	char buff[20] = {0};

	set_default_result(info);

	snprintf(buff, sizeof(buff), "0x%X", info->fw_ic_ver);
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = 2;
	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));
}

static void get_threshold(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;

	char buff[16] = {0};
	int threshold;

	set_default_result(info);

	disable_irq(info->irq);
	threshold = i2c_smbus_read_byte_data(info->client, 0x05);
	enable_irq(info->irq);
	if (threshold < 0) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
		info->cmd_state = 3;
		return;
	}
	snprintf(buff, sizeof(buff), "%d", threshold);

	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = 2;
	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));
}

static void module_off_master(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;

	char buff[3] = {0};

	mutex_lock(&info->lock);
	if (info->enabled) {
		disable_irq(info->irq);
		info->enabled = false;
		touch_is_pressed = 0;
	}
	mutex_unlock(&info->lock);

	info->pdata->vdd_on(0);

	if (info->pdata->is_vdd_on() == 0)
		snprintf(buff, sizeof(buff), "%s", "OK");
	else
		snprintf(buff, sizeof(buff), "%s", "NG");

	set_default_result(info);
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));

	if (strncmp(buff, "OK", 2) == 0)
		info->cmd_state = 2;
	else
		info->cmd_state = 3;

	dev_info(&info->client->dev, "%s: %s\n", __func__, buff);
}

static void module_on_master(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;

	char buff[3] = {0};

	mms_pwr_on_reset(info);

	mutex_lock(&info->lock);
	if (!info->enabled) {
		enable_irq(info->irq);
		info->enabled = true;
	}
	mutex_unlock(&info->lock);

	if (info->pdata->is_vdd_on() == 1)
		snprintf(buff, sizeof(buff), "%s", "OK");
	else
		snprintf(buff, sizeof(buff), "%s", "NG");

	set_default_result(info);
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));

	if (strncmp(buff, "OK", 2) == 0)
		info->cmd_state = 2;
	else
		info->cmd_state = 3;

	dev_info(&info->client->dev, "%s: %s\n", __func__, buff);

}

static void get_chip_vendor(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;

	char buff[16] = {0};

	set_default_result(info);

	snprintf(buff, sizeof(buff), "%s", "MELFAS");
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = 2;
	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));
}

static void get_chip_name(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;

	char buff[16] = {0};

	set_default_result(info);

	snprintf(buff, sizeof(buff), "%s", info->pdata->tsp_ic_name);
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = 2;
	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));
}

static void get_reference(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;

	char buff[16] = {0};
	unsigned int val;
	int node;

	set_default_result(info);
	node = check_rx_tx_num(info);

	if (node < 0)
		return;

	val = info->reference[node];
	snprintf(buff, sizeof(buff), "%u", val);
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));

	info->cmd_state = 2;

	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));

}

static void get_cm_abs(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;

	char buff[16] = {0};
	unsigned int val;
	int node;

	set_default_result(info);
	node = check_rx_tx_num(info);

	if (node < 0)
		return;

	val = info->raw[node];
	snprintf(buff, sizeof(buff), "%u", val);
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = 2;

	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__, buff,
			strnlen(buff, sizeof(buff)));
}

static void get_cm_delta(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;

	char buff[16] = {0};
	unsigned int val;
	int node;

	set_default_result(info);
	node = check_rx_tx_num(info);

	if (node < 0)
		return;

	val = info->inspection[node];
	snprintf(buff, sizeof(buff), "%u", val);
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = 2;

	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__, buff,
			strnlen(buff, sizeof(buff)));
}

static void get_intensity(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;

	char buff[16] = {0};
	unsigned int val;
	int node;

	set_default_result(info);
	node = check_rx_tx_num(info);

	if (node < 0)
		return;

	val = info->intensity[node];

	snprintf(buff, sizeof(buff), "%u", val);
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = 2;

	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__, buff,
			strnlen(buff, sizeof(buff)));
}

static void get_x_num(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;

	char buff[16] = {0};
	int val;

	set_default_result(info);
	disable_irq(info->irq);
	val = i2c_smbus_read_byte_data(info->client, 0x0B);
	enable_irq(info->irq);
	if (val < 0) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
		info->cmd_state = 3;

		dev_err(&info->client->dev,
			"%s: fail to read num of x (%d).\n", __func__, val);

		return ;
	}
	snprintf(buff, sizeof(buff), "%u", val);
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = 2;

	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__, buff,
			strnlen(buff, sizeof(buff)));
}

static void get_y_num(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;

	char buff[16] = {0};
	int val;

	set_default_result(info);
	disable_irq(info->irq);
	val = i2c_smbus_read_byte_data(info->client, 0x0C);
	enable_irq(info->irq);
	if (val < 0) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
		info->cmd_state = 3;

		dev_err(&info->client->dev,
			"%s: fail to read num of y (%d).\n", __func__, val);

		return ;
	}
	snprintf(buff, sizeof(buff), "%u", val);
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = 2;

	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__, buff,
			strnlen(buff, sizeof(buff)));
}

static void run_reference_read(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;

	set_default_result(info);
	/* CORE 4.5
	get_raw_data_all(info, MMS_VSC_CMD_REFER);
	*/
	get_raw_data(info, MMS_VSC_CMD_REFER);
	info->cmd_state = 2;

	dev_info(&info->client->dev, "%s\n", __func__);
}

static void run_cm_abs_read(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;

	set_default_result(info);
	/* CORE 4.5
	get_raw_data_all(info, MMS_VSC_CMD_CM_ABS);
	get_raw_data_all(info, MMS_VSC_CMD_EXIT);
	*/
	get_raw_data(info, MMS_VSC_CMD_CM_ABS);
	info->cmd_state = 2;

	dev_info(&info->client->dev, "%s\n", __func__);
}

static void run_cm_delta_read(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;

	set_default_result(info);
	/* CORE 4.5
	get_raw_data_all(info, MMS_VSC_CMD_CM_DELTA);
	get_raw_data_all(info, MMS_VSC_CMD_EXIT);
	*/
	get_raw_data(info, MMS_VSC_CMD_CM_DELTA);
	info->cmd_state = 2;

	dev_info(&info->client->dev, "%s\n", __func__);
}

static void run_intensity_read(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;

	set_default_result(info);
	/* CORE 4.5
	get_raw_data_all(info, MMS_VSC_CMD_INTENSITY);
	*/
	get_intensity_data(info);
	info->cmd_state = 2;

	dev_info(&info->client->dev, "%s\n", __func__);
}

static ssize_t store_cmd(struct device *dev, struct device_attribute
		*devattr, const char *buf, size_t count)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;

	char *cur, *start, *end;
	char buff[TSP_CMD_STR_LEN] = {0};
	int len, i;
	struct tsp_cmd *tsp_cmd_ptr = NULL;
	char delim = ',';
	bool cmd_found = false;
	int param_cnt = 0;


	if (info->cmd_is_running == true) {
		dev_err(&info->client->dev, "tsp_cmd: other cmd is running.\n");
		goto err_out;
	}


	/* check lock  */
	mutex_lock(&info->cmd_lock);
	info->cmd_is_running = true;
	mutex_unlock(&info->cmd_lock);

	info->cmd_state = 1;

	for (i = 0; i < ARRAY_SIZE(info->cmd_param); i++)
		info->cmd_param[i] = 0;

	len = (int)count;
	if (*(buf + len - 1) == '\n')
		len--;
	memset(info->cmd, 0x00, ARRAY_SIZE(info->cmd));
	memcpy(info->cmd, buf, len);

	cur = strchr(buf, (int)delim);
	if (cur)
		memcpy(buff, buf, cur - buf);
	else
		memcpy(buff, buf, len);

	/* find command */
	list_for_each_entry(tsp_cmd_ptr, &info->cmd_list_head, list) {
		if (!strcmp(buff, tsp_cmd_ptr->cmd_name)) {
			cmd_found = true;
			break;
		}
	}

	/* set not_support_cmd */
	if (!cmd_found) {
		list_for_each_entry(tsp_cmd_ptr, &info->cmd_list_head, list) {
			if (!strcmp("not_support_cmd", tsp_cmd_ptr->cmd_name))
				break;
		}
	}

	/* parsing parameters */
	if (cur && cmd_found) {
		cur++;
		start = cur;
		memset(buff, 0x00, ARRAY_SIZE(buff));
		do {
			if (*cur == delim || cur - buf == len) {
				end = cur;
				memcpy(buff, start, end - start);
				*(buff + strlen(buff)) = '\0';
				if (kstrtoint(buff, 10,
					info->cmd_param + param_cnt) < 0)
					goto err_out;
				start = cur + 1;
				memset(buff, 0x00, ARRAY_SIZE(buff));
				param_cnt++;
			}
			cur++;
		} while (cur - buf <= len);
	}

	dev_info(&client->dev, "cmd = %s\n", tsp_cmd_ptr->cmd_name);
	for (i = 0; i < param_cnt; i++)
		dev_info(&client->dev, "cmd param %d= %d\n", i,
							info->cmd_param[i]);

	tsp_cmd_ptr->cmd_func(info);


err_out:
	return count;
}

static ssize_t show_cmd_status(struct device *dev,
		struct device_attribute *devattr, char *buf)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);
	char buff[16] = {0};

	dev_info(&info->client->dev, "tsp cmd: status:%d\n", info->cmd_state);

	if (info->cmd_state == 0)
		snprintf(buff, sizeof(buff), "WAITING");

	else if (info->cmd_state == 1)
		snprintf(buff, sizeof(buff), "RUNNING");

	else if (info->cmd_state == 2)
		snprintf(buff, sizeof(buff), "OK");

	else if (info->cmd_state == 3)
		snprintf(buff, sizeof(buff), "FAIL");

	else if (info->cmd_state == 4)
		snprintf(buff, sizeof(buff), "NOT_APPLICABLE");

	return snprintf(buf, TSP_BUF_SIZE, "%s\n", buff);
}

static ssize_t show_cmd_result(struct device *dev, struct device_attribute
		*devattr, char *buf)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);

	dev_info(&info->client->dev, "tsp cmd: result: %s\n", info->cmd_result);

	mutex_lock(&info->cmd_lock);
	info->cmd_is_running = false;
	mutex_unlock(&info->cmd_lock);

	info->cmd_state = 0;

	return snprintf(buf, TSP_BUF_SIZE, "%s\n", info->cmd_result);
}

#ifdef ESD_DEBUG

static bool intensity_log_flag;

static ssize_t show_intensity_logging_on(struct device *dev,
		struct device_attribute *devattr, char *buf)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;
	struct file *fp;
	char log_data[160] = {0,};
	char buff[16] = {0,};
	mm_segment_t old_fs;
	long nwrite;
	u32 val;
	int i, y, c;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

#define MELFAS_DEBUG_LOG_PATH "/sdcard/melfas_log"

	dev_info(&client->dev, "%s: start.\n", __func__);
	fp = filp_open(MELFAS_DEBUG_LOG_PATH, O_RDWR|O_CREAT,
			S_IRWXU|S_IRWXG|S_IRWXO);
	if (IS_ERR(fp)) {
		dev_err(&client->dev, "%s: fail to open log file\n", __func__);
		goto open_err;
	}

	intensity_log_flag = 1;
	do {
		for (y = 0; y < 3; y++) {
			/* for tx chanel 0~2 */
			memset(log_data, 0x00, 160);

			snprintf(buff, 16, "%1u: ", y);
			strncat(log_data, buff, strnlen(buff, 16));

			for (i = 0; i < RX_NUM; i++) {
				val = get_raw_data_one(info, i, y,
						MMS_VSC_CMD_INTENSITY);
				snprintf(buff, 16, "%5u, ", val);
				strncat(log_data, buff, strnlen(buff, 16));
			}
			memset(buff, '\n', 2);
			c = (y == 2) ? 2 : 1;
			strncat(log_data, buff, c);
			nwrite = vfs_write(fp, (const char __user *)log_data,
					strnlen(log_data, 160), &fp->f_pos);
		}
		usleep_range(5000);
	} while (intensity_log_flag);

	filp_close(fp, current->files);
	set_fs(old_fs);

	return 0;

 open_err:
	set_fs(old_fs);
	return FAIL;
}

static ssize_t show_intensity_logging_off(struct device *dev,
		struct device_attribute *devattr, char *buf)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);
	intensity_log_flag = 0;
	usleep_range(10000);
	get_raw_data_all(info, MMS_VSC_CMD_EXIT);
	return 0;
}

#endif

#ifdef SEC_TKEY_FACTORY_TEST
static ssize_t tkey_threshold_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;
	int tkey_threshold;

	disable_irq(info->irq);
	tkey_threshold = i2c_smbus_read_byte_data(info->client, 0x20);
	dev_info(&client->dev, "touch key threshold: %d\n", tkey_threshold);
	enable_irq(info->irq);
	return snprintf(buf, sizeof(int), "%d\n", tkey_threshold);
}

static ssize_t back_key_state_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;
	int i, ret, val;

	for (i = 0; i < ARRAY_SIZE(info->keycode); i++) {
		if (info->keycode[i] == KEY_BACK)
			break;
	}
	dev_info(&client->dev, "back key state: %d\n", info->key_pressed[i]);

	/* back key*/
	ret = get_raw_data_tkey(info, 1, VSC_INTENSITY_TK);

	if (ret < 0)
		dev_err(&client->dev, "%s: fail to read (%d)\n", __func__, ret);

	val = (u16)ret;

	dev_info(&client->dev, "%s: val=%d\n", __func__, val);
	return sprintf(buf, "%d\n", val);
}

static ssize_t home_key_state_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;
	int i, ret, val;

	for (i = 0; i < ARRAY_SIZE(info->keycode); i++) {
		if (info->keycode[i] == KEY_HOMEPAGE)
			break;
	}
	dev_info(&client->dev, "home key state: %d\n", info->key_pressed[i]);

	/* home key*/
	ret = get_raw_data_tkey(info, 1, VSC_INTENSITY_TK);
	if (ret < 0)
		dev_err(&client->dev, "%s: fail to read (%d)\n", __func__, ret);

	val = (u16)ret;

	dev_info(&client->dev, "%s: val=%d\n", __func__, val);
	return sprintf(buf, "%d\n", val);
}

static ssize_t recent_key_state_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;
	int i, ret, val;

	for (i = 0; i < ARRAY_SIZE(info->keycode); i++) {
		if (info->keycode[i] == KEY_F3)
			break;
	}
	dev_info(&client->dev, "recent key state: %d\n", info->key_pressed[i]);

	/* recent key*/
	ret = get_raw_data_tkey(info, 2, VSC_INTENSITY_TK);
	if (ret < 0)
		dev_err(&client->dev, "%s: fail to read (%d)\n", __func__, ret);

	val = (u16)ret;

	dev_info(&client->dev, "%s: val=%d\n", __func__, val);
	return sprintf(buf, "%d\n", val);
}

static ssize_t menu_key_state_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;
	int i, ret, val;

	for (i = 0; i < ARRAY_SIZE(info->keycode); i++) {
		if (info->keycode[i] == KEY_MENU)
			break;
	}
	dev_info(&client->dev, "menu key state: %d\n", info->key_pressed[i]);

	/* menu key*/
	ret = get_raw_data_tkey(info, 0, VSC_INTENSITY_TK);

	if (ret < 0)
		dev_err(&client->dev, "%s: fail to read (%d)\n", __func__, ret);

	val = (u16)ret;

	dev_info(&client->dev, "%s: val=%d\n", __func__, val);
	return sprintf(buf, "%d\n", val);
}

static ssize_t tkey_rawcounter_show0(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;
	u32 ret;
	u16 val;

	/* back key*/
	ret = get_raw_data_tkey(info, 0, VSC_RAW_TK);
	if (ret < 0)
		dev_err(&client->dev, "%s: fail to read (%d)\n", __func__, ret);

	val = (u16)ret;

	dev_info(&client->dev, "%s: val=%d\n", __func__, val);
	return sprintf(buf, "%d\n", val);
}

static ssize_t tkey_rawcounter_show1(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;
	int ret;
	u16 val;

	/* home key*/
	ret = get_raw_data_tkey(info, 1, VSC_RAW_TK);
	if (ret < 0)
		dev_err(&client->dev, "%s: fail to read (%d)\n", __func__, ret);

	val = (u16)ret;

	dev_info(&client->dev, "%s: val=%d\n", __func__, val);
	return sprintf(buf, "%d\n", val);
}

static ssize_t tkey_rawcounter_show2(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;
	int ret;
	u16 val;

	/* recent key*/
	ret = get_raw_data_tkey(info, 2, VSC_RAW_TK);
	if (ret < 0)
		dev_err(&client->dev, "%s: fail to read (%d)\n", __func__, ret);

	val = (u16)ret;

	dev_info(&client->dev, "%s: val=%d\n", __func__, val);
	return sprintf(buf, "%d\n", val);
}

static ssize_t tkey_rawcounter_show3(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;
	int ret;
	u16 val;

	/* menu key*/
	ret = get_raw_data_tkey(info, 3, VSC_RAW_TK);
	if (ret < 0)
		dev_err(&client->dev, "%s: fail to read (%d)\n", __func__, ret);

	val = (u16)ret;

	dev_info(&client->dev, "%s: val=%d\n", __func__, val);
	return sprintf(buf, "%d\n", val);
}
#endif



#ifdef SEC_TKEY_FACTORY_TEST
static DEVICE_ATTR(touchkey_threshold, S_IRUGO, tkey_threshold_show, NULL);
static DEVICE_ATTR(touchkey_back, S_IRUGO, back_key_state_show, NULL);
static DEVICE_ATTR(touchkey_home, S_IRUGO, home_key_state_show, NULL);
static DEVICE_ATTR(touchkey_recent, S_IRUGO, recent_key_state_show, NULL);
static DEVICE_ATTR(touchkey_menu, S_IRUGO, menu_key_state_show, NULL);
static DEVICE_ATTR(touchkey_raw_data0, S_IRUGO, tkey_rawcounter_show0, NULL) ;
static DEVICE_ATTR(touchkey_raw_data1, S_IRUGO, tkey_rawcounter_show1, NULL) ;
static DEVICE_ATTR(touchkey_raw_data2, S_IRUGO, tkey_rawcounter_show2, NULL) ;
static DEVICE_ATTR(touchkey_raw_data3, S_IRUGO, tkey_rawcounter_show3, NULL) ;

static struct attribute *touchkey_attributes[] = {
	&dev_attr_touchkey_threshold.attr,
	&dev_attr_touchkey_back.attr,
	&dev_attr_touchkey_home.attr,
	&dev_attr_touchkey_recent.attr,
	&dev_attr_touchkey_menu.attr,
	&dev_attr_touchkey_raw_data0.attr,
	&dev_attr_touchkey_raw_data1.attr,
	&dev_attr_touchkey_raw_data2.attr,
	&dev_attr_touchkey_raw_data3.attr,
	NULL,
};
static struct attribute_group touchkey_attr_group = {
	.attrs = touchkey_attributes,
};

static int factory_init_tk(struct mms_ts_info *info)
{
	struct i2c_client *client = info->client;
	int ret;

	info->dev_tk = device_create(sec_class, NULL, (dev_t)NULL, info,
								"sec_touchkey");
	if (IS_ERR(info->dev_tk)) {
		dev_err(&client->dev, "Failed to create fac touchkey dev\n");
		ret = -ENODEV;
		info->dev_tk = NULL;
		goto err_create_dev_tk;
	}

	ret = sysfs_create_group(&info->dev_tk->kobj, &touchkey_attr_group);
	if (ret) {
		dev_err(&client->dev, "Failed to create sysfs (touchkey_attr_group).\n");
		ret = (ret > 0) ? -ret : ret;
		goto err_create_tk_sysfs;
	}

	info->key_pressed = kzalloc(sizeof(bool) * ARRAY_SIZE(info->keycode),
								GFP_KERNEL);
	if (!info->key_pressed) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		ret = -ENOMEM;
		goto err_alloc;
	}

	return 0;

err_alloc:
	sysfs_remove_group(&info->dev_tk->kobj, &touchkey_attr_group);
err_create_tk_sysfs:
err_create_dev_tk:
	return ret;
}
#endif


static DEVICE_ATTR(close_tsp_test, S_IRUGO, show_close_tsp_test, NULL);
static DEVICE_ATTR(cmd, S_IWUSR | S_IWGRP, NULL, store_cmd);
static DEVICE_ATTR(cmd_status, S_IRUGO, show_cmd_status, NULL);
static DEVICE_ATTR(cmd_result, S_IRUGO, show_cmd_result, NULL);
#ifdef ESD_DEBUG
static DEVICE_ATTR(intensity_logging_on, S_IRUGO, show_intensity_logging_on,
		NULL);
static DEVICE_ATTR(intensity_logging_off, S_IRUGO, show_intensity_logging_off,
		NULL);
#endif

static struct attribute *sec_touch_facotry_attributes[] = {
		&dev_attr_close_tsp_test.attr,
		&dev_attr_cmd.attr,
		&dev_attr_cmd_status.attr,
		&dev_attr_cmd_result.attr,
#ifdef ESD_DEBUG
		&dev_attr_intensity_logging_on.attr,
		&dev_attr_intensity_logging_off.attr,
#endif
		NULL,
};

static struct attribute_group sec_touch_factory_attr_group = {
	.attrs = sec_touch_facotry_attributes,
};
#endif /* SEC_TSP_FACTORY_TEST */


struct class *sec_class;

static int __init mms_ts_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct mms_ts_info *info;
	struct input_dev *input_dev;
	int ret = 0;
	int i;
	int boot_v, core_v, config_v, fw_v;
#ifdef SEC_TSP_FACTORY_TEST
	struct device *fac_dev_ts;
#endif
	touch_is_pressed = 0;

	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C))
		return -EIO;

	info = kzalloc(sizeof(struct mms_ts_info), GFP_KERNEL);
	if (!info) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		ret = -ENOMEM;
		goto err_alloc;
	}

	input_dev = input_allocate_device();
	if (!input_dev) {
		dev_err(&client->dev, "Failed to allocate memory for input device\n");
		ret = -ENOMEM;
		goto err_input_alloc;
	}

	info->client = client;
	info->input_dev = input_dev;
	info->pdata = client->dev.platform_data;
	info->irq = -1;
	mutex_init(&info->lock);
	if (info->pdata) {
		info->max_x = info->pdata->max_x;
		info->max_y = info->pdata->max_y;
		info->invert_x = info->pdata->invert_x;
		info->invert_y = info->pdata->invert_y;
	} else {
		info->max_x = 720;
		info->max_y = 1280;
	}

	client->irq = gpio_to_irq(info->pdata->gpio_int);

	i2c_set_clientdata(client, info);
/*
	ret = get_hw_version(info);
	if (ret < 0 && ret != 0xff) {
		dev_err(&client->dev,
			"failed to i2c transfer..no device[%d]\n", ret);
		goto err_i2c_transfer;
	}
*/
	info->register_cb = info->pdata->register_cb;
	info->callbacks.inform_charger = melfas_ta_cb;
	if (info->register_cb)
		info->register_cb(&info->callbacks);

	info->use_surface_touch = info->pdata->use_surface_touch;

	input_mt_init_slots(input_dev, MAX_FINGERS, 0);

	snprintf(info->phys, sizeof(info->phys),
		 "%s/input0", dev_name(&client->dev));
	input_dev->name = "sec_touchscreen"; /*= "Melfas MMSxxx Touchscreen";*/
	input_dev->phys = info->phys;
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &client->dev;
	__set_bit(EV_ABS, input_dev->evbit);
	__set_bit(INPUT_PROP_DIRECT, input_dev->propbit);
	printk("mms_ts_probe, max_x %d, max_y %d\n", info->max_x, info->max_y);
	input_set_abs_params(input_dev, ABS_MT_WIDTH_MAJOR, 0, MAX_WIDTH, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X,
				0, info->max_x, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y,
				0, info->max_y, 0, 0);
	if (info->use_surface_touch) {
		input_set_abs_params(info->input_dev, ABS_MT_TOUCH_MAJOR,
					0, MAX_PRESSURE, 0, 0);
		input_set_abs_params(info->input_dev, ABS_MT_TOUCH_MINOR,
					0, MAX_PRESSURE, 0, 0);

#if 0 /* SPRD's test code */
		input_set_abs_params(input_dev, ABS_MT_ANGLE,
					MIN_ANGLE, MAX_ANGLE, 0, 0);
		input_set_abs_params(input_dev, ABS_MT_PALM,
					0, 1, 0, 0);
#endif
	}

	if (info->pdata->use_touchkey) {
		dev_info(&client->dev, "Melfas ts use touchkey\n");
		info->use_touchkey = info->pdata->use_touchkey;
		memcpy(info->keycode, info->pdata->touchkey_keycode,
			sizeof(info->pdata->touchkey_keycode));
		__set_bit(EV_KEY, input_dev->evbit);
		__set_bit(EV_LED, input_dev->evbit);
		__set_bit(LED_MISC, input_dev->ledbit);
		for (i = 0; i < ARRAY_SIZE(info->keycode); i++)
			set_bit(info->keycode[i], input_dev->keybit);
	}

	input_set_drvdata(input_dev, info);

	ret = input_register_device(input_dev);
	if (ret) {
		dev_err(&client->dev,
			"Failed to register input dev (%d)\n", ret);
		goto err_reg_input_dev;
	}

#ifdef CONFIG_SEC_DVFS
#if TOUCH_BOOSTER
	mutex_init(&info->dvfs_lock);
	INIT_DELAYED_WORK(&info->work_dvfs_off, set_dvfs_off);
	info->dvfs_lock_status = false;
#endif
#endif
	/* Turn on touch screen power  */
	info->pdata->vdd_on(1);

	/* Key led power */
	info->pdata->tkey_led_vdd_on(1);

#if ISC_DL_MODE
	info->fw_core_ver = get_fw_version(info, SEC_CORE);
	dev_info(&client->dev, "core version : 0x%02x\n", info->fw_core_ver);

	if (info->fw_core_ver != 0x53) {
		dev_err(&client->dev, "core version must be 0x53\n");
		dev_info(&client->dev, "excute core firmware update\n");
		ret = isp_fw_update(info);
		if (ret) {
			dev_err(&client->dev,
				"Failed to initialize (%d)\n", ret);
			goto err_reg_input_dev;
		}
		info->fw_core_ver = get_fw_version(info, SEC_CORE);
	}

	info->fw_ic_ver = get_fw_version(info, SEC_CONFIG);
	if ((info->fw_ic_ver < FW_VERSION) || (info->fw_ic_ver == 0xff)) {
		dev_info(&client->dev, "firmware update\n");
		dev_info(&client->dev, "ic:0x%x, bin:0x%x\n",
				info->fw_ic_ver, FW_VERSION);
		ret = isc_fw_update(info);
		if (ret) {
			dev_err(&client->dev, "retry isp fw update\n");
			ret = isp_fw_update(info);
			if (ret) {
				dev_err(&client->dev,
					"Failed to initialize (%d)\n", ret);
				goto err_reg_input_dev;
			}
		}
	}
#else
	info->fw_ic_ver = get_fw_version(info, SEC_CONFIG);
	if (info->fw_ic_ver == FW_VERSION)
		dev_info(&client->dev, "fw update does not need(1)\n");
/* SPRD remove this part  */
/*
	else if (machine_is_jasper2())
		ret = isp_fw_update(info);
*/
	else
		dev_info(&client->dev, "fw update does not need(2)\n");

#endif
	info->palm_status = 0;
	info->enabled = true;

	mms_ts_finish_config(info);

#ifdef CONFIG_HAS_EARLYSUSPEND
	info->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN - 1;
	info->early_suspend.suspend = mms_ts_early_suspend;
	info->early_suspend.resume = mms_ts_late_resume;
	register_early_suspend(&info->early_suspend);
#endif

	boot_v = i2c_smbus_read_byte_data(info->client, 0xE1);
	core_v = i2c_smbus_read_byte_data(info->client, 0xE2);
	config_v = i2c_smbus_read_byte_data(info->client, 0xE3);
	fw_v = i2c_smbus_read_byte_data(info->client, 0xF3);

	dev_info(&client->dev, "Boot:0x%02x, CORE:0x%02x, CONFIG:0x%02x, %d\n",
		boot_v, core_v, config_v, fw_v);

#ifdef SEC_TSP_FACTORY_TEST
	INIT_LIST_HEAD(&info->cmd_list_head);
	for (i = 0; i < ARRAY_SIZE(tsp_cmds); i++)
		list_add_tail(&tsp_cmds[i].list, &info->cmd_list_head);

	mutex_init(&info->cmd_lock);
	info->cmd_is_running = false;

        sec_class = class_create(THIS_MODULE, "sec");
        if (IS_ERR(sec_class)) {
                pr_err("Failed to create class(sec)!\n");
                return PTR_ERR(sec_class);
        }

	fac_dev_ts = device_create(sec_class,
			NULL, 0, info, "tsp");
	if (IS_ERR(fac_dev_ts))
		dev_err(&client->dev, "Failed to create device for the sysfs\n");

	ret = sysfs_create_group(&fac_dev_ts->kobj,
			       &sec_touch_factory_attr_group);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to create sysfs group\n");
		goto err_sysfs_create_group_touch;
	}
#endif

#ifdef SEC_TKEY_FACTORY_TEST
	ret = factory_init_tk(info);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to init factory init (tk)\n");
		goto err_factory_init;
	}
#endif

#ifdef CONFIG_LEDS_CLASS
	if (info->pdata->use_touchkey) {
		info->leds.name = TOUCHKEY_BACKLIGHT;
		info->leds.brightness = LED_FULL;
		info->leds.max_brightness = LED_FULL;
		info->leds.brightness_set = msm_tkey_led_set;

		ret = led_classdev_register(&client->dev, &info->leds);
		if (ret) {
			dev_err(&client->dev,
				"Failed to register led(%d)\n", ret);
			goto fail_led_reg;
		}
	}
#endif

	return 0;
#ifdef CONFIG_LEDS_CLASS
fail_led_reg:
	led_classdev_unregister(&info->leds);
#endif
#ifdef SEC_TSP_FACTORY_TEST
err_sysfs_create_group_touch:
	sysfs_remove_group(&fac_dev_ts->kobj,
				&sec_touch_factory_attr_group);
#endif
#ifdef SEC_TKEY_FACTORY_TEST
err_factory_init:
	free_irq(info->irq, info);
#endif
err_reg_input_dev:
	input_free_device(input_dev);
//err_i2c_transfer:
err_input_alloc:
	kfree(info->fw_name);
	kfree(info);
err_alloc:
	/* turn off power  */
	info->pdata->vdd_on(0);
	/* Key led power */
	info->pdata->tkey_led_vdd_on(0);
	return ret;
}

static int __exit mms_ts_remove(struct i2c_client *client)
{
	struct mms_ts_info *info = i2c_get_clientdata(client);

	led_classdev_unregister(&info->leds);
	if (info->irq >= 0)
		free_irq(info->irq, info);
	input_unregister_device(info->input_dev);
#ifdef SEC_TKEY_FACTORY_TEST
	sysfs_remove_group(&info->dev_tk->kobj, &touchkey_attr_group);
	kfree(info->key_pressed);
#endif
	kfree(info->fw_name);
	kfree(info);

	return 0;
}

#if defined(CONFIG_PM) || defined(CONFIG_HAS_EARLYSUSPEND)
static int mms_ts_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mms_ts_info *info = i2c_get_clientdata(client);

	if (!info->enabled)
		return 0;

	pr_notice("%s: users=%d\n", __func__, info->input_dev->users);
	mutex_lock(&info->input_dev->mutex);
	if (!info->input_dev->users)
		goto out;
#ifdef CONFIG_LEDS_CLASS
	if (info->pdata->use_touchkey) {
		info->tkey_led_reserved = true;
		info->pdata->tkey_led_vdd_on(0);
	}
#endif
	disable_irq(info->irq);
	info->enabled = false;
	touch_is_pressed = 0;
	release_all_fingers(info);
	info->pdata->vdd_on(0);
	msleep(50);

out:
	mutex_unlock(&info->input_dev->mutex);
	return 0;
}

static int mms_ts_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mms_ts_info *info = i2c_get_clientdata(client);

	if (info->enabled)
		return 0;

	pr_notice("%s: users=%d\n", __func__, info->input_dev->users);

	info->pdata->vdd_on(1);
	msleep(120);
#ifdef CONFIG_LEDS_CLASS
	if (info->pdata->use_touchkey && info->tkey_led_reserved) {
		info->tkey_led_reserved = false;
		info->pdata->tkey_led_vdd_on(1);
	}
#endif

	if (info->ta_status) {
		dev_notice(&client->dev, "TA connect!!!\n");
		i2c_smbus_write_byte_data(info->client, 0x33, 0x1);
	} else {
		dev_notice(&client->dev, "TA disconnect!!!\n");
		i2c_smbus_write_byte_data(info->client, 0x33, 0x2);
	}

	mutex_lock(&info->input_dev->mutex);
	info->enabled = true;
	mms_set_noise_mode(info);
	enable_irq(info->irq);
	mutex_unlock(&info->input_dev->mutex);

	return 0;
}
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static void mms_ts_early_suspend(struct early_suspend *h)
{
	struct mms_ts_info *info;
	info = container_of(h, struct mms_ts_info, early_suspend);
	mms_ts_suspend(&info->client->dev);
}

static void mms_ts_late_resume(struct early_suspend *h)
{
	struct mms_ts_info *info;
	info = container_of(h, struct mms_ts_info, early_suspend);
	mms_ts_resume(&info->client->dev);
}
#endif

#if defined(CONFIG_PM) && !defined(CONFIG_HAS_EARLYSUSPEND)
static const struct dev_pm_ops mms_ts_pm_ops = {
	.suspend	= mms_ts_suspend,
	.resume		= mms_ts_resume,
};
#endif

static const struct i2c_device_id mms_ts_id[] = {
	{ "mms_ts", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, mms_ts_id);

static struct i2c_driver mms_ts_driver = {
	.probe		= mms_ts_probe,
	.remove		= mms_ts_remove,
	.driver = {
		.name = "mms_ts",
#if defined(CONFIG_PM) && !defined(CONFIG_HAS_EARLYSUSPEND)
		.pm	= &mms_ts_pm_ops,
#endif
	},
	.id_table	= mms_ts_id,
};

static int __init mms_ts_init(void)
{
#ifdef CONFIG_SAMSUNG_LPM_MODE
	if (poweroff_charging) {
		pr_notice("%s : LPM Charging Mode!!\n", __func__);
		return 0;
	}
#endif

	return i2c_add_driver(&mms_ts_driver);
}

static void __exit mms_ts_exit(void)
{
	i2c_del_driver(&mms_ts_driver);
}

module_init(mms_ts_init);
module_exit(mms_ts_exit);

/* Module information */
MODULE_DESCRIPTION("Touchscreen driver for Melfas MMS-series controllers");
MODULE_LICENSE("GPL");
