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

#ifndef LIB_PMIO_H
#define LIB_PMIO_H

#include <linux/rbtree.h>

#include "pablo-mmio.h"

#define PMIO_REG_STRIDE		4 /* Bytes */
#define PMIO_MAX_NUM_COREX	4

struct pmio_cache_ops;
struct pmio_formatter_ops;

struct pablo_mmio {
	const char *name;

	void *dev;
	void *ctx;

	bool relaxed_io;
	bool sanity_bypass;

	bool use_single_read;
	bool use_single_write;

	unsigned int max_register;
	const struct pmio_access_table *wr_table;
	const struct pmio_access_table *rd_table;
	const struct pmio_access_table *volatile_table;
	const struct pmio_access_table *wr_noinc_table;

	void __iomem *mmio_base;
	int (*reg_read)(void *ctx, unsigned int reg, unsigned int *val);
	int (*reg_write)(void *ctx, unsigned int reg, unsigned int val);
	int (*reg_update_bits)(void *ctx, unsigned int reg,
			       unsigned int mask, unsigned int val);

	struct rb_root range_tree;

	int num_corexs;
	int corex_stride;
	unsigned int corex_mask;
	unsigned int corex_direct_index;

	const struct pmio_cache_ops *cache_ops;
	enum pmio_cache_type cache_type;

	unsigned int cache_size_raw;
	unsigned int num_reg_defaults;
	unsigned int num_reg_defaults_raw;

	bool cache_only;
	bool cache_bypass;
	bool cache_free;

	struct pmio_reg_def *reg_defaults;
	const void *reg_defaults_raw;
	void *cache;
	bool cache_dirty;
	bool no_sync_defaults;

	int dma_addr_shift;
	phys_addr_t phys_base;

	struct pmio_field **fields;
};

struct pmio_cache_ops {
	const char *name;
	enum pmio_cache_type type;
	int (*init)(struct pablo_mmio *pmio);
	int (*exit)(struct pablo_mmio *pmio);
	int (*reset)(struct pablo_mmio *pmio);
	int (*read)(struct pablo_mmio *pmio, unsigned int reg, unsigned int *value);
	int (*raw_read)(struct pablo_mmio *pmio, unsigned int reg, void *val, size_t len);
	int (*write)(struct pablo_mmio *pmio, unsigned int reg, unsigned int value);
	int (*raw_write)(struct pablo_mmio *pmio, unsigned int reg, const void *val, size_t len);
	int (*sync)(struct pablo_mmio *pmio, unsigned int min, unsigned int max);
	int (*fsync)(struct pablo_mmio *pmio, void *buf,
		     enum pmio_formatter_type fmt,
		     unsigned int min, unsigned int max);
	int (*drop)(struct pablo_mmio *pmio, unsigned int min, unsigned int max);
};

struct pmio_formatter_ops {
	const char *name;
	enum pmio_cache_type type;
	int (*init)(struct pablo_mmio *pmio);
	int (*exit)(struct pablo_mmio *pmio);
	int (*flush)(struct pablo_mmio *pmio, unsigned int min, unsigned int max);
};

#if IS_ENABLED(PMIO_RANGE_CHECK_FULL)
bool pmio_writeable(struct pablo_mmio *pmio, unsigned int reg);
bool pmio_readable(struct pablo_mmio *pmio, unsigned int reg);
#else
#define pmio_writeable(p, r)	true
#define pmio_readable(p, r)	true
#endif
bool pmio_cached(struct pablo_mmio *pmio, unsigned int reg);
bool pmio_volatile(struct pablo_mmio *pmio, unsigned int reg);
bool pmio_writeable_noinc(struct pablo_mmio *pmio, unsigned int reg);

int _pmio_write(struct pablo_mmio *pmio, unsigned int reg, unsigned int val);

enum {
	PMIO_RANGE_S_USER_INIT,
};

struct pmio_range_node {
	struct rb_node node;
	const char *name;
	struct pablo_mmio *pmio;

	unsigned int min;
	unsigned int max;

	/* noinc registers like LUT */
	unsigned int select_reg;
	unsigned int select_val;
	unsigned int offset_reg;
	unsigned int trigger_reg;
	unsigned int trigger_val;
	unsigned int stride;

	unsigned long state;

	unsigned int cache_size;
	bool cache_dirty;
	void *cache;
};

/* pmio_cache core declarations */
int pmio_cache_init(struct pablo_mmio *pmio, const struct pmio_config *config);
void pmio_cache_exit(struct pablo_mmio *pmio);
void pmio_cache_reset(struct pablo_mmio *pmio);
int pmio_cache_read(struct pablo_mmio *pmio,
		    unsigned int reg, unsigned int *val);
int pmio_cache_write(struct pablo_mmio *pmio,
		     unsigned int reg, unsigned int val);
int pmio_cache_raw_write(struct pablo_mmio *pmio, unsigned int reg,
			 const void *val, size_t len);
int pmio_cache_bulk_write(struct pablo_mmio *pmio, unsigned int reg,
			  const void *val, size_t count);
int pmio_cache_sync_block(struct pablo_mmio *pmio, void *block,
			  unsigned int block_base, unsigned int start,
			  unsigned int end);
int pmio_cache_fsync_block(struct pablo_mmio *pmio, void *block,
			   unsigned int block_base,
			   void *buf, enum pmio_formatter_type fmt,
			   unsigned int start, unsigned int end);
int pmio_cache_fsync_noinc(struct pablo_mmio *pmio, void *buf,
			   struct pmio_range_node *range);

#define pmio_cache_get_val_addr(p, b, i)	((b) + ((i) * PMIO_REG_STRIDE))
int pmio_cache_get_val(struct pablo_mmio *pmio, const void *base,
		       unsigned int idx, unsigned int *val);
int pmio_cache_lookup_reg(struct pablo_mmio *pmio, unsigned int reg);

extern struct pmio_cache_ops pmio_cache_rbtree_ops;
extern struct pmio_cache_ops pmio_cache_flat_ops;

int _pmio_raw_write(struct pablo_mmio *pmio, unsigned int reg,
		    const void *val, size_t len);

static inline const char *pmio_name(const struct pablo_mmio *pmio)
{
	return pmio->name;
}

#define pmio_get_offset(p, i)		((i) << 2)
#define pmio_get_index_by_order(p, r)	((r) >> 2)

#endif /* LIB_PMIO_H */
