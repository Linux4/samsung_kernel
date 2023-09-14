/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 * Authors:<jinglong.chen@spreadtrum.com>
 * Owner:
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

#include "wlan_common.h"

int fq_table[5][4] =
{
	{0,  0,  0,  0},
	{8,  0,  0,  0},
	{1,  1,  0,  0},
	{1,  1,  1,  0},
	{1,  1,  1,  1},
};

int wfq_table[5][4] = 
{
	{0,  0,  0,  0  },
	{10, 0,  0,  0  },
	{20, 10, 0,  0  },
	{30, 20, 10, 0  },
	{40, 30, 20, 10 },
};

int fd_special_table[2][2] =
{
	{15, 1},
	{15, 1},
};

void qos_enable(qos_t *qos, int flag)
{
	qos->enable = flag;
}

char qos_match_q(qos_t *qos, unsigned char *frame, unsigned int len)
{
	unsigned short  eth_type;
	int             priority;
	int    q_id = MSG_Q_ID_0;

	if(0 == qos->enable)
		return q_id;
	/* vo vi bk be*/
	eth_type  = ( (frame[12]<<8) | frame[13] );
	if(IP_TYPE != eth_type)
		goto OUT;
	priority = frame[15]&0xE0;
	switch(priority)
	{
		case 0x20:
		case 0x40:
			q_id = MSG_Q_ID_3;
			break;
		case 0x80:
		case 0xA0:
			q_id = MSG_Q_ID_1;
			break;
		case 0xc0:
		case 0xe0:
			q_id = MSG_Q_ID_0;
			break;
		default:
			q_id = MSG_Q_ID_2;
			break;
	}
	
OUT:	
	return q_id;
}

void qos_wfq(qos_t *qos)
{
	int t,i,weight,q[4] = {0};
	for(i=0, t=0, weight=0; i<4; i++)
	{
		if( msg_num(qos->msg_q + i) > 0)
		{
			q[t] = i;
			t++;
		}
	}
	if(0 == t)
		return;
	for(i=0; i<t; i++)
	{
		weight += wfq_table[t][i];
	}
	for(i=0; i<t; i++)
	{
		qos->going[q[i]] = wfq_table[t][q[i]]*msg_num(qos->msg_q + q[i])/weight;
	}
}

void qos_fq(qos_t *qos)
{	
	int i,j,t,k,q[4] = {0};
	for(i=0,t=0; i<4; i++)
	{
		if( msg_num(qos->msg_q + i) > 0)
		{
			q[t] = i;
			t++;
		}
	}
	if(0 == t)
		return;
	/* vi & bk*/
	if( (2 == t) && (1 == q[0]) && (2 == q[1]) && (1 != qos->wmm_report_flag) )
	{
		qos->going[1] = fd_special_table[0][0];
		qos->going[2] = fd_special_table[0][1];
		if( msg_num(qos->msg_q + 1) < qos->going[1])
			qos->going[1] = msg_num(qos->msg_q + 1);
		if( msg_num(qos->msg_q + 2) < qos->going[2])
			qos->going[2] = msg_num(qos->msg_q + 2);
		return;
	}
	/*bk & be*/
	if( (2 == t) && (2 == q[0]) && (3 == q[1]) )
	{
		qos->going[2] = fd_special_table[1][0];
		qos->going[3] = fd_special_table[1][1];
		
		if( msg_num(qos->msg_q + 2) < qos->going[2])
			qos->going[2] = msg_num(qos->msg_q + 2);
		if( msg_num(qos->msg_q + 3) < qos->going[3])
			qos->going[3] = msg_num(qos->msg_q + 3);
		return;
	}
	
	for(i=0; i<t; i++)
	{
		k = 0;
		qos->going[q[i]] = fq_table[t][i];
		if( msg_num(qos->msg_q + q[i]) < qos->going[q[i]] )
		{
			k = msg_num(qos->msg_q + q[i]);
			qos->going[q[i]] = k;
		}
	}
}

void qos_sched(qos_t *qos, msg_q_t **q, int *num)
{
	int i, round;
	msg_q_t *msg_q = NULL;
	if(0 == qos->enable)
	{
		*q       = qos->msg_q;
		*num     = msg_num(qos->msg_q);
		return;
	}
	for(round=0;  round <4; round++)
	{
		if( (MSG_Q_ID_0 == qos->index) && (0 == qos->going[MSG_Q_ID_0]) )
			qos_fq(qos);
		if(qos->going[qos->index] > 0)
			break;
		qos->index = INCR_RING_BUFF_INDX(qos->index, 4);
	}
	*q       = qos->msg_q + qos->index;
	*num     = qos->going[qos->index];
}

