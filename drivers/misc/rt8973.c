/* drivers/mfd/rt8973.c
 * Richtek RT8973 Multifunction Device / MUIC Driver
 *
 * Copyright (C) 2013
 * Author: Patrick Chang <patrick_chang@richtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */
#include <linux/kernel.h>
#include <linux/rtdefs.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/wakelock.h>
#if defined (CONFIG_OF)
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif

#include <linux/rt8973.h>
#include <plat/udc-hs.h>
#include <asm/system_info.h>

struct rt8973_muic_data *g_data;
struct device_desc {
	char *name;
	uint32_t reg_val;
	int cable_type;
};

static struct device_desc device_to_cable_type_mapping[] = {
	{
	 .name = "OTG",
	 .reg_val = RT8973_DEVICE1_OTG,
	 .cable_type = ATTACHED_DEV_OTG_MUIC,
	 },
	{
	 .name = "USB SDP",
	 .reg_val = RT8973_DEVICE1_SDP,
	 .cable_type = ATTACHED_DEV_USB_MUIC,
	 },
	{
	 .name = "UART",
	 .reg_val = RT8973_DEVICE1_UART,
	 .cable_type = ATTACHED_DEV_UART_MUIC,
	 },
	{
	 .name = "USB CDP",
	 .reg_val = RT8973_DEVICE1_CDPORT,
	 .cable_type = ATTACHED_DEV_CDP_MUIC,
	 },
	{
	 .name = "USB DCP",
	 .reg_val = RT8973_DEVICE1_DCPORT,
	 .cable_type = ATTACHED_DEV_TA_MUIC,
	 },
};

struct id_desc {
	char *name;
	enum muic_attached_dev cable_type_with_vbus;
	enum muic_attached_dev cable_type_without_vbus;
};

static struct id_desc id_to_cable_type_mapping[] = {
	{			/* 00000,  0, 0x00 */
	 .name = "OTG",
	 .cable_type_with_vbus = ATTACHED_DEV_OTG_MUIC,
	 .cable_type_without_vbus = ATTACHED_DEV_OTG_MUIC,
	 },
	{			/* 00001,  1, 0x01 */
	 .name = "UNKNOWN",
	 .cable_type_with_vbus = ATTACHED_DEV_UNKNOWN_VB_MUIC,
	 .cable_type_without_vbus = ATTACHED_DEV_UNKNOWN_MUIC,
	 },
	{			/* 00010,  2, 0x02 */
	 .name = "UNKNOWN",
	 .cable_type_with_vbus = ATTACHED_DEV_UNKNOWN_VB_MUIC,
	 .cable_type_without_vbus = ATTACHED_DEV_UNKNOWN_MUIC,
	 },
	{			/* 00011,  3, 0x03 */
	 .name = "UNKNOWN",
	 .cable_type_with_vbus = ATTACHED_DEV_UNKNOWN_VB_MUIC,
	 .cable_type_without_vbus = ATTACHED_DEV_UNKNOWN_MUIC,
	 },
	{			/* 00100,  4, 0x04 */
	 .name = "UNKNOWN",
	 .cable_type_with_vbus = ATTACHED_DEV_UNKNOWN_VB_MUIC,
	 .cable_type_without_vbus = ATTACHED_DEV_UNKNOWN_MUIC,
	 },
	{			/* 00101,  5, 0x05 */
	 .name = "UNKNOWN",
	 .cable_type_with_vbus = ATTACHED_DEV_UNKNOWN_VB_MUIC,
	 .cable_type_without_vbus = ATTACHED_DEV_UNKNOWN_MUIC,
	 },
	{			/* 00110,  6, 0x06 */
	 .name = "UNKNOWN",
	 .cable_type_with_vbus = ATTACHED_DEV_UNKNOWN_VB_MUIC,
	 .cable_type_without_vbus = ATTACHED_DEV_UNKNOWN_MUIC,
	 },
	{			/* 00111,  7, 0x07 */
	 .name = "UNKNOWN",
	 .cable_type_with_vbus = ATTACHED_DEV_UNKNOWN_VB_MUIC,
	 .cable_type_without_vbus = ATTACHED_DEV_UNKNOWN_MUIC,
	 },
	{			/* 01000,  8, 0x08 */
	 .name = "UNKNOWN",
	 .cable_type_with_vbus = ATTACHED_DEV_UNKNOWN_VB_MUIC,
	 .cable_type_without_vbus = ATTACHED_DEV_UNKNOWN_MUIC,
	 },
	{			/* 01001,  9, 0x09 */
	 .name = "UNKNOWN",
	 .cable_type_with_vbus = ATTACHED_DEV_UNKNOWN_VB_MUIC,
	 .cable_type_without_vbus = ATTACHED_DEV_UNKNOWN_MUIC,
	 },
	{			/* 01010, 10, 0x0A */
	 .name = "UNKNOWN",
	 .cable_type_with_vbus = ATTACHED_DEV_UNKNOWN_VB_MUIC,
	 .cable_type_without_vbus = ATTACHED_DEV_UNKNOWN_MUIC,
	 },
	{			/* 01011, 11, 0x0B */
	 .name = "UNKNOWN",
	 .cable_type_with_vbus = ATTACHED_DEV_UNKNOWN_VB_MUIC,
	 .cable_type_without_vbus = ATTACHED_DEV_UNKNOWN_MUIC,
	 },
	{			/* 01100, 12, 0x0C */
	 .name = "UNKNOWN",
	 .cable_type_with_vbus = ATTACHED_DEV_UNKNOWN_VB_MUIC,
	 .cable_type_without_vbus = ATTACHED_DEV_UNKNOWN_MUIC,
	 },
	{			/* 01101, 13, 0x0D */
	 .name = "UNKNOWN",
	 .cable_type_with_vbus = ATTACHED_DEV_UNKNOWN_VB_MUIC,
	 .cable_type_without_vbus = ATTACHED_DEV_UNKNOWN_MUIC,
	 },
	{			/* 01110, 14, 0x0E */
	 .name = "UNKNOWN",
	 .cable_type_with_vbus = ATTACHED_DEV_UNKNOWN_VB_MUIC,
	 .cable_type_without_vbus = ATTACHED_DEV_UNKNOWN_MUIC,
	 },
	{			/* 01111, 15, 0x0F, VZW Incompatible */
	 .name = "UNKNOWN",
	 .cable_type_with_vbus = ATTACHED_DEV_UNKNOWN_MUIC,
	 .cable_type_without_vbus = ATTACHED_DEV_UNKNOWN_MUIC,
	 },
	{			/* 10000, 16, 0x10 */
	 .name = "UNKNOWN",
	 .cable_type_with_vbus = ATTACHED_DEV_UNKNOWN_VB_MUIC,
	 .cable_type_without_vbus = ATTACHED_DEV_UNKNOWN_MUIC,
	 },
	{			/* 10001, 17, 0x11 */
	 .name = "UNKNOWN",
	 .cable_type_with_vbus = ATTACHED_DEV_UNKNOWN_VB_MUIC,
	 .cable_type_without_vbus = ATTACHED_DEV_UNKNOWN_MUIC,
	 },
	{			/* 10010, 18, 0x12 */
	 .name = "UNKNOWN",
	 .cable_type_with_vbus = ATTACHED_DEV_UNKNOWN_VB_MUIC,
	 .cable_type_without_vbus = ATTACHED_DEV_UNKNOWN_MUIC,
	 },
	{			/* 10011, 19, 0x13 */
	 .name = "UNKNOWN",
	 .cable_type_with_vbus = ATTACHED_DEV_UNKNOWN_VB_MUIC,
	 .cable_type_without_vbus = ATTACHED_DEV_UNKNOWN_MUIC,
	 },
	{			/* 10100, 20, 0x14 */
	 .name = "PS_CABLE",
	 .cable_type_with_vbus = ATTACHED_DEV_PS_CABLE_MUIC,
	 .cable_type_without_vbus = ATTACHED_DEV_PS_CABLE_MUIC,
	 },
	{			/* 10101, 21, 0x15 */
	/* charging in original code */
	 .name = "UNKNOWN",
	 .cable_type_with_vbus = ATTACHED_DEV_UNKNOWN_VB_MUIC,
	 .cable_type_without_vbus = ATTACHED_DEV_UNKNOWN_MUIC,
	 },
	{			/* 10110, 22, 0x16 */
	 .name = "UNKNOWN",
	 .cable_type_with_vbus = ATTACHED_DEV_UNKNOWN_VB_MUIC,
	 .cable_type_without_vbus = ATTACHED_DEV_UNKNOWN_MUIC,
	 },
	{			/* 10111, 23, 0x17 */
	 .name = "200K",
	 .cable_type_with_vbus = ATTACHED_DEV_200K_MUIC,
	 .cable_type_without_vbus = ATTACHED_DEV_UNKNOWN_MUIC,
	 },
	{			/* 11000, 24, 0x18 */
	 .name = "JIG USB OFF",
	 .cable_type_with_vbus = ATTACHED_DEV_JIG_USB_OFF_MUIC,
	 .cable_type_without_vbus = ATTACHED_DEV_JIG_USB_OFF_MUIC,
	 },
	{			/* 11001, 25, 0x19 */
	 .name = "JIG USB ON",
	 .cable_type_with_vbus = ATTACHED_DEV_JIG_USB_ON_MUIC,
	 .cable_type_without_vbus = ATTACHED_DEV_JIG_USB_ON_MUIC,
	 },
	{			/* 11010, 26, 0x1A */
	 .name = "DESKDOCK",
	 .cable_type_with_vbus = ATTACHED_DEV_DESKDOCK_MUIC,
	 .cable_type_without_vbus = ATTACHED_DEV_DESKDOCK_MUIC,
	 },
	{			/* 11011, 27, 0x1B */
	 .name = "UNKNOWN",
	 .cable_type_with_vbus = ATTACHED_DEV_UNKNOWN_VB_MUIC,
	 .cable_type_without_vbus = ATTACHED_DEV_UNKNOWN_MUIC,
	 },
	{			/* 11100, 28, 0x1C */
	 .name = "JIG UART OFF",
	 .cable_type_with_vbus = ATTACHED_DEV_JIG_UART_OFF_VB_MUIC,
	 .cable_type_without_vbus = ATTACHED_DEV_JIG_UART_OFF_MUIC,
	 },
	{			/* 11101, 29, 0x1D */
	 .name = "JIG UART ON",
	 .cable_type_with_vbus = ATTACHED_DEV_JIG_UART_ON_MUIC,
	 .cable_type_without_vbus = ATTACHED_DEV_JIG_UART_ON_MUIC,
	 },
	{			/* 11110, 30, 0x1E */
	 .name = "UNKNOWN",
	 .cable_type_with_vbus = ATTACHED_DEV_UNKNOWN_VB_MUIC,
	 .cable_type_without_vbus = ATTACHED_DEV_UNKNOWN_MUIC,
	 },
	{			/* 11111, 31, 0x1F */
	 .name = "NO CABLE",
	 .cable_type_with_vbus = ATTACHED_DEV_UNKNOWN_MUIC,
	 .cable_type_without_vbus = ATTACHED_DEV_NONE_MUIC,
	 },
};

enum {
	VBUS_SHIFT = 0,
	ACCESSORY_SHIFT,
	OCP_SHIFT,
	OVP_SHIFT,
	OTP_SHIFT,
	ADC_CHG_SHIFT,
	CABLE_CHG_SHIFT,
	OTG_SHIFT,
	DCDT_SHIFT,
	USB_SHIFT,
	UART_SHIFT,
	JIG_SHIFT,
	L200K_USB_SHIFT,
	DOCK_SHIFT,
};

/* SAMSUNG MUIC CODE */
extern struct sec_switch_data sec_switch_data;

/* pdata from probe function. keep it for gpio control functions */
struct muic_platform_data muic_pdata;

/* don't access this variable directly!! except get_switch_sel_value function.
 * you must get switch_sel value by using get_switch_sel function. */
static int switch_sel;

/* func : set_switch_sel
 * switch_sel value get from bootloader comand line
 * switch_sel data consist 4 bits
 */
static int set_switch_sel(char *str)
{
	get_option(&str, &switch_sel);
	switch_sel = switch_sel & 0x0f;
	muic_dbg("%s switch_sel: 0x%x\n",  __func__,
			switch_sel);

	return switch_sel;
}
__setup("pmic_info=", set_switch_sel);

static int get_switch_sel(void)
{
	return switch_sel;
}
/* SAMSUNG MUIC CODE */

static int32_t rt8973_read(struct rt8973_muic_data *muic_data, uint8_t reg,
			   uint8_t nbytes, uint8_t *buff)
{
	int ret;
	mutex_lock(&muic_data->muic_mutex);
	ret = i2c_smbus_read_i2c_block_data(muic_data->i2c, reg, nbytes, buff);
	mutex_unlock(&muic_data->muic_mutex);
	return ret;
}

static int32_t rt8973_reg_read(struct rt8973_muic_data *muic_data, int reg)
{
	int ret;
	mutex_lock(&muic_data->muic_mutex);
	ret = i2c_smbus_read_byte_data(muic_data->i2c, reg);
	mutex_unlock(&muic_data->muic_mutex);
	return ret;
}

int32_t rt8973_reg_write(struct rt8973_muic_data *muic_data, int reg, unsigned char data)
{
	int ret;
	mutex_lock(&muic_data->muic_mutex);
	ret = i2c_smbus_write_byte_data(muic_data->i2c, reg, data);
	mutex_unlock(&muic_data->muic_mutex);
	return ret;
}

static int32_t rt8973_assign_bits(struct rt8973_muic_data *muic_data, int reg,
				  unsigned char mask, unsigned char data)
{
	struct i2c_client *i2c;
	int ret;
	i2c = muic_data->i2c;
	mutex_lock(&muic_data->muic_mutex);
	ret = i2c_smbus_read_byte_data(i2c, reg);
	if (ret < 0)
		goto out;
	ret &= ~mask;
	ret |= data;
	ret = i2c_smbus_write_byte_data(i2c, reg, ret);
out:
	mutex_unlock(&muic_data->muic_mutex);
	return ret;
}

int32_t rt8973_set_bits(struct rt8973_muic_data *muic_data, int reg, unsigned char mask)
{
	return rt8973_assign_bits(muic_data, reg, mask, mask);
}

int32_t rt8973_clr_bits(struct rt8973_muic_data *muic_data, int reg, unsigned char mask)
{
	return rt8973_assign_bits(muic_data, reg, mask, 0);
}

static inline int rt8973_update_regs(struct rt8973_muic_data *muic_data)
{
	int ret;
	ret = rt8973_read(muic_data, RT8973_REG_INT_FLAG1,
			  2, muic_data->curr_status.irq_flags);
	if (ret < 0)
		return ret;
	ret = rt8973_read(muic_data, RT8973_REG_DEVICE1,
			  2, muic_data->curr_status.device_reg);
	if (ret < 0)
		return ret;
	ret = rt8973_reg_read(muic_data, RT8973_REG_ADC);
	if (ret < 0)
		return ret;
	muic_data->curr_status.id_adc = ret;

	/* invalid id value */
	if (ret >= ARRAY_SIZE(id_to_cable_type_mapping))
		return -EINVAL;
	return 0;
}

static ssize_t rt8973_muic_show_uart_sel(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct rt8973_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *pdata = muic_data->pdata;

	switch (pdata->uart_path) {
	case MUIC_PATH_UART_AP:
		muic_dbg("%s AP\n",  __func__);
		return sprintf(buf, "AP\n");
	case MUIC_PATH_UART_CP:
		muic_dbg("%s CP\n",  __func__);
		return sprintf(buf, "CP\n");
	default:
		break;
	}

	muic_dbg("%s UNKNOWN\n",  __func__);
	return sprintf(buf, "UNKNOWN\n");
}

static int switch_to_ap_uart(struct rt8973_muic_data *muic_data);
static int switch_to_cp_uart(struct rt8973_muic_data *muic_data);

static ssize_t rt8973_muic_set_uart_sel(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct rt8973_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *pdata = muic_data->pdata;

	if (!strncasecmp(buf, "AP", 2)) {
		pdata->uart_path = MUIC_PATH_UART_AP;
		switch_to_ap_uart(muic_data);
	} else if (!strncasecmp(buf, "CP", 2)) {
		pdata->uart_path = MUIC_PATH_UART_CP;
		switch_to_cp_uart(muic_data);
	} else {
		muic_dbg("%s invalid value\n",  __func__);
	}

	muic_dbg("%s uart_path(%d)\n",  __func__,
			pdata->uart_path);

	return count;
}

static ssize_t rt8973_muic_show_usb_sel(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct rt8973_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *pdata = muic_data->pdata;

	switch (pdata->usb_path) {
	case MUIC_PATH_USB_AP:
		return sprintf(buf, "PDA\n");
	case MUIC_PATH_USB_CP:
		return sprintf(buf, "MODEM\n");
	default:
		break;
	}

	muic_dbg("%s UNKNOWN\n",  __func__);
	return sprintf(buf, "UNKNOWN\n");
}

static ssize_t rt8973_muic_set_usb_sel(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct rt8973_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *pdata = muic_data->pdata;

	if (!strncasecmp(buf, "PDA", 3)) {
		pdata->usb_path = MUIC_PATH_USB_AP;
	} else if (!strncasecmp(buf, "MODEM", 5)) {
		pdata->usb_path = MUIC_PATH_USB_CP;
	} else {
		muic_dbg("%s invalid value\n",  __func__);
	}

	muic_dbg("%s usb_path(%d)\n",  __func__,
			pdata->usb_path);

	return count;
}

static ssize_t rt8973_muic_show_adc(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct rt8973_muic_data *muic_data = dev_get_drvdata(dev);
	int ret;

	ret = rt8973_reg_read(muic_data, RT8973_REG_ADC);
	if (ret < 0) {
		muic_err("%s err read adc reg(%d)\n",  __func__,
				ret);
		return sprintf(buf, "UNKNOWN\n");
	}

	return sprintf(buf, "%x\n", (ret & ADC_ADC_MASK));
}

static ssize_t rt8973_muic_show_usb_state(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	struct rt8973_muic_data *muic_data = dev_get_drvdata(dev);

	muic_dbg("%s attached_dev(%d)\n",  __func__,
			muic_data->curr_status.cable_type);

	switch (muic_data->curr_status.cable_type) {
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_CDP_MUIC:
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
		return sprintf(buf, "USB_STATE_CONFIGURED\n");
	default:
		break;
	}

	return 0;
}

#if defined(CONFIG_USB_HOST_NOTIFY)
static ssize_t rt8973_muic_show_otg_test(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct rt8973_muic_data *muic_data = dev_get_drvdata(dev);

	muic_dbg("%s %d\n",
		 __func__, !muic_data->vbus_ignore);

	return sprintf(buf, "%x\n", !muic_data->vbus_ignore);
}

static ssize_t rt8973_muic_set_otg_test(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct rt8973_muic_data *muic_data = dev_get_drvdata(dev);
	int val = 0;

	muic_dbg("%s buf:%s\n",  __func__, buf);

	if (sscanf(buf, "%d", &val) != 1) {
		muic_dbg("%s Wrong command\n",  __func__);
		return count;
	}

	muic_data->vbus_ignore = !val;

	return count;
}
#endif

static ssize_t rt8973_muic_show_attached_dev(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct rt8973_muic_data *muic_data = dev_get_drvdata(dev);

	muic_dbg("%s attached_dev:%d\n",  __func__,
			muic_data->curr_status.cable_type);

	switch(muic_data->curr_status.cable_type) {
	case ATTACHED_DEV_NONE_MUIC:
		return sprintf(buf, "No VPS\n");
	case ATTACHED_DEV_USB_MUIC:
		return sprintf(buf, "USB\n");
	case ATTACHED_DEV_CDP_MUIC:
		return sprintf(buf, "CDP\n");
	case ATTACHED_DEV_OTG_MUIC:
		return sprintf(buf, "OTG\n");
	case ATTACHED_DEV_TA_MUIC:
		return sprintf(buf, "TA\n");
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
		return sprintf(buf, "JIG UART OFF\n");
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
		return sprintf(buf, "JIG UART OFF/VB\n");
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
		return sprintf(buf, "JIG UART ON\n");
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
		return sprintf(buf, "JIG USB OFF\n");
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
		return sprintf(buf, "JIG USB ON\n");
	case ATTACHED_DEV_DESKDOCK_MUIC:
		return sprintf(buf, "DESKDOCK\n");
	case ATTACHED_DEV_PS_CABLE_MUIC:
		return sprintf(buf, "PS CABLE\n");
	default:
		break;
	}

	return sprintf(buf, "UNKNOWN\n");
}

static ssize_t rt8973_muic_show_apo_factory(struct device *dev,
					     struct device_attribute *attr,
					     char *buf)
{
	struct rt8973_muic_data *muic_data = dev_get_drvdata(dev);
	const char *mode;

	/* true: Factory mode, false: not Factory mode */
	if (muic_data->is_factory_start)
		mode = "FACTORY_MODE";
	else
		mode = "NOT_FACTORY_MODE";

	muic_dbg("%s apo factory=%s\n",  __func__, mode);

	return sprintf(buf, "%s\n", mode);
}

static ssize_t rt8973_muic_set_apo_factory(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct rt8973_muic_data *muic_data = dev_get_drvdata(dev);
	const char *mode;

	muic_dbg("%s buf:%s\n",  __func__, buf);

	/* "FACTORY_START": factory mode */
	if (!strncmp(buf, "FACTORY_START", 13)) {
		muic_data->is_factory_start = true;
		mode = "FACTORY_MODE";
	} else {
		muic_dbg("%s Wrong command\n",  __func__);
		return count;
	}

	muic_dbg("%s apo factory=%s\n",  __func__, mode);

	return count;
}

static DEVICE_ATTR(uart_sel, 0664, rt8973_muic_show_uart_sel,
		rt8973_muic_set_uart_sel);
static DEVICE_ATTR(usb_sel, 0664,
		rt8973_muic_show_usb_sel, rt8973_muic_set_usb_sel);
static DEVICE_ATTR(adc, 0664, rt8973_muic_show_adc, NULL);
static DEVICE_ATTR(usb_state, 0664, rt8973_muic_show_usb_state, NULL);
#if defined(CONFIG_USB_HOST_NOTIFY)
static DEVICE_ATTR(otg_test, 0664,
		rt8973_muic_show_otg_test, rt8973_muic_set_otg_test);
#endif
static DEVICE_ATTR(attached_dev, 0664, rt8973_muic_show_attached_dev, NULL);
static DEVICE_ATTR(apo_factory, 0664,
		rt8973_muic_show_apo_factory,
		rt8973_muic_set_apo_factory);

static struct attribute *rt8973_muic_attributes[] = {
	&dev_attr_uart_sel.attr,
	&dev_attr_usb_sel.attr,
	&dev_attr_adc.attr,
	&dev_attr_usb_state.attr,
#if defined(CONFIG_USB_HOST_NOTIFY)
	&dev_attr_otg_test.attr,
#endif
	&dev_attr_attached_dev.attr,
	&dev_attr_apo_factory.attr,
	NULL
};

static const struct attribute_group rt8973_muic_group = {
	.attrs = rt8973_muic_attributes,
};

static int set_com_sw(struct rt8973_muic_data *muic_data,
			enum rt8973_reg_manual_sw1_value reg_val)
{
	int ret = 0;
	u8 temp = 0;

	temp = rt8973_reg_read(muic_data, RT8973_REG_MANUAL_SW1);
	if (temp < 0)
		muic_err("%s err read MANSW1(%d)\n",  __func__,
				temp);

	if (reg_val ^ temp) {
		muic_dbg("%s reg_val(0x%x)!=MANSW1 reg(0x%x), update reg\n",
				 __func__, reg_val, temp);

		ret = rt8973_reg_write(muic_data, RT8973_REG_MANUAL_SW1,
				reg_val);
		if (ret < 0)
			muic_err("%s err write MANSW1(%d)\n",
					__func__, reg_val);
	} else {
		muic_dbg("%s MANSW1 reg(0x%x), just return\n",
				 __func__, reg_val);
		return 0;
	}

	temp = rt8973_reg_read(muic_data, RT8973_REG_MANUAL_SW1);
	if (temp < 0)
		muic_err("%s err read MANSW1(%d)\n",  __func__,
				temp);

	if (temp ^ reg_val) {
		muic_err("%s err set MANSW1(0x%x)\n",  __func__,
			temp);
		ret = -EAGAIN;
	}

	return ret;
}

static int com_to_open(struct rt8973_muic_data *muic_data)
{
	enum rt8973_reg_manual_sw1_value reg_val;
	int ret = 0;

	reg_val = MANSW1_OPEN;
	ret = set_com_sw(muic_data, reg_val);
	if (ret)
		muic_err("%s set_com_sw err\n",  __func__);

	return ret;
}

static int com_to_usb(struct rt8973_muic_data *muic_data)
{
	enum rt8973_reg_manual_sw1_value reg_val;
	int ret = 0;

	reg_val = MANSW1_USB;
	ret = set_com_sw(muic_data, reg_val);
	if (ret)
		muic_err("%s set_com_usb err\n",  __func__);

	return ret;
}

static int com_to_uart(struct rt8973_muic_data *muic_data)
{
	enum rt8973_reg_manual_sw1_value reg_val;
	int ret = 0;

	reg_val = MANSW1_UART;
	ret = set_com_sw(muic_data, reg_val);
	if (ret)
		muic_err("%s set_com_uart err\n",  __func__);

	return ret;
}

static int switch_to_ap_usb(struct rt8973_muic_data *muic_data)
{
	struct muic_platform_data *pdata = muic_data->pdata;
	int ret = 0;

	muic_dbg("%s\n",  __func__);

	if (pdata->set_gpio_usb_sel)
		pdata->set_gpio_usb_sel(MUIC_PATH_USB_AP);
	else
		muic_dbg("%s there is no usb_sel pin!\n",  __func__);

	ret = com_to_usb(muic_data);
	if (ret) {
		muic_err("%s com->usb set err\n",  __func__);
		return ret;
	}

	return ret;
}

static int switch_to_cp_usb(struct rt8973_muic_data *muic_data)
{
	struct muic_platform_data *pdata = muic_data->pdata;
	int ret = 0;

	muic_dbg("%s\n",  __func__);

	if (pdata->set_gpio_usb_sel)
		pdata->set_gpio_usb_sel(MUIC_PATH_USB_CP);
	else
		muic_dbg("%s there is no usb_sel pin!\n",  __func__);

	ret = com_to_usb(muic_data);
	if (ret) {
		muic_err("%s com->usb set err\n",  __func__);
		return ret;
	}

	return ret;
}

static int switch_to_ap_uart(struct rt8973_muic_data *muic_data)
{
	struct muic_platform_data *pdata = muic_data->pdata;
	int ret = 0;

	muic_dbg("%s\n",  __func__);

	if (pdata->set_gpio_uart_sel)
		pdata->set_gpio_uart_sel(MUIC_PATH_UART_AP);
	else
		muic_dbg("%s there is no uart_sel pin!\n",  __func__);

	ret = com_to_uart(muic_data);
	if (ret) {
		muic_err("%s com->uart set err\n",  __func__);
		return ret;
	}

	return ret;
}

static int switch_to_cp_uart(struct rt8973_muic_data *muic_data)
{
	struct muic_platform_data *pdata = muic_data->pdata;
	int ret = 0;

	muic_dbg("%s\n",  __func__);

	if (pdata->set_gpio_uart_sel)
		pdata->set_gpio_uart_sel(MUIC_PATH_UART_CP);
	else
		muic_dbg("%s there is no uart_sel pin!\n",  __func__);

	ret = com_to_uart(muic_data);
	if (ret) {
		muic_err("%s com->uart set err\n",  __func__);
		return ret;
	}

	return ret;
}

static int attach_charger(struct rt8973_muic_data *muic_data,
                        enum muic_attached_dev new_dev)
{
	int ret = 0;

	muic_dbg("%s new_dev(%d)\n",  __func__, new_dev);

	if (!muic_data->switch_data->charger_cb) {
		muic_err("%s charger_cb is NULL\n",  __func__);
		return -ENXIO;
	}

	if (ATTACHED_DEV_JIG_UART_OFF_VB_MUIC !=
		muic_data->curr_status.cable_type) {
		ret = com_to_open(muic_data);
		if (ret)
			muic_dbg("%s error from com_to_open\n", __func__);
	}
	/* Change to manual-config */
	rt8973_clr_bits(muic_data,
		RT8973_REG_CONTROL, CTRL_AUTO_CONFIG_MASK);

	ret = muic_data->switch_data->charger_cb(new_dev);
	if (ret) {
		muic_data->curr_status.cable_type = ATTACHED_DEV_NONE_MUIC;
		muic_err("%s charger_cb(%d) err\n",  __func__, ret);
	}

	return ret;
}

static int detach_charger(struct rt8973_muic_data *muic_data)
{
	struct sec_switch_data *switch_data = muic_data->switch_data;
	int ret = 0;

	muic_dbg("%s\n",  __func__);

	if (!switch_data->charger_cb) {
		muic_err("%s charger_cb is NULL\n",  __func__);
		return -ENXIO;
	}

	/* Change to auto-config */
	rt8973_set_bits(muic_data,
		RT8973_REG_CONTROL, CTRL_AUTO_CONFIG_MASK);

	ret = switch_data->charger_cb(muic_data->curr_status.cable_type);
	if (ret) {
		muic_err("%s charger_cb(%d) err\n",  __func__, ret);
		return ret;
	}

	return ret;
}

static int attach_usb_util(struct rt8973_muic_data *muic_data,
			enum muic_attached_dev new_dev)
{
	int ret = 0;

	ret = attach_charger(muic_data, new_dev);
	if (ret) {
		muic_dbg("%s: fail to attach_charger\n", __func__);
		return ret;
	}

	if (muic_data->pdata->usb_path == MUIC_PATH_USB_CP) {
		ret = switch_to_cp_usb(muic_data);
		return ret;
	}

	ret = switch_to_ap_usb(muic_data);
	return ret;
}
/* SAMSUNG MUIC CODE */

static int rt8973_get_cable_type_by_device_reg(struct rt8973_muic_data *muic_data)
{
	uint32_t device_reg = muic_data->curr_status.device_reg[1];
	int i;

	muic_dbg("Device = 0x%x, 0x%x\n",
	       (int)muic_data->curr_status.device_reg[0],
	       (int)muic_data->curr_status.device_reg[1]);

	device_reg <<= 8;
	device_reg |= muic_data->curr_status.device_reg[0];

	for (i = 0; i < ARRAY_SIZE(device_to_cable_type_mapping); i++) {
		if (device_to_cable_type_mapping[i].reg_val == device_reg)
			return device_to_cable_type_mapping[i].cable_type;
	}
	/* not found */
	return -EINVAL;
}

static int rt8973_get_cable_type_by_id(struct rt8973_muic_data *muic_data)
{
	int id_val = muic_data->curr_status.id_adc;
	int cable_type;
	muic_dbg("ID value = 0x%x\n", id_val);
	if (muic_data->curr_status.vbus_status)
		cable_type = id_to_cable_type_mapping[id_val].cable_type_with_vbus;
	else
		cable_type = id_to_cable_type_mapping[id_val].cable_type_without_vbus;

/* need to be changed? */
	/* Special case for L ID200K USB cable */
	if (cable_type == ATTACHED_DEV_200K_MUIC) {
	/* ID = 200KOhm, VBUS = 1, DCD_T = 0 and CHGDET = 0 ==> L SDP*/
		if ((muic_data->curr_status.irq_flags[0]&0x0C) == 0)
			cable_type = ATTACHED_DEV_USB_MUIC;
	}
/* need to be changed? */

	return cable_type;
}

static int rt8973_get_cable_type(struct rt8973_muic_data *muic_data)
{
	int ret;
	ret = rt8973_get_cable_type_by_device_reg(muic_data);
	if (ret >= 0)
		return ret;
	else
		return rt8973_get_cable_type_by_id(muic_data);
}

static void rt8973_preprocess_status(struct rt8973_muic_data *muic_data)
{
	muic_data->curr_status.vbus_status =
		!(muic_data->curr_status.irq_flags[1] & INT_UVLO_MASK);
	muic_data->curr_status.cable_type =
		rt8973_get_cable_type(muic_data);
	muic_data->curr_status.ocp_status =
		!!(muic_data->curr_status.irq_flags[1] & INT_OCP_LATCH_MASK);
	muic_data->curr_status.ovp_status =
		!!(muic_data->curr_status.irq_flags[1] & INT_OVP_FET_MASK);
	muic_data->curr_status.otp_status =
		!!(muic_data->curr_status.irq_flags[1] & INT_OTP_FET_MASK);
	muic_data->curr_status.adc_chg_status =
		(muic_data->prev_status.id_adc != muic_data->curr_status.id_adc);
	muic_data->curr_status.otg_status =
		!!(muic_data->curr_status.cable_type == ATTACHED_DEV_OTG_MUIC);
	muic_data->curr_status.accessory_status =
		((muic_data->curr_status.cable_type != ATTACHED_DEV_NONE_MUIC) &&
		(muic_data->curr_status.cable_type != ATTACHED_DEV_UNKNOWN_MUIC));
	muic_data->curr_status.cable_chg_status =
		(muic_data->curr_status.cable_type != muic_data->prev_status.cable_type);
	muic_data->curr_status.dcdt_status =
		!!(muic_data->curr_status.irq_flags[0] & INT_DCD_T_MASK);
	muic_data->curr_status.usb_connect =
		((muic_data->curr_status.cable_type == ATTACHED_DEV_USB_MUIC) ||
		(muic_data->curr_status.cable_type == ATTACHED_DEV_CDP_MUIC) ||
		(muic_data->curr_status.cable_type == ATTACHED_DEV_JIG_USB_OFF_MUIC) ||
		(muic_data->curr_status.cable_type == ATTACHED_DEV_200K_MUIC) ||
		(muic_data->curr_status.cable_type == ATTACHED_DEV_JIG_USB_ON_MUIC));
	muic_data->curr_status.uart_connect =
		((muic_data->curr_status.cable_type == ATTACHED_DEV_UART_MUIC) ||
		(muic_data->curr_status.cable_type == ATTACHED_DEV_JIG_UART_OFF_MUIC) ||
		(muic_data->curr_status.cable_type == ATTACHED_DEV_JIG_UART_ON_MUIC) ||
		(muic_data->curr_status.cable_type == ATTACHED_DEV_JIG_UART_OFF_VB_MUIC));
	muic_data->curr_status.jig_connect =
		((muic_data->curr_status.cable_type == ATTACHED_DEV_JIG_USB_OFF_MUIC) ||
		(muic_data->curr_status.cable_type == ATTACHED_DEV_JIG_USB_ON_MUIC) ||
		(muic_data->curr_status.cable_type == ATTACHED_DEV_JIG_UART_OFF_MUIC) ||
		(muic_data->curr_status.cable_type == ATTACHED_DEV_JIG_UART_ON_MUIC) ||
		(muic_data->curr_status.cable_type == ATTACHED_DEV_JIG_UART_OFF_VB_MUIC));
	muic_data->curr_status.l200k_usb_connect =
		(muic_data->curr_status.cable_type == ATTACHED_DEV_200K_MUIC);

#if defined(CONFIG_MUIC_DESKDOCK)
	muic_data->curr_status.dock_status =
		(muic_data->curr_status.cable_type == ATTACHED_DEV_DESKDOCK_MUIC);
#endif	/* CONFIG_MUIC_DESKDOCK */

	muic_dbg("INTF1 = 0x%x, INTF2 = 0x%x\n",
		(int)muic_data->curr_status.irq_flags[0],
		(int)muic_data->curr_status.irq_flags[1]);
	muic_dbg("DEV1 = 0x%x, DEV2 = 0x%x ADC = 0x%x\n",
		(int)muic_data->curr_status.device_reg[0],
		(int)muic_data->curr_status.device_reg[1],
		(int)muic_data->curr_status.id_adc);

	muic_dbg("cable = %d, vbus = %d, acc = %d, ocp = %d, ovp = %d, otp = %d\n",
	       muic_data->curr_status.cable_type, muic_data->curr_status.vbus_status,
	       muic_data->curr_status.accessory_status, muic_data->curr_status.ocp_status,
	       muic_data->curr_status.ovp_status, muic_data->curr_status.otp_status);
	muic_dbg("adc_chg = %d, cable_chg = %d, otg = %d, dcdt = %d, usb = %d\n",
	       muic_data->curr_status.adc_chg_status, muic_data->curr_status.cable_chg_status,
	       muic_data->curr_status.otg_status, muic_data->curr_status.dcdt_status,
	       muic_data->curr_status.usb_connect);
	muic_dbg("uart = %d, jig = %d, 200k = %d, dock = %d\n",
	       muic_data->curr_status.uart_connect, muic_data->curr_status.jig_connect,
	       muic_data->curr_status.l200k_usb_connect, muic_data->curr_status.dock_status);
}

#define FLAG_HIGH           (0x01)
#define FLAG_LOW            (0x02)
#define FLAG_LOW_TO_HIGH    (0x04)
#define FLAG_HIGH_TO_LOW    (0x08)
#define FLAG_RISING         FLAG_LOW_TO_HIGH
#define FLAG_FALLING        FLAG_HIGH_TO_LOW
#define FLAG_CHANGED        (FLAG_LOW_TO_HIGH | FLAG_HIGH_TO_LOW)

static inline uint32_t state_check(unsigned int old_state,
				   unsigned int new_state,
				   unsigned int bit_mask)
{
	unsigned int ret = 0;

	old_state &= bit_mask;
	new_state &= bit_mask;

	if (new_state)
		ret |= FLAG_HIGH;
	else
		ret |= FLAG_LOW;

	if (old_state != new_state) {
		if (new_state)
			ret |= FLAG_LOW_TO_HIGH;
		else
			ret |= FLAG_HIGH_TO_LOW;
	}

	return ret;
}

struct rt8973_event_handler {
	char *name;
	uint32_t bit_mask;
	uint32_t type;
	void (*handler) (struct rt8973_muic_data *muic_data,
			 const struct rt8973_event_handler *handler,
			 unsigned int old_status, unsigned int new_status);

};

static void rt8973_ocp_handler(struct rt8973_muic_data *muic_data,
			       const struct rt8973_event_handler *handler,
			       unsigned int old_status, unsigned int new_status)
{
	muic_err("OCP event triggered\n");
}

static void rt8973_ovp_handler(struct rt8973_muic_data *muic_data,
			       const struct rt8973_event_handler *handler,
			       unsigned int old_status, unsigned int new_status)
{
	muic_err("OVP event triggered\n");

	detach_charger(muic_data);
}

static void rt8973_otp_handler(struct rt8973_muic_data *muic_data,
			       const struct rt8973_event_handler *handler,
			       unsigned int old_status, unsigned int new_status)
{
	muic_err("OTP event triggered\n");
}

struct rt8973_event_handler urgent_event_handlers[] = {
	{
	 .name = "OCP",
	 .bit_mask = (1 << OCP_SHIFT),
	 .type = FLAG_RISING,
	 .handler = rt8973_ocp_handler,
	 },
	{
	 .name = "OVP",
	 .bit_mask = (1 << OVP_SHIFT),
	 .type = FLAG_RISING,
	 .handler = rt8973_ovp_handler,
	 },
	{
	 .name = "OTP",
	 .bit_mask = (1 << OTP_SHIFT),
	 .type = FLAG_RISING,
	 .handler = rt8973_otp_handler,
	 },
};

#if RTDBGINFO_LEVEL <= RTDBGLEVEL
static char *rt8973_cable_names[] = {
	"ATTACHED_DEV_NONE_MUIC",
	"ATTACHED_DEV_USB_MUIC",
	"ATTACHED_DEV_CDP_MUIC",
	"ATTACHED_DEV_OTG_MUIC",
	"ATTACHED_DEV_TA_MUIC",
	"ATTACHED_DEV_DESKDOCK_MUIC",
	"ATTACHED_DEV_CARDOCK_MUIC",
	"ATTACHED_DEV_JIG_UART_OFF_MUIC",
	"ATTACHED_DEV_JIG_UART_OFF_VB_MUIC",	/* VBUS enabled */
	"ATTACHED_DEV_JIG_UART_ON_MUIC",
	"ATTACHED_DEV_JIG_USB_OFF_MUIC",
	"ATTACHED_DEV_JIG_USB_ON_MUIC",
	"ATTACHED_DEV_UART_MUIC",
	"ATTACHED_DEV_200K_MUIC",
	"ATTACHED_DEV_PS_CABLE_MUIC",
	"ATTACHED_DEV_UNKNOWN_VB_MUIC",
	"ATTACHED_DEV_UNKNOWN_MUIC"
};
#endif /*RTDBGINFO_LEVEL<=RTDBGLEVEL */

static void rt8973_cable_change_handler(struct rt8973_muic_data *muic_data,
					const struct rt8973_event_handler
					*handler, unsigned int old_status,
					unsigned int new_status)
{
	muic_dbg("Cable change to %s\n",
	       rt8973_cable_names[muic_data->curr_status.cable_type]);

	if (ATTACHED_DEV_JIG_UART_OFF_VB_MUIC
		== muic_data->curr_status.cable_type)
		if (muic_data->vbus_ignore)
			return ;

	if (muic_data->switch_data->charger_cb)
		muic_data->switch_data->charger_cb(muic_data->curr_status.cable_type);
}

static void rt8973_otg_attach_handler(struct rt8973_muic_data *muic_data,
				      const struct rt8973_event_handler
				      *handler, unsigned int old_status,
				      unsigned int new_status)
{
	muic_dbg("%s\n", __func__);

	if (muic_data->pdata->usb_path == MUIC_PATH_USB_CP)
		switch_to_cp_usb(muic_data);
	else
		switch_to_ap_usb(muic_data);

	/* Disable USBCHDEN and AutoConfig*/
	rt8973_clr_bits(muic_data, RT8973_REG_CONTROL,
			CTRL_AUTO_CONFIG_MASK | CTRL_USBCHDEN_MASK);

	if ((muic_data->pdata->usb_path == MUIC_PATH_USB_AP) &&
			(muic_data->switch_data->usb_cb) && (muic_data->is_usb_ready))
		muic_data->switch_data->usb_cb(USB_OTGHOST_ATTACHED);
}

static void rt8973_otg_detach_handler(struct rt8973_muic_data *muic_data,
				      const struct rt8973_event_handler
				      *handler, unsigned int old_status,
				      unsigned int new_status)
{
	int ret = 0;

	muic_dbg("%s\n", __func__);

	/* Enable USBCHDEN and AutoConfig*/
	rt8973_set_bits(muic_data, RT8973_REG_CONTROL,
			CTRL_AUTO_CONFIG_MASK | CTRL_USBCHDEN_MASK);

	ret = com_to_open(muic_data);
	if (ret)
		muic_dbg("%s error from com_to_open\n", __func__);

	if (muic_data->pdata->usb_path == MUIC_PATH_USB_CP) {
		muic_dbg("%s MUIC_PATH_USB_CP\n", __func__);
		return;
	}

	if ((muic_data->pdata->usb_path == MUIC_PATH_USB_AP) &&
			(muic_data->switch_data->usb_cb) && (muic_data->is_usb_ready))
		muic_data->switch_data->usb_cb(USB_OTGHOST_DETACHED);
}

static void attach_usb(struct rt8973_muic_data *muic_data,
				enum muic_attached_dev new_dev)
{
	int ret = 0;

	muic_dbg("%s\n", __func__);

	ret = attach_usb_util(muic_data, new_dev);
	if (ret) {
		muic_dbg("%s error from attach_usb_util\n", __func__);
		return;
	}

	if ((muic_data->pdata->usb_path == MUIC_PATH_USB_AP) &&
		(muic_data->switch_data->usb_cb) && (muic_data->is_usb_ready))
		muic_data->switch_data->usb_cb(USB_CABLE_ATTACHED);
}

static void detach_usb(struct rt8973_muic_data *muic_data)
{
	int ret = 0;

	muic_dbg("%s\n", __func__);

	ret = detach_charger(muic_data);
	if (ret)
		muic_dbg("%s error from detach_charger\n", __func__);

	ret = com_to_open(muic_data);
	if (ret)
		muic_dbg("%s error from com_to_open\n", __func__);

	if (muic_data->pdata->usb_path == MUIC_PATH_USB_CP) {
		muic_dbg("%s MUIC_PATH_USB_CP\n", __func__);
		return;
	}

	if ((muic_data->pdata->usb_path == MUIC_PATH_USB_AP) &&
		(muic_data->switch_data->usb_cb) && (muic_data->is_usb_ready))
		muic_data->switch_data->usb_cb(USB_CABLE_DETACHED);
}

static void rt8973_usb_attach_handler(struct rt8973_muic_data *muic_data,
				      const struct rt8973_event_handler
				      *handler, unsigned int old_status,
				      unsigned int new_status)
{
	muic_dbg("%s\n", __func__);
	attach_usb(muic_data, muic_data->curr_status.cable_type);
}

static void rt8973_usb_detach_handler(struct rt8973_muic_data *muic_data,
				      const struct rt8973_event_handler
				      *handler, unsigned int old_status,
				      unsigned int new_status)
{
	muic_dbg("%s\n", __func__);
	detach_usb(muic_data);
}

/* need to be updated */
static void rt8973_uart_attach_handler(struct rt8973_muic_data *muic_data,
				       const struct rt8973_event_handler
				       *handler, unsigned int old_status,
				       unsigned int new_status)
{
	muic_dbg("%s\n", __func__);
}

static void rt8973_uart_detach_handler(struct rt8973_muic_data *muic_data,
				       const struct rt8973_event_handler
				       *handler, unsigned int old_status,
				       unsigned int new_status)
{
	muic_dbg("%s\n", __func__);
}
/* need to be updated */

static int attach_deskdock(struct rt8973_muic_data *muic_data,
					enum muic_attached_dev new_dev)
{
	int ret = 0;

	muic_dbg("%s vbus (%d)\n", __func__,
				muic_data->curr_status.vbus_status);

	/* In SM5502, VBUS INT enable code is in here */

	/* In SM5502, switching audio path code is in here */

	if (muic_data->curr_status.cable_type != ATTACHED_DEV_DESKDOCK_MUIC
		&& muic_data->switch_data->dock_cb)
			muic_data->switch_data->dock_cb(MUIC_DOCK_DESKDOCK);

	if (muic_data->curr_status.vbus_status)
		ret = attach_charger(muic_data, muic_data->curr_status.cable_type);
	else
		ret = detach_charger(muic_data);

	return ret;
}

static int detach_deskdock(struct rt8973_muic_data *muic_data)
{
	int ret = 0;

	muic_dbg("%s\n", __func__);

	/* In SM5502, VBUS INT enable code is in here */

	/* In SM5502, switching audio path code is in here */

	ret = detach_charger(muic_data);
	if (ret)
		muic_err("%s err detach_charger(%d)\n", __func__, ret);

	if (muic_data->switch_data->dock_cb)
		muic_data->switch_data->dock_cb(MUIC_DOCK_DETACHED);

	return ret;
}

static inline jig_type_t get_jig_type(int cable_type)
{
	jig_type_t type = JIG_USB_BOOT_ON;

	muic_dbg("%s\n", __func__);

	switch (cable_type) {
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
		type = JIG_UART_BOOT_OFF;
		break;
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
		type = JIG_UART_BOOT_ON;
		break;
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
		type = JIG_USB_BOOT_OFF;
		break;
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
		type = JIG_USB_BOOT_ON;
		break;
	}
	return type;
}

static int attach_jig_uart_boot_off(struct rt8973_muic_data *muic_data)
{
	int ret = 0;

	muic_dbg("%s\n", __func__);

	/* In SM5502, VBUS INT enable code is in here */

	if (muic_data->pdata->uart_path == MUIC_PATH_UART_AP)
		ret = switch_to_ap_uart(muic_data);
	else
		ret = switch_to_cp_uart(muic_data);

	/* Disable USBCHDEN and AutoConfig*/
	rt8973_clr_bits(muic_data, RT8973_REG_CONTROL,
			CTRL_AUTO_CONFIG_MASK | CTRL_USBCHDEN_MASK);

	if (muic_data->curr_status.vbus_status) {
		if(muic_data->switch_data->host_notify_cb &&
			muic_data->switch_data->host_notify_cb(1) > 0) {
			muic_dbg("%s OTG_TEST\n", __func__);
			/* in OTG_TEST mode, do not charge */
			/* duplicated code? */
			muic_data->curr_status.cable_type = ATTACHED_DEV_NONE_MUIC;
			ret = attach_charger(muic_data, ATTACHED_DEV_NONE_MUIC);
			if (ret) {
				muic_dbg("%s error from attach_charger\n",
					__func__);
				return ret;
			}
			muic_data->curr_status.cable_type =
				ATTACHED_DEV_JIG_UART_OFF_VB_MUIC;
		} else {
			/* JIG_UART_OFF_VB */
			ret = attach_charger(muic_data,
				ATTACHED_DEV_JIG_UART_OFF_VB_MUIC);
			if (ret) {
				muic_dbg("%s error from attach_charger\n",
					__func__);
				return ret;
			}
		}
	} else {
		muic_dbg("%s NO VBUS\n", __func__);
		if (muic_data->curr_status.cable_type ==
				ATTACHED_DEV_JIG_UART_OFF_VB_MUIC) {
			if (muic_data->switch_data->host_notify_cb) {
				muic_data->switch_data->host_notify_cb(0);
			}
		}
		/* duplicated code? */
		muic_data->curr_status.cable_type = ATTACHED_DEV_JIG_UART_OFF_MUIC;

		ret = muic_data->switch_data->charger_cb(ATTACHED_DEV_JIG_UART_OFF_MUIC);
		if (ret) {
			muic_data->curr_status.cable_type = ATTACHED_DEV_NONE_MUIC;
			muic_err("%s charger_cb(%d) err\n",  __func__, ret);
		}
	}

	return ret;
}

static int detach_jig_uart_boot_off(struct rt8973_muic_data *muic_data)
{
	int ret = 0;

	muic_dbg("%s\n", __func__);

	/* Enable USBCHDEN and AutoConfig*/
	rt8973_set_bits(muic_data, RT8973_REG_CONTROL,
			CTRL_AUTO_CONFIG_MASK | CTRL_USBCHDEN_MASK);

	ret = muic_data->switch_data->charger_cb(muic_data->curr_status.cable_type);
	if (ret) {
		muic_err("%s charger_cb(%d) err\n",  __func__, ret);
		return ret;
	}

	muic_data->curr_status.cable_type = ATTACHED_DEV_NONE_MUIC;

	return ret;
}

static int attach_jig_usb_boot_off(struct rt8973_muic_data *muic_data)
{
	int ret = 0;

	muic_dbg("%s\n", __func__);

	ret = attach_usb_util(muic_data, ATTACHED_DEV_JIG_USB_OFF_MUIC);
	if (ret) {
		muic_dbg("%s error from attach_usb_util\n", __func__);
		return ret;
	}

	if ((muic_data->pdata->usb_path == MUIC_PATH_USB_AP) &&
			(muic_data->switch_data->usb_cb) && (muic_data->is_usb_ready))
		muic_data->switch_data->usb_cb(USB_CABLE_ATTACHED);

	return ret;
}

static int attach_jig_usb_boot_on(struct rt8973_muic_data *muic_data)
{
	int ret = 0;

	muic_dbg("%s\n", __func__);

	ret = attach_usb_util(muic_data, ATTACHED_DEV_JIG_USB_ON_MUIC);
	if (ret) {
		muic_dbg("%s error from attach_usb_util\n", __func__);
		return ret;
	}

	if ((muic_data->pdata->usb_path == MUIC_PATH_USB_AP) &&
			(muic_data->switch_data->usb_cb) && (muic_data->is_usb_ready))
		muic_data->switch_data->usb_cb(USB_CABLE_ATTACHED);

	return ret;
}

static void rt8973_jig_attach_handler(struct rt8973_muic_data *muic_data,
				      const struct rt8973_event_handler
				      *handler, unsigned int old_status,
				      unsigned int new_status)
{
	jig_type_t type;

	type = get_jig_type(muic_data->curr_status.cable_type);

	muic_dbg("JIG attached (type = %d)\n", (int)type);

	switch (type) {
	case JIG_UART_BOOT_OFF:
		attach_jig_uart_boot_off(muic_data);
		break;
	case JIG_UART_BOOT_ON:
	/* This is the mode to wake up device during factory mode. */
		attach_deskdock(muic_data, muic_data->curr_status.cable_type);
		break;
	case JIG_USB_BOOT_OFF:
		attach_jig_usb_boot_off(muic_data);
		break;
	case JIG_USB_BOOT_ON:
		attach_jig_usb_boot_on(muic_data);
		break;
	}
}

static void rt8973_jig_detach_handler(struct rt8973_muic_data *muic_data,
				      const struct rt8973_event_handler
				      *handler, unsigned int old_status,
				      unsigned int new_status)
{
	jig_type_t type;

	type = get_jig_type(muic_data->prev_status.cable_type);

	muic_dbg("JIG detached (type = %d)\n", (int)type);

	switch (type) {
	case JIG_UART_BOOT_OFF:
		detach_jig_uart_boot_off(muic_data);
		break;
	case JIG_UART_BOOT_ON:
	/* This is the mode to wake up device during factory mode. */
		detach_deskdock(muic_data);
		break;
	case JIG_USB_BOOT_OFF:
	case JIG_USB_BOOT_ON:
		detach_usb(muic_data);
		break;
	}
}

static void rt8973_dock_attach_handler(struct rt8973_muic_data *muic_data,
				      const struct rt8973_event_handler
				      *handler, unsigned int old_status,
				      unsigned int new_status)
{
	int ret = 0;

	muic_dbg("%s\n", __func__);
	ret = attach_deskdock(muic_data, muic_data->curr_status.cable_type);
	if (ret)
		muic_dbg("%s error from attach_deskdock\n", __func__);
}

static void rt8973_dock_detach_handler(struct rt8973_muic_data *muic_data,
				      const struct rt8973_event_handler
				      *handler, unsigned int old_status,
				      unsigned int new_status)
{
	int ret = 0;

	muic_dbg("%s\n", __func__);
	ret = detach_deskdock(muic_data);
	if (ret)
		muic_dbg("%s error from detach_deskdock\n", __func__);
}

static void rt8973_l200k_usb_attach_handler(struct rt8973_muic_data *muic_data,
				      const struct rt8973_event_handler
				      *handler, unsigned int old_status,
				      unsigned int new_status)
{
	int ret = 0;

	muic_dbg("200K special USB cable attached\n");
	/* Make switch connect to USB path */
	ret = switch_to_ap_usb(muic_data);
	if (ret)
		muic_err("%s com->usb set err\n",  __func__);

	/* Change to manual-config */
	rt8973_clr_bits(muic_data,
		RT8973_REG_CONTROL, CTRL_AUTO_CONFIG_MASK);
}

static void rt8973_l200k_usb_detach_handler(struct rt8973_muic_data *muic_data,
				      const struct rt8973_event_handler
				      *handler, unsigned int old_status,
				      unsigned int new_status)
{
	int ret = 0;

	muic_dbg("200K special USB cable detached\n");

	/* Change to auto-config */
	rt8973_set_bits(muic_data,
		RT8973_REG_CONTROL, CTRL_AUTO_CONFIG_MASK);

	/* Make switch opened */
	ret = com_to_open(muic_data);
	if (ret)
		muic_dbg("%s error from com_to_open\n", __func__);
}

static void rt8973_vbus_attach_handler(struct rt8973_muic_data *muic_data,
				      const struct rt8973_event_handler
				      *handler, unsigned int old_status,
				      unsigned int new_status)
{
	jig_type_t type;

	muic_dbg("%s\n", __func__);

	type = get_jig_type(muic_data->curr_status.cable_type);
	if (type == JIG_UART_BOOT_OFF)
		attach_jig_uart_boot_off(muic_data);
}

static void rt8973_vbus_detach_handler(struct rt8973_muic_data *muic_data,
				      const struct rt8973_event_handler
				      *handler, unsigned int old_status,
				      unsigned int new_status)
{
	jig_type_t type;

	muic_dbg("%s\n", __func__);
	type = get_jig_type(muic_data->curr_status.cable_type);
	if (type == JIG_UART_BOOT_OFF)
		attach_jig_uart_boot_off(muic_data);
}

struct rt8973_event_handler normal_event_handlers[] = {
	{
        .name = "VBUS on",
        .bit_mask = (1 << VBUS_SHIFT),
        .type = FLAG_RISING,
        .handler = rt8973_vbus_attach_handler,
	},
	{
        .name = "VBUS off",
        .bit_mask = (1 << VBUS_SHIFT),
        .type = FLAG_FALLING,
        .handler = rt8973_vbus_detach_handler,
	},
	{
        .name = "200K special USB attached",
        .bit_mask = (1 << L200K_USB_SHIFT),
        .type = FLAG_RISING,
        .handler = rt8973_l200k_usb_attach_handler,
	},
	{
        .name = "200K special USB detached",
        .bit_mask = (1 << L200K_USB_SHIFT),
        .type = FLAG_FALLING,
        .handler = rt8973_l200k_usb_detach_handler,
	},
	{
	 .name = "OTG attached",
	 .bit_mask = (1 << OTG_SHIFT),
	 .type = FLAG_RISING,
	 .handler = rt8973_otg_attach_handler,
	 },
	{
	 .name = "OTG detached",
	 .bit_mask = (1 << OTG_SHIFT),
	 .type = FLAG_FALLING,
	 .handler = rt8973_otg_detach_handler,
	 },
	{
	 .name = "Cable changed",
	 .bit_mask = (1 << CABLE_CHG_SHIFT),
	 .type = FLAG_HIGH,
	 .handler = rt8973_cable_change_handler,
	 },
	{
	 .name = "USB attached",
	 .bit_mask = (1 << USB_SHIFT),
	 .type = FLAG_RISING,
	 .handler = rt8973_usb_attach_handler,
	 },
	{
	 .name = "USB detached",
	 .bit_mask = (1 << USB_SHIFT),
	 .type = FLAG_FALLING,
	 .handler = rt8973_usb_detach_handler,
	 },
	{
	 .name = "UART attached",
	 .bit_mask = (1 << UART_SHIFT),
	 .type = FLAG_RISING,
	 .handler = rt8973_uart_attach_handler,
	 },
	{
	 .name = "UART detached",
	 .bit_mask = (1 << UART_SHIFT),
	 .type = FLAG_FALLING,
	 .handler = rt8973_uart_detach_handler,
	 },
	{
	 .name = "JIG attached",
	/* Check VBUS */
	 .bit_mask = (1 << JIG_SHIFT),
	 .type = FLAG_RISING,
	 .handler = rt8973_jig_attach_handler,
	 },
	{
	 .name = "JIG detached",
	 .bit_mask = (1 << JIG_SHIFT),
	 .type = FLAG_FALLING,
	 .handler = rt8973_jig_detach_handler,
	 },
	{
	 .name = "DOCK attached",
	 .bit_mask = (1 << DOCK_SHIFT),
	 .type = FLAG_RISING,
	 .handler = rt8973_dock_attach_handler,
	 },
	{
	 .name = "DOCK detached",
	 .bit_mask = (1 << DOCK_SHIFT),
	 .type = FLAG_FALLING,
	 .handler = rt8973_dock_detach_handler,
	 },

};

static void rt8973_process_urgent_evt(struct rt8973_muic_data *muic_data)
{
	unsigned int i;
	unsigned int type, sta_chk;
	uint32_t prev_status, curr_status;
	prev_status = muic_data->prev_status.status;
	curr_status = muic_data->curr_status.status;
	for (i = 0; i < ARRAY_SIZE(urgent_event_handlers); i++) {
		sta_chk = state_check(prev_status,
				      curr_status,
				      urgent_event_handlers[i].bit_mask);
		type = urgent_event_handlers[i].type;
		if (type & sta_chk) {

			if (urgent_event_handlers[i].handler)
				urgent_event_handlers[i].handler(muic_data,
						urgent_event_handlers + i,
						prev_status,
						curr_status);
		}
	}
}

static void rt8973_process_normal_evt(struct rt8973_muic_data *muic_data)
{
	unsigned int i;
	unsigned int type, sta_chk;
	uint32_t prev_status, curr_status;
	prev_status = muic_data->prev_status.status;
	curr_status = muic_data->curr_status.status;
	for (i = 0; i < ARRAY_SIZE(normal_event_handlers); i++) {
		sta_chk = state_check(prev_status,
				      curr_status,
				      normal_event_handlers[i].bit_mask);
		type = normal_event_handlers[i].type;
		if (type & sta_chk) {

			if (normal_event_handlers[i].handler)
				normal_event_handlers[i].handler(muic_data,
						normal_event_handlers + i,
						prev_status,
						curr_status);
		}
	}
}

static void rt8973_dwork(struct work_struct *work)
{
	struct rt8973_muic_data *muic_data =
		container_of(work, struct rt8973_muic_data, dwork.work);
	int ret;

	/* enable interrupt */
	/* Below 1 line, can replace "set_int_mask" */
	rt8973_clr_bits(muic_data, RT8973_REG_CONTROL, 0x01);
	muic_data->prev_status = muic_data->curr_status;
	ret = rt8973_update_regs(muic_data);
	if (ret < 0) {
		muic_err("Error : can't update(read) register status:%d\n", ret);
		/* roll back status */
		muic_data->curr_status = muic_data->prev_status;
		return;
	}

	rt8973_preprocess_status(muic_data);
	rt8973_process_urgent_evt(muic_data);
	if (muic_data->curr_status.dcdt_status && !muic_data->curr_status.jig_connect) {
		if (muic_data->dcdt_retry_count >= DCD_T_RETRY) {
			/* reset counter */
			muic_data->dcdt_retry_count = 0;
			muic_dbg("Exceeded maxium retry times\n");
			/* continue to process event */
			if (!(INT_POR_MASK & muic_data->curr_status.irq_flags[1]))
				muic_data->curr_status.cable_type = ATTACHED_DEV_TA_MUIC;
		} else {
			muic_data->dcdt_retry_count++;
			/* DCD_T -> roll back previous status */
			muic_data->curr_status = muic_data->prev_status;
			muic_dbg("DCD_T event triggered, do re-detect\n");
			rt8973_clr_bits(muic_data, RT8973_REG_CONTROL, 0x60);
			msleep_interruptible(1);
			rt8973_set_bits(muic_data, RT8973_REG_CONTROL, 0x60);
			return;
		}
	}
	rt8973_process_normal_evt(muic_data);
}

void rt8973_reg_dump(void)
{
	int i, tmp;
	const enum rt8973_reg_list list[] = {
		RT8973_REG_CHIP_ID,
		RT8973_REG_CONTROL,
		RT8973_REG_INT_FLAG1,
		RT8973_REG_INT_FLAG2,
		RT8973_REG_INTERRUPT_MASK1,
		RT8973_REG_INTERRUPT_MASK2,
		RT8973_REG_ADC,
		RT8973_REG_DEVICE1,
		RT8973_REG_DEVICE2,
		RT8973_REG_MANUAL_SW1,
		RT8973_REG_MANUAL_SW2,
		RT8973_REG_RESET,
	};

	muic_dbg("####### MUIC REG DUMP START ########\n");
	for (i = 0 ; i < ARRAY_SIZE(list) ; i++) {
		tmp = rt8973_reg_read(g_data, list[i]);
		muic_dbg("[%x] %x\n", list[i], tmp);
	}
}
EXPORT_SYMBOL(rt8973_reg_dump);

static irqreturn_t rt8973_irq_handler(int irq, void *data)
{
	struct rt8973_muic_data *muic_data = data;

	wake_lock_timeout(&muic_data->muic_wake_lock, HZ / 2);
	schedule_delayed_work(&muic_data->dwork, HZ / 10);
	return IRQ_HANDLED;
}

static const struct i2c_device_id rt8973_id_table[] = {
	{"rt8973", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, rt8973_id_table);

static bool rt8973_reset_check(struct i2c_client *i2c)
{
	int ret;
	ret = i2c_smbus_read_byte_data(i2c, RT8973_REG_CHIP_ID);
	if (ret < 0) {
		muic_err("can't read device ID from IC(%d)\n", ret);
		return false;
	}

	/* write default value instead of sending reset command*/
	/* REG[0x02] = 0xE5, REG[0x14] = 0x01*/
	i2c_smbus_write_byte_data(i2c, RT8973_REG_CONTROL, 0xE5);
	i2c_smbus_write_byte_data(i2c, RT8973_REG_MANUAL_SW2, 0x01);
	return true;
}

static void rt8973_init_regs(struct rt8973_muic_data *muic_data)
{
	int muic_data_id = rt8973_reg_read(muic_data, RT8973_REG_CHIP_ID);
	muic_dbg("rt8973_init_regs\n");
	muic_data->curr_status.cable_type = ATTACHED_DEV_UNKNOWN_MUIC;
	muic_data->curr_status.id_adc = 0x1f;
	/* for rev 0, turn off i2c reset function */
	if (((muic_data_id & 0xf8) >> 3) == 0)
		rt8973_set_bits(muic_data, RT8973_REG_CONTROL, 0x08);
	/* Only mask Connect */
	rt8973_reg_write(muic_data, RT8973_REG_INTERRUPT_MASK1, 0x20);
	/* Dummy read */
	rt8973_reg_read(muic_data, RT8973_REG_INT_FLAG1);
	/* Only mask OCP_LATCH and POR */
	rt8973_reg_write(muic_data, RT8973_REG_INTERRUPT_MASK2, 0x24);
	/* enable interrupt */
	//rt8973_clr_bits(muic_data, RT8973_REG_CONTROL, 0x01);

	/* usb is ready */
	muic_data->is_usb_ready = true;

	g_data = muic_data;
}

#ifdef CONFIG_OF
static int rt8973_parse_dt(struct device *dev, struct rt8973_platform_data *pdata)
{

        struct device_node *np = dev->of_node;

        pdata->irq_gpio = of_get_named_gpio_flags(np, "rt8973,irq-gpio",
                0, &pdata->irq_gpio_flags);
	muic_dbg("%s: irq-gpio: %u \n", __func__, pdata->irq_gpio);

        return 0;
}
#endif

static int __devinit rt8973_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct muic_platform_data *pdata = client->dev.platform_data;
	struct rt8973_muic_data *muic_data;
	int ret = 0, tmp_irq;

	muic_pdata = *pdata;

	muic_dbg("Richtek RT8973 driver probing...\n");
	dev_info(&client->dev,"%s:rt8973 probe called \n",__func__);

#ifdef CONFIG_OF
	if(client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
			sizeof(struct rt8973_platform_data), GFP_KERNEL);
		if (!pdata) {
			dev_err(&client->dev, "Failed to allocate memory \n");
				return -ENOMEM;
		}
		pdata = &rt8973_pdata;
		ret = rt8973_parse_dt(&client->dev, pdata);
		if (ret < 0)
			return ret;

		client->irq = gpio_to_irq(pdata->irq_gpio);
	} else
#endif /* CONFIG_OF */
		pdata = client->dev.platform_data;

	if (!pdata)
		return -EINVAL;

	ret = i2c_check_functionality(adapter,
				      I2C_FUNC_SMBUS_BYTE_DATA |
				      I2C_FUNC_SMBUS_I2C_BLOCK);
	if (ret < 0) {
		muic_err("%s: i2c functionality check error\n", __func__);
		ret = -EIO;
		goto err_i2c_func;
	}
	if (!rt8973_reset_check(client)) {
		ret = -ENODEV;
		goto err_reset_rt8973_fail;
	}

	muic_data = kzalloc(sizeof(struct rt8973_muic_data), GFP_KERNEL);
	if (muic_data == NULL) {
		muic_err("%s: failed to allocate driver data\n", __func__);
		ret = -ENOMEM;
		goto err_nomem;
	}
	muic_data->pdata = pdata;
	muic_data->i2c = client;
	i2c_set_clientdata(client, muic_data);
	muic_data->switch_data = &sec_switch_data;

        if (pdata->init_gpio_cb)
                ret = pdata->init_gpio_cb(get_switch_sel());

        if (ret) {
                muic_err("%s: failed to init gpio(%d)\n", __func__, ret);
                goto fail_init_gpio;
        }

	mutex_init(&muic_data->muic_mutex);
	muic_data->is_factory_start = false;

	wake_lock_init(&muic_data->muic_wake_lock, WAKE_LOCK_SUSPEND,
		       "rt8973_wakelock");

	muic_data->is_usb_ready = false;

	dev_set_drvdata(switch_device, muic_data);

	ret = gpio_request(pdata->irq_gpio, "RT8973_EINT");
	if (ret < 0)
		muic_err("Warning : failed to request GPIO%d (retval = %d)\n",
				  pdata->irq_gpio, ret);

	ret = gpio_direction_input(pdata->irq_gpio);
	if (ret < 0)
		muic_err("Warning : failed to set GPIO%d as input pin(retval = %d)\n",
				  pdata->irq_gpio, ret);

	INIT_DELAYED_WORK(&muic_data->dwork, rt8973_dwork);

	rt8973_init_regs(muic_data);

	tmp_irq = gpio_to_irq(pdata->irq_gpio);
	if (tmp_irq < 0) {
		muic_err("failed irq num (gpio=%d, retval=%d)\n",
			pdata->irq_gpio, ret);
		goto err_irq_fail;
	}

	muic_dbg("Request IRQ %d(GPIO %d)...\n",
		tmp_irq, pdata->irq_gpio);

	ret = request_threaded_irq(tmp_irq, NULL,
		rt8973_irq_handler, RT8973_IRQF_MODE,
		RT8973_DEVICE_NAME, muic_data);
	if (ret < 0) {
		muic_err("failed to request irq %d (gpio=%d, retval=%d)\n",
			tmp_irq, pdata->irq_gpio, ret);
		goto err_request_irq_fail;
	}

	ret = enable_irq_wake(tmp_irq);
	if (ret < 0)
		muic_err("%s failed to enable wakeup src\n",  __func__);

	ret = sysfs_create_group(&switch_device->kobj, &rt8973_muic_group);
	if (ret) {
		muic_err("%s: failed to create rt8973 muic attribute group\n", __func__);
		goto fail_sysfs;
	}

	if (muic_data->switch_data->init_cb)
		muic_data->switch_data->init_cb();

	schedule_delayed_work(&muic_data->dwork, HZ / 2);

	muic_dbg("Richtek RT8973 MUIC driver initialize successfully\n");

	return 0;
fail_sysfs:
err_request_irq_fail:
err_irq_fail:
	mutex_destroy(&muic_data->muic_mutex);
	gpio_free(pdata->irq_gpio);
	wake_lock_destroy(&muic_data->muic_wake_lock);
fail_init_gpio:
	kfree(muic_data);
err_nomem:
err_reset_rt8973_fail:
err_i2c_func:
	return ret;
}

static int __devexit rt8973_remove(struct i2c_client *client)
{
	struct rt8973_muic_data *muic_data = i2c_get_clientdata(client);
	muic_dbg("Richtek RT8973 driver removing...\n");
	if (muic_data) {
		gpio_free(muic_data->pdata->irq_gpio);
		mutex_destroy(&muic_data->muic_mutex);
		cancel_delayed_work(&muic_data->dwork);
		wake_lock_destroy(&muic_data->muic_wake_lock);
		kfree(muic_data);
	}
	return 0;
}

static void rt8973_shutdown(struct i2c_client *client)
{
	struct muic_platform_data *pdata = client->dev.platform_data;
	struct rt8973_muic_data *muic_data = i2c_get_clientdata(client);

	free_irq(gpio_to_irq(pdata->irq_gpio), muic_data);
	i2c_smbus_write_byte_data(client, RT8973_REG_RESET, 0x01);
	muic_dbg("Shutdown : reset rt8973...\n");
}

static struct of_device_id rt8973_i2c_match_table[] = {
	{ .compatible = "rt8973,i2c",},
	{},
};
MODULE_DEVICE_TABLE(of, rt8973_i2c_match_table);

static struct i2c_driver rt8973_driver = {
	.driver = {
		.name = RT8973_DEVICE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = rt8973_i2c_match_table,
	},
	.probe = rt8973_probe,
	.remove = __devexit_p(rt8973_remove),
	.shutdown = rt8973_shutdown,
	.id_table = rt8973_id_table,
};

static int __init rt8973_i2c_init(void)
{
	muic_dbg("%s\n", __func__);
	return i2c_add_driver(&rt8973_driver);
}

/* MUIC driver's initializing priority must be lower than Samsung's battery driver
 * Otherwise, battery driver might not be ready to receive first USB/charger attached event
 * during booting.
 * We suggest to use late_initcall()
 */
late_initcall(rt8973_i2c_init);

static void __exit rt8973_i2c_exit(void)
{
	i2c_del_driver(&rt8973_driver);
}

module_exit(rt8973_i2c_exit);

MODULE_DESCRIPTION("Richtek RT8973 MUIC Driver");
MODULE_AUTHOR("Patrick Chang <patrick_chang@richtek.com>");
MODULE_VERSION(RT8973_DRV_VER);
MODULE_LICENSE("GPL");
