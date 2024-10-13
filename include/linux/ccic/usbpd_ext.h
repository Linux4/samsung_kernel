#if defined(CONFIG_CCIC_NOTIFIER)
#include <linux/ccic/ccic_notifier.h>
#endif
#if defined(CONFIG_DUAL_ROLE_USB_INTF)
#include <linux/usb/class-dual-role.h>
#elif defined(CONFIG_TYPEC)
#include <linux/usb/typec.h>
#endif

#ifndef __USBPD_EXT_H__
#define __USBPD_EXT_H__

#ifdef CONFIG_IFCONN_NOTIFIER
extern struct pdic_notifier_data pd_noti;
#endif

/* CCIC Dock Observer Callback parameter */
enum {
	CCIC_DOCK_DETACHED	= 0,
	CCIC_DOCK_HMT		= 105,	/* Samsung Gear VR */
	CCIC_DOCK_ABNORMAL	= 106,
	CCIC_DOCK_MPA		= 109,	/* Samsung Multi Port Adaptor */
	CCIC_DOCK_DEX		= 110,	/* Samsung Dex */
	CCIC_DOCK_HDMI		= 111,	/* Samsung HDMI Dongle */
	CCIC_DOCK_NEW		= 200,	/* For New Event  */
};

#define GEAR_VR_DETACH_WAIT_MS		(1000)

/* For DP */
#define TypeC_POWER_SINK_INPUT     0
#define TypeC_POWER_SOURCE_OUTPUT     1
#define TypeC_DP_SUPPORT	(0xFF01)

/* Samsung Acc VID */
#define SAMSUNG_VENDOR_ID		0x04E8
#define SAMSUNG_MPA_VENDOR_ID		0x04B4
#define TypeC_DP_SUPPORT	(0xFF01)
/* Samsung Acc PID */
#define GEARVR_PRODUCT_ID		0xA500
#define GEARVR_PRODUCT_ID_1		0xA501
#define GEARVR_PRODUCT_ID_2		0xA502
#define GEARVR_PRODUCT_ID_3		0xA503
#define GEARVR_PRODUCT_ID_4		0xA504
#define GEARVR_PRODUCT_ID_5		0xA505
#define DEXDOCK_PRODUCT_ID		0xA020
#define HDMI_PRODUCT_ID			0xA025
#define MPA_PRODUCT_ID			0x2122
/* Samsung UVDM structure */
#define SEC_UVDM_SHORT_DATA		0x0
#define SEC_UVDM_LONG_DATA		0x1
#define SEC_UVDM_ININIATOR		0x0
#define SEC_UVDM_RESPONDER_ACK	0x1
#define SEC_UVDM_RESPONDER_NAK	0x2
#define SEC_UVDM_RESPONDER_BUSY	0x3
#define SEC_UVDM_UNSTRUCTURED_VDM	0x4

/*For DP Pin Assignment */
#define DP_PIN_ASSIGNMENT_NODE	0x00000000
#define DP_PIN_ASSIGNMENT_A	0x00000001	/* ( 1 << 0 ) */
#define DP_PIN_ASSIGNMENT_B	0x00000002	/* ( 1 << 1 ) */
#define DP_PIN_ASSIGNMENT_C	0x00000004	/* ( 1 << 2 ) */
#define DP_PIN_ASSIGNMENT_D	0x00000008	/* ( 1 << 3 ) */
#define DP_PIN_ASSIGNMENT_E	0x00000010	/* ( 1 << 4 ) */
#define DP_PIN_ASSIGNMENT_F	0x00000020	/* ( 1 << 5 ) */

/* For DP VDM Modes VDO Port_Capability */
typedef enum {
    num_Reserved_Capable        = 0,
    num_UFP_D_Capable           = 1,
    num_DFP_D_Capable           = 2,
    num_DFP_D_and_UFP_D_Capable = 3
} Num_DP_Port_Capability_Type;

/* For DP VDM Modes VDO Receptacle_Indication */
typedef enum {
    num_USB_TYPE_C_PLUG        = 0,
    num_USB_TYPE_C_Receptacle  = 1
} Num_DP_Receptacle_Indication_Type;


/* For DP_Status_Update Port_Connected */
typedef enum {
    num_Adaptor_Disable         = 0,
    num_Connect_DFP_D           = 1,
    num_Connect_UFP_D           = 2,
    num_Connect_DFP_D_and_UFP_D = 3
} Num_DP_Port_Connected_Type;

/* For DP_Configure Select_Configuration */
typedef enum {
    num_Cfg_for_USB             = 0,
    num_Cfg_UFP_U_as_DFP_D      = 1,
    num_Cfg_UFP_U_as_UFP_D      = 2,
    num_Cfg_Reserved            = 3
} Num_DP_Sel_Configuration_Type;

#if defined(CONFIG_CCIC_NOTIFIER)
void ccic_event_work(void *data, int dest, int id, int attach, int event);
extern void select_pdo(int num);
#endif
#ifdef CONFIG_CHECK_CTYPE_SIDE
int usbpd_manager_get_side_check(void);
#endif
#if defined(CONFIG_IFCONN_NOTIFIER)
void ifconn_event_work(void *pd_data, int dest, int id, int event, void *data);
extern void select_pdo(int num);
#endif
#if defined(CONFIG_DUAL_ROLE_USB_INTF)
extern void role_swap_check(struct work_struct *wk);
extern int dual_role_is_writeable(struct dual_role_phy_instance *drp,
				  enum dual_role_property prop);
extern int dual_role_get_local_prop(struct dual_role_phy_instance *dual_role,
				    enum dual_role_property prop,
				    unsigned int *val);
extern int dual_role_set_prop(struct dual_role_phy_instance *dual_role,
			      enum dual_role_property prop,
			      const unsigned int *val);
extern int dual_role_init(void *_data);
#elif defined(CONFIG_TYPEC)
extern void typec_role_swap_check(struct work_struct *wk);
extern int typec_port_type_set(const struct typec_capability *cap,
                    enum typec_port_type port_type);
extern int typec_get_pd_support(void *_data);
extern int typec_init(void *_data);
#endif
#endif
