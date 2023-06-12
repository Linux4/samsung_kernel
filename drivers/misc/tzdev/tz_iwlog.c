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

#include <linux/string.h>
#include <linux/spinlock.h>

#include "tz_iwcbuf.h"
#include "tz_iwlog.h"
#include "tzdev.h"

#define TZ_IWLOG_BUF_SIZE		(CONFIG_TZLOG_PG_CNT * PAGE_SIZE - sizeof(struct tzio_iw_channel))

#define TZ_IWLOG_LINE_MAX_LEN	256
#define TZ_IWLOG_PREFIX			KERN_DEFAULT "SW> "

static DEFINE_SPINLOCK(tz_iwlog_slock);
static DEFINE_PER_CPU(struct tzio_iw_channel *, tz_iwlog_channel);

struct tz_iwlog_print_state {
	char line[TZ_IWLOG_LINE_MAX_LEN + 1];	/* one byte for \0 */
	unsigned int line_len;
};

static DEFINE_PER_CPU(struct tz_iwlog_print_state, tz_iwlog_print_state);

static unsigned int tz_iwlog_channel_print(const char *buf, unsigned int count)
{
	struct tz_iwlog_print_state *ps;
	unsigned int count_out, avail, bytes_in, bytes_out, bytes_printed, tmp, wait_dta;
	char *p;

	ps = &get_cpu_var(tz_iwlog_print_state);

	count_out = count;

	wait_dta = 0;
	while (count_out) {
		avail = TZ_IWLOG_LINE_MAX_LEN - ps->line_len;

		p = memchr(buf, '\n', count_out);

		if (p) {
			if (p - buf > avail) {
				bytes_in = avail;
				bytes_out = avail;
			} else {
				bytes_in = p - buf + 1;
				bytes_out = p - buf;
			}
		} else {
			if (count_out >= avail) {
				bytes_in = avail;
				bytes_out = avail;
			} else {
				bytes_in = count_out;
				bytes_out = count_out;

				wait_dta = 1;
			}
		}

		memcpy(&ps->line[ps->line_len], buf, bytes_out);
		ps->line_len += bytes_out;

		if (wait_dta)
			break;

		ps->line[ps->line_len] = 0;

		bytes_printed = 0;
		while (bytes_printed < ps->line_len) {
			tmp = printk(TZ_IWLOG_PREFIX "%s\n", &ps->line[bytes_printed]);
			if (!tmp)
				break;

			bytes_printed += tmp;
		}

		ps->line_len = 0;

		count_out -= bytes_in;
		buf += bytes_in;
	}

	put_cpu_var(tz_iwlog_print_state);

	return count;
}

void tz_iwlog_read_channels(void)
{
	struct tzio_iw_channel *ch;
	unsigned int i, bytes, count, write_count;

	spin_lock(&tz_iwlog_slock);

	for (i = 0; i < NR_SW_CPU_IDS; ++i) {
		ch = per_cpu(tz_iwlog_channel, i);
		if (!ch)
			continue;

		write_count = ch->write_count;
		if (write_count < ch->read_count) {
			count = TZ_IWLOG_BUF_SIZE - ch->read_count;

			bytes = tz_iwlog_channel_print(ch->buffer + ch->read_count, count);
			if (bytes < count) {
				ch->read_count += bytes;
				continue;
			}

			ch->read_count = 0;
		}

		count = write_count - ch->read_count;

		bytes = tz_iwlog_channel_print(ch->buffer + ch->read_count, count);
		ch->read_count += count;
	}

	spin_unlock(&tz_iwlog_slock);
}

int tz_iwlog_alloc_channels(void)
{
	unsigned int i;

	for (i = 0; i < NR_SW_CPU_IDS; ++i) {
		struct tzio_iw_channel *channel = tzio_alloc_iw_channel(
				TZDEV_CONNECT_LOG, CONFIG_TZLOG_PG_CNT);
		if (!channel)
			return -ENOMEM;

		per_cpu(tz_iwlog_channel, i) = channel;
	}

	return 0;
}
