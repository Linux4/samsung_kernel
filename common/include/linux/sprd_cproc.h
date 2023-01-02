/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
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

#ifndef _SPRD_CPROC_H
#define _SPRD_CPROC_H

enum {
	CPROC_CTRL_SHUT_DOWN = 0,
	CPROC_CTRL_DEEP_SLEEP = 1,
	CPROC_CTRL_RESET = 2,
	CPROC_CTRL_GET_STATUS = 3,
	CPROC_CTRL_NR,
};

#define CPROC_IRAM_DATA_NR 3

enum {
	CPROC_REGION_CP_MEM = 0,
	CPROC_REGION_IRAM_MEM = 1,
	CPROC_REGION_CTRL_REG = 1,
	CPROC_REGION_NR,
};

struct cproc_segments {
	char			*name;
	uint32_t		base;		/* segment addr */
	uint32_t		maxsz;		/* segment size */
};

struct cproc_ctrl {
	uint32_t iram_addr;
	uint32_t iram_data[CPROC_IRAM_DATA_NR];
	uint32_t ctrl_reg[CPROC_CTRL_NR];
	uint32_t ctrl_mask[CPROC_CTRL_NR];
};

struct cproc_init_data {
	char			*devname;
	uint32_t		base;		/* CP base addr */
	uint32_t		maxsz;		/* CP max size */
	int			(*start)(void *arg);
	int			(*stop)(void *arg);
	int			(*suspend)(void *arg);
	int			(*resume)(void *arg);

	struct cproc_ctrl 	*ctrl;

	int			wdtirq;
	uint32_t		segnr;
	struct cproc_segments	segs[];
};

#endif
