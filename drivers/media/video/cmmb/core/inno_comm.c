/*
 * innofidei if2xx demod spi communication driver
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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/suspend.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/list.h>
#include <asm/uaccess.h>
#include "inno_core.h"
#include "inno_cmd.h"
#include "inno_reg.h"
#include "inno_comm.h"

#ifdef INNO_DEBUG_LOGTOFILE
extern unsigned char inno_log_buf[];
extern void inno_logfile_write(unsigned char*in_buf);
#undef pr_debug
#undef pr_err
#define pr_debug(fmt, ...) \
		{sprintf(inno_log_buf,"INFO:[INNO/core/inno_comm.c]" pr_fmt(fmt),##__VA_ARGS__); \
				inno_logfile_write(inno_log_buf);}
#define pr_err(fmt, ...) \
		{sprintf(inno_log_buf,"ERR:[INNO/core/inno_comm.c]" pr_fmt(fmt),##__VA_ARGS__); \
				inno_logfile_write(inno_log_buf);}
#else

#undef pr_debug
#define pr_debug(fmt, ...) \
printk(KERN_DEBUG "[INNO/core/inno_comm.c]" pr_fmt(fmt),##__VA_ARGS__)

#undef pr_err
#define pr_err(fmt, ...) \
printk(KERN_ERR "[INNO/core/inno_comm.c]" pr_fmt(fmt), ##__VA_ARGS__)

#endif
/**
 * inno_comm_cmd_idle -check if previous cmd had been done
 */
int inno_comm_cmd_idle(unsigned char *pstatus)
{
        int i;
        for(i = 0; i < 500; i++) {
                inno_comm_get_unit(FETCH_PER_COMM31, pstatus, 1);
                if((*pstatus & CMD_BUSY) == 0)   //bit7:1 cmd busy; 0 cmd over
                        return 1;
        }
		
        inno_comm_get_unit(0x4000401c,(unsigned char *)&i, 4);
		pr_debug("0x4000401c=%08x\n",i);
        inno_comm_get_unit(0x5000215c,(unsigned char *)&i, 4);
		pr_debug("0x5000215c=%08x\n",i);
        return 0;
}
EXPORT_SYMBOL_GPL(inno_comm_cmd_idle);

/**
 * inno_comm_send_cmd -send cmd and cmd data to if20x
 */ 
int inno_comm_send_cmd(struct inno_cmd_frame *cmd_frame)
{
	unsigned char cmd_status = 0;
	int ret =0;

	if(inno_comm_cmd_idle(&cmd_status))
	{
	        /* send data to data reg*/
	        inno_comm_send_unit(FETCH_PER_COMM1, cmd_frame->data, 7);

		/* set busy status */
		cmd_status = CMD_BUSY;
		inno_comm_send_unit(FETCH_PER_COMM31, &cmd_status, 1);
		 
	        /* send cmd to cmd reg*/
	        inno_comm_send_unit(FETCH_PER_COMM0, &cmd_frame->code, 1);
        }
	else
		ret = -EBUSY;

	return ret;
}
EXPORT_SYMBOL_GPL(inno_comm_send_cmd);

/**
 * inno_comm_get_rsp -get rsp from if2xx
 */
int inno_comm_get_rsp(struct inno_rsp_frame *rsp_frame)
{
	unsigned char status;
	int ret =0;

	if(inno_comm_cmd_idle(&status))
	{
		/* RSP_DATA_OK: 0x00 RSP_DATA_ERR:0x01 */
		if((status & 0x7F) == 0) 
		{
			/* get rsp data from rsp reg */
			ret = inno_comm_get_unit(FETCH_PER_COMM8, (unsigned char *)rsp_frame, 8);
		} 
		else 
		{
			pr_err("\r\n rsp error = 0x%x\r\n", status&0x7f);
                     ret = -EIO;
		}
	
	}
	else
		ret = -EBUSY;

	return ret;
}
EXPORT_SYMBOL_GPL(inno_comm_get_rsp);
