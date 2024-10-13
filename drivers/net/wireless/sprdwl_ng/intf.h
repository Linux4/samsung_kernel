/*
 * Copyright (C) 2013 Spreadtrum Communications Inc.
 *
 * Authors:
 * Keguang Zhang <keguang.zhang@spreadtrum.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __SPRDWL_INTF_H__
#define __SPRDWL_INTF_H__

#include "sprdwl.h"
#include "msg.h"

struct sprdwl_priv;

struct sprdwl_if_ops {
	struct sprdwl_msg_buf *(*get_msg_buf)(void *sdev,
					      enum sprdwl_head_type type,
					      enum sprdwl_mode mode);
	void (*free_msg_buf)(void *sdev, struct sprdwl_msg_buf *msg);
	int (*tx)(void *spdev, struct sprdwl_msg_buf *msg);
	void (*force_exit)(void);
	int (*is_exit)(void);
	int (*suspend)(struct sprdwl_priv *priv);
	int (*resume)(struct sprdwl_priv *priv);
	void (*debugfs)(struct dentry *dir);
};

#endif/*__INTF_H__*/
