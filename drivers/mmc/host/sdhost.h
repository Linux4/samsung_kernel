/*
 *  linux/drivers/mmc/host/sdhost.h - Secure Digital Host Controller Interface driver
 *
 *  Copyright (C) 2014-2014 Jason.Wu(Jishuang.Wu), All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 */

#ifndef __SDHOST_H_
#define __SDHOST_H_

#include <linux/scatterlist.h>
#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/io.h>

#include <linux/clk.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/host.h>
#include <linux/mmc/card.h>
#include <linux/mmc/slot-gpio.h>

/******************************************************************************
* Controller flag
*------------------------------------------------------------------------------
*/
#define SDHOST_FLAG_ENABLE_ACMD12	0
#define SDHOST_FLAG_ENABLE_ACMD23	0
#define SDHOST_FLAG_USE_ADMA		1
#define SDHOST_MAX_TIMEOUT  3
/*
 * Controller registers
 */
#if 1
#define __local_writel(v,base,reg)		writel((v),((base)+(reg)))
#define __local_writew(v,base,reg)		writew((v),((base)+(reg)))
#define __local_writeb(v,base,reg)		writeb((v),((base)+(reg)))
#define __local_readl(base,reg)			readl(((base)+(reg)))
#define __local_readw(base,reg)		readw(((base)+(reg)))
#define __local_readb(base,reg)		readb(((base)+(reg)))
#else
static inline void __local_writel(uint32_t val, void __iomem * ioaddr, uint32_t reg)
{
	writel(val, (ioaddr + reg));
}

static inline void __local_writew(uint16_t val, void __iomem * ioaddr, uint32_t reg)
{
	uint32_t addr;
	uint32_t value;
	uint32_t ofst;

	ofst = (reg & 0x3) << 3;
	addr = reg & (~((uint32_t) (0x3)));
	value = readl((ioaddr + addr));
	value &= (~(((uint32_t) ((uint16_t) (-1))) << ofst));
	value |= (((uint32_t) val) << ofst);
	writel(value, (ioaddr + addr));
}

static inline void __local_writeb(uint8_t val, void __iomem * ioaddr, uint32_t reg)
{
	uint32_t addr;
	uint32_t value;
	uint32_t ofst;

	ofst = (reg & 0x3) << 3;
	addr = reg & (~((uint32_t) (0x3)));
	value = readl((ioaddr + addr));
	value &= (~(((uint32_t) ((uint8_t) (-1))) << ofst));
	value |= (((uint32_t) val) << ofst);
	writel(value, (ioaddr + addr));
}

static inline uint32_t __local_readl(void __iomem * ioaddr, uint32_t reg)
{
	return readl((ioaddr + reg));
}

static inline uint16_t __local_readw(void __iomem * ioaddr, uint32_t reg)
{
	uint32_t addr;
	uint32_t value;
	uint32_t ofst;

	ofst = (reg & 0x3) << 3;
	addr = reg & (~((uint32_t) (0x3)));
	value = readl((ioaddr + addr));
	return ((uint16_t) (value >> ofst));

}

static inline uint8_t __local_readb(void __iomem * ioaddr, uint32_t reg)
{
	uint32_t addr;
	uint32_t value;
	uint32_t ofst;

	ofst = (reg & 0x3) << 3;
	addr = reg & (~((uint32_t) (0x3)));
	value = readl((ioaddr + addr));
	return ((uint8_t) (value >> ofst));

}
#endif

#define _sdhost_writel(ioadr, val, reg)			__local_writel((val), (ioadr),(reg))
#define _sdhost_writew(ioadr, val, reg)			__local_writew((val), (ioadr),(reg))
#define _sdhost_writeb(ioadr, val, reg)			__local_writeb((val), (ioadr),(reg))
#define _sdhost_readl(ioadr, reg)				__local_readl( (ioadr),(reg))
#define _sdhost_readw(ioadr, reg)				__local_readw( (ioadr),(reg))
#define _sdhost_readb(ioadr, reg)				__local_readb( (ioadr),(reg))

#define SDHOST_32_SYS_ADDR	0x00
#define SDHOST_32_BLK_CNT	0x00	// used in cmd23 with ADMA in sdio 3.0
#define SDHOST_16_BLK_CNT	0x06
#define _sdhost_set_16_blk_cnt(ioadr,blkCnt)\
{\
	__local_writew(((blkCnt)&0xFFFF),(ioadr),SDHOST_16_BLK_CNT);\
}
#define _sdhost_set_32_blk_cnt(ioadr,blkCnt)\
{\
	__local_writel(((blkCnt)&0xFFFFFFFF),(ioadr),SDHOST_32_BLK_CNT);\
}
#define SDHOST_16_BLK_SIZE	0x04
#define _sdhost_set_blk_size(ioadr,blkSize)\
{\
	__local_writew(((blkSize)&0xFFF)|0x7000,(ioadr),SDHOST_16_BLK_SIZE);\
}

#define SDHOST_32_ARG			0x08
#define SDHOST_16_TR_MODE		0x0C
#define __ACMD_DIS	0x00
#define __ACMD12	0x01
#define __ACMD23	0x02
#define _sdhost_set_trans_mode(ioadr,ifMulti,ifRead,autoCmd,ifBlkCnt,ifDma) \
{\
	__local_writew(\
		((((ifMulti)?1:0)<<5)|		\
		(((ifRead)?1:0)<<4)|		\
		(((u16)(autoCmd))<<2)|	\
		(((ifBlkCnt)?1:0)<<1)|	\
		(((ifDma)?1:0)<<0)),		\
		(ioadr) , SDHOST_16_TR_MODE	\
	);	\
}
#define SDHOST_16_CMD			0x0E
#define _CMD_INDEX_CHK			0x0010
#define _CMD_CRC_CHK			0x0008
#define _CMD_RSP_NONE			0x0000
#define _CMD_RSP_136			0x0001
#define _CMD_RSP_48				0x0002
#define _CMD_RSP_48_BUSY		0x0003
#define _RSP0			0
#define _RSP1_5_6_7		_CMD_INDEX_CHK|_CMD_CRC_CHK|_CMD_RSP_48
#define _RSP2			_CMD_CRC_CHK|_CMD_RSP_136
#define _RSP3_4			_CMD_RSP_48
#define _RSP1B_5B		_CMD_INDEX_CHK|_CMD_CRC_CHK|_CMD_RSP_48_BUSY

#define _sdhost_set_cmd(ioadr,cmd,ifHasData,rspType)\
{\
	__local_writew(\
		((((u16)(cmd))<<8)|			\
		(((ifHasData)?1:0)<<5)|		\
		((u16)(rspType))),			\
		(ioadr) , SDHOST_16_CMD	\
	);	\
}

#define SDHOST_32_TR_MODE_AND_CMD		0x0C
#define _sdhost_set_trans_and_cmd(ioadr,ifMulti,ifRead,autoCmd,ifBlkCnt,ifDma,cmd,ifHasData,rspType)	\
{\
	__local_writel(					\
		((((ifMulti)?1:0)<<5)|			\
		(((ifRead)?1:0)<<4)|			\
		(((u32)(autoCmd))<<2)|		\
		(((ifBlkCnt)?1:0)<<1)|			\
		(((ifDma)?1:0)<<0)|			\
		(((u32)(cmd))<<24)|			\
		(((ifHasData)?1:0)<<21)|		\
		(((u32)(rspType))	<<16)),		\
		(ioadr), SDHOST_32_TR_MODE_AND_CMD\
	);\
}

#define SDHOST_32_RESPONSE	0x10
#define SDHOST_32_PRES_STATE	0x24
#define  _DATA_LVL_MASK		0x00F00000

#define SDHOST_8_HOST_CTRL	0x28
#define __8_BIT_MOD	0x20
#define __4_BIT_MOD	0x02
#define __1_BIT_MOD	0x00
#define __SDMA_MOD		0x00
#define __32ADMA_MOD	0x10
#define __64ADMA_MOD	0x18
#define __HISPD_MOD		0x04
static inline void _sdhost_set_buswidth(void __iomem * ioadr, uint32_t buswidth)
{
	u8 ctrl = 0;
	ctrl = __local_readb(ioadr, SDHOST_8_HOST_CTRL);
	ctrl &= (~(__8_BIT_MOD | __4_BIT_MOD | __1_BIT_MOD));
	switch (buswidth) {
	case MMC_BUS_WIDTH_1:
		ctrl |= __1_BIT_MOD;
		break;
	case MMC_BUS_WIDTH_4:
		ctrl |= __4_BIT_MOD;
		break;
	case MMC_BUS_WIDTH_8:
		ctrl |= __8_BIT_MOD;
		break;
	default:
		BUG_ON(1);
		break;
	}
	__local_writeb(ctrl, ioadr, SDHOST_8_HOST_CTRL);
}

static inline void _sdhost_set_DMA(void __iomem * ioadr, u8 dmaMod)
{
	u8 ctrl = 0;
	ctrl = __local_readb(ioadr, SDHOST_8_HOST_CTRL);
	ctrl &= (~(__SDMA_MOD | __32ADMA_MOD | __64ADMA_MOD));
	ctrl |= dmaMod;
	__local_writeb(ctrl, ioadr, SDHOST_8_HOST_CTRL);
}

static inline void _sdhost_enable_HISPD(void __iomem * ioadr)
{
	u8 ctrl = 0;
	ctrl = __local_readb(ioadr, SDHOST_8_HOST_CTRL);
	ctrl |= __HISPD_MOD;
	__local_writeb(ctrl, ioadr, SDHOST_8_HOST_CTRL);
}

//#define SDHOST_8_PWR_CTRL     0x29    // not used
#define SDHOST_8_BLK_GAP		0x2A	// not used
#define SDHOST_8_WACKUP_CTRL	0x2B	// not used

#define SDHOST_16_CLK_CTRL	0x2C
#define __CLK_IN_EN		0x0001
#define __CLK_IN_STABLE	0x0002
#define __CLK_SD			0x0004
#define __CLK_MAX_DIV		2046
static inline void _sdhost_AllClk_off(void __iomem * ioadr)
{
	__local_writew(0, ioadr, SDHOST_16_CLK_CTRL);
}

static inline void _sdhost_SDClk_off(void __iomem * ioadr)
{
	u16 ctrl = 0;
	ctrl = __local_readw(ioadr, SDHOST_16_CLK_CTRL);
	ctrl &= (~__CLK_SD);
	__local_writew(ctrl, ioadr, SDHOST_16_CLK_CTRL);
}

static inline void _sdhost_SDClk_on(void __iomem * ioadr)
{
	u16 ctrl = 0;
	ctrl = __local_readw(ioadr, SDHOST_16_CLK_CTRL);
	ctrl |= __CLK_SD;
	__local_writew(ctrl, ioadr, SDHOST_16_CLK_CTRL);
}

static inline uint32_t _sdhost_calcDiv(uint32_t baseClk, uint32_t clk)
{
	uint32_t N;

	if (baseClk <= clk) {
		return 0;
	}

	N = (uint32_t) (baseClk / clk);
	N = (N >> 1);
	if (N)
		N--;
	if ((baseClk / ((N + 1) << 1)) > clk) {
		N++;
	}
	if (__CLK_MAX_DIV < N) {
		N = __CLK_MAX_DIV;
	}
	return N;
}

static inline void _sdhost_Clk_set_and_on(void __iomem * ioadr, uint32_t div)
{
	u16 ctrl = 0;
	__local_writew(0, ioadr, SDHOST_16_CLK_CTRL);
	ctrl |= (uint16_t) (((div & 0x300) >> 2) | ((div & 0xFF) << 8));
	ctrl |= __CLK_IN_EN;
	__local_writew(ctrl, ioadr, SDHOST_16_CLK_CTRL);
	while (!(__CLK_IN_STABLE & __local_readw(ioadr, SDHOST_16_CLK_CTRL))) ;
}

#define SDHOST_8_TIMEOUT		0x2E
#define __DATA_TIMEOUT_MAX_VAL		0xe
static inline uint8_t _sdhost_calcTimeout(unsigned int clock, uint8_t TimeOutValue)
{
	unsigned target_timeout, current_timeout;
	uint8_t count = 0;
	target_timeout = TimeOutValue * clock;
	current_timeout = 1 << 16;
	while (target_timeout > current_timeout) {
		count++;
		current_timeout <<= 1;
	}
	count--;
	if (count >= 0xF)
		count = 0xE;
	return count;
}

#define SDHOST_8_RST			0x2F
#define  _RST_ALL		0x01
#define  _RST_CMD		0x02
#define  _RST_DATA		0x04
#define  _RST_EMMC		0x08	// spredtrum define it byself
#define _sdhost_reset(ioadr,mask)	\
{\
	__local_writeb((_RST_EMMC|(mask)), (ioadr) , SDHOST_8_RST);	\
	while (__local_readb((ioadr), SDHOST_8_RST) & (mask));	\
}
/* spredtrum define it byself */
#define _sdhost_reset_emmc(ioadr)	\
{\
	__local_writeb(0, (ioadr) , SDHOST_8_RST);\
	mdelay(2);\
	__local_writeb(_RST_EMMC, (ioadr) , SDHOST_8_RST);\
}

#define SDHOST_32_INT_ST		0x30
#define SDHOST_32_INT_ST_EN		0x34
#define SDHOST_32_INT_SIG_EN	0x38
#define _INT_CMD_END			0x00000001
#define _INT_TRAN_END			0x00000002
#define _INT_DMA_END			0x00000008
#define _INT_WR_RDY				0x00000010	// not used
#define _INT_RD_RDY				0x00000020	// not used
#define _INT_ERR				0x00008000
#define _INT_ERR_CMD_TIMEOUT	0x00010000
#define _INT_ERR_CMD_CRC		0x00020000
#define _INT_ERR_CMD_END		0x00040000
#define _INT_ERR_CMD_INDEX		0x00080000
#define _INT_ERR_DATA_TIMEOUT	0x00100000
#define _INT_ERR_DATA_CRC		0x00200000
#define _INT_ERR_DATA_END		0x00400000
#define _INT_ERR_CUR_LIMIT		0x00800000
#define _INT_ERR_ACMD			0x01000000
#define _INT_ERR_ADMA			0x02000000

// used in irq
#define _INT_FILTER_ERR_CMD	(_INT_ERR_CMD_TIMEOUT|_INT_ERR_CMD_CRC|_INT_ERR_CMD_END|_INT_ERR_CMD_INDEX)
#define _INT_FILTER_ERR_DATA	(_INT_ERR_DATA_TIMEOUT|_INT_ERR_DATA_CRC|_INT_ERR_DATA_END)
#define _INT_FILTER_ERR		(_INT_ERR|_INT_FILTER_ERR_CMD|_INT_FILTER_ERR_DATA|_INT_ERR_ACMD|_INT_ERR_ADMA)
#define _INT_FILTER_NORMAL	(_INT_CMD_END|_INT_TRAN_END)

// used for setting
#define _DATA_FILTER_RD_SIGLE		_INT_TRAN_END|_INT_DMA_END|_INT_ERR|_INT_ERR_DATA_TIMEOUT|_INT_ERR_DATA_CRC|_INT_ERR_DATA_END
#define _DATA_FILTER_RD_MULTI		_INT_TRAN_END|_INT_DMA_END|_INT_ERR|_INT_ERR_DATA_TIMEOUT|_INT_ERR_DATA_CRC|_INT_ERR_DATA_END
#define _DATA_FILTER_WR_SIGLE		_INT_TRAN_END|_INT_DMA_END|_INT_ERR|_INT_ERR_DATA_TIMEOUT|_INT_ERR_DATA_CRC
#define _DATA_FILTER_WR_MULT		_INT_TRAN_END|_INT_DMA_END|_INT_ERR|_INT_ERR_DATA_TIMEOUT|_INT_ERR_DATA_CRC
#define _CMD_FILTER_R0				_INT_CMD_END
#define _CMD_FILTER_R2				_INT_CMD_END|_INT_ERR|_INT_ERR_CMD_TIMEOUT|_INT_ERR_CMD_CRC|_INT_ERR_CMD_END
#define _CMD_FILTER_R3				_INT_CMD_END|_INT_ERR|_INT_ERR_CMD_TIMEOUT|_INT_ERR_CMD_END
#define _CMD_FILTER_R1_R4_R5_R6_R7	_INT_CMD_END|_INT_ERR|_INT_ERR_CMD_TIMEOUT|_INT_ERR_CMD_CRC|_INT_ERR_CMD_END|_INT_ERR_CMD_INDEX
#define _CMD_FILTER_R1B				_INT_CMD_END|_INT_ERR|_INT_ERR_CMD_TIMEOUT|_INT_ERR_CMD_CRC|_INT_ERR_CMD_END|_INT_ERR_CMD_INDEX|_INT_TRAN_END|_INT_ERR_DATA_TIMEOUT

#define _sdhost_disableall_int(ioadr)	\
{\
	__local_writel(0x0, (ioadr) , SDHOST_32_INT_SIG_EN);		\
	__local_writel(0x0, (ioadr) , SDHOST_32_INT_ST_EN);		\
	__local_writel(0xFFFFFFFF, (ioadr) , SDHOST_32_INT_ST);	\
}
#define _sdhost_enable_int(ioadr,mask)			\
{\
	__local_writel((mask), (ioadr) , SDHOST_32_INT_ST_EN);		\
	__local_writel((mask), (ioadr) , SDHOST_32_INT_SIG_EN);		\
}

#define _sdhost_clear_int(ioadr,mask)			\
{\
	__local_writel((mask), (ioadr) , SDHOST_32_INT_ST);			\
}

#define SDHOST_16_ACMD_ERR		0x3C

#define SDHOST_16_HOST_CTRL_2	0x3E
#define __TIMING_MODE_SDR12		0x0000
#define __TIMING_MODE_SDR25		0x0001
#define __TIMING_MODE_SDR50		0x0002
#define __TIMING_MODE_SDR104	0x0003
#define __TIMING_MODE_DDR50		0x0004
#define __TIMING_MODE_SDR200	0x0005
#define _sdhost_set_UHS_mode(ioadr,mode)	\
{\
	__local_writew((mode),(ioadr),SDHOST_16_HOST_CTRL_2);\
}

#define SDHOST_MAX_CUR	1020

// following register is defined by spreadtrum self.It is not standard register of SDIO.
#define _sdhost_set_delay(ioadr,writeDelay,readPosDelay,readNegDelay)	\
{\
	__local_writel((writeDelay),(ioadr),0x80);\
	__local_writel((readPosDelay),(ioadr),0x84);\
	__local_writel((readNegDelay),(ioadr),0x88);\
}

#define SDHOST_8_SLT_INT_ST	0xFC

/******************************************************************************
* Controller block structure
*------------------------------------------------------------------------------
*/

struct sdhost_host {
//--globe resource---
	spinlock_t lock;
	struct mmc_host *mmc;
//--basic resource---
	void __iomem *ioaddr;
	int irq;
	const char *deviceName;
	int detect_gpio;
	const char *SD_Pwr_Name;
	const char *_1_8V_signal_Name;
	uint32_t ocr_avail;
	char *clkName;
	char *clkParentName;
	uint32_t base_clk;
	uint32_t caps;
	uint32_t caps2;
	uint32_t pm_caps;
	uint32_t writeDelay;
	uint32_t readPosDelay;
	uint32_t readNegDelay;
//--extern resource getted by base resource---
	uint64_t dma_mask;
	uint8_t dataTimeOutVal;
	struct regulator *SD_pwr;
	struct regulator *_1_8V_signal;
	uint32_t signal_default_Voltage;
	bool _1_8V_signal_enabled;
	struct clk *clk;
	struct clk *clk_parent;
	struct tasklet_struct finish_tasklet;
	struct timer_list timer;
//--runtime param---
	uint32_t int_filter;
	struct mmc_ios ios;
	struct mmc_request *mrq;	/* Current request */
	struct mmc_command *cmd;	/* Current command */
	uint16_t autoCmdMode;
//--debugfs---
	struct dentry *debugfs_root;
};

#endif /* __SDHOST_H_ */
