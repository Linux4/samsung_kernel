/*
 * This file is part of the Dyna-Image CS5032 sensor driver for MTK platform.
 * CS5032 is combined proximity, ambient light sensor and IRLED.
 *
 * Contact: John Huang <john.huang@dyna-image.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 *
 * Filename: CS5032.h
 *
 * Summary:
 *	CS5032 sensor dirver header file.
 *
 * Modification History:
 * Date     By				Summary
 * -------- --------       --------------------------------------
 * 15/09/14 Templeton Tsai		 Original Creation
 *
 */

/*
 * Definitions for CS5032 als/ps sensor chip.
 */
#ifndef __CS5032_H__
#define __CS5032_H__

#include <linux/ioctl.h>



#define CS5032_SUCCESS					0
#define CS5032_ERR_I2C					-1
#define CS5032_ERR_STATUS				-3
#define CS5032_ERR_SETUP_FAILURE			-4
#define CS5032_ERR_GETGSENSORDATA			-5
#define CS5032_ERR_IDENTIFICATION			-6


#define CS5032_NUM_CACHABLE_REGS	58
#define CS5032_ALS_GAIN_1   0x0
#define CS5032_ALS_GAIN_4   0x1
#define CS5032_ALS_GAIN_16  0x2
#define CS5032_ALS_GAIN_64  0x3


#define CS5032_SYS_MGS_ENABLE 0x8
#define CS5032_SYS_RST_ENABLE 0x4
#define CS5032_SYS_PS_ENABLE 0x2
#define CS5032_SYS_ALS_ENABLE 0x1
#define CS5032_SYS_DEV_DOWN 0x0



/* CS5032 control Register */
/*============================================================================*/
#define CS5032_REG_SYS_CON        0x00
#define CS5032_REG_SYS_CON_MASK	0x0F
#define CS5032_REG_SYS_CON_SHIFT	(0)
/* CS5032 interrupt flag */
/*============================================================================*/
#define CS5032_REG_SYS_INTSTATUS   0x01
#define CS5032_REG_SYS_INT_POR_MASK	0x80
#define CS5032_REG_SYS_INT_POR_SHIFT	(7)
#define CS5032_REG_SYS_INT_MGS_MASK	0x40
#define CS5032_REG_SYS_INT_MGS_SHIFT	(6)
#define CS5032_REG_SYS_INT_ERRF_MASK	0x20//read only
#define CS5032_REG_SYS_INT_AL_MASK	0x01
#define CS5032_REG_SYS_INT_AL_SHIFT	(0)


/* CS5032 interrupt control */
/*============================================================================*/
#define CS5032_REG_SYS_INTCTRL     0x02
#define CS5032_REG_SYS_INTCTRL_PIEN_MASK     0x01
#define CS5032_REG_SYS_INTCTRL_PSPEND_MASK   0x02
#define CS5032_REG_SYS_INTCTRL_PSPEND_SHIFT   (5)
#define CS5032_REG_SYS_INTCTRL_PSMODE_MASK   0x03
#define CS5032_REG_SYS_INTCTRL_PSACC_MASK    0x04
#define CS5032_REG_SYS_INTCTRL_AIEN_MASK     0x05
#define CS5032_REG_SYS_INTCTRL_ALPEND_MASK   0x06
#define CS5032_REG_SYS_INTCTRL_MGSIEN_MASK   0x07

/* CS5032 waiting time Register */
#define CS5032_REG_WAITING_TIME     0x06
#define CS5032_REG_WAITING_TIME_WTIME_MASK 0x40
#define CS5032_REG_WAITING_TIME_WTIME_SHIFT (0)
#define CS5032_REG_WAITING_TIME_WUNIT_MASK 0x7F

/* CS5032 ALS control Register */
#define CS5032_REG_ALS_CON    0x07
#define CS5032_REG_ALS_ALSRC_CON_MASK     0x30
#define CS5032_REG_ALS_ALSRC_CON_SHIFT    (4)
#define CS5032_REG_ALS_ALGAIN_CON_MASK    0x03
#define CS5032_REG_ALS_ALGAIN_CON_SHIFT   (0)

/* CS5032 ALS persistence Register */
#define CS5032_REG_ALS_PERS    0x08
#define CS5032_REG_ALS_PERS_MASK    0x3F


/* CS5032 ALS time Register */
#define CS5032_REG_ALS_TIME    0x0A
#define CS5032_REG_ALS_TIME_MASK    0xFF
#define CS5032_REG_ALS_TIME_SHIFT    (0)


/* CS5032 Error Flag Situation Register */
#define CS5032_REG_ERR    0x1E
#define CS5032_REG_ERRC_ERR_MASK      0x08
#define CS5032_REG_ERRB_ERR_MASK      0x04
#define CS5032_REG_ERRG_ERR_MASK      0x02
#define CS5032_REG_ERRR_ERR_MASK      0x01


/* CS5032 Red Data Low Register */
#define CS5032_REG_RED_DATA_LOW    0x28
#define CS5032_REG_RED_DATA_LOW_MASK    0xFF


/* CS5032 Red Data High Register */
#define CS5032_REG_RED_DATA_HIGH    0x29
#define CS5032_REG_RED_DATA_HIGH_MASK    0xFF

/* CS5032 Green Data Low Register */
#define CS5032_REG_GREEN_DATA_LOW    0x2A
#define CS5032_REG_GREEN_DATA_LOW_MASK    0xFF


/* CS5032 Green Data High Register */
#define CS5032_REG_GREEN_DATA_HIGH    0x2B
#define CS5032_REG_GREEN_DATA_HIGH_MASK    0xFF

/* CS5032 Blue Data Low Register */
#define CS5032_REG_BLUE_DATA_LOW    0x2C
#define CS5032_REG_BLUE_DATA_LOW_MASK    0xFF

/* CS5032 Blue Data High Register */
#define CS5032_REG_BLUE_DATA_HIGH    0x2D
#define CS5032_REG_BLUE_DATA_HIGH_MASK    0xFF

/* CS5032 COMP Data Low Register */
#define CS5032_REG_COMP_DATA_LOW    0x2E
#define CS5032_REG_COMP_DATA_LOW_MASK    0xFF

/* CS5032 COMP Data High Register */
#define CS5032_REG_COMP_DATA_HIGH    0x2F
#define CS5032_REG_COMP_DATA_HIGH_MASK    0xFF


/* CS5032 L Data Low Register */
#define CS5032_REG_L_DATA_LOW    0x30
#define CS5032_REG_L_DATA_LOW_MASK    0xFF

/* CS5032 L Data High Register */
#define CS5032_REG_L_DATA_HIGH    0x31
#define CS5032_REG_L_DATA_HIGH_MASK    0xFF


/* CS5032 AL Low Threshold Low Register */
#define CS5032_REG_AL_ALTHL_LOW    0x32
#define CS5032_REG_AL_ALTHL_LOW_MASK    0xFF
#define CS5032_REG_AL_ALTHL_LOW_SHIFT    (0)

/* CS5032 AL Low Threshold High Register */
#define CS5032_REG_AL_ALTHL_HIGH    0x33
#define CS5032_REG_AL_ALTHL_HIGH_MASK    0xFF
#define CS5032_REG_AL_ALTHL_HIGH_SHIFT    (0)

/* CS5032 AL High Threshold Low Register */
#define CS5032_REG_AL_ALTHH_LOW    0x34
#define CS5032_REG_AL_ALTHH_LOW_MASK    0xFF
#define CS5032_REG_AL_ALTHH_LOW_SHIFT    (0)

/* CS5032 AL High Threshold High Register */
#define CS5032_REG_AL_ALTHH_HIGH    0x35
#define CS5032_REG_AL_ALTHH_HIGH_MASK    0xFF
#define CS5032_REG_AL_ALTHH_HIGH_SHIFT   (0)


/* CS5032 LuminanceCoef R Register */
#define CS5032_REG_LUM_COEFR    0x3C
#define CS5032_REG_LUM_COEFR_MASK    0xFF
#define CS5032_REG_LUM_COEFR_SHIFT    (0)

/* CS5032 LuminanceCoef G Register */
#define CS5032_REG_LUM_COEFG    0x3D
#define CS5032_REG_LUM_COEFG_MASK    0xFF
#define CS5032_REG_LUM_COEFG_SHIFT    (0)

/* CS5032 LuminanceCoef B Register */
#define CS5032_REG_LUM_COEFB    0x3E
#define CS5032_REG_LUM_COEFB_MASK    0xFF
#define CS5032_REG_LUM_COEFB_SHIFT    (0)


#endif

