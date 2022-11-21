/* linux/driver/input/misc/gp2a.c
 * Copyright (C) 2010 Samsung Electronics. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/gpio.h>
#include <linux/wakelock.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/uaccess.h>
#include <linux/gp2a.h>

#if defined(CONFIG_MACH_LT02)
#include <mach/mfp-pxa986-lt02.h>
#elif defined(CONFIG_MACH_COCOA7)
#include <mach/mfp-pxa986-cocoa7.h>
#else
#include <mach/mfp-pxa988-aruba.h>
#endif

#include <linux/sensors_head.h>
#include <linux/regulator/machine.h>


/* Note about power vs enable/disable:
 *  The chip has two functions, proximity and ambient light sensing.
 *  There is no separate power enablement to the two functions (unlike
 *  the Capella CM3602/3623).
 *  This module implements two drivers: /dev/proximity and /dev/light.
 *  When either driver is enabled (via sysfs attributes), we give power
 *  to the chip.  When both are disabled, we remove power from the chip.
 *  In suspend, we remove power if light is disabled but not if proximity is
 *  enabled (proximity is allowed to wakeup from suspend).
 *
 *  There are no ioctls for either driver interfaces.  Output is via
 *  input device framework and control via sysfs attributes.
 */


#define gp2a_dbgmsg(str, args...) pr_debug("%s: " str, __func__, ##args)

#if defined(CONFIG_MACH_KYLE) || defined(CONFIG_MACH_ARUBA_TD)
#define GP2AP002X_PROXIMITY_OFFSET
#endif

/* ADDSEL is LOW */
#define REGS_PROX		0x0 /* Read  Only */
#define REGS_GAIN		0x1 /* Write Only */
#define REGS_HYS		0x2 /* Write Only */
#define REGS_CYCLE		0x3 /* Write Only */
#define REGS_OPMOD		0x4 /* Write Only */
#define REGS_CON		0x6 /* Write Only */

#if 0
#if defined(CONFIG_MACH_KYLE_I)
#define PROX_NONDETECT	0x2F
#define PROX_DETECT		0x0F
#elif defined(CONFIG_MACH_KYLE)
#define PROX_NONDETECT	0x40
#define PROX_DETECT		0x20
#elif defined(CONFIG_MACH_ARUBA_OPEN)
#define PROX_NONDETECT	0x2F
#define PROX_DETECT		0x0F
#else
#define PROX_NONDETECT	0x40
#define PROX_DETECT		0x20
#endif
#endif
#define PROX_NONDETECT	0x2F
#define PROX_DETECT		0x0F


#ifdef GP2AP002X_PROXIMITY_OFFSET
#define PROX_NONDETECT_MODE1	0x43
#define PROX_DETECT_MODE1		0x28
#define PROX_NONDETECT_MODE2	0x48
#define PROX_DETECT_MODE2		0x42
#define OFFSET_FILE_PATH	"/data/prox_cal"
#endif

static int nondetect;
static int detect;
/* sensor type */
#define PROXIMITY	1
#define CHIP_DEV_NAME	"GP2AP002"
#define CHIP_DEV_VENDOR	"SHARP"
struct workqueue_struct *prox_wq;

struct gp2a_data;
static struct regulator *prox_3_0;
static struct regulator *proxLed;
enum {
	LIGHT_ENABLED = BIT(0),
	PROXIMITY_ENABLED = BIT(1),
};
/* driver data */
struct gp2a_data {
	struct input_dev *proximity_input_dev;
	struct device *proximity_dev;
	struct gp2a_platform_data *pdata;
	struct i2c_client *i2c_client;
	int irq;
	bool on;
	u8 power_state;
	struct mutex power_lock;
	struct wake_lock prx_wake_lock;
	struct workqueue_struct *wq;
	struct work_struct work_prox;
	char val_state;
#ifdef GP2AP002X_PROXIMITY_OFFSET
	char cal_mode;
#endif
};

int gp2a_i2c_read(struct gp2a_data *gp2a, u8 reg, u8 *val)
{
	int err = 0;
	unsigned char data[2] = {reg, 0};
	int retry = 10;
	struct i2c_msg msg[2] = {};
	struct i2c_client *client = gp2a->i2c_client;

	if ((client == NULL) || (!client->adapter))
		return -ENODEV;

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = data;

	msg[1].addr = client->addr;
	msg[1].flags = 1;
	msg[1].len = 2;
	msg[1].buf = data;

	while (retry--) {
		data[0] = reg;

		err = i2c_transfer(client->adapter, msg, 2);

		if (err >= 0) {
			*val = data[1];
			return 0;
		}
	}
	return err;
}
int gp2a_i2c_write(struct gp2a_data *gp2a, u8 reg, u8 *val)
{
	int err = 0;
	struct i2c_msg msg[1];
	unsigned char data[2];
	int retry = 10;
	struct i2c_client *client = gp2a->i2c_client;

	if ((client == NULL) || (!client->adapter))
		return -ENODEV;

	while (retry--) {
		data[0] = reg;
		data[1] = *val;

		msg->addr = client->addr;
		msg->flags = 0; /* write */
		msg->len = 2;
		msg->buf = data;

		err = i2c_transfer(client->adapter, msg, 1);

		if (err >= 0)
			return 0;
	}
	return err;
}

static ssize_t adc_read(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct gp2a_data *gp2a = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", gp2a->val_state);
}

static ssize_t state_read(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct gp2a_data *gp2a = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", gp2a->val_state);
}

static ssize_t name_read(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", CHIP_DEV_NAME);
}

static ssize_t vendor_read(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", CHIP_DEV_VENDOR);
}


#ifdef GP2AP002X_PROXIMITY_OFFSET
int gp2a_cal_mode_read_file(char *mode)
{
	int err = 0;
	mm_segment_t old_fs;
	struct file *cal_mode_filp = NULL;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_mode_filp = filp_open(OFFSET_FILE_PATH,
		O_RDONLY, 0666);
	if (IS_ERR(cal_mode_filp)) {
		err = PTR_ERR(cal_mode_filp);
		if (err != -ENOENT)
			pr_err("%s: Can't open cal_mode file\n", __func__);
		set_fs(old_fs);
		return err;
	}
	err = cal_mode_filp->f_op->read(cal_mode_filp,
		(char *)&mode,
		sizeof(u8), &cal_mode_filp->f_pos);

	if (err != sizeof(u8)) {
		pr_err("%s: Can't read the cal_mode from file\n",
			__func__);
		filp_close(cal_mode_filp, current->files);
		set_fs(old_fs);
		return -EIO;
	}

	filp_close(cal_mode_filp, current->files);
	set_fs(old_fs);

	return err;
}

static int gp2a_cal_mode_save_file(char mode)
{
	struct file *cal_mode_filp = NULL;
	int err = 0;
	mm_segment_t old_fs;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_mode_filp = filp_open(OFFSET_FILE_PATH,
		O_CREAT | O_TRUNC | O_WRONLY, 0666);
	if (IS_ERR(cal_mode_filp)) {
		pr_err("%s: Can't open cal_mode file\n",
			__func__);
		set_fs(old_fs);
		err = PTR_ERR(cal_mode_filp);
		pr_err("%s: err = %d\n",
			__func__, err);
		return err;
	}

	err = cal_mode_filp->f_op->write(cal_mode_filp,
		(char *)&mode, sizeof(u8), &cal_mode_filp->f_pos);
	if (err != sizeof(u8)) {
		pr_err("%s: Can't read the cal_mode from file\n", __func__);
		err = -EIO;
	}

	filp_close(cal_mode_filp, current->files);
	set_fs(old_fs);

	return err;
}

static ssize_t prox_cal_read(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct gp2a_data *gp2a = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", gp2a->cal_mode);
}

static ssize_t prox_cal_write(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct gp2a_data *gp2a = dev_get_drvdata(dev);
	u8 value;
	int err;

	if (sysfs_streq(buf, "1")) {
		gp2a->cal_mode = 1;
		nondetect = PROX_NONDETECT_MODE1;
		detect = PROX_DETECT_MODE1;
	} else if (sysfs_streq(buf, "2")) {
		gp2a->cal_mode = 2;
		nondetect = PROX_NONDETECT_MODE2;
		detect = PROX_DETECT_MODE2;
	} else if (sysfs_streq(buf, "0")) {
		gp2a->cal_mode = 0;
		nondetect = PROX_NONDETECT;
		detect = PROX_DETECT;
	} else {
		pr_err("%s: invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

#if defined(CONFIG_MACH_KYLE)
	value = 0x00;
#else
	value = 0x08;
#endif
	gp2a_i2c_write(gp2a, REGS_GAIN, &value);
	value = nondetect;
	gp2a_i2c_write(gp2a, REGS_HYS, &value);
	value = 0x04;
	gp2a_i2c_write(gp2a, REGS_CYCLE, &value);
	value = 0x03;
	gp2a_i2c_write(gp2a, REGS_OPMOD, &value);
	value = 0x00;
	gp2a_i2c_write(gp2a, REGS_CON, &value);

	err = gp2a_cal_mode_save_file(gp2a->cal_mode);

	if (err < 0) {
		pr_err("%s: prox_cal_write() failed\n", __func__);
		return err;
	}

	return size;
}
#endif

static DEVICE_ATTR(adc, 0440, adc_read, NULL);
static DEVICE_ATTR(state, 0440, state_read, NULL);
static DEVICE_ATTR(name, 0440, name_read, NULL);
static DEVICE_ATTR(vendor, 0440, vendor_read, NULL);
#ifdef GP2AP002X_PROXIMITY_OFFSET
static DEVICE_ATTR(prox_cal, 0664, prox_cal_read, prox_cal_write);
#endif

static struct device_attribute *proxi_attrs[] = {
	&dev_attr_adc,
	&dev_attr_state,
	&dev_attr_name,
	&dev_attr_vendor,
#ifdef GP2AP002X_PROXIMITY_OFFSET
	&dev_attr_prox_cal,
#endif
	NULL,
};
static void proximity_regulator(int en)
{
	u8 ret;
	if (!prox_3_0) {
		prox_3_0 = regulator_get(NULL, "v_proxy_3v");
		if (IS_ERR(prox_3_0)) {
			pr_err("%s regulator get error!\n", __func__);
			prox_3_0 = NULL;
			return;
		}
	}
	if (!proxLed) {
		proxLed = regulator_get(NULL, "v_proxy_led_3v3");
		if (IS_ERR(proxLed)) {
			pr_err("%s regulator get error!\n", __func__);
			proxLed = NULL;
			return;
		}
	}
	if(en)
	{
		//LDO Power On=============
		regulator_set_voltage(prox_3_0, 3000000, 3000000);
		regulator_enable(prox_3_0);
		msleep(2);

		regulator_set_voltage(proxLed, 3300000, 3300000);
		regulator_enable(proxLed);
		msleep(2);
		printk("[GP2A]prox LDO power on\n");
	}
	else
	{
		ret = regulator_disable(prox_3_0);
		if (ret) {
			printk(KERN_ERR "%s: v_prox_3_0 disable failed (%d)\n",__func__, ret);
		}
		ret = regulator_disable(proxLed);
		if (ret) {
			printk(KERN_ERR "%s: v_proxLed disable failed (%d)\n",__func__, ret);
		}
		printk("[GP2A]prox LDO power off\n");
	}
}
static ssize_t proximity_enable_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct gp2a_data *gp2a = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%d\n",
		       (gp2a->power_state & PROXIMITY_ENABLED) ? 1 : 0);
}

static ssize_t proximity_enable_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	struct gp2a_data *gp2a = dev_get_drvdata(dev);
	bool new_value;
	u8 value;
#ifdef GP2AP002X_PROXIMITY_OFFSET
	u8 err;
#endif

	if (sysfs_streq(buf, "1")) {
		new_value = true;
	} else if (sysfs_streq(buf, "0")) {
		new_value = false;
	} else {
		pr_err("%s: invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	mutex_lock(&gp2a->power_lock);
	gp2a_dbgmsg("new_value = %d, old state = %d\n",
		    new_value,
		    (gp2a->power_state & PROXIMITY_ENABLED) ? 1 : 0);
#ifdef ALPS_DEBUG
	pr_info("[TMP] new_value = %d, old state = %d\n",
			new_value,
			(gp2a->power_state & PROXIMITY_ENABLED) ? 1 : 0);
#endif
	if (new_value && !(gp2a->power_state & PROXIMITY_ENABLED)) {
		pr_info("[TMP] %s, %d\n", __func__, __LINE__);
#ifdef GP2AP002X_PROXIMITY_OFFSET
		err = gp2a_cal_mode_read_file(&gp2a->cal_mode);
		if (err < 0 && err != -ENOENT)
			pr_err("%s: cal_mode file read fail\n", __func__);

		pr_info("%s: mode = %02x\n", __func__, gp2a->cal_mode);
		if (gp2a->cal_mode == 2) {
			nondetect = PROX_NONDETECT_MODE2;
			detect = PROX_DETECT_MODE2;
		} else if (gp2a->cal_mode == 1) {
			nondetect = PROX_NONDETECT_MODE1;
			detect = PROX_DETECT_MODE1;
		} else {
			nondetect = PROX_NONDETECT;
			detect = PROX_DETECT;
		}
#endif
		gp2a->val_state = 1;
		input_report_abs(gp2a->proximity_input_dev,
			ABS_DISTANCE,
			gp2a->val_state);
		input_sync(gp2a->proximity_input_dev);

		gp2a->power_state |= PROXIMITY_ENABLED;
//		gp2a->pdata->power(true);
		proximity_regulator(1);
		msleep(20);

		value = 0x18;
		gp2a_i2c_write(gp2a, REGS_CON, &value);
#if defined(CONFIG_MACH_KYLE)
		value = 0x00;
#else
		value = 0x08;
#endif
		gp2a_i2c_write(gp2a, REGS_GAIN, &value);
		value = nondetect;
		gp2a_i2c_write(gp2a, REGS_HYS, &value);
		value = 0x04;
		gp2a_i2c_write(gp2a, REGS_CYCLE, &value);

		#if defined(CONFIG_MACH_GEIM)
			enable_irq_wake(gp2a->irq);
		#else
			enable_irq_wake(gp2a->irq);
		#endif
		value = 0x03;
		gp2a_i2c_write(gp2a, REGS_OPMOD, &value);
		enable_irq(gp2a->irq);
		value = 0x00;
		gp2a_i2c_write(gp2a, REGS_CON, &value);

	} else if (!new_value && (gp2a->power_state & PROXIMITY_ENABLED)) {
		pr_info("[TMP] %s, %d\n", __func__, __LINE__);
		#if defined(CONFIG_MACH_GEIM)
			disable_irq_wake(gp2a->irq);
		#else
			//disable_irq_wake(gp2a->irq);
		#endif
		disable_irq(gp2a->irq);
		value = 0x02;
		gp2a_i2c_write(gp2a, REGS_OPMOD, &value);
		gp2a->power_state &= ~PROXIMITY_ENABLED;
		//gp2a->pdata->power(false);
		proximity_regulator(0);
	}
	mutex_unlock(&gp2a->power_lock);

	return size;
}

static struct device_attribute dev_attr_proximity_enable =
	__ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
	       proximity_enable_show, proximity_enable_store);

static struct attribute *proximity_sysfs_attrs[] = {
	&dev_attr_proximity_enable.attr,
	NULL
};

static struct attribute_group proximity_attribute_group = {
	.attrs = proximity_sysfs_attrs,
};

/* This function is for light sensor.  It operates every a few seconds.
 * It asks for work to be done on a thread because i2c needs a thread
 * context (slow and blocking) and then reschedules the timer to run again.
 */

static void gp2a_prox_work_func(struct work_struct *work)
{
	struct gp2a_data *gp2a = container_of(work,
		struct gp2a_data, work_prox);
	u8 vo, value;
	if (gp2a->irq != 0) {
		#if defined(CONFIG_MACH_GEIM)
			disable_irq_wake(gp2a->irq);
		#else
			disable_irq_wake(gp2a->irq);
		#endif
		disable_irq(gp2a->irq);
	} else {
		return ;
	}

	gp2a_i2c_read(gp2a, REGS_PROX, &vo);
	vo = 0x01 & vo;
	if (vo == gp2a->val_state) {
		if (!vo) {
			vo = 0x01;
			value = nondetect;
		} else {
			vo = 0x00;
			value = detect;
		}
#ifdef ALPS_DEBUG
		pr_info("%s: %d\n", __func__, gp2a->val_state);
#endif
		gp2a_i2c_write(gp2a, REGS_HYS, &value);
		gp2a->val_state = vo;
	}


	input_report_abs(gp2a->proximity_input_dev,
		ABS_DISTANCE,
		gp2a->val_state);
	input_sync(gp2a->proximity_input_dev);
	msleep(20);

	value = 0x18;
	gp2a_i2c_write(gp2a, REGS_CON, &value);
	if (gp2a->irq != 0) {
		enable_irq(gp2a->irq);
		#if defined(CONFIG_MACH_GEIM)
			enable_irq_wake(gp2a->irq);
		#else
			enable_irq_wake(gp2a->irq);
		#endif
	}
	value = 0x00;
	gp2a_i2c_write(gp2a, REGS_CON, &value);
}

/* interrupt happened due to transition/change of near/far proximity state */
irqreturn_t gp2a_irq_handler(int irq, void *data)
{
	struct gp2a_data *gp2a = data;
	pr_info("%s:  work\n", __func__);
	if (gp2a->irq != -1) {
		schedule_work((struct work_struct *)&gp2a->work_prox);
		wake_lock_timeout(&gp2a->prx_wake_lock, 3*HZ);
	}
	return IRQ_HANDLED;
}

#define GPIO_PS_VOUT mfp_to_gpio(GPIO092_GPIO_92)

static int gp2a_setup_irq(struct gp2a_data *gp2a)
{
	int rc;
	int irq = -1;
	u8 value;

	gp2a_dbgmsg("start\n");
	printk("%s gp2a->i2c_client->irq:%d", __func__,gp2a->i2c_client->irq);

	rc = gpio_request(gp2a->i2c_client->irq, "gpio_proximity_out");
	if (rc < 0) {
		pr_err("%s: gpio %d request failed (%d)\n",
			__func__, gp2a->i2c_client->irq, rc);
		return rc;
	}

	rc = gpio_direction_input(gp2a->i2c_client->irq);
	if (rc < 0) {
		pr_err("%s: failed to set gpio %d as input (%d)\n",
			__func__, gp2a->i2c_client->irq, rc);
		goto err_gpio_direction_input;
	}

	value = 0x18;
	gp2a_i2c_write(gp2a, REGS_CON, &value);
	irq = gpio_to_irq(gp2a->i2c_client->irq);
	rc = request_irq(gp2a->i2c_client->irq,
			 gp2a_irq_handler,
			 IRQF_TRIGGER_FALLING,
			 "proximity_int",
			 gp2a);

	if (rc < 0) {
		pr_err("%s: request_irq(%d) failed for gpio %d (%d)\n",
			__func__, irq,
			gp2a->i2c_client->irq, rc);
		goto err_request_irq;
	} else{
		pr_info("%s: request_irq(%d) success for gpio %d\n",
			__func__, irq, gp2a->i2c_client->irq);
	}
	/* start with interrupts disabled */
	disable_irq(irq);
	gp2a->irq = irq;

	gp2a->val_state = 1;
	gp2a->power_state &= PROXIMITY_ENABLED;
	gp2a_dbgmsg("success\n");

	value = 0x08;
	gp2a_i2c_write(gp2a, REGS_GAIN, &value);
	value = nondetect;
	gp2a_i2c_write(gp2a, REGS_HYS, &value);
	value = 0x04;
	gp2a_i2c_write(gp2a, REGS_CYCLE, &value);
	value = 0x18;
	gp2a_i2c_write(gp2a, REGS_CON, &value);
	value = 0x02;
	gp2a_i2c_write(gp2a, REGS_OPMOD, &value);
	goto done;

err_request_irq:
err_gpio_direction_input:
	gpio_free(gp2a->i2c_client->irq);
done:
	return rc;
}

static int gp2a_i2c_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	int ret = -ENODEV;
	struct input_dev *input_dev;
	struct gp2a_data *gp2a;
//	struct gp2a_platform_data *pdata = client->dev.platform_data;
	u8 value;

	pr_info("[TMP] %s, %d\n", __func__, __LINE__);
#if defined(CONFIG_MACH_KYLE)
	nondetect = PROX_NONDETECT;
	detect = PROX_DETECT;
#else
        nondetect = PROX_NONDETECT;
        detect = PROX_DETECT;
#endif
	pr_info("%s: %02x %02x\n", __func__, nondetect, detect);

#if 0 // no pdata here
	if (!pdata) {
		pr_err("%s: missing pdata!\n", __func__);
		return ret;
	}

	if (!pdata->power) {
		pr_err("%s: incomplete pdata!\n", __func__);
		return ret;
	}

	/* power on gp2a */
	pdata->power(true);
#endif
	proximity_regulator(1);
	msleep(20);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s: i2c functionality check failed!\n", __func__);
		return ret;
	}

	gp2a = kzalloc(sizeof(struct gp2a_data), GFP_KERNEL);
	if (!gp2a) {
		pr_err("%s: failed to alloc memory for module data\n",
		       __func__);
		return -ENOMEM;
	}

//	gp2a->pdata = pdata;
	gp2a->i2c_client = client;
	i2c_set_clientdata(client, gp2a);

	/* wake lock init */
	wake_lock_init(&gp2a->prx_wake_lock, WAKE_LOCK_SUSPEND,
		       "prx_wake_lock");
	mutex_init(&gp2a->power_lock);

	/* allocate proximity input_device */
	input_dev = input_allocate_device();
	if (!input_dev) {
		pr_err("%s: could not allocate input device\n", __func__);
		goto err_input_allocate_device_proximity;
	}

	gp2a->proximity_input_dev = input_dev;
	input_set_drvdata(input_dev, gp2a);
	input_dev->name = "proximity_sensor";
	input_set_capability(input_dev, EV_ABS, ABS_DISTANCE);
	input_set_abs_params(input_dev, ABS_DISTANCE, 0, 1, 0, 0);

	ret = input_register_device(input_dev);
	if (ret < 0) {
		pr_err("%s: could not register input device\n",
			__func__);
		input_free_device(input_dev);
		goto err_input_allocate_device_proximity;
	}

	ret = sysfs_create_group(&input_dev->dev.kobj,
				 &proximity_attribute_group);
	if (ret) {
		pr_err("%s: could not create sysfs group\n", __func__);
		goto err_sysfs_create_group_proximity;
	}

	/* the timer just fires off a work queue request.  we need a thread
	   to read the i2c (can be slow and blocking). */
	INIT_WORK(&gp2a->work_prox, gp2a_prox_work_func);


#if 0
	ret = gp2a_setup_irq(gp2a);

	if (ret) {
		pr_err("%s: could not setup irq\n", __func__);
		goto err_setup_irq;
	}
#endif
//===========================================
printk("%s gp2a->i2c_client->irq:%d", __func__,GPIO_PS_VOUT);

	//gpio_request(GPIO_PS_VOUT, "gpio_proximity_out");
	//gpio_direction_input(GPIO_PS_VOUT);

	ret= gpio_request(GPIO_PS_VOUT, "gpio_proximity_out");
	if (ret < 0) {
		pr_err("%s: gpio mfp_to_gpio(%d) request failed =>(%d)\n",
			__func__, GPIO092_GPIO_92, ret);
		return ret;
	}

	ret= gpio_direction_input(GPIO_PS_VOUT);
	if (ret < 0) {
		pr_err("%s: failed to set gpio %d as input (%d)\n",
			__func__, GPIO_PS_VOUT, ret);
		goto err_setup_irq;
	}

	value = 0x18;
	gp2a_i2c_write(gp2a, REGS_CON, &value);

	gp2a->irq = gpio_to_irq(GPIO_PS_VOUT);
	ret= request_irq(gp2a->irq, gp2a_irq_handler, IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING, "proximity_int", gp2a);
	if (ret < 0) {
		pr_err("%s: request_irq(%d) failed for gpio %d (%d)\n",
			__func__, GPIO_PS_VOUT,
			gp2a->irq, ret);
		goto err_setup_irq;
	} else{
		pr_info("%s: request_irq(%d) success for gpio %d\n",
			__func__, GPIO_PS_VOUT, gp2a->irq);
	}
	/* start with interrupts disabled */
	disable_irq(gp2a->irq);

	gp2a->val_state = 1;
	gp2a->power_state &= PROXIMITY_ENABLED;
	gp2a_dbgmsg("success\n");

	value = 0x08;
	gp2a_i2c_write(gp2a, REGS_GAIN, &value);
	value = nondetect;
	gp2a_i2c_write(gp2a, REGS_HYS, &value);
	value = 0x04;
	gp2a_i2c_write(gp2a, REGS_CYCLE, &value);
	value = 0x18;
	gp2a_i2c_write(gp2a, REGS_CON, &value);
	value = 0x02;
	gp2a_i2c_write(gp2a, REGS_OPMOD, &value);
//==============================================

#if 1 //not yet ready
	ret = sensors_register(&gp2a->proximity_dev, gp2a,
		proxi_attrs, "proximity_sensor");
	if (ret < 0) {
		pr_info("%s: could not sensors_register\n", __func__);
		goto exit_gp2a_sensors_register;
	}
#endif

	/* set initial proximity value as 1 */
	input_report_abs(gp2a->proximity_input_dev, ABS_DISTANCE, 1);
	input_sync(gp2a->proximity_input_dev);

	pr_info("[TMP] %s, %d\n", __func__, __LINE__);

	//pdata->power(false);
	proximity_regulator(0);
	goto done;

	/* error, unwind it all */
exit_gp2a_sensors_register:
	free_irq(gp2a->irq, gp2a);
	gpio_free(gp2a->i2c_client->irq);
err_setup_irq:
	pr_info("err_setup_irq\n");
	sysfs_remove_group(&gp2a->proximity_input_dev->dev.kobj,
			   &proximity_attribute_group);
err_sysfs_create_group_proximity:
	pr_info("err_sysfs_create_group_proximity\n");
	input_unregister_device(gp2a->proximity_input_dev);
err_input_allocate_device_proximity:
	pr_info("err_input_allocate_device_proximity\n");
	mutex_destroy(&gp2a->power_lock);
	wake_lock_destroy(&gp2a->prx_wake_lock);
	kfree(gp2a);
done:
	pr_info("done\n");
	return ret;
}

static int gp2a_suspend(struct device *dev)
{
	return 0;
}

static int gp2a_resume(struct device *dev)
{

	return 0;
}

static void gp2a_i2c_shutdown(struct i2c_client *client)
{
#if !defined(CONFIG_MACH_KYLE)
	struct gp2a_data *gp2a = i2c_get_clientdata(client);
	if (gp2a != NULL) {
		if (gp2a->power_state & PROXIMITY_ENABLED) {
			disable_irq_wake(gp2a->irq);
			disable_irq(gp2a->irq);
			msleep(20);
		}
		sysfs_remove_group(&gp2a->proximity_input_dev->dev.kobj,
				   &proximity_attribute_group);
		input_unregister_device(gp2a->proximity_input_dev);

		free_irq(gp2a->irq, gp2a);
		gpio_free(gp2a->i2c_client->irq);

		//gp2a->pdata->power(false);
		if (gp2a->power_state & PROXIMITY_ENABLED)
			proximity_regulator(0);

		mutex_destroy(&gp2a->power_lock);

		wake_lock_destroy(&gp2a->prx_wake_lock);

		kfree(gp2a);
	}
#endif
}

static const struct i2c_device_id gp2a_device_id[] = {
	{"gp2a", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, gp2a_device_id);

static const struct dev_pm_ops gp2a_pm_ops = {
	.suspend = gp2a_suspend,
	.resume = gp2a_resume
};

static struct i2c_driver gp2a_i2c_driver = {
	.driver = {
		.name = "gp2a",
		.owner = THIS_MODULE,
		.pm = &gp2a_pm_ops
	},
	.probe		= gp2a_i2c_probe,
	.shutdown	= gp2a_i2c_shutdown,
	.id_table	= gp2a_device_id,
};


static int __init gp2a_init(void)
{
	pr_info("[TMP] %s, %d\n", __func__, __LINE__);
	return i2c_add_driver(&gp2a_i2c_driver);
}

static void __exit gp2a_exit(void)
{
	i2c_del_driver(&gp2a_i2c_driver);
}

module_init(gp2a_init);
module_exit(gp2a_exit);

MODULE_AUTHOR("mjchen@sta.samsung.com");
MODULE_DESCRIPTION("Optical Sensor driver for gp2ap002a00f");
MODULE_LICENSE("GPL");
