/*+bug427813, add, zouhaifeng.wt, 20190122, bootloader log transmitted to kernel log*/
#ifndef _WT_BOOTLOADER_LOG_SAVE_H
#define _WT_BOOTLOADER_LOG_SAVE_H

#include <linux/types.h>

#define WT_BOOTLOADER_LOG_MAGIC           0x474C4C42  /* B L L G */
#define WT_PANIC_KEY_LOG_MAGIC            0x474C4B50  /* P K L G */
#define WT_RAMDUMP_MODE_MAGIC             0x4D4D4452  /* R D M M */

struct wt_panic_key_log {
	uint32_t magic;
	uint32_t panic_key_log_size;
	uint8_t reserved[24];
}__attribute__((aligned(8)));

/* please must align in 8 bytes */
struct wt_logbuf_info {
	uint32_t magic;
	uint32_t bootloader_log_size;
	uint64_t bootloader_log_addr;
	uint64_t panic_key_log_addr;
	uint8_t  boot_reason_str[32];
	uint32_t boot_reason_copies;
	uint32_t bootloader_started;
	uint32_t kernel_started;
	uint32_t is_ramdump_mode;
	uint64_t kernel_log_addr;
	uint32_t kernel_log_size;
	uint32_t checksum;
	uint8_t  reserved[40];
}__attribute__((aligned(8)));

//extern int wt_is_ddr_initialized;
//void wt_bootloader_log_save(char *, uint32_t);
//void wt_aboot_bootloader_log_save(uint8_t *, uint32_t);

extern struct wt_logbuf_info *logbuf_head;

int wt_bootloader_log_init(void);
int wt_bootloader_log_handle(void);
void wt_bootloader_log_exit(void);

#endif
/*-bug427813, add, zouhaifeng.wt, 20190122, bootloader log transmitted to kernel log*/
