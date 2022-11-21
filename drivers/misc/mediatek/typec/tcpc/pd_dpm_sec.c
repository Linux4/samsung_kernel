// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2014-2017 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/mutex.h>

#include "inc/tcpci.h"
#include "inc/pd_policy_engine.h"
#include "inc/pd_dpm_core.h"
#include "inc/pd_dpm_pdo_select.h"
#include "inc/pd_core.h"
#include "pd_dpm_prv.h"
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
#include <linux/usb/typec/common/pdic_notifier.h>
#include <linux/usb/typec/common/pdic_core.h>
#endif /* CONFIG_PDIC_NOTIFIER */
#if defined(CONFIG_USB_HW_PARAM)
#include <linux/usb_notify.h>
#endif

#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)

int sec_dfp_uvdm_ready(void)
{
	int uvdm_ready = 0;
	struct device *pdic_device = get_pdic_device();
	ppdic_data_t pdic_data;
	struct tcpc_device *tcpc;
	struct pd_port *pd_port;

	if (!pdic_device) {
		pr_err("%s pdic_device is null\n", __func__);
		return -ENODEV;
	}

	pdic_data = dev_get_drvdata(pdic_device);
	if(!pdic_data) {
		pr_err("%s pdic_data is null\n", __func__);
		return -ENODEV;
	}

	tcpc = pdic_data->drv_data;
	if (!tcpc) {
		pr_err("%s tcpc is null\n", __func__);
		return -ENXIO;
	}

	pd_port = &tcpc->pd_port;
	if (!pd_port) {
		pr_err("%s pd_port is null\n", __func__);
		return -ENXIO;
	}

	if (pd_port->sec_dfp_state == SEC_DFP_ENTER_MODE_DONE &&
			pd_port->pe_data.pe_ready)
		uvdm_ready = 1;

	pr_info("%s uvdm_ready=%d state=%d pe_ready=%d\n", __func__,
			uvdm_ready, pd_port->sec_dfp_state,
			pd_port->pe_data.pe_ready);

	return uvdm_ready;
}

void sec_dfp_uvdm_close(void)
{
	struct device *pdic_device = get_pdic_device();
	ppdic_data_t pdic_data;
	struct tcpc_device *tcpc;
	struct pd_port *pd_port;

	if (!pdic_device) {
		pr_err("%s pdic_device is null\n", __func__);
		return;
	}

	pdic_data = dev_get_drvdata(pdic_device);
	if(!pdic_data) {
		pr_err("%s pdic_data is null\n", __func__);
		return;
	}

	tcpc = pdic_data->drv_data;
	if (!tcpc) {
		pr_err("%s tcpc is null\n", __func__);
		return;
	}

	pd_port = &tcpc->pd_port;
	if (!pd_port) {
		pr_err("%s pd_port is null\n", __func__);
		return;
	}

	pd_port->uvdm_out_ok = 1;
	pd_port->uvdm_in_ok = 1;
	wake_up(&pd_port->uvdm_out_wq);
	wake_up(&pd_port->uvdm_in_wq);

	pd_port->pe_data.modal_operation = false;

	pr_info("%s\n", __func__);
}

static void sec_dfp_send_uvdm(struct pd_port *pd_port, int cnt)
{
	int i;

	if (cnt > PD_DATA_OBJ_SIZE)
		cnt = PD_DATA_OBJ_SIZE;

	pd_port->uvdm_cnt = cnt;
	pd_port->uvdm_wait_resp = true;

	for (i = 0; i < cnt; i++)
		pd_port->uvdm_data[i] = pd_port->uvdm_data_obj[i].object;

	pd_put_tcp_vdm_event(pd_port, TCP_DPM_EVT_UVDM);
}

#define SEC_UVDM_WAIT_MS 2000
#define MAX_INPUT_DATA 255

int sec_dfp_uvdm_out_request_message(void *data, int size)
{
	uint8_t *SEC_DATA;
	uint8_t rcv_data[MAX_INPUT_DATA] = {0,};
	int need_set_cnt = 0;
	int cur_set_data = 0;
	int cur_set_num = 0;
	int remained_data_size = 0;
	int accumulated_data_size = 0;
	int received_data_index = 0;
	int time_left = 0;
	int i;
	struct device *pdic_device = get_pdic_device();
	ppdic_data_t pdic_data;
	struct tcpc_device *tcpc;
	struct pd_port *pd_port;
#ifdef CONFIG_USB_NOTIFY_PROC_LOG
	int event;
#endif

	if (!pdic_device) {
		pr_err("%s pdic_device is null\n", __func__);
		return -ENODEV;
	}

	pdic_data = dev_get_drvdata(pdic_device);
	if(!pdic_data) {
		pr_err("%s pdic_data is null\n", __func__);
		return -ENODEV;
	}

	tcpc = pdic_data->drv_data;
	if (!tcpc) {
		pr_err("%s tcpc is null\n", __func__);
		return -ENXIO;
	}

	pd_port = &tcpc->pd_port;
	if (!pd_port) {
		pr_err("%s pd_port is null\n", __func__);
		return -ENXIO;
	}

	set_uvdm_header(&pd_port->uvdm_data_obj[0], SAMSUNG_VENDOR_ID, 0);
	set_endian(data, rcv_data, size);

	if (size <= 1) {
		pr_info("%s process short data\n", __func__);
		/* VDM Header + 6 VDOs = MAX 7 */
		pd_port->uvdm_data_obj[1].sec_uvdm_header.total_set_num = 1;
		pd_port->uvdm_data_obj[1].sec_uvdm_header.data = rcv_data[0];
		pd_port->uvdm_out_ok = 0;
		sec_dfp_send_uvdm(pd_port, 2);
		time_left =
			wait_event_interruptible_timeout(
				pd_port->uvdm_out_wq, pd_port->uvdm_out_ok,
				msecs_to_jiffies(SEC_UVDM_WAIT_MS));
		if (pd_port->uvdm_out_ok == 2)	{
			pr_err("%s NAK\n", __func__);
			return -ENODATA;
		} else if (pd_port->uvdm_out_ok == 3) {
			pr_err("%s BUSY\n", __func__);
			return -EBUSY;
		} else if (!time_left) {
			pr_err("%s timeout\n", __func__);
#ifdef CONFIG_USB_NOTIFY_PROC_LOG
			event = NOTIFY_EXTRA_UVDM_TIMEOUT;
			store_usblog_notify(NOTIFY_EXTRA, (void *)&event, NULL);
#endif
			return -ETIME;
		} else if (time_left == -ERESTARTSYS)
			return -ERESTARTSYS;
	} else {
		pr_info("%s process long data\n", __func__);
		need_set_cnt = set_uvdmset_count(size);
		pd_port->uvdm_first_req = true;
		pd_port->uvdm_dir =  DIR_OUT;
		cur_set_num = 1;
		accumulated_data_size = 0;
		remained_data_size = size;

		if (pd_port->uvdm_first_req)
			set_sec_uvdm_header(&pd_port->uvdm_data_obj[0],
					pd_port->sec_pid,
					SEC_UVDM_LONG_DATA,
					SEC_UVDM_ININIATOR, DIR_OUT,
					need_set_cnt, 0);
		while (cur_set_num <= need_set_cnt) {
			cur_set_data = 0;
			time_left = 0;
			set_sec_uvdm_tx_header(
					&pd_port->uvdm_data_obj[0], pd_port->uvdm_first_req,
					cur_set_num, size, remained_data_size);
			cur_set_data =
				get_data_size(pd_port->uvdm_first_req, remained_data_size);

			pr_info("%s cur_set_data:%d, size:%d, cur_set_num:%d\n",
				__func__, cur_set_data, size, cur_set_num);

			if (pd_port->uvdm_first_req) {
				SEC_DATA =
					(uint8_t *)&pd_port->uvdm_data_obj[3];
				for (i = 0; i < SEC_UVDM_MAXDATA_FIRST; i++)
					SEC_DATA[i] =
						rcv_data[received_data_index++];
			} else {
				SEC_DATA =
					(uint8_t *)&pd_port->uvdm_data_obj[2];
				for (i = 0; i < SEC_UVDM_MAXDATA_NORMAL; i++)
					SEC_DATA[i] =
						rcv_data[received_data_index++];
			}

			set_sec_uvdm_tx_tailer(&pd_port->uvdm_data_obj[0]);
			pd_port->uvdm_out_ok = 0;

			sec_dfp_send_uvdm(pd_port, 7);
			time_left =
				wait_event_interruptible_timeout(
					pd_port->uvdm_out_wq, pd_port->uvdm_out_ok,
					msecs_to_jiffies(SEC_UVDM_WAIT_MS));
			if (pd_port->uvdm_out_ok == 2 ||
				pd_port->uvdm_out_ok == 4)	{
				pr_err("%s NAK\n", __func__);
				return -ENODATA;
			} else if (pd_port->uvdm_out_ok == 3 ||
					   pd_port->uvdm_out_ok == 5) {
				pr_err("%s BUSY\n", __func__);
				return -EBUSY;
			} else if (!time_left) {
				pr_err("%s timeout\n", __func__);
#ifdef CONFIG_USB_NOTIFY_PROC_LOG
				event = NOTIFY_EXTRA_UVDM_TIMEOUT;
				store_usblog_notify(NOTIFY_EXTRA, (void *)&event, NULL);
#endif
				return -ETIME;
			} else if (time_left == -ERESTARTSYS)
				return -ERESTARTSYS;

			accumulated_data_size += cur_set_data;
			remained_data_size -= cur_set_data;
			if (pd_port->uvdm_first_req)
				pd_port->uvdm_first_req = false;
			cur_set_num++;
		}
	}
	return size;
}

int sec_dfp_uvdm_in_request_message(void *data)
{
	uint8_t in_data[MAX_INPUT_DATA] = {0,};

	s_uvdm_header	SEC_RES_HEADER;
	s_tx_header	SEC_TX_HEADER;
	s_tx_tailer	SEC_TX_TAILER;
	data_obj_type	uvdm_data_obj[PD_DATA_OBJ_SIZE];

	int cur_set_data = 0;
	int cur_set_num = 0;
	int total_set_num = 0;
	int rcv_data_size = 0;
	int total_rcv_size = 0;
	int ack = 0;
	int size = 0;
	int time_left = 0;
	int i;
	int cal_checksum = 0;
	struct device *pdic_device = get_pdic_device();
	ppdic_data_t pdic_data;
	struct tcpc_device *tcpc;
	struct pd_port *pd_port;
#ifdef CONFIG_USB_NOTIFY_PROC_LOG
	int event;
#endif

	if (!pdic_device) {
		pr_err("%s pdic_device is null\n", __func__);
		return -ENODEV;
	}

	pdic_data = dev_get_drvdata(pdic_device);
	if(!pdic_data) {
		pr_err("%s pdic_data is null\n", __func__);
		return -ENODEV;
	}

	tcpc = pdic_data->drv_data;
	if (!tcpc) {
		pr_err("%s tcpc is null\n", __func__);
		return -ENXIO;
	}

	pd_port = &tcpc->pd_port;
	if (!pd_port) {
		pr_err("%s pd_port is null\n", __func__);
		return -ENXIO;
	}

	pr_info("%s\n", __func__);

	pd_port->uvdm_dir = DIR_IN;
	pd_port->uvdm_first_req = true;

	/* Common : Fill the UVDMHeader*/
	set_uvdm_header(&pd_port->uvdm_data_obj[0], SAMSUNG_VENDOR_ID, 0);

	/* Common : Fill the First SEC_VDMHeader*/
	if (pd_port->uvdm_first_req)
		set_sec_uvdm_header(&pd_port->uvdm_data_obj[0],
				pd_port->sec_pid,
				SEC_UVDM_LONG_DATA,
				SEC_UVDM_ININIATOR, DIR_IN, 0, 0);
	/* Send data to PDIC */
	pd_port->uvdm_in_ok = 0;
	sec_dfp_send_uvdm(pd_port, 2);

	cur_set_num = 0;
	total_set_num = 1;

	do {
		time_left =
			wait_event_interruptible_timeout(
					pd_port->uvdm_in_wq, pd_port->uvdm_in_ok,
					msecs_to_jiffies(SEC_UVDM_WAIT_MS));
		if (pd_port->uvdm_in_ok == 2)	{
			pr_err("%s NAK\n", __func__);
			return -ENODATA;
		} else if (pd_port->uvdm_in_ok == 3) {
			pr_err("%s BUSY\n", __func__);
			return -EBUSY;
		} else if (!time_left) {
			pr_err("%s timeout\n", __func__);
#ifdef CONFIG_USB_NOTIFY_PROC_LOG
			event = NOTIFY_EXTRA_UVDM_TIMEOUT;
			store_usblog_notify(NOTIFY_EXTRA, (void *)&event, NULL);
#endif
			return -ETIME;
		} else if (time_left == -ERESTARTSYS)
			return -ERESTARTSYS;

		/* read data */
		for (i = 0; i < pd_port->uvdm_cnt; i++)
			uvdm_data_obj[i].object = pd_port->uvdm_data[i];

		if (pd_port->uvdm_first_req) {
			SEC_RES_HEADER.object = uvdm_data_obj[1].object;
			SEC_TX_HEADER.object = uvdm_data_obj[2].object;

			if (SEC_RES_HEADER.data_type == TYPE_SHORT) {
				pr_info("%s - process short data\n", __func__);
				if (rcv_data_size >= MAX_INPUT_DATA) {
					pr_err("%s too many size(%d)\n", __func__, rcv_data_size);
					return -ENOMEM;
				}
				in_data[rcv_data_size++] = SEC_RES_HEADER.data;
				return rcv_data_size;
			}
			/* 1. check the data size received */
			size = SEC_TX_HEADER.total_size;
			cur_set_data = SEC_TX_HEADER.cur_size;
			cur_set_num = SEC_TX_HEADER.order_cur_set;
			total_set_num = SEC_RES_HEADER.total_set_num;

			pd_port->uvdm_first_req = false;
			/* 2. copy data to buffer */
			for (i = 0; i < SEC_UVDM_MAXDATA_FIRST; i++) {
				if (rcv_data_size >= MAX_INPUT_DATA) {
					pr_err("%s too many size(%d)\n", __func__, rcv_data_size);
					return -ENOMEM;
				}
				in_data[rcv_data_size++] =
					uvdm_data_obj[3+i/SEC_UVDM_ALIGN].byte[i%SEC_UVDM_ALIGN];
			}
			total_rcv_size += cur_set_data;
			pd_port->uvdm_first_req = false;
		} else {
			SEC_TX_HEADER.object = uvdm_data_obj[1].object;
			cur_set_data = SEC_TX_HEADER.cur_size;
			cur_set_num = SEC_TX_HEADER.order_cur_set;
			/* 2. copy data to buffer */
			for (i = 0 ; i < SEC_UVDM_MAXDATA_NORMAL; i++) {
				if (rcv_data_size >= MAX_INPUT_DATA) {
					pr_err("%s too many size(%d)\n", __func__, rcv_data_size);
					return -ENOMEM;
				}
				in_data[rcv_data_size++] =
					uvdm_data_obj[2+i/SEC_UVDM_ALIGN].byte[i%SEC_UVDM_ALIGN];
			}
			total_rcv_size += cur_set_data;
		}

		/* 3. Check Checksum */
		SEC_TX_TAILER.object = uvdm_data_obj[6].object;
		cal_checksum =
			get_checksum((char *)&uvdm_data_obj[0], 4, SEC_UVDM_CHECKSUM_COUNT);
		ack = (cal_checksum == SEC_TX_TAILER.checksum) ? RX_ACK : RX_NAK;

		/* 4.1. Common : Fill the UVDMHeader*/
		set_uvdm_header(&pd_port->uvdm_data_obj[0],
				SAMSUNG_VENDOR_ID, 0);
		/* 4.2. Common : Fill the First SEC_VDMHeader*/
		set_sec_uvdm_rx_header(&pd_port->uvdm_data_obj[0],
				cur_set_num, cur_set_data, ack);
		pd_port->uvdm_in_ok = 0;
		sec_dfp_send_uvdm(pd_port, 2);
	} while (cur_set_num < total_set_num);

	set_endian(in_data, data, size);
	return size;
}

static void sec_dfp_receive_samsung_uvdm_message(struct pd_port *pd_port)
{
	int i = 0;
	data_obj_type uvdm_data_obj[PD_DATA_OBJ_SIZE];
	s_uvdm_header SEC_UVDM_RES_HEADER;
	s_rx_header SEC_UVDM_RX_HEADER;

	for (i = 0; i < pd_port->uvdm_cnt; i++)
		uvdm_data_obj[i].object = pd_port->uvdm_data[i];

	pr_info("%s dir %s\n", __func__, (pd_port->uvdm_dir == DIR_OUT)
		? "OUT":"IN");

	if (pd_port->uvdm_dir == DIR_OUT) {
		if (pd_port->uvdm_first_req) {
			SEC_UVDM_RES_HEADER.object = uvdm_data_obj[1].object;
			if (SEC_UVDM_RES_HEADER.data_type == TYPE_LONG) {
				SEC_UVDM_RX_HEADER.object = uvdm_data_obj[2].object;
				if (SEC_UVDM_RES_HEADER.cmd_type == RES_ACK) {
					if (SEC_UVDM_RX_HEADER.result_value == RX_ACK) {
						pd_port->uvdm_out_ok = 1;
					} else if (SEC_UVDM_RX_HEADER.result_value == RX_NAK) {
						pr_err("%s SEC_UVDM_RX_HEADER : RX_NAK\n", __func__);
						pd_port->uvdm_out_ok = 4;
					} else if (SEC_UVDM_RX_HEADER.result_value == RX_BUSY) {
						pr_err("%s SEC_UVDM_RX_HEADER : RX_BUSY\n", __func__);
						pd_port->uvdm_out_ok = 5;
					}
				} else if (SEC_UVDM_RES_HEADER.cmd_type == RES_NAK) {
					pr_err("%s SEC_UVDM_RES_HEADER : RES_NAK\n", __func__);
					pd_port->uvdm_out_ok = 2;
				} else if (SEC_UVDM_RES_HEADER.cmd_type == RES_BUSY) {
					pr_err("%s SEC_UVDM_RES_HEADER : RES_BUSY\n", __func__);
					pd_port->uvdm_out_ok = 3;
				}
			} else if (SEC_UVDM_RES_HEADER.data_type == TYPE_SHORT) {
				if (SEC_UVDM_RES_HEADER.cmd_type == RES_ACK) {
					pd_port->uvdm_out_ok = 1;
				} else if (SEC_UVDM_RES_HEADER.cmd_type == RES_NAK) {
					pr_err("%s SEC_UVDM_RES_HEADER : RES_NAK\n", __func__);
					pd_port->uvdm_out_ok = 2;
				} else if (SEC_UVDM_RES_HEADER.cmd_type == RES_BUSY) {
					pr_err("%s SEC_UVDM_RES_HEADER : RES_BUSY\n", __func__);
					pd_port->uvdm_out_ok = 3;
				}
			}
		} else {
			SEC_UVDM_RX_HEADER.object = uvdm_data_obj[1].object;
			if (SEC_UVDM_RX_HEADER.result_value == RX_ACK) {
				pd_port->uvdm_out_ok = 1;
			} else if (SEC_UVDM_RX_HEADER.result_value == RX_NAK) {
				pr_err("%s SEC_UVDM_RX_HEADER : RX_NAK\n", __func__);
				pd_port->uvdm_out_ok = 4;
			} else if (SEC_UVDM_RX_HEADER.result_value == RX_BUSY) {
				pr_err("%s SEC_UVDM_RX_HEADER : RX_BUSY\n", __func__);
				pd_port->uvdm_out_ok = 5;
			}
		}
		wake_up(&pd_port->uvdm_out_wq);
	} else { /* DIR_IN */
		if (pd_port->uvdm_first_req) { /* Long = Short */
			SEC_UVDM_RES_HEADER.object = uvdm_data_obj[1].object;
			if (SEC_UVDM_RES_HEADER.cmd_type == RES_ACK) {
				pd_port->uvdm_in_ok = 1;
			} else if (SEC_UVDM_RES_HEADER.cmd_type == RES_NAK) {
				pr_err("%s SEC_UVDM_RES_HEADER : RES_NAK\n", __func__);
				pd_port->uvdm_in_ok = 2;
			} else if (SEC_UVDM_RES_HEADER.cmd_type == RES_BUSY) {
				pr_err("%s SEC_UVDM_RES_HEADER : RES_BUSY\n", __func__);
				pd_port->uvdm_in_ok = 3;
			}
		} else {
			/* IN response case, SEC_TX_HEADER has no ACK message. So uvdm_is_ok is always 1. */
			pd_port->uvdm_in_ok = 1;
		}
		wake_up(&pd_port->uvdm_in_wq);
	}
}

static int sec_dfp_check_accessory(struct pd_port *pd_port)
{
	int acc_type = PDIC_DOCK_DETACHED;
	uint32_t vid = pd_port->sec_vid;
	uint32_t pid = pd_port->sec_pid;

	if (pd_port->sec_acc_type == PDIC_DOCK_DETACHED) {
		if (vid == SAMSUNG_VENDOR_ID) {
			switch (pid) {
			/* GearVR: Reserved GearVR PID+6 */
			case GEARVR_PRODUCT_ID ... GEARVR_PRODUCT_ID_5:
				acc_type = PDIC_DOCK_HMT;
				pr_info("%s: Samsung Gear VR connected\n",
					__func__);
				break;
			case DEXDOCK_PRODUCT_ID:
				acc_type = PDIC_DOCK_DEX;
				pr_info("%s: Samsung DEX connected\n",
					__func__);
				break;
			case DEXPAD_PRODUCT_ID:
				acc_type = PDIC_DOCK_DEXPAD;
				pr_info("%s: Samsung DEX PAD connected\n",
					__func__);
				break;
			case HDMI_PRODUCT_ID:
				acc_type = PDIC_DOCK_HDMI;
				pr_info("%s: Samsung HDMI adapter(EE-HG950) connected\n",
					__func__);
				break;
			default:
				acc_type = PDIC_DOCK_NEW;
				if (pid == FRIENDS_PRODUCT_ID)
					pr_info("%s: Samsung Friends Stand connected\n",
						__func__);
				else
					pr_info("%s: default device connected\n",
						__func__);
				break;
			}
		} else if (vid == SAMSUNG_MPA_VENDOR_ID) {
			switch (pid) {
			case MPA_PRODUCT_ID:
				acc_type = PDIC_DOCK_MPA;
				pr_info("%s: Samsung MPA connected\n",
					__func__);
				break;
			default:
				acc_type = PDIC_DOCK_NEW;
				pr_info("%s: default device connected\n",
					__func__);
				break;
			}
		} else {
			acc_type = PDIC_DOCK_NEW;
			pr_info("%s: unknown device connected\n",
				__func__);
		}
		pd_port->sec_acc_type = acc_type;
	} else {
		acc_type = pd_port->sec_acc_type;
	}

	if (acc_type != PDIC_DOCK_NEW)
		pdic_send_dock_intent(acc_type);

	pdic_send_dock_uevent(vid, pid, acc_type);

	return (acc_type != PDIC_DOCK_NEW || vid == SAMSUNG_VENDOR_ID) ? 1 : 0;
}

void sec_dfp_accessory_detach_handler(struct pd_port *pd_port)
{
	pr_info("%s acc_type=%d\n", __func__, pd_port->sec_acc_type);

	if (pd_port->sec_acc_type != PDIC_DOCK_DETACHED) {
		if (pd_port->sec_acc_type != PDIC_DOCK_NEW)
			pdic_send_dock_intent(PDIC_DOCK_DETACHED);
		pdic_send_dock_uevent(pd_port->sec_vid, pd_port->sec_pid,
				PDIC_DOCK_DETACHED);
	}

	pd_port->sec_dfp_state = SEC_DFP_NONE;
	pd_port->sec_acc_type = PDIC_DOCK_DETACHED;
	pd_port->sec_vid = 0;
	pd_port->sec_pid = 0;
	pd_port->sec_bcd_device = 0;
}

bool sec_dfp_notify_discover_id(struct pd_port *pd_port,
	struct svdm_svid_data *svid_data, bool ack)
{
	uint32_t *payload = pd_get_msg_vdm_data_payload(pd_port);

	pr_info("%s ack=%d state=%d\n", __func__, ack, pd_port->sec_dfp_state);

	if (pd_port->sec_dfp_state != SEC_DFP_DISCOVER_ID)
		return true;

	if (!ack) {
		pd_port->sec_dfp_state = SEC_DFP_ERR;
		return true;
	}

	if (!payload) {
		pr_err("%s payload is NULL, return\n", __func__);
		return true;
	}

	pd_port->sec_dfp_state = SEC_DFP_DISCOVER_SVIDS;

	pd_port->sec_vid = PD_IDH_VID(payload[0]);
	pd_port->sec_pid = PD_PRODUCT_PID(payload[2]);
	pd_port->sec_bcd_device = PD_PRODUCT_BCD(payload[2]);
	pr_info("%s VID:0x%04x PID:0x%04x bcdDevice:0x%04x\n", __func__,
			pd_port->sec_vid, pd_port->sec_pid,
			pd_port->sec_bcd_device);

	if (sec_dfp_check_accessory(pd_port))
		pr_info("%s Samsung Accessory Connected\n", __func__);

	return true;
}

bool sec_dfp_notify_discover_svid(
	struct pd_port *pd_port, struct svdm_svid_data *svid_data, bool ack)
{
	pr_info("%s ack=%d state=%d exist=%d\n", __func__, ack,
			pd_port->sec_dfp_state, svid_data->exist);

	if (!svid_data->exist) {
		pd_port->sec_dfp_state = SEC_DFP_ERR;
		return false;
	}

	if (pd_port->sec_dfp_state != SEC_DFP_DISCOVER_SVIDS)
		return false;

	if (!ack) {
		pd_port->sec_dfp_state = SEC_DFP_ERR;
		return false;
	}

	pd_port->sec_dfp_state = SEC_DFP_DISCOVER_MODES;
	dpm_reaction_set(pd_port, DPM_REACTION_DISCOVER_CABLE_FLOW);

	return true;
}

bool sec_dfp_notify_discover_modes(
	struct pd_port *pd_port, struct svdm_svid_data *svid_data, bool ack)
{
	pr_info("%s ack=%d state=%d mode_cnt=%d\n", __func__, ack,
			pd_port->sec_dfp_state, svid_data->remote_mode.mode_cnt);

	if (pd_port->sec_dfp_state != SEC_DFP_DISCOVER_MODES)
		return false;

	if (!ack) {
		pd_port->sec_dfp_state = SEC_DFP_ERR;
		return false;
	}

	if (svid_data->remote_mode.mode_cnt == 0) {
		pd_port->sec_dfp_state = SEC_DFP_ERR;
		return false;
	}

	pd_port->mode_obj_pos = 1;
	pd_port->sec_dfp_state = SEC_DFP_ENTER_MODE;
	pd_put_tcp_vdm_event(pd_port, TCP_DPM_EVT_ENTER_MODE);

	return true;
}

bool sec_dfp_notify_enter_mode(struct pd_port *pd_port,
		struct svdm_svid_data *svid_data, uint8_t ops, bool ack)
{
	pr_info("%s ack=%d state=%d\n", __func__, ack, pd_port->sec_dfp_state);

	if (pd_port->sec_dfp_state != SEC_DFP_ENTER_MODE)
		return true;

	if (!ack) {
		pd_port->sec_dfp_state = SEC_DFP_ERR;
		return true;
	}

	pd_port->sec_dfp_state = SEC_DFP_ENTER_MODE_DONE;

	return true;
}

bool sec_dfp_notify_uvdm(struct pd_port *pd_port,
		struct svdm_svid_data *svid_data, bool ack)
{
	pr_info("%s ack=%d state=%d\n", __func__, ack, pd_port->sec_dfp_state);

	if (pd_port->sec_dfp_state != SEC_DFP_ENTER_MODE_DONE)
		return true;

	if (!ack)
		return true;

	sec_dfp_receive_samsung_uvdm_message(pd_port);

	return true;
}

bool sec_dfp_notify_pe_startup(
		struct pd_port *pd_port, struct svdm_svid_data *svid_data)
{
	pr_info("%s\n", __func__);
	pd_port->sec_dfp_state = SEC_DFP_DISCOVER_ID;

	return true;
}

int sec_dfp_notify_pe_ready(struct pd_port *pd_port,
		struct svdm_svid_data *svid_data)
{
	pr_info("%s data_role=%d state=%d\n", __func__,
			pd_port->data_role, pd_port->sec_dfp_state);

	if (pd_port->data_role != PD_ROLE_DFP)
		return 0;

	if (pd_port->sec_dfp_state != SEC_DFP_DISCOVER_MODES)
		return 0;

	pd_port->mode_svid = SAMSUNG_VID;
	pd_put_tcp_vdm_event(pd_port, TCP_DPM_EVT_DISCOVER_MODES);

	return 1;
}

bool sec_dfp_parse_svid_data(struct pd_port *pd_port,
		struct svdm_svid_data *svid_data)
{
	pr_info("%s\n", __func__);
	pd_port->sec_acc_type = PDIC_DOCK_DETACHED;
	pd_port->sec_vid = 0;
	pd_port->sec_pid = 0;
	pd_port->sec_bcd_device = 0;
	pd_port->sec_dfp_state = SEC_DFP_NONE;
	init_waitqueue_head(&pd_port->uvdm_out_wq);
	init_waitqueue_head(&pd_port->uvdm_in_wq);
	pd_port->uvdm_out_ok = 1;
	pd_port->uvdm_in_ok = 1;
	return true;
}

#endif	/* CONFIG_PDIC_NOTIFIER */
