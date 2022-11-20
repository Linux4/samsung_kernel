/*
 *  sm5703-irq.c
 *  Samsung Interrupt controller support for SM5703
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
#include <linux/mfd/sm5703.h>
#include <linux/mfd/sm5703-private.h>

static const u8 sm5703_mask_reg[] = {
	[MUIC_INT1]     = SM5703_MUIC_REG_INTMASK1,
	[MUIC_INT2]     = SM5703_MUIC_REG_INTMASK2,
};

static struct i2c_client *get_i2c(struct sm5703_dev *sm5703, int src)
{
	switch (src) {
	case MUIC_INT1:
	case MUIC_INT2:
        return sm5703->muic;
	}

    return ERR_PTR(-EINVAL);
}

struct sm5703_irq_data {
	int mask;
	int group;
};

#define DECLARE_IRQ(idx, _group, _mask)     [(idx)] = { .group = (_group), .mask = (_mask) }

static const struct sm5703_irq_data sm5703_irqs[] = {
    /* MUIC-irqs */
    DECLARE_IRQ(SM5703_MUIC_IRQ_INT1_LKR,	            MUIC_INT1,      1 << 4),    
    DECLARE_IRQ(SM5703_MUIC_IRQ_INT1_LKP,	            MUIC_INT1,      1 << 3),    
    DECLARE_IRQ(SM5703_MUIC_IRQ_INT1_KP,	            MUIC_INT1,      1 << 2),
    DECLARE_IRQ(SM5703_MUIC_IRQ_INT1_DETACH,	        MUIC_INT1,      1 << 1),
    DECLARE_IRQ(SM5703_MUIC_IRQ_INT1_ATTACH,	        MUIC_INT1,      1 << 0),

    DECLARE_IRQ(SM5703_MUIC_IRQ_INT2_VBUSDET_ON,        MUIC_INT2,      1 << 7),
    DECLARE_IRQ(SM5703_MUIC_IRQ_INT2_RID_CHARGER,       MUIC_INT2,      1 << 6),
    DECLARE_IRQ(SM5703_MUIC_IRQ_INT2_MHL,               MUIC_INT2,      1 << 5),
    DECLARE_IRQ(SM5703_MUIC_IRQ_INT2_STUCK_KEY_RCV,	    MUIC_INT2,      1 << 4),
    DECLARE_IRQ(SM5703_MUIC_IRQ_INT2_STUCK_KEY,         MUIC_INT2,      1 << 3),
    DECLARE_IRQ(SM5703_MUIC_IRQ_INT2_ADC_CHG,           MUIC_INT2,      1 << 2),
    DECLARE_IRQ(SM5703_MUIC_IRQ_INT2_REV_ACCE,          MUIC_INT2,      1 << 1),
    DECLARE_IRQ(SM5703_MUIC_IRQ_INT2_VBUS_OFF,	        MUIC_INT2,      1 << 0),
};

static void sm5703_irq_lock(struct irq_data *data)
{
	struct sm5703_dev *sm5703 = irq_get_chip_data(data->irq);

	mutex_lock(&sm5703->irqlock);
}

static void sm5703_irq_sync_unlock(struct irq_data *data)
{
	struct sm5703_dev *sm5703 = irq_get_chip_data(data->irq);
	int i;

	for (i = 0; i < SM5703_IRQ_GROUP_NR; i++) {
		struct i2c_client *i2c = get_i2c(sm5703, i);

		if (IS_ERR_OR_NULL(i2c))
			continue;

		sm5703->irq_masks_cache[i] = sm5703->irq_masks_cur[i];

            sm5703_write_reg(i2c, sm5703_mask_reg[i], sm5703->irq_masks_cur[i]);
	}

	mutex_unlock(&sm5703->irqlock);
}

static const inline struct sm5703_irq_data *
irq_to_sm5703_irq(struct sm5703_dev *sm5703, int irq)
{
	return &sm5703_irqs[irq - sm5703->irq_base];
}

static void sm5703_irq_mask(struct irq_data *data)
{
	struct sm5703_dev *sm5703 = irq_get_chip_data(data->irq);
	const struct sm5703_irq_data *irq_data = irq_to_sm5703_irq(sm5703, data->irq);

	if (irq_data->group >= SM5703_IRQ_GROUP_NR)
		return;

    sm5703->irq_masks_cur[irq_data->group] |= irq_data->mask;
}

static void sm5703_irq_unmask(struct irq_data *data)
{
	struct sm5703_dev *sm5703 = irq_get_chip_data(data->irq);
    const struct sm5703_irq_data *irq_data = irq_to_sm5703_irq(sm5703, data->irq);

	if (irq_data->group >= SM5703_IRQ_GROUP_NR)
		return;

    sm5703->irq_masks_cur[irq_data->group] &= ~irq_data->mask;
}

static struct irq_chip sm5703_irq_chip = {
	.name			        = MFD_DEV_NAME,
	.irq_bus_lock		    = sm5703_irq_lock,
	.irq_bus_sync_unlock	= sm5703_irq_sync_unlock,
	.irq_mask		        = sm5703_irq_mask,
	.irq_unmask		        = sm5703_irq_unmask,
};

static irqreturn_t sm5703_irq_thread(int irq, void *data)
{
	struct sm5703_dev *sm5703 = data;
	u8 irq_reg[SM5703_IRQ_GROUP_NR] = {0};
	int i, ret;

	ret = sm5703_bulk_read(sm5703->muic, SM5703_MUIC_REG_INT1, SM5703_NUM_IRQ_MUIC_REGS, &irq_reg[MUIC_INT1]);
	if (ret) {
		pr_err("%s:%s fail to read MUIC_INT source: %d\n", MFD_DEV_NAME, __func__, ret);
		return IRQ_NONE;
	}
		
	/* Apply masking */
	for (i = 0; i < SM5703_IRQ_GROUP_NR; i++) {
		irq_reg[i] &= ~sm5703->irq_masks_cur[i];
	}

	/* Report */
	for (i = 0; i < SM5703_IRQ_NR; i++) {
		if (irq_reg[sm5703_irqs[i].group] & sm5703_irqs[i].mask) {
			handle_nested_irq(sm5703->irq_base + i);
		}
	}

	return IRQ_HANDLED;
}

int sm5703_irq_init(struct sm5703_dev *sm5703)
{
    int i;
    int ret;

    if (!sm5703->irq_gpio) {
        dev_warn(sm5703->dev, "No interrupt specified.\n");
        sm5703->irq_base = 0;
        return 0;
    }

    if (!sm5703->irq_base) {
        dev_err(sm5703->dev, "No interrupt base specified.\n");
        return 0;
    }

    mutex_init(&sm5703->irqlock);

    sm5703->irq = gpio_to_irq(sm5703->irq_gpio);
    pr_info("%s:%s irq=%d, irq->gpio=%d\n", MFD_DEV_NAME, __func__, sm5703->irq, sm5703->irq_gpio);

    ret = gpio_request(sm5703->irq_gpio, "if_pmic_irq");
    if (ret) {
        dev_err(sm5703->dev, "%s: fail requesting gpio %d\n", __func__, sm5703->irq_gpio);
        return ret;
    }
    gpio_direction_input(sm5703->irq_gpio);
    gpio_free(sm5703->irq_gpio);

    /* Mask individual interrupt sources */
    for (i = 0; i < SM5703_IRQ_GROUP_NR; i++) {
        struct i2c_client *i2c;

        sm5703->irq_masks_cur[i] = 0xff;
        sm5703->irq_masks_cache[i] = 0xff;

        i2c = get_i2c(sm5703, i);

        if (IS_ERR_OR_NULL(i2c))
            continue;

	sm5703_write_reg(i2c, sm5703_mask_reg[i], 0xff);
    }

    /* Register with genirq */
    for (i = 0; i < SM5703_IRQ_NR; i++) {
        int cur_irq;
        cur_irq = i + sm5703->irq_base;
        irq_set_chip_data(cur_irq, sm5703);
        irq_set_chip_and_handler(cur_irq, &sm5703_irq_chip, handle_level_irq);
        irq_set_nested_thread(cur_irq, 1);
#ifdef CONFIG_ARM
        set_irq_flags(cur_irq, IRQF_VALID);
#else
        irq_set_noprobe(cur_irq);
#endif
    }

    ret = request_threaded_irq(sm5703->irq, NULL, sm5703_irq_thread, IRQF_TRIGGER_LOW | IRQF_ONESHOT,
                   "sm5703-irq", sm5703);
    if (ret) {
        dev_err(sm5703->dev, "Fail to request IRQ %d: %d\n", sm5703->irq, ret);
        return ret;
    }

    return 0;
}
EXPORT_SYMBOL_GPL(sm5703_irq_init);

void sm5703_irq_exit(struct sm5703_dev *sm5703)
{
	if (sm5703->irq)
		free_irq(sm5703->irq, sm5703);
}
EXPORT_SYMBOL_GPL(sm5703_irq_exit);



