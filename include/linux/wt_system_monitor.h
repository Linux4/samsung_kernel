/* chk72707, xieshuaishuai, ADD, 20201208, add bootloader log feature to project,start. */
#ifndef _WT_SYSTEM_MONITOR_H
#define _WT_SYSTEM_MONITOR_H

//#define UEFI_BOOT
#define LK_BOOT

//#define WT_SYSTEM_MONITOR
//#define WT_BOOT_REASON
#define WT_BOOTLOADER_LOG

#ifdef WT_SYSTEM_MONITOR
#define WT_BOOT_REASON
#include "wt_system_monitor_log_save.h"
/* /dev/block/bootdevice/by-name/log /dev/block/mmcblk0p58 */
#define LOG_PART_DEV                 "/hdev/sdc1h0p58"
#endif

#ifdef WT_BOOT_REASON
#define WT_BOOTLOADER_LOG
#include "wt_boot_reason.h"
#endif

#ifdef WT_BOOTLOADER_LOG
#include "wt_bootloader_log_save.h"
#if defined(UEFI_BOOT)
#define WT_BOOTLOADER_LOG_ADDR       0xA0000000
#elif defined(LK_BOOT)
#define WT_BOOTLOADER_LOG_ADDR       0x92000000
#endif

#define WT_BOOTLOADER_LOG_SIZE       0x00100000
#define WT_BOOTLOADER_LOG_HALF_SIZE  0x00040000
#define WT_PANIC_KEY_LOG_SIZE        0x00010000
#define WT_PANIC_KEY_LOG_ADDR        (WT_BOOTLOADER_LOG_ADDR + WT_BOOTLOADER_LOG_SIZE - WT_PANIC_KEY_LOG_SIZE)
#endif

#endif
/* chk72707, xieshuaishuai, ADD, 20201208, add bootloader log feature to project,end. */
