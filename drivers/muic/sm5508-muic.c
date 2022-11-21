/*
 * driver/muic/sm5508-muic.c - SM5508 MUIC micro USB switch device driver
 *
 * Copyright (C) 2013 Samsung Electronics
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
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <linux/input.h>
#include <linux/switch.h>
#if defined(CONFIG_OF)
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif
#include <linux/muic/muic.h>

#ifdef CONFIG_MUIC_NOTIFIER
#include <linux/muic/muic_notifier.h>
#endif

#include <linux/muic/sm5508-muic.h>
#include "../battery_v2/include/sec_charging_common.h"
#include <linux/wakelock.h>

static struct wake_lock sm5508_irq_wakelock;

static struct sm5508_muic_usbsw *local_usbsw;

static int sm5508_set_afc_ctrl_reg(struct sm5508_muic_usbsw *usbsw, int shift, bool on);

static int sm5508_muic_attach_dev(struct sm5508_muic_usbsw *usbsw, muic_attached_dev_t new_dev);
static int sm5508_muic_detach_dev(struct sm5508_muic_usbsw *usbsw);

static int sm5508_muic_write_byte(struct i2c_client *client, int reg, int val)
{
	int ret;
	int retry = 0;
	int written = 0;

	ret = i2c_smbus_write_byte_data(client, reg, val);
	while (ret < 0) {
		pr_err("%s: i2c write error reg(0x%x), retrying(%d)...\n", __func__, reg, retry);
		if (retry > 5) {
			pr_err("%s: retry failed!!\n", __func__);
			break;
		}
		msleep(100);
		written = i2c_smbus_read_byte_data(client, reg);
		if (written < 0)
			pr_err("%s: error reg(0x%x) written(0x%x)\n", __func__, reg, written);
		ret = i2c_smbus_write_byte_data(client, reg, val);
		retry++;
	}

	return ret;
}

static int sm5508_muic_read_byte(struct i2c_client *client, int reg)
{
	int ret = 0;
	int retry = 0;

	ret = i2c_smbus_read_byte_data(client, reg);
	while (ret < 0) {
		pr_err("%s: reg(0x%x), retrying(%d)...\n", __func__, reg, retry);
		if (retry > 5) {
			pr_err("%s: retry failed!!\n", __func__);
			break;
		}
		msleep(100);
		ret = i2c_smbus_read_byte_data(client, reg);
		retry++;
	}

	return ret;
}

static int set_com_sw(struct sm5508_muic_usbsw *usbsw, enum sm5508_reg_manual_sw1_value reg_val)
{
	struct i2c_client *client = usbsw->client;
	int ret = 0, value = 0;

	value = sm5508_muic_read_byte(client, SM5508_MUIC_REG_MANSW1);
	if (value < 0)
		pr_info("%s err read MANSW1(%d)\n", __func__, value);

	if (reg_val != value) {
		pr_info("%s: reg_val(0x%x) != MANSW1 reg(0x%x), update reg\n", __func__, reg_val, value);

		ret = sm5508_muic_write_byte(client, SM5508_MUIC_REG_MANSW1, reg_val);
		if (ret < 0)
			pr_info("%s: err write REG_MANSW1(%d)\n", __func__, ret);
		else
			pr_info("%s: REG_MANSW1 = [0x%02x]\n", __func__, reg_val);
	} else {
		pr_info("%s: reg_val(0x%x) == MANSW1 reg(0x%x), just return\n", __func__, reg_val, value);
		return 0;
	}

	return ret;
}

static int com_to_open(struct sm5508_muic_usbsw *usbsw)
{
	int ret = 0;

	ret = set_com_sw(usbsw, SM5508_MANSW1_OPEN);

	return ret;
}
/*
static int com_U_to_A(struct sm5508_muic_usbsw *usbsw)
{
	int ret = 0;

	ret = set_com_sw(usbsw, SM5508_MANSW1_U_TO_A);

	return ret;
}

static int com_U_to_UART(struct sm5508_muic_usbsw *usbsw)
{
	int ret = 0;

	ret = set_com_sw(usbsw, SM5508_MANSW1_U_TO_UART);

	return ret;
}
*/
static void sm5508_muic_reg_init(struct sm5508_muic_usbsw *usbsw)
{
	struct i2c_client *client = usbsw->client;
	int ret;

	pr_info("%s: called\n", __func__);

	ret = sm5508_muic_read_byte(client, SM5508_MUIC_REG_DEVID);
	if (ret < 0)
		pr_info("%s err read DEVID(%d)\n", __func__, ret);
	else
		pr_info("%s: DEVID: [0x%02x]\n", __func__, ret);

	ret = sm5508_muic_write_byte(client, SM5508_MUIC_REG_INTMASK1, SM5508_INT_MASK1_VALUE);
	if (ret < 0)
		pr_err("%s: err write INTMASK1(%d)\n", __func__, ret);
	else
		pr_info("%s: INTMASK1 = [0x%02x]\n", __func__, SM5508_INT_MASK1_VALUE);

	ret = sm5508_muic_write_byte(client, SM5508_MUIC_REG_INTMASK2, SM5508_INT_MASK2_VALUE);
	if (ret < 0)
		pr_err("%s: err write INTMASK2(%d)\n", __func__, ret);
	else
		pr_info("%s: INTMASK2 = [0x%02x]\n", __func__, SM5508_INT_MASK2_VALUE);

	ret = sm5508_muic_write_byte(client, SM5508_MUIC_REG_INTMASK3, SM5508_INT_MASK3_VALUE);
	if (ret < 0)
		pr_err("%s: err write INTMASK3(%d)\n", __func__, ret);
	else
		pr_info("%s: INTMASK3 = [0x%02x]\n", __func__, SM5508_INT_MASK3_VALUE);

	ret = sm5508_muic_write_byte(client, SM5508_MUIC_REG_CTRL, SM5508_CONTROL_VALUE);
	if (ret < 0)
		pr_err("%s: err write REG_CTRL(%d)\n", __func__, ret);
	else
		pr_info("%s: REG_CTRL = [0x%02x]\n", __func__, SM5508_CONTROL_VALUE);

	ret = sm5508_muic_read_byte(client, SM5508_MUIC_REG_MANSW2);
	if (ret < 0) {
		pr_info("%s err read MANUAL SWITCH2(%d)\n", __func__, ret);
	} else {
		pr_info("%s: MANUAL SWITCH2 = [0x%02x]\n", __func__, ret);
		ret |= SM5508_RID_DISABLE_MASK;
		pr_info("%s: after MANUAL SWITCH2 = [0x%02x]\n", __func__, ret);
		ret = sm5508_muic_write_byte(client, SM5508_MUIC_REG_MANSW2, ret);	
	}
}

static int sm5508_afc_ta_attach(struct sm5508_muic_usbsw *usbsw)
{
	struct i2c_client *client = usbsw->client;
	int ret = 0, afctxd = 0;
	union power_supply_propval value;
	int retry = 0;

	pr_info("%s: AFC_TA_ATTACHED\n", __func__);

	/* read VBUS VALID */
	ret = sm5508_muic_read_byte(client, SM5508_MUIC_REG_VBUS_VALID);
	if (ret < 0)
		pr_info("%s err read VBUS(%d)\n", __func__, ret);
	else
		pr_info("%s VBUS: [0x%02x]\n", __func__, ret);

	if ((ret&0x01) == 0x00) {
		pr_info("%s: VBUS NOT VALID [0x%02x] just return\n", __func__, ret);
		return 0;
	}

	/* read clear : AFC_STATUS */
	ret = sm5508_muic_read_byte(client, SM5508_MUIC_REG_AFC_STATUS);
	if (ret < 0)
		pr_info("%s err read AFC_STATUS(%d)\n", __func__, ret);
	else	
		pr_info("%s: AFC_STATUS: [0x%02x]\n", __func__, ret);

	usbsw->attached_dev = ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC;
#if defined(CONFIG_USE_SECOND_MUIC)
	muic_notifier_attach_attached_dev(usbsw->attached_dev + ATTACHED_DEV_NUM);
#else
	muic_notifier_attach_attached_dev(usbsw->attached_dev);
#endif
	
	for (retry = 0; retry < 10; retry++) {
		msleep(50);
		psy_do_property("charger", get, POWER_SUPPLY_PROP_CURRENT_MAX, value);
		if (value.intval <= 500) {
			pr_info("%s: PREPARE Success(%d mA) - retry(%d)\n", __func__, value.intval, retry);
			break;
		} else {
			pr_info("%s: PREPARE fail(%d mA) - retry(%d)\n", __func__, value.intval, retry);
		}
	}

	/* voltage(9.0V)  + current(1.65A) setting : 0x46 */
	/* voltage(12.0V) + current(2.1A) setting  : 0x79 */
	afctxd = SM5508_MUIC_AFC_9V;
	ret = sm5508_muic_write_byte(client, SM5508_MUIC_REG_AFC_TXD, afctxd);
	if (ret < 0)
		pr_info("%s: err write AFC_TXD(%d)\n", __func__, ret);
	else
		pr_info("%s: AFC_TXD = [0x%02x]\n", __func__, afctxd);

	/* ENAFC set '1' */
	sm5508_set_afc_ctrl_reg(usbsw, SM5508_AFC_ENAFC_SHIFT, 1);
	pr_info("%s: AFCCTRL_ENAFC 1\n", __func__);

	usbsw->afc_retry_count = 0;

	return 0;
}

static int sm5508_afc_ta_accept(struct sm5508_muic_usbsw *usbsw)
{
	struct i2c_client *client = usbsw->client;
	int dev3 = 0, vol = 0;

	pr_info("%s: AFC_ACCEPTED\n", __func__);

	/* ENAFC set '0' */
	sm5508_set_afc_ctrl_reg(usbsw, SM5508_AFC_ENAFC_SHIFT, 0);

	dev3 = sm5508_muic_read_byte(client, SM5508_MUIC_REG_DEV_T3);
	if (dev3 < 0)
		pr_info("%s err read DEV_T3(%d)\n", __func__, dev3);
	else	
		pr_info("%s: DEV_T3: [0x%02x]\n", __func__, dev3);

	/* 5V Default, From 5.0V to 20V in 1V steps */
	vol = sm5508_muic_read_byte(client, SM5508_MUIC_REG_AFC_TXD);
	if (vol < 0)
		pr_info("%s err read AFC_TXD(%d)\n", __func__, vol);
	else
		pr_info("%s: AFC_TXD: [0x%02x]\n", __func__, vol);

	if (vol != 0)
		pr_info("%s: err read vol(0x%x)\n", __func__, (int)vol);

	if (dev3 & SM5508_DEV_TYPE3_AFC_TA_CHG) {
		vol = (vol&0xF0)>>4;
		if (vol == 0x07)
			usbsw->attached_dev = ATTACHED_DEV_AFC_CHARGER_12V_MUIC;
		else if (vol == 0x04)
			usbsw->attached_dev = ATTACHED_DEV_AFC_CHARGER_9V_MUIC;
		else
			usbsw->attached_dev = ATTACHED_DEV_AFC_CHARGER_5V_MUIC;
#if defined(CONFIG_USE_SECOND_MUIC)
	muic_notifier_attach_attached_dev(usbsw->attached_dev + ATTACHED_DEV_NUM);
#else
	muic_notifier_attach_attached_dev(usbsw->attached_dev);
#endif
	} else {
		usbsw->attached_dev = ATTACHED_DEV_AFC_CHARGER_5V_MUIC;
#if defined(CONFIG_USE_SECOND_MUIC)
	muic_notifier_attach_attached_dev(usbsw->attached_dev + ATTACHED_DEV_NUM);
#else
	muic_notifier_attach_attached_dev(usbsw->attached_dev);
#endif
		pr_info("%s: attached_dev(%d)\n", __func__, usbsw->attached_dev);
	}

	return 0;
}

static int sm5508_afc_multi_byte(struct sm5508_muic_usbsw *usbsw)
{
	struct i2c_client *client = usbsw->client;
	int multi_byte[6] = {0, 0, 0, 0, 0, 0};
	int i = 0;
	int ret = 0;
	int voltage_find = 0;

	pr_info("%s: AFC_MULTI_BYTE\n", __func__);

	/* ENAFC set '0' */
	sm5508_set_afc_ctrl_reg(usbsw, SM5508_AFC_ENAFC_SHIFT, 0);

	/* read AFC_RXD1 ~ RXD6 */
	voltage_find = 0;
	for (i = 0 ; i < 6 ; i++) {
		multi_byte[i] = sm5508_muic_read_byte(client, SM5508_MUIC_REG_AFC_RXD1 + i);
		if (multi_byte[i] < 0)
			pr_info("%s: err read AFC_RXD%d(%d)\n", __func__, i+1, multi_byte[i]);
		else
			pr_info("%s: AFC_RXD%d: [0x%02x]\n", __func__, i+1, multi_byte[i]);

		if (multi_byte[i] == 0x00)
			break;

		if (i >= 1) {   /* voltate find */
			if (((multi_byte[i]&0xF0)>>4) >= ((multi_byte[voltage_find]&0xF0)>>4))
				voltage_find = i;
		}
	}

	pr_info("%s: AFC_RXD%d multi_byte[%d]=0x%02x\n", __func__,
			voltage_find+1, voltage_find, multi_byte[voltage_find]);

	if (((multi_byte[i]&0xF0)>>4) > 0x04) {
		ret = sm5508_muic_write_byte(client, SM5508_MUIC_REG_AFC_TXD, SM5508_MUIC_AFC_9V);
		if (ret < 0)
			pr_info("%s: err write AFC_TXD(%d)\n", __func__, ret);
		else
			pr_info("%s: AFC_TXD = [0x%02x]\n", __func__, SM5508_MUIC_AFC_9V);
	} else {
		ret = sm5508_muic_write_byte(client, SM5508_MUIC_REG_AFC_TXD, multi_byte[voltage_find]);
		if (ret < 0)
			pr_info("%s: err write AFC_TXD(%d)\n", __func__, ret);
		else
			pr_info("%s: AFC_TXD = [0x%02x]\n", __func__, multi_byte[voltage_find]);
	}

	/* ENAFC set '1' */
	sm5508_set_afc_ctrl_reg(usbsw, SM5508_AFC_ENAFC_SHIFT, 1);
	pr_info("%s: AFCCTRL_ENAFC 1\n", __func__);

	return 0;
}

static int sm5508_afc_error(struct sm5508_muic_usbsw *usbsw)
{
	struct i2c_client *client = usbsw->client;
	int value = 0;
	int dev3 = 0;

	pr_info("%s: AFC_ERROR : afc_retry_count(%d)\n", __func__, usbsw->afc_retry_count);

	/* ENAFC set '0' */
	sm5508_set_afc_ctrl_reg(usbsw, SM5508_AFC_ENAFC_SHIFT, 0);

	/* read AFC_STATUS */
	value = sm5508_muic_read_byte(client, SM5508_MUIC_REG_AFC_STATUS);
	if (value < 0)
		pr_info("%s: err read AFC_STATUS(%d)\n", __func__, value);
	else
		pr_info("%s: AFCSTATUS: [0x%02x]\n", __func__, value);

	if (usbsw->afc_retry_count < 5) {
		dev3 = sm5508_muic_read_byte(client, SM5508_MUIC_REG_DEV_T3);
		if (dev3 < 0)
			pr_info("%s: err read DEVICE_TYPE3(%d)\n", __func__, dev3);
		else
			pr_info("%s: DEVICE_TYPE3: [0x%02x]\n", __func__, dev3);

		if ((dev3 & SM5508_DEV_TYPE3_QC20_TA_CHG) && (usbsw->afc_retry_count >= 2)) { /* QC20_TA */

			usbsw->attached_dev = ATTACHED_DEV_QC_CHARGER_9V_MUIC;
			sm5508_set_afc_ctrl_reg(usbsw, SM5508_AFC_ENQC20_9V_SHIFT, 1); /* QC20 9V */

			pr_info("%s: ENQC20_9V\n", __func__);

		} else {
			msleep(100); /* 100ms delay */
			/* ENAFC set '1' */
			sm5508_set_afc_ctrl_reg(usbsw, SM5508_AFC_ENAFC_SHIFT, 1);
			usbsw->afc_retry_count++;
			pr_info("%s: re-start AFC (afc_retry_count=%d)\n", __func__, usbsw->afc_retry_count);
		}
	} else {
		pr_info("%s: ENAFC end, afc_retry_count(%d)\n", __func__, usbsw->afc_retry_count);
		usbsw->attached_dev = ATTACHED_DEV_AFC_CHARGER_5V_MUIC;
#if defined(CONFIG_USE_SECOND_MUIC)
	muic_notifier_attach_attached_dev(usbsw->attached_dev + ATTACHED_DEV_NUM);
#else
	muic_notifier_attach_attached_dev(usbsw->attached_dev);
#endif
	}

	return 0;
}

static int sm5508_set_afc_ctrl_reg(struct sm5508_muic_usbsw *usbsw, int shift, bool on)
{
	struct i2c_client *client = usbsw->client;
	u8 reg_val = 0;
	int ret = 0;

	pr_info("%s: Register[%d], set [%d]\n", __func__, shift, on);

	ret = sm5508_muic_read_byte(client, SM5508_MUIC_REG_AFC_CTRL);
	if (ret < 0)
		pr_info("%s: err read AFC_CTRL(%d)\n", __func__, ret);
	else
		pr_info("%s: AFC_CTRL: [0x%02x]\n", __func__, ret);

	if (on)
		reg_val = ret | (0x1 << shift);
	else
		reg_val = ret & ~(0x1 << shift);

	if (reg_val ^ ret) {
		pr_info("%s: reg_val(0x%x) != AFC_CTRL reg(0x%x), update reg\n", __func__, reg_val, ret);

		ret = sm5508_muic_write_byte(client, SM5508_MUIC_REG_AFC_CTRL, reg_val);
		if (ret < 0)
			pr_err("%s: err write AFC_CTRL(%d)\n", __func__, ret);
		else
			pr_info("%s: AFC_CTRL = [0x%02x]\n", __func__, reg_val);
	} else {
		pr_info("%s: (0x%x), just return\n", __func__, ret);
		return 0;
	}

	ret = sm5508_muic_read_byte(client, SM5508_MUIC_REG_AFC_CTRL);
	if (ret < 0)
		pr_info("%s: err read AFC_CTRL(%d)\n", __func__, ret);
	else
		pr_info("%s: changed AFC_CTRL: [0x%02x]\n", __func__, ret);

	return ret;
}


static int sm5508_muic_attach_dev(struct sm5508_muic_usbsw *usbsw, muic_attached_dev_t new_dev)
{
	bool noti_f = true;

	pr_info("%s:attached_dev:%d new_dev:%d\n", __func__, usbsw->attached_dev, new_dev);

	noti_f = true;

	switch (new_dev) {
	case ATTACHED_DEV_USB_MUIC:
		/* To do : USB */
		usbsw->attached_dev = new_dev;
	break;
	case ATTACHED_DEV_CDP_MUIC:
		/* To do : CDP */
		usbsw->attached_dev = new_dev;
	break;
	case ATTACHED_DEV_TA_MUIC:
		/* To do : TA */
		usbsw->attached_dev = new_dev;
	break;
	case ATTACHED_DEV_UNDEFINED_CHARGING_MUIC:
		/* To do : Not used*/
		usbsw->attached_dev = new_dev;
	break;
	case ATTACHED_DEV_USB_LANHUB_MUIC:
		/* To do : Not used */
		usbsw->attached_dev = new_dev;
	break;
	case ATTACHED_DEV_OTG_MUIC:
		/* To do : Not used : OTG */
		usbsw->attached_dev = new_dev;
	break;
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
		/* To do : JIG_USB_ON */
		usbsw->attached_dev = new_dev;
	break;
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
		/* To do : JIG_USB_OFF */
		usbsw->attached_dev = new_dev;
	break;
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
		/* To do : JIG_UART_ON */
		usbsw->attached_dev = new_dev;
	break;
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
		/* To do : JIG_UART_OFF */
		usbsw->attached_dev = new_dev;
	break;
	case ATTACHED_DEV_CHARGING_CABLE_MUIC:
		/* To do : Not used : PPD */
		usbsw->attached_dev = new_dev;
	break;
	case ATTACHED_DEV_UNIVERSAL_MMDOCK_MUIC:
		/* To do : Not used : TTY */
		usbsw->attached_dev = new_dev;
	break;
	case ATTACHED_DEV_DESKDOCK_MUIC:
		/* To do : Not used : AV */
		usbsw->attached_dev = new_dev;
	break;
	case ATTACHED_DEV_MHL_MUIC:
		/* To do : Not used : Others */
		usbsw->attached_dev = new_dev;
	break;
	case ATTACHED_DEV_DESKDOCK_VB_MUIC:
		/* To do : Not used : DESKDOCK + VB */
		usbsw->attached_dev = new_dev;
	break;
	case ATTACHED_DEV_AFC_CHARGER_9V_MUIC:
		/* To do : AFC 9V */
		usbsw->attached_dev = new_dev;
	break;
	case ATTACHED_DEV_QC_CHARGER_9V_MUIC:
		/* To do : QC20 9V */
		usbsw->attached_dev = new_dev;
	break;
	default:
		/* To do : Others */
		usbsw->attached_dev = new_dev;
		pr_info("%s: unsupported dev=%d, adc=0x%x, vbus=0x%x\n", __func__, new_dev, usbsw->adc, usbsw->vbus);
	break;
	}

#if defined(CONFIG_MUIC_NOTIFIER)
	if (noti_f)
#if defined(CONFIG_USE_SECOND_MUIC)
	muic_notifier_attach_attached_dev(usbsw->attached_dev + ATTACHED_DEV_NUM);
#else
	muic_notifier_attach_attached_dev(new_dev);
#endif
	else
		pr_info("%s: attach Noti. for (%d) discarded.\n", __func__, new_dev);
#endif /* CONFIG_MUIC_NOTIFIER */

	return 0;
}

static int sm5508_muic_detach_dev(struct sm5508_muic_usbsw *usbsw)
{
	struct i2c_client *client = usbsw->client;
	int ret = 0;
	bool noti_f = true;

	pr_info("%s: attached_dev:%d\n", __func__, usbsw->attached_dev);

#if defined(CONFIG_USE_SECOND_MUIC)
	if (usbsw->attached_dev == ATTACHED_DEV_NONE_MUIC) {
		pr_info("%s:%s Duplicated(%d), just ignore\n", __func__, usbsw->attached_dev);
		goto out_without_noti;
	}
#endif

	ret = com_to_open(usbsw);

	/* AFC_CNTL : reset*/
	ret = sm5508_muic_write_byte(client, SM5508_MUIC_REG_AFC_CTRL, 0x00);
	if (ret < 0)
		pr_info("%s: err write AFC_CTRL(%d)\n", __func__, ret);
	else
		pr_info("%s: AFC_CTRL = [0x%02x]\n", __func__, 0x00);

	switch (usbsw->attached_dev) {
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_CDP_MUIC:
		/* To do */
	break;
	case ATTACHED_DEV_TA_MUIC:
		/* To do */
	break;
	case ATTACHED_DEV_UNDEFINED_CHARGING_MUIC:
	case ATTACHED_DEV_USB_LANHUB_MUIC:
		/* To do */
	break;
	case ATTACHED_DEV_OTG_MUIC:
		/* To do */
	break;
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
		/* To do */
	break;
	case ATTACHED_DEV_CHARGING_CABLE_MUIC:
	case ATTACHED_DEV_UNIVERSAL_MMDOCK_MUIC:
	case ATTACHED_DEV_DESKDOCK_MUIC:
	case ATTACHED_DEV_DESKDOCK_VB_MUIC:
		/* To do */
	break;
	case ATTACHED_DEV_MHL_MUIC:
		/* To do */
	break;
	case ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_9V_MUIC:
	case ATTACHED_DEV_QC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_QC_CHARGER_9V_MUIC:
		/* To do */
	break;
	default:
		pr_info("%s: invalid attached_dev type(%d)\n", __func__, usbsw->attached_dev);
	break;
	}

	usbsw->intr1	= 0;
	usbsw->intr2	= 0;
	usbsw->intr3	= 0;

	usbsw->dev1		= 0;
	usbsw->dev2		= 0;
	usbsw->dev3		= 0;
	usbsw->adc		= 0x1F;
	usbsw->vbus		= 0;

#if defined(CONFIG_MUIC_NOTIFIER)
	if (noti_f)
#if defined(CONFIG_USE_SECOND_MUIC)
	muic_notifier_detach_attached_dev(usbsw->attached_dev + ATTACHED_DEV_NUM);
#else
	muic_notifier_detach_attached_dev(usbsw->attached_dev);
#endif
	else
		pr_info("%s: detach Noti. for (%d) discarded.\n", __func__, usbsw->attached_dev);
#endif /* CONFIG_MUIC_NOTIFIER */

out_without_noti:
	usbsw->attached_dev = ATTACHED_DEV_NONE_MUIC;

	return 0;
}

static void sm5508_muic_detect_dev(struct sm5508_muic_usbsw *usbsw)
{
	struct i2c_client *client = usbsw->client;
	muic_attached_dev_t new_dev = ATTACHED_DEV_NONE_MUIC;
	int intr = MUIC_INTR_DETACH;
	int dev1 = 0, dev2 = 0, dev3 = 0, vbus = 0, adc = 0, chg_type = 0, status = 0;

	dev1 = sm5508_muic_read_byte(client, SM5508_MUIC_REG_DEV_T1);
	dev2 = sm5508_muic_read_byte(client, SM5508_MUIC_REG_DEV_T2);
	dev3 = sm5508_muic_read_byte(client, SM5508_MUIC_REG_DEV_T3);
	vbus = sm5508_muic_read_byte(client, SM5508_MUIC_REG_VBUS_VALID);
	adc  = sm5508_muic_read_byte(client, SM5508_MUIC_REG_ADC);
	chg_type = sm5508_muic_read_byte(client, SM5508_MUIC_REG_CHG_TYPE);
	status = sm5508_muic_read_byte(client, SM5508_MUIC_REG_STATUS);

	pr_info("%s: dev[1:0x%02x, 2:0x%02x, 3:0x%02x], adc:0x%02x, vbus:0x%02x, chg_type:0x%02x, status:0x%02x\n",
			__func__, dev1, dev2, dev3, adc, vbus, chg_type, status);

	usbsw->dev1	= dev1;
	usbsw->dev2	= dev2;
	usbsw->dev3	= dev3;
	usbsw->adc	= adc;
	usbsw->vbus	= vbus;

	/* Attached */
	switch (dev1) {
	case SM5508_DEV_TYPE1_USB_OTG:
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_OTG_MUIC;
		pr_info("%s: **OTG\n", __func__);
	break;
	case SM5508_DEV_TYPE1_TA:
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_TA_MUIC;
		pr_info("%s: **TA(DCP)\n", __func__);
	break;
	case SM5508_DEV_TYPE1_CDP:
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_CDP_MUIC;
		pr_info("%s: **USB_CDP\n", __func__);
		break;
	case SM5508_DEV_TYPE1_CARKIT_CHG:
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_UNDEFINED_CHARGING_MUIC;
		pr_info("%s: **CARKIT\n", __func__);
	break;
	case SM5508_DEV_TYPE1_UART:
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_UNDEFINED_CHARGING_MUIC;
		pr_info("%s: **UART\n", __func__);
	break;
	case SM5508_DEV_TYPE1_USB:
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_USB_MUIC;
		pr_info("%s: **USB\n", __func__);
		break;
	case SM5508_DEV_TYPE1_AUDIO_2:
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_USB_LANHUB_MUIC;
		pr_info("%s: **AUDIO_TYPE2\n", __func__);
		break;
	case SM5508_DEV_TYPE1_AUDIO_1:
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_UNDEFINED_CHARGING_MUIC;
		pr_info("%s: **AUDIO_TYPE1\n", __func__);
		break;
	default:
		break;
	}

	switch (dev2) {
	case SM5508_DEV_TYPE2_AV:
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_DESKDOCK_MUIC;
		pr_info("%s: **AV\n", __func__);
	break;
	case SM5508_DEV_TYPE2_TTY:
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_UNIVERSAL_MMDOCK_MUIC;
		pr_info("%s: **TTY\n", __func__);
	break;
	case SM5508_DEV_TYPE2_PPD:
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_CHARGING_CABLE_MUIC;
		pr_info("%s: **PPD\n", __func__);
	break;
	case SM5508_DEV_TYPE2_JIG_UART_OFF:
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_JIG_UART_OFF_MUIC;
		pr_info("%s: **JIG_UART_OFF\n", __func__);
		break;
	case SM5508_DEV_TYPE2_JIG_UART_ON:
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_JIG_UART_ON_MUIC;
		pr_info("%s: **JIG_UART_ON\n", __func__);
	break;
	case SM5508_DEV_TYPE2_JIG_USB_OFF:
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_JIG_USB_OFF_MUIC;
		pr_info("%s: **JIG_USB_OFF\n", __func__);
		break;
	case SM5508_DEV_TYPE2_JIG_USB_ON:
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_JIG_USB_ON_MUIC;
		pr_info("%s: **JIG_USB_ON\n", __func__);
		break;
	default:
		break;
	}

	switch (dev3) {
	case SM5508_DEV_TYPE3_AFC_TA_CHG:
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_AFC_CHARGER_9V_MUIC;
		pr_info("%s: **AFC TA\n", __func__);
	break;
	case SM5508_DEV_TYPE3_U200_CHG:
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_TA_MUIC;
		pr_info("%s: **U200\n", __func__);
	break;
	case SM5508_DEV_TYPE3_LO_TA_CHG:
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_TA_MUIC;
		pr_info("%s: **LO_TA\n", __func__);
	break;
	case SM5508_DEV_TYPE3_AV_WITH_VBUS:
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_DESKDOCK_VB_MUIC;
		pr_info("%s: **AV with VBUS\n", __func__);
	break;
	case SM5508_DEV_TYPE3_DCD_OUT_SDP_CHG:
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_USB_MUIC;
		pr_info("%s: **DCD_OUT_SDP\n", __func__);
	break;
	case SM5508_DEV_TYPE3_QC20_TA_CHG:
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_QC_CHARGER_9V_MUIC;
		pr_info("%s: **QC20(9V)\n", __func__);
	break;
	case SM5508_DEV_TYPE3_MHL:
		intr = MUIC_INTR_ATTACH;
		new_dev = ATTACHED_DEV_MHL_MUIC;
		pr_info("%s: **MHL\n", __func__);
	break;
	default:
		break;
	}

	if ((adc != SM5508_ADC_OPEN) && (intr == MUIC_INTR_DETACH)) {
		intr = MUIC_INTR_ATTACH;
		if (vbus & 0x01)
			new_dev = ATTACHED_DEV_UNKNOWN_VB_MUIC;
		else
			new_dev = ATTACHED_DEV_UNKNOWN_MUIC;
	}

	if (intr == MUIC_INTR_ATTACH)
		sm5508_muic_attach_dev(usbsw, new_dev);
	else
		sm5508_muic_detach_dev(usbsw);
}

static irqreturn_t sm5508_muic_irq_thread(int irq, void *data)
{
	struct sm5508_muic_usbsw *usbsw = data;
	struct i2c_client *client = usbsw->client;
	int intr1 = 0, intr2 = 0, intr3 = 0;
	int adc = 0, vbus = 0;

	pr_info("%s: called\n", __func__);

	mutex_lock(&usbsw->muic_mutex);

	intr1 = sm5508_muic_read_byte(client, SM5508_MUIC_REG_INT1);
	intr2 = sm5508_muic_read_byte(client, SM5508_MUIC_REG_INT2);
	intr3 = sm5508_muic_read_byte(client, SM5508_MUIC_REG_INT3);
	vbus  = sm5508_muic_read_byte(client, SM5508_MUIC_REG_VBUS_VALID);
	adc   = sm5508_muic_read_byte(client, SM5508_MUIC_REG_ADC);

	pr_info("%s: intr1: 0x%02x, intr2:0x%02x, intr3:0x%02x, vbus:0x%02x, adc:0x%02x\n",
				__func__, intr1, intr2, intr3, vbus, adc);

	usbsw->intr1	= intr1;
	usbsw->intr2	= intr2;
	usbsw->intr3	= intr3;
	usbsw->adc		= adc;
	usbsw->vbus		= vbus;

	if ((intr1 & SM5508_INT1_ATTACH_MASK) ||
		(intr1 & SM5508_INT1_DETACH_MASK) ||
		(intr1 & SM5508_INT1_KP_MASK) ||
		(intr1 & SM5508_INT1_LKP_MASK) ||
		(intr1 & SM5508_INT1_LKR_MASK) ||
		(intr2 & SM5508_INT2_VBUS_U_OFF_MASK) ||
		(intr2 & SM5508_INT2_RSRV_ATTACH_MASK) ||
		(intr2 & SM5508_INT2_ADC_CHANGE_MASK) ||
		(intr2 & SM5508_INT2_STUCK_KEY_MASK) ||
		(intr2 & SM5508_INT2_STUCK_KEY_RCV_MASK) ||
		(intr2 & SM5508_INT2_MHL_MASK) ||
		(intr2 & SM5508_INT2_RID_CHARGER_MASK) ||
		(intr2 & SM5508_INT2_VBUS_U_ON_MASK)) {
		sm5508_muic_detect_dev(usbsw);

	/* AFC TA */
	} else if ((intr3 & SM5508_INT3_AFC_TA_ATTACHED_MASK) ||
				(intr3 & SM5508_INT3_AFC_ACCEPTED_MASK) ||
				(intr3 & SM5508_INT3_AFC_AFC_VBUS_9V_MASK) ||
				(intr3 & SM5508_INT3_AFC_MULTI_BYTE_MASK) ||
				(intr3 & SM5508_INT3_AFC_STA_CHG_MASK) ||
				(intr3 & SM5508_INT3_AFC_ERROR_MASK) ||
				(intr3 & SM5508_INT3_AFC_QC20ACCEPTED_MASK) ||
				(intr3 & SM5508_INT3_AFC_QC_VBUS_9V_MASK)) {

		if (intr3 & SM5508_INT3_AFC_ERROR_MASK) {  /*AFC_ERROR*/
			sm5508_afc_error(usbsw);
			goto irq_end;
		}  /*AFC_ERROR end*/

		if (intr3 & SM5508_INT3_AFC_TA_ATTACHED_MASK) {  /*AFC_TA_ATTACHED*/
			sm5508_afc_ta_attach(usbsw);
			goto irq_end;
		}

		if (intr3 & SM5508_INT3_AFC_ACCEPTED_MASK) {  /*AFC_ACCEPTED*/
			sm5508_afc_ta_accept(usbsw);
			goto irq_end;
		}

		if (intr3 & SM5508_INT3_AFC_MULTI_BYTE_MASK) {  /*AFC_MULTI_BYTE*/
			sm5508_afc_multi_byte(usbsw);
			goto irq_end;
		}

		if (intr3 & SM5508_INT3_AFC_STA_CHG_MASK) {
			pr_info("%s: INT3_AFC_STA_CHG_MASK\n", __func__);
			goto irq_end;
		}

		if (intr3 & SM5508_INT3_AFC_AFC_VBUS_9V_MASK) {
			pr_info("%s: INT3_AFC_AFC_VBUS_9V_MASK\n", __func__);
			goto irq_end;
		}

		if (intr3 & SM5508_INT3_AFC_QC20ACCEPTED_MASK) {
			pr_info("%s: INT3_AFC_QC20ACCEPTED_MASK\n", __func__);
			goto irq_end;
		}

		if (intr3 & SM5508_INT3_AFC_QC_VBUS_9V_MASK) {
			pr_info("%s: INT3_AFC_QC_VBUS_9V_MASK\n", __func__);
			goto irq_end;
		}
	} else {
		sm5508_muic_detect_dev(usbsw);
	}

irq_end:
	mutex_unlock(&usbsw->muic_mutex);

	pr_info("%s: end\n", __func__);
	return IRQ_HANDLED;
}

static int sm5508_muic_irq_init(struct sm5508_muic_usbsw *usbsw)
{
	struct i2c_client *i2c = usbsw->client;
	int ret = 0;

	if (!usbsw->pdata->gpio_irq) {
		pr_err("%s: No interrupt specified\n", __func__);
		return -ENXIO;
	}

	i2c->irq = gpio_to_irq(usbsw->pdata->gpio_irq);

	if (i2c->irq) {
		ret = request_threaded_irq(i2c->irq, NULL,
					sm5508_muic_irq_thread, (IRQF_TRIGGER_LOW | IRQF_ONESHOT),
					"sm5508 muic micro USB", usbsw);
		if (ret < 0) {
			pr_err("%s: failed to reqeust IRQ(%d)\n", __func__, i2c->irq);
			return ret;
		}

		enable_irq_wake(i2c->irq);
		if (ret < 0)
			pr_err("%s: failed to enable wakeup src\n", __func__);
	}

	return 0;
}


static void sm5508_muic_afc_init_detect(struct work_struct *work)
{
	struct sm5508_muic_usbsw *usbsw = container_of(work,
										struct sm5508_muic_usbsw, init_work.work);
	int ret = 0;
	int dev1 = 0, dev3 = 0, vbus = 0, afc_status = 0;
	int value = 0;

	dev1 = sm5508_muic_read_byte(usbsw->client, SM5508_MUIC_REG_DEV_T1);
	dev3 = sm5508_muic_read_byte(usbsw->client, SM5508_MUIC_REG_DEV_T3);
	vbus = sm5508_muic_read_byte(usbsw->client, SM5508_MUIC_REG_VBUS_VALID);
	afc_status = sm5508_muic_read_byte(usbsw->client, SM5508_MUIC_REG_STATUS);

	pr_info("%s: dev1:[0x%02x], dev3:[0x%02x], vbus:[0x%02x], afc_status:[0x%02x]\n", __func__, dev1, dev3, vbus, afc_status);

	/* power off 상태에서 AFC TA가 삽입된 경우 */
	if ((dev1 == SM5508_DEV_TYPE1_TA) && (dev3 == 0x00) && (afc_status == 0x01)) {
		value = SM5508_MUIC_AFC_9V; /* 0x46 : 9V / 1.65A */
		ret = sm5508_muic_write_byte(usbsw->client, SM5508_MUIC_REG_AFC_TXD, value);
		if (ret != 0)
			pr_info("%s: err write AFC_TXD(%d)\n", __func__, ret);
		else
			pr_info("%s: AFC_TXD = [0x%02x]\n", __func__, value);

		/* ENAFC set '1' */
		sm5508_afc_ta_attach(usbsw);
	}
}

static void muic_print_reg_info(struct work_struct *work)
{
	struct sm5508_muic_usbsw *usbsw = container_of(work, struct sm5508_muic_usbsw, usb_work.work);
	int dev1 = 0, dev2 = 0, dev3 = 0, vbus = 0, adc = 0, chg_type = 0, afc_status = 0;
	int manual_sw1 = 0, manual_sw2 = 0, afc_ctrl = 0;
	
	wake_lock(&sm5508_irq_wakelock);

	dev1 = sm5508_muic_read_byte(usbsw->client, SM5508_MUIC_REG_DEV_T1);
	dev2 = sm5508_muic_read_byte(usbsw->client, SM5508_MUIC_REG_DEV_T2);
	dev3 = sm5508_muic_read_byte(usbsw->client, SM5508_MUIC_REG_DEV_T3);
	vbus = sm5508_muic_read_byte(usbsw->client, SM5508_MUIC_REG_VBUS_VALID);
	adc  = sm5508_muic_read_byte(usbsw->client, SM5508_MUIC_REG_ADC);
	chg_type = sm5508_muic_read_byte(usbsw->client, SM5508_MUIC_REG_CHG_TYPE);
	afc_status = sm5508_muic_read_byte(usbsw->client, SM5508_MUIC_REG_STATUS);
	manual_sw1 = sm5508_muic_read_byte(usbsw->client, SM5508_MUIC_REG_MANSW1);
	manual_sw2 = sm5508_muic_read_byte(usbsw->client, SM5508_MUIC_REG_MANSW2);
	afc_ctrl = sm5508_muic_read_byte(usbsw->client, SM5508_MUIC_REG_AFC_CTRL);

	wake_unlock(&sm5508_irq_wakelock);

	pr_info("%s: D1:0x%02x, D2:0x%02x, D3:0x%02x, VB:0x%02x, ADC:0x%02x, CT:0x%02x, AS:0x%02x, MS1:0x%02x, MS2:0x%02x, AC:0x%02x\n",
		__func__, dev1, dev2, dev3, vbus, adc, chg_type, afc_status, manual_sw1, manual_sw2, afc_ctrl);

	schedule_delayed_work(&usbsw->usb_work, msecs_to_jiffies(60000));
}

static int sm5508_muic_parse_dt(struct device *dev, struct sm5508_muic_platform_data *pdata)
{
	struct device_node *np = dev->of_node;

	if (np == NULL) {
		pr_err("%s: np NULL\n", __func__);
		return -EINVAL;
	}

	/* i2c, irq gpio info */
	pdata->gpio_scl = of_get_named_gpio(np, "sm5508_muic,gpio-scl", 0);
	if (pdata->gpio_scl < 0) {
		pr_err("%s: failed to get gpio_scl = %d\n", __func__, pdata->gpio_scl);
		pdata->gpio_scl = 0;
	}

	pdata->gpio_sda = of_get_named_gpio(np, "sm5508_muic,gpio-sda",	0);
	if (pdata->gpio_sda < 0) {
		pr_err("%s: failed to get gpio_sda = %d\n", __func__, pdata->gpio_sda);
		pdata->gpio_sda = 0;
	}

	pdata->gpio_irq = of_get_named_gpio(np, "sm5508_muic,irq-gpio",	0);
	if (pdata->gpio_irq < 0) {
		pr_err("%s: failed to get gpio_irq = %d\n", __func__, pdata->gpio_irq);
		pdata->gpio_irq = 0;
	}

	pr_info("%s: gpio_irq: %u, gpio_scl:%d, gpio_sda:%d\n", __func__, pdata->gpio_irq, pdata->gpio_scl, pdata->gpio_sda);

	return 1;
}

static int sm5508_muic_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct sm5508_muic_usbsw *usbsw;
	struct sm5508_muic_platform_data *pdata;
	int ret = 0;

	pr_info("%s: sm5508 muic probe called\n", __func__);

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
					sizeof(struct sm5508_muic_platform_data), GFP_KERNEL);
		if (!pdata) {
			pr_err("%s: Failed to allocate memory\n", __func__);
			return -ENOMEM;
		}
		ret = sm5508_muic_parse_dt(&client->dev, pdata);
		if (ret < 0)
			return ret;
	} else {
		pdata = client->dev.platform_data;
	}

	if (!pdata)
		return -EINVAL;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -EIO;

	usbsw = kzalloc(sizeof(struct sm5508_muic_usbsw), GFP_KERNEL);
	if (!usbsw) {
		pr_err("%s: failed to allocate driver data\n", __func__);
		kfree(usbsw);
		return -ENOMEM;
	}

	usbsw->client = client;
	if (client->dev.of_node)
		usbsw->pdata = pdata;
	else
		usbsw->pdata = client->dev.platform_data;

	if (!usbsw->pdata)
		goto fail1;

	i2c_set_clientdata(client, usbsw);

	mutex_init(&usbsw->muic_mutex);
	local_usbsw = usbsw;

	sm5508_muic_reg_init(usbsw);

	ret = sm5508_muic_irq_init(usbsw);
	if (ret) {
		pr_info("%s: failed to enable  irq init\n", __func__);
		goto fail2;
	}

	wake_lock_init(&sm5508_irq_wakelock, WAKE_LOCK_SUSPEND, "sm5508-irq");

	INIT_DELAYED_WORK(&usbsw->usb_work, muic_print_reg_info);
	schedule_delayed_work(&usbsw->usb_work, msecs_to_jiffies(10000));

	/* initial cable detection */
	INIT_DELAYED_WORK(&usbsw->init_work, sm5508_muic_afc_init_detect);
	schedule_delayed_work(&usbsw->init_work, msecs_to_jiffies(2000));

	/* initial cable detection */
	sm5508_muic_irq_thread(-1, usbsw);

	return 0;

fail2:
	if (usbsw->pdata->gpio_irq)
		free_irq(usbsw->pdata->gpio_irq, usbsw);

	mutex_destroy(&usbsw->muic_mutex);
	i2c_set_clientdata(client, NULL);

fail1:
	kfree(usbsw);
	return ret;
}

static int sm5508_muic_remove(struct i2c_client *client)
{
	struct sm5508_muic_usbsw *usbsw = i2c_get_clientdata(client);

	cancel_delayed_work(&usbsw->usb_work);
	cancel_delayed_work(&usbsw->init_work);
	if (usbsw->pdata->gpio_irq) {
		disable_irq_wake(usbsw->pdata->gpio_irq);
		free_irq(usbsw->pdata->gpio_irq, usbsw);
	}
	mutex_destroy(&usbsw->muic_mutex);
	i2c_set_clientdata(client, NULL);

	kfree(usbsw);
	return 0;
}

static const struct i2c_device_id sm5508_muic_id[] = {
	{"siliconmitus,sm5508", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, sm5508_muic_id);

static struct of_device_id sm5508_muic_i2c_match_table[] = {
	{ .compatible = "siliconmitus,sm5508",},
	{},
};
MODULE_DEVICE_TABLE(of, sm5508_muic_i2c_match_table);

static struct i2c_driver sm5508_muic_i2c_driver = {
	.driver = {
		.name = "sm5508_muic_ct",
		.owner = THIS_MODULE,
		.of_match_table = sm5508_muic_i2c_match_table,
	},
	.probe  = sm5508_muic_probe,
	.remove = sm5508_muic_remove,
	.id_table = sm5508_muic_id,
};

static int __init sm5508_muic_init(void)
{
	pr_info("[SM0213]: muic_init call! \n");
	return i2c_add_driver(&sm5508_muic_i2c_driver);
}
/*
Late init call is required MUIC was accessing
USB driver before USB initialization and watch dog reset
was happening when booted with USB connected*/
late_initcall(sm5508_muic_init);

static void __exit sm5508_muic_exit(void)
{
	i2c_del_driver(&sm5508_muic_i2c_driver);
}
module_exit(sm5508_muic_exit);

MODULE_DESCRIPTION("SM5508 MUIC Micro USB Switch driver");
MODULE_LICENSE("GPL");

