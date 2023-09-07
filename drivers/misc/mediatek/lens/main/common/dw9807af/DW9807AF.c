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
 * DW9807AF voice coil motor driver
 *
 *
 */

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/fs.h>

#include "lens_info.h"
#ifdef CONFIG_CAMERA_HW_BIG_DATA
#include "hwparam.h"
#endif

#define AF_DRVNAME "DW9807AF_DRV"
#define AF_I2C_SLAVE_ADDR        0x18


#define AF_DEBUG
#ifdef AF_DEBUG
#define LOG_INF(format, args...) pr_debug(AF_DRVNAME " [%s] " format, __func__, ##args)
#else
#define LOG_INF(format, args...)
#endif


static struct i2c_client *g_pstAF_I2Cclient;
static int *g_pAF_Opened;
static spinlock_t *g_pAF_SpinLock;


static unsigned long g_u4AF_INF;
static unsigned long g_u4AF_MACRO = 1023;
static unsigned long g_u4TargetPosition;
static unsigned long g_u4CurrPosition;

static int g_LensSleep = 3000;
static int g_LensDAC1  = 300;
static int g_LensStep1 = 75;
static int g_LensStep2 = 15;
static int g_LensDAC2  = 100;

static int s4DW9807AF_ReadReg(unsigned short *a_pu2Result)
{
	int i4RetValue = 0;
	char cReadCmdMsb = 0x03;
	char cBuffMsb = 0;
	char cReadCmdLsb =  0x04;
	char cBuffLsb =  0;

	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, &cReadCmdMsb, 1);
	if (i4RetValue != 1) {
		LOG_INF(" I2C write failed!!\n");
		return -1;
	}

	i4RetValue = i2c_master_recv(g_pstAF_I2Cclient, (char *)&cBuffMsb, 1);
	if (i4RetValue != 1) {
		LOG_INF("Msb I2C read failed!!\n");
		return -1;
	}
	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, &cReadCmdLsb, 1);
	if (i4RetValue != 1) {
		LOG_INF(" I2C write failed!!\n");
		return -1;
	}

	i4RetValue = i2c_master_recv(g_pstAF_I2Cclient, (char *)&cBuffLsb, 1);
	if (i4RetValue != 1) {
		LOG_INF("Lsb I2C read failed!!\n");
		return -1;
	}
	*a_pu2Result = (((cBuffMsb & 0x3)  <<  8)  | (cBuffLsb));

	return 0;
}

static int s4AF_WriteReg(u16 a_u2Data)
{
	int i4RetValue = 0;

	char puSendCmdMsb[2] = { 0x03, (char)((a_u2Data >> 8)&0x3) };
	char puSendCmdLsb[2] = { 0x04,  (char)(a_u2Data & 0xFF) };


	g_pstAF_I2Cclient->addr = AF_I2C_SLAVE_ADDR;

	g_pstAF_I2Cclient->addr = g_pstAF_I2Cclient->addr >> 1;

	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puSendCmdMsb, 2);

	if (i4RetValue < 0) {
		LOG_INF("I2C send failed!!\n");
		return -1;
	}


	g_pstAF_I2Cclient->addr = AF_I2C_SLAVE_ADDR;

	g_pstAF_I2Cclient->addr = g_pstAF_I2Cclient->addr >> 1;

	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puSendCmdLsb, 2);

	if (i4RetValue < 0) {
		LOG_INF("I2C send failed!!\n");
		return -1;
	}

	return 0;
}

static inline int getAFInfo(__user struct stAF_MotorInfo  *pstMotorInfo)
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

	if (copy_to_user(pstMotorInfo, &stMotorInfo, sizeof(struct stAF_MotorInfo)))
		LOG_INF("copy to user failed when getting motor information\n");

	return 0;
}

static int initdrv(void)
{
	char puSendCmd2[2] = { 0x02, 0x02 };  /*ring control mode*/
	char puSendCmd3[2] = { 0x06, 0x61 };  /*ring control mode*/
	char puSendCmd4[2] = { 0x07, 0x36 };  /*ring control mode*/
	int i4RetValue = 0;

	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puSendCmd2, 2);
	if (i4RetValue < 0) {
		LOG_INF("I2C send failed!!\n");
		return -1;
	} else if (i4RetValue != 2) {
		LOG_INF("I2C send failed!! %d bytes transferred (expected 2)\n", i4RetValue);
		return -1;
	}

    i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puSendCmd3, 2);
	if (i4RetValue < 0) {
		LOG_INF("I2C send failed!!\n");
		return -1;
	} else if (i4RetValue != 2) {
		LOG_INF("I2C send failed!! %d bytes transferred (expected 2)\n", i4RetValue);
		return -1;
	}

    i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puSendCmd4, 2);
	if (i4RetValue < 0) {
		LOG_INF("I2C send failed!!\n");
		return -1;
	} else if (i4RetValue != 2) {
		LOG_INF("I2C send failed!! %d bytes transferred (expected 2)\n", i4RetValue);
		return -1;
	}

	return 0;
}

static inline int moveAF(unsigned long a_u4Position)
{
	int ret = 0;
#ifdef CONFIG_CAMERA_HW_BIG_DATA
	struct cam_hw_param *hw_param = NULL;
#endif

	if ((a_u4Position > g_u4AF_MACRO) || (a_u4Position < g_u4AF_INF)) {
		LOG_INF("out of range\n");
		return -EINVAL;
	}

	if (*g_pAF_Opened == 1) {
		unsigned short InitPos;

#ifdef CONFIG_CAMERA_HW_BIG_DATA
		if (initdrv() < 0) {
			is_sec_get_rear_hw_param(&hw_param);

			if (hw_param)
				hw_param->i2c_af_err_cnt++;			
		}
#else
		initdrv();
#endif
		ret = s4DW9807AF_ReadReg(&InitPos);

		if (ret == 0) {
			LOG_INF("Init Pos %6d\n", InitPos);

			spin_lock(g_pAF_SpinLock);
			g_u4CurrPosition = (unsigned long)InitPos;
			spin_unlock(g_pAF_SpinLock);

		} else {
			spin_lock(g_pAF_SpinLock);
			g_u4CurrPosition = 0;
			spin_unlock(g_pAF_SpinLock);
		}

		spin_lock(g_pAF_SpinLock);
		*g_pAF_Opened = 2;
		spin_unlock(g_pAF_SpinLock);
	}

	if (g_u4CurrPosition == a_u4Position)
		return 0;

	spin_lock(g_pAF_SpinLock);
	g_u4TargetPosition = a_u4Position;
	spin_unlock(g_pAF_SpinLock);

	 /*LOG_INF("move [curr] %ld [target] %ld\n", g_u4CurrPosition, g_u4TargetPosition); */


	if (s4AF_WriteReg((unsigned short)g_u4TargetPosition) == 0) {
		spin_lock(g_pAF_SpinLock);
		g_u4CurrPosition = (unsigned long)g_u4TargetPosition;
		spin_unlock(g_pAF_SpinLock);
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

static inline int setAFPara(__user struct stAF_MotorCmd *pstMotorCmd)
{
	struct stAF_MotorCmd stMotorCmd;

	if (copy_from_user(&stMotorCmd, pstMotorCmd, sizeof(stMotorCmd)))
		LOG_INF("copy to user failed when getting motor command\n");

	LOG_INF("Motor CmdID : %x\n", stMotorCmd.u4CmdID);

	LOG_INF("Motor Param : %x\n", stMotorCmd.u4Param);

	if (stMotorCmd.u4Param > 0) {
		switch (stMotorCmd.u4CmdID) {
		case 3:
			g_LensSleep = stMotorCmd.u4Param;
			break;
		case 4:
			g_LensStep1 = stMotorCmd.u4Param;
			break;
		case 5:
			g_LensDAC1 = stMotorCmd.u4Param;
			break;
		case 6:
			g_LensStep2 = stMotorCmd.u4Param;
			break;
		case 7:
			g_LensDAC2 = stMotorCmd.u4Param;
			break;
		}
	}

	return 0;
}

/* ////////////////////////////////////////////////////////////// */
long DW9807AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command, unsigned long a_u4Param)
{
	long i4RetValue = 0;

	switch (a_u4Command) {
	case AFIOC_G_MOTORINFO:
		i4RetValue = getAFInfo((__user struct stAF_MotorInfo *) (a_u4Param));
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
		i4RetValue = setAFPara((__user struct stAF_MotorCmd *) (a_u4Param));
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
int DW9807AF_Release(struct inode *a_pstInode, struct file *a_pstFile)
{
	int LensSleepU = g_LensSleep + 500;

	LOG_INF("Start\n");

	if (*g_pAF_Opened == 2) {

		short i;
		unsigned short TargetPosition;

		TargetPosition = (unsigned short)g_u4TargetPosition;

		LOG_INF("Wait\n");

		LOG_INF("g_LensSleep(%d)\n", g_LensSleep);
		LOG_INF("g_LensDAC1(%d)\n",  g_LensDAC1);
		LOG_INF("g_LensStep1(%d)\n", g_LensStep1);
		LOG_INF("g_LensDAC2(%d)\n",  g_LensDAC2);
		LOG_INF("g_LensStep2(%d)\n", g_LensStep2);

		for (i = TargetPosition; i >= g_LensDAC1 ; i -= g_LensStep1) {
			s4AF_WriteReg(i);
			usleep_range(g_LensSleep, LensSleepU);
		}

		for (i = g_LensDAC1 ; i >= g_LensDAC2 ; i -= g_LensStep2) {
			s4AF_WriteReg(i);
			usleep_range(g_LensSleep, LensSleepU);
		}
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

int DW9807AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient, spinlock_t *pAF_SpinLock, int *pAF_Opened)
{
	LOG_INF("DW9807AF_SetI2Cclient DONE\n");
	g_pstAF_I2Cclient = pstAF_I2Cclient;
	g_pAF_SpinLock = pAF_SpinLock;
	g_pAF_Opened = pAF_Opened;

	g_LensSleep = 3000;
	g_LensDAC1  = 300;
	g_LensStep1 = 75;
	g_LensStep2 = 15;
	g_LensDAC2  = 100;

	return 1;
}
