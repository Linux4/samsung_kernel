/*
 * extcon-s2mm001b.c - Samsung S2MM001B extcon driver to support USB switches
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd
 * Author: Seonggyu Park <seongyu.park@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#define DEBUG

#include <linux/delay.h>
#include <linux/edge_wakeup_mmp.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/regmap.h>
#include <linux/switch.h>
#include <linux/pm.h>
#include <linux/extcon.h>

#include <linux/extcon/extcon-s2mm001b.h>
#include <linux/extcon/extcon-sec.h>

enum s2mm001b_adc_acc_type {
	ADC_CATKIT_T1 = 0x17,
	ADC_CATKIT_T2 = 0x1B,
};

const char *s2mm001b_extcon_cable[CABLE_NAME_MAX + 1] = {
	[S2MM001B_CABLE_OTG]		= "OTG",
	[S2MM001B_CABLE_VIDEO]		= "VIDEO",
	[S2MM001B_CABLE_MHL]		= "MHL",
	[S2MM001B_CABLE_PPD]		= "Phone Powered Device",
	[S2MM001B_CABLE_TTY_CON]	= "TTY-Converter",
	[S2MM001B_CABLE_UART]		= "UART",
	[S2MM001B_CABLE_CARKIT_T1]	= "Carkit Type1",
	[S2MM001B_CABLE_JIG_USB_OFF]	= "JIG USB Off",
	[S2MM001B_CABLE_JIG_USB_ON]	= "JIG USB On",
	[S2MM001B_CABLE_AV]		= "AV",
	[S2MM001B_CABLE_CARKIT_T2]	= "Carkit Type2",
	[S2MM001B_CABLE_JIG_UART_OFF]	= "JIG UART Off",
	[S2MM001B_CABLE_JIG_UART_ON]	= "JIG UART On",
	[S2MM001B_CABLE_USB]		= "USB",
	[S2MM001B_CABLE_CDP]		= "CDP",
	[S2MM001B_CABLE_DCP]		= "DCP",
	[S2MM001B_CABLE_APPLE_CHG]	= "Apple charger",
	[S2MM001B_CABLE_U200]		= "U200",

	[S2MM001B_CABLE_UNKNOWN_VB]	= "Unknown with VBUS",
};


enum s2mm001b_reg {
	S2MM001B_REG_DEVID		= 0x01,
	S2MM001B_REG_CTRL1		= 0x02,
	S2MM001B_REG_INT1		= 0x03,
	S2MM001B_REG_INT2		= 0x04,
	S2MM001B_REG_INTMASK1		= 0x05,
	S2MM001B_REG_INTMASK2		= 0x06,
	S2MM001B_REG_ADC		= 0x07,
	S2MM001B_REG_TIMING1		= 0x08,
	S2MM001B_REG_TIMING2		= 0x09,
	S2MM001B_REG_DEV_T1		= 0x0a,
	S2MM001B_REG_DEV_T2		= 0x0b,
	S2MM001B_REG_BUTTON1		= 0X0c,
	S2MM001B_REG_BUTTON2		= 0X0d,
	S2MM001B_REG_MANSW1		= 0x13,
	S2MM001B_REG_MANSW2		= 0x14,
	S2MM001B_REG_DEV_T3		= 0x15,
	S2MM001B_REG_RESET		= 0x1B,
	S2MM001B_REG_TIMING3		= 0x20,
	S2MM001B_REG_OCP_SET		= 0x22,
	S2MM001B_REG_CTRL2		= 0x23,
	S2MM001B_REG_ADC_MODE		= 0x25,
	S2MM001B_REG_HIDDEN		= 0x2B,
	S2MM001B_REG_END,
};

/* S2MM001B Control register */
#define CTRL1_SWITCH_OPEN		(1 << 4)
#define CTRL1_RAW_DATA			(1 << 3)
#define CTRL1_MANUAL_SW			(1 << 2)
#define CTRL1_WAIT			(1 << 1)
#define CTRL1_INT_MASK			(1 << 0)
#define CTRL1_DEF_INT			0x1F

#define RESET_BIT			(1 << 0)

struct s2mm001b_device_id {
	u8 version;
	u8 vendor;
};

struct s2mm001b_reg_value {
	u8 int1;
	u8 int2;
	u8 adc;
	u8 devt1;
	u8 devt2;
	u8 devt3;
	u8 but1;
	u8 but2;
	u8 stat;
};

struct s2mm001b_muic_irq {
	unsigned int irq;
	const char *name;
	unsigned int virq;
};

struct s2mm001b_chip {
	struct device *dev;
	struct i2c_client *client;

	struct mutex mutex;
	struct work_struct work;

	struct regmap *regmap;
	struct extcon_dev *edev;
	struct switch_dev *sdev;

	struct s2mm001b_muic_irq *muic_irqs;
	struct s2mm001b_platform_data *pdata;
	struct s2mm001b_reg_value reg_value;
	struct s2mm001b_device_id id;
};

/* S2MM001B Interrupt 1 register */
#define INT1_VBUS_OVP_OCP_CLR		(1 << 7)
#define INT1_VBUS_OCP			(1 << 6)
#define INT1_VBUS_OVP			(1 << 5)
#define INT1_LKR			(1 << 4) /* Long Key Released */
#define INT1_LKP			(1 << 3) /* Long Key Pressed */
#define INT1_KP				(1 << 2) /* Key Pressed */
#define INT1_DETACH			(1 << 1)
#define INT1_ATTACH			(1 << 0)

/* S2MM001B Interrupt 2 register */
#define INT2_VBUS_OUT			(1 << 7)
#define INT2_AV_CHARGE			(1 << 6)
#define INT2_MHDL			(1 << 5)
#define INT2_STUCK_RCV			(1 << 4)
#define INT2_STUCK			(1 << 3)
#define INT2_ADC_CHANGE			(1 << 2)
#define INT2_RSV_ATTACH			(1 << 1) /* Reseved Accessory */
#define INT2_VBUS_DET			(1 << 0)

/* S2MM001B Hidden register */
#define HID_ONESHOT			(1 << 2)

enum s2mm001b_irqs {
	/* INT1 */
	S2MM001B_IRQ_INT1_ATTACH,
	S2MM001B_IRQ_INT1_DETACH,
	S2MM001B_IRQ_INT1_KP,
	S2MM001B_IRQ_INT1_LKP,
	S2MM001B_IRQ_INT1_LKR,
	S2MM001B_IRQ_INT1_VBUS_OVP,
	S2MM001B_IRQ_INT1_VBUS_OCP,
	S2MM001B_IRQ_INT1_VBUS_OVP_OCV_CLR,

	/* INT2 */
	S2MM001B_IRQ_INT2_VBUS_DET,
	S2MM001B_IRQ_INT2_RSV_ATTACH,
	S2MM001B_IRQ_INT2_ADC_CHANGE,
	S2MM001B_IRQ_INT2_STUCK,
	S2MM001B_IRQ_INT2_STUCK_RCV,
	S2MM001B_IRQ_INT2_MHDL,
	S2MM001B_IRQ_INT2_AV_CHARGER,
	S2MM001B_IRQ_INT2_VBUS_OUT,
};

static struct s2mm001b_muic_irq s2mm001b_muic_irqs[] = {
	{ S2MM001B_IRQ_INT1_ATTACH,		"attach" },
	{ S2MM001B_IRQ_INT1_DETACH,		"detach" },
	{ S2MM001B_IRQ_INT1_KP,			"key press" },
	{ S2MM001B_IRQ_INT1_LKP,		"long key press" },
	{ S2MM001B_IRQ_INT1_LKR,		"long key release" },
	{ S2MM001B_IRQ_INT1_VBUS_OVP,		"OVP detect" },
	{ S2MM001B_IRQ_INT1_VBUS_OCP,		"OCP detect" },
	{ S2MM001B_IRQ_INT1_VBUS_OVP_OCV_CLR,	"OVP or OCP clear" },
	{ S2MM001B_IRQ_INT2_VBUS_DET,		"VBUS in" },
	{ S2MM001B_IRQ_INT2_RSV_ATTACH,		"reserved accessory attach" },
	{ S2MM001B_IRQ_INT2_ADC_CHANGE,		"ADC change" },
	{ S2MM001B_IRQ_INT2_STUCK,		"stuck key" },
	{ S2MM001B_IRQ_INT2_STUCK_RCV,		"stuck key recovered" },
	{ S2MM001B_IRQ_INT2_MHDL,		"MHDL attach" },
	{ S2MM001B_IRQ_INT2_AV_CHARGER,		"AV with VBUS attach" },
	{ S2MM001B_IRQ_INT2_VBUS_OUT,		"VBUS out" },
};

/* for API support */
static struct s2mm001b_chip *usbsw_api;

static struct reg_default s2mm001b_reg_default[] = {
	{
		.reg = S2MM001B_REG_CTRL1,
		.def = CTRL1_SWITCH_OPEN
			| !CTRL1_RAW_DATA
			| CTRL1_MANUAL_SW
			| CTRL1_WAIT
			| !CTRL1_INT_MASK,
	}, {
		.reg = S2MM001B_REG_INTMASK1,
		.def = INT1_VBUS_OVP_OCP_CLR
			| INT1_VBUS_OCP
			| INT1_VBUS_OVP
			| INT1_LKR
			| INT1_LKP
			| INT1_KP
			| !INT1_DETACH
			| !INT1_ATTACH,
	}, {
		.reg = S2MM001B_REG_INTMASK2,
		.def = INT2_AV_CHARGE
			| INT2_MHDL
			| INT2_STUCK_RCV
			| INT2_STUCK
			| INT2_ADC_CHANGE
			| INT2_RSV_ATTACH
			| !INT2_VBUS_DET,
	}, {
		.reg = S2MM001B_REG_HIDDEN,
		.def = 0x04 & !HID_ONESHOT,
	},
};

static bool s2mm001b_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case S2MM001B_REG_INT1:
	case S2MM001B_REG_INT2:
		return true;
	default:
		break;
	}
	return false;
}

static struct regmap_config s2mm001b_regmap_config = {
	.reg_bits	= 8,
	.val_bits	= 8,
	.volatile_reg = s2mm001b_volatile_reg,
	.max_register = S2MM001B_REG_END,
};

static int read_reg(struct s2mm001b_chip *usbsw, u8 reg, u8 *data)
{
	int ret;
	unsigned int buf;

	ret = regmap_read(usbsw->regmap, (unsigned)reg, &buf);
	if (ret < 0) {
		dev_err(usbsw->dev, "I2C Read REG[0x%.2X] failed\n", reg);
		return ret;
	}

	*data = (u8)buf;

	dev_dbg(usbsw->dev, "I2C Read REG[0x%.2X] DATA[0x%.2X]\n", reg, *data);
	return ret;
}

static int write_reg(struct s2mm001b_chip *usbsw, u8 reg, u8 data)
{
	int ret;

	ret = regmap_write(usbsw->regmap, (unsigned)reg, (unsigned) data);
	if (ret < 0) {
		dev_err(usbsw->dev, "I2C write REG[0x%.2X] failed\n", reg);
		return ret;
	}

	dev_dbg(usbsw->dev, "I2C Write REG[0x%.2X] DATA[0x%.2X]\n", reg, data);
	return ret;
}

static int s2mm001b_reg_init(struct s2mm001b_chip *usbsw)
{
	int ret;

	ret = regmap_multi_reg_write(usbsw->regmap, s2mm001b_reg_default,
					ARRAY_SIZE(s2mm001b_reg_default));
	if (IS_ERR_VALUE(ret))
		dev_err(usbsw->dev, "failed to registers init.\n");

	return ret;
}

/*
 * Manual Switch
 * D- [7:5] / D+ [4:2] / CHARGER[1] / OTGEN[0]
 * 000: Open all / 001: USB / 010: AUDIO / 011: UART /
 */
#define MANUAL_SW1_DM_SHIFT		5
#define MANUAL_SW1_DP_SHIFT		2
#define MANUAL_SW1_D_OPEN		(0x0)
#define MANUAL_SW1_D_USB		(0x1)
#define MANUAL_SW1_D_AUDIO		(0x2)
#define MANUAL_SW1_D_UART		(0x3)
#define MANUAL_SW1_V_CHARGER		(0x2)
#define MANUAL_SW1_V_OTGEN		(0x1)
#define MANUAL_SW1_V_OFF		(0x0)

/* MANUAL SWITCH 1 */
#define CON_TO_USB	((MANUAL_SW1_D_USB << MANUAL_SW1_DM_SHIFT) | \
				(MANUAL_SW1_D_USB << MANUAL_SW1_DP_SHIFT) | \
				(MANUAL_SW1_V_CHARGER))
#define CON_TO_OTG	((MANUAL_SW1_D_USB << MANUAL_SW1_DM_SHIFT) | \
				(MANUAL_SW1_D_USB << MANUAL_SW1_DP_SHIFT) | \
				(MANUAL_SW1_V_OTGEN))
#define CON_TO_UART	((MANUAL_SW1_D_UART << MANUAL_SW1_DM_SHIFT) | \
				(MANUAL_SW1_D_UART << MANUAL_SW1_DP_SHIFT) | \
				(MANUAL_SW1_V_OFF))
#define CON_TO_OPEN	((MANUAL_SW1_D_OPEN << MANUAL_SW1_DM_SHIFT) | \
				(MANUAL_SW1_D_OPEN << MANUAL_SW1_DP_SHIFT) | \
				(MANUAL_SW1_V_OFF))

enum { USB = 0, OTG, UART, OPEN, AUTO };

static int __s2mm0001b_set_path(struct s2mm001b_chip *usbsw,
							const unsigned path)
{
	const char *connect[] = {"USB", "OTG", "UART", "OPEN", "AUTO"};
	bool chagne_ok, man_mode;
	int ret = 0;

	disable_irq(usbsw->client->irq);
	mutex_lock(&usbsw->mutex);

	man_mode = (path == AUTO) ? false : true;

	ret = regmap_update_bits_check(usbsw->regmap,
					S2MM001B_REG_CTRL1,
					CTRL1_MANUAL_SW,
				man_mode ? !CTRL1_MANUAL_SW : CTRL1_MANUAL_SW,
					&chagne_ok);
	if (IS_ERR_VALUE(ret)) {
		dev_err(usbsw->dev, "change to manual mode failed\n");
		goto fail_exit;
	}

	if (!chagne_ok)
		dev_warn(usbsw->dev, "already be changed to %s\n",
								connect[path]);

	switch (path) {
	case USB:
		ret = write_reg(usbsw, S2MM001B_REG_MANSW1, CON_TO_USB);
		break;
	case OTG:
		ret = write_reg(usbsw, S2MM001B_REG_MANSW1, CON_TO_OTG);
		break;
	case UART:
		ret = write_reg(usbsw, S2MM001B_REG_MANSW1, CON_TO_UART);
		break;
	case OPEN:
		ret = write_reg(usbsw, S2MM001B_REG_MANSW1, CON_TO_OPEN);
		break;
	case AUTO:
	default:
		break;
	}

	if (IS_ERR_VALUE(ret)) {
		dev_err(usbsw->dev, "manually change path to %s failed\n",
								connect[path]);
		goto fail_exit;
	}

	mutex_unlock(&usbsw->mutex);
	enable_irq(usbsw->client->irq);

	dev_info(usbsw->dev, "manually path to %s mode\n", connect[path]);

	return ret;

fail_exit:
	mutex_unlock(&usbsw->mutex);
	enable_irq(usbsw->client->irq);
	return ret;
}

/**
 * s2mm001b_set_path() - Change connector path manually
 * @path : path name of you want change ("USB", "UART". "OPEN")
 */
int s2mm001b_set_path(const char *path)
{
	unsigned to_path;

	if (!usbsw_api)
		return -1;

	if (!strncmp("USB", path, 4)) {
		to_path = USB;
	} else if (!strncmp("OTG", path, 4)) {
		to_path = OTG;
	} else if (!strncmp("UART", path, 4)) {
		to_path = UART;
	} else if (!strncmp("OPEN", path, 4)) {
		to_path = OPEN;
	} else if (!strncmp("AUTO", path, 4)) {
		to_path = AUTO;
	} else {
		dev_err(usbsw_api->dev, "invalid parameter: %s", path);
		return -1;
	}

	return __s2mm0001b_set_path(usbsw_api, to_path);
}
EXPORT_SYMBOL_GPL(s2mm001b_set_path);

static int s2mm001b_restore(struct s2mm001b_chip *usbsw)
{
	struct i2c_client *client = usbsw->client;

	u8 sintm1, sintm2, smansw1, sctrl;

	disable_irq(client->irq);

	mutex_lock(&usbsw->mutex);

	read_reg(usbsw, S2MM001B_REG_INTMASK1, &sintm1);
	read_reg(usbsw, S2MM001B_REG_INTMASK2, &sintm2);
	read_reg(usbsw, S2MM001B_REG_MANSW1, &smansw1);
	read_reg(usbsw, S2MM001B_REG_CTRL1, &sctrl);

	write_reg(usbsw, S2MM001B_REG_RESET, 1);
	msleep(20);

	write_reg(usbsw, S2MM001B_REG_INTMASK1, sintm1);
	write_reg(usbsw, S2MM001B_REG_INTMASK2, sintm2);
	write_reg(usbsw, S2MM001B_REG_MANSW1, smansw1);
	write_reg(usbsw, S2MM001B_REG_CTRL1, sctrl);

	mutex_unlock(&usbsw->mutex);

	enable_irq(client->irq);

	dev_info(usbsw->dev, "IC restored!\n");

	return 0;
}

/* S2MM001B Device Type 1 register */
#define DEV_TYPE1_USB_OTG		(1 << 7)
#define DEV_TYPE1_DEDICATED_CHG		(1 << 6)
#define DEV_TYPE1_CDP			(1 << 5)
#define DEV_TYPE1_CARKIT		(1 << 4)
#define DEV_TYPE1_UART			(1 << 3)
#define DEV_TYPE1_USB			(1 << 2)
#define DEV_TYPE1_AUDIO_2		(1 << 1)
#define DEV_TYPE1_AUDIO_1		(1 << 0)
#define DEV_TYPE1_USB_TYPES		(DEV_TYPE1_USB_OTG | \
						DEV_TYPE1_USB)
#define DEV_TYPE1_CHG_TYPES		(DEV_TYPE1_DEDICATED_CHG | \
						DEV_TYPE1_CDP)

/* S2MM001B Device Type 2 register */
#define DEV_TYPE2_AV			(1 << 6)
#define DEV_TYPE2_TTY			(1 << 5)
#define DEV_TYPE2_PPD			(1 << 4)
#define DEV_TYPE2_JIG_UART_OFF		(1 << 3)
#define DEV_TYPE2_JIG_UART_ON		(1 << 2)
#define DEV_TYPE2_JIG_USB_OFF		(1 << 1)
#define DEV_TYPE2_JIG_USB_ON		(1 << 0)
#define DEV_TYPE2_JIG_USB_TYPES		(DEV_TYPE2_JIG_USB_OFF | \
						DEV_TYPE2_JIG_USB_ON)
#define DEV_TYPE2_JIG_UART_TYPES	(DEV_TYPE2_JIG_UART_OFF | \
						DEV_TYPE2_JIG_UART_ON)
#define DEV_TYPE2_JIG_TYPES		(DEV_TYPE2_JIG_UART_TYPES | \
						DEV_TYPE2_JIG_USB_TYPES)

/* S2MM001B Device Type 3 register */
#define DEV_TYPE3_AV75			(1 << 7) /* A/V Cable 75 ohm */
#define DEV_TYPE3_U200_CHG		(1 << 6)
#define DEV_TYPE3_APPLE_CHG		(1 << 5)
#define DEV_TYPE3_AV_WITH_VBUS		(1 << 4)
#define DEV_TYPE3_NO_STD_CHG		(1 << 2)
#define DEV_TYPE3_VBUS_VALID		(1 << 1)
#define DEV_TYPE3_MHL			(1 << 0)
#define DEV_TYPE3_CHG_TYPE		(DEV_TYPE3_U200_CHG | \
						DEV_TYPE3_NO_STD_CHG | \
						DEV_TYPE3_APPLE_CHG)

static unsigned s2mm001b_detect_cable(struct s2mm001b_chip *usbsw)
{
	unsigned cable_idx = S2MM001B_CABLE_NONE;
	u8 devt1, devt2, devt3, adc;

	read_reg(usbsw, S2MM001B_REG_DEV_T1, &devt1);
	read_reg(usbsw, S2MM001B_REG_DEV_T2, &devt2);
	read_reg(usbsw, S2MM001B_REG_DEV_T3, &devt3);
	read_reg(usbsw, S2MM001B_REG_ADC, &adc);

	if (devt1 & DEV_TYPE1_USB_OTG) {
		cable_idx = S2MM001B_CABLE_OTG;
	} else if (devt1 & DEV_TYPE1_DEDICATED_CHG) {
		cable_idx = S2MM001B_CABLE_DCP;
	} else if (devt1 & DEV_TYPE1_CDP) {
		cable_idx = S2MM001B_CABLE_CDP;
	} else if (devt1 & DEV_TYPE1_CARKIT) {
		switch (adc) {
		case ADC_CATKIT_T1:
		cable_idx = S2MM001B_CABLE_CARKIT_T1;
		case ADC_CATKIT_T2:
		cable_idx = S2MM001B_CABLE_CARKIT_T2;
		}
	} else if (devt1 & DEV_TYPE1_UART) {
		cable_idx = S2MM001B_CABLE_UART;
	} else if (devt1 & DEV_TYPE1_USB) {
		cable_idx = S2MM001B_CABLE_USB;
	} else if (devt1 & DEV_TYPE1_AUDIO_2) {
		;
	} else if (devt1 & DEV_TYPE1_AUDIO_1) {
		;
	} else if (devt2 & DEV_TYPE2_AV) {
		cable_idx = S2MM001B_CABLE_AV;
	} else if (devt2 & DEV_TYPE2_TTY) {
		cable_idx = S2MM001B_CABLE_TTY_CON;
	} else if (devt2 & DEV_TYPE2_PPD) {
		cable_idx = S2MM001B_CABLE_PPD;
	} else if (devt2 & DEV_TYPE2_JIG_UART_OFF) {
		cable_idx = S2MM001B_CABLE_JIG_UART_OFF;
	} else if (devt2 & DEV_TYPE2_JIG_UART_ON) {
		cable_idx = S2MM001B_CABLE_JIG_UART_ON;
	} else if (devt2 & DEV_TYPE2_JIG_USB_OFF) {
		cable_idx = S2MM001B_CABLE_JIG_USB_OFF;
	} else if (devt2 & DEV_TYPE2_JIG_USB_ON) {
		cable_idx = S2MM001B_CABLE_JIG_USB_ON;
	} else if (devt3 & DEV_TYPE3_AV75) {
		;
	} else if (devt3 & DEV_TYPE3_U200_CHG) {
		cable_idx = S2MM001B_CABLE_U200;
	} else if (devt3 & DEV_TYPE3_APPLE_CHG) {
		cable_idx = S2MM001B_CABLE_APPLE_CHG;
	} else if (devt3 & DEV_TYPE3_AV_WITH_VBUS) {
		;
	} else if (devt3 & DEV_TYPE3_NO_STD_CHG) {
		cable_idx = S2MM001B_CABLE_UNKNOWN_VB;
	} else if (devt3 & DEV_TYPE3_VBUS_VALID) {
		cable_idx = S2MM001B_CABLE_UNKNOWN_VB;
	} else if (devt3 & DEV_TYPE3_MHL) {
		cable_idx = S2MM001B_CABLE_MHL;
	}

	if (devt3 & DEV_TYPE3_VBUS_VALID) {
		dev_info(usbsw->dev, "VBUS Appeared\n");
		blocking_notifier_call_chain(&sec_vbus_notifier, CABLE_VBUS_OCCURRENCE, NULL);
	}

	usbsw->reg_value.devt1 = devt1;
	usbsw->reg_value.devt2 = devt2;
	usbsw->reg_value.devt3 = devt3;
	usbsw->reg_value.adc = adc;

	return cable_idx;
}

static void s2mm001b_cable_handler(struct s2mm001b_chip *usbsw, bool attach)
{
	static unsigned prev_cable_idx = S2MM001B_CABLE_NONE;
	const char **cable_names = usbsw->edev->supported_cable;
	unsigned cable_idx;

	if (!cable_names)
		return;

	if (attach == true) {
		cable_idx = s2mm001b_detect_cable(usbsw);
		if (cable_idx == S2MM001B_CABLE_NONE) {
			dev_warn(usbsw->dev, "couldn't find any cables\n");
			return;
		}
		prev_cable_idx = cable_idx;
	} else {
		if (unlikely(prev_cable_idx == S2MM001B_CABLE_NONE)) {
			dev_warn(usbsw->dev, "The detach interrupt occured! But couldn't find previous cable type\n");
			return;
		}
		cable_idx = prev_cable_idx;
	}

	dev_dbg(usbsw->dev, "%s connector %s\n",
					cable_names[cable_idx],
					attach ? "attached" : "detached");

	extcon_set_cable_state(usbsw->edev, cable_names[cable_idx], attach);

	return;
}

static void s2mm001b_work_cb(struct work_struct *work)
{
	u8 intr1, intr2, val;
	struct s2mm001b_chip *usbsw =
				container_of(work, struct s2mm001b_chip, work);

	msleep(50); /* wait for MUIC set internal register values */

	mutex_lock(&usbsw->mutex);
	disable_irq(usbsw->client->irq);

	/* EOS Test recovery */
	read_reg(usbsw, S2MM001B_REG_CTRL1, &val);
	if (val == CTRL1_DEF_INT) {
		dev_warn(usbsw->dev, "Detected IC has been reset!");
		if (IS_ERR_VALUE(s2mm001b_reg_init(usbsw)))
			dev_err(usbsw->dev, "Recovery failed");
		else
			dev_warn(usbsw->dev, "Recovery done");
	}

	/* ESD handling */
	read_reg(usbsw, S2MM001B_REG_RESET, &val);
	if (val == RESET_BIT)
		s2mm001b_reg_init(usbsw);

	/* Read and Clear Interrupt1/2 */
	read_reg(usbsw, S2MM001B_REG_INT1, &intr1);
	read_reg(usbsw, S2MM001B_REG_INT2, &intr2);

	/*
	* s2mm001b raises mutually exclusiv VBUS_DET/VBUS_OUT interrupts
	* with ATTACH/DETACH interrupts
	*/
	if (intr1 & INT1_DETACH)
		s2mm001b_cable_handler(usbsw, false);
	else if (intr1 & INT1_ATTACH || intr2 & INT2_RSV_ATTACH)
		s2mm001b_cable_handler(usbsw, true);
	else if (intr2 & INT2_VBUS_DET) {
		dev_info(usbsw->dev, "VBUS Appeared\n");
		blocking_notifier_call_chain(&sec_vbus_notifier, CABLE_VBUS_OCCURRENCE, NULL);
	}
	else if (intr2 & INT2_VBUS_OUT || (intr2 & INT2_VBUS_OUT && intr1 & INT1_DETACH)) {
		dev_info(usbsw->dev, "VBUS Disappeared\n");
		blocking_notifier_call_chain(&sec_vbus_notifier, CABLE_VBUS_NONE, NULL);
	}

	enable_irq(usbsw->client->irq);
	mutex_unlock(&usbsw->mutex);
}

static irqreturn_t s2mm001b_irq_handler(int irq, void *data)
{
	struct s2mm001b_chip *usbsw = data;

	dev_dbg(usbsw->dev, "%s", __func__);
	schedule_work(&usbsw->work);

	return IRQ_HANDLED;
}

#ifdef CONFIG_PM_SLEEP
static int s2mm001b_suspend(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct s2mm001b_chip *usbsw = i2c_get_clientdata(i2c);

	dev_info(usbsw->dev, "suspend\n");

	return 0;
}

static int s2mm001b_resume(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct s2mm001b_chip *usbsw = i2c_get_clientdata(i2c);
	int ret = 0;
	u8 val;

	/* EOS Test recovery */
	read_reg(usbsw, S2MM001B_REG_CTRL1, &val);
	if (val == CTRL1_DEF_INT) {
		dev_warn(usbsw->dev, "Detected IC has been reset!");
		ret = s2mm001b_reg_init(usbsw);
		if (IS_ERR_VALUE(ret))
			dev_err(usbsw->dev, "Recovery failed");
		else
			dev_warn(usbsw->dev, "Recovery done");
	}

	dev_info(usbsw->dev, "resume\n");

	return ret;
}
#endif

static SIMPLE_DEV_PM_OPS(s2mm001b_pm_ops, s2mm001b_suspend, s2mm001b_resume);

static const struct of_device_id s2mm001b_dt_ids[] = {
	{ .compatible = "samsung,s2mm001b" },
	{},
};

MODULE_DEVICE_TABLE(of, s2mm001b_dt_ids);

static int s2mm001b_probe_dt(struct s2mm001b_chip *usbsw, struct device_node *np)
{
	int ret, muic_int;
	const struct of_device_id *match;

	match = of_match_device(s2mm001b_dt_ids, usbsw->dev);
	if (!match)
		return -EINVAL;

	muic_int = of_get_named_gpio(np, "muic-int", 0);
	if (muic_int < 0) {
		dev_err(usbsw->dev, "%s: of_get_named_gpio failed: %d\n", __func__, muic_int);
		return -EINVAL;
	}

	ret = request_mfp_edge_wakeup(muic_int, NULL, NULL, usbsw->dev);
	if (ret) {
		dev_err(usbsw->dev, "%s: failed to request edge wakeup.\n", __func__);
		return -EINVAL;
	}

	return 0;
}

#define VERSION_MASK			0xF8
#define VENDOR_MASK			0x07

static __init int s2mm001b_probe(struct i2c_client *client,
					const struct i2c_device_id *id)
{
	struct device_node *np = client->dev.of_node;
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct s2mm001b_chip *usbsw;
	struct s2mm001b_platform_data *pdata = client->dev.platform_data;
	int ret = 0;
	u8 device_id;

	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "Not compatible i2c function\n");
		return -EIO;
	}

	usbsw = kzalloc(sizeof(struct s2mm001b_chip), GFP_KERNEL);
	if (!usbsw) {
		dev_err(&client->dev, "failed to allocate driver data\n");
		return -ENOMEM;
	}

	usbsw->dev = &client->dev;
	usbsw->client = client;
	usbsw->pdata = pdata;

	i2c_set_clientdata(client, usbsw);
	mutex_init(&usbsw->mutex);
	INIT_WORK(&usbsw->work, s2mm001b_work_cb);

	usbsw->regmap = devm_regmap_init_i2c(client, &s2mm001b_regmap_config);
	if (!usbsw->regmap) {
		ret = PTR_ERR(usbsw->regmap);
		dev_err(usbsw->dev,
				"failed to allocate register map: %d\n", ret);
		goto regmap_init_failed;
	}

	/* Do some dt things */
	ret = s2mm001b_probe_dt(usbsw, np);
	if (IS_ERR_VALUE(ret)) {
		dev_err(usbsw->dev, "failed to do some DT things\n");
		goto dt_things_failed;
	}

	/* Allocate extcon device */
	usbsw->edev =
		devm_extcon_dev_allocate(usbsw->dev, s2mm001b_extcon_cable);
	if (!usbsw->edev) {
		ret = PTR_ERR(usbsw->edev);
		dev_err(usbsw->dev, "failed to allocate memory for extcon\n");
		goto extcon_dev_alloc_failed;
	}
	usbsw->edev->name = S2MM001B_EXTCON_NAME;

	/* Register extcon device */
	ret = devm_extcon_dev_register(usbsw->dev, usbsw->edev);
	if (IS_ERR_VALUE(ret)) {
		dev_err(usbsw->dev, "failed to register extcon device\n");
		goto extcon_register_failed;
	}

	read_reg(usbsw, S2MM001B_REG_DEVID, &device_id);
	usbsw->id.version = device_id & VERSION_MASK;
	usbsw->id.vendor = device_id & VENDOR_MASK;
	dev_info(usbsw->dev, "device id: version 0x%02X, vendor 0x%02X",
			usbsw->id.version, usbsw->id.vendor);

	ret = s2mm001b_reg_init(usbsw);
	if (ret)
		goto s2mm001b_reg_init_failed;

	ret = devm_request_irq(usbsw->dev, client->irq,
						s2mm001b_irq_handler,
					IRQF_NO_SUSPEND | IRQF_TRIGGER_FALLING,
						"s2mm001b Micro USB IC",
						usbsw);
	if (IS_ERR_VALUE(ret)) {
		dev_err(usbsw->dev, "Unable to get IRQ %d\n", client->irq);
		goto request_irq_failed;
	}

	usbsw_api = usbsw;

	return 0;

request_irq_failed:
s2mm001b_reg_init_failed:
	devm_extcon_dev_unregister(usbsw->dev, usbsw->edev);
extcon_register_failed:
	devm_extcon_dev_free(usbsw->dev, usbsw->edev);
extcon_dev_alloc_failed:
	regmap_exit(usbsw->regmap);
dt_things_failed:
regmap_init_failed:
	i2c_set_clientdata(client, NULL);
	kfree(usbsw);
	return ret;
}

static const struct i2c_device_id s2mm001b_i2c_ids[] = {
	{ "s2mm001b", 0 },
	{}
};

MODULE_DEVICE_TABLE(i2c, s2mm001b_i2c_ids);

static struct i2c_driver s2mm001b_i2c_driver = {
	.driver = {
		.name = "s2mm001b",
		.owner = THIS_MODULE,
		.pm = &s2mm001b_pm_ops,
		.of_match_table = of_match_ptr(s2mm001b_dt_ids),
	},
	.probe = s2mm001b_probe,
	.id_table = s2mm001b_i2c_ids,
};

module_i2c_driver(s2mm001b_i2c_driver);

/*
 * Detect accessory after completing the initialization of platform
 *
 * After completing the booting of platform, the extcon provider
 * driver should notify cable state to upper layer.
 */
static __init int s2mm001b_init_detect(void)
{
	if (IS_ERR_OR_NULL(usbsw_api))
		return -1;

	mutex_lock(&usbsw_api->mutex);
	disable_irq(usbsw_api->client->irq);

	dev_info(usbsw_api->dev, "%s", __func__);
	s2mm001b_cable_handler(usbsw_api, true);

	enable_irq(usbsw_api->client->irq);
	mutex_unlock(&usbsw_api->mutex);

	return 0;
}

late_initcall_sync(s2mm001b_init_detect);

MODULE_DESCRIPTION("Samsung s2mm001b Extcon driver");
MODULE_AUTHOR("Seonggyu Park <seongyu.park@samsung.com>");
MODULE_LICENSE("GPL");
