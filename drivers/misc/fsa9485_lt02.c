/*
 * driver/misc/fsa9480.c - FSA9480 micro USB switch device driver
 *
 * Copyright (C) 2010 Samsung Electronics
 * Minkyu Kang <mk7.kang@samsung.com>
 * Wonguk Jeong <wonguk.jeong@samsung.com>
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/i2c/fsa9485.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <linux/mfd/pmic8058.h>
#include <linux/input.h>
#include <linux/sii9234.h>
#include <linux/mfd/pm8xxx/pm8921-charger.h>
/* Use Only TSU6721 */
#define FSA9485_REG_DEV_T3				0x15
#define INT_MASK2						0xA0

/* DEVICE ID */
#define FSA9485_DEV_ID					0x00
#define FSA9485_DEV_ID_REV					0x10
#define TSU6721_DEV_ID					0x0A
#define TSU6721_DEV_ID_REV				0x12

/* FSA9480 I2C registers */
#define FSA9485_REG_DEVID				0x01
#define FSA9485_REG_CTRL				0x02
#define FSA9485_REG_INT1				0x03
#define FSA9485_REG_INT2				0x04
#define FSA9485_REG_INT1_MASK			0x05
#define FSA9485_REG_INT2_MASK			0x06
#define FSA9485_REG_ADC					0x07
#define FSA9485_REG_TIMING1				0x08
#define FSA9485_REG_TIMING2				0x09
#define FSA9485_REG_DEV_T1				0x0a
#define FSA9485_REG_DEV_T2				0x0b
#define FSA9485_REG_BTN1				0x0c
#define FSA9485_REG_BTN2				0x0d
#define FSA9485_REG_CK					0x0e
#define FSA9485_REG_CK_INT1				0x0f
#define FSA9485_REG_CK_INT2				0x10
#define FSA9485_REG_CK_INTMASK1			0x11
#define FSA9485_REG_CK_INTMASK2			0x12
#define FSA9485_REG_MANSW1				0x13
#define FSA9485_REG_MANSW2				0x14
#define FSA9485_REG_MANUAL_OVERRIDES1	0x1B
#define FSA9485_REG_RESERVED_1D			0x1D
#define FSA9485_REG_RESERVED_20			0x20


/* Control */
#define CON_SWITCH_OPEN		(1 << 4)
#define CON_RAW_DATA		(1 << 3)
#define CON_MANUAL_SW		(1 << 2)
#define CON_WAIT			(1 << 1)
#define CON_INT_MASK		(1 << 0)
#define CON_MASK		(CON_SWITCH_OPEN | CON_RAW_DATA | \
				CON_MANUAL_SW | CON_WAIT)

/* Device Type 1 */
#define DEV_USB_OTG			(1 << 7)
#define DEV_DEDICATED_CHG	(1 << 6)
#define DEV_USB_CHG			(1 << 5)
#define DEV_CAR_KIT			(1 << 4)
#define DEV_UART			(1 << 3)
#define DEV_USB				(1 << 2)
#define DEV_AUDIO_2			(1 << 1)
#define DEV_AUDIO_1			(1 << 0)

#define DEV_T1_USB_MASK		(DEV_USB_OTG | DEV_USB_CHG | DEV_USB)
#define DEV_T1_UART_MASK	(DEV_UART)
#define DEV_T1_CHARGER_MASK	(DEV_DEDICATED_CHG | DEV_CAR_KIT)

/* Device Type 2 */
#define DEV_AUDIO_DOCK		(1 << 8)
#define DEV_SMARTDOCK		(1 << 7)
#define DEV_AV			(1 << 6)
#define DEV_TTY				(1 << 5)
#define DEV_PPD				(1 << 4)
#define DEV_JIG_UART_OFF	(1 << 3)
#define DEV_JIG_UART_ON		(1 << 2)
#define DEV_JIG_USB_OFF		(1 << 1)
#define DEV_JIG_USB_ON		(1 << 0)

#define DEV_T2_USB_MASK		(DEV_JIG_USB_OFF | DEV_JIG_USB_ON)
#define DEV_T2_UART_MASK	(DEV_JIG_UART_OFF)
#define DEV_T2_JIG_MASK		(DEV_JIG_USB_OFF | DEV_JIG_USB_ON | \
				DEV_JIG_UART_OFF)
#define DEV_T2_JIG_ALL_MASK	(DEV_JIG_USB_OFF | DEV_JIG_USB_ON | \
				DEV_JIG_UART_OFF | DEV_JIG_UART_ON)

/* Device Type 3 */
#define DEV_MHL				(1 << 0)
#define DEV_VBUS_DEBOUNCE		(1 << 1)
#define DEV_NON_STANDARD		(1 << 2)
#define DEV_AV_VBUS			(1 << 4)
#define DEV_APPLE_CHARGER		(1 << 5)
#define DEV_U200_CHARGER		(1 << 6)

#define DEV_T3_CHARGER_MASK	(DEV_NON_STANDARD | DEV_APPLE_CHARGER | \
				DEV_U200_CHARGER)

/*
 * Manual Switch
 * D- [7:5] / D+ [4:2]
 * 000: Open all / 001: USB / 010: AUDIO / 011: UART / 100: V_AUDIO
 */
#define SW_VAUDIO		((4 << 5) | (4 << 2) | (1 << 1) | (1 << 0))
#define SW_UART			((3 << 5) | (3 << 2))
#define SW_AUDIO		((2 << 5) | (2 << 2) | (1 << 1) | (1 << 0))
#define SW_AUDIO_TSU	((2 << 5) | (2 << 2) | (1 << 0))
#define SW_DHOST		((1 << 5) | (1 << 2) | (1 << 1) | (1 << 0))
#define SW_DHOST_TSU	((1 << 5) | (1 << 2) | (1 << 0))
#define SW_AUTO			((0 << 5) | (0 << 2))
#define SW_USB_OPEN		(1 << 0)
#define SW_ALL_OPEN		(0)

/* Interrupt 1 */
#define INT_DETACH			(1 << 1)
#define INT_ATTACH			(1 << 0)
#define INT_OVP_EN			(1 << 5)
#define INT_OXP_DISABLE		(1 << 7)


#define	ADC_GND					0x00
#define	ADC_MHL					0x01
#define	ADC_DOCK_PREV_KEY		0x04
#define	ADC_DOCK_NEXT_KEY		0x07
#define	ADC_DOCK_VOL_DN			0x0a
#define	ADC_DOCK_VOL_UP			0x0b
#define	ADC_DOCK_PLAY_PAUSE_KEY 0x0d
#define	ADC_CEA936ATYPE1_CHG	0x17
#define	ADC_JIG_USB_OFF			0x18
#define	ADC_JIG_USB_ON			0x19
#define	ADC_DESKDOCK			0x1a
#define	ADC_CEA936ATYPE2_CHG	0x1b
#define	ADC_JIG_UART_OFF		0x1c
#define	ADC_JIG_UART_ON			0x1d
#define	ADC_CARDOCK				0x1d
#define	ADC_OPEN				0x1f

int uart_connecting;
EXPORT_SYMBOL(uart_connecting);
int detached_status;
EXPORT_SYMBOL(detached_status);
static int jig_state;

struct fsa9485_last_state {
	int dev1;
	int dev2;
	int dev3;
	int int1;
	int int2;
	int	attach;
	int detach;
};

struct fsa9485_usbsw {
	struct i2c_client		*client;
	struct fsa9485_platform_data	*pdata;
	int				dev1;
	int				dev2;
	int				dev3;
	int				mansw;
	int				dock_attached;
	int				dev_id;

	int			previous_key;

	struct delayed_work	init_work;
	struct mutex		mutex;
	int				adc;
	struct fsa9485_last_state last_state;
};

enum {
	DOCK_KEY_NONE			= 0,
	DOCK_KEY_VOL_UP_PRESSED,
	DOCK_KEY_VOL_UP_RELEASED,
	DOCK_KEY_VOL_DOWN_PRESSED,
	DOCK_KEY_VOL_DOWN_RELEASED,
	DOCK_KEY_PREV_PRESSED,
	DOCK_KEY_PREV_RELEASED,
	DOCK_KEY_PLAY_PAUSE_PRESSED,
	DOCK_KEY_PLAY_PAUSE_RELEASED,
	DOCK_KEY_NEXT_PRESSED,
	DOCK_KEY_NEXT_RELEASED,

};
static struct fsa9485_usbsw *local_usbsw;

#if defined(CONFIG_VIDEO_MHL_V2)
#define MHL_DEVICE		2
static int isDeskdockconnected;
#endif

static int is_ti_muic(void)
{
	return ((local_usbsw->dev_id == TSU6721_DEV_ID ||
		local_usbsw->dev_id == TSU6721_DEV_ID_REV) ? 1 : 0);
}

static void DisableFSA9480Interrupts(void)
{
	struct i2c_client *client = local_usbsw->client;
	int value, ret;

	value = i2c_smbus_read_byte_data(client, FSA9485_REG_CTRL);
	value |= 0x01;

	ret = i2c_smbus_write_byte_data(client, FSA9485_REG_CTRL, value);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

}

static void EnableFSA9480Interrupts(void)
{
	struct i2c_client *client = local_usbsw->client;
	int value, ret;

	value = i2c_smbus_read_byte_data(client, FSA9485_REG_CTRL);
	value &= 0xFE;

	ret = i2c_smbus_write_byte_data(client, FSA9485_REG_CTRL, value);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

}

void FSA9485_CheckAndHookAudioDock(int value)
{
	struct i2c_client *client = local_usbsw->client;
	struct fsa9485_platform_data *pdata = local_usbsw->pdata;
	int ret = 0;
	if (pdata->mhl_sel)
		pdata->mhl_sel(value);
	if (value) {
		pr_info("FSA9485_CheckAndHookAudioDock ON\n");
		pdata->callback(CABLE_TYPE_DESK_DOCK, FSA9485_ATTACHED);
		local_usbsw->last_state.attach = DESKDOCK_CALL;

		if (is_ti_muic())
			ret = i2c_smbus_write_byte_data(client,
				FSA9485_REG_MANSW1, SW_AUDIO_TSU);
		else
			ret = i2c_smbus_write_byte_data(client,
				FSA9485_REG_MANSW1, SW_AUDIO);

		if (ret < 0)
			dev_err(&client->dev, "%s: err %d\n",
						__func__, ret);

		ret = i2c_smbus_read_byte_data(client,
						FSA9485_REG_CTRL);
		if (ret < 0)
			dev_err(&client->dev, "%s: err %d\n",
						__func__, ret);

		ret = i2c_smbus_write_byte_data(client,
				FSA9485_REG_CTRL,
				ret & ~CON_MANUAL_SW & ~CON_RAW_DATA);
		if (ret < 0)
			dev_err(&client->dev,
					"%s: err %d\n", __func__, ret);
	} else {
		dev_info(&client->dev, "FSA9485_CheckAndHookAudioDock Off\n");
		pdata->callback(CABLE_TYPE_DESK_DOCK, FSA9485_DETACHED);
		local_usbsw->last_state.detach = DESKDOCK_CALL;

		ret = i2c_smbus_read_byte_data(client,
					FSA9485_REG_CTRL);
		if (ret < 0)
			dev_err(&client->dev,
				"%s: err %d\n", __func__, ret);

		ret = i2c_smbus_write_byte_data(client,
				FSA9485_REG_CTRL,
				ret | CON_MANUAL_SW | CON_RAW_DATA);
		if (ret < 0)
			dev_err(&client->dev,
				"%s: err %d\n", __func__, ret);
	}
}

static void fsa9485_reg_init(struct fsa9485_usbsw *usbsw)
{
	struct i2c_client *client = usbsw->client;
	unsigned int ctrl = CON_MASK;
	int ret;

	pr_info("fsa9485_reg_init is called\n");

	usbsw->dev_id = i2c_smbus_read_byte_data(client, FSA9485_REG_DEVID);
	local_usbsw->dev_id = usbsw->dev_id;
	if (usbsw->dev_id < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, usbsw->dev_id);

	dev_info(&client->dev, " fsa9485_reg_init dev ID: 0x%x\n",
			usbsw->dev_id);

	/* mask interrupts (unmask attach/detach only) */
	ret = i2c_smbus_write_byte_data(client, FSA9485_REG_INT1_MASK, 0x5c);

	if (is_ti_muic()) {
		ret = i2c_smbus_write_byte_data(client,
				FSA9485_REG_INT2_MASK, INT_MASK2);
		if (ret < 0)
			dev_err(&client->dev, "%s: err %d\n", __func__, ret);
	} else {
		ret = i2c_smbus_write_byte_data(client,
				FSA9485_REG_INT2_MASK, 0x18);
		if (ret < 0)
			dev_err(&client->dev, "%s: err %d\n", __func__, ret);
	}

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	/* mask all car kit interrupts */
	ret = i2c_smbus_write_word_data(client,
					FSA9485_REG_CK_INTMASK1, 0x07ff);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	/* ADC Detect Time: 500ms */
	ret = i2c_smbus_write_byte_data(client, FSA9485_REG_TIMING1, 0x0);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	usbsw->mansw = i2c_smbus_read_byte_data(client, FSA9485_REG_MANSW1);
	if (usbsw->mansw < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, usbsw->mansw);

	if (usbsw->mansw)
		ctrl &= ~CON_MANUAL_SW;	/* Manual Switching Mode */
	else
		ctrl &= ~(CON_INT_MASK);

	ret = i2c_smbus_write_byte_data(client, FSA9485_REG_CTRL, ctrl);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	if (!is_ti_muic()) {
		/* apply Battery Charging Spec. 1.1 @TA/USB detect */
		ret = i2c_smbus_write_byte_data(client,
				FSA9485_REG_RESERVED_20, 0x04);
		if (ret < 0)
			dev_err(&client->dev, "%s: err %d\n", __func__, ret);
	} else {
		/* BCDv1.2 Timer  default  1.8s -> 0.6s */
		ret = i2c_smbus_write_byte_data(client,
			FSA9485_REG_RESERVED_20, 0x05);
		if (ret < 0)
			dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	}
}

static ssize_t fsa9485_show_control(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct fsa9485_usbsw *usbsw = dev_get_drvdata(dev);
	struct i2c_client *client = usbsw->client;
	int value;

	value = i2c_smbus_read_byte_data(client, FSA9485_REG_CTRL);
	if (value < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, value);

	return snprintf(buf, 13, "CONTROL: %02x\n", value);
}

static ssize_t fsa9485_show_device_type(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct fsa9485_usbsw *usbsw = dev_get_drvdata(dev);
	struct i2c_client *client = usbsw->client;
	int value;

	value = i2c_smbus_read_byte_data(client, FSA9485_REG_DEV_T1);
	if (value < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, value);

	return snprintf(buf, 11, "DEVICE_TYPE: %02x\n", value);
}

static ssize_t fsa9485_show_manualsw(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fsa9485_usbsw *usbsw = dev_get_drvdata(dev);
	struct i2c_client *client = usbsw->client;
	int value;

	value = i2c_smbus_read_byte_data(client, FSA9485_REG_MANSW1);
	if (value < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, value);

	if (value == SW_VAUDIO)
		return snprintf(buf, 7, "VAUDIO\n");
	else if (value == SW_UART)
		return snprintf(buf, 5, "UART\n");
	else if (value == SW_AUDIO)
		return snprintf(buf, 6, "AUDIO\n");
	else if (value == SW_DHOST)
		return snprintf(buf, 6, "DHOST\n");
	else if (value == SW_AUTO)
		return snprintf(buf, 5, "AUTO\n");
	else
		return snprintf(buf, 4, "%x", value);
}

static ssize_t fsa9485_set_manualsw(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct fsa9485_usbsw *usbsw = dev_get_drvdata(dev);
	struct i2c_client *client = usbsw->client;
	int value, ret;
	unsigned int path = 0;

	value = i2c_smbus_read_byte_data(client, FSA9485_REG_CTRL);
	if (value < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, value);

	if ((value & ~CON_MANUAL_SW) !=
			(CON_SWITCH_OPEN | CON_RAW_DATA | CON_WAIT))
		return 0;

	if (!strncmp(buf, "VAUDIO", 6)) {
		path = SW_VAUDIO;
		value &= ~CON_MANUAL_SW;
	} else if (!strncmp(buf, "UART", 4)) {
		path = SW_UART;
		value &= ~CON_MANUAL_SW;
	} else if (!strncmp(buf, "AUDIO", 5)) {
		path = SW_AUDIO;
		value &= ~CON_MANUAL_SW;
	} else if (!strncmp(buf, "DHOST", 5)) {
		path = SW_DHOST;
		value &= ~CON_MANUAL_SW;
	} else if (!strncmp(buf, "AUTO", 4)) {
		path = SW_AUTO;
		value |= CON_MANUAL_SW;
	} else {
		dev_err(dev, "Wrong command\n");
		return 0;
	}

	usbsw->mansw = path;

	ret = i2c_smbus_write_byte_data(client, FSA9485_REG_MANSW1, path);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	ret = i2c_smbus_write_byte_data(client, FSA9485_REG_CTRL, value);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return count;
}
static ssize_t fsa9480_show_usb_state(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct fsa9485_usbsw *usbsw = dev_get_drvdata(dev);
	struct i2c_client *client = usbsw->client;
	unsigned char device_type1, device_type2;

	device_type1 = i2c_smbus_read_byte_data(client, FSA9485_REG_DEV_T1);
	if (device_type1 < 0) {
		dev_err(&client->dev, "%s: err %d ", __func__, device_type1);
		return (ssize_t)device_type1;
	}
	device_type2 = i2c_smbus_read_byte_data(client, FSA9485_REG_DEV_T2);
	if (device_type2 < 0) {
		dev_err(&client->dev, "%s: err %d ", __func__, device_type2);
		return (ssize_t)device_type2;
	}

	if (device_type1 & DEV_T1_USB_MASK || device_type2 & DEV_T2_USB_MASK)
		return snprintf(buf, 22, "USB_STATE_CONFIGURED\n");

	return snprintf(buf, 25, "USB_STATE_NOTCONFIGURED\n");
}

static ssize_t fsa9485_show_adc(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct fsa9485_usbsw *usbsw = dev_get_drvdata(dev);
	struct i2c_client *client = usbsw->client;
	int adc;

	adc = i2c_smbus_read_byte_data(client, FSA9485_REG_ADC);
	if (adc < 0) {
		dev_err(&client->dev,
			"%s: err at read adc %d\n", __func__, adc);
		return snprintf(buf, 9, "UNKNOWN\n");
	}

	return snprintf(buf, 4, "%x\n", adc);
}

static ssize_t fsa9485_reset(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct fsa9485_usbsw *usbsw = dev_get_drvdata(dev);
	struct i2c_client *client = usbsw->client;
	int ret;

	if (!strncmp(buf, "1", 1)) {
		dev_info(&client->dev,
			"fsa9480 reset after delay 1000 msec.\n");
		msleep(1000);
		ret = i2c_smbus_write_byte_data(client,
					FSA9485_REG_MANUAL_OVERRIDES1, 0x01);
		if (ret < 0)
				dev_err(&client->dev,
					"cannot soft reset, err %d\n", ret);

	dev_info(&client->dev, "fsa9480_reset_control done!\n");
	} else {
		dev_info(&client->dev,
			"fsa9480_reset_control, but not reset_value!\n");
	}

	fsa9485_reg_init(usbsw);

	return count;
}


static DEVICE_ATTR(control, S_IRUGO, fsa9485_show_control, NULL);
static DEVICE_ATTR(device_type, S_IRUGO, fsa9485_show_device_type, NULL);
static DEVICE_ATTR(switch, S_IRUGO | S_IWUSR,
		fsa9485_show_manualsw, fsa9485_set_manualsw);
static DEVICE_ATTR(usb_state, S_IRUGO, fsa9480_show_usb_state, NULL);
static DEVICE_ATTR(adc, S_IRUGO, fsa9485_show_adc, NULL);
static DEVICE_ATTR(reset_switch, S_IWUSR | S_IWGRP, NULL, fsa9485_reset);

static struct attribute *fsa9485_attributes[] = {
	&dev_attr_control.attr,
	&dev_attr_device_type.attr,
	&dev_attr_switch.attr,
	NULL
};

static const struct attribute_group fsa9485_group = {
	.attrs = fsa9485_attributes,
};

void fsa9485_otg_detach(void)
{
	unsigned int data = 0;
	int ret;
	struct i2c_client *client = local_usbsw->client;

	if (local_usbsw->dev1 & DEV_USB_OTG) {
		dev_info(&client->dev, "%s: real device\n", __func__);
		data = 0x00;
		ret = i2c_smbus_write_byte_data(client,
						FSA9485_REG_MANSW2, data);
		if (ret < 0)
			dev_info(&client->dev, "%s: err %d\n", __func__, ret);
		data = SW_ALL_OPEN;
		ret = i2c_smbus_write_byte_data(client,
						FSA9485_REG_MANSW1, data);
		if (ret < 0)
			dev_info(&client->dev, "%s: err %d\n", __func__, ret);

		data = 0x1A;
		ret = i2c_smbus_write_byte_data(client,
						FSA9485_REG_CTRL, data);
		if (ret < 0)
			dev_info(&client->dev, "%s: err %d\n", __func__, ret);
		local_usbsw->last_state.detach = OTG_CALL;
	} else
		dev_info(&client->dev, "%s: not real device\n", __func__);
}
EXPORT_SYMBOL(fsa9485_otg_detach);


void fsa9485_manual_switching(int path)
{
	struct i2c_client *client = local_usbsw->client;
	int value, ret;
	unsigned int data = 0;

	value = i2c_smbus_read_byte_data(client, FSA9485_REG_CTRL);
	if (value < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, value);

	if ((value & ~CON_MANUAL_SW) !=
			(CON_SWITCH_OPEN | CON_RAW_DATA | CON_WAIT))
		return;

	if (path == SWITCH_PORT_VAUDIO) {
		data = SW_VAUDIO;
		value &= ~CON_MANUAL_SW;
	} else if (path ==  SWITCH_PORT_UART) {
		data = SW_UART;
		value &= ~CON_MANUAL_SW;
	} else if (path ==  SWITCH_PORT_AUDIO) {
		data = SW_AUDIO;
		value &= ~CON_MANUAL_SW;
	} else if (path ==  SWITCH_PORT_USB) {
		data = SW_DHOST;
		value &= ~CON_MANUAL_SW;
	} else if (path ==  SWITCH_PORT_AUTO) {
		data = SW_AUTO;
		value |= CON_MANUAL_SW;
	} else if (path ==  SWITCH_PORT_USB_OPEN) {
		data = SW_USB_OPEN;
		value &= ~CON_MANUAL_SW;
	} else if (path ==  SWITCH_PORT_ALL_OPEN) {
		data = SW_ALL_OPEN;
		value &= ~CON_MANUAL_SW;
	} else {
		pr_info("%s: wrong path (%d)\n", __func__, path);
		return;
	}

	local_usbsw->mansw = data;

	/* path for FTM sleep */
	if (path ==  SWITCH_PORT_ALL_OPEN) {
		ret = i2c_smbus_write_byte_data(client,
					FSA9485_REG_MANUAL_OVERRIDES1, 0x0a);
		if (ret < 0)
			dev_err(&client->dev, "%s: err %d\n", __func__, ret);

		ret = i2c_smbus_write_byte_data(client,
						FSA9485_REG_MANSW1, data);
		if (ret < 0)
			dev_err(&client->dev, "%s: err %d\n", __func__, ret);

		ret = i2c_smbus_write_byte_data(client,
						FSA9485_REG_MANSW2, data);
		if (ret < 0)
			dev_err(&client->dev, "%s: err %d\n", __func__, ret);

		ret = i2c_smbus_write_byte_data(client,
						FSA9485_REG_CTRL, value);
		if (ret < 0)
			dev_err(&client->dev, "%s: err %d\n", __func__, ret);
	} else {
		ret = i2c_smbus_write_byte_data(client,
						FSA9485_REG_MANSW1, data);
		if (ret < 0)
			dev_err(&client->dev, "%s: err %d\n", __func__, ret);

		ret = i2c_smbus_write_byte_data(client,
						FSA9485_REG_CTRL, value);
		if (ret < 0)
			dev_err(&client->dev, "%s: err %d\n", __func__, ret);
	}

}
EXPORT_SYMBOL(fsa9485_manual_switching);

#if defined(CONFIG_VIDEO_MHL_V2)
int dock_det(void)
{
	return isDeskdockconnected;
}
EXPORT_SYMBOL(dock_det);
#endif

int check_jig_state(void)
{
	return jig_state;
}
EXPORT_SYMBOL(check_jig_state);

void fsa9485_monitor(void)
{
	int val1, val2, val3;
	struct i2c_client *client = local_usbsw->client;

	val1 = i2c_smbus_read_byte_data(client, FSA9485_REG_DEV_T1);
	val2 = i2c_smbus_read_byte_data(client, FSA9485_REG_DEV_T2);
	val3 = i2c_smbus_read_byte_data(client, is_ti_muic() ?
			FSA9485_REG_DEV_T3 : FSA9485_REG_RESERVED_1D);
	if (!is_ti_muic())
		val3 = val3 & 0x02;

	pr_info("%s lastINT[%x][%x] lastDev[%x][%x][%x] currDev[%x][%x][%x] callState[%d][%d]\n",
			__func__, local_usbsw->last_state.int1,
			local_usbsw->last_state.int2,
			local_usbsw->last_state.dev1,
			local_usbsw->last_state.dev2,
			local_usbsw->last_state.dev3, val1, val2, val3,
			local_usbsw->last_state.attach,
			local_usbsw->last_state.detach);
}
EXPORT_SYMBOL(fsa9485_monitor);
static int fsa9485_detect_dev(struct fsa9485_usbsw *usbsw)
{
	int ret, adc;
	unsigned int val1, val2, val3;
	struct fsa9485_platform_data *pdata = usbsw->pdata;
	struct i2c_client *client = usbsw->client;
#if defined(CONFIG_VIDEO_MHL_V2)
	u8 mhl_ret = 0;
#endif

	val1 = i2c_smbus_read_byte_data(client, FSA9485_REG_DEV_T1);
	if (val1 < 0) {
		dev_err(&client->dev, "%s: err %d\n", __func__, val1);
		return val1;
	}

	val2 = i2c_smbus_read_byte_data(client, FSA9485_REG_DEV_T2);
	if (val2 < 0) {
		dev_err(&client->dev, "%s: err %d\n", __func__, val2);
		return val2;
	}
	jig_state =  (val2 & DEV_T2_JIG_ALL_MASK) ? 1 : 0;

	if (is_ti_muic()) {
		val3 = i2c_smbus_read_byte_data(client, FSA9485_REG_DEV_T3);
		if (val3 < 0) {
			dev_err(&client->dev, "%s: err %d\n", __func__, val3);
			return val3;
		}
	} else {
		val3 = i2c_smbus_read_byte_data(client,
				FSA9485_REG_RESERVED_1D);
		if (val3 < 0) {
			dev_err(&client->dev, "%s: err %d\n", __func__, val3);
			return val3;
		}
		val3 = val3 & 0x02;
	}

	adc = i2c_smbus_read_byte_data(client, FSA9485_REG_ADC);
#if defined(CONFIG_USB_SWITCH_SMART_DOCK_ENABLE)
	if (adc == 0x10)
		val2 = DEV_SMARTDOCK;
	else
#endif
	if (adc == 0x11 || adc == 0x12) {
		val2 = DEV_AUDIO_DOCK;
		dev_err(&client->dev, "adc is audio");
		val1 = 0;
	}
	dev_err(&client->dev,
			"dev1: 0x%x, dev2: 0x%x, dev3: 0x%x, ADC: 0x%x Jig:%s\n",
			val1, val2, val3, adc,
			(check_jig_state() ? "ON" : "OFF"));

	if ((val1+val2+val3 != 0)) {
		pr_info("%s Save state\n", __func__);
		local_usbsw->last_state.dev1 = val1;
		local_usbsw->last_state.dev2 = val2;
		local_usbsw->last_state.dev3 = val3;
	}

	/* Attached */
	if (val1 || val2 || (val3 & ~DEV_VBUS_DEBOUNCE) ||
			((val3 & DEV_VBUS_DEBOUNCE) && (adc != ADC_OPEN))) {
		/* USB */
		if (val1 & DEV_USB || val2 & DEV_T2_USB_MASK) {
			dev_info(&client->dev, "usb connect\n");
			pdata->callback(CABLE_TYPE_USB, FSA9485_ATTACHED);
			local_usbsw->last_state.attach = USB_CALL;

			if (usbsw->mansw) {
				ret = i2c_smbus_write_byte_data(client,
					FSA9485_REG_MANSW1, usbsw->mansw);

				if (ret < 0)
					dev_err(&client->dev,
						"%s: err %d\n", __func__, ret);
			}
		/* USB_CDP */
		} else if (val1 & DEV_USB_CHG) {
			dev_info(&client->dev, "usb_cdp connect\n");
#ifdef CONFIG_TSU6721_CDP_FIX
			pdata->callback(CABLE_TYPE_CDP, is_ti_muic() ? 
					TSU6721_ATTACHED : FSA9485_ATTACHED);
#else
			pdata->callback(CABLE_TYPE_CDP, FSA9485_ATTACHED);
#endif
			local_usbsw->last_state.attach = CDP_CALL;

			if (usbsw->mansw) {
				ret = i2c_smbus_write_byte_data(client,
					FSA9485_REG_MANSW1, usbsw->mansw);

				if (ret < 0)
					dev_err(&client->dev,
						"%s: err %d\n", __func__, ret);
			}

		/* UART */
		} else if (val1 & DEV_T1_UART_MASK || val2 & DEV_T2_UART_MASK) {
			uart_connecting = 1;
			dev_info(&client->dev, "uart connect\n");
			i2c_smbus_write_byte_data(client,
						FSA9485_REG_CTRL, 0x1E);
			pdata->callback(CABLE_TYPE_UARTOFF, FSA9485_ATTACHED);
			local_usbsw->last_state.attach = UART_CALL;

			if (usbsw->mansw) {
				ret = i2c_smbus_write_byte_data(client,
					FSA9485_REG_MANSW1, SW_UART);

				if (ret < 0)
					dev_err(&client->dev,
						"%s: err %d\n", __func__, ret);
			}
		/* CHARGER */
		} else if ((val1 & DEV_T1_CHARGER_MASK) ||
				(val3 & DEV_T3_CHARGER_MASK)) {
			dev_info(&client->dev, "charger connect\n");
			pdata->callback(CABLE_TYPE_AC, FSA9485_ATTACHED);
			local_usbsw->last_state.attach = CHARGER_CALL;
		/* for SAMSUNG OTG */
#if defined(CONFIG_USB_HOST_NOTIFY)
		} else if (val1 & DEV_USB_OTG) {
			dev_info(&client->dev, "otg connect\n");
			pdata->callback(CABLE_TYPE_OTG, FSA9485_ATTACHED);
			local_usbsw->last_state.attach = OTG_CALL;

			i2c_smbus_write_byte_data(client, FSA9485_REG_MANSW1,
					is_ti_muic() ? 0x25 : 0x27);
			i2c_smbus_write_byte_data(client,
						FSA9485_REG_MANSW2, 0x02);
			msleep(50);
			i2c_smbus_write_byte_data(client,
						FSA9485_REG_CTRL, 0x1a);
#endif
		/* JIG */
		} else if (val2 & DEV_T2_JIG_MASK) {
			dev_info(&client->dev, "jig connect\n");
			pdata->callback(CABLE_TYPE_JIG, FSA9485_ATTACHED);
			local_usbsw->last_state.attach = JIG_CALL;
		/* Desk Dock */
		} else if ((val2 & DEV_AV) || (val3 & DEV_AV_VBUS)) {
			if ((!is_ti_muic()) && ((adc & 0x1F) == 0x1A)) {
				pr_info("FSA Deskdock Attach\n");
				FSA9485_CheckAndHookAudioDock(1);
#if defined(CONFIG_VIDEO_MHL_V2)
				isDeskdockconnected = 1;
#endif
				i2c_smbus_write_byte_data(client,
					FSA9485_REG_RESERVED_20, 0x08);
			} else if (is_ti_muic()) {
				pr_info("TI Deskdock Attach\n");
				FSA9485_CheckAndHookAudioDock(1);
#if defined(CONFIG_VIDEO_MHL_V2)
				isDeskdockconnected = 1;
#endif
#if defined(CONFIG_USB_SWITCH_SMART_DOCK_ENABLE)
			} else {
				pr_info("MHL Attach\n");
				if (!is_ti_muic())
					i2c_smbus_write_byte_data(client,
						FSA9485_REG_RESERVED_20, 0x08);
#if defined(CONFIG_VIDEO_MHL_V2)
				DisableFSA9480Interrupts();
				if (!isDeskdockconnected) {
					if (!poweroff_charging)
						mhl_ret = mhl_onoff_ex(1);
					else
						pr_info("LPM mode, skip MHL sequence\n");
				}
			local_usbsw->last_state.device = MHL_CALL;
				if (mhl_ret != MHL_DEVICE &&
						(adc & 0x1F) == 0x1A) {
					FSA9485_CheckAndHookAudioDock(1);
					isDeskdockconnected = 1;
				}
				EnableFSA9480Interrupts();
#else
				FSA9485_CheckAndHookAudioDock(1);
#endif
#endif
			}
		/* Car Dock */
		} else if (val2 & DEV_JIG_UART_ON) {
			pdata->callback(CABLE_TYPE_CARDOCK,
				FSA9485_ATTACHED);
			local_usbsw->last_state.attach = CARDOCK_CALL;
			ret = i2c_smbus_write_byte_data(client,
					FSA9485_REG_MANSW1, SW_AUDIO);
			if (ret < 0)
				dev_err(&client->dev,
						"%s: err %d\n", __func__, ret);

			ret = i2c_smbus_read_byte_data(client,
					FSA9485_REG_CTRL);
			if (ret < 0)
				dev_err(&client->dev,
						"%s: err %d\n", __func__, ret);

			ret = i2c_smbus_write_byte_data(client,
					FSA9485_REG_CTRL, ret & ~CON_MANUAL_SW);
			if (ret < 0)
				dev_err(&client->dev,
						"%s: err %d\n", __func__, ret);
			usbsw->dock_attached = FSA9485_ATTACHED;

#if defined(CONFIG_USB_SWITCH_SMART_DOCK_ENABLE)
		/* SmartDock */
		} else if (val2 & DEV_SMARTDOCK) {
			usbsw->adc = adc;
			dev_info(&client->dev, "smart dock connect\n");

			usbsw->mansw = SW_DHOST;
			ret = i2c_smbus_write_byte_data(client,
					FSA9485_REG_MANSW1, SW_DHOST);
			if (ret < 0)
				dev_err(&client->dev,
						"%s: err %d\n", __func__, ret);
			ret = i2c_smbus_read_byte_data(client,
					FSA9485_REG_CTRL);
			if (ret < 0)
				dev_err(&client->dev,
						"%s: err %d\n", __func__, ret);
			ret = i2c_smbus_write_byte_data(client,
					FSA9485_REG_CTRL, ret & ~CON_MANUAL_SW);
			if (ret < 0)
				dev_err(&client->dev,
						"%s: err %d\n", __func__, ret);

			pdata->callback(CABLE_TYPE_SMART_DOCK,
				FSA9485_ATTACHED);
			local_usbsw->last_state.attach = SMARTDOCK_CALL;
#if defined(CONFIG_VIDEO_MHL_V2)
			mhl_onoff_ex(1);
#endif
#endif
#if defined(CONFIG_USB_HOST_NOTIFY)
		/* Audio Dock */
		} else if (val2 & DEV_AUDIO_DOCK) {
			usbsw->adc = adc;
			dev_info(&client->dev, "audio dock connect\n");

			usbsw->mansw = SW_DHOST;

			if (is_ti_muic())
				ret = i2c_smbus_write_byte_data(client,
					FSA9485_REG_MANSW1, SW_DHOST_TSU);
			else
				ret = i2c_smbus_write_byte_data(client,
					FSA9485_REG_MANSW1, SW_DHOST);

			if (ret < 0)
				dev_err(&client->dev,
						"%s: err %d\n", __func__, ret);

			ret = i2c_smbus_read_byte_data(client,
				FSA9485_REG_CTRL);
			if (ret < 0)
				dev_err(&client->dev,
						"%s: err %d\n", __func__, ret);
			ret = i2c_smbus_write_byte_data(client,
				FSA9485_REG_CTRL, ret & ~CON_MANUAL_SW);
			if (ret < 0)
				dev_err(&client->dev,
						"%s: err %d\n", __func__, ret);

			pdata->callback(CABLE_TYPE_AUDIO_DOCK,
				FSA9485_ATTACHED);
			local_usbsw->last_state.attach = AUDIODOCK_CALL;
#endif
		/* Incompatible */
		} else if (val3 & DEV_VBUS_DEBOUNCE) {
			dev_info(&client->dev,
					"Incompatible Charger connect\n");
			pdata->callback(CABLE_TYPE_INCOMPATIBLE,
					FSA9485_ATTACHED);
			local_usbsw->last_state.attach = INCOMPATIBLE_CALL;
		}
	/* Detached */
	} else {
		/* USB */
		if (usbsw->dev1 & DEV_USB ||
				usbsw->dev2 & DEV_T2_USB_MASK) {
			pdata->callback(CABLE_TYPE_USB, FSA9485_DETACHED);
			local_usbsw->last_state.detach = USB_CALL;
		} else if (usbsw->dev1 & DEV_USB_CHG) {
			pdata->callback(CABLE_TYPE_CDP, FSA9485_DETACHED);
			local_usbsw->last_state.detach = CDP_CALL;

		/* UART */
		} else if (usbsw->dev1 & DEV_T1_UART_MASK ||
				usbsw->dev2 & DEV_T2_UART_MASK) {
			pdata->callback(CABLE_TYPE_UARTOFF, FSA9485_DETACHED);
			local_usbsw->last_state.detach = UART_CALL;
			uart_connecting = 0;
			dev_info(&client->dev, "[FSA9485] uart disconnect\n");

		/* CHARGER */
		} else if ((usbsw->dev1 & DEV_T1_CHARGER_MASK) ||
				(usbsw->dev3 & DEV_T3_CHARGER_MASK)) {
			pdata->callback(CABLE_TYPE_AC, FSA9485_DETACHED);
			local_usbsw->last_state.detach = CHARGER_CALL;
		/* for SAMSUNG OTG */
		} else if (usbsw->dev1 & DEV_USB_OTG) {
			i2c_smbus_write_byte_data(client,
						FSA9485_REG_CTRL, 0x1E);
		/* JIG */
		} else if (usbsw->dev2 & DEV_T2_JIG_MASK) {
			pdata->callback(CABLE_TYPE_JIG, FSA9485_DETACHED);
			local_usbsw->last_state.detach = JIG_CALL;
		/* Desk Dock */
		} else if ((usbsw->dev2 & DEV_AV) || (usbsw->dev3 & DEV_AV_VBUS)) {

			pr_info("Deskdock Detach\n");

			if (!is_ti_muic())
				i2c_smbus_write_byte_data(client,
					FSA9485_REG_RESERVED_20, 0x04);
#if defined(CONFIG_VIDEO_MHL_V2)
			if (isDeskdockconnected)
				FSA9485_CheckAndHookAudioDock(0);
#if defined CONFIG_MHL_D3_SUPPORT
			mhl_onoff_ex(false);
			detached_status = 1;
#endif
			isDeskdockconnected = 0;
#else
			FSA9485_CheckAndHookAudioDock(0);
#endif
		/* Car Dock */
		} else if (usbsw->dev2 & DEV_JIG_UART_ON) {
			pdata->callback(CABLE_TYPE_CARDOCK,
				FSA9485_DETACHED_DOCK);
			local_usbsw->last_state.detach = CARDOCK_CALL;
			ret = i2c_smbus_read_byte_data(client,
					FSA9485_REG_CTRL);
			if (ret < 0)
				dev_err(&client->dev,
					"%s: err %d\n", __func__, ret);

			ret = i2c_smbus_write_byte_data(client,
					FSA9485_REG_CTRL,
					ret | CON_MANUAL_SW);
			if (ret < 0)
				dev_err(&client->dev,
					"%s: err %d\n", __func__, ret);
			usbsw->dock_attached = FSA9485_DETACHED;
#if defined(CONFIG_USB_SWITCH_SMART_DOCK_ENABLE)
		} else if (usbsw->adc == 0x10) {
			dev_info(&client->dev, "smart dock disconnect\n");
			ret = i2c_smbus_read_byte_data(client,
						FSA9485_REG_CTRL);
			if (ret < 0)
				dev_err(&client->dev,
					"%s: err %d\n", __func__, ret);

			ret = i2c_smbus_write_byte_data(client,
					FSA9485_REG_CTRL,
					ret | CON_MANUAL_SW);
			if (ret < 0)
				dev_err(&client->dev,
					"%s: err %d\n", __func__, ret);

			pdata->callback(CABLE_TYPE_SMART_DOCK,
				FSA9485_DETACHED);
			local_usbsw->last_state.detach = SMARTDOCK_CALL;
			usbsw->adc = 0;
#if defined(CONFIG_VIDEO_MHL_V2)
			mhl_onoff_ex(false);
#endif
#endif
		} else if (usbsw->dev2 == DEV_AUDIO_DOCK) {
			dev_info(&client->dev, "audio dock disconnect\n");
			ret = i2c_smbus_read_byte_data(client,
				    FSA9485_REG_CTRL);
			    if (ret < 0)
					dev_err(&client->dev,
					    "%s: err %d\n", __func__, ret);
				ret = i2c_smbus_write_byte_data(client,
						FSA9485_REG_CTRL,
						ret | CON_MANUAL_SW);
			    if (ret < 0)
					dev_err(&client->dev,
						"%s: err %d\n", __func__, ret);

			pdata->callback(CABLE_TYPE_AUDIO_DOCK,
					FSA9485_DETACHED);
			local_usbsw->last_state.detach = AUDIODOCK_CALL;
			usbsw->adc = 0;
		} else if (usbsw->dev3 & DEV_VBUS_DEBOUNCE) {
			dev_info(&client->dev,
					"Incompatible Charger disconnect\n");
			pdata->callback(CABLE_TYPE_INCOMPATIBLE,
					FSA9485_DETACHED);
			local_usbsw->last_state.detach = INCOMPATIBLE_CALL;
		}
	}
	usbsw->dev1 = val1;
	usbsw->dev2 = val2;
	usbsw->dev3 = val3;

	return adc;
}

static int fsa9485_detach_dev(struct fsa9485_usbsw *usbsw)
{
	int ret;
	struct fsa9485_platform_data *pdata = usbsw->pdata;
	struct i2c_client *client = usbsw->client;
#if defined(CONFIG_VIDEO_MHL_V2)
	u8 mhl_ret = 0;
#endif
	/* USB */
	if (usbsw->dev1 & DEV_USB ||
			usbsw->dev2 & DEV_T2_USB_MASK) {
		pdata->callback(CABLE_TYPE_USB, FSA9485_DETACHED);
		local_usbsw->last_state.detach = USB_CALL;
	} else if (usbsw->dev1 & DEV_USB_CHG) {
		pdata->callback(CABLE_TYPE_CDP, FSA9485_DETACHED);
		local_usbsw->last_state.detach = CDP_CALL;
		/* UART */
	} else if (usbsw->dev1 & DEV_T1_UART_MASK ||
			usbsw->dev2 & DEV_T2_UART_MASK) {
		pdata->callback(CABLE_TYPE_UARTOFF, FSA9485_DETACHED);
		local_usbsw->last_state.detach = UART_CALL;
		uart_connecting = 0;
		dev_info(&client->dev, "[FSA9485] uart disconnect\n");
		/* CHARGER */
	} else if ((usbsw->dev1 & DEV_T1_CHARGER_MASK) ||
			(usbsw->dev3 & DEV_T3_CHARGER_MASK)) {
		pdata->callback(CABLE_TYPE_AC, FSA9485_DETACHED);
		local_usbsw->last_state.detach = CHARGER_CALL;
	/* for SAMSUNG OTG */
	} else if (usbsw->dev1 & DEV_USB_OTG) {
		i2c_smbus_write_byte_data(client,
					FSA9485_REG_CTRL, 0x1E);
	/* JIG */
	} else if (usbsw->dev2 & DEV_T2_JIG_MASK) {
		pdata->callback(CABLE_TYPE_JIG, FSA9485_DETACHED);
		local_usbsw->last_state.detach = JIG_CALL;
	/* Desk Dock */
	} else if ((usbsw->dev2 & DEV_AV) || (usbsw->dev3 & DEV_AV_VBUS)) {
			pr_info("Deskdock/MHL Detach\n");
			if (!is_ti_muic())
			i2c_smbus_write_byte_data(client,
				FSA9485_REG_RESERVED_20, 0x04);
#if defined(CONFIG_VIDEO_MHL_V2)
		if (isDeskdockconnected)
			FSA9485_CheckAndHookAudioDock(0);
#if defined CONFIG_MHL_D3_SUPPORT
		mhl_onoff_ex(false);
		detached_status = 1;
#endif
		isDeskdockconnected = 0;
#else
		FSA9485_CheckAndHookAudioDock(0);
#endif
	/* Car Dock */
	} else if (usbsw->dev2 & DEV_JIG_UART_ON) {
		pdata->callback(CABLE_TYPE_CARDOCK,
			FSA9485_DETACHED_DOCK);
		local_usbsw->last_state.detach = CARDOCK_CALL;
		ret = i2c_smbus_read_byte_data(client,
				FSA9485_REG_CTRL);
		if (ret < 0)
			dev_err(&client->dev,
					"%s: err %d\n", __func__, ret);
		ret = i2c_smbus_write_byte_data(client,
				FSA9485_REG_CTRL,
				ret | CON_MANUAL_SW);
		if (ret < 0)
			dev_err(&client->dev,
				"%s: err %d\n", __func__, ret);
		usbsw->dock_attached = FSA9485_DETACHED;
	} else if (usbsw->adc == 0x10) {
		dev_info(&client->dev, "smart dock disconnect\n");
		ret = i2c_smbus_read_byte_data(client,
					FSA9485_REG_CTRL);
		if (ret < 0)
			dev_err(&client->dev,
				"%s: err %d\n", __func__, ret);
			ret = i2c_smbus_write_byte_data(client,
				FSA9485_REG_CTRL,
				ret | CON_MANUAL_SW);
		if (ret < 0)
			dev_err(&client->dev,
				"%s: err %d\n", __func__, ret);
			pdata->callback(CABLE_TYPE_SMART_DOCK,
			FSA9485_DETACHED);
		local_usbsw->last_state.detach = SMARTDOCK_CALL;
		usbsw->adc = 0;
#if defined(CONFIG_VIDEO_MHL_V2)
		mhl_onoff_ex(false);
#endif
	} else if (usbsw->dev2 == DEV_AUDIO_DOCK) {
		dev_info(&client->dev, "audio dock disconnect\n");
		ret = i2c_smbus_read_byte_data(client,
			    FSA9485_REG_CTRL);
		    if (ret < 0)
				dev_err(&client->dev,
				    "%s: err %d\n", __func__, ret);
			ret = i2c_smbus_write_byte_data(client,
					FSA9485_REG_CTRL,
					ret | CON_MANUAL_SW);
		    if (ret < 0)
				dev_err(&client->dev,
					"%s: err %d\n", __func__, ret);
			pdata->callback(CABLE_TYPE_AUDIO_DOCK,
				FSA9485_DETACHED);
		local_usbsw->last_state.detach = AUDIODOCK_CALL;
		usbsw->adc = 0;
	} else if (usbsw->dev3 & DEV_VBUS_DEBOUNCE) {
		dev_info(&client->dev,
				"Incompatible Charger disconnect\n");
		pdata->callback(CABLE_TYPE_INCOMPATIBLE,
				FSA9485_DETACHED);
		local_usbsw->last_state.detach = INCOMPATIBLE_CALL;
	} else
		pr_info("%s cannot detach due to invalid device type",
				__func__);

	usbsw->dev1 = 0;
	usbsw->dev2 = 0;
	usbsw->dev3 = 0;

	return 0;
}

static int fsa9485_check_dev(struct fsa9485_usbsw *usbsw)
{
	struct i2c_client *client = usbsw->client;
	int device_type1, device_type2, device_type3, device_type = 0;

	device_type1 = i2c_smbus_read_byte_data(client, FSA9485_REG_DEV_T1);
	if (device_type1 < 0) {
		dev_err(&client->dev, "%s: err %d\n", __func__, device_type1);
		return 0;
	}
	device_type2 = i2c_smbus_read_byte_data(client, FSA9485_REG_DEV_T2);
	if (device_type2 < 0) {
		dev_err(&client->dev, "%s: err %d\n", __func__, device_type2);
		return 0;
	}
	device_type3 = i2c_smbus_read_byte_data(client, FSA9485_REG_DEV_T3);
	if (device_type3 < 0) {
		dev_err(&client->dev, "%s: err %d\n", __func__, device_type3);
		return 0;
	}
	device_type = device_type1+(device_type2<<8)+(device_type3<<16);

	return device_type;
}

static irqreturn_t fsa9485_irq_thread(int irq, void *data)
{
	struct fsa9485_usbsw *usbsw = data;
	struct i2c_client *client = usbsw->client;
	int intr, intr2, detect;

	DisableFSA9480Interrupts();
	/* FSA9485 : Read interrupt -> Read Device
	 FSA9485 : Read Device -> Read interrupt */
	mutex_lock(&usbsw->mutex);
	if (is_ti_muic())
		msleep(50);
	pr_info("fsa9485_irq_thread is called\n");

	/* read and clear interrupt status bits */
	intr = i2c_smbus_read_byte_data(client, FSA9485_REG_INT1);
	dev_info(&client->dev, "%s: intr : 0x%x",
					__func__, intr & 0xff);

	intr2 = i2c_smbus_read_byte_data(client, FSA9485_REG_INT2);
	dev_info(&client->dev, "%s: intr2 : 0x%x\n",
					__func__, intr2 & 0xff);

	local_usbsw->last_state.int1 = intr;
	local_usbsw->last_state.int2 = intr2;

	if (intr & INT_OVP_EN)
		usbsw->pdata->oxp_callback(ENABLE);
	else if (intr & INT_OXP_DISABLE)
		usbsw->pdata->oxp_callback(DISABLE);

	if (intr == (INT_ATTACH + INT_DETACH)) {
		fsa9485_detach_dev(usbsw);
		EnableFSA9480Interrupts();
		return IRQ_HANDLED;
	}

	/* device detection */
	detect = fsa9485_detect_dev(usbsw);
	mutex_unlock(&usbsw->mutex);
	pr_info("%s: detect dev_adc: %x\n", __func__, detect);

	EnableFSA9480Interrupts();

	return IRQ_HANDLED;
}

static int fsa9485_irq_init(struct fsa9485_usbsw *usbsw)
{
	struct i2c_client *client = usbsw->client;
	int ret;

	if (client->irq) {
		ret = request_threaded_irq(client->irq, NULL,
			fsa9485_irq_thread, IRQF_TRIGGER_FALLING,
			"fsa9485 micro USB", usbsw);
		if (ret) {
			dev_err(&client->dev, "failed to reqeust IRQ\n");
			return ret;
		}

		ret = enable_irq_wake(client->irq);
		if (ret < 0)
			dev_err(&client->dev,
				"failed to enable wakeup src %d\n", ret);
	}

	return 0;
}

static void fsa9485_init_detect(struct work_struct *work)
{
	struct fsa9485_usbsw *usbsw = container_of(work,
			struct fsa9485_usbsw, init_work.work);
	int ret = 0;

	dev_info(&usbsw->client->dev, "%s\n", __func__);

	mutex_lock(&usbsw->mutex);
	fsa9485_detect_dev(usbsw);
	mutex_unlock(&usbsw->mutex);

	ret = fsa9485_irq_init(usbsw);
	if (ret)
		dev_info(&usbsw->client->dev,
				"failed to enable  irq init %s\n", __func__);

	ret = i2c_smbus_read_byte_data(usbsw->client, FSA9485_REG_INT1);
	dev_info(&usbsw->client->dev, "%s: intr1 : 0x%x\n",
		__func__, ret & 0xff);
	local_usbsw->last_state.int1 = ret;

	ret = i2c_smbus_read_byte_data(usbsw->client, FSA9485_REG_INT2);
	dev_info(&usbsw->client->dev, "%s: intr2 : 0x%x\n",
		__func__, ret & 0xff);
	local_usbsw->last_state.int2 = ret;
}

static int __devinit fsa9485_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct fsa9485_usbsw *usbsw;
	int ret = 0;
	struct device *switch_dev;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -EIO;

	usbsw = kzalloc(sizeof(struct fsa9485_usbsw), GFP_KERNEL);
	if (!usbsw) {
		dev_err(&client->dev, "failed to allocate driver data\n");
		kfree(usbsw);
		return -ENOMEM;
	}

	usbsw->client = client;
	usbsw->pdata = client->dev.platform_data;
	if (!usbsw->pdata)
		goto fail1;

	i2c_set_clientdata(client, usbsw);

	mutex_init(&usbsw->mutex);

	local_usbsw = usbsw;

	fsa9485_reg_init(usbsw);

	ret = sysfs_create_group(&client->dev.kobj, &fsa9485_group);
	if (ret) {
		dev_err(&client->dev,
				"failed to create fsa9485 attribute group\n");
		goto fail2;
	}

	/* make sysfs node /sys/class/sec/switch/usb_state */
	switch_dev = device_create(sec_class, NULL, 0, NULL, "switch");
	if (IS_ERR(switch_dev)) {
		pr_err("[FSA9485] Failed to create device (switch_dev)!\n");
		ret = PTR_ERR(switch_dev);
		goto fail2;
	}

	ret = device_create_file(switch_dev, &dev_attr_usb_state);
	if (ret < 0) {
		pr_err("[FSA9485] Failed to create file (usb_state)!\n");
		goto err_create_file_state;
	}

	ret = device_create_file(switch_dev, &dev_attr_adc);
	if (ret < 0) {
		pr_err("[FSA9485] Failed to create file (adc)!\n");
		goto err_create_file_adc;
	}

	ret = device_create_file(switch_dev, &dev_attr_reset_switch);
	if (ret < 0) {
		pr_err("[FSA9485] Failed to create file (reset_switch)!\n");
		goto err_create_file_reset_switch;
	}

	dev_set_drvdata(switch_dev, usbsw);
	/* fsa9485 dock init*/
	if (usbsw->pdata->dock_init)
		usbsw->pdata->dock_init();

	/* initial cable detection */
	INIT_DELAYED_WORK(&usbsw->init_work, fsa9485_init_detect);
	schedule_delayed_work(&usbsw->init_work, msecs_to_jiffies(2700));

	return 0;

err_create_file_reset_switch:
	device_remove_file(switch_dev, &dev_attr_reset_switch);
err_create_file_adc:
	device_remove_file(switch_dev, &dev_attr_adc);
err_create_file_state:
	device_remove_file(switch_dev, &dev_attr_usb_state);
fail2:
	if (client->irq)
		free_irq(client->irq, usbsw);
fail1:
	mutex_destroy(&usbsw->mutex);
	i2c_set_clientdata(client, NULL);
	kfree(usbsw);
	return ret;
}

static int __devexit fsa9485_remove(struct i2c_client *client)
{
	struct fsa9485_usbsw *usbsw = i2c_get_clientdata(client);
	cancel_delayed_work(&usbsw->init_work);
	if (client->irq) {
		disable_irq_wake(client->irq);
		free_irq(client->irq, usbsw);
	}
	mutex_destroy(&usbsw->mutex);
	i2c_set_clientdata(client, NULL);

	sysfs_remove_group(&client->dev.kobj, &fsa9485_group);
	kfree(usbsw);
	return 0;
}

static int fsa9485_resume(struct i2c_client *client)
{
	struct fsa9485_usbsw *usbsw = i2c_get_clientdata(client);

/* add for fsa9485_irq_thread i2c error during wakeup */
	fsa9485_check_dev(usbsw);

	i2c_smbus_read_byte_data(client, FSA9485_REG_INT1);
	i2c_smbus_read_byte_data(client, FSA9485_REG_INT2);

	/* device detection */
	mutex_lock(&usbsw->mutex);
	fsa9485_detect_dev(usbsw);
	mutex_unlock(&usbsw->mutex);

	return 0;
}


static const struct i2c_device_id fsa9485_id[] = {
	{"fsa9485", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, fsa9485_id);

static struct i2c_driver fsa9485_i2c_driver = {
	.driver = {
		.name = "fsa9485",
	},
	.probe = fsa9485_probe,
	.remove = __devexit_p(fsa9485_remove),
	.resume = fsa9485_resume,
	.id_table = fsa9485_id,
};

static int __init fsa9485_init(void)
{
	return i2c_add_driver(&fsa9485_i2c_driver);
}
module_init(fsa9485_init);

static void __exit fsa9485_exit(void)
{
	i2c_del_driver(&fsa9485_i2c_driver);
}
module_exit(fsa9485_exit);

MODULE_AUTHOR("Minkyu Kang <mk7.kang@samsung.com>");
MODULE_DESCRIPTION("FSA9485 USB Switch driver");
MODULE_LICENSE("GPL");
