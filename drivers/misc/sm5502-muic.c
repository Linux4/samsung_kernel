/*
 * driver/misc/sm5502-muic.c - SM5502 micro USB switch device driver
 *
 * Copyright (C) 2010 Samsung Electronics
 * Hyoyoung Kim <hyway.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <plat/udc-hs.h>
#include <linux/host_notify.h>

#include <linux/muic.h>
#include <linux/sm5502.h>

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
	pr_info("%s:%s switch_sel: 0x%x\n", MUIC_DEV_NAME, __func__,
			switch_sel);

	return switch_sel;
}
__setup("pmic_info=", set_switch_sel);

static int get_switch_sel(void)
{
	return switch_sel;
}
static int sm5502_i2c_read_byte(const struct i2c_client *client, u8 command)
{
	int ret;
	int retry = 0;

	ret = i2c_smbus_read_byte_data(client, command);

	while(ret < 0){
		pr_err("%s:i2c err on reading reg(0x%x), retrying ...\n",
					__func__, command);
		if(retry > 10)
		{
			pr_err("%s: retry count > 10 : failed !!\n", __func__);
		}
		msleep(100);
		ret = i2c_smbus_read_byte_data(client, command);
		retry ++;
	}

	return ret;
}
static int sm5502_i2c_write_byte(const struct i2c_client *client,
			u8 command, u8 value)
{
	int ret;
	int retry = 0;

	ret = i2c_smbus_write_byte_data(client, command, value);

	while(ret < 0){
		pr_err("%s:i2c err on writing reg(0x%x) to 0x%x, retrying...\n",
					__func__, command, value);
		if(retry > 10)
		{
			pr_err("%s: retry count > 10 : failed !!\n", __func__);
		}
		msleep(100);
		ret = i2c_smbus_write_byte_data(client, command, value);
		retry ++;
	}

	return ret;
}

static ssize_t sm5502_muic_show_uart_sel(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct sm5502_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *pdata = muic_data->pdata;

	switch (pdata->uart_path) {
	case MUIC_PATH_UART_AP:
		pr_info("%s:%s AP\n", MUIC_DEV_NAME, __func__);
		return sprintf(buf, "AP\n");
	case MUIC_PATH_UART_CP:
		pr_info("%s:%s CP\n", MUIC_DEV_NAME, __func__);
		return sprintf(buf, "CP\n");
	default:
		break;
	}

	pr_info("%s:%s UNKNOWN\n", MUIC_DEV_NAME, __func__);
	return sprintf(buf, "UNKNOWN\n");
}

static int switch_to_ap_uart(struct sm5502_muic_data *muic_data);
static int switch_to_cp_uart(struct sm5502_muic_data *muic_data);

/*
static void sm5502_disable_interrupt(struct sm5502_muic_data *muic_data)
{
	struct i2c_client *i2c = muic_data->i2c;
	int value, ret;

	value = sm5502_i2c_read_byte(i2c, SM5502_MUIC_REG_CTRL);
	value |= CTRL_INT_MASK_MASK;

	ret = sm5502_i2c_write_byte(i2c, SM5502_MUIC_REG_CTRL, value);
	if (ret < 0)
		dev_err(&i2c->dev, "%s: err %d\n", __func__, ret);

}

static void sm5502_enable_interrupt(struct sm5502_muic_data *muic_data)
{
	struct i2c_client *i2c = muic_data->i2c;
	int value, ret;

	value = sm5502_i2c_read_byte(i2c, SM5502_MUIC_REG_CTRL);
	value &= (~CTRL_INT_MASK_MASK);

	ret = sm5502_i2c_write_byte(i2c, SM5502_MUIC_REG_CTRL, value);
	if (ret < 0)
		dev_err(&i2c->dev, "%s: err %d\n", __func__, ret);

}
*/

static ssize_t sm5502_muic_set_uart_sel(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct sm5502_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *pdata = muic_data->pdata;

	if (!strncasecmp(buf, "AP", 2)) {
		pdata->uart_path = MUIC_PATH_UART_AP;
		switch_to_ap_uart(muic_data);
	} else if (!strncasecmp(buf, "CP", 2)) {
		pdata->uart_path = MUIC_PATH_UART_CP;
		switch_to_cp_uart(muic_data);
	} else {
		pr_warn("%s:%s invalid value\n", MUIC_DEV_NAME, __func__);
	}

	pr_info("%s:%s uart_path(%d)\n", MUIC_DEV_NAME, __func__,
			pdata->uart_path);

	return count;
}

static ssize_t sm5502_muic_show_usb_sel(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct sm5502_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *pdata = muic_data->pdata;

	switch (pdata->usb_path) {
	case MUIC_PATH_USB_AP:
		return sprintf(buf, "PDA\n");
	case MUIC_PATH_USB_CP:
		return sprintf(buf, "MODEM\n");
	default:
		break;
	}

	pr_info("%s:%s UNKNOWN\n", MUIC_DEV_NAME, __func__);
	return sprintf(buf, "UNKNOWN\n");
}

static ssize_t sm5502_muic_set_usb_sel(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct sm5502_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *pdata = muic_data->pdata;

	if (!strncasecmp(buf, "PDA", 3)) {
		pdata->usb_path = MUIC_PATH_USB_AP;
	} else if (!strncasecmp(buf, "MODEM", 5)) {
		pdata->usb_path = MUIC_PATH_USB_CP;
	} else {
		pr_warn("%s:%s invalid value\n", MUIC_DEV_NAME, __func__);
	}

	pr_info("%s:%s usb_path(%d)\n", MUIC_DEV_NAME, __func__,
			pdata->usb_path);

	return count;
}

static ssize_t sm5502_muic_show_adc(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct sm5502_muic_data *muic_data = dev_get_drvdata(dev);
	int ret;

	ret = sm5502_i2c_read_byte(muic_data->i2c, SM5502_MUIC_REG_ADC);
	if (ret < 0) {
		pr_err("%s:%s err read adc reg(%d)\n", MUIC_DEV_NAME, __func__,
				ret);
		return sprintf(buf, "UNKNOWN\n");
	}

	return sprintf(buf, "%x\n", (ret & ADC_ADC_MASK));
}

static ssize_t sm5502_muic_show_usb_state(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	struct sm5502_muic_data *muic_data = dev_get_drvdata(dev);

	pr_info("%s:%s muic_data->attached_dev(%d)\n", MUIC_DEV_NAME, __func__,
			muic_data->attached_dev);

	switch (muic_data->attached_dev) {
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
static ssize_t sm5502_muic_show_otg_test(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct sm5502_muic_data *muic_data = dev_get_drvdata(dev);
	int ret;
	u8 val = 0;

	ret = sm5502_i2c_read_byte(muic_data->i2c, SM5502_MUIC_REG_INTMASK2);

	pr_info("func:%s ret:%d val:%x buf%s\n", __func__, ret, val, buf);

	if (ret) {
		pr_err("%s: fail to read muic reg\n", __func__);
		return sprintf(buf, "UNKNOWN\n");
	}

	val &= INT_CHG_DET_MASK;
	return sprintf(buf, "%x\n", val);
}

static ssize_t sm5502_muic_set_otg_test(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sm5502_muic_data *muic_data = dev_get_drvdata(dev);
	u8 val;

	pr_info("%s:%s buf:%s\n", MUIC_DEV_NAME, __func__, buf);
	if (!strncmp(buf, "0", 1)) {
		val = 0;
	} else if (!strncmp(buf, "1", 1)) {
		val = 1;
	} else {
		pr_warn("%s:%s Wrong command\n", MUIC_DEV_NAME, __func__);
		return count;
	}
	val = sm5502_i2c_read_byte(muic_data->i2c, SM5502_MUIC_REG_INTMASK2);
	val = sm5502_i2c_write_byte(muic_data->i2c,
			SM5502_MUIC_REG_INTMASK2, val|INT_CHG_DET_MASK);
	if (val < 0) {
		pr_err("%s:%s err writing INTMASK reg(%d)\n",
					MUIC_DEV_NAME, __func__, val);
	}

	val = 0;
	val = sm5502_i2c_read_byte(muic_data->i2c, SM5502_MUIC_REG_INTMASK2);
	val &= INT_CHG_DET_MASK;
	pr_info("%s: CHG_DET(0x%02x)\n", __func__, val);

	return count;
}
#endif

static ssize_t sm5502_muic_show_attached_dev(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct sm5502_muic_data *muic_data = dev_get_drvdata(dev);

	pr_info("%s:%s attached_dev:%d\n", MUIC_DEV_NAME, __func__,
			muic_data->attached_dev);

	switch(muic_data->attached_dev) {
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
	case ATTACHED_DEV_AUDIODOCK_MUIC:
		return sprintf(buf, "AUDIODOCK\n");
	default:
		break;
	}

	return sprintf(buf, "UNKNOWN\n");
}

static ssize_t sm5502_muic_show_audio_path(struct device *dev,
					     struct device_attribute *attr,
					     char *buf)
{
	return 0;
}

static ssize_t sm5502_muic_set_audio_path(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t count)
{
	return 0;
}


static ssize_t sm5502_muic_show_apo_factory(struct device *dev,
					     struct device_attribute *attr,
					     char *buf)
{
	struct sm5502_muic_data *muic_data = dev_get_drvdata(dev);
	const char *mode;

	/* true: Factory mode, false: not Factory mode */
	if (muic_data->is_factory_start)
		mode = "FACTORY_MODE";
	else
		mode = "NOT_FACTORY_MODE";

	pr_info("%s:%s apo factory=%s\n", MUIC_DEV_NAME, __func__, mode);

	return sprintf(buf, "%s\n", mode);
}

static ssize_t sm5502_muic_set_apo_factory(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct sm5502_muic_data *muic_data = dev_get_drvdata(dev);
	const char *mode;

	pr_info("%s:%s buf:%s\n", MUIC_DEV_NAME, __func__, buf);

	/* "FACTORY_START": factory mode */
	if (!strncmp(buf, "FACTORY_START", 13)) {
		muic_data->is_factory_start = true;
		mode = "FACTORY_MODE";
	} else {
		pr_warn("%s:%s Wrong command\n", MUIC_DEV_NAME, __func__);
		return count;
	}

	pr_info("%s:%s apo factory=%s\n", MUIC_DEV_NAME, __func__, mode);

	return count;
}

static DEVICE_ATTR(uart_sel, 0664, sm5502_muic_show_uart_sel,
		sm5502_muic_set_uart_sel);
static DEVICE_ATTR(usb_sel, 0664,
		sm5502_muic_show_usb_sel, sm5502_muic_set_usb_sel);
static DEVICE_ATTR(adc, 0664, sm5502_muic_show_adc, NULL);
static DEVICE_ATTR(usb_state, 0664, sm5502_muic_show_usb_state, NULL);
#if defined(CONFIG_USB_HOST_NOTIFY)
static DEVICE_ATTR(otg_test, 0664,
		sm5502_muic_show_otg_test, sm5502_muic_set_otg_test);
#endif
static DEVICE_ATTR(attached_dev, 0664, sm5502_muic_show_attached_dev, NULL);
static DEVICE_ATTR(audio_path, 0664,
		sm5502_muic_show_audio_path, sm5502_muic_set_audio_path);
static DEVICE_ATTR(apo_factory, 0664,
		sm5502_muic_show_apo_factory,
		sm5502_muic_set_apo_factory);

static struct attribute *sm5502_muic_attributes[] = {
	&dev_attr_uart_sel.attr,
	&dev_attr_usb_sel.attr,
	&dev_attr_adc.attr,
	&dev_attr_usb_state.attr,
#if defined(CONFIG_USB_HOST_NOTIFY)
	&dev_attr_otg_test.attr,
#endif
	&dev_attr_attached_dev.attr,
	&dev_attr_audio_path.attr,
	&dev_attr_apo_factory.attr,
	NULL
};

static const struct attribute_group sm5502_muic_group = {
	.attrs = sm5502_muic_attributes,
};

static int set_ctrl_reg(struct sm5502_muic_data *muic_data, int shift, bool on)
{
	struct i2c_client *i2c = muic_data->i2c;
	u8 reg_val;
	int ret = 0;

	ret = sm5502_i2c_read_byte(i2c, SM5502_MUIC_REG_CTRL);
	if (ret < 0)
		pr_err("%s:%s err read CTRL(%d)\n", MUIC_DEV_NAME, __func__,
				ret);

	ret &= CTRL_MASK;

	if (on)
		reg_val = ret | (0x1 << shift);
	else
		reg_val = ret & ~(0x1 << shift);

	if (reg_val ^ ret) {
		pr_info("%s:%s reg_val(0x%x)!=CTRL reg(0x%x), update reg\n",
				MUIC_DEV_NAME, __func__, reg_val, ret);

		ret = sm5502_i2c_write_byte(i2c, SM5502_MUIC_REG_CTRL,
				reg_val);
		if (ret < 0)
			pr_err("%s:%s err write CTRL(%d)\n", MUIC_DEV_NAME,
					__func__, ret);
	} else {
		pr_info("%s:%s reg_val(0x%x)==CTRL reg(0x%x), just return\n",
				MUIC_DEV_NAME, __func__, reg_val, ret);
		return 0;
	}

	ret = sm5502_i2c_read_byte(i2c, SM5502_MUIC_REG_CTRL);
	if (ret < 0)
		pr_err("%s:%s err read CTRL(%d)\n", MUIC_DEV_NAME, __func__,
				ret);
	else
		pr_info("%s:%s CTRL reg after change(0x%x)\n", MUIC_DEV_NAME,
				__func__, ret);

	return ret;
}

static int set_int_mask(struct sm5502_muic_data *muic_data, bool on)
{
	int shift = CTRL_INT_MASK_SHIFT;
	int ret = 0;

	ret = set_ctrl_reg(muic_data, shift, on);

	return ret;
}

/*
static int set_manual_sw(struct sm5502_muic_data *muic_data, bool on)
{
	int shift = CTRL_MANUAL_SW_SHIFT;
	int ret = 0;

	ret = set_ctrl_reg(muic_data, shift, on);

	return ret;
}
*/

static int set_com_sw(struct sm5502_muic_data *muic_data,
			enum sm5502_reg_manual_sw1_value reg_val)
{
	struct i2c_client *i2c = muic_data->i2c;
	int ret = 0;
	u8 temp = 0;

	temp = sm5502_i2c_read_byte(i2c, SM5502_MUIC_REG_MANSW1);
	if (temp < 0)
		pr_err("%s:%s err read MANSW1(%d)\n", MUIC_DEV_NAME, __func__,
				temp);

	if (reg_val ^ temp) {
		pr_info("%s:%s reg_val(0x%x)!=MANSW1 reg(0x%x), update reg\n",
				MUIC_DEV_NAME, __func__, reg_val, temp);

		ret = sm5502_i2c_write_byte(i2c, SM5502_MUIC_REG_MANSW1,
				reg_val);
		if (ret < 0)
			pr_err("%s:%s err write MANSW1(%d)\n", MUIC_DEV_NAME,
					__func__, reg_val);
	} else {
		pr_info("%s:%s MANSW1 reg(0x%x), just return\n",
				MUIC_DEV_NAME, __func__, reg_val);
		return 0;
	}

	temp = sm5502_i2c_read_byte(i2c, SM5502_MUIC_REG_MANSW1);
	if (temp < 0)
		pr_err("%s:%s err read MANSW1(%d)\n", MUIC_DEV_NAME, __func__,
				temp);

	if (temp ^ reg_val) {
		pr_err("%s:%s err set MANSW1(0x%x)\n", MUIC_DEV_NAME, __func__,
			temp);
		ret = -EAGAIN;
	}

	return ret;
}

#define com_to_open com_to_open_with_vbus

#ifndef com_to_open
static int com_to_open(struct sm5502_muic_data *muic_data)
{
	enum sm5502_reg_manual_sw1_value reg_val;
	int ret = 0;

	reg_val = MANSW1_OPEN;
	ret = set_com_sw(muic_data, reg_val);
	if (ret)
		pr_err("%s:%s set_com_sw err\n", MUIC_DEV_NAME, __func__);

	return ret;
}
#endif

static int com_to_open_with_vbus(struct sm5502_muic_data *muic_data)
{
	enum sm5502_reg_manual_sw1_value reg_val;
	int ret = 0;

	reg_val = MANSW1_OPEN_WITH_V_BUS;
	ret = set_com_sw(muic_data, reg_val);
	if (ret)
		pr_err("%s:%s set_com_sw err\n", MUIC_DEV_NAME, __func__);

	return ret;
}

static int com_to_usb(struct sm5502_muic_data *muic_data)
{
	enum sm5502_reg_manual_sw1_value reg_val;
	int ret = 0;

	reg_val = MANSW1_USB;
	ret = set_com_sw(muic_data, reg_val);
	if (ret)
		pr_err("%s:%s set_com_usb err\n", MUIC_DEV_NAME, __func__);

	return ret;
}

static int com_to_uart(struct sm5502_muic_data *muic_data)
{
	enum sm5502_reg_manual_sw1_value reg_val;
	int ret = 0;

	reg_val = MANSW1_UART;
	ret = set_com_sw(muic_data, reg_val);
	if (ret)
		pr_err("%s:%s set_com_uart err\n", MUIC_DEV_NAME, __func__);

	return ret;
}

static int com_to_audio(struct sm5502_muic_data *muic_data)
{
	enum sm5502_reg_manual_sw1_value reg_val;
	int ret = 0;

	reg_val = MANSW1_AUDIO;
	ret = set_com_sw(muic_data, reg_val);
	if (ret)
		pr_err("%s:%s set_com_audio err\n", MUIC_DEV_NAME, __func__);

	return ret;
}

static int switch_to_dock_audio(struct sm5502_muic_data *muic_data)
{
	struct muic_platform_data *pdata = muic_data->pdata;
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	if (pdata->set_gpio_doc_switch)
		pdata->set_gpio_doc_switch(GPIO_LEVEL_HIGH);
	else
		pr_info("%s:%s there is no dock_sel pin!\n", MUIC_DEV_NAME, __func__);

	ret = com_to_audio(muic_data);
	if (ret) {
		pr_err("%s:%s com->audio set err\n", MUIC_DEV_NAME, __func__);
		return ret;
	}

	return ret;
}

static int switch_to_system_audio(struct sm5502_muic_data *muic_data)
{
	struct muic_platform_data *pdata = muic_data->pdata;
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	if (pdata->set_gpio_doc_switch)
		pdata->set_gpio_doc_switch(GPIO_LEVEL_LOW);
	else
		pr_info("%s:%s there is no dock_sel pin!\n", MUIC_DEV_NAME, __func__);

	return ret;
}


static int switch_to_ap_usb(struct sm5502_muic_data *muic_data)
{
	struct muic_platform_data *pdata = muic_data->pdata;
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	if (pdata->set_gpio_usb_sel)
		pdata->set_gpio_usb_sel(MUIC_PATH_USB_AP);
	else
		pr_info("%s:%s there is no usb_sel pin!\n", MUIC_DEV_NAME, __func__);

	ret = com_to_usb(muic_data);
	if (ret) {
		pr_err("%s:%s com->usb set err\n", MUIC_DEV_NAME, __func__);
		return ret;
	}

	return ret;
}

static int switch_to_cp_usb(struct sm5502_muic_data *muic_data)
{
	struct muic_platform_data *pdata = muic_data->pdata;
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	if (pdata->set_gpio_usb_sel)
		pdata->set_gpio_usb_sel(MUIC_PATH_USB_CP);
	else
		pr_info("%s:%s there is no usb_sel pin!\n", MUIC_DEV_NAME, __func__);

	ret = com_to_usb(muic_data);
	if (ret) {
		pr_err("%s:%s com->usb set err\n", MUIC_DEV_NAME, __func__);
		return ret;
	}

	return ret;
}

static int switch_to_ap_uart(struct sm5502_muic_data *muic_data)
{
	struct muic_platform_data *pdata = muic_data->pdata;
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	if (pdata->set_gpio_uart_sel)
		pdata->set_gpio_uart_sel(MUIC_PATH_UART_AP);
	else
		pr_info("%s:%s there is no uart_sel pin!\n", MUIC_DEV_NAME, __func__);

	ret = com_to_uart(muic_data);
	if (ret) {
		pr_err("%s:%s com->uart set err\n", MUIC_DEV_NAME, __func__);
		return ret;
	}

	return ret;
}

static int switch_to_cp_uart(struct sm5502_muic_data *muic_data)
{
	struct muic_platform_data *pdata = muic_data->pdata;
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	if (pdata->set_gpio_uart_sel)
		pdata->set_gpio_uart_sel(MUIC_PATH_UART_CP);
	else
		pr_info("%s:%s there is no uart_sel pin!\n", MUIC_DEV_NAME, __func__);

	ret = com_to_uart(muic_data);
	if (ret) {
		pr_err("%s:%s com->uart set err\n", MUIC_DEV_NAME, __func__);
		return ret;
	}

	return ret;
}

static int attach_charger(struct sm5502_muic_data *muic_data,
			enum muic_attached_dev new_dev)
{
	int ret = 0;

	pr_info("%s:%s new_dev(%d)\n", MUIC_DEV_NAME, __func__, new_dev);

	if (!muic_data->switch_data->charger_cb) {
		pr_err("%s:%s charger_cb is NULL\n", MUIC_DEV_NAME, __func__);
		return -ENXIO;
	}
/*
	if (muic_data->attached_dev == new_dev) {
		pr_info("%s:%s already, ignore.\n", MUIC_DEV_NAME, __func__);
		return ret;
	}
*/
	ret = muic_data->switch_data->charger_cb(new_dev);
	if (ret) {
		muic_data->attached_dev = ATTACHED_DEV_NONE_MUIC;
		pr_err("%s:%s charger_cb(%d) err\n", MUIC_DEV_NAME, __func__,
				ret);
	}

	muic_data->attached_dev = new_dev;

	return ret;
}

static int detach_charger(struct sm5502_muic_data *muic_data)
{
	struct sec_switch_data *switch_data = muic_data->switch_data;
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	if (!switch_data->charger_cb) {
		pr_err("%s:%s charger_cb is NULL\n", MUIC_DEV_NAME, __func__);
		return -ENXIO;
	}

	ret = switch_data->charger_cb(ATTACHED_DEV_NONE_MUIC);
	if (ret) {
		pr_err("%s:%s charger_cb(%d) err\n", MUIC_DEV_NAME, __func__,
				ret);
		return ret;
	}

	muic_data->attached_dev = ATTACHED_DEV_NONE_MUIC;

	return ret;
}

static int attach_usb_util(struct sm5502_muic_data *muic_data,
			enum muic_attached_dev new_dev)
{
	int ret = 0;

	ret = attach_charger(muic_data, new_dev);
	if (ret)
		return ret;

	if (muic_data->pdata->usb_path == MUIC_PATH_USB_CP) {
		ret = switch_to_cp_usb(muic_data);
		return ret;
	}

	ret = switch_to_ap_usb(muic_data);
	return ret;
}

static int attach_usb(struct sm5502_muic_data *muic_data,
			enum muic_attached_dev new_dev)
{
	struct muic_platform_data *pdata = muic_data->pdata;
	struct sec_switch_data *switch_data = muic_data->switch_data;
	int ret = 0;

	if (muic_data->attached_dev == new_dev) {
		pr_info("%s:%s duplicated(USB)\n", MUIC_DEV_NAME, __func__);
		return ret;
	}

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	ret = attach_usb_util(muic_data, new_dev);
	if (ret)
		return ret;

	if ((pdata->usb_path == MUIC_PATH_USB_AP) &&
			(switch_data->usb_cb) && (muic_data->is_usb_ready))
		switch_data->usb_cb(USB_CABLE_ATTACHED);

	return ret;
}
static int attach_otg_usb(struct sm5502_muic_data *muic_data,
			enum muic_attached_dev new_dev)
{
	struct muic_platform_data *pdata = muic_data->pdata;
	struct sec_switch_data *switch_data = muic_data->switch_data;
	int ret = 0;

	if (muic_data->attached_dev == new_dev) {
		pr_info("%s:%s duplicated(USB)\n", MUIC_DEV_NAME, __func__);
		return ret;
	}

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	if (muic_data->pdata->usb_path == MUIC_PATH_USB_CP)
		ret = switch_to_cp_usb(muic_data);
	else
		ret = switch_to_ap_usb(muic_data);

	muic_data->attached_dev = new_dev;

	if ((pdata->usb_path == MUIC_PATH_USB_AP) &&
			(switch_data->usb_cb) && (muic_data->is_usb_ready))
		switch_data->usb_cb(USB_OTGHOST_ATTACHED);

	return ret;
}

static int detach_otg_usb(struct sm5502_muic_data *muic_data)
{
	struct muic_platform_data *pdata = muic_data->pdata;
	struct sec_switch_data *switch_data = muic_data->switch_data;
	int ret = 0;

	pr_info("%s:%s attached_dev type(%d)\n", MUIC_DEV_NAME, __func__,
			muic_data->attached_dev);

	ret = com_to_open(muic_data);
	if (ret)
		return ret;

	muic_data->attached_dev = ATTACHED_DEV_NONE_MUIC;

	if (pdata->usb_path == MUIC_PATH_USB_CP)
		return ret;

	if ((pdata->usb_path == MUIC_PATH_USB_AP) &&
			(switch_data->usb_cb) && (muic_data->is_usb_ready))
		switch_data->usb_cb(USB_OTGHOST_DETACHED);

	return ret;
}



static int detach_usb(struct sm5502_muic_data *muic_data)
{
	struct muic_platform_data *pdata = muic_data->pdata;
	struct sec_switch_data *switch_data = muic_data->switch_data;
	int ret = 0;

	pr_info("%s:%s attached_dev type(%d)\n", MUIC_DEV_NAME, __func__,
			muic_data->attached_dev);

	ret = detach_charger(muic_data);
	if (ret)
		return ret;

	ret = com_to_open(muic_data);
	if (ret)
		return ret;

	muic_data->attached_dev = ATTACHED_DEV_NONE_MUIC;

	if (pdata->usb_path == MUIC_PATH_USB_CP)
		return ret;

	if ((pdata->usb_path == MUIC_PATH_USB_AP) &&
			(switch_data->usb_cb) && (muic_data->is_usb_ready))
		switch_data->usb_cb(USB_CABLE_DETACHED);

	return ret;
}

static int set_vbus_interrupt(struct sm5502_muic_data *muic_data, int enable)
{
	struct i2c_client *i2c = muic_data->i2c;
	int ret = 0;

	if (enable)
	{
		ret = sm5502_i2c_write_byte(i2c, SM5502_MUIC_REG_INTMASK2,
			REG_INTMASK2_VBUS);
		if (ret < 0)
			pr_err("%s: err mask interrupt2(%d)\n", __func__, ret);
	}
	else
	{
		ret = sm5502_i2c_write_byte(i2c, SM5502_MUIC_REG_INTMASK2,
			REG_INTMASK2_VALUE);
		if (ret < 0)
			pr_err("%s: err mask interrupt2(%d)\n", __func__, ret);
	}
	return ret;
}

static int attach_deskdock(struct sm5502_muic_data *muic_data,
			enum muic_attached_dev new_dev, u8 vbvolt)
{
	int ret = 0;

	pr_info("%s:%s vbus(%x)\n", MUIC_DEV_NAME, __func__, vbvolt);

	ret = set_vbus_interrupt(muic_data, 1);
	if (ret)
		return ret;

	ret = switch_to_dock_audio(muic_data);
	if (ret)
		return ret;

	if(muic_data->attached_dev != ATTACHED_DEV_DESKDOCK_MUIC
		&& muic_data->switch_data->dock_cb)
			muic_data->switch_data->dock_cb(MUIC_DOCK_DESKDOCK);

	if (vbvolt)
		ret = attach_charger(muic_data, new_dev);
	else
		ret = detach_charger(muic_data);
	if (ret)
		return ret;

	muic_data->attached_dev = new_dev;

	return ret;
}

static int detach_deskdock(struct sm5502_muic_data *muic_data)
{
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	ret = set_vbus_interrupt(muic_data, 0);
	if (ret)
		return ret;

	ret = switch_to_system_audio(muic_data);
	if(ret)
		pr_err("%s:%s err changing audio path(%d)\n", MUIC_DEV_NAME,
			__func__, ret);

	ret = detach_charger(muic_data);
	if(ret)
		pr_err("%s:%s err detach_charger(%d)\n", MUIC_DEV_NAME, __func__, ret);

	if(muic_data->switch_data->dock_cb)
		muic_data->switch_data->dock_cb(MUIC_DOCK_DETACHED);

	muic_data->attached_dev = ATTACHED_DEV_NONE_MUIC;

	return ret;
}

static int attach_audiodock(struct sm5502_muic_data *muic_data,
			enum muic_attached_dev new_dev, u8 vbus)
{
	struct muic_platform_data *pdata = muic_data->pdata;
	struct sec_switch_data *switch_data = muic_data->switch_data;
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	ret = set_vbus_interrupt(muic_data, 1);
	if (ret)
		return ret;

	if (!vbus)
	{
		ret = detach_charger(muic_data);
		if(ret)
			pr_err("%s:%s err detach_charger(%d)\n",
				MUIC_DEV_NAME, __func__, ret);

		if(muic_data->switch_data->dock_cb)
			muic_data->switch_data->dock_cb(MUIC_DOCK_DETACHED);

		ret = com_to_open(muic_data);
		if (ret)
			return ret;

		muic_data->attached_dev = new_dev;

		if ((pdata->usb_path == MUIC_PATH_USB_AP) &&
			(switch_data->usb_cb) && (muic_data->is_usb_ready))
			switch_data->usb_cb(USB_POWERED_HOST_DETACHED);

		return ret;
	}

	ret = attach_usb_util(muic_data, new_dev);
	if (ret)
		pr_info("%s:%s attach_usb_util(%d)\n", MUIC_DEV_NAME, __func__,ret);

	muic_data->attached_dev = new_dev;

	if ((pdata->usb_path == MUIC_PATH_USB_AP) &&
			(switch_data->usb_cb) && (muic_data->is_usb_ready))
		switch_data->usb_cb(USB_POWERED_HOST_ATTACHED);

	if(muic_data->switch_data->dock_cb)
		muic_data->switch_data->dock_cb(MUIC_DOCK_AUDIODOCK);

	return ret;
}

static int detach_audiodock(struct sm5502_muic_data *muic_data)
{
	struct muic_platform_data *pdata = muic_data->pdata;
	struct sec_switch_data *switch_data = muic_data->switch_data;
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	ret = set_vbus_interrupt(muic_data, 0);
	if (ret)
		return ret;

	ret = detach_charger(muic_data);
	if(ret)
		pr_err("%s:%s err detach_charger(%d)\n", MUIC_DEV_NAME, __func__, ret);

	if(muic_data->switch_data->dock_cb)
		muic_data->switch_data->dock_cb(MUIC_DOCK_DETACHED);

	ret = com_to_open(muic_data);
	if (ret)
		return ret;

	muic_data->attached_dev = ATTACHED_DEV_NONE_MUIC;

	if ((pdata->usb_path == MUIC_PATH_USB_AP) &&
			(switch_data->usb_cb) && (muic_data->is_usb_ready))
		switch_data->usb_cb(USB_POWERED_HOST_DETACHED);

	return ret;
}

static int attach_jig_uart_boot_off(struct sm5502_muic_data *muic_data,
				u8 vbvolt)
{
	struct muic_platform_data *pdata = muic_data->pdata;
	struct sec_switch_data *sd = muic_data->switch_data;
	enum muic_attached_dev new_dev;
	enum muic_attached_dev curr_dev = muic_data->attached_dev;
	int ret = 0;

	pr_info("%s:%s JIG UART BOOT-OFF(0x%x)\n", MUIC_DEV_NAME, __func__,
			vbvolt);

	ret = set_vbus_interrupt(muic_data, 1);
	if (ret)
		return ret;

	if (pdata->uart_path == MUIC_PATH_UART_AP)
		ret = switch_to_ap_uart(muic_data);
	else
		ret = switch_to_cp_uart(muic_data);

	/* if VBUS is enabled, call host_notify_cb to check if it is OTGTEST*/
	if (vbvolt)
	{
		if(sd->host_notify_cb && sd->host_notify_cb(1) > 0)
		{
			pr_info("%s:%s OTG_TEST\n", MUIC_DEV_NAME, __func__);
			/* in OTG_TEST mode, do not charge */
			new_dev = ATTACHED_DEV_NONE_MUIC;
			ret = attach_charger(muic_data, new_dev);
			muic_data->attached_dev =
				ATTACHED_DEV_JIG_UART_OFF_VB_MUIC;
		}
		else
		{
			/* JIG_UART_OFF_VB */
			new_dev = ATTACHED_DEV_JIG_UART_OFF_VB_MUIC;
			ret = attach_charger(muic_data, new_dev);
		}
	}
	else
	{
		if (curr_dev == ATTACHED_DEV_JIG_UART_OFF_VB_MUIC)
		{
			if (sd->host_notify_cb)
			{
				sd->host_notify_cb(0);
			}
		}
		new_dev = ATTACHED_DEV_JIG_UART_OFF_MUIC;
		ret = attach_charger(muic_data, new_dev);
	}

	return ret;
}
static int detach_jig_uart_boot_off(struct sm5502_muic_data *muic_data)
{
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	ret = set_vbus_interrupt(muic_data, 0);
	if (ret)
		return ret;

	ret = detach_charger(muic_data);
	if(ret)
		pr_err("%s:%s err detach_charger(%d)\n", MUIC_DEV_NAME, __func__, ret);

	muic_data->attached_dev = ATTACHED_DEV_NONE_MUIC;

	return ret;
}

static int attach_jig_usb_boot_off(struct sm5502_muic_data *muic_data,
				u8 vbvolt)
{
	struct muic_platform_data *pdata = muic_data->pdata;
	struct sec_switch_data *switch_data = muic_data->switch_data;
	int ret = 0;

	if (muic_data->attached_dev == ATTACHED_DEV_JIG_USB_OFF_MUIC) {
		pr_info("%s:%s duplicated(JIG USB OFF)\n", MUIC_DEV_NAME,
				__func__);
		return ret;
	}

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	ret = attach_usb_util(muic_data, ATTACHED_DEV_JIG_USB_OFF_MUIC);
	if (ret)
		return ret;

	if ((pdata->usb_path == MUIC_PATH_USB_AP) &&
			(switch_data->usb_cb) && (muic_data->is_usb_ready))
		switch_data->usb_cb(USB_CABLE_ATTACHED);

	return ret;
}

static int attach_jig_usb_boot_on(struct sm5502_muic_data *muic_data,
				u8 vbvolt)
{
	struct muic_platform_data *pdata = muic_data->pdata;
	struct sec_switch_data *switch_data = muic_data->switch_data;
	int ret = 0;

	if (muic_data->attached_dev == ATTACHED_DEV_JIG_USB_ON_MUIC) {
		pr_info("%s:%s duplicated(JIG USB ON)\n", MUIC_DEV_NAME,
				__func__);
		return ret;
	}

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	ret = attach_usb_util(muic_data, ATTACHED_DEV_JIG_USB_ON_MUIC);
	if (ret)
		return ret;

	if ((pdata->usb_path == MUIC_PATH_USB_AP) &&
			(switch_data->usb_cb) && (muic_data->is_usb_ready))
		switch_data->usb_cb(USB_CABLE_ATTACHED);

	return ret;
}

static void sm5502_muic_handle_attach(struct sm5502_muic_data *muic_data,
			enum muic_attached_dev new_dev, int adc, u8 vbvolt)
{
	int ret = 0;

	switch (muic_data->attached_dev) {
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_CDP_MUIC:
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
		if (new_dev != muic_data->attached_dev) {
			pr_warn("%s:%s new(%d)!=attached(%d), assume detach\n",
					MUIC_DEV_NAME, __func__, new_dev,
					muic_data->attached_dev);
			ret = detach_usb(muic_data);
		}
		break;
	case ATTACHED_DEV_OTG_MUIC:
		if (new_dev != muic_data->attached_dev) {
			pr_warn("%s:%s new(%d)!=attached(%d), assume detach\n",
					MUIC_DEV_NAME, __func__, new_dev,
					muic_data->attached_dev);
			ret = detach_otg_usb(muic_data);
		}
		break;

	case ATTACHED_DEV_AUDIODOCK_MUIC:
		if (new_dev != muic_data->attached_dev) {
			pr_warn("%s:%s new(%d)!=attached(%d), assume detach\n",
					MUIC_DEV_NAME, __func__, new_dev,
					muic_data->attached_dev);
			ret = detach_audiodock(muic_data);
		}
		break;

	case ATTACHED_DEV_TA_MUIC:
		if (new_dev != muic_data->attached_dev) {
			pr_warn("%s:%s new(%d)!=attached(%d), assume detach\n",
					MUIC_DEV_NAME, __func__, new_dev,
					muic_data->attached_dev);
			ret = detach_charger(muic_data);
		}
		break;
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
		if (new_dev != ATTACHED_DEV_JIG_UART_OFF_MUIC) {
			pr_warn("%s:%s new(%d)!=attached(%d), assume detach\n",
					MUIC_DEV_NAME, __func__, new_dev,
					muic_data->attached_dev);
			ret = detach_jig_uart_boot_off(muic_data);
		}
		break;

	case ATTACHED_DEV_JIG_UART_ON_MUIC:
	case ATTACHED_DEV_DESKDOCK_MUIC:
		if (new_dev != muic_data->attached_dev) {
			pr_warn("%s:%s new(%d)!=attached(%d), assume detach\n",
					MUIC_DEV_NAME, __func__, new_dev,
					muic_data->attached_dev);
			ret = detach_deskdock(muic_data);
		}
		break;
	default:
		break;
	}

	pr_info("[TEST] %s, 0x%x\n", __func__, new_dev);

	switch (new_dev) {
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_CDP_MUIC:
		ret = attach_usb(muic_data, new_dev);
		break;
	case ATTACHED_DEV_OTG_MUIC:
		ret = attach_otg_usb(muic_data, new_dev);
		break;
	case ATTACHED_DEV_AUDIODOCK_MUIC:
		ret = attach_audiodock(muic_data, new_dev, vbvolt);
		break;
	case ATTACHED_DEV_TA_MUIC:
	case ATTACHED_DEV_UNKNOWN_VB_MUIC:
		com_to_open_with_vbus(muic_data);
		ret = attach_charger(muic_data, new_dev);
		break;
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
		ret = attach_jig_uart_boot_off(muic_data, vbvolt);
		break;
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
		/* call attach_deskdock to wake up the device */
		ret = attach_deskdock(muic_data, new_dev, vbvolt);
		break;
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
		ret = attach_jig_usb_boot_off(muic_data, vbvolt);
		break;
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
		ret = attach_jig_usb_boot_on(muic_data, vbvolt);
		break;
	case ATTACHED_DEV_DESKDOCK_MUIC:
		ret = attach_deskdock(muic_data, new_dev, vbvolt);
		break;
	default:
		pr_warn("%s:%s unsupported dev=%d, adc=0x%x, vbus=%c\n",
				MUIC_DEV_NAME, __func__, new_dev, adc,
				(vbvolt ? 'O' : 'X'));
		break;
	}

	if(ret) pr_warn("%s:%s something wrong with attaching %d (ERR=%d)\n",
				MUIC_DEV_NAME, __func__, new_dev, ret);
}

static void sm5502_muic_handle_detach(struct sm5502_muic_data *muic_data)
{
	int ret = 0;

	ret = com_to_open(muic_data);

	switch (muic_data->attached_dev) {
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_CDP_MUIC:
		ret = detach_usb(muic_data);
		break;
	case ATTACHED_DEV_OTG_MUIC:
		ret = detach_otg_usb(muic_data);
		break;
	case ATTACHED_DEV_TA_MUIC:
		ret = detach_charger(muic_data);
		break;
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
		ret = detach_jig_uart_boot_off(muic_data);
		break;
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
	case ATTACHED_DEV_DESKDOCK_MUIC:
		ret = detach_deskdock(muic_data);
		break;
	case ATTACHED_DEV_AUDIODOCK_MUIC:
		ret = detach_audiodock(muic_data);
		break;
	case ATTACHED_DEV_NONE_MUIC:
		pr_info("%s:%s duplicated(NONE)\n", MUIC_DEV_NAME, __func__);
		break;
	case ATTACHED_DEV_UNKNOWN_MUIC:
		pr_info("%s:%s UNKNOWN\n", MUIC_DEV_NAME, __func__);
		ret = detach_charger(muic_data);
		muic_data->attached_dev = ATTACHED_DEV_NONE_MUIC;
		break;
	default:
		pr_info("%s:%s invalid attached_dev type(%d)\n", MUIC_DEV_NAME,
			__func__, muic_data->attached_dev);
		muic_data->attached_dev = ATTACHED_DEV_NONE_MUIC;
		break;
	}
    if(ret) pr_warn("%s:%s something wrong with detaching %d (ERR=%d)\n",
				MUIC_DEV_NAME, __func__, muic_data->attached_dev, ret);

}

static void sm5502_muic_detect_dev(struct sm5502_muic_data *muic_data)
{
	struct i2c_client *i2c = muic_data->i2c;
	enum muic_attached_dev new_dev = ATTACHED_DEV_UNKNOWN_MUIC;
	int intr = MUIC_INTR_DETACH;
	int vbvolt = 0;
	int val1, val2, val3, adc;

/*  don't call filter_vps_cb, since filter_vps_cb is empty anyways
	struct sec_switch_data *switch_data = muic_data->switch_data;

	if(switch_data->filter_vps_cb)
		switch_data->filter_vps_cb(muic_data);
*/

	val1 = sm5502_i2c_read_byte(i2c, SM5502_MUIC_REG_DEV_T1);
	if (val1 < 0)
		pr_err("%s:%s err %d\n", MUIC_DEV_NAME, __func__, val1);

	val2 = sm5502_i2c_read_byte(i2c, SM5502_MUIC_REG_DEV_T2);
	if (val2 < 0)
		pr_err("%s:%s err %d\n", MUIC_DEV_NAME, __func__, val2);

	val3 = sm5502_i2c_read_byte(i2c, SM5502_MUIC_REG_DEV_T3);
	if (val3 < 0)
		pr_err("%s:%s err %d\n", MUIC_DEV_NAME, __func__, val3);

	vbvolt = sm5502_i2c_read_byte(i2c, SM5502_MUIC_REG_RSVD_ID1);
	vbvolt &= RSVD1_VBUS;

	adc = sm5502_i2c_read_byte(i2c, SM5502_MUIC_REG_ADC);
	pr_info("%s:%s dev[1:0x%x, 2:0x%x, 3:0x%x], adc:0x%x, vbvolt:0x%x\n",
		MUIC_DEV_NAME, __func__, val1, val2, val3, adc, vbvolt);

	/* Attached */
	switch (val1) {
	case DEV_TYPE1_CDP:
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_CDP_MUIC;
		pr_info("%s : USB_CDP DETECTED\n", MUIC_DEV_NAME);
		break;
	case DEV_TYPE1_USB:
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_USB_MUIC;
		pr_info("%s : USB DETECTED\n", MUIC_DEV_NAME);
		break;
	case DEV_TYPE1_DEDICATED_CHG:
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_TA_MUIC;
		pr_info("%s : CHARGER DETECTED\n", MUIC_DEV_NAME);
		break;
	case DEV_TYPE1_USB_OTG:
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_OTG_MUIC;
		pr_info("%s : USB_OTG DETECTED\n", MUIC_DEV_NAME);
		break;
	default:
		break;
	}

	switch (val2) {
	case DEV_TYPE2_JIG_UART_OFF:
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_JIG_UART_OFF_MUIC;
		pr_info("%s : JIG_UART_OFF DETECTED\n", MUIC_DEV_NAME);
		break;
	case DEV_TYPE2_JIG_USB_OFF:
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_JIG_USB_OFF_MUIC;
		pr_info("%s : JIG_USB_OFF DETECTED\n", MUIC_DEV_NAME);
		break;
	case DEV_TYPE2_JIG_USB_ON:
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_JIG_USB_ON_MUIC;
		pr_info("%s : JIG_USB_ON DETECTED\n", MUIC_DEV_NAME);
		break;
	default:
		break;
	}

	if (val3 & DEV_TYPE3_CHG_TYPE) {
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_TA_MUIC;
		pr_info("%s : TYPE3_CHARGER DETECTED\n", MUIC_DEV_NAME);
	}

	if (val2 & DEV_TYPE2_AV || val3 & DEV_TYPE3_AV_WITH_VBUS) {
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_DESKDOCK_MUIC;
		pr_info("%s : DESKDOCK DETECTED\n", MUIC_DEV_NAME);
	}

	/* If there is no matching device found using device type registers
		use ADC to find the attached device */
	if(intr != MUIC_INTR_ATTACH)
		switch (adc) {
		case ADC_CEA936ATYPE1_CHG : /*200k ohm */
			intr = MUIC_INTR_ATTACH;
			/* This is workaournd for LG USB cable which has 219k ohm ID */
			new_dev = ATTACHED_DEV_USB_MUIC;
			pr_info("%s : TYPE1 CHARGER DETECTED(USB)\n", MUIC_DEV_NAME);
			break;
		case ADC_CEA936ATYPE2_CHG:
			intr = MUIC_INTR_ATTACH;
			new_dev = ATTACHED_DEV_TA_MUIC;
			pr_info("%s : TYPE1/2 CHARGER DETECTED(TA)\n", MUIC_DEV_NAME);
			break;
		case ADC_JIG_USB_OFF:
			if (new_dev != ATTACHED_DEV_JIG_USB_OFF_MUIC) {
				intr = MUIC_INTR_ATTACH;
				new_dev = ATTACHED_DEV_JIG_USB_OFF_MUIC;
				pr_info("%s : ADC JIG_USB_OFF DETECTED\n", MUIC_DEV_NAME);
			}
			break;
		case ADC_JIG_USB_ON:
			if (new_dev != ATTACHED_DEV_JIG_USB_ON_MUIC) {
				intr = MUIC_INTR_ATTACH;
				new_dev = ATTACHED_DEV_JIG_USB_ON_MUIC;
				pr_info("%s : ADC JIG_USB_ON DETECTED\n", MUIC_DEV_NAME);
			}
			break;
		case ADC_JIG_UART_OFF:
			if (new_dev != ATTACHED_DEV_JIG_UART_OFF_MUIC) {
				intr = MUIC_INTR_ATTACH;
				new_dev = ATTACHED_DEV_JIG_UART_OFF_MUIC;
				pr_info("%s : ADC JIG_UART_OFF DETECTED\n", MUIC_DEV_NAME);
			}
			break;
		case ADC_JIG_UART_ON:
			/* This is the mode to wake up device during factory mode. */
			if (new_dev != ATTACHED_DEV_JIG_UART_ON_MUIC
					&& muic_data->is_factory_start) {
				intr = MUIC_INTR_ATTACH;
				new_dev = ATTACHED_DEV_JIG_UART_ON_MUIC;
				pr_info("%s : ADC JIG_UART_ON DETECTED\n", MUIC_DEV_NAME);
			}
			break;
		case ADC_AUDIODOCK:
			intr = MUIC_INTR_ATTACH;
			new_dev = ATTACHED_DEV_AUDIODOCK_MUIC;
			pr_info("%s : ADC AUDIODOCK DETECTED\n", MUIC_DEV_NAME);
			break;
		case ADC_OPEN:
			/* sometimes muic fails to catch JIG_UART_OFF detaching */
			/* double check with ADC */
			if (new_dev == ATTACHED_DEV_JIG_UART_OFF_MUIC)
			{
				new_dev = ATTACHED_DEV_UNKNOWN_MUIC;
				intr = MUIC_INTR_DETACH;
				pr_info("%s : ADC OPEN DETECTED\n", MUIC_DEV_NAME);
			}
			break;
		default:
			pr_warn("%s:%s unsupported ADC(0x%02x)\n",
				MUIC_DEV_NAME, __func__, adc);
			if (vbvolt) {
				new_dev = ATTACHED_DEV_UNKNOWN_VB_MUIC;
				intr = MUIC_INTR_ATTACH;
			} else
				intr = MUIC_INTR_DETACH;
			break;
		}

	if (intr == MUIC_INTR_ATTACH) {
		sm5502_muic_handle_attach(muic_data, new_dev, adc, vbvolt);
	} else {
		sm5502_muic_handle_detach(muic_data);
	}
}

static int sm5502_muic_reg_init(struct sm5502_muic_data *muic_data)
{
	struct i2c_client *i2c = muic_data->i2c;
	int ret;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	ret = sm5502_i2c_write_byte(i2c, SM5502_MUIC_REG_INTMASK1,
			REG_INTMASK1_VALUE);
	if (ret < 0)
		pr_err("%s: err mask interrupt1(%d)\n", __func__, ret);

	ret = sm5502_i2c_write_byte(i2c, SM5502_MUIC_REG_INTMASK2,
			REG_INTMASK2_VALUE);
	if (ret < 0)
		pr_err("%s: err mask interrupt1(%d)\n", __func__, ret);

	ret = sm5502_i2c_write_byte(i2c, SM5502_MUIC_REG_CTRL, CTRL_MASK);
	if (ret < 0)
		pr_err("%s: err write ctrl(%d)\n", __func__, ret);

	ret = sm5502_i2c_write_byte(i2c, SM5502_MUIC_REG_TIMING1,
						REG_TIMING1_VALUE);
	if (ret < 0)
		pr_err("%s: err write timing1(%d)\n", __func__, ret);

	/* enable ChargePump */
	ret = sm5502_i2c_write_byte(i2c, SM5502_MUIC_REG_RSVD_ID3, 0);
	if (ret < 0)
		pr_err("%s: err write ctrl(%d)\n", __func__, ret);

	return ret;
}


static irqreturn_t sm5502_muic_irq_thread(int irq, void *data)
{
	struct sm5502_muic_data *muic_data = data;
	struct i2c_client *i2c = muic_data->i2c;
	int intr1, intr2;

	pr_info("%s:%s irq(%d)\n", MUIC_DEV_NAME, __func__, irq);

	mutex_lock(&muic_data->muic_mutex);

	/* read and clear interrupt status bits */
	intr1 = sm5502_i2c_read_byte(i2c, SM5502_MUIC_REG_INT1);
	intr2 = sm5502_i2c_read_byte(i2c, SM5502_MUIC_REG_INT2);

	if ((intr1 < 0) || (intr2 < 0)) {
		pr_err("%s: err read interrupt status [1:0x%x, 2:0x%x]\n",
				__func__, intr1, intr2);
		goto skip_detect_dev;
	}

	pr_info("%s:%s intr[1:0x%x, 2:0x%x]\n", MUIC_DEV_NAME, __func__,
			intr1, intr2);

	if (intr1 & INT_OVP_EN_MASK) {
		int ctrl;
		ctrl = sm5502_i2c_read_byte(i2c, SM5502_MUIC_REG_CTRL);
		if (ctrl == 0x1F) {
			/* CONTROL register is reset to 1F */
			pr_err("%s:muic could have been reseted. Initilize!!\n",
				__func__);
			sm5502_muic_reg_init(muic_data);

			/* MUIC Interrupt On */
			set_int_mask(muic_data, false);
		}
		if (intr2 & INT_CHG_DET_MASK) {
			sm5502_muic_handle_detach(muic_data);
			goto skip_detect_dev;
		}
	}

	/* device detection */
	sm5502_muic_detect_dev(muic_data);

skip_detect_dev:
	mutex_unlock(&muic_data->muic_mutex);

	return IRQ_HANDLED;
}

static void sm5502_muic_usb_detect(struct work_struct *work)
{
	struct sm5502_muic_data *muic_data =
		container_of(work, struct sm5502_muic_data, usb_work.work);
	struct muic_platform_data *pdata = muic_data->pdata;
	struct sec_switch_data *switch_data = muic_data->switch_data;

	pr_info("%s:%s pdata->usb_path(%d)\n", MUIC_DEV_NAME, __func__,
			pdata->usb_path);

	mutex_lock(&muic_data->muic_mutex);
	muic_data->is_usb_ready = true;

	if (pdata->usb_path != MUIC_PATH_USB_CP) {
		if (switch_data->usb_cb) {
			switch ((int)muic_data->attached_dev) {
			case ATTACHED_DEV_USB_MUIC:
			case ATTACHED_DEV_CDP_MUIC:
			case ATTACHED_DEV_JIG_USB_OFF_MUIC:
			case ATTACHED_DEV_JIG_USB_ON_MUIC:
				switch_data->usb_cb(USB_CABLE_ATTACHED);
				break;
			case ATTACHED_DEV_OTG_MUIC:
				switch_data->usb_cb(USB_OTGHOST_ATTACHED);
				break;
			case ATTACHED_DEV_AUDIODOCK_MUIC:
				switch_data->usb_cb(USB_POWERED_HOST_ATTACHED);
				break;
			}
		}
	}
	mutex_unlock(&muic_data->muic_mutex);
}

static void sm5502_muic_init_detect(struct work_struct *work)
{
	struct sm5502_muic_data *muic_data =
		container_of(work, struct sm5502_muic_data, init_work.work);

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	/* MUIC Interrupt On */
	set_int_mask(muic_data, false);

	sm5502_muic_irq_thread(-1, muic_data);
}

static int sm5502_init_rev_info(struct sm5502_muic_data *muic_data)
{
	u8 dev_id;
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	dev_id = sm5502_i2c_read_byte(muic_data->i2c, SM5502_MUIC_REG_DEVID);
	if (dev_id < 0) {
		pr_err("%s:%s i2c io error(%d)\n", MUIC_DEV_NAME, __func__,
				ret);
		ret = -ENODEV;
	} else {
		muic_data->muic_vendor = (dev_id & 0x7);
		muic_data->muic_version = ((dev_id & 0xF8) >> 3);
		pr_info("%s:%s device found: vendor=0x%x, ver=0x%x\n",
				MUIC_DEV_NAME, __func__, muic_data->muic_vendor,
				muic_data->muic_version);
	}

	return ret;
}

static int sm5502_muic_irq_init(struct sm5502_muic_data *muic_data)
{
	struct i2c_client *i2c = muic_data->i2c;
	struct muic_platform_data *pdata = muic_data->pdata;
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	if (!pdata->irq_gpio) {
		pr_warn("%s:%s No interrupt specified\n", MUIC_DEV_NAME,
				__func__);
		return -ENXIO;
	}

	i2c->irq = gpio_to_irq(pdata->irq_gpio);

	if (i2c->irq) {
		ret = request_threaded_irq(i2c->irq, NULL,
				sm5502_muic_irq_thread,
				(IRQF_TRIGGER_FALLING | IRQF_NO_SUSPEND),
				"sm5502-muic", muic_data);
		if (ret < 0) {
			pr_err("%s:%s failed to reqeust IRQ(%d)\n",
					MUIC_DEV_NAME, __func__, i2c->irq);
			return ret;
		}

		ret = enable_irq_wake(i2c->irq);
		if (ret < 0)
			pr_err("%s:%s failed to enable wakeup src\n",
					MUIC_DEV_NAME, __func__);
	}

	return ret;
}

static int sm5502_muic_probe(struct i2c_client *i2c,
				const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(i2c->dev.parent);
	struct muic_platform_data *pdata = i2c->dev.platform_data;
	struct sm5502_muic_data *muic_data;
	int ret = 0;

	/* save platfom data for gpio control fucntions */
	muic_pdata = *pdata;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		pr_err("%s: i2c functionality check error\n", __func__);
		ret = -EIO;
		goto err_return;
	}

	muic_data = kzalloc(sizeof(struct sm5502_muic_data), GFP_KERNEL);
	if (!muic_data) {
		pr_err("%s: failed to allocate driver data\n", __func__);
		ret = -ENOMEM;
		goto err_return;
	}

	if (!pdata) {
		pr_err("%s: failed to get i2c platform data\n", __func__);
		ret = -ENODEV;
		goto err_io;
	}
	muic_data->pdata = pdata;

	muic_data->i2c = i2c;
	i2c_set_clientdata(i2c, muic_data);

	muic_data->switch_data = &sec_switch_data;

	if (pdata->init_gpio_cb)
		ret = pdata->init_gpio_cb(get_switch_sel());
	if (ret) {
		pr_err("%s: failed to init gpio(%d)\n", __func__, ret);
		goto fail_init_gpio;
	}

	mutex_init(&muic_data->muic_mutex);

	muic_data->is_factory_start = false;
#if 0
	wake_lock_init(&muic_data->muic_wake_lock, WAKE_LOCK_SUSPEND,
		"muic wake lock");
#endif

	muic_data->attached_dev = ATTACHED_DEV_UNKNOWN_MUIC;
	muic_data->is_usb_ready = false;

	/* set switch device's driver_data */
	dev_set_drvdata(switch_device, muic_data);

	/* create sysfs group */
	ret = sysfs_create_group(&switch_device->kobj, &sm5502_muic_group);
	if (ret) {
		pr_err("%s: failed to create sm5502 muic attribute group\n",
			__func__);
		goto fail;
	}

	ret = sm5502_init_rev_info(muic_data);
	if (ret) {
		pr_err("%s: failed to init muic rev register(%d)\n", __func__,
			ret);
		goto fail;
	}

	ret = sm5502_muic_reg_init(muic_data);
	if (ret) {
		pr_err("%s: failed to init muic register(%d)\n", __func__, ret);
		goto fail;
	}

	if (muic_data->switch_data->init_cb)
		muic_data->switch_data->init_cb();

	ret = sm5502_muic_irq_init(muic_data);
	if (ret) {
		pr_err("%s: failed to init muic irq(%d)\n", __func__, ret);
		goto fail_init_irq;
	}

	/* initial cable detection */
	INIT_DELAYED_WORK(&muic_data->init_work, sm5502_muic_init_detect);
	schedule_delayed_work(&muic_data->init_work, msecs_to_jiffies(300));

	INIT_DELAYED_WORK(&muic_data->usb_work, sm5502_muic_usb_detect);
	schedule_delayed_work(&muic_data->usb_work, msecs_to_jiffies(13000));

	return 0;

fail_init_irq:
	if (i2c->irq)
		free_irq(i2c->irq, muic_data);
fail:
#if 0
	wake_lock_destroy(&info->muic_wake_lock);
#endif
	mutex_destroy(&muic_data->muic_mutex);
fail_init_gpio:
	i2c_set_clientdata(i2c, NULL);
err_io:
	kfree(muic_data);
err_return:
	return ret;
}

static int __devexit sm5502_muic_remove(struct i2c_client *i2c)
{
	struct sm5502_muic_data *muic_data = i2c_get_clientdata(i2c);
	sysfs_remove_group(&switch_device->kobj, &sm5502_muic_group);

	if (muic_data) {
		pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);
		cancel_delayed_work(&muic_data->init_work);
		cancel_delayed_work(&muic_data->usb_work);
		disable_irq_wake(muic_data->i2c->irq);
		free_irq(muic_data->i2c->irq, muic_data);
#if 0
		wake_lock_destroy(&info->muic_wake_lock);
#endif
		mutex_destroy(&muic_data->muic_mutex);
		i2c_set_clientdata(muic_data->i2c, NULL);
		kfree(muic_data);
	}
	return 0;
}

static const struct i2c_device_id sm5502_i2c_id[] = {
	{ MUIC_DEV_NAME, 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, sm5502_i2c_id);

static void sm5502_muic_shutdown(struct i2c_client *i2c)
{
	struct sm5502_muic_data *muic_data = i2c_get_clientdata(i2c);
	int ret;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);
	if (!muic_data->i2c) {
		pr_err("%s:%s no muic i2c client\n", MUIC_DEV_NAME, __func__);
		return;
	}

	pr_info("%s:%s open D+,D-,V_bus line\n", MUIC_DEV_NAME, __func__);
	ret = com_to_open(muic_data);
	if (ret < 0)
		pr_err("%s:%s fail to open mansw1 reg\n", MUIC_DEV_NAME,
				__func__);
}

static struct i2c_driver sm5502_muic_driver = {
	.driver		= {
		.name	= MUIC_DEV_NAME,
	},
	.probe		= sm5502_muic_probe,
	.remove		= __devexit_p(sm5502_muic_remove),
	.shutdown	= sm5502_muic_shutdown,
	.id_table	= sm5502_i2c_id,
};

static int __init sm5502_muic_init(void)
{
	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);
	return i2c_add_driver(&sm5502_muic_driver);
}
module_init(sm5502_muic_init);

static void __exit sm5502_muic_exit(void)
{
	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);
	i2c_del_driver(&sm5502_muic_driver);
}
module_exit(sm5502_muic_exit);

MODULE_AUTHOR("Hyoyoung Kim <hyway.kim@samsung.com>");
MODULE_DESCRIPTION("SM5502 USB Switch driver");
MODULE_LICENSE("GPL");
