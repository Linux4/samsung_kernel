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
#include "wlan_event_q.h"

static void update_buff_status(unsigned char *buff, unsigned short index,
			       unsigned char status)
{
	unsigned char offset = 0;
	offset = (index & 0x3) << 1;
	buff += (index >> 2);
	*buff = (status << offset) | (*buff & (~(0x3 << offset)));
}

static unsigned char check_buff_status(unsigned char *buff,
				       unsigned short index)
{
	unsigned char offset = 0;
	unsigned char status = 0;
	offset = (index & 0x3) << 1;
	buff += (index >> 2);
	status = (*buff >> offset) & 0x3;
	return status;
}

void *alloc_event(m_event_t *mEventQ)
{
	unsigned short index = 0;
	unsigned char *free_event_buff = 0;
	if (mEventQ->event_cnt == mEventQ->max_events) {
		return NULL;
	}
	spin_lock_bh(&(mEventQ->spinlock));
	index = INCR_RING_BUFF_INDX(mEventQ->tail_index, mEventQ->max_events);
	if (check_buff_status(mEventQ->buf_status, index) == EVENT_BUFFER_FREE) {
		mEventQ->tail_index = index;
		update_buff_status(mEventQ->buf_status, index,
				   EVENT_BUFFER_ALLOC);
		free_event_buff =
		    (mEventQ->event_buf + (index * mEventQ->event_size));
	}
	spin_unlock_bh(&(mEventQ->spinlock));
	return free_event_buff;
}

void post_event(unsigned char *event, m_event_t *mEventQ)
{
	unsigned short index = 0;
	spin_lock_bh(&(mEventQ->spinlock));
	index =
	    ((unsigned int)event -
	     (unsigned int)mEventQ->event_buf) / mEventQ->event_size;
	update_buff_status(mEventQ->buf_status, index, EVENT_BUFFER_VALID);
	mEventQ->event_cnt++;
	spin_unlock_bh(&(mEventQ->spinlock));
}

unsigned char *get_event(m_event_t *mEventQ)
{
	unsigned char status = 0;
	unsigned char *head_event = NULL;
	if (mEventQ->event_cnt == 0)
		return NULL;
	status = check_buff_status(mEventQ->buf_status, mEventQ->head_index);
	if (status == EVENT_BUFFER_VALID) {
		head_event =
		    (mEventQ->event_buf) +
		    (mEventQ->head_index * mEventQ->event_size);
	}
	return head_event;
}

void free_event(void *event, m_event_t *mEventQ)
{
	spin_lock_bh(&(mEventQ->spinlock));
	update_buff_status(mEventQ->buf_status, mEventQ->head_index,
			   EVENT_BUFFER_FREE);
	mEventQ->head_index =
	    INCR_RING_BUFF_INDX(mEventQ->head_index, mEventQ->max_events);
	mEventQ->event_cnt--;
	spin_unlock_bh(&(mEventQ->spinlock));
}

int event_q_init(m_event_t *mEventQ, m_event_conf_t *conf)
{
	mEventQ->event_size = conf->event_size;
	mEventQ->max_events = conf->max_events;
	mEventQ->tail_index = conf->max_events - 1;
	mEventQ->head_index = 0;
	mEventQ->highThres = conf->highThres;
	mEventQ->lowThres = conf->lowThres;
	mEventQ->weight = conf->weight;

	mEventQ->buf_status =
	    kmalloc(EVENT_TOTAL_MEM_SIZE
		    ((mEventQ->max_events), (mEventQ->event_size)), GFP_KERNEL);
	if (NULL == mEventQ->buf_status) {
		ASSERT();
		return ERROR;
	}
	memset(mEventQ->buf_status, 0,
	       EVENT_TOTAL_MEM_SIZE((mEventQ->max_events),
				    (mEventQ->event_size)));
	mEventQ->event_buf =
	    (unsigned char
	     *)(WORD_ALIGN((unsigned int)(mEventQ->buf_status +
					  EVENT_STATUS_SIZE(mEventQ->
							    max_events))));
	spin_lock_init(&(mEventQ->spinlock));
	return OK;
}

int event_q_deinit(m_event_t *mEventQ)
{
	if (NULL != mEventQ->buf_status)
		kfree(mEventQ->buf_status);
	memset((unsigned char *)mEventQ, 0, sizeof(m_event_t));
	return OK;
}

