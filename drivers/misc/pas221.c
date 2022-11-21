/* linux/driver/input/misc/pas221.c
 * Copyright (C) 2012 Partron Co., Ltd. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/i2c.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <linux/gpio.h>
#include <linux/wakelock.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/uaccess.h>
#include <linux/pas221.h>
#include <linux/regulator/consumer.h>
#if defined(CONFIG_MACH_HENDRIX)
#include <mach/mfp-pxa986-hendrix.h>
#endif
#define LIGHT_BUFFER_NUM	5
#define PROX_READ_NUM		5	/*40*/

#define TAG	"PAS221"

#define B_PROX_EN 				(BIT(5))
#define B_PROX_SLP_400MS		(0)
#define B_PROX_SLP_100MS		(BIT(0)<<2)
#define B_PROX_SLP_50MS			(BIT(1)<<2)
#define B_PROX_SLP_0MS			(7<<2)

#define B_PROX_IRDR_DRV_62_5MA	(BIT(0))
#define B_PROX_IRDR_DRV_125MA	(BIT(1))
#define B_PROX_IRDR_DRV_250MA	(BIT(1)|BIT(0))
#define B_INT_ALG_WINDOW_COMPARATOR (BIT(7))
#define B_INT_ALG_HYSTRESIS_WINDOW	(0)
#define B_PROX_OFFSET(x)		((x)<<3)
#define B_ALS_EN				(BIT(2))
#define B_ALS_RANGE_125			(0)
#define B_ALS_RANGE_250			(1)
#define	B_ALS_RANGE_2000		(2)
#define B_ALS_RANGE_4000		(3)

#define PROX_DET_OFFSET			20
#define PROX_HYS_OFFSET			18
#define XTALK_GAIN				24
#define RECALC_THRESHOLD		5
#define PS_UNDER_SUNLIGHT 		(XTALK_GAIN + RECALC_THRESHOLD)
#define PS_CPST_ALS_THRES		5000

#define B_INT_PROX_FLAG			(BIT(7))
#define B_INT_ALS_FLAG			(BIT(3))
#define B_ALS_INT_PRST_MASK		((0x3 << 1)^0xff)
#define B_ALS_INT_PRST(x)		((x)<< 1)
#define B_PROX_PRST(x)			((x)<<5)

#define DEBUG

#define OFFSET_FILE_PATH        "/efs/prox_cal"
#define CHIP_NAME       "PAS221"

enum {
	REGS_RESERVED=0x00,
	REGS_CONFIG0,
	REGS_CONFIG1,
	REGS_CONFIG2,
	REGS_INTCONFIG,
	REGS_PROX_INT_TL,
	REGS_PROX_INT_TH,
	REGS_ALS_INT_TL,
	REGS_ALS_INT_TLH,
	REGS_ALS_INT_TH,
	REGS_PROX_DATA,
	REGS_ALS_DATA_HB,
	REGS_ALS_DATA_LB,
	REGS_PROX_AMBIR,
	REGS_TESTMODEENB,
	REGS_TESTREG1=0x10,
};

static const u8 reg_defaults[16] = {
	0x00,
	B_PROX_EN | B_PROX_SLP_50MS | B_PROX_IRDR_DRV_250MA,
	B_INT_ALG_HYSTRESIS_WINDOW | B_PROX_OFFSET(0) | B_ALS_EN | B_ALS_RANGE_4000,
	0x00,
	B_PROX_PRST(1),	
	0x00, 
	0xff, 
	0x00, 
	0x0f, 
	0xff, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
	0x00, 
};

static const int adc_table[4] = {
	15,
	150,
	1500,
	15000,
};

enum {
	LIGHT_ENABLED = BIT(0),
	PROXIMITY_ENABLED = BIT(1),
}; 

enum ps_power_mode { ps_enable, ps_disable };
enum als_power_mode { als_enable, als_disable };

static u8 m_als_range = 3;

static struct regulator *VPROX_3_0_V;
static struct regulator *VPROX_LED_3_0_V;

/* driver data */
struct pas221_data {
	struct input_dev *proximity_input_dev;
	struct input_dev *light_input_dev;
	struct i2c_client *i2c_client;
	struct work_struct work_light;
	struct work_struct work_prox;
	struct hrtimer light_timer;
	struct hrtimer prox_timer;
	struct mutex power_lock;
	struct wake_lock prx_wake_lock;
	struct workqueue_struct *light_wq;
	struct workqueue_struct *prox_wq;
	struct class *lightsensor_class;
	struct class *proximity_class;
	struct device *switch_cmd_dev;
	struct device *proximity_dev;
	struct pas221_platform_data *pdata;
	bool als_buf_initialized;
	bool on;
	int als_index_count;
	int irq;
	int avg[3];
	int light_count; 
	int light_buffer;
	ktime_t light_poll_delay;
	ktime_t prox_poll_delay;
	u8 power_state;

	int poffset;
	u8 prox_hys[3];
	u8 xtalk;
};

extern struct class *sensors_class;

static int pas221_i2c_read8(struct pas221_data *pas221, u8 cmd, u8 *val)
{
	int err = 0;
	int retry = 10;
	struct i2c_client *client = pas221->i2c_client;

	if ((client == NULL) || (!client->adapter))
		return -ENODEV;

	while (retry--) {
		err = i2c_smbus_read_i2c_block_data(client, cmd, 1, val);
		if (err >= 0)
			return err;
	}

	return err;
}

static int pas221_i2c_read16(struct pas221_data *pas221, u8 cmd, u8 *val)
{
	int err = 0;
	int retry = 10;
	struct i2c_client *client = pas221->i2c_client;

	if ((client == NULL) || (!client->adapter))
		return -ENODEV;

	while (retry--) {
		err = i2c_smbus_read_i2c_block_data(client, cmd, 2, val);
		if (err >= 0)
			return err;
	}

	return err;
}

static int pas221_i2c_readn(struct pas221_data *pas221, u8 cmd, u8 *val, u8 cnt)
{
	int err = 0;
	int retry = 10;
	struct i2c_client *client = pas221->i2c_client;

	if ((client == NULL) || (!client->adapter))
		return -ENODEV;

	while (retry--) {
		err = i2c_smbus_read_i2c_block_data(client, cmd, cnt, val);
		if (err >= 0)
			return err;
	}

	return err;
}

int pas221_i2c_write(struct pas221_data *pas221, u8 cmd, u8 val)
{
	u8 data[2]={0, };
	int err = 0;
	int retry = 10;
	struct i2c_msg msg[1];
	struct i2c_client *client = pas221->i2c_client;

	if ((client == NULL) || (!client->adapter))
		return -ENODEV;

	data[0]=cmd;
	data[1]=val;

	msg->addr = client->addr;
	msg->flags = 0;
	msg->len = 2;
	msg->buf = data;

	while (retry--) {
		err = i2c_transfer(client->adapter, msg, 1);
		if (err >= 0)
			return err;
	}

	return err;
}

static void pas221_initial_reg_ps_off_als_on(struct pas221_data *pas221)
{
	pas221_i2c_write(pas221, REGS_CONFIG0, reg_defaults[REGS_CONFIG0]&(0x20^0xff));
	pas221_i2c_write(pas221, REGS_CONFIG1, reg_defaults[REGS_CONFIG1]);
	pas221_i2c_write(pas221, REGS_CONFIG2, reg_defaults[REGS_CONFIG2]);
	pas221_i2c_write(pas221, REGS_INTCONFIG, reg_defaults[REGS_INTCONFIG]);
	pas221_i2c_write(pas221, REGS_PROX_INT_TL, reg_defaults[REGS_PROX_INT_TL]);
	pas221_i2c_write(pas221, REGS_PROX_INT_TH, reg_defaults[REGS_PROX_INT_TH]);
	pas221_i2c_write(pas221, REGS_ALS_INT_TL, reg_defaults[REGS_ALS_INT_TL]);
	pas221_i2c_write(pas221, REGS_ALS_INT_TLH, reg_defaults[REGS_ALS_INT_TLH]);
	pas221_i2c_write(pas221, REGS_ALS_INT_TH, reg_defaults[REGS_ALS_INT_TH]);
}

static void pas221_initial_reg_ps_on_als_on(struct pas221_data *pas221)
{
		pas221_i2c_write(pas221, REGS_CONFIG0, reg_defaults[REGS_CONFIG0]);
		pas221_i2c_write(pas221, REGS_CONFIG1, reg_defaults[REGS_CONFIG1]);
		pas221_i2c_write(pas221, REGS_CONFIG2, reg_defaults[REGS_CONFIG2]);
		pas221_i2c_write(pas221, REGS_INTCONFIG, reg_defaults[REGS_INTCONFIG]);
		pas221_i2c_write(pas221, REGS_PROX_INT_TL, reg_defaults[REGS_PROX_INT_TL]);
		pas221_i2c_write(pas221, REGS_PROX_INT_TH, reg_defaults[REGS_PROX_INT_TH]);
		pas221_i2c_write(pas221, REGS_ALS_INT_TL, reg_defaults[REGS_ALS_INT_TL]);
		pas221_i2c_write(pas221, REGS_ALS_INT_TLH, reg_defaults[REGS_ALS_INT_TLH]);
		pas221_i2c_write(pas221, REGS_ALS_INT_TH, reg_defaults[REGS_ALS_INT_TH]);
}


static void pas221_initial_reg_ps_on_als_off(struct pas221_data *pas221)
{
	pas221_i2c_write(pas221, REGS_CONFIG0, reg_defaults[REGS_CONFIG0]);//PS ON
	pas221_i2c_write(pas221, REGS_CONFIG2, reg_defaults[REGS_CONFIG2]);
	pas221_i2c_write(pas221, REGS_INTCONFIG, reg_defaults[REGS_INTCONFIG]);
	pas221_i2c_write(pas221, REGS_PROX_INT_TL, reg_defaults[REGS_PROX_INT_TL]);
	pas221_i2c_write(pas221, REGS_PROX_INT_TH, reg_defaults[REGS_PROX_INT_TH]);
	pas221_i2c_write(pas221, REGS_ALS_INT_TL, reg_defaults[REGS_ALS_INT_TL]);
	pas221_i2c_write(pas221, REGS_ALS_INT_TLH, reg_defaults[REGS_ALS_INT_TLH]);
	pas221_i2c_write(pas221, REGS_ALS_INT_TH, reg_defaults[REGS_ALS_INT_TH]);
}


inline static void pas221_interrupt_clear(struct pas221_data *pas221)
{
	pas221_i2c_write(pas221, REGS_INTCONFIG, reg_defaults[REGS_INTCONFIG]);
}


static void pas221_change_prox_offset(struct pas221_data *pas221, u8 offset_comp)
{
	u8 offset = offset_comp&(0xf<<3);
	pas221_i2c_write(pas221, REGS_CONFIG1, (reg_defaults[REGS_CONFIG1]&((0x0f<<3)^0xff))|offset);
}

static void pas221_reg_ps_als_enable(struct pas221_data *pas221, enum ps_power_mode ps_en, enum als_power_mode als_en)
{
	if(ps_en==ps_enable)
		pas221_i2c_write(pas221, REGS_CONFIG0, reg_defaults[REGS_CONFIG0]);//PS ON
	else 
		pas221_i2c_write(pas221, REGS_CONFIG0, reg_defaults[REGS_CONFIG0]&(0x20^0xff));//PS OFF

	if(als_en==als_enable)
		pas221_i2c_write(pas221, REGS_CONFIG1, reg_defaults[REGS_CONFIG1]);//ALS ON
	else
		pas221_i2c_write(pas221, REGS_CONFIG1, reg_defaults[REGS_CONFIG1]&(0x02^0xff));//ALS OFF
}

static void pas221_light_enable(struct pas221_data *pas221)
{
#ifdef DEBUG
	pr_err(TAG"  [%s]\t", __func__);
#endif
	pas221->light_count = 0;
	pas221->light_buffer = 0;
	pas221_initial_reg_ps_off_als_on(pas221);
	hrtimer_start(&pas221->light_timer, pas221->light_poll_delay,
						HRTIMER_MODE_REL);
}

static void pas221_light_disable(struct pas221_data *pas221)
{
#ifdef DEBUG
	pr_err(TAG"  [%s]\t", __func__);
#endif
	pas221_reg_ps_als_enable(pas221, ps_disable, als_disable);
	hrtimer_cancel(&pas221->light_timer);
	cancel_work_sync(&pas221->work_light);
}

static int lightsensor_get_alsvalue(struct pas221_data *pas221)
{
	int value = 0;
	u8 als_value[2] = {0, };
	u8 ir_value = 0;
	int adc_value;
	int x = 0, k = 0;

	/* get ALS */
	pas221_i2c_read16(pas221, REGS_ALS_DATA_HB, als_value);
	adc_value = ((als_value[0]<<8) | als_value[1]);
	value = (adc_value>=1) ? --adc_value : 0 ;

	/* get IR */
	pas221_i2c_read8(pas221, REGS_PROX_AMBIR, &ir_value);
	ir_value = (ir_value>>1)&0x7f;

	if( ir_value>48 && value>17 && value<2000 )	{
		x = ((ir_value-48)*1000000)/value;
		x = x/4;
	} else {
		x = 0;
	}

	if( x>=0 && x<=4000 ) k = 100;
	else if( x>4000 && x<=20000 ) k = 115;
	else if( x>20000 ) k = 150;

	value = (value*k)/100;

	switch(m_als_range)
	{
		case 3:// 4000 lux
			value *= 572; value /=117;	
			//value *= 4.884;
			break;
		case 2: // 2000 lux
			value *= 286; value /=117; 
			//value *= 2.442
			break;
		case 1: // 250 lux
			value *= 36; value /=117; 
			//value *= 0.3053
			break;
		case 0: // 128 lux
			value *= 32; value /=204; ;
			//0.156
			break;
	}

	if(value==4) 
	{
		pr_err(TAG" ALS adc_value:%x, value:%d", adc_value, value);
	}
	return value;
}

static void proxsensor_get_avgvalue(struct pas221_data *pas221)
{
	int min = 0, max = 0, avg = 0;
	int i;
	u8 proximity_value = 0;

	for (i = 0; i < PROX_READ_NUM; i++)	{
		msleep(101);
		pas221_i2c_read8(pas221, REGS_PROX_DATA, &proximity_value);
		avg += proximity_value;
	
		if (!i)
			min = proximity_value;
		else if (proximity_value < min)
			min = proximity_value;
	
		if (proximity_value > max)
			max = proximity_value;
	}
	avg /= PROX_READ_NUM;

	pas221->avg[0] = min;
	pas221->avg[1] = avg;
	pas221->avg[2] = max;
}

static ssize_t proximity_state_show(struct device *dev,
	struct device_attribute *attr, char *buf) 
{
	struct pas221_data *pas221 = dev_get_drvdata(dev);
	u8 proximity_value = 0;
	int poffset = pas221->poffset;

	if (!(pas221->power_state & PROXIMITY_ENABLED)) {
		mutex_lock(&pas221->power_lock);
		pas221->pdata->proximity_power(1);
		pas221_initial_reg_ps_on_als_on(pas221);
		mutex_unlock(&pas221->power_lock);
	}

	msleep(20);
	pas221_i2c_read8(pas221, REGS_PROX_DATA, &proximity_value);

	if (!(pas221->power_state & PROXIMITY_ENABLED)) {
		mutex_lock(&pas221->power_lock);
		pas221_reg_ps_als_enable(pas221, ps_disable, als_enable);
		pas221->pdata->proximity_power(0);
		mutex_unlock(&pas221->power_lock);
	}
	if(proximity_value - poffset > 0) proximity_value-=poffset;
	else proximity_value = 0;
	return sprintf(buf, "%d", proximity_value);
}

static ssize_t lightsensor_file_state_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct pas221_data *pas221 = dev_get_drvdata(dev);
	int adc = 0;
	
	if (!(pas221->power_state & LIGHT_ENABLED))
		pas221_light_enable(pas221);

	adc = lightsensor_get_alsvalue(pas221);

	if (!(pas221->power_state & LIGHT_ENABLED))
		pas221_light_disable(pas221);

return sprintf(buf, "%d\n", adc);
}

static ssize_t poll_delay_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct pas221_data *pas221 = dev_get_drvdata(dev);
	return sprintf(buf, "%lld\n", ktime_to_ns(pas221->light_poll_delay));
}

static ssize_t poll_delay_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct pas221_data *pas221 = dev_get_drvdata(dev);
	int64_t new_delay;
	int err;
	
	err = strict_strtoll(buf, 10, &new_delay);
	if (err < 0)
		return err;

	printk(KERN_INFO "[PAS221] poll_delay_store : new_delay=%lld\n", new_delay);

	mutex_lock(&pas221->power_lock);
	if (new_delay != ktime_to_ns(pas221->light_poll_delay)) {
		pas221->light_poll_delay = ns_to_ktime(new_delay);
		if (pas221->power_state & LIGHT_ENABLED) {
			pas221_light_disable(pas221);
			pas221_light_enable(pas221);
		}
	}
	mutex_unlock(&pas221->power_lock);

	return size;
}

static ssize_t light_enable_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct pas221_data *pas221 = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n",
		       (pas221->power_state & LIGHT_ENABLED) ? 1 : 0);
}

static ssize_t proximity_enable_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct pas221_data *pas221 = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n",
		       (pas221->power_state & PROXIMITY_ENABLED) ? 1 : 0);
}

static ssize_t light_enable_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	struct pas221_data *pas221 = dev_get_drvdata(dev);
	bool new_value;
	if (sysfs_streq(buf, "1"))
		new_value = true;
	else if (sysfs_streq(buf, "0"))
		new_value = false;
	else {
		pr_err("%s: invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	mutex_lock(&pas221->power_lock);
	if (new_value && !(pas221->power_state & LIGHT_ENABLED)) {
		pas221->power_state |= LIGHT_ENABLED;
		pas221_light_enable(pas221);
	} else if (!new_value && (pas221->power_state & LIGHT_ENABLED)) {
		pas221_light_disable(pas221);
		pas221->power_state &= ~LIGHT_ENABLED;
	}
	mutex_unlock(&pas221->power_lock);
	return size;
}

static ssize_t proximity_enable_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	struct pas221_data *pas221 = dev_get_drvdata(dev);
	bool new_value;
	u8 dat[3];	
	u8 tmp;
	u8 val;
	int poffset;
	int als_value;
	u8 ps_cpst=0;

	
	if (sysfs_streq(buf, "1"))
		new_value = true;
	else if (sysfs_streq(buf, "0"))
		new_value = false;
	else {
		pr_err("%s: invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

        printk(KERN_INFO "[PAS221] proximity_enable_store: new_value=%d\n", new_value);   

	mutex_lock(&pas221->power_lock);
	if (new_value && !(pas221->power_state & PROXIMITY_ENABLED)) {
		pas221->power_state |= PROXIMITY_ENABLED;
		pas221->pdata->proximity_power(1);
		pas221_initial_reg_ps_on_als_on(pas221);
		msleep(101);
		pas221_i2c_readn(pas221, REGS_PROX_DATA, dat, 3);
		als_value=(dat[1]&0x0f)<<8 | dat[2];

		if(dat[0]==0 && als_value>=0xfff) {
			tmp = PS_UNDER_SUNLIGHT;
			pr_err(TAG"PS_UNDER_SUNLIGHT detection !!! ");
		} else {
			ps_cpst = als_value/PS_CPST_ALS_THRES;
			tmp = dat[0] + ps_cpst;
			if(tmp<dat[0]) tmp=0xff;
		}

		pr_err(TAG"PS_DAT:%d", tmp);
		if(tmp < pas221->prox_hys[2]-RECALC_THRESHOLD) {
			pr_err(TAG" RECALC PROXIMITY !!!!!!!!! \t");
			val=0;
			poffset=0;
				
			if(tmp > XTALK_GAIN) {
				val = tmp / XTALK_GAIN;
				val = val > 0xf ? 0xf : val ;
			}
			poffset = tmp % XTALK_GAIN;
						
			poffset=poffset*4/5;
			pas221->xtalk = val;
			pas221->poffset = poffset;
			pas221->prox_hys[0]=PROX_HYS_OFFSET + poffset;
			pas221->prox_hys[1]=PROX_DET_OFFSET + poffset;
			pas221->prox_hys[2]=tmp;
		}
		pas221_change_prox_offset(pas221, B_PROX_OFFSET(pas221->xtalk));
		pas221_i2c_write(pas221, REGS_PROX_INT_TL, pas221->prox_hys[0]);
		pas221_i2c_write(pas221, REGS_PROX_INT_TH, pas221->prox_hys[1]);
		pr_err(TAG" xtalk : %d, poffset %d, ps_cpst %d, hys0 : %d , hys1: %d ", \
			pas221->xtalk,pas221->poffset, ps_cpst, pas221->prox_hys[0], pas221->prox_hys[1]); 

		enable_irq(pas221->irq);
		enable_irq_wake(pas221->irq);
	} 
	else if (!new_value && (pas221->power_state & PROXIMITY_ENABLED)) {
		pas221->power_state &= ~PROXIMITY_ENABLED;
		disable_irq_wake(pas221->irq);
		disable_irq(pas221->irq);
		pas221_reg_ps_als_enable(pas221, ps_disable, als_enable);
		pas221_interrupt_clear(pas221);
		pas221->pdata->proximity_power(0);
	}
	mutex_unlock(&pas221->power_lock);
	return size;
}

static ssize_t proximity_avg_show(struct device *dev,
	struct device_attribute *attr, char *buf) 
{
	struct pas221_data *pas221 = dev_get_drvdata(dev);
	int adc[3];
	int poffset=pas221->poffset;

	if((pas221->avg[0]-poffset)<0) {
		poffset=pas221->avg[0];
	}
	adc[0]=pas221->avg[0]-poffset;
	adc[1]=pas221->avg[1]-poffset;
	adc[2]=pas221->avg[2]-poffset;

	return sprintf(buf, "%d,%d,%d\n", adc[0], adc[1], adc[2]);
}

static ssize_t proximity_avg_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	struct pas221_data *pas221 = dev_get_drvdata(dev);
	bool new_value;

	if (sysfs_streq(buf, "1"))
		new_value = true;
	else if (sysfs_streq(buf, "0"))
		new_value = false;
	else {
		pr_err("%s: invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	mutex_lock(&pas221->power_lock);
	if (new_value) {
		if (!(pas221->power_state & PROXIMITY_ENABLED)) {
			pas221->pdata->proximity_power(1);
			pas221_initial_reg_ps_on_als_on(pas221);
		}
		hrtimer_start(&pas221->prox_timer, pas221->prox_poll_delay,
							HRTIMER_MODE_REL);
	} else if (!new_value) {
		hrtimer_cancel(&pas221->prox_timer);
		cancel_work_sync(&pas221->work_prox);
		if (!(pas221->power_state & PROXIMITY_ENABLED)) {
			pas221_reg_ps_als_enable(pas221, ps_disable, als_enable);
			pas221->pdata->proximity_power(0);
		}
	}
	mutex_unlock(&pas221->power_lock);

	return size;
}

static ssize_t prox_open_calibration_show(struct device *dev, struct device_attribute *attr, char     *buf)
{
	int count = 0;
	int isOpenCal = 1;
	printk(KERN_INFO "[PAS221] %s\n", __func__);

	count = sprintf(buf, "%d\n", isOpenCal);
	return count;
}


static DEVICE_ATTR(prox_avg, 0644,
		   proximity_avg_show, proximity_avg_store);
static DEVICE_ATTR(state, 0644, proximity_state_show, NULL);

static DEVICE_ATTR(adc, 0644, lightsensor_file_state_show,
	NULL);

static DEVICE_ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
		   poll_delay_show, poll_delay_store);

static struct device_attribute dev_attr_light_enable =
	__ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
	       light_enable_show, light_enable_store);

static struct device_attribute dev_attr_proximity_enable =
	__ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
	       proximity_enable_show, proximity_enable_store);

static struct device_attribute dev_attr_prox_cal_open =
	__ATTR(prox_cal_open, S_IRUGO, prox_open_calibration_show, NULL);

static struct attribute *light_sysfs_attrs[] = {
	&dev_attr_light_enable.attr,
	&dev_attr_poll_delay.attr,
	NULL
};

static struct attribute_group light_attribute_group = {
	.attrs = light_sysfs_attrs,
};

static struct attribute *proximity_sysfs_attrs[] = {
	&dev_attr_proximity_enable.attr,
	&dev_attr_prox_cal_open.attr,        
	NULL
};

static struct attribute_group proximity_attribute_group = {
	.attrs = proximity_sysfs_attrs,
};

static void pas221_work_func_light(struct work_struct *work) 
{
	int i;
	int als;
	struct pas221_data *pas221 = container_of(work, struct pas221_data,
					      work_light);

	als = lightsensor_get_alsvalue(pas221);

	for (i = 0; ARRAY_SIZE(adc_table); i++)
		if (als <= adc_table[i])
			break;

	if (pas221->light_buffer == i)	{
		if (pas221->light_count++ == LIGHT_BUFFER_NUM) {
			//input_report_abs(pas221->light_input_dev,
			//				ABS_MISC, als + 1);
			input_report_abs(pas221->light_input_dev,
							ABS_MISC, als);
			input_sync(pas221->light_input_dev);
			pas221->light_count = 0;
        }
	} else {
		pas221->light_buffer = i;
		pas221->light_count = 0;
    }
}

static void pas221_work_func_prox(struct work_struct *work)
{
	printk(KERN_INFO "[PAS221] %s : \n",__func__);
	struct pas221_data *pas221 = container_of(work, struct pas221_data,
					      work_prox);
	proxsensor_get_avgvalue(pas221);
}

/* This function is for light sensor.  It operates every a few seconds.
 * It asks for work to be done on a thread because i2c needs a thread
 * context (slow and blocking) and then reschedules the timer to run again.
 */
static enum hrtimer_restart pas221_light_timer_func(struct hrtimer *timer)
{
	struct pas221_data *pas221
			= container_of(timer, struct pas221_data, light_timer);
	queue_work(pas221->light_wq, &pas221->work_light);
	hrtimer_forward_now(&pas221->light_timer, pas221->light_poll_delay);
	return HRTIMER_RESTART;
}

static enum hrtimer_restart pas221_prox_timer_func(struct hrtimer *timer)
{
	struct pas221_data *pas221
			= container_of(timer, struct pas221_data, prox_timer);
	queue_work(pas221->prox_wq, &pas221->work_prox);
	hrtimer_forward_now(&pas221->prox_timer, pas221->prox_poll_delay);
	return HRTIMER_RESTART;
}

/* interrupt happened due to transition/change of near/far proximity state */
irqreturn_t pas221_irq_thread_fn(int irq, void *data)
{
	printk(KERN_INFO "[PAS221] %s : \n",__func__);
	struct pas221_data *ip = data;
	u8 val = 1;
	
	//val = gpio_get_value(ip->i2c_client->irq_gpio);
        val = gpio_get_value(ip->pdata->irq_gpio);
        printk(KERN_INFO "[pas221] gpio get value : %d\n", val);
	if (val < 0) {
		pr_err("%s: gpio_get_value error %d\n", __func__, val);
		return IRQ_HANDLED;
	}
	/* for debugging : going to be removed */
#ifdef DEBUG
	pr_err(TAG" report_abs ABS_DISTANCE : %s", (val==0 ? "close":"far"));
#endif
	/* 0 is close, 1 is far */
	input_report_abs(ip->proximity_input_dev, ABS_DISTANCE, val);
	input_sync(ip->proximity_input_dev); 
	wake_lock_timeout(&ip->prx_wake_lock, 3*HZ);

	return IRQ_HANDLED;
}

static int pas221_setup_irq(struct pas221_data *pas221)
{
	printk(KERN_INFO "[PAS221] pas221_setup_irq called\n");
	int rc = -EIO;
	int irq;
		
	if (gpio_request(pas221->pdata->irq_gpio, "Proximity Out")) {
            printk(KERN_ERR "Proximity Request GPIO_%d failed!\n", pas221->pdata->irq_gpio);
	}
        else {
            printk(KERN_ERR "Proximity Request GPIO_%d Sucess!\n", pas221->pdata->irq_gpio);
        }
        
	gpio_direction_input(pas221->pdata->irq_gpio);
	
	//irq = gpio_to_irq(pas221->i2c_client->irq);
	irq = pas221->pdata->irq;
	printk(KERN_INFO "[PAS221] setup_irq : %d\n", irq);
	
	rc = request_threaded_irq(irq, NULL, pas221_irq_thread_fn,
			 IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
			 "proximity_int", pas221);
	
	printk(KERN_INFO "[PAS221] rc : %d\n", rc);
	if (rc < 0) {
		pr_err("%s: request_irq(%d) failed for gpio %d (%d)\n",
			__func__, irq, irq, rc);
		return rc;
	}
        else {
		printk(KERN_INFO "[PAS221] request_irq success IRQ_NO:%d", irq);
        }

	/* start with interrupts disabled */
	disable_irq(irq);
	pas221->irq = irq;

	return rc;
}
 
static int pas221_setup_reg(struct pas221_data *pas221)
{
	int err = 0;
	u8 val;
	u8 tmp;
	u8 dat[3];
	int poffset;
	int als_value;
	u8 ps_cpst=0;

	/* initializing the proximity and light sensor registers */
	mutex_lock(&pas221->power_lock);
	pas221->pdata->proximity_power(1);
	pas221_initial_reg_ps_on_als_on(pas221);
	mutex_unlock(&pas221->power_lock);

	/* calibration proximity */
	msleep(101);		
	tmp=0;
	pas221_i2c_readn(pas221, REGS_PROX_DATA, dat, 3);
	als_value=(dat[1]&0x0f)<<8 | dat[2];
    val=0;
	poffset=0;
	if(dat[0]==0)
	{
		tmp = PS_UNDER_SUNLIGHT; 
		pr_err(TAG"init PS == 0 detection !!! ");
	}
	else
	{
		ps_cpst = als_value/PS_CPST_ALS_THRES;
		tmp = dat[0] + ps_cpst;
		if(tmp<dat[0]) tmp=0xff;
	}
	if(tmp > XTALK_GAIN)
	{
		val = tmp / XTALK_GAIN;
	}
	poffset = tmp % XTALK_GAIN;
	
	poffset=poffset*4/5;
	pas221->xtalk = val>0xf? 0xf:val ;
	pas221->poffset = poffset;
	pas221->prox_hys[0]=PROX_HYS_OFFSET + poffset;
	pas221->prox_hys[1]=PROX_DET_OFFSET + poffset;
	pas221->prox_hys[2]=tmp;
	pr_err(TAG" %s: prox_off %d -- poffset %d ps_cpst %d\t", __func__, val, poffset, ps_cpst);	
	pas221_change_prox_offset(pas221, B_PROX_OFFSET(val));
	pas221_i2c_write(pas221, REGS_PROX_INT_TL, pas221->prox_hys[0]);
	pas221_i2c_write(pas221, REGS_PROX_INT_TH, pas221->prox_hys[1]);
	

	if (err < 0) {
		pr_err(TAG" %s: read ps_data failed\n", __func__);
		err = -EIO;
	}
	mutex_lock(&pas221->power_lock);
	pas221_reg_ps_als_enable(pas221, ps_disable, als_disable);
	pas221->pdata->proximity_power(0);
	mutex_unlock(&pas221->power_lock);

	return err;
}

static int pas221_i2c_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	int ret = -ENODEV;
	struct input_dev *input_dev;
	struct pas221_data *pas221;

	printk(KERN_INFO "[PAS221] %s start \n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err(TAG" %s: i2c functionality check failed!\n", __func__);
		return ret;
	}

	pas221 = kzalloc(sizeof(struct pas221_data), GFP_KERNEL);
	if (!pas221) {
		pr_err(TAG" %s: failed to alloc memory for module data\n",
		       __func__);
		return -ENOMEM;
	}

	pas221->pdata = client->dev.platform_data;
	pas221->i2c_client = client;
	i2c_set_clientdata(client, pas221);

	/* wake lock init */
	wake_lock_init(&pas221->prx_wake_lock, WAKE_LOCK_SUSPEND,
		       "prx_wake_lock");
	mutex_init(&pas221->power_lock);

	/* setup initial registers */
	ret = pas221_setup_reg(pas221);
	if (ret < 0) {
		pr_err(TAG" %s: could not setup regs\n", __func__);
		goto err_setup_reg;
	}

	ret = pas221_setup_irq(pas221);
	if (ret) {
		pr_err(TAG" %s: could not setup irq\n", __func__);
		goto err_setup_irq;
	}

	/* allocate proximity input_device */
	input_dev = input_allocate_device();
	if (!input_dev) {
		pr_err(TAG" %s: could not allocate input device\n", __func__);
		goto err_input_allocate_device_proximity;
	}
	pas221->proximity_input_dev = input_dev;
	input_set_drvdata(input_dev, pas221);
	input_dev->name = "proximity_sensor";
	input_set_capability(input_dev, EV_ABS, ABS_DISTANCE);
	input_set_abs_params(input_dev, ABS_DISTANCE, 0, 1, 0, 0);

	ret = input_register_device(input_dev);
	if (ret < 0) {
		pr_err(TAG" %s: could not register input device\n", __func__);
		input_free_device(input_dev);
		goto err_input_register_device_proximity;
	}
	ret = sysfs_create_group(&input_dev->dev.kobj,
				 &proximity_attribute_group);
	if (ret) {
		pr_err(TAG" %s: could not create sysfs group\n", __func__);
		goto err_sysfs_create_group_proximity;
	}

	/* light_timer settings. we poll for light values using a timer. */
	hrtimer_init(&pas221->light_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	pas221->light_poll_delay = ns_to_ktime(200 * NSEC_PER_MSEC);
	pas221->light_timer.function = pas221_light_timer_func;

	/* prox_timer settings. we poll for proximity values using a timer. */
	hrtimer_init(&pas221->prox_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	pas221->prox_poll_delay = ns_to_ktime(2000 * NSEC_PER_MSEC);
	pas221->prox_timer.function = pas221_prox_timer_func;

	/* the timer just fires off a work queue request.  we need a thread
	   to read the i2c (can be slow and blocking). */
	pas221->light_wq = create_singlethread_workqueue("pas221_light_wq");
	if (!pas221->light_wq) {
		ret = -ENOMEM;
		pr_err(TAG" %s: could not create light workqueue\n", __func__);
		goto err_create_light_workqueue;
	}
	pas221->prox_wq = create_singlethread_workqueue("pas221_prox_wq");
	if (!pas221->prox_wq) {
		ret = -ENOMEM;
		pr_err(TAG" %s: could not create prox workqueue\n", __func__);
		goto err_create_prox_workqueue;
	}

	/* this is the thread function we run on the work queue */
	INIT_WORK(&pas221->work_light, pas221_work_func_light);
	INIT_WORK(&pas221->work_prox, pas221_work_func_prox);

	/* allocate lightsensor-level input_device */
	input_dev = input_allocate_device();
	if (!input_dev) {
		pr_err(TAG" %s: could not allocate input device\n", __func__);
		ret = -ENOMEM;
		goto err_input_allocate_device_light;
	}
	input_set_drvdata(input_dev, pas221);
	input_dev->name = "light_sensor";
	input_set_capability(input_dev, EV_ABS, ABS_MISC);
	input_set_abs_params(input_dev, ABS_MISC, 0, 1, 0, 0);

	ret = input_register_device(input_dev);
	if (ret < 0) {
		pr_err(TAG" %s: could not register input device\n", __func__);
		input_free_device(input_dev);
		goto err_input_register_device_light;
	}
	pas221->light_input_dev = input_dev;
	ret = sysfs_create_group(&input_dev->dev.kobj,
				 &light_attribute_group);
	if (ret) {
		pr_err(TAG" %s: could not create sysfs group\n", __func__);
		goto err_sysfs_create_group_light;
	}

	/* set sysfs for proximity sensor */
	pas221->proximity_dev = device_create(sensors_class,
		NULL, 0, NULL, "proximity_sensor");
	if (IS_ERR(pas221->proximity_dev)) {
		pr_err(TAG" %s: could not create proximity_dev\n", __func__);
		goto err_proximity_device_create;
	}

	if (device_create_file(pas221->proximity_dev,&dev_attr_state) < 0) {
		pr_err(TAG" %s: could not create device file(%s)!\n", __func__,
			dev_attr_state.attr.name);
		goto err_proximity_device_create_file1;
	}

	if (device_create_file(pas221->proximity_dev,
		&dev_attr_prox_avg) < 0) {
		pr_err(TAG" %s: could not create device file(%s)!\n", __func__,
			dev_attr_prox_avg.attr.name);
		goto err_proximity_device_create_file2;
	}

	dev_set_drvdata(pas221->proximity_dev, pas221);
#if 0
	/* set sysfs for light sensor */
	pas221->switch_cmd_dev = device_create(sensors_class,
					NULL, 0, NULL, "light_sensor");
	if (IS_ERR(pas221->switch_cmd_dev)) {
		pr_err(TAG" %s: could not create light_dev\n", __func__);
		goto err_light_device_create;
	}

	if (device_create_file(pas221->switch_cmd_dev, &dev_attr_adc) < 0) {
		pr_err(TAG" %s: could not create device file(%s)!\n", __func__,
			dev_attr_adc.attr.name);
		goto err_light_device_create_file1;
	}

	dev_set_drvdata(pas221->switch_cmd_dev, pas221);
#endif
	/*Pulling the GPIO_PS_OUT Pin High*/
	printk(KERN_INFO "[PAS221] gpio_get_value of GPIO_PS_OUT is %d\n",gpio_get_value(pas221->pdata->irq_gpio));

	/* set initial proximity value as 1 */
	input_report_abs(pas221->proximity_input_dev, ABS_DISTANCE, 1);
	input_sync(pas221->proximity_input_dev);

	printk(KERN_INFO "[PAS221] %s end\n", __func__);
    
	goto done;

/* error, unwind it all */
err_light_device_create_file1:
	device_remove_file(pas221->proximity_dev, &dev_attr_adc);
err_light_device_create:
	device_destroy(sensors_class, 0);
err_proximity_device_create_file2:
	device_remove_file(pas221->proximity_dev, &dev_attr_prox_avg);
err_proximity_device_create_file1:
	device_remove_file(pas221->proximity_dev, &dev_attr_state);
err_proximity_device_create:
	device_destroy(sensors_class, 0);
err_sysfs_create_group_light:
	sysfs_remove_group(&input_dev->dev.kobj,
                        &light_attribute_group);
err_input_register_device_light:
	input_unregister_device(pas221->light_input_dev);
err_input_allocate_device_light:
err_create_light_workqueue:
	destroy_workqueue(pas221->light_wq);
err_create_prox_workqueue:
	destroy_workqueue(pas221->prox_wq);
err_sysfs_create_group_proximity:
	 sysfs_remove_group(&pas221->proximity_input_dev->dev.kobj,
                            &proximity_attribute_group);
err_input_register_device_proximity:
	input_unregister_device(pas221->proximity_input_dev);
err_input_allocate_device_proximity:
err_setup_irq:
	free_irq(pas221->irq, 0);
err_setup_reg:
	mutex_destroy(&pas221->power_lock);
	wake_lock_destroy(&pas221->prx_wake_lock);
	kfree(pas221);
done:
	return ret;
}

static int pas221_suspend(struct device *dev)
{
	/* We disable power only if proximity is disabled.  If proximity
	   is enabled, we leave power on because proximity is allowed
	   to wake up device.  We remove power without changing
	   pas221->power_state because we use that state in resume.
	*/
	struct i2c_client *client = to_i2c_client(dev);
	struct pas221_data *pas221 = i2c_get_clientdata(client);
	if (pas221->power_state & LIGHT_ENABLED)
		pas221_light_disable(pas221);
	return 0;
}

static int pas221_resume(struct device *dev)
{
	/* Turn power back on if we were before suspend. */
	struct i2c_client *client = to_i2c_client(dev);
	struct pas221_data *pas221 = i2c_get_clientdata(client);
	if (pas221->power_state & LIGHT_ENABLED)
		pas221_light_enable(pas221);
	return 0;
}

static int pas221_i2c_remove(struct i2c_client *client)
{
	struct pas221_data *pas221 = i2c_get_clientdata(client);

	device_remove_file(	pas221->proximity_dev,
				&dev_attr_prox_avg		);
	device_remove_file(	pas221->switch_cmd_dev,
				&dev_attr_adc);
	device_destroy(		pas221->lightsensor_class, 0);
	class_destroy(	pas221->lightsensor_class);
	device_remove_file(pas221->proximity_dev, &dev_attr_prox_avg);
	device_remove_file(pas221->proximity_dev, &dev_attr_state);
	device_destroy(pas221->proximity_class, 0);
	class_destroy(pas221->proximity_class);
	sysfs_remove_group(&pas221->light_input_dev->dev.kobj,
			   &light_attribute_group);
	input_unregister_device(pas221->light_input_dev);
	sysfs_remove_group(&pas221->proximity_input_dev->dev.kobj,
			   &proximity_attribute_group);
	input_unregister_device(pas221->proximity_input_dev);
	free_irq(pas221->irq, NULL);
	if (pas221->power_state) {
		if (pas221->power_state & LIGHT_ENABLED)
			pas221_light_disable(pas221);
		if (pas221->power_state & PROXIMITY_ENABLED) {
			pas221_reg_ps_als_enable(pas221, ps_disable, als_disable);
			pas221->pdata->proximity_power(0);
		}
	}
	destroy_workqueue(pas221->light_wq);
	destroy_workqueue(pas221->prox_wq);
	mutex_destroy(&pas221->power_lock);
	wake_lock_destroy(&pas221->prx_wake_lock);
	kfree(pas221);
	return 0;
}

static const struct i2c_device_id pas221_device_id[] = {
	{"pas221", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, pas221_device_id);

static const struct dev_pm_ops pas221_pm_ops = {
	.suspend = pas221_suspend,
	.resume = pas221_resume
};

static struct i2c_driver pas221_i2c_driver = {
	.driver = {
		.name = "pas221",
		.owner = THIS_MODULE,
		.pm = &pas221_pm_ops
	},
	.probe		= pas221_i2c_probe,
	.remove		= pas221_i2c_remove,
	.id_table	= pas221_device_id,
};


static int __init pas221_init(void)
{
       	struct device *dev_t;    
	printk("%s called",__func__); 
#if defined (CONFIG_MACH_HENDRIX) 
	int ret;
	  //LDO Power On=============
	if (!VPROX_3_0_V) {
               VPROX_3_0_V = regulator_get(NULL, "v_proxy_3v");
               if (IS_ERR(VPROX_3_0_V)) {
                        pr_err("%s regulator get error! %d \n", __func__, __LINE__);
                        VPROX_3_0_V = NULL;
               }
        }
        regulator_set_voltage(VPROX_3_0_V, 3000000, 3000000);
        regulator_enable(VPROX_3_0_V);
        msleep(2);

        if (!VPROX_LED_3_0_V) {
               VPROX_LED_3_0_V = regulator_get(NULL, "v_proxy_led_3v");
               if (IS_ERR(VPROX_LED_3_0_V)) {
                       pr_err("%s regulator get error! %d \n", __func__, __LINE__);
                       VPROX_LED_3_0_V = NULL;
               }
        }
        regulator_set_voltage(VPROX_LED_3_0_V, 2900000, 2900000);
        regulator_enable(VPROX_LED_3_0_V);
        msleep(2);
#endif	
	return i2c_add_driver(&pas221_i2c_driver);
}

static void __exit pas221_exit(void)
{
	printk("%s called",__func__);
 
	i2c_del_driver(&pas221_i2c_driver);
}

module_init(pas221_init);
module_exit(pas221_exit);

MODULE_AUTHOR("partron@partron.co.kr");
MODULE_DESCRIPTION("Proximity and Ambient Sensor driver for pas221");
MODULE_LICENSE("GPL");
