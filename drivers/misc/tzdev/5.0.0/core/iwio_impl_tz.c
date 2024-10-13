/*
 * Copyright (c) 2022 Samsung Electronics Co., Ltd All Rights Reserved
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

#include "tzdev_internal.h"
#include "core/iwio.h"
#include "core/iwio_impl.h"
#include "core/log.h"

static int connect_aux_channel(struct page *page)
{
	unsigned long pfn;

	pfn = page_to_pfn(page);

	BUG_ON(pfn > U32_MAX);

	return tzdev_smc_connect_aux((unsigned int)pfn);
}

static int connect(unsigned int mode, struct page **pages, unsigned int num_pages, void *impl_data)
{
	int ret;
	unsigned int i;
	sk_pfn_t *pfns;
	struct tz_iwio_aux_channel *aux_ch;
	unsigned int offset, num_pfns;
	unsigned int pfns_in_buf = TZ_IWIO_AUX_BUF_SIZE / sizeof(sk_pfn_t);

	(void)impl_data;

	if (!num_pages)
		return -EINVAL;

	pfns = kmalloc_array(num_pages, sizeof(sk_pfn_t), GFP_KERNEL);
	if (!pfns) {
		log_error(tzdev_iwio, "IW channel pfns buffer allocation failed\n");
		return -ENOMEM;
	}

	for (i = 0; i < num_pages; i++)
		pfns[i] = page_to_pfn(pages[i]);

	/* Push PFNs list into aux channel */
	aux_ch = tz_iwio_get_aux_channel();

	for (offset = 0; offset < num_pages; ) {
		num_pfns = min(pfns_in_buf, num_pages - offset);

		memcpy(aux_ch->buffer, pfns + offset,
				num_pfns * sizeof(sk_pfn_t));

		ret = tzdev_smc_connect(mode, num_pfns);
		if (ret) {
			log_error(tzdev_iwio, "IW buffer registration failed, error=%d\n", ret);
			break;
		}

		if (pfns_in_buf > U32_MAX - offset) {
			log_error(tzdev_iwio,
					"Overflow detected when calculating offset, offset=%u, pfns_in_buf=%u\n",
					offset, pfns_in_buf);
			BUG();
		}

		offset += pfns_in_buf;
	}

	tz_iwio_put_aux_channel();
	kfree(pfns);

	return ret;
}

static int deinit(void *impl_data)
{
	(void)impl_data;

	return 0;
}

static const struct tzdev_iwio_impl impl = {
	.data_size = 0,
	.connect_aux_channel = connect_aux_channel,
	.connect = connect,
	.deinit = deinit,
};

const struct tzdev_iwio_impl *tzdev_get_iwio_impl(void)
{
	return &impl;
}
