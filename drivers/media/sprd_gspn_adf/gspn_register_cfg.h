
/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
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

/*
define all gspn relative register, like lock enable/module reset/lock select/
gspn module control regiter structure/trust zone ops/command queue regiter/
regiter opertion macro define
*/

#ifndef _GSPN_REGISTER_CFG_H_
#define _GSPN_REGISTER_CFG_H_

#define REG_GET(reg)                (*(volatile uint32_t*)(reg))
#define REG_SET(reg,value)          (*(volatile uint32_t*)(reg) = (value))
#define REG_OR(reg,value)           (*(volatile uint32_t*)(reg) |= (value))
#define REG_CLR(reg,value)			(*(volatile uint32_t*)(reg) &= (~(value)))


//#define CONFIG_MINI_CODE
#if 0//def CONFIG_MINI_CODE
/*POWER_EN*/
#define GSPN_POWER_EN_BASE          (0x402B0040) // TODO: whale chip

#define AHB_EN_BASE                 (0x63100000) // TODO: whale chip
#define GSPN_EN_BASE                (AHB_EN_BASE)

#define AHB_RESET_BASE              (0x23100000) // TODO: whale chip
#define GSPN_RESET_BASE             (AHB_RESET_BASE + 0x04)
#define GSPN_EN_RST_BIT(id)         BIT((3+(id)))
#define IOMMU_EN_RST_BIT(id)        BIT((5+(id)))

#define AP_INT_BASE                 (0x40370000) // TODO: whale chip
#define GSPN_INT_STATUS             (AP_INT_BASE + 0x00)
#define GSPN_INT_RAW                (AP_INT_BASE + 0x04)
#define GSPN_INT_ENABLE             (AP_INT_BASE + 0x08)
#define GSPN_INT_CLEAR              (AP_INT_BASE + 0x0C)
#define GSPN_INT_BIT(id)            BIT((15-(id)))

#define IOMMU_CTL_BASE(id)          ((id)?0x63800000:0x63700000) // TODO: whale chip

#define GSPN_REG_BASE           (0x63500000)// TODO: whale chip
#define GSP_GLB_CFG             (GSPN_REG_BASE + 0x000)
#define GSP_MOD1_CFG            (GSPN_REG_BASE + 0x004)
#define GSP_MOD2_CFG            (GSPN_REG_BASE + 0x008)
#define GSP_CMD_STS0            (GSPN_REG_BASE + 0x00C)
#define GSP_CMD_STS1            (GSPN_REG_BASE + 0x010)
#define GSP_CMD_ADDR            (GSPN_REG_BASE + 0x014)

/*Destination reg 1*/
#define DES_DATA_CFG1           (GSPN_REG_BASE + 0x018)
#define DES_Y_ADDR1             (GSPN_REG_BASE + 0x01C)
#define DES_U_ADDR1             (GSPN_REG_BASE + 0x020)
#define DES_V_ADDR1             (GSPN_REG_BASE + 0x024)
#define DES_PITCH1              (GSPN_REG_BASE + 0x028)
#define BACK_RGB                (GSPN_REG_BASE + 0x02C)
#define DES_SCL_SIZE            (GSPN_REG_BASE + 0x030)
#define WORK_DES_START          (GSPN_REG_BASE + 0x034)
#define WORK_SCR_SIZE           (GSPN_REG_BASE + 0x038)
#define WORK_SCR_START          (GSPN_REG_BASE + 0x03C)

/*Destination reg 2*/
#define DES_DATA_CFG2           (GSPN_REG_BASE + 0x040)
#define DES_Y_ADDR2             (GSPN_REG_BASE + 0x044)
#define DES_U_ADDR2             (GSPN_REG_BASE + 0x048)
#define DES_V_ADDR2             (GSPN_REG_BASE + 0x04C)
#define DES_PITCH2              (GSPN_REG_BASE + 0x050)
#define AXI_DEBUG               (GSPN_REG_BASE + 0x054)
#define GSP_INT                 (GSPN_REG_BASE + 0x058)

/*LAYER0*/
#define LAYER0_CFG              (GSPN_REG_BASE + 0x05C)
#define LAYER0_Y_ADDR           (GSPN_REG_BASE + 0x060)
#define LAYER0_U_ADDR           (GSPN_REG_BASE + 0x064)
#define LAYER0_V_ADDR           (GSPN_REG_BASE + 0x068)
#define LAYER0_PITCH            (GSPN_REG_BASE + 0x06C)
#define LAYER0_CLIP_START       (GSPN_REG_BASE + 0x070)
#define LAYER0_CLIP_SIZE        (GSPN_REG_BASE + 0x074)
#define LAYER0_DES_START        (GSPN_REG_BASE + 0x078)
#define LAYER0_PALLET_RGB       (GSPN_REG_BASE + 0x07C)
#define LAYER0_CK               (GSPN_REG_BASE + 0x080)
#define Y2R_Y_PARAM             (GSPN_REG_BASE + 0x084)
#define Y2R_U_PARAM             (GSPN_REG_BASE + 0x088)
#define Y2R_V_PARAM             (GSPN_REG_BASE + 0x08C)

/*LAYER1*/
#define LAYER1_CFG              (GSPN_REG_BASE + 0x090)
#define LAYER1_R_ADDR           (GSPN_REG_BASE + 0x094)
#define LAYER1_PITCH            (GSPN_REG_BASE + 0x098)
#define LAYER1_CLIP_START       (GSPN_REG_BASE + 0x09C)
#define LAYER1_CLIP_SIZE        (GSPN_REG_BASE + 0x0A0)
#define LAYER1_DES_START        (GSPN_REG_BASE + 0x0A4)
#define LAYER1_PALLET_RGB       (GSPN_REG_BASE + 0x0A8)
#define LAYER1_CK               (GSPN_REG_BASE + 0x0AC)

/*LAYER2*/
#define LAYER2_CFG              (GSPN_REG_BASE + 0x0B0)
#define LAYER2_R_ADDR           (GSPN_REG_BASE + 0x0B4)
#define LAYER2_PITCH            (GSPN_REG_BASE + 0x0B8)
#define LAYER2_CLIP_START       (GSPN_REG_BASE + 0x0BC)
#define LAYER2_CLIP_SIZE        (GSPN_REG_BASE + 0x0C0)
#define LAYER2_DES_START        (GSPN_REG_BASE + 0x0C4)
#define LAYER2_PALLET_RGB       (GSPN_REG_BASE + 0x0C8)
#define LAYER2_CK               (GSPN_REG_BASE + 0x0CC)

/*LAYER3*/
#define LAYER3_CFG              (GSPN_REG_BASE + 0x0D0)
#define LAYER3_R_ADDR           (GSPN_REG_BASE + 0x0D4)
#define LAYER3_PITCH            (GSPN_REG_BASE + 0x0D8)
#define LAYER3_CLIP_START       (GSPN_REG_BASE + 0x0DC)
#define LAYER3_CLIP_SIZE        (GSPN_REG_BASE + 0x0E0)
#define LAYER3_DES_START        (GSPN_REG_BASE + 0x0E4)
#define LAYER3_PALLET_RGB       (GSPN_REG_BASE + 0x0E8)
#define LAYER3_CK               (GSPN_REG_BASE + 0x0EC)

/*HORIZONTAL COEF TABLE (72*8)*/
#define HOR_COEF_1_12           (GSPN_REG_BASE + 0x100)
#define HOR_COEF_1_34           (GSPN_REG_BASE + 0x104)
#define HOR_COEF_1_56           (GSPN_REG_BASE + 0x108)
#define HOR_COEF_1_78           (GSPN_REG_BASE + 0x10C)
#define HOR_COEF_2_12           (GSPN_REG_BASE + 0x110)
#define HOR_COEF_2_34           (GSPN_REG_BASE + 0x114)
#define HOR_COEF_2_56           (GSPN_REG_BASE + 0x118)
#define HOR_COEF_2_78           (GSPN_REG_BASE + 0x11C)
#define HOR_COEF_3_12           (GSPN_REG_BASE + 0x120)
#define HOR_COEF_3_34           (GSPN_REG_BASE + 0x124)
#define HOR_COEF_3_56           (GSPN_REG_BASE + 0x128)
#define HOR_COEF_3_78           (GSPN_REG_BASE + 0x12C)
#define HOR_COEF_4_12           (GSPN_REG_BASE + 0x130)
#define HOR_COEF_4_34           (GSPN_REG_BASE + 0x134)
#define HOR_COEF_4_56           (GSPN_REG_BASE + 0x138)
#define HOR_COEF_4_78           (GSPN_REG_BASE + 0x13C)
#define HOR_COEF_5_12           (GSPN_REG_BASE + 0x140)
#define HOR_COEF_5_34           (GSPN_REG_BASE + 0x144)
#define HOR_COEF_5_56           (GSPN_REG_BASE + 0x148)
#define HOR_COEF_5_78           (GSPN_REG_BASE + 0x14C)
#define HOR_COEF_6_12           (GSPN_REG_BASE + 0x150)
#define HOR_COEF_6_34           (GSPN_REG_BASE + 0x154)
#define HOR_COEF_6_56           (GSPN_REG_BASE + 0x158)
#define HOR_COEF_6_78           (GSPN_REG_BASE + 0x15C)
#define HOR_COEF_7_12           (GSPN_REG_BASE + 0x160)
#define HOR_COEF_7_34           (GSPN_REG_BASE + 0x164)
#define HOR_COEF_7_56           (GSPN_REG_BASE + 0x168)
#define HOR_COEF_7_78           (GSPN_REG_BASE + 0x16C)
#define HOR_COEF_8_12           (GSPN_REG_BASE + 0x170)
#define HOR_COEF_8_34           (GSPN_REG_BASE + 0x174)
#define HOR_COEF_8_56           (GSPN_REG_BASE + 0x178)
#define HOR_COEF_8_78           (GSPN_REG_BASE + 0x17C)

/*HORIZONTAL COEF TABLE (72*8)*/
#define VER_COEF_1_12           (GSPN_REG_BASE + 0x180)
#define VER_COEF_1_34           (GSPN_REG_BASE + 0x184)
#define VER_COEF_2_12           (GSPN_REG_BASE + 0x188)
#define VER_COEF_2_34           (GSPN_REG_BASE + 0x18C)
#define VER_COEF_3_12           (GSPN_REG_BASE + 0x190)
#define VER_COEF_3_34           (GSPN_REG_BASE + 0x194)
#define VER_COEF_4_12           (GSPN_REG_BASE + 0x198)
#define VER_COEF_4_34           (GSPN_REG_BASE + 0x19C)
#define VER_COEF_5_12           (GSPN_REG_BASE + 0x1A0)
#define VER_COEF_5_34           (GSPN_REG_BASE + 0x1A4)
#define VER_COEF_6_12           (GSPN_REG_BASE + 0x1A8)
#define VER_COEF_6_34           (GSPN_REG_BASE + 0x1AC)
#define VER_COEF_7_12           (GSPN_REG_BASE + 0x1B0)
#define VER_COEF_7_34           (GSPN_REG_BASE + 0x1B4)
#define VER_COEF_8_12           (GSPN_REG_BASE + 0x1B8)
#define VER_COEF_8_34           (GSPN_REG_BASE + 0x1BC)

/*ROT*/
#define ROT_MOD_CFG             (GSPN_REG_BASE + 0x200)
#define ROT_SRC_ADDR            (GSPN_REG_BASE + 0x204)
#define ROT_DES_ADDR            (GSPN_REG_BASE + 0x208)
#define ROT_PITCH               (GSPN_REG_BASE + 0x20C)
#define ROT_CLIP_START          (GSPN_REG_BASE + 0x210)
#define ROT_CLIP_SIZE           (GSPN_REG_BASE + 0x214)

#define GSPN_HOR_COEF_BASE      HOR_COEF_1_12
#define GSPN_VER_COEF_BASE      VER_COEF_1_12

#define GSPN_AUTO_GATE_ENABLE()  // TODO:

#else

#define GSPN_CLK_GATE_ENABLE(core_mng) {\
    int i = 0;\
    while(i < (core_mng)->core_cnt) {\
        REG_OR((core_mng)->cores[i].auto_gate_reg_base,(core_mng)->cores[i].auto_gate_bit);\
        REG_OR((core_mng)->cores[i].auto_gate_reg_base,(core_mng)->cores[i].noc_auto_bit);\
        REG_OR((core_mng)->cores[i].auto_gate_reg_base,(core_mng)->cores[i].noc_force_bit);\
        REG_OR((core_mng)->cores[i].auto_gate_reg_base,(core_mng)->cores[i].mtx_auto_bit);\
        REG_OR((core_mng)->cores[i].auto_gate_reg_base,(core_mng)->cores[i].mtx_force_bit);\
        i++;\
    }\
}
#endif

typedef enum
{
    GSPN_IRQ_TYPE_DISABLE = 0x00,
    GSPN_IRQ_TYPE_ENABLE,
    GSPN_IRQ_TYPE_INVALID,
} GSPN_IRQ_TYPE_E;


typedef struct
{
    volatile uint32_t hor_coef_1_12;
    volatile uint32_t hor_coef_1_34;
    volatile uint32_t hor_coef_1_56;
    volatile uint32_t hor_coef_1_78;
    volatile uint32_t hor_coef_2_12;
    volatile uint32_t hor_coef_2_34;
    volatile uint32_t hor_coef_2_56;
    volatile uint32_t hor_coef_2_78;
    volatile uint32_t hor_coef_3_12;
    volatile uint32_t hor_coef_3_34;
    volatile uint32_t hor_coef_3_56;
    volatile uint32_t hor_coef_3_78;
    volatile uint32_t hor_coef_4_12;
    volatile uint32_t hor_coef_4_34;
    volatile uint32_t hor_coef_4_56;
    volatile uint32_t hor_coef_4_78;
    volatile uint32_t hor_coef_5_12;
    volatile uint32_t hor_coef_5_34;
    volatile uint32_t hor_coef_5_56;
    volatile uint32_t hor_coef_5_78;
    volatile uint32_t hor_coef_6_12;
    volatile uint32_t hor_coef_6_34;
    volatile uint32_t hor_coef_6_56;
    volatile uint32_t hor_coef_6_78;
    volatile uint32_t hor_coef_7_12;
    volatile uint32_t hor_coef_7_34;
    volatile uint32_t hor_coef_7_56;
    volatile uint32_t hor_coef_7_78;
    volatile uint32_t hor_coef_8_12;
    volatile uint32_t hor_coef_8_34;
    volatile uint32_t hor_coef_8_56;
    volatile uint32_t hor_coef_8_78;
    volatile uint32_t vor_coef_1_12;
    volatile uint32_t vor_coef_1_34;
    volatile uint32_t vor_coef_2_12;
    volatile uint32_t vor_coef_2_34;
    volatile uint32_t vor_coef_3_12;
    volatile uint32_t vor_coef_3_34;
    volatile uint32_t vor_coef_4_12;
    volatile uint32_t vor_coef_4_34;
    volatile uint32_t vor_coef_5_12;
    volatile uint32_t vor_coef_5_34;
    volatile uint32_t vor_coef_6_12;
    volatile uint32_t vor_coef_6_34;
    volatile uint32_t vor_coef_7_12;
    volatile uint32_t vor_coef_7_34;
    volatile uint32_t vor_coef_8_12;
    volatile uint32_t vor_coef_8_34;
} GSPN_COEF_REG_T;

typedef struct //_GSP_GLB_CFG_MAP
{
    volatile uint32_t run_mod           :   1;
    volatile uint32_t cmd_en            :   1;
    volatile uint32_t scl_clr           :   1;
    volatile uint32_t cmd_word_endian_mod   :   2;
    volatile uint32_t cmd_dword_endian_mod  :   1;
    volatile uint32_t htap4_en          :   1;
    volatile uint32_t reserved          :   9;
    volatile uint32_t gap_wb            :   8;
    volatile uint32_t gap_rb            :   8;
} gspn_glb_cfg_mBits;

typedef union //_GSP_GLB_CFG_TAG
{
    volatile gspn_glb_cfg_mBits mBits;
    volatile uint32_t dwValue;
} gspn_glb_cfg_u;

typedef struct //_GSP_MOD1_CFG_MAP
{
    volatile uint32_t bld_run       :   1;
    volatile uint32_t bld_busy      :   1;
    volatile uint32_t rch_busy      :   1;
    volatile uint32_t wch_busy      :   1;
    volatile uint32_t reserved1     :   4;
    volatile uint32_t err1_flg      :   1;
    volatile uint32_t err1_code     :   5;
    volatile uint32_t reserved2     :   2;
    volatile uint32_t l0_en         :   1;
    volatile uint32_t l1_en         :   1;
    volatile uint32_t l2_en         :   1;
    volatile uint32_t l3_en         :   1;
    volatile uint32_t pmargb_l0     :   1;
    volatile uint32_t pmargb_l1     :   1;
    volatile uint32_t pmargb_l2     :   1;
    volatile uint32_t pmargb_l3     :   1;
    volatile uint32_t scale_seq     :   1;
    volatile uint32_t dither1_en    :   1;
    volatile uint32_t scale_en      :   1;
    volatile uint32_t pmargb_en     :   1;
    volatile uint32_t bg_en         :   1;
    volatile uint32_t reserved3     :   3;
} gspn_mod1_cfg_mBits;

typedef union //_GSP_MOD1_CFG_TAG
{
    volatile gspn_mod1_cfg_mBits mBits;
    volatile uint32_t dwValue;
} gspn_mod1_cfg_u;

typedef struct //_GSP_MOD2_CFG_MAP
{
    volatile uint32_t scl_run       :   1;
    volatile uint32_t scl_busy      :   1;
    volatile uint32_t reserved1     :   6;
    volatile uint32_t dither2_en    :   1;
    volatile uint32_t reserved2     :   15;
    volatile uint32_t err2_flg      :   1;
    volatile uint32_t err2_code     :   4;
    volatile uint32_t reserved3     :   3;
} gspn_mod2_cfg_mBits;

typedef union //_GSP_MOD2_CFG_TAG
{
    volatile gspn_mod2_cfg_mBits mBits;
    volatile uint32_t dwValue;
} gspn_mod2_cfg_u;

typedef struct //_GSP_CMD_STS0_MAP
{
    volatile uint32_t stat_cmd_num  :   16;
    volatile uint32_t stopc         :   1;
    volatile uint32_t reserved      :   8;
    volatile uint32_t new_c         :   1;
    volatile uint32_t new_l0        :   1;
    volatile uint32_t new_l1        :   1;
    volatile uint32_t new_l2        :   1;
    volatile uint32_t new_l3        :   1;
    volatile uint32_t new_d         :   1;
    volatile uint32_t new_m1        :   1;
} gspn_cmd_sts0_mBits;

typedef union //_GSP_CMD_STS0_TAG
{
    volatile gspn_cmd_sts0_mBits mBits;
    volatile uint32_t dwValue;
} gspn_cmd_sts0_u;

typedef struct //_GSP_CMD_STS1_MAP
{
    volatile uint32_t coef_cache_addr   :32;
} gspn_cmd_sts1_mBits;

typedef union //_GSP_CMD_STS1_TAG
{
    volatile gspn_cmd_sts1_mBits mBits;
    volatile uint32_t dwValue;
} gspn_cmd_sts1_u;

typedef struct //_DES_DATA_CFG1_MAP
{
    volatile uint32_t y_word_endian_mod     :2;
    volatile uint32_t y_dword_endian_mod    :1;
    volatile uint32_t uv_word_endian_mod    :2;
    volatile uint32_t uv_dword_endian_mod   :1;
    volatile uint32_t r5_rgb_swap_mod       :3;
    volatile uint32_t a_swap_mod            :1;
    volatile uint32_t rot_mod               :3;
    volatile uint32_t r2y_mod               :1;
    volatile uint32_t reserved0             :2;
    volatile uint32_t format                :3;
    volatile uint32_t compress_r8           :1;
    volatile uint32_t reserved1             :12;
} gspn_des_data_cfg1_mBits;

typedef union //_DES_DATA_CFG1_TAG
{
    volatile gspn_des_data_cfg1_mBits mBits;
    volatile uint32_t dwValue;
} gspn_des_data_cfg1_u;

typedef struct
{
    volatile uint32_t addr  :32;
} gspn_addr_mBits;

typedef union
{
    volatile gspn_addr_mBits mBits;
    volatile uint32_t dwValue;
} gspn_addr_u;

typedef struct
{
    volatile uint32_t w             :13;
    volatile uint32_t reserved1     :3;
    volatile uint32_t h             :13;
    volatile uint32_t reserved2     :3;
} gspn_size_mBits;

typedef union
{
    volatile gspn_size_mBits mBits;
    volatile uint32_t dwValue;
} gspn_size_u;

typedef struct
{
    volatile uint32_t b  :8;
    volatile uint32_t g  :8;
    volatile uint32_t r  :8;
    volatile uint32_t a  :8;
} gspn_argb_mBits;

typedef union //_BACK_RGB_TAG
{
    volatile gspn_argb_mBits mBits;
    volatile uint32_t dwValue;
} gspn_argb_u;

typedef struct
{
    volatile uint32_t x             :13;
    volatile uint32_t reserved1     :3;
    volatile uint32_t y             :13;
    volatile uint32_t reserved2     :3;
} gspn_point_mBits;

typedef union
{
    volatile gspn_point_mBits mBits;
    volatile uint32_t dwValue;
} gspn_point_u;

typedef struct //_DES_DATA_CFG2_MAP
{
    volatile uint32_t y_word_endian_mod     :2;
    volatile uint32_t y_dword_endian_mod    :1;
    volatile uint32_t uv_word_endian_mod    :2;
    volatile uint32_t uv_dword_endian_mod   :1;
    volatile uint32_t r5_rgb_swap_mod       :3;
    volatile uint32_t a_swap_mod            :1;
    volatile uint32_t rot_mod               :3;
    volatile uint32_t r2y_mod               :1;
    volatile uint32_t reserved1             :2;
    volatile uint32_t format                :3;
    volatile uint32_t compress_r8           :1;
    volatile uint32_t reserved2             :12;
} gspn_des_data_cfg2_mBits;

typedef union //_DES_DATA_CFG2_TAG
{
    volatile gspn_des_data_cfg2_mBits mBits;
    volatile uint32_t dwValue;
} gspn_des_data_cfg2_u;

typedef struct //_AXI_DEBUG_MAP
{
    volatile uint32_t axi_debug     :32;
} gspn_axi_debug_mBits;

typedef union //_AXI_DEBUG_TAG
{
    volatile gspn_axi_debug_mBits mBits;
    volatile uint32_t dwValue;
} gspn_axi_debug_u;

typedef struct //_GSP_INT_MAP
{
    volatile uint32_t int_bld_raw       :   1;
    volatile uint32_t int_scl_raw       :   1;
    volatile uint32_t int_rot_raw       :   1;
    volatile uint32_t int_berr_raw      :   1;
    volatile uint32_t int_serr_raw      :   1;
    volatile uint32_t reserved1         :   3;
    volatile uint32_t int_bld_en        :   1;
    volatile uint32_t int_scl_en        :   1;
    volatile uint32_t int_rot_en        :   1;
    volatile uint32_t int_berr_en       :   1;
    volatile uint32_t int_serr_en       :   1;
    volatile uint32_t reserved2         :   3;
    volatile uint32_t int_bld_clr       :   1;
    volatile uint32_t int_scl_clr       :   1;
    volatile uint32_t int_rot_clr       :   1;
    volatile uint32_t int_berr_clr      :   1;
    volatile uint32_t int_serr_clr      :   1;
    volatile uint32_t reserved3         :   3;
    volatile uint32_t int_bld_sts       :   1;
    volatile uint32_t int_scl_sts       :   1;
    volatile uint32_t int_rot_sts       :   1;
    volatile uint32_t int_berr_sts      :   1;
    volatile uint32_t int_serr_sts      :   1;
    volatile uint32_t reserved4         :   3;
} gspn_int_mBits;

typedef union //_GSP_INT_TAG
{
    volatile gspn_int_mBits mBits;
    volatile uint32_t dwValue;
} gspn_int_u;

typedef struct //_LAYER0_CFG_MAP
{
    volatile uint32_t y_word_endian_mod         :2;
    volatile uint32_t y_dword_endian_mod        :1;
    volatile uint32_t uv_word_endian_mod        :2;
    volatile uint32_t uv_dword_endian_mod       :1;
    volatile uint32_t r5_rgb_swap_mod           :3;
    volatile uint32_t a_swap_mod                :1;
    volatile uint32_t reserved0                 :6;
    volatile uint32_t format                    :3;
    volatile uint32_t ck_en                     :1;
    volatile uint32_t pallet_en                 :1;
    volatile uint32_t layer_num                 :2;
    volatile uint32_t y2r_mod                   :2;
    volatile uint32_t reserved1                 :7;
} gspn_layer0_cfg_mBits;

typedef union //_LAYER0_CFG_TAG
{
    volatile gspn_layer0_cfg_mBits mBits;
    volatile uint32_t dwValue;
} gspn_layer0_cfg_u;

typedef struct
{
    volatile uint32_t w            :13;
    volatile uint32_t reserved     :19;
} gspn_pitch_mBits;

typedef union //_LAYER0_PITCH_TAG
{
    volatile gspn_pitch_mBits mBits;
    volatile uint32_t dwValue;
} gspn_pitch_u;

typedef struct //_Y2R_Y_PARAM_MAP
{
    volatile uint32_t y_contrast        :10;
    volatile uint32_t reserved1         :6;
    volatile uint32_t y_brightness      :9;
    volatile uint32_t reserved2         :7;
} gspn_y2r_y_param_mBits;

typedef union //_Y2R_Y_PARAM_TAG
{
    volatile gspn_y2r_y_param_mBits mBits;
    volatile uint32_t dwValue;
} gspn_y2r_y_param_u;

typedef struct
{
    volatile uint32_t saturation        :10;
    volatile uint32_t reserved1         :6;
    volatile uint32_t offset            :8;
    volatile uint32_t reserved2         :8;
} gspn_y2r_uv_param_mBits;

typedef union //_Y2R_U_PARAM_TAG
{
    volatile gspn_y2r_uv_param_mBits mBits;
    volatile uint32_t dwValue;
} gspn_y2r_uv_param_u;

typedef struct
{
    volatile uint32_t word_endian_mod   :2;
    volatile uint32_t dword_endian_mod  :1;
    volatile uint32_t r5_rgb_swap_mod   :3;
    volatile uint32_t a_swap_mod        :1;
    volatile uint32_t reserved1         :9;
    volatile uint32_t format            :2;
    volatile uint32_t ck_en             :1;
    volatile uint32_t pallet_en         :1;
    volatile uint32_t reserved2         :12;
} gspn_layer1_cfg_mBits;

typedef union //_LAYER1_CFG_TAG
{
    volatile gspn_layer1_cfg_mBits mBits;
    volatile uint32_t dwValue;
} gspn_layer1_cfg_u;

typedef struct //GSPN_CTL_REG_T
{
    volatile gspn_glb_cfg_u glb_cfg;
    volatile gspn_mod1_cfg_u mod1_cfg;
    volatile gspn_mod2_cfg_u mod2_cfg;
    volatile gspn_cmd_sts0_u cmd_sts0;
    volatile gspn_cmd_sts1_u cmd_sts1;
    volatile gspn_addr_u cmd_addr;

    volatile gspn_des_data_cfg1_u des1_data_cfg;
    volatile gspn_addr_u des1_y_addr;
    volatile gspn_addr_u des1_u_addr;
    volatile gspn_addr_u des1_v_addr;
    volatile gspn_size_u des1_pitch;
    volatile gspn_argb_u bg_rgb;
    volatile gspn_size_u des_scl_size;
    volatile gspn_point_u work_des_start;
    volatile gspn_size_u work_src_size;
    volatile gspn_point_u work_src_start;

    volatile gspn_des_data_cfg2_u des2_data_cfg;
    volatile gspn_addr_u des2_y_addr;
    volatile gspn_addr_u des2_u_addr;
    volatile gspn_addr_u des2_v_addr;
    volatile gspn_size_u des2_pitch;
    volatile gspn_axi_debug_u axi_debug;
    volatile gspn_int_u int_ctl;

    volatile gspn_layer0_cfg_u l0_cfg;
    volatile gspn_addr_u l0_y_addr;
    volatile gspn_addr_u l0_u_addr;
    volatile gspn_addr_u l0_v_addr;
    volatile gspn_pitch_u l0_pitch;
    volatile gspn_point_u l0_clip_start;
    volatile gspn_size_u l0_clip_size;
    volatile gspn_point_u l0_des_start;
    volatile gspn_argb_u l0_pallet_rgb;
    volatile gspn_argb_u l0_ck;
    volatile gspn_y2r_y_param_u y2r_y_param;
    volatile gspn_y2r_uv_param_u y2r_u_param;
    volatile gspn_y2r_uv_param_u y2r_v_param;

    volatile gspn_layer1_cfg_u l1_cfg;
    volatile gspn_addr_u l1_r_addr;
    volatile gspn_pitch_u l1_pitch;
    volatile gspn_point_u l1_clip_start;
    volatile gspn_size_u l1_clip_size;
    volatile gspn_point_u l1_des_start;
    volatile gspn_argb_u l1_pallet_rgb;
    volatile gspn_argb_u l1_ck;

    volatile gspn_layer1_cfg_u l2_cfg;
    volatile gspn_addr_u l2_r_addr;
    volatile gspn_pitch_u l2_pitch;
    volatile gspn_point_u l2_clip_start;
    volatile gspn_size_u l2_clip_size;
    volatile gspn_point_u l2_des_start;
    volatile gspn_argb_u l2_pallet_rgb;
    volatile gspn_argb_u l2_ck;

    volatile gspn_layer1_cfg_u l3_cfg;
    volatile gspn_addr_u l3_r_addr;
    volatile gspn_pitch_u l3_pitch;
    volatile gspn_point_u l3_clip_start;
    volatile gspn_size_u l3_clip_size;
    volatile gspn_point_u l3_des_start;
    volatile gspn_argb_u l3_pallet_rgb;
    volatile gspn_argb_u l3_ck;

    volatile uint32_t reserved[4];
    volatile GSPN_COEF_REG_T coef_tab;
} GSPN_CTL_REG_T;

typedef struct
{
    volatile uint32_t l0_en          :        1;
    volatile uint32_t l1_en          :        1;
    volatile uint32_t l2_en          :        1;
    volatile uint32_t l3_en          :        1;
    volatile uint32_t pmargb_l0      :        1;
    volatile uint32_t pmargb_l1      :        1;
    volatile uint32_t pmargb_l2      :        1;
    volatile uint32_t pmargb_l3      :        1;
    volatile uint32_t scl_seq        :        1;
    volatile uint32_t dither_en      :        1;
    volatile uint32_t scale_en       :        1;
    volatile uint32_t pmargb_en      :        1;
    volatile uint32_t bg_en          :        1;
    volatile uint32_t reserved0      :        3;
    volatile uint32_t new_c          :        1;
    volatile uint32_t new_l0         :        1;
    volatile uint32_t new_l1         :        1;
    volatile uint32_t new_l2         :        1;
    volatile uint32_t new_l3         :        1;
    volatile uint32_t new_d          :        1;
    volatile uint32_t new_m1         :        1;
    volatile uint32_t reserved1      :        8;
    volatile uint32_t stopc          :        1;
} GSPN_CMD_CTL_REG_mBits;

typedef union
{
    volatile GSPN_CMD_CTL_REG_mBits mBits;
    volatile uint32_t dwValue;
} GSPN_CMD_CTL_REG_U;

typedef struct
{
    volatile GSPN_CMD_CTL_REG_U cmd_ctl_info;
    volatile uint32_t coef_cathe_addr;
    volatile uint32_t next_cmd_addr;
    volatile uint32_t reserved;
} GSPN_CMD_HEARD_REG_T;

typedef struct
{
    volatile gspn_des_data_cfg1_u dat_cfg;
    volatile uint32_t y_addr;
    volatile uint32_t u_addr;
    volatile uint32_t v_addr;
    volatile uint32_t pitch;
    volatile uint32_t bg_rgb;
    volatile uint32_t scale_out_size;
    volatile uint32_t work_dst_start;
    volatile uint32_t work_src_size;
    volatile uint32_t work_src_start;
} GSPN_CMD_DST_REG_T;

typedef struct
{
    volatile gspn_layer0_cfg_u layer_cfg;
    volatile uint32_t y_addr;
    volatile uint32_t u_addr;
    volatile uint32_t v_addr;
    volatile uint32_t pitch;
    volatile uint32_t clip_start;
    volatile uint32_t clip_size;
    volatile uint32_t des_start;
    volatile uint32_t pallet_rgb;
    volatile uint32_t color_key;
    volatile uint32_t y2r_y_param;
    volatile uint32_t y2r_u_param;
    volatile uint32_t y2r_v_param;
    volatile uint32_t reserved;
} GSPN_CMD_IMG_REG_T;

typedef struct
{
    volatile gspn_layer1_cfg_u layer_cfg;
    volatile uint32_t layer_r_addr;
    volatile uint32_t layer_pitch;
    volatile uint32_t clip_start;
    volatile uint32_t clip_size;
    volatile uint32_t dst_start;
    volatile uint32_t layer_grey_rgb;
    volatile uint32_t color_key;
} GSPN_CMD_OSD_REG_T;

typedef struct
{
    volatile GSPN_CMD_HEARD_REG_T head_info;
    volatile GSPN_CMD_DST_REG_T des_info;//des_info
    volatile GSPN_CMD_IMG_REG_T img_info;
    volatile GSPN_CMD_OSD_REG_T osd1_info;
    volatile GSPN_CMD_OSD_REG_T osd2_info;
    volatile GSPN_CMD_OSD_REG_T osd3_info;
    volatile GSPN_COEF_REG_T coef_matrix;
} GSPN_CMD_REG_T;

/*global cfg*/
#define GSPN_R_GAP_SET(reg_base,v)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->glb_cfg.mBits.gap_rb = (v)
#define GSPN_W_GAP_SET(reg_base,v)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->glb_cfg.mBits.gap_wb = (v)
#define GSPN_HTAP4_SET(reg_base,v)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->glb_cfg.mBits.htap4_en = (v)
#define GSPN_CMD_WORD_ENDIAN_SET(reg_base,v)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->glb_cfg.mBits.cmd_word_endian_mod = (v)
#define GSPN_CMD_DWORD_ENDIAN_SET(reg_base,v)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->glb_cfg.mBits.cmd_dword_endian_mod = (v)
#define GSPN_CMD_EN_SET(reg_base,v)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->glb_cfg.mBits.cmd_en = (v)
#define GSPN_CMD_ADDR_SET(reg_base,v)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->cmd_addr.dwValue = (v)
#define GSPN_CMD_ADDR_GET(reg_base)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->cmd_addr.dwValue
#define GSPN_RUN_MODE_SET(reg_base,v)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->glb_cfg.mBits.run_mod = (v)
#define GSPN_SCL_RST(reg_base)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->glb_cfg.mBits.scl_clr = 1;\
    udelay(10);\
    ((volatile GSPN_CTL_REG_T*)reg_base)->glb_cfg.mBits.scl_clr = 0;
#define GSPN_RUN_MODE_GET(reg_base)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->glb_cfg.mBits.run_mod

/*mod1 cfg*/
#define GSPN_MOD1_Lx_EN_GET(reg_base,x)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->mod1_cfg.mBits.l##x##_en
#define GSPN_MOD1_Lx_EN_SET(reg_base,v,x)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->mod1_cfg.mBits.l##x##_en = (v)
#define GSPN_MOD1_LxPM_EN_SET(reg_base,v,x)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->mod1_cfg.mBits.pmargb_l##x = (v)
#define GSPN_MOD1_PM_EN_SET(reg_base,v)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->mod1_cfg.mBits.pmargb_en = (v)
#define GSPN_MOD1_SCL_SEQ_SET(reg_base,v)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->mod1_cfg.mBits.scale_seq = (v)
#define GSPN_MOD1_DITHER_EN_SET(reg_base,v)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->mod1_cfg.mBits.dither1_en = (v)
#define GSPN_MOD1_SCL_EN_SET(reg_base,v)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->mod1_cfg.mBits.scale_en = (v)
#define GSPN_MOD1_BG_EN_SET(reg_base,v)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->mod1_cfg.mBits.bg_en = (v)
#define GSPN_BLEND_TRIGGER(reg_base)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->mod1_cfg.mBits.bld_run = 1
#define GSPN_MOD1_BUSY_GET(reg_base)\
    ((((volatile GSPN_CTL_REG_T*)reg_base)->mod1_cfg.mBits.bld_busy)\
    ||(((volatile GSPN_CTL_REG_T*)reg_base)->mod1_cfg.mBits.rch_busy)\
    ||(((volatile GSPN_CTL_REG_T*)reg_base)->mod1_cfg.mBits.wch_busy))
#define GSPN_MOD2_BUSY_GET(reg_base)\
    (((volatile GSPN_CTL_REG_T*)reg_base)->mod2_cfg.mBits.scl_busy)
#define GSPN_MOD1_CH_BUSY_GET(reg_base)\
    (((volatile GSPN_CTL_REG_T*)reg_base)->mod1_cfg.mBits.rch_busy)\
    ||(((volatile GSPN_CTL_REG_T*)reg_base)->mod1_cfg.mBits.wch_busy)
#define GSPN_MOD1_ERRFLAG_GET(reg_base)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->mod1_cfg.mBits.err1_flg
#define GSPN_MOD1_ERRCODE_GET(reg_base)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->mod1_cfg.mBits.err1_code

/*mod2 cfg*/
#define GSPN_MOD2_ERRFLAG_GET(reg_base)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->mod2_cfg.mBits.err2_flg
#define GSPN_MOD2_ERRCODE_GET(reg_base)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->mod2_cfg.mBits.err2_code
#define GSPN_MOD2_DITHER_EN_SET(reg_base,v)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->mod2_cfg.mBits.dither2_en = (v)
#define GSPN_SCALE_TRIGGER(reg_base)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->mod2_cfg.mBits.scl_run = 1

/*dst1 cfg*/
#define GSPN_DST1_R8COMPRESS_SET(reg_base,v)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->des1_data_cfg.mBits.compress_r8 = (v)
#define GSPN_DST1_FMT_SET(reg_base,v)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->des1_data_cfg.mBits.format = (v)
#define GSPN_DST1_R2Y_MODE_SET(reg_base,v)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->des1_data_cfg.mBits.r2y_mod = (v)
#define GSPN_DST1_ROT_MODE_SET(reg_base,v)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->des1_data_cfg.mBits.rot_mod = (v)
#define GSPN_DST1_Y_WORD_ENDIAN_SET(reg_base,v)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->des1_data_cfg.mBits.y_word_endian_mod = (v)
#define GSPN_DST1_Y_DWORD_ENDIAN_SET(reg_base,v)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->des1_data_cfg.mBits.y_dword_endian_mod = (v)
#define GSPN_DST1_UV_WORD_ENDIAN_SET(reg_base,v)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->des1_data_cfg.mBits.uv_word_endian_mod = (v)
#define GSPN_DST1_UV_DWORD_ENDIAN_SET(reg_base,v)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->des1_data_cfg.mBits.uv_dword_endian_mod = (v)
#define GSPN_DST1_R5_RGB_SWAP_SET(reg_base,v)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->des1_data_cfg.mBits.r5_rgb_swap_mod = (v)
#define GSPN_DST1_R8_AR_SWAP_SET(reg_base,v)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->des1_data_cfg.mBits.a_swap_mod = (v)
#define GSPN_DST1_Y_ADDR_SET(reg_base,v)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->des1_y_addr.dwValue = (v)
#define GSPN_DST1_U_ADDR_SET(reg_base,v)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->des1_u_addr.dwValue = (v)
#define GSPN_DST1_V_ADDR_SET(reg_base,v)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->des1_v_addr.dwValue = (v)
#define GSPN_DST1_Y_ADDR_GET(reg_base)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->des1_y_addr.dwValue
#define GSPN_DST1_U_ADDR_GET(reg_base)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->des1_u_addr.dwValue
#define GSPN_DST1_V_ADDR_GET(reg_base)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->des1_v_addr.dwValue
#define GSPN_DST1_PITCH_SET(reg_base,W,H)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->des1_pitch.mBits.w = (W);\
    ((volatile GSPN_CTL_REG_T*)reg_base)->des1_pitch.mBits.h = (H)
#define GSPN_DST1_BG_COLOR_SET(reg_base,A,R,G,B)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->bg_rgb.mBits.a = (A);\
    ((volatile GSPN_CTL_REG_T*)reg_base)->bg_rgb.mBits.r = (R);\
    ((volatile GSPN_CTL_REG_T*)reg_base)->bg_rgb.mBits.g = (G);\
    ((volatile GSPN_CTL_REG_T*)reg_base)->bg_rgb.mBits.b = (B)
#define GSPN_SCL_OUT_SIZE_SET(reg_base,W,H)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->des_scl_size.mBits.w = (W);\
    ((volatile GSPN_CTL_REG_T*)reg_base)->des_scl_size.mBits.h = (H)
#define GSPN_WORK_DST_START_SET(reg_base,X,Y)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->work_des_start.mBits.x = (X);\
    ((volatile GSPN_CTL_REG_T*)reg_base)->work_des_start.mBits.y = (Y)
#define GSPN_WORK_SRC_SIZE_SET(reg_base,W,H)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->work_src_size.mBits.w = (W);\
    ((volatile GSPN_CTL_REG_T*)reg_base)->work_src_size.mBits.h = (H)
#define GSPN_WORK_SRC_START_SET(reg_base,X,Y)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->work_src_start.mBits.x = (X);\
    ((volatile GSPN_CTL_REG_T*)reg_base)->work_src_start.mBits.y = (Y)


/*dst2 cfg*/
#define GSPN_DST2_R8COMPRESS_SET(reg_base,v)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->des2_data_cfg.mBits.compress_r8 = (v)
#define GSPN_DST2_FMT_SET(reg_base,v)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->des2_data_cfg.mBits.format = (v)
#define GSPN_DST2_R2Y_MODE_SET(reg_base,v)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->des2_data_cfg.mBits.r2y_mod = (v)
#define GSPN_DST2_ROT_MODE_SET(reg_base,v)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->des2_data_cfg.mBits.rot_mod = (v)
#define GSPN_DST2_Y_WORD_ENDIAN_SET(reg_base,v)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->des2_data_cfg.mBits.y_word_endian_mod = (v)
#define GSPN_DST2_Y_DWORD_ENDIAN_SET(reg_base,v)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->des2_data_cfg.mBits.y_dword_endian_mod = (v)
#define GSPN_DST2_UV_WORD_ENDIAN_SET(reg_base,v)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->des2_data_cfg.mBits.uv_word_endian_mod = (v)
#define GSPN_DST2_UV_DWORD_ENDIAN_SET(reg_base,v)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->des2_data_cfg.mBits.uv_dword_endian_mod = (v)
#define GSPN_DST2_R5_RGB_SWAP_SET(reg_base,v)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->des2_data_cfg.mBits.r5_rgb_swap_mod = (v)
#define GSPN_DST2_R8_AR_SWAP_SET(reg_base,v)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->des2_data_cfg.mBits.a_swap_mod = (v)
#define GSPN_DST2_Y_ADDR_SET(reg_base,v)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->des2_y_addr.dwValue = (v)
#define GSPN_DST2_U_ADDR_SET(reg_base,v)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->des2_u_addr.dwValue = (v)
#define GSPN_DST2_V_ADDR_SET(reg_base,v)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->des2_v_addr.dwValue = (v)
#define GSPN_DST2_Y_ADDR_GET(reg_base)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->des2_y_addr.dwValue
#define GSPN_DST2_U_ADDR_GET(reg_base)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->des2_u_addr.dwValue
#define GSPN_DST2_V_ADDR_GET(reg_base)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->des2_v_addr.dwValue
#define GSPN_DST2_PITCH_SET(reg_base,W,H)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->des2_pitch.mBits.w = (W);\
    ((volatile GSPN_CTL_REG_T*)reg_base)->des2_pitch.mBits.h = (H)


/*layer 0~3 common macro define*/
#define GSPN_Lx_Y_WORD_ENDIAN_SET(reg_base,v,L)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->l##L##_cfg.mBits.word_endian_mod = (v)
#define GSPN_Lx_Y_DWORD_ENDIAN_SET(reg_base,v,L)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->l##L##_cfg.mBits.dword_endian_mod = (v)
#define GSPN_Lx_R5_RGB_SWAP_SET(reg_base,v,L)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->l##L##_cfg.mBits.r5_rgb_swap_mod = (v)
#define GSPN_Lx_R8_AR_SWAP_SET(reg_base,v,L)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->l##L##_cfg.mBits.a_swap_mod = (v)
#define GSPN_Lx_FMT_SET(reg_base,v,L)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->l##L##_cfg.mBits.format = (v)
#define GSPN_Lx_CK_EN_SET(reg_base,v,L)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->l##L##_cfg.mBits.ck_en = (v)
#define GSPN_Lx_PALLET_EN_SET(reg_base,v,L)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->l##L##_cfg.mBits.pallet_en = (v)
#define GSPN_Lx_ADDR_SET(reg_base,v,L)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->l##L##_r_addr.dwValue = (v)
#define GSPN_Lx_ADDR_GET(reg_base,L)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->l##L##_r_addr.dwValue
#define GSPN_Lx_PITCH_SET(reg_base,v,L)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->l##L##_pitch.mBits.w = (v)
#define GSPN_Lx_CLIP_START_SET(reg_base,X,Y,L)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->l##L##_clip_start.mBits.x = (X);\
    ((volatile GSPN_CTL_REG_T*)reg_base)->l##L##_clip_start.mBits.y = (Y)
#define GSPN_Lx_CLIP_SIZE_SET(reg_base,W,H,L)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->l##L##_clip_size.mBits.w = (W);\
    ((volatile GSPN_CTL_REG_T*)reg_base)->l##L##_clip_size.mBits.h = (H)
#define GSPN_Lx_DST_START_SET(reg_base,X,Y,L)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->l##L##_des_start.mBits.x = (X);\
    ((volatile GSPN_CTL_REG_T*)reg_base)->l##L##_des_start.mBits.y = (Y)
#define GSPN_Lx_PALLET_COLOR_SET(reg_base,A,R,G,B,L)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->l##L##_pallet_rgb.mBits.a = (A);\
    ((volatile GSPN_CTL_REG_T*)reg_base)->l##L##_pallet_rgb.mBits.r = (R);\
    ((volatile GSPN_CTL_REG_T*)reg_base)->l##L##_pallet_rgb.mBits.g = (G);\
    ((volatile GSPN_CTL_REG_T*)reg_base)->l##L##_pallet_rgb.mBits.b = (B)
#define GSPN_Lx_CK_COLOR_SET(reg_base,A,R,G,B,L)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->l##L##_ck.mBits.a = (A);\
    ((volatile GSPN_CTL_REG_T*)reg_base)->l##L##_ck.mBits.r = (R);\
    ((volatile GSPN_CTL_REG_T*)reg_base)->l##L##_ck.mBits.g = (G);\
    ((volatile GSPN_CTL_REG_T*)reg_base)->l##L##_ck.mBits.b = (B)
#define GSPN_Lx_INFO_CFG(reg_base,lx_info,L)\
    GSPN_Lx_PALLET_EN_SET(reg_base,(lx_info)->pallet_en,L);\
    GSPN_Lx_CK_EN_SET(reg_base,(lx_info)->ck_en,L);\
    GSPN_Lx_FMT_SET(reg_base,(lx_info)->fmt,L);\
    GSPN_Lx_R8_AR_SWAP_SET(reg_base,(lx_info)->endian.a_swap_mod,L);\
    GSPN_Lx_R5_RGB_SWAP_SET(reg_base,(lx_info)->endian.rgb_swap_mod,L);\
    GSPN_Lx_Y_WORD_ENDIAN_SET(reg_base,(lx_info)->endian.y_word_endn,L);\
    GSPN_Lx_Y_DWORD_ENDIAN_SET(reg_base,(lx_info)->endian.y_dword_endn,L);\
    GSPN_Lx_ADDR_SET(reg_base,(lx_info)->addr.plane_y,L);\
    GSPN_Lx_PITCH_SET(reg_base,(lx_info)->pitch.w,L);\
    GSPN_Lx_CLIP_START_SET(reg_base,(lx_info)->clip_start.x,(lx_info)->clip_start.y,L);\
    GSPN_Lx_CLIP_SIZE_SET(reg_base,(lx_info)->clip_size.w,(lx_info)->clip_size.h,L);\
    GSPN_Lx_DST_START_SET(reg_base,(lx_info)->dst_start.x,(lx_info)->dst_start.y,L);\
    GSPN_Lx_PALLET_COLOR_SET(reg_base,(lx_info)->pallet_color.a,(lx_info)->pallet_color.r,(lx_info)->pallet_color.g,(lx_info)->pallet_color.b,L);\
    GSPN_Lx_CK_COLOR_SET(reg_base,(lx_info)->alpha,(lx_info)->ck_color.r,(lx_info)->ck_color.g,(lx_info)->ck_color.b,L);\
    GSPN_MOD1_LxPM_EN_SET(reg_base,(lx_info)->pmargb_mod,L);\
    GSPN_MOD1_Lx_EN_SET(reg_base,(lx_info)->layer_en,L)


/*layer 0 special macro define*/
#define GSPN_L0_Y_WORD_ENDIAN_SET(reg_base,v)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->l0_cfg.mBits.y_word_endian_mod = (v)
#define GSPN_L0_Y_DWORD_ENDIAN_SET(reg_base,v)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->l0_cfg.mBits.y_dword_endian_mod = (v)
#define GSPN_L0_UV_WORD_ENDIAN_SET(reg_base,v)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->l0_cfg.mBits.uv_word_endian_mod = (v)
#define GSPN_L0_UV_DWORD_ENDIAN_SET(reg_base,v)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->l0_cfg.mBits.uv_dword_endian_mod = (v)
#define GSPN_L0_ZORDER_SET(reg_base,v)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->l0_cfg.mBits.layer_num = (v)
#define GSPN_L0_Y2R_MODE_SET(reg_base,v)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->l0_cfg.mBits.y2r_mod = (v)
#define GSPN_L0_Y_ADDR_SET(reg_base,v)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->l0_y_addr.dwValue = (v)
#define GSPN_L0_U_ADDR_SET(reg_base,v)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->l0_u_addr.dwValue = (v)
#define GSPN_L0_V_ADDR_SET(reg_base,v)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->l0_v_addr.dwValue = (v)
#define GSPN_L0_Y_ADDR_GET(reg_base)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->l0_y_addr.dwValue
#define GSPN_L0_U_ADDR_GET(reg_base)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->l0_u_addr.dwValue
#define GSPN_L0_V_ADDR_GET(reg_base)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->l0_v_addr.dwValue
#define GSPN_L0_CONTRAST_SET(reg_base,v)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->y2r_y_param.mBits.y_contrast = (v)

/*B2T() used to convert int32_t to bitwidth's two's complement*/
//#define B2T(bitwidth,i)	((((i)<0)?((~(-1*(i)))+1):(i))& ((1UL<<(bitwidth))-1))
#define B2T(bitwidth,i) (i & ((1UL<<(bitwidth))-1))

#define GSPN_L0_BRIGHT_SET(reg_base,v)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->y2r_y_param.mBits.y_brightness = B2T(9,(v))
#define GSPN_L0_USATURATION_SET(reg_base,v)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->y2r_u_param.mBits.saturation = (v)
#define GSPN_L0_UOFFSET_SET(reg_base,v)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->y2r_u_param.mBits.offset = (v)
#define GSPN_L0_VSATURATION_SET(reg_base,v)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->y2r_v_param.mBits.saturation = (v)
#define GSPN_L0_VOFFSET_SET(reg_base,v)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->y2r_v_param.mBits.offset = (v)

/*local interrupt cfg*/
#define GSPN_INT_REG_GET(reg_base)    (((volatile GSPN_CTL_REG_T*)reg_base)->int_ctl.dwValue)
#define GSPN_INT_REG_SET(reg_base,v)    ((volatile GSPN_CTL_REG_T*)reg_base)->int_ctl.dwValue = (v)
#define GSPN_INT_RAW_BLD_BIT    (BIT(0+0))
#define GSPN_INT_RAW_SCL_BIT    (BIT(1+0))
#define GSPN_INT_RAW_ROT_BIT    (BIT(2+0))
#define GSPN_INT_RAW_BERR_BIT    (BIT(3+0))
#define GSPN_INT_RAW_SERR_BIT    (BIT(4+0))
#define GSPN_INT_EN_BLD_BIT    (BIT(0+8))
#define GSPN_INT_EN_SCL_BIT    (BIT(1+8))
#define GSPN_INT_EN_ROT_BIT    (BIT(2+8))
#define GSPN_INT_EN_BERR_BIT    (BIT(3+8))
#define GSPN_INT_EN_SERR_BIT    (BIT(4+8))
#define GSPN_INT_CLR_BLD_BIT    (BIT(0+16))
#define GSPN_INT_CLR_SCL_BIT    (BIT(1+16))
#define GSPN_INT_CLR_ROT_BIT    (BIT(2+16))
#define GSPN_INT_CLR_BERR_BIT    (BIT(3+16))
#define GSPN_INT_CLR_SERR_BIT    (BIT(4+16))
#define GSPN_INT_STS_BLD_BIT    (BIT(0+24))
#define GSPN_INT_STS_SCL_BIT    (BIT(1+24))
#define GSPN_INT_STS_ROT_BIT    (BIT(2+24))
#define GSPN_INT_STS_BERR_BIT    (BIT(3+24))
#define GSPN_INT_STS_SERR_BIT    (BIT(4+24))
#define GSPN_IRQSTATUS_CLEAR(reg_base)\
    ((volatile GSPN_CTL_REG_T*)reg_base)->int_ctl.dwValue |= 0x00FB0000;\
    udelay(10);\
    ((volatile GSPN_CTL_REG_T*)reg_base)->int_ctl.dwValue &= 0xFF04FFFF
#define GSPN_IRQENABLE_SET(reg_base,v)\
    if(v)\
        ((volatile GSPN_CTL_REG_T*)reg_base)->int_ctl.dwValue |= 0x0000FB00;\
    else\
        ((volatile GSPN_CTL_REG_T*)reg_base)->int_ctl.dwValue &= 0xFFFF04FF

#define GSPN_SOFT_RESET(reg_base,reset_base,reset_bit) {\
    int i = 0;\
    while(i < 10000/*polling 10ms*/) {\
        if(!GSPN_MOD1_CH_BUSY_GET(reg_base)) {\
            REG_SET(reset_base,REG_GET(reset_base) |(reset_bit));\
            udelay(10);\
            REG_SET(reset_base,REG_GET(reset_base) & (~(reset_bit)));\
            break;\
        }\
        udelay(1);\
        i++;\
    }\
}

#endif

