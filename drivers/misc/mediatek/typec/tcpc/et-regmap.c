// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/semaphore.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>

#include "inc/et-regmap.h"
#define ET_REGMAP_VERSION	"1.2.1_G"

#define ERR_MSG_SIZE		128
#define MAX_BYTE_SIZE		32

struct et_regmap_ops {
	int (*regmap_block_write)(struct et_regmap_device *rd, u32 reg,
				  int bytes, const void *src);
	int (*regmap_block_read)(struct et_regmap_device *rd, u32 reg,
				 int bytes, void *dst);
};

enum {
	ET_DBG_REG_ADDR,
	ET_DBG_DATA,
	ET_DBG_REGS,
	ET_DBG_SYNC,
	ET_DBG_ERROR,
	ET_DBG_NAME,
	ET_DBG_BLOCK,
	ET_DBG_SIZE,
	ET_DBG_DEVICE_ADDR,
	ET_DBG_SUPPORT_MODE,
	ET_DBG_IO_LOG,
	ET_DBG_CACHE_MODE,
	ET_DBG_REG_SIZE,
	ET_DBG_MAX,
};

struct reg_index_offset {
	int index;
	int offset;
};

#ifdef CONFIG_DEBUG_FS
struct et_debug_data {
	struct reg_index_offset rio;
	unsigned int reg_addr;
	unsigned int reg_size;
};

struct et_debug_st {
	void *info;
	int id;
};
#endif /* CONFIG_DEBUG_FS */

/* et_regmap_device
 *
 * ETEK regmap device. One for each et_regmap.
 *
 */
struct et_regmap_device {
	struct et_regmap_properties props;
	struct et_regmap_fops *rops;
	struct et_regmap_ops regmap_ops;
	struct device dev;
	void *client;
	struct semaphore semaphore;
	struct semaphore write_mode_lock;
	struct delayed_work et_work;
	int dev_addr;
	unsigned char *alloc_data;
	unsigned char **cache_data;
	unsigned long *cached;
	unsigned long *cache_dirty;
	char *err_msg;
	unsigned char error_occurred:1;
	unsigned char regval[MAX_BYTE_SIZE];

	int (*et_block_write[4])(struct et_regmap_device *rd,
				 const struct et_register *rm, int size,
				 const unsigned char *wdata, int count,
				 int cache_idx, int cache_offset);
#ifdef CONFIG_DEBUG_FS
	struct dentry *et_den;
	struct dentry *et_debug_file[ET_DBG_MAX];
	struct et_debug_st rtdbg_st[ET_DBG_MAX];
	struct dentry **et_reg_file;
	struct et_debug_st **reg_st;
	struct et_debug_data dbg_data;
#endif /* CONFIG_DEBUG_FS */
};

static struct reg_index_offset find_register_index(
		const struct et_regmap_device *rd, u32 reg)
{
	int i = 0, j = 0, unit = ET_1BYTE_MODE;
	struct reg_index_offset rio = {-1, -1};
	const et_register_map_t *rm = rd->props.rm;

	for (i = 0; i < rd->props.register_num; i++) {
		if (reg == rm[i]->addr) {
			rio.index = i;
			rio.offset = 0;
			break;
		}
		if (reg > rm[i]->addr &&
		    (reg - rm[i]->addr) < rm[i]->size) {
			rio.index = i;
			for (j = 0; rd->props.group[j].mode != ET_DUMMY_MODE;
									j++) {
				if (reg >= rd->props.group[j].start &&
				    reg <= rd->props.group[j].end) {
					unit = rd->props.group[j].mode;
					break;
				}
			}
			rio.offset = (reg - rm[i]->addr) * unit;
		}
	}
	return rio;
}

static int et_chip_block_write(struct et_regmap_device *rd, u32 reg,
				int bytes, const void *src);

/* et_regmap_cache_sync - sync all cache data to chip */
void et_regmap_cache_sync(struct et_regmap_device *rd)
{
	int ret = 0, i = 0, j = 0;
	const et_register_map_t rm = NULL;

	down(&rd->semaphore);
	for (i = 0; i < rd->props.register_num; i++) {
		if (!rd->cache_dirty[i])
			continue;
		rm = rd->props.rm[i];
		for (j = 0; j < rm->size; j++) {
			if (!test_bit(j, &rd->cache_dirty[i]))
				continue;
			ret = et_chip_block_write(rd, rm->addr + j, 1,
						  rd->cache_data[i] + j);
			if (ret < 0) {
				dev_notice(&rd->dev,
					   "%s block write fail(%d) @ 0x%02x\n",
					   __func__, ret, rm->addr + j);
				goto err_cache_sync;
			}
		}
		rd->cache_dirty[i] = 0;
	}
	dev_info(&rd->dev, "%s successfully\n", __func__);
err_cache_sync:
	up(&rd->semaphore);
}
EXPORT_SYMBOL(et_regmap_cache_sync);

/* et_regmap_cache_write_back - write current cache data to chip
 * @rd: et_regmap_device pointer.
 * @reg: register address
 */
void et_regmap_cache_write_back(struct et_regmap_device *rd, u32 reg)
{
	int ret = 0, j = 0;
	struct reg_index_offset rio = {-1, -1};
	const et_register_map_t rm = NULL;

	rio = find_register_index(rd, reg);
	if (rio.index < 0 || rio.offset != 0) {
		dev_notice(&rd->dev, "%s reg 0x%02x is out of map\n",
				     __func__, reg);
		return;
	}

	down(&rd->semaphore);
	if (!rd->cache_dirty[rio.index])
		goto out;
	rm = rd->props.rm[rio.index];
	for (j = 0; j < rm->size; j++) {
		if (!test_bit(j, &rd->cache_dirty[rio.index]))
			continue;
		ret = et_chip_block_write(rd, rm->addr + j, 1,
					  rd->cache_data[rio.index] + j);
		if (ret < 0) {
			dev_notice(&rd->dev,
				   "%s block write fail(%d) @ 0x%02x\n",
				   __func__, ret, rm->addr + j);
			goto err_cache_write_back;
		}
	}
	rd->cache_dirty[rio.index] = 0;
out:
	dev_info(&rd->dev, "%s successfully\n", __func__);
err_cache_write_back:
	up(&rd->semaphore);
}
EXPORT_SYMBOL(et_regmap_cache_write_back);

/* et_is_reg_volatile - check register map is volatile or not
 * @rd: et_regmap_device pointer.
 * @reg: register address.
 */
int et_is_reg_volatile(struct et_regmap_device *rd, u32 reg)
{
	struct reg_index_offset rio = {-1, -1};
	const et_register_map_t rm = NULL;

	rio = find_register_index(rd, reg);
	if (rio.index < 0) {
		dev_notice(&rd->dev, "%s reg 0x%02x is out of range\n",
				     __func__, reg);
		return -EINVAL;
	}
	rm = rd->props.rm[rio.index];

	return (rm->reg_type & ET_REG_TYPE_MASK) == ET_VOLATILE ? 1 : 0;
}
EXPORT_SYMBOL(et_is_reg_volatile);

/* et_reg_regsize - get register map size for specific register
 * @rd: et_regmap_device pointer.
 * @reg: register address
 */
int et_get_regsize(struct et_regmap_device *rd, u32 reg)
{
	struct reg_index_offset rio = {-1, -1};

	rio = find_register_index(rd, reg);
	if (rio.index < 0 || rio.offset != 0) {
		dev_notice(&rd->dev, "%s reg 0x%02x is out of map\n",
				     __func__, reg);
		return -EINVAL;
	}
	return rd->props.rm[rio.index]->size;
}
EXPORT_SYMBOL(et_get_regsize);

static void et_work_func(struct work_struct *work)
{
	struct et_regmap_device *rd =
		container_of(work, struct et_regmap_device, et_work.work);

	dev_info(&rd->dev, "%s\n", __func__);
	et_regmap_cache_sync(rd);
}

static int et_chip_block_write(struct et_regmap_device *rd, u32 reg,
			       int bytes, const void *src)
{
	if ((rd->props.et_regmap_mode & ET_IO_BLK_MODE_MASK) == ET_IO_BLK_ALL ||
	    (rd->props.et_regmap_mode & ET_IO_BLK_MODE_MASK) == ET_IO_BLK_CHIP)
		return -EPERM;

	return rd->rops->write_device(rd->client, reg, bytes, src);
}

static int et_chip_block_read(struct et_regmap_device *rd, u32 reg,
			      int bytes, void *dst)
{
	return rd->rops->read_device(rd->client, reg, bytes, dst);
}

static int et_block_write(struct et_regmap_device *rd,
			  const struct et_register *rm, int size,
			  const unsigned char *wdata, int count,
			  int cache_idx, int cache_offset)
{
	int ret = 0, j = 0, change = 0;

	down(&rd->write_mode_lock);
	for (j = cache_offset; j < cache_offset + size; j++, count++) {
		ret = test_and_set_bit(j, &rd->cached[cache_idx]);
		if (ret && (rm->reg_type & ET_WR_ONCE)) {
			if (rd->cache_data[cache_idx][j] ==
				(wdata[count] & rm->wbit_mask[j]))
				continue;
		}
		rd->cache_data[cache_idx][j] = wdata[count] & rm->wbit_mask[j];
		change++;
	}

	if (!change)
		goto out;

	ret = et_chip_block_write(rd, rm->addr + cache_offset, size,
				  rd->cache_data[cache_idx] + cache_offset);
	if (ret < 0)
		dev_notice(&rd->dev, "%s block write fail(%d) @ 0x%02x\n",
				     __func__, ret, rm->addr + cache_offset);
out:
	up(&rd->write_mode_lock);
	return ret < 0 ? ret : 0;
}

static int et_block_write_blk_all(struct et_regmap_device *rd,
				  const struct et_register *rm, int size,
				  const unsigned char *wdata, int count,
				  int cache_idx, int cache_offset)
{
	return 0;
}

static int et_block_write_blk_cache(struct et_regmap_device *rd,
				    const struct et_register *rm, int size,
				    const unsigned char *wdata, int count,
				    int cache_idx, int cache_offset)
{
	int ret = 0;

	down(&rd->write_mode_lock);
	ret = et_chip_block_write(rd, rm->addr + cache_offset, size,
				  &wdata[count]);
	up(&rd->write_mode_lock);
	if (ret < 0)
		dev_notice(&rd->dev, "%s block write fail(%d) @ 0x%02x\n",
				     __func__, ret, rm->addr + cache_offset);
	return ret < 0 ? ret : 0;
}

static int et_block_write_blk_chip(struct et_regmap_device *rd,
				   const struct et_register *rm, int size,
				   const unsigned char *wdata, int count,
				   int cache_idx, int cache_offset)
{
	int ret = 0, j = 0;

	down(&rd->write_mode_lock);
	for (j = cache_offset; j < cache_offset + size; j++, count++) {
		ret = test_and_set_bit(j, &rd->cached[cache_idx]);
		if (ret && (rm->reg_type & ET_WR_ONCE)) {
			if (rd->cache_data[cache_idx][j] ==
				(wdata[count] & rm->wbit_mask[j]))
				continue;
		}
		rd->cache_data[cache_idx][j] = wdata[count] & rm->wbit_mask[j];
		set_bit(j, &rd->cache_dirty[cache_idx]);
	}
	up(&rd->write_mode_lock);
	return 0;
}

static int (*et_block_map[])(struct et_regmap_device *rd,
			     const struct et_register *rm, int size,
			     const unsigned char *wdata, int count,
			     int cache_idx, int cache_offset) = {
	&et_block_write,
	&et_block_write_blk_all,
	&et_block_write_blk_cache,
	&et_block_write_blk_chip,
};

static int _et_cache_block_write(struct et_regmap_device *rd, u32 reg,
				 int bytes, const void *src, bool asyn)
{
	int ret = 0, i = 0, j = 0, count = 0, size = 0;
	struct reg_index_offset rio = {-1, -1};
	const et_register_map_t rm = NULL;
	const unsigned char *wdata = src;
	unsigned char wri_data[128], blk_index = 0;

	rio = find_register_index(rd, reg);
	if (rio.index < 0) {
		dev_notice(&rd->dev, "%s reg 0x%02x is out of range\n",
				     __func__, reg);
		return -EINVAL;
	}

	for (i = rio.index, j = rio.offset, count = 0;
		i < rd->props.register_num && count < bytes;
		i++, j = 0, count += size) {
		rm = rd->props.rm[i];
		size = (bytes - count) <= (rm->size - j) ?
			(bytes - count) : (rm->size - j);
		if ((rm->reg_type & ET_REG_TYPE_MASK) == ET_VOLATILE) {
			ret = et_chip_block_write(rd, rm->addr + j, size,
						  &wdata[count]);
		} else if (asyn) {
			ret = rd->props.et_regmap_mode & ET_IO_BLK_MODE_MASK;
			if (ret == ET_IO_BLK_ALL || ret == ET_IO_BLK_CACHE) {
				dev_notice(&rd->dev, "%s ret = %d\n",
						     __func__, ret);
				ret = -EPERM;
				goto err_cache_block_write;
			}

			ret = et_block_write_blk_chip
				(rd, rm, size, wdata, count, i, j);
		} else {
			blk_index = (rd->props.et_regmap_mode &
				     ET_IO_BLK_MODE_MASK) >> 3;

			ret = rd->et_block_write[blk_index]
				(rd, rm, size, wdata, count, i, j);
		}
		if (ret < 0) {
			dev_notice(&rd->dev,
				   "%s block write fail(%d) @ 0x%02x\n",
				   __func__, ret, rm->addr + j);
			goto err_cache_block_write;
		}
	}
	if (rd->props.io_log_en) {
		j = 0;
		for (i = 0; i < count; i++) {
			ret = snprintf(wri_data + j, sizeof(wri_data) - j,
				       "%02x,", wdata[i]);
			if ((ret < 0) || (ret >= sizeof(wri_data) - j))
				return -EINVAL;

			j += ret;
		}
		dev_info(&rd->dev, "ET_REGMAP [WRITE] reg0x%02x  [Data] %s\n",
				   reg, wri_data);
	}
	return 0;
err_cache_block_write:
	return ret;
}

static int et_cache_block_write(struct et_regmap_device *rd, u32 reg,
				int bytes, const void *src)
{
	return _et_cache_block_write(rd, reg, bytes, src, false);
}

static int et_asyn_cache_block_write(struct et_regmap_device *rd, u32 reg,
				     int bytes, const void *src)
{
	int ret = 0;

	ret = _et_cache_block_write(rd, reg, bytes, src, true);
	if (ret >= 0)
		mod_delayed_work(system_wq, &rd->et_work, msecs_to_jiffies(1));
	return ret;
}

static int et_cache_block_read(struct et_regmap_device *rd, u32 reg,
			int bytes, void *dest)
{
	int ret = 0, i = 0, j = 0, count = 0, total_bytes = 0;
	struct reg_index_offset rio = {-1, -1};
	const et_register_map_t rm = NULL;
	unsigned long mask = 0;

	rio = find_register_index(rd, reg);
	if (rio.index < 0) {
		dev_notice(&rd->dev, "%s reg 0x%02x is out of range\n",
				     __func__, reg);
		return -EINVAL;
	}

	rm = rd->props.rm[rio.index];

	total_bytes += (rm->size - rio.offset);

	for (i = rio.index + 1; i < rd->props.register_num; i++)
		total_bytes += rd->props.rm[i]->size;

	if (bytes > total_bytes) {
		dev_notice(&rd->dev, "%s bytes %d is out of range\n",
				     __func__, bytes);
		return -EINVAL;
	}

	for (i = rio.index, j = rio.offset, count = 0;
		i < rd->props.register_num && count < bytes; i++, j = 0) {
		rm = rd->props.rm[i];
		mask = GENMASK(rm->size - 1, j);
		if ((rd->cached[i] & mask) == mask) {
			count += rm->size - j;
			continue;
		}
		ret = rd->rops->read_device(rd->client, rm->addr,
					    rm->size, rd->regval);
		if (ret < 0) {
			dev_notice(&rd->dev,
				   "%s read device fail(%d) @ 0x%02x\n",
				   __func__, ret, rm->addr);
			return ret;
		}
		for (; j < rm->size && count < bytes; j++, count++) {
			if (test_bit(j, &rd->cached[i]))
				continue;
			rd->cache_data[i][j] = rd->regval[j];
			if ((rm->reg_type & ET_REG_TYPE_MASK) != ET_VOLATILE)
				set_bit(j, &rd->cached[i]);
		}
	}

	if (rd->props.io_log_en)
		dev_info(&rd->dev, "ET_REGMAP [READ] reg0x%02x\n", reg);

	memcpy(dest, &rd->cache_data[rio.index][rio.offset], bytes);

	return 0;
}

static u32 cpu_to_chip(struct et_regmap_device *rd, u32 cpu_data, int size)
{
	u32 chip_data = 0;

	if (rd->props.et_format == ET_BIG_ENDIAN) {
#ifdef CONFIG_CPU_BIG_ENDIAN
		chip_data = cpu_data << (4 - size) * 8;
#else
		chip_data = cpu_to_be32(cpu_data);
		chip_data >>= (4 - size) * 8;
#endif /* CONFIG_CPU_BIG_ENDIAN */
	} else {
		chip_data = cpu_to_le32(cpu_data);
	}

	return chip_data;
}

static u32 chip_to_cpu(struct et_regmap_device *rd, u32 chip_data, int size)
{
	u32 cpu_data = 0;

	if (rd->props.et_format == ET_BIG_ENDIAN) {
#ifdef CONFIG_CPU_BIG_ENDIAN
		cpu_data = chip_data >> ((4 - size) * 8);
#else
		cpu_data = be32_to_cpu(chip_data);
		cpu_data >>= (4 - size) * 8;
#endif /* CONFIG_CPU_BIG_ENDIAN */
	} else {
		cpu_data = le32_to_cpu(chip_data);
	}

	return cpu_data;
}

/* _et_regmap_reg_write - write data to specific register map
 * only support 1, 2, 4 bytes regisetr map
 * @rd: et_regmap_device pointer.
 * @rrd: et_reg_data pointer.
 */
static int _et_regmap_reg_write(struct et_regmap_device *rd,
				struct et_reg_data *rrd, bool asyn)
{
	int ret = -ENOTSUPP, size = 0;
	struct reg_index_offset rio = {-1, -1};
	const et_register_map_t *rm = rd->props.rm;
	u32 tmp_data = 0;

	rio = find_register_index(rd, rrd->reg);
	if (rio.index < 0 || rio.offset != 0) {
		dev_notice(&rd->dev, "%s reg 0x%02x is out of map\n",
				     __func__, rrd->reg);
		return -EINVAL;
	}

	size = rm[rio.index]->size;
	if (size < 1 || size > 4) {
		dev_notice(&rd->dev, "%s only support 1~4 bytes(%d)\n",
				     __func__, size);
		return -EINVAL;
	}

	tmp_data = cpu_to_chip(rd, rrd->et_data.data_u32, size);

	down(&rd->semaphore);
	ret = (asyn ? et_asyn_cache_block_write :
		rd->regmap_ops.regmap_block_write)
		(rd, rrd->reg, size, &tmp_data);
	up(&rd->semaphore);
	if (ret < 0)
		dev_notice(&rd->dev, "%s block write fail(%d) @ 0x%02x\n",
				     __func__, ret, rrd->reg);
	return (ret < 0) ? ret : 0;
}

int et_regmap_reg_write(struct et_regmap_device *rd, struct et_reg_data *rrd,
			u32 reg, const u32 data)
{
	rrd->reg = reg;
	rrd->et_data.data_u32 = data;
	return _et_regmap_reg_write(rd, rrd, false);
}
EXPORT_SYMBOL(et_regmap_reg_write);

int et_asyn_regmap_reg_write(struct et_regmap_device *rd,
			     struct et_reg_data *rrd, u32 reg, const u32 data)
{
	rrd->reg = reg;
	rrd->et_data.data_u32 = data;
	return _et_regmap_reg_write(rd, rrd, true);
}
EXPORT_SYMBOL(et_asyn_regmap_reg_write);

/* _et_regmap_reg_read - register read for specific register map
 * only support 1, 2, 4 bytes register map.
 * @rd: et_regmap_device pointer.
 * @rrd: et_reg_data pointer.
 */
static int _et_regmap_reg_read(struct et_regmap_device *rd,
			       struct et_reg_data *rrd)
{
	int ret = -ENOTSUPP, size = 0;
	struct reg_index_offset rio = {-1, -1};
	const et_register_map_t *rm = rd->props.rm;
	u32 data = 0;

	rio = find_register_index(rd, rrd->reg);
	if (rio.index < 0 || rio.offset != 0) {
		dev_notice(&rd->dev, "%s reg 0x%02x is out of map\n",
				     __func__, rrd->reg);
		return -EINVAL;
	}

	size = rm[rio.index]->size;
	if (size < 1 || size > 4) {
		dev_notice(&rd->dev, "%s only support 1~4 bytes(%d)\n",
				     __func__, size);
		return -EINVAL;
	}

	down(&rd->semaphore);
	ret = rd->regmap_ops.regmap_block_read(rd, rrd->reg, size, &data);
	up(&rd->semaphore);
	if (ret < 0) {
		dev_notice(&rd->dev, "%s block read fail(%d) @ 0x%02x\n",
				     __func__, ret, rrd->reg);
		goto out;
	}
	rrd->et_data.data_u32 = chip_to_cpu(rd, data, size);
out:
	return (ret < 0) ? ret : 0;
}

int et_regmap_reg_read(struct et_regmap_device *rd,
			struct et_reg_data *rrd, u32 reg)
{
	rrd->reg = reg;
	return _et_regmap_reg_read(rd, rrd);
}
EXPORT_SYMBOL(et_regmap_reg_read);

/* _et_regmap_update_bits - assign bits specific register map */
static int _et_regmap_update_bits(struct et_regmap_device *rd,
				  struct et_reg_data *rrd)
{

	int ret = -ENOTSUPP, size = 0;
	struct reg_index_offset rio = {-1, -1};
	const et_register_map_t *rm = rd->props.rm;
	u32 new = 0, old = 0;
	bool change = false;

	rio = find_register_index(rd, rrd->reg);
	if (rio.index < 0 || rio.offset != 0) {
		dev_notice(&rd->dev, "%s reg 0x%02x is out of map\n",
				     __func__, rrd->reg);
		return -EINVAL;
	}

	size = rm[rio.index]->size;
	if (size < 1 || size > 4) {
		dev_notice(&rd->dev, "%s only support 1~4 bytes(%d)\n",
				     __func__, size);
		return -EINVAL;
	}

	down(&rd->semaphore);
	ret = rd->regmap_ops.regmap_block_read(rd, rrd->reg, size, &old);
	if (ret < 0) {
		dev_notice(&rd->dev, "%s block read fail(%d) @ 0x%02x\n",
				     __func__, ret, rrd->reg);
		goto out;
	}

	old = chip_to_cpu(rd, old, size);

	new = (old & ~(rrd->mask)) | (rrd->et_data.data_u32 & rrd->mask);
	change = old != new;
	if ((rm[rio.index]->reg_type & ET_WR_ONCE) && !change)
		goto out;

	new = cpu_to_chip(rd, new, size);

	ret = rd->regmap_ops.regmap_block_write
		(rd, rrd->reg, size, &new);
	if (ret < 0)
		dev_notice(&rd->dev, "%s block write fail(%d) @ 0x%02x\n",
				     __func__, ret, rrd->reg);
out:
	up(&rd->semaphore);
	return (ret < 0) ? ret : 0;
}

int et_regmap_update_bits(struct et_regmap_device *rd, struct et_reg_data *rrd,
			  u32 reg, u32 mask, u32 data)
{
	rrd->reg = reg;
	rrd->mask = mask;
	rrd->et_data.data_u32 = data;
	return _et_regmap_update_bits(rd, rrd);
}
EXPORT_SYMBOL(et_regmap_update_bits);

/* et_regmap_block_write - block write data to register
 * @rd: et_regmap_device pointer
 * @reg: register address
 * @bytes: length for write
 * @src: source of write data
 */
int et_regmap_block_write(struct et_regmap_device *rd, u32 reg,
			  int bytes, const void *src)
{
	int ret = 0;

	down(&rd->semaphore);
	ret = rd->regmap_ops.regmap_block_write(rd, reg, bytes, src);
	up(&rd->semaphore);
	return (ret < 0) ? ret : 0;
}
EXPORT_SYMBOL(et_regmap_block_write);

/* et_asyn_regmap_block_write - asyn block write */
int et_asyn_regmap_block_write(struct et_regmap_device *rd, u32 reg,
			       int bytes, const void *src)
{
	int ret = 0;

	down(&rd->semaphore);
	ret = et_asyn_cache_block_write(rd, reg, bytes, src);
	up(&rd->semaphore);
	return (ret < 0) ? ret : 0;
}
EXPORT_SYMBOL(et_asyn_regmap_block_write);

/* et_regmap_block_read - block read data form register
 * @rd: et_regmap_device pointer
 * @reg: register address
 * @bytes: length for read
 * @dst: destination for read data
 */
int et_regmap_block_read(struct et_regmap_device *rd, u32 reg,
			 int bytes, void *dst)
{
	int ret = 0;

	down(&rd->semaphore);
	ret = rd->regmap_ops.regmap_block_read(rd, reg, bytes, dst);
	up(&rd->semaphore);
	return (ret < 0) ? ret : 0;
}
EXPORT_SYMBOL(et_regmap_block_read);

void et_cache_getlasterror(struct et_regmap_device *rd, char *buf, size_t size)
{
	int ret = 0;

	down(&rd->semaphore);
	ret = snprintf(buf, size, "%s", rd->err_msg);
	up(&rd->semaphore);
	if ((ret < 0) || (ret >= size))
		dev_notice(&rd->dev, "%s snprintf fail(%d)\n", __func__, ret);
}
EXPORT_SYMBOL(et_cache_getlasterror);

void et_cache_clrlasterror(struct et_regmap_device *rd)
{
	int ret = 0;

	down(&rd->semaphore);
	rd->error_occurred = 0;
	rd->err_msg[0] = 0;
	up(&rd->semaphore);
	if ((ret < 0) || (ret >= ERR_MSG_SIZE))
		dev_notice(&rd->dev, "%s snprintf fail(%d)\n", __func__, ret);
}
EXPORT_SYMBOL(et_cache_clrlasterror);

/* initialize cache data from et_register */
static int et_regmap_cache_init(struct et_regmap_device *rd)
{
	int ret = 0, i = 0, j = 0, count = 0, bytes_num = 0;
	const et_register_map_t *rm = rd->props.rm;

	pr_info("%s\n", __func__);

	down(&rd->semaphore);
	rd->cache_data = devm_kzalloc(&rd->dev, rd->props.register_num *
			sizeof(*rd->cache_data), GFP_KERNEL);
	rd->cached = devm_kzalloc(&rd->dev, rd->props.register_num *
			sizeof(*rd->cached), GFP_KERNEL);
	rd->cache_dirty = devm_kzalloc(&rd->dev, rd->props.register_num *
			sizeof(*rd->cache_dirty), GFP_KERNEL);

	if (!rd->cache_data || !rd->cached || !rd->cache_dirty) {
		ret = -ENOMEM;
		goto out;
	}

	if (rd->props.group == NULL) {
		rd->props.group = devm_kzalloc(&rd->dev,
					       sizeof(*rd->props.group) * 2,
					       GFP_KERNEL);
		if (!rd->props.group) {
			ret = -ENOMEM;
			goto out;
		}
		rd->props.group[0].start = 0;
		rd->props.group[0].end = U32_MAX;
		rd->props.group[0].mode = ET_1BYTE_MODE;
		rd->props.group[1].mode = ET_DUMMY_MODE;
	}

	for (i = 0; i < rd->props.register_num; i++)
		bytes_num += rm[i]->size;

	rd->alloc_data = devm_kzalloc(&rd->dev, bytes_num, GFP_KERNEL);
	if (!rd->alloc_data) {
		ret = -ENOMEM;
		goto out;
	}

	for (i = 0; i < rd->props.register_num; i++) {
		rd->cache_data[i] = rd->alloc_data + count;
		count += rm[i]->size;
	}

	/* set 0xff writeable mask for NORMAL and RESERVE type */
	for (i = 0; i < rd->props.register_num; i++) {
		if ((rm[i]->reg_type & ET_REG_TYPE_MASK) == ET_NORMAL ||
		    (rm[i]->reg_type & ET_REG_TYPE_MASK) == ET_RESERVE) {
			for (j = 0; j < rm[i]->size; j++)
				rm[i]->wbit_mask[j] = 0xff;
		}
	}

	pr_info("%s successfully\n", __func__);
out:
	up(&rd->semaphore);
	return ret;
}

/* et_regmap_cache_reload - reload cache from chip */
int et_regmap_cache_reload(struct et_regmap_device *rd)
{
	int i = 0;

	dev_info(&rd->dev, "%s\n", __func__);
	down(&rd->semaphore);
	for (i = 0; i < rd->props.register_num; i++)
		rd->cache_dirty[i] = rd->cached[i] = 0;
	up(&rd->semaphore);
	return 0;
}
EXPORT_SYMBOL(et_regmap_cache_reload);

/* et_regmap_add_debubfs - add user own debugfs node
 * @rd: et_regmap_devcie pointer.
 * @name: a pointer to a string containing the name of the file to create.
 * @mode: the permission that the file should have.
 * @data: a pointer to something that the caller will want to get to later
 *        on.  The inode.i_private pointer will point to this value on
 *        the open() call.
 * @fops: a pointer to a struct file_operations that should be used for
 *        this file.
 */
int et_regmap_add_debugfs(struct et_regmap_device *rd, const char *name,
			  umode_t mode, void *data,
			  const struct file_operations *fops)
{
#ifdef CONFIG_DEBUG_FS
	struct dentry *den = NULL;

	den = debugfs_create_file(name, mode, rd->et_den, data, fops);
	if (!den)
		return -EINVAL;
#endif /* CONFIG_DEBUG_FS */
	return 0;
}
EXPORT_SYMBOL(et_regmap_add_debugfs);

static void et_regmap_set_cache_mode(struct et_regmap_device *rd,
				     unsigned char mode)
{
	unsigned char mode_mask = mode & ET_CACHE_MODE_MASK;

	dev_info(&rd->dev, "%s mode_mask = %d\n", __func__, mode_mask);

	down(&rd->write_mode_lock);
	if (mode_mask == ET_CACHE_WR_THROUGH) {
		et_regmap_cache_reload(rd);
		rd->regmap_ops.regmap_block_write = et_cache_block_write;
		rd->regmap_ops.regmap_block_read = et_cache_block_read;
	} else if (mode_mask == ET_CACHE_WR_BACK) {
		et_regmap_cache_reload(rd);
		rd->regmap_ops.regmap_block_write = et_asyn_cache_block_write;
		rd->regmap_ops.regmap_block_read = et_cache_block_read;
	} else if (mode_mask == ET_CACHE_DISABLE) {
		rd->regmap_ops.regmap_block_write = et_chip_block_write;
		rd->regmap_ops.regmap_block_read = et_chip_block_read;
	} else {
		dev_notice(&rd->dev, "%s invalid cache mode\n", __func__);
		goto err_mode;
	}

	rd->props.et_regmap_mode &= ~ET_CACHE_MODE_MASK;
	rd->props.et_regmap_mode |= mode_mask;
err_mode:
	up(&rd->write_mode_lock);
}

#ifdef CONFIG_DEBUG_FS
struct dentry *et_regmap_dir;

#define erro_printf(rd, fmt, ...)					\
do {									\
	int ret = 0;							\
	size_t len = 0;							\
									\
	dev_notice(&rd->dev, fmt, ##__VA_ARGS__);			\
	down(&rd->semaphore);						\
	len = strlen(rd->err_msg);					\
	ret = snprintf(rd->err_msg + len, ERR_MSG_SIZE - len,		\
		       fmt, ##__VA_ARGS__);				\
	rd->error_occurred = 1;						\
	up(&rd->semaphore);						\
	if ((ret < 0) || (ret >= ERR_MSG_SIZE - len))			\
		dev_notice(&rd->dev, "%s snprintf fail(%d)\n",		\
				     __func__, ret);			\
} while (0)

static int get_parameters(char *buf, unsigned long *param, int num_of_par)
{
	int cnt = 0;
	char *token = strsep(&buf, " ");

	for (cnt = 0; cnt < num_of_par; cnt++) {
		if (token) {
			if (kstrtoul(token, 0, &param[cnt]) != 0)
				return -EINVAL;

			token = strsep(&buf, " ");
		} else
			return -EINVAL;
	}

	return 0;
}

static int get_data(const char *buf, size_t count,
		    unsigned char *data_buffer, unsigned int data_length)
{
	int i = 0, ptr = 0;
	u8 value = 0;
	char token[5] = {0};

	token[0] = '0';
	token[1] = 'x';
	token[4] = 0;
	if (buf[0] != '0' || buf[1] != 'x')
		return -EINVAL;

	ptr = 2;
	for (i = 0; (i < data_length) && (ptr + 2 <= count); i++) {
		token[2] = buf[ptr++];
		token[3] = buf[ptr++];
		ptr++;
		if (kstrtou8(token, 16, &value) != 0)
			return -EINVAL;
		data_buffer[i] = value;
	}
	return 0;
}

static void et_show_regs(struct et_regmap_device *rd, struct seq_file *seq_file)
{
	int ret = 0, i = 0, j = 0;
	const et_register_map_t *rm = rd->props.rm;

	for (i = 0; i < rd->props.register_num; i++) {
		down(&rd->semaphore);
		ret = rd->regmap_ops.regmap_block_read(rd, rm[i]->addr,
						       rm[i]->size, rd->regval);
		up(&rd->semaphore);
		if (ret < 0) {
			erro_printf(rd, "%s block read fail(%d) @ 0x%02x\n",
					__func__, ret, rm[i]->addr);
			break;
		}

		if ((rm[i]->reg_type & ET_REG_TYPE_MASK) != ET_RESERVE) {
			seq_printf(seq_file, "reg0x%02x:0x", rm[i]->addr);
			for (j = 0; j < rm[i]->size; j++)
				seq_printf(seq_file, "%02x,", rd->regval[j]);
			seq_puts(seq_file, "\n");
		} else
			seq_printf(seq_file,
				   "reg0x%02x:reserve\n", rm[i]->addr);
	}
}

static int general_read(struct seq_file *seq_file, void *_data)
{
	int ret = 0, i = 0, size = 0;
	struct et_debug_st *st = seq_file->private;
	struct et_regmap_device *rd = st->info;
	unsigned char data = 0;

	switch (st->id) {
	case ET_DBG_REG_ADDR:
		seq_printf(seq_file, "0x%02x\n", rd->dbg_data.reg_addr);
		break;
	case ET_DBG_DATA:
		if (rd->dbg_data.reg_size == 0)
			rd->dbg_data.reg_size = 1;
		size = rd->dbg_data.reg_size;

		down(&rd->semaphore);
		if (rd->dbg_data.rio.index < 0)
			ret = et_chip_block_read(rd, rd->dbg_data.reg_addr,
						 size, rd->regval);
		else
			ret = rd->regmap_ops.regmap_block_read(rd,
					rd->dbg_data.reg_addr,
					size, rd->regval);
		up(&rd->semaphore);
		if (ret < 0) {
			erro_printf(rd, "%s block read fail(%d) @ 0x%02x\n",
					__func__, ret, rd->dbg_data.reg_addr);
			break;
		}

		seq_puts(seq_file, "0x");
		for (i = 0; i < size; i++)
			seq_printf(seq_file, "%02x,", rd->regval[i]);
		seq_puts(seq_file, "\n");
		break;
	case ET_DBG_ERROR:
		seq_puts(seq_file, "======== Error Message ========\n");
		seq_puts(seq_file, rd->error_occurred ? rd->err_msg :
				   "No Error\n");
		break;
	case ET_DBG_REGS:
		et_show_regs(rd, seq_file);
		break;
	case ET_DBG_NAME:
		seq_printf(seq_file, "%s\n", rd->props.aliases);
		break;
	case ET_DBG_SIZE:
		seq_printf(seq_file, "%u\n", rd->dbg_data.reg_size);
		break;
	case ET_DBG_BLOCK:
		data = rd->props.et_regmap_mode & ET_IO_BLK_MODE_MASK;
		if (data == ET_IO_PASS_THROUGH)
			seq_puts(seq_file, "0 => IO_PASS_THROUGH\n");
		else if (data == ET_IO_BLK_ALL)
			seq_puts(seq_file, "1 => IO_BLK_ALL\n");
		else if (data == ET_IO_BLK_CACHE)
			seq_puts(seq_file, "2 => IO_BLK_CACHE\n");
		else if (data == ET_IO_BLK_CHIP)
			seq_puts(seq_file, "3 => IO_BLK_CHIP\n");
		break;
	case ET_DBG_DEVICE_ADDR:
		seq_printf(seq_file, "0x%02x\n", rd->dev_addr);
		break;
	case ET_DBG_SUPPORT_MODE:
		seq_puts(seq_file, " == BLOCK MODE ==\n");
		seq_puts(seq_file, "0 => IO_PASS_THROUGH\n");
		seq_puts(seq_file, "1 => IO_BLK_ALL\n");
		seq_puts(seq_file, "2 => IO_BLK_CACHE\n");
		seq_puts(seq_file, "3 => IO_BLK_CHIP\n");
		seq_puts(seq_file, " == CACHE MODE ==\n");
		seq_puts(seq_file, "0 => CACHE_WR_THROUGH\n");
		seq_puts(seq_file, "1 => CACHE_WR_BACK\n");
		seq_puts(seq_file, "2 => CACHE_DISABLE\n");
		break;
	case ET_DBG_IO_LOG:
		seq_printf(seq_file, "%d\n", rd->props.io_log_en);
		break;
	case ET_DBG_CACHE_MODE:
		data = rd->props.et_regmap_mode & ET_CACHE_MODE_MASK;
		if (data == ET_CACHE_WR_THROUGH)
			seq_puts(seq_file, "0 => CACHE_WR_THROUGH\n");
		else if (data == ET_CACHE_WR_BACK)
			seq_puts(seq_file, "1 => CACHE_WR_BACK\n");
		else if (data == ET_CACHE_DISABLE)
			seq_puts(seq_file, "2 => CACHE_DISABLE\n");
		break;
	case ET_DBG_REG_SIZE:
		size = et_get_regsize(rd, rd->dbg_data.reg_addr);
		seq_printf(seq_file, "%d\n", size);
		break;
	}
	return 0;
}

static int general_open(struct inode *inode, struct file *file)
{
	return single_open(file, general_read, inode->i_private);
}

static ssize_t general_write(struct file *file, const char __user *ubuf,
			     size_t count, loff_t *ppos)
{
	int ret = 0;
	struct et_debug_st *st =
		((struct seq_file *)file->private_data)->private;
	struct et_regmap_device *rd = st->info;
	struct reg_index_offset rio = {-1, -1};
	char lbuf[128];
	ssize_t res = 0;
	unsigned int size = 0;
	unsigned long param = 0;

	dev_info(&rd->dev, "%s @ %p, count = %u, pos = %llu\n",
			   __func__, ubuf, (unsigned int)count, *ppos);
	*ppos = 0;
	res = simple_write_to_buffer(lbuf, sizeof(lbuf) - 1, ppos, ubuf, count);
	if (res <= 0)
		return -EFAULT;
	count = res;
	lbuf[count] = '\0';

	switch (st->id) {
	case ET_DBG_REG_ADDR:
		ret = get_parameters(lbuf, &param, 1);
		if (ret < 0)
			return ret;
		rio = find_register_index(rd, param);
		down(&rd->semaphore);
		rd->dbg_data.rio = rio;
		rd->dbg_data.reg_addr = param;
		up(&rd->semaphore);
		if (rio.index < 0)
			erro_printf(rd, "%s reg 0x%02lx is out of range\n",
					__func__, param);
		break;
	case ET_DBG_DATA:
		if (rd->dbg_data.reg_size == 0)
			rd->dbg_data.reg_size = 1;
		size = rd->dbg_data.reg_size;

		if ((size - 1) * 3 + 5 != count) {
			erro_printf(rd,
			"%s wrong input length, size = %u, count = %u\n",
				    __func__, size, (unsigned int)count);
			return -EINVAL;
		}

		memset(rd->regval, 0, sizeof(rd->regval));
		ret = get_data(lbuf, count, rd->regval, size);
		if (ret < 0) {
			erro_printf(rd, "%s get data fail(%d)\n",
					__func__, ret);
			return ret;
		}

		down(&rd->semaphore);
		if (rd->dbg_data.rio.index < 0)
			ret = et_chip_block_write(rd, rd->dbg_data.reg_addr,
						  size, rd->regval);
		else
			ret = rd->regmap_ops.regmap_block_write(rd,
					rd->dbg_data.reg_addr,
					size, rd->regval);
		up(&rd->semaphore);
		if (ret < 0) {
			erro_printf(rd, "%s block write fail(%d) @ 0x%02x\n",
					__func__, ret, rd->dbg_data.reg_addr);
			return ret;
		}
		break;
	case ET_DBG_SYNC:
		ret = get_parameters(lbuf, &param, 1);
		if (ret < 0)
			return ret;
		if (param)
			et_regmap_cache_sync(rd);
		break;
	case ET_DBG_ERROR:
		ret = get_parameters(lbuf, &param, 1);
		if (ret < 0)
			return ret;
		if (param)
			et_cache_clrlasterror(rd);
		break;
	case ET_DBG_SIZE:
		ret = get_parameters(lbuf, &param, 1);
		if (ret < 0)
			return ret;
		if (param >= 1 && param <= MAX_BYTE_SIZE) {
			down(&rd->semaphore);
			rd->dbg_data.reg_size = param;
			up(&rd->semaphore);
		} else {
			erro_printf(rd, "%s size(%lu) must be %d ~ %d\n",
					__func__, param, 1, MAX_BYTE_SIZE);
			return -EINVAL;
		}
		break;
	case ET_DBG_BLOCK:
		ret = get_parameters(lbuf, &param, 1);
		if (ret < 0)
			return ret;
		if (param > 3)
			param = 3;
		param <<= 3;

		down(&rd->semaphore);
		rd->props.et_regmap_mode &= ~ET_IO_BLK_MODE_MASK;
		rd->props.et_regmap_mode |= param;
		up(&rd->semaphore);
		if (param == ET_IO_PASS_THROUGH)
			et_regmap_cache_sync(rd);
		break;
	case ET_DBG_IO_LOG:
		ret = get_parameters(lbuf, &param, 1);
		if (ret < 0)
			return ret;
		down(&rd->semaphore);
		rd->props.io_log_en = !!param;
		up(&rd->semaphore);
		break;
	case ET_DBG_CACHE_MODE:
		ret = get_parameters(lbuf, &param, 1);
		if (ret < 0)
			return ret;
		if (param > 2)
			param = 2;
		param <<= 1;
		et_regmap_set_cache_mode(rd, param);
		break;
	default:
		return -EINVAL;
	}

	return count;
}

static const struct file_operations general_ops = {
	.owner = THIS_MODULE,
	.open = general_open,
	.write = general_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

#define ET_CREATE_GENERAL_FILE(_id, _name, _mode)			\
{									\
	rd->rtdbg_st[_id].info = rd;					\
	rd->rtdbg_st[_id].id = _id;					\
	rd->et_debug_file[_id] = debugfs_create_file(_name, _mode, dir,	\
				 &rd->rtdbg_st[_id], &general_ops);	\
	if (!rd->et_debug_file[_id])					\
		return -EINVAL;						\
}

/* create general debugfs node */
static int et_create_general_debug(struct et_regmap_device *rd,
				    struct dentry *dir)
{
	ET_CREATE_GENERAL_FILE(ET_DBG_REG_ADDR, "reg_addr", 0444);
	ET_CREATE_GENERAL_FILE(ET_DBG_DATA, "data", 0444);
	ET_CREATE_GENERAL_FILE(ET_DBG_REGS, "regs", 0444);
	ET_CREATE_GENERAL_FILE(ET_DBG_SYNC, "sync", 0444);
	ET_CREATE_GENERAL_FILE(ET_DBG_ERROR, "Error", 0444);
	ET_CREATE_GENERAL_FILE(ET_DBG_NAME, "name", 0444);
	ET_CREATE_GENERAL_FILE(ET_DBG_BLOCK, "block", 0444);
	ET_CREATE_GENERAL_FILE(ET_DBG_SIZE, "size", 0444);
	ET_CREATE_GENERAL_FILE(ET_DBG_DEVICE_ADDR, "device_addr", 0444);
	ET_CREATE_GENERAL_FILE(ET_DBG_SUPPORT_MODE, "suppoet_mode", 0444);
	ET_CREATE_GENERAL_FILE(ET_DBG_IO_LOG, "io_log", 0444);
	ET_CREATE_GENERAL_FILE(ET_DBG_CACHE_MODE, "cache_mode", 0444);
	ET_CREATE_GENERAL_FILE(ET_DBG_REG_SIZE, "reg_size", 0444);

	return 0;
}

static int eachreg_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t eachreg_read(struct file *file, char __user *ubuf,
			    size_t count, loff_t *ppos)
{
	int ret = 0, i = 0, j = 0;
	struct et_debug_st *st = file->private_data;
	struct et_regmap_device *rd = st->info;
	const et_register_map_t rm = rd->props.rm[st->id];
	char *lbuf = NULL;
	const size_t lbuf_size = MAX_BYTE_SIZE * 3 + 11;

	lbuf = kzalloc(lbuf_size, GFP_KERNEL);
	if (!lbuf)
		return -ENOMEM;

	down(&rd->semaphore);
	ret = rd->regmap_ops.regmap_block_read(rd, rm->addr,
					       rm->size, rd->regval);
	up(&rd->semaphore);
	if (ret < 0) {
		dev_notice(&rd->dev, "%s block read fail(%d) @ 0x%02x\n",
				     __func__, ret, rm->size);
		goto out;
	}

	ret = snprintf(lbuf + j, lbuf_size - j, "reg0x%02x:0x", rm->addr);
	if ((ret < 0) || (ret >= lbuf_size - j)) {
		ret = -EINVAL;
		goto out;
	}
	j += ret;
	for (i = 0; i < rm->size; i++) {
		ret = snprintf(lbuf + j, lbuf_size - j, "%02x,", rd->regval[i]);
		if ((ret < 0) || (ret >= lbuf_size - j)) {
			ret = -EINVAL;
			goto out;
		}
		j += ret;
	}
	ret = snprintf(lbuf + j, lbuf_size - j, "\n");
	if ((ret < 0) || (ret >= lbuf_size - j)) {
		ret = -EINVAL;
		goto out;
	}
	j += ret;
	ret = simple_read_from_buffer(ubuf, count, ppos, lbuf, strlen(lbuf));
out:
	kfree(lbuf);
	return ret;
}

static ssize_t eachreg_write(struct file *file, const char __user *ubuf,
			     size_t count, loff_t *ppos)
{
	int ret = 0;
	struct et_debug_st *st = file->private_data;
	struct et_regmap_device *rd = st->info;
	const et_register_map_t rm = rd->props.rm[st->id];
	char lbuf[128];
	ssize_t res = 0;

	if ((rm->size - 1) * 3 + 5 != count) {
		dev_notice(&rd->dev,
			   "%s wrong input length, size = %u, count = %u\n",
			   __func__, rm->size, (unsigned int)count);
		return -EINVAL;
	}

	dev_info(&rd->dev, "%s @ %p, count = %u, pos = %llu\n",
			   __func__, ubuf, (unsigned int)count, *ppos);
	*ppos = 0;
	res = simple_write_to_buffer(lbuf, sizeof(lbuf) - 1, ppos, ubuf, count);
	if (res <= 0)
		return -EFAULT;
	count = res;
	lbuf[count] = '\0';

	memset(rd->regval, 0, sizeof(rd->regval));
	ret = get_data(lbuf, count, rd->regval, rm->size);
	if (ret < 0) {
		dev_notice(&rd->dev, "%s get data fail(%d)\n", __func__, ret);
		return ret;
	}

	down(&rd->semaphore);
	ret = rd->regmap_ops.regmap_block_write(rd, rm->addr, rm->size,
						rd->regval);
	up(&rd->semaphore);
	if (ret < 0) {
		dev_notice(&rd->dev, "%s block write fail(%d) @ 0x%02x\n",
				     __func__, ret, rm->addr);
		return ret;
	}

	return count;
}

static const struct file_operations eachreg_ops = {
	.open = eachreg_open,
	.read = eachreg_read,
	.write = eachreg_write,
};

/* create every register node at debugfs */
static int et_create_every_debug(struct et_regmap_device *rd,
				 struct dentry *dir)
{
	int ret = 0, i = 0;
	char buf[10];

	rd->et_reg_file = devm_kzalloc(&rd->dev,
		rd->props.register_num * sizeof(*rd->et_reg_file), GFP_KERNEL);
	if (!rd->et_reg_file)
		return -ENOMEM;

	rd->reg_st = devm_kzalloc(&rd->dev,
				  rd->props.register_num * sizeof(*rd->reg_st),
				  GFP_KERNEL);
	if (!rd->reg_st)
		return -ENOMEM;

	for (i = 0; i < rd->props.register_num; i++) {
		ret = snprintf(buf, sizeof(buf),
			       "reg0x%02x", rd->props.rm[i]->addr);
		if ((ret < 0) || (ret >= sizeof(buf))) {
			dev_notice(&rd->dev, "%s snprintf fail(%d)\n",
					     __func__, ret);
			continue;
		}

		rd->et_reg_file[i] = devm_kzalloc(&rd->dev,
						  sizeof(*rd->et_reg_file[i]),
						  GFP_KERNEL);
		rd->reg_st[i] = devm_kzalloc(&rd->dev,
					     sizeof(*rd->reg_st[i]),
					     GFP_KERNEL);
		if (!rd->et_reg_file[i] || !rd->reg_st[i])
			return -ENOMEM;

		rd->reg_st[i]->info = rd;
		rd->reg_st[i]->id = i;
		rd->et_reg_file[i] = debugfs_create_file(buf, 0444, dir,
							 rd->reg_st[i],
							 &eachreg_ops);
		if (!rd->et_reg_file[i])
			return -EINVAL;
	}

	return 0;
}
#endif /* CONFIG_DEBUG_FS */

/* check the et_register format is correct */
static int et_regmap_check(struct et_regmap_device *rd)
{
	int i = 0;
	const et_register_map_t *rm = rd->props.rm;

	/* check name property */
	if (!rd->props.name) {
		pr_notice("%s no name\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < rd->props.register_num; i++) {
		/* check byte size, 1 byte ~ 32 bytes is valid */
		if (rm[i]->size < 1 || rm[i]->size > MAX_BYTE_SIZE) {
			pr_notice("%s size(%d) must be %d ~ %d @ 0x%02x\n",
				  __func__, rm[i]->size, 1, MAX_BYTE_SIZE,
				  rm[i]->addr);
			return -EINVAL;
		}
	}

	for (i = 0; i < rd->props.register_num - 1; i++) {
		/* check register sequence */
		if (rm[i]->addr >= rm[i + 1]->addr) {
			pr_info("%s sequence error @ 0x%02x\n",
				__func__, rm[i]->addr);
		}
	}

	/* no default reg_addr and reister_map first addr is not 0x00 */
#ifdef CONFIG_DEBUG_FS
	if (!rd->dbg_data.reg_addr && rm[0]->addr) {
		rd->dbg_data.reg_addr = rm[0]->addr;
		rd->dbg_data.rio.index = 0;
		rd->dbg_data.rio.offset = 0;
	}
#endif /* CONFIG_DEBUG_FS */
	return 0;
}

struct et_regmap_device *et_regmap_device_register_ex
			(struct et_regmap_properties *props,
			 struct et_regmap_fops *rops,
			 struct device *parent,
			 void *client, int dev_addr, void *drvdata)
{
	int ret = 0, i = 0;
	struct et_regmap_device *rd = NULL;

	if (!props) {
		pr_notice("%s et_regmap_properties is NULL\n", __func__);
		return NULL;
	}
	if (!rops) {
		pr_notice("%s et_regmap_fops is NULL\n", __func__);
		return NULL;
	}

	pr_info("%s name = %s\n", __func__, props->name);
	rd = kzalloc(sizeof(*rd), GFP_KERNEL);
	if (!rd)
		return NULL;

	memcpy(&rd->props, props, sizeof(rd->props));
	rd->rops = rops;
	rd->dev.parent = parent;
	dev_set_drvdata(&rd->dev, drvdata);
	rd->client = client;
	sema_init(&rd->semaphore, 1);
	sema_init(&rd->write_mode_lock, 1);
	INIT_DELAYED_WORK(&rd->et_work, et_work_func);
	rd->dev_addr = dev_addr;

	/* check et_registe_map format */
	ret = et_regmap_check(rd);
	if (ret < 0) {
		pr_notice("%s check fail(%d)\n", __func__, ret);
		goto out;
	}

	dev_set_name(&rd->dev, "et_regmap_%s", rd->props.name);
	ret = device_register(&rd->dev);
	if (ret) {
		pr_notice("%s device register fail(%d)\n", __func__, ret);
		goto out;
	}

	rd->err_msg = devm_kzalloc(&rd->dev, ERR_MSG_SIZE, GFP_KERNEL);
	if (!rd->err_msg)
		goto err_msgalloc;

	ret = et_regmap_cache_init(rd);
	if (ret < 0) {
		pr_notice("%s init fail(%d)\n", __func__, ret);
		goto err_cacheinit;
	}

	for (i = 0; i <= 3; i++)
		rd->et_block_write[i] = et_block_map[i];

	et_regmap_set_cache_mode(rd, rd->props.et_regmap_mode);

#ifdef CONFIG_DEBUG_FS
	rd->et_den = debugfs_create_dir(props->name, et_regmap_dir);
	if (rd->et_den) {
		ret = et_create_general_debug(rd, rd->et_den);
		if (ret < 0) {
			pr_notice("%s create general debug fail(%d)\n",
				  __func__, ret);
			goto err_create_general_debug;
		}
		if (rd->props.et_regmap_mode & ET_DBG_MODE_MASK) {
			ret = et_create_every_debug(rd, rd->et_den);
			if (ret < 0) {
				pr_notice("%s create every debug fail(%d)\n",
					  __func__, ret);
				goto err_create_every_debug;
			}
		}
	} else {
		pr_notice("%s debugfs create dir fail\n", __func__);
		goto err_debug;
	}
#endif /* CONFIG_DEBUG_FS */

	return rd;

#ifdef CONFIG_DEBUG_FS
err_create_every_debug:
err_create_general_debug:
	debugfs_remove_recursive(rd->et_den);
err_debug:
#endif /* CONFIG_DEBUG_FS */
err_cacheinit:
err_msgalloc:
	device_unregister(&rd->dev);
out:
	kfree(rd);
	return NULL;
}
EXPORT_SYMBOL(et_regmap_device_register_ex);

/* et_regmap_device_unregister - unregister et_regmap_device */
void et_regmap_device_unregister(struct et_regmap_device *rd)
{
	if (!rd)
		return;
	down(&rd->semaphore);
	rd->rops = NULL;
	up(&rd->semaphore);
#ifdef CONFIG_DEBUG_FS
	debugfs_remove_recursive(rd->et_den);
#endif /* CONFIG_DEBUG_FS */
	device_unregister(&rd->dev);
	kfree(rd);
}
EXPORT_SYMBOL(et_regmap_device_unregister);

static int __init regmap_plat_init(void)
{
	pr_info("Init ETEK RegMap %s\n", ET_REGMAP_VERSION);
#ifdef CONFIG_DEBUG_FS
	et_regmap_dir = debugfs_create_dir("et-regmap", NULL);
	if (!et_regmap_dir) {
		pr_notice("%s debugfs create dir fail\n", __func__);
		return -EINVAL;
	}
#endif /* CONFIG_DEBUG_FS */
	return 0;
}
subsys_initcall(regmap_plat_init);

static void __exit regmap_plat_exit(void)
{
#ifdef CONFIG_DEBUG_FS
	debugfs_remove(et_regmap_dir);
#endif /* CONFIG_DEBUG_FS */
}
module_exit(regmap_plat_exit);

MODULE_DESCRIPTION("ETEK regmap Driver");
MODULE_AUTHOR("Jeff Chang <jeff_chang@etek.com>");
MODULE_VERSION(ET_REGMAP_VERSION);
MODULE_LICENSE("GPL");

/* Release Note
 * 1.2.1
 *	Fix the deadlock in et_asyn_cache_block_write()
 *
 * 1.2.0
 *	Revise memory allocation, code flow, and error handling
 *
 * 1.1.14
 *	Fix Coverity by Mandatory's
 */
