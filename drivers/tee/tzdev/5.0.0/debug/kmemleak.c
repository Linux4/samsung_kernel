/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd All Rights Reserved
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
#include "core/log.h"
#include "core/notifier.h"
#include "core/subsystem.h"

enum {
	TZDEV_KMEMLEAK_CMD_GET_BUF_SIZE,
};

static void *buf;

static int kmemleak_init_call(struct notifier_block *cb, unsigned long code, void *unused)
{
	int ret;
	unsigned int num_pages;
	struct tz_iwio_aux_channel *ch;

	(void)cb;
	(void)code;
	(void)unused;

	ch = tz_iwio_get_aux_channel();

	ret = tzdev_smc_kmemleak_cmd(TZDEV_KMEMLEAK_CMD_GET_BUF_SIZE);
	if (ret) {
		if (ret == -ENOSYS) {
			tz_iwio_put_aux_channel();
			return 0;
		}

		log_error(tzdev, "Failed to get buffer size, error=%d\n", ret);
		tz_iwio_put_aux_channel();
		return ret;
	}

	num_pages = *((unsigned int *)ch->buffer);
	tz_iwio_put_aux_channel();

	buf = tz_iwio_alloc_iw_channel(TZ_IWIO_CONNECT_KMEMLEAK, num_pages, NULL, NULL, NULL);
	if (IS_ERR(buf))
		return NOTIFY_BAD;

	return NOTIFY_DONE;
}

static int kmemleak_fini_call(struct notifier_block *cb, unsigned long code, void *unused)
{
	(void)cb;
	(void)code;
	(void)unused;

	tz_iwio_free_iw_channel(buf);

	return NOTIFY_DONE;
}

static struct notifier_block kmemleak_init_notifier = {
	.notifier_call = kmemleak_init_call,
};

static struct notifier_block kmemleak_fini_notifier = {
	.notifier_call = kmemleak_fini_call,
};

int tz_kmemleak_init(void)
{
	int rc;

	rc = tzdev_blocking_notifier_register(TZDEV_INIT_NOTIFIER, &kmemleak_init_notifier);
	if (rc)
		return rc;

	rc = tzdev_blocking_notifier_register(TZDEV_FINI_NOTIFIER, &kmemleak_fini_notifier);
	if (rc) {
		tzdev_blocking_notifier_unregister(TZDEV_INIT_NOTIFIER, &kmemleak_init_notifier);
		return rc;
	}

	return 0;
}

tzdev_early_initcall(tz_kmemleak_init);
