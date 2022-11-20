#if defined(CONFIG_PDIC_NOTIFIER)
#include <linux/usb/typec/common/pdic_notifier.h>
#endif
#if defined(CONFIG_DUAL_ROLE_USB_INTF)
#include <linux/usb/class-dual-role.h>
#elif defined(CONFIG_TYPEC)
#include <linux/usb/typec.h>
#endif

#ifdef CONFIG_BATTERY_SAMSUNG
#ifdef CONFIG_USB_TYPEC_MANAGER_NOTIFIER
#include <linux/battery/battery_notifier.h>
#endif
#endif

#ifndef __USBPD_EXT_H__
#define __USBPD_EXT_H__

#if defined(CONFIG_BATTERY_SAMSUNG) && defined(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
extern struct pdic_notifier_struct pd_noti;
#endif

#if defined(CONFIG_PDIC_NOTIFIER)
void pdic_event_work(void *data, int dest, int id, int attach, int event);
extern void select_pdo(int num);
extern int sec_pd_select_pps(int num, int ppsVol, int ppsCur);
extern int sec_pd_get_acco_max_power(unsigned int *pdo_pos,
	unsigned int *taMaxVol, unsigned int * taMaxCur, unsigned int *taMaxPwr);
extern int sec_pps_enable(int num, int ppsVol, int ppsCur, int enable);
extern int sec_get_pps_voltage(void);
extern void pdo_ctrl_by_flash(bool mode);
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
