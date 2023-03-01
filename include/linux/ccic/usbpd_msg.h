#ifndef __USBPD_MSG_H__
#define __USBPD_MSG_H__

/* for header */
#define USBPD_REV_20	(1)
#define PD_SID		(0xFF00)
#define PD_SID_1	(0xFF01)

#define MAX_INPUT_DATA (255)
#define SEC_UVDM_ALIGN (4)
#define SEC_UVDM_WAIT_MS (5000)
#define SEC_UVDM_MAXDATA_FIRST (12)
#define SEC_UVDM_MAXDATA_NORMAL (16)
#define SEC_UVDM_CHECKSUM_COUNT (20)

enum uvdm_res_type {
	RES_INIT = 0,
	RES_ACK,
	RES_NAK,
	RES_BUSY,
};

enum uvdm_rx_type {
	RX_ACK = 0,
	RX_NAK,
	RX_BUSY,
};

typedef union {
	u16 word;
	u8  byte[2];

	struct {
		unsigned msg_type:4;
		unsigned rsvd:1;
		unsigned port_data_role:1;
		unsigned spec_revision:2;
		unsigned port_power_role:1;
		unsigned msg_id:3;
		unsigned num_data_objs:3;
		unsigned extended:1;
	};
} msg_header_type;

typedef union {
	u32 object;
	u16 word[2];
	u8  byte[4];

	struct {
		unsigned data_size:9;
		unsigned reserved:1;
		unsigned request_chunk:1;
		unsigned chunk_number:4;
		unsigned chunked:1;
		unsigned data : 16;
	} extended_msg_header_type;

	struct {
		unsigned:30;
		unsigned supply_type:2;
	} power_data_obj_supply_type;

	struct {
		unsigned max_current:10;        /* 10mA units */
		unsigned voltage:10;            /* 50mV units */
		unsigned peak_current:2;
		unsigned rsvd:3;
		unsigned data_role_swap:1;
		unsigned usb_comm_capable:1;
		unsigned externally_powered:1;
		unsigned usb_suspend_support:1;
		unsigned dual_role_power:1;
		unsigned supply:2;
	} power_data_obj;

	struct {
		unsigned op_current:10;	/* 10mA units */
		unsigned voltage:10;	/* 50mV units */
		unsigned rsvd:5;
		unsigned data_role_swap:1;
		unsigned usb_comm_capable:1;
		unsigned externally_powered:1;
		unsigned higher_capability:1;
		unsigned dual_role_power:1;
		unsigned supply_type:2;
	} power_data_obj_sink;

	struct {
		unsigned max_current:10;	/* 10mA units */
		unsigned min_voltage:10;	/* 50mV units */
		unsigned max_voltage:10;	/* 50mV units */
		unsigned supply_type:2;
	} power_data_obj_variable;

	struct {
		unsigned max_power:10;		/* 250mW units */
		unsigned min_voltage:10;	/* 50mV units  */
		unsigned max_voltage:10;	/* 50mV units  */
		unsigned supply_type:2;
	} power_data_obj_battery;

	struct {
		unsigned min_current:10;	/* 10mA units */
		unsigned op_current:10;		/* 10mA units */
		unsigned rsvd:4;
		unsigned no_usb_suspend:1;
		unsigned usb_comm_capable:1;
		unsigned capability_mismatch:1;
		unsigned give_back:1;
		unsigned object_position:3;
		unsigned:1;
	} request_data_object;

	struct {
		unsigned max_power:10;		/* 250mW units */
		unsigned op_power:10;		/* 250mW units */
		unsigned rsvd:4;
		unsigned no_usb_suspend:1;
		unsigned usb_comm_capable:1;
		unsigned capability_mismatch:1;
		unsigned give_back:1;
		unsigned object_position:3;
		unsigned:1;
	} request_data_object_battery;

	struct {
		unsigned vendor_defined:15;
		unsigned vdm_type:1;
		unsigned vendor_id:16;
	} unstructured_vdm;

	struct {
		unsigned data:8;
		unsigned total_number_of_uvdm_set:4;
		unsigned rsvd:1;
		unsigned cmd_type:2;
		unsigned data_type:1;
		unsigned pid:16;
	} sec_uvdm_header;

	struct {
		unsigned command:5;
		unsigned reserved:1;
		unsigned command_type:2;
		unsigned obj_pos:3;
		unsigned rsvd:2;
		unsigned version:2;
		unsigned vdm_type:1;
		unsigned svid:16;
	} structured_vdm;

	struct {
		unsigned USB_Vendor_ID:16;
		unsigned rsvd:10;
		unsigned Product_Type:3;
		unsigned Data_Capable_USB_Device:1;
		unsigned Data_Capable_USB_Host:1;
	} id_header_vdo;

	struct {
		unsigned Device_Version:16;
		unsigned USB_Product_ID:16;
	} product_vdo;

	struct {
		unsigned port_capability:2;
		unsigned displayport_protocol:4;
		unsigned receptacle_indication:1;
		unsigned usb_r2_signaling:1;
		unsigned dfp_d_pin_assignments:8;
		unsigned ufp_d_pin_assignments:8;
		unsigned rsvd:8;
	} displayport_capabilities;

	struct {
		unsigned port_connected:2;
		unsigned power_low:1;
		unsigned enabled:1;
		unsigned multi_function_preferred:1;
		unsigned usb_configuration_request:1;
		unsigned exit_displayport_mode_request:1;
		unsigned hpd_state:1;
		unsigned irq_hpd:1;
		unsigned rsvd:23;
	} displayport_status;

	struct{
		unsigned select_configuration:2;
		unsigned displayport_protocol:4;
		unsigned rsvd1:2;
		unsigned ufp_u_pin_assignment:8;
		unsigned rsvd2:16;
	} displayport_configurations;

	struct{
		unsigned svid_1:16;
		unsigned svid_0:16;
	} vdm_svid;
} data_obj_type;

typedef union {
	u32 object;
	u16 word[2];
	u8  byte[4];
	struct {
		unsigned vendor_defined:15;
		unsigned vdm_type:1;
		unsigned vendor_id:16;
	};
} uvdm_header;

typedef union {
	u32 object;
	u16 word[2];
	u8  byte[4];

	struct{
		unsigned data:8;
		unsigned total_set_num:4;
		unsigned direction:1;
		unsigned cmd_type:2;
		unsigned data_type:1;
		unsigned pid:16;
	};
} s_uvdm_header;

typedef union {
	u32 object;
	u16 word[2];
	u8  byte[4];

	struct{
		unsigned cur_size:8;
		unsigned total_size:8;
		unsigned reserved:12;
		unsigned order_cur_set:4;
	};
} s_tx_header;

typedef union {
	u32 object;
	u16 word[2];
	u8  byte[4];

	struct{
		unsigned checksum:16;
		unsigned reserved:16;
	};
} s_tx_tailer;

typedef union {
	u32 object;
	u16 word[2];
	u8  byte[4];

	struct{
		unsigned reserved:18;
		unsigned result_value:2;
		unsigned rcv_data_size:8;
		unsigned order_cur_set:4;
	};
} s_rx_header;

typedef enum {
	POWER_TYPE_FIXED = 0,
	POWER_TYPE_BATTERY,
	POWER_TYPE_VARIABLE,
} power_supply_type;

typedef enum {
	SOP_TYPE_SOP,
	SOP_TYPE_SOP1,
	SOP_TYPE_SOP2,
	SOP_TYPE_SOP1_DEBUG,
	SOP_TYPE_SOP2_DEBUG
} sop_type;

enum usbpd_control_msg_type {
#if 0
	USBPD_GoodCRC        = 0x1,
	USBPD_GotoMin        = 0x2,
	USBPD_Accept         = 0x3,
	USBPD_Reject         = 0x4,
	USBPD_Ping           = 0x5,
	USBPD_PS_RDY         = 0x6,
	USBPD_Get_Source_Cap = 0x7,
	USBPD_Get_Sink_Cap   = 0x8,
	USBPD_DR_Swap        = 0x9,
	USBPD_PR_Swap        = 0xA,
	USBPD_VCONN_Swap     = 0xB,
	USBPD_Wait           = 0xC,
	USBPD_Soft_Reset     = 0xD,
	USBPD_UVDM_MSG       = 0xE
#endif
	USBPD_GoodCRC        = 1,
	USBPD_GotoMin        = 2,
	USBPD_Accept         = 3,
	USBPD_Reject         = 4,
	USBPD_Ping           = 5,
	USBPD_PS_RDY         = 6,
	USBPD_Get_Source_Cap = 7,
	USBPD_Get_Sink_Cap   = 8,
	USBPD_DR_Swap        = 9,
	USBPD_PR_Swap        = 10,
	USBPD_VCONN_Swap     = 11,
	USBPD_Wait           = 12,
	USBPD_Soft_Reset     = 13,
	USBPD_Not_Supported  = 16,
	USBPD_Get_Source_Cap_Extended  = 17,
	USBPD_Get_Status	 = 18,
	USBPD_FR_Swap		 = 19,
	USBPD_Get_PPS_Status = 20,
	USBPD_Get_Country_Codes		   = 21,
	USBPD_Get_Sink_Cap_Extended	   = 22,
	USBPD_UVDM_MSG       = 23,
};

enum usbpd_check_msg_pass {
	NONE_CHECK_MSG_PASS,
	CHECK_MSG_PASS,
};

enum usbpd_port_data_role {
	USBPD_UFP,
	USBPD_DFP,
};

enum usbpd_port_power_role {
	USBPD_SINK,
	USBPD_SOURCE,
	USBPD_DRP,
};

enum usbpd_port_vconn_role {
	USBPD_VCONN_OFF,
	USBPD_VCONN_ON,
};

enum usbpd_power_role_swap {
	USBPD_SINK_OFF,
	USBPD_SINK_ON,
	USBPD_SOURCE_OFF,
	USBPD_SOURCE_ON,
	USBPD_PR_DONE,
};

enum usbpd_port_role {
	USBPD_Rp	= 0x01,
	USBPD_Rd	= 0x01 << 1,
	USBPD_Ra	= 0x01 << 2,
};

enum usbpd_port_rp_level {
    USBPD_56k   = 1,
    USBPD_22k   = 3,
    USBPD_10k   = 7,
};

enum {
	USBPD_CC_OFF,
	USBPD_CC_ON,
};

enum vdm_command_type{
	Initiator       = 0,
	Responder_ACK   = 1,
	Responder_NAK   = 2,
	Responder_BUSY  = 3
};

enum vdm_type{
	Unstructured_VDM = 0,
	Structured_VDM = 1
};

enum vdm_configure_type{
	USB		= 0,
	USB_U_AS_DFP_D	= 1,
	USB_U_AS_UFP_D	= 2
};

enum vdm_displayport_protocol{
	UNSPECIFIED	= 0,
	DP_V_1_3	= 1,
	GEN_2		= 1 << 1
};

enum dp_support {
	USBPD_NOT_DP = 0,
	USBPD_DP_SUPPORT = 1,
};

enum vdm_pin_assignment{
	DE_SELECT_PIN		= 0,
	PIN_ASSIGNMENT_A	= 1,
	PIN_ASSIGNMENT_B	= 1 << 1,
	PIN_ASSIGNMENT_C	= 1 << 2,
	PIN_ASSIGNMENT_D	= 1 << 3,
	PIN_ASSIGNMENT_E	= 1 << 4,
	PIN_ASSIGNMENT_F	= 1 << 5,
};

enum vdm_command_msg {
	Discover_Identity		= 1,
	Discover_SVIDs			= 2,
	Discover_Modes			= 3,
	Enter_Mode			= 4,
	Exit_Mode			= 5,
	Attention			= 6,
	DisplayPort_Status_Update	= 0x10,
	DisplayPort_Configure		= 0x11,
};

enum usbpd_connect_type {
	USBPD_UP_SIDE	= 1,
	USBPD_DOWN_SIDE	= 2,
	USBPD_UNDEFFINED_SIDE	= 3,
};


enum usbpd_data_msg_type {
#if 0
	USBPD_Source_Capabilities	= 0x1,
	USBPD_Request		        = 0x2,
	USBPD_BIST			= 0x3,
	USBPD_Sink_Capabilities		= 0x4,
	USBPD_Vendor_Defined		= 0xF,
#endif
	USBPD_Source_Capabilities	= 0x1,
	USBPD_Request		        = 0x2,
	USBPD_BIST					= 0x3,
	USBPD_Sink_Capabilities		= 0x4,
	USBPD_Battery_Status		= 0x5,
	USBPD_Alert					= 0x6,
	USBPD_Get_Country_Info		= 0x7,
	USBPD_Vendor_Defined		= 0xF,
};

enum usbpd_extended_msg_type {
	USBPD_Source_Capabilities_Extended	= 1,
	USBPD_Status						= 2,
	USBPD_Get_Battery_Cap				= 3,
	USBPD_Get_Battery_Status			= 4,
	USBPD_Battery_Capabilities			= 5,
	USBPD_Get_Manufacturer_Info			= 6,
	USBPD_Manufacturer_Info				= 7,
	USBPD_Security_Request				= 8,
	USBPD_Security_Response				= 9,
	USBPD_Firmware_Update_Request		= 10,
	USBPD_Firmware_Update_Response		= 11,
	USBPD_PPS_Status					= 12,
	USBPD_Country_Info					= 13,
	USBPD_Country_Codes					= 14,
	USBPD_Sink_Capabilities_Extended	= 15
};

enum usbpd_msg_type {
	USBPD_CTRL_MSG		= 0,
	USBPD_DATA_MSG		= 1,
	USBPD_EXTENDED_MSG		= 2,
};

#endif

