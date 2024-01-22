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
 * GT9778AF voice coil motor driver
 *
 *
 */

#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>

#include "lens_info.h"

#include "kd_imgsensor_sysfs_adapter.h"

#define AF_DRVNAME "GT9778AF_DRV"
#define AF_I2C_SLAVE_ADDR 0x18

#define AF_DEBUG
#ifdef AF_DEBUG
#define LOG_INF(format, args...)                                               \
	pr_info(AF_DRVNAME " [%s] " format, __func__, ##args)
#else
#define LOG_INF(format, args...)
#endif

#define LOG_ERR(format, args...)                                               \
	pr_err(AF_DRVNAME " [%s] " format, __func__, ##args)


static struct i2c_client *g_pstAF_I2Cclient;
static int *g_pAF_Opened;
static spinlock_t *g_pAF_SpinLock;

static unsigned long g_u4AF_INF;
static unsigned long g_u4AF_MACRO = 1023;
static unsigned long g_u4CurrPosition;

static int s4AF_ReadReg(u8 a_uAddr, u16 *a_pu2Result, int size)
{
	int i4RetValue = 0;
	char pBuff[2];
	char puSendCmd[1];

	puSendCmd[0] = a_uAddr;

	g_pstAF_I2Cclient->addr = AF_I2C_SLAVE_ADDR;

	g_pstAF_I2Cclient->addr = g_pstAF_I2Cclient->addr >> 1;

	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 1);

	if (i4RetValue < 0) {
		LOG_INF("I2C read - send failed!!\n");
		return -1;
	}

	i4RetValue = i2c_master_recv(g_pstAF_I2Cclient, pBuff, size);

	if (i4RetValue < 0) {
		LOG_INF("I2C read - recv failed!!\n");
		return -1;
	}

	if (size == 2)
		*a_pu2Result = (pBuff[0] << 8) | pBuff[1];
	else
		*a_pu2Result = pBuff[0];

	return 0;
}

static int s4AF_WriteReg_8(u8 a_uAddr, u8 a_uData)
{
	int i4RetValue = 0;

	char puSendCmd[2] = {(char)a_uAddr, (char)(a_uData & 0xFF)};

	g_pstAF_I2Cclient->addr = AF_I2C_SLAVE_ADDR;

	g_pstAF_I2Cclient->addr = g_pstAF_I2Cclient->addr >> 1;

	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 2);

	if (i4RetValue < 0) {
		LOG_INF("I2C write failed!!\n");
		return -1;
	}

	return 0;
}

static int s4AF_WriteReg(u8 a_uAddr, u16 a_u2Data)
{
	int i4RetValue = 0;

	char puSendCmd[3] = {(char)a_uAddr, (char)((a_u2Data >> 8) & 0xFF), (char)(a_u2Data & 0xFF)};

	g_pstAF_I2Cclient->addr = AF_I2C_SLAVE_ADDR;

	g_pstAF_I2Cclient->addr = g_pstAF_I2Cclient->addr >> 1;

	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 3);

	if (i4RetValue < 0) {
		LOG_INF("I2C write failed!!\n");
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

static inline int setVCMPos(unsigned long a_u4Position)
{
	int i4RetValue = 0;

	i4RetValue = s4AF_WriteReg(0x3, (u16)(a_u4Position & 0x03FF));

	return i4RetValue;
}

/* moveAF only use to control moving the motor */
static inline int moveAF(unsigned long a_u4Position)
{
	int ret = 0;

	if (setVCMPos(a_u4Position) == 0) {
		g_u4CurrPosition = a_u4Position;
		ret = 0;
	} else {
		LOG_INF("set I2C failed when moving the motor\n");
		ret = -1;
	}

	return ret;
}

/* initAF include driver initialization and standby mode */
static int initAF(void)
{
	int ret = 0;
	unsigned char sac_mode = 0x05, sac_timing = 0x6E;

	LOG_INF("+\n");

	if (*g_pAF_Opened == 1) {

		unsigned short icInfo;

		s4AF_ReadReg(0x00, &icInfo, 1);  //ic info
		LOG_INF("IC model is 0x%x\n", icInfo);

		if (!IMGSENSOR_GET_SAC_VALUE_BY_SENSOR_IDX(0, &sac_mode, &sac_timing)) {
			ret |= -1;
			LOG_ERR("failed to get sac value from ROM:\n");
			LOG_INF("SAC setting (default) - sac_mode: 0x%x, sac_timing: 0x%x\n", sac_mode, sac_timing);
		} else
			LOG_INF("SAC setting (read from ROM) - sac_mode: 0x%x, sac_timing: 0x%x\n",
				sac_mode, sac_timing);

		ret = s4AF_WriteReg_8(0x06, sac_mode);
		if (ret < 0)
			LOG_ERR("fail to set sac_mode\n");

		ret = s4AF_WriteReg_8(0x07, sac_timing);
		if (ret < 0)
			LOG_ERR("fail to set sac_timing\n");

		// Power down disable and ringing set
		ret = s4AF_WriteReg_8(0x02, 0x02);
		if (ret < 0)
			LOG_ERR("fail to disable power down and set ringing\n");

		usleep_range(1000, 1100);

		moveAF(512);

		usleep_range(9000, 10100);

		spin_lock(g_pAF_SpinLock);
		*g_pAF_Opened = 2;
		spin_unlock(g_pAF_SpinLock);
	}

#ifdef IMGSENSOR_HW_PARAM
	if (ret)
		imgsensor_increase_hw_param_af_err_cnt(SENSOR_POSITION_REAR);
#endif

	LOG_INF("-\n");

	return 0;
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

static inline int setAFPara(__user struct stAF_MotorCmd *pstMotorCmd)
{
	struct stAF_MotorCmd stMotorCmd;
	int ret = -EOPNOTSUPP;

	if (copy_from_user(&stMotorCmd, pstMotorCmd, sizeof(stMotorCmd)))
		LOG_INF("copy to user failed when getting motor command\n");

	LOG_INF("Motor CmdID : %x\n", stMotorCmd.u4CmdID);

	LOG_INF("Motor Param : %x\n", stMotorCmd.u4Param);

	switch (stMotorCmd.u4CmdID) {
	default:
		LOG_INF("default cmd\n");
		break;
	}

	return ret;
}

/* ////////////////////////////////////////////////////////////// */
long GT9778AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
		    unsigned long a_u4Param)
{
	long i4RetValue = 0;

	LOG_INF("DW9825AF E %d %lu\n", a_u4Command, a_u4Param);
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

	case AFIOC_S_SETPARA:
		i4RetValue =
			setAFPara((__user struct stAF_MotorCmd *)(a_u4Param));
		break;
	default:
		LOG_INF("No CMD\n");
		i4RetValue = -EPERM;
		break;
	}

	return i4RetValue;
}

void GT9778AF_softlanding(void)
{
	unsigned short recvData = 0;
	int lensPosition = 512;
	int interval = 50;
	int intervalCount = 3;
	int i = 0;
	int maxDelayCount = 5;
	int delayCount = 0;
	int delayTime = 2000;

	// Lens Position Check
	// s4AF_ReadReg(0x03, &recvData, 2);
	// lensPosition = recvData;

	lensPosition = g_u4CurrPosition;
	if (lensPosition < g_u4AF_INF)
		lensPosition = g_u4AF_INF;

	if (lensPosition > g_u4AF_MACRO)
		lensPosition = g_u4AF_MACRO;

	interval = (512 - lensPosition) / intervalCount;
	LOG_INF("lens position: %d, interval: %d\n", lensPosition, interval);

	for (i = 0; i < intervalCount; i++)	{
		lensPosition += interval;

		moveAF(lensPosition);

		do {
			usleep_range(delayTime, delayTime + 100);
			s4AF_ReadReg(0x05, &recvData, 1);
			if ((++delayCount) >= maxDelayCount)
				break;
		} while (recvData == 0x01);

		LOG_INF("lens position: %d, lens status: 0x%4x, delayCount %d, delay time: %d\n",
			lensPosition, recvData, delayCount, delayTime * delayCount);

		delayCount = 0;
	}
}

/* Main jobs: */
/* 1.Deallocate anything that "open" allocated in private_data. */
/* 2.Shut down the device on last close. */
/* 3.Only called once on last time. */
/* Q1 : Try release multiple times. */
int GT9778AF_Release(struct inode *a_pstInode, struct file *a_pstFile)
{
	LOG_INF("Start\n");

	if (*g_pAF_Opened == 2)
		LOG_INF("Wait\n");

	if (*g_pAF_Opened) {
		LOG_INF("Free\n");

		spin_lock(g_pAF_SpinLock);
		*g_pAF_Opened = 0;
		spin_unlock(g_pAF_SpinLock);

		GT9778AF_softlanding();
	}

	LOG_INF("End\n");

	return 0;
}

int GT9778AF_PowerDown(struct i2c_client *pstAF_I2Cclient,
			int *pAF_Opened)
{
	g_pstAF_I2Cclient = pstAF_I2Cclient;
	g_pAF_Opened = pAF_Opened;

	LOG_INF("+\n");
	if (*g_pAF_Opened == 0)
		LOG_INF("Set power down\n");

	LOG_INF("-\n");

	return 0;
}

int GT9778AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
			  spinlock_t *pAF_SpinLock, int *pAF_Opened)
{
	g_pstAF_I2Cclient = pstAF_I2Cclient;
	g_pAF_SpinLock = pAF_SpinLock;
	g_pAF_Opened = pAF_Opened;

	initAF();

	return 1;
}

int GT9778AF_GetFileName(unsigned char *pFileName)
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

