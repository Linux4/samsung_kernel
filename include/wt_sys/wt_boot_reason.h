/**
 * file wt_boot_reason.h
 *
 * Add for wt boot reason feature
 *
 * Copyright (C) 2020 Wingtech.Co.Ltd. All rights reserved.
 *
 * =============================================================================
 *                             EDIT HISTORY
 *
 * when         who          what, where, why
 * ----------   --------     ---------------------------------------------------
 * 2020/03/13   wanghui      Initial Revision
 *
 * =============================================================================
 *
 */

#ifndef _WT_BOOT_REASON_H
#define _WT_BOOT_REASON_H

#define   RESET_MAGIC_PANIC                         0x43494E50     /* P N I C */
#define   RESET_MAGIC_WDT_BARK                      0x4B524257     /* W B R K */
#define   RESET_MAGIC_THERMAL                       0x4C4D4854     /* T H M L */
#define   RESET_MAGIC_VMREBOOT                      0x42524D56     /* V M R B */
#define   RESET_MAGIC_CMD_REBOOT                    0x42444D43     /* C M D B */
#define   RESET_MAGIC_INIT                          0x54494E49     /* I N I T */
/* SSR */
#define   RESET_MAGIC_IPA_FWS                       0x46415049     /* I P A F */
#define   RESET_MAGIC_VENUS                         0x534E4556     /* V E N S */
#define   RESET_MAGIC_SPSS                          0x53535053     /* S P S S */
#define   RESET_MAGIC_AXXX_ZAP                      0x50415A41     /* A Z A P */
#define   RESET_MAGIC_SLPI                          0x49504C53     /* S L P I */
#define   RESET_MAGIC_ADSP                          0x50534441     /* A D S P */
#define   RESET_MAGIC_CDSP                          0x50534443     /* C D S P */
#define   RESET_MAGIC_WCNSS                         0x534E4357     /* W C N S */
#define   RESET_MAGIC_MODEM                         0x4D444F4D     /* M O D M */
#define   RESET_MAGIC_IPA_UC                        0x55415049     /* I P A U */
#define   RESET_MAGIC_APSS                          0x53535041     /* A P S S */
#define   RESET_MAGIC_CVPSS                         0x53505643     /* C V P S */
#define   RESET_MAGIC_WLAN                          0x4E414C57     /* W L A N */
#define   RESET_MAGIC_ESOC                          0x434F5345     /* E S O C */
#define   RESET_MAGIC_NPU                           0x0055504E     /* N P U */
#define   RESET_MAGIC_SUBSYSTEM                     0x53425553     /* S U B S */
/* USER SPACE ERROR */
#define   RESET_MAGIC_USER_PANIC                    0x41505355     /* U S P A */

#ifdef CONFIG_WT_BOOT_REASON
void wt_btreason_subsystem_failure_log(char *reason, size_t size);
void wt_btreason_set_subsystem_magic(const char *name, int restart_level);
void wt_btreason_set_reset_magic(uint32_t magic_number);
void wt_btreason_clear_reset_magic(void);
void wt_btreason_set_oops(int panic_oops);
void wt_brlog_save_pc_lr_value(uint64_t pc, uint64_t lr);
int wt_btreason_log_save(const char *fmt, ...);
#else
static inline void wt_btreason_subsystem_failure_log(char *reason, size_t size) {}
static inline void wt_btreason_set_subsystem_magic(const char *name, int restart_level) {}
static inline void wt_btreason_set_reset_magic(uint32_t magic_number) {}
static inline void wt_btreason_clear_reset_magic(void) {}
static inline void wt_btreason_set_oops(int panic_oops) {}
static inline void wt_brlog_save_pc_lr_value(uint64_t pc, uint64_t lr) {}
static inline int wt_btreason_log_save(const char *fmt, ...) { return 0; }
#endif

#endif

