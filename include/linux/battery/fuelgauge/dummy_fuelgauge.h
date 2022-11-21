/*
 *  Copyright (C) 2011 STMicroelectronics.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __STC3115_BATTERY_H_
#define __STC3115_BATTERY_H_

#include <linux/android_alarm.h>

#define GG_VERSION "2.00a"

/*Function declaration*/
int STC31xx_SetPowerSavingMode(void);
int STC31xx_StopPowerSavingMode(void);
int STC31xx_AlarmSet(void);
int STC31xx_AlarmStop(void);
int STC31xx_AlarmGet(void);
int STC31xx_AlarmClear(void);
int STC31xx_AlarmSetVoltageThreshold(int VoltThresh);
int STC31xx_AlarmSetSOCThreshold(int SOCThresh);
int STC31xx_RelaxTmrSet(int CurrentThreshold);

extern int MainTrim(struct i2c_client *client);
/* ******************************************************************************** */
/*        STC311x DEVICE SELECTION                                                  */
/* STC3115 version only                                                             */
/* -------------------------------------------------------------------------------- */
#define STC3115 
#define BATD_UC8
/* ******************************************************************************** */


/* Private define ------------------------------------------------------------*/


/* ******************************************************************************** */
/*        SPECIAL FUNCTIONS                                                         */
/* -------------------------------------------------------------------------------- */
/*                                                                                  */
/* define TEMPCOMP_SOC to enable SOC temperature compensation */
#define TEMPCOMP_SOC

/* ******************************************************************************** */


/* ******************************************************************************** */
/*        INTERNAL PARAMETERS                                                       */
/*   TO BE ADJUSTED ACCORDING TO BATTERY/APPLICATION CHARACTERISTICS                */
/* -------------------------------------------------------------------------------- */
/*                                                                                  */
#define BATT_CHG_VOLTAGE   4250   /* min voltage at the end of the charge (mV)      */
#define BATT_MIN_VOLTAGE   3300   /* nearly empty battery detection level (mV)      */
#define MAX_HRSOC          51200  /* 100% in 1/512% units*/
#define MAX_SOC            1000   /* 100% in 0.1% units */
/*                                                                                  */
#define CHG_MIN_CURRENT     200   /* min charge current in mA                       */
#define CHG_END_CURRENT      20   /* end charge current in mA                       */
#define APP_MIN_CURRENT     (-5)  /* minimum application current consumption in mA ( <0 !) */
#define APP_MIN_VOLTAGE	    3000  /* application cut-off voltage                    */
#define TEMP_MIN_ADJ		 (-5) /* minimum temperature for gain adjustment */

#define VMTEMPTABLE        { 85, 90, 100, 160, 320, 440, 840 }  /* normalized VM_CNF at 60, 40, 25, 10, 0, -10C, -20C */

#define AVGFILTER           4  /* average filter constant */

/* ******************************************************************************** */

/* Private define ------------------------------------------------------------*/

#define STC31xx_SLAVE_ADDRESS            0xE0   /* STC31xx 8-bit address byte */

/*Address of the STC311x register --------------------------------------------*/
#define STC311x_REG_MODE                 0x00    /* Mode Register             */
#define STC311x_REG_CTRL                 0x01    /* Control and Status Register */
#define STC311x_REG_SOC                  0x02    /* SOC Data (2 bytes) */
#define STC311x REG_COUNTER              0x04    /* Number of Conversion (2 bytes) */
#define STC311x_REG_CURRENT              0x06    /* Battery Current (2 bytes) */
#define STC311x_REG_VOLTAGE              0x08    /* Battery Voltage (2 bytes) */
#define STC311x_REG_TEMPERATURE          0x0A    /* Temperature               */
#define STC311x_REG_CC_ADJ_HIGH          0x0B    /* CC adjustement     */
#define STC311x_REG_VM_ADJ_HIGH          0x0C    /* VM adjustement     */
#define STC311x_REG_CC_ADJ_LOW           0x19    /* CC adjustement     */
#define STC311x_REG_VM_ADJ_LOW           0x1A    /* VM adjustement     */
#define STC311x_ACC_CC_ADJ_HIGH          0x1B    /* CC accumulator     */
#define STC311x_ACC_CC_ADJ_LOW           0x1C    /* CC accumulator     */
#define STC311x_ACC_VM_ADJ_HIGH          0x1D    /* VM accumulator     */
#define STC311x_ACC_VM_ADJ_LOW           0x1E    /* VM accumulator     */
#define STC311x_REG_OCV                  0x0D    /* Battery OCV (2 bytes) */
#define STC311x_REG_CC_CNF               0x0F    /* CC configuration (2 bytes)    */
#define STC311x_REG_VM_CNF               0x11    /* VM configuration (2 bytes)    */
#define STC311x_REG_ALARM_SOC            0x13    /* SOC alarm level         */
#define STC311x_REG_ALARM_VOLTAGE        0x14    /* Low voltage alarm level */
#define STC311x_REG_CURRENT_THRES        0x15    /* Current threshold for relaxation */
#define STC311x_REG_RELAX_COUNT          0x16    /* Voltage relaxation counter   */
#define STC311x_REG_RELAX_MAX            0x17    /* Voltage relaxation max count */

/*Bit mask definition*/
#define STC311x_VMODE   		 0x01	 /* Voltage mode bit mask     */
#define STC311x_ALM_ENA			 0x08	 /* Alarm enable bit mask     */
#define STC311x_GG_RUN			 0x10	 /* Alarm enable bit mask     */
#define STC311x_FORCE_CC		 0x20	 /* Force CC bit mask     */
#define STC311x_FORCE_VM		 0x40	 /* Force VM bit mask     */
#define STC311x_SOFTPOR 		 0x11	 /* soft reset     */
#define STC311x_CLR_VM_ADJ   0x02  /* Clear VM ADJ register bit mask */
#define STC311x_CLR_CC_ADJ   0x04  /* Clear CC ADJ register bit mask */

#define STC311x_REG_ID                   0x18    /* Chip ID (1 byte)       */
#define STC311x_ID                       0x13    /* STC3115 ID */
#define STC311x_ID_2                     0x14    /* STC3115 ID */

#define STC311x_REG_RAM                  0x20    /* General Purpose RAM Registers */
#define RAM_SIZE                         16      /* Total RAM size of STC3115 in bytes */

#define STC311x_REG_OCVTAB               0x30
#define OCVTAB_SIZE                      16      /* OCVTAB size of STC3115 in bytes */

#define VCOUNT				 4       /* counter value for 1st current/temp measurements */


#define M_STAT 0x1010       /* GG_RUN & PORDET mask in STC311x_BattDataTypeDef status word */
#define M_RST  0x1800       /* BATFAIL & PORDET mask */
#define M_RUN  0x0010       /* GG_RUN mask in STC311x_BattDataTypeDef status word */
#define M_GGVM 0x0400       /* GG_VM mask */
#define M_BATFAIL 0x0800    /* BATFAIL mask*/
#define M_VMOD 0x0001       /* VMODE mask */

#define OK 0

/* STC311x RAM test word */
#define RAM_TSTWORD 0x53A9

/* Gas gauge states */
#define GG_INIT     'I'
#define GG_RUNNING  'R'
#define GG_POWERDN  'D'

#define VM_MODE 1
#define CC_MODE 0

#define STC311x_VF_BATTERY_ON 200
#define STC311x_VF_BATTERY_OFF 0

/* gas gauge structure definition ------------------------------------*/

#define NTEMP 7
static const int TempTable[NTEMP] = {60, 40, 25, 10, 0, -10, -20} ;   /* temperature table from 60C to -20C (descending order!) */
static const int DefVMTempTable[NTEMP] = VMTEMPTABLE;

/* Private variables ---------------------------------------------------------*/

/* #define CurrentFactor  (24084/SENSERESISTOR)         LSB=5.88uV/R= ~24084/R/4096 - convert to mA  */
#define VoltageFactor  9011                          /* LSB=2.20mV ~9011/4096 - convert to mV         */


/* structure of the STC311x battery monitoring parameters */
typedef struct  {
  int Voltage;        /* battery voltage in mV */
  int Current;        /* battery current in mA */
  int Temperature;    /* battery temperature in 0.1C */
  int SOC;            /* battery relative SOC (%) in 0.1% */
  int OCV;
  int AvgSOC;
  int AvgCurrent;
  int AvgVoltage;
  int AvgTemperature;
  int ChargeValue;    /* remaining capacity in mAh */
  int RemTime;        /* battery remaining operating time during discharge (min) */
  int State;          /* charge (>0)/discharge(<0) state */
  int CalStat;        /* Internal status */
  /* -- parameters -- */
  int Vmode;       /* 1=Voltage mode, 0=mixed mode */
  int Alm_SOC;     /* SOC alm level */
  int Alm_Vbat;    /* Vbat alm level */
  int CC_cnf;      /* nominal CC_cnf */
  int VM_cnf;      /* nominal VM cnf */
  int Cnom;        /* nominal capacity in mAh */
  int Rsense;      /* sense resistor */
  int RelaxCurrent; /* current for relaxation (< C/20) */
  int Adaptive;     /* adaptive mode */
  int CapDerating[6];   /* capacity derating in 0.1%, for temp = 60, 40, 25, 10,   0, -10 C */
  int OCVOffset[16];    /* OCV curve adjustment */
  int OCVOffset2[16];    /* OCV curve adjustment */
  int ExternalTemperature;
  int ForceExternalTemperature;
} GasGauge_DataTypeDef;

/* structure of the STC311x battery monitoring data */
typedef struct  {
  /* STC311x data */
  int STC_Status;  /* status word  */
  int Vmode;       /* 1=Voltage mode, 0=mixed mode */
  int Voltage;     /* voltage in mV            */
  int Current;     /* current in mA            */
  int Temperature; /* temperature in 0.1C     */
  int HRSOC;       /* uncompensated SOC in 1/512%   */
  int OCV;         /* OCV in mV*/
  int ConvCounter; /* convertion counter       */
  int RelaxTimer;  /* current relax timer value */
  int CC_adj;      /* CC adj */
  int VM_adj;      /* VM adj */
  /* results & internals */
  int SOC;         /* compensated SOC in 0.1% */
  int AvgSOC;      /* in 0.1% */
  int AvgVoltage;
  int AvgCurrent;
  int AvgTemperature;
  int AccSOC;
  int AccVoltage;
  int AccCurrent;
  int AccTemperature;
  int BattState;
  int GG_Mode;     /* 1=VM active, 0=CC active */
  int LastMode;
  int LastSOC;
  int LastTemperature;
  int BattOnline;	// BATD
  int IDCode;
  /* parameters */
  int Alm_SOC;     /* SOC alm level in % */
  int Alm_Vbat;    /* Vbat alm level in mV */
  int CC_cnf;      /* nominal CC_cnf */
  int VM_cnf;      /* nominal VM cnf */
  int Cnom;        /* nominal capacity is mAh */
  int Rsense;      /* sense resistor in milliOhms */
  int CurrentFactor;
  int RelaxThreshold;   /* current threshold for VM (mA)  */
  int Adaptive;     /* adaptive mode */
  int VM_TempTable[NTEMP];
  int CapacityDerating[NTEMP];
  char OCVOffset[OCVTAB_SIZE];
  char OCVOffset2[OCVTAB_SIZE];
} STC311x_BattDataTypeDef;

static STC311x_BattDataTypeDef BattData;   /* STC311x data */

/* structure of the STC311x RAM registers for the Gas Gauge algorithm data */
static union {
  unsigned char db[RAM_SIZE];  /* last byte holds the CRC */
  struct {
    short int TstWord;     /* 0-1 */
    short int HRSOC;       /* 2-3 SOC backup */
    short int CC_cnf;      /* 4-5 current CC_cnf */
    short int VM_cnf;      /* 6-7 current VM_cnf */
    char SOC;              /* 8 SOC for trace (in %) */
    char GG_Status;        /* 9  */
    /* bytes ..RAM_SIZE-2 are free, last byte RAM_SIZE-1 is the CRC */
  } reg;
} GG_Ram;

int Capacity_Adjust;

/* -------------------------------------------------------------------------------- */
/*        INTERNAL ANDROID DRIVER PARAMETERS                                        */
/*   TO BE ADJUSTED ACCORDING TO BATTERY/APPLICATION CHARACTERISTICS                */
/* -------------------------------------------------------------------------------- */

#define STC3100_BATTERY_FULL 95
#define STC311x_DELAY	1000  

/* ******************************************************************************** */

/* ******************************************************************************** */

static struct i2c_client *sav_client;

struct stc311x_chip {
	struct i2c_client		*client;
	struct delayed_work		work;  
	struct power_supply		battery;
	struct stc311x_platform_data	*pdata;

	/* State Of Connect */
	int online;
	/* battery SOC (capacity) */
	int batt_soc;
	/* battery voltage */
	int batt_voltage;
	/* Current */
	int batt_current;
	/* State Of Charge */
	int status;

};

struct sec_fg_info {
	int (*battery_online)(void);
	int (*charger_online)(void);
	int (*charger_enable)(void);
	int (*power_supply_register)(struct device *parent, struct power_supply *psy);
	void (*power_supply_unregister)(struct power_supply *psy);

	int Vmode;       /* 1=Voltage mode, 0=mixed mode */
  	int Alm_SOC;     /* SOC alm level %*/
  	int Alm_Vbat;    /* Vbat alm level mV*/
  	int CC_cnf;      /* nominal CC_cnf */
  	int VM_cnf;      /* nominal VM cnf */
  	int Cnom;        /* nominal capacity in mAh */
  	int Rsense;      /* sense resistor mOhms*/
  	int RelaxCurrent; /* current for relaxation in mA (< C/20) */
  	int Adaptive;     /* 1=Adaptive mode enabled, 0=Adaptive mode disabled */
  	int CapDerating[7];   /* capacity derating in 0.1%, for temp = 60, 40, 25, 10,   0, -10 °C,-20°C */
  	int OCVOffset[16];    /* OCV curve adjustment */
	int OCVOffset2[16];    /* OCV curve adjustment */
  	int (*ExternalTemperature) (void); /*External temperature fonction, return °C*/
  	int ForceExternalTemperature; /* 1=External temperature, 0=STC3115 temperature */
};

#endif
