/*
 * Copyright (C) 2013-2016 Samsung Electronics, Inc.
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

#include <linux/gfp.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <asm/pgtable.h>

#include "tzdev.h"
#include "tzlog.h"
#include "tz_iwcbuf.h"

struct tzio_iw_channel *tzio_alloc_iw_channel(unsigned int mode,
		unsigned int num_pages)
{
	struct tzio_iw_channel *channel;
	struct page *page[num_pages];
	sk_pfn_t pfns[num_pages];
	struct tzio_aux_channel *aux_ch;
	unsigned int i, j;

	/* Allocate non-contiguous buffer to reduce page allocator pressure */
	for (i = 0; i < num_pages; i++) {
		page[i] = alloc_page(GFP_KERNEL);
		if (!page[i]) {
			tzdev_print(0, "TZDev channel creation failed\n");
			goto free_buffer;
		}
		pfns[i] = page_to_pfn(page[i]);
	}

	channel = vmap(page, num_pages, VM_MAP, PAGE_KERNEL);

	if (!channel) {
		tzdev_print(0, "TZDev channel mapping failed\n");
		goto free_buffer;
	}

	/* Push PFNs list into aux channel */
	aux_ch = tzdev_get_aux_channel();
	memcpy(aux_ch->buffer, &pfns, num_pages * sizeof(sk_pfn_t));

	if (tzdev_smc_connect(mode, 0, num_pages)) {
		tzdev_put_aux_channel();
		vunmap(channel);
		tzdev_print(0, "TZDev channel registration failed\n");
		goto free_buffer;
	}

	tzdev_put_aux_channel();

	return channel;

free_buffer:
	for (j = 0; j < i; j++)
		__free_page(page[j]);

	return NULL;
}
