/*
 *
 * Copyright (C) 2017 Samsung Electronics
 *
 * Author:Wookwang Lee. <wookwang.lee@samsung.com>,
 * Author:Guneet Singh Khurana  <gs.khurana@samsung.com>,
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef __LINUX_CCIC_CORE_H__
#define __LINUX_CCIC_CORE_H__

/* CCIC Dock Observer Callback parameter */
enum {
	CCIC_DOCK_DETACHED	= 0,
	CCIC_DOCK_HMT		= 105,	/* Samsung Gear VR */
	CCIC_DOCK_ABNORMAL	= 106,
	CCIC_DOCK_MPA		= 109,	/* Samsung Multi Port Adaptor */
	CCIC_DOCK_DEX		= 110,	/* Samsung Dex */
	CCIC_DOCK_HDMI		= 111,	/* Samsung HDMI Dongle */
	CCIC_DOCK_T_VR		= 112,
	CCIC_DOCK_UVDM		= 113,
	CCIC_DOCK_DEXPAD	= 114,
	CCIC_DOCK_TYPEC_ANALOG_EARPHONE	= 115,	/* RA/RA TypeC Analog Earphone*/
	CCIC_DOCK_NEW		= 200,  /* For New uevent */
};
typedef enum {
    TYPE_C_DETACH = 0,
    TYPE_C_ATTACH_DFP = 1, /* Host */
    TYPE_C_ATTACH_UFP = 2, /* Device */
    TYPE_C_ATTACH_DRP = 3, /* Dual role */
    TYPE_C_PR_SWAP = 4,
    TYPE_C_DR_SWAP = 5,
} CCIC_OTP_MODE;

#if defined(CONFIG_TYPEC)
typedef enum {
	TRY_ROLE_SWAP_NONE = 0,
	TRY_ROLE_SWAP_PR = 1, /* pr_swap */
	TRY_ROLE_SWAP_DR = 2, /* dr_swap */
	TRY_ROLE_SWAP_TYPE = 3, /* type */
} CCIC_ROLE_SWAP_MODE;

#define TRY_ROLE_SWAP_WAIT_MS 5000
#endif
#define DUAL_ROLE_SET_MODE_WAIT_MS 1500
#define GEAR_VR_DETACH_WAIT_MS		1000
#define SAMSUNG_PRODUCT_ID		0x6860
#define SAMSUNG_PRODUCT_TYPE	0x2
/* Samsung Acc VID */
#define SAMSUNG_VENDOR_ID		0x04E8
#define SAMSUNG_MPA_VENDOR_ID		0x04B4
/* Samsung Acc PID */
#define GEARVR_PRODUCT_ID		0xA500
#define GEARVR_PRODUCT_ID_1		0xA501
#define GEARVR_PRODUCT_ID_2		0xA502
#define GEARVR_PRODUCT_ID_3		0xA503
#define GEARVR_PRODUCT_ID_4		0xA504
#define GEARVR_PRODUCT_ID_5		0xA505
#define DEXDOCK_PRODUCT_ID		0xA020
#define HDMI_PRODUCT_ID			0xA025
#define MPA2_PRODUCT_ID			0xA027
#define UVDM_PROTOCOL_ID		0xA028
#define DEXPAD_PRODUCT_ID		0xA029
#define MPA_PRODUCT_ID			0x2122
#define FRIENDS_PRODUCT_ID		0xB002
/* Samsung UVDM structure */
#define SEC_UVDM_SHORT_DATA		0x0
#define SEC_UVDM_LONG_DATA		0x1
#define SEC_UVDM_ININIATOR		0x0
#define SEC_UVDM_RESPONDER_ACK	0x1
#define SEC_UVDM_RESPONDER_NAK	0x2
#define SEC_UVDM_RESPONDER_BUSY	0x3
#define SEC_UVDM_UNSTRUCTURED_VDM	0x0

#define SEC_UVDM_ALIGN (4)
#define SEC_UVDM_WAIT_MS (2000)
#define SEC_UVDM_MAXDATA_FIRST (12)
#define SEC_UVDM_MAXDATA_NORMAL (16)
#define SEC_UVDM_CHECKSUM_COUNT (20)

/*For DP Pin Assignment */
#define DP_PIN_ASSIGNMENT_NODE	0x00000000
#define DP_PIN_ASSIGNMENT_A	0x00000001	/* ( 1 << 0 ) */
#define DP_PIN_ASSIGNMENT_B	0x00000002	/* ( 1 << 1 ) */
#define DP_PIN_ASSIGNMENT_C	0x00000004	/* ( 1 << 2 ) */
#define DP_PIN_ASSIGNMENT_D	0x00000008	/* ( 1 << 3 ) */
#define DP_PIN_ASSIGNMENT_E	0x00000010	/* ( 1 << 4 ) */
#define DP_PIN_ASSIGNMENT_F	0x00000020	/* ( 1 << 5 ) */

#define MAX_INPUT_DATA (255)
#define USBPD_MAX_COUNT_MSG_OBJECT	(8) /* 0..7 */

#if !defined(CONFIG_CCIC_S2MU107) && !defined(CONFIG_CCIC_S2MU106)
#define USBPD_Vendor_Defined		0xF
#endif

#define SEC_UVDM_OUTREQ_NUMOBJ (7)
#define SEC_UVDM_INREQ_NUMOBJ (2)

typedef union {
	u16 word;
	u8  byte[2];

	struct {
		unsigned msg_type:5;
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
		unsigned vendor_defined:15;
		unsigned vdm_type:1;
		unsigned vendor_id:16;
	};
	struct {
		uint32_t	VDM_command:5,
				Rsvd2_VDM_header:1,
				VDM_command_type:2,
				Object_Position:3,
				Rsvd_VDM_header:2,
				Structured_VDM_Version:2,
				VDM_Type:1,
				Standard_Vendor_ID:16;
	} BITS;
} uvdm_header;

#if !defined(CONFIG_CCIC_S2MU107) && !defined(CONFIG_CCIC_S2MU106)
typedef union {
	u32 object;
	u16 word[2];
	u8  byte[4];

	struct {
		unsigned:30;
		unsigned supply_type:2;
	} power_data_obj_supply_type;

	struct {
		unsigned max_current:10;        /* 10mA units */
		unsigned voltage:10;            /* 50mV units */
		unsigned peak_current:2;
		unsigned reserved:3;
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
		unsigned reserved:5;
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
		unsigned max_current:7;		/* 50mA units */
		unsigned reserved1:1;
		unsigned min_voltage:8;		/* 100mV units  */
		unsigned reserved2:1;
		unsigned max_voltage:8;		/* 100mV units  */
		unsigned reserved3:2;
		unsigned pps_power_limited:1;
		unsigned pps_supply:2;
		unsigned supply_type:2;
	} power_data_obj_programmable;

	struct {
		unsigned op_current:7;		/* 50mA units */
		unsigned reserved1:2;
		unsigned output_voltage:11;	/* 20mV units */
		unsigned reserved2:3;
		unsigned unchunked_ext_msg_support:1;
		unsigned no_usb_suspend:1;
		unsigned usb_comm_capable:1;
		unsigned capability_mismatch:1;
		unsigned reserved3:1;
		unsigned object_position:3;
		unsigned reserved4:1;
	} request_data_object_programmable;

	struct {
		unsigned reserved:16;
		unsigned hot_swappable_batteries:4;
		unsigned fixed_batteries:4;
		unsigned type_of_alert:8;
	} alert_data_obejct;

	struct {
		unsigned min_current:10;	/* 10mA units */
		unsigned op_current:10;		/* 10mA units */
		unsigned:4;
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
		unsigned:4;
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

	struct{
		unsigned data:8;
		unsigned total_set_num:4;
		unsigned direction:1;
		unsigned cmd_type:2;
		unsigned data_type:1;
		unsigned pid:16;
	} sec_uvdm_header;

	struct {
		unsigned command:5;
		unsigned reserved1:1;
		unsigned command_type:2;
		unsigned obj_pos:3;
		unsigned reserved2:2;
		unsigned version:2;
		unsigned vdm_type:1;
		unsigned svid:16;
	} structured_vdm;

	struct {
		unsigned usb_vendor_id:16;
		unsigned reserved:10;
		unsigned modal_operation_supported:1;
		unsigned product_type:3;
		unsigned data_capable_usb_device:1;
		unsigned data_capable_usb_host:1;
	} id_header;

	struct {
		unsigned cert_test_id:20;
		unsigned reserved:12;
	} cert_stat_vdo;

	struct {
		unsigned device_version:16;
		unsigned product_id:16;
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

	struct{
		unsigned reserved:28;
		unsigned bist_mode:4;
	} bist_data_object;
} data_obj_type;
#endif

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

#if defined(CONFIG_CCIC_NOTIFIER)
struct ccic_state_work {
	struct work_struct ccic_work;
	int dest;
	int id;
	int attach;
	int event;
	int sub;
};
typedef enum {
	CLIENT_OFF = 0,
	CLIENT_ON = 1,
} CCIC_DEVICE_REASON;

typedef enum {
	HOST_OFF = 0,
	HOST_ON = 1,
} CCIC_HOST_REASON;
#endif

enum uvdm_data_type {
	TYPE_SHORT = 0,
	TYPE_LONG,
};

enum uvdm_direction_type {
	DIR_OUT = 0,
	DIR_IN,
};

struct uvdm_data {
	unsigned short pid; /* Product ID */
	char type; /* uvdm_data_type */
	char dir; /* uvdm_direction_type */
	unsigned int size; /* data size */
	void __user *pData; /* data pointer */
};
#ifdef CONFIG_COMPAT
struct uvdm_data_32 {
	unsigned short pid; /* Product ID */
	char type; /* uvdm_data_type */
	char dir; /* uvdm_direction_type */
	unsigned int size; /* data size */
	compat_uptr_t pData; /* data pointer */
};
#endif

struct ccic_misc_dev {
	struct uvdm_data u_data;
#ifdef CONFIG_COMPAT
	struct uvdm_data_32 u_data_32;
#endif
	atomic_t open_excl;
	atomic_t ioctl_excl;
	int (*uvdm_write)(void *data, int size);
	int (*uvdm_read)(void *data);
	int (*uvdm_ready)(void);
	void (*uvdm_close)(void);
};

typedef struct _ccic_data_t {
	const char *name;
	void *ccic_syfs_prop;
	void *drv_data;
	void (*set_enable_alternate_mode)(int);
	struct ccic_misc_dev *misc_dev;
} ccic_data_t, *pccic_data_t;

int ccic_core_init(void);
int ccic_core_register_chip(pccic_data_t pccic_data);
void ccic_core_unregister_chip(void);
int ccic_register_switch_device(int mode);
void ccic_send_dock_intent(int type);
void ccic_send_dock_uevent(u32 vid, u32 pid, int state);
void *ccic_core_get_drvdata(void);
int ccic_misc_init(pccic_data_t pccic_data);
void ccic_misc_exit(void);
/* SEC UVDM Utility function */
void set_endian(char *src, char *dest, int size);
int get_checksum(char *data, int start_addr, int size);
int set_uvdmset_count(int size);
void set_msg_header(void *data, int msg_type, int obj_num);
void set_uvdm_header(void *data, int vid, int vdm_type);
void set_sec_uvdm_header(void *data, int pid, bool data_type, int cmd_type,
		bool dir, int total_set_num, uint8_t received_data);
int get_data_size(int first_set, int remained_data_size);
void set_sec_uvdm_tx_header(void *data, int first_set, int cur_set, int total_size,
		int remained_size);
void set_sec_uvdm_tx_tailer(void *data);
void set_sec_uvdm_rx_header(void *data, int cur_num, int cur_set, int ack);
struct device *get_ccic_device(void);
#endif

