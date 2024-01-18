/*
 * Copyright (c) 2014-2019 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * Samsung TN common code
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef SEC_COMMON_H
#define SEC_COMMON_H

int seccmn_chrg_set_charging_mode(char value);
int seccmn_recv_is_boot_recovery(void);
void seccmn_exin_set_batt_info(int cap, int volt, int temp, int curr);
#endif
