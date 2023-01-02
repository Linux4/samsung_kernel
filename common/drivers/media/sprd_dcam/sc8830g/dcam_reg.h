/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef _REG_DCAM_TIGER_H_
#define _REG_DCAM_TIGER_H_

#include <mach/globalregs.h>

#define BIT_0                                          0x01
#define BIT_1                                          0x02
#define BIT_2                                          0x04
#define BIT_3                                          0x08
#define BIT_4                                          0x10
#define BIT_5                                          0x20
#define BIT_6                                          0x40
#define BIT_7                                          0x80
#define BIT_8                                          0x0100
#define BIT_9                                          0x0200
#define BIT_10                                         0x0400
#define BIT_11                                         0x0800
#define BIT_12                                         0x1000
#define BIT_13                                         0x2000
#define BIT_14                                         0x4000
#define BIT_15                                         0x8000
#define BIT_16                                         0x010000
#define BIT_17                                         0x020000
#define BIT_18                                         0x040000
#define BIT_19                                         0x080000
#define BIT_20                                         0x100000
#define BIT_21                                         0x200000
#define BIT_22                                         0x400000
#define BIT_23                                         0x800000
#define BIT_24                                         0x01000000
#define BIT_25                                         0x02000000
#define BIT_26                                         0x04000000
#define BIT_27                                         0x08000000
#define BIT_28                                         0x10000000
#define BIT_29                                         0x20000000
#define BIT_30                                         0x40000000
#define BIT_31                                         0x80000000

#define GLOBAL_BASE                                    SPRD_GREG_BASE
#define ARM_GLB_GEN0                                   (GLOBAL_BASE + 0x008UL)
#define ARM_GLB_GEN3                                   (GLOBAL_BASE + 0x01CUL)
#define ARM_GLB_PLL_SCR                                (GLOBAL_BASE + 0x070UL)
//#define GR_CLK_GEN5                                    (GLOBAL_BASE + 0x07CUL)
#define CLK_DLY_CTRL                                   (GLOBAL_BASE + 0x05CUL)

#define DCAM_AHB_BASE                                  SPRD_MMAHB_BASE
#define DCAM_AHB_CTL0                                  (DCAM_AHB_BASE + 0x0000UL)
#define DCAM_EB                                        (DCAM_AHB_CTL0 + 0x0000UL)
#define CSI2_DPHY_EB                                   (DCAM_AHB_CTL0 + 0x0000UL)
#define DCAM_RST                                       (DCAM_AHB_CTL0 + 0x0004UL)
#define DCAM_MATRIX_EB                                 (DCAM_AHB_CTL0 + 0x0008UL)
#define DCAM_CCIR_PCLK_EB                              (SPRD_MMCKG_BASE + 0x0028UL)


#define DCAM_CFG                                       (DCAM_BASE + 0x0000UL)
#define DCAM_CONTROL                                   (DCAM_BASE + 0x0004UL)
#define DCAM_PATH0_CFG                                 (DCAM_BASE + 0x0008UL)
#define DCAM_PATH0_SRC_SIZE                            (DCAM_BASE + 0x000CUL)
#define DCAM_PATH1_CFG                                 (DCAM_BASE + 0x0010UL)
#define DCAM_PATH1_SRC_SIZE                            (DCAM_BASE + 0x0014UL)
#define DCAM_PATH1_DST_SIZE                            (DCAM_BASE + 0x0018UL)
#define DCAM_PATH1_TRIM_START                          (DCAM_BASE + 0x001CUL)
#define DCAM_PATH1_TRIM_SIZE                           (DCAM_BASE + 0x0020UL)
#define DCAM_PATH2_CFG                                 (DCAM_BASE + 0x0024UL)
#define DCAM_PATH2_SRC_SIZE                            (DCAM_BASE + 0x0028UL)
#define DCAM_PATH2_DST_SIZE                            (DCAM_BASE + 0x002CUL)
#define DCAM_PATH2_TRIM_START                          (DCAM_BASE + 0x0030UL)
#define DCAM_PATH2_TRIM_SIZE                           (DCAM_BASE + 0x0034UL)
#define REV_SLICE_VER                                  (DCAM_BASE + 0x0038UL)
#define DCAM_INT_STS                                   (DCAM_BASE + 0x003CUL)
#define DCAM_INT_MASK                                  (DCAM_BASE + 0x0040UL)
#define DCAM_INT_CLR                                   (DCAM_BASE + 0x0044UL)
#define DCAM_INT_RAW                                   (DCAM_BASE + 0x0048UL)
#define DCAM_FRM_ADDR0                                 (DCAM_BASE + 0x004CUL)
#define DCAM_FRM_ADDR1                                 (DCAM_BASE + 0x0050UL)
#define DCAM_FRM_ADDR2                                 (DCAM_BASE + 0x0054UL)
#define DCAM_FRM_ADDR3                                 (DCAM_BASE + 0x0058UL)
#define DCAM_FRM_ADDR4                                 (DCAM_BASE + 0x005CUL)
#define DCAM_FRM_ADDR5                                 (DCAM_BASE + 0x0060UL)
#define DCAM_FRM_ADDR6                                 (DCAM_BASE + 0x0064UL)
#define DCAM_BURST_GAP                                 (DCAM_BASE + 0x0068UL)
#define DCAM_ENDIAN_SEL                                (DCAM_BASE + 0x006CUL)
#define DCAM_AHBM_STS                                  (DCAM_BASE + 0x0070UL)
#define DCAM_FRM_ADDR7                                 (DCAM_BASE + 0x0074UL)
#define DCAM_FRM_ADDR8                                 (DCAM_BASE + 0x0078UL)
#define DCAM_FRM_ADDR9                                 (DCAM_BASE + 0x007CUL)
#define DCAM_FRM_ADDR10                                (DCAM_BASE + 0x0080UL)
#define DCAM_FRM_ADDR11                                (DCAM_BASE + 0x0084UL)
#define CCIR_PATT_CFG                                  (DCAM_BASE + 0x0088UL)
#define CCIR_PATT_SIZE                                 (DCAM_BASE + 0x008CUL)
#define CCIR_PATT_VBLANK                               (DCAM_BASE + 0x0090UL)
#define CCIR_PATT_HBLANK                               (DCAM_BASE + 0x0094UL)
#define REV_BURST_IN_CFG                               (DCAM_BASE + 0x0098UL)
#define REV_BURST_IN_TRIM_START                        (DCAM_BASE + 0x009CUL)
#define REV_BURST_IN_TRIM_SIZE                         (DCAM_BASE + 0x00A0UL)
#define REV_SLICE_CFG                                  (DCAM_BASE + 0x00A4UL)
#define PATH1_SLICE_O_VCNT                             (DCAM_BASE + 0x00A8UL)
#define PATH2_SLICE_O_VCNT                             (DCAM_BASE + 0x00ACUL)

#define CAP_CCIR_CTRL                                  (DCAM_BASE + 0x0100UL)
#define CAP_CCIR_FRM_CTRL                              (DCAM_BASE + 0x0104UL)
#define CAP_CCIR_START                                 (DCAM_BASE + 0x0108UL)
#define CAP_CCIR_END                                   (DCAM_BASE + 0x010CUL)
#define CAP_CCIR_IMG_DECI                              (DCAM_BASE + 0x0110UL)
#define CAP_CCIR_ATV_FIX                               (DCAM_BASE + 0x0114UL)
#define CAP_CCIR_OBSERVE                               (DCAM_BASE + 0x0118UL)
#define CAP_CCIR_JPG_CTRL                              (DCAM_BASE + 0x011CUL)
#define CAP_CCIR_FRM_SIZE                              (DCAM_BASE + 0x0120UL)
#define CAP_CCIR_PLCK_SRC                              (SPRD_PIN_BASE + 0x04)
#define CAP_SPI_CFG                                    (DCAM_BASE + 0x0124UL)

#define CAP_MIPI_CTRL                                  (DCAM_BASE + 0x0128UL)
#define CAP_MIPI_FRM_CTRL                              (DCAM_BASE + 0x012CUL)
#define CAP_MIPI_START                                 (DCAM_BASE + 0x0130UL)
#define CAP_MIPI_END                                   (DCAM_BASE + 0x0134UL)
#define CAP_MIPI_IMG_DECI                              (DCAM_BASE + 0x0138UL)
#define CAP_MIPI_JPG_CTRL                              (DCAM_BASE + 0x013CUL)
#define CAP_MIPI_FRM_SIZE                              (DCAM_BASE + 0x0140UL)
#define CAP_SENSOR_CTRL                                (DCAM_BASE + 0x0144UL)

#define BLC_CFG                                        (DCAM_BASE + 0x0148UL)
#define BLC_START                                      (DCAM_BASE + 0x014CUL)
#define BLC_END                                        (DCAM_BASE + 0x0150UL)
#define BLC_TARGET                                     (DCAM_BASE + 0x0154UL)
#define BLC_MANUAL                                     (DCAM_BASE + 0x0158UL)
#define BLC_VALUE1                                     (DCAM_BASE + 0x015CUL)
#define BLC_VALUE2                                     (DCAM_BASE + 0x0160UL)

#define DCAM_END                                       BLC_VALUE2


#define DCAM_EB_BIT                                    BIT_0
#define CCIR_EB_BIT                                    BIT_1
#define MIPI_EB_BIT                                    BIT_10 // aiden: todo

#define DCAM_MOD_RST_BIT                               BIT_0
#define CCIR_RST_BIT                                   BIT_1
#define PATH0_RST_BIT                                  BIT_7
#define PATH1_RST_BIT                                  BIT_8
#define PATH2_RST_BIT                                  BIT_9
#define CCIR_PCLK_EB_BIT                               BIT_16

#define DCAM_PATH_NUM                                  3
#define DCAM_CAP_SKIP_FRM_MAX                          16
#define DCAM_FRM_DECI_FAC_MAX                          4
#define DCAM_CAP_FRAME_WIDTH_MAX                       4092
#define DCAM_CAP_FRAME_HEIGHT_MAX                      4092
#define DCAM_PATH_FRAME_WIDTH_MAX                      4092
#define DCAM_PATH_FRAME_HEIGHT_MAX                     4092
#define DCAM_CAP_X_DECI_FAC_MAX                        4 // cap deci: 0 - 1/8
#define DCAM_CAP_Y_DECI_FAC_MAX                        4 // cap deci: 0 - 1/8
#define DCAM_JPG_BUF_UNIT                              (1 << 15)
#define DCAM_JPG_UNITS                                 (1 << 10)
#define DCAM_SC_COEFF_UP_MAX                           4 // path scaling: 1/8 - 4
#define DCAM_SC_COEFF_DOWN_MAX                         7 // path scaling: 1/8 - 4
#define DCAM_PATH_DECI_FAC_MAX                         4 // path deci: 1/2 - 1/16
#define DCAM_PATH1_LINE_BUF_LENGTH                     2048
#define DCAM_PATH2_LINE_BUF_LENGTH                     4096
#define DCAM_ISP_LINE_BUF_LENGTH                       3280
#define DCAM_IRQ                                       IRQ_DCAM_INT

#define DCAM_PIXEL_ALIGN_WIDTH                         4
#define DCAM_PIXEL_ALIGN_HEIGHT                        2

#define DCAM_PATH_NUM                                  3
enum {
	DCAM_PATH0 = 0,
	DCAM_PATH1,
	DCAM_PATH2,
	DCAM_PATH_MAX
};

#ifdef   __cplusplus
}
#endif

#endif //_REG_DCAM_TIGER_H_


