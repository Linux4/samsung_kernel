/*
 * extcon-sm5504.c - Silicon Mitus SM5504 extcon drvier to support USB switches
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd
 * Author: Seonggyu Park <seongyu.park@samsung.com>
 *
 * based on sm5502 extcon driver
 * Copyright (c) 2014 Samsung Electronics Co., Ltd
 * Author: Chanwoo Choi <cw00.choi@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#define DEBUG

#include <linux/delay.h>
#include <linux/edge_wakeup_mmp.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/irqdomain.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/extcon.h>
#include <linux/extcon/extcon-sm5504.h>
#include <linux/extcon/extcon-sec.h>

struct sm5504_muic_info {
	struct device *dev;
	struct extcon_dev *edev;
	struct i2c_client *i2c;
	struct regmap *regmap;
	struct regmap_irq_chip_data *irq_data;
	struct work_struct irq_work;
	struct mutex mutex;
	struct notifier_block panic_notif;

	int irq;
	u8 intr1, intr2;
};

/* SM5504 Interrupt 1 register */
#define INT1_ADC_CHG			(1 << 6)
#define INT1_CONNECT			(1 << 5)
#define INT1_OVP			(1 << 4)
#define INT1_DCD_OUT			(1 << 3)
#define INT1_CHGDET			(1 << 2)
#define INT1_DETACH			(1 << 1)
#define INT1_ATTACH			(1 << 0)

/* SM5504 Interrupt 2 register */
#define INT2_OVP_OCP			(1 << 7)
#define INT2_OCP			(1 << 6)
#define INT2_OCP_LATCH			(1 << 5)
#define INT2_OVP_FET			(1 << 4)
#define INT2_POR			(1 << 2)
#define INT2_UVLO			(1 << 1)
#define INT2_RID_CHARGER		(1 << 0)

#define CTRL_DEF_INT			0xE5

static struct sm5504_muic_info *info_api;
static int bcd_scan;

/* Default value of SM5504 register to bring up MUIC device. */
static struct reg_default sm5504_reg_defaults[] = {
	{
		.reg = SM5504_REG_CONTROL,
		.def = (!SM5504_REG_CONTROL_MASK_INT_MASK)
			| SM5504_REG_CONTROL_MANUAL_SW_MASK
			| (!SM5504_REG_CONTROL_SW_OPEN_MASK)
			| SM5504_REG_CONTROL_CHGTYP_MASK
			| SM5504_REG_CONTROL_USBCHDEN_MASK
			| SM5504_REG_CONTROL_ADC_EN_MASK,
	}, {
		.reg = SM5504_REG_INTMASK1,
		.def = SM5504_REG_INTM1_CHGDET_MASK
			| SM5504_REG_INTM1_DCD_OUT_MASK
			| SM5504_REG_INTM1_OVP_MASK
			| SM5504_REG_INTM1_CONNECT_EVENT_MASK
			| SM5504_REG_INTM1_ADC_CHG_EVENT_MASK,
	}, {
		.reg = SM5504_REG_INTMASK2,
		.def = !SM5504_REG_INTM2_RID_CHARGER_MASK
			| !SM5504_REG_INTM2_UVLO_MASK
			| SM5504_REG_INTM2_POR_MASK
			| !SM5504_REG_INTM2_OVP_FET_MASK
			| !SM5504_REG_INTM2_OCP_LATCH_MASK
			| !SM5504_REG_INTM2_OCP_MASK
			| SM5504_REG_INTM2_OVP_OCP_MASK,
	}, {
		.reg = SM5504_REG_RSVD21,
		.def = (!SM5504_REG_RSVD_ID1_JIG_PERIODIC)
			| (!SM5504_REG_RSVD_ID1_DEV_TYPE_MODE)
			| SM5504_REG_RSVD_ID1_ADC_DET_T50
			| SM5504_REG_RSVD_ID1_VBUS_DELAY_ENABLE_MASK
			| SM5504_REG_RSVD_ID1_ENABLE_DCDTIMEOUT_MASK
			| SM5504_REG_RSVD_ID1_DCDTIMER_MASK,
	},
};

const char *sm5504_extcon_cable[CABLE_NAME_MAX + 1] = {
	[SM5504_CABLE_OTG]		= "OTG",
	[SM5504_CABLE_USB]		= "USB",
	[SM5504_CABLE_UART]		= "UART",
	[SM5504_CABLE_CARKIT_T1]	= "Carkit Type1",
	[SM5504_CABLE_CDP]		= "CDP",
	[SM5504_CABLE_DCP]		= "DCP",
	[SM5504_CABLE_DESKTOP_DOCK]	= "Desktop Dock",
	[SM5504_CABLE_OTHER_CHARGER]	= "Other Charger",
	[SM5504_CABLE_JIG_UART_OFF]	= "JIG UART Off",
	[SM5504_CABLE_JIG_UART_ON]	= "JIG UART On",
	[SM5504_CABLE_JIG_USB_OFF]	= "JIG USB Off",
	[SM5504_CABLE_JIG_USB_ON]	= "JIG USB On",
	[SM5504_CABLE_UNKNOWN]		= "Unknown",
	[SM5504_CABLE_UNKNOWN_VB]	= "Unknown VB",
};

/* Define interrupt list of SM5504 to register regmap_irq */
static const struct regmap_irq sm5504_irqs[] = {
	/* INT1 interrupts */
	{ .reg_offset = 0, .mask = SM5504_IRQ_INT1_ATTACH_MASK, },
	{ .reg_offset = 0, .mask = SM5504_IRQ_INT1_DETACH_MASK, },
	{ .reg_offset = 0, .mask = SM5504_IRQ_INT1_CHGDET_MASK, },
	{ .reg_offset = 0, .mask = SM5504_IRQ_INT1_DCD_OUT_MASK, },
	{ .reg_offset = 0, .mask = SM5504_IRQ_INT1_OVP_MASK, },
	{ .reg_offset = 0, .mask = SM5504_IRQ_INT1_CONNECT_MASK, },
	{ .reg_offset = 0, .mask = SM5504_IRQ_INT1_ADC_CHG_MASK, },

	/* INT2 interrupts */
	{ .reg_offset = 1, .mask = SM5504_IRQ_INT2_RID_CHARGER_MASK,},
	{ .reg_offset = 1, .mask = SM5504_IRQ_INT2_UVLO_MASK, },
	{ .reg_offset = 1, .mask = SM5504_IRQ_INT2_POR_MASK, },
	{ .reg_offset = 1, .mask = SM5504_IRQ_INT2_OVP_FET_MASK, },
	{ .reg_offset = 1, .mask = SM5504_IRQ_INT2_OCP_LATCH_MASK, },
	{ .reg_offset = 1, .mask = SM5504_IRQ_INT2_OCP_MASK, },
	{ .reg_offset = 1, .mask = SM5504_IRQ_INT2_OVP_OCP_MASK, },
};

static const struct regmap_irq_chip sm5504_muic_irq_chip = {
	.name			= "sm5504",
	.status_base		= SM5504_REG_INT1,
	.mask_base		= SM5504_REG_INTMASK1,
	.mask_invert		= false,
	.num_regs		= 2,
	.irqs			= sm5504_irqs,
	.num_irqs		= ARRAY_SIZE(sm5504_irqs),
};

/* Define regmap configuration of SM5504 for I2C communication  */
static bool sm5504_muic_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case SM5504_REG_INT1:
	case SM5504_REG_INT2:
		return true;
	default:
		break;
	}
	return false;
}

static const struct regmap_config sm5504_muic_regmap_config = {
	.reg_bits	= 8,
	.val_bits	= 8,
	.volatile_reg	= sm5504_muic_volatile_reg,
	.max_register	= SM5504_REG_END,
};

static int read_reg(struct sm5504_muic_info *info, u8 reg, u8 *data)
{
	int ret;
	unsigned int buf;

	ret = regmap_read(info->regmap, (unsigned)reg, &buf);
	if (ret < 0) {
		dev_err(info->dev, "I2C Read REG[0x%.2X] failed\n", reg);
		return ret;
	}

	*data = (u8)buf;

	dev_dbg(info->dev, "I2C Read REG[0x%.2X] DATA[0x%.2X]\n", reg, *data);
	return ret;
}

static int write_reg(struct sm5504_muic_info *info, u8 reg, u8 data)
{
	int ret;

	ret = regmap_write(info->regmap, (unsigned)reg, (unsigned) data);
	if (ret < 0) {
		dev_err(info->dev, "I2C write REG[0x%.2X] failed\n", reg);
		return ret;
	}

	dev_dbg(info->dev, "I2C Write REG[0x%.2X] DATA[0x%.2X]\n", reg, data);
	return ret;
}

static int sm5504_reg_init(struct sm5504_muic_info *info)
{
	u8 intr1, intr2;
	int ret;

	ret = regmap_multi_reg_write(info->regmap, sm5504_reg_defaults,
					ARRAY_SIZE(sm5504_reg_defaults));
	if (IS_ERR_VALUE(ret))
		dev_err(info->dev, "failed to registers init.\n");

	/* Read and Clear INTERRUPT1,2 REGS */
	dev_info(info->dev, "The First INT1&2 R/C\n");

	mutex_lock(&info->mutex);
	disable_irq(info->irq);
	read_reg(info, SM5504_REG_INT1, &intr1);
	read_reg(info, SM5504_REG_INT2, &intr2);
	enable_irq(info->irq);
	mutex_unlock(&info->mutex);

	return ret;
}

enum { USB = 0, UART, OPEN, AUTO };

/* Change DM_CON/DP_CON/VBUSIN switch according to cable type */
static int __sm5504_set_path(struct sm5504_muic_info *info,
							const unsigned path)
{
	const char *connect[] = {"USB", "UART", "OPEN", "AUTO"};
	bool chagne_ok, man_mode;
	int ret = 0;

	disable_irq(info->irq);
	mutex_lock(&info->mutex);

	man_mode = (path == AUTO) ? false : true;

	ret = regmap_update_bits_check(info->regmap,
					SM5504_REG_CONTROL,
					SM5504_REG_CONTROL_MANUAL_SW_MASK,
					man_mode ?
					   !SM5504_REG_CONTROL_MANUAL_SW_MASK :
					   SM5504_REG_CONTROL_MANUAL_SW_MASK,
					&chagne_ok);
	if (IS_ERR_VALUE(ret)) {
		dev_err(info->dev, "change to %s mode failed\n", connect[path]);
		goto fail_exit;
	}

	if (!chagne_ok)
		dev_warn(info->dev, "already be changed to %s\n", connect[path]);

	switch(path) {
	case USB:
		ret = regmap_update_bits_check(info->regmap,
						SM5504_REG_MANUAL_SW1,
						SM5504_REG_MANUAL_SW1_MASK,
						DM_DP_SWITCH_USB,
						&chagne_ok);
		break;
	case UART:
		ret = regmap_update_bits_check(info->regmap,
						SM5504_REG_MANUAL_SW1,
						SM5504_REG_MANUAL_SW1_MASK,
						DM_DP_SWITCH_UART,
						&chagne_ok);
		break;
	case OPEN:
		ret = regmap_update_bits_check(info->regmap,
						SM5504_REG_MANUAL_SW1,
						SM5504_REG_MANUAL_SW1_MASK,
						DM_DP_SWITCH_OPEN,
						&chagne_ok);
		break;
	case AUTO:
	default:
		chagne_ok = true;
	}

	if (IS_ERR_VALUE(ret)) {
		mutex_unlock(&info->mutex);
		dev_err(info->dev, "change to %s failed\n", connect[path]);
		goto fail_exit;
	}

	if (!chagne_ok)
		dev_warn(info->dev, "already be changed to %s\n", connect[path]);

	mutex_unlock(&info->mutex);
	enable_irq(info->irq);

	dev_info(info->dev, "switch to %s\n", connect[path]);

	return ret;

fail_exit:
	mutex_unlock(&info->mutex);
	enable_irq(info->irq);
	return ret;
}

/**
 * sm5504_set_path() - Change connector path manually
 * @path : path name of you want change ("USB", "UART". "OPEN")
 */
int sm5504_set_path(const char *path)
{
	unsigned to_path;

	if (!info_api)
		return -1;

	if (!strncmp("USB", path, 4)) {
		to_path = USB;
	} else if (!strncmp("UART", path, 4)) {
		to_path = UART;
	} else if (!strncmp("OPEN", path, 4)) {
		to_path = OPEN;
	} else if (!strncmp("AUTO", path, 4)) {
		to_path = AUTO;
	} else {
		dev_err(info_api->dev, "invalid parameter: %s", path);
		return -1;
	}

	return __sm5504_set_path(info_api, to_path);
}
EXPORT_SYMBOL_GPL(sm5504_set_path);

static void sm5504_chargepump(struct sm5504_muic_info *info, bool enable)
{
	u8 value = enable ? 0x00 : 0x01;

	write_reg(info, SM5504_REG_CHG_PUMP, value);
	return;
}

static unsigned sm5504_detect_cable(struct sm5504_muic_info *info)
{
	int cable_idx = SM5504_CABLE_NONE;
	u8 devt1, devt2, adc, chg_type;
	u8 regctrl1;

	read_reg(info, SM5504_REG_DEV_TYPE1, &devt1);
	read_reg(info, SM5504_REG_DEV_TYPE2, &devt2);
	read_reg(info, SM5504_REG_ADC, &adc);
	read_reg(info, SM5504_REG_CHG_TYPE, &chg_type);

	if (bcd_scan) {
		bcd_scan = 0;
		dev_info(info->dev, "BCD Scan end chg_type = 0x%x\n", chg_type);
		if (chg_type == SM5504_REG_CHG_TYPE_DCP_MASK)
			cable_idx = SM5504_CABLE_DCP;
		else
			cable_idx = SM5504_CABLE_USB;
	} else {
		if (devt1 & SM5504_REG_DEV_TYPE1_USB_OTG_MASK)
			cable_idx = SM5504_CABLE_OTG;
		else if (devt1 & SM5504_REG_DEV_TYPE1_USB_SDP_MASK) {
			/* LG A1401, A1357 Charger */
			if (info->intr1 & INT1_DCD_OUT && info->intr2 & INT2_RID_CHARGER)
				cable_idx = SM5504_CABLE_OTHER_CHARGER;
			else
				cable_idx = SM5504_CABLE_USB;
		}
		else if (devt1 & SM5504_REG_DEV_TYPE1_UART_MASK) {
			if (!(info->intr2 & INT2_UVLO))
				cable_idx = SM5504_CABLE_UNKNOWN_VB;
			else
				cable_idx = SM5504_CABLE_UART;
		}
		else if (adc == SM5504_219K_ADC) {
			/* LG 219K cable */
			dev_info(info->dev, "[MUIC] 219K USB Cable/Charger Connected\n");
			read_reg(info, SM5504_REG_CONTROL, &regctrl1);

			regctrl1 &= (~SM5504_REG_CONTROL_USBCHDEN_MASK);
			write_reg(info, SM5504_REG_CONTROL, regctrl1);

			msleep(1);

			regctrl1 |= (SM5504_REG_CONTROL_USBCHDEN_MASK);
			write_reg(info, SM5504_REG_CONTROL, regctrl1);

			bcd_scan = 1;
			dev_info(info->dev, "[MUIC] BCD Scan start \n");

			return SM5504_CABLE_NONE;
		}
		else if (devt1 & SM5504_REG_DEV_TYPE1_CAR_KIT_CHARGER_MASK)
			cable_idx = SM5504_CABLE_CARKIT_T1;
		else if (devt1 & SM5504_REG_DEV_TYPE1_USB_CDP_MASK)
			cable_idx = SM5504_CABLE_CDP;
		else if (devt1 & SM5504_REG_DEV_TYPE1_USB_DCP_MASK)
			cable_idx = SM5504_CABLE_DCP;
		else if (devt2 & SM5504_REG_DEV_TYPE2_JIG_USB_ON_MASK)
			cable_idx = SM5504_CABLE_JIG_USB_ON;
		else if (devt2 & SM5504_REG_DEV_TYPE2_JIG_USB_OFF_MASK)
			cable_idx = SM5504_CABLE_JIG_USB_OFF;
		else if (devt2 & SM5504_REG_DEV_TYPE2_JIG_UART_ON_MASK)
			cable_idx = SM5504_CABLE_JIG_UART_ON;
		else if (devt2 & SM5504_REG_DEV_TYPE2_JIG_UART_OFF_MASK)
			cable_idx = SM5504_CABLE_JIG_UART_OFF;
		else if (devt2 & SM5504_REG_DEV_TYPE2_UNKNOWN_ACC_MASK) {
			if (adc == SM5504_365K_ADC)
				cable_idx = SM5504_CABLE_DESKTOP_DOCK;
			else if (!(info->intr2 & INT2_UVLO))
				cable_idx = SM5504_CABLE_UNKNOWN_VB;
			else
				cable_idx = SM5504_CABLE_UNKNOWN;
		}
	}

	dev_info(info->dev, "[muic]%s cable : %s\n", __func__,
						sm5504_extcon_cable[cable_idx]);
	return cable_idx;
}

static int sm5504_muic_cable_handler(struct sm5504_muic_info *info,
				     bool attached)
{
	static unsigned int prev_cable_type = SM5504_CABLE_NONE;
	const char **cable_names = info->edev->supported_cable;
	unsigned int idx = 0;
	int ret;

	if (!cable_names)
		return 0;

	if (attached == true) {
		idx = sm5504_detect_cable(info);
		if (idx == SM5504_CABLE_NONE) {
			dev_warn(info->dev, "couldn't find any cables\n");
			return 0;
		}
		sm5504_chargepump(info, true);
		prev_cable_type = idx;
	} else {
		if (unlikely(prev_cable_type == SM5504_CABLE_NONE)) {
			dev_warn(info->dev, "The detach interrupt occured! But couldn't find previous cable type\n");
			return 0;
		}
		sm5504_chargepump(info, false);
		idx = prev_cable_type;
	}

	dev_dbg(info->dev, "%s connector %s\n", cable_names[idx],
					attached ? "attached" : "dettached");

	/* Change the state of external accessory */
	extcon_set_cable_state(info->edev, cable_names[idx], attached);

	return 0;
}

static void sm5504_muic_irq_work(struct work_struct *work)
{
	struct sm5504_muic_info *info = container_of(work,
					struct sm5504_muic_info, irq_work);
	u8 intr1, intr2, val;
	int ret = 0;

	if (!info->edev)
		return;

	msleep(50); /* wait for MUIC set internal register values */

	mutex_lock(&info->mutex);
	disable_irq(info->irq);

	/* EOS Test recovery */
	read_reg(info, SM5504_REG_CONTROL, &val);
	if (val == CTRL_DEF_INT) {
		dev_warn(info->dev, "Detected IC has been reset!");
		ret = sm5504_reg_init(info);
		if (IS_ERR_VALUE(ret))
			dev_err(info->dev, "Recovery failed");
		else
			dev_warn(info->dev, "Recovery done");
	}

	/* Read and Clear Interrupt1/2 */
	read_reg(info, SM5504_REG_INT1, &intr1);
	read_reg(info, SM5504_REG_INT2, &intr2);

	/* Save intr1/2 register value */
	info->intr1 = intr1;
	info->intr2 = intr2;

	/* Detect attached or detached cables */
	if (intr1 & INT1_DETACH) {
		ret = sm5504_muic_cable_handler(info, false);
		bcd_scan = 0;
	} else if ((intr1 & (INT1_ATTACH | INT1_CONNECT)) || (intr2 & INT2_RID_CHARGER)) {
		ret = sm5504_muic_cable_handler(info, true);
	}
	if (ret < 0)
		dev_err(info->dev, "failed to handle MUIC interrupt\n");

	/* Check VBUS */
	if (intr2 & INT2_UVLO) {
		dev_info(info->dev, "VBUS Disappeared\n");
		blocking_notifier_call_chain(&sec_vbus_notifier, CABLE_VBUS_NONE, NULL);
	} else if (!(intr2 & INT2_UVLO)) {
		dev_info(info->dev, "VBUS Appeared\n");
		blocking_notifier_call_chain(&sec_vbus_notifier, CABLE_VBUS_OCCURRENCE, NULL);
	}

	enable_irq(info->irq);
	mutex_unlock(&info->mutex);

	return;
}

static void __sm5504_dump(struct sm5504_muic_info *info)
{
	u8 val;

	if (read_reg(info, SM5504_REG_DEV_TYPE1, &val) >= 0)
		pr_info("DEV1: 0x%x\n", val);
	if (read_reg(info, SM5504_REG_DEV_TYPE2, &val) >= 0)
		pr_info("DEV2: 0x%x\n", val);
	if (read_reg(info, SM5504_REG_ADC, &val) >= 0)
		pr_info("ADC: 0x%x\n", val);
	if (read_reg(info, SM5504_REG_CHG_TYPE, &val) >= 0)
		pr_info("CHG_TYPE: 0x%x\n", val);
	if (read_reg(info, SM5504_REG_INT1, &val) >= 0)
		pr_info("INT1: 0x%x\n", val);
	if (read_reg(info, SM5504_REG_INT2, &val) >= 0)
		pr_info("INT2: 0x%x\n", val);
	if (read_reg(info, SM5504_REG_CONTROL, &val) >= 0)
		pr_info("CTRL: 0x%x\n", val);

	/* dump connected cable status */
	pr_info("cable state: 0x%x\n", info->edev->state);
}

void sm5504_dump_fn(void)
{
	__sm5504_dump(info_api);
}
EXPORT_SYMBOL_GPL(sm5504_dump_fn);

static int sm5504_panic_handler(struct notifier_block *self,
				unsigned long l, void *buf)
{
	struct sm5504_muic_info *info =
		container_of(self, struct sm5504_muic_info, panic_notif);
	static bool retry = false;

	if (retry) {
		pr_err("%s: err: retry=%d, prevent retring dumpy\n",
				__func__, retry);
		return NOTIFY_DONE;
	}
	retry = true;

	__sm5504_dump(info);

	return NOTIFY_DONE;
}

static int sm5504_muic_i2c_remove(struct i2c_client *i2c)
{
	struct sm5504_muic_info *info = i2c_get_clientdata(i2c);

	regmap_del_irq_chip(info->irq, info->irq_data);

	return 0;
}

static irqreturn_t sm5504_irq_handler(int irq, void *data)
{
	struct sm5504_muic_info *info = data;

	dev_dbg(info->dev, "%s", __func__);
	schedule_work(&info->irq_work);

	return IRQ_HANDLED;
}

#ifdef CONFIG_PM_SLEEP
static int sm5504_muic_suspend(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct sm5504_muic_info *info = i2c_get_clientdata(i2c);

	enable_irq_wake(info->irq);
	dev_info(info->dev, "suspend\n");

	return 0;
}

static int sm5504_muic_resume(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct sm5504_muic_info *info = i2c_get_clientdata(i2c);
	u8 val;
	int ret = 0;

	disable_irq_wake(info->irq);

	/* EOS Test recovery */
	read_reg(info, SM5504_REG_CONTROL, &val);
	if (val == CTRL_DEF_INT) {
		dev_warn(info->dev, "Detected IC has been reset!");
		ret = sm5504_reg_init(info);
		if (IS_ERR_VALUE(ret))
			dev_err(info->dev, "Recovery failed");
		else
			dev_warn(info->dev, "Recovery done");
	}

	dev_info(info->dev, "resume\n");

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(sm5504_muic_pm_ops,
			 sm5504_muic_suspend, sm5504_muic_resume);

static struct of_device_id sm5504_dt_match[] = {
	{ .compatible = "siliconmitus,sm5504" },
	{ },
};
MODULE_DEVICE_TABLE(of, sm5504_dt_match);

static int sm5504_probe_dt(struct sm5504_muic_info *info, struct device_node *np)
{
	int ret, muic_int;
	const struct of_device_id *match;

	match = of_match_device(sm5504_dt_match, info->dev);
	if (!match)
		return -EINVAL;

	muic_int = of_get_named_gpio(np, "muic-int", 0);
	if (muic_int < 0) {
		dev_err(info->dev, "%s: of_get_named_gpio failed: %d\n", __func__, muic_int);
		return -EINVAL;
	}

	ret = request_mfp_edge_wakeup(muic_int, NULL, NULL, info->dev);
	if (ret)
		dev_err(info->dev, "%s: failed to request edge wakeup.\n", __func__);

	return 0;
}

static int sm5504_muic_i2c_probe(struct i2c_client *i2c,
				 const struct i2c_device_id *id)
{
	struct device_node *np = i2c->dev.of_node;
	struct sm5504_muic_info *info;
	int i, ret, irq_flags;

	if (!np)
		return -EINVAL;

	info = devm_kzalloc(&i2c->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;
	i2c_set_clientdata(i2c, info);

	info->dev = &i2c->dev;
	info->i2c = i2c;
	info->irq = i2c->irq;
	mutex_init(&info->mutex);
	INIT_WORK(&info->irq_work, sm5504_muic_irq_work);

	info->regmap = devm_regmap_init_i2c(i2c, &sm5504_muic_regmap_config);
	if (IS_ERR(info->regmap)) {
		ret = PTR_ERR(info->regmap);
		dev_err(info->dev, "failed to allocate register map: %d\n",
				   ret);
		return ret;
	}

	/* Do some dt things */
	ret = sm5504_probe_dt(info, np);
	if (ret)
		return ret;

	/* Allocate extcon device */
	info->edev = devm_extcon_dev_allocate(info->dev, sm5504_extcon_cable);
	if (IS_ERR(info->edev)) {
		dev_err(info->dev, "failed to allocate memory for extcon\n");
		return -ENOMEM;
	}
	info->edev->name = np->name;

	/* Register extcon device */
	ret = devm_extcon_dev_register(info->dev, info->edev);
	if (ret) {
		dev_err(info->dev, "failed to register extcon device\n");
		return ret;
	}

	ret = sm5504_reg_init(info);
	if (ret)
		goto sm5504_reg_init_failed;

	ret = devm_request_irq(info->dev, info->irq,
						sm5504_irq_handler,
					IRQF_NO_SUSPEND | IRQF_TRIGGER_FALLING,
						"sm5504 Micro USB IC",
						info);
	if (IS_ERR_VALUE(ret)) {
		dev_err(info->dev, "Unable to get IRQ %d\n", info->irq);
		goto request_irq_failed;
	}

	/* registers panic notifier to dump SM5504 status for debug */
	memset(&info->panic_notif, 0, sizeof(info->panic_notif));
	info->panic_notif.notifier_call = sm5504_panic_handler;
	atomic_notifier_chain_register(&panic_notifier_list, &info->panic_notif);

	info_api = info;

request_irq_failed:
sm5504_reg_init_failed:

	return 0;
}

static const struct i2c_device_id sm5504_i2c_id[] = {
	{ "sm5504", TYPE_SM5504 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sm5504_i2c_id);

static struct i2c_driver sm5504_muic_i2c_driver = {
	.driver		= {
		.name	= "sm5504",
		.owner	= THIS_MODULE,
		.pm	= &sm5504_muic_pm_ops,
		.of_match_table = of_match_ptr(sm5504_dt_match),
	},
	.probe	= sm5504_muic_i2c_probe,
	.remove	= sm5504_muic_i2c_remove,
	.id_table = sm5504_i2c_id,
};

module_i2c_driver(sm5504_muic_i2c_driver);

/*
 * Detect accessory after completing the initialization of platform
 *
 * After completing the booting of platform, the extcon provider
 * driver should notify cable state to upper layer.
 */
static __init int sm5504_init_detect(void)
{
	u8 intr1, intr2;

	if (IS_ERR_OR_NULL(info_api))
		return -1;

	dev_info(info_api->dev, "%s\n", __func__);

	mutex_lock(&info_api->mutex);
	disable_irq(info_api->irq);

	/* Turn off ChargePump */
	sm5504_chargepump(info_api, false);

	/* Read and Clear Interrupt 2 */
	read_reg(info_api, SM5504_REG_INT1, &intr1);
	read_reg(info_api, SM5504_REG_INT2, &intr2);

	/* Save intr2 register value */
	info_api->intr1 = intr1;
	info_api->intr2 = intr2;

	/* Notify the state of connector cable or not  */
	if (sm5504_muic_cable_handler(info_api, true) < 0)
		dev_warn(info_api->dev, "failed to detect cable state\n");

	/* Check VBUS */
	if (intr2 & INT2_UVLO) {
		dev_info(info_api->dev, "VBUS Disappeared\n");
		blocking_notifier_call_chain(&sec_vbus_notifier, CABLE_VBUS_NONE, NULL);
	} else if (!(intr2 & INT2_UVLO)) {
		dev_info(info_api->dev, "VBUS Appeared\n");
		blocking_notifier_call_chain(&sec_vbus_notifier, CABLE_VBUS_OCCURRENCE, NULL);
	}

	enable_irq(info_api->irq);
	mutex_unlock(&info_api->mutex);

	return 0;
}

late_initcall_sync(sm5504_init_detect);

MODULE_DESCRIPTION("Silicon Mitus SM5504 Extcon driver");
MODULE_AUTHOR("Seonggyu Park <seongyu.park@samsung.com>");
MODULE_LICENSE("GPL");
