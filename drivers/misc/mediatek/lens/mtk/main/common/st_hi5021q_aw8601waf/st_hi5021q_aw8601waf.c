/*
 * Copyright (c) 2018 AWINIC Technology CO., LTD
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
 * AW8601WAF voice coil motor driver
 */
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/printk.h>

#include "lens_info.h"
#include "st_hi5021q_aw8601waf.h"

#define AW8601_DRIVER_VERSION	"v1.2.0"
#define AF_I2C_SLAVE_ADDR	0x18

static struct i2c_client *g_pstAF_I2Cclient;
static int *g_pAF_Opened;
static spinlock_t *g_pAF_SpinLock;

static unsigned long g_u4AF_INF;
static unsigned long g_u4AF_MACRO = 1023;
static unsigned long g_u4TargetPosition;
static unsigned long g_u4CurrPosition;

static int s4AW8601WAF_ReadReg(unsigned char a_uAddr, unsigned char *a_puData,
							unsigned int data_len)
{
	int ret = 0;
	struct i2c_msg msg[2];

	g_pstAF_I2Cclient->addr = (AF_I2C_SLAVE_ADDR >> 1);

	msg[0].addr = g_pstAF_I2Cclient->addr;
	msg[0].flags = 0;
	msg[0].len = sizeof(unsigned char);
	msg[0].buf = &a_uAddr;

	msg[1].addr = g_pstAF_I2Cclient->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = data_len;
	msg[1].buf = a_puData;

	ret = i2c_transfer(g_pstAF_I2Cclient->adapter, msg, ARRAY_SIZE(msg));
	if (ret < 0) {
		AWLOGE("i2c reads transfer error.");
		return -EAGAIN;
	} else if (ret != AW_I2C_READ_MSG_NUM) {
		AWLOGE("i2c reads transfer error(size error).");
		return -EAGAIN;
	}

	return AW_SUCCESS;
}

static int s4AF_WriteReg(u16 a_u2Data)
{
	int i4RetValue = 0;
	int loop = AW_EOORHANDLE_LOOP;

	char puSendCmd[3] = { 0x03, (char)(a_u2Data >> 8),
						(char)(a_u2Data & 0xFF)};

	g_pstAF_I2Cclient->addr = (AF_I2C_SLAVE_ADDR >> 1);

	do {
		i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 3);
		if (i4RetValue < 0)
			AWLOGE("I2C send failed!!.");
		else
			break;
	} while (loop--);

	if ((loop == 0) && (i4RetValue < 0))
		return AW_ERROR;

	return AW_SUCCESS;
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
		AWLOGE("copy to user failed when getting motor information.");

	return AW_SUCCESS;
}

static int initdrv(void)
{
	int i4RetValue = 0;
	char puSendCmdArray[7][2] = {
	{0x02, 0x01}, {0x02, 0x00}, {0xFE, 0xFE},
	{0x02, 0x02}, {0x06, 0x40}, {0x07, 0x08}, {0xFE, 0xFE},
	};
	unsigned char cmd_number;

	for (cmd_number = 0; cmd_number < 7; cmd_number++) {
		if (puSendCmdArray[cmd_number][0] != 0xFE) {
			i4RetValue = i2c_master_send(g_pstAF_I2Cclient,
						puSendCmdArray[cmd_number], 2);
			if (i4RetValue < 0)
				return AW_ERROR;
		} else {
			udelay(100);
		}
	}

	AWLOGI("Init drv OK.");

	return i4RetValue;
}

static inline int moveAF(unsigned long a_u4Position)
{
	int ret = 0;

	if ((a_u4Position > g_u4AF_MACRO) || (a_u4Position < g_u4AF_INF)) {
		AWLOGE("Target position out of range.");
		return -EINVAL;
	}

	if (*g_pAF_Opened == 1) {
		unsigned short InitPos;
		unsigned char addr = AW_REG_VCM_MSB;
		unsigned char pos[2] = { 0 };
		int loop = AW_EOORHANDLE_LOOP;

		do {
			ret = initdrv();
			if (ret < 0)
				AWLOGE("Init drv error.");
			else
				break;
		} while (loop--);

		if ((loop == 0) && (ret < 0))
			return AW_ERROR;

		ret = s4AW8601WAF_ReadReg(addr, pos, 2);
		if (ret == 0) {
			InitPos = (unsigned short)((pos[0] << 8) | pos[1]);
			AWLOGD("Init Pos 0x%04x.", InitPos);

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

	AWLOGD("Target position: 0x%04lx.", g_u4TargetPosition);

	if (s4AF_WriteReg((unsigned short)g_u4TargetPosition) == AW_SUCCESS) {
		spin_lock(g_pAF_SpinLock);
		g_u4CurrPosition = (unsigned long)g_u4TargetPosition;
		spin_unlock(g_pAF_SpinLock);
	} else {
		AWLOGE("set I2C failed when moving the motor.");
		ret = AW_ERROR;
	}

	return ret;
}

static inline int setAFInf(unsigned long a_u4Param)
{
	if (a_u4Param > AW_LIMITPOS_MAX) {
		AWLOGE("a_u4Param out of range.");
		return -EINVAL;
	}

	spin_lock(g_pAF_SpinLock);
	g_u4AF_INF = a_u4Param;
	spin_unlock(g_pAF_SpinLock);
	AWLOGD("g_u4AF_INF = 0x%04lx.", g_u4AF_INF);

	return AW_SUCCESS;
}

static inline int setAFMacro(unsigned long a_u4Param)
{
	if (a_u4Param > AW_LIMITPOS_MAX) {
		AWLOGE("a_u4Param out of range.");
		return -EINVAL;
	}

	spin_lock(g_pAF_SpinLock);
	g_u4AF_MACRO = a_u4Param;
	spin_unlock(g_pAF_SpinLock);
	AWLOGD("g_u4AF_MACRO = 0x%04lx.", g_u4AF_MACRO);

	return AW_SUCCESS;
}

long ST_HI5021Q_AW8601WAF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
							unsigned long a_u4Param)
{
	long i4RetValue = 0;

	switch (a_u4Command) {
	case AFIOC_G_MOTORINFO:
		i4RetValue = getAFInfo(
				(__user struct stAF_MotorInfo *) (a_u4Param));
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
		AWLOGE("No CMD.");
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
int ST_HI5021Q_AW8601WAF_Release(struct inode *a_pstInode, struct file *a_pstFile)
{
	AWLOGI("Start.");

	if (*g_pAF_Opened == 2)
		AWLOGI("Wait.");

	if (*g_pAF_Opened) {
		AWLOGI("Free.");

		spin_lock(g_pAF_SpinLock);
		*g_pAF_Opened = 0;
		spin_unlock(g_pAF_SpinLock);
	}

	AWLOGI("End.");

	return AW_SUCCESS;
}

int ST_HI5021Q_AW8601WAF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				spinlock_t *pAF_SpinLock, int *pAF_Opened)
{
	AWLOGI("driver version %s.", AW8601_DRIVER_VERSION);
	g_pstAF_I2Cclient = pstAF_I2Cclient;
	g_pAF_SpinLock = pAF_SpinLock;
	g_pAF_Opened = pAF_Opened;

	return 1;
}

int ST_HI5021Q_AW8601WAF_GetFileName(unsigned char *pFileName)
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

