/*
 * Copyright (C) 2021 Samsung Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/*
 * DW9818AF voice coil motor driver
 *
 *
 */
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>

#include "lens_info.h"
#include "imgsensor_sysfs.h"
#include "kd_imgsensor_sysfs_adapter.h"

#define AF_DRVNAME "DW9818AF_DRV"
#define AF_I2C_SLAVE_ADDR 0x18

#define AF_DEBUG
#ifdef AF_DEBUG
#define LOG_INF(format, args...)                                               \
	pr_debug(AF_DRVNAME " [%s] " format, __func__, ##args)
#else
#define LOG_INF(format, args...)
#endif

static struct i2c_client *g_pstAF_I2Cclient;
static int *g_pAF_Opened;
static spinlock_t *g_pAF_SpinLock;

static unsigned long g_u4AF_INF;
static unsigned long g_u4AF_MACRO = 1023;
static unsigned long g_u4CurrPosition;

#define REG_IC_INFO     0x00 // Default: 0xEE
#define REG_CONTROL     0x02 // Default: 0x00, W, [0] = PD(Power Down mode)
#define REG_VCM_MSB     0x03 // Default: 0x00, R/W, [1:0] = Pos[9:8]
#define REG_VCM_LSB     0x04 // Default: 0x00, R/W, [7:0] = Pos[7:0]
#define REG_MODE        0x06 // Default: 0x00, R/W, [3] = RING [1:0] = (SAC1~0)
#define REG_PRESCALE    0x07 // Default: 0x03, R/W, [1:0] = PRESC
#define REG_SACTIMING   0x08 // Default: 0x7F, R/W, [6:0] = SACT
#define REG_SLD_MSB   0x0A // Default: 0x00, R/W, [9:8] = Soft landing Target Code
#define REG_SLD_LSB   0x0B // Default: 0x00, R/W, [7:0] = Soft landing Target Code
#define REG_SLD_EN   0x0C // Default: 0x00, R/W, [0] = Soft landing Enable
#define REG_SLD_TIME   0x0D // Default: 0x85, R/W, [7:0] = Soft landing Time

static int i2c_read(u8 a_u2Addr, u8 *a_puBuff)
{
	int i4RetValue = 0;
	char puReadCmd[1] = {(char)(a_u2Addr)};

	g_pstAF_I2Cclient->addr = AF_I2C_SLAVE_ADDR;

	g_pstAF_I2Cclient->addr = g_pstAF_I2Cclient->addr >> 1;

	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puReadCmd, 1);
	if (i4RetValue < 0) {
		LOG_INF(" I2C write failed!!\n");
		return -1;
	}

	i4RetValue = i2c_master_recv(g_pstAF_I2Cclient, (char *)a_puBuff, 1);
	if (i4RetValue < 0) {
		LOG_INF(" I2C read failed!!\n");
		return -1;
	}

	return 0;
}

static u8 read_data(u8 addr)
{
	u8 get_byte = 0xFF;

	i2c_read(addr, &get_byte);

	return get_byte;
}

static int s4AF_WriteReg(u8 a_u2Addr, u8 a_u2Data)
{
	int i4RetValue = 0;
	char puSendCmd[2] = {(char)a_u2Addr, (char)a_u2Data};

	g_pstAF_I2Cclient->addr = AF_I2C_SLAVE_ADDR;

	g_pstAF_I2Cclient->addr = g_pstAF_I2Cclient->addr >> 1;

	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 2);

	if (i4RetValue < 0) {
		LOG_INF("I2C send failed!!\n");
		return -1;
	}

	return 0;
}

static inline int getAFInfo(__user struct stAF_MotorInfo *pstMotorInfo)
{
	struct stAF_MotorInfo stMotorInfo;

	stMotorInfo.u4MacroPosition = g_u4AF_MACRO;
	stMotorInfo.u4InfPosition = g_u4AF_INF;
	stMotorInfo.u4CurrentPosition = g_u4CurrPosition;
	stMotorInfo.bIsSupportSR = 1;

	stMotorInfo.bIsMotorMoving = 1;

	if (*g_pAF_Opened >= 1)
		stMotorInfo.bIsMotorOpen = 1;
	else
		stMotorInfo.bIsMotorOpen = 0;

	if (copy_to_user(pstMotorInfo, &stMotorInfo,
			 sizeof(struct stAF_MotorInfo)))
		LOG_INF("copy to user failed when getting motor information\n");

	return 0;
}

/* initAF include driver initialization and standby mode */
static int initAF(void)
{
	int ret = 0;
	u8 i2c_data[2];
	u8 data = 0xFF;
	u8 sac_mode = 0x00, sac_time = 0x7F;

	LOG_INF("+\n");
	
	data = read_data(REG_IC_INFO);
	LOG_INF("module id:%d\n", data);
	
	/* PD(Power Down) mode enable */
	i2c_data[0] = REG_CONTROL;
	i2c_data[1] = 0x01;
	ret |= s4AF_WriteReg(i2c_data[0], i2c_data[1]);
	
	/* PD disable(normal operation) */
	i2c_data[0] = REG_CONTROL;
	i2c_data[1] = 0x00;
	ret |= s4AF_WriteReg(i2c_data[0], i2c_data[1]);

	usleep_range(5000, 5500);

 	if (!IMGSENSOR_GET_SAC_VALUE_BY_SENSOR_IDX(0, &sac_mode, &sac_time)) {
		ret |= -1;
		pr_err("[%s] DW9818 : failed to get sac value\n", __func__);
	}	
	
	/* Ring mode enable & SAC write
	 * RING mode [3] SAC[1:0] setting */
	i2c_data[0] = REG_MODE;
	i2c_data[1] = ((sac_mode >> 5) | 0x08);
	ret |= s4AF_WriteReg(i2c_data[0], i2c_data[1]);
	
	/*
	 * PRESC[1:0] mode setting
	 */
	i2c_data[0] = REG_PRESCALE;
	i2c_data[1] = (sac_mode & 0x07);
	ret |= s4AF_WriteReg(i2c_data[0], i2c_data[1]);
	
	/*
	 * SAC TIMING[6:0] mode setting
	 */
	i2c_data[0] = REG_SACTIMING;
	i2c_data[1] = sac_time;
	ret |= s4AF_WriteReg(i2c_data[0], i2c_data[1]);

	usleep_range(5000, 5500);
	

	if (*g_pAF_Opened == 1) {

		spin_lock(g_pAF_SpinLock);
		*g_pAF_Opened = 2;
		spin_unlock(g_pAF_SpinLock);
	}

	LOG_INF("-\n");

	return 0;
}

/* moveAF only use to control moving the motor */
static inline int moveAF(unsigned long a_u4Position)
{
	int ret = 0;

	if (s4AF_WriteReg(REG_VCM_MSB, (unsigned short)(a_u4Position>>8)) == 0 &&
	s4AF_WriteReg(REG_VCM_LSB, (unsigned short)(a_u4Position&0xff)) == 0) {
		g_u4CurrPosition = a_u4Position;
		ret = 0;
	} else {
		LOG_INF("set I2C failed when moving the motor\n");
		ret = -1;
	}

	return ret;
}

static inline int setAFInf(unsigned long a_u4Position)
{
	spin_lock(g_pAF_SpinLock);
	g_u4AF_INF = a_u4Position;
	spin_unlock(g_pAF_SpinLock);
	return 0;
}

static inline int setAFMacro(unsigned long a_u4Position)
{
	spin_lock(g_pAF_SpinLock);
	g_u4AF_MACRO = a_u4Position;
	spin_unlock(g_pAF_SpinLock);
	return 0;
}

/* ////////////////////////////////////////////////////////////// */
long DW9818AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
		    unsigned long a_u4Param)
{
	long i4RetValue = 0;

	switch (a_u4Command) {
	case AFIOC_G_MOTORINFO:
		i4RetValue =
			getAFInfo((__user struct stAF_MotorInfo *)(a_u4Param));
		break;

	case AFIOC_T_MOVETO:
		i4RetValue = moveAF(a_u4Param);
		break;

	case AFIOC_T_SETINFPOS:
		i4RetValue = setAFInf(a_u4Param);
		break;

	case AFIOC_T_SETMACROPOS:
		i4RetValue = setAFMacro(a_u4Param);
		break;

	default:
		LOG_INF("No CMD\n");
		i4RetValue = -EPERM;
		break;
	}

	return i4RetValue;
}

/* Main jobs: */
/* 1.Deallocate anything that "open" allocated in private_data. */
/* 2.Shut down the device on last close. */
/* 3.Only called once on last time. */
/* Q1 : Try release multiple times. */
int DW9818AF_Release(struct inode *a_pstInode, struct file *a_pstFile)
{
	LOG_INF("Start\n");

	if (*g_pAF_Opened == 2) {
		LOG_INF("Wait\n");

		/* Set SLD(Soft-landing) Time 
			: Step period = (50 + LAND_STEP) us (decimal) */
		s4AF_WriteReg(REG_SLD_TIME, 0xB3); // SLD Time : 179us
		usleep_range(2000, 2100);

		/* Set SLD Target Data */
		s4AF_WriteReg(REG_SLD_MSB, 0x02); // SLD Target Code : 512 code
		s4AF_WriteReg(REG_SLD_LSB, 0x00);

		/* Set SLD_EN */
		s4AF_WriteReg(REG_SLD_EN, 0x01); // Soft landing Enable
		usleep_range(2000, 2100);
	}

	if (*g_pAF_Opened) {
		LOG_INF("Free\n");

		spin_lock(g_pAF_SpinLock);
		*g_pAF_Opened = 0;
		spin_unlock(g_pAF_SpinLock);
	}

	LOG_INF("End\n");

	return 0;
}

int DW9818AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
			  spinlock_t *pAF_SpinLock, int *pAF_Opened)
{
	g_pstAF_I2Cclient = pstAF_I2Cclient;
	g_pAF_SpinLock = pAF_SpinLock;
	g_pAF_Opened = pAF_Opened;

	initAF();

	return 1;
}

int DW9818AF_GetFileName(unsigned char *pFileName)
{
	#if SUPPORT_GETTING_LENS_FOLDER_NAME
	char FilePath[256];
	char *FileString;

	sprintf(FilePath, "%s", __FILE__);
	FileString = strrchr(FilePath, '/');
	*FileString = '\0';
	FileString = (strrchr(FilePath, '/') + 1);
	strncpy(pFileName, FileString, AF_MOTOR_NAME);
	LOG_INF("FileName : %s\n", pFileName);
	#else
	pFileName[0] = '\0';
	#endif
	return 1;
}
