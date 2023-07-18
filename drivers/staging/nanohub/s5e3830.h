/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * CHUB IF Driver Exynos specific code
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 * Authors:
 *	 Sukwon Ryu <sw.ryoo@samsung.com>
 *
 */
#ifndef _S5E3830_H_
#define _S5E3830_H_

#undef PACKET_SIZE_MAX
#define PACKET_SIZE_MAX			780
#define LOGBUF_NUM				80
#define CIPC_START_OFFSET		8192
#define CHUB_CLK_BASE			36000000

#define CHUB_CPU_CONFIGURATION                  (0x11862D00)
#define REG_CHUB_CPU_STATUS			(0x0)
#define REG_CHUB_CPU_OPTION			(0x0)
#define ENABLE_SYSRESETREQ			BIT(0)
#define CHUB_RESET_RELEASE_VALUE		(0x0)

#define REG_MAILBOX_ISSR0 (0x080)
#define REG_MAILBOX_ISSR1 (0x084)
#define REG_MAILBOX_ISSR2 (0x088)
#define REG_MAILBOX_ISSR3 (0x08C)

#endif /* _S5E8535_H_ */
