
#include <linux/module.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>
#include <linux/regulator/consumer.h>
#include <linux/input/mt.h>
#include <linux/of_gpio.h>
#include <linux/power_supply.h>

#include "../../../sec_input/sec_input.h"
//#include <linux/samsung/sec_class.h>
#include <linux/sec_class.h>
#if IS_ENABLED(CONFIG_HALL_NOTIFIER)
#include <linux/hall/hall_ic_notifier.h>
#endif

#define HAECHITECH_READ_DEVICE_ID	0x00 /* 2 byte */

#define HAECHITECH_REG_WORD_ST             0x10
#define HAECHITECH_REG_WORD_X              0x11
#define HAECHITECH_REG_WORD_Y              0x13
#define HAECHITECH_REG_WORD_Z              0x15

#define HAECHITECH_REG_CNTL1               0x20
#define HAECHITECH_REG_CNTL2               0x21
#define HAECHITECH_REG_CNTL3               0x22
#define HAECHITECH_REG_CNTL4               0x31
#define HAECHITECH_REG_CNTL5               0x47

#define HAECHITECH_REG_THRESHOLD_X                 0x23	/*4 bytes */
#define HAECHITECH_REG_THRESHOLD_Y                 0x27
#define HAECHITECH_REG_THRESHOLD_Z                 0x2B
//#define HAECHITECH_REG_THRESHOLD_V                 0x25
#define HAECHITECH_REG_SRST                0x30

#define HAECHITECH_CNTL3_SELT_TEST_SET           0x80

u8 measurement_mode[7] = { 0x00, 0x02, 0x04, 0x06, 0x08, 0x0A, 0x0C};

struct haechitech_platform_data {
	int gpio_int;
	int gpio_scl;
	int gpio_sda;
	int gpio_rst;

	struct pinctrl *pinctrl;

	struct regulator *dvdd;
	struct regulator *avdd;
	struct regulator *pullup;

	u8 msrNo;

	/* settings */
	u8 measurement_number;
	/* DRDY event */
	u8 drdy_en;
	/* SW event */
	u8 sw_x_en;
	u8 sw_y_en;
	u8 sw_z_en;
	/* ERR event*/
	u8 err_en;
	/* POL setting*/
	u8 pol_x;
	u8 pol_y;
	u8 pol_z;
	/* SDR setting */
	u8 sdr;
	/* SMR setting */
	u8 smr;
	/* THRESHOLD setting*/
	s16 bop_x;
	s16 brp_x;
	s16 bop_y;
	s16 brp_y;
	s16 bop_z;
	s16 brp_z;

};

struct haechitech_info {
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct haechitech_platform_data *pdata;
	struct mutex i2c_lock;
	struct delayed_work dwork;
	struct device *sec_dev;
	struct work_struct probe_work;
	struct workqueue_struct *probe_workqueue;

	u8 mode;

	/* shell command */
	u8 dwork_state;

	u8 state;
	s16 x;
	s16 y;
	s16 z;

	int cal_value[6];
	bool calibrated;
#if IS_ENABLED(CONFIG_HALL_NOTIFIER)
	struct notifier_block hall_ic_nb;
#endif
	struct completion resume_done;
};

static int haechitech_power_control(struct haechitech_info *info, bool on);
static int haechitech_setup_device_control(struct haechitech_info *info);

