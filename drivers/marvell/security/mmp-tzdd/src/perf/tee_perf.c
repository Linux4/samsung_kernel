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
#include "tee_perf_priv.h"
#include "tee_perf.h"

#define PMNC_MASK     0x3f	/* Mask for writable bits */
#define PMNC_E        (1 << 0)	/* Enable all counters */
#define PMNC_P        (1 << 1)	/* Reset all counters */
#define PMNC_C        (1 << 2)	/* Cycle counter reset */
#define PMNC_D        (1 << 3)	/* CCNT counts every 64th cpu cycle */
#define PMNC_X        (1 << 4)	/* Export to ETM */
#define PMNC_DP       (1 << 5)	/* Disable CCNT if non-invasive debug */

#define FLAG_C        (1 << 31)
#define FLAG_MASK     0x8000000f	/* Mask for writable bits */

#define CNTENC_C      (1 << 31)
#define CNTENC_MASK   0x8000000f	/* Mask for writable bits */

#define CNTENS_C      (1 << 31)
#define CNTENS_MASK   0x8000000f	/* Mask for writable bits */

static inline uint32_t _tee_perf_pmnc_read(void)
{
	volatile uint32_t val;

	asm volatile ("mrc p15, 0, %0, c9, c12, 0"
					: "=r" (val));
	return val;
}

static inline void _tee_perf_pmnc_write(uint32_t val)
{
	val &= PMNC_MASK;
	asm volatile ("mcr p15, 0, %0, c9, c12, 0"
					:
					: "r" (val));
}

static inline void _tee_perf_pmnc_disable_counter(void)
{
	volatile uint32_t val;

	val = CNTENC_C;

	val &= CNTENC_MASK;
	asm volatile ("mcr p15, 0, %0, c9, c12, 2"
					:
					: "r" (val));
}

static void _tee_perf_pmnc_reset_counter(void)
{
	volatile uint32_t val = 0;

	asm volatile ("mcr p15, 0, %0, c9, c13, 0"
					:
					: "r" (val));
}

static inline void _tee_perf_pmnc_enable_counter(void)
{
	volatile uint32_t val;

	val = CNTENS_C;
	val &= CNTENS_MASK;
	asm volatile ("mcr p15, 0, %0, c9, c12, 1"
					:
					: "r" (val));
}

static inline void _tee_perf_start_pmnc(void)
{
	_tee_perf_pmnc_write(_tee_perf_pmnc_read() | PMNC_E);
}

static inline void _tee_perf_stop_pmnc(void)
{
	_tee_perf_pmnc_write(_tee_perf_pmnc_read() & ~PMNC_E);
}

static inline uint32_t _tee_perf_pmnc_getreset_flags(void)
{
	volatile uint32_t val;

	/* Read */
	asm volatile ("mrc p15, 0, %0, c9, c12, 3"
					: "=r" (val));

	/* Write to clear flags */
	val &= FLAG_MASK;
	asm volatile ("mcr p15, 0, %0, c9, c12, 3"
					:
					: "r" (val));

	return val;
}

static uint32_t tee_perf_counter_read(void)
{
	volatile uint32_t val;

	asm volatile ("mrc p15, 0, %0, c9, c13, 0"
					: "=r" (val));
	return val;
}

static uint32_t tee_perf_start_counter(void)
{
	uint32_t val;
	_tee_perf_pmnc_write(PMNC_P | PMNC_C);
	_tee_perf_pmnc_disable_counter();
	_tee_perf_pmnc_reset_counter();
	_tee_perf_pmnc_enable_counter();
	val = tee_perf_counter_read();
	_tee_perf_start_pmnc();
	return val;
}

static uint32_t tee_perf_stop_counter(void)
{
	uint32_t val;
	_tee_perf_stop_pmnc();
	val = tee_perf_counter_read();
	return val;
}

static tee_perf_desc *g_tee_perf_header;
static tee_perf_record *g_tee_perf_base;

void tee_perf_init(uint32_t *buffer)
{
	g_tee_perf_header = (tee_perf_desc *)buffer;
	g_tee_perf_base =
	    (tee_perf_record *) (buffer +
				 sizeof(tee_perf_desc) / sizeof(uint32_t));
	g_tee_perf_header->offset = 0;
}

void tee_prepare_record_time(void)
{
	g_tee_perf_header->offset = 0;
	tee_perf_start_counter();
}

void tee_add_time_record_point(char name[4])
{
	tee_perf_record *current_record =
	    g_tee_perf_base + g_tee_perf_header->offset;
	uint32_t *record_name = (uint32_t *) (current_record->name);
	*record_name = *(uint32_t *) name;
	current_record->value = tee_perf_counter_read();
	g_tee_perf_header->offset++;
}

void tee_finish_record_time(void)
{
	tee_perf_stop_counter();
	g_tee_perf_header->total = g_tee_perf_header->offset;
	g_tee_perf_header->offset = 0;
}
