/*****************************************************************************
	*
	* Filename:
	* ---------
	*	 GC13053mipi_Otp.c
	*
	* Project:
	* --------
	*	 
	*
	* Description:
	* ------------
	*	 Source code of gc13053 otp driver
	*
	* Driver Version:
	* ------------
	*	 V0.0001.120
	*
	****************************************************************************/
#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>
#include <linux/types.h>

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "gc13053mipi_Otp.h"

/************************** Modify Following Strings for Debug **************************/
#define PFX "GC13053_OTP"
/****************************   Modify end    *******************************************/
#define GC13053_PDAF_DEBUG      0
#if GC13053_PDAF_DEBUG
#define LOG_INF(format, args...)	pr_debug(PFX "[%s] " format, __func__, ##args)
#else
#define LOG_INF(format, args...)
#endif

static kal_uint8 gc13053_slave_addr = 0x72;
static struct gc13053_otp_t gc13053_otp_data;

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pu_send_cmd[2] = { (char)((addr >> 8) & 0xff), (char)(addr & 0xff) };

	iReadRegI2C(pu_send_cmd, 2, (u8 *)&get_byte, 1, gc13053_slave_addr);

	return get_byte;
}

static void write_cmos_sensor_8bit(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[3] = {
		(char)((addr >> 8) & 0xff),
		(char)(addr & 0xff),
		(char)(para & 0xff)
	};

	iWriteRegI2C(pu_send_cmd, 3, gc13053_slave_addr);
}

static void table_write_cmos_sensor(kal_uint16 *para, kal_uint32 len)
{
	char puSendCmd[I2C_BUFFER_LEN];
	kal_uint32 tosend = 0, idx = 0;
	kal_uint16 addr = 0, data = 0;

	while (len > idx) {
		addr = para[idx];
		puSendCmd[tosend++] = (char)((addr >> 8) & 0xff);
		puSendCmd[tosend++] = (char)(addr & 0xff);
		data = para[idx + 1];
		puSendCmd[tosend++] = (char)(data & 0xff);
		idx += 2;
#if GC13053_MULTI_WRITE
		if (tosend >= I2C_BUFFER_LEN || idx == len) {
			iBurstWriteReg_multi(puSendCmd, tosend, gc13053_slave_addr, 3, GC13053_I2C_SPEED);
			tosend = 0;
		}
#else
		iWriteRegI2CTiming(puSendCmd, 3, gc13053_slave_addr, GC13053_I2C_SPEED);
		tosend = 0;
#endif
	}
}

static kal_uint8 gc13053_otp_read_byte(kal_uint16 addr)
{
	kal_uint16 read_flow_addr_data[] = {
		0x0313, 0x00,
		0x0a69, (addr >> 8) & 0xff,
		0x0a6a, addr & 0xff,
		0x0313, 0x20,
	};
	table_write_cmos_sensor(read_flow_addr_data,
	sizeof(read_flow_addr_data) / sizeof(kal_uint16));
	return read_cmos_sensor(0x0a6c);
}

static void gc13053_otp_read_group(kal_uint16 addr, kal_uint8 *data, kal_uint16 length)
{
	kal_uint16 i = 0;
	kal_uint16 read_flow_addr_data[] = {
		0x0313, 0x00,
		0x0a69, (addr >> 8) & 0x3f,
		0x0a6a, addr & 0xff,
		0x0313, 0x20,
		0x0313, 0x12,
	};

	if ((((addr & 0x3fff) >> 3) + length) > GC13053_OTP_DATA_LENGTH) {
		LOG_INF("out of range, start addr: 0x%.4x, length = %d\n", addr & 0x3fff, length);
		return;
	}

	table_write_cmos_sensor(read_flow_addr_data,
		sizeof(read_flow_addr_data) / sizeof(kal_uint16));

	for (i = 0; i < length; i++)
		data[i] = read_cmos_sensor(0x0a6c);
}

static void gc13053_gcore_read_reg(void)
{
	kal_uint8 i = 0;
	kal_uint16 base = 0;
	kal_uint8 reg[GC13053_OTP_REG_DATA_SIZE];
	struct gc13053_reg_update_t *pRegs = &gc13053_otp_data.regs;

	memset(&reg, 0, GC13053_OTP_REG_DATA_SIZE);
	pRegs->flag = gc13053_otp_read_byte(GC13053_OTP_REG_FLAG_OFFSET);
	LOG_INF("register update flag = 0x%x\n", pRegs->flag);
	if (GC13053_OTP_CHECK_2BIT_FLAG(pRegs->flag, 0)) {
		gc13053_otp_read_group(GC13053_OTP_REG_DATA_OFFSET, &reg[0], GC13053_OTP_REG_DATA_SIZE);
		for (i = 0; i < GC13053_OTP_REG_REG_SIZE; i++) {
			base = 3 * i;
			if (GC13053_OTP_CHECK_2BIT_FLAG(reg[base], 6)) {
				pRegs->reg[pRegs->cnt].addr = (((reg[base] & 0x0f) << 8) | reg[base + 1]);
				pRegs->reg[pRegs->cnt].value = reg[base + 2];
				LOG_INF("register[%d] :0x%x->0x%x\n",
						pRegs->cnt, pRegs->reg[pRegs->cnt].addr, pRegs->reg[pRegs->cnt].value);
				pRegs->cnt++;
			}
		}

	}
}

static kal_uint8 gc13053_read_otp_info(void)
{
#if GC13053_OTP_DEBUG
	kal_uint16 i = 0;
	kal_uint8 debug[GC13053_OTP_DATA_LENGTH];
#endif
	memset(&gc13053_otp_data, 0, GC13053_OTP_EFF_LENGTH);
	gc13053_gcore_read_reg();
#if GC13053_OTP_DEBUG
	memset(&debug[0], 0, GC13053_OTP_DATA_LENGTH);
	gc13053_otp_read_group(GC13053_OTP_START_ADDR, &debug[0], GC13053_OTP_DATA_LENGTH);
	for (i = 0; i < GC13053_OTP_DATA_LENGTH; i++)
		LOG_INF("addr = 0x%.4x, data = 0x%.2x\n", GC13053_OTP_START_ADDR + i * 8, debug[i]);
#endif
return 0;
}

static void gc13053_otp_update_reg(void)
{
	kal_uint8 i = 0;

	LOG_INF("reg count = %d\n", gc13053_otp_data.regs.cnt);

	if (GC13053_OTP_CHECK_2BIT_FLAG(gc13053_otp_data.regs.flag, 0))
		for (i = 0; i < gc13053_otp_data.regs.cnt; i++) {
			write_cmos_sensor_8bit(gc13053_otp_data.regs.reg[i].addr, gc13053_otp_data.regs.reg[i].value);
			LOG_INF("reg[%d]:0x%x -> 0x%x\n", i,
			gc13053_otp_data.regs.reg[i].addr, gc13053_otp_data.regs.reg[i].value);
		}
}

static void gc13053_update_otp_info(void)
{
	gc13053_otp_update_reg();
}

static kal_uint16 gc13053_otp_init_addr_data[] = {
	0x0a67, 0x80,
	0x0317, 0x08,
	0x0a54, 0x08,
	0x0313, 0x00,
	0x0a59, 0x40,
	0x0a65, 0x10,
};

static kal_uint16 gc13053_otp_close_addr_data[] = {
	0x0313, 0x00,
	0x0317, 0x00,
	0x0a67, 0x00,
};

kal_uint8 gc13053_otp_function(kal_uint8 i2c_slave_addr)
{
	gc13053_slave_addr = i2c_slave_addr;
	table_write_cmos_sensor(gc13053_otp_init_addr_data,
		sizeof(gc13053_otp_init_addr_data) / sizeof(kal_uint16));
	gc13053_read_otp_info();
	table_write_cmos_sensor(gc13053_otp_close_addr_data,
		sizeof(gc13053_otp_close_addr_data) / sizeof(kal_uint16));
	gc13053_update_otp_info();

	return 0;
}
