// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define pr_fmt(fmt) "[" KBUILD_MODNAME "] PMIO: " fmt

#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/rbtree.h>
#include <asm/unaligned.h>

#include "pablo-mmio.h"
#include "pmio.h"
#define CREATE_TRACE_POINTS
#include "trace_pmio.h"
#include "is-common-config.h"
#include "pablo-debug.h"

static int pmio_reg_read(void *ctx, unsigned int reg, unsigned int *val);
static int pmio_reg_write(void *ctx, unsigned int reg, unsigned int val);

bool pmio_reg_in_ranges(unsigned int reg,
			const struct pmio_range *ranges,
			unsigned int nranges)
{
	const struct pmio_range *r;
	int i;

	for (i = 0, r = ranges; i < nranges; i++, r++)
		if (pmio_reg_in_range(reg, r))
			return true;

	return false;
}
EXPORT_SYMBOL(pmio_reg_in_ranges);

bool pmio_check_range_table(struct pablo_mmio *pmio, unsigned int reg,
			    const struct pmio_access_table *table)
{
	if (pmio_reg_in_ranges(reg, table->no_ranges, table->n_no_ranges))
		return false;

	if (!table->n_yes_ranges)
		return true;

	return pmio_reg_in_ranges(reg, table->yes_ranges,
				  table->n_yes_ranges);
}
EXPORT_SYMBOL(pmio_check_range_table);

bool pmio_cached(struct pablo_mmio *pmio, unsigned int reg)
{
	int ret;
	unsigned int val;

	if (pmio->max_register && reg > pmio->max_register)
		return false;

	ret = pmio_cache_read(pmio, reg, &val);
	if (ret)
		return false;

	return true;
}

#if IS_ENABLED(PMIO_RANGE_CHECK_FULL)
bool pmio_writeable(struct pablo_mmio *pmio, unsigned int reg)
{
	reg &= ~pmio->corex_mask;

	if (pmio->max_register && reg > pmio->max_register)
		return false;

	if (pmio->wr_table)
		return pmio_check_range_table(pmio, reg, pmio->wr_table);

	return true;
}

bool pmio_readable(struct pablo_mmio *pmio, unsigned int reg)
{
	reg &= ~pmio->corex_mask;

	if (pmio->max_register && reg > pmio->max_register)
		return false;

	if (pmio->rd_table)
		return pmio_check_range_table(pmio, reg, pmio->rd_table);

	return true;
}

static bool pmio_is_aligned(struct pablo_mmio *pmio, unsigned int reg)
{
	if (!IS_ALIGNED(reg, PMIO_REG_STRIDE)) {
		pr_err("%s: unaligned access to 0x%d\n", pmio->name, reg);
		return false;
	}

	return true;
}
#else
#define pmio_is_aligned(p, r)	true
#endif

bool pmio_volatile(struct pablo_mmio *pmio, unsigned int reg)
{
	if (!pmio_readable(pmio, reg))
		return false;

	reg &= ~pmio->corex_mask;

	if (pmio->volatile_table)
		return pmio_check_range_table(pmio, reg, pmio->volatile_table);

	return !pmio->cache_ops;
}

bool pmio_writeable_noinc(struct pablo_mmio *pmio, unsigned int reg)
{
	reg &= ~pmio->corex_mask;

	if (pmio->wr_noinc_table)
		return pmio_check_range_table(pmio, reg, pmio->wr_noinc_table);

	return false;
}

static bool pmio_volatile_range(struct pablo_mmio *pmio, unsigned int reg,
	size_t num)
{
	unsigned int i;

	for (i = 0; i < num; i++)
		if (!pmio_volatile(pmio, reg + pmio_get_offset(pmio, i)))
			return false;

	return true;
}

static int pmio_set_name(struct pablo_mmio *pmio, const struct pmio_config *config)
{
	if (config->name) {
		const char *name = kstrdup_const(config->name, GFP_KERNEL);

		if (!name)
			return -ENOMEM;

		kfree_const(pmio->name);
		pmio->name = name;
	}

	return 0;
}

static bool pmio_range_add(struct pablo_mmio *pmio, struct pmio_range_node *data)
{
	struct rb_root *root = &pmio->range_tree;
	struct rb_node **new = &(root->rb_node), *parent = NULL;

	while (*new) {
		struct pmio_range_node *range =
			rb_entry(*new, struct pmio_range_node, node);

		parent = *new;
		if (data->max < range->min)
			new = &((*new)->rb_left);
		else if (data->min > range->max)
			new = &((*new)->rb_right);
		else
			return false;
	}

	rb_link_node(&data->node, parent, new);
	rb_insert_color(&data->node, root);

	return true;
}

static struct pmio_range_node *pmio_range_lookup(struct pablo_mmio *pmio,
						 unsigned int reg)
{
	struct rb_node *node = pmio->range_tree.rb_node;

	while (node) {
		struct pmio_range_node *range =
			rb_entry(node, struct pmio_range_node, node);

		if (reg < range->min)
			node = node->rb_left;
		else if (reg > range->max)
			node = node->rb_right;
		else
			return range;
	}

	return NULL;
}

static void pmio_range_exit(struct pablo_mmio *pmio)
{
	struct rb_node *next;
	struct pmio_range_node *range;

	next = rb_first(&pmio->range_tree);
	while (next) {
		range = rb_entry(next, struct pmio_range_node, node);
		next = rb_next(&range->node);
		rb_erase(&range->node, &pmio->range_tree);
		kfree(range->cache);
		kfree(range);
	}
}

/**
 * pmio_init() - Initialise Pablo MMIO
 *
 * @dev:	[in]	Device that will be interacted with
 * @ctx:	[in]	Data passed to specific callbacks
 * @config:	[in]	Configuration for PMIO
 *
 * Context: Process context. May sleep due to 'GFP_KERNEL' flag.
 *
 * Return: The return value will be an ERR_PTR() on error or a valid pointer to
 * a struct pablo_mmio.
 */
struct pablo_mmio *pmio_init(void *dev, void *ctx, const struct pmio_config *config)
{
	struct pablo_mmio *pmio;
	int ret;
	int i;

	if (!config)
		return ERR_PTR(-EINVAL);

	pmio = kzalloc(sizeof(*pmio), GFP_KERNEL);
	if (!pmio) {
		ret = -ENOMEM;
		goto err_alloc_pmio;
	}

	ret = pmio_set_name(pmio, config);
	if (ret)
		goto err_set_name;

	pmio->dev = dev;
	pmio->ctx = ctx;

	if (config->num_corexs > PMIO_MAX_NUM_COREX) {
		pr_err("%d COREXs cannot support. up to %d\n", config->num_corexs,
								PMIO_MAX_NUM_COREX);
		ret = -EINVAL;
		goto err_corex_config;
	}

	pmio->num_corexs = config->num_corexs;
	pmio->corex_stride = config->corex_stride;
	if (pmio->num_corexs && pmio->corex_stride) {
		u32 ob2 = order_base_2(pmio->num_corexs);
		if ((bits_per(pmio->corex_stride) + ob2) >= 32) {
			ret = -EINVAL;
			goto err_corex_config;
		}

		for (i = 0; i <= ob2; i++)
			pmio->corex_mask |= (pmio->corex_stride << i);

		pmio->corex_direct_index = config->corex_direct_offset / pmio->corex_stride;
	}

	pmio->relaxed_io = config->relaxed_io;

	pmio->max_register = config->max_register;
	pmio->wr_table = config->wr_table;
	pmio->rd_table = config->rd_table;
	pmio->volatile_table = config->volatile_table;
	pmio->wr_noinc_table = config->wr_noinc_table;

	pmio->use_single_read = config->use_single_read;
	pmio->use_single_write = config->use_single_write;

	pmio->mmio_base = config->mmio_base;
	pmio->reg_read = pmio_reg_read;
	pmio->reg_write = pmio_reg_write;

	pmio->dma_addr_shift = config->dma_addr_shift;
	pmio->phys_base = config->phys_base;

	if (config->reg_read)
		pmio->reg_read = config->reg_read;
	if (config->reg_write)
		pmio->reg_write = config->reg_write;

	pmio->range_tree = RB_ROOT;
	for (i = 0; i < config->num_ranges; i++) {
		const struct pmio_range_cfg *range = &config->ranges[i];
		struct pmio_range_node *new;

		if (range->max < range->min) {
			pr_err("invalid range %d: 0x%x < 0x%x\n", i,
			       range->max, range->min);
			ret = -EINVAL;
			goto err_init_range;
		}

		if (range->max > pmio->max_register) {
			pr_err("invalid range %d: 0x%x > 0x%x\n", i,
			       range->max, pmio->max_register);
			ret = -EINVAL;
			goto err_init_range;
		}

		new = kzalloc(sizeof(*new), GFP_KERNEL);
		if (!new) {
			pr_err("failed to alloc range %d\n", i);
			ret = -ENOMEM;
			goto err_init_range;
		}

		new->name = range->name;
		new->pmio = pmio;
		new->min = range->min;
		new->max = range->max;

		if (range->user_init)
			set_bit(PMIO_RANGE_S_USER_INIT, &new->state);

		if (range->select_reg) {
			new->select_reg = range->select_reg;
			new->select_val = range->select_val;
		} else {
			new->select_reg = pmio->max_register + 1;
		}

		if (range->offset_reg)
			new->offset_reg = range->offset_reg;
		else
			new->offset_reg = pmio->max_register + 1;

		if (range->trigger_reg) {
			new->trigger_reg = range->trigger_reg;
			new->trigger_val = range->trigger_val;
		} else {
			new->trigger_reg = pmio->max_register + 1;
		}

		new->stride = range->stride;
		new->cache_size = range->count * range->stride * PMIO_REG_STRIDE;
		new->cache_dirty = false;
		new->cache = kzalloc(new->cache_size, GFP_KERNEL);
		if (!new->cache) {
			pr_err("failed to alloc range cache %d\n", i);
			ret = -ENOMEM;
			kfree(new);
			goto err_init_range;
		}

		if (!pmio_range_add(pmio, new)) {
			pr_err("failed to add range %d\n", i);
			ret = -ENOMEM;
			kfree(new->cache);
			kfree(new);
			goto err_init_range;
		}
	}

	pmio->cache_type = config->cache_type;
	ret = pmio_cache_init(pmio, config);
	if (ret)
		goto err_init_cache;

	return pmio;

err_init_cache:
err_init_range:
	pmio_range_exit(pmio);

err_corex_config:
err_set_name:
	kfree_const(pmio->name);

err_alloc_pmio:
	kfree(pmio);

	return ERR_PTR(ret);
}
EXPORT_SYMBOL(pmio_init);

/**
 * pmio_exit() - Free a previously allocated PMIO
 *
 * @pmio:	[in]	Pablo MMIO to operate on.
 *
 * Context: Any context.
 */
void pmio_exit(struct pablo_mmio *pmio)
{
	pmio_cache_exit(pmio);
	pmio_range_exit(pmio);
	kfree_const(pmio->name);
	kfree(pmio);
}
EXPORT_SYMBOL(pmio_exit);

/**
 * pmio_reinit_cache() - Reinitialise the current PMIO cache
 *
 * @pmio:	[in]	Pablo MMIO to operate on
 * @config:	[in]	New configuration. Only the cache data will be used.
 *
 * Discard any existing register cache for the PMIO and initialize a
 * new cache. This can be used to restore the cache to defaults or to
 * update the cache configuration to reflect runtime discovery of the
 * hardware.
 *
 * Context: Process context. May sleep due to 'GFP_KERNEL' flag.
 * No explicit locking is done here, the user needs to ensure that
 * this function will not race with other calls to PMIO.
 *
 * Return: Can return negative error values, returns 0 on success. (see pmio_cache_init)
 */
int pmio_reinit_cache(struct pablo_mmio *pmio, const struct pmio_config *config)
{
	int ret;

	pmio_cache_exit(pmio);

	pmio->max_register = config->max_register;
	pmio->cache_type = config->cache_type;

	ret = pmio_set_name(pmio, config);
	if (ret)
		return ret;

	pmio->cache_bypass = false;
	pmio->cache_only = false;

	return pmio_cache_init(pmio, config);
}
EXPORT_SYMBOL(pmio_reinit_cache);

/**
 * pmio_reset_cache() - Reset the current PMIO cache
 *
 * @pmio:	[in]	Pablo MMIO to operate on
 *
 * Discard any existing register cache for the PMIO
 * and restore the cache to defaults.
 *
 * Context: Any context.
 */
void pmio_reset_cache(struct pablo_mmio *pmio)
{
	pmio_cache_reset(pmio);
}
EXPORT_SYMBOL(pmio_reset_cache);

static int pmio_reg_write(void *ctx, unsigned int reg, unsigned int val)
{
	struct pablo_mmio *pmio = ctx;

	if (unlikely(pmio->relaxed_io))
		writel_relaxed(val, pmio->mmio_base + reg);
	else
		writel(val, pmio->mmio_base + reg);

	return 0;
}

int _pmio_write(struct pablo_mmio *pmio, unsigned int reg, unsigned int val)
{
	int ret;

	if (unlikely(!pmio_writeable(pmio, reg)))
		return -EIO;

	if (likely(!pmio->cache_bypass)) {
		ret = pmio_cache_write(pmio, reg, val);
		if (ret)
			return ret;

		if (likely(pmio->cache_only && !pmio_volatile(pmio, reg))) {
			pmio->cache_dirty = true;
			return 0;
		}
	}

	trace_pmio_reg_write(pmio, reg, val);

	return pmio->reg_write(pmio->ctx ? pmio->ctx : pmio, reg, val);
}

/**
 * pmio_write() - Write a value to a single register
 *
 * @pmio:	[in]	Pablo MMIO to write to
 * @reg:	[in]	Register to write to
 * @val:	[in]	Value to be written
 *
 * Context: Any context.
 *
 * Return: A value of zero will be returned on success, a negative errno will
 * be returned in error cases.
 */
int pmio_write(struct pablo_mmio *pmio, unsigned int reg, unsigned int val)
{
	if (unlikely(!pmio_is_aligned(pmio, reg)))
		return -EPERM;

	return _pmio_write(pmio, reg, val);
}
EXPORT_SYMBOL(pmio_write);

int pmio_write_relaxed(struct pablo_mmio *pmio, unsigned int reg, unsigned int val)
{
	int ret;
	bool relaxed;

	relaxed = pmio->relaxed_io;
	pmio->relaxed_io = true;

	ret = _pmio_write(pmio, reg, val);

	pmio->relaxed_io = relaxed;

	return ret;
}
EXPORT_SYMBOL(pmio_write_relaxed);

static void __pmio_raw_write(struct pablo_mmio *pmio, unsigned int reg,
			     const void *val, size_t len)
{
	void __iomem *to = pmio->mmio_base + reg;

	trace_pmio_hw_write_start(pmio, reg, len / PMIO_REG_STRIDE);

	while (len && !IS_ALIGNED((unsigned long)to, 8)) {
		__raw_writel(*(u32 *)val, to);
		val += 4;
		to += 4;
		len -= 4;
	}

	while (len >= 8) {
		__raw_writeq(*(u64 *)val, to);
		val += 8;
		to += 8;
		len -= 8;
	}

	while (len) {
		__raw_writel(*(u32 *)val, to);
		val += 4;
		to += 4;
		len -= 4;
	}

	trace_pmio_hw_write_done(pmio, reg, len / PMIO_REG_STRIDE);
}

int _pmio_raw_write(struct pablo_mmio *pmio, unsigned int reg, const void *val, size_t len)
{
	size_t count = len / PMIO_REG_STRIDE;
	unsigned int element;
	size_t chunk_count, chunk_bytes, chunk_regs;
	int ret, i;

	if (!count)
		return -EINVAL;

	for (i = 0; i < count; i++) {
		element = reg + pmio_get_offset(pmio, i);
		if (!pmio_writeable(pmio, element) ||
				pmio_writeable_noinc(pmio, element)) {
			return -EINVAL;
		}
	}

	if (!pmio->cache_bypass) {
		ret = pmio_cache_raw_write(pmio, reg, val, len);
		if (ret) {
			ret = pmio_cache_bulk_write(pmio, reg, val, count);
			if (ret)
				return ret;
		}

		if (pmio->cache_only && !pmio_volatile(pmio, reg)) {
			pmio->cache_dirty = true;
			return 0;
		}
	}

	chunk_regs = count;
	if (pmio->use_single_write)
		chunk_regs = 1;

	chunk_count = count / chunk_regs;
	chunk_bytes = chunk_regs * PMIO_REG_STRIDE;

	for (i = 0; i < chunk_count; i++) {
		__pmio_raw_write(pmio, reg, val, chunk_bytes);
		reg += pmio_get_offset(pmio, chunk_regs);
		val += chunk_bytes;
		len -= chunk_bytes;
	}

	if (len)
		__pmio_raw_write(pmio, reg, val, len);

	return 0;
}

/**
 * pmio_raw_write() - Write raw values to one or more registers
 *
 * @pmio:	[in]	Pablo MMIO to write to
 * @reg:	[in]	Initial register to write to
 * @val:	[in]	Block of data to be written
 * @len:	[in]	Length of data pointed to by val
 *
 * This function is intended to be used for things like firmware
 * download where a large block of data needs to be transferred to the
 * device.
 *
 * Context: Any context.
 *
 * Return: A value of zero will be returned on success, a negative errno will
 * be returned in error cases.
 */
int pmio_raw_write(struct pablo_mmio *pmio, unsigned int reg, const void *val, size_t len)
{
	if (len % PMIO_REG_STRIDE)
		return -EINVAL;

	if (!pmio_is_aligned(pmio, reg))
		return -EPERM;

	return _pmio_raw_write(pmio, reg, val, len);
}
EXPORT_SYMBOL(pmio_raw_write);

/**
 * pmio_noinc_write() - Write data from a register without incrementing the
 * register number
 *
 * @pmio:	[in]	Pablo MMIO to write to
 * @reg:	[in]	Register to write to
 * @offset:	[in]	Internal offset to write to
 * @val:	[in]	Pointer to data buffer
 * @count:	[in]	Number of registers to write
 * @force:	[in]	Boolean indicating use force cache update
 *
 * The PMIO API usually assumes that bulk bus write operations will write a
 * range of registers. Some devices have certain registers for which a write
 * operation can write to an internal FIFO like Look Up Table.
 * The target register must be no increment writeable.
 * This will attempt multiple writes as required to write count.
 *
 * Context: Any context.
 *
 * Return: A value of zero will be returned on success, a negative errno will
 * be returned in error cases.
 */
int pmio_noinc_write(struct pablo_mmio *pmio, unsigned int reg, unsigned int offset,
		     const void *val, size_t count, bool force)
{
	int i, j;
	unsigned int ival;
	struct pmio_range_node *range;
	size_t size, ofs;
	int ret = 0;

	if (!count)
		return -EINVAL;

	if (!pmio_is_aligned(pmio, reg))
		return -EPERM;

	if (!pmio_writeable_noinc(pmio, reg)) {
		ret = -EINVAL;
		goto err_range;
	}

	range = pmio_range_lookup(pmio, reg);
	if (!range) {
		ret = -EINVAL;
		goto err_range;
	}

	ofs = offset * range->stride * PMIO_REG_STRIDE;
	size = count * range->stride * PMIO_REG_STRIDE;

	if (range->cache_size < (ofs + size)) {
		pr_err("out of bound: 0x%lx(0x%x)\n", offset + count,
				range->cache_size / range->stride / PMIO_REG_STRIDE);
		ret = -EINVAL;
		goto err_range;
	}

	if (!pmio->cache_bypass) {
		if (test_and_clear_bit(PMIO_RANGE_S_USER_INIT, &range->state))
			force = true;

		if (force || memcmp(range->cache + ofs, val, size)) {
			memcpy(range->cache + ofs, val, size);

			if (pmio->cache_only) {
				range->cache_dirty = true;
				pmio->cache_dirty = true;
			}
		}

		if (pmio->cache_only)
			goto skip_hw_write;
	}

	trace_pmio_hw_write_start(pmio, reg, count);

	if (range->select_reg <= pmio->max_register) {
		ret = pmio->reg_write(pmio->ctx ?
					pmio->ctx : pmio, range->select_reg,
				      range->select_val);
		if (ret) {
			pr_err("error in writing register: 0x%x ret: %d\n",
							range->select_reg, ret);
			goto err_hw_write;
		}
	}

	if (range->offset_reg <= pmio->max_register) {
		ret = pmio->reg_write(pmio->ctx ?
					pmio->ctx : pmio, range->offset_reg,
				      offset);
		if (ret) {
			pr_err("error in writing register: 0x%x ret: %d\n",
							range->offset_reg, ret);
			goto err_hw_write;
		}
	}

	if (range->trigger_reg <= pmio->max_register) {
		ret = pmio->reg_write(pmio->ctx ?
					pmio->ctx : pmio, range->trigger_reg,
				      range->trigger_val);
		if (ret) {
			pr_err("error in writing register: 0x%x ret: %d\n",
							range->trigger_reg, ret);
			goto err_hw_write;
		}
	}

	for (i = 0; i < count; i++) {
		for (j = 0; j < range->stride; j++) {
			ival = *(u32 *)(val + pmio_get_offset(pmio, i * range->stride + j));
			ret = pmio->reg_write(pmio->ctx ? pmio->ctx : pmio, reg, ival);
			if (ret) {
				pr_err("error in writing register: 0x%x ret: %d\n",
						reg + pmio_get_offset(pmio, i * range->stride + j),
						ret);
				goto err_hw_write;
			}
		}
	}

err_hw_write:
	trace_pmio_hw_write_done(pmio, reg, count);

skip_hw_write:
err_range:

	return ret;
}
EXPORT_SYMBOL(pmio_noinc_write);

/**
 * pmio_bulk_write() - Write multiple registers to the device
 *
 * @pmio:	[in]	Pablo MMIO to write to
 * @reg:	[in]	First register to be write from
 * @val:	[in]	Block of data to be written
 * @count:	[in]	Number of registers to write
 *
 * This function is intended to be used for writing a large block of
 * data to the device either in single transfer or multiple transfer.
 *
 * Context: Any context.
 *
 * Return: A value of zero will be returned on success, a negative errno will
 * be returned in error cases.
 */
int pmio_bulk_write(struct pablo_mmio *pmio, unsigned int reg, const void *val, size_t count)
{
	int i;
	unsigned int element, ival;
	int ret = 0;

	if (!count)
		return -EINVAL;

	if (!pmio_is_aligned(pmio, reg))
		return -EPERM;

	for (i = 0; i < count; i++) {
		element = reg + pmio_get_offset(pmio, i);
		if (!pmio_writeable(pmio, element) ||
				pmio_writeable_noinc(pmio, element)) {
			ret = -EINVAL;
			goto err_range;
		}
	}

	if (!pmio->cache_bypass) {
		for (i = 0; i < count; i++) {
			ival = *(u32 *)(val + pmio_get_offset(pmio, i));
			ret = pmio_cache_write(pmio, reg + pmio_get_offset(pmio, i), ival);
			if (ret) {
				pr_err("error in caching of register: 0x%x ret: %d\n",
						reg + pmio_get_offset(pmio, i), ret);

				pmio_cache_drop_region(pmio, reg + i, reg + count);

				goto err_cache_write;
			}
		}

		if (pmio->cache_only && !pmio_volatile(pmio, reg)) {
			pmio->cache_dirty = true;
			ret = 0;
			goto skip_hw_write;
		}
	}

	trace_pmio_hw_write_start(pmio, reg, count);

	for (i = 0; i < count; i++) {
		ival = *(u32 *)(val + pmio_get_offset(pmio, i));
		ret = pmio->reg_write(pmio->ctx ? pmio->ctx : pmio,
					reg + pmio_get_offset(pmio, i), ival);
		if (ret) {
			pr_err("error in writing register: 0x%x ret: %d\n",
					reg + pmio_get_offset(pmio, i), ret);

			pmio_cache_drop_region(pmio, reg + i, reg + count);

			goto err_hw_write;
		}
	}

	trace_pmio_hw_write_done(pmio, reg, count);

err_hw_write:
skip_hw_write:
err_cache_write:
err_range:

	return ret;
}
EXPORT_SYMBOL(pmio_bulk_write);

static int _pmio_multi_write(struct pablo_mmio *pmio, const struct pmio_reg_seq *regs,
			     int num_regs)
{
	int i;
	int ret;

	for (i = 0; i < num_regs; i++) {
		ret = _pmio_write(pmio, regs[i].reg, regs[i].val);
		if (ret != 0)
			return ret;

		if (regs[i].delay_us)
			fsleep(regs[i].delay_us);
	}

	return 0;
}

/**
 * pmio_multi_write() - Write multiple registers to the device
 *
 * @pmio:	[in]	Pablo MMIO to write to
 * @regs:	[in]	Array of structures containing register, value to
 *			be written
 * @num_regs:	[in]	Number of registers to write
 *
 * Write multiple registers to the device where the set of register, value
 * pairs are supplied in any order, possibly not all in a single range.
 *
 *
 * Context: Any context.
 *
 * Return: A value of zero will be returned on success, a negative errno will
 * be returned in error cases.
 */
int pmio_multi_write(struct pablo_mmio *pmio, const struct pmio_reg_seq *regs,
		     int num_regs)
{
	return _pmio_multi_write(pmio, regs, num_regs);
}
EXPORT_SYMBOL(pmio_multi_write);

/**
 * pmio_multi_write_bypassed() - Write multiple registers to the
 *                               device but not the cache
 *
 * @pmio:	[in]	Pablo MMIO to write to
 * @regs:	[in]	Array of structures containing register, value to
 *			be written
 * @num_regs:	[in]	Number of registers to write
 *
 * Write multiple registers to the device where the set of register, value
 * pairs are supplied in any order.
 *
 * Context: Any context.
 *
 * Return: A value of zero will be returned on success, a negative errno will
 * be returned in error cases.
 */
int pmio_multi_write_bypassed(struct pablo_mmio *pmio,
			      const struct pmio_reg_seq *regs,
			      int num_regs)
{
	int ret;
	bool bypass;

	bypass = pmio->cache_bypass;
	pmio->cache_bypass = true;

	ret = _pmio_multi_write(pmio, regs, num_regs);

	pmio->cache_bypass = bypass;

	return ret;
}
EXPORT_SYMBOL(pmio_multi_write_bypassed);

static int pmio_reg_read(void *ctx, unsigned int reg, unsigned int *val)
{
	struct pablo_mmio *pmio = ctx;

	if (unlikely(pmio->relaxed_io))
		*val = readl_relaxed(pmio->mmio_base + reg);
	else
		*val = readl(pmio->mmio_base + reg);

	return 0;
}

static int _pmio_read(struct pablo_mmio *pmio, unsigned int reg, unsigned int *val)
{
	int ret;

	if (likely(!pmio->cache_bypass)) {
		ret = pmio_cache_read(pmio, reg, val);
		if (ret == 0)
			return 0;
	}

	if (unlikely(pmio->cache_only && !pmio_volatile(pmio, reg)))
		return -EBUSY;

	if (unlikely(!pmio_readable(pmio, reg)))
		return -EIO;

	ret = pmio->reg_read(pmio->ctx ? pmio->ctx : pmio, reg, val);
	if (ret == 0) {
		trace_pmio_reg_read(pmio, reg, *val);

		if (likely(!pmio->cache_bypass))
			pmio_cache_write(pmio, reg, *val);
	}

	return ret;
}

/**
 * pmio_read() - Read a value from a single register
 *
 * @pmio:	[in]	Pablo MMIO to read from
 * @reg:	[in]	Register to be read from
 * @val:	[in]	Pointer to store read value
 *
 * Context: Any context.
 *
 * Return: A value of zero will be returned on success, a negative errno will
 * be returned in error cases.
 */
int pmio_read(struct pablo_mmio *pmio, unsigned int reg, unsigned int *val)
{
	if (unlikely(!pmio_is_aligned(pmio, reg)))
		return -EPERM;

	return _pmio_read(pmio, reg, val);
}
EXPORT_SYMBOL(pmio_read);

int pmio_read_relaxed(struct pablo_mmio *pmio, unsigned int reg, unsigned int *val)
{
	int ret;
	bool relaxed;

	relaxed = pmio->relaxed_io;
	pmio->relaxed_io = true;

	ret = _pmio_read(pmio, reg, val);

	pmio->relaxed_io = relaxed;

	return ret;
}
EXPORT_SYMBOL(pmio_read_relaxed);

static void __pmio_raw_read(struct pablo_mmio *pmio, unsigned int reg, void *val,
			    unsigned int len)
{
	trace_pmio_hw_read_start(pmio, reg, len / PMIO_REG_STRIDE);

	memcpy_fromio(val, pmio->mmio_base + reg, len);

	trace_pmio_hw_read_done(pmio, reg, len / PMIO_REG_STRIDE);
}

/**
 * pmio_raw_read() - Read raw data from the device
 *
 * @pmio:	[in]	Pablo MMIO to read from
 * @reg:	[in]	First register to be read from
 * @val:	[in]	Pointer to store read value
 * @len:	[in]	Size of data to read
 *
 * Context: Any context.
 *
 * Return: A value of zero will be returned on success, a negative errno will
 * be returned in error cases.
 */
int pmio_raw_read(struct pablo_mmio *pmio, unsigned int reg, void *val, size_t len)
{
	size_t count = len / PMIO_REG_STRIDE;
	unsigned int v;
	int i;
	int ret = 0;

	if (len % PMIO_REG_STRIDE)
		return -EINVAL;

	if (!pmio_is_aligned(pmio, reg))
		return -EPERM;

	if (!count)
		return -EINVAL;

	if (pmio_volatile_range(pmio, reg, count) || pmio->cache_bypass ||
	    pmio->cache_type == PMIO_CACHE_NONE) {
		size_t chunk_count, chunk_bytes;
		size_t chunk_regs = count;

		if (pmio->use_single_read)
			chunk_regs = 1;

		chunk_count = count / chunk_regs;
		chunk_bytes = chunk_regs * PMIO_REG_STRIDE;

		for (i = 0; i < chunk_count; i++) {
			__pmio_raw_read(pmio, reg, val, chunk_bytes);
			reg += pmio_get_offset(pmio, chunk_regs);
			val += chunk_bytes;
			len -= chunk_bytes;
		}

		if (len)
			__pmio_raw_read(pmio, reg, val, len);
	} else {
		for (i = 0; i < count; i++) {
			ret = _pmio_read(pmio, reg + pmio_get_offset(pmio, i), &v);
			if (ret)
				goto err_read;

			put_unaligned_le32(v, val + (i * PMIO_REG_STRIDE));
		}
	}

err_read:

	return ret;
}
EXPORT_SYMBOL(pmio_raw_read);

/**
 * pmio_bulk_read() - Read multiple registers from the device
 *
 * @pmio:	[in]	Pablo MMIO to read from
 * @reg:	[in]	First register to be read from
 * @val:	[in]	Pointer to store read value, in native register size for device
 * @count:	[in]	Number of registers to read
 *
 * Context: Any context.
 *
 * Return: A value of zero will be returned on success, a negative errno will
 * be returned in error cases.
 */
int pmio_bulk_read(struct pablo_mmio *pmio, unsigned int reg, void *val,
		   size_t count)
{
	int i;
	unsigned int ival;
	int ret = 0;
	u32 *vals = val;

	if (!count)
		return -EINVAL;

	if (!pmio_is_aligned(pmio, reg))
		return -EPERM;

	trace_pmio_hw_read_start(pmio, reg, count);

	for (i = 0; i < count; i++) {
		ret = _pmio_read(pmio, reg + pmio_get_offset(pmio, i), &ival);
		if (ret) {
			pr_err("error in reading register: 0x%x ret: %d\n",
					reg + pmio_get_offset(pmio, i), ret);
			goto err_hw_read;
		}

		vals[i] = ival;
	}

err_hw_read:
	trace_pmio_hw_read_done(pmio, reg, count);

	return ret;
}
EXPORT_SYMBOL(pmio_bulk_read);

static int _pmio_update_bits(struct pablo_mmio *pmio, unsigned int reg,
			     unsigned int mask, unsigned int val,
			     bool force, bool *changed)
{
	int ret;
	unsigned int tmp, orig;

	if (changed)
		*changed = false;

	if (pmio_volatile(pmio, reg) && pmio->reg_update_bits) {
		ret = pmio->reg_update_bits(pmio->ctx ? pmio->ctx : pmio, reg, mask, val);
		if (ret == 0 && changed)
			*changed = true;
	} else {
		ret = _pmio_read(pmio, reg, &orig);
		if (ret)
			return ret;

		tmp = orig & ~mask;
		tmp |= val & mask;

		if (force || (tmp != orig)) {
			ret = _pmio_write(pmio, reg, tmp);
			if (ret == 0 && changed)
				*changed = true;
		}
	}

	return ret;
}

/**
 * pmio_update_bits_base() - Perform a read/modify/write cycle on a register
 *
 * @pmio:	[in]	Pablo MMIO to write to
 * @reg:	[in]	Register to update
 * @mask:	[in]	Bitmask to change
 * @val:	[in]	New value for bitmask
 * @force:	[in]	Boolean indicating use force update
 * @changed:	[out]	Boolean indicating if a write was done
 *
 * Perform a read/modify/write cycle on a register with force, change
 * options.
 *
 * Context: Any context.
 *
 * Return: A value of zero will be returned on success, a negative errno will
 * be returned in error cases.
 */
int pmio_update_bits_base(struct pablo_mmio *pmio, unsigned int reg,
			  unsigned int mask, unsigned int val,
			  bool force, bool *changed)
{
	return _pmio_update_bits(pmio, reg, mask, val, force, changed);
}
EXPORT_SYMBOL(pmio_update_bits_base);

/**
 * pmio_test_bits() - Check if all specified bits are set in a register.
 *
 * @pmio:	[in]	Pablo MMIO to operate on
 * @reg:	[in]	Register to read from
 * @bits:	[in]	Bits to test
 *
 * Context: Any context.
 *
 * Return: 0 if at least one of the tested bits is not set, 1 if all
 * tested bits are set and a negative error number if the underlying
 * pmio_read() fails.
 */
int pmio_test_bits(struct pablo_mmio *pmio, unsigned int reg, unsigned int bits)
{
	unsigned int val, ret;

	ret = pmio_read(pmio, reg, &val);
	if (ret)
		return ret;

	return (val & bits) == bits;
}
EXPORT_SYMBOL(pmio_test_bits);

void pmio_set_relaxed_io(struct pablo_mmio *pmio, bool enable)
{
	pmio->relaxed_io = enable;
	if (!pmio->relaxed_io)
		__iomb();
}
EXPORT_SYMBOL(pmio_set_relaxed_io);

static void pmio_field_init(struct pmio_field *field, struct pablo_mmio *pmio,
			    struct pmio_field_desc desc)
{
	field->pmio = pmio;
	field->reg = desc.reg;
	field->shift = desc.lsb;
	field->mask = GENMASK(desc.msb, desc.lsb);
}

/**
 * pmio_field_alloc() - Allocate and initialise a register field.
 *
 * @pmio:	[in]	PMIO in which this register filed is located.
 * @desc:	[in]	Description of a register field
 *
 * Context: Process context. May sleep due to 'GFP_KERNEL' flag.
 *
 * Return: The return value will be an ERR_PTR() on error or a valid pointer
 * to a struct pmio_field. The pmio_field should be freed by the
 * user once its finished working with it using pmio_field_free().
 */
struct pmio_field *pmio_field_alloc(struct pablo_mmio *pmio,
				    struct pmio_field_desc desc)
{
	struct pmio_field *field = kzalloc(sizeof(*field), GFP_KERNEL);

	if (!field)
		return ERR_PTR(-ENOMEM);

	pmio_field_init(field, pmio, desc);

	return field;
}
EXPORT_SYMBOL(pmio_field_alloc);

/**
 * pmio_field_free() - Free register field allocated using pmio_field_alloc.
 *
 * @field:	[in]	PMIO field which should be freed.
 *
 * Context: Any context.
 */
void pmio_field_free(struct pmio_field *field)
{
	kfree(field);
}
EXPORT_SYMBOL(pmio_field_free);

/**
 * pmio_field_bulk_alloc() - Allocate and initialise a bulk register field.
 *
 * @pmio:	[in]	PMIO in which this register filed is located.
 * @fields:	[out]	Register fields will be allocated
 * @descs:	[in]	Descriptions of register fields
 * @num_descs:	[in]	Number of register fields.
 *
 * Context: Process context. May sleep due to 'GFP_KERNEL' flag.
 *
 * Return: The return value will be an -ENOMEM on error or zero for success.
 * Newly allocated pmio_fields should be freed by calling pmio_field_bulk_free().
 */
int pmio_field_bulk_alloc(struct pablo_mmio *pmio,
			  struct pmio_field **fields,
			  const struct pmio_field_desc *descs,
			  int num_fields)
{
	struct pmio_field *pf;
	int i;

	if (!num_fields)
		return -EINVAL;

	pf = kcalloc(num_fields, sizeof(*pf), GFP_KERNEL);
	if (!pf)
		return -ENOMEM;

	if (!*fields) {
		*fields = pf;

		pmio->fields = kcalloc(num_fields, sizeof(pf), GFP_KERNEL);
		for (i = 0; i < num_fields; i++) {
			pmio_field_init(&pf[i], pmio, descs[i]);
			pmio->fields[i] = &pf[i];
		}
	} else {
		for (i = 0; i < num_fields; i++) {
			pmio_field_init(&pf[i], pmio, descs[i]);
			fields[i] = &pf[i];
		}
	}

	return 0;
}
EXPORT_SYMBOL(pmio_field_bulk_alloc);

/**
 * pmio_field_bulk_free() - Free register field allocated using
 *			    pmio_field_bulk_alloc().
 *
 * @fields:	[in]	Register fields which should be freed.
 *
 * Context: Any context.
 */
void pmio_field_bulk_free(struct pablo_mmio *pmio, struct pmio_field *fields)
{
	kfree(fields);
	if (pmio->fields) {
		kfree(pmio->fields);
		pmio->fields = NULL;
	}
}
EXPORT_SYMBOL(pmio_field_bulk_free);

/**
 * pmio_field_read() - Read a value to a single register field
 *
 * @field:	[in]	Register field to read from
 * @val:	[in]	Pointer to store read value
 *
 * Context: Any context.
 *
 * Return: A value of zero will be returned on success, a negative errno will
 * be returned in error cases.
 */
int pmio_field_read(struct pablo_mmio *pmio, unsigned int reg,
		    struct pmio_field *field, unsigned int *val)
{
	int ret;
	unsigned int reg_val;

	ret = pmio_read(pmio, reg, &reg_val);
	if (ret)
		return ret;

	reg_val &= field->mask;
	reg_val >>= field->shift;
	*val = reg_val;

	return ret;
}
EXPORT_SYMBOL(pmio_field_read);

/**
 * pmio_field_update_bits_base() - Perform a read/modify/write cycle a
 *				   register field.
 *
 * @field:	[in]	Register field to write to
 * @mask:	[in]	Bitmask to change
 * @val:	[in]	Value to be written
 * @force:	[in]	Boolean indicating use force update
 * @changed:	[out]	Boolean indicating if a write was done
 *
 * Perform a read/modify/write cycle on the register field with change,
 * force option.
 *
 * Context: Any context.
 *
 * Return: A value of zero will be returned on success, a negative errno will
 * be returned in error cases.
 */
int pmio_field_update_bits_base(struct pablo_mmio *pmio,
				unsigned int reg,
				struct pmio_field *field,
				unsigned int mask, unsigned int val,
				bool force, bool *changed)
{
	mask = (mask << field->shift) & field->mask;

	return pmio_update_bits_base(pmio, reg,
				     mask, val << field->shift,
				     force, changed);
}
EXPORT_SYMBOL(pmio_field_update_bits_base);

/**
 * pmio_field_update_to - update a reigster field value to a given buffer.
 *
 * @field:	[in]	Register field to write to
 * @val:	[in]	Value to be written
 * @orig:	[in]	Original register value
 * @update:	[out]	Buffer to be written
 *
 * Context: Any context.
 */
void pmio_field_update_to(struct pmio_field *field, unsigned int val,
			  unsigned int orig, unsigned int *update)
{
	orig &= ~field->mask;
	orig |= (val << field->shift) & field->mask;
	*update = orig;
}
EXPORT_SYMBOL(pmio_field_update_to);

/* FIXME: remove */
void __iomem *pmio_get_base(struct pablo_mmio *pmio)
{
	return pmio->mmio_base;
}
EXPORT_SYMBOL(pmio_get_base);

struct pmio_field *pmio_get_field(struct pablo_mmio *pmio, unsigned int index)
{
	return pmio->fields ? pmio->fields[index] : NULL;
}
EXPORT_SYMBOL(pmio_get_field);

void PMIO_SET_F_COREX(struct pablo_mmio *pmio, u32 R, u32 F, u32 val)
{
	pmio->mmio_base += (pmio->corex_mask & R);
	pmio_field_write(pmio, R, pmio_get_field(pmio, F), val);
	pmio->mmio_base -= (pmio->corex_mask & R);
}
EXPORT_SYMBOL(PMIO_SET_F_COREX);

u32 PMIO_SET_V(struct pablo_mmio *pmio, u32 reg_val, u32 F, u32 val)
{
	u32 update;

	pmio_field_update_to(pmio_get_field(pmio, F), val, reg_val, &update);

	return update;
}
EXPORT_SYMBOL(PMIO_SET_V);

u32 PMIO_GET_F(struct pablo_mmio *pmio, u32 R, u32 F)
{
	u32 val;

	pmio_field_read(pmio, R, pmio_get_field(pmio, F), &val);

	return val;
}
EXPORT_SYMBOL(PMIO_GET_F);

u32 PMIO_GET_R(struct pablo_mmio *pmio, u32 R)
{
	u32 val;

	pmio_read(pmio, R, &val);

	return val;
}
EXPORT_SYMBOL(PMIO_GET_R);
