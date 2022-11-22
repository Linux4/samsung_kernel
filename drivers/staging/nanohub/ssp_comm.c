/*
 *  Copyright (C) 2012, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */


#include "ssp.h"

#define SSP_DEBUG_ON		"SSP:DEBUG=1"
#define SSP_DEBUG_OFF		"SSP:DEBUG=0"
#define SSP_GET_HW_REV		"SSP:GET_HW_REV"
#define SSP_GET_AP_REV		"SSP:GET_AP_REV"
#define SSP_OIS_NOTIFY_RESET	"SSP:OIS_NOTIFY_RESET"
#define TRANSFER_BUF_SIZE	1024

bool ssp_dbg;
bool ssp_pkt_dbg;

#define dprint(fmt, args...) \
	do { \
		if (unlikely(ssp_dbg)) \
			pr_debug("[SSP]:(%s:%d): " fmt, \
			__func__, __LINE__, ##args); \
	} while (0)

#define DEBUG_SHOW_HEX_SEND(msg, len) \
	do { \
		if (unlikely(ssp_pkt_dbg)) { \
			print_hex_dump(KERN_INFO, "SSP->MCU: ", \
			DUMP_PREFIX_NONE, 16, 1, (msg), (len), true); \
		} \
	} while (0)

#define DEBUG_SHOW_HEX_RECV(msg, len) \
	do { \
		if (unlikely(ssp_pkt_dbg)) { \
			print_hex_dump(KERN_INFO, "SSP<-MCU: ", \
			DUMP_PREFIX_NONE, 16, 1, (msg), (len), true); \
		} \
	} while (0)

enum packet_state_e {
	WAITFOR_PKT_HEADER = 0,
	WAITFOR_PKT_COMPLETE
};

#define MAX_SSP_DATA_SIZE	3000
struct ssp_packet_state {
	u8 buf[MAX_SSP_DATA_SIZE];
	s32 rxlen;
	u64 timestamp;

	enum packet_state_e state;
	bool islocked;

	u8 type;
	u16 opts;
	u16 required;
	struct ssp_msg *msg;
};
struct ssp_packet_state ssp_pkt;

int ssp_on_mcu_ready(void *ssh_data, bool ready)
{
	struct ssp_data *data = (struct ssp_data *)ssh_data;

	if (data == NULL)
		return -1;

	if (ready == true) {
		/* Start queue work for initializing MCU */
		data->bFirstRef == true ? data->bFirstRef = false : data->uResetCnt++;
		memset(&ssp_pkt, 0, sizeof(ssp_pkt));
		wake_lock_timeout(data->ssp_wake_lock, HZ);
		queue_work(data->ssp_mcu_ready_wq, &data->work_ssp_mcu_ready);
	} else {
		/* Disable SSP */
		ssp_enable(data, false);
		dprint("MCU is not ready and disabled SSP\n");
	}

	return 0;
}

 void makeResetInfoString(char *src, char *dst)
{
        int i, idx = 0, dstLen = (int)strlen(dst), totalLen = (int)strlen(src);
        
        for(i = 0; i < totalLen && idx < dstLen; i++) {
            if(src[i] == '"' || src[i] == '<' || src[i] == '>')
                continue;
            if(src[i] == ';')
                break;
            dst[idx++] = src[i];
        }
}
int ssp_on_control(void *ssh_data, const char *str_ctrl)
{
	struct ssp_data *data = (struct ssp_data *)ssh_data;

	if (!ssh_data || !str_ctrl)
		return -1;

	if (strstr(str_ctrl, SSP_GET_HW_REV)) {
		return data->ap_rev;
	} else if (strstr(str_ctrl, SSP_GET_AP_REV)) {
                return data->ap_type;
        } else if (strstr(str_ctrl, SSP_OIS_NOTIFY_RESET)) {
		if (data->ois_reset->ois_func != NULL) {
			data->ois_reset->ois_func(data->ois_reset->core);
		}
	}
	dprint("Received string command from LHD(=%s)\n", str_ctrl);

	return 0;
}

int ssp_on_mcu_reset(void *ssh_data, bool IsNoResp)
{
	struct ssp_data *data = (struct ssp_data *)ssh_data;

	if (!data)
		return -1;
        if(IsNoResp && !data->resetting)
            data->IsNoRespCnt++;

	data->resetting = true;
	//data->uResetCnt++;

	return 0;
}

void ssp_mcu_ready_work_func(struct work_struct *work)
{
	struct ssp_data *data = container_of(work,
					struct ssp_data, work_ssp_mcu_ready);
	int ret = 0;
	int retries = 0;

	ret = wait_for_completion_timeout(&data->hub_data->mcu_init_done, COMPLETION_TIMEOUT);

	if (unlikely(!ret))
		pr_err("[SSP] Sensors of MCU are not ready!\n");
	else
		pr_err("[SSP] Sensors of MCU are ready!\n");

	dprint("MCU is ready.(work_queue)\n");

	clean_pending_list(data);
	ssp_enable(data, true);
retries:
	ret = initialize_mcu(data);
	if (ret != SUCCESS) {
		msleep(100);
		if (++retries > 3) {
			pr_err("[SSP] fail to initialize mcu\n");
			ssp_enable(data, false);
                        data->resetting = false;
			return;
		}
		goto retries;
	}
	dprint("mcu is initiialized (retries=%d)\n", retries);

	/* initialize variables for timestamp */
	ssp_reset_batching_resources(data);

	/* recover previous state */
	sync_sensor_state(data);
	ssp_sensorhub_report_notice(data, MSG2SSP_AP_STATUS_RESET);

	if (data->uLastAPState != 0)
#ifdef CONFIG_PANEL_NOTIFY
		ssp_send_cmd(data, get_copr_status(data), 0);
#else
		ssp_send_cmd(data, data->uLastAPState, 0);
#endif
	if (data->uLastResumeState != 0)
		ssp_send_cmd(data, data->uLastResumeState, 0);

	data->resetting = false;
}

#define reset_ssp_pkt() \
do { \
	ssp_pkt.rxlen = 0; \
	ssp_pkt.required = 4; \
	ssp_pkt.state = WAITFOR_PKT_HEADER; \
} while (0)

static inline struct ssp_msg *find_ssp_msg(struct ssp_data *data)
{
	struct ssp_msg *msg, *n;
	bool found = false;

	func_dbg();
	mutex_lock(&data->pending_mutex);
	if (list_empty(&data->pending_list)) {
		pr_err("[SSP] list empty error!\n");
	    mutex_unlock(&data->pending_mutex);
		goto errexit;
	}

	list_for_each_entry_safe(msg, n, &data->pending_list, list) {
		if (msg->options == ssp_pkt.opts) {
			list_del(&msg->list);
			found = true;
			break;
		}
	}
	mutex_unlock(&data->pending_mutex);

	if (!found) {
		pr_err("[SSP] %s %d - Not match error\n", __func__, ssp_pkt.opts);
		goto errexit;
	}

	/* For dead msg, make a temporary buffer to read. */
	if (msg->dead && !msg->free_buffer) {
		msg->buffer = kzalloc(msg->length, GFP_KERNEL);
		msg->free_buffer = 1;
	}
	if (msg->buffer == NULL) {
		pr_err("[SSP]: %s() : msg->buffer is NULL\n", __func__);
		goto errexit;
	} else if (msg->length <= 0) {
		pr_err("[SSP]: %s() : msg->length is less than 0\n", __func__);
		goto errexit;
	}
	return msg;

errexit:
	pr_err("[SSP] %s opts:%d, state:%d, type:%d\n", __func__,
			ssp_pkt.opts, ssp_pkt.state, ssp_pkt.type);
	print_hex_dump(KERN_INFO, "[SSP]: ",
			DUMP_PREFIX_NONE, 16, 1, ssp_pkt.buf, 4, true);

	return NULL;
}

static inline void ssp_complete_msg(struct ssp_data *data, struct ssp_msg *msg)
{
	func_dbg();
	if (msg->done != NULL && !completion_done(msg->done)) {
		dprint("complete(mg->done)\n");
		complete(msg->done);
	}
	if (msg->dead_hook != NULL)
		*(msg->dead_hook) = true;

	clean_msg(msg);
}

int sensorhub_comms_write(struct platform_device *pdev, u8 *buf, int length, int timeout)
{
	struct contexthub_ipc_info *ipc = platform_get_drvdata(pdev);
	int ret;
	//func_dbg();
	ret = contexthub_ipc_write(ipc, buf, length, timeout);
	if (!ret) {
		ret = ERROR;
		pr_err("%s : write fail\n", __func__);
	}
	return ret = ret == length ? SUCCESS : FAIL;
}

#define SSP_PACKET_SEND_HEADER 25

int ssp_do_transfer(struct ssp_data *data, struct ssp_msg *msg,
		struct completion *done, int timeout) {
	int status = 0;
	bool msg_dead = false, ssp_down = false;
	bool use_no_irq = false;
	int length = 0;
	int type = 0;
	u8 buf[TRANSFER_BUF_SIZE];

	if (data == NULL || msg == NULL) {
		pr_err("[SSP] %s():[SSP] data or msg is NULL\n", __func__);
		return -1;
	}
	type = msg->options & SSP_SPI_MASK;
	mutex_lock(&data->comm_mutex);

	if (timeout)
		wake_lock(data->ssp_comm_wake_lock);

	ssp_down = data->bSspShutdown;

	if (ssp_down || (type == AP2HUB_WRITE && msg->length >= TRANSFER_BUF_SIZE - SSP_PACKET_SEND_HEADER)) {
		pr_err("[SSP]: ssp_down = %d (msg->length=%d) msg->cmd %x returning\n",
			ssp_down, msg->length, msg->cmd);
		clean_msg(msg);
		mdelay(5);
		if (timeout)
			wake_unlock(data->ssp_comm_wake_lock);

		mutex_unlock(&data->comm_mutex);
		return -1;
	}

	msg->dead_hook = &msg_dead;
	msg->dead = false;
	msg->done = done;
	use_no_irq = (msg->length == 0) || (type == AP2HUB_WRITE);

	mutex_lock(&data->pending_mutex);

	msg->ap_time_ns_64 = get_current_timestamp();

	memcpy(buf, (unsigned char *)msg, SSP_PACKET_SEND_HEADER);

	//DEBUG_SHOW_HEX_SEND((unsigned char *)buf, SSP_PACKET_SEND_HEADER);	
	if (type == AP2HUB_READ) {
		length = SSP_PACKET_SEND_HEADER;	
	} else {
		memcpy(buf + SSP_PACKET_SEND_HEADER, (unsigned char *)msg->buffer, msg->length);
		
		//DEBUG_SHOW_HEX_SEND((unsigned char *)buf + SSP_PACKET_SEND_HEADER, msg->length);	
		length = SSP_PACKET_SEND_HEADER + msg->length;
	}

	DEBUG_SHOW_HEX_SEND((unsigned char *)buf, length);	
	status = sensorhub_comms_write(data->pdev, (unsigned char *)buf, length, timeout);


	if (!use_no_irq) {
		/* moved UP */
		/* mutex_lock(&data->pending_mutex);*/
		list_add_tail(&msg->list, &data->pending_list);
		/* moved down */
		/* mutex_unlock(&data->pending_mutex);*/
	}

	mutex_unlock(&data->pending_mutex);

	if (status == 1 && done != NULL && type == AP2HUB_READ) {
		dprint("waiting completion ...\n");
		if (wait_for_completion_timeout(done,
				msecs_to_jiffies(timeout)) == 0) {

			pr_err("[SSP] %s(): completion is timeout!\n",
				__func__);
			pr_err("[SSP] ==========TIME-OUT MSG================\n", __func__);
			DEBUG_SHOW_HEX_SEND((unsigned char *)msg, length);
			pr_err("[SSP] ======================================\n", __func__);
			status = -2;

			mutex_lock(&data->pending_mutex);
			if (!use_no_irq && !msg_dead) {
				if ((msg->list.next != NULL) &&
					(msg->list.next != LIST_POISON1))
					list_del(&msg->list);
			}
			mutex_unlock(&data->pending_mutex);
		} else {
			dprint("completion is cleared !\n");
		}
	}

	mutex_lock(&data->pending_mutex);
	if (msg != NULL && !msg_dead) {
		msg->done = NULL;
		msg->dead_hook = NULL;

		if (status != 1)
			msg->dead = true;
		if (status == -2)
			data->uTimeOutCnt++;
	}
	mutex_unlock(&data->pending_mutex);

	if (use_no_irq)
		clean_msg(msg);

	if (timeout)
		wake_unlock(data->ssp_comm_wake_lock);

	mutex_unlock(&data->comm_mutex);

	return status;
}

#define PACKET_HEADER_SIZE 12

void ssp_recv_packet(struct ssp_data *data, char *packet, int packet_len)
{
	if (packet_len < PACKET_HEADER_SIZE) {
		//TODO: error handling
		return;	
	}

	DEBUG_SHOW_HEX_RECV(packet, PACKET_HEADER_SIZE);

	memcpy(&ssp_pkt.opts, packet, sizeof(ssp_pkt.opts));
	memcpy(&ssp_pkt.rxlen, packet + sizeof(ssp_pkt.opts), 2);
	memcpy(&ssp_pkt.timestamp, packet + 4, 8);
	
	ssp_pkt.type = ssp_pkt.opts & SSP_SPI_MASK;

	memcpy(ssp_pkt.buf, packet + PACKET_HEADER_SIZE, ssp_pkt.rxlen);
	DEBUG_SHOW_HEX_RECV(packet + PACKET_HEADER_SIZE, ssp_pkt.rxlen);

	if(ssp_pkt.type == AP2HUB_READ) {
		ssp_pkt.msg = find_ssp_msg(data);
		if (!ssp_pkt.msg) {
			pr_err("[SSP] find_ssp_msg --- failed");
			return;
		}
		else {
			if (ssp_pkt.msg != NULL && ssp_pkt.rxlen == ssp_pkt.msg->length
				&& ssp_pkt.rxlen < MAX_SSP_DATA_SIZE) {
				mutex_lock(&data->pending_mutex);

				memcpy(ssp_pkt.msg->buffer, ssp_pkt.buf, ssp_pkt.rxlen);
				
				ssp_complete_msg(data, ssp_pkt.msg);

				mutex_unlock(&data->pending_mutex);
				ssp_pkt.msg = NULL;
			} else {
				pr_err("[SSP] %s(): invalid packe!\n",
				__func__);
				DEBUG_SHOW_HEX_RECV(packet, packet_len);
			}
		}
	} else {
		data->timestamp = get_current_timestamp();
		parse_dataframe(data, ssp_pkt.buf, ssp_pkt.rxlen);
	}
    return;
}