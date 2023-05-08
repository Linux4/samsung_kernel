/*
 *  include/linux/battery/charger/rt9455_charger.h
 *  Include header file for Richtek RT9455 Switch Mode Charger IC
 *
 *  Copyright (C) 2013 Richtek Electronics
 *  cy_huang <cy_huang@richtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LINUX_BATTERY_CHARGER_RT9455_H

#define SEC_CHARGER_I2C_SLAVEADDR	(0x22)

#define RT9455_DEVICE_NAME "RT9455"

#define RT9455_RST_MASK		0x80
#define RT9455_RST_SHIFT	7

#define RT9455_OPAMODE_MASK	0x01
#define RT9455_OPAMODE_SHIFT	0
enum {
	OPA_CHARGER_MODE,
	OPA_BOOST_MODE,
};

enum {
	IAICR_EXTERNAL,
	IAICR_INTERNAL,
};

enum {
	OCP_2P5A,
	OCP_3P5A,
};

enum {
	SEL_FREQ_3MHz,
	SEL_FREQ_1P5MHz,
};

#define RT9455_AICR_LIMIT_MASK	0xC0
#define RT9455_AICR_LIMIT_SHIFT	6
enum {
	IAICR_LIMIT_100MA,
	IAICR_LIMIT_500MA,
	IAICR_LIMIT_1A,
	IAICR_LIMIT_NO,
	IAICR_LIMIT_MAX,
};

#define RT9455_VOREG_MASK	0xFC
#define RT9455_VOREG_SHIFT	2

enum {
	OTGPL_ACTIVE_LOW,
	OTGPL_ACTIVE_HIGH,
};

#define RT9455_IEOC_MASK	0x3
#define RT9455_IEOC_SHIFT	0
enum {
	IEOC_10P,
	IEOC_30P,
	IEOC_20P,
	IEOC_RESV,
};

enum {
	IPREC_20MA,
	IPREC_40MA,
	IPREC_60MA,
};

enum {
	VDPM_4V,
	VDPM_4P25V,
	VDPM_4P5V,
	VDPM_DISABLE,
};

enum {
	VPREC_2V,
	VPREC_2P2V,
	VPREC_2P4V,
	VPREC_2P6V,
	VPREC_2P8V,
	VPREC_3V,
};


#define RT9455_ICHRG_MASK	0x70
#define RT9455_ICHRG_SHIFT	4
enum {
	ICHRG_20MV,
	ICHRG_26MV,
	ICHRG_32MV,
	ICHRG_38MV,
	ICHRG_44MV,
	ICHRG_50MV,
	ICHRG_56MV,
	ICHRG_62MV,
	ICHRG_MAX,
};

#define RT9455_CHGEN_MASK	0x10
#define RT9455_CHGEN_SHIFT	4

#define RT9455_TEEN_MASK	0x80
#define RT9455_TEEN_SHIFT	3

enum {
	RT9455_REG_CTRL1,
	RT9455_REG_CTRL2,
	RT9455_REG_CTRL3,
	RT9455_REG_DEVICE_ID,
	RT9455_REG_CTRL4,
	RT9455_REG_CTRL5,
	RT9455_REG_CTRL6,
	RT9455_REG_CTRL7,
	RT9455_REG_IRQ1,
	RT9455_REG_IRQ2,
	RT9455_REG_IRQ3,
	RT9455_REG_MASK1,
	RT9455_REG_MASK2,
	RT9455_REG_MASK3,
	RT9455_REG_MAX,
};

// VBOOST EVENT BIT
#define RT9455_EVENT_BSTVINOVI	0x80
#define RT9455_EVENT_BSTOLI	0x40
#define RT9455_EVENT_BSTLOWVI	0x20
#define RT9455_EVENT_BST32SI	0x08

// CHARGER EVENT BIT
#define RT9455_EVENT_CHRVPI	0x80
#define RT9455_EVENT_CHBATOVI	0x20
#define RT9455_EVENT_CHTERMI	0x10
#define RT9455_EVENT_CHRCHGI	0x08
#define RT9455_EVNET_CH32MI	0x04
#define RT9455_EVENT_CHTREGI	0x02
#define RT9455_EVENT_CHDPMI	0x01

// GENERAL EVENT BIT
#define RT9455_EVENT_TSDI	0x80
#define RT9455_EVENT_VINOVPI	0x40
#define RT9455_EVENT_BATAB	0x01

// RESET EVENT BIT
#define RT9455_EVENT_BEFORE_RST	0x80
#define RT9455_EVENT_AFTER_RST	0x40

// If not set, no irq event will be masked
struct rt9455_irq_mask {
	union {
		struct {
			unsigned char BATABM:1;
			unsigned char Resv:5;
			unsigned char VINOVPIM:1;
			unsigned char TSDM:1;
		}bitfield;
		unsigned char val;
	}mask1;
	union {
		struct {
			unsigned char CHDPMIM:1;
			unsigned char CHTREGIM:1;
			unsigned char CH32MIM:1;
			unsigned char CHRCHGIM:1;
			unsigned char CHTERMIM:1;
			unsigned char CHBATOVIM:1;
			unsigned char Resv:1;
			unsigned char CHRVPIM:1;
		}bitfield;
		unsigned char val;
	}mask2;
	union {
		struct {
			unsigned char Resv1:3;
			unsigned char BST32SIM:1;
			unsigned char Resv2:1;
			unsigned char BSTLOWVIM:1;
			unsigned char BSTOLIM:1;
			unsigned char BSTVIMOVIM:1;
		}bitfield;
		unsigned char val;
	}mask3;
};

struct rt9455_init_ctrlval {
	union {
		struct {
			unsigned char OPA_MODE:1;
			unsigned char HZ_BIT:1;
			unsigned char IAICR_INT:1;
			unsigned char TE:1;
			unsigned char HIGH_OCP:1;
			unsigned char SEL_SWFREQ:1;
			unsigned char IAICR:2;
		}bitfield;
		unsigned char val;
	}ctrl2;
	union {
		struct {
			unsigned char OTG_EN:1;
			unsigned char OTG_PL:1;
			unsigned char VOREG:6;
		}bitfield;
		unsigned char val;
	}ctrl3;
	union {
		struct {
			unsigned char IEOC:2;
			unsigned char IPREC:2;
			unsigned char VDPM:2;
			unsigned char Resv:1;
			unsigned char TMR_EN:1;
		}bitfield;
		unsigned char val;
	}ctrl5;
	union {
		struct {
			unsigned char VPREC:3;
			unsigned char Resv1:1;
			unsigned char ICHRG:3;
			unsigned char Resv2:1;
		}bitfield;
		unsigned char val;
	}ctrl6;
	union {
		struct {
			unsigned char VMREG:4;
			unsigned char CHG_EN:1;
			unsigned char Resv1:1;
			unsigned char BATD_EN:1;
			unsigned char Resv2:1;
		}bitfield;
		unsigned char val;
	}ctrl7;
};

struct rt9455_callbacks {
	void (*boost_callback)(int);
	void (*charger_callback)(int);
	void (*general_callback)(int);
	void (*reset_callback)(int);
};

struct rt9455_chip_init_data {
	struct rt9455_irq_mask irq_mask;
	struct rt9455_init_ctrlval init_ctrlval;
	struct rt9455_callbacks *callbacks;
};

#if 0
struct rt9455_chip {
	struct i2c_client *i2c;
	struct device *dev;
	struct workqueue_struct *wq;
	struct rt9455_callbacks *cb;
	int intr_gpio;
	int irq;
	int suspend;
	int otg_ext_enable;
	struct work_struct work;
	struct mutex io_lock;
};
#endif

//external function for the customer
#if 0
int rt9455_ext_force_otamode(int onoff);
int rt9455_ext_charger_current(int value);
int rt9455_ext_charger_switch(int onoff);
#endif

#ifdef CONFIG_CHARGER_RT9455_DBG
#define RT_DBG(format, args...) \
	pr_info("%s:%s() line-%d: " format, RT9455_DEVICE_NAME, __FUNCTION__, __LINE__, ##args)
#else
#define RT_DBG(format, args...)
#endif /* CONFIG_CHARGER_RT9455_DBG */

#endif /* __LINUX_BATTERY_CHARGER_RT9455_H */
