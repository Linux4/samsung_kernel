/* the head file modifier:     g   2015-03-26 16:06:16*/

/*
* Copyright (C) 2013 Spreadtrum Communications Inc.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
*************************************************
* Automatically generated C header: do not edit *
*************************************************
*/

#ifndef __SCI_GLB_REGS_H__
#error "Don't include this file directly, Pls include sci_glb_regs.h"
#endif


#ifndef __H_REGS_AP_CLK_HEADFILE_H__
#define __H_REGS_AP_CLK_HEADFILE_H__ __FILE__



#define REG_AP_CLK_AP_APB_CFG     SCI_ADDR(REGS_AP_CLK_BASE, 0x0020 )
#define REG_AP_CLK_NANDC_2X_CFG   SCI_ADDR(REGS_AP_CLK_BASE, 0x0024 )
#define REG_AP_CLK_NANDC_1X_CFG   SCI_ADDR(REGS_AP_CLK_BASE, 0x0028 )
#define REG_AP_CLK_NANDC_ECC_CFG  SCI_ADDR(REGS_AP_CLK_BASE, 0x002C )
#define REG_AP_CLK_OTG2_UTMI_CFG  SCI_ADDR(REGS_AP_CLK_BASE, 0x0030 )
#define REG_AP_CLK_USB3_UTMI_CFG  SCI_ADDR(REGS_AP_CLK_BASE, 0x0034 )
#define REG_AP_CLK_USB3_PIPE_CFG  SCI_ADDR(REGS_AP_CLK_BASE, 0x0038 )
#define REG_AP_CLK_USB3_REF_CFG   SCI_ADDR(REGS_AP_CLK_BASE, 0x003C )
#define REG_AP_CLK_HSIC_UTMI_CFG  SCI_ADDR(REGS_AP_CLK_BASE, 0x0040 )
#define REG_AP_CLK_ZIPENC_CFG     SCI_ADDR(REGS_AP_CLK_BASE, 0x0044 )
#define REG_AP_CLK_ZIPDEC_CFG     SCI_ADDR(REGS_AP_CLK_BASE, 0x0048 )
#define REG_AP_CLK_UART0_CFG      SCI_ADDR(REGS_AP_CLK_BASE, 0x004C )
#define REG_AP_CLK_UART1_CFG      SCI_ADDR(REGS_AP_CLK_BASE, 0x0050 )
#define REG_AP_CLK_UART2_CFG      SCI_ADDR(REGS_AP_CLK_BASE, 0x0054 )
#define REG_AP_CLK_UART3_CFG      SCI_ADDR(REGS_AP_CLK_BASE, 0x0058 )
#define REG_AP_CLK_UART4_CFG      SCI_ADDR(REGS_AP_CLK_BASE, 0x005C )
#define REG_AP_CLK_I2C0_CFG       SCI_ADDR(REGS_AP_CLK_BASE, 0x0060 )
#define REG_AP_CLK_I2C1_CFG       SCI_ADDR(REGS_AP_CLK_BASE, 0x0064 )
#define REG_AP_CLK_I2C2_CFG       SCI_ADDR(REGS_AP_CLK_BASE, 0x0068 )
#define REG_AP_CLK_I2C3_CFG       SCI_ADDR(REGS_AP_CLK_BASE, 0x006C )
#define REG_AP_CLK_I2C4_CFG       SCI_ADDR(REGS_AP_CLK_BASE, 0x0070 )
#define REG_AP_CLK_I2C5_CFG       SCI_ADDR(REGS_AP_CLK_BASE, 0x0074 )
#define REG_AP_CLK_SPI0_CFG       SCI_ADDR(REGS_AP_CLK_BASE, 0x0078 )
#define REG_AP_CLK_SPI1_CFG       SCI_ADDR(REGS_AP_CLK_BASE, 0x007C )
#define REG_AP_CLK_SPI2_CFG       SCI_ADDR(REGS_AP_CLK_BASE, 0x0080 )
#define REG_AP_CLK_IIS0_CFG       SCI_ADDR(REGS_AP_CLK_BASE, 0x0084 )
#define REG_AP_CLK_IIS1_CFG       SCI_ADDR(REGS_AP_CLK_BASE, 0x0088 )
#define REG_AP_CLK_IIS2_CFG       SCI_ADDR(REGS_AP_CLK_BASE, 0x008C )
#define REG_AP_CLK_IIS3_CFG       SCI_ADDR(REGS_AP_CLK_BASE, 0x0090 )
#define REG_AP_CLK_BIST_192M_CFG  SCI_ADDR(REGS_AP_CLK_BASE, 0x0094 )
#define REG_AP_CLK_BIST_256M_CFG  SCI_ADDR(REGS_AP_CLK_BASE, 0x0098 )

/*---------------------------------------------------------------------------
// Register Name   : REG_AP_CLK_AP_APB_CFG
// Register Offset : 0x0020
// Description     :
---------------------------------------------------------------------------*/

#define BIT_AP_CLK_CGM_AP_APB_SEL(x)                  (((x) & 0x3))

/*---------------------------------------------------------------------------
// Register Name   : REG_AP_CLK_NANDC_2X_CFG
// Register Offset : 0x0024
// Description     :
---------------------------------------------------------------------------*/

#define BIT_AP_CLK_CGM_NANDC_2X_DIV(x)                (((x) & 0x7) << 8)
#define BIT_AP_CLK_CGM_NANDC_2X_SEL(x)                (((x) & 0x3))

/*---------------------------------------------------------------------------
// Register Name   : REG_AP_CLK_NANDC_1X_CFG
// Register Offset : 0x0028
// Description     :
---------------------------------------------------------------------------*/

#define BIT_AP_CLK_CGM_NANDC_1X_DIV                   BIT(8)
#define BIT_AP_CLK_CGM_NANDC_1X_SEL(x)                (((x) & 0x3))

/*---------------------------------------------------------------------------
// Register Name   : REG_AP_CLK_NANDC_ECC_CFG
// Register Offset : 0x002C
// Description     :
---------------------------------------------------------------------------*/

#define BIT_AP_CLK_CGM_NANDC_ECC_SEL                  BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AP_CLK_OTG2_UTMI_CFG
// Register Offset : 0x0030
// Description     :
---------------------------------------------------------------------------*/

#define BIT_AP_CLK_CGM_OTG2_UTMI_PAD_SEL              BIT(16)

/*---------------------------------------------------------------------------
// Register Name   : REG_AP_CLK_USB3_UTMI_CFG
// Register Offset : 0x0034
// Description     :
---------------------------------------------------------------------------*/

#define BIT_AP_CLK_CGM_USB3_UTMI_PAD_SEL              BIT(16)

/*---------------------------------------------------------------------------
// Register Name   : REG_AP_CLK_USB3_PIPE_CFG
// Register Offset : 0x0038
// Description     :
---------------------------------------------------------------------------*/

#define BIT_AP_CLK_CGM_USB3_PIPE_PAD_SEL              BIT(16)

/*---------------------------------------------------------------------------
// Register Name   : REG_AP_CLK_USB3_REF_CFG
// Register Offset : 0x003C
// Description     :
---------------------------------------------------------------------------*/

#define BIT_AP_CLK_CGM_USB3_REF_SEL                   BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AP_CLK_HSIC_UTMI_CFG
// Register Offset : 0x0040
// Description     :
---------------------------------------------------------------------------*/

#define BIT_AP_CLK_CGM_HSIC_UTMI_PAD_SEL              BIT(16)

/*---------------------------------------------------------------------------
// Register Name   : REG_AP_CLK_ZIPENC_CFG
// Register Offset : 0x0044
// Description     :
---------------------------------------------------------------------------*/

#define BIT_AP_CLK_CGM_ZIPENC_SEL(x)                  (((x) & 0x3))

/*---------------------------------------------------------------------------
// Register Name   : REG_AP_CLK_ZIPDEC_CFG
// Register Offset : 0x0048
// Description     :
---------------------------------------------------------------------------*/

#define BIT_AP_CLK_CGM_ZIPDEC_SEL(x)                  (((x) & 0x3))

/*---------------------------------------------------------------------------
// Register Name   : REG_AP_CLK_UART0_CFG
// Register Offset : 0x004C
// Description     :
---------------------------------------------------------------------------*/

#define BIT_AP_CLK_CGM_UART0_DIV(x)                   (((x) & 0x7) << 8)
#define BIT_AP_CLK_CGM_UART0_SEL(x)                   (((x) & 0x3))

/*---------------------------------------------------------------------------
// Register Name   : REG_AP_CLK_UART1_CFG
// Register Offset : 0x0050
// Description     :
---------------------------------------------------------------------------*/

#define BIT_AP_CLK_CGM_UART1_DIV(x)                   (((x) & 0x7) << 8)
#define BIT_AP_CLK_CGM_UART1_SEL(x)                   (((x) & 0x3))

/*---------------------------------------------------------------------------
// Register Name   : REG_AP_CLK_UART2_CFG
// Register Offset : 0x0054
// Description     :
---------------------------------------------------------------------------*/

#define BIT_AP_CLK_CGM_UART2_DIV(x)                   (((x) & 0x7) << 8)
#define BIT_AP_CLK_CGM_UART2_SEL(x)                   (((x) & 0x3))

/*---------------------------------------------------------------------------
// Register Name   : REG_AP_CLK_UART3_CFG
// Register Offset : 0x0058
// Description     :
---------------------------------------------------------------------------*/

#define BIT_AP_CLK_CGM_UART3_DIV(x)                   (((x) & 0x7) << 8)
#define BIT_AP_CLK_CGM_UART3_SEL(x)                   (((x) & 0x3))

/*---------------------------------------------------------------------------
// Register Name   : REG_AP_CLK_UART4_CFG
// Register Offset : 0x005C
// Description     :
---------------------------------------------------------------------------*/

#define BIT_AP_CLK_CGM_UART4_DIV(x)                   (((x) & 0x7) << 8)
#define BIT_AP_CLK_CGM_UART4_SEL(x)                   (((x) & 0x3))

/*---------------------------------------------------------------------------
// Register Name   : REG_AP_CLK_I2C0_CFG
// Register Offset : 0x0060
// Description     :
---------------------------------------------------------------------------*/

#define BIT_AP_CLK_CGM_I2C0_DIV(x)                    (((x) & 0x7) << 8)
#define BIT_AP_CLK_CGM_I2C0_SEL(x)                    (((x) & 0x3))

/*---------------------------------------------------------------------------
// Register Name   : REG_AP_CLK_I2C1_CFG
// Register Offset : 0x0064
// Description     :
---------------------------------------------------------------------------*/

#define BIT_AP_CLK_CGM_I2C1_DIV(x)                    (((x) & 0x7) << 8)
#define BIT_AP_CLK_CGM_I2C1_SEL(x)                    (((x) & 0x3))

/*---------------------------------------------------------------------------
// Register Name   : REG_AP_CLK_I2C2_CFG
// Register Offset : 0x0068
// Description     :
---------------------------------------------------------------------------*/

#define BIT_AP_CLK_CGM_I2C2_DIV(x)                    (((x) & 0x7) << 8)
#define BIT_AP_CLK_CGM_I2C2_SEL(x)                    (((x) & 0x3))

/*---------------------------------------------------------------------------
// Register Name   : REG_AP_CLK_I2C3_CFG
// Register Offset : 0x006C
// Description     :
---------------------------------------------------------------------------*/

#define BIT_AP_CLK_CGM_I2C3_DIV(x)                    (((x) & 0x7) << 8)
#define BIT_AP_CLK_CGM_I2C3_SEL(x)                    (((x) & 0x3))

/*---------------------------------------------------------------------------
// Register Name   : REG_AP_CLK_I2C4_CFG
// Register Offset : 0x0070
// Description     :
---------------------------------------------------------------------------*/

#define BIT_AP_CLK_CGM_I2C4_DIV(x)                    (((x) & 0x7) << 8)
#define BIT_AP_CLK_CGM_I2C4_SEL(x)                    (((x) & 0x3))

/*---------------------------------------------------------------------------
// Register Name   : REG_AP_CLK_I2C5_CFG
// Register Offset : 0x0074
// Description     :
---------------------------------------------------------------------------*/

#define BIT_AP_CLK_CGM_I2C5_DIV(x)                    (((x) & 0x7) << 8)
#define BIT_AP_CLK_CGM_I2C5_SEL(x)                    (((x) & 0x3))

/*---------------------------------------------------------------------------
// Register Name   : REG_AP_CLK_SPI0_CFG
// Register Offset : 0x0078
// Description     :
---------------------------------------------------------------------------*/

#define BIT_AP_CLK_CGM_SPI0_PAD_SEL                   BIT(16)
#define BIT_AP_CLK_CGM_SPI0_DIV(x)                    (((x) & 0x7) << 8)
#define BIT_AP_CLK_CGM_SPI0_SEL(x)                    (((x) & 0x3))

/*---------------------------------------------------------------------------
// Register Name   : REG_AP_CLK_SPI1_CFG
// Register Offset : 0x007C
// Description     :
---------------------------------------------------------------------------*/

#define BIT_AP_CLK_CGM_SPI1_PAD_SEL                   BIT(16)
#define BIT_AP_CLK_CGM_SPI1_DIV(x)                    (((x) & 0x7) << 8)
#define BIT_AP_CLK_CGM_SPI1_SEL(x)                    (((x) & 0x3))

/*---------------------------------------------------------------------------
// Register Name   : REG_AP_CLK_SPI2_CFG
// Register Offset : 0x0080
// Description     :
---------------------------------------------------------------------------*/

#define BIT_AP_CLK_CGM_SPI2_PAD_SEL                   BIT(16)
#define BIT_AP_CLK_CGM_SPI2_DIV(x)                    (((x) & 0x7) << 8)
#define BIT_AP_CLK_CGM_SPI2_SEL(x)                    (((x) & 0x3))

/*---------------------------------------------------------------------------
// Register Name   : REG_AP_CLK_IIS0_CFG
// Register Offset : 0x0084
// Description     :
---------------------------------------------------------------------------*/

#define BIT_AP_CLK_CGM_IIS0_PAD_SEL                   BIT(16)
#define BIT_AP_CLK_CGM_IIS0_DIV(x)                    (((x) & 0x3F) << 8)
#define BIT_AP_CLK_CGM_IIS0_SEL(x)                    (((x) & 0x3))

/*---------------------------------------------------------------------------
// Register Name   : REG_AP_CLK_IIS1_CFG
// Register Offset : 0x0088
// Description     :
---------------------------------------------------------------------------*/

#define BIT_AP_CLK_CGM_IIS1_PAD_SEL                   BIT(16)
#define BIT_AP_CLK_CGM_IIS1_DIV(x)                    (((x) & 0x3F) << 8)
#define BIT_AP_CLK_CGM_IIS1_SEL(x)                    (((x) & 0x3))

/*---------------------------------------------------------------------------
// Register Name   : REG_AP_CLK_IIS2_CFG
// Register Offset : 0x008C
// Description     :
---------------------------------------------------------------------------*/

#define BIT_AP_CLK_CGM_IIS2_PAD_SEL                   BIT(16)
#define BIT_AP_CLK_CGM_IIS2_DIV(x)                    (((x) & 0x3F) << 8)
#define BIT_AP_CLK_CGM_IIS2_SEL(x)                    (((x) & 0x3))

/*---------------------------------------------------------------------------
// Register Name   : REG_AP_CLK_IIS3_CFG
// Register Offset : 0x0090
// Description     :
---------------------------------------------------------------------------*/

#define BIT_AP_CLK_CGM_IIS3_PAD_SEL                   BIT(16)
#define BIT_AP_CLK_CGM_IIS3_DIV(x)                    (((x) & 0x3F) << 8)
#define BIT_AP_CLK_CGM_IIS3_SEL(x)                    (((x) & 0x3))

/*---------------------------------------------------------------------------
// Register Name   : REG_AP_CLK_BIST_192M_CFG
// Register Offset : 0x0094
// Description     :
---------------------------------------------------------------------------*/

#define BIT_AP_CLK_CGM_BIST_192M_SEL                  BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AP_CLK_BIST_256M_CFG
// Register Offset : 0x0098
// Description     :
---------------------------------------------------------------------------*/

#define BIT_AP_CLK_CGM_BIST_256M_SEL                  BIT(0)

#endif
