/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 */
#ifndef ABOV_SAR_H
#define ABOV_SAR_H

#include <linux/device.h>
#include <linux/slab.h>
#include <linux/interrupt.h>

#define KEY_CAP_CH0              0x270
#define KEY_CAP_CH1              0x271
#define KEY_CAP_CH2              0x272

/*
 *  I2C Registers
 */
#define ABOV_VERSION_REG		    0x01
#define ABOV_MODELNO_REG		    0x02
#define ABOV_VENDOR_ID_REG		    0x03
#define ABOV_IRQSTAT_REG			0x04
#define ABOV_SOFTRESET_REG  		0x06
#define ABOV_CTRL_MODE_REG			0x07
#define ABOV_CTRL_CHANNEL_REG		0x08
#define ABOV_RECALI_REG				0xFB

#define ABOV_DIFF_CH0_MSB           0x0A
#define ABOV_DIFF_CH0_LSB           0x0B
#define ABOV_DIFF_CH1_MSB           0x0C
#define ABOV_DIFF_CH1_LSB           0x0D
#define ABOV_DIFF_CH2_MSB           0x0E
#define ABOV_DIFF_CH2_LSB           0x0F
#define ABOV_CAP_CH0_MSB            0x18
#define ABOV_CAP_CH0_LSB            0x19
#define ABOV_CAP_CH1_MSB            0x1A
#define ABOV_CAP_CH1_LSB            0x1B
#define ABOV_CAP_CH2_MSB            0x1C
#define ABOV_CAP_CH2_LSB            0x1D
#define ABOV_CAP_REF0_MSB           0x1E
#define ABOV_CAP_REF0_LSB           0x1F
#define ABOV_CAP_REF1_MSB           0x20
#define ABOV_CAP_REF1_LSB           0x21
#define ABOV_CAP_REF2_MSB           0x22
#define ABOV_CAP_REF2_LSB           0x23
#define ABOV_CAP_OFFSET_MSB         0x24
#define ABOV_CAP_OFSSET_LSB         0x25

#define ABOV_CAP_BL0_MSB            0x34
#define ABOV_CAP_BL0_LSB            0x35
#define ABOV_CAP_BL1_MSB            0x36
#define ABOV_CAP_BL1_LSB            0x37
#define ABOV_CAP_BL2_MSB            0x38
#define ABOV_CAP_BL2_LSB            0x39
#define ABOV_CAP_BL3_MSB            0x3A
#define ABOV_CAP_BL3_LSB            0x3B
#define ABOV_CAP_BL4_MSP            0x3C
#define ABOV_CAP_BL4_LSB            0x3D
#define ABOV_CAP_BL5_MSB            0x3E
#define ABOV_CAP_BL5_LSB            0x3F
#define ABOV_CAP_BL6_MSB            0x40
#define ABOV_CAP_BL6_LSB            0x41

#define ABOV_CTRL_MODE_ACTIVE		0x00
#define ABOV_CTRL_MODE_SLEEP		0x01
#define ABOV_CTRL_MODE_STOP			0x02

/* enable body stat */
#define ABOV_TCHCMPSTAT_TCHSTAT0_FLAG   0x03
/* enable body stat */
#define ABOV_TCHCMPSTAT_TCHSTAT1_FLAG   0x0c
/* enable body stat */
#define ABOV_TCHCMPSTAT_TCHSTAT2_FLAG   0x30

/* cap_button_mask */
#define ABOV_CAP_BUTTON_MASK    0x15

/* Flash erase time */
#define ABOV_FW_FLASH_ERASE_TIME   3000

/* bootloader device id */
#define ABOV_BOOT_DEVICE_ID   0x70

/* input report value */
/* Define it acrroding customer require*/
/* release or far from state */
#define CAP_STATE_FAR      5
/* first detect or near state */
#define CAP_STATE_NEAR     0
/* second detect or body state */
#define CAP_STATE_BODY     1
/* default state while disable sar sensor */
#define CAP_STATE_NONE    -1

enum boot_mode {
	NORMAL_MODE,
	BOOTLOADER_MODE,
	UNKONOW_MODE,
};

/**************************************
* define platform data
*
**************************************/
struct smtc_reg_data {
	unsigned char reg;
	unsigned char val;
};

typedef struct smtc_reg_data smtc_reg_data_t;
typedef struct smtc_reg_data *psmtc_reg_data_t;

struct _buttonInfo {
	/* The Key to send to the input */
	int keycode;
	/* Mask to look for on Touch Status */
	int mask;
	/* Current state of button. */
	int state;
};

struct _totalButtonInformation {
	struct _buttonInfo *buttons;
	int buttonSize;
	struct input_dev *input_ch0;
	struct input_dev *input_ch1;
	struct input_dev *input_ch2;
};

typedef struct _totalButtonInformation buttonInformation_t;
typedef struct _totalButtonInformation *pbuttonInformation_t;

/* Define Registers that need to be initialized to values different than
 * default
 */
static struct smtc_reg_data abov_i2c_reg_setup[] = {
	{
		.reg = ABOV_CTRL_MODE_REG,
		.val = 0x00,
	},
	{
		.reg = ABOV_CTRL_CHANNEL_REG,
		.val = 0x3F,
	},
	{
		.reg = ABOV_RECALI_REG,
		.val = 0x01,
	},
};

static struct _buttonInfo psmtcButtons[] = {
	{
		.keycode = KEY_CAP_CH0,
		.mask = ABOV_TCHCMPSTAT_TCHSTAT0_FLAG,
	},
	{
		.keycode = KEY_CAP_CH1,
		.mask = ABOV_TCHCMPSTAT_TCHSTAT1_FLAG,
	},
	{
		.keycode = KEY_CAP_CH2,
		.mask = ABOV_TCHCMPSTAT_TCHSTAT2_FLAG,
	},
};

struct abov_platform_data {
	int i2c_reg_num;
	struct smtc_reg_data *pi2c_reg;
	struct regulator *cap_vdd;
	struct regulator *cap_svdd;
	bool cap_vdd_en;
	bool cap_svdd_en;
	unsigned irq_gpio;
	/* used for custom setting for channel and scan period */
	int cap_channel_ch0;
	int cap_channel_ch1;
	int cap_channel_ch2;

	const char *cap_ch0_name;
	const char *cap_ch1_name;
	const char *cap_ch2_name;
	pbuttonInformation_t pbuttonInformation;
	const char *fw_name;

	int (*get_is_nirq_low)(unsigned irq_gpio);
	int (*init_platform_hw)(void);
	void (*exit_platform_hw)(void);
};
typedef struct abov_platform_data abov_platform_data_t;
typedef struct abov_platform_data *pabov_platform_data_t;

#ifdef USE_SENSORS_CLASS
static struct sensors_classdev sensors_capsensor_ch0_cdev = {
	.name = "A96T3X6",
	.vendor = "abov",
	.sensor_name = "grip_sensor",
	.version = 1,
	.type = SENSOR_TYPE_SAR_MIAN,
	.max_range = "5",
	.resolution = "5.0",
	.sensor_power = "3",
	.min_delay = 0, /* in microseconds */
	.fifo_reserved_event_count = 0,
	.fifo_max_event_count = 0,
	.enabled = 0,
	.delay_msec = 100,
	.sensors_enable = NULL,
	.sensors_poll_delay = NULL,
};
static struct sensors_classdev sensors_capsensor_ch1_cdev = {
	.name = "A96T3X6",
	.vendor = "abov",
       .sensor_name = "grip_sensor_wifi",
	.version = 1,
	.type = SENSOR_TYPE_SAR_WCN,
	.max_range = "5",
	.resolution = "5.0",
	.sensor_power = "3",
	.min_delay = 0, /* in microseconds */
	.fifo_reserved_event_count = 0,
	.fifo_max_event_count = 0,
	.enabled = 0,
	.delay_msec = 100,
	.sensors_enable = NULL,
	.sensors_poll_delay = NULL,
};
#ifndef CONFIG_INPUT_ABOV_TWO_CHANNEL
static struct sensors_classdev sensors_capsensor_ch2_cdev = {
	.name = "A96T3X6",
	.vendor = "abov",
       .sensor_name = "grip_sensor_sub",
	.version = 1,
	.type = SENSOR_TYPE_SAR_SUB,
	.max_range = "5",
	.resolution = "5.0",
	.sensor_power = "3",
	.min_delay = 0, /* in microseconds */
	.fifo_reserved_event_count = 0,
	.fifo_max_event_count = 0,
	.enabled = 0,
	.delay_msec = 100,
	.sensors_enable = NULL,
	.sensors_poll_delay = NULL,
};
#endif
#endif

#ifdef USE_SENSORS_CLASS
typedef enum{
	CAPSENSOR_ENABLE_FLAG_CH0=1,
	CAPSENSOR_ENABLE_FLAG_CH1=2,
	CAPSENSOR_ENABLE_FLAG_CH2=4,
	CAPSENSOR_ENABLE_FLAG_MAX
}capsensor_enable_flag_t;
#endif

/***************************************
* define data struct/interrupt
* @pdev: pdev common device struction for linux
* @dworker: work struct for worker function
* @board: constant pointer to platform data
* @mutex: mutex for interrupt process
* @lock: Spin Lock used for nirq worker function
* @bus: either i2c_client or spi_client
* @pDevice: device specific struct pointer
* @read_flag : used for dump specified register
* @irq: irq number used
* @irqTimeout: msecs only set if useIrqTimer is true
* @irq_disabled: whether irq should be ignored
* @irq_gpio: irq gpio number
* @useIrqTimer: older models need irq timer for pen up cases
* @read_reg: record reg address which want to read
* @init: (re)initialize device
* @get_nirq_low: get whether nirq is low (platform data)
* @statusFunc: array of functions to call for corresponding status bit
***************************************/
#define USE_THREADED_IRQ

typedef struct abovXX abovXX_t, *pabovXX_t;
struct abovXX {
	struct device *pdev;
	struct delayed_work dworker;
	struct abov_platform_data *board;
#if defined(USE_THREADED_IRQ)
	struct mutex mutex;
#else
	spinlock_t	lock;
#endif
	void *bus;
	void *pDevice;
	int read_flag;
	int irq;
	int irqTimeout;
	/* interrupt check flag */
	int int_state;
	char irq_disabled;
	/* whether irq should be ignored.. cases if enable/disable irq is not used
	 * or does not work properly */
	u8 useIrqTimer;
	u8 read_reg;

#ifdef USE_SENSORS_CLASS
	// sensors enable flag
	capsensor_enable_flag_t enable_flag;
#endif

#ifdef USE_USB_CHANGE_RECAL
	struct work_struct ps_notify_work;
	struct notifier_block ps_notif;
	bool ps_is_present;
#endif

    bool abov_first_boot;
	int interrupt_count;
	u16 ch0_backgrand_cap;
	u16 ch1_backgrand_cap;
	u16 ch2_backgrand_cap;
    int user_test;

#if POWER_ENABLE
    int power_state;
#endif

	bool loading_fw;
	struct work_struct fw_update_work;

	/* Function Pointers */
	int (*init)(pabovXX_t this);
	/* since we are trying to avoid knowing registers, create a pointer to a
	 * common read register which would be to read what the interrupt source
	 * is from
	 */
	int (*get_nirq_low)(unsigned irq_gpio);
	void (*statusFunc)(pabovXX_t this);
	struct pinctrl *sar_pinctl;
	struct pinctrl_state *pinctrl_state_default;
       char channel_status; //bug 492320,20191119,gaojingxuan.wt,add,use SS hal
       bool first_cali_flag;
#if defined(CONFIG_SENSORS)
	bool skip_data;
#endif
};

int abovXX_sar_init(pabovXX_t this);
int abovXX_sar_remove(pabovXX_t this);

#endif
