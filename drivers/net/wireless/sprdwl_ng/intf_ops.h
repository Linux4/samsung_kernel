/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 * Authors	:
 * Keguang Zhang <keguang.zhang@spreadtrum.com>
 * Jingxiang Li <Jingxiang.li@spreadtrum.com>
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

#ifndef __SPRDWL_INTF_OPS_H__
#define __SPRDWL_INTF_OPS_H__

#include <linux/dcache.h>
#include "intf.h"

static
inline struct sprdwl_msg_buf *sprdwl_intf_get_msg_buf(struct sprdwl_priv *priv,
						      enum sprdwl_head_type
						      type,
						      enum sprdwl_mode mode)
{
	return priv->if_ops->get_msg_buf(priv->hw_priv, type, mode);
}

static inline void sprdwl_intf_free_msg_buf(struct sprdwl_priv *priv,
					    struct sprdwl_msg_buf *msg)
{
	return priv->if_ops->free_msg_buf(priv->hw_priv, msg);
}

/* return:
 *      0, msg buf freed by the real driver
 *      others, skb need free by the caller,remember not use msg->skb!
 */
static inline int sprdwl_intf_tx(struct sprdwl_priv *priv,
				 struct sprdwl_msg_buf *msg)
{
	return priv->if_ops->tx(priv->hw_priv, msg);
}

static inline void sprdwl_intf_force_exit(struct sprdwl_priv *priv)
{
	priv->if_ops->force_exit();
}

static inline int sprdwl_intf_is_exit(struct sprdwl_priv *priv)
{
	return priv->if_ops->is_exit();
}

static inline int sprdwl_intf_suspend(struct sprdwl_priv *priv)
{
	if (priv->if_ops->suspend)
		return priv->if_ops->suspend(priv);

	return 0;
}

static inline int sprdwl_intf_resume(struct sprdwl_priv *priv)
{
	if (priv->if_ops->resume)
		return priv->if_ops->resume(priv);

	return 0;
}

static inline void sprdwl_intf_debugfs(struct sprdwl_priv *priv,
				       struct dentry *dir)
{
	if (priv->if_ops->debugfs)
		priv->if_ops->debugfs(dir);
}
#endif
