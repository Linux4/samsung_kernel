/*
* File:   fusb30x_global.h
* Company: Fairchild Semiconductor
*
* Created on September 11, 2015, 15:28 AM
*/

#ifndef FUSB30X_TYPES_H
#define FUSB30X_TYPES_H

#include <linux/i2c.h>								// i2c_client, spinlock_t
#include <linux/hrtimer.h>							// hrtimer
#include <linux/semaphore.h>
#include <linux/workqueue.h>
#include <linux/pm_wakeup.h>
#include <linux/extcon.h>
#include <linux/usb/role.h>
#include <mt-plat/v1/charger_class.h>
#include <linux/pinctrl/consumer.h>
#include "FSCTypes.h"								// FUSB30x custom types
/* hs14 code for SR-AL6528A-01-300 by wenyaqi at 2022/09/11 start */
#include <linux/power_supply.h>
/* hs14 code for SR-AL6528A-01-300 by wenyaqi at 2022/09/11 end */

#include "../core/Port.h"									// Port object
#include "../core/modules/dpm.h"
/* hs14 code for SR-AL6528A-01-322 by wenyaqi at 2022/09/17 start */
#include "../../../../../power/supply/mediatek/charger/adapter_class.h"

#include <linux/usb/typec.h>

#ifdef FSC_DEBUG
#include <linux/debugfs.h>

#define FSC_HOSTCOMM_BUFFER_SIZE	64				// Length of the hostcomm buffer
#endif // FSC_DEBUG

struct fusb30x_chip									// Contains data required by this driver
{
	struct mutex lock;								// Synchronization lock
	/* hs14 code for AL6528ADEU-1931 by wenyaqi at 2022/12/01 start */
	struct mutex event_lock;
	/* hs14 code for AL6528ADEU-1931 by wenyaqi at 2022/12/01 end */
	struct wakeup_source fusb302_wakelock;			// wake lock
	struct semaphore suspend_lock;

#ifdef FSC_DEBUG
	FSC_U8 dbgTimerTicks;							// Count of timer ticks
	FSC_U8 dbgTimerRollovers;						// Timer tick counter rollover counter
	FSC_U8 dbgSMTicks;								// Count of state machine ticks
	FSC_U8 dbgSMRollovers;							// State machine tick counter rollover counter
	FSC_S32 dbg_gpio_StateMachine;					// Gpio that toggles every time the state machine is triggered
	FSC_BOOL dbg_gpio_StateMachine_value;			// Value of sm toggle state machine
	char HostCommBuf[FSC_HOSTCOMM_BUFFER_SIZE];		// Buffer used to communicate with HostComm

	struct dentry *debugfs_parent;					// Parent for DebugFS entry
#endif // FSC_DEBUG

	/* Internal config data */
	FSC_S32 InitDelayMS;							// Number of milliseconds to wait before initializing the fusb30x
	FSC_S32 numRetriesI2C;							// Number of times to retry I2C reads/writes

	/* I2C */
	struct i2c_client* client;						// I2C client provided by kernel
	FSC_BOOL use_i2c_blocks;						// True if I2C_FUNC_SMBUS_I2C_BLOCK is supported

	/* GPIO */
	FSC_S32 gpio_VBus5V;							// VBus 5V GPIO pin
	FSC_BOOL gpio_VBus5V_value;						// true if active, false otherwise
	FSC_S32 gpio_VBusOther;							// VBus other GPIO pin (eg. VBus 12V) (NOTE: Optional feature - if set to <0 during GPIO init, then feature is disabled)
	FSC_BOOL gpio_VBusOther_value;					// true if active, false otherwise
	FSC_S32 gpio_IntN;								// INT_N GPIO pin

	FSC_S32 gpio_IntN_irq;							// IRQ assigned to INT_N GPIO pin
	FSC_S32 gpio_Discharge;							// Discharge GPIO pin
	FSC_BOOL gpio_Discharge_value;					// true if active, false otherwise

	/* Threads */
	struct work_struct sm_worker;					// Main state machine actions
	struct workqueue_struct *highpri_wq;
	FSC_BOOL queued;

	/* Timers */
	struct hrtimer sm_timer;						// High-resolution timer for the state machine

	/* Port Object */
	struct Port port;
	DevicePolicyPtr_t dpm;
	struct extcon_dev	*extcon;
	struct regulator	*vbus;
	struct regulator	*vconn;
	FSC_BOOL	is_vbus_present;
	struct power_supply			*usb_psy;
	struct power_supply			*pogo_psy;
	struct dual_role_phy_instance *dual_role;
	struct dual_role_phy_desc   *dr_desc;

	// USB Role Switch
	struct usb_role_switch *role_sw;
	struct device_connection dev_conn;

	struct charger_device *primary_charger;
	struct adapter_device *pd_adapter;

	//kernel 5.4
	struct typec_capability	typec_caps;
	struct typec_port	*typec_port;
	struct typec_partner	*partner;
	struct typec_partner_desc partner_desc;
	struct usb_pd_identity	partner_identity;
	//

	struct delayed_work apsd_recheck_work;
	struct delayed_work delay_init_work;
	/* hs14 code for AL6528ADEU-2820 by lina at 2022/11/23 start */
	struct delayed_work delay_attach_check_work;
	/* hs14 code for AL6528ADEU-2820 by lina at 2022/11/23 end */
	int usb_state;

	FSC_BOOL during_port_type_swap;
	PortConfig_t bak_port_config;
	FSC_BOOL during_drpr_swap;

#if 0 //#ifdef CONFIG_PINCTRL
	struct pinctrl	*pinctrl_int;
	struct pinctrl_state	*pinctrl_state_int;
#endif

	/* hs14 code for SR-AL6528A-01-300 by wenyaqi at 2022/09/11 start */
	struct device *dev;
	struct power_supply *typec_psy;
	struct power_supply_desc psd;
	/* hs14 code for SR-AL6528A-01-300 by wenyaqi at 2022/09/11 end */
};

extern struct fusb30x_chip* g_chip;

extern void notify_adapter_event(enum adapter_type type, enum adapter_event evt,
	void *val);
/* hs14 code for SR-AL6528A-01-322 by wenyaqi at 2022/09/17 end */

struct fusb30x_chip* fusb30x_GetChip(void);			// Getter for the global chip structure
void fusb30x_SetChip(struct fusb30x_chip* newChip); // Setter for the global chip structure

#endif /* FUSB30X_TYPES_H */
