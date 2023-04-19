// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 * Gwanghui Lee <gwanghui.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __PANEL_DUMP_H__
#define __PANEL_DUMP_H__

struct dump_expect {
	u32 offset;
	u8 mask;
	u8 value;
	char *msg;
};

struct dumpinfo {
	struct pnobj base;
	struct resinfo *res;
	int (*callback)(struct dumpinfo *info);
	struct dump_expect *expects;
	unsigned int nr_expects;
};

#define DUMPINFO_INIT(_dumpname, _resource, _callback)	\
	{ .base = __PNOBJ_INITIALIZER(_dumpname, CMD_TYPE_DMP) \
	, .res = (_resource) \
	, .callback = (_callback) }

#define DUMPINFO_INIT_V2(_dumpname, _resource, _callback, _expects)	\
	{ .base = __PNOBJ_INITIALIZER(_dumpname, CMD_TYPE_DMP) \
	, .res = (_resource) \
	, .callback = (_callback) \
	, .expects = (_expects) \
	, .nr_expects = ARRAY_SIZE(_expects) }

static inline char *get_dump_name(struct dumpinfo *dump)
{
	return get_pnobj_name(&dump->base);
}

struct dumpinfo *create_dumpinfo(char *name,
		struct resinfo *res, int (*callback)(struct dumpinfo *),
		struct dump_expect *expects, unsigned int nr_expects);
void destroy_dumpinfo(struct dumpinfo *dump);

#endif /* __PANEL_DUMP_H__ */
