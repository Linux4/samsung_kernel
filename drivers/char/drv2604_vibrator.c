/*
 *    Vibrator driver for TI DRV2604 vibrator driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/mfd/88pm80x.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/regulator/machine.h>

#include "../staging/android/timed_output.h"

#define DRV2604_NAME "drv2604-vibrator"

/* prints wrapper for better debug */
#define VIB_ERR_MSG(fmt, ...) \
	pr_err(DRV2604_NAME ": %s(%d): " fmt, __func__, __LINE__ , ##__VA_ARGS__)

/* DRV2604 registers */
#define DRV2604_STATUS				0x00
#define DRV2604_MODE				0x01
#define DRV2604_RTP				0x02
#define DRV2604_WAVEFORMSEQ1			0x04
#define DRV2604_WAVEFORMSEQ2			0x05
#define DRV2604_GO				0x0C
#define DRV2604_ODT				0x0D
#define DRV2604_SPT				0x0E
#define DRV2604_SNT				0x0F
#define DRV2604_BRT				0x10
#define DRV2604_RATED_VOLTAGE			0x16
#define DRV2604_OVERDRIVE_CLAMP_VOLTAGE	0x17
#define DRV2604_FBCTRL				0x1A
#define DRV2604_RAM_ADDR_UP_BYTE		0xFD
#define DRV2604_RAM_ADDR_LOW_BYTE		0xFE
#define DRV2604_RAM_DATA			0xFF

/*
 * Registers Bits definitions
 */

/* STATUS */
#define STATUS_DIAG_RESULT	BIT(3)

/* MODE */
#define MODE_TYPE_INT_TRIG		0x0
#define MODE_TYPE_EXT_LEVEL_TRIG	0x2
#define MODE_TYPE_AUTOCAL		0x7
#define MODE_SET(x)			((x) & 0x7)
#define MODE_STANDBY			BIT(6)
#define MODE_INIT_MASK			0x0

/* GO */
#define GO_CMD		(0x1)
#define STOP_CMD	(0x0)

/* RATED_VOLTAGE */
#define LRA_RV_3p0	0x7D
#define ERM_RV_3p0	0x8D
#define ERM_RV_2p6	0x79

/* OVERDRIVE_CLAMP_VOLTAGE */
#define LRA_ODV_3p6	0xA4
#define ERM_ODV_3p6	0xBA

/* FBCTRL */
#define FBCTRL_BEMF_GAIN(x)		((x) & 0x3)
#define FBCTRL_LOOP_RESPONSE(x)	(((x) & 0x3) << 2)
#define FBCTRL_BRAKE_FACTOR(x)		(((x) & 0x7) << 4)
#define FBCTRL_N_ERM_LRA(x)		(((x) & 0x1) << 7)
#define DEFAULT_BEMF_GAIN_ERM		0x0
#define DEFAULT_LR4ERM			0x1
#define DEFAULT_BF4ERM			0x2
#define SET_ERM			0x0

/* WAVEFORMSEQ */
#define WAVEFORMSEQ_TERMINATE	0

/* definitions */
#define WAIT_FOR_CAL_MSEC		500
#define CHECK_CAL_PROCESS_RETRIES	5
#define VIBRA_OFF_VALUE	0
#define VIBRA_ON_VALUE		1
#define LDO_VOLTAGE_3p3V	3300000

const unsigned char ram_table_header[] = {
0x00,		/* RAM Library Revision Byte (to track library revisions) */
0x01, 0x00, 0xE2,	/* Waveform #1 Continuous Buzz */
0x01, 0x04, 0xE2	/* Waveform #2 Continuous Buzz */
};

const unsigned char ram_table_data[] = {
0x3F, 0xFF,		/* Test Continuous Buzz */
0x3F, 0xFF		/* Test Continuous Buzz */
};

struct drv2604_vibrator_info {
	void (*power)(int on);
	struct timed_output_dev vibrator_timed_dev;
	struct timer_list vibrate_timer;
	struct work_struct vibrator_off_work;
	struct mutex vib_mutex;
	struct regulator *vib_regulator;
	int enable;
};

struct i2c_client *drv2604_i2c;


/* I2C LOCAL UTILITIES */
static inline int drv2604_i2c_read_single_byte(struct i2c_client *i2c,
				    int reg, unsigned char *dest)
{
	int ret;

	ret = i2c_smbus_read_byte_data(i2c, reg);
	if (ret < 0)
		return ret;

	*dest = (unsigned char)ret;

	return 0;
}

static inline int drv2604_i2c_write_multiple_byte(struct i2c_client *i2c,
				     int reg, int bytes, void *src)
{
	unsigned char buf[bytes + 1];
	int ret;

	buf[0] = (unsigned char)reg;
	memcpy(&buf[1], src, bytes);

	ret = i2c_master_send(i2c, buf, bytes + 1);
	if (ret < 0)
		return ret;
	return 0;
}

#define I2C_SINGLE_BYTE_BUF_SIZE 2
static inline int drv2604_i2c_write_single_byte(struct i2c_client *i2c, int reg,
							unsigned char val)
{
	unsigned char buf[I2C_SINGLE_BYTE_BUF_SIZE];
	int ret;

	buf[0] = (unsigned char)reg;
	buf[1] = (unsigned char)val;

	ret = i2c_master_send(i2c, buf, I2C_SINGLE_BYTE_BUF_SIZE);
	if (ret < 0)
		return ret;
	return 0;
}

static inline int drv2604_i2c_set_reg_bits(struct i2c_client *i2c, int reg,
							unsigned char mask)
{
	int ret;
	unsigned char val;

	if (drv2604_i2c_read_single_byte(i2c, reg, &val)) {
		VIB_ERR_MSG("I2C read failed\n");
		return -1;
	}

	val |= mask;
	ret = drv2604_i2c_write_single_byte(i2c, reg, val);
	if (ret) {
		VIB_ERR_MSG("I2C write failed\n");
		return -1;
	}

	return 0;
}

static inline int drv2604_i2c_clr_reg_bits(struct i2c_client *i2c, int reg,
							unsigned char mask)
{
	int ret;
	unsigned char val;

	if (drv2604_i2c_read_single_byte(i2c, reg, &val)) {
		VIB_ERR_MSG("I2C read failed\n");
		return -1;
	}

	val &= (~mask);
	ret = drv2604_i2c_write_single_byte(i2c, reg, val);
	if (ret) {
		VIB_ERR_MSG("I2C write failed\n");
		return -1;
	}

	return 0;
}

static int drv2604_control_vibrator(struct drv2604_vibrator_info *info,
				unsigned char value)
{

	int ret;

	mutex_lock(&info->vib_mutex);
	if (info->enable == value) {
		mutex_unlock(&info->vib_mutex);
		return 0;
	}

	if (value == VIBRA_OFF_VALUE) {
		/* turn off actuator */
		ret = drv2604_i2c_write_single_byte(drv2604_i2c,
						DRV2604_GO, STOP_CMD);
		if (ret) {
			VIB_ERR_MSG("I2C write failed\n");
			mutex_unlock(&info->vib_mutex);
			return -1;
		}
	} else if (value == VIBRA_ON_VALUE) {
		/* turn on actuator */
		ret = drv2604_i2c_write_single_byte(drv2604_i2c,
						DRV2604_GO, GO_CMD);
		if (ret) {
			VIB_ERR_MSG("I2C write failed\n");
			mutex_unlock(&info->vib_mutex);
			return -1;
		}
	}
	info->enable = value;
	mutex_unlock(&info->vib_mutex);

	return 0;
}

static void vibrator_off_worker(struct work_struct *work)
{
	struct drv2604_vibrator_info *info;

	info = container_of(work, struct drv2604_vibrator_info,
				vibrator_off_work);
	drv2604_control_vibrator(info, VIBRA_OFF_VALUE);
}

static void on_vibrate_timer_expired(unsigned long x)
{
	struct drv2604_vibrator_info *info;
	info = (struct drv2604_vibrator_info *)x;
	schedule_work(&info->vibrator_off_work);
}

static void vibrator_enable_set_timeout(struct timed_output_dev *sdev,
					int timeout)
{
	struct drv2604_vibrator_info *info;

	info = container_of(sdev, struct drv2604_vibrator_info,
				vibrator_timed_dev);
	pr_debug("Vibrator: Set duration: %dms\n", timeout);

	if (timeout <= 0) {
		drv2604_control_vibrator(info, VIBRA_OFF_VALUE);
		del_timer(&info->vibrate_timer);
	} else {

		drv2604_control_vibrator(info, VIBRA_ON_VALUE);
		mod_timer(&info->vibrate_timer,
			  jiffies + msecs_to_jiffies(timeout));
	}

	return;
}

static int vibrator_get_remaining_time(struct timed_output_dev *sdev)
{
	struct drv2604_vibrator_info *info;
	int rettime;

	info = container_of(sdev, struct drv2604_vibrator_info,
		vibrator_timed_dev);
	rettime = jiffies_to_msecs(jiffies - info->vibrate_timer.expires);
	pr_debug("Vibrator: Current duration: %dms\n", rettime);
	return rettime;
}

static int drv2604_auto_calibration_procedure(struct i2c_client *i2c)
{
	int ret, i;
	/* set status as waiting for calibration process */
	unsigned char val = GO_CMD;

	/*
	 * Set DRV260x Control Registers
	 * =============================
	 */

	/* set rated-voltage register */
	ret = drv2604_i2c_write_single_byte(i2c,
				DRV2604_RATED_VOLTAGE, ERM_RV_2p6);
	if (ret) {
		VIB_ERR_MSG("I2C write failed\n");
		goto end;
	}

	/* set overdrive-clamp-voltage register */
	ret = drv2604_i2c_write_single_byte(i2c,
				DRV2604_OVERDRIVE_CLAMP_VOLTAGE, ERM_ODV_3p6);
	if (ret) {
		VIB_ERR_MSG("I2C write failed\n");
		goto end;
	}

	/* set feedback control register */
	ret = drv2604_i2c_write_single_byte(i2c, DRV2604_FBCTRL,
				FBCTRL_N_ERM_LRA(SET_ERM) |
				FBCTRL_BRAKE_FACTOR(DEFAULT_BF4ERM) |
				FBCTRL_LOOP_RESPONSE(DEFAULT_LR4ERM) |
				FBCTRL_BEMF_GAIN(DEFAULT_BEMF_GAIN_ERM));
	if (ret) {
		VIB_ERR_MSG("I2C write failed\n");
		goto end;
	}

	/*
	 * Run Auto-Calibration
	 * ====================
	 */

	/* exit stand-by mode and set to auto calibration mode */
	ret = drv2604_i2c_write_single_byte(i2c, DRV2604_MODE,
					MODE_SET(MODE_TYPE_AUTOCAL));
	if (ret) {
		VIB_ERR_MSG("I2C write failed\n");
		goto end;
	}

	/* set the GO bit */
	ret = drv2604_i2c_write_single_byte(i2c, DRV2604_GO, GO_CMD);
	if (ret) {
		VIB_ERR_MSG("I2C write failed\n");
		goto end;
	}

	/* wait for calibration process to end */
	i = CHECK_CAL_PROCESS_RETRIES;
	do {
		msleep_interruptible(WAIT_FOR_CAL_MSEC);
		ret = drv2604_i2c_read_single_byte(i2c, DRV2604_GO, &val);
		/* if read error occures- try in next iteration */
		if (ret)
			VIB_ERR_MSG("I2C read failed\n");
	} while (val && (--i));

	if (val) {
		VIB_ERR_MSG("calibration timeout\n");
		ret = -EAGAIN;
		goto end;
	}

	/* check calibration results */
	ret = drv2604_i2c_read_single_byte(i2c, DRV2604_STATUS, &val);
	if (ret) {
		VIB_ERR_MSG("I2C read failed\n");
		goto end;
	}
	if (val & STATUS_DIAG_RESULT) {
		VIB_ERR_MSG("calibration failed\n");
		goto end;
	}
end:
	return ret;
}

static int drv2604_write_waveform_to_ram(struct i2c_client *i2c)
{
	int ret;

	/* Load header into RAM */
	ret = drv2604_i2c_write_single_byte(i2c, DRV2604_RAM_ADDR_UP_BYTE, 0x0);
	if (ret) {
		VIB_ERR_MSG("I2C write failed\n");
		goto end;
	}
	ret = drv2604_i2c_write_single_byte(i2c,
				DRV2604_RAM_ADDR_LOW_BYTE, 0x0);
	if (ret) {
		VIB_ERR_MSG("I2C write failed\n");
		goto end;
	}
	ret = drv2604_i2c_write_multiple_byte(i2c, DRV2604_RAM_DATA,
			ARRAY_SIZE(ram_table_header), (void *)ram_table_header);
	if (ret) {
		VIB_ERR_MSG("I2C write failed\n");
		goto end;
	}

	/* Load data into RAM */
	ret = drv2604_i2c_write_single_byte(i2c,
				DRV2604_RAM_ADDR_UP_BYTE, 0x01);
	if (ret) {
		VIB_ERR_MSG("I2C write failed\n");
		goto end;
	}
	ret = drv2604_i2c_write_single_byte(i2c,
				DRV2604_RAM_ADDR_LOW_BYTE, 0x0);
	if (ret) {
		VIB_ERR_MSG("I2C write failed\n");
		goto end;
	}
	ret = drv2604_i2c_write_multiple_byte(i2c, DRV2604_RAM_DATA,
			ARRAY_SIZE(ram_table_data), (void *)ram_table_data);
	if (ret) {
		VIB_ERR_MSG("I2C write failed\n");
		goto end;
	}
end:
	return ret;
}

static int drv2604_init_procedure(struct i2c_client *i2c)
{
	int ret;

	/* exit stand-by mode */
	ret = drv2604_i2c_write_single_byte(i2c, DRV2604_MODE, MODE_INIT_MASK);
	if (ret) {
		VIB_ERR_MSG("I2C write failed\n");
		goto end;
	}

	/* Set RTP register to zero, prevent playback */
	ret = drv2604_i2c_write_single_byte(i2c, DRV2604_RTP, 0x0);
	if (ret) {
		VIB_ERR_MSG("I2C write failed\n");
		goto end;
	}

	/* Set Library Overdrive time to zero */
	ret = drv2604_i2c_write_single_byte(i2c, DRV2604_ODT, 0x0);
	if (ret) {
		VIB_ERR_MSG("I2C write failed\n");
		goto end;
	}

	/* Set Library Sustain positive time */
	ret = drv2604_i2c_write_single_byte(i2c, DRV2604_SPT, 0x0);
	if (ret) {
		VIB_ERR_MSG("I2C write failed\n");
		goto end;
	}

	/* Set Library Sustain negative time */
	ret = drv2604_i2c_write_single_byte(i2c, DRV2604_SNT, 0x0);
	if (ret) {
		VIB_ERR_MSG("I2C write failed\n");
		goto end;
	}

	/* Set Library Brake Time */
	ret = drv2604_i2c_write_single_byte(i2c, DRV2604_BRT, 0x0);
	if (ret) {
		VIB_ERR_MSG("I2C write failed\n");
		goto end;
	}

	/* perform Auto Calibration Procedure */
	ret = drv2604_auto_calibration_procedure(i2c);
	if (ret) {
		VIB_ERR_MSG("falied to perform auto calibration\n");
		goto end;
	}

	/* populate RAM with waveforms */
	ret = drv2604_write_waveform_to_ram(i2c);
	if (ret) {
		VIB_ERR_MSG("failed to write waveform to RAM\n");
		goto end;
	}

	/* set the desired sequence to be played */
	ret = drv2604_i2c_write_single_byte(i2c, DRV2604_WAVEFORMSEQ1, 0x1);
	if (ret) {
		VIB_ERR_MSG("I2C write failed\n");
		goto end;
	}

	/* Insert termination character in sequence register 2 */
	ret = drv2604_i2c_write_single_byte(i2c, DRV2604_WAVEFORMSEQ2,
							WAVEFORMSEQ_TERMINATE);
	if (ret) {
		VIB_ERR_MSG("I2C write failed\n");
		goto end;
	}

	/* slect internal trigger */
	ret = drv2604_i2c_write_single_byte(i2c, DRV2604_MODE,
					MODE_SET(MODE_TYPE_INT_TRIG));
	if (ret) {
		VIB_ERR_MSG("I2C write failed\n");
		goto end;
	}
end:
	return ret;
}

static int vibrator_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int ret = 0;
	struct drv2604_vibrator_info *info;

	info = devm_kzalloc(&client->dev,
			sizeof(struct drv2604_vibrator_info), GFP_KERNEL);
	if (!info) {
		ret = -ENOMEM;
		goto err;
	}

	/* We should be able to read and write byte data */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		VIB_ERR_MSG("I2C_FUNC_I2C not supported\n");
		ret = -ENOTSUPP;
		goto free_info;
	}

	/* Setup timed_output obj */
	info->vibrator_timed_dev.name = "vibrator";
	info->vibrator_timed_dev.enable = vibrator_enable_set_timeout;
	info->vibrator_timed_dev.get_time = vibrator_get_remaining_time;

	/* Vibrator dev register in /sys/class/timed_output/ */
	ret = timed_output_dev_register(&info->vibrator_timed_dev);
	if (ret < 0) {
		dev_err(&client->dev,
		       "Vibrator: timed_output dev registration failure\n");
		goto err_unregister;
	}

	INIT_WORK(&info->vibrator_off_work, vibrator_off_worker);
	mutex_init(&info->vib_mutex);
	info->enable = 0;

	init_timer(&info->vibrate_timer);
	info->vibrate_timer.function = on_vibrate_timer_expired;
	info->vibrate_timer.data = (unsigned long)info;

	i2c_set_clientdata(client, info);

	info->vib_regulator = regulator_get(&client->dev, "vibrator");
	if (IS_ERR(info->vib_regulator)) {
		VIB_ERR_MSG("get vibrator ldo fail!\n");
		ret = -1;
		goto err_unregister;
	}

	regulator_set_voltage(info->vib_regulator, LDO_VOLTAGE_3p3V,
							LDO_VOLTAGE_3p3V);

	ret = regulator_enable(info->vib_regulator);
	if (ret)
		VIB_ERR_MSG("enable vibrator ldo fail!\n");

	regulator_put(info->vib_regulator);

	/* perform initialization sequence for the DRV2604 */
	drv2604_i2c = client;
	ret = drv2604_init_procedure(client);
	if (ret)
		goto err_unregister;

	return 0;
err_unregister:
	timed_output_dev_unregister(&info->vibrator_timed_dev);
free_info:
	devm_kfree(&client->dev, info);
err:
	return ret;
}

static int vibrator_remove(struct i2c_client *client)
{
	struct drv2604_vibrator_info *info = i2c_get_clientdata(client);

	timed_output_dev_unregister(&info->vibrator_timed_dev);
	i2c_unregister_device(client);
	devm_kfree(&client->dev, info);
	return 0;
}

static const struct i2c_device_id drv2604_id[] = {
	{DRV2604_NAME, 0},
	{}
};

static struct of_device_id drv2604_dt_ids[] = {
	{.compatible = "ti,drv2604-vibrator",},
	{}
};

/* This is the I2C driver that will be inserted */
static struct i2c_driver vibrator_driver = {
	.driver = {
		   .name = DRV2604_NAME,
		   .of_match_table = of_match_ptr(drv2604_dt_ids),
		   },
	.id_table = drv2604_id,
	.probe = vibrator_probe,
	.remove = vibrator_remove,
};

static int __init vibrator_init(void)
{
	int ret;

	ret = i2c_add_driver(&vibrator_driver);
	if (ret)
		VIB_ERR_MSG("i2c_add_driver failed, error %d\n", ret);

	return ret;
}

static void __exit vibrator_exit(void)
{
	i2c_del_driver(&vibrator_driver);
}

module_init(vibrator_init);
module_exit(vibrator_exit);

MODULE_DESCRIPTION("Android Vibrator driver");
MODULE_LICENSE("GPL");
