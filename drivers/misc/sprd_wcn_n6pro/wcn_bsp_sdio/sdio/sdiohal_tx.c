// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Communications Inc.
 *
 * Filename : sdiohal_tx.c
 * Abstract : This file is a implementation for wcn sdio hal function
 */

#include "sdiohal.h"

int sdiohal_tx_data_list_send(struct sdiohal_list_t *data_list)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();
	struct mbuf_t *mbuf_node;
	unsigned int i;
	int ret;

	sdiohal_sdma_enter();
	sdiohal_list_check(data_list, __func__, SDIOHAL_WRITE);
	mbuf_node = data_list->mbuf_head;
	for (i = 0; i < data_list->node_num;
		i++, mbuf_node = mbuf_node->next) {
		if (p_data->send_buf.used_len + sizeof(struct bus_puh_t) +
		    SDIOHAL_ALIGN_4BYTE(mbuf_node->len) >
		    SDIOHAL_TX_SENDBUF_LEN) {
			sdiohal_tx_set_eof(&p_data->send_buf, p_data->eof_buf);
			ret = sdiohal_sdio_pt_write(p_data->send_buf.buf,
						    SDIOHAL_ALIGN_BLK(
						    p_data->send_buf.used_len));
			if (ret)
				pr_err("err 1,type:%d subtype:%d num:%d\n",
				       data_list->type, data_list->subtype,
				       data_list->node_num);
			p_data->send_buf.used_len = 0;
		}
		sdiohal_tx_packer(&p_data->send_buf, data_list, mbuf_node);
	}
	sdiohal_tx_set_eof(&p_data->send_buf, p_data->eof_buf);

	sdiohal_tx_list_denq(data_list);
	ret = sdiohal_sdio_pt_write(p_data->send_buf.buf,
				    SDIOHAL_ALIGN_BLK(
				    p_data->send_buf.used_len));
	if (ret)
		pr_err("err 2,type:%d subtype:%d num:%d\n",
		       data_list->type, data_list->subtype,
		       data_list->node_num);

	p_data->send_buf.used_len = 0;
	sdiohal_sdma_leave();

	return ret;
}

int sdiohal_tx_thread(void *data)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();
	struct sdiohal_list_t data_list;
	struct sched_param param;
	struct timespec tm_begin, tm_end;
	static long time_total_ns;
	static int times_count;
	int ret;

	param.sched_priority = SDIO_TX_TASK_PRIO;
	sched_setscheduler(current, SCHED_FIFO, &param);

	while (1) {
		/* Wait the semaphore */
		sdiohal_tx_down();
		if (p_data->exit_flag)
			break;

		getnstimeofday(&p_data->tm_end_sch);
		sdiohal_pr_perf("tx sch time:%ld\n",
				(long)(timespec_to_ns(&p_data->tm_end_sch) -
				timespec_to_ns(&p_data->tm_begin_sch)));

		sdiohal_lock_tx_ws();
		sdiohal_resume_wait();

		/* wakeup cp */
		sdiohal_cp_tx_wakeup(PACKER_TX);

		while (!sdiohal_is_tx_list_empty()) {
			getnstimeofday(&tm_begin);

			sdiohal_tx_find_data_list(&data_list);
			if (p_data->adma_tx_enable) {
				ret = sdiohal_adma_pt_write(&data_list);
				if (ret)
					pr_err("sdio adma write fail ret:%d\n",
					       ret);
				sdiohal_tx_list_denq(&data_list);
			} else
				sdiohal_tx_data_list_send(&data_list);

			getnstimeofday(&tm_end);
			time_total_ns += timespec_to_ns(&tm_end) -
					timespec_to_ns(&tm_begin);
			times_count++;
			if (!(times_count % PERFORMANCE_COUNT)) {
				sdiohal_pr_perf("tx list avg time:%ld\n",
						(time_total_ns /
						PERFORMANCE_COUNT));
				time_total_ns = 0;
				times_count = 0;
			}
		}

		sdiohal_cp_tx_sleep(PACKER_TX);
		sdiohal_unlock_tx_ws();
	}

	return 0;
}
