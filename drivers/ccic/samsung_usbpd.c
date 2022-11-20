/*
 * Copyright (c) 2012-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt)	"[samsung-vdm] %s: " fmt, __func__

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <linux/component.h>
#include <linux/of_irq.h>
#include <linux/extcon.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/usb/usbpd.h>
#include <linux/platform_device.h>
#include <linux/usb_notify.h>
#include <linux/usb/typec/pdic_core.h>
#include <linux/usb/typec/pm6150/samsung_usbpd.h>
#include <linux/usb/typec/pm6150/pm6150_typec.h>


int samsung_usbpd_get(struct device *dev, struct samsung_usbpd_private *usbpd);
void samsung_usbpd_put(struct device *dev, struct samsung_usbpd_private *usbpd);

int usbpd_send_uvdm(struct usbpd *pd, u16 vid, void * vdos, int num_vdos);

int samsung_usbpd_uvdm_ready(void)
{
	int uvdm_ready = 0;
	struct device *ccic_device = get_ccic_device(); 
	pccic_data_t ccic_data;

	struct pm6150_phydrv_data *phy_data;

	if (!ccic_device) { 
		pr_err("%s: ccic_device is null.\n", __func__); 
		return -ENODEV;
	}
	ccic_data = dev_get_drvdata(ccic_device);
	if (!ccic_data) {
 		pr_err("ccic_data is null\n");
		return -ENXIO;
	}
	phy_data = ccic_data->drv_data;
	if (!phy_data) {
 		pr_err("phy_data is null\n");
		return -ENXIO;
	}

	if (phy_data->is_samsung_accessory_enter_mode
		&& phy_data->pn_flag)
		uvdm_ready = 1;

	pr_info("%s: uvdm ready is %s, entermode : %d, pn_flag=%d \n", __func__,
		uvdm_ready ? "true" : "false",
		phy_data->is_samsung_accessory_enter_mode,
		phy_data->pn_flag);

	return uvdm_ready;
}

void samsung_usbpd_uvdm_close(void)
{
	struct device *ccic_device = get_ccic_device(); 
	pccic_data_t ccic_data;
	struct pm6150_phydrv_data *phy_data;

	if (!ccic_device) { 
		pr_err("%s: ccic_device is null.\n", __func__);
		return;
	}
	ccic_data = dev_get_drvdata(ccic_device);
	if (!ccic_data) {
 		pr_err("ccic_data is null\n");
		return;
	}
	phy_data = ccic_data->drv_data;
	if (!phy_data) {
 		pr_err("phy_data is null\n");
		return;
	}

	complete(&phy_data->uvdm_out_wait);
	complete(&phy_data->uvdm_in_wait);
	pr_info("%s\n", __func__);
}

int samsung_usbpd_uvdm_out_request_message(void *data, int size)
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
	struct device *ccic_device = get_ccic_device(); 
	pccic_data_t ccic_data;
	struct pm6150_phydrv_data *phy_data;
	struct usbpd *pd;
#ifdef CONFIG_USB_NOTIFY_PROC_LOG
	int event;
#endif
	if (!ccic_device) { 
		pr_err("%s: ccic_device is null.\n", __func__); 
		return -ENODEV;
	}
	ccic_data = dev_get_drvdata(ccic_device);
	if (!ccic_data) {
		pr_err("ccic_data is null\n");
		return -ENXIO;
	}
	phy_data = ccic_data->drv_data;
	if (!phy_data) {
		pr_err("phy_data is null\n");
		return -ENXIO;
	}
	pd = phy_data->pd;
	if (!pd) {
		pr_err("pd is null\n");
		return -ENXIO;
	}

	set_msg_header(&phy_data->uvdm_msg_header, USBPD_Vendor_Defined,
			SEC_UVDM_OUTREQ_NUMOBJ);
	set_uvdm_header(&phy_data->uvdm_data_obj[0], SAMSUNG_VENDOR_ID, 0);

	if (size <= 1) {
		pr_info("%s - process short data\n", __func__);
		/* VDM Header + 6 VDOs = MAX 7 */
		phy_data->uvdm_msg_header.num_data_objs = 2;
		phy_data->uvdm_data_obj[1].sec_uvdm_header.total_set_num = 1;
	} else {
		pr_info("%s - process long data\n", __func__);
		set_endian(data, rcv_data, size);
		need_set_cnt = set_uvdmset_count(size);
		phy_data->uvdm_first_req = true;
		phy_data->uvdm_dir =  DIR_OUT;
		cur_set_num = 1;
		accumulated_data_size = 0;
		remained_data_size = size;

		if (phy_data->uvdm_first_req)
			set_sec_uvdm_header(&phy_data->uvdm_data_obj[0],
					phy_data->Product_ID,
					SEC_UVDM_LONG_DATA,
					SEC_UVDM_ININIATOR, DIR_OUT,
					need_set_cnt, 0);
		while (cur_set_num <= need_set_cnt) {
			cur_set_data = 0;
			time_left = 0;
			set_sec_uvdm_tx_header(
					&phy_data->uvdm_data_obj[0], phy_data->uvdm_first_req,
					cur_set_num, size, remained_data_size);
			cur_set_data =
				get_data_size(phy_data->uvdm_first_req, remained_data_size);

			pr_info("%s - cur_set_data:%d, size:%ld, cur_set_num:%d\n",
				__func__, cur_set_data, size, cur_set_num);

			if (phy_data->uvdm_first_req) {
				SEC_DATA =
					(uint8_t *)&phy_data->uvdm_data_obj[3];
				for (i = 0; i < SEC_UVDM_MAXDATA_FIRST; i++)
					SEC_DATA[i] =
						rcv_data[received_data_index++];
			} else {
				SEC_DATA =
					(uint8_t *)&phy_data->uvdm_data_obj[2];
				for (i = 0; i < SEC_UVDM_MAXDATA_NORMAL; i++)
					SEC_DATA[i] =
						rcv_data[received_data_index++];
			}

			set_sec_uvdm_tx_tailer(&phy_data->uvdm_data_obj[0]);

			reinit_completion(&phy_data->uvdm_out_wait);

			usbpd_send_uvdm(pd, USB_C_SAMSUNG_SID,
				&phy_data->uvdm_data_obj[1], 
				SEC_UVDM_OUTREQ_NUMOBJ);
			pr_info("%s : d0 = %x,d1 = %x,d2 = %x,d3 = %x,d4 = %x,d5 = %x,d6 = %x,d7 = %x\n",
				__func__, phy_data->uvdm_data_obj[0],
				phy_data->uvdm_data_obj[1],
				phy_data->uvdm_data_obj[2],
				phy_data->uvdm_data_obj[3],
				phy_data->uvdm_data_obj[4],
				phy_data->uvdm_data_obj[5],
				phy_data->uvdm_data_obj[6],
				phy_data->uvdm_data_obj[7]);

			time_left =
				wait_for_completion_interruptible_timeout(
					&phy_data->uvdm_out_wait,
					msecs_to_jiffies(SEC_UVDM_WAIT_MS));
			if (time_left <= 0) {
				pr_err("%s timeout\n", __func__);
#ifdef CONFIG_USB_NOTIFY_PROC_LOG
				event = NOTIFY_EXTRA_UVDM_TIMEOUT;
				store_usblog_notify(NOTIFY_EXTRA, (void *)&event, NULL);
#endif
				return -ETIME;
			}
			accumulated_data_size += cur_set_data;
			remained_data_size -= cur_set_data;
			if (phy_data->uvdm_first_req)
				phy_data->uvdm_first_req = false;
			cur_set_num++;
		}
	}
	return size;
}

int samsung_usbpd_uvdm_in_request_message(void *data)
{
	uint8_t in_data[MAX_INPUT_DATA] = {0,};
	s_uvdm_header	SEC_RES_HEADER;
	s_tx_header	SEC_TX_HEADER;
	s_tx_tailer	SEC_TX_TAILER;
	data_obj_type	uvdm_data_obj[USBPD_MAX_COUNT_MSG_OBJECT];
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
	struct device *ccic_device = get_ccic_device(); 
	pccic_data_t ccic_data;
	struct pm6150_phydrv_data *phy_data;
	struct usbpd *pd;
#ifdef CONFIG_USB_NOTIFY_PROC_LOG
	int event;
#endif
	if (!ccic_device) { 
		pr_err("%s: ccic_device is null.\n", __func__); 
		return -ENODEV;
	}
	ccic_data = dev_get_drvdata(ccic_device);
	if (!ccic_data) {
		pr_err("ccic_data is null\n");
		return -ENXIO;
	}
	phy_data = ccic_data->drv_data;
	if (!phy_data) {
		pr_err("phy_data is null\n");
		return -ENXIO;
	}
	pd = phy_data->pd;
	if (!pd) {
		pr_err("pd is null\n");
		return -ENXIO;
	}

	pr_info("%s\n", __func__);

	// initialize array
	phy_data->num_vdos = 0;
	memset(phy_data->uvdm_data_obj,0,sizeof(phy_data->uvdm_data_obj));	

	phy_data->uvdm_dir = DIR_IN;
	phy_data->uvdm_first_req = true;

	/* 2. Common : Fill the MSGHeader */
	set_msg_header(&phy_data->uvdm_msg_header, USBPD_Vendor_Defined,
			SEC_UVDM_INREQ_NUMOBJ);
	/* 3. Common : Fill the UVDMHeader*/
	set_uvdm_header(&phy_data->uvdm_data_obj[0], SAMSUNG_VENDOR_ID, 0);

	/* 4. Common : Fill the First SEC_VDMHeader*/
	if (phy_data->uvdm_first_req)
		set_sec_uvdm_header(&phy_data->uvdm_data_obj[0],
				phy_data->Product_ID,
				SEC_UVDM_LONG_DATA,
				SEC_UVDM_ININIATOR, DIR_IN, 0, 0);
	
	/* 5. Send data to PDIC */
	reinit_completion(&phy_data->uvdm_in_wait);

	pr_info("%s : request d0 = %x,d1 = %x,d2 = %x,d3 = %x,d4 = %x,d5 = %x,d6 = %x,d7 = %x\n",
				__func__, phy_data->uvdm_data_obj[0],
				phy_data->uvdm_data_obj[1],
				phy_data->uvdm_data_obj[2],
				phy_data->uvdm_data_obj[3],
				phy_data->uvdm_data_obj[4],
				phy_data->uvdm_data_obj[5],
				phy_data->uvdm_data_obj[6],
				phy_data->uvdm_data_obj[7]);
	usbpd_send_uvdm(pd, USB_C_SAMSUNG_SID, &phy_data->uvdm_data_obj[1],
			SEC_UVDM_INREQ_NUMOBJ);


	// initialize array
	phy_data->num_vdos = 0;
	memset(phy_data->uvdm_data_obj,0,sizeof(phy_data->uvdm_data_obj)); 

	cur_set_num = 0;
	total_set_num = 1;

	do {
		time_left =
			wait_for_completion_interruptible_timeout(
					&phy_data->uvdm_in_wait,
					msecs_to_jiffies(SEC_UVDM_WAIT_MS));
		if (time_left <= 0) {
			pr_err("%s timeout\n", __func__);
#ifdef CONFIG_USB_NOTIFY_PROC_LOG
			event = NOTIFY_EXTRA_UVDM_TIMEOUT;
			store_usblog_notify(NOTIFY_EXTRA, (void *)&event, NULL);
#endif
			return -ETIME;
		}

		/* read data */
		for (i = 0; i < phy_data->num_vdos; i++)
			uvdm_data_obj[i].object = phy_data->uvdm_data_obj[i].object;

		pr_info("%s : read d0 = %x,d1 = %x,d2 = %x,d3 = %x,d4 = %x,d5 = %x,d6 = %x,d7 = %x\n",
				__func__, phy_data->uvdm_data_obj[0],
				phy_data->uvdm_data_obj[1],
				phy_data->uvdm_data_obj[2],
				phy_data->uvdm_data_obj[3],
				phy_data->uvdm_data_obj[4],
				phy_data->uvdm_data_obj[5],
				phy_data->uvdm_data_obj[6],
				phy_data->uvdm_data_obj[7]);

		if (phy_data->uvdm_first_req) {
			SEC_RES_HEADER.object = uvdm_data_obj[0].object;
			SEC_TX_HEADER.object = uvdm_data_obj[1].object;

			if (SEC_RES_HEADER.data_type == TYPE_SHORT) {
				in_data[rcv_data_size++] = SEC_RES_HEADER.data;
				return rcv_data_size;
			}
			/* 1. check the data size received */
			size = SEC_TX_HEADER.total_size;
			cur_set_data = SEC_TX_HEADER.cur_size;
			cur_set_num = SEC_TX_HEADER.order_cur_set;
			total_set_num = SEC_RES_HEADER.total_set_num;

			phy_data->uvdm_first_req = false;
			/* 2. copy data to buffer */
			for (i = 0; i < SEC_UVDM_MAXDATA_FIRST; i++)
				in_data[rcv_data_size++] =
					uvdm_data_obj[2+i/SEC_UVDM_ALIGN].byte[i%SEC_UVDM_ALIGN];
			total_rcv_size += cur_set_data;
			phy_data->uvdm_first_req = false;
		} else {
			SEC_TX_HEADER.object = uvdm_data_obj[0].object;
			cur_set_data = SEC_TX_HEADER.cur_size;
			cur_set_num = SEC_TX_HEADER.order_cur_set;
			/* 2. copy data to buffer */
			for (i = 0 ; i < SEC_UVDM_MAXDATA_NORMAL; i++)
				in_data[rcv_data_size++] =
					uvdm_data_obj[1+i/SEC_UVDM_ALIGN].byte[i%SEC_UVDM_ALIGN];
			total_rcv_size += cur_set_data;
		}
		/* 3. Check Checksum */
		SEC_TX_TAILER.object = uvdm_data_obj[5].object;
		cal_checksum =
			get_checksum((char *)&uvdm_data_obj[0], 0, SEC_UVDM_CHECKSUM_COUNT);
		ack = (cal_checksum == SEC_TX_TAILER.checksum) ? RX_ACK : RX_NAK;

		phy_data->num_vdos = 0;
		memset(phy_data->uvdm_data_obj,0,sizeof(phy_data->uvdm_data_obj)); 

		/* 5. Common : Fill the MSGHeader */
		set_msg_header(&phy_data->uvdm_msg_header,
				USBPD_Vendor_Defined, SEC_UVDM_INREQ_NUMOBJ);
		/* 5.1. Common : Fill the UVDMHeader*/
		set_uvdm_header(&phy_data->uvdm_data_obj[0],
				SAMSUNG_VENDOR_ID, 0);
		/* 5.2. Common : Fill the First SEC_VDMHeader*/
		set_sec_uvdm_rx_header(&phy_data->uvdm_data_obj[0],
				cur_set_num, cur_set_data, ack);
		
		reinit_completion(&phy_data->uvdm_in_wait);

		pr_info("%s : send d0 = %x,d1 = %x,d2 = %x,d3 = %x,d4 = %x,d5 = %x,d6 = %x,d7 = %x\n",
				__func__, phy_data->uvdm_data_obj[0],
				phy_data->uvdm_data_obj[1],
				phy_data->uvdm_data_obj[2],
				phy_data->uvdm_data_obj[3],
				phy_data->uvdm_data_obj[4],
				phy_data->uvdm_data_obj[5],
				phy_data->uvdm_data_obj[6],
				phy_data->uvdm_data_obj[7]);
		usbpd_send_uvdm(pd, USB_C_SAMSUNG_SID,
			&phy_data->uvdm_data_obj[1], SEC_UVDM_INREQ_NUMOBJ);
		phy_data->num_vdos = 0;
		memset(phy_data->uvdm_data_obj,0,sizeof(phy_data->uvdm_data_obj)); 
	} while (cur_set_num < total_set_num);

	set_endian(in_data, data, size);
	return size;
}

/* How to Call? */
static void samsung_usbpd_receive_uvdm_message(struct usbpd *pd, u32 vdm_hdr, const u32 *vdos, int num_vdos)
{
	int i = 0;
	int num_objs;
//	msg_header_type		uvdm_msg_header;
	data_obj_type		uvdm_data_obj[USBPD_MAX_COUNT_MSG_OBJECT];
	s_uvdm_header SEC_UVDM_RES_HEADER;
	s_rx_header SEC_UVDM_RX_HEADER;
	struct device *ccic_device = get_ccic_device();
	pccic_data_t ccic_data;
	struct pm6150_phydrv_data *phy_data;

	if (!ccic_device) { 
		pr_err("%s: ccic_device is null.\n", __func__); 
		return;
	}
	ccic_data = dev_get_drvdata(ccic_device);
	if (!ccic_data) {
		pr_err("ccic_data is null\n");
		return;
	}
	phy_data = ccic_data->drv_data;
	if (!phy_data) {
		pr_err("phy_data is null\n");
		return;
	}
	
	num_objs = num_vdos;
	for (i = 0; i < num_objs; i++) {
		uvdm_data_obj[i].object = vdos[i];
	}
	phy_data->num_vdos = num_vdos;
	for (i = 0; i < phy_data->num_vdos; i++) {
		phy_data->uvdm_data_obj[i].object = vdos[i];
	}

	pr_info("%s : d0 = %x,d1 = %x,d2 = %x,d3 = %x,d4 = %x,d5 = %x,d6 = %x,d7 = %x\n",
				__func__, phy_data->uvdm_data_obj[0],
				phy_data->uvdm_data_obj[1],
				phy_data->uvdm_data_obj[2],
				phy_data->uvdm_data_obj[3],
				phy_data->uvdm_data_obj[4],
				phy_data->uvdm_data_obj[5],
				phy_data->uvdm_data_obj[6],
				phy_data->uvdm_data_obj[7]);
	
	pr_info("%s dir %s\n", __func__,
		(phy_data->uvdm_dir == DIR_OUT)?"OUT":"IN");
	if (phy_data->uvdm_dir == DIR_OUT) {
		if (phy_data->uvdm_first_req) {
			SEC_UVDM_RES_HEADER.object = uvdm_data_obj[0].object;
			if (SEC_UVDM_RES_HEADER.data_type == TYPE_LONG) {
				if (SEC_UVDM_RES_HEADER.cmd_type == RES_ACK) {
					SEC_UVDM_RX_HEADER.object = uvdm_data_obj[1].object;
					if (SEC_UVDM_RX_HEADER.result_value != RX_ACK)
						pr_err("%s Busy or Nak received.\n",
							__func__);
				} else
					pr_err("%s Response type is wrong.\n",
						__func__);
			} else {
				if (SEC_UVDM_RES_HEADER.cmd_type == RES_ACK)
					pr_err("%s Short packet: ack received\n",
						__func__);
				else
					pr_err("%s Short packet: Response type is wrong\n",
						__func__);
			}
		/* Dir: out */
		} else {
			SEC_UVDM_RX_HEADER.object = uvdm_data_obj[0].object;
			if (SEC_UVDM_RX_HEADER.result_value != RX_ACK)
				pr_err("%s Busy or Nak received.\n", __func__);
		}
		complete(&phy_data->uvdm_out_wait);
	} else {
		if (phy_data->uvdm_first_req) {
			SEC_UVDM_RES_HEADER.object = uvdm_data_obj[0].object;
			if (SEC_UVDM_RES_HEADER.cmd_type != RES_ACK) {
				pr_err("%s Busy or Nak received.\n", __func__);
				return;
			}
		}
		complete(&phy_data->uvdm_in_wait);
	}
}

static const char *samsung_usbpd_cmd_name(u8 cmd)
{
	switch (cmd) {
	case USBPD_SVDM_DISCOVER_MODES: return "USBPD_SVDM_DISCOVER_MODES";
	case USBPD_SVDM_ENTER_MODE: return "USBPD_SVDM_ENTER_MODE";
	default: return "SAMSUNG_USBPD_VDM_ERROR";
	}
}

static void samsung_usbpd_send_event(struct samsung_usbpd_private *pd,
		enum samsung_usbpd_events event)
{
	switch (event) {
	case SAMSUNG_USBPD_EVT_DISCOVER:
		usbpd_send_svdm(pd->pd, USB_C_SAMSUNG_SID,
			USBPD_SVDM_DISCOVER_MODES,
			SVDM_CMD_TYPE_INITIATOR, 0x0, 0x0, 0x0);
		break;
	case SAMSUNG_USBPD_EVT_ENTER:
		usbpd_send_svdm(pd->pd, USB_C_SAMSUNG_SID,
			USBPD_SVDM_ENTER_MODE,
			SVDM_CMD_TYPE_INITIATOR, 0x1, 0x0, 0x0);
		break;
	case SAMSUNG_USBPD_EVT_EXIT:
		usbpd_send_svdm(pd->pd, USB_C_SAMSUNG_SID,
			USBPD_SVDM_EXIT_MODE,
			SVDM_CMD_TYPE_INITIATOR, 0x1, 0x0, 0x0);
		break;
	default:
		pr_err("unknown event:%d\n", event);
	}
}

static void samsung_usbpd_connect_cb(struct usbpd_svid_handler *hdlr,bool peer_usb_comm )
{
	struct samsung_usbpd_private *pd;

	pd = container_of(hdlr, struct samsung_usbpd_private, svid_handler);
	if (!pd) {
		pr_err("get_usbpd phandle failed\n");
		return;
	}

	pr_debug("\n");
	samsung_usbpd_send_event(pd, SAMSUNG_USBPD_EVT_DISCOVER);
}

static void samsung_usbpd_disconnect_cb(struct usbpd_svid_handler *hdlr)
{
	struct samsung_usbpd_private *pd;

	pd = container_of(hdlr, struct samsung_usbpd_private, svid_handler);
	if (!pd) {
		pr_err("get_usbpd phandle failed\n");
		return;
	}

	pd->alt_mode = SAMSUNG_USBPD_ALT_MODE_NONE;
	pr_debug("\n");
}

static int samsung_usbpd_validate_callback(u8 cmd,
	enum usbpd_svdm_cmd_type cmd_type, int num_vdos)
{
	int ret = 0;

	if (cmd_type == SVDM_CMD_TYPE_RESP_NAK) {
		pr_err("error: NACK\n");
		ret = -EINVAL;
		goto end;
	}

	if (cmd_type == SVDM_CMD_TYPE_RESP_BUSY) {
		pr_err("error: BUSY\n");
		ret = -EBUSY;
		goto end;
	}

	if (cmd_type != SVDM_CMD_TYPE_RESP_ACK) {
		pr_err("error: invalid cmd type\n");
		ret = -EINVAL;
	}	
end:
	return ret;
}

static void samsung_usbpd_response_cb(struct usbpd_svid_handler *hdlr, u8 cmd,
				enum usbpd_svdm_cmd_type cmd_type,
				const u32 *vdos, int num_vdos)
{
	struct samsung_usbpd_private *pd;

	pd = container_of(hdlr, struct samsung_usbpd_private, svid_handler);

	pr_debug("callback -> cmd: %s, *vdos = 0x%x, num_vdos = %d\n",
				samsung_usbpd_cmd_name(cmd), *vdos, num_vdos);

	if (samsung_usbpd_validate_callback(cmd, cmd_type, num_vdos)) {
		pr_debug("invalid callback received\n");
		return;
	}

	switch (cmd) {
	case USBPD_SVDM_DISCOVER_MODES:
		pd->phy_data->is_samsung_accessory_enter_mode = 0;
		pd->vdo = *vdos;
		pd->alt_mode |= SAMSUNG_USBPD_ALT_MODE_DISCOVER;
		samsung_usbpd_send_event(pd, SAMSUNG_USBPD_EVT_ENTER);
		break;
	case USBPD_SVDM_ENTER_MODE:
		pd->phy_data->is_samsung_accessory_enter_mode = 1;
		pd->alt_mode |= SAMSUNG_USBPD_ALT_MODE_ENTER;
		break;
	default:
		pr_err("unknown cmd: %d\n", cmd);
		break;
	}
}

static struct pm6150_phydrv_data *link_phy_handler(struct samsung_usbpd_private *usbpd)
{
	struct device *ccic_device = get_ccic_device(); 
	pccic_data_t ccic_data;
	struct pm6150_phydrv_data *phy_data;

	if (!ccic_device) { 
		pr_err("%s: ccic_device is null.\n", __func__);
		return NULL;
	}
	ccic_data = dev_get_drvdata(ccic_device);
	if (!ccic_data) {
 		pr_err("ccic_data is null\n");
		return NULL;
	}
	phy_data = ccic_data->drv_data;
	if (!phy_data) {
 		pr_err("phy_data is null\n");
		return NULL;
	}

	phy_data->samsung_usbpd = usbpd;
	phy_data->pd = usbpd->pd;
	return phy_data;
}

static void samsung_usbpd_vdm_response_cb(struct usbpd_svid_handler *hdlr, u32 vdm_hdr,
							const u32 *vdos, int num_vdos)
{
	struct samsung_usbpd_private *pd;
	
	pd = container_of(hdlr, struct samsung_usbpd_private, svid_handler);

	samsung_usbpd_receive_uvdm_message(pd->pd, vdm_hdr, vdos, num_vdos);
}

int samsung_usbpd_get(struct device *dev, struct samsung_usbpd_private *usbpd)
{
	int rc = 0;
	const char *pd_phandle = "qcom,samsung-usbpd-detection";
	struct usbpd *pd = NULL;
	struct usbpd_svid_handler svid_handler = {
		.svid		= USB_C_SAMSUNG_SID,
		.vdm_received	= &samsung_usbpd_vdm_response_cb,
		.connect	= &samsung_usbpd_connect_cb,
		.svdm_received	= &samsung_usbpd_response_cb,
		.disconnect	= &samsung_usbpd_disconnect_cb,
	};

	pd = devm_usbpd_get_by_phandle(dev, pd_phandle);
	if (IS_ERR(pd)) {
		pr_err("usbpd phandle failed (%ld)\n", PTR_ERR(pd));
		rc = PTR_ERR(pd);
		goto error;
	}

	usbpd->dev = dev;
	usbpd->pd = pd;
	usbpd->svid_handler = svid_handler;
	usbpd->phy_data = link_phy_handler(usbpd);
	rc = usbpd_register_svid(usbpd->pd, &usbpd->svid_handler);
	if (rc)
		pr_err("pd registration failed\n");
	pr_info("%s : usbpd_register_svid success \n", __func__);
	return rc;
error:
	rc= -1;
	return rc;
}

void samsung_usbpd_put(struct device *dev, struct samsung_usbpd_private *usbpd)
{
	usbpd_unregister_svid(usbpd->pd, &usbpd->svid_handler);
}

static int samsung_usbpd_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct samsung_usbpd_private *sec_pd;

	pr_info("%s : ++\n", __func__);
	if (!pdev || !pdev->dev.of_node) {
		pr_err("pdev not found\n");
		rc = -ENODEV;
		goto bail;
	}

	sec_pd = devm_kzalloc(&pdev->dev, sizeof(*sec_pd), GFP_KERNEL);
	if (!sec_pd) {
		rc = -ENOMEM;
		goto bail;
	}

	sec_pd->pdev = pdev;
	sec_pd->name = "samsung_usbpd";

	platform_set_drvdata(pdev, sec_pd);

	rc = samsung_usbpd_get(&pdev->dev, sec_pd);
	if (rc < 0) {
		pr_info("%s : probe error %d\n", __func__, rc);
		goto error;
	}

	pr_info("%s : --\n", __func__);
	return rc;
error:
	devm_kfree(&pdev->dev, sec_pd);
bail:
	return rc;
}

static int samsung_usbpd_remove(struct platform_device *pdev)
{
	struct samsung_usbpd_private *sec_pd;

	if (!pdev)
		return -EINVAL;

	sec_pd = platform_get_drvdata(pdev);

	samsung_usbpd_put(&pdev->dev, sec_pd);
	platform_set_drvdata(pdev, NULL);
	devm_kfree(&pdev->dev, sec_pd);

	return 0;
}

static const struct of_device_id samsung_usbpd_dt_match[] = {
	{.compatible = "samsung,usb-pd"},
	{}
};

static struct platform_driver samsung_usbpd_driver = {
	.probe  = samsung_usbpd_probe,
	.remove = samsung_usbpd_remove,
	.driver = {
		.name = "samsung_usbpd",
		.of_match_table = samsung_usbpd_dt_match,
	},
};

static int __init samsung_usbpd_init(void)
{
	int ret;

	pr_info("%s : ++\n", __func__);
	ret = platform_driver_register(&samsung_usbpd_driver);
	if (ret) {
		pr_err("driver register failed");
		return ret;
	}
	pr_info("%s : --\n", __func__);
	return ret;
}
late_initcall(samsung_usbpd_init);

static void __exit samsung_usbpd_cleanup(void)
{
	platform_driver_unregister(&samsung_usbpd_driver);
}
module_exit(samsung_usbpd_cleanup);

