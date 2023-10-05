/*
 * s2mpu16-notifier.c - Interrupt controller support for s2mpu16
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
 */

#include <linux/err.h>
#include <linux/samsung/pmic/s2mpu16.h>
#include <linux/samsung/pmic/s2mpu16-regulator-8835.h>
#include <linux/notifier.h>
#include <linux/irq.h>
#include <linux/samsung/pmic/pmic_class.h>

#define BUCK_SR1S_IDX (10)

static struct notifier_block sub_pmic_notifier;
static struct s2mpu16_dev *s2mpu16_global;
static uint8_t irq_reg[S2MPU16_IRQ_GROUP_NR] = {0};
static int s2mpu16_buck_oi_cnt[S2MPU16_BUCK_MAX];
static int s2mpu16_buck_ocp_cnt[S2MPU16_BUCK_MAX];
static int s2mpu16_temp_cnt[S2MPU16_TEMP_MAX];

/* add IRQ handling func here */
static void s2mpu16_temp_irq(struct s2mpu16_dev *s2mpu16, size_t degree)
{
	int temp = 0;

	s2mpu16_temp_cnt[degree]++;

	if (degree == 0)
		temp = 120;
	else
		temp = 140;

	pr_info("%s: s2mpu16 thermal %dC IRQ: %d\n",
		__func__, temp, s2mpu16_temp_cnt[degree]);
}

static void s2mpu16_buck_ocp_irq(struct s2mpu16_dev *s2mpu16, size_t buck)
{
	s2mpu16_buck_ocp_cnt[buck]++;
	if (buck < BUCK_SR1S_IDX) {
		pr_info("%s: S2MPU16 BUCK[%d] OCP IRQ: %d\n",
			__func__, buck + 1, s2mpu16_buck_ocp_cnt[buck]);
	} else {
		pr_info("%s: S2MPU16 BUCK_SR[%d] OCP IRQ: %d\n",
			__func__, buck + 1 - BUCK_SR1S_IDX, s2mpu16_buck_ocp_cnt[buck]);
	}
}

static void s2mpu16_buck_oi_irq(struct s2mpu16_dev *s2mpu16, size_t buck)
{
	s2mpu16_buck_oi_cnt[buck]++;
	if (buck < BUCK_SR1S_IDX) {
		pr_info("%s: S2MPU16 BUCK[%d] OI IRQ: %d\n",
			__func__, buck + 1, s2mpu16_buck_oi_cnt[buck]);
	} else {
		pr_info("%s: S2MPU16 BUCK_SR[%d] OI IRQ: %d\n",
			__func__, buck + 1 - BUCK_SR1S_IDX, s2mpu16_buck_oi_cnt[buck]);
	}
}

static const uint8_t s2mpu16_get_irq_mask[] = {
	/* OI */
	[S2MPU16_IRQ_OI_B1S] = S2MPU16_PMIC_IRQ_OI_B1_MASK,
	[S2MPU16_IRQ_OI_B2S] = S2MPU16_PMIC_IRQ_OI_B2_MASK,
	[S2MPU16_IRQ_OI_B3S] = S2MPU16_PMIC_IRQ_OI_B3_MASK,
	[S2MPU16_IRQ_OI_B4S] = S2MPU16_PMIC_IRQ_OI_B4_MASK,
	[S2MPU16_IRQ_OI_B5S] = S2MPU16_PMIC_IRQ_OI_B5_MASK,
	[S2MPU16_IRQ_OI_B6S] = S2MPU16_PMIC_IRQ_OI_B6_MASK,
	[S2MPU16_IRQ_OI_B7S] = S2MPU16_PMIC_IRQ_OI_B7_MASK,
	[S2MPU16_IRQ_OI_B8S] = S2MPU16_PMIC_IRQ_OI_B8_MASK,
	[S2MPU16_IRQ_OI_B9S] = S2MPU16_PMIC_IRQ_OI_B9_MASK,
	[S2MPU16_IRQ_OI_B10S] = S2MPU16_PMIC_IRQ_OI_B10_MASK,
	[S2MPU16_IRQ_OI_SR1S] = S2MPU16_PMIC_IRQ_OI_SR1S_MASK,
	[S2MPU16_IRQ_OI_SR2S] = S2MPU16_PMIC_IRQ_OI_SR2S_MASK,
	/* OCP */
	[S2MPU16_IRQ_OCP_B1S] = S2MPU16_PMIC_IRQ_OCP_B1_MASK,
	[S2MPU16_IRQ_OCP_B2S] = S2MPU16_PMIC_IRQ_OCP_B2_MASK,
	[S2MPU16_IRQ_OCP_B3S] = S2MPU16_PMIC_IRQ_OCP_B3_MASK,
	[S2MPU16_IRQ_OCP_B4S] = S2MPU16_PMIC_IRQ_OCP_B4_MASK,
	[S2MPU16_IRQ_OCP_B5S] = S2MPU16_PMIC_IRQ_OCP_B5_MASK,
	[S2MPU16_IRQ_OCP_B6S] = S2MPU16_PMIC_IRQ_OCP_B6_MASK,
	[S2MPU16_IRQ_OCP_B7S] = S2MPU16_PMIC_IRQ_OCP_B7_MASK,
	[S2MPU16_IRQ_OCP_B8S] = S2MPU16_PMIC_IRQ_OCP_B8_MASK,
	[S2MPU16_IRQ_OCP_B9S] = S2MPU16_PMIC_IRQ_OCP_B9_MASK,
	[S2MPU16_IRQ_OCP_B10S] = S2MPU16_PMIC_IRQ_OCP_B10_MASK,
	[S2MPU16_IRQ_OCP_SR1S] = S2MPU16_PMIC_IRQ_OCP_SR1S_MASK,
	[S2MPU16_IRQ_OCP_SR2S] = S2MPU16_PMIC_IRQ_OCP_SR2S_MASK,
	/* Temp */
	[S2MPU16_IRQ_INT120C] = S2MPU16_IRQ_INT120C_MASK,
	[S2MPU16_IRQ_INT140C] = S2MPU16_IRQ_INT140C_MASK,
};

static void s2mpu16_call_interrupt(struct s2mpu16_dev *s2mpu16)
{
	size_t i;
	uint8_t reg = 0;

	/* OI interrupt */
	for (i = 0; i < S2MPU16_BUCK_MAX; i++) {
		reg = S2MPU16_IRQ_OI_B1S + i;
		if (reg < S2MPU16_IRQ_OI_B9S) {
			if (irq_reg[S2MPU16_PMIC_INT4] & s2mpu16_get_irq_mask[reg])
				s2mpu16_buck_oi_irq(s2mpu16, i);
		} else {
			if (irq_reg[S2MPU16_PMIC_INT5] & s2mpu16_get_irq_mask[reg])
				s2mpu16_buck_oi_irq(s2mpu16, i);
		}
	}

	/* OCP interrupt */
	for (i = 0; i < S2MPU16_BUCK_MAX; i++) {
		reg = S2MPU16_IRQ_OCP_B1S + i;
		if (reg < S2MPU16_IRQ_OCP_B9S) {
			if (irq_reg[S2MPU16_PMIC_INT2] & s2mpu16_get_irq_mask[reg])
				s2mpu16_buck_ocp_irq(s2mpu16, i);
		} else {
			if (irq_reg[S2MPU16_PMIC_INT3] & s2mpu16_get_irq_mask[reg])
				s2mpu16_buck_ocp_irq(s2mpu16, i);
		}
	}

	/* Thermal interrupt */
	for (i = 0; i < S2MPU16_TEMP_MAX; i++) {
		reg = S2MPU16_IRQ_INT120C + i;
		if (irq_reg[S2MPU16_PMIC_INT1] & s2mpu16_get_irq_mask[reg])
			s2mpu16_temp_irq(s2mpu16, i);
	}
}

static void s2mpu16_irq_work_func(struct work_struct *work)
{
	char buf[1024] = {0, };
	uint32_t i, cnt = 0, irq_cnt = 0;

	irq_cnt = S2MPU16_NUM_IRQ_PMIC_REGS;

	for (i = 0; i < irq_cnt; i++)
		cnt += snprintf(buf + cnt, sizeof(buf) - 1, "INT%d(0x%02hhx) ", i + 1, irq_reg[i]);

	pr_info("[PMIC] %s: sub(s2mpu16) pmic interrupt %s\n", __func__, buf);
}

static int s2mpu16_notifier_handler(struct notifier_block *nb,
				    unsigned long action,
				    void *data)
{
	int ret, irq_cnt;
	struct s2mpu16_dev *s2mpu16 = data;

	if (!s2mpu16) {
		pr_err("%s: fail to load dev.\n", __func__);
		return IRQ_HANDLED;
	}

	mutex_lock(&s2mpu16->irqlock);

	/* Read interrupt */
	irq_cnt = S2MPU16_NUM_IRQ_PMIC_REGS;

	ret = s2mpu16_bulk_read(s2mpu16->pmic1, S2MPU16_PM1_INT1,
				irq_cnt, &irq_reg[S2MPU16_PMIC_INT1]);
	if (ret) {
		pr_err("%s: fail to read INT sources\n", __func__);
		mutex_unlock(&s2mpu16->irqlock);

		return IRQ_HANDLED;
	}

	queue_delayed_work(s2mpu16->irq_wqueue, &s2mpu16->irq_work, 0);

	/* Call interrupt */
	s2mpu16_call_interrupt(s2mpu16);

	mutex_unlock(&s2mpu16->irqlock);

	return IRQ_HANDLED;
}

static BLOCKING_NOTIFIER_HEAD(s2mpu16_notifier);

static int s2mpu16_register_notifier(struct notifier_block *nb,
			      struct s2mpu16_dev *s2mpu16)
{
	int ret;

	ret = blocking_notifier_chain_register(&s2mpu16_notifier, nb);
	if (ret < 0)
		pr_err("%s: fail to register notifier\n", __func__);

	return ret;
}

void s2mpu16_call_notifier(void)
{
	blocking_notifier_call_chain(&s2mpu16_notifier, 0, s2mpu16_global);
}
EXPORT_SYMBOL(s2mpu16_call_notifier);

static const uint8_t s2mpu16_mask_reg[] = {
	[S2MPU16_PMIC_INT1] = S2MPU16_PM1_INT1M,
	[S2MPU16_PMIC_INT2] = S2MPU16_PM1_INT3M,
	[S2MPU16_PMIC_INT3] = S2MPU16_PM1_INT3M,
	[S2MPU16_PMIC_INT4] = S2MPU16_PM1_INT4M,
	[S2MPU16_PMIC_INT5] = S2MPU16_PM1_INT5M,
	[S2MPU16_PMIC_INT6] = S2MPU16_PM1_INT6M,
	[S2MPU16_PMIC_INT7] = S2MPU16_PM1_INT7M,
};

static int s2mpu16_unmask_interrupt(struct s2mpu16_dev *s2mpu16)
{
	int ret;
	/* unmask IRQM interrupt */
	ret = s2mpu16_update_reg(s2mpu16->i2c, S2MPU16_COMMON_TX_MASK,
				 0x00, S2MPU16_PM_IRQM_MASK);
	if (ret)
		goto err;

	/* unmask OI interrupt */
	ret = s2mpu16_write_reg(s2mpu16->pmic1, S2MPU16_PM1_INT4M, 0x00);
	if (ret)
		goto err;
	ret = s2mpu16_update_reg(s2mpu16->pmic1, S2MPU16_PM1_INT5M, 0x00, 0x33);
	if (ret)
		goto err;

	/* unmask OCP interrupt */
	ret = s2mpu16_write_reg(s2mpu16->pmic1, S2MPU16_PM1_INT2M, 0x00);
	if (ret)
		goto err;
	ret = s2mpu16_update_reg(s2mpu16->pmic1, S2MPU16_PM1_INT3M, 0x00, 0x33);
	if (ret)
		goto err;

	/* unmask Temp interrupt */
	ret = s2mpu16_update_reg(s2mpu16->pmic1, S2MPU16_PM1_INT1M, 0x00, 0x0C);
	if (ret)
		goto err;

	return 0;
err:
	return -1;
}

static int s2mpu16_set_interrupt(struct s2mpu16_dev *s2mpu16)
{
	uint8_t i, val;
	int ret;

	/* Mask all the interrupt sources */
	for (i = 0; i < S2MPU16_NUM_IRQ_PMIC_REGS; i++) {
		ret = s2mpu16_write_reg(s2mpu16->pmic1, s2mpu16_mask_reg[i], 0xFF);
		if (ret)
			goto err;
	}

	//ret = s2mpu16_update_reg(s2mpu16->i2c, S2MPU16_COMMON_TX_MASK,
	//			 S2MPU16_PM_IRQM_MASK, S2MPU16_PM_IRQM_MASK);
	//if (ret)
	//	goto err;

	/* Unmask interrupt sources */
	ret = s2mpu16_unmask_interrupt(s2mpu16);
	if (ret < 0) {
		pr_err("%s: s2mpu16_unmask_interrupt fail\n", __func__);
		goto err;
	}

	/* Check unmask interrupt register */
	for (i = 0; i < S2MPU16_NUM_IRQ_PMIC_REGS; i++) {
		ret = s2mpu16_read_reg(s2mpu16->pmic1, s2mpu16_mask_reg[i], &val);
		if (ret)
			goto err;
		pr_info("%s: INT%dM = 0x%02hhx\n", __func__, i + 1, val);
	}

	return 0;
err:
	return -1;
}

static int s2mpu16_set_wqueue(struct s2mpu16_dev *s2mpu16)
{
	static char device_name[32] = {0, };

	/* Dynamic allocation for device name */
	snprintf(device_name, sizeof(device_name) - 1, "%s-wqueue@%s",
		 dev_driver_string(s2mpu16->dev), dev_name(s2mpu16->dev));

	s2mpu16->irq_wqueue = create_singlethread_workqueue(device_name);
	if (!s2mpu16->irq_wqueue) {
		pr_err("%s: fail to create workqueue\n", __func__);
		return -1;
	}

	INIT_DELAYED_WORK(&s2mpu16->irq_work, s2mpu16_irq_work_func);

	return 0;
}

static void s2mpu16_set_notifier(struct s2mpu16_dev *s2mpu16)
{
	sub_pmic_notifier.notifier_call = s2mpu16_notifier_handler;
	s2mpu16_register_notifier(&sub_pmic_notifier, s2mpu16);
}

#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
static ssize_t irq_read_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	int i, cnt = 0;

	cnt += snprintf(buf + cnt, PAGE_SIZE, "------ INTERRUPTS (/pmic/%s) ------\n",
			dev_driver_string(s2mpu16_global->dev));

	for (i = 0; i < S2MPU16_BUCK_MAX; i++) {
		if (i < BUCK_SR1S_IDX) {
			cnt += snprintf(buf + cnt, PAGE_SIZE, "BUCK%d_OCP_IRQ:\t\t%5d\n",
					i + 1, s2mpu16_buck_ocp_cnt[i]);
		} else {
			cnt += snprintf(buf + cnt, PAGE_SIZE, "BUCK_SR%d_OCP_IRQ:\t\t%5d\n",
					i + 1 - BUCK_SR1S_IDX, s2mpu16_buck_ocp_cnt[i]);
		}
	}

	for (i = 0; i < S2MPU16_BUCK_MAX; i++) {
		if (i < BUCK_SR1S_IDX) {
			cnt += snprintf(buf + cnt, PAGE_SIZE, "BUCK%d_OI_IRQ:\t\t%5d\n",
					i + 1, s2mpu16_buck_oi_cnt[i]);
		} else {
			cnt += snprintf(buf + cnt, PAGE_SIZE, "BUCK_SR%d_OI_IRQ:\t\t%5d\n",
					i + 1 - BUCK_SR1S_IDX, s2mpu16_buck_oi_cnt[i]);
		}
	}

	for (i = 0; i < S2MPU16_TEMP_MAX; i++)
		cnt += snprintf(buf + cnt, PAGE_SIZE, "TEMP_%d_IRQ:\t\t%5d\n",
				i ? 140 : 120, s2mpu16_temp_cnt[i]);

	return cnt;
}

static struct pmic_device_attribute irq_attr[] = {
	PMIC_ATTR(irq_read_all, S_IRUGO, irq_read_show, NULL),
};

static int s2mpu16_create_irq_sysfs(struct s2mpu16_dev *s2mpu16)
{
	struct device *s2mpu16_irq_dev;
	struct device *dev = s2mpu16->dev;
	char device_name[32] = {0, };
	int err = -ENODEV, i = 0;

	pr_info("%s()\n", __func__);

	/* Dynamic allocation for device name */
	snprintf(device_name, sizeof(device_name) - 1, "%s-irq@%s",
		 dev_driver_string(dev), dev_name(dev));

	s2mpu16_irq_dev = pmic_device_create(s2mpu16, device_name);
	s2mpu16->irq_dev = s2mpu16_irq_dev;

	/* Create sysfs entries */
	for (i = 0; i < ARRAY_SIZE(irq_attr); i++) {
		err = device_create_file(s2mpu16_irq_dev, &irq_attr[i].dev_attr);
		if (err)
			goto remove_pmic_device;
	}

	return 0;

remove_pmic_device:
	for (i--; i >= 0; i--)
		device_remove_file(s2mpu16_irq_dev, &irq_attr[i].dev_attr);
	pmic_device_destroy(s2mpu16_irq_dev->devt);

	return -ENODEV;
}
#endif

int s2mpu16_notifier_init(struct s2mpu16_dev *s2mpu16)
{
	s2mpu16_global = s2mpu16;
	mutex_init(&s2mpu16->irqlock);

	/* Register notifier */
	s2mpu16_set_notifier(s2mpu16);

	/* Set workqueue */
	if (s2mpu16_set_wqueue(s2mpu16) < 0) {
		pr_err("%s: s2mpu16_set_wqueue fail\n", __func__);
		goto err;
	}

	/* Set interrupt */
	if (s2mpu16_set_interrupt(s2mpu16) < 0) {
		pr_err("%s: s2mpu16_set_interrupt fail\n", __func__);
		goto err;
	}

#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	if (s2mpu16_create_irq_sysfs(s2mpu16) < 0) {
		pr_info("%s: failed to create sysfs\n", __func__);
		goto err;
	}
#endif
	return 0;
err:
	return -1;
}
EXPORT_SYMBOL_GPL(s2mpu16_notifier_init);

void s2mpu16_notifier_deinit(struct s2mpu16_dev *s2mpu16)
{
#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	struct device *s2mpu16_irq_dev = s2mpu16->irq_dev;
	int i = 0;

	/* Remove sysfs entries */
	for (i = 0; i < ARRAY_SIZE(irq_attr); i++)
		device_remove_file(s2mpu16_irq_dev, &irq_attr[i].dev_attr);
	pmic_device_destroy(s2mpu16_irq_dev->devt);
#endif
}
EXPORT_SYMBOL(s2mpu16_notifier_deinit);
