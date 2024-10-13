/*
** =============================================================================
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of the GNU General Public License
** as published by the Free Software Foundation; either version 2
** of the License, or (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
**
** File:
**     zm918_motor.c
**
** Description:
**     ZM918 vibration motor ic driver
**
** =============================================================================
*/
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/sched.h>
#include <linux/spinlock_types.h>
#include <linux/spinlock.h>
#include <linux/jiffies.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/semaphore.h>
#include <linux/version.h>
#include <linux/input.h>
#include <linux/firmware.h>
#include <linux/interrupt.h>
#include <linux/debugfs.h>
#include <linux/miscdevice.h>
#include <linux/syscalls.h>
#include <linux/power_supply.h>
#include <linux/pm_qos.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/control.h>
#include <sound/soc.h>
#include <linux/debugfs.h>
#include <linux/suspend.h>
#include <linux/vmalloc.h>
#include "zm918_motor.h"

static char zm918_bin_name[][ZM918_BIN_NAME_MAX] = {
	/* 0 */		{"RESERVED.bin"},
	/* 1 */		{"RTP_1.bin"},
	/* 2 */		{"RTP_2.bin"},
	/* 3 */		{"RTP_3.bin"},
	/* 4 */		{"RTP_4.bin"},
	/* 5 */		{"RTP_5.bin"},
	/* 6 */		{"RTP_1.bin"},
	/* 7 */		{"RTP_7.bin"},
	/* 8 */		{"RTP_8.bin"},
	/* 9 */		{"RTP_9.bin"},
	/* 10 */	{"RTP_10.bin"},
	/* 11 */	{"RTP_11.bin"},
	/* 12 */	{"RTP_12.bin"},
	/* 13 */	{"RTP_13.bin"},
	/* 14 */	{"RTP_2.bin"},
	/* 15 */	{"RTP_2.bin"},
	/* 16 */	{"RTP_16.bin"},
	/* 17 */	{"RTP_17.bin"},
	/* 18 */	{"RTP_18.bin"},
	/* 19 */	{"RTP_19.bin"},
	/* 20 */	{"RTP_20.bin"},
	/* 21 */	{"RTP_21.bin"},
	/* 22 */	{"RTP_22.bin"},
	/* 23 */	{"RTP_23.bin"},
	/* 24 */	{"RTP_1.bin"},
	/* 25 */	{"RESERVED.bin"},
	/* 26 */	{"RTP_1.bin"},
	/* 27 */	{"RTP_27.bin"},
	/* 28 */	{"RESERVED.bin"},
	/* 29 */	{"RESERVED.bin"},
	/* 30 */	{"RESERVED.bin"},
	/* 31 */	{"RESERVED.bin"},
	/* 32 */	{"RTP_32.bin"},
	/* 33 */	{"RTP_1.bin"},
	/* 34 */	{"RTP_34.bin"},
	/* 35 */	{"RESERVED.bin"},
	/* 36 */	{"RESERVED.bin"},
	/* 37 */	{"RTP_1.bin"},
	/* 38 */	{"RTP_1.bin"},
	/* 39 */	{"RTP_39.bin"},
	/* 40 */	{"RTP_1.bin"},
	/* 41 */	{"RTP_41.bin"},
	/* 42 */	{"RTP_1.bin"},
	/* 43 */	{"RTP_1.bin"},
	/* 44 */	{"RTP_1.bin"},
	/* 45 */	{"RTP_45.bin"},
	/* 46 */	{"RTP_46.bin"},
	/* 47 */	{"RTP_27.bin"},
	/* 48 */	{"RTP_27.bin"},
	/* 49 */	{"RTP_49.bin"},
	/* 50 */	{"RTP_41.bin"},
	/* 51 */	{"RTP_51.bin"},
	/* 52 */	{"RTP_52.bin"},
	/* 53 */	{"RESERVED.bin"},
	/* 54 */	{"RESERVED.bin"},
	/* 55 */	{"RESERVED.bin"},
	/* 56 */	{"RTP_56.bin"},
	/* 57 */	{"RTP_57.bin"},
	/* 58 */	{"RTP_58.bin"},
	/* 59 */	{"RTP_59.bin"},
	/* 60 */	{"RESERVED.bin"},
	/* 61 */	{"RESERVED.bin"},
	/* 62 */	{"RESERVED.bin"},
	/* 63 */	{"RESERVED.bin"},
	/* 64 */	{"RTP_64.bin"},
	/* 65 */	{"RTP_65.bin"},
	/* 66 */	{"RTP_66.bin"},
	/* 67 */	{"RESERVED.bin"},
	/* 68 */	{"RTP_68.bin"},
	/* 69 */	{"RTP_69.bin"},
	/* 70 */	{"RTP_70.bin"},
	/* 71 */	{"RTP_71.bin"},
	/* 72 */	{"RTP_72.bin"},
	/* 73 */	{"RTP_73.bin"},
	/* 74 */	{"RTP_74.bin"},
	/* 75 */	{"RTP_75.bin"},
	/* 76 */	{"RTP_76.bin"},
	/* 77 */	{"RTP_77.bin"},
	/* 78 */	{"RTP_78.bin"},
	/* 79 */	{"RTP_79.bin"},
	/* 80 */	{"RTP_80.bin"},
	/* 81 */	{"RTP_81.bin"},
	/* 82 */	{"RTP_82.bin"},
	/* 83 */	{"RTP_79.bin"},
	/* 84 */	{"RTP_84.bin"},
	/* 85 */	{"RTP_85.bin"},
	/* 86 */	{"RTP_86.bin"},
	/* 87 */	{"RTP_87.bin"},
	/* 88 */	{"RTP_88.bin"},
	/* 89 */	{"RTP_89.bin"},
	/* 90 */	{"RTP_90.bin"},
	/* 91 */	{"RTP_91.bin"},
	/* 92 */	{"RTP_92.bin"},
	/* 93 */	{"RTP_93.bin"},
	/* 94 */	{"RTP_94.bin"},
	/* 95 */	{"RTP_95.bin"},
	/* 96 */	{"RTP_96.bin"},
	/* 97 */	{"RTP_97.bin"},
	/* 98 */	{"RTP_98.bin"},
	/* 99 */	{"RTP_99.bin"},
	/* 100 */	{"RESERVED.bin"},
	/* 101 */	{"RTP_101.bin"},
	/* 102 */	{"RTP_102.bin"},
	/* 103 */	{"RTP_103.bin"},
	/* 104 */	{"RTP_104.bin"},
	/* 105 */	{"RTP_105.bin"},
	/* 106 */	{"RTP_106.bin"},
	/* 107 */	{"RTP_107.bin"},
	/* 108 */	{"RTP_108.bin"},
	/* 109 */	{"RTP_109.bin"},
	/* 110 */	{"RTP_110.bin"},
	/* 111 */	{"RTP_111.bin"},
	/* 112 */	{"RTP_112.bin"},
	/* 113 */	{"RTP_5.bin"},
	/* 114 */	{"RTP_114.bin"},
	/* 115 */	{"RTP_115.bin"},
	/* 116 */	{"RTP_116.bin"},
	/* 117 */	{"RTP_117.bin"},
	/* 118 */	{"RTP_118.bin"},
	/* 119 */	{"RTP_119.bin"},
	/* 120 */	{"RTP_120.bin"},
	/* 121 */	{"RTP_121.bin"},
	/* 122 */	{"RTP_122.bin"},
	/* 123 */	{"RTP_123.bin"},
	/* 124 */	{"RTP_124.bin"},
	/* 125 */	{"RTP_125.bin"},
	/* 126 */   {"RTP_126.bin"},
	/* 127 */   {"RTP_127.bin"},
};

static int zm918_motor_reg_read(struct zm918 *zm918, unsigned char reg)
{
	unsigned int val;
	int ret;

	ret = regmap_read(zm918->regmap, reg, &val);
	if (ret < 0){
		zm_err("[VIB] 0x%x error (%d)\n", reg, ret);
		return ret;
	}
	else
		return val;
}

static int zm918_motor_reg_write(struct zm918 *zm918,
	unsigned char reg, unsigned char val)
{
	int ret;

	ret = regmap_write(zm918->regmap, reg, val);
	if (ret < 0){
		zm_err("[VIB] 0x%x=0x%x error (%d)\n", reg, val, ret);
	}

	return ret;
}

static int zm918_motor_reg_raw_write(struct zm918 *zm918,
	unsigned char reg, const uint8_t data[], size_t len)
{
	int ret;

	ret = regmap_raw_write(zm918->regmap, reg, data, len);
	if (ret < 0){
		zm_err("[VIB] reg=0x%x, error (%d)\n", reg, ret);
	}

	return ret;
}

static int zm918_motor_set_bits(struct zm918 *zm918,
	unsigned char reg, unsigned char mask, unsigned char val)
{
	int ret;

	ret = regmap_update_bits(zm918->regmap, reg, mask, val);
	if (ret < 0){
		zm_err("[VIB] reg=%x, mask=0x%x, value=0x%x error (%d)\n", reg, mask, val, ret);
	}

	return ret;
}

static struct regmap_config zm918_motor_i2c_regmap = {
	.name = "speedy",
	.reg_bits = 8,
	.val_bits = 8,
	.cache_type = REGCACHE_NONE,
};

static u32 zm918_hz_to_decimal_freq(int freq)
{
	if (freq != 0)
		return (1000000000/(freq * 40 * 512));			// conversion formula shared by IC vendor
	else
		return freq;
}

/*
 * parse device tree
 */
static int zm918_parse_dt(struct device *dev, struct zm918 *zm918,
			    struct device_node *np)
{
	int err;
	int ret = 0;
	const char *motor_type;
	
	zm_info("[VIB] dt parsing start\n");

	err = of_property_read_string(np, "zm918_motor,motor-type", &motor_type);
	if (err < 0) {
		zm_err("[VIB] motor-type read fail(%d)\n", err);
		return -ENODEV;
	}
	if (!strcmp(motor_type, "ACTUATOR_1030")) {
		zm918->dts_info.motor_type = ACTUATOR_1030;
	} else if (!strcmp(motor_type, "ACTUATOR_080935")) {
		zm918->dts_info.motor_type = ACTUATOR_080935;
	} else {
		zm_err("[VIB] Wrong motor type: %s\n", motor_type);
		return -ENODEV;
	}
	zm_info("[VIB] motor-type = %s\n", motor_type);

	zm918->dts_info.gpio_en = of_get_named_gpio(np,
			"zm918_motor,boost_en", 0);
	if (zm918->dts_info.gpio_en < 0)
		zm_err("[VIB] gpio_en read fail(%d)\n",
				zm918->dts_info.gpio_en);
	else
		zm_info("[VIB] motor_en = %d\n", zm918->dts_info.gpio_en);

	err = of_property_read_string(np,
			"zm918_motor,regulator-name", &zm918->dts_info.regulator_name);
	if (err < 0) {
		zm_err("[VIB] regulator-name read fail(%d)\n", err);
		zm918->dts_info.regulator_name = NULL;
	} else
		zm_info("[VIB] regulator-name = %s\n", zm918->dts_info.regulator_name);

	err = of_property_read_u32(np,
			"zm918_motor,drv-freq", &zm918->dts_info.drv_freq);
	if (err < 0) {
		zm918->dts_info.drv_freq = zm918_hz_to_decimal_freq(205);
		zm_err("[VIB] drv_freq read fail(%d) , set to default :%d\n",
				err, zm918->dts_info.drv_freq);
	} else {
		zm918->dts_info.drv_freq = zm918_hz_to_decimal_freq(zm918->dts_info.drv_freq);
		zm_info("[VIB] drv_freq = %d\n", zm918->dts_info.drv_freq);
	}

	err = of_property_read_u32(np,
			"zm918_motor,reso-freq", &zm918->dts_info.reso_freq);
	if (err < 0) {
		zm918->dts_info.reso_freq = zm918_hz_to_decimal_freq(205);
		zm_err("[VIB] reso_freq read fail(%d) , set to default :%d\n",
				err, zm918->dts_info.reso_freq);
	} else {
		zm918->dts_info.reso_freq = zm918_hz_to_decimal_freq(zm918->dts_info.reso_freq);
		zm_info("[VIB] reso_freq = %d\n", zm918->dts_info.reso_freq);
	}

	err = of_property_read_u32(np,
			"zm918_motor,cont_level", &zm918->dts_info.cont_level);
	if (err < 0) {
		zm918->dts_info.cont_level = DEFAULT_MOTOR_CONT_LEVEL;
		zm_err("[VIB] cont level read fail(%d) :%d\n",
				err, zm918->dts_info.cont_level);
	} else
		zm_info("[VIB] cont level = %d\n", zm918->dts_info.cont_level);

	zm918->cont_level = zm918->dts_info.cont_level;

	err = of_property_read_u32(np,
			"zm918_motor,fifo_level", &zm918->dts_info.fifo_level);
	if (err < 0) {
		zm918->dts_info.fifo_level = DEFAULT_MOTOR_FIFO_LEVEL;
		zm_err("[VIB] fifo level read fail(%d) :%d\n",
				err, zm918->dts_info.fifo_level);
	} else
		zm_info("[VIB] fifo level = %d\n", zm918->dts_info.fifo_level);

	zm918->fifo_level = zm918->dts_info.fifo_level;

	err = of_property_read_u32(np, "zm918,vib_mode", &zm918->dts_info.mode);
	if (err < 0)
		zm_err("[VIB] vib_mode not specified");
	else
		zm_info("[VIB] vib_mode = %u\n", zm918->dts_info.mode);

	err = of_property_read_string(np,
			"samsung,vib_type", &zm918->dts_info.vib_type);
	if (err < 0)
		zm_info("[VIB] vib type not specified\n");
	else
		zm_info("[VIB] vib_type = %s\n", zm918->dts_info.vib_type);

	zm_info("[VIB] dt parsing done\n");

	return ret;
}

/* 
 * stop the current playback
 */
static void zm918_haptic_stop(struct zm918 *zm918)
{
	zm_info("enter");
	zm918->play_mode = ZM918_STANDBY_MODE;
	zm918_motor_reg_write(zm918, MOTOR_REG_PLAYBACK_CONTROL, 0x00);		// GO = stop
	zm918_motor_reg_write(zm918, MOTOR_REG_SOFT_EN, 0x00);				// S/W EN Off
}

/*
 * chip working mode setting
 register 0x84 , bit[3:2]
 00 - ram mode
 01 - rtp mode
 10 - cont mode
 */
static int zm918_haptic_play_mode(struct zm918 *zm918, unsigned char play_mode)
{
	zm_info("enter");

	switch (play_mode) {
		case ZM918_STANDBY_MODE:
			zm_info("enter standby mode");
			zm918->play_mode = ZM918_STANDBY_MODE;
			zm918_haptic_stop(zm918);
			break;
		case ZM918_RTP_MODE:
			zm_info("enter rtp mode");
			zm918->play_mode = ZM918_RTP_MODE;
			zm918_motor_set_bits(zm918, MOTOR_REG_PLAY_MODE,
				ZM918_PLAY_MODE_MASK,ZM918_PLAY_MODE_RTP);
			break;
		case ZM918_CONT_MODE:
			zm_info("enter cont mode");
			zm918->play_mode = ZM918_CONT_MODE;
			zm918_motor_set_bits(zm918, MOTOR_REG_PLAY_MODE,
				ZM918_PLAY_MODE_MASK,ZM918_PLAY_MODE_CONT);
			break;
		default:
			zm_err("play mode %d error", play_mode);
			break;
	}
	return 0;
}

static int zm918_haptic_init(struct zm918 *zm918)
{
	int ret = 0;

	zm_info("enter");
	mutex_lock(&zm918->lock);
	/* haptic init */
	zm918->gain = 10000;
	zm918->fifo_repeat = 0;
	zm918->stop_now = 0;
	zm918->activate_mode = zm918->dts_info.mode;
	zm918_haptic_play_mode(zm918, ZM918_STANDBY_MODE);

	mutex_unlock(&zm918->lock);
	return ret;
}

/*
 * gets the zm918 drvier data
 */
inline struct zm918 *zm918_get_drvdata(struct device *dev)
{
	struct sec_vib_inputff_drvdata *ddata = dev_get_drvdata(dev);
	struct zm918 *zm918 = ddata->private_data;

	return zm918;
}

/*
 * gets the zm918 driver data from the input dev
 */
inline struct zm918 *zm918_input_get_drvdata(struct input_dev *dev)
{
	struct sec_vib_inputff_drvdata *ddata = input_get_drvdata(dev);
	struct zm918 *zm918 = ddata->private_data;

	return zm918;
}

static int zm918_set_use_sep_index(struct input_dev *dev, bool use_sep_index)
{
	struct zm918 *zm918 = zm918_input_get_drvdata(dev);

	zm_info("use_sep_index=%d", use_sep_index);

	mutex_lock(&zm918->lock);
	zm918->use_sep_index = use_sep_index;
	mutex_unlock(&zm918->lock);

	return 0;
}

/*
 * Samsung Gain values : 0 ~ 10000
 *
 * Zinitix cont Gain Register Values (Actuator 1030) : 9 ~ 66 (0x09 ~ 0x42)
 *
 * Zinitix cont Gain Register Values (Actuator 080935) : 9 ~ 54 (0x09 ~ 0x36)
 * Samsung mapping formula:
 *   level = (range * ss_gain / 10000) + MIN_MOTOR_CONT_LEVEL
 *
 * For -ve gain, set 7th bit as 1 and in remaining bits, write +ve gain value
 * +ve and -ve gain differ only in actuator direction when they first move, strength is same
 */
static void zm918_set_cont_gain(struct zm918 *zm918)
{
	int range = 0;

	if (!zm918) {
		zm_err("[VIB] dev NULL error\n");
		return;
	}

	range = zm918->dts_info.cont_level - MIN_MOTOR_CONT_LEVEL;

	/* limit range */
	if (zm918->gain > 10000)
		zm918->gain = 10000;
	else if (zm918->gain < -10000)
		zm918->gain = -10000;

	/* convert samsung intensity to zm918 gain level */
	if (zm918->gain == 0)
		zm918->cont_level = 0x0;
	else if (zm918->gain > 0)
		zm918->cont_level = (range * zm918->gain / 10000) + MIN_MOTOR_CONT_LEVEL;
	else if (zm918->gain < 0)
		zm918->cont_level = (1<<7) | ((range * (-(zm918->gain)) / 10000) + MIN_MOTOR_CONT_LEVEL);

	zm_info("samsung intensity=%d, zm cont level=0x%04X",
				zm918->gain, zm918->cont_level);
}

/*
 * Samsung Gain values : 0 ~ 10000
 * Zinitix Fifo Gain Register Values (0x2A): 0x00 ~ 80 (128 values)
 *
 * Samsung mapping formula:
 *   level = (range * ss_gain / 10000) + MIN_MOTOR_FIFO_LEVEL
 *
 * For -ve samsung gain, take absolute value
 */
static void zm918_set_fifo_gain(struct zm918 *zm918)
{
	int range = 0;

	if (!zm918) {
		zm_err("[VIB] dev NULL error\n");
		return;
	}

	range = zm918->dts_info.fifo_level - MIN_MOTOR_FIFO_LEVEL;

	/* limit range */
	if (zm918->gain > 10000 || zm918->gain < -10000)
		zm918->gain = 10000;
	else if (zm918->gain < 0)
		zm918->gain = -(zm918->gain);

	/* convert samsung intensity to zm918 fifo gain level */
	zm918->fifo_level = (range * zm918->gain / 10000) + MIN_MOTOR_FIFO_LEVEL;

	zm_info("samsung intensity=%d, zm fifo level=0x%04X",
				zm918->gain, zm918->fifo_level);
}

/*
 * Samsung Intensity to IC level conversion
 */
static void zm918_haptics_set_gain_work_routine(struct work_struct *work)
{
	struct zm918 *zm918 =
		container_of(work, struct zm918, set_gain_work);

	zm918_set_cont_gain(zm918);
	zm918_set_fifo_gain(zm918);
}

static void zm918_set_auto_cal_mode_config(struct zm918 *zm918)
{
	zm918_motor_reg_write(zm918, MOTOR_REG_SOFT_EN, 0x01);		// S/W EN

	if (zm918->dts_info.motor_type == ACTUATOR_1030) {
		zm918_motor_reg_write(zm918, MOTOR_REG_MODE_01, 0x0d);		// Operationg Mode = Auto Cal
		zm918_motor_reg_write(zm918, MOTOR_REG_STRENGTH, 0x42);		// Strength
		zm918_motor_reg_write(zm918, MOTOR_REG_DRV_FREQ_H, 0x00);	// DRV_FREQ
		zm918_motor_reg_write(zm918, MOTOR_REG_DRV_FREQ_L, 0xEE);	//
		zm918_motor_reg_write(zm918, MOTOR_REG_RESO_FREQ_H, 0x00);	// RESO_FREQ
		zm918_motor_reg_write(zm918, MOTOR_REG_RESO_FREQ_L, 0xEE);	//
	} else if (zm918->dts_info.motor_type == ACTUATOR_080935) {
		zm918_motor_reg_write(zm918, MOTOR_REG_MODE_01, 0x04);		// Operationg Mode = Auto Cal
		zm918_motor_reg_write(zm918, MOTOR_REG_STRENGTH, 0x28);		// Strength
		zm918_motor_reg_write(zm918, MOTOR_REG_DRV_FREQ_H, 0x01);	// DRV_FREQ
		zm918_motor_reg_write(zm918, MOTOR_REG_DRV_FREQ_L, 0x13);	// find better
		zm918_motor_reg_write(zm918, MOTOR_REG_RESO_FREQ_H, 0x01);	// RESO_FREQ 177Hz
		zm918_motor_reg_write(zm918, MOTOR_REG_RESO_FREQ_L, 0x17);	//
	}

	zm918_motor_reg_write(zm918, MOTOR_REG_MODE_13, 0x00);		// SIN_Out_EN 0 : Square Wave
	zm918_motor_reg_write(zm918, MOTOR_REG_OVER_DRV, 0x00);		// Over_DRV_EN 0 : disable

	if (zm918->dts_info.motor_type == ACTUATOR_1030)
		zm918_motor_reg_write(zm918, MOTOR_REG_CAL_EN, 0x03);		// Cal_EN On
	else if (zm918->dts_info.motor_type == ACTUATOR_080935)
		zm918_motor_reg_write(zm918, MOTOR_REG_CAL_EN, 0x0f);		// Cal_EN On

	zm918_motor_reg_write(zm918, MOTOR_REG_PLAY_MODE, 0x0c);	// PLAY_MODE = Auto Cal
}

static void zm918_set_cont_mode_config(struct zm918 *zm918)
{
	zm918_motor_reg_write(zm918, MOTOR_REG_SOFT_EN, 0x01);		// S/W EN
	zm918_motor_reg_write(zm918, MOTOR_REG_MODE_01, 0x45);		// Operationg Mode = Open loop, Brake On
	zm918_motor_reg_write(zm918, MOTOR_REG_STRENGTH, zm918->cont_level);		// Strength
	zm918_motor_reg_write(zm918, MOTOR_REG_DRV_FREQ_H, HIGH_8_BITS(zm918->f0));   // DRV_FREQ
	zm918_motor_reg_write(zm918, MOTOR_REG_DRV_FREQ_L, LOW_8_BITS(zm918->f0));	//
	zm918_motor_reg_write(zm918, MOTOR_REG_RESO_FREQ_H, HIGH_8_BITS(zm918->dts_info.reso_freq)); // RESO_FREQ
	zm918_motor_reg_write(zm918, MOTOR_REG_RESO_FREQ_L, LOW_8_BITS(zm918->dts_info.reso_freq));  //
	zm918_motor_reg_write(zm918, MOTOR_REG_MODE_13, 0x10);		// SIN_Out_EN
	zm918_motor_reg_write(zm918, MOTOR_REG_OVER_DRV, 0x83);		// Over_DRV_EN
	zm918_motor_reg_write(zm918, MOTOR_REG_CAL_EN, 0x00);		// Cal_EN Off
	zm918_motor_reg_write(zm918, MOTOR_REG_PLAY_MODE, 0x08);	// PLAY_MODE
}

static void zm918_play_auto_cal_pattern_cont(struct zm918 *zm918)
{
	zm918_set_auto_cal_mode_config(zm918);
	zm918_motor_reg_write(zm918, MOTOR_REG_PLAYBACK_CONTROL, 0x01);		// GO = run
	mdelay(100);			// 100 ms run
	zm918_motor_reg_write(zm918, MOTOR_REG_PLAYBACK_CONTROL, 0x00);		// GO = stop
	zm918_motor_reg_write(zm918, MOTOR_REG_PLAY_MODE, 0x08);			// PLAY_MODE = Continue
	zm918_motor_reg_write(zm918, MOTOR_REG_MODE_01, 0x45);				// Operating Mode = Open Loop, Brake On
	zm918_motor_reg_write(zm918, MOTOR_REG_MODE_13, 0x30);				// SIN_Out_EN : Out_PWM_VBR_Freq : 97.6 KHz

	if (zm918->dts_info.motor_type == ACTUATOR_1030) {
		zm918_motor_reg_write(zm918, 0x1A, 0x28);		// SEARCH_DRV_RATIO1
		zm918_motor_reg_write(zm918, 0x1B, 0x28);		// SEARCH_DRV_RATIO2
		zm918_motor_reg_write(zm918, 0x1C, 0x28);		// SEARCH_DRV_RATIO3
	} else if (zm918->dts_info.motor_type == ACTUATOR_080935) {
		zm918_motor_reg_write(zm918, 0x1A, 0x28);		// SEARCH_DRV_RATIO1
		zm918_motor_reg_write(zm918, 0x1B, 0x28);		// SEARCH_DRV_RATIO2
		zm918_motor_reg_write(zm918, 0x1C, 0x28);		// SEARCH_DRV_RATIO3
	}

	zm918_motor_reg_write(zm918, MOTOR_REG_CAL_EN, 0x00);				// Cal_EN off

	zm918_motor_reg_write(zm918, MOTOR_REG_PLAYBACK_CONTROL, 0x01);		// GO = run
	mdelay(3);															// 3ms Delay Time
	zm918_motor_reg_write(zm918, MOTOR_REG_PLAYBACK_CONTROL, 0x00);		// GO = stop
	mdelay(100);														// 100ms Delay Time
	zm918_motor_reg_write(zm918, MOTOR_REG_SOFT_EN, 0x00);				// S/W EN Off
}

/*
 * buff : file contents
 * file size : buff size
 */
void zm918_play_sep_index_pattern(struct zm918 *zm918)
{
	int offset = 0;
	int func = 0;
	int data_len = 0;
	bool write_high_bits = 1;
	unsigned char val = 0;
	int file_sz = 0;
	int delay_sz = 0;

	if (!zm918) {
		zm_err("[VIB] dev NULL error\n");
		return;
	}
	file_sz = zm918->p_container->len;

repeated_playback:
	while (offset < file_sz) {
		if (zm918->stop_now) {
			zm918->fifo_repeat = 0;
			break;
		}

		/* task data : func(1bytes) + data_len + data */
		func = zm918->p_container->data[offset];
		offset += 1;

		data_len = zm918->p_container->data[offset] * 256 + zm918->p_container->data[offset + 1];
		if (offset + data_len > file_sz || data_len < 2) {
			zm_err("[VIB] error : data len error : (%d + %d) > %d\n", offset, data_len, file_sz);
			return;
		}
		offset += 2;

		/* function parsing */
		if (func == 0xE1) {  /* i2c write */
			/* register number + register datas */
			zm918_motor_reg_raw_write(zm918, zm918->p_container->data[offset],
							&zm918->p_container->data[offset + 1], data_len - 1);
		} else if (func == 0xE2) { /* delay ms */
			if (data_len != 2) /* delay sz is always 2 bytes */
				zm_err("[VIB] warning : delay function may be wrong : data len = %d\n", data_len);
			delay_sz = (int)(zm918->p_container->data[offset] * 256 + zm918->p_container->data[offset + 1]);
			while (delay_sz > 50 && !zm918->stop_now) {
				mdelay(50);
				delay_sz -= 50;
			}
			if (!zm918->stop_now)
				mdelay(delay_sz);
		} else if (func == 0xE3) { /* i2c write Continuous Mode Strength */
			/* register number + register datas */
			val = (unsigned char)(zm918->cont_level);
			zm918_motor_reg_raw_write(zm918, zm918->p_container->data[offset], &val, data_len - 1);
		} else if (func == 0xE4) { /* i2c write FIFO Strength */
			/* register number + register datas */
			val = (unsigned char)(zm918->fifo_level);
			zm918_motor_reg_raw_write(zm918, zm918->p_container->data[offset], &val, data_len - 1);
		} else if (func == 0xE5) { /* i2c write Frequency */
			/* register number + register datas */
			if (write_high_bits)
				val = HIGH_8_BITS(zm918->dts_info.drv_freq);
			else
				val = LOW_8_BITS(zm918->dts_info.drv_freq);

			write_high_bits = !write_high_bits;
			zm918_motor_reg_raw_write(zm918, zm918->p_container->data[offset], &val, data_len - 1);
		} else if (func == 0xE6) { /* FIFO Frequency */
			val = (int)zm918->p_container->data[offset + 1];
			/* calculate updated fifo read time based on f0 */
			val = ((17000 / (1000000000 / (zm918->f0 * 40 * 512))) * val) / 100 + 1; // *(100/100) to calculate up to 2 decimal
			/* write updated fifo read time */
			zm918_motor_reg_write(zm918, zm918->p_container->data[offset], val);
		} else {
			zm_err("[VIB] error : unknown function = 0x%02x\n", func);
			return;
		}

		/* next task data */
		offset += data_len;
	}

	if (zm918->fifo_repeat) {
		offset = 0;
		func = 0;
		data_len = 0;
		write_high_bits = 1;
		val = 0;
		goto repeated_playback;
	}

	return;
}

static int _zm918_store_sep_index_pattern(struct zm918 *zm918, unsigned char *buff, int file_sz)
{
	int offset = 0;
	int data_sz = 0;
	int n = 0;
#ifdef ZM918_DEBUG_FEATURE
	unsigned short int fchecksum = 0;
	unsigned short checksum = 0;
#endif

	if (!zm918) {
		zm_err("[VIB] dev NULL error\n");
		return -EINVAL;
	}

	/* file size must be greater than header size 21 bytes + checksum size 2 bytes */
	if (file_sz < ZINITIX_HEADER_LEN + 2) {
		zm_err("[VIB] error : file size error = %d\n", file_sz);
		return -EINVAL;
	}

	/* (1) first 21 bytes : header signature */
	if (strncmp(buff, ZINTIIX_SIGNATURE_STR, strlen(ZINTIIX_SIGNATURE_STR)) != 0) {
		zm_err("[VIB] error : file signature mismatch\n");
		return -EINVAL;
	}
	offset += ZINITIX_HEADER_LEN;

#ifdef ZM918_DEBUG_FEATURE
	/*
	 * (2) verify file checksum
	 * last 2 bytes is checksum : big endian
	 */
	fchecksum = (unsigned short int)(buff[file_sz - 1] | (unsigned short int)(buff[file_sz - 2] << 8));
#endif
	/* subtract checksum size 2bytes */
	file_sz -= 2;

#ifdef ZM918_DEBUG_FEATURE
	/* calculate checksum 16 : little endian*/
	for (n = 0; n < file_sz; n += 2) {
		if (n + 1 < file_sz)
			checksum += (unsigned short int)((buff[n]) | (unsigned short int)(buff[n + 1] << 8));
		else
			checksum += (unsigned short int)(buff[n]);
	}

	if (checksum != fchecksum) {
		zm_err("[VIB] error : checksum error : 0x%04x , 0x%04x\n", checksum, fchecksum);
		return -EINVAL;
	}
#endif

	vfree(zm918->p_container);
	data_sz = file_sz - ZINITIX_HEADER_LEN; // data size = file_sz - signature - checksum
	zm918->p_container = vmalloc(data_sz + sizeof(int));
	if (!zm918->p_container) {
		zm_err("[VIB] error allocating memory");
		return -EINVAL;
	}

	zm918->p_container->len = data_sz;
	for (n = 0; n < data_sz; n++)
		zm918->p_container->data[n] = buff[ZINITIX_HEADER_LEN + n];

	return 0;
}

static int zm918_store_sep_index_pattern(struct zm918 *zm918, const char *file_path)
{
	int ret = 0;
	const struct firmware *vib_fw;

	if (!zm918) {
		zm_err("[VIB] dev NULL error\n");
		return -EINVAL;
	}

	zm_info("[VIB] file = %s\n", file_path);

	/* fw load */
	ret = request_firmware(&vib_fw, file_path, zm918->dev);
	if (ret < 0) {
		zm_err("[VIB] failed to read %s\n", file_path);
		goto fw_request_fail;
	}

	ret = _zm918_store_sep_index_pattern(zm918, (unsigned char *) vib_fw->data, vib_fw->size);
	if (ret < 0)
		zm_err("[VIB] pattern not copied");
	release_firmware(vib_fw);

fw_request_fail:
	vib_fw = NULL;

	return 0;
}

/*
 * long vibration timer callback function
 */
static enum hrtimer_restart zm918_vibrator_timer_func(struct hrtimer *timer)
{
	struct zm918 *zm918 = container_of(timer, struct zm918, timer);

	zm_info("enter");
	queue_work(zm918->work_queue, &zm918->stop_work);
	zm918->fifo_repeat = 0;

	return HRTIMER_NORESTART;
}

static void zm918_cont_mode_playback(struct zm918 *zm918,unsigned int duration)
{
	zm_info("enter");
	zm918_set_cont_mode_config(zm918);
	zm918_motor_reg_write(zm918, MOTOR_REG_PLAYBACK_CONTROL, 0x01);	// GO = run
	msleep(duration);		// Delay
	zm918_motor_reg_write(zm918, MOTOR_REG_PLAYBACK_CONTROL, 0x00);	// GO = stop
	zm918_motor_reg_write(zm918, MOTOR_REG_SOFT_EN, 0x00);			// S/W EN Off
}

int is_valid_sep_index(int effect_id)
{
	if (effect_id > 0 && effect_id < 128) {
		switch (effect_id) {
		case 25:
		case 28 ... 31:
		case 35 ... 36:
		case 53 ... 55:
		case 60 ... 63:
		case 67:
		case 100:
			return 0;	/* invalid */
		default:
			return 1;	/* valid */
		}
	}

	return 0;	/* invalid */
}

static inline bool is_valid_params(struct device *dev, struct device_attribute *attr,
	const char *buf, struct zm918 *zm918)
{
	if (!dev) {
		zm_err("[VIB] dev is NULL\n");
		return false;
	}
	if (!attr) {
		zm_err("[VIB] attr is NULL\n");
		return false;
	}
	if (!buf) {
		zm_err("[VIB] buf is NULL\n");
		return false;
	}
	if (!zm918) {
		zm_err("[VIB] zm918 is NULL\n");
		return false;
	}
	return true;
}

static ssize_t pattern_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t len)
{
	struct zm918 *zm918 = zm918_get_drvdata(dev);
	int pattern_num = 0;
	int ret = 0;

	if (!is_valid_params(dev, attr, buf, zm918))
		return -ENODATA;

	ret = kstrtoint(buf, 0, &pattern_num);
	if (ret != 0)
		return -EINVAL;

	zm918->stop_now = 0; // reset
	if (is_valid_sep_index(pattern_num)) {
		zm918_store_sep_index_pattern(zm918, zm918_bin_name[pattern_num]);
		zm918_play_sep_index_pattern(zm918);
	} else if (pattern_num == 200) {
		zm918_play_auto_cal_pattern_cont(zm918);
	} else {
		zm_info("Unsupported sep index : %d", pattern_num);
		return -EINVAL;
	}

	return len;
}
static DEVICE_ATTR_WO(pattern);

static ssize_t adb_control_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct zm918 *zm918 = zm918_get_drvdata(dev);

	if (!zm918) {
		zm_err("[VIB] dev NULL error\n");
		return sprintf(buf, "NULL\n");
	}

	return sprintf(buf, "%d\n", zm918->adb_control);
}

static ssize_t adb_control_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t len)
{
	struct zm918 *zm918 = zm918_get_drvdata(dev);
	int adb_control;
	int ret;

	if (!is_valid_params(dev, attr, buf, zm918))
		return -ENODATA;

	ret = kstrtoint(buf, 0, &adb_control);
	if (ret != 0)
		return -EINVAL;

	zm918->adb_control = adb_control;

	return len;
}
static DEVICE_ATTR_RW(adb_control);

static ssize_t intensity_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct zm918 *zm918 = zm918_get_drvdata(dev);

	if (!zm918) {
		zm_err("[VIB] dev NULL error\n");
		return sprintf(buf, "NULL\n");
	}

	return sprintf(buf, "%d\n", zm918->gain);
}

static ssize_t intensity_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t len)
{
	struct zm918 *zm918 = zm918_get_drvdata(dev);
	int intensity;
	int ret;

	if (!is_valid_params(dev, attr, buf, zm918))
		return -ENODATA;

	if (!zm918->adb_control) {
		zm_info("[VIB] intensity not updated from adb as adb control is disabled");
		return -EINVAL;
	}

	ret = kstrtoint(buf, 0, &intensity);
	if (ret != 0)
		return -EINVAL;

	zm918->gain = intensity;
	zm918_set_cont_gain(zm918);
	zm918_set_fifo_gain(zm918);

	return len;
}
static DEVICE_ATTR_RW(intensity);

static ssize_t frequency_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t len)
{
	struct zm918 *zm918 = zm918_get_drvdata(dev);
	int frequency;
	int ret;

	if (!is_valid_params(dev, attr, buf, zm918))
		return -ENODATA;

	if (!zm918->adb_control) {
		zm_info("[VIB] frequency not updated from adb as adb control is disabled");
		return -EINVAL;
	}

	ret = kstrtoint(buf, 0, &frequency);
	if (ret != 0)
		return -EINVAL;

	if (frequency < 0) {
		zm_info("[VIB] invalid frequency value %d\n", frequency);
		return -EINVAL;
	}

	zm918->dts_info.drv_freq = zm918_hz_to_decimal_freq(frequency);
	zm_info("[VIB] drv_freq = %d\n", zm918->dts_info.drv_freq);

	return len;
}
static DEVICE_ATTR_WO(frequency);

static ssize_t enable_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t len)
{
	struct zm918 *zm918 = zm918_get_drvdata(dev);
	int duration;
	int ret;

	if (!is_valid_params(dev, attr, buf, zm918))
		return -ENODATA;

	ret = kstrtoint(buf, 0, &duration);
	if (ret != 0)
		return -EINVAL;

	zm918_cont_mode_playback(zm918, duration);
	return len;
}
static DEVICE_ATTR_WO(enable);

/**
 * reg_arr - list of registers offset, which wil be used for reading/writing
 *		via adb for debugging/development/tuning purpose.
 **/
static unsigned char reg_arr[] = {
	MOTOR_REG_MODE_01,
	MOTOR_REG_SOFT_EN,
	MOTOR_REG_STRENGTH,
	MOTOR_REG_MODE_13,
	MOTOR_REG_START_STRENGTH,
	MOTOR_REG_SEARCH_DRV_RATIO1,
	MOTOR_REG_SEARCH_DRV_RATIO2,
	MOTOR_REG_SEARCH_DRV_RATIO3,
	MOTOR_REG_WAIT_MAX,
	MOTOR_REG_DRV_FREQ_H,
	MOTOR_REG_DRV_FREQ_L,
	MOTOR_REG_RESO_FREQ_H,
	MOTOR_REG_RESO_FREQ_L,
	MOTOR_REG_FIFO_INTERVAL,
	MOTOR_REG_FIFO_PWM_FREQ,
	MOTOR_REG_PLAY_MODE,
};

/**
 * reg_show - sysfs read interface to show all register value
 *      Accessible only on non-ship binaries.
 **/
static ssize_t reg_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct zm918 *zm918 = zm918_get_drvdata(dev);
	unsigned int val = 0, i=0;
	unsigned int size = sizeof(reg_arr)/sizeof(reg_arr[0]);
	unsigned int len = sprintf(buf, "Register   Value\n");

	for (i = 0; i < size; i++) {
		val = zm918_motor_reg_read(zm918, reg_arr[i]);
		len += sprintf(buf + len, "  0x%02x      0x%02x\n", reg_arr[i], val);
	}

	return len;
}

/**
 * reg_store - sysfs write interface to update given register by
 *      desired value.
 **/
static ssize_t reg_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t len)
{
	struct zm918 *zm918 = zm918_get_drvdata(dev);
	unsigned int reg = 0, regval = 0;
	int ret =0;

	if (sscanf(buf, "%x %x", &reg, &regval) != 2)
		return -EINVAL;
	if (reg > 256 || regval > 256)
		return -EINVAL;

	ret = zm918_motor_reg_write(zm918, reg, regval);
	if (ret < 0)
		zm_err("[VIB] Reg 0x%02x write fail (%d)\n", reg, ret);

	return len;
}
static DEVICE_ATTR_RW(reg);

static struct attribute *zm918_vibrator_attributes[] = {
	&dev_attr_reg.attr,
	&dev_attr_enable.attr,
	&dev_attr_adb_control.attr,
	&dev_attr_intensity.attr,
	&dev_attr_frequency.attr,
	&dev_attr_pattern.attr,
	NULL
};

struct attribute_group zm918_vibrator_attribute_group = {
	.attrs = zm918_vibrator_attributes
};

/* 
 * the callback function of the workqueue vib_work
 */
static void zm918_vib_work_routine(struct work_struct *work)
{
	struct zm918 *zm918 = container_of(work, struct zm918,
					       vib_work);

	if (!zm918) {
		zm_err("[VIB] dev NULL error\n");
		return;
	}

	zm_info("effect_id=%d state=%d", zm918->effect_id, zm918->state);
	mutex_lock(&zm918->lock);
	zm_info("enter");
	/* Enter standby mode */
	zm918_haptic_stop(zm918);
	if (zm918->state) {
		if (zm918->activate_mode == ZM918_CONT_DURATION_MODE) {
			zm_info("enter cont duration mode");
			zm918_set_cont_mode_config(zm918);
			//Play motor for duration
			zm918_motor_reg_write(zm918, MOTOR_REG_PLAYBACK_CONTROL, 0x01);		// GO = run
			/* run ms timer */
			hrtimer_start(&zm918->timer,
				      ktime_set(zm918->duration / 1000,
				      (zm918->duration % 1000) * 1000000),
				      HRTIMER_MODE_REL);
		} else if (zm918->activate_mode == ZM918_BIN_PARSE_MODE)
			zm918_play_sep_index_pattern(zm918);
		else
			zm_err("activate_mode error");
	} else {
		if (zm918->wk_lock_flag == 1) {
			pm_relax(zm918->dev);
			zm918->wk_lock_flag = 0;
		}
	}
	mutex_unlock(&zm918->lock);
}

static int zm918_vibrator_init(struct zm918 *zm918)
{
	zm_info("enter");
	hrtimer_init(&zm918->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	zm918->timer.function = zm918_vibrator_timer_func;
	INIT_WORK(&zm918->vib_work, zm918_vib_work_routine);
	mutex_init(&zm918->lock);
	atomic_set(&zm918->is_in_rtp_loop, 0);
	atomic_set(&zm918->exit_in_rtp_loop, 0);
	init_waitqueue_head(&zm918->wait_q);
	init_waitqueue_head(&zm918->stop_wait_q);

	return 0;
}

/*
 * analysis of upper layer parameters of input architecture
 */
static int zm918_haptics_upload_effect(struct input_dev *dev,
					 struct ff_effect *effect,
					 struct ff_effect *old)
{
	struct zm918 *zm918 = zm918_input_get_drvdata(dev);
	unsigned short data[2] = {0};
	ktime_t rem;
	unsigned long long time_us;
	int ret;

	if (!zm918) {
		zm_err("[VIB] dev NULL error\n");
		return -EINVAL;
	}

	zm_info("enter, effect->type=0x%X, FF_CONSTANT=0x%X, FF_PERIODIC=0x%X",
		effect->type, FF_CONSTANT, FF_PERIODIC);

	if (hrtimer_active(&zm918->timer)) {
		rem = hrtimer_get_remaining(&zm918->timer);
		time_us = ktime_to_us(rem);
		zm_info("waiting for playing clear sequence: %llu us", time_us);
		usleep_range(time_us, time_us + 100);
	}

	zm918->effect_type = effect->type;
	cancel_work_sync(&zm918->vib_work);
	mutex_lock(&zm918->lock);
	zm918->stop_now = 0; // reset

	while (atomic_read(&zm918->exit_in_rtp_loop)) {
		zm_info("goint to waiting rtp exit");
		mutex_unlock(&zm918->lock);
		ret = wait_event_interruptible(zm918->stop_wait_q,
				atomic_read(&zm918->exit_in_rtp_loop) == 0);
		zm_info("wakeup");
		if (ret == -ERESTARTSYS) {
			mutex_unlock(&zm918->lock);
			zm_info(": wake up by signal return erro");
			return ret;
		}
		mutex_lock(&zm918->lock);
	}

	if (zm918->effect_type == FF_CONSTANT) {
		zm_info("effect_type: FF_CONSTANT!");
		zm918->duration = effect->replay.length;
		zm918->activate_mode = ZM918_CONT_DURATION_MODE;
	} else if (zm918->effect_type == FF_PERIODIC) {
		zm_info("effect_type: FF_PERIODIC!");
		if (effect->u.periodic.waveform == FF_CUSTOM) {
			ret = copy_from_user(data, effect->u.periodic.custom_data,
					     sizeof(short) * 2);
			if (ret) {
				mutex_unlock(&zm918->lock);
				return -EFAULT;
			}
		} else {
			data[0] = effect->u.periodic.offset;
			data[1] = 0;
		}

		zm_info("zm918->effect_id= %u", data[0]);

		if (is_valid_sep_index(data[0])) {
			zm918->activate_mode = ZM918_BIN_PARSE_MODE;
			zm918->fifo_repeat = data[1];
			if (zm918->effect_id != data[0]) {
				zm918->effect_id = data[0];
				zm918_store_sep_index_pattern(zm918, zm918_bin_name[zm918->effect_id]);
			}
			zm_info("effect_id=%d, activate_mode=%d, fifo_repeat=%d",
				zm918->effect_id, zm918->activate_mode, zm918->fifo_repeat);
		} else {
			zm_info("Unsupported effect ID: %d", zm918->effect_id);
		}
	} else {
		zm_info("Unsupported effect type: %d", effect->type);
	}
	mutex_unlock(&zm918->lock);
	zm_info("zm918->effect_type= 0x%X", zm918->effect_type);
	return 0;
}

/* 
 * input architecture playback function
 */
static int zm918_haptics_playback(struct input_dev *dev, int effect_id,
					int val)
{
	struct zm918 *zm918 = zm918_input_get_drvdata(dev);
	int rc = 0;

	if (!zm918) {
		zm_err("[VIB] dev NULL error\n");
		return -EINVAL;
	}

	zm_info("enter, effect_id=%d, val=%d, zm918->effect_id=%d, zm918->activate_mode=%d",
		effect_id, val, zm918->effect_id, zm918->activate_mode);

	/* val 1 - start playback , val 0 - stop playback */
	if (val > 0) {
		zm918->state = 1;
	} else {
		queue_work(zm918->work_queue, &zm918->stop_work);
		zm918->stop_now = 1;
		zm918->fifo_repeat = 0;
		zm_err("[VIB] fifo_repeat: %d, stop_now: %d", zm918->fifo_repeat, zm918->stop_now);
		return 0;
	}
	hrtimer_cancel(&zm918->timer);

	// Queue vib work
	if (zm918->effect_type == FF_CONSTANT &&
	    zm918->activate_mode == ZM918_CONT_DURATION_MODE) {
		zm_info("enter cont_duration_mode ");
		queue_work(zm918->work_queue, &zm918->vib_work);
	} else if (zm918->effect_type == FF_PERIODIC &&
		   zm918->activate_mode == ZM918_BIN_PARSE_MODE) {
		zm_info("enter bin parse mode");
		queue_work(zm918->work_queue, &zm918->vib_work);
	} else {
		/*other mode */
	}

	return rc;
}

static int zm918_haptics_erase(struct input_dev *dev, int effect_id)
{
	struct zm918 *zm918 = zm918_input_get_drvdata(dev);
	int rc = 0;

	zm_info("enter");
	zm918->effect_type = 0;
	zm918->duration = 0;
	return rc;
}

static void zm918_haptics_stop_work_routine(struct work_struct *work)
{
	struct zm918 *zm918 = container_of(work, struct zm918, stop_work);

	mutex_lock(&zm918->lock);
	hrtimer_cancel(&zm918->timer);
	zm918_haptic_stop(zm918);
	mutex_unlock(&zm918->lock);
}

static void zm918_initial_reg_settings(struct zm918 *zm918)
{
	zm918_motor_reg_write(zm918, MOTOR_REG_SOFT_EN, 0x01);		// S/W EN High

	if (zm918->dts_info.motor_type == ACTUATOR_1030) {
		zm918_motor_reg_write(zm918, MOTOR_REG_MODE_01, 0x0d);		// Operationg Mode = Open loop, Brake On
		zm918_motor_reg_write(zm918, MOTOR_REG_STRENGTH, zm918->cont_level);		// Strength
		zm918_motor_reg_write(zm918, 0x16, 0x20);		// ZC_MASK_TIME
	} else if (zm918->dts_info.motor_type == ACTUATOR_080935) {
		zm918_motor_reg_write(zm918, MOTOR_REG_MODE_01, 0x45);		// Operationg Mode = Open loop, Brake On
		zm918_motor_reg_write(zm918, MOTOR_REG_STRENGTH, zm918->cont_level);		// Strength
		zm918_motor_reg_write(zm918, 0x16, 0x32);		// ZC_MASK_TIME
	}

	zm918_motor_reg_write(zm918, 0x1E, 0x80);		// WAIT_MAX
	zm918_motor_reg_write(zm918, 0x17, 0x68);		// REF_BR_ADC_VAL
	zm918_motor_reg_write(zm918, 0x18, 0x02);		// BR_LIMIT
	zm918_motor_reg_write(zm918, 0x19, 0x50);		// Start Strength

	if (zm918->dts_info.motor_type == ACTUATOR_1030) {
		zm918_motor_reg_write(zm918, 0x1A, 0x30);		// SEARCH_DRV_RATIO1
		zm918_motor_reg_write(zm918, 0x1B, 0x30);		// SEARCH_DRV_RATIO2
		zm918_motor_reg_write(zm918, 0x1C, 0x30);		// SEARCH_DRV_RATIO3
	} else if (zm918->dts_info.motor_type == ACTUATOR_080935) {
		zm918_motor_reg_write(zm918, 0x1A, 0x38);		// SEARCH_DRV_RATIO1
		zm918_motor_reg_write(zm918, 0x1B, 0x38);		// SEARCH_DRV_RATIO2
		zm918_motor_reg_write(zm918, 0x1C, 0x38);		// SEARCH_DRV_RATIO3
	}

	zm918_motor_reg_write(zm918, 0x1E, 0x80);		// WAIT_MAX
	zm918_motor_reg_write(zm918, MOTOR_REG_DRV_FREQ_H,
							HIGH_8_BITS(zm918->dts_info.drv_freq));		// DRV_FREQ
	zm918_motor_reg_write(zm918, MOTOR_REG_DRV_FREQ_L,
							LOW_8_BITS(zm918->dts_info.drv_freq));		//
	zm918_motor_reg_write(zm918, MOTOR_REG_RESO_FREQ_H,
							HIGH_8_BITS(zm918->dts_info.reso_freq));	// RESO_FREQ
	zm918_motor_reg_write(zm918, MOTOR_REG_RESO_FREQ_L,
							LOW_8_BITS(zm918->dts_info.reso_freq));		//

	if (zm918->dts_info.motor_type == ACTUATOR_1030) {
		zm918_motor_reg_write(zm918, 0x25, 0x14);		// PGA_BEMF_GAIN
	} else if (zm918->dts_info.motor_type == ACTUATOR_080935) {
		zm918_motor_reg_write(zm918, 0x25, 0x03);		// PGA_BEMF_GAIN
		zm918_motor_reg_write(zm918, 0x2B, 0x01);		// PGA_BEMF_GAIN
	}

	zm918_motor_reg_write(zm918, 0x2C, 0x70);		// Over Drive STRENGTH
	zm918_motor_reg_write(zm918, 0x30, 0x7F);		// BR_STRENGTH_P
	zm918_motor_reg_write(zm918, 0x31, 0x7D);		//
	zm918_motor_reg_write(zm918, 0x32, 0x78);		//
	zm918_motor_reg_write(zm918, 0x33, 0x74);		//
	zm918_motor_reg_write(zm918, 0x34, 0x70);		//
	zm918_motor_reg_write(zm918, 0x35, 0x6D);		//
	zm918_motor_reg_write(zm918, 0x36, 0x5D);		//
	zm918_motor_reg_write(zm918, 0x37, 0x50);		// 
	zm918_motor_reg_write(zm918, 0x38, 0x38);		//
	zm918_motor_reg_write(zm918, 0x39, 0x34);		//
	zm918_motor_reg_write(zm918, 0x3A, 0x7F);		// BR_STRENGTH_N
	zm918_motor_reg_write(zm918, 0x3B, 0x7D);		//
	zm918_motor_reg_write(zm918, 0x3C, 0x78);		//
	zm918_motor_reg_write(zm918, 0x3D, 0x74);		//
	zm918_motor_reg_write(zm918, 0x3E, 0x70);		//
	zm918_motor_reg_write(zm918, 0x3F, 0x6D);		//
	zm918_motor_reg_write(zm918, 0x40, 0x5D);		//
	zm918_motor_reg_write(zm918, 0x41, 0x50);		//
	zm918_motor_reg_write(zm918, 0x42, 0x38);		//
	zm918_motor_reg_write(zm918, 0x43, 0x34);		//
	zm918_motor_reg_write(zm918, 0x71, 0x0F);		// cal_freq_range +-15Hz
	zm918_motor_reg_write(zm918, MOTOR_REG_SOFT_EN, 0x00);		// S/W En Low
}

/*
 * input architecture gain setting for waveform data
 */
static void zm918_haptics_set_gain(struct input_dev *dev, u16 gain)
{
	struct zm918 *zm918 = zm918_input_get_drvdata(dev);

	if (zm918->adb_control) {
		zm_info("[VIB] intensity not updated from HAL as adb control is enabled");
		return;
	}

	zm918->gain = gain;
	zm_info("before tuning, gain: 0x%04X", zm918->gain);
	zm918->gain = sec_vib_inputff_tune_gain(&zm918->sec_vib_ddata, zm918->gain);
	zm_info("after tuning, gain: 0x%04X", zm918->gain);
	queue_work(zm918->work_queue, &zm918->set_gain_work);
}

u32 zm918_haptics_get_f0_stored(struct input_dev *dev)
{
	struct zm918 *zm918 = zm918_input_get_drvdata(dev);

	zm_err("enter, get f0 stored");

	return zm918_hz_to_decimal_freq(zm918->f0)*10; // decimal to hz conversion
}

u32 zm918_haptics_set_f0_stored(struct input_dev *dev, u32 val)
{
	struct zm918 *zm918 = zm918_input_get_drvdata(dev);

	zm_err("enter, set f0 stored");

	mutex_lock(&zm918->lock);
	zm918->f0 = val;
	zm918_motor_reg_write(zm918, MOTOR_REG_DRV_FREQ_H, HIGH_8_BITS(zm918->f0)); // DRV_FREQ
	zm918_motor_reg_write(zm918, MOTOR_REG_DRV_FREQ_L, LOW_8_BITS(zm918->f0));	//
	mutex_unlock(&zm918->lock);

	return 0;
}

static const struct sec_vib_inputff_ops zm918_vib_ops = {
	.upload = zm918_haptics_upload_effect,
	.playback = zm918_haptics_playback,
	.set_gain = zm918_haptics_set_gain,
	.erase = zm918_haptics_erase,
	.set_use_sep_index = zm918_set_use_sep_index,
	.get_f0_stored = zm918_haptics_get_f0_stored,
	.set_f0_stored = zm918_haptics_set_f0_stored,
};

static struct attribute_group *zm918_dev_attr_groups[] = {
	&zm918_vibrator_attribute_group,
	NULL
};

static int samsung_zm918_input_init(struct zm918 *zm918)
{
	zm918->sec_vib_ddata.dev = zm918->dev;
	zm918->sec_vib_ddata.vib_ops = &zm918_vib_ops;
	zm918->sec_vib_ddata.vendor_dev_attr_groups = (struct attribute_group **)zm918_dev_attr_groups;
	zm918->sec_vib_ddata.private_data = (void *)zm918;
	zm918->sec_vib_ddata.ff_val = 0;
	zm918->sec_vib_ddata.is_f0_tracking = 1;
	zm918->sec_vib_ddata.use_common_inputff = true;
	sec_vib_inputff_setbit(&zm918->sec_vib_ddata, FF_CONSTANT);
	sec_vib_inputff_setbit(&zm918->sec_vib_ddata, FF_PERIODIC);
	sec_vib_inputff_setbit(&zm918->sec_vib_ddata, FF_CUSTOM);
	sec_vib_inputff_setbit(&zm918->sec_vib_ddata, FF_GAIN);

	return sec_vib_inputff_register(&zm918->sec_vib_ddata);
}

static int zm918_i2c_probe(struct i2c_client *i2c,
			     const struct i2c_device_id *id)
{
	struct zm918 *zm918;
	struct device_node *np = i2c->dev.of_node;
	int ret = -1;
	int err = 0;
	int val = 0;

	zm_info("enter");
	//Check I2c Functionality
	if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C)) {
		zm_err("check_functionality failed");
		return -EIO;
	}

	//Allocate memory to store motor data
	zm918 = devm_kzalloc(&i2c->dev, sizeof(struct zm918), GFP_KERNEL);
	if (zm918 == NULL)
		return -ENOMEM;

	zm918->dev = &i2c->dev;
	zm918->i2c = i2c;

	i2c_set_clientdata(i2c, zm918);

	//Parse Device Tree
	if (np) {
		ret = zm918_parse_dt(&i2c->dev, zm918, np);
		if (ret) {
			zm_err("failed to parse device tree node");
			goto err_parse_dt;
		}
	} 

	//Init regmap
	zm918->regmap = devm_regmap_init_i2c(i2c, &zm918_motor_i2c_regmap);
	if (IS_ERR(zm918->regmap)) {
		err = PTR_ERR(zm918->regmap);
		zm_err("[VIB] Failed to allocate register map: %d\n",err);
		return err;
	}

	//Initialize regulator
	if (zm918->dts_info.regulator_name) {
		zm918->regulator
			= devm_regulator_get(zm918->dev, zm918->dts_info.regulator_name);
		if (IS_ERR(zm918->regulator)) {
			zm_err("[VIB] Failed to get moter power supply.\n");
			return -EFAULT;
		}
		err = regulator_set_voltage(zm918->regulator, MOTOR_VCC, MOTOR_VCC);
		if (err < 0)
			zm_err("[VIB] Failed to set moter power %duV. %d\n", MOTOR_VCC, err);
		err = regulator_enable(zm918->regulator);
		if (err < 0) {
			zm_err("[VIB] %s enable fail(%d)\n", zm918->dts_info.regulator_name, err);
			return -EFAULT;
		} else{
			//vdd power on delay from 0 to 2.5V --> 10ms
			 // So 0 -> 3.0V --> 12 ms 
			//usleep_range(zm918_VDD3P0_ENABLE_DELAY_MIN, zm918_VDD3P0_ENABLE_DELAY_MAX);
			usleep_range(ZM918_VDD3P0_ENABLE_DELAY_MIN, ZM918_VDD3P0_ENABLE_DELAY_MAX);
			zm_info("[VIB] %s enable\n", zm918->dts_info.regulator_name);
		}
	}

	usleep_range(ZM918_BEFORE_INIT_DELAY_MIN, ZM918_BEFORE_INIT_DELAY_MAX);

	//Read F0 cal value
	zm918->f0 = 0;
	val = zm918_motor_reg_read(zm918, 0x4A);
	if (val < 0)
		zm_err("[VIB] read fail for f0 MSB (%d)\n", val);
	else
		zm918->f0 = val << 8;
	val = zm918_motor_reg_read(zm918, 0x4B);
	if (val < 0)
		zm_err("[VIB] read fail for f0 LSB (%d)\n", val);
	else
		zm918->f0 |= val;
	zm_err("[VIB] f0 = 0x%x\n", zm918->f0);

	//Read Motor strength register to check IC i2c working
	err = zm918_motor_reg_read(zm918, MOTOR_REG_STRENGTH);
	if(err < 0){
		zm_err("[VIB] i2c bus fail (%d)\n", err);
		return -EFAULT;
	}else{
		zm_info("[VIB] i2c check - Strength (0x%x)\n", err);
	}

	//initial register settings
	zm918_initial_reg_settings(zm918);

	//Boost IC enable
	if (zm918->dts_info.gpio_en > 0) {
		err = devm_gpio_request_one(zm918->dev, zm918->dts_info.gpio_en,
			GPIOF_DIR_OUT | GPIOF_INIT_HIGH, "motor");
		if (err < 0) {
			zm_err("[VIB] gpio_en request fail %d\n", err);
			return -EFAULT;
		}
		else{
			//10ms for vm rise + 1ms Boost IC start up time
			usleep_range(ZM918_VM_ENABLE_DELAY_MIN, ZM918_VM_ENABLE_DELAY_MAX);
		}
	}
	zm918->gpio_en = zm918->dts_info.gpio_en;

	zm918_vibrator_init(zm918);
	zm918_haptic_init(zm918);

	zm918->work_queue = create_singlethread_workqueue("zm918_vibrator_work_queue");
	if (!zm918->work_queue) {
		zm_err("Error creating zm918_vibrator_work_queue");
		goto err_sysfs;
	}

	INIT_WORK(&zm918->set_gain_work,
		  zm918_haptics_set_gain_work_routine);
	INIT_WORK(&zm918->stop_work,
		  zm918_haptics_stop_work_routine);

	ret = samsung_zm918_input_init(zm918);
	if (ret < 0)
		goto destroy_ff;

	dev_set_drvdata(&i2c->dev, zm918);

	zm_info("probe completed successfully!");
	return 0;

destroy_ff:
err_sysfs:
	//if (gpio_is_valid(zm918->gpio_en))
		//devm_gpio_free(&i2c->dev, zm918->gpio_en);	// temp comment due to build error
err_parse_dt:
	devm_kfree(&i2c->dev, zm918);
	zm918 = NULL;
	return ret;

}

void zm918_shutdown(struct i2c_client *client)
{

}

static const struct i2c_device_id zm918_i2c_id[] = {
	{ ZM918_I2C_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, zm918_i2c_id);

static const struct of_device_id zm918_dt_match[] = {
	{ .compatible = "zm,zm918_haptic" },
	{ },
};

static struct i2c_driver zm918_i2c_driver = {
	.driver = {
		.name = ZM918_I2C_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(zm918_dt_match),
	},
	.probe = zm918_i2c_probe,
	.shutdown 	= zm918_shutdown,
	.id_table = zm918_i2c_id,
};

static int __init zm918_i2c_init(void)
{
	pr_info("[VIB] %s\n", __func__);
	return i2c_add_driver(&zm918_i2c_driver);
}

static void __exit zm918_i2c_exit(void)
{
	i2c_del_driver(&zm918_i2c_driver);
}

module_init(zm918_i2c_init);
module_exit(zm918_i2c_exit);

MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("zm918 Input Haptic driver");
