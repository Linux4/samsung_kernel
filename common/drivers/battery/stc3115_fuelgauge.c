/*
 *  dummy_fuelgauge.c
 *  Samsung Dummy Fuel Gauge Driver
 *
 *  Copyright (C) 2012 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/battery/sec_fuelgauge.h>
#include <linux/i2c.h>
#include <linux/debugfs.h>

#define STC3115_DEBUG

static int fg_regs[] = {
	STC311x_REG_MODE,
	STC311x_REG_CTRL,
	STC311x_REG_SOC,
	STC311x_REG_COUNTER,
	STC311x_REG_CURRENT,
	STC311x_REG_VOLTAGE,
	STC311x_REG_TEMPERATURE,
	STC311x_REG_CC_ADJ_HIGH,
	STC311x_REG_VM_ADJ_HIGH,
	STC311x_REG_CC_ADJ_LOW,
	STC311x_REG_VM_ADJ_LOW,
	STC311x_ACC_CC_ADJ_HIGH,
	STC311x_ACC_CC_ADJ_LOW,
	STC311x_ACC_VM_ADJ_HIGH,
	STC311x_ACC_VM_ADJ_LOW,
	STC311x_REG_OCV,
	STC311x_REG_CC_CNF,
	STC311x_REG_VM_CNF,
	STC311x_REG_ALARM_SOC,
	STC311x_REG_ALARM_VOLTAGE,
	STC311x_REG_CURRENT_THRES,
	STC311x_REG_RELAX_COUNT,
	STC311x_REG_RELAX_MAX,
	-1,
};

/*******************************************************************************
* Function Name  : STC31xx_Write
* Description    : utility function to write several bytes to STC311x registers
* Input          : NumberOfBytes, RegAddress, TxBuffer
* Return         : error status
* Note: Recommended implementation is to used I2C block write. If not available,
* STC311x registers can be written by 2-byte words (unless NumberOfBytes=1)
* or byte per byte.
*******************************************************************************/
static int STC31xx_Write(struct i2c_client *client, int length, int reg , unsigned char *values)
{
	int ret;

    ret = i2c_smbus_write_i2c_block_data(client, reg, length, values);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

/*******************************************************************************
* Function Name  : STC31xx_Read
* Description    : utility function to read several bytes from STC311x registers
* Input          : NumberOfBytes, RegAddress, , RxBuffer
* Return         : error status
* Note: Recommended implementation is to used I2C block read. If not available,
* STC311x registers can be read by 2-byte words (unless NumberOfBytes=1)
* Using byte per byte read is not recommended since it doesn't ensure register data integrity
*******************************************************************************/
static int STC31xx_Read(struct i2c_client *client, int length, int reg , unsigned char *values)
{
	int ret;

	ret = i2c_smbus_read_i2c_block_data(client, reg, length, values);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}


static void stc311x_get_version(struct i2c_client *client)
{
    dev_info(&client->dev, "STC3115 Fuel-Gauge Ver %s\n", GG_VERSION);
}

/*******************************************************************************
* Function Name  : STC31xx_ReadByte
* Description    : utility function to read the value stored in one register
* Input          : RegAddress: STC311x register,
* Return         : 8-bit value, or 0 if error
*******************************************************************************/
static int STC31xx_ReadByte(struct i2c_client *client, int RegAddress)
{
  int value;
  unsigned char data[2];
  int res;

  res=STC31xx_Read(client, 1, RegAddress, data);

  if (res >= 0)
  {
    /* no error */
    value = data[0];
  }
  else
    value=0;

  return(value);
}

/*******************************************************************************
* Function Name  : STC31xx_WriteByte
* Description    : utility function to write a 8-bit value into a register
* Input          : RegAddress: STC311x register, Value: 8-bit value to write
* Return         : error status (OK, !OK)
*******************************************************************************/
static int STC31xx_WriteByte(struct i2c_client *client, int RegAddress, unsigned char Value)
{
  int res;
  unsigned char data[2];

  data[0]= Value;
  res = STC31xx_Write(client, 1, RegAddress, data);

  return(res);

}

/*******************************************************************************
* Function Name  : STC31xx_ReadWord
* Description    : utility function to read the value stored in one register pair
* Input          : RegAddress: STC311x register,
* Return         : 16-bit value, or 0 if error
*******************************************************************************/
static int STC31xx_ReadWord(struct i2c_client *client, int RegAddress)
{
	int value;
	unsigned char data[2];
	int res;

	res=STC31xx_Read(client, 2, RegAddress, data);

	if (res >= 0)
	{
		/* no error */
		value = data[1];
		value = (value <<8) + data[0];
	}
	else
		value=0;

	return(value);
}


/*******************************************************************************
* Function Name  : STC31xx_WriteWord
* Description    : utility function to write a 16-bit value into a register pair
* Input          : RegAddress: STC311x register, Value: 16-bit value to write
* Return         : error status (OK, !OK)
*******************************************************************************/
static int STC31xx_WriteWord(struct i2c_client *client, int RegAddress, int Value)
{
	int res;
	unsigned char data[2];

	data[0]= Value & 0xff;
	data[1]= (Value>>8) & 0xff;
	res = STC31xx_Write(client, 2, RegAddress, data);

	return(res);
}

/*******************************************************************************
* Function Name  : STC311x_xxxx
* Description    :  misc STC311x utility functions
* Input          : None
* Return         : None
*******************************************************************************/
static void STC311x_Reset(struct i2c_client *client)
{
	STC31xx_WriteByte(client, STC311x_REG_CTRL, STC311x_SOFTPOR);  /*   set soft POR */
}

static void STC311x_Reset_VM_Adj(struct i2c_cleint *client)
{
	int value;

	if(BattData.IDCode == STC311x_ID)
		STC31xx_WriteByte(client, STC311x_REG_VM_ADJ_HIGH, 0);
	else
	{
		value=STC31xx_ReadByte(client, STC311x_REG_MODE);
		STC31xx_WriteByte(client, STC311x_REG_MODE, value | STC311x_CLR_VM_ADJ);
	}
}

static void STC311x_Reset_CC_Adj(struct i2c_client *client)
{
	int value;

	if(BattData.IDCode == STC311x_ID)
		STC31xx_WriteByte(client, STC311x_REG_CC_ADJ_HIGH, 0);
	else
	{
		value = STC31xx_ReadByte(client, STC311x_REG_MODE);
		STC31xx_WriteByte(client, STC311x_REG_MODE, value | STC311x_CLR_CC_ADJ);
	}
}

/*******************************************************************************
* Function Name  : STC311x_Status
* Description    :  Read the STC311x status
* Input          : None
* Return         : status word (REG_MODE / REG_CTRL), -1 if error
*******************************************************************************/
static int STC311x_Status(struct i2c_client *client)
{
	int value;

	/* first, check the presence of the STC311x by reading first byte of dev. ID */
	BattData.IDCode = STC31xx_ReadByte(client, STC311x_REG_ID);
	if (BattData.IDCode!= STC311x_ID && BattData.IDCode!= STC311x_ID_2)
		return (-1);

	/* read REG_MODE and REG_CTRL */
	value = STC31xx_ReadWord(client, STC311x_REG_MODE);
	value &= 0x7fff;

	return (value);
}

/*******************************************************************************
* Function Name  : STC311x_SetParam
* Description    :  initialize the STC311x parameters
* Input          : rst: init algo param
* Return         : 0
*******************************************************************************/
static void STC311x_SetParam(struct i2c_client *client)
{
	int value;

	STC31xx_WriteByte(client, STC311x_REG_MODE, 0x01);  /*   set GG_RUN=0 before changing algo parameters */

	/* init OCV curve */
	if(BattData.IDCode == STC311x_ID)
		STC31xx_Write(client, OCVTAB_SIZE, STC311x_REG_OCVTAB, (unsigned char *) BattData.OCVOffset);
	else
		STC31xx_Write(client, OCVTAB_SIZE, STC311x_REG_OCVTAB, (unsigned char *) BattData.OCVOffset2);

	/* set alm level if different from default */
	if (BattData.Alm_SOC !=0 )
		STC31xx_WriteByte(client, STC311x_REG_ALARM_SOC, BattData.Alm_SOC*2);
	if (BattData.Alm_Vbat !=0 )
	{
		value= ((BattData.Alm_Vbat << 9) / VoltageFactor); /* LSB=8*2.44mV */
		STC31xx_WriteByte(client, STC311x_REG_ALARM_VOLTAGE, value);
	}

	/* relaxation timer */
	if (BattData.RelaxThreshold !=0 )
	{
		value = ((BattData.RelaxThreshold << 9) / BattData.CurrentFactor);   /* LSB=8*5.88uV/Rsense */
		STC31xx_WriteByte(client, STC311x_REG_CURRENT_THRES,value);
	}

	/* set parameters if different from default, only if a restart is done (battery change) */
	if (GG_Ram.reg.CC_cnf !=0 )
		STC31xx_WriteWord(client, STC311x_REG_CC_CNF, GG_Ram.reg.CC_cnf);
	if (GG_Ram.reg.VM_cnf !=0 )
		STC31xx_WriteWord(client, STC311x_REG_VM_CNF, GG_Ram.reg.VM_cnf);

	STC31xx_WriteByte(client, STC311x_REG_CTRL, 0x03);  /*   clear PORDET, BATFAIL, free ALM pin, reset conv counter */
	if (BattData.Vmode)
		STC31xx_WriteByte(client, STC311x_REG_MODE, 0x19);  /*   set GG_RUN=1, voltage mode, alm enabled */
	else
		STC31xx_WriteByte(client, STC311x_REG_MODE, 0x18);  /*   set GG_RUN=1, mixed mode, alm enabled */

	return;
}

/*******************************************************************************
* Function Name  : STC311x_Startup
* Description    :  initialize and start the STC311x at application startup
* Input          : None
* Return         : 0 if ok, -1 if error
*******************************************************************************/
static int STC311x_Startup(struct i2c_client *client)
{
	int res;
	int ocv;

	/* check STC310x status */
	res = STC311x_Status(client);
	if (res<0)
		return(res);

	/* read OCV */
	ocv = STC31xx_ReadWord(client, STC311x_REG_OCV);

	STC311x_SetParam(client);  /* set parameters  */

	/* rewrite ocv to start SOC with updated OCV curve */
	STC31xx_WriteWord(client, STC311x_REG_OCV,ocv);

	return(0);
}

/*******************************************************************************
* Function Name  : STC311x_Restore
* Description    :  Restore STC311x state
* Input          : None
* Return         :
*******************************************************************************/
static int STC311x_Restore(struct i2c_client *client)
{
	int res;
	int ocv;

	/* check STC310x status */
	res = STC311x_Status(client);
	if (res < 0)
		return (res);

	/* read OCV */
	ocv=STC31xx_ReadWord(client, STC311x_REG_OCV);

	STC311x_SetParam(client);  /* set parameters  */

#if 1
	/* if restore from unexpected reset, restore SOC (system dependent) */
	if (GG_Ram.reg.GG_Status == GG_RUNNING)
		//if (GG_Ram.reg.SOC != 0)
			STC31xx_WriteWord(client, STC311x_REG_SOC, GG_Ram.reg.SOC * 512);  /*   restore SOC */
#else
	/* rewrite ocv to start SOC with updated OCV curve */
	STC31xx_WriteWord(client, STC311x_REG_OCV,ocv);
#endif
	return(0);
}

/*******************************************************************************
* Function Name  : calcCRC8
* Description    : calculate the CRC8
* Input          : data: pointer to byte array, n: number of vytes
* Return         : CRC calue
*******************************************************************************/
static int calcCRC8(unsigned char *data, int n)
{
	int crc=0;   /* initial value */
	int i, j;

	for (i = 0; i < n; i++)
	{
		crc ^= data[i];
		for (j = 0; j < 8; j++)
		{
			crc <<= 1;
			if (crc & 0x100)  crc ^= 7;
		}
	}

	return(crc & 255);
}

/*******************************************************************************
* Function Name  : UpdateRamCrc
* Description    : calculate the RAM CRC
* Input          : none
* Return         : CRC value
*******************************************************************************/
static int UpdateRamCrc(void)
{
	int res;

	res = calcCRC8(GG_Ram.db,RAM_SIZE-1);
	GG_Ram.db[RAM_SIZE-1] = res;   /* last byte holds the CRC */
	return (res);
}

/*******************************************************************************
* Function Name  : Init_RAM
* Description    : Init the STC311x RAM registers with valid test word and CRC
* Input          : none
* Return         : none
*******************************************************************************/
static void Init_RAM(void)
{
	int index;

	for (index = 0; index < RAM_SIZE; index++)
		GG_Ram.db[index]=0;
	GG_Ram.reg.TstWord = RAM_TSTWORD;  /* id. to check RAM integrity */
	GG_Ram.reg.CC_cnf = BattData.CC_cnf;
	GG_Ram.reg.VM_cnf = BattData.VM_cnf;
	/* update the crc */
	UpdateRamCrc();
}

/*******************************************************************************
* Function Name  : STC311x_ReadRamData
* Description    : utility function to read the RAM data from STC311x
* Input          : ref to RAM data array
* Return         : error status (OK, !OK)
*******************************************************************************/
static int STC311x_ReadRamData(struct i2c_client *client, unsigned char *RamData)
{
	return(STC31xx_Read(client, RAM_SIZE, STC311x_REG_RAM, RamData));
}

/*******************************************************************************
* Function Name  : STC311x_WriteRamData
* Description    : utility function to write the RAM data into STC311x
* Input          : ref to RAM data array
* Return         : error status (OK, !OK)
*******************************************************************************/
static int STC311x_WriteRamData(struct i2c_client *client, unsigned char *RamData)
{
	return (STC31xx_Write(client, RAM_SIZE, STC311x_REG_RAM, RamData));
}

/*******************************************************************************
* Function Name  : Reset_FSM_GG
* Description    : reset the gas gauge state machine and flags
* Input          : None
* Return         : None
*******************************************************************************/
static void Reset_FSM_GG(void)
{
	BattData.BattState = BATT_IDLE;
}

/*******************************************************************************
* Function Name  : GasGauge_Reset
* Description    : Reset the Gas Gauge system
* Input          : None
* Return         : 0 is ok, -1 if I2C error
*******************************************************************************/
void GasGauge_Reset(struct i2c_client *client)
{
	GG_Ram.reg.TstWord=0;  /* reset RAM */
	GG_Ram.reg.GG_Status = 0;
	STC311x_WriteRamData(client, GG_Ram.db);

	STC311x_Reset(client);
}

/*******************************************************************************
* Function Name  : conv
* Description    : conversion utility
*  convert a raw 16-bit value from STC311x registers into user units (mA, mAh, mV, C)
*  (optimized routine for efficient operation on 8-bit processors such as STM8)
* Input          : value, factor
* Return         : result = value * factor / 4096
*******************************************************************************/
static int conv(short value, unsigned short factor)
{
	int v;

	v= ((long)value * factor) >> 11;
	v= (v + 1) / 2;

	return (v);
}

/*******************************************************************************
* Function Name  : Interpolate
* Description    : interpolate a Y value from a X value and X, Y tables (n points)
* Input          : x
* Return         : y
*******************************************************************************/
static int interpolate(int x, int n, int const *tabx, int const *taby )
{
	int index;
	int y;

	if (x >= tabx[0])
		y = taby[0];
	else if (x <= tabx[n-1])
		y = taby[n-1];
	else
	{
		/*  find interval */
		for (index = 1; index < n; index++)
			if (x > tabx[index])
				break;

		/*  interpolate */
		y = (taby[index-1] - taby[index]) * (x - tabx[index]) * 2 / (tabx[index-1] - tabx[index]);
		y = (y+1) / 2;
		y += taby[index];
	}
	return y;
}

static int STC311x_SaveVMCnf(struct i2c_client *client)
{
	int reg_mode;

	/* mode register*/
	reg_mode = BattData.STC_Status & 0xff;

	reg_mode &= ~STC311x_GG_RUN;  /*   set GG_RUN=0 before changing algo parameters */
	STC31xx_WriteByte(client, STC311x_REG_MODE, reg_mode);

	STC31xx_ReadByte(client, STC311x_REG_ID);

	STC31xx_WriteWord(client, STC311x_REG_VM_CNF,GG_Ram.reg.VM_cnf);

	if (BattData.Vmode)
	{
		STC31xx_WriteByte(client, STC311x_REG_MODE,0x19);  /*   set GG_RUN=1, voltage mode, alm enabled */
	}
	else
	{
		STC31xx_WriteByte(client, STC311x_REG_MODE,0x18);  /*   set GG_RUN=1, mixed mode, alm enabled */
		if (BattData.GG_Mode == CC_MODE)
			STC31xx_WriteByte(client, STC311x_REG_MODE,0x38);  /*   force CC mode */
		else
			STC31xx_WriteByte(client, STC311x_REG_MODE,0x58);  /*   force VM mode */
	}

	return(0);
}

/* compensate SOC with temperature, SOC in 0.1% units */
static int CompensateSOC(int value, int temp)
{
	int r, v;

	r = 0;
#ifdef TEMPCOMP_SOC
	r = interpolate(temp / 10, NTEMP, TempTable, BattData.CapacityDerating);  /* for APP_TYP_CURRENT */
#endif
	v = (long) (value-r) * MAX_SOC * 2 / (MAX_SOC-r);   /* compensate */
	v = (v+1)/2;  /* rounding */

	if (v < 0)
		v = 0;

	if (v > MAX_SOC)
		v = MAX_SOC;

	return(v);
}

/*******************************************************************************
* Function Name  : UpdateParam
* Description    : update the algo parameters
* Use            : BattData structure
* Affect         : VM_cnf, CC_cnf
*******************************************************************************/
static void UpdateParam(struct i2c_client *client)
{
	float adapt;

	/* adaptive algorithm at each mode change */
	if ( BattData.GG_Mode != BattData.LastMode )
	{
		/* mode change detected */
		if ( BattData.GG_Mode == CC_MODE)
		{
			/* switch to CC mode, update both VM_Coeff and CC_Coeff gains */
			if (BattData.CC_adj<(-20)) BattData.CC_adj=-20;
			if (BattData.CC_adj>20) BattData.CC_adj=20;
			if ( (BattData.CC_adj<(-2)) || (BattData.CC_adj>2) )
			{
				if ((BattData.AvgTemperature/10)>TEMP_MIN_ADJ)
				{
					if (BattData.LastSOC<BattData.HRSOC)
					{
						GG_Ram.reg.VM_cnf -= (GG_Ram.reg.VM_cnf*BattData.CC_adj)/200;
						GG_Ram.reg.CC_cnf -= (GG_Ram.reg.CC_cnf*BattData.CC_adj)/200;
						BattData.Cnom -= (BattData.Cnom*BattData.CC_adj)/200;
					}
					else
					{
						GG_Ram.reg.VM_cnf += (GG_Ram.reg.VM_cnf*BattData.CC_adj)/200;
						GG_Ram.reg.CC_cnf += (GG_Ram.reg.CC_cnf*BattData.CC_adj)/200;
						BattData.Cnom += (BattData.Cnom*BattData.CC_adj)/200;
					}
				}
				STC311x_Reset_CC_Adj(client);
				STC311x_Reset_VM_Adj(client);
				BattData.LastSOC=BattData.HRSOC;
			}
		}
		BattData.LastMode=BattData.GG_Mode;
	}
}

static void STC311x_SetSOC(struct i2c_client *client, int SOC)
{
	STC31xx_WriteWord(client,STC311x_REG_SOC,SOC);   /* 100% */
}

/*******************************************************************************
* Function Name  : MM_FSM
* Description    : process the Gas Gauge state machine in mixed mode
* Input          : BattData
* Return         :
* Affect         : Global Gas Gauge data
*******************************************************************************/
static void MM_FSM(struct i2c_client *client)
{

	switch (BattData.BattState)
	{
	case BATT_CHARGING:
		if (BattData.AvgCurrent < CHG_MIN_CURRENT)
			BattData.BattState = BATT_ENDCHARG;        /* end of charge */
		break;
	case BATT_ENDCHARG:  /* end of charge state. check if fully charged or charge interrupted */
		if ( BattData.Current > CHG_MIN_CURRENT )
			BattData.BattState = BATT_CHARGING;
		else if (BattData.AvgCurrent < CHG_END_CURRENT )
			BattData.BattState = BATT_IDLE;     /* charge interrupted */
		else if ( (BattData.Current > CHG_END_CURRENT ) && ( BattData.Voltage > BATT_CHG_VOLTAGE ) )
			BattData.BattState = BATT_FULCHARG;  /* end of charge */
		break;
	case BATT_FULCHARG:  /* full charge state. wait for actual end of charge current */
		if ( (BattData.Current > CHG_MIN_CURRENT))
			BattData.BattState = BATT_CHARGING;  /* charge again */
		else if ( BattData.AvgCurrent < CHG_END_CURRENT )
		{
			if ( BattData.AvgVoltage > BATT_CHG_VOLTAGE )
			{
				/* end of charge detected */
				STC311x_SetSOC(client, MAX_HRSOC);
				STC311x_Reset_VM_Adj(client);
				STC311x_Reset_CC_Adj(client);
				BattData.SOC=MAX_SOC;  /* 100% */
				BattData.LastSOC=MAX_HRSOC;
			}
/*        if (BattData.Vmode==0) STC311x_ForceVM(); */
			BattData.BattState = BATT_IDLE;     /* end of charge cycle */
		}
		break;
	case BATT_IDLE:  /* no charging, no discharging */
		if (BattData.Current > CHG_END_CURRENT)
		{
/*        if (BattData.Vmode==0) STC311x_ForceVM(); */
			BattData.BattState = BATT_CHARGING; /* charging again */
		}
		else if (BattData.Current < APP_MIN_CURRENT)
			BattData.BattState = BATT_DISCHARG; /* discharging again */
		break;
	case BATT_DISCHARG:
		if (BattData.Current > APP_MIN_CURRENT)
			BattData.BattState = BATT_IDLE;
		else if (BattData.AvgVoltage < BATT_MIN_VOLTAGE)
			BattData.BattState = BATT_LOWBATT;
		break;
	case BATT_LOWBATT:  /* battery nearly empty... */
		if ( BattData.AvgVoltage > (BATT_MIN_VOLTAGE+50) )
			BattData.BattState = BATT_IDLE;   /* idle */
		else
/*        if (BattData.Vmode==0) STC311x_ForceVM(); */
			break;
	default:
		BattData.BattState = BATT_IDLE;   /* idle */

	} /* end switch */

	if (BattData.Adaptive) UpdateParam(client);
}

static void CompensateVM(struct i2c_client *client, int temp)
{
	int r;

#ifdef TEMPCOMP_SOC
	r = interpolate(temp / 10, NTEMP, TempTable, BattData.VM_TempTable);
	GG_Ram.reg.VM_cnf = (BattData.VM_cnf * r) / 100;
	STC311x_SaveVMCnf(client);  /* save new VM cnf values to STC311x */
#endif
}

/*******************************************************************************
* Function Name  : VM_FSM
* Description    : process the Gas Gauge machine in voltage mode
* Input          : BattData
* Return         :
* Affect         : Global Gas Gauge data
*******************************************************************************/
static void VM_FSM(struct i2c_client *client)
{

#define DELTA_TEMP 30   /* 3 C */

	/* in voltage mode, monitor temperature to compensate voltage mode gain */

	if ( ( BattData.AvgTemperature > (BattData.LastTemperature+DELTA_TEMP)) ||
	     ( BattData.AvgTemperature < (BattData.LastTemperature-DELTA_TEMP)) )
	{
		BattData.LastTemperature = BattData.AvgTemperature;
		CompensateVM(client, BattData.AvgTemperature);
	}

}

/*******************************************************************************
* Function Name  : STC311x_ReadBatteryData
* Description    :  utility function to read the battery data from STC311x
*                  to be called every 5s or so
* Input          : ref to BattData structure
* Return         : error status (OK, !OK)
*******************************************************************************/
static int STC311x_ReadBatteryData(struct i2c_client *client)
{
	unsigned char data[16];
	int res;
	int value;

	res=STC311x_Status(client);
	if (res < 0)
		return(res);  /* return if I2C error or STC3115 not responding */

	/* STC311x status */
	BattData.STC_Status = res;
	if (BattData.STC_Status & M_GGVM)
		BattData.GG_Mode = VM_MODE;   /* VM active */
	else
		BattData.GG_Mode = CC_MODE;   /* CC active */

	/* read STC311x registers 0 to 14 */
	res=STC31xx_Read(client, 15, 0, data);
	if (res < 0)
		return(res);  /* read failed */

	/* fill the battery status data */
	/* SOC */
	value = data[3];
	value = (value<<8) + data[2];
	BattData.HRSOC = value;     /* result in 1/512% */

	/* conversion counter */
	value=data[5];
	value = (value<<8) + data[4];
	BattData.ConvCounter = value;

	/* current */
	value=data[7];
	value = (value<<8) + data[6];
	value &= 0x3fff;   /* mask unused bits */
	if (value>=0x2000)
		value -= 0x4000;  /* convert to signed value */
	BattData.Current = conv(value, BattData.CurrentFactor);  /* result in mA */

	/* voltage */
	value=data[9];
	value = (value<<8) + data[8];
	value &= 0x0fff; /* mask unused bits */
	if (value>=0x0800)
		value -= 0x1000;  /* convert to signed value */
	value = conv(value,VoltageFactor);  /* result in mV */
	BattData.Voltage = value;  /* result in mV */

	/* temperature */
	value=data[10];
	if (value>=0x80)
		value -= 0x100;  /* convert to signed value */
	BattData.Temperature = value*10;  /* result in 0.1C */

	/* CC & VM adjustment counters */
	BattData.CC_adj = data[11];  /* result in 0.5% */
	if (BattData.CC_adj>=0x80)
		BattData.CC_adj -= 0x100;  /* convert to signed value, result in 0.5% */
	BattData.VM_adj = data[12];  /* result in 0.5% */
	if (BattData.VM_adj>=0x80)
		BattData.VM_adj -= 0x100;  /* convert to signed value, result in 0.5% */

	/* OCV */
	value=data[14];
	value = (value<<8) + data[13];
	value &= 0x3fff; /* mask unused bits */
	if (value>=0x02000)
		value -= 0x4000;  /* convert to signed value */
	value = conv(value,VoltageFactor);
	value = (value+2) / 4;  /* divide by 4 with rounding */
	BattData.OCV = value;  /* result in mV */

	res=STC31xx_Read(client, 1, STC311x_REG_RELAX_COUNT, data);
	if (res<0)
		return(res);  /* read failed */
	BattData.RelaxTimer = data[0];

  return(OK);
}

static int STC31xx_Task(struct i2c_client *client, GasGauge_DataTypeDef *GG)
{
	int res, value;

	BattData.Rsense = GG->Rsense;
	BattData.Vmode = GG->Vmode;
	BattData.Alm_SOC = GG->Alm_SOC;
	BattData.Alm_Vbat = GG->Alm_Vbat;
	BattData.RelaxThreshold = GG->RelaxCurrent;
	BattData.Adaptive = GG->Adaptive;

	res = STC311x_ReadBatteryData(client);
	if (res != 0)
		return (-1);

	STC311x_ReadRamData(client, GG_Ram.db);

	if ((GG_Ram.reg.TstWord != RAM_TSTWORD) || (calcCRC8(GG_Ram.db, RAM_SIZE) != 0))
	{
		Init_RAM();
		GG_Ram.reg.GG_Status = GG_INIT;
	}

	if ((BattData.STC_Status & M_BATFAIL) != 0)
	{
		BattData.BattOnline = 0;
	}

#ifdef BATD_UC8
	if ((BattData.STC_Status & M_BATFAIL) != 0)
	{
		if (BattData.ConvCounter > 0)
		{
			GG->Voltage = BattData.Voltage;
			GG->SOC = (BattData.HRSOC * 10 + 256) / 512;
		}
//		GasGauge_Reset(client);
		return (-1);
	}
#endif

	if ((BattData.STC_Status & M_RUN) == 0)
	{
		STC311x_Restore(client);
		GG_Ram.reg.GG_Status = GG_INIT;
	}

	BattData.SOC = (BattData.HRSOC * 10 + 256) /512;
	if (BattData.SOC < 5)
		BattData.SOC = 0;
	else if (BattData.SOC < 30)
		BattData.SOC = (BattData.SOC - 5) * 30 / 25;

	if(GG->ForceExternalTemperature == 1)
		BattData.Temperature = GG->ExternalTemperature;

	if (GG_Ram.reg.GG_Status == GG_INIT)
	{
		if (BattData.ConvCounter > VCOUNT)
		{
			CompensateVM(client, BattData.Temperature);
			BattData.LastTemperature = BattData.Temperature;

			BattData.AvgVoltage = BattData.Voltage;
			BattData.AvgCurrent = BattData.Current;
			BattData.AvgTemperature = BattData.Temperature;
			BattData.AvgSOC = CompensateSOC(BattData.SOC, BattData.Temperature);
			BattData.AccVoltage = BattData.AvgVoltage * AVGFILTER;
			BattData.AccCurrent = BattData.AvgCurrent * AVGFILTER;
			BattData.AccTemperature = BattData.AvgTemperature * AVGFILTER;
			BattData.AccSOC = BattData.AvgSOC * AVGFILTER;

			BattData.LastSOC = BattData.HRSOC;
			BattData.LastMode = BattData.GG_Mode;

			GG_Ram.reg.GG_Status = GG_RUNNING;
		}
	}

	if (GG_Ram.reg.GG_Status != GG_RUNNING)
	{
		GG->SOC = CompensateSOC(BattData.SOC, 250);
		GG->Voltage = BattData.Voltage;
		GG->OCV = BattData.OCV;
		GG->Current = 0;
		GG->Temperature = 250;
	} else {
		if ((BattData.STC_Status & M_BATFAIL) == 0)
		{
			BattData.BattOnline = 1;
		}

		BattData.SOC = CompensateSOC(BattData.SOC, BattData.Temperature);

		if (BattData.AvgVoltage < APP_MIN_VOLTAGE && BattData.SOC < 100)
			BattData.SOC = 0;
		else if (BattData.AvgVoltage < (APP_MIN_VOLTAGE+200)) {
			BattData.SOC = BattData.SOC * (BattData.AvgVoltage - APP_MIN_VOLTAGE) / 200;
			if (BattData.SOC < 0)
				BattData.SOC = 0;
		}

		BattData.AccVoltage += (BattData.Voltage - BattData.AvgVoltage);
		BattData.AccCurrent += (BattData.Current - BattData.AvgCurrent);
		BattData.AccTemperature += (BattData.Temperature - BattData.AvgTemperature);
		BattData.AccSOC +=  (BattData.SOC - BattData.AvgSOC);

		BattData.AvgVoltage = (BattData.AccVoltage+AVGFILTER/2)/AVGFILTER;
		BattData.AvgCurrent = (BattData.AccCurrent+AVGFILTER/2)/AVGFILTER;
		BattData.AvgTemperature = (BattData.AccTemperature+AVGFILTER/2)/AVGFILTER;
		BattData.AvgSOC = (BattData.AccSOC+AVGFILTER/2)/AVGFILTER;

		/* ---------- process the Gas Gauge algorithm -------- */

		if (BattData.Vmode)
			VM_FSM(client);  /* in voltage mode */
		else
			MM_FSM(client);  /* in mixed mode */

		//Lately fully compensation
		if(BattData.BattState > BATT_IDLE && BattData.SOC >= 990 && BattData.SOC < 995 && BattData.AvgCurrent > 220)
		{
			BattData.SOC = 990;
			STC311x_SetSOC(client, 50688);
		}

		GG->Voltage = BattData.Voltage;
		GG->Current = BattData.Current;
		GG->Temperature = BattData.Temperature;
		GG->SOC = BattData.SOC;
		GG->OCV = BattData.OCV;

		GG->AvgVoltage = BattData.AvgVoltage;
		GG->AvgCurrent = BattData.AvgCurrent;
		GG->AvgTemperature = BattData.AvgTemperature;
		GG->AvgSOC = BattData.AvgSOC;

		if (BattData.Vmode)
		{
			/* no current value in voltage mode */
			GG->Current = 0;
			GG->AvgCurrent = 0;
		}

		GG->ChargeValue = (long) BattData.Cnom * BattData.AvgSOC / MAX_SOC;
		if (GG->Current<APP_MIN_CURRENT && BattData.AvgCurrent != 0)
		{
			GG->State=BATT_DISCHARG;
			value = GG->ChargeValue * 60 / (-BattData.AvgCurrent);  /* in minutes */
			if (value < 0)
				value=0;
			GG->RemTime = value;
		}
		else
		{
			GG->RemTime = -1;   /* means no estimated time available */
			if (GG->AvgCurrent>CHG_END_CURRENT)
				GG->State=BATT_CHARGING;
			else
				GG->State=BATT_IDLE;
		}
	}

	GG_Ram.reg.HRSOC = BattData.HRSOC;
	GG_Ram.reg.SOC = (BattData.SOC + 5) / 10;
	UpdateRamCrc();
	STC311x_WriteRamData(client, GG_Ram.db);

#ifdef STC3115_DEBUG // For debug log
	printk("STC3115:%s vol=%d, curr=%d, temp=%d, SOC=%d, ocv=%d, "
	       "avg_vol=%d, avd_curr=%d,avg_temp=%d, avg_soc=%d, GGsts=%c, "
	       "STCsts=0x%x, ExtTemp=%d, Count=%d, HRSOC=%d, CC_cnf=0x%x,\n"
	       "VM_cnf=0x%x, CC_adj=0x%x, VM_adj=0x%x, Online=%d, BattSts=%d\n",
	       __func__, GG->Voltage, GG->Current, GG->Temperature, GG->SOC, GG->OCV,
	       GG->AvgVoltage, GG->AvgCurrent, GG->AvgTemperature, GG->AvgSOC, GG_Ram.reg.GG_Status,
	       BattData.STC_Status, GG->ForceExternalTemperature, BattData.ConvCounter, BattData.HRSOC,
	       BattData.CC_cnf, BattData.VM_cnf, BattData.CC_adj, BattData.VM_adj, BattData.BattOnline,
	       BattData.BattState);

#endif

	if (GG_Ram.reg.GG_Status == GG_RUNNING)
		return 1;
	else
		return 0;
}

static void STC31xx_Work(struct i2c_client *client)
{
	int res, Loop;

	struct sec_fuelgauge_info *fuelgauge = i2c_get_clientdata(client);
	GasGauge_DataTypeDef GasGaugeData;

	GasGaugeData.Vmode = get_battery_data(fuelgauge).Vmode;
	GasGaugeData.Alm_SOC = get_battery_data(fuelgauge).Alm_SOC;
	GasGaugeData.Alm_Vbat = get_battery_data(fuelgauge).Alm_Vbat;
	GasGaugeData.CC_cnf = get_battery_data(fuelgauge).CC_cnf;
	GasGaugeData.VM_cnf = get_battery_data(fuelgauge).VM_cnf;
	GasGaugeData.Cnom = get_battery_data(fuelgauge).Cnom;
	GasGaugeData.Rsense = get_battery_data(fuelgauge).Rsense;
	GasGaugeData.RelaxCurrent = get_battery_data(fuelgauge).RelaxCurrent;
	GasGaugeData.Adaptive = get_battery_data(fuelgauge).Adaptive;
	GasGaugeData.ForceExternalTemperature = get_battery_data(fuelgauge).ForceExternalTemperature;

	for(Loop = 0; Loop < NTEMP; Loop++)
		GasGaugeData.CapDerating[Loop] = get_battery_data(fuelgauge).CapDerating[Loop];
	for(Loop = 0; Loop < 16; Loop++)
		GasGaugeData.OCVOffset[Loop] = get_battery_data(fuelgauge).OCVOffset[Loop];
	for(Loop = 0; Loop < 16; Loop++)
		GasGaugeData.OCVOffset2[Loop] = get_battery_data(fuelgauge).OCVOffset2[Loop];

	res = STC31xx_Task(client, &GasGaugeData);

	if (res > 0)
	{
		fuelgauge->info.batt_soc = GasGaugeData.SOC;
		fuelgauge->info.batt_voltage = GasGaugeData.Voltage;
		fuelgauge->info.batt_avgvoltage = GasGaugeData.AvgVoltage;
		fuelgauge->info.batt_ocv = GasGaugeData.OCV;
		fuelgauge->info.batt_current = GasGaugeData.Current;
		fuelgauge->info.batt_avgcurrent = GasGaugeData.AvgCurrent;
		fuelgauge->info.temperature = GasGaugeData.Temperature;
	} else {
		fuelgauge->info.batt_voltage = GasGaugeData.Voltage;
		fuelgauge->info.batt_soc = GasGaugeData.SOC;
	}

	/* prevent early power off */
	if (fuelgauge->info.batt_soc < 10) {
		if (fuelgauge->info.batt_voltage < 3400)
			fuelgauge->info.vol_count++;
		else
			fuelgauge->info.vol_count = 0;

		if (fuelgauge->info.vol_count <= 3) {
			pr_info("%s: prevent early power off : %d\n", __func__,
				fuelgauge->info.batt_voltage);
			fuelgauge->info.batt_soc = 10;
		}
	}
}

static int STC31xx_Init(struct i2c_client *client, GasGauge_DataTypeDef *GG)
{
	int i, res;

	BattData.Cnom = GG->Cnom;
	BattData.Rsense = GG->Rsense;
	BattData.Vmode = GG->Vmode;
	BattData.CC_cnf = GG->CC_cnf;
	BattData.VM_cnf = GG->VM_cnf;
	BattData.Alm_SOC = GG-> Alm_SOC;
	BattData.Alm_Vbat = GG->Alm_Vbat;
	BattData.RelaxThreshold = GG->RelaxCurrent;
	BattData.Adaptive = GG->Adaptive;

	BattData.BattOnline = 1;

	if (BattData.Rsense == 0)
		BattData.Rsense = 10;

	BattData.CurrentFactor = 24084 / BattData.Rsense;

	if (BattData.CC_cnf == 0)
		BattData.CC_cnf = 395;
	if (BattData.VM_cnf == 0)
		BattData.VM_cnf = 321;

	for (i = 0; i < NTEMP; i++)
		BattData.CapacityDerating[i] = GG->CapDerating[i];
	for (i = 0; i < OCVTAB_SIZE; i++)
		BattData.OCVOffset[i] = GG->OCVOffset[i];
	for (i = 0; i < OCVTAB_SIZE; i++)
		BattData.OCVOffset2[i] = GG->OCVOffset2[i];
	for (i = 0; i < NTEMP; i++)
		BattData.VM_TempTable[i] = DefVMTempTable[i];

	/* check RAM valid */
	STC311x_ReadRamData(client, GG_Ram.db);

	if ( (GG_Ram.reg.TstWord != RAM_TSTWORD) || (calcCRC8(GG_Ram.db,RAM_SIZE)!=0) )
	{
		/* RAM invalid */
		Init_RAM();
		res = STC311x_Startup(client);  /* return -1 if I2C error or STC3115 not present */
	} else {
		/* check STC3115 status */
		if ((STC311x_Status(client) & M_RST) != 0 )
		{
			res = STC311x_Startup(client);  /* return -1 if I2C error or STC3115 not present */
		} else {
			res = STC311x_Restore(client); /* recover from last SOC */
		}
	}

	GG_Ram.reg.GG_Status = GG_INIT;
        /* update the crc */
	UpdateRamCrc();
	STC311x_WriteRamData(client, GG_Ram.db);
	Reset_FSM_GG();
	return (res);    /* return -1 if I2C error or STC3115 not present */
}

static int STC311x_debugfs_show(struct seq_file *s, void *data)
{
	struct sec_fuelgauge_info *fuelgauge = s->private;
	u8 i = 0;
	u8 reg_data;

	seq_printf(s, "STC3115 FUELGAUGE IC :\n");
	seq_printf(s, "==================\n");
	while (fg_regs[i] != -1) {
		STC31xx_Read(fuelgauge->client, 1, fg_regs[i], &reg_data);
		seq_printf(s, "0x%02x:\t0x%0x\n", fg_regs[i], reg_data);
		i++;
	}

	seq_printf(s, "\n");
	return 0;
}

static int STC311x_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, STC311x_debugfs_show, inode->i_private);
}

static const struct file_operations STC311x_debugfs_fops = {
	.open           = STC311x_debugfs_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};

bool sec_hal_fg_init(struct i2c_client *client)
{
	struct sec_fuelgauge_info *fuelgauge = i2c_get_clientdata(client);
	int ret, Loop;
	u8 val;
	GasGauge_DataTypeDef GasGaugeData;

	stc311x_get_version(client);

	GasGaugeData.Vmode = get_battery_data(fuelgauge).Vmode;
	GasGaugeData.Alm_SOC = get_battery_data(fuelgauge).Alm_SOC;
	GasGaugeData.Alm_Vbat = get_battery_data(fuelgauge).Alm_Vbat;
	GasGaugeData.CC_cnf = get_battery_data(fuelgauge).CC_cnf;
	GasGaugeData.VM_cnf = get_battery_data(fuelgauge).VM_cnf;
	GasGaugeData.Cnom = get_battery_data(fuelgauge).Cnom;
	GasGaugeData.Rsense = get_battery_data(fuelgauge).Rsense;
	GasGaugeData.RelaxCurrent = get_battery_data(fuelgauge).RelaxCurrent;
	GasGaugeData.Adaptive = get_battery_data(fuelgauge).Adaptive;
	GasGaugeData.ForceExternalTemperature = get_battery_data(fuelgauge).ForceExternalTemperature;

	for(Loop = 0; Loop < NTEMP; Loop++)
		GasGaugeData.CapDerating[Loop] = get_battery_data(fuelgauge).CapDerating[Loop];
	for(Loop = 0; Loop < 16; Loop++)
		GasGaugeData.OCVOffset[Loop] = get_battery_data(fuelgauge).OCVOffset[Loop];
	for(Loop = 0; Loop < 16; Loop++)
		GasGaugeData.OCVOffset2[Loop] = get_battery_data(fuelgauge).OCVOffset2[Loop];

	ret = STC31xx_Init(client, &GasGaugeData);

	ret = STC31xx_Task(client, &GasGaugeData);

	if (ret > 0)
	{
		fuelgauge->info.batt_soc = GasGaugeData.SOC;
		fuelgauge->info.batt_voltage = GasGaugeData.Voltage;
		fuelgauge->info.batt_avgvoltage = GasGaugeData.AvgVoltage;
		fuelgauge->info.batt_ocv = GasGaugeData.OCV;
		fuelgauge->info.batt_current = GasGaugeData.Current;
		fuelgauge->info.batt_avgcurrent = GasGaugeData.AvgCurrent;
		fuelgauge->info.temperature = GasGaugeData.Temperature;
	} else {
		fuelgauge->info.batt_voltage = GasGaugeData.Voltage;
		fuelgauge->info.batt_soc = GasGaugeData.SOC;
	}

	/* prevent early power off */
	if (fuelgauge->info.batt_soc < 10) {
		if (fuelgauge->info.batt_voltage < 3450)
			fuelgauge->info.vol_count++;
		else
			fuelgauge->info.vol_count = 0;

		if (fuelgauge->info.vol_count <= 3) {
			pr_info("%s: prevent early power off : %d\n", __func__,
				fuelgauge->info.batt_voltage);
			fuelgauge->info.batt_soc = 10;
		}
	}

	(void) debugfs_create_file("stc3115_regs",
		S_IRUGO, NULL, (void *)fuelgauge, &STC311x_debugfs_fops);

	return true;
}

bool sec_hal_fg_reset(struct i2c_client *client)
{
	dev_info(&client->dev, "%s start\n", __func__);

	GasGauge_Reset(client);

	msleep(1000);

	STC311x_Startup(client);

	msleep(200);

	return true;
}

bool sec_hal_fg_suspend(struct i2c_client *client)
{
	return true;
}

bool sec_hal_fg_resume(struct i2c_client *client)
{
	return true;
}

bool sec_hal_fg_fuelalert_init(struct i2c_client *client, int soc)
{
	int res;

	res = STC31xx_ReadByte(client, STC311x_REG_MODE);

	res = STC31xx_WriteByte(client, STC311x_REG_MODE, (res | STC311x_ALM_ENA));
	if (res != 0)
		return false;

	return true;
}

bool sec_hal_fg_is_fuelalerted(struct i2c_client *client)
{
	int res;

	res = STC31xx_ReadByte(client, STC311x_REG_CTRL);
	res = res >> 5;

	dev_info(&client->dev, "%s:0x%x : 0x%x\n",
		 __func__,
		 STC311x_REG_CTRL,
		 res);

	if (res == 0)
		return false;
	else
		return true;
}

bool sec_hal_fg_full_charged(struct i2c_client *client)
{
	return true;
}

bool sec_hal_fg_fuelalert_process(void *irq_data, bool is_fuel_alerted)
{
	return true;
}

bool sec_hal_fg_get_property(struct i2c_client *client,
			       enum power_supply_property psp,
			       union power_supply_propval *val)
{
	int res;
	struct sec_fuelgauge_info *fuelgauge = i2c_get_clientdata(client);
	int temp_value;

	switch (psp) {

	/* Cell voltage (VCELL, mV) */
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		STC31xx_Work(client);
		val->intval = fuelgauge->info.batt_voltage;
		break;

	/* Additional Voltage Information (mV) */
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		switch (val->intval) {
		case SEC_BATTEY_VOLTAGE_AVERAGE:
			val->intval = fuelgauge->info.batt_avgvoltage;
			break;
		case SEC_BATTEY_VOLTAGE_OCV:
			val->intval = fuelgauge->info.batt_ocv;
			break;
		}
		break;

	/* Current (mA) */
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = fuelgauge->info.batt_current;
		break;

	/* Average Current (mA) */
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		val->intval = fuelgauge->info.batt_avgcurrent;
		break;

	/* SOC (%) */
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = fuelgauge->info.batt_soc;
		break;

	/* Battery Temperature */
	case POWER_SUPPLY_PROP_TEMP:

	/* Target Temperature */
	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
		if ((fuelgauge->info.temperature > 470) &&
		    (fuelgauge->info.temperature < 510))
			temp_value = 60;
		else
			temp_value = 40;

		val->intval = fuelgauge->info.temperature - temp_value;
		break;
	default:
		return false;
	}
	return true;
}

bool sec_hal_fg_set_property(struct i2c_client *client,
			       enum power_supply_property psp,
			       const union power_supply_propval *val)
{
	switch (psp) {

	case POWER_SUPPLY_PROP_ONLINE:
	/* Battery Temperature */
	case POWER_SUPPLY_PROP_TEMP:

	/* Target Temperature */
	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
		break;
	default:
		return false;
	}
	return true;
}
