/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series Pablo IS driver
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef PABLO_CRTA_BUFMGR_H
#define PABLO_CRTA_BUFMGR_H

#include <linux/types.h>
#include "is-config.h"

struct device;

enum pablo_crta_buf_type {
	PABLO_CRTA_BUF_BASE,
	PABLO_CRTA_BUF_PCFI = PABLO_CRTA_BUF_BASE,		/* driver <-> crta */
	PABLO_CRTA_BUF_PCSI,					/* driver -> crta */
	PABLO_CRTA_BUF_CDAF,					/* cstat hw -> crta */
	PABLO_CRTA_BUF_PRE_THUMB,				/* cstat hw -> crta */
	PABLO_CRTA_BUF_AE_THUMB,				/* cstat hw -> crta */
	PABLO_CRTA_BUF_AWB_THUMB,				/* cstat hw -> crta */
	PABLO_CRTA_BUF_RGBY_HIST,				/* cstat hw -> crta */
	PABLO_CRTA_BUF_CDAF_MW,					/* cstat hw -> crta */
	PABLO_CRTA_BUF_SS_CTRL,					/* driver <- crta */
	PABLO_CRTA_BUF_LAF,					/* driver -> crta */
	PABLO_CRTA_BUF_MAX
};

struct pablo_crta_bufmgr {
	void					*priv;

	enum pablo_crta_buf_type		type;
	u32					id;
	const struct pablo_crta_bufmgr_ops	*ops;
};

struct pablo_crta_buf_info
{
	enum pablo_crta_buf_type	type;
	u32				id;
	u32				fcount;
	u32				idx;
	void				*kva;
	dma_addr_t			dva;		/* dva for icpu */
	dma_addr_t			dva_cstat;	/* dva for cstat */
	struct is_frame			*frame;
	size_t				size;
};

#define CALL_CRTA_BUFMGR_OPS(bufmgr, op, args...)	\
	((bufmgr && (bufmgr)->ops && (bufmgr)->ops->op) ? ((bufmgr)->ops->op(bufmgr, ##args)) : 0)

struct pablo_crta_bufmgr_ops {
	int (*open)(struct pablo_crta_bufmgr *crta_bufmgr);
	int (*close)(struct pablo_crta_bufmgr *crta_bufmgr);
	int (*get_free_buf)(struct pablo_crta_bufmgr *crta_bufmgr,
			u32 fcount, bool flush, struct pablo_crta_buf_info *buf_info);
	int (*get_process_buf)(struct pablo_crta_bufmgr *crta_bufmgr,
			u32 fcount, struct pablo_crta_buf_info *buf_info);
	int (*get_static_buf)(struct pablo_crta_bufmgr *crta_bufmgr,
			struct pablo_crta_buf_info *buf_info);
	int (*put_buf)(struct pablo_crta_bufmgr *crta_bufmgr,
			struct pablo_crta_buf_info *buf_info);
	int (*flush_buf)(struct pablo_crta_bufmgr *crta_bufmgr,
			u32 fcount);
};

#if IS_ENABLED(CONFIG_PABLO_CRTA_BUFMGR)
int pablo_crta_bufmgr_probe(void);
struct pablo_crta_bufmgr *pablo_get_crta_bufmgr(enum pablo_crta_buf_type type, u32 instance, u32 hw_id);
#else
#define pablo_crta_bufmgr_probe(crta_bufmgr) ({ 0; })
#define pablo_get_crta_bufmgr(type, instance, hw_id) ({ NULL; })
#endif
#endif /* PABLO_CRTA_BUFMGR_H */
