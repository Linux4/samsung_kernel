/*
 *  sm5705-irq.c
 *  Samsung Interrupt controller support for SM5705
 *
 *  Copyright (C) 2016 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/err.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/mfd/sm5705/sm5705.h>

static const u8 sm5705_mask_reg[] = {
	[MUIC_INT1]     = SM5705_MUIC_REG_INTMASK1,
	[MUIC_INT2]     = SM5705_MUIC_REG_INTMASK2,
	[MUIC_INT3_AFC] = SM5705_MUIC_REG_INTMASK3_AFC,
};

static struct i2c_client *get_i2c(struct sm5705_dev *sm5705, int src)
{
	switch (src) {
		case MUIC_INT1:
		case MUIC_INT2:
		case MUIC_INT3_AFC:
			return sm5705->muic;
	}

	return ERR_PTR(-EINVAL);
}

struct sm5705_irq_data {
	int mask;
	int group;
};

#define DECLARE_IRQ(idx, _group, _mask)     [(idx)] = { .group = (_group), .mask = (_mask) }

static const struct sm5705_irq_data sm5705_irqs[] = {
	/* MUIC-irqs */
	DECLARE_IRQ(SM5705_MUIC_IRQ_INT1_OVP,	            MUIC_INT1,      1 << 5),    
	DECLARE_IRQ(SM5705_MUIC_IRQ_INT1_LKR,	            MUIC_INT1,      1 << 4),    
	DECLARE_IRQ(SM5705_MUIC_IRQ_INT1_LKP,	            MUIC_INT1,      1 << 3),    
	DECLARE_IRQ(SM5705_MUIC_IRQ_INT1_KP,	            MUIC_INT1,      1 << 2),
	DECLARE_IRQ(SM5705_MUIC_IRQ_INT1_DETACH,	        MUIC_INT1,      1 << 1),
	DECLARE_IRQ(SM5705_MUIC_IRQ_INT1_ATTACH,	        MUIC_INT1,      1 << 0),

	DECLARE_IRQ(SM5705_MUIC_IRQ_INT2_VBUSDET_ON,        MUIC_INT2,      1 << 7),
	DECLARE_IRQ(SM5705_MUIC_IRQ_INT2_RID_CHARGER,       MUIC_INT2,      1 << 6),
	DECLARE_IRQ(SM5705_MUIC_IRQ_INT2_MHL,               MUIC_INT2,      1 << 5),
	DECLARE_IRQ(SM5705_MUIC_IRQ_INT2_STUCK_KEY_RCV,	    MUIC_INT2,      1 << 4),
	DECLARE_IRQ(SM5705_MUIC_IRQ_INT2_STUCK_KEY,         MUIC_INT2,      1 << 3),
	DECLARE_IRQ(SM5705_MUIC_IRQ_INT2_ADC_CHG,           MUIC_INT2,      1 << 2),
	DECLARE_IRQ(SM5705_MUIC_IRQ_INT2_REV_ACCE,          MUIC_INT2,      1 << 1),
	DECLARE_IRQ(SM5705_MUIC_IRQ_INT2_VBUS_OFF,	        MUIC_INT2,      1 << 0),

	DECLARE_IRQ(SM5705_MUIC_IRQ_INT3_QC20_ACCEPTED,     MUIC_INT3_AFC,  1 << 6),
	DECLARE_IRQ(SM5705_MUIC_IRQ_INT3_AFC_ERROR,         MUIC_INT3_AFC,  1 << 5),
	DECLARE_IRQ(SM5705_MUIC_IRQ_INT3_AFC_STA_CHG,       MUIC_INT3_AFC,  1 << 4),
	DECLARE_IRQ(SM5705_MUIC_IRQ_INT3_MULTI_BYTE,	    MUIC_INT3_AFC,  1 << 3),
	DECLARE_IRQ(SM5705_MUIC_IRQ_INT3_VBUS_UPDATE,       MUIC_INT3_AFC,  1 << 2),
	DECLARE_IRQ(SM5705_MUIC_IRQ_INT3_AFC_ACCEPTED,      MUIC_INT3_AFC,  1 << 1),
	DECLARE_IRQ(SM5705_MUIC_IRQ_INT3_AFC_TA_ATTACHED,	MUIC_INT3_AFC,  1 << 0),
};

static void sm5705_irq_lock(struct irq_data *data)
{
	struct sm5705_dev *sm5705 = irq_get_chip_data(data->irq);

	mutex_lock(&sm5705->irqlock);
}

static void sm5705_irq_sync_unlock(struct irq_data *data)
{
	struct sm5705_dev *sm5705 = irq_get_chip_data(data->irq);
	int i;

	for (i = 0; i < SM5705_IRQ_GROUP_NR; i++) {
		struct i2c_client *i2c = get_i2c(sm5705, i);

		if (IS_ERR_OR_NULL(i2c))
			continue;

		sm5705->irq_masks_cache[i] = sm5705->irq_masks_cur[i];
		sm5705_write_reg(i2c, sm5705_mask_reg[i], sm5705->irq_masks_cur[i]);
	}

	mutex_unlock(&sm5705->irqlock);
}

	static const inline struct sm5705_irq_data *
irq_to_sm5705_irq(struct sm5705_dev *sm5705, int irq)
{
	return &sm5705_irqs[irq - sm5705->irq_base];
}

static void sm5705_irq_mask(struct irq_data *data)
{
	struct sm5705_dev *sm5705 = irq_get_chip_data(data->irq);
	const struct sm5705_irq_data *irq_data = irq_to_sm5705_irq(sm5705, data->irq);

	if (irq_data->group >= SM5705_IRQ_GROUP_NR)
		return;

	sm5705->irq_masks_cur[irq_data->group] |= irq_data->mask;
}

static void sm5705_irq_unmask(struct irq_data *data)
{
	struct sm5705_dev *sm5705 = irq_get_chip_data(data->irq);
	const struct sm5705_irq_data *irq_data = irq_to_sm5705_irq(sm5705, data->irq);

	if (irq_data->group >= SM5705_IRQ_GROUP_NR)
		return;

	sm5705->irq_masks_cur[irq_data->group] &= ~irq_data->mask;
}

static struct irq_chip sm5705_irq_chip = {
	.name			        = UNIVERSAL_MFD_DEV_NAME,
	.irq_bus_lock		    = sm5705_irq_lock,
	.irq_bus_sync_unlock	= sm5705_irq_sync_unlock,
	.irq_mask		        = sm5705_irq_mask,
	.irq_unmask		        = sm5705_irq_unmask,
};

static irqreturn_t sm5705_irq_thread(int irq, void *data)
{
	struct sm5705_dev *sm5705 = data;
	u8 irq_reg[SM5705_IRQ_GROUP_NR] = {0};
	int i, ret;

	ret = sm5705_bulk_read(sm5705->muic, SM5705_MUIC_REG_INT1, SM5705_NUM_IRQ_MUIC_REGS, &irq_reg[MUIC_INT1]);
	if (ret) {
		pr_err("%s:%s fail to read MUIC_INT source: %d\n", UNIVERSAL_MFD_DEV_NAME, __func__, ret);
		return IRQ_NONE;
	}

	for (i = MUIC_INT1; i <= MUIC_INT3_AFC; i++) {
		pr_debug("%s:%s MUIC_INT%d = 0x%x\n", UNIVERSAL_MFD_DEV_NAME, __func__, (i + 1), irq_reg[i]);
	}
		
	/* Apply masking */
	for (i = 0; i < SM5705_IRQ_GROUP_NR; i++) {
		irq_reg[i] &= ~sm5705->irq_masks_cur[i];
	}

	/* Report */
	for (i = 0; i < SM5705_IRQ_NR; i++) {
		if (irq_reg[sm5705_irqs[i].group] & sm5705_irqs[i].mask) {
			handle_nested_irq(sm5705->irq_base + i);
		}
	}

	return IRQ_HANDLED;
}

int sm5705_muic_irq_init(struct sm5705_dev *sm5705)
{
	int i;
	int ret;

	if (!sm5705->irq) {
		dev_warn(sm5705->dev, "No interrupt specified.\n");
		sm5705->irq_base = 0;
		return 0;
	}

	if (!sm5705->irq_base) {
		dev_err(sm5705->dev, "No interrupt base specified.\n");
		return 0;
	}

	mutex_init(&sm5705->irqlock);

	/* Mask individual interrupt sources */
	for (i = 0; i < SM5705_IRQ_GROUP_NR; i++) {
		struct i2c_client *i2c;

		sm5705->irq_masks_cur[i] = 0xff;
		sm5705->irq_masks_cache[i] = 0xff;

		i2c = get_i2c(sm5705, i);

		if (IS_ERR_OR_NULL(i2c))
			continue;

		sm5705_write_reg(i2c, sm5705_mask_reg[i], 0xff);
	}

	/* Register with genirq */
	for (i = 0; i < SM5705_IRQ_NR; i++) {
		int cur_irq;
		cur_irq = i + sm5705->irq_base;
		irq_set_chip_data(cur_irq, sm5705);
		irq_set_chip_and_handler(cur_irq, &sm5705_irq_chip, handle_level_irq);
		irq_set_nested_thread(cur_irq, 1);
#ifdef CONFIG_ARM
		set_irq_flags(cur_irq, IRQF_VALID);
#else
		irq_set_noprobe(cur_irq);
#endif
	}

	irq_set_status_flags(sm5705->irq, IRQ_NOAUTOEN); 

	ret = request_threaded_irq(sm5705->irq, NULL, sm5705_irq_thread, IRQF_TRIGGER_LOW | IRQF_ONESHOT,
			"sm5705-irq", sm5705);
	if (ret) {
		dev_err(sm5705->dev, "Fail to request IRQ %d: %d\n", sm5705->irq, ret);
		return ret;
	}

	enable_irq(sm5705->irq);

	return 0;
}
EXPORT_SYMBOL_GPL(sm5705_muic_irq_init);

void sm5705_muic_irq_exit(struct sm5705_dev *sm5705)
{
	if (sm5705->irq)
		free_irq(sm5705->irq, sm5705);
}
EXPORT_SYMBOL_GPL(sm5705_muic_irq_exit);



