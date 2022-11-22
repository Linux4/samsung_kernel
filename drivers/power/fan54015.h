#ifndef __LINUX_SPRD2701_H
#define __LINUX_SPRD2701_H

#include <linux/ioctl.h>
#include "sprd_battery.h"

typedef unsigned char BYTE;

#define VENDOR_FAN54015					(0x4)
#define VENDOR_TQ24157					(0x2)
/******************************************************************************
* Register addresses
******************************************************************************/
#define FAN5405_REG_CONTROL0			(0x0)
#define FAN5405_REG_CONTROL1			(0x1)
#define FAN5405_REG_OREG				(0x2)
#define FAN5405_REG_IC_INFO				(0x3)
#define FAN5405_REG_IBAT					(0x4)
#define FAN5405_REG_SP_CHARGER			(0x5)
#define FAN5405_REG_SAFETY				(0x6)
#define FAN5405_REG_MONITOR				(0x10)

/******************************************************************************
* Register bits
******************************************************************************/
/* FAN5405_REG_CONTROL0 (0x00) */
#define FAN5405_FAULT				(0x07)
#define FAN5405_FAULT_SHIFT			(0)
#define FAN5405_BOOST				(0x01 << 3)
#define FAN5405_BOOST_SHIFT			(3)
#define FAN5405_STAT					(0x3 <<  4)
#define FAN5405_STAT_SHIFT			(4)
#define FAN5405_EN_STAT				(0x01 << 6)
#define FAN5405_EN_STAT_SHIFT		(6)
#define FAN5405_TMR_RST_OTG			(0x01 << 7)  // writing a 1 resets the t32s timer, writing a 0 has no effect
#define FAN5405_TMR_RST_OTG_SHIFT	(7)

/* FAN5405_REG_CONTROL1 (0x01) */
#define FAN5405_OPA_MODE			(0x01)
#define FAN5405_OPA_MODE_SHIFT		(0)
#define FAN5405_HZ_MODE				(0x01 << 1)
#define FAN5405_HZ_MODE_SHIFT		(1)
#define FAN5405_CE_N					(0x01 << 2)
#define FAN5405_CE_N_SHIFT			(2)
#define FAN5405_TE					(0x01 << 3)
#define FAN5405_TE_SHIFT				(3)
#define FAN5405_VLOWV				(0x03 << 4)
#define FAN5405_VLOWV_SHIFT			(4)
#define FAN5405_IINLIM				(0x03 << 6)
#define FAN5405_IINLIM_SHIFT			(6)

/* FAN5405_REG_OREG (0x02) */
#define FAN5405_OTG_EN				(0x01)
#define FAN5405_OTG_EN_SHIFT			(0)
#define FAN5405_OTG_PL				(0x01 << 1)
#define FAN5405_OTG_PL_SHIFT			(1)
#define FAN5405_OREG					(0x3f << 2)
#define FAN5405_OREG_SHIFT			(2)

/* FAN5405_REG_IC_INFO (0x03) */
#define FAN5405_REV					(0x03)
#define FAN5405_REV_SHIFT			(0)
#define FAN5405_PN					(0x07 << 2)
#define FAN5405_PN_SHIFT				(2)
#define FAN5405_VENDOR_CODE		(0x07 << 5)
#define FAN5405_VENDOR_CODE_SHIFT	(5)

/* FAN5405_REG_IBAT (0x04) */
#define FAN5405_ITERM				(0x07)
#define FAN5405_ITERM_SHIFT			(0)
#define FAN5405_IOCHARGE				(0x07 << 4)
#define FAN5405_IOCHARGE_SHIFT		(4)
#define FAN5405_RESET				(0x01 << 7)
#define FAN5405_RESET_SHIFT			(7)

/* FAN5405_REG_SP_CHARGER (0x05) */
#define FAN5405_VSP                     (0x07)
#define FAN5405_VSP_SHIFT                    0
#define FAN5405_EN_LEVEL           (0x01 << 3)
#define FAN5405_EN_LEVEL_SHIFT               3
#define FAN5405_SP                 (0x01 << 4)
#define FAN5405_SP_SHIFT                     4
#define FAN5405_IO_LEVEL           (0x01 << 5)
#define FAN5405_IO_LEVEL_SHIFT               5
#define FAN5405_DIS_VREG           (0x01 << 6)
#define FAN5405_DIS_VREG_SHIFT               6

/* FAN5405_REG_SAFETY (0x06) */
#define FAN5405_VSAFE                   (0x0f)
#define FAN5405_VSAFE_SHIFT                  0
#define FAN5405_ISAFE              (0x07 << 4)
#define FAN5405_ISAFE_SHIFT                  4

/* FAN5405_REG_MONITOR (0x10) */
#define FAN5405_CV                      (0x01)
#define FAN5405_CV_SHIFT                     0
#define FAN5405_VBUS_VALID         (0x01 << 1)
#define FAN5405_VBUS_VALID_SHIFT             1
#define FAN5405_IBUS               (0x01 << 2)
#define FAN5405_IBUS_SHIFT                   2
#define FAN5405_ICHG               (0x01 << 3)
#define FAN5405_ICHG_SHIFT                   3
#define FAN5405_T_120              (0x01 << 4)
#define FAN5405_T_120_SHIFT                  4
#define FAN5405_LINCHG             (0x01 << 5)
#define FAN5405_LINCHG_SHIFT                 5
#define FAN5405_VBAT_CMP           (0x01 << 6)
#define FAN5405_VBAT_CMP_SHIFT               6
#define FAN5405_ITERM_CMP          (0x01 << 7)
#define FAN5405_ITERM_CMP_SHIFT              7

/******************************************************************************
* bit definitions
******************************************************************************/
/********** FAN5405_REG_CONTROL0 (0x00) **********/
// EN_STAT [6]
#define ENSTAT 1
#define DISSTAT 0
// TMR_RST [7]
#define RESET32S 1

/********** FAN5405_REG_CONTROL1 (0x01) **********/
// OPA_MODE [0]
#define CHARGEMODE 0
#define BOOSTMODE 1
//HZ_MODE [1]
#define NOTHIGHIMP 0
#define HIGHIMP 1
// CE/ [2]
#define ENCHARGER 0
#define DISCHARGER 1
// TE [3]
#define DISTE 0
#define ENTE 1
// VLOWV [5:4]
#define VLOWV3P4 0
#define VLOWV3P5 1
#define VLOWV3P6 2
#define VLOWV3P7 3
// IINLIM [7:6]
#define IINLIM100 0
#define IINLIM500 1
#define IINLIM800 2
#define NOLIMIT 3

/********** FAN5405_REG_OREG (0x02) **********/
// OTG_EN [0]
#define DISOTG 0
#define ENOTG 1
// OTG_PL [1]
#define OTGACTIVELOW 0
#define OTGACTIVEHIGH 1
// OREG [7:2]
#define VOREG4P2 35  // refer to table 3

/********** FAN5405_REG_IC_INFO (0x03) **********/

/********** FAN5405_REG_IBAT (0x04) **********/
// ITERM [2:0] - 68mOhm
#define ITERM49 0
#define ITERM97 1
#define ITERM146 2
#define ITERM194 3
#define ITERM243 4
#define ITERM291 5
#define ITERM340 6
#define ITERM388 7
// IOCHARGE [6:4] - 68mOhm
#define IOCHARGE550 0
#define IOCHARGE650 1
#define IOCHARGE750 2
#define IOCHARGE850 3
#define IOCHARGE1050 4
#define IOCHARGE1150 5
#define IOCHARGE1350 6
#define IOCHARGE1450 7

/********** FAN5405_REG_SP_CHARGER (0x05) **********/
// VSP [2:0]
#define VSP4P213 0
#define VSP4P293 1
#define VSP4P373 2
#define VSP4P453 3
#define VSP4P533 4
#define VSP4P613 5
#define VSP4P693 6
#define VSP4P773 7
// IO_LEVEL [5]
#define ENIOLEVEL 0
#define DISIOLEVEL 1
// DIS_VREG [6]
#define VREGON 0
#define VREGOFF 1

/********** FAN5405_REG_SAFETY (0x06) **********/
// VSAFE [3:0]
#define VSAFE4P20 0
#define VSAFE4P22 1
#define VSAFE4P24 2
#define VSAFE4P26 3
#define VSAFE4P28 4
#define VSAFE4P30 5
#define VSAFE4P32 6
#define VSAFE4P34 7
#define VSAFE4P36 8
#define VSAFE4P38 9
#define VSAFE4P40 10
#define VSAFE4P42 11
#define VSAFE4P44 12
// ISAFE [6:4] - 68mOhm
#define ISAFE550 0
#define ISAFE650 1
#define ISAFE750 2
#define ISAFE850 3
#define ISAFE1050 4
#define ISAFE1150 5
#define ISAFE1350 6
#define ISAFE1450 7

typedef enum{
	FAN5405_MONITOR_NONE,
	FAN5405_MONITOR_CV,
	FAN5405_MONITOR_VBUS_VALID,
	FAN5405_MONITOR_IBUS,
	FAN5405_MONITOR_ICHG,
	FAN5405_MONITOR_T_120,
	FAN5405_MONITOR_LINCHG,
	FAN5405_MONITOR_VBAT_CMP,
	FAN5405_MONITOR_ITERM_CMP
}fan5405_monitor_status;

enum charge_fault{
	CHG_NO_FAULT = 0,
	CHG_VBUS_OVP,
	CHG_SLEEP_MODE,
	CHG_POOR_INPUT,
	CHG_VBAT_OVP,
	CHG_THM_SHUTDOWN,
	CHG_TIMER_FAULT,
	CHG_NO_BAT,
};
enum charge_status{
	CHG_READY= 0,
	CHG_CHGING,
	CHG_DONE,
	CHG_FAULT,
};

struct fan54015{
	struct i2c_client *client;
	struct delayed_work feed_watchdog_work;
	struct delayed_work vbus_detect_work;
};
struct fan54015_platform_data {
	uint16_t version;
	const struct sprd_ext_ic_operations * fan54015_ops;
	//int irq_gpio_number;
};

extern void fan54015_reset_timer(void);
extern void fan54015_sw_reset(void);
extern void fan54015_set_vindpm(BYTE reg_val);
extern void fan54015_termina_cur_set(BYTE reg_val);
extern void fan54015_termina_vol_set(BYTE reg_val);
extern void fan54015_set_safety_vol(BYTE reg_val);
extern void fan54015_set_safety_cur(BYTE reg_val);
extern void fan54015_init(void);
extern void fan54015_otg_enable(int enable);
extern void fan54015_stop_charging(void);
extern BYTE fan54015_get_vendor_id(void);
extern BYTE fan54015_get_chg_status(void);
extern BYTE fan54015_get_fault_val(void);
extern void fan54015_enable_chg(void);
extern void fan54015_set_chg_current(BYTE reg_val);
extern BYTE fan54015_get_chg_current(void);
extern int fan54015_register_notifier(struct notifier_block *nb);
extern int fan54015_unregister_notifier(struct notifier_block *nb);
#endif
