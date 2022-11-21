#ifndef __LINUX_SM5701_H
#define __LINUX_SM5701_H

#include <linux/ioctl.h>

#define CHARGE_STAT_READY	0
#define CHARGE_STAT_INPROGRESS  1
#define CHARGE_STAT_DONE	2
#define CHARGE_STAT_FAULT       3


typedef unsigned char BYTE;


/* SM5701 Registers. */
#define SM5701_INT1		0x00
#define SM5701_INT2		0x01
#define SM5701_INT3		0x02
#define SM5701_INTMASK1		0x03
#define SM5701_INTMASK2		0x04
#define SM5701_INTMASK3		0x05
#define SM5701_STATUS1		0x06
#define SM5701_STATUS2		0x07
#define SM5701_STATUS3		0x08
#define SM5701_CNTL		0x09
#define SM5701_VBUSCNTL		0x0A
#define SM5701_CHGCNTL1		0x0B
#define SM5701_CHGCNTL2		0x0C
#define SM5701_CHGCNTL3		0x0D
#define SM5701_CHGCNTL4		0x0E

#define SM5701_FLEDCNTL1	0x0F
#define SM5701_FLEDCNTL2	0x10
#define SM5701_FLEDCNTL3	0x11
#define SM5701_FLEDCNTL4	0x12
#define SM5701_FLEDCNTL5	0x13
#define SM5701_FLEDCNTL6	0x14
#define SM5701_DEVICE_ID        0x15

#define BATREG_MASK			0x00

//INT1
#define SM5701_INT1_AICL		0x01
#define SM5701_INT1_BATOVP		0x02
#define SM5701_INT1_VBUSOK		0x04
#define SM5701_INT1_VBUSUVLO		0x08
#define SM5701_INT1_VBUSOVP		0x10
#define SM5701_INT1_VBUSLIMIT		0x20
#define SM5701_INT1_CHGRSTF		0x40
#define SM5701_INT1_NOBAT		0x80
//INT2
#define SM5701_INT2_TOPOFF		0x01
#define SM5701_INT2_DONE		0x02
#define SM5701_INT2_FASTTMROFF		0x04
#define SM5701_INT2_CHGON		0x08
//INT3
#define SM5701_INT3_THEMSHDN		0x01
#define SM5701_INT3_THEMREG		0x02
#define SM5701_INT3_FLEDSHORT		0x04
#define SM5701_INT3_FLEDOPEN		0x08
#define SM5701_INT3_BOOSTPOK_NG		0x10
#define SM5701_INT3_BOOSTPOK		0x20
#define SM5701_INT3_AMSTMROFF		0x40
#define SM5701_INT3_LOWBATT		0x80
//INTMSK1
#define SM5701_INTMSK1_AICLM		0x01
#define SM5701_INTMSK1_BATOVPM		0x02
#define SM5701_INTMSK1_VBUSOKM		0x04
#define SM5701_INTMSK1_VBUSUVLOM	0x08
#define SM5701_INTMSK1_VBUSOVPM		0x10
#define SM5701_INTMSK1_VBUSLIMITM	0x20
#define SM5701_INTMSK1_CHGRSTFM		0x40
#define SM5701_INTMSK1_NOBATM		0x80
//INTMSK2
#define SM5701_INTMSK2_TOPOFFM		0x01
#define SM5701_INTMSK2_DONEM		0x02
#define SM5701_INTMSK2_FASTTMROFFM	0x04
#define SM5701_INTMSK2_CHGONM		0x08
//INTMSK3
#define SM5701_INTMSK3_THEMSHDNM	0x01
#define SM5701_INTMSK3_THEMREGM		0x02
#define SM5701_INTMSK3_FLEDSHORTM	0x04
#define SM5701_INTMSK3_FLEDOPENM	0x08
#define SM5701_INTMSK3_BOOSTPOK_NGM	0x10
#define SM5701_INTMSK3_BOOSTPOKM	0x20
#define SM5701_INTMSK3_AMSTMROFFM	0x40
#define SM5701_INTMSK3_LOWBATTM		0x80
//STATUS1
#define SM5701_STATUS1_AICL			0x01
#define SM5701_STATUS1_BATOVP			0x02
#define SM5701_STATUS1_VBUSOK			0x04
#define SM5701_STATUS1_VBUSUVLO			0x08
#define SM5701_STATUS1_VBUSOVP			0x10
#define SM5701_STATUS1_VBUSLIMIT		0x20
#define SM5701_STATUS1_CHGRSTF			0x40
#define SM5701_STATUS1_NOBAT			0x80
//STATUS2
#define SM5701_STATUS2_TOPOFF			0x01
#define SM5701_STATUS2_DONE			0x02
#define SM5701_STATUS2_FASTTMROFF		0x04
#define SM5701_STATUS2_CHGON			0x08
//STATUS3
#define SM5701_STATUS3_THEMSHDN	        	0x01
#define SM5701_STATUS3_THEMREG		    	0x02
#define SM5701_STATUS3_FLEDSHORT		0x04
#define SM5701_STATUS3_FLEDOPEN			0x08
#define SM5701_STATUS3_BOOSTPOK_NG	    	0x10
#define SM5701_STATUS3_BOOSTPOK		    	0x20
#define SM5701_STATUS3_ABSTMROFF		0x40
#define SM5701_STATUS3_LOWBATT			0x80
//CNTL
#define SM5701_CNTL_OPERATIONMODE		0x07
#define SM5701_CNTL_RESET			0x08
#define SM5701_CNTL_OVPSEL			0x30
#define SM5701_CNTL_FREQSEL			0xc0
#define SM5701_RESET                           	0x1
//VBUSCNTL
#define SM5701_VBUSCNTL_VBUSLIMIT		0x07
#define SM5701_VBUSCNTL_AICLTH			0x38
#define SM5701_VBUSCNTL_AICLEN			0x40

//CHGCNTL1
#define SM5701_CHGCNTL1_TOPOFF          	0x0F
#define SM5701_CHGCNTL1_AUTOSTOP        	0x10
#define SM5701_CHGCNTL1_HSSLPCTRL       	0x60

//CHGCNTL2
#define SM5701_CHGCNTL2_FASTCHG			0x3F

//CHGCNTL3
#define SM5701_CHGCNTL3_BATREG			0x1F

//CHGCNTL4
#define SM5701_CHGCNTL4_TOPOFFTIMER		0x03
#define SM5701_CHGCNTL4_FASTTIMER		0x0B


//FLEDCNTL1
#define SM5701_FLEDCNTL1_FLEDEN			0x03
#define SM5701_FLEDCNTL1_ABSTMR			0x0B
#define SM5701_FLEDCNTL1_ENABSTMR		0x10
//FLEDCNTL2
#define SM5701_FLEDCNTL2_ONETIMER		0x0F
#define SM5701_FLEDCNTL2_nONESHOT		0x10
#define SM5701_FLEDCNTL2_SAFET			0x60
#define SM5701_FLEDCNTL2_nENSAFET		0x80
//FLEDCNTL3
#define SM5701_FLEDCNTL3_IFLED			0x1F
//FLEDCNTL4
#define SM5701_FLEDCNTL4_IMLED			0x1F
//FLEDCNTL5
#define SM5701_FLEDCNTL5_LBDHYS			0x03
#define SM5701_FLEDCNTL5_LOWBATT		0x1B
#define SM5701_FLEDCNTL5_LBRSTIMER		0x60
#define SM5701_FLEDCNTL5_ENLOWBATT		0x80
//FLEDCNTL6
#define SM5701_FLEDCNTL6_BSTOUT			0x0F
#define SM5701_BSTOUT_4P5               0x05
#define SM5701_BSTOUT_5P0               0x0A

#define SM5701_STATUS1_NOBAT_SHIFT 7

/* Enable charger Operation Mode */
#define OP_MODE_CHG_ON 0x02
#define OP_MODE_CHG_ON_REV3 0x04

/* Disable charger Operation Mode */
#define OP_MODE_CHG_OFF 0x01


//CNTL
#define SM5701_OPERATIONMODE_SUSPEND                0x0 // 000 : Suspend (charger-OFF) MODE
#define SM5701_OPERATIONMODE_FLASH_ON               0x1 // 001 : Flash LED Driver=ON Ready in Charger & OTG OFF Mode
#define SM5701_OPERATIONMODE_OTG_ON                 0x2 // 010 : OTG=ON in Charger & Flash OFF Mode
#define SM5701_OPERATIONMODE_OTG_ON_FLASH_ON        0x3 // 011 : OTG=ON & Flash LED Driver=ON Ready in charger OFF Mode
#define SM5701_OPERATIONMODE_CHARGER_ON             0x4 // 100 : Charger=ON in OTG & Flash OFF Mode. Same as 0x6(110)
#define SM5701_OPERATIONMODE_CHARGER_ON_FLASH_ON    0x5 // 101 : Charger=ON & Flash LED Driver=ON Ready in OTG OFF Mode. Same as 0x7(111)


#define FREQ_15				(0x00 << 6)
#define FREQ_12				(0x01 << 6)
#define FREQ_18                         (0x02 << 6)
#define FREQ_24                         (0x03 << 6)

enum {
	CHARGER_READY = 0,
	CHARGER_DOING,
	CHARGER_DONE,
	CHARGER_FAULT,
};

enum {
	POWER_SUPPLY_VBUS_UNKNOWN = 0,
	POWER_SUPPLY_VBUS_UVLO,
	POWER_SUPPLY_VBUS_WEAK,
	POWER_SUPPLY_VBUS_OVLO,
	POWER_SUPPLY_VBUS_GOOD,
};


typedef enum{
	SM5701_MONITOR_NONE,
	SM5701_MONITOR_CV,
	SM5701_MONITOR_VBUS_VALID,
	SM5701_MONITOR_IBUS,
	SM5701_MONITOR_ICHG,
	SM5701_MONITOR_T_120,
	SM5701_MONITOR_LINCHG,
	SM5701_MONITOR_VBAT_CMP,
	SM5701_MONITOR_ITERM_CMP
}sm5701_monitor_status;


struct sprd_ext_ic_operations {
	void (*ic_init)(void);
	void (*charge_start_ext)(int);
	void (*charge_stop_ext)(void);
	int  (*get_charging_status)(void);
	void (*timer_callback_ext)(void);
	void (*otg_charge_ext)(int enable);
};

extern void sm5701_TA_StartCharging(void);
extern void sm5701_USB_StartCharging(void);
extern void sm5701_StopCharging(void);
extern sm5701_monitor_status sm5701_Monitor(void);
extern int sm5701_GetCharge_Stat(void);

extern int SM5701_reg_read(struct i2c_client *i2c, u8 reg, u8 *dest);
extern int SM5701_reg_write(struct i2c_client *i2c, u8 reg, u8 value);

const struct sprd_ext_ic_operations *sprd_get_ext_ic_ops(void);

#endif
