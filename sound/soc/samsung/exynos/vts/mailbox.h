/* SPDX-License-Identifier: GPL-2.0-or-later
 * sound/soc/samsung/vts/mailbox.h
 *
 * ALSA SoC - Samsung Mailbox driver
 *
 * Copyright (c) 2016 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __MAILBOX_H
#define __MAILBOX_H

/* MAILBOX */
#define MAILBOX_MCUCTRL                 (0x0000)
#define MAILBOX_INTGR0                  (0x0008)
#define MAILBOX_INTCR0                  (0x000C)
#define MAILBOX_INTMR0                  (0x0010)
#define MAILBOX_INTSR0                  (0x0014)
#define MAILBOX_INTMSR0                 (0x0018)
#define MAILBOX_INTGR1                  (0x001C)
#define MAILBOX_INTCR1                  (0x0020)
#define MAILBOX_INTMR1                  (0x0024)
#define MAILBOX_INTSR1                  (0x0028)
#define MAILBOX_INTMSR1                 (0x002C)
#define MAILBOX_MIF_INIT                (0x004c)
#define MAILBOX_IS_VERSION              (0x0070)
#define MAILBOX_ISSR0                   (0x0100)
#define MAILBOX_ISSR1                   (0x0104)
#define MAILBOX_ISSR2                   (0x0108)
#define MAILBOX_ISSR3                   (0x010C)
#define MAILBOX_ISSR4                   (0x0110)
#define MAILBOX_ISSR5                   (0x0114)

/* MAILBOX_MCUCTRL */
#define MAILBOX_MSWRST_OFFSET		(0)
#define MAILBOX_MSWRST_SIZE		(1)
/* MAILBOX_INT*R0 */
#define MAILBOX_INT0_OFFSET		(0)
#define MAILBOX_INT0_SIZE		(16)
/* MAILBOX_INT*R1 */
#define MAILBOX_INT1_OFFSET		(16)
#define MAILBOX_INT1_SIZE		(16)
/* MAILBOX_SEMA?CON */
#define MAILBOX_INT_EN_OFFSET		(3)
#define MAILBOX_INT_EN_SIZE		(1)
/* MAILBOX_SEMA?STATE */
#define MAILBOX_LOCK_OFFSET		(0)
#define MAILBOX_LOCK_SIZE		(1)

#endif /* __MAILBOX_H */
