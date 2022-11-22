/* driver/sensor/cm36652.c
 * Copyright (c) 2011 SAMSUNG
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */

#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/i2c.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/wakelock.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include "cm36652.h"
#include <linux/sensor/sensors_core.h>
#include <linux/regulator/consumer.h>

///IIO additions...
//
#include <linux/iio/iio.h>
#include <linux/iio/buffer.h>
#include <linux/iio/kfifo_buf.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/events.h>
#include <linux/iio/trigger.h>
#include <linux/iio/trigger_consumer.h>

/* For debugging */
#undef	cm36652_DEBUG

#define	VENDOR		"CAPELLA"
#define	CHIP_ID		"CM36652"

#define I2C_M_WR	0 /* for i2c Write */

/* register addresses */
/* Ambient light sensor */
#define REG_CS_CONF1		0x00
#define REG_ALS_RED_DATA	0x08
#define REG_ALS_GREEN_DATA	0x09 /* ALS */
#define REG_ALS_BLUE_DATA	0x0A
#define REG_WHITE_DATA		0x0B

/* Proximity sensor */
#define REG_PS_CONF1		0x03
#define REG_PS_CONF3		0x04
#define REG_PS_THD		0x05
#define REG_PS_CANC		0x06
#define REG_PS_DATA		0x07

#define ALS_REG_NUM		2
#define PS_REG_NUM		4

#define MSK_L(x)		(x & 0xff)
#define MSK_H(x)		((x & 0xff00) >> 8)

/* Intelligent Cancelation*/
#define CM36652_CANCELATION
#ifdef CM36652_CANCELATION
#define CANCELATION_FILE_PATH	"/efs/FactoryApp/prox_cal"
#define CAL_SKIP_ADC		9
#define CAL_FAIL_ADC		26
#endif

#define PROX_READ_NUM		40

 /* proximity sensor threshold */
#define DEFUALT_HI_THD		0x0011
#define DEFUALT_LOW_THD		0x000D
#define CANCEL_HI_THD		0x000A
#define CANCEL_LOW_THD		0x0007

#define DEFAULT_TRIM		0X0003
 /*lightsnesor log time 6SEC 200mec X 30*/
#define LIGHT_LOG_TIME		30
#define LIGHT_ADD_STARTTIME	300000000
#define LIGHT_ESD_TIME		15

#define CM36652_PROX_NAME	"cm36652_prox"
#define CM36652_LIGHT_NAME	"cm36652_light"

#define CM36652_PROX_INFO_SHARED_MASK		(BIT(IIO_CHAN_INFO_SCALE))
#define CM36652_PROX_INFO_SEPARATE_MASK		(BIT(IIO_CHAN_INFO_RAW))

#define CM36652_LIGHT_INFO_SHARED_MASK		(BIT(IIO_CHAN_INFO_SCALE))
#define CM36652_LIGHT_INFO_SEPARATE_MASK	(BIT(IIO_CHAN_INFO_RAW))

#define CM36652_PROX_CHANNEL()						\
{									\
	.type = IIO_PROXIMITY,						\
	.modified = 1,							\
	.channel2 = IIO_MOD_PROXIMITY,					\
	.info_mask_separate = CM36652_PROX_INFO_SEPARATE_MASK,		\
	.info_mask_shared_by_type = CM36652_PROX_INFO_SHARED_MASK,	\
	.scan_index = CM36652_SCAN_PROX_CH,				\
	.scan_type = IIO_ST('s', 32, 32, 0)				\
}
#define CM36652_LIGHT_CHANNEL(color)					\
{									\
	.type = IIO_LIGHT,						\
	.modified = 1,							\
	.channel2 = IIO_MOD_LIGHT_##color,				\
	.info_mask_separate = CM36652_LIGHT_INFO_SEPARATE_MASK,		\
	.info_mask_shared_by_type = CM36652_LIGHT_INFO_SHARED_MASK,	\
	.scan_index = CM36652_SCAN_LIGHT_CH_##color,			\
	.scan_type = IIO_ST('s', 32, 32, 0)				\
}

enum {
	CM36652_SCAN_PROX_CH,
	CM36652_SCAN_PROX_TIMESTAMP,
};

enum {
	CM36652_SCAN_LIGHT_CH_RED,
	CM36652_SCAN_LIGHT_CH_GREEN,
	CM36652_SCAN_LIGHT_CH_BLUE,
	CM36652_SCAN_LIGHT_CH_CLEAR,
	CM36652_SCAN_LIGHT_TIMESTAMP,
};

enum {
	LIGHT_ENABLED = BIT(0),
	PROXIMITY_ENABLED = BIT(1),
};

#define SENSOR_ENABLE		1
#define SENSOR_DISABLE		0

/* register settings */
static u16 als_reg_setting[ALS_REG_NUM][2] = {
	{REG_CS_CONF1, 0x0000},	/* enable */
	{REG_CS_CONF1, 0x0001},	/* disable */
};

/* Change threshold value on the midas-sensor.c */
enum {
	PS_CONF1 = 0,
	PS_CONF3,
	PS_THD,
	PS_CANCEL,
};
enum {
	REG_ADDR = 0,
	CMD,
};

static u16 ps_reg_init_setting[PS_REG_NUM][2] = {
	{REG_PS_CONF1, 0x7728},	/* REG_PS_CONF1 */
	{REG_PS_CONF3, 0x0000},	/* REG_PS_CONF3 */
	{REG_PS_THD, 0x0110D},	/* REG_PS_THD */
	{REG_PS_CANC, DEFAULT_TRIM},	/* REG_PS_CANC */
};

/* driver data */
struct cm36652_data {
	struct i2c_client *i2c_client;
	struct wake_lock prx_wake_lock;
	struct input_dev *proximity_input_dev;
	struct input_dev *light_input_dev;
	struct cm36652_platform_data *pdata;
	struct mutex power_lock;
	struct mutex read_lock;
	struct hrtimer light_timer;
	struct hrtimer prox_timer;
	struct workqueue_struct *light_wq;
	struct workqueue_struct *prox_wq;
	struct work_struct work_light;
	struct work_struct work_prox;
	struct device *proximity_dev;
	struct device *light_dev;
	struct iio_dev *indio_dev_prox;
	struct iio_dev *indio_dev_light;

	ktime_t light_poll_delay;
	ktime_t prox_poll_delay;
	int irq;
	u8 power_state;
	int avg[3];
	u16 als_data;
	u16 als_red_data;
	u16 als_green_data;
	u16 als_blue_data;
	u16 white_data;
	u16 color[4];
	u8 proximity_detection;
	int count_log_time;
	int count_esd_time;
	unsigned int uProxCalResult;

	int light_delay;
	int prox_delay;

	/* iio variables */
	struct iio_trigger *light_trig;
	struct iio_trigger *prox_trig;
	int16_t sampling_frequency_prox;
	int16_t sampling_frequency_light;
	atomic_t pseudo_irq_enable_prox;
	atomic_t pseudo_irq_enable_light;
	struct mutex lock;
	spinlock_t spin_lock;
	struct regulator *vdd_2p85;
	struct regulator *lvs1_1p8;
};


struct cm36652_sensor_data {
	struct cm36652_data *cdata;
};

static inline s64 cm36652_iio_get_boottime_ns(void)
{
	struct timespec ts;

	ts = ktime_to_timespec(ktime_get_boottime());

	return timespec_to_ns(&ts);
}

irqreturn_t cm36652_iio_pollfunc_store_boottime(int irq, void *p)
{
	struct iio_poll_func *pf = p;
	pf->timestamp = cm36652_iio_get_boottime_ns();
	return IRQ_WAKE_THREAD;
}

static int cm36652_light_data_rdy_trig_poll(struct iio_dev *indio_dev)
{
	struct cm36652_sensor_data *sdata = iio_priv(indio_dev);
	struct cm36652_data *cm36652_iio = sdata->cdata;
	unsigned long flags;
	spin_lock_irqsave(&cm36652_iio->spin_lock, flags);
	iio_trigger_poll(cm36652_iio->light_trig, cm36652_iio_get_boottime_ns());
	spin_unlock_irqrestore(&cm36652_iio->spin_lock, flags);
	return 0;
}

static int cm36652_prox_data_rdy_trig_poll(struct iio_dev *indio_dev)
{
	struct cm36652_sensor_data *sdata = iio_priv(indio_dev);
	struct cm36652_data *cm36652_iio = sdata->cdata;
	unsigned long flags;

	spin_lock_irqsave(&cm36652_iio->spin_lock, flags);
	iio_trigger_poll(cm36652_iio->prox_trig, cm36652_iio_get_boottime_ns());
	spin_unlock_irqrestore(&cm36652_iio->spin_lock, flags);
	return 0;
}


int cm36652_i2c_read_word(struct cm36652_data *cm36652_iio, u8 command, u16 *val)
{
	int err = 0;
	struct i2c_client *client = cm36652_iio->i2c_client;
	struct i2c_msg msg[2];
	unsigned char data[2] = {0,};
	u16 value = 0;

	if ((client == NULL) || (!client->adapter))
		return -ENODEV;

	/* send slave address & command */
	msg[0].addr = client->addr;
	msg[0].flags = I2C_M_WR;
	msg[0].len = 1;
	msg[0].buf = &command;

	/* read word data */
	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 2;
	msg[1].buf = data;

	err = i2c_transfer(client->adapter, msg, 2);
	if (err >= 0) {
		value = (u16)data[1];
		*val = (value << 8) | (u16)data[0];
		return err;
	}

	pr_err("%s, i2c transfer error ret=%d\n", __func__, err);
	return err;
}

int cm36652_i2c_write_word(struct cm36652_data *cm36652_iio, u8 command,
			   u16 val)
{
	int err = 0;
	struct i2c_client *client = cm36652_iio->i2c_client;

	if ((client == NULL) || (!client->adapter))
		return -ENODEV;

	err = i2c_smbus_write_word_data(client, command, val);
	if (err >= 0)
		return 0;

	pr_err("%s, i2c transfer error(%d)\n", __func__, err);
	return err;
}

#if 0
static void cm36652_led_onoff(struct cm36652_data *cm36652_iio, bool onoff)
{
	pr_info("%s onoff : %d\n", __func__,onoff);

	cm36652_iio->vled_2p8 = regulator_get(NULL, "VLED_2.8V");
	if (IS_ERR(cm36652_iio->vled_2p8)) {
		pr_err("%s : regulator 2.8 is not available", __func__);
		return;
	}

	if (onoff) {
		int ret = regulator_enable(cm36652_iio->vled_2p8);
		if (ret < 0) {
			pr_err("%s : regulator 2.8 is not enable", __func__);
			return;
		}
		usleep_range(10000, 20000);
	} else {
		regulator_disable(cm36652_iio->vled_2p8);
	}

	regulator_put(cm36652_iio->vled_2p8);
	return;
}
#endif

static int cm36652_setup_leden_gpio(struct cm36652_data *cm36652_iio)
{
	int rc;
	struct cm36652_platform_data *pdata = cm36652_iio->pdata;

	rc = gpio_request(pdata->leden_gpio, "rgb_en");
	if (rc < 0) {
		pr_err("%s: gpio %d request failed (%d)\n",
			__func__, pdata->leden_gpio, rc);
	}
	gpio_direction_output(pdata->leden_gpio, 1);
	pr_info("%s: gpio %d request success\n", __func__, pdata->leden_gpio);
	return rc;
}

static int cm36652_leden_gpio_onoff(struct cm36652_data *cm36652_iio, bool onoff)
{
	struct cm36652_platform_data *pdata = cm36652_iio->pdata;
	gpio_set_value(pdata->leden_gpio, onoff);
	pr_info("%s onoff:%d\n", __func__, onoff);
	if (onoff)
		msleep(20);
	return 0;
}

static void cm36652_light_enable(struct cm36652_data *cm36652_iio)
{
	pr_info("%s\n", __func__);
	cm36652_iio->power_state |= LIGHT_ENABLED;
	/* enable setting */
	cm36652_i2c_write_word(cm36652_iio, REG_CS_CONF1,
		als_reg_setting[0][1]);
	hrtimer_start(&cm36652_iio->light_timer,
		ns_to_ktime(200 * NSEC_PER_MSEC), HRTIMER_MODE_REL);
}

static void cm36652_light_disable(struct cm36652_data *cm36652_iio)
{
	pr_info("%s\n", __func__);
	/* disable setting */
	cm36652_i2c_write_word(cm36652_iio, REG_CS_CONF1,
		als_reg_setting[1][1]);
	hrtimer_cancel(&cm36652_iio->light_timer);
	cancel_work_sync(&cm36652_iio->work_light);
	cm36652_iio->power_state &= ~LIGHT_ENABLED;
}

#ifdef CM36652_CANCELATION
static int proximity_open_cancelation(struct cm36652_data *data)
{
	struct file *cancel_filp = NULL;
	int err = 0;
	u16 buf = 0;
	mm_segment_t old_fs;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cancel_filp = filp_open(CANCELATION_FILE_PATH, O_RDONLY, 0);
	if (IS_ERR(cancel_filp)) {
		err = PTR_ERR(cancel_filp);
		if (err != -ENOENT)
			pr_err("[SENSOR] %s: Can't open cancelation file\n",
				__func__);
		set_fs(old_fs);
		return err;
	}

	err = cancel_filp->f_op->read(cancel_filp, (char *)&buf,
		sizeof(u16), &cancel_filp->f_pos);
	if (err != sizeof(u16)) {
		pr_err("%s: Can't read the cancel data from file\n", __func__);
		err = -EIO;
	}

	if (buf < CAL_SKIP_ADC)
		goto exit;

	ps_reg_init_setting[PS_CANCEL][CMD] = buf;
	/*If there is an offset cal data. */
	if ((ps_reg_init_setting[PS_CANCEL][CMD] != data->pdata->trim)
		&& (ps_reg_init_setting[PS_CANCEL][CMD] != 0)) {
			ps_reg_init_setting[PS_THD][CMD] =
				((data->pdata->cancel_hi_thd << 8) & 0xff00)
				| (data->pdata->cancel_low_thd & 0xff);
	}

exit:
	pr_info("%s prox_cal[%d] PS_THD[%x]\n", __func__,
		ps_reg_init_setting[PS_CANCEL][CMD],
		ps_reg_init_setting[PS_THD][CMD]);

	filp_close(cancel_filp, current->files);
	set_fs(old_fs);

	return err;
}

static int proximity_store_cancelation(struct device *dev, bool do_calib)
{
	struct cm36652_data *cm36652_iio = dev_get_drvdata(dev);
	struct file *cancel_filp = NULL;
	mm_segment_t old_fs;
	int err = 0;
	u16 ps_data = 0;

	if (do_calib) {
		mutex_lock(&cm36652_iio->read_lock);
		cm36652_i2c_read_word(cm36652_iio, REG_PS_DATA, &ps_data);
		ps_reg_init_setting[PS_CANCEL][CMD] = ps_data;
		mutex_unlock(&cm36652_iio->read_lock);

		if (ps_reg_init_setting[PS_CANCEL][CMD] < CAL_SKIP_ADC) {
			ps_reg_init_setting[PS_CANCEL][CMD] = cm36652_iio->pdata->trim;
			pr_info("%s:crosstalk <= %d SKIP!!\n", __func__, CAL_SKIP_ADC);
			cm36652_iio->uProxCalResult = 2;
			err = 1;
		} else if (ps_reg_init_setting[PS_CANCEL][CMD] < CAL_FAIL_ADC) {
			ps_reg_init_setting[PS_CANCEL][CMD] =
				cm36652_iio->pdata->trim + ps_data;
			pr_info("%s:crosstalk_offset = %u Canceled", __func__,
				ps_reg_init_setting[PS_CANCEL][CMD]);
			cm36652_iio->uProxCalResult = 1;
			err = 0;
			ps_reg_init_setting[PS_THD][CMD] =
				((cm36652_iio->pdata->cancel_hi_thd<< 8) & 0xff00)
				| (cm36652_iio->pdata->cancel_low_thd & 0xff);
		} else {
			ps_reg_init_setting[PS_CANCEL][CMD] = cm36652_iio->pdata->trim;
			pr_info("%s:crosstalk >= %d\n FAILED!!", __func__, CAL_FAIL_ADC);
			ps_reg_init_setting[PS_THD][CMD] =
				((cm36652_iio->pdata->default_hi_thd << 8) & 0xff00)
				| (cm36652_iio->pdata->default_low_thd & 0xff);
			cm36652_iio->uProxCalResult = 0;
			err = 1;
		}
	} else { /* reset */
		ps_reg_init_setting[PS_CANCEL][CMD] = cm36652_iio->pdata->trim;
		ps_reg_init_setting[PS_THD][CMD] =
			((cm36652_iio->pdata->default_hi_thd << 8) & 0xff00)
			| (cm36652_iio->pdata->default_low_thd & 0xff);
	}

	err = cm36652_i2c_write_word(cm36652_iio, REG_PS_CANC,
		ps_reg_init_setting[PS_CANCEL][CMD]);
	if (err < 0)
		pr_err("%s: cm36652_ps_canc_reg is failed. %d\n", __func__,
			err);

	usleep_range(2900, 3000);
	err = cm36652_i2c_write_word(cm36652_iio, REG_PS_THD,
		ps_reg_init_setting[PS_THD][CMD]);
	if (err < 0)
		pr_err("%s: cm36652_ps_canc_reg is failed. %d\n", __func__,
			err);
	pr_info("%s CalResult[%d] PS_THD[%x]\n", __func__,
		cm36652_iio->uProxCalResult, ps_reg_init_setting[PS_THD][CMD]);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cancel_filp = filp_open(CANCELATION_FILE_PATH,
			O_CREAT | O_TRUNC | O_WRONLY | O_SYNC, 0660);
	if (IS_ERR(cancel_filp)) {
		pr_err("[SENSOR] %s: Can't open cancelation file\n", __func__);
		set_fs(old_fs);
		err = PTR_ERR(cancel_filp);
		return err;
	}

	err = cancel_filp->f_op->write(cancel_filp,
		(char *)&ps_reg_init_setting[PS_CANCEL][CMD],
		sizeof(u16), &cancel_filp->f_pos);
	if (err != sizeof(u16)) {
		pr_err("%s: Can't write the cancel data to file\n", __func__);
		err = -EIO;
	}

	filp_close(cancel_filp, current->files);
	set_fs(old_fs);

	if (!do_calib) /* delay for clearing */
		msleep(150);

	return err;
}

static ssize_t proximity_cancel_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	bool do_calib;
	int err;

	if (sysfs_streq(buf, "1")) /* calibrate cancelation value */
		do_calib = true;
	else if (sysfs_streq(buf, "0")) /* reset cancelation value */
		do_calib = false;
	else {
		pr_debug("[SENSOR] %s: invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	err = proximity_store_cancelation(dev, do_calib);
	if (err < 0) {
		pr_err("[SENSOR] %s: proximity_store_cancelation() failed\n",
			__func__);
		return err;
	}

	return size;
}

static ssize_t proximity_cancel_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_info("cm366 %s = %u,%u,%u\n",__func__, ps_reg_init_setting[PS_CANCEL][CMD],
	ps_reg_init_setting[PS_THD][CMD] >> 8, ps_reg_init_setting[PS_THD][CMD] &0x00ff);

	return snprintf(buf, PAGE_SIZE, "%u,%u,%u\n", ps_reg_init_setting[PS_CANCEL][CMD],
	ps_reg_init_setting[PS_THD][CMD] >> 8, ps_reg_init_setting[PS_THD][CMD] &0x00ff);
}
static ssize_t proximity_trim_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct cm36652_data *cm36652_iio = dev_get_drvdata(dev);
	pr_info("%s, %u\n", __func__, cm36652_iio->pdata->trim);
	return snprintf(buf, PAGE_SIZE, "%u\n", cm36652_iio->pdata->trim);
}

static ssize_t proximity_cancel_pass_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct cm36652_data *cm36652_iio = dev_get_drvdata(dev);

	pr_info("%s, %u\n", __func__, cm36652_iio->uProxCalResult);
	return snprintf(buf, PAGE_SIZE, "%u\n", cm36652_iio->uProxCalResult);
}
#endif

static void cm36652_prox_enable(struct cm36652_data *cm36652_iio)
{
	int i;
	int err = 0;
	u8 val;
	u16 ps_data = 0;

	pr_info("%s\n", __func__);
	cm36652_iio->power_state |= PROXIMITY_ENABLED;
//	cm36652_led_onoff(cm36652_iio,1);
	cm36652_leden_gpio_onoff(cm36652_iio, 1);
#ifdef CM36652_CANCELATION
	/* open cancelation data */
	err = proximity_open_cancelation(cm36652_iio);
	if (err < 0 && err != -ENOENT)
		pr_err("[SENSOR] %s: proximity_open_cancelation() failed\n",
			__func__);
#endif
	/* enable settings */
	for (i = 0; i < PS_REG_NUM; i++) {
		cm36652_i2c_write_word(cm36652_iio,
			ps_reg_init_setting[i][REG_ADDR],
			ps_reg_init_setting[i][CMD]);
	}

	val = gpio_get_value(cm36652_iio->pdata->irq);
	if (val == 1) {/* far */
		cm36652_i2c_read_word(cm36652_iio, REG_PS_DATA, &ps_data);
		/* Check adc value */
		if (ps_data < cm36652_iio->pdata->default_hi_thd) {
			cm36652_iio->proximity_detection = val;
			cm36652_prox_data_rdy_trig_poll(cm36652_iio->indio_dev_prox);
			wake_lock_timeout(&cm36652_iio->prx_wake_lock, 3 * HZ);
			pr_info("%s: ps_data = %u\n", __func__, ps_data);
		} else
			pr_info("%s:ps_data = %u, skip!!\n", __func__, ps_data);
	}

	/* 0 is close, 1 is far */
	enable_irq(cm36652_iio->irq);
	enable_irq_wake(cm36652_iio->irq);
}

static void cm36652_prox_disable(struct cm36652_data *cm36652_iio)
{
	pr_info("%s\n", __func__);
	cm36652_iio->power_state &= ~PROXIMITY_ENABLED;
//	cm36652_led_onoff(cm36652_iio,0);
	cm36652_leden_gpio_onoff(cm36652_iio, 0);
	disable_irq_wake(cm36652_iio->irq);
	disable_irq(cm36652_iio->irq);
	/* disable settings */
	cm36652_i2c_write_word(cm36652_iio, REG_PS_CONF1, 0x0001);

}

/* sysfs for vendor & name */
static ssize_t cm36652_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR);
}

static ssize_t cm36652_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", CHIP_ID);
}
static struct device_attribute dev_attr_prox_sensor_vendor =
	__ATTR(vendor, S_IRUSR | S_IRGRP, cm36652_vendor_show, NULL);
static struct device_attribute dev_attr_light_sensor_vendor =
	__ATTR(vendor, S_IRUSR | S_IRGRP, cm36652_vendor_show, NULL);
static struct device_attribute dev_attr_prox_sensor_name =
	__ATTR(name, S_IRUSR | S_IRGRP, cm36652_name_show, NULL);
static struct device_attribute dev_attr_light_sensor_name =
	__ATTR(name, S_IRUSR | S_IRGRP, cm36652_name_show, NULL);

/* proximity sysfs */
static ssize_t proximity_avg_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct cm36652_data *cm36652_iio = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n", cm36652_iio->avg[0],
		cm36652_iio->avg[1], cm36652_iio->avg[2]);
}

static ssize_t proximity_avg_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct cm36652_data *cm36652_iio = dev_get_drvdata(dev);
	bool new_value = false;

	if (sysfs_streq(buf, "1"))
		new_value = true;
	else if (sysfs_streq(buf, "0"))
		new_value = false;
	else {
		pr_err("[SENSOR] %s, invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	pr_info("[SENSOR] %s, average enable = %d\n", __func__, new_value);
	mutex_lock(&cm36652_iio->power_lock);
	if (new_value) {
		if (!(cm36652_iio->power_state & PROXIMITY_ENABLED)) {
			cm36652_i2c_write_word(cm36652_iio, REG_PS_CONF1,
				ps_reg_init_setting[PS_CONF1][CMD]);
		}
		hrtimer_start(&cm36652_iio->prox_timer, cm36652_iio->prox_poll_delay,
			HRTIMER_MODE_REL);
	} else if (!new_value) {
		hrtimer_cancel(&cm36652_iio->prox_timer);
		cancel_work_sync(&cm36652_iio->work_prox);
		if (!(cm36652_iio->power_state & PROXIMITY_ENABLED)) {
			cm36652_i2c_write_word(cm36652_iio, REG_PS_CONF1,
				0x0001);
		}
	}
	mutex_unlock(&cm36652_iio->power_lock);

	return size;
}

static ssize_t proximity_state_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct cm36652_data *cm36652_iio = dev_get_drvdata(dev);
	u16 ps_data;

	mutex_lock(&cm36652_iio->power_lock);
	if (!(cm36652_iio->power_state & PROXIMITY_ENABLED)) {
		pr_info("%s prox_led_on\n", __func__);
		cm36652_i2c_write_word(cm36652_iio, REG_PS_CONF1,
			ps_reg_init_setting[PS_CONF1][CMD]);
	}

	mutex_lock(&cm36652_iio->read_lock);
	cm36652_i2c_read_word(cm36652_iio, REG_PS_DATA, &ps_data);
	mutex_unlock(&cm36652_iio->read_lock);

	if (!(cm36652_iio->power_state & PROXIMITY_ENABLED)) {
		cm36652_i2c_write_word(cm36652_iio, REG_PS_CONF1, 0x0001);
	}
	mutex_unlock(&cm36652_iio->power_lock);

	return snprintf(buf, PAGE_SIZE, "%u\n", ps_data);
}

static ssize_t proximity_thresh_high_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int high, low;

	high = ps_reg_init_setting[PS_THD][CMD] >> 8;
	low = ps_reg_init_setting[PS_THD][CMD] &0x00ff;
	pr_info("cm366 %s = %u,%u\n", __func__, high, low);
	return snprintf(buf, PAGE_SIZE, "%u,%u\n", high, low);
}

static ssize_t proximity_thresh_high_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct cm36652_data *cm36652_iio = dev_get_drvdata(dev);
	u16 thresh_value;
	int err;

	err = kstrtou16(buf, 10, &thresh_value);
	if (err < 0)
		pr_err("%s, kstrtoint failed.\n", __func__);
	pr_info("%s, thresh_value:%u\n", __func__, thresh_value);

	if (thresh_value > 2) {
		ps_reg_init_setting[PS_THD][CMD] =
			(ps_reg_init_setting[PS_THD][CMD] & 0xff) \
			| ((thresh_value << 8) & 0xff00);
		err = cm36652_i2c_write_word(cm36652_iio, REG_PS_THD,
			ps_reg_init_setting[PS_THD][CMD]);
		if (err < 0)
			pr_err("%s: cm36652_ps_high_reg is failed. %d\n",
				__func__, err);
		pr_info("%s, new high threshold = 0x%x\n",
			__func__, thresh_value);
		msleep(150);
	} else
		pr_err("%s, wrong high threshold value(0x%x)!!\n",
			__func__, thresh_value);

	return size;
}

static ssize_t proximity_thresh_low_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int high, low;

	high = ps_reg_init_setting[PS_THD][CMD] >> 8;
	low = ps_reg_init_setting[PS_THD][CMD] &0x00ff;
	pr_info("%s = %u,%u\n", __func__, high, low);

	return snprintf(buf, PAGE_SIZE, "%u,%u\n", high, low);
}

static ssize_t proximity_thresh_low_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct cm36652_data *cm36652_iio = dev_get_drvdata(dev);
	u16 thresh_value;
	int err;

	err = kstrtou16(buf, 10, &thresh_value);
	if (err < 0)
		pr_err("%s, kstrtoint failed.", __func__);
	pr_info("%s, thresh_value:%u\n", __func__, thresh_value);

	if (thresh_value > 2) {
		ps_reg_init_setting[PS_THD][CMD] =
			(ps_reg_init_setting[PS_THD][CMD] & 0xff00) \
			| (thresh_value & 0x00ff) ;
		err = cm36652_i2c_write_word(cm36652_iio, REG_PS_THD,
			ps_reg_init_setting[PS_THD][CMD]);
		if (err < 0)
			pr_err("%s: cm36652_ps_low_reg is failed. %d\n",
				__func__, err);
		pr_info("%s, new low threshold = 0x%x\n",
			__func__, thresh_value);
		msleep(150);
	} else
		pr_err("%s, wrong low threshold value(0x%x)!!\n",
			__func__, thresh_value);

	return size;
}

#ifdef CM36652_CANCELATION
static DEVICE_ATTR(prox_cal, S_IRUGO | S_IWUSR | S_IWGRP,
	proximity_cancel_show, proximity_cancel_store);
static DEVICE_ATTR(prox_trim, S_IRUGO, proximity_trim_show, NULL);
static DEVICE_ATTR(prox_offset_pass, S_IRUGO, proximity_cancel_pass_show, NULL);
#endif
static DEVICE_ATTR(prox_avg, S_IRUGO | S_IWUSR | S_IWGRP,
	proximity_avg_show, proximity_avg_store);
static DEVICE_ATTR(state, S_IRUGO, proximity_state_show, NULL);
static struct device_attribute dev_attr_prox_raw = __ATTR(raw_data,
	S_IRUGO, proximity_state_show, NULL);
static DEVICE_ATTR(thresh_high, S_IRUGO | S_IWUSR | S_IWGRP,
	proximity_thresh_high_show, proximity_thresh_high_store);
static DEVICE_ATTR(thresh_low, S_IRUGO | S_IWUSR | S_IWGRP,
	proximity_thresh_low_show, proximity_thresh_low_store);

static struct device_attribute *prox_sensor_attrs[] = {
	&dev_attr_prox_sensor_vendor,
	&dev_attr_prox_sensor_name,
	&dev_attr_prox_cal,
	&dev_attr_prox_trim,
	&dev_attr_prox_offset_pass,
	&dev_attr_prox_avg,
	&dev_attr_state,
	&dev_attr_thresh_high,
	&dev_attr_thresh_low,
	&dev_attr_prox_raw,
	NULL,
};

/* light sysfs */
static ssize_t light_lux_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct cm36652_data *cm36652_iio = dev_get_drvdata(dev);

	cm36652_i2c_read_word(cm36652_iio, REG_ALS_RED_DATA,
			&cm36652_iio->als_red_data);
	cm36652_i2c_read_word(cm36652_iio, REG_ALS_GREEN_DATA,
			&cm36652_iio->als_green_data);
	cm36652_i2c_read_word(cm36652_iio, REG_ALS_BLUE_DATA,
			&cm36652_iio->als_blue_data);
	cm36652_i2c_read_word(cm36652_iio, REG_WHITE_DATA,
			&cm36652_iio->white_data);

	return snprintf(buf, PAGE_SIZE, "%u,%u,%u,%u\n",
		cm36652_iio->als_red_data, cm36652_iio->als_green_data,
		cm36652_iio->als_blue_data, cm36652_iio->white_data);
}

static ssize_t light_data_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct cm36652_data *cm36652_iio = dev_get_drvdata(dev);

	cm36652_i2c_read_word(cm36652_iio, REG_ALS_RED_DATA,
			&cm36652_iio->als_red_data);
	cm36652_i2c_read_word(cm36652_iio, REG_ALS_GREEN_DATA,
			&cm36652_iio->als_green_data);
	cm36652_i2c_read_word(cm36652_iio, REG_ALS_BLUE_DATA,
			&cm36652_iio->als_blue_data);
	cm36652_i2c_read_word(cm36652_iio, REG_WHITE_DATA,
			&cm36652_iio->white_data);

	return snprintf(buf, PAGE_SIZE, "%u,%u,%u,%u\n",
		cm36652_iio->als_red_data, cm36652_iio->als_green_data,
		cm36652_iio->als_blue_data, cm36652_iio->white_data);
}

static DEVICE_ATTR(lux, S_IRUGO, light_lux_show, NULL);
static DEVICE_ATTR(raw_data, S_IRUGO, light_data_show, NULL);

static struct device_attribute *light_sensor_attrs[] = {
	&dev_attr_light_sensor_vendor,
	&dev_attr_light_sensor_name,
	&dev_attr_lux,
	&dev_attr_raw_data,
	NULL,
};

static const struct iio_chan_spec cm36652_prox_channels[] = {
	CM36652_PROX_CHANNEL(),
	IIO_CHAN_SOFT_TIMESTAMP(CM36652_SCAN_PROX_TIMESTAMP)
};

static const struct iio_chan_spec cm36652_light_channels[] = {
	CM36652_LIGHT_CHANNEL(RED),
	CM36652_LIGHT_CHANNEL(GREEN),
	CM36652_LIGHT_CHANNEL(BLUE),
	CM36652_LIGHT_CHANNEL(CLEAR),
	IIO_CHAN_SOFT_TIMESTAMP(CM36652_SCAN_LIGHT_TIMESTAMP)
};

static int cm36652_read_raw_prox(struct iio_dev *indio_dev,
	struct iio_chan_spec const *chan, int *val, int *val2, long mask)
{
	struct cm36652_sensor_data *sdata = iio_priv(indio_dev);
	struct cm36652_data *cm36652_iio = sdata->cdata;
	int ret = -EINVAL;

	if (chan->type != IIO_PROXIMITY)
		return -EINVAL;

	pr_info(" %s\n", __func__);
	mutex_lock(&cm36652_iio->lock);

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

	mutex_unlock(&cm36652_iio->lock);

	return ret;
}

static int cm36652_read_raw_light(struct iio_dev *indio_dev,
	struct iio_chan_spec const *chan, int *val, int *val2, long mask)
{

	struct cm36652_sensor_data *sdata = iio_priv(indio_dev);
	struct cm36652_data *cm36652_iio = sdata->cdata;
	int ret = -EINVAL;

	pr_info(" %s\n", __func__);

	if (chan->type != IIO_LIGHT)
		return -EINVAL;

	mutex_lock(&cm36652_iio->lock);

	switch (mask) {
	case 0:
		*val = cm36652_iio->color[chan->channel2 - IIO_MOD_LIGHT_RED] ;
		ret = IIO_VAL_INT;
		break;
	case IIO_CHAN_INFO_SCALE:
		/* Gain : counts / uT = 1000 [nT] */
		/* Scaling factor : 1000000 / Gain = 1000 */
		*val = 0;
		*val2 = 1000;
		ret = IIO_VAL_INT_PLUS_MICRO;
		break;
	}

	mutex_unlock(&cm36652_iio->lock);

	return ret;
}

static ssize_t cm36652_light_sampling_frequency_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct cm36652_sensor_data *sdata = iio_priv(indio_dev);
	struct cm36652_data *cm36652_iio = sdata->cdata;

	return sprintf(buf, "%d\n", (int)cm36652_iio->sampling_frequency_light);
}

static ssize_t cm36652_light_sampling_frequency_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct cm36652_sensor_data *sdata = iio_priv(indio_dev);
	struct cm36652_data *cm36652_iio = sdata->cdata;

	int ret, data;
	int delay;
	ret = kstrtoint(buf, 10, &data);
	if (ret)
		return ret;
	if (data <= 0)
		return -EINVAL;
	mutex_lock(&cm36652_iio->lock);
	cm36652_iio->sampling_frequency_light = data;
	delay = MSEC_PER_SEC / cm36652_iio->sampling_frequency_light;
	// has to be checked about delay
	cm36652_iio->light_delay = delay;
	cm36652_iio->light_poll_delay = ns_to_ktime(delay*NSEC_PER_MSEC);
	mutex_unlock(&cm36652_iio->lock);
	return count;
}

/* iio light sysfs - sampling frequency */
static IIO_DEVICE_ATTR(sampling_frequency, S_IRUSR|S_IWUSR,
		cm36652_light_sampling_frequency_show,
		cm36652_light_sampling_frequency_store, 0);

static struct attribute *cm36652_iio_light_attributes[] = {
	&iio_dev_attr_sampling_frequency.dev_attr.attr,
	NULL
};

static const struct attribute_group cm36652_iio_light_attribute_group = {
	.attrs = cm36652_iio_light_attributes,
};

static const struct iio_info cm36652_prox_info= {

	.read_raw = &cm36652_read_raw_prox,
	.driver_module = THIS_MODULE,

};

/* iio prox sysfs - sampling frequency */
static const struct iio_info cm36652_light_info= {
	.read_raw = &cm36652_read_raw_light,
	.attrs = &cm36652_iio_light_attribute_group,
	.driver_module = THIS_MODULE,
};

static irqreturn_t cm36652_light_trigger_handler(int irq, void *p)
{
	struct iio_poll_func *pf = p;
	struct iio_dev *indio_dev = pf->indio_dev;
	struct cm36652_sensor_data *sdata = iio_priv(indio_dev);
	struct cm36652_data *cm36652_iio = sdata->cdata;

	int len = 0, i, j;
	int ret = 0;
	int32_t *data;

	data = (int32_t *) kmalloc(indio_dev->scan_bytes, GFP_KERNEL);
	if (data == NULL)
		goto done;

	if (!bitmap_empty(indio_dev->active_scan_mask, indio_dev->masklength)){
		j = 0;
		for (i = 0; i < 4; i++) {
			if (test_bit(i, indio_dev->active_scan_mask)) {
				data[j] = cm36652_iio->color[i];
				j++;
			}
		}
		len = j * 4;
	}

	/* Guaranteed to be aligned with 8 byte boundary */
	if (indio_dev->scan_timestamp)
		*(s64 *)((u8 *)data + ALIGN(len, sizeof(s64))) = pf->timestamp;
	ret = iio_push_to_buffers(indio_dev, (u8 *)data);
	if (ret < 0)
		pr_err("%s, iio_push buffer failed = %d\n", __func__, ret);
	kfree(data);

done:
	iio_trigger_notify_done(indio_dev->trig);
	return IRQ_HANDLED;
}

static irqreturn_t cm36652_prox_trigger_handler(int irq, void *p)
{
	struct iio_poll_func *pf = p;
	struct iio_dev *indio_dev = pf->indio_dev;
	struct cm36652_sensor_data *sdata = iio_priv(indio_dev);
	struct cm36652_data *cm36652_iio = sdata->cdata;

	int len = 0;
	int ret = 0;
	int32_t *data;

	data = (int32_t *) kmalloc(indio_dev->scan_bytes, GFP_KERNEL);
	if (data == NULL)
		goto done;

	if (!bitmap_empty(indio_dev->active_scan_mask, indio_dev->masklength)) {
		/* TODO : data update */
		if (!cm36652_iio->proximity_detection)
			*data = 0;
		else
			*data = 1;
	}

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

/* interrupt happened due to transition/change of near/far proximity state */
irqreturn_t cm36652_irq_thread_fn(int irq, void *data)
{
	struct cm36652_data *cm36652_iio = data;
	u8 val;
	u16 ps_data = 0;

	val = gpio_get_value(cm36652_iio->pdata->irq);
	cm36652_i2c_read_word(cm36652_iio, REG_PS_DATA, &ps_data);

#ifdef CONFIG_SENSORS_CM36652_DEFENCE_CODE_FOR_NOISE
	if (((ps_data < (ps_reg_init_setting[PS_THD][CMD] & 0x00ff))
		&& (val == 0)) || ((val == 1)
		&& (ps_data > ps_reg_init_setting[PS_THD][CMD] >> 8))
		|| (ps_data > 256)) {
		pr_err("[SENSOR] %s: exception case! val = %d, ps_data = %d\n",
			__func__, val, ps_data);
		return IRQ_HANDLED;
	}
#endif
	cm36652_iio->proximity_detection = val;
	cm36652_prox_data_rdy_trig_poll(cm36652_iio->indio_dev_prox);
	wake_lock_timeout(&cm36652_iio->prx_wake_lock, 3 * HZ);

	pr_info("[SENSOR] %s: val = %u, ps_data = %u (close:0, far:1)\n",
		__func__, val, ps_data);
	return IRQ_HANDLED;
}

static int cm36652_setup_reg(struct cm36652_data *cm36652_iio)
{
	int err = 0, i = 0;
	u16 tmp = 0;

	/* ALS initialization */
	err = cm36652_i2c_write_word(cm36652_iio,
			als_reg_setting[0][0],
			als_reg_setting[0][1]);
	if (err < 0) {
		pr_err("[SENSOR] %s: cm36652_als_reg is failed. %d\n", __func__,
			err);
		return err;
	}
	/* PS initialization */
	for (i = 0; i < PS_REG_NUM; i++) {
		err = cm36652_i2c_write_word(cm36652_iio,
			ps_reg_init_setting[i][REG_ADDR],
			ps_reg_init_setting[i][CMD]);
		if (err < 0) {
			pr_err("[SENSOR] %s: cm36652_ps_reg is failed. %d\n",
				__func__, err);
			return err;
		}
	}

	/* printing the inital proximity value with no contact */
	msleep(50);
	err = cm36652_i2c_read_word(cm36652_iio, REG_PS_DATA, &tmp);
	if (err < 0) {
		pr_err("[SENSOR] %s: read ps_data failed\n", __func__);
		err = -EIO;
	}
	pr_err("%s: initial proximity value = %d\n",
		__func__, tmp);

	/* turn off */
	cm36652_i2c_write_word(cm36652_iio, REG_CS_CONF1, 0x0001);
	cm36652_i2c_write_word(cm36652_iio, REG_PS_CONF1, 0x0001);
	cm36652_i2c_write_word(cm36652_iio, REG_PS_CONF3, 0x0000);

	pr_info("[SENSOR] %s is success.", __func__);
	return err;
}

static int cm36652_setup_irq(struct cm36652_data *cm36652_iio)
{
	int rc;
	struct cm36652_platform_data *pdata = cm36652_iio->pdata;

	rc = gpio_request(pdata->irq, "gpio_proximity_out");
	if (rc < 0) {
		pr_err("%s: gpio %d request failed (%d)\n",
			__func__, pdata->irq, rc);
		return rc;
	}

	rc = gpio_direction_input(pdata->irq);
	if (rc < 0) {
		pr_err("%s: failed to set gpio %d as input (%d)\n",
			__func__, pdata->irq, rc);
		goto err_gpio_direction_input;
	}

	cm36652_iio->irq = gpio_to_irq(pdata->irq);
	rc = request_threaded_irq(cm36652_iio->irq, NULL, cm36652_irq_thread_fn,
		IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
		"proximity_int", cm36652_iio);
	if (rc < 0) {
		pr_err("%s: irq:%d failed for qpio:%d err:%d\n",
			__func__, cm36652_iio->irq, pdata->irq, rc);
		goto err_request_irq;
	}

	/* start with interrupts disabled */
	disable_irq(cm36652_iio->irq);

	pr_err("[SENSOR] %s, dir out success\n", __func__);

	goto done;

err_request_irq:
err_gpio_direction_input:
	gpio_free(pdata->irq);
done:
	return rc;
}

static enum hrtimer_restart cm36652_light_timer_func(struct hrtimer *timer)
{
	struct cm36652_data *cm36652_iio
		= container_of(timer, struct cm36652_data, light_timer);

	queue_work(cm36652_iio->light_wq, &cm36652_iio->work_light);
	hrtimer_forward_now(&cm36652_iio->light_timer,
			cm36652_iio->light_poll_delay);

	return HRTIMER_RESTART;
}

static void cm36652_work_func_light(struct work_struct *work)
{
	struct cm36652_data *cm36652_iio = container_of(work,
					struct cm36652_data, work_light);

	mutex_lock(&cm36652_iio->read_lock);
	cm36652_i2c_read_word(cm36652_iio, REG_ALS_RED_DATA,
			&cm36652_iio->als_red_data);
	cm36652_i2c_read_word(cm36652_iio, REG_ALS_GREEN_DATA,
			&cm36652_iio->als_green_data);
	cm36652_i2c_read_word(cm36652_iio, REG_ALS_BLUE_DATA,
			&cm36652_iio->als_blue_data);
	cm36652_i2c_read_word(cm36652_iio, REG_WHITE_DATA,
			&cm36652_iio->white_data);
	mutex_unlock(&cm36652_iio->read_lock);

	cm36652_iio->color[0] = cm36652_iio->als_red_data;
	cm36652_iio->color[1] = cm36652_iio->als_green_data;
	cm36652_iio->color[2] = cm36652_iio->als_blue_data;
	cm36652_iio->color[3] = cm36652_iio->white_data;

	cm36652_iio->als_data = cm36652_iio->als_green_data;

	cm36652_light_data_rdy_trig_poll(cm36652_iio->indio_dev_light);

	if(cm36652_iio->als_red_data) {
		cm36652_iio->count_esd_time = 0;
	} else if((cm36652_iio->als_red_data ==0) \
			&& (cm36652_iio->count_esd_time>= LIGHT_ESD_TIME)) {
		pr_info("%s, lux=0, reset for ESD\n", __func__);
		/* disable setting */
		cm36652_i2c_write_word(cm36652_iio, REG_CS_CONF1,
				als_reg_setting[1][1]);
		/* enable setting */
		cm36652_i2c_write_word(cm36652_iio, REG_CS_CONF1,
				als_reg_setting[0][1]);
		cm36652_iio->count_esd_time = 0;
	} else if((cm36652_iio->als_red_data==0) \
			&& (cm36652_iio->count_esd_time < LIGHT_ESD_TIME)) {
		cm36652_iio->count_esd_time++;
	}

	if (cm36652_iio->count_log_time >= LIGHT_LOG_TIME) {
		pr_info("%s, %u,%u,%u,%u\n", __func__,
			cm36652_iio->als_red_data, cm36652_iio->als_green_data,
			cm36652_iio->als_blue_data, cm36652_iio->white_data);
		cm36652_iio->count_log_time = 0;
	} else
		cm36652_iio->count_log_time++;

#ifdef cm36652_DEBUG
	pr_info("%s, %u,%u\n", __func__,
		cm36652_iio->als_data, cm36652_iio->white_data);
#endif
}

static void proxsensor_get_avg_val(struct cm36652_data *cm36652_iio)
{
	int min = 0, max = 0, avg = 0;
	int i;
	u16 ps_data = 0;

	for (i = 0; i < PROX_READ_NUM; i++) {
		msleep(40);
		cm36652_i2c_read_word(cm36652_iio, REG_PS_DATA,
			&ps_data);
		avg += ps_data;

		if (!i)
			min = ps_data;
		else if (ps_data < min)
			min = ps_data;

		if (ps_data > max)
			max = ps_data;
	}
	avg /= PROX_READ_NUM;

	cm36652_iio->avg[0] = min;
	cm36652_iio->avg[1] = avg;
	cm36652_iio->avg[2] = max;
}

static void cm36652_work_func_prox(struct work_struct *work)
{
	struct cm36652_data *cm36652_iio = container_of(work,
			struct cm36652_data, work_prox);
	proxsensor_get_avg_val(cm36652_iio);
}

static enum hrtimer_restart cm36652_prox_timer_func(struct hrtimer *timer)
{
	struct cm36652_data *cm36652_iio
			= container_of(timer, struct cm36652_data, prox_timer);
	queue_work(cm36652_iio->prox_wq, &cm36652_iio->work_prox);
	hrtimer_forward_now(&cm36652_iio->prox_timer,
			cm36652_iio->prox_poll_delay);
	return HRTIMER_RESTART;
}

static const struct iio_buffer_setup_ops cm36652_prox_buffer_setup_ops = {
	.preenable = &iio_sw_buffer_preenable,
	.postenable = &iio_triggered_buffer_postenable,
	.predisable = &iio_triggered_buffer_predisable,
};

static const struct iio_buffer_setup_ops cm36652_light_buffer_setup_ops = {
	.preenable = &iio_sw_buffer_preenable,
	.postenable = &iio_triggered_buffer_postenable,
	.predisable = &iio_triggered_buffer_predisable,
};

static int cm36652_prox_probe_buffer(struct iio_dev *indio_dev)
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
	indio_dev->setup_ops = &cm36652_prox_buffer_setup_ops;
	indio_dev->modes |= INDIO_BUFFER_TRIGGERED;

	ret = iio_buffer_register(indio_dev, indio_dev->channels,
			indio_dev->num_channels);
	if (ret)
		goto error_free_buf;
	iio_scan_mask_set(indio_dev, indio_dev->buffer, CM36652_SCAN_PROX_CH);

	pr_err("%s is success\n", __func__);
	return 0;

error_free_buf:
	iio_kfifo_free(indio_dev->buffer);
error_ret:
	return ret;
}

static int cm36652_light_probe_buffer(struct iio_dev *indio_dev)
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
	indio_dev->setup_ops = &cm36652_light_buffer_setup_ops;
	indio_dev->modes |= INDIO_BUFFER_TRIGGERED;

	ret = iio_buffer_register(indio_dev, indio_dev->channels,
			indio_dev->num_channels);
	if (ret)
		goto error_free_buf;
	iio_scan_mask_set(indio_dev, indio_dev->buffer, CM36652_SCAN_LIGHT_CH_RED);
	iio_scan_mask_set(indio_dev, indio_dev->buffer, CM36652_SCAN_LIGHT_CH_GREEN);
	iio_scan_mask_set(indio_dev, indio_dev->buffer, CM36652_SCAN_LIGHT_CH_BLUE);
	iio_scan_mask_set(indio_dev, indio_dev->buffer, CM36652_SCAN_LIGHT_CH_CLEAR);

	pr_err("%s is success\n", __func__);
	return 0;

error_free_buf:
	pr_err("%s, failed \n", __func__);
	iio_kfifo_free(indio_dev->buffer);
error_ret:
	return ret;
}

static int cm36652_prox_pseudo_irq_enable(struct iio_dev *indio_dev)
{
	struct cm36652_sensor_data *sdata = iio_priv(indio_dev);
	struct cm36652_data *cm36652_iio = sdata->cdata;

	if (!atomic_cmpxchg(&cm36652_iio->pseudo_irq_enable_prox, 0, 1)) {
		mutex_lock(&cm36652_iio->lock);
		cm36652_prox_enable(cm36652_iio);
		mutex_unlock(&cm36652_iio->lock);
	}

	return 0;
}

static int cm36652_prox_pseudo_irq_disable(struct iio_dev *indio_dev)
{
	struct cm36652_sensor_data *sdata = iio_priv(indio_dev);
	struct cm36652_data *cm36652_iio = sdata->cdata;

	if (atomic_cmpxchg(&cm36652_iio->pseudo_irq_enable_prox, 1, 0)) {
		mutex_lock(&cm36652_iio->lock);
		cm36652_prox_disable(cm36652_iio);
		mutex_unlock(&cm36652_iio->lock);
	}
	return 0;
}

static int cm36652_light_pseudo_irq_enable(struct iio_dev *indio_dev)
{
	struct cm36652_sensor_data *sdata = iio_priv(indio_dev);
	struct cm36652_data *cm36652_iio = sdata->cdata;

	pr_info("%s\n", __func__);
	if (!atomic_cmpxchg(&cm36652_iio->pseudo_irq_enable_light, 0, 1)) {
		mutex_lock(&cm36652_iio->lock);
		cm36652_light_enable(cm36652_iio);
		mutex_unlock(&cm36652_iio->lock);
	}
	return 0;
}

static int cm36652_light_pseudo_irq_disable(struct iio_dev *indio_dev)
{
	struct cm36652_sensor_data *sdata = iio_priv(indio_dev);
	struct cm36652_data *cm36652_iio = sdata->cdata;

	pr_info("%s\n", __func__);
	if (atomic_cmpxchg(&cm36652_iio->pseudo_irq_enable_light, 1, 0)) {
		mutex_lock(&cm36652_iio->lock);
		cm36652_light_disable(cm36652_iio);
		mutex_unlock(&cm36652_iio->lock);
	}
	return 0;
}

static int cm36652_prox_set_pseudo_irq(struct iio_dev *indio_dev, int enable)
{
	if (enable)
		cm36652_prox_pseudo_irq_enable(indio_dev);
	else
		cm36652_prox_pseudo_irq_disable(indio_dev);
	return 0;
}

static int cm36652_light_set_pseudo_irq(struct iio_dev *indio_dev, int enable)
{
	if (enable)
		cm36652_light_pseudo_irq_enable(indio_dev);
	else
		cm36652_light_pseudo_irq_disable(indio_dev);
	return 0;
}

static int cm36652_data_prox_rdy_trigger_set_state(struct iio_trigger *trig,
		bool state)
{
	struct iio_dev *indio_dev = iio_trigger_get_drvdata(trig);
	cm36652_prox_set_pseudo_irq(indio_dev, state);
	return 0;
}

static int cm36652_data_light_rdy_trigger_set_state(struct iio_trigger *trig,
		bool state)
{
	struct iio_dev *indio_dev = iio_trigger_get_drvdata(trig);
	pr_info("%s, called state = %d\n", __func__, state);
	cm36652_light_set_pseudo_irq(indio_dev, state);
	return 0;
}

static const struct iio_trigger_ops cm36652_prox_trigger_ops = {
	.owner = THIS_MODULE,
	.set_trigger_state = &cm36652_data_prox_rdy_trigger_set_state,
};
static const struct iio_trigger_ops cm36652_light_trigger_ops = {
	.owner = THIS_MODULE,
	.set_trigger_state = &cm36652_data_light_rdy_trigger_set_state,
};


static int cm36652_prox_probe_trigger(struct iio_dev *indio_dev)
{
	int ret;
	struct cm36652_sensor_data *sdata = iio_priv(indio_dev);
	struct cm36652_data *cm36652_iio = sdata->cdata;

	indio_dev->pollfunc = iio_alloc_pollfunc(&cm36652_iio_pollfunc_store_boottime,
			&cm36652_prox_trigger_handler, IRQF_ONESHOT, indio_dev,
			"%s_consumer%d", indio_dev->name, indio_dev->id);

	if (indio_dev->pollfunc == NULL) {
		ret = -ENOMEM;
		goto error_ret;
	}
	cm36652_iio->prox_trig = iio_trigger_alloc("%s-dev%d",
			indio_dev->name,
			indio_dev->id);
	if (!cm36652_iio->prox_trig) {
		ret = -ENOMEM;
		goto error_dealloc_pollfunc;
	}
	cm36652_iio->prox_trig->dev.parent = &cm36652_iio->i2c_client->dev;
	cm36652_iio->prox_trig->ops = &cm36652_prox_trigger_ops;
	iio_trigger_set_drvdata(cm36652_iio->prox_trig, indio_dev);
	ret = iio_trigger_register(cm36652_iio->prox_trig);
	if (ret)
		goto error_free_trig;

	pr_err("%s is success\n", __func__);
	return 0;

error_free_trig:
	iio_trigger_free(cm36652_iio->prox_trig);
error_dealloc_pollfunc:
	iio_dealloc_pollfunc(indio_dev->pollfunc);
error_ret:
	pr_info("%s is failed\n", __func__);
	return ret;
}

static int cm36652_light_probe_trigger(struct iio_dev *indio_dev)
{
	int ret;
	struct cm36652_sensor_data *sdata = iio_priv(indio_dev);
	struct cm36652_data *cm36652_iio = sdata->cdata;

	indio_dev->pollfunc = iio_alloc_pollfunc(&cm36652_iio_pollfunc_store_boottime,
			&cm36652_light_trigger_handler, IRQF_ONESHOT, indio_dev,
			"%s_consumer%d", indio_dev->name, indio_dev->id);

	if (indio_dev->pollfunc == NULL) {
		ret = -ENOMEM;
		goto error_ret;
	}
	cm36652_iio->light_trig = iio_trigger_alloc("%s-dev%d",
			indio_dev->name,
			indio_dev->id);
	if (!cm36652_iio->light_trig) {
		ret = -ENOMEM;
		goto error_dealloc_pollfunc;
	}
	cm36652_iio->light_trig->dev.parent = &cm36652_iio->i2c_client->dev;
	cm36652_iio->light_trig->ops = &cm36652_light_trigger_ops;
	iio_trigger_set_drvdata(cm36652_iio->light_trig, indio_dev);
	ret = iio_trigger_register(cm36652_iio->light_trig);
	if (ret)
		goto error_free_trig;

	pr_err("%s is success\n", __func__);
	return 0;

error_free_trig:
	iio_trigger_free(cm36652_iio->light_trig);
error_dealloc_pollfunc:
	iio_dealloc_pollfunc(indio_dev->pollfunc);
error_ret:
	return ret;
}

static void cm36652_prox_remove_trigger(struct iio_dev *indio_dev)
{
	struct cm36652_sensor_data *sdata = iio_priv(indio_dev);
	struct cm36652_data *cm36652_iio = sdata->cdata;

	iio_trigger_unregister(cm36652_iio->prox_trig);
	iio_trigger_free(cm36652_iio->prox_trig);
	iio_dealloc_pollfunc(indio_dev->pollfunc);
}

static void cm36652_light_remove_trigger(struct iio_dev *indio_dev)
{
	struct cm36652_sensor_data *sdata = iio_priv(indio_dev);
	struct cm36652_data *cm36652_iio = sdata->cdata;

	iio_trigger_unregister(cm36652_iio->light_trig);
	iio_trigger_free(cm36652_iio->light_trig);
	iio_dealloc_pollfunc(indio_dev->pollfunc);
}

static void cm36652_prox_remove_buffer(struct iio_dev *indio_dev)
{
	iio_buffer_unregister(indio_dev);
	iio_kfifo_free(indio_dev->buffer);
}

static void cm36652_light_remove_buffer(struct iio_dev *indio_dev)
{
	iio_buffer_unregister(indio_dev);
	iio_kfifo_free(indio_dev->buffer);
}

#ifdef CONFIG_OF

/* device tree parsing function */
static int cm36652_parse_dt(struct device *dev,
	struct cm36652_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
	enum of_gpio_flags flags;
	int ret;

	pdata->irq = of_get_named_gpio_flags(np, "cm36652,irq_gpio", 0, &flags);
	if (pdata->irq < 0) {
		pr_err("[SENSOR]: %s - get prox_int error\n", __func__);
		return -ENODEV;
	}
	
	pdata->leden_gpio = of_get_named_gpio_flags(np, "cm36652,leden_gpio", 0,
		&flags);
	if (pdata->leden_gpio < 0) {
		pr_err("[SENSOR]: %s - get prox_leden_gpio error\n", __func__);
		return -ENODEV;
	}

	ret = of_property_read_u32(np, "cm36652,default_hi_thd",
	&pdata->default_hi_thd);
	if (ret < 0) {
		pr_err("[SENSOR]: %s - Cannot set default_hi_thd through DTSI error!!\n",
			__func__);
		pdata->default_hi_thd = DEFUALT_HI_THD;
	}

	ret = of_property_read_u32(np, "cm36652,default_low_thd",
		&pdata->default_low_thd);
	if (ret < 0) {
		pr_err("[SENSOR]: %s - Cannot set default_low_thd through DTSI error!!\n",
			__func__);
		pdata->default_low_thd = DEFUALT_LOW_THD;
	}

	ret = of_property_read_u32(np, "cm36652,cancel_hi_thd",
		&pdata->cancel_hi_thd);
	if (ret < 0) {
		pr_err("[SENSOR]: %s - Cannot set cancel_hi_thd through DTSI error!!\n",
			__func__);
		pdata->cancel_hi_thd = CANCEL_HI_THD;
	}

	ret = of_property_read_u32(np, "cm36652,cancel_low_thd",
		&pdata->cancel_low_thd);
	if (ret < 0) {
		pr_err("[SENSOR]: %s - Cannot set cancel_low_thd through DTSI error!!\n",
			__func__);
		pdata->cancel_low_thd = CANCEL_LOW_THD;
	}
	ret = of_property_read_u32(np, "cm36652,trim",
		&pdata->trim);
	if (ret < 0) {
		pr_err("[SENSOR]: %s - Cannot set trim\n",
			__func__);
		pdata->trim = DEFAULT_TRIM;
	}
	ps_reg_init_setting[PS_THD][CMD] =
		((pdata->default_hi_thd << 8) & 0xff00) | (pdata->default_low_thd & 0xff);
#if 0
	if (of_property_read_string(np, "cm36652,regulator_1v8", &pdata->sub_ldo6) < 0)
		pr_err("%s - get sub_ldo6 error\n", __func__);

	if (of_property_read_string(np, "cm36652,regulator_2v8", &pdata->sub_ldo10) < 0)
		pr_err("%s - get sub_ldo10 error\n", __func__);
#endif
	pr_info("%s DefaultTHS[%d/%d] CancelTHD[%d/%d] PS_THD[%x]\n", __func__,
		pdata->default_hi_thd, pdata->default_low_thd,
		pdata->cancel_hi_thd, pdata->cancel_low_thd, ps_reg_init_setting[PS_THD][CMD]);
	return 0;
}
#else
static int cm36652_parse_dt(struct device *dev, struct cm36652_platform_data)
{
	return -ENODEV;
}
#endif
#if 0
static int sensor_power_on_vdd(struct cm36652_platform_data *data, int onoff)
{
	int ret;

	pr_info("%s %s\n", __func__, (onoff) ? "on" : "off");

	data->vio_1p8 = regulator_get(NULL, data->sub_ldo6);
	if (IS_ERR(data->vio_1p8)) {
		pr_err("%s - regulator_get fail\n", __func__);
		goto err_1p8;
	}

	data->vdd_2p8 = regulator_get(NULL, data->sub_ldo10);
	if (IS_ERR(data->vdd_2p8)) {
		pr_err("%s - regulator_get fail\n", __func__);
		goto err_2p8;
	}

	if (onoff == 1) {
		regulator_set_voltage(data->vio_1p8, 1800000, 1800000);
		ret = regulator_enable(data->vio_1p8);
		regulator_set_voltage(data->vdd_2p8, 2800000, 2800000);
		ret = regulator_enable(data->vdd_2p8);

	} else if (onoff == 0) {
		regulator_disable(data->vio_1p8);
		regulator_disable(data->vdd_2p8);
	}
	regulator_put(data->vio_1p8);
	regulator_put(data->vdd_2p8);

	msleep(20);
	return 0;
	
	err_2p8:
		regulator_put(data->vio_1p8);
	err_1p8:
		return -ENODEV;

}
#endif


static int cm36652_light_register(struct cm36652_data *cm36652_iio)
{
	struct cm36652_sensor_data *lignt_p;
	int err;
	int ret = -ENODEV;

	/* iio device register - light*/
	cm36652_iio->indio_dev_light = iio_device_alloc(sizeof(*lignt_p));
	if (!cm36652_iio->indio_dev_light) {
		pr_err("%s, iio_dev_light alloc failed\n", __func__);
		err = -ENOMEM;
		goto err_light_register_failed;
	}

	lignt_p = iio_priv(cm36652_iio->indio_dev_light);
	lignt_p->cdata = cm36652_iio;

	cm36652_iio->indio_dev_light->name = CM36652_LIGHT_NAME;
	cm36652_iio->indio_dev_light->dev.parent = &cm36652_iio->i2c_client->dev;
	cm36652_iio->indio_dev_light->info = &cm36652_light_info;
	cm36652_iio->indio_dev_light->channels = cm36652_light_channels;
	cm36652_iio->indio_dev_light->num_channels = ARRAY_SIZE(cm36652_light_channels);
	cm36652_iio->indio_dev_light->modes = INDIO_DIRECT_MODE;

	cm36652_iio->sampling_frequency_light = 5;
	cm36652_iio->light_delay = MSEC_PER_SEC / cm36652_iio->sampling_frequency_light;

	err = cm36652_light_probe_buffer(cm36652_iio->indio_dev_light);
	if (err) {
		pr_err("%s, light probe buffer failed\n", __func__);
		goto error_free_light_dev;
	}

	err = cm36652_light_probe_trigger(cm36652_iio->indio_dev_light);
	if (err) {
		pr_err("%s, light probe trigger failed\n", __func__);
		goto error_remove_light_buffer;
	}
	err = iio_device_register(cm36652_iio->indio_dev_light);
	if (err) {
		pr_err("%s, light iio register failed\n", __func__);
		goto error_remove_light_trigger;
	}

	/* light_timer settings. we poll for light values using a timer. */
	hrtimer_init(&cm36652_iio->light_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	cm36652_iio->light_poll_delay = ns_to_ktime(200 * NSEC_PER_MSEC);
	cm36652_iio->light_timer.function = cm36652_light_timer_func;

	/* the timer just fires off a work queue request.  we need a thread
	   to read the i2c (can be slow and blocking). */
	cm36652_iio->light_wq = create_singlethread_workqueue("cm36652_light_wq");
	if (!cm36652_iio->light_wq) {
		ret = -ENOMEM;
		pr_err("[SENSOR] %s: could not create light workqueue\n",
			__func__);
		goto err_create_light_workqueue;
	}

	/* this is the thread function we run on the work queue */
	INIT_WORK(&cm36652_iio->work_light, cm36652_work_func_light);

	return 0;

err_create_light_workqueue:
	iio_device_unregister(cm36652_iio->indio_dev_light);
error_remove_light_trigger:
	cm36652_light_remove_trigger(cm36652_iio->indio_dev_light);
error_remove_light_buffer:
	cm36652_light_remove_buffer(cm36652_iio->indio_dev_light);
error_free_light_dev:
	iio_device_free(cm36652_iio->indio_dev_light);
err_light_register_failed:
	return err;
}

static int cm36652_prox_register(struct cm36652_data *cm36652_iio)
{
	struct cm36652_sensor_data *prox_p;
	int err;
	int ret = -ENODEV;

	/* iio device register - prox*/
	cm36652_iio->indio_dev_prox  = iio_device_alloc(sizeof(*prox_p));
	if (!cm36652_iio->indio_dev_prox ) {
		pr_err("%s, iio_dev_prox alloc failed\n", __func__);
		err = -ENOMEM;
		goto err_prox_register_failed;
	}

	prox_p = iio_priv(cm36652_iio->indio_dev_prox );
	prox_p->cdata = cm36652_iio;

	cm36652_iio->indio_dev_prox->name = CM36652_PROX_NAME;
	cm36652_iio->indio_dev_prox->dev.parent = &cm36652_iio->i2c_client->dev;
	cm36652_iio->indio_dev_prox->info = &cm36652_prox_info;
	cm36652_iio->indio_dev_prox->channels = cm36652_prox_channels;
	cm36652_iio->indio_dev_prox->num_channels = ARRAY_SIZE(cm36652_prox_channels);
	cm36652_iio->indio_dev_prox->modes = INDIO_DIRECT_MODE;

	cm36652_iio->sampling_frequency_prox = 5;
	cm36652_iio->prox_delay = MSEC_PER_SEC / cm36652_iio->sampling_frequency_prox;

	/* wake lock init for proximity sensor */
	wake_lock_init(&cm36652_iio->prx_wake_lock, WAKE_LOCK_SUSPEND, "prx_wake_lock");

	/* probe buffer */
	err = cm36652_prox_probe_buffer(cm36652_iio->indio_dev_prox);
	if (err) {
		pr_err("%s, prox probe buffer failed\n", __func__);
		goto error_free_prox_dev;
	}

	/* probe trigger */
	err = cm36652_prox_probe_trigger(cm36652_iio->indio_dev_prox);
	if (err) {
		pr_err("%s, prox probe trigger failed\n", __func__);
		goto error_remove_prox_buffer;
	}
	/* iio device register */
	err = iio_device_register(cm36652_iio->indio_dev_prox);
	if (err) {
		pr_err("%s, prox iio register failed\n", __func__);
		goto error_remove_prox_trigger;
	}


	/* For factory test mode, we use timer to get average proximity data. */
	/* prox_timer settings. we poll for light values using a timer. */
	hrtimer_init(&cm36652_iio->prox_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	cm36652_iio->prox_poll_delay = ns_to_ktime(2000 * NSEC_PER_MSEC);/*2 sec*/
	cm36652_iio->prox_timer.function = cm36652_prox_timer_func;

	/* the timer just fires off a work queue request.  we need a thread
	   to read the i2c (can be slow and blocking). */
	cm36652_iio->prox_wq = create_singlethread_workqueue("cm36652_prox_wq");
	if (!cm36652_iio->prox_wq) {
		ret = -ENOMEM;
		pr_err("[SENSOR] %s: could not create prox workqueue\n",
			__func__);
		goto err_create_prox_workqueue;
	}
	/* this is the thread function we run on the work queue */
	INIT_WORK(&cm36652_iio->work_prox, cm36652_work_func_prox);

	return 0;

err_create_prox_workqueue:
	iio_device_unregister(cm36652_iio->indio_dev_prox);
error_remove_prox_trigger:
	cm36652_prox_remove_trigger(cm36652_iio->indio_dev_prox);
error_remove_prox_buffer:
	cm36652_prox_remove_buffer(cm36652_iio->indio_dev_prox);
error_free_prox_dev:
	iio_device_free(cm36652_iio->indio_dev_prox);
	wake_lock_destroy(&cm36652_iio->prx_wake_lock);
err_prox_register_failed:
	return err;
}

#if 0
static int prox_regulator_onoff(struct device *dev, bool onoff)
{
	struct regulator *vdd;
	struct regulator *vio;
	int ret = 0;

	pr_info("%s %s\n", __func__, (onoff) ? "on" : "off");

	vdd = devm_regulator_get(dev, "cm36652,vdd");
	if (IS_ERR(vdd)) {
		pr_err("%s: cannot get vdd\n", __func__);
		ret = -ENOMEM;
		goto err_vdd;
	} else if (!regulator_get_voltage(vdd)) {
		ret = regulator_set_voltage(vdd, 2850000, 2850000);
	}
	vio = devm_regulator_get(dev, "cm36652,vio");
	if (IS_ERR(vio)) {
		pr_err("%s: cannot get vio\n", __func__);
		ret = -ENOMEM;
		goto err_vio;
	} else if (!regulator_get_voltage(vio)) {
		ret = regulator_set_voltage(vio, 1800000, 1800000);
	}
	if (onoff) {
		ret = regulator_enable(vdd);
		if (ret) {
			pr_err("%s: Failed to enable vdd.\n", __func__);
		}
		msleep(20);
		ret = regulator_enable(vio);
		if (ret) {
			pr_err("%s: Failed to enable vio.\n", __func__);
		}
		msleep(20);
	} else {
		ret = regulator_disable(vdd);
		if (ret) {
			pr_err("%s: Failed to enable vdd.\n", __func__);
		}
		msleep(20);
		ret = regulator_disable(vio);
		if (ret) {
			pr_err("%s: Failed to enable vio.\n", __func__);
		}
		msleep(20);
	}

	devm_regulator_put(vio);
err_vio:
	devm_regulator_put(vdd);
err_vdd:
	return ret;
}
#endif

static int cm36652_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret = -ENODEV;
	struct cm36652_data *cm36652_iio = NULL;
	struct cm36652_platform_data *pdata = NULL;

	pr_info("[SENSOR] %s: Probe Start!\n", __func__);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("[SENSOR] %s: i2c functionality check failed!\n",
			__func__);
		return ret;
	}

	cm36652_iio = kzalloc(sizeof(struct cm36652_data), GFP_KERNEL);
	if (!cm36652_iio) {
		pr_err("[SENSOR] %s: failed to alloc memory for RGB sensor module data\n",
			__func__);
		return -ENOMEM;
	}

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
		sizeof(struct cm36652_platform_data), GFP_KERNEL);
		if (!pdata) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			kfree(cm36652_iio);
			return -ENOMEM;
		}
		ret = cm36652_parse_dt(&client->dev, pdata);
		if (ret)
			goto err_devicetree;
	} else{
		pdata = client->dev.platform_data;
	}


	i2c_set_clientdata(client, cm36652_iio);

	cm36652_iio->i2c_client = client;
	cm36652_iio->pdata = pdata;

	/* setup leden_gpio */
	ret = cm36652_setup_leden_gpio(cm36652_iio);
	if (ret) {
		pr_err("%s: could not setup leden_gpio\n", __func__);
		goto err_setup_leden_gpio;
	}
	cm36652_leden_gpio_onoff(cm36652_iio, 1);

	/* Check if the device is there or not. */
	ret = cm36652_i2c_write_word(cm36652_iio, REG_CS_CONF1, 0x0001);
	if (ret < 0) {
		pr_err("[SENSOR] %s: cm36652 is not connected.(%d)\n", __func__,
			ret);
		goto err_setup_reg;
	}

	/* setup initial registers */
	ret = cm36652_setup_reg(cm36652_iio);
	if (ret < 0) {
		pr_err("[SENSOR] %s: could not setup regs\n", __func__);
		goto err_setup_reg;
	}

	ret = cm36652_light_register(cm36652_iio);
	if (ret < 0)
		goto err_setup_reg;
	ret = cm36652_prox_register(cm36652_iio);
	if (ret < 0)
		goto err_light_unreg;

	spin_lock_init(&cm36652_iio->spin_lock);
	mutex_init(&cm36652_iio->lock);
	mutex_init(&cm36652_iio->read_lock);
	mutex_init(&cm36652_iio->power_lock);

	/* setup irq */

	ret = cm36652_setup_irq(cm36652_iio);
	if (ret) {
		pr_err("%s: could not setup irq\n", __func__);
		goto err_setup_irq;
	}
	cm36652_leden_gpio_onoff(cm36652_iio, 0);

	/* set sysfs for proximity sensor */
	ret = sensors_register(cm36652_iio->proximity_dev,
		cm36652_iio, prox_sensor_attrs, "proximity_sensor");
	if (ret) {
		pr_err("[SENSOR] %s: cound not register proximity sensor device(%d).\n",
			__func__, ret);
		goto prox_sensor_register_failed;
	}
	sensors_create_symlink(&cm36652_iio->indio_dev_prox->dev.kobj,
				"proximity_sensor");

	/* set sysfs for light sensor */
	ret = sensors_register(cm36652_iio->light_dev,
		cm36652_iio, light_sensor_attrs, "light_sensor");
	if (ret) {
		pr_err("[SENSOR] %s: cound not register light sensor device(%d).\n",
			__func__, ret);
		goto light_sensor_register_failed;
	}
	sensors_create_symlink(&cm36652_iio->indio_dev_light->dev.kobj,
				"light_sensor");

	pr_info("[SENSOR] %s is success.\n", __func__);
	goto done;

light_sensor_register_failed:
	sensors_unregister(cm36652_iio->proximity_dev, prox_sensor_attrs);
prox_sensor_register_failed:
	free_irq(cm36652_iio->irq, cm36652_iio);
	gpio_free(cm36652_iio->pdata->irq);
err_setup_irq:
	destroy_workqueue(cm36652_iio->prox_wq);
	iio_device_unregister(cm36652_iio->indio_dev_prox);
	cm36652_prox_remove_trigger(cm36652_iio->indio_dev_prox);
	cm36652_prox_remove_buffer(cm36652_iio->indio_dev_prox);
	iio_device_free(cm36652_iio->indio_dev_prox);
	wake_lock_destroy(&cm36652_iio->prx_wake_lock);
	mutex_destroy(&cm36652_iio->lock);
	mutex_destroy(&cm36652_iio->read_lock);
	mutex_destroy(&cm36652_iio->power_lock);
err_light_unreg:
	destroy_workqueue(cm36652_iio->light_wq);
	iio_device_unregister(cm36652_iio->indio_dev_light);
	cm36652_light_remove_trigger(cm36652_iio->indio_dev_light);
	cm36652_light_remove_buffer(cm36652_iio->indio_dev_light);
	iio_device_free(cm36652_iio->indio_dev_light);
err_setup_reg:
	cm36652_leden_gpio_onoff(cm36652_iio, 0);
	gpio_free(cm36652_iio->pdata->leden_gpio);
err_setup_leden_gpio:
err_devicetree:
	kfree(cm36652_iio);
	pr_err("[SENSOR] error in device tree");
done:
	return ret;
}

static int cm36652_i2c_remove(struct i2c_client *client)
{
	struct cm36652_data *cm36652_iio = i2c_get_clientdata(client);

	/* free irq */
	if (cm36652_iio->power_state & PROXIMITY_ENABLED) {
		disable_irq_wake(cm36652_iio->irq);
		disable_irq(cm36652_iio->irq);
	}
	free_irq(cm36652_iio->irq, cm36652_iio);
	gpio_free(cm36652_iio->pdata->irq);

	/* device off */
	if (cm36652_iio->power_state & LIGHT_ENABLED)
		cm36652_light_disable(cm36652_iio);
	if (cm36652_iio->power_state & PROXIMITY_ENABLED) {
		cm36652_i2c_write_word(cm36652_iio, REG_PS_CONF1, 0x0001);
	}

	/* destroy workqueue */
	destroy_workqueue(cm36652_iio->light_wq);
	destroy_workqueue(cm36652_iio->prox_wq);

	/* sysfs destroy */
	sensors_unregister(cm36652_iio->light_dev, light_sensor_attrs);
	sensors_unregister(cm36652_iio->proximity_dev, prox_sensor_attrs);

	/* lock destroy */
	mutex_destroy(&cm36652_iio->lock);
	mutex_destroy(&cm36652_iio->read_lock);
	mutex_destroy(&cm36652_iio->power_lock);
	wake_lock_destroy(&cm36652_iio->prx_wake_lock);

	kfree(cm36652_iio);

	return 0;
}

static void cm36652_i2c_shutdown(struct i2c_client *client)
{
	struct cm36652_data *cm36652_iio = i2c_get_clientdata(client);

	pr_info("[SENSOR]: %s\n", __func__);
	if (cm36652_iio->power_state & LIGHT_ENABLED)
		cm36652_light_disable(cm36652_iio);
	if (cm36652_iio->power_state & PROXIMITY_ENABLED)
		cm36652_prox_disable(cm36652_iio);
}

static int cm36652_suspend(struct device *dev)
{
	/* We disable power only if proximity is disabled.  If proximity
	   is enabled, we leave power on because proximity is allowed
	   to wake up device.  We remove power without changing
	   cm36652_iio->power_state because we use that state in resume.
	 */
	struct cm36652_data *cm36652_iio = dev_get_drvdata(dev);

	if (cm36652_iio->power_state & LIGHT_ENABLED) {
		pr_info("%s is called.\n", __func__);

		/* disable setting */
		cm36652_i2c_write_word(cm36652_iio, REG_CS_CONF1,
			als_reg_setting[1][1]);
		hrtimer_cancel(&cm36652_iio->light_timer);
		cancel_work_sync(&cm36652_iio->work_light);
	}

	return 0;
}

static int cm36652_resume(struct device *dev)
{
	struct cm36652_data *cm36652_iio = dev_get_drvdata(dev);

	if (cm36652_iio->power_state & LIGHT_ENABLED) {
		pr_info("%s is called.\n", __func__);

		/* enable setting */
		cm36652_i2c_write_word(cm36652_iio, REG_CS_CONF1,
			als_reg_setting[0][1]);
		hrtimer_start(&cm36652_iio->light_timer,
		ns_to_ktime(200 * NSEC_PER_MSEC), HRTIMER_MODE_REL);
	}

	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id cm36652_match_table[] = {
	{ .compatible = "cm36652",},
	{},
};
#else
#define cm36652_match_table NULL
#endif


static const struct i2c_device_id cm36652_device_id[] = {
	{"cm36652", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, cm36652_device_id);

static const struct dev_pm_ops cm36652_pm_ops = {
	.suspend = cm36652_suspend,
	.resume = cm36652_resume
};

static struct i2c_driver cm36652_i2c_driver = {
	.driver = {
		   .name = "cm36652",
		   .owner = THIS_MODULE,
		   .of_match_table = cm36652_match_table,
		   .pm = &cm36652_pm_ops
	},
	.probe = cm36652_i2c_probe,
	.shutdown = cm36652_i2c_shutdown,
	.remove = cm36652_i2c_remove,
	.id_table = cm36652_device_id,
};

static int __init cm36652_init(void)
{
	return i2c_add_driver(&cm36652_i2c_driver);
}

static void __exit cm36652_exit(void)
{
	i2c_del_driver(&cm36652_i2c_driver);
}

module_init(cm36652_init);
module_exit(cm36652_exit);

MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("RGB Sensor device driver for cm36652");
MODULE_LICENSE("GPL");
