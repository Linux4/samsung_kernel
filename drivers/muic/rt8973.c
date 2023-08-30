/*
 * driver/muic/rt8973.c - RT8973 micro USB switch device driver
 *
 * Copyright (C) 2016 Samsung Electronics
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
#include <linux/of_gpio.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/wakelock.h>

#include <linux/muic/muic.h>
#include <linux/muic/rt8973.h>

#if defined(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/muic_notifier.h>
#endif /* CONFIG_MUIC_NOTIFIER */

#if defined(CONFIG_VBUS_NOTIFIER)
#include <linux/vbus_notifier.h>
#endif /* CONFIG_VBUS_NOTIFIER */

static void rt8973_muic_handle_attach(struct rt8973_muic_data *muic_data,
			muic_attached_dev_t new_dev, int adc, u8 vbvolt);
static void rt8973_muic_handle_detach(struct rt8973_muic_data *muic_data);

/* DEBUG_MUIC */
#if defined(DEBUG_MUIC)
#define MAX_LOG 25
#define READ 0
#define WRITE 1

static u8 rt8973_log_cnt;
static u8 rt8973_log[MAX_LOG][3];

static void rt8973_reg_log(u8 reg, u8 value, u8 rw)
{
	rt8973_log[rt8973_log_cnt][0] = reg;
	rt8973_log[rt8973_log_cnt][1] = value;
	rt8973_log[rt8973_log_cnt][2] = rw;
	rt8973_log_cnt++;
	if (rt8973_log_cnt >= MAX_LOG)
		rt8973_log_cnt = 0;
}

static void rt8973_print_reg_log(void)
{
	int i;
	u8 reg, value, rw;
	char mesg[256] = "";

	for (i = 0; i < MAX_LOG; i++) {
		reg = rt8973_log[rt8973_log_cnt][0];
		value = rt8973_log[rt8973_log_cnt][1];
		rw = rt8973_log[rt8973_log_cnt][2];
		rt8973_log_cnt++;

		if (rt8973_log_cnt >= MAX_LOG)
			rt8973_log_cnt = 0;
		sprintf(mesg+strlen(mesg), "%x(%x)%x ", reg, value, rw);
	}
	pr_info("%s:%s:%s\n", MUIC_DEV_NAME, __func__, mesg);
}

void rt8973_read_reg_dump(struct rt8973_muic_data *muic, char *mesg)
{
	int val;

	val = i2c_smbus_read_byte_data(muic->i2c, RT8973_REG_CONTROL);
	sprintf(mesg, "CTRL:%x ", val);
	val = i2c_smbus_read_byte_data(muic->i2c, RT8973_REG_INT_MASK1);
	sprintf(mesg+strlen(mesg), "IM1:%x ", val);
	val = i2c_smbus_read_byte_data(muic->i2c, RT8973_REG_INT_MASK2);
	sprintf(mesg+strlen(mesg), "IM2:%x ", val);
	val = i2c_smbus_read_byte_data(muic->i2c, RT8973_REG_MANUAL_SW1);
	sprintf(mesg+strlen(mesg), "MS1:%x ", val);
	val = i2c_smbus_read_byte_data(muic->i2c, RT8973_REG_MANUAL_SW2);
	sprintf(mesg+strlen(mesg), "MS2:%x ", val);
	val = i2c_smbus_read_byte_data(muic->i2c, RT8973_REG_ADC);
	sprintf(mesg+strlen(mesg), "ADC:%x ", val);
	val = i2c_smbus_read_byte_data(muic->i2c, RT8973_REG_DEVICE1);
	sprintf(mesg+strlen(mesg), "DT1:%x ", val);
	val = i2c_smbus_read_byte_data(muic->i2c, RT8973_REG_DEVICE2);
	sprintf(mesg+strlen(mesg), "DT2:%x ", val);
}

void rt8973_print_reg_dump(struct rt8973_muic_data *muic_data)
{
	char mesg[256] = "";

	rt8973_read_reg_dump(muic_data, mesg);

	pr_info("%s:%s\n", __func__, mesg);
}
#endif

static int rt8973_i2c_read_byte(const struct i2c_client *client, u8 command)
{
	int ret;
	int retry = 0;

	ret = i2c_smbus_read_byte_data(client, command);

	while (ret < 0) {
		pr_err("%s:%s: reg(0x%x), retrying...\n", MUIC_DEV_NAME, __func__, command);
		if (retry > 5) {
			pr_err("%s:%s: retry failed!!\n", MUIC_DEV_NAME, __func__);
			break;
		}
		msleep(100);
		ret = i2c_smbus_read_byte_data(client, command);
		retry++;
	}

#ifdef DEBUG_MUIC
	rt8973_reg_log(command, ret, retry << 1 | READ);
#endif
	return ret;
}

static int rt8973_i2c_write_byte(const struct i2c_client *client, u8 command, u8 value)
{
	int ret;
	int retry = 0;
	int written = 0;

	ret = i2c_smbus_write_byte_data(client, command, value);

	while (ret < 0) {
		pr_err("%s:%s: reg(0x%x), retrying...\n", MUIC_DEV_NAME, __func__, command);
		if (retry > 5) {
			pr_err("%s:%s: retry failed!!\n", MUIC_DEV_NAME, __func__);
			break;
		}
		msleep(100);
		ret = i2c_smbus_read_byte_data(client, command);
		if (written < 0)
			pr_err("%s:%s: reg(0x%x)\n", MUIC_DEV_NAME, __func__, command);
		ret = i2c_smbus_write_byte_data(client, command, value);
		retry++;
	}
#ifdef DEBUG_MUIC
	rt8973_reg_log(command, value, retry << 1 | WRITE);
#endif
	return ret;
}

#if defined(GPIO_DOC_SWITCH)
static int rt8973_set_gpio_doc_switch(int val)
{
	int doc_switch_gpio = muic_pdata.gpio_doc_switch;
	int doc_switch_val;
	int ret;

	ret = gpio_request(doc_switch_gpio, "GPIO_DOC_SWITCH");
	if (ret) {
		pr_err("%s:%s: failed to gpio_request GPIO_DOC_SWITCH\n", MUIC_DEV_NAME, __func__);
		return ret;
	}

	doc_switch_val = gpio_get_value(doc_switch_gpio);

	if (gpio_is_valid(doc_switch_gpio))
			gpio_set_value(doc_switch_gpio, val);

	doc_switch_val = gpio_get_value(doc_switch_gpio);

	gpio_free(doc_switch_gpio);

	pr_info("%s:%s: GPIO_DOC_SWITCH(%d)=%c\n",
		MUIC_DEV_NAME, __func__, doc_switch_gpio, (doc_switch_val == 0 ? 'L' : 'H'));

	return 0;
}
#endif /* GPIO_DOC_SWITCH */

static ssize_t rt8973_muic_show_uart_en(struct device *dev,
						struct device_attribute *attr, char *buf)
{
	struct rt8973_muic_data *muic_data = dev_get_drvdata(dev);
	int ret = 0;

	if (!muic_data->is_rustproof) {
		pr_info("%s:%s: UART ENABLE\n", MUIC_DEV_NAME, __func__);
		ret = sprintf(buf, "1\n");
	} else {
		pr_info("%s:%s: UART DISABLE\n", MUIC_DEV_NAME, __func__);
		ret = sprintf(buf, "0\n");
	}

	return ret;
}

static ssize_t rt8973_muic_set_uart_en(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t count)
{
	struct rt8973_muic_data *muic_data = dev_get_drvdata(dev);

	if (!strncmp(buf, "1", 1))
		muic_data->is_rustproof = false;
	else if (!strncmp(buf, "0", 1))
		muic_data->is_rustproof = true;
	else
		pr_info("%s:%s: invalid value\n", MUIC_DEV_NAME, __func__);

	pr_info("%s:%s: uart_en(%d)\n", MUIC_DEV_NAME, __func__, !muic_data->is_rustproof);

	return count;
}

static ssize_t rt8973_muic_show_usb_en(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct rt8973_muic_data *muic_data = dev_get_drvdata(dev);

	return sprintf(buf, "%s:%s attached_dev = %d\n",
		MUIC_DEV_NAME, __func__, muic_data->attached_dev);
}

static ssize_t rt8973_muic_set_usb_en(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct rt8973_muic_data *muic_data = dev_get_drvdata(dev);
	muic_attached_dev_t new_dev = ATTACHED_DEV_USB_MUIC;

	if (!strncasecmp(buf, "1", 1))
		rt8973_muic_handle_attach(muic_data, new_dev, 0, 0);
	else if (!strncasecmp(buf, "0", 1))
		rt8973_muic_handle_detach(muic_data);
	else
		pr_info("%s:%s: invalid value\n", MUIC_DEV_NAME, __func__);

	pr_info("%s:%s: attached_dev(%d)\n", MUIC_DEV_NAME, __func__, muic_data->attached_dev);

	return count;
}

static ssize_t rt8973_muic_show_adc(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct rt8973_muic_data *muic_data = dev_get_drvdata(dev);
	int ret;

	mutex_lock(&muic_data->muic_mutex);
	ret = rt8973_i2c_read_byte(muic_data->i2c, RT8973_REG_ADC);
	mutex_unlock(&muic_data->muic_mutex);
	if (ret < 0) {
		pr_err("%s:%s: err read adc reg(%d)\n", MUIC_DEV_NAME, __func__, ret);
		return sprintf(buf, "UNKNOWN\n");
	}

	return sprintf(buf, "%x\n", (ret & ADC_ADC_MASK));
}

static ssize_t rt8973_muic_show_usb_state(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	struct rt8973_muic_data *muic_data = dev_get_drvdata(dev);
	static unsigned long swtich_slot_time;

	if (printk_timed_ratelimit(&swtich_slot_time, 5000))
		pr_info("%s:%s: muic_data->attached_dev(%d)\n",
			MUIC_DEV_NAME, __func__, muic_data->attached_dev);

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

#ifdef DEBUG_MUIC
static ssize_t rt8973_muic_show_mansw1(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct rt8973_muic_data *muic_data = dev_get_drvdata(dev);
	int ret;

	mutex_lock(&muic_data->muic_mutex);
	ret = rt8973_i2c_read_byte(muic_data->i2c, RT8973_REG_MANUAL_SW1);
	mutex_unlock(&muic_data->muic_mutex);

	pr_info("%s:%s: ret:%d buf%s\n", MUIC_DEV_NAME, __func__, ret, buf);

	if (ret < 0) {
		pr_err("%s:%s: fail to read muic reg\n", MUIC_DEV_NAME, __func__);
		return sprintf(buf, "UNKNOWN\n");
	}

	return sprintf(buf, "0x%x\n", ret);
}

static ssize_t rt8973_muic_show_mansw2(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct rt8973_muic_data *muic_data = dev_get_drvdata(dev);
	int ret;

	mutex_lock(&muic_data->muic_mutex);
	ret = rt8973_i2c_read_byte(muic_data->i2c, RT8973_REG_MANUAL_SW2);
	mutex_unlock(&muic_data->muic_mutex);

	pr_info("%s:%s: ret:%d buf%s\n", MUIC_DEV_NAME, __func__, ret, buf);

	if (ret < 0) {
		pr_err("%s:%s: fail to read muic reg\n", MUIC_DEV_NAME, __func__);
		return sprintf(buf, "UNKNOWN\n");
	}
	return sprintf(buf, "0x%x\n", ret);
}

static ssize_t rt8973_muic_show_interrupt_status(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct rt8973_muic_data *muic_data = dev_get_drvdata(dev);
	int st1, st2;

	mutex_lock(&muic_data->muic_mutex);
	st1 = rt8973_i2c_read_byte(muic_data->i2c, RT8973_REG_INT1);
	st2 = rt8973_i2c_read_byte(muic_data->i2c, RT8973_REG_INT2);
	mutex_unlock(&muic_data->muic_mutex);

	pr_info("%s:%s: st1:0x%x st2:0x%x buf%s\n", MUIC_DEV_NAME, __func__, st1, st2, buf);

	if (st1 < 0 || st2 < 0) {
		pr_err("%s:%s: fail to read muic reg\n", MUIC_DEV_NAME, __func__);
		return sprintf(buf, "UNKNOWN\n");
	}
	return sprintf(buf, "st1:0x%x st2:0x%x\n", st1, st2);
}

static ssize_t rt8973_muic_show_registers(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct rt8973_muic_data *muic_data = dev_get_drvdata(dev);
	char mesg[256] = "";

	mutex_lock(&muic_data->muic_mutex);
	rt8973_read_reg_dump(muic_data, mesg);
	mutex_unlock(&muic_data->muic_mutex);
	pr_info("%s:%s: %s\n", MUIC_DEV_NAME, __func__, mesg);

	return sprintf(buf, "%s\n", mesg);
}
#endif

#if defined(CONFIG_USB_HOST_NOTIFY)
static ssize_t rt8973_muic_show_otg_test(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct rt8973_muic_data *muic_data = dev_get_drvdata(dev);
	int ret;
	u8 val = 0;

	mutex_lock(&muic_data->muic_mutex);
	ret = rt8973_i2c_read_byte(muic_data->i2c, RT8973_REG_INT_MASK2);
	mutex_unlock(&muic_data->muic_mutex);

	if (ret < 0) {
		pr_err("%s:%s: fail to read muic reg\n", MUIC_DEV_NAME, __func__);
		return sprintf(buf, "UNKNOWN\n");
	}

	pr_info("%s:%s: ret:%d val:%x buf%s\n", MUIC_DEV_NAME, __func__, ret, val, buf);

	val &= INT_CHG_DET_MASK;
	return sprintf(buf, "%x\n", val);
}

static ssize_t rt8973_muic_set_otg_test(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct rt8973_muic_data *muic_data = dev_get_drvdata(dev);

	pr_info("%s:%s: buf:%s\n", MUIC_DEV_NAME, __func__, buf);

	/*
	*	The otg_test is set 0 durring the otg test. Not 1 !!!
	*/

	if (!strncmp(buf, "0", 1))
		muic_data->is_otg_test = true;
	else if (!strncmp(buf, "1", 1))
		muic_data->is_otg_test = false;
	else {
		pr_info("%s:%s: Wrong command\n", MUIC_DEV_NAME, __func__);
		return count;
	}

	return count;
}
#endif

static ssize_t rt8973_muic_show_attached_dev(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct rt8973_muic_data *muic_data = dev_get_drvdata(dev);

	pr_info("%s:%s: attached_dev[%d]\n", MUIC_DEV_NAME, __func__, muic_data->attached_dev);

	switch (muic_data->attached_dev) {
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
	case ATTACHED_DEV_DESKDOCK_VB_MUIC:
		return sprintf(buf, "DESKDOCK\n");
	case ATTACHED_DEV_CHARGING_CABLE_MUIC:
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

	pr_info("%s:%s: mode status[%s]\n", MUIC_DEV_NAME, __func__, mode);

	return sprintf(buf, "%s\n", mode);
}

static ssize_t rt8973_muic_set_apo_factory(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct rt8973_muic_data *muic_data = dev_get_drvdata(dev);
	const char *mode;

	pr_info("%s:%s: buf:%s\n", MUIC_DEV_NAME, __func__, buf);

	/* "FACTORY_START": factory mode */
	if (!strncmp(buf, "FACTORY_START", 13)) {
		muic_data->is_factory_start = true;
		mode = "FACTORY_MODE";
	} else {
		pr_info( "%s:%s: Wrong command\n", MUIC_DEV_NAME, __func__);
		return count;
	}

	return count;
}

static DEVICE_ATTR(uart_en, 0664, rt8973_muic_show_uart_en,
					rt8973_muic_set_uart_en);
static DEVICE_ATTR(usb_en, 0664, rt8973_muic_show_usb_en,
					rt8973_muic_set_usb_en);
static DEVICE_ATTR(adc, 0664, rt8973_muic_show_adc, NULL);
static DEVICE_ATTR(usb_state, 0664, rt8973_muic_show_usb_state, NULL);
static DEVICE_ATTR(attached_dev, 0664, rt8973_muic_show_attached_dev, NULL);
#if defined(CONFIG_USB_HOST_NOTIFY)
static DEVICE_ATTR(otg_test, 0664, rt8973_muic_show_otg_test,
					rt8973_muic_set_otg_test);
#endif
static DEVICE_ATTR(apo_factory, 0664, rt8973_muic_show_apo_factory,
					rt8973_muic_set_apo_factory);
#ifdef DEBUG_MUIC
static DEVICE_ATTR(mansw1, 0664, rt8973_muic_show_mansw1, NULL);
static DEVICE_ATTR(mansw2, 0664, rt8973_muic_show_mansw2, NULL);
static DEVICE_ATTR(dump_registers, 0664, rt8973_muic_show_registers, NULL);
static DEVICE_ATTR(int_status, 0664, rt8973_muic_show_interrupt_status, NULL);
#endif

static struct attribute *rt8973_muic_attributes[] = {
	&dev_attr_uart_en.attr,
	&dev_attr_usb_en.attr,
	&dev_attr_adc.attr,
	&dev_attr_usb_state.attr,
	&dev_attr_attached_dev.attr,
#if defined(CONFIG_USB_HOST_NOTIFY)
	&dev_attr_otg_test.attr,
#endif
	&dev_attr_apo_factory.attr,
#ifdef DEBUG_MUIC
	&dev_attr_mansw1.attr,
	&dev_attr_mansw2.attr,
	&dev_attr_dump_registers.attr,
	&dev_attr_int_status.attr,
#endif
	NULL
};

static const struct attribute_group rt8973_muic_group = {
	.attrs = rt8973_muic_attributes,
};


static int set_ctrl_reg(struct rt8973_muic_data *muic_data, int shift, bool on)
{
	struct i2c_client *i2c = muic_data->i2c;
	u8 reg_val;
	int ret = 0;

	ret = rt8973_i2c_read_byte(i2c, RT8973_REG_CONTROL);
	if (ret < 0)
		pr_err("%s:%s: err read CTRL(%d)\n", MUIC_DEV_NAME, __func__, ret);

	if (on)
		reg_val = ret | (0x1 << shift);
	else
		reg_val = ret & ~(0x1 << shift);

	if (reg_val ^ ret) {
		pr_info("%s:%s: 0x%x != 0x%x, update\n", MUIC_DEV_NAME, __func__, reg_val, ret);

		ret = rt8973_i2c_write_byte(i2c, RT8973_REG_CONTROL, reg_val);
		if (ret < 0)
			pr_err("%s:%s: err write(%d)\n", MUIC_DEV_NAME, __func__, ret);
	} else {
		pr_info("%s:%s: 0x%x == 0x%x, just return\n",
			MUIC_DEV_NAME, __func__, reg_val, ret);
		return 0;
	}

	ret = rt8973_i2c_read_byte(i2c, RT8973_REG_CONTROL);
	if (ret < 0)
		pr_err("%s:%s: err read CTRL(%d)\n", MUIC_DEV_NAME, __func__, ret);
	else
		pr_info("%s:%s: after change(0x%x)\n", MUIC_DEV_NAME, __func__, ret);

	return ret;
}

static int set_com_sw(struct rt8973_muic_data *muic_data,
			enum rt8973_reg_manual_sw1_value reg_val)
{
	struct i2c_client *i2c = muic_data->i2c;
	int ret = 0;
	int temp = 0;

	/*  --- MANSW [7:5][4:2][1:0] : DM DP RSVD --- */
	temp = rt8973_i2c_read_byte(i2c, RT8973_REG_MANUAL_SW1);
	if (temp < 0)
		pr_err("%s:%s: err read MANSW(0x%x)\n", MUIC_DEV_NAME, __func__, temp);

	if ((reg_val & MANUAL_SW_DM_DP_MASK) != (temp & MANUAL_SW_DM_DP_MASK)) {
		pr_info("%s:%s: 0x%x != 0x%x, update\n", MUIC_DEV_NAME, __func__,
			(reg_val & MANUAL_SW_DM_DP_MASK), (temp & MANUAL_SW_DM_DP_MASK));

		ret = rt8973_i2c_write_byte(i2c,
			RT8973_REG_MANUAL_SW1, (reg_val & MANUAL_SW_DM_DP_MASK));
		if (ret < 0)
			pr_err("%s:%s: err write MANSW(0x%x)\n", MUIC_DEV_NAME, __func__,
				(reg_val & MANUAL_SW_DM_DP_MASK));

	} else {
		pr_info("%s:%s: MANSW reg(0x%x), just pass\n", MUIC_DEV_NAME, __func__, reg_val);
	}

	return ret;
}

static int com_to_open(struct rt8973_muic_data *muic_data)
{
	enum rt8973_reg_manual_sw1_value reg_val;
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	reg_val = MANSW1_OPEN;
	ret = set_com_sw(muic_data, reg_val);
	if (ret)
		pr_err("%s:%s: set_com_sw err\n", MUIC_DEV_NAME, __func__);

	return ret;
}

static int com_to_usb(struct rt8973_muic_data *muic_data)
{
	enum rt8973_reg_manual_sw1_value reg_val;
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);
	reg_val = MANSW1_USB;
	ret = set_com_sw(muic_data, reg_val);
	if (ret)
		pr_err("%s:%s: set_com_usb err\n", MUIC_DEV_NAME, __func__);

	return ret;
}

static int com_to_uart(struct rt8973_muic_data *muic_data)
{
	enum rt8973_reg_manual_sw1_value reg_val;
	int ret = 0;

	pr_info("%s:%s: rustproof mode[%d]\n", MUIC_DEV_NAME, __func__, muic_data->is_rustproof);

	if (muic_data->is_rustproof)
		return ret;

	reg_val = MANSW1_UART;
	ret = set_com_sw(muic_data, reg_val);
	if (ret)
		pr_err("%s:%s: set_com_uart err\n", MUIC_DEV_NAME, __func__);

	return ret;
}

static int switch_to_uart(struct rt8973_muic_data *muic_data, int uart_status)
{
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	if (muic_data->pdata->gpio_uart_sel)
		muic_data->pdata->set_gpio_uart_sel(uart_status);

	ret = com_to_uart(muic_data);
	if (ret) {
		pr_err("%s:%s: com->uart set err\n", MUIC_DEV_NAME, __func__);
		return ret;
	}

	return ret;
}

static int attach_usb(struct rt8973_muic_data *muic_data)
{
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);
	ret = com_to_usb(muic_data);

	return ret;
}

static int attach_jig_uart_boot_off(struct rt8973_muic_data *muic_data)
{
	struct muic_platform_data *pdata = muic_data->pdata;
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	if (pdata->uart_path == MUIC_PATH_UART_AP)
		ret = switch_to_uart(muic_data, MUIC_PATH_UART_AP);
	else
		ret = switch_to_uart(muic_data, MUIC_PATH_UART_CP);

	return ret;
}

static int attach_jig_uart_boot_on(struct rt8973_muic_data *muic_data)
{
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	ret = set_com_sw(muic_data, MANSW1_OPEN);
	if (ret)
		pr_err( "%s:%s: set_com_sw err\n", MUIC_DEV_NAME, __func__);

	return ret;
}

static int attach_jig_usb_boot_on_off(struct rt8973_muic_data *muic_data)
{
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	ret = attach_usb(muic_data);
	if (ret)
		return ret;

	return ret;
}

static void rt8973_check_reg(struct rt8973_muic_data *muic_data)
{
	struct i2c_client *i2c = muic_data->i2c;
	int ctrl = 0, mansw2 = 0;

	ctrl = rt8973_i2c_read_byte(i2c, RT8973_REG_CONTROL);
	mansw2 = rt8973_i2c_read_byte(i2c, RT8973_REG_MANUAL_SW1);

	pr_info("%s:%s:  ctrl:0x%02x mansw2:0x%02x\n", MUIC_DEV_NAME, __func__, ctrl, mansw2);
}

static void rt8973_set_manual_mode(struct rt8973_muic_data *muic_data, int en)
{
	struct i2c_client *i2c = muic_data->i2c;
	int reg = 0;
	int ret = 0;

	reg = rt8973_i2c_read_byte(i2c, RT8973_REG_CONTROL);
	if (en) /* manual mode */
		reg &= ~CTRL_AUTO_CONFIG_MASK;
	else /* auto mode */
		reg |= CTRL_AUTO_CONFIG_MASK;

	ret = rt8973_i2c_write_byte(i2c, RT8973_REG_CONTROL, reg);
	if (ret < 0)
		pr_err( "%s:%s: fail to update reg\n", MUIC_DEV_NAME, __func__);
}

int rt8973_muic_set_path(void *drv_data, int path)
{
	struct rt8973_muic_data *muic_data = (struct rt8973_muic_data *)drv_data;
	int ret = 0;

	pr_info("%s:%s path : %d\n", MUIC_DEV_NAME, __func__, path);
	rt8973_check_reg(muic_data);

	if (path) { /* UART */
		rt8973_set_manual_mode(muic_data, 0);
		ret = com_to_uart(muic_data); /*FIXME: it needs not? */
	} else { /* USB */
		rt8973_set_manual_mode(muic_data, 1);
		ret = com_to_usb(muic_data);
	}

	/* FIXME : check register */
	rt8973_check_reg(muic_data);
	return ret;
}

static void rt8973_muic_handle_attach(struct rt8973_muic_data *muic_data,
			muic_attached_dev_t new_dev, int adc, u8 vbvolt)
{
	int ret = 0;
	bool noti = (new_dev != muic_data->attached_dev) ? true : false;

	pr_info("%s:%s: muic_data->attached_dev: %d, new_dev: %d, muic_data->suspended: %d\n",
		MUIC_DEV_NAME, __func__, muic_data->attached_dev, new_dev, muic_data->suspended);

	if (new_dev == muic_data->attached_dev) {
		pr_info("%s:%s: Attach duplicated\n", MUIC_DEV_NAME, __func__);
		return;
	}

	/* Logically Detach Accessary */
	switch (muic_data->attached_dev) {
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_CDP_MUIC:
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
	case ATTACHED_DEV_OTG_MUIC:
	case ATTACHED_DEV_CHARGING_CABLE_MUIC:
	case ATTACHED_DEV_TA_MUIC:
	case ATTACHED_DEV_UNDEFINED_CHARGING_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
		rt8973_muic_handle_detach(muic_data);
                break;
	case ATTACHED_DEV_DESKDOCK_MUIC:
	case ATTACHED_DEV_DESKDOCK_VB_MUIC:
		switch (new_dev) {
		case ATTACHED_DEV_DESKDOCK_MUIC:
		case ATTACHED_DEV_DESKDOCK_VB_MUIC:
			break;
		default:
			rt8973_muic_handle_detach(muic_data);
			break;
		}
		break;
	default:
		break;
	}
	pr_info("%s:%s: new(%d)!=attached(%d)\n", MUIC_DEV_NAME, __func__,
							new_dev, muic_data->attached_dev);

	/* Attach Accessary */
	noti = true;
	switch (new_dev) {
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_CDP_MUIC:
	case ATTACHED_DEV_OTG_MUIC:
		ret = attach_usb(muic_data);
		break;
	case ATTACHED_DEV_CHARGING_CABLE_MUIC:
	case ATTACHED_DEV_TA_MUIC:
	case ATTACHED_DEV_UNDEFINED_CHARGING_MUIC:
	case ATTACHED_DEV_UNKNOWN_MUIC:
		com_to_open(muic_data);
		break;
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
		ret = attach_jig_uart_boot_off(muic_data);
		break;
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
		ret = attach_jig_uart_boot_on(muic_data);
		break;
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
		ret = attach_jig_usb_boot_on_off(muic_data);
		break;
	case ATTACHED_DEV_DESKDOCK_MUIC:
	case ATTACHED_DEV_DESKDOCK_VB_MUIC:
		break;
	default:
		noti = false;
		pr_info("%s:%s: unsupported dev=%d, adc=0x%x, vbus=%c\n",
				MUIC_DEV_NAME, __func__, new_dev, adc, (vbvolt ? 'O' : 'X'));
		break;
	}

	if (ret)
		pr_err("%s:%s: something wrong %d (ERR=%d)\n", MUIC_DEV_NAME, __func__, new_dev, ret);

#if defined(CONFIG_MUIC_NOTIFIER)
	if (noti) {
		if (!muic_data->suspended)
			muic_notifier_attach_attached_dev(new_dev);
		else
			muic_data->need_to_noti = true;
	}
#endif /* CONFIG_MUIC_NOTIFIER */

	muic_data->attached_dev = new_dev;
}

static void rt8973_muic_handle_detach(struct rt8973_muic_data *muic_data)
{
	int ret = 0;
	bool noti = true;

	if (muic_data->attached_dev == ATTACHED_DEV_NONE_MUIC) {
		pr_info("%s:%s: Detach duplicated(NONE)\n", MUIC_DEV_NAME, __func__);
		goto out_without_noti;
	}

	pr_info("%s:%s: Detach device[%d]\n", MUIC_DEV_NAME, __func__, muic_data->attached_dev);

#if defined(CONFIG_MUIC_NOTIFIER)
	if (noti) {
		if (!muic_data->suspended)
			muic_notifier_detach_attached_dev(muic_data->attached_dev);
		else
			muic_data->need_to_noti = true;
	}
#endif /* CONFIG_MUIC_NOTIFIER */

out_without_noti:
	ret = com_to_open(muic_data);
	muic_data->attached_dev = ATTACHED_DEV_NONE_MUIC;
}

static void rt8973_muic_detect_dev(struct rt8973_muic_data *muic_data)
{
	struct i2c_client *i2c = muic_data->i2c;
	muic_attached_dev_t new_dev = ATTACHED_DEV_UNKNOWN_MUIC;
	int vbvolt = 0;
	int id = 0, ctrl = 0, mansw_val1 = 0, mansw_val2 = 0;
	int dev1 = 0, dev2 = 0, adc = 0, int2 = 0;

	id = rt8973_i2c_read_byte(i2c, RT8973_REG_CHIP_ID);
	ctrl = rt8973_i2c_read_byte(i2c, RT8973_REG_CONTROL);
	mansw_val1 = rt8973_i2c_read_byte(i2c, RT8973_REG_MANUAL_SW1);
	mansw_val2 = rt8973_i2c_read_byte(i2c, RT8973_REG_MANUAL_SW2);
	dev1 = rt8973_i2c_read_byte(i2c, RT8973_REG_DEVICE1);
	dev2 = rt8973_i2c_read_byte(i2c, RT8973_REG_DEVICE2);
	adc = rt8973_i2c_read_byte(i2c, RT8973_REG_ADC);
	int2 = rt8973_i2c_read_byte(i2c, RT8973_REG_INT2);

	vbvolt = !(int2 & INT_UVLO_MASK);

	pr_info("%s:%s: id:0x%x, ctrl:0x%x, mansw1:0x%x, mansw2:0x%x\n", MUIC_DEV_NAME, __func__,
									id, ctrl, mansw_val1,mansw_val2);
	pr_info("%s:%s: dev[1:0x%x, 2:0x%x], adc:0x%x, vbvolt:0x%x\n", MUIC_DEV_NAME, __func__,
									dev1, dev2, adc, vbvolt);

	/* Detected */
	switch (dev1) {
	case RT8973_DEVICE1_OTG:
		new_dev = ATTACHED_DEV_OTG_MUIC;
		pr_info("%s:%s: USB_OTG DETECTED\n", MUIC_DEV_NAME, __func__);
		break;
	case RT8973_DEVICE1_SDP:
		new_dev = ATTACHED_DEV_USB_MUIC;
		pr_info("%s:%s: USB DETECTED\n", MUIC_DEV_NAME, __func__);
		break;
	case RT8973_DEVICE1_UART:
		break;
	case RT8973_DEVICE1_CDPORT:
		new_dev = ATTACHED_DEV_CDP_MUIC;
		pr_info("%s:%s: USB_CDP DETECTED\n", MUIC_DEV_NAME, __func__);
		break;
	case RT8973_DEVICE1_DCPORT:
		new_dev = ATTACHED_DEV_TA_MUIC;
		pr_info("%s:%s:DEDICATED CHARGER DETECTED\n", MUIC_DEV_NAME, __func__);
		break;
	default:
		break;
	}

	switch (dev2) {
	case RT8973_DEVICE2_JIG_USB_ON:
		if (!vbvolt)
			break;
		new_dev = ATTACHED_DEV_JIG_USB_ON_MUIC;
		pr_info("%s:%s: JIG_USB_ON DETECTED\n", MUIC_DEV_NAME, __func__);
		break;
	case RT8973_DEVICE2_JIG_USB_OFF:
		if (!vbvolt)
			break;
		new_dev = ATTACHED_DEV_JIG_USB_OFF_MUIC;
		pr_info("%s:%s: JIG_USB_OFF DETECTED\n", MUIC_DEV_NAME, __func__);
		break;
	case RT8973_DEVICE2_JIG_UART_ON:
		if (new_dev != ATTACHED_DEV_JIG_UART_ON_MUIC) {
			new_dev = ATTACHED_DEV_JIG_UART_ON_MUIC;
			pr_info("%s:%s: JIG_UART_ON DETECTED\n", MUIC_DEV_NAME, __func__);
		}
		break;
	case RT8973_DEVICE2_JIG_UART_OFF:
		if (vbvolt) {
			new_dev = ATTACHED_DEV_JIG_UART_OFF_VB_MUIC;
			pr_info("%s:%s: JIG_UART_OFF_VB DETECTED\n", MUIC_DEV_NAME, __func__);
		}
		else {
			new_dev = ATTACHED_DEV_JIG_UART_OFF_MUIC;
			pr_info("%s:%s: JIG_UART_OFF DETECTED\n", MUIC_DEV_NAME, __func__);
		}
		break;
	case RT8973_DEVICE2_UNKNOWN:
		pr_info("%s:%s: UNKNOWN DEVICE DETECTED\n", MUIC_DEV_NAME, __func__);
		break;
	default:
		break;
	}

	/* If there is no matching device found using device type registers
		use ADC to find the attached device */
	if (new_dev == ATTACHED_DEV_UNKNOWN_MUIC) {
		switch (adc) {
		/* This is workaround for LG USB cable which has 219k ohm ID */
		case ADC_CEA936ATYPE1_CHG:
		case ADC_JIG_USB_OFF:
			if (vbvolt) {
				new_dev = ATTACHED_DEV_USB_MUIC;
				pr_info("%s:%s: TYPE1_CHARGER or X_CABLE DETECTED (USB)\n", MUIC_DEV_NAME, __func__);
			}
			break;
		case ADC_JIG_USB_ON:
			if (!vbvolt)
				break;
			new_dev = ATTACHED_DEV_JIG_USB_ON_MUIC;
			pr_info("%s:%s: JIG_USB_ON DETECTED\n", MUIC_DEV_NAME, __func__);
			break;
		case ADC_CEA936ATYPE2_CHG:
			if (vbvolt) {
				new_dev = ATTACHED_DEV_TA_MUIC;
				pr_info("%s:%s: TYPE2_CHARGER DETECTED\n", MUIC_DEV_NAME, __func__);
			}
			break;
		case ADC_JIG_UART_OFF:
			if (vbvolt) {
				new_dev = ATTACHED_DEV_JIG_UART_OFF_VB_MUIC;
				pr_info("%s:%s: JIG_UART_OFF_VB DETECTED\n", MUIC_DEV_NAME, __func__);
			}
			else {
				new_dev = ATTACHED_DEV_JIG_UART_OFF_MUIC;
				pr_info("%s:%s: JIG_UART_OFF DETECTED\n", MUIC_DEV_NAME, __func__);
			}
			break;
		case ADC_JIG_UART_ON:
			new_dev = ATTACHED_DEV_JIG_UART_ON_MUIC;
			pr_info("%s:%s: JIG_UART_ON DETECTED\n", MUIC_DEV_NAME, __func__);
			break;
		case ADC_HMT:
		case ADC_USB_LANHUB:
		case ADC_CHARGING_CABLE:
		case ADC_DESKDOCK:
			if (vbvolt) {
				new_dev = ATTACHED_DEV_UNSUPPORTED_ID_VB_MUIC;
				pr_info("%s:%s: UNSUPPORT CHARGING (0x%02x)\n", MUIC_DEV_NAME, __func__, adc);
			}
			break;
		default:
			break;
		}
	}

	if ((new_dev == ATTACHED_DEV_UNKNOWN_MUIC) && (ADC_OPEN != adc)) {
		if (vbvolt) {
			new_dev = ATTACHED_DEV_UNDEFINED_CHARGING_MUIC;
			pr_info("%s:%s: UNDEFINED CHARGING (0x%02x)\n", MUIC_DEV_NAME, __func__, adc);
		}
	}

	if (new_dev != ATTACHED_DEV_UNKNOWN_MUIC) {
		pr_info("%s:%s ATTACHED\n", MUIC_DEV_NAME, __func__);
		rt8973_muic_handle_attach(muic_data, new_dev, adc, vbvolt);
	}
	else {
		pr_info("%s:%s DETACHED\n", MUIC_DEV_NAME, __func__);
		rt8973_muic_handle_detach(muic_data);
	}

#if defined(CONFIG_VBUS_NOTIFIER)
	vbus_notifier_handle((!!vbvolt) ? STATUS_VBUS_HIGH : STATUS_VBUS_LOW);
#endif /* CONFIG_VBUS_NOTIFIER */

}

static int rt8973_muic_reg_init(struct rt8973_muic_data *muic_data)
{
	struct i2c_client *i2c = muic_data->i2c;
	int ret;
	int dev1, dev2, mansw_val1, adc, ctrl;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	dev1 = rt8973_i2c_read_byte(i2c, RT8973_REG_DEVICE1);
	dev2 = rt8973_i2c_read_byte(i2c, RT8973_REG_DEVICE2);
	mansw_val1 = rt8973_i2c_read_byte(i2c, RT8973_REG_MANUAL_SW1);
	adc = rt8973_i2c_read_byte(i2c, RT8973_REG_ADC);
	pr_info("%s:%s: dev[1:0x%x, 2:0x%x], mansw1:0x%x, adc:0x%x\n", MUIC_DEV_NAME, __func__,
									dev1, dev2, mansw_val1, adc);

	if ((dev1 & RT8973_USB_TYPES) ||
				(dev2 & RT8973_JIG_USB_TYPES)) {
		ret = rt8973_i2c_write_byte(i2c, RT8973_REG_MANUAL_SW1, MANSW1_USB);
		if (ret < 0)
			pr_err("%s:%s usb type detect err %d\n", MUIC_DEV_NAME, __func__, ret);
	} else if (dev2 & RT8973_JIG_UART_TYPES) {
		ret = rt8973_i2c_write_byte(i2c, RT8973_REG_MANUAL_SW1, MANSW1_UART);
		if (ret < 0)
			pr_err("%s:%s uart type detect err %d\n", MUIC_DEV_NAME, __func__, ret);
	}

	ret = rt8973_i2c_write_byte(i2c, RT8973_REG_CONTROL, CTRL_MASK);
	if (ret < 0)
		pr_err( "%s:%s: failed to write ctrl(%d)\n", MUIC_DEV_NAME, __func__, ret);

	ctrl = rt8973_i2c_read_byte(i2c, RT8973_REG_CONTROL);
	pr_info("%s:%s: CTRL:0x%02x\n", MUIC_DEV_NAME, __func__, ctrl);

	return ret;
}

static irqreturn_t rt8973_muic_irq_thread(int irq, void *data)
{
	struct rt8973_muic_data *muic_data = data;
//	struct i2c_client *i2c = muic_data->i2c;
//	enum rt8973_reg_manual_sw1_value reg_val;
//	int ctrl, ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	mutex_lock(&muic_data->muic_mutex);
	wake_lock(&muic_data->muic_wake_lock);

	/* check for muic reset and re-initialize registers */
#if 0
	ctrl = rt8973_i2c_read_byte(i2c, S2MU005_REG_MUIC_CTRL1);

	if (ctrl == 0xDF) {
		/* CONTROL register is reset to DF */
#ifdef DEBUG_MUIC
		rt8973_print_reg_log();
		rt8973_print_reg_dump(muic_data);
#endif
		pr_err("%s:%s: err muic could have been reseted. Initilize!!\n",
						MUIC_DEV_NAME, __func__);
		rt8973_muic_reg_init(muic_data);
		if (muic_data->is_rustproof) {
			pr_info("%s:%s: rustproof is enabled\n", MUIC_DEV_NAME, __func__);
			reg_val = MANSW1_OPEN;
			ret = rt8973_i2c_write_byte(i2c,
				RT8973_REG_MANUAL_SW1, reg_val);
			if (ret < 0)
				pr_err("%s:%s: err write MANSW\n", MUIC_DEV_NAME, __func__);
			ret = rt8973_i2c_read_byte(i2c, RT8973_REG_MANUAL_SW1);
			pr_info("%s:%s: MUIC_SW_CTRL=0x%x\n ", MUIC_DEV_NAME, __func__, ret);
		} else {
			reg_val = MANSW1_UART;
			ret = rt8973_i2c_write_byte(i2c,
				RT8973_REG_MANUAL_SW1, reg_val);
			if (ret < 0)
				pr_err("%s:%s: err write MANSW\n", MUIC_DEV_NAME, __func__);
			ret = rt8973_i2c_read_byte(i2c, RT8973_REG_MANUAL_SW1);
			pr_info("%s:%s: MUIC_SW_CTRL=0x%x\n ", MUIC_DEV_NAME, __func__, ret);
		}
#ifdef DEBUG_MUIC
		rt8973_print_reg_dump(muic_data);
#endif
		/* MUIC Interrupt On */
		ret = set_ctrl_reg(muic_data, CTRL_INT_MASK_SHIFT, false);
	}
#endif

	/* device detection */
	rt8973_muic_detect_dev(muic_data);

	wake_unlock(&muic_data->muic_wake_lock);
	mutex_unlock(&muic_data->muic_mutex);

	return IRQ_HANDLED;
}

static int rt8973_init_rev_info(struct rt8973_muic_data *muic_data)
{
//	u8 dev_id;
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

#if 0
	dev_id = rt8973_i2c_read_byte(muic_data->i2c, RT8973_REG_CHIP_ID);
	if (dev_id < 0) {
		pr_err( "%s:%s: dev_id(%d)\n", MUIC_DEV_NAME, __func__, dev_id);
		ret = -ENODEV;
	} else {
		muic_data->muic_vendor = 0x05;
		muic_data->muic_version = (dev_id & 0x0F);
		pr_info( "%s:%s: vendor=0x%x, ver=0x%x, dev_id=0x%x\n", MUIC_DEV_NAME, __func__,
					muic_data->muic_vendor, muic_data->muic_version, dev_id);
	}
#endif

	return ret;
}

static int rt8973_muic_irq_init(struct rt8973_muic_data *muic_data)
{
	struct i2c_client *i2c = muic_data->i2c;
	struct muic_platform_data *pdata = muic_data->pdata;
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	if (!pdata->irq_gpio) {
		pr_err("%s:%s: No interrupt specified\n", MUIC_DEV_NAME, __func__);
		return -ENXIO;
	}

	i2c->irq = gpio_to_irq(pdata->irq_gpio);
	if (i2c->irq) {
		ret = request_threaded_irq(i2c->irq, NULL,
				rt8973_muic_irq_thread,
				(IRQF_TRIGGER_FALLING |
				IRQF_NO_SUSPEND | IRQF_ONESHOT),
				"rt8973-muic", muic_data);
		if (ret < 0) {
			pr_err("%s:%s: failed to request IRQ(%d)\n",
						MUIC_DEV_NAME, __func__, i2c->irq);
			return ret;
		}

		ret = enable_irq_wake(i2c->irq);
		if (ret < 0)
			pr_err("%s:%s: failed to enable wakeup src\n", MUIC_DEV_NAME, __func__);
	}

	return ret;
}

#if defined(CONFIG_OF)
static int of_rt8973_muic_dt(struct device *dev, struct rt8973_muic_data *muic_data)
{
	struct device_node *np_muic = dev->of_node;
	int ret = 0;

	if (np_muic == NULL) {
		pr_err("%s:%s: np NULL\n", MUIC_DEV_NAME, __func__);
		return -EINVAL;
	} else {
		muic_data->pdata->irq_gpio =
			of_get_named_gpio(np_muic, "rt8973,irq-gpio", 0);
		if (muic_data->pdata->irq_gpio < 0) {
			pr_err("%s:%s: failed to get muic_irq = %d\n",
					MUIC_DEV_NAME, __func__, muic_data->pdata->irq_gpio);
			muic_data->pdata->irq_gpio = 0;
		}
		if (of_gpio_count(np_muic) < 1) {
			pr_err("%s:%s: could not find muic gpio\n", MUIC_DEV_NAME, __func__);
			muic_data->pdata->gpio_uart_sel = 0;
		} else
			muic_data->pdata->gpio_uart_sel = of_get_gpio(np_muic, 0);
	}

	return ret;
}
#endif /* CONFIG_OF */

static int rt8973_muic_probe(struct i2c_client *i2c,
				const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(i2c->dev.parent);
	struct rt8973_muic_data *muic_data;
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		pr_err("%s:%s: i2c functionality check error\n", MUIC_DEV_NAME, __func__);
		ret = -EIO;
		goto err_return;
	}

	muic_data = kzalloc(sizeof(struct rt8973_muic_data), GFP_KERNEL);
	if (!muic_data) {
		pr_err("%s:%s: failed to allocate driver data\n", MUIC_DEV_NAME, __func__);
		ret = -ENOMEM;
		goto err_return;
	}

	/* save platfom data for gpio control functions */
	muic_data->dev = &i2c->dev;
	muic_data->i2c = i2c;
	muic_data->pdata = &muic_pdata;

#if defined(CONFIG_OF)
	ret = of_rt8973_muic_dt(&i2c->dev, muic_data);
	if (ret < 0)
		pr_err("%s:%s: no muic dt! ret[%d]\n", MUIC_DEV_NAME, __func__, ret);
#endif /* CONFIG_OF */

	i2c_set_clientdata(i2c, muic_data);

//	if (muic_pdata.gpio_uart_sel)
//		muic_pdata.set_gpio_uart_sel = rt8973_set_gpio_uart_sel;

	if (muic_data->pdata->init_gpio_cb)
		ret = muic_data->pdata->init_gpio_cb();
	if (ret) {
		pr_err("%s:%s: failed to init gpio(%d)\n", MUIC_DEV_NAME, __func__, ret);
		goto fail_init_gpio;
	}

	mutex_init(&muic_data->muic_mutex);

	muic_data->is_factory_start = false;
	muic_data->attached_dev = ATTACHED_DEV_UNKNOWN_MUIC;
	muic_data->is_usb_ready = false;

#ifdef CONFIG_SEC_SYSFS
	/* create sysfs group */
	ret = sysfs_create_group(&switch_device->kobj, &rt8973_muic_group);
	if (ret) {
		pr_err("%s:%s: failed to create sysfs\n", MUIC_DEV_NAME, __func__);
		goto fail;
	}
	dev_set_drvdata(switch_device, muic_data);
#endif

	ret = rt8973_init_rev_info(muic_data);
	if (ret) {
		pr_err("%s:%s: failed to init rev_info\n", MUIC_DEV_NAME, __func__);
		goto fail;
	}

	ret = rt8973_muic_reg_init(muic_data);
	if (ret) {
		pr_err("%s:%s: failed to init reg\n", MUIC_DEV_NAME, __func__);
		goto fail;
	}

	muic_data->is_rustproof = muic_data->pdata->rustproof_on;
	muic_data->pdata->muic_set_path = rt8973_muic_set_path;

	if (muic_data->pdata->init_switch_dev_cb)
		muic_data->pdata->init_switch_dev_cb();

	ret = rt8973_muic_irq_init(muic_data);
	if (ret) {
		pr_err("%s:%s: failed to init irq(%d)\n", MUIC_DEV_NAME, __func__, ret);
		goto fail_init_irq;
	}

	wake_lock_init(&muic_data->muic_wake_lock, WAKE_LOCK_SUSPEND, "muic_wake");
//	muic_data->muic_wqueue =
//		create_singlethread_workqueue(dev_name(muic_data->dev));

//	INIT_DELAYED_WORK(&muic_data->usb_work, rt8973_muic_usb_check);

	/* initial cable detection */
        ret = set_ctrl_reg(muic_data, CTRL_INT_MASK_SHIFT, false);
	rt8973_muic_irq_thread(-1, muic_data);

	return 0;

fail_init_irq:
	if (i2c->irq)
		free_irq(i2c->irq, muic_data);
fail:
#ifdef CONFIG_SEC_SYSFS
	sysfs_remove_group(&switch_device->kobj, &rt8973_muic_group);
#endif
	mutex_destroy(&muic_data->muic_mutex);
fail_init_gpio:
	i2c_set_clientdata(i2c, NULL);
	kfree(muic_data);
err_return:
	return ret;
}

static int rt8973_muic_remove(struct i2c_client *i2c)
{
	struct rt8973_muic_data *muic_data = i2c_get_clientdata(i2c);
#ifdef CONFIG_SEC_SYSFS
	sysfs_remove_group(&switch_device->kobj, &rt8973_muic_group);
#endif

	if (muic_data) {
		pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);
//		cancel_delayed_work(&muic_data->usb_work);
		disable_irq_wake(muic_data->i2c->irq);
		free_irq(muic_data->i2c->irq, muic_data);
		mutex_destroy(&muic_data->muic_mutex);
		i2c_set_clientdata(muic_data->i2c, NULL);
		kfree(muic_data);
	}
	return 0;
}

static const struct i2c_device_id rt8973_i2c_id[] = {
	{ "rt8973", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, rt8973_i2c_id);

static void rt8973_muic_shutdown(struct i2c_client *i2c)
{
	struct rt8973_muic_data *muic_data = i2c_get_clientdata(i2c);
	int ret;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);
	if (!muic_data->i2c) {
		pr_err("%s:%s: no muic i2c client\n", MUIC_DEV_NAME, __func__);
		return;
	}

	pr_info("%s:%s: open D+,D-,V_bus line\n", MUIC_DEV_NAME, __func__);
	ret = com_to_open(muic_data);
	if (ret < 0)
		pr_err("%s:%s: fail to open mansw1\n", MUIC_DEV_NAME, __func__);

	/* set auto sw mode before shutdown to make sure device goes into */
	/* LPM charging when TA or USB is connected during off state */
	pr_info("%s:%s: muic auto detection enable\n", MUIC_DEV_NAME, __func__);
	ret = set_ctrl_reg(muic_data, CTRL_INT_MASK_SHIFT, true);
	if (ret < 0) {
		pr_err( "%s:%s: fail to update reg\n", MUIC_DEV_NAME, __func__);
		return;
	}
}

#if 0
static int rt8973_muic_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct rt8973_muic_data *muic_data = i2c_get_clientdata(client);

	muic_data->suspended = true;

	return 0;
}

static int rt8973_muic_resume(struct i2c_client *client)
{
	struct rt8973_muic_data *muic_data = i2c_get_clientdata(client);

	muic_data->suspended = false;

	if (muic_data->need_to_noti) {
		if (muic_data->attached_dev)
			muic_notifier_attach_attached_dev(muic_data->attached_dev);
		else
			muic_notifier_detach_attached_dev(muic_data->attached_dev);
		muic_data->need_to_noti = false;
	}

	return 0;
}
#endif

#if defined(CONFIG_OF)
static struct of_device_id sec_muic_i2c_dt_ids[] = {
	{ .compatible = "rt8973,i2c" },
	{ }
};
MODULE_DEVICE_TABLE(of, sec_muic_i2c_dt_ids);
#endif /* CONFIG_OF */

static struct i2c_driver rt8973_muic_driver = {
	.driver = {
		.name = "rt8973",
		.of_match_table = of_match_ptr(sec_muic_i2c_dt_ids),
	},
	.probe = rt8973_muic_probe,
//	.suspend = rt8973_muic_suspend,
//	.resume = rt8973_muic_resume,
	.remove = rt8973_muic_remove,
	.shutdown = rt8973_muic_shutdown,
	.id_table	= rt8973_i2c_id,
};

static int __init rt8973_muic_init(void)
{
	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);
	return i2c_add_driver(&rt8973_muic_driver);
}
module_init(rt8973_muic_init);

static void __exit rt8973_muic_exit(void)
{
	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);
	i2c_del_driver(&rt8973_muic_driver);
}
module_exit(rt8973_muic_exit);

MODULE_DESCRIPTION("RICHTEK RT8973 MUIC driver");
MODULE_AUTHOR("Insun Choi <insun77.choi@samsung.com>");
MODULE_LICENSE("GPL");
