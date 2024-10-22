/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef COMMON_DEF_ID_H
#define COMMON_DEF_ID_H

#define UARTHUB_INFO_LOG  1
#define UARTHUB_DEBUG_LOG 0

#define DBG_LOG_LEN    1024
#define BIT_0xFFFF_FFFF  0xFFFFFFFF
#define BIT_0x7FFF_FFFF  0x7FFFFFFF

/* CR control definition */
#define GET_BIT_MASK(value, mask) \
	((value) & (mask))
#define SET_BIT_MASK(pdest, value, mask) \
	(*(pdest) = (GET_BIT_MASK(*(pdest), ~(mask)) | GET_BIT_MASK(value, mask)))
#define UARTHUB_SET_BIT(REG, BITVAL) \
	(*((unsigned int *)(REG)) |= ((unsigned int)(BITVAL)))
#define UARTHUB_CLR_BIT(REG, BITVAL) \
	((*(unsigned int *)(REG)) &= ~((unsigned int)(BITVAL)))
#define UARTHUB_REG_READ(addr) \
	(*((unsigned int *)(addr)))
#define UARTHUB_REG_READ_BIT(addr, BITVAL) \
	(*((unsigned int *)(addr)) & ((unsigned int)(BITVAL)))
#define UARTHUB_REG_WRITE(addr, data) do {\
	writel(data, (void *)addr); \
	mb(); /* make sure register access in order */ \
} while (0)
#define UARTHUB_REG_WRITE_MASK(reg, data, mask) {\
	unsigned int val = UARTHUB_REG_READ(reg); \
	SET_BIT_MASK(&val, data, mask); \
	UARTHUB_REG_WRITE(reg, val); \
}

/* CODA definition */
#define REG_FLD(width, shift) \
	((unsigned int)((((width) & 0xFF) << 16) | ((shift) & 0xFF)))

#define REG_FLD_WIDTH(field) \
	((unsigned int)(((field) >> 16) & 0xFF))

#define REG_FLD_SHIFT(field) \
	((unsigned int)((field) & 0xFF))

#define REG_FLD_MASK(field) \
	((unsigned int)((1ULL << REG_FLD_WIDTH(field)) - 1) << REG_FLD_SHIFT(field))

#define REG_FLD_VAL(field, val) \
	(((val) << REG_FLD_SHIFT(field)) & REG_FLD_MASK(field))

#define REG_FLD_GET(field, reg32) ((readl((void __iomem *)(reg32)) & REG_FLD_MASK(field)) \
	>> REG_FLD_SHIFT(field))

#define REG_FLD_SET(field, reg32, val)  \
	writel((readl((void __iomem *)(reg32)) &  \
	~REG_FLD_MASK(field))|REG_FLD_VAL((field), (val)), (void __iomem *)(reg32))

#define REG_FLD_RD_SET(field, reg32_rd, reg32_wr, val)  \
	writel((readl((void __iomem *)(reg32_rd)) &  \
	~REG_FLD_MASK(field))|REG_FLD_VAL((field), (val)), (void __iomem *)(reg32_wr))

#define UARTHUB_READ_GPIO(_var, _base, _remap_base, _offset, _mask, _value)\
	_var.addr = _base + _offset;\
	_var.mask = _mask;\
	_var.value = _value;\
	_var.gpio_value = UARTHUB_REG_READ(_remap_base + _offset)

#define UARTHUB_READ_GPIO_BIT(_var, _base, _remap_base, _offset, _mask, _shift)\
	_var.addr = _base + _offset;\
	_var.mask = _mask;\
	_var.gpio_value = (UARTHUB_REG_READ_BIT(_remap_base + _offset, _mask) >> _shift)

/* struct definition */
struct uarthub_uartip_debug_info {
	unsigned int dev0;
	unsigned int dev1;
	unsigned int dev2;
	unsigned int cmm;
	unsigned int ap;
};

enum uarthub_trx_type {
	TX = 0,
	RX,
	TRX,
	TRX_NONE,
};

enum uarthub_pkt_fmt_type {
	pkt_fmt_dev0 = 0,
	pkt_fmt_dev1,
	pkt_fmt_dev2,
	pkt_fmt_esp,
	pkt_fmt_undef,
};

/* UART_IP CODA definition */
#define DEBUG_1(_baseaddr) (_baseaddr+0x64)
#define DEBUG_2(_baseaddr) (_baseaddr+0x68)
#define DEBUG_3(_baseaddr) (_baseaddr+0x6c)
#define DEBUG_4(_baseaddr) (_baseaddr+0x70)
#define DEBUG_5(_baseaddr) (_baseaddr+0x74)
#define DEBUG_6(_baseaddr) (_baseaddr+0x78)
#define DEBUG_7(_baseaddr) (_baseaddr+0x7c)
#define DEBUG_8(_baseaddr) (_baseaddr+0x80)

/* UARTHUB ERROR ID */
#define UARTHUB_ERR_APB_BUS_CLK_DISABLE    (-100)
#define UARTHUB_ERR_HUB_CLK_DISABLE        (-101)
#define UARTHUB_ERR_PLAT_API_NOT_EXIST     (-102)
#define UARTHUB_ERR_DEV_INDEX_NOT_SUPPORT  (-103)
#define UARTHUB_ERR_ENUM_NOT_SUPPORT       (-104)
#define UARTHUB_ERR_MUTEX_LOCK_FAIL        (-105)
#define UARTHUB_ERR_PARA_WRONG             (-106)
#define UARTHUB_ERR_PORT_NO_NOT_SUPPORT    (-107)

/* UARTHUB UT CASE ERROR ID */
#define UARTHUB_UT_ERR_HUB_READY_STA       (-1000)
#define UARTHUB_UT_ERR_TX_FAIL             (-1001)
#define UARTHUB_UT_ERR_RX_FAIL             (-1002)
#define UARTHUB_UT_ERR_IRQ_STA_FAIL        (-1003)
#define UARTHUB_UT_ERR_INTFHUB_ACTIVE      (-1004)
#define UARTHUB_UT_ERR_FIFO_SIZE_FAIL      (-1005)

#endif /* COMMON_DEF_ID_H */
