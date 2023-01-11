/*
 * Copyright (c) [2009-2013] Marvell International Ltd. and its affiliates.
 * All rights reserved.
 * This software file (the "File") is owned and distributed by Marvell
 * International Ltd. and/or its affiliates ("Marvell") under the following
 * licensing terms.
 * If you received this File from Marvell, you may opt to use, redistribute
 * and/or modify this File in accordance with the terms and conditions of
 * the General Public License Version 2, June 1991 (the "GPL License"), a
 * copy of which is available along with the File in the license.txt file
 * or by writing to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA 02111-1307 or on the worldwide web at
 * http://www.gnu.org/licenses/gpl.txt. THE FILE IS DISTRIBUTED AS-IS,
 * WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY
 * DISCLAIMED. The GPL License provides additional details about this
 * warranty disclaimer.
 */

#include "tee_cm_internal.h"
#include "tee_cm.h"

/* variables */
void *in_header;
void *in_buf;		/* rb data buffer */
uint32_t in_buf_size;
void *out_header;
void *out_buf;		/* rc data buffer */
uint32_t out_buf_size;
/* if wr idx wrap around and behind rd idx, this is min space to reserved */
#define BAR_SIZE	(4)

/*** facility functions/macros ***/
extern int32_t *_cm_in_read_idx(void);
extern int32_t *_cm_in_write_idx(void);
extern int32_t *_cm_out_read_idx(void);
extern int32_t *_cm_out_write_idx(void);
#define IN_RD_IDX (*_cm_in_read_idx())		/* RB, read_index */
#define IN_WR_IDX (*_cm_in_write_idx())		/* RB, wirte_index */
#define OUT_RD_IDX (*_cm_out_read_idx())	/* RC, read_index */
#define OUT_WR_IDX (*_cm_out_write_idx())	/* RC, write_index */

extern void cm_call_smi(void *arg);

extern int32_t _cm_get_out_write_idx(void);
extern void _cm_update_out_write_idx(int32_t idx);

/* get out empty size */
uint32_t _out_empty_size(void)
{
	uint32_t ret = 0;
	int32_t delta = OUT_WR_IDX - OUT_RD_IDX;
	if (delta >= 0)	/* data size */
		ret = out_buf_size - (delta) - BAR_SIZE;
	else		/* data size */
		ret = out_buf_size - (delta + out_buf_size) - BAR_SIZE;

	return ret;
}

/* get in data size */
uint32_t _in_data_size(void)
{
	uint32_t ret = 0;
	int32_t delta = IN_WR_IDX - IN_RD_IDX;
	if (delta >= 0)
		ret = delta;
	else
		ret = in_buf_size - delta - BAR_SIZE;

	return ret;
}

/* out writing */
void _out_write(int8_t *buf, uint32_t len, int32_t *idx)
{
	uint32_t end;

	OSA_ASSERT(len < out_buf_size);

	end = (uint32_t)(*idx) + len;
	if (end > out_buf_size) {
		uint32_t remained_sz;
		uint32_t current_sz;

		remained_sz = end - out_buf_size;
		current_sz = out_buf_size - (uint32_t)(*idx);
		memcpy(out_buf + *idx, buf, current_sz);
		memcpy(out_buf, buf + current_sz, remained_sz);
		*idx = remained_sz;
	} else {
		memcpy(out_buf + *idx, buf, len);
		*idx = end;
	}
}

/* in reading */
void _in_read(int8_t *buf, uint32_t len, int32_t *idx)
{
	uint32_t end;

	OSA_ASSERT(len < in_buf_size);

	end = (uint32_t)(*idx) + len;
	if (end > in_buf_size) {
		uint32_t remained_sz;
		uint32_t current_sz;

		remained_sz = end - in_buf_size;
		current_sz = in_buf_size - (uint32_t)(*idx);
		memcpy(buf, in_buf + *idx, current_sz);
		memcpy(buf + current_sz, in_buf, remained_sz);
		*idx = remained_sz;
	} else {
		memcpy(buf, in_buf + *idx, len);
		*idx = end;
	}
}

bool tee_cm_send_data(uint8_t *buf)
{
	bool ret = true;
	uint32_t size;
	/* int32_t idx = OUT_WR_IDX; */
	int32_t idx;
	tee_msg_head_t *tee_msg_head;

	tee_msg_head = (tee_msg_head_t *)buf;

	size = sizeof(tee_msg_head_t) + tee_msg_head->msg_sz;

#if 0	/* zyq 20130207, find the bugs with RB and RC */
	/* check if there is enough space */
	if (_out_empty_size() < size)
		return false;
#endif

	idx = _cm_get_out_write_idx();

	/* write caller's data */
	_out_write((int8_t *)buf, size, &idx);

	/* set out write index */
	_cm_update_out_write_idx(idx);
	/* OUT_WR_IDX = idx; */

	return ret;
}

extern uint32_t pr(int8_t *fmt, ...);
uint32_t tee_cm_get_data_size(void)
{
	uint32_t size_in_data = _in_data_size();
	tee_msg_head_t tee_msg_head;
	int8_t magic[4];
	uint32_t size;
	int32_t idx = IN_RD_IDX;

	if (size_in_data < 4)
		return 0;

	_in_read(magic, 4, &idx);
	idx = IN_RD_IDX;

	if (size_in_data < sizeof(tee_msg_head_t))
		return 0;

	_in_read((int8_t *)(&tee_msg_head), sizeof(tee_msg_head_t), &idx);
	size = sizeof(tee_msg_head_t) + tee_msg_head.msg_sz;

	return size;
}

void tee_cm_recv_data(uint8_t *buf)
{
	tee_msg_head_t *tee_msg_head = (tee_msg_head_t *)buf;
	int32_t idx = IN_RD_IDX;

	_in_read((int8_t *)buf, sizeof(tee_msg_head_t), &idx);

	/* output data */
	_in_read((int8_t *)buf + sizeof(tee_msg_head_t), tee_msg_head->msg_sz, &idx);

	/* update rb read index */
	IN_RD_IDX = idx;
}

void tee_cm_get_recv_buf(void **head)
{
	tee_msg_head_t tee_msg_head;
	int32_t idx = IN_RD_IDX;

	_in_read((int8_t *)(&tee_msg_head), sizeof(tee_msg_head_t), &idx);

	*head = tee_msg_head.tee_msg;
}

void tee_cm_smi(uint32_t flag)
{
	tee_cm_smi_input_t tee_cm_smi_input;

	tee_cm_smi_input.arg0 = flag;
	tee_cm_smi_input.arg1 = 0;
	tee_cm_smi_input.arg2 = 0;

	cm_call_smi(&tee_cm_smi_input);

	return;
}

void tee_cm_get_msgm_head(void *msg_head)
{
	int32_t idx = IN_RD_IDX;

	OSA_ASSERT(msg_head);

	_in_read((int8_t *)msg_head, sizeof(tee_msg_head_t), &idx);

	return;
}
