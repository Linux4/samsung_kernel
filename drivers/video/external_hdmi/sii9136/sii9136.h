#ifndef __HDMI_TX_H__
#define __HDMI_TX_H__

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>

#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>

#include <linux/slab.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/wakelock.h>
#include <linux/regulator/consumer.h>
#include <linux/err.h>
#include <linux/async.h>


#define AUX_ERR  1
#define AUX_OK   0


#define LOG_TAG "[SiI9136]"


#define GPIO_HDMI_INT	155
#define GPIO_HDMI_RESET_N	156

#if 1
#define DEV_DBG_ERROR(fmt, args...)         pr_info(KERN_ERR "===== [SiI9136 Exception] [%s][%d] :: "fmt, __func__, __LINE__, ##args);
#define DEV_DBG(fmt, args...) pr_info(KERN_DEBUG "[SiI9136][%s] :: " fmt, __func__, ##args);
#else
#define SII_DEV_DBG_ERROR(fmt, args...)
#define SII_DEV_DBG(fmt, args...)
#endif


#ifndef LATTICE_ORIGINAL
#define REGULATOR_MAXCOUNT 4
struct sii9136_regulator_info {
	char vdd_name[30];
	unsigned int voltage;
};
#endif

struct sii9136_platform_data {
	struct i2c_client *client;
	struct hrtimer timer;
	struct work_struct work;
#ifndef LATTICE_ORIGINAL
	int gpio_hdmi_int;
	int gpio_hdmi_reset;
	struct regulator *hdmi_regulators[REGULATOR_MAXCOUNT];
#endif
};


typedef struct
{
    unsigned short vertical_lines_total;
    unsigned short horizontal_pixels_total;
    unsigned short vertical_lines_active;
    unsigned short horizontal_pixels_active;
    unsigned char  color_depth;
	unsigned char  hdcp_authenticated;
} VideoFormat_t;

#ifndef LATTICE_ORIGINAL
struct i2c_client* get_simgI2C_client(u8 device_id);
#endif
#endif
