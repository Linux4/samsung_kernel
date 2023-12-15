#ifndef _LINUX_WACOM_H_
#define _LINUX_WACOM_H_

#include <linux/wakelock.h>

#include "wacom_i2c.h"

#ifdef CONFIG_EPEN_WACOM_W9014
#define WACOM_FW_SIZE 131092
#elif defined(CONFIG_EPEN_WACOM_W9019)
#define WACOM_FW_SIZE 131092
#elif defined(CONFIG_EPEN_WACOM_W9012)
#define WACOM_FW_SIZE 131072
#elif defined(CONFIG_EPEN_WACOM_G9PM)
#define WACOM_FW_SIZE 61440
#elif defined(CONFIG_EPEN_WACOM_G9PLL) \
	|| defined(CONFIG_EPEN_WACOM_G10PM)
#define WACOM_FW_SIZE 65535
#else
#define WACOM_FW_SIZE 32768
#endif

/* Wacom Command */
#define COM_COORD_NUM		14
#define COM_RESERVED_NUM	1
#define COM_QUERY_NUM		15
#define COM_QUERY_RETRY		3

#define COM_QUERY		0x2A
#define COM_SURVEYSCAN		0x2B
#define COM_SURVEYEXIT		0x2D

#define COM_SAMPLERATE_STOP	0x30
#define COM_SAMPLERATE_40	0x33
#define COM_SAMPLERATE_80	0x32
#define COM_SAMPLERATE_133	0x31
#define COM_SAMPLERATE_START	COM_SAMPLERATE_133

#define COM_CHECKSUM		0x63

#define COM_NORMAL_SENSE_MODE	0xDB
#define COM_LOW_SENSE_MODE	0xDC
#define COM_REQUEST_SENSE_MODE	0xDD

#define COM_FLASH		0xFF

/* query data format */
#define EPEN_REG_HEADER		0x00
#define EPEN_REG_X1		0x01
#define EPEN_REG_X2		0x02
#define EPEN_REG_Y1		0x03
#define EPEN_REG_Y2		0x04
#define EPEN_REG_PRESSURE1	0x05
#define EPEN_REG_PRESSURE2	0x06
#define EPEN_REG_FWVER1		0X07
#define EPEN_REG_FWVER2		0X08
#define EPEN_REG_MPUVER		0X09
#define EPEN_REG_BLVER		0X0A
#define EPEN_REG_TILT_X		0X0B
#define EPEN_REG_TILT_Y		0X0C
#define EPEN_REG_HEIGHT		0X0D
#define EPEN_REG_FORMATVER	0X0E

/* SURVEY MODE is LPM mode */
#define WACOM_USE_SURVEY_MODE

#define WACOM_PDCT_WORK_AROUND

/* PDCT Signal */
#define PDCT_NOSIGNAL	1
#define PDCT_DETECT_PEN	0

#define WACOM_I2C_MODE_BOOT	true
#define WACOM_I2C_MODE_NORMAL	false

#define EPEN_RESUME_DELAY	180
#define EPEN_OFF_TIME_LIMIT	10000 // usec

/* softkey block workaround */
//#define WACOM_USE_SOFTKEY_BLOCK
#ifdef WACOM_USE_SOFTKEY_BLOCK
#define SOFTKEY_BLOCK_DURATION (HZ / 10)
#endif

/* LCD freq sync */
#ifdef CONFIG_WACOM_LCD_FREQ_COMPENSATE
/* NOISE from LDI. read Vsync at wacom firmware. */
#define LCD_FREQ_SYNC
#endif

#ifdef LCD_FREQ_SYNC
#define LCD_FREQ_BOTTOM 60100
#define LCD_FREQ_TOP 60500
#endif

#define LCD_FREQ_SUPPORT_HWID 8

#define FW_UPDATE_RUNNING	1
#define FW_UPDATE_PASS		2
#define FW_UPDATE_FAIL		-1

struct wacom_i2c;

/*Parameters for wacom own features*/
struct wacom_features {
	char comstat;
	unsigned int fw_version;
	int update_status;
};

#define WACOM_FW_PATH "epen/W9012_T.bin"

enum {
	FW_NONE = 0,
	FW_BUILT_IN,
	FW_HEADER,
	FW_IN_SDCARD,
	FW_EX_SDCARD,
};

enum {
	POWER_STATE_OFF = 0,
	POWER_STATE_LPM_SUSPEND,
	POWER_STATE_LPM_RESUME,
	POWER_STATE_ON,
};

/* header ver 1 */
struct fw_image {
	u8 hdr_ver;
	u8 hdr_len;
	u16 fw_ver1;
	u16 fw_ver2;
	u16 fw_ver3;
	u32 fw_len;
	u8 checksum[5];
	u8 alignment_dummy[3];
	u8 data[0];
} __attribute__ ((packed));

#ifdef WACOM_USE_SURVEY_MODE
/*
 * struct epen_pos - for using to send coordinate in survey mode
 * @id: for extension of function
 *      0 -> not use
 *      1 -> for Screen off Memo
 *      2 -> for oter app or function
 * @x: x coordinate
 * @y: y coordinate
 */
struct epen_pos {
	u8 id;
	int x;
	int y;
};
#endif

struct wacom_i2c {
	struct i2c_client *client;
	struct i2c_client *client_boot;
	struct input_dev *input_dev;
	struct mutex lock;
	struct mutex irq_lock;
	struct wake_lock fw_wakelock;
	struct wake_lock det_wakelock;
	struct device *dev;
	int irq;
	int irq_pdct;
	int pen_pdct;
	int pen_prox;
	int pen_pressed;
	int side_pressed;
	int tool;
	struct delayed_work pen_insert_dwork;
	bool pen_insert;
	bool checksum_result;
	struct wacom_features *wac_feature;
	struct wacom_g5_platform_data *pdata;
	struct delayed_work resume_work;
	struct work_struct reset_work;
	bool reset_on_going;
	bool connection_check;
	int  fail_channel;
	int  min_adc_val;
	bool battery_saving_mode;
	volatile int power_state;
	bool boot_mode;
	bool query_status;
	int wcharging_mode;
#ifdef WACOM_USE_SURVEY_MODE
	bool send_done;
	bool survey_state;
	bool aop_mode;
	bool garbage_irq;
	bool timeout;
	struct wake_lock wakelock;
	struct completion resume_done;
	struct delayed_work irq_ignore_work;
	struct epen_pos survey_pos;
#endif
#ifdef LCD_FREQ_SYNC
	int lcd_freq;
	bool lcd_freq_wait;
	bool use_lcd_freq_sync;
	struct work_struct lcd_freq_work;
	struct delayed_work lcd_freq_done_work;
	struct mutex freq_write_lock;
#endif
#ifdef WACOM_USE_SOFTKEY_BLOCK
	bool block_softkey;
	struct delayed_work softkey_block_work;
#endif
	struct work_struct update_work;
	struct workqueue_struct *fw_wq;
	const struct firmware *firm_data;
	struct fw_image *fw_img;
	u8 *fw_data;
	u32 fw_ver_file;
	char fw_chksum[5];
	u8 fw_update_way;
	bool do_crc_check;
	void (*compulsory_flash_mode)(struct wacom_i2c *, bool);
	int (*reset_platform_hw)(struct wacom_i2c *);
	int (*get_irq_state)(struct wacom_i2c *);
};

#endif /* _LINUX_WACOM_H_ */
