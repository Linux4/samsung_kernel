/*
 * Copyright (C) 2014 Spreadtrum Communications Inc.
 *
 * Authors:<jinglong.chen@spreadtrum.com>
 * Owners:
 *      hua.chen jinglong.chen
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
#include "wlan_msg_q.h"

#define TCP_ACK_WIN_TIMEOUT      (500)
#define TCP_ACK_WIN_H_SIZE       (23168)
#define TCP_ACK_WIN_L_SIZE       (20272)
#define TCP_SESSION_TIMEOUT      (5)

int time_d_value(struct timeval *start, struct timeval *end)
{
	return (end->tv_sec - start->tv_sec) * 1000000 + (end->tv_usec -
							  start->tv_usec);
}

static int get_ip_hdr(unsigned char *frame, int len, unsigned char **ip_hdr)
{
	int ip_hdr_len;
	*ip_hdr = frame + 14;
	ip_hdr_len  = ((*ip_hdr)[0]&0x0F) * 4;
	return ip_hdr_len;
}

static int get_tcp_hdr(unsigned char *frame, int len, unsigned char **tcp_hdr)
{
	int ip_hdr_len, tcp_hdr_len;
	ip_hdr_len  = (frame[14]&0x0F) * 4;
	*tcp_hdr = frame + 14 + ip_hdr_len;
	tcp_hdr_len = ((*tcp_hdr)[12]>>4) * 4;
	return tcp_hdr_len;
}

static bool is_tcp_data(unsigned char *frame, int len)
{
	int ip_hdr_len, tcp_hdr_len;
	unsigned char *ip_hdr, *tcp_hdr;
	if (len <= 54)
		return false;
	if (!(0x08 == frame[12] && 0x0 == frame[13]))	/*IP*/
		return false;
	if (!(0x06 == frame[23]))	/*TCP*/
		return false;

	ip_hdr_len = get_ip_hdr(frame, len, &ip_hdr);
	tcp_hdr_len = get_tcp_hdr(frame, len, &tcp_hdr);

	if ((len - (14 + ip_hdr_len + tcp_hdr_len)) <= 0)
		return false;

	return true;
}

static bool is_tcp_ack(unsigned char *frame, int len)
{
	int ip_hdr_len, tcp_hdr_len;
	unsigned char *ip_hdr, *tcp_hdr;

	if (!(0x08 == frame[12] && 0x0 == frame[13]))	/*IP*/
		return false;
	if (!(0x06 == frame[23]))	/*TCP*/
		return false;

	ip_hdr_len = get_ip_hdr(frame, len, &ip_hdr);
	tcp_hdr_len = get_tcp_hdr(frame, len, &tcp_hdr);

	if ((len - (14 + ip_hdr_len + tcp_hdr_len)) != 0)
		return false;

	return true;
}

static bool ack_no_loss(unsigned char *frame, int len)
{
	if ((54 == len) && (0x10 != frame[0x2f]))
		return true;
	if (len > 54)
		return true;
	return false;
}

static unsigned int get_data_seq(unsigned char *frame, unsigned int len)
{
	unsigned char *tcp_hdr;
	unsigned int seq;
	unsigned char seq1[4];

	get_tcp_hdr(frame, len, &tcp_hdr);

	seq1[3] = tcp_hdr[4];
	seq1[2] = tcp_hdr[5];
	seq1[1] = tcp_hdr[6];
	seq1[0] = tcp_hdr[7];
	memcpy((char *)(&seq), &seq1[0], 4);
	CLEAR_BIT(seq, 31);
	return seq;
}

static unsigned int get_ack_seq(unsigned char *frame, unsigned int len)
{
	unsigned char *tcp_hdr;
	unsigned int   seq;
	unsigned char seq1[4];

	get_tcp_hdr(frame, len, &tcp_hdr);

	seq1[3] = tcp_hdr[8];
	seq1[2] = tcp_hdr[9];
	seq1[1] = tcp_hdr[10];
	seq1[0] = tcp_hdr[11];
	memcpy((char *)(&seq), &seq1[0], 4);
	CLEAR_BIT(seq, 31);
	return seq;
}

static unsigned int tid_calc(unsigned char *frame, unsigned int len)
{
	unsigned int src_ip, dst_ip, tid;
	unsigned short src_port, dst_port;
	unsigned char  *tcp_hdr;

	memcpy((char *)(&src_ip),   &frame[0x1a], 4);
	memcpy((char *)(&dst_ip),   &frame[0x1e], 4);

	get_tcp_hdr(frame, len, &tcp_hdr);

	memcpy((char *)(&src_port), &(tcp_hdr[0]), 2);
	memcpy((char *)(&dst_port), &(tcp_hdr[2]), 2);

	tid = (src_ip ^ dst_ip^dst_port^src_port);
	return tid;
}

static void tcp_session_del(wlan_tcp_session_t  *session)
{
	printkd("DEL TID:0x%x\n", session->tid);
	memset((char *)session, 0,
	       sizeof(wlan_tcp_session_t) - sizeof(msg_q_t));
	return;
}

void tcp_session_updata(const unsigned char vif_id, unsigned char *frame,
			unsigned int len)
{
	unsigned int tid, seq;
	int i, id;
	wlan_vif_t *vif;
	wlan_tcp_session_t *session;
	struct timeval cur_time;
	if (false == g_wlan.netif[vif_id].tcp_ack_suppress)
		return;
	if (false == is_tcp_data(frame, len)) {
		return;
	}
	tid = tid_calc(frame, len);
	seq = get_data_seq(frame, len);
	vif = &(g_wlan.netif[vif_id]);
	do_gettimeofday(&cur_time);
	for (i = 0, id = -1; i < MAX_TCP_SESSION; i++) {
		if ((1 == vif->tcp_session[i].active)
		    && (tid == vif->tcp_session[i].tid)) {
			id = i;
			break;
		}

	}
	if (-1 == id) {
		for (i = 0; i < MAX_TCP_SESSION; i++) {
			if (1 == vif->tcp_session[i].active) {
				if ((cur_time.tv_sec -
				     vif->tcp_session[i].data_time.tv_sec) >
				    TCP_SESSION_TIMEOUT) {
					tcp_session_del(&vif->tcp_session[i]);
					break;
				}
			} else {
				break;
			}
		}
		if (MAX_TCP_SESSION == i)
			return;
		vif->tcp_session[i].tid = tid;
		vif->tcp_session[i].ack_seq = seq;
		vif->tcp_session[i].ack_time = cur_time;
		vif->tcp_session[i].active = 1;
		id = i;
		printkd("NEW TID:0x%x\n", tid);
	} else {
		if ((cur_time.tv_sec - vif->tcp_session[id].data_time.tv_sec) >
		    TCP_SESSION_TIMEOUT) {
			tcp_session_del(&vif->tcp_session[id]);
			return;
		}
	}
	vif->tcp_session[id].data_time = cur_time;
	vif->tcp_session[id].data_seq  = seq;
	return;
}

msg_q_t *wlan_tcpack_q(wlan_vif_t *vif, unsigned char *frame, unsigned int len)
{
	unsigned int tid, i;
	msg_q_t      *msg_q = &(vif->msg_q[1]);

	if (vif->tcp_ack_suppress == false)
		return msg_q;
	if (is_tcp_ack(frame, len) == false)
		return msg_q;
	tid = tid_calc(frame, len);
	for (i = 0; i < MAX_TCP_SESSION; i++) {
		if ((vif->tcp_session[i].active == 1)
		    && (tid == vif->tcp_session[i].tid)) {
			msg_q = &(vif->tcp_session[i].msg_q);
			break;
		}
	}
	return msg_q;
}

int wlan_tcpack_tx(wlan_vif_t *vif, int *done)
{
	int i, ack_cnt, usec, time_dt, seq_dt, index, retry, len, send_pkt;
	unsigned int   seq;
	tx_msg_t      *msg;
	msg_q_t       *msg_q;
	struct timeval cur_time;
	wlan_tcp_session_t *session;
	txfifo_t      *tx_fifo;
	unsigned char *frame;

	retry = *done = 0;
	tx_fifo = &(vif->txfifo);
	do_gettimeofday(&cur_time);

	for (index = 0; index < MAX_TCP_SESSION; index++) {
		session = &(vif->tcp_session[index]);
		msg_q = &(session->msg_q);
		ack_cnt = msg_num(msg_q);
		if (0 == ack_cnt)
			continue;
		usec = time_d_value(&(session->ack_time), &cur_time);
		time_dt =
		    (session->data_seq >=
		     session->ack_seq) ? (session->data_seq -
					  session->ack_seq) : 0;
		if ((1 == session->active) && (time_dt < TCP_ACK_WIN_H_SIZE)
		    && (usec < TCP_ACK_WIN_TIMEOUT)) {
			retry++;
			continue;
		}
		for (i = 0, send_pkt = 0; i < ack_cnt; i++) {
			msg = msg_get(msg_q);
			if (NULL == msg)
				break;
			frame = msg->slice[0].data;
			len = msg->slice[0].len;
			seq = get_ack_seq(frame, len);
			seq_dt = seq - session->ack_seq;

			if ((i > (ack_cnt - 2))
			    || (seq_dt >= TCP_ACK_WIN_L_SIZE)
			    || (0 == session->active)
			    || (true == ack_no_loss(frame, len))) {
				if (TX_FIFO_FULL == tx_fifo_in(tx_fifo, msg)) {
					retry++;
					continue;
				}
				session->ack_seq = seq;
				do_gettimeofday(&(session->ack_time));
				send_pkt++;
			}
			(*done)++;
			dev_kfree_skb((struct sk_buff *)(msg->p));
			msg_free(msg_q, msg);
		}
		for (i = 0; i < send_pkt; i++) {
			trans_up();
		}
	}
	if (retry > 0)
		return ERROR;
	return OK;
}

int wlan_tcpack_buf_malloc(wlan_vif_t *vif)
{
	int i;
	for (i = 0; i < MAX_TCP_SESSION; i++) {
		msg_q_alloc(&(vif->tcp_session[i].msg_q), sizeof(tx_msg_t),
			    100);
	}
	return OK;
}

int wlan_tcpack_buf_free(wlan_vif_t *vif)
{
	int i;
	for (i = 0; i < MAX_TCP_SESSION; i++) {
		msg_q_free(&(vif->tcp_session[i].msg_q));
	}
	return OK;
}

int wlan_rx_buf_decode(unsigned char *buf, unsigned int max_len)
{
	unsigned int p = 0;
	r_msg_hdr_t *msg = NULL;
	unsigned char vif_id;
	unsigned char *frame = NULL;
	unsigned short len;
	unsigned char event;
	if ((NULL == buf) || (0 == max_len)) {
		printke("[%s][ERROR]\n", __func__);
		return OK;
	}
	buf = buf + 8;
	msg = (r_msg_hdr_t *) (buf);
	max_len = max_len - 8;
	while (p < max_len) {

		vif_id = msg->mode;
		frame = (unsigned char *)(msg + 1);
		len = msg->len;
		if ((0xFF == msg->type) || (0xFF == msg->subtype))
			break;
		if (HOST_SC2331_PKT == msg->type) {
			frame = frame + msg->subtype;
			len = len - msg->subtype;
			tcp_session_updata(vif_id, frame, len);
		}
		p = p + sizeof(t_msg_hdr_t) + ALIGN_4BYTE(msg->len);
		msg = (r_msg_hdr_t *) (buf + p);
	}
	return OK;

}

int wlan_tx_buf_decode(unsigned char *buf, unsigned int max_len)
{
	int i, len;
	tx_big_hdr_t *big_hdr = buf;
	t_msg_hdr_t *hdr;
	unsigned char *frame;
	hdr = (t_msg_hdr_t *) (big_hdr + 1);
	for (i = 0; i < big_hdr->msg_num; i++) {
		if ((hdr->type > 2) || (hdr->subtype > 47))
			break;
		if (HOST_SC2331_PKT == hdr->type) {
			frame = (char *)(hdr) + 38;
			len = hdr->len;
		}
		hdr = TX_MSG_NEXT_MSG(hdr);
	}
	return 0;
}
