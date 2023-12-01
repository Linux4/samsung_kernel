/*
 * PHYCAL interface driver for Samsung
 *
 * Copyright (C) 2022 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <asm/io.h>
#include <soc/samsung/exynos-phycal-if.h>

#define PHYCAL_DEBUG 0

int exynos_phycal_phy2virt(struct phycal_p2v_map *p2vmap, unsigned int map_size,
		struct phycal_seq *seq, unsigned int seq_size)
{
	int i, j;

	for (i = 0; i < seq_size; i++) {
		if (seq[i].access_type == PHYCAL_DELAY || seq[i].access_type == PHYCAL_EXT_DELAY)
			continue;

		if (!seq[i].base_pa) {
			pr_err("ERROR!!!! PA absent in seq element (idx: %d)\n", i);
			return i;
		}

		if (seq[i].base_pa) {
			for (j = 0; j < map_size; j++) {
				if (p2vmap[j].pa == (phys_addr_t)seq[i].base_pa)
					break;
			}
			if (j == map_size) {
				pr_err("ERROR!!! there is no such PA in p2v list (idx :%d\n", i);
				return i;
			} else {
				seq[i].base_va = p2vmap[j].va;
			}
		}

	}
	return 0;
}
EXPORT_SYMBOL(exynos_phycal_phy2virt);

static inline void exynos_phycal_wr(struct phycal_seq *seq)
{
	unsigned int val;

	if (seq->mask == 0xffffffff) {
		writel(seq->value, seq->base_va + seq->offset);
	} else {
		val = readl(seq->base_va + seq->offset) & ~(seq->mask);
		val |= seq->value & seq->mask;
		writel(val, seq->base_va + seq->offset);
	}

	if (PHYCAL_DEBUG)
		pr_info("[%s] WRITE, reg_type : %s, val = 0x%x , offset = 0x%x, mask = 0x%x, ~mask: 0x%x\n",
				__func__, seq->sfr_name, seq->value, seq->offset, seq->mask, ~(seq->mask));
	return;
}

/* read seq for printing the value of register */
static inline void exynos_phycal_rd(struct phycal_seq *seq)
{
	unsigned int val;

	if (seq->mask == 0xffffffff) {
		seq->value = readl(seq->base_va + seq->offset);
	} else {
		val = readl(seq->base_va + seq->offset);
		seq->value = (val & seq->mask);
	}

	pr_info("[%s] READ, reg_type : %s, offset = 0x%x, val = 0x%x, mask = 0x%x\n",
			__func__, seq->sfr_name, seq->offset, seq->value, seq->mask);
}

static inline void exynos_phycal_bitset(struct phycal_seq *seq)
{
	unsigned int val;

	if (seq->mask == 0xffffffff) {
		val = readl(seq->base_va + seq->offset) | seq->value;
		writel(val, seq->base_va + seq->offset);
	} else {
		val = readl(seq->base_va + seq->offset) & ~(seq->mask);
		val |= seq->value;
		writel(val, seq->base_va + seq->offset);
	}

	return;
}

static inline bool exynos_phycal_check_value(struct phycal_seq *seq)
{
	u32 val;

	val = readl(seq->base_va + seq->offset);
	val &= seq->mask;
	if (val == seq->value)
		return true;
	else
		return false;
}

static inline bool exynos_phycal_check_bitset(struct phycal_seq *seq, int is_mask)
{
	unsigned int val, i;
	unsigned int delay_num = seq->delay_num;
	unsigned int delay_val = seq->delay_val;

	if (is_mask == 0) {
		for (i = 0; i < delay_num; i++) {
			udelay(delay_val);
			val = readl(seq->base_va + seq->offset) & (seq->mask);
			if (val != 0)
				return true;
		}
	} else {
		for (i = 0; i < delay_num; i++) {
			udelay(delay_val);
			if (exynos_phycal_check_value(seq))
				return true;
		}
	}

	val = readl(seq->base_va + seq->offset) & (seq->mask);
	pr_err("[%s] TIMEOUT (%dusec)!! reg:%s(offset:0x%x, value:0x%x)\n",
			__func__, (delay_num * delay_val), seq->sfr_name, seq->offset, val);

	return false;
}

static inline void exynos_phycal_clear_bit(struct phycal_seq *seq)
{
	unsigned int val;

	val = readl(seq->base_va + seq->offset) & ~(seq->mask);
	writel(val, seq->base_va + seq->offset);

	return;

}

static inline void exynos_phycal_err(struct phycal_seq *seq)
{
	pr_err("## PHYCAL: ERROR: %s\n", seq->errlog);
}

static inline bool exynos_phycal_check_condition(struct phycal_seq *seq)
{
	u32 val;
	val = readl(seq->cond_base_va + seq->cond_offset);
	val &= seq->cond_mask;
	if (val == seq->cond_value)
		return true;
	else
		return false;
}

static inline int exynos_phycal_wait(struct phycal_seq *seq, int idx)
{
	u32 timeout = 0;
	u32 val;

	if (seq->cond_base_va && seq->cond_offset)
		if (!exynos_phycal_check_condition(seq))
			return 0;

	while (1) {
		if (exynos_phycal_check_value(seq))
			break;
		timeout++;
		udelay(1);
		if (timeout > 200000) {
			val = readl(seq->base_va + seq->offset);
			pr_err("[%s] TIMEOUT!! reg:%s(offset:0x%x, value:0x%x, idx:%d)\n",
					__func__, seq->sfr_name, seq->offset, val, idx);
			return -ETIMEDOUT;
		}
	}
	return 0;
}

void exynos_phycal_seq(struct phycal_seq *seq, u32 seq_size, u32 cond0)
{
	int i;

	for (i = 0; i < seq_size; i++) {
		if (seq[i].access_type >= 0x1000) {
			if (seq[i].cond0 & cond0) {
				switch (seq[i].access_type) {
				// ext operation
				case PHYCAL_EXT_READ:
					exynos_phycal_rd(&seq[i]);
					break;

				case PHYCAL_EXT_WRITE:
					exynos_phycal_wr(&seq[i]);
					break;

				case PHYCAL_EXT_SET_BIT:
					exynos_phycal_bitset(&seq[i]);
					break;

				case PHYCAL_EXT_CLR_BIT:
					exynos_phycal_clear_bit(&seq[i]);
					break;

				case PHYCAL_EXT_CHK_BIT:
					if (!exynos_phycal_check_bitset(&seq[i], 0))
					exynos_phycal_err(&seq[i]);
					break;
				case PHYCAL_EXT_CHK_BIT_MASK:
					if (!exynos_phycal_check_bitset(&seq[i], 1))
						exynos_phycal_err(&seq[i]);
					break;
				case PHYCAL_EXT_DELAY:
					udelay(seq[i].delay_val);
					if (PHYCAL_DEBUG)
						pr_err("[%s] delay %d\n", __func__,
							seq[i].delay_val);
					break;
				}
			} else {
				pr_debug("[%s] cond0 is not match(idx : %d, needed_cond: 0x%x seq_cond0 : 0x%x)\n",
						__func__, i, cond0, seq[i].cond0);
			}
		} else {
			switch (seq[i].access_type) {
			case PHYCAL_READ:
				exynos_phycal_rd(&seq[i]);
				break;
			case PHYCAL_WRITE:
				exynos_phycal_wr(&seq[i]);
				break;
			case PHYCAL_DELAY:
				udelay(seq[i].delay_val);
				if (PHYCAL_DEBUG)
					pr_err("[%s] delay %d\n", __func__, seq[i].delay_val);
				break;
			case PHYCAL_COND_READ:
				if (exynos_phycal_check_condition(&seq[i]))
					exynos_phycal_rd(&seq[i]);
				break;
			case PHYCAL_COND_WRITE:
				if (exynos_phycal_check_condition(&seq[i]))
					exynos_phycal_wr(&seq[i]);
				break;
			case PHYCAL_WAIT:
			case PHYCAL_WAIT_TWO:
				if (exynos_phycal_wait(&seq[i], i))
					return;
				break;
			case PHYCAL_CHECK_SKIP:
			case PHYCAL_COND_CHECK_SKIP:
			break;
			case PHYCAL_WRITE_WAIT:
				exynos_phycal_wr(&seq[i]);
				if (exynos_phycal_wait(&seq[i], i))
					return;
				break;
			case PHYCAL_WRITE_RETRY:
			case PHYCAL_WRITE_RETRY_INV:
				break;
			case PHYCAL_CLEAR_PEND:
				break;
			}
		}
	}
}
EXPORT_SYMBOL(exynos_phycal_seq);

MODULE_AUTHOR("Kyounghye Yun <k-hye.yun@samsung.com>");
MODULE_DESCRIPTION("Samsung PHYCAL interface driver");
MODULE_LICENSE("GPL v2");
