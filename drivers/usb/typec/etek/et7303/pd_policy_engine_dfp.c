/*
 * Copyright (C) 2016 Richtek Technology Corp.
 *
 * Power Delivery Policy Engine for DFP
 *
 * Author: TH <tsunghan_tsai@richtek.com>
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/usb/typec/etek/et7303/pd_core.h>
#include <linux/usb/typec/etek/et7303/pd_dpm_core.h>
#include <linux/usb/typec/etek/et7303/tcpci.h>
#include <linux/usb/typec/etek/et7303/pd_policy_engine.h>

#define VDM_HEAD_SVID(head)	((head >> 16) & 0xffff)
#define VDM_HEAD_TYPE(head)	((head >> 15) & 1) /* checked at pd_process_evt_vdm.c */
#define VDM_HEAD_OBJ_POS(head)	((head >> 8) & 7)
#define VDM_HEAD_CMD_TYPE(head)	((head >> 6) & 3) /* checked at pd_process_evt_vdm.c */
#define VDM_HEAD_CMD(head)	((head) & 0x1f)a

#define ID_HEAD_PRODUCT_TYPE(head)	((head >> 27) & 7)

/*
 * [PD2.0] Figure 8-64 DFP to UFP VDM Discover Identity State Diagram
 */

void pe_dfp_ufp_vdm_identity_request_entry(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
	pd_send_vdm_discover_id(pd_port, TCPC_TX_SOP);
}

void pe_dfp_ufp_vdm_identity_acked_entry(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
	uint32_t vdm_hdr = pd_event->pd_msg->payload[0];
	uint32_t vdm_id = pd_event->pd_msg->payload[1];

	pd_disable_timer(pd_port, PD_TIMER_VDM_RESPONSE);

	if (VDM_HEAD_SVID(vdm_hdr) != USB_SID_PD ||
	    VDM_HEAD_OBJ_POS(vdm_hdr) ||
	    ID_HEAD_PRODUCT_TYPE(vdm_id) == 4 ||
	    ID_HEAD_PRODUCT_TYPE(vdm_id) == 7 ||
	    PD_HEADER_CNT(pd_event->pd_msg->msg_hdr) < 5) {
		pr_info("%s: bad identity response -> vdm_hdr = 0x%x, vdm_id = 0x%x, msg_hdr = 0x%x\n",
			__func__, vdm_hdr, vdm_id, pd_event->pd_msg->msg_hdr);
		pd_dpm_dfp_inform_id(pd_port, pd_event, false);
	} else {
		pd_dpm_dfp_inform_id(pd_port, pd_event, true);
	}
	pd_free_pd_event(pd_port, pd_event);
}

void pe_dfp_ufp_vdm_identity_naked_entry(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
	pd_disable_timer(pd_port, PD_TIMER_VDM_RESPONSE);
	pd_dpm_dfp_inform_id(pd_port, pd_event, false);
	pd_free_pd_event(pd_port, pd_event);
}

/*
 * [PD2.0] Figure 8-65 DFP VDM Discover Identity State Diagram
 */

void pe_dfp_cbl_vdm_identity_request_entry(
			struct pd_port *pd_port, struct pd_event *pd_event)
{
	pd_send_vdm_discover_id(pd_port, TCPC_TX_SOP_PRIME);
	pd_port->discover_id_counter++;

	pd_enable_timer(pd_port, PD_TIMER_VDM_RESPONSE);
	pd_free_pd_event(pd_port, pd_event);
}

void pe_dfp_cbl_vdm_identity_acked_entry(
			struct pd_port *pd_port, struct pd_event *pd_event)
{
	pd_port->dpm_flags &=
		~(DPM_FLAGS_CHECK_CABLE_ID | DPM_FLAGS_CHECK_CABLE_ID_DFP);

	pd_disable_timer(pd_port, PD_TIMER_VDM_RESPONSE);
	pd_dpm_inform_cable_id(pd_port, pd_event);

	vdm_put_dpm_notified_event(pd_port);
	pd_free_pd_event(pd_port, pd_event);
}

void pe_dfp_cbl_vdm_identity_naked_entry(
			struct pd_port *pd_port, struct pd_event *pd_event)
{
	pd_disable_timer(pd_port, PD_TIMER_VDM_RESPONSE);

	vdm_put_dpm_notified_event(pd_port);
	pd_free_pd_event(pd_port, pd_event);
}

/*
 * [PD2.0] Figure 8-66 DFP VDM Discover SVIDs State Diagram
 */

void pe_dfp_vdm_svids_request_entry(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
	pd_send_vdm_discover_svids(pd_port, TCPC_TX_SOP);
}

void pe_dfp_vdm_svids_acked_entry(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
	pd_disable_timer(pd_port, PD_TIMER_VDM_RESPONSE);
	pd_dpm_dfp_inform_svids(pd_port, pd_event, true);
	pd_free_pd_event(pd_port, pd_event);
}

void pe_dfp_vdm_svids_naked_entry(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
	pd_disable_timer(pd_port, PD_TIMER_VDM_RESPONSE);
	pd_dpm_dfp_inform_svids(pd_port, pd_event, false);
	pd_free_pd_event(pd_port, pd_event);
}

/*
 * [PD2.0] Figure 8-67 DFP VDM Discover Modes State Diagram
 */

void pe_dfp_vdm_modes_request_entry(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
	pd_send_vdm_discover_modes(pd_port, TCPC_TX_SOP, pd_port->mode_svid);
}

void pe_dfp_vdm_modes_acked_entry(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
	pd_disable_timer(pd_port, PD_TIMER_VDM_RESPONSE);
	pd_dpm_dfp_inform_modes(pd_port, pd_event, true);
	pd_free_pd_event(pd_port, pd_event);
}

void pe_dfp_vdm_modes_naked_entry(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
	pd_disable_timer(pd_port, PD_TIMER_VDM_RESPONSE);
	pd_dpm_dfp_inform_modes(pd_port, pd_event, false);
	pd_free_pd_event(pd_port, pd_event);
}

/*
 * [PD2.0] Figure 8-68 DFP VDM Mode Entry State Diagram
 */

void pe_dfp_vdm_mode_entry_request_entry(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
	pd_send_vdm_enter_mode(pd_port, TCPC_TX_SOP,
		pd_port->mode_svid, pd_port->mode_obj_pos);
}

void pe_dfp_vdm_mode_entry_acked_entry(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
	pd_disable_timer(pd_port, PD_TIMER_VDM_MODE_ENTRY);
	pd_dpm_dfp_inform_enter_mode(pd_port, pd_event, true);
	pd_free_pd_event(pd_port, pd_event);

}

void pe_dfp_vdm_mode_entry_naked_entry(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
	pd_disable_timer(pd_port, PD_TIMER_VDM_MODE_ENTRY);
	pd_dpm_dfp_inform_enter_mode(pd_port, pd_event, false);
	pd_free_pd_event(pd_port, pd_event);
}

/*
 * [PD2.0] Figure 8-69 DFP VDM Mode Exit State Diagram
 */

void pe_dfp_vdm_mode_exit_request_entry(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
	pd_send_vdm_exit_mode(pd_port, TCPC_TX_SOP,
		pd_port->mode_svid, pd_port->mode_obj_pos);
}

void pe_dfp_vdm_mode_exit_acked_entry(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
	pd_disable_timer(pd_port, PD_TIMER_VDM_MODE_EXIT);
	pd_dpm_dfp_inform_exit_mode(pd_port, pd_event);
	pd_free_pd_event(pd_port, pd_event);
}

/*
 * [PD2.0] Figure 8-70 DFP VDM Attention State Diagram
 */

void pe_dfp_vdm_attention_request_entry(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
	pd_dpm_dfp_inform_attention(pd_port, pd_event);
	pd_free_pd_event(pd_port, pd_event);
}

void pe_dfp_vdm_unknown_entry(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
	pd_port->dpm_ack_immediately = true;
	pd_free_pd_event(pd_port, pd_event);
}

/*
 * [PD2.0] Figure 8-83 DFP Cable Soft Reset or Cable Reset State Diagram
 */

#ifdef CONFIG_PD_DFP_RESET_CABLE

void pe_dfp_cbl_send_soft_reset_entry(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
	pd_port->pd_wait_sender_response = true;

	pd_send_cable_soft_reset(pd_port);
	pd_free_pd_event(pd_port, pd_event);
}

void pe_dfp_cbl_send_cable_reset_entry(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
	/* TODO : we don't do cable reset now */
}

#endif	/* CONFIG_PD_DFP_RESET_CABLE */

/*
 * [PD2.0] Display Port
 */

#ifdef CONFIG_USB_PD_ALT_MODE_DFP

void pe_dfp_vdm_dp_status_update_request_entry(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
	pd_dpm_dfp_send_dp_status_update(pd_port, pd_event);
}

void pe_dfp_vdm_dp_status_update_acked_entry(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
	pd_disable_timer(pd_port, PD_TIMER_VDM_RESPONSE);
	pd_dpm_dfp_inform_dp_status_update(pd_port, pd_event, true);
	pd_free_pd_event(pd_port, pd_event);
}

void pe_dfp_vdm_dp_status_update_naked_entry(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
	pd_disable_timer(pd_port, PD_TIMER_VDM_RESPONSE);
	pd_dpm_dfp_inform_dp_status_update(pd_port, pd_event, false);
	pd_free_pd_event(pd_port, pd_event);
}

void pe_dfp_vdm_dp_configuration_request_entry(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
	pd_dpm_dfp_send_dp_configuration(pd_port, pd_event);
}

void pe_dfp_vdm_dp_configuration_acked_entry(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
	pd_disable_timer(pd_port, PD_TIMER_VDM_RESPONSE);
	pd_dpm_dfp_inform_dp_configuration(pd_port, pd_event, true);
	pd_free_pd_event(pd_port, pd_event);
}

void pe_dfp_vdm_dp_configuration_naked_entry(
	struct pd_port *pd_port, struct pd_event *pd_event)
{
	pd_disable_timer(pd_port, PD_TIMER_VDM_RESPONSE);
	pd_dpm_dfp_inform_dp_configuration(pd_port, pd_event, false);
	pd_free_pd_event(pd_port, pd_event);

}

#endif	/* CONFIG_USB_PD_ALT_MODE_DFP */

/*
 * UVDM
 */

#ifdef CONFIG_USB_PD_UVDM

void pe_dfp_uvdm_send_entry(struct pd_port *pd_port, struct pd_event *pd_event)
{
	pd_dpm_dfp_send_uvdm(pd_port, pd_event);
}

void pe_dfp_uvdm_acked_entry(struct pd_port *pd_port, struct pd_event *pd_event)
{
	pd_disable_timer(pd_port, PD_TIMER_UVDM_RESPONSE);
	pd_dpm_dfp_inform_uvdm(pd_port, pd_event, true);
	pd_free_pd_event(pd_port, pd_event);
}

void pe_dfp_uvdm_naked_entry(struct pd_port *pd_port, struct pd_event *pd_event)
{
	pd_disable_timer(pd_port, PD_TIMER_UVDM_RESPONSE);
	pd_dpm_dfp_inform_uvdm(pd_port, pd_event, false);
	pd_free_pd_event(pd_port, pd_event);
}

#endif      /* CONFIG_USB_PD_UVDM */
