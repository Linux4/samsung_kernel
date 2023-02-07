// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
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

	u8 data = 0xFF;

	LOG_INF("+\n");

	mdelay(10);

	data = read_data(0x00);
	LOG_INF("module id:%d\n", data);
	s4AF_WriteReg(0x02, 0x01);
	s4AF_WriteReg(0x02, 0x00);
	mdelay(5);
	s4AF_WriteReg(0x02, 0x02);//ring
	//s4AF_WriteReg(0x06, 0xA4);//101__100 sac4 with x8
	s4AF_WriteReg(0x06, 0xA2);//101__011 sac4 with /2
	mdelay(5);
	s4AF_WriteReg(0x07, 0x3f);//00111111 SACT
	//s4AF_WriteReg(0x07, 0x00);//00111111 SACT
	mdelay(1);
	s4AF_WriteReg(0x0A, 0x00);
	s4AF_WriteReg(0x0B, 0x01);
	s4AF_WriteReg(0x0C, 0xFF);
	s4AF_WriteReg(0x11, 0x00);

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
				mdelay(2);
				Position = upperSoundPos;
				LOG_INF("write to upperSoundPos:%d\n",
				upperSoundPos);
			} else if (Position > lowerSoundPos && Position
				<= upperSoundPos) {
				Position -= (Position % afSteps2);
				if (Position == upperSoundPos)
					Position -= afSteps2;

				moveAF(Position);
			while (read_data(0x05) != 0x00)
				mdelay(2);
				LOG_INF("write to Position:%d\n", Position);

				if (Position != lowerSoundPos)
					Position -= afSteps2;

			} else {
				Position -= (Position % afSteps1);
				moveAF(Position);
			while (read_data(0x05) != 0x00)
				mdelay(2);
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
