
/*
 * 
 * Copyright (C) 2010 Innofidei Corporation
 * Author:      sean <zhaoguangyu@innofidei.com>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 * 
 */

#ifndef __INNO_COMM_H__
#define __INNO_COMM_H_

//#define INNODEBUG_LOGDETAIL
#define NJG1142

#include "inno_demod.h"

int inno_get_intr_ch(unsigned long *ch_bit);
int inno_lgx_fetch_data(struct lgx_device *lgx);
int inno_comm_init(void);
extern int inno_comm_send_unit(unsigned int addr, unsigned char *buf, int len);
extern int inno_comm_get_unit(unsigned int addr, unsigned char *buf, int len);

struct inno_cmd_frame {
        unsigned char      code;
        unsigned char      data[6];
        unsigned char      checksum;
};

struct inno_rsp_frame {
        unsigned char      code;
        unsigned char      data[6];
        unsigned char      checksum;
};

extern int inno_comm_cmd_idle(unsigned char *pstatus);
extern int inno_comm_get_rsp(struct inno_rsp_frame *rsp_frame);
extern int inno_comm_send_cmd(struct inno_cmd_frame *cmd_frame);
#endif
