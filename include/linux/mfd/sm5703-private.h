/*
 *  sm5703-private.h
 *  Samsung IF-PMIC device header file for SM5703
 *
 *  Copyright (C) 2016 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SM5703_PRIV_H__
#define __SM5703_PRIV_H__

#include <linux/i2c.h>

#define SM5703_IRQSRC_MUIC	    (1 << 0)
#define SM5703_IRQSRC_CHG	    (1 << 1)
#define SM5703_IRQSRC_FG            (1 << 2)

/* Slave addr = 0x4A : MUIC */
enum sm5703_muic_reg {
    SM5703_MUIC_REG_DEVICE_ID       = 0x01,
    SM5703_MUIC_REG_CONTROL         = 0x02,
    SM5703_MUIC_REG_INT1            = 0x03,
    SM5703_MUIC_REG_INT2            = 0x04,
    SM5703_MUIC_REG_INTMASK1        = 0x06,
    SM5703_MUIC_REG_INTMASK2        = 0x07,

    SM5703_MUIC_REG_END,
};

enum sm5703_charger_reg {
    SM5703_CHG_REG_INT_SOURCE       = 0x00,
    SM5703_CHG_REG_END,
};
enum sm5703_irq_source {
    MUIC_INT1 = 0,
    MUIC_INT2,
    SM5703_IRQ_GROUP_NR,
};
#define SM5703_NUM_IRQ_MUIC_REGS    3
#define SM5703_NUM_IRQ_CHG_REGS     4

enum sm5703_irq {
    /* MUIC INT1 */
    SM5703_MUIC_IRQ_INT1_LKR,              /* 0  */
    SM5703_MUIC_IRQ_INT1_LKP,              /* 1  */
    SM5703_MUIC_IRQ_INT1_KP,               /* 2  */
    SM5703_MUIC_IRQ_INT1_DETACH,           /* 3  */
    SM5703_MUIC_IRQ_INT1_ATTACH,           /* 4  */

    /* MUIC INT2 */
    SM5703_MUIC_IRQ_INT2_VBUSDET_ON,       /* 5  */
    SM5703_MUIC_IRQ_INT2_RID_CHARGER,      /* 6  */
    SM5703_MUIC_IRQ_INT2_MHL,              /* 7  */
    SM5703_MUIC_IRQ_INT2_STUCK_KEY_RCV,    /* 8  */
    SM5703_MUIC_IRQ_INT2_STUCK_KEY,        /* 9  */
    SM5703_MUIC_IRQ_INT2_ADC_CHG,          /* 10 */
    SM5703_MUIC_IRQ_INT2_REV_ACCE,         /* 11 */
    SM5703_MUIC_IRQ_INT2_VBUS_OFF,         /* 12 */

    SM5703_IRQ_NR,
};

struct sm5703_dev {
	struct device *dev;
	struct i2c_client *muic;        /* 0x4A; MUIC */
	struct mutex i2c_lock;

	int type;

	int irq;
	int irq_base;
	int irq_gpio;
	bool wakeup;
	struct mutex irqlock;
	int irq_masks_cur[SM5703_IRQ_GROUP_NR];
	int irq_masks_cache[SM5703_IRQ_GROUP_NR];

	struct pinctrl *i2c_pinctrl;
	struct pinctrl_state *i2c_gpio_state_active;
	struct pinctrl_state *i2c_gpio_state_suspend;

#ifdef CONFIG_HIBERNATION
	/* For hibernation */
	u8 reg_muic_dump[SM5703_MUIC_REG_END];
#endif

	/* For IC-Reset protection */
	void (*check_muic_reset)(struct i2c_client *, void *);
	void *muic_data;

	u8 device_id;

	struct sm5703_platform_data *pdata;
};

enum sm5703_types {
	TYPE_SM5703,
};

extern int sm5703_irq_init(struct sm5703_dev *sm5703);
extern void sm5703_irq_exit(struct sm5703_dev *sm5703);

/* SM5703 shared i2c API function */
extern int sm5703_read_reg(struct i2c_client *i2c, u8 reg, u8 *dest);
extern int sm5703_bulk_read(struct i2c_client *i2c, u8 reg, int count, u8 *buf);
extern int sm5703_read_word(struct i2c_client *i2c, u8 reg);
extern int sm5703_write_reg(struct i2c_client *i2c, u8 reg, u8 value);
extern int sm5703_bulk_write(struct i2c_client *i2c, u8 reg, int count, u8 *buf);
extern int sm5703_write_word(struct i2c_client *i2c, u8 reg, u16 value);

extern int sm5703_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask);

#endif /* __SM5703_PRIV_H__ */

