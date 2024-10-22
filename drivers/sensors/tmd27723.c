/*
 * Copyright (c) 2010 SAMSUNG
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
#include <linux/module.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>
#include <linux/types.h>

#include "sensors_core.h"


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

/* tmd27723 debug */
/*#define DEBUG 1*/
/*#define tmd27723_DEBUG*/

#define tmd27723_dbgmsg(str, args...) pr_info("%s: " str, __func__, ##args)

#define VENDOR_NAME		"tmd27723"
#define CHIP_NAME		"TMD27723"
#define CHIP_ID			0x39

#define GPIO_PROX_LED_EN 8
#define OFFSET_FILE_PATH	"/efs/prox_cal"

/* Triton register offsets */
#define CNTRL				0x00
#define ALS_TIME			0X01
#define PRX_TIME			0x02
#define WAIT_TIME			0x03
#define ALS_MINTHRESHLO			0X04
#define ALS_MINTHRESHHI			0X05
#define ALS_MAXTHRESHLO			0X06
#define ALS_MAXTHRESHHI			0X07
#define PRX_MINTHRESHLO			0X08
#define PRX_MINTHRESHHI			0X09
#define PRX_MAXTHRESHLO			0X0A
#define PRX_MAXTHRESHHI			0X0B
#define INTERRUPT			0x0C
#define PRX_CFG				0x0D
#define PRX_COUNT			0x0E
#define GAIN				0x0F
#define REVID				0x11
#define CHIPID				0x12
#define STATUS				0x13
#define ALS_CHAN0LO			0x14
#define ALS_CHAN0HI			0x15
#define ALS_CHAN1LO			0x16
#define ALS_CHAN1HI			0x17
#define PRX_LO				0x18
#define PRX_HI				0x19
#define PRX_OFFSET			0x1E
#define TEST_STATUS			0x1F

/*Triton cmd reg masks*/
#define CMD_REG				0X80
#define CMD_BYTE_RW			0x00
#define CMD_WORD_BLK_RW			0x20
#define CMD_SPL_FN			0x60
#define CMD_PROX_INTCLR			0X05
#define CMD_ALS_INTCLR			0X06
#define CMD_PROXALS_INTCLR		0X07
#define CMD_TST_REG			0X08
#define CMD_USER_REG			0X09

/* Triton cntrl reg masks */
#define CNTL_REG_CLEAR			0x00
#define CNTL_PROX_INT_ENBL		0X20
#define CNTL_ALS_INT_ENBL		0X10
#define CNTL_WAIT_TMR_ENBL		0X08
#define CNTL_PROX_DET_ENBL		0X04
#define CNTL_ADC_ENBL			0x02
#define CNTL_PWRON			0x01
#define CNTL_ALSPON_ENBL		0x03
#define CNTL_INTALSPON_ENBL		0x13
#define CNTL_PROXPON_ENBL		0x0F
#define CNTL_INTPROXPON_ENBL		0x2F

/* Triton status reg masks */
#define STA_ADCVALID			0x01
#define STA_PRXVALID			0x02
#define STA_ADC_PRX_VALID		0x03
#define STA_ADCINTR			0x10
#define STA_PRXINTR			0x20

#define	MAX_LUX				40000
#define MIN 1

#define ADC_BUFFER_NUM	6
#define PROX_AVG_COUNT	40

enum {
	LIGHT_ENABLED = BIT(0),
	PROXIMITY_ENABLED = BIT(1),
};

#define tmd27723_PROX_MAX			1023
#define tmd27723_PROX_MIN			0
#define OFFSET_ARRAY_LENGTH		10

struct tmd27723_platform_data {
	int als_int;
	void (*power)(bool);
	int (*light_adc_value)(void);
	u32 als_int_flags;
	u32 ldo_gpio_flags;

	int prox_thresh_hi;
	int prox_thresh_low;
	int prox_th_hi_cal;
	int prox_th_low_cal;
	int als_time;
	int intr_filter;
	int prox_pulsecnt;
	int prox_gain;
	int coef_atime;
	int ga;
	int coef_a;
	int coef_b;
	int coef_c;
	int coef_d;
	bool max_data;

	int min_max;
	int en; /*ldo power control*/
};

/* driver data */
struct tmd27723_data {
	struct input_dev *proximity_input_dev;
	struct input_dev *light_input_dev;
	struct tmd27723_platform_data *pdata;
	struct i2c_client *i2c_client;
	struct device *light_dev;
	struct device *proximity_dev;
	int irq;
	struct work_struct work_light;
	struct work_struct work_prox;
	struct work_struct work_prox_avg;
	struct mutex prox_mutex;

	struct hrtimer timer;
	struct hrtimer prox_avg_timer;
	ktime_t light_poll_delay;
	ktime_t prox_polling_time;
	int adc_value_buf[ADC_BUFFER_NUM];
	int adc_index_count;
	bool adc_buf_initialized;
	u8 power_state;
	struct mutex power_lock;
	struct wake_lock prx_wake_lock;
	struct workqueue_struct *wq;
	struct workqueue_struct *wq_avg;
	int avg[3];
	int prox_avg_enable;
	int cleardata;
	int irdata;
/* Auto Calibration */
	u8 initial_offset;
	u8 offset_value;
	int cal_result;
	int threshold_high;
	int threshold_low;
	int proximity_value;
	bool offset_cal_high;
	bool chip_on_success;
	bool is_requested;
	int err_cnt;
};

static int tmd27723_lightsensor_get_adcvalue(struct tmd27723_data *tmd27723);

static void tmd27723_set_prox_offset(struct tmd27723_data *tmd27723, u8 offset);
static int tmd27723_proximity_open_offset(struct tmd27723_data *data);
static int tmd27723_proximity_adc_read(struct tmd27723_data *tmd27723);

static int tmd27723_i2c_write(struct tmd27723_data *tmd27723, u8 reg, u8 *val)
{
	int ret;

	ret = i2c_smbus_write_byte_data(tmd27723->i2c_client,
		(CMD_REG | reg), *val);

	return ret;
}

static int tmd27723_i2c_read(struct tmd27723_data *tmd27723, u8 reg , u8 *val)
{
	int ret;

	i2c_smbus_write_byte(tmd27723->i2c_client, (CMD_REG | reg));
	ret = i2c_smbus_read_byte(tmd27723->i2c_client);
	*val = ret;

	return ret;
}

static int tmd27723_i2c_write_command(struct tmd27723_data *tmd27723, u8 val)
{
	int ret;

	ret = i2c_smbus_write_byte(tmd27723->i2c_client, val);
	pr_info("[tmd27723 Command] val=[0x%x] - ret=[0x%x]\n", val, ret);

	return ret;
}

static void tmd27723_thresh_set(struct tmd27723_data *tmd27723)
{
	int i = 0;
	int ret = 0;
	u8 prox_int_thresh[4] = {0, };

	/* Setting for proximity interrupt */
	if (tmd27723->proximity_value == 1) {
		prox_int_thresh[0] = (tmd27723->threshold_low) & 0xFF;
		prox_int_thresh[1] = (tmd27723->threshold_low >> 8) & 0xFF;
		prox_int_thresh[2] = (0xFFFF) & 0xFF;
		prox_int_thresh[3] = (0xFFFF >> 8) & 0xFF;
	} else if (tmd27723->proximity_value == 0) {
		prox_int_thresh[0] = (0x0000) & 0xFF;
		prox_int_thresh[1] = (0x0000 >> 8) & 0xFF;
		prox_int_thresh[2] = (tmd27723->threshold_high) & 0xff;
		prox_int_thresh[3] = (tmd27723->threshold_high >> 8) & 0xff;
	}

	for (i = 0; i < 4; i++) {
		ret = tmd27723_i2c_write(tmd27723,
			(CMD_REG|(PRX_MINTHRESHLO + i)),
			&prox_int_thresh[i]);
		if (ret < 0)
			pr_info("tmd27723_i2c_write failed, err = %d\n", ret);
	}
}

static int tmd27723_chip_on(struct tmd27723_data *tmd27723)
{
	int i = 0;
	int ret = 0;
	int num_try_init = 0;
	int fail_num = 0;
	u8 temp_val;
	u8 reg_cntrl;
	u8 prox_int_thresh[4];

	for (num_try_init = 0; num_try_init < 3 ; num_try_init++) {
		fail_num = 0;

		temp_val = CNTL_REG_CLEAR;
		ret = tmd27723_i2c_write(tmd27723, (CMD_REG|CNTRL), &temp_val);
		if (ret < 0) {
			pr_info("tmd27723_i2c_write to clr ctrl reg failed\n");
			fail_num++;
			goto err_chipon_i2c_error;
		}

		temp_val = tmd27723->pdata->als_time;
		ret = tmd27723_i2c_write(tmd27723, (CMD_REG|ALS_TIME), &temp_val);
		if (ret < 0) {
			pr_info("tmd27723_i2c_write to als time reg failed\n");
			fail_num++;
			goto err_chipon_i2c_error;
		}

		temp_val = 0xff;
		ret = tmd27723_i2c_write(tmd27723, (CMD_REG|PRX_TIME), &temp_val);
		if (ret < 0) {
			pr_info("tmd27723_i2c_write to prx time reg failed\n");
			fail_num++;
			goto err_chipon_i2c_error;
		}

		temp_val = 0xff;
		ret = tmd27723_i2c_write(tmd27723, (CMD_REG|WAIT_TIME), &temp_val);
		if (ret < 0) {
			pr_info("tmd27723_i2c_write to wait time reg failed\n");
			fail_num++;
			goto err_chipon_i2c_error;
		}

		temp_val = tmd27723->pdata->intr_filter;
		ret = tmd27723_i2c_write(tmd27723, (CMD_REG|INTERRUPT), &temp_val);
		if (ret < 0) {
			pr_info("tmd27723_i2c_write to interrupt reg failed\n");
			fail_num++;
			goto err_chipon_i2c_error;
		}

		temp_val = 0x0;
		ret = tmd27723_i2c_write(tmd27723, (CMD_REG|PRX_CFG), &temp_val);
		if (ret < 0) {
			pr_info("tmd27723_i2c_write to prox cfg reg failed\n");
			fail_num++;
			goto err_chipon_i2c_error;
		}

		temp_val = tmd27723->pdata->prox_pulsecnt;
		ret = tmd27723_i2c_write(tmd27723, (CMD_REG|PRX_COUNT), &temp_val);
		if (ret < 0) {
			pr_info("tmd27723_i2c_write to prox cnt reg failed\n");
			fail_num++;
			goto err_chipon_i2c_error;
		}
		temp_val = tmd27723->pdata->prox_gain;
		ret = tmd27723_i2c_write(tmd27723, (CMD_REG|GAIN), &temp_val);
		if (ret < 0) {
			pr_info("tmd27723_i2c_write to prox gain reg failed\n");
			fail_num++;
			goto err_chipon_i2c_error;
		}

		/* Setting for proximity interrupt */
		prox_int_thresh[0] = 0;
		prox_int_thresh[1] = 0;
		prox_int_thresh[2] = (tmd27723->threshold_high) & 0xff;
		prox_int_thresh[3] = (tmd27723->threshold_high >> 8) & 0xff;

		for (i = 0; i < 4; i++) {
			ret = tmd27723_i2c_write(tmd27723,
				(CMD_REG|(PRX_MINTHRESHLO + i)),
				&prox_int_thresh[i]);
			if (ret < 0) {
				pr_info("tmd27723_i2c_write failed, err = %d\n",
					ret);
				fail_num++;
				goto err_chipon_i2c_error;
			}
		}

		reg_cntrl = CNTL_INTPROXPON_ENBL;
		ret = tmd27723_i2c_write(tmd27723, (CMD_REG|CNTRL), &reg_cntrl);
		if (ret < 0) {
			pr_info("tmd27723_i2c_write to ctrl reg failed\n");
			fail_num++;
			goto err_chipon_i2c_error;
		}
err_chipon_i2c_error:
		msleep(20);
		if (fail_num == 0)
			break;
	}
	if (ret < 0)
		pr_info("%s: chip_on Failed!\n"\
		"fail_num : %d, ret : %d\n", __func__, fail_num, ret);
	return ret;
}

static int tmd27723_chip_off(struct tmd27723_data *tmd27723)
{
	int ret = 0;
	u8 reg_cntrl;

	if (tmd27723->chip_on_success) {
		reg_cntrl = CNTL_REG_CLEAR;
		tmd27723->chip_on_success = false;
		ret = tmd27723_i2c_write(tmd27723, (CMD_REG | CNTRL),
			&reg_cntrl);
		if (ret < 0) {
			pr_info("tmd27723_i2c_write to ctrl reg failed\n");
			return ret;
		}
	} else {
		pr_err("%s: chip is already turn off!\n", __func__);
	}
	return ret;
}

static int tmd27723_get_lux(struct tmd27723_data *tmd27723)
{
	int als_gain = 1;
	int CPL = 0;
	int lux1 = 0, lux2 = 0;
	int irdata = 0;
	int cleardata = 0;
	int calculated_lux = 0;
	u8 prox_int_thresh[4];
	int i;
	int proximity_value = 0;
	int coef_atime = tmd27723->pdata->coef_atime;
	int ga = tmd27723->pdata->ga;
	int coef_a = tmd27723->pdata->coef_a;
	int coef_b = tmd27723->pdata->coef_b;
	int coef_c = tmd27723->pdata->coef_c;
	int coef_d = tmd27723->pdata->coef_d;
	bool max_data = false;

	if (tmd27723->pdata->max_data)
		max_data = tmd27723->pdata->max_data;

	cleardata = i2c_smbus_read_word_data(tmd27723->i2c_client,
		(CMD_REG | ALS_CHAN0LO));
	irdata = i2c_smbus_read_word_data(tmd27723->i2c_client,
		(CMD_REG | ALS_CHAN1LO));
	if (cleardata < 0 || irdata < 0) {
		tmd27723->err_cnt++;
		pr_err("%s: i2c err_cnt:%d. cleardata:%d, irdata:%d\n",
			__func__, tmd27723->err_cnt, cleardata, irdata);
		return -1;
	}
	pr_debug("%s, cleardata = %d, irdata = %d\n",
		__func__, cleardata, irdata);
	tmd27723->cleardata = cleardata;
	tmd27723->irdata = irdata;

	/* calculate lux */
	CPL = (coef_atime * als_gain * 1000) / ga;
	lux1 = (int)((coef_a * cleardata - coef_b * irdata) / CPL);
	lux2 = (int)((coef_c * cleardata - coef_d * irdata) / CPL);

	if (max_data) {	/* Maximum data */
		if (lux1 > lux2)
			calculated_lux = lux1;
		else if (lux2 >= lux1)
			calculated_lux = lux2;
	} else {	/* Minimum data */
		if (lux1 < lux2)
			calculated_lux = lux1;
		else if (lux2 <= lux1)
			calculated_lux = lux2;
	}
	if (calculated_lux < 0)
		calculated_lux = 0;

	/*
	* protection code for abnormal proximity operation
	* under the strong sunlight
	*/
	if (cleardata >= 18000 || irdata >= 18000) {
		calculated_lux = MAX_LUX;
		proximity_value = 0;
		prox_int_thresh[0] = (0x0000) & 0xFF;
		prox_int_thresh[1] = (0x0000 >> 8) & 0xFF;
		prox_int_thresh[2] = (tmd27723->pdata->prox_thresh_hi) & 0xFF;
		prox_int_thresh[3] = (tmd27723->pdata->prox_thresh_hi >> 8)
			& 0xFF;
		for (i = 0; i < 4; i++)
			tmd27723_i2c_write(tmd27723,
				(CMD_REG|(PRX_MINTHRESHLO + i)),
				&prox_int_thresh[i]);
		input_report_abs(tmd27723->proximity_input_dev,
		ABS_DISTANCE, !proximity_value);
		input_sync(tmd27723->proximity_input_dev);
	}

	return calculated_lux;
}

static void tmd27723_light_enable(struct tmd27723_data *tmd27723)
{
	tmd27723_dbgmsg("starting poll timer, delay %lldns\n",
	ktime_to_ns(tmd27723->light_poll_delay));
	tmd27723->err_cnt = 0;
	if (tmd27723->chip_on_success)
		hrtimer_start(&tmd27723->timer, tmd27723->light_poll_delay,
		HRTIMER_MODE_REL);
	else
		pr_err("%s : tmd27723 chip on failed!\n", __func__);
}

static void tmd27723_light_disable(struct tmd27723_data *tmd27723)
{
	tmd27723_dbgmsg("cancelling poll timer\n");
	hrtimer_cancel(&tmd27723->timer);
	cancel_work_sync(&tmd27723->work_light);
}

static ssize_t tmd27723_poll_delay_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct tmd27723_data *tmd27723 = dev_get_drvdata(dev);
	return sprintf(buf, "%lld\n", ktime_to_ns(tmd27723->light_poll_delay));
}


static ssize_t tmd27723_poll_delay_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct tmd27723_data *tmd27723 = dev_get_drvdata(dev);
	int64_t new_delay;
	int err;

	err = kstrtoll(buf, 10, &new_delay);
	if (err < 0)
		return err;

	/* new_delay *= NSEC_PER_MSEC; */

	tmd27723_dbgmsg("new delay = %lldns, old delay = %lldns\n",
		    new_delay, ktime_to_ns(tmd27723->light_poll_delay));
	mutex_lock(&tmd27723->power_lock);
	if (new_delay != ktime_to_ns(tmd27723->light_poll_delay)) {
		tmd27723->light_poll_delay = ns_to_ktime(new_delay);
		if (tmd27723->power_state & LIGHT_ENABLED) {
			tmd27723_light_disable(tmd27723);
			tmd27723_light_enable(tmd27723);
		}
	}
	mutex_unlock(&tmd27723->power_lock);

	return size;
}

static ssize_t tmd27723_light_enable_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct tmd27723_data *tmd27723 = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n",
		(tmd27723->power_state & LIGHT_ENABLED) ? 1 : 0);
}

static ssize_t tmd27723_proximity_enable_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct tmd27723_data *tmd27723 = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n",
		(tmd27723->power_state & PROXIMITY_ENABLED) ? 1 : 0);
}

static void tmd27723_power_enable(struct device *dev, int en)
{

	int rc;
	struct regulator *vdd;
	struct regulator *vio;

	vdd = devm_regulator_get(dev, "tmd27723,vdd");
	if (IS_ERR(vdd)) {
		pr_err("%s: regulator pointer null vdd fail\n", __func__);
	} else {
		rc = regulator_set_voltage(vdd, 2850000, 2850000);
		if (rc)
			printk(KERN_ERR "%s: set_level failed vdd (%d)\n",
			__func__, rc);
	}
	vio = devm_regulator_get(dev, "tmd27723,vio");
	if (IS_ERR(vio)) {
		pr_err("%s: regulator pointer null vio fail\n", __func__);
	} else {
		rc = regulator_set_voltage(vio, 1800000, 1800000);
		if (rc)
			printk(KERN_ERR "%s: set_level failed vio (%d)\n",
			__func__, rc);
	}

	if(en){
		rc = regulator_enable(vdd);
		if (rc) {
			pr_err("%s: Failed to enable regulator vdd.\n",
				__func__);
		}
		rc = regulator_enable(vio);
		if (rc) {
			pr_err("%s: Failed to enable regulator vio.\n",
				__func__);
		}
	}
	else {
		rc = regulator_disable(vdd);
		if (rc) {
			pr_err("%s: Failed to disable regulatorvdd.\n",
				__func__);
		}
		rc = regulator_disable(vio);
		if (rc) {
			pr_err("%s: Failed to disable regulator vio.\n",
				__func__);
		}
	}

	devm_regulator_put(vdd);
	devm_regulator_put(vio);
}

static void tmd27723_request_gpio(struct tmd27723_data *tmd27723)
{
	int ret = 0;
	ret = gpio_request(tmd27723->pdata->en, "prox_en");
	if (ret) {
		tmd27723->is_requested = false;
		pr_err("[tmd27723]%s: unable to request prox_en [%d]\n",
			__func__, tmd27723->pdata->en);
		return;
	} else {
		tmd27723->is_requested = true;
	}
	ret = gpio_direction_output(tmd27723->pdata->en, 0);
	if (ret)
		pr_err("[tmd27723]%s: unable to set_direction for prox_en [%d]\n",
			__func__, tmd27723->pdata->en);
	pr_info("%s: en: %u\n", __func__, tmd27723->pdata->en);
}

static ssize_t tmd27723_light_enable_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t size)
{
	struct tmd27723_data *tmd27723 = dev_get_drvdata(dev);
	bool new_value;

	if (sysfs_streq(buf, "1")) {
		new_value = true;
	} else if (sysfs_streq(buf, "0")) {
		new_value = false;
	} else {
		pr_err("%s: invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	mutex_lock(&tmd27723->power_lock);
	tmd27723_dbgmsg("new_value = %d, old state = %d\n",
		    new_value, (tmd27723->power_state & LIGHT_ENABLED) ? 1 : 0);
	if (new_value && !(tmd27723->power_state & LIGHT_ENABLED)) {
		if (!tmd27723->power_state) {
			tmd27723->chip_on_success
				= (tmd27723_chip_on(tmd27723) >= 0)
				? true : false;
		}
		if (tmd27723->chip_on_success) {
			tmd27723->power_state |= LIGHT_ENABLED;
			tmd27723_light_enable(tmd27723);
		}
	} else if (!new_value && (tmd27723->power_state & LIGHT_ENABLED)) {
		tmd27723_light_disable(tmd27723);
		tmd27723->power_state &= ~LIGHT_ENABLED;
		if (!tmd27723->power_state) {
			tmd27723_chip_off(tmd27723);
		}
	}
	mutex_unlock(&tmd27723->power_lock);
	return size;
}

static ssize_t tmd27723_proximity_enable_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	struct tmd27723_data *tmd27723 = dev_get_drvdata(dev);
	bool new_value;
	int temp = 0, ret = 0;

	if (sysfs_streq(buf, "1")) {
		new_value = true;
	} else if (sysfs_streq(buf, "0")) {
		new_value = false;
	} else {
		pr_err("%s: invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	mutex_lock(&tmd27723->power_lock);
	tmd27723_dbgmsg("new_value = %d, old state = %d\n", new_value,
		(tmd27723->power_state & PROXIMITY_ENABLED) ? 1 : 0);
	if (new_value && !(tmd27723->power_state & PROXIMITY_ENABLED)) {
		if (tmd27723->is_requested)
			gpio_set_value(tmd27723->pdata->en, 1);
		usleep_range(5000, 6000);
		ret = tmd27723_proximity_open_offset(tmd27723);
		if (ret < 0 && ret != -ENOENT)
			pr_err("%s: tmd27723_proximity_open_offset() failed\n",
			__func__);
		/* set prox_threshold from board file */
		if (tmd27723->offset_value != tmd27723->initial_offset) {
			if (tmd27723->pdata->prox_th_hi_cal &&
				tmd27723->pdata->prox_th_low_cal) {
				tmd27723->threshold_high =
					tmd27723->pdata->prox_th_hi_cal;
				tmd27723->threshold_low =
					tmd27723->pdata->prox_th_low_cal;
			}
		}
		pr_err("%s: th_hi = %d, th_low = %d\n", __func__,
			tmd27723->threshold_high, tmd27723->threshold_low);

		tmd27723->power_state |= PROXIMITY_ENABLED;

		/* interrupt clearing */
		temp = (CMD_REG|CMD_SPL_FN|CMD_PROXALS_INTCLR);
		ret = tmd27723_i2c_write_command(tmd27723, temp);
		if (ret < 0)
			pr_info("tmd27723_i2c_write failed, err = %d\n", ret);
		tmd27723->chip_on_success = (tmd27723_chip_on(tmd27723) >= 0) ?
			true : false;

		input_report_abs(tmd27723->proximity_input_dev, ABS_DISTANCE, 1);
		input_sync(tmd27723->proximity_input_dev);

		enable_irq(tmd27723->irq);
		enable_irq_wake(tmd27723->irq);

	} else if (!new_value && (tmd27723->power_state & PROXIMITY_ENABLED)) {
		disable_irq_wake(tmd27723->irq);
		disable_irq(tmd27723->irq);

		tmd27723->power_state &= ~PROXIMITY_ENABLED;
		if (!tmd27723->power_state) {
			tmd27723_chip_off(tmd27723);
		}
		if (tmd27723->is_requested)
			gpio_set_value(tmd27723->pdata->en, 0);
	}
	mutex_unlock(&tmd27723->power_lock);
	return size;
}

static ssize_t tmd27723_proximity_state_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct tmd27723_data *tmd27723 = dev_get_drvdata(dev);
	int adc = 0;

	adc = i2c_smbus_read_word_data(tmd27723->i2c_client,
			CMD_REG | PRX_LO);
	if (adc > tmd27723_PROX_MAX)
		adc = tmd27723_PROX_MAX;

	return sprintf(buf, "%d\n", adc);
}

static void tmd27723_set_prox_offset(struct tmd27723_data *tmd27723, u8 offset)
{
	int ret = 0;

	ret = tmd27723_i2c_write(tmd27723, (CMD_REG|PRX_OFFSET), &offset);
	if (ret < 0)
		pr_info("tmd27723_i2c_write to prx offset reg failed\n");
}

static int tmd27723_proximity_open_offset(struct tmd27723_data *data)
{
	struct file *offset_filp = NULL;
	int err = 0;
	mm_segment_t old_fs;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	offset_filp = filp_open(OFFSET_FILE_PATH, O_RDONLY, 0666);
	if (IS_ERR(offset_filp)) {
		pr_err("%s: no offset file\n", __func__);
		err = PTR_ERR(offset_filp);
		if (err != -ENOENT)
			pr_err("%s: Can't open cancelation file\n", __func__);
		set_fs(old_fs);
		return err;
	}

	err = offset_filp->f_op->read(offset_filp,
		(char *)&data->offset_value, sizeof(u8), &offset_filp->f_pos);
	if (err != sizeof(u8)) {
		pr_err("%s: Can't read the cancel data from file\n", __func__);
		err = -EIO;
	}

	pr_info("%s: data->offset_value = %d\n",
		__func__, data->offset_value);
	tmd27723_set_prox_offset(data, data->offset_value);
	filp_close(offset_filp, current->files);
	set_fs(old_fs);

	return err;
}

static int tmd27723_proximity_adc_read(struct tmd27723_data *tmd27723)
{
	int sum[OFFSET_ARRAY_LENGTH];
	int i = 0;
	int avg = 0;
	int min = 0;
	int max = 0;
	int total = 0;

	mutex_lock(&tmd27723->prox_mutex);
	for (i = 0; i < OFFSET_ARRAY_LENGTH; i++) {
		usleep_range(11000, 11000);
		sum[i] = i2c_smbus_read_word_data(tmd27723->i2c_client,
			CMD_REG | PRX_LO);
		if (i == 0) {
			min = sum[i];
			max = sum[i];
		} else {
			if (sum[i] < min)
				min = sum[i];
			else if (sum[i] > max)
				max = sum[i];
		}
		total += sum[i];
	}
	mutex_unlock(&tmd27723->prox_mutex);
	total -= (min + max);
	avg = (int)(total / (OFFSET_ARRAY_LENGTH - 2));

	return avg;
}

static int tmd27723_proximity_store_offset(struct device *dev, bool do_calib)
{
	struct tmd27723_data *tmd27723 = dev_get_drvdata(dev);
	struct file *offset_filp = NULL;
	mm_segment_t old_fs;
	int err = 0;
	int adc = 0;
	u8 reg_cntrl = 0x25;
	int target_xtalk = 150;
	int offset_change = 0x20;
	bool offset_cal_baseline = true;

	if (do_calib) {
		/* tap offset button */
		pr_info("%s: offset\n", __func__);
		tmd27723->offset_value = 0x3F;
		err = tmd27723_i2c_write(tmd27723, (CMD_REG|CNTRL), &reg_cntrl);
		if (err < 0)
			pr_info("tmd27723_i2c_write to ctrl reg failed\n");

		usleep_range(12000, 15000);
		while (1) {
			adc = tmd27723_proximity_adc_read(tmd27723);
			pr_info("%s: crosstalk = %d\n", __func__, adc);
			if (offset_cal_baseline)  {
				if (adc >= 250) {
					tmd27723->offset_cal_high = true;
				} else {
					tmd27723->offset_cal_high = false;
					tmd27723->offset_value =
						tmd27723->initial_offset;
					break;
				}
				offset_cal_baseline = false;
			} else	{
				if (tmd27723->offset_cal_high) {
					if (adc > target_xtalk) {
						tmd27723->offset_value +=
							offset_change;
					} else {
						tmd27723->offset_value -=
							offset_change;
					}
				} else	{
					if (adc > target_xtalk) {
						tmd27723->offset_value -=
							offset_change;
					} else {
						tmd27723->offset_value +=
							offset_change;
					}
				}
				offset_change = (int)(offset_change / 2);
				if (offset_change == 0)
					break;
			}
			tmd27723_set_prox_offset(tmd27723,
				tmd27723->offset_value);
			pr_info("%s: P_OFFSET = %d, change = %d\n", __func__,
				tmd27723->offset_value, offset_change);
		}
		adc = tmd27723_proximity_adc_read(tmd27723);
		if (tmd27723->offset_value >= 121 &&
			tmd27723->offset_value < 128) {
			tmd27723->cal_result = 0;
			pr_err("%s: cal fail return -1, adc = %d\n",
				__func__, adc);
		} else {
			if (tmd27723->offset_value != tmd27723->initial_offset &&
				tmd27723->offset_cal_high == true) {
				if (tmd27723->pdata->prox_th_hi_cal) {
					tmd27723->threshold_high =
						tmd27723->pdata->prox_th_hi_cal;
				}
				if (tmd27723->pdata->prox_th_low_cal) {
					tmd27723->threshold_low =
						tmd27723->pdata->prox_th_low_cal;
				}
				tmd27723_thresh_set(tmd27723);
				tmd27723->cal_result = 1;
			} else
				tmd27723->cal_result = 2;
		}
		if (tmd27723->offset_cal_high != false)
			tmd27723_set_prox_offset(tmd27723,
				tmd27723->offset_value);
	} else {
	/* tap reset button */
		pr_info("%s: reset\n", __func__);
		tmd27723->threshold_high = tmd27723->pdata->prox_thresh_hi;
		tmd27723->threshold_low = tmd27723->pdata->prox_thresh_low;
		tmd27723_thresh_set(tmd27723);
		tmd27723->offset_value = tmd27723->initial_offset;
		tmd27723_set_prox_offset(tmd27723, tmd27723->offset_value);
		tmd27723->cal_result = 2;
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	offset_filp = filp_open(OFFSET_FILE_PATH,
			O_CREAT | O_TRUNC | O_WRONLY | O_SYNC, 0666);
	if (IS_ERR(offset_filp)) {
		pr_err("%s: Can't open prox_offset file\n", __func__);
		set_fs(old_fs);
		err = PTR_ERR(offset_filp);
		return err;
	}

	err = offset_filp->f_op->write(offset_filp,
		(char *)&tmd27723->offset_value, sizeof(u8),
		&offset_filp->f_pos);
	if (err != sizeof(u8)) {
		pr_err("%s: Can't write the offset data to file\n", __func__);
		err = -EIO;
	}

	filp_close(offset_filp, current->files);
	set_fs(old_fs);
	return err;
}

static ssize_t tmd27723_proximity_cal_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t size)
{
	bool do_calib;
	int err;

	if (sysfs_streq(buf, "1")) { /* calibrate cancelation value */
		do_calib = true;
	} else if (sysfs_streq(buf, "0")) { /* reset cancelation value */
		do_calib = false;
	} else {
		pr_err("%s: invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	err = tmd27723_proximity_store_offset(dev, do_calib);
	if (err < 0) {
		pr_err("%s: tmd27723_proximity_store_offset() failed\n",
			__func__);
		return err;
	}

	return size;
}

static ssize_t tmd27723_proximity_cal_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tmd27723_data *tmd27723 = dev_get_drvdata(dev);
	u8 p_offset = 0;
	int ret = 0;

	msleep(20);
	ret = tmd27723_i2c_read(tmd27723, PRX_OFFSET, &p_offset);
	if (ret < 0)
		pr_err("%s: tmd27723_i2c_read() failed\n", __func__);

	return sprintf(buf, "%d,%d,%d\n",
		p_offset, tmd27723->threshold_high, tmd27723->threshold_low);
}

static ssize_t tmd27723_prox_offset_pass_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tmd27723_data *tmd27723 = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", tmd27723->cal_result);
}

static ssize_t tmd27723_proximity_avg_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{

	struct tmd27723_data *tmd27723 = dev_get_drvdata(dev);
	return sprintf(buf, "%d,%d,%d\n",
		tmd27723->avg[0], tmd27723->avg[1], tmd27723->avg[2]);
}

static ssize_t tmd27723_proximity_avg_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	struct tmd27723_data *tmd27723 = dev_get_drvdata(dev);
	int new_value = 0;

	if (sysfs_streq(buf, "1")) {
		new_value = true;
	} else if (sysfs_streq(buf, "0")) {
		new_value = false;
	} else {
		pr_err("%s: invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	if (tmd27723->prox_avg_enable == new_value)
		tmd27723_dbgmsg("same status\n");
	else if (new_value == 1) {
		tmd27723_dbgmsg("starting poll timer, delay %lldns\n",
		ktime_to_ns(tmd27723->prox_polling_time));
		hrtimer_start(&tmd27723->prox_avg_timer,
			tmd27723->prox_polling_time, HRTIMER_MODE_REL);
		tmd27723->prox_avg_enable = 1;
	} else {
		tmd27723_dbgmsg("cancelling prox avg poll timer\n");
		hrtimer_cancel(&tmd27723->prox_avg_timer);
		cancel_work_sync(&tmd27723->work_prox_avg);
		tmd27723->prox_avg_enable = 0;
	}

	return 1;
}

static ssize_t tmd27723_proximity_thresh_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tmd27723_data *tmd27723 = dev_get_drvdata(dev);
	int thresh_hi = 0;

	msleep(20);
	thresh_hi = i2c_smbus_read_word_data(tmd27723->i2c_client,
		(CMD_REG | PRX_MAXTHRESHLO));

	pr_info("%s: THRESHOLD = %d\n", __func__, thresh_hi);

	return sprintf(buf, "prox_threshold = %d\n", thresh_hi);
}

static ssize_t tmd27723_proximity_thresh_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct tmd27723_data *tmd27723 = dev_get_drvdata(dev);
	int thresh_value = (u8)(tmd27723->pdata->prox_thresh_hi);
	int err = 0;

	err = kstrtoint(buf, 10, &thresh_value);
	if (err < 0)
		pr_err("%s, kstrtoint failed.", __func__);

	tmd27723->threshold_high = thresh_value;
	tmd27723_thresh_set(tmd27723);
	msleep(20);

	return size;
}

static ssize_t tmd27723_get_vendor_name(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", VENDOR_NAME);
}

static ssize_t get_chip_name(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", CHIP_NAME);
}

static DEVICE_ATTR(vendor, S_IRUGO, tmd27723_get_vendor_name, NULL);
static DEVICE_ATTR(name, S_IRUGO, get_chip_name, NULL);

static ssize_t tmd27723_lightsensor_file_state_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tmd27723_data *tmd27723 = dev_get_drvdata(dev);
	int adc = 0;
	adc = tmd27723_lightsensor_get_adcvalue(tmd27723);

	return sprintf(buf, "%d\n", adc);
}

static DEVICE_ATTR(lux, S_IRUGO, tmd27723_lightsensor_file_state_show, NULL);

static ssize_t tmd27723_lightsensor_raw_data_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tmd27723_data *tmd27723 = dev_get_drvdata(dev);
	return sprintf(buf, "%d, %d\n", tmd27723->cleardata, tmd27723->irdata);
}

static struct device_attribute dev_attr_light_raw_data =
	__ATTR(raw_data, S_IRUGO, tmd27723_lightsensor_raw_data_show, NULL);
static struct device_attribute dev_attr_light_enable =
	__ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
	       tmd27723_light_enable_show, tmd27723_light_enable_store);
static DEVICE_ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
		   tmd27723_poll_delay_show, tmd27723_poll_delay_store);

static struct attribute *light_sysfs_attrs[] = {
	&dev_attr_light_enable.attr,
	&dev_attr_poll_delay.attr,
	NULL
};

static struct attribute_group light_attribute_group = {
	.attrs = light_sysfs_attrs,
};

static struct device_attribute *light_sensor_attrs[] = {
	&dev_attr_lux,
	&dev_attr_vendor,
	&dev_attr_name,
	&dev_attr_light_raw_data,
	NULL
};

static struct device_attribute dev_attr_proximity_enable =
	__ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
	       tmd27723_proximity_enable_show, tmd27723_proximity_enable_store);

static struct attribute *proximity_sysfs_attrs[] = {
	&dev_attr_proximity_enable.attr,
	NULL
};

static struct attribute_group proximity_attribute_group = {
	.attrs = proximity_sysfs_attrs,
};

static struct device_attribute dev_attr_proximity_raw_data =
	__ATTR(raw_data, S_IRUGO, tmd27723_proximity_state_show, NULL);

static DEVICE_ATTR(state, S_IRUGO, tmd27723_proximity_state_show, NULL);
static DEVICE_ATTR(prox_avg, S_IRUGO | S_IWUSR, tmd27723_proximity_avg_show,
	tmd27723_proximity_avg_store);
static DEVICE_ATTR(prox_cal, S_IRUGO | S_IWUSR, tmd27723_proximity_cal_show,
	tmd27723_proximity_cal_store);
static DEVICE_ATTR(prox_offset_pass, S_IRUGO,
	tmd27723_prox_offset_pass_show, NULL);
static DEVICE_ATTR(prox_thresh, S_IRUGO | S_IWUSR,
	tmd27723_proximity_thresh_show, tmd27723_proximity_thresh_store);

static struct device_attribute *prox_sensor_attrs[] = {
	&dev_attr_state,
	&dev_attr_proximity_raw_data,
	&dev_attr_prox_avg,
	&dev_attr_prox_cal,
	&dev_attr_prox_offset_pass,
	&dev_attr_prox_thresh,
	&dev_attr_vendor,
	&dev_attr_name,
	NULL
};

static int tmd27723_lightsensor_get_adcvalue(struct tmd27723_data *tmd27723)
{
	int i = 0;
	int j = 0;
	unsigned int adc_total = 0;
	int adc_avr_value;
	unsigned int adc_index = 0;
	unsigned int adc_max = 0;
	unsigned int adc_min = 0;
	int value = 0;

	/* get ADC */
	value = tmd27723_get_lux(tmd27723);
	adc_index = (tmd27723->adc_index_count++) % ADC_BUFFER_NUM;

	/*ADC buffer initialize (light sensor off ---> light sensor on) */
	if (!tmd27723->adc_buf_initialized) {
		tmd27723->adc_buf_initialized = true;
		for (j = 0; j < ADC_BUFFER_NUM; j++)
			tmd27723->adc_value_buf[j] = value;
	} else
		tmd27723->adc_value_buf[adc_index] = value;

	adc_max = tmd27723->adc_value_buf[0];
	adc_min = tmd27723->adc_value_buf[0];

	for (i = 0; i < ADC_BUFFER_NUM; i++) {
		adc_total += tmd27723->adc_value_buf[i];

		if (adc_max < tmd27723->adc_value_buf[i])
			adc_max = tmd27723->adc_value_buf[i];

		if (adc_min > tmd27723->adc_value_buf[i])
			adc_min = tmd27723->adc_value_buf[i];
	}
	adc_avr_value = (adc_total-(adc_max+adc_min))/(ADC_BUFFER_NUM - 2);

	if (tmd27723->adc_index_count == ADC_BUFFER_NUM-1)
		tmd27723->adc_index_count = 0;

	return adc_avr_value;
}


static void tmd27723_work_func_light(struct work_struct *work)
{
	struct tmd27723_data *tmd27723 = container_of(work,
		struct tmd27723_data, work_light);
	int adc = tmd27723_get_lux(tmd27723);
	if (adc < 0 && tmd27723->err_cnt >= 2) {
		tmd27723->power_state &= ~LIGHT_ENABLED;
		if (!tmd27723->power_state) {
			tmd27723_chip_off(tmd27723);
		}
		hrtimer_cancel(&tmd27723->timer);
		return;
	}
	input_report_rel(tmd27723->light_input_dev, REL_MISC, adc+1);
	input_sync(tmd27723->light_input_dev);
}

static void tmd27723_work_func_prox(struct work_struct *work)
{
	struct tmd27723_data *tmd27723 =
		container_of(work, struct tmd27723_data, work_prox);
	u16 adc_data = 0;
	u16 threshold_high;
	u16 threshold_low;
	u8 prox_int_thresh[4];
	int i;
	int proximity_value = 0;

	if (!tmd27723->chip_on_success) {
		pr_err("%s : chip_on failed!\n", __func__);
		return ;
	}

	/* change Threshold */
	mutex_lock(&tmd27723->prox_mutex);
	adc_data = i2c_smbus_read_word_data(tmd27723->i2c_client,
		CMD_REG | PRX_LO);
	mutex_unlock(&tmd27723->prox_mutex);
	threshold_high = i2c_smbus_read_word_data(tmd27723->i2c_client,
		(CMD_REG | PRX_MAXTHRESHLO));
	threshold_low = i2c_smbus_read_word_data(tmd27723->i2c_client,
		(CMD_REG | PRX_MINTHRESHLO));

	pr_info("%s: hi = %d, low = %d\n", __func__,
		tmd27723->threshold_high, tmd27723->threshold_low);

	/*
	* protection code for abnormal proximity operation
	* under the saturation condition
	*/
	if (tmd27723_get_lux(tmd27723) >= 1500) {
		proximity_value = 0;
		input_report_abs(tmd27723->proximity_input_dev,
			ABS_DISTANCE, !proximity_value);
		input_sync(tmd27723->proximity_input_dev);

		pr_info("%s: prox value = %d\n", __func__, !proximity_value);

		prox_int_thresh[0] = (0x0000) & 0xFF;
		prox_int_thresh[1] = (0x0000 >> 8) & 0xFF;
		prox_int_thresh[2] = (tmd27723->threshold_high) & 0xFF;
		prox_int_thresh[3] = (tmd27723->threshold_high >> 8) & 0xFF;

		for (i = 0; i < 4; i++)
			tmd27723_i2c_write(tmd27723,
				(CMD_REG|(PRX_MINTHRESHLO + i)),
				&prox_int_thresh[i]);
	} else if ((threshold_high ==  (tmd27723->threshold_high)) &&
			(adc_data >=  (tmd27723->threshold_high))) {
		proximity_value = 1;
		input_report_abs(tmd27723->proximity_input_dev,
			ABS_DISTANCE, !proximity_value);
		input_sync(tmd27723->proximity_input_dev);

		pr_info("%s: prox value = %d\n", __func__, !proximity_value);

		prox_int_thresh[0] = (tmd27723->threshold_low) & 0xFF;
		prox_int_thresh[1] = (tmd27723->threshold_low >> 8) & 0xFF;
		prox_int_thresh[2] = (0xFFFF) & 0xFF;
		prox_int_thresh[3] = (0xFFFF >> 8) & 0xFF;
		for (i = 0; i < 4; i++)
			tmd27723_i2c_write(tmd27723,
				(CMD_REG|(PRX_MINTHRESHLO + i)),
				&prox_int_thresh[i]);
	} else if ((threshold_high == (0xFFFF)) &&
			(adc_data <= (tmd27723->threshold_low))) {
		proximity_value = 0;
		input_report_abs(tmd27723->proximity_input_dev,
			ABS_DISTANCE, !proximity_value);
		input_sync(tmd27723->proximity_input_dev);

		pr_info("%s: prox value = %d\n", __func__, !proximity_value);

		prox_int_thresh[0] = (0x0000) & 0xFF;
		prox_int_thresh[1] = (0x0000 >> 8) & 0xFF;
		prox_int_thresh[2] = (tmd27723->threshold_high) & 0xFF;
		prox_int_thresh[3] = (tmd27723->threshold_high >> 8) & 0xFF;
		for (i = 0; i < 4; i++)
			tmd27723_i2c_write(tmd27723, (CMD_REG|(PRX_MINTHRESHLO + i)),
				&prox_int_thresh[i]);

	} else {
		pr_err("%s: Error Case!adc=[%X], th_high=[%d], th_min=[%d]\n",
			__func__, adc_data, threshold_high, threshold_low);
	}

	tmd27723->proximity_value = proximity_value;
	/* reset Interrupt pin */
	/* to active Interrupt, TMD2771x Interuupt pin shoud be reset. */
	i2c_smbus_write_byte(tmd27723->i2c_client,
	(CMD_REG|CMD_SPL_FN|CMD_PROXALS_INTCLR));

	/* enable INT */
	enable_irq(tmd27723->irq);
}

static void tmd27723_work_func_prox_avg(struct work_struct *work)
{
	struct tmd27723_data *tmd27723 = container_of(work,
		struct tmd27723_data, work_prox_avg);
	u16 proximity_value = 0;
	int min = 0, max = 0, avg = 0;
	int i = 0;

	for (i = 0; i < PROX_AVG_COUNT; i++) {
		mutex_lock(&tmd27723->prox_mutex);
		proximity_value = i2c_smbus_read_word_data(tmd27723->i2c_client,
			CMD_REG | PRX_LO);
		mutex_unlock(&tmd27723->prox_mutex);
		if (proximity_value > tmd27723_PROX_MIN) {
			if (proximity_value > tmd27723_PROX_MAX)
				proximity_value = tmd27723_PROX_MAX;
			avg += proximity_value;
			if (!i)
				min = proximity_value;
			if (proximity_value < min)
				min = proximity_value;
			if (proximity_value > max)
				max = proximity_value;
		} else {
			proximity_value = tmd27723_PROX_MIN;
		}
		msleep(40);
	}
	avg /= i;
	tmd27723->avg[0] = min;
	tmd27723->avg[1] = avg;
	tmd27723->avg[2] = max;
}


/* This function is for light sensor.  It operates every a few seconds.
 * It asks for work to be done on a thread because i2c needs a thread
 * context (slow and blocking) and then reschedules the timer to run again.
 */
static enum hrtimer_restart tmd27723_timer_func(struct hrtimer *timer)
{
	struct tmd27723_data *tmd27723 = container_of(timer,
		struct tmd27723_data, timer);
	queue_work(tmd27723->wq, &tmd27723->work_light);
	hrtimer_forward_now(&tmd27723->timer, tmd27723->light_poll_delay);
	return HRTIMER_RESTART;
}

static enum hrtimer_restart tmd27723_prox_timer_func(struct hrtimer *timer)
{
	struct tmd27723_data *tmd27723 = container_of(timer,
		struct tmd27723_data, prox_avg_timer);
	queue_work(tmd27723->wq_avg, &tmd27723->work_prox_avg);
	hrtimer_forward_now(&tmd27723->prox_avg_timer,
		tmd27723->prox_polling_time);

	return HRTIMER_RESTART;
}


/* interrupt happened due to transition/change of near/far proximity state */
irqreturn_t tmd27723_irq_handler(int irq, void *data)
{
	struct tmd27723_data *ip = data;

	pr_info("tmd27723 interrupt handler is called\n");
	wake_lock_timeout(&ip->prx_wake_lock, 3*HZ);
	disable_irq_nosync(ip->irq);
	queue_work(ip->wq, &ip->work_prox);

	return IRQ_HANDLED;
}

static int tmd27723_setup_irq(struct tmd27723_data *tmd27723)
{
	int rc;
	struct tmd27723_platform_data *pdata = tmd27723->pdata;
	int irq;

	tmd27723_dbgmsg("start\n");

	rc = gpio_request(pdata->als_int, "gpio_proximity_int");
	if (rc < 0) {
		pr_err("%s: gpio %d request failed (%d)\n",
			__func__, pdata->als_int, rc);
		return rc;
	}

	rc = gpio_direction_input(pdata->als_int);
	if (rc < 0) {
		pr_err("%s: failed to set gpio %d as input (%d)\n",
			__func__, pdata->als_int, rc);
		goto err_gpio_direction_input;
	}

	irq = gpio_to_irq(pdata->als_int);
	rc = request_threaded_irq(irq, NULL,
			 tmd27723_irq_handler,
			 IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
			 "proximity_int",
			 tmd27723);
	if (rc < 0) {
		pr_err("%s: request_irq(%d) failed for gpio %d (%d)\n",
			__func__, irq,
			pdata->als_int, rc);
		goto err_request_irq;
	}

	/* start with interrupts disabled */
	disable_irq(irq);
	tmd27723->irq = irq;

	tmd27723_dbgmsg("success\n");

	goto done;

err_gpio_direction_input:
err_request_irq:
	gpio_free(pdata->als_int);
done:
	return rc;
}

static int tmd27723_get_initial_offset(struct tmd27723_data *tmd27723)
{
	int ret = 0;
	u8 p_offset = 0;

	ret = tmd27723_i2c_read(tmd27723, PRX_OFFSET, &p_offset);
	if (ret < 0)
		pr_err("%s: tmd27723_i2c_read() failed\n", __func__);
	else
		pr_info("%s: initial offset = %d\n", __func__, p_offset);

	return p_offset;
}

/* device tree parsing function */
static int tmd27723_parse_dt(struct device *dev,
			struct  tmd27723_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
	pdata->als_int = of_get_named_gpio_flags(np, "tmd27723,irq_gpio",
		0, &pdata->als_int_flags);
	pr_info("%s: als_int: %u\n", __func__, pdata->als_int);

	pdata->en = of_get_named_gpio_flags(np, "tmd27723,vled",
		0, &pdata->ldo_gpio_flags);
	pr_info("%s: en: %u\n", __func__, pdata->en);

	of_property_read_u32(np, "tmd27723,prox_thresh_hi",
		&pdata->prox_thresh_hi);
	of_property_read_u32(np, "tmd27723,prox_thresh_low",
		&pdata->prox_thresh_low);
	of_property_read_u32(np, "tmd27723,prox_th_hi_cal",
		&pdata->prox_th_hi_cal);
	of_property_read_u32(np, "tmd27723,prox_th_low_cal",
		&pdata->prox_th_low_cal);
	of_property_read_u32(np, "tmd27723,als_time", &pdata->als_time);
	of_property_read_u32(np, "tmd27723,intr_filter", &pdata->intr_filter);
	of_property_read_u32(np, "tmd27723,prox_pulsecnt",
		&pdata->prox_pulsecnt);
	of_property_read_u32(np, "tmd27723,prox_gain", &pdata->prox_gain);
	of_property_read_u32(np, "tmd27723,coef_atime", &pdata->coef_atime);
	of_property_read_u32(np, "tmd27723,ga", &pdata->ga);
	of_property_read_u32(np, "tmd27723,coef_a", &pdata->coef_a);
	of_property_read_u32(np, "tmd27723,coef_b", &pdata->coef_b);
	of_property_read_u32(np, "tmd27723,coef_c", &pdata->coef_c);
	of_property_read_u32(np, "tmd27723,coef_d", &pdata->coef_d);
	pr_info("%s: prox_thresh_hi: %u\n", __func__, pdata->prox_thresh_hi);
	pr_info("%s: prox_thresh_low: %u\n", __func__, pdata->prox_thresh_low);
	pr_info("%s: prox_th_hi_cal: %u\n", __func__, pdata->prox_th_hi_cal);
	pr_info("%s: prox_th_low_cal: %u\n", __func__, pdata->prox_th_low_cal);
	pr_info("%s: als_time: %u\n", __func__, pdata->als_time);
	pr_info("%s: intr_filter: %u\n", __func__, pdata->intr_filter);
	pr_info("%s: prox_pulsecnt: %u\n", __func__, pdata->prox_pulsecnt);
	pr_info("%s: prox_gain: %u\n", __func__, pdata->prox_gain);
	pr_info("%s: coef_atime: %u\n", __func__, pdata->coef_atime);
	pr_info("%s: ga: %u\n", __func__, pdata->ga);
	pr_info("%s: coef_a: %u\n", __func__, pdata->coef_a);
	pr_info("%s: coef_b: %u\n", __func__, pdata->coef_b);
	pr_info("%s: coef_c: %u\n", __func__, pdata->coef_c);
	pr_info("%s: coef_d: %u\n", __func__, pdata->coef_d);
	pdata->min_max = MIN;
	pdata->max_data = true;

	return 0;
}

static int tmd27723_i2c_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	int ret = -ENODEV;
	u8 chipid = 0;
	struct input_dev *input_dev;
	struct tmd27723_data *tmd27723;
	struct tmd27723_platform_data *pdata = NULL;
	int err;

	pr_info("%s, is called\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s: i2c functionality check failed!\n", __func__);
		goto exit;
	}

	tmd27723 = kzalloc(sizeof(struct tmd27723_data), GFP_KERNEL);
	if (!tmd27723) {
		pr_err("%s: failed to alloc memory for module data\n",
			__func__);
		ret = -ENOMEM;
		goto exit;
	}
	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
			sizeof(struct tmd27723_platform_data), GFP_KERNEL);
		if (!pdata) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			if (tmd27723)
				kfree(tmd27723);
			return -ENOMEM;
		}
		err = tmd27723_parse_dt(&client->dev, pdata);
		if (err)
			goto err_devicetree;
	} else
		pdata = client->dev.platform_data;
	if (!pdata) {
		pr_err("%s: missing pdata!\n", __func__);
		if (tmd27723)
			kfree(tmd27723);
		return ret;
	}

	tmd27723->offset_cal_high = false;
	tmd27723->pdata = pdata;
	tmd27723->i2c_client = client;
	i2c_set_clientdata(client, tmd27723);

	tmd27723_request_gpio(tmd27723);

	tmd27723_power_enable(&client->dev, 1);
	msleep(100);

	chipid = i2c_smbus_read_byte_data(client, CMD_REG | CHIPID);
	if (chipid != CHIP_ID) {
		pr_err("%s: i2c read error [%x]\n", __func__, chipid);
		goto err_chip_id_or_i2c_error;
	}

	tmd27723->threshold_high = tmd27723->pdata->prox_thresh_hi;
	tmd27723->threshold_low = tmd27723->pdata->prox_thresh_low;
	tmd27723->initial_offset = tmd27723_get_initial_offset(tmd27723);
	tmd27723->offset_value = tmd27723->initial_offset;

	mutex_init(&tmd27723->prox_mutex);
	/* wake lock init */
	wake_lock_init(&tmd27723->prx_wake_lock, WAKE_LOCK_SUSPEND,
		       "prx_wake_lock");
	mutex_init(&tmd27723->power_lock);

	/* allocate proximity input_device */
	input_dev = input_allocate_device();
	if (!input_dev) {
		pr_err("%s: could not allocate input device\n", __func__);
		goto err_input_device_proximity;
	}

	tmd27723->proximity_input_dev = input_dev;
	input_set_drvdata(input_dev, tmd27723);
	input_dev->name = "proximity_sensor";
	input_set_capability(input_dev, EV_ABS, ABS_DISTANCE);
	input_set_abs_params(input_dev, ABS_DISTANCE, 0, 1, 0, 0);

	input_report_abs(tmd27723->proximity_input_dev, ABS_DISTANCE, 1);
	input_sync(tmd27723->proximity_input_dev);

	tmd27723_dbgmsg("registering proximity input device\n");
	ret = input_register_device(input_dev);
	if (ret < 0) {
		pr_err("%s: could not register proximity input device\n",
			__func__);
		input_free_device(input_dev);
		goto err_input_device_proximity;
	}

	ret = sensors_create_symlink(&tmd27723->proximity_input_dev->dev.kobj,
				input_dev->name);
	if (ret < 0) {
		pr_err("%s: could not create proximity symlink\n", __func__);
		goto err_create_symlink_proximity;
	}

	ret = sysfs_create_group(&input_dev->dev.kobj,
				 &proximity_attribute_group);
	if (ret) {
		pr_err("%s: could not create sysfs group\n", __func__);
		goto err_sysfs_create_group_proximity;
	}

	/* allocate lightsensor-level input_device */
	input_dev = input_allocate_device();
	if (!input_dev) {
		pr_err("%s: could not allocate input device\n", __func__);
		ret = -ENOMEM;
		goto err_input_device_light;
	}

	input_set_drvdata(input_dev, tmd27723);
	input_dev->name = "light_sensor";
	input_set_capability(input_dev, EV_REL, REL_MISC);

	tmd27723_dbgmsg("registering lightsensor-level input device\n");
	ret = input_register_device(input_dev);
	if (ret < 0) {
		pr_err("%s: could not register light input device\n", __func__);
		input_free_device(input_dev);
		goto err_input_device_light;
	}

	tmd27723->light_input_dev = input_dev;
	ret = sensors_create_symlink(&tmd27723->light_input_dev->dev.kobj,
				tmd27723->light_input_dev->name);
	if (ret < 0) {
		pr_err("%s: could not create light symlink\n", __func__);
		goto err_create_symlink_light;
	}

	ret = sysfs_create_group(&input_dev->dev.kobj,
				 &light_attribute_group);
	if (ret) {
		pr_err("%s: could not create sysfs group\n", __func__);
		goto err_sysfs_create_group_light;
	}
	/* hrtimer settings.  we poll for light values using a timer. */
	hrtimer_init(&tmd27723->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	tmd27723->light_poll_delay = ns_to_ktime(200 * NSEC_PER_MSEC);
	tmd27723->timer.function = tmd27723_timer_func;

	hrtimer_init(&tmd27723->prox_avg_timer, CLOCK_MONOTONIC,
		HRTIMER_MODE_REL);
	tmd27723->prox_polling_time = ns_to_ktime(2000 * NSEC_PER_MSEC);
	tmd27723->prox_avg_timer.function = tmd27723_prox_timer_func;

	/* the timer just fires off a work queue request.  we need a thread
	   to read the i2c (can be slow and blocking). */
	tmd27723->wq = create_singlethread_workqueue("tmd27723_wq");
	if (!tmd27723->wq) {
		ret = -ENOMEM;
		pr_err("%s: could not create workqueue\n", __func__);
		goto err_create_workqueue;
	}

	tmd27723->wq_avg = create_singlethread_workqueue("tmd27723_wq_avg");
	if (!tmd27723->wq_avg) {
		ret = -ENOMEM;
		pr_err("%s: could not create workqueue\n", __func__);
		goto err_create_avg_workqueue;
	}

	/* this is the thread function we run on the work queue */
	INIT_WORK(&tmd27723->work_light, tmd27723_work_func_light);
	INIT_WORK(&tmd27723->work_prox, tmd27723_work_func_prox);
	INIT_WORK(&tmd27723->work_prox_avg, tmd27723_work_func_prox_avg);
	tmd27723->prox_avg_enable = 0;

	ret = tmd27723_setup_irq(tmd27723);
	if (ret) {
		pr_err("%s: could not setup irq\n", __func__);
		goto err_setup_irq;
	}

	/* set sysfs for proximity sensor and light sensor */
	ret = sensors_register(tmd27723->proximity_dev,
		tmd27723, prox_sensor_attrs, "proximity_sensor");
	if (ret) {
		pr_err("%s: cound not register proximity sensor device(%d).\n",
			__func__, ret);
		goto err_proximity_sensor_register_failed;
	}

	ret = sensors_register(tmd27723->light_dev,
				tmd27723, light_sensor_attrs, "light_sensor");
	if (ret) {
		pr_err("%s: cound not register light sensor device(%d).\n",
			__func__, ret);
		goto err_light_sensor_register_failed;
	}

	pr_info("%s is done.", __func__);
	return ret;

	/* error, unwind it all */
err_devicetree:
	pr_warn("\n error in device tree");

err_light_sensor_register_failed:
	sensors_unregister(tmd27723->proximity_dev, prox_sensor_attrs);
err_proximity_sensor_register_failed:
	free_irq(tmd27723->irq, 0);
	gpio_free(tmd27723->pdata->als_int);
err_setup_irq:
	destroy_workqueue(tmd27723->wq_avg);
err_create_avg_workqueue:
	destroy_workqueue(tmd27723->wq);
err_create_workqueue:
	sysfs_remove_group(&tmd27723->light_input_dev->dev.kobj,
			   &light_attribute_group);
err_sysfs_create_group_light:
	sensors_remove_symlink(&tmd27723->light_input_dev->dev.kobj,
				tmd27723->light_input_dev->name);
err_create_symlink_light:
	input_unregister_device(tmd27723->light_input_dev);
err_input_device_light:
	sysfs_remove_group(&tmd27723->proximity_input_dev->dev.kobj,
			   &proximity_attribute_group);
err_sysfs_create_group_proximity:
	sensors_remove_symlink(&tmd27723->proximity_input_dev->dev.kobj,
				tmd27723->proximity_input_dev->name);
err_create_symlink_proximity:
	input_unregister_device(tmd27723->proximity_input_dev);
err_input_device_proximity:
	mutex_destroy(&tmd27723->power_lock);
	wake_lock_destroy(&tmd27723->prx_wake_lock);
	mutex_destroy(&tmd27723->prox_mutex);
err_chip_id_or_i2c_error:
	tmd27723_power_enable(&client->dev, 0);
	kfree(tmd27723);
exit:
	pr_err("%s failed. ret = %d\n", __func__, ret);
	return ret;
}

static int tmd27723_suspend(struct device *dev)
{
	/* We disable power only if proximity is disabled.  If proximity
	   is enabled, we leave power on because proximity is allowed
	   to wake up device.  We remove power without changing
	   gp2a->power_state because we use that state in resume.
	*/
	struct i2c_client *client = to_i2c_client(dev);
	struct tmd27723_data *tmd27723 = i2c_get_clientdata(client);

	if (tmd27723->power_state & LIGHT_ENABLED)
		tmd27723_light_disable(tmd27723);
	if (tmd27723->power_state == LIGHT_ENABLED) {
		tmd27723_chip_off(tmd27723);
	}
	if (tmd27723->power_state & PROXIMITY_ENABLED)
		disable_irq(tmd27723->irq);
	return 0;
}

static int tmd27723_resume(struct device *dev)
{
	/* Turn power back on if we were before suspend. */
	struct i2c_client *client = to_i2c_client(dev);
	struct tmd27723_data *tmd27723 = i2c_get_clientdata(client);

	if (tmd27723->power_state == LIGHT_ENABLED) {
		tmd27723->chip_on_success = (tmd27723_chip_on(tmd27723) >= 0) ?
			true : false;
	}
	if (tmd27723->power_state & LIGHT_ENABLED)
		tmd27723_light_enable(tmd27723);
	if (tmd27723->power_state & PROXIMITY_ENABLED)
		enable_irq(tmd27723->irq);

	return 0;
}

static int tmd27723_i2c_remove(struct i2c_client *client)
{
	struct tmd27723_data *tmd27723 = i2c_get_clientdata(client);
	sysfs_remove_group(&tmd27723->light_input_dev->dev.kobj,
			   &light_attribute_group);
	input_unregister_device(tmd27723->light_input_dev);
	sysfs_remove_group(&tmd27723->proximity_input_dev->dev.kobj,
			   &proximity_attribute_group);
	input_unregister_device(tmd27723->proximity_input_dev);

	free_irq(tmd27723->irq, NULL);
	gpio_free(tmd27723->pdata->als_int);

	if (tmd27723->power_state) {
		tmd27723->power_state = 0;
		if (tmd27723->power_state & LIGHT_ENABLED)
			tmd27723_light_disable(tmd27723);
		tmd27723_power_enable(&client->dev, 0);
	}
	destroy_workqueue(tmd27723->wq);
	destroy_workqueue(tmd27723->wq_avg);

	mutex_destroy(&tmd27723->power_lock);
	wake_lock_destroy(&tmd27723->prx_wake_lock);
	mutex_destroy(&tmd27723->prox_mutex);
	kfree(tmd27723);

	return 0;
}

static const struct i2c_device_id tmd27723_device_id[] = {
	{"tmd27723", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, tmd27723_device_id);

static const struct dev_pm_ops tmd27723_pm_ops = {
	.suspend = tmd27723_suspend,
	.resume = tmd27723_resume
};

static struct of_device_id tmd27723_match_table[] = {
	{ .compatible = "tmd27723,tmd27723",},
	{},
};

static struct i2c_driver tmd27723_i2c_driver = {
	.driver = {
		.name = "tmd27723",
		.owner = THIS_MODULE,
		.pm = &tmd27723_pm_ops,
		.of_match_table = tmd27723_match_table,
	},
	.probe		= tmd27723_i2c_probe,
	.remove	= tmd27723_i2c_remove,
	.id_table	= tmd27723_device_id,
};


static int __init tmd27723_init(void)
{
	return i2c_add_driver(&tmd27723_i2c_driver);
}

static void __exit tmd27723_exit(void)
{
	i2c_del_driver(&tmd27723_i2c_driver);
}

module_init(tmd27723_init);
module_exit(tmd27723_exit);

MODULE_AUTHOR("SAMSUNG");
MODULE_DESCRIPTION("Optical Sensor driver for tmd27723");
MODULE_LICENSE("GPL");

