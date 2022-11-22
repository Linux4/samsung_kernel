/*
 * Copyright (C) 2014 Spreadtrum Communications Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __SPRD_OTP_H__
#define __SPRD_OTP_H__

#include <linux/miscdevice.h>

struct sprd_otp_operations {
	void (*reset) (void);
	 u32(*read) (int blk_index);
	int (*prog) (int blk_index, u32 data);
};

struct sprd_otp_device {
	struct miscdevice misc;
	int blk_max;		/* maximun valid blocks */
	int blk_width;		/* bytes counts per block */
	struct sprd_otp_operations *ops;
	struct list_head list;
};
#define BITSINDEX(b, o)	( (b) * 8 + (o) )

int sprd_efuse_init(void);
int sprd_adie_efuse_init(void);
u32 __adie_efuse_read(int blk_index);
u32 __adie_efuse_read_bits(int bit_index, int length);

int sprd_adie_laserfuse_init(void);
void *sprd_otp_register(const char *name, void *ops, int blk_max,
			int blk_width);

/* EXPORT API */
u32 __ddie_efuse_read(int blk_index);
u32 __adie_laserfuse_read(int blk_index);
u32 __adie_efuse_read(int blk_index);
u32 __adie_efuse_read_bits(int bit_index, int length);

#endif /* __SPRD_OTP_H__ */
