#ifndef _AFC_CHARGER_INTF
#define _AFC_CHARGER_INTF

#include <linux/kernel.h>
#include <linux/kthread.h>

#if IS_ENABLED(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/common/muic_notifier.h>
extern struct muic_platform_data muic_pdata;
#endif

/* AFC Unit Interval */
#define UI				160
#define MPING			(UI << 4)
#define DATA_DELAY		(UI << 3)
#define SYNC_PULSE		(UI >> 2)
#define RESET_DELAY		(UI * 100)

#define AFC_RETRY_CNT			10
#define AFC_OP_CNT			3
#define AFC_RETRY_MAX			10
#define VBUS_RETRY_MAX			10
#define AFC_SPING_CNT			7
#define AFC_SPING_MIN			2
#define AFC_SPING_MAX			18

struct gpio_afc_pdata {
	int gpio_afc_switch;
	int gpio_afc_data;
	int gpio_afc_hvdcp;
};

struct gpio_afc_ddata {
	struct device *dev;
	struct gpio_afc_pdata *pdata;
	struct wakeup_source ws;
	struct mutex mutex;
	struct kthread_worker kworker;
	struct kthread_work kwork;
	spinlock_t spin_lock;
	bool gpio_input;
	bool check_performance;
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	int afc_disable;
	struct delayed_work set_gpio_afc_disable;
#endif
	int curr_voltage;
	int set_voltage;

#if IS_ENABLED(CONFIG_CHARGER_SYV660)
	bool dpdm_ctrl_on;
#endif
#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	unsigned int rp_currentlvl;
	struct notifier_block typec_nb;
	struct mutex tcm_notify_lock;
#endif
#if IS_ENABLED(CONFIG_MUIC_NOTIFIER)
	struct muic_platform_data *muic_pdata;
#endif
};

#if IS_ENABLED(CONFIG_AFC_CHARGER)
extern int set_afc_voltage(int voltage);
#if IS_ENABLED(CONFIG_CHARGER_SYV660)
extern void afc_dpdm_ctrl(bool onoff);
#endif
extern void set_afc_voltage_for_performance(bool enable);
#else /* NOT CONFIG_AFC_CHARGER */
static inline int set_afc_voltage(int voltage)
{
	return -ENOTSUPP;
}
#if IS_ENABLED(CONFIG_CHARGER_SYV660)
static inline void afc_dpdm_ctrl(bool onoff)
{
	return -ENOTSUPP;
}
#endif
static inline void set_afc_voltage_for_performance(bool enable)
{
	return -ENOTSUPP;
}
#endif /* CONFIG_AFC_CHARGER */
#if IS_ENABLED(CONFIG_SEC_HICCUP)
void set_sec_hiccup(bool en);
#endif /* CONFIG_SEC_HICCUP */
#endif /* _AFC_CHSRGER_INTF */
