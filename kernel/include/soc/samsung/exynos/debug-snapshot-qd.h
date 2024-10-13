/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2021 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 */
#ifndef DEBUG_SNAPSHOT_QD_H
#define DEBUG_SNAPSHOT_QD_H

struct dbg_snapshot_qd_region {
	char name[16];
	char struct_name[16];
	u64 virt_addr;
	u64 phys_addr;
	u64 size;
	u32 attr;
	u32 magic;
} __packed;

#define DSS_QD_ATTR_LOG			(0)
#define DSS_QD_ATTR_ARRAY		(1)
#define DSS_QD_ATTR_STRUCT		(2)
#define DSS_QD_ATTR_STACK		(3)
#define DSS_QD_ATTR_SYMBOL		(4)
#define DSS_QD_ATTR_BINARY		(5)
#define DSS_QD_ATTR_ELF			(6)
#define DSS_QD_ATTR_SKIP_ENCRYPT	(1 << 31)

#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT_QUICKDUMP)
extern int dbg_snapshot_qd_add_region(void *v_entry, u32 attr);
extern bool dbg_snapshot_qd_enable(void);
#else
#define dbg_snapshot_qd_add_region(a, b)	(-1)
#define dbg_snapshot_qd_enable()		(-1)
#endif
#endif
