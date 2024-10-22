// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/trace_events.h>

#include "clk-mtk.h"
#include "clk-mux.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6989-clk.h>

/* bringup config */
#define MT_CCF_BRINGUP		1
#define MT_CCF_PLL_DISABLE	0
#define MT_CCF_MUX_DISABLE	0

/* Regular Number Definition */
#define INV_OFS	-1
#define INV_BIT	-1

/* TOPCK MUX SEL REG */
#define CLK_CFG_UPDATE				0x0004
#define CLK_CFG_UPDATE1				0x0008
#define CLK_CFG_UPDATE2				0x000c
#define CKSYS2_CLK_CFG_UPDATE			0x0804
#define VLP_CLK_CFG_UPDATE			0x0004
#define CLK_CFG_0				0x0010
#define CLK_CFG_0_SET				0x0014
#define CLK_CFG_0_CLR				0x0018
#define CLK_CFG_1				0x0020
#define CLK_CFG_1_SET				0x0024
#define CLK_CFG_1_CLR				0x0028
#define CLK_CFG_2				0x0030
#define CLK_CFG_2_SET				0x0034
#define CLK_CFG_2_CLR				0x0038
#define CLK_CFG_3				0x0040
#define CLK_CFG_3_SET				0x0044
#define CLK_CFG_3_CLR				0x0048
#define CLK_CFG_4				0x0050
#define CLK_CFG_4_SET				0x0054
#define CLK_CFG_4_CLR				0x0058
#define CLK_CFG_5				0x0060
#define CLK_CFG_5_SET				0x0064
#define CLK_CFG_5_CLR				0x0068
#define CLK_CFG_6				0x0070
#define CLK_CFG_6_SET				0x0074
#define CLK_CFG_6_CLR				0x0078
#define CLK_CFG_7				0x0080
#define CLK_CFG_7_SET				0x0084
#define CLK_CFG_7_CLR				0x0088
#define CLK_CFG_8				0x0090
#define CLK_CFG_8_SET				0x0094
#define CLK_CFG_8_CLR				0x0098
#define CLK_CFG_9				0x00A0
#define CLK_CFG_9_SET				0x00A4
#define CLK_CFG_9_CLR				0x00A8
#define CLK_CFG_10				0x00B0
#define CLK_CFG_10_SET				0x00B4
#define CLK_CFG_10_CLR				0x00B8
#define CLK_CFG_11				0x00C0
#define CLK_CFG_11_SET				0x00C4
#define CLK_CFG_11_CLR				0x00C8
#define CLK_CFG_12				0x00D0
#define CLK_CFG_12_SET				0x00D4
#define CLK_CFG_12_CLR				0x00D8
#define CLK_CFG_13				0x00E0
#define CLK_CFG_13_SET				0x00E4
#define CLK_CFG_13_CLR				0x00E8
#define CLK_CFG_14				0x00F0
#define CLK_CFG_14_SET				0x00F4
#define CLK_CFG_14_CLR				0x00F8
#define CLK_CFG_15				0x0100
#define CLK_CFG_15_SET				0x0104
#define CLK_CFG_15_CLR				0x0108
#define CLK_CFG_16				0x0110
#define CLK_CFG_16_SET				0x0114
#define CLK_CFG_16_CLR				0x0118
#define CLK_CFG_17				0x0120
#define CLK_CFG_17_SET				0x0124
#define CLK_CFG_17_CLR				0x0128
#define CLK_CFG_18				0x0130
#define CLK_CFG_18_SET				0x0134
#define CLK_CFG_18_CLR				0x0138
#define CLK_AUDDIV_0				0x0320
#define CKSYS2_CLK_CFG_0			0x0810
#define CKSYS2_CLK_CFG_0_SET			0x0814
#define CKSYS2_CLK_CFG_0_CLR			0x0818
#define CKSYS2_CLK_CFG_1			0x0820
#define CKSYS2_CLK_CFG_1_SET			0x0824
#define CKSYS2_CLK_CFG_1_CLR			0x0828
#define CKSYS2_CLK_CFG_2			0x0830
#define CKSYS2_CLK_CFG_2_SET			0x0834
#define CKSYS2_CLK_CFG_2_CLR			0x0838
#define CKSYS2_CLK_CFG_3			0x0840
#define CKSYS2_CLK_CFG_3_SET			0x0844
#define CKSYS2_CLK_CFG_3_CLR			0x0848
#define CKSYS2_CLK_CFG_4			0x0850
#define CKSYS2_CLK_CFG_4_SET			0x0854
#define CKSYS2_CLK_CFG_4_CLR			0x0858
#define CKSYS2_CLK_CFG_5			0x0860
#define CKSYS2_CLK_CFG_5_SET			0x0864
#define CKSYS2_CLK_CFG_5_CLR			0x0868
#define CKSYS2_CLK_CFG_6			0x0870
#define CKSYS2_CLK_CFG_6_SET			0x0874
#define CKSYS2_CLK_CFG_6_CLR			0x0878
#define CKSYS2_CLK_CFG_7			0x0880
#define CKSYS2_CLK_CFG_7_SET			0x0884
#define CKSYS2_CLK_CFG_7_CLR			0x0888
#define VLP_CLK_CFG_0				0x0010
#define VLP_CLK_CFG_0_SET				0x0014
#define VLP_CLK_CFG_0_CLR				0x0018
#define VLP_CLK_CFG_1				0x0020
#define VLP_CLK_CFG_1_SET				0x0024
#define VLP_CLK_CFG_1_CLR				0x0028
#define VLP_CLK_CFG_2				0x0030
#define VLP_CLK_CFG_2_SET				0x0034
#define VLP_CLK_CFG_2_CLR				0x0038
#define VLP_CLK_CFG_3				0x0040
#define VLP_CLK_CFG_3_SET				0x0044
#define VLP_CLK_CFG_3_CLR				0x0048
#define VLP_CLK_CFG_4				0x0050
#define VLP_CLK_CFG_4_SET				0x0054
#define VLP_CLK_CFG_4_CLR				0x0058
#define VLP_CLK_CFG_5				0x0060
#define VLP_CLK_CFG_5_SET				0x0064
#define VLP_CLK_CFG_5_CLR				0x0068

/* TOPCK MUX SHIFT */
#define TOP_MUX_AXI_SHIFT			0
#define TOP_MUX_PERI_FAXI_SHIFT			1
#define TOP_MUX_UFS_FAXI_SHIFT			2
#define TOP_MUX_PEXTP_FAXI_SHIFT		3
#define TOP_MUX_BUS_AXIMEM_SHIFT		4
#define TOP_MUX_MEM_SUB_SHIFT			5
#define TOP_MUX_PERI_FMEM_SUB_SHIFT		6
#define TOP_MUX_UFS_FMEM_SUB_SHIFT		7
#define TOP_MUX_PEXTP_FMEM_SUB_SHIFT		8
#define TOP_MUX_EMI_N_SHIFT			9
#define TOP_MUX_EMI_S_SHIFT			10
#define TOP_MUX_EMI_SLICE_N_SHIFT		11
#define TOP_MUX_EMI_SLICE_S_SHIFT		12
#define TOP_MUX_AP2CONN_HOST_SHIFT		13
#define TOP_MUX_ATB_SHIFT			14
#define TOP_MUX_CIRQ_SHIFT			15
#define TOP_MUX_EFUSE_SHIFT			16
#define TOP_MUX_MCU_L3GIC_SHIFT			17
#define TOP_MUX_MCU_INFRA_SHIFT			18
#define TOP_MUX_DSP_SHIFT			20
#define TOP_MUX_MFG_REF_SHIFT			21
#define TOP_MUX_MFG_EB_SHIFT			23
#define TOP_MUX_UART_SHIFT			24
#define TOP_MUX_SPI0_BCLK_SHIFT			25
#define TOP_MUX_SPI1_BCLK_SHIFT			26
#define TOP_MUX_SPI2_BCLK_SHIFT			27
#define TOP_MUX_SPI3_BCLK_SHIFT			28
#define TOP_MUX_SPI4_BCLK_SHIFT			29
#define TOP_MUX_SPI5_BCLK_SHIFT			30
#define TOP_MUX_SPI6_BCLK_SHIFT			0
#define TOP_MUX_SPI7_BCLK_SHIFT			1
#define TOP_MUX_MSDC_MACRO_1P_SHIFT		2
#define TOP_MUX_MSDC_MACRO_2P_SHIFT		3
#define TOP_MUX_MSDC30_1_SHIFT			4
#define TOP_MUX_MSDC30_2_SHIFT			5
#define TOP_MUX_AUD_INTBUS_SHIFT		6
#define TOP_MUX_DISP_PWM_SHIFT			7
#define TOP_MUX_USB_TOP_SHIFT			8
#define TOP_MUX_SSUSB_XHCI_SHIFT		9
#define TOP_MUX_USB_TOP_1P_SHIFT		10
#define TOP_MUX_SSUSB_XHCI_1P_SHIFT		11
#define TOP_MUX_I2C_SHIFT			12
#define TOP_MUX_AUD_ENGEN1_SHIFT		13
#define TOP_MUX_AUD_ENGEN2_SHIFT		14
#define TOP_MUX_AES_UFSFDE_SHIFT		15
#define TOP_MUX_UFS_SHIFT			16
#define TOP_MUX_UFS_MBIST_SHIFT			17
#define TOP_MUX_PEXTP_MBIST_SHIFT		18
#define TOP_MUX_AUD_1_SHIFT			19
#define TOP_MUX_AUD_2_SHIFT			20
#define TOP_MUX_AUDIO_H_SHIFT			21
#define TOP_MUX_ADSP_SHIFT			22
#define TOP_MUX_ADSP_UARTHUB_BCLK_SHIFT		23
#define TOP_MUX_DPMAIF_MAIN_SHIFT		24
#define TOP_MUX_PWM_SHIFT			25
#define TOP_MUX_MCUPM_SHIFT			26
#define TOP_MUX_SFLASH_SHIFT			27
#define TOP_MUX_IPSEAST_SHIFT			28
#define TOP_MUX_IPSWEST_SHIFT			29
#define TOP_MUX_TL_SHIFT			30
#define TOP_MUX_EMI_INTERFACE_546_SHIFT		0
#define TOP_MUX_SDF_SHIFT			1
#define TOP_MUX_UARTHUB_BCLK_SHIFT		2
#define TOP_MUX_SSR_PKA_SHIFT			3
#define TOP_MUX_SSR_DMA_SHIFT			4
#define TOP_MUX_SSR_KDF_SHIFT			5
#define TOP_MUX_SSR_RNG_SHIFT			6
#define TOP_MUX_SSR_SEJ_SHIFT			7
#define TOP_MUX_DXCC_SHIFT			10
#define TOP_MUX_DPSW_CMP_26M_SHIFT		11
#define TOP_MUX_CAMTG1_SHIFT			0
#define TOP_MUX_CAMTG2_SHIFT			1
#define TOP_MUX_CAMTG3_SHIFT			2
#define TOP_MUX_CAMTG4_SHIFT			3
#define TOP_MUX_CAMTG5_SHIFT			4
#define TOP_MUX_CAMTG6_SHIFT			5
#define TOP_MUX_CAMTG7_SHIFT			6
#define TOP_MUX_SENINF0_SHIFT			7
#define TOP_MUX_SENINF1_SHIFT			8
#define TOP_MUX_SENINF2_SHIFT			9
#define TOP_MUX_SENINF3_SHIFT			10
#define TOP_MUX_SENINF4_SHIFT			11
#define TOP_MUX_SENINF5_SHIFT			12
#define TOP_MUX_CCU_AHB_SHIFT			13
#define TOP_MUX_IMG1_SHIFT			14
#define TOP_MUX_IPE_SHIFT			15
#define TOP_MUX_CAM_SHIFT			16
#define TOP_MUX_CAMTM_SHIFT			17
#define TOP_MUX_DPE_SHIFT			18
#define TOP_MUX_VDEC_SHIFT			19
#define TOP_MUX_CCUSYS_SHIFT			20
#define TOP_MUX_CCUTM_SHIFT			21
#define TOP_MUX_VENC_SHIFT			22
#define TOP_MUX_DP_CORE_SHIFT			23
#define TOP_MUX_DP_SHIFT			24
#define TOP_MUX_DISP_SHIFT			25
#define TOP_MUX_MDP_SHIFT			26
#define TOP_MUX_MMINFRA_SHIFT			27
#define TOP_MUX_MMUP_SHIFT			28
#define TOP_MUX_SCP_SHIFT			0
#define TOP_MUX_SCP_SPI_SHIFT			1
#define TOP_MUX_SCP_IIC_SHIFT			2
#define TOP_MUX_SCP_SPI_HIGH_SPD_SHIFT		3
#define TOP_MUX_SCP_IIC_HIGH_SPD_SHIFT		4
#define TOP_MUX_PWRAP_ULPOSC_SHIFT		5
#define TOP_MUX_SPMI_M_TIA_32K_SHIFT		6
#define TOP_MUX_APXGPT_26M_BCLK_SHIFT		7
#define TOP_MUX_DPSW_SHIFT			8
#define TOP_MUX_SPMI_M_MST_SHIFT		9
#define TOP_MUX_DVFSRC_SHIFT			10
#define TOP_MUX_PWM_VLP_SHIFT			11
#define TOP_MUX_AXI_VLP_SHIFT			12
#define TOP_MUX_SYSTIMER_26M_SHIFT		13
#define TOP_MUX_SSPM_SHIFT			14
#define TOP_MUX_SRCK_SHIFT			15
#define TOP_MUX_CAMTG0_SHIFT			17
#define TOP_MUX_IPS_SHIFT			18
#define TOP_MUX_SSPM_26M_SHIFT			19
#define TOP_MUX_ULPOSC_SSPM_SHIFT		20

/* TOPCK CKSTA REG */
#define CKSTA_REG				0x0230
#define CKSTA_REG1				0x0234
#define CKSTA_REG2				0x0238
#define CKSYS2_CKSTA_REG			0x0A30

/* TOPCK DIVIDER REG */
#define CLK_AUDDIV_2				0x0328
#define CLK_AUDDIV_3				0x0334
#define CLK_AUDDIV_4				0x0338
#define CLK_AUDDIV_5				0x033C

/* APMIXED PLL REG */
#define AP_PLL_CON3				0x00C
#define APLL1_TUNER_CON0			0x040
#define APLL2_TUNER_CON0			0x044
#define MAINPLL_CON0				0x350
#define MAINPLL_CON1				0x354
#define MAINPLL_CON2				0x358
#define MAINPLL_CON3				0x35C
#define UNIVPLL_CON0				0x308
#define UNIVPLL_CON1				0x30C
#define UNIVPLL_CON2				0x310
#define UNIVPLL_CON3				0x314
#define MSDCPLL_CON0				0x360
#define MSDCPLL_CON1				0x364
#define MSDCPLL_CON2				0x368
#define MSDCPLL_CON3				0x36C
#define MMPLL_CON0				0x3A0
#define MMPLL_CON1				0x3A4
#define MMPLL_CON2				0x3A8
#define MMPLL_CON3				0x3AC
#define ADSPPLL_CON0				0x380
#define ADSPPLL_CON1				0x384
#define ADSPPLL_CON2				0x388
#define ADSPPLL_CON3				0x38C
#define APLL1_CON0				0x328
#define APLL1_CON1				0x32C
#define APLL1_CON2				0x330
#define APLL1_CON3				0x334
#define APLL1_CON4				0x338
#define APLL2_CON0				0x33C
#define APLL2_CON1				0x340
#define APLL2_CON2				0x344
#define APLL2_CON3				0x348
#define APLL2_CON4				0x34C
#define EMIPLL_CON0				0x3B0
#define EMIPLL_CON1				0x3B4
#define EMIPLL_CON2				0x3B8
#define EMIPLL_CON3				0x3BC
#define EMIPLL2_CON0				0x3C0
#define EMIPLL2_CON1				0x3C4
#define EMIPLL2_CON2				0x3C8
#define EMIPLL2_CON3				0x3CC
#define MAINPLL2_CON0				0x280
#define MAINPLL2_CON1				0x284
#define MAINPLL2_CON2				0x288
#define MAINPLL2_CON3				0x28C
#define UNIVPLL2_CON0				0x290
#define UNIVPLL2_CON1				0x294
#define UNIVPLL2_CON2				0x298
#define UNIVPLL2_CON3				0x29C
#define MMPLL2_CON0				0x2A0
#define MMPLL2_CON1				0x2A4
#define MMPLL2_CON2				0x2A8
#define MMPLL2_CON3				0x2AC
#define TVDPLL_CON0				0x248
#define TVDPLL_CON1				0x24C
#define TVDPLL_CON2				0x250
#define TVDPLL_CON3				0x254
#define IMGPLL_CON0				0x370
#define IMGPLL_CON1				0x374
#define IMGPLL_CON2				0x378
#define IMGPLL_CON3				0x37C
#define MFGPLL_CON0				0x008
#define MFGPLL_CON1				0x00C
#define MFGPLL_CON2				0x010
#define MFGPLL_CON3				0x014
#define MFGPLL_SC0_CON0				0x008
#define MFGPLL_SC0_CON1				0x00C
#define MFGPLL_SC0_CON2				0x010
#define MFGPLL_SC0_CON3				0x014
#define MFGPLL_SC1_CON0				0x008
#define MFGPLL_SC1_CON1				0x00C
#define MFGPLL_SC1_CON2				0x010
#define MFGPLL_SC1_CON3				0x014
#define CCIPLL_CON0				0x008
#define CCIPLL_CON1				0x00C
#define CCIPLL_CON2				0x010
#define CCIPLL_CON3				0x014
#define ARMPLL_LL_CON0				0x008
#define ARMPLL_LL_CON1				0x00C
#define ARMPLL_LL_CON2				0x010
#define ARMPLL_LL_CON3				0x014
#define ARMPLL_BL_CON0				0x008
#define ARMPLL_BL_CON1				0x00C
#define ARMPLL_BL_CON2				0x010
#define ARMPLL_BL_CON3				0x014
#define ARMPLL_B_CON0				0x008
#define ARMPLL_B_CON1				0x00C
#define ARMPLL_B_CON2				0x010
#define ARMPLL_B_CON3				0x014
#define PTPPLL_CON0				0x008
#define PTPPLL_CON1				0x00C
#define PTPPLL_CON2				0x010
#define PTPPLL_CON3				0x014

/* HW Voter REG */
#define HWV_CG_0_SET				0x0000
#define HWV_CG_0_CLR				0x0004
#define HWV_CG_0_DONE				0x1C00
#define HWV_CG_1_SET				0x0008
#define HWV_CG_1_CLR				0x000C
#define HWV_CG_1_DONE				0x1C04
#define HWV_CG_2_SET				0x0010
#define HWV_CG_2_CLR				0x0014
#define HWV_CG_2_DONE				0x1C08
#define HWV_CG_4_SET				0x0020
#define HWV_CG_4_CLR				0x0024
#define HWV_CG_4_DONE				0x1C10
#define HWV_CG_5_SET				0x0028
#define HWV_CG_5_CLR				0x002C
#define HWV_CG_5_DONE				0x1C14
#define HWV_CG_6_SET				0x0030
#define HWV_CG_6_CLR				0x0034
#define HWV_CG_6_DONE				0x1C18
#define HWV_CG_7_SET				0x0038
#define HWV_CG_7_CLR				0x003C
#define HWV_CG_7_DONE				0x1C1C
#define HWV_CG_8_SET				0x0040
#define HWV_CG_8_CLR				0x0044
#define HWV_CG_8_DONE				0x1C20
#define HWV_CG_9_SET				0x0048
#define HWV_CG_9_CLR				0x004C
#define HWV_CG_9_DONE				0x1C24
#define HWV_CG_10_SET				0x0050
#define HWV_CG_10_CLR				0x0054
#define HWV_CG_10_DONE				0x1C28
#define HWV_CG_11_SET				0x0058
#define HWV_CG_11_CLR				0x005C
#define HWV_CG_11_DONE				0x1C2C
#define HWV_CG_12_SET				0x0060
#define HWV_CG_12_CLR				0x0064
#define HWV_CG_12_DONE				0x1C30
#define HWV_CG_13_SET				0x0068
#define HWV_CG_13_CLR				0x006C
#define HWV_CG_13_DONE				0x1C34
#define HWV_CG_14_SET				0x0070
#define HWV_CG_14_CLR				0x0074
#define HWV_CG_14_DONE				0x1C38
#define HWV_CG_15_SET				0x0078
#define HWV_CG_15_CLR				0x007C
#define HWV_CG_15_DONE				0x1C3C
#define HWV_CG_16_SET				0x0080
#define HWV_CG_16_CLR				0x0084
#define HWV_CG_16_DONE				0x1C40
#define HWV_CG_17_SET				0x0088
#define HWV_CG_17_CLR				0x008C
#define HWV_CG_17_DONE				0x1C44
#define HWV_CG_18_SET				0x0090
#define HWV_CG_18_CLR				0x0094
#define HWV_CG_18_DONE				0x1C48
#define MM_HW_CCF_HW_CCF_0_SET			0x0000
#define MM_HW_CCF_HW_CCF_0_CLR			0x0004
#define MM_HW_CCF_HW_CCF_0_DONE			0x1C00
#define MM_HW_CCF_HW_CCF_1_SET			0x0008
#define MM_HW_CCF_HW_CCF_1_CLR			0x000C
#define MM_HW_CCF_HW_CCF_1_DONE			0x1C04
#define MM_HW_CCF_HW_CCF_2_SET			0x0010
#define MM_HW_CCF_HW_CCF_2_CLR			0x0014
#define MM_HW_CCF_HW_CCF_2_DONE			0x1C08
#define MM_HW_CCF_HW_CCF_3_SET			0x0018
#define MM_HW_CCF_HW_CCF_3_CLR			0x001C
#define MM_HW_CCF_HW_CCF_3_DONE			0x1C0C
#define MM_HW_CCF_HW_CCF_4_SET			0x0020
#define MM_HW_CCF_HW_CCF_4_CLR			0x0024
#define MM_HW_CCF_HW_CCF_4_DONE			0x1C10
#define MM_HW_CCF_HW_CCF_5_SET			0x0028
#define MM_HW_CCF_HW_CCF_5_CLR			0x002C
#define MM_HW_CCF_HW_CCF_5_DONE			0x1C14
#define MM_HW_CCF_HW_CCF_6_SET			0x0030
#define MM_HW_CCF_HW_CCF_6_CLR			0x0034
#define MM_HW_CCF_HW_CCF_6_DONE			0x1C18
#define MM_HW_CCF_HW_CCF_7_SET			0x0038
#define MM_HW_CCF_HW_CCF_7_CLR			0x003C
#define MM_HW_CCF_HW_CCF_7_DONE			0x1C1C
#define MM_HW_CCF_HW_CCF_8_SET			0x0040
#define MM_HW_CCF_HW_CCF_8_CLR			0x0044
#define MM_HW_CCF_HW_CCF_8_DONE			0x1C20
#define MM_HW_CCF_HW_CCF_9_SET			0x0048
#define MM_HW_CCF_HW_CCF_9_CLR			0x004C
#define MM_HW_CCF_HW_CCF_9_DONE			0x1C24
#define MM_HW_CCF_HW_CCF_10_SET			0x0050
#define MM_HW_CCF_HW_CCF_10_CLR			0x0054
#define MM_HW_CCF_HW_CCF_10_DONE		0x1C28
#define MM_HW_CCF_HW_CCF_11_SET			0x0058
#define MM_HW_CCF_HW_CCF_11_CLR			0x005C
#define MM_HW_CCF_HW_CCF_11_DONE		0x1C2C
#define MM_HW_CCF_HW_CCF_12_SET			0x0060
#define MM_HW_CCF_HW_CCF_12_CLR			0x0064
#define MM_HW_CCF_HW_CCF_12_DONE		0x1C30
#define MM_HW_CCF_HW_CCF_13_SET			0x0068
#define MM_HW_CCF_HW_CCF_13_CLR			0x006C
#define MM_HW_CCF_HW_CCF_13_DONE		0x1C34
#define MM_HW_CCF_HW_CCF_14_SET			0x0070
#define MM_HW_CCF_HW_CCF_14_CLR			0x0074
#define MM_HW_CCF_HW_CCF_14_DONE		0x1C38
#define MM_HW_CCF_HW_CCF_15_SET			0x0078
#define MM_HW_CCF_HW_CCF_15_CLR			0x007C
#define MM_HW_CCF_HW_CCF_15_DONE		0x1C3C
#define MM_HW_CCF_HW_CCF_16_SET			0x0080
#define MM_HW_CCF_HW_CCF_16_CLR			0x0084
#define MM_HW_CCF_HW_CCF_16_DONE		0x1C40
#define MM_HW_CCF_HW_CCF_17_SET			0x0088
#define MM_HW_CCF_HW_CCF_17_CLR			0x008C
#define MM_HW_CCF_HW_CCF_17_DONE		0x1C44
#define MM_HW_CCF_HW_CCF_18_SET			0x0090
#define MM_HW_CCF_HW_CCF_18_CLR			0x0094
#define MM_HW_CCF_HW_CCF_18_DONE		0x1C48
#define MM_HW_CCF_HW_CCF_19_SET			0x0098
#define MM_HW_CCF_HW_CCF_19_CLR			0x009C
#define MM_HW_CCF_HW_CCF_19_DONE		0x1C4C
#define MM_HW_CCF_HW_CCF_20_SET			0x00A0
#define MM_HW_CCF_HW_CCF_20_CLR			0x00A4
#define MM_HW_CCF_HW_CCF_20_DONE		0x1C50
#define MM_HW_CCF_HW_CCF_21_SET			0x00A8
#define MM_HW_CCF_HW_CCF_21_CLR			0x00AC
#define MM_HW_CCF_HW_CCF_21_DONE		0x1C54
#define MM_HW_CCF_HW_CCF_22_SET			0x00B0
#define MM_HW_CCF_HW_CCF_22_CLR			0x00B4
#define MM_HW_CCF_HW_CCF_22_DONE		0x1C58
#define MM_HW_CCF_HW_CCF_23_SET			0x00B8
#define MM_HW_CCF_HW_CCF_23_CLR			0x00BC
#define MM_HW_CCF_HW_CCF_23_DONE		0x1C5C
#define MM_HW_CCF_HW_CCF_24_SET			0x00C0
#define MM_HW_CCF_HW_CCF_24_CLR			0x00C4
#define MM_HW_CCF_HW_CCF_24_DONE		0x1C60
#define MM_HW_CCF_HW_CCF_25_SET			0x00C8
#define MM_HW_CCF_HW_CCF_25_CLR			0x00CC
#define MM_HW_CCF_HW_CCF_25_DONE		0x1C64
#define MM_HW_CCF_HW_CCF_26_SET			0x00D0
#define MM_HW_CCF_HW_CCF_26_CLR			0x00D4
#define MM_HW_CCF_HW_CCF_26_DONE		0x1C68
#define MM_HW_CCF_HW_CCF_27_SET			0x00D8
#define MM_HW_CCF_HW_CCF_27_CLR			0x00DC
#define MM_HW_CCF_HW_CCF_27_DONE		0x1C6C
#define MM_HW_CCF_HW_CCF_28_SET			0x00E0
#define MM_HW_CCF_HW_CCF_28_CLR			0x00E4
#define MM_HW_CCF_HW_CCF_28_DONE		0x1C70
#define MM_HW_CCF_HW_CCF_30_SET			0x00F0
#define MM_HW_CCF_HW_CCF_30_CLR			0x00F4
#define MM_HW_CCF_HW_CCF_30_DONE		0x1C78
#define MM_HW_CCF_HW_CCF_31_SET			0x00F8
#define MM_HW_CCF_HW_CCF_31_CLR			0x00FC
#define MM_HW_CCF_HW_CCF_31_DONE		0x1C7C
#define MM_HW_CCF_HW_CCF_32_SET			0x0100
#define MM_HW_CCF_HW_CCF_32_CLR			0x0104
#define MM_HW_CCF_HW_CCF_32_DONE		0x1C80
#define MM_HW_CCF_HW_CCF_33_SET			0x0108
#define MM_HW_CCF_HW_CCF_33_CLR			0x010C
#define MM_HW_CCF_HW_CCF_33_DONE		0x1C84
#define HWV_B0_SET				0x01A0
#define HWV_B0_CLR				0x01A4
#define HWV_B0_SET_STA				0x1474
#define HWV_B0_CLR_STA				0x1478
#define HWV_B0_DONE				0x142C
#define CLK_MMINFRA_PWR_VOTE_BIT		(31)

static DEFINE_SPINLOCK(mt6989_clk_lock);

static const struct mtk_fixed_factor vlp_ck_divs[] = {
	FACTOR(CLK_VLP_CK_LP_REF, "vlp_lp_ref_ck",
			"tck_26m_mx9_ck", 1, 1),
};

static const struct mtk_fixed_factor top_divs[] = {
	FACTOR(CLK_TOP_MAINPLL_D3, "mainpll_d3",
			"mainpll", 1, 3),
	FACTOR(CLK_TOP_MAINPLL_D4, "mainpll_d4",
			"mainpll", 1, 4),
	FACTOR(CLK_TOP_MAINPLL_D4_D2, "mainpll_d4_d2",
			"mainpll", 1, 8),
	FACTOR(CLK_TOP_MAINPLL_D4_D4, "mainpll_d4_d4",
			"mainpll", 1, 16),
	FACTOR(CLK_TOP_MAINPLL_D4_D8, "mainpll_d4_d8",
			"mainpll", 1, 32),
	FACTOR(CLK_TOP_MAINPLL_D5, "mainpll_d5",
			"mainpll", 1, 5),
	FACTOR(CLK_TOP_MAINPLL_D5_D2, "mainpll_d5_d2",
			"mainpll", 1, 10),
	FACTOR(CLK_TOP_MAINPLL_D5_D4, "mainpll_d5_d4",
			"mainpll", 1, 20),
	FACTOR(CLK_TOP_MAINPLL_D5_D8, "mainpll_d5_d8",
			"mainpll", 1, 40),
	FACTOR(CLK_TOP_MAINPLL_D6, "mainpll_d6",
			"mainpll", 1, 6),
	FACTOR(CLK_TOP_MAINPLL_D6_D2, "mainpll_d6_d2",
			"mainpll", 1, 12),
	FACTOR(CLK_TOP_MAINPLL_D7, "mainpll_d7",
			"mainpll", 1, 7),
	FACTOR(CLK_TOP_MAINPLL_D7_D2, "mainpll_d7_d2",
			"mainpll", 1, 14),
	FACTOR(CLK_TOP_MAINPLL_D7_D4, "mainpll_d7_d4",
			"mainpll", 1, 28),
	FACTOR(CLK_TOP_MAINPLL_D7_D8, "mainpll_d7_d8",
			"mainpll", 1, 56),
	FACTOR(CLK_TOP_UNIVPLL_D4, "univpll_d4",
			"univpll", 1, 4),
	FACTOR(CLK_TOP_UNIVPLL_D4_D2, "univpll_d4_d2",
			"univpll", 1, 8),
	FACTOR(CLK_TOP_UNIVPLL_D4_D4, "univpll_d4_d4",
			"univpll", 1, 16),
	FACTOR(CLK_TOP_UNIVPLL_D4_D8, "univpll_d4_d8",
			"univpll", 1, 32),
	FACTOR(CLK_TOP_UNIVPLL_D5, "univpll_d5",
			"univpll", 1, 5),
	FACTOR(CLK_TOP_UNIVPLL_D5_D2, "univpll_d5_d2",
			"univpll", 1, 10),
	FACTOR(CLK_TOP_UNIVPLL_D5_D4, "univpll_d5_d4",
			"univpll", 1, 20),
	FACTOR(CLK_TOP_UNIVPLL_D6, "univpll_d6",
			"univpll", 1, 6),
	FACTOR(CLK_TOP_UNIVPLL_D6_D2, "univpll_d6_d2",
			"univpll", 1, 12),
	FACTOR(CLK_TOP_UNIVPLL_D6_D4, "univpll_d6_d4",
			"univpll", 1, 24),
	FACTOR(CLK_TOP_UNIVPLL_D6_D8, "univpll_d6_d8",
			"univpll", 1, 48),
	FACTOR(CLK_TOP_UNIVPLL_D6_D16, "univpll_d6_d16",
			"univpll", 1, 96),
	FACTOR(CLK_TOP_UNIVPLL_192M, "univpll_192m_ck",
			"univpll", 1, 13),
	FACTOR(CLK_TOP_UNIVPLL_192M_D4, "univpll_192m_d4",
			"univpll", 1, 52),
	FACTOR(CLK_TOP_UNIVPLL_192M_D8, "univpll_192m_d8",
			"univpll", 1, 104),
	FACTOR(CLK_TOP_UNIVPLL_192M_D10, "univpll_192m_d10",
			"univpll", 1, 130),
	FACTOR(CLK_TOP_UNIVPLL_192M_D16, "univpll_192m_d16",
			"univpll", 1, 208),
	FACTOR(CLK_TOP_UNIVPLL_192M_D32, "univpll_192m_d32",
			"univpll", 1, 416),
	FACTOR(CLK_TOP_APLL1, "apll1_ck",
			"apll1", 1, 1),
	FACTOR(CLK_TOP_APLL1_D4, "apll1_d4",
			"apll1", 1, 4),
	FACTOR(CLK_TOP_APLL1_D8, "apll1_d8",
			"apll1", 1, 8),
	FACTOR(CLK_TOP_APLL2, "apll2_ck",
			"apll2", 1, 1),
	FACTOR(CLK_TOP_APLL2_D4, "apll2_d4",
			"apll2", 1, 4),
	FACTOR(CLK_TOP_APLL2_D8, "apll2_d8",
			"apll2", 1, 8),
	FACTOR(CLK_TOP_ADSPPLL, "adsppll_ck",
			"adsppll", 1, 1),
	FACTOR(CLK_TOP_EMIPLL, "emipll_ck",
			"emipll", 1, 1),
	FACTOR(CLK_TOP_EMIPLL2, "emipll2_ck",
			"emipll2", 1, 1),
	FACTOR(CLK_TOP_MMPLL_D6, "mmpll_d6",
			"mmpll", 1, 6),
	FACTOR(CLK_TOP_TVDPLL, "tvdpll_ck",
			"tvdpll", 1, 1),
	FACTOR(CLK_TOP_TVDPLL_D2, "tvdpll_d2",
			"tvdpll", 1, 2),
	FACTOR(CLK_TOP_TVDPLL_D4, "tvdpll_d4",
			"tvdpll", 1, 4),
	FACTOR(CLK_TOP_TVDPLL_D8, "tvdpll_d8",
			"tvdpll", 1, 8),
	FACTOR(CLK_TOP_TVDPLL_D16, "tvdpll_d16",
			"tvdpll", 1, 16),
	FACTOR(CLK_TOP_MSDCPLL, "msdcpll_ck",
			"msdcpll", 1, 1),
	FACTOR(CLK_TOP_MSDCPLL_D2, "msdcpll_d2",
			"msdcpll", 1, 2),
	FACTOR(CLK_TOP_CLKRTC, "clkrtc",
			"clk32k", 1, 1),
	FACTOR(CLK_TOP_TCK_26M_MX9, "tck_26m_mx9_ck",
			"clk26m", 1, 1),
	FACTOR(CLK_TOP_F26M_CK_D2, "f26m_d2",
			"clk13m", 1, 1),
	FACTOR(CLK_TOP_ULPOSC, "ulposc_ck",
			"ulposc", 1, 1),
	FACTOR(CLK_TOP_OSC_D2, "osc_d2",
			"ulposc", 1, 2),
	FACTOR(CLK_TOP_OSC_D3, "osc_d3",
			"ulposc", 1, 3),
	FACTOR(CLK_TOP_OSC_D4, "osc_d4",
			"ulposc", 1, 4),
	FACTOR(CLK_TOP_OSC_D5, "osc_d5",
			"ulposc", 1, 5),
	FACTOR(CLK_TOP_OSC_D6, "osc_d6",
			"ulposc", 1, 6),
	FACTOR(CLK_TOP_OSC_D7, "osc_d7",
			"ulposc", 1, 7),
	FACTOR(CLK_TOP_OSC_D8, "osc_d8",
			"ulposc", 1, 8),
	FACTOR(CLK_TOP_OSC_D10, "osc_d10",
			"ulposc", 1, 10),
	FACTOR(CLK_TOP_OSC_D16, "osc_d16",
			"ulposc", 1, 16),
	FACTOR(CLK_TOP_OSC_D20, "osc_d20",
			"ulposc", 1, 20),
	FACTOR(CLK_TOP_OSC_D32, "osc_d32",
			"ulposc", 1, 32),
	FACTOR(CLK_TOP_OSC_D40, "osc_d40",
			"ulposc", 1, 40),
	FACTOR(CLK_TOP_MAINPLL2_D3, "mainpll2_d3",
			"mainpll2", 1, 3),
	FACTOR(CLK_TOP_MAINPLL2_D4, "mainpll2_d4",
			"mainpll2", 1, 4),
	FACTOR(CLK_TOP_MAINPLL2_D4_D2, "mainpll2_d4_d2",
			"mainpll2", 1, 8),
	FACTOR(CLK_TOP_MAINPLL2_D4_D4, "mainpll2_d4_d4",
			"mainpll2", 1, 16),
	FACTOR(CLK_TOP_MAINPLL2_D5, "mainpll2_d5",
			"mainpll2", 1, 5),
	FACTOR(CLK_TOP_MAINPLL2_D5_D2, "mainpll2_d5_d2",
			"mainpll2", 1, 10),
	FACTOR(CLK_TOP_MAINPLL2_D5_D4, "mainpll2_d5_d4",
			"mainpll2", 1, 20),
	FACTOR(CLK_TOP_MAINPLL2_D6, "mainpll2_d6",
			"mainpll2", 1, 6),
	FACTOR(CLK_TOP_MAINPLL2_D6_D2, "mainpll2_d6_d2",
			"mainpll2", 1, 12),
	FACTOR(CLK_TOP_MAINPLL2_D7, "mainpll2_d7",
			"mainpll2", 1, 7),
	FACTOR(CLK_TOP_MAINPLL2_D7_D2, "mainpll2_d7_d2",
			"mainpll2", 1, 14),
	FACTOR(CLK_TOP_MAINPLL2_D9, "mainpll2_d9",
			"mainpll2", 1, 9),
	FACTOR(CLK_TOP_UNIVPLL2_D3, "univpll2_d3",
			"univpll2", 1, 3),
	FACTOR(CLK_TOP_UNIVPLL2_D4, "univpll2_d4",
			"univpll2", 1, 4),
	FACTOR(CLK_TOP_UNIVPLL2_D4_D2, "univpll2_d4_d2",
			"univpll2", 1, 8),
	FACTOR(CLK_TOP_UNIVPLL2_D5, "univpll2_d5",
			"univpll2", 1, 5),
	FACTOR(CLK_TOP_UNIVPLL2_D5_D2, "univpll2_d5_d2",
			"univpll2", 1, 10),
	FACTOR(CLK_TOP_UNIVPLL2_D6, "univpll2_d6",
			"univpll2", 1, 6),
	FACTOR(CLK_TOP_UNIVPLL2_D6_D2, "univpll2_d6_d2",
			"univpll2", 1, 12),
	FACTOR(CLK_TOP_UNIVPLL2_D6_D4, "univpll2_d6_d4",
			"univpll2", 1, 24),
	FACTOR(CLK_TOP_UNIVPLL2_D6_D8, "univpll2_d6_d8",
			"univpll2", 1, 48),
	FACTOR(CLK_TOP_UNIVPLL2_D6_D16, "univpll2_d6_d16",
			"univpll2", 1, 96),
	FACTOR(CLK_TOP_UNIVPLL2_D7, "univpll2_d7",
			"univpll2", 1, 7),
	FACTOR(CLK_TOP_UNIVPLL2_192M_D8, "univpll2_192m_d8",
			"univpll2", 1, 104),
	FACTOR(CLK_TOP_UNIVPLL2_192M_D10, "univpll2_192m_d10",
			"univpll2", 1, 130),
	FACTOR(CLK_TOP_UNIVPLL2_192M_D16, "univpll2_192m_d16",
			"univpll2", 1, 208),
	FACTOR(CLK_TOP_UNIVPLL2_192M_D32, "univpll2_192m_d32",
			"univpll2", 1, 416),
	FACTOR(CLK_TOP_IMGPLL_D2, "imgpll_d2",
			"imgpll", 1, 2),
	FACTOR(CLK_TOP_IMGPLL_D4, "imgpll_d4",
			"imgpll", 1, 4),
	FACTOR(CLK_TOP_IMGPLL_D5_D2, "imgpll_d5_d2",
			"imgpll", 1, 10),
	FACTOR(CLK_TOP_MMPLL2_D4, "mmpll2_d4",
			"mmpll2", 1, 4),
	FACTOR(CLK_TOP_MMPLL2_D4_D2, "mmpll2_d4_d2",
			"mmpll2", 1, 8),
	FACTOR(CLK_TOP_MMPLL2_D5, "mmpll2_d5",
			"mmpll2", 1, 5),
	FACTOR(CLK_TOP_MMPLL2_D5_D2, "mmpll2_d5_d2",
			"mmpll2", 1, 10),
	FACTOR(CLK_TOP_MMPLL2_D6, "mmpll2_d6",
			"mmpll2", 1, 6),
	FACTOR(CLK_TOP_MMPLL2_D6_D2, "mmpll2_d6_d2",
			"mmpll2", 1, 12),
	FACTOR(CLK_TOP_MMPLL2_D7, "mmpll2_d7",
			"mmpll2", 1, 7),
	FACTOR(CLK_TOP_MMPLL2_D9, "mmpll2_d9",
			"mmpll2", 1, 9),
	FACTOR(CLK_TOP_F26M, "f26m_ck",
			"clk26m", 1, 1),
	FACTOR(CLK_TOP_P_FAXI, "peri_faxi_ck",
			"peri_faxi_sel", 1, 1),
	FACTOR(CLK_TOP_U_FAXI, "ufs_faxi_ck",
			"ufs_faxi_sel", 1, 1),
	FACTOR(CLK_TOP_PEXTP_FAXI, "pextp_faxi_ck",
			"pextp_faxi_sel", 1, 1),
	FACTOR(CLK_TOP_U_FMEM_SUB, "ufs_fmem_sub_ck",
			"ufs_fmem_sub_sel", 1, 1),
	FACTOR(CLK_TOP_PEXTP_FMEM_SUB, "pextp_fmem_sub_ck",
			"pextp_fmem_sub_sel", 1, 1),
	FACTOR(CLK_TOP_ATB, "atb_ck",
			"atb_sel", 1, 1),
	FACTOR(CLK_TOP_UART, "uart_ck",
			"uart_sel", 1, 1),
	FACTOR(CLK_TOP_SPI0_BCLK, "spi0_b_ck",
			"spi0_b_sel", 1, 1),
	FACTOR(CLK_TOP_SPI1_BCLK, "spi1_b_ck",
			"spi1_b_sel", 1, 1),
	FACTOR(CLK_TOP_SPI2_BCLK, "spi2_b_ck",
			"spi2_b_sel", 1, 1),
	FACTOR(CLK_TOP_SPI3_BCLK, "spi3_b_ck",
			"spi3_b_sel", 1, 1),
	FACTOR(CLK_TOP_SPI4_BCLK, "spi4_b_ck",
			"spi4_b_sel", 1, 1),
	FACTOR(CLK_TOP_SPI5_BCLK, "spi5_b_ck",
			"spi5_b_sel", 1, 1),
	FACTOR(CLK_TOP_SPI6_BCLK, "spi6_b_ck",
			"spi6_b_sel", 1, 1),
	FACTOR(CLK_TOP_SPI7_BCLK, "spi7_b_ck",
			"spi7_b_sel", 1, 1),
	FACTOR(CLK_TOP_MSDC30_1, "msdc30_1_ck",
			"msdc30_1_sel", 1, 1),
	FACTOR(CLK_TOP_MSDC30_2, "msdc30_2_ck",
			"msdc30_2_sel", 1, 1),
	FACTOR(CLK_TOP_AUD_INTBUS, "aud_intbus_ck",
			"aud_intbus_sel", 1, 1),
	FACTOR(CLK_TOP_DISP_PWM, "disp_pwm_ck",
			"disp_pwm_sel", 1, 1),
	FACTOR(CLK_TOP_I2C, "i2c_ck",
			"i2c_sel", 1, 1),
	FACTOR(CLK_TOP_AUD_ENGEN1, "aud_engen1_ck",
			"aud_engen1_sel", 1, 1),
	FACTOR(CLK_TOP_AUD_ENGEN2, "aud_engen2_ck",
			"aud_engen2_sel", 1, 1),
	FACTOR(CLK_TOP_AES_UFSFDE, "aes_ufsfde_ck",
			"aes_ufsfde_sel", 1, 1),
	FACTOR(CLK_TOP_UFS, "ufs_ck",
			"ufs_sel", 1, 1),
	FACTOR(CLK_TOP_PEXTP_MBIST, "pextp_mbist_ck",
			"pextp_mbist_sel", 1, 1),
	FACTOR(CLK_TOP_AUD_1, "aud_1_ck",
			"aud_1_sel", 1, 1),
	FACTOR(CLK_TOP_AUD_2, "aud_2_ck",
			"aud_2_sel", 1, 1),
	FACTOR(CLK_TOP_AUDIO_H, "audio_h_ck",
			"audio_h_sel", 1, 1),
	FACTOR(CLK_TOP_DPMAIF_MAIN, "dpmaif_main_ck",
			"dpmaif_main_sel", 1, 1),
	FACTOR(CLK_TOP_PWM, "pwm_ck",
			"pwm_sel", 1, 1),
	FACTOR(CLK_TOP_SFLASH, "sflash_ck",
			"sflash_sel", 1, 1),
	FACTOR(CLK_TOP_SDF, "sdf_ck",
			"sdf_sel", 1, 1),
	FACTOR(CLK_TOP_USB_FMCNT, "ssusb_fmcnt_ck",
			"univpll_192m_d4", 1, 1),
	FACTOR(CLK_TOP_USB_FMCNT_P1, "ssusb_fmcnt_p1_ck",
			"univpll_192m_d4", 1, 1),
	FACTOR(CLK_TOP_AVS_IMG, "avs_img_ck",
			"tck_26m_mx9_ck", 1, 1),
	FACTOR(CLK_TOP_CCU_AHB, "ccu_ahb_ck",
			"ccu_ahb_sel", 1, 1),
	FACTOR(CLK_TOP_CCUSYS, "ccusys_ck",
			"ccusys_sel", 1, 1),
	FACTOR(CLK_TOP_VENC, "venc_ck",
			"venc_sel", 1, 1),
	FACTOR(CLK_TOP_MMINFRA, "mminfra_ck",
			"mminfra_sel", 1, 1),
	FACTOR(CLK_TOP_IMG1, "img1_ck",
			"img1_sel", 1, 1),
	FACTOR(CLK_TOP_IPE, "ipe_ck",
			"ipe_sel", 1, 1),
	FACTOR(CLK_TOP_CAM, "cam_ck",
			"cam_sel", 1, 1),
	FACTOR(CLK_TOP_CAMTM, "camtm_ck",
			"camtm_sel", 1, 1),
	FACTOR(CLK_TOP_VDEC, "vdec_ck",
			"vdec_sel", 1, 1),
	FACTOR(CLK_TOP_DISP, "disp_ck",
			"disp_sel", 1, 1),
	FACTOR(CLK_TOP_MDP, "mdp_ck",
			"mdp_sel", 1, 1),
	FACTOR(CLK_TOP_AVS_VDEC, "avs_vdec_ck",
			"tck_26m_mx9_ck", 1, 1),
};

static const char * const vlp_scp_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d20",
	"mainpll_d6",
	"mainpll_d4",
	"mainpll_d3",
	"apll1_ck"
};

static const char * const vlp_scp_spi_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d20",
	"mainpll_d5_d4",
	"mainpll_d7_d2"
};

static const char * const vlp_scp_iic_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d20",
	"mainpll_d5_d4",
	"mainpll_d7_d2"
};

static const char * const vlp_scp_spi_hs_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d20",
	"mainpll_d5_d4",
	"mainpll_d7_d2",
	"mainpll_d7"
};

static const char * const vlp_scp_iic_hs_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d20",
	"mainpll_d5_d4",
	"mainpll_d7_d2",
	"mainpll_d7"
};

static const char * const vlp_pwrap_ulposc_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d32",
	"osc_d20",
	"osc_d16",
	"mainpll_d7_d8"
};

static const char * const vlp_spmi_32ksel_parents[] = {
	"tck_26m_mx9_ck",
	"clkrtc",
	"osc_d32",
	"osc_d20",
	"osc_d16",
	"mainpll_d7_d8"
};

static const char * const vlp_apxgpt_26m_b_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d20"
};

static const char * const vlp_dpsw_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d10",
	"osc_d7",
	"mainpll_d7_d4"
};

static const char * const vlp_spmi_m_parents[] = {
	"tck_26m_mx9_ck",
	"clkrtc",
	"f26m_d2",
	"osc_d32",
	"osc_d20",
	"osc_d16",
	"mainpll_d7_d8"
};

static const char * const vlp_dvfsrc_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d20"
};

static const char * const vlp_pwm_vlp_parents[] = {
	"tck_26m_mx9_ck",
	"clkrtc",
	"osc_d20",
	"osc_d8",
	"mainpll_d4_d8"
};

static const char * const vlp_axi_vlp_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d20",
	"mainpll_d7_d4",
	"osc_d4",
	"mainpll_d7_d2"
};

static const char * const vlp_systimer_26m_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d20"
};

static const char * const vlp_sspm_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d20",
	"mainpll_d5_d2",
	"osc_d2",
	"mainpll_d6"
};

static const char * const vlp_srck_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d20"
};

static const char * const vlp_camtg0_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_192m_d32",
	"univpll_192m_d16",
	"f26m_d2",
	"osc_d40",
	"osc_d32",
	"univpll_192m_d10",
	"univpll_192m_d8",
	"univpll_d6_d16",
	"osc_d20",
	"tvdpll_d16",
	"univpll_d6_d8"
};

static const char * const vlp_ips_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d2",
	"mainpll_d4"
};

static const char * const vlp_sspm_26m_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d20"
};

static const char * const vlp_ulposc_sspm_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d2",
	"mainpll_d4_d2"
};

static const struct mtk_mux vlp_ck_muxes[] = {
#if MT_CCF_MUX_DISABLE
	/* VLP_CLK_CFG_0 */
	MUX_CLR_SET_UPD(CLK_VLP_CK_SCP_SEL/* dts */, "vlp_scp_sel",
		vlp_scp_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 0/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SCP_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_SCP_SPI_SEL/* dts */, "vlp_scp_spi_sel",
		vlp_scp_spi_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 8/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SCP_SPI_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_SCP_IIC_SEL/* dts */, "vlp_scp_iic_sel",
		vlp_scp_iic_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 16/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SCP_IIC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_SCP_SPI_HIGH_SPD_SEL/* dts */, "vlp_scp_spi_hs_sel",
		vlp_scp_spi_hs_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 24/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SCP_SPI_HIGH_SPD_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_1 */
	MUX_CLR_SET_UPD(CLK_VLP_CK_SCP_IIC_HIGH_SPD_SEL/* dts */, "vlp_scp_iic_hs_sel",
		vlp_scp_iic_hs_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 0/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SCP_IIC_HIGH_SPD_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_PWRAP_ULPOSC_SEL/* dts */, "vlp_pwrap_ulposc_sel",
		vlp_pwrap_ulposc_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 8/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PWRAP_ULPOSC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_SPMI_M_TIA_32K_SEL/* dts */, "vlp_spmi_32ksel",
		vlp_spmi_32ksel_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 16/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPMI_M_TIA_32K_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_APXGPT_26M_BCLK_SEL/* dts */, "vlp_apxgpt_26m_b_sel",
		vlp_apxgpt_26m_b_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 24/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_APXGPT_26M_BCLK_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_2 */
	MUX_CLR_SET_UPD(CLK_VLP_CK_DPSW_SEL/* dts */, "vlp_dpsw_sel",
		vlp_dpsw_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 0/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DPSW_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_SPMI_M_MST_SEL/* dts */, "vlp_spmi_m_sel",
		vlp_spmi_m_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 8/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPMI_M_MST_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_DVFSRC_SEL/* dts */, "vlp_dvfsrc_sel",
		vlp_dvfsrc_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 16/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DVFSRC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_PWM_VLP_SEL/* dts */, "vlp_pwm_vlp_sel",
		vlp_pwm_vlp_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 24/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PWM_VLP_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_3 */
	MUX_CLR_SET_UPD(CLK_VLP_CK_AXI_VLP_SEL/* dts */, "vlp_axi_vlp_sel",
		vlp_axi_vlp_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 0/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AXI_VLP_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_SYSTIMER_26M_SEL/* dts */, "vlp_systimer_26m_sel",
		vlp_systimer_26m_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 8/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SYSTIMER_26M_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_SSPM_SEL/* dts */, "vlp_sspm_sel",
		vlp_sspm_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 16/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SSPM_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_SRCK_SEL/* dts */, "vlp_srck_sel",
		vlp_srck_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 24/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SRCK_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_CAMTG0_SEL/* dts */, "vlp_camtg0_sel",
		vlp_camtg0_parents/* parent */, VLP_CLK_CFG_4, VLP_CLK_CFG_4_SET,
		VLP_CLK_CFG_4_CLR/* set parent */, 8/* lsb */, 4/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CAMTG0_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_IPS_SEL/* dts */, "vlp_ips_sel",
		vlp_ips_parents/* parent */, VLP_CLK_CFG_4, VLP_CLK_CFG_4_SET,
		VLP_CLK_CFG_4_CLR/* set parent */, 16/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_IPS_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_SSPM_26M_SEL/* dts */, "vlp_sspm_26m_sel",
		vlp_sspm_26m_parents/* parent */, VLP_CLK_CFG_4, VLP_CLK_CFG_4_SET,
		VLP_CLK_CFG_4_CLR/* set parent */, 24/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SSPM_26M_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_5 */
	MUX_CLR_SET_UPD(CLK_VLP_CK_ULPOSC_SSPM_SEL/* dts */, "vlp_ulposc_sspm_sel",
		vlp_ulposc_sspm_parents/* parent */, VLP_CLK_CFG_5, VLP_CLK_CFG_5_SET,
		VLP_CLK_CFG_5_CLR/* set parent */, 0/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_ULPOSC_SSPM_SHIFT/* upd shift */),
#else
	/* VLP_CLK_CFG_0 */
	MUX_GATE_CLR_SET_UPD(CLK_VLP_CK_SCP_SEL/* dts */, "vlp_scp_sel",
		vlp_scp_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 0/* lsb */, 3/* width */,
		7/* pdn */, VLP_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SCP_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_SCP_SPI_SEL/* dts */, "vlp_scp_spi_sel",
		vlp_scp_spi_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 8/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SCP_SPI_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_SCP_IIC_SEL/* dts */, "vlp_scp_iic_sel",
		vlp_scp_iic_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 16/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SCP_IIC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_SCP_SPI_HIGH_SPD_SEL/* dts */, "vlp_scp_spi_hs_sel",
		vlp_scp_spi_hs_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 24/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SCP_SPI_HIGH_SPD_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_1 */
	MUX_CLR_SET_UPD(CLK_VLP_CK_SCP_IIC_HIGH_SPD_SEL/* dts */, "vlp_scp_iic_hs_sel",
		vlp_scp_iic_hs_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 0/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SCP_IIC_HIGH_SPD_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_PWRAP_ULPOSC_SEL/* dts */, "vlp_pwrap_ulposc_sel",
		vlp_pwrap_ulposc_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 8/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PWRAP_ULPOSC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_SPMI_M_TIA_32K_SEL/* dts */, "vlp_spmi_32ksel",
		vlp_spmi_32ksel_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 16/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPMI_M_TIA_32K_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_APXGPT_26M_BCLK_SEL/* dts */, "vlp_apxgpt_26m_b_sel",
		vlp_apxgpt_26m_b_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 24/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_APXGPT_26M_BCLK_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_2 */
	MUX_CLR_SET_UPD(CLK_VLP_CK_DPSW_SEL/* dts */, "vlp_dpsw_sel",
		vlp_dpsw_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 0/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DPSW_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_SPMI_M_MST_SEL/* dts */, "vlp_spmi_m_sel",
		vlp_spmi_m_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 8/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPMI_M_MST_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_DVFSRC_SEL/* dts */, "vlp_dvfsrc_sel",
		vlp_dvfsrc_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 16/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DVFSRC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_PWM_VLP_SEL/* dts */, "vlp_pwm_vlp_sel",
		vlp_pwm_vlp_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 24/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PWM_VLP_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_3 */
	MUX_CLR_SET_UPD(CLK_VLP_CK_AXI_VLP_SEL/* dts */, "vlp_axi_vlp_sel",
		vlp_axi_vlp_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 0/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AXI_VLP_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_SYSTIMER_26M_SEL/* dts */, "vlp_systimer_26m_sel",
		vlp_systimer_26m_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 8/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SYSTIMER_26M_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_SSPM_SEL/* dts */, "vlp_sspm_sel",
		vlp_sspm_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 16/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SSPM_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_SRCK_SEL/* dts */, "vlp_srck_sel",
		vlp_srck_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 24/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SRCK_SHIFT/* upd shift */),
	MUX_MULT_HWV(CLK_VLP_CK_CAMTG0_SEL/* dts */, "vlp_camtg0_sel",
		vlp_camtg0_parents/* parent */, VLP_CLK_CFG_4,
		VLP_CLK_CFG_4_SET, VLP_CLK_CFG_4_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, HWV_CG_16_DONE,
		HWV_CG_16_SET, HWV_CG_16_CLR, /* hwv */
		8/* lsb */, 4/* width */,
		15/* pdn */, VLP_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAMTG0_SHIFT/* upd shift */, 0, 0),
	MUX_GATE_CLR_SET_UPD(CLK_VLP_CK_IPS_SEL/* dts */, "vlp_ips_sel",
		vlp_ips_parents/* parent */, VLP_CLK_CFG_4, VLP_CLK_CFG_4_SET,
		VLP_CLK_CFG_4_CLR/* set parent */, 16/* lsb */, 2/* width */,
		23/* pdn */, VLP_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_IPS_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_SSPM_26M_SEL/* dts */, "vlp_sspm_26m_sel",
		vlp_sspm_26m_parents/* parent */, VLP_CLK_CFG_4, VLP_CLK_CFG_4_SET,
		VLP_CLK_CFG_4_CLR/* set parent */, 24/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SSPM_26M_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_5 */
	MUX_CLR_SET_UPD(CLK_VLP_CK_ULPOSC_SSPM_SEL/* dts */, "vlp_ulposc_sspm_sel",
		vlp_ulposc_sspm_parents/* parent */, VLP_CLK_CFG_5, VLP_CLK_CFG_5_SET,
		VLP_CLK_CFG_5_CLR/* set parent */, 0/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_ULPOSC_SSPM_SHIFT/* upd shift */),
#endif
};

static const char * const axi_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d20",
	"osc_d8",
	"osc_d4",
	"mainpll_d4_d4",
	"mainpll_d7_d2"
};

static const char * const peri_faxi_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d7_d8",
	"mainpll_d5_d8",
	"osc_d8",
	"mainpll_d7_d4",
	"mainpll_d5_d4",
	"mainpll_d4_d4",
	"mainpll_d7_d2"
};

static const char * const ufs_faxi_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d7_d8",
	"mainpll_d5_d8",
	"osc_d8",
	"mainpll_d7_d4",
	"mainpll_d5_d4",
	"mainpll_d4_d4",
	"mainpll_d7_d2"
};

static const char * const pextp_faxi_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d7_d8",
	"mainpll_d5_d8",
	"osc_d8",
	"mainpll_d7_d4",
	"mainpll_d5_d4",
	"mainpll_d4_d4",
	"mainpll_d7_d2"
};

static const char * const bus_aximem_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d20",
	"osc_d4",
	"mainpll_d7_d2",
	"mainpll_d5_d2",
	"mainpll_d4_d2",
	"mainpll_d6"
};

static const char * const mem_sub_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d20",
	"osc_d4",
	"univpll_d4_d4",
	"osc_d3",
	"mainpll_d5_d2",
	"mainpll_d4_d2",
	"mainpll_d6",
	"mainpll_d5",
	"univpll_d5",
	"mainpll_d4",
	"mainpll_d3"
};

static const char * const peri_fmem_sub_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d5_d8",
	"mainpll_d5_d4",
	"osc_d4",
	"univpll_d4_d4",
	"mainpll_d5_d2",
	"mainpll_d4_d2",
	"mainpll_d6",
	"mainpll_d5",
	"univpll_d5",
	"mainpll_d4"
};

static const char * const ufs_fmem_sub_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d5_d8",
	"mainpll_d5_d4",
	"osc_d4",
	"univpll_d4_d4",
	"mainpll_d5_d2",
	"mainpll_d4_d2",
	"mainpll_d6",
	"mainpll_d5",
	"univpll_d5",
	"mainpll_d4"
};

static const char * const pextp_fmem_sub_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d5_d8",
	"mainpll_d5_d4",
	"osc_d4",
	"univpll_d4_d4",
	"mainpll_d5_d2",
	"mainpll_d4_d2",
	"mainpll_d6",
	"mainpll_d5",
	"univpll_d5",
	"mainpll_d4"
};

static const char * const emi_n_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d4",
	"mainpll_d6_d2",
	"mainpll_d7",
	"mainpll_d5",
	"emipll_ck"
};

static const char * const emi_s_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d4",
	"mainpll_d6_d2",
	"mainpll_d7",
	"mainpll_d5",
	"emipll_ck"
};

static const char * const emi_slice_n_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d6_d2",
	"mainpll_d5",
	"emipll2_ck"
};

static const char * const emi_slice_s_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d6_d2",
	"mainpll_d5",
	"emipll2_ck"
};

static const char * const ap2conn_host_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d7_d4"
};

static const char * const atb_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d5_d2",
	"mainpll_d4_d2",
	"mainpll_d6"
};

static const char * const cirq_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d20",
	"mainpll_d7_d4"
};

static const char * const efuse_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d20"
};

static const char * const mcu_l3gic_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d8",
	"mainpll_d4_d4",
	"mainpll_d7_d2"
};

static const char * const mcu_infra_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d20",
	"osc_d4",
	"mainpll_d7_d2",
	"mainpll_d5_d2",
	"mainpll_d4_d2",
	"mainpll_d6"
};

static const char * const dsp_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d5",
	"osc_d4",
	"osc_d3",
	"univpll_d6_d2",
	"osc_d2",
	"univpll_d5",
	"ulposc_ck"
};

static const char * const mfg_ref_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d7_d2"
};

static const char * const mfg_eb_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d7_d2",
	"mainpll_d6_d2",
	"mainpll_d5_d2"
};

static const char * const uart_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d8",
	"univpll_d6_d4",
	"univpll_d6_d2"
};

static const char * const spi0_b_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d4",
	"univpll_d5_d4",
	"mainpll_d4_d4",
	"univpll_d4_d4",
	"mainpll_d6_d2",
	"univpll_192m_ck",
	"univpll_d6_d2"
};

static const char * const spi1_b_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d4",
	"univpll_d5_d4",
	"mainpll_d4_d4",
	"univpll_d4_d4",
	"mainpll_d6_d2",
	"univpll_192m_ck",
	"univpll_d6_d2"
};

static const char * const spi2_b_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d4",
	"univpll_d5_d4",
	"mainpll_d4_d4",
	"univpll_d4_d4",
	"mainpll_d6_d2",
	"univpll_192m_ck",
	"univpll_d6_d2"
};

static const char * const spi3_b_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d4",
	"univpll_d5_d4",
	"mainpll_d4_d4",
	"univpll_d4_d4",
	"mainpll_d6_d2",
	"univpll_192m_ck",
	"univpll_d6_d2"
};

static const char * const spi4_b_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d4",
	"univpll_d5_d4",
	"mainpll_d4_d4",
	"univpll_d4_d4",
	"mainpll_d6_d2",
	"univpll_192m_ck",
	"univpll_d6_d2"
};

static const char * const spi5_b_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d4",
	"univpll_d5_d4",
	"mainpll_d4_d4",
	"univpll_d4_d4",
	"mainpll_d6_d2",
	"univpll_192m_ck",
	"univpll_d6_d2"
};

static const char * const spi6_b_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d4",
	"univpll_d5_d4",
	"mainpll_d4_d4",
	"univpll_d4_d4",
	"mainpll_d6_d2",
	"univpll_192m_ck",
	"univpll_d6_d2"
};

static const char * const spi7_b_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d4",
	"univpll_d5_d4",
	"mainpll_d4_d4",
	"univpll_d4_d4",
	"mainpll_d6_d2",
	"univpll_192m_ck",
	"univpll_d6_d2"
};

static const char * const msdc_macro_1p_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d2",
	"mainpll_d6",
	"univpll_d6",
	"msdcpll_ck"
};

static const char * const msdc_macro_2p_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d2",
	"mainpll_d6",
	"univpll_d6",
	"msdcpll_ck"
};

static const char * const msdc30_1_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d4",
	"mainpll_d6_d2",
	"univpll_d6_d2",
	"msdcpll_d2"
};

static const char * const msdc30_2_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d4",
	"mainpll_d6_d2",
	"univpll_d6_d2",
	"msdcpll_d2"
};

static const char * const aud_intbus_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d7_d4",
	"mainpll_d4_d4"
};

static const char * const disp_pwm_parents[] = {
	"osc_d32",
	"tck_26m_mx9_ck",
	"osc_d8",
	"univpll_d6_d4",
	"univpll_d5_d4",
	"osc_d4",
	"mainpll_d4_d4"
};

static const char * const usb_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d5_d2"
};

static const char * const ssusb_xhci_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d5_d2"
};

static const char * const usb_1p_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d5_d4"
};

static const char * const ssusb_xhci_1p_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d5_d4"
};

static const char * const i2c_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d8",
	"univpll_d5_d4",
	"mainpll_d4_d4"
};

static const char * const aud_engen1_parents[] = {
	"tck_26m_mx9_ck",
	"apll1_d8",
	"apll1_d4"
};

static const char * const aud_engen2_parents[] = {
	"tck_26m_mx9_ck",
	"apll2_d8",
	"apll2_d4"
};

static const char * const aes_ufsfde_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d4",
	"univpll_d6_d2",
	"mainpll_d4_d2",
	"univpll_d6",
	"mainpll_d4"
};

static const char * const ufs_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d4",
	"univpll_d6_d2",
	"mainpll_d4_d2",
	"univpll_d6",
	"mainpll_d5",
	"mmpll_d6",
	"univpll_d5"
};

static const char * const ufs_mbist_parents[] = {
	"tck_26m_mx9_ck",
	"tvdpll_d2",
	"mainpll_d4",
	"tvdpll_ck"
};

static const char * const pextp_mbist_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d5"
};

static const char * const aud_1_parents[] = {
	"tck_26m_mx9_ck",
	"apll1_ck"
};

static const char * const aud_2_parents[] = {
	"tck_26m_mx9_ck",
	"apll2_ck"
};

static const char * const audio_h_parents[] = {
	"tck_26m_mx9_ck",
	"apll1_ck",
	"apll2_ck"
};

static const char * const adsp_parents[] = {
	"tck_26m_mx9_ck",
	"adsppll_ck"
};

static const char * const adsp_uarthub_b_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d4",
	"univpll_d6_d2"
};

static const char * const dpmaif_main_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d4_d4",
	"univpll_d5_d2",
	"mainpll_d4_d2",
	"univpll_d4_d2",
	"mainpll_d6",
	"univpll_d6",
	"mainpll_d5",
	"univpll_d5"
};

static const char * const pwm_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d7_d4",
	"univpll_d4_d8"
};

static const char * const mcupm_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d7_d2",
	"mainpll_d6_d2",
	"univpll_d6_d2",
	"mainpll_d5_d2"
};

static const char * const sflash_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d7_d8",
	"univpll_d6_d8"
};

static const char * const ipseast_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d6",
	"mainpll_d5",
	"mainpll_d4",
	"mainpll_d3"
};

static const char * const ipswest_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d6",
	"mainpll_d5",
	"mainpll_d4",
	"mainpll_d3"
};

static const char * const tl_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d4",
	"mainpll_d5_d2"
};

static const char * const md_emi_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4"
};

static const char * const sdf_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d5_d2",
	"mainpll_d4_d2",
	"mainpll_d6",
	"mainpll_d4",
	"univpll_d4"
};

static const char * const uarthub_b_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d4",
	"univpll_d6_d2"
};

static const char * const ssr_pka_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d4",
	"mainpll_d4_d2",
	"mainpll_d7",
	"mainpll_d6",
	"mainpll_d5"
};

static const char * const ssr_dma_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d4",
	"mainpll_d4_d2",
	"mainpll_d7",
	"mainpll_d6",
	"mainpll_d5"
};

static const char * const ssr_kdf_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d4",
	"mainpll_d4_d2",
	"mainpll_d7",
	"mainpll_d6",
	"mainpll_d5"
};

static const char * const ssr_rng_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d4",
	"mainpll_d5_d2",
	"mainpll_d4_d2"
};

static const char * const ssr_sej_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d4",
	"mainpll_d5_d2",
	"mainpll_d4_d2"
};

static const char * const dxcc_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d8",
	"mainpll_d4_d4",
	"mainpll_d4_d2"
};

static const char * const dpsw_cmp_26m_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d20"
};

static const char * const apll_i2sin0_m_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const apll_i2sin1_m_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const apll_i2sin2_m_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const apll_i2sin3_m_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const apll_i2sin4_m_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const apll_i2sin6_m_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const apll_i2sout0_m_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const apll_i2sout1_m_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const apll_i2sout2_m_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const apll_i2sout3_m_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const apll_i2sout4_m_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const apll_i2sout6_m_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const apll_fmi2s_m_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const apll_tdmout_m_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const camtg1_parents[] = {
	"tck_26m_mx9_ck",
	"univpll2_192m_d32",
	"univpll2_192m_d16",
	"f26m_d2",
	"osc_d40",
	"osc_d32",
	"univpll2_192m_d10",
	"univpll2_192m_d8",
	"univpll2_d6_d16",
	"osc_d20",
	"tvdpll_d16",
	"univpll2_d6_d8"
};

static const char * const camtg2_parents[] = {
	"tck_26m_mx9_ck",
	"univpll2_192m_d32",
	"univpll2_192m_d16",
	"f26m_d2",
	"osc_d40",
	"osc_d32",
	"univpll2_192m_d10",
	"univpll2_192m_d8",
	"univpll2_d6_d16",
	"osc_d20",
	"tvdpll_d16",
	"univpll2_d6_d8"
};

static const char * const camtg3_parents[] = {
	"tck_26m_mx9_ck",
	"univpll2_192m_d32",
	"univpll2_192m_d16",
	"f26m_d2",
	"osc_d40",
	"osc_d32",
	"univpll2_192m_d10",
	"univpll2_192m_d8",
	"univpll2_d6_d16",
	"osc_d20",
	"tvdpll_d16",
	"univpll2_d6_d8"
};

static const char * const camtg4_parents[] = {
	"tck_26m_mx9_ck",
	"univpll2_192m_d32",
	"univpll2_192m_d16",
	"f26m_d2",
	"osc_d40",
	"osc_d32",
	"univpll2_192m_d10",
	"univpll2_192m_d8",
	"univpll2_d6_d16",
	"osc_d20",
	"tvdpll_d16",
	"univpll2_d6_d8"
};

static const char * const camtg5_parents[] = {
	"tck_26m_mx9_ck",
	"univpll2_192m_d32",
	"univpll2_192m_d16",
	"f26m_d2",
	"osc_d40",
	"osc_d32",
	"univpll2_192m_d10",
	"univpll2_192m_d8",
	"univpll2_d6_d16",
	"osc_d20",
	"tvdpll_d16",
	"univpll2_d6_d8"
};

static const char * const camtg6_parents[] = {
	"tck_26m_mx9_ck",
	"univpll2_192m_d32",
	"univpll2_192m_d16",
	"f26m_d2",
	"osc_d40",
	"osc_d32",
	"univpll2_192m_d10",
	"univpll2_192m_d8",
	"univpll2_d6_d16",
	"osc_d20",
	"tvdpll_d16",
	"univpll2_d6_d8"
};

static const char * const camtg7_parents[] = {
	"tck_26m_mx9_ck",
	"univpll2_192m_d32",
	"univpll2_192m_d16",
	"f26m_d2",
	"osc_d40",
	"osc_d32",
	"univpll2_192m_d10",
	"univpll2_192m_d8",
	"univpll2_d6_d16",
	"osc_d20",
	"tvdpll_d16",
	"univpll2_d6_d8"
};

static const char * const seninf0_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d10",
	"osc_d8",
	"osc_d5",
	"osc_d4",
	"univpll2_d6_d2",
	"mainpll2_d9",
	"osc_d2",
	"mainpll2_d4_d2",
	"univpll2_d4_d2",
	"mmpll2_d4_d2",
	"univpll2_d7",
	"mainpll2_d6",
	"mmpll2_d7",
	"univpll2_d6",
	"univpll2_d5"
};

static const char * const seninf1_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d10",
	"osc_d8",
	"osc_d5",
	"osc_d4",
	"univpll2_d6_d2",
	"mainpll2_d9",
	"osc_d2",
	"mainpll2_d4_d2",
	"univpll2_d4_d2",
	"mmpll2_d4_d2",
	"univpll2_d7",
	"mainpll2_d6",
	"mmpll2_d7",
	"univpll2_d6",
	"univpll2_d5"
};

static const char * const seninf2_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d10",
	"osc_d8",
	"osc_d5",
	"osc_d4",
	"univpll2_d6_d2",
	"mainpll2_d9",
	"osc_d2",
	"mainpll2_d4_d2",
	"univpll2_d4_d2",
	"mmpll2_d4_d2",
	"univpll2_d7",
	"mainpll2_d6",
	"mmpll2_d7",
	"univpll2_d6",
	"univpll2_d5"
};

static const char * const seninf3_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d10",
	"osc_d8",
	"osc_d5",
	"osc_d4",
	"univpll2_d6_d2",
	"mainpll2_d9",
	"osc_d2",
	"mainpll2_d4_d2",
	"univpll2_d4_d2",
	"mmpll2_d4_d2",
	"univpll2_d7",
	"mainpll2_d6",
	"mmpll2_d7",
	"univpll2_d6",
	"univpll2_d5"
};

static const char * const seninf4_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d10",
	"osc_d8",
	"osc_d5",
	"osc_d4",
	"univpll2_d6_d2",
	"mainpll2_d9",
	"osc_d2",
	"mainpll2_d4_d2",
	"univpll2_d4_d2",
	"mmpll2_d4_d2",
	"univpll2_d7",
	"mainpll2_d6",
	"mmpll2_d7",
	"univpll2_d6",
	"univpll2_d5"
};

static const char * const seninf5_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d10",
	"osc_d8",
	"osc_d5",
	"osc_d4",
	"univpll2_d6_d2",
	"mainpll2_d9",
	"osc_d2",
	"mainpll2_d4_d2",
	"univpll2_d4_d2",
	"mmpll2_d4_d2",
	"univpll2_d7",
	"mainpll2_d6",
	"mmpll2_d7",
	"univpll2_d6",
	"univpll2_d5"
};

static const char * const ccu_ahb_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d3",
	"mmpll2_d5_d2"
};

static const char * const img1_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d4",
	"osc_d3",
	"mmpll2_d6_d2",
	"imgpll_d5_d2",
	"mmpll2_d5_d2",
	"univpll2_d4_d2",
	"mmpll2_d4_d2",
	"mmpll2_d7",
	"univpll2_d6",
	"mmpll2_d6",
	"univpll2_d5",
	"mmpll2_d5",
	"univpll2_d4",
	"imgpll_d4"
};

static const char * const ipe_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d4",
	"osc_d3",
	"mmpll2_d6_d2",
	"mmpll2_d5_d2",
	"univpll2_d4_d2",
	"mmpll2_d4_d2",
	"mainpll2_d6",
	"univpll2_d6",
	"mmpll2_d6",
	"mmpll2_d5",
	"imgpll_d4"
};

static const char * const cam_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d10",
	"osc_d6",
	"osc_d4",
	"osc_d3",
	"mmpll2_d5_d2",
	"univpll2_d4_d2",
	"univpll2_d7",
	"mmpll2_d7",
	"univpll2_d6",
	"mmpll2_d6",
	"univpll2_d5",
	"mmpll2_d5",
	"univpll2_d4",
	"imgpll_d4",
	"mmpll2_d4"
};

static const char * const camtm_parents[] = {
	"tck_26m_mx9_ck",
	"univpll2_d6_d4",
	"osc_d4",
	"osc_d3",
	"univpll2_d6_d2"
};

static const char * const dpe_parents[] = {
	"tck_26m_mx9_ck",
	"mmpll2_d5_d2",
	"univpll2_d4_d2",
	"mmpll2_d7",
	"univpll2_d6",
	"mmpll2_d6",
	"univpll2_d5",
	"mmpll2_d5",
	"imgpll_d4"
};

static const char * const vdec_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll2_d5_d4",
	"mainpll2_d4_d4",
	"mainpll2_d7_d2",
	"mainpll2_d6_d2",
	"mainpll_d5_d2",
	"mainpll2_d5_d2",
	"mainpll2_d9",
	"mainpll2_d4_d2",
	"mainpll2_d7",
	"mainpll2_d6",
	"univpll2_d6",
	"mainpll2_d5",
	"mainpll2_d4",
	"imgpll_d2"
};

static const char * const ccusys_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d4",
	"osc_d3",
	"mmpll2_d5_d2",
	"univpll2_d4_d2",
	"mmpll2_d7",
	"univpll2_d6",
	"mmpll2_d6",
	"univpll2_d5",
	"mainpll2_d4",
	"mainpll2_d3",
	"univpll2_d3"
};

static const char * const ccutm_parents[] = {
	"tck_26m_mx9_ck",
	"univpll2_d6_d4",
	"osc_d4",
	"osc_d3",
	"univpll2_d6_d2"
};

static const char * const venc_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll2_d5_d2",
	"univpll2_d5_d2",
	"mainpll2_d4_d2",
	"mmpll2_d9",
	"univpll2_d4_d2",
	"mmpll2_d4_d2",
	"mainpll2_d6",
	"univpll2_d6",
	"mainpll2_d5",
	"mmpll2_d6",
	"univpll2_d5",
	"mainpll2_d4",
	"univpll2_d4",
	"mmpll2_d4"
};

static const char * const dp_core_parents[] = {
	"tck_26m_mx9_ck",
	"tvdpll_d16",
	"tvdpll_d8",
	"tvdpll_d4",
	"tvdpll_d2"
};

static const char * const dp_parents[] = {
	"tck_26m_mx9_ck",
	"tvdpll_d16",
	"tvdpll_d8",
	"tvdpll_d4",
	"tvdpll_d2"
};

static const char * const disp_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d5_d2",
	"mainpll2_d5_d2",
	"mmpll2_d6_d2",
	"mainpll_d4_d2",
	"mainpll2_d7",
	"mainpll2_d6",
	"mainpll2_d5",
	"mmpll2_d6",
	"mainpll2_d4",
	"univpll2_d4",
	"mainpll2_d3"
};

static const char * const mdp_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d5_d2",
	"mainpll2_d5_d2",
	"mmpll2_d6_d2",
	"mainpll2_d9",
	"mainpll_d4_d2",
	"mainpll2_d7",
	"mainpll2_d6",
	"mainpll2_d5",
	"mmpll2_d6",
	"mainpll2_d4",
	"univpll2_d4",
	"mainpll2_d3"
};

static const char * const mminfra_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d4",
	"mainpll_d7_d2",
	"mainpll_d5_d2",
	"mainpll2_d5_d2",
	"mmpll2_d6_d2",
	"mainpll2_d4_d2",
	"mainpll_d6",
	"univpll2_d6",
	"mainpll2_d5",
	"mmpll2_d6",
	"univpll2_d5",
	"mainpll2_d4",
	"univpll2_d4",
	"mmpll2_d4",
	"mainpll2_d3"
};

static const char * const mmup_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll2_d5_d2",
	"osc_d2",
	"ulposc_ck",
	"mainpll_d4",
	"univpll2_d4",
	"mainpll2_d3"
};

static const struct mtk_mux top_muxes[] = {
#if MT_CCF_MUX_DISABLE
	/* CLK_CFG_0 */
	MUX_CLR_SET_UPD_CHK(CLK_TOP_AXI_SEL/* dts */, "axi_sel",
		axi_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AXI_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 31/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_P_FAXI_SEL/* dts */, "peri_faxi_sel",
		peri_faxi_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PERI_FAXI_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 30/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_U_FAXI_SEL/* dts */, "ufs_faxi_sel",
		ufs_faxi_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_UFS_FAXI_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 29/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_PEXTP_FAXI_SEL/* dts */, "pextp_faxi_sel",
		pextp_faxi_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PEXTP_FAXI_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 28/* cksta shift */),
	/* CLK_CFG_1 */
	MUX_CLR_SET_UPD_CHK(CLK_TOP_BUS_AXIMEM_SEL/* dts */, "bus_aximem_sel",
		bus_aximem_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_BUS_AXIMEM_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 27/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_MEM_SUB_SEL/* dts */, "mem_sub_sel",
		mem_sub_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 8/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MEM_SUB_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 26/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_P_FMEM_SUB_SEL/* dts */, "peri_fmem_sub_sel",
		peri_fmem_sub_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 16/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PERI_FMEM_SUB_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 25/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_U_FMEM_SUB_SEL/* dts */, "ufs_fmem_sub_sel",
		ufs_fmem_sub_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 24/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_UFS_FMEM_SUB_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 24/* cksta shift */),
	/* CLK_CFG_2 */
	MUX_CLR_SET_UPD_CHK(CLK_TOP_PEXTP_FMEM_SUB_SEL/* dts */, "pextp_fmem_sub_sel",
		pextp_fmem_sub_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PEXTP_FMEM_SUB_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 23/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_EMI_N_SEL/* dts */, "emi_n_sel",
		emi_n_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_EMI_N_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 22/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_EMI_S_SEL/* dts */, "emi_s_sel",
		emi_s_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_EMI_S_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 21/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_EMI_SLICE_N_SEL/* dts */, "emi_slice_n_sel",
		emi_slice_n_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_EMI_SLICE_N_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 20/* cksta shift */),
	/* CLK_CFG_3 */
	MUX_CLR_SET_UPD_CHK(CLK_TOP_EMI_SLICE_S_SEL/* dts */, "emi_slice_s_sel",
		emi_slice_s_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_EMI_SLICE_S_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 19/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_AP2CONN_HOST_SEL/* dts */, "ap2conn_host_sel",
		ap2conn_host_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 8/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AP2CONN_HOST_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 18/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_ATB_SEL/* dts */, "atb_sel",
		atb_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_ATB_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 17/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_CIRQ_SEL/* dts */, "cirq_sel",
		cirq_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CIRQ_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 16/* cksta shift */),
	/* CLK_CFG_4 */
	MUX_CLR_SET_UPD_CHK(CLK_TOP_EFUSE_SEL/* dts */, "efuse_sel",
		efuse_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 0/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_EFUSE_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 15/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_MCU_L3GIC_SEL/* dts */, "mcu_l3gic_sel",
		mcu_l3gic_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MCU_L3GIC_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 14/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_MCU_INFRA_SEL/* dts */, "mcu_infra_sel",
		mcu_infra_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MCU_INFRA_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 13/* cksta shift */),
	/* CLK_CFG_5 */
	MUX_CLR_SET_UPD_CHK(CLK_TOP_DSP_SEL/* dts */, "dsp_sel",
		dsp_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DSP_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 11/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_MFG_REF_SEL/* dts */, "mfg_ref_sel",
		mfg_ref_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 8/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MFG_REF_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 10/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_MFG_EB_SEL/* dts */, "mfg_eb_sel",
		mfg_eb_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MFG_EB_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 8/* cksta shift */),
	/* CLK_CFG_6 */
	MUX_CLR_SET_UPD_CHK(CLK_TOP_UART_SEL/* dts */, "uart_sel",
		uart_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_UART_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 7/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_SPI0_BCLK_SEL/* dts */, "spi0_b_sel",
		spi0_b_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPI0_BCLK_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 6/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_SPI1_BCLK_SEL/* dts */, "spi1_b_sel",
		spi1_b_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPI1_BCLK_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 5/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_SPI2_BCLK_SEL/* dts */, "spi2_b_sel",
		spi2_b_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPI2_BCLK_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 4/* cksta shift */),
	/* CLK_CFG_7 */
	MUX_CLR_SET_UPD_CHK(CLK_TOP_SPI3_BCLK_SEL/* dts */, "spi3_b_sel",
		spi3_b_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPI3_BCLK_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 3/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_SPI4_BCLK_SEL/* dts */, "spi4_b_sel",
		spi4_b_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPI4_BCLK_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 2/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_SPI5_BCLK_SEL/* dts */, "spi5_b_sel",
		spi5_b_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPI5_BCLK_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 1/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_SPI6_BCLK_SEL/* dts */, "spi6_b_sel",
		spi6_b_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SPI6_BCLK_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 31/* cksta shift */),
	/* CLK_CFG_8 */
	MUX_CLR_SET_UPD_CHK(CLK_TOP_SPI7_BCLK_SEL/* dts */, "spi7_b_sel",
		spi7_b_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SPI7_BCLK_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 30/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_MSDC_MACRO_1P_SEL/* dts */, "msdc_macro_1p_sel",
		msdc_macro_1p_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_MSDC_MACRO_1P_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 29/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_MSDC_MACRO_2P_SEL/* dts */, "msdc_macro_2p_sel",
		msdc_macro_2p_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_MSDC_MACRO_2P_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 28/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_MSDC30_1_SEL/* dts */, "msdc30_1_sel",
		msdc30_1_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_MSDC30_1_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 27/* cksta shift */),
	/* CLK_CFG_9 */
	MUX_CLR_SET_UPD_CHK(CLK_TOP_MSDC30_2_SEL/* dts */, "msdc30_2_sel",
		msdc30_2_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_MSDC30_2_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 26/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_AUD_INTBUS_SEL/* dts */, "aud_intbus_sel",
		aud_intbus_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AUD_INTBUS_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 25/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_DISP_PWM_SEL/* dts */, "disp_pwm_sel",
		disp_pwm_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_DISP_PWM_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 24/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_USB_TOP_SEL/* dts */, "usb_sel",
		usb_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 24/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_USB_TOP_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 23/* cksta shift */),
	/* CLK_CFG_10 */
	MUX_CLR_SET_UPD_CHK(CLK_TOP_USB_XHCI_SEL/* dts */, "ssusb_xhci_sel",
		ssusb_xhci_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 0/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SSUSB_XHCI_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 22/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_USB_TOP_1P_SEL/* dts */, "usb_1p_sel",
		usb_1p_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 8/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_USB_TOP_1P_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 21/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_USB_XHCI_1P_SEL/* dts */, "ssusb_xhci_1p_sel",
		ssusb_xhci_1p_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 16/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SSUSB_XHCI_1P_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 20/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_I2C_SEL/* dts */, "i2c_sel",
		i2c_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_I2C_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 19/* cksta shift */),
	/* CLK_CFG_11 */
	MUX_CLR_SET_UPD_CHK(CLK_TOP_AUD_ENGEN1_SEL/* dts */, "aud_engen1_sel",
		aud_engen1_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AUD_ENGEN1_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 18/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_AUD_ENGEN2_SEL/* dts */, "aud_engen2_sel",
		aud_engen2_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AUD_ENGEN2_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 17/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_AES_UFSFDE_SEL/* dts */, "aes_ufsfde_sel",
		aes_ufsfde_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AES_UFSFDE_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 16/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_U_SEL/* dts */, "ufs_sel",
		ufs_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_UFS_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 15/* cksta shift */),
	/* CLK_CFG_12 */
	MUX_CLR_SET_UPD_CHK(CLK_TOP_U_MBIST_SEL/* dts */, "ufs_mbist_sel",
		ufs_mbist_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_UFS_MBIST_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 14/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_PEXTP_MBIST_SEL/* dts */, "pextp_mbist_sel",
		pextp_mbist_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 8/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_PEXTP_MBIST_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 13/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_AUD_1_SEL/* dts */, "aud_1_sel",
		aud_1_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 16/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AUD_1_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 12/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_AUD_2_SEL/* dts */, "aud_2_sel",
		aud_2_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 24/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AUD_2_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 11/* cksta shift */),
	/* CLK_CFG_13 */
	MUX_CLR_SET_UPD_CHK(CLK_TOP_AUDIO_H_SEL/* dts */, "audio_h_sel",
		audio_h_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AUDIO_H_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 10/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_ADSP_SEL/* dts */, "adsp_sel",
		adsp_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 8/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_ADSP_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 9/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_ADSP_UARTHUB_BCLK_SEL/* dts */, "adsp_uarthub_b_sel",
		adsp_uarthub_b_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_ADSP_UARTHUB_BCLK_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 8/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_DPMAIF_MAIN_SEL/* dts */, "dpmaif_main_sel",
		dpmaif_main_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 24/* lsb */, 4/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_DPMAIF_MAIN_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 7/* cksta shift */),
	/* CLK_CFG_14 */
	MUX_CLR_SET_UPD_CHK(CLK_TOP_PWM_SEL/* dts */, "pwm_sel",
		pwm_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_PWM_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 6/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_MCUPM_SEL/* dts */, "mcupm_sel",
		mcupm_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_MCUPM_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 5/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_SFLASH_SEL/* dts */, "sflash_sel",
		sflash_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SFLASH_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 4/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_IPSEAST_SEL/* dts */, "ipseast_sel",
		ipseast_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_IPSEAST_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 3/* cksta shift */),
	/* CLK_CFG_15 */
	MUX_CLR_SET_UPD_CHK(CLK_TOP_IPSWEST_SEL/* dts */, "ipswest_sel",
		ipswest_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_IPSWEST_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 2/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_TL_SEL/* dts */, "tl_sel",
		tl_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_TL_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 1/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_EMI_INTERFACE_546_SEL/* dts */, "md_emi_sel",
		md_emi_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 16/* lsb */, 1/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_EMI_INTERFACE_546_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 31/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_SDF_SEL/* dts */, "sdf_sel",
		sdf_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SDF_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 30/* cksta shift */),
	/* CLK_CFG_16 */
	MUX_CLR_SET_UPD_CHK(CLK_TOP_UARTHUB_BCLK_SEL/* dts */, "uarthub_b_sel",
		uarthub_b_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_UARTHUB_BCLK_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 29/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_SSR_PKA_SEL/* dts */, "ssr_pka_sel",
		ssr_pka_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SSR_PKA_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 28/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_SSR_DMA_SEL/* dts */, "ssr_dma_sel",
		ssr_dma_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SSR_DMA_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 27/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_SSR_KDF_SEL/* dts */, "ssr_kdf_sel",
		ssr_kdf_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SSR_KDF_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 26/* cksta shift */),
	/* CLK_CFG_17 */
	MUX_CLR_SET_UPD_CHK(CLK_TOP_SSR_RNG_SEL/* dts */, "ssr_rng_sel",
		ssr_rng_parents/* parent */, CLK_CFG_17, CLK_CFG_17_SET,
		CLK_CFG_17_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SSR_RNG_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 25/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_SSR_SEJ_SEL/* dts */, "ssr_sej_sel",
		ssr_sej_parents/* parent */, CLK_CFG_17, CLK_CFG_17_SET,
		CLK_CFG_17_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SSR_SEJ_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 24/* cksta shift */),
	/* CLK_CFG_18 */
	MUX_CLR_SET_UPD_CHK(CLK_TOP_DXCC_SEL/* dts */, "dxcc_sel",
		dxcc_parents/* parent */, CLK_CFG_18, CLK_CFG_18_SET,
		CLK_CFG_18_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_DXCC_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 21/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_DPSW_CMP_26M_SEL/* dts */, "dpsw_cmp_26m_sel",
		dpsw_cmp_26m_parents/* parent */, CLK_CFG_18, CLK_CFG_18_SET,
		CLK_CFG_18_CLR/* set parent */, 8/* lsb */, 1/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_DPSW_CMP_26M_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 20/* cksta shift */),
	/* CKSYS2_CLK_CFG_0 */
	MUX_CLR_SET_UPD_CHK(CLK_TOP_CAMTG1_SEL/* dts */, "camtg1_sel",
		camtg1_parents/* parent */, CKSYS2_CLK_CFG_0, CKSYS2_CLK_CFG_0_SET,
		CKSYS2_CLK_CFG_0_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CAMTG1_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 31/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_CAMTG2_SEL/* dts */, "camtg2_sel",
		camtg2_parents/* parent */, CKSYS2_CLK_CFG_0, CKSYS2_CLK_CFG_0_SET,
		CKSYS2_CLK_CFG_0_CLR/* set parent */, 8/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CAMTG2_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 30/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_CAMTG3_SEL/* dts */, "camtg3_sel",
		camtg3_parents/* parent */, CKSYS2_CLK_CFG_0, CKSYS2_CLK_CFG_0_SET,
		CKSYS2_CLK_CFG_0_CLR/* set parent */, 16/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CAMTG3_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 29/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_CAMTG4_SEL/* dts */, "camtg4_sel",
		camtg4_parents/* parent */, CKSYS2_CLK_CFG_0, CKSYS2_CLK_CFG_0_SET,
		CKSYS2_CLK_CFG_0_CLR/* set parent */, 24/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CAMTG4_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 28/* cksta shift */),
	/* CKSYS2_CLK_CFG_1 */
	MUX_CLR_SET_UPD_CHK(CLK_TOP_CAMTG5_SEL/* dts */, "camtg5_sel",
		camtg5_parents/* parent */, CKSYS2_CLK_CFG_1, CKSYS2_CLK_CFG_1_SET,
		CKSYS2_CLK_CFG_1_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CAMTG5_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 27/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_CAMTG6_SEL/* dts */, "camtg6_sel",
		camtg6_parents/* parent */, CKSYS2_CLK_CFG_1, CKSYS2_CLK_CFG_1_SET,
		CKSYS2_CLK_CFG_1_CLR/* set parent */, 8/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CAMTG6_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 26/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_CAMTG7_SEL/* dts */, "camtg7_sel",
		camtg7_parents/* parent */, CKSYS2_CLK_CFG_1, CKSYS2_CLK_CFG_1_SET,
		CKSYS2_CLK_CFG_1_CLR/* set parent */, 16/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CAMTG7_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 25/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_SENINF0_SEL/* dts */, "seninf0_sel",
		seninf0_parents/* parent */, CKSYS2_CLK_CFG_1, CKSYS2_CLK_CFG_1_SET,
		CKSYS2_CLK_CFG_1_CLR/* set parent */, 24/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SENINF0_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 24/* cksta shift */),
	/* CKSYS2_CLK_CFG_2 */
	MUX_CLR_SET_UPD_CHK(CLK_TOP_SENINF1_SEL/* dts */, "seninf1_sel",
		seninf1_parents/* parent */, CKSYS2_CLK_CFG_2, CKSYS2_CLK_CFG_2_SET,
		CKSYS2_CLK_CFG_2_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SENINF1_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 23/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_SENINF2_SEL/* dts */, "seninf2_sel",
		seninf2_parents/* parent */, CKSYS2_CLK_CFG_2, CKSYS2_CLK_CFG_2_SET,
		CKSYS2_CLK_CFG_2_CLR/* set parent */, 8/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SENINF2_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 22/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_SENINF3_SEL/* dts */, "seninf3_sel",
		seninf3_parents/* parent */, CKSYS2_CLK_CFG_2, CKSYS2_CLK_CFG_2_SET,
		CKSYS2_CLK_CFG_2_CLR/* set parent */, 16/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SENINF3_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 21/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_SENINF4_SEL/* dts */, "seninf4_sel",
		seninf4_parents/* parent */, CKSYS2_CLK_CFG_2, CKSYS2_CLK_CFG_2_SET,
		CKSYS2_CLK_CFG_2_CLR/* set parent */, 24/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SENINF4_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 20/* cksta shift */),
	/* CKSYS2_CLK_CFG_3 */
	MUX_CLR_SET_UPD_CHK(CLK_TOP_SENINF5_SEL/* dts */, "seninf5_sel",
		seninf5_parents/* parent */, CKSYS2_CLK_CFG_3, CKSYS2_CLK_CFG_3_SET,
		CKSYS2_CLK_CFG_3_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SENINF5_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 19/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_CCU_AHB_SEL/* dts */, "ccu_ahb_sel",
		ccu_ahb_parents/* parent */, CKSYS2_CLK_CFG_3, CKSYS2_CLK_CFG_3_SET,
		CKSYS2_CLK_CFG_3_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CCU_AHB_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 18/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_IMG1_SEL/* dts */, "img1_sel",
		img1_parents/* parent */, CKSYS2_CLK_CFG_3, CKSYS2_CLK_CFG_3_SET,
		CKSYS2_CLK_CFG_3_CLR/* set parent */, 16/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_IMG1_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 17/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_IPE_SEL/* dts */, "ipe_sel",
		ipe_parents/* parent */, CKSYS2_CLK_CFG_3, CKSYS2_CLK_CFG_3_SET,
		CKSYS2_CLK_CFG_3_CLR/* set parent */, 24/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_IPE_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 16/* cksta shift */),
	/* CKSYS2_CLK_CFG_4 */
	MUX_CLR_SET_UPD_CHK(CLK_TOP_CAM_SEL/* dts */, "cam_sel",
		cam_parents/* parent */, CKSYS2_CLK_CFG_4, CKSYS2_CLK_CFG_4_SET,
		CKSYS2_CLK_CFG_4_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CAM_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 15/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_CAMTM_SEL/* dts */, "camtm_sel",
		camtm_parents/* parent */, CKSYS2_CLK_CFG_4, CKSYS2_CLK_CFG_4_SET,
		CKSYS2_CLK_CFG_4_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CAMTM_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 14/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_DPE_SEL/* dts */, "dpe_sel",
		dpe_parents/* parent */, CKSYS2_CLK_CFG_4, CKSYS2_CLK_CFG_4_SET,
		CKSYS2_CLK_CFG_4_CLR/* set parent */, 16/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DPE_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 13/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_VDEC_SEL/* dts */, "vdec_sel",
		vdec_parents/* parent */, CKSYS2_CLK_CFG_4, CKSYS2_CLK_CFG_4_SET,
		CKSYS2_CLK_CFG_4_CLR/* set parent */, 24/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_VDEC_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 12/* cksta shift */),
	/* CKSYS2_CLK_CFG_5 */
	MUX_CLR_SET_UPD_CHK(CLK_TOP_CCUSYS_SEL/* dts */, "ccusys_sel",
		ccusys_parents/* parent */, CKSYS2_CLK_CFG_5, CKSYS2_CLK_CFG_5_SET,
		CKSYS2_CLK_CFG_5_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CCUSYS_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 11/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_CCUTM_SEL/* dts */, "ccutm_sel",
		ccutm_parents/* parent */, CKSYS2_CLK_CFG_5, CKSYS2_CLK_CFG_5_SET,
		CKSYS2_CLK_CFG_5_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CCUTM_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 10/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_VENC_SEL/* dts */, "venc_sel",
		venc_parents/* parent */, CKSYS2_CLK_CFG_5, CKSYS2_CLK_CFG_5_SET,
		CKSYS2_CLK_CFG_5_CLR/* set parent */, 16/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_VENC_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 9/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_DP_CORE_SEL/* dts */, "dp_core_sel",
		dp_core_parents/* parent */, CKSYS2_CLK_CFG_5, CKSYS2_CLK_CFG_5_SET,
		CKSYS2_CLK_CFG_5_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DP_CORE_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 8/* cksta shift */),
	/* CKSYS2_CLK_CFG_6 */
	MUX_CLR_SET_UPD_CHK(CLK_TOP_DP_SEL/* dts */, "dp_sel",
		dp_parents/* parent */, CKSYS2_CLK_CFG_6, CKSYS2_CLK_CFG_6_SET,
		CKSYS2_CLK_CFG_6_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DP_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 7/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_DISP_SEL/* dts */, "disp_sel",
		disp_parents/* parent */, CKSYS2_CLK_CFG_6, CKSYS2_CLK_CFG_6_SET,
		CKSYS2_CLK_CFG_6_CLR/* set parent */, 8/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DISP_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 6/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_MDP_SEL/* dts */, "mdp_sel",
		mdp_parents/* parent */, CKSYS2_CLK_CFG_6, CKSYS2_CLK_CFG_6_SET,
		CKSYS2_CLK_CFG_6_CLR/* set parent */, 16/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MDP_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 5/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_MMINFRA_SEL/* dts */, "mminfra_sel",
		mminfra_parents/* parent */, CKSYS2_CLK_CFG_6, CKSYS2_CLK_CFG_6_SET,
		CKSYS2_CLK_CFG_6_CLR/* set parent */, 24/* lsb */, 4/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MMINFRA_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 4/* cksta shift */),
	/* CKSYS2_CLK_CFG_7 */
	MUX_CLR_SET_UPD_CHK(CLK_TOP_MMUP_SEL/* dts */, "mmup_sel",
		mmup_parents/* parent */, CKSYS2_CLK_CFG_7, CKSYS2_CLK_CFG_7_SET,
		CKSYS2_CLK_CFG_7_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MMUP_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 3/* cksta shift */),
#else
	/* CLK_CFG_0 */
	MUX_CLR_SET_UPD_CHK(CLK_TOP_AXI_SEL/* dts */, "axi_sel",
		axi_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AXI_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 31/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_P_FAXI_SEL/* dts */, "peri_faxi_sel",
		peri_faxi_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PERI_FAXI_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 30/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_U_FAXI_SEL/* dts */, "ufs_faxi_sel",
		ufs_faxi_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_UFS_FAXI_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 29/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_PEXTP_FAXI_SEL/* dts */, "pextp_faxi_sel",
		pextp_faxi_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PEXTP_FAXI_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 28/* cksta shift */),
	/* CLK_CFG_1 */
	MUX_CLR_SET_UPD_CHK(CLK_TOP_BUS_AXIMEM_SEL/* dts */, "bus_aximem_sel",
		bus_aximem_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_BUS_AXIMEM_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 27/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_MEM_SUB_SEL/* dts */, "mem_sub_sel",
		mem_sub_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 8/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MEM_SUB_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 26/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_P_FMEM_SUB_SEL/* dts */, "peri_fmem_sub_sel",
		peri_fmem_sub_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 16/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PERI_FMEM_SUB_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 25/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_U_FMEM_SUB_SEL/* dts */, "ufs_fmem_sub_sel",
		ufs_fmem_sub_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 24/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_UFS_FMEM_SUB_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 24/* cksta shift */),
	/* CLK_CFG_2 */
	MUX_CLR_SET_UPD_CHK(CLK_TOP_PEXTP_FMEM_SUB_SEL/* dts */, "pextp_fmem_sub_sel",
		pextp_fmem_sub_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PEXTP_FMEM_SUB_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 23/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_EMI_N_SEL/* dts */, "emi_n_sel",
		emi_n_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_EMI_N_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 22/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_EMI_S_SEL/* dts */, "emi_s_sel",
		emi_s_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_EMI_S_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 21/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_EMI_SLICE_N_SEL/* dts */, "emi_slice_n_sel",
		emi_slice_n_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_EMI_SLICE_N_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 20/* cksta shift */),
	/* CLK_CFG_3 */
	MUX_CLR_SET_UPD_CHK(CLK_TOP_EMI_SLICE_S_SEL/* dts */, "emi_slice_s_sel",
		emi_slice_s_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_EMI_SLICE_S_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 19/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_AP2CONN_HOST_SEL/* dts */, "ap2conn_host_sel",
		ap2conn_host_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 8/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AP2CONN_HOST_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 18/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_ATB_SEL/* dts */, "atb_sel",
		atb_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_ATB_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 17/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_CIRQ_SEL/* dts */, "cirq_sel",
		cirq_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_CIRQ_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 16/* cksta shift */),
	/* CLK_CFG_4 */
	MUX_CLR_SET_UPD_CHK(CLK_TOP_EFUSE_SEL/* dts */, "efuse_sel",
		efuse_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 0/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_EFUSE_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 15/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_MCU_L3GIC_SEL/* dts */, "mcu_l3gic_sel",
		mcu_l3gic_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MCU_L3GIC_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 14/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_MCU_INFRA_SEL/* dts */, "mcu_infra_sel",
		mcu_infra_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MCU_INFRA_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 13/* cksta shift */),
	/* CLK_CFG_5 */
	MUX_GATE_CLR_SET_UPD_CHK(CLK_TOP_DSP_SEL/* dts */, "dsp_sel",
		dsp_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 0/* lsb */, 3/* width */,
		7/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_DSP_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		11/* cksta shift */),
	MUX_GATE_CLR_SET_UPD_CHK(CLK_TOP_MFG_REF_SEL/* dts */, "mfg_ref_sel",
		mfg_ref_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 8/* lsb */, 1/* width */,
		15/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MFG_REF_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		10/* cksta shift */),
	MUX_GATE_CLR_SET_UPD_CHK(CLK_TOP_MFG_EB_SEL/* dts */, "mfg_eb_sel",
		mfg_eb_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 24/* lsb */, 2/* width */,
		31/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MFG_EB_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		8/* cksta shift */),
	/* CLK_CFG_6 */
	MUX_CLR_SET_UPD_CHK(CLK_TOP_UART_SEL/* dts */, "uart_sel",
		uart_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_UART_SHIFT/* upd shift */,
		CKSTA_REG/* cksta ofs */, 7/* cksta shift */),
	MUX_MULT_HWV(CLK_TOP_SPI0_BCLK_SEL/* dts */, "spi0_b_sel",
		spi0_b_parents/* parent */, CLK_CFG_6,
		CLK_CFG_6_SET, CLK_CFG_6_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, HWV_CG_12_DONE,
		HWV_CG_12_SET, HWV_CG_12_CLR, /* hwv */
		8/* lsb */, 3/* width */,
		15/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SPI0_BCLK_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		6/* cksta shift */),
	MUX_MULT_HWV(CLK_TOP_SPI1_BCLK_SEL/* dts */, "spi1_b_sel",
		spi1_b_parents/* parent */, CLK_CFG_6,
		CLK_CFG_6_SET, CLK_CFG_6_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, HWV_CG_12_DONE,
		HWV_CG_12_SET, HWV_CG_12_CLR, /* hwv */
		16/* lsb */, 3/* width */,
		23/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SPI1_BCLK_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		5/* cksta shift */),
	MUX_MULT_HWV(CLK_TOP_SPI2_BCLK_SEL/* dts */, "spi2_b_sel",
		spi2_b_parents/* parent */, CLK_CFG_6,
		CLK_CFG_6_SET, CLK_CFG_6_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, HWV_CG_12_DONE,
		HWV_CG_12_SET, HWV_CG_12_CLR, /* hwv */
		24/* lsb */, 3/* width */,
		31/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SPI2_BCLK_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		4/* cksta shift */),
	/* CLK_CFG_7 */
	MUX_MULT_HWV(CLK_TOP_SPI3_BCLK_SEL/* dts */, "spi3_b_sel",
		spi3_b_parents/* parent */, CLK_CFG_7,
		CLK_CFG_7_SET, CLK_CFG_7_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, HWV_CG_13_DONE,
		HWV_CG_13_SET, HWV_CG_13_CLR, /* hwv */
		0/* lsb */, 3/* width */,
		7/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SPI3_BCLK_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		3/* cksta shift */),
	MUX_MULT_HWV(CLK_TOP_SPI4_BCLK_SEL/* dts */, "spi4_b_sel",
		spi4_b_parents/* parent */, CLK_CFG_7,
		CLK_CFG_7_SET, CLK_CFG_7_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, HWV_CG_13_DONE,
		HWV_CG_13_SET, HWV_CG_13_CLR, /* hwv */
		8/* lsb */, 3/* width */,
		15/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SPI4_BCLK_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		2/* cksta shift */),
	MUX_MULT_HWV(CLK_TOP_SPI5_BCLK_SEL/* dts */, "spi5_b_sel",
		spi5_b_parents/* parent */, CLK_CFG_7,
		CLK_CFG_7_SET, CLK_CFG_7_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, HWV_CG_13_DONE,
		HWV_CG_13_SET, HWV_CG_13_CLR, /* hwv */
		16/* lsb */, 3/* width */,
		23/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SPI5_BCLK_SHIFT/* upd shift */, CKSTA_REG/* cksta ofs */,
		1/* cksta shift */),
	MUX_MULT_HWV(CLK_TOP_SPI6_BCLK_SEL/* dts */, "spi6_b_sel",
		spi6_b_parents/* parent */, CLK_CFG_7,
		CLK_CFG_7_SET, CLK_CFG_7_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, HWV_CG_13_DONE,
		HWV_CG_13_SET, HWV_CG_13_CLR, /* hwv */
		24/* lsb */, 3/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SPI6_BCLK_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		31/* cksta shift */),
	/* CLK_CFG_8 */
	MUX_MULT_HWV(CLK_TOP_SPI7_BCLK_SEL/* dts */, "spi7_b_sel",
		spi7_b_parents/* parent */, CLK_CFG_8,
		CLK_CFG_8_SET, CLK_CFG_8_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, HWV_CG_14_DONE,
		HWV_CG_14_SET, HWV_CG_14_CLR, /* hwv */
		0/* lsb */, 3/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SPI7_BCLK_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		30/* cksta shift */),
	MUX_GATE_CLR_SET_UPD_CHK(CLK_TOP_MSDC_MACRO_1P_SEL/* dts */, "msdc_macro_1p_sel",
		msdc_macro_1p_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 8/* lsb */, 3/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_MSDC_MACRO_1P_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		29/* cksta shift */),
	MUX_GATE_CLR_SET_UPD_CHK(CLK_TOP_MSDC_MACRO_2P_SEL/* dts */, "msdc_macro_2p_sel",
		msdc_macro_2p_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 16/* lsb */, 3/* width */,
		23/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_MSDC_MACRO_2P_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		28/* cksta shift */),
	MUX_GATE_CLR_SET_UPD_CHK(CLK_TOP_MSDC30_1_SEL/* dts */, "msdc30_1_sel",
		msdc30_1_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 24/* lsb */, 3/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_MSDC30_1_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		27/* cksta shift */),
	/* CLK_CFG_9 */
	MUX_GATE_CLR_SET_UPD_CHK(CLK_TOP_MSDC30_2_SEL/* dts */, "msdc30_2_sel",
		msdc30_2_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 0/* lsb */, 3/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_MSDC30_2_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		26/* cksta shift */),
	MUX_GATE_CLR_SET_UPD_CHK(CLK_TOP_AUD_INTBUS_SEL/* dts */, "aud_intbus_sel",
		aud_intbus_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 8/* lsb */, 2/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AUD_INTBUS_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		25/* cksta shift */),
	MUX_GATE_CLR_SET_UPD_CHK(CLK_TOP_DISP_PWM_SEL/* dts */, "disp_pwm_sel",
		disp_pwm_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 16/* lsb */, 3/* width */,
		23/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_DISP_PWM_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		24/* cksta shift */),
	MUX_GATE_CLR_SET_UPD_CHK(CLK_TOP_USB_TOP_SEL/* dts */, "usb_sel",
		usb_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 24/* lsb */, 1/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_USB_TOP_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		23/* cksta shift */),
	/* CLK_CFG_10 */
	MUX_GATE_CLR_SET_UPD_CHK(CLK_TOP_USB_XHCI_SEL/* dts */, "ssusb_xhci_sel",
		ssusb_xhci_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 0/* lsb */, 1/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SSUSB_XHCI_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		22/* cksta shift */),
	MUX_GATE_CLR_SET_UPD_CHK(CLK_TOP_USB_TOP_1P_SEL/* dts */, "usb_1p_sel",
		usb_1p_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 8/* lsb */, 1/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_USB_TOP_1P_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		21/* cksta shift */),
	MUX_GATE_CLR_SET_UPD_CHK(CLK_TOP_USB_XHCI_1P_SEL/* dts */, "ssusb_xhci_1p_sel",
		ssusb_xhci_1p_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 16/* lsb */, 1/* width */,
		23/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SSUSB_XHCI_1P_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		20/* cksta shift */),
	MUX_GATE_CLR_SET_UPD_CHK(CLK_TOP_I2C_SEL/* dts */, "i2c_sel",
		i2c_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 24/* lsb */, 2/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_I2C_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		19/* cksta shift */),
	/* CLK_CFG_11 */
	MUX_GATE_CLR_SET_UPD_CHK(CLK_TOP_AUD_ENGEN1_SEL/* dts */, "aud_engen1_sel",
		aud_engen1_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 0/* lsb */, 2/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AUD_ENGEN1_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		18/* cksta shift */),
	MUX_GATE_CLR_SET_UPD_CHK(CLK_TOP_AUD_ENGEN2_SEL/* dts */, "aud_engen2_sel",
		aud_engen2_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 8/* lsb */, 2/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AUD_ENGEN2_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		17/* cksta shift */),
	MUX_GATE_CLR_SET_UPD_CHK(CLK_TOP_AES_UFSFDE_SEL/* dts */, "aes_ufsfde_sel",
		aes_ufsfde_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 16/* lsb */, 3/* width */,
		23/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AES_UFSFDE_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		16/* cksta shift */),
	MUX_GATE_CLR_SET_UPD_FLAGS_2_CHK(CLK_TOP_U_SEL/* dts */, "ufs_sel",
		ufs_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 24/* lsb */, 3/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_UFS_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		15/* cksta shift */, MUX_ROUND_CLOSEST),
	/* CLK_CFG_12 */
	MUX_GATE_CLR_SET_UPD_CHK(CLK_TOP_U_MBIST_SEL/* dts */, "ufs_mbist_sel",
		ufs_mbist_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 0/* lsb */, 2/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_UFS_MBIST_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		14/* cksta shift */),
	MUX_GATE_CLR_SET_UPD_CHK(CLK_TOP_PEXTP_MBIST_SEL/* dts */, "pextp_mbist_sel",
		pextp_mbist_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 8/* lsb */, 1/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_PEXTP_MBIST_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		13/* cksta shift */),
	MUX_GATE_CLR_SET_UPD_CHK(CLK_TOP_AUD_1_SEL/* dts */, "aud_1_sel",
		aud_1_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 16/* lsb */, 1/* width */,
		23/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AUD_1_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		12/* cksta shift */),
	MUX_GATE_CLR_SET_UPD_CHK(CLK_TOP_AUD_2_SEL/* dts */, "aud_2_sel",
		aud_2_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 24/* lsb */, 1/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AUD_2_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		11/* cksta shift */),
	/* CLK_CFG_13 */
	MUX_GATE_CLR_SET_UPD_CHK(CLK_TOP_AUDIO_H_SEL/* dts */, "audio_h_sel",
		audio_h_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 0/* lsb */, 2/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AUDIO_H_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		10/* cksta shift */),
	MUX_GATE_CLR_SET_UPD_CHK(CLK_TOP_ADSP_SEL/* dts */, "adsp_sel",
		adsp_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 8/* lsb */, 1/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_ADSP_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		9/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_ADSP_UARTHUB_BCLK_SEL/* dts */, "adsp_uarthub_b_sel",
		adsp_uarthub_b_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_ADSP_UARTHUB_BCLK_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 8/* cksta shift */),
	MUX_GATE_CLR_SET_UPD_CHK(CLK_TOP_DPMAIF_MAIN_SEL/* dts */, "dpmaif_main_sel",
		dpmaif_main_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 24/* lsb */, 4/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_DPMAIF_MAIN_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		7/* cksta shift */),
	/* CLK_CFG_14 */
	MUX_GATE_CLR_SET_UPD_CHK(CLK_TOP_PWM_SEL/* dts */, "pwm_sel",
		pwm_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 0/* lsb */, 2/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_PWM_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		6/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_MCUPM_SEL/* dts */, "mcupm_sel",
		mcupm_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_MCUPM_SHIFT/* upd shift */,
		CKSTA_REG1/* cksta ofs */, 5/* cksta shift */),
	MUX_GATE_CLR_SET_UPD_CHK(CLK_TOP_SFLASH_SEL/* dts */, "sflash_sel",
		sflash_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 16/* lsb */, 2/* width */,
		23/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SFLASH_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		4/* cksta shift */),
	MUX_GATE_CLR_SET_UPD_CHK(CLK_TOP_IPSEAST_SEL/* dts */, "ipseast_sel",
		ipseast_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 24/* lsb */, 3/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_IPSEAST_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		3/* cksta shift */),
	/* CLK_CFG_15 */
	MUX_GATE_CLR_SET_UPD_CHK(CLK_TOP_IPSWEST_SEL/* dts */, "ipswest_sel",
		ipswest_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 0/* lsb */, 3/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_IPSWEST_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		2/* cksta shift */),
	MUX_GATE_CLR_SET_UPD_CHK(CLK_TOP_TL_SEL/* dts */, "tl_sel",
		tl_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 8/* lsb */, 2/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_TL_SHIFT/* upd shift */, CKSTA_REG1/* cksta ofs */,
		1/* cksta shift */),
	MUX_GATE_CLR_SET_UPD_CHK(CLK_TOP_EMI_INTERFACE_546_SEL/* dts */, "md_emi_sel",
		md_emi_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 16/* lsb */, 1/* width */,
		23/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_EMI_INTERFACE_546_SHIFT/* upd shift */, CKSTA_REG2/* cksta ofs */,
		31/* cksta shift */),
	MUX_GATE_CLR_SET_UPD_CHK(CLK_TOP_SDF_SEL/* dts */, "sdf_sel",
		sdf_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 24/* lsb */, 3/* width */,
		31/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_SDF_SHIFT/* upd shift */, CKSTA_REG2/* cksta ofs */,
		30/* cksta shift */),
	/* CLK_CFG_16 */
	MUX_MULT_HWV(CLK_TOP_UARTHUB_BCLK_SEL/* dts */, "uarthub_b_sel",
		uarthub_b_parents/* parent */, CLK_CFG_16,
		CLK_CFG_16_SET, CLK_CFG_16_CLR/* set parent */,
		"hw-voter-regmap" /*comp*/, HWV_CG_15_DONE,
		HWV_CG_15_SET, HWV_CG_15_CLR, /* hwv */
		0/* lsb */, 2/* width */,
		7/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_UARTHUB_BCLK_SHIFT/* upd shift */, CKSTA_REG2/* cksta ofs */,
		29/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_SSR_PKA_SEL/* dts */, "ssr_pka_sel",
		ssr_pka_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SSR_PKA_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 28/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_SSR_DMA_SEL/* dts */, "ssr_dma_sel",
		ssr_dma_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SSR_DMA_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 27/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_SSR_KDF_SEL/* dts */, "ssr_kdf_sel",
		ssr_kdf_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SSR_KDF_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 26/* cksta shift */),
	/* CLK_CFG_17 */
	MUX_CLR_SET_UPD_CHK(CLK_TOP_SSR_RNG_SEL/* dts */, "ssr_rng_sel",
		ssr_rng_parents/* parent */, CLK_CFG_17, CLK_CFG_17_SET,
		CLK_CFG_17_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_SSR_RNG_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 25/* cksta shift */),
	MUX_GATE_CLR_SET_UPD_CHK(CLK_TOP_SSR_SEJ_SEL/* dts */, "ssr_sej_sel",
		ssr_sej_parents/* parent */, CLK_CFG_17, CLK_CFG_17_SET,
		CLK_CFG_17_CLR/* set parent */, 8/* lsb */, 2/* width */,
		15/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_SSR_SEJ_SHIFT/* upd shift */, CKSTA_REG2/* cksta ofs */,
		24/* cksta shift */),
	/* CLK_CFG_18 */
	MUX_CLR_SET_UPD_CHK(CLK_TOP_DXCC_SEL/* dts */, "dxcc_sel",
		dxcc_parents/* parent */, CLK_CFG_18, CLK_CFG_18_SET,
		CLK_CFG_18_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_DXCC_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 21/* cksta shift */),
	MUX_CLR_SET_UPD_CHK(CLK_TOP_DPSW_CMP_26M_SEL/* dts */, "dpsw_cmp_26m_sel",
		dpsw_cmp_26m_parents/* parent */, CLK_CFG_18, CLK_CFG_18_SET,
		CLK_CFG_18_CLR/* set parent */, 8/* lsb */, 1/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_DPSW_CMP_26M_SHIFT/* upd shift */,
		CKSTA_REG2/* cksta ofs */, 20/* cksta shift */),
	/* CKSYS2_CLK_CFG_0 */
	MUX_MULT_HWV_FLAGS(CLK_TOP_CAMTG1_SEL/* dts */, "camtg1_sel",
		camtg1_parents/* parent */, CKSYS2_CLK_CFG_0,
		CKSYS2_CLK_CFG_0_SET, CKSYS2_CLK_CFG_0_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_20_DONE,
		MM_HW_CCF_HW_CCF_20_SET, MM_HW_CCF_HW_CCF_20_CLR, /* hwv */
		0/* lsb */, 4/* width */,
		7/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAMTG1_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		31/* cksta shift */, CLK_EN_MM_INFRA_PWR),
	MUX_MULT_HWV_FLAGS(CLK_TOP_CAMTG2_SEL/* dts */, "camtg2_sel",
		camtg2_parents/* parent */, CKSYS2_CLK_CFG_0,
		CKSYS2_CLK_CFG_0_SET, CKSYS2_CLK_CFG_0_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_20_DONE,
		MM_HW_CCF_HW_CCF_20_SET, MM_HW_CCF_HW_CCF_20_CLR, /* hwv */
		8/* lsb */, 4/* width */,
		15/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAMTG2_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		30/* cksta shift */, CLK_EN_MM_INFRA_PWR),
	MUX_MULT_HWV_FLAGS(CLK_TOP_CAMTG3_SEL/* dts */, "camtg3_sel",
		camtg3_parents/* parent */, CKSYS2_CLK_CFG_0,
		CKSYS2_CLK_CFG_0_SET, CKSYS2_CLK_CFG_0_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_20_DONE,
		MM_HW_CCF_HW_CCF_20_SET, MM_HW_CCF_HW_CCF_20_CLR, /* hwv */
		16/* lsb */, 4/* width */,
		23/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAMTG3_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		29/* cksta shift */, CLK_EN_MM_INFRA_PWR),
	MUX_MULT_HWV_FLAGS(CLK_TOP_CAMTG4_SEL/* dts */, "camtg4_sel",
		camtg4_parents/* parent */, CKSYS2_CLK_CFG_0,
		CKSYS2_CLK_CFG_0_SET, CKSYS2_CLK_CFG_0_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_20_DONE,
		MM_HW_CCF_HW_CCF_20_SET, MM_HW_CCF_HW_CCF_20_CLR, /* hwv */
		24/* lsb */, 4/* width */,
		31/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAMTG4_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		28/* cksta shift */, CLK_EN_MM_INFRA_PWR),
	/* CKSYS2_CLK_CFG_1 */
	MUX_MULT_HWV_FLAGS(CLK_TOP_CAMTG5_SEL/* dts */, "camtg5_sel",
		camtg5_parents/* parent */, CKSYS2_CLK_CFG_1,
		CKSYS2_CLK_CFG_1_SET, CKSYS2_CLK_CFG_1_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_21_DONE,
		MM_HW_CCF_HW_CCF_21_SET, MM_HW_CCF_HW_CCF_21_CLR, /* hwv */
		0/* lsb */, 4/* width */,
		7/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAMTG5_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		27/* cksta shift */, CLK_EN_MM_INFRA_PWR),
	MUX_MULT_HWV_FLAGS(CLK_TOP_CAMTG6_SEL/* dts */, "camtg6_sel",
		camtg6_parents/* parent */, CKSYS2_CLK_CFG_1,
		CKSYS2_CLK_CFG_1_SET, CKSYS2_CLK_CFG_1_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_21_DONE,
		MM_HW_CCF_HW_CCF_21_SET, MM_HW_CCF_HW_CCF_21_CLR, /* hwv */
		8/* lsb */, 4/* width */,
		15/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAMTG6_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		26/* cksta shift */, CLK_EN_MM_INFRA_PWR),
	MUX_MULT_HWV_FLAGS(CLK_TOP_CAMTG7_SEL/* dts */, "camtg7_sel",
		camtg7_parents/* parent */, CKSYS2_CLK_CFG_1,
		CKSYS2_CLK_CFG_1_SET, CKSYS2_CLK_CFG_1_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_21_DONE,
		MM_HW_CCF_HW_CCF_21_SET, MM_HW_CCF_HW_CCF_21_CLR, /* hwv */
		16/* lsb */, 4/* width */,
		23/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAMTG7_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		25/* cksta shift */, CLK_EN_MM_INFRA_PWR),
	MUX_MULT_HWV_FLAGS(CLK_TOP_SENINF0_SEL/* dts */, "seninf0_sel",
		seninf0_parents/* parent */, CKSYS2_CLK_CFG_1,
		CKSYS2_CLK_CFG_1_SET, CKSYS2_CLK_CFG_1_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_21_DONE,
		MM_HW_CCF_HW_CCF_21_SET, MM_HW_CCF_HW_CCF_21_CLR, /* hwv */
		24/* lsb */, 4/* width */,
		31/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SENINF0_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		24/* cksta shift */, CLK_EN_MM_INFRA_PWR),
	/* CKSYS2_CLK_CFG_2 */
	MUX_MULT_HWV_FLAGS(CLK_TOP_SENINF1_SEL/* dts */, "seninf1_sel",
		seninf1_parents/* parent */, CKSYS2_CLK_CFG_2,
		CKSYS2_CLK_CFG_2_SET, CKSYS2_CLK_CFG_2_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_22_DONE,
		MM_HW_CCF_HW_CCF_22_SET, MM_HW_CCF_HW_CCF_22_CLR, /* hwv */
		0/* lsb */, 4/* width */,
		7/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SENINF1_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		23/* cksta shift */, CLK_EN_MM_INFRA_PWR),
	MUX_MULT_HWV_FLAGS(CLK_TOP_SENINF2_SEL/* dts */, "seninf2_sel",
		seninf2_parents/* parent */, CKSYS2_CLK_CFG_2,
		CKSYS2_CLK_CFG_2_SET, CKSYS2_CLK_CFG_2_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_22_DONE,
		MM_HW_CCF_HW_CCF_22_SET, MM_HW_CCF_HW_CCF_22_CLR, /* hwv */
		8/* lsb */, 4/* width */,
		15/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SENINF2_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		22/* cksta shift */, CLK_EN_MM_INFRA_PWR),
	MUX_MULT_HWV_FLAGS(CLK_TOP_SENINF3_SEL/* dts */, "seninf3_sel",
		seninf3_parents/* parent */, CKSYS2_CLK_CFG_2,
		CKSYS2_CLK_CFG_2_SET, CKSYS2_CLK_CFG_2_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_22_DONE,
		MM_HW_CCF_HW_CCF_22_SET, MM_HW_CCF_HW_CCF_22_CLR, /* hwv */
		16/* lsb */, 4/* width */,
		23/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SENINF3_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		21/* cksta shift */, CLK_EN_MM_INFRA_PWR),
	MUX_MULT_HWV_FLAGS(CLK_TOP_SENINF4_SEL/* dts */, "seninf4_sel",
		seninf4_parents/* parent */, CKSYS2_CLK_CFG_2,
		CKSYS2_CLK_CFG_2_SET, CKSYS2_CLK_CFG_2_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_22_DONE,
		MM_HW_CCF_HW_CCF_22_SET, MM_HW_CCF_HW_CCF_22_CLR, /* hwv */
		24/* lsb */, 4/* width */,
		31/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SENINF4_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		20/* cksta shift */, CLK_EN_MM_INFRA_PWR),
	/* CKSYS2_CLK_CFG_3 */
	MUX_MULT_HWV_FLAGS(CLK_TOP_SENINF5_SEL/* dts */, "seninf5_sel",
		seninf5_parents/* parent */, CKSYS2_CLK_CFG_3,
		CKSYS2_CLK_CFG_3_SET, CKSYS2_CLK_CFG_3_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_23_DONE,
		MM_HW_CCF_HW_CCF_23_SET, MM_HW_CCF_HW_CCF_23_CLR, /* hwv */
		0/* lsb */, 4/* width */,
		7/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SENINF5_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		19/* cksta shift */, CLK_EN_MM_INFRA_PWR),
	MUX_MULT_HWV_FLAGS(CLK_TOP_CCU_AHB_SEL/* dts */, "ccu_ahb_sel",
		ccu_ahb_parents/* parent */, CKSYS2_CLK_CFG_3,
		CKSYS2_CLK_CFG_3_SET, CKSYS2_CLK_CFG_3_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_23_DONE,
		MM_HW_CCF_HW_CCF_23_SET, MM_HW_CCF_HW_CCF_23_CLR, /* hwv */
		8/* lsb */, 2/* width */,
		15/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CCU_AHB_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		18/* cksta shift */, CLK_EN_MM_INFRA_PWR),
	MUX_MULT_DFS_HWV_FLAGS(CLK_TOP_IMG1_SEL/* dts */, "img1_sel",
		img1_parents/* parent */, CKSYS2_CLK_CFG_3,
		CKSYS2_CLK_CFG_3_SET, CKSYS2_CLK_CFG_3_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_30_DONE,
		MM_HW_CCF_HW_CCF_30_SET, MM_HW_CCF_HW_CCF_30_CLR, /* hwv */
		16/* lsb */, 4/* width */,
		23/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_IMG1_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		17/* cksta shift */, CLK_EN_MM_INFRA_PWR),
	MUX_MULT_DFS_HWV_FLAGS(CLK_TOP_IPE_SEL/* dts */, "ipe_sel",
		ipe_parents/* parent */, CKSYS2_CLK_CFG_3,
		CKSYS2_CLK_CFG_3_SET, CKSYS2_CLK_CFG_3_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_30_DONE,
		MM_HW_CCF_HW_CCF_30_SET, MM_HW_CCF_HW_CCF_30_CLR, /* hwv */
		24/* lsb */, 4/* width */,
		31/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_IPE_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		16/* cksta shift */, CLK_EN_MM_INFRA_PWR),
	/* CKSYS2_CLK_CFG_4 */
	MUX_MULT_DFS_HWV_FLAGS(CLK_TOP_CAM_SEL/* dts */, "cam_sel",
		cam_parents/* parent */, CKSYS2_CLK_CFG_4,
		CKSYS2_CLK_CFG_4_SET, CKSYS2_CLK_CFG_4_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_31_DONE,
		MM_HW_CCF_HW_CCF_31_SET, MM_HW_CCF_HW_CCF_31_CLR, /* hwv */
		0/* lsb */, 4/* width */,
		7/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAM_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		15/* cksta shift */, CLK_EN_MM_INFRA_PWR),
	MUX_MULT_HWV_FLAGS(CLK_TOP_CAMTM_SEL/* dts */, "camtm_sel",
		camtm_parents/* parent */, CKSYS2_CLK_CFG_4,
		CKSYS2_CLK_CFG_4_SET, CKSYS2_CLK_CFG_4_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_24_DONE,
		MM_HW_CCF_HW_CCF_24_SET, MM_HW_CCF_HW_CCF_24_CLR, /* hwv */
		8/* lsb */, 3/* width */,
		15/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAMTM_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		14/* cksta shift */, CLK_EN_MM_INFRA_PWR),
	MUX_MULT_DFS_HWV_FLAGS(CLK_TOP_DPE_SEL/* dts */, "dpe_sel",
		dpe_parents/* parent */, CKSYS2_CLK_CFG_4,
		CKSYS2_CLK_CFG_4_SET, CKSYS2_CLK_CFG_4_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_31_DONE,
		MM_HW_CCF_HW_CCF_31_SET, MM_HW_CCF_HW_CCF_31_CLR, /* hwv */
		16/* lsb */, 4/* width */,
		23/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_DPE_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		13/* cksta shift */, CLK_EN_MM_INFRA_PWR),
	MUX_MULT_DFS_HWV_FLAGS(CLK_TOP_VDEC_SEL/* dts */, "vdec_sel",
		vdec_parents/* parent */, CKSYS2_CLK_CFG_4,
		CKSYS2_CLK_CFG_4_SET, CKSYS2_CLK_CFG_4_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_31_DONE,
		MM_HW_CCF_HW_CCF_31_SET, MM_HW_CCF_HW_CCF_31_CLR, /* hwv */
		24/* lsb */, 4/* width */,
		31/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_VDEC_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		12/* cksta shift */, CLK_EN_MM_INFRA_PWR),
	/* CKSYS2_CLK_CFG_5 */
	MUX_MULT_HWV_FLAGS(CLK_TOP_CCUSYS_SEL/* dts */, "ccusys_sel",
		ccusys_parents/* parent */, CKSYS2_CLK_CFG_5,
		CKSYS2_CLK_CFG_5_SET, CKSYS2_CLK_CFG_5_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_25_DONE,
		MM_HW_CCF_HW_CCF_25_SET, MM_HW_CCF_HW_CCF_25_CLR, /* hwv */
		0/* lsb */, 4/* width */,
		7/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CCUSYS_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		11/* cksta shift */, CLK_EN_MM_INFRA_PWR),
	MUX_MULT_HWV_FLAGS(CLK_TOP_CCUTM_SEL/* dts */, "ccutm_sel",
		ccutm_parents/* parent */, CKSYS2_CLK_CFG_5,
		CKSYS2_CLK_CFG_5_SET, CKSYS2_CLK_CFG_5_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_25_DONE,
		MM_HW_CCF_HW_CCF_25_SET, MM_HW_CCF_HW_CCF_25_CLR, /* hwv */
		8/* lsb */, 3/* width */,
		15/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CCUTM_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		10/* cksta shift */, CLK_EN_MM_INFRA_PWR),
	MUX_MULT_DFS_HWV_FLAGS(CLK_TOP_VENC_SEL/* dts */, "venc_sel",
		venc_parents/* parent */, CKSYS2_CLK_CFG_5,
		CKSYS2_CLK_CFG_5_SET, CKSYS2_CLK_CFG_5_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_32_DONE,
		MM_HW_CCF_HW_CCF_32_SET, MM_HW_CCF_HW_CCF_32_CLR, /* hwv */
		16/* lsb */, 4/* width */,
		23/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_VENC_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		9/* cksta shift */, CLK_EN_MM_INFRA_PWR),
	MUX_MULT_HWV_FLAGS(CLK_TOP_DP_CORE_SEL/* dts */, "dp_core_sel",
		dp_core_parents/* parent */, CKSYS2_CLK_CFG_5,
		CKSYS2_CLK_CFG_5_SET, CKSYS2_CLK_CFG_5_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_25_DONE,
		MM_HW_CCF_HW_CCF_25_SET, MM_HW_CCF_HW_CCF_25_CLR, /* hwv */
		24/* lsb */, 3/* width */,
		31/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_DP_CORE_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		8/* cksta shift */, CLK_EN_MM_INFRA_PWR),
	/* CKSYS2_CLK_CFG_6 */
	MUX_MULT_HWV_FLAGS(CLK_TOP_DP_SEL/* dts */, "dp_sel",
		dp_parents/* parent */, CKSYS2_CLK_CFG_6,
		CKSYS2_CLK_CFG_6_SET, CKSYS2_CLK_CFG_6_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_26_DONE,
		MM_HW_CCF_HW_CCF_26_SET, MM_HW_CCF_HW_CCF_26_CLR, /* hwv */
		0/* lsb */, 3/* width */,
		7/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_DP_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		7/* cksta shift */, CLK_EN_MM_INFRA_PWR),
	MUX_MULT_DFS_HWV_FLAGS(CLK_TOP_DISP_SEL/* dts */, "disp_sel",
		disp_parents/* parent */, CKSYS2_CLK_CFG_6,
		CKSYS2_CLK_CFG_6_SET, CKSYS2_CLK_CFG_6_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_33_DONE,
		MM_HW_CCF_HW_CCF_33_SET, MM_HW_CCF_HW_CCF_33_CLR, /* hwv */
		8/* lsb */, 4/* width */,
		15/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_DISP_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		6/* cksta shift */, CLK_EN_MM_INFRA_PWR),
	MUX_MULT_DFS_HWV_FLAGS(CLK_TOP_MDP_SEL/* dts */, "mdp_sel",
		mdp_parents/* parent */, CKSYS2_CLK_CFG_6,
		CKSYS2_CLK_CFG_6_SET, CKSYS2_CLK_CFG_6_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_33_DONE,
		MM_HW_CCF_HW_CCF_33_SET, MM_HW_CCF_HW_CCF_33_CLR, /* hwv */
		16/* lsb */, 4/* width */,
		23/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MDP_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		5/* cksta shift */, CLK_EN_MM_INFRA_PWR),
	MUX_MULT_DFS_HWV_DUMMY(CLK_TOP_MMINFRA_SEL/* dts */, "mminfra_sel",
		mminfra_parents/* parent */, CKSYS2_CLK_CFG_6,
		CKSYS2_CLK_CFG_6_SET, CKSYS2_CLK_CFG_6_CLR/* set parent */,
		"mm-hw-ccf-regmap" /*comp*/, MM_HW_CCF_HW_CCF_33_DONE,
		MM_HW_CCF_HW_CCF_33_SET, MM_HW_CCF_HW_CCF_33_CLR, /* hwv */
		24/* lsb */, 4/* width */,
		31/* pdn */, CKSYS2_CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MMINFRA_SHIFT/* upd shift */, CKSYS2_CKSTA_REG/* cksta ofs */,
		4/* cksta shift */, CLK_EN_MM_INFRA_PWR),
	/* CKSYS2_CLK_CFG_7 */
	MUX_CLR_SET_UPD_CHK(CLK_TOP_MMUP_SEL/* dts */, "mmup_sel",
		mmup_parents/* parent */, CKSYS2_CLK_CFG_7, CKSYS2_CLK_CFG_7_SET,
		CKSYS2_CLK_CFG_7_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CKSYS2_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MMUP_SHIFT/* upd shift */,
		CKSYS2_CKSTA_REG/* cksta ofs */, 3/* cksta shift */),
#endif
};

static const struct mtk_composite top_composites[] = {
	/* CLK_AUDDIV_0 */
	MUX(CLK_TOP_APLL_I2SIN0_MCK_SEL/* dts */, "apll_i2sin0_m_sel",
		apll_i2sin0_m_parents/* parent */, 0x0320/* ofs */,
		16/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_I2SIN1_MCK_SEL/* dts */, "apll_i2sin1_m_sel",
		apll_i2sin1_m_parents/* parent */, 0x0320/* ofs */,
		17/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_I2SIN2_MCK_SEL/* dts */, "apll_i2sin2_m_sel",
		apll_i2sin2_m_parents/* parent */, 0x0320/* ofs */,
		18/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_I2SIN3_MCK_SEL/* dts */, "apll_i2sin3_m_sel",
		apll_i2sin3_m_parents/* parent */, 0x0320/* ofs */,
		19/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_I2SIN4_MCK_SEL/* dts */, "apll_i2sin4_m_sel",
		apll_i2sin4_m_parents/* parent */, 0x0320/* ofs */,
		20/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_I2SIN6_MCK_SEL/* dts */, "apll_i2sin6_m_sel",
		apll_i2sin6_m_parents/* parent */, 0x0320/* ofs */,
		21/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_I2SOUT0_MCK_SEL/* dts */, "apll_i2sout0_m_sel",
		apll_i2sout0_m_parents/* parent */, 0x0320/* ofs */,
		22/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_I2SOUT1_MCK_SEL/* dts */, "apll_i2sout1_m_sel",
		apll_i2sout1_m_parents/* parent */, 0x0320/* ofs */,
		23/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_I2SOUT2_MCK_SEL/* dts */, "apll_i2sout2_m_sel",
		apll_i2sout2_m_parents/* parent */, 0x0320/* ofs */,
		24/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_I2SOUT3_MCK_SEL/* dts */, "apll_i2sout3_m_sel",
		apll_i2sout3_m_parents/* parent */, 0x0320/* ofs */,
		25/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_I2SOUT4_MCK_SEL/* dts */, "apll_i2sout4_m_sel",
		apll_i2sout4_m_parents/* parent */, 0x0320/* ofs */,
		26/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_I2SOUT6_MCK_SEL/* dts */, "apll_i2sout6_m_sel",
		apll_i2sout6_m_parents/* parent */, 0x0320/* ofs */,
		27/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_FMI2S_MCK_SEL/* dts */, "apll_fmi2s_m_sel",
		apll_fmi2s_m_parents/* parent */, 0x0320/* ofs */,
		28/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_TDMOUT_MCK_SEL/* dts */, "apll_tdmout_m_sel",
		apll_tdmout_m_parents/* parent */, 0x0320/* ofs */,
		29/* lsb */, 1/* width */),
	/* CLK_AUDDIV_2 */
	DIV_GATE(CLK_TOP_APLL12_CK_DIV_I2SIN0/* dts */, "apll12_div_i2sin0"/* ccf */,
		"apll_i2sin0_m_sel"/* parent */, 0x0320/* pdn ofs */,
		0/* pdn bit */, CLK_AUDDIV_2/* ofs */, 8/* width */,
		0/* lsb */),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV_I2SIN1/* dts */, "apll12_div_i2sin1"/* ccf */,
		"apll_i2sin1_m_sel"/* parent */, 0x0320/* pdn ofs */,
		1/* pdn bit */, CLK_AUDDIV_2/* ofs */, 8/* width */,
		8/* lsb */),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV_I2SIN2/* dts */, "apll12_div_i2sin2"/* ccf */,
		"apll_i2sin2_m_sel"/* parent */, 0x0320/* pdn ofs */,
		2/* pdn bit */, CLK_AUDDIV_2/* ofs */, 8/* width */,
		16/* lsb */),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV_I2SIN3/* dts */, "apll12_div_i2sin3"/* ccf */,
		"apll_i2sin3_m_sel"/* parent */, 0x0320/* pdn ofs */,
		3/* pdn bit */, CLK_AUDDIV_2/* ofs */, 8/* width */,
		24/* lsb */),
	/* CLK_AUDDIV_3 */
	DIV_GATE(CLK_TOP_APLL12_CK_DIV_I2SIN4/* dts */, "apll12_div_i2sin4"/* ccf */,
		"apll_i2sin4_m_sel"/* parent */, 0x0320/* pdn ofs */,
		4/* pdn bit */, CLK_AUDDIV_3/* ofs */, 8/* width */,
		0/* lsb */),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV_I2SIN6/* dts */, "apll12_div_i2sin6"/* ccf */,
		"apll_i2sin6_m_sel"/* parent */, 0x0320/* pdn ofs */,
		5/* pdn bit */, CLK_AUDDIV_3/* ofs */, 8/* width */,
		8/* lsb */),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV_I2SOUT0/* dts */, "apll12_div_i2sout0"/* ccf */,
		"apll_i2sout0_m_sel"/* parent */, 0x0320/* pdn ofs */,
		6/* pdn bit */, CLK_AUDDIV_3/* ofs */, 8/* width */,
		16/* lsb */),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV_I2SOUT1/* dts */, "apll12_div_i2sout1"/* ccf */,
		"apll_i2sout1_m_sel"/* parent */, 0x0320/* pdn ofs */,
		7/* pdn bit */, CLK_AUDDIV_3/* ofs */, 8/* width */,
		24/* lsb */),
	/* CLK_AUDDIV_4 */
	DIV_GATE(CLK_TOP_APLL12_CK_DIV_I2SOUT2/* dts */, "apll12_div_i2sout2"/* ccf */,
		"apll_i2sout2_m_sel"/* parent */, 0x0320/* pdn ofs */,
		8/* pdn bit */, CLK_AUDDIV_4/* ofs */, 8/* width */,
		0/* lsb */),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV_I2SOUT3/* dts */, "apll12_div_i2sout3"/* ccf */,
		"apll_i2sout3_m_sel"/* parent */, 0x0320/* pdn ofs */,
		9/* pdn bit */, CLK_AUDDIV_4/* ofs */, 8/* width */,
		8/* lsb */),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV_I2SOUT4/* dts */, "apll12_div_i2sout4"/* ccf */,
		"apll_i2sout4_m_sel"/* parent */, 0x0320/* pdn ofs */,
		10/* pdn bit */, CLK_AUDDIV_4/* ofs */, 8/* width */,
		16/* lsb */),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV_I2SOUT6/* dts */, "apll12_div_i2sout6"/* ccf */,
		"apll_i2sout6_m_sel"/* parent */, 0x0320/* pdn ofs */,
		11/* pdn bit */, CLK_AUDDIV_4/* ofs */, 8/* width */,
		24/* lsb */),
	/* CLK_AUDDIV_5 */
	DIV_GATE(CLK_TOP_APLL12_CK_DIV_FMI2S/* dts */, "apll12_div_fmi2s"/* ccf */,
		"apll_fmi2s_m_sel"/* parent */, 0x0320/* pdn ofs */,
		12/* pdn bit */, CLK_AUDDIV_5/* ofs */, 8/* width */,
		0/* lsb */),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV_TDMOUT_M/* dts */, "apll12_div_tdmout_m"/* ccf */,
		"apll_tdmout_m_sel"/* parent */, 0x0320/* pdn ofs */,
		13/* pdn bit */, CLK_AUDDIV_5/* ofs */, 8/* width */,
		8/* lsb */),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV_TDMOUT_B/* dts */, "apll12_div_tdmout_b"/* ccf */,
		"apll_tdmout_m_sel"/* parent */, 0x0320/* pdn ofs */,
		14/* pdn bit */, CLK_AUDDIV_5/* ofs */, 8/* width */,
		16/* lsb */),
};


enum subsys_id {
	CCIPLL_PLL_CTRL = 0,
	APMIXEDSYS = 1,
	MFGPLL_PLL_CTRL = 2,
	PTPPLL_PLL_CTRL = 3,
	ARMPLL_BL_PLL_CTRL = 4,
	MFGPLL_SC0_PLL_CTRL = 5,
	ARMPLL_B_PLL_CTRL = 6,
	ARMPLL_LL_PLL_CTRL = 7,
	MFGPLL_SC1_PLL_CTRL = 8,
	PLL_SYS_NUM,
};

static const struct mtk_pll_data *plls_data[PLL_SYS_NUM];
static void __iomem *plls_base[PLL_SYS_NUM];

#define MT6989_PLL_FMAX		(3800UL * MHZ)
#define MT6989_PLL_FMIN		(1500UL * MHZ)
#define MT6989_INTEGER_BITS	8

#if MT_CCF_PLL_DISABLE
#define PLL_CFLAGS		PLL_AO
#else
#define PLL_CFLAGS		(0)
#endif

#define PLL(_id, _name, _reg, _en_reg, _en_mask, _pll_en_bit,		\
			_flags, _rst_bar_mask,				\
			_pd_reg, _pd_shift, _tuner_reg,			\
			_tuner_en_reg, _tuner_en_bit,			\
			_pcw_reg, _pcw_shift, _pcwbits) {		\
		.id = _id,						\
		.name = _name,						\
		.reg = _reg,						\
		.en_reg = _en_reg,					\
		.en_mask = _en_mask,					\
		.pll_en_bit = _pll_en_bit,				\
		.flags = (_flags | PLL_CFLAGS),				\
		.rst_bar_mask = _rst_bar_mask,				\
		.fmax = MT6989_PLL_FMAX,				\
		.fmin = MT6989_PLL_FMIN,				\
		.pd_reg = _pd_reg,					\
		.pd_shift = _pd_shift,					\
		.tuner_reg = _tuner_reg,				\
		.tuner_en_reg = _tuner_en_reg,			\
		.tuner_en_bit = _tuner_en_bit,				\
		.pcw_reg = _pcw_reg,					\
		.pcw_shift = _pcw_shift,				\
		.pcwbits = _pcwbits,					\
		.pcwibits = MT6989_INTEGER_BITS,			\
	}

#define PLL_SETCLR(_id, _name, _pll_setclr, _en_setclr_bit,		\
			_rstb_setclr_bit, _flags, _pd_reg,		\
			_pd_shift, _tuner_reg, _tuner_en_reg,		\
			_tuner_en_bit, _pcw_reg, _pcw_shift,		\
			_pcwbits) {					\
		.id = _id,						\
		.name = _name,						\
		.reg = 0,						\
		.pll_setclr = &(_pll_setclr),				\
		.en_setclr_bit = _en_setclr_bit,			\
		.rstb_setclr_bit = _rstb_setclr_bit,			\
		.flags = (_flags | PLL_CFLAGS),				\
		.fmax = MT6989_PLL_FMAX,				\
		.fmin = MT6989_PLL_FMIN,				\
		.pd_reg = _pd_reg,					\
		.pd_shift = _pd_shift,					\
		.tuner_reg = _tuner_reg,				\
		.tuner_en_reg = _tuner_en_reg,			\
		.tuner_en_bit = _tuner_en_bit,				\
		.pcw_reg = _pcw_reg,					\
		.pcw_shift = _pcw_shift,				\
		.pcwbits = _pcwbits,					\
		.pcwibits = MT6989_INTEGER_BITS,			\
	}

#define PLL_SETCLR_R(_id, _name, _pll_setclr, _hwv_comp,		\
			_en_setclr_bit, _rstb_setclr_bit, _flags,	\
			_pd_reg, _pd_shift, _tuner_reg, _tuner_en_reg,	\
			_tuner_en_bit, _pcw_reg, _pcw_shift,		\
			_pcwbits, _hwv_res_set_ofs, _hwv_res_clr_ofs,	\
			_hwv_done_ofs, _hwv_set_sta_ofs,		\
			_hwv_clr_sta_ofs, _hwv_shift) {			\
		.id = _id,						\
		.name = _name,						\
		.pll_setclr = &(_pll_setclr),				\
		.hwv_comp = _hwv_comp,					\
		.en_setclr_bit = _en_setclr_bit,			\
		.rstb_setclr_bit = _rstb_setclr_bit,			\
		.flags = (_flags | PLL_CFLAGS),				\
		.fmax = MT6989_PLL_FMAX,				\
		.fmin = MT6989_PLL_FMIN,				\
		.pd_reg = _pd_reg,					\
		.pd_shift = _pd_shift,					\
		.tuner_reg = _tuner_reg,				\
		.tuner_en_reg = _tuner_en_reg,				\
		.tuner_en_bit = _tuner_en_bit,				\
		.pcw_reg = _pcw_reg,					\
		.pcw_shift = _pcw_shift,				\
		.pcwbits = _pcwbits,					\
		.pcwibits = MT6989_INTEGER_BITS,			\
		.hwv_res_set_ofs = _hwv_res_set_ofs,			\
		.hwv_res_clr_ofs = _hwv_res_clr_ofs,			\
		.hwv_done_ofs = _hwv_done_ofs,				\
		.hwv_set_sta_ofs = _hwv_set_sta_ofs,			\
		.hwv_clr_sta_ofs = _hwv_clr_sta_ofs,			\
		.hwv_shift = _hwv_shift,				\
	}

#define PLL_HWV_SETCLR(_id, _name, _pll_setclr, _hwv_comp,		\
			_hwv_sta_ofs, _hwv_set_ofs, _hwv_clr_ofs,	\
			_en_setclr_bit, _rstb_setclr_bit, _flags,	\
			_pd_reg, _pd_shift, _tuner_reg, _tuner_en_reg,	\
			_tuner_en_bit, _pcw_reg, _pcw_shift,		\
			_pcwbits) {					\
		.id = _id,						\
		.name = _name,						\
		.pll_setclr = &(_pll_setclr),				\
		.hwv_comp = _hwv_comp,					\
		.hwv_set_ofs = _hwv_set_ofs,				\
		.hwv_clr_ofs = _hwv_clr_ofs,				\
		.hwv_sta_ofs = _hwv_sta_ofs,				\
		.en_setclr_bit = _en_setclr_bit,			\
		.rstb_setclr_bit = _rstb_setclr_bit,			\
		.flags = (_flags | PLL_CFLAGS | CLK_USE_HW_VOTER),	\
		.fmax = MT6989_PLL_FMAX,				\
		.fmin = MT6989_PLL_FMIN,				\
		.pd_reg = _pd_reg,					\
		.pd_shift = _pd_shift,					\
		.tuner_reg = _tuner_reg,				\
		.tuner_en_reg = _tuner_en_reg,				\
		.tuner_en_bit = _tuner_en_bit,				\
		.pcw_reg = _pcw_reg,					\
		.pcw_shift = _pcw_shift,				\
		.pcwbits = _pcwbits,					\
		.pcwibits = MT6989_INTEGER_BITS,			\
	}

#define PLL_HWV_SETCLR_R(_id, _name, _pll_setclr, _hwv_comp,		\
			_hwv_sta_ofs, _hwv_set_ofs, _hwv_clr_ofs,	\
			_en_setclr_bit, _rstb_setclr_bit, _flags,	\
			_pd_reg, _pd_shift, _tuner_reg, _tuner_en_reg,	\
			_tuner_en_bit, _pcw_reg, _pcw_shift,		\
			_pcwbits, _hwv_res_set_ofs, _hwv_res_clr_ofs,	\
			_hwv_done_ofs, _hwv_set_sta_ofs,		\
			_hwv_clr_sta_ofs, _hwv_shift) {	\
		.id = _id,						\
		.name = _name,						\
		.pll_setclr = &(_pll_setclr),				\
		.hwv_comp = _hwv_comp,					\
		.hwv_set_ofs = _hwv_set_ofs,				\
		.hwv_clr_ofs = _hwv_clr_ofs,				\
		.hwv_sta_ofs = _hwv_sta_ofs,				\
		.en_setclr_bit = _en_setclr_bit,			\
		.rstb_setclr_bit = _rstb_setclr_bit,			\
		.flags = (_flags | PLL_CFLAGS | CLK_USE_HW_VOTER),	\
		.fmax = MT6989_PLL_FMAX,				\
		.fmin = MT6989_PLL_FMIN,				\
		.pd_reg = _pd_reg,					\
		.pd_shift = _pd_shift,					\
		.tuner_reg = _tuner_reg,				\
		.tuner_en_reg = _tuner_en_reg,				\
		.tuner_en_bit = _tuner_en_bit,				\
		.pcw_reg = _pcw_reg,					\
		.pcw_shift = _pcw_shift,				\
		.pcwbits = _pcwbits,					\
		.pcwibits = MT6989_INTEGER_BITS,			\
		.hwv_res_set_ofs = _hwv_res_set_ofs,			\
		.hwv_res_clr_ofs = _hwv_res_clr_ofs,			\
		.hwv_done_ofs = _hwv_done_ofs,				\
		.hwv_set_sta_ofs = _hwv_set_sta_ofs,			\
		.hwv_clr_sta_ofs = _hwv_clr_sta_ofs,			\
		.hwv_shift = _hwv_shift,				\
	}

static const struct mtk_pll_data cci_plls[] = {
	PLL(CLK_CCIPLL, "ccipll", CCIPLL_CON0/*base*/,
		CCIPLL_CON0, 0, 0/*en*/,
		PLL_AO, BIT(0)/*rstb*/,
		CCIPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		CCIPLL_CON1, 0, 22/*pcw*/),
};

static struct mtk_pll_setclr_data setclr_data = {
	.en_ofs = 0x0914,
	.en_set_ofs = 0x0918,
	.en_clr_ofs = 0x091c,
	.rstb_ofs = 0x0920,
	.rstb_set_ofs = 0x0924,
	.rstb_clr_ofs = 0x0928,
};

static const struct mtk_pll_data apmixed_plls[] = {
	PLL_HWV_SETCLR(CLK_APMIXED_MAINPLL, "mainpll", setclr_data/*base*/,
		"hw-voter-regmap"/*comp*/, HWV_CG_17_DONE,
		HWV_CG_17_SET, HWV_CG_17_CLR, /* hwv */
		8, 1, HAVE_RST_BAR | PLL_AO,
		MAINPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		MAINPLL_CON1, 0, 22/*pcw*/),
	PLL_HWV_SETCLR_R(CLK_APMIXED_UNIVPLL, "univpll", setclr_data/*base*/,
		"hw-voter-regmap"/*comp*/, HWV_CG_17_DONE,
		HWV_CG_17_SET, HWV_CG_17_CLR, /* hwv */
		3, 2, HAVE_RST_BAR,
		UNIVPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		UNIVPLL_CON1, 0, 22/*pcw*/,
		HWV_B0_SET, HWV_B0_CLR, HWV_B0_DONE,
		HWV_B0_SET_STA, HWV_B0_CLR_STA, 0),
	PLL_SETCLR(CLK_APMIXED_MSDCPLL, "msdcpll", setclr_data/*base*/,
		5, 0, 0,
		MSDCPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		MSDCPLL_CON1, 0, 22/*pcw*/),
	PLL_SETCLR(CLK_APMIXED_MMPLL, "mmpll", setclr_data/*base*/,
		7, 0, HAVE_RST_BAR,
		MMPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		MMPLL_CON1, 0, 22/*pcw*/),
	PLL_SETCLR(CLK_APMIXED_ADSPPLL, "adsppll", setclr_data/*base*/,
		14, 0, 0,
		ADSPPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		ADSPPLL_CON1, 0, 22/*pcw*/),
	PLL_SETCLR(CLK_APMIXED_APLL1, "apll1", setclr_data/*base*/,
		1, 0, 0,
		APLL1_CON1, 24/*pd*/,
		APLL1_TUNER_CON0, AP_PLL_CON3, 0/*tuner*/,
		APLL1_CON2, 0, 32/*pcw*/),
	PLL_SETCLR(CLK_APMIXED_APLL2, "apll2", setclr_data/*base*/,
		0, 0, 0,
		APLL2_CON1, 24/*pd*/,
		APLL2_TUNER_CON0, AP_PLL_CON3, 5/*tuner*/,
		APLL2_CON2, 0, 32/*pcw*/),
	PLL_SETCLR(CLK_APMIXED_EMIPLL, "emipll", setclr_data/*base*/,
		9, 0, PLL_AO,
		EMIPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		EMIPLL_CON1, 0, 22/*pcw*/),
	PLL_SETCLR(CLK_APMIXED_EMIPLL2, "emipll2", setclr_data/*base*/,
		2, 0, PLL_AO,
		EMIPLL2_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		EMIPLL2_CON1, 0, 22/*pcw*/),
	PLL_HWV_SETCLR(CLK_APMIXED_MAINPLL2, "mainpll2", setclr_data/*base*/,
		"mm-hw-ccf-regmap"/*comp*/, MM_HW_CCF_HW_CCF_27_DONE,
		MM_HW_CCF_HW_CCF_27_SET, MM_HW_CCF_HW_CCF_27_CLR, /* hwv */
		13, 6, HAVE_RST_BAR | CLK_EN_MM_INFRA_PWR,
		MAINPLL2_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		MAINPLL2_CON1, 0, 22/*pcw*/),
	PLL_HWV_SETCLR(CLK_APMIXED_UNIVPLL2, "univpll2", setclr_data/*base*/,
		"mm-hw-ccf-regmap"/*comp*/, MM_HW_CCF_HW_CCF_27_DONE,
		MM_HW_CCF_HW_CCF_27_SET, MM_HW_CCF_HW_CCF_27_CLR, /* hwv */
		12, 5, HAVE_RST_BAR | CLK_EN_MM_INFRA_PWR,
		UNIVPLL2_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		UNIVPLL2_CON1, 0, 22/*pcw*/),
	PLL_HWV_SETCLR(CLK_APMIXED_MMPLL2, "mmpll2", setclr_data/*base*/,
		"mm-hw-ccf-regmap"/*comp*/, MM_HW_CCF_HW_CCF_27_DONE,
		MM_HW_CCF_HW_CCF_27_SET, MM_HW_CCF_HW_CCF_27_CLR, /* hwv */
		11, 4, HAVE_RST_BAR | CLK_EN_MM_INFRA_PWR,
		MMPLL2_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		MMPLL2_CON1, 0, 22/*pcw*/),
	PLL_HWV_SETCLR(CLK_APMIXED_TVDPLL, "tvdpll", setclr_data/*base*/,
		"mm-hw-ccf-regmap"/*comp*/, MM_HW_CCF_HW_CCF_27_DONE,
		MM_HW_CCF_HW_CCF_27_SET, MM_HW_CCF_HW_CCF_27_CLR, /* hwv */
		4, 0, CLK_EN_MM_INFRA_PWR,
		TVDPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		TVDPLL_CON1, 0, 22/*pcw*/),
	PLL_HWV_SETCLR(CLK_APMIXED_IMGPLL, "imgpll", setclr_data/*base*/,
		"mm-hw-ccf-regmap"/*comp*/, MM_HW_CCF_HW_CCF_27_DONE,
		MM_HW_CCF_HW_CCF_27_SET, MM_HW_CCF_HW_CCF_27_CLR, /* hwv */
		10, 3, HAVE_RST_BAR | CLK_EN_MM_INFRA_PWR,
		IMGPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		IMGPLL_CON1, 0, 22/*pcw*/),
};

static const struct mtk_pll_data mfg_ao_plls[] = {
	PLL(CLK_MFG_AO_MFGPLL, "mfg-ao-mfgpll", MFGPLL_CON0/*base*/,
		MFGPLL_CON0, 0, 0/*en*/,
		0, BIT(0)/*rstb*/,
		MFGPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		MFGPLL_CON1, 0, 22/*pcw*/),
};

static const struct mtk_pll_data ptp_plls[] = {
	PLL(CLK_PTPPLL, "ptppll", PTPPLL_CON0/*base*/,
		PTPPLL_CON0, 0, 0/*en*/,
		PLL_AO, BIT(0)/*rstb*/,
		PTPPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		PTPPLL_CON1, 0, 22/*pcw*/),
};

static const struct mtk_pll_data cpu_bl_plls[] = {
	PLL(CLK_CPU_BL_ARMPLL_BL, "cpu-bl-armpll-bl", ARMPLL_BL_CON0/*base*/,
		ARMPLL_BL_CON0, 0, 0/*en*/,
		PLL_AO, BIT(0)/*rstb*/,
		ARMPLL_BL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		ARMPLL_BL_CON1, 0, 22/*pcw*/),
};

static const struct mtk_pll_data mfgsc0_ao_plls[] = {
	PLL(CLK_MFGSC0_AO_MFGPLL_SC0, "mfgsc0-ao-mfgpll-sc0", MFGPLL_SC0_CON0/*base*/,
		MFGPLL_SC0_CON0, 0, 0/*en*/,
		0, BIT(0)/*rstb*/,
		MFGPLL_SC0_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		MFGPLL_SC0_CON1, 0, 22/*pcw*/),
};

static const struct mtk_pll_data cpu_b_plls[] = {
	PLL(CLK_CPU_B_ARMPLL_B, "cpu-b-armpll-b", ARMPLL_B_CON0/*base*/,
		ARMPLL_B_CON0, 0, 0/*en*/,
		PLL_AO, BIT(0)/*rstb*/,
		ARMPLL_B_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		ARMPLL_B_CON1, 0, 22/*pcw*/),
};

static const struct mtk_pll_data cpu_ll_plls[] = {
	PLL(CLK_CPU_LL_ARMPLL_LL, "cpu-ll-armpll-ll", ARMPLL_LL_CON0/*base*/,
		ARMPLL_LL_CON0, 0, 0/*en*/,
		PLL_AO, BIT(0)/*rstb*/,
		ARMPLL_LL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		ARMPLL_LL_CON1, 0, 22/*pcw*/),
};

static const struct mtk_pll_data mfgsc1_ao_plls[] = {
	PLL(CLK_MFGSC1_AO_MFGPLL_SC1, "mfgsc1-ao-mfgpll-sc1", MFGPLL_SC1_CON0/*base*/,
		MFGPLL_SC1_CON0, 0, 0/*en*/,
		0, BIT(0)/*rstb*/,
		MFGPLL_SC1_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		MFGPLL_SC1_CON1, 0, 22/*pcw*/),
};

static const struct mtk_hwv_data mminfra_hwv_data = {
	.name = "clk-mm-infra-pwr",
	.set_ofs = 0x0404,
	.clr_ofs = 0x0408,
	.done_ofs = 0x091C,
	.en_ofs = 0x0400,
	.en_shift = CLK_MMINFRA_PWR_VOTE_BIT,
};

static struct regmap *mminfra_hwv_regmap;

static int clk_mt6989_pll_registration(enum subsys_id id,
		const struct mtk_pll_data *plls,
		struct platform_device *pdev,
		int num_plls)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

	void __iomem *base;
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	if (id >= PLL_SYS_NUM) {
		pr_notice("%s init invalid id(%d)\n", __func__, id);
		return 0;
	}

	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base)) {
		pr_err("%s(): ioremap failed\n", __func__);
		return PTR_ERR(base);
	}

	clk_data = mtk_alloc_clk_data(num_plls);
	if (!clk_data)
		return -ENOMEM;

	mtk_clk_register_plls(node, plls, num_plls,
			clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

	plls_data[id] = plls;
	plls_base[id] = base;

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif

	return r;
}

#if IS_ENABLED(CONFIG_MTK_CLKMGR_FTRACE)
static int clk_set_trace_event(struct platform_device *pdev)
{
	trace_set_clr_event(NULL, "clk_disable", 1);
	trace_set_clr_event(NULL, "clk_disable_complete", 1);
	trace_set_clr_event(NULL, "clk_enable", 1);
	trace_set_clr_event(NULL, "clk_enable_complete", 1);
	trace_set_clr_event(NULL, "clk_prepare", 1);
	trace_set_clr_event(NULL, "clk_prepare_complete", 1);
	trace_set_clr_event(NULL, "clk_set_parent", 1);
	trace_set_clr_event(NULL, "clk_set_parent_complete", 1);
	trace_set_clr_event(NULL, "clk_set_rate", 1);
	trace_set_clr_event(NULL, "clk_set_rate_complete", 1);
	trace_set_clr_event(NULL, "clk_unprepare", 1);
	trace_set_clr_event(NULL, "clk_unprepare_complete", 1);
	trace_set_clr_event(NULL, "rpm_idle", 1);
	trace_set_clr_event(NULL, "rpm_resume", 1);
	trace_set_clr_event(NULL, "rpm_return_int", 1);
	trace_set_clr_event(NULL, "rpm_suspend", 1);
	trace_set_clr_event(NULL, "rpm_usage", 1);
	pr_notice("%s init end\n",__func__);

	return 0;
}
#endif

static int clk_mt6989_apmixed_probe(struct platform_device *pdev)
{
	return clk_mt6989_pll_registration(APMIXEDSYS, apmixed_plls,
			pdev, ARRAY_SIZE(apmixed_plls));
}

static int clk_mt6989_cpu_bl_probe(struct platform_device *pdev)
{
	return clk_mt6989_pll_registration(ARMPLL_BL_PLL_CTRL, cpu_bl_plls,
			pdev, ARRAY_SIZE(cpu_bl_plls));
}

static int clk_mt6989_cpu_b_probe(struct platform_device *pdev)
{
	return clk_mt6989_pll_registration(ARMPLL_B_PLL_CTRL, cpu_b_plls,
			pdev, ARRAY_SIZE(cpu_b_plls));
}

static int clk_mt6989_cpu_ll_probe(struct platform_device *pdev)
{
	return clk_mt6989_pll_registration(ARMPLL_LL_PLL_CTRL, cpu_ll_plls,
			pdev, ARRAY_SIZE(cpu_ll_plls));
}

static int clk_mt6989_cci_probe(struct platform_device *pdev)
{
	return clk_mt6989_pll_registration(CCIPLL_PLL_CTRL, cci_plls,
			pdev, ARRAY_SIZE(cci_plls));
}

static int clk_mt6989_mfg_ao_probe(struct platform_device *pdev)
{
	return clk_mt6989_pll_registration(MFGPLL_PLL_CTRL, mfg_ao_plls,
			pdev, ARRAY_SIZE(mfg_ao_plls));
}

static int clk_mt6989_mfgsc0_ao_probe(struct platform_device *pdev)
{
	return clk_mt6989_pll_registration(MFGPLL_SC0_PLL_CTRL, mfgsc0_ao_plls,
			pdev, ARRAY_SIZE(mfgsc0_ao_plls));
}

static int clk_mt6989_mfgsc1_ao_probe(struct platform_device *pdev)
{
	return clk_mt6989_pll_registration(MFGPLL_SC1_PLL_CTRL, mfgsc1_ao_plls,
			pdev, ARRAY_SIZE(mfgsc1_ao_plls));
}

static int clk_mt6989_ptp_probe(struct platform_device *pdev)
{
	return clk_mt6989_pll_registration(PTPPLL_PLL_CTRL, ptp_plls,
			pdev, ARRAY_SIZE(ptp_plls));
}

static int clk_mt6989_top_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

	void __iomem *base;
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base)) {
		pr_err("%s(): ioremap failed\n", __func__);
		return PTR_ERR(base);
	}

	clk_data = mtk_alloc_clk_data(CLK_TOP_NR_CLK);
	if (!clk_data)
		return -ENOMEM;

	mtk_clk_register_factors(top_divs, ARRAY_SIZE(top_divs),
			clk_data);

	mtk_clk_register_muxes(top_muxes, ARRAY_SIZE(top_muxes), node,
			&mt6989_clk_lock, clk_data);

	mtk_clk_register_composites(top_composites, ARRAY_SIZE(top_composites),
			base, &mt6989_clk_lock, clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif

	return r;
}

static int clk_mt6989_vlp_ck_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

	void __iomem *base;
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base)) {
		pr_err("%s(): ioremap failed\n", __func__);
		return PTR_ERR(base);
	}

	clk_data = mtk_alloc_clk_data(CLK_VLP_CK_NR_CLK);
	if (!clk_data)
		return -ENOMEM;

	mtk_clk_register_factors(vlp_ck_divs, ARRAY_SIZE(vlp_ck_divs),
			clk_data);

	mtk_clk_register_muxes(vlp_ck_muxes, ARRAY_SIZE(vlp_ck_muxes), node,
			&mt6989_clk_lock, clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif

	return r;
}

static int clk_mt6989_mminfra_hwv_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	mminfra_hwv_regmap = syscon_node_to_regmap(node);
	if (IS_ERR(mminfra_hwv_regmap)) {
		pr_err("Cannot find regmap for %pOF: %ld\n", node,
				PTR_ERR(mminfra_hwv_regmap));
		return PTR_ERR(mminfra_hwv_regmap);
	}

	mtk_clk_register_mminfra_hwv_data(&mminfra_hwv_data,
			mminfra_hwv_regmap, &pdev->dev);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif

	return 0;
}

/* for suspend LDVT only */
static void pll_force_off_internal(const struct mtk_pll_data *plls,
		void __iomem *base)
{
	void __iomem *rst_reg, *en_reg, *pwr_reg;

	for (; plls->name; plls++) {
		/* do not pwrdn the AO PLLs */
		if ((plls->flags & PLL_AO) == PLL_AO)
			continue;

		if ((plls->flags & HAVE_RST_BAR) == HAVE_RST_BAR) {
			rst_reg = base + plls->en_reg;
			writel(readl(rst_reg) & ~plls->rst_bar_mask,
				rst_reg);
		}

		en_reg = base + plls->en_reg;

		pwr_reg = base + plls->pwr_reg;

		writel(readl(en_reg) & ~plls->en_mask,
				en_reg);
		writel(readl(pwr_reg) | (0x2),
				pwr_reg);
		writel(readl(pwr_reg) & ~(0x1),
				pwr_reg);
	}
}

void mt6989_pll_force_off(void)
{
	int i;

	for (i = 0; i < PLL_SYS_NUM; i++)
		pll_force_off_internal(plls_data[i], plls_base[i]);
}
EXPORT_SYMBOL_GPL(mt6989_pll_force_off);

static const struct of_device_id of_match_clk_mt6989[] = {
	{
#if IS_ENABLED(CONFIG_MTK_CLKMGR_FTRACE)
		.compatible = "clk-ftrace",
		.data = clk_set_trace_event,
	}, {
#endif
		.compatible = "mediatek,mt6989-apmixedsys",
		.data = clk_mt6989_apmixed_probe,
	}, {
		.compatible = "mediatek,mt6989-armpll_bl_pll_ctrl",
		.data = clk_mt6989_cpu_bl_probe,
	}, {
		.compatible = "mediatek,mt6989-armpll_b_pll_ctrl",
		.data = clk_mt6989_cpu_b_probe,
	}, {
		.compatible = "mediatek,mt6989-armpll_ll_pll_ctrl",
		.data = clk_mt6989_cpu_ll_probe,
	}, {
		.compatible = "mediatek,mt6989-ccipll_pll_ctrl",
		.data = clk_mt6989_cci_probe,
	}, {
		.compatible = "mediatek,mt6989-mfgpll_pll_ctrl",
		.data = clk_mt6989_mfg_ao_probe,
	}, {
		.compatible = "mediatek,mt6989-mfgpll_sc0_pll_ctrl",
		.data = clk_mt6989_mfgsc0_ao_probe,
	}, {
		.compatible = "mediatek,mt6989-mfgpll_sc1_pll_ctrl",
		.data = clk_mt6989_mfgsc1_ao_probe,
	}, {
		.compatible = "mediatek,mt6989-ptppll_pll_ctrl",
		.data = clk_mt6989_ptp_probe,
	}, {
		.compatible = "mediatek,mt6989-topckgen",
		.data = clk_mt6989_top_probe,
	}, {
		.compatible = "mediatek,mt6989-vlp_cksys",
		.data = clk_mt6989_vlp_ck_probe,
	}, {
		.compatible = "mediatek,mt6989-mminfra_hwv",
		.data = clk_mt6989_mminfra_hwv_probe,
	}, {
		/* sentinel */
	}
};

static int clk_mt6989_probe(struct platform_device *pdev)
{
	int (*clk_probe)(struct platform_device *pd);
	int r;

	clk_probe = of_device_get_match_data(&pdev->dev);
	if (!clk_probe)
		return -EINVAL;

	r = clk_probe(pdev);
	if (r)
		dev_err(&pdev->dev,
			"could not register clock provider: %s: %d\n",
			pdev->name, r);

	return r;
}

static struct platform_driver clk_mt6989_drv = {
	.probe = clk_mt6989_probe,
	.driver = {
		.name = "clk-mt6989",
		.owner = THIS_MODULE,
		.of_match_table = of_match_clk_mt6989,
	},
};

module_platform_driver(clk_mt6989_drv);
MODULE_LICENSE("GPL");

