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

#define pr_fmt(fmt) "[" KBUILD_MODNAME "] pmio-cache: " fmt

#include <linux/rbtree.h>
#include <linux/slab.h>
#include <linux/bsearch.h>

#include "pmio.h"
#include "trace_pmio.h"
#include "pablo-kernel-variant.h"

static void *kvmemdup(const void *src, size_t len, gfp_t gfp)
{
	void *p = kvmalloc(len, gfp);

	if (p)
		memcpy(p, src, len);

	return p;
}

struct pmio_cache_flat_ctx {
	unsigned int *cache;
	unsigned int cache_count;
	unsigned int *cache_default;
	unsigned long *cache_dirty_bitmap;
};

static int pmio_cache_flat_init(struct pablo_mmio *pmio)
{
	struct pmio_cache_flat_ctx *ctx;
	int i;
	unsigned int reg, idx;

	if (!pmio || !pmio->max_register)
		return -EINVAL;

	pmio->cache = kvzalloc(sizeof *ctx, GFP_KERNEL);
	if (!pmio->cache)
		return -ENOMEM;

	ctx = pmio->cache;
	ctx->cache_count = pmio_get_index_by_order(pmio, pmio->max_register) + 1;
	ctx->cache = kvcalloc(ctx->cache_count, sizeof(unsigned int), GFP_KERNEL);
	if (!ctx->cache)
		goto err_alloc_cache;

	ctx->cache_dirty_bitmap = bitmap_zalloc(pmio->max_register, GFP_KERNEL);
	if (!ctx->cache_dirty_bitmap)
		goto err_alloc_cache_dirty_bitmap;

	for (i = 0; i < pmio->num_reg_defaults; i++) {
		reg = pmio->reg_defaults[i].reg;
		idx = pmio_get_index_by_order(pmio, reg);
		ctx->cache[idx] = pmio->reg_defaults[i].def;
	}

	ctx->cache_default = kvmemdup(ctx->cache,
				      array_size(ctx->cache_count, sizeof(unsigned int)),
				      GFP_KERNEL);

	return 0;

err_alloc_cache_dirty_bitmap:
	kvfree(ctx->cache);

err_alloc_cache:
	ctx->cache_count = 0;
	kvfree(pmio->cache);
	pmio->cache = NULL;

	return -ENOMEM;
}

static int pmio_cache_flat_exit(struct pablo_mmio *pmio)
{
	struct pmio_cache_flat_ctx *ctx = pmio->cache;

	if (!ctx)
		return 0;

	bitmap_free(ctx->cache_dirty_bitmap);
	kvfree(ctx->cache_default);
	kvfree(ctx->cache);
	kvfree(pmio->cache);
	pmio->cache = NULL;

	return 0;
}

static int pmio_cache_flat_reset(struct pablo_mmio *pmio)
{
	struct pmio_cache_flat_ctx *ctx = pmio->cache;
	int i;
	unsigned int reg, idx;

	if (ctx->cache_default) {
		memcpy(ctx->cache, ctx->cache_default,
		       array_size(ctx->cache_count, sizeof(unsigned int)));
	} else {
		for (i = 0; i < pmio->num_reg_defaults; i++) {
			reg = pmio->reg_defaults[i].reg;
			idx = pmio_get_index_by_order(pmio, reg);
			ctx->cache[idx] = pmio->reg_defaults[i].def;
		}
	}

	bitmap_clear(ctx->cache_dirty_bitmap, 0, pmio->max_register);

	return 0;
}

static int pmio_cache_flat_read(struct pablo_mmio *pmio,
				unsigned int reg, unsigned int *value)
{
	struct pmio_cache_flat_ctx *ctx = pmio->cache;
	unsigned int idx = pmio_get_index_by_order(pmio, reg);

	*value = ctx->cache[idx];

	return 0;
}

static int pmio_cache_flat_write(struct pablo_mmio *pmio,
				 unsigned int reg, unsigned int value)
{
	struct pmio_cache_flat_ctx *ctx = pmio->cache;
	unsigned int idx = pmio_get_index_by_order(pmio, reg);

	ctx->cache[idx] = value;
	bitmap_set(ctx->cache_dirty_bitmap, idx, 1);

	return 0;
}

static int pmio_cache_flat_raw_write(struct pablo_mmio *pmio, unsigned int reg,
				     const void *val, size_t len)
{
	struct pmio_cache_flat_ctx *ctx = pmio->cache;
	unsigned int idx = pmio_get_index_by_order(pmio, reg);

	memcpy((void *)&ctx->cache[idx], val, len);
	bitmap_set(ctx->cache_dirty_bitmap, idx, len / PMIO_REG_STRIDE);

	return 0;
}

static int pmio_cache_flat_sync(struct pablo_mmio *pmio, unsigned int min,
				unsigned int max)
{
	struct pmio_cache_flat_ctx *ctx = pmio->cache;
	unsigned int rs, re;
	int ret;

	bitmap_for_each_set_region(ctx->cache_dirty_bitmap, rs, re, min, max) {
		ret = pmio_cache_sync_block(pmio, ctx->cache, 0, rs, re);
		if (ret)
			return ret;

		bitmap_clear(ctx->cache_dirty_bitmap, rs, re - rs);
	}

	return 0;
}

static int pmio_cache_flat_fsync(struct pablo_mmio *pmio, void *buf,
				 enum pmio_formatter_type fmt,
				 unsigned int min, unsigned int max)
{

	struct pmio_cache_flat_ctx *ctx = pmio->cache;
	struct rb_node *node;
	struct pmio_range_node *range;
	unsigned int rs, re;
	int ret;

	bitmap_for_each_set_region(ctx->cache_dirty_bitmap, rs, re, min, max) {
		ret = pmio_cache_fsync_block(pmio, ctx->cache, 0,
					     buf, fmt, rs, re);
		if (ret)
			return ret;

		bitmap_clear(ctx->cache_dirty_bitmap, rs, re - rs);
	}

	for (node = rb_first(&pmio->range_tree); node; node = rb_next(node)) {
		range = rb_entry(node, struct pmio_range_node, node);
		if (range->cache_dirty) {
			ret = pmio_cache_fsync_noinc(pmio, buf, range);
			if (ret)
				return ret;

			range->cache_dirty = false;
		}
	}

	return 0;
}

static int pmio_cache_flat_drop(struct pablo_mmio *pmio, unsigned int min,
				unsigned int max)
{
	struct pmio_cache_flat_ctx *ctx = pmio->cache;

	/* FIXME: restore cache value from default? */

	bitmap_clear(ctx->cache_dirty_bitmap, min, max - min);

	return 0;
}

struct pmio_cache_ops pmio_cache_flat_ops = {
	.type = PMIO_CACHE_FLAT,
	.name = "flat",
	.init = pmio_cache_flat_init,
	.exit = pmio_cache_flat_exit,
	.reset = pmio_cache_flat_reset,
	.read = pmio_cache_flat_read,
	.write = pmio_cache_flat_write,
	.raw_write = pmio_cache_flat_raw_write,
	.sync = pmio_cache_flat_sync,
	.fsync = pmio_cache_flat_fsync,
	.drop = pmio_cache_flat_drop,
};

static int pmio_cache_flat_thin_write(struct pablo_mmio *pmio,
				 unsigned int reg, unsigned int value)
{
	struct pmio_cache_flat_ctx *ctx = pmio->cache;
	unsigned int idx = pmio_get_index_by_order(pmio, reg);

	if (ctx->cache[idx] != value) {
		ctx->cache[idx] = value;
		bitmap_set(ctx->cache_dirty_bitmap, idx, 1);
	}

	return 0;
}

struct pmio_cache_ops pmio_cache_flat_thin_ops = {
	.type = PMIO_CACHE_FLAT_THIN,
	.name = "flat thin",
	.init = pmio_cache_flat_init,
	.exit = pmio_cache_flat_exit,
	.reset = pmio_cache_flat_reset,
	.read = pmio_cache_flat_read,
	.write = pmio_cache_flat_thin_write,
	.raw_write = pmio_cache_flat_raw_write,
	.sync = pmio_cache_flat_sync,
	.fsync = pmio_cache_flat_fsync,
	.drop = pmio_cache_flat_drop,
};

struct pmio_cache_flat_corex_ctx {
	unsigned int cache_count;
	unsigned int *cache[PMIO_MAX_NUM_COREX];
	unsigned long *cache_dirty_bitmap[PMIO_MAX_NUM_COREX];
};

static int pmio_cache_flat_corex_init(struct pablo_mmio *pmio)
{
	struct pmio_cache_flat_corex_ctx *ctx;
	int i, j;
	unsigned int reg, idx;

	if (!pmio || !pmio->max_register)
		return -EINVAL;

	if (!pmio->num_corexs || !pmio->corex_stride)
		return -EINVAL;

	pmio->cache = kvzalloc(sizeof *ctx, GFP_KERNEL);
	if (!pmio->cache)
		return -ENOMEM;

	ctx = pmio->cache;
	ctx->cache_count = pmio_get_index_by_order(pmio, pmio->max_register) + 1;

	for (i = 0; i < pmio->num_corexs; i++) {
		ctx->cache[i] = kvcalloc(ctx->cache_count, sizeof(unsigned int), GFP_KERNEL);
		if (!ctx->cache[i])
			goto err_alloc_cache;
	}

	for (i = 0; i < pmio->num_corexs; i++) {
		ctx->cache_dirty_bitmap[i] = bitmap_zalloc(pmio->max_register, GFP_KERNEL);
		if (!ctx->cache_dirty_bitmap[i])
			goto err_alloc_cache_dirty_bitmap;
	}

	for (i = 0; i < pmio->num_corexs; i++) {
		for (j = 0; j < pmio->num_reg_defaults; j++) {
			reg = pmio->reg_defaults[j].reg;
			idx = pmio_get_index_by_order(pmio, reg);
			ctx->cache[i][idx] = pmio->reg_defaults[j].def;
		}
	}

	return 0;

err_alloc_cache_dirty_bitmap:
	while (i-- > 0)
		bitmap_free(ctx->cache_dirty_bitmap[i]);
	for (i = 0; i < pmio->num_corexs; i++)
		kvfree(ctx->cache[i]);

err_alloc_cache:
	while (i-- > 0)
		kvfree(ctx->cache[i]);
	kvfree(pmio->cache);
	pmio->cache = NULL;

	return -ENOMEM;
}

static int pmio_cache_flat_corex_exit(struct pablo_mmio *pmio)
{
	struct pmio_cache_flat_corex_ctx *ctx = pmio->cache;
	int i;

	if (!ctx)
		return 0;

	for (i = 0; i < pmio->num_corexs; i++) {
		bitmap_free(ctx->cache_dirty_bitmap[i]);
		kvfree(ctx->cache[i]);
	}

	kvfree(pmio->cache);
	pmio->cache = NULL;

	return 0;
}

static int pmio_cache_flat_corex_reset(struct pablo_mmio *pmio)
{
	struct pmio_cache_flat_corex_ctx *ctx = pmio->cache;
	int i;

	for (i = 0; i < pmio->num_corexs; i++) {
		if (i != pmio->corex_direct_index) {
			memcpy(ctx->cache[i],
			       ctx->cache[pmio->corex_direct_index],
			       array_size(ctx->cache_count,
					  sizeof(unsigned int)));

			bitmap_clear(ctx->cache_dirty_bitmap[i], 0,
				     pmio->max_register);
		}
	}

	return 0;
}

static int pmio_cache_flat_corex_read(struct pablo_mmio *pmio,
				      unsigned int reg, unsigned int *value)
{
	struct pmio_cache_flat_corex_ctx *ctx = pmio->cache;
	unsigned int ridx = pmio_get_index_by_order(pmio, reg & ~pmio->corex_mask);
	unsigned int cidx = reg / pmio->corex_stride;

	*value = ctx->cache[cidx][ridx];

	return 0;
}

static int pmio_cache_flat_corex_write(struct pablo_mmio *pmio,
				       unsigned int reg, unsigned int value)
{
	struct pmio_cache_flat_corex_ctx *ctx = pmio->cache;
	unsigned int ridx = pmio_get_index_by_order(pmio, reg & ~pmio->corex_mask);
	unsigned int cidx = reg / pmio->corex_stride;

	if (ctx->cache[cidx][ridx] != value) {
		ctx->cache[cidx][ridx] = value;
		bitmap_set(ctx->cache_dirty_bitmap[cidx], ridx, 1);
	}

	return 0;
}

static int pmio_cache_flat_corex_sync(struct pablo_mmio *pmio, unsigned int min,
				      unsigned int max)
{
	struct pmio_cache_flat_corex_ctx *ctx = pmio->cache;
	unsigned int rs, re;
	int ret, i;

	min = min & ~pmio->corex_mask;
	max = max & ~pmio->corex_mask;

	/* sync COREXs wide */
	for (i = 0; i < pmio->num_corexs; i++) {
		bitmap_for_each_set_region(ctx->cache_dirty_bitmap[i],
					   rs, re, min, max) {
			ret = pmio_cache_sync_block(pmio,
						    ctx->cache[i],
						    pmio->corex_stride * i,
						    rs, re);
			if (ret)
				return ret;

			bitmap_clear(ctx->cache_dirty_bitmap[i], rs, re - rs);
		}
	}

	return 0;
}

static int pmio_cache_flat_corex_fsync(struct pablo_mmio *pmio, void *buf,
				       enum pmio_formatter_type fmt,
				       unsigned int min, unsigned int max)
{
	struct pmio_cache_flat_corex_ctx *ctx = pmio->cache;
	struct rb_node *node;
	struct pmio_range_node *range;
	unsigned int rs, re;
	int ret, i;

	min = min & ~pmio->corex_mask;
	max = max & ~pmio->corex_mask;

	/* format sync COREXs wide */
	for (i = 0; i < pmio->num_corexs; i++) {
		bitmap_for_each_set_region(ctx->cache_dirty_bitmap[i],
					   rs, re, min, max) {
			ret = pmio_cache_fsync_block(pmio,
						     ctx->cache[i],
						     pmio->corex_stride * i,
						     buf, fmt,
						     rs, re);
			if (ret)
				return ret;

			bitmap_clear(ctx->cache_dirty_bitmap[i], rs, re - rs);
		}
	}

	for (node = rb_first(&pmio->range_tree); node; node = rb_next(node)) {
		range = rb_entry(node, struct pmio_range_node, node);
		if (range->cache_dirty) {
			ret = pmio_cache_fsync_noinc(pmio, buf, range);
			if (ret)
				return ret;

			range->cache_dirty = false;
		}
	}

	return 0;
}

static int pmio_cache_flat_corex_drop(struct pablo_mmio *pmio, unsigned int min,
				      unsigned int max)
{
	struct pmio_cache_flat_corex_ctx *ctx = pmio->cache;
	unsigned int cidx = min / pmio->corex_stride;

	min = min & ~pmio->corex_mask;
	max = max & ~pmio->corex_mask;

	bitmap_clear(ctx->cache_dirty_bitmap[cidx], min, max - min);

	return 0;
}

struct pmio_cache_ops pmio_cache_flat_corex_ops = {
	.type = PMIO_CACHE_FLAT_COREX,
	.name = "flat corex",
	.init = pmio_cache_flat_corex_init,
	.exit = pmio_cache_flat_corex_exit,
	.reset = pmio_cache_flat_corex_reset,
	.read = pmio_cache_flat_corex_read,
	.write = pmio_cache_flat_corex_write,
	.sync = pmio_cache_flat_corex_sync,
	.fsync = pmio_cache_flat_corex_fsync,
	.drop = pmio_cache_flat_corex_drop,
};

static const struct pmio_cache_ops *cache_types[] = {
	&pmio_cache_flat_ops,
	&pmio_cache_flat_thin_ops,
	&pmio_cache_flat_corex_ops,
};

static int pmio_cache_hw_init(struct pablo_mmio *pmio)
{
	int i, j;
	int ret;
	int count;
	unsigned int reg, val;
	void *tmp_buf;
	const u32 *vals;

	for (count = 0, i = 0; i < pmio->num_reg_defaults_raw; i++)
		if (pmio_readable(pmio, i * PMIO_REG_STRIDE) &&
		    !pmio_volatile(pmio, i * PMIO_REG_STRIDE))
			count++;

	if (!count) {
		pmio->cache_bypass = true;
		return 0;
	}

	pmio->num_reg_defaults = count;
	pmio->reg_defaults = kvmalloc_array(count, sizeof(struct pmio_reg_def),
					    GFP_KERNEL);
	if (!pmio->reg_defaults)
		return -ENOMEM;

	if (!pmio->reg_defaults_raw) {
		bool bypass = pmio->cache_bypass;
		pr_warn("no cache defaults, reading back from HW\n");

		pmio->cache_bypass = true;
		tmp_buf = kvmalloc(pmio->cache_size_raw, GFP_KERNEL);
		if (!tmp_buf) {
			ret = -ENOMEM;
			goto err_alloc_raw;
		}
		ret = pmio_raw_read(pmio, 0, tmp_buf, pmio->cache_size_raw);
		pmio->cache_bypass = bypass;
		if (!ret) {
			pmio->reg_defaults_raw = tmp_buf;
			pmio->cache_free = true;

			pr_debug("%s register defaults hexdump\n", pmio->name);
			print_hex_dump_debug("offset: ", DUMP_PREFIX_OFFSET,
					16, 4, tmp_buf, pmio->cache_size_raw, false);
		} else {
			kvfree(tmp_buf);
		}
	}

	/* fill the reg_defaults */
	for (i = 0, j = 0, vals = pmio->reg_defaults_raw; i < pmio->num_reg_defaults_raw; i++) {
		reg = i * PMIO_REG_STRIDE;

		if (!pmio_readable(pmio, reg))
			continue;

		if (pmio_volatile(pmio, reg))
			continue;

		if (vals) {
			val = vals[i];
		} else {
			bool bypass = pmio->cache_bypass;

			pmio->cache_bypass = true;
			ret = pmio_read(pmio, reg, &val);
			pmio->cache_bypass = bypass;
			if (ret) {
				pr_err("failed to read reg: 0x%x ret: %d\n", reg, ret);
				goto err_read_to_fill;
			}
		}

		pmio->reg_defaults[j].reg = reg;
		pmio->reg_defaults[j].def = val;
		j++;

		pr_debug("default: reg: 0x%x, val: 0x%x\n", reg, val);
	}

	return 0;

err_read_to_fill:
err_alloc_raw:
	kvfree(pmio->reg_defaults);

	return ret;
}

int pmio_cache_init(struct pablo_mmio *pmio, const struct pmio_config *config)
{
	int ret;
	int i;
	void *tmp_buf;

	if (pmio->cache_type == PMIO_CACHE_NONE) {
		if (config->reg_defaults || config->num_reg_defaults_raw)
			pr_warn("no cache used with register defaults set!\n");
		pmio->cache_bypass = true;
		return 0;
	}

	if (config->reg_defaults && !config->num_reg_defaults) {
		pr_err("register defaults are set without the number!\n");
		return -EINVAL;
	}

	if (!config->reg_defaults && config->num_reg_defaults) {
		pr_err("register defaults are not set with the number!\n");
		return -EINVAL;
	}

	for (i = 0; i < config->num_reg_defaults; i++)
		if (config->reg_defaults[i].reg % PMIO_REG_STRIDE)
			return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(cache_types); i++)
		if (cache_types[i]->type == pmio->cache_type)
			break;

	if (i == ARRAY_SIZE(cache_types)) {
		pr_err("could not match cache type: %d\n", pmio->cache_type);
		return -EINVAL;
	}

	pmio->num_reg_defaults = config->num_reg_defaults;
	pmio->num_reg_defaults_raw = config->num_reg_defaults_raw;
	pmio->reg_defaults_raw = config->reg_defaults_raw;
	pmio->cache_size_raw = config->num_reg_defaults_raw * PMIO_REG_STRIDE;

	pmio->cache = NULL;
	pmio->cache_ops = cache_types[i];

	if (!pmio->cache_ops->read ||
	    !pmio->cache_ops->write ||
	    !pmio->cache_ops->name)
		return -EINVAL;

	if (config->reg_defaults) {
		tmp_buf = kvmemdup(config->reg_defaults, pmio->num_reg_defaults *
				   sizeof(struct pmio_reg_def), GFP_KERNEL);
		if (!tmp_buf)
			return -ENOMEM;
		pmio->reg_defaults = tmp_buf;
	} else if (pmio->num_reg_defaults_raw) {
		ret = pmio_cache_hw_init(pmio);
		if (ret < 0)
			return ret;
		if (pmio->cache_bypass)
			return 0;
	}

	if (!pmio->max_register)
		pmio->max_register = (pmio->num_reg_defaults_raw - 1)
					* PMIO_REG_STRIDE;

	if (pmio->cache_ops->init) {
		pr_info("initializing %s cache\n", pmio->cache_ops->name);
		ret = pmio->cache_ops->init(pmio);
		if (ret)
			goto err_cache_init;
	}

	return 0;

err_cache_init:
	kvfree(pmio->reg_defaults);
	if (pmio->cache_free)
		kvfree(pmio->reg_defaults_raw);

	return ret;
}

void pmio_cache_exit(struct pablo_mmio *pmio)
{
	if (pmio->cache_type == PMIO_CACHE_NONE)
		return;

	kvfree(pmio->reg_defaults);
	if (pmio->cache_free)
		kvfree(pmio->reg_defaults_raw);

	if (pmio->cache_ops->exit) {
		pr_info("destroying %s cache for %s\n",
				pmio->cache_ops->name, pmio->name);
		pmio->cache_ops->exit(pmio);
	}
}

void pmio_cache_reset(struct pablo_mmio *pmio)
{
	if (pmio->cache_type == PMIO_CACHE_NONE)
		return;

	if (pmio->cache_ops->reset) {
		pr_debug("resetting %s cache for %s\n",
				pmio->cache_ops->name, pmio->name);
		pmio->cache_ops->reset(pmio);
	}
}

int pmio_cache_read(struct pablo_mmio *pmio,
		    unsigned int reg, unsigned int *val)
{
	int ret;

	if (pmio->cache_type == PMIO_CACHE_NONE)
		return -ENOSYS;

	if (!pmio_volatile(pmio, reg)) {
		ret = pmio->cache_ops->read(pmio, reg, val);

		if (ret == 0)
			trace_pmio_reg_read_cache(pmio, reg, *val);

		return ret;
	}

	return -EINVAL;
}

int pmio_cache_write(struct pablo_mmio *pmio,
		     unsigned int reg, unsigned int val)
{
	if (pmio->cache_type == PMIO_CACHE_NONE)
		return 0;

	if (!pmio_volatile(pmio, reg))
		return pmio->cache_ops->write(pmio, reg, val);

	return 0;
}

int pmio_cache_raw_write(struct pablo_mmio *pmio, unsigned int reg,
			 const void *val, size_t len)
{
	if (pmio->cache_type == PMIO_CACHE_NONE)
		return 0;

	if (!pmio_volatile(pmio, reg)) {
		if (pmio->cache_ops->raw_write)
			return pmio->cache_ops->raw_write(pmio, reg,
							  val, len);
		else
			return -EINVAL;
	}

	return 0;
}

int pmio_cache_bulk_write(struct pablo_mmio *pmio, unsigned int reg,
			  const void *val, size_t count)
{
	int ret, i;
	unsigned int ival;

	for (i = 0; i < count; i++) {
		ival = *(u32 *)(val + pmio_get_offset(pmio, i));
		ret = pmio_cache_write(pmio, reg + pmio_get_offset(pmio, i), ival);
		if (ret) {
			pr_err("error in caching of register: 0x%x ret: %d\n",
					reg + pmio_get_offset(pmio, i), ret);

			if (pmio->cache_ops && pmio->cache_ops->drop)
				pmio->cache_ops->drop(pmio, reg + i, reg + count);

			return ret;
		}
	}

	return 0;
}

static bool pmio_cache_reg_needs_sync(struct pablo_mmio *pmio,
				      unsigned int reg, unsigned int val)
{
	int ret;

	if (!pmio->no_sync_defaults)
		return true;

	ret = pmio_cache_lookup_reg(pmio, reg & ~pmio->corex_mask);
	if (ret >= 0 && val == pmio->reg_defaults[ret].def)
		return false;

	return true;
}

static int pmio_cache_default_sync(struct pablo_mmio *pmio, unsigned int min,
				   unsigned int max)
{
	unsigned int reg;

	for (reg = min; reg <= max; reg += PMIO_REG_STRIDE) {
		unsigned int val;
		int ret;

		if (pmio_volatile(pmio, reg) ||
		    !pmio_writeable(pmio, reg))
			continue;

		ret = pmio_cache_read(pmio, reg, &val);
		if (ret)
			return ret;

		if (!pmio_cache_reg_needs_sync(pmio, reg, val))
			continue;

		pmio->cache_bypass = true;
		ret = _pmio_write(pmio, reg, val);
		pmio->cache_bypass = false;
		if (ret) {
			pr_err("unable to sync register 0x%x ret: %d\n", reg, ret);
			return ret;
		}
		pr_debug("synced register 0x%x, value 0x%x\n", reg, val);
	}

	return 0;
}

int pmio_cache_sync_region(struct pablo_mmio *pmio, unsigned int min,
			   unsigned int max)
{
	int ret = 0;
	bool bypass;

	if (!pmio->cache_ops)
		return -EINVAL;

	bypass = pmio->cache_bypass;
	if (min == 0 && max == pmio->max_register)
		pr_debug("syncing %s cache\n", pmio->cache_ops->name);
	else
		pr_debug("syncing %s cache from 0x%x-0x%x\n",
			pmio->cache_ops->name, min, max);

	trace_pmio_cache_sync_region(pmio, pmio->cache_ops->name, min, max, "start");

	if (!pmio->cache_dirty)
		goto no_need_sync;

	if (pmio->cache_ops->sync)
		ret = pmio->cache_ops->sync(pmio, min, max);
	else
		ret = pmio_cache_default_sync(pmio, min, max);

	if (ret == 0)
		pmio->cache_dirty = false;

no_need_sync:
	pmio->cache_bypass = bypass;
	pmio->no_sync_defaults = false;

	trace_pmio_cache_sync_region(pmio, pmio->cache_ops->name, min, max, "stop");

	return ret;
}

int pmio_cache_sync(struct pablo_mmio *pmio)
{
	return pmio_cache_sync_region(pmio, 0, pmio->max_register);
}
EXPORT_SYMBOL_GPL(pmio_cache_sync);

int pmio_cache_fsync_region(struct pablo_mmio *pmio, void *buf,
			    enum pmio_formatter_type fmt,
			    unsigned int min, unsigned int max)
{
	int ret = 0;
	bool bypass;

	if (!pmio->cache_ops)
		return -EINVAL;

	bypass = pmio->cache_bypass;
	if (min == 0 && max == pmio->max_register)
		pr_debug("syncing %s cache to formatter: %d\n",
			  pmio->cache_ops->name, fmt);
	else
		pr_debug("syncing %s cache from 0x%x-0x%x to formatter: %d\n",
			  pmio->cache_ops->name, min, max, fmt);

	trace_pmio_cache_fsync_region(pmio, pmio->cache_ops->name, fmt, min, max, "start");

	if (!pmio->cache_dirty)
		goto no_need_sync;

	if (pmio->cache_ops->sync)
		ret = pmio->cache_ops->fsync(pmio, buf, fmt, min, max);
	else
		ret = pmio_cache_default_sync(pmio, min, max);

	if (ret == 0)
		pmio->cache_dirty = false;

no_need_sync:
	pmio->cache_bypass = bypass;
	pmio->no_sync_defaults = false;

	trace_pmio_cache_fsync_region(pmio, pmio->cache_ops->name, fmt, min, max, "stop");

	return ret;
}


int pmio_cache_fsync(struct pablo_mmio *pmio, void *buf, enum pmio_formatter_type fmt)
{
	return pmio_cache_fsync_region(pmio, buf, fmt, 0, pmio->max_register);
}
EXPORT_SYMBOL(pmio_cache_fsync);

int pmio_cache_drop_region(struct pablo_mmio *pmio, unsigned int min,
			   unsigned int max)
{
	int ret;

	if (!pmio->cache_ops || !pmio->cache_ops->drop)
		return -EINVAL;

	trace_pmio_cache_drop_region(pmio, pmio->cache_ops->name, min, max);

	ret = pmio->cache_ops->drop(pmio, min, max);

	return ret;
}

void pmio_cache_set_only(struct pablo_mmio *pmio, bool enable)
{
	if (pmio->cache_bypass && enable) {
		pr_err("bypass cache only?\n");
		return;
	}

	pmio->cache_only = enable;
}
EXPORT_SYMBOL(pmio_cache_set_only);

void pmio_cache_set_bypass(struct pablo_mmio *pmio, bool enable)
{
	if (pmio->cache_only && enable) {
		pr_err("cache only bypass?\n");
		return;
	}

	pmio->cache_bypass = enable;
}
EXPORT_SYMBOL(pmio_cache_set_bypass);

void pmio_cache_mark_dirty(struct pablo_mmio *pmio)
{
	pmio->cache_dirty = true;
	pmio->no_sync_defaults = true;
}

int pmio_cache_get_val(struct pablo_mmio *pmio, const void *base,
		       unsigned int idx, unsigned int *val)
{
	const u32 *cache = base;

	if (!base)
		return -EINVAL;

	*val = cache[idx];

	return 0;
}

static int pmio_cache_default_cmp(const void *a, const void *b)
{
	const struct pmio_reg_def *_a = a;
	const struct pmio_reg_def *_b = b;

	return _a->reg - _b->reg;
}

int pmio_cache_lookup_reg(struct pablo_mmio *pmio, unsigned int reg)
{
	struct pmio_reg_def key;
	struct pmio_reg_def *r;

	key.reg = reg;
	key.def = 0;

	r = bsearch(&key, pmio->reg_defaults, pmio->num_reg_defaults,
		    sizeof(struct pmio_reg_def), pmio_cache_default_cmp);

	if (r)
		return r - pmio->reg_defaults;
	else
		return -ENOENT;
}

static int pmio_cache_sync_block_single(struct pablo_mmio *pmio, void *block,
					unsigned int block_base,
					unsigned int start, unsigned int end)
{
	unsigned int i, regtmp, val;
	int ret;

	for (i = start; i < end; i++) {
		regtmp = block_base + (i * PMIO_REG_STRIDE);

		if (!pmio_writeable(pmio, regtmp))
			continue;

		if (pmio_cache_get_val(pmio, block, i, &val))
			break;

		if (!pmio_cache_reg_needs_sync(pmio, regtmp, val))
			continue;

		pmio->cache_bypass = true;
		ret = _pmio_write(pmio, regtmp, val);
		pmio->cache_bypass = false;
		if (ret) {
			pr_err("unable to sync register 0x%x ret: %d\n", regtmp, ret);
			return ret;
		}
		pr_debug("synced register 0x%x, value 0x%x\n", regtmp, val);
	}

	return 0;
}

static int pmio_cache_sync_block_raw_flush(struct pablo_mmio *pmio, const void **data,
					   unsigned int base, unsigned int cur)
{
	int ret;
	size_t len, count;

	if (*data == NULL)
		return 0;

	count = (cur - base) / PMIO_REG_STRIDE;
	len = count * PMIO_REG_STRIDE;

	pr_debug("writing %zu bytes for %d registers from 0x%x-0x%x\n",
				len, count, base, cur - PMIO_REG_STRIDE);

	pmio->cache_bypass = true;

	ret = _pmio_raw_write(pmio, base, *data, len);
	if (ret)
		pr_err("unable to sync registers 0x%x-0x%x ret: %d\n",
				base, cur - PMIO_REG_STRIDE, ret);

	pmio->cache_bypass = false;

	*data = NULL;

	return ret;
}

static int pmio_cache_sync_block_raw(struct pablo_mmio *pmio, void *block,
				     unsigned int block_base, unsigned int start,
				     unsigned int end)
{
	unsigned int i, val;
	unsigned int regtmp = 0;
	unsigned int base = block_base;
	const void *data = NULL;
	int ret;

	for (i = start; i < end; i++) {
		regtmp = block_base + (i * PMIO_REG_STRIDE);

		if (!pmio_writeable(pmio, regtmp)) {
			ret = pmio_cache_sync_block_raw_flush(pmio, &data,
							      base, regtmp);
			if (ret)
				return ret;
			continue;
		}

		if (pmio_cache_get_val(pmio, block, i, &val))
			break;

		if (!pmio_cache_reg_needs_sync(pmio, regtmp, val)) {
			ret = pmio_cache_sync_block_raw_flush(pmio, &data,
							      base, regtmp);
			if (ret)
				return ret;
			continue;
		}

		if (!data) {
			data = pmio_cache_get_val_addr(pmio, block, i);
			base = regtmp;
		}
	}

	return pmio_cache_sync_block_raw_flush(pmio, &data, base, regtmp +
					       PMIO_REG_STRIDE);
}

int pmio_cache_sync_block(struct pablo_mmio *pmio, void *block,
			  unsigned int block_base, unsigned int start,
			  unsigned int end)
{
	if (!pmio->use_single_write)
		return pmio_cache_sync_block_raw(pmio, block,
						 block_base, start, end);
	else
		return pmio_cache_sync_block_single(pmio, block,
						    block_base, start, end);
}

static const u32 type_map_lookup_tbl[NUM_OF_CLD_VALUES + 1] = {
	0x00000000,
	0x00000001,
	0x00000005,
	0x00000015,
	0x00000055,
	0x00000155,
	0x00000555,
	0x00001555,
	0x00005555,
	0x00015555,
	0x00055555,
	0x00155555,
	0x00555555,
	0x01555555,
	0x05555555,
	0x15555555,
	0x55555555,
};

static int inc_addr_formatter_flush(struct pablo_mmio *pmio, const void **data,
				    void *buf, enum pmio_formatter_type fmt,
				    unsigned int base, unsigned int cur)
{
	size_t count;
	const size_t payload_bytes = NUM_OF_CLD_VALUES * PMIO_REG_STRIDE;
	struct c_loader_buffer *clb = (struct c_loader_buffer *)buf;
	struct c_loader_header *clh = &clb->clh[clb->num_of_headers];
	struct c_loader_payload *clp = &clb->clp[clb->num_of_headers];
	const void *val = *data;

	if (*data == NULL)
		return 0;

	count = (cur - base) / PMIO_REG_STRIDE;

	pr_debug("writing %zu bytes for %d registers from 0x%x-0x%x\n",
				count * PMIO_REG_STRIDE, count,
				base, cur - PMIO_REG_STRIDE);

	while (count >= NUM_OF_CLD_VALUES) {
		memcpy(clp, val, payload_bytes);

		clh->mode = 0x0008;
		clh->frame_id = clb->frame_id;
		clh->payload_dva = (clb->payload_dva
			+ (clb->num_of_headers * payload_bytes)) >> pmio->dma_addr_shift;
		clh->cr_addr = pmio->phys_base + base;
		clh->type_map = type_map_lookup_tbl[NUM_OF_CLD_VALUES];

		val += payload_bytes;
		count -= NUM_OF_CLD_VALUES;
		base += payload_bytes;

		clb->num_of_headers++;
		clh = &clb->clh[clb->num_of_headers];
		clp = &clb->clp[clb->num_of_headers];
	}

	if (count) {
		memcpy(clp, val, count * PMIO_REG_STRIDE);

		clh->mode = 0x0008;
		clh->frame_id = clb->frame_id;
		clh->payload_dva = (clb->payload_dva
			+ (clb->num_of_headers * payload_bytes)) >> pmio->dma_addr_shift;
		clh->cr_addr = pmio->phys_base + base;
		clh->type_map = type_map_lookup_tbl[count];

		clb->num_of_headers++;
	}

	*data = NULL;

	return 0;
}

static int pair_formatter_flush(struct pablo_mmio *pmio, const void **data,
				void *buf, enum pmio_formatter_type fmt,
				unsigned int base, unsigned int cur)
{
	size_t count;
	const size_t payload_bytes = NUM_OF_CLD_VALUES * PMIO_REG_STRIDE;
	struct c_loader_buffer *clb = (struct c_loader_buffer *)buf;
	struct c_loader_header *clh = &clb->clh[clb->num_of_headers];
	struct c_loader_payload *clp = &clb->clp[clb->num_of_headers];
	const void *val = *data;

	if (*data == NULL)
		return 0;

	count = (cur - base) / PMIO_REG_STRIDE;

	pr_debug("writing %zu bytes for %d registers from 0x%x-0x%x\n",
				count * PMIO_REG_STRIDE, count,
				base, cur - PMIO_REG_STRIDE);

	while (count > 0) {
		if (clb->num_of_pairs == 0) {
			clh->mode = 0x0009;
			clh->frame_id = clb->frame_id;
			clh->payload_dva = (clb->payload_dva
				+ (clb->num_of_headers * payload_bytes)) >> pmio->dma_addr_shift;
			clh->cr_addr = 0x0;
			clh->type_map = 0x5;
		} else {
			clh->type_map = (clh->type_map << 4) | 0x05;
		}

		clp->pairs[clb->num_of_pairs].addr = pmio->phys_base + base;
		clp->pairs[clb->num_of_pairs].val = *(u32 *)val;

		clb->num_of_pairs++;
		val += PMIO_REG_STRIDE;
		count--;
		base += PMIO_REG_STRIDE;

		if (clb->num_of_pairs == NUM_OF_CLD_PAIRS) {
			clb->num_of_headers++;
			clb->num_of_pairs = 0;

			clh = &clb->clh[clb->num_of_headers];
			clp = &clb->clp[clb->num_of_headers];
		}
	}

	*data = NULL;

	return 0;
}

static int pmio_cache_fsync_block_raw(struct pablo_mmio *pmio, void *block,
				      unsigned int block_base,
				      void *buf, enum pmio_formatter_type fmt,
				      unsigned int start, unsigned int end)
{
	unsigned int i, val;
	unsigned int regtmp = 0;
	unsigned int base = block_base;
	const void *data = NULL;
	int ret;
	int (*formatter_flush)(struct pablo_mmio *pmio, const void **data,
		     void *buf, enum pmio_formatter_type fmt,
		     unsigned int base, unsigned int cur);

	if (fmt == PMIO_FORMATTER_INC)
		formatter_flush = inc_addr_formatter_flush;
	else
		formatter_flush = pair_formatter_flush;

	for (i = start; i < end; i++) {
		regtmp = block_base + (i * PMIO_REG_STRIDE);

		if (!pmio_writeable(pmio, regtmp)) {
			ret = formatter_flush(pmio, &data, buf, fmt, base, regtmp);
			if (ret)
				return ret;
			continue;
		}

		if (pmio_cache_get_val(pmio, block, i, &val))
			break;

		if (!pmio_cache_reg_needs_sync(pmio, regtmp, val)) {
			ret = formatter_flush(pmio, &data, buf, fmt, base, regtmp);
			if (ret)
				return ret;
			continue;
		}

		if (!data) {
			data = pmio_cache_get_val_addr(pmio, block, i);
			base = regtmp;
		}
	}

	return formatter_flush(pmio, &data, buf, fmt, base, regtmp + PMIO_REG_STRIDE);
}

int pmio_cache_fsync_block(struct pablo_mmio *pmio, void *block,
			   unsigned int block_base,
			   void *buf, enum pmio_formatter_type fmt,
			   unsigned int start, unsigned int end)
{
	return pmio_cache_fsync_block_raw(pmio, block, block_base,
					  buf, fmt, start, end);
}

static int rpt_addr_formatter_flush(struct pablo_mmio *pmio, void *buf, const void *val,
				    unsigned int base, size_t count)
{
	const size_t payload_bytes = NUM_OF_CLD_VALUES * PMIO_REG_STRIDE;
	struct c_loader_buffer *clb = (struct c_loader_buffer *)buf;
	struct c_loader_header *clh = &clb->clh[clb->num_of_headers];
	struct c_loader_payload *clp = &clb->clp[clb->num_of_headers];

	pr_debug("writing %zu bytes for noinc register: 0x%x\n",
			count * PMIO_REG_STRIDE, base);

	if (clb->num_of_pairs > 0) {
		clb->num_of_headers++;
		clb->num_of_pairs = 0;

		clh = &clb->clh[clb->num_of_headers];
		clp = &clb->clp[clb->num_of_headers];
	}

	while (count >= NUM_OF_CLD_VALUES) {
		memcpy(clp, val, payload_bytes);

		clh->mode = 0x000B;
		clh->frame_id = clb->frame_id;
		clh->payload_dva = (clb->payload_dva
			+ (clb->num_of_headers * payload_bytes)) >> pmio->dma_addr_shift;
		clh->cr_addr = pmio->phys_base + base;
		clh->type_map = type_map_lookup_tbl[NUM_OF_CLD_VALUES];

		val += payload_bytes;
		count -= NUM_OF_CLD_VALUES;

		clb->num_of_headers++;
		clh = &clb->clh[clb->num_of_headers];
		clp = &clb->clp[clb->num_of_headers];
	}

	if (count) {
		memcpy(clp, val, count * PMIO_REG_STRIDE);

		clh->mode = 0x000B;
		clh->frame_id = clb->frame_id;
		clh->payload_dva = (clb->payload_dva
			+ (clb->num_of_headers * payload_bytes)) >> pmio->dma_addr_shift;
		clh->cr_addr = pmio->phys_base + base;
		clh->type_map = type_map_lookup_tbl[count];

		clb->num_of_headers++;
	}

	return 0;
}

int pmio_cache_fsync_noinc(struct pablo_mmio *pmio, void *buf, struct pmio_range_node *range)
{
	return  rpt_addr_formatter_flush(pmio, buf, range->cache, range->min,
					 range->cache_size / PMIO_REG_STRIDE);
}

/* support pair mode only temporarily */
int pmio_cache_ext_fsync(struct pablo_mmio *pmio, void *buf, const void *items,
			 size_t block_count)
{
	const size_t payload_bytes = NUM_OF_CLD_PAIRS * PMIO_REG_STRIDE * 2;
	struct c_loader_buffer *clb = (struct c_loader_buffer *)buf;
	struct c_loader_header *clh = &clb->clh[clb->num_of_headers];
	struct c_loader_payload *clp = &clb->clp[clb->num_of_headers];
	int i;

	pr_debug("writing %zu bytes for external register\n",
				block_count * PMIO_REG_STRIDE);

	if (clb->num_of_pairs > 0) {
		clb->num_of_headers++;
		clb->num_of_pairs = 0;

		clh = &clb->clh[clb->num_of_headers];
		clp = &clb->clp[clb->num_of_headers];
	}

	while (block_count >= NUM_OF_CLD_PAIRS) {
		memcpy(clp, items, payload_bytes);
		for (i = 0; i < NUM_OF_CLD_PAIRS; i++)
			clp->pairs[i].addr += pmio->phys_base;

		clh->mode = 0x0009;
		clh->frame_id = clb->frame_id;
		clh->payload_dva = (clb->payload_dva
			+ (clb->num_of_headers * payload_bytes)) >> pmio->dma_addr_shift;
		clh->cr_addr = 0;
		clh->type_map = type_map_lookup_tbl[NUM_OF_CLD_PAIRS * 2];

		items += payload_bytes;
		block_count -= NUM_OF_CLD_PAIRS;

		clb->num_of_headers++;
		clh = &clb->clh[clb->num_of_headers];
		clp = &clb->clp[clb->num_of_headers];
	}

	if (block_count) {
		memcpy(clp, items, block_count * PMIO_REG_STRIDE * 2);
		for (i = 0; i < block_count; i++)
			clp->pairs[i].addr += pmio->phys_base;

		clh->mode = 0x0009;
		clh->frame_id = clb->frame_id;
		clh->payload_dva = (clb->payload_dva
			+ (clb->num_of_headers * payload_bytes)) >> pmio->dma_addr_shift;
		clh->cr_addr = 0;
		clh->type_map = type_map_lookup_tbl[block_count * 2];

		clb->num_of_headers++;
	}

	return 0;
}
EXPORT_SYMBOL(pmio_cache_ext_fsync);
