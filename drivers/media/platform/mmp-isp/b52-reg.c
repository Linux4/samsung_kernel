/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <asm/segment.h>
#include <media/v4l2-dev.h>
#include <uapi/linux/videodev2.h>
#include <linux/v4l2-mediabus.h>
#include <linux/bitops.h>
#include <linux/firmware.h>
#include <linux/proc_fs.h>
#include <linux/kernel.h>
#include <linux/pm_qos.h>

#include <media/b52socisp/b52socisp-vdev.h>
#include <media/b52-sensor.h>
#include "b52-reg.h"

#include <linux/workqueue.h>

static u64 of_jiffies;
static u8 of;
static DEFINE_MUTEX(cmd_mutex);
static void __iomem *b52_base;
static atomic_t streaming_state = ATOMIC_INIT(0);
static DEFINE_SPINLOCK(mac_overflow_lock);
static int mac_overflow[MAX_MAC_NUM][MAX_PORT_NUM] = {
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
};
static u32 mac_base[MAX_MAC_NUM] = {
	MAC1_REG_BASE, MAC2_REG_BASE, MAC3_REG_BASE
};

static u32 mac_addr_offset[MAX_PORT_NUM][MAX_MAC_ADDR_NUM] = {
	{REG_MAC_ADDR0, REG_MAC_ADDR1,  REG_MAC_ADDR2,  REG_MAC_ADDR16},
	{REG_MAC_ADDR3, REG_MAC_ADDR12, REG_MAC_ADDR13, REG_MAC_ADDR16},
	{REG_MAC_ADDR8, REG_MAC_ADDR9}
};

static u32 b52_reg_map[][2] = {
	{FW_BUF_START,		FW_BUF_END},
	{DATA_BUF_START,	DATA_BUF_END},
	{CACHE_START,		CACHE_END},
	{LINE_BUF_START,	LINE_BUF_END},
	{HW_REG_START,		HW_REG_END},
};

static DECLARE_COMPLETION(b52isp_cmd_done);
static __u8 b52isp_cmd_id;

static int b52_set_aecagc_reg(struct v4l2_subdev *sd, int p_num);
static int b52_set_vcm_reg(struct v4l2_subdev *sd, int p_num);
static struct delayed_work rdy_work;
#define FRAME_TIME_MS 10

#ifdef CONFIG_ISP_USE_TWSI3
static void *sensor;
static struct work_struct twsi_work;
#define	REG_FW_AGC_AF_FLG	0x63910
#define	REG_FW_AGC_FLG		0x33590
#define	REG_FW_AF_FLG		0x33da0
#define	REG_FW_EXPO_VAL		0x30221
#define	REG_FW_GAIN_VAL		0x3021e
#define	REG_FW_AF_VAL		0x33c3e
#define	REG_FW_VTS_VAL		0x33050

static DEFINE_SPINLOCK(twsi_work_lock);
static void twsi_do_work(struct work_struct *data)
{
	u8 i;
	u32 expo = 0;
	u16 val = 0;
	u8 aec_agc_flg, af_flg;
	struct b52_sensor *psensor = (struct b52_sensor *)sensor;

	if (atomic_read(&streaming_state) == 0)
		return;

	spin_lock(&twsi_work_lock);
	aec_agc_flg = b52_readb(REG_FW_AGC_FLG);
	b52_writeb(REG_FW_AGC_FLG, 0);
	af_flg = b52_readb(REG_FW_AF_FLG);
	b52_writeb(REG_FW_AF_FLG, 0);
	spin_unlock(&twsi_work_lock);

	if ((aec_agc_flg == CMD_WB_EXPO_GAIN) || (aec_agc_flg == CMD_WB_EXPO)) {
		val = b52_readw(REG_FW_VTS_VAL);
		for (i = 0; i < 3; i++)
			expo |= b52_readb(REG_FW_EXPO_VAL + i) << ((2 - i) * 8);
		b52_sensor_call(psensor, s_expo, expo, val);
	}

	if ((aec_agc_flg == CMD_WB_EXPO_GAIN) || (aec_agc_flg == CMD_WB_GAIN)) {
		val = b52_readw(REG_FW_GAIN_VAL);
		b52_sensor_call(psensor, s_gain, val);
	}

	if (af_flg == CMD_WB_FOCUS) {
		val = b52_readw(REG_FW_AF_VAL);
		b52_sensor_call(psensor, s_focus, val);
	}
}
void b52_init_workqueue(void *data)
{
	sensor = data;
	INIT_WORK(&twsi_work, twsi_do_work);
}
EXPORT_SYMBOL_GPL(b52_init_workqueue);
#endif
static void rdy_do_work(struct work_struct *data)
{
	/*FIXME only MAC1 is support*/
	if (atomic_read(&streaming_state) == 0)
		return;

	b52_writeb(mac_base[0] + REG_MAC_RDY_ADDR1, R_RDY_0);
}
static DECLARE_DELAYED_WORK(rdy_work, rdy_do_work);

#if 1
inline u8 b52_readb(const u32 addr)
{
	if (addr < DATA_BUF_END)
		return readb(b52_base + (addr ^ 0x3));
	if ((addr >= HW_REG_START) && (addr <= HW_REG_END))
		return readb(b52_base + addr);
	if ((addr >= LINE_BUF_START) && (addr <= LINE_BUF_END))
		return readb(b52_base + (addr ^ 0x3));

	return WARN_ON(~0);
}

inline void b52_writeb(const u32 addr, const u8 value)
{
	if (addr < DATA_BUF_END)
		writeb(value, b52_base + (addr ^ 0x3));
	if ((addr >= HW_REG_START) && (addr <= HW_REG_END))
		writeb(value, b52_base + addr);
}

inline u16 b52_readw(const u32 addr)
{
	if (addr < DATA_BUF_END)
		return readw(b52_base + (addr ^ 0x2));
	if ((addr >= HW_REG_START) && (addr <= HW_REG_END))
		return (readb(b52_base + addr) << 8)
			+ readb(b52_base + addr + 1);
	return WARN_ON(~0);
}

inline void b52_writew(const u32 addr, const u16 value)
{
	if (addr < DATA_BUF_END)
		writew(value, b52_base + (addr ^ 0x2));
	if ((addr >= HW_REG_START) && (addr <= HW_REG_END)) {
		writeb((value >> 8) & 0xFF, b52_base + addr + 0);
		writeb((value >> 0) & 0xFF, b52_base + addr + 1);
	}
}

inline u32 b52_readl(const u32 addr)
{
	if (addr < DATA_BUF_END)
		return readl(b52_base + addr);
	if ((addr >= HW_REG_START) && (addr <= HW_REG_END))
		return (readb(b52_base + addr + 0) << 24) +
			(readb(b52_base + addr + 1) << 16) +
			(readb(b52_base + addr + 2) << 8) +
			(readb(b52_base + addr + 3) << 0);
	return WARN_ON(~0);
}

inline void b52_writel(const u32 addr, const u32 value)
{
	if (addr < DATA_BUF_END)
		writel(value, b52_base + addr);
	if ((addr >= HW_REG_START) && (addr <= HW_REG_END)) {
		writeb((value >> 24) & 0xFF, b52_base + addr + 0);
		writeb((value >> 16) & 0xFF, b52_base + addr + 1);
		writeb((value >> 8) & 0xFF, b52_base + addr + 2);
		writeb((value >> 0) & 0xFF, b52_base + addr + 3);
	}
}
#else
static inline u8 b52_readb(const u32 addr)
{
	pr_err("%s: addr %x\n", __func__, addr);
	return 0;
}

static inline void b52_writeb(
				const u32 addr, const u8 value)
{
	pr_err("%s: addr %x, value %x\n", __func__, addr, value);
}

static inline u16 b52_readw(const u32 addr)
{
	pr_err("%s: addr %x\n", __func__, addr);
	return 0;
}

static inline void b52_writew(
				const u32 addr, const u16 value)
{
	pr_err("%s: addr %x, value %x\n", __func__, addr, value);
}

static inline u32 b52_readl(const u32 addr)
{
	pr_err("%s: addr %x\n", __func__, addr);
	return 0;
}

static inline void b52_writel(
				const u32 addr, const u32 value)
{
	pr_err("%s: addr %x, value %x\n", __func__, addr, value);
}
#endif

#if 1
#define	ISP_TOP0			(0x00)
#define	ISP_TOP1			(0x01)
#define	ISP_TOP2			(0x02)
#define	ISP_TOP3			(0x03)
#define	ISP_TOP5			(0x05)
#define	ISP_TOP6			(0x06)
#define ISP_TESTPAT         (0x07)
#define	ISP_TOP11			(0x0b)
#define	ISP_TOP99			(0x63)
#define	ISP_TOP100			(0x64)

#define	ISP_H_IN_SIZE		(0x10)
#define	ISP_V_IN_SIZE		(0x12)
#define	ISP_SCALE_H_OUT		(0x14)
#define	ISP_SCALE_V_OUT		(0x16)
#define	ISP_LENC_V_OFF		(0x1a)
#define	ISP_H_IN_SIZE		(0x10)
#define	ISP_TOP33			(0x21)
#define	ISP_CTRL2b			(0x2b)
#define	ISP_SCALE10			(0x2e)
#define	ISP_TOP38			(0x30)
#define	ISP_ANTI			(0x31)
#define	ISP_SD_H_OUT_SZ		(0x32)
#define	ISP_SD_V_OUT_SZ		(0x34)

#define	ISP_TOP80			(0x50)
#define	ISP_YUVD1_H_SZ_OUT	(0x54)
#define	ISP_YUVD1_V_SZ_OUT	(0x56)
#define	ISP_SC1_H_SZ_OUT	(0x58)
#define	ISP_SC1_V_SZ_OUT	(0x5a)

#define	ISP_SCALE0_X_OFF	(0x65)
#define	ISP_SCALE0_Y_OFF	(0x67)
#define	ISP_SCALE1_X_OFF	(0x69)
#define	ISP_SCALE1_Y_OFF	(0x6b)

#define	ISP_TOP113			(0x79)
#define	ISP_YUVD2_H_SZ_OUT	(0x7c)
#define	ISP_YUVD2_V_SZ_OUT	(0x7e)
#define	ISP_SC2_H_SZ_OUT	(0x81)
#define	ISP_SC2_V_SZ_OUT	(0x83)
#define	ISP_SCALE2_X_OFF	(0x8a)
#define	ISP_SCALE2_Y_OFF	(0x8c)

#define ISP_TONE_CTRL0		(0xa00)


#define	IDI_BASE			(0x63c00)
#define IDI_X_OFFSET16		(0x00)
#define IDI_Y_OFFSET16		(0x02)
#define IDI_WIDTH16			(0x04)
#define IDI_HEIGHT16		(0x06)
#define IDI_X_OFFSET64		(0x08)
#define IDI_Y_OFFSET64		(0x0a)
#define IDI_WIDTH64			(0x0c)
#define IDI_HEIGHT64		(0x0e)
#define IDI_LINE_LENG16		(0x10)
#define IDI_LINE_LENG64		(0x12)
#define IDI_CTRL_14			(0x14)
#define IDI_CTRL_15			(0x15)

static u32 isp_base[2] = { ISP1_REG_BASE, ISP2_REG_BASE};

void __maybe_unused isp_conf_100a(int isp_id, u16 win,
				u16 hin, u16 wout, u16 hout)
{
	u32	isp_b = isp_base[isp_id];
	u32 off;

	b52_writeb(isp_b+ISP_TOP0, 0x3f);
	b52_writeb(isp_b+ISP_TOP1, 0x6f);
	b52_writeb(isp_b+ISP_TOP2, 0x87);
	b52_writeb(isp_b+ISP_TOP3, 0xcf);
	b52_writeb(isp_b+ISP_TOP5, 0x50);
	b52_writeb(isp_b+ISP_TOP6, 0x06);

	b52_writew(isp_b+ISP_H_IN_SIZE, win);
	b52_writew(isp_b+ISP_V_IN_SIZE, hin);
	b52_writew(isp_b+ISP_SCALE_H_OUT, wout);
	b52_writew(isp_b+ISP_SCALE_V_OUT, hout);

	b52_writew(isp_b+ISP_LENC_V_OFF, 0x19f6);
	b52_writeb(isp_b+ISP_TOP33, 0x00);
	b52_writew(isp_b+ISP_SCALE0_X_OFF, 0);

	b52_writeb(isp_b+ISP_SCALE10, 0x40);
	b52_writeb(isp_b+ISP_TOP38, 0x92);
	b52_writeb(isp_b+ISP_ANTI, 0x50);

	b52_writew(isp_b+ISP_SD_H_OUT_SZ, wout);
	b52_writew(isp_b+ISP_SD_V_OUT_SZ, hout);

	b52_writeb(isp_b+ISP_CTRL2b, 0x8b);
	b52_writeb(isp_b+ISP_TOP80, 0x18);

	b52_writeb(isp_b+ISP_TOP113, 0x04);

	b52_writeb(isp_b+0x81, 0x00);
	b52_writew(isp_b+0x83, 0xa2);
	b52_writew(isp_b+0x8a, 0x89);
	b52_writew(isp_b+0x8c, 0xb1);


#define	AFC_nX0			(0x1104)
#define	AFC_nY0			(0x1106)
#define	AFC_nW0			(0x1108)
#define	AFC_nH0			(0x110a)
#define	AFC_nW1			(0x110c)
#define	AFC_nH1			(0x110e)
#define	AFC_CTRL10		(0x1110)

	b52_writew(isp_b+AFC_nX0, 0x0001);
	b52_writew(isp_b+AFC_nY0, 0x0000);
	b52_writew(isp_b+AFC_nW0, 0x0078);
	b52_writew(isp_b+AFC_nH0, 0x0078);
	b52_writew(isp_b+AFC_nW1, 0x0208);
	b52_writew(isp_b+AFC_nH1, 0x0208);
	b52_writeb(isp_b+AFC_CTRL10, 0);


#define AFC_OFF         (0x1100)
	for (off = 0x14; off <= 0x19; off++)
		b52_writeb(isp_b+AFC_OFF+off, 0);



#define	LENC_H_SCALE		(0x100)
#define	LENC_V_SCALE		(0x102)
#define	LENC_OFFSET_X		(0x218)
#define	LENC_OFFSET_Y		(0x21a)
#define	LENC_OFF_X_RIGHT	(0x21c)
#define	LENC_OFF_Y_RIGHT	(0x21e)
#define	LENC_MAX_R_MAN		(0x220)
#define	LENC_MAX_R_MAN_R	(0x222)

	b52_writew(isp_b+LENC_H_SCALE, 0x22e8);
	b52_writew(isp_b+LENC_V_SCALE, 0x1555);
	b52_writew(isp_b+LENC_OFFSET_X, 0x0c50);
	b52_writew(isp_b+LENC_OFFSET_Y, 0x0c58);
	b52_writew(isp_b+LENC_OFF_X_RIGHT, 0x0c60);
	b52_writew(isp_b+LENC_OFF_Y_RIGHT, 0x0c68);
	b52_writew(isp_b+LENC_MAX_R_MAN, 0x1);
	b52_writew(isp_b+LENC_MAX_R_MAN_R, 0x1);

	b52_writeb(isp_b+0x1cc4, 0);


#define	AWB_32		(0x320)
	b52_writeb(isp_b+AWB_32, 0x01);
	b52_writew(isp_b+0x300, 0x100);
	b52_writew(isp_b+0x302, 0x80);
	b52_writew(isp_b+0x304, 0x100);
	b52_writew(isp_b+0x306, 0x100);

#if 0
	for (off = 0x68310; off <= 0x68317; off++)
		b52_writeb(OVTISP_BASE+off, 0);

	for (off = 0x69cc8; off <= 0x69ccf; off++)
		b52_writeb(OVTISP_BASE+off, 0);

#endif


#define	STRETCH_STEP1		(0x2007)
#define	STRETCH_STEP2		(0x2008)

	b52_writeb(isp_b+STRETCH_STEP1, 0x1);
	b52_writeb(isp_b+STRETCH_STEP2, 0x4);


#define	RAW_DNS_OFF			(0x2300)
#define	CIP_CTRL0D_L		(0x60d)
#define	LOWLEVEL_00			(0x1500)

	for (off = 0x00; off <= 0x13; off++)
		b52_writeb(isp_b+RAW_DNS_OFF+off, 0);

	for (off = 0x18; off <= 0x41; off++)
		b52_writeb(isp_b+RAW_DNS_OFF+off, 0);

	b52_writeb(isp_b+CIP_CTRL0D_L, 0);
	b52_writeb(isp_b+LOWLEVEL_00, 0x3f);


	b52_writeb(isp_b+0x807, 0x07);
	b52_writeb(isp_b+0x808, 0x09);
	b52_writeb(isp_b+0x80f, 0x00);
	b52_writeb(isp_b+0x810, 0x00);
	b52_writeb(isp_b+0x813, 0x00);
	b52_writeb(isp_b+0x814, 0x00);
	b52_writeb(isp_b+0x823, 0x40);

	b52_writeb(isp_b+0x827, 0x60);
	b52_writeb(isp_b+0x828, 0x60);
	b52_writeb(isp_b+0x82b, 0x40);
	b52_writeb(isp_b+0x82c, 0x40);
	b52_writeb(isp_b+0x82f, 0x01);

	b52_writeb(isp_b+0x833, 0x0);
	b52_writeb(isp_b+0x834, 0x0);
	b52_writeb(isp_b+0x835, 0x0);
	b52_writeb(isp_b+0x836, 0x0);
	b52_writeb(isp_b+0x1cf8, 0x0);
	b52_writeb(isp_b+0x1cf9, 0x0);

	b52_writeb(isp_b+0x1201, 0x52);
	b52_writeb(isp_b+0x1206, 0x1f);
	b52_writeb(isp_b+0x1209, 0x6f);
	b52_writeb(isp_b+0x120a, 0x66);
	b52_writeb(isp_b+0x120b, 0xdf);
	b52_writeb(isp_b+0x120c, 0xec);
	b52_writeb(isp_b+0x120d, 0x47);

	b52_writeb(isp_b+0x120e, 0x4f);
	b52_writeb(isp_b+0x120f, 0x5e);
	b52_writeb(isp_b+0x1210, 0x5a);
	b52_writeb(isp_b+0x1211, 0xf0);
	b52_writeb(isp_b+0x1212, 0x10);

	b52_writeb(isp_b+0x2342, 0x0c);

#if 0
	b52_writeb(OVTISP_BASE+0x6882f, 0x01);

	b52_writeb(OVTISP_BASE+0x66cf8, 0x00);
	b52_writeb(OVTISP_BASE+0x66cf9, 0x00);

	b52_writeb(OVTISP_BASE+0x66cfa, 0x00);
	b52_writeb(OVTISP_BASE+0x66cfb, 0x00);
	b52_writeb(OVTISP_BASE+0x66cfc, 0x00);
	b52_writeb(OVTISP_BASE+0x66cfd, 0x00);

	b52_writeb(OVTISP_BASE+0x66d84, 0x03);
	b52_writeb(OVTISP_BASE+0x66d85, 0xa4);
	b52_writeb(OVTISP_BASE+0x66d86, 0x02);
	b52_writeb(OVTISP_BASE+0x66d87, 0x00);
	b52_writeb(OVTISP_BASE+0x66d88, 0x19);
	b52_writeb(OVTISP_BASE+0x66d89, 0x5c);
	b52_writeb(OVTISP_BASE+0x66d8d, 0x00);
	b52_writeb(OVTISP_BASE+0x66d8e, 0x04);
	b52_writeb(OVTISP_BASE+0x66d8f, 0x00);
#endif

	b52_writeb(isp_b+0x1cfa, 0x00);
	b52_writeb(isp_b+0x1cfb, 0x00);
	b52_writeb(isp_b+0x1cfc, 0x00);
	b52_writeb(isp_b+0x1cfd, 0x00);

	b52_writeb(isp_b+0x1d84, 0x03);
	b52_writeb(isp_b+0x1d85, 0xa4);
	b52_writeb(isp_b+0x1d86, 0x02);
	b52_writeb(isp_b+0x1d87, 0x00);
	b52_writeb(isp_b+0x1d88, 0x19);
	b52_writeb(isp_b+0x1d89, 0x5c);

	b52_writeb(isp_b+0x1d8d, 0x00);
	b52_writeb(isp_b+0x1d8e, 0x04);
	b52_writeb(isp_b+0x1d8f, 0x00);

	b52_writeb(isp_b+0x912, 0x1f);
	b52_writeb(isp_b+0x913, 0xff);
	b52_writeb(isp_b+0x914, 0xff);

	b52_writeb(isp_b+0x1d91, 0x01);
	b52_writeb(isp_b+0x1d92, 0x19);
	b52_writeb(isp_b+0x1d93, 0x92);
	b52_writeb(isp_b+0x1d97, 0x5d);

	b52_writeb(isp_b+0x140f, 0x00);
	b52_writeb(isp_b+0x1411, 0x00);
	b52_writeb(isp_b+0x1412, 0x58);
	b52_writeb(isp_b+0x1415, 0x00);

	b52_writeb(isp_b+0x1416, 0x48);
	b52_writeb(isp_b+0x1417, 0x00);
	b52_writeb(isp_b+0x1c00, 0x00);
	b52_writeb(isp_b+0x1c01, 0x40);
	b52_writeb(isp_b+0x1c02, 0x00);
	b52_writeb(isp_b+0x1c03, 0x40);


	b52_writeb(isp_b+ISP_TONE_CTRL0, 0x1d);

	b52_writeb(isp_b+0x1d99, 0x00);
	b52_writeb(isp_b+0x1d9a, 0x58);

	b52_writeb(isp_b+0x1d9b, 0x00);
	b52_writew(isp_b+0x1d9d, 0x00a0);
	b52_writeb(isp_b+0x1d9f, 0x00);
	b52_writew(isp_b+0x1da1, 0x00dc);

	b52_writeb(isp_b+0x1da3, 0x00);
	b52_writew(isp_b+0x1da5, 0x0114);

	b52_writeb(isp_b+0x1da7, 0x00);
	b52_writew(isp_b+0x1da9, 0x0144);

	b52_writeb(isp_b+0x1dab, 0x00);
	b52_writew(isp_b+0x1dad, 0x0174);

	b52_writeb(isp_b+0x1daf, 0x00);
	b52_writew(isp_b+0x1db1, 0x01a4);

	b52_writeb(isp_b+0x1db3, 0x00);
	b52_writew(isp_b+0x1db5, 0x01d4);

	b52_writeb(isp_b+0x1db7, 0x00);
	b52_writew(isp_b+0x1db9, 0x0204);

	b52_writeb(isp_b+0x1dbb, 0x00);
	b52_writew(isp_b+0x1dbd, 0x0238);

	b52_writeb(isp_b+0x1dbf, 0x00);
	b52_writew(isp_b+0x1dc1, 0x026c);

	b52_writeb(isp_b+0x1dc3, 0x00);
	b52_writew(isp_b+0x1dc5, 0x02ac);

	b52_writeb(isp_b+0x1dc7, 0x00);
	b52_writew(isp_b+0x1dc9, 0x02ec);

	b52_writeb(isp_b+0x1dcb, 0x00);
	b52_writew(isp_b+0x1dcd, 0x0338);

	b52_writeb(isp_b+0x1dcf, 0x00);
	b52_writew(isp_b+0x1dd1, 0x0394);

	b52_writeb(isp_b+0x1dd3, 0x00);

	b52_writew(isp_b+0x1dd8, 0x00ba);
	b52_writew(isp_b+0x1dda, 0x00cc);
	b52_writew(isp_b+0x1ddc, 0x00df);
	b52_writew(isp_b+0x1dde, 0x00ed);
	b52_writew(isp_b+0x1de0, 0x00fc);
	b52_writew(isp_b+0x1de2, 0x0108);
	b52_writew(isp_b+0x1de4, 0x0111);
	b52_writew(isp_b+0x1de6, 0x0118);
	b52_writew(isp_b+0x1de8, 0x011d);
	b52_writew(isp_b+0x1dea, 0x0120);

	b52_writew(isp_b+0x1dec, 0x0122);
	b52_writew(isp_b+0x1dee, 0x011f);
	b52_writew(isp_b+0x1df0, 0x011c);
	b52_writew(isp_b+0x1df2, 0x0116);
	b52_writew(isp_b+0x1df4, 0x010c);

	b52_writew(isp_b+0x1df8, 0xbae3);
	b52_writew(isp_b+0x1dfa, 0x8892);
	b52_writew(isp_b+0x1dfc, 0xaaaa);
	b52_writew(isp_b+0x1dfe, 0xaaaa);
	b52_writew(isp_b+0x1e00, 0xaa9d);

	b52_writeb(isp_b+0x1e02, 0x9d);
	b52_writeb(isp_b+0x1e05, 0xd7);
	b52_writeb(isp_b+0x1e06, 0xb2);
	b52_writeb(isp_b+0x1e07, 0x97);
	b52_writeb(isp_b+0x1e08, 0x16);
	b52_writeb(isp_b+0x1e09, 0x16);
	b52_writeb(isp_b+0x1e0a, 0x15);
	b52_writeb(isp_b+0x1e0b, 0x15);
	b52_writeb(isp_b+0x1e0c, 0x15);
	b52_writeb(isp_b+0x1e0d, 0x15);
	b52_writeb(isp_b+0x1e0e, 0x15);
	b52_writeb(isp_b+0x1e0f, 0x15);
	b52_writeb(isp_b+0x1e10, 0x15);
	b52_writeb(isp_b+0x1e11, 0x15);
	b52_writeb(isp_b+0x1e12, 0x15);

	b52_writeb(isp_b+0x1e15, 0x16);
	b52_writeb(isp_b+0x1e16, 0x16);
	b52_writeb(isp_b+0x1e17, 0x16);


	/* set for AEC register */
#define	AEC_STATWIN_TOP		(0x1403)
#define	AEC_STATWIN_RIGHT	(0x1405)
#define	AEC_STATWIN_BOTTOM	(0x1407)
#define	AEC_WIN_LEFT		(0x1409)
#define	AEC_WIN_TOP			(0x140d)
#define	AEC_WIN_WIDTH		(0x1411)
#define	AEC_WIN_HEIGHT		(0x1415)
#define	AEC_4F				(0x144f)
#define	AEC_67				(0x1467)

	b52_writew(isp_b+AEC_STATWIN_TOP, 0x0000);
	b52_writew(isp_b+AEC_STATWIN_BOTTOM, 0x0060);
	b52_writew(isp_b+AEC_WIN_LEFT, 0x2c);
	b52_writew(isp_b+AEC_WIN_TOP,  0x24);

	/* set for SDE registers*/
#define	SDE_CTRL1a			(0xb1a)
#define	SDE_START_GAIN		(0xb1b)
#define	SDE_END_GAIN		(0xb1c)
#define	SDE_LOW_THRE		(0xb1d)
#define	SDE_HIGH_THRE		(0xb1e)
#define	SDE_YUV_THRE01		(0xb01)
#define	SDE_YUV_THRE10		(0xb02)
#define	SDE_YUV_THRE11		(0xb03)
#define	SDE_YUV_THRE20		(0xb04)
#define	SDE_YUV_THRE21		(0xb05)
#define	SDE_YGAIN			(0xb07)
#define	SDE_UV_MAX_00		(0xb09)
#define	SDE_UV_MAX_01		(0xb0b)
#define	SDE_UV_MAX_10		(0xb0d)
#define	SDE_UV_MAX_11		(0xb0f)
#define	SDE_HGAIN			(0xb19)

	b52_writeb(isp_b+SDE_CTRL1a, 0x24);
	b52_writeb(isp_b+SDE_START_GAIN, 0x20);
	b52_writeb(isp_b+SDE_END_GAIN, 0x40);
	b52_writeb(isp_b+SDE_LOW_THRE, 0x20);
	b52_writeb(isp_b+SDE_HIGH_THRE, 0x20);

	b52_writeb(isp_b+SDE_YUV_THRE01, 0xff);
	b52_writeb(isp_b+SDE_YUV_THRE10, 0x80);
	b52_writeb(isp_b+SDE_YUV_THRE11, 0x7f);
	b52_writeb(isp_b+SDE_YUV_THRE20, 0x80);
	b52_writeb(isp_b+SDE_YUV_THRE21, 0x7f);
	b52_writew(isp_b+SDE_YGAIN, 0x80);
	b52_writew(isp_b+SDE_UV_MAX_00, 0x0040 /*0x09*/);
	b52_writew(isp_b+SDE_UV_MAX_01, 0x09);
	b52_writew(isp_b+SDE_UV_MAX_10, 0x09);
	b52_writew(isp_b+SDE_UV_MAX_11, 0x0040/*0x09*/);
	b52_writeb(isp_b+SDE_HGAIN, 0x04);

	/* set for YUV_CROP register */
#define	YUV_CROP_OFF		(0x00f0)
#define	YUV_CROP_CH1_OFF	(0x0e00)
#define	YUV_CROP_CH2_OFF	(0x2100)
#define	YUV_CROP_CH3_OFF	(0x2700)

#define	CROP_WIDTH			(0x04)
#define	CROP_HEIGHT			(0x06)

	b52_writew(isp_b+YUV_CROP_OFF+CROP_WIDTH, 0x500);
	b52_writew(isp_b+YUV_CROP_OFF+CROP_HEIGHT, 0x320);
	b52_writew(isp_b+YUV_CROP_CH1_OFF+CROP_WIDTH, wout);
	b52_writew(isp_b+YUV_CROP_CH1_OFF+CROP_HEIGHT, hout);

#define	UV_DNS_03		(0xc03)
#define	UV_DNS_04		(0xc04)
#define	UV_DNS_05		(0xc05)
#define	UV_DNS_06		(0xc06)
#define	UV_DNS_07		(0xc07)

	b52_writeb(isp_b+UV_DNS_03, 0x08);
	b52_writeb(isp_b+UV_DNS_04, 0x0c);
	b52_writeb(isp_b+UV_DNS_05, 0x10);
	b52_writeb(isp_b+UV_DNS_06, 0x46);
	b52_writeb(isp_b+UV_DNS_07, 0x00);

#define	RGBhDNS_CTRL1	(0xd01)
#define	RGBhDNS_CTRL2	(0xd02)
#define	RGBhDNS_CTRL3	(0xd03)
#define	RGBhDNS_CTRL4	(0xd04)
#define	RGBhDNS_CTRL5	(0xd05)
#define	RGBhDNS_CTRL6	(0xd06)
#define	RGBhDNS_CTRL7	(0xd07)

	b52_writeb(isp_b+RGBhDNS_CTRL1, 0x01);
	b52_writeb(isp_b+RGBhDNS_CTRL2, 0x02);
	b52_writeb(isp_b+RGBhDNS_CTRL3, 0x03);
	b52_writeb(isp_b+RGBhDNS_CTRL4, 0x04);
	b52_writeb(isp_b+RGBhDNS_CTRL5, 0x04);
	b52_writeb(isp_b+RGBhDNS_CTRL6, 0x04);
	b52_writeb(isp_b+RGBhDNS_CTRL7, 0x04);

#define TDNS_OFF		(0x2500)

	b52_writeb(isp_b+TDNS_OFF+0x00, 0x01);
	b52_writeb(isp_b+TDNS_OFF+0x01, 0x05);
	b52_writeb(isp_b+TDNS_OFF+0x02, 0x05);
	b52_writeb(isp_b+TDNS_OFF+0x03, 0x40);
	b52_writeb(isp_b+TDNS_OFF+0x04, 0x60);
	b52_writeb(isp_b+TDNS_OFF+0x05, 0xbd);
	b52_writeb(isp_b+TDNS_OFF+0x06, 0xbd);
	b52_writeb(isp_b+TDNS_OFF+0x07, 0xbd);
	b52_writeb(isp_b+TDNS_OFF+0x08, 0x14);
	b52_writeb(isp_b+TDNS_OFF+0x09, 0x22);
	b52_writeb(isp_b+TDNS_OFF+0x0a, 0x30);
	b52_writeb(isp_b+TDNS_OFF+0x0b, 0x0a);
	b52_writeb(isp_b+TDNS_OFF+0x0c, 0x0f);
	b52_writeb(isp_b+TDNS_OFF+0x0d, 0x14);
	b52_writeb(isp_b+TDNS_OFF+0x0e, 0x23);
	b52_writeb(isp_b+TDNS_OFF+0x0f, 0x04);
	b52_writeb(isp_b+TDNS_OFF+0x10, 0x04);
	b52_writeb(isp_b+TDNS_OFF+0x11, 0x04);
	b52_writeb(isp_b+TDNS_OFF+0x12, 0x08);
	b52_writeb(isp_b+TDNS_OFF+0x13, 0x00);
	b52_writeb(isp_b+TDNS_OFF+0x14, 0x08);


#define	YUV_ADJ_OFF		(0x2400)

	/* set YUV_ADJ register */
	for (off = 0x00; off <= 0x23; off++)
		b52_writeb(isp_b+YUV_ADJ_OFF+off, 0);

	b52_writeb(isp_b+YUV_ADJ_OFF+0x2c, 0);
	b52_writeb(isp_b+YUV_ADJ_OFF+0x2f, 0);
	b52_writeb(isp_b+YUV_ADJ_OFF+0x30, 0);
	b52_writeb(isp_b+YUV_ADJ_OFF+0x31, 0);
	b52_writeb(isp_b+YUV_ADJ_OFF+0x33, 0);
	b52_writeb(isp_b+YUV_ADJ_OFF+0x34, 0);

	for (off = 0x48; off <= 0x4f; off++)
		b52_writeb(isp_b+YUV_ADJ_OFF+off, 0);

}
#endif

#ifdef CONFIG_ISP_USE_TWSI3
static void b52_read_vcm_info(int pipe_id, u16 *val)
{

	/* get af from blueprint data.
	 * pipeline1 and pipeline2 have different register*/
	if (pipe_id == 0)
		*val = b52_readw(FW_P1_REG_AF_BASE + REG_FW_AF_OFFSET);
	else
		*val = b52_readw(FW_P2_REG_AF_BASE + REG_FW_AF_OFFSET);
}
#endif

int b52_read_pipeline_info(int pipe_id, u8 *buffer)
{
	int i;
	u32 reg;

	if (!buffer)
		return -EINVAL;

	switch (pipe_id) {
	case 0:
		reg = CMD_BUF_B;
		break;
	case 1:
		reg = CMD_BUF_B + PIPE_INFO_OFFSET;
		break;
	default:
		return -EINVAL;
	}

	for (i = 0; i < PIPE_INFO_SIZE; i++)
		buffer[i] = b52_readb(reg + i);
	return 0;
}

static int b52_write_pipeline_info(int path, struct isp_videobuf *ivb)
{
	int i;
	u8 *buf;
	u32 reg;

	if (!ivb)
		return -EINVAL;

	switch (path) {
	case B52ISP_ISD_PIPE1:
	case B52ISP_ISD_MS1:
		reg = CMD_BUF_C;
		break;
	case B52ISP_ISD_PIPE2:
	case B52ISP_ISD_MS2:
		reg = CMD_BUF_C + PIPE_INFO_OFFSET;
		break;
	default:
		pr_err("%s: wrong path\n", __func__);
		return -EINVAL;
	}
	/*
	 * pipeinfo is only usded by process raw,
	 * so hardcode the num planes is 2.
	 * */
	if (ivb->vb.num_planes != 2) {
		pr_err("%s: no pipeline info\n", __func__);
		return -EINVAL;
	}

	if (ivb->vb.v4l2_planes[1].length < PIPE_INFO_SIZE) {
		pr_err("%s: pipeinfo buffer to short\n", __func__);
		return -EINVAL;
	}

	buf = ivb->va[1];
	for (i = 0; i < PIPE_INFO_SIZE; i++)
		b52_writeb(reg + i, buf[i]);

	return 0;
}

int __maybe_unused b52_read_debug_info(u8 *buffer)
{
	int i;

	for (i = 0; i < 0x40; i++)
		buffer[i] = b52_readb(0x4E000 + i);
	return 0;
}

int b52_get_metadata_len(int path)
{
	u32 base;
	switch (path) {
	case B52ISP_ISD_PIPE1:
		base = FW_P1_REG_BASE;
		break;
	case B52ISP_ISD_PIPE2:
		base = FW_P2_REG_BASE;
		break;
	default:
		pr_err("%s: wrong path\n", __func__);
		return -EINVAL;
	}

	return b52_readw(base + REG_FW_METADATA_LEN);
}
EXPORT_SYMBOL_GPL(b52_get_metadata_len);

static inline int b52_get_metadata_port(struct b52isp_cmd *cmd)
{
	u32 base;
	if (!(cmd->flags & BIT(CMD_FLAG_META_DATA)))
		return -EINVAL;

	switch (cmd->path) {
	case B52ISP_ISD_PIPE1:
	case B52ISP_ISD_MS1:
		base = FW_P1_REG_BASE;
		break;
	case B52ISP_ISD_PIPE2:
	case B52ISP_ISD_MS2:
		base = FW_P2_REG_BASE;
		break;
	default:
		pr_err("%s: wrong path\n", __func__);
		return -EINVAL;
	}

	cmd->meta_port = b52_readb(base + REG_FW_METADATA_PORT);
	return 0;
}

struct b52_reg_xlate_item {
	__u32	start;
	__u32	end;
	__u32	reg;
};

static struct b52_reg_xlate_item b52_pipe1_lookup_table[] = {
	{0x30200,	0x3039F,	0x30200},
	{0x31C00,	0x31CFF,	0x31C00},
	{0x31E00,	0x31EFF,	0x31E00},
	{0x32D00,	0x32DFF,	0x32D00},
	{0x33000,	0x335FF,	0x33000},
	{0x33C00,	0x33DFF,	0x33C00},
	{0x65000,	0x665FF,	0x65000},
	{0x66600,	0x667FF,	0x66600},
	{0x67000,	0x67FFF,	0x67000},
};

static struct b52_reg_xlate_item b52_pipe2_lookup_table[] = {
	{0x30200,	0x3039F,	0x30030},
	{0x31C00,	0x31CFF,	0x31D00},
	{0x31E00,	0x31EFF,	0x31F00},
	{0x32D00,	0x32DFF,	0x32D00},
	{0x33000,	0x335FF,	0x33600},
	{0x33C00,	0x33DFF,	0x33E00},
	{0x65000,	0x665FF,	0x68000},
	/* 0x66600 ~ 0x667FF don't exist on pipeline2 */
	{0x67000,	0x67FFF,	0x6A000},
};

int b52_rw_pipe_ctdata(int pipe_id, int write,
			struct b52_regval *entry, u32 nr_entry)
{
	struct b52_reg_xlate_item *table;
	long offset;
	u32 tab_sz, i, j;

	switch (pipe_id) {
	case 1:
		table = b52_pipe1_lookup_table;
		tab_sz = ARRAY_SIZE(b52_pipe1_lookup_table);
		break;
	case 2:
		table = b52_pipe2_lookup_table;
		tab_sz = ARRAY_SIZE(b52_pipe2_lookup_table);
		break;
	default:
		WARN_ON(1);
		return -EACCES;
	}

	for (i = 0; i < nr_entry; i++, entry++) {
		for (j = 0; j < tab_sz; j++) {
			offset = (long)entry->reg - (long)table[j].start;
			if ((offset >= 0) && (entry->reg < table[j].end))
				goto addr_match;
		}
		pr_err("%s: access to 0x%05X rejected\n", __func__, entry->reg);
		return -EACCES;
addr_match:
		if (write)
			b52_writeb(table[j].reg + offset, entry->val & 0xFF);
		/* read back always happens */
		entry->val = b52_readb(table[j].reg + offset);
	}
	return 0;
}

static int b52_basic_init(struct b52isp_cmd *cmd)
{
	int pipe_id;
#ifdef CONFIG_ISP_USE_TWSI3
	u16 af_init_val;
#endif
	struct v4l2_subdev *sd = cmd->sensor;

	if (!(cmd->flags & BIT(CMD_FLAG_INIT)))
		return 0;

	if (cmd->path == B52ISP_ISD_PIPE1)
		pipe_id = 0;
	else if (cmd->path == B52ISP_ISD_PIPE2)
		pipe_id = 1;
	else
		return -EINVAL;

	b52_set_aecagc_reg(sd, pipe_id);
	b52_set_vcm_reg(sd, pipe_id);

#ifdef CONFIG_ISP_USE_TWSI3
	/*FIXME: set af init val, let it stay at current pos*/
	b52_read_vcm_info(pipe_id, &af_init_val);
	b52_sensor_call(sensor, s_focus, af_init_val);
#endif

	return 0;
}

static int wait_cmd_done(int cmd)
{
	int ret = 0;
	u8 val;

	if (cmd == CMD_DOWNLOAD_FW) {
		ret = wait_for_completion_timeout(
			&b52isp_cmd_done, WAIT_CMD_TIMEOUT);
		if (!ret) {
			pr_err("%s: Firmware download failed\n", __func__);
			return -EINVAL;
		}
		return 0;
	}

	b52_writeb(CMD_REG0, cmd);
	ret = wait_for_completion_timeout(
		&b52isp_cmd_done, WAIT_CMD_TIMEOUT);
	if (!ret) {
		pr_err("%s: wait cmd %d failed\n", __func__, cmd);
		return -ETIMEDOUT;
	}

	if (b52isp_cmd_id != cmd) {
		pr_err("%s: cmd not match(%d:%d)\n", __func__,
			cmd, b52isp_cmd_id);
		return -EINVAL;
	}

	val = b52_readb(CMD_RESULT);
	if (val != CMD_SET_SUCCESS) {
		pr_err("%s: cmd %d result failed\n", __func__, cmd);
		return -EINVAL;
	}

	pr_debug("cmd %d success\n", cmd);
	return 0;
}

static int wait_cmd_capture_img_done(struct b52isp_cmd *cmd)
{
	u8 i;
	u8 val;
	int ret = 0;
	int ref_cnt = 0;
	struct b52isp_output *output = cmd->output;
	int num_planes = output[0].pix_mp.num_planes;

start_bracket:
	if (ref_cnt++ > 30) {
		pr_err("%s: retry CMD_CAPTUR_IMG over 30 times\n", __func__);
		return -EINVAL;
	} else if (ref_cnt) {
		ret = b52_fill_mmu_chnl(cmd->priv, output[0].buf[0], num_planes);
		if (ret < 0) {
			pr_err("%s: failed to fill mmu channel\n", __func__);
			return ret;
		}
	}

	b52_writeb(CMD_REG0, CMD_CAPTURE_IMG);

	ret = wait_for_completion_timeout(
		&b52isp_cmd_done, WAIT_CMD_TIMEOUT);
	if (!ret) {
		pr_err("wait CMD_CAPTURE_IMG failed, L%d\n", __LINE__);
		goto start_bracket;
	}

	/*
	 * Frame1: the address is set before send command
	 * Frame2: FW set 0x3359f with 1, wait host to clear the 0x3359f
	 * Frame3: FW set 0x3359f with 2, wait host to clear the 0x3359f
	 */
	for (i = 1; i < output[0].nr_buffer; i++) {
		if (b52isp_cmd_id != CMD_UPDATE_ADDR) {
			pr_err("%s: cmd not match(%d:%d)\n", __func__,
				   CMD_UPDATE_ADDR, b52isp_cmd_id);
			goto start_bracket;
		}

		val = b52_readb(REG_FW_IMG_ADDR_ID);
		if (i != val) {
			pr_err("%s: image address id error %d\n", __func__, val);
			goto start_bracket;
		}

		ret = b52_fill_mmu_chnl(cmd->priv, output[0].buf[i], num_planes);
		if (ret < 0)
			return ret;
		b52_writeb(REG_FW_IMG_ADDR_ID, 0);

		ret = wait_for_completion_timeout(
			&b52isp_cmd_done, WAIT_CMD_TIMEOUT);
		if (!ret) {
			pr_err("wait CMD_CAPTURE_IMG failed, L%d\n", __LINE__);
			goto start_bracket;
		}
	}

	if (b52isp_cmd_id != CMD_CAPTURE_IMG) {
		pr_err("%s:%d cmd not match(%d:%d)\n", __func__, __LINE__,
			CMD_CAPTURE_IMG, b52isp_cmd_id);
		goto start_bracket;
	}

	val = b52_readb(CMD_RESULT);
	if (val != CMD_SET_SUCCESS) {
		pr_err("CMD_CAPTURE_IMG result failed\n");
		goto start_bracket;
	}

	pr_debug("CMD_CAPTURE_IMG success\n");
	return 0;
}

void b52_set_base_addr(void __iomem *base)
{
	static int init;

	b52_base = base;

	if (init)
		return;

	b52_writeb(REG_TOP_SW_STANDBY, SW_STANDBY);
	b52_writeb(REG_TOP_CLK_RST1, 0xF0);
	usleep_range(500, 550);

	b52_writel(REG_ISP_INT_EN, INT_CMD_DONE);
	b52_writel(REG_ISP_INT_STAT, 0x0);

	init = 1;
}
EXPORT_SYMBOL_GPL(b52_set_base_addr);

void b52_set_sccb_clock_rate(u32 input_rate, u32 sccb_rate)
{
	/*
	 * sccb_rate = input_rate/(64*reg0x63600)
	 * value of 0x63600 = input_rate/sccb_rate/64
	 * use DIV_ROUND_UP to keep SCCB clk below I2C clk
	 */
	u8 val = DIV_ROUND_UP(input_rate, sccb_rate << 6);

	b52_writeb(SCCB_MASTER1_REG_BASE + REG_SCCB_SPEED, val);
	b52_writeb(SCCB_MASTER2_REG_BASE + REG_SCCB_SPEED, val);
}
EXPORT_SYMBOL_GPL(b52_set_sccb_clock_rate);

int b52_load_fw(struct device *dev, void __iomem *base, int enable,
					int pwr, int fw_version)
{
	int ret, i;
	const struct firmware *fw;
	__u32 *src, *dst;
	const char *fw_name;
	if (!pwr)
		return 0;

	if (!dev || !base) {
		pr_err("%s param error\n", __func__);
		return -EINVAL;
	}

	b52_base = base;

	if (!enable)
		return 0;

	b52_writeb(REG_TOP_SW_STANDBY, SW_STANDBY);
	b52_writeb(REG_TOP_CLK_RST1, 0xF0);
	usleep_range(500, 550);
	b52_writeb(REG_TOP_CORE_CTRL1_L, 0x40);

	b52_writel(REG_ISP_INT_EN, INT_CMD_DONE | (7 << 24));
	b52_writel(REG_ISP_INT_STAT, 0x0);

	b52_writeb(0x63042, 0xf1);
	switch (fw_version) {
	case 1:
		fw_name = FW_FILE_V324;
		break;
	case 2:
		fw_name = FW_FILE_V325;
		break;
	case 3:
		fw_name = FW_FILE_V326;
		break;
	default:
		fw_name = NULL;
	}
	ret = request_firmware(&fw, fw_name, dev);
	if (ret < 0) {
		pr_err("request_firmware failed\n");
		return ret;
	}

	src = (__u32 *)fw->data;
	dst = b52_base;
	for (i = 0; i < fw->size / 4; i++)
		dst[i] = swab32(src[i]);
	for (i *= 4; i < fw->size; i++)
		b52_writeb(0, fw->data[i]);

	release_firmware(fw);

	b52_writeb(0x63042, 0xf0);

	ret = wait_cmd_done(CMD_DOWNLOAD_FW);
	if (ret < 0)
		return ret;

	b52_writeb(REG_TOP_CORE_CTRL0_H, 0x80);
	b52_writeb(REG_TOP_CORE_CTRL4_H, 0x0);
	b52_writeb(REG_TOP_CORE_CTRL4_L, 0x0);

	/* default Pipeline clock is 312M, the sccb clock is 400K */
	b52_set_sccb_clock_rate(312000000, 400000);
	pr_err("B52ISP version: HW %d.%d, SWM %d.%d, revision %d\n",
	       b52_readb(REG_HW_VERSION), b52_readb(REG_HW_VERSION + 1),
	       b52_readb(REG_SWM_VERSION), b52_readb(REG_SWM_VERSION + 1),
	       b52_readw(REG_FW_VERSION));
	return 0;
}
EXPORT_SYMBOL_GPL(b52_load_fw);

static int b52_convert_input_fmt(u32 fmt, u16 *val, u8 *bpp)
{
	u16 __val;
	u8 __bpp;
	/* fmt [2:0]:
	 * 0 for RAW8,
	 * 1 for RAW10, the length is 16bit if from DDR
	 * 2 for RAW12,
	 * 3 for RAW14,
	 * 4 for YUV422,
	 * 5 for RGB565,
	 * 6 for RGB888
	 */
	switch (fmt) {
	case V4L2_PIX_FMT_SBGGR8:
	case V4L2_PIX_FMT_SGBRG8:
	case V4L2_PIX_FMT_SGRBG8:
	case V4L2_PIX_FMT_SRGGB8:
		__val = 0;
		__bpp = 8;
		break;
	case V4L2_PIX_FMT_SBGGR10:
	case V4L2_PIX_FMT_SGBRG10:
	case V4L2_PIX_FMT_SGRBG10:
	case V4L2_PIX_FMT_SRGGB10:
		__val = 1;
		__bpp = 10;
		break;
	case V4L2_PIX_FMT_SBGGR16:
	case V4L2_PIX_FMT_SGBRG16:
	case V4L2_PIX_FMT_SGRBG16:
	case V4L2_PIX_FMT_SRGGB16:
		__val = 1;
		__bpp = 16;
		break;
	case V4L2_PIX_FMT_SBGGR12:
	case V4L2_PIX_FMT_SGBRG12:
	case V4L2_PIX_FMT_SGRBG12:
	case V4L2_PIX_FMT_SRGGB12:
		__val = 2;
		__bpp = 12;
		break;
#if 0
	case V4L2_PIX_FMT_SBGGR14:
	case V4L2_PIX_FMT_SGBRG14:
	case V4L2_PIX_FMT_SGRBG14:
	case V4L2_PIX_FMT_SRGGB14:
		__val = 3;
		__bpp = 14;
		break;
#endif
	case V4L2_PIX_FMT_YVYU:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_VYUY:
		__val = 4;
		__bpp = 16;
		break;
	case V4L2_PIX_FMT_RGB565:
		__val = 5;
		__bpp = 18;
		break;
	case V4L2_PIX_FMT_RGB32:
		__val = 6;
		__bpp = 32;
		break;
	default:
		pr_err("input format not support: %d\n", fmt);
		return -EINVAL;
	}

	if (val)
		*val = __val;

	if (bpp)
		*bpp = __bpp;

	return 0;
}

static int b52_convert_output_fmt(u32 fmt, u16 *val, u8 *bpp)
{
	/* fmt [2:0]:
	 * 0 for RAW8,
	 * 1 for RAW10, but ISP output 16 bit
	 * 2 for RAW12,
	 * 3 for RAW14,
	 * 4 for YUV422,
	 * 5 for NV12,
	 * 6 for RGB565,
	 * 7 for RGB888
	 * 8 for YUV420
	 */
	switch (fmt) {
	case V4L2_PIX_FMT_SBGGR8:
	case V4L2_PIX_FMT_SGBRG8:
	case V4L2_PIX_FMT_SGRBG8:
	case V4L2_PIX_FMT_SRGGB8:
		*val = 0;
		*bpp = 8;
		break;
#if 0
	/*Since ISP output 16 bit, use the sbggr16*/
	case V4L2_PIX_FMT_SBGGR10:
	case V4L2_PIX_FMT_SGBRG10:
	case V4L2_PIX_FMT_SGRBG10:
	case V4L2_PIX_FMT_SRGGB10:
		*val = 1;
		*bpp = 10;
		break;
#endif
	case V4L2_PIX_FMT_SBGGR12:
	case V4L2_PIX_FMT_SGBRG12:
	case V4L2_PIX_FMT_SGRBG12:
	case V4L2_PIX_FMT_SRGGB12:
		*val = 2;
		*bpp = 12;
		break;
#if 0
	case V4L2_PIX_FMT_SBGGR14:
	case V4L2_PIX_FMT_SGBRG14:
	case V4L2_PIX_FMT_SGRBG14:
	case V4L2_PIX_FMT_SRGGB14:
		*val = 3;
		*bpp = 14;
		break;
#endif
	case V4L2_PIX_FMT_SBGGR16:
	case V4L2_PIX_FMT_SGBRG16:
	case V4L2_PIX_FMT_SGRBG16:
	case V4L2_PIX_FMT_SRGGB16:
		*val = 1;
		*bpp = 16;
		break;
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_YVYU:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_VYUY:
		*val = 4;
		*bpp = 16;
		break;
	case V4L2_PIX_FMT_NV12M:
	case V4L2_PIX_FMT_NV21M:
		*val = 5;
		*bpp = 12;
		break;
	case V4L2_PIX_FMT_RGB565:
		*val = 6;
		*bpp = 18;
		break;
	case V4L2_PIX_FMT_RGB32:
		*val = 7;
		*bpp = 32;
		break;
	case V4L2_PIX_FMT_YUV420M:
	case V4L2_PIX_FMT_YVU420M:
		*val = 8;
		*bpp = 12;
		break;
	default:
		pr_err("output format not support: %d\n", fmt);
		return -EINVAL;
	}

	return 0;
}

struct b52_input_cfg {
	u16 type;
	u16 width;
	u16 height;
};

static int b52_calc_input_cfg(struct b52_input_cfg *cfg,
		struct v4l2_pix_format *fmt, int src)
{
	int ret = 0;
	ret = b52_convert_input_fmt(fmt->pixelformat, &cfg->type, NULL);
	if (ret)
		return ret;

	cfg->type |= ISP_ENABLE;
	cfg->width = fmt->width;
	cfg->height = fmt->height;

	/* FIXME: src*/
	switch (src) {
	case CMD_SRC_SNSR:
		cfg->type |= SRC_PRIMARY_MIPI;
		break;
	case CMD_SRC_AXI:
		cfg->type |= SRC_MEMORY;
		break;
	default:
		pr_err("%s: wrong input type", __func__);
		ret = -EINVAL;
		break;
	}

	pr_debug("%s:type 0x%x, w %d, h %d\n", __func__,
			cfg->type, cfg->width, cfg->height);
	return ret;
}

static inline void b52_set_input(struct b52_input_cfg *cfg)
{
	b52_writew(ISP_INPUT_TYPE, cfg->type);
	b52_writew(ISP_INPUT_WIDTH, cfg->width);
	b52_writew(ISP_INPUT_HEIGHT, cfg->height);
}

static int b52_cfg_input(struct v4l2_pix_format *fmt, int src)
{
	int ret = 0;
	struct b52_input_cfg cfg;
	memset(&cfg, 0, sizeof(cfg));

	if (!fmt) {
		pr_err("%s param error\n", __func__);
		return -EINVAL;
	}

	ret = b52_calc_input_cfg(&cfg, fmt, src);
	if (ret)
		goto out;

	b52_set_input(&cfg);
out:
	return ret;
}

struct b52_idi_cfg {
	u16 scale;
	u16 width;
	u16 height;
	u16 h_start;
	u16 v_start;
};

static int b52_calc_idi_cfg(struct b52_idi_cfg *cfg,
		u32 src_width, struct v4l2_rect *r)
{
	if (!src_width) {
		pr_err("%s: src width is 0\n", __func__);
		return -EINVAL;
	}

	/* FIXME: need to refine */
	switch (r->width * 100 / src_width) {
	case (3 * 100 / 5):
		cfg->scale = IDI_SCALE_DOWN_EN | IDI_SD_3_5;
		break;
	case (1 * 100 / 2):
		cfg->scale = IDI_SCALE_DOWN_EN | IDI_SD_1_2;
		break;
	case (2 * 100 / 5):
		cfg->scale = IDI_SCALE_DOWN_EN | IDI_SD_2_5;
		break;
	case (1 * 100 / 3):
		cfg->scale = IDI_SCALE_DOWN_EN | IDI_SD_1_3;
		break;
	case (2 * 100 / 3):
		cfg->scale = IDI_SCALE_DOWN_EN | IDI_SD_2_3;
		break;
	case (1 * 100 / 1):
		cfg->scale = 0;
		break;
	default:
		cfg->scale = 0;
		pr_err("%s:wrong calc scale, src_w %x, des_w %x\n",
				__func__, src_width, r->width);
		break;
	}

	cfg->width = r->width;
	cfg->height = r->height;
	cfg->h_start = r->left;
	cfg->v_start = r->top;

	pr_debug("%s:scale 0x%x, w %d, h %d, h_s %d, v_s %d\n",
		__func__, cfg->scale, cfg->width, cfg->height,
		cfg->h_start, cfg->v_start);
	return 0;
}
static inline void b52_set_idi(struct b52_idi_cfg *cfg)
{
	b52_writew(ISP_IDI_CTRL, cfg->scale);
	b52_writew(ISP_IDI_OUTPUT_WIDTH, cfg->width);
	b52_writew(ISP_IDI_OUTPUT_HEIGHT, cfg->height);
	b52_writew(ISP_IDI_OUTPUT_H_START, cfg->h_start);
	b52_writew(ISP_IDI_OUTPUT_V_START, cfg->v_start);
}
static int b52_cfg_idi(struct v4l2_pix_format *fmt, struct v4l2_rect *r)
{
	int ret = 0;
	struct b52_idi_cfg cfg;
	memset(&cfg, 0, sizeof(cfg));

	if (!fmt || !r) {
		pr_err("%s param error\n", __func__);
		return -EINVAL;
	}

	ret = b52_calc_idi_cfg(&cfg, fmt->width, r);
	if (ret)
		goto out;
	b52_set_idi(&cfg);
out:
	return ret;
}

struct b52_output_cfg {
	u8 port;
	u16 fmt;
	u16 width;
	u16 height;
	u16 mem_w;
	u16 mem_uv_w;
};

static int b52_bit_cnt(u8 n)
{
	int cnt = 0;
	while (n != 0) {
		n = n & (n-1);
		cnt++;
	}
	return cnt;
}

static int b52_calc_output_cfg(struct b52_output_cfg *cfg,
		struct v4l2_pix_format_mplane *fmt, int port, int no_zoom)
{
	int ret;
	u8 bpp;
	if (port > MAX_OUTPUT_PORT || port < 0) {
		pr_err("%s param error: %d\n", __func__, port);
		return -EINVAL;
	}

	cfg->port = port;
	ret = b52_convert_output_fmt(fmt->pixelformat, &cfg->fmt, &bpp);
	if (ret)
		return ret;

	if (no_zoom)
		cfg->fmt |= ISP_OUTPUT_NO_ZOOM;

	cfg->width = fmt->width;
	cfg->height = fmt->height;

	switch (fmt->num_planes) {
	case 1:
		cfg->mem_w = fmt->plane_fmt[0].bytesperline * 8 / bpp;
		cfg->mem_uv_w = 0;
		break;
	case 2:
	case 3:
		cfg->mem_w = fmt->plane_fmt[0].bytesperline;
		cfg->mem_uv_w = fmt->plane_fmt[1].bytesperline;
		break;
	default:
		pr_err("%s: wrong num %d\n", __func__, fmt->num_planes);
		return -EINVAL;
	}

	pr_debug("%s:port %d, fmt %d, w %d, h %d, mem_w %d, mem_uv %d\n",
		__func__, cfg->port, cfg->fmt, cfg->width, cfg->height,
		cfg->mem_w, cfg->mem_uv_w);

	return 0;
}

static inline void b52_set_output(struct b52_output_cfg *cfg)
{
	u32 offset;
	offset = cfg->port * ISP_OUTPUT_OFFSET;

	b52_writew(ISP_OUTPUT1_TYPE + offset, cfg->fmt);
	b52_writew(ISP_OUTPUT1_WIDTH + offset, cfg->width);
	b52_writew(ISP_OUTPUT1_HEIGHT + offset, cfg->height);
	b52_writew(ISP_OUTPUT1_MEM_WIDTH + offset, cfg->mem_w);
	b52_writew(ISP_OUTPUT1_MEM_UV_WIDTH + offset, cfg->mem_uv_w);
}

static int b52_cfg_mac_fmt(int port,
		struct v4l2_pix_format_mplane *fmt)
{
	int mac_id = port >> 1;
	int port_id = port % 2;
	u32 reg;
	int val = 0;

	if (port_id == 0)
		reg = REG_MAC_W_SWITCH_CTRL0;
	else
		reg = REG_MAC_W_SWITCH_CTRL1;

	reg =  mac_base[mac_id] + reg;

	switch (fmt->pixelformat) {
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_NV12M:
	case V4L2_PIX_FMT_YUV420M:
		val = 0;
		break;
	case V4L2_PIX_FMT_VYUY:
	case V4L2_PIX_FMT_NV21M:
	case V4L2_PIX_FMT_YVU420M:
		val = 8;
		break;
	case V4L2_PIX_FMT_YUYV:
		val = 4;
		break;
	default:
		pr_debug("format not need switch: %08x", fmt->pixelformat);
		break;
	}

	b52_writeb(reg, val);

	return 0;
}

static int b52_cfg_output(struct b52isp_output *output, u8 bit_map)
{
	int i, j;
	int ret = 0;
	u8 bit = 0;
	unsigned long map = bit_map;
	struct b52_output_cfg cfg;
	memset(&cfg, 0, sizeof(cfg));

	for (i = 0, j = 0; i < B52_OUTPUT_PER_PIPELINE; i++) {
		if (output[i].vnode == NULL)
			continue;
		ret = b52_calc_output_cfg(&cfg, &output[i].pix_mp, j, output[i].no_zoom);
		if (ret)
			goto out;

		b52_set_output(&cfg);
		bit = find_first_bit(&map, BITS_PER_LONG);
		map &= ~(1 << bit);
		b52_cfg_mac_fmt(bit, &output[i].pix_mp);
		j++;
	}

	b52_writew(ISP_OUTPUT_PORT_SEL, bit_map);

out:
	return ret;
}

struct b52_rd_cfg {
	u8 port;
	u16 width;
	u16 height;
	u16 pitch;
};

static int b52_calc_rd_cfg(struct b52_rd_cfg *cfg,
		struct v4l2_pix_format *fmt, u8 port)
{
	u8 bpp;
	int ret;

	if (port >= MAX_RD_PORT) {
		pr_err("%s param error\n", __func__);
		return -EINVAL;
	}

	ret = b52_convert_input_fmt(fmt->pixelformat, NULL, &bpp);
	if (ret)
		return ret;

	cfg->port   = (1 << port);
	cfg->width  = fmt->width;
	cfg->height = fmt->height;
	cfg->pitch  = fmt->bytesperline * 8 / bpp;

	pr_debug("%s:port %d,w %d, h %d, pitch %d\n", __func__,
		cfg->port, cfg->width, cfg->height, cfg->pitch);

	return 0;
}

static inline void b52_set_rd(struct b52_rd_cfg *cfg)
{
	b52_writew(ISP_RD_PORT_SEL, cfg->port);
	b52_writew(ISP_RD_WIDTH, cfg->width);
	b52_writew(ISP_RD_HEIGHT, cfg->height);
	b52_writew(ISP_RD_MEM_WIDTH, cfg->pitch);
}

static int b52_cfg_rd(struct v4l2_pix_format *fmt,
			u8 port, dma_addr_t addr1, dma_addr_t addr2)
{
	int ret = 0;
	struct b52_rd_cfg cfg;
	memset(&cfg, 0, sizeof(cfg));

	if (!fmt) {
		pr_err("%s param error\n", __func__);
		return -EINVAL;
	}

	ret = b52_calc_rd_cfg(&cfg, fmt, port);
	if (ret)
		goto out;

	b52_set_rd(&cfg);

	if (addr1)
		b52_writel(ISP_INPUT_ADDR1, addr1);

	if (addr2)
		b52_writel(ISP_INPUT_ADDR2, addr2);

out:
	return ret;
}

struct b52_sensor_cfg {
	u16 expo_ratio;
	u16 max_expo;
	u16 min_expo;
	u16 max_frationalexpo;
	u16 min_frationalexpo;
	u16 max_gain;
	u16 min_gain;
	u16 vts;
	u16 hts;
	u16 band_50hz;
	u16 band_60hz;
};

static inline void b52_set_expo_ratio(u16 raio)
{
	b52_writew(ISP_EXPO_RATIO, raio);
}

static inline void b52_set_expo_range(u16 max, u16 min)
{
	b52_writew(ISP_MIN_EXPO, min);
	b52_writew(ISP_MAX_EXPO, max);
}

static inline void b52_set_fration_expo_range(u16 max, u16 min)
{
	b52_writew(REG_FW_AEC_MAX_FRATIONAL_EXP, max);
}

static inline void b52_set_gain_range(u16 max, u16 min)
{
	b52_writew(ISP_MIN_GAIN, min);
	b52_writew(ISP_MAX_GAIN, max);
}

static inline void b52_set_sensor_vts(u16 vts)
{
	b52_writew(ISP_VTS, vts);
}

static inline void b52_set_sensor_hts(u16 hts)
{
	b52_writew(ISP_HTS, hts);
}

static inline void b52_set_sensor_band(u16 band_50hz, u16 band_60hz)
{
	b52_writew(ISP_50HZ_BAND, band_50hz);
	b52_writew(ISP_60HZ_BAND, band_60hz);
}

static inline void b52_set_bracket_ratio(u16 ratio1, u16 ratio2)
{
	b52_writew(ISP_BRACKET_RATIO1, ratio1);
	b52_writew(ISP_BRACKET_RATIO2, ratio2);
}

static inline void b52_set_zoom_in_ratio(u16 ratio)
{
	b52_writew(ISP_ZOOM_IN_RATIO, ratio);
}

static int b52_calc_sensor_cfg(struct v4l2_subdev *sd,
		struct b52_sensor_cfg *cfg)
{
	struct b52_sensor *sensor = to_b52_sensor(sd);
	if (!sensor) {
		pr_err("%s: no sensor attached\n", __func__);
		return -EINVAL;
	}

	b52_sensor_call(sensor, g_param_range, B52_SENSOR_EXPO,
			&cfg->min_expo, &cfg->max_expo);

	b52_sensor_call(sensor, g_param_range, B52_SENSOR_FRACTIONALEXPO,
			&cfg->min_frationalexpo, &cfg->max_frationalexpo);

	b52_sensor_call(sensor, g_param_range, B52_SENSOR_GAIN,
			&cfg->min_gain, &cfg->max_gain);

	b52_sensor_call(sensor, g_param_range, B52_SENSOR_REQ_VTS,
			&cfg->vts, &cfg->vts);

	b52_sensor_call(sensor, g_param_range, B52_SENSOR_REQ_HTS,
			&cfg->hts, &cfg->hts);

	b52_sensor_call(sensor, g_band_step,
			&cfg->band_50hz, &cfg->band_60hz);
	/* FIXME: replace with really value*/
	cfg->expo_ratio = 0x0100;

	pr_debug("%s:expo[%x,(%x,%x)], gain[%x,%x], vts %x, band(%x,%x)\n",
		__func__, cfg->expo_ratio, cfg->min_expo, cfg->max_expo,
		cfg->min_gain, cfg->max_gain, cfg->vts,
		cfg->band_50hz, cfg->band_60hz);

	return 0;
}

static void b52_set_sensor(struct b52_sensor_cfg *cfg)
{
	b52_set_expo_ratio(cfg->expo_ratio);
	b52_set_expo_range(cfg->max_expo, cfg->min_expo);
	b52_set_fration_expo_range(cfg->max_frationalexpo,
				cfg->min_frationalexpo);
	b52_set_gain_range(cfg->max_gain, cfg->min_gain);
	b52_set_sensor_vts(cfg->vts);
	b52_set_sensor_hts(cfg->hts);
	b52_set_sensor_band(cfg->band_50hz, cfg->band_60hz);
}

static int b52_cfg_isp(struct v4l2_subdev *sd)
{
	int ret;
	struct b52_sensor_cfg cfg;
	memset(&cfg, 0, sizeof(cfg));

	ret = b52_calc_sensor_cfg(sd, &cfg);
	if (ret)
		goto out;
	b52_set_sensor(&cfg);
out:
	return ret;
}

static int b52_cfg_isp_ms(const struct b52_sensor_data *data, int path)
{
	struct b52_sensor_cfg cfg;
	int base = FW_P1_REG_BASE;

	memset(&cfg, 0, sizeof(cfg));
	cfg.min_expo = data->expo_range.min;
	cfg.max_expo = data->expo_range.max;
	cfg.min_frationalexpo =  data->frationalexp_range.min;
	cfg.max_frationalexpo =  data->frationalexp_range.max;
	cfg.min_gain = data->gain_range[B52_SENSOR_AG].min;
	cfg.max_gain = data->gain_range[B52_SENSOR_AG].max;
	cfg.min_gain = cfg.min_gain*data->gain_range[B52_SENSOR_DG].min/B52_GAIN_UNIT;
	cfg.max_gain = cfg.max_gain*data->gain_range[B52_SENSOR_DG].max/B52_GAIN_UNIT;
	cfg.vts = data->vts_range.min;
	cfg.hts = data->res[0].hts;
	/* FIXME: replace with really value*/
	cfg.expo_ratio = 0x0100;
	cfg.band_50hz = 0;
	cfg.band_60hz = 0;

	b52_set_sensor(&cfg);

	if (path == B52ISP_ISD_MS1)
		base = FW_P1_REG_BASE;
	else if (path == B52ISP_ISD_MS2)
		base = FW_P1_REG_BASE + FW_P1_P2_OFFSET;
	else
		pr_err("%s: path is error\n", __func__);
	/*
	 * vendor recommands to use Q4,
	 * ISP also uses it to calculate stretch gain.
	 */
	b52_writeb(base + REG_FW_SSOR_GAIN_MODE, SSOR_GAIN_Q4);

	pr_debug("%s:expo[%x,(%x,%x)], gain[%x,%x], vts %x, band(%x,%x)\n",
		__func__, cfg.expo_ratio, cfg.min_expo, cfg.max_expo,
		cfg.min_gain, cfg.max_gain, cfg.vts,
		cfg.band_50hz, cfg.band_60hz);
	return 0;
}

static inline void b52_zoom_ddr_qos(int zoom)
{
	s32 freq;
	if (zoom < 0x300)
		freq = 312000;
	else if (zoom < 0x380) /* 0x380 is 3.5x zoom */
		freq = 416000;
	else
		freq = 528000;
	b52isp_set_ddr_qos(freq);
}

static int b52_cfg_zoom(struct v4l2_rect *src,
			struct v4l2_rect *dst)
{
	u32 ratio;
	if (!src || !dst || !src->width) {
		pr_err("%s: parameter error\n", __func__);
		return -EINVAL;
	}

	ratio = dst->width / src->width;
	if (ratio > ZOOM_RATIO_MAX || ratio < ZOOM_RATIO_MIN) {
		pr_err("b52: zoom ratio error %x, use default value\n", ratio);
		ratio = 0x100;
	}
	b52_set_zoom_in_ratio(ratio);

	b52_zoom_ddr_qos(ratio);

	return 0;
}

static int b52_cfg_capture(struct b52isp_output *output, u8 cnt)
{
	int i;
	int j;
	int p;
	int total = 0;

	for (i = 0; i < cnt; i++) {
		for (j = 0; j < output[i].nr_buffer; j++) {
			if (!output[i].buf[j]) {
				pr_err("%s: buf %d null\n", __func__, j);
				return -EINVAL;
			}
			if (total > 2) {
				pr_err("%s: total num %d error,i %d,j %d\n",
						__func__, total, i, j);
				return -EINVAL;
			}

			for (p = 0; p < output[i].pix_mp.num_planes; p++)
				b52_writel(ISP_OUTPUT_ADDR1 + total*12 + p*4,
				(u32)output[i].buf[j]->ch_info[p].daddr);
			total++;
		}
	}

	return 0;
}



struct b52_hdr_cfg {
	u16 ctrl;
	u16 l_expo;
	u16 s_expo;
	u16 l_gain;
	u16 s_gain;
};

static int b52_calc_hdr_cfg(struct b52_hdr_cfg *cfg)
{
	/*FIXME*/
	cfg->ctrl   = 0x0005;
	cfg->l_expo = 0x01f0;
	cfg->s_expo = 0x0020;
	cfg->l_gain = 0x0010;
	cfg->s_gain = 0x0010;

	pr_debug("%s:ctrl %x, expo(%x,%x), gain(%x,%x)\n", __func__,
		cfg->ctrl, cfg->s_expo, cfg->l_expo, cfg->s_gain, cfg->l_gain);

	return 0;
}

static inline void b52_set_hdr(struct b52_hdr_cfg *cfg)
{
	b52_writew(ISP_HDR_CTRL, cfg->ctrl);
	b52_writew(ISP_HDR_L_EXPO, cfg->l_expo);
	b52_writew(ISP_HDR_S_EXPO, cfg->s_expo);
	b52_writew(ISP_HDR_L_GAIN, cfg->l_gain);
	b52_writew(ISP_HDR_S_GAIN, cfg->s_gain);
}

static int b52_cfg_hdr(void)
{
	int ret = 0;
	struct b52_hdr_cfg cfg;
	memset(&cfg, 0, sizeof(cfg));

	ret = b52_calc_hdr_cfg(&cfg);
	if (ret)
		goto out;

	b52_set_hdr(&cfg);
out:
	return ret;
}


static void b52_dump_regs(char *name, u32 start, u32 end, u8 len)
{
	if (!name)
		return;

	pr_info("start to dump %s\n", name);

	if (len == 1)
		for (; start <= end;  start += len)
			pr_info("REG[0x%08X]-->0x%04X\n",
					start, b52_readb(start));
	else if (len == 2)
		for (; start <= end;  start += len)
			pr_info("REG[0x%08X]-->0x%04X\n",
					start, b52_readw(start));
	else if (len == 4)
		for (; start <= end;  start += len)
			pr_info("REG[0x%08X]-->0x%04X\n",
					start, b52_readl(start));

	pr_info("end to dump %s\n\n", name);
}

struct dump_ctx {
	char *name;
	u32 start;
	u32 end;
	u8 len;
};

static void b52_dump_cmd_setting(void)
{
	int i;
	struct dump_ctx ctx[] = {
		{"cmd regs cfg",          0x63900, 0x63911, 1},
		{"input configuration",   0x34000, 0x3400f, 2},
		{"output configuration",  0x34020, 0x34049, 2},
		{"read configuration",    0x34060, 0x34067, 2},
		{"ISP configuration",     0x34070, 0x34085, 2},
		{"HDR configuration",     0x34090, 0x34099, 2},
		{"capture configuration", 0x340a0, 0x340cb, 2},
		{"output info pipe 1",    0x34200, 0x34213, 2},
		{"output info pipe 2",    0x34280, 0x34293, 2},
		{"input info pipe 1",     0x34300, 0x34313, 2},
		{"input info pipe 2",     0x34380, 0x34393, 2},
		{"command buffer D",      0x34400, 0x3440f, 1},
	};

	for (i = 0; i < ARRAY_SIZE(ctx); i++)
		b52_dump_regs(ctx[i].name,
			ctx[i].start, ctx[i].end, ctx[i].len);
}

static void __maybe_unused b52_dump_debug_regs(void)
{
	int i;

	struct dump_ctx ctx[] = {
		{"idi1 setting",          0x63c00, 0x63cff, 1},
		{"ISP1 setting",          0x65010, 0x65017, 1},
		{"ISP1 counter",          0x65037, 0x6503a, 1},
		{"yuv crop no scale",     0x650f0, 0x650fb, 1},
		{"yuv crop scale1",       0x65e00, 0x65e0b, 1},
		{"yuv crop scale2",       0x67100, 0x6710b, 1},
		{"yuv crop scale2",       0x67700, 0x6770b, 1},
		{"chanale1 scale1",       0x65014, 0x65035, 1},
		{"chanale2 scale2",       0x65052, 0x6505f, 1},
		{"chanale3 scale3",       0x6507a, 0x65088, 1},
		{"mac 1",                 0x63b00, 0x63bd2, 1},
	};

	for (i = 0; i < ARRAY_SIZE(ctx); i++)
		b52_dump_regs(ctx[i].name,
			ctx[i].start, ctx[i].end, ctx[i].len);
}

static void __maybe_unused b52_dump_isp_cnt(void)
{
	u32 val;

	val = b52_readw(0x63c62);
	pr_info("idi cis_pcnt: %d\n", val);
	val = b52_readw(0x63c64);
	pr_info("idi pipe pcnt: %d\n", val);
	val = b52_readw(0x63c66);
	pr_info("idi pipe lcnt: %d\n", val);
	val = b52_readw(0x63c68);
	pr_info("idi mac pcnt: %d\n", val);
	val = b52_readw(0x63c6a);
	pr_info("idi mac lcnt: %d\n", val);

	val = b52_readw(0x65037);
	pr_info("isp input pcnt: %d\n", val);
	val = b52_readw(0x65039);
	pr_info("isp input lcnt: %d\n", val);
	val = b52_readw(0x650f8);
	pr_info("isp no scale output pcnt: %d\n", val);
	val = b52_readw(0x650fa);
	pr_info("isp no scale output lcnt: %d\n", val);
	val = b52_readw(0x65e08);
	pr_info("isp scale1 output pcnt: %d\n", val);
	val = b52_readw(0x65e0a);
	pr_info("isp scale1 output lcnt: %d\n", val);
}

static void __maybe_unused b52_dump_mac_setting(void)
{
	int i;
	struct dump_ctx ctx[] = {
		{"mac setting",	0x63b00, 0x63bd2, 1},
	};

	for (i = 0; i < ARRAY_SIZE(ctx); i++)
		b52_dump_regs(ctx[i].name,
			ctx[i].start, ctx[i].end, ctx[i].len);
}

static int __maybe_unused b52_dump_memory(char *file_name,
		char *buffer, u32 len)
{
	struct file *fp;
	mm_segment_t fs;
	loff_t pos;

	fp = filp_open(file_name, O_RDWR | O_CREAT, 0644);
	if (IS_ERR(fp)) {
		pr_err("create file error\n");
		return -1;
	}
	fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;
	vfs_write(fp, buffer, len, &pos);

	filp_close(fp, NULL);
	set_fs(fs);

	return 0;
}

struct b52_pipe_info {
	u16 expo;
	u16 gain;
	u16 b_gain;
	u16 g_gain;
	u16 af_pos;
	u16 zoom_in_ratio;
	u16 stretch_gain;
	u16 sll;
};

#define PIPE_INFO_OFFSET   (0x80)
int b52_get_pipe_info(struct b52_pipe_info *info, u8 id)
{
	int i;
	u32 base;

	if (!info || id > MAX_PIPE_NUM) {
		pr_err("%s param error\n", __func__);
		return -EINVAL;
	}

	base = CMD_BUF_B + (id - 1) * PIPE_INFO_OFFSET;
	for (i = 0; i < sizeof(*info); i += 2)
		((u16 *)info)[i] = b52_readw(base + i);

	return 0;
}

struct b52_sccb {
	u8 speed;
	u8 slave_id;
	u8 addr_h;
	u8 addr_l;
	u8 o_data_h;
	u8 o_data_l;
	u8 len_ctrl;
	u8 i_data_h;
	u8 i_data_l;
	u8 cmd;
};

#define ISP_READ_I2C_LOOP_CNT 50
static inline void b52_isp_wait_sccb_idle(u32 base, u32 us)
{
	int cnt;
	u8 val;

	cnt = ISP_READ_I2C_LOOP_CNT;
	do {
		usleep_range(us, us + 10);
		val = b52_readb(base + REG_SCCB_STATUS) & SCCB_ST_BUSY;
	} while ((val != SCCB_ST_IDLE) && (cnt-- > 0));
}

static int __b52_isp_read_i2c(struct b52_sccb *sccb, u8 pos)
{
	u32 base = 0;
	int cnt = 0;

	if (!sccb) {
		pr_err("%s, %d\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (pos == B52_SENSOR_I2C_PRIMARY_MASTER)
		base = SCCB_MASTER1_REG_BASE;
	else if (pos == B52_SENSOR_I2C_SECONDARY_MASTER)
		base = SCCB_MASTER2_REG_BASE;
	else {
		pr_err("%s pos error\n", __func__);
		return -EINVAL;
	}

	do {
		cnt++;
		b52_writeb(base + REG_SCCB_SPEED, sccb->speed);
		b52_writeb(base + REG_SCCB_SLAVE_ID, sccb->slave_id);
		b52_writeb(base + REG_SCCB_ADDRESS_H, sccb->addr_h);
		b52_writeb(base + REG_SCCB_ADDRESS_L, sccb->addr_l);
		b52_writeb(base + REG_SCCB_2BYTE_CONTROL, sccb->len_ctrl);

		if (cnt > ISP_READ_I2C_LOOP_CNT) {
			pr_err("%s: w/r ISP register failed\n", __func__);
			return -EAGAIN;
		}
	} while ((b52_readb(base + REG_SCCB_SPEED) != sccb->speed) ||
		(b52_readb(base + REG_SCCB_SLAVE_ID) != sccb->slave_id) ||
		(b52_readb(base + REG_SCCB_ADDRESS_H) != sccb->addr_h) ||
		(b52_readb(base + REG_SCCB_ADDRESS_L) != sccb->addr_l) ||
		(b52_readb(base + REG_SCCB_2BYTE_CONTROL) != sccb->len_ctrl));

	pr_debug("%s: read loop %d\n", __func__, cnt);

	b52_isp_wait_sccb_idle(base, 100);
	b52_writeb(base + REG_SCCB_COMMAND, sccb->cmd);
	b52_isp_wait_sccb_idle(base, 100);
	b52_writeb(base + REG_SCCB_COMMAND, 0xF9);
	b52_isp_wait_sccb_idle(base, 100);

	sccb->i_data_h = b52_readb(base + REG_SCCB_INPUT_DATA_H);
	sccb->i_data_l = b52_readb(base + REG_SCCB_INPUT_DATA_L);

	return 0;
}

int b52_isp_read_i2c(const struct b52_sensor_i2c_attr *attr,
		u16 reg, u16 *val, u8 pos)
{
	int ret;
	struct b52_sccb sccb;

	if (!attr) {
		pr_err("%s, %d\n", __func__, __LINE__);
		return -EINVAL;
	}

	memset(&sccb, 0, sizeof(sccb));

	sccb.speed    = 0x12;/*default value*/
	sccb.slave_id = (attr->addr << 1);
	sccb.addr_h   = (reg >> 8) & 0xff;
	sccb.addr_l   = (reg >> 0) & 0xff;
	sccb.cmd      = 0x33;

	if (attr->val_len == I2C_8BIT)
		sccb.len_ctrl = (0x0 << 1);
	else if (attr->val_len == I2C_16BIT)
		sccb.len_ctrl = (0x1 << 1);

	if (attr->reg_len == I2C_8BIT)
		sccb.len_ctrl |= (0x0 << 0);
	else if (attr->reg_len == I2C_16BIT)
		sccb.len_ctrl |= (0x1 << 0);
	ret = __b52_isp_read_i2c(&sccb, pos);
	if (ret)
		return ret;

	if (attr->val_len == I2C_8BIT)
		*val = sccb.i_data_l;
	else if (attr->val_len == I2C_16BIT)
		*val = (sccb.i_data_h << 8) | sccb.i_data_l;

	return 0;
};
EXPORT_SYMBOL_GPL(b52_isp_read_i2c);

static int b52_fill_cmd_i2c_buf(const struct regval_tab *regs,
		u8 addr, u16 num, u8 addr_len, u8 data_len, u8 pos, u16 written)
{
	int i = 0;
	u8 val;

	if (pos == B52_SENSOR_I2C_NONE)
		return 0;

	if (num + written > I2C_MAX_NUM) {
		pr_err("%s: parameter error\n", __func__);
		return -EINVAL;
	}
	if (addr_len == I2C_8BIT) {
		for (i = 0; i < num; i++) {
			b52_writeb((CMD_BUF_D + 0 + 4 * (i + written)),
					(0xff));
			b52_writeb((CMD_BUF_D + 1 + 4 * (i + written)),
					(regs[i].reg & 0xff));
		}
	} else if (addr_len == I2C_16BIT) {
		for (i = 0; i < num; i++) {
			b52_writeb((CMD_BUF_D + 0 + 4 * (i + written)),
					((regs[i].reg & 0xff00) >> 8));
			b52_writeb((CMD_BUF_D + 1 + 4 * (i + written)),
					(regs[i].reg & 0xff));
		}
	} else {
		pr_err("the length do not support");
		return -EINVAL;
	}
	if (data_len == I2C_8BIT) {
		for (i = 0; i < num; i++) {
			b52_writeb((CMD_BUF_D + 2 + 4 * (i + written)),
					regs[i].val & 0xff);
			b52_writeb((CMD_BUF_D + 3 + 4 * (i + written)),
					0xff);
		}
	} else if (data_len == I2C_16BIT) {
		for (i = 0; i < num; i++) {
			b52_writeb((CMD_BUF_D + 2 + 4 * (i + written)),
					((regs[i].val & 0xff00) >> 8));
			b52_writeb((CMD_BUF_D + 3 + 4 * (i + written)),
					(regs[i].val & 0xff));
		}
	} else {
		pr_err("the length do not support");
		return -EINVAL;
	}

	switch (pos) {
	case B52_SENSOR_I2C_PRIMARY_MASTER:
		val = I2C_PRIMARY;
		break;
	case B52_SENSOR_I2C_SECONDARY_MASTER:
		val = I2C_SECONDARY;
		break;
	case B52_SENSOR_I2C_BOTH:
		val = I2C_BOTH;
		break;
	default:
		pr_err("error i2c pos");
		return -EINVAL;
	}

	val |= I2C_WRITE |
		((addr_len == I2C_8BIT) ? I2C_8BIT_ADDR : I2C_16BIT_ADDR) |
		((data_len == I2C_8BIT) ? I2C_8BIT_DATA : I2C_16BIT_DATA);
	b52_writeb(CMD_REG1, val);
	b52_writeb(CMD_REG2, addr << 1);
	b52_writeb(CMD_REG3, num);
	return 0;
}

int b52_cmd_read_i2c(struct b52_cmd_i2c_data *data)
{
	u8 val;
	int ret = 0;
	int i = 0;
	struct regval_tab *tab;

	if (!data || !data->tab)
		return -EINVAL;

	if (data->num == 0 || data->pos == B52_SENSOR_I2C_NONE)
		return 0;

	if (data->num > I2C_MAX_NUM) {
		pr_err("%s: parameter error\n", __func__);
		return -EINVAL;
	}

	tab = data->tab;

	if (data->attr->reg_len == I2C_16BIT) {
		for (i = 0; i < data->num; i++) {
			b52_writeb((CMD_BUF_D + 0 + 4 * i),
					((tab[i].reg & 0xff00) >> 8));
			b52_writeb((CMD_BUF_D + 1 + 4 * i),
					(tab[i].reg & 0xff));
			b52_writeb((CMD_BUF_D + 2 + 4 * i), 0xff);
			b52_writeb((CMD_BUF_D + 3 + 4 * i), 0xff);
		}
	} else
		pr_err("the length do not support");

	switch (data->pos) {
	case B52_SENSOR_I2C_PRIMARY_MASTER:
		val = I2C_PRIMARY;
		break;
	case B52_SENSOR_I2C_SECONDARY_MASTER:
		val = I2C_SECONDARY;
		break;
	case B52_SENSOR_I2C_BOTH:
		val = I2C_BOTH;
		break;
	default:
		pr_err("error i2c pos");
		return -EINVAL;
	}

	val |= I2C_READ;
	val |= (data->attr->reg_len == I2C_8BIT) ?
		I2C_8BIT_ADDR : I2C_16BIT_ADDR;
	val |= (data->attr->val_len == I2C_8BIT) ?
		I2C_8BIT_DATA : I2C_16BIT_DATA;

	mutex_lock(&cmd_mutex);
	b52_writeb(CMD_REG1, val);
	b52_writeb(CMD_REG2, (data->attr->addr << 1));
	b52_writeb(CMD_REG3, 1);

	ret = wait_cmd_done(CMD_I2C_GRP_WR);
	mutex_unlock(&cmd_mutex);
	if (ret)
		return ret;

	if (data->attr->val_len == I2C_16BIT) {
		for (i = 0; i < data->num; i++) {
			tab[i].val = b52_readb(CMD_BUF_D + 2 + 4 * i) << 8;
			tab[i].val |= b52_readb(CMD_BUF_D + 3 + 4 * i);
		}
	} else if (data->attr->val_len == I2C_8BIT) {
		for (i = 0; i < data->num; i++)
			tab[i].val = b52_readb(CMD_BUF_D + 3 + 4 * i);
	} else
		pr_err("the length do not support");

	return ret;
}
EXPORT_SYMBOL_GPL(b52_cmd_read_i2c);

int b52_cmd_write_i2c(struct b52_cmd_i2c_data *data)
{
	u32 num;
	u16 write_num = 0;
	int ret = 0;
	const struct b52_sensor_i2c_attr *attr;

	if (!data || !data->attr || !data->tab) {
		pr_err("Error: %s, %d\n", __func__, __LINE__);
		return -EINVAL;
	}
	attr = data->attr;
	num = data->num;
	mutex_lock(&cmd_mutex);
	do {
		write_num = (num < I2C_MAX_NUM) ? num : I2C_MAX_NUM;
		b52_fill_cmd_i2c_buf(data->tab, attr->addr, write_num,
				attr->reg_len, attr->val_len, data->pos, 0);
		ret = wait_cmd_done(CMD_I2C_GRP_WR);
		if (ret) {
			int times = 0;
			for(; times < 3; times++) {
				ret = wait_cmd_done(CMD_I2C_GRP_WR);
				if(ret) 
					continue;
				else
					break;
			}
			pr_err("re-send cmd %d times: reg_len:%d, val_len:%d\n",
					times, attr->reg_len, attr->val_len);
			if(times >= 3) {
			mutex_unlock(&cmd_mutex);
			return ret;
		}
		}
		data->tab += write_num;
		num -= write_num;
	} while (num > 0);
	mutex_unlock(&cmd_mutex);
	return ret;
}
EXPORT_SYMBOL_GPL(b52_cmd_write_i2c);

static u8 b52_convert_mode_cfg(int flag, int num)
{
	u8 val = 0;

	if ((flag & BIT(CMD_FLAG_INIT)) || (flag & BIT(CMD_FLAG_MS)))
		val |= INTI_MODE_ENABLE;
	if (flag & BIT(CMD_FLAG_MUTE))
		val |= BYPASS_MAC_INT_ENABLE;
	if (flag & BIT(CMD_FLAG_LOCK_AEAG))
		val |= KEEP_EXPO_RATIO_ENABLE;
	if (flag & BIT(CMD_FLAG_META_DATA))
		val |= ENABEL_METADATA;
	if (flag & BIT(CMD_FLAG_LINEAR_YUV))
		val |= ENABLE_LINEAR_YUV;

	switch (num) {
	case 0:
		break;
	case 1:
		val |= NORMAL_CAPTURE;
		break;
	case 2:
		val |= BRACKET_CAPTURE;
		val |= BRACKET_2_EXPO;
		break;
	case 3:
		val |= BRACKET_CAPTURE;
		val |= BRACKET_3_EXPO;
		break;
	default:
		pr_err("%s: output number is wrong", __func__);
		break;
	}

	return val;
}
static u8 b52_convert_work_mode(int path, int type)
{
	u8 val = 0;

	if (type)
		goto type2;

	switch (path) {
	case B52ISP_ISD_PIPE1:
	case B52ISP_ISD_MS1:
	case B52ISP_ISD_DUMP1:
		val = PIPELINE_1;
		break;
	case B52ISP_ISD_PIPE2:
	case B52ISP_ISD_MS2:
	case B52ISP_ISD_DUMP2:
		val = PIPELINE_2;
		break;
	case B52ISP_ISD_3D:
		val = STEREO_3D;
		break;
	case B52ISP_ISD_HS:
		val = HIGH_SPEED;
		break;
	case B52ISP_ISD_HDR:
		val = HDR;
		break;
	default:
		pr_err("%s: wrong path\n", __func__);
		break;
	}
	return val;

type2:
	switch (path) {
	case B52ISP_ISD_DUMP1:
	case B52ISP_ISD_PIPE1:
		val = PIPELINE_1 + 1;
		break;
	case B52ISP_ISD_DUMP2:
	case B52ISP_ISD_PIPE2:
		val = PIPELINE_2 + 1;
		break;
	case B52ISP_ISD_3D:
		val = STEREO_3D + 1;
		break;
	default:
		pr_err("%s: wrong path\n", __func__);
		break;
	}
	return val;
}

static void b52_g_sensor_fmt_data(struct v4l2_subdev *sd,
		struct b52_cmd_i2c_data *data)
{
	struct b52_sensor *sensor = to_b52_sensor(sd);
	b52_sensor_call(sensor, g_cur_fmt, data);
}

static void b52_cfg_isp_lsc_phase(struct b52isp_cmd *cmd)
{
	int hflip;
	int vflip;
	u8 phase = 0;
	struct v4l2_subdev *sd = cmd->sensor;
	struct b52_sensor *sensor = to_b52_sensor(sd);

	hflip = v4l2_ctrl_g_ctrl(sensor->ctrls.hflip);
	vflip = v4l2_ctrl_g_ctrl(sensor->ctrls.vflip);

	if (hflip && vflip)
		phase |= LSC_MIRR | LSC_FLIP;
	else if (hflip && !vflip)
		phase |= LSC_MIRR;
	else if (!hflip && vflip)
		phase |= LSC_FLIP;

	switch (cmd->path) {
	case B52ISP_ISD_PIPE1:
	case B52ISP_ISD_MS1:
		b52_writeb(REG_ISP_LSC_PHASE, phase);
		break;
	case B52ISP_ISD_PIPE2:
	case B52ISP_ISD_MS2:
		pr_err("%s: haven't support pipe2, need OVT FW support %d\n", __func__, cmd->path);
		break;
	default:
		pr_err("%s: wrong path %d\n", __func__, cmd->path);
		break;
	}
}

static int b52_cfg_pixel_order(struct v4l2_pix_format *fmt, int path)
{
	u8 order = PIXEL_ORDER_BGGR;

	switch (fmt->pixelformat) {
	case V4L2_PIX_FMT_SBGGR8:
	case V4L2_PIX_FMT_SBGGR10:
	case V4L2_PIX_FMT_SBGGR16:
		order = PIXEL_ORDER_BGGR;
		break;

	case V4L2_PIX_FMT_SGBRG8:
	case V4L2_PIX_FMT_SGBRG10:
	case V4L2_PIX_FMT_SGBRG16:
		order = PIXEL_ORDER_GBRG;
		break;

	case V4L2_PIX_FMT_SGRBG8:
	case V4L2_PIX_FMT_SGRBG10:
	case V4L2_PIX_FMT_SGRBG16:
		order = PIXEL_ORDER_GRBG;
		break;

	case V4L2_PIX_FMT_SRGGB8:
	case V4L2_PIX_FMT_SRGGB10:
	case V4L2_PIX_FMT_SRGGB16:
		order = PIXEL_ORDER_RGGB;
		break;
	default:
		pr_err("%s: wrong pixelformat\n", __func__);
		break;
	}

	switch (path) {
	case B52ISP_ISD_PIPE1:
	case B52ISP_ISD_MS1:
		b52_writeb(ISP1_REG_BASE + REG_ISP_TOP6, order);
		break;
	case B52ISP_ISD_PIPE2:
	case B52ISP_ISD_MS2:
		b52_writeb(ISP2_REG_BASE + REG_ISP_TOP6, order);
		break;
	default:
		pr_err("%s: wrong path %d\n", __func__, path);
		break;
	}

	return 0;
}

static int b52_cmd_set_fmt(struct b52isp_cmd *cmd)
{
	int i;
	u8 val;
	int ret;
	u32 flags = cmd->flags;
	struct b52_cmd_i2c_data data;
	struct v4l2_subdev *sd = cmd->sensor;
	const struct b52_sensor_data *sensordata = cmd->memory_sensor_data;

	for (i = 0; i < cmd->nr_mac; i++) {
		b52_writeb(mac_base[i] + REG_MAC_RDY_ADDR0, 0);
		b52_writeb(mac_base[i] + REG_MAC_RDY_ADDR1, 0);
	}
	if (cmd->flags & BIT(CMD_FLAG_STREAM_OFF)) {
		atomic_set(&streaming_state, 0);

		cancel_delayed_work(&rdy_work);
#ifdef CONFIG_ISP_USE_TWSI3
		cancel_work_sync(&twsi_work);
#endif
		flags &= ~BIT(CMD_FLAG_META_DATA);
		if (flags & BIT(CMD_FLAG_MS)) {
			msleep(60);
			b52_writeb(mac_base[0] + REG_MAC_RDY_ADDR0, 0);
		}

		b52_writeb(mac_base[0] + REG_MAC_FRAME_CTRL0, FORCE_OVERFLOW |
			b52_readb(mac_base[0] + REG_MAC_FRAME_CTRL0));
	}
	if (!(flags & BIT(CMD_FLAG_MS)) &&
		!(flags & BIT(CMD_FLAG_STREAM_OFF))) {
		b52_basic_init(cmd);

		b52_g_sensor_fmt_data(sd, &data);
		b52_fill_cmd_i2c_buf(data.tab, data.attr->addr, data.num,
				data.attr->reg_len, data.attr->val_len, data.pos, 0);
	}

	val = b52_convert_mode_cfg(flags, 0);
	b52_writeb(CMD_REG5, val);
	val = b52_convert_work_mode(cmd->path, 0);
	b52_writeb(CMD_REG6, val);
	val = b52_bit_cnt(cmd->output_map);
	b52_writeb(CMD_REG7, val);

	b52_writeb(CMD_REG8, (u8)cmd->enable_map);

	b52_writeb(VTS_SYNC_TO_SENSOR, 0x01);

	b52_cfg_input(&cmd->src_fmt, cmd->src_type);
	b52_cfg_pixel_order(&cmd->src_fmt, cmd->path);
	b52_cfg_isp_lsc_phase(cmd);
	b52_cfg_idi(&cmd->src_fmt, &cmd->pre_crop);
	b52_cfg_output(cmd->output, cmd->output_map);
	b52_cfg_zoom(&cmd->pre_crop, &cmd->post_crop);

	if (flags & BIT(CMD_FLAG_META_DATA))
		b52_writel(ISP_META_ADDR, cmd->meta_dma);

	if (flags & BIT(CMD_FLAG_MS)) {
		b52_cfg_rd(&cmd->src_fmt, cmd->mem.axi_id,
			(u32)cmd->mem.buf[0]->ch_info[0].daddr, 0);
		pr_debug("memory sensor name:%s\n", sensordata->name);
		b52_cfg_isp_ms(sensordata, cmd->path);
		b52_writeb(REG_ISP_TOP37, RGBHGMA_ENABLE);
		b52_write_pipeline_info(cmd->path, cmd->mem.buf[0]);
	} else
		b52_cfg_isp(sd);
	ret = wait_cmd_done(CMD_SET_FMT);
	if (ret < 0) {
		b52_dump_isp_cnt(); /* dump ISP cnt to check */
		return ret;
	}

	b52_writeb(VTS_SYNC_TO_SENSOR, 0x00);

	if (!(flags & BIT(CMD_FLAG_MS)) &&
		(flags & BIT(CMD_FLAG_STREAM_OFF)))
		return ret;

	atomic_set(&streaming_state, 1);
	if (flags & BIT(CMD_FLAG_MS))
		schedule_delayed_work(&rdy_work,
			msecs_to_jiffies(FRAME_TIME_MS));
	if (flags & BIT(CMD_FLAG_META_DATA))
		ret = b52_get_metadata_port(cmd);
	return ret;
}

static int b52_cmd_chg_fmt(struct b52isp_cmd *cmd)
{
	u8 val;
	int ret;

	val = b52_convert_work_mode(cmd->path, 0);
	b52_writeb(CMD_REG6, val);
	b52_writeb(CMD_REG8, (u8)cmd->enable_map);

	b52_cfg_output(cmd->output, cmd->output_map);

	ret = wait_cmd_done(CMD_CHANGE_FMT);
	if (ret < 0)
		b52_dump_isp_cnt(); /* dump ISP cnt to check */
	return ret;
}

static int b52_cmd_capture_img(struct b52isp_cmd *cmd)
{
	u8 val;
	int ret;
	int bracket;
	struct b52_cmd_i2c_data data;
	struct v4l2_subdev *sd = cmd->sensor;

	b52_g_sensor_fmt_data(sd, &data);
	b52_fill_cmd_i2c_buf(data.tab, data.attr->addr, data.num,
			data.attr->reg_len, data.attr->val_len, data.pos, 0);
	b52_writeb(CMD_REG4, 0);

	val = b52_convert_mode_cfg(cmd->flags, cmd->output[0].nr_buffer);
	b52_writeb(CMD_REG5, val);
	bracket = val & BRACKET_CAPTURE;
	if (bracket)
		b52_set_bracket_ratio(cmd->b_ratio_1, cmd->b_ratio_2);
	val = b52_convert_work_mode(cmd->path, 0);
	b52_writeb(CMD_REG6, val);
	val = b52_bit_cnt((u8)cmd->output_map);
	b52_writeb(CMD_REG7, val);
	b52_writeb(CMD_REG8, 0x20);

	b52_cfg_input(&cmd->src_fmt, cmd->src_type);
	b52_cfg_idi(&cmd->src_fmt, &cmd->pre_crop);
	b52_cfg_output(cmd->output, cmd->output_map);
	b52_cfg_zoom(&cmd->pre_crop, &cmd->post_crop);
	b52_cfg_isp(sd);
	b52_cfg_capture(cmd->output, 1);

	atomic_set(&streaming_state, 1);
	if (!cmd->priv) {
		b52_writeb(REG_FW_MMU_CTRL, MMU_DISABLE);
		ret = wait_cmd_done(CMD_CAPTURE_IMG);
	} else {
		b52_writeb(REG_FW_MMU_CTRL, MMU_ENABLE);
		ret = wait_cmd_capture_img_done(cmd);
	}
	atomic_set(&streaming_state, 0);
	return ret;
}

static int b52_cmd_capture_raw(struct b52isp_cmd *cmd)
{
	u8 val;
	int ret;

	val = b52_convert_work_mode(cmd->path, 1);
	b52_writeb(CMD_REG1, val);

	b52_cfg_input(&cmd->src_fmt, cmd->src_type);
	b52_cfg_idi(&cmd->src_fmt, &cmd->pre_crop);
	b52_cfg_output(cmd->output, cmd->output_map);

	ret = wait_cmd_done(CMD_CAPTURE_RAW);
	return ret;
}

static int b52_cmd_tdns(struct b52isp_cmd *cmd)
{
	u8 val;
	int ret;

	val = b52_convert_work_mode(cmd->path, 1);
	b52_writeb(CMD_REG1, val);

	b52_cfg_output(cmd->output, cmd->output_map);
	b52_cfg_rd(&cmd->src_fmt, cmd->mem.axi_id, 0, 0);

b52_dump_cmd_setting();
	ret = wait_cmd_done(CMD_TDNS);
	return ret;
}

static dma_addr_t y_phy_addr, uv_phy_addr;
static char *y_buffer, *uv_buffer;

static void b52_free_coherent_mem(u32 size)
{
	if (y_buffer) {
		dma_free_coherent(NULL, size, y_buffer, y_phy_addr);
		y_buffer = NULL;
	}

	if (uv_buffer) {
		dma_free_coherent(NULL, size, uv_buffer, uv_phy_addr);
		uv_buffer = NULL;
	}
}

static int b52_alloc_coherent_mem(u32 size)
{
	int ret;

	if (!y_buffer) {
		y_buffer = (char *)dma_alloc_coherent(NULL, size,
				&y_phy_addr, GFP_KERNEL | __GFP_ZERO);
		if (y_buffer == NULL) {
			pr_err("%s: no mem for Y adv dns feature\n", __func__);
			return -ENOMEM;
		}
	}

	if (!uv_buffer) {
		uv_buffer = (char *)dma_alloc_coherent(NULL, size,
				&uv_phy_addr, GFP_KERNEL | __GFP_ZERO);
		if (uv_buffer == NULL) {
			pr_err("%s: no mem for UV adv dns feature\n", __func__);
			ret = -ENOMEM;
			goto out;
		}
	}

	return 0;

out:
	b52_free_coherent_mem(size);
	return ret;
}

static int b52_cfg_adv_dns(int type, u8 *dns)
{
	int ret = 0;

	*dns = ENABLE_ADV_DNS | ADV_DNS_MERGE;
	switch (type) {
	case ADV_DNS_NONE:
		*dns = DISABLE_ADV_DNS;
		break;
	case ADV_DNS_Y:
		*dns |= ADV_Y_DNS;
		break;
	case ADV_DNS_UV:
		*dns |= ADV_UV_DNS;
		break;
	case ADV_DNS_YUV:
		*dns |= ADV_Y_DNS | ADV_UV_DNS;
		break;
	default:
		return -EINVAL;
	}

	return ret;
}

static int b52_alloc_mem_dns(int use_buf, int enable, u32 size)
{
	int ret = 0;
	if (!use_buf)
		return 0;

	if (enable) {
		ret = b52_alloc_coherent_mem(size);
		if (ret < 0)
			return ret;

		b52_writel(ISP_DNS_Y_ADDR, y_phy_addr);
		b52_writel(ISP_DNS_UV_ADDR, uv_phy_addr);
	} else
		b52_free_coherent_mem(size);
	return ret;
}
int b52_dns_process(struct b52isp_cmd *cmd, u8 dns)
{
	u8 dns_buf;
	u32 fmt;
	int ret = 0;
	fmt = cmd->output[0].pix_mp.pixelformat;
	switch (fmt) {
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_YVYU:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_VYUY:
		b52_writeb(CMD_REG3, dns | ADV_DNS_UYVY);
		dns_buf = 1;
		pr_debug("fmt:V4L2_PIX_FMT_UYVY\n");
		break;
	case V4L2_PIX_FMT_NV12M:
	case V4L2_PIX_FMT_NV21M:
		b52_writeb(CMD_REG3, DISABLE_ADV_DNS);
		ret = wait_cmd_done(CMD_PROCESS_RAW);

		b52_writeb(CMD_REG3, dns | ADV_DNS_NV12);
		b52_writel(ISP_DNS_Y_ADDR,
			(u32)cmd->output[0].buf[0]->ch_info[0].daddr);
		b52_writel(ISP_DNS_UV_ADDR,
			(u32)cmd->output[0].buf[0]->ch_info[1].daddr);
		dns_buf = 0;
		pr_debug("fmt:V4L2_PIX_FMT_NV21M\n");
		break;
	case V4L2_PIX_FMT_YUV420M:
	case V4L2_PIX_FMT_YVU420M:
		b52_writeb(CMD_REG3, DISABLE_ADV_DNS);
		ret = wait_cmd_done(CMD_PROCESS_RAW);

		b52_writeb(CMD_REG3, dns | ADV_DNS_I420);
		b52_writel(ISP_DNS_Y_ADDR,
			(u32)cmd->output[0].buf[0]->ch_info[0].daddr);
		b52_writel(ISP_DNS_U_ADDR,
			(u32)cmd->output[0].buf[0]->ch_info[1].daddr);
		b52_writel(ISP_DNS_V_ADDR,
			(u32)cmd->output[0].buf[0]->ch_info[2].daddr);
		dns_buf = 0;
		pr_debug("fmt:V4L2_PIX_FMT_YVU420M\n");
		break;
	default:
		pr_err("output format not support: %d\n", fmt);
		return -EINVAL;
	}
	ret = b52_alloc_mem_dns(dns_buf, 1, cmd->src_fmt.sizeimage >> 1);
	if (ret < 0) {
		pr_err("alloc mem fail\n");
		return ret;
	}
	b52_writeb(CMD_REG4, (((cmd->adv_dns.Y_times)&0x0f)<<4) |
			((cmd->adv_dns.UV_times)&0x0f));
	ret = wait_cmd_done(CMD_UV_DENOISE);
	if (ret < 0) {
		pr_err("first dns fail we will try again\n");
		ret = wait_cmd_done(CMD_UV_DENOISE);
	}
	b52_alloc_mem_dns(dns_buf, 0, cmd->src_fmt.sizeimage >> 1);
	return ret;
}
static int b52_cmd_process_raw(struct b52isp_cmd *cmd)
{
	u8 val;
	int ret;
	int cnt;
	u8 dns;

	val = b52_convert_work_mode(cmd->path, 0);
	b52_writeb(CMD_REG1, val);
	val = b52_bit_cnt((u8)cmd->output_map);
	cnt = val;
	b52_writeb(CMD_REG2, val);

	b52_cfg_input(&cmd->src_fmt, cmd->src_type);
	b52_cfg_pixel_order(&cmd->src_fmt, cmd->path);
	b52_cfg_output(cmd->output, cmd->output_map);
	b52_cfg_rd(&cmd->src_fmt, cmd->mem.axi_id,
		(u32)cmd->mem.buf[0]->ch_info[0].daddr, 0);

	b52_cfg_zoom(&cmd->pre_crop, &cmd->post_crop);
	b52_cfg_capture(cmd->output, cnt);
	b52_write_pipeline_info(cmd->path, cmd->mem.buf[0]);

	ret = b52_cfg_adv_dns(cmd->adv_dns.type, &dns);
	if (ret < 0)
		return ret;

	if (dns == DISABLE_ADV_DNS) {
		b52_writeb(CMD_REG3, DISABLE_ADV_DNS);
		ret = wait_cmd_done(CMD_PROCESS_RAW);
		if (ret < 0) {
			pr_err("process raw failed try again\n");
			ret = wait_cmd_done(CMD_PROCESS_RAW);
			if (ret < 0)
				pr_err("process raw failed\n");
		}
	} else {
		ret = b52_dns_process(cmd, dns);
		if (ret < 0)
			pr_err("dns process raw failed\n");
	}
	return ret;
}

int b52_cmd_zoom_in(int path, int zoom)
{
	u8 val;
	int ret;

	if (zoom > ZOOM_RATIO_MAX || zoom < ZOOM_RATIO_MIN) {
		pr_err("zoom cmd: zoom error %x\n", zoom);
		return -EINVAL;
	}
	b52_zoom_ddr_qos(zoom);
	mutex_lock(&cmd_mutex);
	val = b52_convert_work_mode(path, 0);
	b52_writeb(CMD_REG1, val);
	b52_writew(CMD_REG2, zoom);
	ret = wait_cmd_done(CMD_ZOOM_IN_MODE);
	mutex_unlock(&cmd_mutex);

	return ret;
}
EXPORT_SYMBOL_GPL(b52_cmd_zoom_in);

static int __maybe_unused b52_cmd_awb(struct b52isp_cmd *cmd)
{
	/* FIXME */
	b52_writeb(CMD_REG1, 1);
	return wait_cmd_done(CMD_AWB_MODE);
}

int b52_cmd_anti_shake(u16 block_size, int enable)
{
	int ret;

	mutex_lock(&cmd_mutex);
	b52_writeb(CMD_REG1, enable);
	b52_writew(CMD_REG2, block_size);
	ret = wait_cmd_done(CMD_ANTI_SHAKE);
	mutex_unlock(&cmd_mutex);

	return ret;
}
EXPORT_SYMBOL_GPL(b52_cmd_anti_shake);

static inline int b52_cmd_abort(void)
{
	return wait_cmd_done(CMD_ABORT);
}

int b52_cmd_vcm(void)
{
	int ret;
	mutex_lock(&cmd_mutex);
	ret = wait_cmd_done(CMD_AF_MODE);
	mutex_unlock(&cmd_mutex);
	return ret;
}
EXPORT_SYMBOL_GPL(b52_cmd_vcm);

static int __maybe_unused b52_cmd_still_hdr(struct b52isp_cmd *cmd)
{
	u8 val;
	int ret;

	val = b52_convert_mode_cfg(cmd->flags, 0);
	b52_writeb(CMD_REG5, val);

	b52_cfg_input(&cmd->src_fmt, cmd->src_type);
	b52_cfg_output(cmd->output, cmd->output_map);
	b52_cfg_rd(&cmd->src_fmt, cmd->mem.axi_id,
			cmd->mem.buf[0]->ch_info[0].daddr,
			cmd->mem.buf[1]->ch_info[0].daddr);
	b52_cfg_hdr();
	b52_cfg_capture(cmd->output, 1);

b52_dump_cmd_setting();
	ret = wait_cmd_done(CMD_HDR_STILL);
	return ret;
}

int b52_cmd_effect(int reg_nums)
{
	int ret;
	mutex_lock(&cmd_mutex);
	if (atomic_read(&streaming_state) == 0)
		b52_writeb(CMD_REG5, SED_EOF_DIS);
	else
		b52_writeb(CMD_REG5, SED_EOF_EN);
	b52_writeb(CMD_REG3, reg_nums);
	ret = wait_cmd_done(CMD_EFFECT);
	mutex_unlock(&cmd_mutex);
	return ret;
}
EXPORT_SYMBOL_GPL(b52_cmd_effect);

static int __maybe_unused dump_cmd_ctx(struct b52isp_cmd *cmd)
{
	int i, j;
	struct v4l2_rect *r;
	struct v4l2_pix_format *f;
	struct v4l2_pix_format_mplane *fm;
	struct isp_videobuf *b;

	if (!cmd) {
		pr_err("%s, %d\n", __func__, __LINE__);
		return -EINVAL;
	}

	pr_info("\nstart to dump cmd ctx\n");
	pr_info("cmd %d, flags 0x%x, path %d, src_type %d\n",
		cmd->cmd_name, cmd->flags, cmd->path, cmd->src_type);

	f = &cmd->src_fmt;
	pr_info("src w %d, h %d, pfmt 0x%x, pitch %d, s %d\n", f->width,
		f->height, f->pixelformat, f->bytesperline, f->sizeimage);

	r = &cmd->pre_crop;
	pr_info("pre crop:l %d, t %d, w %d, h %d\n",
		r->left, r->top, r->width, r->height);

	r = &cmd->post_crop;
	pr_info("post crop:l %d, t %d, w %d, h %d\n",
		r->left, r->top, r->width, r->height);

	pr_info("output map 0x%lx, ena 0x%lx\n", cmd->output_map,
		cmd->enable_map);

	for (i = 0; i < B52_OUTPUT_PER_PIPELINE; i++) {
		if (cmd->output[i].vnode == NULL)
			continue;
		fm = &cmd->output[i].pix_mp;
		pr_info("out %d:w %d, h %d, fmt 0x%x, planes %d\n", i,
			fm->width, fm->height, fm->pixelformat, fm->num_planes);
		for (j = 0; j < fm->num_planes; j++)
			pr_info("plane %d:size %d, pitch %d\n", j,
				fm->plane_fmt[j].sizeimage,
				fm->plane_fmt[j].bytesperline);

		for (j = 0; j < cmd->output[i].nr_buffer; j++) {
			b = cmd->output[i].buf[j];
			if (!b) {
				pr_err("%s, %d\n", __func__, __LINE__);
				return -EINVAL;
			}
			switch (fm->num_planes) {
			case 1:
				pr_info("addr 0x%p\n",
					(void *)b->ch_info[0].daddr);
				break;
			case 2:
				pr_info("addr Y 0x%p, UV 0x%p\n",
					(void *)b->ch_info[0].daddr,
					(void *)b->ch_info[1].daddr);
				break;
			case 3:
				pr_info("addr Y 0x%p, U 0x%p, V 0x%p\n",
					(void *)b->ch_info[0].daddr,
					(void *)b->ch_info[1].daddr,
					(void *)b->ch_info[2].daddr);
				break;
			default:
				pr_err("%s, %d\n", __func__, __LINE__);
				return -EINVAL;
			}

		}
	}
	pr_info("\nend of dump cmd ctx\n\n");

	return 0;
}

static void b52_set_isp_default(void)
{
	u32 reg;

	for (reg = CMD_REG1; reg <= CMD_REG15; reg++)
		b52_writeb(reg, 0);
}

int b52_hdl_cmd(struct b52isp_cmd *cmd)
{
	int ret = 0;
	if (!cmd) {
		pr_err("%s: parameter is null\n", __func__);
		return -EINVAL;
	}

	if (!b52_base) {
		pr_err("%s: firmware is not loaded\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&cmd_mutex);
	b52_set_isp_default();

	switch (cmd->cmd_name) {
	case CMD_TEST:
		ret = b52_cmd_set_fmt(cmd);
		b52_writeb(0x68007, b52_readb(0x68007) | 1 << 5);
		break;
	case CMD_TEMPORAL_DNS:
		ret = b52_cmd_tdns(cmd);
		break;
	case CMD_RAW_DUMP:
		ret = b52_cmd_capture_raw(cmd);
		break;
	case CMD_RAW_PROCESS:
		ret = b52_cmd_process_raw(cmd);
		break;
	case CMD_SET_FORMAT:
		ret = b52_cmd_set_fmt(cmd);
		break;
	case CMD_CHG_FORMAT:
		ret = b52_cmd_chg_fmt(cmd);
		break;
	case CMD_IMG_CAPTURE:
	    ret = b52_cmd_capture_img(cmd);
		break;
	default:
		pr_err("%s: wrong cmd id\n", __func__);
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&cmd_mutex);
	return ret;
}
EXPORT_SYMBOL_GPL(b52_hdl_cmd);

static int b52_set_aecagc_reg(struct v4l2_subdev *sd, int p_num)
{
	u32 base;
	u8 type;
	u8 val;
	int ret;
	u8 gain_shift, expo_shift, dgain_channel;
	struct b52_sensor_regs reg;
	struct b52_sensor_i2c_attr attr;
	struct b52_sensor *sensor = to_b52_sensor(sd);
	if (p_num > 1) {
		pr_err("%s: parameter error %d\n", __func__, p_num);
		return -EINVAL;
	}
	base = FW_P1_REG_BASE + p_num * FW_P1_P2_OFFSET;
	type = sensor->drvdata->type;
	gain_shift = sensor->drvdata->gain_shift;
	expo_shift = sensor->drvdata->expo_shift;
	dgain_channel = sensor->drvdata->dgain_channel;
	/*
	 * vendor recommands to use Q4,
	 * ISP also uses it to calculate stretch gain.
	 */
	b52_writeb(base + REG_FW_SSOR_GAIN_MODE, SSOR_GAIN_Q4);

#ifdef CONFIG_ISP_USE_TWSI3
	/*
	 * FIXME: In firmware, expo and gain take effect after 2 frames
	 * for sony sensor, while ov sensor expo after 2 frames and
	 * gain after one frame.
	 * IMX219 is like ov sensor.
	 */
	if (type == SONY_SENSOR)
		type = OVT_SENSOR;
#endif
	b52_writeb(base + REG_FW_SSOR_TYPE, type);

	ret = b52_sensor_call(sensor, g_sensor_attr, &attr);
	if (ret < 0)
		return ret;
	b52_writeb(base + REG_FW_SSOR_DEV_ID, attr.addr<<1);
	/*16-bit addr; 8-bit data */
	val = sensor->pos;
	if (attr.reg_len == I2C_16BIT)
		val |= FOCUS_ADDR_16BIT;
	if (attr.val_len == I2C_16BIT)
		val |= FOCUS_DATA_16BIT;
	b52_writeb(base + REG_FW_SSOR_I2C_OPT, val);

	ret = b52_sensor_call(sensor, g_aecagc_reg, B52_SENSOR_EXPO, &reg);
	if (ret < 0)
		return ret;
	switch (reg.num) {
	case 2:
		b52_writew(base + REG_FW_SSOR_AEC_ADDR_0, reg.tab->reg);
		b52_writew(base + REG_FW_SSOR_AEC_ADDR_1, reg.tab->reg);
		b52_writew(base + REG_FW_SSOR_AEC_ADDR_2, reg.tab->reg + 1);
		b52_writeb(base + REG_FW_SSOR_AEC_MSK_0, 0x00);
		b52_writeb(base + REG_FW_SSOR_AEC_MSK_1, 0xff);
		b52_writeb(base + REG_FW_SSOR_AEC_MSK_2, 0xff);
		break;
	case 3:
		b52_writew(base + REG_FW_SSOR_AEC_ADDR_0, reg.tab->reg);
		b52_writew(base + REG_FW_SSOR_AEC_ADDR_1, reg.tab->reg + 1);
		b52_writew(base + REG_FW_SSOR_AEC_ADDR_2, reg.tab->reg + 2);
		b52_writeb(base + REG_FW_SSOR_AEC_MSK_0, 0xff);
		b52_writeb(base + REG_FW_SSOR_AEC_MSK_1, 0xff);
		b52_writeb(base + REG_FW_SSOR_AEC_MSK_2, 0xff);
		break;
	default:
		pr_err("%s: no expo reg", __func__);
		break;
	}
	ret = b52_sensor_call(sensor, g_aecagc_reg, B52_SENSOR_FRACTIONALEXPO, &reg);
	if (!ret && (reg.num > 0)) {
		b52_writeb(base + REG_FW_AEC_ALLOW_FRATIONAL_EXP, 0x01);
		b52_writeb(base + REG_FW_SSOR_FRATIONAL_ADDR_0, reg.tab->reg);
		b52_writeb(base + REG_FW_SSOR_FRATIONAL_ADDR_1,
						reg.tab->reg + 1);
	}
	b52_writeb(base + REG_FW_AEC_EXP_SHIFT, expo_shift);

	ret = b52_sensor_call(sensor, g_aecagc_reg, B52_SENSOR_VTS, &reg);
	if (ret < 0)
		return ret;
	switch (reg.num) {
	case 1:
		b52_writew(base + REG_FW_SSOR_AEC_ADDR_4, reg.tab->reg);
		b52_writew(base + REG_FW_SSOR_AEC_ADDR_5, reg.tab->reg);
		b52_writeb(base + REG_FW_SSOR_AEC_MSK_4, 0x00);
		b52_writeb(base + REG_FW_SSOR_AEC_MSK_5, 0xff);
		break;
	case 2:
		b52_writew(base + REG_FW_SSOR_AEC_ADDR_4, reg.tab->reg);
		b52_writew(base + REG_FW_SSOR_AEC_ADDR_5, reg.tab->reg + 1);
		b52_writeb(base + REG_FW_SSOR_AEC_MSK_4, 0xff);
		b52_writeb(base + REG_FW_SSOR_AEC_MSK_5, 0xff);
		break;
	default:
		pr_err("%s: no again reg", __func__);
		break;
	}
	b52_writeb(base + REG_FW_AEC_GAIN_SHIFT, gain_shift);

	ret = b52_sensor_call(sensor, g_aecagc_reg, B52_SENSOR_AGAIN, &reg);
	if (ret < 0)
		return ret;
	switch (reg.num) {
	case 1:
		b52_writew(base + REG_FW_SSOR_AEC_ADDR_6, reg.tab->reg);
		b52_writew(base + REG_FW_SSOR_AEC_ADDR_7, reg.tab->reg);
		b52_writeb(base + REG_FW_SSOR_AEC_MSK_6, 0x00);
		b52_writeb(base + REG_FW_SSOR_AEC_MSK_7, 0xff);
		break;
	case 2:
		b52_writew(base + REG_FW_SSOR_AEC_ADDR_6, reg.tab->reg);
		b52_writew(base + REG_FW_SSOR_AEC_ADDR_7, reg.tab->reg + 1);
		b52_writeb(base + REG_FW_SSOR_AEC_MSK_6, 0xff);
		b52_writeb(base + REG_FW_SSOR_AEC_MSK_7, 0xff);
		break;
	default:
		pr_err("%s: no again reg", __func__);
		break;
	}
	ret = b52_sensor_call(sensor, g_aecagc_reg, B52_SENSOR_DGAIN, &reg);
	if (ret < 0)
		return ret;
	if (reg.tab) {
		switch (reg.num) {
		case 1:
			b52_writew(base + REG_FW_SSOR_AEC_ADDR_8, reg.tab->reg);
			b52_writew(base + REG_FW_SSOR_AEC_ADDR_9, reg.tab->reg);
			b52_writeb(base + REG_FW_SSOR_AEC_MSK_8, 0x00);
			b52_writeb(base + REG_FW_SSOR_AEC_MSK_9, 0xff);
			break;
		case 2:
			b52_writew(base + REG_FW_SSOR_AEC_ADDR_8, reg.tab->reg);
			b52_writew(base + REG_FW_SSOR_AEC_ADDR_9,
							reg.tab->reg + 1);
			b52_writeb(base + REG_FW_SSOR_AEC_MSK_8, 0xff);
			b52_writeb(base + REG_FW_SSOR_AEC_MSK_9, 0xff);
			break;
		default:
			pr_err("%s: no dgain reg", __func__);
			break;
		}
	}
	b52_writeb(base + REG_FW_AEC_DGAIN_CHANNEL, dgain_channel);
#ifdef CONFIG_ISP_USE_TWSI3
	b52_writeb(base + REG_FW_EXPO_GAIN_WR, EXPO_GAIN_HOST_WR);
#endif

	return 0;
}
EXPORT_SYMBOL_GPL(b52_set_aecagc_reg);

static int b52_set_vcm_reg(struct v4l2_subdev *sd, int p_num)
{
	u32 base;
	int ret;
	u8 val = 0;
	struct b52_sensor_vcm vcm;
	struct b52_sensor *sensor = to_b52_sensor(sd);

	if (p_num > 1) {
		pr_err("%s: parameter error %d\n", __func__, p_num);
		return -EINVAL;
	}
	base = FW_P1_REG_AF_BASE + p_num * FW_P1_P2_AF_OFFSET;

	ret = b52_sensor_call(sensor, g_vcm_info, &vcm);
	if (ret == 1) /*if ret==1, which means, the sensor has no vcm*/
		return 0;

	if (ret < 0)
		return ret;

	val = sensor->pos;
	if (vcm.attr->reg_len == I2C_16BIT)
		val |= FOCUS_ADDR_16BIT;
	if (vcm.attr->val_len == I2C_16BIT)
		val |= FOCUS_DATA_16BIT;
	b52_writeb(base + REG_FW_FOCUS_I2C_OPT, val);
	b52_writeb(base + REG_FW_FOCUS_I2C_ADDR, vcm.attr->addr << 1);
	b52_writew(base + REG_FW_FOCUS_REG_ADDR_MSB, vcm.pos_reg_msb);
	b52_writew(base + REG_FW_FOCUS_REG_ADDR_LSB, vcm.pos_reg_lsb);

#ifdef CONFIG_ISP_USE_TWSI3
	b52_writeb(base + REG_FW_VCM_TYPE, VCM_HOST_WR);
#else
	b52_writeb(base + REG_FW_VCM_TYPE, vcm.type);
#endif

	return 0;
}

int b52_set_focus_win(struct v4l2_rect *win, int id)
{
	u32 base = ISP1_REG_BASE + id * ISP1_ISP2_OFFSER;

	b52_writew(base + REG_AFC_WIN_X0, win->left);
	b52_writew(base + REG_AFC_WIN_Y0, win->top);
	b52_writew(base + REG_AFC_WIN_W0, win->width);
	b52_writew(base + REG_AFC_WIN_H0, win->height);

	return 0;
}
EXPORT_SYMBOL_GPL(b52_set_focus_win);

void b52_clear_overflow_flag(u8 mac, u8 port)
{
	unsigned long irq_flags;

	if (mac >= MAX_MAC_NUM || port >= MAX_PORT_NUM) {
		pr_err("%s param error\n", __func__);
		return;
	}

	spin_lock_irqsave(&mac_overflow_lock, irq_flags);
	mac_overflow[mac][port] = 0;
	spin_unlock_irqrestore(&mac_overflow_lock, irq_flags);
}

int b52_update_mac_addr(dma_addr_t *addr, dma_addr_t meta_addr,
			u8 plane, u8 mac, u8 port)
{
	int i;
	u32 reg = 0;
	u8 val = 0;
	unsigned long irq_flags;

	if (!addr || plane >= MAX_MAC_ADDR_NUM ||
			mac >= MAX_MAC_NUM || port >= MAX_PORT_NUM) {
		pr_err("%s param error\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < plane; i++) {
		reg =  mac_base[mac] + mac_addr_offset[port][i];
		b52_writel(reg, addr[i]);
	}
	/* Update meta address */
	reg = mac_base[mac] + mac_addr_offset[port][MAX_MAC_ADDR_NUM - 1];
	if (meta_addr)
		b52_writel(reg, meta_addr);

	/* Update ready port, RDY_0 for s0, RDY_2 for s1 */
	switch (port) {
	case MAC_PORT_W0:
		val = W_RDY_0;
		reg = mac_base[mac] + REG_MAC_RDY_ADDR0;
		break;
	case MAC_PORT_W1:
		val = W_RDY_2;
		reg = mac_base[mac] + REG_MAC_RDY_ADDR0;
		break;
	case MAC_PORT_R:
	/* FIXME using workqueue to controll the input fps */
	/*	val = R_RDY_0;
		reg = mac_base[mac] + REG_MAC_RDY_ADDR1;
	*/
		schedule_delayed_work(&rdy_work,
				msecs_to_jiffies(FRAME_TIME_MS));
		return 0;
	default:
		pr_err("%s param error\n", __func__);
		return -EINVAL;
	}

	spin_lock_irqsave(&mac_overflow_lock, irq_flags);
	if (mac_overflow[mac][port]) {
		spin_unlock_irqrestore(&mac_overflow_lock, irq_flags);
		return 0;
	}
	b52_writeb(reg, val | b52_readb(reg));
	spin_unlock_irqrestore(&mac_overflow_lock, irq_flags);

	return 0;
}
EXPORT_SYMBOL(b52_update_mac_addr);

void b52_clear_mac_rdy_bit(u8 mac, u8 port)
{
	u32 reg = 0;
	u8 val = 0;

	if (mac >= MAX_MAC_NUM || port >= MAX_PORT_NUM) {
		pr_err("%s param error\n", __func__);
		return;
	}

	switch (port) {
	case MAC_PORT_W0:
		val = W_RDY_0;
		reg = mac_base[mac] + REG_MAC_RDY_ADDR0;
		break;
	case MAC_PORT_W1:
		val = W_RDY_2;
		reg = mac_base[mac] + REG_MAC_RDY_ADDR0;
		break;
	case MAC_PORT_R:
		val = R_RDY_0;
		reg = mac_base[mac] + REG_MAC_RDY_ADDR1;
		break;
	default:
		pr_err("%s,%d param error\n", __func__, __LINE__);
		return;
	}

	b52_writeb(reg, b52_readb(reg) & (~val));
}
EXPORT_SYMBOL(b52_clear_mac_rdy_bit);

static int b52_config_mac(u8 mac_id, u8 port_id, int enable)
{
	__u8 val;
	u32 reg;

	if (mac_id >= MAX_MAC_NUM || port_id >= MAX_PORT_NUM) {
		pr_err("%s param error\n", __func__);
		return -EINVAL;
	}

	if (!enable) {
		if (port_id == MAC_PORT_R)
			reg = REG_MAC_INT_EN_CTRL1;
		else
			reg = REG_MAC_INT_EN_CTRL0;

		b52_writeb(mac_base[mac_id] + reg, 0);

		return 0;
	}

	if (port_id == MAC_PORT_R) {
		/* read axi id can not be same with write axi id */
		b52_writeb(mac_base[mac_id] + REG_MAC_RD_ID0, 0x66);

		b52_writeb(mac_base[mac_id] + REG_MAC_INT_EN_CTRL1,
			   R_INT_DONE | R_INT_SOF | R_INT_UNDERFLOW);

		return 0;
	}

	/* Setup AXI_ID for MMU channel */
	b52_writeb(mac_base[mac_id] + REG_MAC_WR_ID0, 0x10);
	b52_writeb(mac_base[mac_id] + REG_MAC_WR_ID1, 0x32);
	b52_writeb(mac_base[mac_id] + REG_MAC_WR_ID2, 0x54);
	/* meta data not use mmu, and not same with read ID */
	b52_writeb(mac_base[mac_id] + REG_MAC_WR_ID3, 0x77);

	/* Enable MAC Module Interrupt */
	b52_writeb(mac_base[mac_id] + REG_MAC_INT_EN_CTRL0, 0xFF);
	/* Set IRQ option 6*/
	val = b52_readb(REG_TOP_INTR_CTRL_L);
	val &= ~(7 << (mac_id * 3));
	val |= (6 << (mac_id * 3));
	b52_writeb(REG_TOP_INTR_CTRL_L, val);
	if (mac_id == 2) {
		val = b52_readb(REG_TOP_INTR_CTRL_H);
		val &= ~1;
		val |= (6 >> 2) & 0x1;
		b52_writeb(REG_TOP_INTR_CTRL_H, val);
	}

	/*read to clear mac irq status*/
	val = b52_readb(mac_base[mac_id] + REG_MAC_FRAME_CTRL1);
	val &= ~(1 << 1);
	b52_writeb(mac_base[mac_id] + REG_MAC_FRAME_CTRL1, val);

	/* Disable Ping-pong */
	val = b52_readb(mac_base[mac_id] + REG_MAC_W_PP_CTRL);
	b52_writeb(mac_base[mac_id] + REG_MAC_W_PP_CTRL, val & ~(3));

	/* enable channel_ctr_en*/
	val = b52_readb(mac_base[mac_id] + REG_MAC_R_OUTSTD_CTRL);
	val |= CHANNEL_CTR_EN;
	b52_writeb(mac_base[mac_id] + REG_MAC_R_OUTSTD_CTRL, val);

	return 0;
}
int b52_ctrl_mac_irq(u8 mac_id, u8 port_id, int enable)
{
	int *refcnt;
	int ret = 0;
	static int mac_w_cnt[MAX_MAC_NUM];
	static int mac_r_cnt[MAX_MAC_NUM];

	if (mac_id >= MAX_MAC_NUM || port_id >= MAX_PORT_NUM) {
		pr_err("%s, %d, param error\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (port_id == MAC_PORT_R)
		refcnt = mac_r_cnt;
	else
		refcnt = mac_w_cnt;

	if (enable) {
		if (refcnt[mac_id] == 0)
			ret = b52_config_mac(mac_id, port_id, 1);

		refcnt[mac_id]++;
	} else {
		if (WARN_ON(refcnt[mac_id] <= 0))
			return -EINVAL;

		if (--refcnt[mac_id] > 0)
			return 0;

		ret = b52_config_mac(mac_id, port_id, 0);
	}

	return ret;
}
EXPORT_SYMBOL(b52_ctrl_mac_irq);

/*
 * Acknowledge IRQ, and translate them into bits
 */
void b52_ack_xlate_irq(__u32 *event, int max_mac_num, struct work_struct *work)
{
	static int drop_cnt[MAX_MAC_NUM];
	__u32 reg = b52_readl(REG_ISP_INT_STAT);
	__u32 cmd_reg = b52_readb(REG_FW_CPU_CMD_ID);
#ifdef CONFIG_ISP_USE_TWSI3
	__u32 twsi_reg = b52_readb(REG_FW_AGC_AF_FLG);
#endif
	int i;
	if (reg & INT_CMD_DONE) {
		if (b52_readb(CMD_ID) == CMD_DOWNLOAD_FW)
			complete(&b52isp_cmd_done);
		if (cmd_reg) {
			b52_writeb(REG_FW_CPU_CMD_ID, 0);
			b52isp_cmd_id = cmd_reg;
			complete(&b52isp_cmd_done);
		}
#ifdef CONFIG_ISP_USE_TWSI3
		if (CMD_WB_EXPO_GAIN <= twsi_reg && twsi_reg <= CMD_WB_FOCUS)
			schedule_work(&twsi_work);
#endif
	}

	for (i = 0; i < max_mac_num; i++) {
		__u16 mac_irq;
		__u8 irq_src, irq0 = 0, irq1 = 0, irqr = 0, rdy;

		event[i] = 0;

		mac_irq = (b52_readw(mac_base[i] + REG_MAC_INT_STAT1));
		if (!(mac_irq & 0xFFFF))
			continue;

		irq_src = b52_readb(mac_base[i] + REG_MAC_INT_SRC);
		rdy = b52_readb(mac_base[i] + REG_MAC_RDY_ADDR0);

		/* build up write ports virtual IRQs */
		if (mac_irq & W_INT_START0) {
			if (irq_src & INT_SRC_W1)
				irq0 |= VIRT_IRQ_START;
			else if ((rdy & W_RDY_0) == 0)
				irq0 |= VIRT_IRQ_DROP;
			if (irq_src & INT_SRC_W2)
				irq1 |= VIRT_IRQ_START;
			else if ((rdy & W_RDY_2) == 0)
				irq1 |= VIRT_IRQ_DROP;
		}
		if (mac_irq & W_INT_DROP0) {
			if ((rdy & W_RDY_0) == 0)
				irq0 |= VIRT_IRQ_DROP;
			if ((rdy & W_RDY_2) == 0)
				irq1 |= VIRT_IRQ_DROP;

			drop_cnt[i]++;
		}
		if (mac_irq & W_INT_DONE0) {
			if (irq_src & INT_SRC_W1)
				irq0 |= VIRT_IRQ_DONE;
			if (irq_src & INT_SRC_W2)
				irq1 |= VIRT_IRQ_DONE;
		}
		if (mac_irq & W_INT_OVERFLOW0) {
			irq0 |= VIRT_IRQ_FIFO;
			irq1 |= VIRT_IRQ_FIFO;
			spin_lock(&mac_overflow_lock);
			mac_overflow[i][MAC_PORT_W0] = 1;
			mac_overflow[i][MAC_PORT_W1] = 1;
			/* stop ouputting data, untill handle overflow */
			if (rdy)
				b52_writeb(mac_base[i] + REG_MAC_RDY_ADDR0, 0);
			spin_unlock(&mac_overflow_lock);

			of_jiffies = get_jiffies_64();
			if (atomic_read(&streaming_state)) {
				b52isp_set_ddr_threshold(work, 1);
				of = 1;
			}
		}

		if (mac_irq & (W_INT_DONE0 | W_INT_OVERFLOW0))
			drop_cnt[i] = 0;
		if (drop_cnt[i] >= 3) {
			b52_writeb(mac_base[i] + REG_MAC_FRAME_CTRL0, FORCE_OVERFLOW
				| b52_readb(mac_base[i] + REG_MAC_FRAME_CTRL0));
			drop_cnt[i] = 0;
		}

		/* build up read port virtual IRQs */
		irqr = ((mac_irq >> 8) & R_INT_DONE) ? VIRT_IRQ_DONE : 0 |
			((mac_irq >> 8) & R_INT_SOF) ? VIRT_IRQ_START : 0 |
			((mac_irq >> 8) & R_INT_UNDERFLOW) ? VIRT_IRQ_FIFO : 0;

		event[i] |= virt_irq(0, irq0) | virt_irq(1, irq1) |
				virt_irq(2, irqr);

		if (of && time_before64(of_jiffies + 2 * HZ, get_jiffies_64())) {
			b52isp_set_ddr_threshold(work, 0);
			of = 0;
		}
	}
}

static struct device test_dev = {
	.init_name = "test",
};

#define	B52_REG_PROC_FILE	"driver/b52_reg"
static struct proc_dir_entry	*b52_reg_proc;
static char			out_buf[PAGE_SIZE];
static size_t			out_cnt;

static ssize_t b52_proc_read_reg(
	struct file *filp,
	char __user *buffer,
	size_t length,
	loff_t *ppos)
{
	return simple_read_from_buffer(buffer, length, ppos, out_buf, out_cnt);
}

static ssize_t b52_proc_write_reg(struct file *filp,
	const char __user *buffer, size_t length, loff_t *offset)
{
	int ret;
	char readwrite;
	u32	addr;
	u32	tmp, val, i, cnt;
	int	rw_write = 0;
	char buf[64];
	u32     offst = 0;

	memset(out_buf, 0, PAGE_SIZE);
	if (!b52_base) {
		out_cnt = 0;
		return length;
	}

	if (copy_from_user(buf, buffer, length)) {
		pr_err("error: copy_from_user\n");
		return -EFAULT;
	}
	ret = sscanf(buf, "%c %x %x", &readwrite, &addr, &tmp);
	switch (readwrite) {
	case 'w':
		rw_write = 1;
		break;
	case 'r':
		rw_write = 0;
		break;
	default:
		pr_err("error: access could only be r or w\n");
		pr_info("[b52_reg] usage:\n");
		pr_info(" read:  echo r [addr] [count] > /proc/driver/b52_reg\n");
		pr_info(" write: echo w [addr] [value] > /proc/driver/b52_reg\n");
		return -EFAULT;
	}

	for (i = 0; i < ARRAY_SIZE(b52_reg_map); i++) {
		if ((addr >= b52_reg_map[i][0]) && (addr <= b52_reg_map[i][1]))
			goto addr_valid;
	}
	pr_err("b52-reg: address 0x%X not in ISP register map\n", addr);
	return -EINVAL;

addr_valid:
	if (rw_write) {
		val = tmp;
		b52_writeb(addr, val);
		out_cnt = snprintf(out_buf, PAGE_SIZE,
				"[0x%05X]: 0x%02X\n", addr, b52_readb(addr));
		pr_info("%s", out_buf);
	} else {
		cnt = tmp;
		if (cnt == 0)
			cnt = 1;
		else if (cnt > 1024)
			cnt = 1024;
		for (i = 0; i < cnt; i++) {
			offst += snprintf(out_buf + offst, PAGE_SIZE,
				"[0x%05X]: 0x%02X\n", addr, b52_readb(addr));
			addr++;
			if (offst >= PAGE_SIZE)
				break;
		}
		out_cnt = offst;
		pr_info("\n%s", out_buf);
	}
	return length;
}


static const struct file_operations b52_reg_proc_ops = {
	.read	= b52_proc_read_reg,
	.write	= b52_proc_write_reg,
};


static int __init b52_reg_init(void)
{
	b52_reg_proc = proc_create(
			B52_REG_PROC_FILE, 0644, NULL, &b52_reg_proc_ops);
	if (!b52_reg_proc)
		pr_err("proc file create failed!\n");

	return device_register(&test_dev);
}

static void __exit b52_reg_cleanup(void)
{
	device_unregister(&test_dev);
	remove_proc_entry(B52_REG_PROC_FILE, NULL);
}

module_init(b52_reg_init);
module_exit(b52_reg_cleanup);

MODULE_DESCRIPTION("b52 configuration helper");
MODULE_AUTHOR("Jianle Wang <wangjl@marvell.com>");
