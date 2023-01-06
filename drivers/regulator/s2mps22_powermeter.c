/*
 * s2mps22.c
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/err.h>
#include <linux/slab.h>
#include <linux/mfd/samsung/s2mps22.h>
#include <linux/mfd/samsung/s2mps22-regulator.h>
#include <linux/platform_device.h>
#include <linux/regulator/pmic_class.h>

#define CURRENT_METER		1
#define POWER_METER		2
#define SYNC_MODE		1
#define ASYNC_MODE		2

#define PICO_MICRO		1000000
#define NANO_MICRO		1000

static struct adc_info *adc_meter;

struct adc_info {
	struct i2c_client *i2c;
	u8 adc_mode;
	u8 adc_sync_mode;
	u8 *adc_reg;
	u16 *adc_val;
	u16 *current_val;
	u16 *power_val;
	u8 adc_ctrl1;
	u8 ptr_base;
	struct mutex adc_lock;
};

#ifdef CONFIG_DRV_SAMSUNG_PMIC
static const unsigned int current_buck_coeffs[S2MPS22_BUCK_CNT] =
	{CURRENT_B1, CURRENT_B2, CURRENT_B3_4, CURRENT_B3_4};

static const unsigned int current_ldo_coeffs[S2MPS22_LDO_CNT] =
	{CURRENT_L1_3, CURRENT_L2, CURRENT_L1_3,
	 CURRENT_L4_5, CURRENT_L4_5, CURRENT_L6_7,
	 CURRENT_L6_7};

static const unsigned int power_buck_coeffs[S2MPS22_BUCK_CNT] =
	{POWER_B1, POWER_B2, POWER_B3_4, POWER_B3_4};

static const unsigned int power_ldo_coeffs[S2MPS22_LDO_CNT] =
	{POWER_L1_3, POWER_L2, POWER_L1_3, POWER_L4,
	 POWER_L5_6_7, POWER_L5_6_7, POWER_L5_6_7};

static unsigned int get_coeff_c(struct device *dev, u8 adc_reg_num)
{
	unsigned int coeff;

	/* if the regulator is LDO */
	if (adc_reg_num >= S2MPS22_LDO_START &&
	    adc_reg_num <= S2MPS22_LDO_END)
		coeff = current_ldo_coeffs[adc_reg_num - S2MPS22_LDO_START];

	/* if the regulator is BUCK */
	else if (adc_reg_num >= S2MPS22_BUCK_START &&
		 adc_reg_num <= S2MPS22_BUCK_END)
		coeff = current_buck_coeffs[adc_reg_num - S2MPS22_BUCK_START];
	else {
		dev_err(dev, "%s: invalid adc regulator number(%d)\n",
			__func__, adc_reg_num);
		coeff = 0;
	}
	return coeff;
}

static unsigned int get_coeff_p(struct device *dev, u8 adc_reg_num)
{
	unsigned int coeff;
	/* if the regulator is LDO */
	if (adc_reg_num >= S2MPS22_LDO_START &&
	    adc_reg_num <= S2MPS22_LDO_END)
		coeff = power_ldo_coeffs[adc_reg_num - S2MPS22_LDO_START];
	/* if the regulator is BUCK */
	else if (adc_reg_num >= S2MPS22_BUCK_START &&
		 adc_reg_num <= S2MPS22_BUCK_END)
		coeff = power_buck_coeffs[adc_reg_num - S2MPS22_BUCK_START];
	else {
		dev_err(dev, "%s: invalid adc regulator number(%d)\n",
			__func__, adc_reg_num);
		coeff = 0;
	}

	return coeff;
}

static void s2m_adc_read_data(struct device *dev, int channel)
{
	size_t i = 0;
	u8 data_l, data_h;

	mutex_lock(&adc_meter->adc_lock);

	/* ASYNCRD bit '1' --> 2ms delay --> read  in case of ADC Async mode */
	if (adc_meter->adc_sync_mode == ASYNC_MODE) {
		s2mps22_update_reg(adc_meter->i2c, S2MPS22_REG_ADC_CTRL2,
				   ADC_ASYNCRD_MASK, ADC_ASYNCRD_MASK);
		usleep_range(2000, 2100);
	}

	if (channel < 0) {
		/* current */
		for (i = 0; i < S2MPS22_MAX_ADC_CHANNEL; i++) {
			s2mps22_update_reg(adc_meter->i2c, S2MPS22_REG_ADC_CTRL3,
					   (i + CURRENT_PTR_BASE) | ADC_EN_MASK,
					   ADC_PTR_MASK | ADC_EN_MASK);
			s2mps22_read_reg(adc_meter->i2c,
					 S2MPS22_REG_ADC_DATA, &data_l);
			adc_meter->current_val[i] = data_l;
		}
		/* power */
		for (i = 0; i < S2MPS22_MAX_ADC_CHANNEL; i++) {
			s2mps22_update_reg(adc_meter->i2c, S2MPS22_REG_ADC_CTRL3,
					   (2 * i + POWER_PTR_BASE) | ADC_EN_MASK,
					   ADC_PTR_MASK | ADC_EN_MASK);
			s2mps22_read_reg(adc_meter->i2c,
					 S2MPS22_REG_ADC_DATA, &data_l);

			s2mps22_update_reg(adc_meter->i2c, S2MPS22_REG_ADC_CTRL3,
					   (2 * i + 1 + POWER_PTR_BASE) |
					   ADC_EN_MASK, ADC_PTR_MASK |
					   ADC_EN_MASK);
			s2mps22_read_reg(adc_meter->i2c,
					 S2MPS22_REG_ADC_DATA, &data_h);

			adc_meter->power_val[i] = ((data_h & 0xff) << 8) |
						  (data_l & 0xff);
		}
	} else {
		/* current */
		s2mps22_update_reg(adc_meter->i2c, S2MPS22_REG_ADC_CTRL3,
				   (channel + CURRENT_PTR_BASE) |
				   ADC_EN_MASK, ADC_PTR_MASK | ADC_EN_MASK);
		s2mps22_read_reg(adc_meter->i2c, S2MPS22_REG_ADC_DATA, &data_l);
		adc_meter->current_val[channel] = data_l;

		/* power */
		s2mps22_update_reg(adc_meter->i2c, S2MPS22_REG_ADC_CTRL3,
				   (2 * channel + POWER_PTR_BASE) | ADC_EN_MASK,
				   ADC_PTR_MASK | ADC_EN_MASK);
		s2mps22_read_reg(adc_meter->i2c, S2MPS22_REG_ADC_DATA, &data_l);

		s2mps22_update_reg(adc_meter->i2c, S2MPS22_REG_ADC_CTRL3,
				   (2 * channel + 1 + POWER_PTR_BASE) | ADC_EN_MASK,
				   ADC_PTR_MASK | ADC_EN_MASK);
		s2mps22_read_reg(adc_meter->i2c, S2MPS22_REG_ADC_DATA, &data_h);

		adc_meter->power_val[channel] = ((data_h & 0xff) << 8) |
						(data_l & 0xff);
	}

	mutex_unlock(&adc_meter->adc_lock);
}

static ssize_t adc_val_power_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	size_t i = 0;

	for(i = 0; i <= 7; i++) {
		if((adc_meter->adc_reg[i] >= S2MPS22_LDO_START) &&
		   (adc_meter->adc_reg[i] <= S2MPS22_LDO_END))
			return snprintf(buf, PAGE_SIZE,
					"Power-Meter Function for LDOs Not Supported\n");
	}

	s2m_adc_read_data(dev, -1);

	return snprintf(buf, PAGE_SIZE,
			"CH0[0x%02hhx]:%d uW (%d), CH1[0x%02hhx]:%d uW (%d), CH2[0x%02hhx]:%d uW (%d), "
			"CH3[0x%02hhx]:%d uW (%d)\nCH4[0x%02hhx]:%d uW (%d), CH5[0x%02hhx]:%d uW (%d), "
			"CH6[0x%02hhx]:%d uW (%d), CH7[0x%02hhx]:%d uW (%d)\n",
			adc_meter->adc_reg[0],
			adc_meter->power_val[0] * get_coeff_p(dev, adc_meter->adc_reg[0]) / PICO_MICRO,
			adc_meter->power_val[0],

			adc_meter->adc_reg[1],
			adc_meter->power_val[1] * get_coeff_p(dev, adc_meter->adc_reg[1]) / PICO_MICRO,
			adc_meter->power_val[1],

			adc_meter->adc_reg[2],
			adc_meter->power_val[2] * get_coeff_p(dev, adc_meter->adc_reg[2]) / PICO_MICRO,
			adc_meter->power_val[2],

			adc_meter->adc_reg[3],
			adc_meter->power_val[3] * get_coeff_p(dev, adc_meter->adc_reg[3]) / PICO_MICRO,
			adc_meter->power_val[3],

			adc_meter->adc_reg[4],
			adc_meter->power_val[4] * get_coeff_p(dev, adc_meter->adc_reg[4]) / PICO_MICRO,
			adc_meter->power_val[4],

			adc_meter->adc_reg[5],
			adc_meter->power_val[5] * get_coeff_p(dev, adc_meter->adc_reg[5]) / PICO_MICRO,
			adc_meter->power_val[5],

			adc_meter->adc_reg[6],
			adc_meter->power_val[6] * get_coeff_p(dev, adc_meter->adc_reg[6]) / PICO_MICRO,
			adc_meter->power_val[6],

			adc_meter->adc_reg[7],
			adc_meter->power_val[7] * get_coeff_p(dev, adc_meter->adc_reg[7]) / PICO_MICRO,
			adc_meter->power_val[7]);

}

static ssize_t adc_val_current_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	s2m_adc_read_data(dev, -1);

	return snprintf(buf, PAGE_SIZE,
			"CH0[0x%02hhx]:%d uA (%d), CH1[0x%02hhx]:%d uA (%d), CH2[0x%02hhx]:%d uA (%d), "
			"CH3[0x%02hhx]:%d uA (%d)\nCH4[0x%02hhx]:%d uA (%d), CH5[0x%02hhx]:%d uA (%d), "
			"CH6[0x%02hhx]:%d uA (%d), CH7[0x%02hhx]:%d uA (%d)\n",
			adc_meter->adc_reg[0],
			adc_meter->current_val[0] * get_coeff_c(dev, adc_meter->adc_reg[0]) / NANO_MICRO,
			adc_meter->current_val[0],

			adc_meter->adc_reg[1],
			adc_meter->current_val[1] * get_coeff_c(dev, adc_meter->adc_reg[1]) / NANO_MICRO,
			adc_meter->current_val[1],

			adc_meter->adc_reg[2],
			adc_meter->current_val[2] * get_coeff_c(dev, adc_meter->adc_reg[2]) / NANO_MICRO,
			adc_meter->current_val[2],

			adc_meter->adc_reg[3],
			adc_meter->current_val[3] * get_coeff_c(dev, adc_meter->adc_reg[3]) / NANO_MICRO,
			adc_meter->current_val[3],

			adc_meter->adc_reg[4],
			adc_meter->current_val[4] * get_coeff_c(dev, adc_meter->adc_reg[4]) / NANO_MICRO,
			adc_meter->current_val[4],

			adc_meter->adc_reg[5],
			adc_meter->current_val[5] * get_coeff_c(dev, adc_meter->adc_reg[5]) / NANO_MICRO,
			adc_meter->current_val[5],

			adc_meter->adc_reg[6],
			adc_meter->current_val[6] * get_coeff_c(dev, adc_meter->adc_reg[6]) / NANO_MICRO,
			adc_meter->current_val[6],

			adc_meter->adc_reg[7],
			adc_meter->current_val[7] * get_coeff_c(dev, adc_meter->adc_reg[7]) / NANO_MICRO,
			adc_meter->current_val[7]);
}

static ssize_t adc_en_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	u8 adc_ctrl3;
	s2mps22_read_reg(adc_meter->i2c, S2MPS22_REG_ADC_CTRL3, &adc_ctrl3);

	if (adc_ctrl3 & ADC_EN_MASK)
		return snprintf(buf, PAGE_SIZE, "ADC enable (0x%02hhx)\n", adc_ctrl3);
	else
		return snprintf(buf, PAGE_SIZE, "ADC disable (0x%02hhx)\n", adc_ctrl3);
}

static ssize_t adc_en_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	int ret;
	u8 temp, val;

	ret = kstrtou8(buf, 16, &temp);
	if (ret)
		return -EINVAL;
	else {
		switch (temp) {
		case 0 :
			val = 0x00;
			break;
		case 1 :
			val = 0x80;
			break;
		default :
			val = 0x00;
			break;
		}
	}

	s2mps22_update_reg(adc_meter->i2c,
			   S2MPS22_REG_ADC_CTRL3, val, ADC_EN_MASK);
	return count;
}

static ssize_t adc_mode_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	switch (adc_meter->adc_mode) {
		case CURRENT_METER :
			return snprintf(buf, PAGE_SIZE,
					"CURRENT MODE (%d)\n", CURRENT_METER);
		case POWER_METER :
			return snprintf(buf, PAGE_SIZE,
					"POWER MODE (%d)\n", POWER_METER);
		default :
			return snprintf(buf, PAGE_SIZE, "error\n");
	}
}

static ssize_t adc_mode_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	int ret;
	u8 temp;

	ret = kstrtou8(buf, 16, &temp);

	if (ret)
		return -EINVAL;
	else {
		switch (temp) {
		case CURRENT_METER :
			adc_meter->adc_mode = CURRENT_METER;
			adc_meter->ptr_base = CURRENT_PTR_BASE;
			break;
		case POWER_METER :
			adc_meter->adc_mode = POWER_METER;
			adc_meter->ptr_base = POWER_PTR_BASE;
			break;
		default :
			adc_meter->adc_mode = CURRENT_METER;
			adc_meter->ptr_base = CURRENT_PTR_BASE;
			break;
		}
		return count;
	}
}

static ssize_t adc_sync_mode_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{

	switch (adc_meter->adc_sync_mode) {
	case SYNC_MODE:
		return snprintf(buf, PAGE_SIZE,
				"SYNC_MODE (%d)\n", adc_meter->adc_sync_mode);
	case ASYNC_MODE:
		return snprintf(buf, PAGE_SIZE,
				"ASYNC_MODE (%d)\n", adc_meter->adc_sync_mode);
	default:
		return snprintf(buf, PAGE_SIZE,
				"error (0x%02hhx)\n", adc_meter->adc_sync_mode);
	}
}

static ssize_t adc_sync_mode_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	int ret;
	u8 temp;

	ret = kstrtou8(buf, 16, &temp);
	if (ret)
		return -EINVAL;

	switch (temp) {
	case SYNC_MODE:
		adc_meter->adc_sync_mode = SYNC_MODE;
		break;
	case ASYNC_MODE:
		adc_meter->adc_sync_mode = ASYNC_MODE;
		break;
	default:
		adc_meter->adc_sync_mode = SYNC_MODE;
		break;
	}
	return count;
}

static int adc_val_show_function(char *buf, unsigned int coeff_c, int chan)
{
	if((adc_meter->adc_mode == POWER_METER) &&
	   (adc_meter->adc_reg[chan] >= S2MPS22_LDO_START) &&
	   (adc_meter->adc_reg[chan] <= S2MPS22_LDO_END))
		return snprintf(buf, PAGE_SIZE,
				"[CH%d] Power-Meter for LDOs Not Supported, "
				"%d(0x%x) uA\n", chan,
				adc_meter->current_val[chan] * coeff_c / NANO_MICRO,
				adc_meter->current_val[chan]);

	return 0;
}

static int convert_adc_val(struct device *dev, char *buf, int channel)
{
	unsigned int coeff_p = get_coeff_p(dev, adc_meter->adc_reg[channel]);
	unsigned int coeff_c = get_coeff_c(dev, adc_meter->adc_reg[channel]);
	int ret;

	s2m_adc_read_data(dev, channel);

	ret = adc_val_show_function(buf, coeff_c, channel);
	if (ret)
		return ret;

	return snprintf(buf, PAGE_SIZE, "[CH%d] %d(0x%x)uA, %d(0x%x) uW\n",
			channel,
			adc_meter->current_val[channel] * coeff_c / NANO_MICRO,
			adc_meter->current_val[channel],
			adc_meter->power_val[channel] * coeff_p / PICO_MICRO,
			adc_meter->power_val[channel]);
}

static ssize_t adc_val_0_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	return convert_adc_val(dev, buf, 0);
}

static ssize_t adc_val_1_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	return convert_adc_val(dev, buf, 1);
}

static ssize_t adc_val_2_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	return convert_adc_val(dev, buf, 2);
}

static ssize_t adc_val_3_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	return convert_adc_val(dev, buf, 3);
}

static ssize_t adc_val_4_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	return convert_adc_val(dev, buf, 4);
}

static ssize_t adc_val_5_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	return convert_adc_val(dev, buf, 5);
}

static ssize_t adc_val_6_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	return convert_adc_val(dev, buf, 6);
}

static ssize_t adc_val_7_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	return convert_adc_val(dev, buf, 7);
}

static ssize_t adc_reg_0_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%02hhx\n", adc_meter->adc_reg[0]);
}

static ssize_t adc_reg_1_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%02hhx\n", adc_meter->adc_reg[1]);
}

static ssize_t adc_reg_2_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%02hhx\n", adc_meter->adc_reg[2]);
}

static ssize_t adc_reg_3_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%02hhx\n", adc_meter->adc_reg[3]);
}

static ssize_t adc_reg_4_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%02hhx\n", adc_meter->adc_reg[4]);
}

static ssize_t adc_reg_5_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%02hhx\n", adc_meter->adc_reg[5]);
}

static ssize_t adc_reg_6_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%02hhx\n", adc_meter->adc_reg[6]);
}

static ssize_t adc_reg_7_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%02hhx\n", adc_meter->adc_reg[7]);
}

static void adc_reg_update(struct device *dev)
{
	size_t i = 0;

	/* ADC OFF */
	s2mps22_update_reg(adc_meter->i2c, S2MPS22_REG_ADC_CTRL3,
			   0x00, ADC_EN_MASK);

	/* CHANNEL setting */
	for (i = 0; i < S2MPS22_MAX_ADC_CHANNEL; i++) {
		s2mps22_update_reg(adc_meter->i2c, S2MPS22_REG_ADC_CTRL3,
				   i + MUX_PTR_BASE, ADC_PTR_MASK);
		s2mps22_write_reg(adc_meter->i2c, S2MPS22_REG_ADC_DATA,
				   adc_meter->adc_reg[i]);
	}

	/* ADC EN */
	switch (adc_meter->adc_mode) {
	case CURRENT_METER :
		adc_meter->adc_mode = CURRENT_METER;
		adc_meter->ptr_base = CURRENT_PTR_BASE;
		pr_info("%s: current mode enable\n", __func__);
		break;
	case POWER_METER:
		adc_meter->adc_mode = POWER_METER;
		adc_meter->ptr_base = POWER_PTR_BASE;
		pr_info("%s: power mode enable\n",  __func__);
		break;
	default :
		adc_meter->adc_mode = CURRENT_METER;
		adc_meter->ptr_base = CURRENT_PTR_BASE;
		pr_info("%s: current mode enable\n", __func__);
		break;
	}
}

static u8 buf_to_adc_reg(const char *buf)
{
	u8 adc_reg_num;

	if (kstrtou8(buf, 16, &adc_reg_num))
		return 0;

	if ((adc_reg_num >= S2MPS22_BUCK_START &&
	    adc_reg_num <= S2MPS22_BUCK_END) ||
	    (adc_reg_num >= S2MPS22_LDO_START && adc_reg_num <= S2MPS22_LDO_END))
		return adc_reg_num;
	else
		return 0;
}

static ssize_t assign_adc_reg(struct device *dev, const char *buf,
			      size_t count, int channel)
{
	u8 adc_reg_num = buf_to_adc_reg(buf);
	if (!adc_reg_num)
		return -EINVAL;
	else {
		adc_meter->adc_reg[channel] = adc_reg_num;
		adc_reg_update(dev);
		return count;
	}
}

static ssize_t adc_reg_0_store(struct device *dev, struct device_attribute *attr,
			       const char *buf, size_t count)
{
	return assign_adc_reg(dev, buf, count, 0);
}

static ssize_t adc_reg_1_store(struct device *dev, struct device_attribute *attr,
			       const char *buf, size_t count)
{
	return assign_adc_reg(dev, buf, count, 1);
}

static ssize_t adc_reg_2_store(struct device *dev, struct device_attribute *attr,
			       const char *buf, size_t count)
{
	return assign_adc_reg(dev, buf, count, 2);
}

static ssize_t adc_reg_3_store(struct device *dev, struct device_attribute *attr,
			       const char *buf, size_t count)
{
	return assign_adc_reg(dev, buf, count, 3);
}

static ssize_t adc_reg_4_store(struct device *dev, struct device_attribute *attr,
			       const char *buf, size_t count)
{
	return assign_adc_reg(dev, buf, count, 4);
}

static ssize_t adc_reg_5_store(struct device *dev, struct device_attribute *attr,
			       const char *buf, size_t count)
{
	return assign_adc_reg(dev, buf, count, 5);
}

static ssize_t adc_reg_6_store(struct device *dev, struct device_attribute *attr,
			       const char *buf, size_t count)
{
	return assign_adc_reg(dev, buf, count, 6);
}

static ssize_t adc_reg_7_store(struct device *dev, struct device_attribute *attr,
			       const char *buf, size_t count)
{
	return assign_adc_reg(dev, buf, count, 7);
}

static void adc_ctrl1_update(struct device *dev)
{
	/* ADC temporarily off */
	s2mps22_update_reg(adc_meter->i2c, S2MPS22_REG_ADC_CTRL3,
			   0x00, ADC_EN_MASK);

	/* update ADC_CTRL1 register */
	s2mps22_write_reg(adc_meter->i2c, S2MPS22_REG_ADC_CTRL1,
			  adc_meter->adc_ctrl1);

	/* ADC Continuous ON */
	s2mps22_update_reg(adc_meter->i2c, S2MPS22_REG_ADC_CTRL3,
			   ADC_EN_MASK, ADC_EN_MASK);
}

static ssize_t adc_ctrl1_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%02hhx\n", adc_meter->adc_ctrl1);
}

static ssize_t adc_ctrl1_store(struct device *dev, struct device_attribute *attr,
			       const char *buf, size_t count)
{
	int ret;
	u8 temp;

	ret = kstrtou8(buf, 16, &temp);
	if (ret)
		return -EINVAL;
	else {
		temp &= 0x0f;
		adc_meter->adc_ctrl1 &= 0xf0;
		adc_meter->adc_ctrl1 |= temp;
		adc_ctrl1_update(dev);
		return count;
	}
}

static DEVICE_ATTR(power_val_all, 0444, adc_val_power_show, NULL);
static DEVICE_ATTR(current_val_all, 0444, adc_val_current_show, NULL);
static DEVICE_ATTR(adc_en, 0644, adc_en_show, adc_en_store);
static DEVICE_ATTR(adc_mode, 0644, adc_mode_show, adc_mode_store);
static DEVICE_ATTR(adc_sync_mode, 0644, adc_sync_mode_show, adc_sync_mode_store);
static DEVICE_ATTR(adc_val_0, 0444, adc_val_0_show, NULL);
static DEVICE_ATTR(adc_val_1, 0444, adc_val_1_show, NULL);
static DEVICE_ATTR(adc_val_2, 0444, adc_val_2_show, NULL);
static DEVICE_ATTR(adc_val_3, 0444, adc_val_3_show, NULL);
static DEVICE_ATTR(adc_val_4, 0444, adc_val_4_show, NULL);
static DEVICE_ATTR(adc_val_5, 0444, adc_val_5_show, NULL);
static DEVICE_ATTR(adc_val_6, 0444, adc_val_6_show, NULL);
static DEVICE_ATTR(adc_val_7, 0444, adc_val_7_show, NULL);
static DEVICE_ATTR(adc_reg_0, 0644, adc_reg_0_show, adc_reg_0_store);
static DEVICE_ATTR(adc_reg_1, 0644, adc_reg_1_show, adc_reg_1_store);
static DEVICE_ATTR(adc_reg_2, 0644, adc_reg_2_show, adc_reg_2_store);
static DEVICE_ATTR(adc_reg_3, 0644, adc_reg_3_show, adc_reg_3_store);
static DEVICE_ATTR(adc_reg_4, 0644, adc_reg_4_show, adc_reg_4_store);
static DEVICE_ATTR(adc_reg_5, 0644, adc_reg_5_show, adc_reg_5_store);
static DEVICE_ATTR(adc_reg_6, 0644, adc_reg_6_show, adc_reg_6_store);
static DEVICE_ATTR(adc_reg_7, 0644, adc_reg_7_show, adc_reg_7_store);
static DEVICE_ATTR(adc_ctrl1, 0644, adc_ctrl1_show, adc_ctrl1_store);

int create_s2mps22_powermeter_sysfs(struct s2mps22_dev *s2mps22)
{
	struct device *s2mps22_adc_dev;
	int err = -ENODEV;

	pr_info("%s: slave pmic powermeter sysfs start\n", __func__);

	s2mps22_adc_dev = pmic_device_create(s2mps22, "s2mps22_adc");
	s2mps22->powermeter_dev = s2mps22_adc_dev;

	/* create sysfs entries */
	err = device_create_file(s2mps22_adc_dev, &dev_attr_adc_en);
	if (err)
		goto remove_pmic_device;
	err = device_create_file(s2mps22_adc_dev, &dev_attr_adc_mode);
	if (err)
		goto remove_adc_en;
	err = device_create_file(s2mps22_adc_dev, &dev_attr_adc_sync_mode);
	if (err)
		goto remove_adc_mode;
	err = device_create_file(s2mps22_adc_dev, &dev_attr_power_val_all);
	if (err)
		goto remove_adc_sync_mode;
	err = device_create_file(s2mps22_adc_dev, &dev_attr_current_val_all);
	if (err)
		goto remove_power_val_all;
	err = device_create_file(s2mps22_adc_dev, &dev_attr_adc_val_0);
	if (err)
		goto remove_current_val_all;
	err = device_create_file(s2mps22_adc_dev, &dev_attr_adc_val_1);
	if (err)
		goto remove_adc_val_0;
	err = device_create_file(s2mps22_adc_dev, &dev_attr_adc_val_2);
	if (err)
		goto remove_adc_val_1;
	err = device_create_file(s2mps22_adc_dev, &dev_attr_adc_val_3);
	if (err)
		goto remove_adc_val_2;
	err = device_create_file(s2mps22_adc_dev, &dev_attr_adc_val_4);
	if (err)
		goto remove_adc_val_3;
	err = device_create_file(s2mps22_adc_dev, &dev_attr_adc_val_5);
	if (err)
		goto remove_adc_val_4;
	err = device_create_file(s2mps22_adc_dev, &dev_attr_adc_val_6);
	if (err)
		goto remove_adc_val_5;
	err = device_create_file(s2mps22_adc_dev, &dev_attr_adc_val_7);
	if (err)
		goto remove_adc_val_6;
	err = device_create_file(s2mps22_adc_dev, &dev_attr_adc_reg_0);
	if (err)
		goto remove_adc_val_7;
	err = device_create_file(s2mps22_adc_dev, &dev_attr_adc_reg_1);
	if (err)
		goto remove_adc_reg_0;
	err = device_create_file(s2mps22_adc_dev, &dev_attr_adc_reg_2);
	if (err)
		goto remove_adc_reg_1;
	err = device_create_file(s2mps22_adc_dev, &dev_attr_adc_reg_3);
	if (err)
		goto remove_adc_reg_2;
	err = device_create_file(s2mps22_adc_dev, &dev_attr_adc_reg_4);
	if (err)
		goto remove_adc_reg_3;
	err = device_create_file(s2mps22_adc_dev, &dev_attr_adc_reg_5);
	if (err)
		goto remove_adc_reg_4;
	err = device_create_file(s2mps22_adc_dev, &dev_attr_adc_reg_6);
	if (err)
		goto remove_adc_reg_5;
	err = device_create_file(s2mps22_adc_dev, &dev_attr_adc_reg_7);
	if (err)
		goto remove_adc_reg_6;
	err = device_create_file(s2mps22_adc_dev, &dev_attr_adc_ctrl1);
	if (err)
		goto remove_adc_reg_7;

#ifdef CONFIG_SEC_PM
	if (!IS_ERR_OR_NULL(s2mps22->ap_sub_pmic_dev)) {
		err = sysfs_create_link(&s2mps22->ap_sub_pmic_dev->kobj,
				&s2mps22_adc_dev->kobj, "power_meter");
		if (err)
			pr_err("%s: fail to create link for power_meter(%d)\n",
					__func__, err);
	}
#endif /* CONFIG_SEC_PM */

	return 0;

remove_adc_reg_7:
	device_remove_file(s2mps22_adc_dev, &dev_attr_adc_reg_7);
remove_adc_reg_6:
	device_remove_file(s2mps22_adc_dev, &dev_attr_adc_reg_6);
remove_adc_reg_5:
	device_remove_file(s2mps22_adc_dev, &dev_attr_adc_reg_5);
remove_adc_reg_4:
	device_remove_file(s2mps22_adc_dev, &dev_attr_adc_reg_4);
remove_adc_reg_3:
	device_remove_file(s2mps22_adc_dev, &dev_attr_adc_reg_3);
remove_adc_reg_2:
	device_remove_file(s2mps22_adc_dev, &dev_attr_adc_reg_2);
remove_adc_reg_1:
	device_remove_file(s2mps22_adc_dev, &dev_attr_adc_reg_1);
remove_adc_reg_0:
	device_remove_file(s2mps22_adc_dev, &dev_attr_adc_reg_0);
remove_adc_val_7:
	device_remove_file(s2mps22_adc_dev, &dev_attr_adc_val_7);
remove_adc_val_6:
	device_remove_file(s2mps22_adc_dev, &dev_attr_adc_val_6);
remove_adc_val_5:
	device_remove_file(s2mps22_adc_dev, &dev_attr_adc_val_5);
remove_adc_val_4:
	device_remove_file(s2mps22_adc_dev, &dev_attr_adc_val_4);
remove_adc_val_3:
	device_remove_file(s2mps22_adc_dev, &dev_attr_adc_val_3);
remove_adc_val_2:
	device_remove_file(s2mps22_adc_dev, &dev_attr_adc_val_2);
remove_adc_val_1:
	device_remove_file(s2mps22_adc_dev, &dev_attr_adc_val_1);
remove_adc_val_0:
	device_remove_file(s2mps22_adc_dev, &dev_attr_adc_val_0);
remove_current_val_all:
	device_remove_file(s2mps22_adc_dev, &dev_attr_current_val_all);
remove_power_val_all:
	device_remove_file(s2mps22_adc_dev, &dev_attr_power_val_all);
remove_adc_sync_mode:
	device_remove_file(s2mps22_adc_dev, &dev_attr_adc_sync_mode);
remove_adc_mode:
	device_remove_file(s2mps22_adc_dev, &dev_attr_adc_mode);
remove_adc_en:
	device_remove_file(s2mps22_adc_dev, &dev_attr_adc_en);
remove_pmic_device:
	pmic_device_destroy(s2mps22->dev->devt);

	return 1;
}
#endif
void s2mps22_powermeter_init(struct s2mps22_dev *s2mps22)
{
	size_t i = 0;
#ifdef CONFIG_DRV_SAMSUNG_PMIC
	int ret;
#endif

	adc_meter = kzalloc(sizeof(struct adc_info), GFP_KERNEL);
	if (!adc_meter) {
		pr_err("%s: adc_meter alloc fail.\n", __func__);
		return;
	}

	adc_meter->current_val = kzalloc(sizeof(u16) * S2MPS22_MAX_ADC_CHANNEL,
					 GFP_KERNEL);
	adc_meter->power_val = kzalloc(sizeof(u16) * S2MPS22_MAX_ADC_CHANNEL,
				       GFP_KERNEL);
	adc_meter->adc_reg = kzalloc(sizeof(u8) * S2MPS22_MAX_ADC_CHANNEL,
				     GFP_KERNEL);

	pr_info("%s: s2mps22 power meter init start\n", __func__);

	/* initial regulators : BUCK 1,2,3,4, LDO 1,2,3,4 */
	adc_meter->adc_reg[0] = 0x1;
	adc_meter->adc_reg[1] = 0x2;
	adc_meter->adc_reg[2] = 0x3;
	adc_meter->adc_reg[3] = 0x4;
	adc_meter->adc_reg[4] = 0x1;
	adc_meter->adc_reg[5] = 0x2;
	adc_meter->adc_reg[6] = 0x3;
	adc_meter->adc_reg[7] = 0x4;

	adc_meter->adc_mode = s2mps22->adc_mode;
	adc_meter->adc_sync_mode = s2mps22->adc_sync_mode;
	mutex_init(&adc_meter->adc_lock);

	/* SMP_NUM=1011(16384), RATIO=10(125khz), (8us x 16384 x 16 x 8ch)=~16s in case of async mode */
	if (adc_meter->adc_sync_mode == ASYNC_MODE) {
		adc_meter->adc_ctrl1 = 0x2B;
		s2mps22_write_reg(s2mps22->pmic, S2MPS22_REG_ADC_CTRL1,
				  adc_meter->adc_ctrl1);
	}

	/* enable DC offset calibration */
	s2mps22_update_reg(s2mps22->pmic, S2MPS22_REG_ADC_CTRL2,
			   ADC_CAL_EN_MASK, ADC_CAL_EN_MASK);

	/* CHANNEL setting */
	for (i = 0; i < S2MPS22_MAX_ADC_CHANNEL; i++) {
		s2mps22_update_reg(s2mps22->pmic, S2MPS22_REG_ADC_CTRL3,
				   i + MUX_PTR_BASE, ADC_PTR_MASK);
		s2mps22_write_reg(s2mps22->pmic, S2MPS22_REG_ADC_DATA,
				  adc_meter->adc_reg[i]);
	}

	/* set ptr_base according to adc_mode */
	switch (adc_meter->adc_mode) {
		case CURRENT_METER :
			adc_meter->ptr_base = CURRENT_PTR_BASE;
			break;
		case POWER_METER :
			adc_meter->ptr_base = POWER_PTR_BASE;
			break;
		default :
			adc_meter->adc_mode = CURRENT_METER;
			adc_meter->ptr_base = CURRENT_PTR_BASE;
			break;
	}

	/* turn on ADC */
	s2mps22_update_reg(s2mps22->pmic, S2MPS22_REG_ADC_CTRL3,
			   ADC_EN_MASK, ADC_EN_MASK);

	adc_meter->i2c = s2mps22->pmic;

	/* create sysfs entries */
#ifdef CONFIG_DRV_SAMSUNG_PMIC
	ret = create_s2mps22_powermeter_sysfs(s2mps22);
	if (ret) {
		pr_info("%s: failed to create sysfs\n", __func__);
		kfree(adc_meter->adc_val);
		kfree(adc_meter->adc_reg);

		return;
	}
#endif
	pr_info("%s: s2mps22 power meter init end\n", __func__);
}

void s2mps22_powermeter_deinit(struct s2mps22_dev *s2mps22)
{
#ifdef CONFIG_DRV_SAMSUNG_PMIC
	struct device *s2mps22_adc_dev = s2mps22->powermeter_dev;
	/* remove sysfs entries */
	device_remove_file(s2mps22_adc_dev, &dev_attr_adc_en);
	device_remove_file(s2mps22_adc_dev, &dev_attr_adc_mode);
	device_remove_file(s2mps22_adc_dev, &dev_attr_adc_sync_mode);
	device_remove_file(s2mps22_adc_dev, &dev_attr_power_val_all);
	device_remove_file(s2mps22_adc_dev, &dev_attr_current_val_all);
	device_remove_file(s2mps22_adc_dev, &dev_attr_adc_val_0);
	device_remove_file(s2mps22_adc_dev, &dev_attr_adc_val_1);
	device_remove_file(s2mps22_adc_dev, &dev_attr_adc_val_2);
	device_remove_file(s2mps22_adc_dev, &dev_attr_adc_val_3);
	device_remove_file(s2mps22_adc_dev, &dev_attr_adc_val_4);
	device_remove_file(s2mps22_adc_dev, &dev_attr_adc_val_5);
	device_remove_file(s2mps22_adc_dev, &dev_attr_adc_val_6);
	device_remove_file(s2mps22_adc_dev, &dev_attr_adc_val_7);
	device_remove_file(s2mps22_adc_dev, &dev_attr_adc_reg_0);
	device_remove_file(s2mps22_adc_dev, &dev_attr_adc_reg_1);
	device_remove_file(s2mps22_adc_dev, &dev_attr_adc_reg_2);
	device_remove_file(s2mps22_adc_dev, &dev_attr_adc_reg_3);
	device_remove_file(s2mps22_adc_dev, &dev_attr_adc_reg_4);
	device_remove_file(s2mps22_adc_dev, &dev_attr_adc_reg_5);
	device_remove_file(s2mps22_adc_dev, &dev_attr_adc_reg_6);
	device_remove_file(s2mps22_adc_dev, &dev_attr_adc_reg_7);
	device_remove_file(s2mps22_adc_dev, &dev_attr_adc_ctrl1);
	pmic_device_destroy(s2mps22->dev->devt);
#endif
	/* ADC turned off */
	s2mps22_update_reg(s2mps22->pmic, S2MPS22_REG_ADC_CTRL3, 0, ADC_EN_MASK);
	kfree(adc_meter->adc_val);
	kfree(adc_meter->adc_reg);
}
