/*
 * Copyright (c) [2009-2013] Marvell International Ltd. and its affiliates.
 * All rights reserved.
 * This software file (the "File") is owned and distributed by Marvell
 * International Ltd. and/or its affiliates ("Marvell") under the following
 * licensing terms.
 * If you received this File from Marvell, you may opt to use, redistribute
 * and/or modify this File in accordance with the terms and conditions of
 * the General Public License Version 2, June 1991 (the "GPL License"), a
 * copy of which is available along with the File in the license.txt file
 * or by writing to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA 02111-1307 or on the worldwide web at
 * http://www.gnu.org/licenses/gpl.txt. THE FILE IS DISTRIBUTED AS-IS,
 * WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY
 * DISCLAIMED. The GPL License provides additional details about this
 * warranty disclaimer.
 */
#ifndef _TEE_PERF_COUNTER
#define _TEE_PERF_COUNTER

#ifdef TEE_PERF_MEASURE
typedef struct _tee_perf_record {
	char name[4];
	unsigned int value;
} tee_perf_record;

typedef struct _tee_perf_desc {
	volatile unsigned int offset;
	unsigned int total;
} tee_perf_desc;

extern void tee_perf_init(unsigned int *base);
extern void tee_prepare_record_time(void);
extern void tee_add_time_record_point(char name[4]);
extern void tee_finish_record_time(void);
#else
#define tee_perf_init(x) do { } while (0)
#define tee_prepare_record_time() do { } while (0)
#define tee_add_time_record_point(x) do { } while (0)
#define tee_finish_record_time() do {} while (0)
#endif

#endif /* _TEE_PERF_COUNTER */
