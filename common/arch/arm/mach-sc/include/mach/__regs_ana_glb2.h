/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
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

#ifndef __ANA_REGS_GLB2_H__
#define __ANA_REGS_GLB2_H__

#ifndef __SCI_GLB_REGS_H__
#error  "Don't include this file directly, include <mach/sci_glb_regs.h>"
#endif

#define ANA_REGS_GLB2

/* registers definitions for controller ANA_REGS_GLB2 */
#define ANA_REG_GLB2_LDO_TRIM0          SCI_ADDR(ANA_REGS_GLB2_BASE, 0x0000)
#define ANA_REG_GLB2_LDO_TRIM1          SCI_ADDR(ANA_REGS_GLB2_BASE, 0x0004)
#define ANA_REG_GLB2_LDO_TRIM2          SCI_ADDR(ANA_REGS_GLB2_BASE, 0x0008)
#define ANA_REG_GLB2_LDO_TRIM3          SCI_ADDR(ANA_REGS_GLB2_BASE, 0x000c)
#define ANA_REG_GLB2_LDO_TRIM4          SCI_ADDR(ANA_REGS_GLB2_BASE, 0x0010)
#define ANA_REG_GLB2_LDO_TRIM5          SCI_ADDR(ANA_REGS_GLB2_BASE, 0x0014)
#define ANA_REG_GLB2_LDO_TRIM6          SCI_ADDR(ANA_REGS_GLB2_BASE, 0x0018)
#define ANA_REG_GLB2_LDO_TRIM7          SCI_ADDR(ANA_REGS_GLB2_BASE, 0x001c)
#define ANA_REG_GLB2_LDO_TRIM8          SCI_ADDR(ANA_REGS_GLB2_BASE, 0x0020)
#define ANA_REG_GLB2_LDO_TRIM_SEL       SCI_ADDR(ANA_REGS_GLB2_BASE, 0x0024)

/* bits definitions for register ANA_REG_GLB2_LDO_TRIM0 */
#define BITS_TRIM_VDD18(_x_)            ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)) )
#define BITS_TRIM_VDD28(_x_)            ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )

/* bits definitions for register ANA_REG_GLB2_LDO_TRIM1 */
#define BITS_TRIM_VDD25(_x_)            ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)) )
#define BITS_TRIM_VDD3V(_x_)            ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )

/* bits definitions for register ANA_REG_GLB2_LDO_TRIM2 */
#define BITS_TRIM_DVDD18(_x_)           ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)) )
#define BITS_TRIM_SD0(_x_)              ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )

/* bits definitions for register ANA_REG_GLB2_LDO_TRIM3 */
#define BITS_TRIM_SD1(_x_)              ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)) )
#define BITS_TRIM_SD3(_x_)              ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )

/* bits definitions for register ANA_REG_GLB2_LDO_TRIM4 */
#define BITS_TRIM_VSIM(_x_)             ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)) )
#define BITS_TRIM_USB(_x_)              ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )

/* bits definitions for register ANA_REG_GLB2_LDO_TRIM5 */
#define BITS_TRIM_RF(_x_)               ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)) )
#define BITS_TRIM_ABB(_x_)              ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )

/* bits definitions for register ANA_REG_GLB2_LDO_TRIM6 */
#define BITS_TRIM_CAMA(_x_)             ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)) )
#define BITS_TRIM_CAMCORE(_x_)          ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )

/* bits definitions for register ANA_REG_GLB2_LDO_TRIM7 */
#define BITS_TRIM_CAMIO(_x_)            ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)) )
#define BITS_TRIM_CAMMOT(_x_)           ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )

/* bits definitions for register ANA_REG_GLB2_LDO_TRIM8 */
#define BITS_TRIM_CMMB1V2(_x_)          ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)) )
#define BITS_TRIM_CMMB1V8(_x_)          ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )

/* bits definitions for register ANA_REG_GLB2_LDO_TRIM_SEL */
#define BITS_LDO_RF_CAL_EN(_x_)         ( (_x_) & (BIT(0)) )
#define BITS_LDO_ABB_CAL_EN(_x_)        ( (_x_) & (BIT(1)) )
#define BITS_LDO_CAMA_CAL_EN(_x_)       ( (_x_) & (BIT(0)|BIT(1)) )
#define BITS_LDO_VDD3V_CAL_EN(_x_)      ( (_x_) & (BIT(5)) )
#define BITS_LDO_VDD28_CAL_EN(_x_)      ( (_x_) & (BIT(2)|BIT(5)) )
#define BITS_LDO_VSIM_CAL_EN(_x_)       ( (_x_) & (BIT(3)|BIT(5)) )
#define BITS_LDO_CAMMOT_CAL_EN(_x_)     ( (_x_) & (BIT(2)|BIT(3)|BIT(5)) )
#define BITS_LDO_SD0_CAL_EN(_x_)        ( (_x_) & (BIT(4)|BIT(5)) )
#define BITS_LDO_USB_CAL_EN(_x_)        ( (_x_) & (BIT(2)|BIT(4)|BIT(5)) )
#define BITS_LDO_DVDD18_CAL_EN(_x_)     ( (_x_) & (BIT(3)|BIT(4)|BIT(5)) )
#define BITS_LDO_VDD25_CAL_EN(_x_)      ( (_x_) & (BIT(2)|BIT(3)|BIT(4)|BIT(5)) )
#define BITS_LDO_CAMIO_CAL_EN(_x_)      ( (_x_) & (BIT(6)) )
#define BITS_LDO_CAMCORE_CAL_EN(_x_)    ( (_x_) & (BIT(7)) )
#define BITS_LDO_CMMB1P2_CAL_EN(_x_)    ( (_x_) & (BIT(6)|BIT(7)) )
#define BITS_LDO_CMMB1P8_CAL_EN(_x_)    ( (_x_) & (BIT(8)) )
#define BITS_LDO_VDD18_CAL_EN(_x_)      ( (_x_) & (BIT(6)|BIT(8)) )
#define BITS_LDO_SD1_CAL_EN(_x_)        ( (_x_) & (BIT(7)|BIT(8)) )
#define BITS_LDO_SD3_CAL_EN(_x_)        ( (_x_) & (BIT(6)|BIT(7)|BIT(8)) )

/* vars definitions for controller ANA_REGS_GLB2 */

#endif //__ANA_REGS_GLB2_H__
