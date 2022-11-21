#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
#include <linux/usb/typec/common/pdic_notifier.h>
#endif
#if IS_ENABLED(CONFIG_DUAL_ROLE_USB_INTF)
#include <linux/usb/class-dual-role.h>
#elif IS_ENABLED(CONFIG_TYPEC)
#include <linux/usb/typec.h>
#endif
#if IS_ENABLED(CONFIG_IFCONN_NOTIFIER)
#include <linux/power/s2m_chg_manager.h>
#endif
#ifdef CONFIG_BATTERY_SAMSUNG
#ifdef CONFIG_USB_TYPEC_MANAGER_NOTIFIER
#include <linux/battery/battery_notifier.h>
#endif
#endif

#ifndef __USBPD_EXT_H__
#define __USBPD_EXT_H__

#if IS_ENABLED(CONFIG_IFCONN_NOTIFIER)
extern struct pdic_notifier_data pd_noti;
#endif
#if defined(CONFIG_BATTERY_SAMSUNG) && defined(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
extern struct pdic_notifier_struct pd_noti;
#endif

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
#define MPA2_PRODUCT_ID			0x2122
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

#if IS_ENABLED(CONFIG_CHECK_CTYPE_SIDE)
int usbpd_manager_get_side_check(void);
#endif

#if defined(CONFIG_PDIC_NOTIFIER)
void pdic_event_work(void *data, int dest, int id, int attach, int event);
extern void select_pdo(int num);
extern int sec_pd_select_pps(int num, int ppsVol, int ppsCur);
extern int sec_pd_get_apdo_max_power(unsigned int *pdo_pos,
	unsigned int *taMaxVol, unsigned int * taMaxCur, unsigned int *taMaxPwr);
extern int sec_pps_enable(int num, int ppsVol, int ppsCur, int enable);
extern int sec_get_pps_voltage(void);
extern void pdo_ctrl_by_flash(bool mode);
#endif

#if IS_ENABLED(CONFIG_IFCONN_NOTIFIER)
void ifconn_event_work(void *pd_data, int dest, int id, int event, void *data);
#endif

#if IS_ENABLED(CONFIG_DUAL_ROLE_USB_INTF)
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
#elif IS_ENABLED(CONFIG_TYPEC)
extern void typec_role_swap_check(struct work_struct *wk);
extern int typec_port_type_set(struct typec_port *port, enum typec_port_type port_type);
extern int typec_get_pd_support(void *_data);
extern int typec_init(void *_data);

#endif
#endif
