/*****************************************************************************
 *
 * Filename:
 * ---------
 *     GC13053mipi_Otp.h
 *
 * Project:
 * --------
 *     
 *
 * Description:
 * ------------
 *     CMOS sensor header file
 *
 ****************************************************************************/
#ifndef _GC13053MIPI_OTP_H
#define _GC13053MIPI_OTP_H

#define GC13053_OTP_DEBUG						0
#define GC13053_MULTI_WRITE						1
#define GC13053_I2C_SPEED						400

#if GC13053_MULTI_WRITE
#define I2C_BUFFER_LEN							765 /* Max is 255, each 3 bytes */
#else
#define I2C_BUFFER_LEN							3
#endif

/* OTP FLAG TYPE */
#define GC13053_OTP_FLAG_VALID					0x01
#define GC13053_OTP_CHECK_2BIT_FLAG(flag, bit)	(((flag >> bit) & 0x03) == GC13053_OTP_FLAG_VALID)
/* OTP REGISTER UPDATE PARAMETERS */
#define GC13053_OTP_REG_DATA_SIZE				30
#define GC13053_OTP_REG_REG_SIZE				10
#define GC13053_OTP_REG_FLAG_OFFSET				0x1060
#define GC13053_OTP_REG_DATA_OFFSET				0x1068
#define GC13053_OTP_GROUP_CNT					2

#if GC13053_OTP_DEBUG
#define GC13053_OTP_START_ADDR					0x0000
#endif

#define GC13053_OTP_DATA_LENGTH					1009
#define GC13053_OTP_EFF_LENGTH                  GC13053_OTP_REG_DATA_SIZE

/*REG STRUCTURE */
struct gc13053_reg_t {
	kal_uint8 check_sum;
	kal_uint16 addr;
	kal_uint8 value;
};

struct gc13053_reg_update_t {
	kal_uint8 flag;
	kal_uint8 cnt;
	struct gc13053_reg_t reg[GC13053_OTP_REG_REG_SIZE];
};

/* OTP STRUCTURE */
struct gc13053_otp_t {
struct gc13053_reg_update_t regs;
};


extern int iReadRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u8 *a_pRecvData, u16 a_sizeRecvData, u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u16 i2cId);
extern int iWriteRegI2CTiming(u8 *a_pSendData, u16 a_sizeSendData, u16 i2cId, u16 timing);
extern int iBurstWriteReg_multi(u8 *pData, u32 bytes, u16 i2cId, u16 transfer_length, u16 timing);
#endif
