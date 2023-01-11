/*
 *  apds9930.c - Linux kernel modules for ambient light + proximity sensor
 *
 *  Copyright (C) 2012 Lee Kai Koon <kai-koon.lee@avagotech.com>
 *  Copyright (C) 2012 Avago Technologies
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/input.h>
#include <linux/ioctl.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>
#include <linux/pm.h>

#include "apds9930.h"

#define APDS9930_DRV_NAME	"avago,apds9930"
#define DRIVER_VERSION		"1.0.0"

#define ABS_LIGHT	0x29	/* added to support LIGHT - light sensor */

#define APDS9930_INT	IRQ_EINT(20)

#define APDS9930_PS_DETECTION_THRESHOLD		150
#define APDS9930_PS_HSYTERESIS_THRESHOLD	130
#define APDS9930_PS_PULSE_NUMBER		8

#define APDS9930_ALS_THRESHOLD_HSYTERESIS	20	/* 20 = 20% */

#define APDS9930_GA	48	/* 0.48 without glass window */
#define APDS9930_COE_B	223	/* 2.23 without glass window */
#define APDS9930_COE_C	70	/* 0.70 without glass window */
#define APDS9930_COE_D	142	/* 1.42 without glass window */
#define APDS9930_DF	52

#define DEVICE_ATTR2(_name, _mode, _show, _store) \
struct device_attribute dev_attr2_##_name = __ATTR(_name, _mode, _show, _store)

/* Change History
 *
 * 1.0.0	Fundamental Functions of APDS-993x
 *
 */

#define APDS_IOCTL_PS_ENABLE		1
#define APDS_IOCTL_PS_GET_ENABLE	2
#define APDS_IOCTL_PS_POLL_DELAY	3
#define APDS_IOCTL_ALS_ENABLE		4
#define APDS_IOCTL_ALS_GET_ENABLE	5
#define APDS_IOCTL_ALS_POLL_DELAY	6
#define APDS_IOCTL_PS_GET_PDATA		7	/* pdata */
#define APDS_IOCTL_ALS_GET_CH0DATA	8	/* ch0data */
#define APDS_IOCTL_ALS_GET_CH1DATA	9	/* ch1data */

#define APDS_DISABLE_PS			0
#define APDS_ENABLE_PS			1

#define APDS_DISABLE_ALS		0
#define APDS_ENABLE_ALS_WITH_INT	1
#define APDS_ENABLE_ALS_NO_INT		2

#define APDS_ALS_POLL_SLOW		0	/* 1 Hz (1s) */
#define APDS_ALS_POLL_MEDIUM		1	/* 10 Hz (100ms) */
#define APDS_ALS_POLL_FAST		2	/* 20 Hz (50ms) */

/*
 * Defines
 */

#define APDS9930_ENABLE_REG	0x00
#define APDS9930_ATIME_REG	0x01
#define APDS9930_PTIME_REG	0x02
#define APDS9930_WTIME_REG	0x03
#define APDS9930_AILTL_REG	0x04
#define APDS9930_AILTH_REG	0x05
#define APDS9930_AIHTL_REG	0x06
#define APDS9930_AIHTH_REG	0x07
#define APDS9930_PILTL_REG	0x08
#define APDS9930_PILTH_REG	0x09
#define APDS9930_PIHTL_REG	0x0A
#define APDS9930_PIHTH_REG	0x0B
#define APDS9930_PERS_REG	0x0C
#define APDS9930_CONFIG_REG	0x0D
#define APDS9930_PPCOUNT_REG	0x0E
#define APDS9930_CONTROL_REG	0x0F
#define APDS9930_REV_REG	0x11
#define APDS9930_ID_REG		0x12
#define APDS9930_STATUS_REG	0x13
#define APDS9930_CH0DATAL_REG	0x14
#define APDS9930_CH0DATAH_REG	0x15
#define APDS9930_CH1DATAL_REG	0x16
#define APDS9930_CH1DATAH_REG	0x17
#define APDS9930_PDATAL_REG	0x18
#define APDS9930_PDATAH_REG	0x19

#define CMD_BYTE		0x80
#define CMD_WORD		0xA0
#define CMD_SPECIAL		0xE0

#define CMD_CLR_PS_INT		0xE5
#define CMD_CLR_ALS_INT		0xE6
#define CMD_CLR_PS_ALS_INT	0xE7


/* Register Value define : ATIME */
#define APDS9930_100MS_ADC_TIME	0xDB  /* 100.64ms integration time */
#define APDS9930_50MS_ADC_TIME	0xED  /* 51.68ms integration time */
#define APDS9930_27MS_ADC_TIME	0xF6  /* 27.2ms integration time */

/* Register Value define : PRXCNFG */
#define APDS9930_ALS_REDUCE	0x04  /* ALSREDUCE - ALS Gain reduced by 4x */

/* Register Value define : PERS */
#define APDS9930_PPERS_0	0x00  /* Every proximity ADC cycle */
#define APDS9930_PPERS_1	0x10  /* 1 consecutive proximity value out of range */
#define APDS9930_PPERS_2	0x20  /* 2 consecutive proximity value out of range */
#define APDS9930_PPERS_3	0x30  /* 3 consecutive proximity value out of range */
#define APDS9930_PPERS_4	0x40  /* 4 consecutive proximity value out of range */
#define APDS9930_PPERS_5	0x50  /* 5 consecutive proximity value out of range */
#define APDS9930_PPERS_6	0x60  /* 6 consecutive proximity value out of range */
#define APDS9930_PPERS_7	0x70  /* 7 consecutive proximity value out of range */
#define APDS9930_PPERS_8	0x80  /* 8 consecutive proximity value out of range */
#define APDS9930_PPERS_9	0x90  /* 9 consecutive proximity value out of range */
#define APDS9930_PPERS_10	0xA0  /* 10 consecutive proximity value out of range */
#define APDS9930_PPERS_11	0xB0  /* 11 consecutive proximity value out of range */
#define APDS9930_PPERS_12	0xC0  /* 12 consecutive proximity value out of range */
#define APDS9930_PPERS_13	0xD0  /* 13 consecutive proximity value out of range */
#define APDS9930_PPERS_14	0xE0  /* 14 consecutive proximity value out of range */
#define APDS9930_PPERS_15	0xF0  /* 15 consecutive proximity value out of range */

#define APDS9930_APERS_0	0x00  /* Every ADC cycle */
#define APDS9930_APERS_1	0x01  /* 1 consecutive proximity value out of range */
#define APDS9930_APERS_2	0x02  /* 2 consecutive proximity value out of range */
#define APDS9930_APERS_3	0x03  /* 3 consecutive proximity value out of range */
#define APDS9930_APERS_5	0x04  /* 5 consecutive proximity value out of range */
#define APDS9930_APERS_10	0x05  /* 10 consecutive proximity value out of range */
#define APDS9930_APERS_15	0x06  /* 15 consecutive proximity value out of range */
#define APDS9930_APERS_20	0x07  /* 20 consecutive proximity value out of range */
#define APDS9930_APERS_25	0x08  /* 25 consecutive proximity value out of range */
#define APDS9930_APERS_30	0x09  /* 30 consecutive proximity value out of range */
#define APDS9930_APERS_35	0x0A  /* 35 consecutive proximity value out of range */
#define APDS9930_APERS_40	0x0B  /* 40 consecutive proximity value out of range */
#define APDS9930_APERS_45	0x0C  /* 45 consecutive proximity value out of range */
#define APDS9930_APERS_50	0x0D  /* 50 consecutive proximity value out of range */
#define APDS9930_APERS_55	0x0E  /* 55 consecutive proximity value out of range */
#define APDS9930_APERS_60	0x0F  /* 60 consecutive proximity value out of range */

/* Register Value define : CONTROL */
#define APDS9930_AGAIN_1X	0x00  /* 1X ALS GAIN */
#define APDS9930_AGAIN_8X	0x01  /* 8X ALS GAIN */
#define APDS9930_AGAIN_16X	0x02  /* 16X ALS GAIN */
#define APDS9930_AGAIN_120X	0x03  /* 120X ALS GAIN */

#define APDS9930_PRX_IR_DIOD	0x20  /* Proximity uses CH1 diode */

#define APDS9930_PGAIN_1X	0x00  /* PS GAIN 1X */
#define APDS9930_PGAIN_2X	0x04  /* PS GAIN 2X */
#define APDS9930_PGAIN_4X	0x08  /* PS GAIN 4X */
#define APDS9930_PGAIN_8X	0x0C  /* PS GAIN 8X */

#define APDS9930_PDRVIE_100MA	0x00  /* PS 100mA LED drive */
#define APDS9930_PDRVIE_50MA	0x40  /* PS 50mA LED drive */
#define APDS9930_PDRVIE_25MA	0x80  /* PS 25mA LED drive */
#define APDS9930_PDRVIE_12_5MA	0xC0  /* PS 12.5mA LED drive */


enum {
	APDS9930_ALS_RES_27MS = 0,    /* 27.2ms integration time */
	APDS9930_ALS_RES_51MS = 1,    /* 51.68ms integration time */
	APDS9930_ALS_RES_100MS = 2,     /* 100.64ms integration time */
} apds9930_als_res_e;

enum {
	APDS9930_ALS_GAIN_1X = 0,    /* 1x AGAIN */
	APDS9930_ALS_GAIN_8X = 1,    /* 8x AGAIN */
	APDS9930_ALS_GAIN_16X = 2,    /* 16x AGAIN */
	APDS9930_ALS_GAIN_120X = 3,     /* 120x AGAIN */
} apds9930_als_gain_e;

/*
 * Structs
 */

struct apds9930_data {
	struct i2c_client *client;
	struct mutex update_lock;
	struct delayed_work	dwork;		/* for PS interrupt */
	struct delayed_work	als_dwork;	/* for ALS polling */
	struct input_dev *input_dev_als;
	struct input_dev *input_dev_ps;
	struct regulator *avdd;

	int irq;
	int suspended;
	unsigned int enable_suspended_value;	/* suspend_resume usage */

	unsigned int enable;
	unsigned int atime;
	unsigned int ptime;
	unsigned int wtime;
	unsigned int ailt;
	unsigned int aiht;
	unsigned int pilt;
	unsigned int piht;
	unsigned int pers;
	unsigned int config;
	unsigned int ppcount;
	unsigned int control;

	/* control flag from HAL */
	unsigned int enable_ps_sensor;
	unsigned int enable_als_sensor;

	/* PS parameters */
	unsigned int ps_threshold;
	unsigned int ps_hysteresis_threshold;	/* always lower than ps_threshold */
	unsigned int ps_detection;		/* 0 = near-to-far; 1 = far-to-near */
	unsigned int ps_data;			/* to store PS data */

	/* ALS parameters */
	unsigned int als_threshold_l;	/* low threshold */
	unsigned int als_threshold_h;	/* high threshold */
	unsigned int als_data;		/* to store ALS data */
	int als_prev_lux;		/* to store previous lux value */

	unsigned int als_gain;		/* needed for Lux calculation */
	unsigned int als_poll_delay;	/* needed for light sensor polling : micro-second (us) */
	unsigned int als_atime_index;	/* storage for als integratiion time */
	unsigned int als_again_index;	/* storage for als GAIN */
	unsigned int als_reduce;	/* flag indicate ALS 6x reduction */

	struct apds9930_platform_data *pdata;	/* platform data */
};

/*
 * Global data
 */
static struct i2c_client *apds9930_i2c_client; /* global i2c_client to support ioctl */
static struct workqueue_struct *apds_workqueue;

static unsigned char apds9930_als_atime_tb[] = { 0xF6, 0xED, 0xDB };
static unsigned short __attribute__ ((unused))
		apds9930_als_integration_tb[] = {2720, 5168, 10064};
static unsigned short __attribute__ ((unused))
		apds9930_als_res_tb[] = { 10240, 19456, 37888 };
static unsigned char __attribute__ ((unused))
		apds9930_als_again_tb[] = { 1, 8, 16, 120 };
static unsigned char __attribute__ ((unused))
		apds9930_als_again_bit_tb[] = { 0x00, 0x01, 0x02, 0x03 };

/*
 * Management functions
 */

static int apds9930_set_command(struct i2c_client *client, int command)
{
	int ret;
	int clearInt;

	if (command == 0)
		clearInt = CMD_CLR_PS_INT;
	else if (command == 1)
		clearInt = CMD_CLR_ALS_INT;
	else
		clearInt = CMD_CLR_PS_ALS_INT;

	ret = i2c_smbus_write_byte(client, clearInt);

	return ret;
}

static int apds9930_set_enable(struct i2c_client *client, int enable)
{
	struct apds9930_data *data = i2c_get_clientdata(client);
	int ret;

	ret = i2c_smbus_write_byte_data(client, CMD_BYTE | APDS9930_ENABLE_REG, enable);
	data->enable = enable;

	return ret;
}

static int apds9930_set_atime(struct i2c_client *client, int atime)
{
	struct apds9930_data *data = i2c_get_clientdata(client);
	int ret;

	ret = i2c_smbus_write_byte_data(client, CMD_BYTE | APDS9930_ATIME_REG, atime);

	data->atime = atime;

	return ret;
}

static int apds9930_set_ptime(struct i2c_client *client, int ptime)
{
	struct apds9930_data *data = i2c_get_clientdata(client);
	int ret;

	ret = i2c_smbus_write_byte_data(client, CMD_BYTE | APDS9930_PTIME_REG, ptime);

	data->ptime = ptime;

	return ret;
}

static int apds9930_set_wtime(struct i2c_client *client, int wtime)
{
	struct apds9930_data *data = i2c_get_clientdata(client);
	int ret;

	ret = i2c_smbus_write_byte_data(client, CMD_BYTE | APDS9930_WTIME_REG, wtime);

	data->wtime = wtime;

	return ret;
}

static int apds9930_set_ailt(struct i2c_client *client, int threshold)
{
	struct apds9930_data *data = i2c_get_clientdata(client);
	int ret;

	ret = i2c_smbus_write_word_data(client, CMD_WORD | APDS9930_AILTL_REG, threshold);

	data->ailt = threshold;

	return ret;
}

static int apds9930_set_aiht(struct i2c_client *client, int threshold)
{
	struct apds9930_data *data = i2c_get_clientdata(client);
	int ret;

	ret = i2c_smbus_write_word_data(client, CMD_WORD | APDS9930_AIHTL_REG, threshold);

	data->aiht = threshold;

	return ret;
}

static int apds9930_set_pilt(struct i2c_client *client, int threshold)
{
	struct apds9930_data *data = i2c_get_clientdata(client);
	int ret;

	ret = i2c_smbus_write_word_data(client, CMD_WORD | APDS9930_PILTL_REG, threshold);

	data->pilt = threshold;

	return ret;
}

static int apds9930_set_piht(struct i2c_client *client, int threshold)
{
	struct apds9930_data *data = i2c_get_clientdata(client);
	int ret;

	ret = i2c_smbus_write_word_data(client, CMD_WORD | APDS9930_PIHTL_REG, threshold);

	data->piht = threshold;

	return ret;
}

static int apds9930_set_pers(struct i2c_client *client, int pers)
{
	struct apds9930_data *data = i2c_get_clientdata(client);
	int ret;

	ret = i2c_smbus_write_byte_data(client, CMD_BYTE | APDS9930_PERS_REG, pers);

	data->pers = pers;

	return ret;
}

static int apds9930_set_config(struct i2c_client *client, int config)
{
	struct apds9930_data *data = i2c_get_clientdata(client);
	int ret;

	ret = i2c_smbus_write_byte_data(client, CMD_BYTE | APDS9930_CONFIG_REG, config);

	data->config = config;

	return ret;
}

static int apds9930_set_ppcount(struct i2c_client *client, int ppcount)
{
	struct apds9930_data *data = i2c_get_clientdata(client);
	int ret;

	ret = i2c_smbus_write_byte_data(client, CMD_BYTE | APDS9930_PPCOUNT_REG, ppcount);

	data->ppcount = ppcount;

	return ret;
}

static int apds9930_set_control(struct i2c_client *client, int control)
{
	struct apds9930_data *data = i2c_get_clientdata(client);
	int ret;

	ret = i2c_smbus_write_byte_data(client, CMD_BYTE | APDS9930_CONTROL_REG, control);

	data->control = control;

	return ret;
}

static int luxcalculation(struct i2c_client *client, int ch0data, int ch1data)
{
	int IAC1 = 0;
	int IAC2 = 0;
	int IAC = 0;

	/* re-adjust COE_B to avoid 2 decimal point */
	IAC1 = (ch0data - (APDS9930_COE_B*ch1data) / 100);
	/* re-adjust COE_C and COE_D to void 2 decimal point */
	IAC2 = ((APDS9930_COE_C*ch0data) / 100 - (APDS9930_COE_D*ch1data) / 100);

	if (IAC1 > IAC2)
		IAC = IAC1;
	else if (IAC1 <= IAC2)
		IAC = IAC2;
	else
		IAC = 0;

	if (IAC1 < 0 && IAC2 < 0) {
		IAC = 0;	/* cdata and irdata saturated */
		return -1;	/* don't report first, change gain may help */
	}

	return IAC;
}

static void apds9930_change_ps_threshold(struct i2c_client *client)
{
	struct apds9930_data *data = i2c_get_clientdata(client);

	data->ps_data =	i2c_smbus_read_word_data(client, CMD_WORD | APDS9930_PDATAL_REG);

	if ((data->ps_data > data->pilt) && (data->ps_data >= data->piht)) {
		/* far-to-near detected */
		data->ps_detection = 1;

		data->ps_data = 2;
		/* FAR-to-NEAR detection */
		input_report_abs(data->input_dev_ps, ABS_DISTANCE, data->ps_data);
		input_sync(data->input_dev_ps);

		i2c_smbus_write_word_data(client, CMD_WORD | APDS9930_PILTL_REG,
					  data->ps_hysteresis_threshold);
		i2c_smbus_write_word_data(client, CMD_WORD | APDS9930_PIHTL_REG, 1023);

		data->pilt = data->ps_hysteresis_threshold;
		data->piht = 1023;

		pr_debug("far-to-near detected\n");
	} else if ((data->ps_data <= data->pilt) && (data->ps_data < data->piht)) {
		/* near-to-far detected */
		data->ps_detection = 0;
		/* NEAR-to-FAR detection */
		input_report_abs(data->input_dev_ps, ABS_DISTANCE, 10);
		input_sync(data->input_dev_ps);

		i2c_smbus_write_word_data(client, CMD_WORD | APDS9930_PILTL_REG, 0);
		i2c_smbus_write_word_data(client, CMD_WORD | APDS9930_PIHTL_REG,
					  data->ps_threshold);

		data->pilt = 0;
		data->piht = data->ps_threshold;

		pr_debug("near-to-far detected\n");
	}
	pr_debug
	    ("high threshhold change to %d, the low threshhold change to %d",
	     data->pilt, data->piht);
}

static void apds9930_change_als_threshold(struct i2c_client *client)
{
	struct apds9930_data *data = i2c_get_clientdata(client);
	int ch0data, ch1data;
	int luxValue = 0;

	ch0data = i2c_smbus_read_word_data(client, CMD_WORD | APDS9930_CH0DATAL_REG);
	ch1data = i2c_smbus_read_word_data(client, CMD_WORD | APDS9930_CH1DATAL_REG);

	luxValue = luxcalculation(client, ch0data, ch1data);

	luxValue = luxValue > 0 ? luxValue : 0;
	luxValue = luxValue < 10000 ? luxValue : 10000;

	/* check PS under sunlight */
	/* PS was previously in far-to-near condition */
	if ((data->ps_detection == 1) && (ch0data >
				(75 * (1024 * (256 - apds9930_als_atime_tb[data->als_atime_index]))) / 100)) {
		/* need to inform input event as there will be no interrupt from the PS */
		input_report_abs(data->input_dev_ps, ABS_DISTANCE, 0); /* NEAR-to-FAR detection */
		input_sync(data->input_dev_ps);

		i2c_smbus_write_word_data(client, CMD_WORD | APDS9930_PILTL_REG, 0);
		i2c_smbus_write_word_data(client, CMD_WORD | APDS9930_PIHTL_REG, data->ps_threshold);

		data->pilt = 0;
		data->piht = data->ps_threshold;

		data->ps_detection = 0;	/* near-to-far detected */

		pr_debug("apds_993x_proximity_handler = FAR\n");
	}

	input_report_abs(data->input_dev_als, ABS_PRESSURE, luxValue); /* report the lux level */
	input_sync(data->input_dev_als);
	/* restart timer */
	schedule_delayed_work(&data->als_dwork, msecs_to_jiffies(data->als_poll_delay));
}

static void apds9930_reschedule_work(struct apds9930_data *data,
					  unsigned long delay)
{
	/*
	 * If work is already scheduled then subsequent schedules will not
	 * change the scheduled time that's why we have to cancel it first.
	 */
	cancel_delayed_work(&data->dwork);
	queue_delayed_work(apds_workqueue, &data->dwork, delay);
}

/* ALS polling routine */
static void apds9930_als_polling_work_handler(struct work_struct *work)
{
	struct apds9930_data *data = container_of(work, struct apds9930_data, als_dwork.work);
	struct i2c_client *client = data->client;
	int ch0data, ch1data, pdata;
	int luxValue = 0;

	ch0data = i2c_smbus_read_word_data(client, CMD_WORD | APDS9930_CH0DATAL_REG);
	ch1data = i2c_smbus_read_word_data(client, CMD_WORD | APDS9930_CH1DATAL_REG);
	pdata = i2c_smbus_read_word_data(client, CMD_WORD | APDS9930_PDATAL_REG);

	luxValue = luxcalculation(client, ch0data, ch1data);

	luxValue = luxValue > 0 ? luxValue : 0;
	luxValue = luxValue < 10000 ? luxValue : 10000;

	data->als_data = luxValue;

	pr_debug("lux=%d ch0data=%d ch1data=%d pdata=%d delay=%d again=%d als_reduce=%d)\n",
			luxValue, ch0data, ch1data, pdata, data->als_poll_delay,
				apds9930_als_again_tb[data->als_again_index], data->als_reduce);

	/* check PS under sunlight */
	/* PS was previously in far-to-near condition */
	if ((data->ps_detection == 1) && (ch0data > (75*(1024*(256-data->atime)))/100)) {
		/* need to inform input event as there will be no interrupt from the PS */
		input_report_abs(data->input_dev_ps, ABS_DISTANCE, 0); /* NEAR-to-FAR detection */
		input_sync(data->input_dev_ps);

		i2c_smbus_write_word_data(client, CMD_WORD | APDS9930_PILTL_REG, 0);
		i2c_smbus_write_word_data(client, CMD_WORD | APDS9930_PIHTL_REG, data->ps_threshold);

		data->pilt = 0;
		data->piht = data->ps_threshold;

		data->ps_detection = 0;	/* near-to-far detected */

		pr_debug("apds_993x_proximity_handler = FAR\n");
	}

	input_report_abs(data->input_dev_als, ABS_PRESSURE, luxValue);	/*report the lux level */
	input_sync(data->input_dev_als);
	/* restart timer */
	schedule_delayed_work(&data->als_dwork, msecs_to_jiffies(data->als_poll_delay));
}

/* PS interrupt routine */
static void apds9930_work_handler(struct work_struct *work)
{
	struct apds9930_data *data = container_of(work, struct apds9930_data, dwork.work);
	struct i2c_client *client = data->client;
	int status;
	int ch0data;
	int enable;

	status = i2c_smbus_read_byte_data(client, CMD_BYTE | APDS9930_STATUS_REG);
	enable = i2c_smbus_read_byte_data(client, CMD_BYTE | APDS9930_ENABLE_REG);

	/* disable 993x's ADC first */
	i2c_smbus_write_byte_data(client, CMD_BYTE | APDS9930_ENABLE_REG, 1);

	pr_debug("status = %x\n", status);

	if ((status & enable & 0x30) == 0x30) {
		/* both PS and ALS are interrupted */
		apds9930_change_als_threshold(client);

		ch0data = i2c_smbus_read_word_data(client, CMD_WORD | APDS9930_CH0DATAL_REG);
		if (ch0data < (75 * (1024 * (256 - data->atime))) / 100)
			apds9930_change_ps_threshold(client);
		else {
			if (data->ps_detection == 1)
				apds9930_change_ps_threshold(client);
			else
				pr_debug("Triggered by background ambient noise\n");
		}

		apds9930_set_command(client, 2);	/* 2 = CMD_CLR_PS_ALS_INT */
	} else if ((status & enable & 0x20) == 0x20) {
		/* only PS is interrupted */

		/* check if this is triggered by background ambient noise */
		ch0data = i2c_smbus_read_word_data(client, CMD_WORD | APDS9930_CH0DATAL_REG);
		if (ch0data < (75 * (apds9930_als_res_tb[data->als_atime_index])) / 100)
			apds9930_change_ps_threshold(client);
		else {
			if (data->ps_detection == 1)
				apds9930_change_ps_threshold(client);
			else
				pr_debug("Triggered by background ambient noise\n");
		}

		apds9930_set_command(client, 0);	/* 0 = CMD_CLR_PS_INT */
	} else if ((status & enable & 0x10) == 0x10) {
		/* only ALS is interrupted */
		apds9930_change_als_threshold(client);

		apds9930_set_command(client, 1);	/* 1 = CMD_CLR_ALS_INT */
	}

	i2c_smbus_write_byte_data(client, CMD_BYTE | APDS9930_ENABLE_REG, data->enable);
}

/* assume this is ISR */
static irqreturn_t apds9930_interrupt(int vec, void *info)
{
	struct i2c_client *client = (struct i2c_client *)info;
	struct apds9930_data *data = i2c_get_clientdata(client);

	apds9930_reschedule_work(data, 0);

	return IRQ_HANDLED;
}

/*
 * IOCTL support
 */

static int apds9930_enable_als_sensor(struct i2c_client *client, int val)
{
	struct apds9930_data *data = i2c_get_clientdata(client);

	pr_debug("%s: enable als sensor ( %d)\n", __func__, val);

	if ((val != APDS_DISABLE_ALS) && (val != APDS_ENABLE_ALS_WITH_INT)
					&& (val != APDS_ENABLE_ALS_NO_INT)) {
		pr_debug("%s: enable als sensor=%d\n", __func__, val);
		return -1;
	}

	if ((val == APDS_ENABLE_ALS_WITH_INT) || (val == APDS_ENABLE_ALS_NO_INT)) {

		if (regulator_enable(data->avdd)) {
			dev_err(&client->dev, "avago sensor avdd power supply enable failed\n");
			goto out;
		}

		/* turn on light  sensor */
		data->enable_als_sensor = val;

		apds9930_set_enable(client, 0); /* Power Off */
		/* 100.64ms */
		apds9930_set_atime(client, apds9930_als_atime_tb[data->als_atime_index]);

		apds9930_set_ailt(client, 0);
		apds9930_set_aiht(client, 0xffff);

		apds9930_set_control(client, 0xE0);	/* 12.5mA, IR-diode, 1X PGAIN, 1X AGAIN */
		apds9930_set_pers(client, 0x13);	/* 3 persistence */
		apds9930_set_enable(client, 0x3);	/* only enable light sensor */

		/*
		 * If work is already scheduled then subsequent schedules will not
		 * change the scheduled time that's why we have to cancel it first.
		 */
		cancel_delayed_work(&data->als_dwork);
		flush_delayed_work(&data->als_dwork);
		queue_delayed_work(apds_workqueue, &data->als_dwork,
				   msecs_to_jiffies(data->als_poll_delay));
	} else {
		/* turn off light sensor
		 * what if the p sensor is active?
		 */
		data->enable_als_sensor = APDS_DISABLE_ALS;

		if (data->enable_ps_sensor)
			pr_info("PS sensor cannot work without ALS sensor");
		else {
			apds9930_set_enable(client, 0);
		/*
		 * If work is already scheduled then subsequent schedules will not
		 * change the scheduled time that's why we have to cancel it first.
		 */
			cancel_delayed_work(&data->als_dwork);
			flush_delayed_work(&data->als_dwork);
			regulator_disable(data->avdd);
		}
	}
out:
	return 0;
}

static int apds9930_set_als_poll_delay(struct i2c_client *client, unsigned int val)
{
	struct apds9930_data *data = i2c_get_clientdata(client);
	int ret;
	int atime_index = 0;

	pr_debug("%s : %d\n", __func__, val);

	if ((val != APDS_ALS_POLL_SLOW) && (val != APDS_ALS_POLL_MEDIUM)
						&& (val != APDS_ALS_POLL_FAST)) {
		pr_debug("%s:invalid value=%d\n", __func__, val);
		return -1;
	}

	if (val == APDS_ALS_POLL_FAST) {
		data->als_poll_delay = 50;		/* 50ms */
		atime_index = APDS9930_ALS_RES_27MS;
	} else if (val == APDS_ALS_POLL_MEDIUM) {
		data->als_poll_delay = 100;		/* 100ms */
		atime_index = APDS9930_ALS_RES_51MS;
	} else {	/* APDS_ALS_POLL_SLOW */
		data->als_poll_delay = 1000;		/* 1000ms */
		atime_index = APDS9930_ALS_RES_100MS;
	}

	ret = apds9930_set_atime(client, apds9930_als_atime_tb[atime_index]);
	if (ret >= 0) {
		data->als_atime_index = atime_index;
		pr_debug("poll delay %d, atime_index %d\n", data->als_poll_delay, data->als_atime_index);
	} else {
		return -1;
	}

	/*
	 * If work is already scheduled then subsequent schedules will not
	 * change the scheduled time that's why we have to cancel it first.
	 */
	cancel_delayed_work(&data->als_dwork);
	flush_delayed_work(&data->als_dwork);
	queue_delayed_work(apds_workqueue, &data->als_dwork, msecs_to_jiffies(data->als_poll_delay));

	return 0;
}

static int apds9930_enable_ps_sensor(struct i2c_client *client, int val)
{
	struct apds9930_data *data = i2c_get_clientdata(client);

	pr_debug("enable ps senosr ( %d)\n", val);

	if ((val != APDS_DISABLE_PS) && (val != APDS_ENABLE_PS)) {
		pr_debug("%s:invalid value=%d\n", __func__, val);
		return -1;
	}

	if (val == APDS_ENABLE_PS) {

		if (regulator_enable(data->avdd)) {
			dev_err(&client->dev, "avago sensor avdd power supply enable failed\n");
			goto out;
		}

		/* turn on p sensor */
		if (data->enable_ps_sensor == APDS_DISABLE_PS) {

			data->enable_ps_sensor = APDS_ENABLE_PS;

			apds9930_set_enable(client, 0); /* Power Off */
			apds9930_set_atime(client, 0xf6);	/* 27.2ms */
			apds9930_set_ptime(client, 0xff);	/* 2.72ms */

			apds9930_set_ppcount(client, 8);	/* 8-pulse */
			/* 12.5mA, IR-diode, 1X PGAIN, 1X AGAIN */
			apds9930_set_control(client, 0xE0);

			apds9930_set_pilt(client, 0);
			apds9930_set_piht(client, APDS9930_PS_DETECTION_THRESHOLD);
			apds9930_set_pers(client, 0x13);	/* 3 persistence */

			data->ps_threshold = APDS9930_PS_DETECTION_THRESHOLD;
			data->ps_hysteresis_threshold =
			    APDS9930_PS_HSYTERESIS_THRESHOLD;

			apds9930_set_ailt(client, 0);
			apds9930_set_aiht(client, 0xffff);
			apds9930_set_enable(client, 0x27);	/* only enable PS interrupt */
		}
	} else {
		/*
		 * turn off p sensor - kk 25 Apr 2011 we can't turn off the entire sensor,
		 * the light sensor may be needed by HAL.
		 */
		data->enable_ps_sensor = APDS_DISABLE_PS;
		if (data->enable_als_sensor == APDS_ENABLE_ALS_NO_INT) {
			apds9930_set_enable(client, 0x0B);	 /* no ALS interrupt */

			/*
			 * If work is already scheduled then subsequent schedules will not
			 * change the scheduled time that's why we have to cancel it first.
			 */
			cancel_delayed_work(&data->als_dwork);
			flush_delayed_work(&data->als_dwork);
			queue_delayed_work(apds_workqueue, &data->als_dwork, msecs_to_jiffies(data->als_poll_delay));	/* 100ms */
		} else if (data->enable_als_sensor == APDS_ENABLE_ALS_WITH_INT) {
			/* reconfigute light sensor setting */
			apds9930_set_enable(client, 0); /* Power Off */
			apds9930_set_ailt(client, 0xFFFF);	/* Force ALS interrupt */
			apds9930_set_aiht(client, 0);

			apds9930_set_enable(client, 0x13);	 /* enable ALS interrupt */
		} else {	/* APDS_DISABLE_ALS */
			apds9930_set_enable(client, 0);

			/*
			 * If work is already scheduled then subsequent schedules will not
			 * change the scheduled time that's why we have to cancel it first.
			 */
			cancel_delayed_work(&data->als_dwork);
			flush_delayed_work(&data->als_dwork);
		}
		regulator_disable(data->avdd);
	}
out:
	return 0;
}

static int apds9930_ps_open(struct inode *inode, struct file *file)
{
	pr_debug("apds9930_ps_open\n");
	return 0;
}

static int apds9930_ps_release(struct inode *inode, struct file *file)
{
	pr_debug("apds9930_ps_release\n");
	return 0;
}

static long apds9930_ps_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct apds9930_data *data;
	struct i2c_client *client;
	int enable;
	int ret = -1;

	if (arg == 0)
		return -1;
	if (apds9930_i2c_client == NULL) {
		pr_debug("apds9930_ps_ioctl error: i2c driver not installed\n");
		return -EFAULT;
	}

	client = apds9930_i2c_client;
	data = i2c_get_clientdata(apds9930_i2c_client);

	switch (cmd) {
	case APDS_IOCTL_PS_ENABLE:
		if (copy_from_user(&enable, (void __user *)arg, sizeof(enable))) {
			pr_debug("apds9930_ps_ioctl: copy_from_user failed\n");
			return -EFAULT;
		}
		ret = apds9930_enable_ps_sensor(client, enable);
		if (ret < 0)
			return ret;
		break;
	case APDS_IOCTL_PS_GET_ENABLE:
		if (copy_to_user((void __user *)arg, &data->enable_ps_sensor,
						sizeof(data->enable_ps_sensor))) {
			pr_debug("apds9930_ps_ioctl: copy_to_user failed\n");
			return -EFAULT;
		}
		break;
	case APDS_IOCTL_PS_GET_PDATA:
		data->ps_data =	i2c_smbus_read_word_data(client, CMD_WORD | APDS9930_PDATAL_REG);
		if (copy_to_user((void __user *)arg, &data->ps_data, sizeof(data->ps_data))) {
			pr_debug("apds9930_ps_ioctl: copy_to_user failed\n");
			return -EFAULT;
		}
		break;
	default:
		break;
	}

	return 0;
}

static int apds9930_als_open(struct inode *inode, struct file *file)
{
	pr_debug("apds9930_als_open\n");
	return 0;
}

static int apds9930_als_release(struct inode *inode, struct file *file)
{
	pr_debug("apds9930_als_release\n");
	return 0;
}

static long apds9930_als_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct apds9930_data *data;
	struct i2c_client *client;
	int enable;
	int ret = -1;
	unsigned int delay;

	if (arg == 0)
		return -1;

	if (apds9930_i2c_client == NULL) {
		pr_debug("apds9930_als_ioctl error: i2c driver not installed\n");
		return -EFAULT;
	}

	client = apds9930_i2c_client;
	data = i2c_get_clientdata(apds9930_i2c_client);

	switch (cmd) {
	case APDS_IOCTL_ALS_ENABLE:
		if (copy_from_user(&enable, (void __user *)arg, sizeof(enable))) {
			pr_debug("apds9930_als_ioctl: copy_from_user failed\n");
			return -EFAULT;
		}
		ret = apds9930_enable_als_sensor(client, enable);
		if (ret < 0)
			return ret;
		break;
	case APDS_IOCTL_ALS_POLL_DELAY:
		if (data->enable_als_sensor == APDS_ENABLE_ALS_NO_INT) {
			if (copy_from_user(&delay, (void __user *)arg, sizeof(delay))) {
				pr_debug("apds9930_als_ioctl: copy_to_user failed\n");
				return -EFAULT;
			}
			ret = apds9930_set_als_poll_delay(client, delay);

			if (ret < 0)
				return ret;
		} else {
			pr_debug("apds9930_als_ioctl: als is not in polling mode!\n");
			return -EFAULT;
		}
		break;
	case APDS_IOCTL_ALS_GET_ENABLE:
		if (copy_to_user((void __user *)arg, &data->enable_als_sensor,
						sizeof(data->enable_als_sensor))) {
			pr_debug("apds9930_als_ioctl: copy_to_user failed\n");
			return -EFAULT;
		}
		break;
	case APDS_IOCTL_ALS_GET_CH0DATA:
		data->als_data = i2c_smbus_read_word_data(client, CMD_WORD | APDS9930_CH0DATAL_REG);
		if (copy_to_user((void __user *)arg, &data->als_data, sizeof(data->als_data))) {
			pr_debug("apds9930_ps_ioctl: copy_to_user failed\n");
			return -EFAULT;
		}
		break;
	case APDS_IOCTL_ALS_GET_CH1DATA:
		data->als_data = i2c_smbus_read_word_data(client, CMD_WORD | APDS9930_CH1DATAL_REG);
		if (copy_to_user((void __user *)arg, &data->als_data, sizeof(data->als_data))) {
			pr_debug("apds9930_ps_ioctl: copy_to_user failed\n");
			return -EFAULT;
		}
		break;
	default:
		break;
	}

	return 0;
}

/*
 * SysFS support
 */

static ssize_t apds9930_show_ch0data(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct apds9930_data *data = input_get_drvdata(input);
	struct i2c_client *client = data->client;

	int ch0data;

	ch0data = i2c_smbus_read_word_data(client, CMD_WORD | APDS9930_CH0DATAL_REG);

	return sprintf(buf, "%d\n", ch0data);
}

static DEVICE_ATTR(ch0data, S_IRUGO,
		   apds9930_show_ch0data, NULL);

static ssize_t apds9930_show_ch1data(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct apds9930_data *data = input_get_drvdata(input);
	struct i2c_client *client = data->client;

	int ch1data;

	ch1data = i2c_smbus_read_word_data(client, CMD_WORD | APDS9930_CH1DATAL_REG);

	return sprintf(buf, "%d\n", ch1data);
}

static DEVICE_ATTR(ch1data, S_IRUGO,
		   apds9930_show_ch1data, NULL);

static ssize_t apds9930_show_pdata(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct apds9930_data *data = input_get_drvdata(input);
	struct i2c_client *client = data->client;

	int pdata;

	pdata = i2c_smbus_read_word_data(client, CMD_WORD | APDS9930_PDATAL_REG);

	return sprintf(buf, "%d\n", pdata);
}

static DEVICE_ATTR(pdata, S_IRUGO,
		   apds9930_show_pdata, NULL);

static ssize_t apds9930_show_proximity_enable(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct apds9930_data *data = input_get_drvdata(input);

	return sprintf(buf, "%d\n", data->enable_ps_sensor);
}

static ssize_t apds9930_store_proximity_enable(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct apds9930_data *data = input_get_drvdata(input);
	struct i2c_client *client = data->client;

	unsigned long val;
	int success = kstrtoul(buf, 10, &val);

	if (success == 0) {
		pr_debug("%s: enable ps senosr ( %ld)\n", __func__, val);
		if ((val != APDS_DISABLE_PS) && (val != APDS_ENABLE_PS)) {
			pr_debug("**%s:store invalid value=%ld\n", __func__, val);
			return count;
		}
		apds9930_enable_ps_sensor(client, val);
	}

	return count;
}

static DEVICE_ATTR(active, S_IRUGO | S_IWUSR | S_IWGRP,
		apds9930_show_proximity_enable, apds9930_store_proximity_enable);

static ssize_t apds9930_show_light_enable(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct apds9930_data *data = input_get_drvdata(input);

	return sprintf(buf, "%d\n", data->enable_als_sensor);
}

static ssize_t apds9930_store_light_enable(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct apds9930_data *data = input_get_drvdata(input);
	struct i2c_client *client = data->client;

	unsigned long val;
	int success = kstrtoul(buf, 10, &val);

	if (success == 0) {
		pr_debug("%s: enable als sensor ( %ld)\n", __func__, val);
		if ((val != APDS_DISABLE_ALS) && (val != APDS_ENABLE_ALS_WITH_INT)
						&& (val != APDS_ENABLE_ALS_NO_INT)) {
			pr_debug("**%s: store invalid valeu=%ld\n", __func__, val);
			return count;
		}
		apds9930_enable_als_sensor(client, val);
	}

	return count;
}

static DEVICE_ATTR2(active, S_IRUGO | S_IWUSR | S_IWGRP,
		apds9930_show_light_enable, apds9930_store_light_enable);

static struct attribute *apds9930_als_attributes[] = {
	&dev_attr_ch0data.attr,
	&dev_attr_ch1data.attr,
	&dev_attr2_active.attr,
	NULL
};

static const struct attribute_group apds9930_als_attr_group = {
	.attrs = apds9930_als_attributes,
};

static struct attribute *apds9930_ps_attributes[] = {
	&dev_attr_pdata.attr,
	&dev_attr_active.attr,
	NULL
};

static const struct attribute_group apds9930_ps_attr_group = {
	.attrs = apds9930_ps_attributes,
};


static const struct file_operations apds9930_ps_fops = {
	.owner = THIS_MODULE,
	.open = apds9930_ps_open,
	.release = apds9930_ps_release,
	.unlocked_ioctl = apds9930_ps_ioctl,
};

static struct miscdevice apds9930_ps_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "apds_ps_dev",
	.fops = &apds9930_ps_fops,
};

static const struct file_operations apds9930_als_fops = {
	.owner = THIS_MODULE,
	.open = apds9930_als_open,
	.release = apds9930_als_release,
	.unlocked_ioctl = apds9930_als_ioctl,
};

static struct miscdevice apds9930_als_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "apds_als_dev",
	.fops = &apds9930_als_fops,
};

/*
 * Initialization function
 */

static int apds9930_init_client(struct i2c_client *client)
{
	struct apds9930_data *data = i2c_get_clientdata(client);
	int err;
	int id;

	err = apds9930_set_enable(client, 0);

	if (err < 0)
		return err;

	id = i2c_smbus_read_byte_data(client, CMD_BYTE | APDS9930_ID_REG);
	if (id == 0x39 || id == 0x30) {
		pr_debug("APDS-9930\n");
	} else {
		pr_debug("Not APDS-9930\n");
		return -EIO;
	}

	/* 100.64ms ALS integration time */
	err = apds9930_set_atime(client, apds9930_als_atime_tb[data->als_atime_index]);
	if (err < 0)
		return err;

	err = apds9930_set_ptime(client, 0xFF);	/* 2.72ms Prox integration time */
	if (err < 0)
		return err;

	err = apds9930_set_wtime(client, 0xFF);	/* 27.2ms Wait time */
	if (err < 0)
		return err;

	err = apds9930_set_ppcount(client, APDS9930_PS_PULSE_NUMBER);
	if (err < 0)
		return err;

	err = apds9930_set_config(client, 0);		/* no long wait */
	if (err < 0)
		return err;

	err = apds9930_set_control(client, 0xE0);
	if (err < 0)
		return err;

	err = apds9930_set_pilt(client, 0);		/* init threshold for proximity */
	if (err < 0)
		return err;

	err = apds9930_set_piht(client, APDS9930_PS_DETECTION_THRESHOLD);
	if (err < 0)
		return err;

	/* force first ALS interrupt to get the environment reading */
	err = apds9930_set_ailt(client, 0);
	if (err < 0)
		return err;

	err = apds9930_set_aiht(client, 0xFFFF);
	if (err < 0)
		return err;

	err = apds9930_set_pers(client, 0x12);	/* 2 consecutive Interrupt persistence */
	if (err < 0)
		return err;

	/* sensor is in disabled mode but all the configurations are preset */

	return 0;
}

#ifdef CONFIG_OF
static int apds9930_probe_dt(struct i2c_client *client)
{
	struct apds9930_platform_data *platform_data;
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

static struct of_device_id inv_match_table[] = {
	{ .compatible = "avago,apds9930", },
	{}
};
#endif

/*
 * I2C init/probing/exit functions
 */

static struct i2c_driver apds9930_driver;
static int apds9930_probe(struct i2c_client *client,
				   const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct apds9930_data *data;
	struct apds9930_platform_data *pdata;
	struct regulator *avdd;
	int err = 0;

#ifdef CONFIG_OF
	err = apds9930_probe_dt(client);

	if (err == -ENOMEM) {
		dev_err(&client->dev,
		"%s: Failed to alloc mem for apds9930_platform_data\n", __func__);
		return err;
	} else if (err == -EINVAL) {
		kfree(client->dev.platform_data);
		dev_err(&client->dev,
			"%s: Probe device tree data failed\n", __func__);
		return err;
	}

	pdata = client->dev.platform_data;
#else
	pdata = client->dev.platform_data;
	if (!pdata) {
		dev_err(&client->dev, "%s: No platform data found\n", __func__);
		return -EINVAL;
	}
#endif

	avdd = regulator_get(&client->dev, "avdd");
	if (IS_ERR(avdd)) {
		dev_err(&client->dev, "sensor avdd power supply get failed\n");
		goto out;
	}

	regulator_set_voltage(avdd, 2800000, 2800000);
	if (regulator_enable(avdd)) {
		dev_err(&client->dev, "avago sensors regulator enable failed\n");
		goto out;
	}

	/* add delay to make sure ldo enabled */
	usleep_range(2000, 2200);

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE)) {
		err = -EIO;
		goto exit;
	}

	if (i2c_smbus_read_byte(client) < 0) {
		dev_err(&client->dev, "i2c_smbus_read_byte error!\n");
		err = -EIO;
		goto exit;
	}

	data = kzalloc(sizeof(struct apds9930_data), GFP_KERNEL);
	if (!data) {
		err = -ENOMEM;
		goto exit;
	}

	data->client = client;
	apds9930_i2c_client = client;

	data->avdd = avdd;

	i2c_set_clientdata(client, data);

	data->enable = 0;	/* default mode is standard */
	data->ps_threshold = APDS9930_PS_DETECTION_THRESHOLD;
	data->ps_hysteresis_threshold = APDS9930_PS_HSYTERESIS_THRESHOLD;
	data->ps_detection = 0;	/* default to no detection */
	data->enable_als_sensor = 0;	/* default to 0 */
	data->enable_ps_sensor = 0;	/* default to 0 */
	data->als_poll_delay = 100;	/* default to 100ms */
	data->als_atime_index = APDS9930_ALS_RES_100MS;	/* 100ms ATIME */
	data->als_again_index = APDS9930_ALS_GAIN_8X;	/* 8x AGAIN */
	data->als_reduce = 0;	/* no ALS 6x reduction */
	data->als_prev_lux = 0;
	data->suspended = 0;
	data->enable_suspended_value = 0;	/* suspend_resume usage */

	mutex_init(&data->update_lock);
	INIT_DELAYED_WORK(&data->dwork, apds9930_work_handler);
	INIT_DELAYED_WORK(&data->als_dwork, apds9930_als_polling_work_handler);

	if (request_irq((client->irq), apds9930_interrupt, IRQF_TRIGGER_FALLING,
		APDS9930_DRV_NAME, (void *)client)) {
		pr_debug("%s Could not allocate irq resource !\n", __func__);
		goto exit_kfree;
	}

	pr_info("%s interrupt is hooked\n", __func__);

	/* Initialize the APDS993X chip */
	err = apds9930_init_client(client);
	if (err)
		goto exit_kfree;

	/* Register to Input Device */
	data->input_dev_als = input_allocate_device();
	if (!data->input_dev_als) {
		err = -ENOMEM;
		pr_debug("Failed to allocate input device als\n");
		goto exit_free_irq;
	}

	data->input_dev_ps = input_allocate_device();
	if (!data->input_dev_ps) {
		err = -ENOMEM;
		pr_debug("Failed to allocate input device ps\n");
		goto exit_free_dev_als;
	}

	data->input_dev_als->name = "APDS_light_sensor";
	data->input_dev_als->id.bustype = BUS_I2C;
	input_set_capability(data->input_dev_als, EV_ABS, ABS_MISC);
	__set_bit(EV_ABS, data->input_dev_als->evbit);
	__set_bit(ABS_PRESSURE, data->input_dev_als->absbit);
	input_set_abs_params(data->input_dev_als, ABS_LIGHT, 0, 30000, 0, 0);
	input_set_drvdata(data->input_dev_als, data);

	data->input_dev_ps->name = "APDS_proximity_sensor";
	data->input_dev_ps->id.bustype = BUS_I2C;
	input_set_capability(data->input_dev_ps, EV_ABS, ABS_MISC);
	__set_bit(EV_ABS, data->input_dev_ps->evbit);
	__set_bit(ABS_DISTANCE, data->input_dev_ps->absbit);
	input_set_abs_params(data->input_dev_ps, ABS_DISTANCE, 0, 10, 0, 0);
	input_set_drvdata(data->input_dev_ps, data);

	err = input_register_device(data->input_dev_als);
	if (err) {
		err = -ENOMEM;
		pr_debug("Unable to register input device als: %s\n",
		       data->input_dev_als->name);
		goto exit_free_dev_ps;
	}

	err = input_register_device(data->input_dev_ps);
	if (err) {
		err = -ENOMEM;
		pr_debug("Unable to register input device ps: %s\n",
		       data->input_dev_ps->name);
		goto exit_unregister_dev_als;
	}

	/* Register sysfs hooks */
	err = sysfs_create_group(&data->input_dev_als->dev.kobj, &apds9930_als_attr_group);
	if (err)
		goto exit_unregister_dev_als;

	err = sysfs_create_group(&data->input_dev_ps->dev.kobj, &apds9930_ps_attr_group);
	if (err)
		goto exit_unregister_dev_ps;

	/* Register for sensor ioctl */
	err = misc_register(&apds9930_ps_device);
	if (err) {
		pr_debug("Unalbe to register ps ioctl: %d", err);
		goto exit_remove_sysfs_group;
	}

	err = misc_register(&apds9930_als_device);
	if (err) {
		pr_debug("Unalbe to register als ioctl: %d", err);
		goto exit_unregister_ps_ioctl;
	}

	pr_debug("%s support ver. %s enabled\n", __func__, DRIVER_VERSION);
	regulator_disable(avdd);

	return 0;

exit_unregister_ps_ioctl:
	misc_deregister(&apds9930_ps_device);
exit_remove_sysfs_group:
	sysfs_remove_group(&data->input_dev_als->dev.kobj, &apds9930_als_attr_group);
	sysfs_remove_group(&data->input_dev_ps->dev.kobj, &apds9930_ps_attr_group);
exit_unregister_dev_ps:
	input_unregister_device(data->input_dev_ps);
exit_unregister_dev_als:
	input_unregister_device(data->input_dev_als);
exit_free_dev_ps:
exit_free_dev_als:
exit_free_irq:
	free_irq((client->irq), client);
exit_kfree:
	kfree(data);
exit:
	regulator_disable(avdd);
out:
	regulator_put(avdd);
	return err;
}

static int apds9930_remove(struct i2c_client *client)
{
	struct apds9930_data *data = i2c_get_clientdata(client);

	/* Power down the device */
	apds9930_set_enable(client, 0);

	misc_deregister(&apds9930_als_device);
	misc_deregister(&apds9930_ps_device);

	sysfs_remove_group(&data->input_dev_als->dev.kobj, &apds9930_als_attr_group);
	sysfs_remove_group(&data->input_dev_ps->dev.kobj, &apds9930_ps_attr_group);

	input_unregister_device(data->input_dev_ps);
	input_unregister_device(data->input_dev_als);

	free_irq((client->irq), client);

	kfree(data);

	return 0;
}

#ifdef CONFIG_PM

static int apds9930_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct apds9930_data *data = i2c_get_clientdata(client);

	pr_debug("apds9930_suspend\n");

	/* Do nothing as p-sensor is in active */
	if (!data->enable)
		return 0;

	data->suspended = 1;
	data->enable_suspended_value = data->enable;

	apds9930_set_enable(client, 0);
	apds9930_set_command(client, 2);

	cancel_delayed_work(&data->als_dwork);
	flush_delayed_work(&data->als_dwork);

	cancel_delayed_work(&data->dwork);
	flush_delayed_work(&data->dwork);

	flush_workqueue(apds_workqueue);

	disable_irq(client->irq);

	if (NULL != apds_workqueue) {
		destroy_workqueue(apds_workqueue);
		pr_debug(KERN_INFO "%s, Destroy workqueue\n", __func__);
		apds_workqueue = NULL;
	}

	regulator_disable(data->avdd);
	return 0;
}

static int apds9930_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct apds9930_data *data = i2c_get_clientdata(client);
	int err = 0;

	/* Do nothing as it was not suspended */
	pr_debug("apds9930_resume (enable=%d)\n", data->enable_suspended_value);

	if (!data->enable_suspended_value)
		return 0;

	if (apds_workqueue == NULL) {
		apds_workqueue = create_workqueue("proximity_als");
		if (NULL == apds_workqueue)
			return -ENOMEM;
	}

	if (!data->suspended)
		return 0;	/* if previously not suspended, leave it */
	if (regulator_enable(data->avdd)) {
		dev_err(&client->dev, "avago sensor avdd power supply enable failed\n");
		goto out;
	}

	enable_irq(client->irq);

	err = apds9930_set_enable(client, data->enable_suspended_value);

	if (err < 0) {
		pr_debug(KERN_INFO "%s, enable set Fail\n", __func__);
		return 0;
	}

	data->suspended = 0;

	apds9930_set_command(client, 2);	/* clear pending interrupt */
out:
	return 0;
}

#else

#define apds9930_suspend	NULL
#define apds9930_resume		NULL

#endif /* CONFIG_PM */

static const struct i2c_device_id apds9930_id[] = {
	{ "apds9930", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, apds9930_id);

static SIMPLE_DEV_PM_OPS(apds9930_pm_ops, apds9930_suspend, apds9930_resume);
static struct i2c_driver apds9930_driver = {
	.driver = {
		.name	= APDS9930_DRV_NAME,
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(inv_match_table),
#endif
		.pm = &apds9930_pm_ops,
	},
	.probe	= apds9930_probe,
	.remove	= apds9930_remove,
	.id_table = apds9930_id,
};

static int __init apds9930_init(void)
{
	apds_workqueue = create_workqueue("proximity_als");

	if (!apds_workqueue)
		return -ENOMEM;

	return i2c_add_driver(&apds9930_driver);
}

static void __exit apds9930_exit(void)
{
	if (apds_workqueue)
		destroy_workqueue(apds_workqueue);

	apds_workqueue = NULL;

	i2c_del_driver(&apds9930_driver);
}

MODULE_AUTHOR("Lee Kai Koon <kai-koon.lee@avagotech.com>");
MODULE_DESCRIPTION("APDS993X ambient light + proximity sensor driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRIVER_VERSION);

module_init(apds9930_init);
module_exit(apds9930_exit);

