/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2021 Spreadtrum corporation. http://www.unisoc.com
 */
#ifndef __SPRD_KTP_H__
#define __SPRD_KTP_H__

enum ktp_event_type {
	KTP_NONE = 0,
	KTP_CTXID = 1,
	KTP_IRQ = 2,
};
#define KTYPE_NOPC 0x80

#ifdef CONFIG_SPRD_KTP

int kevent_tp(enum ktp_event_type ktp_type, void *data);

#else

static inline int kevent_tp(enum ktp_event_type ktp_type, void *data)
{
	return 0;
}

#endif

#endif
