/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: cy_huang <cy_huang@etek.com>
 */

#ifndef LINUX_MISC_ET_REGMAP_H
#define LINUX_MISC_ET_REGMAP_H

#include <linux/i2c.h>

enum et_access_mode {
	ET_1BYTE_MODE = 1,
	ET_2BYTE_MODE = 2,
	ET_4BYTE_MODE = 4,
	ET_DUMMY_MODE = -1,
};

/* start: the start address of group
 * end: the end address of group
 * mode: access mode (1,2,4 bytes)
 */
struct et_access_group {
	u32 start;
	u32 end;
	enum et_access_mode mode;
};

/* et_reg_type
 * ET_NORMAL	: Write without mask, Read from cache
 * ET_WBITS	: Write with mask, Read from cache
 * ET_VOLATILE	: Write to chip directly, Read from chip
 * ET_RESERVE	: Reserve registers (Write/Read as ET_NORMAL)
 */
#define ET_REG_TYPE_MASK	(0x03)
#define ET_NORMAL		(0x00)
#define ET_WBITS		(0x01)
#define ET_VOLATILE		(0x02)
#define ET_RESERVE		(0x03)

/* ET_WR_ONCE: if write data = cache data, data will not be written.
 */
#define ET_WR_ONCE		(0x08)
#define ET_NORMAL_WR_ONCE	(ET_NORMAL | ET_WR_ONCE)
#define ET_WBITS_WR_ONCE	(ET_WBITS | ET_WR_ONCE)

enum et_data_format {
	ET_LITTLE_ENDIAN,
	ET_BIG_ENDIAN,
};

/* et_regmap_mode
 *  0  0  0  0  0  0  0  0
 *	  |  |  |  |  |  |
 *	  |  |  |  |__|  byte_mode
 *        |  |__|   ||
 *        |   ||   Cache_mode
 *        |   Block_mode
 *        Debug_mode
 */

#define ET_BYTE_MODE_MASK	(0x01)
/* 1 byte for each register */
#define ET_SINGLE_BYTE		(0 << 0)
/* multi bytes for each regiseter */
#define ET_MULTI_BYTE		(1 << 0)

#define ET_CACHE_MODE_MASK	 (0x06)
/* write to cache and chip synchronously */
#define ET_CACHE_WR_THROUGH	 (0 << 1)
/* write to cache and chip asynchronously */
#define ET_CACHE_WR_BACK	 (1 << 1)
/* disable cache */
#define ET_CACHE_DISABLE	 (2 << 1)

#define ET_IO_BLK_MODE_MASK	(0x18)
/* pass through all write function */
#define ET_IO_PASS_THROUGH	(0 << 3)
/* block all write function */
#define ET_IO_BLK_ALL		(1 << 3)
/* block cache write function */
#define ET_IO_BLK_CACHE		(2 << 3)
/* block chip write function */
#define ET_IO_BLK_CHIP		(3 << 3)

#define ET_DBG_MODE_MASK	(0x20)
/* create general debugfs for register map */
#define ET_DBG_GENERAL		(0 << 5)
/* create node for each regisetr map by register address */
#define ET_DBG_SPECIAL		(1 << 5)


/* struct et_register
 *
 * Ricktek register map structure for storing mapping data
 * @addr: register address.
 * @size: register size in byte.
 * @reg_type: register R/W type (ET_NORMAL, ET_WBITS, ET_VOLATILE, ET_RESERVE).
 * @wbit_mask: register writeable bits mask.
 */
struct et_register {
	u32 addr;
	unsigned int size;
	unsigned char reg_type;
	unsigned char *wbit_mask;
};

/* Declare a et_register by ET_REG_DECL
 */
#define ET_REG_DECL(_addr, _reg_length, _reg_type, _mask_...) \
	static unsigned char et_writable_mask_##_addr[_reg_length] = _mask_; \
	static struct et_register et_register_##_addr = { \
		.addr = _addr, \
		.size = _reg_length, \
		.reg_type = _reg_type, \
		.wbit_mask = et_writable_mask_##_addr, \
	}

#define et_register_map_t struct et_register *

#define ET_REG(_addr) (&et_register_##_addr)

/* et_regmap_properties
 * @name: the name of debug node.
 * @aliases: alisis name of et_regmap_device.
 * @rm: et_regiseter_map pointer array.
 * @register_num: the number of et_register_map registers.
 * @group: register map access group.
 * @et_format: default is little endian.
 * @et_regmap_mode: et_regmap_device mode.
 * @io_log_en: enable/disable io log
 */
struct et_regmap_properties {
	const char *name;
	const char *aliases;
	const et_register_map_t *rm;
	int register_num;
	struct et_access_group *group;
	const enum et_data_format et_format;
	unsigned char et_regmap_mode;
	unsigned char io_log_en:1;
};

/* A passing struct for et_regmap_reg_read and et_regmap_reg_write function
 * reg: regmap addr.
 * mask: mask for update bits.
 * et_data: register value.
 */
struct et_reg_data {
	u32 reg;
	u32 mask;
	union {
		u32 data_u32;
		u16 data_u16;
		u8 data_u8;
		u8 data[4];
	} et_data;
};

struct et_regmap_device;

/* basic chip read/write function */
struct et_regmap_fops {
	int (*read_device)(void *client, u32 addr, int len, void *dst);
	int (*write_device)(void *client, u32 addr, int len, const void *src);
};

/* with device address */
extern struct et_regmap_device*
et_regmap_device_register_ex(struct et_regmap_properties *props,
			     struct et_regmap_fops *rops,
			     struct device *parent,
			     void *client, int dev_addr, void *drvdata);

static inline struct et_regmap_device*
et_regmap_device_register(struct et_regmap_properties *props,
			  struct et_regmap_fops *rops,
			  struct device *parent,
			  struct i2c_client *client, void *drvdata)
{
	return et_regmap_device_register_ex(props, rops, parent,
					    client, client->addr, drvdata);
}

extern void et_regmap_device_unregister(struct et_regmap_device *rd);

extern int et_regmap_cache_reload(struct et_regmap_device *rd);

extern int et_regmap_block_write(struct et_regmap_device *rd, u32 reg,
				 int bytes, const void *src);
extern int et_asyn_regmap_block_write(struct et_regmap_device *rd, u32 reg,
				      int bytes, const void *src);
extern int et_regmap_block_read(struct et_regmap_device *rd, u32 reg,
				int bytes, void *dst);
extern int et_regmap_reg_read(struct et_regmap_device *rd,
			      struct et_reg_data *rrd, u32 reg);
extern int et_regmap_reg_write(struct et_regmap_device *rd,
			       struct et_reg_data *rrd, u32 reg,
			       const u32 data);
extern int et_asyn_regmap_reg_write(struct et_regmap_device *rd,
				    struct et_reg_data *rrd, u32 reg,
				    const u32 data);
extern int et_regmap_update_bits(struct et_regmap_device *rd,
				 struct et_reg_data *rrd, u32 reg,
				 u32 mask, u32 data);
extern void et_regmap_cache_sync(struct et_regmap_device *rd);
extern void et_regmap_cache_write_back(struct et_regmap_device *rd, u32 reg);
extern int et_is_reg_volatile(struct et_regmap_device *rd, u32 reg);
extern int et_get_regsize(struct et_regmap_device *rd, u32 reg);
extern void et_cache_getlasterror(struct et_regmap_device *rd, char *buf,
				  size_t size);
extern void et_cache_clrlasterror(struct et_regmap_device *rd);
extern int et_regmap_add_debugfs(struct et_regmap_device *rd, const char *name,
				 umode_t mode, void *data,
				 const struct file_operations *fops);
#endif /* LINUX_MISC_ET_REGMAP_H */
