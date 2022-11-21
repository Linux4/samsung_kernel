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
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/sensor/sensors_core.h>
#include "tmd37xx.h"
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

/* taos debug */
#define MODULE_NAME_PROX "proximity_sensor"
#define taos_dbgmsg(str, args...) pr_info("%s: " str, __func__, ##args)
#define TAOS_DEBUG
#ifdef TAOS_DEBUG
#define gprintk(fmt, x...) \
	pr_info("%s(%d):" fmt, __func__, __LINE__, ## x)
#else
#define gprintk(x...) do { } while (0)
#endif
 
#define VENDOR_NAME	"TAOS"
#if defined(  FEATURE_TMD3700)
#define CHIP_NAME       "TMD3700"
#endif

#if defined(  FEATURE_TMD3782)
#define CHIP_NAME       "TMD3782"
#endif

#define CHIP_ID			0xC0

/* sensor type */
#define LIGHT			0
#define PROXIMITY		1
#define ALL				2

enum {
	LIGHT_ENABLED = BIT(0),
	PROXIMITY_ENABLED = BIT(1),
};

enum {
	STATE_CLOSE = 0,
	STATE_FAR = 1,
};

enum {
	OFF = 0,
	ON = 1,
};
#define Atime_ms		504 /* 50.4 ms */
#define DGF				542//625
#define R_Coef1			(350)//(-580)
#define G_Coef1			(1000)//(1010)
#define B_Coef1			(290)//(80)
#define IR_R_Coef1			(-1)
#define IR_G_Coef1			(109)
#define IR_B_Coef1			(-29)
#define IR_C_Coef1			(57)
#define IR_Coef1			(38)
#define CT_Coef1			(3697)//(2855)
#define CT_Offset1			(1882)//(1973)
#define INTEGRATION_CYCLE	240

#define ADC_BUFFER_NUM	6
#define PROX_AVG_COUNT	40
#define MAX_LUX		150000
#define TAOS_PROX_MAX			1023
#define TAOS_PROX_MIN			0

#define OFFSET_ARRAY_LENGTH		10
#define OFFSET_FILE_PATH	"/efs/prox_cal"

#define CAL_SKIP_ADC	204
#define CAL_FAIL_ADC	480

#ifdef CONFIG_PROX_WINDOW_TYPE
#define WINDOW_TYPE_FILE_PATH "/sys/class/sec/sec_touch_ic/window_type"
#endif




/* driver data */
struct taos_data {
	struct i2c_client *i2c_client;
	struct taos_platform_data *pdata;
	struct input_dev *proximity_input_dev;
	struct input_dev *light_input_dev;
	struct device *light_dev;
	struct device *proximity_dev;
	struct work_struct work_light;
	struct work_struct work_prox;
	struct work_struct work_prox_avg;
	struct mutex prox_mutex;
	struct mutex power_lock;
	struct wake_lock prx_wake_lock;
	struct hrtimer timer;
	struct hrtimer prox_avg_timer;
	struct workqueue_struct *wq;
	struct workqueue_struct *wq_avg;
	ktime_t light_poll_delay;
	ktime_t prox_polling_time;
	u8 power_state;
	int irq;
	bool adc_buf_initialized;
	int adc_value_buf[ADC_BUFFER_NUM];
	int adc_index_count;
	int avg[3];
	int prox_avg_enable;
	s32 clrdata;
	s32 reddata;
	s32 grndata;
	s32 bludata;
	s32 irdata;
	int lux;
/* Auto Calibration */
	u16 offset_value;
	int cal_result;
	int threshold_high;
	int threshold_low;
	int proximity_value;
	bool set_manual_thd;
#ifdef CONFIG_PROX_WINDOW_TYPE
	char windowtype[2];
#endif
	struct regulator *vdd_2p85;
	struct regulator *leda_2p8;
	struct regulator *lvs1_1p8;
	
};
static void taos_thresh_set(struct taos_data *taos);
static int proximity_get_adc(struct taos_data *taos);
static int lightsensor_get_adcvalue(struct taos_data *taos);
static int proximity_open_offset(struct taos_data *data);
static int proximity_adc_read(struct taos_data *taos);
#ifdef CONFIG_PROX_WINDOW_TYPE
static int proximity_open_window_type(struct taos_data *data);
#endif
static int tmd3700_interrupt_clear(struct taos_data *taos);


static int tmd3782_setup_leden_gpio(struct taos_data *info)
{
#ifdef FEATURE_TAOS_CODE

	int rc;
	struct taos_platform_data *pdata = info->pdata;

	rc = gpio_request(pdata->enable, "prox_en");
	if (rc < 0) {
		pr_err("%s: gpio %d request failed (%d)\n",
			__func__, pdata->enable, rc);
	}
	gpio_direction_output(pdata->enable, 1);
	pr_info("%s: gpio %d request success\n", __func__, pdata->enable);
	return rc;
#else
    return 0;
#endif // #ifndef FEATURE_TAOS_CODE

}

static int tmd3782_leden_gpio_onoff(struct taos_data *info, bool onoff)
{
#ifdef FEATURE_TAOS_CODE
	struct taos_platform_data *pdata = info->pdata;
	gpio_set_value(pdata->enable, onoff);
	pr_info("%s onoff:%d\n", __func__, onoff);
	if (onoff)
		msleep(10);
#endif     
	return 0;
}

static void sensor_power_on_vdd(struct taos_data *info, int onoff)
{
	int ret;
	if (!info->lvs1_1p8) {
		info->lvs1_1p8 = regulator_get(&info->i2c_client->dev,
			"reg_vio");
		if(IS_ERR(info->lvs1_1p8)) {
			SENSOR_ERR("regulator_get for lvs1_1p8 failed\n");
		}
	}
	
	if (!info->vdd_2p85) {
		info->vdd_2p85 =
			regulator_get(&info->i2c_client->dev, "reg_vdd");
		if(IS_ERR(info->vdd_2p85)){
			SENSOR_ERR("regulator_get for vdd_2p85 failed\n");
		}
	}
	
	if (onoff == 1) {
		if(!(IS_ERR(info->vdd_2p85))) {
		ret = regulator_enable(info->vdd_2p85);
			if (ret)
				SENSOR_ERR("Failed to enable regulator vdd_2p85.\n");
			}

		
		if(!(IS_ERR(info->lvs1_1p8))) {
		ret = regulator_enable(info->lvs1_1p8);
		if (ret)
			pr_err("%s: Failed to enable regulator lvs1_1p8.\n",
				__func__);
			}
	} else if (onoff == 0) {
		if(!(IS_ERR(info->vdd_2p85))) {
					if (regulator_is_enabled(info->vdd_2p85)) {
						ret = regulator_disable(info->vdd_2p85);
						if (ret)
							SENSOR_ERR("error vdd_2p85 disabling regulator\n");
					}
				}

		if(!(IS_ERR(info->lvs1_1p8))) {
			if (regulator_is_enabled(info->lvs1_1p8)) {
				ret = regulator_disable(info->lvs1_1p8);
				if (ret)
					pr_err("%s: error lvs1_1p8 disabling regulator\n",
						__func__);
			}
		}
	}
	msleep(30);

	return;
}
#ifndef FEATURE_TMD3700 
static int opt_i2c_write_without_cmdreg(struct taos_data *taos, u8 reg, u8 val)
{
	int ret;

	ret = i2c_smbus_write_byte_data(taos->i2c_client,
		 reg, val);

	return ret;
}
#endif // FEATURE_TMD3700

static int opt_i2c_write(struct taos_data *taos, u8 reg, u8 *val)
{
	int ret;

	ret = i2c_smbus_write_byte_data(taos->i2c_client,
		(CMD_REG | reg), *val);

	if(ret)
		SENSOR_ERR(": %d\n", ret);
	return ret;
}
#ifndef FEATURE_TMD3700

static int opt_i2c_read(struct taos_data *taos, u8 reg , u8 *val)
{
	int ret;

	i2c_smbus_write_byte(taos->i2c_client, (CMD_REG | reg));
	ret = i2c_smbus_read_byte(taos->i2c_client);
	*val = ret;

	return ret;
}
static int opt_i2c_write_command(struct taos_data *taos, u8 val)
{
	int ret;

	ret = i2c_smbus_write_byte(taos->i2c_client, val);
	gprintk("[TAOS Command] val=[0x%x] - ret=[0x%x]\n", val, ret);

	return ret;
}
#endif

static int proximity_get_adc(struct taos_data *taos)
{
	int adc = 0;
#ifdef FEATURE_TMD3700
	adc = i2c_smbus_read_byte_data(taos->i2c_client, CMD_REG |PRX_DATA_HIGH  );
#else
	adc = i2c_smbus_read_word_data(taos->i2c_client, CMD_REG | PRX_LO );
#endif 
	if (adc < taos->pdata->prox_rawdata_trim)
		return TAOS_PROX_MIN;
	if (adc > TAOS_PROX_MAX)
		adc = TAOS_PROX_MAX;

	return adc - taos->pdata->prox_rawdata_trim;
}

static int taos_proximity_get_threshold(struct taos_data *taos, u8 buf)
{
#ifndef FEATURE_TMD3700
	u16 threshold;
#else
	int threshold;
#endif

#ifndef FEATURE_TMD3700
	threshold = i2c_smbus_read_word_data(taos->i2c_client, (CMD_REG | buf));
	/*ret =  i2c_smbus_read_i2c_block_data(taos->i2c_client,CMD_REG | CMD_INCR | buf, 2, &threshold);
      if(ret < 0)
		  pr_err("read fail\n");*/
#else
	threshold = i2c_smbus_read_byte_data(taos->i2c_client, (CMD_REG | buf));
#endif 

	pr_err("%s: OKGA threshold = %d,trim = %d,buf=%x\n", __func__,threshold,taos->pdata->prox_rawdata_trim,buf);

#ifndef FEATURE_TMD3700
	if ((threshold == 0xFFFF) || (threshold == 0))
		return (int)threshold;
#else
	if ((threshold == 0xFF) || (threshold == 0))
		return threshold;
#endif
	return (int)threshold - taos->pdata->prox_rawdata_trim;
}


/*static int opt_i2c_blk_read(struct taos_data *taos,
		u8 reg, u8 *val, int size)
{
	s32 ret;
	struct i2c_client *client = chip->client;

	ret =  i2c_smbus_read_i2c_block_data(client,
			CMD_REG | CMD_INCR| reg, size, val);
	if (ret < 0)
		dev_err(&client->dev, "%s: failed at address %x (%d bytes)\n",
				__func__, reg, size);
	return ret;
}*/


#ifndef FEATURE_TMD3700

static int opt_i2c_blk_write(struct taos_data *taos,
		u8 reg, u8 *val, int size)
{
	s32 ret;
	struct i2c_client *client = taos->i2c_client;

	ret =  i2c_smbus_write_i2c_block_data(client,
			CMD_REG | CMD_INCR | reg, size, val);
	if (ret < 0)
		dev_err(&client->dev, "%s: failed at address %x (%d bytes)\n",
				__func__, reg, size);
	return ret;
}

#endif
static void taos_thresh_set(struct taos_data *taos)
{
	int ret = 0;
#ifndef FEATURE_TMD3700
	u8 prox_int_thresh[4] = {0,};
#else
	u8 prox_int_thresh[2] = {0,};
#endif
	u16 trim = (u16)taos->pdata->prox_rawdata_trim;
#ifndef FEATURE_TMD3700
	/* Setting for proximity interrupt */
	if (taos->proximity_value == STATE_CLOSE) {
		prox_int_thresh[0] = ((u16)taos->threshold_low+trim) & 0xFF;
		prox_int_thresh[1] = ((taos->threshold_low+trim) >> 8) & 0xFF;
		prox_int_thresh[2] = (0xFFFF) & 0xFF;
		prox_int_thresh[3] = (0xFFFF >> 8) & 0xFF;
	} else {
		prox_int_thresh[0] = (0x0000) & 0xFF;
		prox_int_thresh[1] = (0x0000 >> 8) & 0xFF;
		prox_int_thresh[2] = (taos->threshold_high+trim) & 0xFF;
		prox_int_thresh[3] = ((taos->threshold_high+trim) >> 8) & 0xFF;
	}

	/*ret = opt_i2c_blk_write(taos, PRX_MINTHRESH,
		&prox_int_thresh[0],
		PRX_MINTHRESHHI - PRX_MINTHRESH + 1);
	
	ret = opt_i2c_blk_write(taos, PRX_MAXTHRESH,
		&prox_int_thresh[2],
		PRX_MAXTHRESHHI - PRX_MAXTHRESH + 1);*/
	ret = opt_i2c_blk_write(taos, PRX_MINTHRESH,
		&prox_int_thresh[0],
	      PRX_MAXTHRESHHI - PRX_MINTHRESH + 1);
	

	/*for (i = 0; i < 4; i++) {
		ret = opt_i2c_write(taos, (CMD_REG|(PRX_MINTHRESH + i)),
			&prox_int_thresh[i]);
		if (ret < 0)
			gprintk("opt_i2c_write failed, err = %d\n", ret);
	}*/
#else
	/* Setting for proximity interrupt */
	if (taos->proximity_value == STATE_CLOSE) {
              // Low -> threshod_low , High -> 0xff 
		 prox_int_thresh[0] = taos->threshold_low+trim;
              prox_int_thresh[1] = 0xFF;
	} else {
	       // Low -> 0, Hight -> threshod_high
		prox_int_thresh[0] = 0;
		prox_int_thresh[1] = taos->threshold_high+trim;
	}

        ret = opt_i2c_write(taos, (CMD_REG|PRX_MINTHRESH),
        	&prox_int_thresh[0]);

        if (ret < 0)
        	gprintk("opt_i2c_write failed, err = %d\n", ret);

        ret = opt_i2c_write(taos, (CMD_REG|(PRX_MAXTHRESH)),
        	&prox_int_thresh[1]);
        
        if (ret < 0)
        	gprintk("opt_i2c_write failed, err = %d\n", ret);

#endif
}

static int taos_chip_on(struct taos_data *taos)
{
#ifdef FEATURE_TMD3700
int ret = 0;
	u8 temp_val;

	tmd3700_interrupt_clear(taos);
		
	tmd3782_leden_gpio_onoff(taos, 1);

	pr_err("%s: \n", __func__);

	temp_val = taos->pdata->prox_thresh_low;
	ret = opt_i2c_write(taos, (CMD_REG|PRX_MINTHRESH), &temp_val);

	temp_val = taos->pdata->prox_thresh_hi;
	ret = opt_i2c_write(taos, (CMD_REG|PRX_MAXTHRESH), &temp_val);


	if (ret < 0)
		gprintk("opt_i2c_write to interrupt reg failed\n");

	temp_val = 0x20;// proximity upper 4bit | als lower 4bit :  proximity cycle 2, als cycle  0 every time
	ret = opt_i2c_write(taos, (CMD_REG|PPERS), &temp_val);
	if (ret < 0)
		gprintk("opt_i2c_write to interrupt reg failed\n");


	 temp_val = 0x86;//ppulse_len: 16us , ppluse : 6
	//temp_val = taos->pdata->prox_pulse_len | taos->pdata->prox_pulse_cnt;
	ret = opt_i2c_write(taos, (CMD_REG|PGCFG0), &temp_val);
	if (ret < 0)
		gprintk("opt_i2c_write to prox cfg reg failed\n");

	//temp_val = taos->pdata->prox_gain;
	 temp_val = 0x90;//pagin 4x , pgldrive 102mA
	ret = opt_i2c_write(taos, (CMD_REG|PGCFG1), &temp_val);
	if (ret < 0)
		gprintk("opt_i2c_write to prox cfg reg failed\n");


	temp_val =  P_TIME_US(20000);//4//20mSec , (pTime +1) * 88us
	pr_err("%s:PTIME %x  \n", __func__, temp_val);

	ret = opt_i2c_write(taos, (CMD_REG|PRX_RATE), &temp_val);
	if (ret < 0)
		gprintk("opt_i2c_write to prox cfg reg failed\n");


	temp_val = 0x7f;//// one-shot autozero
	ret = opt_i2c_write(taos, (CMD_REG|AZ_CONFIG), &temp_val);
	if (ret < 0)
		gprintk("opt_i2c_write to prox cfg reg failed\n");


	temp_val = 0x20; //asein|psien|pien|aien|cien|0|0|0
	ret = opt_i2c_write(taos, (CMD_REG |INTENAB), &temp_val);

	temp_val = 0x00; //wtime 2.8 msec
	ret = opt_i2c_write(taos, (CMD_REG |WAIT_TIME), &temp_val);

	temp_val = CMD_pen | CMD_pon |CMD_aen |CMD_wen; // wen|pen|aen|pon, Proximity , als , poweron  enable
	ret = opt_i2c_write(taos, (CMD_REG), &temp_val);
	if (ret < 0)
		gprintk("opt_i2c_write to clr ctrl reg failed\n");

	pr_err("%s:thresh_min %x , thresh_max %x , \n", __func__, taos->pdata->prox_thresh_low,taos->pdata->prox_thresh_hi	);

	return ret;
#else
	int ret = 0;
	u8 temp_val;
	u8 reg_cntrl;
	tmd3782_leden_gpio_onoff(taos, 1);
	temp_val = CNTL_PWRON;
	ret = opt_i2c_write(taos, (CMD_REG|CNTRL), &temp_val);
	if (ret < 0)
		gprintk("opt_i2c_write to clr ctrl reg failed\n");
 
	temp_val = taos->pdata->als_time;
	ret = opt_i2c_write(taos, (CMD_REG|ALS_TIME), &temp_val);
	if (ret < 0)
		gprintk("opt_i2c_write to als time reg failed\n");

	temp_val = 0xff;
	ret = opt_i2c_write(taos, (CMD_REG|WAIT_TIME), &temp_val);
	if (ret < 0)
		gprintk("opt_i2c_write to wait time reg failed\n");

	temp_val = taos->pdata->intr_filter;
	ret = opt_i2c_write(taos, (CMD_REG|INTERRUPT), &temp_val);
	if (ret < 0)
		gprintk("opt_i2c_write to interrupt reg failed\n");

	temp_val = 0x0;
	ret = opt_i2c_write(taos, (CMD_REG|PRX_CFG), &temp_val);
	if (ret < 0)
		gprintk("opt_i2c_write to prox cfg reg failed\n");

	temp_val = taos->pdata->prox_pulsecnt;
	ret = opt_i2c_write(taos, (CMD_REG|PRX_COUNT), &temp_val);
	if (ret < 0)
		gprintk("opt_i2c_write to prox cnt reg failed\n");

	temp_val = taos->pdata->als_gain;
	ret = opt_i2c_write(taos, (CMD_REG|GAIN), &temp_val);
	if (ret < 0)
		gprintk("opt_i2c_write to prox gain reg failed\n");

	reg_cntrl = CNTL_INTPROXPON_ENBL;
	ret = opt_i2c_write(taos, (CMD_REG|CNTRL), &reg_cntrl);
	if (ret < 0)
		gprintk("opt_i2c_write to ctrl reg failed\n");

	return ret;
#endif	
}

static int taos_chip_off(struct taos_data *taos)
{
#ifdef FEATURE_TMD3700
	int ret = 0;
	u8 reg_cntrl;

	pr_err("%s: \n", __func__);

	reg_cntrl = CNTL_REG_CLEAR; //asein|psien|pien|aien|cien|0|0|0
	ret = opt_i2c_write(taos, (CMD_REG |INTENAB), &reg_cntrl);
	if (ret < 0) {
		gprintk("opt_i2c_write to ctrl reg failed\n");
		return ret;
	}

	//ret = i2c_smbus_read_byte_data(taos->i2c_client, CMD_REG | STATUS);//read status 
	//pr_err("%s  status [%x] \n", __func__, ret);

	reg_cntrl = CNTL_REG_CLEAR;
	ret = opt_i2c_write(taos, (CMD_REG), &reg_cntrl);
	if (ret < 0) {
		gprintk("opt_i2c_write to ctrl reg failed\n");
		return ret;
	}

	tmd3700_interrupt_clear(taos);

	/*sensor_power_on_vdd(taos,OFF);*/
	/*tmd3700_leden_gpio_onoff(taos, OFF);*/

	return ret;

#else
	int ret = 0;
	u8 reg_cntrl;

	reg_cntrl = CNTL_REG_CLEAR;
	ret = opt_i2c_write(taos, (CMD_REG | CNTRL), &reg_cntrl);
	if (ret < 0) {
		gprintk("opt_i2c_write to ctrl reg failed\n");
		return ret;
	}

	/*sensor_power_on_vdd(taos,OFF);*/
	/*tmd3782_leden_gpio_onoff(taos, OFF);*/

	return ret;
#endif	
}

static int taos_get_cct(struct taos_data *taos)
{
	int bp1 = taos->bludata - taos->irdata;
	int rp1 = taos->reddata - taos->irdata;
	int cct = 0;

	if (rp1 != 0)
		cct = CT_Coef1 * bp1 / rp1 + CT_Offset1;

	return cct;
}

static int taos_get_lux(struct taos_data *taos)
{
	s32 rp1, gp1, bp1;
	s32 clrdata = 0;
	s32 reddata = 0;
	s32 grndata = 0;
	s32 bludata = 0;
	s32 calculated_lux = 0;
	u8 reg_gain = 0x0;
	u16 temp_gain = 0x0;
	int gain = 1;
	int ret = 0;

	temp_gain = i2c_smbus_read_word_data(taos->i2c_client,
		(CMD_REG | CFG1));
	if(temp_gain < 0)
		SENSOR_ERR("temp_gain: %d\n", temp_gain);
	reg_gain = temp_gain & 0xff;

	clrdata = i2c_smbus_read_word_data(taos->i2c_client,
		(CMD_REG | CLR_CHAN0LO));
	reddata = i2c_smbus_read_word_data(taos->i2c_client,
		(CMD_REG | RED_CHAN1LO));
	grndata = i2c_smbus_read_word_data(taos->i2c_client,
		(CMD_REG | GRN_CHAN1LO));
	bludata = i2c_smbus_read_word_data(taos->i2c_client,
		(CMD_REG | BLU_CHAN1LO));

	if(clrdata < 0 || reddata < 0 || grndata < 0 || bludata < 0)
		SENSOR_ERR("err: %d, %d, %d, %d\n", clrdata, reddata, grndata, bludata);

	taos->clrdata = clrdata;
	taos->reddata = reddata;
	taos->grndata = grndata;
	taos->bludata = bludata;

	switch (reg_gain & 0x03) {
	case 0x00:
		gain = 1;
		break;
	case 0x01:
		gain = 4;
		break;
	case 0x02:
		gain = 16;
		break;
/*	case 0x03:
		gain = 64;
		break;
*/
	default:
		break;
	}

	if (gain == 1 && clrdata < 25) {
		reg_gain = 0x22;
		ret = opt_i2c_write(taos, (CMD_REG | CFG1), &reg_gain);
		if (ret < 0)
			SENSOR_ERR("opt_i2c_write failed, err = %d\n", ret);
		return taos->lux;
	} else if (gain == 16 && clrdata > 15000) {
		reg_gain = 0x20;
		ret = opt_i2c_write(taos, (CMD_REG | CFG1), &reg_gain);
		if (ret < 0)
			SENSOR_ERR("opt_i2c_write failed, err = %d\n", ret);
		return taos->lux;
	}

	if ((clrdata >= 18500) && (gain == 1)) {
		calculated_lux = MAX_LUX;
		return calculated_lux;
	}

	/* calculate lux */
	taos->irdata = (reddata + grndata +
		bludata - clrdata) /2;

	/* remove ir from counts*/
	rp1 = taos->reddata - taos->irdata;
	gp1 = taos->grndata - taos->irdata;
	bp1 = taos->bludata - taos->irdata;

	calculated_lux = (rp1 * R_Coef1 + gp1 * G_Coef1 + bp1 * B_Coef1) / 1000;

	if (calculated_lux < 0)
		calculated_lux = 0;
	else {
		/* divide by CPL, CPL = (Atime_ms * ALS_GAIN / DGF); */
		calculated_lux = calculated_lux*DGF;
		calculated_lux *= 10; /* Atime_ms */
		calculated_lux /= Atime_ms;
		calculated_lux /= gain;
	}

	taos->lux = (int)calculated_lux;
	//SENSOR_ERR("Lux = %d\n",taos->lux);	
	return taos->lux;
}

static void taos_light_enable(struct taos_data *taos)
{
	taos_dbgmsg("starting poll timer, delay %lldns\n",
	ktime_to_ns(taos->light_poll_delay));
	taos_get_lux(taos);
	hrtimer_start(&taos->timer, taos->light_poll_delay, HRTIMER_MODE_REL);
}

static void taos_light_disable(struct taos_data *taos)
{
	taos_dbgmsg("cancelling poll timer\n");
	hrtimer_cancel(&taos->timer);
	cancel_work_sync(&taos->work_light);
}

static ssize_t poll_delay_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct taos_data *taos = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%lld\n",
		ktime_to_ns(taos->light_poll_delay));
}


static ssize_t poll_delay_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct taos_data *taos = dev_get_drvdata(dev);
	int64_t new_delay;
	int err;

	err = kstrtoll(buf, 10, &new_delay);
	if (err < 0)
		return err;

	taos_dbgmsg("new delay = %lldns, old delay = %lldns\n",
		    new_delay, ktime_to_ns(taos->light_poll_delay));
	mutex_lock(&taos->power_lock);
	if (new_delay != ktime_to_ns(taos->light_poll_delay)) {
		taos->light_poll_delay = ns_to_ktime(new_delay);
		if (taos->power_state & LIGHT_ENABLED) {
			taos_light_disable(taos);
			taos_light_enable(taos);
		}
	}
	mutex_unlock(&taos->power_lock);

	return size;
}

static ssize_t light_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct taos_data *taos = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%d\n",
		       (taos->power_state & LIGHT_ENABLED) ? 1 : 0);
}

static ssize_t proximity_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct taos_data *taos = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%d\n",
		       (taos->power_state & PROXIMITY_ENABLED) ? 1 : 0);
}

static ssize_t light_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct taos_data *taos = dev_get_drvdata(dev);
	bool new_value;


	if (sysfs_streq(buf, "1")) {
		new_value = true;
	} else if (sysfs_streq(buf, "0")) {
		new_value = false;
	} else {
		pr_err("%s: invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	mutex_lock(&taos->power_lock);
	taos_dbgmsg("new_value = %d, old state = %d\n",
		    new_value, (taos->power_state & LIGHT_ENABLED) ? 1 : 0);
	if (new_value && !(taos->power_state & LIGHT_ENABLED)) {
		if (!taos->power_state) {
			taos_chip_on(taos);
			msleep(60); /*more than 58 ms*/
		}
		taos->power_state |= LIGHT_ENABLED;
		taos_light_enable(taos);
	} else if (!new_value && (taos->power_state & LIGHT_ENABLED)) {
		taos_light_disable(taos);
		taos->power_state &= ~LIGHT_ENABLED;
		if (!taos->power_state)
			taos_chip_off(taos);
	}
	mutex_unlock(&taos->power_lock);
	return size;
}

static int tmd3700_interrupt_clear(struct taos_data *taos)
{
	int ret;
	int val =0 ;

      val = 0xfa;//asein|psien|pien|aien|cien|0|0|0
	ret = i2c_smbus_write_byte_data(taos->i2c_client, CMD_REG | STATUS, val);
	
	if(ret)
		SENSOR_ERR(": %d\n", ret);
	return 0;
}



static ssize_t proximity_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct taos_data *taos = dev_get_drvdata(dev);
	bool new_value;
	int ret = 0;

	if (sysfs_streq(buf, "1")) {
		new_value = true;
	} else if (sysfs_streq(buf, "0")) {
		new_value = false;
	} else {
		pr_err("%s: invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	mutex_lock(&taos->power_lock);
	taos_dbgmsg("new_value = %d, old state = %d\n",
		    new_value, (taos->power_state & PROXIMITY_ENABLED) ? 1 : 0);
	if (new_value && !(taos->power_state & PROXIMITY_ENABLED)) {
		if (taos->set_manual_thd == false) {
			ret = proximity_open_offset(taos);
			if (ret < 0 && ret != -ENOENT)
				pr_err("%s: proximity_open_offset() failed\n",
				__func__);
#ifdef CONFIG_PROX_WINDOW_TYPE
			ret = proximity_open_window_type(taos);
#endif
			taos->threshold_high = taos->pdata->prox_thresh_hi
				+ taos->offset_value;
			taos->threshold_low = taos->pdata->prox_thresh_low
				+ taos->offset_value;
			pr_err("%s: th_hi = %d, th_low = %d\n", __func__,
				taos->threshold_high, taos->threshold_low);
		}

		if (!taos->power_state)
			taos_chip_on(taos);

		taos->power_state |= PROXIMITY_ENABLED;
		taos->proximity_value = STATE_FAR;
		taos_thresh_set(taos);

		/* interrupt clearing */
              // if CHIP == TMD3782 
              #ifndef FEATURE_TMD3700
              temp = (CMD_REG|CMD_SPL_FN|CMD_PROXALS_INTCLR);
		 ret = opt_i2c_write_command(taos, temp);
              #else
		 ret= tmd3700_interrupt_clear(taos);
              #endif 
		if (ret < 0)
			SENSOR_ERR("opt_i2c_write failed, err = %d\n", ret);

		input_report_abs(taos->proximity_input_dev, ABS_DISTANCE, 1);
		input_sync(taos->proximity_input_dev);

		enable_irq(taos->irq);
		enable_irq_wake(taos->irq);

	} else if (!new_value && (taos->power_state & PROXIMITY_ENABLED)) {
		disable_irq_wake(taos->irq);
		disable_irq(taos->irq);

		taos->power_state &= ~PROXIMITY_ENABLED;
		if (!taos->power_state)
			taos_chip_off(taos);
	}
	mutex_unlock(&taos->power_lock);
	return size;
}

static ssize_t proximity_state_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct taos_data *taos = dev_get_drvdata(dev);
	int adc = 0;

	adc = proximity_get_adc(taos);

	return snprintf(buf, PAGE_SIZE, "%d\n", adc);

}

#ifdef CONFIG_PROX_WINDOW_TYPE
static void change_proximity_default_threshold(struct taos_data *data)
{
	int trim = data->pdata->prox_rawdata_trim;
	switch (data->windowtype[1]) {
	case WINTYPE_WHITE:
		data->pdata->prox_thresh_hi = WHITEWINDOW_HI_THRESHOLD-trim;
		data->pdata->prox_thresh_low = WHITEWINDOW_LOW_THRESHOLD-trim;
		break;
	case WINTYPE_OTHERS:
		data->pdata->prox_thresh_hi = BLACKWINDOW_HI_THRESHOLD-trim;
		data->pdata->prox_thresh_low = BLACKWINDOW_LOW_THRESHOLD-trim;
		break;
	default:
		data->pdata->prox_thresh_hi = data->pdata->prox_thresh_hi;
		data->pdata->prox_thresh_low = data->pdata->prox_thresh_low;
		break;
	}
}

static int proximity_open_window_type(struct taos_data *data)
{
	struct file *wintype_filp = NULL;
	int err = 0;
	mm_segment_t old_fs;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	wintype_filp = filp_open(WINDOW_TYPE_FILE_PATH, O_RDONLY, 0666);
	if (IS_ERR(wintype_filp)) {
		pr_err("%s: no window_type file\n", __func__);
		err = PTR_ERR(wintype_filp);
		if (err != -ENOENT)
			pr_err("%s: Can't open window_type file\n", __func__);
		set_fs(old_fs);

		data->windowtype[0] = 0;
		data->windowtype[1] = 0;
		goto exit;
	}

	err = wintype_filp->f_op->read(wintype_filp,
		(u8 *)&data->windowtype, sizeof(u8) * 2, &wintype_filp->f_pos);
	if (err != sizeof(u8) * 2) {
		pr_err("%s: Can't read the window_type data from file\n",
			__func__);
		err = -EIO;
	}

	pr_err("%s: %c%c\n",
		__func__, data->windowtype[0], data->windowtype[1]);
	filp_close(wintype_filp, current->files);
	set_fs(old_fs);
exit:
	change_proximity_default_threshold(data);
	return err;
}
#endif

static int proximity_open_offset(struct taos_data *data)
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
			pr_err("%s: Can't open offset file\n", __func__);
		set_fs(old_fs);
		return err;
	}

	err = offset_filp->f_op->read(offset_filp,
		(char *)&data->offset_value, sizeof(u16), &offset_filp->f_pos);
	if (err != sizeof(u16)) {
		pr_err("%s: Can't read the offset data from file\n", __func__);
		err = -EIO;
	}

	pr_err("%s: data->offset_value = %d\n",
		__func__, data->offset_value);
	filp_close(offset_filp, current->files);
	set_fs(old_fs);

	return err;
}

static int proximity_adc_read(struct taos_data *taos)
{
	int sum[OFFSET_ARRAY_LENGTH];
	int i = 0;
	int avg = 0;
	int min = 0;
	int max = 0;
	int total = 0;

	mutex_lock(&taos->prox_mutex);
	for (i = 0; i < OFFSET_ARRAY_LENGTH; i++) {
		usleep_range(11000, 12000);
		sum[i] = proximity_get_adc(taos);
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
	mutex_unlock(&taos->prox_mutex);
	total -= (min + max);
	avg = (int)(total / (OFFSET_ARRAY_LENGTH - 2));

	return avg;
}

static int proximity_store_offset(struct device *dev, bool do_calib)
{
	struct taos_data *taos = dev_get_drvdata(dev);
	struct file *offset_filp = NULL;
	mm_segment_t old_fs;
	int err = 0;
	u16 abnormal_ct = proximity_adc_read(taos);
	u16 offset = 0;

	if (do_calib) {
		/* tap offset button */
		pr_info("[SENSOR] %s: calibration start\n", __func__);
		if (abnormal_ct < CAL_SKIP_ADC) {
			taos->offset_value = 0;
			taos->threshold_high = taos->pdata->prox_thresh_hi;
			taos->threshold_low = taos->pdata->prox_thresh_low;
			taos_thresh_set(taos);
			taos->set_manual_thd = false;
			taos->cal_result = 2;
			pr_info("%s: crosstalk < %d, skip calibration\n",
				__func__, CAL_SKIP_ADC);
		} else if ((abnormal_ct >= CAL_SKIP_ADC)
			&& (abnormal_ct <= CAL_FAIL_ADC)) {
			offset = abnormal_ct / 2;
			taos->offset_value = offset;
			taos->threshold_high = taos->pdata->prox_thresh_hi
				+ offset;
			taos->threshold_low = taos->pdata->prox_thresh_low
				+ offset;
			taos_thresh_set(taos);
			taos->set_manual_thd = false;
			taos->cal_result = 1;
		} else {
			taos->offset_value = 0;
			taos->threshold_high = taos->pdata->prox_thresh_hi;
			taos->threshold_low = taos->pdata->prox_thresh_low;
			taos_thresh_set(taos);
			taos->set_manual_thd = false;
			taos->cal_result = 0;
			pr_info("%s: crosstalk > %d, calibration failed\n",
				__func__, CAL_FAIL_ADC);
		}
	} else {
		/* tap reset button */
		pr_info("%s: reset\n", __func__);
		taos->threshold_high = taos->pdata->prox_thresh_hi;
		taos->threshold_low = taos->pdata->prox_thresh_low;
		taos_thresh_set(taos);
		taos->offset_value = 0;
		taos->cal_result = 2;
		taos->set_manual_thd = false;
	}
	pr_info("%s: abnormal_ct : %d, offset : %d\n", __func__, abnormal_ct,
		taos->offset_value);
	/* store offset in file */
	old_fs = get_fs();
	set_fs(KERNEL_DS);

	offset_filp = filp_open(OFFSET_FILE_PATH,
			O_CREAT | O_TRUNC | O_WRONLY, 0666);
	if (IS_ERR(offset_filp)) {
		pr_err("%s: Can't open prox_offset file\n", __func__);
		set_fs(old_fs);
		err = PTR_ERR(offset_filp);
		return err;
	}

	err = offset_filp->f_op->write(offset_filp,
		(char *)&taos->offset_value, sizeof(u16), &offset_filp->f_pos);
	if (err != sizeof(u16))
		pr_err("%s: Can't write the offset data to file\n", __func__);

	filp_close(offset_filp, current->files);
	set_fs(old_fs);

	return 1;
}

static ssize_t proximity_cal_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
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

	err = proximity_store_offset(dev, do_calib);
	if (err < 0) {
		pr_err("%s: proximity_store_offset() failed\n", __func__);
		return err;
	}

	return size;
}

static ssize_t proximity_cal_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct taos_data *taos = dev_get_drvdata(dev);

	proximity_open_offset(taos);

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n",
		taos->offset_value, taos->threshold_high, taos->threshold_low);
}

static ssize_t prox_offset_pass_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct taos_data *taos = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", taos->cal_result);
}

static ssize_t proximity_avg_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{

	struct taos_data *taos = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n",
		taos->avg[0], taos->avg[1], taos->avg[2]);
}

static ssize_t proximity_avg_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct taos_data *taos = dev_get_drvdata(dev);
	int new_value = 0;

	if (sysfs_streq(buf, "1")) {
		new_value = true;
	} else if (sysfs_streq(buf, "0")) {
		new_value = false;
	} else {
		pr_err("%s: invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}
	if (taos->prox_avg_enable == new_value)
		taos_dbgmsg("%s same status\n", __func__);
	else if (new_value == 1) {
		taos_dbgmsg("starting poll timer, delay %lldns\n",
		ktime_to_ns(taos->prox_polling_time));
		hrtimer_start(&taos->prox_avg_timer,
			taos->prox_polling_time, HRTIMER_MODE_REL);
		taos->prox_avg_enable = 1;
	} else {
		taos_dbgmsg("cancelling prox avg poll timer\n");
		hrtimer_cancel(&taos->prox_avg_timer);
		cancel_work_sync(&taos->work_prox_avg);
		taos->prox_avg_enable = 0;
	}

	return 1;
}

static ssize_t proximity_thresh_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct taos_data *taos = dev_get_drvdata(dev);
	int thresh_hi = 0;

	msleep(20);
	thresh_hi = taos_proximity_get_threshold(taos, PRX_MAXTHRESH);

	pr_err("%s: THRESHOLD = %d\n", __func__, thresh_hi);

	return snprintf(buf, PAGE_SIZE, "prox_threshold = %d\n", thresh_hi);
}

static ssize_t proximity_thresh_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct taos_data *taos = dev_get_drvdata(dev);
	int thresh_value = (u8)(taos->pdata->prox_thresh_hi);
	int err = 0;

	err = kstrtoint(buf, 10, &thresh_value);
	pr_err("%s, value = %d\n", __func__, thresh_value);
	if (err < 0)
		pr_err("%s, kstrtoint failed.", __func__);

	taos->threshold_high = thresh_value;
	taos_thresh_set(taos);
	msleep(20);

	return size;
}
static ssize_t thresh_high_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct taos_data *taos = dev_get_drvdata(dev);
	int thresh_hi = 0, thresh_low = 0;

	msleep(20);
	thresh_low = taos_proximity_get_threshold(taos, PRX_MINTHRESH);
	thresh_hi = taos_proximity_get_threshold(taos, PRX_MAXTHRESH);

	pr_err("%s: thresh_hi = %d, thresh_low = %d\n", __func__, thresh_hi,
		thresh_low);

	return snprintf(buf, PAGE_SIZE, "%d,%d\n", thresh_hi, thresh_low);
}

static ssize_t thresh_high_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct taos_data *taos = dev_get_drvdata(dev);
	int thresh_value = (u8)(taos->pdata->prox_thresh_hi);
	int err = 0;

	err = kstrtoint(buf, 10, &thresh_value);
	pr_err("%s, thresh_value = %d\n", __func__, thresh_value);
	if (err < 0)
		pr_err("%s, kstrtoint failed.", __func__);

	taos->threshold_high = thresh_value;
	taos_thresh_set(taos);
	msleep(20);
	taos->set_manual_thd = true;

	return size;
}
static ssize_t thresh_low_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct taos_data *taos = dev_get_drvdata(dev);
	int thresh_hi = 0, thresh_low = 0;

	msleep(20);
	thresh_hi = taos_proximity_get_threshold(taos, PRX_MAXTHRESH);
	thresh_low = taos_proximity_get_threshold(taos, PRX_MINTHRESH);

	pr_err("%s: thresh_hi = %d, thresh_low = %d\n", __func__, thresh_hi,
		thresh_low);

	return snprintf(buf, PAGE_SIZE, "%d,%d\n", thresh_hi, thresh_low);
}

static ssize_t thresh_low_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct taos_data *taos = dev_get_drvdata(dev);
	int thresh_value = (u8)(taos->pdata->prox_thresh_low);
	int err = 0;

	err = kstrtoint(buf, 10, &thresh_value);
	pr_err("%s, thresh_value = %d\n", __func__, thresh_value);
	if (err < 0)
		pr_err("%s, kstrtoint failed.", __func__);

	taos->threshold_low = thresh_value;
	taos_thresh_set(taos);
	msleep(20);
	taos->set_manual_thd = true;

	return size;
}
static ssize_t get_vendor_name(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR_NAME);
}

static ssize_t get_chip_name(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", CHIP_NAME);
}

static DEVICE_ATTR(vendor, S_IRUGO, get_vendor_name, NULL);
static DEVICE_ATTR(name, S_IRUGO, get_chip_name, NULL);

static ssize_t lightsensor_file_state_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct taos_data *taos = dev_get_drvdata(dev);
	int adc = 0;
	adc = lightsensor_get_adcvalue(taos);

	return snprintf(buf, PAGE_SIZE, "%d\n", adc);
}

static ssize_t lightsensor_raw_data_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct taos_data *taos = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u,%u,%u,%u\n",
		taos->reddata, taos->grndata, taos->bludata, taos->clrdata);
}

static struct device_attribute dev_attr_light_raw_data =
	__ATTR(raw_data, S_IRUGO, lightsensor_raw_data_show, NULL);


static DEVICE_ATTR(adc, S_IRUGO, lightsensor_file_state_show, NULL);
static DEVICE_ATTR(lux, S_IRUGO, lightsensor_file_state_show, NULL);
static DEVICE_ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
		   poll_delay_show, poll_delay_store);

static struct device_attribute dev_attr_light_enable =
	__ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
	       light_enable_show, light_enable_store);

static struct attribute *light_sysfs_attrs[] = {
	&dev_attr_light_enable.attr,
	&dev_attr_poll_delay.attr,
	NULL
};

static struct attribute_group light_attribute_group = {
	.attrs = light_sysfs_attrs,
};

static struct device_attribute *lightsensor_additional_attributes[] = {
	&dev_attr_adc,
	&dev_attr_lux,
	&dev_attr_vendor,
	&dev_attr_name,
	&dev_attr_light_raw_data,
	NULL
};

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


static struct device_attribute dev_attr_proximity_raw_data =
	__ATTR(raw_data, S_IRUGO, proximity_state_show, NULL);

static DEVICE_ATTR(prox_cal, S_IRUGO | S_IWUSR, proximity_cal_show,
	proximity_cal_store);
static DEVICE_ATTR(prox_avg, S_IRUGO|S_IWUSR, proximity_avg_show,
	proximity_avg_store);
static DEVICE_ATTR(state, S_IRUGO, proximity_state_show, NULL);
static DEVICE_ATTR(prox_offset_pass, S_IRUGO,
	prox_offset_pass_show, NULL);
static DEVICE_ATTR(prox_thresh, 0644, proximity_thresh_show,
	proximity_thresh_store);
static DEVICE_ATTR(thresh_high, 0644, thresh_high_show,
	thresh_high_store);
static DEVICE_ATTR(thresh_low, 0644, thresh_low_show,
	thresh_low_store);
static struct device_attribute *prox_sensor_attrs[] = {
	&dev_attr_state,
	&dev_attr_prox_avg,
	&dev_attr_vendor,
	&dev_attr_name,
	&dev_attr_proximity_raw_data,
	&dev_attr_prox_cal,
	&dev_attr_prox_offset_pass,
	&dev_attr_prox_thresh,
	&dev_attr_thresh_high,
	&dev_attr_thresh_low,
	NULL
};

static int lightsensor_get_adcvalue(struct taos_data *taos)
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
	value = taos_get_lux(taos);

	adc_index = (taos->adc_index_count++) % ADC_BUFFER_NUM;

	/*ADC buffer initialize (light sensor off ---> light sensor on) */
	if (!taos->adc_buf_initialized) {
		taos->adc_buf_initialized = true;
		for (j = 0; j < ADC_BUFFER_NUM; j++)
			taos->adc_value_buf[j] = value;
	} else
		taos->adc_value_buf[adc_index] = value;

	adc_max = taos->adc_value_buf[0];
	adc_min = taos->adc_value_buf[0];

	for (i = 0; i < ADC_BUFFER_NUM; i++) {
		adc_total += taos->adc_value_buf[i];

		if (adc_max < taos->adc_value_buf[i])
			adc_max = taos->adc_value_buf[i];

		if (adc_min > taos->adc_value_buf[i])
			adc_min = taos->adc_value_buf[i];
	}
	adc_avr_value = (adc_total-(adc_max+adc_min))/(ADC_BUFFER_NUM - 2);

	if (taos->adc_index_count == ADC_BUFFER_NUM)
		taos->adc_index_count = 0;

	return adc_avr_value;
}


static void taos_work_func_light(struct work_struct *work)
{
	struct taos_data *taos = container_of(work, struct taos_data,
					      work_light);
#if 1	
	int adc = taos_get_lux(taos);
	int cct = taos_get_cct(taos);
	//int prox_adc = proximity_get_adc(taos);

        //gprintk("#>[LIGHT] lux=[%d] cct=[%d]\n", adc, cct);
	  //gprintk("#>[Proximity] pData=[%d]\n", prox_adc);
			
	input_report_rel(taos->light_input_dev, REL_MISC, adc + 1);
	input_report_rel(taos->light_input_dev, REL_WHEEL, cct);
	input_sync(taos->light_input_dev);
#else
	int adc = proximity_get_adc(taos);
	        gprintk("#>[Proximity] pData=[%d]\n", adc);
#endif

}



static void taos_work_func_prox(struct work_struct *work)
{
	struct taos_data *taos =
		container_of(work, struct taos_data, work_prox);
	int adc_data;
	int threshold_high;
	int threshold_low;
	//u8 chipid = 0x69;
	//int ret = 0;
	//int i = 0;
	int proximity_value = 0;

	/* disable INT */
	disable_irq_nosync(taos->irq);

	/*while (chipid != 0x69 && i < 10) {
		msleep(20);
		ret = opt_i2c_read(taos, CHIPID, &chipid);
		i++;
	}
	if (ret < 0)
		gprintk("opt_i2c_read failed, err = %d\n", ret);
     */
     
	/* change Threshold */
	mutex_lock(&taos->prox_mutex);
	adc_data = proximity_get_adc(taos);
	mutex_unlock(&taos->prox_mutex);
#ifndef FEATURE_TMD3700
	threshold_high = taos_proximity_get_threshold(taos, PRX_MAXTHRESHHI);
	threshold_low = taos_proximity_get_threshold(taos, PRX_MINTHRESHLO);
#else
	threshold_high = taos_proximity_get_threshold(taos, PRX_MAXTHRESH);
	threshold_low = taos_proximity_get_threshold(taos, PRX_MINTHRESH);
#endif 
	pr_err("%s: hi = %d, low = %d, adc_data = %d\n", __func__,
		taos->threshold_high, taos->threshold_low, adc_data);


	if ((taos->proximity_value ==	STATE_FAR) &&
		(adc_data >=  (taos->threshold_high))) {
		proximity_value = STATE_CLOSE;
		input_report_abs(taos->proximity_input_dev,
			ABS_DISTANCE, proximity_value);
		input_sync(taos->proximity_input_dev);
		pr_info("[%s] prox value = %d\n", __func__, proximity_value);
	} 
	else if ((taos->proximity_value ==	STATE_CLOSE) &&
             (adc_data <= (taos->threshold_low)))
	{
		proximity_value = STATE_FAR;
		input_report_abs(taos->proximity_input_dev,
			ABS_DISTANCE, proximity_value);
		input_sync(taos->proximity_input_dev);
		pr_info("[%s] prox value = %d\n", __func__, proximity_value);
	} else {
		pr_err("[%s]Error Case!adc=[%X], th_high=[%d], th_min=[%d]\n",
			__func__, adc_data, threshold_high, threshold_low);
		goto exit;
	}

       if(proximity_value == STATE_CLOSE)
            gprintk("[PROX] CLOSE pdata=[%d] \n", adc_data);
       else if (proximity_value == STATE_FAR)
            gprintk("[PROX] FAR pdata=[%d] \n", adc_data);
       else 
            gprintk("[PROX] Unknown pdata=[%d] \n", adc_data);

	taos->proximity_value = proximity_value;
	//mutex_lock(&taos->prox_mutex);
	taos_thresh_set(taos);
	//mutex_unlock(&taos->prox_mutex);
	/* reset Interrupt pin */
	/* to active Interrupt, TMD2771x Interuupt pin shoud be reset. */
exit:
#ifndef FEATURE_TMD3700
	i2c_smbus_write_byte(taos->i2c_client,
	(CMD_REG|CMD_SPL_FN|CMD_PROXALS_INTCLR));
#else
	tmd3700_interrupt_clear(taos);
#endif 
	/* enable INT */
	enable_irq(taos->irq);
}

static void taos_work_func_prox_avg(struct work_struct *work)
{
	struct taos_data *taos = container_of(work, struct taos_data,
		work_prox_avg);
	int proximity_value = 0;
	int min = 0, max = 0, avg = 0;
	int i = 0;

	for (i = 0; i < PROX_AVG_COUNT; i++) {
		mutex_lock(&taos->prox_mutex);

		proximity_value = proximity_get_adc(taos);
		mutex_unlock(&taos->prox_mutex);
		if (proximity_value > TAOS_PROX_MIN) {
			avg += proximity_value;
			if (!i)
				min = proximity_value;
			if (proximity_value < min)
				min = proximity_value;
			if (proximity_value > max)
				max = proximity_value;
		} else {
			proximity_value = TAOS_PROX_MIN;
		}
		msleep(40);
	}
	avg /= i;
	taos->avg[0] = min;
	taos->avg[1] = avg;
	taos->avg[2] = max;
}


/* This function is for light sensor.  It operates every a few seconds.
 * It asks for work to be done on a thread because i2c needs a thread
 * context (slow and blocking) and then reschedules the timer to run again.
 */
static enum hrtimer_restart taos_timer_func(struct hrtimer *timer)
{
	struct taos_data *taos = container_of(timer, struct taos_data, timer);
	queue_work(taos->wq, &taos->work_light);
	hrtimer_forward_now(&taos->timer, taos->light_poll_delay);
	return HRTIMER_RESTART;
}

static enum hrtimer_restart taos_prox_timer_func(struct hrtimer *timer)
{
	struct taos_data *taos = container_of(timer, struct taos_data,
		prox_avg_timer);
	queue_work(taos->wq_avg, &taos->work_prox_avg);
	hrtimer_forward_now(&taos->prox_avg_timer, taos->prox_polling_time);

	return HRTIMER_RESTART;
}


/* interrupt happened due to transition/change of near/far proximity state */
irqreturn_t taos_irq_handler(int irq, void *data)
{
	struct taos_data *ip = data;

	if (ip->irq != -1) {
		wake_lock_timeout(&ip->prx_wake_lock, 3*HZ);
		queue_work(ip->wq, &ip->work_prox);
	}
	pr_err("taos interrupt handler is called\n");
	return IRQ_HANDLED;
}

static int taos_setup_irq(struct taos_data *taos)
{
	int rc = -EIO;
	struct taos_platform_data *pdata = taos->pdata;
	int irq;

	taos_dbgmsg("start\n");
	
        rc = gpio_request(pdata->als_int, "gpio_proximity_out");
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

	rc = request_threaded_irq(irq, NULL, taos_irq_handler,
		IRQF_TRIGGER_FALLING |IRQF_ONESHOT, 
		      "proximity_int", taos);
	if (rc < 0) {
		pr_err("%s: request_irq(%d) failed for gpio %d (%d)\n",
			__func__, irq,
			pdata->als_int, rc);
		goto err_request_irq;
	}

	/* start with interrupts disabled */
	disable_irq(irq);
	taos->irq = irq;

	taos_dbgmsg("success\n");

	goto done;

err_request_irq:
err_gpio_direction_input:
	gpio_free(pdata->als_int);
done:
	return rc;
}

static int taos_get_initial_offset(struct taos_data *taos)
{
	int ret = 0;
	u8 p_offset = 0;

	ret = proximity_open_offset(taos);
	if (ret < 0) {
		p_offset = 0;
		taos->offset_value = 0;
	} else
		p_offset = taos->offset_value;

	pr_err("%s: initial offset = %d\n", __func__, p_offset);

	return p_offset;
}

#ifdef CONFIG_OF
/* device tree parsing function */
static int taos_parse_dt(struct device *dev, struct  taos_platform_data *pdata)
{
	int ret = 0;
	struct device_node *np = dev->of_node;

	pdata->als_int = of_get_named_gpio_flags(np, "taos,irq_gpio",
		0, &pdata->als_int_flags);

	pdata->enable = of_get_named_gpio(np, "taos,en", 0);

	ret = of_property_read_u32(np, "taos,prox_rawdata_trim",
		&pdata->prox_rawdata_trim);

	ret = of_property_read_u32(np, "taos,prox_thresh_hi",
		&pdata->prox_thresh_hi);
	ret = of_property_read_u32(np, "taos,prox_thresh_low",
		&pdata->prox_thresh_low);

	ret = of_property_read_u32(np, "taos,als_time",
		&pdata->als_time);
	ret = of_property_read_u32(np, "taos,intr_filter",
		&pdata->intr_filter);
	ret = of_property_read_u32(np, "taos,prox_pulsecnt",
		&pdata->prox_pulsecnt);
	ret = of_property_read_u32(np, "taos,als_gain",
		&pdata->als_gain);
	ret = of_property_read_u32(np, "taos,coef_atime",
		&pdata->coef_atime);
	ret = of_property_read_u32(np, "taos,ga",
		&pdata->ga);
	ret = of_property_read_u32(np, "taos,coef_a",
		&pdata->coef_a);
	ret = of_property_read_u32(np, "taos,coef_b",
		&pdata->coef_b);
	ret = of_property_read_u32(np, "taos,coef_c",
		&pdata->coef_c);
	ret = of_property_read_u32(np, "taos,coef_d",
		&pdata->coef_d);

	pr_info("%s irq_gpio:%d and enable gpio %d\n", __func__,
		pdata->als_int, pdata->enable);
	return 0;
}
#else
static int taos_parse_dt(struct device *dev,struct taos_platform_data *pdata)
{
	return -ENODEV;
}
#endif

static int taos_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret = -ENODEV, err;
	int chipid , revid = 0;
	struct input_dev *input_dev;
	struct taos_data *taos;
	struct taos_platform_data *pdata = NULL;


	pr_info("%s Start\n", __func__);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s: i2c functionality check failed!\n", __func__);
		return ret;
	}
	taos = kzalloc(sizeof(struct taos_data), GFP_KERNEL);
	if (!taos) {
		pr_err("%s: failed to alloc memory for module data\n",
		       __func__);
		ret = -ENOMEM;
		goto done;
	}

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
			sizeof(struct taos_platform_data), GFP_KERNEL);
		if (!pdata) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			ret = -ENOMEM;
			goto err_taos_data_free;
		}
		err = taos_parse_dt(&client->dev, pdata);
		if (err)
			goto err_devicetree;
	} else
	pdata = client->dev.platform_data;
	
	if (!pdata) {
		pr_err("%s: missing pdata!\n", __func__);
		goto err_taos_data_free;
	}


	taos->pdata = pdata;
	taos->i2c_client = client;
	i2c_set_clientdata(client, taos);
	taos->lux = 0;
	taos->offset_value = taos_get_initial_offset(taos);
#ifdef CONFIG_PROX_WINDOW_TYPE
	proximity_open_window_type(taos);
#endif
	taos->set_manual_thd = false;
	sensor_power_on_vdd(taos, 1);
	ret = tmd3782_setup_leden_gpio(taos);
	if (ret) {
		pr_err("%s: could not setup leden_gpio\n", __func__);
		goto err_setup_leden_gpio;
	}
	tmd3782_leden_gpio_onoff(taos, 1);
	/* ID Check */
	chipid = i2c_smbus_read_byte_data(client, CMD_REG | CHIPID);
       /*!> Need to check DEVID and VID */    
	taos_dbgmsg(" ID register = %x \n",chipid);

	revid = i2c_smbus_read_byte_data(client, CMD_REG | REVID);
	   /*!> Need to check DEVID and VID */	  
	taos_dbgmsg(" REV register = %x \n",revid);


//#ifndef FEATURE_TODO
#if 0
       if (chipid != CHIP_ID) {
		pr_err("%s: i2c read error [%X]\n", __func__, chipid);
		goto err_chip_id_or_i2c_error;
	}
//#endif // FEATURE_TODO
#endif

	taos->threshold_high = taos->pdata->prox_thresh_hi + taos->offset_value;
	taos->threshold_low = taos->pdata->prox_thresh_low + taos->offset_value;

	mutex_init(&taos->prox_mutex);

	/* wake lock init */
	wake_lock_init(&taos->prx_wake_lock, WAKE_LOCK_SUSPEND,
		       "prx_wake_lock");
	mutex_init(&taos->power_lock);

	/* allocate proximity input_device */
	input_dev = input_allocate_device();
	if (!input_dev) {
		pr_err("%s: could not allocate input device\n", __func__);
		goto err_input_allocate_device_proximity;
	}
	taos->proximity_input_dev = input_dev;
	input_set_drvdata(input_dev, taos);
	input_dev->name = "proximity_sensor";
	input_set_capability(input_dev, EV_ABS, ABS_DISTANCE);
	input_set_abs_params(input_dev, ABS_DISTANCE, 0, 1, 0, 0);

	taos_dbgmsg("registering proximity input device\n");
	ret = input_register_device(input_dev);
	if (ret < 0) {
		pr_err("%s: could not register input device\n", __func__);
		input_free_device(input_dev);
		goto err_input_register_device_proximity;
	}

	ret = sensors_register(taos->proximity_dev, taos, prox_sensor_attrs,
		MODULE_NAME_PROX); /* factory attributs */
	if (ret < 0) {
		pr_err("%s: could not registersensors_register\n", __func__);
		input_unregister_device(input_dev);
		goto err_input_register_device_proximity;
	}
	ret = sensors_create_symlink(&input_dev->dev.kobj, input_dev->name);
	if (ret < 0) {
		input_unregister_device(input_dev);
		sensors_unregister(taos->proximity_dev, prox_sensor_attrs);
		goto err_input_register_device_proximity;
	}
	ret = sysfs_create_group(&input_dev->dev.kobj,
		&proximity_attribute_group);
	if (ret < 0) {
		pr_err("%s: could not create sysfs group\n", __func__);
		input_unregister_device(input_dev);

		sensors_unregister(taos->proximity_dev, prox_sensor_attrs);
		sensors_remove_symlink(&input_dev->dev.kobj,
			taos->proximity_input_dev->name);
		goto err_input_register_device_proximity;
	}

	/* hrtimer settings.  we poll for light values using a timer. */
	hrtimer_init(&taos->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	taos->light_poll_delay = ns_to_ktime(200 * NSEC_PER_MSEC);
	taos->timer.function = taos_timer_func;

	/* the timer just fires off a work queue request.  we need a thread
	   to read the i2c (can be slow and blocking). */
	taos->wq = create_singlethread_workqueue("taos_wq");
	if (!taos->wq) {
		ret = -ENOMEM;
		pr_err("%s: could not create workqueue\n", __func__);
		goto err_create_workqueue;
	}

	taos->wq_avg = create_singlethread_workqueue("taos_wq_avg");
	if (!taos->wq_avg) {
		ret = -ENOMEM;
		pr_err("%s: could not create workqueue\n", __func__);
		goto err_create_avg_workqueue;
	}

	/* this is the thread function we run on the work queue */
	INIT_WORK(&taos->work_light, taos_work_func_light);
	INIT_WORK(&taos->work_prox, taos_work_func_prox);
	INIT_WORK(&taos->work_prox_avg, taos_work_func_prox_avg);
	taos->prox_avg_enable = 0;

	hrtimer_init(&taos->prox_avg_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	taos->prox_polling_time = ns_to_ktime(2000 * NSEC_PER_MSEC);
	taos->prox_avg_timer.function = taos_prox_timer_func;

	/* allocate lightsensor-level input_device */
	input_dev = input_allocate_device();
	if (!input_dev) {
		pr_err("%s: could not allocate input device\n", __func__);
		ret = -ENOMEM;
		goto err_input_allocate_device_light;
	}
	input_set_drvdata(input_dev, taos);
	input_dev->name = "light_sensor";
	input_set_capability(input_dev, EV_REL, REL_MISC);
	input_set_capability(input_dev, EV_REL, REL_WHEEL);

	taos_dbgmsg("registering lightsensor-level input device\n");
	ret = input_register_device(input_dev);
	if (ret < 0) {
		pr_err("%s: could not register input device\n", __func__);
		input_free_device(input_dev);
		goto err_input_register_device_light;
	}

	ret = sensors_register(taos->light_dev, taos,
		lightsensor_additional_attributes, "light_sensor");
	if (ret < 0) {
		pr_err("%s: cound not register light sensor device(%d).\n",
			__func__, ret);
		input_unregister_device(input_dev);
		goto err_input_register_device_light;
	}
	ret = sensors_create_symlink(&input_dev->dev.kobj, input_dev->name);
	if (ret < 0) {
		input_unregister_device(input_dev);
		sensors_unregister(taos->light_dev,
			lightsensor_additional_attributes);
		goto out_sensor_register_failed1;
	}
	ret = sysfs_create_group(&input_dev->dev.kobj, &light_attribute_group);
	if (ret < 0) {
		pr_err("%s: could not create sysfs group\n", __func__);
		input_unregister_device(input_dev);

		sensors_unregister(taos->light_dev,
			lightsensor_additional_attributes);
		sensors_remove_symlink(&taos->light_input_dev->dev.kobj,
			taos->proximity_input_dev->name);
		goto out_sensor_register_failed1;
	}
	taos->light_input_dev = input_dev;
	ret = taos_setup_irq(taos);
	if (ret < 0) {
		pr_err("%s: could not setup irq\n", __func__);
		goto err_setup_irq;
	}
	/*sensor_power_on_vdd(taos,0);*/
    tmd3782_leden_gpio_onoff(taos, OFF);
    printk("#> probe OK \n");
	goto done;
	/* error, unwind it all */
err_devicetree:
	pr_err("%s error in device tree\n", __func__);
out_sensor_register_failed1:
	sensors_unregister(taos->light_dev, lightsensor_additional_attributes);
err_setup_irq:

err_input_register_device_light:
err_input_allocate_device_light:
	destroy_workqueue(taos->wq_avg);
err_create_avg_workqueue:
	destroy_workqueue(taos->wq);
err_create_workqueue:
	sysfs_remove_group(&taos->proximity_input_dev->dev.kobj,
			   &proximity_attribute_group);
err_input_register_device_proximity:
err_input_allocate_device_proximity:
	free_irq(taos->irq, 0);
	gpio_free(taos->pdata->als_int);
	mutex_destroy(&taos->power_lock);
	wake_lock_destroy(&taos->prx_wake_lock);
//err_chip_id_or_i2c_error:
err_setup_leden_gpio:
	gpio_free(taos->pdata->enable);
err_taos_data_free:
	kfree(taos);

done:
	return ret;
}

static int taos_suspend(struct device *dev)
{
	/* We disable power only if proximity is disabled.  If proximity
	   is enabled, we leave power on because proximity is allowed
	   to wake up device.  We remove power without changing
	   taos->power_state because we use that state in resume.
	*/
	struct i2c_client *client = to_i2c_client(dev);
	struct taos_data *taos = i2c_get_clientdata(client);

	disable_irq(taos->irq);

	if (taos->power_state & LIGHT_ENABLED)
		taos_light_disable(taos);
	if (taos->power_state == LIGHT_ENABLED)
		taos_chip_off(taos);

	pr_info("%s, is called\n", __func__);

	return 0;
}

static int taos_resume(struct device *dev)
{
	/* Turn power back on if we were before suspend. */
	struct i2c_client *client = to_i2c_client(dev);
	struct taos_data *taos = i2c_get_clientdata(client);

	if (taos->power_state == LIGHT_ENABLED)
		taos_chip_on(taos);
	if (taos->power_state & LIGHT_ENABLED)
		taos_light_enable(taos);

	enable_irq(taos->irq);

	pr_info("%s, is called\n", __func__);

	return 0;
}

static int taos_i2c_remove(struct i2c_client *client)
{
	struct taos_data *taos = i2c_get_clientdata(client);

	sensors_unregister(taos->proximity_dev, prox_sensor_attrs);
	sensors_remove_symlink(&taos->proximity_input_dev->dev.kobj,
		taos->proximity_input_dev->name);
	sysfs_remove_group(&taos->proximity_input_dev->dev.kobj,
		&proximity_attribute_group);
	input_unregister_device(taos->light_input_dev);

	sensors_unregister(taos->light_dev, lightsensor_additional_attributes);
	sensors_remove_symlink(&taos->light_input_dev->dev.kobj,
		taos->proximity_input_dev->name);
	sysfs_remove_group(&taos->light_input_dev->dev.kobj,
		&light_attribute_group);
	input_unregister_device(taos->proximity_input_dev);
	free_irq(taos->irq, NULL);
	gpio_free(taos->pdata->als_int);
	if (taos->power_state) {
		taos->power_state = 0;
		if (taos->power_state & LIGHT_ENABLED)
			taos_light_disable(taos);
		//taos->pdata->power(false);
		sensor_power_on_vdd(taos, 0);
		tmd3782_leden_gpio_onoff(taos, 0);
		gpio_free(taos->pdata->enable);
	}
	destroy_workqueue(taos->wq);
	destroy_workqueue(taos->wq_avg);
	mutex_destroy(&taos->power_lock);
	wake_lock_destroy(&taos->prx_wake_lock);
	kfree(taos);
	return 0;
}

static const struct i2c_device_id taos_device_id[] = {
	{"taos", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, taos_device_id);

static const struct dev_pm_ops taos_pm_ops = {
	.suspend = taos_suspend,
	.resume = taos_resume
};
#ifdef CONFIG_OF
static struct of_device_id tm37xx_match_table[] = {
	{ .compatible = "taos,tmd3700",},
	{ .compatible = "taos,tmd3782",},
};
#else
#define tm37xx_match_table NULL
#endif
static struct i2c_driver taos_i2c_driver = {
	.driver = {
		.name = "taos",
		.owner = THIS_MODULE,
		.pm = &taos_pm_ops,
		.of_match_table = tm37xx_match_table,
	},
	.probe		= taos_i2c_probe,
	.remove		= taos_i2c_remove,
	.id_table	= taos_device_id,
};


static int __init taos_init(void)
{
	printk("taos_init\n");
	return i2c_add_driver(&taos_i2c_driver);
}

static void __exit taos_exit(void)
{
	i2c_del_driver(&taos_i2c_driver);
}

module_init(taos_init);
module_exit(taos_exit);

MODULE_AUTHOR("SAMSUNG");
MODULE_DESCRIPTION("Optical Sensor driver for taos");
MODULE_LICENSE("GPL");
