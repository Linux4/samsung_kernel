/*
 *  Copyright (C) 2011 STMicroelectronics.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __STBCFG01_FUELGAUGE_H_
#define __STBCFG01_FUELGAUGE_H_

#include <linux/mfd/core.h>
#include <linux/mfd/stbcfg01.h>
#include <linux/mfd/stbcfg01-private.h>

#define sec_fuelgauge_dev_t	struct stbcfg01_dev
#define sec_fuelgauge_pdata_t	struct stbcfg01_platform_data

/* structure of the STBCFG01 battery monitoring parameters */
typedef struct  {
	int Voltage;        /* battery voltage in mV */
	int Current;        /* battery current in mA */
	int Temperature;    /* battery temperature in 0.1°C */
	int SOC;            /* battery relative SOC (%) in 0.1% */
	int OCV;
	int AvgSOC;
	int AvgCurrent;
	int AvgVoltage;
	int AvgTemperature;
	int ChargeValue;    /* remaining capacity in mAh */
	int RemTime;        /* battery remaining operating time during discharge (min) */
	/* -- parameters -- */
	int Alm_en;      /* 1=alarms enabled, 0=alarms disabled */
	int Alm_SOC;     /* SOC alm level */
	int Alm_Vbat;    /* Vbat alm level */
	int VM_cnf;      /* nominal VM cnf for discharging */
	int VM_cnf_chg;  /* nominal VM cnf for charging */
	int Cnom;        /* nominal capacity in mAh */
	int CapDerating[6];   /* capacity derating in 0.1%, for temp = 60, 40, 25, 10,   0, -10 °C */
	int VM_cnf_comp[6];   /* VM_cnf temperature compensation */
	int OCVOffset[16];    /* OCV curve adjustment */
	int ExternalTemperature;
	int ForceExternalTemperature;
	int SysCutoffVolt;
	int EarlyEmptyCmp;
} GasGauge_DataTypeDef;

/* structure of the STBCFG01 battery monitoring data */
typedef struct  {
	/* STBCFG01 data */
	int STBCFG_GG_Status;  /* status word  */
	int Voltage;     /* voltage in mV            */
	int Current;     /* current in mA            */
	int Temperature; /* temperature in 0.1°C     */
	int HRSOC;       /* uncompensated SOC in 1/512%   */
	int OCV;         /* OCV in mV */
	int ConvCounter; /* convertion counter       */
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
	int GG_Mode;     /* 1=VM active, 0=CC active */
	int LastMode;
	int LastSOC;
	int LastTemperature;
	int BattOnline;	// BATD
	int LastChgStatus;
	/* parameters */
	int Alm_en;      /* FG alarms enable */
	int Alm_SOC;     /* SOC alm level in % */
	int Alm_Vbat;    /* Vbat alm level in mV */
	int VM_cnf;      /* nominal VM cnf for discharging */
	int VM_cnf_chg;  /* nominal VM cnf for charging */
	int Cnom;        /* nominal capacity is mAh */
	int VM_TempTable[NTEMP];
	int CapacityDerating[NTEMP];
	char OCVOffset[OCVTAB_SIZE];
} STBCFG01_BattDataTypeDef;

static STBCFG01_BattDataTypeDef BattData;         /* STBCFG01 global batt data */

/* structure of the STBCFG01 RAM registers for the Gas Gauge algorithm data */
static union {
	unsigned char db[RAM_SIZE];  /* last byte holds the CRC */
	struct {
		short int TstWord;     /* 0-1 */
		short int HRSOC;       /* 2-3 SOC backup */
		short int dummy;       /* 4-5 not used */
		short int VM_cnf;      /* 6-7 current VM_cnf */
		char SOC;              /* 8 SOC for trace (in %) */
		char GG_Status;        /* 9  */
		/* bytes ..RAM_SIZE-2 are free, last byte RAM_SIZE-1 is the CRC */
	} reg;
} GG_Ram;

struct sec_fg_info {
	/* State Of Battery Connect */
	int online;
	/* battery SOC (capacity) */
	int batt_soc;
	/* battery voltage */
	int batt_voltage;
	/* Current */
	int batt_current;
	/* Battery Charger status */
	int status;
	int ExternalTemperature;
	int current_temp_adc;
};

#endif /* __STBCFG01_FUELGAUGE_H_ */


