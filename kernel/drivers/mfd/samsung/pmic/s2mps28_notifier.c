/*
 * s2mps28_notifier.c - Interrupt controller support for S2MPS28
 *
 * Copyright (C) 2023 Samsung Electronics Co.Ltd
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
#include <linux/samsung/pmic/s2mps28.h>
#include <linux/samsung/pmic/s2mps28-regulator.h>
#include <linux/notifier.h>
#include <linux/irq.h>
#include <linux/samsung/pmic/pmic_class.h>

#if IS_ENABLED(CONFIG_SEC_DEBUG_EXTRA_INFO)
#include <linux/sec_debug.h>
#endif

#define BUCK_SR1S_IDX (5)

static struct notifier_block sub_pmic_notifier[TYPE_S2MPS28_MAX];
static struct s2mps28_dev *s2mps28_global[TYPE_S2MPS28_MAX];
static uint8_t irq_reg[TYPE_S2MPS28_MAX][S2MPS28_IRQ_GROUP_NR] = {0};
static int s2mps28_buck_oi_cnt[TYPE_S2MPS28_MAX][S2MPS28_BUCK_MAX]; /* buck 1S ~ 5S, SR1 */
static int s2mps28_buck_ocp_cnt[TYPE_S2MPS28_MAX][S2MPS28_BUCK_MAX]; /* buck 1S ~ 5S, SR1 */
static int s2mps28_temp_cnt[TYPE_S2MPS28_MAX][S2MPS28_TEMP_MAX]; /* Temp */

static const uint8_t s2mps28_get_irq_mask[] = {
	/* OI */
	[S2MPS28_IRQ_OI_B1S] = S2MPS28_PMIC_IRQ_OI_B1_MASK,
	[S2MPS28_IRQ_OI_B2S] = S2MPS28_PMIC_IRQ_OI_B2_MASK,
	[S2MPS28_IRQ_OI_B3S] = S2MPS28_PMIC_IRQ_OI_B3_MASK,
	[S2MPS28_IRQ_OI_B4S] = S2MPS28_PMIC_IRQ_OI_B4_MASK,
	[S2MPS28_IRQ_OI_B5S] = S2MPS28_PMIC_IRQ_OI_B5_MASK,
	[S2MPS28_IRQ_OI_SR1S] = S2MPS28_PMIC_IRQ_OI_SR1_MASK,

	/* OCP */
	[S2MPS28_IRQ_OCP_B1S] = S2MPS28_PMIC_IRQ_OCP_B1_MASK,
	[S2MPS28_IRQ_OCP_B2S] = S2MPS28_PMIC_IRQ_OCP_B2_MASK,
	[S2MPS28_IRQ_OCP_B3S] = S2MPS28_PMIC_IRQ_OCP_B3_MASK,
	[S2MPS28_IRQ_OCP_B4S] = S2MPS28_PMIC_IRQ_OCP_B4_MASK,
	[S2MPS28_IRQ_OCP_B5S] = S2MPS28_PMIC_IRQ_OCP_B5_MASK,
	[S2MPS28_IRQ_OCP_SR1S] = S2MPS28_PMIC_IRQ_OCP_SR1_MASK,

	/* Temp */
	[S2MPS28_IRQ_INT120C] = S2MPS28_IRQ_INT120C_MASK,
	[S2MPS28_IRQ_INT140C] = S2MPS28_IRQ_INT140C_MASK,
};

static const uint8_t s2mps28_mask_reg[] = {
	[S2MPS28_PMIC_INT1] = S2MPS28_PM1_INT1M,
	[S2MPS28_PMIC_INT2] = S2MPS28_PM1_INT2M,
	[S2MPS28_PMIC_INT3] = S2MPS28_PM1_INT3M,
	[S2MPS28_PMIC_INT4] = S2MPS28_PM1_INT4M,
};

/* add IRQ handling func here */
static void s2mps28_temp_irq(struct s2mps28_dev *s2mps28, int degree)
{
	int temp = 0, dev_type = s2mps28->device_type;

	s2mps28_temp_cnt[s2mps28->device_type][degree]++;
	if (degree == 0)
		temp = 120;
	else
		temp = 140;

	pr_info("%s: SUB%d thermal %dC IRQ: %d\n",
		__func__, dev_type + 1, temp, s2mps28_temp_cnt[dev_type][degree]);
}

static void s2mps28_buck_ocp_irq(struct s2mps28_dev *s2mps28, int buck)
{
	int dev_type = s2mps28->device_type;

	s2mps28_buck_ocp_cnt[dev_type][buck]++;
	if (buck == BUCK_SR1S_IDX)
		pr_info("%s: SUB%d BUCK_SR[%d] OCP IRQ: %d\n",
			__func__, dev_type + 1, buck + 1 - BUCK_SR1S_IDX,
			s2mps28_buck_ocp_cnt[dev_type][buck]);
	else
		pr_info("%s: SUB%d BUCK[%d] OCP IRQ: %d\n",
			__func__, dev_type + 1,buck + 1,
			s2mps28_buck_ocp_cnt[dev_type][buck]);

#if IS_ENABLED(CONFIG_SEC_DEBUG_EXTRA_INFO)
	secdbg_exin_set_sub_ocp_count(OCP_OI_COUNTER_TYPE_OCP);
#endif
}

static void s2mps28_buck_oi_irq(struct s2mps28_dev *s2mps28, int buck)
{
	int dev_type = s2mps28->device_type;

	s2mps28_buck_oi_cnt[dev_type][buck]++;
	if (buck == BUCK_SR1S_IDX)
		pr_info("%s: SUB%d BUCK_SR[%d] OI IRQ: %d\n",
			__func__, dev_type + 1, buck + 1 - BUCK_SR1S_IDX,
			s2mps28_buck_oi_cnt[dev_type][buck]);
	else
		pr_info("%s: SUB%d BUCK[%d] OI IRQ: %d\n",
			__func__, dev_type + 1, buck + 1,
			s2mps28_buck_oi_cnt[dev_type][buck]);

#if IS_ENABLED(CONFIG_SEC_DEBUG_EXTRA_INFO)
	secdbg_exin_set_sub_ocp_count(OCP_OI_COUNTER_TYPE_OI);
#endif
}

static void s2mps28_call_interrupt(struct s2mps28_dev *s2mps28)
{
	size_t i;
	uint8_t reg = 0, dev_type = s2mps28->device_type;

	/* OI interrupt */
	for (i = 0; i < S2MPS28_BUCK_MAX; i++) {
		reg = S2MPS28_IRQ_OI_B1S + i;
		if (irq_reg[dev_type][S2MPS28_PMIC_INT3] & s2mps28_get_irq_mask[reg])
			s2mps28_buck_oi_irq(s2mps28, i);
	}

	/* OCP interrupt */
	for (i = 0; i < S2MPS28_BUCK_MAX; i++) {
		reg = S2MPS28_IRQ_OCP_B1S + i;
		if (irq_reg[dev_type][S2MPS28_PMIC_INT2] & s2mps28_get_irq_mask[reg])
			s2mps28_buck_ocp_irq(s2mps28, i);
	}

	/* Thermal interrupt */
	for (i = 0; i < S2MPS28_TEMP_MAX; i++) {
		reg = S2MPS28_IRQ_INT120C + i;
		if (irq_reg[dev_type][S2MPS28_PMIC_INT1] & s2mps28_get_irq_mask[reg])
			s2mps28_temp_irq(s2mps28, i);
	}
}

static void s2mps28_irq_work_func(struct work_struct *work)
{
	char buf[1024] = {0, };
	int i, cnt = 0, irq_cnt = 0, dev_type;
	struct delayed_work *dw = container_of(work, struct delayed_work, work);
	struct s2mps28_dev *s2mps28 = container_of(dw, struct s2mps28_dev, irq_work);

	dev_type = s2mps28->device_type;
	irq_cnt = S2MPS28_NUM_IRQ_PMIC_REGS;

	for (i = 0; i < irq_cnt; i++)
		cnt += snprintf(buf + cnt, sizeof(buf) - 1, "INT%d(0x%02hhx) ", i + 1, irq_reg[dev_type][i]);

	pr_info("[SUB%d_PMIC] %s: pmic interrupt %s\n", dev_type + 1, __func__, buf);
}

static int s2mps28_notifier_handler(struct notifier_block *nb,
				    unsigned long action,
				    void *data)
{
	int ret, irq_cnt;
	struct s2mps28_dev *s2mps28 = data;

	if (!s2mps28) {
		pr_err("%s: s2mps28_dev fail to load dev.\n", __func__);
		return IRQ_HANDLED;
	}

	mutex_lock(&s2mps28->irqlock);

	/* Read interrupt */
	irq_cnt = S2MPS28_NUM_IRQ_PMIC_REGS;

	ret = s2mps28_bulk_read(s2mps28->pm1, S2MPS28_PM1_INT1,
				irq_cnt, &irq_reg[s2mps28->device_type][S2MPS28_PMIC_INT1]);
	if (ret) {
		pr_err("%s: SUB%d fail to read INT sources\n", __func__, s2mps28->device_type + 1);
		mutex_unlock(&s2mps28->irqlock);

		return IRQ_HANDLED;
	}

	queue_delayed_work(s2mps28->irq_wqueue, &s2mps28->irq_work, 0);

	/* Call interrupt */
	s2mps28_call_interrupt(s2mps28);

	mutex_unlock(&s2mps28->irqlock);

	return IRQ_HANDLED;
}

static BLOCKING_NOTIFIER_HEAD(s2mps28_1_notifier);
static BLOCKING_NOTIFIER_HEAD(s2mps28_2_notifier);
static BLOCKING_NOTIFIER_HEAD(s2mps28_3_notifier);
static BLOCKING_NOTIFIER_HEAD(s2mps28_4_notifier);
static BLOCKING_NOTIFIER_HEAD(s2mps28_5_notifier);

static int s2mps28_register_notifier(struct notifier_block *nb, struct s2mps28_dev *s2mps28)
{
	int ret = 0, dev_type = s2mps28->device_type;

	switch (dev_type) {
	case TYPE_S2MPS28_1:
		ret = blocking_notifier_chain_register(&s2mps28_1_notifier, nb);
		break;
	case TYPE_S2MPS28_2:
		ret = blocking_notifier_chain_register(&s2mps28_2_notifier, nb);
		break;
	case TYPE_S2MPS28_3:
		ret = blocking_notifier_chain_register(&s2mps28_3_notifier, nb);
		break;
	case TYPE_S2MPS28_4:
		ret = blocking_notifier_chain_register(&s2mps28_4_notifier, nb);
		break;
	case TYPE_S2MPS28_5:
		ret = blocking_notifier_chain_register(&s2mps28_5_notifier, nb);
		break;
	default:
		pr_err("%s: Not find dev_type\n", __func__);
		return -EEXIST;
	}

	if (ret < 0)
		pr_err("%s: SUB%d fail to register notifier\n", __func__, dev_type + 1);

	return ret;
}

void s2mps28_1_call_notifier(void)
{
	blocking_notifier_call_chain(&s2mps28_1_notifier, 0, s2mps28_global[TYPE_S2MPS28_1]);
}
EXPORT_SYMBOL_GPL(s2mps28_1_call_notifier);

void s2mps28_2_call_notifier(void)
{
	blocking_notifier_call_chain(&s2mps28_2_notifier, 0, s2mps28_global[TYPE_S2MPS28_2]);
}
EXPORT_SYMBOL_GPL(s2mps28_2_call_notifier);

void s2mps28_3_call_notifier(void)
{
	blocking_notifier_call_chain(&s2mps28_3_notifier, 0, s2mps28_global[TYPE_S2MPS28_3]);
}
EXPORT_SYMBOL_GPL(s2mps28_3_call_notifier);

void s2mps28_4_call_notifier(void)
{
	blocking_notifier_call_chain(&s2mps28_4_notifier, 0, s2mps28_global[TYPE_S2MPS28_4]);
}
EXPORT_SYMBOL_GPL(s2mps28_4_call_notifier);

void s2mps28_5_call_notifier(void)
{
	blocking_notifier_call_chain(&s2mps28_5_notifier, 0, s2mps28_global[TYPE_S2MPS28_5]);
}
EXPORT_SYMBOL_GPL(s2mps28_5_call_notifier);

static int s2mps28_unmask_interrupt(struct s2mps28_dev *s2mps28)
{
	int ret;

	/* unmask OI interrupt */
	ret = s2mps28_write_reg(s2mps28->pm1, S2MPS28_PM1_INT3M, 0x00);
	if (ret)
		goto err;

	/* unmask OCP interrupt */
	ret = s2mps28_write_reg(s2mps28->pm1, S2MPS28_PM1_INT2M, 0x00);
	if (ret)
		goto err;

	/* unmask Temp interrupt */
	ret = s2mps28_update_reg(s2mps28->pm1, S2MPS28_PM1_INT1M, 0x00, 0x0C);
	if (ret)
		goto err;

	return 0;
err:
	return -1;
}

static int s2mps28_set_interrupt(struct s2mps28_dev *s2mps28)
{
	uint8_t i, val;
	int ret, dev_type = s2mps28->device_type;

	/* Mask all the interrupt sources */
	for (i = 0; i < S2MPS28_NUM_IRQ_PMIC_REGS; i++) {
		ret = s2mps28_write_reg(s2mps28->pm1, s2mps28_mask_reg[i], 0xFF);
		if (ret)
			goto err;
	}

	/* Unmask interrupt sources */
	ret = s2mps28_unmask_interrupt(s2mps28);
	if (ret < 0) {
		pr_err("[SUB%d_PMIC] %s: s2mps28_unmask_interrupt fail\n", dev_type + 1, __func__);
		goto err;
	}

	/* Check unmask interrupt register */
	for (i = 0; i < S2MPS28_NUM_IRQ_PMIC_REGS; i++) {
		ret = s2mps28_read_reg(s2mps28->pm1, s2mps28_mask_reg[i], &val);
		if (ret)
			goto err;
		pr_info("[SUB%d_PMIC] %s: INT%dM = 0x%02hhx\n", dev_type + 1, __func__, i + 1, val);
	}

	return 0;
err:
	return -1;
}

static int s2mps28_set_wqueue(struct s2mps28_dev *s2mps28)
{
	static char device_name[32] = {0, };
	int dev_type = s2mps28->device_type;

	/* Dynamic allocation for device name */
	snprintf(device_name, sizeof(device_name) - 1, "%s-%d-wqueue@%s",
		 dev_driver_string(s2mps28->dev), dev_type + 1, dev_name(s2mps28->dev));

	s2mps28->irq_wqueue = create_singlethread_workqueue(device_name);
	if (!s2mps28->irq_wqueue) {
		pr_err("%s: sub%d fail to create workqueue\n", __func__, dev_type + 1);
		return -1;
	}

	INIT_DELAYED_WORK(&s2mps28->irq_work, s2mps28_irq_work_func);

	return 0;
}

static void s2mps28_set_notifier(struct s2mps28_dev *s2mps28)
{
	int dev_type = s2mps28->device_type;

	sub_pmic_notifier[dev_type].notifier_call = s2mps28_notifier_handler;
	s2mps28_register_notifier(&sub_pmic_notifier[dev_type], s2mps28);
}

#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
static ssize_t irq_read_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	int i, cnt = 0, dev_type;
	struct s2mps28_dev *s2mps28 = dev_get_drvdata(dev);

	dev_type = s2mps28->device_type;
	cnt += snprintf(buf + cnt, PAGE_SIZE, "------ INTERRUPTS (/pmic/%s-%d) ------\n",
			dev_driver_string(s2mps28->dev), dev_type + 1);

	for (i = 0; i < S2MPS28_BUCK_MAX; i++) {
		if (i == BUCK_SR1S_IDX)
			cnt += snprintf(buf + cnt, PAGE_SIZE, "BUCK_SR%d_OCP_IRQ:\t%5d\n",
					i + 1 - BUCK_SR1S_IDX, s2mps28_buck_ocp_cnt[dev_type][i]);
		else
			cnt += snprintf(buf + cnt, PAGE_SIZE, "BUCK%d_OCP_IRQ:\t\t%5d\n",
					i + 1, s2mps28_buck_ocp_cnt[dev_type][i]);
	}

	for (i = 0; i < S2MPS28_BUCK_MAX; i++) {
		if (i == BUCK_SR1S_IDX)
			cnt += snprintf(buf + cnt, PAGE_SIZE, "BUCK_SR%d_OI_IRQ:\t%5d\n",
					i + 1 - BUCK_SR1S_IDX, s2mps28_buck_oi_cnt[dev_type][i]);
		else
			cnt += snprintf(buf + cnt, PAGE_SIZE, "BUCK%d_OI_IRQ:\t\t%5d\n",
					i + 1, s2mps28_buck_oi_cnt[dev_type][i]);

	}

	for (i = 0; i < S2MPS28_TEMP_MAX; i++)
		cnt += snprintf(buf + cnt, PAGE_SIZE, "TEMP_%d_IRQ:\t\t%5d\n",
				i ? 140 : 120, s2mps28_temp_cnt[dev_type][i]);

	return cnt;
}

static struct pmic_device_attribute irq_attr[] = {
	PMIC_ATTR(irq_read_all, S_IRUGO, irq_read_show, NULL),
};

static int s2mps28_create_irq_sysfs(struct s2mps28_dev *s2mps28)
{
	struct device *s2mps28_irq_dev;
	struct device *dev = s2mps28->dev;
	char device_name[32] = {0, };
	int err = -ENODEV, i = 0, dev_type = s2mps28->device_type;

	pr_info("[SUB%d_PMIC] %s()\n", dev_type + 1, __func__);

	/* Dynamic allocation for device name */
	snprintf(device_name, sizeof(device_name) - 1, "%s-%d-irq@%s",
		 dev_driver_string(dev), s2mps28->device_type + 1, dev_name(dev));

	s2mps28_irq_dev = pmic_device_create(s2mps28, device_name);
	s2mps28->irq_dev = s2mps28_irq_dev;

	/* Create sysfs entries */
	for (i = 0; i < ARRAY_SIZE(irq_attr); i++) {
		err = device_create_file(s2mps28_irq_dev, &irq_attr[i].dev_attr);
		if (err)
			goto remove_pmic_device;
	}

	return 0;

remove_pmic_device:
	for (i--; i >= 0; i--)
		device_remove_file(s2mps28_irq_dev, &irq_attr[i].dev_attr);
	pmic_device_destroy(s2mps28_irq_dev->devt);

	return -ENODEV;
}
#endif

int s2mps28_notifier_init(struct s2mps28_dev *s2mps28)
{
	int dev_type = s2mps28->device_type;

	s2mps28_global[dev_type] = s2mps28;
	mutex_init(&s2mps28->irqlock);
	s2mps28_set_notifier(s2mps28);

	if (s2mps28_set_wqueue(s2mps28) < 0) {
		pr_err("[SUB%d_PMIC] %s: s2mps28_set_wqueue fail\n", dev_type + 1, __func__);
		goto err;
	}

	if (s2mps28_set_interrupt(s2mps28) < 0) {
		pr_err("[SUB%d_PMIC] %s: s2mps28_set_interrupt fail\n", dev_type + 1, __func__);
		goto err;
	}

#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	if (s2mps28_create_irq_sysfs(s2mps28) < 0) {
		pr_err("[SUB%d_PMIC] %s: failed to create sysfs\n", dev_type + 1, __func__);
		goto err;
	}
#endif
	return 0;
err:
	return -1;
}
EXPORT_SYMBOL_GPL(s2mps28_notifier_init);

void s2mps28_notifier_deinit(struct s2mps28_dev *s2mps28)
{
#if IS_ENABLED(CONFIG_DRV_SAMSUNG_PMIC)
	struct device *s2mps28_irq_dev = s2mps28->irq_dev;
	int i = 0;

	/* Remove sysfs entries */
	for (i = 0; i < ARRAY_SIZE(irq_attr); i++)
		device_remove_file(s2mps28_irq_dev, &irq_attr[i].dev_attr);
	pmic_device_destroy(s2mps28_irq_dev->devt);
#endif
}
EXPORT_SYMBOL_GPL(s2mps28_notifier_deinit);
