/*
 * Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 * Filename : gnss_common.h
 * Abstract : This file is a implementation for driver of gnss:
 *
 * Authors  : zhaohui.chen
 *
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __GNSS_COMMON_H__
#define __GNSS_COMMON_H__

#ifndef CONFIG_WCN_INTEG
/* begin: address map on gnss side, operate by SDIO BUS */
/* set(s)/clear(c) */
#define GNSS_SET_OFFSET                 0x1000
#define GNSS_CLEAR_OFFSET               0x2000

#define GNSS_APB_BASE              0x40bc8000
#define REG_GNSS_APB_MCU_AP_RST        (GNSS_APB_BASE + 0x0280) /* s/c */
#define BIT_GNSS_APB_MCU_AP_RST_SOFT    (1<<0)    /* bit0 */

#define GNSS_INDIRECT_OP_REG		0x40b20000

#define GNSS_AHB_BASE			   0x40b18000
#define GNSS_ARCH_EB_REG		   (GNSS_AHB_BASE + 0x084)
#define GNSS_ARCH_EB_REG_BYPASS    (1<<1)

#ifdef CONFIG_UMW2652
#define GNSS_CALI_ADDRESS 0x40aabf4c
#define GNSS_CALI_DATA_SIZE 0x1c
#else
#define GNSS_CALI_ADDRESS 0x40aaff4c
#define GNSS_CALI_DATA_SIZE 0x14
#endif
#define GNSS_CALI_DONE_FLAG 0x1314520

#ifdef CONFIG_UMW2652
#define GNSS_EFUSE_ADDRESS 0x40aabf40
#else
#define GNSS_EFUSE_ADDRESS 0x40aaff40
#endif

#define GNSS_EFUSE_DATA_SIZE 0xc

/*  GNSS assert workaround */
#ifdef CONFIG_UMW2652
#define GNSS_BOOTSTATUS_ADDRESS  0x40aabf6c
#else
#define GNSS_BOOTSTATUS_ADDRESS  0x40aaff6c
#endif
#define GNSS_BOOTSTATUS_SIZE     0x4
#define GNSS_BOOTSTATUS_MAGIC    0x12345678

/* end: address map on gnss side */

int gnss_write_data(void);
int gnss_backup_data(void);
void gnss_file_path_set(char *buf);
#endif

#if (defined(CONFIG_UMW2652) || defined(CONFIG_UMW2631_I))

#define SC2730_PIN_REG_BASE     0x0480
#define PTEST0			0x0
#define PTEST0_MASK		(BIT(4) | BIT(5))
#define PTEST0_sel(x)		(((x)&0x3)<<4)

#define REGS_ANA_APB_BASE	0x1800
#define XTL_WAIT_CTRL0		0x378
#define BIT_XTL_EN		BIT(8)

#define TSEN_CTRL0		0x334
#define BIT_TSEN_CLK_SRC_SEL	BIT(4)
#define BIT_TSEN_ADCLDO_EN	BIT(15)

#define TSEN_CTRL1		 0x338
#define BIT_TSEN_CLK_EN		BIT(7)
#define BIT_TSEN_SDADC_EN	BIT(11)
#define BIT_TSEN_UGBUF_EN	BIT(14)

#define TSEN_CTRL2		0x33c
#define TSEN_CTRL3		0x340
#define BIT_TSEN_EN		BIT(0)
#define BIT_TSEN_TIME_SEL_MASK  (BIT(4) | BIT(5))
#define BIT_TSEN_TIME_sel(x)    (((x)&0x3)<<4)

#define TSEN_CTRL4		0x344
#define TSEN_CTRL5		0x348
#define CLK32KLESS_CTRL0	0x368
#define M26_TSX_32KLESS		0x8010


#define UMP7522_REGS_ANA_APB_BASE	 0x2000
#define UMP7522_TSEN_CTRL1		     0x00FC
#define UMP7522_BIT_RG_CLK_26M_TSEN  BIT(0)
#define UMP7522_BIT_TESN_SDADC_EN    BIT(4)

#define UMP7522_TSEN_CTRL3		     0x0104
#define UMP7522_BIT_TESE_ADCLDO_EN   BIT(12)
#define UMP7522_BIT_TSEN_UGBUF_EN    BIT(8)
#define UMP7522_BIT_TSEN_EN          BIT(4)

#define UMP7522_TSEN_CTRL6           0x0110
#define UMP7522_BIT_TESN_SEL_EN      BIT(6)

#define UMP7522_TSEN_CTRL4      0x0108
#define UMP7522_TSEN_CTRL5      0x010C

enum{
	TSEN_EXT,
	TSEN_INT,
};

/* qogirl6 begin */
/* for tcxo */
#define XTLBUF3_REL_CFG_ADDR    0x640209e0
#define XTLBUF3_REL_CFG_OFFSET  0x9e0
#define WCN_XTL_CTRL_ADDR     0x64020fe4
#define WCN_XTL_CTRL_OFFSET    0xfe4
/* qogirl6 end */

#endif

#if (defined(CONFIG_UMW2652) || defined(CONFIG_UMW2631_I) \
	|| defined(CONFIG_SC2355))
#define PMIC_CHIPID_SC27XX     (0x2730)
#define PMIC_CHIPID_UMP9622    (0x7522)
#endif

bool gnss_delay_ctl(void);

#endif
