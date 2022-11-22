/*
 *  kernel/driver/char/modem_interface/modem_buffer.h
 *
 *  Generic modem interface buffer defination.
 *
 *  Author:     Jiayong Yang(Jiayong.Yang@spreadtrum.com)
 *  Created:    Jul 27, 2012
 *  Copyright:  Spreadtrum Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */
#ifndef _MODEM_BUFFER_H
#define _MODEM_BUFFER_H

#define	RECV_BUFFER_SIZE	(65*1024)
enum   BUF_type_t{
	BUF_SEND,
	BUF_RECV,
};

enum   BUF_status_t{
	BUF_STATUS_IDLE,
	BUF_STATUS_NOEMPTY,
	BUF_STATUS_WRITTEN,
	BUF_STATUS_FULL,
	BUF_STATUS_SENT,
};

struct single_buffer_t{
	char    		*addr;
	int			size;
	int			write_point;
	int			read_point;
	enum BUF_status_t	status;
};

struct modem_buffer{
	struct mutex		mutex;
	struct semaphore	buf_read_sem;
	struct semaphore	buf_write_sem;
	enum BUF_type_t		type;
	int			save_index;
	int			trans_index;
	int			trans_count;
	int			lost_count;
	struct  single_buffer_t buffer[2];
};

extern int  pingpang_buffer_send(struct modem_buffer *buffer);
extern int  pingpang_buffer_init(struct modem_buffer *buffer);
extern void pingpang_buffer_free(struct modem_buffer *buffer);
extern void pingpang_buffer_send_complete(struct modem_buffer *buffer,int index);
extern int  pingpang_buffer_read(struct modem_buffer *buffer,char *data,int size);
extern int  pingpang_buffer_write(struct modem_buffer *buffer,char *data,int size);
extern int  save_to_receive_buffer(struct modem_buffer *buffer,char *data,int size);
#endif
