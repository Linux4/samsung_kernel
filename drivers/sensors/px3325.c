/*
 * This file is part of the PX3325 sensor driver.
 * PX3325 is combined proximity sensor and IRLED.
 *
 * Contact: YC Hou <yc.hou@dyna-image.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 *
 * Filename: px3325_v1.02_20140103.c
 *
 * Summary:
 *	PX3325 sensor dirver.
 *
 * Modification History:
 * Date     By       Summary
 * -------- -------- -------------------------------------------------------
 * 07/10/13 YC       Original Creation (version:1.00 base on datasheet rev0.50)
 * 01/02/14 YC       Set default settings to 2X, 25T for recommend settings. 
 *                   Ver 1.01.
 * 01/03/14 YC       Modify to support devicetree. Change to ver 1.02.
 */

#include "px3325.h"

/*
 * register access helpers
 */

static int __px3325_read_reg(struct i2c_client *client,
			       u32 reg, u8 mask, u8 shift)
{
	struct px3325_data *data = i2c_get_clientdata(client);
	u8 idx = 0xff;

	ADD_TO_IDX(reg,idx)
	return (data->reg_cache[idx] & mask) >> shift;
}

static int __px3325_write_reg(struct i2c_client *client,
				u32 reg, u8 mask, u8 shift, u8 val)
{
	struct px3325_data *data = i2c_get_clientdata(client);
	int ret = 0;
	u8 tmp;
	u8 idx = 0xff;

	ADD_TO_IDX(reg,idx)
	if (idx >= PX3325_NUM_CACHABLE_REGS)
		return -EINVAL;

	mutex_lock(&data->lock);

	tmp = data->reg_cache[idx];
	tmp &= ~mask;
	tmp |= val << shift;

	ret = i2c_smbus_write_byte_data(client, reg, tmp);
	if (!ret)
		data->reg_cache[idx] = tmp;

	mutex_unlock(&data->lock);
	return ret;
}

/*
 * internally used functions
 */

/* mode */
static int px3325_get_mode(struct i2c_client *client)
{
	int ret;

	ret = __px3325_read_reg(client, PX3325_MODE_COMMAND,
			PX3325_MODE_MASK, PX3325_MODE_SHIFT);
	return ret;
}

static int px3325_set_mode(struct i2c_client *client, int mode)
{
	struct px3325_data *data = i2c_get_clientdata(client);
  
	if (mode != px3325_get_mode(client))
	{
		__px3325_write_reg(client, PX3325_MODE_COMMAND,
			  PX3325_MODE_MASK, PX3325_MODE_SHIFT, mode);

		/* Enable/Disable PS */
		if (PS_ACTIVE & mode)
		{
			wake_lock(&data->prx_wake_lock);
        
			if (ps_polling)
				hrtimer_start(&data->proximity_timer, data->proximity_poll_delay,
					HRTIMER_MODE_REL);
		}
		else
		{
			wake_unlock(&data->prx_wake_lock);
        
			if (ps_polling)
			{
				hrtimer_cancel(&data->proximity_timer);
					cancel_work_sync(&data->work_proximity);
			}    
		}
	}
	return 0;
}

/* PX low threshold */
static int px3325_get_plthres(struct i2c_client *client)
{
	int lsb, msb;
	lsb = __px3325_read_reg(client, PX3325_PX_LTHL,
				PX3325_PX_LTHL_MASK, PX3325_PX_LTHL_SHIFT);
	msb = __px3325_read_reg(client, PX3325_PX_LTHH,
				PX3325_PX_LTHH_MASK, PX3325_PX_LTHH_SHIFT);
	return ((msb << 8) | lsb);
}

static int px3325_set_plthres(struct i2c_client *client, int val)
{
	int lsb, msb, err;
	
	msb = (val >> 8) & PX3325_PX_LTHH_MASK;
	lsb = val & PX3325_PX_LTHL_MASK;
	
	err = __px3325_write_reg(client, PX3325_PX_LTHL,
		PX3325_PX_LTHL_MASK, PX3325_PX_LTHL_SHIFT, lsb);
	if (err)
		return err;

	err = __px3325_write_reg(client, PX3325_PX_LTHH,
		PX3325_PX_LTHH_MASK, PX3325_PX_LTHH_SHIFT, msb);

	return err;
}

/* PX high threshold */
static int px3325_get_phthres(struct i2c_client *client)
{
	int lsb, msb;
	lsb = __px3325_read_reg(client, PX3325_PX_HTHL,
				PX3325_PX_HTHL_MASK, PX3325_PX_HTHL_SHIFT);
	msb = __px3325_read_reg(client, PX3325_PX_HTHH,
				PX3325_PX_HTHH_MASK, PX3325_PX_HTHH_SHIFT);
	return ((msb << 8) | lsb);
}

static int px3325_set_phthres(struct i2c_client *client, int val)
{
	int lsb, msb, err;
	
	msb = (val >> 8) & PX3325_PX_HTHH_MASK ;
	lsb = val & PX3325_PX_HTHL_MASK;
	
	err = __px3325_write_reg(client, PX3325_PX_HTHL,
		PX3325_PX_HTHL_MASK, PX3325_PX_HTHL_SHIFT, lsb);
	if (err)
		return err;

	err = __px3325_write_reg(client, PX3325_PX_HTHH,
		PX3325_PX_HTHH_MASK, PX3325_PX_HTHH_SHIFT, msb);

	return err;
}

/* object */
static int px3325_get_object(struct i2c_client *client)
{
	return __px3325_read_reg(client, PX3325_OBJ_COMMAND,
				PX3325_OBJ_MASK, PX3325_OBJ_SHIFT);
}

/* int status */
static int px3325_get_intstat(struct i2c_client *client)
{
	int val;
	
	val = i2c_smbus_read_byte_data(client, PX3325_INT_COMMAND);
	val &= PX3325_INT_MASK;

	return val >> PX3325_INT_SHIFT;
}

/* PX value */
static int px3325_get_px_value(struct i2c_client *client, int lock)
{
	struct px3325_data *data = i2c_get_clientdata(client);
	u8 lsb, msb;
	u16 raw;

	if (!lock) mutex_lock(&data->lock);
	
	lsb = i2c_smbus_read_byte_data(client, PX3325_PX_LSB);
	msb = i2c_smbus_read_byte_data(client, PX3325_PX_MSB);

	if (!lock) mutex_unlock(&data->lock);

	raw = (msb & 0x03) * 256 + lsb;

	return raw;
}

/* calibration */
static int px3325_get_calibration(struct i2c_client *client)
{
	int lsb, msb;
	lsb = __px3325_read_reg(client, PX3325_PX_CALI_L,
				PX3325_PX_CALI_L_MASK, PX3325_PX_CALI_L_SHIFT);
	msb = __px3325_read_reg(client, PX3325_PX_CALI_H,
				PX3325_PX_CALI_H_MASK, PX3325_PX_CALI_H_MASK);
	return ((msb << 8) | lsb);
}

static int px3325_set_calibration(struct i2c_client *client, int val)
{
	int lsb, msb, err;
	
	msb = (val >> 8) & PX3325_PX_CALI_H_MASK ;
	lsb = val & PX3325_PX_CALI_L_MASK;
	
	err = __px3325_write_reg(client, PX3325_PX_CALI_L,
		PX3325_PX_CALI_L_MASK, PX3325_PX_CALI_L_SHIFT, lsb);
	if (err)
		return err;

	err = __px3325_write_reg(client, PX3325_PX_CALI_H,
		PX3325_PX_CALI_H_MASK, PX3325_PX_CALI_H_MASK, msb);

	return err;
}

/* gain */
static int px3325_get_gain(struct i2c_client *client)
{
	int gain;
	gain = __px3325_read_reg(client, PX3325_PX_GAIN,
				PX3325_PX_GAIN_MASK, PX3325_PX_GAIN_SHIFT);
	return gain;
}

static int px3325_set_gain(struct i2c_client *client, int val)
{
	int err;
	
	err = __px3325_write_reg(client, PX3325_PX_GAIN,
		PX3325_PX_GAIN_MASK, PX3325_PX_GAIN_SHIFT, val);

	return err;
}

/* integrated time */
static int px3325_get_integrate(struct i2c_client *client)
{
	int inte;
	inte = __px3325_read_reg(client, PX3325_PX_INTEGRATE,
				PX3325_PX_INTEGRATE_MASK, PX3325_PX_INTEGRATE_SHIFT);
	return inte;
}

static int px3325_set_integrate(struct i2c_client *client, int val)
{
	int err;
	
	err = __px3325_write_reg(client, PX3325_PX_INTEGRATE,
		PX3325_PX_INTEGRATE_MASK, PX3325_PX_INTEGRATE_SHIFT, val);

	return err;
}

/*
 * sysfs layer
 */

/* mode */
static ssize_t px3325_show_mode(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct px3325_data *data = input_get_drvdata(input);
	return sprintf(buf, "%d\n", px3325_get_mode(data->client));
}

static ssize_t px3325_store_mode(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct px3325_data *data = input_get_drvdata(input);
	unsigned long val;
	int ret;

	if ((strict_strtoul(buf, 10, &val) < 0) || (val > 6))
		return -EINVAL;

	ret = px3325_set_mode(data->client, val);
	
	if (ret < 0)
		return ret;
	return count;
}

static DEVICE_ATTR(mode, S_IWUSR | S_IRUGO,
		   px3325_show_mode, px3325_store_mode);

/* enable_ps_sensor */
static ssize_t px3325_show_enable_ps_sensor(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct px3325_data *data = input_get_drvdata(input);
	return sprintf(buf, "%d\n", px3325_get_mode(data->client)&0x02);
}

static ssize_t px3325_store_enable_ps_sensor(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct px3325_data *data = input_get_drvdata(input);
	unsigned long val;
	int ret;

	if ((strict_strtoul(buf, 10, &val) < 0) || (val > 1))
		return -EINVAL;
	
	ret = px3325_set_mode(data->client, val ? 2:0);
	
	if (ret < 0)
		return ret;
	return count;
}

static DEVICE_ATTR(enable_ps_sensor, S_IWUSR | S_IRUGO,
		   px3325_show_enable_ps_sensor, px3325_store_enable_ps_sensor);
           
/* ps_poll_delay */
static ssize_t px3325_show_ps_poll_delay(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct px3325_data *data = input_get_drvdata(input);
	return sprintf(buf, "%d\n", (int)ktime_to_ns(data->proximity_poll_delay)/1000);
}

static ssize_t px3325_store_ps_poll_delay(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct px3325_data *data = input_get_drvdata(input);
	unsigned long val;

	if (strict_strtoul(buf, 10, &val) < 0)
		return -EINVAL;
    
	data->proximity_poll_delay = ns_to_ktime(val*1000);
	return count;
}

static DEVICE_ATTR(ps_poll_delay, S_IWUSR | S_IRUGO,
		   px3325_show_ps_poll_delay, px3325_store_ps_poll_delay);

/* Px data */
static ssize_t px3325_show_pxvalue(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct px3325_data *data = input_get_drvdata(input);

	/* No Px data if non active */
	if (px3325_get_mode(data->client) != PS_ACTIVE)
		return -EBUSY;

	return sprintf(buf, "%d\n", px3325_get_px_value(data->client,0));
}

static DEVICE_ATTR(pxvalue, S_IRUGO, px3325_show_pxvalue, NULL);

/* proximity object detect */
static ssize_t px3325_show_object(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct px3325_data *data = input_get_drvdata(input);
	return sprintf(buf, "%d\n", px3325_get_object(data->client));
}

static DEVICE_ATTR(object, S_IRUGO, px3325_show_object, NULL);

/* Px low threshold */
static ssize_t px3325_show_plthres(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct px3325_data *data = input_get_drvdata(input);
	return sprintf(buf, "%d\n", px3325_get_plthres(data->client));
}

static ssize_t px3325_store_plthres(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct px3325_data *data = input_get_drvdata(input);
	unsigned long val;
	int ret;

	if (strict_strtoul(buf, 10, &val) < 0)
		return -EINVAL;

	ret = px3325_set_plthres(data->client, val);
	if (ret < 0)
		return ret;

	return count;
}

static DEVICE_ATTR(plthres, S_IWUSR | S_IRUGO,
		   px3325_show_plthres, px3325_store_plthres);

/* Px high threshold */
static ssize_t px3325_show_phthres(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct px3325_data *data = input_get_drvdata(input);
	return sprintf(buf, "%d\n", px3325_get_phthres(data->client));
}

static ssize_t px3325_store_phthres(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct px3325_data *data = input_get_drvdata(input);
	unsigned long val;
	int ret;

	if (strict_strtoul(buf, 10, &val) < 0)
		return -EINVAL;

	ret = px3325_set_phthres(data->client, val);
	if (ret < 0)
		return ret;

	return count;
}

static DEVICE_ATTR(phthres, S_IWUSR | S_IRUGO,
		   px3325_show_phthres, px3325_store_phthres);


/* calibration */
static ssize_t px3325_show_calibration_state(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct px3325_data *data = input_get_drvdata(input);
	return sprintf(buf, "%d\n", px3325_get_calibration(data->client));
}

static ssize_t px3325_store_calibration_state(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct px3325_data *data = input_get_drvdata(input);
	unsigned long val;
	int ret;

	if (strict_strtoul(buf, 10, &val) < 0)
		return -EINVAL;

	ret = px3325_set_calibration(data->client, val);
	if (ret < 0)
		return ret;

	return count;
}

static DEVICE_ATTR(calibration, S_IWUSR | S_IRUGO,
		   px3325_show_calibration_state, px3325_store_calibration_state);

/* gain */
static ssize_t px3325_show_gain_state(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct px3325_data *data = input_get_drvdata(input);
	return sprintf(buf, "%d\n", px3325_get_gain(data->client));
}

static ssize_t px3325_store_gain_state(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct px3325_data *data = input_get_drvdata(input);
	unsigned long val;
	int ret;

	if (strict_strtoul(buf, 10, &val) < 0)
		return -EINVAL;

	ret = px3325_set_gain(data->client, val);
	if (ret < 0)
		return ret;

	return count;
}

static DEVICE_ATTR(gain, S_IWUSR | S_IRUGO,
		   px3325_show_gain_state, px3325_store_gain_state);

/* integrated time */
static ssize_t px3325_show_integrate_state(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct input_dev *input = to_input_dev(dev);
	struct px3325_data *data = input_get_drvdata(input);
	return sprintf(buf, "%d\n", px3325_get_integrate(data->client));
}

static ssize_t px3325_store_integrate_state(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct input_dev *input = to_input_dev(dev);
	struct px3325_data *data = input_get_drvdata(input);
	unsigned long val;
	int ret;

	if (strict_strtoul(buf, 10, &val) < 0)
		return -EINVAL;

	ret = px3325_set_integrate(data->client, val);
	if (ret < 0)
		return ret;

	return count;
}

static DEVICE_ATTR(integrate, S_IWUSR | S_IRUGO,
		   px3325_show_integrate_state, px3325_store_integrate_state);

#ifdef DI_DBG
/* engineer mode */
static ssize_t px3325_em_read(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct px3325_data *data = i2c_get_clientdata(client);
	int i;
	u8 tmp;
	
	for (i = 0; i < PX3325_NUM_CACHABLE_REGS; i++)
	{
		mutex_lock(&data->lock);
		tmp = i2c_smbus_read_byte_data(data->client, px3325_reg[i]);
		mutex_unlock(&data->lock);

		printk("Reg[0x%x] Val[0x%x]\n", px3325_reg[i], tmp);
	}

	return 0;
}

static ssize_t px3325_em_write(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct px3325_data *data = i2c_get_clientdata(client);
	u32 addr,val,idx=0;
	int ret = 0;

	sscanf(buf, "%x%x", &addr, &val);

	printk("Write [%x] to Reg[%x]...\n",val,addr);
	mutex_lock(&data->lock);

	ret = i2c_smbus_write_byte_data(data->client, addr, val);
	ADD_TO_IDX(addr,idx)
	if (!ret)
		data->reg_cache[idx] = val;

	mutex_unlock(&data->lock);

	return count;
}
static DEVICE_ATTR(em, S_IWUSR |S_IRUGO,
				   px3325_em_read, px3325_em_write);
#endif

static struct attribute *px3325_ps_attributes[] = {
	&dev_attr_mode.attr,
	&dev_attr_enable_ps_sensor.attr,
	&dev_attr_ps_poll_delay.attr,
	&dev_attr_object.attr,
	&dev_attr_pxvalue.attr,
	&dev_attr_plthres.attr,
	&dev_attr_phthres.attr,
	&dev_attr_calibration.attr,
	&dev_attr_gain.attr,
	&dev_attr_integrate.attr,
#ifdef DI_DBG
	&dev_attr_em.attr,
#endif
	NULL
};

static const struct attribute_group px3325_ps_attr_group = {
	.attrs = px3325_ps_attributes,
};

static struct device_attribute *prox_sensor_attrs[] = {
	&dev_attr_mode,
	&dev_attr_enable_ps_sensor,
	&dev_attr_ps_poll_delay,
	&dev_attr_object,
	&dev_attr_pxvalue,
	&dev_attr_plthres,
	&dev_attr_phthres,
	&dev_attr_calibration,
	&dev_attr_gain,
	&dev_attr_integrate,

	NULL
};

static int px3325_init_client(struct i2c_client *client)
{
	struct px3325_data *data = i2c_get_clientdata(client);
	int i;

	/* read all the registers once to fill the cache.
	 * if one of the reads fails, we consider the init failed */
	for (i = 0; i < PX3325_NUM_CACHABLE_REGS; i++) {
		int v = i2c_smbus_read_byte_data(client, px3325_reg[i]);
		if (v < 0)
			return -ENODEV;

		data->reg_cache[i] = v;
	}

	/* set defaults */
	px3325_set_gain(client, 0x01);  // 2X gain
	px3325_set_integrate(client, 0x18);  // 25T
	px3325_set_mode(client, 0);    // power down for power saving.

	return 0;
}

static void px3325_work_func_proximity(struct work_struct *work)
{
	struct px3325_data *data = container_of(work, struct px3325_data, work_proximity);
	int Pval, phth, plth;

	mutex_lock(&data->lock);
	
	Pval = px3325_get_px_value(data->client,1);

	plth = px3325_get_plthres(data->client);
	phth = px3325_get_phthres(data->client);

	printk("PS data = [%d]\t", Pval);
	printk("%s\n", (Pval > phth) ? "obj near":((Pval < plth) ? "obj far":"obj in range"));

	mutex_unlock(&data->lock);

	input_report_abs(data->proximity_input_dev, ABS_DISTANCE, Pval);
	input_sync(data->proximity_input_dev);
}

static enum hrtimer_restart px3325_pxy_timer_func(struct hrtimer *timer)
{
	struct px3325_data *data = container_of(timer, struct px3325_data, proximity_timer);
	queue_work(data->wq, &data->work_proximity);
	hrtimer_forward_now(&data->proximity_timer, data->proximity_poll_delay);
	return HRTIMER_RESTART;
}

static void px3325_timer_init(struct px3325_data *data)
{
	if (ps_polling)
	{
		/* proximity hrtimer settings. */
		hrtimer_init(&data->proximity_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		data->proximity_poll_delay = ns_to_ktime(50 * NSEC_PER_MSEC);
		data->proximity_timer.function = px3325_pxy_timer_func;
	}
}

static int px3325_input_init(struct px3325_data *data)
{
    struct input_dev *input_dev;
    int ret;

    /* allocate proximity input_device */
    input_dev = input_allocate_device();
    if (!input_dev) {
        PX_DBG("could not allocate input device\n");
        goto err_pxy_all;
    }
    data->proximity_input_dev = input_dev;
    input_set_drvdata(input_dev, data);
    input_dev->name = "DI proximity sensor";
    input_set_capability(input_dev, EV_ABS, ABS_DISTANCE);
    input_set_abs_params(input_dev, ABS_DISTANCE, 0, 1, 0, 0);

    PX_DBG("registering proximity input device\n");
    ret = input_register_device(input_dev);
    if (ret < 0) {
        PX_DBG("could not register input device\n");
        goto err_pxy_reg;;
    }
	ret = sensors_create_symlink(&data->proximity_input_dev->dev.kobj,
				input_dev->name);
	if (ret < 0) {
		pr_err("%s: could not create proximity symlink\n", __func__);
		goto err_create_symlink_proximity;
	}

    ret = sysfs_create_group(&input_dev->dev.kobj,
                 &px3325_ps_attr_group);
    if (ret) {
        PX_DBG("could not create sysfs group\n");
        goto err_pxy_sys;
    }

    return 0;

err_pxy_sys:
    input_unregister_device(data->proximity_input_dev);
err_pxy_reg:
    input_free_device(input_dev);
err_create_symlink_proximity:
	sensors_remove_symlink(&data->proximity_input_dev->dev.kobj,
				data->proximity_input_dev->name);
err_pxy_all:
    return (-1);   
}

static void px3325_input_fini(struct px3325_data *data)
{
    struct input_dev *dev = data->proximity_input_dev;

    input_unregister_device(dev);
    input_free_device(dev);
}

/*
 * I2C layer
 */

static irqreturn_t px3325_irq(int irq, void *data_)
{
	struct px3325_data *data = data_;
	u8 int_stat;
 
	mutex_lock(&data->lock);

	int_stat = px3325_get_intstat(data->client);
	
	// PX int
	if (int_stat & PX3325_INT_PMASK)
		queue_work(data->wq, &data->work_proximity);

	mutex_unlock(&data->lock);

	return IRQ_HANDLED;
}

#ifdef CONFIG_PM
static int px3325_suspend(struct device *dev)
{
	//struct px3325_data *data = container_of(h, struct px3325_data, early_suspend);
	
	return 0;
}

static int px3325_resume(struct device *dev)
{

	PX_DBG("px3325_suspend\n");
	return 0;
}
#else
#define px3325_early_suspend NULL
#define px3325_late_resume NULL
#endif

static void px3325_power_enable(int en)
{
	int rc;
	static struct regulator* ldo17;
	static struct regulator* ldo5;
	
	printk(KERN_ERR "%s %s\n", __func__, (en) ? "on" : "off");
	if(!ldo17){
		ldo17 = regulator_get(NULL,"8916_l17");
		rc = regulator_set_voltage(ldo17,2800000,2800000);
		pr_info("[px3325] %s, %d\n", __func__, __LINE__);
		if (rc){
			printk(KERN_ERR "%s: px3325 set_level failed (%d)\n",__func__, rc);
		}
	}
	if(!ldo5){
		ldo5 = regulator_get(NULL,"8916_l5");
		rc = regulator_set_voltage(ldo5,1800000,1800000);
		pr_info("[px3325] %s, %d\n", __func__, __LINE__);
		if (rc){
			printk(KERN_ERR "%s: px3325 set_level failed (%d)\n",__func__, rc);
		}
	}

	if(en){
		rc = regulator_enable(ldo17);
		if (rc){
			printk(KERN_ERR "%s: px3325 enable failed (%d)\n",__func__, rc);
		}
		rc = regulator_enable(ldo5);
		if (rc){
			printk(KERN_ERR "%s: px3325 enable failed (%d)\n",__func__, rc);
		}
	}
	else{
		rc = regulator_disable(ldo17);
		if (rc){
			printk(KERN_ERR "%s: px3325 disable failed (%d)\n",__func__, rc);
		}
		
	}
	return;
}

#ifdef CONFIG_OF

/* device tree parsing function */
static int px3325_parse_dt(struct device *dev,
			struct  px3325_data *pdata)
{

	struct device_node *np = dev->of_node;
	/*irq */
	pdata->irq = of_get_named_gpio_flags(np, "px3325,irq_gpio",0, &pdata->irq_int_flags);
	pr_info("%s: irq: %u \n", __func__, pdata->irq);

	pdata->en = of_get_named_gpio_flags(np, "px3325,en", 0, &pdata->ldo_gpio_flags);
	   pr_info("%s: en: %u \n", __func__, pdata->en);


	return 0;
}
#else
static int px3325_parse_dt(struct device *dev,
struct  taos_platform_data)
{
	return -ENODEV;
}
#endif
static void px3325_request_gpio(struct px3325_data *pdata)
{
	int ret = 0;
	ret = gpio_request(pdata->en, "prox_en");
	if (ret) {
			pdata->is_requested = false;
		pr_err("[px3325]%s: unable to request prox_en [%d]\n",
			__func__, pdata->en);
		return;
	} else {
		pdata->is_requested = true;
	}
	ret = gpio_direction_output(pdata->en, 0);
	if (ret)
		pr_err("[px3325]%s: unable to set_direction for prox_en [%d]\n",
		__func__, pdata->en);
	pr_info("%s: en: %u\n", __func__, pdata->en);
}
static int  px3325_probe(struct i2c_client *client,
				    const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct px3325_data *data;
//	struct input_dev *input_dev;

	int err = 0;
	int rc = -EIO;
	int irq;
	int ret = -ENODEV;

	PX_FUN();
	pr_info("%s, is called\n", __func__);
	px3325_power_enable(1);
	msleep(20);

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE))
		return -EIO;

	data = kzalloc(sizeof(struct px3325_data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;
	
	err = px3325_parse_dt( &client->dev,data);
	if (err) {
		pr_err("%s, parse dt fail\n", __func__);
		goto err_devicetree;
	}
	
	px3325_request_gpio(data);

	msleep(20);

	data->client = client;
	i2c_set_clientdata(client, data);
	mutex_init(&data->lock);
	wake_lock_init(&data->prx_wake_lock, WAKE_LOCK_SUSPEND, "prx_wake_lock");

	px3325_timer_init(data);

	/* initialize the PX3325 chip */
	err = px3325_init_client(client);
	if (err)
		goto exit_kfree;

	err = px3325_input_init(data);
	if (err)
		goto exit_kfree;

	if (!ps_polling)
	{
	
		rc = gpio_request(data->irq, "gpio_proximity_int");
		if (rc < 0) {
			pr_err("%s: gpio %d request failed (%d)\n",
				__func__, data->irq, rc);
			return rc;
		}
		rc = gpio_direction_input(data->irq);
		if (rc < 0) {
			pr_err("%s: failed to set gpio %d as input (%d)\n",
				__func__, data->irq, rc);
			goto err_gpio_direction_input;
		}

		//data->irq = client->irq;
		
		irq = gpio_to_irq(data->irq);
		err = request_threaded_irq(irq, NULL, px3325_irq,
                               IRQF_TRIGGER_FALLING,
                               "px3325", data);
                               
		if (err) {
			dev_err(&client->dev, "ret: %d, could not get IRQ %d\n",err,irq);
				goto exit_irq;
		}
		
		disable_irq(irq);
	}

	INIT_WORK(&data->work_proximity, px3325_work_func_proximity);
  
	data->wq = create_singlethread_workqueue("px3325_wq");
	if (!data->wq) {
		PX_DBG("could not create workqueue\n");
		goto exit_work;
	}
	
	ret = sensors_register(data->proximity_dev,
				data, prox_sensor_attrs, "proximity_sensor");
	if (ret) {
		pr_err("%s: cound not register proximity sensor device(%d).\n",
			__func__, ret);
		goto err_proximity_sensor_register_failed;
	}

	dev_info(&client->dev, "Driver version %s enabled\n", DRIVER_VERSION);
	
	pr_info("%s, is sucess\n", __func__);
	return 0;

err_devicetree:
printk("\n error in device tree");

err_proximity_sensor_register_failed:
	free_irq(data->irq, 0);
exit_work:
	destroy_workqueue(data->wq);
	
err_gpio_direction_input:
	gpio_free(data->irq);

exit_irq:
	px3325_input_fini(data);
	
exit_kfree:
	wake_lock_destroy(&data->prx_wake_lock);
	mutex_destroy(&data->lock);
	kfree(data);
	return err;
}

static int  px3325_remove(struct i2c_client *client)
{
	struct px3325_data *data = i2c_get_clientdata(client);
	
	if (!ps_polling)
		free_irq(data->irq, data);

	sysfs_remove_group(&data->proximity_input_dev->dev.kobj, &px3325_ps_attr_group);
	input_unregister_device(data->proximity_input_dev);

	if ((data->reg_cache[0] & PS_ACTIVE) && ps_polling) 
	{
			hrtimer_cancel(&data->proximity_timer);
			cancel_work_sync(&data->work_proximity);
	}

	destroy_workqueue(data->wq);
	mutex_destroy(&data->lock);
	wake_lock_destroy(&data->prx_wake_lock);
	kfree(data);
	
	return 0;
}


static const struct i2c_device_id px3325_id[] = {
	{ "px3325", 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, px3325_id);

//#ifdef CONFIG_OF
static struct of_device_id px3325_match_table[] = {
        { .compatible = "liteon,px3325",},
        { },
};
//#else
//#define px3325_match_table NULL
//#endif

static const struct dev_pm_ops px3325_pm_ops = {
	.suspend = px3325_suspend,
	.resume = px3325_resume
};


static struct i2c_driver px3325_driver = {
	.driver = {
		.name	= PX3325_DRV_NAME,
		.owner	= THIS_MODULE,
		.pm = &px3325_pm_ops,
		.of_match_table = px3325_match_table,
	},
	.probe	= px3325_probe,
	.remove	= px3325_remove,
	.id_table = px3325_id,
};

static int __init px3325_init(void)
{
	PX_FUN();
    
	if(i2c_add_driver(&px3325_driver))
	{
		PX_ERR("add driver error\n");
		return -1;
	} 

	PX_DBG("init finish\n");
    
	return 0;
}

static void __exit px3325_exit(void)
{
	i2c_del_driver(&px3325_driver);
}

MODULE_AUTHOR("yc.hou@dyna-image.com");
MODULE_DESCRIPTION("PX3325 driver.");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(DRIVER_VERSION);

module_init(px3325_init);
module_exit(px3325_exit);

