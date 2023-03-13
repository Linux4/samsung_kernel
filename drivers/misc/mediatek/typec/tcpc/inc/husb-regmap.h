/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef LINUX_MISC_HUSB_REGMAP_H
#define LINUX_MISC_HUSB_REGMAP_H

#include <linux/i2c.h>

enum husb_access_mode {
	HUSB_1BYTE_MODE = 1,
	HUSB_2BYTE_MODE = 2,
	HUSB_4BYTE_MODE = 4,
	HUSB_DUMMY_MODE = -1,
};

/* start: the start address of group
 * end: the end address of group
 * mode: access mode (1,2,4 bytes)
 */
struct husb_access_group {
	u32 start;
	u32 end;
	enum husb_access_mode mode;
};

/* husb_reg_type
 * HUSB_NORMAL	: Write without mask, Read from cache
 * HUSB_WBITS	: Write with mask, Read from cache
 * HUSB_VOLATILE	: Write to chip directly, Read from chip
 * HUSB_RESERVE	: Reserve registers (Write/Read as HUSB_NORMAL)
 */
#define HUSB_REG_TYPE_MASK	(0x03)
#define HUSB_NORMAL		(0x00)
#define HUSB_WBITS		(0x01)
#define HUSB_VOLATILE		(0x02)
#define HUSB_RESERVE		(0x03)

/* HUSB_WR_ONCE: if write data = cache data, data will not be written.
 */
#define HUSB_WR_ONCE		(0x08)
#define HUSB_NORMAL_WR_ONCE	(HUSB_NORMAL | HUSB_WR_ONCE)
#define HUSB_WBITS_WR_ONCE	(HUSB_WBITS | HUSB_WR_ONCE)

enum husb_data_format {
	HUSB_LITTLE_ENDIAN,
	HUSB_BIG_ENDIAN,
};

/* husb_regmap_mode
 *  0  0  0  0  0  0  0  0
 *	  |  |  |  |  |  |
 *	  |  |  |  |__|  byte_mode
 *        |  |__|   ||
 *        |   ||   Cache_mode
 *        |   Block_mode
 *        Debug_mode
 */

#define HUSB_BYTE_MODE_MASK	(0x01)
/* 1 byte for each register */
#define HUSB_SINGLE_BYTE		(0 << 0)
/* multi bytes for each regiseter */
#define HUSB_MULTI_BYTE		(1 << 0)

#define HUSB_CACHE_MODE_MASK	 (0x06)
/* write to cache and chip synchronously */
#define HUSB_CACHE_WR_THROUGH	 (0 << 1)
/* write to cache and chip asynchronously */
#define HUSB_CACHE_WR_BACK	 (1 << 1)
/* disable cache */
#define HUSB_CACHE_DISABLE	 (2 << 1)

#define HUSB_IO_BLK_MODE_MASK	(0x18)
/* pass through all write function */
#define HUSB_IO_PASS_THROUGH	(0 << 3)
/* block all write function */
#define HUSB_IO_BLK_ALL		(1 << 3)
/* block cache write function */
#define HUSB_IO_BLK_CACHE		(2 << 3)
/* block chip write function */
#define HUSB_IO_BLK_CHIP		(3 << 3)

#define HUSB_DBG_MODE_MASK	(0x20)
/* create general debugfs for register map */
#define HUSB_DBG_GENERAL		(0 << 5)
/* create node for each regisetr map by register address */
#define HUSB_DBG_SPECIAL		(1 << 5)


/* struct husb_register
 *
 * Ricktek register map structure for storing mapping data
 * @addr: register address.
 * @size: register size in byte.
 * @reg_type: register R/W type (HUSB_NORMAL, HUSB_WBITS, HUSB_VOLATILE, HUSB_RESERVE).
 * @wbit_mask: register writeable bits mask.
 */
struct husb_register {
	u32 addr;
	unsigned int size;
	unsigned char reg_type;
	unsigned char *wbit_mask;
};

/* Declare a husb_register by HUSB_REG_DECL
 */
#define HUSB_REG_DECL(_addr, _reg_length, _reg_type, _mask_...) \
	static unsigned char husb_writable_mask_##_addr[_reg_length] = _mask_; \
	static struct husb_register husb_register_##_addr = { \
		.addr = _addr, \
		.size = _reg_length, \
		.reg_type = _reg_type, \
		.wbit_mask = husb_writable_mask_##_addr, \
	}

#define husb_register_map_t struct husb_register *

#define HUSB_REG(_addr) (&husb_register_##_addr)

/* husb_regmap_properties
 * @name: the name of debug node.
 * @aliases: alisis name of husb_regmap_device.
 * @rm: husb_regiseter_map pointer array.
 * @register_num: the number of husb_register_map registers.
 * @group: register map access group.
 * @husb_format: default is little endian.
 * @husb_regmap_mode: husb_regmap_device mode.
 * @io_log_en: enable/disable io log
 */
struct husb_regmap_properties {
	const char *name;
	const char *aliases;
	const husb_register_map_t *rm;
	int register_num;
	struct husb_access_group *group;
	const enum husb_data_format husb_format;
	unsigned char husb_regmap_mode;
	unsigned char io_log_en:1;
};

/* A passing struct for husb_regmap_reg_read and husb_regmap_reg_write function
 * reg: regmap addr.
 * mask: mask for update bits.
 * husb_data: register value.
 */
struct husb_reg_data {
	u32 reg;
	u32 mask;
	union {
		u32 data_u32;
		u16 data_u16;
		u8 data_u8;
		u8 data[4];
	} husb_data;
};

struct husb_regmap_device;

/* basic chip read/write function */
struct husb_regmap_fops {
	int (*read_device)(void *client, u32 addr, int len, void *dst);
	int (*write_device)(void *client, u32 addr, int len, const void *src);
};

/* with device address */
extern struct husb_regmap_device*
husb_regmap_device_register_ex(struct husb_regmap_properties *props,
			     struct husb_regmap_fops *rops,
			     struct device *parent,
			     void *client, int dev_addr, void *drvdata);

static inline struct husb_regmap_device*
husb_regmap_device_register(struct husb_regmap_properties *props,
			  struct husb_regmap_fops *rops,
			  struct device *parent,
			  struct i2c_client *client, void *drvdata)
{
	return husb_regmap_device_register_ex(props, rops, parent,
					    client, client->addr, drvdata);
}

extern void husb_regmap_device_unregister(struct husb_regmap_device *rd);

extern int husb_regmap_cache_reload(struct husb_regmap_device *rd);

extern int husb_regmap_block_write(struct husb_regmap_device *rd, u32 reg,
				 int bytes, const void *src);
extern int husb_asyn_regmap_block_write(struct husb_regmap_device *rd, u32 reg,
				      int bytes, const void *src);
extern int husb_regmap_block_read(struct husb_regmap_device *rd, u32 reg,
				int bytes, void *dst);
extern int husb_regmap_reg_read(struct husb_regmap_device *rd,
			      struct husb_reg_data *rrd, u32 reg);
extern int husb_regmap_reg_write(struct husb_regmap_device *rd,
			       struct husb_reg_data *rrd, u32 reg,
			       const u32 data);
extern int husb_asyn_regmap_reg_write(struct husb_regmap_device *rd,
				    struct husb_reg_data *rrd, u32 reg,
				    const u32 data);
extern int husb_regmap_update_bits(struct husb_regmap_device *rd,
				 struct husb_reg_data *rrd, u32 reg,
				 u32 mask, u32 data);
extern void husb_regmap_cache_sync(struct husb_regmap_device *rd);
extern void husb_regmap_cache_write_back(struct husb_regmap_device *rd, u32 reg);
extern int husb_is_reg_volatile(struct husb_regmap_device *rd, u32 reg);
extern int husb_get_regsize(struct husb_regmap_device *rd, u32 reg);
extern void husb_cache_getlasterror(struct husb_regmap_device *rd, char *buf,
				  size_t size);
extern void husb_cache_clrlasterror(struct husb_regmap_device *rd);
extern int husb_regmap_add_debugfs(struct husb_regmap_device *rd, const char *name,
				 umode_t mode, void *data,
				 const struct file_operations *fops);
#endif /* LINUX_MISC_HUSB_REGMAP_H */
