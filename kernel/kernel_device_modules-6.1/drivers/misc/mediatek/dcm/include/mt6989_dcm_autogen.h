/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __MTK_DCM_AUTOGEN_H__
#define __MTK_DCM_AUTOGEN_H__

#include <mtk_dcm.h>

#if IS_ENABLED(CONFIG_OF)
/* TODO: Fix all base addresses. */
extern unsigned long dcm_mcusys_par_wrap_base;
extern unsigned long dcm_infra_ao_bcrm_base;
extern unsigned long dcm_peri_ao_bcrm_base;
extern unsigned long dcm_vlp_ao_bcrm_base;

#if !defined(MCUSYS_PAR_WRAP_BASE)
#define MCUSYS_PAR_WRAP_BASE (dcm_mcusys_par_wrap_base - 0x10)
#endif /* !defined(MCUSYS_PAR_WRAP_BASE) */
#if !defined(INFRA_AO_BCRM_BASE)
#define INFRA_AO_BCRM_BASE (dcm_infra_ao_bcrm_base)
#endif /* !defined(INFRA_AO_BCRM_BASE) */
#if !defined(PERI_AO_BCRM_BASE)
#define PERI_AO_BCRM_BASE (dcm_peri_ao_bcrm_base)
#endif /* !defined(PERI_AO_BCRM_BASE) */
#if !defined(VLP_AO_BCRM_BASE)
#define VLP_AO_BCRM_BASE (dcm_vlp_ao_bcrm_base)
#endif /* !defined(VLP_AO_BCRM_BASE) */

#else /* !IS_ENABLED(CONFIG_OF)) */

/* Here below used in CTP and pl for references. */
#undef MCUSYS_PAR_WRAP_BASE
#undef INFRA_AO_BCRM_BASE
#undef PERI_AO_BCRM_BASE
#undef VLP_AO_BCRM_BASE

/* Base */
#define MCUSYS_PAR_WRAP_BASE 0xc000000
#define INFRA_AO_BCRM_BASE   0x10022000
#define PERI_AO_BCRM_BASE    0x11035000
#define VLP_AO_BCRM_BASE     0x1c017000
#endif /* if IS_ENABLED(CONFIG_OF)) */

/* Register Definition */
#define MCUSYS_PAR_WRAP_L3_SHARE_DCM_CTRL                 (MCUSYS_PAR_WRAP_BASE + 0x78)
#define MCUSYS_PAR_WRAP_MP_ADB_DCM_CFG0                   (MCUSYS_PAR_WRAP_BASE + 0x270)
#define MCUSYS_PAR_WRAP_ADB_FIFO_DCM_EN                   (MCUSYS_PAR_WRAP_BASE + 0x278)
#define HUNTER_CORE0_CONFIG_REG_STALL_DCM_CONF            (MCUSYS_PAR_WRAP_BASE + 0x180210)
#define MCUSYS_PAR_WRAP_MP0_DCM_CFG0                      (MCUSYS_PAR_WRAP_BASE + 0x27c)
#define HUNTER_CORE1_CONFIG_REG_STALL_DCM_CONF            (MCUSYS_PAR_WRAP_BASE + 0x190210)
#define HUNTER_CORE2_CONFIG_REG_STALL_DCM_CONF            (MCUSYS_PAR_WRAP_BASE + 0x1a0210)
#define HUNTER_CORE3_CONFIG_REG_STALL_DCM_CONF            (MCUSYS_PAR_WRAP_BASE + 0x1b0210)
#define BCPU_CORE4_STALL_DCM_CONF                         (MCUSYS_PAR_WRAP_BASE + 0x1c0210)
#define BCPU_CORE5_STALL_DCM_CONF                         (MCUSYS_PAR_WRAP_BASE + 0x1d0210)
#define BCPU_CORE6_STALL_DCM_CONF                         (MCUSYS_PAR_WRAP_BASE + 0x1e0210)
#define BCPU_CORE7_STALL_DCM_CONF                         (MCUSYS_PAR_WRAP_BASE + 0x1f0210)
#define MCUSYS_PAR_WRAP_QDCM_CONFIG0                      (MCUSYS_PAR_WRAP_BASE + 0x280)
#define MCUSYS_PAR_WRAP_L3GIC_ARCH_CG_CONFIG              (MCUSYS_PAR_WRAP_BASE + 0x294)
#define MCUSYS_PAR_WRAP_QDCM_CONFIG1                      (MCUSYS_PAR_WRAP_BASE + 0x284)
#define MCUSYS_PAR_WRAP_QDCM_CONFIG2                      (MCUSYS_PAR_WRAP_BASE + 0x288)
#define MCUSYS_PAR_WRAP_QDCM_CONFIG3                      (MCUSYS_PAR_WRAP_BASE + 0x28c)
#define MCUSYS_PAR_WRAP_CI700_DCM_CTRL                    (MCUSYS_PAR_WRAP_BASE + 0x298)
#define MCUSYS_PAR_WRAP_CBIP_CABGEN_3TO1_CONFIG           (MCUSYS_PAR_WRAP_BASE + 0x2a0)
#define MCUSYS_PAR_WRAP_CBIP_CABGEN_2TO1_CONFIG           (MCUSYS_PAR_WRAP_BASE + 0x2a4)
#define MCUSYS_PAR_WRAP_CBIP_CABGEN_4TO2_CONFIG           (MCUSYS_PAR_WRAP_BASE + 0x2a8)
#define MCUSYS_PAR_WRAP_CBIP_CABGEN_1TO2_CONFIG           (MCUSYS_PAR_WRAP_BASE + 0x2ac)
#define MCUSYS_PAR_WRAP_CBIP_CABGEN_2TO5_CONFIG           (MCUSYS_PAR_WRAP_BASE + 0x2b0)
#define MCUSYS_PAR_WRAP_CBIP_P2P_CONFIG0                  (MCUSYS_PAR_WRAP_BASE + 0x2b4)
#define MCUSYS_PAR_WRAP_MP_CENTRAL_FABRIC_SUB_CHANNEL_CG  (MCUSYS_PAR_WRAP_BASE + 0x2b8)
#define MCUSYS_PAR_WRAP_CMN_DIV_CLK_CTRL                  (MCUSYS_PAR_WRAP_BASE + 0x2d8)
#define MCUSYS_PAR_WRAP_ACP_SLAVE_DCM_EN                  (MCUSYS_PAR_WRAP_BASE + 0x2dc)
#define MCUSYS_PAR_WRAP_GIC_SPI_SLOW_CK_CFG               (MCUSYS_PAR_WRAP_BASE + 0x2e0)
#define MCUSYS_PAR_WRAP_EBG_CKE_WRAP_FIFO_CFG             (MCUSYS_PAR_WRAP_BASE + 0x404)
#define CPC_CONFIG_REG_CPC_DCM_ENABLE                     (MCUSYS_PAR_WRAP_BASE + 0x4019c)
#define VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_0 (INFRA_AO_BCRM_BASE + 0x18)
#define VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_1 (INFRA_AO_BCRM_BASE + 0x1c)
#define VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_2 (INFRA_AO_BCRM_BASE + 0x20)
#define VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_3 (INFRA_AO_BCRM_BASE + 0x24)
#define VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_4 (INFRA_AO_BCRM_BASE + 0x28)
#define VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_5 (INFRA_AO_BCRM_BASE + 0x2c)
#define VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_6 (INFRA_AO_BCRM_BASE + 0x30)
#define VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_7 (INFRA_AO_BCRM_BASE + 0x34)
#define VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_8 (INFRA_AO_BCRM_BASE + 0x38)
#define VDNR_DCM_TOP_INFRA_PAR_BUS_u_INFRA_PAR_BUS_CTRL_9 (INFRA_AO_BCRM_BASE + 0x3c)
#define VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_0   (PERI_AO_BCRM_BASE + 0x18)
#define VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_1   (PERI_AO_BCRM_BASE + 0x1c)
#define VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_2   (PERI_AO_BCRM_BASE + 0x20)
#define VDNR_DCM_TOP_VLP_PAR_BUS_u_VLP_PAR_BUS_CTRL_0     (VLP_AO_BCRM_BASE + 0xa4)

void dcm_mcusys_par_wrap_mcu_l3c_dcm(int on);
bool dcm_mcusys_par_wrap_mcu_l3c_dcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_acp_dcm(int on);
bool dcm_mcusys_par_wrap_mcu_acp_dcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_adb_dcm(int on);
bool dcm_mcusys_par_wrap_mcu_adb_dcm_is_on(void);
void dcm_mcusys_par_wrap_core0_stall_dcm(int on);
bool dcm_mcusys_par_wrap_core0_stall_dcm_is_on(void);
void dcm_mcusys_par_wrap_core1_stall_dcm(int on);
bool dcm_mcusys_par_wrap_core1_stall_dcm_is_on(void);
void dcm_mcusys_par_wrap_core2_stall_dcm(int on);
bool dcm_mcusys_par_wrap_core2_stall_dcm_is_on(void);
void dcm_mcusys_par_wrap_core3_stall_dcm(int on);
bool dcm_mcusys_par_wrap_core3_stall_dcm_is_on(void);
void dcm_mcusys_par_wrap_core4_stall_dcm(int on);
bool dcm_mcusys_par_wrap_core4_stall_dcm_is_on(void);
void dcm_mcusys_par_wrap_core5_stall_dcm(int on);
bool dcm_mcusys_par_wrap_core5_stall_dcm_is_on(void);
void dcm_mcusys_par_wrap_core6_stall_dcm(int on);
bool dcm_mcusys_par_wrap_core6_stall_dcm_is_on(void);
void dcm_mcusys_par_wrap_core7_stall_dcm(int on);
bool dcm_mcusys_par_wrap_core7_stall_dcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_apb_dcm(int on);
bool dcm_mcusys_par_wrap_mcu_apb_dcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_io_dcm(int on);
bool dcm_mcusys_par_wrap_mcu_io_dcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_bus_qdcm(int on);
bool dcm_mcusys_par_wrap_mcu_bus_qdcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_core_qdcm(int on);
bool dcm_mcusys_par_wrap_mcu_core_qdcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_bkr_ldcm(int on);
bool dcm_mcusys_par_wrap_mcu_bkr_ldcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_cbip_dcm(int on);
bool dcm_mcusys_par_wrap_mcu_cbip_dcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_misc_dcm(int on);
bool dcm_mcusys_par_wrap_mcu_misc_dcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_bkr_div_dcm(int on);
bool dcm_mcusys_par_wrap_mcu_bkr_div_dcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_dsu_acp_dcm(int on);
bool dcm_mcusys_par_wrap_mcu_dsu_acp_dcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_chi_mon_dcm(int on);
bool dcm_mcusys_par_wrap_mcu_chi_mon_dcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_gic_spi_dcm(int on);
bool dcm_mcusys_par_wrap_mcu_gic_spi_dcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_ebg_dcm(int on);
bool dcm_mcusys_par_wrap_mcu_ebg_dcm_is_on(void);
void dcm_mcusys_par_wrap_cpc_pbi_dcm(int on);
bool dcm_mcusys_par_wrap_cpc_pbi_dcm_is_on(void);
void dcm_mcusys_par_wrap_cpc_turbo_dcm(int on);
bool dcm_mcusys_par_wrap_cpc_turbo_dcm_is_on(void);
void dcm_infra_ao_bcrm_infra_bus_dcm(int on);
bool dcm_infra_ao_bcrm_infra_bus_dcm_is_on(void);
void dcm_peri_ao_bcrm_peri_bus_dcm(int on);
bool dcm_peri_ao_bcrm_peri_bus_dcm_is_on(void);
void dcm_vlp_ao_bcrm_vlp_bus_dcm(int on);
bool dcm_vlp_ao_bcrm_vlp_bus_dcm_is_on(void);
#endif /* __MTK_DCM_AUTOGEN_H__ */
