/*
 * s2mpb06-irq.c - Interrupt controller support for S2MPB06
 *
 * Copyright (C) 2022 Samsung Electronics Co.Ltd
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * This driver is based on max77804-irq.c
 */

#include <linux/err.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/samsung/pmic/s2mpb06.h>
#include <linux/samsung/pmic/s2mpb06-regulator.h>

static const u8 s2mpb06_mask_reg[] = {
	[OCP_INT] = S2MPB06_REG_OCP_INT,
	[TSD_INT] = S2MPB06_REG_TSD_CTRL,
};

struct s2mpb06_irq_data {
	int mask;
	enum s2mpb06_irq_source group;
};

#define _INT(macro)	S2MPB06_##macro##_INT

#define DECLARE_IRQ(idx, _group, _mask)	\
	[(idx)] = { .group = (_group), .mask = (_mask) }

static const struct s2mpb06_irq_data s2mpb06_irqs[] = {
	DECLARE_IRQ(_INT(LDO1_OCP), OCP_INT, (1 << 1)),
	DECLARE_IRQ(_INT(LDO2_OCP), OCP_INT, (1 << 2)),
	DECLARE_IRQ(_INT(LDO3_OCP), OCP_INT, (1 << 3)),
	DECLARE_IRQ(_INT(LDO4_OCP), OCP_INT, (1 << 4)),
	DECLARE_IRQ(_INT(LDO5_OCP), OCP_INT, (1 << 5)),
	DECLARE_IRQ(_INT(LDO6_OCP), OCP_INT, (1 << 6)),
	DECLARE_IRQ(_INT(LDO7_OCP), OCP_INT, (1 << 7)),
	DECLARE_IRQ(_INT(TW_FALL), TSD_INT, (1 << 6)),
	DECLARE_IRQ(_INT(TSD), TSD_INT, (1 << 5)),
	DECLARE_IRQ(_INT(TW_RISE), TSD_INT, (1 << 4)),
};

static void s2mpb06_irq_lock(struct irq_data *data)
{
	struct s2mpb06_dev *s2mpb06 = irq_get_chip_data(data->irq);

	mutex_lock(&s2mpb06->irqlock);
}

static void s2mpb06_irq_sync_unlock(struct irq_data *data)
{
	struct s2mpb06_dev *s2mpb06 = irq_get_chip_data(data->irq);
	int i;

	for (i = 0; i < S2MPB06_IRQ_GROUP_NR; i++) {
		u8 mask_reg = s2mpb06_mask_reg[i];
		struct i2c_client *i2c = s2mpb06->i2c;

		if (mask_reg == S2MPB06_REG_INVALID || IS_ERR_OR_NULL(i2c))
			continue;
		s2mpb06->irq_masks_cache[i] = s2mpb06->irq_masks_cur[i];

		s2mpb06_write_reg(i2c, s2mpb06_mask_reg[i],
				~s2mpb06->irq_masks_cur[i]);
	}

	mutex_unlock(&s2mpb06->irqlock);
}

static const inline struct s2mpb06_irq_data *
irq_to_s2mpb06_irq(struct s2mpb06_dev *s2mpb06, int irq)
{
	return &s2mpb06_irqs[irq - s2mpb06->irq_base];
}

static void s2mpb06_irq_mask(struct irq_data *data)
{
	struct s2mpb06_dev *s2mpb06 = irq_get_chip_data(data->irq);
	const struct s2mpb06_irq_data *irq_data =
	    irq_to_s2mpb06_irq(s2mpb06, data->irq);

	if (irq_data->group >= S2MPB06_IRQ_GROUP_NR)
		return;

	s2mpb06->irq_masks_cur[irq_data->group] |= irq_data->mask;
}

static void s2mpb06_irq_unmask(struct irq_data *data)
{
	struct s2mpb06_dev *s2mpb06 = irq_get_chip_data(data->irq);
	const struct s2mpb06_irq_data *irq_data =
	    irq_to_s2mpb06_irq(s2mpb06, data->irq);

	if (irq_data->group >= S2MPB06_IRQ_GROUP_NR)
		return;

	s2mpb06->irq_masks_cur[irq_data->group] &= ~irq_data->mask;
}

static struct irq_chip s2mpb06_irq_chip = {
	.name			= MFD_DEV_NAME,
	.irq_bus_lock		= s2mpb06_irq_lock,
	.irq_bus_sync_unlock	= s2mpb06_irq_sync_unlock,
	.irq_mask		= s2mpb06_irq_mask,
	.irq_unmask		= s2mpb06_irq_unmask,
};

static void s2mpb06_irq_masking(struct s2mpb06_dev *s2mpb06, uint8_t *irq_reg)
{
	int i;

	for (i = 0; i < S2MPB06_IRQ_GROUP_NR; i++)
		irq_reg[i] &= ~s2mpb06->irq_masks_cur[i];
}

static void s2mpb06_irq_report(struct s2mpb06_dev *s2mpb06, uint8_t *irq_reg)
{
	int i;

	for (i = 0; i < S2MPB06_IRQ_NR; i++)
		if (irq_reg[s2mpb06_irqs[i].group] & s2mpb06_irqs[i].mask)
			handle_nested_irq(s2mpb06->irq_base + i);
}

static uint8_t s2mpb06_convert_tsd(const uint8_t tsd_int_val)
{
	uint8_t convert_val = 0;

	convert_val |= (tsd_int_val & (1 << 4)) << 2;
	convert_val |= (tsd_int_val & (1 << 1)) << 4;
	convert_val |= (tsd_int_val & (1 << 0)) << 4;

	return convert_val;
}

static irqreturn_t s2mpb06_irq_thread(int irq, void *data)
{
	struct s2mpb06_dev *s2mpb06 = data;
	uint8_t irq_reg[S2MPB06_IRQ_GROUP_NR] = {0};
	int ret;

	pr_debug("%s: irq gpio state(0x%02hhx)\n", __func__,
			gpio_get_value(s2mpb06->irq_gpio));

	ret = s2mpb06_read_reg(s2mpb06->i2c, S2MPB06_REG_OCP_STATUS, &irq_reg[OCP_INT]);
	ret = s2mpb06_write_reg(s2mpb06->i2c, S2MPB06_REG_OCP_STATUS, irq_reg[OCP_INT]);
	pr_info("%s: OCP interrupt(0x%02hhx)\n", __func__, irq_reg[OCP_INT]);

	ret = s2mpb06_read_reg(s2mpb06->i2c, S2MPB06_REG_UVLO_TSD_STATUS, &irq_reg[TSD_INT]);
	ret = s2mpb06_write_reg(s2mpb06->i2c, S2MPB06_REG_UVLO_TSD_STATUS, irq_reg[TSD_INT]);
	pr_info("%s: TSD interrupt(0x%02hhx)\n", __func__, irq_reg[TSD_INT]);

	irq_reg[TSD_INT] = s2mpb06_convert_tsd(irq_reg[TSD_INT]);
	pr_info("%s: Convert TSD(0x%02hhx)\n", __func__, irq_reg[TSD_INT]);

	s2mpb06_irq_masking(s2mpb06, irq_reg);
	s2mpb06_irq_report(s2mpb06, irq_reg);

	return IRQ_HANDLED;
}

int s2mpb06_irq_init(struct s2mpb06_dev *s2mpb06)
{
	int i;
	int ret;

	if (!s2mpb06->irq_gpio) {
		pr_warn("%s: No interrupt specified.\n", __func__);
		s2mpb06->irq_base = 0;
		return 0;
	}

	if (s2mpb06->irq_base < 0) {
		pr_err("%s: No interrupt base specified.\n", __func__);
		return 0;
	}

	mutex_init(&s2mpb06->irqlock);

	s2mpb06->irq = gpio_to_irq(s2mpb06->irq_gpio);
	pr_info("%s: irq=%d, irq->gpio=%d\n", __func__,
			s2mpb06->irq, s2mpb06->irq_gpio);

	ret = gpio_request(s2mpb06->irq_gpio, "camera_pmic_irq");
	if (ret) {
		pr_err("%s: failed requesting gpio %d\n",
			__func__, s2mpb06->irq_gpio);
		goto err;
	}
	gpio_direction_input(s2mpb06->irq_gpio);
	gpio_free(s2mpb06->irq_gpio);

	/* Mask interrupt sources */
	for (i = 0; i < S2MPB06_IRQ_GROUP_NR; i++) {
		struct i2c_client *i2c;

		s2mpb06->irq_masks_cur[i] = 0xff;
		s2mpb06->irq_masks_cache[i] = 0xff;

		i2c = s2mpb06->i2c;

		if (IS_ERR_OR_NULL(i2c))
			continue;
		if (s2mpb06_mask_reg[i] == S2MPB06_REG_INVALID)
			continue;
		s2mpb06_write_reg(i2c, s2mpb06_mask_reg[i], 0x00);
	}

	/* Register with genirq */
	for (i = 0; i < S2MPB06_IRQ_NR; i++) {
		int cur_irq = i + s2mpb06->irq_base;
		ret = irq_set_chip_data(cur_irq, s2mpb06);
		if (ret) {
			dev_err(s2mpb06->dev,
				"Failed to irq_set_chip_data %d: %d\n",
				s2mpb06->irq, ret);
			goto err;
		}
		irq_set_chip_and_handler(cur_irq, &s2mpb06_irq_chip,
						 handle_edge_irq);
		irq_set_nested_thread(cur_irq, 1);
#if IS_ENABLED(CONFIG_ARM)
		set_irq_flags(cur_irq, IRQF_VALID);
#else
		irq_set_noprobe(cur_irq);
#endif
	}

	ret = devm_request_threaded_irq(s2mpb06->dev, s2mpb06->irq, NULL,
			s2mpb06_irq_thread, IRQF_TRIGGER_LOW | IRQF_ONESHOT,
			"s2mpb06-irq", s2mpb06);
	if (ret) {
		pr_err("%s: Failed to request IRQ %d: %d\n",
			__func__, s2mpb06->irq, ret);
		goto err;
	}

	return 0;
err:
	mutex_destroy(&s2mpb06->irqlock);
	return ret;
}
EXPORT_SYMBOL_GPL(s2mpb06_irq_init);

void s2mpb06_irq_exit(struct s2mpb06_dev *s2mpb06)
{
	if (s2mpb06->irq)
		free_irq(s2mpb06->irq, s2mpb06);
	mutex_destroy(&s2mpb06->irqlock);
}
EXPORT_SYMBOL_GPL(s2mpb06_irq_exit);
