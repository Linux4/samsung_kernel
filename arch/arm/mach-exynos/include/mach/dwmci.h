/* linux/arch/arm/mach-exynos4/include/mach/dwmci.h
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * Synopsys DesignWare Mobile Storage for EXYNOS4210
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_ARM_ARCH_DWMCI_H
#define __ASM_ARM_ARCH_DWMCI_H __FILE__

#include <linux/mmc/dw_mmc.h>

#define DWMCI_CTRL		0x000
#define DWMCI_PWREN		0x004
#define DWMCI_CLKDIV		0x008
#define DWMCI_CLKSRC		0x00c
#define DWMCI_CLKENA		0x010
#define DWMCI_TMOUT		0x014
#define DWMCI_CTYPE		0x018
#define DWMCI_BLKSIZ		0x01c
#define DWMCI_BYTCNT		0x020
#define DWMCI_INTMASK		0x024
#define DWMCI_CMDARG		0x028
#define DWMCI_CMD		0x02c
#define DWMCI_RESP0		0x030
#define DWMCI_RESP1		0x034
#define DWMCI_RESP2		0x038
#define DWMCI_RESP3		0x03c
#define DWMCI_MINTSTS		0x040
#define DWMCI_RINTSTS		0x044
#define DWMCI_STATUS		0x048
#define DWMCI_FIFOTH		0x04c
#define DWMCI_CDETECT		0x050
#define DWMCI_WRTPRT		0x054
#define DWMCI_GPIO		0x058
#define DWMCI_TCBCNT		0x05c
#define DWMCI_TBBCNT		0x060
#define DWMCI_DEBNCE		0x064
#define DWMCI_USRID		0x068
#define DWMCI_VERID		0x06c
#define DWMCI_HCON		0x070
#define DWMCI_UHS_REG		0x074
#define DWMCI_BMOD		0x080
#define DWMCI_PLDMND		0x084
#define DWMCI_DBADDR		0x088
#define DWMCI_IDSTS		0x08c
#define DWMCI_IDINTEN		0x090
#define DWMCI_DSCADDR		0x094
#define DWMCI_BUFADDR		0x098
#define DWMCI_CLKSEL		0x09c
#define DWMCI_CDTHRCTL		0x100

/* Command register defines */
#define DWMCI_CMD_START			BIT(31)
#define DWMCI_USE_HOLD_REG		BIT(29)
#define DWMCI_VOLT_SWITCH		BIT(28)
#define DWMCI_CMD_CCS_EXP		BIT(23)
#define DWMCI_CMD_CEATA_RD		BIT(22)
#define DWMCI_CMD_UPD_CLK		BIT(21)
#define DWMCI_CMD_INIT			BIT(15)
#define DWMCI_CMD_STOP			BIT(14)
#define DWMCI_CMD_PRV_DAT_WAIT		BIT(13)
#define DWMCI_CMD_SEND_STOP		BIT(12)
#define DWMCI_CMD_STRM_MODE		BIT(11)
#define DWMCI_CMD_DAT_WR		BIT(10)
#define DWMCI_CMD_DAT_EXP		BIT(9)
#define DWMCI_CMD_RESP_CRC		BIT(8)
#define DWMCI_CMD_RESP_LONG		BIT(7)
#define DWMCI_CMD_RESP_EXP		BIT(6)
#define DWMCI_CMD_INDX(n)		((n) & 0x1F)

#define DWMCI_CLKSEL	0x09c

#define DWMCI_DDR200_EMMC_DDR_REG	0x10C
#define DWMCI_DDR200_ENABLE_SHIFT	0x110

#define DWMCI_DDR200_RDDQS_EN		0x110
#define DWMCI_DDR200_ASYNC_FIFO_CTRL	0x114
#define DWMCI_DDR200_DLINE_CTRL		0x118
/* DDR200 RDDQS Enable*/
#define DWMCI_TXDT_CRC_TIMER_FASTLIMIT(x)	(((x) & 0xFF) << 16)
#define DWMCI_TXDT_CRC_TIMER_INITVAL(x)		(((x) & 0xFF) << 8)
#define DWMCI_BUSY_CHK_CLK_STOP_EN		BIT(2)
#define DWMCI_RXDATA_START_BIT_SEL		BIT(1)
#define DWMCI_RDDQS_EN				BIT(0)
#define DWMCI_DDR200_RDDQS_EN_DEF	DWMCI_TXDT_CRC_TIMER_FASTLIMIT(0x12) | \
					DWMCI_TXDT_CRC_TIMER_INITVAL(0x14)
#define DWMCI_DDR200_DLINE_CTRL_DEF	DWMCI_FIFO_CLK_DELAY_CTRL(0x2) | \
					DWMCI_RD_DQS_DELAY_CTRL(0x40)

/* DDR200 Async FIFO Control */
#define DWMCI_ASYNC_FIFO_RESET		BIT(0)

/* DDR200 DLINE Control */
#define DWMCI_FIFO_CLK_DELAY_CTRL(x)	(((x) & 0x3) << 16)
#define DWMCI_RD_DQS_DELAY_CTRL(x)	((x) & 0x3FF)

/* SMU(Security Management Unit) Control */
#define DWMCI_EMMCP_BASE		0x1000
#define DWMCI_MPSECURITY		(DWMCI_EMMCP_BASE + 0x0010)
#define DWMCI_MPSBEGIN0			(DWMCI_EMMCP_BASE + 0x0200)
#define DWMCI_MPSEND0			(DWMCI_EMMCP_BASE + 0x0204)
#define DWMCI_MPSCTRL0			(DWMCI_EMMCP_BASE + 0x020C)

/* SMU control bits */
#define DWMCI_MPSCTRL_SECURE_READ_BIT		BIT(7)
#define DWMCI_MPSCTRL_SECURE_WRITE_BIT		BIT(6)
#define DWMCI_MPSCTRL_NON_SECURE_READ_BIT	BIT(5)
#define DWMCI_MPSCTRL_NON_SECURE_WRITE_BIT	BIT(4)
#define DWMCI_MPSCTRL_USE_FUSE_KEY		BIT(3)
#define DWMCI_MPSCTRL_ECB_MODE			BIT(2)
#define DWMCI_MPSCTRL_ENCRYPTION		BIT(1)
#define DWMCI_MPSCTRL_VALID			BIT(0)

extern void exynos_register_notifier(void *data);
extern void exynos_unregister_notifier(void *data);
extern void exynos_dwmci_set_platdata(struct dw_mci_board *pd, u32 slot_id);

#endif /* __ASM_ARM_ARCH_DWMCI_H */
