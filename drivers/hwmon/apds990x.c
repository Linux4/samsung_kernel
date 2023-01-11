/*
 *  apds990x.c - Linux kernel modules for ambient light + proximity sensor
 *
 *  Copyright (C) 2010 Lee Kai Koon <kai-koon.lee@avagotech.com>
 *  Copyright (C) 2010 Avago Technologies
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
#include <linux/interrupt.h>
#include <linux/input.h>

#define APDS990x_DRV_NAME	"apds990x"
#define DRIVER_VERSION		"1.0.4"

#define APDS990x_PS_DETECTION_THRESHOLD		150
#define APDS990x_PS_HSYTERESIS_THRESHOLD	130

#define APDS990x_ALS_THRESHOLD_HSYTERESIS	20	/* 20 = 20% */

/* Change History
 *
 * 1.0.1	Functions apds990x_show_rev(), apds990x_show_id() and apds990x_show_status()
 *			have missing CMD_BYTE in the i2c_smbus_read_byte_data(). APDS-990x needs
 *			CMD_BYTE for i2c write/read byte transaction.
 *
 *
 * 1.0.2	Include PS switching threshold level when interrupt occurred
 *
 *
 * 1.0.3	Implemented ISR and delay_work, correct PS threshold storing
 *
 * 1.0.4	Added Input Report Event
 */

/*
 * Defines
 */

#define APDS990x_ENABLE_REG	0x00
#define APDS990x_ATIME_REG	0x01
#define APDS990x_PTIME_REG	0x02
#define APDS990x_WTIME_REG	0x03
#define APDS990x_AILTL_REG	0x04
#define APDS990x_AILTH_REG	0x05
#define APDS990x_AIHTL_REG	0x06
#define APDS990x_AIHTH_REG	0x07
#define APDS990x_PILTL_REG	0x08
#define APDS990x_PILTH_REG	0x09
#define APDS990x_PIHTL_REG	0x0A
#define APDS990x_PIHTH_REG	0x0B
#define APDS990x_PERS_REG	0x0C
#define APDS990x_CONFIG_REG	0x0D
#define APDS990x_PPCOUNT_REG	0x0E
#define APDS990x_CONTROL_REG	0x0F
#define APDS990x_REV_REG	0x11
#define APDS990x_ID_REG		0x12
#define APDS990x_STATUS_REG	0x13
#define APDS990x_CDATAL_REG	0x14
#define APDS990x_CDATAH_REG	0x15
#define APDS990x_IRDATAL_REG	0x16
#define APDS990x_IRDATAH_REG	0x17
#define APDS990x_PDATAL_REG	0x18
#define APDS990x_PDATAH_REG	0x19

#define CMD_BYTE	0x80
#define CMD_WORD	0xA0
#define CMD_SPECIAL	0xE0

#define CMD_CLR_PS_INT	0xE5
#define CMD_CLR_ALS_INT	0xE6
#define CMD_CLR_PS_ALS_INT	0xE7
#define DEVICE_ATTR2(_name, _mode, _show, _store) \
struct device_attribute dev_attr2_##_name = __ATTR(_name, _mode, _show, _store)

/*
 * Structs
 */

struct apds990x_data {
	struct i2c_client *client;
	struct mutex update_lock;
	struct delayed_work dwork;	/* for PS interrupt */
	struct delayed_work als_dwork;	/* for ALS polling */
	struct input_dev *input_dev_als;
	struct input_dev *input_dev_ps;

	unsigned int enable;
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
	atomic_t enable_ps_sensor;
	atomic_t enable_als_sensor;

	/* PS parameters */
	unsigned int ps_threshold;
	unsigned int ps_hysteresis_threshold;	/* always lower than ps_threshold */
	unsigned int ps_detection;	/* 0 = near-to-far; 1 = far-to-near */
	unsigned int ps_data;	/* to store PS data */

	/* ALS parameters */
	unsigned int als_threshold_l;	/* low threshold */
	unsigned int als_threshold_h;	/* high threshold */
	unsigned int als_data;	/* to store ALS data */

	unsigned int als_gain;	/* needed for Lux calculation */
	atomic_t als_poll_delay;	/* needed for light sensor polling : micro-second (us) */
	unsigned int als_atime;	/* storage for als integratiion time */

};

/*
 * Global data
 */
/*
 * Management functions
 */

static int apds990x_set_command(struct i2c_client *client, int command)
{
	struct apds990x_data *data = i2c_get_clientdata(client);
	int ret;
	int clearInt;

	if (command == 0)
		clearInt = CMD_CLR_PS_INT;
	else if (command == 1)
		clearInt = CMD_CLR_ALS_INT;
	else
		clearInt = CMD_CLR_PS_ALS_INT;

	mutex_lock(&data->update_lock);
	ret = i2c_smbus_write_byte(client, clearInt);
	mutex_unlock(&data->update_lock);

	return ret;
}

static int apds990x_set_enable(struct i2c_client *client, int enable)
{
	struct apds990x_data *data = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&data->update_lock);
	ret =
	    i2c_smbus_write_byte_data(client, CMD_BYTE | APDS990x_ENABLE_REG,
				      enable);
	mutex_unlock(&data->update_lock);

	data->enable = enable;

	return ret;
}

static int apds990x_set_atime(struct i2c_client *client, int atime)
{
	struct apds990x_data *data = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&data->update_lock);
	ret =
	    i2c_smbus_write_byte_data(client, CMD_BYTE | APDS990x_ATIME_REG,
				      atime);
	mutex_unlock(&data->update_lock);

	return ret;
}

static int apds990x_set_ptime(struct i2c_client *client, int ptime)
{
	struct apds990x_data *data = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&data->update_lock);
	ret =
	    i2c_smbus_write_byte_data(client, CMD_BYTE | APDS990x_PTIME_REG,
				      ptime);
	mutex_unlock(&data->update_lock);

	data->ptime = ptime;

	return ret;
}

static int apds990x_set_wtime(struct i2c_client *client, int wtime)
{
	struct apds990x_data *data = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&data->update_lock);
	ret =
	    i2c_smbus_write_byte_data(client, CMD_BYTE | APDS990x_WTIME_REG,
				      wtime);
	mutex_unlock(&data->update_lock);

	data->wtime = wtime;

	return ret;
}

static int apds990x_set_ailt(struct i2c_client *client, int threshold)
{
	struct apds990x_data *data = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&data->update_lock);
	ret =
	    i2c_smbus_write_word_data(client, CMD_WORD | APDS990x_AILTL_REG,
				      threshold);
	mutex_unlock(&data->update_lock);

	data->ailt = threshold;

	return ret;
}

static int apds990x_set_aiht(struct i2c_client *client, int threshold)
{
	struct apds990x_data *data = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&data->update_lock);
	ret =
	    i2c_smbus_write_word_data(client, CMD_WORD | APDS990x_AIHTL_REG,
				      threshold);
	mutex_unlock(&data->update_lock);

	data->aiht = threshold;

	return ret;
}

static int apds990x_set_pilt(struct i2c_client *client, int threshold)
{
	struct apds990x_data *data = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&data->update_lock);
	ret =
	    i2c_smbus_write_word_data(client, CMD_WORD | APDS990x_PILTL_REG,
				      threshold);
	mutex_unlock(&data->update_lock);

	data->pilt = threshold;

	return ret;
}

static int apds990x_set_piht(struct i2c_client *client, int threshold)
{
	struct apds990x_data *data = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&data->update_lock);
	ret =
	    i2c_smbus_write_word_data(client, CMD_WORD | APDS990x_PIHTL_REG,
				      threshold);
	mutex_unlock(&data->update_lock);

	data->piht = threshold;

	return ret;
}

static int apds990x_set_pers(struct i2c_client *client, int pers)
{
	struct apds990x_data *data = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&data->update_lock);
	ret =
	    i2c_smbus_write_byte_data(client, CMD_BYTE | APDS990x_PERS_REG,
				      pers);
	mutex_unlock(&data->update_lock);

	data->pers = pers;

	return ret;
}

static int apds990x_set_config(struct i2c_client *client, int config)
{
	struct apds990x_data *data = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&data->update_lock);
	ret =
	    i2c_smbus_write_byte_data(client, CMD_BYTE | APDS990x_CONFIG_REG,
				      config);
	mutex_unlock(&data->update_lock);

	data->config = config;

	return ret;
}

static int apds990x_set_ppcount(struct i2c_client *client, int ppcount)
{
	struct apds990x_data *data = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&data->update_lock);
	ret =
	    i2c_smbus_write_byte_data(client, CMD_BYTE | APDS990x_PPCOUNT_REG,
				      ppcount);
	mutex_unlock(&data->update_lock);

	data->ppcount = ppcount;

	return ret;
}

static int apds990x_set_control(struct i2c_client *client, int control)
{
	struct apds990x_data *data = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&data->update_lock);
	ret =
	    i2c_smbus_write_byte_data(client, CMD_BYTE | APDS990x_CONTROL_REG,
				      control);
	mutex_unlock(&data->update_lock);

	data->control = control;

	/* obtain ALS gain value */
	if ((data->control & 0x03) == 0x00)	/* 1X Gain */
		data->als_gain = 1;
	else if ((data->control & 0x03) == 0x01)	/* 8X Gain */
		data->als_gain = 8;
	else if ((data->control & 0x03) == 0x02)	/* 16X Gain */
		data->als_gain = 16;
	else			/* 120X Gain */
		data->als_gain = 120;

	return ret;
}

static int LuxCalculation(struct i2c_client *client, int cdata, int irdata)
{
#ifdef LUX_UNIT
	struct apds990x_data *data = i2c_get_clientdata(client);
	int luxValue = 0;
	int GA = 48;		/* 0.48 without glass window */
	int DF = 52;
#endif

	int IAC1 = 0;
	int IAC2 = 0;
	int IAC = 0;
	int COE_B = 223;	/* 2.23 without glass window */
	int COE_C = 70;		/* 0.70 without glass window */
	int COE_D = 142;	/* 1.42 without glass window */

	IAC1 = (cdata - (COE_B * irdata) / 100);	/* re-adjust COE_B to avoid 2 decimal point */
	IAC2 = ((COE_C * cdata) / 100 - (COE_D * irdata) / 100);	/*re-adjust COE_C and COE_D to void 2 decimal point */

	if (IAC1 > IAC2)
		IAC = IAC1;
	else if (IAC1 <= IAC2)
		IAC = IAC2;
	else
		IAC = 0;
#ifdef LUX_UNIT
	luxValue =
	    ((IAC * GA * DF) / 100) / (((272 * (256 - data->atime)) / 100) *
				       data->als_gain);
	return luxValue;
#endif
	return IAC;
}

static void apds990x_change_ps_threshold(struct i2c_client *client)
{
	struct apds990x_data *data = i2c_get_clientdata(client);

	data->ps_data =
	    i2c_smbus_read_word_data(client, CMD_WORD | APDS990x_PDATAL_REG);

	if ((data->ps_data > data->pilt) && (data->ps_data >= data->piht)) {
		/* far-to-near detected */
		data->ps_detection = 1;

		data->ps_data = 2;
		input_report_abs(data->input_dev_ps, ABS_DISTANCE, data->ps_data);	/* FAR-to-NEAR detection */
		input_sync(data->input_dev_ps);

		i2c_smbus_write_word_data(client, CMD_WORD | APDS990x_PILTL_REG,
					  data->ps_hysteresis_threshold);
		i2c_smbus_write_word_data(client, CMD_WORD | APDS990x_PIHTL_REG,
					  1023);

		data->pilt = data->ps_hysteresis_threshold;
		data->piht = 1023;
		pr_debug("far-to-near detected\n");
	} else if ((data->ps_data <= data->pilt)
		   && (data->ps_data < data->piht)) {
		/* near-to-far detected */
		data->ps_detection = 0;

		data->ps_data = 10;
		input_report_abs(data->input_dev_ps, ABS_DISTANCE, data->ps_data);	/* NEAR-to-FAR detection */
		input_sync(data->input_dev_ps);

		i2c_smbus_write_word_data(client, CMD_WORD | APDS990x_PILTL_REG,
					  0);
		i2c_smbus_write_word_data(client, CMD_WORD | APDS990x_PIHTL_REG,
					  data->ps_threshold);

		data->pilt = 0;
		data->piht = data->ps_threshold;
		pr_debug("near-to-far detected\n");
	}
	pr_debug
	    ("high threshhold change to %d, the low threshhold change to %d",
	     data->pilt, data->piht);
}

static void apds990x_change_als_threshold(struct i2c_client *client)
{
	struct apds990x_data *data = i2c_get_clientdata(client);
	int cdata, irdata;
	int luxValue = 0;

	cdata =
	    i2c_smbus_read_word_data(client, CMD_WORD | APDS990x_CDATAL_REG);
	irdata =
	    i2c_smbus_read_word_data(client, CMD_WORD | APDS990x_IRDATAL_REG);

	luxValue = LuxCalculation(client, cdata, irdata);

	luxValue = luxValue > 0 ? luxValue : 0;
	luxValue = luxValue < 10000 ? luxValue : 10000;

	/* check PS under sunlight */
	if ((data->ps_detection == 1)
	    && (cdata > (75 * (1024 * (256 - data->als_atime))) / 100))
		/* PS was previously in far-to-near condition */
	{
		/* need to inform input event as there will be no interrupt from the PS */
		data->ps_data = 10;
		input_report_abs(data->input_dev_ps, ABS_DISTANCE, data->ps_data);	/* NEAR-to-FAR detection */
		input_sync(data->input_dev_ps);

		i2c_smbus_write_word_data(client, CMD_WORD | APDS990x_PILTL_REG,
					  0);
		i2c_smbus_write_word_data(client, CMD_WORD | APDS990x_PIHTL_REG,
					  data->ps_threshold);

		data->pilt = 0;
		data->piht = data->ps_threshold;

		data->ps_detection = 0;	/* near-to-far detected */

		pr_debug("apds_990x_proximity_handler = FAR\n");
	}

	input_report_abs(data->input_dev_als, ABS_PRESSURE, luxValue);	/*report the lux level */
	input_sync(data->input_dev_als);

	data->als_data = luxValue;

	data->als_threshold_l =
	    (cdata * (100 - APDS990x_ALS_THRESHOLD_HSYTERESIS)) / 100;
	data->als_threshold_h =
	    (cdata * (100 + APDS990x_ALS_THRESHOLD_HSYTERESIS)) / 100;

	if (data->als_threshold_h >= 65535)
		data->als_threshold_h = 65535;

	i2c_smbus_write_word_data(client, CMD_WORD | APDS990x_AILTL_REG,
				  data->als_threshold_l);

	i2c_smbus_write_word_data(client, CMD_WORD | APDS990x_AIHTL_REG,
				  data->als_threshold_h);
}

static void apds990x_reschedule_work(struct apds990x_data *data,
				     unsigned long delay)
{
	unsigned long flags;

	spin_lock_irqsave(&data->update_lock.wait_lock, flags);

	/*
	 * If work is already scheduled then subsequent schedules will not
	 * change the scheduled time that's why we have to cancel it first.
	 */
	cancel_delayed_work(&data->dwork);
	schedule_delayed_work(&data->dwork, delay);

	spin_unlock_irqrestore(&data->update_lock.wait_lock, flags);
}

/* ALS polling routine */
static void apds990x_als_polling_work_handler(struct work_struct *work)
{
	struct apds990x_data *data =
	    container_of(work, struct apds990x_data, als_dwork.work);
	struct i2c_client *client = data->client;
	int cdata, irdata, pdata;
	int luxValue = 0;

	cdata =
	    i2c_smbus_read_word_data(client, CMD_WORD | APDS990x_CDATAL_REG);
	irdata =
	    i2c_smbus_read_word_data(client, CMD_WORD | APDS990x_IRDATAL_REG);
	pdata =
	    i2c_smbus_read_word_data(client, CMD_WORD | APDS990x_PDATAL_REG);

	luxValue = LuxCalculation(client, cdata, irdata);

	luxValue = luxValue > 0 ? luxValue : 0;
	luxValue = luxValue < 10000 ? luxValue : 10000;

	data->als_data = luxValue;
	pr_debug("%s: lux = %d cdata = %x  irdata = %x pdata = %x\
	   als_data = %x\n", __func__, luxValue, cdata, irdata, pdata, data->als_data);
	/* check PS under sunlight */
	if ((data->ps_detection == 1)
	    && (cdata > (75 * (1024 * (256 - data->als_atime))) / 100))
		/*PS was previously in far-to-near condition */
	{
		/* need to inform input event as there will be no interrupt from the PS */
		input_report_abs(data->input_dev_ps, ABS_DISTANCE, 0);	/* NEAR-to-FAR detection */
		input_sync(data->input_dev_ps);

		i2c_smbus_write_word_data(client, CMD_WORD | APDS990x_PILTL_REG,
					  0);
		i2c_smbus_write_word_data(client, CMD_WORD | APDS990x_PIHTL_REG,
					  data->ps_threshold);

		data->pilt = 0;
		data->piht = data->ps_threshold;

		data->ps_detection = 0;	/* near-to-far detected */
		data->ps_data = 10;
		pr_debug("apds_990x_proximity_handler = FAR\n");
	}

	input_report_abs(data->input_dev_als, ABS_PRESSURE, luxValue);	/*report the lux level */
	input_sync(data->input_dev_als);

	schedule_delayed_work(&data->als_dwork, msecs_to_jiffies(atomic_read(&data->als_poll_delay)));	/* restart timer */
}

/* PS interrupt routine */
static void apds990x_work_handler(struct work_struct *work)
{
	struct apds990x_data *data =
	    container_of(work, struct apds990x_data, dwork.work);
	struct i2c_client *client = data->client;
	int status;
	int cdata;

	status =
	    i2c_smbus_read_byte_data(client, CMD_BYTE | APDS990x_STATUS_REG);

	i2c_smbus_write_byte_data(client, CMD_BYTE | APDS990x_ENABLE_REG, 1);	/* disable 990x's ADC first */

	pr_debug("status = %x\n", status);
	if ((status & data->enable & 0x30) == 0x30) {
		/* both PS and ALS are interrupted */
		apds990x_change_als_threshold(client);

		cdata =
		    i2c_smbus_read_word_data(client,
					     CMD_WORD | APDS990x_CDATAL_REG);
		if (cdata < (75 * (1024 * (256 - data->als_atime))) / 100)
			apds990x_change_ps_threshold(client);
		else {
			if (data->ps_detection == 1)
				apds990x_change_ps_threshold(client);
			else
				pr_info
				    ("Triggered by background ambient noise\n");

		}

		apds990x_set_command(client, 2);	/* 2 = CMD_CLR_PS_ALS_INT */
	} else if ((status & data->enable & 0x20) == 0x20) {
		/* only PS is interrupted */

		/* check if this is triggered by background ambient noise */
		cdata =
		    i2c_smbus_read_word_data(client,
					     CMD_WORD | APDS990x_CDATAL_REG);
		if (cdata < (75 * (1024 * (256 - data->als_atime))) / 100)
			apds990x_change_ps_threshold(client);
		else {
			if (data->ps_detection == 1)
				apds990x_change_ps_threshold(client);
			else
				pr_info
				    ("Triggered by background ambient noise\n");

		}

		apds990x_set_command(client, 0);	/* 0 = CMD_CLR_PS_INT */
	} else if ((status & data->enable & 0x10) == 0x10) {
		/* only ALS is interrupted */
		apds990x_change_als_threshold(client);

		apds990x_set_command(client, 1);	/* 1 = CMD_CLR_ALS_INT */
	}

	i2c_smbus_write_byte_data(client, CMD_BYTE | APDS990x_ENABLE_REG,
				  data->enable);
}

/* assume this is ISR */
static irqreturn_t apds990x_interrupt(int vec, void *info)
{
	struct i2c_client *client = (struct i2c_client *)info;
	struct apds990x_data *data = i2c_get_clientdata(client);

	pr_debug("==> apds990x_interrupt\n");
	apds990x_reschedule_work(data, 0);

	return IRQ_HANDLED;
}

/*
 * SysFS support
 */

static ssize_t apds990x_show_enable_ps_sensor(struct device *dev,
					      struct device_attribute *attr,
					      char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct apds990x_data *data = input_get_drvdata(input);

	return sprintf(buf, "%d\n", atomic_read(&data->enable_ps_sensor));
}

static ssize_t apds990x_store_enable_ps_sensor(struct device *dev,
					       struct device_attribute *attr,
					       const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct apds990x_data *data = input_get_drvdata(input);
	struct i2c_client *client = data->client;
	unsigned long val = simple_strtoul(buf, NULL, 10);
	unsigned long flags;

	pr_info("%s: enable ps senosr ( %ld)\n", __func__, val);

	if ((val != 0) && (val != 1)) {
		pr_err("%s:store unvalid value=%ld\n", __func__, val);
		return count;
	}

	if (val == 1) {
		/* turn on p sensor */
		if (atomic_read(&data->enable_ps_sensor) == 0) {

			int cdata;
			apds990x_set_enable(client, 0);	/* Power Off */
			apds990x_set_atime(client, data->als_atime);	/* 27.2ms */
			apds990x_set_ptime(client, 0xff);	/* 2.72ms */

			apds990x_set_ppcount(client, 8);	/* 8-pulse */
			apds990x_set_control(client, 0xE0);	/* 12.5mA, IR-diode, 1X PGAIN, 1X AGAIN */

			apds990x_set_pilt(client, 0);	/*init threshold for proximity */
			apds990x_set_piht(client,
					  APDS990x_PS_DETECTION_THRESHOLD);

			data->ps_threshold = APDS990x_PS_DETECTION_THRESHOLD;
			data->ps_hysteresis_threshold =
			    APDS990x_PS_HSYTERESIS_THRESHOLD;

			apds990x_set_ailt(client, 0);
			apds990x_set_aiht(client, 0xffff);

			apds990x_set_pers(client, 0x13);	/* 3 persistence */

			if (atomic_read(&data->enable_als_sensor) == 0) {

				/* we need this polling timer routine for sunlight canellation */
				spin_lock_irqsave(&data->update_lock.wait_lock,
						  flags);

				/*
				 * If work is already scheduled then subsequent schedules will not
				 * change the scheduled time that's why we have to cancel it first.
				 */
				cancel_delayed_work(&data->als_dwork);
				schedule_delayed_work(&data->als_dwork, msecs_to_jiffies(atomic_read(&data->als_poll_delay)));	/* 100ms */
				spin_unlock_irqrestore(&data->update_lock.
						       wait_lock, flags);
			}
			apds990x_set_enable(client, 0x27);	/* only enable PS interrupt */
			cdata =
			    i2c_smbus_read_word_data(client,
						     CMD_WORD |
						     APDS990x_CDATAL_REG);
			if (cdata <
			    (75 * (1024 * (256 - data->als_atime))) / 100)
				apds990x_change_ps_threshold(client);
			else {
				if (data->ps_detection == 1)
					apds990x_change_ps_threshold(client);
				else
					pr_info
					    ("Triggered by background ambient noise\n");
			}

		}
	} else {
		/* turn off p sensor - kk 25 Apr 2011 we can't turn off the entire sensor, the light sensor may be needed by HAL */
		if (atomic_read(&data->enable_ps_sensor)) {
			if (atomic_read(&data->enable_als_sensor)) {
				/*reconfigute light sensor setting */
				apds990x_set_enable(client, 0);	/* Power Off */

				apds990x_set_atime(client, data->als_atime);	/* previous als poll delay */

				apds990x_set_ailt(client, 0);
				apds990x_set_aiht(client, 0xffff);

				apds990x_set_control(client, 0xE0);	/* 12.5mA, IR-diode, 1X PGAIN, 1X AGAIN */
				apds990x_set_pers(client, 0x13);	/* 3 persistence */

				apds990x_set_enable(client, 0x3);	/* only enable light sensor */

				spin_lock_irqsave(&data->update_lock.wait_lock,
						  flags);

				/*
				 * If work is already scheduled then subsequent schedules will not
				 * change the scheduled time that's why we have to cancel it first.
				 */
				cancel_delayed_work(&data->als_dwork);
				schedule_delayed_work(&data->als_dwork, msecs_to_jiffies(atomic_read(&data->als_poll_delay)));	/* 100ms */
				spin_unlock_irqrestore(&data->
						       update_lock.wait_lock,
						       flags);

			} else {
				apds990x_set_enable(client, 0);

				spin_lock_irqsave(&data->update_lock.wait_lock,
						  flags);

				/*
				 * If work is already scheduled then subsequent schedules will not
				 * change the scheduled time that's why we have to cancel it first.
				 */
				cancel_delayed_work(&data->als_dwork);

				spin_unlock_irqrestore(&data->
						       update_lock.wait_lock,
						       flags);
			}
		}
	}
	atomic_set(&data->enable_ps_sensor, val);
	return count;
}

static ssize_t apds990x_show_enable_als_sensor(struct device *dev,
					       struct device_attribute *attr,
					       char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct apds990x_data *data = input_get_drvdata(input);
	return sprintf(buf, "%d\n", atomic_read(&data->enable_als_sensor));
}

static ssize_t apds990x_store_enable_als_sensor(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct apds990x_data *data = input_get_drvdata(input);
	struct i2c_client *client = data->client;
	unsigned long val = simple_strtoul(buf, NULL, 10);
	unsigned long flags;

	pr_info("%s: enable als sensor ( %ld)\n", __func__, val);

	if ((val != 0) && (val != 1)) {
		pr_err("%s:store unvalid value=%ld\n", __func__, val);
		return count;
	}

	if (val == 1) {
		/* turn on light  sensor */
		if (atomic_read(&data->enable_als_sensor) == 0) {
			if (atomic_read(&data->enable_ps_sensor) == 0) {
				apds990x_set_enable(client, 0);	/* Power Off */

				apds990x_set_atime(client, data->als_atime);	/* 100.64ms */

				apds990x_set_ailt(client, 0);
				apds990x_set_aiht(client, 0xffff);

				apds990x_set_control(client, 0xE0);	/* 12.5mA, IR-diode, 1X PGAIN, 1X AGAIN */
				apds990x_set_pers(client, 0x13);	/* 3 persistence */
				apds990x_set_enable(client, 0x3);	/* only enable light sensor */
				spin_lock_irqsave(&data->update_lock.wait_lock,
						  flags);

				/*
				 * If work is already scheduled then subsequent schedules will not
				 * change the scheduled time that's why we have to cancel it first.
				 */
				cancel_delayed_work(&data->als_dwork);
				schedule_delayed_work(&data->als_dwork,
						      msecs_to_jiffies
						      (atomic_read
						       (&data->als_poll_delay)));

				spin_unlock_irqrestore(&data->
						       update_lock.wait_lock,
						       flags);
			}
		}
	} else {
		/* turn off light sensor */
		/* what if the p sensor is active? */
		if (atomic_read(&data->enable_als_sensor)) {
			if (atomic_read(&data->enable_ps_sensor)) {
				pr_info
				    ("PS sensor cannot work without ALS sensor");
				val = 1;
			} else {
				apds990x_set_enable(client, 0);
				spin_lock_irqsave(&data->update_lock.wait_lock,
						  flags);
				/*
				 * If work is already scheduled then subsequent schedules will not
				 * change the scheduled time that's why we have to cancel it first.
				 */
				cancel_delayed_work(&data->als_dwork);
				spin_unlock_irqrestore(&data->
						       update_lock.wait_lock,
						       flags);
			}
		}
	}
	atomic_set(&data->enable_als_sensor, val);
	return count;
}

static ssize_t apds990x_show_als_poll_delay(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct apds990x_data *data = input_get_drvdata(input);

	return sprintf(buf, "%d\n", (atomic_read(&data->als_poll_delay)));	/*return in micro-second */
}

static ssize_t apds990x_store_als_poll_delay(struct device *dev,
					     struct device_attribute *attr,
					     const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct apds990x_data *data = input_get_drvdata(input);
	unsigned long val = simple_strtoul(buf, NULL, 10);
	unsigned long flags;
#ifdef LUX_UNIT
	int poll_delay = 0;
#endif
	atomic_set(&data->als_poll_delay, val);	/*convert us => ms */
	val = val * USEC_PER_MSEC;
	if (val < 5000)
		val = 5000;	/*minimum 5ms */

	/*poll_delay and data->als_atime just the ATIME */
#ifdef LUX_UNIT
	poll_delay = 256 - (val / 2720);	/* the minimum is 2.72ms = 2720 us, maximum is 696.32ms */
	if (poll_delay >= 256)
		data->als_atime = 255;
	else if (poll_delay < 0)
		data->als_atime = 0;
	else
		data->als_atime = poll_delay;

	ret = apds990x_set_atime(client, data->als_atime);

	if (ret < 0)
		return ret;
#endif
	if (atomic_read(&data->enable_als_sensor)) {
		/* we need this polling timer routine for sunlight canellation */
		spin_lock_irqsave(&data->update_lock.wait_lock, flags);

		/*
		 * If work is already scheduled then subsequent schedules will not
		 * change the scheduled time that's why we have to cancel it first.
		 */
		cancel_delayed_work(&data->als_dwork);
		schedule_delayed_work(&data->als_dwork, msecs_to_jiffies(atomic_read(&data->als_poll_delay)));	/* 100ms */
		spin_unlock_irqrestore(&data->update_lock.wait_lock, flags);
	}
	return count;
}

static ssize_t apds990x_show_als_data(struct device *dev,
				      struct device_attribute *attr, char *buf)
{

	int val;
	struct input_dev *input = to_input_dev(dev);
	struct apds990x_data *data = input_get_drvdata(input);

	val = data->als_data;
	return sprintf(buf, "%d\n", val);

}

static ssize_t apds990x_show_ps_poll_delay(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	return 0;
}

static ssize_t apds990x_store_ps_poll_delay(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t count)
{
	return 0;
}

static ssize_t apds990x_show_ps_data(struct device *dev,
				     struct device_attribute *attr, char *buf)
{

	int val;
	struct input_dev *input = to_input_dev(dev);
	struct apds990x_data *data = input_get_drvdata(input);

	val = data->ps_data;
	return sprintf(buf, "%d\n", val);

}

static ssize_t wake_set(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	return count;
}

static ssize_t show_status(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	/* TODO */
	return 0;
}

static DEVICE_ATTR(active, S_IRUGO | S_IWUGO, apds990x_show_enable_als_sensor,
		   apds990x_store_enable_als_sensor);
static DEVICE_ATTR(interval, S_IRUGO | S_IWUGO, apds990x_show_als_poll_delay,
		   apds990x_store_als_poll_delay);
static DEVICE_ATTR(wake, S_IWUGO, NULL, wake_set);
static DEVICE_ATTR(data, S_IRUGO, apds990x_show_als_data, NULL);
static DEVICE_ATTR(status, S_IRUGO, show_status, NULL);

static DEVICE_ATTR2(active, S_IRUGO | S_IWUGO, apds990x_show_enable_ps_sensor,
		    apds990x_store_enable_ps_sensor);
static DEVICE_ATTR2(interval, S_IRUGO | S_IWUGO, apds990x_show_ps_poll_delay,
		    apds990x_store_ps_poll_delay);
static DEVICE_ATTR2(wake, S_IWUGO, NULL, wake_set);
static DEVICE_ATTR2(data, S_IRUGO, apds990x_show_ps_data, NULL);
static DEVICE_ATTR2(status, S_IRUGO, show_status, NULL);

static struct attribute *apds990x_als_attributes[] = {
	&dev_attr_active.attr,
	&dev_attr_interval.attr,
	&dev_attr_wake.attr,
	&dev_attr_data.attr,
	&dev_attr_status.attr,
	NULL
};

static const struct attribute_group apds990x_als_attr_group = {
	.attrs = apds990x_als_attributes,
};

static struct attribute *apds990x_ps_attributes[] = {
	&dev_attr2_active.attr,
	&dev_attr2_interval.attr,
	&dev_attr2_wake.attr,
	&dev_attr2_data.attr,
	&dev_attr2_status.attr,
	NULL
};

static const struct attribute_group apds990x_ps_attr_group = {
	.attrs = apds990x_ps_attributes,
};

/*
 * Initialization function
 */

static int apds990x_init_client(struct i2c_client *client)
{
	struct apds990x_data *data = i2c_get_clientdata(client);
	int err;
	int id;

	err = apds990x_set_enable(client, 0);

	if (err < 0)
		return err;

	id = i2c_smbus_read_byte_data(client, CMD_BYTE | APDS990x_ID_REG);
	if (id == 0x20)
		pr_info("APDS-9901\n");
	else if (id == 0x29)
		pr_info("APDS-990x\n");
	else {
		pr_err("Neither APDS-9901 nor APDS-9901\n");
		return -EIO;
	}

	apds990x_set_atime(client, data->als_atime);	/*100.64ms ALS integration time */
	apds990x_set_ptime(client, 0xFF);	/*2.72ms Prox integration time */
	apds990x_set_wtime(client, 0xFF);	/*2.72ms Wait time */

	apds990x_set_ppcount(client, 0x08);	/*8-Pulse for proximity */
	apds990x_set_config(client, 0);	/*no long wait */
	apds990x_set_control(client, 0xE0);	/*12.5mA, IR-diode, 1X PGAIN, 1X AGAIN */

	apds990x_set_pilt(client, 0);	/* init threshold for proximity */
	apds990x_set_piht(client, APDS990x_PS_DETECTION_THRESHOLD);

	data->ps_threshold = APDS990x_PS_DETECTION_THRESHOLD;
	data->ps_hysteresis_threshold = APDS990x_PS_HSYTERESIS_THRESHOLD;

	apds990x_set_ailt(client, 0);	/*init threshold for als */
	apds990x_set_aiht(client, 0xFFFF);

	apds990x_set_pers(client, 0x12);	/*2 consecutive Interrupt persistence */

	/* sensor is in disabled mode but all the configurations are preset */

	return 0;
}

/*
 * I2C init/probing/exit functions
 */

static struct i2c_driver apds990x_driver;
static int apds990x_probe(struct i2c_client *client,
				    const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct apds990x_data *data;
	int err = 0;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE)) {
		err = -EIO;
		goto exit;
	}

	data = kzalloc(sizeof(struct apds990x_data), GFP_KERNEL);
	if (!data) {
		err = -ENOMEM;
		goto exit;
	}
	data->client = client;
	i2c_set_clientdata(client, data);
	data->enable = 0;	/* default mode is standard */
	data->ps_threshold = 0;
	data->ps_hysteresis_threshold = 0;
	data->ps_detection = 0;	/* default to no detection */
	atomic_set(&data->enable_als_sensor, 0);	/*default to 0 */
	atomic_set(&data->enable_ps_sensor, 0);	/*default to 0 */
	atomic_set(&data->als_poll_delay, 100);	/* default to 100ms */
	data->als_atime = 0x50;
	pr_debug("enable = %x\n", data->enable);

	mutex_init(&data->update_lock);

	INIT_DELAYED_WORK(&data->dwork, apds990x_work_handler);
	INIT_DELAYED_WORK(&data->als_dwork, apds990x_als_polling_work_handler);

	if (request_irq
	    ((data->client->irq), apds990x_interrupt,
	     IRQF_DISABLED | IRQ_TYPE_EDGE_FALLING, APDS990x_DRV_NAME,
	     (void *)client)) {
		pr_err("%s Could not allocate irq resource!\n", __func__);
		goto exit_kfree;
	}

	pr_info("%s interrupt is hooked\n", __func__);

	/* Initialize the APDS990x chip */
	err = apds990x_init_client(client);
	if (err)
		goto exit_free_irq;

	/* Register to Input Device */
	data->input_dev_als = input_allocate_device();
	if (!data->input_dev_als) {
		err = -ENOMEM;
		pr_err("Failed to allocate input device als\n");
		goto exit_free_irq;
	}

	data->input_dev_ps = input_allocate_device();
	if (!data->input_dev_ps) {
		err = -ENOMEM;
		pr_err("Failed to allocate input device ps\n");
		goto exit_free_dev_als;
	}
	data->input_dev_als->name = "APDS_light_sensor";
	data->input_dev_als->id.bustype = BUS_I2C;
	input_set_capability(data->input_dev_als, EV_ABS, ABS_MISC);
	__set_bit(EV_ABS, data->input_dev_als->evbit);
	__set_bit(ABS_PRESSURE, data->input_dev_als->absbit);
	input_set_abs_params(data->input_dev_als, ABS_PRESSURE, 0, 65535, 0, 0);
	input_set_drvdata(data->input_dev_als, data);

	data->input_dev_ps->name = "APDS_proximity_sensor";
	data->input_dev_ps->id.bustype = BUS_I2C;
	input_set_capability(data->input_dev_ps, EV_ABS, ABS_MISC);
	__set_bit(EV_ABS, data->input_dev_ps->evbit);
	__set_bit(ABS_DISTANCE, data->input_dev_ps->absbit);
	input_set_abs_params(data->input_dev_ps, ABS_DISTANCE, 0, 1, 0, 0);
	input_set_drvdata(data->input_dev_ps, data);

	err = input_register_device(data->input_dev_als);
	if (err) {
		err = -ENOMEM;
		pr_err("Unable to register input device als: %s\n",
		       data->input_dev_als->name);
		goto exit_free_dev_ps;
	}

	err = input_register_device(data->input_dev_ps);
	if (err) {
		err = -ENOMEM;
		pr_err("Unable to register input device ps: %s\n",
		       data->input_dev_ps->name);
		goto exit_unregister_dev_als;
	}

	/* Register sysfs hooks */
	err =
	    sysfs_create_group(&data->input_dev_als->dev.kobj,
			       &apds990x_als_attr_group);
	if (err)
		goto exit_unregister_dev_ps;
	err =
	    sysfs_create_group(&data->input_dev_ps->dev.kobj,
			       &apds990x_ps_attr_group);
	if (err)
		goto exit_unregister_dev_ps;
	pr_info("%s support ver. %s enabled\n", __func__, DRIVER_VERSION);

	return 0;

exit_unregister_dev_ps:
	input_unregister_device(data->input_dev_als);
exit_unregister_dev_als:
	input_unregister_device(data->input_dev_ps);
exit_free_dev_ps:
	input_free_device(data->input_dev_ps);
exit_free_dev_als:
	input_free_device(data->input_dev_als);
exit_free_irq:
	free_irq((data->client->irq), client);
exit_kfree:
	kfree(data);
exit:
	return err;
}

static int apds990x_remove(struct i2c_client *client)
{
	struct apds990x_data *data = i2c_get_clientdata(client);

	input_unregister_device(data->input_dev_als);
	input_unregister_device(data->input_dev_ps);

	input_free_device(data->input_dev_als);
	input_free_device(data->input_dev_ps);

	free_irq((data->client->irq), client);

	sysfs_remove_group(&data->input_dev_als->dev.kobj,
			   &apds990x_als_attr_group);
	sysfs_remove_group(&data->input_dev_ps->dev.kobj,
			   &apds990x_ps_attr_group);

	/* Power down the device */
	apds990x_set_enable(client, 0);

	kfree(data);

	return 0;
}

#ifdef CONFIG_PM

static int apds990x_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct apds990x_data *data = i2c_get_clientdata(client);
	unsigned long flags;
	int ret;
	ret = apds990x_set_enable(client, 0);
	pr_info("enter %s\n", __func__);
	if (atomic_read(&data->enable_als_sensor)
	    || atomic_read(&data->enable_ps_sensor)) {
		spin_lock_irqsave(&data->update_lock.wait_lock, flags);
		cancel_delayed_work(&data->als_dwork);
		spin_unlock_irqrestore(&data->update_lock.wait_lock, flags);
	}
	return ret;
}

static int apds990x_resume(struct i2c_client *client)
{
	/*Just restore the sensor state, not enable sensor */
	struct apds990x_data *data = i2c_get_clientdata(client);
	unsigned long flags;
	int ret = 0;

	pr_info("enter %s \n", __func__);

	/*Check PS enable first */
	if (atomic_read(&data->enable_ps_sensor)) {
		/*PS enable first */
		apds990x_set_enable(client, 0);	/* Power Off */
		apds990x_set_ptime(client, 0xff);	/* 2.72ms */
		apds990x_set_ppcount(client, 8);	/* 8-pulse */
		apds990x_set_pilt(client, 0);	/* init threshold for proximity */
		apds990x_set_piht(client, APDS990x_PS_DETECTION_THRESHOLD);
		data->ps_threshold = APDS990x_PS_DETECTION_THRESHOLD;
		data->ps_hysteresis_threshold =
		    APDS990x_PS_HSYTERESIS_THRESHOLD;
		/*AS enable must */
		apds990x_set_atime(client, data->als_atime);	/* 27.2ms */
		apds990x_set_ailt(client, 0);
		apds990x_set_aiht(client, 0xffff);
		/*common part */
		apds990x_set_control(client, 0xE0);	/* 12.5mA, IR-diode, 1X PGAIN, 1X AGAIN */
		apds990x_set_pers(client, 0x13);	/* 3 persistence */
		ret = apds990x_set_enable(client, 0x27);

		pr_debug("%s to resume ps_sensor", __func__);
	} else {
		/*no need to enable PS */
		apds990x_set_enable(client, 0);	/* Power Off */
		if (atomic_read(&data->enable_als_sensor)) {
			/*AS enable */
			apds990x_set_atime(client, data->als_atime);	/* 100.64ms */
			apds990x_set_ailt(client, 0);
			apds990x_set_aiht(client, 0xffff);
			/*common part */
			apds990x_set_control(client, 0xE0);	/* 12.5mA, IR-diode, 1X PGAIN, 1X AGAIN */
			apds990x_set_pers(client, 0x13);	/* 3 persistence */
			ret = apds990x_set_enable(client, 0x03);

			pr_debug("%s to just resume als_sensor", __func__);
		}
	}
	/*after enable the device, Als schedule here */
	if (atomic_read(&data->enable_als_sensor)
	    || atomic_read(&data->enable_ps_sensor)) {
		spin_lock_irqsave(&data->update_lock.wait_lock, flags);
		schedule_delayed_work(&data->als_dwork,
				      msecs_to_jiffies(atomic_read
						       (&data->
							als_poll_delay)));
		spin_unlock_irqrestore(&data->update_lock.wait_lock, flags);

		pr_debug("%s to resume als queue work", __func__);

	}
	return ret;
}
#else

#define apds990x_suspend NULL
#define apds990x_resume NULL

#endif /* CONFIG_PM */

static struct of_device_id apds990x_dt_ids[] = {
	{ .compatible = "avago,apds990x", },
	{}
};

static const struct i2c_device_id apds990x_id[] = {
	{"apds990x", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, apds990x_id);

static struct i2c_driver apds990x_driver = {
	.driver = {
		   .name = APDS990x_DRV_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(apds990x_dt_ids),
		   },
	.class = I2C_CLASS_HWMON,
	.suspend = apds990x_suspend,
	.resume = apds990x_resume,
	.probe = apds990x_probe,
	.remove = apds990x_remove,
	.id_table = apds990x_id,
};

static int __init apds990x_init(void)
{
	return i2c_add_driver(&apds990x_driver);
}

static void __exit apds990x_exit(void)
{
	i2c_del_driver(&apds990x_driver);
}

MODULE_AUTHOR("Lee Kai Koon <kai-koon.lee@avagotech.com>");
MODULE_DESCRIPTION("APDS990x ambient light + proximity sensor driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRIVER_VERSION);

module_init(apds990x_init);
module_exit(apds990x_exit);
