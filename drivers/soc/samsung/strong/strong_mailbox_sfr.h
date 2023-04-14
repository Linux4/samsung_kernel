/*
 * strong_mailbox_sfr.h - Samsung Strong Mailbox driver for the Exynos
 *
 * Copyright (C) 2021 Samsung Electronics
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __STRONG_MAILBOX_SFR_H__
#define __STRONG_MAILBOX_SFR_H__

/*****************************************************************************/
/* IPC									     */
/*****************************************************************************/

#define STR_MAX_MB_DATA_LEN		(((64 - 10) / 2) * WORD_SIZE)

/*****************************************************************************/
/* Mailbox                                                                   */
/*****************************************************************************/
#define STR_MB_BASE			(0x18240000) /* Mailbox_NS */

#define STR_MB_INTGR0_OFFSET		(0x08)
#define STR_MB_INTCR0_OFFSET		(0x0C)
#define STR_MB_INTMR0_OFFSET		(0x10)
#define STR_MB_INTSR0_OFFSET		(0x14)
#define STR_MB_INTGR1_OFFSET		(0x1C)
#define STR_MB_INTCR1_OFFSET		(0x20)
#define STR_MB_INTMR1_OFFSET		(0x24)
#define STR_MB_INTSR1_OFFSET		(0x28)

#define STR_MB_INTGR0_ON                (1 << 16)
#define STR_MB_INTGR1_ON                (1 << 0)
#define STR_MB_INTGR0_DBG_ON            (1 << 17)

/* REE's write CMD */
#define STR_MB_CTRL_00_OFFSET_R2S		(0x100)
#define STR_MB_CTRL_01_OFFSET_R2S		(0x104)
#define STR_MB_CTRL_02_OFFSET_R2S		(0x108)
#define STR_MB_CTRL_03_OFFSET_R2S		(0x10C)

/* SEE's write CMD */
#define STR_MB_CTRL_00_OFFSET_S2R		(0x110)
#define STR_MB_CTRL_01_OFFSET_S2R		(0x114)
#define STR_MB_CTRL_02_OFFSET_S2R		(0x118)
#define STR_MB_CTRL_03_OFFSET_S2R		(0x11C)

/* REE's ret CMD */
#define STR_MB_CTRL_04_OFFSET_R2S		(0x120)
/* SEE's ret CMD */
#define STR_MB_CTRL_04_OFFSET_S2R		(0x124)

#define STR_MB_DATA_00_OFFSET		(0x128)
#define STR_MB_DATA_00_OFFSET_R2S	(STR_MB_DATA_00_OFFSET)
#define STR_MB_DATA_00_OFFSET_S2R	(STR_MB_DATA_00_OFFSET + STR_MAX_MB_DATA_LEN)

/* REE debug sfr */
#define STR_MB_DATA_DEBUG_START_OFFSET  (STR_MB_DATA_00_OFFSET_R2S + STR_MAX_MB_DATA_LEN - 4) /* ~ 0x200 */
#define STR_MB_DATA_DEBUG_END_OFFSET    (STR_MB_DATA_00_OFFSET_R2S) /* ~ 0x200 */
#endif /* __STRONG_MAILBOX_SFR_H__ */
