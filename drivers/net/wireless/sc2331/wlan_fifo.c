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

#include<linux/slab.h>
#include "wlan_common.h"
#include "wlan_fifo.h"

extern unsigned char *wifi_256k_alloc(void);
extern int wifi_256k_free(unsigned char *mem);

unsigned char *fifo_mem_alloc(unsigned int byte)
{
	unsigned char *mem = NULL;
	if (byte > 16 * 1024) {
		mem = wifi_256k_alloc();
	} else {
		mem = kmalloc(byte, GFP_KERNEL);
		printkd("kmalloc(%d):0x%x\n", byte, (int)mem);
	}
	if (NULL == mem)
		printke("[%s][NULL]\n", __func__);
	return mem;
}

int fifo_mem_free(unsigned char *mem, unsigned int byte)
{
	if (NULL == mem)
		return ERROR;
	if (byte > 16 * 1024) {
		wifi_256k_free(mem);
	} else {
		printkd("kfree:0x%x\n", (int)mem);
		kfree(mem);
	}
	return OK;
}

int tx_fifo_in(txfifo_t *fifo, tx_msg_t *msg)
{
	int ret;
	unsigned int unitLen, rd;
	unsigned char *p;
	t_msg_hdr_t *hdr = &(msg->hdr);
	unitLen = TX_MSG_UNIT_LEN(hdr);
	rd = fifo->RD;
	if (fifo->WT >= rd) {
		if (unitLen > (fifo->size - fifo->WT)) {
			if ((unitLen + sizeof(tx_big_hdr_t)) > rd) {
				ret = TX_FIFO_FULL;
				goto out;
			}
			spin_lock(&(fifo->lock));
			fifo->LASTWT = fifo->WT;
			spin_unlock(&(fifo->lock));
			fifo->WT = 0;
		}
	} else {
		if ((unitLen + sizeof(tx_big_hdr_t) > (rd - fifo->WT))) {
			ret = TX_FIFO_FULL;
			goto out;
		}
	}
	hdr = (t_msg_hdr_t *) (fifo->mem + fifo->WT);
	memcpy((unsigned char *)hdr, (unsigned char *)(&(msg->hdr)),
	       sizeof(t_msg_hdr_t));
	p = (unsigned char *)hdr + TX_MSG_HEAD_FILED(hdr);
	if (NULL != msg->slice[0].data) {
		memcpy(p, (unsigned char *)(msg->slice[0].data), msg->hdr.len);
	}
	if (HOST_SC2331_PKT == hdr->type) {
		memcpy((char *)(hdr + 1), (char *)(&(fifo->seq1)), 4);
		fifo->seq1++;
	} else if (HOST_SC2331_CMD == hdr->type) {
		printkp("[CMD_IN][%s]\n", get_cmd_name(hdr->subtype));
	} else {
	}
	fifo->wt_cnt++;
	fifo->WT = fifo->WT + unitLen;

	ret = OK;
out:
	return ret;
}

int tx_fifo_out(const unsigned char netif_id, const unsigned chn,
		txfifo_t *fifo, P_FUNC_1 pfunc, unsigned short *count)
{
	unsigned int len, num, wt, seq;
	unsigned char *readTo, *readFrom, *readMax;
	t_msg_hdr_t *hdr;
	tx_big_hdr_t *big_hdr;
	int ret = ERROR;

restart:
	wt = fifo->WT;
	if (fifo->RD == wt) {
		readMax = fifo->mem + fifo->LASTWT;
	} else if (wt < fifo->RD) {
		if (fifo->RD == fifo->LASTWT) {
			fifo->RD = 0;
			spin_lock(&(fifo->lock));
			fifo->LASTWT = -1;
			spin_unlock(&(fifo->lock));
			goto restart;
		} else {
			readMax = fifo->mem + fifo->LASTWT;
		}
	} else {
		readMax = fifo->mem + wt;
	}

	readTo = fifo->mem + fifo->RD;
	readFrom = readTo - sizeof(tx_big_hdr_t);
	big_hdr = (tx_big_hdr_t *) (readFrom);

	memset((unsigned char *)big_hdr, 0, sizeof(tx_big_hdr_t));
	big_hdr->mode = netif_id;
	len = sizeof(tx_big_hdr_t);

	for (num = 0; num < fifo->max_msg_num; num++) {
		hdr = (t_msg_hdr_t *) (readTo);
		if ((unsigned char *)hdr >= readMax)
			break;
		len = len + TX_MSG_UNIT_LEN(hdr);
		if (len >= fifo->cp2_txRam)
			break;
		if ((hdr->type > 2) || (hdr->subtype > 47)) {
			ASSERT();
			break;
		}
		if (HOST_SC2331_PKT == hdr->type) {
			memcpy((char *)(&seq), (char *)(hdr + 1), 4);
			fifo->seq2++;
		} else if (HOST_SC2331_CMD == hdr->type) {
			printkp("[CMD_OUT][%s]\n", get_cmd_name(hdr->subtype));
		} else {
		}
		memcpy((unsigned char *)(&(big_hdr->msg[num])), readTo,
		       sizeof(t_msg_hdr_t));
		big_hdr->msg_num++;
		hdr = TX_MSG_NEXT_MSG(hdr);
		readTo = (unsigned char *)hdr;
	}
	*count = num;
	len = readTo - readFrom;
	big_hdr->len = len;

	if (len >= (sizeof(tx_big_hdr_t) + sizeof(t_msg_hdr_t))) {
		ret = pfunc(chn, readFrom, len);
		if (OK != ret) {
			ret = HW_WRITE_ERROR;
			goto out;
		} else
			ret = OK;
	} else {
		ASSERT();
		ret = -5;
	}
	if ((fifo->RD > wt) && ((readMax - readTo) < sizeof(t_msg_hdr_t))) {
		fifo->RD = 0;
		fifo->rd_cnt = fifo->rd_cnt + num;
		spin_lock(&(fifo->lock));
		fifo->LASTWT = -1;
		spin_unlock(&(fifo->lock));
	} else {
		fifo->RD = readTo - fifo->mem;
		fifo->rd_cnt = fifo->rd_cnt + num;
	}
out:
	return ret;
}

unsigned int tx_fifo_in_pkt(txfifo_t *tx_fifo)
{
	return tx_fifo->wt_cnt - tx_fifo->rd_cnt;
}

unsigned int tx_fifo_used(txfifo_t *tx_fifo)
{
	unsigned int used;
	txfifo_t fifo = { 0 };
	memcpy((unsigned char *)(&fifo), (unsigned char *)(tx_fifo), 12);
	used =
	    ((-1 ==
	      fifo.LASTWT) ? (fifo.WT - fifo.RD) : ((fifo.LASTWT - fifo.RD) +
						    fifo.WT));
	return used;
}

int tx_fifo_alloc(txfifo_t *tx_fifo, txfifo_conf_t *conf)
{
	tx_fifo->size = conf->size;
	tx_fifo->cp2_txRam = conf->cp2_txRam;
	tx_fifo->mem = fifo_mem_alloc(tx_fifo->size);
	if (NULL == tx_fifo->mem) {
		ASSERT();
		return ERROR;
	}
	tx_fifo->mem = tx_fifo->mem + sizeof(tx_big_hdr_t);
	tx_fifo->size =
	    tx_fifo->size - (sizeof(tx_big_hdr_t) + SDIO_ALIGN_SIZE);
	tx_fifo->WT = 0;
	tx_fifo->RD = 0;
	tx_fifo->LASTWT = -1;
	tx_fifo->max_msg_num = conf->max_msg_num;
	spin_lock_init(&(tx_fifo->lock));
	return OK;
}

int tx_fifo_free(txfifo_t *tx_fifo)
{
	unsigned char *fifo_mem = tx_fifo->mem - sizeof(tx_big_hdr_t);
	if (NULL != fifo_mem)
		fifo_mem_free(fifo_mem,
			      (tx_fifo->size + sizeof(tx_big_hdr_t) +
			       SDIO_ALIGN_SIZE));

	memset((unsigned char *)tx_fifo, 0, sizeof(txfifo_t));
	return OK;
}

int rx_fifo_in(const unsigned char chn, rxfifo_t *fifo, P_FUNC_2 pfunc)
{
	int ret;
	unsigned int rx_len = -1;
	bool full = false;
	if (fifo->WT >= fifo->RD) {
		if ((fifo->WT - fifo->RD) >= (fifo->block_num - 1))
			full = true;
	} else {
		if (1 == (fifo->RD - fifo->WT))
			full = true;
	}
	if (true == full)
		return ERROR;
	ret = pfunc(chn, fifo->mem[fifo->WT], &rx_len);
	if (OK == ret)
		fifo->WT = INCR_RING_BUFF_INDX(fifo->WT, fifo->block_num);
	return ret;
}

int rx_fifo_out(rxfifo_t *fifo, P_FUNC_3 pfunc)
{
	int ret = OK;
	if (fifo->WT == fifo->RD)
		return ERROR;
	ret = pfunc(fifo->mem[fifo->RD], fifo->block_size);
	fifo->RD = INCR_RING_BUFF_INDX(fifo->RD, fifo->block_num);
	return ret;
}

int rx_fifo_used(rxfifo_t *fifo)
{
	int ret = 0;
	if (fifo->WT >= fifo->RD)
		ret = fifo->WT - fifo->RD;
	else
		ret = fifo->block_num - (fifo->RD - fifo->WT);
	return ret;
}

int rx_fifo_alloc(rxfifo_t *fifo)
{
	int i;
	memset(fifo, 0, sizeof(rxfifo_t));
	fifo->block_size = RX_FIFO_BLOCK_SIZE;
	fifo->block_num = RX_FIFO_BLOCK_NUM;
	for (i = 0; i < fifo->block_num; i++) {
		fifo->mem[i] = kmalloc(fifo->block_size, GFP_KERNEL);
		if (NULL == fifo->mem[i]) {
			ASSERT();
			return ERROR;
		}
	}
	return OK;
}

int rx_fifo_free(rxfifo_t *fifo)
{
	int i;
	for (i = 0; i < fifo->block_num; i++) {
		if (NULL != fifo->mem[i])
			kfree(fifo->mem[i]);
	}
	memset(fifo, 0, sizeof(rxfifo_t));
	return OK;
}
