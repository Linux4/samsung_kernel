// SPDX-License-Identifier: GPL-2.0
/*
 * dump display
 *
 * Copyright (C) 2021 Huaqin, Inc., Hui yang
 *
 * The main authors of the dump display code are:
 *
 * Hui Yang <yanghui@huaqin.com>
 */
#ifdef CONFIG_RECORD_CRASH_INFO
#ifndef CRASH_RECORD_H
#define CRASH_RECORD_H

void set_crash_info_main_reason(const char *str);
void record_backtrace_msg(void);
void set_crash_info_sub_reason(const char *str);

#endif
#endif
