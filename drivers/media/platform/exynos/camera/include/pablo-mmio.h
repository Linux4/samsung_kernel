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

#ifndef PABLO_MMIO_H
#define PABLO_MMIO_H

#include <linux/list.h>
#include <linux/ktime.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/iopoll.h>

#define PMIO_RANGE_CHECK_FULL	0

/**
* DOC: Pablo MMIO(PMIO) concept
*
*       [IP]       [Instance]         [Frame]                    :                         : }
*    +--------+    :        :        :        :                  +-------------------------+
*    |        | 1 *+--------+ 1 1..* +--------+             ---> | DMA buffer for c-loader | } ---> { c-loader }
*    |  PMIO  |--->|        |------->| PMIO $ |}    /\     /\    +-------------------------+             |
*    | Config | :  |  PMIO  |  \     +-------+     /  \   /  \   :                         : }           |
*    |        | :  +--------+   \--->| PMIO $ |}-[Formatter]  \                                          |
*    |        | :  +--------+    \   +--------+         \/     -> { Configuration Registers } <-----------
*    +--------+    |        |     \->| PMIO $ |}                  :                         :
*                  :        :        :        :                   :                         :
*/

struct pablo_mmio;

struct pmio_field {
	struct pablo_mmio *pmio;
	unsigned int mask;
	/* lsb */
	unsigned int shift;
	unsigned int reg;
};

enum pmio_cache_type {
	PMIO_CACHE_NONE,
	PMIO_CACHE_FLAT,
	PMIO_CACHE_FLAT_THIN,
	PMIO_CACHE_FLAT_COREX,
};

enum pmio_formatter_type {
	PMIO_FORMATTER_BYPASS,
	PMIO_FORMATTER_INC,
	PMIO_FORMATTER_PAIR,
	PMIO_FORMATTER_RPT,
	PMIO_FORMATTER_FULL,
};

struct c_loader_header {
	u16	frame_id;
	u16	mode;
	u32	payload_dva;
	u32	cr_addr;
	u32	type_map;
} __attribute__((packed));

#define SIZE_OF_CLD_PAYLOAD	64
#define NUM_OF_CLD_VALUES	(SIZE_OF_CLD_PAYLOAD >> 2)
#define NUM_OF_CLD_PAIRS	(SIZE_OF_CLD_PAYLOAD >> 3)
struct c_loader_payload {
	union {
		u32 values[NUM_OF_CLD_VALUES];
		struct {
			u32 addr;
			u32 val;
		} pairs[NUM_OF_CLD_PAIRS];
	};
} __attribute__((packed));

struct c_loader_buffer {
	u32 num_of_headers;
	u32 num_of_values;
	u32 num_of_pairs;

	u16 frame_id;
	dma_addr_t header_dva;
	dma_addr_t payload_dva;
	struct c_loader_header *clh;
	struct c_loader_payload *clp;
};

/**
 * struct pmio_reg_def - Default value for a register.
 *
 * @reg: Register address.
 * @def: Register default value.
 *
 * We use an array of structs rather than a simple array as many modern devices
 * have very sparse register maps.
 */
struct pmio_reg_def {
	unsigned int reg;
	unsigned int def;
};

/**
 * struct pmio_reg_seq - An individual write from a sequence of writes.
 *
 * @reg: Register address.
 * @val: Register value.
 * @delay_us: Delay to be applied after the register write in microseconds
 *
 * Register/value pairs for sequences of writes with an optional delay in
 * microseconds to be applied after each write.
 */
struct pmio_reg_seq {
	unsigned int reg;
	unsigned int val;
	unsigned int delay_us;
};

#define PMIO_REG_SEQ(_reg, _def, _delay_us) {		\
				.reg = _reg,		\
				.def = _def,		\
				.delay_us = _delay_us,	\
				}
#define PMIO_REG_SEQ0(_reg, _def)	PMIO_REG_SEQ(_reg, _def, 0)

/**
 * pmio_read_poll_timeout - Poll until a condition is met or a timeout occurs
 *
 * @pmio:	[in]	Pablo MMIO to write to
 * @reg:	[in]	Register to poll
 * @val:	[in]	Unsigned integer variable to read the value into
 * @cond:	[in]	Break condition (usually involving @val)
 * @sleep_us:	[in]	Maximum time to sleep between reads in us (0 tight-loops).
 *			Should be less than ~20ms since usleep_range is used.
 * @timeout_us:	[in]	Timeout in us, 0 means never timeout
 *
 * Context: Must not be called from atomic context if sleep_us or timeout_us are used.
 *
 * Return: 0 on success and -ETIMEDOUT upon a timeout or the pmio_read
 * error return value in case of a error read. In the two former cases,
 * the last read value at @reg is stored in @val.
 *
 * This is modelled after the readx_poll_timeout macros in linux/iopoll.h.
 */
#define pmio_read_poll_timeout(pmio, reg, val, cond, sleep_us, timeout_us) \
({ \
	int __ret, __tmp; \
	__tmp = read_poll_timeout(pmio_read, __ret, __ret || (cond), \
			sleep_us, timeout_us, false, (pmio), (reg), &(val)); \
	__ret ?: __tmp; \
})

/**
 * pmio_field_read_poll_timeout - Poll until a condition is met or a timeout occurs
 *
 * @field:	[in]	PMIO field to read from
 * @val:	[in]	Unsigned integer variable to read the value into
 * @cond:	[in]	Break condition (usually involving @val)
 * @sleep_us:	[in]	Maximum time to sleep between reads in us (0 tight-loops).
 *			Should be less than ~20ms since usleep_range is used.
 * @timeout_us:	[in]	Timeout in us, 0 means never timeout
 *
 * Context: Must not be called from atomic context if sleep_us or timeout_us are used.
 *
 * Return: 0 on success and -ETIMEDOUT upon a timeout or the pmio_field_read
 * error return value in case of a error read. In the two former cases,
 * the last read value is stored in @val.
 *
 * This is modelled after the readx_poll_timeout macros in linux/iopoll.h.
 */
#define pmio_field_read_poll_timeout(field, val, cond, sleep_us, timeout_us) \
({ \
	int __ret, __tmp; \
	__tmp = read_poll_timeout(pmio_field_read, __ret, __ret || (cond), \
			sleep_us, timeout_us, false, (field), &(val)); \
	__ret ?: __tmp; \
})

/**
 * struct pmio_range - A register range, used for access related checks
 *		       (readable/writeable/volatile checks)
 *
 * @min: Address of first register
 * @max: Address of last register
 */
struct pmio_range {
	unsigned int min;
	unsigned int max;
};

#define pmio_reg_range(_min, _max) { .min = _min, .max = _max, }

/**
 * struct pmio_access_table - A table of register ranges for access checks
 *
 * @yes_ranges : Pointer to an array of PMIO ranges used as "yes ranges"
 * @n_yes_ranges: Size of the above array
 * @no_ranges: Pointer to an array of PMIO ranges used as "no ranges"
 * @n_no_ranges: Size of the above array
 *
 * A table of ranges including some yes ranges and some no ranges.
 * If a register belongs to a no_range, the corresponding check function
 * will return false. If a register belongs to a yes range, the corresponding
 * check function will return true. "no_ranges" are searched first.
 */
struct pmio_access_table {
	const struct pmio_range *yes_ranges;
	unsigned int n_yes_ranges;
	const struct pmio_range *no_ranges;
	unsigned int n_no_ranges;
};

/**
 * struct pmio_range_cfg - Configuration for indirectly accessed registers.
 *
 * @name: Descriptive name for diagnostics
 *
 * @min: Address of the lowest register address in this range.
 * @max: Address of the highest register in this range.
 *
 * @user_init: If set, Cache for the range will be initialized at 1st
 *	       write operation by user buffer instead of zero-initialized.
 *
 * @select_reg: Selection register between multiple register sets.
 * @select_val: Value to be written to select_reg.
 * @offset_reg: Start offset register of internal FIFO.
 * @trigger_reg: Trigger register of internal FIFO.
 * @trigger_val: Value to be written to trigger_reg.
 * @stride: Number of register stride for each entry.
 * @count: Size of registers of internal FIFO.
 *
 */
struct pmio_range_cfg {
	const char *name;

	unsigned int min;
	unsigned int max;

	unsigned int user_init;

	/* noinc registers like LUT */
	unsigned int select_reg;
	unsigned int select_val;
	unsigned int offset_reg;
	unsigned int trigger_reg;
	unsigned int trigger_val;
	unsigned int stride;
	unsigned int count;
};

/**
 * struct pmio_config - Configuration for the PMIO of a device.
 *
 * @name: Optional name of the PMIO.
 *
 * @num_corexs:   Number of corex shadow sets.
 * @corex_stride: The corex address stride. Valid corex addresses base are a
 *                multiple of this value.
 * @corex_direct_offset: The offset of COREX direct set.

 * @relaxed_io: Optional, specifies whether PMIO operations use relaxed
 *		I/O memory access primitives or not.
 *
 * @max_register:     Optional, specifies the maximum valid register address.
 * @wr_table:         Optional, points to a struct pmio_access_table specifying
 *                    valid ranges for write access. The register can be written
 *                    to if it belongs to one of the ranges specified by wr_table.
 * @rd_table:         As above, for read access. The register can be read from if
 *                    it belongs to one of the ranges specified by rd_table.
 * @volatile_table:   As above, for volatile registers. The register value can't
 *                    be cached if it belongs to one of the ranges specified by
 *                    volatile_table.
 * @wr_noinc_table:   As above, for no increment writeable registers. The register
 *                    supports multiple write operations without incrementing the
 *                    register number if it belongs to one of the ranges specified
 *                    by wr_noinc_table.
 *
 * @use_single_read:  If set, converts the bulk read operation into a series of
 *                    single read operations. This is useful for a device that
 *                    does not support  bulk read.
 * @use_single_write: If set, converts the bulk write operation into a series of
 *                    single write operations. This is useful for a device that
 *                    does not support bulk write.
 *
 * @mmio_base:	  Base address for I/O memory access.
 * @reg_read:	  Optional callback that if filled will be used to perform
 *          	  all the reads from the registers.
 * @reg_write:	  Same as above for writing.
 *
 * @ranges:	Array of configuration entries for indirectly accessed address ranges.
 * @num_ranges: Number of range configuration entries.
 *
 * @fields:	Optional array of PMIO field description entries.
 * @num_fields: Number of PMIO field description entries.
 *
 * @cache_type:		  The actual cache type.
 * @reg_defaults:	  Power on reset values for registers (for use with
 *			  register cache support).
 * @num_reg_defaults:	  Number of elements in reg_defaults.
 * @reg_defaults_raw:	  Power on reset values for registers (for use with
 *			  register cache support).
 * @num_reg_defaults_raw: Number of elements in reg_defaults_raw.
 *
 * @dma_addr_shfit: DMA address shift for header in format-sync.
 * @phys_base:	    Physical base address for header/payload in format-sync.
 *
 */
struct pmio_config {
	const char *name;

	int num_corexs;
	int corex_stride;
	int corex_direct_offset;

	bool relaxed_io;

	unsigned int max_register;
	const struct pmio_access_table *wr_table;
	const struct pmio_access_table *rd_table;
	const struct pmio_access_table *volatile_table;
	const struct pmio_access_table *wr_noinc_table;

	bool use_single_read;
	bool use_single_write;

	void __iomem *mmio_base;
	int (*reg_read)(void *ctx, unsigned int reg, unsigned int *val);
	int (*reg_write)(void *ctx, unsigned int reg, unsigned int val);

	const struct pmio_range_cfg *ranges;
	unsigned int num_ranges;

	const struct pmio_field_desc *fields;
	unsigned int num_fields;

	enum pmio_cache_type cache_type;
	const struct pmio_reg_def *reg_defaults;
	unsigned int num_reg_defaults;
	const void *reg_defaults_raw;
	unsigned int num_reg_defaults_raw;

	int dma_addr_shift;
	phys_addr_t phys_base;
};

struct pablo_mmio *pmio_init(void *dev, void *ctx, const struct pmio_config *config);
void pmio_exit(struct pablo_mmio *pmio);
int pmio_reinit_cache(struct pablo_mmio *pmio,
		      const struct pmio_config *config);
void pmio_reset_cache(struct pablo_mmio *pmio);

int pmio_write(struct pablo_mmio *pmio, unsigned int reg, unsigned int val);
int pmio_write_relaxed(struct pablo_mmio *pmio, unsigned int reg, unsigned int val);
int pmio_raw_write(struct pablo_mmio *pmio, unsigned int reg,
		   const void *val, size_t len);
int pmio_noinc_write(struct pablo_mmio *pmio, unsigned int reg, unsigned int offset,
		     const void *val, size_t count, bool force);
int pmio_bulk_write(struct pablo_mmio *pmio, unsigned int reg, const void *val,
		    size_t count);
int pmio_multi_write(struct pablo_mmio *pmio, const struct pmio_reg_seq *regs,
		     int num_regs);
int pmio_multi_write_bypassed(struct pablo_mmio *pmio,
			      const struct pmio_reg_seq *regs,
			      int num_regs);

int pmio_read(struct pablo_mmio *pmio, unsigned int reg, unsigned int *val);
int pmio_read_relaxed(struct pablo_mmio *pmio, unsigned int reg, unsigned int *val);
int pmio_raw_read(struct pablo_mmio *pmio, unsigned int reg, void *val,
		  size_t val_len);
int pmio_bulk_read(struct pablo_mmio *pmio, unsigned int reg, void *val,
		   size_t count);

int pmio_update_bits_base(struct pablo_mmio *pmio, unsigned int reg,
			  unsigned int mask, unsigned int val,
			  bool force, bool *changed);
static inline int pmio_update_bits(struct pablo_mmio *pmio, unsigned int reg,
				   unsigned int mask, unsigned int val)
{
	return pmio_update_bits_base(pmio, reg, mask, val, false, NULL);
}
static inline int pmio_update_bits_check(struct pablo_mmio *pmio, unsigned int reg,
					 unsigned int mask, unsigned int val,
					 bool *changed)
{
	return pmio_update_bits_base(pmio, reg, mask, val, false, changed);
}
static inline int pmio_write_bits(struct pablo_mmio *pmio, unsigned int reg,
				  unsigned int mask, unsigned int val)
{
	return pmio_update_bits_base(pmio, reg, mask, val, true, NULL);
}
static inline int pmio_set_bits(struct pablo_mmio *pmio,
				unsigned int reg, unsigned int bits)
{
	return pmio_update_bits_base(pmio, reg, bits, bits, false, NULL);
}

static inline int pmio_clear_bits(struct pablo_mmio *pmio,
				  unsigned int reg, unsigned int bits)
{
	return pmio_update_bits_base(pmio, reg, bits, 0, false, NULL);
}
int pmio_test_bits(struct pablo_mmio *pmio, unsigned int reg, unsigned int bits);

void pmio_set_relaxed_io(struct pablo_mmio *pmio, bool enable);

int pmio_cache_sync(struct pablo_mmio *pmio);
int pmio_cache_fsync(struct pablo_mmio *pmio, void *buf, enum pmio_formatter_type fmt);
int pmio_cache_sync_region(struct pablo_mmio *pmio, unsigned int min,
			   unsigned int max);
int pmio_cache_fsync_region(struct pablo_mmio *pmio, void *buf,
			    enum pmio_formatter_type fmt,
			    unsigned int min, unsigned int max);
int pmio_cache_ext_fsync(struct pablo_mmio *pmio, void *buf, const void *items,
			 size_t block_count);
int pmio_cache_drop_region(struct pablo_mmio *pmio, unsigned int min,
			   unsigned int max);
void pmio_cache_set_only(struct pablo_mmio *pmio, bool enable);
void pmio_cache_set_bypass(struct pablo_mmio *pmio, bool enable);
void pmio_cache_mark_dirty(struct pablo_mmio *pmio);

bool pmio_check_range_table(struct pablo_mmio *pmio, unsigned int reg,
			    const struct pmio_access_table *table);
static inline bool pmio_reg_in_range(unsigned int reg,
				     const struct pmio_range *range)
{
	return reg >= range->min && reg <= range->max;
}
bool pmio_reg_in_ranges(unsigned int reg,
			const struct pmio_range *ranges,
			unsigned int nranges);

void pmio_set_relaxed_io(struct pablo_mmio *pmio, bool enable);

/**
 * struct pmio_field_desc - Description of a register field
 *
 * @reg: Offset of the register.
 * @lsb: lsb of the register field.
 * @msb: msb of the register field.
 */
struct pmio_field_desc {
	unsigned int reg;
	unsigned int lsb;
	unsigned int msb;
};

#define PMIO_FIELD_DESC(_reg, _lsb, _msb) {	\
				.reg = _reg,	\
				.lsb = _lsb,	\
				.msb = _msb,	\
				}

struct pmio_field *pmio_field_alloc(struct pablo_mmio *pmio,
				    struct pmio_field_desc desc);
void pmio_field_free(struct pmio_field *field);
int pmio_field_bulk_alloc(struct pablo_mmio *pmio,
			  struct pmio_field **fields,
			  const struct pmio_field_desc *desc,
			  int num_fields);
void pmio_field_bulk_free(struct pablo_mmio *pmio, struct pmio_field *field);

int pmio_field_read(struct pablo_mmio *pmio, unsigned int reg,
		    struct pmio_field *field, unsigned int *val);
int pmio_field_update_bits_base(struct pablo_mmio *pmio,
				unsigned int reg,
				struct pmio_field *field,
				unsigned int mask, unsigned int val,
				bool force, bool *changed);
void pmio_field_update_to(struct pmio_field *field, unsigned int val,
			  unsigned int orig, unsigned int *update);

static inline int pmio_field_write(struct pablo_mmio *pmio,
				   unsigned int reg,
				   struct pmio_field *field,
				   unsigned int val)
{
	return pmio_field_update_bits_base(pmio, reg, field, ~0, val, false, NULL);
}

static inline int pmio_field_force_write(struct pablo_mmio *pmio,
					 unsigned int reg,
					 struct pmio_field *field,
					 unsigned int val)
{
	return pmio_field_update_bits_base(pmio, reg, field, ~0, val, true, NULL);
}

static inline int pmio_field_update_bits(struct pablo_mmio *pmio,
					 unsigned int reg,
					 struct pmio_field *field,
					 unsigned int mask, unsigned int val)
{
	return pmio_field_update_bits_base(pmio, reg, field, mask, val, false, NULL);
}

static inline int
pmio_field_force_update_bits(struct pablo_mmio *pmio,
			     unsigned int reg,
			     struct pmio_field *field,
			     unsigned int mask, unsigned int val)
{
	return pmio_field_update_bits_base(pmio, reg, field, mask, val, true, NULL);
}

/* FIXME: remove */
void __iomem *pmio_get_base(struct pablo_mmio *pmio);
struct pmio_field *pmio_get_field(struct pablo_mmio *pmio, unsigned int index);

#define PMIO_SET_F(pmio, R, F, val)	pmio_field_force_write(pmio, R, pmio_get_field(pmio, F), val)
#define PMIO_SET_R(pmio, R, val)	pmio_write(pmio, R, val)
void PMIO_SET_F_COREX(struct pablo_mmio *pmio, u32 R, u32 F, u32 val);
u32 PMIO_SET_V(struct pablo_mmio *pmio, u32 reg_val, u32 F, u32 val);
u32 PMIO_GET_F(struct pablo_mmio *pmio, u32 R, u32 F);
u32 PMIO_GET_R(struct pablo_mmio *pmio, u32 R);

#endif /* PABLO_MMIO_H */
