/**
 * @file wt_sys_config.h
 *
 * Add for wt sys config file
 *
 * Copyright (C) 2020 Wingtech.Co.Ltd. All rights reserved.
 *
 * =============================================================================
 *                             EDIT HISTORY
 *
 * when         who          what, where, why
 * ----------   --------     ---------------------------------------------------
 * 2020/03/10   wanghui      Initial Revision
 *
 * =============================================================================
 *
 */
#ifndef _WT_SYS_CONFIG_H
#define _WT_SYS_CONFIG_H

#ifndef WT_SHARE_MEMORY_ADDR
#define WT_SHARE_MEMORY_ADDR         0x68000000
#endif

#define WT_SYSTEM_MONITOR_ADDR       (WT_SHARE_MEMORY_ADDR)
#define WT_SYSTEM_MONITOR_SIZE       0x1000
#define WT_BOOTLOADER_LOG_ADDR       (WT_SYSTEM_MONITOR_ADDR + WT_SYSTEM_MONITOR_SIZE)
#define WT_BOOTLOADER_LOG_SIZE       0x90000

#define WT_BLLOG_ONE_SIZE            0x30000
#define WT_BLLOG_ARRAY_SIZE          4096

#define WT_BOOT_REASON_ADDR          (WT_BOOTLOADER_LOG_ADDR + WT_BOOTLOADER_LOG_SIZE)
#define WT_BOOT_REASON_SIZE          0x10000

#endif

