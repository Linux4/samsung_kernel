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
 * Driver for CAM_CAL
 *
 *
 */

#if 0
/*#include <linux/i2c.h>*/
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/fs.h>
/*#include "kd_camera_hw.h"*/
#include "eeprom.h"
#include "eeprom_define.h"
#include "CAT24s128c.h"
#include <asm/system.h>  /* for SMP */
#else

#ifndef CONFIG_MTK_I2C_EXTENSION
#define CONFIG_MTK_I2C_EXTENSION
#endif
#include <linux/i2c.h>
#undef CONFIG_MTK_I2C_EXTENSION
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include "kd_camera_hw.h"
#include "cam_cal.h"
#include "cam_cal_define.h"
#include <linux/delay.h>
#include "CAT24s128c.h"
/*#include <asm/system.h>//for SMP*/
#endif
/* #define EEPROMGETDLT_DEBUG */
#define EEPROM_DEBUG
#ifdef EEPROM_DEBUG
#define EEPROMDB pr_debug
#else
#define EEPROMDB(x, ...)
#endif

/*#define CAT24s128c_DRIVER_ON 0*/
#ifdef CAT24s128c_DRIVER_ON
#define EEPROM_I2C_BUSNUM 1
static struct i2c_board_info kd_eeprom_dev __initdata = { I2C_BOARD_INFO("CAM_CAL_DRV", 0xAA >> 1)};

/*******************************************************************************
*
********************************************************************************/
#define EEPROM_ICS_REVISION 1 /* seanlin111208 */
/*******************************************************************************
*
********************************************************************************/
#define EEPROM_DRVNAME "CAM_CAL_DRV"
#define EEPROM_I2C_GROUP_ID 0

/*******************************************************************************
*
********************************************************************************/
/* #define FM50AF_EEPROM_I2C_ID 0x28 */
#define FM50AF_EEPROM_I2C_ID 0xA1


/*******************************************************************************
* define LSC data for M24C08F EEPROM on L10 project
********************************************************************************/
#define SampleNum 221
#define Boundary_Address 256
#define EEPROM_Address_Offset 0xC
#define EEPROM_DEV_MAJOR_NUMBER 226

/* EEPROM READ/WRITE ID */
#define S24CS64A_DEVICE_ID				0xAA



/*******************************************************************************
*
********************************************************************************/


/* 81 is used for V4L driver */
static dev_t g_EEPROMdevno = MKDEV(EEPROM_DEV_MAJOR_NUMBER, 0);
static struct cdev *g_pEEPROM_CharDrv;

static struct class *EEPROM_class;
static atomic_t g_EEPROMatomic;
/* static DEFINE_SPINLOCK(kdeeprom_drv_lock); */
/* spin_lock(&kdeeprom_drv_lock); */
/* spin_unlock(&kdeeprom_drv_lock); */
#endif/*CAT24s128c_DRIVER_ON*/

#define Read_NUMofEEPROM 2
static struct i2c_client *g_pstI2Cclient;

/*static DEFINE_SPINLOCK(g_EEPROMLock);  for SMP */


/*******************************************************************************
*
********************************************************************************/






/*******************************************************************************
*
********************************************************************************/
/* maximun read length is limited at "I2C_FIFO_SIZE" in I2c-mt6516.c which is 8 bytes */
#if 0
static int iWriteEEPROM(u16 a_u2Addr, u32 a_u4Bytes, u8 *puDataInBytes)
{
	u32 u4Index;
	int i4RetValue;
	char puSendCmd[8] = {
		(char)(a_u2Addr >> 8),
		(char)(a_u2Addr & 0xFF),
		0, 0, 0, 0, 0, 0
	};
	if (a_u4Bytes + 2 > 8) {
		EEPROMDB("[CAT24s128c] exceed I2c-mt65xx.c 8 bytes limitation (include address 2 Byte)\n");
		return -1;
	}

	for (u4Index = 0; u4Index < a_u4Bytes; u4Index += 1)
		puSendCmd[(u4Index + 2)] = puDataInBytes[u4Index];

	i4RetValue = i2c_master_send(g_pstI2Cclient, puSendCmd, (a_u4Bytes + 2));
	if (i4RetValue != (a_u4Bytes + 2)) {
		EEPROMDB("[CAT24s128c] I2C write  failed!!\n");
		return -1;
	}
	mdelay(10); /* for tWR singnal --> write data form buffer to memory. */

	/* EEPROMDB("[EEPROM] iWriteEEPROM done!!\n"); */
	return 0;
}
#endif

/* maximun read length is limited at "I2C_FIFO_SIZE" in I2c-mt65xx.c which is 8 bytes */
static int iReadEEPROM(u16 a_u2Addr, u32 ui4_length, u8 *a_puBuff)
{
	int  i4RetValue = 0;
	char puReadCmd[2] = {(char)(a_u2Addr >> 8), (char)(a_u2Addr & 0xFF)};
	struct i2c_msg msg[2];

	/* EEPROMDB("[EEPROM] iReadEEPROM!!\n"); */

	if (ui4_length > 8) {
		EEPROMDB("[CAT24s128c] exceed I2c-mt65xx.c 8 bytes limitation\n");
		return -1;
	}
#ifdef CONFIG_MTK_I2C_EXTENSION
	spin_lock(&g_EEPROMLock); /* for SMP */
	g_pstI2Cclient->addr = g_pstI2Cclient->addr & (I2C_MASK_FLAG | I2C_WR_FLAG);
	spin_unlock(&g_EEPROMLock); /* for SMP */
#endif
	msg[0].addr = g_pstI2Cclient->addr;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = puReadCmd;
	msg[1].addr = g_pstI2Cclient->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = ui4_length;
	msg[1].buf = a_puBuff;
	i4RetValue = i2c_transfer(g_pstI2Cclient->adapter, msg, 2);
	if (i4RetValue != 2) {
		EEPROMDB("[CAT24s128c] I2C read data failed!!\n");
		return -1;
	}
#ifdef CONFIG_MTK_I2C_EXTENSION
	spin_lock(&g_EEPROMLock); /* for SMP */
	g_pstI2Cclient->addr = g_pstI2Cclient->addr & I2C_MASK_FLAG;
	spin_unlock(&g_EEPROMLock); /* for SMP */
#endif
	/* EEPROMDB("[CAT24s128c] iReadEEPROM done!!\n"); */
	return 0;
}

static int iReadData(unsigned int  ui4_offset, unsigned int  ui4_length, unsigned char *pinputdata)
{
	int  i4RetValue = 0;
	int  i4ResidueDataLength;
	u32 u4IncOffset = 0;
	u32 u4CurrentOffset;
	u8 *pBuff;
	/* EEPROMDB("[S24EEPORM] iReadData\n" ); */

	if (ui4_offset + ui4_length >= 0x2000) {
		EEPROMDB("[CAT24s128c] Read Error!! CAT24s128c not supprt address >= 0x2000!!\n");
		return -1;
	}

	i4ResidueDataLength = (int)ui4_length;
	u4CurrentOffset = ui4_offset;
	pBuff = pinputdata;
	do {
		if (i4ResidueDataLength >= 8) {
			i4RetValue = iReadEEPROM((u16)u4CurrentOffset, 8, pBuff);
			if (i4RetValue != 0) {
				EEPROMDB("[CAT24s128c] I2C iReadData failed!!\n");
				return -1;
			}
			u4IncOffset += 8;
			i4ResidueDataLength -= 8;
			u4CurrentOffset = ui4_offset + u4IncOffset;
			pBuff = pinputdata + u4IncOffset;
		} else {
			i4RetValue = iReadEEPROM((u16)u4CurrentOffset, i4ResidueDataLength, pBuff);
			if (i4RetValue != 0) {
				EEPROMDB("[CAT24s128c] I2C iReadData failed!!\n");
				return -1;
			}
			u4IncOffset += 8;
			i4ResidueDataLength -= 8;
			u4CurrentOffset = ui4_offset + u4IncOffset;
			pBuff = pinputdata + u4IncOffset;
			/* break; */
		}
	} while (i4ResidueDataLength > 0);




	return 0;
}



#if defined(titan6757_jade_o)
/*******************************************************************************
* for sysfs
********************************************************************************/
#define SYSFSLOG(fmt, arg...) pr_err("[%s] " fmt, __func__, ##arg)
#define OTP_RELOAD 0

// common type and function
typedef enum {
	TYPE_STRING,
	TYPE_UINT,
	TYPE_MODULEID,
	TYPE_DUALCAL,
	TYPE_TILTCAL,
} t_loop_datatype;

typedef struct {
	unsigned int offset;
	unsigned int size;
	u8 *data;
	u8 *dataname;
	t_loop_datatype datatype;
} t_loop_data;

static void sprintf_sysfsdata(char* buff, unsigned int bufmaxsize, t_loop_datatype datatype, u8 *data) {
	if(TYPE_STRING == datatype) {
		if(bufmaxsize-1 >= strlen(data))
			snprintf(buff, bufmaxsize, "%s", (u8*)data);
	}
	else if(TYPE_UINT == datatype) {
		if(bufmaxsize-1 >= 10) // strlen("4294967295") == 10
			snprintf(buff, bufmaxsize, "%u", *(unsigned int*)data);
	}
	else if(TYPE_MODULEID == datatype) {
		if(bufmaxsize-1 >= 15) {
			if(strlen(data) >= 5) { // %c should be not null, or log stops.
				snprintf(buff, bufmaxsize, "%c%c%c%c%c%02x%02x%02x%02x%02x",
					data[0], data[1], data[2], data[3], data[4],
					data[5], data[6], data[7], data[8], data[9]);
			} else {
				snprintf(buff, bufmaxsize, "-ERR-%02x%02x%02x%02x%02x",
					data[5], data[6], data[7], data[8], data[9]);
			}
		}
	}
	else if(TYPE_DUALCAL == datatype) {
		// "2*OTP_DUAL_CAL_BUFF_LEN > OTP_MAX_DATA_STRING_LEN_IN_LOG" is assumed.
		int i;
		for(i=0; bufmaxsize>3 ; ++i) {
			snprintf(buff, bufmaxsize, "%02X", *data);
			buff+=2; data++; bufmaxsize-=2;
			if(((i+1)%10) == 0) {
				snprintf(buff, bufmaxsize, " ");
				buff++; bufmaxsize--;
			}
		}
	}
	else if(TYPE_TILTCAL == datatype) {
		int *data_i = (int *)data;		
		snprintf(buff, bufmaxsize, "%d %d %d %d %d %d %d %d", 
						data_i[0], data_i[1], data_i[2], data_i[3],data_i[7], data_i[8], data_i[9], data_i[10]);
	}
};

#define OTP_MFR_INFO_LEN 11
#define OTP_MODULEID_LEN 10
#define OTP_AF_CAL_LEN 4
#define OTP_DUAL_CAL_READ_LEN 56
#define OTP_DUAL_CAL_BUFF_LEN 512
#define OTP_MAX_DATA_STRING_LEN_IN_LOG 200
#define OTP_MAX_DATA_TILT_CAL (32+12) //16bytes 20cm + 12bytes space + 16bytes 60cm
#define OTP_DATA_TILT_CAL_EACH_SIZE 16 //20cm or 60cm
#define OTP_DATA_TILT_60CM_OFFSET (16+12) //between 20cm and 60cm


// rear and front sysfs data
typedef struct {
	u8 mfr_info[OTP_MFR_INFO_LEN+1]; // manufacturer_information
	unsigned int af_macro_cal;
	unsigned int af_pan_cal;
	u8 mfr_info2[OTP_MFR_INFO_LEN+1]; // manufacturer_information
	unsigned int af_macro_cal2;
	unsigned int af_pan_cal2;
	u8 module_id[OTP_MODULEID_LEN+1];
	u8 dual_cal_data[OTP_DUAL_CAL_BUFF_LEN+1];
	unsigned int dual_cal_size;
	u8 tilt_cal_data[OTP_MAX_DATA_TILT_CAL];
} t_rear_otp_info;

static t_rear_otp_info rear_otp_info = {
	.af_macro_cal = -1,
	.af_pan_cal = -1,
	.dual_cal_data = {0},
	.dual_cal_size = OTP_DUAL_CAL_BUFF_LEN,
};

static t_loop_data rear_loop_data[] = {
	{0x30, OTP_MFR_INFO_LEN, rear_otp_info.mfr_info, "manufacturer_information", TYPE_STRING},
	{0x40, OTP_MFR_INFO_LEN, rear_otp_info.mfr_info2, "manufacturer_information2", TYPE_STRING},
	{0x10C, OTP_AF_CAL_LEN, (u8 *)&rear_otp_info.af_macro_cal, "af_macro_cal", TYPE_UINT},
	{0x114, OTP_AF_CAL_LEN, (u8 *)&rear_otp_info.af_pan_cal, "af_pan_cal", TYPE_UINT},
	{0xAE, OTP_MODULEID_LEN, rear_otp_info.module_id, "module_id", TYPE_MODULEID},
	{0x1E18, OTP_DUAL_CAL_READ_LEN, rear_otp_info.dual_cal_data, "dual_cal_data", TYPE_DUALCAL},
	{0x1E18, OTP_MAX_DATA_TILT_CAL, rear_otp_info.tilt_cal_data, "tilt_cal_data", TYPE_TILTCAL},
};
static unsigned int rear_loop_len = sizeof(rear_loop_data)/sizeof(rear_loop_data[0]);

#if defined(titan6757_jade_o)
static int update_cam_eeprom_crc32(char *eeprom_name);
#endif

// update function
static int update_cam_sysfs(char *eeprom_name, t_loop_data *loop_data, unsigned int loop_len)
{
	unsigned int i;
#if defined(titan6757_jade_o)
	update_cam_eeprom_crc32(eeprom_name);
#endif
	for(i=0; i<loop_len; ++i) {
		int ret = 0;
		t_loop_data* it = &loop_data[i];
		char data_string_to_print[OTP_MAX_DATA_STRING_LEN_IN_LOG] = {0};
		ret = iReadData(it->offset, it->size, it->data);
		sprintf_sysfsdata(data_string_to_print, OTP_MAX_DATA_STRING_LEN_IN_LOG, it->datatype, it->data);
		SYSFSLOG("(%s) %s to init %s = addr[0x%X],size[%d],val[%s]\n", eeprom_name, ((ret!=0)?"failed":"passed"),
			it->dataname, it->offset, it->size, data_string_to_print);
		if ( ret != 0 )
			return 1;
	}

	return 0;
}

static int sysfs_updated = 0;

#if 0
extern int g_is_imx258_open;
#endif
static int is_camera_sensor_opened(void)
{
#if 0
	// In J7Max, EEPROM CAT24s128c can be accessed during imx258 sensor is opened.
	return g_is_imx258_open;
#else
	// In other projects, it's not certain which sensor should be checked.
	return 1;
#endif
}

// interface functions
void cat24s128c_update_sysfs_information(void)
{
	int isErr = 0;
	if(!OTP_RELOAD && sysfs_updated) {
		return;
	}
	isErr = update_cam_sysfs("CAT24s128c", rear_loop_data, rear_loop_len);
	if(!isErr)
		sysfs_updated = 1;
}

#if defined(titan6757_jade_o)
// For EEPROM crc32
/////////////////////////////
//#define USE_TABLE_CREATE
#ifdef USE_TABLE_CREATE
static unsigned long table[256];
#else
static unsigned long table[256] = {
	0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
	0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988, 0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91,
	0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
	0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5,
	0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172, 0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B,
	0x35B5A8FA, 0x42B2986C, 0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
	0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
	0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924, 0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D,
	0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
	0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01,
	0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E, 0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457,
	0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
	0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB,
	0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0, 0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9,
	0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
	0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81, 0xB7BD5C3B, 0xC0BA6CAD,
	0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A, 0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683,
	0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
	0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7,
	0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC, 0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5,
	0xD6D6A3E8, 0xA1D1937E, 0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
	0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55, 0x316E8EEF, 0x4669BE79,
	0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236, 0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F,
	0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
	0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713,
	0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38, 0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21,
	0x86D3D2D4, 0xF1D4E242, 0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
	0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45,
	0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2, 0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB,
	0xAED16A4A, 0xD9D65ADC, 0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
	0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693, 0x54DE5729, 0x23D967BF,
	0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94, 0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};
#endif

static void makeCRCtable(unsigned long *table, unsigned long id)
{
#ifdef USE_TABLE_CREATE
	unsigned long i, j, k;

	for(i = 0; i < 256; ++i) {
		k = i;

		for(j = 0; j < 8; ++j) {
			if(k & 1)
				k = (k >> 1) ^ id;
			else
				k >>= 1;
		}

		table[i] = k;
	}
#endif
}

static unsigned long getCRC(volatile unsigned short *mem, signed long size,
	volatile unsigned short *crcH, volatile unsigned short *crcL)
{
	unsigned char mem0, mem1;
	long i;
	signed long count;
	unsigned long CRC = 0;

	/* duration : under 1 ms */
	makeCRCtable(table, 0xEDB88320);
	count = size/2;

	CRC = ~CRC;
	for(i = 0; i < count; i++) {
		mem0 = (unsigned char)(mem[i] & 0x00ff);
		mem1 = (unsigned char)((mem[i] >> 8) & 0x00ff);

		CRC = table[(CRC ^ (mem0)) & 0xFF] ^ ((CRC >> 8) & 0x00ffffff);
		CRC = table[(CRC ^ (mem1)) & 0xFF] ^ ((CRC >> 8) & 0x00ffffff);
	}
	if (size % 2 != 0) {
		mem0 = (unsigned char)(mem[i] & 0x00ff);
		CRC = table[(CRC ^ (mem0)) & 0xFF] ^ ((CRC >> 8) & 0x00ffffff);
	}
	CRC = ~CRC;

	/*
	 * high 2 byte of crc32
	 * g_CAL[0page 31 addr.] or g_CAL[127page 31 addr.]
	 */
	if(crcH)
		*crcH = (unsigned short)((CRC >> 16)&(0x0000ffff));
	/*
	 * low 2 byte of crc32
	 * g_CAL[0page 30 addr.] or g_CAL[127page 30 addr.]
	 */
	if(crcL)
		*crcL = (unsigned short)((CRC) & (0x0000ffff));

	return CRC;
}
/////////////////////////////
typedef struct {
	u32 start;
	u32 end;
	u32 off_crc32;
} t_crc_info;
static t_crc_info rear_EEPROM_info[] = {// all: 0x00~0x2BFF=0x2C00
	{  0x00,   0xD7,   0xFC}, // header:    0x00  ~0xD7  =0xD8
	{ 0x100,  0x19F,  0x1FC}, // oem:       0x100 ~0x19F =0xA0
	{ 0x200,  0x22F,  0x2FC}, // awbaf:     0x200 ~0x22F =0x30
	{ 0x300,  0xAFF,  0xDFC}, // shade1:    0x300 ~0xAFF =0x800
	{ 0xE00, 0x15FF, 0x18FC}, // shade2:    0xE00 ~0x15FF=0x800
	//{0x1900, 0x1F3F, 0x1FFC}, // dualdepth: 0x1900~0x1F3F=0x640
//	{0x2000, 0x287F, 0x2BFC}, // pdaf:      0x2000~0x287F=0x880
//	{0xFFFF, 0xFFFF, 0xFFFF},
};
static u8 EEPROM_data[0x2C00];
static int check_crc32(u8* data, u32 len, u32 crc32_offset)
{
	u32 checksum_data = (data[crc32_offset+3]<<24) | (data[crc32_offset+2]<<16) | (data[crc32_offset+1]<<8) | (data[crc32_offset]);
	u32 checksum = (u32)getCRC((u16 *)data, len, NULL, NULL);
//	u32 checksum = (u32)crc32(0, data, len);
	SYSFSLOG("checksum=0x%X, data=0x%X, res=%d\n", checksum, checksum_data, (checksum != checksum_data));
	if(checksum != checksum_data)
		return -1;
	return 0;
}

static int flag_Cal_data_correct = 0xff;

// http://mobilerndhub.sec.samsung.net/OG/OG02_6110/s?defs=fimc_is_ois_crc_check
// // Platform / N / NILE / STRAWBERRY / EXYNOS8895
// reference: http://mobilerndhub.sec.samsung.net/OG/OG02_6110/xref/android/kernel/exynos8895/drivers/media/platform/exynos/fimc-is2/vendor/mcd/fimc-is-device-ois_common.c#538
// /android/kernel/include/linux/crc32.h
static int update_cam_eeprom_crc32(char *eeprom_name)
{
	int ret = 0;
	int i, nSections=sizeof(rear_EEPROM_info)/sizeof(t_crc_info);
#if 0
	int EEPROM_size=0x2C00;
	ret = iReadData(0, EEPROM_size, EEPROM_data);
	SYSFSLOG("(%s) read all done (ret=%d)\n", eeprom_name, ret);
#else
	for(i=0; i<nSections; ++i) {
		t_crc_info* info=&rear_EEPROM_info[i];
		int tret = iReadData(info->start, info->end-info->start+1, EEPROM_data+info->start);
		SYSFSLOG("(%s) %d th section read done (ret=%d)\n", eeprom_name, i, tret);
		ret |= tret;
		tret = iReadData(info->off_crc32, 4, EEPROM_data+info->off_crc32);
		SYSFSLOG("(%s) %d th CRC32 read done (ret=%d)\n", eeprom_name, i, tret);
		ret |= tret;
	}
#endif
	if ( ret == 0 ) {
		flag_Cal_data_correct = 0x1;
		for(i=0; i<nSections; ++i) {
			t_crc_info* info=&rear_EEPROM_info[i];
			int check_err = check_crc32(&EEPROM_data[info->start], info->end-info->start+1, info->off_crc32-info->start);
			if( check_err ) {
				SYSFSLOG("CRC error occurred in Section %d\n", i);
				flag_Cal_data_correct = 0xff;
				break;
			}
		}
	}
	return ret;
}

/*implement by customer  0x01:correct 0xff:fail*/
int Is_Cal_data_correct_main(void)
{
	int ret = 0x01;
	char manfac_id = rear_otp_info.mfr_info[OTP_MFR_INFO_LEN - 1];
	if((manfac_id != 'B' ) && (manfac_id != 'M' ))
		return 0xFF;		
	SYSFSLOG("return %d (but flag_Cal_data_correct = 0x%X)\n", ret, flag_Cal_data_correct);
	//return ret;
	return flag_Cal_data_correct;
}
#endif

void get_rear_mfr_info(u8 *data)
{
	if(sysfs_updated == 0)
		SYSFSLOG("(CAT24s128c) error: all sysfs wasn't updated, mfr info = [%s]\n", rear_otp_info.mfr_info);
	memcpy((void *)data, (void *)rear_otp_info.mfr_info, OTP_MFR_INFO_LEN);
	data[OTP_MFR_INFO_LEN] = '\0';
}
void get_rear_mfr_info2(u8 *data)
{
	if(sysfs_updated == 0)
		SYSFSLOG("(CAT24s128c) error: all sysfs wasn't updated, mfr info2 = [%s]\n", rear_otp_info.mfr_info2);
	memcpy((void *)data, (void *)rear_otp_info.mfr_info2, OTP_MFR_INFO_LEN);
	data[OTP_MFR_INFO_LEN] = '\0';
}
unsigned int get_rear_af_macro_cal(void)
{
	if(sysfs_updated == 0)
		SYSFSLOG("(CAT24s128c) error: all sysfs wasn't updated, af_macro_cal = [%d]\n", rear_otp_info.af_macro_cal);
	return rear_otp_info.af_macro_cal;
}
unsigned int get_rear_af_pan_cal(void)
{
	if(sysfs_updated == 0)
		SYSFSLOG("(CAT24s128c) error: all sysfs wasn't updated, af_pan_cal = [%d]\n", rear_otp_info.af_pan_cal);
	return rear_otp_info.af_pan_cal;
}
void get_rear_moduleid(u8 *data)
{
	if(sysfs_updated == 0)
		SYSFSLOG("(CAT24s128c) error: all sysfs wasn't updated, moduleid = [%s]\n", rear_otp_info.module_id);
	memcpy((void *)data, (void *)rear_otp_info.module_id, OTP_MODULEID_LEN);
	data[OTP_MODULEID_LEN] = '\0';
}
void get_rear_dual_cal_data(u8 *data)
{
	if(sysfs_updated == 0)
		SYSFSLOG("(CAT24s128c) error: all sysfs wasn't updated, dual cal data which size %d\n", OTP_DUAL_CAL_BUFF_LEN);
	memcpy((void *)data, (void *)rear_otp_info.dual_cal_data, OTP_DUAL_CAL_BUFF_LEN);
	data[OTP_DUAL_CAL_BUFF_LEN] = '\0';
}
unsigned int get_rear_dual_cal_size(void)
{
	return OTP_DUAL_CAL_BUFF_LEN;
}
void get_rear_tilt_cal_data(u8 *data)
{
	memcpy(data, rear_otp_info.tilt_cal_data, OTP_DATA_TILT_CAL_EACH_SIZE); //20cm
	memcpy(data+OTP_DATA_TILT_CAL_EACH_SIZE, rear_otp_info.tilt_cal_data+OTP_DATA_TILT_60CM_OFFSET, OTP_DATA_TILT_CAL_EACH_SIZE); //60cm
	
}

#undef SYSFSLOG
///////////////////////////////////////////////////////////////////////
#endif


unsigned int cat24s128c_selective_read_region(struct i2c_client *client, unsigned int addr,
	unsigned char *data, unsigned int size)
{
#if defined(titan6757_jade_o)
	int retry = 50;
#endif

	EEPROMDB("[CAT24s128c] Start Read\n");
	g_pstI2Cclient = client;

#if defined(titan6757_jade_o)
	// EEPROM Cat24s128c can be accessed during the sensor is opened.
	while(is_camera_sensor_opened() == 0) {
		if(retry <= 0) {
			pr_err("[CAT24s128c] sel_read sensor is not ready. but no more wait\n");
			break;
		}
		msleep(10);
		pr_err("[CAT24s128c] sel_read sensor is not ready. wait 10ms retry=%d\n", retry);
		retry--;
	}
	cat24s128c_update_sysfs_information();
#endif

	if (iReadData(addr, size, data) == 0) {
		EEPROMDB("[CAT24s128c] Read success\n");
		return size;
	} else {
		EEPROMDB("[CAT24s128c] Read failed\n");
		return 0;
	}
}




/*#define CAT24s128c_DRIVER_ON 0*/
#ifdef CAT24s128c_DRIVER_ON

/*******************************************************************************
*
********************************************************************************/
#define NEW_UNLOCK_IOCTL
#ifndef NEW_UNLOCK_IOCTL
static int EEPROM_Ioctl(struct inode *a_pstInode,
			struct file *a_pstFile,
			unsigned int a_u4Command,
			unsigned long a_u4Param)
#else
static long EEPROM_Ioctl(
	struct file *file,
	unsigned int a_u4Command,
	unsigned long a_u4Param
)
#endif
{
	int i4RetValue = 0;
	u8 *pBuff = NULL;
	u8 *pWorkingBuff = NULL;
	stCAM_CAL_INFO_STRUCT *ptempbuf;
	ssize_t writeSize;
	u8 readTryagain = 0;

#ifdef EEPROMGETDLT_DEBUG
	struct timeval ktv1, ktv2;
	unsigned long TimeIntervalUS;
#endif

#if defined(titan6757_jade_o)
	int retry = 50;
	while(is_camera_sensor_opened() == 0) {
		if(retry <= 0) {
			pr_err("[CAT24s128c] sel_read sensor is not ready. but no more wait\n");
			break;
		}
		msleep(10);
		pr_err("[CAT24s128c] ioctl sensor is not ready. wait 10ms retry=%d\n", retry);
		retry--;
	}
#endif

	if (_IOC_DIR(a_u4Command) != _IOC_NONE) {
		pBuff = kmalloc(sizeof(stCAM_CAL_INFO_STRUCT), GFP_KERNEL);

		if (pBuff == NULL) {
			EEPROMDB("[CAT24s128c] ioctl allocate mem failed\n");
			return -ENOMEM;
		}

		if (_IOC_WRITE & _IOC_DIR(a_u4Command)) {
			if (copy_from_user((u8 *) pBuff, (u8 *) a_u4Param, sizeof(stCAM_CAL_INFO_STRUCT))) {
				/* get input structure address */
				kfree(pBuff);
				EEPROMDB("[CAT24s128c] ioctl copy from user failed\n");
				return -EFAULT;
			}
		}
	}

	ptempbuf = (stCAM_CAL_INFO_STRUCT *)pBuff;
	pWorkingBuff = kmalloc(ptempbuf->u4Length, GFP_KERNEL);
	if (pWorkingBuff == NULL) {
		kfree(pBuff);
		EEPROMDB("[CAT24s128c] ioctl allocate mem failed\n");
		return -ENOMEM;
	}
	EEPROMDB("[CAT24s128c] init Working buffer address 0x%8x  command is 0x%8x\n", (u32)pWorkingBuff,
	(u32)a_u4Command);

	if (copy_from_user((u8 *)pWorkingBuff, (u8 *)ptempbuf->pu1Params, ptempbuf->u4Length)) {
		kfree(pBuff);
		kfree(pWorkingBuff);
		EEPROMDB("[CAT24s128c] ioctl copy from user failed\n");
		return -EFAULT;
	}

	switch (a_u4Command) {
	case CAM_CALIOC_S_WRITE:
		EEPROMDB("[CAT24s128c] Write CMD\n");
#ifdef EEPROMGETDLT_DEBUG
		do_gettimeofday(&ktv1);
#endif
		i4RetValue = iWriteData((u16)ptempbuf->u4Offset, ptempbuf->u4Length, pWorkingBuff);
#ifdef EEPROMGETDLT_DEBUG
		do_gettimeofday(&ktv2);
		if (ktv2.tv_sec > ktv1.tv_sec)
			TimeIntervalUS = ktv1.tv_usec + 1000000 - ktv2.tv_usec;
		else
			TimeIntervalUS = ktv2.tv_usec - ktv1.tv_usec;

		EEPROMDB("Write data %d bytes take %lu us\n", ptempbuf->u4Length, TimeIntervalUS);
#endif
		break;
	case CAM_CALIOC_G_READ:
		EEPROMDB("[CAT24s128c] Read CMD\n");
#ifdef EEPROMGETDLT_DEBUG
		do_gettimeofday(&ktv1);
#endif
		EEPROMDB("[CAT24s128c] offset %x\n", ptempbuf->u4Offset);
		EEPROMDB("[CAT24s128c] length %x\n", ptempbuf->u4Length);
		EEPROMDB("[CAT24s128c] Before read Working buffer address 0x%8x\n", (u32)pWorkingBuff);


		if (ptempbuf->u4Offset == 0x0024C32a) {
			*(u32 *)pWorkingBuff = 0x0124C32a;
		} else {
			readTryagain = 3;
			while (readTryagain > 0) {
				i4RetValue =  iReadDataFromCAT24s128c((u16)ptempbuf->u4Offset, ptempbuf->u4Length,
				pWorkingBuff);
				EEPROMDB("[CAT24s128c] error (%d) Read retry (%d)\n", i4RetValue, readTryagain);
				if (i4RetValue != 0)
					readTryagain--;
				else
					readTryagain = 0;
			}


		}
		EEPROMDB("[CAT24s128c] After read Working buffer data  0x%4x\n", *pWorkingBuff);


#ifdef EEPROMGETDLT_DEBUG
		do_gettimeofday(&ktv2);
		if (ktv2.tv_sec > ktv1.tv_sec)
			TimeIntervalUS = ktv1.tv_usec + 1000000 - ktv2.tv_usec;
		else
			TimeIntervalUS = ktv2.tv_usec - ktv1.tv_usec;

		EEPROMDB("Read data %d bytes take %lu us\n", ptempbuf->u4Length, TimeIntervalUS);
#endif

		break;
	default:
		EEPROMDB("[CAT24s128c] No CMD\n");
		i4RetValue = -EPERM;
		break;
	}

	if (_IOC_READ & _IOC_DIR(a_u4Command)) {
		/* copy data to user space buffer, keep other input paremeter unchange. */
		EEPROMDB("[CAT24s128c] to user length %d\n", ptempbuf->u4Length);
		EEPROMDB("[CAT24s128c] to user  Working buffer address 0x%8x\n", (u32)pWorkingBuff);
		if (copy_to_user((u8 __user *) ptempbuf->pu1Params, (u8 *)pWorkingBuff,
		ptempbuf->u4Length)) {
			kfree(pBuff);
			kfree(pWorkingBuff);
			EEPROMDB("[CAT24s128c] ioctl copy to user failed\n");
			return -EFAULT;
		}
	}

	kfree(pBuff);
	kfree(pWorkingBuff);
	return i4RetValue;
}


static u32 g_u4Opened;
/* #define */
/* Main jobs: */
/* 1.check for device-specified errors, device not ready. */
/* 2.Initialize the device if it is opened for the first time. */
static int EEPROM_Open(struct inode *a_pstInode, struct file *a_pstFile)
{
	int ret = 0;

	EEPROMDB("[CAT24s128c] EEPROM_Open\n");
	spin_lock(&g_EEPROMLock);
	if (g_u4Opened) {
		spin_unlock(&g_EEPROMLock);
		ret = -EBUSY;
	} else {
		g_u4Opened = 1;
		atomic_set(&g_EEPROMatomic, 0);
	}
	spin_unlock(&g_EEPROMLock);

	return ret;
}

/* Main jobs: */
/* 1.Deallocate anything that "open" allocated in private_data. */
/* 2.Shut down the device on last close. */
/* 3.Only called once on last time. */
/* Q1 : Try release multiple times. */
static int EEPROM_Release(struct inode *a_pstInode, struct file *a_pstFile)
{
	spin_lock(&g_EEPROMLock);

	g_u4Opened = 0;

	atomic_set(&g_EEPROMatomic, 0);

	spin_unlock(&g_EEPROMLock);

	return 0;
}

static const struct file_operations g_stEEPROM_fops = {
	.owner = THIS_MODULE,
	.open = EEPROM_Open,
	.release = EEPROM_Release,
	/* .ioctl = EEPROM_Ioctl */
	.unlocked_ioctl = EEPROM_Ioctl
};

#define EEPROM_DYNAMIC_ALLOCATE_DEVNO 1
static inline int RegisterEEPROMCharDrv(void)
{
	struct device *EEPROM_device = NULL;

#if EEPROM_DYNAMIC_ALLOCATE_DEVNO
	if (alloc_chrdev_region(&g_EEPROMdevno, 0, 1, EEPROM_DRVNAME)) {
		EEPROMDB("[CAT24s128c] Allocate device no failed\n");

		return -EAGAIN;
	}
#else
	if (register_chrdev_region(g_EEPROMdevno, 1, EEPROM_DRVNAME)) {
		EEPROMDB("[CAT24s128c] Register device no failed\n");

		return -EAGAIN;
	}
#endif

	/* Allocate driver */
	g_pEEPROM_CharDrv = cdev_alloc();

	if (g_pEEPROM_CharDrv == NULL) {
		unregister_chrdev_region(g_EEPROMdevno, 1);

		EEPROMDB("[CAT24s128c] Allocate mem for kobject failed\n");

		return -ENOMEM;
	}

	/* Attatch file operation. */
	cdev_init(g_pEEPROM_CharDrv, &g_stEEPROM_fops);

	g_pEEPROM_CharDrv->owner = THIS_MODULE;

	/* Add to system */
	if (cdev_add(g_pEEPROM_CharDrv, g_EEPROMdevno, 1)) {
		EEPROMDB("[CAT24s128c] Attatch file operation failed\n");

		unregister_chrdev_region(g_EEPROMdevno, 1);

		return -EAGAIN;
	}

	EEPROM_class = class_create(THIS_MODULE, "EEPROMdrv");
	if (IS_ERR(EEPROM_class)) {
		int ret = PTR_ERR(EEPROM_class);

		EEPROMDB("Unable to create class, err = %d\n", ret);
		return ret;
	}
	EEPROM_device = device_create(EEPROM_class, NULL, g_EEPROMdevno, NULL, EEPROM_DRVNAME);

	return 0;
}

static inline void UnregisterEEPROMCharDrv(void)
{
	/* Release char driver */
	cdev_del(g_pEEPROM_CharDrv);

	unregister_chrdev_region(g_EEPROMdevno, 1);

	device_destroy(EEPROM_class, g_EEPROMdevno);
	class_destroy(EEPROM_class);
}


/* //////////////////////////////////////////////////////////////////// */
#ifndef EEPROM_ICS_REVISION
static int EEPROM_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info);
#elif 0
static int EEPROM_i2c_detect(struct i2c_client *client, struct i2c_board_info *info);
#else
#endif
static int EEPROM_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int EEPROM_i2c_remove(struct i2c_client *);

static const struct i2c_device_id EEPROM_i2c_id[] = {{EEPROM_DRVNAME, 0}, {} };
#if 0 /* test110314 Please use the same I2C Group ID as Sensor */
static unsigned short force[] = {EEPROM_I2C_GROUP_ID, S24CS64A_DEVICE_ID, I2C_CLIENT_END, I2C_CLIENT_END};
#else



#endif
/* static const unsigned short * const forces[] = { force, NULL }; */
/* static struct i2c_client_address_data addr_data = { .forces = forces,}; */


static struct i2c_driver EEPROM_i2c_driver = {
	.probe = EEPROM_i2c_probe,
	.remove = EEPROM_i2c_remove,
	/* .detect = EEPROM_i2c_detect, */
	.driver.name = EEPROM_DRVNAME,
	.id_table = EEPROM_i2c_id,
};

#ifndef EEPROM_ICS_REVISION
static int EEPROM_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info)
{
	strcpy(info->type, EEPROM_DRVNAME);
	return 0;
}
#endif
static int EEPROM_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int i4RetValue = 0;

	EEPROMDB("[CAT24s128c] Attach I2C\n");
	/* spin_lock_init(&g_EEPROMLock); */

	/* get sensor i2c client */
	spin_lock(&g_EEPROMLock); /* for SMP */
	g_pstI2Cclient = client;
	g_pstI2Cclient->addr = S24CS64A_DEVICE_ID >> 1;
	spin_unlock(&g_EEPROMLock); /* for SMP */

	EEPROMDB("[CAT24s128c] g_pstI2Cclient->addr = 0x%8x\n", g_pstI2Cclient->addr);
	/* Register char driver */
	i4RetValue = RegisterEEPROMCharDrv();

	if (i4RetValue) {
		EEPROMDB("[CAT24s128c] register char device failed!\n");
		return i4RetValue;
	}


	EEPROMDB("[CAT24s128c] Attached!!\n");
	return 0;
}

static int EEPROM_i2c_remove(struct i2c_client *client)
{
	return 0;
}

static int EEPROM_probe(struct platform_device *pdev)
{
	return i2c_add_driver(&EEPROM_i2c_driver);
}

static int EEPROM_remove(struct platform_device *pdev)
{
	i2c_del_driver(&EEPROM_i2c_driver);
	return 0;
}

/* platform structure */
static struct platform_driver g_stEEPROM_Driver = {
	.probe      = EEPROM_probe,
	.remove = EEPROM_remove,
	.driver     = {
		.name   = EEPROM_DRVNAME,
		.owner  = THIS_MODULE,
	}
};


static struct platform_device g_stEEPROM_Device = {
	.name = EEPROM_DRVNAME,
	.id = 0,
	.dev = {
	}
};

static int __init EEPROM_i2C_init(void)
{
	i2c_register_board_info(EEPROM_I2C_BUSNUM, &kd_eeprom_dev, 1);
	if (platform_driver_register(&g_stEEPROM_Driver)) {
		EEPROMDB("failed to register CAT24s128c driver\n");
		return -ENODEV;
	}

	if (platform_device_register(&g_stEEPROM_Device)) {
		EEPROMDB("failed to register CAT24s128c driver, 2nd time\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit EEPROM_i2C_exit(void)
{
	platform_driver_unregister(&g_stEEPROM_Driver);
}

module_init(EEPROM_i2C_init);
module_exit(EEPROM_i2C_exit);

MODULE_DESCRIPTION("EEPROM driver");
MODULE_AUTHOR("Sean Lin <Sean.Lin@Mediatek.com>");
MODULE_LICENSE("GPL");

#endif
