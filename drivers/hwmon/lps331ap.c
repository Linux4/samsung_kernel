/*
* STMicroelectronics LPS331AP Barometer Sensor module driver
*
* Copyright (C) 2012 Marvell International Ltd.
* Copyright (C) 2011 STMicroelectronics- MSH - Motion Mems BU - Application Team
* Matteo Dameno (matteo.dameno@st.com)
*
* Both authors are willing to be considered the contact and update points for
* the driver.
*
* Obtained values are then expressed as
* mbar (=0.1 kPa) and Celsius degrees.
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
*
*/
/******************************************************************************
 Revision 1.0.0 2011/Feb/14:
	first release
	moved to input/misc
 Revision 1.0.1 2011/Apr/04:
	xxx
 Revision 1.0.2 2011/Sep/01:
	corrects ord bug, forces BDU enable
 Revision 1.0.3 2011/Sep/15:
	introduces compansation params reading and sysfs file to get them
 Revision 1.0.4 2011/Dec/12:
	sets maximum allowable resolution modes dynamically with ODR;
 Revision 1.0.5 2012/Feb/29:
	introduces more compansation params and extends sysfs file content
	format to get them; reduces minimum polling period define;
	enables by default DELTA_EN bit1 on CTRL_REG1
	corrects bug on TSH acquisition
 Revision 1.0.6 2012/Mar/30:
	introduces one more compansation param and extends sysfs file content
	format to get it.
******************************************************************************/
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/regulator/consumer.h>
#include "lps331ap.h"

#define	DEBUG	0

#define	PR_ABS_MAX	(8388607)	/* 24 bit 2'compl */
#define	PR_ABS_MIN	(-8388608)

#ifdef SHRT_MAX
#define	TEMP_MAX	SHRT_MAX
#define	TEMP_MIN	SHRT_MIN
#else
#define	TEMP_MAX	SHORT_MAX
#define	TEMP_MIN	SHORT_MIN
#endif

#define	WHOAMI_LPS331AP_PRS	0xBB	/* Expectd content for WAI */

/* CONTROL REGISTERS */
#define	REF_P_XL	0x08	/* pressure reference */
#define	REF_P_L		0x09	/* pressure reference */
#define	REF_P_H		0x0A	/* pressure reference */
#define	WHO_AM_I	0x0F	/* WhoAmI register */
#define	TP_RESOL	0x10	/* Pres Temp resolution set */
#define	CTRL_REG1	0x20	/* power / ODR control reg */
#define	CTRL_REG2	0x21	/* boot reg */
#define	CTRL_REG3	0x22	/* interrupt control reg */
#define	INT_CFG_REG	0x23	/* interrupt config reg */
#define	INT_SRC_REG	0x24	/* interrupt source reg */
#define	THS_P_L		0x25	/* pressure threshold */
#define	THS_P_H		0x26	/* pressure threshold */
#define	STATUS_REG	0X27	/* status reg */
#define	PRESS_OUT_XL	0x28	/* press output (3 regs) */
#define	TEMP_OUT_L	0x2B	/* temper output (2 regs) */

/* REGISTERS ALIASES */
#define	P_REF_INDATA_REG	REF_P_XL
#define	T_REF_INDATA_REG	REF_T_L
#define	P_THS_INDATA_REG	THS_P_L
#define	P_OUTDATA_REG		PRESS_OUT_XL
#define	T_OUTDATA_REG		TEMP_OUT_L
#define	OUTDATA_REG		PRESS_OUT_XL

/* REGISTERS MASK */
#define	LPS331AP_PRS_ENABLE_MASK	0x80	/*  ctrl_reg1 */
#define	LPS331AP_PRS_ODR_MASK		0x70	/*  ctrl_reg1 */
#define	LPS331AP_PRS_BDU_MASK		0x04	/* ctrl_reg1 */
#define	LPS331AP_PRS_PM_NORMAL		0x80	/* Power Normal Mode */
#define	LPS331AP_PRS_PM_OFF			0x00	/*  Power Down * */
#define	LPS331AP_PRS_BDU_ON		0x04	/* En BDU Block Data Upd */
#define	LPS331AP_PRS_RES_AVGTEMP_064	0X60
#define	LPS331AP_PRS_RES_AVGTEMP_128	0X70
#define	LPS331AP_PRS_RES_AVGPRES_512	0X0A

#define	LPS331AP_PRS_RES_MAX		(LPS331AP_PRS_RES_AVGTEMP_128	| \
						LPS331AP_PRS_RES_AVGPRES_512)
						/* Max Resol. for 1/7/12,5Hz */

#define	LPS331AP_PRS_RES_25HZ		(LPS331AP_PRS_RES_AVGTEMP_064	| \
						LPS331AP_PRS_RES_AVGPRES_512)
						/* Max Resol. for 25Hz */

#define	FUZZ			0
#define	FLAT			0

#define	I2C_AUTO_INCREMENT	0x80

/* RESUME STATE INDICES */
#define	LPS331AP_RES_REF_P_XL		0
#define	LPS331AP_RES_REF_P_L		1
#define	LPS331AP_RES_REF_P_H		2
#define	LPS331AP_RES_REF_T_L		3
#define	LPS331AP_RES_REF_T_H		4
#define	LPS331AP_RES_TP_RESOL		5
#define	LPS331AP_RES_CTRL_REG1		6
#define	LPS331AP_RES_CTRL_REG2		7
#define	LPS331AP_RES_CTRL_REG3		8
#define	LPS331AP_RES_INT_CFG_REG	9
#define	LPS331AP_RES_THS_P_L		10
#define	LPS331AP_RES_THS_P_H		11

#define	RESUME_ENTRIES			12
/* end RESUME STATE INDICES */

#define LPS331AP_PRS_MIN_POLL_PERIOD_MS	1

#define	SAD0L				0x00
#define	SAD0H				0x01
#define	LPS331AP_PRS_I2C_SADROOT	0x2E
#define	LPS331AP_PRS_I2C_SAD_L		((LPS331AP_PRS_I2C_SADROOT<<1)|SAD0L)
#define	LPS331AP_PRS_I2C_SAD_H		((LPS331AP_PRS_I2C_SADROOT<<1)|SAD0H)
#define	LPS331AP_PRS_DEV_NAME		"LPS_pressure_sensor"

/* input define mappings */
#define ABS_PR		ABS_PRESSURE
#define ABS_TEMP	ABS_X
#define ABS_DLTPR	ABS_MISC

/* Barometer and Termometer output data rate ODR */
#define	LPS331AP_PRS_ODR_ONESH	0x00	/* one shot both */
#define	LPS331AP_PRS_ODR_1_1	0x10	/* 1  Hz baro,  1  Hz term ODR */
#define	LPS331AP_PRS_ODR_7_7	0x50	/* 7  Hz baro,  7  Hz term ODR */
#define	LPS331AP_PRS_ODR_12_12	0x60	/* 12.5Hz baro, 12.5Hz term ODR */
#define	LPS331AP_PRS_ODR_25_25	0x70	/* 25  Hz baro, 25  Hz term ODR */

/* Pressure section defines */
/* Pressure Sensor Operating Mode */
#define	LPS331AP_PRS_ENABLE		0x01
#define	LPS331AP_PRS_DISABLE		0x00

/* Output conversion factors */
#define	SENSITIVITY_T		480	/* 480 LSB/degrC */
#define	SENSITIVITY_P		4096	/* LSB/mbar */
#define	SENSITIVITY_P_SHIFT	12	/* 4096 LSB/mbar */
#define	TEMPERATURE_OFFSET	(42)	/* 42.5 degrC */

struct lps331ap_prs_odr {
	unsigned int cutoff_ms;
	unsigned int mask;
};

struct lps331ap_prs_odr lps331ap_prs_odr_table[] = {
	{40, LPS331AP_PRS_ODR_25_25},
	{80, LPS331AP_PRS_ODR_12_12},
	{143, LPS331AP_PRS_ODR_7_7},
	{1000, LPS331AP_PRS_ODR_1_1},
};

struct lps331ap_prs_data {
	struct i2c_client *client;
	struct lps331ap_prs_platform_data *pdata;

	struct mutex lock;
	struct delayed_work input_work;
	struct input_dev *input_dev;
	struct regulator *avdd;

	int hw_initialized;
	/* hw_working=-1 means not tested yet */
	int hw_working;
	int user_count;

	atomic_t enabled;
	int on_before_suspend;
	u8 resume_state[RESUME_ENTRIES];
};

static struct i2c_client *lps331ap_i2c_client; /* global i2c_client */

#if DEBUG
static void lps331ap_dump_registers(struct lps331ap_prs_data *prs);
#endif

struct outputdata {
	s32 press;
	s16 temperature;
};

static int lps331ap_prs_i2c_read(struct lps331ap_prs_data *prs,
				 u8 *buf, int len)
{
	int ret;
	struct i2c_msg msgs[] = {
		{
		 .addr = prs->client->addr,
		 .flags = prs->client->flags & I2C_M_TEN,
		 .len = 1,
		 .buf = buf,
		 }, {
		     .addr = prs->client->addr,
		     .flags = (prs->client->flags & I2C_M_TEN) | I2C_M_RD,
		     .len = len,
		     .buf = buf,
		     },
	};

	ret = i2c_transfer(prs->client->adapter, msgs, 2);
	if (unlikely(ret != 2)) {
		dev_err(&prs->client->dev, "read transfer error\n");
		ret = -EIO;
	}
	return 0;
}

static int lps331ap_prs_i2c_write(struct lps331ap_prs_data *prs,
				  u8 *buf, int len)
{
	int ret;
	struct i2c_msg msgs[] = {
		{
		 .addr = prs->client->addr,
		 .flags = prs->client->flags & I2C_M_TEN,
		 .len = len + 1,
		 .buf = buf,
		 },
	};

	ret = i2c_transfer(prs->client->adapter, msgs, 1);
	if (unlikely(ret != 1)) {
		dev_err(&prs->client->dev, "write transfer error\n");
		ret = -EIO;
	}
	return 0;
}

static int lps331ap_prs_hw_init(struct lps331ap_prs_data *prs)
{
	int ret;
	u8 buf[6];

	dev_dbg(&prs->client->dev, "%s: hw init start\n",
		LPS331AP_PRS_DEV_NAME);

	buf[0] = WHO_AM_I;
	ret = lps331ap_prs_i2c_read(prs, buf, 1);
	if (ret < 0)
		goto error_firstread;
	else
		prs->hw_working = 1;
	if (buf[0] != WHOAMI_LPS331AP_PRS) {
		ret = -1;
		goto error_unknown_device;
	}

	buf[0] = (I2C_AUTO_INCREMENT | P_REF_INDATA_REG);
	buf[1] = prs->resume_state[LPS331AP_RES_REF_P_XL];
	buf[2] = prs->resume_state[LPS331AP_RES_REF_P_L];
	buf[3] = prs->resume_state[LPS331AP_RES_REF_P_H];
	buf[4] = prs->resume_state[LPS331AP_RES_REF_T_L];
	buf[5] = prs->resume_state[LPS331AP_RES_REF_T_H];
	ret = lps331ap_prs_i2c_write(prs, buf, 5);
	if (ret < 0)
		goto err_resume_state;

	buf[0] = TP_RESOL;
	buf[1] = prs->resume_state[LPS331AP_RES_TP_RESOL];
	ret = lps331ap_prs_i2c_write(prs, buf, 1);
	if (ret < 0)
		goto err_resume_state;

	buf[0] = (I2C_AUTO_INCREMENT | P_THS_INDATA_REG);
	buf[1] = prs->resume_state[LPS331AP_RES_THS_P_L];
	buf[2] = prs->resume_state[LPS331AP_RES_THS_P_H];
	ret = lps331ap_prs_i2c_write(prs, buf, 2);
	if (ret < 0)
		goto err_resume_state;

	buf[0] = (I2C_AUTO_INCREMENT | CTRL_REG1);
	buf[1] = prs->resume_state[LPS331AP_RES_CTRL_REG1];
	buf[2] = prs->resume_state[LPS331AP_RES_CTRL_REG2];
	buf[3] = prs->resume_state[LPS331AP_RES_CTRL_REG3];
	ret = lps331ap_prs_i2c_write(prs, buf, 3);
	if (ret < 0)
		goto err_resume_state;

	buf[0] = INT_CFG_REG;
	buf[1] = prs->resume_state[LPS331AP_RES_INT_CFG_REG];
	ret = lps331ap_prs_i2c_write(prs, buf, 1);
	if (ret < 0)
		goto err_resume_state;

	prs->hw_initialized = 1;
	dev_dbg(&prs->client->dev, "%s: hw init done\n", LPS331AP_PRS_DEV_NAME);
	return 0;

error_firstread:
	prs->hw_working = 0;
	dev_warn(&prs->client->dev, "Error reading WHO_AM_I: is device" \
		"available/working?\n");
	goto err_resume_state;
error_unknown_device:
	dev_err(&prs->client->dev,
		"device unknown. Expected: 0x%02x,"
		" Replies: 0x%02x\n", WHOAMI_LPS331AP_PRS, buf[0]);
err_resume_state:
	prs->hw_initialized = 0;
	dev_err(&prs->client->dev, "hw init error 0x%02x,0x%02x: %d\n", buf[0],
		buf[1], ret);
	return ret;
}

static void lps331ap_prs_device_power_off(struct lps331ap_prs_data *prs)
{
	int ret;
	u8 buf[2] = { CTRL_REG1, LPS331AP_PRS_PM_OFF };

	ret = lps331ap_prs_i2c_write(prs, buf, 1);
	if (ret < 0)
		dev_err(&prs->client->dev, "soft power off failed: %d\n", ret);
	prs->hw_initialized = 0;

	regulator_disable(prs->avdd);
}

static int lps331ap_prs_device_power_on(struct lps331ap_prs_data *prs)
{
	int ret = -1;

	if (regulator_enable(prs->avdd)) {
		dev_err(&prs->client->dev, "BOSCH sensor regulator enable failed\n");
		return ret;
	} else
		ret = 1;

	if (!prs->hw_initialized) {
		ret = lps331ap_prs_hw_init(prs);
		if (prs->hw_working == 1 && ret < 0) {
			lps331ap_prs_device_power_off(prs);
			return ret;
		}
	}
	return 0;
}

int lps331ap_prs_update_odr(struct lps331ap_prs_data *prs, int poll_period_ms)
{
	int ret = -1;
	int i;

	u8 buf[2];
	u8 init_val, updated_val;
	u8 curr_val, new_val;
	u8 mask = LPS331AP_PRS_ODR_MASK;
	u8 resol = LPS331AP_PRS_RES_MAX;

	/*
	 * Following, looks for the longest possible odr interval scrolling the
	 * odr_table vector from the end (longest period) backward (shortest
	 * period), to support the poll_interval requested by the system.
	 * It must be the longest period shorter than the set poll period.
	 */
	for (i = ARRAY_SIZE(lps331ap_prs_odr_table) - 1; i >= 0; i--) {
		if ((lps331ap_prs_odr_table[i].cutoff_ms <= poll_period_ms)
		    || (i == 0))
			break;
	}

	new_val = lps331ap_prs_odr_table[i].mask;
	if (new_val == LPS331AP_PRS_ODR_25_25)
		resol = LPS331AP_PRS_RES_25HZ;

	if (atomic_read(&prs->enabled)) {
		buf[0] = CTRL_REG1;
		ret = lps331ap_prs_i2c_read(prs, buf, 1);
		if (ret < 0)
			goto error;
		/* work on all but ENABLE bits */
		init_val = buf[0];
		prs->resume_state[LPS331AP_RES_CTRL_REG1] = init_val;

		curr_val = ((LPS331AP_PRS_ENABLE_MASK & LPS331AP_PRS_PM_OFF)
			    | ((~LPS331AP_PRS_ENABLE_MASK) & init_val));
		buf[0] = CTRL_REG1;
		buf[1] = curr_val;
		ret = lps331ap_prs_i2c_write(prs, buf, 1);
		if (ret < 0)
			goto error;

		buf[0] = CTRL_REG1;
		updated_val = ((mask & new_val) | ((~mask) & curr_val));

		buf[0] = CTRL_REG1;
		buf[1] = updated_val;
		ret = lps331ap_prs_i2c_write(prs, buf, 1);
		if (ret < 0)
			goto error;

		curr_val = ((LPS331AP_PRS_ENABLE_MASK & LPS331AP_PRS_PM_NORMAL)
			    | ((~LPS331AP_PRS_ENABLE_MASK) & updated_val));
		buf[0] = CTRL_REG1;
		buf[1] = curr_val;
		ret = lps331ap_prs_i2c_write(prs, buf, 1);
		if (ret < 0)
			goto error;

		buf[0] = TP_RESOL;
		buf[1] = resol;
		ret = lps331ap_prs_i2c_write(prs, buf, 1);
		if (ret < 0)
			goto error;

		prs->resume_state[LPS331AP_RES_CTRL_REG1] = curr_val;
		prs->resume_state[LPS331AP_RES_TP_RESOL] = resol;
	}
	return ret;

error:
	dev_err(&prs->client->dev, "update odr failed 0x%02x,0x%02x: %d\n",
		buf[0], buf[1], ret);

	return ret;
}

static int lps331ap_prs_get_presstemp_data(struct lps331ap_prs_data *prs,
					   struct outputdata *out)
{
	int ret;
	u8 prs_data[5];
	s32 pressure;
	s16 temperature;
	int reg_to_read = 5;

	prs_data[0] = (I2C_AUTO_INCREMENT | OUTDATA_REG);
	ret = lps331ap_prs_i2c_read(prs, prs_data, reg_to_read);
	if (ret < 0)
		return ret;

	pressure = (s32) ((((s8) prs_data[2]) << 16) |
			  (prs_data[1] << 8) | (prs_data[0]));
	temperature = (s16) ((((s8) prs_data[4]) << 8) | (prs_data[3]));

	out->press = pressure;
	out->temperature = temperature;

	return ret;
}

static void lps331ap_prs_report_values(struct lps331ap_prs_data *prs,
				       struct outputdata *out)
{
	input_report_abs(prs->input_dev, ABS_PRESSURE, out->press);
	input_report_abs(prs->input_dev, ABS_X, out->temperature);
	input_sync(prs->input_dev);
}

static int lps331ap_prs_enable(struct lps331ap_prs_data *prs)
{
	int ret;
	mutex_lock(&prs->lock);
	if (prs->user_count == 0 && !atomic_cmpxchg(&prs->enabled, 0, 1)) {
		ret = lps331ap_prs_device_power_on(prs);
		if (ret < 0) {
			atomic_set(&prs->enabled, 0);
			mutex_unlock(&prs->lock);
			return ret;
		}
		schedule_delayed_work(&prs->input_work,
				      msecs_to_jiffies(prs->
						       pdata->poll_interval));
		prs->user_count++;
	} else {
		prs->user_count++;
	}
	mutex_unlock(&prs->lock);
	return 0;
}

static int lps331ap_prs_disable(struct lps331ap_prs_data *prs)
{
	mutex_lock(&prs->lock);
	if (prs->user_count == 1 && atomic_cmpxchg(&prs->enabled, 1, 0)) {
		mutex_unlock(&prs->lock);
		cancel_delayed_work_sync(&prs->input_work);
		mutex_lock(&prs->lock);
		lps331ap_prs_device_power_off(prs);
		prs->user_count--;
	} else if (prs->user_count > 1) {
		prs->user_count--;
	}
	mutex_unlock(&prs->lock);
	return 0;
}

static ssize_t attr_get_polling_rate(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	int val;
	struct lps331ap_prs_data *prs = dev_get_drvdata(dev);
	mutex_lock(&prs->lock);
	val = prs->pdata->poll_interval;
	mutex_unlock(&prs->lock);
	return sprintf(buf, "%d\n", val);
}

static ssize_t attr_set_polling_rate(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t size)
{
	struct lps331ap_prs_data *prs = dev_get_drvdata(dev);
	unsigned long interval_ms;

	if (kstrtoul(buf, 10, &interval_ms))
		return -EINVAL;
	if (!interval_ms)
		return -EINVAL;
	interval_ms = max_t(unsigned int, interval_ms,
			    prs->pdata->min_interval);
	mutex_lock(&prs->lock);
	prs->pdata->poll_interval = interval_ms;
	lps331ap_prs_update_odr(prs, interval_ms);
	mutex_unlock(&prs->lock);
	return size;
}

static ssize_t attr_get_enable(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	struct lps331ap_prs_data *prs = dev_get_drvdata(dev);
	int val = atomic_read(&prs->enabled);
	return sprintf(buf, "%d\n", val);
}

static ssize_t attr_set_enable(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t size)
{
	struct lps331ap_prs_data *prs = dev_get_drvdata(dev);

	int enable = strcmp(buf, "1\n") ? 0 : 1;

	if (enable)
		lps331ap_prs_enable(prs);
	else
		lps331ap_prs_disable(prs);

	return size;
}

static ssize_t attr_get_prs(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct lps331ap_prs_data *prs = dev_get_drvdata(dev);
	static struct outputdata output;
	int ret;
	s32 pressure;
	ret = lps331ap_prs_get_presstemp_data(prs, &output);
	if (ret < 0)
		dev_err(&prs->client->dev, "get_pressure_data failed\n");
	pressure = output.press;
	pressure /= SENSITIVITY_P;
	return sprintf(buf, "%d\n", pressure);
}

static ssize_t attr_get_temp(struct device *dev,
			 struct device_attribute *attr, char *buf)
{
	struct lps331ap_prs_data *prs = dev_get_drvdata(dev);
	static struct outputdata output;
	int ret;
	s16 temperature;
	ret = lps331ap_prs_get_presstemp_data(prs, &output);
	if (ret < 0)
		dev_err(&prs->client->dev, "get_pressure_data failed\n");
	temperature = output.temperature;
	temperature /= SENSITIVITY_T;
	temperature += TEMPERATURE_OFFSET;
	return sprintf(buf, "%d\n", temperature);
}

static DEVICE_ATTR(interval, S_IRUGO | S_IWUSR | S_IWGRP,
		   attr_get_polling_rate, attr_set_polling_rate);
static DEVICE_ATTR(active, S_IRUGO | S_IWUSR | S_IWGRP,
		   attr_get_enable, attr_set_enable);
static DEVICE_ATTR(prs, S_IRUGO | S_IWUSR | S_IWGRP, attr_get_prs, NULL);
static DEVICE_ATTR(temp, S_IRUGO | S_IWUSR | S_IWGRP, attr_get_temp, NULL);

static struct attribute *sysfs_lps331ap_attributes[] = {
	&dev_attr_interval.attr,
	&dev_attr_active.attr,
	&dev_attr_prs.attr,
	&dev_attr_temp.attr,
	NULL
};

static struct attribute_group sysfs_lps331ap_attribute_group = {
	.attrs = sysfs_lps331ap_attributes
};

static void lps331ap_prs_input_work_func(struct work_struct *work)
{
	struct lps331ap_prs_data *prs = container_of((struct delayed_work *)
						     work,
						     struct lps331ap_prs_data,
						     input_work);

	static struct outputdata output;
	int ret;

	mutex_lock(&prs->lock);
	ret = lps331ap_prs_get_presstemp_data(prs, &output);
	if (ret < 0)
		dev_err(&prs->client->dev, "get_pressure_data failed\n");
	else
		lps331ap_prs_report_values(prs, &output);

	schedule_delayed_work(&prs->input_work,
			      msecs_to_jiffies(prs->pdata->poll_interval));
	mutex_unlock(&prs->lock);
}

int lps331ap_prs_input_open(struct input_dev *input)
{
	return 0;
}

void lps331ap_prs_input_close(struct input_dev *dev)
{
}

static int lps331ap_prs_validate_pdata(struct lps331ap_prs_data *prs)
{
	/* checks for correctness of minimal polling period */
	prs->pdata->min_interval =
	    max((unsigned int)LPS331AP_PRS_MIN_POLL_PERIOD_MS,
		prs->pdata->min_interval);

	prs->pdata->poll_interval = max(prs->pdata->poll_interval,
					prs->pdata->min_interval);

	/* Checks polling interval relative to minimum polling interval */
	if (prs->pdata->poll_interval < prs->pdata->min_interval) {
		dev_err(&prs->client->dev, "minimum poll interval violated\n");
		return -EINVAL;
	}

	return 0;
}

static int lps331ap_prs_input_init(struct lps331ap_prs_data *prs)
{
	int ret;
	INIT_DELAYED_WORK(&prs->input_work, lps331ap_prs_input_work_func);
	prs->input_dev = input_allocate_device();
	if (!prs->input_dev) {
		ret = -ENOMEM;
		dev_err(&prs->client->dev, "input device allocate failed\n");
		goto err0;
	}

	prs->input_dev->open = lps331ap_prs_input_open;
	prs->input_dev->close = lps331ap_prs_input_close;
	prs->input_dev->name = LPS331AP_PRS_DEV_NAME;
	prs->input_dev->id.bustype = BUS_I2C;
	prs->input_dev->id.vendor = 0;

	input_set_drvdata(prs->input_dev, prs);

	set_bit(EV_ABS, prs->input_dev->evbit);
	set_bit(ABS_PRESSURE, prs->input_dev->absbit);
	set_bit(ABS_MISC, prs->input_dev->absbit);
	set_bit(ABS_X, prs->input_dev->absbit);

	input_set_abs_params(prs->input_dev, ABS_PRESSURE,
			     PR_ABS_MIN, PR_ABS_MAX, FUZZ, FLAT);
	input_set_abs_params(prs->input_dev, ABS_X,
			     TEMP_MIN, TEMP_MAX, FUZZ, FLAT);

	ret = input_register_device(prs->input_dev);
	if (ret) {
		dev_err(&prs->client->dev,
			"unable to register input polled device %s\n",
			prs->input_dev->name);
		goto err1;
	}

	return 0;

err1:
	input_free_device(prs->input_dev);
err0:
	return ret;
}

static void lps331ap_prs_input_cleanup(struct lps331ap_prs_data *prs)
{
	input_unregister_device(prs->input_dev);
}

static int lps331ap_prs_probe(struct i2c_client *client,
			      const struct i2c_device_id *id)
{
	struct lps331ap_prs_data *prs;
	struct lps331ap_prs_platform_data *lps331ap_plat_data;
	struct device_node *np = client->dev.of_node;
	struct regulator *avdd;
	int ret = -1;
	int tempvalue;

	dev_info(&client->dev, "%s: probe start.\n", LPS331AP_PRS_DEV_NAME);

#ifdef CONFIG_OF
	lps331ap_plat_data = kzalloc(sizeof(*lps331ap_plat_data), GFP_KERNEL);
	if (lps331ap_plat_data == NULL) {
		dev_err(&client->dev,"Alloc GFP_KERNEL memory failed.");
		return -ENOMEM;
	}
	if (of_property_read_u32(np, "lps331ap,poll_interval",
			 &lps331ap_plat_data->poll_interval)) {
		dev_err(&client->dev, "failed to get lps331ap,poll_interval property\n");
	}
	client->dev.platform_data = lps331ap_plat_data;
#endif

	if (client->dev.platform_data == NULL) {
		dev_err(&client->dev, "platform data is NULL. exiting.\n");
		ret = -ENODATA;
		goto err_exit_check_functionality_failed;
	}

	avdd = regulator_get(&client->dev, "avdd");
	if (IS_ERR(avdd)) {
		dev_err(&client->dev, "sensor avdd power supply get failed\n");
		goto out;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "client not i2c capable\n");
		ret = -ENODEV;
		goto err_exit_check_functionality_failed;
	}

	if (!i2c_check_functionality(client->adapter,
				     I2C_FUNC_SMBUS_BYTE |
				     I2C_FUNC_SMBUS_BYTE_DATA |
				     I2C_FUNC_SMBUS_WORD_DATA)) {
		dev_err(&client->dev, "client not smb-i2c capable:2\n");
		ret = -EIO;
		goto err_exit_check_functionality_failed;
	}

	if (!i2c_check_functionality(client->adapter,
				     I2C_FUNC_SMBUS_I2C_BLOCK)) {
		dev_err(&client->dev, "client not smb-i2c capable:3\n");
		ret = -EIO;
		goto err_exit_check_functionality_failed;
	}

	prs = devm_kzalloc(&client->dev, sizeof(struct lps331ap_prs_data),
			GFP_KERNEL);
	if (prs == NULL) {
		ret = -ENOMEM;
		dev_err(&client->dev,
			"failed to allocate memory for module data: "
			"%d\n", ret);
		goto err_exit_alloc_data_failed;
	}
	mutex_init(&prs->lock);
	mutex_lock(&prs->lock);

	prs->client = client;
	lps331ap_i2c_client = client;
	prs->avdd = avdd;

	i2c_set_clientdata(client, prs);

	prs->pdata = kmemdup(client->dev.platform_data,
			     sizeof(*prs->pdata), GFP_KERNEL);
	if (!prs->pdata) {
		ret = -ENOMEM;
		dev_err(&client->dev,
			"failed to allocate memory for pdata: %d\n", ret);
		goto err_exit_kfree_pdata;
	}

	ret = lps331ap_prs_validate_pdata(prs);
	if (ret < 0) {
		dev_err(&client->dev, "failed to validate platform data\n");
		goto err_exit_kfree_pdata;
	}

	ret = lps331ap_prs_device_power_on(prs);
	if (ret < 0) {
		dev_err(&client->dev, "power on failed: %d\n", ret);
		goto err_exit_kfree_pdata;
	}

	if (i2c_smbus_read_byte(client) < 0) {
		dev_err(&client->dev, "%s:i2c_smbus_read_byte error!!\n",
			LPS331AP_PRS_DEV_NAME);
		goto err_power_off;
	} else {
		dev_dbg(&prs->client->dev, "%s Device detected!\n",
			LPS331AP_PRS_DEV_NAME);
	}

	/* read chip id */
	tempvalue = i2c_smbus_read_byte_data(client, WHO_AM_I);
	if ((tempvalue & 0x00FF) == WHOAMI_LPS331AP_PRS) {
		dev_info(&client->dev, "%s I2C driver registered!\n",
			 LPS331AP_PRS_DEV_NAME);
	} else {
		prs->client = NULL;
		ret = -ENODEV;
		dev_err(&client->dev, "I2C driver not registered." \
			"Device unknown: %d\n", ret);
		goto err_power_off;
	}
	memset(prs->resume_state, 0, ARRAY_SIZE(prs->resume_state));
	prs->resume_state[LPS331AP_RES_CTRL_REG1] =
	    ((LPS331AP_PRS_ENABLE_MASK & LPS331AP_PRS_PM_NORMAL) |
		(LPS331AP_PRS_ODR_MASK & LPS331AP_PRS_ODR_1_1) |
		(LPS331AP_PRS_BDU_MASK & LPS331AP_PRS_BDU_ON)
	    );

	prs->resume_state[LPS331AP_RES_TP_RESOL] = LPS331AP_PRS_RES_MAX;

	atomic_set(&prs->enabled, 1);

	ret = lps331ap_prs_update_odr(prs, prs->pdata->poll_interval);
	if (ret < 0) {
		dev_err(&client->dev, "update_odr failed\n");
		goto err_power_off;
	}

	ret = lps331ap_prs_input_init(prs);
	if (ret < 0) {
		dev_err(&client->dev, "input init failed\n");
		goto err_power_off;
	}
	ret = sysfs_create_group(&prs->input_dev->dev.kobj,
				 &sysfs_lps331ap_attribute_group);
	if (ret < 0) {
		dev_err(&client->dev,
			"device LPS331AP_PRS_DEV_NAME sysfs register failed\n");
		goto err_input_cleanup;
	}

	lps331ap_prs_device_power_off(prs);
	/* As default, do not report information */
	atomic_set(&prs->enabled, 0);
	mutex_unlock(&prs->lock);
	dev_info(&client->dev, "%s: probed\n", LPS331AP_PRS_DEV_NAME);
	return 0;

err_input_cleanup:
#ifdef CONFIG_OF
	kfree(lps331ap_plat_data);
#endif
	lps331ap_prs_input_cleanup(prs);
err_power_off:
	lps331ap_prs_device_power_off(prs);
err_exit_kfree_pdata:
	kfree(prs->pdata);
	mutex_unlock(&prs->lock);
	devm_kfree(&client->dev, prs);
err_exit_alloc_data_failed:
err_exit_check_functionality_failed:
	dev_err(&client->dev, "%s: Driver Init failed\n",
		LPS331AP_PRS_DEV_NAME);
	return ret;
out:
	regulator_put(avdd);
	return ret;
}

static int lps331ap_prs_remove(struct i2c_client *client)
{
	struct lps331ap_prs_data *prs = i2c_get_clientdata(client);

	lps331ap_prs_input_cleanup(prs);
	lps331ap_prs_device_power_off(prs);
	sysfs_remove_group(&prs->input_dev->dev.kobj,
			   &sysfs_lps331ap_attribute_group);

	kfree(prs->pdata);
	devm_kfree(&client->dev, prs);

#ifdef CONFIG_OF
	kfree(client->dev.platform_data);
#endif

	return 0;
}

#ifdef CONFIG_PM

static int lps331ap_resume(struct device *dev)
{
	struct i2c_client *client = container_of((struct device *)dev,
						 struct i2c_client, dev);


	struct lps331ap_prs_data *prs = i2c_get_clientdata(client);

	if (prs->on_before_suspend)
		return lps331ap_prs_enable(prs);
	return 0;
}

static int lps331ap_suspend(struct device *dev)
{
	struct i2c_client *client = container_of((struct device *)dev,
						 struct i2c_client, dev);

	struct lps331ap_prs_data *prs = i2c_get_clientdata(client);
	prs->on_before_suspend = atomic_read(&prs->enabled);
	return lps331ap_prs_disable(prs);
}


static const struct dev_pm_ops lps331ap_pm_ops = {
	.suspend = lps331ap_suspend,
	.resume = lps331ap_resume,
};

#define LPS331AP_PM_OPS (&lps331ap_pm_ops)
#else
#define LPS331AP_PM_OPS NULL
#endif /* CONFIG_PM */

#if DEBUG
static void lps331ap_dump_registers(struct lps331ap_prs_data *prs)
{
	int ret;
	/*
	 * Data bytes from hardware     PRESS_OUT_XL,PRESS_OUT_L,PRESS_OUT_H,
	 *                              TEMP_OUT_L, TEMP_OUT_H
	 */
	u8 prs_data[13];
	int reg_to_read = 9;
	int i;
	prs_data[0] = (I2C_AUTO_INCREMENT | 0x08);
	ret = lps331ap_prs_i2c_read(prs, prs_data, reg_to_read);
	if (ret < 0)
		dev_info(&prs->client->dev, "%s error\n", __func__);
	for (i = 0; i <= 8; i++)
		dev_info(&prs->client->dev, "prs_register[0x%x]=0x%x\n", i + 8,
			 prs_data[i]);

	reg_to_read = 13;
	prs_data[0] = (I2C_AUTO_INCREMENT | 0x20);
	ret = lps331ap_prs_i2c_read(prs, prs_data, reg_to_read);
	if (ret < 0)
		dev_info(&prs->client->dev, "error %s\n", __func__);
	for (i = 0; i <= 12; i++)
		dev_info(&prs->client->dev, "prs_register[0x%x]=0x%x\n", i + 32,
			 prs_data[i]);
}
#endif

static struct of_device_id lps331ap_prs_dt_ids[] = {
	{ .compatible = "st,lps331ap", },
	{}
};

static const struct i2c_device_id lps331ap_prs_id[]
= { {LPS331AP_PRS_DEV_NAME, 0}, {}, };

MODULE_DEVICE_TABLE(i2c, lps331ap_prs_id);

static struct i2c_driver lps331ap_prs_driver = {
	.driver = {
		   .name  = LPS331AP_PRS_DEV_NAME,
		   .owner = THIS_MODULE,
		   .pm    = LPS331AP_PM_OPS,
		   .of_match_table = of_match_ptr(lps331ap_prs_dt_ids),
		   },
	.probe = lps331ap_prs_probe,
	.remove = lps331ap_prs_remove,
	.id_table = lps331ap_prs_id,
};

module_i2c_driver(lps331ap_prs_driver);

MODULE_DESCRIPTION("STMicrolelectronics lps331ap pressure sensor driver");
MODULE_AUTHOR("Matteo Dameno, Carmine Iascone, STMicroelectronics.");
MODULE_AUTHOR("Lu Cao <lucao@marvell.com>");
MODULE_LICENSE("GPL v2");
