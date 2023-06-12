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

struct cproc_segments {
	char			*name;
	uint32_t		base;		/* segment addr */
	uint32_t		maxsz;		/* segment size */
};

struct cproc_init_data {
	char			*devname;
	uint32_t		base;		/* CP base addr */
	uint32_t		maxsz;		/* CP max size */
	int			(*start)(void *arg);
	int			(*stop)(void *arg);
	int			(*suspend)(void *arg);
	int			(*resume)(void *arg);

	int			wdtirq;
	uint32_t		segnr;
	struct cproc_segments	segs[];
};

#endif
