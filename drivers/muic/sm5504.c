/*
 * driver/muic/sm5504.c - sm5504 micro USB switch device driver
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

#include <linux/device.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/wakelock.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>

#include <linux/muic/muic.h>
#include <linux/muic/sm5504.h>

#if defined(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/muic_notifier.h>
#endif /* CONFIG_MUIC_NOTIFIER */

#if defined(CONFIG_VBUS_NOTIFIER)
#include <linux/vbus_notifier.h>
#endif /* CONFIG_VBUS_NOTIFIER */

extern struct muic_platform_data muic_pdata;
static void sm5504_muic_handle_attach(struct sm5504_muic_data *muic_data,
			muic_attached_dev_t new_dev, int adc, u8 vbvolt);
static void sm5504_muic_handle_detach(struct sm5504_muic_data *muic_data);

static int sm5504_i2c_read_byte(const struct i2c_client *client, u8 command)
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

	return ret;
}

static int sm5504_i2c_write_byte(const struct i2c_client *client, u8 command, u8 value)
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

	return ret;
}

static ssize_t sm5504_muic_show_uart_en(struct device *dev,
						struct device_attribute *attr, char *buf)
{
	struct sm5504_muic_data *muic_data = dev_get_drvdata(dev);
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

static ssize_t sm5504_muic_set_uart_en(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t count)
{
	struct sm5504_muic_data *muic_data = dev_get_drvdata(dev);

	if (!strncmp(buf, "1", 1))
		muic_data->is_rustproof = false;
	else if (!strncmp(buf, "0", 1))
		muic_data->is_rustproof = true;
	else
		pr_info("%s:%s: invalid value\n", MUIC_DEV_NAME, __func__);

	pr_info("%s:%s: uart_en(%d)\n", MUIC_DEV_NAME, __func__, !muic_data->is_rustproof);

	return count;
}

static ssize_t sm5504_muic_show_usb_en(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct sm5504_muic_data *muic_data = dev_get_drvdata(dev);

	return sprintf(buf, "%s:%s attached_dev = %d\n",
		MUIC_DEV_NAME, __func__, muic_data->attached_dev);
}

static ssize_t sm5504_muic_set_usb_en(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct sm5504_muic_data *muic_data = dev_get_drvdata(dev);
	muic_attached_dev_t new_dev = ATTACHED_DEV_USB_MUIC;

	if (!strncasecmp(buf, "1", 1))
		sm5504_muic_handle_attach(muic_data, new_dev, 0, 0);
	else if (!strncasecmp(buf, "0", 1))
		sm5504_muic_handle_detach(muic_data);
	else
		pr_info("%s:%s: invalid value\n", MUIC_DEV_NAME, __func__);

	pr_info("%s:%s: attached_dev(%d)\n", MUIC_DEV_NAME, __func__, muic_data->attached_dev);

	return count;
}

static ssize_t sm5504_muic_show_adc(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct sm5504_muic_data *muic_data = dev_get_drvdata(dev);
	int ret;

	mutex_lock(&muic_data->muic_mutex);
	ret = sm5504_i2c_read_byte(muic_data->i2c, SM5504_MUIC_REG_ADC);
	mutex_unlock(&muic_data->muic_mutex);
	if (ret < 0) {
		pr_err("%s:%s: err read adc reg(%d)\n", MUIC_DEV_NAME, __func__, ret);
		return sprintf(buf, "UNKNOWN\n");
	}

	return sprintf(buf, "%x\n", (ret & ADC_MASK));
}

static ssize_t sm5504_muic_show_usb_state(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	struct sm5504_muic_data *muic_data = dev_get_drvdata(dev);
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

static ssize_t sm5504_muic_show_attached_dev(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct sm5504_muic_data *muic_data = dev_get_drvdata(dev);

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

#if defined(CONFIG_USB_HOST_NOTIFY)
static ssize_t sm5504_muic_show_otg_test(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct sm5504_muic_data *muic_data = dev_get_drvdata(dev);
	int ret;
	u8 val = 0;

	mutex_lock(&muic_data->muic_mutex);
	ret = sm5504_i2c_read_byte(muic_data->i2c, SM5504_MUIC_REG_INT_MASK2);
	mutex_unlock(&muic_data->muic_mutex);

	if (ret < 0) {
		pr_err("%s:%s: fail to read muic reg\n", MUIC_DEV_NAME, __func__);
		return sprintf(buf, "UNKNOWN\n");
	}

	pr_info("%s:%s: ret:%d val:%x buf%s\n", MUIC_DEV_NAME, __func__, ret, val, buf);

	val &= INTMASK1_CHGDET;
	return sprintf(buf, "%x\n", val);
}

static ssize_t sm5504_muic_set_otg_test(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sm5504_muic_data *muic_data = dev_get_drvdata(dev);

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

static ssize_t sm5504_muic_show_apo_factory(struct device *dev,
					     struct device_attribute *attr,
					     char *buf)
{
	struct sm5504_muic_data *muic_data = dev_get_drvdata(dev);
	const char *mode;

	/* true: Factory mode, false: not Factory mode */
	if (muic_data->is_factory_start)
		mode = "FACTORY_MODE";
	else
		mode = "NOT_FACTORY_MODE";

	pr_info("%s:%s: mode status[%s]\n", MUIC_DEV_NAME, __func__, mode);

	return sprintf(buf, "%s\n", mode);
}

static ssize_t sm5504_muic_set_apo_factory(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct sm5504_muic_data *muic_data = dev_get_drvdata(dev);
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

static DEVICE_ATTR(uart_en, 0664, sm5504_muic_show_uart_en,
					sm5504_muic_set_uart_en);
static DEVICE_ATTR(usb_en, 0664, sm5504_muic_show_usb_en,
					sm5504_muic_set_usb_en);
static DEVICE_ATTR(adc, 0664, sm5504_muic_show_adc, NULL);
static DEVICE_ATTR(usb_state, 0664, sm5504_muic_show_usb_state, NULL);
static DEVICE_ATTR(attached_dev, 0664, sm5504_muic_show_attached_dev, NULL);
#if defined(CONFIG_USB_HOST_NOTIFY)
static DEVICE_ATTR(otg_test, 0664, sm5504_muic_show_otg_test,
					sm5504_muic_set_otg_test);
#endif
static DEVICE_ATTR(apo_factory, 0664, sm5504_muic_show_apo_factory,
					sm5504_muic_set_apo_factory);

static struct attribute *sm5504_muic_attributes[] = {
	&dev_attr_uart_en.attr,
	&dev_attr_usb_en.attr,
	&dev_attr_adc.attr,
	&dev_attr_usb_state.attr,
	&dev_attr_attached_dev.attr,
#if defined(CONFIG_USB_HOST_NOTIFY)
	&dev_attr_otg_test.attr,
#endif
	&dev_attr_apo_factory.attr,
	NULL
};

static const struct attribute_group sm5504_muic_group = {
	.attrs = sm5504_muic_attributes,
};

static int set_com_sw(struct sm5504_muic_data *muic_data,
			u8 reg_val)
{
	struct i2c_client *i2c = muic_data->i2c;
	int ret = 0;
	int temp = 0;

	/*  --- MANSW [7:5][4:2][1:0] : DM DP RSVD --- */
	temp = sm5504_i2c_read_byte(i2c, SM5504_MUIC_REG_MAN_SW1);
	if (temp < 0)
		pr_err("%s:%s: err read MANSW(0x%x)\n", MUIC_DEV_NAME, __func__, temp);

	temp &= ~MANUAL_SW1_MASK;
	temp |= (reg_val & MANUAL_SW1_MASK);
	pr_info("%s:%s 0x%02x, 0x%02x\n", MUIC_DEV_NAME, __func__, temp, reg_val);
	ret = sm5504_i2c_write_byte(i2c, SM5504_MUIC_REG_MAN_SW1, temp);

	return ret;
}

static int com_to_open(struct sm5504_muic_data *muic_data)
{
	u8 reg_val;
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	reg_val = COM_OPEM;
	ret = set_com_sw(muic_data, reg_val);
	if (ret)
		pr_err("%s:%s: set_com_sw err\n", MUIC_DEV_NAME, __func__);

	return ret;
}

static int com_to_usb(struct sm5504_muic_data *muic_data)
{
	u8 reg_val;
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);
	reg_val = COM_TO_USB;
	ret = set_com_sw(muic_data, reg_val);
	if (ret)
		pr_err("%s:%s: set_com_usb err\n", MUIC_DEV_NAME, __func__);

	return ret;
}

static int com_to_uart(struct sm5504_muic_data *muic_data)
{
	u8 reg_val;
	int ret = 0;

	pr_info("%s:%s: rustproof mode[%d]\n", MUIC_DEV_NAME, __func__, muic_data->is_rustproof);

	if (muic_data->is_rustproof)
		return ret;

	reg_val = COM_TO_UART;
	ret = set_com_sw(muic_data, reg_val);
	if (ret)
		pr_err("%s:%s: set_com_uart err\n", MUIC_DEV_NAME, __func__);

	return ret;
}

static void sm5504_check_reg(struct sm5504_muic_data *muic_data)
{
	struct i2c_client *i2c = muic_data->i2c;
	int ctrl = 0, mansw2 = 0;

	ctrl = sm5504_i2c_read_byte(i2c, SM5504_MUIC_REG_CTRL);
	mansw2 = sm5504_i2c_read_byte(i2c, SM5504_MUIC_REG_MAN_SW1);

	pr_info("%s:%s:  ctrl:0x%02x mansw2:0x%02x\n", MUIC_DEV_NAME, __func__, ctrl, mansw2);
}

static void sm5504_set_manual_mode(struct sm5504_muic_data *muic_data, int en)
{
	struct i2c_client *i2c = muic_data->i2c;
	int reg = 0;
	int ret = 0;

	reg = sm5504_i2c_read_byte(i2c, SM5504_MUIC_REG_CTRL);
	if (en) /* manual mode */
		reg &= ~MANUAL_SWITCH;
	else /* auto mode */
		reg |= MANUAL_SWITCH;

	ret = sm5504_i2c_write_byte(i2c, SM5504_MUIC_REG_CTRL, reg);
	if (ret < 0)
		pr_err( "%s:%s: fail to update reg\n", MUIC_DEV_NAME, __func__);
}

int sm5504_muic_set_path(void *drv_data, int path)
{
	struct sm5504_muic_data *muic_data = (struct sm5504_muic_data *)drv_data;
	int ret = 0;

	pr_info("%s: %s path : %d\n", MUIC_DEV_NAME, __func__, path);
	sm5504_check_reg(muic_data);

	if (path) { /* UART */
		sm5504_set_manual_mode(muic_data, 0);
		ret = com_to_uart(muic_data); /*FIXME: it needs not? */
	} else { /* USB */
		sm5504_set_manual_mode(muic_data, 1);
		ret = com_to_usb(muic_data);
	}

	/* FIXME : check register */
	sm5504_check_reg(muic_data);
	return ret;
}

static void sm5504_muic_handle_detach(struct sm5504_muic_data *muic_data)
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

static void sm5504_muic_handle_attach(struct sm5504_muic_data *muic_data,
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
	case ATTACHED_DEV_NONE_MUIC:
		break;
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
		switch (new_dev) {
		case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
		case ATTACHED_DEV_JIG_UART_OFF_MUIC:
			break;
		default:
			sm5504_muic_handle_detach(muic_data);
			break;
		}
		break;
	case ATTACHED_DEV_DESKDOCK_MUIC:
	case ATTACHED_DEV_DESKDOCK_VB_MUIC:
		switch (new_dev) {
		case ATTACHED_DEV_DESKDOCK_MUIC:
		case ATTACHED_DEV_DESKDOCK_VB_MUIC:
			break;
		default:
			sm5504_muic_handle_detach(muic_data);
			break;
		}
		break;
	default:
		sm5504_muic_handle_detach(muic_data);
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
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
		ret = com_to_usb(muic_data);
		break;
	case ATTACHED_DEV_TA_MUIC:
	case ATTACHED_DEV_UNDEFINED_CHARGING_MUIC:
	case ATTACHED_DEV_UNKNOWN_MUIC:
	case ATTACHED_DEV_RDU_TA_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
	case ATTACHED_DEV_DESKDOCK_MUIC:
	case ATTACHED_DEV_DESKDOCK_VB_MUIC:
		ret = com_to_open(muic_data);
		break;
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
		ret = com_to_uart(muic_data);
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


static void sm5504_muic_detect_dev(struct sm5504_muic_data *muic_data)
{
	struct i2c_client *i2c = muic_data->i2c;
	muic_attached_dev_t new_dev = ATTACHED_DEV_UNKNOWN_MUIC;
	int id = 0, ctrl = 0, mansw_val1 = 0, mansw_val2 = 0;
	int dev1 = 0, dev2 = 0, adc = 0;

	id = sm5504_i2c_read_byte(i2c, SM5504_MUIC_REG_DEV_ID);
	ctrl = sm5504_i2c_read_byte(i2c, SM5504_MUIC_REG_CTRL);
	mansw_val1 = sm5504_i2c_read_byte(i2c, SM5504_MUIC_REG_MAN_SW1);
	mansw_val2 = sm5504_i2c_read_byte(i2c, SM5504_MUIC_REG_MAN_SW2);
	dev1 = sm5504_i2c_read_byte(i2c, SM5504_MUIC_REG_DEV_TYPE1);
	dev2 = sm5504_i2c_read_byte(i2c, SM5504_MUIC_REG_DEV_TYPE2);
	adc = sm5504_i2c_read_byte(i2c, SM5504_MUIC_REG_ADC);

	pr_info("%s:%s: id:0x%02x, ctrl:0x%02x, mansw1:0x%02x, mansw2:0x%02x\n", MUIC_DEV_NAME, __func__, id, ctrl, mansw_val1,mansw_val2);
	pr_info("%s:%s: dev[1:0x%02x, 2:0x%02x], adc:0x%02x, vbvolt:0x%02x\n", MUIC_DEV_NAME, __func__,
									dev1, dev2, adc, muic_data->vbvolt);

	/* Detected */
	switch (dev1) {
	case SM5504_DEV_OTG:
		new_dev = ATTACHED_DEV_OTG_MUIC;
		pr_info("%s:%s: USB_OTG DETECTED\n", MUIC_DEV_NAME, __func__);
		// OTG enable W/A for JAVA Rev03 board
		if (muic_data->otg_en == 1)
			break;
		else
			return;
	case SM5504_DEV_SDP:
		new_dev = ATTACHED_DEV_USB_MUIC;
		pr_info("%s:%s: USB DETECTED\n", MUIC_DEV_NAME, __func__);
		break;
	case SM5504_DEV_UART:
		break;
	case SM5504_DEV_CDP:
		new_dev = ATTACHED_DEV_CDP_MUIC;
		pr_info("%s:%s: USB_CDP DETECTED\n", MUIC_DEV_NAME, __func__);
		break;
	case SM5504_DEV_DCP:
		new_dev = ATTACHED_DEV_TA_MUIC;
		pr_info("%s:%s:DEDICATED CHARGER DETECTED\n", MUIC_DEV_NAME, __func__);
		break;
	default:
		break;
	}

	switch (dev2) {
	case SM5504_DEV_JIG_USB_ON:
		if (!muic_data->vbvolt)
			break;
		new_dev = ATTACHED_DEV_JIG_USB_ON_MUIC;
		pr_info("%s:%s: JIG_USB_ON DETECTED\n", MUIC_DEV_NAME, __func__);
		break;
	case SM5504_DEV_JIG_USB_OFF:
		if (!muic_data->vbvolt)
			break;
		new_dev = ATTACHED_DEV_JIG_USB_OFF_MUIC;
		pr_info("%s:%s: JIG_USB_OFF DETECTED\n", MUIC_DEV_NAME, __func__);
		break;
	case SM5504_DEV_JIG_UART_ON:
		if (new_dev != ATTACHED_DEV_JIG_UART_ON_MUIC) {
			new_dev = ATTACHED_DEV_JIG_UART_ON_MUIC;
			pr_info("%s:%s: JIG_UART_ON DETECTED\n", MUIC_DEV_NAME, __func__);
		}
		break;
	case SM5504_DEV_JIG_UART_OFF:
		if (!muic_data->is_otg_test) {
			if (muic_data->vbvolt) {
				new_dev = ATTACHED_DEV_JIG_UART_OFF_VB_MUIC;
				pr_info("%s:%s: JIG_UART_OFF_VB DETECTED\n", MUIC_DEV_NAME, __func__);
			}
			else {
				new_dev = ATTACHED_DEV_JIG_UART_OFF_MUIC;
				pr_info("%s:%s: JIG_UART_OFF DETECTED\n", MUIC_DEV_NAME, __func__);
			}
		}
		else {
			new_dev = ATTACHED_DEV_JIG_UART_OFF_MUIC;
			pr_info("%s:%s: JIG_UART_OFF DETECTED\n", MUIC_DEV_NAME, __func__);
		}
		break;
	case SM5504_DEV_JIG_UNKNOWN:
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
			if (muic_data->vbvolt) {
				new_dev = ATTACHED_DEV_USB_MUIC;
				pr_info("%s:%s: TYPE1_CHARGER or X_CABLE DETECTED (USB)\n", MUIC_DEV_NAME, __func__);
			}
			break;
		case ADC_JIG_USB_ON:
			if (!muic_data->vbvolt)
				break;
			new_dev = ATTACHED_DEV_JIG_USB_ON_MUIC;
			pr_info("%s:%s: JIG_USB_ON DETECTED\n", MUIC_DEV_NAME, __func__);
			break;
		case ADC_CEA936ATYPE2_CHG:
			if (muic_data->vbvolt) {
				new_dev = ATTACHED_DEV_TA_MUIC;
				pr_info("%s:%s: TYPE2_CHARGER DETECTED\n", MUIC_DEV_NAME, __func__);
			}
			break;
		case ADC_JIG_UART_OFF:
			if (muic_data->vbvolt) {
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
		case ADC_RDU_TA:
			if (muic_data->vbvolt) {
				new_dev = ATTACHED_DEV_RDU_TA_MUIC;
				pr_info("%s:%s: LDU/RDU TA DETECTED\n", MUIC_DEV_NAME, __func__);
			}
			break;
		case ADC_DESKDOCK:
			if (muic_data->vbvolt) {
				new_dev = ATTACHED_DEV_DESKDOCK_VB_MUIC;
				pr_info("%s:%s: DESKDOCK+VB DETECTED\n", MUIC_DEV_NAME, __func__);
			}
			else {
				new_dev = ATTACHED_DEV_DESKDOCK_MUIC;
				pr_info("%s:%s: DESKDOCK DETECTED\n", MUIC_DEV_NAME, __func__);
			}
			break;
		default:
			break;
		}
	}

	if ((new_dev == ATTACHED_DEV_UNKNOWN_MUIC) && (ADC_OPEN != adc)) {
		if (muic_data->vbvolt) {
			new_dev = ATTACHED_DEV_UNDEFINED_CHARGING_MUIC;
			pr_info("%s:%s: UNDEFINED CHARGING (0x%02x)\n", MUIC_DEV_NAME, __func__, adc);
		}
	}

	if (new_dev != ATTACHED_DEV_UNKNOWN_MUIC) {
		pr_info("%s:%s ATTACHED\n", MUIC_DEV_NAME, __func__);
		sm5504_muic_handle_attach(muic_data, new_dev, adc, muic_data->vbvolt);
	}
	else {
		pr_info("%s:%s DETACHED\n", MUIC_DEV_NAME, __func__);
		sm5504_muic_handle_detach(muic_data);
	}

#if defined(CONFIG_VBUS_NOTIFIER)
	vbus_notifier_handle((!!muic_data->vbvolt) ? STATUS_VBUS_HIGH : STATUS_VBUS_LOW);
#endif /* CONFIG_VBUS_NOTIFIER */

}

static int sm5504_muic_reg_init(struct sm5504_muic_data *muic_data);
static irqreturn_t sm5504_muic_irq_thread(int irq, void *data)
{
	struct sm5504_muic_data *muic_data = data;
	struct i2c_client *i2c = muic_data->i2c;
	int int1 = 0, int2 = 0;
	int ctrl = 0;

	msleep(10);

	/* Read and Clear Interrupt1/2 */
	int1 = sm5504_i2c_read_byte(i2c, SM5504_MUIC_REG_INT1);
	int2 = sm5504_i2c_read_byte(i2c, SM5504_MUIC_REG_INT2);
	pr_info("%s:%s: INT1[0x%02x],INT2[0x%02x]\n", MUIC_DEV_NAME, __func__, int1, int2);

	muic_data->vbvolt = !(int2 & INT_UVLO_MASK);

	ctrl = sm5504_i2c_read_byte(i2c, SM5504_MUIC_REG_CTRL);
	pr_info("%s:%s: CTRL[0x%02x]\n", MUIC_DEV_NAME, __func__, ctrl);
	if (ctrl&0x01) {
		sm5504_muic_reg_init(muic_data);
		return IRQ_HANDLED;
	}

	mutex_lock(&muic_data->muic_mutex);
	wake_lock(&muic_data->muic_wake_lock);

	/* device detection */
	sm5504_muic_detect_dev(muic_data);
	
	wake_unlock(&muic_data->muic_wake_lock);
	mutex_unlock(&muic_data->muic_mutex);
	return IRQ_HANDLED;
}

static int sm5504_muic_irq_init(struct sm5504_muic_data *muic_data)
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
				sm5504_muic_irq_thread,
				(IRQF_TRIGGER_FALLING |
				IRQF_NO_SUSPEND | IRQF_ONESHOT),
				"sm5504-muic", muic_data);
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
static int of_sm5504_muic_dt(struct device *dev, struct sm5504_muic_data *muic_data)
{
	struct device_node *np_muic = dev->of_node;
	int ret = 0;

	if (np_muic == NULL) {
		pr_err("%s:%s: np NULL\n", MUIC_DEV_NAME, __func__);
		return -EINVAL;
	} else {
		muic_data->pdata->irq_gpio =
			of_get_named_gpio(np_muic, "sm5504,irq-gpio", 0);
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

		// OTG enable W/A for JAVA Rev03 board
		ret = of_property_read_u8(np_muic, "sm5504,otg_en", &muic_data->otg_en);
		if (ret) {
			pr_err("%s:%s There is no Property of sm5504,otg_en\n",
						MUIC_DEV_NAME, __func__);
			muic_data->otg_en = 0;
		}
		pr_info("%s:%s muic_data->otg_en:0x%02x\n", MUIC_DEV_NAME, __func__, muic_data->otg_en);
	}

	return ret;
}
#endif /* CONFIG_OF */

static int sm5504_muic_reg_init(struct sm5504_muic_data *muic_data)
{
	struct i2c_client *i2c = muic_data->i2c;
	int ret = 0;
	int dev1 = 0, dev2 = 0, adc = 0, mansw1 = 0, ctrl = 0, intmask2 = 0;
	int int1, int2, reg_set;

	/* Read and Clear Interrupt1/2 */
	int1 = sm5504_i2c_read_byte(i2c, SM5504_MUIC_REG_INT1);
	int2 = sm5504_i2c_read_byte(i2c, SM5504_MUIC_REG_INT2);
	pr_info("%s:%s: INT1[0x%02x],INT2[0x%02x]\n", MUIC_DEV_NAME, __func__, int1, int2);

	dev1 = sm5504_i2c_read_byte(i2c, SM5504_MUIC_REG_DEV_TYPE1);
	dev2 = sm5504_i2c_read_byte(i2c, SM5504_MUIC_REG_DEV_TYPE2);
	mansw1 = sm5504_i2c_read_byte(i2c, SM5504_MUIC_REG_MAN_SW1);
	adc = sm5504_i2c_read_byte(i2c, SM5504_MUIC_REG_ADC);
	ctrl = sm5504_i2c_read_byte(i2c, SM5504_MUIC_REG_CTRL);
	intmask2 = sm5504_i2c_read_byte(i2c, SM5504_MUIC_REG_INT_MASK2);

	pr_info("%s:%s: DEV1[0x%02x], DEV2[0x%02x], MANSW1[0x%02x], \
			ADC[0x%02x], CTRL[0x%02x], INTMASK2[0x%02x]\n", MUIC_DEV_NAME, \
			__func__, dev1, dev2, mansw1, adc, ctrl, intmask2);

	ret = sm5504_i2c_write_byte(i2c, SM5504_MUIC_REG_CTRL, CTRL_INIT);
	if (ret < 0)
		pr_err( "%s:%s: failed to write ctrl(%d)\n", MUIC_DEV_NAME, __func__, ret);
	ctrl = sm5504_i2c_read_byte(i2c, SM5504_MUIC_REG_CTRL);

	ret = sm5504_i2c_write_byte(i2c, SM5504_MUIC_REG_INT_MASK2, INTMASK2_INIT);
	if (ret < 0)
		pr_err( "%s:%s: failed to write intmask2(%d)\n", MUIC_DEV_NAME, __func__, ret);
	intmask2 = sm5504_i2c_read_byte(i2c, SM5504_MUIC_REG_INT_MASK2);

	pr_info("%s:%s:After: CTRL[0x%02x], INTMASK2[0x%02x]\n", MUIC_DEV_NAME, __func__, ctrl, intmask2);

	/* W/A for VBUS detect time (300ms->140ms) */
	reg_set = sm5504_i2c_read_byte(i2c, SM5504_MUIC_REG_SET);
	pr_info("%s:%s:SET Reg: [0x%02x]", MUIC_DEV_NAME, __func__, reg_set);
	ret = sm5504_i2c_write_byte(i2c, SM5504_MUIC_REG_SET, VBUS_140MS);
	reg_set = sm5504_i2c_read_byte(i2c, SM5504_MUIC_REG_SET);
	pr_info(" -> [0x%02x]\n", reg_set);

	return ret;
}

static int sm5504_muic_probe(struct i2c_client *i2c,
				const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(i2c->dev.parent);
	struct sm5504_muic_data *muic_data;
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		pr_err("%s:%s: i2c functionality check error\n", MUIC_DEV_NAME, __func__);
		ret = -EIO;
		goto err_return;
	}

	muic_data = kzalloc(sizeof(struct sm5504_muic_data), GFP_KERNEL);
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
	ret = of_sm5504_muic_dt(&i2c->dev, muic_data);
	if (ret < 0)
		pr_err("%s:%s: no muic dt! ret[%d]\n", MUIC_DEV_NAME, __func__, ret);
#endif /* CONFIG_OF */

	i2c_set_clientdata(i2c, muic_data);

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
	muic_data->is_otg_test = false;
	muic_data->vbvolt = 0;

#ifdef CONFIG_SEC_SYSFS
	/* create sysfs group */
	ret = sysfs_create_group(&switch_device->kobj, &sm5504_muic_group);
	if (ret) {
		pr_err("%s:%s: failed to create sysfs\n", MUIC_DEV_NAME, __func__);
		goto fail;
	}
	dev_set_drvdata(switch_device, muic_data);
#endif

	if (muic_data->pdata->init_switch_dev_cb)
		muic_data->pdata->init_switch_dev_cb();

	ret = sm5504_muic_reg_init(muic_data);
	if (ret) {
		pr_err("%s:%s: failed to init reg\n", MUIC_DEV_NAME, __func__);
		goto fail;
	}

	muic_data->is_rustproof = muic_data->pdata->rustproof_on;
	muic_data->pdata->muic_set_path = sm5504_muic_set_path;

	ret = sm5504_muic_irq_init(muic_data);
	if (ret) {
		pr_err("%s:%s: failed to init irq(%d)\n", MUIC_DEV_NAME, __func__, ret);
		goto fail_init_irq;
	}

	wake_lock_init(&muic_data->muic_wake_lock, WAKE_LOCK_SUSPEND, "muic_wake");

	/* initial cable detection */
	sm5504_muic_irq_thread(-1, muic_data);

	return 0;

fail_init_irq:
	if (i2c->irq)
		free_irq(i2c->irq, muic_data);
fail:
#ifdef CONFIG_SEC_SYSFS
	sysfs_remove_group(&switch_device->kobj, &sm5504_muic_group);
#endif
	mutex_destroy(&muic_data->muic_mutex);
fail_init_gpio:
	i2c_set_clientdata(i2c, NULL);
	kfree(muic_data);
err_return:
	return ret;
}

static const struct i2c_device_id sm5504_i2c_id[] = {
	{ "sm5504", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, sm5504_i2c_id);

#if defined(CONFIG_OF)
static struct of_device_id sec_muic_i2c_dt_ids[] = {
	{ .compatible = "sm5504,i2c" },
	{ }
};
MODULE_DEVICE_TABLE(of, sec_muic_i2c_dt_ids);
#endif /* CONFIG_OF */

static struct i2c_driver sm5504_muic_driver = {
	.driver = {
		.name = "sm5504",
		.of_match_table = of_match_ptr(sec_muic_i2c_dt_ids),
	},
	.probe = sm5504_muic_probe,
	/* TODO: suspend resume remove shutdown */
	.id_table	= sm5504_i2c_id,
};

static int __init sm5504_muic_init(void)
{
	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);
	return i2c_add_driver(&sm5504_muic_driver);
}
module_init(sm5504_muic_init);

static void __exit sm5504_muic_exit(void)
{
	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);
	i2c_del_driver(&sm5504_muic_driver);
}
module_exit(sm5504_muic_exit);

MODULE_DESCRIPTION("SM5504 MUIC driver");
MODULE_AUTHOR("Ji-Hun Kim <ji_hun.kim@samsung.com>");
MODULE_LICENSE("GPL");
