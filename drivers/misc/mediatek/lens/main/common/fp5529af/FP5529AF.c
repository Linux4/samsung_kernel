/*
 * Copyright (C) 2015 MediaTek Inc.
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
 * FP5529AF voice coil motor driver
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

#define AF_DRVNAME "FP5529AF_DRV"
#define AF_I2C_SLAVE_ADDR 0x18

#define AF_DEBUG
#ifdef AF_DEBUG
#define LOG_INF(format, args...)                                               \
	pr_info(AF_DRVNAME " [%s] " format, __func__, ##args)
#else
#define LOG_INF(format, args...)
#endif

static struct i2c_client *g_pstAF_I2Cclient;
static int *g_pAF_Opened;
static spinlock_t *g_pAF_SpinLock;

static unsigned long g_u4AF_INF;
static unsigned long g_u4AF_MACRO = 1023;
static unsigned long g_u4CurrPosition;




#if 0
static int s4AF_ReadReg(unsigned short *a_pu2Result)
{
	int i4RetValue = 0;
	char pBuff[2];

	g_pstAF_I2Cclient->addr = AF_I2C_SLAVE_ADDR;

	g_pstAF_I2Cclient->addr = g_pstAF_I2Cclient->addr >> 1;

	i4RetValue = i2c_master_recv(g_pstAF_I2Cclient, pBuff, 2);

	if (i4RetValue < 0) {
		LOG_INF("I2C read failed!!\n");
		return -1;
	}

	*a_pu2Result = (((u16)pBuff[0]) << 4) + (pBuff[1] >> 4);

	return 0;
}
#endif

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


/* initAF include driver initialization and standby mode */
static int initAF(void)
{
	int ret = 0;
#ifdef IMGSENSOR_HW_PARAM
	struct cam_hw_param *hw_param = NULL;
#endif
	u8 data = 0xFF;
	u8 ac_mode = 0xA2, ac_time = 0x3F;

	LOG_INF("+\n");

	// reduce delay to optimize entry time: 9900 -> 5000
	// reference document: FP5529-Preliminary 0.1-JUL-2018.pdf
	usleep_range(5000, 5500);
	data = read_data(0x00);
	LOG_INF("module id:%d\n", data);
	ret |= s4AF_WriteReg(0x02, 0x01);
	ret |= s4AF_WriteReg(0x02, 0x00);
	usleep_range(5000, 5500);
	ret |= s4AF_WriteReg(0x02, 0x02);//ring

	if (!IMGSENSOR_GET_SAC_VALUE_BY_SENSOR_IDX(0, &ac_mode, &ac_time)) {
		ret |= -1;
		pr_err("[%s] FP5529: failed to get sac value\n", __func__);
	}

	pr_info("[%s] FP5529 SAC setting (read from eeprom) - ac_mode: 0x%x, ac_time: 0x%x\n", __func__, ac_mode, ac_time);
	s4AF_WriteReg(0x06, ac_mode);
	// remove delay: usleep_range(4900, 5000);
	s4AF_WriteReg(0x07, ac_time);

	// remove delay: usleep_range(900, 1000);
	ret |= s4AF_WriteReg(0x0A, 0x00);
	ret |= s4AF_WriteReg(0x0B, 0x01);
	ret |= s4AF_WriteReg(0x0C, 0xFF);
	ret |= s4AF_WriteReg(0x11, 0x00);

	if (*g_pAF_Opened == 1) {

		spin_lock(g_pAF_SpinLock);
		*g_pAF_Opened = 2;
		spin_unlock(g_pAF_SpinLock);

	}
#ifdef IMGSENSOR_HW_PARAM
	if (ret != 0) {
		imgsensor_sec_get_hw_param(&hw_param, SENSOR_POSITION_REAR);
		if (hw_param)
			hw_param->i2c_af_err_cnt++;
	}
#endif
	LOG_INF("-\n");

	return 0;
}

/* moveAF only use to control moving the motor */
static inline int moveAF(unsigned long a_u4Position)
{
	int ret = 0;

	if (s4AF_WriteReg(0x03, (unsigned short)(a_u4Position>>8)) == 0 &&
	s4AF_WriteReg(0x04, (unsigned short)(a_u4Position&0xff)) == 0) {
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
long FP5529AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
		    unsigned long a_u4Param)
{
	long i4RetValue = 0;


	LOG_INF("a_u4Command %lu\n", a_u4Param);

	switch (a_u4Command) {
	case AFIOC_G_MOTORINFO:
		i4RetValue =
			getAFInfo((__user struct stAF_MotorInfo *)(a_u4Param));
		break;

	case AFIOC_T_MOVETO:
	LOG_INF("AFIOC_T_MOVETO %lu\n", a_u4Param);

		i4RetValue = moveAF(a_u4Param);
		break;

	case AFIOC_T_SETINFPOS:
	LOG_INF("AFIOC_T_SETINFPOS %lu\n", a_u4Param);

		i4RetValue = setAFInf(a_u4Param);
		break;

	case AFIOC_T_SETMACROPOS:
	LOG_INF("AFIOC_T_SETMACROPOS %lu\n", a_u4Param);

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
int FP5529AF_Release(struct inode *a_pstInode, struct file *a_pstFile)
{
	int upperSoundPos = 300;
	int lowerSoundPos = 200;
	int afSteps1 = 100;
	int afSteps2 = 10;

	LOG_INF("Start\n");

	if (*g_pAF_Opened == 2) {
		LOG_INF("Wait\n");
	//	s4AF_WriteReg(0x80); /* Power down mode */
	}

	if (*g_pAF_Opened) {
		LOG_INF("Free\n");

	if (g_u4AF_INF < g_u4CurrPosition) {
		int Position = g_u4CurrPosition;

		LOG_INF("g_u4CurrPosition:%lu  g_u4AF_INF:%lu\n",
		g_u4CurrPosition, g_u4AF_INF);

		while (Position > g_u4AF_INF) {
			if (Position > upperSoundPos) {
				moveAF(upperSoundPos);
				usleep_range(1900, 2000);
				Position = upperSoundPos;
				LOG_INF("write to upperSoundPos:%d\n",
				upperSoundPos);
			} else if (Position > lowerSoundPos && Position
				<= upperSoundPos) {
				Position -= (Position % afSteps2);
				if (Position == upperSoundPos)
					Position -= afSteps2;

				moveAF(Position);
				usleep_range(7900, 8000);
				LOG_INF("write to Position:%d\n", Position);

				if (Position != lowerSoundPos)
					Position -= afSteps2;

			} else {
				Position -= (Position % afSteps1);
				moveAF(Position);
				usleep_range(7900, 8000);
				LOG_INF("write to Position:%d\n", Position);
				Position -= afSteps1;
				if (Position < 0)
					break;

			}
		}
	}
		spin_lock(g_pAF_SpinLock);
		*g_pAF_Opened = 0;
		spin_unlock(g_pAF_SpinLock);
	}

	LOG_INF("End\n");

	return 0;
}

int FP5529AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
			  spinlock_t *pAF_SpinLock, int *pAF_Opened)
{
	g_pstAF_I2Cclient = pstAF_I2Cclient;
	g_pAF_SpinLock = pAF_SpinLock;
	g_pAF_Opened = pAF_Opened;

	initAF();

	return 1;
}

int FP5529AF_GetFileName(unsigned char *pFileName)
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
