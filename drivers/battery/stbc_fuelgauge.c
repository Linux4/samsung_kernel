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

#ifdef STBCFG01_DEBUG_PROC_FILE
#include <linux/proc_fs.h>
#endif

/* -------------------------------------------------------------------------- */
/* I2C interface */
/* -----------------------------------------------------------------

 The following routines interface with the I2C primitives
   I2C_Read(u8_I2C_address, u8_NumberOfBytes, u8_RegAddress, pu8_RxBuffer);
   I2C_Write(u8_I2C_address, u8_NumberOfBytes, u8_RegAddress, pu8_TxBuffer);

  note: here I2C_Address is the 8-bit address byte

 ----------------------------------------------------------------- */

/*******************************************************************************
* Function Name  : STBCFG01_Write
* Description    : utility function to write several bytes to STBCFG01 registers
* Input          : NumberOfBytes, RegAddress, TxBuffer
* Return         : error status
* Note: Recommended implementation is to used I2C block write. If not available,
* STBCFG01 registers can be written by 2-byte words (unless NumberOfBytes=1)
* or byte per byte.
*******************************************************************************/
int STBCFG01_Write(struct i2c_client *client, int length, int reg, unsigned char *values)
{
	int ret;

	ret = i2c_smbus_write_i2c_block_data(client, reg, length, values);
	if (ret < 0)
		dev_err(&client->dev, "%s: Error(%d)\n", __func__, ret);

#ifdef STBCFG01_DEBUG_MESSAGES_EXTRA_I2C
	{
		int i;
		printk("STBCFG01 I2C Wr: RegAddr 0x%X, Val", reg);
		for (i=0; i<length; i++)
			printk(" 0x%X", values[i]);
		printk("; returned %d\n", ret);
	}
#endif
	return ret;
}

/*******************************************************************************
* Function Name  : STBCFG01_Read
* Description    : utility function to read several bytes from STBCFG01 registers
* Input          : NumberOfBytes, RegAddress, , RxBuffer
* Return         : error status
* Note: Recommended implementation is to used I2C block read. If not available,
* STBCFG01 registers can be read by 2-byte words (unless NumberOfBytes=1)
* Using byte per byte read is not recommended since it doesn't ensure register data integrity
*******************************************************************************/
int STBCFG01_Read(struct i2c_client *client, int length, int reg, unsigned char *values)
{
	int ret;
	ret = i2c_smbus_read_i2c_block_data(client, reg, length, values);
	if (ret < 0)
		dev_err(&client->dev, "%s: Error(%d)\n", __func__, ret);

#ifdef STBCFG01_DEBUG_MESSAGES_EXTRA_I2C
	{
		int i;
		printk("STBCFG01 I2C Rd: RegAddr 0x%X, Val", reg);
		for (i=0; i<length; i++)
			printk(" 0x%X", values[i]);
		printk("; returned %d\n", ret);
	}
#endif
	return ret;
}
/* ---- end of I2C primitive interface --------------------------------------------- */

static int STBCFG01_get_online()
{
	return BattData.BattOnline;
}

/*******************************************************************************
* Function Name  : STBCFG01_ReadByte
* Description    : utility function to read the value stored in one register
* Input          : RegAddress: STBCFG01 register,
* Return         : 8-bit value, or 0 if error
*******************************************************************************/
int STBCFG01_ReadByte(struct i2c_client *client, int RegAddress)
{
	int value;
	unsigned char data[2];
	int res;

	res=STBCFG01_Read(client, 1, RegAddress, data);

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
* Function Name  : STBCFG01_WriteByte
* Description    : utility function to write a 8-bit value into a register
* Input          : RegAddress: STBCFG01 register, Value: 8-bit value to write
* Return         : error status (OK, !OK)
*******************************************************************************/
int STBCFG01_WriteByte(struct i2c_client *client, int RegAddress, unsigned char Value)
{
	int res;
	unsigned char data[2];

	data[0]= Value;
	res = STBCFG01_Write(client, 1, RegAddress, data);

	return(res);
}

/*******************************************************************************
* Function Name  : STBCFG01_ReadWord
* Description    : utility function to read the value stored in one register pair
* Input          : RegAddress: STBCFG01 register,
* Return         : 16-bit value, or 0 if error
*******************************************************************************/
int STBCFG01_ReadWord(struct i2c_client *client, int RegAddress)
{
	int value;
	unsigned char data[2];
	int res;

	res=STBCFG01_Read(client, 2, RegAddress, data);

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
* Function Name  : STBCFG01_WriteWord
* Description    : utility function to write a 16-bit value into a register pair
* Input          : RegAddress: STBCFG01 register, Value: 16-bit value to write
* Return         : error status (OK, !OK)
*******************************************************************************/
int STBCFG01_WriteWord(struct i2c_client *client, int RegAddress, int Value)
{
	int res;
	unsigned char data[2];

	data[0]= Value & 0xff;
	data[1]= (Value>>8) & 0xff;
	res = STBCFG01_Write(client, 2, RegAddress, data);

	return(res);
}
/* ---- end of I2C R/W interface --------------------------------------------- */

/*******************************************************************************
* Function Name  : STBCFG01_AlarmSet
* Description    :  Set the alarm function and set the alarm threshold
* Input          : None
* Return         : error status (OK, !OK)
*******************************************************************************/
static int STBCFG01_AlarmSet(struct i2c_client *client)
{
	int res;

	/* Read the mode register*/
	res = STBCFG01_ReadByte(client, STBCFG01_REG_MODE);

	/* Set the ALM_ENA bit to 1 */
	res = STBCFG01_WriteByte(client, STBCFG01_REG_MODE, (res | STBCFG01_ALM_ENA));
	if (res!= OK)
		return (res);

	return (OK);
}

/*******************************************************************************
* Function Name  : STBCFG01_AlarmStop
* Description    :  Stop the alarm function
* Input          : None
* Return         : error status (OK, !OK)
*******************************************************************************/
static int STBCFG01_AlarmStop(struct i2c_client *client)
{
	int res;

	/* Read the mode register*/
	res = STBCFG01_ReadByte(client, STBCFG01_REG_MODE);

	/* Set the ALM_ENA bit to 0 */
	res = STBCFG01_WriteByte(client, STBCFG01_REG_MODE, (res & ~STBCFG01_ALM_ENA));
	if (res!= OK)
		return (res);

	return (OK);
}

/*******************************************************************************
* Function Name  : STBCFG01_AlarmGet
* Description    : Return the ALM status
* Input          : None
* Return         : ALM status 00 : no alarm
*                             01 : SOC alarm
*                             10 : Voltage alarm
*                             11 : SOC and voltage alarm
*******************************************************************************/
static int STBCFG01_AlarmGet(struct i2c_client *client)
{
	int res;

	/* Read the mode register*/
	res = STBCFG01_ReadByte(client, STBCFG01_REG_CTRL);
	res = res >> 5;

	return (res);
}

/*******************************************************************************
* Function Name  : STBCFG01_AlarmClear
* Description    :  Clear the alarm signal
* Input          : None
* Return         : error status (OK, !OK)
*******************************************************************************/
static int STBCFG01_AlarmClear(struct i2c_client *client)
{
	int res;

	/* clear ALM bits*/
	res = STBCFG01_WriteByte(client, STBCFG01_REG_CTRL, 0x01);
	if (res!= OK)
		return (res);

	return (res);
}

/*******************************************************************************
* Function Name  : STBCFG01_Status
* Description    : Read the STBCFG01 status
* Input          : None
* Return         : status word (REG_MODE / REG_CTRL), -1 if error
*******************************************************************************/
static int STBCFG01_Status(struct i2c_client *client)
{
	int res, value;

	pr_info("[%s]start\n", __func__);

	/* first, check the presence of the STBCFG01 by reading first byte of dev. ID */
	res = STBCFG01_ReadByte(client ,STBCFG01_REG_ID);
	if (res!= STBCFG01_ID)
		return (-1);

	/* read REG_MODE and REG_CTRL */
	value = STBCFG01_ReadWord(client, STBCFG01_REG_MODE);
	value &= 0x7fff;

	pr_info("[%s]end!!!\n", __func__);

	return (value);
}

/*******************************************************************************
* Function Name  : STBCFG01_AlarmSetVoltageThreshold
* Description    : Set the alarm threshold
* Input          : int voltage threshold
* Return         : error status (OK, !OK)
*******************************************************************************/
static int STBCFG01_AlarmSetVoltageThreshold(struct i2c_client *client, int VoltThresh)
{
	int res;
	int value;

	BattData.Alm_Vbat = VoltThresh;

	value= ((BattData.Alm_Vbat << 9) / VoltageFactor);
	res = STBCFG01_WriteByte(client, STBCFG01_REG_ALARM_VOLTAGE, value);
	if (res!= OK)
		return (res);

	return (OK);
}


/*******************************************************************************
* Function Name  : STBCFG01_AlarmSetSOCThreshold
* Description    : Set the alarm threshold
* Input          : int voltage threshold
* Return         : error status (OK, !OK)
*******************************************************************************/
static int STBCFG01_AlarmSetSOCThreshold(struct i2c_client *client, int SOCThresh)
{
	int res;

	BattData.Alm_SOC = SOCThresh;
	res = STBCFG01_WriteByte(client, STBCFG01_REG_ALARM_SOC, BattData.Alm_SOC*2);
	if (res!= OK)
		return (res);

	return (OK);
}

/*******************************************************************************
* Function Name  : STBCFG01_SetParam
* Description    : initialize the STBCFG01 parameters
* Input          : rst: init algo param
* Return         : 0
*******************************************************************************/
static void STBCFG01_SetParam(struct i2c_client *client)
{
	int reg_mode;

	STBCFG01_WriteByte(client, STBCFG01_REG_MODE, 0x01);  /* set GG_RUN=0 before changing algo parameters */

	/* init OCV curve */
	STBCFG01_Write(client, OCVTAB_SIZE, STBCFG01_REG_OCVTAB, (unsigned char *) BattData.OCVOffset);

	// set alarms
	STBCFG01_AlarmSetSOCThreshold(client, BattData.Alm_SOC);
	STBCFG01_AlarmSetVoltageThreshold(client, BattData.Alm_Vbat);

	if (GG_Ram.reg.VM_cnf != 0)
		STBCFG01_WriteWord(client, STBCFG01_REG_VM_CNF, GG_Ram.reg.VM_cnf);
	else
		STBCFG01_WriteWord(client, STBCFG01_REG_VM_CNF, BattData.VM_cnf);

	reg_mode = 0x11;  // set GG run, bit 0 should be written 1
	reg_mode |= 0x20;  // Workaround - disable entering LPM mode of charger
	if (BattData.Alm_en)
		reg_mode |= 0x08;  // enable Alarms

	STBCFG01_WriteByte(client, STBCFG01_REG_CTRL, 0x01);   // Clear flags, set IRQ driven by alarm
	STBCFG01_WriteByte(client, STBCFG01_REG_MODE, reg_mode);

	return;
}



/*******************************************************************************
* Function Name  : STBCFG01_Startup
* Description    : initialize and start the STBCFG01 at application startup
* Input          : None
* Return         : 0 if ok, -1 if error
*******************************************************************************/
static int STBCFG01_Startup(struct i2c_client *client)
{
	int res;
	int ocv;

	/* check STBCFG01 status */
	res = STBCFG01_Status(client);
	if (res<0)
		return(res);

	/* read OCV */
	ocv = STBCFG01_ReadWord(client, STBCFG01_REG_OCV);

	STBCFG01_SetParam(client);  /* set parameters  */

#ifdef STBCFG01_DEBUG_MESSAGES_BASIC
	printk("STBCFG01 Startup() - initial OCV restored to 0x%X (=%d)\n", ocv, ocv);
#endif

	/* rewrite ocv to start SOC with updated OCV curve */
	STBCFG01_WriteWord(client, STBCFG01_REG_OCV,ocv);

	return(0);
}


/*******************************************************************************
* Function Name  : STBCFG01_Restore
* Description    : Restore STBCFG01 state
* Input          : None
* Return         :
*******************************************************************************/
static int STBCFG01_Restore(struct i2c_client *client)
{
	int res;
	int ocv;

	/* check STBCFG01 status */
	res = STBCFG01_Status(client);
	if (res<0)
		return(res);

	/* read OCV */
	ocv = STBCFG01_ReadWord(client, STBCFG01_REG_OCV);

	STBCFG01_SetParam(client);  /* set parameters  */


#ifdef STBCFG01_DEBUG_MESSAGES_BASIC
	printk("STBCFG01 Restore() - initial OCV restored to 0x%X (=%d)\n", ocv, ocv);
#endif
	STBCFG01_WriteWord(client, STBCFG01_REG_OCV,ocv);
	return 0;
}

/*******************************************************************************
* Function Name  : STBCFG01_Powerdown
* Description    :  stop the STBCFG01 at application power down
* Input          : None
* Return         : error status (OK, !OK)
*******************************************************************************/
static int STBCFG01_Powerdown(struct i2c_client *client)
{
	int res;

	/* write 0x01 into the REG_CTRL to release IO0 pin open, */
	STBCFG01_WriteByte(client, STBCFG01_REG_CTRL, 0x01);

	/* write 0 into the REG_MODE register to put the STBCFG01 in standby mode */
	res = STBCFG01_WriteByte(client, STBCFG01_REG_MODE, 0x01);
	if (res!= OK)
		return (res);
	return (OK);
}

/*******************************************************************************
* Function Name  : STBCFG01_xxxx
* Description    :  misc STBCFG01 utility functions
* Input          : None
* Return         : None
*******************************************************************************/
static void STBCFG01_Reset(struct i2c_client *client)
{
	STBCFG01_WriteByte(client, STBCFG01_REG_CTRL, STBCFG01_SOFTPOR);  /*   set soft POR */
}

#if 0
static void STBCFG01_SetSOC(sturct i2c_client *client, int SOC)
{
	STBCFG01_WriteWord(client, STBCFG01_REG_SOC, SOC);   /* 100% */
}
#endif

static int STBCFG01_SaveVMCnf(struct i2c_client *client)
{
	int reg_mode, ocv;

	/* backup OCV(to be retored) */
	ocv = STBCFG01_ReadWord(client, STBCFG01_REG_OCV);

	/* clear GG RUN bit */
	reg_mode = BattData.STBCFG_GG_Status & 0xFF;

	reg_mode &= ~STBCFG01_GG_RUN;  /* set GG_RUN=0 before changing algo parameters */
	STBCFG01_WriteByte(client, STBCFG01_REG_MODE, reg_mode);

	//STBCFG01_ReadByte(client, STBCFG01_REG_ID);  // why? for delay?

	/* write new VM_cnf */
	STBCFG01_WriteWord(client, STBCFG01_REG_VM_CNF, GG_Ram.reg.VM_cnf);

	reg_mode |= STBCFG01_GG_RUN;  /* set GG_RUN=1 again */
	STBCFG01_WriteByte(client, STBCFG01_REG_MODE, reg_mode);

	/* restore OCV */
	STBCFG01_WriteWord(client, STBCFG01_REG_OCV, ocv);

	/* for debug */
	//STBCFG01_ReadWord(client, STBCFG01_REG_OCV);

	return (0);
}


static int STBCFG01_SaveOffsets(struct i2c_client *client)
{
	int reg_mode;

	/* mode register*/
	reg_mode = BattData.STBCFG_GG_Status & 0xff;

	reg_mode &= ~STBCFG01_GG_RUN;  /* set GG_RUN=0 before changing algo parameters */
	STBCFG01_WriteByte(client, STBCFG01_REG_MODE, reg_mode);

	STBCFG01_ReadByte(client, STBCFG01_REG_ID);  // why? for delay?

	STBCFG01_Write(client, OCVTAB_SIZE, STBCFG01_REG_OCVTAB, (unsigned char *) BattData.OCVOffset);

	reg_mode |= STBCFG01_GG_RUN;  /* set GG_RUN=1 again */
	STBCFG01_WriteByte(client, STBCFG01_REG_MODE, reg_mode);

	return (0);
}

/*******************************************************************************
* Function Name  : Interpolate
* Description    : interpolate a Y value from a X value and X, Y tables (n points)
* Input          : x
* Return         : y
*******************************************************************************/
static int interpolate(int x, int n, int const *tabx, int const *taby)
{
	int index;
	int y;

	if (x >= tabx[0])
		y = taby[0];
	else if (x <= tabx[n-1])
		y = taby[n-1];
	else
	{
		/* find interval */
		for (index = 1; index < n; index++)
			if (x > tabx[index])
				break;
		/* interpolate */
		y = (taby[index-1] - taby[index]) * (x - tabx[index]) / (tabx[index-1] - tabx[index]);
		y += taby[index];
	}
	return y;
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
			if (crc & 0x100)
				crc ^= 7;
		}
	}
	return (crc & 255);
}

/*******************************************************************************
* Function Name  : STBCFG01_ReadRamData
* Description    : utility function to read the RAM data from STBCFG01
* Input          : ref to RAM data array

* Return         : error status (OK, !OK)
*******************************************************************************/
static int STBCFG01_ReadRamData(struct i2c_client *client, unsigned char *RamData)
{
	return (STBCFG01_Read(client, RAM_SIZE, STBCFG01_REG_RAM, RamData));
}

/*******************************************************************************
* Function Name  : STBCFG01_WriteRamData
* Description    : utility function to write the RAM data into STBCFG01
* Input          : ref to RAM data array
* Return         : error status (OK, !OK)
*******************************************************************************/
static int STBCFG01_WriteRamData(struct i2c_client *client, unsigned char *RamData)
{
	return (STBCFG01_Write(client, RAM_SIZE, STBCFG01_REG_RAM, RamData));
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
	return(res);
}

/*******************************************************************************
* Function Name  : Init_RAM
* Description    : Init the STBCFG01 RAM registers with valid test word and CRC
* Input          : none
* Return         : none
*******************************************************************************/
static void Init_RAM(void)
{
	int index;

	for (index = 0; index < RAM_SIZE; index++)
		GG_Ram.db[index]=0;
	GG_Ram.reg.TstWord=RAM_TSTWORD;  /* id. to check RAM integrity */
	GG_Ram.reg.dummy = 0;  // not used
	GG_Ram.reg.VM_cnf = BattData.VM_cnf;
	/* update the crc */
	UpdateRamCrc();
}

static int PlatformData2GasGaugeData(struct sec_fuelgauge_info *fuelgauge, GasGauge_DataTypeDef *GasGaugeData)
{
	int Loop;

	if (fuelgauge->pdata->battery_data)
	{
		GasGaugeData->Alm_en = get_battery_data(fuelgauge).Alm_en;       /* FG alarms enable */
		GasGaugeData->Alm_SOC = get_battery_data(fuelgauge).Alm_SOC;     /* SOC alarm level % */
		GasGaugeData->Alm_Vbat = get_battery_data(fuelgauge).Alm_Vbat;   /* Vbat alarm level mV */
		GasGaugeData->VM_cnf = get_battery_data(fuelgauge).VM_cnf;       /* nominal VM cnf for discharging */
		GasGaugeData->VM_cnf_chg = get_battery_data(fuelgauge).VM_cnf_chg;/* nominal VM cnf for charging */
		GasGaugeData->Cnom = get_battery_data(fuelgauge).Cnom;           /* nominal capacity in mAh */
		for(Loop=0;Loop<NTEMP;Loop++)
			GasGaugeData->CapDerating[Loop] = get_battery_data(fuelgauge).CapDerating[Loop];
		for(Loop=0;Loop<NTEMP;Loop++)
			GasGaugeData->VM_cnf_comp[Loop] = get_battery_data(fuelgauge).VM_cnf_comp[Loop];
		for(Loop=0;Loop<16;Loop++)
			GasGaugeData->OCVOffset[Loop] = get_battery_data(fuelgauge).OCVOffset[Loop];
		GasGaugeData->ExternalTemperature = fuelgauge->info.ExternalTemperature; /* External temperature function, return 0.1 °C */
		GasGaugeData->ForceExternalTemperature = 1;   /* no internal sensor, force external */
		GasGaugeData->SysCutoffVolt = get_battery_data(fuelgauge).SysCutoffVolt;
		GasGaugeData->EarlyEmptyCmp = get_battery_data(fuelgauge).EarlyEmptyCmp;
		return 0;
	} else {
		return (-1);
	}
}

/*******************************************************************************
* Function Name  : conv
* Description    : conversion utility
*  convert a raw 16-bit value from STBCFG01 registers into user units (mA, mAh, mV, °C)
*  (optimized routine for efficient operation on 8-bit processors such as STM8)
* Input          : value, factor
* Return         : result = value * factor / 4096
*******************************************************************************/
static int conv(short value, unsigned short factor)
{
	int v;

	v= ( (long) value * factor ) >> 11;
	v= (v+1)/2;

	return (v);
}

static void GasGauge_Reset(struct i2c_client *client)
{
	GG_Ram.reg.TstWord = 0;
	GG_Ram.reg.GG_Status = 0;
	STBCFG01_WriteRamData(client, GG_Ram.db);
	STBCFG01_Reset(client);
}

/*******************************************************************************
* Function Name  : STBCFG01_ReadBatteryData
* Description    :  utility function to read the battery data from STBCFG01
*                  to be called every 5s or so
* Input          : ref to BattData structure
* Return         : error status (OK, !OK)
*******************************************************************************/
static int STBCFG01_ReadBatteryData(struct i2c_client *client, STBCFG01_BattDataTypeDef *BattData)
{
	unsigned char data[16];
	int res;
	int value;

	pr_info("[%s] start \n", __func__);

	res = STBCFG01_Status(client);
	if (res<0)
		return (res);  /* return if I2C error or STBCFG01 not responding */

	/* STBCFG01 status */
	BattData->STBCFG_GG_Status = res;
	BattData->GG_Mode = VM_MODE;   /* VM active */

	/* read STBCFG01 registers 0 to 14 */
	res = STBCFG01_Read(client, 15, 0, data);
	if (res<0)
		return (res);  /* read failed */

	/* fill the battery status data */
	/* SOC */
	value = data[3];
	value = (value<<8) + data[2];
	BattData->HRSOC = value;     /* result in 1/512% */

	/* conversion counter */
	value = data[5];
	value = (value<<8) + data[4];
	BattData->ConvCounter = value;

	BattData->Current = 0;

	/* voltage */
	value = data[9];
	value = (value<<8) + data[8];
	value &= 0x0fff; /* mask unused bits */
	if (value >= 0x0800)
		value -= 0x1000;  /* convert to signed value */
	value = conv(value,VoltageFactor);  /* result in mV */
	BattData->Voltage = value;  /* result in mV */

	/* temperature */
	value = data[10];
	if (value >= 0x80)
		value -= 0x100;  /* convert to signed value */
	BattData->Temperature = value * 10;  /* result in 0.1°C */

	/* OCV */
	value = data[14];
	value = (value<<8) + data[13];
	value &= 0x3fff; /* mask unused bits */
	if (value >= 0x02000)
		value -= 0x4000;  /* convert to signed value */
	value = conv(value,VoltageFactor);
	value = (value+2) / 4;  /* divide by 4 with rounding */
	BattData->OCV = value;  /* result in mV */

	return (OK);
}

/*******************************************************************************
* Function Name  : CompensateSOC
* Description    : compensate SOC with temperature, SOC in 0.1% units
* Use            : -
* Affect         : returns new SOC
*******************************************************************************/
static int CompensateSOC(int value, int temp)
{
	int r, v;

	r=0;
#ifdef TEMPCOMP_SOC
	r = interpolate(temp / 10, NTEMP, TempTable, BattData.CapacityDerating);
#endif
	v = (long) (value-r) * MAX_SOC / (MAX_SOC-r);   /* compensate */
	if (v < 0)
		v = 0;
	if (v > MAX_SOC)
		v = MAX_SOC;

	return (v);
}

/*******************************************************************************
* Function Name  : CompensateVM
* Description    : sets new VMcnf
* Input          : temperature, charging/discharging status
* Return         :
* Affect         : sets new VMcnf
*******************************************************************************/
static void CompensateVM(struct i2c_client *client, int temp, int chg_status)
{
#ifdef TEMPCOMP_VM
	int r;

	r = interpolate(temp / 10, NTEMP, TempTable, BattData.VM_TempTable);
	if (chg_status == POWER_SUPPLY_STATUS_CHARGING)
		GG_Ram.reg.VM_cnf = (BattData.VM_cnf_chg * r) / 100;
	else
		GG_Ram.reg.VM_cnf = (BattData.VM_cnf * r) / 100;
	STBCFG01_SaveVMCnf(client);  /* save new VM cnf values to STBCFG01 */
#ifdef STBCFG01_DEBUG_MESSAGES_BASIC
	printk("STBCFG01: new VM_cnf value %d for temperature %d deg and chg status %d\n", GG_Ram.reg.VM_cnf, (temp/10), chg_status);
#endif
#else
	if (chg_status == POWER_SUPPLY_STATUS_CHARGING)
		GG_Ram.reg.VM_cnf = BattData.VM_cnf_chg;
	else
		GG_Ram.reg.VM_cnf = BattData.VM_cnf;
	STBCFG01_SaveVMCnf(client);  /* save new VM cnf values to STBCFG01 */
#ifdef STBCFG01_DEBUG_MESSAGES_BASIC
	printk("STBCFG01: new VM_cnf value %d for chg status %d\n", GG_Ram.reg.VM_cnf, chg_status);
#endif
#endif
}

#ifdef STBCFG01_DEBUG_PROC_FILE
static int STBCFG01_read_proc_reg(struct sec_fuelgauge_info *fuelgauge, char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
	int len = 0;

	len += sprintf(buf+len, "STBCFG01 registers (status from last I2C read operation)\n");
	len += sprintf(buf+len, "Driver version: %s\n", STBCFG01_DRIVER_VERSION);
	len += sprintf(buf+len, "Fuelgauge:\n");
	len += sprintf(buf+len, " Mode=0x%X, Control=0x%X, ExtTemp=%d\n", (BattData.STBCFG_GG_Status & 0xFF), (BattData.STBCFG_GG_Status >> 8), (BattData.Temperature/10));
	len += sprintf(buf+len, " Volt=%d, VoltAvg=%d, OCV=%d, SOC=%d, SOCAvg=%d, HRSOC=0x%X\n", BattData.Voltage, BattData.AvgVoltage, BattData.OCV, BattData.SOC, BattData.AvgSOC, BattData.HRSOC);
	len += sprintf(buf+len, " AlmSOC=%d, AlmVolt=%d\n", BattData.Alm_SOC, BattData.Alm_Vbat);
	len += sprintf(buf+len, " VM_cnf=0x%X, VM_cnf_chg=0x%X, now used VM_cnf=0x%X\n", BattData.VM_cnf, BattData.VM_cnf_chg, GG_Ram.reg.VM_cnf);
	len += sprintf(buf+len, "Offsets: %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d\n", (char) BattData.OCVOffset[0], (char) BattData.OCVOffset[1], (char) BattData.OCVOffset[2], (char) BattData.OCVOffset[3], (char) BattData.OCVOffset[4], (char) BattData.OCVOffset[5], (char) BattData.OCVOffset[6], (char) BattData.OCVOffset[7], (char) BattData.OCVOffset[8], (char) BattData.OCVOffset[9], (char) BattData.OCVOffset[10], (char) BattData.OCVOffset[11], (char) BattData.OCVOffset[12], (char) BattData.OCVOffset[13], (char) BattData.OCVOffset[14], (char) BattData.OCVOffset[15]);
	len += sprintf(buf+len, "Power supply structure values to be provided:\n");
	len += sprintf(buf+len, " SOC=%d, Volt=%d, Curr=%d, Status=%d, Online=%d\n", fuelgauge->info.batt_soc, fuelgauge->info.batt_voltage, fuelgauge->info.batt_current, fuelgauge->info.status, fuelgauge->info.online);

	return len;
}

static int STBCFG01_read_proc_debug(struct sec_fuelgauge_info *fuelgauge, char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
	int len = 0;

	len += sprintf(buf+len, "STBCFG01 fuelgauge parameters changer\n");
	len += sprintf(buf+len, "Write in this file ASCII string of space delimited hexadecimal values to update parameters\n");
	len += sprintf(buf+len, "Params order: VM_cnf VM_cnf_chg or VM_cnf VM_cnf_chg Offset[0] .. Offset[15]\n");

	return len;
}

static int hex2val_4(char digit)  // TBD use smarter logic, include smallcaps
{
	int ret;
	switch(digit) {
	case '1':
		ret = 1;
		break;
	case '2':
		ret = 2;
		break;
	case '3':
		ret = 3;
		break;
	case '4':
		ret = 4;
		break;
	case '5':
		ret = 5;
		break;
	case '6':
		ret = 6;
		break;
	case '7':
		ret = 7;
		break;
	case '8':
		ret = 8;
		break;
	case '9':
		ret = 9;
		break;
	case 'A':
		ret = 0xA;
		break;
	case 'B':
		ret = 0xB;
		break;
	case 'C':
		ret = 0xC;
		break;
	case 'D':
		ret = 0xD;
		break;
	case 'E':
		ret = 0xE;
		break;
	case 'F':
		ret = 0xF;
		break;
	default:
		ret = 0;
	}
	return ret;
}

static int hex2val_8(char digit1, char digit2)
{
	return ((hex2val_4(digit1) << 4) + hex2val_4(digit2));
}

static int hex2val_16(char digit1, char digit2, char digit3, char digit4)
{
	return ((hex2val_4(digit1) << 12) + (hex2val_4(digit2) << 8) + (hex2val_4(digit3) << 4) + hex2val_4(digit4));
}

static int STBCFG01_write_proc_debug(struct sec_fuelgauge_info *fuelgauge, struct file *file, const char *buf, int count, void *data)
{
	int i,x;
	int new_VM_cnf, new_VM_cnf_chg;
	int new_Offset[16];

	if (count > 59)
		count = 59;  // 2*3 for VM_cnf + 16*3 for offsets -1 last space

	printk("STBCFG01 New FG data written (char count %d)\n", count);

	i = 0;
	if (count >= (i+4)) {
		new_VM_cnf = hex2val_16(buf[i], buf[i+1], buf[i+2], buf[i+3]);
		i = i+4;
		printk("New VM_cnf %d (0x%X)\n", new_VM_cnf, new_VM_cnf);
	} else {
		printk("Premature data end\n");
		return count;
	}

	if (count >= (i+1)) {
		if (buf[i] != ' ') {
			printk("Invalid data delimiter\n");
			return count;
		}
		i = i+1;
	} else {
		printk("Premature data end\n");
		return count;
	}

	if (count >= (i+4)) {
		new_VM_cnf_chg = hex2val_16(buf[i], buf[i+1], buf[i+2], buf[i+3]);
		i = i+4;
		printk("New VM_cnf_chg %d (0x%X)\n", new_VM_cnf_chg, new_VM_cnf_chg);
	} else {
		printk("Premature data end\n");
		return count;
	}

	if (count < (i+3)) {
		BattData.VM_cnf = new_VM_cnf;
		BattData.VM_cnf_chg = new_VM_cnf_chg;
		CompensateVM(fuelgauge->client, BattData.Temperature, STBCFG01_get_status());
		printk("Data end, only VM_cnf and VM_cnf_chg updated. Re-plug USB to complete the update.\n");
		return count;
	} else {
		if (buf[i] != ' ') {
			printk("Invalid data delimiter\n");
			return count;
		}
		i = i+1;
	}

	for (x=0; x<15; x++) {
		if (count >= (i+2)) {
			new_Offset[x] = hex2val_8(buf[i], buf[i+1]);
			i = i+2;
			printk("New Offset[%d] %d (0x%X)\n", x, new_Offset[x], new_Offset[x]);
		} else {
			printk("Premature data end\n");
			return count;
		}

		if (count >= (i+1)) {
			if (buf[i] != ' ') {
				printk("Invalid data delimiter\n");
				return count;
			}
			i = i+1;
		} else {
			printk("Premature data end\n");
			return count;
		}
	}

	x = 15;
	if (count >= (i+2)) {
		new_Offset[x] = hex2val_8(buf[i], buf[i+1]);
		i = i+2;
		printk("New Offset[%d] %d (0x%X)\n", x, new_Offset[x], new_Offset[x]);
	} else {
		printk("Premature data end\n");
		return count;
	}

	for (x=0; x<=15; x++) {
		BattData.OCVOffset[x] = new_Offset[x];
	}
	STBCFG01_SaveOffsets(fuelgauge->client);

	BattData.VM_cnf = new_VM_cnf;
	BattData.VM_cnf_chg = new_VM_cnf_chg;
	CompensateVM(fuelgauge->client, BattData.Temperature, STBCFG01_get_status());

	printk("Data end, VM_cnf, VM_cnf_chg and Offsets updated. Re-plug USB to complete the update.\n");

	return count;
}
#endif

static bool STBCFG01_Bat_check(struct i2c_client *client)
{
	int res;
	bool ret = true;

	res = STBCFG01_ReadByte(client, STBCFG01_REG_CTRL);

	res = res >> 2;

	if (res & 0x1)
		ret = false;

	pr_info("[%s]Battery = %d\n", __func__, ret);

	return ret;
}

/*******************************************************************************
* Function Name  : GasGauge_Task
* Description    : Periodic Gas Gauge task, to be called e.g. every 5 sec.
* Input          : pointer to gas gauge data structure
* Return         : 1 if GG running, 0 if not running, -1 if error, 2 if running but flag that reset recovered
* Affect         : global STBCFG01 data and gas gauge variables
*******************************************************************************/
static int GasGauge_Task(struct i2c_client *client, GasGauge_DataTypeDef *GG)
{
	int res;
	int was_reset = 0;

	BattData.Alm_en = GG->Alm_en;
	BattData.Alm_SOC = GG->Alm_SOC;
	BattData.Alm_Vbat = GG->Alm_Vbat;

	// read battery data into global variables
	res = STBCFG01_ReadBatteryData(client, &BattData);
	if (res!=0)
		return(-1); /* abort in case of I2C failure */

	// check if RAM data is ok (battery has not been changed)
	STBCFG01_ReadRamData(client, GG_Ram.db);
	if ((GG_Ram.reg.TstWord!= RAM_TSTWORD) || (calcCRC8(GG_Ram.db,RAM_SIZE)!=0))
	{
		/* if RAM non ok, reset it and set init state */
		Init_RAM();
		GG_Ram.reg.GG_Status = GG_INIT;
		was_reset = 1;
#ifdef STBCFG01_DEBUG_MESSAGES_BASIC
		printk("STBCFG01 GasGauge_Task() - invalid RAM CRC detected\n");
#endif
	}

	// check battery presence
	if ((BattData.STBCFG_GG_Status & M_BATFAIL) != 0)
	{
		BattData.BattOnline = 0;
	}

	// if not running restore STBCFG01
	if ((BattData.STBCFG_GG_Status & M_RUN) == 0)
	{
#ifdef STBCFG01_DEBUG_MESSAGES_BASIC
		printk("STBCFG01 GasGauge_Task() - GG not running, restart it\n");
#endif
		STBCFG01_Restore(client);
		GG_Ram.reg.GG_Status = GG_INIT;
		was_reset = 1;
		msleep(100);
		res = STBCFG01_ReadBatteryData(client, &BattData);
		if (res != 0)
			return (-1); /* abort in case of I2C failure */
	}

	// process SOC (rounding, correcting 3% to 0.5% region)
	BattData.SOC = (BattData.HRSOC * 10 + 256) / 512;  /* in 0.1% unit  */
	if (BattData.SOC < 5)
		BattData.SOC = 0;
	else if (BattData.SOC < 30)
		BattData.SOC = (BattData.SOC - 5) * 30 / 25;

	// force external temperature
	if (GG->ForceExternalTemperature == 1)
		BattData.Temperature = GG->ExternalTemperature;

	// check INIT state
	if (GG_Ram.reg.GG_Status == GG_INIT)
	{
		int status;

		/* update VM_cnf */
		status = STBCFG01_get_status();
		CompensateVM(client, BattData.Temperature, status);
		BattData.LastChgStatus = status;
		BattData.LastTemperature = BattData.Temperature;

		/* init averaging */
		BattData.AvgVoltage = BattData.Voltage;
		BattData.AvgCurrent = BattData.Current;
		BattData.AvgTemperature = BattData.Temperature;
		BattData.AvgSOC = CompensateSOC(BattData.SOC, BattData.Temperature);  /* in 0.1% unit  */
		BattData.AccVoltage = BattData.AvgVoltage*AVGFILTER;
		BattData.AccCurrent = BattData.AvgCurrent*AVGFILTER;
		BattData.AccTemperature = BattData.AvgTemperature*AVGFILTER;
		BattData.AccSOC = BattData.AvgSOC*AVGFILTER;

		/* init adaptive algo */
		BattData.LastSOC=BattData.HRSOC;
		BattData.LastMode=BattData.GG_Mode;

		GG_Ram.reg.GG_Status = GG_RUNNING;
	}

	// check battery presence
	if ((BattData.STBCFG_GG_Status & M_BATFAIL) == 0)
		BattData.BattOnline = 1;

	// SOC derating with temperature
	BattData.SOC = CompensateSOC(BattData.SOC, BattData.Temperature);

	// early empty compensation (reduce SOC if VBAT very low)
	if (GG->EarlyEmptyCmp > 0) {
		if (BattData.Voltage < (GG->SysCutoffVolt + GG->EarlyEmptyCmp))
			BattData.SOC = BattData.SOC * (BattData.Voltage - GG->SysCutoffVolt) / GG->EarlyEmptyCmp;
	}
	if (BattData.Voltage < GG->SysCutoffVolt)
		BattData.SOC = 0;


	// averaging
	BattData.AccVoltage += (BattData.Voltage - BattData.AvgVoltage);
	BattData.AccCurrent += (BattData.Current - BattData.AvgCurrent);
	BattData.AccTemperature += (BattData.Temperature - BattData.AvgTemperature);
	BattData.AccSOC +=  (BattData.SOC - BattData.AvgSOC);

	BattData.AvgVoltage = (BattData.AccVoltage+AVGFILTER/2)/AVGFILTER;
	BattData.AvgCurrent = (BattData.AccCurrent+AVGFILTER/2)/AVGFILTER;
	BattData.AvgTemperature = (BattData.AccTemperature+AVGFILTER/2)/AVGFILTER;
	BattData.AvgSOC = (BattData.AccSOC+AVGFILTER/2)/AVGFILTER;

	// monitor temperature to compensate VM_cnf
	if ((BattData.AvgTemperature > (BattData.LastTemperature+DELTA_TEMP)) ||
	    (BattData.AvgTemperature < (BattData.LastTemperature-DELTA_TEMP)))
	{
		int status;
		status = STBCFG01_get_status();
		CompensateVM(client, BattData.AvgTemperature, status);
		BattData.LastTemperature = BattData.AvgTemperature;
	}
	// monitor charging status to compensate VM_cnf
	if (STBCFG01_get_status() != BattData.LastChgStatus)
	{
		int status;
		status = STBCFG01_get_status();
		BattData.LastChgStatus = status;
		CompensateVM(client, BattData.AvgTemperature, status);
	}

	/* -------- APPLICATION RESULTS ------------ */
	/* fill gas gauge data with battery data */
	GG->Voltage = BattData.Voltage;
	GG->Current = BattData.Current;
	GG->Temperature = BattData.Temperature;
	GG->SOC = BattData.SOC;
	GG->OCV = BattData.OCV;

	GG->AvgVoltage = BattData.AvgVoltage;
	GG->AvgCurrent = BattData.AvgCurrent;
	GG->AvgTemperature = BattData.AvgTemperature;
	GG->AvgSOC = BattData.AvgSOC;

	// SOC range check (preventive operation)
	if (GG->SOC < 0)
		GG->SOC = 0;
	else if (GG->SOC > 1000)
		GG->SOC = 1000;  // in 0.1%

	GG->ChargeValue = (long) BattData.Cnom * BattData.AvgSOC / MAX_SOC;
	GG->RemTime = -1;   /* means no estimated time available */

	/* save SOC */
	GG_Ram.reg.HRSOC = BattData.HRSOC;
	GG_Ram.reg.SOC = (GG->SOC+5)/10;    /* trace SOC in % */
	UpdateRamCrc();
	STBCFG01_WriteRamData(client, GG_Ram.db);

	if (GG_Ram.reg.GG_Status==GG_RUNNING)
	{
		if (was_reset)
			return 2;
		else
			return 1;
	}
	else
		return 0;
}

/*******************************************************************************
* Function Name  : GasGauge_Start
* Description    : Start the Gas Gauge system
* Input          : algo parameters in GG structure
* Return         : 0 is ok, -1 if STBCFG01 not found or I2C error
* Affect         : global STBCFG01 data and gas gauge variables
*******************************************************************************/
static int GasGauge_Start(struct i2c_client *client, GasGauge_DataTypeDef *GG)
{
	int res, i;

	BattData.Cnom = GG->Cnom;
	BattData.VM_cnf = GG->VM_cnf;
	BattData.VM_cnf_chg = GG->VM_cnf_chg;
	BattData.Alm_en = GG->Alm_en;
	BattData.Alm_SOC = GG->Alm_SOC;
	BattData.Alm_Vbat = GG->Alm_Vbat;

	// BATD
	BattData.BattOnline = 1;
	if (BattData.VM_cnf==0)
		BattData.VM_cnf=300;

	for (i=0;i<NTEMP;i++)
		BattData.CapacityDerating[i] = GG->CapDerating[i];
	for (i=0;i<OCVTAB_SIZE;i++)
		BattData.OCVOffset[i] = GG->OCVOffset[i];
	for (i=0;i<NTEMP;i++)
		BattData.VM_TempTable[i] = GG->VM_cnf_comp[i];

	/* check RAM valid */
	STBCFG01_ReadRamData(client, GG_Ram.db);

	if ((GG_Ram.reg.TstWord != RAM_TSTWORD) || (calcCRC8(GG_Ram.db,RAM_SIZE) != 0))
	{
		/* RAM invalid */
		Init_RAM();
		res = STBCFG01_Startup(client);  /* return -1 if I2C error or STBCFG01 not present */
	}
	else
	{
		/* check STBCFG01 status */
		if ((STBCFG01_Status(client) & M_RST) != 0)
		{
			res = STBCFG01_Startup(client);  /* return -1 if I2C error or STBCFG01 not present */
		}
		else
		{
			res = STBCFG01_Restore(client); /* recover from last */
		}
	}

	GG_Ram.reg.GG_Status = GG_INIT;
	/* update the crc */
	UpdateRamCrc();
	STBCFG01_WriteRamData(client, GG_Ram.db);

	return (res);    /* return -1 if I2C error or STBCFG01 not present */
}

static void STBCFG01_work(struct sec_fuelgauge_info *fuelgauge)
{
	GasGauge_DataTypeDef GasGaugeData;
	int res;

#ifdef STBCFG01_DEBUG_MESSAGES_BASIC
	printk("STBCFG01 work() started\n");
#endif

	PlatformData2GasGaugeData(fuelgauge, &GasGaugeData);

	res = GasGauge_Task(fuelgauge->client, &GasGaugeData);  // process gas gauge algorithm, returns result

#ifdef STBCFG01_DEBUG_MESSAGES_BASIC
	printk("STBCFG01_work(): GasGauge_Task finished, return value %d\n", res);
#endif

	if (res >= 0)
	{
		fuelgauge->info.batt_soc = GasGaugeData.SOC+5;
		fuelgauge->info.batt_voltage = GasGaugeData.Voltage;
		fuelgauge->info.batt_current = GasGaugeData.Current;
		fuelgauge->info.status = STBCFG01_get_status();
		fuelgauge->info.online = STBCFG01_get_online();
	}
	else if (res == -1) // I2C communication error, do not update anything (last values already there)
	{
	}

	pr_info("[%s]voltage = [%d][%d]\n", __func__,
		fuelgauge->info.batt_voltage, GasGaugeData.Voltage);
	pr_info("[%s]soc = [%d][%d]\n", __func__,
		fuelgauge->info.batt_soc, GasGaugeData.SOC+5);
	pr_info("[%s]current = [%d][%d]\n", __func__,
		fuelgauge->info.batt_current, GasGaugeData.Current);
	pr_info("[%s]temp = [%d]\n", __func__,
		BattData.Temperature);
}

bool sec_hal_fg_init(struct sec_fuelgauge_info *fuelgauge)
{
	int ret, res;
	GasGauge_DataTypeDef GasGaugeData;

	fuelgauge->info.ExternalTemperature = 250;

	/* init gas gauge data structure */
	ret = PlatformData2GasGaugeData(fuelgauge, &GasGaugeData);

	if (ret==0)
		printk("STBCFG01_probe(): FG platformdata processed\n");
	else
		printk("STBCFG01_probe(): platformdata not provided\n");

	GasGauge_Start(fuelgauge->client, &GasGaugeData);

	ret = GasGauge_Task(fuelgauge->client, &GasGaugeData);
	if (res >= 0)
	{
		fuelgauge->info.batt_soc = GasGaugeData.SOC+5;
		fuelgauge->info.batt_voltage = GasGaugeData.Voltage;
		fuelgauge->info.batt_current = GasGaugeData.Current;

		fuelgauge->info.status = STBCFG01_get_status();
		fuelgauge->info.online = STBCFG01_get_online();
	}
	else if (res == -1)  // I2C communication error, init variables by some reasonable values
	{
		fuelgauge->info.batt_soc = 1000;   // not 0% otherwise phone switches off automatically in a short time
		fuelgauge->info.batt_voltage = 0;
		fuelgauge->info.batt_current = 0;
		fuelgauge->info.status = POWER_SUPPLY_STATUS_UNKNOWN;
		fuelgauge->info.online = 1;
	}

#ifdef STBCFG01_DEBUG_MESSAGES_BASIC
	printk("STBCFG01_probe(): GasGauge_Task finished, return value %d\n", res);
#endif

#ifdef STBCFG01_DEBUG_MESSAGES_EXTRA_REGISTERS
	printk("STBCFG01_probe(): GasGauge structure data listing after _Task\n");
	printk(" Volt=%d, Curr=%d, Temp=%d, SOC=%d, OCV=%d, AvSOC=%d, AvCur=%d, AvVolt=%d, AvTmp=%d, ChgVal=%d, RemTm=%d\n", GasGaugeData.Voltage, GasGaugeData.Current, GasGaugeData.Temperature, GasGaugeData.SOC, GasGaugeData.OCV, GasGaugeData.AvgSOC, GasGaugeData.AvgCurrent, GasGaugeData.AvgVoltage, GasGaugeData.AvgTemperature, GasGaugeData.ChargeValue, GasGaugeData.RemTime);
	printk(" AlmSOC=%d, AlmVbat=%d, VMcnf=%d, Cnom=%d, ForceExTmp=%d\n", GasGaugeData.Alm_SOC, GasGaugeData.Alm_Vbat, GasGaugeData.VM_cnf, GasGaugeData.Cnom, GasGaugeData.ForceExternalTemperature);

	printk("STBCFG01_work(): BattData structure data listing after _Task\n");
	printk(" Volt=%d, OCV=%d, SOC=%d, HRSOC=%d TEMP=%d\n", BattData.Voltage, BattData.OCV, BattData.SOC, BattData.HRSOC, BattData.Temperature);
#endif

#ifdef STBCFG01_DEBUG_PROC_FILE
	create_proc_read_entry("driver/stbcfg01_reg", 0, NULL, STBCFG01_read_proc_reg, NULL);
#ifdef STBCFG01_DEBUG_MESSAGES_BASIC
	printk("STBCFG01: /proc/driver/STBCFG01_reg file created\n");
#endif
	proc_debug_entry = create_proc_entry("driver/stbcfg01_debug", 0666, NULL);
	proc_debug_entry->read_proc = STBCFG01_read_proc_debug;
	proc_debug_entry->write_proc = STBCFG01_write_proc_debug;
#ifdef STBCFG01_DEBUG_MESSAGES_BASIC
	printk("STBCFG01: /proc/driver/stbcfg01_debug r/w file created\n");
#endif
#endif
	return true;
}

bool sec_hal_fg_suspend(struct sec_fuelgauge_info *fuelgauge)
{
	return true;
}

bool sec_hal_fg_resume(struct sec_fuelgauge_info *fuelgauge)
{
	return true;
}

bool sec_hal_fg_fuelalert_init(struct sec_fuelgauge_info *fuelgauge, int soc)
{
	int res;

	pr_info("[%s]\n", __func__);

	res = STBCFG01_AlarmSet(fuelgauge->client);
	if (res != OK)
		return false;

	return true;
}

bool sec_hal_fg_is_fuelalerted(struct sec_fuelgauge_info *fuelgauge)
{
	int res;

	res = STBCFG01_AlarmGet(fuelgauge->client);
	pr_info("[%s]res = %d\n", __func__, res);

	if (res == 1 || res == 2) {
		STBCFG01_AlarmClear(fuelgauge->client);
		STBCFG01_AlarmSet(fuelgauge->client);
		return true;
	}
	return false;
}

bool sec_hal_fg_full_charged(struct sec_fuelgauge_info *fuelgauge)
{
	return true;
}

bool sec_hal_fg_fuelalert_process(void *irq_data, bool is_fuel_alerted)
{
	return true;
}

bool sec_hal_fg_reset(struct sec_fuelgauge_info *fuelgauge)
{
	pr_info("[%s] start\n", __func__);

	GasGauge_Reset(fuelgauge->client);
	msleep(1000);
	STBCFG01_Startup(fuelgauge->client);
	msleep(200);
	return true;
}

bool sec_hal_fg_get_property(struct sec_fuelgauge_info *fuelgauge,
			       enum power_supply_property psp,
			       union power_supply_propval *val)
{
	val->intval = 1;

	switch (psp) {
	/* Cell voltage (VCELL, mV) */
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		STBCFG01_work(fuelgauge);
		val->intval = fuelgauge->info.batt_voltage;
		break;

	/* Additional Voltage Information (mV) */
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		val->intval = fuelgauge->info.batt_voltage;
		break;

	/* Current (mA) */
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = fuelgauge->info.batt_current;
		break;

	/* Average Current (mA) */
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		val->intval = fuelgauge->info.batt_current;
		break;

	/* SOC (%) */
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = fuelgauge->info.batt_soc;
		break;

	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = STBCFG01_Bat_check(fuelgauge->client);
		break;

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

bool sec_hal_fg_set_property(struct sec_fuelgauge_info *fuelgauge,
			       enum power_supply_property psp,
			       const union power_supply_propval *val)
{
	switch (psp) {

	case POWER_SUPPLY_PROP_STATUS:
	case POWER_SUPPLY_PROP_CHARGE_FULL:
	case POWER_SUPPLY_PROP_ONLINE:
	case POWER_SUPPLY_PROP_CAPACITY:
		break;
	/* Battery Temperature */
	case POWER_SUPPLY_PROP_TEMP:
		fuelgauge->info.ExternalTemperature = val->intval;
		break;
	/* Target Temperature */
	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
		break;
	default:
		return false;
	}
	return true;
}

ssize_t sec_hal_fg_show_attrs(struct device *dev,
				const ptrdiff_t offset, char *buf)
{
	struct sec_fg_info *info = dev_get_drvdata(dev->parent);
	int i = 0;

	switch (offset) {
/*	case FG_REG:
		break;
	case FG_DATA:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%02x%02x\n",
			info->reg_data[1], info->reg_data[0]);
		break;
*/
	default:
		i = -EINVAL;
		break;
	}
	return i;
}


ssize_t sec_hal_fg_store_attrs(struct device *dev,
				const ptrdiff_t offset,
				const char *buf, size_t count)
{
	struct sec_fg_info *info = dev_get_drvdata(dev->parent);
	int ret = 0;
	int x = 0;
	int data;

	switch (offset) {
/*
	case FG_REG:
		if (sscanf(buf, "%x\n", &x) == 1) {
			info->reg_addr = x;
			data = rt5033_fg_i2c_read_word(info,
				info->reg_addr);
			info->reg_data[0] = data&0xff;
			info->reg_data[1] = (data>>8)&0xff;
			dev_dbg(info->dev,
				"%s: (read) addr = 0x%x, data = 0x%02x%02x\n",
				 __func__, info->reg_addr,
				 info->reg_data[1], info->reg_data[0]);
			ret = count;
		}
		break;
	case FG_DATA:
		if (sscanf(buf, "%x\n", &x) == 1) {
			info->reg_data[0] = (x & 0xFF00) >> 8;
			info->reg_data[1] = (x & 0x00FF);
			dev_dbg(info->dev,
				"%s: (write) addr = 0x%x, data = 0x%02x%02x\n",
				__func__, info->reg_addr, info->reg_data[1], info->reg_data[0]);

			rt5033_fg_i2c_write_word(info,
				info->reg_addr, x);
			ret = count;
		}
		break;
		*/
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}




