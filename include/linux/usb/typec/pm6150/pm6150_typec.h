/*
 * Copyrights (C) 2019 Samsung Electronics, Inc.
 * Copyrights (C) 2019 Silicon Mitus, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __PM6150_TYPEC_H__
#define __PM6150_TYPEC_H__

#if defined(CONFIG_CCIC_NOTIFIER)
#include <linux/usb/typec/pdic_notifier.h>
#include <linux/usb/typec/pdic_core.h>
#endif
#if defined(CONFIG_DUAL_ROLE_USB_INTF)
#include <linux/usb/class-dual-role.h>
#elif defined(CONFIG_TYPEC)
#include <linux/usb/typec.h>
#endif
#include <linux/usb/usbpd.h>
#include <linux/power_supply.h>
#include <linux/usb/typec/pm6150/samsung_usbpd.h>

/* Function Status from s2mm005 definition */
typedef enum {
	pm6150_State_PE_Initial_detach	= 0,
	pm6150_State_PE_SRC_Send_Capabilities = 3,
	pm6150_State_PE_SNK_Wait_for_Capabilities = 17,
	pm6150_State_PE_SNK_Ready = 21,
} pm6150_pd_state_t;

struct pm6150_phydrv_data {
	struct device *dev;
	struct usbpd *pd;
#if defined(CONFIG_CCIC_NOTIFIER)
	struct workqueue_struct *ccic_wq;
#endif
	bool is_water_detect;
	int rid;	
	int pd_state;
	int cable;

	/* To Support Samsung UVDM Protocol Message */
	uint32_t acc_type;
	uint32_t Vendor_ID;
	uint32_t Product_ID;
	uint32_t Device_Version;
	struct delayed_work	acc_detach_handler;
	uint32_t is_samsung_accessory_enter_mode;
	bool pn_flag;
	struct samsung_usbpd_private *samsung_usbpd;
	uint32_t dr_swap_cnt;

	int num_vdos;
	msg_header_type 	uvdm_msg_header;
	data_obj_type		uvdm_data_obj[USBPD_MAX_COUNT_MSG_OBJECT];
	bool uvdm_first_req;
	bool uvdm_dir;
	struct completion uvdm_out_wait;
	struct completion uvdm_in_wait;
};

extern int samsung_usbpd_uvdm_ready(void);
extern void samsung_usbpd_uvdm_close(void);
extern int samsung_usbpd_uvdm_out_request_message(void *data, int size);
extern int samsung_usbpd_uvdm_in_request_message(void *data);
#if defined(CONFIG_CCIC_NOTIFIER)
void pm6150_ccic_event_work(int dest, int id, int attach, int event, int sub);
int pm6150_usbpd_create(struct device* dev, struct pm6150_phydrv_data** parent_data);
int pm6150_usbpd_destroy(void);
#endif
#endif /* __PM6150_TYPEC_H__ */
