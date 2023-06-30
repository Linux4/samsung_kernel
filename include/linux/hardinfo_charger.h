/*
 * Huaqin  Inc. (C) 2011. All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __HARDINFO_CHARGER_H__
#define __HARDINFO_CHARGER_H__

#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/fs.h>

enum hardware_id {
    CHG_INFO = 0,
    TCPC_INFO,
};

extern void set_hardinfo_charger_data(enum hardware_id id, const void *data);


#endif