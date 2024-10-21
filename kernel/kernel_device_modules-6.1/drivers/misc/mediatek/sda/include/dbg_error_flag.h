/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2022 Mediatek Inc.
 */

/*
 * #define INFRA_LASTBUS_TIMEOUT_MASK      (1 << 0)
 * #define PERI_LASTBUS_TIMEOUT_MASK       (1 << 1)
 * #define DRAM_MD32_WDT_EVENT_CH_A_MASK   (1 << 2)
 * #define DRAM_MD32_WDT_EVENT_CH_B_MASK   (1 << 3)
 * #define DRAM_MD32_WDT_EVENT_CH_C_MASK   (1 << 4)
 * #define DRAM_MD32_WDT_EVENT_CH_D_MASK   (1 << 5)
 * #define INFRA_SMMU_IRQ_MASK             (1 << 6)
 * #define INFRA_SMMU_NS_IRQ_MASK          (1 << 7)
 * #define AP_TRACKER_TIMEOUT_MASK         (1 << 8)
 * #define INFRA_TRACKER_TIMEOUT_MASK      (1 << 9)
 * #define VLP_TRACKER_TIMEOUT_MASK        (1 << 10)
 * #define MCU_TO_SOC_DFD_EVENT_MASK       (1 << 11)
 * #define APU_SMMU_IRQ_MASK               (1 << 12)
 * #define MFG_TO_SOC_DFD_EVENT_MASK       (1 << 13)
 * #define MMINFRA_SMMU_IRQ_MASK           (1 << 14)
 * #define MFG_TO_EMI_SLV_PARITY_MASK      (1 << 15)
 * #define MCU2SUB_EMI_M1_PARITY_MASK      (1 << 16)
 * #define MCU2SUB_EMI_M0_PARITY_MASK      (1 << 17)
 * #define MCU2EMI_M1_PARITY_MASK          (1 << 18)
 * #define MCU2EMI_M0_PARITY_MASK          (1 << 19)
 * #define MCU2INFRA_REG_PARITY_MASK       (1 << 20)
 * #define INFRA_L3_CACHE2MCU_PARITY_MASK  (1 << 21)
 * #define EMI_PARITY_CEN__MASK            (1 << 22)
 * #define EMI_PARITY_SUB_CEN_MASK         (1 << 23)
 * #define EMI_PARITY_CHAN1_MASK           (1 << 24)
 * #define EMI_PARITY_CHAN2_MASK           (1 << 25)
 * #define EMI_PARITY_CHAN3_MASK           (1 << 26)
 * #define EMI_PARITY_CHAN4_MASK           (1 << 27)
 * #define DRAMC_ERROR_FLAG_CH_A_MASK      (1 << 28)
 * #define DRAMC_ERROR_FLAG_CH_B_MASK      (1 << 29)
 * #define DRAMC_ERROR_FLAG_CH_C_MASK      (1 << 30)
 * #define DRAMC_ERROR_FLAG_CH_D_MASK      (1 << 31)
 */

#define NAME_LEN 64

enum DBG_ERR_FLAG_OP {
	DBG_ERR_FLAG_CLR = 0,
	DBG_ERR_FLAG_UNMASK = 1,
	NR_DBG_ERR_FLAG_OP
};

struct DBG_ERROR_FLAG_DESC {
	char mask_name[NAME_LEN];
	bool support;
	u32 mask;
};

enum DBG_ERROR_FLAG_ENUM {
	INFRA_LASTBUS_TIMEOUT,
	PERI_LASTBUS_TIMEOUT,
	DRAM_MD32_WDT_EVENT_CH_A,
	DRAM_MD32_WDT_EVENT_CH_B,
	DRAM_MD32_WDT_EVENT_CH_C,
	DRAM_MD32_WDT_EVENT_CH_D,
	INFRA_SMMU_IRQ,
	INFRA_SMMU_NS_IRQ,
	AP_TRACKER_TIMEOUT,
	INFRA_TRACKER_TIMEOUT,
	VLP_TRACKER_TIMEOUT,
	MCU_TO_SOC_DFD_EVENT,
	APU_SMMU_IRQ,
	MFG_TO_SOC_DFD_EVENT,
	MMINFRA_SMMU_IRQ,
	MFG_TO_EMI_SLV_PARITY,
	MCU2SUB_EMI_M1_PARITY,
	MCU2SUB_EMI_M0_PARITY,
	MCU2EMI_M1_PARITY,
	MCU2EMI_M0_PARITY,
	MCU2INFRA_REG_PARITY,
	INFRA_L3_CACHE2MCU_PARITY,
	EMI_PARITY_CEN,
	EMI_PARITY_SUB_CEN,
	EMI_PARITY_CHAN1,
	EMI_PARITY_CHAN2,
	EMI_PARITY_CHAN3,
	EMI_PARITY_CHAN4,
	DRAMC_ERROR_FLAG_CH_A,
	DRAMC_ERROR_FLAG_CH_B,
	DRAMC_ERROR_FLAG_CH_C,
	DRAMC_ERROR_FLAG_CH_D,
	DBG_ERROR_FLAG_TOTAL,
};

struct DBG_ERROR_FLAG_DESC dbg_error_flag_desc[DBG_ERROR_FLAG_TOTAL] = {
	[INFRA_LASTBUS_TIMEOUT] = {
		.mask_name = "infra-lastbus-timeout-mask",
	},
	[PERI_LASTBUS_TIMEOUT] = {
		.mask_name = "peri-lastbus-timeout-mask",
	},
	[DRAM_MD32_WDT_EVENT_CH_A] = {
		.mask_name = "dram-md32-wdt-event-ch-a-mask",
	},
	[DRAM_MD32_WDT_EVENT_CH_B] = {
		.mask_name = "dram-md32-wdt-event-ch-b-mask",
	},
	[DRAM_MD32_WDT_EVENT_CH_C] = {
		.mask_name = "dram-md32-wdt-event-ch-c-mask",
	},
	[DRAM_MD32_WDT_EVENT_CH_D] = {
		.mask_name = "dram-md32-wdt-event-ch-d-mask",
	},
	[INFRA_SMMU_IRQ] = {
		.mask_name = "infra-smmu-irq-mask",
	},
	[INFRA_SMMU_NS_IRQ] = {
		.mask_name = "infra-smmu-ns-irq-mask",
	},
	[AP_TRACKER_TIMEOUT] = {
		.mask_name = "ap-tracker-timeout-mask",
	},
	[INFRA_TRACKER_TIMEOUT] = {
		.mask_name = "infra-tracker-timeout-mask",
	},
	[VLP_TRACKER_TIMEOUT] = {
		.mask_name = "vlp-tracker-timeout-mask",
	},
	[MCU_TO_SOC_DFD_EVENT] = {
		.mask_name = "mcu-to-soc-dfd-event-mask",
	},
	[APU_SMMU_IRQ] = {
		.mask_name = "apu-smmu-irq-mask",
	},
	[MFG_TO_SOC_DFD_EVENT] = {
		.mask_name = "mfg-to-soc-dfd-event-mask",
	},
	[MMINFRA_SMMU_IRQ] = {
		.mask_name = "mminfra-smmu-irq-mask",
	},
	[MFG_TO_EMI_SLV_PARITY] = {
		.mask_name = "mfg-to-emi-slv-parity-mask",
	},
	[MCU2SUB_EMI_M1_PARITY] = {
		.mask_name = "mcu2sub-emi-m1-parity-mask",
	},
	[MCU2SUB_EMI_M0_PARITY] = {
		.mask_name = "mcu2sub-emi-m0-parity-mask",
	},
	[MCU2EMI_M1_PARITY] = {
		.mask_name = "mcu2emi-m1-parity-mask",
	},
	[MCU2EMI_M0_PARITY] = {
		.mask_name = "mcu2emi-m0-parity-mask",
	},
	[MCU2INFRA_REG_PARITY] = {
		.mask_name = "mcu2infra-reg-parity-mask",
	},
	[INFRA_L3_CACHE2MCU_PARITY] = {
		.mask_name = "infra-l3-cache2mcu-parity-mask",
	},
	[EMI_PARITY_CEN] = {
		.mask_name = "emi-parity-cen-mask",
	},
	[EMI_PARITY_SUB_CEN] = {
		.mask_name = "emi-parity-sub-cen-mask",
	},
	[EMI_PARITY_CHAN1] = {
		.mask_name = "emi-parity-chan1-mask",
	},
	[EMI_PARITY_CHAN2] = {
		.mask_name = "emi-parity-chan2-mask",
	},
	[EMI_PARITY_CHAN3] = {
		.mask_name = "emi-parity-chan3-mask",
	},
	[EMI_PARITY_CHAN4] = {
		.mask_name = "emi-parity-chan4-mask",
	},
	[DRAMC_ERROR_FLAG_CH_A] = {
		.mask_name = "dramc-error-flag-ch-a-mask",
	},
	[DRAMC_ERROR_FLAG_CH_B] = {
		.mask_name = "dramc-error-flag-ch-b-mask",
	},
	[DRAMC_ERROR_FLAG_CH_C] = {
		.mask_name = "dramc-error-flag-ch-c-mask",
	},
	[DRAMC_ERROR_FLAG_CH_D] = {
		.mask_name = "dramc-error-flag-ch-d-mask",
	},
};

extern void dbg_error_flag_register_notify(struct notifier_block *nb);
extern unsigned int get_dbg_error_flag_mask(unsigned int err_flag_enum);
