/*
 *  Copyright (C) 2010,Imagis Technology Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/stat.h>

#include "ist30xx.h"
#include "ist30xx_update.h"
#include "ist30xx_misc.h"
#include "ist30xx_tracking.h"

IST30XXC_RING_BUF TrackingBuf;
IST30XXC_RING_BUF *pTrackingBuf;

bool track_initialize = false;

void ist30xxc_tracking_init(void)
{
	if (track_initialize)
		return;

	pTrackingBuf = &TrackingBuf;

	pTrackingBuf->RingBufCtr = 0;
	pTrackingBuf->RingBufInIdx = 0;
	pTrackingBuf->RingBufOutIdx = 0;

	track_initialize = true;
}

void ist30xxc_tracking_deinit(void)
{
}

static spinlock_t mr_lock = __SPIN_LOCK_UNLOCKED();
int ist30xxc_get_track(u32 *track, int cnt)
{
	int i;
	u8 *buf = (u8 *)track;
	unsigned long flags;


	cnt *= sizeof(track[0]);

	spin_lock_irqsave(&mr_lock, flags);

	if (pTrackingBuf->RingBufCtr < (u16)cnt) {
		spin_unlock_irqrestore(&mr_lock, flags);
		return IST30XXC_RINGBUF_NOT_ENOUGH;
	}

	for (i = 0; i < cnt; i++) {
		if (pTrackingBuf->RingBufOutIdx == IST30XXC_MAX_LOG_SIZE)
			pTrackingBuf->RingBufOutIdx = 0;

		*buf++ = (u8)pTrackingBuf->LogBuf[pTrackingBuf->RingBufOutIdx++];
		pTrackingBuf->RingBufCtr--;
	}

	spin_unlock_irqrestore(&mr_lock, flags);

	return IST30XXC_RINGBUF_NO_ERR;
}

u32 ist30xxc_get_track_cnt(void)
{
	return pTrackingBuf->RingBufCtr;
}

#if IST30XXC_TRACKING_MODE
int ist30xxc_put_track(u32 *track, int cnt)
{
	int i;
	u8 *buf = (u8 *)track;
	unsigned long flags;

	spin_lock_irqsave(&mr_lock, flags);

	cnt *= sizeof(track[0]);

	pTrackingBuf->RingBufCtr += cnt;
	if (pTrackingBuf->RingBufCtr > IST30XXC_MAX_LOG_SIZE) {
		pTrackingBuf->RingBufOutIdx +=
			(pTrackingBuf->RingBufCtr - IST30XXC_MAX_LOG_SIZE);
		if (pTrackingBuf->RingBufOutIdx >= IST30XXC_MAX_LOG_SIZE)
			pTrackingBuf->RingBufOutIdx -= IST30XXC_MAX_LOG_SIZE;

		pTrackingBuf->RingBufCtr = IST30XXC_MAX_LOG_SIZE;
	}

	for (i = 0; i < cnt; i++) {
		if (pTrackingBuf->RingBufInIdx == IST30XXC_MAX_LOG_SIZE)
			pTrackingBuf->RingBufInIdx = 0;
		pTrackingBuf->LogBuf[pTrackingBuf->RingBufInIdx++] = *buf++;
	}

	spin_unlock_irqrestore(&mr_lock, flags);

	return IST30XXC_RINGBUF_NO_ERR;
}

int ist30xxc_put_track_ms(u32 ms)
{
	ms &= 0x0000FFFF;
	ms |= IST30XXC_TRACKING_MAGIC;

	return ist30xxc_put_track(&ms, 1);
}

static struct timespec t_track;
int ist30xxc_tracking(u32 status)
{
	u32 ms;

	if (!track_initialize)
		ist30xxc_tracking_init();

	ktime_get_ts(&t_track);
	ms = t_track.tv_sec * 1000 + t_track.tv_nsec / 1000000;

	ist30xxc_put_track_ms(ms);
	ist30xxc_put_track(&status, 1);

	return 0;
}
#else
int ist30xxc_put_track(u32 *track, int cnt)
{
	return 0;
}
int ist30xxc_put_track_ms(u32 ms)
{
	return 0;
}
int ist30xxc_tracking(u32 status)
{
	return 0;
}
#endif // IST30XXC_TRACKING_MODE

#define MAX_TRACKING_COUNT      (1024)
/* sysfs: /sys/class/touch/tracking/track_frame */
ssize_t ist30xxc_track_frame_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	int i, buf_cnt = 0;
	u32 track_cnt = MAX_TRACKING_COUNT;
	u32 track;
	char msg[10];

	mutex_lock(&ist30xxc_mutex);

	buf[0] = '\0';

	if (track_cnt > ist30xxc_get_track_cnt())
		track_cnt = ist30xxc_get_track_cnt();

	track_cnt /= sizeof(track);

	ts_verb("num: %d of %d\n", track_cnt, ist30xxc_get_track_cnt());

	for (i = 0; i < track_cnt; i++) {
		ist30xxc_get_track(&track, 1);

		ts_verb("%08X\n", track);

		buf_cnt += sprintf(msg, "%08x", track);
		strcat(buf, msg);
	}

	mutex_unlock(&ist30xxc_mutex);

	return buf_cnt;
}

/* sysfs: /sys/class/touch/tracking/track_cnt */
ssize_t ist30xxc_track_cnt_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	u32 val = (u32)ist30xxc_get_track_cnt();

	ts_verb("tracking cnt: %d\n", val);

	return sprintf(buf, "%08x", val);
}


/* sysfs  */
static DEVICE_ATTR(track_frame, S_IRUGO, ist30xxc_track_frame_show, NULL);
static DEVICE_ATTR(track_cnt, S_IRUGO, ist30xxc_track_cnt_show, NULL);

static struct attribute *tracking_attributes[] = {
	&dev_attr_track_frame.attr,
	&dev_attr_track_cnt.attr,
	NULL,
};

static struct attribute_group tracking_attr_group = {
	.attrs	= tracking_attributes,
};

extern struct class *ist30xxc_class;
struct device *ist30xxc_tracking_dev;

int ist30xxc_init_tracking_sysfs(void)
{
	/* /sys/class/touch/tracking */
	ist30xxc_tracking_dev = device_create(ist30xxc_class, NULL, 0, NULL,
					     "tracking");

	/* /sys/class/touch/tracking/... */
	if (sysfs_create_group(&ist30xxc_tracking_dev->kobj, &tracking_attr_group))
		ts_err("[ TSP ] Failed to create sysfs group(%s)!\n", "tracking");

	ist30xxc_tracking_init();

	return 0;
}
