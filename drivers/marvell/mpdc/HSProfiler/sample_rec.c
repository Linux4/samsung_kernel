/*
** (C) Copyright 2009 Marvell International Ltd.
**  		All Rights Reserved

** This software file (the "File") is distributed by Marvell International Ltd.
** under the terms of the GNU General Public License Version 2, June 1991 (the "License").
** You may use, redistribute and/or modify this File in accordance with the terms and
** conditions of the License, a copy of which is available along with the File in the
** license.txt file or by writing to the Free Software Foundation, Inc., 59 Temple Place,
** Suite 330, Boston, MA 02111-1307 or on the worldwide web at http://www.gnu.org/licenses/gpl.txt.
** THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED WARRANTIES
** OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY DISCLAIMED.
** The License provides additional details about this warranty disclaimer.
*/

#include "hs_drv.h"
#include "PXD_hs.h"
#include "ring_buffer.h"

#include <linux/interrupt.h>
#include <linux/sched.h>

static void write_sample_record(unsigned int cpu,
                                PXD32_Hotspot_Sample_V2 *sample_rec,
                                bool *need_flush,
                                bool *buffer_full)
{
	struct RingBufferInfo * sample_buffer = &per_cpu(g_sample_buffer_hs, cpu);
	
	g_sample_count_hs++;

	write_ring_buffer(&sample_buffer->buffer, sample_rec, sizeof(PXD32_Hotspot_Sample_V2), buffer_full, need_flush);
}

bool write_sample(unsigned int cpu, PXD32_Hotspot_Sample_V2 * sample_rec)
{
	bool need_flush = false;
	bool buffer_full = false;
	struct RingBufferInfo * sample_buffer = &per_cpu(g_sample_buffer_hs, cpu);

	write_sample_record(cpu, sample_rec, &need_flush, &buffer_full);

	if (need_flush && !sample_buffer->is_full_event_set)
	{
		sample_buffer->is_full_event_set = true;

		if (waitqueue_active(&pxhs_kd_wait))
			wake_up_interruptible(&pxhs_kd_wait);
	}

	return buffer_full;
}
