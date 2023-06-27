/*
 * stbcfg01-private.h - Voltage regulator driver for the STBCFG01 driver
 *
 *  Copyright (C) 2009-2010 Samsung Electrnoics
 *
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __LINUX_MFD_STBCFG01_PRIV_H
#define __LINUX_MFD_STBCFG01_PRIV_H

#include "stbcfg01.h"

// CURRENT REVISION
#define STBCFG01_DRIVER_VERSION "LoganTD_1.1"


// define for enabling /proc/driver/STBCFG01_* files (comment the line to disable)
#define STBCFG01_DEBUG_PROC_FILE

// general define for enabling debug messages (comment the line to disable messages)
#define STBCFG01_DEBUG_MESSAGES

// additional defines to select specific debug messages if needed
#ifdef STBCFG01_DEBUG_MESSAGES
#define STBCFG01_DEBUG_MESSAGES_BASIC
#define STBCFG01_DEBUG_MESSAGES_EXTRA_STATUS
//#define STBCFG01_DEBUG_MESSAGES_EXTRA_I2C
//#define STBCFG01_DEBUG_MESSAGES_EXTRA_REGISTERS
#endif

/*******************************************************************************
 * General constants and algorithm parameters
*******************************************************************************/

#define TEMPCOMP_SOC              /* temperature compensation of SOC */
#define TEMPCOMP_VM               /* temperature compensation of VM_cnf */
#define DELTA_TEMP         30     /* delta temp to recalculate VM_cnf compensation, in 0.1°C */

#define MAX_HRSOC          51200  /* 100% in 1/512% units */
#define MAX_SOC            1000   /* 100% in 0.1% units */

#define AVGFILTER          4      /* average filter constant */

#define NTEMP              7      /* number of items in temperature compensation arrays */
#define RAM_SIZE           16     /* total RAM size of STBCFG01 in bytes */
#define OCVTAB_SIZE        16     /* OCVTAB size of STBCFG01 in bytes */

#define VoltageFactor      11015  /* LSB to convert to mV */

#define STBCFG01_DELAY	   1000   /* scheduler interval */

/*******************************************************************************
 * Register map, masks and values
*******************************************************************************/

/*Address of the STBCFG01 register --------------------------------------------*/
#define STBCFG01_REG_MODE                 0x00    /* Mode Register             */
#define STBCFG01_REG_CTRL                 0x01    /* Control and Status Register */
#define STBCFG01_REG_SOC                  0x02    /* SOC Data (2 bytes) */
#define STBCFG01_REG_COUNTER              0x04    /* Number of Conversion (2 bytes) */
#define STBCFG01_REG_VOLTAGE              0x08    /* Battery Voltage (2 bytes) */
#define STBCFG01_REG_TEMPERATURE          0x0A    /* Temperature               */
#define STBCFG01_REG_OCV                  0x0D    /* Battery OCV (2 bytes) */
#define STBCFG01_REG_VM_CNF               0x11    /* VM configuration (2 bytes)    */
#define STBCFG01_REG_ALARM_SOC            0x13    /* SOC alarm level         */
#define STBCFG01_REG_ALARM_VOLTAGE        0x14    /* Low voltage alarm level */

#define STBCFG01_REG_CHG_CFG1             0x90    /* Charger configuration register */
#define STBCFG01_REG_CHG_CFG2             0x91    /* Charger configuration register */
#define STBCFG01_REG_CHG_CFG3             0x92    /* Charger configuration register */
#define STBCFG01_REG_CHG_CFG4             0x93    /* Charger configuration register */
#define STBCFG01_REG_CHG_CFG5             0x94    /* Charger configuration register */
#define STBCFG01_REG_CHG_STATUS1          0x95    /* Charger status register */
#define STBCFG01_REG_CHG_STATUS2          0x96    /* Charger status register */
#define STBCFG01_REG_CHG_INT_EN1          0x97    /* Charger interrupt enable register */
#define STBCFG01_REG_CHG_INT_EN2          0x98    /* Charger interrupt enable register */
#define STBCFG01_REG_CHG_INT_LATCH1       0x99    /* Charger interrupt status/latch register */
#define STBCFG01_REG_CHG_INT_LATCH2       0x9A    /* Charger interrupt status/latch register */

/*Bit mask definition*/
#define STBCFG01_ALM_ENA		  0x08	 /* Alarm enable bit mask     */
#define STBCFG01_OTG_ENA		  0x40	 /* OTG boost enable bit mask     */
#define STBCFG01_GG_RUN			  0x10	 /* GG run bit mask     */
#define STBCFG01_SOFTPOR			0x11	 /* soft reset     */

#define STBCFG01_REG_ID                   0x18    /* Chip ID (1 byte)       */
#define STBCFG01_ID                       0x14    /* STBCFG01 ID */

#define STBCFG01_REG_RAM                  0x20    /* General Purpose RAM Registers */

#define STBCFG01_REG_OCVTAB               0x30

#define M_STAT 0x1010       /* GG_RUN & PORDET mask in STBCFG01_BattDataTypeDef status word */
#define M_RST  0x1800       /* BATFAIL & PORDET mask */
#define M_RUN  0x0010       /* GG_RUN mask in STBCFG01_BattDataTypeDef status word */
#define M_BATFAIL 0x0800    /* BATFAIL mask*/

#define OK 0

/* STBCFG01 RAM test word */
#define RAM_TSTWORD 0x53A9

/* Gas gauge states */
#define GG_INIT     'I'
#define GG_RUNNING  'R'
#define GG_POWERDN  'D'

#define VM_MODE 1
#define CC_MODE 0

#define STBCFG01_VF_BATTERY_ON 200
#define STBCFG01_VF_BATTERY_OFF 0

#define STBCFG01_DEBUG_MESSAGES_BASIC
#define STBCFG01_DEBUG_MESSAGES_EXTRA_REGISTERS
#define STBCFG01_DEBUG_MESSAGES_EXTRA_I2C

static const int TempTable[NTEMP] = {60, 40, 25, 10, 0, -10, -20};   /* temperature table from 60°C to -20°C (descending order!) */

extern int STBCFG01_Write(struct i2c_client *client, int length, int reg, unsigned char *values);
extern int STBCFG01_Read(struct i2c_client *client, int length, int reg, unsigned char *values);
extern int STBCFG01_ReadByte(struct i2c_client *client, int RegAddress);
extern int STBCFG01_WriteByte(struct i2c_client *client, int RegAddress, unsigned char Value);
extern int STBCFG01_ReadWord(struct i2c_client *client, int RegAddress);
extern int STBCFG01_WriteWord(struct i2c_client *client, int RegAddress, int Value);

extern int STBCFG01_get_status(void);

struct battery_data_t {
  int Alm_en;		/* SOC and VBAT alarms enable, 0=disabled, 1=enabled */
  int Alm_SOC;          /* SOC alarm level % */
  int Alm_Vbat;         /* Vbat alarm level mV */
  int VM_cnf;           /* nominal VM cnf for discharging */
  int VM_cnf_chg;       /* nominal VM cnf for charging */
  int Cnom;             /* nominal capacity in mAh */
  int CapDerating[7];   /* capacity derating in 0.1%, for temp = 60, 40, 25, 10, 0, -10, -20°C */
  int VM_cnf_comp[7];   /* VM_cnf temperature compensation multiplicator in percents, for temp = 60, 40, 25, 10, 0, -10, -20°C */
  int OCVOffset[16];    /* OCV curve adjustment */

  int SysCutoffVolt;	/* system cut-off voltage, system does not work under this voltage (platform UVLO) in mV */
  int EarlyEmptyCmp;	/* difference vs SysCutoffVolt to start nearly empty compensation in mV, typically 200 */

  int IFast;            /* fast charge current, range 550-1250mA, value = IFast*100+550 mA */
  int IFast_usb;        /* fast charge current (another conf. for USB), range 550-1250mA, value = IFast*100+550 mA */
  int IPre;             /* pre-charge current, 0=450 mA, 1=100 mA */
  int ITerm;            /* termination current, range 50-300 mA, value = ITerm*25+50 mA */
  int VFloat;           /* floating voltage, range 3.52-4.78 V, value = VFloat*0.02+3.52 V */
  int ARChg;            /* automatic recharge, 0=disabled, 1=enabled */
  int Iin_lim;          /* input current limit, 0=100mA 1=500mA 2=800mA 3=1.2A 4=no limit */
  int DICL_en;          /* dynamic input current limit, 0=disabled, 1=enabled */
  int VDICL;            /* dynamic input curr lim thres, range 4.00-4.75V, value = VDICL*0.25+4.00 V */
  int DIChg_adj;        /* dynamic charging current adjust enable, 0=disabled, 1=enabled */
  int TPre;             /* pre-charge timer, 0=disabled, 1=enabled */
  int TFast;            /* fast charge timer, 0=disabled, 1=enabled */
  int PreB_en;          /* pre-bias function, 0=disabled, 1=enabled */
  int LDO_UVLO_th;      /* LDO UVLO threshold, 0=3.5V 1=4.65V */
  int LDO_en;           /* LDO output, 0=disabled, 1=enabled */
  int IBat_lim;         /* OTG average battery current limit, 0=350mA 1=450mA 2=550mA 3=950mA */
  int WD;               /* watchdog timer function, 0=disabled, 1=enabled */
  int FSW;              /* switching frequency selection, 0=2MHz, 1=3MHz */

  int GPIO_cd;          /* charger disable GPIO */
  int GPIO_shdn;        /* all charger circuitry shutdown GPIO or 0=not used */

  int (*ExternalTemperature)(void);                                               /* external temperature function, return 0.1 °C */
  int (*power_supply_register)(struct device *parent, struct power_supply *psy);  /* custom power supply structure registration function */
  void (*power_supply_unregister)(struct power_supply *psy);                      /* custom power supply structure unregistration function */
};

#ifdef STBCFG01_DEBUG_PROC_FILE
static struct proc_dir_entry *proc_debug_entry;
#endif

struct stbcfg01_dev {
	struct device *dev;
	struct i2c_client *i2c;
	struct stbcfg01_platform_data *pdata;
};

#endif /*  __LINUX_MFD_STBCFG01_PRIV_H */
