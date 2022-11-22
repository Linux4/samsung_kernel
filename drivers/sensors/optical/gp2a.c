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
#include <linux/sensors_core.h>
#include <linux/module.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>
//#include "sensors_head.h"



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

#define GP2AP002X_PROXIMITY_OFFSET

/* ADDSEL is LOW */
#define REGS_PROX		0x0 /* Read  Only */
#define REGS_GAIN		0x1 /* Write Only */
#define REGS_HYS		0x2 /* Write Only */
#define REGS_CYCLE		0x3 /* Write Only */
#define REGS_OPMOD		0x4 /* Write Only */
#define REGS_CON		0x6 /* Write Only */

#define PROX_NONDETECT	0x40
#define PROX_DETECT		0x20


#ifdef GP2AP002X_PROXIMITY_OFFSET
#define PROX_NONDETECT_MODE1	0x43
#define PROX_DETECT_MODE1		0x28
#define PROX_NONDETECT_MODE2	0x48
#define PROX_DETECT_MODE2		0x42
#define OFFSET_FILE_PATH	"/efs/prox_cal"
#endif

static int nondetect;
static int detect;
/* sensor type */
#define PROXIMITY	1
#define CHIP_DEV_NAME	"GP2AP002"
#define CHIP_DEV_VENDOR	"SHARP"
struct workqueue_struct *prox_wq;
struct gp2a_data;
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
	u32  vdd_en ;
	struct regulator *vdd_2p85;
	};
static void sensor_power_on_vdd(struct gp2a_data * , int );

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
	int err = -1;
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
	int ret=0;
#ifdef GP2AP002X_PROXIMITY_OFFSET
	int err;
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
#if defined(CONFIG_MACH_AMAZING_CDMA)
		LED_onoff(1);
#endif
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
		sensor_power_on_vdd(gp2a, 1);
		msleep(20);

		value = 0x18;
		gp2a_i2c_write(gp2a, REGS_CON, &value);
		pr_info("%s: ret = %d, REGS_CON value-%d\n", __func__, ret, value);
#if defined(CONFIG_MACH_KYLE)
		value = 0x00;
#else
		value = 0x08;
#endif
		gp2a_i2c_write(gp2a, REGS_GAIN, &value);
		value = nondetect;
		gp2a_i2c_write(gp2a, REGS_HYS, &value);
		pr_info("%s:  REGS_HYS value-%d\n", __func__, value);
		value = 0x04;
		gp2a_i2c_write(gp2a, REGS_CYCLE, &value);
		#if defined(CONFIG_MACH_GEIM)
			enable_irq_wake(gp2a->irq);
		#else
			enable_irq_wake(gp2a->irq);
		#endif
		value = 0x03;
		ret = gp2a_i2c_write(gp2a, REGS_OPMOD, &value);
		pr_info("%s: ret = %d, REGS_OPMOD value-%d\n", __func__, ret, value);
		enable_irq(gp2a->irq);
		value = 0x00;
		ret = gp2a_i2c_write(gp2a, REGS_CON, &value);
		pr_info("%s: ret = %d\n", __func__, ret);
	} else if (!new_value && (gp2a->power_state & PROXIMITY_ENABLED)) {
		pr_info("[TMP] %s, %d\n", __func__, __LINE__);
#if defined(CONFIG_MACH_AMAZING_CDMA)
		LED_onoff(0);
#endif
		#if defined(CONFIG_MACH_GEIM)
			disable_irq_wake(gp2a->irq);
		#else
			disable_irq_wake(gp2a->irq);
		#endif
		disable_irq(gp2a->irq);
		value = 0x02;
		gp2a_i2c_write(gp2a, REGS_OPMOD, &value);
		gp2a->power_state &= ~PROXIMITY_ENABLED;
		sensor_power_on_vdd(gp2a,0);
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

static int gp2a_setup_irq(struct gp2a_data *gp2a)
{
	int rc = -EIO;
	struct gp2a_platform_data *pdata = gp2a->pdata;
	int irq = -1;
	u8 value;

	gp2a_dbgmsg("start\n");
	rc = gpio_request(pdata->gpio_int, "gpio_proximity_out");
	if (rc < 0) {
		pr_err("%s: gpio %d request failed (%d)\n",
			__func__, pdata->gpio_int, rc);
		return rc;
	}

	rc = gpio_direction_input(pdata->gpio_int);
	if (rc < 0) {
		pr_err("%s: failed to set gpio %d as input (%d)\n",
			__func__, pdata->gpio_int, rc);
		goto err_gpio_direction_input;
	}

	value = 0x18;
	gp2a_i2c_write(gp2a, REGS_CON, &value);
	irq = gpio_to_irq(pdata->gpio_int);
	rc = request_irq(irq,
			 gp2a_irq_handler,
			 IRQF_TRIGGER_FALLING,
			 "proximity_int",
			 gp2a);

	if (rc < 0) {
		pr_err("%s: request_irq(%d) failed for gpio %d (%d)\n",
			__func__, irq,
			pdata->gpio_int, rc);
		goto err_request_irq;
	} else{
		pr_info("%s: request_irq(%d) success for gpio %d\n",
			__func__, irq, pdata->gpio_int);
	}
	/*start with interrupts disabled*/
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
	gpio_free(pdata->gpio_int);
done:
	return rc;
}

static void gp2a_request_gpio(struct gp2a_platform_data *pdata)
{
	int ret;
	ret = gpio_request(pdata->vdd_en, "vdd_en");
	gpio_tlmm_config(GPIO_CFG(pdata->vdd_en, 0,
			GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);
	if (ret) {
		pr_err("[GP2A]%s: unable to request vdd_en [%d]\n",
				__func__, pdata->vdd_en);
		return;
	}
	ret = gpio_direction_output(pdata->vdd_en, 1);
	if (ret)
		pr_err("[GP2A]%s: unable to set_direction for mms_vdd_en [%d]\n",__func__, pdata->vdd_en);
}

#ifdef CONFIG_OF
/*device tree parsing */

static int gp2a_parse_dt(struct device *dev,
struct gp2a_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
	pdata->vdd_en= of_get_named_gpio(np, "gp2a,vdd_en-gpio", 0);
	pdata->prox_cal_path = of_get_property(np, "gp2a,prox_cal_path",NULL);
	pdata->gpio_scl = of_get_named_gpio(np, "gp2a,scl-gpio",	0);
	pdata->gpio_sda = of_get_named_gpio(np, "gp2a,sda-gpio",	0);
	pdata->gpio_int = of_get_named_gpio(np, "gp2a,irq-gpio", 0);
	return 0;
}
#else
static int gp2a_parse_dt(struct device *dev,
		struct gp2a_platform_data *pdata)
	{
		return -ENODEV;
	}
#endif


static void sensor_power_on_vdd(struct gp2a_data *info, int onoff)
{
	int ret;
	if (info->vdd_2p85 == NULL) {
		info->vdd_2p85 =regulator_get(&info->i2c_client->dev, "8226_l19");
		if (IS_ERR(info->vdd_2p85)){
			return ;
		}
		ret = regulator_set_voltage(info->vdd_2p85, 2850000, 2850000);
		if (ret)
			pr_err("%s: error vsensor_2p85 setting voltage ret=%d\n",__func__, ret);
	}
	if (onoff == 1) {
		ret = regulator_enable(info->vdd_2p85);
		if (ret)
			pr_err("%s: error enablinig regulator\n", __func__);
		} else if (onoff == 0) {
		if (regulator_is_enabled(info->vdd_2p85)) {
			ret = regulator_disable(info->vdd_2p85);
				if (ret)
				pr_err("%s: error vsensor_2p85 enabling regulator\n",__func__);
			}
		}
	msleep(30);
	return;
}
static int gp2a_i2c_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	int ret = -ENODEV;
	struct input_dev *input_dev;
	struct gp2a_data *gp2a;
	struct gp2a_platform_data *pdata ;
	u8 vo = -1;
	int dt_err,err;
	pr_info("[GP2A] %s,Start %d\n", __func__, __LINE__);

	nondetect = PROX_NONDETECT;
	detect = PROX_DETECT;
	if(client->dev.of_node) {
		pdata = devm_kzalloc (&client->dev ,
			sizeof(struct gp2a_platform_data), GFP_KERNEL);
		if(!pdata) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		return -ENOMEM;
		}
		dt_err = gp2a_parse_dt(&client->dev, pdata);
		if(dt_err)
			goto err_devicetree;
	} else
		pdata = client->dev.platform_data;
	if (!pdata) {
		pr_err("%s: missing pdata!\n", __func__);
		err = -EINVAL;
		goto done;
	}
	pr_info("%s: %02x %02x\n", __func__, nondetect, detect);
	if (!pdata) {
		pr_err("%s: missing pdata!\n", __func__);
		return ret;
	}

	gp2a = kzalloc(sizeof(struct gp2a_data), GFP_KERNEL);
	if (!gp2a) {
		pr_err("%s: failed to alloc memory for module data\n",
		       __func__);
		return -ENOMEM;
	}
	/* power on gp2a */
	gp2a->pdata = pdata;
	gp2a->i2c_client = client;
	i2c_set_clientdata(client, gp2a);
	gp2a_request_gpio(pdata);
	sensor_power_on_vdd(gp2a,1);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s: i2c functionality check failed!\n", __func__);
		return ret;
	}

	ret = gp2a_i2c_read(gp2a, REGS_PROX, &vo);
	pr_err("%s: gp2a_i2c_read in  val-%d ret-%d\n", __func__, vo, ret);

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
	input_dev->name = "proximity_sensor";
	ret = input_register_device(input_dev);
	if (ret < 0) {
		pr_err("%s: could not register input device\n",
			__func__);
		input_free_device(input_dev);
		goto err_input_allocate_device_proximity;
	}

	gp2a->proximity_input_dev = input_dev;
	input_set_drvdata(input_dev, gp2a);
	input_set_capability(input_dev, EV_ABS, ABS_DISTANCE);
	input_set_abs_params(input_dev, ABS_DISTANCE, 0, 1, 0, 0);

	ret = sysfs_create_group(&input_dev->dev.kobj,
				 &proximity_attribute_group);

	if (ret) {
		pr_err("%s: could not create sysfs group\n", __func__);
		goto err_sysfs_create_group_proximity;
	}

	/* the timer just fires off a work queue request.  we need a thread
	   to read the i2c (can be slow and blocking). */
	INIT_WORK(&gp2a->work_prox, gp2a_prox_work_func);
	ret = gp2a_setup_irq(gp2a);
	if (ret < 0) {
		pr_err("%s: could not setup irq\n", __func__);
		goto err_setup_irq;
	}

	ret = sensors_register(gp2a->proximity_dev, gp2a,
		proxi_attrs, "proximity_sensor");
	if (ret < 0) {
		pr_info("%s: could not sensors_register\n", __func__);
		goto exit_gp2a_sensors_register;
	}

	/* set initial proximity value as 1 */
	input_report_abs(gp2a->proximity_input_dev, ABS_DISTANCE, 1);
	input_sync(gp2a->proximity_input_dev);

	pr_info("[TMP] %s, %d\n", __func__, __LINE__);

	sensor_power_on_vdd(gp2a,0);
	goto done;

	/* error, unwind it all */
exit_gp2a_sensors_register:
	free_irq(gp2a->irq, gp2a);
	gpio_free(gp2a->pdata->gpio_int);
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
err_devicetree:
	kfree(pdata);
done:
	pr_info("[GP2A] %s,Success %d\n", __func__, __LINE__);
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
		gpio_free(gp2a->pdata->gpio_int);

		sensor_power_on_vdd(gp2a,0);

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
#ifdef CONFIG_OF
static struct of_device_id gp2a_match_table[] = {
	{ .compatible = "gp2a,gp2a020",},
	{},
};
#else
#define gp2a_match_table NULL
#endif

static struct i2c_driver gp2a_i2c_driver = {
	.driver = {
		.name = "gp2a",
		.owner = THIS_MODULE,
		.pm = &gp2a_pm_ops,
		.of_match_table = gp2a_match_table,
	},
	.probe		= gp2a_i2c_probe,
	.shutdown	= gp2a_i2c_shutdown,
	.id_table	= gp2a_device_id,
};




module_i2c_driver(gp2a_i2c_driver);

MODULE_AUTHOR("mjchen@sta.samsung.com");
MODULE_DESCRIPTION("Optical Sensor driver for gp2ap002a00f");
MODULE_LICENSE("GPL");
