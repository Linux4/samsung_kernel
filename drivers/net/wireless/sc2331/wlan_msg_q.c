/*
 * Copyright (C) 2014 Spreadtrum Communications Inc.
 *
 * Authors:<jinglong.chen@spreadtrum.com>
 * Owner:
 *      jinglong.chen
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/irqflags.h>
#include <linux/spinlock.h>
#include<linux/slab.h>
#include "wlan_common.h"
#include "wlan_msg_q.h"

int msg_q_in(msg_q_t *msg_q, void *msg)
{
	spin_lock_bh(&(msg_q->wt_lock));
	if ((msg_q->wt + 1) % (msg_q->num) == msg_q->rd) {
		spin_unlock_bh(&(msg_q->wt_lock));
		return ERROR;
	}
	memcpy((msg_q->mem + (msg_q->size) * (msg_q->wt)), (unsigned char *)msg,
	       msg_q->size);
	msg_q->wt = INCR_RING_BUFF_INDX(msg_q->wt, msg_q->num);
	msg_q->wt_cnt++;
	spin_unlock_bh(&(msg_q->wt_lock));
	return OK;
}

unsigned char *msg_get(msg_q_t *msg_q)
{
	unsigned char *msg = NULL;
	if (msg_q->wt == msg_q->rd)
		return NULL;
	msg = msg_q->mem + (msg_q->size) * (msg_q->rd);
	return msg;
}

int msg_free(msg_q_t *msg_q, void *msg)
{
	if (msg_q->rd == msg_q->wt)
		return ERROR;
	msg_q->rd = INCR_RING_BUFF_INDX(msg_q->rd, msg_q->num);
	msg_q->rd_cnt++;
	return OK;
}

int msg_num(msg_q_t *msg_q)
{
	return msg_q->wt_cnt - msg_q->rd_cnt;
}

int msg_q_alloc(msg_q_t *msg_q, unsigned short size, unsigned short num)
{
	memset((char *)msg_q, 0, sizeof(msg_q_t));
	msg_q->mem = kmalloc(size * num, GFP_KERNEL);
	if (NULL == msg_q->mem)
		return ERROR;
	msg_q->num = num;
	msg_q->size = size;
	spin_lock_init(&(msg_q->wt_lock));
	return OK;
}

int msg_q_free(msg_q_t *msg_q)
{
	kfree(msg_q->mem);
	memset((char *)msg_q, 0, sizeof(msg_q_t));
	return OK;
}
