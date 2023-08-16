/*
 * This file is part of the CS5032, AP3212C and AP3216C sensor driver.
 * CS5032 is combined proximity and ambient light sensor.
 * AP3216C is combined proximity, ambient light sensor and IRLED.
 *
 * Contact: John Huang <john.huang@dyna-image.com>
 *	    Templeton Tsai <templeton.tsai@dyna-image.com>
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 *
 * Filename: CS5032.c
 *
 * Summary:
 *	CS5032 device driver.
 *
 * Modification History:
 * Date     By       Summary
 * -------- -------- -------------------------------------------------------
 * 15/09/14 Templeton Tsai       1. Init CS5032
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/string.h>
#include <mach/gpio.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/workqueue.h>
#include <linux/timer.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>

#include <linux/sensor/sensors_core.h>
#include "cs5032.h"

///IIO additions...
//
#include <linux/iio/iio.h>
#include <linux/iio/buffer.h>
#include <linux/iio/kfifo_buf.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/events.h>
#include <linux/iio/trigger.h>
#include <linux/iio/trigger_consumer.h>

#define CS5032_DRV_NAME		"CS5032"
#define DRIVER_VERSION		"1"

#define VENDOR_NAME	"LITEON"
#define MODEL_NAME	"CS5032"
#define MODULE_NAME	"light_sensor"
#define CS5032_LIGHT_NAME "cs5032_light"

#define PL_TIMER_DELAY 5L
#define POLLING_MODE 1
#define CS5032_DEFAULT_DELAY		200

#define SENSOR_ENABLE		1
#define SENSOR_DISABLE	0

enum {
    CS5032_SCAN_LIGHT_CH,
    CS5032_SCAN_LIGHT_TIMESTAMP,
};

#define CS5032_LIGHT_INFO_SHARED_MASK	(BIT(IIO_CHAN_INFO_SCALE))
#define CS5032_LIGHT_INFO_SEPARATE_MASK	(BIT(IIO_CHAN_INFO_RAW))

#define CS5032_LIGHT_CHANNEL()					\
{								\
    .type = IIO_LIGHT,					\
    .modified = 1,						\
    .channel2 = IIO_MOD_LIGHT,					\
    .info_mask_separate = CS5032_LIGHT_INFO_SEPARATE_MASK,	\
    .info_mask_shared_by_type = CS5032_LIGHT_INFO_SHARED_MASK,\
    .scan_index = CS5032_SCAN_LIGHT_CH,			\
    .scan_type = IIO_ST('s', 32, 32, 0)			\
}

static const struct iio_chan_spec CS5032_light_channels[] = {
    CS5032_LIGHT_CHANNEL(),
    IIO_CHAN_SOFT_TIMESTAMP(CS5032_SCAN_LIGHT_TIMESTAMP)
};

#define LSC_DBG
#ifdef LSC_DBG
#define LDBG(s,args...)	{printk("CS5032: func [%s], line [%d], ",__func__,__LINE__); printk(s,## args);}
#else
#define LDBG(s,args...) {}
#endif

static int CS5032_set_ahthres(struct i2c_client *client, int val);
static int CS5032_set_althres(struct i2c_client *client, int val);

struct CS5032_data {
    struct i2c_client *client;
    u8 reg_cache[CS5032_NUM_CACHABLE_REGS];//TO-DO
    u8 power_state_before_suspend;
    struct input_dev	*lsensor_input_dev;
    struct device *light_dev;
    struct delayed_work work;
    int delay;

    int32_t r_val;
    int32_t g_val;
    int32_t b_val;
    int32_t lux;

    u8 light_enabled;

    /* iio variables */
    struct iio_trigger *trig;
    int16_t sampling_frequency_light;
    atomic_t pseudo_irq_enable_light;
    struct mutex lock;
    spinlock_t spin_lock;
    struct mutex data_mutex;
    struct mutex light_mutex;
};

static struct CS5032_data *CS5032_data_g = NULL;
// CS5032 register
static u8 CS5032_reg[CS5032_NUM_CACHABLE_REGS] =
{
    0x00, 0x01, 0x02, 0x06, 0x07, 0x08, 0x0a, 0x1e, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x3c, 0x3d, 0x3e
};

// CS5032 range
static int CS5032_range[5] = {65536,16384,3072,2048,1024};

#if 0
static u16 CS5032_threshole[8] = {28,444,625,888,1778,3555,7222,0xffff};
#endif

static u8 *reg_array;
static int *range;
static int reg_num = 0;

static int cali = 100;

static int misc_ls_opened = 0;

#define MS_TO_NS(x)	(x * 1E6L)

#define ADD_TO_IDX(addr,idx)	{														\
    int i;												\
    for(i = 0; i < reg_num; i++)						\
    {													\
	if (addr == reg_array[i])						\
	{												\
	    idx = i;									\
	    break;										\
	}												\
    }													\
}

static inline s64 CS5032_iio_get_boottime_ns(void)
{
    struct timespec ts;

    ts = ktime_to_timespec(ktime_get_boottime());

    return timespec_to_ns(&ts);
}

static int CS5032_light_data_rdy_trig_poll(struct iio_dev *indio_dev)
{
    struct CS5032_data *st = iio_priv(indio_dev);
    unsigned long flags;
    spin_lock_irqsave(&st->spin_lock, flags);
    iio_trigger_poll(st->trig, CS5032_iio_get_boottime_ns());
    spin_unlock_irqrestore(&st->spin_lock, flags);
    return 0;
}

/*
 * register access helpers
 */

static int __CS5032_read_reg(struct i2c_client *client,
	u32 reg, u8 mask, u8 shift)
{
    int val = 0;

    val = i2c_smbus_read_byte_data(client, reg);

    return (val & mask) >> shift;
}

static int __CS5032_write_reg(struct i2c_client *client,
	u32 reg, u8 mask, u8 shift, u8 val)
{
    struct CS5032_data *data = i2c_get_clientdata(client);
    int ret = 0;
    u8 tmp;
    u8 idx = 0xff;

    ADD_TO_IDX(reg,idx)
	if (idx >= reg_num)
	    return -EINVAL;

    tmp = data->reg_cache[idx];
    tmp &= ~mask;
    tmp |= val << shift;

    ret = i2c_smbus_write_byte_data(client, reg, tmp);
    if (!ret)
	data->reg_cache[idx] = tmp;

    return ret;
}

/*
 * internally used functions
 */

/* range */
static int CS5032_get_range(struct i2c_client *client)
{
    u8 idx = __CS5032_read_reg(client, CS5032_REG_ALS_CON,
	    CS5032_REG_ALS_ALGAIN_CON_MASK, CS5032_REG_ALS_ALGAIN_CON_SHIFT);
    return range[idx];
}

static int CS5032_set_range(struct i2c_client *client, int range)
{
    return __CS5032_write_reg(client, CS5032_REG_ALS_CON,
	    CS5032_REG_ALS_ALGAIN_CON_MASK, CS5032_REG_ALS_ALGAIN_CON_SHIFT, range);
}


/* mode */
static int CS5032_get_mode(struct i2c_client *client)
{
    int ret;

    ret = __CS5032_read_reg(client, CS5032_REG_SYS_CON,
	    CS5032_REG_SYS_CON_MASK, CS5032_REG_SYS_CON_SHIFT);
    return ret;
}

static int CS5032_set_mode(struct i2c_client *client, int mode)
{
    int ret;
  if(mode == CS5032_SYS_ALS_ENABLE) {

	LDBG("mode = %x\n", mode);
	ret = __CS5032_write_reg(client, CS5032_REG_ALS_CON,
		CS5032_REG_ALS_ALGAIN_CON_MASK, CS5032_REG_ALS_ALGAIN_CON_SHIFT, 1);
	ret = __CS5032_write_reg(client, CS5032_REG_ALS_TIME,
		CS5032_REG_ALS_TIME_MASK, CS5032_REG_ALS_TIME_SHIFT, 0x3F);//resolution
	ret = CS5032_set_althres(client, 9830);
	ret = CS5032_set_ahthres(client, 55704);
	misc_ls_opened = 1;
    } else if(mode == CS5032_SYS_DEV_DOWN) {
	LDBG("mode = %x\n", mode);
	misc_ls_opened = 0;
    }

    ret = __CS5032_write_reg(client, CS5032_REG_SYS_CON,
	    CS5032_REG_SYS_CON_MASK, CS5032_REG_SYS_CON_SHIFT, mode);
    return ret;
}

/* ALS low threshold */
static int CS5032_get_althres(struct i2c_client *client)
{
    int lsb, msb;
    lsb = __CS5032_read_reg(client, CS5032_REG_AL_ALTHL_LOW,
	    CS5032_REG_AL_ALTHL_LOW_MASK, CS5032_REG_AL_ALTHL_LOW_SHIFT);
    msb = __CS5032_read_reg(client, CS5032_REG_AL_ALTHL_HIGH,
	    CS5032_REG_AL_ALTHL_HIGH_MASK, CS5032_REG_AL_ALTHL_HIGH_SHIFT);
    return ((msb << 8) | lsb);
}

static int CS5032_set_althres(struct i2c_client *client, int val)
{

    int lsb, msb, err;

    msb = val >> 8;
    lsb = val & CS5032_REG_AL_ALTHL_LOW_MASK;

    err = __CS5032_write_reg(client, CS5032_REG_AL_ALTHL_LOW,
	    CS5032_REG_AL_ALTHL_LOW_MASK, CS5032_REG_AL_ALTHL_LOW_SHIFT, lsb);
    if (err)
	return err;

    err = __CS5032_write_reg(client, CS5032_REG_AL_ALTHL_HIGH,
	    CS5032_REG_AL_ALTHL_HIGH_MASK, CS5032_REG_AL_ALTHL_HIGH_SHIFT, msb);

    return err;
}

/* ALS high threshold */
static int CS5032_get_ahthres(struct i2c_client *client)
{
    int lsb, msb;
    lsb = __CS5032_read_reg(client, CS5032_REG_AL_ALTHH_LOW,
	    CS5032_REG_AL_ALTHH_LOW_MASK, CS5032_REG_AL_ALTHH_LOW_SHIFT);
    msb = __CS5032_read_reg(client, CS5032_REG_AL_ALTHH_HIGH,
	    CS5032_REG_AL_ALTHH_HIGH_MASK, CS5032_REG_AL_ALTHH_HIGH_SHIFT);
    return ((msb << 8) | lsb);
}

static int CS5032_set_ahthres(struct i2c_client *client, int val)
{
    int lsb, msb, err;

    msb = val >> 8;
    lsb = val & CS5032_REG_AL_ALTHH_LOW_MASK;

    err = __CS5032_write_reg(client, CS5032_REG_AL_ALTHH_LOW,
	    CS5032_REG_AL_ALTHH_LOW_MASK, CS5032_REG_AL_ALTHH_LOW_SHIFT, lsb);
    if (err)
	return err;

    err = __CS5032_write_reg(client, CS5032_REG_AL_ALTHH_HIGH,
	    CS5032_REG_AL_ALTHH_HIGH_MASK, CS5032_REG_AL_ALTHH_HIGH_SHIFT, msb);

    return err;
}

#define COEF_R (0x00000001)//2's complement, refer to datacheet, add up to 32bits to accommondate r count value to prevent from overflowing
#define COEF_G (0x00000040)//2's complement, refer to datacheet, add up to 32bits to accommondate r count value to prevent from overflowing
#define COEF_B (0xffffffea)//2's complement, refer to datacheet, add up to 32bits to accommondate r count value to prevent from overflowing

static int32_t CS5032_get_als_value(struct CS5032_data *data)
{
    int32_t r_lsb, r_msb, g_lsb, g_msb, b_lsb, b_msb, lux;

    r_lsb = i2c_smbus_read_byte_data(data->client, CS5032_REG_RED_DATA_LOW);
    if (r_lsb < 0)
	return -1;
    r_msb = i2c_smbus_read_byte_data(data->client, CS5032_REG_RED_DATA_HIGH);
    if (r_msb < 0)
	return -1;
    data->r_val = r_msb << 8 | r_lsb;

    g_lsb = i2c_smbus_read_byte_data(data->client, CS5032_REG_GREEN_DATA_LOW);
    if (g_lsb < 0)
	return -1;
    g_msb = i2c_smbus_read_byte_data(data->client, CS5032_REG_GREEN_DATA_HIGH);
    if (g_msb < 0)
	return -1;
    data->g_val = g_msb << 8 | g_lsb;

    b_lsb = i2c_smbus_read_byte_data(data->client, CS5032_REG_BLUE_DATA_LOW);
    if (b_lsb < 0)
	return -1;
    b_msb = i2c_smbus_read_byte_data(data->client, CS5032_REG_BLUE_DATA_HIGH);
    if (b_msb < 0)
	return -1;
    data->b_val = b_msb << 8 | b_lsb;

    lux = (data->r_val * COEF_R + data->g_val * COEF_G + data->b_val * COEF_B)/64;//refer to datasheet

    return lux;
}

#if 0
static int CS5032_get_intstat(struct i2c_client *client)
{
    int val;

    val = i2c_smbus_read_byte_data(client, CS5032_REG_SYS_INTSTATUS);

    return val;
}
#endif

static int CS5032_lsensor_enable(struct CS5032_data *data)
{
    int ret = 0,mode;

    if (data->light_enabled == SENSOR_DISABLE) {
        mode = CS5032_get_mode(data->client);
        if((mode & CS5032_SYS_ALS_ENABLE) == 0){
            mode |= CS5032_SYS_ALS_ENABLE;
            ret = CS5032_set_mode(data->client,mode);
        }

        schedule_delayed_work(&data->work,
            msecs_to_jiffies(100));
	data->light_enabled = SENSOR_ENABLE;
    }
    return ret;
}

static int CS5032_lsensor_disable(struct CS5032_data *data)
{
    int ret = 0,mode;

    if (data->light_enabled == SENSOR_ENABLE) {
        mode = CS5032_get_mode(data->client);
        if(mode & CS5032_SYS_ALS_ENABLE){
	      mode &= ~CS5032_SYS_ALS_ENABLE;
	      if(mode == CS5032_SYS_RST_ENABLE)
	          mode = CS5032_SYS_DEV_DOWN;
            ret = CS5032_set_mode(data->client,mode);
        }

        cancel_delayed_work_sync(&data->work);
        data->light_enabled = SENSOR_DISABLE;
    }
    return ret;
}

static int CS5032_register_lsensor_device(struct i2c_client *client, struct CS5032_data *data)
{
    struct input_dev *input_dev;
    int rc;

    LDBG("allocating input device lsensor\n");
    input_dev = input_allocate_device();
    if (!input_dev) {
	dev_err(&client->dev,"%s: could not allocate input device for lsensor\n", __FUNCTION__);
	rc = -ENOMEM;
	goto done;
    }
    data->lsensor_input_dev = input_dev;
    input_set_drvdata(input_dev, data);
    input_dev->name = "lightsensor-level";
    input_dev->dev.parent = &client->dev;
    set_bit(EV_ABS, input_dev->evbit);
    input_set_abs_params(input_dev, ABS_MISC, 0, 8, 0, 0);

    rc = input_register_device(input_dev);
    if (rc < 0) {
	pr_err("%s: could not register input device for lsensor\n", __FUNCTION__);
	goto done;
    }
done:
    return rc;
}


static void CS5032_unregister_lsensor_device(struct i2c_client *client, struct CS5032_data *data)
{
    input_unregister_device(data->lsensor_input_dev);
}

#if 0
static void CS5032_change_ls_threshold(struct i2c_client *client)
{
    struct CS5032_data *data = i2c_get_clientdata(client);
    int value;

    value = CS5032_get_als_value(client);
    if(value > 0){
	CS5032_set_althres(client,CS5032_threshole[value-1]);
	CS5032_set_ahthres(client,CS5032_threshole[value]);
    }
    else{
	CS5032_set_althres(client,0);
	CS5032_set_ahthres(client,CS5032_threshole[value]);
    }

    input_report_abs(data->lsensor_input_dev, ABS_MISC, value);
    input_sync(data->lsensor_input_dev);

}
#endif

static int CS5032_resume(struct device *dev)
{
    struct CS5032_data *data = dev_get_drvdata(dev);

    if (data->light_enabled) {
        CS5032_lsensor_enable(data);
    }
    return 0;
}

static int CS5032_suspend(struct device *dev)
{
    struct CS5032_data *data = dev_get_drvdata(dev);

    if (data->light_enabled) {
        CS5032_lsensor_disable(data);
    }
    return 0;
}


/* range */
static ssize_t CS5032_show_range(struct device *dev,
	struct device_attribute *attr, char *buf)
{
    struct CS5032_data *data = CS5032_data_g;
    return sprintf(buf, "%i\n", CS5032_get_range(data->client));
}

static ssize_t CS5032_store_range(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
    struct CS5032_data *data = CS5032_data_g;
    unsigned long val;
    int ret;

    if ((strict_strtoul(buf, 10, &val) < 0) || (val > 3))
	return -EINVAL;

    ret = CS5032_set_range(data->client, val);
    if (ret < 0)
	return ret;

    return count;
}


/* mode */
static ssize_t CS5032_show_mode(struct device *dev,
	struct device_attribute *attr, char *buf)
{
    struct CS5032_data *data = CS5032_data_g;
    return sprintf(buf, "%d\n", CS5032_get_mode(data->client));
}

static ssize_t CS5032_store_mode(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
    struct CS5032_data *data = CS5032_data_g;
    unsigned long val;
    int ret;

    if ((strict_strtoul(buf, 10, &val) < 0))
	return -EINVAL;
    ret = CS5032_set_mode(data->client, val);

    if (ret < 0)
	return ret;
    return count;
}

#if 0
/* lux */
static ssize_t CS5032_show_lux(struct device *dev,
	struct device_attribute *attr, char *buf)
{
    struct CS5032_data *data = CS5032_data_g;

    /* No LUX data if power down */
    if (CS5032_get_mode(CS5032_data_g->client) != CS5032_SYS_ALS_ENABLE)
	return sprintf((char*) buf, "%s\n", "Please power up first!");

    return sprintf(buf, "%d\n", CS5032_get_als_value(data));
}
#endif

/* ALS low threshold */
static ssize_t CS5032_show_althres(struct device *dev,
	struct device_attribute *attr, char *buf)
{
    struct CS5032_data *data = CS5032_data_g;
    return sprintf(buf, "%d\n", CS5032_get_althres(data->client));
}

static ssize_t CS5032_store_althres(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
    struct CS5032_data *data = CS5032_data_g;
    unsigned long val;
    int ret;

    if (strict_strtoul(buf, 10, &val) < 0)
	return -EINVAL;

    ret = CS5032_set_althres(data->client, val);
    if (ret < 0)
	return ret;

    return count;
}


/* ALS high threshold */
static ssize_t CS5032_show_ahthres(struct device *dev,
	struct device_attribute *attr, char *buf)
{
    struct CS5032_data *data = CS5032_data_g;
    return sprintf(buf, "%d\n", CS5032_get_ahthres(data->client));
}

static ssize_t CS5032_store_ahthres(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
    struct CS5032_data *data = CS5032_data_g;
    unsigned long val;
    int ret;

    if (strict_strtoul(buf, 10, &val) < 0)
	return -EINVAL;

    ret = CS5032_set_ahthres(data->client, val);
    if (ret < 0)
	return ret;

    return count;
}

/* calibration */
static ssize_t CS5032_show_calibration_state(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
    return sprintf(buf, "%d\n", cali);
}

static ssize_t CS5032_store_calibration_state(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
    struct CS5032_data *data = CS5032_data_g;
    int stdls, lux;
    char tmp[10];

    LDBG("DEBUG CS5032_store_calibration_state..\n");

    /* No LUX data if not operational */
    if (CS5032_get_mode(data->client) == CS5032_SYS_DEV_DOWN)
    {
	printk("Please power up first!");
	return -EINVAL;
    }

    cali = 100;
    sscanf(buf, "%d %s", &stdls, tmp);

    if (!strncmp(tmp, "-setcv", 6))
    {
	cali = stdls;
	return -EBUSY;
    }

    if (stdls < 0)
    {
	printk("Std light source: [%d] < 0 !!!\nCheck again, please.\n\
		Set calibration factor to 100.\n", stdls);
	return -EBUSY;
    }

    lux = CS5032_get_als_value(data);
    cali = stdls * 100 / lux;

    return -EBUSY;
}

/* light sysfs */
static ssize_t CS5032_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "%s\n", MODEL_NAME);
}

static ssize_t CS5032_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR_NAME);
}

static ssize_t light_data_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
    struct CS5032_data *data = dev_get_drvdata(dev);
    int ret;

    ret = CS5032_get_als_value(data);

    return snprintf(buf, PAGE_SIZE, "%u,%u,%u\n",
                                data->r_val, data->g_val, data->b_val);
}

#ifdef LSC_DBG
/* engineer mode */
static ssize_t CS5032_em_read(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
    struct CS5032_data *data = CS5032_data_g;
    int i;
    u8 tmp;

    LDBG("DEBUG CS5032_em_read..\n");

    for (i = 0; i < reg_num; i++)
    {
	tmp = i2c_smbus_read_byte_data(data->client, reg_array[i]);

	LDBG("Reg[0x%x] Val[0x%x]\n", reg_array[i], tmp);
    }

    return 0;
}

static ssize_t CS5032_em_write(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
    struct CS5032_data *data = CS5032_data_g;
    u32 addr,val,idx=0;
    int ret = 0;

    LDBG("DEBUG CS5032_em_write..\n");

    sscanf(buf, "%x%x", &addr, &val);

    printk("Write [%x] to Reg[%x]...\n",val,addr);

    ret = i2c_smbus_write_byte_data(data->client, addr, val);
    ADD_TO_IDX(addr,idx)
	if (!ret)
	    data->reg_cache[idx] = val;

    return count;
}
#endif


static DEVICE_ATTR(range, S_IWUSR | S_IRUGO, CS5032_show_range, CS5032_store_range);
static DEVICE_ATTR(mode, 0666, CS5032_show_mode, CS5032_store_mode);
static DEVICE_ATTR(althres, S_IWUSR | S_IRUGO, CS5032_show_althres, CS5032_store_althres);
static DEVICE_ATTR(ahthres, S_IWUSR | S_IRUGO, CS5032_show_ahthres, CS5032_store_ahthres);
static DEVICE_ATTR(calibration, S_IWUSR | S_IRUGO, CS5032_show_calibration_state, CS5032_store_calibration_state);
#ifdef LSC_DBG
static DEVICE_ATTR(em, S_IWUSR | S_IRUGO, CS5032_em_read, CS5032_em_write);
#endif
static DEVICE_ATTR(name, S_IRUGO, CS5032_name_show, NULL);
static DEVICE_ATTR(vendor, S_IRUGO, CS5032_vendor_show, NULL);
static DEVICE_ATTR(lux, S_IRUGO, light_data_show, NULL);
static DEVICE_ATTR(raw_data, S_IRUGO, light_data_show, NULL);


static struct device_attribute *sensor_attrs[] = {
    &dev_attr_range,
    &dev_attr_mode,
    &dev_attr_althres,
    &dev_attr_ahthres,
    &dev_attr_calibration,
#ifdef LSC_DBG
    &dev_attr_em,
#endif
    &dev_attr_name,
    &dev_attr_vendor,
    &dev_attr_lux,
    &dev_attr_raw_data,
    NULL,
};

static ssize_t CS5032_light_sampling_frequency_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    struct iio_dev *indio_dev = dev_get_drvdata(dev);
    struct CS5032_data *st = iio_priv(indio_dev);
    return sprintf(buf, "%d\n", (int)st->sampling_frequency_light);
}

static ssize_t CS5032_light_sampling_frequency_store(struct device *dev,
        struct device_attribute *attr,
        const char *buf, size_t count)
{
    struct iio_dev *indio_dev = dev_get_drvdata(dev);
    struct CS5032_data *st = iio_priv(indio_dev);
    int ret, data;
    int delay;
    ret = kstrtoint(buf, 10, &data);
    if (ret)
        return ret;
    if (data <= 0)
        return -EINVAL;
    mutex_lock(&st->lock);
    st->sampling_frequency_light = data;
    delay = MSEC_PER_SEC / st->sampling_frequency_light;
    // has to be checked about delay
    st->delay = delay;
    mutex_unlock(&st->lock);
    return count;
}

/* iio light sysfs - sampling frequency */
static IIO_DEVICE_ATTR(sampling_frequency, S_IRUSR|S_IWUSR,
        CS5032_light_sampling_frequency_show,
        CS5032_light_sampling_frequency_store, 0);

static struct attribute *CS5032_iio_light_attributes[] = {
    &iio_dev_attr_sampling_frequency.dev_attr.attr,
    NULL
};

static const struct attribute_group CS5032_iio_light_attribute_group = {
    .attrs = CS5032_iio_light_attributes,
};


static int CS5032_init_client(struct i2c_client *client)
{
    struct CS5032_data *data = i2c_get_clientdata(client);
    int i;

    LDBG("DEBUG CS5032_init_client..\n");


    /* read all the registers once to fill the cache.
     * if one of the reads fails, we consider the init failed */
    for (i = 0; i < reg_num; i++) {
	int v = i2c_smbus_read_byte_data(client, reg_array[i]);
	if (v < 0)
	    return -ENODEV;

	data->reg_cache[i] = v;
    }

    /* set defaults */
    CS5032_set_range(client, CS5032_ALS_GAIN_1);
    CS5032_set_mode(client, CS5032_SYS_RST_ENABLE);

    return 0;
}

static void CS5032_work_func_light(struct work_struct *work)
{

    struct CS5032_data *data = container_of((struct delayed_work *)work,
			struct CS5032_data, work);
    struct iio_dev *indio_dev = iio_priv_to_dev(data);

    unsigned long delay = msecs_to_jiffies(data->delay);

    data->lux = CS5032_get_als_value(data);

    CS5032_light_data_rdy_trig_poll(indio_dev);

    schedule_delayed_work(&data->work, delay);
}

static irqreturn_t CS5032_light_trigger_handler(int irq, void *p)
{
    struct iio_poll_func *pf = p;
    struct iio_dev *indio_dev = pf->indio_dev;
    struct CS5032_data *st = iio_priv(indio_dev);
    int len = 0;
    int ret = 0;
    int32_t *data;

    data = (int32_t *) kmalloc(indio_dev->scan_bytes, GFP_KERNEL);
    if (data == NULL)
        goto done;
    if (!bitmap_empty(indio_dev->active_scan_mask, indio_dev->masklength))
        *data = st->lux;

    len = 4;
    /* Guaranteed to be aligned with 8 byte boundary */
    if (indio_dev->scan_timestamp)
        *(s64 *)((u8 *)data + ALIGN(len, sizeof(s64))) = pf->timestamp;
    ret = iio_push_to_buffers(indio_dev, (u8 *)data);
    if (ret < 0)
        pr_err("%s, iio_push buffer failed = %d\n",
			__func__, ret);
    kfree(data);
done:
    iio_trigger_notify_done(indio_dev->trig);
    return IRQ_HANDLED;
}


static const struct iio_buffer_setup_ops CS5032_light_buffer_setup_ops = {
    .preenable = &iio_sw_buffer_preenable,
    .postenable = &iio_triggered_buffer_postenable,
    .predisable = &iio_triggered_buffer_predisable,
};

irqreturn_t CS5032_iio_pollfunc_store_boottime(int irq, void *p)
{
    struct iio_poll_func *pf = p;
    pf->timestamp = CS5032_iio_get_boottime_ns();
    return IRQ_WAKE_THREAD;
}

static int CS5032_light_pseudo_irq_enable(struct iio_dev *indio_dev)
{
    struct CS5032_data *st = iio_priv(indio_dev);
    pr_err(" %s : START\n", __func__);

    if (!atomic_cmpxchg(&st->pseudo_irq_enable_light, 0, 1)) {
        pr_err("%s, enable routine\n", __func__);
        mutex_lock(&st->lock);
        CS5032_lsensor_enable(st);
        mutex_unlock(&st->lock);
        schedule_delayed_work(&st->work, 0);
    }
    return 0;
}

static int CS5032_light_pseudo_irq_disable(struct iio_dev *indio_dev)
{
    struct CS5032_data *st = iio_priv(indio_dev);
    if (atomic_cmpxchg(&st->pseudo_irq_enable_light, 1, 0)) {
        mutex_lock(&st->lock);
        CS5032_lsensor_disable(st);
        mutex_unlock(&st->lock);
        }
    return 0;
}


static int CS5032_light_set_pseudo_irq(struct iio_dev *indio_dev, int enable)
{
    if (enable)
        CS5032_light_pseudo_irq_enable(indio_dev);
    else
        CS5032_light_pseudo_irq_disable(indio_dev);
    return 0;
}


static int CS5032_data_light_rdy_trigger_set_state(struct iio_trigger *trig,
        bool state)
{
    struct iio_dev *indio_dev = iio_trigger_get_drvdata(trig);
    pr_info("%s, called state = %d\n", __func__, state);
    CS5032_light_set_pseudo_irq(indio_dev, state);
    return 0;
}


static const struct iio_trigger_ops CS5032_light_trigger_ops = {
    .owner = THIS_MODULE,
    .set_trigger_state = &CS5032_data_light_rdy_trigger_set_state,
};

static int CS5032_read_raw_light(struct iio_dev *indio_dev,
        struct iio_chan_spec const *chan,
        int *val, int *val2, long mask)
{
    struct CS5032_data  *st = iio_priv(indio_dev);
    int ret = -EINVAL;

    if (chan->type != IIO_LIGHT)
        return -EINVAL;

    mutex_lock(&st->lock);

    switch (mask) {
    case 0:
        ret = IIO_VAL_INT;
        break;
    case IIO_CHAN_INFO_SCALE:
        *val = 0;
        *val2 = 1000;
        ret = IIO_VAL_INT_PLUS_MICRO;
        break;
    }

    mutex_unlock(&st->lock);

    return ret;
}


static int CS5032_light_probe_buffer(struct iio_dev *indio_dev)
{
    int ret;
    struct iio_buffer *buffer;

    buffer = iio_kfifo_allocate(indio_dev);
    if (!buffer) {
        ret = -ENOMEM;
        goto error_ret;
    }

    buffer->scan_timestamp = true;
    indio_dev->buffer = buffer;
    indio_dev->setup_ops = &CS5032_light_buffer_setup_ops;
    indio_dev->modes |= INDIO_BUFFER_TRIGGERED;

    ret = iio_buffer_register(indio_dev, indio_dev->channels,
        indio_dev->num_channels);
    if (ret)
        goto error_free_buf;

    iio_scan_mask_set(indio_dev, indio_dev->buffer, CS5032_SCAN_LIGHT_CH);

    pr_err("%s, successed \n", __func__);
    return 0;

error_free_buf:
	pr_err("%s, failed \n", __func__);
	iio_kfifo_free(indio_dev->buffer);
error_ret:
	return ret;
}

static int CS5032_light_probe_trigger(struct iio_dev *indio_dev)
{
    int ret;
    struct CS5032_data *st = iio_priv(indio_dev);
    indio_dev->pollfunc = iio_alloc_pollfunc(&CS5032_iio_pollfunc_store_boottime,
            &CS5032_light_trigger_handler, IRQF_ONESHOT, indio_dev,
            "%s_consumer%d", indio_dev->name, indio_dev->id);

    pr_err("%s is called\n", __func__);

    if (indio_dev->pollfunc == NULL) {
        ret = -ENOMEM;
        goto error_ret;
    }
    st->trig = iio_trigger_alloc("%s-dev%d",
            indio_dev->name,
            indio_dev->id);
    if (!st->trig) {
        ret = -ENOMEM;
        goto error_dealloc_pollfunc;
    }
    st->trig->dev.parent = &st->client->dev;
    st->trig->ops = &CS5032_light_trigger_ops;
    iio_trigger_set_drvdata(st->trig, indio_dev);
    ret = iio_trigger_register(st->trig);

    if (ret)
        goto error_free_trig;
        pr_err("%s is success\n", __func__);

    return 0;

error_free_trig:
    iio_trigger_free(st->trig);
error_dealloc_pollfunc:
    iio_dealloc_pollfunc(indio_dev->pollfunc);
error_ret:
    return ret;
}

static void CS5032_light_remove_buffer(struct iio_dev *indio_dev)
{
    iio_buffer_unregister(indio_dev);
    iio_kfifo_free(indio_dev->buffer);
}

static void CS5032_light_remove_trigger(struct iio_dev *indio_dev)
{
    struct CS5032_data *st = iio_priv(indio_dev);
    iio_trigger_unregister(st->trig);
    iio_trigger_free(st->trig);
    iio_dealloc_pollfunc(indio_dev->pollfunc);
}

static const struct iio_info CS5032_light_info= {
    .read_raw = &CS5032_read_raw_light,
    .attrs = &CS5032_iio_light_attribute_group,
    .driver_module = THIS_MODULE,
};


static int __devinit CS5032_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
    struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
    struct CS5032_data *data;
    struct CS5032_data *data_iio = NULL;
    struct iio_dev *indio_dev;
    int err = 0;

    LDBG("CS5032_probe\n");

    if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE)){
	err = -EIO;
	goto exit_free_gpio;
    }

    reg_array = CS5032_reg;
    range = CS5032_range;
    reg_num = CS5032_NUM_CACHABLE_REGS;

    data = kzalloc(sizeof(struct CS5032_data), GFP_KERNEL);
    if (!data){
	err = -ENOMEM;
	goto exit_free_gpio;
    }

    /* iio device register - light*/
    indio_dev = iio_device_alloc(sizeof(*data_iio));
    if (!indio_dev) {
        pr_err("%s, iio_dev_light alloc failed\n", __func__);
        goto exit_free_gpio;
    }
    i2c_set_clientdata(client, indio_dev);

    indio_dev->name = CS5032_LIGHT_NAME;
    indio_dev->dev.parent = &client->dev;
    indio_dev->info = &CS5032_light_info;
    indio_dev->channels = CS5032_light_channels;
    indio_dev->num_channels = ARRAY_SIZE(CS5032_light_channels);
    indio_dev->modes = INDIO_DIRECT_MODE;

    data_iio = iio_priv(indio_dev);
    data_iio->client = client;
    data_iio->sampling_frequency_light = 5;
    data_iio->delay = MSEC_PER_SEC / data_iio->sampling_frequency_light;
    data_iio->light_enabled = SENSOR_DISABLE;

    spin_lock_init(&data_iio->spin_lock);
    mutex_init(&data_iio->lock);
    mutex_init(&data_iio->data_mutex);
    mutex_init(&data_iio->light_mutex);

    pr_err("%s, before light probe buffer\n", __func__);
    err = CS5032_light_probe_buffer(indio_dev);
    if (err) {
        pr_err("%s, light probe buffer failed\n", __func__);
        goto error_free_light_dev;
    }

    err = CS5032_light_probe_trigger(indio_dev);
    if (err) {
        pr_err("%s, light probe trigger failed\n", __func__);
        goto error_remove_light_buffer;
    }

    err = iio_device_register(indio_dev);
    if (err) {
        pr_err("%s, light iio register failed\n", __func__);
        goto error_remove_light_trigger;
    }

    INIT_DELAYED_WORK(&data_iio->work, CS5032_work_func_light);

    data->client = client;
    i2c_set_clientdata(client, data);

    /* initialize the CS5032 chip */
    err = CS5032_init_client(client);
    if (err)
	goto error_init_client;

    /* input device init */
    err = CS5032_register_lsensor_device(client,data);
    if (err){
	dev_err(&client->dev, "failed to register_lsensor_device\n");
	goto error_init_client;
    }

    CS5032_data_g = data;
    data->delay = CS5032_DEFAULT_DELAY;

    /* set sysfs for light sensor */
    sensors_register(data->light_dev, data, sensor_attrs, MODULE_NAME);

    return 0;

error_init_client:
    iio_device_unregister(indio_dev);
error_remove_light_trigger:
    CS5032_light_remove_trigger(indio_dev);
error_remove_light_buffer:
    CS5032_light_remove_buffer(indio_dev);
error_free_light_dev:
    iio_device_free(indio_dev);
    kfree(data);
exit_free_gpio:
    return err;
}

static int __devexit CS5032_remove(struct i2c_client *client)
{
    struct CS5032_data *data = i2c_get_clientdata(client);

    CS5032_unregister_lsensor_device(client,data);
    CS5032_set_mode(client, CS5032_SYS_DEV_DOWN);

    /* sysfs destroy */
    sensors_unregister(data->light_dev, sensor_attrs);

    kfree(i2c_get_clientdata(client));

    return 0;
}

#ifdef CONFIG_OF
static struct of_device_id CS5032_match_table[] = {
    { .compatible = "cs5032",},
    {},
};
#else
#define CS5032_match_table NULL
#endif

static const struct i2c_device_id CS5032_id[] = {
    { CS5032_DRV_NAME, 0 },
    {}
};
MODULE_DEVICE_TABLE(i2c, CS5032_id);

static const struct dev_pm_ops CS5032_pm_ops = {
	.suspend = CS5032_suspend,
	.resume = CS5032_resume
};

static struct i2c_driver CS5032_driver = {
    .driver = {
	.name	= CS5032_DRV_NAME,
	.owner	= THIS_MODULE,
	.of_match_table = CS5032_match_table,
	.pm = &CS5032_pm_ops
    },
    .probe	= CS5032_probe,
    .remove	= __devexit_p(CS5032_remove),
    .id_table = CS5032_id,
};

static int __init CS5032_init(void)
{
    int ret;

    ret = i2c_add_driver(&CS5032_driver);
    return ret;

}

static void __exit CS5032_exit(void)
{
    i2c_del_driver(&CS5032_driver);
}

MODULE_AUTHOR("Templeton Tsai Dyna-Image Corporation.");
MODULE_DESCRIPTION("CS5032 driver.");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(DRIVER_VERSION);

module_init(CS5032_init);
module_exit(CS5032_exit);


