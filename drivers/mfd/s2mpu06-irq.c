/*
 6* s2mpu06-irq.c - Interrupt controller support for S2MPU06
 *
 * Copyright (C) 2015 Samsung Electronics Co.Ltd
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
*/

#include <linux/err.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/mfd/samsung/s2mpu06.h>
#include <linux/mfd/samsung/s2mpu06-private.h>

#include <sound/soc.h>
#include <sound/cod9002x.h>

static const u8 s2mpu06_mask_reg[] = {
	/* TODO: Need to check other INTMASK */
	[PMIC_INT1] = 	S2MPU06_PMIC_REG_INT1M,
	[PMIC_INT2] =	S2MPU06_PMIC_REG_INT2M,
	[PMIC_INT3] = 	S2MPU06_PMIC_REG_INT3M,
	[CHG_INT1] = 	S2MPU06_CHG_REG_INT1M,
	[CHG_INT2] = 	S2MPU06_CHG_REG_INT2M,
	[CHG_INT3] = 	S2MPU06_CHG_REG_INT3M,
	[CHG_PMIC_INT] = S2MPU06_CHG_REG_PMIC_INTM,
	[FG_INT] = 	S2MPU06_FG_REG_IRQ_INTM,
};

static struct i2c_client *get_i2c(struct s2mpu06_dev *s2mpu06,
				enum s2mpu06_irq_source src)
{
	switch (src) {
	case PMIC_INT1 ... PMIC_INT3:
		return s2mpu06->pmic;
	case CHG_INT1 ... CHG_PMIC_INT:
		return s2mpu06->charger;
	case FG_INT:
		return s2mpu06->fuelgauge;
	default:
		return ERR_PTR(-EINVAL);
	}
}

struct s2mpu06_irq_data {
	int mask;
	enum s2mpu06_irq_source group;
};

#define DECLARE_IRQ(idx, _group, _mask)		\
	[(idx)] = { .group = (_group), .mask = (_mask) }
static const struct s2mpu06_irq_data s2mpu06_irqs[] = {
	DECLARE_IRQ(S2MPU06_PMIC_IRQ_PWRONR_INT1,	PMIC_INT1, 1 << 1),
	DECLARE_IRQ(S2MPU06_PMIC_IRQ_PWRONF_INT1,	PMIC_INT1, 1 << 0),
	DECLARE_IRQ(S2MPU06_PMIC_IRQ_JIGONBF_INT1,	PMIC_INT1, 1 << 2),
	DECLARE_IRQ(S2MPU06_PMIC_IRQ_JIGONBR_INT1,	PMIC_INT1, 1 << 3),
	DECLARE_IRQ(S2MPU06_PMIC_IRQ_ACOKBF_INT1,	PMIC_INT1, 1 << 4),
	DECLARE_IRQ(S2MPU06_PMIC_IRQ_ACOKBR_INT1,	PMIC_INT1, 1 << 5),
	DECLARE_IRQ(S2MPU06_PMIC_IRQ_PWRON1S_INT1,	PMIC_INT1, 1 << 6),
	DECLARE_IRQ(S2MPU06_PMIC_IRQ_MRB_INT1,		PMIC_INT1, 1 << 7),

	DECLARE_IRQ(S2MPU06_PMIC_IRQ_RTC60S_INT2,	PMIC_INT2, 1 << 0),
	DECLARE_IRQ(S2MPU06_PMIC_IRQ_RTCA1_INT2,	PMIC_INT2, 1 << 1),
	DECLARE_IRQ(S2MPU06_PMIC_IRQ_RTCA0_INT2,	PMIC_INT2, 1 << 2),
	DECLARE_IRQ(S2MPU06_PMIC_IRQ_SMPL_INT2,		PMIC_INT2, 1 << 3),
	DECLARE_IRQ(S2MPU06_PMIC_IRQ_RTC1S_INT2,	PMIC_INT2, 1 << 4),
	DECLARE_IRQ(S2MPU06_PMIC_IRQ_WTSR_INT2,		PMIC_INT2, 1 << 5),
	DECLARE_IRQ(S2MPU06_PMIC_IRQ_WRSTB_INT2,	PMIC_INT2, 1 << 7),

	DECLARE_IRQ(S2MPU06_PMIC_IRQ_120C_INT3,		PMIC_INT3, 1 << 0),
	DECLARE_IRQ(S2MPU06_PMIC_IRQ_140C_INT3,		PMIC_INT3, 1 << 1),
	DECLARE_IRQ(S2MPU06_PMIC_IRQ_TSD_INT3,		PMIC_INT3, 1 << 2),

	DECLARE_IRQ(S2MPU06_PMIC_IRQ_120C_INT3,		PMIC_INT3, 1 << 0),
	DECLARE_IRQ(S2MPU06_PMIC_IRQ_140C_INT3,		PMIC_INT3, 1 << 1),
	DECLARE_IRQ(S2MPU06_PMIC_IRQ_TSD_INT3,		PMIC_INT3, 1 << 2),

	DECLARE_IRQ(S2MPU06_CHG_IRQ_EOC_INT1,		CHG_INT1, 1 << 1),
	DECLARE_IRQ(S2MPU06_CHG_IRQ_CINIR_INT1,		CHG_INT1, 1 << 2),
	DECLARE_IRQ(S2MPU06_CHG_IRQ_BATP_INT1,		CHG_INT1, 1 << 3),
	DECLARE_IRQ(S2MPU06_CHG_IRQ_BATLV_INT1,		CHG_INT1, 1 << 4),
	DECLARE_IRQ(S2MPU06_CHG_IRQ_TOPOFF_INT1,	CHG_INT1, 1 << 5),
	DECLARE_IRQ(S2MPU06_CHG_IRQ_CINOVP_INT1,	CHG_INT1, 1 << 6),
	DECLARE_IRQ(S2MPU06_CHG_IRQ_CHGTSD_INT1,	CHG_INT1, 1 << 7),

	DECLARE_IRQ(S2MPU06_CHG_IRQ_CHGVINVR_INT2,	CHG_INT2, 1 << 0),
	DECLARE_IRQ(S2MPU06_CHG_IRQ_CHGTR_INT2,		CHG_INT2, 1 << 1),
	DECLARE_IRQ(S2MPU06_CHG_IRQ_TMROUT_INT2,	CHG_INT2, 1 << 2),
	DECLARE_IRQ(S2MPU06_CHG_IRQ_RECHG_INT2,		CHG_INT2, 1 << 3),
	DECLARE_IRQ(S2MPU06_CHG_IRQ_CHGTERM_INT2,	CHG_INT2, 1 << 4),
	DECLARE_IRQ(S2MPU06_CHG_IRQ_BATOVP_INT2,	CHG_INT2, 1 << 5),
	DECLARE_IRQ(S2MPU06_CHG_IRQ_CHGVIN_INT2,	CHG_INT2, 1 << 6),
	DECLARE_IRQ(S2MPU06_CHG_IRQ_CIN2BAT_INT2,	CHG_INT2, 1 << 7),

	DECLARE_IRQ(S2MPU06_CHG_IRQ_CHGSTS_INT3,	CHG_INT3, 1 << 1),
	DECLARE_IRQ(S2MPU06_CHG_IRQ_OTGILIM_INT3,	CHG_INT3, 1 << 4),
	DECLARE_IRQ(S2MPU06_CHG_IRQ_BSTINLV_INT3,	CHG_INT3, 1 << 5),
	DECLARE_IRQ(S2MPU06_CHG_IRQ_BSTILIM_INT3,	CHG_INT3, 1 << 6),
	DECLARE_IRQ(S2MPU06_CHG_IRQ_VMIDOVP_INT3,	CHG_INT3, 1 << 7),

	DECLARE_IRQ(S2MPU06_CHG_IRQ_WDT_PM,		CHG_PMIC_INT, 1 << 0),
	DECLARE_IRQ(S2MPU06_CHG_IRQ_TSD_PM,		CHG_PMIC_INT, 1 << 6),
	DECLARE_IRQ(S2MPU06_CHG_IRQ_VDDALV_PM,		CHG_PMIC_INT, 1 << 7),

	DECLARE_IRQ(S2MPU06_FG_IRQ_VBAT_L_INT,		FG_INT, 1 << 0),
	DECLARE_IRQ(S2MPU06_FG_IRQ_SOC_L_INT,		FG_INT, 1 << 1),
	DECLARE_IRQ(S2MPU06_FG_IRQ_IDLE_ST_INT,		FG_INT, 1 << 2),
	DECLARE_IRQ(S2MPU06_FG_IRQ_INIT_ST_INT,		FG_INT, 1 << 3),
};

static const char * const s2mpu06_irqs_name[] = {
	/* PMIC */
	"S2MPU06_PMIC_IRQ_PWRONR_INT1",
	"S2MPU06_PMIC_IRQ_PWRONF_INT1",
	"S2MPU06_PMIC_IRQ_JIGONBF_INT1",
	"S2MPU06_PMIC_IRQ_JIGONBR_INT1",
	"S2MPU06_PMIC_IRQ_ACOKBF_INT1",
	"S2MPU06_PMIC_IRQ_ACOKBR_INT1",
	"S2MPU06_PMIC_IRQ_PWRON1S_INT1",
	"S2MPU06_PMIC_IRQ_MRB_INT1",

	"S2MPU06_PMIC_IRQ_RTC60S_INT2",
	"S2MPU06_PMIC_IRQ_RTCA1_INT2",
	"S2MPU06_PMIC_IRQ_RTCA0_INT2",
	"S2MPU06_PMIC_IRQ_SMPL_INT2",
	"S2MPU06_PMIC_IRQ_RTC1S_INT2",
	"S2MPU06_PMIC_IRQ_WTSR_INT2",
	"S2MPU06_PMIC_IRQ_WRSTB_INT2",

	"S2MPU06_PMIC_IRQ_120C_INT3",
	"S2MPU06_PMIC_IRQ_140C_INT3",
	"S2MPU06_PMIC_IRQ_TSD_INT3",

	/* Charger */
	"S2MPU06_CHG_IRQ_EOC_INT1",
	"S2MPU06_CHG_IRQ_CINIR_INT1",
	"S2MPU06_CHG_IRQ_BATP_INT1",
	"S2MPU06_CHG_IRQ_BATLV_INT1",
	"S2MPU06_CHG_IRQ_TOPOFF_INT1",
	"S2MPU06_CHG_IRQ_CINOVP_INT1",
	"S2MPU06_CHG_IRQ_CHGTSD_INT1",

	"S2MPU06_CHG_IRQ_CHGVINVR_INT2",
	"S2MPU06_CHG_IRQ_CHGTR_INT2",
	"S2MPU06_CHG_IRQ_TMROUT_INT2",
	"S2MPU06_CHG_IRQ_RECHG_INT2",
	"S2MPU06_CHG_IRQ_CHGTERM_INT2",
	"S2MPU06_CHG_IRQ_BATOVP_INT2",
	"S2MPU06_CHG_IRQ_CHGVIN_INT2",
	"S2MPU06_CHG_IRQ_CIN2BAT_INT2",

	"S2MPU06_CHG_IRQ_CHGSTS_INT3",
	"S2MPU06_CHG_IRQ_OTGILIM_INT3",
	"S2MPU06_CHG_IRQ_BSTINLV_INT3",
	"S2MPU06_CHG_IRQ_BSTILIM_INT3",
	"S2MPU06_CHG_IRQ_VMIDOVP_INT3",

	"S2MPU06_CHG_IRQ_WDT_PM",
	"S2MPU06_CHG_IRQ_TSD_PM",
	"S2MPU06_CHG_IRQ_VDDALV_PM",

	/* Fuelgauge */
	"S2MPU06_FG_IRQ_VBAT_L_INT",
	"S2MPU06_FG_IRQ_SOC_L_INT",
	"S2MPU06_FG_IRQ_IDLE_ST_INT",
	"S2MPU06_FG_IRQ_INIT_ST_INT",

	"S2MPU06_IRQ_NR"
};

static void s2mpu06_irq_lock(struct irq_data *data)
{
	struct s2mpu06_dev *s2mpu06 = irq_get_chip_data(data->irq);

	mutex_lock(&s2mpu06->irqlock);
}

static void s2mpu06_irq_sync_unlock(struct irq_data *data)
{
	struct s2mpu06_dev *s2mpu06 = irq_get_chip_data(data->irq);
	int i;

	for (i = 0; i < S2MPU06_IRQ_GROUP_NR; i++) {
		u8 mask_reg = s2mpu06_mask_reg[i];
		struct i2c_client *i2c = get_i2c(s2mpu06, i);

		if (mask_reg == S2MPU06_REG_INVALID ||
				IS_ERR_OR_NULL(i2c))
			continue;
		s2mpu06->irq_masks_cache[i] = s2mpu06->irq_masks_cur[i];

		s2mpu06_write_reg(i2c, s2mpu06_mask_reg[i],
				s2mpu06->irq_masks_cur[i]);
	}

	mutex_unlock(&s2mpu06->irqlock);
}

static const inline struct s2mpu06_irq_data *
irq_to_s2mpu06_irq(struct s2mpu06_dev *s2mpu06, int irq)
{
	return &s2mpu06_irqs[irq - s2mpu06->irq_base];
}

static void s2mpu06_irq_mask(struct irq_data *data)
{
	struct s2mpu06_dev *s2mpu06 = irq_get_chip_data(data->irq);
	const struct s2mpu06_irq_data *irq_data =
	    irq_to_s2mpu06_irq(s2mpu06, data->irq);

	if (irq_data->group >= S2MPU06_IRQ_GROUP_NR)
		return;

	s2mpu06->irq_masks_cur[irq_data->group] |= irq_data->mask;
}

static void s2mpu06_irq_unmask(struct irq_data *data)
{
	struct s2mpu06_dev *s2mpu06 = irq_get_chip_data(data->irq);
	const struct s2mpu06_irq_data *irq_data =
	    irq_to_s2mpu06_irq(s2mpu06, data->irq);

	if (irq_data->group >= S2MPU06_IRQ_GROUP_NR)
		return;

	s2mpu06->irq_masks_cur[irq_data->group] &= ~irq_data->mask;
}

static struct irq_chip s2mpu06_irq_chip = {
	.name			= MFD_DEV_NAME,
	.irq_bus_lock		= s2mpu06_irq_lock,
	.irq_bus_sync_unlock	= s2mpu06_irq_sync_unlock,
	.irq_mask		= s2mpu06_irq_mask,
	.irq_unmask		= s2mpu06_irq_unmask,
};

int codec_notifier_flag = 0;

void set_codec_notifier_flag()
{
	codec_notifier_flag = 1;
}
EXPORT_SYMBOL(set_codec_notifier_flag);

static irqreturn_t s2mpu06_irq_thread(int irq, void *data)
{
	struct s2mpu06_dev *s2mpu06 = data;
	u8 irq_reg[S2MPU06_IRQ_GROUP_NR] = {0};
	u8 irq_src;
	u8 irq1_codec, irq2_codec, irq3_codec, status1;
	int i, ret;

	pr_debug("%s: irq gpio pre-state(0x%02x)\n", __func__,
				gpio_get_value(s2mpu06->irq_gpio));

	ret = s2mpu06_read_reg(s2mpu06->i2c,
					S2MPU06_PMIC_REG_INTSRC, &irq_src);
	if (ret) {
		pr_err("%s:%s Failed to read interrupt source: %d\n",
			MFD_DEV_NAME, __func__, ret);
		return IRQ_NONE;
	}

	pr_info("%s: interrupt source(0x%02x)\n", __func__, irq_src);

	if (irq_src & S2MPU06_IRQSRC_PMIC) {
		/* PMIC_INT */
		ret = s2mpu06_bulk_read(s2mpu06->pmic, S2MPU06_PMIC_REG_INT1,
				S2MPU06_NUM_IRQ_PMIC_REGS, &irq_reg[PMIC_INT1]);
		if (ret) {
			pr_err("%s:%s Failed to read pmic interrupt: %d\n",
				MFD_DEV_NAME, __func__, ret);
			return IRQ_NONE;
		}

		pr_info("%s: pmic interrupt(0x%02x, 0x%02x, 0x%02x)\n",
			__func__, irq_reg[PMIC_INT1], irq_reg[PMIC_INT2],
			irq_reg[PMIC_INT3]);

		pr_info("%s: pmic interrupt mask(0x%02x, 0x%02x, 0x%02x)\n",
			__func__, s2mpu06->irq_masks_cur[PMIC_INT1], 
			s2mpu06->irq_masks_cur[PMIC_INT2], 
			s2mpu06->irq_masks_cur[PMIC_INT3]);
	}

	if(irq_src & S2MPU06_IRQSRC_CODEC) {
		if(s2mpu06->codec){
			pr_err("%s codec interrupt occur\n", __func__);
			ret = s2mpu06_read_codec_reg(s2mpu06->codec, 0x1, &irq1_codec);
			ret = s2mpu06_read_codec_reg(s2mpu06->codec, 0x2, &irq2_codec);
			ret = s2mpu06_read_codec_reg(s2mpu06->codec, 0x3, &irq3_codec);
			ret = s2mpu06_read_codec_reg(s2mpu06->codec, 0x7, &status1);

			if(codec_notifier_flag)
				cod9002x_call_notifier(irq1_codec, irq2_codec, irq3_codec, status1);
		}
	}

	if (irq_src & S2MPU06_IRQSRC_CHG) {
		/* CHG_INT */
		ret = s2mpu06_bulk_read(s2mpu06->charger, S2MPU06_CHG_REG_INT1,
				S2MPU06_NUM_IRQ_CHG_REGS, &irq_reg[CHG_INT1]);
		if (ret) {
			pr_err("%s:%s Failed to read charger interrupt: %d\n",
				MFD_DEV_NAME, __func__, ret);
			return IRQ_NONE;
		}

		pr_info("%s: charger interrupt(0x%02x, 0x%02x, 0x%02x, 0x%02x)\n",
			__func__, irq_reg[CHG_INT1], irq_reg[CHG_INT2], irq_reg[CHG_INT3],
			irq_reg[CHG_PMIC_INT]);

		pr_info("%s: charger interrupt mask(0x%02x, 0x%02x, 0x%02x, 0x%02x)\n",
			__func__, s2mpu06->irq_masks_cur[CHG_INT1],
			s2mpu06->irq_masks_cur[CHG_INT2], s2mpu06->irq_masks_cur[CHG_INT3],
			s2mpu06->irq_masks_cur[CHG_PMIC_INT]);
	}

	if (irq_src & S2MPU06_IRQSRC_FG) {
		/* FG_INT */
		ret = s2mpu06_read_reg(s2mpu06->fuelgauge, S2MPU06_FG_REG_IRQ_INT,
				&irq_reg[FG_INT]);
		if (ret) {
			pr_err("%s:%s Failed to read charger interrupt: %d\n",
				MFD_DEV_NAME, __func__, ret);
			return IRQ_NONE;
		}
		pr_info("%s: fuelgauge interrupt(0x%02x)\n", __func__,
			irq_reg[FG_INT]);

		pr_info("%s: fuelgauge interrupt mask(0x%02x)\n", __func__,
			s2mpu06->irq_masks_cur[FG_INT]);
	}
	/* Apply masking */
	for (i = 0; i < S2MPU06_IRQ_GROUP_NR; i++)
		irq_reg[i] &= ~s2mpu06->irq_masks_cur[i];

	/* Report */
	for (i = 0; i < S2MPU06_IRQ_NR; i++) {
		if (irq_reg[s2mpu06_irqs[i].group] & s2mpu06_irqs[i].mask) {
			pr_info("%s: interrupt caused by %s\n", __func__, s2mpu06_irqs_name[i]);
			handle_nested_irq(s2mpu06->irq_base + i);
		}
	}

	return IRQ_HANDLED;
}

int s2mpu06_irq_init(struct s2mpu06_dev *s2mpu06)
{
	int i;
	int ret;
	u8 i2c_data;
	int cur_irq;

	if (!s2mpu06->irq_gpio) {
		dev_warn(s2mpu06->dev, "No interrupt specified.\n");
		s2mpu06->irq_base = 0;
		return 0;
	}

	if (!s2mpu06->irq_base) {
		dev_err(s2mpu06->dev, "No interrupt base specified.\n");
		return 0;
	}

	mutex_init(&s2mpu06->irqlock);

	s2mpu06->irq = gpio_to_irq(s2mpu06->irq_gpio);
	pr_info("%s:%s irq=%d, irq->gpio=%d\n", MFD_DEV_NAME, __func__,
			s2mpu06->irq, s2mpu06->irq_gpio);

	ret = gpio_request(s2mpu06->irq_gpio, "if_pmic_irq");
	if (ret) {
		dev_err(s2mpu06->dev, "%s: failed requesting gpio %d\n",
			__func__, s2mpu06->irq_gpio);
		return ret;
	}
	gpio_direction_input(s2mpu06->irq_gpio);
	gpio_free(s2mpu06->irq_gpio);

	/* Mask individual interrupt sources */
	for (i = 0; i < S2MPU06_IRQ_GROUP_NR; i++) {
		struct i2c_client *i2c;

		s2mpu06->irq_masks_cur[i] = 0xff;
		s2mpu06->irq_masks_cache[i] = 0xff;

		i2c = get_i2c(s2mpu06, i);

		if (IS_ERR_OR_NULL(i2c))
			continue;
		if (s2mpu06_mask_reg[i] == S2MPU06_REG_INVALID)
			continue;

		s2mpu06_write_reg(i2c, s2mpu06_mask_reg[i], 0xff);
	}

	/* Register with genirq */
	for (i = 0; i < S2MPU06_IRQ_NR; i++) {
		cur_irq = i + s2mpu06->irq_base;
		irq_set_chip_data(cur_irq, s2mpu06);
		irq_set_chip_and_handler(cur_irq, &s2mpu06_irq_chip,
					 handle_level_irq);
		irq_set_nested_thread(cur_irq, 1);
#ifdef CONFIG_ARM
		set_irq_flags(cur_irq, IRQF_VALID);
#else
		irq_set_noprobe(cur_irq);
#endif
	}

	s2mpu06_write_reg(s2mpu06->i2c, S2MPU06_PMIC_REG_INTSRC_MASK, 0xff);
	/* Unmask s2mpu06 interrupt */
	ret = s2mpu06_read_reg(s2mpu06->i2c, S2MPU06_PMIC_REG_INTSRC_MASK,
			  &i2c_data);
	if (ret) {
		pr_err("%s:%s fail to read intsrc mask reg\n",
					 MFD_DEV_NAME, __func__);
		return ret;
	}

	if (s2mpu06->pmic_rev)
		i2c_data &= ~(S2MPU06_IRQSRC_PMIC);	/* Unmask pmic interrupt */
	i2c_data &= ~(S2MPU06_IRQSRC_CHG);	/* Unmask charger interrupt */
	i2c_data &= ~(S2MPU06_IRQSRC_FG);	/* Unmask fuelgauge interrupt */

	if(s2mpu06->codec)
		i2c_data &= ~(S2MPU06_IRQSRC_CODEC);

	s2mpu06_write_reg(s2mpu06->i2c, S2MPU06_PMIC_REG_INTSRC_MASK,
			   i2c_data);

	pr_info("%s:%s s2mpu06_PMIC_REG_INTSRC_MASK=0x%02x\n",
			MFD_DEV_NAME, __func__, i2c_data);

	ret = request_threaded_irq(s2mpu06->irq, NULL, s2mpu06_irq_thread,
				   IRQF_TRIGGER_LOW | IRQF_ONESHOT,
				   "s2mpu06-irq", s2mpu06);

	if (ret) {
		dev_err(s2mpu06->dev, "Failed to request IRQ %d: %d\n",
			s2mpu06->irq, ret);
		return ret;
	}

	return 0;
}

void s2mpu06_irq_exit(struct s2mpu06_dev *s2mpu06)
{
	if (s2mpu06->irq)
		free_irq(s2mpu06->irq, s2mpu06);
}
