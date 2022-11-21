
#ifndef _LINUX_WACOM_I2C_H
#define _LINUX_WACOM_I2C_H

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/hrtimer.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/wakelock.h>
#include <linux/uaccess.h>
#include <linux/of_gpio.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#ifdef CONFIG_INPUT_BOOSTER
#include <linux/input/input_booster.h>
#endif
#include <linux/regulator/consumer.h>

/*
 * Wacom Interrupt Name
 */
#define WACOM_DEVICE_NAME		"wacom_i2c"
#define WACOM_INTERRUPT_NAME		"wacom_irq"
#define WACOM_PDCT_NAME		"wacom_pdct"
#define WACOM_PEN_INSERT_NAME	"wacom_pen_insert"

/*
 * Support feature for THIS Module
 */
#define WACOM_PDCT_WORK_AROUND
#define WACOM_PEN_DETECT
#define WACOM_HAVE_FWE_PIN
#define WACOM_USE_SOFTKEY
#define COOR_WORK_AROUND
#define WACOM_UMS_UPDATE
#define WACOM_USE_GAIN
#define BATTERY_SAVING_MODE
#define WACOM_CONNECTION_CHECK

#ifdef CONFIG_SEC_TBLTE_PROJECT
#define WACOM_MAINSCREEN_DISABLE
#endif

#ifdef BATTERY_SAVING_MODE
#ifndef WACOM_PEN_DETECT
#define WACOM_PEN_DETECT
#endif
#endif

/* wacom region : touch region, touchkey region,
 * HW team request : block touchkey event between touch release ~ 100msec.
 */
#define USE_WACOM_BLOCK_KEYEVENT

#ifdef CONFIG_WACOM_LCD_FREQ_COMPENSATE
/* NOISE from LDI. read Vsync at wacom firmware. */
#define USE_WACOM_LCD_WORKAROUND
#endif

/* Report Tilt X, Tilt Y for calculate degree and orientation
 * Report  Hover Height for using SPEN SDK
 */
#define USE_WACOM_TILT_HEIGH

/* for reduce query waiting time.
 * before : send QUERY command, delay 100 msec, receive QUERY data(14 byte).
 * after : receive 24 or 26 byte data, upper 12~14 data is garbage or coordinate data,
 *           lower 14 byte is QUERY data.
 */
#undef USE_REDUCE_QUERY_TIME

/* Wacom Firmware Update Path, Firmware name */
#define WACOM_MAX_FW_PATH		64
#define WACOM_FW_NAME_NONE		NULL
#define WACOM_UMS_FW_PATH		"/sdcard/firmware/wacom_firm.bin"

/* { 0x1F, 0xAA, 0xBB, 0xCC, 0xDD} : 0x1F is fixed value. */
#ifdef CONFIG_SEC_TRLTE_PROJECT
#if defined(CONFIG_MACH_TRLTE_ATT) || defined(CONFIG_MACH_TRLTE_TMO)
#define WACOM_FW_NAME_W9012		"epen/W9012_tr.bin"
#define WACOM_FW_CHECKSUM		{ 0x1F, 0xEE, 0x3C, 0x80, 0x41, } /* ver 0x0237 */
#else
#define WACOM_FW_NAME_W9012		"epen/W9012_tr_239.bin"
#define WACOM_FW_CHECKSUM		{ 0x1F, 0x76, 0x4E, 0x9B, 0x88, } /* ver 0x0239 */
#endif
#undef USE_REDUCE_QUERY_TIME

#else // CONFIG_SEC_TBLTE_PROJECT
#define WACOM_FW_NAME_W9012		"epen/W9012_tb.bin"
#define WACOM_FW_CHECKSUM		{ 0x1F, 0x0C, 0xF6, 0x6C, 0x4A, } /* ver 0x024B */
#undef USE_REDUCE_QUERY_TIME
#endif

/* Wacom Command */
#define COM_READ_ALL_DATA_LENGTH	28
#define NUM_OF_UNNECESSARY_DATA		14

#define COM_COORD_NUM			12
#define COM_QUERY_NUM			14
#define COM_SAMPLERATE_STOP		0x30
#define COM_SAMPLERATE_40		0x33
#define COM_SAMPLERATE_80		0x32
#define COM_SAMPLERATE_133		0x31
#define COM_SURVEYSCAN			0x2B
#define COM_QUERY			0x2A
#define COM_FLASH			0xff
#define COM_CHECKSUM			0x63

/* Wacom coord information size in device tree */
#define WACOM_COORDS_ARR_SIZE	9

/* Wacom I2C address for digitizer and its boot loader */
#define WACOM_I2C_ADDR			0x56
#define WACOM_I2C_BOOT			0x09
#define WACOM_I2C_9001_BOOT		0x57
#define WACOM_I2C_STOP			0x30
#define WACOM_I2C_START			0x31
#define WACOM_I2C_GRID_CHECK		0xC9
#define WACOM_STATUS			0xD8

/* Wacom PDCT Signal */
#define PDCT_NOSIGNAL			1
#define PDCT_DETECT_PEN			0

/* Wacom origin offset :  This will be removed. Firmware supported */
#define EPEN_B660_ORG_X			456
#define EPEN_B660_ORG_Y			504
#define EPEN_B713_ORG_X			676
#define EPEN_B713_ORG_Y			724

/*HWID to distinguish Detect Switch*/
#define WACOM_DETECT_SWITCH_HWID	0xFFFF

/*HWID to distinguish FWE1*/
#define WACOM_FWE1_HWID			0xFFFF

/*HWID to distinguish B911 Digitizer*/
#define WACOM_DTYPE_B911_HWID		1

/* Sampling Rate / Report rate */
#define SAMPLING_RATE_120HZ		0x31
#define SAMPLING_RATE_80HZ		0x32
#define SAMPLING_RATE_40HZ		0x33
#define SAMPLING_RATE_NOT_DEFINED	0

/* Wacom data read from QUERY */
struct wacom_query_data {
	int x_max;
	int y_max;
	int pressure_max;
	char comstat;
	u8 data[COM_COORD_NUM];
	unsigned int fw_version_ic;
	unsigned int fw_version_bin;
	int firm_update_status;
	int tiltx_max;
	int tilty_max;
	int height_max;
	unsigned char mpu_type;
	unsigned char bootloader_ver;
};

/* Wacom data read from DEVICETREE */
struct wacom_devicetree_data {
/* using dts feature */
	const char *basic_model;
	bool i2c_pull_up;
	int gpio_int;
	u32 irq_gpio_flags;
	int gpio_sda;
	u32 sda_gpio_flags;
	int gpio_scl;
	u32 scl_gpio_flags;
	int vdd_en;
	int gpio_pen_reset_n;
	u32 pen_reset_n_gpio_flags;
	int gpio_pen_fwe1;
	u32 pen_fwe1_gpio_flags;
	int gpio_pen_pdct;
	u32 pen_pdct_gpio_flags;

	int x_invert;
	int y_invert;
	int xy_switch;
	int min_x;
	int max_x;
	int min_y;
	int max_y;
	int max_pressure;
	int min_pressure;
	int gpio_pendct;
	u32 ic_mpu_ver;
	u32 irq_flags;
	u32 wacom_firmup_flag;

/* using dts feature */
#ifdef WACOM_PEN_DETECT
	int gpio_pen_insert;
#endif

};

/*Parameters for i2c driver*/
struct wacom_i2c {
	struct i2c_client *client;
	struct i2c_client *client_boot;
	struct input_dev *input_dev;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
	struct mutex lock;
	struct mutex irq_lock;
#ifdef WACOM_BOOSTER
	struct input_booster *wacom_booster;
#endif
	unsigned char * fw_data;
	bool ums_binary;

	struct device	*dev;
	int irq;
#ifdef WACOM_PDCT_WORK_AROUND
	int irq_pdct;
#endif
	int pen_pdct;
	int gpio;
	int irq_flag;
	int pen_prox;
	int pen_pressed;
	int side_pressed;
	int tool;
	s16 last_x;
	s16 last_y;

	unsigned char sampling_rate;
#ifdef WACOM_PEN_DETECT
	int irq_pen_insert;
	struct delayed_work pen_insert_dwork;
	bool pen_insert;
	int gpio_pen_insert;
#endif
	int invert_pen_insert;
#ifdef WACOM_HAVE_FWE_PIN
	int gpio_fwe;
	bool have_fwe_pin;
#endif
	bool checksum_result;
	struct wacom_query_data *wac_query_data;
	struct wacom_devicetree_data *wac_dt_data;
	int (*power)(int on);
	struct delayed_work resume_work;

#ifdef BATTERY_SAVING_MODE
	bool battery_saving_mode;
#endif
	bool power_enable;
	bool boot_mode;
	bool query_status;
	int ic_mpu_ver;
	int boot_ver;
	bool init_fail;
#ifdef USE_WACOM_LCD_WORKAROUND
	unsigned int vsync;
	struct delayed_work read_vsync_work;
	struct delayed_work boot_done_work;
	bool wait_done;
	bool boot_done;
	unsigned int delay_time;
#endif
#ifdef USE_WACOM_BLOCK_KEYEVENT
	struct delayed_work	touch_pressed_work;
	unsigned int key_delay_time;
	bool touchkey_skipped;
	bool touch_pressed;
#endif
	bool enabled;

#ifdef WACOM_HAVE_FWE_PIN
	void (*compulsory_flash_mode)(struct wacom_i2c *, bool);
#endif
	int (*reset_platform_hw)(struct wacom_i2c *);
	int (*wacom_start)(struct wacom_i2c *);
	int (*wacom_stop)(struct wacom_i2c *);
	int (*wacom_i2c_send)(struct wacom_i2c *,
			  const char *, int , bool);
	int (*wacom_i2c_recv)(struct wacom_i2c *,
			char *, int, bool);
	int (*wacom_i2c_query)(struct wacom_i2c *);
	void (*wacom_i2c_disable)(struct wacom_i2c *);
	void (*wacom_i2c_enable)(struct wacom_i2c *);
	void (*wacom_enable_irq)(struct wacom_i2c *, bool);

	struct wake_lock wakelock_pen_insert;
	struct wake_lock wakelock;
};

extern struct class *sec_class;
extern unsigned int system_rev;
extern int boot_mode_recovery;
#ifdef CONFIG_SAMSUNG_LPM_MODE
extern int poweroff_charging;
#endif
#ifdef USE_WACOM_LCD_WORKAROUND
extern int ldi_fps(unsigned int input_fps);
#endif

extern int wacom_factory_probe(struct device *dev);
extern void wacom_factory_release(struct device *dev);
extern int get_lcd_attached(void);
extern int wacom_i2c_modecheck(struct wacom_i2c *wac_i2c);
#endif /* _LINUX_WACOM_I2C_H */
