/*
 * s2mpu10-irq.c - Interrupt controller support for S2MPU10
 *
 * Copyright (C) 2016 Samsung Electronics Co.Ltd
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
#include <linux/mfd/samsung/s2mpu10.h>
#include <linux/mfd/samsung/s2mpu10-regulator.h>
#include <linux/mfd/samsung/s2mpu11-regulator.h>
#include <linux/of_irq.h>
#include <linux/sizes.h>
#include <asm/io.h>
#include <linux/workqueue.h>

#define S2MPU10_IBI_CNT		4

#if defined(CONFIG_SND_SOC_AUD3003X_5PIN) || defined(CONFIG_SND_SOC_AUD3003X_6PIN)
#include <sound/aud3003x.h>
#endif

static u8 irq_reg[S2MPU10_IRQ_GROUP_NR] = {0};

static const u8 s2mpu10_mask_reg[] = {
	[PMIC_INT1] = 	S2MPU10_PMIC_REG_INT1M,
	[PMIC_INT2] =	S2MPU10_PMIC_REG_INT2M,
	[PMIC_INT3] = 	S2MPU10_PMIC_REG_INT3M,
	[PMIC_INT4] =	S2MPU10_PMIC_REG_INT4M,
	[PMIC_INT5] = 	S2MPU10_PMIC_REG_INT5M,
	[PMIC_INT6] = 	S2MPU10_PMIC_REG_INT6M,
};

static struct i2c_client *get_i2c(struct s2mpu10_dev *s2mpu10,
				enum s2mpu10_irq_source src)
{
	switch (src) {
	case PMIC_INT1 ... PMIC_INT6:
		return s2mpu10->pmic;
	default:
		return ERR_PTR(-EINVAL);
	}
}

struct s2mpu10_irq_data {
	int mask;
	enum s2mpu10_irq_source group;
};

#define DECLARE_IRQ(idx, _group, _mask)		\
	[(idx)] = { .group = (_group), .mask = (_mask) }
static const struct s2mpu10_irq_data s2mpu10_irqs[] = {
	DECLARE_IRQ(S2MPU10_PMIC_IRQ_PWRONR_INT1,	PMIC_INT1, 1 << 1),
	DECLARE_IRQ(S2MPU10_PMIC_IRQ_PWRONF_INT1,	PMIC_INT1, 1 << 0),
	DECLARE_IRQ(S2MPU10_PMIC_IRQ_JIGONBF_INT1,	PMIC_INT1, 1 << 2),
	DECLARE_IRQ(S2MPU10_PMIC_IRQ_JIGONBR_INT1,	PMIC_INT1, 1 << 3),
	DECLARE_IRQ(S2MPU10_PMIC_IRQ_ACOKF_INT1,	PMIC_INT1, 1 << 4),
	DECLARE_IRQ(S2MPU10_PMIC_IRQ_ACOKR_INT1,	PMIC_INT1, 1 << 5),
	DECLARE_IRQ(S2MPU10_PMIC_IRQ_PWRON1S_INT1,	PMIC_INT1, 1 << 6),
	DECLARE_IRQ(S2MPU10_PMIC_IRQ_MRB_INT1,		PMIC_INT1, 1 << 7),

	DECLARE_IRQ(S2MPU10_PMIC_IRQ_RTC60S_INT2,	PMIC_INT2, 1 << 0),
	DECLARE_IRQ(S2MPU10_PMIC_IRQ_RTCA1_INT2,	PMIC_INT2, 1 << 1),
	DECLARE_IRQ(S2MPU10_PMIC_IRQ_RTCA0_INT2,	PMIC_INT2, 1 << 2),
	DECLARE_IRQ(S2MPU10_PMIC_IRQ_SMPL_INT2,		PMIC_INT2, 1 << 3),
	DECLARE_IRQ(S2MPU10_PMIC_IRQ_RTC1S_INT2,	PMIC_INT2, 1 << 4),
	DECLARE_IRQ(S2MPU10_PMIC_IRQ_WTSR_INT2,		PMIC_INT2, 1 << 5),
	DECLARE_IRQ(S2MPU10_PMIC_IRQ_ADCDONE_INT2,	PMIC_INT2, 1 << 6),
	DECLARE_IRQ(S2MPU10_PMIC_IRQ_WTSRB_INT2,	PMIC_INT2, 1 << 7),

	DECLARE_IRQ(S2MPU10_PMIC_IRQ_OCPB1_INT3,	PMIC_INT3, 1 << 0),
	DECLARE_IRQ(S2MPU10_PMIC_IRQ_OCPB2_INT3,	PMIC_INT3, 1 << 1),
	DECLARE_IRQ(S2MPU10_PMIC_IRQ_OCPB3_INT3,	PMIC_INT3, 1 << 2),
	DECLARE_IRQ(S2MPU10_PMIC_IRQ_OCPB4_INT3,	PMIC_INT3, 1 << 3),
	DECLARE_IRQ(S2MPU10_PMIC_IRQ_OCPB5_INT3,	PMIC_INT3, 1 << 4),
	DECLARE_IRQ(S2MPU10_PMIC_IRQ_OCPB6_INT3,	PMIC_INT3, 1 << 5),
	DECLARE_IRQ(S2MPU10_PMIC_IRQ_OCPB7_INT3,	PMIC_INT3, 1 << 6),
	DECLARE_IRQ(S2MPU10_PMIC_IRQ_OCPB8_INT3,	PMIC_INT3, 1 << 7),

	DECLARE_IRQ(S2MPU10_PMIC_IRQ_OCPB9_INT4,	PMIC_INT4, 1 << 0),
	DECLARE_IRQ(S2MPU10_PMIC_IRQ_OCPB10_INT4,	PMIC_INT4, 1 << 1),
	DECLARE_IRQ(S2MPU10_PMIC_IRQ_UV_BB_INT4,	PMIC_INT4, 1 << 2),
	DECLARE_IRQ(S2MPU10_PMIC_IRQ_BB_NTR_DET_INT4,	PMIC_INT4, 1 << 3),
	DECLARE_IRQ(S2MPU10_PMIC_IRQ_SC_LDO1_INT4,	PMIC_INT4, 1 << 4),
	DECLARE_IRQ(S2MPU10_PMIC_IRQ_SC_LDO2_INT4,	PMIC_INT4, 1 << 5),
	DECLARE_IRQ(S2MPU10_PMIC_IRQ_SC_LDO7_INT4,	PMIC_INT4, 1 << 6),
	DECLARE_IRQ(S2MPU10_PMIC_IRQ_SC_LDO19_INT4,	PMIC_INT4, 1 << 7),

	DECLARE_IRQ(S2MPU10_PMIC_IRQ_OI_B1_INT5,	PMIC_INT5, 1 << 0),
	DECLARE_IRQ(S2MPU10_PMIC_IRQ_OI_B2_INT5,	PMIC_INT5, 1 << 1),
	DECLARE_IRQ(S2MPU10_PMIC_IRQ_OI_B3_INT5,	PMIC_INT5, 1 << 2),
	DECLARE_IRQ(S2MPU10_PMIC_IRQ_OI_B4_INT5,	PMIC_INT5, 1 << 3),
	DECLARE_IRQ(S2MPU10_PMIC_IRQ_OI_B5_INT5,	PMIC_INT5, 1 << 4),
	DECLARE_IRQ(S2MPU10_PMIC_IRQ_OI_B6_INT5,	PMIC_INT5, 1 << 5),
	DECLARE_IRQ(S2MPU10_PMIC_IRQ_OI_B7_INT5,	PMIC_INT5, 1 << 6),
	DECLARE_IRQ(S2MPU10_PMIC_IRQ_OI_B8_INT5,	PMIC_INT5, 1 << 7),

	DECLARE_IRQ(S2MPU10_PMIC_IRQ_OI_B9_INT6,	PMIC_INT6, 1 << 0),
	DECLARE_IRQ(S2MPU10_PMIC_IRQ_OI_BB_INT6,	PMIC_INT6, 1 << 1),
	DECLARE_IRQ(S2MPU10_PMIC_IRQ_INT120C_INT6,	PMIC_INT6, 1 << 2),
	DECLARE_IRQ(S2MPU10_PMIC_IRQ_INT140C_INT6,	PMIC_INT6, 1 << 3),
	DECLARE_IRQ(S2MPU10_PMIC_IRQ_TSD_INT6,		PMIC_INT6, 1 << 4),
	DECLARE_IRQ(S2MPU10_PMIC_IRQ_TIMEOUT2_INT6,	PMIC_INT6, 1 << 5),
	DECLARE_IRQ(S2MPU10_PMIC_IRQ_TIMEOUT3_INT6,	PMIC_INT6, 1 << 6),
	DECLARE_IRQ(S2MPU10_PMIC_IRQ_SUB_SMPL_INT6,	PMIC_INT6, 1 << 7),
};

static void s2mpu10_irq_lock(struct irq_data *data)
{
	struct s2mpu10_dev *s2mpu10 = irq_get_chip_data(data->irq);

	mutex_lock(&s2mpu10->irqlock);
}

static void s2mpu10_irq_sync_unlock(struct irq_data *data)
{
	struct s2mpu10_dev *s2mpu10 = irq_get_chip_data(data->irq);
	int i;

	for (i = 0; i < S2MPU10_IRQ_GROUP_NR; i++) {
		u8 mask_reg = s2mpu10_mask_reg[i];
		struct i2c_client *i2c = get_i2c(s2mpu10, i);

		if (mask_reg == S2MPU10_REG_INVALID ||
				IS_ERR_OR_NULL(i2c))
			continue;
		s2mpu10->irq_masks_cache[i] = s2mpu10->irq_masks_cur[i];

		s2mpu10_write_reg(i2c, s2mpu10_mask_reg[i],
				s2mpu10->irq_masks_cur[i]);
	}

	mutex_unlock(&s2mpu10->irqlock);
}

static const inline struct s2mpu10_irq_data *
irq_to_s2mpu10_irq(struct s2mpu10_dev *s2mpu10, int irq)
{
	return &s2mpu10_irqs[irq - s2mpu10->irq_base];
}

static void s2mpu10_irq_mask(struct irq_data *data)
{
	struct s2mpu10_dev *s2mpu10 = irq_get_chip_data(data->irq);
	const struct s2mpu10_irq_data *irq_data =
	    irq_to_s2mpu10_irq(s2mpu10, data->irq);

	if (irq_data->group >= S2MPU10_IRQ_GROUP_NR)
		return;

	s2mpu10->irq_masks_cur[irq_data->group] |= irq_data->mask;
}

static void s2mpu10_irq_unmask(struct irq_data *data)
{
	struct s2mpu10_dev *s2mpu10 = irq_get_chip_data(data->irq);
	const struct s2mpu10_irq_data *irq_data =
	    irq_to_s2mpu10_irq(s2mpu10, data->irq);

	if (irq_data->group >= S2MPU10_IRQ_GROUP_NR)
		return;

	s2mpu10->irq_masks_cur[irq_data->group] &= ~irq_data->mask;
}

static struct irq_chip s2mpu10_irq_chip = {
	.name			= MFD_DEV_NAME,
	.irq_bus_lock		= s2mpu10_irq_lock,
	.irq_bus_sync_unlock	= s2mpu10_irq_sync_unlock,
	.irq_mask		= s2mpu10_irq_mask,
	.irq_unmask		= s2mpu10_irq_unmask,
};

static void s2mpu10_report_irq(struct s2mpu10_dev *s2mpu10)
{
	int i;

	/* Apply masking */
	for (i = 0; i < S2MPU10_IRQ_GROUP_NR; i++)
		irq_reg[i] &= ~s2mpu10->irq_masks_cur[i];

	/* Report */
	for (i = 0; i < S2MPU10_IRQ_NR; i++) {
		if (irq_reg[s2mpu10_irqs[i].group] & s2mpu10_irqs[i].mask)
			handle_nested_irq(s2mpu10->irq_base + i);
	}
}

static int s2mpu10_power_key_detection(struct s2mpu10_dev *s2mpu10)
{
	int ret, i;
	u8 val;

	/* Determine falling/rising edge for PWR_ON key */
	if ((irq_reg[PMIC_INT1] & 0x3) == 0x3) {
		ret = s2mpu10_read_reg(s2mpu10->i2c,
				       S2MPU10_PMIC_REG_STATUS1, &val);
		if (ret) {
			pr_err("%s: fail to read register\n", __func__);
			goto power_key_err;
		}
		irq_reg[PMIC_INT1] &= 0xFC;

		if (val & S2MPU10_STATUS1_PWRON) {
			irq_reg[PMIC_INT1] = S2MPU10_FALLING_EDGE;
			s2mpu10_report_irq(s2mpu10);

			/* clear irq */
			for (i = 0; i < S2MPU10_IRQ_GROUP_NR; i++)
				irq_reg[i] &= 0x00;

			irq_reg[PMIC_INT1] = S2MPU10_RISING_EDGE;
		} else {
			irq_reg[PMIC_INT1] = S2MPU10_RISING_EDGE;
			s2mpu10_report_irq(s2mpu10);

			/* clear irq */
			for (i = 0; i < S2MPU10_IRQ_GROUP_NR; i++)
				irq_reg[i] &= 0x00;

			irq_reg[PMIC_INT1] = S2MPU10_FALLING_EDGE;
		}
	}
	return 0;

power_key_err:
	return 1;
}

#if defined(CONFIG_SND_SOC_AUD3003X_5PIN) || defined(CONFIG_SND_SOC_AUD3003X_6PIN)
int codec_notifier_flag = 0;

void set_codec_notifier_flag(bool on)
{
	if (on)
		codec_notifier_flag = true;
	else
		codec_notifier_flag = false;
}
#endif

static void s2mpu10_irq_work_func(struct work_struct *work)
{
	pr_info("%s: master pmic interrupt(0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x)\n",
		 __func__, irq_reg[PMIC_INT1], irq_reg[PMIC_INT2], irq_reg[PMIC_INT3],
		 irq_reg[PMIC_INT4], irq_reg[PMIC_INT5], irq_reg[PMIC_INT6]);
}

static irqreturn_t s2mpu10_irq_thread(int irq, void *data)
{
	struct s2mpu10_dev *s2mpu10 = data;
	u8 ibi_src[S2MPU10_IBI_CNT] = {0};
	u32 val;
	int i, ret;

	/* Read VGPIO_RX_MONITOR */
	val = readl(s2mpu10->mem_base);

	for (i = 0; i < S2MPU10_IBI_CNT; i++) {
		ibi_src[i] = val & 0xFF;
		val = (val >> 8);
	}

	/* notify Master PMIC */
	if (ibi_src[0] & S2MPU10_IBI0_PMIC_M) {
		ret = s2mpu10_bulk_read(s2mpu10->pmic, S2MPU10_PMIC_REG_INT1,
				S2MPU10_NUM_IRQ_PMIC_REGS, &irq_reg[PMIC_INT1]);
		if (ret) {
			pr_err("%s:%s Failed to read pmic interrupt: %d\n",
				MFD_DEV_NAME, __func__, ret);
			return IRQ_HANDLED;
		}

		queue_delayed_work(s2mpu10->irq_wqueue, &s2mpu10->irq_work, 0);

		/* Power-key W/A */
		ret = s2mpu10_power_key_detection(s2mpu10);
		if (ret)
			pr_err("%s: POWER-KEY detection error\n", __func__);

		/* Report IRQ */
		s2mpu10_report_irq(s2mpu10);
	}
	/* notify Slave PMIC */
	if (ibi_src[0] & S2MPU10_IBI0_PMIC_S) {
		pr_info("%s: IBI from slave pmic\n", __func__);
		s2mpu11_call_notifier();
	}
	/* notify Codec */
	if (ibi_src[0] & S2MPU10_IBI0_CODEC) {
		pr_info("%s: IBI from codec\n", __func__);
		/* TODO : IRQ from s2mpu11 codec */
#if defined(CONFIG_SND_SOC_AUD3003X_5PIN) || defined(CONFIG_SND_SOC_AUD3003X_6PIN)
		if (codec_notifier_flag)
			aud3003x_call_notifier();
		else
			pr_err("%s: codec handler not registered!\n", __func__);
#endif
	}


	return IRQ_HANDLED;
}

static int s2mpu10_set_wqueue(struct s2mpu10_dev *s2mpu10)
{
	s2mpu10->irq_wqueue = create_singlethread_workqueue("s2mpu10-wqueue");
	if (!s2mpu10->irq_wqueue) {
		pr_err("%s: fail to create workqueue\n", __func__);
		return 1;
	}

	INIT_DELAYED_WORK(&s2mpu10->irq_work, s2mpu10_irq_work_func);

	return 0;
}

static void s2mpu10_set_vgpio_monitor(struct s2mpu10_dev *s2mpu10)
{
	s2mpu10->mem_base = ioremap(VGPIO_I3C_BASE + VGPIO_MONITOR_ADDR, SZ_32);
	if (s2mpu10->mem_base == NULL)
		pr_info("%s: fail to allocate memory\n", __func__);
}

int s2mpu10_irq_init(struct s2mpu10_dev *s2mpu10)
{
	int i;
	int ret;
	u8 i2c_data;
	int cur_irq;

	if (!s2mpu10->irq_base) {
		dev_err(s2mpu10->dev, "No interrupt base specified.\n");
		return 0;
	}

	mutex_init(&s2mpu10->irqlock);

	/* Set VGPIO monitor */
	s2mpu10_set_vgpio_monitor(s2mpu10);

	/* Set workqueue */
	s2mpu10_set_wqueue(s2mpu10);

	/* Mask individual interrupt sources */
	for (i = 0; i < S2MPU10_IRQ_GROUP_NR; i++) {
		struct i2c_client *i2c;

		s2mpu10->irq_masks_cur[i] = 0xff;
		s2mpu10->irq_masks_cache[i] = 0xff;

		i2c = get_i2c(s2mpu10, i);

		if (IS_ERR_OR_NULL(i2c))
			continue;
		if (s2mpu10_mask_reg[i] == S2MPU10_REG_INVALID)
			continue;

		s2mpu10_write_reg(i2c, s2mpu10_mask_reg[i], 0xff);
	}

	/* Register with genirq */
	for (i = 0; i < S2MPU10_IRQ_NR; i++) {
		cur_irq = i + s2mpu10->irq_base;
		irq_set_chip_data(cur_irq, s2mpu10);
		irq_set_chip_and_handler(cur_irq, &s2mpu10_irq_chip,
					 handle_level_irq);
		irq_set_nested_thread(cur_irq, 1);
#ifdef CONFIG_ARM
		set_irq_flags(cur_irq, IRQF_VALID);
#else
		irq_set_noprobe(cur_irq);
#endif
	}

	s2mpu10_write_reg(s2mpu10->i2c, S2MPU10_PMIC_REG_IRQM, 0xff);
	/* Unmask s2mpu10 interrupt */
	ret = s2mpu10_read_reg(s2mpu10->i2c, S2MPU10_PMIC_REG_IRQM,
			  &i2c_data);
	if (ret) {
		pr_err("%s:%s fail to read intsrc mask reg\n",
					 MFD_DEV_NAME, __func__);
		return ret;
	}

	i2c_data &= ~(S2MPU10_IRQSRC_PMIC);	/* Unmask pmic interrupt */
	s2mpu10_write_reg(s2mpu10->i2c, S2MPU10_PMIC_REG_IRQM,
			   i2c_data);

	pr_info("%s:%s s2mpu10_PMIC_REG_IRQM=0x%02x\n",
			MFD_DEV_NAME, __func__, i2c_data);

	/* register irq to gic */
	s2mpu10->irq = irq_of_parse_and_map(s2mpu10->dev->of_node, 0);
	ret = request_threaded_irq(s2mpu10->irq, NULL, s2mpu10_irq_thread,
				   IRQF_ONESHOT,
				   "s2mpu10-irq", s2mpu10);

	if (ret) {
		dev_err(s2mpu10->dev, "Failed to request IRQ %d: %d\n",
			s2mpu10->irq, ret);
		destroy_workqueue(s2mpu10->irq_wqueue);

		return ret;
	}

	return 0;
}

void s2mpu10_irq_exit(struct s2mpu10_dev *s2mpu10)
{
	if (s2mpu10->irq)
		free_irq(s2mpu10->irq, s2mpu10);

	iounmap(s2mpu10->mem_base);

	cancel_delayed_work_sync(&s2mpu10->irq_work);
	destroy_workqueue(s2mpu10->irq_wqueue);
}
