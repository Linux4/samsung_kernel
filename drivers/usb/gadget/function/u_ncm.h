/*
 * u_ncm.h
 *
 * Utility definitions for the ncm function
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Author: Andrzej Pietrasiewicz <andrzej.p@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef U_NCM_H
#define U_NCM_H

#include <linux/usb/composite.h>

struct f_ncm_opts {
	struct usb_function_instance	func_inst;
	struct net_device		*net;
	bool				bound;

	/*
	 * Read/write access to configfs attributes is handled by configfs.
	 *
	 * This is to protect the data from concurrent access by read/write
	 * and create symlink/remove symlink.
	 */
	struct mutex			lock;
	int				refcnt;
};

struct ncm_header {
	u32 signature;
	u16 header_len;
	u16 sequence;
	u16 blk_len;
	u16 index;
	u32 dgram_sig;
	u16 dgram_header_len;
	u16 dgram_rev;
	u16 dgram_index0;
	u16 dgram_len0;
} __packed;

#define NCM_NTH_SIGNATURE		(0x484D434E)
#define NCM_NTH_LEN16			(0xC)
#define NCM_NTH_SEQUENCE		(0x0)
#define NCM_NTH_INDEX16			(0xC)
#define NCM_NDP_SIGNATURE		(0x304D434E)
#define NCM_NDP_LEN16			(0xB4)
#define NCM_NDP_REV			(0x0)

#endif /* U_NCM_H */
