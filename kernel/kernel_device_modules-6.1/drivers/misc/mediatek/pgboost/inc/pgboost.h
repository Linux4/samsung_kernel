/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

#ifndef _PGBOOST_H_
#define _PGBOOST_H_

enum pgboost_action {
	FAVOR_STRICT = 0x1,
	/* Interesting rank is rank#0 */
	FAVOR_RANK_0 = 0x10,
	/* Interesting rank is rank#1 */
	FAVOR_RANK_1 = 0x20,
	/* No interesting rank */
	NO_FAVOR = 0x80,
	/* Mask for FAVOR_RANK_0, FAVOR_RANK_1, NO_FAVOR */
	FAVOR_MASK = 0xF0,
};

#ifdef DEBUG_PGBOOST_ENABLED
#define PGBOOST_DEBUG(fmt, args...)	pr_info("PGBOOST_DEBUG: "fmt, ##args)
#else
#define PGBOOST_DEBUG(fmt, args...)
#endif

extern bool mt_boot_finish(void);

#endif /* _PGBOOST_H_ */

