#ifndef _LINUX_POWER_ISL9226_H
#define _LINUX_POWER_ISL9226_H

#define OPMOD		(0x00)
#define CCTRL1		(0x01)
#define CCTRL2		(0x02)
#define VCTRL1		(0x03)
#define VCTRL2		(0x04)
#define STAT1		(0x05)
#define STAT2		(0x06)

#define CHGEN		(1 << 0)
#define AUTOSTP		(1 << 1)
#define AUTORES		(1 << 2)
#define EOCSPPM		(1 << 3)
#define JEITAEN		(1 << 4)
#define ENIDPM		(1 << 5)
#define SUSTIME	(1 << 6)

#define CHGING		(1 << 0)
#define BATFUL		(1 << 1)
#define CONST_VOL	(1 << 2)
#define EOC1		(1 << 3)
#define PGOOD		(1 << 4)

#define VMAX4_2V	(0x13)

struct isl9226_charger_pdata {
	unsigned int default_input_current;
	unsigned int usb_input_current;
	unsigned int ac_input_current;
	unsigned int eoc_current;
	unsigned int prechg_current;
	unsigned int prechg_voltage;
};
#endif
