/*!
 * @section LICENSE
 *
 * (C) Copyright 2013 Bosch Sensortec GmbH All Rights Reserved
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *------------------------------------------------------------------------------
 * Disclaimer
 *
 * Common: Bosch Sensortec products are developed for the consumer goods
 * industry. They may only be used within the parameters of the respective valid
 * product data sheet.  Bosch Sensortec products are provided with the express
 * understanding that there is no warranty of fitness for a particular purpose.
 * They are not fit for use in life-sustaining, safety or security sensitive
 * systems or any system or device that may lead to bodily harm or property
 * damage if the system or device malfunctions. In addition, Bosch Sensortec
 * products are not fit for use in products which interact with motor vehicle
 * systems.  The resale and/or use of products are at the purchaser's own risk
 * and his own responsibility. The examination of fitness for the intended use
 * is the sole responsibility of the Purchaser.
 *
 * The purchaser shall indemnify Bosch Sensortec from all third party claims,
 * including any claims for incidental, or consequential damages, arising from
 * any product use not covered by the parameters of the respective valid product
 * data sheet or not approved by Bosch Sensortec and reimburse Bosch Sensortec
 * for all costs in connection with such claims.
 *
 * The purchaser must monitor the market for the purchased products,
 * particularly with regard to product safety and inform Bosch Sensortec without
 * delay of all security relevant incidents.
 *
 * Engineering Samples are marked with an asterisk (*) or (e). Samples may vary
 * from the valid technical specifications of the product series. They are
 * therefore not intended or fit for resale to third parties or for use in end
 * products. Their sole purpose is internal client testing. The testing of an
 * engineering sample may in no way replace the testing of a product series.
 * Bosch Sensortec assumes no liability for the use of engineering samples. By
 * accepting the engineering samples, the Purchaser agrees to indemnify Bosch
 * Sensortec from all claims arising from the use of engineering samples.
 *
 * Special: This software module (hereinafter called "Software") and any
 * information on application-sheets (hereinafter called "Information") is
 * provided free of charge for the sole purpose to support your application
 * work. The Software and Information is subject to the following terms and
 * conditions:
 *
 * The Software is specifically designed for the exclusive use for Bosch
 * Sensortec products by personnel who have special experience and training. Do
 * not use this Software if you do not have the proper experience or training.
 *
 * This Software package is provided `` as is `` and without any expressed or
 * implied warranties, including without limitation, the implied warranties of
 * merchantability and fitness for a particular purpose.
 *
 * Bosch Sensortec and their representatives and agents deny any liability for
 * the functional impairment of this Software in terms of fitness, performance
 * and safety. Bosch Sensortec and their representatives and agents shall not be
 * liable for any direct or indirect damages or injury, except as otherwise
 * stipulated in mandatory applicable law.
 *
 * The Information provided is believed to be accurate and reliable. Bosch
 * Sensortec assumes no responsibility for the consequences of use of such
 * Information nor for any infringement of patents or other rights of third
 * parties which may result from its use.
 *
 * @filename bmp18x-core.c
 * @date	 "Mon Jul 8 10:27:33 2013 +0800"
 * @id		 "4fd82f5"
 *
 * @brief
 * The core code of BMP18X device driver
 *
 * @detail
 * This file implements the core code of BMP18X device driver,
 * which includes hardware related functions, input device register,
 * device attribute files, etc.
 *
 * Copyright (c) 2011  Bosch Sensortec GmbH
 * Copyright (c) 2011  Unixphere
 *
 * Based on:
 * BMP085 driver, bmp085.c
 * Copyright (c) 2010  Christoph Mair <christoph.mair@gmail.com>
 *
 * This driver supports the bmp18x digital barometric pressure
 * and temperature sensors from Bosch Sensortec.
 *
 * A pressure measurement is issued by reading from pressure0_input.
 * The return value ranges from 30000 to 110000 pascal with a resulution
 * of 1 pascal (0.01 millibar) which enables measurements from 9000m above
 * to 500m below sea level.
 *
 * The temperature can be read from temp0_input. Values range from
 * -400 to 850 representing the ambient temperature in degree celsius
 * multiplied by 10.The resolution is 0.1 celsius.
 *
 * Because ambient pressure is temperature dependent, a temperature
 * measurement will be executed automatically even if the user is reading
 * from pressure0_input. This happens if the last temperature measurement
 * has been executed more then one second ago.
 *
 * To decrease RMS noise from pressure measurements, the bmp18x can
 * autonomously calculate the average of up to eight samples. This is
 * set up by writing to the oversampling sysfs file. Accepted values
 * are 0, 1, 2 and 3. 2^x when x is the value written to this file
 * specifies the number of samples used to calculate the ambient pressure.
 * RMS noise is specified with six pascal (without averaging) and decreases
 * down to 3 pascal when using an oversampling setting of 3.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * REVISION:V1.3
 * HISTORY: V1.0 --- Driver code creation.
 *          V1.1 --- Add sw oversampling.
 *          V1.2 --- 1.Add module.h and change license to GPLv2.
 *                   2.Use EV_MSC instead of EV_ABS to pass the same data.
 *          V1.3 --- Add selftest function.
*/

#include <linux/device.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/module.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/regulator/consumer.h>

#include "bmp18x.h"

#define BMP18X_CHIP_ID			0x55

#define BMP18X_CALIBRATION_DATA_START	0xAA
#define BMP18X_CALIBRATION_DATA_LENGTH	11	/* 16 bit values */
#define BMP18X_CHIP_ID_REG		0xD0
#define BMP18X_CTRL_REG			0xF4
#define BMP18X_TEMP_MEASUREMENT		0x2E
#define BMP18X_PRESSURE_MEASUREMENT	0x34
#define BMP18X_CONVERSION_REGISTER_MSB	0xF6
#define BMP18X_CONVERSION_REGISTER_LSB	0xF7
#define BMP18X_CONVERSION_REGISTER_XLSB	0xF8
#define BMP18X_CRC_REG_START		0x80
#define BMP18X_TEMP_CONVERSION_TIME	5

#define ABS_MIN_PRESSURE	30000
#define ABS_MAX_PRESSURE	120000
#define BMP_DELAY_DEFAULT   200

struct bmp18x_calibration_data {
	s16 AC1, AC2, AC3;
	u16 AC4, AC5, AC6;
	s16 B1, B2;
	s16 MB, MC, MD;
};

/* Each client has this additional data */
struct bmp18x_data {
	struct	bmp18x_data_bus data_bus;
	struct	device *dev;
	struct	mutex lock;
	struct	bmp18x_calibration_data calibration;
	struct	regulator	*avdd;
	struct	bmp18x_platform_data	*pdata;
	u8	oversampling_setting;
	u8	sw_oversampling_setting;
	u32	raw_temperature;
	u32	raw_pressure;
	u32	temp_measurement_period;
	u32	last_temp_measurement;
	s32	b6; /* calculated temperature correction coefficient */
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
	struct input_dev	*input;
	struct delayed_work work;
	u32					delay;
	u32					enable;
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void bmp18x_early_suspend(struct early_suspend *h);
static void bmp18x_late_resume(struct early_suspend *h);
#endif

static s32 bmp18x_read_calibration_data(struct bmp18x_data *data)
{
	u16 tmp[BMP18X_CALIBRATION_DATA_LENGTH];
	struct bmp18x_calibration_data *cali = &(data->calibration);
	s32 status = data->data_bus.bops->read_block(data->data_bus.client,
				BMP18X_CALIBRATION_DATA_START,
				BMP18X_CALIBRATION_DATA_LENGTH*sizeof(u16),
				(u8 *)tmp);
	if (status < 0)
		return status;

	if (status != BMP18X_CALIBRATION_DATA_LENGTH*sizeof(u16))
		return -EIO;

	cali->AC1 =  be16_to_cpu(tmp[0]);
	cali->AC2 =  be16_to_cpu(tmp[1]);
	cali->AC3 =  be16_to_cpu(tmp[2]);
	cali->AC4 =  be16_to_cpu(tmp[3]);
	cali->AC5 =  be16_to_cpu(tmp[4]);
	cali->AC6 = be16_to_cpu(tmp[5]);
	cali->B1 = be16_to_cpu(tmp[6]);
	cali->B2 = be16_to_cpu(tmp[7]);
	cali->MB = be16_to_cpu(tmp[8]);
	cali->MC = be16_to_cpu(tmp[9]);
	cali->MD = be16_to_cpu(tmp[10]);
	return 0;
}


static s32 bmp18x_update_raw_temperature(struct bmp18x_data *data)
{
	u16 tmp;
	s32 status;

	mutex_lock(&data->lock);
	status = data->data_bus.bops->write_byte(data->data_bus.client,
				BMP18X_CTRL_REG, BMP18X_TEMP_MEASUREMENT);
	if (status != 0) {
		dev_err(data->dev,
			"Error while requesting temperature measurement.\n");
		goto exit;
	}
	msleep(BMP18X_TEMP_CONVERSION_TIME);

	status = data->data_bus.bops->read_block(data->data_bus.client,
		BMP18X_CONVERSION_REGISTER_MSB, sizeof(tmp), (u8 *)&tmp);
	if (status < 0)
		goto exit;
	if (status != sizeof(tmp)) {
		dev_err(data->dev,
			"Error while reading temperature measurement result\n");
		status = -EIO;
		goto exit;
	}
	data->raw_temperature = be16_to_cpu(tmp);
	data->last_temp_measurement = jiffies;
	status = 0;	/* everything ok, return 0 */

exit:
	mutex_unlock(&data->lock);
	return status;
}

static s32 bmp18x_update_raw_pressure(struct bmp18x_data *data)
{
	u32 tmp = 0;
	s32 status;

	mutex_lock(&data->lock);
	status = data->data_bus.bops->write_byte(data->data_bus.client,
		BMP18X_CTRL_REG, BMP18X_PRESSURE_MEASUREMENT +
		(data->oversampling_setting<<6));
	if (status != 0) {
		dev_err(data->dev,
			"Error while requesting pressure measurement.\n");
		goto exit;
	}

	/* wait for the end of conversion */
	msleep(2+(3 << data->oversampling_setting));

	/* copy data into a u32 (4 bytes), but skip the first byte. */
	status = data->data_bus.bops->read_block(data->data_bus.client,
			BMP18X_CONVERSION_REGISTER_MSB, 3, ((u8 *)&tmp)+1);
	if (status < 0)
		goto exit;
	if (status != 3) {
		dev_err(data->dev,
			"Error while reading pressure measurement results\n");
		status = -EIO;
		goto exit;
	}
	data->raw_pressure = be32_to_cpu((tmp));
	data->raw_pressure >>= (8-data->oversampling_setting);
	status = 0;	/* everything ok, return 0 */

exit:
	mutex_unlock(&data->lock);
	return status;
}


/*
 * This function starts the temperature measurement and returns the value
 * in tenth of a degree celsius.
 */
static s32 bmp18x_get_temperature(struct bmp18x_data *data, int *temperature)
{
	struct bmp18x_calibration_data *cali = &data->calibration;
	long x1, x2;
	int status;

	status = bmp18x_update_raw_temperature(data);
	if (status != 0)
		goto exit;

	x1 = ((data->raw_temperature - cali->AC6) * cali->AC5) >> 15;
	x2 = (cali->MC << 11) / (x1 + cali->MD);
	data->b6 = x1 + x2 - 4000;
	/* if NULL just update b6. Used for pressure only measurements */
	if (temperature != NULL)
		*temperature = (x1+x2+8) >> 4;

exit:
	return status;
}

/*
 * This function starts the pressure measurement and returns the value
 * in millibar. Since the pressure depends on the ambient temperature,
 * a temperature measurement is executed according to the given temperature
 * measurememt period (default is 1 sec boundary). This period could vary
 * and needs to be adjusted accoring to the sensor environment, i.e. if big
 * temperature variations then the temperature needs to be read out often.
 */
static s32 bmp18x_get_pressure(struct bmp18x_data *data, int *pressure)
{
	struct bmp18x_calibration_data *cali = &data->calibration;
	s32 x1, x2, x3, b3;
	u32 b4, b7;
	s32 p;
	int status;
	int i_loop, i;
	u32 p_tmp;

	/* update the ambient temperature according to the given meas. period */
	if (data->last_temp_measurement +
			data->temp_measurement_period < jiffies) {
		status = bmp18x_get_temperature(data, NULL);
		if (status != 0)
			goto exit;
	}

	if ((data->oversampling_setting == 3)
		&& (data->sw_oversampling_setting == 1)) {
		i_loop = 3;
	} else {
		i_loop = 1;
	}

	p_tmp = 0;
	for (i = 0; i < i_loop; i++) {
		status = bmp18x_update_raw_pressure(data);
		if (status != 0)
			goto exit;
		p_tmp += data->raw_pressure;
	}

	data->raw_pressure = (p_tmp + (i_loop >> 1)) / i_loop;

	x1 = (data->b6 * data->b6) >> 12;
	x1 *= cali->B2;
	x1 >>= 11;

	x2 = cali->AC2 * data->b6;
	x2 >>= 11;

	x3 = x1 + x2;

	b3 = (((((s32)cali->AC1) * 4 + x3) << data->oversampling_setting) + 2);
	b3 >>= 2;

	x1 = (cali->AC3 * data->b6) >> 13;
	x2 = (cali->B1 * ((data->b6 * data->b6) >> 12)) >> 16;
	x3 = (x1 + x2 + 2) >> 2;
	b4 = (cali->AC4 * (u32)(x3 + 32768)) >> 15;

	b7 = ((u32)data->raw_pressure - b3) *
					(50000 >> data->oversampling_setting);
	p = ((b7 < 0x80000000) ? ((b7 << 1) / b4) : ((b7 / b4) * 2));

	x1 = p >> 8;
	x1 *= x1;
	x1 = (x1 * 3038) >> 16;
	x2 = (-7357 * p) >> 16;
	p += (x1 + x2 + 3791) >> 4;

	*pressure = p;

exit:
	return status;
}

/*
 * This function sets the chip-internal oversampling. Valid values are 0..3.
 * The chip will use 2^oversampling samples for internal averaging.
 * This influences the measurement time and the accuracy; larger values
 * increase both. The datasheet gives on overview on how measurement time,
 * accuracy and noise correlate.
 */
static void bmp18x_set_oversampling(struct bmp18x_data *data,
						unsigned char oversampling)
{
	if (oversampling > 3)
		oversampling = 3;
	data->oversampling_setting = oversampling;
}

/*
 * Returns the currently selected oversampling. Range: 0..3
 */
static unsigned char bmp18x_get_oversampling(struct bmp18x_data *data)
{
	return data->oversampling_setting;
}

static int bmp18x_check_calib_param(struct bmp18x_data *data)
{
	struct bmp18x_calibration_data *cali = &(data->calibration);

	/* check that not all calibration parameters are 0 */
	if (cali->AC1 == 0 && cali->AC2 == 0 && cali->AC3 == 0
		&& cali->AC4 == 0 && cali->AC5 == 0 && cali->AC6 == 0) {
		dev_err(data->dev, "all calibration parameters are zero\n");
		return 1;
	}

	/*
	* check whether all the calibration parameters are
	* within 4 sigma range
	*/
	if (cali->AC1 <= -29250 || cali->AC1 >= 32750)
		return 2;
	else if (cali->AC2 <= -4820 || cali->AC2 >= 3626)
		return 3;
	else if (cali->AC3 <= -16660 || cali->AC3 >= -13151)
		return 4;
	else if (cali->AC4 <= 20780 || cali->AC4 >= 44820)
		return 5;
	else if (cali->AC5 <= 21730 || cali->AC5 >= 34475)
		return 6;
	else if (cali->AC6 <= 500 || cali->AC6 >= 56200)
		return 7;
	else if (cali->B1 <= 4935 || cali->B1 >= 8637)
		return 8;
	else if (cali->B2 < -182 || cali->B2 >= 297)
		return 9;

	dev_info(data->dev, "calibration parameters are OK\n");
	return 0;
}

static int bmp18x_check_pt(struct bmp18x_data *data)
{
	int temperature = 0;
	int pressure;

	/* check ut and t */
	bmp18x_get_temperature(data, &temperature);
	if (data->raw_temperature == 0 || data->raw_temperature < 16000
		|| data->raw_temperature == 65535) {
		dev_err(data->dev, "ut is out of range:%d\n",
			data->raw_temperature);
		return 10;
	}
	if (temperature < 0 || temperature >= 40*10) {
		dev_err(data->dev, "temperature value is out of range:%d*0.01degree\n",
			temperature);
		return 11;
	}

	/* check up and p */
	bmp18x_get_pressure(data, &pressure);
	if (data->raw_pressure == 0 || data->raw_pressure < 15000
		|| data->raw_pressure == 65535) {
		dev_err(data->dev, "up is out of range:%d\n",
			data->raw_pressure);
		return 12;
	}
	if (pressure < 900*100 || pressure > 1100*100) {
		dev_err(data->dev, "pressure value is out of range:%d Pa\n",
			pressure);
		return 13;
	}

	dev_info(data->dev, "bmp18x temperature and pressure values are OK\n");
	return 0;
}

static unsigned char bmp18x_st_calc_crc(unsigned char seed, unsigned char data)
{
	unsigned char poly = 0x1D;
	unsigned char bit, din;

	for (bit = 0; bit < 8; bit++) {
		if (((seed & 0x80) > 0) ^ ((data & 0x80) > 0))
			din = 1;
		else
			din = 0;
		seed = (seed & 0x7F) << 1;
		data = (data & 0x7F) << 1;
		seed = seed ^ (poly * din);
	}

	return seed;
}

static int bmp18x_check_crc(struct bmp18x_data *data)
{
	unsigned char i;
	unsigned char current_register;
	unsigned char crc_val;
	unsigned char registers[32];
	s32 status;

	/* read all relevant registers for CRC calculation */
	status = data->data_bus.bops->read_block(data->data_bus.client,
			BMP18X_CRC_REG_START,
			32,
			registers);
	if (status < 0)
		return 14;

	crc_val = 0xFF;

	for (i = 0; i < 32; i++) {
		current_register = registers[i];
		if (i != 4) /* that is ee_crc register */
			crc_val = bmp18x_st_calc_crc(crc_val, current_register);
	}

	crc_val = (crc_val ^ 0xFF);
	if (crc_val == registers[4]) /* calculated crc is correct */
		return 0;
	else /* data or crc is wrong */
		return 15;
}

static int bmp18x_do_selftest(struct bmp18x_data *data)
{
	int err = 0;
	/* 0: failed, 1: success */
	u8 selftest;

	err = bmp18x_check_calib_param(data);
	if (err) {
		selftest = 0;
		dev_err(data->dev, "bmp18x_check_calib_param:err=%d\n", err);
		goto exit;
	}

	err = bmp18x_check_pt(data);
	if (err) {
		selftest = 0;
		dev_err(data->dev, "bmp18x_check_pt:err=%d\n", err);
		goto exit;
	}

	err = bmp18x_check_crc(data);
	if (err) {
		selftest = 0;
		dev_err(data->dev, "bmp18x_check_crc:err=%d\n", err);
		goto exit;
	}

	/* selftest is OK */
	selftest = 1;
	dev_info(data->dev, "bmp18x self test is OK\n");
exit:
	return selftest;
}

/* sysfs callbacks */
static ssize_t set_oversampling(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct bmp18x_data *data = dev_get_drvdata(dev);
	unsigned long oversampling;
	int success = kstrtoul(buf, 10, &oversampling);
	if (success == 0) {
		mutex_lock(&data->lock);
		bmp18x_set_oversampling(data, oversampling);
		if (oversampling != 3)
			data->sw_oversampling_setting = 0;
		mutex_unlock(&data->lock);
		return count;
	}
	return success;
}

static ssize_t show_oversampling(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct bmp18x_data *data = dev_get_drvdata(dev);
	return sprintf(buf, "%u\n", bmp18x_get_oversampling(data));
}
static DEVICE_ATTR(oversampling, S_IWUSR | S_IRUGO,
					show_oversampling, set_oversampling);

static ssize_t set_sw_oversampling(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct bmp18x_data *data = dev_get_drvdata(dev);
	unsigned long sw_oversampling;
	int success = kstrtoul(buf, 10, &sw_oversampling);
	if (success == 0) {
		mutex_lock(&data->lock);
		data->sw_oversampling_setting = sw_oversampling ? 1 : 0;
		mutex_unlock(&data->lock);
		return count;
	}
	return success;
}

static ssize_t show_sw_oversampling(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct bmp18x_data *data = dev_get_drvdata(dev);
	return sprintf(buf, "%u\n", data->sw_oversampling_setting);
}
static DEVICE_ATTR(sw_oversampling, S_IWUSR | S_IRUGO,
				show_sw_oversampling, set_sw_oversampling);

static ssize_t show_delay(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct bmp18x_data *data = dev_get_drvdata(dev);
	return sprintf(buf, "%u\n", data->delay);
}

static ssize_t set_delay(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct bmp18x_data *data = dev_get_drvdata(dev);
	unsigned long delay;
	int success = kstrtoul(buf, 10, &delay);
	if (success == 0) {
		mutex_lock(&data->lock);
		data->delay = delay;
		mutex_unlock(&data->lock);
		return count;
	}
	return success;
}
static DEVICE_ATTR(interval, S_IWUSR | S_IRUGO,
				show_delay, set_delay);

static ssize_t show_enable(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct bmp18x_data *data = dev_get_drvdata(dev);
	return sprintf(buf, "%u\n", data->enable);
}

static ssize_t set_enable(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct bmp18x_data *data = dev_get_drvdata(dev);
	unsigned long enable;
	int success = kstrtoul(buf, 10, &enable);
	if (success == 0) {
		mutex_lock(&data->lock);
		data->enable = enable ? 1 : 0;

		if (data->enable) {
			bmp18x_enable(dev);
		} else {
			bmp18x_disable(dev);
		}
		mutex_unlock(&data->lock);
		return count;
	}
	return success;
}
static DEVICE_ATTR(active, S_IWUSR | S_IRUGO,
				show_enable, set_enable);

static ssize_t show_temperature(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int temperature;
	int status;
	struct bmp18x_data *data = dev_get_drvdata(dev);

	status = bmp18x_get_temperature(data, &temperature);
	if (status != 0)
		return status;
	else
		return sprintf(buf, "%d\n", temperature);
}
static DEVICE_ATTR(temp0_input, S_IRUGO, show_temperature, NULL);


static ssize_t show_pressure(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	int pressure;
	int status;
	struct bmp18x_data *data = dev_get_drvdata(dev);

	status = bmp18x_get_pressure(data, &pressure);
	if (status != 0)
		return status;
	else
		return sprintf(buf, "%d\n", pressure);
}
static DEVICE_ATTR(pressure0_input, S_IRUGO, show_pressure, NULL);

static ssize_t show_selftest(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct bmp18x_data *data = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", bmp18x_do_selftest(data));
}

static DEVICE_ATTR(selftest, S_IWUSR | S_IRUGO, show_selftest, NULL);

static struct attribute *bmp18x_attributes[] = {
	&dev_attr_temp0_input.attr,
	&dev_attr_pressure0_input.attr,
	&dev_attr_oversampling.attr,
	&dev_attr_sw_oversampling.attr,
	&dev_attr_interval.attr,
	&dev_attr_active.attr,
	&dev_attr_selftest.attr,
	NULL
};

static const struct attribute_group bmp18x_attr_group = {
	.attrs = bmp18x_attributes,
};

static void bmp18x_work_func(struct work_struct *work)
{
	struct bmp18x_data *client_data =
		container_of((struct delayed_work *)work,
		struct bmp18x_data, work);
	unsigned long delay = msecs_to_jiffies(client_data->delay);
	unsigned long j1 = jiffies;
	int pressure;
	int status;

	status = bmp18x_get_pressure(client_data, &pressure);

	if (status == 0) {
		input_event(client_data->input, EV_MSC, MSC_RAW, pressure);
		input_sync(client_data->input);
	}

	schedule_delayed_work(&client_data->work, delay-(jiffies-j1));
}

static int bmp18x_input_init(struct bmp18x_data *data)
{
	struct input_dev *dev;
	int err;

	dev = input_allocate_device();
	if (!dev)
		return -ENOMEM;
	dev->name = BMP18X_NAME;
	dev->id.bustype = BUS_I2C;

	input_set_capability(dev, EV_MSC, MSC_RAW);
	input_set_drvdata(dev, data);

	err = input_register_device(dev);
	if (err < 0) {
		input_free_device(dev);
		return err;
	}
	data->input = dev;

	return 0;
}

static void bmp18x_input_delete(struct bmp18x_data *data)
{
	struct input_dev *dev = data->input;

	input_unregister_device(dev);
	input_free_device(dev);
}

static int bmp18x_init_client(struct bmp18x_data *data,
			      struct bmp18x_platform_data *pdata)
{
	int status = bmp18x_read_calibration_data(data);
	if (status != 0)
		goto exit;
	data->last_temp_measurement = 0;
	data->temp_measurement_period =
		pdata ? (pdata->temp_measurement_period/1000)*HZ : 1*HZ;
	data->oversampling_setting = pdata ? pdata->default_oversampling : 3;
	if (data->oversampling_setting == 3)
		data->sw_oversampling_setting
			= pdata ? pdata->default_sw_oversampling : 0;
	mutex_init(&data->lock);
exit:
	return status;
}

int bmp18x_probe(struct device *dev, struct bmp18x_data_bus *data_bus)
{
	struct bmp18x_data *data;
	struct bmp18x_platform_data *pdata = dev->platform_data;
	u8 chip_id = pdata && pdata->chip_id ? pdata->chip_id : BMP18X_CHIP_ID;
	int err = 0;
	struct regulator *avdd = NULL;

	avdd = regulator_get(dev, "avdd");
	if (IS_ERR(avdd)) {
		dev_err(dev, "BOSCH sensor avdd power supply get failed\n");
		goto out;
	}
	regulator_set_voltage(avdd, 2800000, 2800000);
	if (regulator_enable(avdd)) {
		dev_err(dev, "BOSCH sensor regulator enable failed\n");
		goto out;
	}

	if (data_bus->bops->read_byte(data_bus->client,
			BMP18X_CHIP_ID_REG) != chip_id) {
		pr_debug(KERN_ERR "%s: chip_id failed!\n", BMP18X_NAME);
		err = -ENODEV;
		goto exit;
	}

	data = kzalloc(sizeof(struct bmp18x_data), GFP_KERNEL);
	if (!data) {
		err = -ENOMEM;
		goto exit;
	}

	dev_set_drvdata(dev, data);
	data->data_bus = *data_bus;
	data->dev = dev;

	data->avdd = avdd;
	/* Initialize the BMP18X chip */
	err = bmp18x_init_client(data, pdata);
	if (err != 0)
		goto exit_free;

	/* Initialize the BMP18X input device */
	err = bmp18x_input_init(data);
	if (err != 0)
		goto exit_free;

	/* Register sysfs hooks */
	err = sysfs_create_group(&data->input->dev.kobj, &bmp18x_attr_group);
	if (err)
		goto error_sysfs;
	/* workqueue init */
	INIT_DELAYED_WORK(&data->work, bmp18x_work_func);
	data->delay  = BMP_DELAY_DEFAULT;
	data->enable = 0;

#ifdef CONFIG_HAS_EARLYSUSPEND
	data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	data->early_suspend.suspend = bmp18x_early_suspend;
	data->early_suspend.resume = bmp18x_late_resume;
	register_early_suspend(&data->early_suspend);
#endif

	dev_info(dev, "Succesfully initialized bmp18x!\n");
	regulator_disable(avdd);
	return 0;

error_sysfs:
	bmp18x_input_delete(data);
exit_free:
	kfree(data);
exit:
	regulator_disable(avdd);
out:
	regulator_put(avdd);
	return err;
}
EXPORT_SYMBOL(bmp18x_probe);

int bmp18x_remove(struct device *dev)
{
	struct bmp18x_data *data = dev_get_drvdata(dev);
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&data->early_suspend);
#endif
	sysfs_remove_group(&data->input->dev.kobj, &bmp18x_attr_group);
	kfree(data);

	return 0;
}
EXPORT_SYMBOL(bmp18x_remove);

#ifdef CONFIG_PM
int bmp18x_disable(struct device *dev)
{
	struct bmp18x_data *data = dev_get_drvdata(dev);

	if (data->enable) {
		cancel_delayed_work_sync(&data->work);
		regulator_disable(data->avdd);
	}

	return 0;
}
EXPORT_SYMBOL(bmp18x_disable);

int bmp18x_enable(struct device *dev)
{
	struct bmp18x_data *data = dev_get_drvdata(dev);
	if (data->enable) {
		if (regulator_enable(data->avdd)) {
			dev_err(dev, "BOSCH sensor regulator enable failed\n");
			goto out;
		}
		schedule_delayed_work(&data->work,
					msecs_to_jiffies(data->delay));
	}
out:
	return 0;
}
EXPORT_SYMBOL(bmp18x_enable);
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static void bmp18x_early_suspend(struct early_suspend *h)
{
	struct bmp18x_data *data =
		container_of(h, struct bmp18x_data, early_suspend);
	if (data->enable) {
		cancel_delayed_work_sync(&data->work);
		(void) bmp18x_disable(data->dev);
	}
}

static void bmp18x_late_resume(struct early_suspend *h)
{
	struct bmp18x_data *data =
		container_of(h, struct bmp18x_data, early_suspend);

	if (data->enable) {
		(void) bmp18x_enable(data->dev);
		schedule_delayed_work(&data->work,
					msecs_to_jiffies(data->delay));
	}

}
#endif

MODULE_AUTHOR("Eric Andersson <eric.andersson@unixphere.com>");
MODULE_DESCRIPTION("BMP18X driver");
MODULE_LICENSE("GPLv2");
