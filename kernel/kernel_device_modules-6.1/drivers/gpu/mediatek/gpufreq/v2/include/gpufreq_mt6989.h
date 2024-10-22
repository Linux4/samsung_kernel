/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

#ifndef __GPUFREQ_MT6989_H__
#define __GPUFREQ_MT6989_H__

/**************************************************
 * GPUFREQ Config
 **************************************************/
#define GPUFREQ_DEBUG_VERSION               (0x20230414)
/* 0 -> power on once then never off and disable DDK power on/off callback */
#define GPUFREQ_POWER_CTRL_ENABLE           (1)
/* 0 -> disable DDK runtime active-sleep callback */
#define GPUFREQ_ACTIVE_SLEEP_CTRL_ENABLE    (0)
/*
 * (DVFS_ENABLE, CUST_INIT)
 * (1, 1) -> DVFS enable and init to CUST_INIT_OPPIDX
 * (1, 0) -> DVFS enable
 * (0, 1) -> DVFS disable but init to CUST_INIT_OPPIDX (do DVFS only onces)
 * (0, 0) -> DVFS disable
 */
#define GPUFREQ_DVFS_ENABLE                 (1)
#define GPUFREQ_CUST_INIT_ENABLE            (1)
#define GPUFREQ_CUST_INIT_OPPIDX            (38)
/* MFGSYS Legacy Feature */
#define GPUFREQ_HWDCM_ENABLE                (0)
#define GPUFREQ_ACP_ENABLE                  (0)
#define GPUFREQ_PDCA_ENABLE                 (0)
#define GPUFREQ_PDCA_IRQ_ENABLE             (0)
#define GPUFREQ_PDCA_PIPELINE_ENABLE        (0)
#define GPUFREQ_GPM1_2_ENABLE               (0)
#define GPUFREQ_GPM3_0_ENABLE               (0)
#define GPUFREQ_AXI_MERGER_ENABLE           (0)
#define GPUFREQ_AXUSER_SLC_ENABLE           (0)
#define GPUFREQ_DFD2_0_ENABLE               (1)
#define GPUFREQ_AVS_ENABLE                  (0)
#define GPUFREQ_TEMPER_COMP_ENABLE          (0)
#define GPUFREQ_DEVAPC_CHECK_ENABLE         (0)
/* MFGSYS New Feature */
#define GPUFREQ_COVSRAM_CTRL_ENABLE         (1)
#define GPUFREQ_DREQ_AUTO_ENABLE            (1)
#define GPUFREQ_HBVC_ENABLE                 (0)
#define GPUFREQ_BUS_CLK_DIV2_ENABLE         (0)
#define GPUFREQ_HW_DELSEL_ENABLE            (0)
#define GPUFREQ_DFD3_6_ENABLE               (0)
#define GPUFREQ_SW_BRCAST_ENABLE            (0)
#define GPUFREQ_BRCAST_PARITY_CHECK         (0)
#define GPUFREQ_BRISKET_BYPASS_MODE         (1)
#define GPUFREQ_SW_VMETER_ENABLE            (0)

/**************************************************
 * PMIC Setting
 **************************************************/
/*
 * PMIC hardware range:
 * VGPU      0.4 - 1.19V (MT6373_VBUCK3)
 * VSTACK    0.4 - 1.2V  (MT6316_VBUCK1_2_3_4)
 * VSRAM     0.4 - 1.19V (MT6373_VBUCK4)
 */
#define VGPU_MAX_VOLT                       (119000)        /* mV x 100 */
#define VGPU_MIN_VOLT                       (40000)         /* mV x 100 */
#define VGPU_ON_SETTLE_TIME                 (180)           /* us */
#define VSTACK_MAX_VOLT                     (119000)        /* mV x 100 */
#define VSTACK_MIN_VOLT                     (40000)         /* mV x 100 */
#define VSTACK_ON_SETTLE_TIME               (180)           /* us */
#define VSRAM_MAX_VOLT                      (119000)        /* mV x 100 */
#define VSRAM_MIN_VOLT                      (40000)         /* mV x 100 */
#define VSRAM_THRESH                        (75000)         /* mV x 100 */
#define VGPU_PMIC_STEP                      (625)           /* mV x 100 */
#define VSTACK_PMIC_STEP                    (500)           /* mV x 100 */
#define PMIC_STEP_ROUNDUP(volt, step)       ((volt % step) ? (volt - (volt % step) + step) : volt)
#define DREQ_TOP_VSRAM_ACK                  (BIT(0))
#define DREQ_TOP_VLOG_ACK                   (BIT(1))
#define DREQ_ST0_VSRAM_ACK                  (BIT(0))
#define DREQ_ST0_VLOG_ACK                   (BIT(3))
#define DREQ_ST1_VSRAM_ACK                  (BIT(1))
#define DREQ_ST1_VLOG_ACK                   (BIT(4))
#define TOP_HW_DELSEL_MASK                  (BIT(17))
#define STACK_HW_DELSEL_MASK                (GENMASK(5, 0))
#define BUCK_ON_SEPARATE_TIME               (100)           /* us */
#define BUCK_ON_SETTLE_TIME                 (MAX(VGPU_ON_SETTLE_TIME, VSTACK_ON_SETTLE_TIME))
/*
 * Buck on state = VGPU/VSTACK > 0.4V
 * Digital sync time = 0.3us
 * Soft-start time   = 40us
 * Slew rate         = 12.5mV/us
 * Force PFM mode    = 10us
 * Clk variation     = 1.1
 * Tgap = 50us  = (0.4V / 12.5mV) * 1.1 + margin
 * Ton  = 135us = (0.9V / 12.5mV + 40us + 10us) * 1.1 + 0.3us
 */
#define BUCK_ON_OPT_SEPARATE_TIME           (50)            /* us */
#define BUCK_ON_OPT_SETTLE_TIME             (135)           /* us */

/**************************************************
 * Clock Setting
 **************************************************/
#define POSDIV_2_MAX_FREQ                   (1900000)         /* KHz */
#define POSDIV_2_MIN_FREQ                   (750000)          /* KHz */
#define POSDIV_4_MAX_FREQ                   (950000)          /* KHz */
#define POSDIV_4_MIN_FREQ                   (375000)          /* KHz */
#define POSDIV_8_MAX_FREQ                   (475000)          /* KHz */
#define POSDIV_8_MIN_FREQ                   (187500)          /* KHz */
#define POSDIV_16_MAX_FREQ                  (237500)          /* KHz */
#define POSDIV_16_MIN_FREQ                  (125000)          /* KHz */
#define POSDIV_SHIFT                        (24)              /* bit */
#define DDS_SHIFT                           (14)              /* bit */
#define MFGPLL_FIN                          (26)              /* MHz */
#define MFG_REF_SEL_BIT                     (BIT(8))
#define MFG_TOP_SEL_BIT                     (BIT(0))
#define MFG_SC0_SEL_BIT                     (BIT(1))
#define MFG_SC1_SEL_BIT                     (BIT(2))
#define FREQ_ROUNDUP_TO_10(freq)            ((freq % 10) ? (freq - (freq % 10) + 10) : freq)

/**************************************************
 * MTCMOS Setting
 **************************************************/
#define GPUFREQ_CHECK_MFG_PWR_STATUS        (0)
#define MFG_0_1_PWR_MASK                    (GENMASK(1, 0))
#define MFG_0_20_37_PWR_MASK                (GENMASK(20, 0) | BIT(0) << 23)
#define MFG_0_1_2_3_9_37_PWR_MASK           (GENMASK(3, 0) | BIT(9) | BIT(23))
#define MFG_1_2_3_9_37_PWR_MASK             (GENMASK(3, 1) | BIT(9) | BIT(23))
#define MFG_0_1_PWR_STATUS \
	(((DRV_Reg32(SPM_XPU_PWR_STATUS) & BIT(1)) >> 1) | \
	(DRV_Reg32(MFG_RPC_MFG_PWR_CON_STATUS) & BIT(1)))
#define MFG_0_22_37_PWR_STATUS \
	(((DRV_Reg32(SPM_XPU_PWR_STATUS) & BIT(1)) >> 1) | \
	(DRV_Reg32(MFG_RPC_MFG_PWR_CON_STATUS) & GENMASK(22, 1)) | \
	(((DRV_Reg32(MFG_RPC_MFG37_PWR_CON) & BIT(30)) >> 30) << 23))
#define MFG_0_31_PWR_STATUS \
	(((DRV_Reg32(SPM_XPU_PWR_STATUS) & BIT(1)) >> 1) | \
	DRV_Reg32(MFG_RPC_MFG_PWR_CON_STATUS))
#define MFG_32_37_PWR_STATUS \
	(DRV_Reg32(MFG_RPC_MFG_PWR_CON_STATUS_1) | \
	(((DRV_Reg32(MFG_RPC_MFG37_PWR_CON) & BIT(30)) >> 30) << 5))

/**************************************************
 * Shader Core Setting
 **************************************************/
#define MFG3_SHADER_STACK0                  (T0C0 | T0C1)   /* MFG9,  MFG12 */
#define MFG4_SHADER_STACK1                  (T1C0 | T1C1)   /* MFG10, MFG13 */
#define MFG22_SHADER_STACK3                 (T3C0 | T3C1)   /* MFG11, MFG14 */
#define MFG6_SHADER_STACK4                  (T4C0 | T4C1)   /* MFG15, MFG18 */
#define MFG7_SHADER_STACK5                  (T5C0 | T5C1)   /* MFG16, MFG19 */
#define MFG8_SHADER_STACK6                  (T6C0 | T6C1)   /* MFG17, MFG20 */
#define GPU_SHADER_PRESENT_1 \
	(T0C0)
#define GPU_SHADER_PRESENT_2 \
	(MFG3_SHADER_STACK0)
#define GPU_SHADER_PRESENT_3 \
	(MFG3_SHADER_STACK0 | T1C0)
#define GPU_SHADER_PRESENT_4 \
	(MFG3_SHADER_STACK0 | MFG4_SHADER_STACK1)
#define GPU_SHADER_PRESENT_5 \
	(MFG3_SHADER_STACK0 | MFG4_SHADER_STACK1 | T3C0)
#define GPU_SHADER_PRESENT_6 \
	(MFG3_SHADER_STACK0 | MFG4_SHADER_STACK1 | MFG22_SHADER_STACK3)
#define GPU_SHADER_PRESENT_7 \
	(MFG3_SHADER_STACK0 | MFG4_SHADER_STACK1 | MFG22_SHADER_STACK3 | \
	 T4C0)
#define GPU_SHADER_PRESENT_8 \
	(MFG3_SHADER_STACK0 | MFG4_SHADER_STACK1 | MFG22_SHADER_STACK3 | \
	 MFG6_SHADER_STACK4)
#define GPU_SHADER_PRESENT_9 \
	(MFG3_SHADER_STACK0 | MFG4_SHADER_STACK1 | MFG22_SHADER_STACK3 | \
	 MFG6_SHADER_STACK4 | T5C0)
#define GPU_SHADER_PRESENT_10 \
	(MFG3_SHADER_STACK0 | MFG4_SHADER_STACK1 | MFG22_SHADER_STACK3 | \
	 MFG6_SHADER_STACK4 | MFG7_SHADER_STACK5)
#define GPU_SHADER_PRESENT_11 \
	(MFG3_SHADER_STACK0 | MFG4_SHADER_STACK1 | MFG22_SHADER_STACK3 | \
	 MFG6_SHADER_STACK4 | MFG7_SHADER_STACK5 | T6C0)
#define GPU_SHADER_PRESENT_12 \
	(MFG3_SHADER_STACK0 | MFG4_SHADER_STACK1 | MFG22_SHADER_STACK3 | \
	 MFG6_SHADER_STACK4 | MFG7_SHADER_STACK5 | MFG8_SHADER_STACK6)
#define SHADER_CORE_NUM                 (12)
struct gpufreq_core_mask_info g_core_mask_table[SHADER_CORE_NUM] = {
	{12, GPU_SHADER_PRESENT_12},
	{11, GPU_SHADER_PRESENT_11},
	{10, GPU_SHADER_PRESENT_10},
	{9, GPU_SHADER_PRESENT_9},
	{8, GPU_SHADER_PRESENT_8},
	{7, GPU_SHADER_PRESENT_7},
	{6, GPU_SHADER_PRESENT_6},
	{5, GPU_SHADER_PRESENT_5},
	{4, GPU_SHADER_PRESENT_4},
	{3, GPU_SHADER_PRESENT_3},
	{2, GPU_SHADER_PRESENT_2},
	{1, GPU_SHADER_PRESENT_1},
};

/**************************************************
 * DVFS Constraint Setting
 **************************************************/
#define SW_VMETER_DELSEL_0_VOLT             (60000)         /* mV x 100 */
#define SW_VMETER_DELSEL_1_VOLT             (50000)         /* mV x 100 */
#define NO_DELSEL_FLOOR_VSTACK              (52000)         /* mV x 100 */
#define NO_GPM3_CEILING_OPP                 (5)
#define PARKING_UPBOUND                     (0)
#define PARKING_SEGMENT                     (12)
#define PARKING_BINNING                     (46)
#define PARKING_TOP_DELSEL                  (53)
#define PARKING_LOWBOUND                    (54)
#define NUM_PARKING_IDX                     ARRAY_SIZE(g_parking_idx)
static int g_parking_idx[] = {
	PARKING_UPBOUND,
	PARKING_SEGMENT,
	PARKING_BINNING,
	PARKING_TOP_DELSEL,
	PARKING_LOWBOUND,
};

/**************************************************
 * Dynamic Power Setting
 **************************************************/
#define GPU_DYN_REF_POWER                   (1613)          /* mW  */
#define GPU_DYN_REF_POWER_FREQ              (1612000)       /* KHz */
#define GPU_DYN_REF_POWER_VOLT              (100000)        /* mV x 100 */
#define STACK_DYN_REF_POWER                 (9860)          /* mW  */
#define STACK_DYN_REF_POWER_FREQ            (1612000)       /* KHz */
#define STACK_DYN_REF_POWER_VOLT            (100000)        /* mV x 100 */

/**************************************************
 * Leakage Power Setting
 **************************************************/
#define LKG_RT_STACK_FAKE_EFUSE             (204)           /* mA  */
#define LKG_HT_STACK_FAKE_EFUSE             (787)           /* mA  */
#define LKG_RT_GPU_DEVINFO_OFS              (8)
#define LKG_RT_GPU_EFUSE_VOLT               (900)           /* mV */
#define LKG_RT_GPU_EFUSE_TEMPER             (30)            /* 'C */
#define LKG_HT_GPU_DEVINFO_OFS              (16)
#define LKG_HT_GPU_EFUSE_VOLT               (900)           /* mV */
#define LKG_HT_GPU_EFUSE_TEMPER             (65)            /* 'C */
#define LKG_RT_STACK_DEVINFO_OFS            (0)
#define LKG_RT_STACK_EFUSE_VOLT             (800)           /* mV */
#define LKG_RT_STACK_EFUSE_TEMPER           (30)            /* 'C */
#define LKG_HT_STACK_DEVINFO_OFS            (0)
#define LKG_HT_STACK_EFUSE_VOLT             (800)           /* mV */
#define LKG_HT_STACK_EFUSE_TEMPER           (65)            /* 'C */
#define LKG_RT_SRAM_DEVINFO_OFS             (16)
#define LKG_RT_SRAM_EFUSE_VOLT              (900)           /* mV */
#define LKG_RT_SRAM_EFUSE_TEMPER            (65)            /* 'C */
#define LKG_HT_SRAM_DEVINFO_OFS             (24)
#define LKG_HT_SRAM_EFUSE_VOLT              (900)           /* mV */
#define LKG_HT_SRAM_EFUSE_TEMPER            (65)            /* 'C */
#define NUM_LKG_INFO                        ARRAY_SIZE(g_leakage_table)
static const uint16_t g_leakage_table[] = {};

/**************************************************
 * Temperature Compensation Setting
 **************************************************/
#define GPU_MAX_SIGNOFF_VOLT                (100000)        /* mV * 100 */
#define STACK_MAX_SIGNOFF_VOLT              (100000)        /* mV * 100 */
#define TEMPERATURE_DEFAULT                 (-274)          /* 'C */
#define TEMPER_COMP_DEFAULT_VOLT            (0)
#define TEMPER_COMP_10_25_VOLT              (1875)          /* mV * 100 */
#define TEMPER_COMP_10_VOLT                 (4375)          /* mV * 100 */

/**************************************************
 * GPM 3.0 Setting
 **************************************************/
#define GPM3OP(_temper, _ceiling, _i_stack, _i_sram) \
	{                                  \
		.temper = _temper,             \
		.ceiling = _ceiling,           \
		.i_stack = _i_stack,           \
		.i_sram = _i_sram,             \
	}
#define GPU_DYN_REF_CURRENT                 (3085)          /* mA */
#define GPU_DYN_REF_CURRENT_FREQ            (1612000)       /* KHz */
#define GPU_DYN_REF_CURRENT_VOLT            (100000)        /* mV x 100 */
#define STACK_DYN_REF_CURRENT               (37553)         /* mA */
#define STACK_DYN_REF_CURRENT_FREQ          (2000000)       /* KHz */
#define STACK_DYN_REF_CURRENT_VOLT          (100000)        /* mV x 100 */
#define GPM3_STACK_IMAX                     (20000)         /* mA */
#define GPM3_TEMPER_OFFSET                  (6)             /* 'C */
#define NUM_GPM3_LIMIT                      ARRAY_SIZE(g_gpm3_table)
static struct gpufreq_gpm3_info g_gpm3_table[] = {
	GPM3OP(25,  0, 0, 0),
	GPM3OP(30,  0, 0, 0),
	GPM3OP(35,  0, 0, 0),
	GPM3OP(40,  0, 0, 0),
	GPM3OP(45,  0, 0, 0),
	GPM3OP(50,  0, 0, 0),
	GPM3OP(55,  0, 0, 0),
	GPM3OP(60,  0, 0, 0),
	GPM3OP(65,  0, 0, 0),
	GPM3OP(70,  0, 0, 0),
	GPM3OP(75,  0, 0, 0),
	GPM3OP(80,  0, 0, 0),
	GPM3OP(85,  0, 0, 0),
	GPM3OP(90,  0, 0, 0),
	GPM3OP(95,  0, 0, 0),
	GPM3OP(100, 0, 0, 0),
	GPM3OP(105, 0, 0, 0),
};

/**************************************************
 * Segment Definition
 **************************************************/
#define MT6989_SEGMENT_UPBOUND              (12)

/**************************************************
 * Enumeration
 **************************************************/
enum gpufreq_segment {
	MT6989_SEGMENT = 0,
};

enum gpufreq_clk_src {
	CLOCK_SUB = 0,
	CLOCK_MAIN,
};

/**************************************************
 * Structure
 **************************************************/
struct gpufreq_pmic_info {
	struct regulator *reg_vgpu;
	struct regulator *reg_vstack;
	struct regulator *reg_vsram;
};

struct gpufreq_clk_info {
	struct clk *clk_mfg_top_pll;
	struct clk *clk_mfg_sc0_pll;
	struct clk *clk_mfg_sc1_pll;
	struct clk *clk_mfg_ref_sel;
};

struct gpufreq_status {
	struct gpufreq_opp_info *signed_table;
	struct gpufreq_opp_info *working_table;
	int buck_count;
	int mtcmos_count;
	int cg_count;
	int power_count;
	int active_count;
	unsigned int segment_id;
	int signed_opp_num;
	int segment_upbound;
	int segment_lowbound;
	int opp_num;
	int max_oppidx;
	int min_oppidx;
	int cur_oppidx;
	unsigned int cur_freq;
	unsigned int cur_volt;
	unsigned int cur_vsram;
	unsigned int lkg_rt_info;
	unsigned int lkg_ht_info;
	unsigned int lkg_rt_info_sram;
	unsigned int lkg_ht_info_sram;
};

struct gpufreq_volt_sb {
	unsigned int oppidx;
	unsigned int vgpu;
	unsigned int vstack;
	unsigned int vgpu_up;
	unsigned int vgpu_down;
	unsigned int vstack_up;
	unsigned int vstack_down;
};

struct gpufreq_avs_info {
	void __iomem *reg_gpu_avs;
	void __iomem *reg_stack_avs;
	unsigned int reg_gpu_mcl50;
	unsigned int reg_stack_mcl50;
	unsigned int vgpu_ptpv1_shift;
	unsigned int vstack_ptpv1_shift;
};

/**************************************************
 * GPU Platform OPP Table Definition
 **************************************************/
#define GPU_SIGNED_OPP_0                    (0)
#define GPU_SIGNED_OPP_1                    (12)
#define GPU_SIGNED_OPP_2                    (36)
#define GPU_SIGNED_OPP_3                    (54)
#define NUM_GPU_SIGNED_IDX                  ARRAY_SIZE(g_gpu_signed_idx)
#define NUM_GPU_SIGNED_OPP                  ARRAY_SIZE(g_gpu_default_opp_table)
static const int g_gpu_signed_idx[] = {
	GPU_SIGNED_OPP_0,
	GPU_SIGNED_OPP_1,
	GPU_SIGNED_OPP_2,
	GPU_SIGNED_OPP_3,
};
static struct gpufreq_opp_info g_gpu_default_opp_table[] = {
	GPUOP(1612000, 100000, 75000, POSDIV_POWER_2, 0, 0), /*  0 signoff */
	GPUOP(1612000, 100000, 75000, POSDIV_POWER_2, 0, 0), /*  1 */
	GPUOP(1612000, 100000, 75000, POSDIV_POWER_2, 0, 0), /*  2 */
	GPUOP(1612000, 100000, 75000, POSDIV_POWER_2, 0, 0), /*  3 */
	GPUOP(1612000, 100000, 75000, POSDIV_POWER_2, 0, 0), /*  4 */
	GPUOP(1612000, 100000, 75000, POSDIV_POWER_2, 0, 0), /*  5 */
	GPUOP(1612000, 100000, 75000, POSDIV_POWER_2, 0, 0), /*  6 */
	GPUOP(1612000, 100000, 75000, POSDIV_POWER_2, 0, 0), /*  7 */
	GPUOP(1612000, 100000, 75000, POSDIV_POWER_2, 0, 0), /*  8 */
	GPUOP(1612000, 100000, 75000, POSDIV_POWER_2, 0, 0), /*  9 */
	GPUOP(1612000, 100000, 75000, POSDIV_POWER_2, 0, 0), /* 10 */
	GPUOP(1612000, 100000, 75000, POSDIV_POWER_2, 0, 0), /* 11 */
	GPUOP(1352000, 90000,  75000, POSDIV_POWER_2, 0, 0), /* 12 signoff */
	GPUOP(1352000, 90000,  75000, POSDIV_POWER_2, 0, 0), /* 13 */
	GPUOP(1352000, 90000,  75000, POSDIV_POWER_2, 0, 0), /* 14 */
	GPUOP(1352000, 90000,  75000, POSDIV_POWER_2, 0, 0), /* 15 */
	GPUOP(1352000, 90000,  75000, POSDIV_POWER_2, 0, 0), /* 16 */
	GPUOP(1352000, 90000,  75000, POSDIV_POWER_2, 0, 0), /* 17 */
	GPUOP(1352000, 90000,  75000, POSDIV_POWER_2, 0, 0), /* 18 */
	GPUOP(1352000, 90000,  75000, POSDIV_POWER_2, 0, 0), /* 19 */
	GPUOP(1352000, 90000,  75000, POSDIV_POWER_2, 0, 0), /* 20 */
	GPUOP(1352000, 90000,  75000, POSDIV_POWER_2, 0, 0), /* 21 */
	GPUOP(1352000, 90000,  75000, POSDIV_POWER_2, 0, 0), /* 22 */
	GPUOP(1352000, 90000,  75000, POSDIV_POWER_2, 0, 0), /* 23 */
	GPUOP(1352000, 90000,  75000, POSDIV_POWER_2, 0, 0), /* 24 */
	GPUOP(1352000, 90000,  75000, POSDIV_POWER_2, 0, 0), /* 25 */
	GPUOP(1352000, 90000,  75000, POSDIV_POWER_4, 0, 0), /* 26 */
	GPUOP(1352000, 90000,  75000, POSDIV_POWER_4, 0, 0), /* 27 */
	GPUOP(1352000, 90000,  75000, POSDIV_POWER_4, 0, 0), /* 28 */
	GPUOP(1352000, 90000,  75000, POSDIV_POWER_2, 0, 0), /* 29 */
	GPUOP(1352000, 90000,  75000, POSDIV_POWER_2, 0, 0), /* 30 */
	GPUOP(1352000, 90000,  75000, POSDIV_POWER_2, 0, 0), /* 31 */
	GPUOP(1352000, 90000,  75000, POSDIV_POWER_2, 0, 0), /* 32 */
	GPUOP(1352000, 90000,  75000, POSDIV_POWER_2, 0, 0), /* 33 */
	GPUOP(1352000, 90000,  75000, POSDIV_POWER_2, 0, 0), /* 34 */
	GPUOP(1352000, 90000,  75000, POSDIV_POWER_2, 0, 0), /* 35 */
	GPUOP(1352000, 90000,  75000, POSDIV_POWER_2, 0, 0), /* 36 signoff */
	GPUOP(1300000, 88125,  75000, POSDIV_POWER_2, 0, 0), /* 37 */
	GPUOP(1248000, 86250,  75000, POSDIV_POWER_2, 0, 0), /* 38 */
	GPUOP(1196000, 84375,  75000, POSDIV_POWER_2, 0, 0), /* 39 */
	GPUOP(1144000, 82500,  75000, POSDIV_POWER_2, 0, 0), /* 40 */
	GPUOP(1092000, 80625,  75000, POSDIV_POWER_2, 0, 0), /* 41 */
	GPUOP(1040000, 78750,  75000, POSDIV_POWER_2, 0, 0), /* 42 */
	GPUOP(988000,  76875,  75000, POSDIV_POWER_2, 0, 0), /* 43 */
	GPUOP(936000,  75000,  75000, POSDIV_POWER_4, 0, 0), /* 44 */
	GPUOP(884000,  73125,  75000, POSDIV_POWER_4, 0, 0), /* 45 */
	GPUOP(832000,  71250,  75000, POSDIV_POWER_4, 0, 0), /* 46 */
	GPUOP(780000,  69375,  75000, POSDIV_POWER_4, 0, 0), /* 47 */
	GPUOP(728000,  67500,  75000, POSDIV_POWER_4, 0, 0), /* 48 */
	GPUOP(676000,  65625,  75000, POSDIV_POWER_4, 0, 0), /* 49 */
	GPUOP(624000,  63750,  75000, POSDIV_POWER_4, 0, 0), /* 50 */
	GPUOP(572000,  61875,  75000, POSDIV_POWER_4, 0, 0), /* 51 */
	GPUOP(520000,  60000,  75000, POSDIV_POWER_4, 0, 0), /* 52 */
	GPUOP(364000,  54375,  75000, POSDIV_POWER_8, 0, 0), /* 53 */
	GPUOP(260000,  50000,  75000, POSDIV_POWER_8, 0, 0), /* 54 signoff */
};

#define STACK_SIGNED_OPP_0                  (0)
#define STACK_SIGNED_OPP_1                  (12)
#define STACK_SIGNED_OPP_2                  (36)
#define STACK_SIGNED_OPP_3                  (54)
#define NUM_STACK_SIGNED_IDX                ARRAY_SIZE(g_stack_signed_idx)
#define NUM_STACK_SIGNED_OPP                ARRAY_SIZE(g_stack_default_opp_table)
static const int g_stack_signed_idx[] = {
	STACK_SIGNED_OPP_0,
	STACK_SIGNED_OPP_1,
	STACK_SIGNED_OPP_2,
	STACK_SIGNED_OPP_3,
};
static struct gpufreq_opp_info g_stack_default_opp_table[] = {
	GPUOP(1612000, 100000, 75000, POSDIV_POWER_2, 0, 0), /*  0 signoff */
	GPUOP(1586000, 99000,  75000, POSDIV_POWER_2, 0, 0), /*  1 */
	GPUOP(1560000, 97500,  75000, POSDIV_POWER_2, 0, 0), /*  2 */
	GPUOP(1534000, 96500,  75000, POSDIV_POWER_2, 0, 0), /*  3 */
	GPUOP(1508000, 95000,  75000, POSDIV_POWER_2, 0, 0), /*  4 */
	GPUOP(1482000, 94000,  75000, POSDIV_POWER_2, 0, 0), /*  5 */
	GPUOP(1456000, 92500,  75000, POSDIV_POWER_2, 0, 0), /*  6 */
	GPUOP(1430000, 91500,  75000, POSDIV_POWER_2, 0, 0), /*  7 */
	GPUOP(1404000, 90000,  75000, POSDIV_POWER_2, 0, 0), /*  8 */
	GPUOP(1378000, 89000,  75000, POSDIV_POWER_2, 0, 0), /*  9 */
	GPUOP(1352000, 87500,  75000, POSDIV_POWER_2, 0, 0), /* 10 */
	GPUOP(1326000, 86500,  75000, POSDIV_POWER_2, 0, 0), /* 11 */
	GPUOP(1300000, 85000,  75000, POSDIV_POWER_2, 0, 0), /* 12 signoff */
	GPUOP(1274000, 84500,  75000, POSDIV_POWER_2, 0, 0), /* 13 */
	GPUOP(1248000, 83500,  75000, POSDIV_POWER_2, 0, 0), /* 14 */
	GPUOP(1222000, 82500,  75000, POSDIV_POWER_2, 0, 0), /* 15 */
	GPUOP(1196000, 82000,  75000, POSDIV_POWER_2, 0, 0), /* 16 */
	GPUOP(1170000, 81000,  75000, POSDIV_POWER_2, 0, 0), /* 17 */
	GPUOP(1144000, 80000,  75000, POSDIV_POWER_2, 0, 0), /* 18 */
	GPUOP(1118000, 79500,  75000, POSDIV_POWER_2, 0, 0), /* 19 */
	GPUOP(1092000, 78500,  75000, POSDIV_POWER_2, 0, 0), /* 20 */
	GPUOP(1066000, 77500,  75000, POSDIV_POWER_2, 0, 0), /* 21 */
	GPUOP(1040000, 77000,  75000, POSDIV_POWER_2, 0, 0), /* 22 */
	GPUOP(1014000, 76000,  75000, POSDIV_POWER_2, 0, 0), /* 23 */
	GPUOP(988000,  75000,  75000, POSDIV_POWER_2, 0, 0), /* 24 */
	GPUOP(962000,  74500,  75000, POSDIV_POWER_2, 0, 0), /* 25 */
	GPUOP(936000,  73500,  75000, POSDIV_POWER_4, 0, 0), /* 26 */
	GPUOP(910000,  72500,  75000, POSDIV_POWER_4, 0, 0), /* 27 */
	GPUOP(884000,  72000,  75000, POSDIV_POWER_4, 0, 0), /* 28 */
	GPUOP(858000,  71000,  75000, POSDIV_POWER_4, 0, 0), /* 29 */
	GPUOP(832000,  70000,  75000, POSDIV_POWER_4, 0, 0), /* 30 */
	GPUOP(806000,  69500,  75000, POSDIV_POWER_4, 0, 0), /* 31 */
	GPUOP(780000,  68500,  75000, POSDIV_POWER_4, 0, 0), /* 32 */
	GPUOP(754000,  67500,  75000, POSDIV_POWER_4, 0, 0), /* 33 */
	GPUOP(728000,  67000,  75000, POSDIV_POWER_4, 0, 0), /* 34 */
	GPUOP(702000,  66000,  75000, POSDIV_POWER_4, 0, 0), /* 35 */
	GPUOP(676000,  65000,  75000, POSDIV_POWER_4, 0, 0), /* 36 signoff */
	GPUOP(650000,  64500,  75000, POSDIV_POWER_4, 0, 0), /* 37 */
	GPUOP(624000,  63500,  75000, POSDIV_POWER_4, 0, 0), /* 38 */
	GPUOP(598000,  62500,  75000, POSDIV_POWER_4, 0, 0), /* 39 */
	GPUOP(572000,  61500,  75000, POSDIV_POWER_4, 0, 0), /* 40 */
	GPUOP(546000,  60500,  75000, POSDIV_POWER_4, 0, 0), /* 41 */
	GPUOP(520000,  59500,  75000, POSDIV_POWER_4, 0, 0), /* 42 */
	GPUOP(494000,  58500,  75000, POSDIV_POWER_4, 0, 0), /* 43 */
	GPUOP(468000,  57500,  75000, POSDIV_POWER_8, 0, 0), /* 44 */
	GPUOP(442000,  57000,  75000, POSDIV_POWER_8, 0, 0), /* 45 */
	GPUOP(416000,  56000,  75000, POSDIV_POWER_8, 0, 0), /* 46 */
	GPUOP(390000,  55000,  75000, POSDIV_POWER_8, 0, 0), /* 47 */
	GPUOP(364000,  54000,  75000, POSDIV_POWER_8, 0, 0), /* 48 */
	GPUOP(338000,  53000,  75000, POSDIV_POWER_8, 0, 0), /* 49 */
	GPUOP(312000,  52000,  75000, POSDIV_POWER_8, 0, 0), /* 50 */
	GPUOP(286000,  51000,  75000, POSDIV_POWER_8, 0, 0), /* 51 */
	GPUOP(260000,  50000,  75000, POSDIV_POWER_8, 0, 0), /* 52 */
	GPUOP(260000,  50000,  75000, POSDIV_POWER_8, 0, 0), /* 53 */
	GPUOP(260000,  50000,  75000, POSDIV_POWER_8, 0, 0), /* 54 signoff */
};

/**************************************************
 * OPP Adjustment
 **************************************************/
static struct gpufreq_adj_info g_gpu_avs_table[NUM_GPU_SIGNED_IDX] = {
	ADJOP(GPU_SIGNED_OPP_0, 0, 0, 0),
	ADJOP(GPU_SIGNED_OPP_1, 0, 0, 0),
	ADJOP(GPU_SIGNED_OPP_2, 0, 0, 0),
	ADJOP(GPU_SIGNED_OPP_3, 0, 0, 0),
};

static struct gpufreq_adj_info g_stack_avs_table[NUM_STACK_SIGNED_IDX] = {
	ADJOP(STACK_SIGNED_OPP_0, 0, 0, 0),
	ADJOP(STACK_SIGNED_OPP_1, 0, 0, 0),
	ADJOP(STACK_SIGNED_OPP_2, 0, 0, 0),
	ADJOP(STACK_SIGNED_OPP_3, 0, 0, 0),
};

static struct gpufreq_adj_info g_gpu_aging_table[NUM_GPU_SIGNED_IDX] = {
	ADJOP(GPU_SIGNED_OPP_0, 0, 625, 0),
	ADJOP(GPU_SIGNED_OPP_1, 0, 625, 0),
	ADJOP(GPU_SIGNED_OPP_2, 0, 625, 0),
	ADJOP(GPU_SIGNED_OPP_3, 0, 625, 0),
};

static struct gpufreq_adj_info g_stack_aging_table[NUM_STACK_SIGNED_IDX] = {
	ADJOP(STACK_SIGNED_OPP_0, 0, 500, 0),
	ADJOP(STACK_SIGNED_OPP_1, 0, 500, 0),
	ADJOP(STACK_SIGNED_OPP_2, 0, 500, 0),
	ADJOP(STACK_SIGNED_OPP_3, 0, 500, 0),
};

#endif /* __GPUFREQ_MT6989_H__ */
