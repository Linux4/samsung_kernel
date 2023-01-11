/*
 * Device driver for monitoring ambient light intensity in (lux)
 * and proximity detection (prox) within the TAOS TSL2X7X family of devices.
 *
 * Copyright (c) 2012, TAOS Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA        02110-1301, USA.
 */

#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/workqueue.h>
#include <linux/pm_runtime.h>
#include <linux/of_gpio.h>
#include "../iio.h"
#include "../sysfs.h"
#include "../events.h"
#include "../ring_sw.h"
#include "../trigger.h"
#include "../trigger_consumer.h"
#include "tsl2x7x.h"

/* Cal defs*/
#define PROX_STAT_CAL        0
#define PROX_STAT_SAMP       1
#define MAX_SAMPLES_CAL      200

/* TSL2X7X Device ID */
#define TRITON_ID    0x00
#define SWORDFISH_ID 0x30
#define HALIBUT_ID   0x20

/* Lux calculation constants */
#define TSL2X7X_LUX_CALC_OVER_FLOW     65535

/* TAOS Register definitions - note:
 * depending on device, some of these register are not used and the
 * register address is benign.
 */
/* 2X7X register offsets */
#define TSL2X7X_MAX_CONFIG_REG         16

/* Device Registers and Masks */
#define TSL2X7X_CNTRL                  0x00
#define TSL2X7X_ALS_TIME               0X01
#define TSL2X7X_PRX_TIME               0x02
#define TSL2X7X_WAIT_TIME              0x03
#define TSL2X7X_ALS_MINTHRESHLO        0X04
#define TSL2X7X_ALS_MINTHRESHHI        0X05
#define TSL2X7X_ALS_MAXTHRESHLO        0X06
#define TSL2X7X_ALS_MAXTHRESHHI        0X07
#define TSL2X7X_PRX_MINTHRESHLO        0X08
#define TSL2X7X_PRX_MINTHRESHHI        0X09
#define TSL2X7X_PRX_MAXTHRESHLO        0X0A
#define TSL2X7X_PRX_MAXTHRESHHI        0X0B
#define TSL2X7X_PERSISTENCE            0x0C
#define TSL2X7X_PRX_CONFIG             0x0D
#define TSL2X7X_PRX_COUNT              0x0E
#define TSL2X7X_GAIN                   0x0F
#define TSL2X7X_NOTUSED                0x10
#define TSL2X7X_REVID                  0x11
#define TSL2X7X_CHIPID                 0x12
#define TSL2X7X_STATUS                 0x13
#define TSL2X7X_ALS_CHAN0LO            0x14
#define TSL2X7X_ALS_CHAN0HI            0x15
#define TSL2X7X_ALS_CHAN1LO            0x16
#define TSL2X7X_ALS_CHAN1HI            0x17
#define TSL2X7X_PRX_LO                 0x18
#define TSL2X7X_PRX_HI                 0x19

/* tsl2X7X cmd reg masks */
#define TSL2X7X_CMD_REG                0x80
#define TSL2X7X_CMD_SPL_FN             0x60

#define TSL2X7X_CMD_PROX_INT_CLR       0X05
#define TSL2X7X_CMD_ALS_INT_CLR        0x06
#define TSL2X7X_CMD_PROXALS_INT_CLR    0X07

/* tsl2X7X cntrl reg masks */
#define TSL2X7X_CNTL_ADC_ENBL          0x02
#define TSL2X7X_CNTL_PWR_ON            0x01

/* tsl2X7X status reg masks */
#define TSL2X7X_STA_ADC_VALID          0x01
#define TSL2X7X_STA_PRX_VALID          0x02
#define TSL2X7X_STA_ADC_PRX_VALID      (TSL2X7X_STA_ADC_VALID |\
		TSL2X7X_STA_PRX_VALID)
#define TSL2X7X_STA_ALS_INTR           0x10
#define TSL2X7X_STA_PRX_INTR           0x20

/* tsl2X7X cntrl reg masks */
#define TSL2X7X_CNTL_REG_CLEAR         0x00
#define TSL2X7X_CNTL_PROX_INT_ENBL     0X20
#define TSL2X7X_CNTL_ALS_INT_ENBL      0X10
#define TSL2X7X_CNTL_WAIT_TMR_ENBL     0X08
#define TSL2X7X_CNTL_PROX_DET_ENBL     0X04
#define TSL2X7X_CNTL_PWRON             0x01
#define TSL2X7X_CNTL_ALSPON_ENBL       0x03
#define TSL2X7X_CNTL_INTALSPON_ENBL    0x13
#define TSL2X7X_CNTL_PROXPON_ENBL      0x0F
#define TSL2X7X_CNTL_INTPROXPON_ENBL   0x2F

/*Prox diode to use */
#define TSL2X7X_DIODE0                 0x10
#define TSL2X7X_DIODE1                 0x20
#define TSL2X7X_DIODE_BOTH             0x30

/* LED Power */
#define TSL2X7X_mA100                  0x00
#define TSL2X7X_mA50                   0x40
#define TSL2X7X_mA25                   0x80
#define TSL2X7X_mA13                   0xD0
#define TSL2X7X_MAX_TIMER_CNT          (0xFF)

/*Common device IIO EventMask */
#define TSL2X7X_EVENT_MASK \
	(IIO_EV_BIT(IIO_EV_TYPE_THRESH, IIO_EV_DIR_RISING) | \
	 IIO_EV_BIT(IIO_EV_TYPE_THRESH, IIO_EV_DIR_FALLING)),

#define TSL2X7X_MIN_ITIME_X100 272

#define DEVICE_ATTR2(_name, _mode, _show, _store) \
	struct device_attribute dev_attr2_##_name = \
	__ATTR(_name, _mode, _show, _store)


#define PL_DEGUG_ON 0
#define PL_DEGUG(fmt, args...) do { \
	if (PL_DEGUG_ON) \
		pr_info("[PL][%s]" fmt, __func__, ##args); \
} while (0)


/* TAOS txx2x7x Device family members */
enum {
	tsl2571,
	tsl2671,
	tmd2671,
	tsl2771,
	tmd2771,
	tsl2572,
	tsl2672,
	tmd2672,
	tsl2772,
	tmd2772
};

enum {
	TSL2X7X_CHIP_UNKNOWN = 0,
	TSL2X7X_CHIP_WORKING = 1,
	TSL2X7X_CHIP_SUSPENDED = 2
};

struct tsl2x7x_parse_result {
	int integer;
	int fract;
};

/* Per-device data */
struct tsl2x7x_als_info {
	u16 als_ch0;
	u16 als_ch1;
	u16 lux;
};

struct tsl2x7x_prox_stat {
	int min;
	int max;
	int mean;
	unsigned long stddev;
};

struct tsl2x7x_chip_info {
	int num_channels;
	struct iio_chan_spec		channel[4];
	const struct iio_info		*info;
};

struct tsl2x7x_chip {
	kernel_ulong_t id;
	struct mutex prox_mutex;
	struct mutex als_mutex;
	struct i2c_client *client;
	bool prox_enable;
	bool als_enable;
	u16 prox_data;
	struct workqueue_struct *tsl2x7x_wq;
	struct work_struct irq_work;
	struct work_struct calc_work;
	struct tsl2x7x_als_info als_cur_info;
	struct tsl2x7x_settings tsl2x7x_settings;
	struct tsl2X7X_platform_data *pdata;
	int als_time_scale;
	int als_saturation;
	int tsl2x7x_chip_status;
	u8 tsl2x7x_config[TSL2X7X_MAX_CONFIG_REG];
	const struct tsl2x7x_chip_info	*chip_info;
	const struct iio_info *info;
	s64 event_timestamp;
	/* This structure is intentionally large to accommodate
	 * updates via sysfs. */
	/* Sized to 9 = max 8 segments + 1 termination segment */
	struct tsl2x7x_lux tsl2x7x_device_lux[TSL2X7X_MAX_LUX_TABLE_SIZE];
};

/* Different devices require different coefficents */
static const struct tsl2x7x_lux tsl2x71_lux_table[] = {
	{ 14461,   611,   1211 },
	{ 18540,   352,    623 },
	{     0,     0,      0 },
};

static const struct tsl2x7x_lux tmd2x71_lux_table[] = {
	{ 11635,   115,    256 },
	{ 15536,    87,    179 },
	{     0,     0,      0 },
};

static const struct tsl2x7x_lux tsl2x72_lux_table[] = {
	{ 14013,   466,   917 },
	{ 18222,   310,   552 },
	{     0,     0,     0 },
};

static const struct tsl2x7x_lux tmd2x72_lux_table[] = {
	{ 13218,   130,   262 },
	{ 17592,   92,    169 },
	{     0,     0,     0 },
};

static const struct tsl2x7x_lux *tsl2x7x_default_lux_table_group[] = {
	[tsl2571] =	tsl2x71_lux_table,
	[tsl2671] =	tsl2x71_lux_table,
	[tmd2671] =	tmd2x71_lux_table,
	[tsl2771] =	tsl2x71_lux_table,
	[tmd2771] =	tmd2x71_lux_table,
	[tsl2572] =	tsl2x72_lux_table,
	[tsl2672] =	tsl2x72_lux_table,
	[tmd2672] =	tmd2x72_lux_table,
	[tsl2772] =	tsl2x72_lux_table,
	[tmd2772] =	tmd2x72_lux_table,
};

static const struct tsl2x7x_settings tsl2x7x_default_settings = {
	.als_time = 237, /* 50 ms */
	.als_gain = 0,
	.prx_time = 255, /* 2.72 ms */
	.prox_gain = 1,
	.wait_time = 255, /* 2.72 ms */
	.prox_config = 0,
	.als_gain_trim = 1000,
	.als_cal_target = 150,
	.als_thresh_low = 200,
	.als_thresh_high = 256,
	.persistence = 21,
	.interrupts_en = 0,
	.prox_thres_low  = 0,
	.prox_thres_high = 1023,
	.prox_max_samples_cal = 30,
	.prox_pulse_count = 8
};

static const s16 tsl2x7x_als_gainadj[] = {
	1,
	8,
	16,
	120
};

static const s16 tsl2x7x_prx_gainadj[] = {
	1,
	2,
	4,
	8
};

/* Channel variations */
enum {
	ALS,
	PRX,
	ALSPRX,
	PRX2,
	ALSPRX2,
};

static const u8 device_channel_config[] = {
	ALS,
	PRX,
	PRX,
	ALSPRX,
	ALSPRX,
	ALS,
	PRX2,
	PRX2,
	ALSPRX2,
	ALSPRX2
};


static struct input_dev *tsl2x7x_prox_dev;
static struct input_dev *tsl2x7x_als_dev;

static int prox_thres_low;
static int prox_thres_high = 1023;


/**
 * tsl2x7x_parse_buffer() - parse a decimal result from a buffer.
 * @*buf:                   pointer to char buffer to parse
 * @*result:                pointer to buffer to contain
 *                          resulting interger / decimal as ints.
 *
 */
	static int
tsl2x7x_parse_buffer(const char *buf, struct tsl2x7x_parse_result *result)
{
	int integer = 0, fract = 0, fract_mult = 100000;
	bool integer_part = true, negative = false;

	if (buf[0] == '-') {
		negative = true;
		buf++;
	}

	while (*buf) {
		if ('0' <= *buf && *buf <= '9') {
			if (integer_part)
				integer = integer*10 + *buf - '0';
			else {
				fract += fract_mult*(*buf - '0');
				if (fract_mult == 1)
					break;
				fract_mult /= 10;
			}
		} else if (*buf == '\n') {
			if (*(buf + 1) == '\0')
				break;
			else
				return -EINVAL;
		} else if (*buf == '.') {
			integer_part = false;
		} else {
			return -EINVAL;
		}
		buf++;
	}
	if (negative) {
		if (integer)
			integer = -integer;
		else
			fract = -fract;
	}

	result->integer = integer;
	result->fract = fract;

	return 0;
}

/**
 * tsl2x7x_i2c_read() - Read a byte from a register.
 * @client:	i2c client
 * @reg:	device register to read from
 * @*val:	pointer to location to store register contents.
 *
 */
	static int
tsl2x7x_i2c_read(struct i2c_client *client, u8 reg, u8 *val)
{
	int ret = 0;

	/* select register to write */
	ret = i2c_smbus_write_byte(client, (TSL2X7X_CMD_REG | reg));
	if (ret < 0) {
		dev_err(&client->dev, "%s: failed to write register %x\n"
				, __func__, reg);
		return ret;
	}

	/* read the data */
	ret = i2c_smbus_read_byte(client);
	if (ret >= 0)
		*val = (u8)ret;
	else
		dev_err(&client->dev, "%s: failed to read register %x\n"
				, __func__, reg);

	return ret;
}

/**
 * tsl2x7x_get_lux() - Reads and calculates current lux value.
 * @indio_dev:	pointer to IIO device
 *
 * The raw ch0 and ch1 values of the ambient light sensed in the last
 * integration cycle are read from the device.
 * Time scale factor array values are adjusted based on the integration time.
 * The raw values are multiplied by a scale factor, and device gain is obtained
 * using gain index. Limit checks are done next, then the ratio of a multiple
 * of ch1 value, to the ch0 value, is calculated. Array tsl2x7x_device_lux[]
 * is then scanned to find the first ratio value that is just above the ratio
 * we just calculated. The ch0 and ch1 multiplier constants in the array are
 * then used along with the time scale factor array values, to calculate the
 * lux.
 */
static int tsl2x7x_get_lux(struct iio_dev *indio_dev)
{
	u32 ch0, ch1; /* separated ch0/ch1 data from device */
	u32 lux; /* raw lux calculated from device data */
	u64 lux64;
	u32 ratio;
	u8 buf[4];
	struct tsl2x7x_lux *p;
	struct tsl2x7x_chip *chip = iio_priv(indio_dev);
	int i, ret;
	u32 ch0lux = 0;
	u32 ch1lux = 0;

	if (mutex_trylock(&chip->als_mutex) == 0)
		return chip->als_cur_info.lux; /* busy, so return LAST VALUE */

	if (chip->tsl2x7x_chip_status != TSL2X7X_CHIP_WORKING) {
		/* device is not enabled */
		dev_err(&chip->client->dev, "%s: device is not enabled\n",
				__func__);
		ret = -EBUSY ;
		goto out_unlock;
	}

	ret = tsl2x7x_i2c_read(chip->client,
			(TSL2X7X_CMD_REG | TSL2X7X_STATUS), &buf[0]);
	if (ret < 0) {
		dev_err(&chip->client->dev,
				"%s: Failed to read STATUS Reg\n", __func__);
		goto out_unlock;
	}
	/* is data new & valid */
	if (!(buf[0] & TSL2X7X_STA_ADC_VALID)) {
		dev_err(&chip->client->dev,
				"%s: data not valid yet\n", __func__);
		ret = chip->als_cur_info.lux; /* return LAST VALUE */
		goto out_unlock;
	}

	for (i = 0; i < 4; i++) {
		ret = tsl2x7x_i2c_read(chip->client,
				(TSL2X7X_CMD_REG | (TSL2X7X_ALS_CHAN0LO + i)),
				&buf[i]);
		if (ret < 0) {
			dev_err(&chip->client->dev,
				"%s: failed to read. err=%x\n", __func__, ret);
			goto out_unlock;
		}
	}

	/* clear any existing interrupt status */
	ret = i2c_smbus_write_byte(chip->client,
			(TSL2X7X_CMD_REG |
			 TSL2X7X_CMD_SPL_FN |
			 TSL2X7X_CMD_ALS_INT_CLR));
	if (ret < 0) {
		dev_err(&chip->client->dev,
				"%s: i2c_write_command failed - err = %d\n",
				__func__, ret);
		goto out_unlock; /* have no data, so return failure */
	}

	/* extract ALS/lux data */
	ch0 = le16_to_cpup((const __le16 *)&buf[0]);
	ch1 = le16_to_cpup((const __le16 *)&buf[2]);

	chip->als_cur_info.als_ch0 = ch0;
	chip->als_cur_info.als_ch1 = ch1;

	if ((ch0 >= chip->als_saturation) || (ch1 >= chip->als_saturation)) {
		lux = TSL2X7X_LUX_CALC_OVER_FLOW;
		goto return_max;
	}

	if (ch0 == 0) {
		/* have no data, so return LAST VALUE */
		ret = chip->als_cur_info.lux;
		goto out_unlock;
	}

	/* add attenuation compensation */
	ch0 = ch0*8;
	ch1 = ch1*3;

	/* calculate ratio */
	ratio = (ch1 << 15) / ch0;
	/* convert to unscaled lux using the pointer to the table */
	p = (struct tsl2x7x_lux *) chip->tsl2x7x_device_lux;
	while (p->ratio != 0 && p->ratio < ratio)
		p++;

	if (p->ratio == 0) {
		lux = 0;
	} else {
		ch0lux = DIV_ROUND_UP((ch0 * p->ch0),
			tsl2x7x_als_gainadj[chip->tsl2x7x_settings.als_gain]);
		ch1lux = DIV_ROUND_UP((ch1 * p->ch1),
			tsl2x7x_als_gainadj[chip->tsl2x7x_settings.als_gain]);
		lux = ch0lux - ch1lux;
	}

	/* note: lux is 31 bit max at this point */
	if (ch1lux > ch0lux) {
		dev_dbg(&chip->client->dev, "ch1lux > ch0lux-return last value\n");
		ret = chip->als_cur_info.lux;
		goto out_unlock;
	}

	/* adjust for active time scale */
	if (chip->als_time_scale == 0)
		lux = 0;
	else
		lux = (lux + (chip->als_time_scale >> 1)) /
			chip->als_time_scale;

	/* adjust for active gain scale
	 * The tsl2x7x_device_lux tables have a factor of 256 built-in.
	 * User-specified gain provides a multiplier.
	 * Apply user-specified gain before shifting right to retain precision.
	 * Use 64 bits to avoid overflow on multiplication.
	 * Then go back to 32 bits before division to avoid using div_u64().
	 */

	lux64 = lux;
	lux64 = lux64 * chip->tsl2x7x_settings.als_gain_trim;
	lux64 >>= 8;
	lux = lux64;
	lux = (lux + 500) / 1000;

	if (lux > TSL2X7X_LUX_CALC_OVER_FLOW) /* check for overflow */
		lux = TSL2X7X_LUX_CALC_OVER_FLOW;

	/* Update the structure with the latest lux. */
return_max:
	chip->als_cur_info.lux = lux;
	ret = lux;

out_unlock:
	mutex_unlock(&chip->als_mutex);

	return ret;
}

/**
 * tsl2x7x_get_prox() - Reads proximity data registers and updates
 *                      chip->prox_data.
 *
 * @indio_dev:	pointer to IIO device
 */
static int tsl2x7x_get_prox(struct iio_dev *indio_dev)
{
	int i;
	int ret;
	u8 status;
	u8 chdata[2];
	struct tsl2x7x_chip *chip = iio_priv(indio_dev);

	if (mutex_trylock(&chip->prox_mutex) == 0) {
		dev_err(&chip->client->dev,
				"%s: Can't get prox mutex\n", __func__);
		return -EBUSY;
	}
	ret = tsl2x7x_i2c_read(chip->client,
			(TSL2X7X_CMD_REG | TSL2X7X_STATUS), &status);
	if (ret < 0) {
		dev_err(&chip->client->dev,
				"%s: i2c err=%d\n", __func__, ret);
		goto prox_poll_err;
	}

	switch (chip->id) {
	case tsl2571:
	case tsl2671:
	case tmd2671:
	case tsl2771:
	case tmd2771:
		break;
	case tsl2572:
	case tsl2672:
	case tmd2672:
	case tsl2772:
	case tmd2772:
		if (!(status & TSL2X7X_STA_PRX_VALID))
			goto prox_poll_err;
		break;
	}

	for (i = 0; i < 2; i++) {
		ret = tsl2x7x_i2c_read(chip->client,
				(TSL2X7X_CMD_REG |
				 (TSL2X7X_PRX_LO + i)), &chdata[i]);
		if (ret < 0)
			goto prox_poll_err;
	}

	chip->prox_data =
		le16_to_cpup((const __le16 *)&chdata[0]);

prox_poll_err:

	mutex_unlock(&chip->prox_mutex);

	return chip->prox_data;
}

/**
 * tsl2x7x_defaults() - Populates the device nominal operating parameters
 *                      with those provided by a 'platform' data struct or
 *                      with prefined defaults.
 *
 * @chip:               pointer to device structure.
 */
static void tsl2x7x_defaults(struct tsl2x7x_chip *chip)
{
	/* If Operational settings defined elsewhere.. */
	if (chip->pdata && chip->pdata->platform_default_settings)
		memcpy(&(chip->tsl2x7x_settings),
				chip->pdata->platform_default_settings,
				sizeof(tsl2x7x_default_settings));
	else
		memcpy(&(chip->tsl2x7x_settings),
				&tsl2x7x_default_settings,
				sizeof(tsl2x7x_default_settings));
	/* Load up the proper lux table. */
	if (chip->pdata && chip->pdata->platform_lux_table[0].ratio != 0)
		memcpy(chip->tsl2x7x_device_lux,
				chip->pdata->platform_lux_table,
				sizeof(chip->pdata->platform_lux_table));
	else
		memcpy(chip->tsl2x7x_device_lux,
		(struct tsl2x7x_lux *)tsl2x7x_default_lux_table_group[chip->id],
				MAX_DEFAULT_TABLE_BYTES);
}

/**
 * tsl2x7x_als_calibrate() -	Obtain single reading and calculate
 *                              the als_gain_trim.
 *
 * @indio_dev:	pointer to IIO device
 */
static int tsl2x7x_als_calibrate(struct iio_dev *indio_dev)
{
	struct tsl2x7x_chip *chip = iio_priv(indio_dev);
	u8 reg_val;
	int gain_trim_val;
	int ret;
	int lux_val;

	ret = i2c_smbus_write_byte(chip->client,
			(TSL2X7X_CMD_REG | TSL2X7X_CNTRL));
	if (ret < 0) {
		dev_err(&chip->client->dev,
				"%s: failed to write CNTRL register, ret=%d\n",
				__func__, ret);
		return ret;
	}

	reg_val = i2c_smbus_read_byte(chip->client);
	if ((reg_val & (TSL2X7X_CNTL_ADC_ENBL | TSL2X7X_CNTL_PWR_ON))
			!= (TSL2X7X_CNTL_ADC_ENBL | TSL2X7X_CNTL_PWR_ON)) {
		dev_err(&chip->client->dev,
				"%s: failed: ADC not enabled\n", __func__);
		return -1;
	}

	ret = i2c_smbus_write_byte(chip->client,
			(TSL2X7X_CMD_REG | TSL2X7X_CNTRL));
	if (ret < 0) {
		dev_err(&chip->client->dev,
				"%s: failed to write ctrl reg: ret=%d\n",
				__func__, ret);
		return ret;
	}

	reg_val = i2c_smbus_read_byte(chip->client);
	if ((reg_val & TSL2X7X_STA_ADC_VALID) != TSL2X7X_STA_ADC_VALID) {
		dev_err(&chip->client->dev,
			"%s: failed: STATUS - ADC not valid.\n", __func__);
		return -ENODATA;
	}

	lux_val = tsl2x7x_get_lux(indio_dev);
	if (lux_val < 0) {
		dev_err(&chip->client->dev,
				"%s: failed to get lux\n", __func__);
		return lux_val;
	}

	gain_trim_val =  (((chip->tsl2x7x_settings.als_cal_target)
			* chip->tsl2x7x_settings.als_gain_trim) / lux_val);
	if ((gain_trim_val < 250) || (gain_trim_val > 4000))
		return -ERANGE;

	chip->tsl2x7x_settings.als_gain_trim = gain_trim_val;
	dev_info(&chip->client->dev,
			"%s als_calibrate completed\n", chip->client->name);

	return (int) gain_trim_val;
}

static int tsl2x7x_chip_on(struct iio_dev *indio_dev)
{
	int i;
	int ret = 0;
	u8 *dev_reg;
	u8 utmp;
	int als_count;
	int als_time;
	struct tsl2x7x_chip *chip = iio_priv(indio_dev);

	if (chip->pdata && chip->pdata->power_on)
		chip->pdata->power_on(indio_dev);
	/* Non calculated parameters */
	chip->tsl2x7x_config[TSL2X7X_PRX_TIME] =
		chip->tsl2x7x_settings.prx_time;
	chip->tsl2x7x_config[TSL2X7X_WAIT_TIME] =
		chip->tsl2x7x_settings.wait_time;
	chip->tsl2x7x_config[TSL2X7X_PRX_CONFIG] =
		chip->tsl2x7x_settings.prox_config;

	chip->tsl2x7x_config[TSL2X7X_ALS_MINTHRESHLO] =
		(chip->tsl2x7x_settings.als_thresh_low) & 0xFF;
	chip->tsl2x7x_config[TSL2X7X_ALS_MINTHRESHHI] =
		(chip->tsl2x7x_settings.als_thresh_low >> 8) & 0xFF;
	chip->tsl2x7x_config[TSL2X7X_ALS_MAXTHRESHLO] =
		(chip->tsl2x7x_settings.als_thresh_high) & 0xFF;
	chip->tsl2x7x_config[TSL2X7X_ALS_MAXTHRESHHI] =
		(chip->tsl2x7x_settings.als_thresh_high >> 8) & 0xFF;
	chip->tsl2x7x_config[TSL2X7X_PERSISTENCE] =
		chip->tsl2x7x_settings.persistence;

	chip->tsl2x7x_config[TSL2X7X_PRX_COUNT] =
		chip->tsl2x7x_settings.prox_pulse_count;
	chip->tsl2x7x_config[TSL2X7X_PRX_MINTHRESHLO] =
		chip->tsl2x7x_settings.prox_thres_low & 0xFF;
	chip->tsl2x7x_config[TSL2X7X_PRX_MINTHRESHHI] =
		(chip->tsl2x7x_settings.prox_thres_low >> 8) & 0xFF;
	chip->tsl2x7x_config[TSL2X7X_PRX_MAXTHRESHLO] =
		chip->tsl2x7x_settings.prox_thres_high & 0xFF;
	chip->tsl2x7x_config[TSL2X7X_PRX_MAXTHRESHHI] =
		(chip->tsl2x7x_settings.prox_thres_high >> 8) & 0xFF;

	/* and make sure we're not already on */
	if (chip->tsl2x7x_chip_status == TSL2X7X_CHIP_WORKING) {
		/* if forcing a register update - turn off, then on */
		dev_info(&chip->client->dev, "device is already enabled\n");
		return -EINVAL;
	}

	/* determine als integration register */
	als_count = (TSL2X7X_MAX_TIMER_CNT -
			chip->tsl2x7x_settings.als_time) + 1;
	if (als_count == 0)
		als_count = 1; /* ensure at least one cycle */

	/* convert back to time (encompasses overrides) */
	als_time = (als_count * TSL2X7X_MIN_ITIME_X100 + 50) / 100;
	chip->tsl2x7x_config[TSL2X7X_ALS_TIME] = 256 - als_count;

	/* Set the gain based on tsl2x7x_settings struct */
	chip->tsl2x7x_config[TSL2X7X_GAIN] =
		(chip->tsl2x7x_settings.als_gain |
		 (TSL2X7X_mA100 | TSL2X7X_DIODE1)
		 | ((chip->tsl2x7x_settings.prox_gain) << 2));

	/* set chip struct re scaling and saturation */
	chip->als_saturation = als_count * 922; /* 90% of full scale */
	chip->als_time_scale = (als_time + 25) / 50;

	/* Use the following shadow copy for our delay before enabling ADC.
	 * Write all the registers. */
	for (i = 0, dev_reg = chip->tsl2x7x_config;
			i < TSL2X7X_MAX_CONFIG_REG; i++) {
		ret = i2c_smbus_write_byte_data(chip->client,
				TSL2X7X_CMD_REG + i, *dev_reg++);
		if (ret < 0) {
			dev_err(&chip->client->dev,
			"%s: failed on write to reg %d.\n", __func__, i);
			return ret;
		}
	}

	/* TSL2X7X Specific power-on / adc enable sequence
	 * Power on the device 1st. */
	utmp = (chip->als_enable << 1 | chip->als_enable << 4) |
		(chip->prox_enable << 2 | chip->prox_enable << 5) |
		TSL2X7X_CNTL_PWR_ON;
	ret = i2c_smbus_write_byte_data(chip->client,
			TSL2X7X_CMD_REG | TSL2X7X_CNTRL, utmp);
	if (ret < 0) {
		dev_err(&chip->client->dev,
				"%s: failed on CNTRL reg.\n", __func__);
		return ret;
	}
	msleep(20);	/* Power-on settling time */

	chip->tsl2x7x_chip_status = TSL2X7X_CHIP_WORKING;

	return ret;
}

static int tsl2x7x_chip_off(struct iio_dev *indio_dev)
{
	int ret;
	struct tsl2x7x_chip *chip = iio_priv(indio_dev);

	/* turn device off */
	chip->tsl2x7x_chip_status = TSL2X7X_CHIP_SUSPENDED;

	ret = i2c_smbus_write_byte_data(chip->client,
			TSL2X7X_CMD_REG | TSL2X7X_CNTRL, 0x00);

	if (chip->pdata && chip->pdata->power_off)
		chip->pdata->power_off(chip->client);
	return ret;
}

/**
 * tsl2x7x_invoke_change
 * @indio_dev:	pointer to IIO device
 *
 * Obtain and lock both ALS and PROX resources,
 * determine and save device state (On/Off),
 * cycle device to implement updated parameter,
 * put device back into proper state, and unlock
 * resource.
 */
	static
int tsl2x7x_invoke_change(struct iio_dev *indio_dev)
{
	struct tsl2x7x_chip *chip = iio_priv(indio_dev);
	int device_status = chip->tsl2x7x_chip_status;

	mutex_lock(&chip->als_mutex);
	mutex_lock(&chip->prox_mutex);

	if (device_status == TSL2X7X_CHIP_WORKING)
		tsl2x7x_chip_off(indio_dev);

	tsl2x7x_chip_on(indio_dev);

	if (device_status != TSL2X7X_CHIP_WORKING)
		tsl2x7x_chip_off(indio_dev);

	mutex_unlock(&chip->prox_mutex);
	mutex_unlock(&chip->als_mutex);

	return 0;
}

	static
void tsl2x7x_prox_calculate(int *data, int length,
		struct tsl2x7x_prox_stat *statP)
{
	int i;
	int sample_sum;
	int tmp;

	if (length == 0)
		length = 1;

	sample_sum = 0;
	statP->min = INT_MAX;
	statP->max = INT_MIN;
	for (i = 0; i < length; i++) {
		sample_sum += data[i];
		statP->min = min(statP->min, data[i]);
		statP->max = max(statP->max, data[i]);
	}

	statP->mean = sample_sum / length;
	sample_sum = 0;
	for (i = 0; i < length; i++) {
		tmp = data[i] - statP->mean;
		sample_sum += tmp * tmp;
	}
	statP->stddev = int_sqrt((long)sample_sum)/length;
}

/**
 * tsl2x7x_prox_cal() - Calculates std. and sets thresholds.
 * @indio_dev:	pointer to IIO device
 *
 * Calculates a standard deviation based on the samples,
 * and sets the threshold accordingly.
 */
static void tsl2x7x_prox_cal(struct iio_dev *indio_dev)
{
	int prox_history[MAX_SAMPLES_CAL + 1];
	int i;
	struct tsl2x7x_prox_stat prox_stat_data[2];
	struct tsl2x7x_prox_stat *calp;
	struct tsl2x7x_chip *chip = iio_priv(indio_dev);
	u8 tmp_irq_settings;
	u8 current_state = chip->tsl2x7x_chip_status;

	if (chip->tsl2x7x_settings.prox_max_samples_cal > MAX_SAMPLES_CAL) {
		dev_err(&chip->client->dev,
				"%s: max prox samples cal is too big: %d\n",
				__func__,
				chip->tsl2x7x_settings.prox_max_samples_cal);
		chip->tsl2x7x_settings.prox_max_samples_cal = MAX_SAMPLES_CAL;
	}

	/* have to stop to change settings */
	tsl2x7x_chip_off(indio_dev);

	/* Enable proximity detection save just in case prox not wanted yet*/
	tmp_irq_settings = chip->tsl2x7x_settings.interrupts_en;
	chip->tsl2x7x_settings.interrupts_en |= TSL2X7X_CNTL_PROX_INT_ENBL;

	/*turn on device if not already on*/
	tsl2x7x_chip_on(indio_dev);

	/*gather the samples*/
	for (i = 0; i < chip->tsl2x7x_settings.prox_max_samples_cal; i++) {
		mdelay(15);
		tsl2x7x_get_prox(indio_dev);
		prox_history[i] = chip->prox_data;
		dev_info(&chip->client->dev, "2 i=%d prox data= %d\n",
				i, chip->prox_data);
	}

	tsl2x7x_chip_off(indio_dev);
	calp = &prox_stat_data[PROX_STAT_CAL];
	tsl2x7x_prox_calculate(prox_history,
			chip->tsl2x7x_settings.prox_max_samples_cal, calp);
	chip->tsl2x7x_settings.prox_thres_high = (calp->max << 1) - calp->mean;

	dev_info(&chip->client->dev, " cal min=%d mean=%d max=%d\n",
			calp->min, calp->mean, calp->max);
	dev_info(&chip->client->dev,
			"%s proximity threshold set to %d\n",
			chip->client->name,
			chip->tsl2x7x_settings.prox_thres_high);

	/* back to the way they were */
	chip->tsl2x7x_settings.interrupts_en = tmp_irq_settings;
	if (current_state == TSL2X7X_CHIP_WORKING)
		tsl2x7x_chip_on(indio_dev);
}

static ssize_t tsl2x7x_power_state_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct tsl2x7x_chip *chip = iio_priv(indio_dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", chip->tsl2x7x_chip_status);
}

static ssize_t tsl2x7x_power_state_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	bool value;

	if (strtobool(buf, &value))
		return -EINVAL;

	if (value)
		tsl2x7x_chip_on(indio_dev);
	else
		tsl2x7x_chip_off(indio_dev);

	return len;
}

static ssize_t tsl2x7x_gain_available_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct tsl2x7x_chip *chip = iio_priv(indio_dev);

	switch (chip->id) {
	case tsl2571:
	case tsl2671:
	case tmd2671:
	case tsl2771:
	case tmd2771:
		return snprintf(buf, PAGE_SIZE, "%s\n", "1 8 16 128");
		break;
	}

	return snprintf(buf, PAGE_SIZE, "%s\n", "1 8 16 120");
}

static ssize_t tsl2x7x_prox_gain_available_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", "1 2 4 8");
}

static ssize_t tsl2x7x_als_time_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct tsl2x7x_chip *chip = iio_priv(indio_dev);
	int y, z;

	y = (TSL2X7X_MAX_TIMER_CNT - (u8)chip->tsl2x7x_settings.als_time) + 1;
	z = y * TSL2X7X_MIN_ITIME_X100;
	z /= 100;
	y /= 1000;
	z %= 1000;

	return snprintf(buf, PAGE_SIZE, "%d.%03d\n", y, z);
}

static ssize_t tsl2x7x_als_time_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct tsl2x7x_chip *chip = iio_priv(indio_dev);
	struct tsl2x7x_parse_result result;
	int ret = len;

	result.integer = 0;
	result.fract = 0;

	tsl2x7x_parse_buffer(buf, &result);

	if (result.integer | (result.fract > 696000))
		ret = -EINVAL;

	if (ret == len) {
		result.fract *= 100;
		result.fract /= TSL2X7X_MIN_ITIME_X100;
		result.fract /= 1000;

		chip->tsl2x7x_settings.als_time =
			(TSL2X7X_MAX_TIMER_CNT - (u8)result.fract);

		dev_info(&chip->client->dev, "%s: als time = %d",
				__func__, chip->tsl2x7x_settings.als_time);

		tsl2x7x_invoke_change(indio_dev);
	}

	return ret;
}

static IIO_CONST_ATTR(in_illuminance0_integration_time_available,
		".00272 - .696");

static ssize_t tsl2x7x_als_cal_target_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct tsl2x7x_chip *chip = iio_priv(indio_dev);

	return snprintf(buf, PAGE_SIZE, "%d\n",
			chip->tsl2x7x_settings.als_cal_target);
}

static ssize_t tsl2x7x_als_cal_target_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct tsl2x7x_chip *chip = iio_priv(indio_dev);
	unsigned long value;

	if (kstrtoul(buf, 0, &value))
		return -EINVAL;

	if (value)
		chip->tsl2x7x_settings.als_cal_target = value;

	return len;
}

static ssize_t tsl2x7x_do_calibrate(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	bool value;

	if (strtobool(buf, &value))
		return -EINVAL;

	if (value)
		tsl2x7x_als_calibrate(indio_dev);

	return len;
}

static ssize_t tsl2x7x_luxtable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct tsl2x7x_chip *chip = iio_priv(indio_dev);
	int i = 0;
	int offset = 0;

	while (i < (TSL2X7X_MAX_LUX_TABLE_SIZE * 3)) {
		offset += snprintf(buf + offset, PAGE_SIZE, "%d,%d,%d,",
				chip->tsl2x7x_device_lux[i].ratio,
				chip->tsl2x7x_device_lux[i].ch0,
				chip->tsl2x7x_device_lux[i].ch1);
		if (chip->tsl2x7x_device_lux[i].ratio == 0) {
			/* We just printed the first "0" entry.
			 * Now get rid of the extra "," and break. */
			offset--;
			break;
		}
		i++;
	}

	offset += snprintf(buf + offset, PAGE_SIZE, "\n");
	return offset;
}

static ssize_t tsl2x7x_luxtable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct tsl2x7x_chip *chip = iio_priv(indio_dev);
	int value[ARRAY_SIZE(chip->tsl2x7x_device_lux)*3 + 1];
	int n;

	get_options(buf, ARRAY_SIZE(value), value);

	/* We now have an array of ints starting at value[1], and
	 * enumerated by value[0].
	 * We expect each group of three ints is one table entry,
	 * and the last table entry is all 0.
	 */
	n = value[0];
	if ((n % 3) || n < 6 ||
			n > ((ARRAY_SIZE(chip->tsl2x7x_device_lux) - 1) * 3)) {
		dev_info(dev, "LUX TABLE INPUT ERROR 1 Value[0]=%d\n", n);
		return -EINVAL;
	}

	if ((value[(n - 2)] | value[(n - 1)] | value[n]) != 0) {
		dev_info(dev, "LUX TABLE INPUT ERROR 2 Value[0]=%d\n", n);
		return -EINVAL;
	}

	if (chip->tsl2x7x_chip_status == TSL2X7X_CHIP_WORKING)
		tsl2x7x_chip_off(indio_dev);

	/* Zero out the table */
	memset(chip->tsl2x7x_device_lux, 0, sizeof(chip->tsl2x7x_device_lux));
	memcpy(chip->tsl2x7x_device_lux, &value[1], (value[0] * 4));

	return len;
}

static ssize_t tsl2x7x_do_prox_calibrate(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	bool value;

	if (strtobool(buf, &value))
		return -EINVAL;

	if (value)
		tsl2x7x_prox_cal(indio_dev);

	return len;
}

static ssize_t tsl2x7x_prox_raw_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct tsl2x7x_chip *chip = iio_priv(indio_dev);
	int val;

	/*
	 * 3.0.20 change: update tsl2x7x_prox_poll() to tsl2x7x_get_prox()
	 *	tsl2x7x_prox_poll(indio_dev);
	 */
	tsl2x7x_get_prox(indio_dev);
	/*
	 * 3.0.20 change: update
	 *	val = chip->prox_cur_info.prox_data;
	 */
	val = chip->prox_data;

	return snprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t tsl2x7x_proximity_calibscale_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct tsl2x7x_chip *chip = iio_priv(indio_dev);
	int val;

	val = tsl2x7x_als_gainadj[chip->tsl2x7x_settings.prox_gain];

	return snprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t tsl2x7x_proximity_calibscale_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct tsl2x7x_chip *chip = iio_priv(indio_dev);
	unsigned long value;

	if (kstrtoul(buf, 0, &value))
		return -EINVAL;

	switch (value) {
	case 1:
		chip->tsl2x7x_settings.prox_gain = 0;
		break;
	case 2:
		chip->tsl2x7x_settings.prox_gain = 1;
		break;
	case 4:
		chip->tsl2x7x_settings.prox_gain = 2;
		break;
	case 8:
		chip->tsl2x7x_settings.prox_gain = 3;
		break;
	default:
		return -EINVAL;
	}

	tsl2x7x_invoke_change(indio_dev);

	return len;
}

static ssize_t tsl2x7x_intensity_calibscale_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct tsl2x7x_chip *chip = iio_priv(indio_dev);
	int val;

	val = tsl2x7x_als_gainadj[chip->tsl2x7x_settings.als_gain];

	return snprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t tsl2x7x_intensity_calibscale_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct tsl2x7x_chip *chip = iio_priv(indio_dev);
	unsigned long value;

	if (kstrtoul(buf, 0, &value))
		return -EINVAL;

	switch (value) {
	case 1:
		chip->tsl2x7x_settings.als_gain = 0;
		break;
	case 8:
		chip->tsl2x7x_settings.als_gain = 1;
		break;
	case 16:
		chip->tsl2x7x_settings.als_gain = 2;
		break;
	case 120:
		if (chip->id > tsl2771)
			return -EINVAL;
		chip->tsl2x7x_settings.als_gain = 3;
		break;
	case 128:
		if (chip->id < tsl2572)
			return -EINVAL;
		chip->tsl2x7x_settings.als_gain = 3;
		break;
	default:
		return -EINVAL;
	}

	tsl2x7x_invoke_change(indio_dev);

	return len;
}

static ssize_t tsl2x7x_intensity_calibbias_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct tsl2x7x_chip *chip = iio_priv(indio_dev);
	int val;

	val = chip->tsl2x7x_settings.als_gain_trim;

	return snprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t tsl2x7x_intensity_calibbias_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct tsl2x7x_chip *chip = iio_priv(indio_dev);
	unsigned long value;

	if (kstrtoul(buf, 0, &value))
		return -EINVAL;

	chip->tsl2x7x_settings.als_gain_trim = value;

	return len;
}

/* 3.0.20 change: add attributes to substitute for .read_raw*/

static ssize_t tsl2x7x_in_intensity0_raw_value_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct tsl2x7x_chip *chip = iio_priv(indio_dev);
	int val;

	tsl2x7x_get_lux(indio_dev);
	val = chip->als_cur_info.als_ch0;

	return snprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t tsl2x7x_in_intensity1_raw_value_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct tsl2x7x_chip *chip = iio_priv(indio_dev);
	int val;

	tsl2x7x_get_lux(indio_dev);
	val = chip->als_cur_info.als_ch1;

	return snprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t tsl2x7x_in_illuminance0_value_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct tsl2x7x_chip *chip = iio_priv(indio_dev);
	int val;

	tsl2x7x_get_lux(indio_dev);
	val = chip->als_cur_info.lux;

	return snprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t tsl2x7x_illuminance0_thresh_falling_value_show(
		struct device *dev, struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct tsl2x7x_chip *chip = iio_priv(indio_dev);
	int val;

	val = chip->tsl2x7x_settings.als_thresh_low;

	return snprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t tsl2x7x_illuminance0_thresh_falling_value_store(
		struct device *dev, struct device_attribute *attr,
		const char *buf, size_t len)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct tsl2x7x_chip *chip = iio_priv(indio_dev);
	unsigned long value;

	if (kstrtoul(buf, 0, &value))
		return -EINVAL;

	chip->tsl2x7x_settings.als_thresh_low = value;

	tsl2x7x_invoke_change(indio_dev);

	return len;
}

static ssize_t tsl2x7x_illuminance0_thresh_rising_value_show(
		struct device *dev, struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct tsl2x7x_chip *chip = iio_priv(indio_dev);
	int val;

	val = chip->tsl2x7x_settings.als_thresh_high;

	return snprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t tsl2x7x_illuminance0_thresh_rising_value_store(
		struct device *dev, struct device_attribute *attr,
		const char *buf, size_t len)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct tsl2x7x_chip *chip = iio_priv(indio_dev);
	unsigned long value;

	if (kstrtoul(buf, 0, &value))
		return -EINVAL;

	chip->tsl2x7x_settings.als_thresh_high = value;

	tsl2x7x_invoke_change(indio_dev);

	return len;
}

static ssize_t tsl2x7x_proximity0_thresh_falling_value_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct tsl2x7x_chip *chip = iio_priv(indio_dev);
	int val;

	val = chip->tsl2x7x_settings.prox_thres_low;

	return snprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t tsl2x7x_proximity0_thresh_falling_value_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct tsl2x7x_chip *chip = iio_priv(indio_dev);
	unsigned long value;

	if (kstrtoul(buf, 0, &value))
		return -EINVAL;

	chip->tsl2x7x_settings.prox_thres_low = value;

	tsl2x7x_invoke_change(indio_dev);

	return len;
}

static ssize_t tsl2x7x_proximity0_thresh_rising_value_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct tsl2x7x_chip *chip = iio_priv(indio_dev);
	int val;

	val = chip->tsl2x7x_settings.prox_thres_high;

	return snprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t tsl2x7x_proximity0_thresh_rising_value_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct tsl2x7x_chip *chip = iio_priv(indio_dev);
	unsigned long value;

	if (kstrtoul(buf, 0, &value))
		return -EINVAL;

	chip->tsl2x7x_settings.prox_thres_high = value;

	tsl2x7x_invoke_change(indio_dev);

	return len;
}

/* persistence settings */
static ssize_t tsl2x7x_als_persistence_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct tsl2x7x_chip *chip = iio_priv(indio_dev);
	int y, z, filter_delay;
	int persistence_mapped;

	/* Determine integration time */
	y = (TSL2X7X_MAX_TIMER_CNT - (u8)chip->tsl2x7x_settings.als_time) + 1;
	z = y * TSL2X7X_MIN_ITIME_X100;
	z /= 100;

	switch (chip->tsl2x7x_settings.persistence & 0xF) {
	case 0:
		persistence_mapped = 0;
		break;
	case 1:
		persistence_mapped = 1;
		break;
	case 2:
		persistence_mapped = 2;
		break;
	case 3:
		persistence_mapped = 3;
		break;
	case 4:
		persistence_mapped = 5;
		break;
	case 5:
	case 6:
	case 7:
	case 8:
	case 9:
	case 0xA:
	case 0xB:
	case 0xC:
	case 0xD:
	case 0xE:
	case 0xF:
		persistence_mapped = ((chip->tsl2x7x_settings.persistence &
					0xF) - 3) * 5;
		break;
	default:
		persistence_mapped = 60;
	}

	filter_delay = z * persistence_mapped;
	y = (filter_delay / 1000);
	z = (filter_delay % 1000);

	return snprintf(buf, PAGE_SIZE, "%d.%03d\n", y, z);
}

static ssize_t tsl2x7x_als_persistence_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct tsl2x7x_chip *chip = iio_priv(indio_dev);
	struct tsl2x7x_parse_result result;
	int y, z, filter_delay;
	int filter_delay_mapped = 0;
	int ret = len;

	result.integer = 0;
	result.fract = 0;
	tsl2x7x_parse_buffer(buf, &result);

	result.fract /= 1000;
	y = (TSL2X7X_MAX_TIMER_CNT - (u8)chip->tsl2x7x_settings.als_time) + 1;
	z = y * TSL2X7X_MIN_ITIME_X100;
	z /= 100;

	filter_delay =
		DIV_ROUND_UP(((result.integer * 1000) + result.fract), z);

	if (filter_delay <= 4)
		filter_delay_mapped = filter_delay;
	else if (filter_delay <= 60)
		filter_delay_mapped = filter_delay / 5 + 3;
	else
		ret = -EINVAL;

	if (ret == len) {
		chip->tsl2x7x_settings.persistence &= 0xF0;
		chip->tsl2x7x_settings.persistence |=
			(filter_delay_mapped & 0x0F);

		dev_info(&chip->client->dev, "%s: als persistence = %d",
				__func__, filter_delay);

		tsl2x7x_invoke_change(indio_dev);
	}

	return ret;
}

static ssize_t tsl2x7x_prox_persistence_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct tsl2x7x_chip *chip = iio_priv(indio_dev);
	int y, z, filter_delay;

	/* Determine integration time */
	y = (TSL2X7X_MAX_TIMER_CNT - (u8)chip->tsl2x7x_settings.prx_time) + 1;
	z = y * TSL2X7X_MIN_ITIME_X100;
	filter_delay = z * ((chip->tsl2x7x_settings.persistence & 0xF0) >> 4);
	filter_delay /= 100;
	y = (filter_delay / 1000);
	z = (filter_delay % 1000);

	return snprintf(buf, PAGE_SIZE, "%d.%03d\n", y, z);
}

static ssize_t tsl2x7x_prox_persistence_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct tsl2x7x_chip *chip = iio_priv(indio_dev);
	struct tsl2x7x_parse_result result;
	int y, z, filter_delay;
	int ret = len;

	result.integer = 0;
	result.fract = 0;
	tsl2x7x_parse_buffer(buf, &result);

	result.fract /= 1000;
	y = (TSL2X7X_MAX_TIMER_CNT - (u8)chip->tsl2x7x_settings.prx_time) + 1;
	z = y * TSL2X7X_MIN_ITIME_X100;
	z /= 100;

	filter_delay =
		DIV_ROUND_UP(((result.integer * 1000) + result.fract), z);

	if (filter_delay > 0xF) {
		ret = -EINVAL;
	} else {
		chip->tsl2x7x_settings.persistence &= 0x0F;
		chip->tsl2x7x_settings.persistence |=
			((filter_delay << 4) & 0xF0);
	}

	dev_info(&chip->client->dev, "%s: prox persistence = %d",
			__func__, filter_delay);

	tsl2x7x_invoke_change(indio_dev);

	return ret;
}

/*
 *  3.0.20 change: end of attribute adds
 */

static ssize_t tsl2x7x_als_thresh_period_avail_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct tsl2x7x_chip *chip = iio_priv(indio_dev);

	int y, z, ymin, ymax, zmin, zmax, filter_delay_max, filter_delay_min;
	int persistence_max = 60;
	int persistence_min = 1;

	/* Determine integration time */
	y = (TSL2X7X_MAX_TIMER_CNT - (u8)chip->tsl2x7x_settings.als_time) + 1;

	z = y * TSL2X7X_MIN_ITIME_X100;
	z /= 100;

	filter_delay_max = z * persistence_max;
	filter_delay_min = z * persistence_min;

	ymin = (filter_delay_min / 1000);
	zmin = (filter_delay_min % 1000);

	ymax = (filter_delay_max / 1000);
	zmax = (filter_delay_max % 1000);

	return snprintf(buf, PAGE_SIZE,
			"available threshold period: %d.%03d - %d.%03d\n",
			ymin, zmin, ymax, zmax);
}

static ssize_t tsl2x7x_prox_thresh_period_avail_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct tsl2x7x_chip *chip = iio_priv(indio_dev);

	int y, z, filter_delay_max, filter_delay_min;
	int ymin, ymax, zmin, zmax;

	y = (TSL2X7X_MAX_TIMER_CNT - (u8)chip->tsl2x7x_settings.prx_time) + 1;
	z = y * TSL2X7X_MIN_ITIME_X100;

	filter_delay_max = z * 0xF;
	filter_delay_min = z;

	filter_delay_max /= 100;
	filter_delay_min /= 100;

	ymin = (filter_delay_min / 1000);
	zmin = (filter_delay_min % 1000);
	ymax = (filter_delay_max / 1000);
	zmax = (filter_delay_max % 1000);

	return snprintf(buf, PAGE_SIZE,
			"available threshold period: %d.%03d - %d.%03d\n",
			ymin, zmin, ymax, zmax);
}

/* 3.0.20 change: set/show enables*/

static ssize_t tsl2x7x_intensity_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct i2c_client *client = input_get_drvdata(input);
	struct iio_dev *indio_dev =
		(struct iio_dev *)i2c_get_clientdata(client);
	struct tsl2x7x_chip *chip = iio_priv(indio_dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", chip->als_enable);
}

static ssize_t tsl2x7x_intensity_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{

	struct input_dev *input = to_input_dev(dev);
	struct i2c_client *client = input_get_drvdata(input);
	struct iio_dev *indio_dev =
		(struct iio_dev *)i2c_get_clientdata(client);
	struct tsl2x7x_chip *chip = iio_priv(indio_dev);
	unsigned long value;

	if (kstrtoul(buf, 0, &value))
		return -EINVAL;

	if (value) {	/* enable illuminance interrupt */
		if (chip->als_enable)
			goto als_enable_exit;
		chip->als_enable = true;
	} else {
		if (!chip->als_enable)
			goto als_enable_exit;
		chip->als_enable = false;
	}

	tsl2x7x_invoke_change(indio_dev);

als_enable_exit:
	return len;
}


static ssize_t tsl2x7x_intensity_interval_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct i2c_client *client = input_get_drvdata(input);
	struct iio_dev *indio_dev =
		(struct iio_dev *)i2c_get_clientdata(client);
	struct tsl2x7x_chip *chip = iio_priv(indio_dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", chip->als_enable);
}

static ssize_t tsl2x7x_intensity_interval_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	return len;
}

static ssize_t tsl2x7x_proximity_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct i2c_client *client = input_get_drvdata(input);
	struct iio_dev *indio_dev =
		(struct iio_dev *)i2c_get_clientdata(client);
	struct tsl2x7x_chip *chip = iio_priv(indio_dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", chip->prox_enable);
}

static int32_t set_ps_threshold(struct iio_dev *indio_dev)
{
	int ret = 0;
	unsigned char utemp_data = 0;
	u16 ps_data;
	char data_count = 0;
	int ave_ps_int32 = 0;
	int prox_data_max = 0;
	struct tsl2x7x_chip *chip = iio_priv(indio_dev);

	ret = tsl2x7x_i2c_read(chip->client,
			(TSL2X7X_CMD_REG | TSL2X7X_CNTRL), &utemp_data);
	if (ret < 0)
		return ret;

	utemp_data |= (TSL2X7X_CNTL_PWR_ON | 1<<2);
	utemp_data &= ~(1<<5);

	ret = i2c_smbus_write_byte_data(chip->client,
			TSL2X7X_CMD_REG | TSL2X7X_CNTRL, utemp_data);
	if (ret < 0)
		return ret;

	while (data_count < 5) {
		msleep(50);
		tsl2x7x_get_prox(indio_dev);
		ps_data = chip->prox_data;
		if (prox_data_max < ps_data)
			prox_data_max = ps_data;
		pr_info("ps_data = %d\n", ps_data);
		ave_ps_int32 += ps_data;
		data_count++;
	}
	ave_ps_int32 /= 5;

	prox_thres_low =
		((prox_data_max - ave_ps_int32)*270 + 50)/100 + ave_ps_int32;
	if (prox_thres_low > 700)
		prox_thres_low = 700;
	if (prox_thres_low < 400)
		prox_thres_low = 400;
	prox_thres_high = prox_thres_low + 300;
	chip->tsl2x7x_settings.prox_thres_high = prox_thres_high;
	chip->tsl2x7x_settings.prox_thres_low = 0;
	input_report_abs(tsl2x7x_prox_dev, ABS_DISTANCE, 5);
	input_sync(tsl2x7x_prox_dev);
	return ret;
}


static ssize_t tsl2x7x_proximity_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	struct input_dev *input = to_input_dev(dev);
	struct i2c_client *client = input_get_drvdata(input);
	struct iio_dev *indio_dev =
		(struct iio_dev *)i2c_get_clientdata(client);
	struct tsl2x7x_chip *chip = iio_priv(indio_dev);

	unsigned long value;

	if (kstrtoul(buf, 0, &value))
		return -EINVAL;

	if (value) {	/* enable illuminance interrupt */
		disable_irq(client->irq);
		set_ps_threshold(indio_dev);
		enable_irq(client->irq);

		if (chip->prox_enable)
			goto prox_enable_exit;
		chip->prox_enable = true;
	} else {
		if (!chip->prox_enable)
			goto prox_enable_exit;
		chip->prox_enable = false;

		cancel_work_sync(&chip->irq_work);
	}
	tsl2x7x_invoke_change(indio_dev);
prox_enable_exit:
	return len;
}


static ssize_t tsl2x7x_proximity_interval_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct i2c_client *client = input_get_drvdata(input);
	struct iio_dev *indio_dev =
		(struct iio_dev *)i2c_get_clientdata(client);
	struct tsl2x7x_chip *chip = iio_priv(indio_dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", chip->prox_enable);
}

static ssize_t tsl2x7x_proximity_interval_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	return len;
}

static DEVICE_ATTR(in_proximity0_raw, S_IRUGO,
		tsl2x7x_prox_raw_show, NULL);

static DEVICE_ATTR(in_proximity0_calibscale, S_IRUGO | S_IWUSR,
		tsl2x7x_proximity_calibscale_show,
		tsl2x7x_proximity_calibscale_store);

static DEVICE_ATTR(in_intensity0_calibscale, S_IRUGO | S_IWUSR,
		tsl2x7x_intensity_calibscale_show,
		tsl2x7x_intensity_calibscale_store);

static DEVICE_ATTR(in_intensity0_calibbias, S_IRUGO | S_IWUSR,
		tsl2x7x_intensity_calibbias_show,
		tsl2x7x_intensity_calibbias_store);

static DEVICE_ATTR(in_intensity0_raw, S_IRUGO,
		tsl2x7x_in_intensity0_raw_value_show, NULL);

static DEVICE_ATTR(in_intensity1_raw, S_IRUGO,
		tsl2x7x_in_intensity1_raw_value_show, NULL);

static DEVICE_ATTR(in_illuminance0_input, S_IRUGO,
		tsl2x7x_in_illuminance0_value_show, NULL);

static DEVICE_ATTR(power_state, S_IRUGO | S_IWUSR,
		tsl2x7x_power_state_show, tsl2x7x_power_state_store);

static DEVICE_ATTR(in_proximity0_calibscale_available, S_IRUGO,
		tsl2x7x_prox_gain_available_show, NULL);

static DEVICE_ATTR(in_illuminance0_calibscale_available, S_IRUGO,
		tsl2x7x_gain_available_show, NULL);

static DEVICE_ATTR(in_illuminance0_integration_time, S_IRUGO | S_IWUSR,
		tsl2x7x_als_time_show, tsl2x7x_als_time_store);

static DEVICE_ATTR(in_illuminance0_target_input, S_IRUGO | S_IWUSR,
		tsl2x7x_als_cal_target_show, tsl2x7x_als_cal_target_store);

static DEVICE_ATTR(in_illuminance0_calibrate, S_IWUSR, NULL,
		tsl2x7x_do_calibrate);

static DEVICE_ATTR(in_proximity0_calibrate, S_IWUSR, NULL,
		tsl2x7x_do_prox_calibrate);

static DEVICE_ATTR(in_illuminance0_lux_table, S_IRUGO | S_IWUSR,
		tsl2x7x_luxtable_show, tsl2x7x_luxtable_store);

static IIO_DEVICE_ATTR_NAMED(in_intensity0_thresh_falling_value,
		in_intensity0_thresh_falling_value,
		S_IRUGO | S_IWUSR,
		tsl2x7x_illuminance0_thresh_falling_value_show,
		tsl2x7x_illuminance0_thresh_falling_value_store,
		0);

static IIO_DEVICE_ATTR_NAMED(in_intensity0_thresh_rising_value,
		in_intensity0_thresh_rising_value,
		S_IRUGO | S_IWUSR,
		tsl2x7x_illuminance0_thresh_rising_value_show,
		tsl2x7x_illuminance0_thresh_rising_value_store,
		0);

static IIO_DEVICE_ATTR_NAMED(in_proximity0_thresh_falling_value,
		in_proximity0_thresh_falling_value,
		S_IRUGO | S_IWUSR,
		tsl2x7x_proximity0_thresh_falling_value_show,
		tsl2x7x_proximity0_thresh_falling_value_store,
		0);

static IIO_DEVICE_ATTR_NAMED(in_proximity0_thresh_rising_value,
		in_proximity0_thresh_rising_value,
		S_IRUGO | S_IWUSR,
		tsl2x7x_proximity0_thresh_rising_value_show,
		tsl2x7x_proximity0_thresh_rising_value_store,
		0);

static IIO_DEVICE_ATTR_NAMED(in_intensity0_thresh_period_available,
		in_intensity0_thresh_period_available,
		S_IRUGO,
		tsl2x7x_als_thresh_period_avail_show,
		NULL,
		0);

static IIO_DEVICE_ATTR_NAMED(in_proximity0_thresh_period_available,
		in_proximity0_thresh_period_available,
		S_IRUGO,
		tsl2x7x_prox_thresh_period_avail_show,
		NULL,
		0);

static IIO_DEVICE_ATTR_NAMED(in_intensity0_thresh_period,
		in_intensity0_thresh_period,
		S_IRUGO | S_IWUSR,
		tsl2x7x_als_persistence_show,
		tsl2x7x_als_persistence_store,
		0);

static IIO_DEVICE_ATTR_NAMED(in_proximity0_thresh_period,
		in_proximity0_thresh_period,
		S_IRUGO | S_IWUSR,
		tsl2x7x_prox_persistence_show,
		tsl2x7x_prox_persistence_store,
		0);
/*
   static DEVICE_ATTR(proximity_enable, S_IRUGO | S_IWUSR,
   tsl2x7x_proximity_enable_show, tsl2x7x_proximity_enable_store);

   static DEVICE_ATTR(intensity_enable, S_IRUGO | S_IWUSR,
   tsl2x7x_intensity_enable_show, tsl2x7x_intensity_enable_store);
 */

static DEVICE_ATTR2(active, S_IRUGO | S_IWUSR,
		tsl2x7x_proximity_enable_show, tsl2x7x_proximity_enable_store);
static DEVICE_ATTR2(interval, S_IRUGO | S_IWUSR,
		tsl2x7x_proximity_interval_show,
		tsl2x7x_proximity_interval_store);
static DEVICE_ATTR(active, S_IRUGO | S_IWUSR,
		tsl2x7x_intensity_enable_show, tsl2x7x_intensity_enable_store);
static DEVICE_ATTR(interval, S_IRUGO | S_IWUSR,
		tsl2x7x_intensity_interval_show,
		tsl2x7x_intensity_interval_store);

/* Use the default register values to identify the Taos device */
static int tsl2x7x_device_id(unsigned char *id, int target)
{
	switch (target) {
	case tsl2571:
	case tsl2671:
	case tsl2771:
		return ((*id & 0xf0) == TRITON_ID);
		break;
	case tmd2671:
	case tmd2771:
		return ((*id & 0xf0) == HALIBUT_ID);
		break;
	case tsl2572:
	case tsl2672:
	case tmd2672:
	case tsl2772:
	case tmd2772:
		return ((*id & 0xf0) == SWORDFISH_ID);
		break;
	}

	return -EINVAL;
}

static void tsl2x7x_irq_work(struct work_struct *work)
{
	struct tsl2x7x_chip *chip =
		container_of(work, struct tsl2x7x_chip, irq_work);
	struct iio_dev *indio_dev = iio_priv_to_dev(chip);
/*	s64 timestamp = iio_get_time_ns();*/
	int ret;
	int i;
	u8 value;
	u8 direction = 5;

	value = i2c_smbus_read_byte_data(chip->client,
			TSL2X7X_CMD_REG | TSL2X7X_STATUS);

	/* What type of interrupt do we need to process */
	if (value & TSL2X7X_STA_PRX_INTR) {
		ret = tsl2x7x_get_prox(indio_dev); /* freshen data for ABI */
		if (ret < 0)
			chip->prox_data = 0;
		pr_info("Proximity interrupt:%d\n", chip->prox_data);
		if (chip->prox_data >
				chip->tsl2x7x_settings.prox_thres_high) {
			direction = 0;
			chip->tsl2x7x_settings.prox_thres_high = 1023;
			chip->tsl2x7x_settings.prox_thres_low = prox_thres_low;
		} else if (chip->prox_data <
				chip->tsl2x7x_settings.prox_thres_low) {
			direction = 5;
			chip->tsl2x7x_settings.prox_thres_high =
				prox_thres_high;
			chip->tsl2x7x_settings.prox_thres_low = 0;
		}

		PL_DEGUG("direction = %d\n", direction);

		chip->tsl2x7x_config[TSL2X7X_PRX_MINTHRESHLO] =
			chip->tsl2x7x_settings.prox_thres_low & 0xFF;
		chip->tsl2x7x_config[TSL2X7X_PRX_MINTHRESHHI] =
			(chip->tsl2x7x_settings.prox_thres_low >> 8) & 0xFF;
		chip->tsl2x7x_config[TSL2X7X_PRX_MAXTHRESHLO] =
			chip->tsl2x7x_settings.prox_thres_high & 0xFF;
		chip->tsl2x7x_config[TSL2X7X_PRX_MAXTHRESHHI] =
			(chip->tsl2x7x_settings.prox_thres_high >> 8) & 0xFF;
		for (i = TSL2X7X_PRX_MINTHRESHLO;
				i < TSL2X7X_PRX_MAXTHRESHHI + 1; i++) {
			ret = i2c_smbus_write_byte_data(chip->client,
					i | TSL2X7X_CMD_REG,
					chip->tsl2x7x_config[i]);
			if (ret < 0) {
				dev_err(&chip->client->dev,
					"%s: failed on write to reg %d.\n",
					__func__, i);
				return;
			}
		}
		input_report_abs(tsl2x7x_prox_dev, ABS_DISTANCE, direction);
		input_sync(tsl2x7x_prox_dev);
	}

	if (value & TSL2X7X_STA_ALS_INTR) {
		tsl2x7x_get_lux(indio_dev); /* freshen data for ABI */
		pr_info("Intensity interrupt:%d\n", chip->als_cur_info.lux);
		/*
		   iio_push_event(indio_dev,
		   IIO_UNMOD_EVENT_CODE(IIO_LIGHT,
		   0,
		   IIO_EV_TYPE_THRESH,
		   IIO_EV_DIR_EITHER),
		   timestamp);
		 */
		chip->tsl2x7x_settings.als_thresh_low =
			chip->als_cur_info.als_ch0 * 3 / 4;
		chip->tsl2x7x_settings.als_thresh_high =
			chip->als_cur_info.als_ch0 * 5 / 4;

		chip->tsl2x7x_config[TSL2X7X_ALS_MINTHRESHLO] =
			chip->tsl2x7x_settings.als_thresh_low & 0xFF;
		chip->tsl2x7x_config[TSL2X7X_ALS_MINTHRESHHI] =
			(chip->tsl2x7x_settings.als_thresh_low >> 8) & 0xFF;
		chip->tsl2x7x_config[TSL2X7X_ALS_MAXTHRESHLO] =
			chip->tsl2x7x_settings.als_thresh_high & 0xFF;
		chip->tsl2x7x_config[TSL2X7X_ALS_MAXTHRESHHI] =
			(chip->tsl2x7x_settings.als_thresh_high >> 8) & 0xFF;
		for (i = TSL2X7X_ALS_MINTHRESHLO;
				i < TSL2X7X_ALS_MAXTHRESHHI + 1; i++) {
			ret = i2c_smbus_write_byte_data(chip->client,
					i | TSL2X7X_CMD_REG,
					chip->tsl2x7x_config[i]);
			if (ret < 0) {
				dev_err(&chip->client->dev,
					"%s: failed on write to reg %d.\n",
					__func__, i);
				return;
			}
		}
		/* Input report */
		input_report_abs(tsl2x7x_als_dev, ABS_PRESSURE,
				chip->als_cur_info.lux);
		input_sync(tsl2x7x_als_dev);
	}

	/* Clear interrupt now that we have handled it. */
	ret = i2c_smbus_write_byte(chip->client,
			TSL2X7X_CMD_REG | TSL2X7X_CMD_SPL_FN |
			TSL2X7X_CMD_PROXALS_INT_CLR);
	if (ret < 0)
		dev_err(&chip->client->dev,
			"%s: Failed to clear irq from event handler. err = %d\n",
			__func__, ret);

	enable_irq(chip->client->irq);
}

static irqreturn_t tsl2x7x_event_handler(int irq, void *private)
{
	struct iio_dev *indio_dev = private;
	struct tsl2x7x_chip *chip = iio_priv(indio_dev);

	disable_irq_nosync(irq);
	queue_work(chip->tsl2x7x_wq, &chip->irq_work);
	return IRQ_HANDLED;
}

static struct attribute *tsl2x7x_ALS_device_attrs[] = {
	&dev_attr_power_state.attr,
	&dev_attr_in_illuminance0_calibscale_available.attr,
	&dev_attr_in_illuminance0_integration_time.attr,
	&iio_const_attr_in_illuminance0_integration_time_available\
		.dev_attr.attr,
	&dev_attr_in_illuminance0_target_input.attr,
	&dev_attr_in_illuminance0_calibrate.attr,
	&dev_attr_in_illuminance0_lux_table.attr,
	/* 3.0.20 change: added attributes */
	&dev_attr_in_intensity0_raw.attr,
	&dev_attr_in_intensity1_raw.attr,
	&dev_attr_in_illuminance0_input.attr,
	&dev_attr_in_intensity0_calibscale.attr,
	&dev_attr_in_intensity0_calibbias.attr,
	NULL
};

static struct attribute *tsl2x7x_PRX_device_attrs[] = {
	&dev_attr_power_state.attr,
	&dev_attr_in_proximity0_calibrate.attr,
	/* 3.0.20 change: added attributes */
	&dev_attr_in_proximity0_raw.attr,
	&dev_attr_in_proximity0_calibscale.attr,
	NULL
};

static struct attribute *tsl2x7x_ALSPRX_device_attrs[] = {
	&dev_attr_power_state.attr,
	&dev_attr_in_illuminance0_calibscale_available.attr,
	&dev_attr_in_illuminance0_integration_time.attr,
	&iio_const_attr_in_illuminance0_integration_time_available\
		.dev_attr.attr,
	&dev_attr_in_illuminance0_target_input.attr,
	&dev_attr_in_illuminance0_calibrate.attr,
	&dev_attr_in_illuminance0_lux_table.attr,
	&dev_attr_in_proximity0_calibrate.attr,
	/* 3.0.20 change: adding attributes */
	&dev_attr_in_intensity0_raw.attr,
	&dev_attr_in_intensity1_raw.attr,
	&dev_attr_in_illuminance0_input.attr,
	&dev_attr_in_proximity0_raw.attr,
	&dev_attr_in_proximity0_calibscale.attr,
	&dev_attr_in_intensity0_calibscale.attr,
	&dev_attr_in_intensity0_calibbias.attr,
	NULL
};

static struct attribute *tsl2x7x_PRX2_device_attrs[] = {
	&dev_attr_power_state.attr,
	&dev_attr_in_proximity0_calibrate.attr,
	&dev_attr_in_proximity0_calibscale_available.attr,
	/* 3.0.20 change: added attributes */
	&dev_attr_in_proximity0_raw.attr,
	&dev_attr_in_proximity0_calibscale.attr,
	NULL
};

static struct attribute *tsl2x7x_ALSPRX2_device_attrs[] = {
	&dev_attr_power_state.attr,
	&dev_attr_in_illuminance0_calibscale_available.attr,
	&dev_attr_in_illuminance0_integration_time.attr,
	&iio_const_attr_in_illuminance0_integration_time_available\
		.dev_attr.attr,
	&dev_attr_in_illuminance0_target_input.attr,
	&dev_attr_in_illuminance0_calibrate.attr,
	&dev_attr_in_illuminance0_lux_table.attr,
	&dev_attr_in_proximity0_calibrate.attr,
	&dev_attr_in_proximity0_calibscale_available.attr,
	/* 3.0.20 change: added attributes */
	&dev_attr_in_intensity0_raw.attr,
	&dev_attr_in_intensity1_raw.attr,
	&dev_attr_in_illuminance0_input.attr,
	&dev_attr_in_proximity0_raw.attr,
	&dev_attr_in_proximity0_calibscale.attr,
	&dev_attr_in_intensity0_calibscale.attr,
	&dev_attr_in_intensity0_calibbias.attr,
	NULL
};

static struct attribute *tsl2x7x_ALS_event_attrs[] = {
	&iio_dev_attr_in_intensity0_thresh_period.dev_attr.attr,
	/* 3.0.20 change: adds */
	&iio_dev_attr_in_intensity0_thresh_falling_value.dev_attr.attr,
	&iio_dev_attr_in_intensity0_thresh_rising_value.dev_attr.attr,
	/* 3.0.20 change: additions for original driver */
	&iio_dev_attr_in_intensity0_thresh_period_available.dev_attr.attr,
	NULL,
};
static struct attribute *tsl2x7x_PRX_event_attrs[] = {
	&iio_dev_attr_in_proximity0_thresh_period.dev_attr.attr,
	/* 3.0.20 change: adds */
	&iio_dev_attr_in_proximity0_thresh_falling_value.dev_attr.attr,
	&iio_dev_attr_in_proximity0_thresh_rising_value.dev_attr.attr,
	/* 3.0.20 change: additions for original driver */
	&iio_dev_attr_in_proximity0_thresh_period_available.dev_attr.attr,
	NULL,
};

static struct attribute *tsl2x7x_ALSPRX_event_attrs[] = {
	&iio_dev_attr_in_intensity0_thresh_period.dev_attr.attr,
	&iio_dev_attr_in_proximity0_thresh_period.dev_attr.attr,
	/* 3.0.20 change: adds */
	&iio_dev_attr_in_intensity0_thresh_falling_value.dev_attr.attr,
	&iio_dev_attr_in_intensity0_thresh_rising_value.dev_attr.attr,
	&iio_dev_attr_in_proximity0_thresh_falling_value.dev_attr.attr,
	&iio_dev_attr_in_proximity0_thresh_rising_value.dev_attr.attr,
	/* 3.0.20 change: additions for original driver */
	&iio_dev_attr_in_intensity0_thresh_period_available.dev_attr.attr,
	&iio_dev_attr_in_proximity0_thresh_period_available.dev_attr.attr,
	NULL,
};

static struct attribute *tsl2x7x_PRX2_event_attrs[] = {
	/* 3.0.20 change: adds */
	&iio_dev_attr_in_proximity0_thresh_period.dev_attr.attr,
	&iio_dev_attr_in_proximity0_thresh_falling_value.dev_attr.attr,
	&iio_dev_attr_in_proximity0_thresh_rising_value.dev_attr.attr,
	NULL,
};

static struct attribute *tsl2x7x_ALSPRX2_event_attrs[] = {
	/* 3.0.20 change: adds */
	&iio_dev_attr_in_intensity0_thresh_period.dev_attr.attr,
	&iio_dev_attr_in_proximity0_thresh_period.dev_attr.attr,
	&iio_dev_attr_in_intensity0_thresh_falling_value.dev_attr.attr,
	&iio_dev_attr_in_intensity0_thresh_rising_value.dev_attr.attr,
	&iio_dev_attr_in_proximity0_thresh_falling_value.dev_attr.attr,
	&iio_dev_attr_in_proximity0_thresh_rising_value.dev_attr.attr,
	NULL,
};

static const struct attribute_group tsl2x7x_device_attr_group_tbl[] = {
	[ALS] = {
		.attrs = tsl2x7x_ALS_device_attrs,
	},
	[PRX] = {
		.attrs = tsl2x7x_PRX_device_attrs,
	},
	[ALSPRX] = {
		.attrs = tsl2x7x_ALSPRX_device_attrs,
	},
	[PRX2] = {
		.attrs = tsl2x7x_PRX2_device_attrs,
	},
	[ALSPRX2] = {
		.attrs = tsl2x7x_ALSPRX2_device_attrs,
	},
};

static struct attribute_group tsl2x7x_event_attr_group_tbl[] = {
	[ALS] = {
		.attrs = tsl2x7x_ALS_event_attrs,
	},
	[PRX] = {
		.attrs = tsl2x7x_PRX_event_attrs,
	},
	[ALSPRX] = {
		.attrs = tsl2x7x_ALSPRX_event_attrs,
	},
	/* 3.0.20 change: add PRX2 devices */
	[PRX2] = {
		.attrs = tsl2x7x_PRX2_event_attrs,
	},
	/* 3.0.20 change: add ALSPRX2 devices */
	[ALSPRX2] = {
		.attrs = tsl2x7x_ALSPRX2_event_attrs,
	},
};
/*
   static struct attribute *tsl2x7x_prox_device_attrs[] = {
   &dev_attr_proximity_enable.attr,
   NULL
   };

   static struct attribute *tsl2x7x_als_device_attrs[] = {
   &dev_attr_intensity_enable.attr,
   NULL
   };
 */

static struct attribute *tsl2x7x_prox_device_attrs[] = {
	&dev_attr2_active.attr,
	&dev_attr2_interval.attr,
	NULL
};

static struct attribute *tsl2x7x_als_device_attrs[] = {
	&dev_attr_active.attr,
	&dev_attr_interval.attr,
	NULL
};


static struct attribute_group tsl2x7x_prox_attr_group = {
	.attrs = tsl2x7x_prox_device_attrs,
};

static struct attribute_group tsl2x7x_als_attr_group = {
	.attrs = tsl2x7x_als_device_attrs,
};

static const struct iio_info tsl2x7x_device_info[] = {
	[ALS] = {
		.attrs = &tsl2x7x_device_attr_group_tbl[ALS],
		.event_attrs = &tsl2x7x_event_attr_group_tbl[ALS],
		.driver_module = THIS_MODULE,
	},
	[PRX] = {
		.attrs = &tsl2x7x_device_attr_group_tbl[PRX],
		.event_attrs = &tsl2x7x_event_attr_group_tbl[PRX],
		.driver_module = THIS_MODULE,
	},
	[ALSPRX] = {
		.attrs = &tsl2x7x_device_attr_group_tbl[ALSPRX],
		.event_attrs = &tsl2x7x_event_attr_group_tbl[ALSPRX],
		.driver_module = THIS_MODULE,
	},
	[PRX2] = {
		.attrs = &tsl2x7x_device_attr_group_tbl[PRX2],
		.event_attrs = &tsl2x7x_event_attr_group_tbl[PRX],
		.driver_module = THIS_MODULE,
	},
	[ALSPRX2] = {
		.attrs = &tsl2x7x_device_attr_group_tbl[ALSPRX2],
		.event_attrs = &tsl2x7x_event_attr_group_tbl[ALSPRX],
		.driver_module = THIS_MODULE,
	},
};

static const struct tsl2x7x_chip_info tsl2x7x_chip_info_tbl[] = {
	[ALS] = {
		.num_channels = 0,
		.info = &tsl2x7x_device_info[ALS],
	},
	[PRX] = {
		.num_channels = 0,
		.info = &tsl2x7x_device_info[PRX],
	},
	[ALSPRX] = {
		.num_channels = 0,
		.info = &tsl2x7x_device_info[ALSPRX],
	},
	[PRX2] = {
		.num_channels = 0,
		.info = &tsl2x7x_device_info[PRX2],
	},
	[ALSPRX2] = {
		.num_channels = 0,
		.info = &tsl2x7x_device_info[ALSPRX2],
	},
};

/**
 * tsl2772_buffer_preenable() setup the parameters of the buffer before enabling
 **/
static int tsl2772_buffer_preenable(struct iio_dev *indio_dev)
{

	struct iio_buffer *buffer = indio_dev->buffer;

	size_t d_size;

	d_size = buffer->bytes_per_datum * 1;

	d_size += sizeof(s64);

	if (d_size % sizeof(s64))
		d_size += sizeof(s64) - (d_size % sizeof(s64));

	if (buffer->access->set_bytes_per_datum)
		buffer->access->set_bytes_per_datum(buffer, d_size);

	return 0;
}

/*
 *
 * 3.1.10 changing buffer --> buffer for 3.0.20 compatibility
 *
 */

static const struct iio_buffer_setup_ops tsl2772_buffer_setup_ops = {
	.preenable = &tsl2772_buffer_preenable,
	/*
	 * 3.0.20 change: - this is a change relative to 3.2.5
	 *
	 *	.postenable = &iio_triggered_buffer_postenable,
	 *	.predisable = &iio_triggered_buffer_predisable,
	 *
	 */

	.postenable = &iio_triggered_buffer_postenable,
	.predisable = &iio_triggered_buffer_predisable,
};


int tsl2x7x_register_buffer_funcs_and_init(struct iio_dev *indio_dev)
{
	int ret;

	pr_info("iio_sw_rb_allocate\n");

	indio_dev->buffer = iio_sw_rb_allocate(indio_dev);
	if (!indio_dev->buffer) {
		ret = -ENOMEM;
		goto error_ret;
	}
	/* Effectively select the ring buffer implementation */
	/* indio_dev->buffer->access = &ring_sw_access_funcs;*/

	/* Ring buffer functions - here trigger setup related */
	/*indio_dev->buffer->setup_ops = &tsl2772_buffer_setup_ops;*/
	indio_dev->buffer->scan_timestamp = true;

	/* Flag that polled ring buffering is possible */
	indio_dev->modes |= INDIO_BUFFER_TRIGGERED;

	pr_info("register_ring_funcs_and_init complete\n");

	return 0;

error_ret:
	return ret;
}

#ifdef CONFIG_OF
static int tsl2x7x_probe_dt(struct i2c_client *client)
{
	struct tsl2X7X_platform_data *platform_data;
	struct device_node *np = client->dev.of_node;

	platform_data = kzalloc(sizeof(*platform_data), GFP_KERNEL);
	if (platform_data == NULL) {
		dev_err(&client->dev, "Alloc GFP_KERNEL memory failed.");
		return -ENOMEM;
	}
	client->dev.platform_data = platform_data;

	platform_data->irq =
		of_get_named_gpio(np, "irq-gpios", 0);
	if (platform_data->irq < 0) {
		dev_err(&client->dev, "of_get_named_gpio irq faild\n");
		return -EINVAL;
	}

	return 0;
}
static struct of_device_id tsl2x7x_dt_ids[] = {
	{ .compatible = "taos,tmd27713", },
	{}
};

#endif

static int tsl2x7x_probe(struct i2c_client *clientp,
		const struct i2c_device_id *id)
{
	int ret;
	unsigned char device_id;
	struct iio_dev *indio_dev;
	struct tsl2x7x_chip *chip;
	struct tsl2X7X_platform_data *plat_data;

	indio_dev = iio_allocate_device(sizeof(*chip));
	if (!indio_dev)
		return -ENOMEM;

#ifdef CONFIG_OF
	ret = tsl2x7x_probe_dt(clientp);
	if (ret == -ENOMEM) {
		dev_err(&clientp->dev,
		"%s: Failed to alloc mem for tsl2x7x_platform_data\n",
		__func__);
		return ret;
	} else if (ret == -EINVAL) {
		kfree(clientp->dev.platform_data);
		dev_err(&clientp->dev,
			"%s: Probe device tree data failed\n", __func__);
		return ret;
	}

	plat_data = clientp->dev.platform_data;
#else
	plat_data = (struct tsl2X7X_platform_data *)clientp->dev.platform_data;
	if (!plat_data) {
		PL_DEGUG("can't get platform data\n");
		return -EINVAL;
	}
#endif
	tsl2x7x_prox_dev = input_allocate_device();
	if (!tsl2x7x_prox_dev) {
		dev_err(&clientp->dev,
				"__init: Not enough memory for input_dev\n");
		ret = -ENOMEM;
		goto prox_input_allocate_fail;
	}

	/*Since we now have the struct, populate.*/
	tsl2x7x_prox_dev->name = "TSL_proximity_sensor";
	tsl2x7x_prox_dev->id.bustype = BUS_I2C;
	tsl2x7x_prox_dev->id.vendor = 0x0089;
	tsl2x7x_prox_dev->id.product = 0x2771;
	tsl2x7x_prox_dev->id.version = 3;
	tsl2x7x_prox_dev->phys = "tsl2x7x";

	/* Since there are no defines for LIGHT
	 * or PROX in this version use something
	 * logical.
	 */
	set_bit(EV_ABS, tsl2x7x_prox_dev->evbit);
	set_bit(ABS_DISTANCE, tsl2x7x_prox_dev->absbit); /* Proximity */
	input_set_abs_params(tsl2x7x_prox_dev, ABS_DISTANCE, 0, 10, 0, 0);
	input_set_drvdata(tsl2x7x_prox_dev, clientp);
	ret = input_register_device(tsl2x7x_prox_dev);
	if (ret) {
		dev_err(&clientp->dev,	"tsl2771 Failed to register input_dev\n");
		goto prox_input_register_fail;
	}
	ret = sysfs_create_group(&tsl2x7x_prox_dev->dev.kobj,
			&tsl2x7x_prox_attr_group);
	if (ret) {
		dev_err(&clientp->dev,	"tsl2771 Failed to register input_dev\n");
		goto prox_sysfs_create_fail;
	}

	tsl2x7x_als_dev = input_allocate_device();
	if (!tsl2x7x_als_dev) {
		dev_err(&clientp->dev,
				"__init: Not enough memory for input_dev\n");
		ret = -ENOMEM;
		goto als_input_allocate_fail;
	}

	/*Since we now have the struct, populate.*/
	tsl2x7x_als_dev->name = "TSL_light_sensor";
	tsl2x7x_als_dev->id.bustype = BUS_I2C;
	tsl2x7x_als_dev->id.vendor = 0x0089;
	tsl2x7x_als_dev->id.product = 0x2771;
	tsl2x7x_als_dev->id.version = 3;
	tsl2x7x_als_dev->phys = "tsl2x7x";

	/* Since there are no defines for LIGHT
	 * or PROX in this version use something
	 * logical.
	 */
	set_bit(EV_ABS, tsl2x7x_als_dev->evbit);
	set_bit(ABS_PRESSURE, tsl2x7x_als_dev->absbit); /* Proximity */
	input_set_abs_params(tsl2x7x_als_dev, ABS_PRESSURE, 0, 100000, 0, 0);
	input_set_drvdata(tsl2x7x_als_dev, clientp);

	ret = input_register_device(tsl2x7x_als_dev);
	if (ret) {
		dev_err(&clientp->dev,	"tsl2771 Failed to register input_dev\n");
		goto als_input_register_fail;
	}
	ret = sysfs_create_group(&tsl2x7x_als_dev->dev.kobj,
			&tsl2x7x_als_attr_group);
	if (ret) {
		dev_err(&clientp->dev,	"tsl2771 Failed to register input_dev\n");
		goto als_sysfs_create_fail;
	}

	chip = iio_priv(indio_dev);
	chip->client = clientp;
	i2c_set_clientdata(clientp, indio_dev);

	ret = tsl2x7x_i2c_read(chip->client,
			TSL2X7X_CHIPID, &device_id);
	if (ret < 0)
		goto tsl2x7x_check_err;

	if ((!tsl2x7x_device_id(&device_id, id->driver_data)) ||
			(tsl2x7x_device_id(&device_id, id->driver_data) ==
			 -EINVAL)) {
		dev_info(&chip->client->dev,
				"%s: i2c device found does not match expected id\n",
				__func__);
		goto tsl2x7x_check_err;
	}

	ret = i2c_smbus_write_byte(clientp, (TSL2X7X_CMD_REG | TSL2X7X_CNTRL));
	if (ret < 0) {
		dev_err(&clientp->dev, "%s: write to cmd reg failed. err = %d\n",
				__func__, ret);
		goto tsl2x7x_check_err;
	}

	/* ALS and PROX functions can be invoked via user space poll
	 * or H/W interrupt. If busy return last sample. */
	mutex_init(&chip->als_mutex);
	mutex_init(&chip->prox_mutex);

	chip->tsl2x7x_chip_status = TSL2X7X_CHIP_UNKNOWN;
	chip->pdata = clientp->dev.platform_data;
	chip->id = id->driver_data;
	chip->chip_info =
		&tsl2x7x_chip_info_tbl[device_channel_config[id->driver_data]];

	indio_dev->info = chip->chip_info->info;
	indio_dev->dev.parent = &clientp->dev;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->name = chip->client->name;
	indio_dev->channels = chip->chip_info->channel;
	indio_dev->num_channels = chip->chip_info->num_channels;

	chip->tsl2x7x_wq = create_singlethread_workqueue("tsl2x7x_wq");
	if (!(chip->tsl2x7x_wq)) {
		dev_err(&clientp->dev, "[tsl2x7x] %s: create workqueue failed\n",
				__func__);
		ret = -ENOMEM;
		goto tsl2x7x_check_err;
	}
	INIT_WORK(&chip->irq_work, tsl2x7x_irq_work);
	gpio_request(plat_data->irq, "tsl2x7x_irq");
	gpio_direction_input(plat_data->irq);
	if (clientp->irq) {
		ret = request_threaded_irq(clientp->irq,
				NULL,
				&tsl2x7x_event_handler,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				"TSL2X7X_event",
				indio_dev);
		if (ret) {
			dev_err(&clientp->dev,
					"%s: irq request failed", __func__);
			goto tsl2x7x_check_err1;
		}
	}

	/* Load up the defaults */
	tsl2x7x_defaults(chip);
	/* Make sure the chip is on */
	tsl2x7x_chip_on(indio_dev);

	/* 3.0.20 change: add buffer init and register */
	ret = tsl2x7x_register_buffer_funcs_and_init(indio_dev);
	if (ret) {
		dev_err(&clientp->dev, "buffer init failed\n");
		goto tsl2x7x_iio_register_err;
	}
	/* 3.0.20 change: end of add */

	ret = iio_device_register(indio_dev);
	if (ret) {
		dev_err(&clientp->dev,
				"%s: iio registration failed\n", __func__);
		goto tsl2x7x_iio_register_err;
	}
	pm_runtime_enable(&clientp->dev);
	pm_runtime_put_sync_suspend(&clientp->dev);
	pm_runtime_get_sync(&clientp->dev);
	pm_runtime_forbid(&clientp->dev);

	device_init_wakeup(&clientp->dev, 1);
	dev_info(&clientp->dev, "%s Light sensor found.\n", id->name);

	return 0;

tsl2x7x_iio_register_err:
	if (clientp->irq)
		free_irq(clientp->irq, indio_dev);
tsl2x7x_check_err1:
	destroy_workqueue(chip->tsl2x7x_wq);
tsl2x7x_check_err:
	sysfs_remove_group(&tsl2x7x_als_dev->dev.kobj, &tsl2x7x_als_attr_group);
als_sysfs_create_fail:
	input_unregister_device(tsl2x7x_als_dev);
als_input_register_fail:
	input_free_device(tsl2x7x_als_dev);
als_input_allocate_fail:
	sysfs_remove_group(&tsl2x7x_prox_dev->dev.kobj,
			&tsl2x7x_prox_attr_group);
prox_sysfs_create_fail:
	input_unregister_device(tsl2x7x_prox_dev);
prox_input_register_fail:
	input_free_device(tsl2x7x_prox_dev);
prox_input_allocate_fail:
#ifdef CONFIG_OF
	kfree(plat_data);
#endif
	iio_free_device(indio_dev);

	return ret;
}

static int tsl2x7x_runtime_suspend(struct device *dev)
{


	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct tsl2x7x_chip *chip = iio_priv(indio_dev);
	PL_DEGUG("\n");
	if (device_may_wakeup(dev))
		enable_irq_wake(chip->client->irq);
	return 0;
}

static int tsl2x7x_runtime_resume(struct device *dev)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct tsl2x7x_chip *chip = iio_priv(indio_dev);
	PL_DEGUG("\n");
	if (device_may_wakeup(dev))
		disable_irq_wake(chip->client->irq);
	return 0;
}


static int tsl2x7x_remove(struct i2c_client *client)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct tsl2x7x_chip *chip = iio_priv(indio_dev);

	tsl2x7x_chip_off(indio_dev);

	iio_device_unregister(indio_dev);
	if (client->irq)
		free_irq(client->irq, indio_dev);
	flush_workqueue(chip->tsl2x7x_wq);
	destroy_workqueue(chip->tsl2x7x_wq);
	sysfs_remove_group(&tsl2x7x_als_dev->dev.kobj,
			&tsl2x7x_als_attr_group);
	input_unregister_device(tsl2x7x_als_dev);
	input_free_device(tsl2x7x_als_dev);
	sysfs_remove_group(&tsl2x7x_prox_dev->dev.kobj,
			&tsl2x7x_prox_attr_group);
	input_unregister_device(tsl2x7x_prox_dev);
	input_free_device(tsl2x7x_prox_dev);
	iio_free_device(indio_dev);

	return 0;
}

static struct i2c_device_id tsl2x7x_idtable[] = {
	{ "tsl2571", tsl2571 },
	{ "tsl2671", tsl2671 },
	{ "tmd2671", tmd2671 },
	{ "tsl2771", tsl2771 },
	{ "tmd2771", tmd2771 },
	{ "tsl2572", tsl2572 },
	{ "tsl2672", tsl2672 },
	{ "tmd2672", tmd2672 },
	{ "tsl2772", tsl2772 },
	{ "tmd2772", tmd2772 },
	{ "tmd27713", tmd2771 },
	{}
};

static int tsl2x7x_power(struct device *dev, int on)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct tsl2x7x_chip *chip = iio_priv(indio_dev);
	u8 status;
	int ret;

	PL_DEGUG("\n");
	ret = tsl2x7x_i2c_read(chip->client,
			(TSL2X7X_CMD_REG | TSL2X7X_CNTRL), &status);
	if (ret < 0) {
		dev_err(&chip->client->dev,
				"%s: Failed to read STATUS Reg\n", __func__);
		return ret;
	}

	if (on)
		status |= TSL2X7X_CNTL_PWR_ON;
	else
		status &= ~TSL2X7X_CNTL_PWR_ON;

	ret = i2c_smbus_write_byte_data(chip->client,
			TSL2X7X_CMD_REG | TSL2X7X_CNTRL, status);
	if (ret < 0) {
		dev_err(&chip->client->dev,
				"%s: Failed to write STATUS Reg\n", __func__);
		return ret;
	}

	return 0;
}

static int tsl2x7x_suspend(struct device *dev)
{
	PL_DEGUG("\n");
	return tsl2x7x_power(dev, 0);
}

static int tsl2x7x_resume(struct device *dev)
{
	PL_DEGUG("\n");
	return tsl2x7x_power(dev, 1);
}


MODULE_DEVICE_TABLE(i2c, tsl2x7x_idtable);

static const struct dev_pm_ops tsl2x7x_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(tsl2x7x_suspend,
			tsl2x7x_resume)
		SET_RUNTIME_PM_OPS(tsl2x7x_runtime_suspend,
				tsl2x7x_runtime_resume, NULL)
};

/* Driver definition */
static struct i2c_driver tsl2x7x_driver = {
	.driver = {
		.name = "tsl2x7x",
		.pm = &tsl2x7x_pm_ops,
		.of_match_table = of_match_ptr(tsl2x7x_dt_ids),
	},
	.id_table = tsl2x7x_idtable,
	.probe = tsl2x7x_probe,
	.remove = tsl2x7x_remove,
};

module_i2c_driver(tsl2x7x_driver);

MODULE_AUTHOR("J. August Brenner<jbrenner@taosinc.com>");
MODULE_DESCRIPTION("TAOS tsl2x7x ambient and proximity light sensor driver");
MODULE_LICENSE("GPL");
