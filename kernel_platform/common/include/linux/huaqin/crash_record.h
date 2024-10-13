/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Dump Display
 */
#ifndef CRASH_RECORD_H
#define CRASH_RECORD_H

void set_crash_info_main_reason(const char *str);
void record_backtrace_msg(void);
void set_crash_info_sub_reason(const char *str);

#endif
