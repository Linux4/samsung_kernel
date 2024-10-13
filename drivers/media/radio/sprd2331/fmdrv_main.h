/*
 *  FM Drivr for Connectivity chip of Spreadtrum.
 *
 *  FM driver main module header.
 *
 *  Copyright (C) 2015 Spreadtrum Company
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef _FMDRV_MAIN_H
#define _FMDRV_MAIN_H
#include <linux/fs.h>
int fm_open(struct inode *, struct file *);
int fm_powerup(void *);
int fm_powerdown(void);
int fm_tune(void *);
int fm_seek(void *);
int fm_getrssi(void *);
int fm_mute(void *);
int fm_rds_onoff(void *);
int fm_read_rds_data(struct file *, char *, size_t , loff_t *);
struct fm_rds_data *get_rds_data();
#endif
